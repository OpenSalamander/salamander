// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "cfgdlg.h"
#include "menu.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "stswnd.h"
#include "snooper.h"
#include "shellib.h"
#include "drivelst.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"
#include "zip.h"

//****************************************************************************

// define the "Lock Volume" event GUID (e.g., "chkdsk /f E:" where E: is a USB stick): {50708874-C9AF-11D1-8FEF-00A0C9A06D32}
GUID GUID_IO_LockVolume = {0x50708874, 0xC9AF, 0x11D1, 0x8F, 0xEF, 0x00, 0xA0, 0xC9, 0xA0, 0x6D, 0x32};
//
// Ioevent.h from the DDK defines this constant (and many others):
//
//  Volume lock event.  This event is signalled when an attempt is made to
//  lock a volume.  There is no additional data.
//
// DEFINE_GUID( GUID_IO_VOLUME_LOCK, 0x50708874L, 0xc9af, 0x11d1, 0x8f, 0xef, 0x00, 0xa0, 0xc9, 0xa0, 0x6d, 0x32 );

BOOL IsCustomEventGUID(LPARAM lParam, REFGUID guidEvent)
{
    BOOL ret = FALSE;
    __try
    {
        DEV_BROADCAST_HDR* data = (DEV_BROADCAST_HDR*)lParam;
        if (data != NULL)
        {
            if (data->dbch_devicetype == DBT_DEVTYP_HANDLE)
            {
                DEV_BROADCAST_HANDLE* d = (DEV_BROADCAST_HANDLE*)data;
                if (IsEqualGUID(d->dbch_eventguid, guidEvent))
                    ret = TRUE;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return ret;
}

//****************************************************************************
//
// WindowProc
//

LRESULT
CFilesWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CFilesWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    BOOL setWait;
    BOOL probablyUselessRefresh;
    HCURSOR oldCur;
    switch (uMsg)
    {
        //---  expanding the listbox over the entire window
    case WM_SIZE:
    {
        if (ListBox != NULL && ListBox->HWindow != NULL && StatusLine != NULL && DirectoryLine != NULL)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            int dlHeight = 3;
            int stHeight = 0;
            int windowsCount = 1;
            if (DirectoryLine->HWindow != NULL)
            {
                dlHeight = DirectoryLine->GetNeededHeight();
                RECT r;
                GetClientRect(DirectoryLine->HWindow, &r);
                r.left += DirectoryLine->GetToolBarWidth();
                InvalidateRect(DirectoryLine->HWindow, &r, FALSE);
                windowsCount++;
            }
            if (StatusLine->HWindow != NULL)
            {
                stHeight = StatusLine->GetNeededHeight();
                InvalidateRect(StatusLine->HWindow, NULL, FALSE);
                windowsCount++;
            }

            HDWP hdwp = HANDLES(BeginDeferWindowPos(windowsCount));
            if (hdwp != NULL)
            {
                if (DirectoryLine->HWindow != NULL)
                    hdwp = HANDLES(DeferWindowPos(hdwp, DirectoryLine->HWindow, NULL,
                                                  0, 0, width, dlHeight,
                                                  SWP_NOACTIVATE | SWP_NOZORDER));

                hdwp = HANDLES(DeferWindowPos(hdwp, ListBox->HWindow, NULL,
                                              0, dlHeight, width, height - stHeight - dlHeight,
                                              SWP_NOACTIVATE | SWP_NOZORDER));

                if (StatusLine->HWindow != NULL)
                    hdwp = HANDLES(DeferWindowPos(hdwp, StatusLine->HWindow, NULL,
                                                  0, height - stHeight, width, stHeight,
                                                  SWP_NOACTIVATE | SWP_NOZORDER));

                HANDLES(EndDeferWindowPos(hdwp));
            }
            break;
        }
        break;
    }

    case WM_ERASEBKGND:
    {
        if (ListBox != NULL && ListBox->HWindow != NULL && DirectoryLine != NULL)
        {
            if (DirectoryLine->HWindow == NULL)
            {
                RECT r;
                GetClientRect(HWindow, &r);
                r.bottom = 3;
                FillRect((HDC)wParam, &r, HDialogBrush);
            }
        }
        return TRUE;
    }

    case WM_DEVICECHANGE:
    {
        switch (wParam)
        {
        case 0x8006 /* DBT_CUSTOMEVENT */:
        {
            //          TRACE_I("WM_DEVICECHANGE: DBT_CUSTOMEVENT");

            if (IsCustomEventGUID(lParam, GUID_IO_LockVolume))
            { // occurs on XP when "chkdsk /f e:" ("e:" is a removable USB stick) is run, and unfortunately also when opening .ifo and .vob files (DVD) and when starting Ashampoo Burning Studio 6 -- "lock volume" request
                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();                 // pause reading icons/thumbnails
                DetachDirectory((CFilesWindow*)this, TRUE); // close change notifications and DeviceNotification

                HANDLES(EnterCriticalSection(&TimeCounterSection));
                int t1 = MyTimeCounter++;
                HANDLES(LeaveCriticalSection(&TimeCounterSection));
                BOOL salIsActive = GetForegroundWindow() == MainWindow->HWindow;
                PostMessage(HWindow, WM_USER_REFRESH_DIR_EX, salIsActive, t1); // refresh resumes icon/thumbnail reading and reopens change notifications and DeviceNotification; we know this is probably an unnecessary refresh
            }
            break;
        }

        case DBT_DEVICEQUERYREMOVE:
        {
            //          TRACE_I("WM_DEVICECHANGE: DBT_DEVICEQUERYREMOVE");
            DetachDirectory((CFilesWindow*)this, TRUE, FALSE); // without closing DeviceNotification
            return TRUE;                                       // allow removal of this device
        }

        case DBT_DEVICEQUERYREMOVEFAILED:
        {
            //          TRACE_I("WM_DEVICECHANGE: DBT_DEVICEQUERYREMOVEFAILED");
            ChangeDirectory(this, GetPath(), MyGetDriveType(GetPath()) == DRIVE_REMOVABLE);
            return TRUE;
        }

        case DBT_DEVICEREMOVEPENDING:
        case DBT_DEVICEREMOVECOMPLETE:
        {
            //          if (wParam == DBT_DEVICEREMOVEPENDING) TRACE_I("WM_DEVICECHANGE: DBT_DEVICEREMOVEPENDING");
            //          else TRACE_I("WM_DEVICECHANGE: DBT_DEVICEREMOVECOMPLETE");
            DetachDirectory((CFilesWindow*)this, TRUE); // close DeviceNotification
            if (MainWindow->LeftPanel == this)
            {
                if (!ChangeLeftPanelToFixedWhenIdleInProgress)
                    ChangeLeftPanelToFixedWhenIdle = TRUE;
            }
            else
            {
                if (!ChangeRightPanelToFixedWhenIdleInProgress)
                    ChangeRightPanelToFixedWhenIdle = TRUE;
            }
            return TRUE;
        }

            //        default: TRACE_I("WM_DEVICECHANGE: other message: " << wParam); break;
        }
        break;
    }

    case WM_USER_DROPUNPACK:
    {
        // TRACE_I("WM_USER_DROPUNPACK received!");
        char* tgtPath = (char*)wParam;
        int operation = (int)lParam;
        if (operation == SALSHEXT_COPY) // unpack
        {
            ProgressDialogActivateDrop = LastWndFromGetData;
            UnpackZIPArchive(NULL, FALSE, tgtPath);
            ProgressDialogActivateDrop = NULL; // clear global variable for further use of progress dialog
            SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH, tgtPath, NULL);
        }
        free(tgtPath);
        return 0;
    }

    case WM_USER_DROPFROMFS:
    {
        TRACE_I("WM_USER_DROPFROMFS received: " << (lParam == SALSHEXT_COPY ? "Copy" : (lParam == SALSHEXT_MOVE ? "Move" : "Unknown")));
        char* tgtPath = (char*)wParam;
        int operation = (int)lParam;
        if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
            (operation == SALSHEXT_COPY && GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMFS) ||
             operation == SALSHEXT_MOVE && GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMFS)) &&
            Dirs->Count + Files->Count > 0)
        {
            int count = GetSelCount();
            if (count > 0 || GetCaretIndex() != 0 ||
                Dirs->Count == 0 || strcmp(Dirs->At(0).Name, "..") != 0) // check whether we are working only with ".."
            {
                BeginSuspendMode(); // snooper takes a break
                BeginStopRefresh(); // just to avoid distributing notifications about path changes

                UserWorkedOnThisPath = TRUE;
                StoreSelection(); // save the selection for the Restore Selection command

                ProgressDialogActivateDrop = LastWndFromGetData;

                int selectedDirs = 0;
                if (count > 0)
                {
                    // count how many directories are selected (the rest of the marked items are files)
                    int i;
                    for (i = 0; i < Dirs->Count; i++) // ".." cannot be selected, so the test would be unnecessary
                    {
                        if (Dirs->At(i).Selected)
                            selectedDirs++;
                    }
                }
                else
                    count = 0;

                int panel = MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT;
                BOOL copy = (operation == SALSHEXT_COPY);
                BOOL operationMask = FALSE;
                BOOL cancelOrHandlePath = FALSE;
                char targetPath[2 * MAX_PATH];
                lstrcpyn(targetPath, tgtPath, 2 * MAX_PATH - 1);
                if (tgtPath[0] == '\\' && tgtPath[1] == '\\' || // UNC path
                    tgtPath[0] != 0 && tgtPath[1] == ':')       // regular disk path (C:\path)
                {
                    int l = (int)strlen(targetPath);
                    if (l > 3 && targetPath[l - 1] == '\\')
                        targetPath[l - 1] = 0; // except for "c:\" remove the trailing backslash
                }
                targetPath[strlen(targetPath) + 1] = 0; // ensure two nulls at the end of the string

                // lower thread priority to "normal" so the operation does not put too much load on the system
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                BOOL ret = GetPluginFS()->CopyOrMoveFromFS(copy, 5, GetPluginFS()->GetPluginFSName(),
                                                           HWindow, panel,
                                                           count - selectedDirs, selectedDirs,
                                                           targetPath, operationMask,
                                                           cancelOrHandlePath,
                                                           ProgressDialogActivateDrop);

                // increase thread priority again; the operation has finished
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                if (ret && !cancelOrHandlePath)
                {
                    if (targetPath[0] != 0) // change focus to 'targetPath'
                    {
                        lstrcpyn(NextFocusName, targetPath, MAX_PATH);
                        // RefreshDirectory may not run; the source may be unchanged, so post a message just in case
                        PostMessage(HWindow, WM_USER_DONEXTFOCUS, 0, 0);
                    }

                    // successful operation, but do not clear the source selection because this is drag and drop
                    //            SetSel(FALSE, -1, TRUE);   // explicit redraw
                    //            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);  // selection-change notification
                    UpdateWindow(MainWindow->HWindow);
                }

                ProgressDialogActivateDrop = NULL;              // clear the global variable for further use of the progress dialog
                if (tgtPath[0] == '\\' && tgtPath[1] == '\\' || // UNC path
                    tgtPath[0] != 0 && tgtPath[1] == ':')       // regular disk path (C:\path)
                {
                    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH, tgtPath, NULL);
                }

                EndStopRefresh();
                EndSuspendMode(); // the snooper will start again now
            }
        }
        free(tgtPath);
        return 0;
    }

    case WM_USER_UPDATEPANEL:
    {
        // someone dispatched messages (a message box opened) and the panel
        // content must be updated
        RefreshListBox(0, -1, -1, FALSE, FALSE);
        return 0;
    }

    case WM_USER_ENTERMENULOOP:
    case WM_USER_LEAVEMENULOOP:
    {
        // just pass it to the main window
        return SendMessage(MainWindow->HWindow, uMsg, wParam, lParam);
    }

    case WM_USER_CONTEXTMENU:
    {
        CMenuPopup* popup = (CMenuPopup*)(CGUIMenuPopupAbstract*)wParam;
        // if the Alt+F1(2) menu is open above this panel and the RClick belongs to it,
        // pass the notification to it
        if (OpenedDrivesList != NULL && OpenedDrivesList->GetMenuPopup() == popup)
        {
            return OpenedDrivesList->OnContextMenu((BOOL)lParam, -1, PANEL_SOURCE, NULL);
        }
        return FALSE; //p.s. do not run the command, do not open the submenu
    }

    case WM_TIMER:
    {
        if (wParam == IDT_SM_END_NOTIFY)
        {
            KillTimer(HWindow, IDT_SM_END_NOTIFY);
            if (SmEndNotifyTimerSet) // not just a stray WM_TIMER
                PostMessage(HWindow, WM_USER_SM_END_NOTIFY_DELAYED, 0, 0);
            SmEndNotifyTimerSet = FALSE;
            return 0;
        }
        else
        {
            if (wParam == IDT_REFRESH_DIR_EX)
            {
                KillTimer(HWindow, IDT_REFRESH_DIR_EX);
                if (RefreshDirExTimerSet) // not just a stray WM_TIMER
                    PostMessage(HWindow, WM_USER_REFRESH_DIR_EX_DELAYED, FALSE, RefreshDirExLParam);
                RefreshDirExTimerSet = FALSE;
                return 0;
            }
            else
            {
                if (wParam == IDT_ICONOVRREFRESH)
                {
                    KillTimer(HWindow, IDT_ICONOVRREFRESH);
                    if (IconOvrRefreshTimerSet && // not just a stray WM_TIMER
                        Configuration.EnableCustomIconOverlays && Is(ptDisk) &&
                        (UseSystemIcons || UseThumbnails) && IconCache != NULL)
                    {
                        //              TRACE_I("Timer IDT_ICONOVRREFRESH: refreshing icon overlays");
                        LastIconOvrRefreshTime = GetTickCount();
                        SleepIconCacheThread();
                        WakeupIconCacheThread();
                    }
                    IconOvrRefreshTimerSet = FALSE;
                    return 0;
                }
                else
                {
                    if (wParam == IDT_INACTIVEREFRESH)
                    {
                        KillTimer(HWindow, IDT_INACTIVEREFRESH);
                        if (InactiveRefreshTimerSet) // not just a stray WM_TIMER
                        {
                            //                TRACE_I("Timer IDT_INACTIVEREFRESH: posting refresh!");
                            PostMessage(HWindow, WM_USER_INACTREFRESH_DIR, FALSE, InactRefreshLParam);
                        }
                        InactiveRefreshTimerSet = FALSE;
                        return 0;
                    }
                }
            }
        }
        break;
    }

    case WM_USER_REFRESH_DIR_EX:
    {
        if (!RefreshDirExTimerSet)
        {
            if (SetTimer(HWindow, IDT_REFRESH_DIR_EX, wParam ? 5000 : 200, NULL))
            {
                RefreshDirExTimerSet = TRUE;
                RefreshDirExLParam = lParam;
            }
            else
                PostMessage(HWindow, WM_USER_REFRESH_DIR_EX_DELAYED, FALSE, lParam);
        }
        else // waiting for WM_USER_REFRESH_DIR_EX_DELAYED to be posted
        {
            if (RefreshDirExLParam < lParam) // take the "newer" time
                RefreshDirExLParam = lParam;

            KillTimer(HWindow, IDT_REFRESH_DIR_EX); // restart timer so slow remains slow (5000ms) and fast remains fast (200ms) - the previous refresh type must not matter
            if (!SetTimer(HWindow, IDT_REFRESH_DIR_EX, wParam ? 5000 : 200, NULL))
            {
                RefreshDirExTimerSet = FALSE;
                PostMessage(HWindow, WM_USER_REFRESH_DIR_EX_DELAYED, FALSE, lParam);
            }
        }
        return 0;
    }

    case WM_USER_SM_END_NOTIFY:
    {
        if (!SmEndNotifyTimerSet)
        {
            if (SetTimer(HWindow, IDT_SM_END_NOTIFY, 200, NULL))
            {
                SmEndNotifyTimerSet = TRUE;
                return 0;
            }
            else
                uMsg = WM_USER_SM_END_NOTIFY_DELAYED;
        }
        else
            return 0;
        // the break is not missing -- if the timer cannot be started, WM_USER_SM_END_NOTIFY_DELAYED runs immediately
    }
        //--- suspend mode ended; check if a refresh is needed
    case WM_USER_SM_END_NOTIFY_DELAYED:
    {
        if (SnooperSuspended || StopRefresh)
            return 0;                        // wait for the next WM_USER_SM_END_NOTIFY_DELAYED
        if (PluginFSNeedRefreshAfterEndOfSM) // should the plugin FS be refreshed?
        {
            PluginFSNeedRefreshAfterEndOfSM = FALSE;
            PostMessage(HWindow, WM_USER_REFRESH_PLUGINFS, 0, 0); // attempt it now
        }

        if (NeedRefreshAfterEndOfSM) // should a refresh occur?
        {
            NeedRefreshAfterEndOfSM = FALSE;
            lParam = RefreshAfterEndOfSMTime;
            wParam = FALSE; // do not trigger RefreshFinishedEvent
        }
        else
            return 0;
    }
        //--- a directory content change was recorded during suspend mode
    case WM_USER_S_REFRESH_DIR:
    {
        if (uMsg == WM_USER_S_REFRESH_DIR && // content change recorded during suspend mode
            !IconCacheValid && UseSystemIcons && Is(ptDisk) && GetNetworkDrive())
        {
            // TRACE_I("Delaying refresh from suspend mode until all icons are read.");
            NeedRefreshAfterIconsReading = TRUE;
            RefreshAfterIconsReadingTime = max(RefreshAfterIconsReadingTime, (int)lParam);
            if (wParam)
                SetEvent(RefreshFinishedEvent); // probably unnecessary but mentioned in WM_USER_S_REFRESH_DIR docs
            return 0;                           // we noted the change (refresh will be posted once icon reading ends); stop processing
        }

        setWait = FALSE;
        if (lParam >= LastRefreshTime)
        {                                                          // not an unnecessary old refresh
            setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // already waiting?
            if (setWait)
                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
            DWORD err = CheckPath(FALSE, NULL, ERROR_SUCCESS, !SnooperSuspended && !StopRefresh);
            if (err == ERROR_SUCCESS)
            {
                if (GetMonitorChanges()) // the snooper might have removed it from the list
                    ChangeDirectory(this, GetPath(), MyGetDriveType(GetPath()) == DRIVE_REMOVABLE);
            }
            else
            {
                if (err == ERROR_USER_TERMINATED)
                {
                    DetachDirectory(this);
                    ChangeToRescuePathOrFixedDrive(HWindow);
                }
            }
        }
    }
        //--- icon reading has finished; check whether a refresh is needed
    case WM_USER_ICONREADING_END:
    {
        //      TRACE_I("WM_USER_ICONREADING_END");
        probablyUselessRefresh = FALSE;
        if (uMsg == WM_USER_ICONREADING_END)
        {
            IconCacheValid = TRUE;
            EndOfIconReadingTime = GetTickCount();
            if (NeedRefreshAfterIconsReading) // should we refresh?
            {
                //          TRACE_I("Doing delayed refresh (all icons are read).");
                NeedRefreshAfterIconsReading = FALSE;
                lParam = RefreshAfterIconsReadingTime;
                wParam = FALSE; // do not trigger RefreshFinishedEvent
                setWait = FALSE;
                probablyUselessRefresh = TRUE; // probably just a refresh incorrectly triggered by the system after loading icons from a network drive
                                               //          TRACE_I("delayed refresh (after reading of all icons): probablyUselessRefresh=TRUE");
            }
            else
            {
                if (NeedIconOvrRefreshAfterIconsReading) // refresh icon overlays
                {
                    NeedIconOvrRefreshAfterIconsReading = FALSE;

                    if (Configuration.EnableCustomIconOverlays && Is(ptDisk) &&
                        (UseSystemIcons || UseThumbnails) && IconCache != NULL)
                    {
                        //              TRACE_I("NeedIconOvrRefreshAfterIconsReading: refreshing icon overlays");
                        LastIconOvrRefreshTime = GetTickCount();
                        SleepIconCacheThread();
                        WakeupIconCacheThread();
                    }
                }
                return 0;
            }
        }
    }
        //--- a directory content change was recorded
    case WM_USER_REFRESH_DIR:
    case WM_USER_REFRESH_DIR_EX_DELAYED:
    case WM_USER_INACTREFRESH_DIR:
    {
        //      if (uMsg == WM_USER_INACTREFRESH_DIR) TRACE_I("WM_USER_INACTREFRESH_DIR");
        if (uMsg != WM_USER_ICONREADING_END)
        {
            if (GetTickCount() - EndOfIconReadingTime < 1000)
            {
                probablyUselessRefresh = TRUE; // for one second after icon reading finishes, we still expect a redundant refresh caused by icon reading
                                               //          TRACE_I("less than second after reading of icons was finished: probablyUselessRefresh=TRUE");
            }
            else
            {
                probablyUselessRefresh = (uMsg == WM_USER_REFRESH_DIR_EX_DELAYED || uMsg == WM_USER_INACTREFRESH_DIR); // deferred refresh that may also be unnecessary (this prevents an infinite loop when reading icons on network drives triggers additional refreshes)
                                                                                                                       //          TRACE_I("WM_USER_REFRESH_DIR_EX_DELAYED or WM_USER_INACTREFRESH_DIR: probablyUselessRefresh=" << probablyUselessRefresh);
            }
        }
        if ((uMsg == WM_USER_REFRESH_DIR && wParam || // content change reported by the snooper
             uMsg == WM_USER_ICONREADING_END ||       // or a notification that icon reading finished (it may arrive later, and icon reading may already have restarted)
             uMsg == WM_USER_INACTREFRESH_DIR) &&     // or deferred refresh in an inactive window (refresh requested by the snooper or when ending suspend mode)
            !IconCacheValid &&
            UseSystemIcons && Is(ptDisk) && GetNetworkDrive())
        {
            //        TRACE_I("Delaying refresh until all icons are read.");
            NeedRefreshAfterIconsReading = TRUE;
            RefreshAfterIconsReadingTime = max(RefreshAfterIconsReadingTime, (int)lParam);
            // the change notification was recorded (refresh will be posted once icon reading finishes), stopping processing
        }
        else
        {
            if (SnooperSuspended || StopRefresh)
            { // suspend mode is already on (working with internal data -> cannot refresh)
                NeedRefreshAfterEndOfSM = TRUE;
                RefreshAfterEndOfSMTime = max(RefreshAfterEndOfSMTime, (int)lParam);
                if ((uMsg == WM_USER_S_REFRESH_DIR || uMsg == WM_USER_SM_END_NOTIFY_DELAYED) && setWait)
                {
                    SetCursor(oldCur);
                }
            }
            else // not a refresh during suspend mode
            {
                if (lParam >= LastRefreshTime) // not a stale refresh
                {
                    BOOL isInactiveRefresh = FALSE;
                    BOOL skipRefresh = FALSE;
                    if ((uMsg == WM_USER_REFRESH_DIR && wParam ||     // content change reported by the snooper
                         uMsg == WM_USER_ICONREADING_END ||           // or notification that icon reading finished (deferred refresh requested by the snooper + after ending suspend mode)
                         uMsg == WM_USER_INACTREFRESH_DIR) &&         // or deferred refresh in an inactive window (refresh requested by the snooper or when ending suspend mode)
                        GetForegroundWindow() != MainWindow->HWindow) // inactive Salamander main window: slow down refreshes if needed
                    {
                        //              TRACE_I("Refresh from snooper in inactive window");
                        isInactiveRefresh = TRUE;
                        if (LastInactiveRefreshStart != LastInactiveRefreshEnd) // a refresh has already occurred since the last deactivation
                        {
                            DWORD delay = 20 * (LastInactiveRefreshEnd - LastInactiveRefreshStart);
                            //                TRACE_I("Calculated delay between refreshes is " << delay);
                            if (delay < MIN_DELAY_BETWEENINACTIVEREFRESHES)
                                delay = MIN_DELAY_BETWEENINACTIVEREFRESHES;
                            if (delay > MAX_DELAY_BETWEENINACTIVEREFRESHES)
                                delay = MAX_DELAY_BETWEENINACTIVEREFRESHES;
                            //                TRACE_I("Delay between refreshes is " << delay);
                            DWORD ti = GetTickCount();
                            //                TRACE_I("Last refresh was before " << ti - LastInactiveRefreshEnd);
                            if (InactiveRefreshTimerSet ||                 // timer already running, just wait for it
                                ti - LastInactiveRefreshEnd + 100 < delay) // +100 so the timer isn't needlessly set (ensures at least a 100ms refresh delay)
                            {
                                //                  TRACE_I("Delaying refresh");
                                if (!InactiveRefreshTimerSet) // timer not running yet, create it
                                {
                                    //                    TRACE_I("Setting timer");
                                    if (SetTimer(HWindow, IDT_INACTIVEREFRESH, max(200, delay - (ti - LastInactiveRefreshEnd)), NULL))
                                    {
                                        InactiveRefreshTimerSet = TRUE;
                                        InactRefreshLParam = lParam;
                                        skipRefresh = TRUE;
                                    }
                                }
                                else // timer is already running; just wait for it
                                {
                                    //                    TRACE_I("Timer already set");
                                    if (lParam > InactRefreshLParam)
                                        InactRefreshLParam = lParam; // use the newer time for InactRefreshLParam
                                    skipRefresh = TRUE;
                                }
                            }
                        }
                    }
                    if (!skipRefresh)
                    {
                        if (uMsg == WM_USER_REFRESH_DIR || uMsg == WM_USER_REFRESH_DIR_EX_DELAYED ||
                            uMsg == WM_USER_ICONREADING_END || uMsg == WM_USER_INACTREFRESH_DIR)
                        {
                            setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // already waiting?
                            if (setWait)
                                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                        }
                        char pathBackup[MAX_PATH];
                        CPanelType typeBackup;
                        if (isInactiveRefresh)
                        {
                            lstrcpyn(pathBackup, GetPath(), MAX_PATH); // we care only about disk paths and archive paths (the snooper does not report changes for plugin FS)
                            typeBackup = GetPanelType();
                            LastInactiveRefreshStart = GetTickCount();
                        }

                        HANDLES(EnterCriticalSection(&TimeCounterSection));
                        LastRefreshTime = MyTimeCounter++;
                        HANDLES(LeaveCriticalSection(&TimeCounterSection));

                        RefreshDirectory(probablyUselessRefresh, FALSE, isInactiveRefresh);

                        if (isInactiveRefresh)
                        {
                            if (typeBackup != GetPanelType() || StrICmp(pathBackup, GetPath()) != 0)
                            { // if the path changed (most likely because someone just deleted the directory shown in the panel), perform any further refresh immediately (the newly displayed directory may be deleted as well, so we can back out of it quickly)
                                LastInactiveRefreshEnd = LastInactiveRefreshStart;
                            }
                            else
                            {
                                LastInactiveRefreshEnd = GetTickCount();
                                if ((int)(LastInactiveRefreshEnd - LastInactiveRefreshStart) <= 0)
                                    LastInactiveRefreshEnd = LastInactiveRefreshStart + 1; // must not be equal (this means "no refresh yet")
                            }
                        }
                        /*  // Petr: It is unclear why LastRefreshTime was set only here. If a change occurs during a refresh, another refresh is required. This caused problems in Nethood because the enumeration thread posted a refresh before RefreshDirectory finished, so it was ignored as a refresh during a refresh.
              HANDLES(EnterCriticalSection(&TimeCounterSection));
              LastRefreshTime = MyTimeCounter++;
              HANDLES(LeaveCriticalSection(&TimeCounterSection));
*/
                        if (setWait)
                            SetCursor(oldCur);
                    }
                }
                //          else TRACE_I("Skipping useless refresh (it's time is older than time of last refresh)");
            }
        }
        if (wParam)
            SetEvent(RefreshFinishedEvent);
        return 0;
    }

    case WM_USER_REFRESH_PLUGINFS:
    {
        if (SnooperSuspended || StopRefresh)
        { // suspend mode is already active (working with internal data -> cannot refresh)
            // moreover, we might also be inside a plugin -> multiple calls to plugin methods are not supported
            PluginFSNeedRefreshAfterEndOfSM = TRUE;
        }
        else
        { // we are not inside a plugin
            if (Is(ptPluginFS))
            {
                if (GetPluginFS()->NotEmpty())
                    GetPluginFS()->Event(FSE_ACTIVATEREFRESH, GetPanelCode());
            }
        }
        return 0;
    }

    case WM_USER_REFRESHINDEX:
    case WM_USER_REFRESHINDEX2:
    {
        BOOL isDir = (int)wParam < Dirs->Count;
        CFileData* file = isDir ? &Dirs->At((int)wParam) : (((int)wParam < Dirs->Count + Files->Count) ? &Files->At((int)wParam - Dirs->Count) : NULL);

        if (uMsg == WM_USER_REFRESHINDEX)
        {
            // if a "static" association icon was loaded, store it in Associations
            // also covers thumbnails - the Flag==1 or 2 condition does not apply
            if (file != NULL && !isDir &&                                   // this is a file
                (!Is(ptPluginFS) || GetPluginIconsType() != pitFromPlugin)) // not an icon from a plugin
            {
                char buf[MAX_PATH + 4]; // extension in lowercase
                char *s1 = buf, *s2 = file->Ext;
                while (*s2 != 0)
                    *s1++ = LowerCase[*s2++];
                *((DWORD*)s1) = 0;
                int index;
                CIconSizeEnum iconSize = IconCache->GetIconSize();
                if (Associations.GetIndex(buf, index) &&             // the extension has an icon (association)
                    (Associations[index].GetIndex(iconSize) == -1 || // this icon is being loaded
                     Associations[index].GetIndex(iconSize) == -3))
                {
                    int icon;
                    CIconList* srcIconList;
                    int srcIconListIndex;
                    memmove(buf, file->Name, file->NameLen);
                    *(DWORD*)(buf + file->NameLen) = 0;
                    if (IconCache->GetIndex(buf, icon, NULL, NULL) &&                                 // the icon thread is loading it
                        (IconCache->At(icon).GetFlag() == 1 || IconCache->At(icon).GetFlag() == 2) && // icon is loaded new or old
                        IconCache->GetIcon(IconCache->At(icon).GetIndex(),
                                           &srcIconList, &srcIconListIndex)) // able to obtain the loaded icon
                    {                                                        // icon for the extension -> icon thread has already loaded it
                        CIconList* dstIconList;
                        int dstIconListIndex;
                        int i = Associations.AllocIcon(&dstIconList, &dstIconListIndex, iconSize);
                        if (i != -1) // we obtained space for a new icon
                        {            // copy it from IconCache to Associations
                            Associations[index].SetIndex(i, iconSize);

                            BOOL leaveSection;
                            if (!IconCacheValid)
                            {
                                HANDLES(EnterCriticalSection(&ICSectionUsingIcon));
                                leaveSection = TRUE;
                            }
                            else
                                leaveSection = FALSE;

                            dstIconList->Copy(dstIconListIndex, srcIconList, srcIconListIndex);

                            if (leaveSection)
                            {
                                HANDLES(LeaveCriticalSection(&ICSectionUsingIcon));
                            }

                            if (!StopIconRepaint)
                            {
                                // repaint panels only if the icon sizes match
                                if (iconSize == GetIconSizeForCurrentViewMode())
                                    RepaintIconOnly(-1); // all of ours

                                CFilesWindow* otherPanel = MainWindow->GetOtherPanel(this);
                                if (iconSize == otherPanel->GetIconSizeForCurrentViewMode())
                                    otherPanel->RepaintIconOnly(-1); // and all in the other panel
                            }
                            else
                                PostAllIconsRepaint = TRUE;
                        }
                    }
                }
            }
        }

        // redraw the affected index
        if (file != NULL) // file is used here only to test for NULL
        {
            if (!StopIconRepaint) // if icon repainting is allowed
                RepaintIconOnly((int)wParam);
            else
                PostAllIconsRepaint = TRUE;
        }
        return 0;
    }

    case WM_USER_DROPCOPYMOVE:
    {
        CTmpDropData* data = (CTmpDropData*)wParam;
        if (data != NULL)
        {
            FocusFirstNewItem = TRUE;
            DropCopyMove(data->Copy, data->TargetPath, data->Data);
            DestroyCopyMoveData(data->Data);
            delete data;
        }
        return 0;
    }

    case WM_USER_DROPTOARCORFS:
    {
        CTmpDragDropOperData* data = (CTmpDragDropOperData*)wParam;
        if (data != NULL)
        {
            FocusFirstNewItem = TRUE;
            DragDropToArcOrFS(data);
            delete data->Data;
            delete data;
        }
        return 0;
    }

    case WM_USER_CHANGEDIR:
    {
        // postprocess only paths obtained as text (not directly from a dropped directory)
        char buff[2 * MAX_PATH];
        strcpy_s(buff, (char*)lParam);
        if (!(BOOL)wParam || PostProcessPathFromUser(HWindow, buff))
            ChangeDir(buff, -1, NULL, 3 /*change-dir*/, NULL, (BOOL)wParam);
        return 0;
    }

    case WM_USER_FOCUSFILE:
    {
        // We must bring the window to the front here because calling ChangeDir can
        // pop up a message box (the path does not exist) which would otherwise remain under Find.
        SetForegroundWindow(MainWindow->HWindow);
        if (IsIconic(MainWindow->HWindow))
        {
            ShowWindow(MainWindow->HWindow, SW_RESTORE);
        }
        if (Is(ptDisk) && IsTheSamePath(GetPath(), (char*)lParam) ||
            ChangeDir((char*)lParam))
        {
            strcpy(NextFocusName, (char*)wParam);
            SendMessage(HWindow, WM_USER_DONEXTFOCUS, 0, 0);
            //        SetForegroundWindow(MainWindow->HWindow);  // it's too late here - moved above
            UpdateWindow(MainWindow->HWindow);
        }
        return 0;
    }

    case WM_USER_VIEWFILE:
    {
        COpenViewerData* data = (COpenViewerData*)wParam;
        ViewFile(data->FileName, (BOOL)lParam, 0xFFFFFFFF, data->EnumFileNamesSourceUID,
                 data->EnumFileNamesLastFileIndex);
        return 0;
    }

    case WM_USER_EDITFILE:
    {
        EditFile((char*)wParam);
        return 0;
    }

    case WM_USER_VIEWFILEWITH:
    {
        COpenViewerData* data = (COpenViewerData*)wParam;
        ViewFile(data->FileName, FALSE, (DWORD)lParam, data->EnumFileNamesSourceUID, // FIXME_X64 - verify cast to (DWORD)
                 data->EnumFileNamesLastFileIndex);
        return 0;
    }

    case WM_USER_EDITFILEWITH:
    {
        EditFile((char*)wParam, (DWORD)lParam); // FIXME_X64 - verify cast to (DWORD)
        return 0;
    }

        //    case WM_USER_RENAME_NEXT_ITEM:
        //    {
        //      int index = GetCaretIndex();
        //      QuickRenameOnIndex(index + (wParam ? 1 : -1));
        //      return 0;
        //    }

    case WM_USER_DONEXTFOCUS: // if RefreshDirectory did not manage it already, do it here
    {
        DontClearNextFocusName = FALSE;
        if (NextFocusName[0] != 0) // if there is something to focus
        {
            int total = Files->Count + Dirs->Count;
            int found = -1;
            int i;
            for (i = 0; i < total; i++)
            {
                CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                if (StrICmp(f->Name, NextFocusName) == 0)
                {
                    if (strcmp(f->Name, NextFocusName) == 0) // file found with exact case match
                    {
                        NextFocusName[0] = 0;
                        SetCaretIndex(i, FALSE);
                        break;
                    }
                    if (found == -1)
                        found = i; // file found (case-insensitive)
                }
            }
            if (i == total && found != -1)
            {
                NextFocusName[0] = 0;
                SetCaretIndex(found, FALSE);
            }
        }
        return 0;
    }

    case WM_USER_SELCHANGED:
    {
        int count = GetSelCount();
        if (count != 0)
        {
            CQuadWord selectedSize(0, 0);
            BOOL displaySize = (ValidFileData & (VALID_DATA_SIZE | VALID_DATA_PL_SIZE)) != 0;
            int totalCount = Dirs->Count + Files->Count;
            int files = 0;
            int dirs = 0;

            CQuadWord plSize;
            BOOL plSizeValid = FALSE;
            BOOL testPlSize = (ValidFileData & VALID_DATA_PL_SIZE) && PluginData.NotEmpty();
            BOOL sizeValid = (ValidFileData & VALID_DATA_SIZE) != 0;
            int i;
            for (i = 0; i < totalCount; i++)
            {
                BOOL isDir = i < Dirs->Count;
                CFileData* f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                if (i == 0 && isDir && strcmp(Dirs->At(0).Name, "..") == 0)
                    continue;
                if (f->Selected == 1)
                {
                    if (isDir)
                        dirs++;
                    else
                        files++;
                    plSizeValid = testPlSize && PluginData.GetByteSize(f, isDir, &plSize);
                    if (plSizeValid || sizeValid && (!isDir || f->SizeValid))
                        selectedSize += plSizeValid ? plSize : f->Size;
                    else
                        displaySize = FALSE; // file of unknown size or directory without known/calculated size
                }
            }
            if (files > 0 || dirs > 0)
            {
                char buff[1000];
                DWORD varPlacements[100];
                int varPlacementsCount = 100;
                BOOL done = FALSE;
                if (Is(ptZIPArchive) || Is(ptPluginFS))
                {
                    if (PluginData.NotEmpty())
                    {
                        if (PluginData.GetInfoLineContent(MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT,
                                                          NULL, FALSE, files, dirs,
                                                          displaySize, selectedSize, buff,
                                                          varPlacements, varPlacementsCount))
                        {
                            done = TRUE;
                            if (StatusLine->SetText(buff))
                                StatusLine->SetSubTexts(varPlacements, varPlacementsCount);
                        }
                        else
                            varPlacementsCount = 100; // might have been corrupted
                    }
                }
                if (!done)
                {
                    char text[200];
                    if (displaySize)
                    {
                        ExpandPluralBytesFilesDirs(text, 200, selectedSize, files, dirs, TRUE);
                        LookForSubTexts(text, varPlacements, &varPlacementsCount);
                    }
                    else
                        ExpandPluralFilesDirs(text, 200, files, dirs, epfdmSelected, FALSE);
                    if (StatusLine->SetText(text) && displaySize)
                        StatusLine->SetSubTexts(varPlacements, varPlacementsCount);
                    varPlacementsCount = 100; // might have been corrupted
                }
            }
            else
                TRACE_E("Unexpected situation in CFilesWindow::WindowProc(WM_USER_SELCHANGED)");
        }

        if (count == 0)
        {
            LastFocus = INT_MAX;
            int index = GetCaretIndex();
            ItemFocused(index); // when deselecting
        }
        IdleRefreshStates = TRUE; // at the next Idle enforce checking of state variables
        return 0;
    }

    case WM_CREATE:
    {
        //---  add this panel to the array of sources for file enumeration in viewers
        EnumFileNamesAddSourceUID(HWindow, &EnumFileNamesSourceUID);

        //---  create listbox with files and directories
        ListBox = new CFilesBox(this);
        if (ListBox == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        //---  create status line with information about the current file
        StatusLine = new CStatusWindow(this, blBottom, ooStatic);
        if (StatusLine == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        ToggleStatusLine();
        //---  create status line with information about the current directory
        DirectoryLine = new CStatusWindow(this, blTop, ooStatic);
        if (DirectoryLine == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        DirectoryLine->SetLeftPanel(MainWindow->LeftPanel == this);
        ToggleDirectoryLine();
        //---  apply view type and load directory contents
        SetThumbnailSize(Configuration.ThumbnailSize); // ListBox must exist
        if (!ListBox->CreateEx(WS_EX_WINDOWEDGE,
                               CFILESBOX_CLASSNAME,
                               "",
                               WS_BORDER | WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                               0, 0, 0, 0, // dummy
                               HWindow,
                               (HMENU)IDC_FILES,
                               HInstance,
                               ListBox))
        {
            TRACE_E("Unable to create listbox.");
            return -1;
        }
        RegisterDragDrop();

        int index;
        switch (GetViewMode())
        {
            //        case vmThumbnails: index = 0; break;
            //        case vmBrief: index = 1; break;
        case vmDetailed:
            index = 2;
            break;
        default:
        {
            TRACE_E("Unsupported ViewMode=" << GetViewMode());
            index = 2;
        }
        }
        SelectViewTemplate(index, FALSE, FALSE);
        ShowWindow(ListBox->HWindow, SW_SHOW);

        // align AutomaticRefresh variable with the directory line
        SetAutomaticRefresh(AutomaticRefresh, TRUE);

        return 0;
    }

    case WM_DESTROY:
    {
        //---  remove this panel from the array of sources for file enumeration in viewers
        EnumFileNamesRemoveSourceUID(HWindow);

        CancelUI(); // cancel QuickSearch and QuickEdit
        LastRefreshTime = INT_MAX;
        BeginStopRefresh();
        DetachDirectory(this);
        //---  release child windows
        RevokeDragDrop();
        ListBox->DetachWindow();
        delete ListBox;
        ListBox = NULL; // just to be sure so errors show up...

        StatusLine->DestroyWindow();
        delete StatusLine;
        StatusLine = NULL;

        DirectoryLine->DestroyWindow();
        delete DirectoryLine;
        DirectoryLine = NULL; // fix for crash
                              //---
        return 0;
    }

    case WM_USER_ENUMFILENAMES: // searching for next/previous name for the viewer
    {
        HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));

        if (InactiveRefreshTimerSet) // if a refresh is pending here, execute it now; otherwise enumeration would use an outdated listing; a longer delay is fine, GetFileNameForViewer waits for the result...
        {
            //        TRACE_I("Refreshing during enumeration (refresh in inactive window was delayed)");
            KillTimer(HWindow, IDT_INACTIVEREFRESH);
            InactiveRefreshTimerSet = FALSE;
            LastInactiveRefreshEnd = LastInactiveRefreshStart;
            SendMessage(HWindow, WM_USER_INACTREFRESH_DIR, FALSE, InactRefreshLParam);
        }

        if ((int)wParam /* reqUID */ == FileNamesEnumData.RequestUID && // no new request was made (otherwise this one would be useless)
            EnumFileNamesSourceUID == FileNamesEnumData.SrcUID &&       // the source hasn't changed
            !FileNamesEnumData.TimedOut)                                // someone is still waiting for the result
        {
            if (Files != NULL && Is(ptDisk))
            {
                BOOL selExists = FALSE;
                if (FileNamesEnumData.PreferSelected) // if needed, check whether anything is selected
                {
                    int i;
                    for (i = 0; i < Files->Count; i++)
                    {
                        if (Files->At(i).Selected)
                        {
                            selExists = TRUE;
                            break;
                        }
                    }
                }

                int index = FileNamesEnumData.LastFileIndex;
                int count = Files->Count;
                BOOL indexNotFound = TRUE;
                if (index == -1) // searching from the first or last item
                {
                    if (FileNamesEnumData.RequestType == fnertFindPrevious)
                        index = count; // searching for the previous item + start at the last item
                                       // else  // searching for the next item + start at the first item
                }
                else
                {
                    if (FileNamesEnumData.LastFileName[0] != 0) // we know the full file name at 'index', so check whether the array shifted and, if needed, find the new index
                    {
                        int pathLen = (int)strlen(GetPath());
                        if (StrNICmp(GetPath(), FileNamesEnumData.LastFileName, pathLen) == 0)
                        { // file path must match the path in the panel ("always true")
                            const char* name = FileNamesEnumData.LastFileName + pathLen;
                            if (*name == '\\' || *name == '/')
                                name++;

                            CFileData* f = (index >= 0 && index < count) ? &Files->At(index) : NULL;
                            BOOL nameIsSame = f != NULL && StrICmp(name, f->Name) == 0;
                            if (nameIsSame)
                                indexNotFound = FALSE;
                            if (f == NULL || !nameIsSame)
                            { // the name at index 'index' is not FileNamesEnumData.LastFileName, try to find a new index for this name
                                int i;
                                for (i = 0; i < count && StrICmp(name, Files->At(i).Name) != 0; i++)
                                    ;
                                if (i != count) // new index found
                                {
                                    indexNotFound = FALSE;
                                    index = i;
                                }
                            }
                        }
                        else
                            TRACE_E("Unexpected situation in WM_USER_ENUMFILENAMES: paths are different!");
                    }
                    if (index >= count)
                    {
                        if (FileNamesEnumData.RequestType == fnertFindNext)
                            index = count - 1;
                        else
                            index = count;
                    }
                    if (index < 0)
                        index = 0;
                }

                int wantedViewerType = 0;
                BOOL onlyAssociatedExtensions = FALSE;
                if (FileNamesEnumData.OnlyAssociatedExtensions) // does the viewer request filtering by associated extensions?
                {
                    if (FileNamesEnumData.Plugin != NULL) // viewer from a plugin
                    {
                        int pluginIndex = Plugins.GetIndex(FileNamesEnumData.Plugin);
                        if (pluginIndex != -1) // "always true"
                        {
                            wantedViewerType = -1 - pluginIndex;
                            onlyAssociatedExtensions = TRUE;
                        }
                    }
                    else // internal viewer
                    {
                        wantedViewerType = VIEWER_INTERNAL;
                        onlyAssociatedExtensions = TRUE;
                    }
                }

                BOOL preferSelected = selExists && FileNamesEnumData.PreferSelected;
                switch (FileNamesEnumData.RequestType)
                {
                case fnertFindNext: // next
                {
                    CDynString strViewerMasks;
                    if (!onlyAssociatedExtensions || MainWindow->GetViewersAssoc(wantedViewerType, &strViewerMasks))
                    {
                        CMaskGroup masks;
                        int errorPos;
                        if (!onlyAssociatedExtensions || masks.PrepareMasks(errorPos, strViewerMasks.GetString()))
                        {
                            while (index + 1 < count)
                            {
                                index++;
                                CFileData* f = &(Files->At(index));
                                if (f->Selected || !preferSelected)
                                {
                                    if (!onlyAssociatedExtensions || masks.AgreeMasks(f->Name, f->Ext))
                                    {
                                        FileNamesEnumData.Found = TRUE;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                            TRACE_E("Unexpected situation in WM_USER_ENUMFILENAMES: grouped viewer's masks can't be prepared for use!");
                    }
                    break;
                }

                case fnertFindPrevious: // previous
                {
                    CDynString strViewerMasks;
                    if (!onlyAssociatedExtensions || MainWindow->GetViewersAssoc(wantedViewerType, &strViewerMasks))
                    {
                        CMaskGroup masks;
                        int errorPos;
                        if (!onlyAssociatedExtensions || masks.PrepareMasks(errorPos, strViewerMasks.GetString()))
                        {
                            while (index - 1 >= 0)
                            {
                                index--;
                                CFileData* f = &(Files->At(index));
                                if (f->Selected || !preferSelected)
                                {
                                    if (!onlyAssociatedExtensions || masks.AgreeMasks(f->Name, f->Ext))
                                    {
                                        FileNamesEnumData.Found = TRUE;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                            TRACE_E("Unexpected situation in WM_USER_ENUMFILENAMES: grouped viewer's masks can't be prepared for use!");
                    }
                    break;
                }

                case fnertIsSelected: // query selection
                {
                    if (!indexNotFound && index >= 0 && index < Files->Count)
                    {
                        FileNamesEnumData.IsFileSelected = Files->At(index).Selected;
                        FileNamesEnumData.Found = TRUE;
                    }
                    break;
                }

                case fnertSetSelection: // set selection
                {
                    if (!indexNotFound && index >= 0 && index < Files->Count)
                    {
                        SetSel(FileNamesEnumData.Select, Dirs->Count + index, TRUE);
                        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                        FileNamesEnumData.Found = TRUE;
                    }
                    break;
                }
                }
                if (FileNamesEnumData.Found)
                {
                    lstrcpyn(FileNamesEnumData.FileName, GetPath(), MAX_PATH);
                    SalPathAppend(FileNamesEnumData.FileName, Files->At(index).Name, MAX_PATH);
                    FileNamesEnumData.LastFileIndex = index;
                }
                else
                    FileNamesEnumData.NoMoreFiles = TRUE;
            }
            else
                TRACE_E("Unexpected situation in handling of WM_USER_ENUMFILENAMES: srcUID was not changed before changing path from disk or invalidating of listing!");
            SetEvent(FileNamesEnumDone);
        }
        HANDLES(LeaveCriticalSection(&FileNamesEnumDataSect));
        return 0;
    }

    case WM_SETFOCUS:
    {
        SetFocus(ListBox->HWindow);
        break;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

void CFilesWindow::ClearCutToClipFlag(BOOL repaint)
{
    CALL_STACK_MESSAGE_NONE
    int total = Dirs->Count;
    int i;
    for (i = 0; i < total; i++)
    {
        CFileData* f = &Dirs->At(i);
        if (f->CutToClip != 0)
        {
            f->CutToClip = 0;
            f->Dirty = 1;
        }
    }
    total = Files->Count;
    for (i = 0; i < total; i++)
    {
        CFileData* f = &Files->At(i);
        if (f->CutToClip != 0)
        {
            f->CutToClip = 0;
            f->Dirty = 1;
        }
    }
    CutToClipChanged = FALSE;
    if (repaint)
        RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
}

void CFilesWindow::OpenDirHistory()
{
    CALL_STACK_MESSAGE1("CFilesWindow::OpenDirHistory()");
    if (!MainWindow->DirHistory->HasPaths())
        return;

    BeginStopRefresh(); // snooper takes a break

    CMenuPopup menu;

    RECT r;
    GetWindowRect(HWindow, &r);
    BOOL exludeRect = FALSE;
    int y = r.top;
    if (DirectoryLine != NULL && DirectoryLine->HWindow != NULL)
    {
        if (DirectoryLine->GetTextFrameRect(&r))
        {
            y = r.bottom;
            exludeRect = TRUE;
            menu.SetMinWidth(r.right - r.left);
        }
    }

    MainWindow->DirHistory->FillHistoryPopupMenu(&menu, 1, -1, FALSE);
    DWORD cmd = menu.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_VERTICAL, r.left, y, HWindow, exludeRect ? &r : NULL);
    if (cmd != 0)
        MainWindow->DirHistory->Execute(cmd, FALSE, this, TRUE, FALSE);

    EndStopRefresh(); // the snooper will start again now
}

void CFilesWindow::OpenStopFilterMenu()
{
    CALL_STACK_MESSAGE1("CFilesWindow::OpenStopFilterMenu()");

    BeginStopRefresh(); // snooper takes a break

    CMenuPopup menu;

    RECT r;
    GetWindowRect(HWindow, &r);
    BOOL exludeRect = FALSE;
    int y = r.top;
    if (DirectoryLine != NULL && DirectoryLine->HWindow != NULL)
    {
        if (DirectoryLine->GetFilterFrameRect(&r))
        {
            y = r.bottom;
            exludeRect = TRUE;
        }
    }

    /* used by the export_mnu.py script which generates salmenu.mnu for the Translator
       keep synchronized with the InsertItem() calls below...
MENU_TEMPLATE_ITEM StopFilterMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_HIDDEN_ATTRIBUTE
  {MNTT_IT, IDS_HIDDEN_FILTER
  {MNTT_IT, IDS_HIDDEN_HIDECMD
  {MNTT_PE, 0
};
*/
    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID | MENU_MASK_STATE;
    mii.Type = MENU_TYPE_STRING;

    mii.String = LoadStr(IDS_HIDDEN_ATTRIBUTE);
    mii.ID = 1;
    mii.State = (HiddenDirsFilesReason & HIDDEN_REASON_ATTRIBUTE) ? 0 : MENU_STATE_GRAYED;
    menu.InsertItem(-1, TRUE, &mii);

    mii.String = LoadStr(IDS_HIDDEN_FILTER);
    mii.ID = 2;
    mii.State = (HiddenDirsFilesReason & HIDDEN_REASON_FILTER) ? 0 : MENU_STATE_GRAYED;
    menu.InsertItem(-1, TRUE, &mii);

    mii.String = LoadStr(IDS_HIDDEN_HIDECMD);
    mii.ID = 3;
    mii.State = (HiddenDirsFilesReason & HIDDEN_REASON_HIDECMD) ? 0 : MENU_STATE_GRAYED;
    menu.InsertItem(-1, TRUE, &mii);

    DWORD cmd = menu.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_VERTICAL, r.left, y, HWindow, exludeRect ? &r : NULL);
    switch (cmd)
    {
    case 1:
    {
        PostMessage(MainWindow->HWindow, WM_COMMAND, CM_TOGGLEHIDDENFILES, 0);
        break;
    }

    case 2:
    {
        ChangeFilter(TRUE);
        break;
    }

    case 3:
    {
        ShowHideNames(0);
        break;
    }
    }

    EndStopRefresh(); // the snooper will start again now
}

// fills the popup based on available columns
BOOL CFilesWindow::FillSortByMenu(CMenuPopup* popup)
{
    CALL_STACK_MESSAGE1("CFilesWindow::FillSortByMenu()");

    // remove existing items
    popup->RemoveAllItems();

    /* used by the export_mnu.py script which generates salmenu.mnu for the Translator
       keep synchronized with the InsertItem() calls below...
MENU_TEMPLATE_ITEM SortByMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_COLUMN_MENU_NAME
  {MNTT_IT, IDS_COLUMN_MENU_EXT
  {MNTT_IT, IDS_COLUMN_MENU_TIME
  {MNTT_IT, IDS_COLUMN_MENU_SIZE
  {MNTT_IT, IDS_COLUMN_MENU_ATTR
  {MNTT_IT, IDS_MENU_LEFT_SORTOPTIONS
  {MNTT_PE, 0
};
*/

    // temporary solution for 1.6 beta 6: always populate the
    // Name, Ext, Date, and Size entries (regardless of ValidFileData)
    // the order must correspond to the CSortType enum
    int textResID[5] = {IDS_COLUMN_MENU_NAME, IDS_COLUMN_MENU_EXT, IDS_COLUMN_MENU_TIME, IDS_COLUMN_MENU_SIZE, IDS_COLUMN_MENU_ATTR};
    int leftCmdID[5] = {CM_LEFTNAME, CM_LEFTEXT, CM_LEFTTIME, CM_LEFTSIZE, CM_LEFTATTR};
    int rightCmdID[5] = {CM_RIGHTNAME, CM_RIGHTEXT, CM_RIGHTTIME, CM_RIGHTSIZE, CM_RIGHTATTR};
    int imgIndex[5] = {IDX_TB_SORTBYNAME, IDX_TB_SORTBYEXT, IDX_TB_SORTBYDATE, IDX_TB_SORTBYSIZE, -1};
    int* cmdID = MainWindow->LeftPanel == this ? leftCmdID : rightCmdID;
    MENU_ITEM_INFO mii;
    int i;
    for (i = 0; i < 5; i++)
    {
        mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_IMAGEINDEX | MENU_MASK_ID | MENU_MASK_STATE;
        mii.Type = MENU_TYPE_STRING;
        mii.String = LoadStr(textResID[i]);
        mii.ImageIndex = imgIndex[i];
        mii.ID = cmdID[i];
        mii.State = 0;
        if (SortType == (CSortType)i)
            mii.State = MENU_STATE_CHECKED;
        popup->InsertItem(-1, TRUE, &mii);
    }
    // separator
    mii.Mask = MENU_MASK_TYPE;
    mii.Type = MENU_TYPE_SEPARATOR;
    popup->InsertItem(-1, TRUE, &mii);
    // options
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID;
    mii.Type = MENU_TYPE_STRING;
    mii.String = LoadStr(IDS_MENU_LEFT_SORTOPTIONS);
    mii.ID = CM_SORTOPTIONS;
    popup->InsertItem(-1, TRUE, &mii);

    return TRUE;
}

void CFilesWindow::SetThumbnailSize(int size)
{
    if (size < THUMBNAIL_SIZE_MIN || size > THUMBNAIL_SIZE_MAX)
    {
        TRACE_E("size=" << size);
        size = THUMBNAIL_SIZE_DEFAULT;
    }
    if (ListBox == NULL)
    {
        TRACE_E("ListBox == NULL");
    }
    else
    {
        if (size != ListBox->ThumbnailWidth || size != ListBox->ThumbnailHeight)
        {
            // clear the icon cache
            SleepIconCacheThread();
            IconCache->Release();
            EndOfIconReadingTime = GetTickCount() - 10000;

            ListBox->ThumbnailWidth = size;
            ListBox->ThumbnailHeight = size;
        }
    }
}

int CFilesWindow::GetThumbnailSize()
{
    if (ListBox == NULL)
    {
        TRACE_E("ListBox == NULL");
        return THUMBNAIL_SIZE_DEFAULT;
    }
    else
    {
        if (ListBox->ThumbnailWidth != ListBox->ThumbnailHeight)
            TRACE_E("ThumbnailWidth != ThumbnailHeight");
        return ListBox->ThumbnailWidth;
    }
}

void CFilesWindow::SetFont()
{
    if (DirectoryLine != NULL)
        DirectoryLine->SetFont();
    //if (ListBox != NULL)  // this is set by the SetFont() call
    //  ListBox->SetFont();
    if (StatusLine != NULL)
        StatusLine->SetFont();
}

//****************************************************************************

void CFilesWindow::LockUI(BOOL lock)
{
    if (DirectoryLine != NULL && DirectoryLine->HWindow != NULL)
        EnableWindow(DirectoryLine->HWindow, !lock);
    if (StatusLine != NULL && StatusLine->HWindow != NULL)
        EnableWindow(StatusLine->HWindow, !lock);
    if (ListBox->HeaderLine.HWindow != NULL)
        EnableWindow(ListBox->HeaderLine.HWindow, !lock);
}
