// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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

// defining the event GUID "Lock Volume" (e.g. "chkdsk /f E:", where E: is a USB stick): {50708874-C9AF-11D1-8FEF-00A0C9A06D32}
GUID GUID_IO_LockVolume = {0x50708874, 0xC9AF, 0x11D1, 0x8F, 0xEF, 0x00, 0xA0, 0xC9, 0xA0, 0x6D, 0x32};
//
// in Ioevent.h from DDK is the definition of this constant (and many others):
//
//  Volume lock event. This event is signalled when an attempt is made to
//  Lock a volume. There is no additional data.
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
        //--- stretching the listbox across the entire window
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
        case 0x8006 /* DBT_CUSTOMEVENT*/:
        {
            //          TRACE_I("WM_DEVICECHANGE: DBT_CUSTOMEVENT");

            if (IsCustomEventGUID(lParam, GUID_IO_LockVolume))
            { // It occurs on XP machines when running "chkdsk /f e:" (where "e:" is a removable USB stick) and unfortunately also when opening .ifo and .vob files (DVD) and when launching Ashampoo Burning Studio 6 -- it requests "lock volume"
                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();                 // pause reading icons/thumbnails
                DetachDirectory((CFilesWindow*)this, TRUE); // close change notifications + DeviceNotification

                HANDLES(EnterCriticalSection(&TimeCounterSection));
                int t1 = MyTimeCounter++;
                HANDLES(LeaveCriticalSection(&TimeCounterSection));
                BOOL salIsActive = GetForegroundWindow() == MainWindow->HWindow;
                PostMessage(HWindow, WM_USER_REFRESH_DIR_EX, salIsActive, t1); // refresh refreshes reading of icons/thumbnails + reopens change notifications + DeviceNotification; we know that it is probably an unnecessary refresh
            }
            break;
        }

        case DBT_DEVICEQUERYREMOVE:
        {
            //          TRACE_I("WM_DEVICECHANGE: DBT_DEVICEQUERYREMOVE");
            DetachDirectory((CFilesWindow*)this, TRUE, FALSE); // without closing DeviceNotification
            return TRUE;                                       // Allow removal of this device
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
            DetachDirectory((CFilesWindow*)this, TRUE); // Close DeviceNotification
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
            ProgressDialogActivateDrop = NULL; // we need to clean up the global variable for the next use of the progress dialog
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
                Dirs->Count == 0 || strcmp(Dirs->At(0).Name, "..") != 0) // test if only ".." is being worked with
            {
                BeginSuspendMode(); // He was snoring in his sleep
                BeginStopRefresh(); // just to avoid distributing news about changes on the roads

                UserWorkedOnThisPath = TRUE;
                StoreSelection(); // Save the selection for the Restore Selection command

                ProgressDialogActivateDrop = LastWndFromGetData;

                int selectedDirs = 0;
                if (count > 0)
                {
                    // we will count how many directories are marked (the rest of the marked items are files)
                    int i;
                    for (i = 0; i < Dirs->Count; i++) // ".." cannot be marked, the test would be pointless
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
                    tgtPath[0] != 0 && tgtPath[1] == ':')       // classic disk path (C:\path)
                {
                    int l = (int)strlen(targetPath);
                    if (l > 3 && targetPath[l - 1] == '\\')
                        targetPath[l - 1] = 0; // Except for "c:\" we will remove the trailing backslash
                }
                targetPath[strlen(targetPath) + 1] = 0; // ensure two zeros at the end of the string

                // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                BOOL ret = GetPluginFS()->CopyOrMoveFromFS(copy, 5, GetPluginFS()->GetPluginFSName(),
                                                           HWindow, panel,
                                                           count - selectedDirs, selectedDirs,
                                                           targetPath, operationMask,
                                                           cancelOrHandlePath,
                                                           ProgressDialogActivateDrop);

                // increase the thread priority again, operation completed
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                if (ret && !cancelOrHandlePath)
                {
                    if (targetPath[0] != 0) // change focus to 'targetPath'
                    {
                        lstrcpyn(NextFocusName, targetPath, MAX_PATH);
                        // RefreshDirectory may not have to run - the source may not have changed - for safety, we will post a message
                        PostMessage(HWindow, WM_USER_DONEXTFOCUS, 0, 0);
                    }

                    // Successful operation, but we won't deselect the source because it's a drag & drop
                    //            SetSel(FALSE, -1, TRUE);   // explicit redraw
                    //            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);  // sel-change notify
                    UpdateWindow(MainWindow->HWindow);
                }

                ProgressDialogActivateDrop = NULL;              // we need to clean up the global variable for the next use of the progress dialog
                if (tgtPath[0] == '\\' && tgtPath[1] == '\\' || // UNC path
                    tgtPath[0] != 0 && tgtPath[1] == ':')       // classic disk path (C:\path)
                {
                    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH, tgtPath, NULL);
                }

                EndStopRefresh();
                EndSuspendMode(); // now he's sniffling again, he'll start up
            }
        }
        free(tgtPath);
        return 0;
    }

    case WM_USER_UPDATEPANEL:
    {
        // someone distributed messages (a messagebox opened) and needs to be updated
        // content of the panel
        RefreshListBox(0, -1, -1, FALSE, FALSE);
        return 0;
    }

    case WM_USER_ENTERMENULOOP:
    case WM_USER_LEAVEMENULOOP:
    {
        // only pass to the main window
        return SendMessage(MainWindow->HWindow, uMsg, wParam, lParam);
    }

    case WM_USER_CONTEXTMENU:
    {
        CMenuPopup* popup = (CMenuPopup*)(CGUIMenuPopupAbstract*)wParam;
        // if the Alt+F1(2) menu is open above this panel and RClick belongs to it,
        // we will pass him the notification
        if (OpenedDrivesList != NULL && OpenedDrivesList->GetMenuPopup() == popup)
        {
            return OpenedDrivesList->OnContextMenu((BOOL)lParam, -1, PANEL_SOURCE, NULL);
        }
        return FALSE; //p.s. do not execute the command, do not open the submenu
    }

    case WM_TIMER:
    {
        if (wParam == IDT_SM_END_NOTIFY)
        {
            KillTimer(HWindow, IDT_SM_END_NOTIFY);
            if (SmEndNotifyTimerSet) // it's not just about a "stray" WM_TIMER
                PostMessage(HWindow, WM_USER_SM_END_NOTIFY_DELAYED, 0, 0);
            SmEndNotifyTimerSet = FALSE;
            return 0;
        }
        else
        {
            if (wParam == IDT_REFRESH_DIR_EX)
            {
                KillTimer(HWindow, IDT_REFRESH_DIR_EX);
                if (RefreshDirExTimerSet) // it's not just about a "stray" WM_TIMER
                    PostMessage(HWindow, WM_USER_REFRESH_DIR_EX_DELAYED, FALSE, RefreshDirExLParam);
                RefreshDirExTimerSet = FALSE;
                return 0;
            }
            else
            {
                if (wParam == IDT_ICONOVRREFRESH)
                {
                    KillTimer(HWindow, IDT_ICONOVRREFRESH);
                    if (IconOvrRefreshTimerSet && // it's not just about a "stray" WM_TIMER
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
                        if (InactiveRefreshTimerSet) // it's not just about a "stray" WM_TIMER
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
        else // waiting for sending WM_USER_REFRESH_DIR_EX_DELAYED
        {
            if (RefreshDirExLParam < lParam) // let's take the "newer" time
                RefreshDirExLParam = lParam;

            KillTimer(HWindow, IDT_REFRESH_DIR_EX); // we set the timer again so that slow is slow (5000ms) and fast is fast (200ms) - the abbreviation must not depend on the type of the previous refresh
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
        // here the break is not missing -- if the timer cannot be set up, WM_USER_SM_END_NOTIFY_DELAYED will be executed immediately
    }
        //--- suspend mode was terminated, let's see if we need to refresh
    case WM_USER_SM_END_NOTIFY_DELAYED:
    {
        if (SnooperSuspended || StopRefresh)
            return 0;                        // Wait for the next WM_USER_SM_END_NOTIFY_DELAYED
        if (PluginFSNeedRefreshAfterEndOfSM) // Does the FS plug-in need to be refreshed?
        {
            PluginFSNeedRefreshAfterEndOfSM = FALSE;
            PostMessage(HWindow, WM_USER_REFRESH_PLUGINFS, 0, 0); // let's try to execute it now
        }

        if (NeedRefreshAfterEndOfSM) // Should it be refreshed?
        {
            NeedRefreshAfterEndOfSM = FALSE;
            lParam = RefreshAfterEndOfSMTime;
            wParam = FALSE; // We will not throw RefreshFinishedEvent
        }
        else
            return 0;
    }
        //--- a change in content was recorded in the directory during suspend mode
    case WM_USER_S_REFRESH_DIR:
    {
        if (uMsg == WM_USER_S_REFRESH_DIR && // change in content recorded during suspend mode
            !IconCacheValid && UseSystemIcons && Is(ptDisk) && GetNetworkDrive())
        {
            // TRACE_I("Delaying refresh from suspend mode until all icons are read.");
            NeedRefreshAfterIconsReading = TRUE;
            RefreshAfterIconsReadingTime = max(RefreshAfterIconsReadingTime, (int)lParam);
            if (wParam)
                SetEvent(RefreshFinishedEvent); // probably unnecessary, but it is in the description of WM_USER_S_REFRESH_DIR
            return 0;                           // We have noted the change report (refresh will be posted after the reading of icons is completed), we are finishing processing
        }

        setWait = FALSE;
        if (lParam >= LastRefreshTime)
        {                                                          // It's not about an unnecessary old refresh
            setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
            if (setWait)
                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
            DWORD err = CheckPath(FALSE, NULL, ERROR_SUCCESS, !SnooperSuspended && !StopRefresh);
            if (err == ERROR_SUCCESS)
            {
                if (GetMonitorChanges()) // the snooper may have kicked him off the list
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
        //--- reading of icons was completed, let's check if a refresh is needed
    case WM_USER_ICONREADING_END:
    {
        //      TRACE_I("WM_USER_ICONREADING_END");
        probablyUselessRefresh = FALSE;
        if (uMsg == WM_USER_ICONREADING_END)
        {
            IconCacheValid = TRUE;
            EndOfIconReadingTime = GetTickCount();
            if (NeedRefreshAfterIconsReading) // Should it be refreshed?
            {
                //          TRACE_I("Doing delayed refresh (all icons are read).");
                NeedRefreshAfterIconsReading = FALSE;
                lParam = RefreshAfterIconsReadingTime;
                wParam = FALSE; // We will not throw RefreshFinishedEvent
                setWait = FALSE;
                probablyUselessRefresh = TRUE; // probably just a refresh triggered incorrectly by the system after loading icons from a network drive
                                               //          TRACE_I("delayed refresh (after reading of all icons): probablyUselessRefresh=TRUE");
            }
            else
            {
                if (NeedIconOvrRefreshAfterIconsReading) // refresh the icon overlays
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
        //--- a change in the directory content was recorded
    case WM_USER_REFRESH_DIR:
    case WM_USER_REFRESH_DIR_EX_DELAYED:
    case WM_USER_INACTREFRESH_DIR:
    {
        //      if (uMsg == WM_USER_INACTREFRESH_DIR) TRACE_I("WM_USER_INACTREFRESH_DIR");
        if (uMsg != WM_USER_ICONREADING_END)
        {
            if (GetTickCount() - EndOfIconReadingTime < 1000)
            {
                probablyUselessRefresh = TRUE; // 1 second after reading the icons, we still expect an unnecessary refresh caused by reading the icons
                                               //          TRACE_I("less than second after reading of icons was finished: probablyUselessRefresh=TRUE");
            }
            else
            {
                probablyUselessRefresh = (uMsg == WM_USER_REFRESH_DIR_EX_DELAYED || uMsg == WM_USER_INACTREFRESH_DIR); // It is about a delayed refresh, which can also be unnecessary (this prevents an infinite cycle when reading icons on a network drive, which triggers another refresh)
                                                                                                                       //          TRACE_I("WM_USER_REFRESH_DIR_EX_DELAYED or WM_USER_INACTREFRESH_DIR: probablyUselessRefresh=" << probablyUselessRefresh);
            }
        }
        if ((uMsg == WM_USER_REFRESH_DIR && wParam || // change of content reported by snooper
             uMsg == WM_USER_ICONREADING_END ||       // or reporting the end of reading icons (may come late, icons can be read again)
             uMsg == WM_USER_INACTREFRESH_DIR) &&     // or deferred refresh in an inactive window (it is a refresh requested by the snooper or when exiting suspend mode)
            !IconCacheValid &&
            UseSystemIcons && Is(ptDisk) && GetNetworkDrive())
        {
            //        TRACE_I("Delaying refresh until all icons are read.");
            NeedRefreshAfterIconsReading = TRUE;
            RefreshAfterIconsReadingTime = max(RefreshAfterIconsReadingTime, (int)lParam);
            // We have noted the change report (refresh will be posted after the reading of icons is completed), we are finishing processing
        }
        else
        {
            if (SnooperSuspended || StopRefresh)
            { // suspend mode is now turned on (working on internal data -> cannot be refreshed)
                NeedRefreshAfterEndOfSM = TRUE;
                RefreshAfterEndOfSMTime = max(RefreshAfterEndOfSMTime, (int)lParam);
                if ((uMsg == WM_USER_S_REFRESH_DIR || uMsg == WM_USER_SM_END_NOTIFY_DELAYED) && setWait)
                {
                    SetCursor(oldCur);
                }
            }
            else // It's not about refresh in suspend mode
            {
                if (lParam >= LastRefreshTime) // It's not about an unnecessary old refresh
                {
                    BOOL isInactiveRefresh = FALSE;
                    BOOL skipRefresh = FALSE;
                    if ((uMsg == WM_USER_REFRESH_DIR && wParam ||     // change of content reported by snooper
                         uMsg == WM_USER_ICONREADING_END ||           // or reporting the end of reading icons (delayed refresh requested by snooper + after exiting suspend mode)
                         uMsg == WM_USER_INACTREFRESH_DIR) &&         // or deferred refresh in an inactive window (it is a refresh requested by the snooper or when exiting suspend mode)
                        GetForegroundWindow() != MainWindow->HWindow) // inactive main window of Salamander: slow down refresh if necessary
                    {
                        //              TRACE_I("Refresh from snooper in inactive window");
                        isInactiveRefresh = TRUE;
                        if (LastInactiveRefreshStart != LastInactiveRefreshEnd) // some refresh has already occurred since the last deactivation
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
                            if (InactiveRefreshTimerSet ||                 // timer is already running, we just need to wait for it
                                ti - LastInactiveRefreshEnd + 100 < delay) // +100 to prevent the timer from being "wasted" (so that the delay of the refresh is at least 100ms)
                            {
                                //                  TRACE_I("Delaying refresh");
                                if (!InactiveRefreshTimerSet) // timer is not running yet, let's start it
                                {
                                    //                    TRACE_I("Setting timer");
                                    if (SetTimer(HWindow, IDT_INACTIVEREFRESH, max(200, delay - (ti - LastInactiveRefreshEnd)), NULL))
                                    {
                                        InactiveRefreshTimerSet = TRUE;
                                        InactRefreshLParam = lParam;
                                        skipRefresh = TRUE;
                                    }
                                }
                                else // timer is already running, we just need to wait for it
                                {
                                    //                    TRACE_I("Timer already set");
                                    if (lParam > InactRefreshLParam)
                                        InactRefreshLParam = lParam; // we will take the newer time into InactRefreshLParam
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
                            setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
                            if (setWait)
                                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                        }
                        char pathBackup[MAX_PATH];
                        CPanelType typeBackup;
                        if (isInactiveRefresh)
                        {
                            lstrcpyn(pathBackup, GetPath(), MAX_PATH); // We are only interested in disk paths and paths to archives (our snooper does not inform us about changes in plugin-FS)
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
                            { // if the path has changed (most likely someone just deleted the directory displayed in the panel), we will perform an additional refresh without waiting (it can be expected that they will also delete the directory newly displayed in the panel, so that we can quickly "retreat" from it)
                                LastInactiveRefreshEnd = LastInactiveRefreshStart;
                            }
                            else
                            {
                                LastInactiveRefreshEnd = GetTickCount();
                                if ((int)(LastInactiveRefreshEnd - LastInactiveRefreshStart) <= 0)
                                    LastInactiveRefreshEnd = LastInactiveRefreshStart + 1; // must not be the same (this is the "no refresh yet" state)
                            }
                        }
                        /*  // Petr: I don't know why the setting of LastRefreshTime was here - logically, if there is a change during refresh, another refresh is necessary - it was a mess in Nethood because the enumeration thread managed to post a refresh before the completion of RefreshDirectory, so it was ignored (it's a refresh during refresh)
              HANDLES(EnterCriticalSection(&TimeCounterSection));
              LastRefreshTime = MyTimeCounter++;
              HANDLES(LeaveCriticalSection(&TimeCounterSection));*/
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
        { // suspend mode is now turned on (working on internal data -> cannot be refreshed)
            // we can also be inside the plug-in -> we do not support multiple calls to plug-in methods
            PluginFSNeedRefreshAfterEndOfSM = TRUE;
        }
        else
        { // We are not inside the plug-in
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
            // if it was about loading a "static" association icon, we save it to Associations (count
            // if it's thumbnaily - the condition won't fall on Flag==1 or 2)
            if (file != NULL && !isDir &&                                   // it's about the file
                (!Is(ptPluginFS) || GetPluginIconsType() != pitFromPlugin)) // It's not about the icon from the plug-in
            {
                char buf[MAX_PATH + 4]; // suffix in lowercase letters
                char *s1 = buf, *s2 = file->Ext;
                while (*s2 != 0)
                    *s1++ = LowerCase[*s2++];
                *((DWORD*)s1) = 0;
                int index;
                CIconSizeEnum iconSize = IconCache->GetIconSize();
                if (Associations.GetIndex(buf, index) &&             // extension has an icon (association)
                    (Associations[index].GetIndex(iconSize) == -1 || // It's about an icon that is being loaded
                     Associations[index].GetIndex(iconSize) == -3))
                {
                    int icon;
                    CIconList* srcIconList;
                    int srcIconListIndex;
                    memmove(buf, file->Name, file->NameLen);
                    *(DWORD*)(buf + file->NameLen) = 0;
                    if (IconCache->GetIndex(buf, icon, NULL, NULL) &&                                 // icon-thread is loading it
                        (IconCache->At(icon).GetFlag() == 1 || IconCache->At(icon).GetFlag() == 2) && // icon is loaded new or old
                        IconCache->GetIcon(IconCache->At(icon).GetIndex(),
                                           &srcIconList, &srcIconListIndex)) // it will be possible to obtain the loaded icon
                    {                                                        // icon for the extension -> icon-thread has already loaded it
                        CIconList* dstIconList;
                        int dstIconListIndex;
                        int i = Associations.AllocIcon(&dstIconList, &dstIconListIndex, iconSize);
                        if (i != -1) // we have obtained a spot for a new icon
                        {            // we will copy it from IconCache to Associations
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
                                // Redraw panels only if they match the size of the icons
                                if (iconSize == GetIconSizeForCurrentViewMode())
                                    RepaintIconOnly(-1); // all of us here

                                CFilesWindow* otherPanel = MainWindow->GetOtherPanel(this);
                                if (iconSize == otherPanel->GetIconSizeForCurrentViewMode())
                                    otherPanel->RepaintIconOnly(-1); // and all the neighbors
                            }
                            else
                                PostAllIconsRepaint = TRUE;
                        }
                    }
                }
            }
        }

        // we will perform a redraw of the affected index
        if (file != NULL) // file is used here only for testing for NULL
        {
            if (!StopIconRepaint) // if icon refreshing is allowed
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
        // We will perform postprocessing only on paths that we obtained as text (and not directly by dropping the directory)
        char buff[2 * MAX_PATH];
        strcpy_s(buff, (char*)lParam);
        if (!(BOOL)wParam || PostProcessPathFromUser(HWindow, buff))
            ChangeDir(buff, -1, NULL, 3 /*change-dir*/, NULL, (BOOL)wParam);
        return 0;
    }

    case WM_USER_FOCUSFILE:
    {
        // We need to pull the window out here because ChangeDir can be called during the call
        // to pop up a messagebox (path does not exist) and it would remain under Find.
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
            //        SetForegroundWindow(MainWindow->HWindow);  // it's too late here - moved up
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
        ViewFile(data->FileName, FALSE, (DWORD)lParam, data->EnumFileNamesSourceUID, // FIXME_X64 - verify typecasting to (DWORD)
                 data->EnumFileNamesLastFileIndex);
        return 0;
    }

    case WM_USER_EDITFILEWITH:
    {
        EditFile((char*)wParam, (DWORD)lParam); // FIXME_X64 - verify typecasting to (DWORD)
        return 0;
    }

        //    case WM_USER_RENAME_NEXT_ITEM:
        //    {
        //      int index = GetCaretIndex();
        //      QuickRenameOnIndex(index + (wParam ? 1 : -1));
        //      return 0;
        //    }

    case WM_USER_DONEXTFOCUS: // if RefreshDirectory hasn't done it yet, we'll do it here
    {
        DontClearNextFocusName = FALSE;
        if (NextFocusName[0] != 0) // if there is something to focus on
        {
            int total = Files->Count + Dirs->Count;
            int found = -1;
            int i;
            for (i = 0; i < total; i++)
            {
                CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                if (StrICmp(f->Name, NextFocusName) == 0)
                {
                    if (strcmp(f->Name, NextFocusName) == 0) // file found exactly
                    {
                        NextFocusName[0] = 0;
                        SetCaretIndex(i, FALSE);
                        break;
                    }
                    if (found == -1)
                        found = i; // file found (ignore-case)
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
                        displaySize = FALSE; // file of unknown size or directory without known/computed size
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
                            varPlacementsCount = 100; // could be damaged
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
                    varPlacementsCount = 100; // could be damaged
                }
            }
            else
                TRACE_E("Unexpected situation in CFilesWindow::WindowProc(WM_USER_SELCHANGED)");
        }

        if (count == 0)
        {
            LastFocus = INT_MAX;
            int index = GetCaretIndex();
            ItemFocused(index); // when unchecked
        }
        IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
        return 0;
    }

    case WM_CREATE:
    {
        //--- adding this panel to the array of resources for enumerating files in viewers
        EnumFileNamesAddSourceUID(HWindow, &EnumFileNamesSourceUID);

        //--- creating a listbox with files and directories
        ListBox = new CFilesBox(this);
        if (ListBox == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        //--- creating a status line with information about the current file
        StatusLine = new CStatusWindow(this, blBottom, ooStatic);
        if (StatusLine == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        ToggleStatusLine();
        //--- creating a status line with information about the current directory
        DirectoryLine = new CStatusWindow(this, blTop, ooStatic);
        if (DirectoryLine == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        DirectoryLine->SetLeftPanel(MainWindow->LeftPanel == this);
        ToggleDirectoryLine();
        //--- setting up the view type + loading the contents of the directory
        SetThumbnailSize(Configuration.ThumbnailSize); // There must be a ListBox
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

        // Compare the settings of the AutomaticRefresh variable and directory-liny
        SetAutomaticRefresh(AutomaticRefresh, TRUE);

        return 0;
    }

    case WM_DESTROY:
    {
        //--- removing this panel from the list of sources for file enumeration in viewers
        EnumFileNamesRemoveSourceUID(HWindow);

        CancelUI(); // cancel QuickSearch and QuickEdit
        LastRefreshTime = INT_MAX;
        BeginStopRefresh();
        DetachDirectory(this);
        //--- releasing child window
        RevokeDragDrop();
        ListBox->DetachWindow();
        delete ListBox;
        ListBox = NULL; // just to be sure, let errors show up...

        StatusLine->DestroyWindow();
        delete StatusLine;
        StatusLine = NULL;

        DirectoryLine->DestroyWindow();
        delete DirectoryLine;
        DirectoryLine = NULL; // parachute repair
                              //---
        return 0;
    }

    case WM_USER_ENUMFILENAMES: // searching for the next/previous name for viewer
    {
        HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));

        if (InactiveRefreshTimerSet) // if there is a delayed refresh here, we must perform it immediately, otherwise we will be enumerating over an outdated listing; if it takes longer, it doesn't matter, GetFileNameForViewer will wait for the result...
        {
            //        TRACE_I("Refreshing during enumeration (refresh in inactive window was delayed)");
            KillTimer(HWindow, IDT_INACTIVEREFRESH);
            InactiveRefreshTimerSet = FALSE;
            LastInactiveRefreshEnd = LastInactiveRefreshStart;
            SendMessage(HWindow, WM_USER_INACTREFRESH_DIR, FALSE, InactRefreshLParam);
        }

        if ((int)wParam /* reqUID*/ == FileNamesEnumData.RequestUID && // no further request was made (this one would then be useless)
            EnumFileNamesSourceUID == FileNamesEnumData.SrcUID &&       // no change in the source
            !FileNamesEnumData.TimedOut)                                // someone is still waiting for the result
        {
            if (Files != NULL && Is(ptDisk))
            {
                BOOL selExists = FALSE;
                if (FileNamesEnumData.PreferSelected) // if necessary, we will determine if the selectiona exists
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
                if (index == -1) // search from the first or from the last
                {
                    if (FileNamesEnumData.RequestType == fnertFindPrevious)
                        index = count; // search for previous + start at the last
                                       // else  // looking for the next one + starting from the first one
                }
                else
                {
                    if (FileNamesEnumData.LastFileName[0] != 0) // we know the full file name as 'index', we will check if the array has not been spread/collapsed + possibly find a new index
                    {
                        int pathLen = (int)strlen(GetPath());
                        if (StrNICmp(GetPath(), FileNamesEnumData.LastFileName, pathLen) == 0)
                        { // the path to the file must match the path in the panel ("always true")
                            const char* name = FileNamesEnumData.LastFileName + pathLen;
                            if (*name == '\\' || *name == '/')
                                name++;

                            CFileData* f = (index >= 0 && index < count) ? &Files->At(index) : NULL;
                            BOOL nameIsSame = f != NULL && StrICmp(name, f->Name) == 0;
                            if (nameIsSame)
                                indexNotFound = FALSE;
                            if (f == NULL || !nameIsSame)
                            { // the name at index 'index' is not FileNamesEnumData.LastFileName, let's try to find a new index for this name
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
                if (FileNamesEnumData.OnlyAssociatedExtensions) // Does the viewer want filtering by associated extensions?
                {
                    if (FileNamesEnumData.Plugin != NULL) // viewer from plugin
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

                case fnertIsSelected: // finding the label
                {
                    if (!indexNotFound && index >= 0 && index < Files->Count)
                    {
                        FileNamesEnumData.IsFileSelected = Files->At(index).Selected;
                        FileNamesEnumData.Found = TRUE;
                    }
                    break;
                }

                case fnertSetSelection: // setting the label
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

    BeginStopRefresh(); // He was snoring in his sleep

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

    EndStopRefresh(); // now he's sniffling again, he'll start up
}

void CFilesWindow::OpenStopFilterMenu()
{
    CALL_STACK_MESSAGE1("CFilesWindow::OpenStopFilterMenu()");

    BeginStopRefresh(); // He was snoring in his sleep

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

    /* used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   to keep synchronized with the InsertItem() calls below...
MENU_TEMPLATE_ITEM StopFilterMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_HIDDEN_ATTRIBUTE
  {MNTT_IT, IDS_HIDDEN_FILTER
  {MNTT_IT, IDS_HIDDEN_HIDECMD
  {MNTT_PE, 0
};*/
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

    EndStopRefresh(); // now he's sniffling again, he'll start up
}

// populates the popup based on available columns
BOOL CFilesWindow::FillSortByMenu(CMenuPopup* popup)
{
    CALL_STACK_MESSAGE1("CFilesWindow::FillSortByMenu()");

    // Remove existing items
    popup->RemoveAllItems();

    /* is used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   to keep synchronized with the InsertItem() calls below...
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
};*/

    // Temporary solution for 1.6 beta 6: always pour (regardless of ValidFileData)
    // fields Name, Ext, Date, Size
    // Order must correspond to the CSortType enum
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
            // Clearing icon cache
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
    //if (ListBox != NULL)  // this is set from calling SetFont()
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
