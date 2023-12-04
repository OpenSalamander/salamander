// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "translator.h"
#include "wndrh.h"
#include "datarh.h"
#include "wndframe.h"

const char* RHWINDOW_NAME = "Resource Symbols (Alt+2)";

//*****************************************************************************
//
// CRHListBox
//

CRHListBox::CRHListBox()
    : CWindow(ooStatic)
{
}

void CRHListBox::OnContextMenu(int x, int y)
{
    int count = ListView_GetItemCount(HWindow);
    int row = ListView_GetNextItem(HWindow, -1, LVNI_SELECTED);
    if (row < 0 || row >= count)
        return;

    int index = DataRH.RowToIndex(row + 1);
    if (index == -1)
        return;

    const CDataRHItem* item = &DataRH.Items[index];
    char buff[1000];
    HMENU hMenu = CreatePopupMenu();

    sprintf_s(buff, "Copy to Clipboard: %s", item->Name);
    InsertMenu(hMenu, -1, MF_BYPOSITION, 1, buff);
    sprintf_s(buff, "Copy to Clipboard: %d", item->ID);
    InsertMenu(hMenu, -1, MF_BYPOSITION, 2, buff);
    sprintf_s(buff, "Copy to Clipboard: %d", item->ID);
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, buff);
    sprintf_s(buff, "Center view to selected item\tSpace");
    InsertMenu(hMenu, -1, MF_BYPOSITION, 3, buff);
    DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                 x, y, HWindow, NULL);
    DestroyMenu(hMenu);
    switch (cmd)
    {
    case 1:
    {
        CopyTextToClipboard(item->Name, -1);
        break;
    }

    case 2:
    {
        sprintf_s(buff, "%d", item->ID);
        CopyTextToClipboard(buff, -1);
        break;
    }

    case 3:
    {
        PostMessage(HWindow, WM_KEYDOWN, VK_SPACE, 0);
        break;
    }
    }
}

LRESULT
CRHListBox::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_LBUTTONUP:
    case WM_LBUTTONDOWN:
    {
        SetFocus(HWindow);
        return 0;
    }

    case WM_RBUTTONDOWN:
    {
        SetFocus(HWindow);
        SetCapture(HWindow);
        return 0;
    }

    case WM_RBUTTONUP:
    {
        if (GetCapture() != HWindow)
            break;
        ReleaseCapture();

        POINT p;
        DWORD messagePos = GetMessagePos();
        p.x = GET_X_LPARAM(messagePos);
        p.y = GET_Y_LPARAM(messagePos);
        OnContextMenu(p.x, p.y);

        return 0;
    }

    case WM_CHAR:
        return 0; // jinak hleda podle prvniho znaku, coz je nechtene

    case WM_KEYDOWN:
    {
        BOOL topFirst = TRUE;
        int top = ListView_GetTopIndex(HWindow);
        int newTop = top;
        switch (wParam)
        {
        case VK_UP:
            newTop--;
            break;
        case VK_DOWN:
            newTop++;
            break;
        case VK_PRIOR:
            newTop -= (RHWindow.VisibleItems - 1);
            break;
        case VK_NEXT:
            newTop += (RHWindow.VisibleItems - 1);
            break;
        case VK_HOME:
            newTop = 0;
            break;
        case VK_END:
        {
            newTop = ListView_GetItemCount(RHWindow.ListBox.HWindow) - 1;
            break;
        }
        case VK_SPACE:
        {
            int index = ListView_GetNextItem(HWindow, -1, LVNI_SELECTED);
            if (index != -1)
            {
                newTop = index - RHWindow.VisibleItems / 2;
                topFirst = FALSE;
            }
            break;
        }
        }
        if (newTop < 0)
            newTop = 0;
        if (newTop != top)
        {
            int count = ListView_GetItemCount(RHWindow.ListBox.HWindow);
            ListView_EnsureVisible(RHWindow.ListBox.HWindow, topFirst ? 0 : count - 1, FALSE);
            ListView_EnsureVisible(RHWindow.ListBox.HWindow, newTop, FALSE);
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//*****************************************************************************
//
// CRHWindow
//

CRHWindow::CRHWindow()
    : CWindow(ooStatic)
{
    DefWndProc = DefMDIChildProc;
    HFont = NULL;
    VisibleItems = 1;
}

CRHWindow::~CRHWindow()
{
    if (HFont != NULL)
    {
        HANDLES(DeleteObject(HFont));
        HFont = NULL;
    }
}

void CRHWindow::GetVisibleItems()
{
    RECT r;
    GetClientRect(HWindow, &r);
    SetWindowPos(ListBox.HWindow, NULL, 0, 0, r.right, r.bottom, SWP_NOZORDER);

    int count = ListView_GetItemCount(ListBox.HWindow);
    RECT iR;
    ListView_GetItemRect(ListBox.HWindow, 0, &iR, LVIR_LABEL);
    int itemHeight = iR.bottom - iR.top;
    if (itemHeight != 0)
    {
        VisibleItems = (r.bottom - 2) / itemHeight;
        if (VisibleItems < 1)
            VisibleItems = 1;
    }
    else
        VisibleItems = 1;
}

LRESULT
CRHWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        LOGFONT lf;
        GetFixedLogFont(&lf);
        HFont = HANDLES(CreateFontIndirect(&lf));

        // priradim oknu ikonku
        //      SendMessage(HWindow, WM_SETICON, ICON_BIG,
        //                  (LPARAM)LoadIcon(HInstance, MAKEINTRESOURCE(IDI_MAIN)));
        ListBox.CreateEx(WS_EX_STATICEDGE, "SysListView32", "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | LVS_NOSORTHEADER,
                         0, 0, 0, 0,
                         HWindow,
                         NULL, //HMenu
                         HInstance,
                         &ListBox);
        SendMessage(ListBox.HWindow, WM_SETFONT, (WPARAM)HFont, (LPARAM)FALSE);

        DWORD origFlags = ListView_GetExtendedListViewStyle(ListBox.HWindow);
        ListView_SetExtendedListViewStyle(ListBox.HWindow, origFlags | LVS_EX_FULLROWSELECT);

        LVCOLUMNW lvc;
        lvc.mask = LVCF_TEXT | LVCF_FMT;
        lvc.pszText = (wchar_t*)L"RG";
        lvc.fmt = LVCFMT_LEFT;
        lvc.iSubItem = 0;
        SendMessageW(ListBox.HWindow, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc);
        ListView_SetColumnWidth(ListBox.HWindow, 0, 2000);

        HMENU hMenu = GetSystemMenu(HWindow, FALSE);
        if (hMenu != NULL)
            EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

        break;
    }

    case WM_DESTROY:
    {
        if (HFont != NULL)
        {
            HANDLES(DeleteObject(HFont));
            HFont = NULL;
        }
        break;
    }

    case WM_SIZE:
    {
        if (ListBox.HWindow != NULL)
        {
            GetVisibleItems();
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
        if (ListBox.HWindow != NULL)
            SetFocus(ListBox.HWindow);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

CRHWindow RHWindow;
