// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* RHWINDOW_NAME;

//*****************************************************************************
//
// CRHListBox
//

class CRHListBox : public CWindow
{
public:
    CRHListBox();

    void OnContextMenu(int x, int y);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CRHWindow
//

class CRHWindow : public CWindow
{
public:
    CRHListBox ListBox;
    HFONT HFont;
    int VisibleItems; // pocet zobrazenych polozek

public:
    CRHWindow();
    ~CRHWindow();

    void GetVisibleItems();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

extern CRHWindow RHWindow;
