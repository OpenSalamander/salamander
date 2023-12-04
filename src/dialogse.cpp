// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//#include "mainwnd.h"
//#include "cfgdlg.h"
//#include "dialogs.h"
//#include "execute.h"
//#include "plugins.h"
//#include "fileswnd.h"
//#include "pack.h"
//extern "C" {
//#include "shexreg.h"
//}

//****************************************************************************
//
// CCfgPageShellExt
//
/*
CCfgPageShellExt::CCfgPageShellExt()
  : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_SHELLEXT /* jestli casem ozivime, pridat help: , IDD_CFGPAGE_SHELLEXT*/
/*, PSP_USETITLE, NULL)
{
  EditLB = NULL;
  DisableNotification = FALSE;
}

CCfgPageShellExt::~CCfgPageShellExt()
{
}

void
CCfgPageShellExt::Transfer(CTransferInfo &ti)
{
  CALL_STACK_MESSAGE1("CCfgPageShellExt::Transfer()");


  if (ti.Type == ttDataToWindow)
    SECLoadRegistry();

  if (ti.Type == ttDataToWindow)
  {
    int count = SECGetCount();
    int i;
    for (i = 0; i < count; i++)
      EditLB->AddItem();
    EditLB->SetCurSel(0);
  }

  ti.CheckBox(IDC_SE_SUBMENU, ShellExtConfigSubmenu);
  ti.EditLine(IDC_SE_SUBMENUNAME, ShellExtConfigSubmenuName, SEC_SUBMENUNAME_MAX);

  if (ti.Type == ttDataFromWindow)
    SECSaveRegistry();
}

void
CCfgPageShellExt::Validate(CTransferInfo &ti)
{
  CALL_STACK_MESSAGE1("CCfgPageShellExt::Validate()");

  // chteji-li mit itemy v submenu, musi byt submenu neprazdny retezec
  BOOL submenu;
  ti.CheckBox(IDC_SE_SUBMENU, submenu);
  if (submenu)
  {
    char buff[SEC_SUBMENUNAME_MAX];
    ti.EditLine(IDC_SE_SUBMENUNAME, buff, SEC_SUBMENUNAME_MAX);
    if (strlen(buff) == 0)
    {
      SalMessageBox(HWindow, LoadStr(IDS_SE_INVALIDSUBMENUNAME), LoadStr(IDS_ERRORTITLE),
                    MB_OK | MB_ICONEXCLAMATION);
      ti.ErrorOn(IDC_SE_SUBMENUNAME);
      return;
    }
  }

  // zkontroluju vsechny itemy
  // kazda musi mit vybran alespon jeden z OF, MF, OD, MD

  CShellExtConfigItem *item = NULL;
  int count = SECGetCount();
  int i;
  for (i = 0; i < count; i++)
  {
    item = SECGetItem(i);

    BOOL file = item->OneFile | item->MoreFiles | item->OneDirectory | item->MoreDirectories;

    if (!file)
    {
      SalMessageBox(HWindow, LoadStr(IDS_SE_INVALIDOPTIONS), LoadStr(IDS_ERRORTITLE),
                    MB_OK | MB_ICONEXCLAMATION);
      EditLB->SetCurSel(i);
      ti.ErrorOn(IDC_SE_OF);
      return;
    }
    if (item->LogicalAnd)
    {
      if (item->OneFile && item->MoreFiles)
      {
        SalMessageBox(HWindow, LoadStr(IDS_SE_INVALIDOPTIONSCOMB), LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);
        EditLB->SetCurSel(i);
        ti.ErrorOn(IDC_SE_OF);
        return;
      }
      if (item->OneDirectory && item->MoreDirectories)
      {
        SalMessageBox(HWindow, LoadStr(IDS_SE_INVALIDOPTIONSCOMB), LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);
        EditLB->SetCurSel(i);
        ti.ErrorOn(IDC_SE_OD);
        return;
      }
    }
  }
}

void 
CCfgPageShellExt::LoadControls()
{
  CALL_STACK_MESSAGE1("CCfgPageShellExt::LoadControls()");

  CShellExtConfigItem *item = NULL;
  int index;
  EditLB->GetCurSelItemID(index);
  BOOL empty = FALSE;
  if (index == -1) 
    empty = TRUE;
  else
    item = SECGetItem(index);

  DisableNotification = TRUE;

  CheckDlgButton(HWindow, IDC_SE_OF, (!empty && item->OneFile) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(HWindow, IDC_SE_OD, (!empty && item->OneDirectory) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(HWindow, IDC_SE_MF, (!empty && item->MoreFiles) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(HWindow, IDC_SE_MD, (!empty && item->MoreDirectories) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(HWindow, IDC_SE_AND, (!empty && item->LogicalAnd) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(HWindow, IDC_SE_OR, (!empty && !item->LogicalAnd) ? BST_CHECKED : BST_UNCHECKED);

  DisableNotification = FALSE;
}

void 
CCfgPageShellExt::StoreControls()
{
  CALL_STACK_MESSAGE1("CCfgPageShellExt::StoreControls()");

  int index;
  EditLB->GetCurSel(index);
  if (!DisableNotification && index >= 0 && index < EditLB->GetCount())
  {
    CShellExtConfigItem *item = SECGetItem(index);
    item->OneFile = (IsDlgButtonChecked(HWindow, IDC_SE_OF) == BST_CHECKED);
    item->OneDirectory = (IsDlgButtonChecked(HWindow, IDC_SE_OD) == BST_CHECKED);
    item->MoreFiles = (IsDlgButtonChecked(HWindow, IDC_SE_MF) == BST_CHECKED);
    item->MoreDirectories = (IsDlgButtonChecked(HWindow, IDC_SE_MD) == BST_CHECKED);
    item->LogicalAnd = (IsDlgButtonChecked(HWindow, IDC_SE_AND) == BST_CHECKED);
  }
  ShellExtConfigSubmenu = (IsDlgButtonChecked(HWindow, IDC_SE_SUBMENU) == BST_CHECKED);
}

void 
CCfgPageShellExt::EnableControls()
{
  CALL_STACK_MESSAGE1("CCfgPageShellExt::EnableControls()");

  if (DisableNotification) return;
  int index;
  EditLB->GetCurSelItemID(index);
  BOOL empty = FALSE;
  if (index == -1) 
    empty = TRUE;

  EnableWindow(GetDlgItem(HWindow, IDC_SE_OF), !empty);
  EnableWindow(GetDlgItem(HWindow, IDC_SE_OD), !empty);
  EnableWindow(GetDlgItem(HWindow, IDC_SE_MF), !empty);
  EnableWindow(GetDlgItem(HWindow, IDC_SE_MD), !empty);
  EnableWindow(GetDlgItem(HWindow, IDC_SE_AND), !empty);
  EnableWindow(GetDlgItem(HWindow, IDC_SE_OR), !empty);

//  if (!empty)
//  {
//    CShellExtConfigItem *item = SECGetItem(index);
//    if (item->LogicalAnd)
//    {
//      EnableWindow(GetDlgItem(HWindow, IDC_SE_OF), !item->MoreFiles);
//      EnableWindow(GetDlgItem(HWindow, IDC_SE_MF), !item->OneFile);
//      EnableWindow(GetDlgItem(HWindow, IDC_SE_OD), !item->MoreDirectories);
//      EnableWindow(GetDlgItem(HWindow, IDC_SE_MD), !item->OneDirectory);
//    }
//  }

  EnableWindow(GetDlgItem(HWindow, IDC_SE_SUBMENUNAME), ShellExtConfigSubmenu);
}

BOOL 
CCfgPageShellExt::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CALL_STACK_MESSAGE4("CCfgPageShellExt::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      EditLB = new CEditListBox(HWindow, IDC_SE_LIST, ELB_ITEMINDEXES | ELB_RIGHTARROW);
      if (EditLB == NULL) TRACE_E(LOW_MEMORY);
      EditLB->MakeHeader(IDC_SE_NAME);
      break;
    }

    case WM_DESTROY:
    {
      // provedu uklidove prace
      SECDeleteAllItems();
      ShellExtConfigVersion = 0; // priste znovu nahrajeme obsah z registry
      break;
    }

    case WM_COMMAND:
    {
      if (HIWORD(wParam) == BN_CLICKED)
      {
        StoreControls(); 
        EnableControls();
      }

      if (LOWORD(wParam) == IDC_SE_LIST && HIWORD(wParam) == LBN_SELCHANGE)
      {
        EditLB->OnSelChanged();
        LoadControls();
        EnableControls();
      }
      break;
    }

    case WM_NOTIFY:
    {
      NMHDR *nmhdr = (NMHDR *)lParam;
      switch (nmhdr->idFrom)
      {
        case IDC_SE_LIST:
        {
          switch (nmhdr->code)
          {
            case EDTLBN_GETDISPINFO:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              if (dispInfo->ToDo == edtlbGetData)
              {
                strcpy(dispInfo->Buffer, SECGetName(dispInfo->ItemID));
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                return TRUE;
              }
              else
              {
                if (dispInfo->ItemID == -1)
                {
                  int index = SECAddItem(NULL);
                  if (index == -1)
                  {
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                  }
                  SECSetName(index, dispInfo->Buffer);
                  EditLB->SetItemData();
                }
                else 
                {
                  int index = dispInfo->ItemID;
                  SECSetName(index, dispInfo->Buffer);
                }

                LoadControls();
                EnableControls();
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                return TRUE;
              }
              break;
            }

            case EDTLBN_MOVEITEM:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              int index;
              EditLB->GetCurSel(index);
              SECSwapItems(index, index + (dispInfo->Up ? -1 : 1));
              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // povolim prohozeni
              return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
              int index;
              EditLB->GetCurSel(index);
              SECDeleteItem(index);
              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // povolim smazani
              return TRUE;
            }

            case EDTLBN_CONTEXTMENU:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              TrackExecuteMenu2(HWindow, dispInfo->Point, dispInfo->HEdit, 
                                ArgsCustomPackers);
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
      int idCtrl = wParam;
      if (idCtrl == IDC_SE_LIST) 
      {
        EditLB->OnDrawItem(lParam);
        return TRUE;
      }
      break;
    }
  }
  return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}
*/