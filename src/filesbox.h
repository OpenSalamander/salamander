// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ****************************************************************************

class CFilesWindow;
class CFilesBox;

#define UPDATE_VERT_SCROLL 0x00000001
#define UPDATE_HORZ_SCROLL 0x00000002

enum CViewModeEnum
{
    vmBrief,      // several columns of data; horizontal scrollbar only; bottom items are always fully visible
    vmDetailed,   // single column of data; both scrollbars shown; last row may be partially visible
    vmIcons,      // large icons left-to-right then top-to-bottom; vertical scrollbar only
    vmThumbnails, // thumbnails left-to-right then top-to-bottom; vertical scrollbar only
    vmTiles       // large (48x48) icons left-to-right then top-to-bottom; vertical scrollbar only
};

//****************************************************************************
//
// CBottomBar
//

// holds the scrollbar and the rectangle behind it
class CBottomBar : public CWindow
{
protected:
    HWND HScrollBar;
    BOOL VertScrollSpace;   // reserve space for the vertical scrollbar
    CFilesBox* RelayWindow; // used to relay messages

public:
    CBottomBar();
    ~CBottomBar();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LayoutChilds(); // computes rectangles and positions child windows

    friend class CFilesBox;
};

//****************************************************************************
//
// CHeaderLine
//

enum CHeaderHitTestEnum
{
    hhtNone,    // area after the last item
    hhtItem,    // item
    hhtDivider, // divider of an item with adjustable width
};

class CHeaderLine : public CWindow
{
protected:
    CFilesBox* Parent;
    TDirectArray<CColumn>* Columns; // pointer to the array held in CFilesWindow
    int Width;
    int Height;
    int HotIndex;        // index of the column currently highlighted
    BOOL HotExt;         // meaningful only when HotIndex==0 (Name)
    int DownIndex;       // column index where the LDOWN occurred
    BOOL DownVisible;    // is it currently pressed?
    int DragIndex;       // index of the column whose width is being set
    BOOL MouseIsTracked; // is the mouse tracked using TrackMouseEvent?
    int OldDragColWidth; // column width before dragging started; used when resizing the column width

public:
    CHeaderLine();
    ~CHeaderLine();

    // walks through the Columns array and sets minimal widths for columns
    // based on column names and whether sorting is possible by the column
    // (then reserves space for the arrow symbol if needed)
    void SetMinWidths();

    // Determines which part contains point [xPos, yPos] (in client coordinates).
    // If it returns hhtItem or hhtDivider, it sets the variable 'index'
    // to index of the column from Columns array.
    // If "Name" was clicked, it sets the variable 'extInName'
    // When "Ext" is clicked it becomes TRUE, otherwise FALSE.
    CHeaderHitTestEnum HitTest(int xPos, int yPos, int& index, BOOL& extInName);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void PaintAllItems(HDC hDC, HRGN hUpdateRgn);
    void PaintItem(HDC hDC, int index, int x = -1); // for 'x==-1' the correct position is computed
    void PaintItem2(int index);
    void Cancel();

    friend class CFilesBox;
    friend class CFilesWindow;
};

//****************************************************************************
//
// CFilesBox
//

class CFilesBox : public CWindow
{
protected:
    CFilesWindow* Parent;
    HDC HPrivateDC; // each panel has its own DC
    HWND HVScrollBar;
    HWND HHScrollBar;
    CBottomBar BottomBar;
    CHeaderLine HeaderLine;

    RECT ClientRect;    // dimensions of the entire window
    RECT HeaderRect;    // position of the header control within ClientRect
    RECT VScrollRect;   // position of the vertical scrollbar within ClientRect
    RECT BottomBarRect; // position of the horizontal scrollbar within ClientRect
    RECT FilesRect;     // position of the drawing area within ClientRect

    int TopIndex;            // vmBrief || vmDetailed: index of the first displayed item
                             // vmIcons || vmThumbnails: panel scrolled in pixels
    int XOffset;             // X coordinate of the first visible point
    int ItemsCount;          // total number of items
    int EntireItemsInColumn; // number of fully visible items in a column
    int ItemsInColumn;       // total number of rows
    int EntireColumnsCount;  // number of fully visible columns - valid only in Brief mode
    int ColumnsCount;        // total number of columns

    SCROLLINFO OldVertSI; // to avoid setting the scrollbar unnecessarily
    SCROLLINFO OldHorzSI; // to avoid setting the scrollbar unnecessarily

    CViewModeEnum ViewMode;
    BOOL HeaderLineVisible; // is the header line displayed?

    int ItemHeight; // item height
    int ItemWidth;

    int ThumbnailWidth; // thumbnail dimensions controlled by CConfiguration::ThumbnailSize
    int ThumbnailHeight;

    // used to accumulate microsteps when rotating the mouse wheel;
    // see http://msdn.microsoft.com/en-us/library/ms997498.aspx (Best Practices for Supporting Microsoft Mouse and Keyboard Devices)
    int MouseWheelAccumulator;  // vertical
    int MouseHWheelAccumulator; // horizontal

public:
    CFilesBox(CFilesWindow* parent);

    // sets the number of items in the panel
    // count             - number of items in the panel
    // suggestedXOffset  - new XOffset
    // suggestedTopIndex - new TopIndex
    // disableSBUpdate   - if FALSE, the change will not be reflected in the scrollbar
    void SetItemsCount(int count, int xOffset, int topIndex, BOOL disableSBUpdate);
    void SetItemsCount2(int count); // simple version: just sets the count and calls UpdateInternalData()
    int GetEntireItemsInColumn();

    int GetColumnsCount();

    void SetMode(CViewModeEnum mode, BOOL headerLine);

    int GetItemWidth() { return ItemWidth; }
    int GetItemHeight() { return ItemHeight; }
    void SetItemWidthHeight(int width, int height);

    int GetTopIndex() { return TopIndex; }
    int GetXOffset() { return XOffset; }

    // returns the index of the item at coordinates 'x' and 'y'; obeys 'nearest'
    // if 'labelRect' != NULL, returns the position of rectangle around the text
    // if no item matching the criteria is found, returns INT_MAX
    int GetIndex(int x, int y, BOOL nearest = FALSE, RECT* labelRect = NULL);

    void PaintAllItems(HRGN hUpdateRgn, DWORD drawFlags);
    void PaintItem(int index, DWORD drawFlags);

    void PaintHeaderLine(); // if it is visible it will be repainted

    // in 'rect' returns the bounding rectangle of item 'index'
    // returns TRUE if the rectangle is at least partially visible
    BOOL GetItemRect(int index, RECT* rect);

    // forcePaint - even if no scrolling occurs the item will be repainted
    // scroll: FALSE - the item will be fully visible; TRUE - the item will be at least partially visible
    // selFocChangeOnly: if not scrolled, DRAWFLAG_SELFOC_CHANGE is used when painting
    void EnsureItemVisible(int index, BOOL forcePaint = FALSE, BOOL scroll = FALSE, BOOL selFocChangeOnly = FALSE);

    // returns the new TopIndex that EnsureItemVisible would set for scroll == FALSE
    int PredictTopIndex(int index);

    // special version: scrolls only the part of the screen that doesn't contain
    // the item index, then repaints the remaining items
    void EnsureItemVisible2(int newTopIndex, int index);

    // returns TRUE if item 'index' is at least partially visible; if 'isFullyVisible' is not NULL,
    // it receives TRUE when the item is fully visible (FALSE = not visible or only partially)
    BOOL IsItemVisible(int index, BOOL* isFullyVisible);

    // returns the index of the first at least partially visible item and the
    // number of partially visible items (determines all at least partially visible
    // items - used when prioritizing icon and thumbnail loading)
    void GetVisibleItems(int* firstIndex, int* count);

    CHeaderLine* GetHeaderLine() { return &HeaderLine; }

    // resets the accumulator used for mouse wheel detection; after reset, the next rotation starts from scratch
    // Microsoft recommends in Best Practices for Supporting Microsoft Mouse and Keyboard Devices (http://msdn.microsoft.com/en-us/library/ms997498.aspx)
    // to clear the accumulator when the wheel direction changes or the window focus changes
    void ResetMouseWheelAccumulator()
    {
        MouseWheelAccumulator = 0;
        MouseHWheelAccumulator = 0;
    }

protected:
    void LayoutChilds(BOOL updateAndCheck = TRUE);                               // computes rectangle size and positions child windows
    void SetupScrollBars(DWORD flags = UPDATE_VERT_SCROLL | UPDATE_HORZ_SCROLL); // updates scrollbar information
    BOOL ShowHideChilds();                                                       // shows or hides children based on Mode; returns TRUE when a change occurs otherwise returns FALSE
    void UpdateInternalData();
    void CheckAndCorrectBoundaries(); // ensures scrollbars are in valid ranges, adjusting them if needed

    void OnHScroll(int scrollCode, int pos);
    void OnVScroll(int scrollCode, int pos);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CFilesWindow;
    friend class CFilesMap;
    friend class CScrollPanel;
    friend class CBottomBar;
    friend class CHeaderLine;
};
