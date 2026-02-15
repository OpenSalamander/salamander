// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CConfigPageServers
//

CConfigPageServers::CConfigPageServers() : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGSERVERTYPES, IDD_CFGSERVERTYPES, PSP_HASHELP, NULL)
{
    TmpServerTypeList = NULL;
}

CConfigPageServers::~CConfigPageServers()
{
    if (TmpServerTypeList != NULL)
        delete TmpServerTypeList;
}

void CConfigPageServers::Transfer(CTransferInfo& ti)
{
    HWND listbox;
    if (ti.GetControl(listbox, IDL_SUPPORTEDSERVERS))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(listbox, WM_SETREDRAW, FALSE, 0);
            SendMessage(listbox, LB_RESETCONTENT, 0, 0);
            if (TmpServerTypeList != NULL)
                TmpServerTypeList->AddNamesToListbox(listbox);
            SendMessage(listbox, LB_SETCURSEL, 0, 0);
            SendMessage(listbox, WM_SETREDRAW, TRUE, 0);
        }
        else
        {
            // transfer the data back into the configuration
            if (TmpServerTypeList != NULL)
            {
                CServerTypeList* s = Config.LockServerTypeList();
                s->DestroyMembers();
                s->CopyItemsFrom(TmpServerTypeList);
                Config.UnlockServerTypeList();

                // refresh the panel listings -> so the new parsers are used
                CPluginFSInterface* fs1 = (CPluginFSInterface*)SalamanderGeneral->GetPanelPluginFS(PANEL_LEFT);
                if (fs1 != NULL)
                {
                    fs1->SetNextRefreshCanUseOldListing(TRUE);
                    SalamanderGeneral->PostRefreshPanelFS(fs1, FALSE);
                }
                CPluginFSInterface* fs2 = (CPluginFSInterface*)SalamanderGeneral->GetPanelPluginFS(PANEL_RIGHT);
                if (fs2 != NULL)
                {
                    fs2->SetNextRefreshCanUseOldListing(TRUE);
                    SalamanderGeneral->PostRefreshPanelFS(fs2, FALSE);
                }
            }
        }
    }
}

void CConfigPageServers::RefreshList(BOOL focusLast, int focusIndex)
{
    HWND listbox = GetDlgItem(HWindow, IDL_SUPPORTEDSERVERS);
    int focus;
    if (focusIndex != -1)
        focus = focusIndex;
    else
        focus = (int)SendMessage(listbox, LB_GETCURSEL, 0, 0);
    int topIndex = (int)SendMessage(listbox, LB_GETTOPINDEX, 0, 0);
    SendMessage(listbox, WM_SETREDRAW, FALSE, 0);
    SendMessage(listbox, LB_RESETCONTENT, 0, 0);
    if (TmpServerTypeList != NULL)
        TmpServerTypeList->AddNamesToListbox(listbox);
    int count = TmpServerTypeList != NULL ? TmpServerTypeList->Count : 0;
    if (focus >= count)
        focus = count - 1;
    if (focusLast)
        focus = count - 1;
    SendMessage(listbox, LB_SETTOPINDEX, topIndex, 0);
    SendMessage(listbox, LB_SETCURSEL, focus, 0);
    SendMessage(listbox, WM_SETREDRAW, TRUE, 0);
    PostMessage(HWindow, WM_COMMAND, MAKELONG(IDL_SUPPORTEDSERVERS, LBN_SELCHANGE), 0);
}

void CConfigPageServers::EnableControls()
{
    HWND list = GetDlgItem(HWindow, IDL_SUPPORTEDSERVERS);
    int caret = (int)SendMessage(list, LB_GETCURSEL, 0, 0);
    int count = TmpServerTypeList != NULL ? TmpServerTypeList->Count : 0;
    BOOL enableUp = FALSE;
    BOOL enableDown = FALSE;
    BOOL enableEdit = FALSE;
    if (caret != LB_ERR)
    {
        enableUp = caret > 0;
        enableDown = caret + 1 < count;
        enableEdit = count != 0;
    }
    HWND focus = GetFocus();
    if (!enableUp && focus == GetDlgItem(HWindow, IDB_MOVESERVERUP) ||
        !enableDown && focus == GetDlgItem(HWindow, IDB_MOVESERVERDOWN) ||
        !enableEdit && focus == GetDlgItem(HWindow, IDB_EDITSERVER))
    {
        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)list, TRUE);
    }
    EnableWindow(GetDlgItem(HWindow, IDB_MOVESERVERUP), enableUp);
    EnableWindow(GetDlgItem(HWindow, IDB_MOVESERVERDOWN), enableDown);
    EnableWindow(GetDlgItem(HWindow, IDB_EDITSERVER), enableEdit);
}

void CConfigPageServers::MoveItem(HWND list, int fromIndex, int toIndex)
{
    // 'TmpServerTypeList' is certainly not NULL; 'fromIndex' and 'toIndex' are definitely valid indices
    // swap the data in the array
    CServerType* s = TmpServerTypeList->At(fromIndex);
    TmpServerTypeList->Detach(fromIndex);
    if (TmpServerTypeList->IsGood())
    {
        TmpServerTypeList->Insert(toIndex, s);
        if (TmpServerTypeList->IsGood())
        {
            // swap the data in the listbox
            SendMessage(list, WM_SETREDRAW, FALSE, 0);
            int topIndex = (int)SendMessage(list, LB_GETTOPINDEX, 0, 0);
            SendMessage(list, LB_DELETESTRING, fromIndex, 0);
            char typeBuf[SERVERTYPE_MAX_SIZE + 101];
            SendMessage(list, LB_INSERTSTRING, toIndex,
                        (LPARAM)GetTypeNameForUser(s->TypeName, typeBuf, SERVERTYPE_MAX_SIZE + 101));
            SendMessage(list, LB_SETTOPINDEX, topIndex, 0);
            SendMessage(list, LB_SETCURSEL, toIndex, 0);
            SendMessage(list, WM_SETREDRAW, TRUE, 0);
        }
        else
        {
            SendMessage(list, LB_DELETESTRING, fromIndex, 0);
            TmpServerTypeList->ResetState();
            delete s; // it was left outside the array, destroy it
        }
        PostMessage(HWindow, WM_COMMAND, MAKELONG(IDL_SUPPORTEDSERVERS, LBN_SELCHANGE), 0);
    }
    else
        TmpServerTypeList->ResetState();
}

char ImpExpInitDir[MAX_PATH] = "";

void CConfigPageServers::OnExportServer(CServerType* serverType)
{
    if (ImpExpInitDir[0] == 0)
        GetMyDocumentsPath(ImpExpInitDir);
    char fileName[MAX_PATH];
    lstrcpyn(fileName, serverType->TypeName[0] == '*' ? serverType->TypeName + 1 : serverType->TypeName,
             MAX_PATH - 4);
    strcat(fileName, ".str");
    SalamanderGeneral->SalMakeValidFileNameComponent(fileName);

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = HWindow;
    char* s = LoadStr(IDS_SRVTYPEFILEFILTER);
    ofn.lpstrFilter = s;
    while (*s != 0) // create a double-null-terminated list
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = ImpExpInitDir;
    ofn.lpstrDefExt = "str";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = LoadStr(IDS_SRVTYPESAVEASTITLE);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT |
                OFN_NOTESTFILECREATE | OFN_HIDEREADONLY;

    char buf[200 + MAX_PATH];
    if (SalamanderGeneral->SafeGetSaveFileName(&ofn))
    {
        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

        s = strrchr(fileName, '\\');
        if (s != NULL)
        {
            memcpy(ImpExpInitDir, fileName, s - fileName);
            ImpExpInitDir[s - fileName] = 0;
        }

        if (SalamanderGeneral->SalGetFileAttributes(fileName) != 0xFFFFFFFF) // so a read-only file can be overwritten
            SetFileAttributes(fileName, FILE_ATTRIBUTE_ARCHIVE);
        HANDLE file = HANDLES_Q(CreateFile(fileName, GENERIC_WRITE,
                                           FILE_SHARE_READ, NULL,
                                           CREATE_ALWAYS,
                                           FILE_FLAG_SEQUENTIAL_SCAN,
                                           NULL));
        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD err = serverType->ExportToFile(file);

            HANDLES(CloseHandle(file));
            SetCursor(oldCur);
            if (err != NO_ERROR) // display the error
            {
                sprintf(buf, LoadStr(IDS_SRVTYPEEXPORTERROR), SalamanderGeneral->GetErrorText(err));
                SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                DeleteFile(fileName); // delete the file if there was an error
            }
        }
        else
        {
            DWORD err = GetLastError();
            SetCursor(oldCur);
            sprintf(buf, LoadStr(IDS_SRVTYPEEXPORTERROR), SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
        }
        // announce the change on the path (our file was added)
        SalamanderGeneral->CutDirectory(fileName);
        SalamanderGeneral->PostChangeOnPathNotification(fileName, FALSE);
    }
}

void CConfigPageServers::OnImportServer()
{
    CServerType* serverType = new CServerType;
    if (TmpServerTypeList != NULL && serverType != NULL)
    {
        if (ImpExpInitDir[0] == 0)
            GetMyDocumentsPath(ImpExpInitDir);
        char fileName[MAX_PATH];
        fileName[0] = 0;
        OPENFILENAME ofn;
        memset(&ofn, 0, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = HWindow;
        char* s = LoadStr(IDS_SRVTYPEFILEFILTER);
        ofn.lpstrFilter = s;
        while (*s != 0) // create a double-null-terminated list
        {
            if (*s == '|')
                *s = 0;
            s++;
        }
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.nFilterIndex = 1;
        ofn.lpstrInitialDir = ImpExpInitDir;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

        char buf[300 + MAX_PATH];
        char typeBuf[SERVERTYPE_MAX_SIZE + 101];
        if (SalamanderGeneral->SafeGetOpenFileName(&ofn))
        {
            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            s = strrchr(fileName, '\\');
            if (s != NULL)
            {
                memcpy(ImpExpInitDir, fileName, s - fileName);
                ImpExpInitDir[s - fileName] = 0;
            }

            HANDLE file = HANDLES_Q(CreateFile(fileName, GENERIC_READ,
                                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                               OPEN_EXISTING,
                                               FILE_FLAG_SEQUENTIAL_SCAN,
                                               NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                DWORD err;
                int errResID;
                if (serverType->ImportFromFile(file, &err, &errResID) && err == NO_ERROR && errResID == 0)
                {
                    int index;
                    if (TmpServerTypeList->ContainsTypeName(serverType->TypeName, NULL, &index))
                    {
                        sprintf(buf, LoadStr(IDS_SRVTYPEOVERWRITE),
                                GetTypeNameForUser(serverType->TypeName, typeBuf, SERVERTYPE_MAX_SIZE + 101));
                        if (SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPPLUGINTITLE),
                                                             MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                                 MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
                        {
                            delete (TmpServerTypeList->At(index));
                            TmpServerTypeList->At(index) = serverType;
                            serverType = NULL;         // it has been added, we will not deallocate it
                            RefreshList(FALSE, index); // show the new type to the user (it is at index 'index')
                        }
                    }
                    else // we are about to add a new server type
                    {
                        TmpServerTypeList->Add(serverType);
                        if (TmpServerTypeList->IsGood())
                        {
                            serverType = NULL; // it has been added, we will not deallocate it
                            RefreshList(TRUE); // show the new type to the user (it is at the end of the list)
                        }
                        else
                            TmpServerTypeList->ResetState();
                    }
                }

                HANDLES(CloseHandle(file));
                SetCursor(oldCur);
                if (err != NO_ERROR || errResID != 0) // display the error
                {
                    if (errResID != 0)
                        sprintf(buf, LoadStr(IDS_SRVTYPEIMPORTERROR), LoadStr(errResID));
                    else
                    {
                        sprintf(buf, LoadStr(IDS_SRVTYPEIMPORTERROR), SalamanderGeneral->GetErrorText(err));
                    }
                    SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                                     MB_OK | MB_ICONEXCLAMATION);
                }
            }
            else
            {
                DWORD err = GetLastError();
                SetCursor(oldCur);
                sprintf(buf, LoadStr(IDS_SRVTYPEIMPORTERROR), SalamanderGeneral->GetErrorText(err));
                SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
        }
    }
    if (serverType != NULL)
        delete serverType;
}

INT_PTR
CConfigPageServers::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfigPageServers::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (TmpServerTypeList != NULL)
        {
            TRACE_E("Unaxpected situation in CConfigPageServers::DialogProc(): TmpServerTypeList is not empty!");
            delete TmpServerTypeList;
        }
        TmpServerTypeList = new CServerTypeList;
        if (TmpServerTypeList != NULL)
        {
            if (!TmpServerTypeList->CopyItemsFrom(Config.LockServerTypeList()))
            {
                delete TmpServerTypeList;
                TmpServerTypeList = NULL;
            }
            Config.UnlockServerTypeList();
        }
        if (TmpServerTypeList == NULL)
            TRACE_E("CConfigPageServers::DialogProc(): Unable to make a copy of server types list, dialog can't work.");

        // attach to the listbox (due to Alt+arrow keys; they do not go to WM_VKEYTOITEM)
        CServersListbox* list = new CServersListbox(this, IDL_SUPPORTEDSERVERS);
        if (list != NULL && list->HWindow == NULL)
            delete list; // if the control is missing, release the object (otherwise it frees itself)

        SalamanderGUI->AttachButton(HWindow, IDB_OTHERSERVERACT, BTF_RIGHTARROW);
        INT_PTR ret = CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
        EnableControls();
        return ret;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDL_SUPPORTEDSERVERS:
        {
            if (HIWORD(wParam) == LBN_SELCHANGE)
                EnableControls();
            break;
        }

        case IDB_MOVESERVERDOWN:
        case IDB_MOVESERVERUP:
        case CM_REMOVESERVER:
        case IDB_NEWSERVER:
        case CM_COPYSERVERTO:
        case CM_RENAMESERVER:
        case CM_EXPORTSERVER:
        case IDB_EDITSERVER:
        {
            HWND list = GetDlgItem(HWindow, IDL_SUPPORTEDSERVERS);
            int caret = (int)SendMessage(list, LB_GETCURSEL, 0, 0);
            if (TmpServerTypeList != NULL &&
                (LOWORD(wParam) == IDB_NEWSERVER ||
                 caret != LB_ERR && caret >= 0 && caret < TmpServerTypeList->Count))
            {
                CServerType* s = NULL;
                if (caret >= 0 && caret < TmpServerTypeList->Count)
                {
                    s = TmpServerTypeList->At(caret);
                    if (s == NULL)
                        return TRUE; // fatal, do not continue
                }
                char buf[300 + SERVERTYPE_MAX_SIZE];
                char typeBuf[SERVERTYPE_MAX_SIZE + 101];
                switch (LOWORD(wParam))
                {
                case IDB_MOVESERVERDOWN:
                {
                    if (caret + 1 < TmpServerTypeList->Count)
                        MoveItem(list, caret, caret + 1);
                    break;
                }

                case IDB_MOVESERVERUP:
                {
                    if (caret > 0)
                        MoveItem(list, caret, caret - 1);
                    break;
                }

                case CM_REMOVESERVER:
                {
                    sprintf(buf, LoadStr(IDS_SRVTYPEREMOVECONF),
                            GetTypeNameForUser(s->TypeName, typeBuf, SERVERTYPE_MAX_SIZE + 101));
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = HWindow;
                    params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED |
                                   MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT;
                    params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
                    params.Text = buf;
                    if (SalamanderGeneral->SalMessageBoxEx(&params) == IDYES)
                    {
                        TmpServerTypeList->Delete(caret);
                        if (!TmpServerTypeList->IsGood())
                            TmpServerTypeList->ResetState(); // deletion happened anyway
                        // remove the row in the listbox as well
                        SendMessage(list, WM_SETREDRAW, FALSE, 0);
                        int topIndex = (int)SendMessage(list, LB_GETTOPINDEX, 0, 0);
                        SendMessage(list, LB_DELETESTRING, caret, 0);
                        SendMessage(list, LB_SETTOPINDEX, topIndex, 0);
                        if (caret >= TmpServerTypeList->Count)
                            caret = TmpServerTypeList->Count - 1;
                        if (caret >= 0)
                            SendMessage(list, LB_SETCURSEL, caret, 0);
                        SendMessage(list, WM_SETREDRAW, TRUE, 0);
                        PostMessage(HWindow, WM_COMMAND, MAKELONG(IDL_SUPPORTEDSERVERS, LBN_SELCHANGE), 0);
                    }
                    break;
                }

                case IDB_NEWSERVER:
                case CM_COPYSERVERTO:
                case CM_RENAMESERVER:
                {
                    char name[SERVERTYPE_MAX_SIZE];
                    name[0] = '*';
                    if (LOWORD(wParam) == CM_RENAMESERVER || LOWORD(wParam) == CM_COPYSERVERTO)
                    {
                        lstrcpyn(name + 1, s != NULL ? (s->TypeName[0] == '*' ? s->TypeName + 1 : s->TypeName) : "",
                                 SERVERTYPE_MAX_SIZE - 1);
                    }
                    else
                        name[1] = 0;
                    CRenameDlg dlg(HWindow, name + 1,
                                   LOWORD(wParam) == IDB_NEWSERVER || LOWORD(wParam) == CM_COPYSERVERTO,
                                   FALSE, TRUE,
                                   s != NULL ? GetTypeNameForUser(s->TypeName, typeBuf, SERVERTYPE_MAX_SIZE + 101) : NULL);
                    if (LOWORD(wParam) == CM_COPYSERVERTO)
                        dlg.CopyDataFromFocusedServer = TRUE;
                    while (1)
                    {
                        if (dlg.Execute() == IDOK)
                        {
                            if (TmpServerTypeList->ContainsTypeName(name, LOWORD(wParam) == CM_RENAMESERVER ? s : NULL))
                            {
                                sprintf(buf, LoadStr(IDS_SRVTYPENOTUNIQUE), name + 1);
                                SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                                                 MB_OK | MB_ICONEXCLAMATION);
                                continue; // let the user change the name
                            }
                            if (LOWORD(wParam) == IDB_NEWSERVER || LOWORD(wParam) == CM_COPYSERVERTO) // new/copy to
                            {
                                if (s != NULL && dlg.CopyDataFromFocusedServer)
                                {
                                    TmpServerTypeList->AddServerType(name, s);
                                }
                                else
                                {
                                    const char* column = "1,name,0,\\0,0,\\0,1,\\0";
                                    TmpServerTypeList->AddServerType(name, NULL, 1, &column, NULL);
                                }
                            }
                            else // rename
                            {
                                if (strcmp(s->TypeName, s->TypeName[0] != '*' ? name + 1 : name) != 0)
                                { // prevent switching to user defined just by pressing OK without changing the name
                                    UpdateStr(s->TypeName, name);
                                }
                            }
                            RefreshList(LOWORD(wParam) == IDB_NEWSERVER || LOWORD(wParam) == CM_COPYSERVERTO);
                        }
                        break;
                    }
                    break;
                }

                case CM_EXPORTSERVER:
                {
                    OnExportServer(s);
                    break;
                }

                case IDB_EDITSERVER:
                {
                    CEditServerTypeDlg(HWindow, s).Execute();
                    RefreshList(FALSE);
                    break;
                }
                }
            }
            return TRUE;
        }

        case CM_IMPORTSERVER:
        {
            OnImportServer();
            return TRUE;
        }

        case IDB_OTHERSERVERACT:
        {
            HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_SERVERSOTHERACT));
            if (main != NULL)
            {
                HMENU subMenu = GetSubMenu(main, 0);
                if (subMenu != NULL)
                {
                    BOOL enable = TmpServerTypeList != NULL ? (TmpServerTypeList->Count > 0) : FALSE;
                    MyEnableMenuItem(subMenu, CM_COPYSERVERTO, enable);
                    MyEnableMenuItem(subMenu, CM_RENAMESERVER, enable);
                    MyEnableMenuItem(subMenu, CM_REMOVESERVER, enable);
                    MyEnableMenuItem(subMenu, CM_EXPORTSERVER, enable);
                    TPMPARAMS tpmPar;
                    tpmPar.cbSize = sizeof(tpmPar);
                    GetWindowRect(GetDlgItem(HWindow, IDB_OTHERSERVERACT), &tpmPar.rcExclude);
                    DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                                 tpmPar.rcExclude.right, tpmPar.rcExclude.top, HWindow, &tpmPar);
                    if (cmd != 0)
                        PostMessage(HWindow, WM_COMMAND, cmd, 0);
                }
                DestroyMenu(main);
            }
            return TRUE;
        }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CEditServerTypeDlg
//

CEditServerTypeDlg::CEditServerTypeDlg(HWND parent, CServerType* serverType)
    : CCenteredDialog(HLanguage, IDD_EDITSERVERTYPE, IDD_EDITSERVERTYPE, parent), ColumnsData(5, 5)
{
    ServerType = serverType;
    HListView = NULL;
    CanReadListViewChanges = TRUE;

    // create a local copy of the column data
    BOOL ok = TRUE;
    int i;
    for (i = 0; i < ServerType->Columns.Count; i++)
    {
        CSrvTypeColumn* c = ServerType->Columns[i]->MakeCopy();
        if (c != NULL)
        {
            ColumnsData.Add(c);
            if (!ColumnsData.IsGood())
            {
                ok = FALSE;
                delete c;
                ColumnsData.ResetState();
                break;
            }
        }
        else
        {
            ok = FALSE;
            break;
        }
    }
    if (!ok)
        ColumnsData.DestroyMembers(); // safeguard against a potential overlooked error

    RawListing = NULL;
    RawListIncomplete = FALSE;
}

CEditServerTypeDlg::~CEditServerTypeDlg()
{
    if (RawListing != NULL)
        free(RawListing);
}

void CEditServerTypeDlg::Validate(CTransferInfo& ti)
{
    // verify the autodetect condition
    char cond[AUTODETCOND_MAX_SIZE];
    ti.EditLine(IDE_AUTODETECTCOND, cond, AUTODETCOND_MAX_SIZE);
    int errorPos = -1;
    int errorResID = -1;
    BOOL lowMem = FALSE;
    char errBuf[200];
    errBuf[0] = 0;
    CFTPAutodetCondNode* node = CompileAutodetectCond(cond, &errorPos, &errorResID, &lowMem, errBuf, 200);
    if (node != NULL)
        delete node; // the condition is OK, delete it again
    else             // display the error and mark the error in the "autodetect condition" edit box
    {
        if (errBuf[0] != 0 || errorResID != -1) // some "reasonable" error was found, comment on it
        {
            char buf[300];
            sprintf(buf, LoadStr(IDS_STPAR_UNABLECOMPAUTODCOND), (errBuf[0] == 0 ? LoadStr(errorResID) : errBuf));
            SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            // mark the error location in the condition text
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_AUTODETECTCOND), TRUE);
            SendDlgItemMessage(HWindow, IDE_AUTODETECTCOND, EM_SETSEL, (WPARAM)errorPos,
                               (LPARAM)errorPos);
            ti.ErrorOn(IDE_AUTODETECTCOND);
        }
    }

    if (ti.IsGood()) // validate the columns
    {
        BOOL errResID = 0;
        if (!ValidateSrvTypeColumns(&ColumnsData, &errResID))
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(errResID),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDL_SRVTYPECOLUMNS);
        }
    }

    if (ti.IsGood()) // validate the parsing rules
    {
        char rules[PARSER_MAX_SIZE];
        ti.EditLine(IDE_PARSINGRULES, rules, PARSER_MAX_SIZE);
        errorPos = -1;
        errorResID = -1;
        lowMem = FALSE;
        CFTPParser* parser = CompileParsingRules(rules, &ColumnsData, &errorPos, &errorResID, &lowMem);
        if (parser != NULL)
            delete parser; // the parser is OK, delete it again
        else               // display the error and mark the error in the "rules for parsing" edit box
        {
            if (errorResID != -1) // some "reasonable" error was found, comment on it
            {
                char buf[300];
                sprintf(buf, LoadStr(IDS_STPAR_UNABLECOMPPARSER), LoadStr(errorResID));
                SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                // mark the error location in the parsing rules text
                SendDlgItemMessage(HWindow, IDE_PARSINGRULES, EM_SETSEL, (WPARAM)errorPos,
                                   (LPARAM)errorPos);
                SendDlgItemMessage(HWindow, IDE_PARSINGRULES, EM_SCROLLCARET, 0, 0); // scroll caret into view
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_PARSINGRULES), TRUE);
                ti.ErrorOn(IDE_PARSINGRULES);
            }
        }
    }
}

void CEditServerTypeDlg::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        ti.EditLine(IDE_AUTODETECTCOND, HandleNULLStr(ServerType->AutodetectCond), AUTODETCOND_MAX_SIZE);
        ti.EditLine(IDE_PARSINGRULES, HandleNULLStr(ServerType->RulesForParsing), PARSER_MAX_SIZE, FALSE);
    }
    else // data from window
    {
        BOOL change = FALSE; // TRUE = the data changed in the dialog (requires renaming to "user defined")
        char buf[AUTODETCOND_MAX_SIZE];
        ti.EditLine(IDE_AUTODETECTCOND, buf, AUTODETCOND_MAX_SIZE);
        if (strcmp(buf, HandleNULLStr(ServerType->AutodetectCond)) != 0)
        {
            change = TRUE;
            UpdateStr(ServerType->AutodetectCond, (buf[0] == 0 ? NULL : buf));
            if (ServerType->CompiledAutodetCond != NULL)
            {
                delete ServerType->CompiledAutodetCond;
                ServerType->CompiledAutodetCond = NULL;
            }
        }

        char buf2[PARSER_MAX_SIZE];
        ti.EditLine(IDE_PARSINGRULES, buf2, PARSER_MAX_SIZE);
        if (strcmp(buf2, HandleNULLStr(ServerType->RulesForParsing)) != 0)
        {
            change = TRUE;
            UpdateStr(ServerType->RulesForParsing, (buf2[0] == 0 ? NULL : buf2));
            if (ServerType->CompiledParser != NULL)
            {
                delete ServerType->CompiledParser;
                ServerType->CompiledParser = NULL;
            }
        }

        // check for changes to the columns and, if needed, copy the local column data into the result object
        BOOL needUpdate = TRUE;
        if (ServerType->Columns.Count == ColumnsData.Count)
        {
            char colStr1[STC_MAXCOLUMNSTR];
            char colStr2[STC_MAXCOLUMNSTR];
            int i;
            for (i = 0; i < ColumnsData.Count; i++)
            {
                ColumnsData[i]->SaveToStr(colStr1, STC_MAXCOLUMNSTR);
                ServerType->Columns[i]->SaveToStr(colStr2, STC_MAXCOLUMNSTR);
                if (strcmp(colStr1, colStr2) != 0)
                    break;
            }
            needUpdate = (i < ColumnsData.Count);
        }

        if (needUpdate)
        {
            change = TRUE;
            BOOL ok = TRUE;
            ServerType->Columns.DestroyMembers();
            if (!ServerType->Columns.IsGood())
                ServerType->Columns.ResetState();
            if (ServerType->CompiledParser != NULL)
            {
                delete ServerType->CompiledParser;
                ServerType->CompiledParser = NULL;
            }
            int i;
            for (i = 0; i < ColumnsData.Count; i++)
            {
                CSrvTypeColumn* c = ColumnsData[i]->MakeCopy();
                if (c != NULL)
                {
                    ServerType->Columns.Add(c);
                    if (!ServerType->Columns.IsGood())
                    {
                        ok = FALSE;
                        delete c;
                        ServerType->Columns.ResetState();
                        break;
                    }
                }
                else
                {
                    ok = FALSE;
                    break;
                }
            }
            if (!ok)
                ServerType->Columns.DestroyMembers(); // safeguard against a potential overlooked error
        }

        // if the data changed, we must change the server type to "user defined"
        if (change && ServerType->TypeName[0] != '*')
        {
            char name[SERVERTYPE_MAX_SIZE];
            name[0] = '*';
            lstrcpyn(name + 1, ServerType->TypeName, SERVERTYPE_MAX_SIZE - 1);
            BOOL err = FALSE;
            UpdateStr(ServerType->TypeName, name, &err);
            if (err && ServerType->TypeName[0] != 0)
                ServerType->TypeName[0] = '*'; // "user defined" at any cost
        }
    }
}

void CEditServerTypeDlg::EnableControls()
{
    int i = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
    if (i == -1)
        return; // another call will arrive once focus returns; wait for it
    BOOL enableRemove = (i > 0);
    HWND focus = GetFocus();
    if (!enableRemove && focus == GetDlgItem(HWindow, IDB_REMOVECOLUMN))
    {
        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)HListView, TRUE);
    }
    EnableWindow(GetDlgItem(HWindow, IDB_REMOVECOLUMN), enableRemove);
}

void CEditServerTypeDlg::InitColumns()
{
    CALL_STACK_MESSAGE1("CEditServerTypeDlg::InitColumns()");
    LV_COLUMN lvc;
    int header[6] = {IDS_SRVTYPECOL_ID, IDS_SRVTYPECOL_NAME, IDS_SRVTYPECOL_TYPE,
                     IDS_SRVTYPECOL_EMPTYVAL, IDS_SRVTYPECOL_DESCR, IDS_SRVTYPECOL_ALIGN};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; i < 6; i++) // create columns
    {
        lvc.pszText = LoadStr(header[i]);
        lvc.iSubItem = i;
        ListView_InsertColumn(HListView, i, &lvc);
        //    ListView_SetColumnWidth(HListView, i, LVSCW_AUTOSIZE_USEHEADER);  // set the widths later in SetColumnWidths()
    }
}

void CEditServerTypeDlg::SetColumnWidths()
{
    int i;
    for (i = 0; i < 6; i++)
        ListView_SetColumnWidth(HListView, i, LVSCW_AUTOSIZE_USEHEADER);
}

void CEditServerTypeDlg::RefreshListView(BOOL onlySet, int selIndex)
{
    CanReadListViewChanges = FALSE;
    //  LockWindowUpdate(HListView);    // do not use - it makes the entire Windows flicker
    SendMessage(HListView, WM_SETREDRAW, FALSE, 0);
    // I see no reason to dim the window here; there are few columns and calling SetColumnWidths()
    // does not cause the list view to flicker
    //  SetWindowPos(HListView, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOZORDER);

    int topIndex = 0;
    if (ListView_GetItemCount(HListView) > 0)
        topIndex = ListView_GetTopIndex(HListView);

    if (!onlySet)
        ListView_DeleteAllItems(HListView);
    int i;
    for (i = 0; i < ColumnsData.Count; i++)
    {
        CSrvTypeColumn* col = ColumnsData[i];

        // column ID + insert the item if needed
        if (!onlySet)
        {
            LVITEM lvi;
            lvi.mask = LVIF_TEXT;
            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.pszText = HandleNULLStr(col->ID);
            ListView_InsertItem(HListView, &lvi);
        }
        else
            ListView_SetItemText(HListView, i, 0, HandleNULLStr(col->ID));

        // checkbox visible
        // unfortunately this is not available in the header: ListView_SetCheckState(HListView, i, col->Visible != FALSE);
        // so I replaced it with this:
        ListView_SetItemState(HListView, i, INDEXTOSTATEIMAGEMASK((col->Visible != FALSE) + 1),
                              LVIS_STATEIMAGEMASK);

        // column name
        char bufName[STC_NAME_MAX_SIZE + 2];
        if (col->NameID != -1)
            LoadStdColumnStrName(bufName, STC_NAME_MAX_SIZE, col->NameID);
        else
            _snprintf_s(bufName, _TRUNCATE, "\"%s\"", HandleNULLStr(col->NameStr));
        ListView_SetItemText(HListView, i, 1, bufName);

        // column type
        char bufType[100];
        GetColumnTypeName(bufType, 100, col->Type);
        ListView_SetItemText(HListView, i, 2, bufType);

        // column empty value
        char* emptyVal;
        char emptyValBuf[100];
        if (col->EmptyValue == NULL || *(col->EmptyValue) == 0)
        {
            GetColumnEmptyValueForType(emptyValBuf, 100, col->Type);
            emptyVal = emptyValBuf;
        }
        else
            emptyVal = col->EmptyValue;
        ListView_SetItemText(HListView, i, 3, emptyVal);

        // column description
        char bufDescr[STC_DESCR_MAX_SIZE + 2];
        if (col->DescrID != -1)
            LoadStdColumnStrDescr(bufDescr, STC_DESCR_MAX_SIZE, col->DescrID);
        else
            _snprintf_s(bufDescr, _TRUNCATE, "\"%s\"", HandleNULLStr(col->DescrStr));
        ListView_SetItemText(HListView, i, 4, bufDescr);

        // column alignment
        char emptyBuff[] = "";
        ListView_SetItemText(HListView, i, 5, col->Type >= stctFirstGeneral ? LoadStr(col->LeftAlignment ? IDS_SRVTYPECOL_ALIGNLEFT : IDS_SRVTYPECOL_ALIGNRIGHT) : emptyBuff);
    }

    int count = ListView_GetItemCount(HListView);
    if ((!onlySet || selIndex != -1) && count > 0)
    {
        if (topIndex >= count)
            topIndex = count - 1;
        if (topIndex < 0)
            topIndex = 0;
        // replacement for SetTopIndex in list view
        ListView_EnsureVisible(HListView, count - 1, FALSE);
        ListView_EnsureVisible(HListView, topIndex, FALSE);

        if (selIndex >= count)
            selIndex = count - 1;
        if (selIndex < 0)
            selIndex = 0;
        DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItemState(HListView, selIndex, state, state);
        ListView_EnsureVisible(HListView, selIndex, FALSE);
    }
    //  LockWindowUpdate(NULL);  // do not use - it makes the entire Windows flicker
    //  SetWindowPos(HListView, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOZORDER);
    SendMessage(HListView, WM_SETREDRAW, TRUE, 0);

    CanReadListViewChanges = TRUE;
}

LRESULT
CEditRulesControlWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_GETDLGCODE: // make sure the text in the edit box is not constantly selected
    {
        LRESULT ret = CWindow::WindowProc(uMsg, wParam, lParam);
        return (ret & (~DLGC_HASSETSEL));
    }

    case WM_RBUTTONDOWN:
    {
        DWORD ret = (DWORD)SendMessage(HWindow, EM_CHARFROMPOS, 0, lParam);
        DWORD i = (unsigned short)LOWORD(ret);
        if (GetFocus() != HWindow)
        {
            SendMessage(GetParent(HWindow), WM_NEXTDLGCTL, (WPARAM)HWindow, TRUE);
            SendMessage(HWindow, EM_SETSEL, i, i); // always change the caret position
        }
        else
        {
            DWORD start, end;
            SendMessage(HWindow, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
            if ((i < start || i >= end) && (i < end || i >= start))
                SendMessage(HWindow, EM_SETSEL, i, i); // click outside the selection -> change the caret position
        }
        break;
    }

    case WM_CONTEXTMENU:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (x == -1 || y == -1)
        {
            POINT p;
            if (GetCaretPos(&p))
            {
                ClientToScreen(HWindow, &p);
                x = p.x;
                y = p.y;
            }
            else
            {
                RECT r;
                GetWindowRect(HWindow, &r);
                r.right = r.left;
                r.bottom = r.top;
                SalamanderGeneral->MultiMonEnsureRectVisible(&r, FALSE);
                x = r.left;
                y = r.top;
            }
        }

        HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_SRVTYPERULESMENU));
        if (main != NULL)
        {
            HMENU subMenu = GetSubMenu(main, 0);
            if (subMenu != NULL)
            {
                MyEnableMenuItem(subMenu, CM_STRM_UNDO, (BOOL)SendMessage(HWindow, EM_CANUNDO, 0, 0));
                DWORD start, end;
                SendMessage(HWindow, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
                MyEnableMenuItem(subMenu, CM_STRM_CUT, start != end);
                MyEnableMenuItem(subMenu, CM_STRM_COPY, start != end);
                MyEnableMenuItem(subMenu, CM_STRM_PASTE, IsClipboardFormatAvailable(CF_TEXT) || IsClipboardFormatAvailable(CF_UNICODETEXT));
                MyEnableMenuItem(subMenu, CM_STRM_DELETE, start != end);
                DWORD len = GetWindowTextLength(HWindow);
                MyEnableMenuItem(subMenu, CM_STRM_SELECTALL, start - end != len && end - start != len);
                DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                             x, y, HWindow, NULL);
                int strID = -1;
                switch (cmd)
                {
                case CM_STRM_UNDO:
                    SendMessage(HWindow, EM_UNDO, 0, 0);
                    break;
                case CM_STRM_CUT:
                    SendMessage(HWindow, WM_CUT, 0, 0);
                    break;
                case CM_STRM_COPY:
                    SendMessage(HWindow, WM_COPY, 0, 0);
                    break;
                case CM_STRM_PASTE:
                    SendMessage(HWindow, WM_PASTE, 0, 0);
                    break;
                case CM_STRM_DELETE:
                    SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM) "");
                    break;
                case CM_STRM_SELECTALL:
                    SendMessage(HWindow, EM_SETSEL, 0, -1);
                    break;

                case CM_STRM_RULESTART:
                    strID = 0;
                    break;
                case CM_STRM_RULEEND:
                    strID = 1;
                    break;
                case CM_STRM_COMMENT:
                    strID = 2;
                    break;
                case CM_STRM_STVARISFIRSTNELINE:
                    strID = 3;
                    break;
                case CM_STRM_STVARISLASTNELINE:
                    strID = 4;
                    break;
                case CM_STRM_STVARNEXTCHAR:
                    strID = 5;
                    break;
                case CM_STRM_STVARNEXTWORD:
                    strID = 6;
                    break;
                case CM_STRM_STVARRESTOFLINE:
                    strID = 7;
                    break;
                case CM_STRM_OPEREQUAL:
                    strID = 8;
                    break;
                case CM_STRM_OPERNOTEQUAL:
                    strID = 9;
                    break;
                case CM_STRM_OPERSTREQUAL:
                    strID = 10;
                    break;
                case CM_STRM_OPERSTRNOTEQUAL:
                    strID = 11;
                    break;
                case CM_STRM_OPERISINSTR:
                    strID = 12;
                    break;
                case CM_STRM_OPERISNOTINSTR:
                    strID = 13;
                    break;
                case CM_STRM_OPERENDWITH:
                    strID = 14;
                    break;
                case CM_STRM_OPERNOTENDWITH:
                    strID = 15;
                    break;
                case CM_STRM_SKIPWSIFANY:
                    strID = 16;
                    break;
                case CM_STRM_SKIPWS:
                    strID = 17;
                    break;
                case CM_STRM_SKIPNUMOFWS:
                    strID = 18;
                    break;
                case CM_STRM_SKIPWSENDEOLS:
                    strID = 19;
                    break;
                case CM_STRM_SKIPRESTOFLINE:
                    strID = 20;
                    break;
                case CM_STRM_ASGNRESTOFLINE:
                    strID = 21;
                    break;
                case CM_STRM_SKIPWORD:
                    strID = 22;
                    break;
                case CM_STRM_ASGNWORD:
                    strID = 23;
                    break;
                case CM_STRM_ASGNNUM:
                    strID = 24;
                    break;
                case CM_STRM_ASGNNUMWITHSEP:
                    strID = 25;
                    break;
                case CM_STRM_ASGN3LETTERMONTH:
                    strID = 26;
                    break;
                case CM_STRM_ASGNMONTH:
                    strID = 27;
                    break;
                case CM_STRM_ASGNDAY:
                    strID = 28;
                    break;
                case CM_STRM_ASGNYEAR:
                    strID = 29;
                    break;
                case CM_STRM_ASGNTIME:
                    strID = 30;
                    break;
                case CM_STRM_ASGNYEARORTIME:
                    strID = 31;
                    break;
                case CM_STRM_SKIPNUMOFLETTERS:
                    strID = 32;
                    break;
                case CM_STRM_ASGNNUMOFLETTERS:
                    strID = 33;
                    break;
                case CM_STRM_SKIPALLTO:
                    strID = 34;
                    break;
                case CM_STRM_ASGNALLTO:
                    strID = 35;
                    break;
                case CM_STRM_ASGNALLUPTO:
                    strID = 36;
                    break;
                case CM_STRM_ASGNUNIXLINK:
                    strID = 37;
                    break;
                case CM_STRM_ASGNUNIXDEV:
                    strID = 38;
                    break;
                case CM_STRM_IF:
                    strID = 39;
                    break;
                case CM_STRM_ASGNEXPR:
                    strID = 40;
                    break;
                case CM_STRM_CUTWSTRAIL:
                    strID = 41;
                    break;
                case CM_STRM_CUTWSINIT:
                    strID = 42;
                    break;
                case CM_STRM_CUTWSBOTH:
                    strID = 43;
                    break;
                case CM_STRM_BACK:
                    strID = 44;
                    break;
                case CM_STRM_ADDSTRTOCOL:
                    strID = 45;
                    break;
                case CM_STRM_ASGN3LETMONTHGEN:
                    strID = 46;
                    break;
                case CM_STRM_ASGNTEXTMONTH:
                    strID = 47;
                    break;
                case CM_STRM_ASGNTEXTMONTHGEN:
                    strID = 48;
                    break;
                case CM_STRM_ASGNPOSNUM:
                    strID = 49;
                    break;
                case CM_STRM_CUTENDOFSTR:
                    strID = 50;
                    break;
                case CM_STRM_SKIPALLTONUMBER:
                    strID = 51;
                    break;
                }
                if (strID >= 0)
                {
                    const char* strArr[] = {
                        "\r\n*",
                        ";\r\n",
                        "\r\n# line comment\r\n",
                        "first_nonempty_line",
                        "last_nonempty_line",
                        "next_char",
                        "next_word",
                        "rest_of_line",
                        " == ",
                        " != ",
                        " eq ",
                        " not_eq ",
                        " in ",
                        " not_in ",
                        " end_with ",
                        " not_end_with ",
                        "skip_white_spaces()",
                        "white_spaces()",
                        "white_spaces(number)",
                        "white_spaces_and_line_ends()",
                        "rest_of_line()",
                        "rest_of_line(<column-id>)",
                        "word()",
                        "word(<column-id>)",
                        "number(<column-id>)",
                        "number_with_separators(<column-id>, \"separators\")",
                        "month_3(<column-id>)",
                        "month(<column-id>)",
                        "day(<column-id>)",
                        "year(<column-id>)",
                        "time(<column-id>)",
                        "year_or_time(<date-column-id>, <time-column-id>)",
                        "all(number)",
                        "all(<column-id>, number)",
                        "all_to(\"string\")",
                        "all_to(<column-id>, \"string\")",
                        "all_up_to(<column-id>, \"string\")",
                        "unix_link(<is_dir>, <name-column-id>, <link-column-id>)",
                        "unix_device(<column-id>)",
                        "if(boolean-expression)",
                        "assign(<column-id>, expression)",
                        "cut_white_spaces_end(<column-id>)",
                        "cut_white_spaces_start(<column-id>)",
                        "cut_white_spaces(<column-id>)",
                        "back(number)",
                        "add_string_to_column(<column-id>, string-expression)",
                        "month_3(<column-id>, \"jan feb mar apr may jun jul aug sep oct nov dec\")",
                        "month_txt(<column-id>)",
                        "month_txt(<column-id>, \"Jan. Feb. März Apr. Mai Juni Juli Aug. Sept. Okt. Nov. Dez.\")",
                        "positive_number(<column-id>)",
                        "cut_end_of_string(<column-id>, number)",
                        "skip_to_number()",
                    };
                    int firstFunc = 16; /* index of the first function - UPDATE!!! */
                    if (strID < 52 /* number of strings in strArr - UPDATE!!! */)
                    {
                        const char* str = strArr[strID];
                        if (start == 0 || end == 0) // if we are at the start of the edit line, skip the leading EOL
                        {
                            if (*str == '\r')
                                str++;
                            if (*str == '\n')
                                str++;
                        }
                        BOOL commaBefore = FALSE;
                        BOOL spaceBefore = FALSE;
                        BOOL commaAfter = FALSE;
                        BOOL spaceAfter = FALSE;
                        if (strID >= firstFunc)
                        {
                            spaceBefore = TRUE;
                            char buf2[PARSER_MAX_SIZE];
                            GetWindowText(HWindow, buf2, PARSER_MAX_SIZE);
                            DWORD pos = min(start, end);
                            if (pos <= strlen(buf2))
                            {
                                char* s = buf2 + pos - 1;
                                while (s >= buf2 && (*s == ' ' || *s == ')'))
                                {
                                    if (*s == ')')
                                    {
                                        commaBefore = TRUE;
                                        break;
                                    }
                                    else
                                        spaceBefore = FALSE; // unnecessary, it's already there
                                    s--;
                                }
                            }
                            pos = max(start, end);
                            if (pos <= strlen(buf2))
                            {
                                char* s = buf2 + pos;
                                spaceAfter = (*s != 0 && IsCharAlpha(*s));
                                while (*s != 0 && (*s <= ' ' || IsCharAlpha(*s)))
                                {
                                    if (*s > ' ') // alpha
                                    {
                                        commaAfter = TRUE;
                                        break;
                                    }
                                    s++;
                                }
                            }
                        }
                        char strBuf[200];
                        sprintf(strBuf, "%s%s%s%s%s", (commaBefore ? "," : ""), (spaceBefore ? " " : ""),
                                str, (commaAfter ? "," : ""), (spaceAfter ? " " : ""));
                        SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM)strBuf);
                    }
                }
            }
            DestroyMenu(main);
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

INT_PTR
CEditServerTypeDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CEditServerTypeDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // disable select-all on focus and provide a custom context menu for the parsing rules edit
        CEditRulesControlWindow* wnd = new CEditRulesControlWindow(HWindow, IDE_PARSINGRULES);
        if (wnd != NULL && wnd->HWindow == NULL)
            delete wnd; // attaching failed - it will not deallocate itself

        // listview setup
        HListView = GetDlgItem(HWindow, IDL_SRVTYPECOLUMNS);
        DWORD exFlags = LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        // insert columns
        InitColumns();

        // insert items
        RefreshListView(FALSE);

        // set column widths
        SetColumnWidths();

        // adjust buttons
        SalamanderGUI->ChangeToArrowButton(HWindow, IDB_ADCONDINSERT);
        SalamanderGUI->AttachButton(HWindow, IDB_MOVECOLUMN, BTF_RIGHTARROW);

        // finish initializing the dialog
        INT_PTR ret = CCenteredDialog::DialogProc(uMsg, wParam, lParam);

        // enable controls
        EnableControls();
        return ret;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
        {
            BOOL change = FALSE; // TRUE = the data in the dialog were changed
            char buf[AUTODETCOND_MAX_SIZE];
            CTransferInfo ti(HWindow, ttDataFromWindow);
            ti.EditLine(IDE_AUTODETECTCOND, buf, AUTODETCOND_MAX_SIZE);
            if (strcmp(buf, HandleNULLStr(ServerType->AutodetectCond)) != 0)
                change = TRUE;
            else
            {
                char buf2[PARSER_MAX_SIZE];
                ti.EditLine(IDE_PARSINGRULES, buf2, PARSER_MAX_SIZE);
                if (strcmp(buf2, HandleNULLStr(ServerType->RulesForParsing)) != 0)
                    change = TRUE;
                else
                {
                    // check for a change in columns
                    if (ServerType->Columns.Count == ColumnsData.Count)
                    {
                        char colStr1[STC_MAXCOLUMNSTR];
                        char colStr2[STC_MAXCOLUMNSTR];
                        int i;
                        for (i = 0; i < ColumnsData.Count; i++)
                        {
                            ColumnsData[i]->SaveToStr(colStr1, STC_MAXCOLUMNSTR);
                            ServerType->Columns[i]->SaveToStr(colStr2, STC_MAXCOLUMNSTR);
                            if (strcmp(colStr1, colStr2) != 0)
                                break;
                        }
                        change = (i < ColumnsData.Count);
                    }
                    else
                        change = TRUE;
                }
            }

            if (change)
            {
                if (SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_SRVTYPE_EXITCONF),
                                                     LoadStr(IDS_FTPPLUGINTITLE),
                                                     MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                         MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
                {
                    return 0;
                }
            }
            break;
        }

        case IDB_ADCONDINSERT:
        {
            HMENU menu = CreatePopupMenu();
            if (menu != NULL)
            {
                /* used by the script export_mnu.py, which generates salmenu.mnu for Translator
   keep synchronized with the AppendMenu() calls below...
MENU_TEMPLATE_ITEM EditServerTypeADCondMenu[] =
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_ADCSTR_AND
  {MNTT_IT, IDS_ADCSTR_OR
  {MNTT_IT, IDS_ADCSTR_NOT
  {MNTT_IT, IDS_ADCSTR_LEFTPAR
  {MNTT_IT, IDS_ADCSTR_RIGHTPAR
  {MNTT_IT, IDS_ADCSTR_SYSTCONTAINS
  {MNTT_IT, IDS_ADCSTR_WELMSGCONTAINS
  {MNTT_IT, IDS_ADCSTR_SYSTCONTAINSREG
  {MNTT_IT, IDS_ADCSTR_WELMSGCONTAINSREG
  {MNTT_PE, 0
};
*/
                int resIDArr[] = {IDS_ADCSTR_AND, IDS_ADCSTR_OR, IDS_ADCSTR_NOT, -1,
                                  IDS_ADCSTR_LEFTPAR, IDS_ADCSTR_RIGHTPAR, -1,
                                  IDS_ADCSTR_SYSTCONTAINS, IDS_ADCSTR_WELMSGCONTAINS,
                                  IDS_ADCSTR_SYSTCONTAINSREG, IDS_ADCSTR_WELMSGCONTAINSREG, 0};
                const char* stringArr[] = {" and", " or", " not", NULL,
                                           " (", ") ", NULL,
                                           " syst_contains(\"pattern\")",
                                           " welcome_contains(\"pattern\")",
                                           " reg_exp_in_syst(\"pattern\")",
                                           " reg_exp_in_welcome(\"pattern\")", NULL};
                int* r = resIDArr;
                const char** s = stringArr;
                DWORD index = 1;
                while (*r != 0)
                {
                    if (*r != -1)
                        AppendMenu(menu, MF_STRING, (UINT_PTR)index, LoadStr(*r));
                    else
                        AppendMenu(menu, MF_SEPARATOR, NULL, NULL);
                    index++;
                    s++;
                    r++;
                }
                TPMPARAMS tpmPar;
                tpmPar.cbSize = sizeof(tpmPar);
                GetWindowRect(GetDlgItem(HWindow, IDB_ADCONDINSERT), &tpmPar.rcExclude);
                DWORD cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                             tpmPar.rcExclude.right, tpmPar.rcExclude.top, HWindow, &tpmPar);
                if (cmd != 0)
                {
                    r = resIDArr;
                    s = stringArr;
                    index = 1;
                    while (*r != 0)
                    {
                        if (index == cmd && *r != -1 && *s != NULL) // found, insert the text
                        {
                            DWORD start;
                            SendDlgItemMessage(HWindow, IDE_AUTODETECTCOND, EM_GETSEL, (WPARAM)&start, NULL);
                            SendDlgItemMessage(HWindow, IDE_AUTODETECTCOND, EM_REPLACESEL, TRUE,
                                               (LPARAM)(start == 0 && (*s)[0] == ' ' ? *s + 1 : *s));
                            break;
                        }
                        index++;
                        s++;
                        r++;
                    }
                }
                DestroyMenu(menu);
            }
            return TRUE; // do not continue further
        }

        case IDB_NEWCOLUMN:
        case IDB_EDITCOLUMN:
        {
            int i = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
            if (i >= 0 && i < ColumnsData.Count)
            {
                if (CEditSrvTypeColumnDlg(HWindow, &ColumnsData, &i,
                                          LOWORD(wParam) == IDB_EDITCOLUMN)
                        .Execute() == IDOK)
                {
                    RefreshListView(FALSE, i); // full refresh; even editing can move items
                    SetColumnWidths();
                }
            }
            return TRUE; // do not continue further
        }

        case IDB_REMOVECOLUMN:
        {
            int i = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
            if (i > 0 && i < ColumnsData.Count)
            {
                char buf[200 + STC_ID_MAX_SIZE];
                sprintf(buf, LoadStr(IDS_SRVTYPECOL_REMOVECONF), HandleNULLStr(ColumnsData[i]->ID));
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = HWindow;
                params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED |
                               MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT;
                params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
                params.Text = buf;
                if (SalamanderGeneral->SalMessageBoxEx(&params) == IDYES)
                {
                    if (ListView_DeleteItem(HListView, i))
                    {
                        ColumnsData.Delete(i);
                        if (!ColumnsData.IsGood())
                            ColumnsData.ResetState();
                        SetColumnWidths();
                        i = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
                        if (i != -1)
                        {
                            DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
                            ListView_SetItemState(HListView, i, state, state);
                        }
                    }
                }
            }
            return TRUE; // do not continue further
        }

        case CM_MOVECOLUMNUP:
        {
            int i = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
            if (i > 1 && i < ColumnsData.Count && ColumnsData[i - 1]->Type != stctExt)
            {
                CSrvTypeColumn* swap = ColumnsData[i - 1];
                ColumnsData[i - 1] = ColumnsData[i];
                ColumnsData[i] = swap;
                RefreshListView(TRUE, i - 1);
            }
            return TRUE; // do not continue further
        }

        case CM_MOVECOLUMNDOWN:
        {
            int i = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
            if (i > 0 && i + 1 < ColumnsData.Count && ColumnsData[i]->Type != stctExt)
            {
                CSrvTypeColumn* swap = ColumnsData[i];
                ColumnsData[i] = ColumnsData[i + 1];
                ColumnsData[i + 1] = swap;
                RefreshListView(TRUE, i + 1);
            }
            return TRUE; // do not continue further
        }

        case IDB_MOVECOLUMN:
        {
            HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_SRVTYPEMOVECOLUMN));
            if (main != NULL)
            {
                HMENU subMenu = GetSubMenu(main, 0);
                if (subMenu != NULL)
                {
                    int i = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
                    BOOL enable = i > 1 && i - 1 < ColumnsData.Count && ColumnsData[i - 1]->Type != stctExt;
                    MyEnableMenuItem(subMenu, CM_MOVECOLUMNUP, enable);
                    enable = i > 0 && i + 1 < ColumnsData.Count && ColumnsData[i]->Type != stctExt;
                    MyEnableMenuItem(subMenu, CM_MOVECOLUMNDOWN, enable);
                    TPMPARAMS tpmPar;
                    tpmPar.cbSize = sizeof(tpmPar);
                    GetWindowRect(GetDlgItem(HWindow, IDB_MOVECOLUMN), &tpmPar.rcExclude);
                    DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                                 tpmPar.rcExclude.right, tpmPar.rcExclude.top, HWindow, &tpmPar);
                    if (cmd != 0)
                        PostMessage(HWindow, WM_COMMAND, cmd, 0);
                }
                DestroyMenu(main);
            }
            return TRUE; // do not continue further
        }

        case IDB_TESTPARSER:
        {
            char rules[PARSER_MAX_SIZE];
            GetDlgItemText(HWindow, IDE_PARSINGRULES, rules, PARSER_MAX_SIZE);

            // compile the parsing rules -> obtain the listing parser
            int errorPos = -1;
            int errorResID = -1;
            BOOL lowMem = FALSE;
            CFTPParser* parser = CompileParsingRules(rules, &ColumnsData, &errorPos, &errorResID, &lowMem);
            if (parser != NULL) // we have the parser, we can open the dialog
            {
                CSrvTypeTestParserDlg(HWindow, parser, &ColumnsData, &RawListing, &RawListIncomplete).Execute();
                delete parser;
            }
            else // display the error and mark the error in the "rules for parsing" edit box
            {
                if (errorResID != -1) // some "reasonable" error was found, comment on it
                {
                    char buf[300];
                    sprintf(buf, LoadStr(IDS_STPAR_UNABLECOMPPARSER), LoadStr(errorResID));
                    SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                                     MB_OK | MB_ICONEXCLAMATION);
                    // mark the error location in the parsing rules text
                    SendDlgItemMessage(HWindow, IDE_PARSINGRULES, EM_SETSEL, (WPARAM)errorPos,
                                       (LPARAM)errorPos);
                    SendDlgItemMessage(HWindow, IDE_PARSINGRULES, EM_SCROLLCARET, 0, 0); // scroll caret into view
                    SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_PARSINGRULES), TRUE);
                }
            }
            return TRUE; // do not continue further
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDL_SRVTYPECOLUMNS)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case NM_DBLCLK:
            {
                PostMessage(HWindow, WM_COMMAND, IDB_EDITCOLUMN, 0);
                break;
            }

            case NM_RCLICK:
            {
                HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_SRVTYPECOLUMNS));
                if (main != NULL)
                {
                    HMENU subMenu = GetSubMenu(main, 0);
                    if (subMenu != NULL)
                    {
                        int i = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
                        MyEnableMenuItem(subMenu, IDB_EDITCOLUMN, i != -1);
                        MyEnableMenuItem(subMenu, IDB_REMOVECOLUMN, i > 0);
                        BOOL enable = i > 1 && i - 1 < ColumnsData.Count && ColumnsData[i - 1]->Type != stctExt;
                        MyEnableMenuItem(subMenu, CM_MOVECOLUMNUP, enable);
                        enable = i > 0 && i + 1 < ColumnsData.Count && ColumnsData[i]->Type != stctExt;
                        MyEnableMenuItem(subMenu, CM_MOVECOLUMNDOWN, enable);
                        DWORD pos = GetMessagePos();
                        DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                                     GET_X_LPARAM(pos), GET_Y_LPARAM(pos), HWindow, NULL);
                        if (cmd != 0)
                            PostMessage(HWindow, WM_COMMAND, cmd, 0);
                    }
                    DestroyMenu(main);
                }
                return FALSE; // continue processing
            }

            case LVN_KEYDOWN:
            {
                LPNMLVKEYDOWN nmhk = (LPNMLVKEYDOWN)nmh;
                BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
                if (altPressed && (nmhk->wVKey == VK_UP || nmhk->wVKey == VK_DOWN))
                {
                    PostMessage(HWindow, WM_COMMAND,
                                (nmhk->wVKey == VK_UP ? CM_MOVECOLUMNUP : CM_MOVECOLUMNDOWN), 0);
                }
                if (nmhk->wVKey == VK_INSERT)
                    PostMessage(HWindow, WM_COMMAND, IDB_NEWCOLUMN, 0);
                if (nmhk->wVKey == VK_DELETE)
                    PostMessage(HWindow, WM_COMMAND, IDB_REMOVECOLUMN, 0);
                return FALSE; // continue processing
            }

            case LVN_ITEMCHANGING:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                // write the new checkbox state into ColumnsData
                if (CanReadListViewChanges && // also generated during filling; changing the variable is undesirable
                    (nmhi->uOldState & LVIS_STATEIMAGEMASK) != (nmhi->uNewState & LVIS_STATEIMAGEMASK) &&
                    nmhi->iItem >= 0 && nmhi->iItem < ColumnsData.Count)
                {
                    if ((ColumnsData[nmhi->iItem]->Type == stctName || ColumnsData[nmhi->iItem]->Type == stctExt) &&
                        (nmhi->uNewState & LVIS_STATEIMAGEMASK) != INDEXTOSTATEIMAGEMASK(2))
                    {
                        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                        return TRUE; // end of processing
                    }
                    ColumnsData[nmhi->iItem]->Visible = (nmhi->uNewState & LVIS_STATEIMAGEMASK) ==
                                                        INDEXTOSTATEIMAGEMASK(2); // 2 = "checked" state
                }
                return FALSE; // continue processing
            }

            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                if ((nmhi->uOldState & LVIS_SELECTED) != (nmhi->uNewState & LVIS_SELECTED))
                    EnableControls();
                return FALSE; // continue processing
            }
            }
        }
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(HListView, GetSysColor(COLOR_WINDOW));
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}
