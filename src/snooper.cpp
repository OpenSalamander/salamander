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
HANDLE DataUsageMutex = NULL;       // kvuli arrayum s daty pro thread i proces
HANDLE RefreshFinishedEvent = NULL; // kvuli "PostMessage", ceka na zprac.
HANDLE WantDataEvent = NULL;        // hl. thread chce pristoupit ke spolecnym datum
HANDLE TerminateEvent = NULL;       // hl. thread chce terminovat thread cmuchala
HANDLE ContinueEvent = NULL;        // pomocne event pro synchronizaci
HANDLE BeginSuspendEvent = NULL;    // zacatek suspend modu
HANDLE EndSuspendEvent = NULL;      // konec suspend modu pro cmuchala
HANDLE SharesEvent = NULL;          // bude signaled pokud se zmeni LanMan Shares

int SnooperSuspended = 0;

CRITICAL_SECTION TimeCounterSection; // pro synchronizaci pristupu k MyTimeCounter
int MyTimeCounter = 0;               // aktualni cas

HANDLE SafeFindCloseThread = NULL;              // thread "safe handle killer"
TDirectArray<HANDLE> SafeFindCloseCNArr(10, 5); // bezpecne (netuhnouci) zavirani handlu change-notify
CRITICAL_SECTION SafeFindCloseCS;               // krit. sekce pro pristup do pole handlu
BOOL SafeFindCloseTerminate = FALSE;            // pro ukonceni threadu
HANDLE SafeFindCloseStart = NULL;               // "starter" threadu - je-li non-signaled, ceka
HANDLE SafeFindCloseFinished = NULL;            // signaled -> thread uz zavrel vsechny handly

DWORD WINAPI ThreadFindCloseChangeNotification(void* param);

void DoWantDataEvent()
{
    ReleaseMutex(DataUsageMutex);                  // uvolnime data pro hl. thread
    WaitForSingleObject(WantDataEvent, INFINITE);  // pockame az je zabere
    WaitForSingleObject(DataUsageMutex, INFINITE); // az skonci jsou opet nase
    SetEvent(ContinueEvent);                       // uz jsou nase, pustime dale hl. thread
}

unsigned ThreadSnooperBody(void* /*param*/) // nevolat funkce hl. threadu (ani TRACE) !!!
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
    else // klic je o.k., nahodime notifikace (bez toho se uz RegNotifyChangeKeyValue nezavola)
    {
        if ((res = RegNotifyChangeKeyValue(sharesKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, SharesEvent,
                                           TRUE)) != ERROR_SUCCESS)
        {
            TRACE_E("Unable to monitor registry (LanMan Shares). error: " << GetErrorText(res));
        }
    }

    if (WaitForSingleObject(DataUsageMutex, INFINITE) == WAIT_OBJECT_0)
    {
        SetEvent(ContinueEvent); // ted uz jsou data cmuchala, hl. thread muze pokracovat

        WindowArray.Add(NULL); // zakladni objekty, musi byt na zacatku !
        WindowArray.Add(NULL);
        WindowArray.Add(NULL);
        WindowArray.Add(NULL);
        ObjectArray.Add(WantDataEvent);
        ObjectArray.Add(TerminateEvent);
        ObjectArray.Add(BeginSuspendEvent);
        ObjectArray.Add(SharesEvent);

        BOOL ignoreRefreshes = FALSE;        // TRUE = ignorovat refreshe (zmeny v adresarich), jinak fungujeme normalne
        DWORD ignoreRefreshesAbsTimeout = 0; // az bude (int)(GetTickCount() - ignoreRefreshesAbsTimeout) >= 0, prepneme ignoreRefreshes na FALSE
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

                SetEvent(ContinueEvent); // uz jsme v suspendu -> pustime dale hl. thread

                TDirectArray<HWND> refreshPanels(10, 5); // pro pripad smazani sledovaneho adresare

                ObjectArray[2] = EndSuspendEvent; // misto beginu ted end suspend modu

                BOOL setSharesEvent = FALSE; // TRUE => znovu nahodit sledovani registry
                BOOL suspendNotFinished = TRUE;
                while (suspendNotFinished) // pockame na konec suspend modu
                {                          // osetreni vseho krome zmen v adresarich
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
                        // obnovime shary + refreshneme prip. i panely (pomoci WM_USER_REFRESH_SHARES)
                        setSharesEvent = TRUE;
                        break;
                    }

                    case WAIT_TIMEOUT:
                        break; // ignorujeme (konec rezimu ignorovani zmen v adresarich)

                    default:
                    {
                        int index = res - WAIT_OBJECT_0;
                        if (index < 0 || index >= WindowArray.Count)
                        {
                            TRACE_E("Unexpected value returned from WaitForMultipleObjects(): " << res);
                            break; // pro pripad nejake jine hodnoty res
                        }

                        // volani FindCloseChangeNotification znehodnoti ostatni handly na stejnou cestu
                        // (dela u UNC cest), proto signaled-state simulujeme nasilne
                        HANDLE sameHandle = NULL; // != NULL -> handle na stejnou cestu
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

                        // uz doslo ke zmenene, dalsi nas nezajima, za suspend prijde refresh
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
                        refreshPanels.Add(WindowArray[index]->HWindow); // pridame mezi obnovovane
                        ObjectArray.Delete(index);                      // vyhodime ho ze seznamu
                        WindowArray.Delete(index);

                        // pokud je potreba obejit chybu systemu, provedeme to zde
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
                                    refreshPanels.Add(WindowArray[index]->HWindow); // pridame mezi obnovovane
                                    ObjectArray.Delete(index);                      // vyhodime ho ze seznamu
                                    WindowArray.Delete(index);
                                }
                            }
                        }
                        break;
                    }
                    }
                }
                SetEvent(ContinueEvent); // uz nejsme suspendu -> pustime dale hl. thread

                if (setSharesEvent) // budeme sledovat dalsi zmeny v registry
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
                // refreshneme zmenene panely
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
                // jeste posleme notifikaci o ukonceni suspend modu
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
                    // dame si prestavku, aby se nezahltil system
                    ignoreRefreshes = TRUE;
                    ignoreRefreshesAbsTimeout = GetTickCount() + REFRESH_PAUSE;
                }
                break;
            }

            case WAIT_OBJECT_0 + 3: // SharesEvent
            {                       // nechame refreshout panely
                if (MainWindowCS.LockIfNotClosed())
                {
                    if (MainWindow != NULL)
                        PostMessage(MainWindow->HWindow, WM_USER_REFRESH_SHARES, 0, 0);
                    MainWindowCS.Unlock();
                }
                // budeme sledovat dalsi zmeny v registry
                if ((res = RegNotifyChangeKeyValue(sharesKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, SharesEvent,
                                                   TRUE)) != ERROR_SUCCESS)
                {
                    TRACE_E("Unable to monitor registry (LanMan Shares). error: " << GetErrorText(res));
                }
                break;
            }

            case WAIT_TIMEOUT:
                break; // ignorujeme (konec rezimu ignorovani zmen v adresarich)

            default:
            {
                int index;
                index = res - WAIT_OBJECT_0;
                if (index < 0 || index >= WindowArray.Count)
                {
                    DWORD err = GetLastError();
                    TRACE_E("Unexpected value returned from WaitForMultipleObjects(): " << res);
                    break; // pro pripad nejake jine hodnoty res
                }

                // volani FindNextChangeNotification znehodnoti ostatni handly na stejnou cestu
                // (dela u UNC cest), proto signaled-state simulujeme nasilne
                HANDLE sameHandle = NULL; // != NULL -> handle na stejnou cestu
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
                FindNextChangeNotification((HANDLE)ObjectArray[index]); // stornujem tuto zmenu
                                                                        // indexy se muzou zmenit...
            ERROR_BYPASS:

                HANDLE objects[4];
                objects[0] = WantDataEvent;        // v refreshi se muzou menit data
                objects[1] = TerminateEvent;       // pro pripad konce bez stihnuti refreshe
                objects[2] = BeginSuspendEvent;    // pro pripad volani BeginSuspendMode pri refreshi
                objects[3] = RefreshFinishedEvent; // zprava od hl. threadu o ukonceni r.

                BOOL refreshNotFinished = TRUE;
                while (refreshNotFinished) // pockame na zpracovani
                {                          // osetreni vseho krome zmen v adresarich
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

                // pokud je potreba obejit chybu systemu, provedeme to zde
                if (sameHandle != NULL)
                {
                    for (index = 0; index < ObjectArray.Count; index++)
                    {
                        if (sameHandle == (HANDLE)ObjectArray[index])
                        {
                            int r = WaitForSingleObject(sameHandle, 0); // simulace wait-funkce pro pripad, ze chyba zanikne
                            sameHandle = NULL;

                            HANDLES(EnterCriticalSection(&TimeCounterSection));
                            PostMessage(WindowArray[index]->HWindow, WM_USER_REFRESH_DIR, TRUE, MyTimeCounter++);
                            HANDLES(LeaveCriticalSection(&TimeCounterSection));

                            if (r != WAIT_TIMEOUT) // pokud neni chyba hledame dalsi zmenu
                            {
                                FindNextChangeNotification((HANDLE)ObjectArray[index]); // stornujem tuto zmenu
                            }

                            goto ERROR_BYPASS;
                        }
                    }
                }

                // dame si prestavku, aby se nezahltil system
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
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
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
    //---  vytvoreni eventu a mutexu pro synchronizaci
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

    // event "starteru" pro thread "safe handle killer"
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
    //---  start threadu cmuchala
    DWORD ThreadID;
    Thread = HANDLES(CreateThread(NULL, 0, ThreadSnooper, NULL, 0, &ThreadID));
    if (Thread == NULL)
    {
        TRACE_E("Unable to start Snooper thread.");
        return FALSE;
    }
    //  SetThreadPriority(Thread, THREAD_PRIORITY_LOWEST);
    WaitForSingleObject(ContinueEvent, INFINITE); // pockame na zabrani dat cmuchalem

    HANDLES(InitializeCriticalSection(&SafeFindCloseCS));
    //---  start threadu "safe handle killer"
    SafeFindCloseThread = HANDLES(CreateThread(NULL, 0, ThreadFindCloseChangeNotification, NULL, 0, &ThreadID));
    if (SafeFindCloseThread == NULL)
    {
        TRACE_E("Unable to start safe-handle-killer thread.");
        return FALSE;
    }
    // nutne zvyseni priority aby bezel pred hl. threadem (hl. thread
    // potrebuje mit zavrene handly okamzite, pri chybe nedochazi k aktivnimu cekani, pohoda)
    SetThreadPriority(SafeFindCloseThread, THREAD_PRIORITY_HIGHEST);

    return TRUE;
}

void TerminateThread()
{
    if (Thread != NULL) // terminovani threadu cmuchala
    {
        SetEvent(TerminateEvent);              // pozadame cmuchala o ukonceni cinnosti
        WaitForSingleObject(Thread, INFINITE); // pockame az chcipne
        HANDLES(CloseHandle(Thread));          // zavreme handle threadu
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
        SafeFindCloseTerminate = TRUE; // zakilujeme thread
        SetEvent(SafeFindCloseStart);
        if (WaitForSingleObject(SafeFindCloseThread, 1000) == WAIT_TIMEOUT) // pockame az se ukonci
        {
            TerminateThread(SafeFindCloseThread, 666);          // nepovedlo se, zabijeme ho natvrdo
            WaitForSingleObject(SafeFindCloseThread, INFINITE); // pockame az thread skutecne skonci, nekdy mu to dost trva
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
    SetEvent(WantDataEvent);                       // pozadame cmuchala o uvolneni DataUsageMutexu
    WaitForSingleObject(DataUsageMutex, INFINITE); // pockame na nej
    SetEvent(WantDataEvent);                       // cmuchal uz zase muze zacit cekat na DataUsageMutex
                                                   //---  ted uz jsou data hl. threadu, cmuchal ceka
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak FindFirstChangeNotification
    // mezery/tecky orizne a pracuje tak s jinou cestou
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
            // zaregistrujeme okno panelu pro prijem zprav o zmenach media (odstraneni, atd.)
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
    ReleaseMutex(DataUsageMutex);                 // uvolnime cmuchalovi DataUsageMutex
    WaitForSingleObject(ContinueEvent, INFINITE); // a pockame az si ho zabere
}

// thread, ve kterem provedeme zavreni handlu na "odpojene" sitove zarizeni (dlouhe cekani)
unsigned ThreadFindCloseChangeNotificationBody(void* param)
{
    CALL_STACK_MESSAGE1("ThreadFindCloseChangeNotificationBody()");
    SetThreadNameInVCAndTrace("SafeHandleKiller");
    //  TRACE_I("Begin");

    while (!SafeFindCloseTerminate)
    {
        WaitForSingleObject(SafeFindCloseStart, INFINITE); // cekame na odstartovani nebo ukonceni

        while (1)
        {
            // vyzvedneme handle
            HANDLES(EnterCriticalSection(&SafeFindCloseCS));
            HANDLE h;
            BOOL br = FALSE;

            if (SafeFindCloseCNArr.IsGood() && SafeFindCloseCNArr.Count > 0)
            {
                h = SafeFindCloseCNArr[SafeFindCloseCNArr.Count - 1];
                SafeFindCloseCNArr.Delete(SafeFindCloseCNArr.Count - 1);
                if (!SafeFindCloseCNArr.IsGood())
                    SafeFindCloseCNArr.ResetState(); // nemuze se nepovest, hlasi jen nedostatek pameti pro zmenseni pole
            }
            else
                br = TRUE;
            HANDLES(LeaveCriticalSection(&SafeFindCloseCS));

            if (br)
                break; // uz neni co zavirat, pockame na dalsi start

            // zavreme handle
            //      TRACE_I("Killing ... " << h);
            HANDLES(FindCloseChangeNotification(h));
        }

        SetEvent(SafeFindCloseFinished); // pustime hl. thread dale ...
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
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
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
    SetEvent(WantDataEvent);                       // pozadame cmuchala o uvolneni DataUsageMutexu
    WaitForSingleObject(DataUsageMutex, INFINITE); // pockame na nej
    SetEvent(WantDataEvent);                       // cmuchal uz zase muze zacit cekat na DataUsageMutex
    BOOL registerDevNot = FALSE;
    HANDLE registerDevNotHandle = NULL;
    //---  ted uz jsou data hl. threadu, cmuchal ceka
    if (win->DeviceNotification != NULL)
    {
        UnregisterDeviceNotification(win->DeviceNotification);
        win->DeviceNotification = NULL;
    }

    int i;
    for (i = 0; i < WindowArray.Count; i++)
        if (win == WindowArray[i])
        {
            // pokud je change notifikace na odpojenem sitovem disku
            // nemuzem si dovolit cekat ... nechame to zavrit jiny thread
            HANDLES(EnterCriticalSection(&SafeFindCloseCS));
            SafeFindCloseCNArr.Add(ObjectArray[i]);
            if (!SafeFindCloseCNArr.IsGood())
                SafeFindCloseCNArr.ResetState(); // chyby ignorujeme
            HANDLES(LeaveCriticalSection(&SafeFindCloseCS));
            ResetEvent(SafeFindCloseFinished);               // budeme cekat na nahozeni...
            SetEvent(SafeFindCloseStart);                    // nastartujeme uklid
            WaitForSingleObject(SafeFindCloseFinished, 200); // 200 ms time-out pro zavreni handlu

            // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak FindFirstChangeNotification
            // mezery/tecky orizne a pracuje tak s jinou cestou
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
                ObjectArray.Delete(i); // vyhodime ho ze seznamu
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
    //---  nebylo nalezeno -> pridame
    if (i == WindowArray.Count)
    {
        // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak FindFirstChangeNotification
        // mezery/tecky orizne a pracuje tak s jinou cestou
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
        // zaregistrujeme okno panelu pro prijem zprav o zmenach media (odstraneni, atd.)
        DEV_BROADCAST_HANDLE dbh;
        memset(&dbh, 0, sizeof(dbh));
        dbh.dbch_size = sizeof(dbh);
        dbh.dbch_devicetype = DBT_DEVTYP_HANDLE;
        dbh.dbch_handle = registerDevNotHandle;
        win->DeviceNotification = RegisterDeviceNotificationA(win->HWindow, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
    }
    //---
    ReleaseMutex(DataUsageMutex);                 // uvolnime cmuchalovi DataUsageMutex
    WaitForSingleObject(ContinueEvent, INFINITE); // a pockame az si ho zabere
}

void DetachDirectory(CFilesWindow* win, BOOL waitForHandleClosure, BOOL closeDevNotifification)
{
    CALL_STACK_MESSAGE3("DetachDirectory(, %d, %d)", waitForHandleClosure, closeDevNotifification);
    SetEvent(WantDataEvent);                       // pozadame cmuchala o uvolneni DataUsageMutexu
    WaitForSingleObject(DataUsageMutex, INFINITE); // pockame na nej
    SetEvent(WantDataEvent);                       // cmuchal uz zase muze zacit cekat na DataUsageMutex
                                                   //---  ted uz jsou data hl. threadu, cmuchal ceka
    if (closeDevNotifification && win->DeviceNotification != NULL)
    {
        UnregisterDeviceNotification(win->DeviceNotification);
        win->DeviceNotification = NULL;
    }

    int i;
    for (i = 0; i < WindowArray.Count; i++)
        if (win == WindowArray[i])
        {
            // pokud je change notifikace na odpojenem sitovem disku
            // nemuzem si dovolit cekat ... nechame to zavrit jiny thread
            HANDLES(EnterCriticalSection(&SafeFindCloseCS));
            SafeFindCloseCNArr.Add(ObjectArray[i]);
            if (!SafeFindCloseCNArr.IsGood())
                SafeFindCloseCNArr.ResetState(); // chyby ignorujeme
            HANDLES(LeaveCriticalSection(&SafeFindCloseCS));
            ResetEvent(SafeFindCloseFinished);                                             // budeme cekat na nahozeni...
            SetEvent(SafeFindCloseStart);                                                  // nastartujeme uklid
            WaitForSingleObject(SafeFindCloseFinished, waitForHandleClosure ? 5000 : 200); // 200 ms time-out pro zavreni handlu

            ObjectArray.Delete(i); // vyhodime ho ze seznamu
            WindowArray.Delete(i);
            win->SetAutomaticRefresh(FALSE);
        }
    //---
    ReleaseMutex(DataUsageMutex);                 // uvolnime cmuchalovi DataUsageMutex
    WaitForSingleObject(ContinueEvent, INFINITE); // a pockame az si ho zabere
}

/*
#define SUSPMODESTACKSIZE 50

class CSuspModeStack
{
  protected:
    DWORD CallerCalledFromArr[SUSPMODESTACKSIZE];  // pole navratovych adres funkci, odkud se volal BeginSuspendMode()
    DWORD CalledFromArr[SUSPMODESTACKSIZE];        // pole adres, odkud se volal BeginSuspendMode()
    int Count;                                     // pocet prvku v predchozich dvou polich
    int Ignored;                                   // pocet volani BeginSuspendMode(), ktere jsme museli ignorovat (prilis male SUSPMODESTACKSIZE -> pripadne zvetsit)

  public:
    CSuspModeStack() {Count = 0; Ignored = 0;}
    ~CSuspModeStack() {CheckIfEmpty(1);}  // jeden BeginSuspendMode() je OK: vola se pri deaktivaci hl. okna Salama (pred uzavrenim hl. okna)

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
#ifdef _DEBUG     // testujeme, jestli se BeginSuspendMode() a EndSuspendMode() volaji ze stejne funkce (podle navratove adresy volajici funkce -> takze nepozna "chybu" pri volani z ruznych funkci, ktere se obe volaji ze stejne funkce)
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

pokud bude jeste nekdy potreba ozivit tenhle kod, vyuzit toho, ze lze nahradit (x86 i x64):
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
        SnooperSuspended = 0; // nepouziva zase nekdo blbe CM_LEFTREFRESH, CM_RIGHTREFRESH nebo CM_ACTIVEREFRESH
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
#ifdef _DEBUG     // testujeme, jestli se BeginSuspendMode() a EndSuspendMode() volaji ze stejne funkce (podle navratove adresy volajici funkce -> takze nepozna "chybu" pri volani z ruznych funkci, ktere se obe volaji ze stejne funkce)
void EndSuspendMode(BOOL debugDoNotTestCaller)
{
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

pokud bude jeste nekdy potreba ozivit tenhle kod, vyuzit toho, ze lze nahradit (x86 i x64):
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
