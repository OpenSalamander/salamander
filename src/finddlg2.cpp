// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "edtlbwnd.h"
#include "mainwnd.h"
#include "find.h"
#include "toolbar.h"
#include "menu.h"
#include "shellib.h"
#include "gui.h"
#include "plugins.h"
#include "fileswnd.h"
#include "dialogs.h"

//****************************************************************************
//
// CFindDialog (continued finddlg1.cpp)
//

void CFindDialog::OnHideSelection()
{
    HWND hListView = FoundFilesListView->HWindow;
    DWORD totalCount = ListView_GetItemCount(hListView);
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount == 0)
        return;

    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    DWORD newSelectIndex = 0;
    int i;
    for (i = 0; i < (int)totalCount; i++)
    {
        if (ListView_GetItemState(hListView, i, LVIS_SELECTED) & LVIS_SELECTED)
            break;
        else
            newSelectIndex = i + 1;
    }

    int deletedCount = 0;
    for (i = totalCount - 1; i >= 0; i--)
    {
        if (ListView_GetItemState(hListView, i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            FoundFilesListView->Delete(i);
            deletedCount++;
        }
    }
    if (deletedCount > 0)
    {
        // hack hack: https://forum.altap.cz/viewtopic.php?f=2&t=3112&p=14801#p14801
        // Without resetting to zero, the listview remembers the previous selection state
        ListView_SetItemCount(FoundFilesListView->HWindow, 0);
        // update listview with new item count
        ListView_SetItemCount(FoundFilesListView->HWindow, totalCount - deletedCount);
    }

    if (totalCount - deletedCount > 0)
    {
        if (newSelectIndex >= totalCount - deletedCount)
            newSelectIndex = totalCount - deletedCount - 1;

        ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED); // -1: all items
        ListView_SetItemState(hListView, newSelectIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(hListView, newSelectIndex, FALSE);
    }
    else
        UpdateStatusBar = TRUE;
    UpdateListViewItems();
    SetCursor(hOldCursor);
}

void CFindDialog::OnHideDuplicateNames()
{
    HWND hListView = FoundFilesListView->HWindow;
    DWORD totalCount = ListView_GetItemCount(hListView);
    if (totalCount == 0)
        return;

    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    // sort by path
    FoundFilesListView->SortItems(1);

    // if the item has the same path and name as the previous item, we will exclude it
    int deletedCount = 0;
    CFoundFilesData* lastData = NULL;
    int i;
    for (i = totalCount - 1; i >= 0; i--)
    {
        if (lastData == NULL)
            lastData = FoundFilesListView->At(i);
        else
        {
            CFoundFilesData* data = FoundFilesListView->At(i);
            if (lastData->IsDir == data->IsDir &&
                RegSetStrICmp(lastData->Path, data->Path) == 0 &&
                RegSetStrICmp(lastData->Name, data->Name) == 0)
            {
                FoundFilesListView->Delete(i);
                deletedCount++;
            }
            else
                lastData = data;
        }
    }
    if (deletedCount > 0)
    {
        // update listview with new item count
        ListView_SetItemCount(FoundFilesListView->HWindow, totalCount - deletedCount);
    }

    if (totalCount - deletedCount > 0)
    {
        // stupidly set selected to the zeroth item... this could use improvement over time
        ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED);
        ListView_SetItemState(hListView, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(hListView, 0, FALSE);
    }
    else
        UpdateStatusBar = TRUE;
    UpdateListViewItems();
    SetCursor(hOldCursor);
}

void CFindDialog::OnDelete(BOOL toRecycle)
{
    CALL_STACK_MESSAGE1("CFindDialog::OnDelete()");
    HWND hListView = FoundFilesListView->HWindow;
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount == 0)
        return;

    // calculate the final size of the list
    DWORD listSize = FoundFilesListView->GetSelectedListSize();
    if (listSize <= 2)
        return;

    char* list = (char*)malloc(listSize);
    if (list == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return;
    }

    // let the list be filled
    if (!FoundFilesListView->GetSelectedList(list, listSize))
    {
        free(list);
        return;
    }

    // we will save the focused item and index
    CFoundFilesData lastFocusedItem;
    int lastFocusedIndex = ListView_GetNextItem(FoundFilesListView->HWindow, 0, LVIS_FOCUSED);
    if (lastFocusedIndex != -1)
    {
        CFoundFilesData* lastItem = FoundFilesListView->At(lastFocusedIndex);
        lastFocusedItem.Set(lastItem->Path, lastItem->Name, lastItem->Size, lastItem->Attr, &lastItem->LastWrite, lastItem->IsDir);
    }

    CShellExecuteWnd shellExecuteWnd;
    SHFILEOPSTRUCT fo;
    fo.hwnd = shellExecuteWnd.Create(HWindow, "SEW: CFindDialog::OnDelete toRecycle=%d", toRecycle);
    fo.wFunc = FO_DELETE;
    fo.pFrom = list;
    fo.pTo = NULL;
    fo.fFlags = toRecycle ? FOF_ALLOWUNDO : 0;
    fo.fAnyOperationsAborted = FALSE;
    fo.hNameMappings = NULL;
    fo.lpszProgressTitle = "";
    // we will perform the deletion itself - amazingly easy, unfortunately they sometimes fall here ;-)
    CALL_STACK_MESSAGE1("CFindDialog::OnDelete::SHFileOperation");
    SHFileOperation(&fo);
    free(list);

    // update the list
    FoundFilesListView->CheckAndRemoveSelectedItems(FALSE, lastFocusedIndex, &lastFocusedItem);
}

void CFindDialog::OnSelectAll()
{
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    ListView_SetItemState(FoundFilesListView->HWindow, -1, LVIS_SELECTED, LVIS_SELECTED);
    SetCursor(hOldCursor);
}

void CFindDialog::OnInvertSelection()
{
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    HWND hListView = FoundFilesListView->HWindow;
    int count = ListView_GetItemCount(hListView);
    int i;
    for (i = 0; i < count; i++)
    {
        DWORD state = ListView_GetItemState(hListView, i, LVIS_SELECTED);
        ListView_SetItemState(hListView, i, (state & LVIS_SELECTED) ? 0 : LVIS_SELECTED, LVIS_SELECTED)
    }
    SetCursor(hOldCursor);
}

void CFindDialog::OnShowLog()
{
    if (Log.GetCount() == 0)
        return;

    CFindLogDialog dlg(HWindow, &Log);
    dlg.Execute();
}

void CFindDialog::OnEnterIdle()
{
    if (UpdateStatusBar && !IsSearchInProgress())
    {
        UpdateStatusText();
        UpdateStatusBar = FALSE;
    }
}

void CFindDialog::UpdateStatusText()
{
    int count = ListView_GetSelectedCount(FoundFilesListView->HWindow);
    if (count != 0)
    {
        CQuadWord selectedSize;
        selectedSize = CQuadWord(0, 0);
        int totalCount = ListView_GetItemCount(FoundFilesListView->HWindow);
        int files = 0;
        int dirs = 0;

        int index = -1;
        do
        {
            index = ListView_GetNextItem(FoundFilesListView->HWindow, index, LVNI_SELECTED);
            if (index != -1)
            {
                CFoundFilesData* item = FoundFilesListView->At(index);
                if (item->IsDir)
                    dirs++;
                else
                {
                    files++;
                    selectedSize += item->Size;
                }
            }
        } while (index != -1);

        char text[200];
        if (dirs > 0)
            ExpandPluralFilesDirs(text, 200, files, dirs, epfdmSelected, FALSE);
        else
            ExpandPluralBytesFilesDirs(text, 200, selectedSize, files, dirs, FALSE);
        SendMessage(HStatusBar, SB_SETTEXT, 1 | SBT_NOBORDERS, (LPARAM)text);
    }
    else
    {
        SendMessage(HStatusBar, SB_SETTEXT, 1 | SBT_NOBORDERS, (LPARAM) "");
    }
}

void CFindDialog::OnColorsChange()
{
    if (MainMenu != NULL)
    {
        MainMenu->SetImageList(HGrayToolBarImageList, TRUE);
        MainMenu->SetHotImageList(HHotToolBarImageList, TRUE);
    }
    if (TBHeader != NULL)
        TBHeader->OnColorsChange();
    if (FoundFilesListView != NULL && FoundFilesListView->HWindow != NULL)
        ListView_SetImageList(FoundFilesListView->HWindow, HFindSymbolsImageList, LVSIL_SMALL);
    if (MenuBar != NULL)
    {
        MenuBar->SetFont();
        MenuBarHeight = MenuBar->GetNeededHeight();
        LayoutControls();
    }
}

//****************************************************************************
//
// CFindTBHeader
//

#define FINDTBHDR_BUTTONS 9

CFindTBHeader::CFindTBHeader(HWND hDlg, int ctrlID)
    : CWindow(hDlg, ctrlID, ooAllocated)
{
    CALL_STACK_MESSAGE2("CFindTBHeader::CFindTBHeader(, %d)", ctrlID);
    HNotifyWindow = hDlg;
    Text[0] = 0;
    ErrorsCount = 0;
    InfosCount = 0;
    FoundCount = -1; // to force the settings
    FlashIconCounter = 0;
    StopFlash = FALSE;

    DWORD exStyle = (DWORD)GetWindowLongPtr(HWindow, GWL_EXSTYLE);
    exStyle |= WS_EX_STATICEDGE;
    SetWindowLongPtr(HWindow, GWL_EXSTYLE, exStyle);

    LogToolBar = NULL;
    HWarningIcon = NULL;
    HInfoIcon = NULL;
    HEmptyIcon = NULL;

    ToolBar = new CToolBar(HWindow);

    ToolBar->CreateWnd(HWindow);

    ToolBar->SetImageList(HGrayToolBarImageList);
    ToolBar->SetHotImageList(HHotToolBarImageList);
    ToolBar->SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_TEXT);

    int ids[FINDTBHDR_BUTTONS] = {CM_FIND_FOCUS, CM_FIND_VIEW, CM_FIND_EDIT, CM_FIND_DELETE, CM_FIND_USERMENU, CM_FIND_PROPERTIES, CM_FIND_CLIPCUT, CM_FIND_CLIPCOPY, IDC_FIND_STOP};
    int idx[FINDTBHDR_BUTTONS] = {IDX_TB_FOCUS, IDX_TB_VIEW, IDX_TB_EDIT, IDX_TB_DELETE, IDX_TB_USERMENU, IDX_TB_PROPERTIES, IDX_TB_CLIPBOARDCUT, IDX_TB_CLIPBOARDCOPY, IDX_TB_STOP};

    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_ID | TLBI_MASK_IMAGEINDEX | TLBI_MASK_TEXT;
    tii.Style = TLBI_STYLE_SHOWTEXT;
    int buttonsCount = 0;
    int i;
    for (i = 0; i < FINDTBHDR_BUTTONS; i++)
    {
        tii.ImageIndex = idx[i];
        tii.Text = NULL;
        if (i == 0)
            tii.Text = LoadStr(IDS_FINDTB_FOCUS);
        tii.ID = ids[i];
        ToolBar->InsertItem2(i, TRUE, &tii);
        buttonsCount++;
    }
    ShowWindow(ToolBar->HWindow, SW_SHOW);
}

BOOL CFindTBHeader::CreateLogToolbar(BOOL errors, BOOL infos)
{
    BOOL justCreated = FALSE;
    if (LogToolBar == NULL)
    {
        LogToolBar = new CToolBar(HWindow);
        if (LogToolBar == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }

        LogToolBar->CreateWnd(HWindow);
        LogToolBar->SetStyle(TLB_STYLE_IMAGE);
        WarningDisplayed = FALSE;
        InfoDisplayed = FALSE;

        justCreated = TRUE;
    }

    BOOL change = FALSE;
    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_ID | TLBI_MASK_ICON;
    tii.Style = TLBI_STYLE_SHOWTEXT;
    if (errors && !WarningDisplayed)
    {
        if (InfoDisplayed)
            LogToolBar->RemoveItem(0, TRUE);
        if (HWarningIcon == NULL)
        {
            int iconSize = IconSizes[ICONSIZE_16];
            LoadIconWithScaleDown(NULL, (PCWSTR)IDI_EXCLAMATION, iconSize, iconSize, &HWarningIcon);
        }
        tii.HIcon = HWarningIcon;
        tii.ID = CM_FIND_MESSAGES;
        LogToolBar->InsertItem2(0, TRUE, &tii);
        WarningDisplayed = TRUE;
        change = TRUE;
    }
    if (infos && !errors && !InfoDisplayed)
    {
        if (HInfoIcon == NULL)
        {
            int iconSize = IconSizes[ICONSIZE_16];
            LoadIconWithScaleDown(NULL, (PCWSTR)IDI_INFORMATION, iconSize, iconSize, &HInfoIcon);
        }
        tii.HIcon = HInfoIcon;
        tii.ID = CM_FIND_MESSAGES;
        LogToolBar->InsertItem2(0, TRUE, &tii);
        InfoDisplayed = TRUE;
        change = TRUE;
    }

    if (justCreated)
        ShowWindow(LogToolBar->HWindow, SW_SHOW);

    return change;
}

void CFindTBHeader::OnColorsChange()
{
    if (ToolBar != NULL)
    {
        ToolBar->SetImageList(HGrayToolBarImageList);
        ToolBar->SetHotImageList(HHotToolBarImageList);
        ToolBar->OnColorsChanged();
    }
}

int CFindTBHeader::GetNeededHeight()
{
    return ToolBar->GetNeededHeight() + 2;
}

BOOL CFindTBHeader::EnableItem(DWORD position, BOOL byPosition, BOOL enabled)
{
    return ToolBar->EnableItem(position, byPosition, enabled);
}

void CFindTBHeader::SetFoundCount(int foundCount)
{
    if (foundCount != FoundCount)
    {
        char buff[200];
        GetWindowText(HWindow, buff, 200);
        sprintf(Text, buff, foundCount);

        RECT r;
        GetClientRect(HWindow, &r);
        InvalidateRect(HWindow, &r, TRUE);
    }
}

#define FLASH_ICON_COUNT 20
#define FLASH_ICON_DELAY 333

void CFindTBHeader::StartFlashIcon()
{
    StopFlash = FALSE;
    if (GetForegroundWindow() != HNotifyWindow)
        SendMessage(HNotifyWindow, WM_USER_FLASHICON, 0, 0); // if we are not active, we postpone blinking for later
    else
        SetTimer(HWindow, IDT_FLASHICON, FLASH_ICON_DELAY, NULL);
    FlashIconCounter = FLASH_ICON_COUNT;
}

void CFindTBHeader::StopFlashIcon()
{
    StopFlash = TRUE;
}

void CFindTBHeader::SetFont()
{
    if (ToolBar != NULL && ToolBar->HWindow != NULL)
        ToolBar->SetFont();
}

void CFindTBHeader::SetErrorsInfosCount(int errorsCount, int infosCount)
{
    if (ErrorsCount != errorsCount || InfosCount != infosCount)
    {
        BOOL change = FALSE;
        if (errorsCount != 0 || infosCount != 0)
        {
            change = CreateLogToolbar(errorsCount != 0, infosCount != 0);
            if (change)
                StartFlashIcon(); // if we are not blinking right now, then we will blink
        }
        if (errorsCount == 0 && infosCount == 0 && LogToolBar != NULL)
        {
            DestroyWindow(LogToolBar->HWindow);
            LogToolBar = NULL;
            change = TRUE;
        }
        ErrorsCount = errorsCount;
        InfosCount = infosCount;
        if (change)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            SendMessage(HWindow, WM_SIZE, SIZE_RESTORED,
                        MAKELONG(r.right - r.left, r.bottom - r.top));
            UpdateWindow(HWindow);
        }
    }
}

LRESULT
CFindTBHeader::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFindTBHeader::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_DESTROY:
    {
        if (FlashIconCounter > 0)
            KillTimer(HWindow, WM_TIMER);
        if (ToolBar != NULL)
            DestroyWindow(ToolBar->HWindow);
        if (LogToolBar != NULL)
            DestroyWindow(LogToolBar->HWindow);
        if (HWarningIcon != NULL)
            DestroyIcon(HWarningIcon);
        if (HInfoIcon != NULL)
            DestroyIcon(HInfoIcon);
        if (HEmptyIcon != NULL)
            DestroyIcon(HEmptyIcon);
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = HANDLES(BeginPaint(HWindow, &ps));
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT r;
        GetClientRect(HWindow, &r);
        RECT tr = r;
        tr.right -= 7;
        if (LogToolBar != NULL)
            tr.right -= LogToolBar->GetNeededWidth();

        HFONT hOldFont = (HFONT)SelectObject(hdc, (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0));
        int oldBkMode = SetBkMode(hdc, TRANSPARENT);
        FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE + 1));
        DrawText(hdc, Text, -1, &tr, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
        SetBkMode(hdc, oldBkMode);
        SelectObject(hdc, hOldFont);

        return TRUE;
    }

    case WM_TIMER:
    {
        BOOL stopped = FALSE;
        if (wParam == IDT_FLASHICON)
        {
            if (FlashIconCounter > 0 && LogToolBar != NULL && LogToolBar->HWindow != NULL)
            {
                if (HEmptyIcon == NULL)
                {
                    int iconSize = IconSizes[ICONSIZE_16];
                    LoadIconWithScaleDown(HInstance, (PCWSTR)IDI_EMPTY, iconSize, iconSize, &HEmptyIcon);
                }
                TLBI_ITEM_INFO2 tii;
                tii.Mask = TLBI_MASK_ICON;
                if ((FlashIconCounter & 0x00000001) == 0)
                    tii.HIcon = HEmptyIcon;
                else
                {
                    tii.HIcon = ErrorsCount > 0 ? HWarningIcon : HInfoIcon;
                    if (StopFlash)
                    {
                        FlashIconCounter = 1;
                        StopFlash = FALSE;
                        stopped = TRUE; // so that we don't drift apart again
                    }
                }
                LogToolBar->SetItemInfo2(0, TRUE, &tii);
                UpdateWindow(LogToolBar->HWindow);
                FlashIconCounter--;
            }
            if (FlashIconCounter <= 0)
            {
                KillTimer(HWindow, IDT_FLASHICON);
                if (!stopped && GetForegroundWindow() != HNotifyWindow)
                {
                    // The user probably didn't see the blinking, I will repeat it during the next activation of Findu
                    SendMessage(HNotifyWindow, WM_USER_FLASHICON, 0, 0);
                }
            }
            return 0;
        }
        break;
    }

    case WM_SIZE:
    {
        InvalidateRect(HWindow, NULL, TRUE);
        int tbW = ToolBar->GetNeededWidth();
        int tbH = ToolBar->GetNeededHeight();
        SetWindowPos(ToolBar->HWindow, NULL, 0, 0, tbW, tbH, SWP_NOZORDER | SWP_NOACTIVATE);

        if (LogToolBar != NULL)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            tbW = LogToolBar->GetNeededWidth();
            tbH = LogToolBar->GetNeededHeight();
            SetWindowPos(LogToolBar->HWindow, NULL, width - tbW, 0, tbW, tbH, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }

    case WM_COMMAND:
    {
        if ((HWND)lParam == ToolBar->HWindow)
            PostMessage(HNotifyWindow, WM_COMMAND, MAKEWPARAM(LOWORD(wParam), 0), (LPARAM)HWindow);
        else if (LogToolBar != NULL && (HWND)lParam == LogToolBar->HWindow)
            PostMessage(HNotifyWindow, WM_COMMAND, MAKEWPARAM(LOWORD(wParam), 0), (LPARAM)HWindow);
        break;
    }

    case WM_USER_TBGETTOOLTIP:
    {
        TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
        if (tt->HToolBar == ToolBar->HWindow)
        {
            int tooltips[FINDTBHDR_BUTTONS] = {IDS_FINDTBTT_FOCUS, IDS_FINDTBTT_VIEW, IDS_FINDTBTT_EDIT, IDS_FINDTBTT_DELETE, IDS_FINDTBTT_USERMENU, IDS_FINDTBTT_PROPERTIES, IDS_FINDTBTT_CUT, IDS_FINDTBTT_COPY, IDS_FINDTBTT_STOP};
            lstrcpy(tt->Buffer, LoadStr(tooltips[tt->Index]));
            PrepareToolTipText(tt->Buffer, FALSE);
        }
        else
        {
            wsprintf(tt->Buffer, LoadStr(IDS_FINDTBTT_MESSAGES), ErrorsCount, InfosCount);
        }
        return TRUE;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//*********************************************************************************
//
// CFindManageDialog
//

CFindManageDialog::CFindManageDialog(HWND hParent, const CFindOptionsItem* currenOptionsItem)
    : CCommonDialog(HLanguage, IDD_FINDSETTINGS, IDD_FINDSETTINGS, hParent)
{
    CurrenOptionsItem = currenOptionsItem;
    EditLB = NULL;
    FO = new CFindOptions();
    if (FO == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return;
    }
    FO->Load(FindOptions); // Load global configuration into the working variable
}

CFindManageDialog::~CFindManageDialog()
{
    if (FO != NULL)
        delete FO;
}

void CFindManageDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFindManageDialog::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        int i;
        for (i = 0; i < FO->GetCount(); i++)
            EditLB->AddItem((INT_PTR)FO->At(i));
        EditLB->SetCurSel(0);
        LoadControls();
        //    EnableButtons();
    }
    else
    {
        FindOptions.Load(*FO);
    }
}

void CFindManageDialog::LoadControls()
{
    CALL_STACK_MESSAGE1("CFindManageDialog::LoadControls()");
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    BOOL empty = FALSE;
    if (itemID == -1)
        empty = TRUE;

    CFindOptionsItem* item = NULL;
    if (!empty)
        item = (CFindOptionsItem*)itemID;
    else
        item = (CFindOptionsItem*)CurrenOptionsItem;
    SetDlgItemText(HWindow, IDC_FFS_NAMED, item->NamedText);
    SetDlgItemText(HWindow, IDC_FFS_LOOKIN, item->LookInText);
    SetDlgItemText(HWindow, IDC_FFS_SUBDIRS, item->SubDirectories ? LoadStr(IDS_INFODLGYES) : LoadStr(IDS_INFODLGNO));
    SetDlgItemText(HWindow, IDC_FFS_CONTAINING, item->GrepText);
    char buff[200];
    BOOL dirty;
    item->Criteria.GetAdvancedDescription(buff, 200, dirty);
    SetDlgItemText(HWindow, IDC_FFS_ADVANCED, buff);

    EnableWindow(GetDlgItem(HWindow, IDC_FFS_AUTOLOAD), !empty);
    CheckDlgButton(HWindow, IDC_FFS_AUTOLOAD, item->AutoLoad);
}

INT_PTR
CFindManageDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFindManageDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        EditLB = new CEditListBox(HWindow, IDC_FFS_NAMES);
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        EditLB->MakeHeader(IDC_FFS_NAMESLABEL);
        EditLB->EnableDrag(HWindow);
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_FFS_AUTOLOAD:
        {
            if (HIWORD(wParam) == BN_CLICKED)
            {
                int index;
                EditLB->GetCurSel(index);
                if (index != -1)
                {
                    BOOL checked = IsDlgButtonChecked(HWindow, IDC_FFS_AUTOLOAD);
                    int i;
                    for (i = 0; i < FO->GetCount(); i++)
                        if (FO->At(i)->AutoLoad)
                            FO->At(i)->AutoLoad = FALSE;
                    if (checked)
                        FO->At(index)->AutoLoad = TRUE;
                    InvalidateRect(EditLB->HWindow, NULL, TRUE);
                    UpdateWindow(EditLB->HWindow);
                }
            }
            break;
        }

        case IDC_FFS_NAMES:
        {
            if (HIWORD(wParam) == LBN_SELCHANGE)
            {
                EditLB->OnSelChanged();
                LoadControls();
            }
            break;
        }

        case IDC_FFS_ADD:
        {
            CFindOptionsItem* item = new CFindOptionsItem();
            if (item != NULL)
            {
                *item = *CurrenOptionsItem;
                item->BuildItemName();
                FO->Add(item);
                int index = EditLB->AddItem((INT_PTR)item);
                EditLB->SetCurSel(index);
            }
            else
                TRACE_E(LOW_MEMORY);
            break;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDC_FFS_NAMES:
        {
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ToDo == edtlbGetData)
                {
                    strcpy(dispInfo->Buffer, ((CFindOptionsItem*)dispInfo->ItemID)->ItemName);
                    dispInfo->Bold = ((CFindOptionsItem*)dispInfo->ItemID)->AutoLoad;
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                    return TRUE;
                }
                else
                {
                    CFindOptionsItem* item;
                    if (dispInfo->ItemID == -1)
                    {
                        item = new CFindOptionsItem();
                        if (item == NULL)
                        {
                            TRACE_E(LOW_MEMORY);
                            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                        *item = *CurrenOptionsItem;
                        FO->Add(item);
                        lstrcpyn(item->ItemName, dispInfo->Buffer, ITEMNAME_TEXT_LEN);
                        EditLB->SetItemData((INT_PTR)item);
                        LoadControls();
                    }
                    else
                    {
                        item = (CFindOptionsItem*)dispInfo->ItemID;
                        lstrcpyn(item->ItemName, dispInfo->Buffer, ITEMNAME_TEXT_LEN);
                    }
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
              int srcIndex = index;
              int dstIndex = index + (dispInfo->Up ? -1 : 1);

              CFindOptionsItem tmp;
              tmp = *FO->At(srcIndex);
              *FO->At(srcIndex) = *FO->At(dstIndex);
              *FO->At(dstIndex) = tmp;

              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // allow swapping
              return TRUE;
            }*/
            case EDTLBN_MOVEITEM2:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                int srcIndex = index;
                int dstIndex = dispInfo->NewIndex;

                CFindOptionsItem tmp;
                tmp = *FO->At(srcIndex);
                if (srcIndex < dstIndex)
                {
                    int i;
                    for (i = srcIndex; i < dstIndex; i++)
                        *FO->At(i) = *FO->At(i + 1);
                }
                else
                {
                    int i;
                    for (i = srcIndex; i > dstIndex; i--)
                        *FO->At(i) = *FO->At(i - 1);
                }
                *FO->At(dstIndex) = tmp;

                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow change
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                int index;
                EditLB->GetCurSel(index);
                FO->Delete(index);
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
        if (idCtrl == IDC_FFS_NAMES)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//*********************************************************************************
//
// CFindIgnoreDialog
//

CFindIgnoreDialog::CFindIgnoreDialog(HWND hParent, CFindIgnore* globalIgnoreList)
    : CCommonDialog(HLanguage, IDD_FINDIGNORE, IDD_FINDIGNORE, hParent)
{
    EditLB = NULL;
    DisableNotification = FALSE;
    GlobalIgnoreList = globalIgnoreList;
    HChecked = NULL;
    HUnchecked = NULL;

    IgnoreList = new CFindIgnore;
    if (IgnoreList == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return;
    }
    IgnoreList->Load(GlobalIgnoreList); // Load global configuration into the working variable
}

CFindIgnoreDialog::~CFindIgnoreDialog()
{
    if (HChecked != NULL)
        DestroyIcon(HChecked);
    if (HUnchecked != NULL)
        DestroyIcon(HUnchecked);
    if (IgnoreList != NULL)
        delete IgnoreList;
}

void CFindIgnoreDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFindIgnoreDialog::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        FillList();
    }
    else
    {
        GlobalIgnoreList->Load(IgnoreList);
    }
}

BOOL IsIgnorePathValid(const char* path)
{
    // path must not contain '?' and '*' characters -- we do not support wildcards
    BOOL foundChar = FALSE;
    const char* p = path;
    while (*p != 0)
    {
        if (*p == '?' || *p == '*')
            return FALSE;
        if (*p == '\\')
            foundChar = TRUE;
        if (LowerCase[*p] >= 'a' && LowerCase[*p] <= 'z')
            foundChar = TRUE;
        p++;
    }
    if (!foundChar)
        return FALSE;
    return TRUE;
}

void CFindIgnoreDialog::Validate(CTransferInfo& ti)
{
    int i;
    for (i = 0; i < IgnoreList->GetCount(); i++)
    {
        CFindIgnoreItem* item = IgnoreList->At(i);
        if (item->Enabled && !IsIgnorePathValid(item->Path))
        {
            SalMessageBox(HWindow, LoadStr(IDS_ACBADDRIVE), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_FFI_NAMES);
            EditLB->SetCurSel(i);
            PostMessage(HWindow, WM_USER_EDIT, 0, 0);
            return;
        }
    }
}

void CFindIgnoreDialog::FillList()
{
    EditLB->DeleteAllItems();
    int i;
    for (i = 0; i < IgnoreList->GetCount(); i++)
        EditLB->AddItem((INT_PTR)IgnoreList->At(i));
    EditLB->SetCurSel(0);
}

INT_PTR
CFindIgnoreDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFindIgnoreDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        EditLB = new CEditListBox(HWindow, IDC_FFI_NAMES, ELB_ITEMINDEXES | ELB_ENABLECOMMANDS | ELB_SHOWICON | ELB_SPACEASICONCLICK); // we need enabler and icons
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        EditLB->MakeHeader(IDC_FFI_NAMESLABEL);
        EditLB->EnableDrag(HWindow);
        int iconSize = IconSizes[ICONSIZE_16];
        HIMAGELIST hIL = CreateCheckboxImagelist(iconSize);
        HUnchecked = ImageList_GetIcon(hIL, 0, ILD_NORMAL);
        HChecked = ImageList_GetIcon(hIL, 1, ILD_NORMAL);
        ImageList_Destroy(hIL);

        break;
    }

    case WM_DRAWITEM:
    {
        int idCtrl = (int)wParam;
        if (idCtrl == IDC_FFI_NAMES)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }

    case WM_USER_EDIT:
    {
        SetFocus(EditLB->HWindow);
        EditLB->OnBeginEdit();
        return 0;
    }

    case WM_COMMAND:
    {
        if (DisableNotification)
            return 0;

        switch (LOWORD(wParam))
        {
        case IDC_FFI_RESET:
        {
            if (SalMessageBox(HWindow, LoadStr(IDS_FINDIGNORE_RESET), LoadStr(IDS_QUESTION),
                              MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
            {
                IgnoreList->Reset();
                FillList();
            }
            return 0;
        }

        case IDC_FFI_NAMES:
        {
            if (HIWORD(wParam) == LBN_SELCHANGE)
                EditLB->OnSelChanged();
            break;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDC_FFI_NAMES:
        {
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ToDo == edtlbGetData)
                {
                    CFindIgnoreItem* item = IgnoreList->At((int)dispInfo->ItemID);
                    strcpy(dispInfo->Buffer, item->Path);
                    dispInfo->HIcon = item->Enabled ? HChecked : HUnchecked;
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                    return TRUE;
                }
                else
                {
                    if (dispInfo->ItemID == -1)
                    {
                        if (!IgnoreList->Add(TRUE, dispInfo->Buffer))
                        {
                            TRACE_E(LOW_MEMORY);
                            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                        EditLB->SetItemData(IgnoreList->GetCount() - 1);
                    }
                    else
                    {
                        int index = (int)dispInfo->ItemID;
                        CFindIgnoreItem* item = IgnoreList->At(index);
                        IgnoreList->Set(index, item->Enabled, dispInfo->Buffer);
                    }

                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break;
            }

            case EDTLBN_ENABLECOMMANDS:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                dispInfo->Enable = TLBHDRMASK_NEW | TLBHDRMASK_MODIFY;
                if (index > 0 && index < IgnoreList->GetCount())
                    dispInfo->Enable |= TLBHDRMASK_UP;
                if (index < IgnoreList->GetCount())
                    dispInfo->Enable |= TLBHDRMASK_DELETE;
                if (index < IgnoreList->GetCount() - 1)
                    dispInfo->Enable |= TLBHDRMASK_DOWN;
                return TRUE;
            }

            case EDTLBN_MOVEITEM2:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                IgnoreList->Move(index, dispInfo->NewIndex);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow change
                return TRUE;
            }

            case EDTLBN_ICONCLICKED:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ItemID >= 0 && dispInfo->ItemID < IgnoreList->GetCount())
                {
                    CFindIgnoreItem* item = IgnoreList->At((int)dispInfo->ItemID);
                    item->Enabled = item->Enabled ? FALSE : TRUE;
                    EditLB->RedrawFocusedItem();
                }
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                int index;
                EditLB->GetCurSel(index);
                IgnoreList->Delete(index);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow deletion
                return TRUE;
            }
            }
            break;
        }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// Drag&Drop + Clipboard Copy&Cut
//

BOOL CFindDialog::InitializeOle()
{
    if (!OleInitialized)
    {
        OleInitialized = (OleInitialize(NULL) == S_OK);
    }
    return OleInitialized;
}

void CFindDialog::UninitializeOle()
{
    if (OleInitialized)
    {
        __try
        {
            OleFlushClipboard(); // we pass the data from the IDataObject to the system, which we left lying on the clipboard (this IDataObject will be released by this)
            OleUninitialize();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            OCUExceptionHasOccured++;
        }
        OleInitialized = FALSE;
    }
}

const char*
CFindDialog::GetName(int index)
{
    if (index < 0 || index >= FoundFilesListView->GetCount())
        return FALSE;
    return FoundFilesListView->At(index)->Name;
}

const char*
CFindDialog::GetPath(int index)
{
    if (index < 0 || index >= FoundFilesListView->GetCount())
        return FALSE;
    return FoundFilesListView->At(index)->Path;
}

BOOL CFindDialog::GetCommonPrefixPath(char* buffer, int bufferMax, int& commonPrefixChars)
{
    HWND hListView = FoundFilesListView->HWindow;
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount == 0)
    {
        TRACE_E("Selected count = 0");
        return FALSE;
    }

    char path[MAX_PATH];
    int pathLen = 0;
    path[0] = 0;

    int index = -1;
    do
    {
        index = ListView_GetNextItem(FoundFilesListView->HWindow, index, LVNI_SELECTED);
        if (index != -1)
        {
            CFoundFilesData* file = FoundFilesListView->At(index);
            if (path[0] == 0)
            {
                lstrcpy(path, file->Path); // in the first step, we simply write down the path
                pathLen = lstrlen(path);
            }
            else
            {
                int count = CommonPrefixLength(path, file->Path);
                if (count < pathLen)
                {
                    // the path has been shortened
                    path[count] = 0;
                    pathLen = count;
                }
                if (count == 0)
                    return FALSE; // there is no common part of the path
            }
        }
    } while (index != -1);

    if (pathLen == 0)
        return FALSE;
    if (pathLen + 1 > bufferMax)
    {
        TRACE_E("Buffer is small. " << pathLen + 1 << " bytes is needed");
        return FALSE;
    }
    lstrcpy(buffer, path);
    commonPrefixChars = pathLen;
    return TRUE;
}

struct CMyEnumFileNamesData
{
    CFindDialog* FindDialog;
    HWND HListView;
    int CommonPrefixChars; // number of characters in the shared path
    int LastIndex;
};

static char MyEnumFileNamesBuffer[MAX_PATH]; // the function is called from GUI => cannot be called in multiple threads => we can afford a static buffer
const char* MyEnumFileNames(int index, void* param)
{
    CMyEnumFileNamesData* data = (CMyEnumFileNamesData*)param;
    int foundIndex = ListView_GetNextItem(data->HListView, data->LastIndex, LVNI_SELECTED);
    if (foundIndex != -1)
    {
        data->LastIndex = foundIndex;
        const char* p = data->FindDialog->GetPath(foundIndex) + data->CommonPrefixChars;
        while (*p == '\\')
            p++;
        if (*p != 0)
        {
            lstrcpy(MyEnumFileNamesBuffer, p);
            SalPathAddBackslash(MyEnumFileNamesBuffer, MAX_PATH);
        }
        else
            MyEnumFileNamesBuffer[0] = 0;
        lstrcat(MyEnumFileNamesBuffer, data->FindDialog->GetName(foundIndex));
        return MyEnumFileNamesBuffer;
    }
    TRACE_E("Next item was not found");
    return NULL;
}

void ContextMenuInvoke(IContextMenu2* contextMenu, CMINVOKECOMMANDINFO* ici)
{
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        contextMenu->InvokeCommand(ici);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 8))
    {
        ICExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void ContextMenuQuery(IContextMenu2* contextMenu, HMENU h)
{
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        UINT flags = CMF_NORMAL | CMF_EXPLORE;
        // we will handle the pressed shift - extended context menu, under W2K there is for example Run as...
#define CMF_EXTENDEDVERBS 0x00000100 // rarely used verbs
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shiftPressed)
            flags |= CMF_EXTENDEDVERBS;

        contextMenu->QueryContextMenu(h, 0, 0, -1, flags);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 9))
    {
        QCMExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void CFindDialog::OnDrag(BOOL rightMouseButton)
{
    if (!InitializeOle())
        return;

    HWND hListView = FoundFilesListView->HWindow;
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount < 1)
        return;

    char commonPrefixPath[MAX_PATH];
    int commonPrefixChars;
    if (!GetCommonPrefixPath(commonPrefixPath, MAX_PATH, commonPrefixChars))
    {
        SalMessageBox(HWindow, LoadStr(IDS_COMMONPREFIXNOTFOUND), LoadStr(IDS_ERRORTITLE),
                      MB_ICONEXCLAMATION | MB_OK);
        return;
    }

    // we will save the focused item and index
    CFoundFilesData lastFocusedItem;
    int lastFocusedIndex = ListView_GetNextItem(FoundFilesListView->HWindow, 0, LVIS_FOCUSED);
    if (lastFocusedIndex != -1)
    {
        CFoundFilesData* lastItem = FoundFilesListView->At(lastFocusedIndex);
        lastFocusedItem.Set(lastItem->Path, lastItem->Name, lastItem->Size, lastItem->Attr, &lastItem->LastWrite, lastItem->IsDir);
    }

    CMyEnumFileNamesData data;
    data.FindDialog = this;
    data.HListView = hListView;
    data.CommonPrefixChars = commonPrefixChars;
    data.LastIndex = -1;

    CImpIDropSource* dropSource = new CImpIDropSource(FALSE);
    if (dropSource == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return;
    }

    IDataObject* dataObject = CreateIDataObject(MainWindow->HWindow, commonPrefixPath,
                                                selCount, MyEnumFileNames, &data);
    if (dataObject == NULL)
    {
        SalMessageBox(HWindow, LoadStr(IDS_FOUNDITEMNOTFOUND), LoadStr(IDS_ERRORTITLE),
                      MB_ICONEXCLAMATION | MB_OK);
        dropSource->Release();
        return;
    }

    DWORD dwEffect;
    HRESULT hr = DoDragDrop(dataObject, dropSource,
                            DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK,
                            &dwEffect);

    if (hr == DRAGDROP_S_DROP)
    {
        // returns dwEffect == 0 for MOVE, see "Handling Shell Data Transfer Scenarios" section
        // "Handling Optimized Move Operations": http://msdn.microsoft.com/en-us/library/windows/desktop/bb776904%28v=vs.85%29.aspx
        // (abbreviated: an optimized Move is being performed, which means no copy is made to the target followed by deletion
        //            originalu, so that the source does not unintentionally delete the original (which may not have been moved yet), receives
        //            result of the operation DROPEFFECT_NONE or DROPEFFECT_COPY),
        // thus we initialize this variable
        if (!rightMouseButton && // the right button has a menu: so dropSource->LastEffect contains the effect before displaying the menu and not the final effect: in addition to Move, it is 0 in dwEffect - for Move it is the same as for Cancel - so we are unable to distinguish Move and Cancel, so we better not delete anything
            (dropSource->LastEffect & DROPEFFECT_MOVE))
        {
            // because the move operation is running in its own thread, the files have not been deleted yet
            // so we force remove (a mess, but we don't have to program for another week...)
            FoundFilesListView->CheckAndRemoveSelectedItems(TRUE, lastFocusedIndex, &lastFocusedItem);
        }
    }

    dropSource->Release();
    dataObject->Release();
}

void CFindDialog::OnContextMenu(int x, int y)
{
    if (!InitializeOle())
        return;

    HWND hListView = FoundFilesListView->HWindow;
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount < 1)
        return;

    char commonPrefixPath[MAX_PATH];
    int commonPrefixChars;
    if (!GetCommonPrefixPath(commonPrefixPath, MAX_PATH, commonPrefixChars))
    {
        SalMessageBox(HWindow, LoadStr(IDS_COMMONPREFIXNOTFOUND), LoadStr(IDS_ERRORTITLE),
                      MB_ICONEXCLAMATION | MB_OK);
        return;
    }
    CMyEnumFileNamesData data;
    data.FindDialog = this;
    data.HListView = hListView;
    data.CommonPrefixChars = commonPrefixChars;
    data.LastIndex = -1;
    ContextMenu = CreateIContextMenu2(HWindow, commonPrefixPath,
                                      selCount, MyEnumFileNames, &data);

    if (ContextMenu != NULL)
    {
        HMENU hMenu = CreatePopupMenu();
        ContextMenuQuery(ContextMenu, hMenu);
        RemoveUselessSeparatorsFromMenu(hMenu);

        CMenuPopup popup;
        popup.SetTemplateMenu(hMenu);
        DWORD cmd = popup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                x, y, HWindow, NULL);
        if (cmd != 0)
        {
            char cmdName[2000]; // intentionally we have 2000 instead of 200, shell extensions sometimes write double (consideration: unicode = 2 * "number of characters"), etc.
            if (AuxGetCommandString(ContextMenu, cmd, GCS_VERB, NULL, cmdName, 200) != NOERROR)
            {
                cmdName[0] = 0;
            }
            BOOL cmdCut = stricmp(cmdName, "cut") == 0;
            BOOL cmdCopy = stricmp(cmdName, "copy") == 0;
            BOOL cmdDelete = stricmp(cmdName, "delete") == 0;

            CShellExecuteWnd shellExecuteWnd;
            CMINVOKECOMMANDINFOEX ici;
            ZeroMemory(&ici, sizeof(CMINVOKECOMMANDINFOEX));
            ici.cbSize = sizeof(CMINVOKECOMMANDINFOEX);
            ici.fMask = CMIC_MASK_PTINVOKE;
            if (CanUseShellExecuteWndAsParent(cmdName))
                ici.hwnd = shellExecuteWnd.Create(HWindow, "SEW: CFindDialog::OnContextMenu cmd=%d cmdName=%s", cmd, cmdName);
            else
                ici.hwnd = HWindow;
            ici.lpVerb = MAKEINTRESOURCE(cmd);
            ici.nShow = SW_SHOWNORMAL;
            ici.ptInvoke.x = x;
            ici.ptInvoke.y = y;

            if (!cmdDelete)
            {
                ContextMenuInvoke(ContextMenu, (CMINVOKECOMMANDINFO*)&ici); // call our own delete, so it behaves the same on both the key and context menu
            }

            if (cmdCut || cmdCopy)
            {
                // set preferred drop effect + origin from Salama
                SetClipCutCopyInfo(HWindow, cmdCopy, TRUE);
            }

            // Let's add the Delete command
            if (cmdDelete)
                PostMessage(HWindow, WM_COMMAND, CM_FIND_DELETE, 0);
        }
        ContextMenu->Release();
        ContextMenu = NULL;
        DestroyMenu(hMenu);
    }
    else
        SalMessageBox(HWindow, LoadStr(IDS_FOUNDITEMNOTFOUND), LoadStr(IDS_ERRORTITLE),
                      MB_ICONEXCLAMATION | MB_OK);
}

BOOL CFindDialog::InvokeContextMenu(const char* lpVerb)
{
    BOOL ret = FALSE;
    HWND hListView = FoundFilesListView->HWindow;
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount < 1)
        return FALSE;
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    if (InitializeOle())
    {
        char commonPrefixPath[MAX_PATH];
        int commonPrefixChars;
        if (GetCommonPrefixPath(commonPrefixPath, MAX_PATH, commonPrefixChars))
        {
            CMyEnumFileNamesData data;
            data.FindDialog = this;
            data.HListView = hListView;
            data.CommonPrefixChars = commonPrefixChars;
            data.LastIndex = -1;
            IContextMenu2* menu = CreateIContextMenu2(HWindow, commonPrefixPath,
                                                      selCount, MyEnumFileNames, &data);

            if (menu != NULL)
            {
                CShellExecuteWnd shellExecuteWnd;
                CMINVOKECOMMANDINFOEX ici;
                ZeroMemory(&ici, sizeof(CMINVOKECOMMANDINFOEX));
                ici.cbSize = sizeof(CMINVOKECOMMANDINFOEX);
                ici.fMask = CMIC_MASK_PTINVOKE;
                ici.hwnd = shellExecuteWnd.Create(HWindow, "SEW: CFindDialog::InvokeContextMenu lpVerb=%s", lpVerb);
                ici.lpVerb = lpVerb;
                ici.nShow = SW_SHOWNORMAL;
                GetListViewContextMenuPos(FoundFilesListView->HWindow, &ici.ptInvoke);

                ContextMenuInvoke(menu, (CMINVOKECOMMANDINFO*)&ici);
                menu->Release();

                ret = TRUE;
            }
            else
                SalMessageBox(HWindow, LoadStr(IDS_FOUNDITEMNOTFOUND), LoadStr(IDS_ERRORTITLE),
                              MB_ICONEXCLAMATION | MB_OK);
        }
        else
            SalMessageBox(HWindow, LoadStr(IDS_COMMONPREFIXNOTFOUND), LoadStr(IDS_ERRORTITLE),
                          MB_ICONEXCLAMATION | MB_OK);
    }
    SetCursor(hOldCursor);
    return TRUE;
}

void CFindDialog::OnCutOrCopy(BOOL cut)
{
    if (InvokeContextMenu(cut ? "cut" : "copy"))
    {
        // set preferred drop effect + origin from Salama
        SetClipCutCopyInfo(HWindow, !cut, TRUE);
    }
}

void CFindDialog::OnProperties()
{
    InvokeContextMenu("properties");
}

void CFindDialog::OnOpen(BOOL onlyFocused)
{
    int count = ListView_GetSelectedCount(FoundFilesListView->HWindow);
    if (count == 0)
        return;

    if (!InitializeOle())
        return;

    int index = -1;
    do
    {
        // Petr: I replaced LVNI_SELECTED with LVNI_FOCUSED because people were complaining that
        // marks a bunch of files and then opens them all unintentionally with a double-click, which
        // means the start of many processes, just a mess ... proposed confirmation for opening
        // more than five files, but I find it more logical to open only the most focused ones
        // (as done in the panel; however, Explorer opens all selected, so in that
        // we are different now)
        // Honza 4/2014: if the searched files are from different submenus, it does not work for them
        // Open command from the context menu, so we removed the old functionality from people
        // when they opened all files with one Enter. We are therefore introducing a new command Open Selected.
        // Bug on the forum: https://forum.altap.cz/viewtopic.php?f=6&t=7449
        index = ListView_GetNextItem(FoundFilesListView->HWindow, index, onlyFocused ? LVNI_FOCUSED : LVNI_SELECTED);
        if (index != -1)
        {
            CFoundFilesData* file = FoundFilesListView->At(index);
            BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
            HCURSOR oldCur;
            if (setWait)
                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            char fullPath[MAX_PATH];
            lstrcpy(fullPath, file->Path);
            if (SalPathAppend(fullPath, file->Name, MAX_PATH))
                MainWindow->FileHistory->AddFile(fhitOpen, 0, fullPath);

            ExecuteAssociation(HWindow, file->Path, file->Name);
            if (setWait)
                SetCursor(oldCur);
        }
    } while (!onlyFocused && index != -1);
}

//****************************************************************************
//
// CFindDuplicatesDialog
//

BOOL CFindDuplicatesDialog::SameName = TRUE;
BOOL CFindDuplicatesDialog::SameSize = TRUE;
BOOL CFindDuplicatesDialog::SameContent = TRUE;

CFindDuplicatesDialog::CFindDuplicatesDialog(HWND hParent)
    : CCommonDialog(HLanguage, IDD_FIND_DUPLICATE, IDD_FIND_DUPLICATE, hParent)
{
}

void CFindDuplicatesDialog::Validate(CTransferInfo& ti)
{
    BOOL sameName = IsDlgButtonChecked(HWindow, IDC_FD_SAME_NAME);
    BOOL sameSize = IsDlgButtonChecked(HWindow, IDC_FD_SAME_SIZE);
    if (!sameName && !sameSize)
    {
        SalMessageBox(HWindow, LoadStr(IDS_FIND_DUPS_NO_OPTION), LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_FD_SAME_NAME);
    }
}

void CFindDuplicatesDialog::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_FD_SAME_NAME, SameName);
    ti.CheckBox(IDC_FD_SAME_SIZE, SameSize);
    ti.CheckBox(IDC_FD_SAME_CONTENT, SameContent);

    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CFindDuplicatesDialog::EnableControls()
{
    BOOL sameSize = IsDlgButtonChecked(HWindow, IDC_FD_SAME_SIZE);
    if (!sameSize)
        CheckDlgButton(HWindow, IDC_FD_SAME_CONTENT, BST_UNCHECKED);
    EnableWindow(GetDlgItem(HWindow, IDC_FD_SAME_CONTENT), sameSize);
}

INT_PTR
CFindDuplicatesDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFindDuplicatesDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
            EnableControls();
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//*********************************************************************************
//
// CFindLog
//

#define DEFERRED_ERRORS_MAX 300 // maximum number of stored errors

CFindLog::CFindLog()
    : Items(1, 50)
{
    Clean();
}

CFindLog::~CFindLog()
{
    Clean();
}

void CFindLog::Clean()
{
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CFindLogItem* item = &Items[i];
        free(item->Text);
        free(item->Path);
    }
    Items.DetachMembers();
    SkippedErrors = 0;
    ErrorCount = 0;
    InfoCount = 0;
}

BOOL CFindLog::Add(DWORD flags, const char* text, const char* path)
{
    CFindLogItem item;

    if (flags & FLI_ERROR)
        ErrorCount++;
    else
        InfoCount++;

    if (Items.Count > DEFERRED_ERRORS_MAX)
    {
        SkippedErrors++;
        return TRUE;
    }

    item.Flags = flags;
    item.Text = DupStr(text == NULL ? "" : text);
    if (item.Text == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    item.Path = DupStr(path == NULL ? "" : path);
    if (item.Path == NULL)
    {
        TRACE_E(LOW_MEMORY);
        free(item.Text);
        return FALSE;
    }

    Items.Add(item);
    if (!Items.IsGood())
    {
        TRACE_E(LOW_MEMORY);
        free(item.Text);
        free(item.Path);
        Items.ResetState();
        return FALSE;
    }

    return TRUE;
}

const CFindLogItem*
CFindLog::Get(int index)
{
    if (index < 0 || index >= Items.Count)
    {
        TRACE_E("Index is out of range");
        return NULL;
    }
    return &Items[index];
}

//*********************************************************************************
//
// CFindLogDialog
//

CFindLogDialog::CFindLogDialog(HWND hParent, CFindLog* log)
    : CCommonDialog(HLanguage, IDD_FIND_LOG, IDD_FIND_LOG, hParent)
{
    Log = log;
    HListView = NULL;
}

void CFindLogDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        HListView = GetDlgItem(HWindow, IDC_FINDLOG_LIST);

        int iconSize = IconSizes[ICONSIZE_16];
        HIMAGELIST hIcons = ImageList_Create(iconSize, iconSize, ILC_MASK | GetImageListColorFlags(), 2, 0); // listview will take care of destruction

        HICON hWarning;
        LoadIconWithScaleDown(NULL, (PCWSTR)IDI_EXCLAMATION, iconSize, iconSize, &hWarning);
        HICON hInfo;
        LoadIconWithScaleDown(NULL, (PCWSTR)IDI_INFORMATION, iconSize, iconSize, &hInfo);
        ImageList_SetImageCount(hIcons, 2);
        ImageList_ReplaceIcon(hIcons, 0, hWarning);
        ImageList_ReplaceIcon(hIcons, 1, hInfo);
        DestroyIcon(hWarning);
        DestroyIcon(hInfo);
        HIMAGELIST hOldIcons = ListView_SetImageList(HListView, hIcons, LVSIL_SMALL);
        if (hOldIcons != NULL)
            ImageList_Destroy(hOldIcons);

        DWORD exFlags = LVS_EX_FULLROWSELECT;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        // initialize columns
        int header[] = {IDS_FINDLOG_TYPE, IDS_FINDLOG_TEXT, IDS_FINDLOG_PATH, -1};

        LV_COLUMN lvc;
        lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
        lvc.fmt = LVCFMT_LEFT;
        int i;
        for (i = 0; header[i] != -1; i++)
        {
            lvc.pszText = LoadStr(header[i]);
            lvc.iSubItem = i;
            ListView_InsertColumn(HListView, i, &lvc);
        }

        // adding items
        LVITEM lvi;
        lvi.mask = LVIF_IMAGE;
        lvi.iSubItem = 0;
        char buff[4000];
        const CFindLogItem* item;
        for (i = 0; i < Log->GetCount(); i++)
        {
            item = Log->Get(i);

            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.iImage = (item->Flags & FLI_ERROR) != 0 ? 0 : 1;
            ListView_InsertItem(HListView, &lvi);
            ListView_SetItemText(HListView, i, 0, LoadStr((item->Flags & FLI_ERROR) != 0 ? IDS_FINDLOG_ERROR : IDS_FINDLOG_INFO));

            // remove characters '\r' and '\n' from the text
            lstrcpyn(buff, item->Text, 4000);
            int j;
            for (j = 0; buff[j] != 0; j++)
                if (buff[j] == '\r' || buff[j] == '\n')
                    buff[j] = ' ';

            ListView_SetItemText(HListView, i, 1, buff);
            ListView_SetItemText(HListView, i, 2, item->Path);
        }

        if (Log->GetSkippedCount() > 0)
        {
            wsprintf(buff, LoadStr(IDS_FINDERRORS_SKIPPING), Log->GetSkippedCount());

            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.iImage = 1; // question
            ListView_InsertItem(HListView, &lvi);
            ListView_SetItemText(HListView, i, 0, LoadStr(IDS_FINDLOG_INFO));
            ListView_SetItemText(HListView, i, 1, buff);
        }

        // zeroth item will be selected
        DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItemState(HListView, 0, state, state);

        // set column widths
        ListView_SetColumnWidth(HListView, 0, LVSCW_AUTOSIZE_USEHEADER);
        ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);
        ListView_SetColumnWidth(HListView, 2, LVSCW_AUTOSIZE_USEHEADER);
    }
}

void CFindLogDialog::OnFocusFile()
{
    CALL_STACK_MESSAGE1("CFindLogDialog::OnFocusFile()");
    const CFindLogItem* item = GetSelectedItem();
    if (item == NULL)
        return;

    if (SalamanderBusy)
    {
        Sleep(200); // give Salam cas - if it was about switching from the main window, she could
                    // still need to finish processing the message queue from the menu
        if (SalamanderBusy)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SALAMANDBUSY2),
                          LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
    static char FocusPath[2 * MAX_PATH];
    lstrcpyn(FocusPath, item->Path, _countof(FocusPath));
    char buffEmpty[] = "";
    char* p = buffEmpty;
    if (FocusPath[0] != 0)
    {
        if (FocusPath[strlen(FocusPath) - 1] == '\\')
            FocusPath[strlen(FocusPath) - 1] = 0;
        p = strrchr(FocusPath, '\\');
        if (p == NULL)
        {
            TRACE_E("p == NULL");
            return;
        }
        *p = 0;
    }

    SendMessage(MainWindow->GetActivePanel()->HWindow, WM_USER_FOCUSFILE, (WPARAM)p + 1, (LPARAM)FocusPath);
}

void CFindLogDialog::OnIgnore()
{
    CALL_STACK_MESSAGE1("CFindLogDialog::OnIgnore()");
    const CFindLogItem* item = GetSelectedItem();
    if (item == NULL)
        return;

    CTruncatedString str;
    str.Set(LoadStr(IDS_FINDLOG_IGNORE), item->Path);
    CMessageBox msgBox(HWindow, MSGBOXEX_OKCANCEL | MSGBOXEX_ICONQUESTION,
                       LoadStr(IDS_QUESTION), &str, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL);
    if (msgBox.Execute() == IDOK)
    {
        FindIgnore.AddUnique(TRUE, item->Path);
    }
}

const CFindLogItem*
CFindLogDialog::GetSelectedItem()
{
    CALL_STACK_MESSAGE1("CFindLogDialog::OnIgnore()");
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    if (index < 0 || index >= Log->GetCount())
        return NULL;
    return Log->Get(index);
}

void CFindLogDialog::EnableControls()
{
    const CFindLogItem* item = GetSelectedItem();
    BOOL path = item != NULL && item->Path != NULL && item->Path[0] != 0;
    TRACE_I("path=" << path);
    EnableWindow(GetDlgItem(HWindow, IDC_FINDLOG_FOCUS), path);
    EnableWindow(GetDlgItem(HWindow, IDC_FINDLOG_IGNORE), path && (item->Flags & FLI_IGNORE) != 0);
}

INT_PTR
CFindLogDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFindLogDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_FINDLOG_FOCUS:
        {
            OnFocusFile();
            return 0;
        }

        case IDC_FINDLOG_IGNORE:
        {
            OnIgnore();
            return 0;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDC_FINDLOG_LIST)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case LVN_ITEMCHANGED:
            {
                EnableControls();
                return 0;
            }
            }
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
