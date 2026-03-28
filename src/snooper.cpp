// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "snooper.h"

CWindowArray WindowArray(10, 5);
CObjectArray ObjectArray(10, 5);

HANDLE Thread = NULL;
HANDLE DataUsageMutex = NULL;       // for arrays with data shared by the thread and the process
HANDLE RefreshFinishedEvent = NULL; // due to "PostMessage" waits for processing to complete
HANDLE WantDataEvent = NULL;        // main thread requests access to the shared data
HANDLE TerminateEvent = NULL;       // main thread requests termination of the snooper thread
HANDLE ContinueEvent = NULL;        // helper event used for synchronization
HANDLE BeginSuspendEvent = NULL;    // beginning of suspend mode
HANDLE EndSuspendEvent = NULL;      // end of the snooper's suspend mode
HANDLE SharesEvent = NULL;          // signaled when LanMan Shares changes

int SnooperSuspended = 0;

CRITICAL_SECTION TimeCounterSection; // synchronizes access to MyTimeCounter
int MyTimeCounter = 0;               // current time

HANDLE SafeFindCloseThread = NULL;              // "safe handle killer" thread
TDirectArray<HANDLE> SafeFindCloseCNArr(10, 5); // safely (without hanging) closes change-notify handles
CRITICAL_SECTION SafeFindCloseCS;               // critical section for accessing the handle array
BOOL SafeFindCloseTerminate = FALSE;            // until thread termination is requested
HANDLE SafeFindCloseStart = NULL;               // thread "starter"—waits while non-signaled
HANDLE SafeFindCloseFinished = NULL;            // signaled once the thread has closed all handles

DWORD WINAPI ThreadFindCloseChangeNotification(void* param);

void DoWantDataEvent()
{
    ReleaseMutex(DataUsageMutex);                  // release the data to the main thread
    WaitForSingleObject(WantDataEvent, INFINITE);  // wait until it takes ownership
    WaitForSingleObject(DataUsageMutex, INFINITE); // once it finishes, take ownership again
    SetEvent(ContinueEvent);                       // we own it again, let the main thread continue
}

unsigned ThreadSnooperBody(void* /*param*/) // do not call main-thread functions (not even TRACE) !!!
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
    else // the key is OK, enable notifications (otherwise RegNotifyChangeKeyValue will not be called again)
    {
        if ((res = RegNotifyChangeKeyValue(sharesKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, SharesEvent,
                                           TRUE)) != ERROR_SUCCESS)
        {
            TRACE_E("Unable to monitor registry (LanMan Shares). error: " << GetErrorText(res));
        }
    }

    if (WaitForSingleObject(DataUsageMutex, INFINITE) == WAIT_OBJECT_0)
    {
        SetEvent(ContinueEvent); // the data now belong to the snooper; the main thread can continue

        WindowArray.Add(NULL); // base objects; must come first!
        WindowArray.Add(NULL);
        WindowArray.Add(NULL);
        WindowArray.Add(NULL);
        ObjectArray.Add(WantDataEvent);
        ObjectArray.Add(TerminateEvent);
        ObjectArray.Add(BeginSuspendEvent);
        ObjectArray.Add(SharesEvent);

        BOOL ignoreRefreshes = FALSE;        // TRUE = ignore refreshes (directory changes); otherwise operate normally
        DWORD ignoreRefreshesAbsTimeout = 0; // when (int)(GetTickCount() - ignoreRefreshesAbsTimeout) >= 0, set ignoreRefreshes to FALSE
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

                SetEvent(ContinueEvent); // we are already in suspend; allow the main thread to continue

                TDirectArray<HWND> refreshPanels(10, 5); // for the case where the monitored directory is deleted

                ObjectArray[2] = EndSuspendEvent; // replace the begin event with the end-of-suspend event

                BOOL setSharesEvent = FALSE; // TRUE => rearm registry monitoring
                BOOL suspendNotFinished = TRUE;
                while (suspendNotFinished) // wait for suspend mode to end
                {                          // handle everything except directory changes
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
                        // refresh shares and, if needed, panels (via WM_USER_REFRESH_SHARES)
                        setSharesEvent = TRUE;
                        break;
                    }

                    case WAIT_TIMEOUT:
                        break; // ignore it (the mode for ignoring directory changes has just ended)

                    default:
                    {
                        int index = res - WAIT_OBJECT_0;
                        if (index < 0 || index >= WindowArray.Count)
                        {
                            TRACE_E("Unexpected value returned from WaitForMultipleObjects(): " << res);
                            break; // in case res holds some other value
                        }

                        // calling FindCloseChangeNotification invalidates other handles for the same path
                        // (happens for UNC paths), so simulate the signaled state manually
                        HANDLE sameHandle = NULL; // != NULL -> handle for the same path
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

                        // a change already occurred; ignore further ones because a refresh will follow after suspend
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
                        refreshPanels.Add(WindowArray[index]->HWindow); // add to the refresh list
                        ObjectArray.Delete(index);                      // remove it from the list
                        WindowArray.Delete(index);

                        // work around any system bug here
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
                                    refreshPanels.Add(WindowArray[index]->HWindow); // add to the refresh list
                                    ObjectArray.Delete(index);                      // remove it from the list
                                    WindowArray.Delete(index);
                                }
                            }
                        }
                        break;
                    }
                    }
                }
                SetEvent(ContinueEvent); // no longer suspended; allow the main thread to continue

                if (setSharesEvent) // resume monitoring further registry changes
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
                // refresh panels that changed
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
                // also notify that suspend mode ended
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
                    // pause briefly so the system is not overwhelmed
                    ignoreRefreshes = TRUE;
                    ignoreRefreshesAbsTimeout = GetTickCount() + REFRESH_PAUSE;
                }
                break;
            }

            case WAIT_OBJECT_0 + 3: // SharesEvent
            {                       // let the panels refresh
                if (MainWindowCS.LockIfNotClosed())
                {
                    if (MainWindow != NULL)
                        PostMessage(MainWindow->HWindow, WM_USER_REFRESH_SHARES, 0, 0);
                    MainWindowCS.Unlock();
                }
                // resume monitoring further registry changes
                if ((res = RegNotifyChangeKeyValue(sharesKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, SharesEvent,
                                                   TRUE)) != ERROR_SUCCESS)
                {
                    TRACE_E("Unable to monitor registry (LanMan Shares). error: " << GetErrorText(res));
                }
                break;
            }

            case WAIT_TIMEOUT:
                break; // ignore it (end of directory change ignoring mode)

            default:
            {
                int index;
                index = res - WAIT_OBJECT_0;
                if (index < 0 || index >= WindowArray.Count)
                {
                    DWORD err = GetLastError();
                    TRACE_E("Unexpected value returned from WaitForMultipleObjects(): " << res);
                    break; // in case res holds some other value
                }

                // calling FindNextChangeNotification invalidates other handles for the same path
                // (happens for UNC paths), so simulate the signaled state manually
                HANDLE sameHandle = NULL; // != NULL -> handle for the same path
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
                FindNextChangeNotification((HANDLE)ObjectArray[index]); // discard this change
                                                                        // indexes might change...
            ERROR_BYPASS:

                HANDLE objects[4];
                objects[0] = WantDataEvent;        // data may change during the refresh
                objects[1] = TerminateEvent;       // in case it terminates before the refresh finishes
                objects[2] = BeginSuspendEvent;    // in case BeginSuspendMode is called during the refresh
                objects[3] = RefreshFinishedEvent; // message from the main thread about finishing the refresh

                BOOL refreshNotFinished = TRUE;
                while (refreshNotFinished) // wait for processing to finish
                {                          // handle everything except directory changes
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

                // work around any system bug here
                if (sameHandle != NULL)
                {
                    for (index = 0; index < ObjectArray.Count; index++)
                    {
                        if (sameHandle == (HANDLE)ObjectArray[index])
                        {
                            int r = WaitForSingleObject(sameHandle, 0); // simulate the wait function in case the error clears
                            sameHandle = NULL;

                            HANDLES(EnterCriticalSection(&TimeCounterSection));
                            PostMessage(WindowArray[index]->HWindow, WM_USER_REFRESH_DIR, TRUE, MyTimeCounter++);
                            HANDLES(LeaveCriticalSection(&TimeCounterSection));

                            if (r != WAIT_TIMEOUT) // if the error is cleared, request the next change
                            {
                                FindNextChangeNotification((HANDLE)ObjectArray[index]); // discard this change
                            }

                            goto ERROR_BYPASS;
                        }
                    }
                }

                // take a break to avoid overloading the system
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
        TerminateProcess(GetCurrentProcess(), 1); // more forceful exit (this one still calls something)
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
    //--- create events and the mutex for synchronization
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

    // "starter" event for the "safe handle killer" thread
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
    //---  start the snooper thread
    DWORD ThreadID;
    Thread = HANDLES(CreateThread(NULL, 0, ThreadSnooper, NULL, 0, &ThreadID));
    if (Thread == NULL)
    {
        TRACE_E("Unable to start Snooper thread.");
        return FALSE;
    }
    //  SetThreadPriority(Thread, THREAD_PRIORITY_LOWEST);
    WaitForSingleObject(ContinueEvent, INFINITE); // wait until the snooper acquires the data

    HANDLES(InitializeCriticalSection(&SafeFindCloseCS));
    //---  start the "safe handle killer" thread
    SafeFindCloseThread = HANDLES(CreateThread(NULL, 0, ThreadFindCloseChangeNotification, NULL, 0, &ThreadID));
    if (SafeFindCloseThread == NULL)
    {
        TRACE_E("Unable to start safe-handle-killer thread.");
        return FALSE;
    }
    // raise its priority so it runs ahead of the main thread (the main thread
    // needs the handles closed immediately; on error there is no busy waiting, so this is acceptable)
    SetThreadPriority(SafeFindCloseThread, THREAD_PRIORITY_HIGHEST);

    return TRUE;
}

void TerminateThread()
{
    if (Thread != NULL) // terminate the snooper thread
    {
        SetEvent(TerminateEvent);              // request the snooper to terminate
        WaitForSingleObject(Thread, INFINITE); // wait until it terminates
        HANDLES(CloseHandle(Thread));          // close the thread handle
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
        SafeFindCloseTerminate = TRUE; // request thread termination
        SetEvent(SafeFindCloseStart);
        if (WaitForSingleObject(SafeFindCloseThread, 1000) == WAIT_TIMEOUT) // wait for it to exit
        {
            TerminateThread(SafeFindCloseThread, 666);          // failed, kill it forcefully
            WaitForSingleObject(SafeFindCloseThread, INFINITE); // wait until the thread actually ends; this can take quite a while
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
    SetEvent(WantDataEvent);                       // ask the snooper to release the DataUsageMutex
    WaitForSingleObject(DataUsageMutex, INFINITE); // wait for it
    SetEvent(WantDataEvent);                       // the snooper can resume waiting on DataUsageMutex
                                                   //--- the data now belong to the main thread; the snooper is waiting
    // if the path ends with a space or dot we must append '\\', otherwise FindFirstChangeNotification
    // trims the trailing spaces/dots and thus works with a different path
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
            // register the panel window to receive notifications about media changes (removal, etc.)
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
    ReleaseMutex(DataUsageMutex);                 // release the DataUsageMutex back to the snooper
    WaitForSingleObject(ContinueEvent, INFINITE); // and wait until it acquires it
}

// thread used to close handles for a "disconnected" network device (long wait)
unsigned ThreadFindCloseChangeNotificationBody(void* param)
{
    CALL_STACK_MESSAGE1("ThreadFindCloseChangeNotificationBody()");
    SetThreadNameInVCAndTrace("SafeHandleKiller");
    //  TRACE_I("Begin");

    while (!SafeFindCloseTerminate)
    {
        WaitForSingleObject(SafeFindCloseStart, INFINITE); // wait for start or termination

        while (1)
        {
            // retrieve a handle
            HANDLES(EnterCriticalSection(&SafeFindCloseCS));
            HANDLE h;
            BOOL br = FALSE;

            if (SafeFindCloseCNArr.IsGood() && SafeFindCloseCNArr.Count > 0)
            {
                h = SafeFindCloseCNArr[SafeFindCloseCNArr.Count - 1];
                SafeFindCloseCNArr.Delete(SafeFindCloseCNArr.Count - 1);
                if (!SafeFindCloseCNArr.IsGood())
                    SafeFindCloseCNArr.ResetState(); // cannot fail; it only reports lack of memory when shrinking the array
            }
            else
                br = TRUE;
            HANDLES(LeaveCriticalSection(&SafeFindCloseCS));

            if (br)
                break; // nothing left to close, wait for the next start

            // close the handle
            //      TRACE_I("Killing ... " << h);
            HANDLES(FindCloseChangeNotification(h));
        }

        SetEvent(SafeFindCloseFinished); // let the main thread continue ...
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
        TerminateProcess(GetCurrentProcess(), 1); // more forceful exit (this one still performs some calls)
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
    SetEvent(WantDataEvent);                       // ask the snooper to release the DataUsageMutex
    WaitForSingleObject(DataUsageMutex, INFINITE); // wait for it
    SetEvent(WantDataEvent);                       // the snooper can resume waiting on DataUsageMutex
    BOOL registerDevNot = FALSE;
    HANDLE registerDevNotHandle = NULL;
    //--- the data now belong to the main thread; the snooper is waiting
    if (win->DeviceNotification != NULL)
    {
        UnregisterDeviceNotification(win->DeviceNotification);
        win->DeviceNotification = NULL;
    }

    int i;
    for (i = 0; i < WindowArray.Count; i++)
        if (win == WindowArray[i])
        {
            // if the change notification targets a disconnected network drive
            // we can't afford to wait... let another thread close it
            HANDLES(EnterCriticalSection(&SafeFindCloseCS));
            SafeFindCloseCNArr.Add(ObjectArray[i]);
            if (!SafeFindCloseCNArr.IsGood())
                SafeFindCloseCNArr.ResetState(); // ignore errors
            HANDLES(LeaveCriticalSection(&SafeFindCloseCS));
            ResetEvent(SafeFindCloseFinished);               // wait for it to be signaled...
            SetEvent(SafeFindCloseStart);                    // start the cleanup
            WaitForSingleObject(SafeFindCloseFinished, 200); // 200 ms timeout for closing the handle

            // if the path ends with a space or dot we must append '\\', otherwise FindFirstChangeNotification
            // trims the trailing spaces/dots and thus works with a different path
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
                ObjectArray.Delete(i); // delete it from the list
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
    //---  not found -> add it
    if (i == WindowArray.Count)
    {
        // if the path ends with a space or dot we must append '\\', otherwise FindFirstChangeNotification
        // trims the trailing spaces/dots and thus works with a different path
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
        // register the panel window to receive notifications about media changes (removal, etc.)
        DEV_BROADCAST_HANDLE dbh;
        memset(&dbh, 0, sizeof(dbh));
        dbh.dbch_size = sizeof(dbh);
        dbh.dbch_devicetype = DBT_DEVTYP_HANDLE;
        dbh.dbch_handle = registerDevNotHandle;
        win->DeviceNotification = RegisterDeviceNotificationA(win->HWindow, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
    }
    //---
    ReleaseMutex(DataUsageMutex);                 // release the DataUsageMutex back to the snooper
    WaitForSingleObject(ContinueEvent, INFINITE); // and wait until the snooper grabs it
}

void DetachDirectory(CFilesWindow* win, BOOL waitForHandleClosure, BOOL closeDevNotifification)
{
    CALL_STACK_MESSAGE3("DetachDirectory(, %d, %d)", waitForHandleClosure, closeDevNotifification);
    SetEvent(WantDataEvent);                       // ask the snooper to release the DataUsageMutex
    WaitForSingleObject(DataUsageMutex, INFINITE); // wait for it
    SetEvent(WantDataEvent);                       // the snooper can resume waiting on DataUsageMutex
                                                   //--- the data now belong to the main thread; the snooper is waiting
    if (closeDevNotifification && win->DeviceNotification != NULL)
    {
        UnregisterDeviceNotification(win->DeviceNotification);
        win->DeviceNotification = NULL;
    }

    int i;
    for (i = 0; i < WindowArray.Count; i++)
        if (win == WindowArray[i])
        {
            // if the change notification targets a disconnected network drive
            // we can't afford to wait... let another thread close it
            HANDLES(EnterCriticalSection(&SafeFindCloseCS));
            SafeFindCloseCNArr.Add(ObjectArray[i]);
            if (!SafeFindCloseCNArr.IsGood())
                SafeFindCloseCNArr.ResetState(); // ignore errors
            HANDLES(LeaveCriticalSection(&SafeFindCloseCS));
            ResetEvent(SafeFindCloseFinished);                                             // wait for it to be signaled...
            SetEvent(SafeFindCloseStart);                                                  // start the cleanup
            WaitForSingleObject(SafeFindCloseFinished, waitForHandleClosure ? 5000 : 200); // 200 ms timeout for handle closure

            ObjectArray.Delete(i); // delete it from the list
            WindowArray.Delete(i);
            win->SetAutomaticRefresh(FALSE);
        }
    //---
    ReleaseMutex(DataUsageMutex);                 // release the DataUsageMutex back to the snooper
    WaitForSingleObject(ContinueEvent, INFINITE); // and wait until the snooper grabs it
}

/*
#define SUSPMODESTACKSIZE 50

class CSuspModeStack
{
  protected:
    DWORD CallerCalledFromArr[SUSPMODESTACKSIZE];  // array of return addresses of functions from which BeginSuspendMode() was called
    DWORD CalledFromArr[SUSPMODESTACKSIZE];        // array of addresses from which BeginSuspendMode() was called
    int Count;                                     // number of elements in the previous two arrays
    int Ignored;                                   // number of BeginSuspendMode() calls we had to ignore (SUSPMODESTACKSIZE too small -> enlarge if needed)

  public:
    CSuspModeStack() {Count = 0; Ignored = 0;}
    ~CSuspModeStack() {CheckIfEmpty(1);}  // one BeginSuspendMode() is OK: invoked when the main Salamander window is deactivated (before the main window closes)

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

CSuspModeStack SuspModeStack;
*/

void BeginSuspendMode(BOOL debugDoNotTestCaller)
{
    /*
#ifdef _DEBUG     // verify whether BeginSuspendMode() and EndSuspendMode() are invoked from the same function (based on the return address of the calling function -> cannot detect a "bug" when called from different functions that are both invoked from the same function)
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

if this code ever needs to be revived, use the fact that it can be replaced (x86 and x64):
    called_from = *(DWORD_PTR *)_AddressOfReturnAddress();

    caller_called_from = *(DWORD*)((char*)(*register_ebp) + 4);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    called_from = -1;
    caller_called_from = -1;
  }
  SuspModeStack.Push(debugDoNotTestCaller ? 0 : caller_called_from, called_from);
#endif // _DEBUG
*/

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
        SnooperSuspended = 0; // reset; maybe someone is misusing CM_LEFTREFRESH, CM_RIGHTREFRESH, or CM_ACTIVEREFRESH again
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

/*
#ifdef _DEBUG     // verify whether BeginSuspendMode() and EndSuspendMode() are invoked from the same function (based on the return address of the calling function -> cannot detect a "bug" when called from different functions that are both invoked from the same function)
void EndSuspendMode(BOOL debugDoNotTestCaller)
{
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

if this code ever needs to be revived, use the fact that it can be replaced (x86 and x64):
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
#endif // _DEBUG
*/
