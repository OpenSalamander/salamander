// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/// Nazev tridy okna CTabList.
#define WC_TABLIST L"TabList32"

#define HEADER_ITEMS 13

class CTabList;

class CListView : public CWindow
{
public:
    HWND HToolTip;
    CTabList* TabList;

    int LastItem;
    int LastSubItem;
    int LastWidth;

public:
    CListView();
    ~CListView();

    void Attach(HWND hListView);

protected:
    LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void GotoError(int nVirtKey);
};

class CTabList : public CWindow
{
protected:
    CListView ListView;
    HWND HListView;
    HIMAGELIST HImageList;
    HWND HToolTip;
    double CounterAnchor;
    int AnchorIndex;
    int CounterColumnIndex;
    RECT CounterColumnRect;
    int MessageColumnIndex;
    BOOL DeltaMode;

    int ColumnIndex[HEADER_ITEMS];

public:
    CTabList(CObjectOrigin origin = ooAllocated);
    ~CTabList();

    void RedrawListView();
    HWND GetListViewHWND();
    void SetCount(int count);

    void GetHeaderWidths();

    void GetText(int iItem, int iSubItem, WCHAR* buff, int buffMax, BOOL preferEndOfText = FALSE);

    int GetSelectedIndex(); // vraci -1 pokud neni zadna polozka vybrana; jinak zero-based index polozky

    void SwitchDeltaMode();
    BOOL GetDeltaMode() { return DeltaMode; }

protected:
    void BuildHeader();
    void GetCounterRect();
    void RepaintCounterColumn();

    LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CListView;
};
