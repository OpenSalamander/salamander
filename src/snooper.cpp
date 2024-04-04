// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "snooper.h"

CWindowArray WindowArray(10, 5);
CObjectArray ObjectArray(10, 5);

HANDLE Thread = NULL;
HANDLE DataUsageMutex = NULL;       // for arrays with data for thread and process
HANDLE RefreshFinishedEvent = NULL; // waits for processing due to "PostMessage"
HANDLE WantDataEvent = NULL;        // main thread wants to access shared data
HANDLE TerminateEvent = NULL;       // main thread wants to terminate the thread cmuchala
HANDLE ContinueEvent = NULL;        // helper event for synchronization
HANDLE BeginSuspendEvent = NULL;    // start of suspend mode
HANDLE EndSuspendEvent = NULL;      // End of suspend mode for cmuchala
HANDLE SharesEvent = NULL;          // will be signaled if LanMan Shares change

int SnooperSuspended = 0;

CRITICAL_SECTION TimeCounterSection; // for synchronizing access to MyTimeCounter
int MyTimeCounter = 0;               // current time

HANDLE SafeFindCloseThread = NULL;              // thread "safe handle killer"
TDirectArray<HANDLE> SafeFindCloseCNArr(10, 5); // Safe (non-blocking) closing of the change-notify trade
CRITICAL_SECTION SafeFindCloseCS;               // critical section for accessing the trade array
BOOL SafeFindCloseTerminate = FALSE;            // to terminate the thread
HANDLE SafeFindCloseStart = NULL;               // "starter" of the thread - if it is non-signaled, it waits
HANDLE SafeFindCloseFinished = NULL;            // signaled -> thread has closed all handles

DWORD WINAPI ThreadFindCloseChangeNotification(void* param);

void DoWantDataEvent()
{
    ReleaseMutex(DataUsageMutex);                  // Release data for the main thread
    WaitForSingleObject(WantDataEvent, INFINITE);  // we will wait until it takes effect
    WaitForSingleObject(DataUsageMutex, INFINITE); // once it's over, they're ours again
    SetEvent(ContinueEvent);                       // They are ours now, let's release the main thread further
}

unsigned ThreadSnooperBody(void* /*param*/) // do not call functions of the main thread (nor TRACE) !!!
{
    CALL_STACK_MESSAGE1("ThreadSnooperBody()");
    SetThreadNameInVCAndTrace("Snooper");
    TRACE_I("Begin");

    DWORD res;
    HKEY sharesKey;
    res = HANDLES_Q(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                 "system\\currentcontrolset\\services\\lanmanserver\\shares",
                                 0, KEY_NOTIFY, &sharesKey));
    if (res != ERROR_SUCCESS)
    {
        sharesKey = NULL;
        TRACE_E("Unable to open key in registry (LanMan Shares). error: " << GetErrorText(res));
    }
    else // Key is okay, let's set up notifications (without this, RegNotifyChangeKeyValue won't be called anymore)
    {
        if ((res = RegNotifyChangeKeyValue(sharesKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, SharesEvent,
                                           TRUE)) != ERROR_SUCCESS)
        {
            TRACE_E("Unable to monitor registry (LanMan Shares). error: " << GetErrorText(res));
        }
    }

    if (WaitForSingleObject(DataUsageMutex, INFINITE) == WAIT_OBJECT_0)
    {
        SetEvent(ContinueEvent); // now the data are ready, main thread can continue

        WindowArray.Add(NULL); // basic objects, must be at the beginning!
        WindowArray.Add(NULL);
        WindowArray.Add(NULL);
        WindowArray.Add(NULL);
        ObjectArray.Add(WantDataEvent);
        ObjectArray.Add(TerminateEvent);
        ObjectArray.Add(BeginSuspendEvent);
        ObjectArray.Add(SharesEvent);

        BOOL ignoreRefreshes = FALSE;        // TRUE = ignore refreshes (changes in directories), otherwise we operate normally
        DWORD ignoreRefreshesAbsTimeout = 0; // When (int)(GetTickCount() - ignoreRefreshesAbsTimeout) >= 0, we switch ignoreRefreshes to FALSE
        BOOL notEnd = TRUE;
        while (notEnd)
        {
            int timeout = ignoreRefreshes ? (int)(ignoreRefreshesAbsTimeout - GetTickCount()) : INFINITE;
            if (ignoreRefreshes && timeout <= 0)
            {
                ignoreRefreshes = FALSE;
                ignoreRefreshesAbsTimeout = 0;
                timeout = INFINITE;
            }
            //      TRACE_I("Snooper is waiting for: " << (ignoreRefreshes ? min(4, ObjectArray.Count) : ObjectArray.Count) << " events");
            res = WaitForMultipleObjects(ignoreRefreshes ? min(4, ObjectArray.Count) : ObjectArray.Count,
                                         (HANDLE*)ObjectArray.GetData(),
                                         FALSE, timeout);
            CALL_STACK_MESSAGE2("ThreadSnooperBody::wait_satisfied: 0x%X", res);
            switch (res)
            {
            case WAIT_OBJECT_0:
                DoWantDataEvent();
                break; // WantDataEvent
            case WAIT_OBJECT_0 + 1:
                notEnd = FALSE;
                break;              // TerminateEvent
            case WAIT_OBJECT_0 + 2: // BeginSuspendMode
            {
                TRACE_I("Start suspend mode");

                SetEvent(ContinueEvent); // we are already in suspend -> let's continue the main thread

                TDirectArray<HWND> refreshPanels(10, 5); // in case of deleting the watched directory

                ObjectArray[2] = EndSuspendEvent; // instead of begin now end suspend mode

                BOOL setSharesEvent = FALSE; // TRUE => reset the registry monitoring again
                BOOL suspendNotFinished = TRUE;
                while (suspendNotFinished) // Wait for the end of suspend mode
                {                          // Handling everything except changes in directories
                    timeout = ignoreRefreshes ? (int)(ignoreRefreshesAbsTimeout - GetTickCount()) : INFINITE;
                    if (ignoreRefreshes && timeout <= 0)
                    {
                        ignoreRefreshes = FALSE;
                        ignoreRefreshesAbsTimeout = 0;
                        timeout = INFINITE;
                    }
                    res = WaitForMultipleObjects(ignoreRefreshes ? min(4, ObjectArray.Count) : ObjectArray.Count,
                                                 (HANDLE*)ObjectArray.GetData(),
                                                 FALSE, timeout);

                    CALL_STACK_MESSAGE2("ThreadSnooperBody::suspend_wait_satisfied: 0x%X", res);
                    switch (res)
                    {
                    case WAIT_OBJECT_0:
                        DoWantDataEvent();
                        break; // WantDataEvent
                    case WAIT_OBJECT_0 + 1:
                        suspendNotFinished = notEnd = FALSE;
                        break; // TerminateEvent
                    case WAIT_OBJECT_0 + 2:
                        suspendNotFinished = FALSE;
                        break;              // EndSuspendEvent
                    case WAIT_OBJECT_0 + 3: // SharesEvent
                    {
                        // refresh shares and refresh panels if necessary (using WM_USER_REFRESH_SHARES)
                        setSharesEvent = TRUE;
                        break;
                    }

                    case WAIT_TIMEOUT:
                        break; // we are ignoring (end of ignoring changes in directories mode)

                    default:
                    {
                        int index = res - WAIT_OBJECT_0;
                        if (index < 0 || index >= WindowArray.Count)
                        {
                            TRACE_E("Unexpected value returned from WaitForMultipleObjects(): " << res);
                            break; // in case of any other value res
                        }

                        // Calling FindCloseChangeNotification invalidates other handles on the same path
                        // (doing at UNC paths), therefore we simulate the signaled state forcibly
                        HANDLE sameHandle = NULL; // != NULL -> handle in the same way
                        CFilesWindow* actWin = WindowArray[index];
                        int e;
                        for (e = 0; e < WindowArray.Count; e++)
                        {
                            CFilesWindow* w = WindowArray[e];
                            if (w != NULL && w != actWin && actWin->SamePath(w))
                            {
                                sameHandle = (HANDLE)ObjectArray[e];
                                break;
                            }
                        }

                        // Change has already occurred, we are not interested in any more, refresh will come after suspension
                        if (MainWindowCS.LockIfNotClosed())
                        {
                            //                  TRACE_I("Change notification in suspend mode: " << (MainWindow->LeftPanel == WindowArray[index] ? "left" : "right"));
                            MainWindowCS.Unlock();
                        }
                        HDEVNOTIFY panelDevNotification = WindowArray[index]->DeviceNotification;
                        if (panelDevNotification != NULL)
                        {
                            UnregisterDeviceNotification(panelDevNotification);
                            WindowArray[index]->DeviceNotification = NULL;
                        }
                        HANDLES(FindCloseChangeNotification((HANDLE)ObjectArray[index]));
                        refreshPanels.Add(WindowArray[index]->HWindow); // add to the refreshed
                        ObjectArray.Delete(index);                      // we remove him from the list
                        WindowArray.Delete(index);

                        // if it is necessary to bypass a system error, we will do it here
                        if (sameHandle != NULL)
                        {
                            for (index = 0; index < ObjectArray.Count; index++)
                            {
                                if ((HANDLE)ObjectArray[index] == sameHandle)
                                {
                                    HDEVNOTIFY panelDevNotification2 = WindowArray[index]->DeviceNotification;
                                    if (panelDevNotification2 != NULL)
                                    {
                                        UnregisterDeviceNotification(panelDevNotification2);
                                        WindowArray[index]->DeviceNotification = NULL;
                                    }
                                    HANDLES(FindCloseChangeNotification((HANDLE)ObjectArray[index]));
                                    refreshPanels.Add(WindowArray[index]->HWindow); // add to the refreshed
                                    ObjectArray.Delete(index);                      // we remove him from the list
                                    WindowArray.Delete(index);
                                }
                            }
                        }
                        break;
                    }
                    }
                }
                SetEvent(ContinueEvent); // we are no longer suspended -> let's continue the main thread

                if (setSharesEvent) // we will monitor further changes in the registry
                {
                    if (MainWindowCS.LockIfNotClosed())
                    {
                        if (MainWindow != NULL)
                            PostMessage(MainWindow->HWindow, WM_USER_REFRESH_SHARES, 0, 0);
                        MainWindowCS.Unlock();
                    }
                    if ((res = RegNotifyChangeKeyValue(sharesKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, SharesEvent,
                                                       TRUE)) != ERROR_SUCCESS)
                    {
                        TRACE_E("Unable to monitor registry (LanMan Shares). error: " << GetErrorText(res));
                    }
                }

                ObjectArray[2] = BeginSuspendEvent;
                TRACE_I("End suspend mode");

                CALL_STACK_MESSAGE1("ThreadSnooperBody::post_refresh");

                HANDLES(EnterCriticalSection(&TimeCounterSection));
                // refresh the changed panels
                int i;
                for (i = 0; i < refreshPanels.Count; i++)
                {
                    HWND wnd = refreshPanels[i];
                    if (IsWindow(wnd))
                    {
                        PostMessage(wnd, WM_USER_S_REFRESH_DIR, FALSE, MyTimeCounter++);
                    }
                }
                HANDLES(LeaveCriticalSection(&TimeCounterSection));
                // We will still send a notification about exiting suspend mode
                if (MainWindowCS.LockIfNotClosed())
                {
                    if (MainWindow != NULL && MainWindow->LeftPanel != NULL && MainWindow->RightPanel != NULL)
                    {
                        PostMessage(MainWindow->LeftPanel->HWindow, WM_USER_SM_END_NOTIFY, 0, 0);
                        PostMessage(MainWindow->RightPanel->HWindow, WM_USER_SM_END_NOTIFY, 0, 0);
                    }
                    MainWindowCS.Unlock();
                }

                if (refreshPanels.Count > 0)
                {
                    // Let's take a break so the system doesn't get overloaded
                    ignoreRefreshes = TRUE;
                    ignoreRefreshesAbsTimeout = GetTickCount() + REFRESH_PAUSE;
                }
                break;
            }

            case WAIT_OBJECT_0 + 3: // SharesEvent
            {                       // let's refresh the panels
                if (MainWindowCS.LockIfNotClosed())
                {
                    if (MainWindow != NULL)
                        PostMessage(MainWindow->HWindow, WM_USER_REFRESH_SHARES, 0, 0);
                    MainWindowCS.Unlock();
                }
                // we will monitor further changes in the registry
                if ((res = RegNotifyChangeKeyValue(sharesKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, SharesEvent,
                                                   TRUE)) != ERROR_SUCCESS)
                {
                    TRACE_E("Unable to monitor registry (LanMan Shares). error: " << GetErrorText(res));
                }
                break;
            }

            case WAIT_TIMEOUT:
                break; // we are ignoring (end of ignoring changes in directories mode)

            default:
            {
                int index;
                index = res - WAIT_OBJECT_0;
                if (index < 0 || index >= WindowArray.Count)
                {
                    DWORD err = GetLastError();
                    TRACE_E("Unexpected value returned from WaitForMultipleObjects(): " << res);
                    break; // in case of any other value res
                }

                // Calling FindNextChangeNotification invalidates other handles on the same path
                // (doing at UNC paths), therefore we simulate the signaled state forcibly
                HANDLE sameHandle = NULL; // != NULL -> handle in the same way
                CFilesWindow* actWin = WindowArray[index];
                int e;
                for (e = 0; e < WindowArray.Count; e++)
                {
                    CFilesWindow* w = WindowArray[e];
                    if (w != NULL && w != actWin && actWin->SamePath(w))
                    {
                        sameHandle = (HANDLE)ObjectArray[e];
                        break;
                    }
                }

                if (MainWindowCS.LockIfNotClosed())
                {
                    //            TRACE_I("Change notification: " << (MainWindow->LeftPanel == WindowArray[index] ? "left" : "right"));
                    MainWindowCS.Unlock();
                }
                HANDLES(EnterCriticalSection(&TimeCounterSection));
                PostMessage(WindowArray[index]->HWindow, WM_USER_REFRESH_DIR, TRUE, MyTimeCounter++);
                HANDLES(LeaveCriticalSection(&TimeCounterSection));
                FindNextChangeNotification((HANDLE)ObjectArray[index]); // Canceling this change
                                                                        // indexes can change...
            ERROR_BYPASS:

                HANDLE objects[4];
                objects[0] = WantDataEvent;        // data can change in the refresh
                objects[1] = TerminateEvent;       // in case of reaching the end without refreshing
                objects[2] = BeginSuspendEvent;    // in case BeginSuspendMode is called during refresh
                objects[3] = RefreshFinishedEvent; // message from the main thread about the termination of r.

                BOOL refreshNotFinished = TRUE;
                while (refreshNotFinished) // waiting for processing
                {                          // Handling everything except changes in directories
                    res = WaitForMultipleObjects(4, objects, FALSE, INFINITE);

                    switch (res)
                    {
                    case WAIT_OBJECT_0 + 0:
                        DoWantDataEvent();
                        break;              // WantDataEvent
                    case WAIT_OBJECT_0 + 1: // TerminateEvent
                        refreshNotFinished = notEnd = FALSE;
                        break;
                    case WAIT_OBJECT_0 + 2: // BeginSuspendEvent
                        refreshNotFinished = FALSE;
                        SetEvent(BeginSuspendEvent);
                        break;
                    default:
                        refreshNotFinished = FALSE;
                        break; // RefreshFinishedEvent
                    }
                }

                // if it is necessary to bypass a system error, we will do it here
                if (sameHandle != NULL)
                {
                    for (index = 0; index < ObjectArray.Count; index++)
                    {
                        if (sameHandle == (HANDLE)ObjectArray[index])
                        {
                            int r = WaitForSingleObject(sameHandle, 0); // Simulation of the wait function in case the error disappears
                            sameHandle = NULL;

                            HANDLES(EnterCriticalSection(&TimeCounterSection));
                            PostMessage(WindowArray[index]->HWindow, WM_USER_REFRESH_DIR, TRUE, MyTimeCounter++);
                            HANDLES(LeaveCriticalSection(&TimeCounterSection));

                            if (r != WAIT_TIMEOUT) // if there is no error we look for the next change
                            {
                                FindNextChangeNotification((HANDLE)ObjectArray[index]); // Canceling this change
                            }

                            goto ERROR_BYPASS;
                        }
                    }
                }

                // Let's take a break so the system doesn't get overloaded
                ignoreRefreshes = TRUE;
                ignoreRefreshesAbsTimeout = GetTickCount() + REFRESH_PAUSE;

                break;
            }
            }
        }
        ReleaseMutex(DataUsageMutex);
    }
    if (sharesKey != NULL)
        HANDLES(RegCloseKey(sharesKey));
    TRACE_I("End");
    return 0;
}

unsigned ThreadSnooperEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ThreadSnooperBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread Snooper: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ThreadSnooper(void* param)
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return ThreadSnooperEH(param);
}

BOOL InitializeThread()
{
    //--- creating an event and a mutex for synchronization
    DataUsageMutex = HANDLES(CreateMutex(NULL, FALSE, NULL));
    if (DataUsageMutex == NULL)
    {
        TRACE_E("Unable to create DataUsageMutex mutex.");
        return FALSE;
    }
    WantDataEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (WantDataEvent == NULL)
    {
        TRACE_E("Unable to create WantDataEvent event.");
        return FALSE;
    }
    ContinueEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (ContinueEvent == NULL)
    {
        TRACE_E("Unable to create ContinueEvent event.");
        return FALSE;
    }
    RefreshFinishedEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (RefreshFinishedEvent == NULL)
    {
        TRACE_E("Unable to create RefreshFinishedEvent event.");
        return FALSE;
    }
    TerminateEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (TerminateEvent == NULL)
    {
        TRACE_E("Unable to create TerminateEvent event.");
        return FALSE;
    }
    BeginSuspendEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (BeginSuspendEvent == NULL)
    {
        TRACE_E("Unable to create BeginSuspendEvent event.");
        return FALSE;
    }
    EndSuspendEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (EndSuspendEvent == NULL)
    {
        TRACE_E("Unable to create EndSuspendEvent event.");
        return FALSE;
    }
    SharesEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (SharesEvent == NULL)
    {
        TRACE_E("Unable to create SharesEvent event.");
        return FALSE;
    }

    // event "starter" for thread "safe handle killer"
    SafeFindCloseStart = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (SafeFindCloseStart == NULL)
    {
        TRACE_E("Unable to create SafeFindCloseStart event.");
        return FALSE;
    }
    SafeFindCloseFinished = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (SafeFindCloseFinished == NULL)
    {
        TRACE_E("Unable to create SafeFindCloseFinished event.");
        return FALSE;
    }

    HANDLES(InitializeCriticalSection(&TimeCounterSection));
    //--- start of the cmuchala thread
    DWORD ThreadID;
    Thread = HANDLES(CreateThread(NULL, 0, ThreadSnooper, NULL, 0, &ThreadID));
    if (Thread == NULL)
    {
        TRACE_E("Unable to start Snooper thread.");
        return FALSE;
    }
    //  SetThreadPriority(Thread, THREAD_PRIORITY_LOWEST);
    WaitForSingleObject(ContinueEvent, INFINITE); // Wait for the data acquisition to finish

    HANDLES(InitializeCriticalSection(&SafeFindCloseCS));
    //--- start thread "safe handle killer"
    SafeFindCloseThread = HANDLES(CreateThread(NULL, 0, ThreadFindCloseChangeNotification, NULL, 0, &ThreadID));
    if (SafeFindCloseThread == NULL)
    {
        TRACE_E("Unable to start safe-handle-killer thread.");
        return FALSE;
    }
    // It is necessary to increase the priority so that it runs ahead of the main thread (main thread
    // needs to have handles closed immediately, no active waiting in case of an error, cool)
    SetThreadPriority(SafeFindCloseThread, THREAD_PRIORITY_HIGHEST);

    return TRUE;
}

void TerminateThread()
{
    if (Thread != NULL) // thread termination cmuchala
    {
        SetEvent(TerminateEvent);              // Ask cmuchala to terminate the activity
        WaitForSingleObject(Thread, INFINITE); // wait until it dies
        HANDLES(CloseHandle(Thread));          // close the handle of the thread
    }
    if (DataUsageMutex != NULL)
        HANDLES(CloseHandle(DataUsageMutex));
    if (RefreshFinishedEvent != NULL)
        HANDLES(CloseHandle(RefreshFinishedEvent));
    if (WantDataEvent != NULL)
        HANDLES(CloseHandle(WantDataEvent));
    if (ContinueEvent != NULL)
        HANDLES(CloseHandle(ContinueEvent));
    if (TerminateEvent != NULL)
        HANDLES(CloseHandle(TerminateEvent));
    if (BeginSuspendEvent != NULL)
        HANDLES(CloseHandle(BeginSuspendEvent));
    if (EndSuspendEvent != NULL)
        HANDLES(CloseHandle(EndSuspendEvent));
    if (SharesEvent != NULL)
        HANDLES(CloseHandle(SharesEvent));
    HANDLES(DeleteCriticalSection(&TimeCounterSection));

    if (SafeFindCloseThread != NULL)
    {
        SafeFindCloseTerminate = TRUE; // Locking the thread
        SetEvent(SafeFindCloseStart);
        if (WaitForSingleObject(SafeFindCloseThread, 1000) == WAIT_TIMEOUT) // wait until it finishes
        {
            TerminateThread(SafeFindCloseThread, 666);          // It didn't work out, we'll kill him for sure
            WaitForSingleObject(SafeFindCloseThread, INFINITE); // Wait until the thread actually finishes, sometimes it takes quite a while
        }
        HANDLES(CloseHandle(SafeFindCloseThread));
    }
    if (SafeFindCloseStart != NULL)
        HANDLES(CloseHandle(SafeFindCloseStart));
    if (SafeFindCloseFinished != NULL)
        HANDLES(CloseHandle(SafeFindCloseFinished));
    HANDLES(DeleteCriticalSection(&SafeFindCloseCS));
}

void AddDirectory(CFilesWindow* win, const char* path, BOOL registerDevNotification)
{
    CALL_STACK_MESSAGE3("AddDirectory(, %s, %d)", path, registerDevNotification);
    SetEvent(WantDataEvent);                       // Request cmuchala to release the DataUsageMutex
    WaitForSingleObject(DataUsageMutex, INFINITE); // we will wait for him
    SetEvent(WantDataEvent);                       // cmuchal can now start waiting for DataUsageMutex
                                                   //--- now the main thread data is available, cmuchal is waiting
    // if the path ends with a space/dot, we need to append '\\', otherwise FindFirstChangeNotification
    // Trims spaces/dots and operates with a different path
    char pathCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(path, pathCopy);
    HANDLE h = HANDLES_Q(FindFirstChangeNotification(path, FALSE,
                                                     FILE_NOTIFY_CHANGE_FILE_NAME |
                                                         FILE_NOTIFY_CHANGE_DIR_NAME |
                                                         FILE_NOTIFY_CHANGE_ATTRIBUTES |
                                                         FILE_NOTIFY_CHANGE_SIZE |
                                                         FILE_NOTIFY_CHANGE_LAST_WRITE));
    if (h != INVALID_HANDLE_VALUE)
    {
        win->SetAutomaticRefresh(TRUE);
        WindowArray.Add(win);
        ObjectArray.Add(h);

        if (registerDevNotification)
        {
            // Register the panel window to receive messages about media changes (deletion, etc.)
            DEV_BROADCAST_HANDLE dbh;
            memset(&dbh, 0, sizeof(dbh));
            dbh.dbch_size = sizeof(dbh);
            dbh.dbch_devicetype = DBT_DEVTYP_HANDLE;
            dbh.dbch_handle = h;
            if (win->DeviceNotification != NULL)
            {
                TRACE_E("AddDirectory(): unexpected situation: win->DeviceNotification != NULL");
                UnregisterDeviceNotification(win->DeviceNotification);
            }
            win->DeviceNotification = RegisterDeviceNotificationA(win->HWindow, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
        }
    }
    else
    {
        win->SetAutomaticRefresh(FALSE);
        TRACE_W("Unable to receive change notifications for directory '" << path << "' (auto-refresh will not work).");
    }
    //---
    ReleaseMutex(DataUsageMutex);                 // Release the DataUsageMutex for Cmucha
    WaitForSingleObject(ContinueEvent, INFINITE); // and wait until he takes it
}

// thread in which we will close the trade on a "disconnected" network device (long waiting)
unsigned ThreadFindCloseChangeNotificationBody(void* param)
{
    CALL_STACK_MESSAGE1("ThreadFindCloseChangeNotificationBody()");
    SetThreadNameInVCAndTrace("SafeHandleKiller");
    //  TRACE_I("Begin");

    while (!SafeFindCloseTerminate)
    {
        WaitForSingleObject(SafeFindCloseStart, INFINITE); // waiting for start or finish

        while (1)
        {
            // retrieve handle
            HANDLES(EnterCriticalSection(&SafeFindCloseCS));
            HANDLE h;
            BOOL br = FALSE;

            if (SafeFindCloseCNArr.IsGood() && SafeFindCloseCNArr.Count > 0)
            {
                h = SafeFindCloseCNArr[SafeFindCloseCNArr.Count - 1];
                SafeFindCloseCNArr.Delete(SafeFindCloseCNArr.Count - 1);
                if (!SafeFindCloseCNArr.IsGood())
                    SafeFindCloseCNArr.ResetState(); // Cannot resize, reports only lack of memory to shrink the array
            }
            else
                br = TRUE;
            HANDLES(LeaveCriticalSection(&SafeFindCloseCS));

            if (br)
                break; // There is nothing to close anymore, we will wait for the next start

            // close the handle
            //      TRACE_I("Killing ... " << h);
            HANDLES(FindCloseChangeNotification(h));
        }

        SetEvent(SafeFindCloseFinished); // let's start the main thread further ...
    }
    //  TRACE_I("End");
    return 0;
}

unsigned ThreadFindCloseChangeNotificationEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ThreadFindCloseChangeNotificationBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Safe Handle Killer: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ThreadFindCloseChangeNotification(void* param)
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return ThreadFindCloseChangeNotificationEH(param);
}

void ChangeDirectory(CFilesWindow* win, const char* newPath, BOOL registerDevNotification)
{
    CALL_STACK_MESSAGE3("ChangeDirectory(, %s, %d)", newPath, registerDevNotification);
    SetEvent(WantDataEvent);                       // Request cmuchala to release the DataUsageMutex
    WaitForSingleObject(DataUsageMutex, INFINITE); // we will wait for him
    SetEvent(WantDataEvent);                       // cmuchal can now start waiting for DataUsageMutex
    BOOL registerDevNot = FALSE;
    HANDLE registerDevNotHandle = NULL;
    //--- now the main thread data is available, cmuchal is waiting
    if (win->DeviceNotification != NULL)
    {
        UnregisterDeviceNotification(win->DeviceNotification);
        win->DeviceNotification = NULL;
    }

    int i;
    for (i = 0; i < WindowArray.Count; i++)
        if (win == WindowArray[i])
        {
            // if the change notification is on a disconnected network disk
            // can't afford to wait ... let's have another thread close it
            HANDLES(EnterCriticalSection(&SafeFindCloseCS));
            SafeFindCloseCNArr.Add(ObjectArray[i]);
            if (!SafeFindCloseCNArr.IsGood())
                SafeFindCloseCNArr.ResetState(); // ignoring errors
            HANDLES(LeaveCriticalSection(&SafeFindCloseCS));
            ResetEvent(SafeFindCloseFinished);               // we will wait for the launch...
            SetEvent(SafeFindCloseStart);                    // start cleaning
            WaitForSingleObject(SafeFindCloseFinished, 200); // 200 ms time-out for closing the trade

            // if the path ends with a space/dot, we need to append '\\', otherwise FindFirstChangeNotification
            // Trims spaces/dots and operates with a different path
            char newPathCopy[3 * MAX_PATH];
            MakeCopyWithBackslashIfNeeded(newPath, newPathCopy);
            ObjectArray[i] = HANDLES_Q(FindFirstChangeNotification(newPath, FALSE,
                                                                   FILE_NOTIFY_CHANGE_FILE_NAME |
                                                                       FILE_NOTIFY_CHANGE_DIR_NAME |
                                                                       FILE_NOTIFY_CHANGE_ATTRIBUTES |
                                                                       FILE_NOTIFY_CHANGE_SIZE |
                                                                       FILE_NOTIFY_CHANGE_LAST_WRITE));
            if ((HANDLE)ObjectArray[i] == INVALID_HANDLE_VALUE)
            {
                win->SetAutomaticRefresh(FALSE);
                ObjectArray.Delete(i); // we remove him from the list
                WindowArray.Delete(i);
                TRACE_W("Unable to receive change notifications for directory '" << newPath << "' (auto-refresh will not work).");
            }
            else
            {
                if (registerDevNotification)
                {
                    registerDevNot = TRUE;
                    registerDevNotHandle = (HANDLE)ObjectArray[i];
                }
            }
            break;
        }
    //--- not found -> adding
    if (i == WindowArray.Count)
    {
        // if the path ends with a space/dot, we need to append '\\', otherwise FindFirstChangeNotification
        // Trims spaces/dots and operates with a different path
        char newPathCopy[3 * MAX_PATH];
        MakeCopyWithBackslashIfNeeded(newPath, newPathCopy);
        HANDLE h = HANDLES_Q(FindFirstChangeNotification(newPath, FALSE,
                                                         FILE_NOTIFY_CHANGE_FILE_NAME |
                                                             FILE_NOTIFY_CHANGE_DIR_NAME |
                                                             FILE_NOTIFY_CHANGE_ATTRIBUTES |
                                                             FILE_NOTIFY_CHANGE_SIZE |
                                                             FILE_NOTIFY_CHANGE_LAST_WRITE));
        if (h != INVALID_HANDLE_VALUE)
        {
            win->SetAutomaticRefresh(TRUE);
            WindowArray.Add(win);
            ObjectArray.Add(h);
            if (registerDevNotification)
            {
                registerDevNot = TRUE;
                registerDevNotHandle = h;
            }
        }
        else
        {
            win->SetAutomaticRefresh(FALSE);
            TRACE_W("Unable to receive change notifications for directory '" << newPath << "' (auto-refresh will not work).");
        }
    }
    if (registerDevNot)
    {
        // Register the panel window to receive messages about media changes (deletion, etc.)
        DEV_BROADCAST_HANDLE dbh;
        memset(&dbh, 0, sizeof(dbh));
        dbh.dbch_size = sizeof(dbh);
        dbh.dbch_devicetype = DBT_DEVTYP_HANDLE;
        dbh.dbch_handle = registerDevNotHandle;
        win->DeviceNotification = RegisterDeviceNotificationA(win->HWindow, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
    }
    //---
    ReleaseMutex(DataUsageMutex);                 // Release the DataUsageMutex for Cmucha
    WaitForSingleObject(ContinueEvent, INFINITE); // and wait until he takes it
}

void DetachDirectory(CFilesWindow* win, BOOL waitForHandleClosure, BOOL closeDevNotifification)
{
    CALL_STACK_MESSAGE3("DetachDirectory(, %d, %d)", waitForHandleClosure, closeDevNotifification);
    SetEvent(WantDataEvent);                       // Request cmuchala to release the DataUsageMutex
    WaitForSingleObject(DataUsageMutex, INFINITE); // we will wait for him
    SetEvent(WantDataEvent);                       // cmuchal can now start waiting for DataUsageMutex
                                                   //--- now the main thread data is available, cmuchal is waiting
    if (closeDevNotifification && win->DeviceNotification != NULL)
    {
        UnregisterDeviceNotification(win->DeviceNotification);
        win->DeviceNotification = NULL;
    }

    int i;
    for (i = 0; i < WindowArray.Count; i++)
        if (win == WindowArray[i])
        {
            // if the change notification is on a disconnected network disk
            // can't afford to wait ... let's have another thread close it
            HANDLES(EnterCriticalSection(&SafeFindCloseCS));
            SafeFindCloseCNArr.Add(ObjectArray[i]);
            if (!SafeFindCloseCNArr.IsGood())
                SafeFindCloseCNArr.ResetState(); // ignoring errors
            HANDLES(LeaveCriticalSection(&SafeFindCloseCS));
            ResetEvent(SafeFindCloseFinished);                                             // we will wait for the launch...
            SetEvent(SafeFindCloseStart);                                                  // start cleaning
            WaitForSingleObject(SafeFindCloseFinished, waitForHandleClosure ? 5000 : 200); // 200 ms time-out for closing the trade

            ObjectArray.Delete(i); // we remove him from the list
            WindowArray.Delete(i);
            win->SetAutomaticRefresh(FALSE);
        }
    //---
    ReleaseMutex(DataUsageMutex);                 // Release the DataUsageMutex for Cmucha
    WaitForSingleObject(ContinueEvent, INFINITE); // and wait until he takes it
}

/*  #define SUSPMODESTACKSIZE 50

class CSuspModeStack
{
  protected:
    DWORD CallerCalledFromArr[SUSPMODESTACKSIZE];  // array of return addresses of functions from which BeginSuspendMode() was called
    DWORD CalledFromArr[SUSPMODESTACKSIZE];        // array of addresses from which BeginSuspendMode() was called
    int Count;                                     // number of elements in the previous two arrays
    int Ignored;                                   // number of calls to BeginSuspendMode() that had to be ignored (SUSPMODESTACKSIZE too small -> potentially increase)

  public:
    CSuspModeStack() {Count = 0; Ignored = 0;}
    ~CSuspModeStack() {CheckIfEmpty(1);}  // one BeginSuspendMode() is OK: called when deactivating the main Salama window (before closing the main window)

    void Push(DWORD caller_called_from, DWORD called_from);
    void Pop(DWORD caller_called_from, DWORD called_from);
    void CheckIfEmpty(int checkLevel);
};

void
CSuspModeStack::Push(DWORD caller_called_from, DWORD called_from)
{
  if (Count < SUSPMODESTACKSIZE)
  {
    CallerCalledFromArr[Count] = caller_called_from;
    CalledFromArr[Count] = called_from;
    Count++;
  }
  else
  {
    Ignored++;
    TRACE_E("CSuspModeStack::Push(): you should increase SUSPMODESTACKSIZE! ignored=" << Ignored);
  }
}

void
CSuspModeStack::Pop(DWORD caller_called_from, DWORD called_from)
{
  if (Ignored == 0)
  {
    if (Count > 0)
    {
      Count--;
      if (CallerCalledFromArr[Count] != caller_called_from)
      {
        TRACE_E("CSuspModeStack::Pop(): strange situation: BeginCallerCalledFrom!=StopCallerCalledFrom - BeginCalledFrom,StopCalledFrom");
        TRACE_E("CSuspModeStack::Pop(): strange situation: 0x" << std::hex <<
                CallerCalledFromArr[Count] << "!=0x" << caller_called_from << " - 0x" <<
                CalledFromArr[Count] << ",0x" << called_from << std::dec);
      }
    }
    else TRACE_E("CSuspModeStack::Pop(): unexpected call!");
  }
  else Ignored--;
}

void
CSuspModeStack::CheckIfEmpty(int checkLevel)
{
  if (Count > checkLevel)
  {
    TRACE_E("CSuspModeStack::CheckIfEmpty(" << checkLevel << "): listing remaining BeginSuspendMode calls: CallerCalledFrom,CalledFrom");
    int i;
    for (i = 0; i < Count; i++)
    {
      TRACE_E("CSuspModeStack::CheckIfEmpty():: 0x" << std::hex <<
              CallerCalledFromArr[i] << ",0x" << CalledFromArr[i] << std::dec);
    }
  }
}

CSuspModeStack SuspModeStack;*/

void BeginSuspendMode(BOOL debugDoNotTestCaller)
{
    /*  #ifdef _DEBUG     // testing if BeginSuspendMode() and EndSuspendMode() are called from the same function (based on the return address of the calling function -> so it won't detect an "error" when called from different functions, both called from the same function)
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

    // if this code ever needs to be revived, take advantage of the fact that it can be replaced (x86 and x64):
    called_from = *(DWORD_PTR *)_AddressOfReturnAddress();

    caller_called_from = *(DWORD*)((char*)(*register_ebp) + 4);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    called_from = -1;
    caller_called_from = -1;
  }
  SuspModeStack.Push(debugDoNotTestCaller ? 0 : caller_called_from, called_from);
#endif // _DEBUG*/

    if (SnooperSuspended == 0)
    {
        SetEvent(BeginSuspendEvent);
        WaitForSingleObject(ContinueEvent, INFINITE);
    }
    SnooperSuspended++;
}

//#ifdef _DEBUG
//void EndSuspendModeBody()
//#else // _DEBUG
void EndSuspendMode(BOOL debugDoNotTestCaller)
//#endif // _DEBUG
{
    CALL_STACK_MESSAGE1("EndSuspendMode()");

    if (SnooperSuspended < 1)
    {
        TRACE_E("Incorrect call to EndSuspendMode()");
        SnooperSuspended = 0; // Doesn't anyone stupidly use CM_LEFTREFRESH, CM_RIGHTREFRESH or CM_ACTIVEREFRESH again?
    }
    else
    {
        if (SnooperSuspended == 1)
        {
            SetEvent(EndSuspendEvent);
            WaitForSingleObject(ContinueEvent, INFINITE);
        }
        SnooperSuspended--;
    }
}

/*  #ifdef _DEBUG     // testing if BeginSuspendMode() and EndSuspendMode() are called from the same function (based on the return address of the calling function -> so it won't detect an "error" when called from different functions, both called from the same function)
void EndSuspendMode(BOOL debugDoNotTestCaller)
{
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

    if this code ever needs to be revived, take advantage of the fact that it can be replaced (x86 and x64):
    called_from = *(DWORD_PTR *)_AddressOfReturnAddress();

    caller_called_from = *(DWORD*)((char*)(*register_ebp) + 4);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    called_from = -1;
    caller_called_from = -1;
  }
  SuspModeStack.Pop(debugDoNotTestCaller ? 0 : caller_called_from, called_from);

  EndSuspendModeBody();
}
#endif // _DEBUG*/
