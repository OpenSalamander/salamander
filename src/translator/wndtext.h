// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* TEXTWINDOW_NAME;

//*****************************************************************************
//
// CListView
//

class CListView : public CWindow
{
public:
    CListView();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CTextWindow
//

class CTextWindow : public CWindow
{
public:
    HWND HTranslated;
    HWND HOriginal;
    HWND HOriginalLabel;
    HWND HTranslatedLabel;
    CListView ListView;
    HIMAGELIST HImageList;
    DWORD Mode;

public:
    CTextWindow();
    ~CTextWindow();

    void SaveColumnsWidth();

    void SetColumnsWidth();

    void EnableControls();

    void Navigate(BOOL down);

    // provede kontrolu textu, zda je mozne prejit na jinou polozku
    BOOL CanLeaveText();

    void ChangeCurrentState(BOOL forceTranslated = FALSE);
    void RefreshBookmarks();

    void SelectItem(WORD id);
    void SelectIndex(WORD index);
    WORD GetSelectIndex();
    void RemoveSelection();

    void SetFocusTranslated();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // naplni sloupce do ListView
    void InitListView();
    void OnContextMenu(int x, int y);
};

extern CTextWindow TextWindow;
