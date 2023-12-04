// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
// podpora pro plneni comboboxu s defaultnim chovanim pri chybach behem operaci

int OperDefFileAlreadyExists[] = {IDS_OPERATIONSUSERPROMPT, IDS_OPERATIONSAUTORENAME, IDS_OPERATIONSRESUME,
                                  IDS_OPERATIONSRESUMEOROVERWR, IDS_OPERATIONSOVERWRITE,
                                  IDS_OPERATIONSSKIP, -1};
int OperDefDirAlreadyExists[] = {IDS_OPERATIONSUSERPROMPT, IDS_OPERATIONSAUTORENAME,
                                 IDS_OPERATIONSUSEEXISTINGDIR, IDS_OPERATIONSSKIP, -1};
int OperDefCannotCreateFileOrDir[] = {IDS_OPERATIONSUSERPROMPT, IDS_OPERATIONSAUTORENAME, IDS_OPERATIONSSKIP, -1};
int OperDefRetryOnCreatedFile[] = {IDS_OPERATIONSUSERPROMPT, IDS_OPERATIONSAUTORENAME, IDS_OPERATIONSRESUME,
                                   IDS_OPERATIONSRESUMEOROVERWR, IDS_OPERATIONSOVERWRITE,
                                   IDS_OPERATIONSSKIP, -1};
int OperDefRetryOnResumedFile[] = {IDS_OPERATIONSUSERPROMPT, IDS_OPERATIONSAUTORENAME, IDS_OPERATIONSRESUME,
                                   IDS_OPERATIONSRESUMEOROVERWR, IDS_OPERATIONSOVERWRITE,
                                   IDS_OPERATIONSSKIP, -1};
int OperDefAsciiTrModeForBinFile[] = {IDS_OPERATIONSUSERPROMPT, IDS_OPERATIONSIGNORE,
                                      IDS_OPERATIONSINBINMODE, IDS_OPERATIONSSKIP, -1};
int OperDefUnknownAttrs[] = {IDS_OPERATIONSUSERPROMPT, IDS_OPERATIONSIGNORE, IDS_OPERATIONSSKIP, -1};
int OperDefDeleteArr[] = {IDS_OPERATIONSUSERPROMPT, IDS_OPERATIONSDELETE, IDS_OPERATIONSSKIP, -1};

void HandleOperationsCombo(int* value, CTransferInfo& ti, int resID, int arrValuesResID[])
{
    HWND combo;
    if (ti.GetControl(combo, resID))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(combo, CB_RESETCONTENT, 0, 0);
            int i = 0;
            while (arrValuesResID[i] != -1)
            {
                SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(arrValuesResID[i]));
                i++;
            }
            if (*value < 0 || *value >= i)
                *value = 0;
            SendMessage(combo, CB_SETCURSEL, *value, 0);
        }
        else
        {
            int res = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (res != CB_ERR)
                *value = res;
        }
    }
}

//
// ****************************************************************************
// CSolveItemErrorDlg
//

int SolveItemErrorGetDlgResId(CSolveItemErrorDlgType dlgType)
{
    switch (dlgType)
    {
    case sidtTransferFailedOnCreatedFile:
    case sidtTransferFailedOnResumedFile:
    case sidtUploadTransferFailedOnCreatedFile:
    case sidtUploadTransferFailedOnResumedFile:
        return IDD_SOLVEITEMERRDETAILEDEX;

    case sidtCannotCreateTgtDir:
        return IDD_SOLVEITEMERRDETAILED;

    case sidtASCIITrModeForBinFile:
    case sidtUploadASCIITrModeForBinFile:
        return IDD_SOLVEITEMERRASCIITR;

    case sidtCannotCreateTgtFile:
    case sidtUploadCannotCreateTgtFile:
    case sidtUploadCannotCreateTgtDir:
        return IDD_SOLVEITEMERRDETAILED2;

    case sidtUploadCrDirAutoRenFailed:
        return IDD_SOLVEITEMERRDETAILED3;

    case sidtUploadFileAutoRenFailed:
    case sidtUploadStoreFileFailed:
        return IDD_SOLVEITEMERRDETAILED3EX;

    case sidtTgtFileAlreadyExists:
    case sidtUploadTgtFileAlreadyExists:
        return IDD_SOLVEITEMERRSIMPLEEX;

    //case sidtTgtDirAlreadyExists:
    //case sidtUploadTgtDirAlreadyExists:
    default:
        return IDD_SOLVEITEMERRSIMPLE;
    }
}

CSolveItemErrorDlg::CSolveItemErrorDlg(HWND parent, CFTPOperation* oper, DWORD winError,
                                       const char* errDescription,
                                       const char* ftpPath, const char* ftpName,
                                       const char* diskPath, const char* diskName,
                                       BOOL* applyToAll, char** newName, CSolveItemErrorDlgType dlgType)
    : CCenteredDialog(HLanguage, SolveItemErrorGetDlgResId(dlgType), IDH_SOLVEERROR, parent)
{
    Oper = oper;
    WinError = winError;
    ErrDescription = errDescription;
    FtpPath = ftpPath;
    FtpName = ftpName;
    DiskPath = diskPath;
    DiskName = diskName;
    ApplyToAll = applyToAll;
    NewName = newName;
    DontTransferName = dlgType == sidtASCIITrModeForBinFile || dlgType == sidtUploadASCIITrModeForBinFile;
    UsedButtonID = 0;
    DlgType = dlgType;
}

void CSolveItemErrorDlg::Validate(CTransferInfo& ti)
{
    char buf[MAX_PATH];
    if (!DontTransferName)
    {
        ti.EditLine(IDE_SCRD_TGTNAME, buf, MAX_PATH);
        if (buf[0] == 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MAYNOTBEEMPTY),
                                             LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_SCRD_TGTNAME);
            return;
        }
    }
}

// jaky vyznam ma tlacitko ve spojeni s "do the same action for all operations with the same error":
// tlacitka (poradi hodnot v jednom radku pole):
//   use-alternate-name, resume, resume or overwrite, overwrite, skip, use-existing-dir, ignore, in-binary-mode
// specialni hodnota: -1 = nema smysl
int ButtonActionsTbl[][8] = {
    // radek 0 = file exists: download+upload
    {FILEALREADYEXISTS_AUTORENAME, FILEALREADYEXISTS_RESUME, FILEALREADYEXISTS_RES_OVRWR, FILEALREADYEXISTS_OVERWRITE, FILEALREADYEXISTS_SKIP, -1, -1, -1},
    // radek 1 = retry na souboru FTP klientem primo vytvorenem nebo prepsanem: download+upload
    {RETRYONCREATFILE_AUTORENAME, RETRYONCREATFILE_RESUME, RETRYONCREATFILE_RES_OVRWR, RETRYONCREATFILE_OVERWRITE, RETRYONCREATFILE_SKIP, -1, -1, -1},
    // radek 2 = retry na souboru FTP klientem resumnutem: download+upload
    {RETRYONRESUMFILE_AUTORENAME, RETRYONRESUMFILE_RESUME, RETRYONRESUMFILE_RES_OVRWR, RETRYONRESUMFILE_OVERWRITE, RETRYONRESUMFILE_SKIP, -1, -1, -1},
    // radek 3 = cilovy soubor/adresar nelze vytvorit: download+upload
    {CANNOTCREATENAME_AUTORENAME, -1, -1, -1, CANNOTCREATENAME_SKIP, -1, -1, -1},
    // radek 4 = ASCII transfer mode pro binarni soubor: download+upload
    {-1, -1, -1, -1, ASCIITRFORBINFILE_SKIP, -1, ASCIITRFORBINFILE_IGNORE, ASCIITRFORBINFILE_INBINMODE},
    // radek 5 = cilovy adresar jiz existuje: download+upload
    {DIRALREADYEXISTS_AUTORENAME, -1, -1, -1, DIRALREADYEXISTS_SKIP, DIRALREADYEXISTS_JOIN, -1, -1},
};

void CSolveItemErrorDlg::Transfer(CTransferInfo& ti)
{
    char buf[500];
    ti.CheckBox(IDC_SCRD_APPLYTOALL, *ApplyToAll);
    int value = 0;
    BOOL upload = DlgType == sidtUploadCannotCreateTgtDir || DlgType == sidtUploadTgtDirAlreadyExists ||
                  DlgType == sidtUploadCrDirAutoRenFailed || DlgType == sidtUploadCannotCreateTgtFile ||
                  DlgType == sidtUploadTgtFileAlreadyExists || DlgType == sidtUploadASCIITrModeForBinFile ||
                  DlgType == sidtUploadTransferFailedOnCreatedFile || DlgType == sidtUploadTransferFailedOnResumedFile ||
                  DlgType == sidtUploadFileAutoRenFailed || DlgType == sidtUploadStoreFileFailed;
    if (ti.Type == ttDataToWindow)
    {
        int dlgResID = SolveItemErrorGetDlgResId(DlgType);
        if (dlgResID == IDD_SOLVEITEMERRDETAILED || dlgResID == IDD_SOLVEITEMERRDETAILED2 ||
            dlgResID == IDD_SOLVEITEMERRDETAILED3 || dlgResID == IDD_SOLVEITEMERRDETAILEDEX ||
            dlgResID == IDD_SOLVEITEMERRDETAILED3EX)
        {
            buf[0] = 0;
            switch (DlgType)
            {
            case sidtTransferFailedOnCreatedFile:
            case sidtUploadTransferFailedOnCreatedFile:
                lstrcpyn(buf, LoadStr(IDS_SIED_DETAIL1), 500);
                break;

            case sidtTransferFailedOnResumedFile:
            case sidtUploadTransferFailedOnResumedFile:
                lstrcpyn(buf, LoadStr(IDS_SIED_DETAIL2), 500);
                break;

            default:
            {
                if (WinError != NO_ERROR)
                    FTPGetErrorText(WinError, buf, 500);
                else
                {
                    if (ErrDescription != NULL)
                        lstrcpyn(buf, ErrDescription, 500);
                }
                char* s = buf + strlen(buf);
                while (--s >= buf && (*s == '\r' || *s == '\n'))
                    ;
                *(s + 1) = 0;
                break;
            }
            }
            ti.EditLine(IDE_SCRD_DETAILS, buf, 500);
            SendDlgItemMessage(HWindow, IDE_SCRD_DETAILS, EM_SETSEL, 0, 0);
        }

        if (!upload)
        {
            ti.EditLine(IDE_SCRD_SRCPATH, (char*)FtpPath, FTP_MAX_PATH);
            ti.EditLine(IDE_SCRD_SRCNAME, (char*)FtpName, FTP_MAX_PATH);
            ti.EditLine(IDE_SCRD_TGTPATH, (char*)DiskPath, MAX_PATH);
            ti.EditLine(IDE_SCRD_TGTNAME, (char*)DiskName, MAX_PATH);
        }
        else
        {
            ti.EditLine(IDE_SCRD_SRCPATH, (char*)DiskPath, MAX_PATH);
            ti.EditLine(IDE_SCRD_SRCNAME, (char*)DiskName, MAX_PATH);
            ti.EditLine(IDE_SCRD_TGTPATH, (char*)FtpPath, FTP_MAX_PATH);
            ti.EditLine(IDE_SCRD_TGTNAME, (char*)FtpName, FTP_MAX_PATH);
        }
    }
    else
    {
        if (UsedButtonID == 0)
            TRACE_E("CSolveItemErrorDlg::Transfer(): unexpected call: UsedButtonID == 0!");
        if (UsedButtonID == CM_SIED_OVERWRITEALL)
        {
            *ApplyToAll = TRUE;
            UsedButtonID = CM_SIED_OVERWRITE;
        }
        if (!DontTransferName && NewName != NULL)
        {
            ti.EditLine(IDE_SCRD_TGTNAME, buf, MAX_PATH);
            if (!upload && strcmp(buf, DiskName) != 0 ||
                upload && strcmp(buf, FtpName) != 0)
            {
                *NewName = SalamanderGeneral->DupStr(buf);
                if (*NewName == NULL)
                {
                    SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_OPERDOPPR_LOWMEM),
                                                     LoadStr(IDS_FTPERRORTITLE),
                                                     MB_OK | MB_ICONEXCLAMATION);
                    ti.ErrorOn(IDE_SCRD_TGTNAME);
                    return;
                }
            }
        }

        if (*ApplyToAll)
        {
            value = -1;
            int typeIndex = -1;
            switch (DlgType)
            {
            case sidtCannotCreateTgtDir:
                typeIndex = 3;
                break;
            case sidtTgtDirAlreadyExists:
                typeIndex = 5;
                break;
            case sidtCannotCreateTgtFile:
                typeIndex = 3;
                break;

            case sidtTgtFileAlreadyExists:
            case sidtUploadTgtFileAlreadyExists:
                typeIndex = 0;
                break;

            case sidtTransferFailedOnCreatedFile:
            case sidtUploadTransferFailedOnCreatedFile:
                typeIndex = 1;
                break;

            case sidtTransferFailedOnResumedFile:
            case sidtUploadTransferFailedOnResumedFile:
                typeIndex = 2;
                break;

            case sidtASCIITrModeForBinFile:
            case sidtUploadASCIITrModeForBinFile:
                typeIndex = 4;
                break;

            case sidtUploadCannotCreateTgtDir:
                typeIndex = 3;
                break;
            case sidtUploadTgtDirAlreadyExists:
                typeIndex = 5;
                break;
            case sidtUploadCannotCreateTgtFile:
                typeIndex = 3;
                break;

            case sidtUploadCrDirAutoRenFailed:
            case sidtUploadFileAutoRenFailed:
            case sidtUploadStoreFileFailed:
                break; // tyto dialogy nemaji checkbox "Remember choice and do not show ..."

            default:
                TRACE_E("Unexpected situation in CSolveItemErrorDlg::Transfer(): unknown DlgType!");
                break;
            }
            if (typeIndex != -1)
            {
                switch (UsedButtonID)
                {
                case CM_SCRD_USEALTNAME:
                    value = ButtonActionsTbl[typeIndex][0];
                    break;
                case CM_SIED_RESUME:
                    value = ButtonActionsTbl[typeIndex][1];
                    break;
                case CM_SIED_RESUMEOROVR:
                    value = ButtonActionsTbl[typeIndex][2];
                    break;
                case CM_SIED_OVERWRITE:
                    value = ButtonActionsTbl[typeIndex][3];
                    break;
                // case CM_SIED_OVERWRITEALL:  // meni se na CM_SIED_OVERWRITE + *ApplyToAll==TRUE
                case IDB_SCRD_SKIP:
                    value = ButtonActionsTbl[typeIndex][4];
                    break;
                case CM_SDEX_USEEXISTINGDIR:
                    value = ButtonActionsTbl[typeIndex][5];
                    break;
                case CM_SATR_IGNORE:
                    value = ButtonActionsTbl[typeIndex][6];
                    break;

                case IDOK:
                {
                    if (DlgType == sidtASCIITrModeForBinFile || DlgType == sidtUploadASCIITrModeForBinFile)
                        value = ButtonActionsTbl[typeIndex][7]; // in-binary-mode
                    break;
                }
                }
            }
            if (value != -1)
            {
                switch (DlgType)
                {
                case sidtCannotCreateTgtDir:
                    Oper->SetCannotCreateDir(value);
                    break;
                case sidtTgtDirAlreadyExists:
                    Oper->SetDirAlreadyExists(value);
                    break;
                case sidtCannotCreateTgtFile:
                    Oper->SetCannotCreateFile(value);
                    break;
                case sidtTgtFileAlreadyExists:
                    Oper->SetFileAlreadyExists(value);
                    break;
                case sidtTransferFailedOnCreatedFile:
                    Oper->SetRetryOnCreatedFile(value);
                    break;
                case sidtTransferFailedOnResumedFile:
                    Oper->SetRetryOnResumedFile(value);
                    break;
                case sidtASCIITrModeForBinFile:
                    Oper->SetAsciiTrModeButBinFile(value);
                    break;
                case sidtUploadCannotCreateTgtDir:
                    Oper->SetUploadCannotCreateDir(value);
                    break;
                case sidtUploadTgtDirAlreadyExists:
                    Oper->SetUploadDirAlreadyExists(value);
                    break;
                case sidtUploadCannotCreateTgtFile:
                    Oper->SetUploadCannotCreateFile(value);
                    break;
                case sidtUploadTgtFileAlreadyExists:
                    Oper->SetUploadFileAlreadyExists(value);
                    break;
                case sidtUploadASCIITrModeForBinFile:
                    Oper->SetUploadAsciiTrModeButBinFile(value);
                    break;
                case sidtUploadTransferFailedOnCreatedFile:
                    Oper->SetUploadRetryOnCreatedFile(value);
                    break;
                case sidtUploadTransferFailedOnResumedFile:
                    Oper->SetUploadRetryOnResumedFile(value);
                    break;
                default:
                    TRACE_E("Unexpected situation in CSolveItemErrorDlg::Transfer(): unknown DlgType!");
                    break;
                }
            }
        }
    }
}

INT_PTR
CSolveItemErrorDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSolveItemErrorDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        int titleID = -1;
        switch (DlgType)
        {
        case sidtCannotCreateTgtDir:
        case sidtUploadCannotCreateTgtDir:
            titleID = IDS_SIED_TITLE1;
            break;

        case sidtTgtDirAlreadyExists:
            titleID = IDS_SIED_TITLE2;
            break;
        case sidtUploadTgtDirAlreadyExists:
            titleID = IDS_SIED_TITLE6;
            break;

        case sidtCannotCreateTgtFile:
        case sidtUploadCannotCreateTgtFile:
            titleID = IDS_SIED_TITLE3;
            break;

        case sidtTgtFileAlreadyExists:
        case sidtUploadTgtFileAlreadyExists:
            titleID = IDS_SIED_TITLE4;
            break;

        case sidtTransferFailedOnCreatedFile:
        case sidtTransferFailedOnResumedFile:
        case sidtUploadTransferFailedOnCreatedFile:
        case sidtUploadTransferFailedOnResumedFile:
            titleID = IDS_SIED_TITLE5;
            break;

        case sidtUploadFileAutoRenFailed:
            titleID = IDS_SIED_TITLE7;
            break;
        case sidtUploadStoreFileFailed:
            titleID = IDS_SIED_TITLE8;
            break;

        case sidtUploadASCIITrModeForBinFile:
            break;
        case sidtASCIITrModeForBinFile:
            break;
        case sidtUploadCrDirAutoRenFailed:
            break;

        default:
            TRACE_E("Unexpected situation in CSolveItemErrorDlg::DialogProc(): unknown DlgType!");
            break;
        }
        if (titleID != -1)
            SetWindowText(HWindow, LoadStr(titleID));
        SalamanderGUI->AttachButton(HWindow, IDOK, BTF_DROPDOWN);

        if (DlgType == sidtTgtFileAlreadyExists ||
            DlgType == sidtUploadTgtFileAlreadyExists)
        { // chceme svuj vlastni fokus na Overwrite buttonu
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, CM_SIED_OVERWRITE), TRUE);
            CCenteredDialog::DialogProc(uMsg, wParam, lParam);
            return FALSE;
        }
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (LOWORD(wParam) == IDOK) // dropdown menu na Retry buttonu
        {
            int menuID = IDM_DIREXISTSERRRETRY;
            if (DlgType == sidtTgtFileAlreadyExists ||
                DlgType == sidtUploadTgtFileAlreadyExists)
            {
                menuID = IDM_FILEEXISTSERRRETRY;
            }
            if (DlgType == sidtTransferFailedOnCreatedFile ||
                DlgType == sidtTransferFailedOnResumedFile ||
                DlgType == sidtUploadTransferFailedOnCreatedFile ||
                DlgType == sidtUploadTransferFailedOnResumedFile ||
                DlgType == sidtUploadFileAutoRenFailed ||
                DlgType == sidtUploadStoreFileFailed)
            {
                menuID = IDM_TRANSFERFAILEDERRRETRY;
            }
            if (DlgType == sidtASCIITrModeForBinFile || DlgType == sidtUploadASCIITrModeForBinFile)
            {
                menuID = IDM_ASCIITRFORBINFILE;
            }
            if (DlgType == sidtCannotCreateTgtFile ||
                DlgType == sidtCannotCreateTgtDir ||
                DlgType == sidtUploadCannotCreateTgtFile ||
                DlgType == sidtUploadCannotCreateTgtDir)
            {
                menuID = IDM_CANTCRFILEERRRETRY;
            }
            HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(menuID)); // zpristupnime vsechny prikazy v obou variantach ("join" by chybel v situaci, kdy adresar existuje + podle defaultu se zkusi autorename, ktery selze -> "already exists" se tvari diky chybe jako "cannot create")
            if (main != NULL)
            {
                HMENU subMenu = GetSubMenu(main, 0);
                if (subMenu != NULL)
                {
                    CGUIMenuPopupAbstract* salMenu = SalamanderGUI->CreateMenuPopup();
                    if (salMenu != NULL)
                    {
                        salMenu->SetTemplateMenu(subMenu);

                        RECT r;
                        GetWindowRect(GetDlgItem(HWindow, (int)wParam), &r);
                        BOOL selectMenuItem = LOWORD(lParam);
                        DWORD flags = MENU_TRACK_RETURNCMD;
                        if (selectMenuItem)
                        {
                            salMenu->SetSelectedItemIndex(0);
                            flags |= MENU_TRACK_SELECT;
                        }
                        DWORD cmd = salMenu->Track(flags, r.left, r.bottom, HWindow, &r);
                        if (cmd != 0)
                            PostMessage(HWindow, WM_COMMAND, cmd, 0);
                        SalamanderGUI->DestroyMenuPopup(salMenu);
                    }
                }
                DestroyMenu(main);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        case CM_SATR_IGNORE:
        case CM_SATR_RETRY:
        case CM_SCRD_USEALTNAME:
        case CM_SDEX_USEEXISTINGDIR:
        case CM_SIED_RESUME:
        case CM_SIED_RESUMEOROVR:
        case CM_SIED_OVERWRITE:
        case CM_SIED_OVERWRITEALL:
        case IDB_SCRD_SKIP:
        {
            UsedButtonID = LOWORD(wParam);
            if (LOWORD(wParam) == IDB_SCRD_SKIP)
                DontTransferName = TRUE;
            if (!ValidateData() ||
                !TransferData(ttDataFromWindow))
            {
                UsedButtonID = 0;
                return TRUE;
            }
            if (LOWORD(wParam) == CM_SIED_OVERWRITEALL)
                wParam = CM_SIED_OVERWRITE;
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            UsedButtonID = 0;
            return TRUE;
        }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CSolveItemErrUnkAttrDlg
//

CSolveItemErrUnkAttrDlg::CSolveItemErrUnkAttrDlg(HWND parent, CFTPOperation* oper,
                                                 const char* path, const char* name,
                                                 const char* origRights, WORD newAttr,
                                                 BOOL* applyToAll)
    : CCenteredDialog(HLanguage, IDD_SOLVEITEMERRUNKATTR, IDH_SOLVEERROR, parent)
{
    Oper = oper;
    Path = path;
    Name = name;
    OrigRights = origRights;
    NewAttr = newAttr;
    ApplyToAll = applyToAll;
    UsedButtonID = 0;
}

void CSolveItemErrUnkAttrDlg::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_SCRD_APPLYTOALL, *ApplyToAll);
    if (ti.Type == ttDataToWindow)
    {
        ti.EditLine(IDE_SCRD_SRCPATH, (char*)Path, FTP_MAX_PATH);
        ti.EditLine(IDE_SCRD_SRCNAME, (char*)Name, FTP_MAX_PATH);
        char* attrs = (char*)OrigRights;
        if (attrs == NULL)
            attrs = LoadStr(IDS_OPERDOPPR_UNKEXISTATTR);
        ti.EditLine(IDE_SCRD_CURATTR, attrs, 100);
        char buf[100];
        GetUNIXRightsStr(buf, 20, NewAttr);
        sprintf(buf + strlen(buf), " (%03o)", NewAttr);
        ti.EditLine(IDE_SCRD_NEWATTR, buf, MAX_PATH);
    }
    else
    {
        if (UsedButtonID == 0)
            TRACE_E("CSolveItemErrUnkAttrDlg::Transfer(): unexpected call: UsedButtonID == 0!");
        if (*ApplyToAll)
        {
            switch (UsedButtonID)
            {
            // case CM_SIEA_RETRY:  // u Retry to nema smysl
            // case CM_SIEA_SETNEWATTRS:   // neresime ... tohle otevira novy dialog
            case IDOK:
                Oper->SetUnknownAttrs(UNKNOWNATTRS_IGNORE);
                break;
            case IDB_SCRD_SKIP:
                Oper->SetUnknownAttrs(UNKNOWNATTRS_SKIP);
                break;
            }
        }
    }
}

INT_PTR
CSolveItemErrUnkAttrDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSolveItemErrUnkAttrDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalamanderGUI->AttachButton(HWindow, IDOK, BTF_DROPDOWN);
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (LOWORD(wParam) == IDOK) // dropdown menu na Ignore buttonu
        {
            HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_UNKATTRSERRIGNORE));
            if (main != NULL)
            {
                HMENU subMenu = GetSubMenu(main, 0);
                if (subMenu != NULL)
                {
                    CGUIMenuPopupAbstract* salMenu = SalamanderGUI->CreateMenuPopup();
                    if (salMenu != NULL)
                    {
                        salMenu->SetTemplateMenu(subMenu);

                        RECT r;
                        GetWindowRect(GetDlgItem(HWindow, (int)wParam), &r);
                        BOOL selectMenuItem = LOWORD(lParam);
                        DWORD flags = MENU_TRACK_RETURNCMD;
                        if (selectMenuItem)
                        {
                            salMenu->SetSelectedItemIndex(0);
                            flags |= MENU_TRACK_SELECT;
                        }
                        DWORD cmd = salMenu->Track(flags, r.left, r.bottom, HWindow, &r);
                        if (cmd != 0)
                            PostMessage(HWindow, WM_COMMAND, cmd, 0);
                        SalamanderGUI->DestroyMenuPopup(salMenu);
                    }
                }
                DestroyMenu(main);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        case CM_SIEA_RETRY:
        case CM_SIEA_SETNEWATTRS:
        case IDB_SCRD_SKIP:
        {
            UsedButtonID = LOWORD(wParam);
            if (!ValidateData() ||
                !TransferData(ttDataFromWindow))
            {
                UsedButtonID = 0;
                return TRUE;
            }
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            UsedButtonID = 0;
            return TRUE;
        }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CSolveItemSetNewAttrDlg
//

extern void UpdateCheckBox(HWND hWindow, int checkID, int value); // funkce definovana u CChangeAttrsDlg

CSolveItemSetNewAttrDlg::CSolveItemSetNewAttrDlg(HWND parent, CFTPOperation* oper,
                                                 const char* path, const char* name,
                                                 const char* origRights, WORD* attr,
                                                 BOOL* applyToAll)
    : CCenteredDialog(HLanguage, IDD_SOLVEITEMSETNEWATTR, IDD_SOLVEITEMSETNEWATTR, parent)
{
    Oper = oper;
    Path = path;
    Name = name;
    OrigRights = origRights;
    Attr = attr;
    ApplyToAll = applyToAll;

    EnableNotification = TRUE;
}

void CSolveItemSetNewAttrDlg::RefreshNumValue()
{
    UINT userRead = IsDlgButtonChecked(HWindow, IDC_READOWNER);
    UINT groupRead = IsDlgButtonChecked(HWindow, IDC_READGROUP);
    UINT othersRead = IsDlgButtonChecked(HWindow, IDC_READOTHERS);
    UINT userWrite = IsDlgButtonChecked(HWindow, IDC_WRITEOWNER);
    UINT groupWrite = IsDlgButtonChecked(HWindow, IDC_WRITEGROUP);
    UINT othersWrite = IsDlgButtonChecked(HWindow, IDC_WRITEOTHERS);
    UINT userExec = IsDlgButtonChecked(HWindow, IDC_EXECUTEOWNER);
    UINT groupExec = IsDlgButtonChecked(HWindow, IDC_EXECUTEGROUP);
    UINT othersExec = IsDlgButtonChecked(HWindow, IDC_EXECUTEOTHERS);

    char text[4];
    text[3] = 0;
    text[0] = '0' + (userRead << 2) + (userWrite << 1) + userExec;
    text[1] = '0' + (groupRead << 2) + (groupWrite << 1) + groupExec;
    text[2] = '0' + (othersRead << 2) + (othersWrite << 1) + othersExec;
    EnableNotification = FALSE;
    HWND edit = GetDlgItem(HWindow, IDE_NUMATTRVALUE);
    DWORD start = 0;
    DWORD end = 0;
    SendMessage(edit, EM_GETSEL, (WPARAM)(&start), (LPARAM)(&end));
    SetWindowText(edit, text);
    SendMessage(edit, EM_SETSEL, start, end);
    EnableNotification = TRUE;
}

void CSolveItemSetNewAttrDlg::Validate(CTransferInfo& ti)
{
    char text[5];
    if (GetDlgItemText(HWindow, IDE_NUMATTRVALUE, text, 5) != 3)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CHATTRNUMVAL3DIGITS), LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_NUMATTRVALUE);
        return;
    }
}

void CSolveItemSetNewAttrDlg::Transfer(CTransferInfo& ti)
{
    int userRead = ((*Attr & 0400) != 0);
    int groupRead = ((*Attr & 0040) != 0);
    int othersRead = ((*Attr & 0004) != 0);
    int userWrite = ((*Attr & 0200) != 0);
    int groupWrite = ((*Attr & 0020) != 0);
    int othersWrite = ((*Attr & 0002) != 0);
    int userExec = ((*Attr & 0100) != 0);
    int groupExec = ((*Attr & 0010) != 0);
    int othersExec = ((*Attr & 0001) != 0);
    ti.CheckBox(IDC_READOWNER, userRead);
    ti.CheckBox(IDC_WRITEOWNER, userWrite);
    ti.CheckBox(IDC_EXECUTEOWNER, userExec);
    ti.CheckBox(IDC_READGROUP, groupRead);
    ti.CheckBox(IDC_WRITEGROUP, groupWrite);
    ti.CheckBox(IDC_EXECUTEGROUP, groupExec);
    ti.CheckBox(IDC_READOTHERS, othersRead);
    ti.CheckBox(IDC_WRITEOTHERS, othersWrite);
    ti.CheckBox(IDC_EXECUTEOTHERS, othersExec);

    if (ti.Type == ttDataToWindow)
    {
        RefreshNumValue();

        EnableNotification = FALSE;
        ti.EditLine(IDE_SCRD_SRCPATH, (char*)Path, FTP_MAX_PATH);
        ti.EditLine(IDE_SCRD_SRCNAME, (char*)Name, FTP_MAX_PATH);
        char* origRights = (char*)OrigRights;
        if (origRights == NULL)
            origRights = LoadStr(IDS_OPERDOPPR_UNKEXISTATTR);
        ti.EditLine(IDE_SCRD_CURATTR, origRights, 100);
        EnableNotification = TRUE;
    }
    else
    {
        *Attr = 0;
        if (userRead)
            *Attr |= 0400;
        if (groupRead)
            *Attr |= 0040;
        if (othersRead)
            *Attr |= 0004;
        if (userWrite)
            *Attr |= 0200;
        if (groupWrite)
            *Attr |= 0020;
        if (othersWrite)
            *Attr |= 0002;
        if (userExec)
            *Attr |= 0100;
        if (groupExec)
            *Attr |= 0010;
        if (othersExec)
            *Attr |= 0001;
    }
    ti.CheckBox(IDC_SCRD_APPLYTOALL, *ApplyToAll);
}

INT_PTR
CSolveItemSetNewAttrDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSolveItemSetNewAttrDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (EnableNotification)
        {
            if (HIWORD(wParam) == BN_CLICKED &&
                (LOWORD(wParam) == IDC_READOWNER ||
                 LOWORD(wParam) == IDC_WRITEOWNER ||
                 LOWORD(wParam) == IDC_EXECUTEOWNER ||
                 LOWORD(wParam) == IDC_READGROUP ||
                 LOWORD(wParam) == IDC_WRITEGROUP ||
                 LOWORD(wParam) == IDC_EXECUTEGROUP ||
                 LOWORD(wParam) == IDC_READOTHERS ||
                 LOWORD(wParam) == IDC_WRITEOTHERS ||
                 LOWORD(wParam) == IDC_EXECUTEOTHERS))
            {
                RefreshNumValue();
            }
            else
            {
                if (HIWORD(wParam) == EN_CHANGE)
                {
                    char text[5];
                    if (GetDlgItemText(HWindow, IDE_NUMATTRVALUE, text, 5) == 3)
                    {
                        DWORD attr = 0;
                        if (text[0] >= '0' && text[0] <= '7')
                            attr |= (text[0] - '0') << 6;
                        if (text[1] >= '0' && text[1] <= '7')
                            attr |= (text[1] - '0') << 3;
                        if (text[2] >= '0' && text[2] <= '7')
                            attr |= text[2] - '0';

                        int userRead = ((attr & 0400) != 0);
                        int groupRead = ((attr & 0040) != 0);
                        int othersRead = ((attr & 0004) != 0);
                        int userWrite = ((attr & 0200) != 0);
                        int groupWrite = ((attr & 0020) != 0);
                        int othersWrite = ((attr & 0002) != 0);
                        int userExec = ((attr & 0100) != 0);
                        int groupExec = ((attr & 0010) != 0);
                        int othersExec = ((attr & 0001) != 0);
                        UpdateCheckBox(HWindow, IDC_READOWNER, userRead);
                        UpdateCheckBox(HWindow, IDC_WRITEOWNER, userWrite);
                        UpdateCheckBox(HWindow, IDC_EXECUTEOWNER, userExec);
                        UpdateCheckBox(HWindow, IDC_READGROUP, groupRead);
                        UpdateCheckBox(HWindow, IDC_WRITEGROUP, groupWrite);
                        UpdateCheckBox(HWindow, IDC_EXECUTEGROUP, groupExec);
                        UpdateCheckBox(HWindow, IDC_READOTHERS, othersRead);
                        UpdateCheckBox(HWindow, IDC_WRITEOTHERS, othersWrite);
                        UpdateCheckBox(HWindow, IDC_EXECUTEOTHERS, othersExec);

                        RefreshNumValue();
                    }
                }
            }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CSolveLowMemoryErr
//

CSolveLowMemoryErr::CSolveLowMemoryErr(HWND parent, const char* ftpPath,
                                       const char* ftpName, BOOL* applyToAll,
                                       int titleID)
    : CCenteredDialog(HLanguage, IDD_SOLVELOWMEMORYERR, IDH_SOLVEERROR, parent)
{
    FtpPath = ftpPath;
    FtpName = ftpName;
    ApplyToAll = applyToAll;
    TitleID = titleID;
}

void CSolveLowMemoryErr::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        ti.EditLine(IDE_SCRD_SRCPATH, (char*)FtpPath, FTP_MAX_PATH);
        ti.EditLine(IDE_SCRD_SRCNAME, (char*)FtpName, FTP_MAX_PATH);
    }
    ti.CheckBox(IDC_SCRD_APPLYTOALL, *ApplyToAll);
}

INT_PTR
CSolveLowMemoryErr::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSolveLowMemoryErr::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (TitleID != -1)
            SetWindowText(HWindow, LoadStr(TitleID));
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_SCRD_SKIP:
        {
            if (!ValidateData() ||
                !TransferData(ttDataFromWindow))
                return TRUE;
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CConfigPageOperations
//

CConfigPageOperations::CConfigPageOperations() : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGOPERATIONS, IDD_CFGOPERATIONS, PSP_HASHELP, NULL)
{
}

void CConfigPageOperations::Transfer(CTransferInfo& ti)
{
    HandleOperationsCombo(&Config.OperationsFileAlreadyExists, ti, IDC_FILEALREADYEXISTS,
                          OperDefFileAlreadyExists);

    HandleOperationsCombo(&Config.OperationsDirAlreadyExists, ti, IDC_DIRALREADYEXISTS,
                          OperDefDirAlreadyExists);

    HandleOperationsCombo(&Config.OperationsCannotCreateFile, ti, IDC_CANNOTCREATEFILE,
                          OperDefCannotCreateFileOrDir);
    HandleOperationsCombo(&Config.OperationsCannotCreateDir, ti, IDC_CANNOTCREATEDIR,
                          OperDefCannotCreateFileOrDir);

    HandleOperationsCombo(&Config.OperationsRetryOnCreatedFile, ti, IDC_RETRYONCREATED,
                          OperDefRetryOnCreatedFile);

    HandleOperationsCombo(&Config.OperationsRetryOnResumedFile, ti, IDC_RETRYONRESUMED,
                          OperDefRetryOnResumedFile);

    HandleOperationsCombo(&Config.OperationsAsciiTrModeButBinFile, ti, IDC_ASCIITRFORBINFILE,
                          OperDefAsciiTrModeForBinFile);

    HandleOperationsCombo(&Config.OperationsUnknownAttrs, ti, IDC_UNKNOWNATTRS,
                          OperDefUnknownAttrs);

    HandleOperationsCombo(&Config.OperationsNonemptyDirDel, ti, IDC_NONEMPTYDIRDEL,
                          OperDefDeleteArr);
    HandleOperationsCombo(&Config.OperationsHiddenFileDel, ti, IDC_HIDDENFILEDEL,
                          OperDefDeleteArr);
    HandleOperationsCombo(&Config.OperationsHiddenDirDel, ti, IDC_HIDDENDIRDEL,
                          OperDefDeleteArr);
}

//
// ****************************************************************************
// CConfigPageOperations2
//

CConfigPageOperations2::CConfigPageOperations2() : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGOPERATIONS2, IDD_CFGOPERATIONS2, PSP_HASHELP, NULL)
{
}

void CConfigPageOperations2::Transfer(CTransferInfo& ti)
{
    HandleOperationsCombo(&Config.UploadFileAlreadyExists, ti, IDC_FILEALREADYEXISTS,
                          OperDefFileAlreadyExists);

    HandleOperationsCombo(&Config.UploadDirAlreadyExists, ti, IDC_DIRALREADYEXISTS,
                          OperDefDirAlreadyExists);

    HandleOperationsCombo(&Config.UploadCannotCreateFile, ti, IDC_CANNOTCREATEFILE,
                          OperDefCannotCreateFileOrDir);
    HandleOperationsCombo(&Config.UploadCannotCreateDir, ti, IDC_CANNOTCREATEDIR,
                          OperDefCannotCreateFileOrDir);

    HandleOperationsCombo(&Config.UploadRetryOnCreatedFile, ti, IDC_RETRYONCREATED,
                          OperDefRetryOnCreatedFile);

    HandleOperationsCombo(&Config.UploadRetryOnResumedFile, ti, IDC_RETRYONRESUMED,
                          OperDefRetryOnResumedFile);

    HandleOperationsCombo(&Config.UploadAsciiTrModeButBinFile, ti, IDC_ASCIITRFORBINFILE,
                          OperDefAsciiTrModeForBinFile);
}

//****************************************************************************
//
// COperDlgListView
//

COperDlgListView::COperDlgListView() : CWindow(ooStatic)
{
    OperDlg = NULL;
    HToolTip = NULL;
    LastItem = -1;
    LastSubItem = -1;
    LastWidth = 0;
    ConsOrItems = FALSE;
    Scrolling = FALSE;
    LastLButtonDownTime = GetTickCount() - 2000;
    LastLButtonDownLParam = -1;
}

COperDlgListView::~COperDlgListView()
{
    if (HToolTip != NULL)
        DestroyWindow(HToolTip);
}

#define TTS_NOANIMATE 0x10 // nemam to v headrech, takze tu konstantu nadefinuju tady

void COperDlgListView::Attach(HWND hListView, COperationDlg* operDlg, BOOL consOrItems)
{
    OperDlg = operDlg;
    ConsOrItems = consOrItems;
    AttachToWindow(hListView);

    HToolTip = CreateWindow(TOOLTIPS_CLASS, (LPSTR)NULL, TTS_ALWAYSTIP | TTS_NOPREFIX | TTS_NOANIMATE,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                            HWindow, (HMENU)NULL, DLLInstance, NULL);
    // default font vytahneme z dialogu
    HFONT dlgFont = (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0);
    if (dlgFont != NULL || SystemFont != NULL)
        SendMessage(HToolTip, WM_SETFONT, (WPARAM)(dlgFont != NULL ? dlgFont : SystemFont), TRUE);

    // zakomentoval jsem SetWindowPos(HWND_TOPMOST), protoze jinak messageboxy nad dialogem operace nefunguji
    // zcela korektne - pri Alt+TAB do messageboxu se automaticky neaktivuje (nevytahne nahoru)
    // dialog operace
    //  SetWindowPos(HToolTip, HWND_TOPMOST, 0, 0, 0, 0,
    //               SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOSIZE);

    SendMessage(HToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, 0);
    SendMessage(HToolTip, TTM_SETDELAYTIME, TTDT_RESHOW, 0);

    if (HToolTip != NULL)
    {
        static char emptyBuff[] = "";
        TOOLINFO ti;
        ti.cbSize = sizeof(ti);
        ti.hwnd = HWindow;
        ti.hinst = 0;
        ti.lpszText = emptyBuff;
        ti.uId = 0;
        ti.uFlags = TTF_TRANSPARENT;
        ti.rect.left = 0;
        ti.rect.top = 0;
        ti.rect.right = 100;
        ti.rect.bottom = 100;
        ::SendMessage(HToolTip, TTM_ADDTOOL, 0, (LPARAM)(LPTOOLINFO)&ti);
    }
}

LRESULT
COperDlgListView::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    {
        if (uMsg == WM_LBUTTONDOWN)
        {
            if (LastLButtonDownLParam == lParam &&                                       // mys je na stejnem miste
                (int)(GetTickCount() - LastLButtonDownTime) < (int)GetDoubleClickTime()) // kliknuti v limitu pro double-click
            {                                                                            // vygenerujeme WM_LBUTTONDBLCLK (je potreba na Viste)
                uMsg = WM_LBUTTONDBLCLK;
                LastLButtonDownTime = GetTickCount() - 2000; // aby na dalsi dvojklik nestacil jeden WM_LBUTTONDOWN
                break;
            }
            LastLButtonDownTime = GetTickCount(); // zapamatujeme si cas 1. kliknuti
            LastLButtonDownLParam = lParam;       // zapamatujeme si misto 1. kliknuti
        }
        if (HToolTip != NULL)
        {
            MSG msg;
            msg.wParam = wParam;
            msg.lParam = lParam;
            msg.message = uMsg;
            msg.hwnd = HWindow;
            ::SendMessage(HToolTip, TTM_RELAYEVENT, 0, (LPARAM)(LPMSG)&msg);
        }

        if (HToolTip != NULL && uMsg == WM_MOUSEMOVE)
        {
            LVHITTESTINFO hti;
            hti.pt.x = LOWORD(lParam);
            hti.pt.y = HIWORD(lParam);

            if (GetForegroundWindow() == OperDlg->HWindow &&
                ListView_SubItemHitTest(HWindow, &hti) != -1 &&
                (hti.iItem != LastItem || hti.iSubItem != LastSubItem) &&
                hti.iItem >= ListView_GetTopIndex(HWindow))
            {
                // vytahneme text z prislusneho sloupce a radku listview
                char buff[1000];
                buff[0] = 0;
                int index = hti.iItem;
                if (OperDlg->ShowOnlyErrors && index >= 0 && index < OperDlg->ErrorsIndexes.Count)
                    index = OperDlg->ErrorsIndexes[index];
                NMLVDISPINFO lvdi;
                lvdi.item.mask = LVIF_TEXT;
                lvdi.item.iSubItem = hti.iSubItem;
                lvdi.item.pszText = buff;
                lvdi.item.cchTextMax = 1000;
                if (ConsOrItems)
                    OperDlg->WorkersList->GetListViewDataFor(index, &lvdi, buff, 1000);
                else
                    OperDlg->Queue->GetListViewDataFor(index, &lvdi, buff, 1000);
                LastWidth = ListView_GetStringWidth(HWindow, buff);

                LastItem = hti.iItem;
                LastSubItem = hti.iSubItem;

                RECT rect;
                if (hti.iSubItem == 0)
                    ListView_GetItemRect(HWindow, hti.iItem, &rect, LVIR_LABEL);
                else
                    ListView_GetSubItemRect(HWindow, hti.iItem, hti.iSubItem, LVIR_BOUNDS, &rect);

                TOOLINFO ti;
                ti.cbSize = sizeof(ti);
                ti.hwnd = HWindow;
                ti.hinst = 0;
                ti.uId = 0;
                ti.uFlags = TTF_TRANSPARENT;
                ti.rect = rect;

                RECT cr;
                GetClientRect(HWindow, &cr);
                if (rect.right > cr.right)
                    rect.right = cr.right;

                if (LastWidth > rect.right - rect.left - (hti.iSubItem == 0 ? 4 : 12))
                    ti.lpszText = buff;
                else
                {
                    static char emptyBuff[] = "";
                    ti.lpszText = emptyBuff;
                    LastItem = -1;
                }
                ::SendMessage(HToolTip, TTM_SETTOOLINFO, 0, (LPARAM)(LPTOOLINFO)&ti);

                // nastavime proporcionalne delku zobrazeni tooltipu (delsi text = delsi zobrazeni)
                int len = (int)strlen(buff);
                len = max(100, (len * 10) / 4) * 80; // pouzil jsem Honzuv recept ;-)
                ::SendMessage(HToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, len);
            }
        }

        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* nmhdr = (NMHDR*)lParam;

        if (nmhdr->code == TTN_SHOW && nmhdr->hwndFrom == HToolTip && HToolTip != NULL)
        {
            RECT rect;
            if (LastSubItem == 0)
                ListView_GetItemRect(HWindow, LastItem, &rect, LVIR_LABEL);
            else
                ListView_GetSubItemRect(HWindow, LastItem, LastSubItem, LVIR_BOUNDS, &rect);

            POINT pt;
            int width = LastWidth + (WindowsVistaAndLater ? 11 : 7);
            pt.x = rect.left;
            pt.y = rect.top;
            ClientToScreen(HWindow, &pt);

            RECT scrRect;
            scrRect.left = pt.x;
            scrRect.right = rect.right - rect.left + pt.x;
            scrRect.top = pt.y;
            scrRect.bottom = rect.bottom - rect.top + pt.y;
            RECT monRect;
            SalamanderGeneral->MultiMonGetClipRectByRect(&scrRect, &monRect, NULL);

            if (WindowsVistaAndLater)
            {
                if (LastSubItem == 0)
                    pt.x -= 4;
            }
            else
            {
                if (LastSubItem == 0)
                    pt.x--;
                else
                    pt.x += 3;
            }

            if (pt.x + width > monRect.right)
                pt.x = monRect.right - width;

            // zakomentoval jsem SetWindowPos(HWND_TOPMOST), protoze jinak messageboxy nad dialogem operace nefunguji
            // zcela korektne - pri Alt+TAB do messageboxu se automaticky neaktivuje (nevytahne nahoru)
            // dialog operace
            /*
        SetWindowPos(HToolTip, HWND_TOPMOST,
                     pt.x,
                     pt.y,
                     width, rect.bottom - rect.top,
                     SWP_NOACTIVATE);
*/
            SetWindowPos(HToolTip, NULL,
                         pt.x,
                         pt.y,
                         width, rect.bottom - rect.top,
                         SWP_NOACTIVATE | SWP_NOZORDER);
            return TRUE;
        }
        break;
    }

    case WM_VSCROLL:
    {
        if (LOWORD(wParam) == SB_ENDSCROLL)
            Scrolling = FALSE;
        else
            Scrolling = TRUE;
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

void COperDlgListView::HideToolTip(int onlyIfOnIndex)
{
    if (HToolTip != NULL && (onlyIfOnIndex == -1 || onlyIfOnIndex == LastItem))
    {
        static char emptyBuff[] = "";
        TOOLINFO ti;
        ti.cbSize = sizeof(ti);
        ti.hwnd = HWindow;
        ti.hinst = 0;
        ti.uId = 0;
        ti.uFlags = TTF_TRANSPARENT;
        ti.lpszText = emptyBuff;
        ::SendMessage(HToolTip, TTM_SETTOOLINFO, 0, (LPARAM)(LPTOOLINFO)&ti);
        LastItem = -1;
    }
}

//
// ****************************************************************************
// CGetDiskFreeSpaceThread
//

CGetDiskFreeSpaceThread::CGetDiskFreeSpaceThread(const char* path, HWND dialog) : CThread("GetDiskFreeSpaceThread")
{
    HANDLES(InitializeCriticalSection(&GetFreeSpaceCritSect));
    lstrcpyn(Path, path, MAX_PATH);
    FreeSpace.Set(-1, -1);
    Dialog = dialog;
    WorkOrTerminate = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL)); // auto, non-signaled
    if (WorkOrTerminate == NULL)
        TRACE_E("Unable to create event WorkOrTerminate!");
    TerminateThread = FALSE;
}

CGetDiskFreeSpaceThread::~CGetDiskFreeSpaceThread()
{
    HANDLES(DeleteCriticalSection(&GetFreeSpaceCritSect));
    if (WorkOrTerminate != NULL)
        HANDLES(CloseHandle(WorkOrTerminate));
}

void CGetDiskFreeSpaceThread::ScheduleTerminate()
{
    CALL_STACK_MESSAGE1("CGetDiskFreeSpaceThread::Terminate()");

    HANDLES(EnterCriticalSection(&GetFreeSpaceCritSect));
    TerminateThread = TRUE;
    HANDLES(LeaveCriticalSection(&GetFreeSpaceCritSect));

    SetEvent(WorkOrTerminate);
}

void CGetDiskFreeSpaceThread::ScheduleGetDiskFreeSpace()
{
    SetEvent(WorkOrTerminate);
}

CQuadWord
CGetDiskFreeSpaceThread::GetResult()
{
    CALL_STACK_MESSAGE1("CGetDiskFreeSpaceThread::GetResult()");

    HANDLES(EnterCriticalSection(&GetFreeSpaceCritSect));
    CQuadWord res = FreeSpace;
    HANDLES(LeaveCriticalSection(&GetFreeSpaceCritSect));

    return res;
}

unsigned
CGetDiskFreeSpaceThread::Body()
{
    CALL_STACK_MESSAGE1("CGetDiskFreeSpaceThread::Body()");

    while (1)
    {
        DWORD res = WaitForSingleObject(WorkOrTerminate, INFINITE);

        // zjistime co mame delat
        HANDLES(EnterCriticalSection(&GetFreeSpaceCritSect));
        char path[MAX_PATH];
        BOOL terminate = TerminateThread;
        lstrcpyn(path, Path, MAX_PATH);
        HANDLES(LeaveCriticalSection(&GetFreeSpaceCritSect));
        if (terminate)
            break; // koncime...

        CQuadWord freeSpace;
        DWORD ti = GetTickCount();
        SalamanderGeneral->GetDiskFreeSpace(&freeSpace, path, NULL);
        if (GetTickCount() - ti > 10000)
            freeSpace.Set(-1, -1); // vysledky starsi deseti sekund hazime do kose (stejne uz je uplne jina situace)

        HANDLES(EnterCriticalSection(&GetFreeSpaceCritSect));
        FreeSpace = freeSpace;
        terminate = TerminateThread;
        HWND dialog = Dialog;
        HANDLES(LeaveCriticalSection(&GetFreeSpaceCritSect));
        if (terminate)
            break; // koncime...
        if (dialog != NULL)
            PostMessage(dialog, WM_APP_HAVEDISKFREESPACE, 0, 0); // posleme info o vysledcich a jdeme zase cekat...
    }

    return 0;
}

//
// ****************************************************************************
// CSolveItemErrorSimpleDlg
//

CSolveItemErrorSimpleDlg::CSolveItemErrorSimpleDlg(HWND parent, CFTPOperation* oper,
                                                   const char* ftpPath, const char* ftpName,
                                                   BOOL* applyToAll, CSolveItemErrorSimpleDlgType dlgType)
    : CCenteredDialog(HLanguage, IDD_SOLVEITEMSIMPLEERR, IDH_SOLVEERROR, parent)
{
    Oper = oper;
    FtpPath = ftpPath;
    FtpName = ftpName;
    ApplyToAll = applyToAll;
    DlgType = dlgType;
    UsedButtonID = 0;
}

void CSolveItemErrorSimpleDlg::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_SISE_APPLYTOALL, *ApplyToAll);
    if (ti.Type == ttDataToWindow)
    {
        ti.EditLine(IDE_SISE_PATH, (char*)FtpPath, FTP_MAX_PATH);
        ti.EditLine(IDE_SISE_NAME, (char*)FtpName, FTP_MAX_PATH);
    }
    else
    {
        if (UsedButtonID == 0)
            TRACE_E("CSolveItemErrUnkAttrDlg::Transfer(): unexpected call: UsedButtonID == 0!");
        if (*ApplyToAll)
        {
            int value = -1;
            switch (UsedButtonID)
            {
                // case CM_SISE_RETRY:  // u Retry to nema smysl

            case IDOK:
            {
                switch (DlgType)
                {
                case sisdtDelHiddenDir:
                    value = HIDDENDIRDEL_DELETEIT;
                    break;
                case sisdtDelHiddenFile:
                    value = HIDDENFILEDEL_DELETEIT;
                    break;
                case sisdtDelNonEmptyDir:
                    value = NONEMPTYDIRDEL_DELETEIT;
                    break;
                default:
                    TRACE_E("Unexpected situation in CSolveItemErrorSimpleDlg::Transfer(): unknown DlgType!");
                    break;
                }
                break;
            }

            case IDB_SISE_SKIP:
            {
                switch (DlgType)
                {
                case sisdtDelHiddenDir:
                    value = HIDDENDIRDEL_SKIP;
                    break;
                case sisdtDelHiddenFile:
                    value = HIDDENFILEDEL_SKIP;
                    break;
                case sisdtDelNonEmptyDir:
                    value = NONEMPTYDIRDEL_SKIP;
                    break;
                default:
                    TRACE_E("Unexpected situation in CSolveItemErrorSimpleDlg::Transfer(): unknown DlgType!");
                    break;
                }
                break;
            }
            }
            if (value != -1)
            {
                switch (DlgType)
                {
                case sisdtDelHiddenDir:
                    Oper->SetParamsForDeleteOper(NULL, NULL, &value);
                    break;
                case sisdtDelHiddenFile:
                    Oper->SetParamsForDeleteOper(NULL, &value, NULL);
                    break;
                case sisdtDelNonEmptyDir:
                    Oper->SetParamsForDeleteOper(&value, NULL, NULL);
                    break;
                default:
                    TRACE_E("Unexpected situation in CSolveItemErrorSimpleDlg::Transfer(): unknown DlgType!");
                    break;
                }
            }
        }
    }
}

INT_PTR
CSolveItemErrorSimpleDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSolveItemErrorSimpleDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        int titleID = IDS_SISE_TITLE1;
        switch (DlgType)
        {
        case sisdtDelHiddenDir:
            titleID = IDS_SISE_TITLE1;
            break;
        case sisdtDelHiddenFile:
            titleID = IDS_SISE_TITLE2;
            break;
        case sisdtDelNonEmptyDir:
            titleID = IDS_SISE_TITLE3;
            break;
        default:
            TRACE_E("Unexpected situation in CSolveItemErrorSimpleDlg::DialogProc(): unknown DlgType!");
            break;
        }
        SetWindowText(HWindow, LoadStr(titleID));
        SalamanderGUI->AttachButton(HWindow, IDOK, BTF_DROPDOWN);
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (LOWORD(wParam) == IDOK) // dropdown menu na Retry buttonu
        {
            int menuID = IDM_DELETEERRORRETRY;
            HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(menuID));
            if (main != NULL)
            {
                HMENU subMenu = GetSubMenu(main, 0);
                if (subMenu != NULL)
                {
                    CGUIMenuPopupAbstract* salMenu = SalamanderGUI->CreateMenuPopup();
                    if (salMenu != NULL)
                    {
                        salMenu->SetTemplateMenu(subMenu);

                        RECT r;
                        GetWindowRect(GetDlgItem(HWindow, (int)wParam), &r);
                        BOOL selectMenuItem = LOWORD(lParam);
                        DWORD flags = MENU_TRACK_RETURNCMD;
                        if (selectMenuItem)
                        {
                            salMenu->SetSelectedItemIndex(0);
                            flags |= MENU_TRACK_SELECT;
                        }
                        DWORD cmd = salMenu->Track(flags, r.left, r.bottom, HWindow, &r);
                        if (cmd != 0)
                            PostMessage(HWindow, WM_COMMAND, cmd, 0);
                        SalamanderGUI->DestroyMenuPopup(salMenu);
                    }
                }
                DestroyMenu(main);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        case CM_SISE_RETRY:
        case IDB_SISE_SKIP:
        {
            UsedButtonID = LOWORD(wParam);
            if (!ValidateData() ||
                !TransferData(ttDataFromWindow))
            {
                UsedButtonID = 0;
                return TRUE;
            }
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            UsedButtonID = 0;
            return TRUE;
        }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CSolveServerCmdErr
//

CSolveServerCmdErr::CSolveServerCmdErr(HWND parent, int titleID, const char* ftpPath,
                                       const char* ftpName, const char* errorDescr,
                                       BOOL* applyToAll, CSolveItemErrorSrvCmdDlgType dlgType)
    : CCenteredDialog(HLanguage, ftpName[0] == 0 ? IDD_SOLVESERVERCMDERR3 : IDD_SOLVESERVERCMDERR, IDH_SOLVEERROR, parent)
{
    TitleID = titleID;
    FtpPath = ftpPath;
    FtpName = ftpName;
    ErrorDescr = errorDescr;
    ApplyToAll = applyToAll;
    DlgType = dlgType;
}

void CSolveServerCmdErr::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        ti.EditLine(IDE_SSCD_PATH, (char*)FtpPath, FTP_MAX_PATH);
        if (FtpName[0] != 0)
            ti.EditLine(IDE_SSCD_NAME, (char*)FtpName, FTP_MAX_PATH);
        ti.EditLine(IDE_SSCD_ERRDSCR, (char*)ErrorDescr, FTP_MAX_PATH);
    }
    ti.CheckBox(IDC_SSCD_APPLYTOALL, *ApplyToAll);
}

INT_PTR
CSolveServerCmdErr::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSolveServerCmdErr::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetWindowText(HWindow, LoadStr(TitleID));
        if (DlgType != siscdtSimple)
            SalamanderGUI->AttachButton(HWindow, IDOK, BTF_DROPDOWN);
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (LOWORD(wParam) == IDOK && // dropdown menu na Retry buttonu
            (DlgType == siscdtDeleteFile || DlgType == siscdtDeleteDir))
        {
            int menuID = IDM_DELETEERRORONSRVRETRY;
            HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(menuID));
            if (main != NULL)
            {
                HMENU subMenu = GetSubMenu(main, 0);
                if (subMenu != NULL)
                {
                    CGUIMenuPopupAbstract* salMenu = SalamanderGUI->CreateMenuPopup();
                    if (salMenu != NULL)
                    {
                        salMenu->SetTemplateMenu(subMenu);

                        RECT r;
                        GetWindowRect(GetDlgItem(HWindow, (int)wParam), &r);
                        BOOL selectMenuItem = LOWORD(lParam);
                        DWORD flags = MENU_TRACK_RETURNCMD;
                        if (selectMenuItem)
                        {
                            salMenu->SetSelectedItemIndex(0);
                            flags |= MENU_TRACK_SELECT;
                        }
                        DWORD cmd = salMenu->Track(flags, r.left, r.bottom, HWindow, &r);
                        if (cmd != 0)
                            PostMessage(HWindow, WM_COMMAND, cmd, 0);
                        SalamanderGUI->DestroyMenuPopup(salMenu);
                    }
                }
                DestroyMenu(main);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_SSCD_SKIP:
        case CM_SSCD_MARKASDELETED:
        {
            if (!ValidateData() ||
                !TransferData(ttDataFromWindow))
                return TRUE;
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CSolveServerCmdErr2
//

CSolveServerCmdErr2::CSolveServerCmdErr2(HWND parent, int titleID, const char* ftpPath,
                                         const char* ftpName, const char* diskPath,
                                         const char* diskName, const char* errorDescr,
                                         BOOL* applyToAll, CSolveItemErrorSrvCmdDlgType2 dlgType)
    : CCenteredDialog(HLanguage, dlgType == siscdt2Simple ? IDD_SOLVESERVERCMDERR2 : IDD_SOLVESERVERCMDERR2EX,
                      IDH_SOLVEERROR, parent)
{
    TitleID = titleID;
    FtpPath = ftpPath;
    FtpName = ftpName;
    DiskPath = diskPath;
    DiskName = diskName;
    ErrorDescr = errorDescr;
    ApplyToAll = applyToAll;
    DlgType = dlgType;
}

void CSolveServerCmdErr2::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        if (DlgType == siscdt2UploadUnableToStore || DlgType == siscdt2UploadTestIfFinished)
        {
            ti.EditLine(IDE_SSCD_TGTPATH, (char*)FtpPath, FTP_MAX_PATH);
            ti.EditLine(IDE_SSCD_TGTNAME, (char*)FtpName, FTP_MAX_PATH);
            ti.EditLine(IDE_SSCD_SRCPATH, (char*)DiskPath, MAX_PATH);
            ti.EditLine(IDE_SSCD_SRCNAME, (char*)DiskName, MAX_PATH);
        }
        else
        {
            ti.EditLine(IDE_SSCD_SRCPATH, (char*)FtpPath, FTP_MAX_PATH);
            ti.EditLine(IDE_SSCD_SRCNAME, (char*)FtpName, FTP_MAX_PATH);
            ti.EditLine(IDE_SSCD_TGTPATH, (char*)DiskPath, MAX_PATH);
            ti.EditLine(IDE_SSCD_TGTNAME, (char*)DiskName, MAX_PATH);
        }
        ti.EditLine(IDE_SSCD_ERRDSCR, (char*)ErrorDescr, FTP_MAX_PATH);
    }
    ti.CheckBox(IDC_SSCD_APPLYTOALL, *ApplyToAll);
}

INT_PTR
CSolveServerCmdErr2::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSolveServerCmdErr2::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetWindowText(HWindow, LoadStr(TitleID));
        if (DlgType != siscdt2Simple)
            SalamanderGUI->AttachButton(HWindow, IDOK, BTF_DROPDOWN);
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (LOWORD(wParam) == IDOK && // dropdown menu na Retry buttonu
            (DlgType == siscdt2ResumeFile || DlgType == siscdt2ResumeTestFailed ||
             DlgType == siscdt2UploadUnableToStore || DlgType == siscdt2UploadTestIfFinished))
        {
            int menuID = DlgType == siscdt2ResumeFile || DlgType == siscdt2UploadUnableToStore ? IDM_RESUMEERRORONRETRY : DlgType == siscdt2UploadTestIfFinished ? IDM_UPLTESTIFFINISHEDONRETRY
                                                                                                                                                                 : IDM_RESUMETESTFAILEDONRETRY;
            HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(menuID));
            if (main != NULL)
            {
                HMENU subMenu = GetSubMenu(main, 0);
                if (subMenu != NULL)
                {
                    CGUIMenuPopupAbstract* salMenu = SalamanderGUI->CreateMenuPopup();
                    if (salMenu != NULL)
                    {
                        salMenu->SetTemplateMenu(subMenu);

                        RECT r;
                        GetWindowRect(GetDlgItem(HWindow, (int)wParam), &r);
                        BOOL selectMenuItem = LOWORD(lParam);
                        DWORD flags = MENU_TRACK_RETURNCMD;
                        if (selectMenuItem)
                        {
                            salMenu->SetSelectedItemIndex(0);
                            flags |= MENU_TRACK_SELECT;
                        }
                        DWORD cmd = salMenu->Track(flags, r.left, r.bottom, HWindow, &r);
                        if (cmd != 0)
                            PostMessage(HWindow, WM_COMMAND, cmd, 0);
                        SalamanderGUI->DestroyMenuPopup(salMenu);
                    }
                }
                DestroyMenu(main);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_SSCD_SKIP:
        case CM_SSCD_USEALTNAME:
        case CM_SSCD_RESUME:
        case CM_SSCD_RESUMEOROVR:
        case CM_SSCD_OVERWRITE:
        case CM_SSCD_REDUCEFILESIZE:
        case CM_SSCD_MARKASUPLOADED:
        {
            if (!ValidateData() ||
                !TransferData(ttDataFromWindow))
                return TRUE;
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}
