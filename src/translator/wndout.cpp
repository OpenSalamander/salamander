// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndout.h"
#include "wndframe.h"
#include "wndtext.h"
#include "wndtree.h"
#include "wndprev.h"
#include "datarh.h"

const char* OUTWINDOW_NAME = "Output (Alt+4)";

//*****************************************************************************
//
// COutListBox
//

void OnGoto(HWND hWnd)
{
    int index = ListView_GetNextItem(hWnd, -1, LVIS_SELECTED);
    if (index != -1)
    {
        LVITEM lvi;
        lvi.mask = LVIF_PARAM;
        lvi.iItem = index;
        ListView_GetItem(hWnd, &lvi);
        int lineIndex = lvi.lParam;
        COutLine* outLine = &OutWindow.OutLines[lineIndex];
        if (outLine->Type != rteNone)
        {
            // vyhledame spravny list v tree okne a zvolime ho
            LPARAM lParam = outLine->OwnerID;
            switch (outLine->Type)
            {
            case rteDialog:
                lParam |= TREE_TYPE_DIALOG;
                break;
            case rteMenu:
                lParam |= TREE_TYPE_MENU;
                break;
            case rteString:
                lParam |= TREE_TYPE_STRING;
                break;
            }
            HTREEITEM hItem = TreeWindow.GetItem(lParam);
            if (hItem != NULL)
            {
                TreeWindow.SelectItem(hItem);
                if (outLine->Type == rteDialog && outLine->ControlID != 0)
                {
                    TextWindow.RemoveSelection();
                    PreviewWindow.HighlightControl(outLine->ControlID);
                }
                else
                    TextWindow.SelectIndex(outLine->LVIndex);

                BOOL found = FALSE;
                for (int i = 0; i < Data.MenuPreview.Count; i++)
                {
                    CMenuPreview* menuPreview = Data.MenuPreview[i];
                    if (menuPreview->HasLine(index))
                    {
                        PreviewWindow.PreviewMenu(i);
                        found = TRUE;
                        break;
                    }
                }
                if (!found)
                    PreviewWindow.PreviewMenu(-1);
            }
        }
    }
}

void OnContextMenu(HWND hWnd, int x, int y)
{
    char buff[1000];
    HMENU hMenu = CreatePopupMenu();
    COutLine* outLine = NULL;
    int index = ListView_GetNextItem(hWnd, -1, LVIS_SELECTED);
    if (index != -1)
    {
        LVITEM lvi;
        lvi.mask = LVIF_PARAM;
        lvi.iItem = index;
        ListView_GetItem(hWnd, &lvi);
        int lineIndex = lvi.lParam;
        outLine = &OutWindow.OutLines[lineIndex];
        if (outLine->Type == rteDialog || outLine->Type == rteMenu)
        {
            sprintf_s(buff, "Copy to Clipboard: %s", DataRH.GetIdentifier(outLine->OwnerID, FALSE));
            InsertMenu(hMenu, -1, MF_BYPOSITION, 1, buff);
            sprintf_s(buff, "Copy to Clipboard: %d", outLine->OwnerID);
            InsertMenu(hMenu, -1, MF_BYPOSITION, 2, buff);
            InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, "");
        }
    }
    sprintf_s(buff, "Clear");
    InsertMenu(hMenu, -1, MF_BYPOSITION, 10, buff);

    DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                 x, y, hWnd, NULL);
    DestroyMenu(hMenu);
    switch (cmd)
    {
    case 1:
    {
        CopyTextToClipboard(DataRH.GetIdentifier(outLine->OwnerID, FALSE), -1);
        break;
    }

    case 2:
    {
        sprintf_s(buff, "%d", outLine->OwnerID);
        CopyTextToClipboard(buff, -1);
        break;
    }

    case 10: // clear
    {
        OutWindow.Clear();
        break;
    }
    }
}

WNDPROC DefListViewWndProc = NULL;

LRESULT
ListViewWindowProcW(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_LBUTTONDBLCLK:
    {
        if (!TextWindow.CanLeaveText())
            return 0;

        int index = ListView_GetNextItem(hWnd, -1, LVIS_SELECTED);
        if (index != -1)
        {
            OnGoto(hWnd);
        }
        break;
    }

    case WM_KEYDOWN:
    {
        if (wParam == VK_RETURN)
            OnGoto(hWnd);

        if (wParam == VK_DELETE)
            OutWindow.Clear();

        break;
    }

    case WM_TIMER:
    {
        return 0; // pod W7 chodi nejakej cizi timer, ktery nam zpusobi ensure visible
    }
    }
    return DefListViewWndProc(hWnd, uMsg, wParam, lParam);
}

//*****************************************************************************
//
// COutWindow
//

COutWindow::COutWindow()
    : CWindow(ooStatic), OutLines(500, 200)
{
    DefWndProc = DefMDIChildProc;
    HListView = NULL;
    HBoldFont = NULL;
    SelectableCount = 0;
}

COutWindow::~COutWindow()
{
    Clear();
    if (HBoldFont != NULL)
    {
        HANDLES(DeleteObject(HBoldFont));
        HBoldFont = NULL;
    }
}

void COutWindow::Clear()
{
    Data.MenuPreview.DestroyMembers();

    ListView_DeleteAllItems(HListView);
    SelectableCount = 0;
    for (int i = 0; i < OutLines.Count; i++)
    {
        if (OutLines[i].Text != NULL)
        {
            free(OutLines[i].Text);
            OutLines[i].Text = NULL;
        }
    }
    OutLines.DestroyMembers();
}

void COutWindow::EnablePaint(BOOL enable)
{
    //SendMessage(HListBox, WM_SETREDRAW, enable, 0);
}

wchar_t* dupstr(const wchar_t* s)
{
    wchar_t* p = NULL;
    if (s)
    {
        int len = wcslen(s);
        p = (wchar_t*)malloc(2 * (len + 1));
        wcscpy_s(p, len + 1, s);
    }
    return p;
}

void COutWindow::InsertListViewLine(const wchar_t* text, LPARAM lParam)
{
    int count = ListView_GetItemCount(HListView);

    LVITEMW lvi;
    lvi.mask = LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
    lvi.iItem = count;
    lvi.iSubItem = 0;
    lvi.state = 0;
    lvi.lParam = lParam;
    wchar_t buff[10000];
    wcscpy_s(buff, text);
    lvi.pszText = buff;
    SendMessageW(HListView, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
}

void COutWindow::AddLine(const wchar_t* text, CMessageTypeEnum msgType, CResTypeEnum type,
                         WORD ownerID, WORD lvIndex, WORD controlID)
{
    COutLine line;
    line.MsgType = msgType;
    line.Type = type;
    line.OwnerID = ownerID;
    line.LVIndex = lvIndex;
    line.ControlID = controlID;
    line.Text = dupstr(text);
    int lineIndex = OutLines.Add(line);
    if (OutLines.IsGood() && HListView != NULL)
    {
        InsertListViewLine(text, lineIndex);
        if (type != rteNone)
            SelectableCount++;
    }
}

int COutWindow::GetErrorLines()
{
    int errs = 0;
    for (int i = 0; i < OutLines.Count; i++)
    {
        if (OutLines[i].MsgType == mteError)
            errs++;
    }
    return errs;
}

int COutWindow::GetInfoLines()
{
    int info = 0;
    for (int i = 0; i < OutLines.Count; i++)
    {
        if (OutLines[i].MsgType == mteInfo)
            info++;
    }
    return info;
}

void COutWindow::FocusLastItem()
{
    int index = ListView_GetItemCount(HListView);
    if (index > 0)
    {
        ListView_SetItemState(HListView, index - 1, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
        ListView_EnsureVisible(HListView, index - 1, FALSE);
    }
}

void COutWindow::Navigate(BOOL next)
{
    if (GetSelectableLinesCount() < 1)
        return;

    int index = ListView_GetNextItem(HListView, -1, LVIS_SELECTED);
    if (index == -1)
    {
        if (next)
            index = -1;
        else
            index = OutLines.Count;
    }

    int newIndex = -1;

    if (next)
    {
        for (int i = index + 1; i < OutLines.Count; i++)
        {
            if (OutLines[i].Type != rteNone)
            {
                newIndex = i;
                break;
            }
        }
    }
    else
    {
        for (int i = index - 1; i >= 0; i--)
        {
            if (OutLines[i].Type != rteNone)
            {
                newIndex = i;
                break;
            }
        }
    }
    // -1 -> zadna selection
    ListView_SetItemState(HListView, newIndex, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
    ListView_EnsureVisible(HListView, newIndex, FALSE);
    if (newIndex != -1)
        OnGoto(HListView);
}

LRESULT
COutWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // priradim oknu ikonku
        //      SendMessage(HWindow, WM_SETICON, ICON_BIG,
        //                  (LPARAM)LoadIcon(HInstance, MAKEINTRESOURCE(IDI_MAIN)));

        HListView = CreateWindowExW(WS_EX_STATICEDGE, L"SysListView32", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | LVS_NOSORTHEADER,
                                    0, 0, 0, 0,
                                    HWindow,
                                    NULL, //HMenu
                                    HInstance,
                                    NULL);

        HFONT hFont = (HFONT)SendMessage(HListView, WM_GETFONT, 0, 0);
        LOGFONT lf;
        GetObject(hFont, sizeof(lf), &lf);
        lf.lfWeight = FW_BOLD;
        HBoldFont = HANDLES(CreateFontIndirect(&lf));

        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | LVS_EX_FULLROWSELECT);

        LVCOLUMNW lvc;
        lvc.mask = LVCF_TEXT | LVCF_FMT;
        lvc.pszText = (wchar_t*)L"Message";
        lvc.fmt = LVCFMT_LEFT;
        lvc.iSubItem = 0;
        SendMessageW(HListView, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc);
        ListView_SetColumnWidth(HListView, 0, 3000);

        DefListViewWndProc = (WNDPROC)GetWindowLongW(HListView, GWL_WNDPROC);
        SetWindowLongW(HListView, GWL_WNDPROC, (LONG)ListViewWindowProcW);

        HMENU hMenu = GetSystemMenu(HWindow, FALSE);
        if (hMenu != NULL)
            EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

        if (OutLines.Count > 0)
        {
            for (int i = 0; i < OutLines.Count; i++)
            {
                COutLine* line = &OutLines[i];
                InsertListViewLine(line->Text, i);
                if (line->Type != rteNone)
                    SelectableCount++;
            }
        }

        break;
    }

    case WM_DESTROY:
    {
        if (HBoldFont != NULL)
        {
            HANDLES(DeleteObject(HBoldFont));
            HBoldFont = NULL;
        }
        break;
    }

    case WM_SIZE:
    {
        if (HListView != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            SetWindowPos(HListView, NULL, 0, 0, r.right, r.bottom, SWP_NOZORDER);
        }
        break;
    }

    case WM_CLOSE:
    {
        PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_CLOSE, 0); // bezpecny close-window
        return 0;
    }

    case WM_SETFOCUS:
    {
        if (HListView != NULL)
            SetFocus(HListView);
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* nmh = (NMHDR*)lParam;
        if (nmh->hwndFrom == HListView)
        {
            switch (((LPNMHDR)lParam)->code)
            {
                /*
          case NM_DBLCLK:
          {
            OnOpen();
            break;
          }
          */

            case NM_RCLICK:
            {
                POINT p;
                DWORD messagePos = GetMessagePos();
                p.x = GET_X_LPARAM(messagePos);
                p.y = GET_Y_LPARAM(messagePos);
                OnContextMenu(HListView, p.x, p.y);
                break;
            }

            case NM_CUSTOMDRAW:
            {
                LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lParam;

                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;

                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
                    return CDRF_NOTIFYSUBITEMDRAW; // pozadame si o zaslani notifikace CDDS_ITEMPREPAINT | CDDS_SUBITEM

                if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
                {
                    LVITEM lvi;
                    lvi.mask = LVIF_PARAM;
                    lvi.iItem = cd->nmcd.dwItemSpec;
                    ListView_GetItem(HListView, &lvi);
                    int lineIndex = lvi.lParam;
                    COutLine* outLine = &OutWindow.OutLines[lineIndex];
                    COLORREF textColor = GetSysColor(COLOR_WINDOWTEXT); // mteInfo
                    switch (outLine->MsgType)
                    {

                    case mteWarning:
                    {
                        textColor = RGB(0, 0, 160);
                        break;
                    }

                    case mteError:
                    {
                        textColor = RGB(255, 0, 0);
                        break;
                    }

                    case mteSummary:
                    {
                        SelectObject(cd->nmcd.hdc, HBoldFont);
                        break;
                    }
                    }

                    cd->clrText = textColor;
                    return CDRF_NEWFONT;
                }
                break;
            }
            }
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

COutWindow OutWindow;
