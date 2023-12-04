// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// FS-name pridelene Salamanderem po loadu plug-inu
char AssignedFSName[MAX_PATH] = "";
int AssignedFSNameLen = 0;

// FS-name pro FTP over SSL (FTPS) pridelene Salamanderem po loadu pluginu
char AssignedFSNameFTPS[MAX_PATH] = "";
int AssignedFSNameIndexFTPS = -1;
int AssignedFSNameLenFTPS = 0;

HICON FTPIcon = NULL;        // ikona (16x16) FTP
HICON FTPLogIcon = NULL;     // ikona (16x16) dialogu FTP Logs
HICON FTPLogIconBig = NULL;  // velka (32x32) ikona dialogu FTP Logs
HICON FTPOperIcon = NULL;    // ikona (16x16) dialogu operace
HICON FTPOperIconBig = NULL; // velka (32x32) ikona dialogu operace
HCURSOR DragCursor = NULL;   // kurzor pro drag&drop listbox v Connect dialogu
HFONT FixedFont = NULL;      // font pro Welcome Message dialog (fixed, aby lepe fungovalo usporadani textu)
HFONT SystemFont = NULL;     // font prostredi (dialogy, wait-window, atd.)
HICON WarningIcon = NULL;    // mala (16x16) ikona "warning" do dialogu operace

const char* SAVEBITS_CLASSNAME = "SalamanderFTPClientSaveBits"; // trida pro CWaitWindow

ATOM AtomObject2 = 0; // atom pro CSetWaitCursorWindow

CThreadQueue AuxThreadQueue("FTP Aux"); // fronta vsech pomocnych threadu

CRITICAL_SECTION IncListingCounterSect; // krit. sekce IncListingCounter()
DWORD IncListingCounterCounter = 1;     // counter pro IncListingCounter()

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

BOOL InitFS()
{
#ifdef _DEBUG // musi byt 4 byty, protoze se uklada do LastWrite do horniho a spodniho DWORDu
    if (sizeof(CFTPTime) != 4 || sizeof(CFTPDate) != 4)
        TRACE_E("FATAL ERROR: sizeof(CFTPTime) or sizeof(CFTPDate) is not 4 bytes!");
#endif

    HANDLES(InitializeCriticalSection(&IncListingCounterSect));
    HANDLES(InitializeCriticalSection(&PanelCtrlConSect));
    HANDLES(InitializeCriticalSection(&CFTPQueueItem::NextItemUIDCritSect));
    HANDLES(InitializeCriticalSection(&CFTPOperation::NextOrdinalNumberCS));
    WorkerMayBeClosedEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL)); // manual, nonsignaled
    if (WorkerMayBeClosedEvent == NULL)
    {
        TRACE_E("Nepodarilo se vytvorit event WorkerMayBeClosedEvent.");
        return FALSE;
    }
    HANDLES(InitializeCriticalSection(&WorkerMayBeClosedStateCS));

    if (!InitializeWinLib("FTP_Client", DLLInstance))
        return FALSE;
    SetupWinLibHelp(HTMLHelpCallback);
    SetWinLibStrings(LoadStr(IDS_INVALIDNUMBER), LoadStr(IDS_FTPPLUGINTITLE));

    AtomObject2 = GlobalAddAtom("object handle2"); // atom pro CSetWaitCursorWindow
    if (AtomObject2 == 0)
    {
        TRACE_E("GlobalAddAtom has failed");
        return FALSE;
    }

    FTPIcon = (HICON)HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDC_FTPICON), IMAGE_ICON,
                                       16, 16, SalamanderGeneral->GetIconLRFlags()));
    FTPLogIcon = (HICON)HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDC_FTPLOGICON), IMAGE_ICON,
                                          16, 16, SalamanderGeneral->GetIconLRFlags()));
    FTPLogIconBig = (HICON)HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDC_FTPLOGICON), IMAGE_ICON,
                                             32, 32, SalamanderGeneral->GetIconLRFlags()));
    FTPOperIcon = (HICON)HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDC_FTPOPERICON), IMAGE_ICON,
                                           16, 16, SalamanderGeneral->GetIconLRFlags()));
    FTPOperIconBig = (HICON)HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDC_FTPOPERICON), IMAGE_ICON,
                                              32, 32, SalamanderGeneral->GetIconLRFlags()));
    WarningIcon = (HICON)HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDC_WARNINGICON), IMAGE_ICON,
                                           16, 16, SalamanderGeneral->GetIconLRFlags()));

    DragCursor = LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_DRAGCURSOR));

    LOGFONT srcLF;
    SystemFont = (HFONT)HANDLES(GetStockObject(DEFAULT_GUI_FONT));
    GetObject(SystemFont, sizeof(srcLF), &srcLF);

    LOGFONT lf;
    lf.lfHeight = srcLF.lfHeight;
    lf.lfWidth = 0;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    lf.lfWeight = FW_NORMAL;
    lf.lfItalic = 0;
    lf.lfUnderline = 0;
    lf.lfStrikeOut = 0;
    lf.lfCharSet = SalamanderGeneral->GetUserDefaultCharset();
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strcpy(lf.lfFaceName, "Consolas");
    FixedFont = HANDLES(CreateFontIndirect(&lf));

    if (!CWaitWindow::RegisterUniversalClass(CS_DBLCLKS | CS_SAVEBITS,
                                             0,
                                             0,
                                             DLLInstance,
                                             NULL,
                                             LoadCursor(NULL, IDC_ARROW),
                                             (HBRUSH)(COLOR_3DFACE + 1),
                                             NULL,
                                             SAVEBITS_CLASSNAME,
                                             NULL))
    {
        TRACE_E("Nepodarilo se registrovat tridu SAVEBITS_CLASSNAME.");
        return FALSE;
    }

    FTPDiskThread = new CFTPDiskThread();
    if (FTPDiskThread != NULL && FTPDiskThread->IsGood())
    {
        if (FTPDiskThread->Create(AuxThreadQueue) == NULL)
        { // thread se nepustil, error
            delete FTPDiskThread;
            FTPDiskThread = NULL;
            return FALSE;
        }
    }
    else // malo pameti, error
    {
        if (FTPDiskThread != NULL)
        {
            delete FTPDiskThread;
            FTPDiskThread = NULL;
        }
        else
            TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    return TRUE;
}

void ReleaseFS()
{
    // zrusime workery, kteri meli predat connectionu do panelu ale zatim to nestihli
    ReturningConnections.CloseData();

    // zavreme dialogy operaci (uz by mely byt zavrene, zde jen pro pripad chyby)
    FTPOperationsList.CloseAllOperationDlgs();

    // ukoncime workery operaci (uz by mely byt ukoncene, zde jen pro pripad chyby)
    FTPOperationsList.StopWorkers(SalamanderGeneral->GetMsgBoxParent(),
                                  -1 /* vsechny operace */,
                                  -1 /* vsechny workery */);

    if (!UnregisterClass(SAVEBITS_CLASSNAME, DLLInstance))
        TRACE_E("UnregisterClass(SAVEBITS_CLASSNAME) has failed");

    // zavreme vsechny nemodalni dialogy (Welcome Message)
    int count = ModelessDlgs.Count;
    while (ModelessDlgs.Count > 0)
    {
        DestroyWindow(ModelessDlgs[ModelessDlgs.Count - 1]->HWindow);
        if (count == ModelessDlgs.Count)
        {
            TRACE_E("Unexpected situation in ReleaseFS().");
            delete ModelessDlgs[ModelessDlgs.Count - 1];
            ModelessDlgs.Delete(ModelessDlgs.Count - 1);
            if (!ModelessDlgs.IsGood())
                ModelessDlgs.ResetState();
        }
        count = ModelessDlgs.Count;
    }

    // zavreme Logs dialog
    Logs.CloseLogsDlg();

    // zavreme disk thread (cekame bez limitu, ale umoznime to userovi ESCapnout)
    if (FTPDiskThread != NULL)
    {
        HANDLE t = FTPDiskThread->GetHandle();
        FTPDiskThread->Terminate();
        GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help
        HWND waitWndParent = SalamanderGeneral->GetMsgBoxParent();
        SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_CLOSINGDISKTHREAD), LoadStr(IDS_FTPPLUGINTITLE),
                                                2000, TRUE, waitWndParent);
        while (1)
        {
            if (AuxThreadQueue.WaitForExit(t, 100))
                break;
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == waitWndParent ||
                SalamanderGeneral->GetSafeWaitWindowClosePressed())
            {
                MSG msg; // vyhodime nabufferovany ESC
                while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                    ;

                SalamanderGeneral->ShowSafeWaitWindow(FALSE);
                if (SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                                     LoadStr(IDS_CANCELDISKTHREAD),
                                                     LoadStr(IDS_FTPPLUGINTITLE),
                                                     MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                         MB_ICONQUESTION) == IDYES)
                {
                    TRACE_I("FTP Disk Thread was terminated (it probably executes some long disk operation).");
                    break;
                }
                SalamanderGeneral->ShowSafeWaitWindow(TRUE);
            }
        }
        SalamanderGeneral->DestroySafeWaitWindow();
    }

    // pomocne thready, ktere nedobehly "legalne", pozabijime
    AuxThreadQueue.KillAll(TRUE, 0, 0);

    if (FTPIcon != NULL)
        HANDLES(DestroyIcon(FTPIcon));
    if (FTPLogIcon != NULL)
        HANDLES(DestroyIcon(FTPLogIcon));
    if (FTPLogIconBig != NULL)
        HANDLES(DestroyIcon(FTPLogIconBig));
    if (FTPOperIcon != NULL)
        HANDLES(DestroyIcon(FTPOperIcon));
    if (FTPOperIconBig != NULL)
        HANDLES(DestroyIcon(FTPOperIconBig));
    if (WarningIcon != NULL)
        HANDLES(DestroyIcon(WarningIcon));
    // DragCursor je "shared", takze ho zrusi az unload DLL pluginu
    if (FixedFont != NULL)
        HANDLES(DeleteObject(FixedFont));
    // SystemFont neni treba mazat (stock object)

    ReleaseWinLib(DLLInstance);

    if (WorkerMayBeClosedEvent != NULL)
        HANDLES(CloseHandle(WorkerMayBeClosedEvent));
    HANDLES(DeleteCriticalSection(&WorkerMayBeClosedStateCS));
    HANDLES(DeleteCriticalSection(&CFTPOperation::NextOrdinalNumberCS));
    HANDLES(DeleteCriticalSection(&CFTPQueueItem::NextItemUIDCritSect));
    HANDLES(DeleteCriticalSection(&PanelCtrlConSect));
    HANDLES(DeleteCriticalSection(&IncListingCounterSect));

    if (AtomObject2 != 0)
        GlobalDeleteAtom(AtomObject2);

#ifdef _DEBUG
    TRACE_I("CUploadListingsOnServer::FindPath(): cached searches / total searches: " << CUploadListingsOnServer::FoundPathIndexesInCache << " / " << CUploadListingsOnServer::FoundPathIndexesTotal);
#endif
}

DWORD IncListingCounter()
{
    HANDLES(EnterCriticalSection(&IncListingCounterSect));
    DWORD ret = IncListingCounterCounter++;
    HANDLES(LeaveCriticalSection(&IncListingCounterSect));
    return ret;
}

//
// ****************************************************************************
// CPluginInterfaceForFS
//

CPluginFSInterfaceAbstract*
CPluginInterfaceForFS::OpenFS(const char* fsName, int fsNameIndex)
{
    CPluginFSInterface* fs = new CPluginFSInterface;
    if (fs != NULL)
    {
        FTPConnections.Add(fs);
        if (!FTPConnections.IsGood())
        {
            FTPConnections.ResetState();
            delete fs;
            fs = NULL;
        }
        else
            ActiveFSCount++;
    }
    else
        TRACE_E(LOW_MEMORY);
    return fs;
}

void CPluginInterfaceForFS::CloseFS(CPluginFSInterfaceAbstract* fs)
{
    if (fs != NULL)
    {
        ActiveFSCount--;
        BOOL found = FALSE;
        int i;
        for (i = 0; i < FTPConnections.Count; i++)
        {
            if (FTPConnections[i] == fs)
            {
                FTPConnections.Delete(i);
                if (!FTPConnections.IsGood())
                    FTPConnections.ResetState();
                found = TRUE;
                break;
            }
        }
        if (!found) // "i == FTPConnections.Count" nestaci (je tam Delete(i))
            TRACE_E("Unexpected situation in CPluginInterfaceForFS::CloseFS(): FS not found in FTPConnections.");

        delete ((CPluginFSInterface*)fs); // aby se volal spravny destruktor
    }
}

void CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(int panel)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", panel);
    ConnectFTPServer(SalamanderGeneral->GetMsgBoxParent(), panel);
}

void ConnectFTPServer(HWND parent, int panel) // vola se v Alt+F1/F2 a v Drive barach a z menu pluginu
{
    CConnectDlg dlg(parent);
    if (dlg.IsGood())
    {
        Config.QuickConnectServer.Release(); // vycistime+nastavime quick-connect pred otevrenim dialogu
        while (1)
        {
            if (dlg.Execute() == IDOK) // connect
            {
                // nechame prekreslit hlavni okno (aby user cely connect nekoukal na zbytek po connect dialogu)
                UpdateWindow(SalamanderGeneral->GetMainWindowHWND());

                // zjistime, jestli je vybrany server z dialogu FTP nebo FTPS
                BOOL isFTPS = FALSE;
                if (Config.LastBookmark == 0)
                    isFTPS = Config.QuickConnectServer.EncryptControlConnection == 1;
                else
                {
                    if (Config.LastBookmark - 1 >= 0 && Config.LastBookmark - 1 < Config.FTPServerList.Count)
                        isFTPS = Config.FTPServerList[Config.LastBookmark - 1]->EncryptControlConnection == 1;
                }

                // zmenime cestu v aktualnim panelu na zadany FTP server (dle bookmarky)
                Config.UseConnectionDataFromConfig = TRUE; // nasledujici zmena cesty pouzije data z konfigurace
                Config.ChangingPathInInactivePanel = panel != PANEL_SOURCE && SalamanderGeneral->GetSourcePanel() != panel;
                int failReason;
                if (!SalamanderGeneral->ChangePanelPathToPluginFS(panel, isFTPS ? AssignedFSNameFTPS : AssignedFSName,
                                                                  "", &failReason))
                { // pri uspechu vraci failReason==CHPPFR_SHORTERPATH (user-part cesty neni "")
                    Config.UseConnectionDataFromConfig = FALSE;
                    if (failReason == CHPPFR_INVALIDPATH ||   // nepristupna cesta (neda se zalogovat nebo se neda nic vylistovat)
                        failReason == CHPPFR_CANNOTCLOSEPATH) // nelze zavrit cestu v panelu (nelze otevrit novou cestu)
                    {
                        if (failReason == CHPPFR_CANNOTCLOSEPATH && Config.LastBookmark != 0)
                            break; // jen u quick-connectu (pro ulozeni dat, jinak nema smysl)
                        continue;  // zopakujeme si zadani (aby user neprisel o connect-data u quick-connectu)
                    }
                }
                Config.UseConnectionDataFromConfig = FALSE;
                break; // uspech
            }
            else
                break; // cancel/close
        }
        Config.QuickConnectServer.Release(); // ted jiz tato data jiste nebudou potreba (nastavena jsou i po Cancel v dialogu)
    }
}

void OrganizeBookmarks(HWND parent) // vola se v Alt+F1/F2 a v Drive barach a z menu pluginu
{
    CConnectDlg dlg(parent, 1);
    if (dlg.IsGood())
        dlg.Execute();
}

BOOL CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                           CPluginFSInterfaceAbstract* pluginFS,
                                                           const char* pluginFSName, int pluginFSNameIndex,
                                                           BOOL isDetachedFS, BOOL& refreshMenu,
                                                           BOOL& closeMenu, int& postCmd, void*& postCmdParam)
{
    CALL_STACK_MESSAGE7("CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(, %d, %d, %d, , %s, %d, %d, , , ,)",
                        panel, x, y, pluginFSName, pluginFSNameIndex, isDetachedFS);

    refreshMenu = FALSE;
    closeMenu = FALSE;
    postCmd = 0;
    postCmdParam = NULL;

    BOOL ret = FALSE;
    if (pluginFS == NULL) // menu pro polozku FS (FTP Client)
    {
        HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_CHNGDRVCONTMENU));
        if (main != NULL)
        {
            HMENU subMenu = GetSubMenu(main, 0);
            if (subMenu != NULL)
            {
                DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                             x, y, parent, NULL);
                switch (cmd)
                {
                case CM_CONNECTSRV:
                {
                    ret = TRUE;
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_CONNECTFTPSERVER;
                    break;
                }

                case CM_ORGBOOKMARKS:
                {
                    ret = TRUE;
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_ORGANIZEBOOKMARKS;
                    break;
                }

                case CM_SHOWLOGS:
                {
                    ret = TRUE;
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_SHOWLOGS;
                    break;
                }
                }
            }
            DestroyMenu(main);
        }
    }
    else
    {
        HMENU main = LoadMenu(HLanguage,
                              MAKEINTRESOURCE(isDetachedFS ? IDM_DETPATHCONTEXTMENU : IDM_ACTPATHCONTEXTMENU));
        if (main != NULL)
        {
            HMENU subMenu = GetSubMenu(main, 0);
            if (subMenu != NULL)
            {
                if (!isDetachedFS)
                {
                    // najdeme a nastavime "check-marky" do submenu "Tranfer Mode" (hledame ho jako prvni submenu)
                    ((CPluginFSInterface*)pluginFS)->SetTransferModeCheckMarksInSubMenu(subMenu, 0);
                    ((CPluginFSInterface*)pluginFS)->SetListHiddenFilesCheckInMenu(subMenu);
                    ((CPluginFSInterface*)pluginFS)->SetShowCertStateInMenu(subMenu);
                }
                DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                             x, y, parent, NULL);
                switch (cmd)
                {
                case CM_OPENDETACHED: // odpojeny FS: open...
                {
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_OPENDETACHED;
                    postCmdParam = (void*)pluginFS;
                    ret = TRUE;
                    break;
                }

                case CM_SHOWLOG:
                {
                    if (isDetachedFS)
                        postCmdParam = (void*)pluginFS; // odpojeny FS
                    else
                        postCmdParam = NULL; // aktivni FS
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_SHOWLOG;
                    ret = TRUE;
                    break;
                }

                case CM_DISCONNECTSRV:
                {
                    if (isDetachedFS)
                        postCmdParam = (void*)pluginFS; // odpojeny FS
                    else
                        postCmdParam = NULL; // aktivni FS
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_DISCONNECT;
                    ret = TRUE;
                    break;
                }

                case CM_REFRESHPATH:
                {
                    SalamanderGeneral->PostRefreshPanelFS(pluginFS, FALSE);
                    closeMenu = TRUE;
                    ret = TRUE;
                    break;
                }

                case CM_ADDBOOKMARK:
                {
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_ADDBOOKMARK;
                    postCmdParam = (void*)pluginFS;
                    ret = TRUE;
                    break;
                }

                case CM_SENDFTPCOMMAND:
                {
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_SENDFTPCOMMAND;
                    if (!isDetachedFS)
                        postCmdParam = (void*)pluginFS; // prikaz je jen pro aktivni FS
                    ret = TRUE;
                    break;
                }

                case CM_SHOWRAWLISTING:
                {
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_SHOWRAWLISTING;
                    if (!isDetachedFS)
                        postCmdParam = (void*)pluginFS; // prikaz je jen pro aktivni FS
                    ret = TRUE;
                    break;
                }

                case CM_LISTHIDDENFILES:
                {
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_LISTHIDDENFILES;
                    if (!isDetachedFS)
                        postCmdParam = (void*)pluginFS; // prikaz je jen pro aktivni FS
                    ret = TRUE;
                    break;
                }

                case CM_TRMODEAUTO:
                case CM_TRMODEASCII:
                case CM_TRMODEBINARY:
                    ((CPluginFSInterface*)pluginFS)->SetTransferModeByMenuCmd(cmd);
                    break;

                case CM_SHOWCERT:
                {
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_SHOWCERT;
                    postCmdParam = (void*)pluginFS;
                    ret = TRUE;
                    break;
                }
                }
            }
            DestroyMenu(main);
        }
    }
    return ret;
}

void CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(%d, %d,)", panel, postCmd);

    switch (postCmd)
    {
    case CHNGDRV_CONNECTFTPSERVER: // polozka pro FS (FTP Client): Connect to...
    {
        ExecuteChangeDriveMenuItem(panel);
        break;
    }

    case CHNGDRV_ORGANIZEBOOKMARKS:
    {
        OrganizeBookmarks(SalamanderGeneral->GetMsgBoxParent());
        break;
    }

    case CHNGDRV_SHOWLOGS:
    {
        int leftOrRightPanel = panel == PANEL_SOURCE ? SalamanderGeneral->GetSourcePanel() : panel;
        SalamanderGeneral->PostMenuExtCommand(leftOrRightPanel == PANEL_LEFT ? FTPCMD_SHOWLOGSLEFT : FTPCMD_SHOWLOGSRIGHT, TRUE); // spusti se az v "sal-idle"
        break;
    }

    case CHNGDRV_DISCONNECT: // aktivni nebo odpojeny FS: Disconnect...
    {
        if (postCmdParam != NULL) // odpojeny FS
        {
            SalamanderGeneral->CloseDetachedFS(SalamanderGeneral->GetMsgBoxParent(),
                                               (CPluginFSInterfaceAbstract*)postCmdParam);
        }
        else
            SalamanderGeneral->PostMenuExtCommand(FTPCMD_DISCONNECT, TRUE); // aktivni FS, spusti se az v "sal-idle"
        break;
    }

    case CHNGDRV_OPENDETACHED: // odpojeny FS: Open...
    {
        CPluginFSInterfaceAbstract* fs = (CPluginFSInterfaceAbstract*)postCmdParam;
        SalamanderGeneral->ChangePanelPathToDetachedFS(PANEL_SOURCE, fs, NULL);
        break;
    }

    case CHNGDRV_SHOWLOG:
    {
        CPluginFSInterface* fs = (CPluginFSInterface*)postCmdParam;
        if (fs == NULL)
            fs = (CPluginFSInterface*)(SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE));
        if (fs != NULL)
        {
            GlobalShowLogUID = fs->GetLogUID();
            if (GlobalShowLogUID == -1 || !Logs.HasLogWithUID(GlobalShowLogUID))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_FSHAVENOLOG),
                                                  LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
            }
            else                                                              // otevreme okno Logs s vybranym logem GlobalShowLogUID
                SalamanderGeneral->PostMenuExtCommand(FTPCMD_SHOWLOGS, TRUE); // spusti se az v "sal-idle"
        }
        else
            TRACE_E("Unexpected situation in CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(): choosen FS not found!");
        break;
    }

    case CHNGDRV_ADDBOOKMARK: // aktivni/odpojeny FS: Add Bookmark...
    {
        ((CPluginFSInterface*)postCmdParam)->AddBookmark(SalamanderGeneral->GetMsgBoxParent());
        break;
    }

    case CHNGDRV_SENDFTPCOMMAND: // aktivni FS: Send FTP Command...
    {
        if (postCmdParam != 0)
            ((CPluginFSInterface*)postCmdParam)->SendUserFTPCommand(SalamanderGeneral->GetMsgBoxParent());
        break;
    }

    case CHNGDRV_SHOWRAWLISTING: // aktivni FS: Show Raw Listing...
    {
        if (postCmdParam != 0)
            ((CPluginFSInterface*)postCmdParam)->ShowRawListing(SalamanderGeneral->GetMsgBoxParent());
        break;
    }

    case CHNGDRV_LISTHIDDENFILES: // aktivni FS: List Hidden Files (Unix)
    {
        if (postCmdParam != 0)
        {
            ((CPluginFSInterface*)postCmdParam)->ToggleListHiddenFiles(SalamanderGeneral->GetMsgBoxParent());
            SalamanderGeneral->PostRefreshPanelFS((CPluginFSInterface*)postCmdParam, FALSE);
        }
        break;
    }

    case CHNGDRV_SHOWCERT:
    {
        ((CPluginFSInterface*)postCmdParam)->ShowSecurityInfo(SalamanderGeneral->GetMsgBoxParent());
        break;
    }
    }
}

void CPluginInterfaceForFS::ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                        const char* pluginFSName, int pluginFSNameIndex,
                                        CFileData& file, int isDir)
{
    CPluginFSInterface* fs = (CPluginFSInterface*)pluginFS;
    if (isDir || file.IsLink) // podadresar nebo up-dir nebo link (muze byt na soubor i adresar - zatim preferujeme tento test, jestli jde o adresar)
    {
        char newUserPart[FTP_USERPART_SIZE];
        char newPath[FTP_MAX_PATH];
        char cutDir[FTP_MAX_PATH];
        lstrcpyn(newPath, fs->Path, FTP_MAX_PATH);
        CFTPServerPathType type = fs->GetFTPServerPathType(newPath);
        if (isDir == 2) // up-dir
        {
            if (FTPCutDirectory(type, newPath, FTP_MAX_PATH, cutDir, FTP_MAX_PATH, NULL)) // zkratime cestu o posl. komponentu
            {
                int topIndex; // pristi top-index, -1 -> neplatny
                if (!fs->TopIndexMem.FindAndPop(type, newPath, topIndex))
                    topIndex = -1;
                // zmenime cestu v panelu
                fs->MakeUserPart(newUserPart, FTP_USERPART_SIZE, newPath);
                SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, newUserPart, NULL,
                                                             topIndex, cutDir);
            }
        }
        else // podadresar
        {
            // zaloha udaju pro TopIndexMem (backupPath + topIndex)
            char backupPath[FTP_MAX_PATH];
            strcpy(backupPath, newPath);
            int topIndex = SalamanderGeneral->GetPanelTopIndex(panel);
            if (FTPPathAppend(type, newPath, FTP_MAX_PATH, file.Name, TRUE)) // nastavime cestu
            {
                // zmenime cestu v panelu
                fs->MakeUserPart(newUserPart, FTP_USERPART_SIZE, newPath);
                if (SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, newUserPart))
                {
                    fs->TopIndexMem.Push(type, backupPath, topIndex); // zapamatujeme top-index pro navrat
                }
            }
        }
    }
    else // soubor
    {
    }
}

BOOL WINAPI
CPluginInterfaceForFS::DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                    CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForFS::DisconnectFS(, %d, %d, , %s, %d)",
                        isInPanel, panel, pluginFSName, pluginFSNameIndex);
    ((CPluginFSInterface*)pluginFS)->CalledFromDisconnectDialog = TRUE; // potlacime zbytecne dotazy (user dal prikaz pro disconnect, jen ho provedeme)
    BOOL ret = FALSE;
    if (isInPanel)
    {
        SalamanderGeneral->DisconnectFSFromPanel(parent, panel);
        ret = SalamanderGeneral->GetPanelPluginFS(panel) != pluginFS;
    }
    else
    {
        ret = SalamanderGeneral->CloseDetachedFS(parent, pluginFS);
    }
    if (!ret)
        ((CPluginFSInterface*)pluginFS)->CalledFromDisconnectDialog = FALSE; // vypneme potlaceni zbytecnych dotazu
    return ret;
}

void CPluginInterfaceForFS::ConvertPathToInternal(const char* fsName, int fsNameIndex,
                                                  char* fsUserPart)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForFS::ConvertPathToInternal(%s, %d, %s)",
                        fsName, fsNameIndex, fsUserPart);
    FTPConvertHexEscapeSequences(fsUserPart);
}

void CPluginInterfaceForFS::ConvertPathToExternal(const char* fsName, int fsNameIndex,
                                                  char* fsUserPart)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForFS::ConvertPathToExternal(%s, %d, %s)",
                        fsName, fsNameIndex, fsUserPart);
    FTPAddHexEscapeSequences(fsUserPart, MAX_PATH);
}

//****************************************************************************
//
// CTopIndexMem
//

void CTopIndexMem::Push(CFTPServerPathType type, const char* path, int topIndex)
{
    // zjistime, jestli path navazuje na Path (path==Path+"/jmeno")
    char testPath[FTP_MAX_PATH];
    lstrcpyn(testPath, path, FTP_MAX_PATH);
    BOOL ok = FALSE;
    if (FTPCutDirectory(type, testPath, FTP_MAX_PATH, NULL, 0, NULL))
    {
        ok = FTPIsTheSameServerPath(type, testPath, Path);
    }

    if (ok) // navazuje -> zapamatujeme si dalsi top-index
    {
        if (TopIndexesCount == TOP_INDEX_MEM_SIZE) // je potreba vyhodit z pameti prvni top-index
        {
            int i;
            for (i = 0; i < TOP_INDEX_MEM_SIZE - 1; i++)
                TopIndexes[i] = TopIndexes[i + 1];
            TopIndexesCount--;
        }
        strcpy(Path, path);
        TopIndexes[TopIndexesCount++] = topIndex;
    }
    else // nenavazuje -> prvni top-index v rade
    {
        strcpy(Path, path);
        TopIndexesCount = 1;
        TopIndexes[0] = topIndex;
    }
}

BOOL CTopIndexMem::FindAndPop(CFTPServerPathType type, const char* path, int& topIndex)
{
    // zjistime, jestli path odpovida Path (path==Path)
    if (FTPIsTheSameServerPath(type, path, Path))
    {
        if (TopIndexesCount > 0)
        {
            if (!FTPCutDirectory(type, Path, FTP_MAX_PATH, NULL, 0, NULL))
                Path[0] = 0;
            topIndex = TopIndexes[--TopIndexesCount];
            return TRUE;
        }
        else // tuto hodnotu jiz nemame (nebyla ulozena nebo mala pamet->byla vyhozena)
        {
            Clear();
            return FALSE;
        }
    }
    else // dotaz na jinou cestu -> vycistime pamet, doslo k dlouhemu skoku
    {
        Clear();
        return FALSE;
    }
}
