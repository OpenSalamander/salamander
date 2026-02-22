// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

// FS-name assigned by Salamander after loading the plug-in
char AssignedFSName[MAX_PATH] = "";
int AssignedFSNameLen = 0;

// FS-name for FTP over SSL (FTPS) assigned by Salamander after loading the plugin
char AssignedFSNameFTPS[MAX_PATH] = "";
int AssignedFSNameIndexFTPS = -1;
int AssignedFSNameLenFTPS = 0;

HICON FTPIcon = NULL;        // icon (16x16) of FTP
HICON FTPLogIcon = NULL;     // icon (16x16) of the FTP Logs dialog
HICON FTPLogIconBig = NULL;  // large (32x32) icon of the FTP Logs dialog
HICON FTPOperIcon = NULL;    // icon (16x16) of the operations dialog
HICON FTPOperIconBig = NULL; // large (32x32) icon of the operations dialog
HCURSOR DragCursor = NULL;   // cursor for the drag & drop listbox in the Connect dialog
HFONT FixedFont = NULL;      // font for the Welcome Message dialog (fixed so the text layout works better)
HFONT SystemFont = NULL;     // environment font (dialogs, wait window, etc.)
HICON WarningIcon = NULL;    // small (16x16) "warning" icon for the operations dialog

const char* SAVEBITS_CLASSNAME = "SalamanderFTPClientSaveBits"; // class for CWaitWindow

ATOM AtomObject2 = 0; // atom for CSetWaitCursorWindow

CThreadQueue AuxThreadQueue("FTP Aux"); // queue of all auxiliary threads

CRITICAL_SECTION IncListingCounterSect; // critical section for IncListingCounter()
DWORD IncListingCounterCounter = 1;     // counter for IncListingCounter()

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

BOOL InitFS()
{
#ifdef _DEBUG // must be 4 bytes because it is stored in LastWrite into the upper and lower DWORD
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

    AtomObject2 = GlobalAddAtom("object handle2"); // atom for CSetWaitCursorWindow
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
        { // thread did not start, error
            delete FTPDiskThread;
            FTPDiskThread = NULL;
            return FALSE;
        }
    }
    else // not enough memory, error
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
    // cancel workers that were supposed to hand over the connection to the panel but have not made it yet
    ReturningConnections.CloseData();

    // close operation dialogs (they should already be closed, this is just in case of an error)
    FTPOperationsList.CloseAllOperationDlgs();

    // terminate operation workers (they should already be finished, this is just in case of an error)
    FTPOperationsList.StopWorkers(SalamanderGeneral->GetMsgBoxParent(),
                                  -1 /* all operations */,
                                  -1 /* all workers */);

    if (!UnregisterClass(SAVEBITS_CLASSNAME, DLLInstance))
        TRACE_E("UnregisterClass(SAVEBITS_CLASSNAME) has failed");

    // close all modeless dialogs (Welcome Message)
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

    // close the Logs dialog
    Logs.CloseLogsDlg();

    // close the disk thread (we wait without a limit but allow the user to press ESC)
    if (FTPDiskThread != NULL)
    {
        HANDLE t = FTPDiskThread->GetHandle();
        FTPDiskThread->Terminate();
        GetAsyncKeyState(VK_ESCAPE); // initialize GetAsyncKeyState - see help
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
                MSG msg; // discard the buffered ESC
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

    // kill auxiliary threads that did not finish "legally"
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
    // DragCursor is "shared", so it will be destroyed only when the plugin DLL is unloaded
    if (FixedFont != NULL)
        HANDLES(DeleteObject(FixedFont));
    // SystemFont does not need to be deleted (stock object)

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
        if (!found) // "i == FTPConnections.Count" is not enough (there is Delete(i))
            TRACE_E("Unexpected situation in CPluginInterfaceForFS::CloseFS(): FS not found in FTPConnections.");

        delete ((CPluginFSInterface*)fs); // to call the correct destructor
    }
}

void CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(int panel)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", panel);
    ConnectFTPServer(SalamanderGeneral->GetMsgBoxParent(), panel);
}

void ConnectFTPServer(HWND parent, int panel) // called in Alt+F1/F2 and in Drive bars and from the plugin menu
{
    CConnectDlg dlg(parent);
    if (dlg.IsGood())
    {
        Config.QuickConnectServer.Release(); // clear + set quick-connect before opening the dialog
        while (1)
        {
            if (dlg.Execute() == IDOK) // connect
            {
                // let the main window redraw (so the user does not stare at the stale background during the entire connect dialog)
                UpdateWindow(SalamanderGeneral->GetMainWindowHWND());

                // determine whether the selected server from the dialog is FTP or FTPS
                BOOL isFTPS = FALSE;
                if (Config.LastBookmark == 0)
                    isFTPS = Config.QuickConnectServer.EncryptControlConnection == 1;
                else
                {
                    if (Config.LastBookmark - 1 >= 0 && Config.LastBookmark - 1 < Config.FTPServerList.Count)
                        isFTPS = Config.FTPServerList[Config.LastBookmark - 1]->EncryptControlConnection == 1;
                }

                // change the path in the current panel to the selected FTP server (according to the bookmark)
                Config.UseConnectionDataFromConfig = TRUE; // the next path change will use data from the configuration
                Config.ChangingPathInInactivePanel = panel != PANEL_SOURCE && SalamanderGeneral->GetSourcePanel() != panel;
                int failReason;
                if (!SalamanderGeneral->ChangePanelPathToPluginFS(panel, isFTPS ? AssignedFSNameFTPS : AssignedFSName,
                                                                  "", &failReason))
                { // on success it returns failReason == CHPPFR_SHORTERPATH (user part of the path is not "")
                    Config.UseConnectionDataFromConfig = FALSE;
                    if (failReason == CHPPFR_INVALIDPATH ||   // inaccessible path (cannot log in or list anything)
                        failReason == CHPPFR_CANNOTCLOSEPATH) // cannot close the path in the panel (cannot open a new path)
                    {
                        if (failReason == CHPPFR_CANNOTCLOSEPATH && Config.LastBookmark != 0)
                            break; // only for quick-connect (to save data, otherwise it makes no sense)
                        continue;  // repeat the input (so the user does not lose quick-connect data)
                    }
                }
                Config.UseConnectionDataFromConfig = FALSE;
                break; // success
            }
            else
                break; // cancel/close
        }
        Config.QuickConnectServer.Release(); // these data will certainly no longer be needed (they are set even after Cancel in the dialog)
    }
}

void OrganizeBookmarks(HWND parent) // called in Alt+F1/F2 and in Drive bars and from the plugin menu
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
    if (pluginFS == NULL) // menu for the FS item (FTP Client)
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
                    // find and set the check marks in the "Transfer Mode" submenu (we look for it as the first submenu)
                    ((CPluginFSInterface*)pluginFS)->SetTransferModeCheckMarksInSubMenu(subMenu, 0);
                    ((CPluginFSInterface*)pluginFS)->SetListHiddenFilesCheckInMenu(subMenu);
                    ((CPluginFSInterface*)pluginFS)->SetShowCertStateInMenu(subMenu);
                }
                DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                             x, y, parent, NULL);
                switch (cmd)
                {
                case CM_OPENDETACHED: // detached FS: open...
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
                        postCmdParam = (void*)pluginFS; // detached FS
                    else
                        postCmdParam = NULL; // active FS
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_SHOWLOG;
                    ret = TRUE;
                    break;
                }

                case CM_DISCONNECTSRV:
                {
                    if (isDetachedFS)
                        postCmdParam = (void*)pluginFS; // detached FS
                    else
                        postCmdParam = NULL; // active FS
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
                        postCmdParam = (void*)pluginFS; // command is only for the active FS
                    ret = TRUE;
                    break;
                }

                case CM_SHOWRAWLISTING:
                {
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_SHOWRAWLISTING;
                    if (!isDetachedFS)
                        postCmdParam = (void*)pluginFS; // command is only for the active FS
                    ret = TRUE;
                    break;
                }

                case CM_LISTHIDDENFILES:
                {
                    closeMenu = TRUE;
                    postCmd = CHNGDRV_LISTHIDDENFILES;
                    if (!isDetachedFS)
                        postCmdParam = (void*)pluginFS; // command is only for the active FS
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
    case CHNGDRV_CONNECTFTPSERVER: // menu item for FS (FTP Client): Connect to...
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
        SalamanderGeneral->PostMenuExtCommand(leftOrRightPanel == PANEL_LEFT ? FTPCMD_SHOWLOGSLEFT : FTPCMD_SHOWLOGSRIGHT, TRUE); // will run later in "sal-idle"
        break;
    }

    case CHNGDRV_DISCONNECT: // active or detached FS: Disconnect...
    {
        if (postCmdParam != NULL) // detached FS
        {
            SalamanderGeneral->CloseDetachedFS(SalamanderGeneral->GetMsgBoxParent(),
                                               (CPluginFSInterfaceAbstract*)postCmdParam);
        }
        else
            SalamanderGeneral->PostMenuExtCommand(FTPCMD_DISCONNECT, TRUE); // active FS, will run later in "sal-idle"
        break;
    }

    case CHNGDRV_OPENDETACHED: // detached FS: Open...
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
            else                                                              // open the Logs window with the selected log GlobalShowLogUID
                SalamanderGeneral->PostMenuExtCommand(FTPCMD_SHOWLOGS, TRUE); // will run later in "sal-idle"
        }
        else
            TRACE_E("Unexpected situation in CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(): choosen FS not found!");
        break;
    }

    case CHNGDRV_ADDBOOKMARK: // active/detached FS: Add Bookmark...
    {
        ((CPluginFSInterface*)postCmdParam)->AddBookmark(SalamanderGeneral->GetMsgBoxParent());
        break;
    }

    case CHNGDRV_SENDFTPCOMMAND: // active FS: Send FTP Command...
    {
        if (postCmdParam != 0)
            ((CPluginFSInterface*)postCmdParam)->SendUserFTPCommand(SalamanderGeneral->GetMsgBoxParent());
        break;
    }

    case CHNGDRV_SHOWRAWLISTING: // active FS: Show Raw Listing...
    {
        if (postCmdParam != 0)
            ((CPluginFSInterface*)postCmdParam)->ShowRawListing(SalamanderGeneral->GetMsgBoxParent());
        break;
    }

    case CHNGDRV_LISTHIDDENFILES: // active FS: List Hidden Files (Unix)
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
    if (isDir || file.IsLink) // subdirectory or up-dir or link (it can target a file or directory - we currently prefer this test to see if it is a directory)
    {
        char newUserPart[FTP_USERPART_SIZE];
        char newPath[FTP_MAX_PATH];
        char cutDir[FTP_MAX_PATH];
        lstrcpyn(newPath, fs->Path, FTP_MAX_PATH);
        CFTPServerPathType type = fs->GetFTPServerPathType(newPath);
        if (isDir == 2) // up-dir
        {
            if (FTPCutDirectory(type, newPath, FTP_MAX_PATH, cutDir, FTP_MAX_PATH, NULL)) // shorten the path by the last component
            {
                int topIndex; // next top-index, -1 -> invalid
                if (!fs->TopIndexMem.FindAndPop(type, newPath, topIndex))
                    topIndex = -1;
                // change the path in the panel
                fs->MakeUserPart(newUserPart, FTP_USERPART_SIZE, newPath);
                SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, newUserPart, NULL,
                                                             topIndex, cutDir);
            }
        }
        else // subdirectory
        {
            // backup of data for TopIndexMem (backupPath + topIndex)
            char backupPath[FTP_MAX_PATH];
            strcpy(backupPath, newPath);
            int topIndex = SalamanderGeneral->GetPanelTopIndex(panel);
            if (FTPPathAppend(type, newPath, FTP_MAX_PATH, file.Name, TRUE)) // set the path
            {
                // change the path in the panel
                fs->MakeUserPart(newUserPart, FTP_USERPART_SIZE, newPath);
                if (SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, newUserPart))
                {
                    fs->TopIndexMem.Push(type, backupPath, topIndex); // remember the top-index for the return
                }
            }
        }
    }
    else // file
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
    ((CPluginFSInterface*)pluginFS)->CalledFromDisconnectDialog = TRUE; // suppress unnecessary prompts (the user issued the disconnect command, we just perform it)
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
        ((CPluginFSInterface*)pluginFS)->CalledFromDisconnectDialog = FALSE; // disable suppression of unnecessary prompts
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
    // determine whether path follows Path (path == Path+"/name")
    char testPath[FTP_MAX_PATH];
    lstrcpyn(testPath, path, FTP_MAX_PATH);
    BOOL ok = FALSE;
    if (FTPCutDirectory(type, testPath, FTP_MAX_PATH, NULL, 0, NULL))
    {
        ok = FTPIsTheSameServerPath(type, testPath, Path);
    }

    if (ok) // it follows -> remember the next top-index
    {
        if (TopIndexesCount == TOP_INDEX_MEM_SIZE) // we need to discard the first top-index from memory
        {
            int i;
            for (i = 0; i < TOP_INDEX_MEM_SIZE - 1; i++)
                TopIndexes[i] = TopIndexes[i + 1];
            TopIndexesCount--;
        }
        strcpy(Path, path);
        TopIndexes[TopIndexesCount++] = topIndex;
    }
    else // it does not follow -> first top-index in sequence
    {
        strcpy(Path, path);
        TopIndexesCount = 1;
        TopIndexes[0] = topIndex;
    }
}

BOOL CTopIndexMem::FindAndPop(CFTPServerPathType type, const char* path, int& topIndex)
{
    // determine whether path matches Path (path == Path)
    if (FTPIsTheSameServerPath(type, path, Path))
    {
        if (TopIndexesCount > 0)
        {
            if (!FTPCutDirectory(type, Path, FTP_MAX_PATH, NULL, 0, NULL))
                Path[0] = 0;
            topIndex = TopIndexes[--TopIndexesCount];
            return TRUE;
        }
        else // we no longer have this value (it was not stored or low memory removed it)
        {
            Clear();
            return FALSE;
        }
    }
    else // query for another path -> clear the memory, a long jump occurred
    {
        Clear();
        return FALSE;
    }
}
