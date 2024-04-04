// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "edtlbwnd.h"
#include "usermenu.h"
#include "execute.h"
#include "plugins.h"
#include "zip.h"
#include "pack.h"
#include "gui.h"

//****************************************************************************
//
// CCfgPageArchivers
//

CCfgPageArchivers::CCfgPageArchivers()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_ARCHIVERS, IDD_CFGPAGE_ARCHIVERS, PSP_USETITLE, NULL)
{
}

void CCfgPageArchivers::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_ARCH_SIMPLEICONS, Configuration.UseSimpleIconsInArchives);
    ti.CheckBox(IDC_ARCH_ANOTHERPANELP, Configuration.UseAnotherPanelForPack);
    ti.CheckBox(IDC_ARCH_ANOTHERPANELU, Configuration.UseAnotherPanelForUnpack);
    ti.CheckBox(IDC_ARCH_ARCHIVENAME, Configuration.UseSubdirNameByArchiveForUnpack);
}

//****************************************************************************
//
// CCfgPagePackers
//

CCfgPagePackers::CCfgPagePackers()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_PACKERS, IDD_CFGPAGE_PACKERS, PSP_USETITLE, NULL)
{
    Config = new CPackerConfig /*(TRUE)*/; // without default values
    if (Config == NULL)
        return;
    Config->Load(PackerConfig);

    EditLB = NULL;
    DisableNotification = FALSE;
}

CCfgPagePackers::~CCfgPagePackers()
{
    if (Config != NULL)
        delete Config;
}

void CCfgPagePackers::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPagePackers::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        SendDlgItemMessage(HWindow, IDC_P1_TYPE, CB_ADDSTRING, 0,
                           (LPARAM)LoadStr(IDS_PUT_EXTERNAL));
        int count = 0;
        int index;
        while ((index = Plugins.GetCustomPackerIndex(count++)) != -1) // as long as there are "custom pack" plugins
        {
            CPluginData* p = Plugins.Get(index);
            if (p != NULL)
            {
                char buf[MAX_PATH];
                p->GetDisplayName(buf, MAX_PATH);
                SendDlgItemMessage(HWindow, IDC_P1_TYPE, CB_ADDSTRING, 0, (LPARAM)buf);
            }
            else
                TRACE_E("Unexpected situation in CCfgPagePackers::Transfer().");
        }
        int i;
        for (i = 0; i < Config->GetPackersCount(); i++)
            EditLB->AddItem();
        EditLB->SetCurSel(0);
    }
    else
    {
        PackerConfig.Load(*Config);
        if (PackerConfig.GetPreferedPacker() == -1)
            PackerConfig.SetPreferedPacker(0);
    }
}

void CCfgPagePackers::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPagePackers::Validate()");
    //  if (Dirty)
    //  {
    int i;
    for (i = 0; i < Config->GetPackersCount(); i++)
    {
        if (Config->GetPackerType(i) == CUSTOMPACKER_EXTERNAL)
        {
            if (!ValidatePathIsNotEmpty(HWindow, Config->GetPackerCmdExecCopy(i)))
            {
                EditLB->SetCurSel(i);
                ti.ErrorOn(IDC_P1_CPCMD);
                return;
            }
            if (Config->GetPackerSupMove(i))
            {
                if (!ValidatePathIsNotEmpty(HWindow, Config->GetPackerCmdExecMove(i)))
                {
                    EditLB->SetCurSel(i);
                    ti.ErrorOn(IDC_P1_MVCMD);
                    return;
                }
            }
        }
    }
    //  }
}

void CCfgPagePackers::LoadControls()
{
    CALL_STACK_MESSAGE1("CCfgPagePackers::LoadControls()");
    INT_PTR index;
    EditLB->GetCurSelItemID(index);
    BOOL empty = FALSE;
    if (index == -1)
        empty = TRUE;

    DisableNotification = TRUE;
    SendDlgItemMessage(HWindow, IDC_P1_EXT, WM_SETTEXT, 0,
                       (LPARAM)(empty ? "" : Config->GetPackerExt((int)index)));

    int type = empty ? 1 : Config->GetPackerType((int)index);

    int cmbSel = -1;
    switch (type)
    {
    case CUSTOMPACKER_EXTERNAL:
        cmbSel = 0;
        break;

    default:
    {
        if (type < 0)
        {
            cmbSel = Plugins.GetCustomPackerCount(-type - 1);
            if (cmbSel != -1)
                cmbSel++;
        }
    }
    }

    SendDlgItemMessage(HWindow, IDC_P1_TYPE, CB_SETCURSEL, cmbSel, 0);

    BOOL copy = !empty && type == CUSTOMPACKER_EXTERNAL;
    SendDlgItemMessage(HWindow, IDC_P1_CPCMD, WM_SETTEXT, 0,
                       (LPARAM)(copy ? Config->GetPackerCmdExecCopy((int)index) : ""));

    SendDlgItemMessage(HWindow, IDC_P1_CPARG, WM_SETTEXT, 0,
                       (LPARAM)(copy ? Config->GetPackerCmdArgsCopy((int)index) : ""));
    SendDlgItemMessage(HWindow, IDC_P1_CPARG, EM_SETSEL, 0, -1); // to overwrite the content of the browse

    BOOL move = copy && Config->GetPackerSupMove((int)index);

    CheckDlgButton(HWindow, IDC_P1_MOVE, move ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessage(HWindow, IDC_P1_MVCMD, WM_SETTEXT, 0,
                       (LPARAM)(move ? Config->GetPackerCmdExecMove((int)index) : ""));
    SendDlgItemMessage(HWindow, IDC_P1_MVARG, WM_SETTEXT, 0,
                       (LPARAM)(move ? Config->GetPackerCmdArgsMove((int)index) : ""));
    SendDlgItemMessage(HWindow, IDC_P1_MVARG, EM_SETSEL, 0, -1); // to overwrite the content of the browse

    BOOL longNames = !empty && Config->GetPackerSupLongNames((int)index);
    CheckDlgButton(HWindow, IDC_P1_LONG, longNames ? BST_CHECKED : BST_UNCHECKED);
    BOOL needANSI = !empty && Config->GetPackerNeedANSIListFile((int)index);
    CheckDlgButton(HWindow, IDC_P1_ANSI, needANSI ? BST_CHECKED : BST_UNCHECKED);

    DisableNotification = FALSE;
}

void CCfgPagePackers::StoreControls()
{
    CALL_STACK_MESSAGE1("CCfgPagePackers::StoreControls()");
    int index;
    EditLB->GetCurSel(index);
    if (!DisableNotification && index >= 0 && index < EditLB->GetCount())
    {
        int len = MAX_PATH + 2;
        char ext[MAX_PATH + 2];
        SendDlgItemMessage(HWindow, IDC_P1_EXT, WM_GETTEXT, len, (LPARAM)ext);
        int cmbSel = (int)SendDlgItemMessage(HWindow, IDC_P1_TYPE, CB_GETCURSEL, 0, 0);
        int type;
        switch (cmbSel)
        {
        case 0:
            type = CUSTOMPACKER_EXTERNAL;
            break;

        default:
        {
            type = Plugins.GetCustomPackerIndex(cmbSel - 1);
            if (type != -1)
                type = -type - 1;
            else
            {
                TRACE_E("Unexpected situation in CCfgPagePackers::StoreControls().");
                type = CUSTOMPACKER_EXTERNAL;
            }
            break;
        }
        }

        char execcopy[MAX_PATH + 2];
        execcopy[0] = 0;
        char argscopy[MAX_PATH + 2];
        argscopy[0] = 0;
        DWORD supmove = FALSE;
        char execmove[MAX_PATH + 2];
        execmove[0] = 0;
        char argsmove[MAX_PATH + 2];
        argsmove[0] = 0;
        DWORD suplong = FALSE;
        BOOL needANSIListFile = FALSE;
        if (type == CUSTOMPACKER_EXTERNAL)
        {
            SendDlgItemMessage(HWindow, IDC_P1_CPCMD, WM_GETTEXT, len, (LPARAM)execcopy);
            SendDlgItemMessage(HWindow, IDC_P1_CPARG, WM_GETTEXT, len, (LPARAM)argscopy);
            supmove = (IsDlgButtonChecked(HWindow, IDC_P1_MOVE) == BST_CHECKED);
            if (supmove)
            {
                SendDlgItemMessage(HWindow, IDC_P1_MVCMD, WM_GETTEXT, len, (LPARAM)execmove);
                SendDlgItemMessage(HWindow, IDC_P1_MVARG, WM_GETTEXT, len, (LPARAM)argsmove);
            }
            suplong = (IsDlgButtonChecked(HWindow, IDC_P1_LONG) == BST_CHECKED);
            needANSIListFile = (IsDlgButtonChecked(HWindow, IDC_P1_ANSI) == BST_CHECKED);
        }
        char title[MAX_PATH + 2];
        strcpy(title, Config->GetPackerTitle(index));
        Config->SetPacker(index, type, title, ext, FALSE,
                          suplong, supmove,
                          execcopy, argscopy,
                          execmove, argsmove, needANSIListFile);
    }
}

void CCfgPagePackers::EnableControls()
{
    CALL_STACK_MESSAGE1("CCfgPagePackers::EnableControls()");
    if (DisableNotification)
        return;
    INT_PTR index;
    EditLB->GetCurSelItemID(index);
    BOOL empty = FALSE;
    if (index == -1)
        empty = TRUE;

    EnableWindow(GetDlgItem(HWindow, IDC_P1_EXT), !empty);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_TYPE), !empty);

    int cmbSel;
    if (!empty)
        cmbSel = (int)SendDlgItemMessage(HWindow, IDC_P1_TYPE, CB_GETCURSEL, 0, 0);

    BOOL external = !empty && cmbSel == 0; //AskExternal
    BOOL copy = !empty && external;
    EnableWindow(GetDlgItem(HWindow, IDC_P1_CPCMD), copy);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_CPCMD_BROWSE), copy);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_CPARG), copy);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_CPARG_BROWSE), copy);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_MOVE), copy);

    BOOL move = FALSE;
    if (!empty)
        move = external && (IsDlgButtonChecked(HWindow, IDC_P1_MOVE) == BST_CHECKED);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_MVCMD), move);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_MVCMD_BROWSE), move);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_MVARG), move);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_MVARG_BROWSE), move);

    EnableWindow(GetDlgItem(HWindow, IDC_P1_LONG), copy);
    EnableWindow(GetDlgItem(HWindow, IDC_P1_ANSI), copy);
}

INT_PTR
CCfgPagePackers::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPagePackers::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        EditLB = new CEditListBox(HWindow, IDC_P1_LIST, ELB_ITEMINDEXES);
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        EditLB->MakeHeader(IDC_P1_NAME);
        EditLB->EnableDrag(::GetParent(HWindow));
        ChangeToArrowButton(HWindow, IDC_P1_CPARG_BROWSE);
        ChangeToArrowButton(HWindow, IDC_P1_MVARG_BROWSE);
        ChangeToArrowButton(HWindow, IDC_P1_CPCMD_BROWSE);
        ChangeToArrowButton(HWindow, IDC_P1_MVCMD_BROWSE);
        SendDlgItemMessage(HWindow, IDC_P1_CPCMD, EM_LIMITTEXT, MAX_PATH, 0);
        SendDlgItemMessage(HWindow, IDC_P1_CPARG, EM_LIMITTEXT, MAX_PATH, 0);
        SendDlgItemMessage(HWindow, IDC_P1_MVCMD, EM_LIMITTEXT, MAX_PATH, 0);
        SendDlgItemMessage(HWindow, IDC_P1_MVARG, EM_LIMITTEXT, MAX_PATH, 0);

        // Dialog elements should stretch according to its size, let's set the dividing controls
        ElasticVerticalLayout(1, IDC_P1_LIST);

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_CHANGE ||
            HIWORD(wParam) == BN_CLICKED ||
            LOWORD(wParam) == IDC_P1_TYPE && HIWORD(wParam) == CBN_SELCHANGE)
        {
            StoreControls();
            EnableControls();
        }

        if (LOWORD(wParam) == IDC_P1_LIST && HIWORD(wParam) == LBN_SELCHANGE)
        {
            EditLB->OnSelChanged();
            LoadControls();
            EnableControls();
        }

        switch (LOWORD(wParam))
        {
        case IDC_P1_CPCMD_BROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_P1_CPCMD_BROWSE, IDC_P1_CPCMD, FALSE,
                             CmdCustomPackers, IDS_EXEFILTER);
            return 0;
        }

        case IDC_P1_CPARG_BROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_P1_CPARG_BROWSE, IDC_P1_CPARG, FALSE,
                             ArgsCustomPackers);
            return 0;
        }

        case IDC_P1_MVCMD_BROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_P1_MVCMD_BROWSE, IDC_P1_MVCMD, FALSE,
                             CmdCustomPackers, IDS_EXEFILTER);
            return 0;
        }

        case IDC_P1_MVARG_BROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_P1_MVARG_BROWSE, IDC_P1_MVARG, FALSE,
                             ArgsCustomPackers);
            return 0;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDC_P1_LIST:
        {
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ToDo == edtlbGetData)
                {
                    strcpy(dispInfo->Buffer, Config->GetPackerTitle((int)dispInfo->ItemID));
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                    return TRUE;
                }
                else
                {
                    if (dispInfo->ItemID == -1)
                    {
                        int index = Config->AddPacker();
                        if (index == -1)
                        {
                            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                        Config->SetPacker(index, CUSTOMPACKER_EXTERNAL, dispInfo->Buffer, "",
                                          FALSE, TRUE, TRUE, "", "", "", "", FALSE);
                        EditLB->SetItemData();
                    }
                    else
                    {
                        int index = (int)dispInfo->ItemID;
                        Config->SetPackerTitle(index, dispInfo->Buffer);
                    }

                    LoadControls();
                    EnableControls();
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break;
            }
                /*              case EDTLBN_MOVEITEM:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              int index;
              EditLB->GetCurSel(index);
              Config->SwapPackers(index, index + (dispInfo->Up ? -1 : 1));
              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // allow swapping
              return TRUE;
            }*/
            case EDTLBN_MOVEITEM2:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                Config->MovePacker(index, dispInfo->NewIndex);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow change
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                int index;
                EditLB->GetCurSel(index);
                Config->DeletePacker(index);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow deletion
                return TRUE;
            }
            }
            break;
        }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        int idCtrl = (int)wParam;
        if (idCtrl == IDC_P1_LIST)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CCfgPageUnpackers
//

CCfgPageUnpackers::CCfgPageUnpackers()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_UNPACKERS, IDD_CFGPAGE_UNPACKERS, PSP_USETITLE, NULL)
{
    Config = new CUnpackerConfig /*(TRUE)*/; // without default values
    if (Config == NULL)
        return;
    Config->Load(UnpackerConfig);

    EditLB = NULL;
    DisableNotification = FALSE;
}

CCfgPageUnpackers::~CCfgPageUnpackers()
{
    if (Config != NULL)
        delete Config;
}

void CCfgPageUnpackers::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageUnpackers::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        SendDlgItemMessage(HWindow, IDC_P2_TYPE, CB_ADDSTRING, 0,
                           (LPARAM)LoadStr(IDS_PUT_EXTERNAL));
        int count = 0;
        int index;
        while ((index = Plugins.GetCustomUnpackerIndex(count++)) != -1) // as long as there are "custom unpack" plugins
        {
            CPluginData* p = Plugins.Get(index);
            if (p != NULL)
            {
                char buf[MAX_PATH];
                p->GetDisplayName(buf, MAX_PATH);
                SendDlgItemMessage(HWindow, IDC_P2_TYPE, CB_ADDSTRING, 0, (LPARAM)buf);
            }
            else
                TRACE_E("Unexpected situation in CCfgPageUnpackers::Transfer().");
        }

        int i;
        for (i = 0; i < Config->GetUnpackersCount(); i++)
            EditLB->AddItem();
        EditLB->SetCurSel(0);
    }
    else
    {
        UnpackerConfig.Load(*Config);
        if (UnpackerConfig.GetPreferedUnpacker() == -1)
            UnpackerConfig.SetPreferedUnpacker(0);
    }
}

void CCfgPageUnpackers::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageUnpackers::Validate()");
    //  if (Dirty)
    //  {
    int i;
    for (i = 0; i < Config->GetUnpackersCount(); i++)
    {
        if (Config->GetUnpackerType(i) == CUSTOMUNPACKER_EXTERNAL)
        {
            if (!ValidatePathIsNotEmpty(HWindow, Config->GetUnpackerCmdExecExtract(i)))
            {
                EditLB->SetCurSel(i);
                ti.ErrorOn(IDC_P2_EXCMD);
                return;
            }
        }

        char masksStr[MAX_PATH];
        strcpy(masksStr, Config->GetUnpackerExt(i));
        char* iterator;
        if (strlen(masksStr) > 0)
        {
            iterator = masksStr + strlen(masksStr) - 1;
            while (iterator >= masksStr && *iterator == ' ')
            {
                *iterator = 0;
                iterator--;
            }
        }
        if (strlen(masksStr) == 0)
        {
            EditLB->SetCurSel(i);
            SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_P2_EXT);
            return;
        }

        CMaskGroup masks(masksStr);
        int errorPos;
        if (!masks.PrepareMasks(errorPos))
        {
            EditLB->SetCurSel(i);
            SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            SendMessage(GetDlgItem(HWindow, IDC_P2_EXT), EM_SETSEL, errorPos, errorPos + 1);
            ti.ErrorOn(IDC_P2_EXT);
            return;
        }
    }
    //  }
}

void CCfgPageUnpackers::LoadControls()
{
    CALL_STACK_MESSAGE1("CCfgPageUnpackers::LoadControls()");
    INT_PTR index;
    EditLB->GetCurSelItemID(index);
    BOOL empty = FALSE;
    if (index == -1)
        empty = TRUE;

    DisableNotification = TRUE;
    SendDlgItemMessage(HWindow, IDC_P2_EXT, WM_SETTEXT, 0,
                       (LPARAM)(empty ? "" : Config->GetUnpackerExt((int)index)));

    int type = empty ? 1 : Config->GetUnpackerType((int)index);

    int cmbSel = -1;
    switch (type)
    {
    case CUSTOMUNPACKER_EXTERNAL:
        cmbSel = 0;
        break;

    default:
    {
        if (type < 0)
        {
            cmbSel = Plugins.GetCustomUnpackerCount(-type - 1);
            if (cmbSel != -1)
                cmbSel++;
        }
    }
    }

    SendDlgItemMessage(HWindow, IDC_P2_TYPE, CB_SETCURSEL, cmbSel, 0);

    BOOL copy = !empty && type == CUSTOMUNPACKER_EXTERNAL;
    SendDlgItemMessage(HWindow, IDC_P2_EXCMD, WM_SETTEXT, 0,
                       (LPARAM)(copy ? Config->GetUnpackerCmdExecExtract((int)index) : ""));
    SendDlgItemMessage(HWindow, IDC_P2_EXARG, WM_SETTEXT, 0,
                       (LPARAM)(copy ? Config->GetUnpackerCmdArgsExtract((int)index) : ""));
    SendDlgItemMessage(HWindow, IDC_P2_EXARG, EM_SETSEL, 0, -1); // to overwrite the content of the browse

    BOOL longNames = !empty && Config->GetUnpackerSupLongNames((int)index);
    CheckDlgButton(HWindow, IDC_P2_LONG, longNames ? BST_CHECKED : BST_UNCHECKED);
    BOOL needANSI = !empty && Config->GetUnpackerNeedANSIListFile((int)index);
    CheckDlgButton(HWindow, IDC_P2_ANSI, needANSI ? BST_CHECKED : BST_UNCHECKED);

    DisableNotification = FALSE;
}

void CCfgPageUnpackers::StoreControls()
{
    CALL_STACK_MESSAGE1("CCfgPageUnpackers::StoreControls()");
    int index;
    EditLB->GetCurSel(index);
    if (!DisableNotification && index >= 0 && index < EditLB->GetCount())
    {
        int len = MAX_PATH + 2;
        char ext[MAX_PATH + 2];
        SendDlgItemMessage(HWindow, IDC_P2_EXT, WM_GETTEXT, len, (LPARAM)ext);
        int cmbSel = (int)SendDlgItemMessage(HWindow, IDC_P2_TYPE, CB_GETCURSEL, 0, 0);
        int type;
        switch (cmbSel)
        {
        case 0:
            type = CUSTOMUNPACKER_EXTERNAL;
            break;

        default:
        {
            type = Plugins.GetCustomUnpackerIndex(cmbSel - 1);
            if (type != -1)
                type = -type - 1;
            else
            {
                TRACE_E("Unexpected situation in CCfgPageUnpackers::StoreControls().");
                type = CUSTOMUNPACKER_EXTERNAL;
            }
            break;
        }
        }

        char execcopy[MAX_PATH + 2];
        execcopy[0] = 0;
        char argscopy[MAX_PATH + 2];
        argscopy[0] = 0;
        DWORD suplong = FALSE;
        BOOL needANSI = FALSE;
        if (type == CUSTOMUNPACKER_EXTERNAL)
        {
            SendDlgItemMessage(HWindow, IDC_P2_EXCMD, WM_GETTEXT, len, (LPARAM)execcopy);
            SendDlgItemMessage(HWindow, IDC_P2_EXARG, WM_GETTEXT, len, (LPARAM)argscopy);
            suplong = (IsDlgButtonChecked(HWindow, IDC_P2_LONG) == BST_CHECKED);
            needANSI = (IsDlgButtonChecked(HWindow, IDC_P2_ANSI) == BST_CHECKED);
        }
        char title[MAX_PATH + 2];
        strcpy(title, Config->GetUnpackerTitle(index));
        Config->SetUnpacker(index, type, title, ext, FALSE,
                            suplong,
                            execcopy, argscopy, needANSI);
    }
}

void CCfgPageUnpackers::EnableControls()
{
    CALL_STACK_MESSAGE1("CCfgPageUnpackers::EnableControls()");
    if (DisableNotification)
        return;
    INT_PTR index;
    EditLB->GetCurSelItemID(index);
    BOOL empty = FALSE;
    if (index == -1)
        empty = TRUE;

    EnableWindow(GetDlgItem(HWindow, IDC_P2_EXT), !empty);
    EnableWindow(GetDlgItem(HWindow, IDC_P2_TYPE), !empty);

    int cmbSel;
    if (!empty)
        cmbSel = (int)SendDlgItemMessage(HWindow, IDC_P2_TYPE, CB_GETCURSEL, 0, 0);

    BOOL external = !empty && cmbSel == 0; //External exit
    BOOL copy = !empty && external;
    EnableWindow(GetDlgItem(HWindow, IDC_P2_EXCMD), copy);
    EnableWindow(GetDlgItem(HWindow, IDC_P2_EXCMD_BROWSE), copy);
    EnableWindow(GetDlgItem(HWindow, IDC_P2_EXARG), copy);
    EnableWindow(GetDlgItem(HWindow, IDC_P2_EXARG_BROWSE), copy);

    EnableWindow(GetDlgItem(HWindow, IDC_P2_LONG), copy);
    EnableWindow(GetDlgItem(HWindow, IDC_P2_ANSI), copy);
}

INT_PTR
CCfgPageUnpackers::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageUnpackers::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        EditLB = new CEditListBox(HWindow, IDC_P2_LIST, ELB_ITEMINDEXES);
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        EditLB->MakeHeader(IDC_P2_NAME);
        EditLB->EnableDrag(::GetParent(HWindow));
        ChangeToArrowButton(HWindow, IDC_P2_EXARG_BROWSE);
        ChangeToArrowButton(HWindow, IDC_P2_EXCMD_BROWSE);
        SendDlgItemMessage(HWindow, IDC_P2_EXCMD, EM_LIMITTEXT, MAX_PATH, 0);
        SendDlgItemMessage(HWindow, IDC_P2_EXARG, EM_LIMITTEXT, MAX_PATH, 0);

        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_MASKS_HINT));

        // Dialog elements should stretch according to its size, let's set the dividing controls
        ElasticVerticalLayout(1, IDC_P2_LIST);

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_CHANGE ||
            HIWORD(wParam) == BN_CLICKED ||
            LOWORD(wParam) == IDC_P2_TYPE && HIWORD(wParam) == CBN_SELCHANGE)
        {
            StoreControls();
            EnableControls();
        }

        if (LOWORD(wParam) == IDC_P2_LIST && HIWORD(wParam) == LBN_SELCHANGE)
        {
            EditLB->OnSelChanged();
            LoadControls();
            EnableControls();
        }

        switch (LOWORD(wParam))
        {
        case IDC_P2_EXCMD_BROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_P2_EXCMD_BROWSE, IDC_P2_EXCMD, FALSE,
                             CmdCustomPackers, IDS_EXEFILTER);
            return 0;
        }

        case IDC_P2_EXARG_BROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_P2_EXARG_BROWSE, IDC_P2_EXARG, FALSE,
                             ArgsCustomPackers);
            return 0;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDC_P2_LIST:
        {
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ToDo == edtlbGetData)
                {
                    strcpy(dispInfo->Buffer, Config->GetUnpackerTitle((int)dispInfo->ItemID));
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                    return TRUE;
                }
                else
                {
                    if (dispInfo->ItemID == -1)
                    {
                        int index = Config->AddUnpacker();
                        if (index == -1)
                        {
                            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                        Config->SetUnpacker(index, CUSTOMUNPACKER_EXTERNAL, dispInfo->Buffer, "",
                                            FALSE, TRUE, "", "", FALSE);
                        EditLB->SetItemData();
                    }
                    else
                    {
                        int index = (int)dispInfo->ItemID;
                        Config->SetUnpackerTitle(index, dispInfo->Buffer);
                    }

                    LoadControls();
                    EnableControls();
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break;
            }
                /*              case EDTLBN_MOVEITEM:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              int index;
              EditLB->GetCurSel(index);
              Config->SwapUnpackers(index, index + (dispInfo->Up ? -1 : 1));
              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // povolim prohozeni
              return TRUE;
            }
*/
            case EDTLBN_MOVEITEM2:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                Config->MoveUnpacker(index, dispInfo->NewIndex);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow change
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                int index;
                EditLB->GetCurSel(index);
                Config->DeleteUnpacker(index);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow deletion
                return TRUE;
            }
            }
            break;
        }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        int idCtrl = (int)wParam;
        if (idCtrl == IDC_P2_LIST)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CCfgPageExternalArchivers
//

CCfgPageExternalArchivers::CCfgPageExternalArchivers()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_ARCHIVERSLOCATIONS, IDD_CFGPAGE_ARCHIVERSLOCATIONS, PSP_USETITLE, NULL)
{
    Config = new CArchiverConfig /*(TRUE)*/; // without default values
    if (Config == NULL)
        return;
    Config->Load(ArchiverConfig);

    DisableNotification = FALSE;
}

CCfgPageExternalArchivers::~CCfgPageExternalArchivers()
{
    if (Config != NULL)
        delete Config;
}

void CCfgPageExternalArchivers::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageExternalArchivers::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        int i;
        for (i = 0; i < Config->GetArchiversCount(); i++)
            SendMessage(HListbox, LB_ADDSTRING, 0, (LPARAM)Config->GetArchiverTitle(i));
        SendMessage(HListbox, LB_SETCURSEL, 0, 0);
        LoadControls();
        EnableControls();
    }
    else
    {
        ArchiverConfig.Load(*Config);
    }
}

void CCfgPageExternalArchivers::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageExternalArchivers::Validate()");
    //  if (Dirty)
    //  {
    int i;
    for (i = 0; i < Config->GetArchiversCount(); i++)
    {
        int errorPos1, errorPos2;
        if (!ValidateCommandFile(HWindow, Config->ArchiverExesAreSame(i) ? (Config->GetPackerExeFile(i) != NULL ? Config->GetPackerExeFile(i) : "") : (Config->GetUnpackerExeFile(i) != NULL ? Config->GetUnpackerExeFile(i) : ""), errorPos1, errorPos2))
        {
            SendMessage(HListbox, LB_SETCURSEL, i, 0);
            LoadControls();
            EnableControls();
            ti.ErrorOn(IDC_P3_VIEW);
            PostMessage(GetDlgItem(HWindow, IDC_P3_VIEW), EM_SETSEL, errorPos1, errorPos2);
            return;
        }
        if (!Config->ArchiverExesAreSame(i))
        {
            if (!ValidateCommandFile(HWindow,
                                     (Config->GetPackerExeFile(i) != NULL ? Config->GetPackerExeFile(i) : ""), errorPos1, errorPos2))
            {
                SendMessage(HListbox, LB_SETCURSEL, i, 0);
                LoadControls();
                EnableControls();
                ti.ErrorOn(IDC_P3_EDIT);
                PostMessage(GetDlgItem(HWindow, IDC_P3_EDIT), EM_SETSEL, errorPos1, errorPos2);
                return;
            }
        }
    }
    //  }
}

void CCfgPageExternalArchivers::LoadControls()
{
    CALL_STACK_MESSAGE1("CCfgPageExternalArchivers::LoadControls()");
    int index = (int)SendMessage(HListbox, LB_GETCURSEL, 0, 0);
    BOOL empty = FALSE;
    if (index == -1)
        empty = TRUE;

    BOOL old = DisableNotification;
    DisableNotification = TRUE;

    BOOL same = !empty && Config->ArchiverExesAreSame(index);
    SendDlgItemMessage(HWindow, IDC_P3_VIEW, WM_SETTEXT, 0,
                       (LPARAM)(empty ? "" : same ? (Config->GetPackerExeFile(index) != NULL ? Config->GetPackerExeFile(index) : "")
                                                  : (Config->GetUnpackerExeFile(index) != NULL ? Config->GetUnpackerExeFile(index) : "")));
    SendDlgItemMessage(HWindow, IDC_P3_EDIT, WM_SETTEXT, 0,
                       (LPARAM)(empty ? "" : (Config->GetPackerExeFile(index) != NULL ? Config->GetPackerExeFile(index) : "")));
    DisableNotification = old;
}

void CCfgPageExternalArchivers::StoreControls()
{
    CALL_STACK_MESSAGE1("CCfgPageExternalArchivers::StoreControls()");
    int index = (int)SendMessage(HListbox, LB_GETCURSEL, 0, 0);
    if (index == -1)
        return;

    if (!DisableNotification)
    {
        int len = MAX_PATH + 2;
        char view[MAX_PATH + 2];
        view[0] = 0;
        char edit[MAX_PATH + 2];
        edit[0] = 0;
        SendDlgItemMessage(HWindow, IDC_P3_VIEW, WM_GETTEXT, len, (LPARAM)view);
        BOOL same = Config->ArchiverExesAreSame(index);
        if (!same)
            SendDlgItemMessage(HWindow, IDC_P3_EDIT, WM_GETTEXT, len, (LPARAM)edit);
        else
            strcpy(edit, view);

        Config->SetPackerExeFile(index, edit);
        Config->SetUnpackerExeFile(index, view);
    }
}

void CCfgPageExternalArchivers::EnableControls()
{
    CALL_STACK_MESSAGE1("CCfgPageExternalArchivers::EnableControls()");
    if (DisableNotification)
        return;
    int index = (int)SendMessage(HListbox, LB_GETCURSEL, 0, 0);
    BOOL empty = FALSE;
    if (index == -1)
        empty = TRUE;
    BOOL same = FALSE;
    if (index != -1)
        same = Config->ArchiverExesAreSame(index);
    EnableWindow(GetDlgItem(HWindow, IDC_P3_EDIT), !same);
    EnableWindow(GetDlgItem(HWindow, IDC_P3_EDIT_BROWSE), !same);
}

INT_PTR
CCfgPageExternalArchivers::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageExternalArchivers::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HListbox = GetDlgItem(HWindow, IDC_P3_LIST);
        HFONT hFont = (HFONT)SendMessage(HListbox, WM_GETFONT, 0, 0);
        TEXTMETRIC tm;
        HDC hdc = HANDLES(GetDC(HWindow));
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        GetTextMetrics(hdc, &tm);
        SelectObject(hdc, hOldFont);
        HANDLES(ReleaseDC(HWindow, hdc));
        SendMessage(HListbox, LB_SETITEMHEIGHT, 0, MAKELPARAM(tm.tmHeight + 1, 0));

        SendDlgItemMessage(HWindow, IDC_P3_VIEW, EM_LIMITTEXT, MAX_PATH, 0);
        SendDlgItemMessage(HWindow, IDC_P3_EDIT, EM_LIMITTEXT, MAX_PATH, 0);

        ChangeToArrowButton(HWindow, IDC_P3_VIEW_BROWSE);
        ChangeToArrowButton(HWindow, IDC_P3_EDIT_BROWSE);

        // Dialog elements should stretch according to its size, let's set the dividing controls
        ElasticVerticalLayout(1, IDC_P3_LIST);

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_CHANGE)
        {
            StoreControls();

            if (!DisableNotification)
            {
                int index = (int)SendMessage(HListbox, LB_GETCURSEL, 0, 0);
                if (index != -1)
                {
                    BOOL same = Config->ArchiverExesAreSame(index);
                    if (!DisableNotification && same)
                    {
                        char edit[MAX_PATH + 2];
                        edit[0] = 0;
                        SendDlgItemMessage(HWindow, IDC_P3_VIEW, WM_GETTEXT, MAX_PATH, (LPARAM)edit);
                        BOOL old = DisableNotification;
                        DisableNotification = TRUE;
                        SendDlgItemMessage(HWindow, IDC_P3_EDIT, WM_SETTEXT, 0, (LPARAM)edit);
                        DisableNotification = old;
                    }
                }
            }
            EnableControls();
        }

        if (LOWORD(wParam) == IDC_P3_LIST && HIWORD(wParam) == LBN_SELCHANGE)
        {
            LoadControls();
            EnableControls();
        }

        switch (LOWORD(wParam))
        {
        case IDC_P3_VIEW_BROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_P3_VIEW_BROWSE, IDC_P3_VIEW, FALSE,
                             CommandExecutes, IDS_EXEFILTER);
            return 0;
        }

        case IDC_P3_EDIT_BROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_P3_EDIT_BROWSE, IDC_P3_EDIT, FALSE,
                             CommandExecutes, IDS_EXEFILTER);
            return 0;
        }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CCfgPageArchivesAssoc
//

CCfgPageArchivesAssoc::CCfgPageArchivesAssoc()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_ASSOCIATIONS, IDD_CFGPAGE_ASSOCIATIONS, PSP_USETITLE, NULL)
{
    Config = new CPackerFormatConfig /*(TRUE)*/; // without default values
    if (Config == NULL)
        return;
    Config->Load(PackerFormatConfig);

    EditLB = NULL;
    DisableNotification = FALSE;
}

CCfgPageArchivesAssoc::~CCfgPageArchivesAssoc()
{
    if (Config != NULL)
        delete Config;
}

void CCfgPageArchivesAssoc::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageArchivesAssoc::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        const char* s;
        int i;
        for (i = 0; i < ArchiverConfig.GetArchiversCount(); i++)
        {
            s = ArchiverConfig.GetArchiverTitle(i);
            SendDlgItemMessage(HWindow, IDC_P4_VIEW, CB_ADDSTRING, 0, (LPARAM)s);
            SendDlgItemMessage(HWindow, IDC_P4_EDIT, CB_ADDSTRING, 0, (LPARAM)s);
        }

        int count = 0;
        int index;
        char buf[MAX_PATH];
        while ((index = Plugins.GetPanelViewIndex(count++)) != -1) // as long as there are "panel view" plug-ins
        {
            CPluginData* p = Plugins.Get(index);
            if (p != NULL)
            {
                p->GetDisplayName(buf, MAX_PATH);
                SendDlgItemMessage(HWindow, IDC_P4_VIEW, CB_ADDSTRING, 0, (LPARAM)buf);
            }
            else
                TRACE_E("Unexpected situation in CCfgPageArchivesAssoc::Transfer().");
        }

        count = 0;
        while ((index = Plugins.GetPanelEditIndex(count++)) != -1) // as long as there are "panel edit" plug-ins
        {
            CPluginData* p = Plugins.Get(index);
            if (p != NULL)
            {
                p->GetDisplayName(buf, MAX_PATH);
                SendDlgItemMessage(HWindow, IDC_P4_EDIT, CB_ADDSTRING, 0, (LPARAM)buf);
            }
            else
                TRACE_E("Unexpected situation in CCfgPageArchivesAssoc::Transfer().");
        }

        s = LoadStr(IDS_PUT_NONE);
        SendDlgItemMessage(HWindow, IDC_P4_EDIT, CB_ADDSTRING, 0, (LPARAM)s);

        for (i = 0; i < Config->GetFormatsCount(); i++)
            EditLB->AddItem();
        EditLB->SetCurSel(0);
    }
    else
    {
        PackerFormatConfig.Load(*Config);
        PackerFormatConfig.BuildArray();
    }
}

void CCfgPageArchivesAssoc::LoadControls()
{
    CALL_STACK_MESSAGE1("CCfgPageArchivesAssoc::LoadControls()");
    INT_PTR index;
    EditLB->GetCurSelItemID(index);
    BOOL empty = 0;
    if (index == -1)
        empty = TRUE;

    DisableNotification = TRUE;

    int unpackIndex = -1;
    int packIndex = -1;
    if (!empty)
    {
        unpackIndex = Config->GetUnpackerIndex((int)index);
        if (unpackIndex < 0)
        {
            unpackIndex = Plugins.GetPanelViewCount(-unpackIndex - 1);
            if (unpackIndex != -1)
                unpackIndex += ArchiverConfig.GetArchiversCount();
        }

        if (Config->GetUsePacker((int)index))
        {
            packIndex = Config->GetPackerIndex((int)index);
            if (packIndex < 0)
            {
                packIndex = Plugins.GetPanelEditCount(-packIndex - 1);
                if (packIndex != -1)
                    packIndex += ArchiverConfig.GetArchiversCount();
            }
        }
        else
        {
            packIndex = (int)SendDlgItemMessage(HWindow, IDC_P4_EDIT, CB_GETCOUNT, 0, 0) - 1; // not supported
        }
    }
    SendDlgItemMessage(HWindow, IDC_P4_VIEW, CB_SETCURSEL, unpackIndex, 0);
    SendDlgItemMessage(HWindow, IDC_P4_EDIT, CB_SETCURSEL, packIndex, 0);

    DisableNotification = FALSE;
}

void CCfgPageArchivesAssoc::StoreControls()
{
    CALL_STACK_MESSAGE1("CCfgPageArchivesAssoc::StoreControls()");
    int index;
    EditLB->GetCurSel(index);
    if (!DisableNotification && index >= 0 && index < EditLB->GetCount())
    {
        int unpackIndex = (int)SendDlgItemMessage(HWindow, IDC_P4_VIEW, CB_GETCURSEL, 0, 0);
        int packIndex = (int)SendDlgItemMessage(HWindow, IDC_P4_EDIT, CB_GETCURSEL, 0, 0);

        if (unpackIndex >= ArchiverConfig.GetArchiversCount())
        {
            unpackIndex = Plugins.GetPanelViewIndex(unpackIndex - ArchiverConfig.GetArchiversCount());
            if (unpackIndex != -1)
                unpackIndex = -unpackIndex - 1;
            else
            {
                TRACE_E("Unexpected situation in CCfgPageArchivesAssoc::StoreControls().");
                unpackIndex = 0;
            }
        }

        BOOL usePacker = TRUE;
        if (packIndex >= ArchiverConfig.GetArchiversCount())
        {
            packIndex = Plugins.GetPanelEditIndex(packIndex - ArchiverConfig.GetArchiversCount());
            if (packIndex != -1)
                packIndex = -packIndex - 1;
            else
                usePacker = FALSE;
        }

        char ext[MAX_PATH + 2];
        strcpy(ext, Config->GetExt(index));
        Config->SetFormat(index, ext, usePacker, packIndex, unpackIndex, FALSE);
    }
}

void CCfgPageArchivesAssoc::EnableControls()
{
    CALL_STACK_MESSAGE1("CCfgPageArchivesAssoc::EnableControls()");
    INT_PTR index;
    EditLB->GetCurSelItemID(index);
    BOOL empty = 0;
    if (index == -1)
        empty = TRUE;

    EnableWindow(GetDlgItem(HWindow, IDC_P4_VIEW), !empty);
    EnableWindow(GetDlgItem(HWindow, IDC_P4_EDIT), !empty);
}

INT_PTR
CCfgPageArchivesAssoc::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageArchivesAssoc::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        EditLB = new CEditListBox(HWindow, IDC_P4_LIST, ELB_ITEMINDEXES);
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        EditLB->MakeHeader(IDC_P4_NAME);
        EditLB->EnableDrag(::GetParent(HWindow));

        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_EXTENDED_MASKS_HINT));

        // Dialog elements should stretch according to its size, let's set the dividing controls
        ElasticVerticalLayout(1, IDC_P4_LIST);

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED ||
            (LOWORD(wParam) == IDC_P4_VIEW || LOWORD(wParam) == IDC_P4_EDIT) && HIWORD(wParam) == CBN_SELCHANGE)
        {
            StoreControls();
            EnableControls();
        }

        if (LOWORD(wParam) == IDC_P4_LIST && HIWORD(wParam) == LBN_SELCHANGE)
        {
            EditLB->OnSelChanged();
            LoadControls();
            EnableControls();
        }
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDC_P4_LIST:
        {
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ToDo == edtlbGetData)
                {
                    strcpy(dispInfo->Buffer, Config->GetExt((int)dispInfo->ItemID));
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                    return TRUE;
                }
                else
                {
                    if (dispInfo->ItemID == -1)
                    {
                        int index = Config->AddFormat();
                        if (index == -1)
                        {
                            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                        Config->SetFormat(index, dispInfo->Buffer,
                                          FALSE,
                                          0,
                                          0, FALSE);
                        EditLB->SetItemData();
                    }
                    else
                    {
                        int index = (int)dispInfo->ItemID;
                        Config->SetFormat(index, dispInfo->Buffer,
                                          Config->GetUsePacker(index),
                                          Config->GetPackerIndex(index),
                                          Config->GetUnpackerIndex(index), FALSE);
                    }

                    LoadControls();
                    EnableControls();
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break;
            }
                /*              case EDTLBN_MOVEITEM:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              int index;
              EditLB->GetCurSel(index);
              Config->SwapFormats(index, index + (dispInfo->Up ? -1 : 1));
              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // povolim prohozeni
              return TRUE;
            }
*/
            case EDTLBN_MOVEITEM2:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                Config->MoveFormat(index, dispInfo->NewIndex);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow change
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                int index;
                EditLB->GetCurSel(index);
                Config->DeleteFormat(index);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow deletion
                return TRUE;
            }
            }
            break;
        }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        int idCtrl = (int)wParam;
        if (idCtrl == IDC_P4_LIST)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}
