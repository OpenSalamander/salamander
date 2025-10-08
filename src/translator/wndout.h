// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* RHWINDOW_NAME;

//*****************************************************************************
//
// COutWindow
//

enum CMessageTypeEnum
{
    mteInfo,
    mteWarning,
    mteError,
    mteSummary
};

enum CResTypeEnum
{
    rteDialog,
    rteMenu,
    rteString,
    rteNone
};

struct COutLine
{
    CMessageTypeEnum MsgType; // info, warning, error
    CResTypeEnum Type;        // dialog, menu, string
    WORD OwnerID;             // ID of the dialog, menu, or string group
    WORD LVIndex;             // position where it is presented in the list view
    WORD ControlID;           // dialogs only: 0 = use LVIndex; otherwise control ID in the dialog
    wchar_t* Text;            // stored text
};

class COutWindow : public CWindow
{
protected:
    HWND HListView;
    HFONT HBoldFont;
    int SelectableCount;
    TDirectArray<COutLine> OutLines;

public:
    COutWindow();
    ~COutWindow();

    void Clear();

    void EnablePaint(BOOL enable); // optimize population
    void AddLine(const wchar_t* text, CMessageTypeEnum msgType, CResTypeEnum type = rteNone, WORD ownerID = 0, WORD lvIndex = 0, WORD controlID = 0);

    int GetLinesCount() { return OutLines.Count; }
    // return the number of lines referencing actual objects
    int GetSelectableLinesCount() { return SelectableCount; }

    int GetErrorLines();
    int GetInfoLines();

    void Navigate(BOOL next);

    void FocusLastItem();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void InsertListViewLine(const wchar_t* text, LPARAM lParam);

    friend void OnGoto(HWND hWnd);
    friend void OnContextMenu(HWND hWnd, int x, int y);
};

extern COutWindow OutWindow;
