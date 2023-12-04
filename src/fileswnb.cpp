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

// definujeme GUID udalosti "Lock Volume" (napr. "chkdsk /f E:", kde E: je USB stick): {50708874-C9AF-11D1-8FEF-00A0C9A06D32}
GUID GUID_IO_LockVolume = {0x50708874, 0xC9AF, 0x11D1, 0x8F, 0xEF, 0x00, 0xA0, 0xC9, 0xA0, 0x6D, 0x32};
//
// v Ioevent.h z DDK je definice teto konstanty (a mnoha dalsich):
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
        //---  roztahovani listboxu po celem okne
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
            { // chodi na XPckach, kdyz se spusti "chkdsk /f e:" ("e:" je removable USB stick) a bohuzel taky pri otevirani .ifo a .vob souboru (DVD) a pri spousteni Ashampoo Burning Studio 6 -- zadost "lock volume"
                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();                 // pozastavime cteni ikonek/thumbnailu
                DetachDirectory((CFilesWindow*)this, TRUE); // zavreme change-notifikace + DeviceNotification

                HANDLES(EnterCriticalSection(&TimeCounterSection));
                int t1 = MyTimeCounter++;
                HANDLES(LeaveCriticalSection(&TimeCounterSection));
                BOOL salIsActive = GetForegroundWindow() == MainWindow->HWindow;
                PostMessage(HWindow, WM_USER_REFRESH_DIR_EX, salIsActive, t1); // refresh obnovi cteni ikon/thumbnailu + otevre opet change-notifikace + DeviceNotification; vime, ze jde pravdepodobne o zbytecny refresh
            }
            break;
        }

        case DBT_DEVICEQUERYREMOVE:
        {
            //          TRACE_I("WM_DEVICECHANGE: DBT_DEVICEQUERYREMOVE");
            DetachDirectory((CFilesWindow*)this, TRUE, FALSE); // bez zavreni DeviceNotification
            return TRUE;                                       // povolime odstraneni tohoto device
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
            DetachDirectory((CFilesWindow*)this, TRUE); // zavreme DeviceNotification
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
            ProgressDialogActivateDrop = NULL; // pro dalsi pouziti progress dialogu musime globalku vycistit
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
                Dirs->Count == 0 || strcmp(Dirs->At(0).Name, "..") != 0) // test jestli se nepracuje jen s ".."
            {
                BeginSuspendMode(); // cmuchal si da pohov
                BeginStopRefresh(); // jen aby se nedistribuovaly zpravy o zmenach na cestach

                UserWorkedOnThisPath = TRUE;
                StoreSelection(); // ulozime selection pro prikaz Restore Selection

                ProgressDialogActivateDrop = LastWndFromGetData;

                int selectedDirs = 0;
                if (count > 0)
                {
                    // spocitame kolik adresaru je oznaceno (zbytek oznacenych polozek jsou soubory)
                    int i;
                    for (i = 0; i < Dirs->Count; i++) // ".." nemuzou byt oznaceny, test by byl zbytecny
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
                if (tgtPath[0] == '\\' && tgtPath[1] == '\\' || // UNC cesta
                    tgtPath[0] != 0 && tgtPath[1] == ':')       // klasicka diskova cesta (C:\path)
                {
                    int l = (int)strlen(targetPath);
                    if (l > 3 && targetPath[l - 1] == '\\')
                        targetPath[l - 1] = 0; // krom "c:\" zrusime koncovy backslash
                }
                targetPath[strlen(targetPath) + 1] = 0; // zajistime dve nuly na konci retezce

                // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                BOOL ret = GetPluginFS()->CopyOrMoveFromFS(copy, 5, GetPluginFS()->GetPluginFSName(),
                                                           HWindow, panel,
                                                           count - selectedDirs, selectedDirs,
                                                           targetPath, operationMask,
                                                           cancelOrHandlePath,
                                                           ProgressDialogActivateDrop);

                // opet zvysime prioritu threadu, operace dobehla
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                if (ret && !cancelOrHandlePath)
                {
                    if (targetPath[0] != 0) // zmena fokusu na 'targetPath'
                    {
                        lstrcpyn(NextFocusName, targetPath, MAX_PATH);
                        // RefreshDirectory nemusi probehnout - zdroj se nemusel zmenit - pro sichr postneme message
                        PostMessage(HWindow, WM_USER_DONEXTFOCUS, 0, 0);
                    }

                    // uspesna operace, ale zdroj neodznacime, protoze jde o drag&drop
                    //            SetSel(FALSE, -1, TRUE);   // explicitni prekresleni
                    //            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);  // sel-change notify
                    UpdateWindow(MainWindow->HWindow);
                }

                ProgressDialogActivateDrop = NULL;              // pro dalsi pouziti progress dialogu musime globalku vycistit
                if (tgtPath[0] == '\\' && tgtPath[1] == '\\' || // UNC cesta
                    tgtPath[0] != 0 && tgtPath[1] == ':')       // klasicka diskova cesta (C:\path)
                {
                    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH, tgtPath, NULL);
                }

                EndStopRefresh();
                EndSuspendMode(); // ted uz zase cmuchal nastartuje
            }
        }
        free(tgtPath);
        return 0;
    }

    case WM_USER_UPDATEPANEL:
    {
        // nekdo rozdistribuoval zpravy (otevrel se messagebox) a je treba updatnou
        // obsah panelu
        RefreshListBox(0, -1, -1, FALSE, FALSE);
        return 0;
    }

    case WM_USER_ENTERMENULOOP:
    case WM_USER_LEAVEMENULOOP:
    {
        // pouze predame hlavnimu oknu
        return SendMessage(MainWindow->HWindow, uMsg, wParam, lParam);
    }

    case WM_USER_CONTEXTMENU:
    {
        CMenuPopup* popup = (CMenuPopup*)(CGUIMenuPopupAbstract*)wParam;
        // pokud je nad timto panelem otevrene Alt+F1(2) menu a RClick patri jemu,
        // predame mu notifikaci
        if (OpenedDrivesList != NULL && OpenedDrivesList->GetMenuPopup() == popup)
        {
            return OpenedDrivesList->OnContextMenu((BOOL)lParam, -1, PANEL_SOURCE, NULL);
        }
        return FALSE; //p.s. nespoustet prikaz, neotevirat submenu
    }

    case WM_TIMER:
    {
        if (wParam == IDT_SM_END_NOTIFY)
        {
            KillTimer(HWindow, IDT_SM_END_NOTIFY);
            if (SmEndNotifyTimerSet) // nejde jen o "zatoulany" WM_TIMER
                PostMessage(HWindow, WM_USER_SM_END_NOTIFY_DELAYED, 0, 0);
            SmEndNotifyTimerSet = FALSE;
            return 0;
        }
        else
        {
            if (wParam == IDT_REFRESH_DIR_EX)
            {
                KillTimer(HWindow, IDT_REFRESH_DIR_EX);
                if (RefreshDirExTimerSet) // nejde jen o "zatoulany" WM_TIMER
                    PostMessage(HWindow, WM_USER_REFRESH_DIR_EX_DELAYED, FALSE, RefreshDirExLParam);
                RefreshDirExTimerSet = FALSE;
                return 0;
            }
            else
            {
                if (wParam == IDT_ICONOVRREFRESH)
                {
                    KillTimer(HWindow, IDT_ICONOVRREFRESH);
                    if (IconOvrRefreshTimerSet && // nejde jen o "zatoulany" WM_TIMER
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
                        if (InactiveRefreshTimerSet) // nejde jen o "zatoulany" WM_TIMER
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
        else // cekame na poslani WM_USER_REFRESH_DIR_EX_DELAYED
        {
            if (RefreshDirExLParam < lParam) // vezmeme "novejsi" cas
                RefreshDirExLParam = lParam;

            KillTimer(HWindow, IDT_REFRESH_DIR_EX); // timer nahodime znovu, aby pomaly byl pomaly (5000ms) a rychly byl rychly (200ms) - zkratka nesmi zalezet na typu predchoziho refreshe
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
        // tady break nechybi -- pokud se nepovede zalozit timer, provede se WM_USER_SM_END_NOTIFY_DELAYED hned
    }
        //--- byl ukoncen suspend mode, podivame se, jestli nepotrebujeme refresh
    case WM_USER_SM_END_NOTIFY_DELAYED:
    {
        if (SnooperSuspended || StopRefresh)
            return 0;                        // pockame na dalsi WM_USER_SM_END_NOTIFY_DELAYED
        if (PluginFSNeedRefreshAfterEndOfSM) // ma dojit k refreshi plug-in FS?
        {
            PluginFSNeedRefreshAfterEndOfSM = FALSE;
            PostMessage(HWindow, WM_USER_REFRESH_PLUGINFS, 0, 0); // zkusime ho provest ted
        }

        if (NeedRefreshAfterEndOfSM) // ma dojit k refreshi?
        {
            NeedRefreshAfterEndOfSM = FALSE;
            lParam = RefreshAfterEndOfSMTime;
            wParam = FALSE; // nebudeme nahazovat RefreshFinishedEvent
        }
        else
            return 0;
    }
        //--- v adresari byla zaznamenana zmena obsahu pri suspend modu
    case WM_USER_S_REFRESH_DIR:
    {
        if (uMsg == WM_USER_S_REFRESH_DIR && // zmena obsahu zaznamenana behem suspend modu
            !IconCacheValid && UseSystemIcons && Is(ptDisk) && GetNetworkDrive())
        {
            // TRACE_I("Delaying refresh from suspend mode until all icons are read.");
            NeedRefreshAfterIconsReading = TRUE;
            RefreshAfterIconsReadingTime = max(RefreshAfterIconsReadingTime, (int)lParam);
            if (wParam)
                SetEvent(RefreshFinishedEvent); // nejspis zbytecne, ale je to v popisu WM_USER_S_REFRESH_DIR
            return 0;                           // hlaseni o zmene jsme poznamenali (refresh se postne az se dokonci cteni ikonek), koncime zpracovani
        }

        setWait = FALSE;
        if (lParam >= LastRefreshTime)
        {                                                          // nejde o zbytecny stary refresh
            setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // ceka uz ?
            if (setWait)
                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
            DWORD err = CheckPath(FALSE, NULL, ERROR_SUCCESS, !SnooperSuspended && !StopRefresh);
            if (err == ERROR_SUCCESS)
            {
                if (GetMonitorChanges()) // snooper ho mozna vykopnul ze seznamu
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
        //--- bylo ukonceno cteni ikonek, podivame se, jestli nepotrebujeme refresh
    case WM_USER_ICONREADING_END:
    {
        //      TRACE_I("WM_USER_ICONREADING_END");
        probablyUselessRefresh = FALSE;
        if (uMsg == WM_USER_ICONREADING_END)
        {
            IconCacheValid = TRUE;
            EndOfIconReadingTime = GetTickCount();
            if (NeedRefreshAfterIconsReading) // ma dojit k refreshi?
            {
                //          TRACE_I("Doing delayed refresh (all icons are read).");
                NeedRefreshAfterIconsReading = FALSE;
                lParam = RefreshAfterIconsReadingTime;
                wParam = FALSE; // nebudeme nahazovat RefreshFinishedEvent
                setWait = FALSE;
                probablyUselessRefresh = TRUE; // nejspis jen refresh vyvolany chybne systemem po nacitani ikonek ze sitoveho drivu
                                               //          TRACE_I("delayed refresh (after reading of all icons): probablyUselessRefresh=TRUE");
            }
            else
            {
                if (NeedIconOvrRefreshAfterIconsReading) // refreshneme icon-overlays
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
        //--- v adresari byla zaznamenana zmena obsahu
    case WM_USER_REFRESH_DIR:
    case WM_USER_REFRESH_DIR_EX_DELAYED:
    case WM_USER_INACTREFRESH_DIR:
    {
        //      if (uMsg == WM_USER_INACTREFRESH_DIR) TRACE_I("WM_USER_INACTREFRESH_DIR");
        if (uMsg != WM_USER_ICONREADING_END)
        {
            if (GetTickCount() - EndOfIconReadingTime < 1000)
            {
                probablyUselessRefresh = TRUE; // do 1 sekundy po dokonceni cteni ikon jeste ocekavame zbytecny refresh zpusobeny ctenim ikonek
                                               //          TRACE_I("less than second after reading of icons was finished: probablyUselessRefresh=TRUE");
            }
            else
            {
                probablyUselessRefresh = (uMsg == WM_USER_REFRESH_DIR_EX_DELAYED || uMsg == WM_USER_INACTREFRESH_DIR); // jde o odlozeny refresh, ktery muze byt take zbytecny (takhle se zamezi nekonecnemu cyklu pri cteni ikon na sitovem disku, ktere vyvolava dalsi refresh)
                                                                                                                       //          TRACE_I("WM_USER_REFRESH_DIR_EX_DELAYED or WM_USER_INACTREFRESH_DIR: probablyUselessRefresh=" << probablyUselessRefresh);
            }
        }
        if ((uMsg == WM_USER_REFRESH_DIR && wParam || // zmena obsahu hlasena snooperem
             uMsg == WM_USER_ICONREADING_END ||       // nebo hlaseni konce cteni ikonek (muze prijit pozde, uz se zase muzou cist ikonky)
             uMsg == WM_USER_INACTREFRESH_DIR) &&     // nebo odlozeny refresh v neaktivnim okne (jde o refresh vyzadany snooperem nebo pri ukonceni suspend modu)
            !IconCacheValid &&
            UseSystemIcons && Is(ptDisk) && GetNetworkDrive())
        {
            //        TRACE_I("Delaying refresh until all icons are read.");
            NeedRefreshAfterIconsReading = TRUE;
            RefreshAfterIconsReadingTime = max(RefreshAfterIconsReadingTime, (int)lParam);
            // hlaseni o zmene jsme poznamenali (refresh se postne az se dokonci cteni ikonek), koncime zpracovani
        }
        else
        {
            if (SnooperSuspended || StopRefresh)
            { // uz je zapnuty suspend mode (pracuje se nad vnitrnimi daty -> nelze je refreshnout)
                NeedRefreshAfterEndOfSM = TRUE;
                RefreshAfterEndOfSMTime = max(RefreshAfterEndOfSMTime, (int)lParam);
                if ((uMsg == WM_USER_S_REFRESH_DIR || uMsg == WM_USER_SM_END_NOTIFY_DELAYED) && setWait)
                {
                    SetCursor(oldCur);
                }
            }
            else // nejde o refresh v suspend modu
            {
                if (lParam >= LastRefreshTime) // nejde o zbytecny stary refresh
                {
                    BOOL isInactiveRefresh = FALSE;
                    BOOL skipRefresh = FALSE;
                    if ((uMsg == WM_USER_REFRESH_DIR && wParam ||     // zmena obsahu hlasena snooperem
                         uMsg == WM_USER_ICONREADING_END ||           // nebo hlaseni konce cteni ikonek (odlozeny refresh vyzadany snooperem + po ukonceni suspend modu)
                         uMsg == WM_USER_INACTREFRESH_DIR) &&         // nebo odlozeny refresh v neaktivnim okne (jde o refresh vyzadany snooperem nebo pri ukonceni suspend modu)
                        GetForegroundWindow() != MainWindow->HWindow) // neaktivni hlavni okno Salamandera: pribrzdime refreshe, je-li treba
                    {
                        //              TRACE_I("Refresh from snooper in inactive window");
                        isInactiveRefresh = TRUE;
                        if (LastInactiveRefreshStart != LastInactiveRefreshEnd) // nejaky refresh uz probehl od posledni deaktivace
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
                            if (InactiveRefreshTimerSet ||                 // timer uz bezi, jen na nej pockame
                                ti - LastInactiveRefreshEnd + 100 < delay) // +100 aby se timer nenahazoval "zbytecne" (at je odklad refreshe aspon o 100ms)
                            {
                                //                  TRACE_I("Delaying refresh");
                                if (!InactiveRefreshTimerSet) // timer jeste nebezi, zalozime ho
                                {
                                    //                    TRACE_I("Setting timer");
                                    if (SetTimer(HWindow, IDT_INACTIVEREFRESH, max(200, delay - (ti - LastInactiveRefreshEnd)), NULL))
                                    {
                                        InactiveRefreshTimerSet = TRUE;
                                        InactRefreshLParam = lParam;
                                        skipRefresh = TRUE;
                                    }
                                }
                                else // timer uz bezi, jen na nej pockame
                                {
                                    //                    TRACE_I("Timer already set");
                                    if (lParam > InactRefreshLParam)
                                        InactRefreshLParam = lParam; // prevezmeme novejsi cas do InactRefreshLParam
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
                            setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // ceka uz ?
                            if (setWait)
                                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                        }
                        char pathBackup[MAX_PATH];
                        CPanelType typeBackup;
                        if (isInactiveRefresh)
                        {
                            lstrcpyn(pathBackup, GetPath(), MAX_PATH); // zajimaji nas jen diskove cesty a cesty do archivu (u plugin-FS nas snooper o zmenach neinformuje)
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
                            { // pokud doslo ke zmene cesty (nejspis nekdo prave smazal adresar zobrazeny v panelu), provedeme pripadny dalsi refresh bez cekani (da se ocekavat, ze smazou i adresar nove zobrazeny v panelu, tak abysme z nej umeli rychle "vycouvat")
                                LastInactiveRefreshEnd = LastInactiveRefreshStart;
                            }
                            else
                            {
                                LastInactiveRefreshEnd = GetTickCount();
                                if ((int)(LastInactiveRefreshEnd - LastInactiveRefreshStart) <= 0)
                                    LastInactiveRefreshEnd = LastInactiveRefreshStart + 1; // nesmi byt shodne (to je stav "zatim zadny refresh")
                            }
                        }
                        /*  // Petr: nevim proc bylo nastaveni LastRefreshTime az zde - logicky pokud dojde ke zmene behem refreshe, je nutne udelat dalsi refresh - sralo se to v Nethoodu, protoze enumeracni thread stihl postnout refresh jeste pred dokoncenim RefreshDirectory, tedy doslo k jeho vyignorovani (je to refresh behem refreshe)
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
        { // uz je zapnuty suspend mode (pracuje se nad vnitrnimi daty -> nelze je refreshnout)
            // navic muzeme byt i uvnitr plug-inu -> vicenasobne volani metod plug-inu nepodporujeme
            PluginFSNeedRefreshAfterEndOfSM = TRUE;
        }
        else
        { // nejsme uvnitr plug-inu
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
            // pokud slo o nacteni "staticke" ikony asociace, ulozime ji do Associations (pocita
            // i s thumbnaily - nedopadne podminka na Flag==1 nebo 2)
            if (file != NULL && !isDir &&                                   // jde o soubor
                (!Is(ptPluginFS) || GetPluginIconsType() != pitFromPlugin)) // nejde o ikonu z plug-inu
            {
                char buf[MAX_PATH + 4]; // pripona malymi pismeny
                char *s1 = buf, *s2 = file->Ext;
                while (*s2 != 0)
                    *s1++ = LowerCase[*s2++];
                *((DWORD*)s1) = 0;
                int index;
                CIconSizeEnum iconSize = IconCache->GetIconSize();
                if (Associations.GetIndex(buf, index) &&             // pripona ma ikonku (asociaci)
                    (Associations[index].GetIndex(iconSize) == -1 || // jde o ikonku, ktera se nacita
                     Associations[index].GetIndex(iconSize) == -3))
                {
                    int icon;
                    CIconList* srcIconList;
                    int srcIconListIndex;
                    memmove(buf, file->Name, file->NameLen);
                    *(DWORD*)(buf + file->NameLen) = 0;
                    if (IconCache->GetIndex(buf, icon, NULL, NULL) &&                                 // icon-thread ji nacita
                        (IconCache->At(icon).GetFlag() == 1 || IconCache->At(icon).GetFlag() == 2) && // ikona je nactena nova nebo stara
                        IconCache->GetIcon(IconCache->At(icon).GetIndex(),
                                           &srcIconList, &srcIconListIndex)) // povede se ziskat nactenou ikonku
                    {                                                        // ikonka pro priponu -> icon-thread uz ji nacetl
                        CIconList* dstIconList;
                        int dstIconListIndex;
                        int i = Associations.AllocIcon(&dstIconList, &dstIconListIndex, iconSize);
                        if (i != -1) // ziskali jsme misto pro novou ikonku
                        {            // nakopirujeme si ji z IconCache do Associations
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
                                // panely prekreslime pouze pokud odpovidaji velikosti ikon
                                if (iconSize == GetIconSizeForCurrentViewMode())
                                    RepaintIconOnly(-1); // u nas vsechny

                                CFilesWindow* otherPanel = MainWindow->GetOtherPanel(this);
                                if (iconSize == otherPanel->GetIconSizeForCurrentViewMode())
                                    otherPanel->RepaintIconOnly(-1); // a u sousedu vsechny
                            }
                            else
                                PostAllIconsRepaint = TRUE;
                        }
                    }
                }
            }
        }

        // provedeme prekresleni postizeneho indexu
        if (file != NULL) // file se zde pouziva jen pro test na NULL
        {
            if (!StopIconRepaint) // pokud je povoleno prekreslovani ikon
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
        // postprocessing provedeme jen u cest, ktere jsme ziskali jako text (a ne primo dropnutim adresare)
        char buff[2 * MAX_PATH];
        strcpy_s(buff, (char*)lParam);
        if (!(BOOL)wParam || PostProcessPathFromUser(HWindow, buff))
            ChangeDir(buff, -1, NULL, 3 /*change-dir*/, NULL, (BOOL)wParam);
        return 0;
    }

    case WM_USER_FOCUSFILE:
    {
        // Musime okno vytahnout uz tady, protoze behem volani ChangeDir muze dojit
        // k vyskoceni messageboxu (cesta neexistuje) a ten by zustal pod Findem.
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
            //        SetForegroundWindow(MainWindow->HWindow);  // tady uz je pozde - presunuto nahoru
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
        ViewFile(data->FileName, FALSE, (DWORD)lParam, data->EnumFileNamesSourceUID, // FIXME_X64 - overit pretypovani na (DWORD)
                 data->EnumFileNamesLastFileIndex);
        return 0;
    }

    case WM_USER_EDITFILEWITH:
    {
        EditFile((char*)wParam, (DWORD)lParam); // FIXME_X64 - overit pretypovani na (DWORD)
        return 0;
    }

        //    case WM_USER_RENAME_NEXT_ITEM:
        //    {
        //      int index = GetCaretIndex();
        //      QuickRenameOnIndex(index + (wParam ? 1 : -1));
        //      return 0;
        //    }

    case WM_USER_DONEXTFOCUS: // pokud to jiz nestihl RefreshDirectory, udelame to tady
    {
        DontClearNextFocusName = FALSE;
        if (NextFocusName[0] != 0) // je-li co fokusit
        {
            int total = Files->Count + Dirs->Count;
            int found = -1;
            int i;
            for (i = 0; i < total; i++)
            {
                CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                if (StrICmp(f->Name, NextFocusName) == 0)
                {
                    if (strcmp(f->Name, NextFocusName) == 0) // soubor nalezen presne
                    {
                        NextFocusName[0] = 0;
                        SetCaretIndex(i, FALSE);
                        break;
                    }
                    if (found == -1)
                        found = i; // soubor nalezen (ignore-case)
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
                        displaySize = FALSE; // soubor nezname velikosti nebo adresar bez zname/vypocitane velikosti
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
                            varPlacementsCount = 100; // mohlo se poskodit
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
                    varPlacementsCount = 100; // mohlo se poskodit
                }
            }
            else
                TRACE_E("Unexpected situation in CFilesWindow::WindowProc(WM_USER_SELCHANGED)");
        }

        if (count == 0)
        {
            LastFocus = INT_MAX;
            int index = GetCaretIndex();
            ItemFocused(index); // pri odznaceni
        }
        IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
        return 0;
    }

    case WM_CREATE:
    {
        //---  pridani tohoto panelu do pole zdroju pro enumeraci souboru ve viewerech
        EnumFileNamesAddSourceUID(HWindow, &EnumFileNamesSourceUID);

        //---  vytvoreni listboxu se soubory a adresari
        ListBox = new CFilesBox(this);
        if (ListBox == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        //---  vytvoreni statusliny s informacemi o akt. souboru
        StatusLine = new CStatusWindow(this, blBottom, ooStatic);
        if (StatusLine == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        ToggleStatusLine();
        //---  vytvoreni statusliny s informacemi o akt. adresari
        DirectoryLine = new CStatusWindow(this, blTop, ooStatic);
        if (DirectoryLine == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        DirectoryLine->SetLeftPanel(MainWindow->LeftPanel == this);
        ToggleDirectoryLine();
        //---  nahozeni typu viewu + nacteni obsahu adresare
        SetThumbnailSize(Configuration.ThumbnailSize); // musi existovat ListBox
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

        // srovname nastaveni promenne AutomaticRefresh a directory-liny
        SetAutomaticRefresh(AutomaticRefresh, TRUE);

        return 0;
    }

    case WM_DESTROY:
    {
        //---  zruseni tohoto panelu z pole zdroju pro enumeraci souboru ve viewerech
        EnumFileNamesRemoveSourceUID(HWindow);

        CancelUI(); // cancel QuickSearch and QuickEdit
        LastRefreshTime = INT_MAX;
        BeginStopRefresh();
        DetachDirectory(this);
        //---  uvolneni child-oken
        RevokeDragDrop();
        ListBox->DetachWindow();
        delete ListBox;
        ListBox = NULL; // pro jistotu, at se chyby ukazou...

        StatusLine->DestroyWindow();
        delete StatusLine;
        StatusLine = NULL;

        DirectoryLine->DestroyWindow();
        delete DirectoryLine;
        DirectoryLine = NULL; // oprava padacky
                              //---
        return 0;
    }

    case WM_USER_ENUMFILENAMES: // hledani dalsiho/predchoziho jmena pro viewer
    {
        HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));

        if (InactiveRefreshTimerSet) // pokud je zde odlozeny refresh, musime jej provest ihned, jinak budeme enumerovat nad neaktualnim listingem; pokud bude dele trvat nevadi, GetFileNameForViewer si na vysledek pocka...
        {
            //        TRACE_I("Refreshing during enumeration (refresh in inactive window was delayed)");
            KillTimer(HWindow, IDT_INACTIVEREFRESH);
            InactiveRefreshTimerSet = FALSE;
            LastInactiveRefreshEnd = LastInactiveRefreshStart;
            SendMessage(HWindow, WM_USER_INACTREFRESH_DIR, FALSE, InactRefreshLParam);
        }

        if ((int)wParam /* reqUID */ == FileNamesEnumData.RequestUID && // nedoslo k zadani dalsiho pozadaku (tento by pak byl k nicemu)
            EnumFileNamesSourceUID == FileNamesEnumData.SrcUID &&       // nedoslo ke zmene zdroje
            !FileNamesEnumData.TimedOut)                                // na vysledek jeste nekdo ceka
        {
            if (Files != NULL && Is(ptDisk))
            {
                BOOL selExists = FALSE;
                if (FileNamesEnumData.PreferSelected) // je-li to treba, zjistime jestli existuje selectiona
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
                if (index == -1) // hledame od prvniho nebo od posledniho
                {
                    if (FileNamesEnumData.RequestType == fnertFindPrevious)
                        index = count; // hledame predchozi + mame zacit na poslednim
                                       // else  // hledame nasledujici + mame zacit na prvnim
                }
                else
                {
                    if (FileNamesEnumData.LastFileName[0] != 0) // zname plne jmeno souboru na 'index', zkontrolujeme jestli nedoslo k rozesunuti/sesunuti pole + pripadne dohledame novy index
                    {
                        int pathLen = (int)strlen(GetPath());
                        if (StrNICmp(GetPath(), FileNamesEnumData.LastFileName, pathLen) == 0)
                        { // cesta k souboru se musi shodovat s cestou v panelu ("always true")
                            const char* name = FileNamesEnumData.LastFileName + pathLen;
                            if (*name == '\\' || *name == '/')
                                name++;

                            CFileData* f = (index >= 0 && index < count) ? &Files->At(index) : NULL;
                            BOOL nameIsSame = f != NULL && StrICmp(name, f->Name) == 0;
                            if (nameIsSame)
                                indexNotFound = FALSE;
                            if (f == NULL || !nameIsSame)
                            { // jmeno na indexu 'index' neni FileNamesEnumData.LastFileName, zkusime najit novy index tohoto jmena
                                int i;
                                for (i = 0; i < count && StrICmp(name, Files->At(i).Name) != 0; i++)
                                    ;
                                if (i != count) // novy index nalezen
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
                if (FileNamesEnumData.OnlyAssociatedExtensions) // preje si viewer filtrovani podle asociovanych pripon?
                {
                    if (FileNamesEnumData.Plugin != NULL) // viewer z pluginu
                    {
                        int pluginIndex = Plugins.GetIndex(FileNamesEnumData.Plugin);
                        if (pluginIndex != -1) // "always true"
                        {
                            wantedViewerType = -1 - pluginIndex;
                            onlyAssociatedExtensions = TRUE;
                        }
                    }
                    else // interni viewer
                    {
                        wantedViewerType = VIEWER_INTERNAL;
                        onlyAssociatedExtensions = TRUE;
                    }
                }

                BOOL preferSelected = selExists && FileNamesEnumData.PreferSelected;
                switch (FileNamesEnumData.RequestType)
                {
                case fnertFindNext: // dalsi
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

                case fnertFindPrevious: // predchozi
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

                case fnertIsSelected: // zjisteni oznaceni
                {
                    if (!indexNotFound && index >= 0 && index < Files->Count)
                    {
                        FileNamesEnumData.IsFileSelected = Files->At(index).Selected;
                        FileNamesEnumData.Found = TRUE;
                    }
                    break;
                }

                case fnertSetSelection: // nastaveni oznaceni
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

    BeginStopRefresh(); // cmuchal si da pohov

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

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

void CFilesWindow::OpenStopFilterMenu()
{
    CALL_STACK_MESSAGE1("CFilesWindow::OpenStopFilterMenu()");

    BeginStopRefresh(); // cmuchal si da pohov

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

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertItem() dole...
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

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

// na zaklade dostupnych sloupcu naplni popup
BOOL CFilesWindow::FillSortByMenu(CMenuPopup* popup)
{
    CALL_STACK_MESSAGE1("CFilesWindow::FillSortByMenu()");

    // sestrelime existujici polozky
    popup->RemoveAllItems();

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertItem() dole...
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

    // docasne reseni pro 1.6 beta 6: naleju vzdy (bez ohledu na ValidFileData)
    // polozky Name, Ext, Date, Size
    // poradi musi korespondovat s CSortType enumem
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
            // vycisteni icon-cache
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
    //if (ListBox != NULL)  // toto se nastavi z volani SetFont()
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
