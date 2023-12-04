// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

int GlobalShowLogUID = -1;      // UID logu, ktery ma FTPCMD_SHOWLOGS zobrazit (-1 == zadny)
int GlobalDisconnectPanel = -1; // panel, pro ktery se vola disconnect (-1 == aktivni panel - source)

//
// ****************************************************************************
// CPluginInterfaceForMenuExt
//

DWORD
CPluginInterfaceForMenuExt::GetMenuItemState(int id, DWORD eventMask)
{
    switch (id)
    {
    case FTPCMD_TRMODEAUTO:
    case FTPCMD_TRMODEASCII:
    case FTPCMD_TRMODEBINARY:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
        {
            return ((CPluginFSInterface*)fs)->GetTransferModeCmdState(id);
        }
        return 0; // disabled
    }

    case FTPCMD_DISCONNECT:
    {
        return MENU_ITEM_STATE_HIDDEN | ((eventMask & MENU_EVENT_THIS_PLUGIN_FS) ? MENU_ITEM_STATE_ENABLED : 0);
    }

    case FTPCMD_LISTHIDDENFILES:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
        {
            return MENU_ITEM_STATE_ENABLED |
                   (((CPluginFSInterface*)fs)->IsListCommandLIST_a() ? MENU_ITEM_STATE_CHECKED : 0);
        }
        return 0; // disabled
    }

    case FTPCMD_SHOWCERT:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
            return ((CPluginFSInterface*)fs)->IsFTPS() ? MENU_ITEM_STATE_ENABLED : 0;
        return 0; // disabled
    }

    default:
    {
        TRACE_E("Unexpected call to CPluginInterfaceForMenuExt::GetMenuItemState(). ID=" << id);
        return 0; // disabled
    }
    }
}

BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                                 int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::ExecuteMenuItem(, , %d, 0x%X)", id, eventMask);
    switch (id)
    {
    case FTPCMD_CONNECTFTPSERVER:
    {
        ConnectFTPServer(parent, PANEL_SOURCE);
        return FALSE; // neodznacovat
    }

    case FTPCMD_ORGANIZEBOOKMARKS:
    {
        OrganizeBookmarks(parent);
        return FALSE; // neodznacovat
    }

    case FTPCMD_DISCONNECT_F12:
    {
        SalamanderGeneral->PostSalamanderCommand(SALCMD_DISCONNECT);
        return FALSE; // neodznacovat
    }

    case FTPCMD_DISCONNECT:
    {
        Config.DisconnectCommandUsed = TRUE;
        int panel = GlobalDisconnectPanel != -1 ? GlobalDisconnectPanel : PANEL_SOURCE;
        SalamanderGeneral->DisconnectFSFromPanel(parent, panel);
        Config.DisconnectCommandUsed = FALSE;
        GlobalDisconnectPanel = -1;
        return FALSE; // neodznacovat
    }

    case FTPCMD_ADDBOOKMARK:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
        {
            ((CPluginFSInterface*)fs)->AddBookmark(parent);
        }
        return FALSE; // neodznacovat
    }

    case FTPCMD_REFRESHPATH:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
        {
            SalamanderGeneral->PostRefreshPanelFS(fs, FALSE);
        }
        return FALSE; // neodznacovat
    }

    case FTPCMD_SHOWLOGS:
    case FTPCMD_SHOWLOGSLEFT:
    case FTPCMD_SHOWLOGSRIGHT:
    {
        if (id != FTPCMD_SHOWLOGS || GlobalShowLogUID == -1) // pokud neni jine, vezmeme aspon aktivni panel, at userovi trochu pomuzeme
        {
            CPluginFSInterface* fs = (CPluginFSInterface*)(SalamanderGeneral->GetPanelPluginFS(id == FTPCMD_SHOWLOGSLEFT ? PANEL_LEFT : id == FTPCMD_SHOWLOGSRIGHT ? PANEL_RIGHT
                                                                                                                                                                   : PANEL_SOURCE));
            if (fs != NULL)
            {
                GlobalShowLogUID = fs->GetLogUID();
                if (GlobalShowLogUID == -1 || !Logs.HasLogWithUID(GlobalShowLogUID))
                    GlobalShowLogUID = -1;
            }
        }
        Logs.ActivateLogsDlg(GlobalShowLogUID);
        GlobalShowLogUID = -1;
        return FALSE; // neodznacovat
    }

    case FTPCMD_CLOSECONNOTIF: // postnuty pomocny prikaz ('salamander' a 'eventMask' jsou prazdne)
    {                          // zkontroluje jestli uz se uzivatel dozvedel o zavreni "control connection"
        ClosedCtrlConChecker.Check(parent);
        return FALSE; // ignoruje se
    }

    case FTPCMD_SENDFTPCOMMAND:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
        {
            ((CPluginFSInterface*)fs)->SendUserFTPCommand(SalamanderGeneral->GetMsgBoxParent());
        }
        return FALSE; // neodznacovat
    }

    case FTPCMD_CHANGETGTPANELPATH:
    {
        if (TargetPanelPath[0] != 0)
        {
            SalamanderGeneral->ChangePanelPath(TargetPanelPathPanel, TargetPanelPath);
            TargetPanelPath[0] = 0;
        }
        return FALSE; // neodznacovat
    }

    case FTPCMD_SHOWRAWLISTING:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
        {
            ((CPluginFSInterface*)fs)->ShowRawListing(SalamanderGeneral->GetMsgBoxParent());
        }
        return FALSE; // neodznacovat
    }

    case FTPCMD_LISTHIDDENFILES:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
        {
            ((CPluginFSInterface*)fs)->ToggleListHiddenFiles(parent);
            SalamanderGeneral->PostRefreshPanelFS(fs, FALSE);
        }
        return FALSE; // neodznacovat
    }

    case FTPCMD_TRMODEAUTO:
    case FTPCMD_TRMODEASCII:
    case FTPCMD_TRMODEBINARY:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
        {
            ((CPluginFSInterface*)fs)->SetTransferModeByMenuCmd2(id);
        }
        return FALSE; // neodznacovat
    }

    case FTPCMD_SHOWCERT:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // jde o nas FS, jinak by byl NULL
            ((CPluginFSInterface*)fs)->ShowSecurityInfo(parent);
        return FALSE; // neodznacovat
    }

    case FTPCMD_CANCELOPERATION: // postnuty pomocny prikaz ('salamander' a 'eventMask' jsou prazdne)
    {                            // uvolni objekt FTP operace
        int uid;
        if (CanceledOperations.GetFirstUID(uid))
        {
            HANDLE dlgThread;                                         // zavreme dialog operace
            if (FTPOperationsList.CloseOperationDlg(uid, &dlgThread)) // je 'uid' platne?
            {
                if (dlgThread != NULL)
                {
                    CALL_STACK_MESSAGE1("AuxThreadQueue.WaitForExit()");
                    AuxThreadQueue.WaitForExit(dlgThread, INFINITE); // pockame na dokonceni threadu dialogu operace
                }
                FTPOperationsList.DeleteOperation(uid, FALSE); // smazeme operaci
            }
        }
        return FALSE; // ignoruje se
    }

    case FTPCMD_RETURNCONNECTION:
    {
        int controlConUID;
        CFTPWorker* workerWithCon;
        while (ReturningConnections.GetFirstCon(&controlConUID, &workerWithCon))
        { // pro vsechny workery vracejici spojeni do "control connection" FS v panelu (mozne i do detached FS)
            int i;
            for (i = 0; i < FTPConnections.Count; i++) // zkusime najit FS vyuzivajici "control connection" s UID 'controlConUID' (FS uz ale muze byt zavreny)
            {
                CPluginFSInterface* fs = FTPConnections[i];
                if (fs->ContainsConWithUID(controlConUID)) // nasli jsme FS s hledanou "control connection"
                {
                    fs->GetConnectionFromWorker(workerWithCon); // nechame prevzit spojeni od workera
                    break;
                }
            }
            workerWithCon->ForceClose(); // tvrde zavreme socket (sice by na close socketu nic nemelo cekat (stacilo by volat CloseSocket()), ale sychrujeme se - SocketClosed se hned po pridani do ReturningConnections dava na TRUE)
            if (workerWithCon->CanDeleteFromRetCons())
                DeleteSocket(workerWithCon);
        }
        return FALSE; // ignoruje se
    }

    case FTPCMD_REFRESHLEFTPANEL:
    case FTPCMD_REFRESHRIGHTPANEL:
    {
        SalamanderGeneral->RefreshPanelPath(id == FTPCMD_REFRESHLEFTPANEL ? PANEL_LEFT : PANEL_RIGHT);
        return FALSE; // ignoruje se
    }

    case FTPCMD_ACTIVWELCOMEMSG:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_TARGET);
        if (fs != NULL)                                      // jde o nas FS, jinak by byl NULL
            ((CPluginFSInterface*)fs)->ActivateWelcomeMsg(); // pokud nejaky msg-box deaktivoval okno welcome-msg, aktivujeme ho zpet
        fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL)                                      // jde o nas FS, jinak by byl NULL
            ((CPluginFSInterface*)fs)->ActivateWelcomeMsg(); // pokud nejaky msg-box deaktivoval okno welcome-msg, aktivujeme ho zpet
        return FALSE;                                        // ignoruje se
    }
    }
    TRACE_E("Unknown command with ID=" << id);
    return FALSE;
}

BOOL WINAPI
CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case FTPCMD_CONNECTFTPSERVER:
        helpID = IDH_CONNECTFTPSERVER;
        break;
    case FTPCMD_ORGANIZEBOOKMARKS:
        helpID = IDH_ORGANIZEBOOKMARKS;
        break;
    case FTPCMD_SHOWLOGS:
        helpID = IDH_SHOWLOGS;
        break;
    case FTPCMD_DISCONNECT_F12:
        helpID = IDH_DISCONNECT_F12;
        break;

    case FTPCMD_TRMODESUBMENU:
    case FTPCMD_TRMODEAUTO:
    case FTPCMD_TRMODEASCII:
    case FTPCMD_TRMODEBINARY:
        helpID = IDH_TRMODESUBMENU;
        break;

    case FTPCMD_REFRESHPATH:
        helpID = IDH_REFRESHPATH;
        break;
    case FTPCMD_ADDBOOKMARK:
        helpID = IDH_ADDBOOKMARK;
        break;
    case FTPCMD_SENDFTPCOMMAND:
        helpID = IDH_SENDFTPCOMMAND;
        break;
    case FTPCMD_SHOWRAWLISTING:
        helpID = IDH_SHOWRAWLISTING;
        break;
    case FTPCMD_LISTHIDDENFILES:
        helpID = IDH_LISTHIDDENFILES;
        break;
    case FTPCMD_SHOWCERT:
        helpID = IDH_SHOWCERT;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}
