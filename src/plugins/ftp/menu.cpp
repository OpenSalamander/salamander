// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

int GlobalShowLogUID = -1;      // UID of the log FTPCMD_SHOWLOGS should display (-1 == none)
int GlobalDisconnectPanel = -1; // panel for which disconnect is called (-1 == active panel - source)

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
        if (fs != NULL) // it is our FS; otherwise it would be NULL
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
        if (fs != NULL) // it is our FS; otherwise it would be NULL
        {
            return MENU_ITEM_STATE_ENABLED |
                   (((CPluginFSInterface*)fs)->IsListCommandLIST_a() ? MENU_ITEM_STATE_CHECKED : 0);
        }
        return 0; // disabled
    }

    case FTPCMD_SHOWCERT:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // it is our FS; otherwise it would be NULL
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
        return FALSE; // do not uncheck
    }

    case FTPCMD_ORGANIZEBOOKMARKS:
    {
        OrganizeBookmarks(parent);
        return FALSE; // do not uncheck
    }

    case FTPCMD_DISCONNECT_F12:
    {
        SalamanderGeneral->PostSalamanderCommand(SALCMD_DISCONNECT);
        return FALSE; // do not uncheck
    }

    case FTPCMD_DISCONNECT:
    {
        Config.DisconnectCommandUsed = TRUE;
        int panel = GlobalDisconnectPanel != -1 ? GlobalDisconnectPanel : PANEL_SOURCE;
        SalamanderGeneral->DisconnectFSFromPanel(parent, panel);
        Config.DisconnectCommandUsed = FALSE;
        GlobalDisconnectPanel = -1;
        return FALSE; // do not uncheck
    }

    case FTPCMD_ADDBOOKMARK:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // it is our FS; otherwise it would be NULL
        {
            ((CPluginFSInterface*)fs)->AddBookmark(parent);
        }
        return FALSE; // do not uncheck
    }

    case FTPCMD_REFRESHPATH:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // it is our FS; otherwise it would be NULL
        {
            SalamanderGeneral->PostRefreshPanelFS(fs, FALSE);
        }
        return FALSE; // do not uncheck
    }

    case FTPCMD_SHOWLOGS:
    case FTPCMD_SHOWLOGSLEFT:
    case FTPCMD_SHOWLOGSRIGHT:
    {
        if (id != FTPCMD_SHOWLOGS || GlobalShowLogUID == -1) // if there is no other one, use at least the active panel to help the user a little
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
        return FALSE; // do not uncheck
    }

    case FTPCMD_CLOSECONNOTIF: // posted helper command ('salamander' and 'eventMask' are empty)
    {                          // checks whether the user has already learned about the closing of the "control connection"
        ClosedCtrlConChecker.Check(parent);
        return FALSE; // ignored
    }

    case FTPCMD_SENDFTPCOMMAND:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // it is our FS; otherwise it would be NULL
        {
            ((CPluginFSInterface*)fs)->SendUserFTPCommand(SalamanderGeneral->GetMsgBoxParent());
        }
        return FALSE; // do not uncheck
    }

    case FTPCMD_CHANGETGTPANELPATH:
    {
        if (TargetPanelPath[0] != 0)
        {
            SalamanderGeneral->ChangePanelPath(TargetPanelPathPanel, TargetPanelPath);
            TargetPanelPath[0] = 0;
        }
        return FALSE; // do not uncheck
    }

    case FTPCMD_SHOWRAWLISTING:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // it is our FS; otherwise it would be NULL
        {
            ((CPluginFSInterface*)fs)->ShowRawListing(SalamanderGeneral->GetMsgBoxParent());
        }
        return FALSE; // do not uncheck
    }

    case FTPCMD_LISTHIDDENFILES:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // it is our FS; otherwise it would be NULL
        {
            ((CPluginFSInterface*)fs)->ToggleListHiddenFiles(parent);
            SalamanderGeneral->PostRefreshPanelFS(fs, FALSE);
        }
        return FALSE; // do not uncheck
    }

    case FTPCMD_TRMODEAUTO:
    case FTPCMD_TRMODEASCII:
    case FTPCMD_TRMODEBINARY:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // it is our FS; otherwise it would be NULL
        {
            ((CPluginFSInterface*)fs)->SetTransferModeByMenuCmd2(id);
        }
        return FALSE; // do not uncheck
    }

    case FTPCMD_SHOWCERT:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL) // it is our FS; otherwise it would be NULL
            ((CPluginFSInterface*)fs)->ShowSecurityInfo(parent);
        return FALSE; // do not uncheck
    }

    case FTPCMD_CANCELOPERATION: // posted helper command ('salamander' and 'eventMask' are empty)
    {                            // releases the FTP operation object
        int uid;
        if (CanceledOperations.GetFirstUID(uid))
        {
            HANDLE dlgThread;                                         // close the operation dialog
            if (FTPOperationsList.CloseOperationDlg(uid, &dlgThread)) // is 'uid' valid?
            {
                if (dlgThread != NULL)
                {
                    CALL_STACK_MESSAGE1("AuxThreadQueue.WaitForExit()");
                    AuxThreadQueue.WaitForExit(dlgThread, INFINITE); // wait for the operation dialog thread to finish
                }
                FTPOperationsList.DeleteOperation(uid, FALSE); // delete the operation
            }
        }
        return FALSE; // ignored
    }

    case FTPCMD_RETURNCONNECTION:
    {
        int controlConUID;
        CFTPWorker* workerWithCon;
        while (ReturningConnections.GetFirstCon(&controlConUID, &workerWithCon))
        { // for all workers returning the connection to the "control connection" FS in the panel (possibly also to detached FS)
            int i;
            for (i = 0; i < FTPConnections.Count; i++) // try to find the FS using the "control connection" with UID 'controlConUID' (the FS might already be closed)
            {
                CPluginFSInterface* fs = FTPConnections[i];
                if (fs->ContainsConWithUID(controlConUID)) // found the FS with the requested "control connection"
                {
                    fs->GetConnectionFromWorker(workerWithCon); // let it take over the connection from the worker
                    break;
                }
            }
            workerWithCon->ForceClose(); // force-close the socket (nothing should be waiting on close socket; CloseSocket() would be enough, but we play it safe - SocketClosed is set to TRUE immediately after adding to ReturningConnections)
            if (workerWithCon->CanDeleteFromRetCons())
                DeleteSocket(workerWithCon);
        }
        return FALSE; // ignored
    }

    case FTPCMD_REFRESHLEFTPANEL:
    case FTPCMD_REFRESHRIGHTPANEL:
    {
        SalamanderGeneral->RefreshPanelPath(id == FTPCMD_REFRESHLEFTPANEL ? PANEL_LEFT : PANEL_RIGHT);
        return FALSE; // ignored
    }

    case FTPCMD_ACTIVWELCOMEMSG:
    {
        CPluginFSInterfaceAbstract* fs = SalamanderGeneral->GetPanelPluginFS(PANEL_TARGET);
        if (fs != NULL)                                      // it is our FS; otherwise it would be NULL
            ((CPluginFSInterface*)fs)->ActivateWelcomeMsg(); // if some message box deactivated the welcome-msg window, activate it again
        fs = SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL)                                      // it is our FS; otherwise it would be NULL
            ((CPluginFSInterface*)fs)->ActivateWelcomeMsg(); // if some message box deactivated the welcome-msg window, activate it again
        return FALSE;                                        // ignored
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
