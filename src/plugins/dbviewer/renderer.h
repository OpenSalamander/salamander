// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern BOOL IsAlphaNumeric[256]; // TRUE/FALSE table for characters (FALSE = neither a letter nor a digit)
extern BOOL IsAlpha[256];

//****************************************************************************
//
// CSelection
//

class CSelection
{
private:
    int FocusX;  // X coordinate of the focus
    int FocusY;  // Y coordinate of the focus
    int AnchorX; // if the selection is dragged, opposite corner to the focus
    int AnchorY; // if the selection is dragged, opposite corner to the focus
    // if only one item is selected, Anchor? == Focus?

    RECT Rect; // normalized rectangle

public:
    CSelection();

    CSelection& operator=(const CSelection& s);

    // return TRUE if cell [x,y] lies inside the selection
    BOOL Contains(int x, int y)
    {
        return x >= Rect.left && x <= Rect.right && y >= Rect.top && y <= Rect.bottom;
    }

    // return TRUE if column x lies inside the selection
    BOOL ContainsColumn(int x)
    {
        return x >= Rect.left && x <= Rect.right;
    }

    // return TRUE if row y lies inside the selection
    BOOL ContainsRow(int y)
    {
        return y >= Rect.top && y <= Rect.bottom;
    }

    // return TRUE if cell [x,y] is the focused cell
    BOOL IsFocus(int x, int y)
    {
        return x == FocusX && y == FocusY;
    }

    void GetFocus(int* x, int* y)
    {
        *x = FocusX;
        *y = FocusY;
    }

    void GetAnchor(int* x, int* y)
    {
        *x = AnchorX;
        *y = AnchorY;
    }

    void GetNormalizedSelection(RECT* selection)
    {
        *selection = Rect;
    }

    void SetFocus(int x, int y)
    {
        FocusX = x;
        FocusY = y;
        Normalize();
    }

    void SetFocusAndAnchor(int x, int y)
    {
        FocusX = x;
        FocusY = y;
        AnchorX = x;
        AnchorY = y;
        Normalize();
    }

    void SetAnchor(int x, int y)
    {
        AnchorX = x;
        AnchorY = y;
        Normalize();
    }

    BOOL FocusIsAnchor()
    {
        return FocusX == AnchorX && FocusY == AnchorY;
    }

    // return TRUE if cell [x,y] had a different state in oldSelection
    BOOL Changed(const CSelection* old, int x, int y)
    {
        // if the selection state changed
        BOOL sel = x >= Rect.left && x <= Rect.right && y >= Rect.top && y <= Rect.bottom;
        BOOL oldSel = x >= old->Rect.left && x <= old->Rect.right && y >= old->Rect.top && y <= old->Rect.bottom;
        // or if the focus state changed
        BOOL foc = FocusX == x && FocusY == y;
        BOOL oldFoc = old->FocusX == x && old->FocusY == y;
        // return TRUE; otherwise FALSE
        return foc != oldFoc || sel != oldSel;
    }

    BOOL ChangedColumn(const CSelection* old, int x)
    {
        BOOL sel = x >= Rect.left && x <= Rect.right;
        BOOL oldSel = x >= old->Rect.left && x <= old->Rect.right;
        return sel != oldSel;
    }

    BOOL ChangedRow(const CSelection* old, int y)
    {
        BOOL sel = y >= Rect.top && y <= Rect.bottom;
        BOOL oldSel = y >= old->Rect.top && y <= old->Rect.bottom;
        return sel != oldSel;
    }

    // trim the selection so it does not exceed the width defined by maxX
    void Clip(int maxX)
    {
        if (FocusX > maxX)
            FocusX = maxX;
        if (AnchorX > maxX)
            AnchorX = maxX;
        Normalize();
    }

private:
    // compute Rect based on Focus? and Anchor?
    // must be called after each change of Focus? or Anchor?
    void Normalize();
};

//****************************************************************************
//
// CBookmark
//

struct CBookmark
{
public:
    int X; // X coordinate of the bookmark
    int Y; // Y coordinate of the bookmark
};

//****************************************************************************
//
// CBookmarkList
//

class CBookmarkList
{
private:
    TDirectArray<CBookmark> Bookmarks;

public:
    CBookmarkList();

    void Toggle(int x, int y);
    BOOL GetNext(int x, int y, int* newX, int* newY, BOOL next);
    BOOL IsMarked(int x, int y);
    void ClearAll();

    int GetCount() { return Bookmarks.Count; }

private:
    BOOL GetIndex(int x, int y, int* index);
};

//****************************************************************************
//
// CRendererWindow
//

#define UPDATE_VERT_SCROLL 0x00000001
#define UPDATE_HORZ_SCROLL 0x00000002

enum CDragSelectionMode
{
    dsmNormal,  // scrolling in both directions
    dsmColumns, // scrolling horizontally
    dsmRows,    // scrolling vertically
    dsmNone     // no scrolling
};

class CViewerWindow;

class CRendererWindow : public CWindow
{
public:
    CDatabase Database; // interface for working with the opened database
    CViewerWindow* Viewer;

    // graphics
    HPEN HGrayPen;
    HPEN HLtGrayPen;
    HPEN HSelectionPen;
    HPEN HBlackPen;
    HFONT HFont; // base font
    HICON HDeleteIcon;
    HICON HMarkedIcon;
    int RowHeight;           // row height
    int CharAvgWidth;        // average character width
    int TopTextMargin;       // vertical text offset within a row
    int LeftTextMargin;      // horizontal text offset within a row
    int Width;               // client width
    int Height;              // client height
    int RowsOnPage;          // number of fully visible rows on the page
    int TopIndex;            // index of the first displayed row
    int XOffset;             // X coordinate of the first displayed point
    CSelection Selection;    // focus and selection placement
    CSelection OldSelection; // previous focus and selection placement
    CBookmarkList Bookmarks; // bookmarks

    CDragSelectionMode DragMode; // selection is currently dragged with the mouse
    DWORD_PTR ScrollTimerID;

    int DragColumn;       // if not -1, we drag the width of this visible column
    int DragColumnOffset; // meaningful only when DragColumn != -1

    BOOL Creating; // window is being created -- do not erase background yet

    BOOL AutoSelect;
    char Coding[210];
    char DefaultCoding[210];
    BOOL UseCodeTable; // should the CodeTable be used for recoding?
    // CodeTable matters only when UseCodeTable is TRUE
    char CodeTable[256]; // translation table

protected:
    int EnumFilesSourceUID;    // source UID for enumerating viewer files
    int EnumFilesCurrentIndex; // index of the current file in the source

    // accumulates micro-steps when turning the mouse wheel, see
    // http://msdn.microsoft.com/en-us/library/ms997498.aspx (Best Practices for Supporting Microsoft Mouse and Keyboard Devices)
    int MouseWheelAccumulator;  // vertical
    int MouseHWheelAccumulator; // horizontal

public:
    CRendererWindow(int enumFilesSourceUID, int enumFilesCurrentIndex);
    ~CRendererWindow();

    void OnFileOpen();
    void OnFileReOpen();
    void OnGoto();

    BOOL OpenFile(const char* name, BOOL useDefaultConfig);

    // update scroll bar information
    void SetupScrollBars(DWORD update = UPDATE_VERT_SCROLL | UPDATE_HORZ_SCROLL);

    void RebuildGraphics(); // called when the configuration changes

    // copy the current selection to the clipboard
    void CopySelectionToClipboard();

    // set the selection to cover all cells and redraw the screen
    void SelectAll();

    // if conversion == NULL, "Don't Convert" will be applied
    void SelectConversion(const char* conversion);

    // invoke the column management dialog
    void ColumnsWasChanged();

    // Bookmarks
    void OnToggleBookmark();
    void OnNextBookmark(BOOL next);
    void OnClearBookmarks();
    int GetBookmarkCount() { return Bookmarks.GetCount(); }

    // Find
    void Find(BOOL forward, BOOL wholeWords,
              CSalamanderBMSearchData* bmSearchData,
              CSalamanderREGEXPSearchData* regexpSearchData);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void PaintTopMargin(HDC hDC, HRGN hUpdateRgn, const RECT* clipRect, BOOL selChangeOnly);
    void PaintBody(HDC hDC, HRGN hUpdateRgn, const RECT* clipRect, BOOL selChangeOnly);
    void Paint(HDC hDC, HRGN hUpdateRgn, BOOL selChangeOnly);

    void EnsureColumnIsVisible(int x);
    void EnsureRowIsVisible(int y);

    void CreateGraphics();  // initialize handles
    void ReleaseGraphics(); // destroy handles

    // [x,y] are client coordinates of the window; returns [column,row] - cell coordinates
    // if getNearest is TRUE, returns the nearest cell even if it is not directly under the point
    // returns TRUE when a cell was found and the column and row values were set
    BOOL HitTest(int x, int y, int* column, int* row, BOOL getNearest);
    BOOL HitTestRow(int y, int* row, BOOL getNearest);
    BOOL HitTestColumn(int x, int* column, BOOL getNearest);
    // return TRUE if x is approximately above a divider between columns
    // column and offset can be NULL
    BOOL HitTestColumnSplit(int x, int* column, int* offset);

    // start editing the cell under the focus
    //    void OnEditCell();

    // find the column with the visibleIndex; if found,
    // set index according to its position in Database.Column and xPos to
    // its X coordinate and return TRUE;
    // otherwise return FALSE
    BOOL GetColumnInfo(int visibleIndex, int* index, int* xPos);

    // enable/disable the timer and state variables during block dragging
    void BeginSelectionDrag(CDragSelectionMode mode);
    void EndSelectionDrag();
    void OnTimer(WPARAM wParam);

    // finish column-width dragging
    void EndColumnDrag();

    void OnHScroll(int scrollCode, int pos);
    void OnVScroll(int scrollCode, int pos);

    // called after resizing the window to prevent nonsensical scrolling
    void CheckAndCorrectBoundaries();

    // prepare the buffer and let Salamander detect the code page
    void RecognizeCodePage();
    // convert characters in the text buffer
    void CodeCharacters(char* text, size_t textLen);

    void SetViewerTitle();

    // return the point above the focus or the selected block
    void GetContextMenuPos(POINT* p);

    // display the context menu for the selected block at position 'p'
    void OnContextMenu(const POINT* p);

    void ResetMouseWheelAccumulator()
    {
        MouseWheelAccumulator = 0;
        MouseHWheelAccumulator = 0;
    }
    void ResetMouseWheelAccumulatorHandler(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CViewerWindow;
};
