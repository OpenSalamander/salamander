// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndtree.h"
#include "wndframe.h"
#include "wndtext.h"
#include "config.h"
#include "datarh.h"

const char* TREEWINDOW_NAME = "Navigator";

//*****************************************************************************
//
// CTreeView
//

CTreeView::CTreeView()
    : CWindow(ooStatic)
{
    SkipNextCharacter = FALSE;
}

LRESULT
CTreeView::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CHAR:
    {
        if (SkipNextCharacter)
        {
            SkipNextCharacter = FALSE; // zamezime pipnuti
            return FALSE;
        }
        break;
    }

    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case VK_RETURN:
        {
            TreeWindow.OnEditLayout();
            SkipNextCharacter = TRUE; // zamezime pipnuti
            return 0;
        }
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//*****************************************************************************
//
// CTreeWindow
//

CTreeWindow::CTreeWindow()
    : CWindow(ooStatic)
{
    DefWndProc = DefMDIChildProc;
    HImageList = NULL;
    EnableTreeNotifications = FALSE;

    HDlgTreeItem = NULL;
    HMenuTreeItem = NULL;
    HStrTreeItem = NULL;
}

void CTreeWindow::Navigate(BOOL down)
{
    if (GetTreeView() == NULL)
        return;

    HTREEITEM hItem = TreeView_GetNextItem(GetTreeView(), NULL, TVGN_CARET);
    if (hItem == NULL)
        return;

    if (down)
        hItem = TreeView_GetNextItem(GetTreeView(), hItem, TVGN_NEXT);
    else
        hItem = TreeView_GetNextItem(GetTreeView(), hItem, TVGN_PREVIOUS);

    if (hItem == NULL)
        return;
    TreeView_SelectItem(GetTreeView(), hItem);
}

DWORD
CTreeWindow::GetCurrentItem()
{
    HTREEITEM hItem = TreeView_GetNextItem(GetTreeView(), NULL, TVGN_CARET);
    if (hItem == NULL)
        return 0;

    TVITEM tvi;
    tvi.mask = TVIF_HANDLE | TVIF_PARAM;
    tvi.hItem = hItem;
    TreeView_GetItem(GetTreeView(), &tvi);
    DWORD lParam = tvi.lParam;
    return lParam;
}

HTREEITEM
CTreeWindow::GetItem(DWORD lParam)
{
    HTREEITEM hItem = NULL;
    if ((lParam & 0xFFFF0000) == TREE_TYPE_STRING)
        hItem = HStrTreeItem;
    else if ((lParam & 0xFFFF0000) == TREE_TYPE_MENU)
        hItem = HMenuTreeItem;
    else if ((lParam & 0xFFFF0000) == TREE_TYPE_DIALOG)
        hItem = HDlgTreeItem;

    if (hItem != NULL)
    {
        WORD id = LOWORD(lParam);

        HTREEITEM hIter = TreeView_GetChild(GetTreeView(), hItem);
        while (hIter != NULL)
        {
            TVITEM tvi;
            tvi.mask = TVIF_HANDLE | TVIF_PARAM;
            tvi.hItem = hIter;
            TreeView_GetItem(GetTreeView(), &tvi);
            if (LOWORD(tvi.lParam) == id)
                return hIter;

            hIter = TreeView_GetNextSibling(GetTreeView(), hIter);
        }
    }
    return NULL;
}

void CTreeWindow::SelectItem(HTREEITEM hItem)
{
    if (GetTreeView() != NULL)
        TreeView_Select(GetTreeView(), hItem, TVGN_CARET);
}

void CTreeWindow::OnContextMenu(LPARAM lParam, int x, int y)
{
    WORD id = LOWORD(lParam);
    int index = -1;
    switch (lParam & 0xFFFF0000)
    {
    case TREE_TYPE_DIALOG:
    {
        index = Data.FindDialogData(id);
        break;
    }

    case TREE_TYPE_MENU:
    {
        index = Data.FindMenuData(id);
        break;
    }

    default:
        return;
    }
    if (index == -1)
        return;

    char itemText[500];
    lstrcpyn(itemText, DataRH.GetIdentifier(id, FALSE), 499);

    HMENU hMenu = CreatePopupMenu();

    char buff[1000];
    sprintf_s(buff, "Copy to Clipboard: %s", itemText);
    InsertMenu(hMenu, -1, MF_BYPOSITION, 1, buff);
    sprintf_s(buff, "Copy to Clipboard: %d", id);
    InsertMenu(hMenu, -1, MF_BYPOSITION, 2, buff);

    DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                 x, y, HWindow, NULL);
    DestroyMenu(hMenu);

    switch (cmd)
    {
    case 1:
    {
        CopyTextToClipboard(itemText, -1);
        break;
    }

    case 2:
    {
        sprintf_s(buff, "%d", id);
        CopyTextToClipboard(buff, -1);
        break;
    }
    }
}

void CTreeWindow::OnEditLayout()
{
    HTREEITEM hHit = TreeView_GetNextItem(GetTreeView(), NULL, TVGN_NEXTSELECTED);
    if (hHit != NULL)
    {
        // vytahneme lParam z nechame rozbalit menu
        TVITEM tvi;
        tvi.mask = TVIF_HANDLE | TVIF_PARAM;
        tvi.hItem = hHit;
        TreeView_GetItem(GetTreeView(), &tvi);
        if ((tvi.lParam & 0xffff0000) == TREE_TYPE_DIALOG)
        {
            PostMessage(FrameWindow.HWindow, WM_COMMAND, ID_TOOLS_EDITLAYOUT, 0);
        }
    }
}

LRESULT
CTreeWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        //      // priradim oknu ikonku
        //      SendMessage(HWindow, WM_SETICON, ICON_BIG,
        //                  (LPARAM)LoadIcon(HInstance, MAKEINTRESOURCE(IDI_MAIN)));

        TreeView.CreateEx(WS_EX_STATICEDGE, WC_TREEVIEW, "",
                          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL |
                              TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT |
                              TVS_DISABLEDRAGDROP | TVS_SHOWSELALWAYS,
                          0, 0, 0, 0,
                          HWindow,
                          NULL, //HMenu
                          HInstance,
                          NULL);

        HImageList = ImageList_LoadImage(HInstance, MAKEINTRESOURCE(IDB_TREE),
                                         16, 0, RGB(255, 0, 255), IMAGE_BITMAP, 0);

        TreeView_SetImageList(GetTreeView(), HImageList, TVSIL_NORMAL);

        break;
    }

    case WM_SIZE:
    {
        if (GetTreeView() != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            SetWindowPos(GetTreeView(), NULL, 0, 0, r.right, r.bottom, SWP_NOZORDER);
        }
        break;
    }

    case WM_CLOSE:
    {
        PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_CLOSE, 0); // bezpecny close-window
        return 0;
    }

    case WM_DESTROY:
    {
        ImageList_Destroy(HImageList);
        return 0;
    }

    case WM_MDIACTIVATE:
    {
        if (GetTreeView() != NULL)
            SetFocus(GetTreeView());
        break;
    }

    case WM_SETFOCUS:
    {
        if (GetTreeView() != NULL)
        {
            SetFocus(GetTreeView());
            return 0;
        }
        break;
    }

    case WM_NOTIFY:
    {
        NMTREEVIEW* nmtv = (NMTREEVIEW*)lParam;
        if (nmtv->hdr.hwndFrom == GetTreeView() && GetTreeView() != NULL)
        {
            switch (nmtv->hdr.code)
            {
            case NM_RCLICK:
            {
                DWORD pos = GetMessagePos();
                int x = GET_X_LPARAM(pos);
                int y = GET_Y_LPARAM(pos);
                TVHITTESTINFO hti;
                hti.pt.x = x;
                hti.pt.y = y;
                ScreenToClient(GetTreeView(), &hti.pt);
                HTREEITEM hHit = TreeView_HitTest(GetTreeView(), &hti);
                if (hHit != NULL && (hti.flags & TVHT_ONITEM) != 0)
                {
                    // vytahneme lParam z nechame rozbalit menu
                    TVITEM tvi;
                    tvi.mask = TVIF_HANDLE | TVIF_PARAM;
                    tvi.hItem = hHit;
                    TreeView_GetItem(GetTreeView(), &tvi);
                    OnContextMenu(tvi.lParam, x, y);
                }
                break;
            }

            case NM_DBLCLK:
            {
                OnEditLayout();
                break;
            }

            case TVN_SELCHANGING:
            {
                return !TextWindow.CanLeaveText();
            }

            case TVN_SELCHANGED:
            {
                TVITEM* item = &nmtv->itemNew;
                Data.FillTexts(item->lParam);
                if (EnableTreeNotifications)
                    Data.SelectedTreeItem = GetCurrentItem();
                break;
            }

            case TVN_ITEMEXPANDED:
            {
                if (!EnableTreeNotifications)
                    break;
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                WORD type = LOWORD(pnmtv->itemNew.lParam);
                if (HIWORD(pnmtv->itemNew.lParam) == TREE_TYPE_NONE)
                {
                    switch (type)
                    {
                    case TREE_TYPE_NONE_STRINGS:
                    {
                        Data.OpenStringTables = (pnmtv->itemNew.state & TVIS_EXPANDED) != 0;
                        Data.SetDirty();
                        break;
                    }

                    case TREE_TYPE_NONE_DIALOGS:
                    {
                        Data.OpenDialogTables = (pnmtv->itemNew.state & TVIS_EXPANDED) != 0;
                        Data.SetDirty();
                        break;
                    }

                    case TREE_TYPE_NONE_MENUS:
                    {
                        Data.OpenMenuTables = (pnmtv->itemNew.state & TVIS_EXPANDED) != 0;
                        Data.SetDirty();
                        break;
                    }
                    }
                }
                break;
            }
            }
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

CTreeWindow TreeWindow;
