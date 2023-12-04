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
    WORD OwnerID;             // ID dialogu, menu, skupiny stringu
    WORD LVIndex;             // na ktere pozici je prezentovan v listview
    WORD ControlID;           // jen dialogy: 0 = pouzit LVIndex, jinak je to ID controlu v dialogu
    wchar_t* Text;            // drzeny text
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

    void EnablePaint(BOOL enable); // pro optimalizaci plneni
    void AddLine(const wchar_t* text, CMessageTypeEnum msgType, CResTypeEnum type = rteNone, WORD ownerID = 0, WORD lvIndex = 0, WORD controlID = 0);

    int GetLinesCount() { return OutLines.Count; }
    // vrati pocet radek, ktere odkazuji na nejake objekty
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
