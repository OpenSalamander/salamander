// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* TREEWINDOW_NAME;

//*****************************************************************************
//
// CTreeView
//

class CTreeView : public CWindow
{
protected:
    BOOL SkipNextCharacter;

public:
    CTreeView();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CTreeWindow
//

class CTreeWindow : public CWindow
{
public:
    CTreeView TreeView;
    HIMAGELIST HImageList;
    BOOL EnableTreeNotifications;

    HTREEITEM HDlgTreeItem;
    HTREEITEM HMenuTreeItem;
    HTREEITEM HStrTreeItem;

public:
    CTreeWindow();

    void Navigate(BOOL down);

    // Find the tree item handle; returns NULL when the item does not exist.
    HTREEITEM GetItem(DWORD lParam);

    // Return the LPARAM that can be supplied to GetItem next time to obtain the same entry.
    DWORD GetCurrentItem();

    void SelectItem(HTREEITEM hItem);

    void OnContextMenu(LPARAM lParam, int x, int y);

    void OnEditLayout();

    HWND GetTreeView() { return TreeView.HWindow; }

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

extern CTreeWindow TreeWindow;
