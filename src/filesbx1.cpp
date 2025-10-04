// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "filesbox.h"
#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "editwnd.h"
#include "mainwnd.h"
#include "stswnd.h"
#include "shellib.h"
#include "snooper.h"

const char* CFILESBOX_CLASSNAME = "SalamanderItemsBox";

//****************************************************************************
//
// CFilesBox
//

CFilesBox::CFilesBox(CFilesWindow* parent)
    : CWindow(ooStatic)
{
    BottomBar.RelayWindow = this;
    Parent = parent;
    HPrivateDC = NULL;
    HVScrollBar = NULL;
    HHScrollBar = NULL;
    ZeroMemory(&ClientRect, sizeof(ClientRect));
    ZeroMemory(&HeaderRect, sizeof(HeaderRect));
    ZeroMemory(&VScrollRect, sizeof(VScrollRect));
    ZeroMemory(&BottomBarRect, sizeof(BottomBarRect));
    ZeroMemory(&FilesRect, sizeof(FilesRect));
    ItemsCount = 0;
    TopIndex = 0;
    XOffset = 0;
    ViewMode = vmDetailed;
    HeaderLineVisible = TRUE;

    OldVertSI.cbSize = 0;
    OldHorzSI.cbSize = 0;

    ItemHeight = 10;      // dummy
    ItemWidth = 10;       // dummy
    ThumbnailWidth = 10;  // dummy
    ThumbnailHeight = 10; // dummy
    EntireItemsInColumn = 1;
    ItemsInColumn = 1;
    EntireColumnsCount = 1;

    HeaderLine.Parent = this;
    HeaderLine.Columns = &Parent->Columns;

    ResetMouseWheelAccumulator();
}

void CFilesBox::SetItemsCount(int count, int xOffset, int topIndex, BOOL disableSBUpdate)
{
    SetItemsCount2(count);
    XOffset = xOffset;
    TopIndex = topIndex;
    OldVertSI.cbSize = 0;
    OldHorzSI.cbSize = 0;
    if (!disableSBUpdate)
    {
        SetupScrollBars();
        CheckAndCorrectBoundaries();
    }
    Parent->VisibleItemsArray.InvalidateArr();
    Parent->VisibleItemsArraySurround.InvalidateArr();
}

void CFilesBox::SetItemsCount2(int count)
{
    ItemsCount = count;
    UpdateInternalData();
}

void CFilesBox::SetMode(CViewModeEnum mode, BOOL headerLine)
{
    ViewMode = mode;
    HeaderLineVisible = headerLine;
    BOOL change = ShowHideChilds();
    LayoutChilds(change);
    if (change)
        InvalidateRect(HWindow, &FilesRect, FALSE);
}

void CFilesBox::SetItemWidthHeight(int width, int height)
{
    if (height < 1)
    {
        TRACE_E("height=" << height);
        height = 1;
    }
    if (width < 1)
    {
        TRACE_E("width=" << width);
        width = 1;
    }
    if (width != ItemWidth || height != ItemHeight)
    {
        ItemWidth = width;
        ItemHeight = height;
        UpdateInternalData();
        ItemBitmap.Enlarge(ItemWidth, ItemHeight); // for the header line
                                                   //    OldVertSI.cbSize = 0;
                                                   //    OldHorzSI.cbSize = 0;
                                                   //    SetupScrollBars();
                                                   //    CheckAndCorrectBoundaries();
    }
}

void CFilesBox::UpdateInternalData()
{
    EntireItemsInColumn = (FilesRect.bottom - FilesRect.top) / ItemHeight;
    if (EntireItemsInColumn < 1)
        EntireItemsInColumn = 1;
    switch (ViewMode)
    {
    case vmDetailed:
    {
        ColumnsCount = 1;
        EntireColumnsCount = 1;
        ItemsInColumn = ItemsCount;
        break;
    }

    case vmBrief:
    {
        ColumnsCount = (ItemsCount + (EntireItemsInColumn - 1)) / EntireItemsInColumn;
        EntireColumnsCount = (FilesRect.right - FilesRect.left) / ItemWidth;
        if (EntireColumnsCount < 1)
            EntireColumnsCount = 1;
        if (EntireColumnsCount > ColumnsCount)
            EntireColumnsCount = ColumnsCount;
        ItemsInColumn = EntireItemsInColumn;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        ColumnsCount = (FilesRect.right - FilesRect.left) / ItemWidth;
        if (ColumnsCount < 1)
            ColumnsCount = 1;
        EntireColumnsCount = ColumnsCount;
        ItemsInColumn = (ItemsCount + (EntireColumnsCount - 1)) / EntireColumnsCount;
        break;
    }
    }
}

int CFilesBox::GetEntireItemsInColumn()
{
    return EntireItemsInColumn;
}

int CFilesBox::GetColumnsCount()
{
    return 1;
}

void CFilesBox::PaintAllItems(HRGN hUpdateRgn, DWORD drawFlags)
{
    int index = TopIndex;

    if (hUpdateRgn != NULL)
        SelectClipRgn(HPrivateDC, hUpdateRgn);

    RECT clipRect;
    int clipRectType = NULLREGION;
    if ((drawFlags & DRAWFLAG_IGNORE_CLIPBOX) == 0)
        clipRectType = GetClipBox(HPrivateDC, &clipRect);

    if (clipRectType == NULLREGION ||
        (clipRectType == SIMPLEREGION &&
         clipRect.left <= FilesRect.left && clipRect.top <= FilesRect.top &&
         clipRect.right >= FilesRect.right && clipRect.bottom >= FilesRect.bottom))
    {
        // if no clipping region is set or it is a simple rectangle,
        // whose clipping is provided in the following test, we will not waste
        // time testing the visibility of individual items
        drawFlags |= DRAWFLAG_SKIP_VISTEST;
    }

    BOOL showDragBox = FALSE;
    if (Parent->DragBox && Parent->DragBoxVisible)
    {
        Parent->DrawDragBox(Parent->OldBoxPoint); // hide the box
        showDragBox = TRUE;
    }
    if (ImageDragging)
        ImageDragShow(FALSE);

    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed mode
        int y = FilesRect.top;
        int yMax = FilesRect.bottom;
        RECT r;
        r.left = FilesRect.left;
        r.right = FilesRect.right;

        // by offsetting between drawing operations we avoid drawing items outside the clipping rectangle
        if (clipRectType == SIMPLEREGION || clipRectType == COMPLEXREGION)
        {
            RECT tmpRect = clipRect;
            if (tmpRect.top < FilesRect.top)
                tmpRect.top = FilesRect.top;
            if (tmpRect.bottom > FilesRect.bottom)
                tmpRect.bottom = FilesRect.bottom;

            // shift the top boundary
            int indexOffset = (tmpRect.top - FilesRect.top) / ItemHeight;
            index += indexOffset;
            y += indexOffset * ItemHeight;
            // shift the bottom boundary
            yMax -= FilesRect.bottom - tmpRect.bottom;
        }

        while (y < yMax && index >= 0 && index < ItemsCount)
        {
            r.top = y;
            r.bottom = y + ItemHeight;
            Parent->DrawBriefDetailedItem(HPrivateDC, index, &r, drawFlags);
            y += ItemHeight;
            index++;
        }

        if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && !(drawFlags & DRAWFLAG_ICON_ONLY) && y < yMax)
        {
            // erase the bottom area
            r.top = y;
            r.bottom = yMax;
            FillRect(HPrivateDC, &r, HNormalBkBrush);
        }
        break;
    }

    case vmBrief:
    {
        // vmBrief mode
        int x = FilesRect.left;
        int y = FilesRect.top;
        int yMax = FilesRect.bottom;
        int xMax = FilesRect.right;
        RECT r;
        r.left = FilesRect.left;
        r.right = FilesRect.left + ItemWidth;

        int firstCol = index / EntireItemsInColumn;
        //      int lastCol = firstCol + (FilesRect.right - FilesRect.left + ItemWidth - 1) / ItemWidth - 1;

        // by offsetting between drawing operations we avoid drawing items outside the clipping rectangle
        if (clipRectType == SIMPLEREGION || clipRectType == COMPLEXREGION)
        {
            RECT tmpRect = clipRect;
            if (tmpRect.left < FilesRect.left)
                tmpRect.left = FilesRect.left;
            if (tmpRect.right > FilesRect.right)
                tmpRect.right = FilesRect.right;

            // shift the left boundary
            int colOffset = (tmpRect.left - FilesRect.left) / ItemWidth;
            firstCol += colOffset;
            r.left += colOffset * ItemWidth;
            // shift the right boundary
            xMax -= FilesRect.right - tmpRect.right;
        }

        index = firstCol * EntireItemsInColumn;
        BOOL painted = FALSE;
        while (index < ItemsCount && r.left <= xMax)
        {
            r.top = y;
            r.bottom = y + ItemHeight;
            r.right = r.left + ItemWidth;
            Parent->DrawBriefDetailedItem(HPrivateDC, index, &r, drawFlags);
            painted = TRUE;
            y += ItemHeight;
            if (y + ItemHeight > yMax)
            {
                if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && y < yMax)
                {
                    // erase the bottom below a full column
                    r.top = y;
                    r.bottom = yMax;
                    FillRect(HPrivateDC, &r, HNormalBkBrush);
                }
                y = FilesRect.top;
                r.left += ItemWidth;
                firstCol++;
            }
            index++;
        }

        if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && painted && y > r.top && y < FilesRect.bottom)
        {
            // erase the area below an incomplete last column
            r.top = y;
            r.bottom = FilesRect.bottom;
            FillRect(HPrivateDC, &r, HNormalBkBrush);
            r.left += ItemWidth;
        }
        if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && r.left <= FilesRect.right)
        {
            // erase the area to the right of the last column
            r.top = FilesRect.top;
            r.right = FilesRect.right;
            r.bottom = FilesRect.bottom;
            FillRect(HPrivateDC, &r, HNormalBkBrush);
        }
        break;
    }

    case vmThumbnails:
    case vmIcons:
    case vmTiles:
    {
        // vmIcons || vmThumbnails mode
        RECT r;

        // compute the drawing boundaries
        int firstRow = TopIndex / ItemHeight;
        int xMax = FilesRect.right;
        int yMax = FilesRect.bottom;

        // limit drawing only to the area requesting a repaint
        if (clipRectType == SIMPLEREGION || clipRectType == COMPLEXREGION)
        {
            RECT tmpRect = clipRect;
            if (tmpRect.top < FilesRect.top)
                tmpRect.top = FilesRect.top;
            if (tmpRect.bottom > FilesRect.bottom)
                tmpRect.bottom = FilesRect.bottom;

            // shift the top boundary
            firstRow = (TopIndex + tmpRect.top) / ItemHeight;
            // shift the right boundary
            yMax -= FilesRect.bottom - tmpRect.bottom;
        }

        // control variables for positioning individual items
        int x = FilesRect.left;
        int y = FilesRect.top - TopIndex + firstRow * ItemHeight;

        // draw from right to left and when we hit the area boundary,
        // move down to the next row
        int index2 = firstRow * ColumnsCount;
        while (index2 < ItemsCount && y < yMax)
        {
            r.top = y;
            r.bottom = y + ItemHeight;
            r.left = x;
            r.right = x + ItemWidth;
            // draw the actual item
            CIconSizeEnum iconSize = (ViewMode == vmIcons) ? ICONSIZE_32 : ICONSIZE_48;
            if (ViewMode == vmTiles)
                Parent->DrawTileItem(HPrivateDC, index2, &r, drawFlags, iconSize);
            else
                Parent->DrawIconThumbnailItem(HPrivateDC, index2, &r, drawFlags, iconSize);
            // move to the next one
            x += ItemWidth;
            // if this is the last item on the row, clear the space after it
            if (x + ItemWidth > xMax || index2 == ItemsCount - 1)
            {
                if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && x < xMax)
                {
                    r.left = x;
                    r.right = xMax;
                    FillRect(HPrivateDC, &r, HNormalBkBrush);
                }
                x = FilesRect.left;
                y += ItemHeight;
            }
            index2++;
        }

        if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && !(drawFlags & DRAWFLAG_ICON_ONLY) && y < yMax)
        {
            // erase the bottom area
            r.left = FilesRect.left;
            r.right = FilesRect.right;
            r.top = y;
            r.bottom = FilesRect.bottom;
            FillRect(HPrivateDC, &r, HNormalBkBrush);
        }
        break;
    }
    }

    // highlight the fact that the panel is empty
    if (ItemsCount == 0 && (drawFlags & DRAWFLAG_ICON_ONLY) == 0)
    {
        char textBuf[300];
        textBuf[0] = 0;
        if (!Parent->Is(ptPluginFS) || !Parent->GetPluginFS()->NotEmpty() ||
            !Parent->GetPluginFS()->GetNoItemsInPanelText(textBuf, 300))
        {
            lstrcpyn(textBuf, LoadStr(IDS_NOITEMSINPANEL), 300);
        }
        RECT textR = FilesRect;
        textR.bottom = textR.top + FontCharHeight + 4;
        BOOL focused = (Parent->FocusVisible || Parent->Parent->EditMode && Parent->Parent->GetActivePanel() == Parent);
        COLORREF newColor;
        if (focused)
        {
            FillRect(HPrivateDC, &textR, HFocusedBkBrush);
            newColor = GetCOLORREF(CurrentColors[ITEM_FG_FOCUSED]);
        }
        else
        {
            FillRect(HPrivateDC, &textR, HNormalBkBrush);
            newColor = GetCOLORREF(CurrentColors[ITEM_FG_NORMAL]);
        }
        int oldBkMode = SetBkMode(HPrivateDC, TRANSPARENT);
        int oldTextColor = SetTextColor(HPrivateDC, newColor);
        HFONT hOldFont = (HFONT)SelectObject(HPrivateDC, Font);
        // if it flickers we can measure the text and use ExtTextOut
        DrawText(HPrivateDC, textBuf, -1,
                 &textR, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(HPrivateDC, hOldFont);
        SetTextColor(HPrivateDC, oldTextColor);
        SetBkMode(HPrivateDC, oldBkMode);
    }

    if (ImageDragging)
        ImageDragShow(TRUE);
    if (showDragBox)
        Parent->DrawDragBox(Parent->OldBoxPoint); // show it again

    if (hUpdateRgn != NULL)
        SelectClipRgn(HPrivateDC, NULL); // remove the clipping region if we set it
}

void CFilesBox::PaintItem(int index, DWORD drawFlags)
{
    RECT r;
    if (GetItemRect(index, &r))
    {
        BOOL showDragBox = FALSE;
        if (Parent->DragBox && Parent->DragBoxVisible)
        {
            Parent->DrawDragBox(Parent->OldBoxPoint); // hide the box
            showDragBox = TRUE;
        }
        // hide the drag image if necessary
        BOOL showImage = FALSE;
        if (ImageDragging)
        {
            RECT r2 = r;
            POINT p;
            p.x = r2.left;
            p.y = r2.top;
            ClientToScreen(HWindow, &p);
            r2.right = p.x + r2.right - r2.left;
            r2.bottom = p.y + r2.bottom - r2.top;
            r2.left = p.x;
            r2.top = p.y;
            if (ImageDragInterfereRect(&r2))
            {
                ImageDragShow(FALSE);
                showImage = TRUE;
            }
        }
        switch (ViewMode)
        {
        case vmBrief:
        case vmDetailed:
        {
            // skip visibility test - we want the best performance
            Parent->DrawBriefDetailedItem(HPrivateDC, index, &r, drawFlags | DRAWFLAG_SKIP_VISTEST);
            break;
        }

        case vmIcons:
        case vmThumbnails:
        {
            // skip visibility test - we want the best performance
            CIconSizeEnum iconSize = (ViewMode == vmIcons) ? ICONSIZE_32 : ICONSIZE_48;
            Parent->DrawIconThumbnailItem(HPrivateDC, index, &r, drawFlags | DRAWFLAG_SKIP_VISTEST, iconSize);
            break;
        }

        case vmTiles:
        {
            // skip visibility test - we want the best performance
            CIconSizeEnum iconSize = ICONSIZE_48;
            Parent->DrawTileItem(HPrivateDC, index, &r, drawFlags | DRAWFLAG_SKIP_VISTEST, iconSize);
            break;
        }
        }
        if (showImage)
            ImageDragShow(TRUE);
        if (showDragBox)
            Parent->DrawDragBox(Parent->OldBoxPoint); // show it again
    }
}

BOOL CFilesBox::GetItemRect(int index, RECT* rect)
{
    switch (ViewMode)
    {
    case vmDetailed:
    {
        rect->left = FilesRect.left;
        rect->right = FilesRect.right;
        rect->top = FilesRect.top + ItemHeight * (index - TopIndex);
        rect->bottom = rect->top + ItemHeight;
        break;
    }

    case vmBrief:
    {
        int leftColumn = TopIndex / ItemWidth;
        int col = (index - TopIndex) / EntireItemsInColumn;
        int row = (index - TopIndex) % EntireItemsInColumn;
        rect->left = FilesRect.left + col * ItemWidth;
        rect->right = rect->left + ItemWidth;
        rect->top = FilesRect.top + row * ItemHeight;
        rect->bottom = rect->top + ItemHeight;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        int row = index / ColumnsCount;
        int col = index % ColumnsCount;
        rect->left = FilesRect.left + col * ItemWidth;
        rect->right = rect->left + ItemWidth;
        rect->top = FilesRect.top + row * ItemHeight - TopIndex;
        rect->bottom = rect->top + ItemHeight;
        break;
    }
    }
    RECT dummy;
    return IntersectRect(&dummy, rect, &FilesRect) != 0;
}

void CFilesBox::EnsureItemVisible(int index, BOOL forcePaint, BOOL scroll, BOOL selFocChangeOnly)
{
    if (index < 0 || index >= ItemsCount)
    {
        TRACE_E("index=" << index);
        return;
    }

    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed
        int newTopIndex = TopIndex;
        if (index < TopIndex)
            newTopIndex = index;
        else
        {
            if (index > TopIndex + EntireItemsInColumn - 1)
            {
                if (!scroll || ItemHeight * (index - TopIndex) >= FilesRect.bottom - FilesRect.top)
                    newTopIndex = index - EntireItemsInColumn + 1;
            }
        }

        if (newTopIndex != TopIndex)
        {
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            if (ImageDragging)
                ImageDragShow(FALSE);
            ScrollWindowEx(HWindow, 0, ItemHeight * (TopIndex - newTopIndex),
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newTopIndex;
            // compute the region of the new item and add it to the region created during scrolling
            HRGN hItemRgn = HANDLES(CreateRectRgn(FilesRect.left,
                                                  FilesRect.top + ItemHeight * (index - TopIndex),
                                                  FilesRect.right,
                                                  FilesRect.top + ItemHeight * (index - TopIndex + 1)));
            CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
            HANDLES(DeleteObject(hItemRgn));
            SetupScrollBars(UPDATE_VERT_SCROLL);
            // when the Selection bit changes the Dirty flag is set and we must
            // not clear it while scrolling because partially visible items are
            // not redrawn completely and will need to be repainted once again
            PaintAllItems(hUpdateRgn, DRAWFLAG_KEEP_DIRTY);
            if (ImageDragging)
                ImageDragShow(TRUE);
            HANDLES(DeleteObject(hUpdateRgn));
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
        }
        else if (forcePaint)
            PaintItem(index, selFocChangeOnly ? DRAWFLAG_SELFOC_CHANGE : 0);
        break;
    }

    case vmBrief:
    {
        // vmBrief
        int leftCol = TopIndex / EntireItemsInColumn;
        int newLeftCol = leftCol;

        int col = index / EntireItemsInColumn;

        if (col < leftCol)
            newLeftCol = col;
        else
        {
            if (col >= leftCol + EntireColumnsCount)
            {
                if (!scroll || ItemWidth * (col - leftCol) >= FilesRect.right - FilesRect.left)
                {
                    newLeftCol = col - EntireColumnsCount + 1;
                    if (newLeftCol > ColumnsCount - EntireColumnsCount)
                        newLeftCol = ColumnsCount - EntireColumnsCount;
                }
            }
        }

        if (newLeftCol != leftCol)
        {
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            if (ImageDragging)
                ImageDragShow(FALSE);
            ScrollWindowEx(HWindow, ItemWidth * (leftCol - newLeftCol), 0,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newLeftCol * EntireItemsInColumn;
            // compute the region of the new item and add it to the region created during scrolling
            HRGN hItemRgn = HANDLES(CreateRectRgn(FilesRect.left + ItemWidth * (col - newLeftCol),
                                                  FilesRect.top,
                                                  FilesRect.left + ItemWidth * (col - newLeftCol + 1),
                                                  FilesRect.bottom));
            CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
            HANDLES(DeleteObject(hItemRgn));
            SetupScrollBars(UPDATE_HORZ_SCROLL);
            // when the Selection bit changes the Dirty flag is set and we must
            // not clear it while scrolling because partially visible items are
            // not redrawn completely and will need to be repainted once again
            PaintAllItems(hUpdateRgn, DRAWFLAG_KEEP_DIRTY);
            if (ImageDragging)
                ImageDragShow(TRUE);
            HANDLES(DeleteObject(hUpdateRgn));
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
        }
        else if (forcePaint)
            PaintItem(index, selFocChangeOnly ? DRAWFLAG_SELFOC_CHANGE : 0);
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        // Icons || Thumbnails
        int newTopIndex = TopIndex;

        // compute the item position
        int itemTop = FilesRect.top + (index / ColumnsCount) * ItemHeight;
        int itemBottom = itemTop + ItemHeight;

        if (itemTop < TopIndex)
        {
            if (!scroll || itemBottom <= TopIndex)
                newTopIndex = itemTop;
        }
        else
        {
            int pageHeight = FilesRect.bottom - FilesRect.top;
            if (itemBottom > TopIndex + pageHeight)
            {
                if (!scroll || itemTop >= TopIndex + pageHeight)
                    newTopIndex = itemBottom - pageHeight;
            }
        }

        if (newTopIndex != TopIndex)
        {
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            if (ImageDragging)
                ImageDragShow(FALSE);
            ScrollWindowEx(HWindow, 0, TopIndex - newTopIndex,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newTopIndex;
            // compute the region of the new item and add it to the region created during scrolling
            int itemLeft = FilesRect.left + (index % ColumnsCount) * ItemWidth;
            HRGN hItemRgn = HANDLES(CreateRectRgn(itemLeft, itemTop - TopIndex,
                                                  itemLeft + ItemWidth, itemBottom - TopIndex));
            CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
            HANDLES(DeleteObject(hItemRgn));
            SetupScrollBars(UPDATE_VERT_SCROLL);
            // when the Selection bit changes the Dirty flag is set and we must
            // not clear it while scrolling because partially visible items are
            // not redrawn completely and will need to be repainted once again
            PaintAllItems(hUpdateRgn, DRAWFLAG_KEEP_DIRTY);
            if (ImageDragging)
                ImageDragShow(TRUE);
            HANDLES(DeleteObject(hUpdateRgn));
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
        }
        else if (forcePaint)
            PaintItem(index, selFocChangeOnly ? DRAWFLAG_SELFOC_CHANGE : 0);
        break;
    }
    }
}

void CFilesBox::GetVisibleItems(int* firstIndex, int* count)
{
    *firstIndex = *count = 0;
    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed
        *firstIndex = TopIndex;
        *count = EntireItemsInColumn +
                 (ItemHeight * EntireItemsInColumn < FilesRect.bottom - FilesRect.top ? 1 : 0);
        break;
    }

    case vmBrief:
    {
        // vmBrief
        *firstIndex = TopIndex;
        *count = (EntireColumnsCount + (ItemWidth * EntireColumnsCount < FilesRect.right - FilesRect.left ? 1 : 0)) *
                 EntireItemsInColumn;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        // Icons || Thumbnails
        if (ItemHeight != 0)
        {
            *firstIndex = (TopIndex / ItemHeight) * ColumnsCount;
            int last = ((TopIndex + FilesRect.bottom - FilesRect.top - 1) / ItemHeight) * ColumnsCount +
                       ColumnsCount - 1;
            if (last >= *firstIndex)
                *count = last - *firstIndex + 1;
        }
        break;
    }
    }
    if (*firstIndex < 0)
        *firstIndex = 0;
    if (*count < 0)
        *count = 0;
}

// assumes scroll == TRUE - even a partially visible item counts as visible
BOOL CFilesBox::IsItemVisible(int index, BOOL* isFullyVisible)
{
    if (isFullyVisible != NULL)
        *isFullyVisible = FALSE;
    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed
        if (index < TopIndex)
            return FALSE; // too far up
        else
        {
            if (index > TopIndex + EntireItemsInColumn - 1)
            {
                if (ItemHeight * (index - TopIndex) >= FilesRect.bottom - FilesRect.top)
                    return FALSE; // too far down
            }
            else
            {
                if (isFullyVisible != NULL)
                    *isFullyVisible = TRUE;
            }
        }
        break;
    }

    case vmBrief:
    {
        // vmBrief
        int leftCol = TopIndex / EntireItemsInColumn;
        int col = index / EntireItemsInColumn;
        if (col < leftCol)
            return FALSE; // too far left
        else
        {
            if (col >= leftCol + EntireColumnsCount)
            {
                if (ItemWidth * (col - leftCol) >= FilesRect.right - FilesRect.left)
                    return FALSE; // too far right
            }
            else
            {
                if (isFullyVisible != NULL)
                    *isFullyVisible = TRUE;
            }
        }
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        // Icons || Thumbnails
        int itemTop = FilesRect.top + (index / ColumnsCount) * ItemHeight;
        int itemBottom = itemTop + ItemHeight;
        if (itemBottom <= TopIndex)
            return FALSE; // item is completely above the visible area
        if (itemTop >= TopIndex + FilesRect.bottom - FilesRect.top)
            return FALSE; // item is completely below the visible area
        if (isFullyVisible != NULL)
            *isFullyVisible = (itemTop >= TopIndex && itemBottom <= TopIndex + FilesRect.bottom - FilesRect.top);
        break;
    }
    }
    return TRUE; // it is visible
}

// assumes scroll == FALSE - the whole item will be visible
int CFilesBox::PredictTopIndex(int index)
{
    int newTopIndex = TopIndex;
    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed
        if (index < TopIndex)
            newTopIndex = index;
        else
        {
            if (index > TopIndex + EntireItemsInColumn - 1)
                newTopIndex = index - EntireItemsInColumn + 1;
        }
        break;
    }

    case vmBrief:
    {
        // vmBrief
        int leftCol = TopIndex / EntireItemsInColumn;
        int newLeftCol = leftCol;

        int col = index / EntireItemsInColumn;

        if (col < leftCol)
            newLeftCol = col;
        else
        {
            if (col >= leftCol + EntireColumnsCount)
            {
                newLeftCol = col - EntireColumnsCount + 1;
                if (newLeftCol > ColumnsCount - EntireColumnsCount)
                    newLeftCol = ColumnsCount - EntireColumnsCount;
            }
        }
        newTopIndex = newLeftCol * EntireItemsInColumn;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        // Icons || Thumbnails

        // compute the item position
        int itemTop = FilesRect.top + (index / ColumnsCount) * ItemHeight;
        int itemBottom = itemTop + ItemHeight;

        if (itemTop < TopIndex)
        {
            newTopIndex = itemTop;
        }
        else
        {
            int pageHeight = FilesRect.bottom - FilesRect.top;
            if (itemBottom > TopIndex + pageHeight)
                newTopIndex = itemBottom - pageHeight;
        }
        break;
    }
    }
    return newTopIndex;
}

void CFilesBox::EnsureItemVisible2(int newTopIndex, int index)
{
    if (newTopIndex == TopIndex)
        return;

    // adjust the scrolling area so that it does not contain the selected item
    RECT sRect = FilesRect;
    if (index == newTopIndex)
        sRect.top += ItemHeight;
    else
        sRect.bottom = sRect.top + ItemHeight * (index - newTopIndex);

    if (ImageDragging)
        ImageDragShow(FALSE);
    HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
    ScrollWindowEx(HWindow, 0, ItemHeight * (TopIndex - newTopIndex),
                   &sRect, &sRect, hUpdateRgn, NULL, 0);

    // compute the region of the new item and add it to the region created during scrolling
    RECT uRect = FilesRect;
    if (index == newTopIndex)
    {
        uRect.top = uRect.top + ItemHeight * (index - newTopIndex);
        uRect.bottom = uRect.top + ItemHeight;
    }
    else
    {
        uRect.top = uRect.top + ItemHeight * (index - newTopIndex);
    }
    HRGN hItemRgn = HANDLES(CreateRectRgn(uRect.left, uRect.top, uRect.right, uRect.bottom));
    CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
    HANDLES(DeleteObject(hItemRgn));

    TopIndex = newTopIndex;
    SetupScrollBars(UPDATE_VERT_SCROLL);
    // when the Selection bit changes the Dirty flag is set and we must
    // not clear it while scrolling because partially visible items are
    // not redrawn completely and will need to be repainted once again
    PaintAllItems(hUpdateRgn, DRAWFLAG_KEEP_DIRTY);
    HANDLES(DeleteObject(hUpdateRgn));
    if (ImageDragging)
        ImageDragShow(TRUE);
    Parent->VisibleItemsArray.InvalidateArr();
    Parent->VisibleItemsArraySurround.InvalidateArr();
}

void CFilesBox::OnHScroll(int scrollCode, int pos)
{
    if (Parent->DragBox && !Parent->ScrollingWindow) // dragging the cage - block mouse wheel scrolling
        return;
    if (ViewMode == vmDetailed)
    {
        if (FilesRect.right - FilesRect.left + 1 > ItemWidth)
            return;
        int newXOffset = XOffset;
        switch (scrollCode)
        {
        case SB_LINEUP:
        {
            newXOffset -= ItemHeight;
            break;
        }

        case SB_LINEDOWN:
        {
            newXOffset += ItemHeight;
            break;
        }

        case SB_PAGEUP:
        {
            newXOffset -= FilesRect.right - FilesRect.left;
            break;
        }

        case SB_PAGEDOWN:
        {
            newXOffset += FilesRect.right - FilesRect.left;
            break;
        }

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
        {
            newXOffset = pos;
            break;
        }
        }
        if (newXOffset < 0)
            newXOffset = 0;
        if (newXOffset >= ItemWidth - (FilesRect.right - FilesRect.left) + 1)
            newXOffset = ItemWidth - (FilesRect.right - FilesRect.left);
        if (newXOffset != XOffset)
        {
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            if (ImageDragging)
                ImageDragShow(FALSE);
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            ScrollWindowEx(HWindow, XOffset - newXOffset, 0,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            if (HeaderLine.HWindow != NULL)
            {
                HRGN hUpdateRgn2 = HANDLES(CreateRectRgn(0, 0, 0, 0));
                ScrollWindowEx(HeaderLine.HWindow, XOffset - newXOffset, 0,
                               NULL, NULL, hUpdateRgn2, NULL, 0);
                XOffset = newXOffset;
                HDC hDC = HANDLES(GetDC(HeaderLine.HWindow));
                HeaderLine.PaintAllItems(hDC, hUpdateRgn2);
                HANDLES(ReleaseDC(HeaderLine.HWindow, hDC));
                HANDLES(DeleteObject(hUpdateRgn2));
            }
            else
                XOffset = newXOffset;
            PaintAllItems(hUpdateRgn, 0);
            HANDLES(DeleteObject(hUpdateRgn));
            if (ImageDragging)
                ImageDragShow(TRUE);
        }
        if (scrollCode != SB_THUMBTRACK)
            SetupScrollBars(UPDATE_HORZ_SCROLL);
    }
    else
    {
        // vmBrief
        if (EntireColumnsCount >= ColumnsCount)
            return;
        int leftColumn = TopIndex / EntireItemsInColumn;
        int newLeftColumn = TopIndex / EntireItemsInColumn;
        switch (scrollCode)
        {
        case SB_LINEUP:
        {
            newLeftColumn -= 1;
            break;
        }

        case SB_LINEDOWN:
        {
            newLeftColumn += 1;
            break;
        }

        case SB_PAGEUP:
        {
            newLeftColumn -= EntireColumnsCount;
            break;
        }

        case SB_PAGEDOWN:
        {
            newLeftColumn += EntireColumnsCount;
            break;
        }

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
        {
            newLeftColumn = pos;
            break;
        }
        }
        if (newLeftColumn < 0)
            newLeftColumn = 0;
        if (newLeftColumn > ColumnsCount - EntireColumnsCount)
            newLeftColumn = ColumnsCount - EntireColumnsCount;
        if (newLeftColumn != leftColumn)
        {
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            if (ImageDragging)
                ImageDragShow(FALSE);
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            ScrollWindowEx(HWindow, ItemWidth * (leftColumn - newLeftColumn), 0,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newLeftColumn * EntireItemsInColumn /*+ topIndexOffset*/;
            PaintAllItems(hUpdateRgn, 0);
            HANDLES(DeleteObject(hUpdateRgn));
            if (ImageDragging)
                ImageDragShow(TRUE);
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
            if (scrollCode == SB_THUMBTRACK)
            {
                Parent->VisibleItemsArray.RefreshArr(Parent);         // do a hard refresh here
                Parent->VisibleItemsArraySurround.RefreshArr(Parent); // do a hard refresh here
            }
        }
        if (scrollCode != SB_THUMBTRACK)
            SetupScrollBars(UPDATE_HORZ_SCROLL);
    }
}

void CFilesBox::OnVScroll(int scrollCode, int pos)
{
    if (Parent->DragBox && !Parent->ScrollingWindow) // dragging the cage - block mouse wheel scrolling
        return;
    int newTopIndex = TopIndex;
    if (ViewMode == vmDetailed)
    {
        // Detailed
        if (EntireItemsInColumn + 1 > ItemsCount)
            return;
        switch (scrollCode)
        {
        case SB_LINEUP:
        {
            newTopIndex--;
            break;
        }

        case SB_LINEDOWN:
        {
            newTopIndex++;
            break;
        }

        case SB_PAGEUP:
        {
            int delta = EntireItemsInColumn - 1;
            if (delta <= 0)
                delta = 1;
            newTopIndex -= delta;
            break;
        }

        case SB_PAGEDOWN:
        {
            int delta = EntireItemsInColumn - 1;
            if (delta <= 0)
                delta = 1;
            newTopIndex += delta;
            break;
        }

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
        {
            newTopIndex = pos;
            break;
        }
        }
        if (newTopIndex < 0)
            newTopIndex = 0;
        if (newTopIndex >= ItemsCount - EntireItemsInColumn + 1)
            newTopIndex = ItemsCount - EntireItemsInColumn;

        if (newTopIndex != TopIndex)
        {
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            if (ImageDragging)
                ImageDragShow(FALSE);
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            ScrollWindowEx(HWindow, 0, ItemHeight * (TopIndex - newTopIndex),
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newTopIndex;
            PaintAllItems(hUpdateRgn, 0);
            HANDLES(DeleteObject(hUpdateRgn));
            if (ImageDragging)
                ImageDragShow(TRUE);
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
            if (scrollCode == SB_THUMBTRACK)
            {
                Parent->VisibleItemsArray.RefreshArr(Parent);         // do a hard refresh here
                Parent->VisibleItemsArraySurround.RefreshArr(Parent); // do a hard refresh here
            }
        }
        if (scrollCode != SB_THUMBTRACK)
            SetupScrollBars(UPDATE_VERT_SCROLL);
    }
    else
    {
        // Icons || Thumbnails

        // what was this test for? It interferes with scrolling when the panel height is smaller than the thumbnail/icon
        //    if (EntireItemsInColumn * ItemHeight >= FilesRect.bottom - FilesRect.top)
        //      return;
        int lineDelta = ItemHeight;
        if (ItemHeight >= FilesRect.bottom - FilesRect.top)
            lineDelta = max(1, FilesRect.bottom - FilesRect.top); // guard against negative or zero delta
        switch (scrollCode)
        {
        case SB_LINEUP:
        {
            newTopIndex -= lineDelta;
            break;
        }

        case SB_LINEDOWN:
        {
            newTopIndex += lineDelta;
            break;
        }

        case SB_PAGEUP:
        case SB_PAGEDOWN:
        {
            int delta = EntireItemsInColumn * lineDelta; //FilesRect.bottom - FilesRect.top;
            if (delta <= 0)
                delta = 1;
            if (scrollCode == SB_PAGEUP)
                newTopIndex -= delta;
            else
                newTopIndex += delta;
            break;
        }

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
        {
            newTopIndex = pos;
            break;
        }
        }
        if (newTopIndex > ItemsInColumn * ItemHeight - (FilesRect.bottom - FilesRect.top))
            newTopIndex = ItemsInColumn * ItemHeight - (FilesRect.bottom - FilesRect.top);
        if (newTopIndex < 0)
            newTopIndex = 0;

        if (newTopIndex != TopIndex)
        {
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            if (ImageDragging)
                ImageDragShow(FALSE);
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            ScrollWindowEx(HWindow, 0, TopIndex - newTopIndex,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newTopIndex;
            PaintAllItems(hUpdateRgn, 0);
            HANDLES(DeleteObject(hUpdateRgn));
            if (ImageDragging)
                ImageDragShow(TRUE);
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
            if (scrollCode == SB_THUMBTRACK)
            {
                Parent->VisibleItemsArray.RefreshArr(Parent);         // do a hard refresh here
                Parent->VisibleItemsArraySurround.RefreshArr(Parent); // do a hard refresh here
            }
        }
        if (scrollCode != SB_THUMBTRACK)
            SetupScrollBars(UPDATE_VERT_SCROLL);
    }
}

LRESULT
CFilesBox::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CFilesBox::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HPrivateDC = NOHANDLES(GetDC(HWindow));
        break;
    }

    case WM_SIZE:
    {
        LayoutChilds();
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HANDLES(BeginPaint(HWindow, &ps));
        PaintAllItems(NULL, 0);
        HANDLES(EndPaint(HWindow, &ps));
        SelectClipRgn(HPrivateDC, NULL);
        return 0;
    }

    case WM_HELP:
    {
        if (MainWindow->HasLockedUI())
            break;
        if (MainWindow->HelpMode == HELP_INACTIVE &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0 &&
            (GetKeyState(VK_MENU) & 0x8000) == 0)
        {
            PostMessage(MainWindow->HWindow, WM_COMMAND, CM_HELP_CONTENTS, 0); // plain F1 (no modifiers)
            return TRUE;
        }
        break;
    }

    case WM_SYSCHAR:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnSysChar(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_CHAR:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnChar(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnSysKeyUp(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        if (MainWindow->HasLockedUI())
            break;
        BOOL firstPress = (lParam & 0x40000000) == 0;
        if (firstPress) // if SHIFT is held, autorepeat occurs, but we care only about the first press
            ResetMouseWheelAccumulator();
        LRESULT lResult;
        if (Parent->OnSysKeyDown(uMsg, wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_SETFOCUS:
    {
        ResetMouseWheelAccumulator();
        Parent->OnSetFocus();
        break;
    }

    case WM_KILLFOCUS:
    {
        ResetMouseWheelAccumulator();
        Parent->OnKillFocus((HWND)wParam);
        break;
    }

    case WM_NCPAINT:
    {
        HDC hdc = HANDLES(GetWindowDC(HWindow));
        if (wParam != 1)
        {
            // the region is in screen coordinates, offset it
            RECT wR;
            GetWindowRect(HWindow, &wR);
            OffsetRgn((HRGN)wParam, -wR.left, -wR.top);

            // clip the DC
            SelectClipRgn(hdc, (HRGN)wParam);
        }
        RECT r;
        GetClientRect(HWindow, &r);
        r.right += 2;
        r.bottom += 2;
        DrawEdge(hdc, &r, BDR_SUNKENOUTER, BF_RECT);
        if (Parent->StatusLine != NULL && Parent->StatusLine->HWindow != NULL)
        {
            r.left = 0;
            r.top = r.bottom - 1;
            r.right = 1;
            r.bottom = r.top + 1;
            FillRect(hdc, &r, HMenuGrayTextBrush);
        }
        HANDLES(ReleaseDC(HWindow, hdc));
        return 0;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_UPDATE)
        {
            Parent->AdjustQuickRenameWindow();
            return 0;
        }
        break;
    }

    // catch a click on the scroll bars and bring Salamander to front
    case WM_PARENTNOTIFY:
    {
        WORD fwEvent = LOWORD(wParam);
        if (fwEvent == WM_LBUTTONDOWN || fwEvent == WM_MBUTTONDOWN || fwEvent == WM_RBUTTONDOWN)
        {
            if (GetActiveWindow() == NULL)
                SetForegroundWindow(MainWindow->HWindow);
        }
        break;
    }

    case WM_VSCROLL:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        int scrollCode = (int)LOWORD(wParam);
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_TRACKPOS;
        GetScrollInfo(HVScrollBar, SB_CTL, &si);
        int pos = si.nTrackPos;
        OnVScroll(scrollCode, pos);
        return 0;
    }

    case WM_HSCROLL:
    {
        if (MainWindow->HasLockedUI())
            break;
        int scrollCode = (int)LOWORD(wParam);
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_TRACKPOS;
        GetScrollInfo(HHScrollBar, SB_CTL, &si);
        int pos = si.nTrackPos;
        OnHScroll(scrollCode, pos);
        return 0;
    }

    case WM_MOUSEACTIVATE:
    {
        if (MainWindow->HasLockedUI())
            break; // when the UI is locked we want a click on the panel to bring Salamander to front
        if (LOWORD(lParam) == HTCLIENT)
        {
            if (!IsIconic(MainWindow->HWindow) &&
                HIWORD(lParam) == WM_LBUTTONDOWN)
                return MA_NOACTIVATE;
            if (!IsIconic(MainWindow->HWindow) &&
                HIWORD(lParam) == WM_RBUTTONDOWN)
                return MA_NOACTIVATE;
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnLButtonDown(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_LBUTTONDBLCLK:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnLButtonDblclk(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_LBUTTONUP:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnLButtonUp(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_RBUTTONDOWN:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnRButtonDown(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_RBUTTONUP:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnRButtonUp(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnMouseMove(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_TIMER:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnTimer(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_SETCURSOR:
    {
        if (Parent->TrackingSingleClick)
            return TRUE;
        break;
    }

    case WM_CAPTURECHANGED:
    {
        LRESULT lResult;
        if (Parent->OnCaptureChanged(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_MOUSELEAVE:
    case WM_CANCELMODE:
    {
        LRESULT lResult;
        if (Parent->OnCancelMode(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_INITMENUPOPUP: // warning: similar code exists also in CMainWindow
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_MENUCHAR:
    {
        if (MainWindow->HasLockedUI())
            break;
        if (Parent->Is(ptPluginFS) && Parent->GetPluginFS()->NotEmpty() &&
            Parent->GetPluginFS()->IsServiceSupported(FS_SERVICE_CONTEXTMENU))
        {
            LRESULT lResult;
            if (Parent->GetPluginFS()->HandleMenuMsg(uMsg, wParam, lParam, &lResult))
                return lResult;
        }
        if (Parent->ContextMenu != NULL)
        {
            IContextMenu3* contextMenu3 = NULL;
            if (uMsg == WM_MENUCHAR)
            {
                if (SUCCEEDED(Parent->ContextMenu->QueryInterface(IID_IContextMenu3, (void**)&contextMenu3)))
                {
                    LRESULT lResult;
                    contextMenu3->HandleMenuMsg2(uMsg, wParam, lParam, &lResult);
                    contextMenu3->Release();
                    return lResult;
                }
            }
            Parent->ContextMenu->HandleMenuMsg(uMsg, wParam, lParam);
        }
        // to make the New submenu work we must also forward the message there
        if (Parent->ContextSubmenuNew != NULL && Parent->ContextSubmenuNew->MenuIsAssigned())
        {
            CALL_STACK_MESSAGE1("CFilesBox::WindowProc::SafeHandleMenuNewMsg2");
            LRESULT lResult;
            Parent->SafeHandleMenuNewMsg2(uMsg, wParam, lParam, &lResult);
            return lResult;
        }
        if (Parent->ContextMenu != NULL)
            return 0;
        break;
    }

    case WM_USER_MOUSEHWHEEL: // horizontal scrolling, available s Windows Vista
    {
        if (MainWindow->HasLockedUI())
            break;
        if (ViewMode == vmDetailed || ViewMode == vmBrief)
        {
            short zDelta = (short)HIWORD(wParam);
            if ((zDelta < 0 && MouseHWheelAccumulator > 0) || (zDelta > 0 && MouseHWheelAccumulator < 0))
                ResetMouseWheelAccumulator(); // when the tilt direction changes reset the accumulator

            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
            GetScrollInfo(HHScrollBar, SB_CTL, &si);

            DWORD wheelScroll = 1;
            if (ViewMode == vmDetailed)
            {
                wheelScroll = ItemHeight * GetMouseWheelScrollChars();
                wheelScroll = max(1, min(wheelScroll, si.nPage - 1)); // limit at most to page size
            }

            MouseHWheelAccumulator += 1000 * zDelta;
            int stepsPerChar = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int charsToScroll = MouseHWheelAccumulator / stepsPerChar;
            if (charsToScroll != 0)
            {
                MouseHWheelAccumulator -= charsToScroll * stepsPerChar;
                OnHScroll(SB_THUMBPOSITION, si.nPos + charsToScroll);
            }
        }
        return 0;
    }

    case WM_MOUSEHWHEEL:
    case WM_MOUSEWHEEL:
    {
        if (MainWindow->HasLockedUI())
            return 0;
        // 7.10.2009 - AS253_B1_IB34: Manison reported that horizontal scrolling did not work for him on Windows Vista.
        // It worked for me (through this approach). After installing IntelliPoint drivers v7 (previously I had none on Vista x64)
        // WM_MOUSEHWHEEL messages stopped passing through the hook and went directly to the focused window;
        // I disabled the hook and now we must capture messages in windows that can have focus
        // so they can be forwarded.
        // 30.11.2012 - on our forum a user reported WM_MOUSEHWHEEL not passing through the message hook (same as before
        // in Manison's case): https://forum.altap.cz/viewtopic.php?f=24&t=6039
        // therefore we will now also catch the message in individual windows where it can potentially appear (depending on focus)
        // and then route it so that it is delivered to the window under the cursor, as we always did

        // if the message arrived "recently" through the other channel, ignore this one
        if (MouseWheelMSGThroughHook && MouseWheelMSGTime != 0 && (GetTickCount() - MouseWheelMSGTime < MOUSEWHEELMSG_VALID))
            return 0;
        MouseWheelMSGThroughHook = FALSE;
        MouseWheelMSGTime = GetTickCount();

        MSG msg;
        DWORD pos = GetMessagePos();
        msg.pt.x = GET_X_LPARAM(pos);
        msg.pt.y = GET_Y_LPARAM(pos);
        msg.lParam = lParam;
        msg.wParam = wParam;
        msg.hwnd = HWindow;
        msg.message = uMsg;
        PostMouseWheelMessage(&msg);
        return 0;
    }

    case WM_USER_MOUSEWHEEL:
    {
        if (MainWindow->HasLockedUI())
            break;
        // proper wheel support see http://msdn.microsoft.com/en-us/library/ms997498.aspx "Best Practices for Supporting Microsoft Mouse and Keyboard Devices"

        if (Parent->DragBox) // prevent drawing errors
            return 0;

        Parent->KillQuickRenameTimer(); // avoid opening QuickRenameWindow

        short zDelta = (short)HIWORD(wParam);
        if ((zDelta < 0 && MouseWheelAccumulator > 0) || (zDelta > 0 && MouseWheelAccumulator < 0))
            ResetMouseWheelAccumulator(); // when the wheel direction changes reset the accumulator

        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        // standard scrolling without modifier keys
        if (!controlPressed && !altPressed && !shiftPressed)
        {
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;

            if (ViewMode == vmBrief)
            {
                GetScrollInfo(HHScrollBar, SB_CTL, &si);
                MouseWheelAccumulator += 1000 * zDelta;
                int stepsPerCol = max(1, (1000 * WHEEL_DELTA));
                int colsToScroll = MouseWheelAccumulator / stepsPerCol;
                if (colsToScroll != 0)
                {
                    MouseWheelAccumulator -= colsToScroll * stepsPerCol;
                    OnHScroll(SB_THUMBPOSITION, si.nPos - colsToScroll);
                }
            }
            else
            {
                GetScrollInfo(HVScrollBar, SB_CTL, &si);

                DWORD wheelScroll = max(1, ItemHeight);
                if (ViewMode == vmDetailed)
                {
                    wheelScroll = GetMouseWheelScrollLines();             // can be up to WHEEL_PAGESCROLL(0xffffffff)
                    wheelScroll = max(1, min(wheelScroll, si.nPage - 1)); // limit at most to page height
                }

                // wheelScrollLines under WinVista with IntelliMouse Explorer 4.0 and IntelliPoint drivers is 40 at maximum wheel speed
                // stepsPerLine would then be 120 / 31 = 3.870967..., after truncation 3, which is a large rounding error
                // therefore I multiply values by 1000 to push the error three orders further
                MouseWheelAccumulator += 1000 * zDelta;
                int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
                int linesToScroll = MouseWheelAccumulator / stepsPerLine;
                if (linesToScroll != 0)
                {
                    MouseWheelAccumulator -= linesToScroll * stepsPerLine;
                    OnVScroll(SB_THUMBPOSITION, si.nPos - linesToScroll);
                }
            }
        }

        // SHIFT: horizontal scrolling
        if (!controlPressed && !altPressed && shiftPressed &&
            (ViewMode == vmDetailed || ViewMode == vmBrief))
        {
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
            GetScrollInfo(HHScrollBar, SB_CTL, &si);

            DWORD wheelScroll = 1;
            if (ViewMode == vmDetailed)
            {
                wheelScroll = ItemHeight * GetMouseWheelScrollLines(); // 'delta' can be up to WHEEL_PAGESCROLL(0xffffffff)
                wheelScroll = max(1, min(wheelScroll, si.nPage));      // limit at most to page width
            }

            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int linesToScroll = MouseWheelAccumulator / stepsPerLine;
            if (linesToScroll != 0)
            {
                MouseWheelAccumulator -= linesToScroll * stepsPerLine;
                OnHScroll(SB_THUMBPOSITION, si.nPos - linesToScroll);
            }
        }

        // ALT: switch panel mode (details, brief, ...)
        if (!controlPressed && altPressed && !shiftPressed)
        {
            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerView = max(1, (1000 * WHEEL_DELTA));
            int viewsToScroll = MouseWheelAccumulator / stepsPerView;
            if (viewsToScroll != 0)
            {
                MouseWheelAccumulator -= viewsToScroll * stepsPerView;
                int newIndex = Parent->GetNextTemplateIndex(viewsToScroll < 0 ? TRUE : FALSE, FALSE);
                Parent->SelectViewTemplate(newIndex, TRUE, FALSE);
            }
        }

        // CTRL: zoom thumbnails
        if (controlPressed && !altPressed && !shiftPressed &&
            ViewMode == vmThumbnails)
        {
            DWORD wheelScroll = 15;
            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int linesToScroll = MouseWheelAccumulator / stepsPerLine;
            if (linesToScroll != 0)
            {
                MouseWheelAccumulator -= linesToScroll * stepsPerLine;

                int size = Configuration.ThumbnailSize;
                size += linesToScroll;
                size = min(THUMBNAIL_SIZE_MAX, max(THUMBNAIL_SIZE_MIN, size));
                if (size != Configuration.ThumbnailSize)
                {
                    Configuration.ThumbnailSize = size;
                    MainWindow->LeftPanel->SetThumbnailSize(size);
                    MainWindow->RightPanel->SetThumbnailSize(size);
                    if (MainWindow->LeftPanel->GetViewMode() == vmThumbnails)
                        MainWindow->LeftPanel->RefreshForConfig();
                    if (MainWindow->RightPanel->GetViewMode() == vmThumbnails)
                        MainWindow->RightPanel->RefreshForConfig();
                }
            }
        }

        if (Parent->TrackingSingleClick)
        {
            // ensure the hot item gets updated
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(HWindow, &p);
            SendMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
        }
        return 0;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

int CFilesBox::GetIndex(int x, int y, BOOL nearest, RECT* labelRect)
{
    CALL_STACK_MESSAGE4("CFilesBox::GetIndex(%d, %d, %d, )", x, y, nearest);

    if (labelRect != NULL && nearest)
        TRACE_E("CFilesBox::GetIndex nearest && labelRect");

    if (ItemsCount == 0)
        return INT_MAX;

    // convert x and y to FilesRect coordinates
    x -= FilesRect.left;
    y -= FilesRect.top;

    if (nearest)
    {
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        if (x >= FilesRect.right - FilesRect.left)
            x = FilesRect.right - FilesRect.left - 1;
        if (y >= FilesRect.bottom - FilesRect.top)
            y = FilesRect.bottom - FilesRect.top - 1;
    }
    else
    {
        if (x < 0 || x >= FilesRect.right - FilesRect.left ||
            y < 0 || y >= FilesRect.bottom - FilesRect.top)
            return INT_MAX;
    }

    RECT labelR; // label rect
    labelR.left = 0;
    labelR.top = 0;
    labelR.right = 50;
    labelR.bottom = 50;

    int itemIndex = INT_MAX;
    switch (ViewMode)
    {
    case vmBrief:
    {
        int itemY = y / ItemHeight;
        if (itemY <= EntireItemsInColumn - 1)
            itemIndex = TopIndex + (x / ItemWidth) * EntireItemsInColumn + itemY;
        else if (nearest)
            itemIndex = TopIndex + (x / ItemWidth) * EntireItemsInColumn + EntireItemsInColumn - 1;

        if (!nearest && Configuration.FullRowSelect)
        {
            int xPos = x % ItemWidth;
            if (xPos >= ItemWidth - 10) // adjustment - we must leave space for cage dragging
                itemIndex = INT_MAX;
        }

        labelR.top = itemY * ItemHeight;
        labelR.bottom = labelR.top + ItemHeight;
        labelR.left = (x / ItemWidth) * ItemWidth + IconSizes[ICONSIZE_16] + 2 - XOffset;
        labelR.right = labelR.left + ItemWidth - 10 - IconSizes[ICONSIZE_16] - 2;
        break;
    }

    case vmDetailed:
    {
        int itemY = y / ItemHeight;
        if (x + XOffset < ItemWidth || nearest)
            itemIndex = itemY + TopIndex;

        labelR.top = itemY * ItemHeight;
        labelR.bottom = labelR.top + ItemHeight;
        labelR.left = IconSizes[ICONSIZE_16] + 2 - XOffset;
        labelR.right = labelR.left + ItemWidth - IconSizes[ICONSIZE_16] - 2;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        int itemY = (y + TopIndex) / ItemHeight;
        int itemX = x / ItemWidth;
        if (itemX < ColumnsCount)
            itemIndex = itemY * ColumnsCount + itemX;
        else if (nearest)
            itemIndex = itemY * ColumnsCount + ColumnsCount - 1;
        break;
    }
    }

    if (itemIndex < 0 || itemIndex >= ItemsCount)
        itemIndex = INT_MAX;
    if (nearest && itemIndex >= ItemsCount)
        itemIndex = ItemsCount - 1;

    if (itemIndex != INT_MAX)
    {
        // (vmBrief+vmDetailed) if FullRowSelect is off, measure the real item length
        if ((ViewMode == vmBrief || ViewMode == vmDetailed) &&
            !nearest && !Configuration.FullRowSelect)
        {
            if (ViewMode == vmDetailed)
                x += XOffset;
            char formatedFileName[MAX_PATH];
            CFileData* f;
            BOOL isDir = itemIndex < Parent->Dirs->Count;
            if (isDir)
                f = &Parent->Dirs->At(itemIndex);
            else
                f = &Parent->Files->At(itemIndex - Parent->Dirs->Count);

            AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0, isDir);

            const char* s = formatedFileName;

            int width = IconSizes[ICONSIZE_16] + 2;

            SIZE sz;
            int len;
            if ((!isDir || Configuration.SortDirsByExt) && ViewMode == vmDetailed &&
                Parent->IsExtensionInSeparateColumn() && f->Ext[0] != 0 && f->Ext > f->Name + 1) // exception for names like ".htaccess" that display in the Name column although they are extensions
            {
                len = (int)(f->Ext - f->Name - 1);
            }
            else
            {
                if (*s == '.' && *(s + 1) == '.' && *(s + 2) == 0)
                {
                    if (ViewMode == vmBrief)
                        width = ItemWidth - 10; // 10 - so that we are not stretched across the entire width
                    else
                        width = Parent->Columns[0].Width - 1;
                    goto SKIP_MES;
                }
                else
                    len = f->NameLen;
            }

            // find the real length of the text
            HDC dc;
            HFONT hOldFont;
            dc = HANDLES(GetDC(HWindow));
            hOldFont = (HFONT)SelectObject(dc, Font);

            GetTextExtentPoint32(dc, s, len, &sz);
            width += 2 + sz.cx + 3;

            if (ViewMode == vmDetailed && width > (int)Parent->Columns[0].Width)
                width = Parent->Columns[0].Width;

            SelectObject(dc, hOldFont);
            HANDLES(ReleaseDC(HWindow, dc));
        SKIP_MES:

            labelR.right = labelR.left + width - IconSizes[ICONSIZE_16] - 2;

            int xPos = x;
            if (ViewMode == vmBrief)
                xPos %= ItemWidth;
            if (xPos >= width)
                itemIndex = INT_MAX;
        }

        if ((ViewMode == vmIcons || ViewMode == vmThumbnails) && !nearest)
        {
            // store relative coordinates in xPos and yPos
            POINT pt;
            RECT rect;
            pt.x = x % ItemWidth;
            pt.y = (y + TopIndex) % ItemHeight;

            rect.top = 2;
            int iconW = 32;
            int iconH = 32;

            // detect a click on the icon
            if (ViewMode == vmThumbnails)
            {
                rect.top = 3;
                iconW = ThumbnailWidth + 2;
                iconH = ThumbnailHeight + 2;
            }
            rect.left = (ItemWidth - iconW) / 2;
            rect.right = rect.left + iconW;
            rect.bottom = rect.top + iconH;
            BOOL hitIcon = PtInRect(&rect, pt);

            // detect a click on the text below the icon
            char formatedFileName[MAX_PATH];
            CFileData* f;
            BOOL isItemUpDir = FALSE;
            if (itemIndex < Parent->Dirs->Count)
            {
                f = &Parent->Dirs->At(itemIndex);
                if (itemIndex == 0 && *f->Name == '.' && *(f->Name + 1) == '.' && *(f->Name + 2) == 0) // "up-dir" can appear only as the first item
                    isItemUpDir = TRUE;
            }
            else
                f = &Parent->Files->At(itemIndex - Parent->Dirs->Count);

            AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0,
                          itemIndex < Parent->Dirs->Count);

            // NOTE: keep in sync with CFilesWindow::SetQuickSearchCaretPos
            char buff[1024];                  // destination buffer for strings
            int maxWidth = ItemWidth - 4 - 1; // -1 so they don't touch
            char* out1 = buff;
            int out1Len = 512;
            int out1Width;
            char* out2 = buff + 512;
            int out2Len = 512;
            int out2Width;
            HDC hDC = ItemBitmap.HMemDC;
            HFONT hOldFont = (HFONT)SelectObject(hDC, Font);
            SplitText(hDC, formatedFileName, f->NameLen, &maxWidth,
                      out1, &out1Len, &out1Width,
                      out2, &out2Len, &out2Width);
            SelectObject(hDC, hOldFont);
            maxWidth += 4;

            if (isItemUpDir) // updir is just "..", we must extend it to the displayed size
            {
                // see CFilesWindow::DrawIconThumbnailItem
                maxWidth = max(maxWidth, (Parent->GetViewMode() == vmThumbnails ? ThumbnailWidth : 32) + 4);
            }

            rect.left = (ItemWidth - maxWidth) / 2;
            rect.right = rect.left + maxWidth;
            rect.top = rect.bottom;
            rect.bottom = rect.top + 3 + 2 + FontCharHeight + 2;
            if (out2Len > 0)
                rect.bottom += FontCharHeight;
            BOOL hitText = PtInRect(&rect, pt);

            labelR = rect;
            int itemY = (y + TopIndex) / ItemHeight;
            int itemX = x / ItemWidth;
            labelR.top += 2 + itemY * ItemHeight - TopIndex;
            labelR.bottom += itemY * ItemHeight - TopIndex - 1;
            labelR.left += itemX * ItemWidth;
            labelR.right += itemX * ItemWidth;

            // if neither the icon nor the text was clicked, ignore the item
            if (!hitIcon && !hitText)
                itemIndex = INT_MAX;
        }

        if ((ViewMode == vmTiles) && !nearest)
        {
            // store relative coordinates in xPos and yPos
            POINT pt;
            RECT rect;
            pt.x = x % ItemWidth;
            pt.y = (y + TopIndex) % ItemHeight;

            int iconW = IconSizes[ICONSIZE_48];
            int iconH = IconSizes[ICONSIZE_48];

            // detect a click on the icon
            rect.top = (ItemHeight - iconH) / 2;
            rect.left = TILE_LEFT_MARGIN;
            rect.right = rect.left + iconW;
            rect.bottom = rect.top + iconH;
            BOOL hitIcon = PtInRect(&rect, pt);

            CFileData* f;
            int isDir = 0;
            if (itemIndex < Parent->Dirs->Count)
            {
                f = &Parent->Dirs->At(itemIndex);
                isDir = itemIndex != 0 || strcmp(f->Name, "..") != 0 ? 1 : 2 /* UP-DIR */;
            }
            else
                f = &Parent->Files->At(itemIndex - Parent->Dirs->Count);

            int itemWidth = rect.right - rect.left; // item width
            int maxTextWidth = ItemWidth - TILE_LEFT_MARGIN - IconSizes[ICONSIZE_48] - TILE_LEFT_MARGIN - 4;
            int widthNeeded = 0;

            char buff[3 * 512]; // destination buffer for strings
            char* out0 = buff;
            int out0Len;
            char* out1 = buff + 512;
            int out1Len;
            char* out2 = buff + 1024;
            int out2Len;
            HDC hDC = ItemBitmap.HMemDC;
            HFONT hOldFont = (HFONT)SelectObject(hDC, Font);
            GetTileTexts(f, isDir, hDC, maxTextWidth, &widthNeeded,
                         out0, &out0Len, out1, &out1Len, out2, &out2Len,
                         Parent->ValidFileData, &Parent->PluginData,
                         Parent->Is(ptDisk));
            SelectObject(hDC, hOldFont);
            widthNeeded += 5;

            int visibleLines = 1; // the name is always visible
            if (out1[0] != 0)
                visibleLines++;
            if (out2[0] != 0)
                visibleLines++;
            int textH = visibleLines * FontCharHeight + 4;

            // rectangle of text
            labelR.left = rect.right + 2;
            labelR.right = labelR.left + widthNeeded;
            labelR.top = (ItemHeight - textH) / 2; // center it
            labelR.bottom = labelR.top + textH;

            BOOL hitText = PtInRect(&labelR, pt);

            // move labelR to the actual position
            int itemY = (y + TopIndex) / ItemHeight;
            int itemX = x / ItemWidth;
            labelR.top += itemY * ItemHeight - TopIndex;
            labelR.bottom += itemY * ItemHeight - TopIndex;
            labelR.left += itemX * ItemWidth;
            labelR.right += itemX * ItemWidth;

            // if neither the icon nor the text was clicked, ignore the item
            if (!hitIcon && !hitText)
                itemIndex = INT_MAX;
        }
    }
    if (labelRect != NULL)
    {
        labelRect->left = labelR.left + FilesRect.left;
        labelRect->top = labelR.top + FilesRect.top;
        labelRect->right = labelR.right + FilesRect.left;
        labelRect->bottom = labelR.bottom + FilesRect.top;
    }
    return itemIndex;
}

BOOL CFilesBox::ShowHideChilds()
{
    BOOL change = FALSE;
    if (HeaderLineVisible && ViewMode != vmDetailed)
    {
        TRACE_E("Header line is supported only in Detailed mode");
        HeaderLineVisible = FALSE;
    }

    // the horizontal scrollbar is allowed in detailed and brief modes; hide it otherwise
    if (ViewMode != vmDetailed && ViewMode != vmBrief && HHScrollBar != NULL)
    {
        DestroyWindow(BottomBar.HWindow);
        BottomBar.HScrollBar = NULL;
        HHScrollBar = NULL;
        change = TRUE;
    }

    // hide the vertical scrollbar in brief mode
    if (ViewMode == vmBrief && HVScrollBar != NULL)
    {
        DestroyWindow(HVScrollBar);
        HVScrollBar = NULL;
        change = TRUE;
    }

    // the header line is allowed only in detailed mode (and only if the user wants it);
    // hide it otherwise
    if ((ViewMode != vmDetailed || !HeaderLineVisible) &&
        HeaderLine.HWindow != NULL)
    {
        DestroyWindow(HeaderLine.HWindow);
        HeaderLine.HWindow = NULL;
        change = TRUE;
    }

    // if we are in detailed or brief mode we need the horizontal scrollbar
    if ((ViewMode == vmDetailed || ViewMode == vmBrief) && HHScrollBar == NULL)
    {
        BottomBar.Create(CWINDOW_CLASSNAME2, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                         0, 0, 0, 0,
                         HWindow,
                         NULL, //HMenu
                         HInstance,
                         &BottomBar);
        HHScrollBar = CreateWindow("scrollbar", "", WS_CHILD | SBS_HORZ | WS_VISIBLE | WS_CLIPSIBLINGS | SBS_HORZ,
                                   0, 0, 0, 0,
                                   BottomBar.HWindow,
                                   NULL, //HMenu
                                   HInstance,
                                   NULL);
        BottomBar.HScrollBar = HHScrollBar;
        change = TRUE;
    }

    // in detailed mode leave a gap on the right side of the horizontal scrollbar under the vertical scrollbar
    BottomBar.VertScrollSpace = (ViewMode == vmDetailed);

    // the vertical scrollbar is needed in all modes except brief
    if (ViewMode != vmBrief)
    {
        if (HVScrollBar == NULL)
        {
            HVScrollBar = CreateWindow("scrollbar", "", WS_CHILD | SBS_VERT | WS_VISIBLE | WS_CLIPSIBLINGS | SBS_VERT,
                                       0, 0, 0, 0,
                                       HWindow,
                                       NULL, //HMenu
                                       HInstance,
                                       NULL);
            change = TRUE;
        }
    }

    // in detailed mode, if a header line is requested, create it
    if (ViewMode == vmDetailed && HeaderLineVisible && HeaderLine.HWindow == NULL)
    {
        HeaderLine.Create(CWINDOW_CLASSNAME2, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                          0, 0, 0, 0,
                          HWindow,
                          NULL, //HMenu
                          HInstance,
                          &HeaderLine);
        change = TRUE;
    }

    return change;
}

void CFilesBox::SetupScrollBars(DWORD flags)
{
    SCROLLINFO si;
    if (HHScrollBar != NULL && flags & UPDATE_HORZ_SCROLL)
    {
        si.cbSize = sizeof(si);
        si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE | SIF_PAGE;
        if (ViewMode == vmDetailed)
            si.nPos = XOffset;
        else
            si.nPos = TopIndex / EntireItemsInColumn;
        si.nMin = 0;
        if (ViewMode == vmDetailed)
        {
            // Detailed
            si.nMax = ItemWidth;
            si.nPage = FilesRect.right - FilesRect.left + 1;
        }
        else
        {
            // Brief
            si.nMax = ColumnsCount;            // total number of columns
            si.nPage = EntireColumnsCount + 1; // number of fully visible columns
        }
        if (OldHorzSI.cbSize == 0 || OldHorzSI.nPos != si.nPos || OldHorzSI.nMax != si.nMax ||
            OldHorzSI.nPage != si.nPage)
        {
            // hide the drag image if necessary
            BOOL showImage = FALSE;
            if (ImageDragging)
            {
                RECT r;
                GetWindowRect(HHScrollBar, &r);
                if (ImageDragInterfereRect(&r))
                {
                    ImageDragShow(FALSE);
                    showImage = TRUE;
                }
            }
            // set the scroll bars
            OldHorzSI = si;
            SetScrollInfo(HHScrollBar, SB_CTL, &si, TRUE);
            // restore the drag image
            if (showImage)
                ImageDragShow(TRUE);
        }
    }

    if (HVScrollBar != NULL && flags & UPDATE_VERT_SCROLL)
    {
        si.cbSize = sizeof(si);
        si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE | SIF_PAGE;
        si.nPos = TopIndex;
        si.nMin = 0;

        if (ViewMode == vmDetailed)
        {
            // Detailed
            si.nMax = ItemsCount;
            si.nPage = EntireItemsInColumn + 1;
        }
        else
        {
            // Icons || Thumbnails
            si.nMax = ItemsInColumn * ItemHeight;
            si.nPage = FilesRect.bottom - FilesRect.top + 1;
        }

        if (OldVertSI.cbSize == 0 || OldVertSI.nPos != si.nPos || OldVertSI.nMax != si.nMax ||
            OldVertSI.nPage != si.nPage)
        {
            // hide the drag image if necessary
            BOOL showImage = FALSE;
            if (ImageDragging)
            {
                RECT r;
                GetWindowRect(HVScrollBar, &r);
                if (ImageDragInterfereRect(&r))
                {
                    ImageDragShow(FALSE);
                    showImage = TRUE;
                }
            }
            // set the scroll bars
            OldVertSI = si;
            SetScrollInfo(HVScrollBar, SB_CTL, &si, TRUE);
            // restore the drag image
            if (showImage)
                ImageDragShow(TRUE);
        }
    }
}

void CFilesBox::CheckAndCorrectBoundaries()
{
    switch (ViewMode)
    {
    case vmDetailed:
    {
        if (FilesRect.right - FilesRect.left > 0 && FilesRect.bottom - FilesRect.top > 0)
        {
            // ensure scrolling when the window grows and we are almost at the right or bottom edge but can still scroll
            int newXOffset = XOffset;
            if (newXOffset > 0 && ItemWidth - newXOffset < (FilesRect.right - FilesRect.left) + 1)
                newXOffset = ItemWidth - (FilesRect.right - FilesRect.left);
            if (ItemWidth < (FilesRect.right - FilesRect.left) + 1)
                newXOffset = 0;

            int newTopIndex = TopIndex;
            if (newTopIndex > 0 && ItemsCount - newTopIndex < EntireItemsInColumn + 1)
                newTopIndex = ItemsCount - EntireItemsInColumn;
            if (ItemsCount < EntireItemsInColumn + 1)
                newTopIndex = 0;

            if (newXOffset != XOffset || newTopIndex != TopIndex)
            {
                if (ImageDragging)
                    ImageDragShow(FALSE);
                HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
                ScrollWindowEx(HWindow, XOffset - newXOffset, ItemHeight * (TopIndex - newTopIndex),
                               &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
                if (HeaderLine.HWindow != NULL && newXOffset != XOffset)
                {
                    HRGN hUpdateRgn2 = HANDLES(CreateRectRgn(0, 0, 0, 0));
                    ScrollWindowEx(HeaderLine.HWindow, XOffset - newXOffset, 0,
                                   NULL, NULL, hUpdateRgn2, NULL, 0);
                    XOffset = newXOffset;
                    HDC hDC = HANDLES(GetDC(HeaderLine.HWindow));
                    HeaderLine.PaintAllItems(hDC, hUpdateRgn2);
                    HANDLES(ReleaseDC(HeaderLine.HWindow, hDC));
                    HANDLES(DeleteObject(hUpdateRgn2));
                }
                else
                    XOffset = newXOffset;
                TopIndex = newTopIndex;
                SetupScrollBars(UPDATE_HORZ_SCROLL | UPDATE_VERT_SCROLL);
                PaintAllItems(hUpdateRgn, 0);
                HANDLES(DeleteObject(hUpdateRgn));
                if (ImageDragging)
                    ImageDragShow(TRUE);
                Parent->VisibleItemsArray.InvalidateArr();
                Parent->VisibleItemsArraySurround.InvalidateArr();
            }
        }
        break;
    }

    case vmBrief:
    {
        int col = ItemsCount / EntireItemsInColumn - TopIndex / EntireItemsInColumn;
        if (col * ItemWidth < FilesRect.right - FilesRect.left - ItemWidth)
        {
            int newTopIndex = (ColumnsCount - EntireColumnsCount) * EntireItemsInColumn;
            if (newTopIndex != TopIndex)
            {
                TopIndex = newTopIndex;
                SetupScrollBars(UPDATE_HORZ_SCROLL);
                InvalidateRect(HWindow, &FilesRect, FALSE);
                Parent->VisibleItemsArray.InvalidateArr();
                Parent->VisibleItemsArraySurround.InvalidateArr();
            }
        }
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        if (FilesRect.right - FilesRect.left > 0 && FilesRect.bottom - FilesRect.top > 0)
        {
            // ensure scrolling when the window grows and we are almost at the right or bottom edge but can still scroll
            int newTopIndex = TopIndex;
            if (newTopIndex > 0 && ItemsInColumn * ItemHeight - newTopIndex < FilesRect.bottom - FilesRect.top)
                newTopIndex = ItemsInColumn * ItemHeight - (FilesRect.bottom - FilesRect.top);
            if (newTopIndex < 0)
                newTopIndex = 0;

            if (newTopIndex != TopIndex)
            {
                if (ImageDragging)
                    ImageDragShow(FALSE);
                HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
                ScrollWindowEx(HWindow, 0, TopIndex - newTopIndex,
                               &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
                TopIndex = newTopIndex;
                SetupScrollBars(UPDATE_HORZ_SCROLL | UPDATE_VERT_SCROLL);
                PaintAllItems(hUpdateRgn, 0);
                HANDLES(DeleteObject(hUpdateRgn));
                if (ImageDragging)
                    ImageDragShow(TRUE);
                Parent->VisibleItemsArray.InvalidateArr();
                Parent->VisibleItemsArraySurround.InvalidateArr();
            }
        }
        break;
    }
    }
}

void CFilesBox::LayoutChilds(BOOL updateAndCheck)
{
    GetClientRect(HWindow, &ClientRect);
    ItemBitmap.Enlarge(ClientRect.right - ClientRect.left, ItemHeight);
    Parent->CancelUI(); // cancel QuickSearch and QuickEdit
    FilesRect = ClientRect;

    if (ClientRect.right > 0 && ClientRect.bottom > 0)
    {
        int deferCount = 0;

        if (HHScrollBar != NULL)
        {
            int scrollH = GetSystemMetrics(SM_CYHSCROLL);

            // place the horizontal scrollbar
            BottomBarRect.left = 0;
            BottomBarRect.top = FilesRect.bottom - scrollH;
            BottomBarRect.right = FilesRect.right;
            BottomBarRect.bottom = FilesRect.bottom;
            deferCount++;
            FilesRect.bottom -= scrollH;
        }
        else
            ZeroMemory(&BottomBarRect, sizeof(BottomBarRect));

        // place the vertical scrollbar
        if (HVScrollBar != NULL)
        {
            int scrollW = GetSystemMetrics(SM_CXVSCROLL);
            VScrollRect.left = FilesRect.right - scrollW;
            VScrollRect.top = 0;
            VScrollRect.right = FilesRect.right;
            VScrollRect.bottom = FilesRect.bottom;
            deferCount++;
            FilesRect.right -= scrollW;
        }
        else
            ZeroMemory(&VScrollRect, sizeof(VScrollRect));

        if (HeaderLine.HWindow != NULL)
        {
            HeaderRect = FilesRect;
            HeaderRect.bottom = HeaderRect.top + FontCharHeight + 4;
            FilesRect.top = HeaderRect.bottom;
            deferCount++;
        }

        RECT oldBR;
        if (HHScrollBar != NULL)
            GetWindowRect(BottomBar.HWindow, &oldBR);
        if (deferCount > 0)
        {
            HDWP hDWP = HANDLES(BeginDeferWindowPos(deferCount));
            if (HeaderLine.HWindow != NULL)
            {
                HeaderLine.Width = HeaderRect.right - HeaderRect.left;
                HeaderLine.Height = HeaderRect.bottom - HeaderRect.top;
                hDWP = HANDLES(DeferWindowPos(hDWP, HeaderLine.HWindow, NULL,
                                              HeaderRect.left, HeaderRect.top,
                                              HeaderRect.right - HeaderRect.left, HeaderRect.bottom - HeaderRect.top,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
            }
            if (HVScrollBar != NULL)
                hDWP = HANDLES(DeferWindowPos(hDWP, HVScrollBar, NULL,
                                              VScrollRect.left, VScrollRect.top,
                                              VScrollRect.right - VScrollRect.left, VScrollRect.bottom - VScrollRect.top,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
            if (HHScrollBar != NULL)
                hDWP = HANDLES(DeferWindowPos(hDWP, BottomBar.HWindow, NULL,
                                              BottomBarRect.left, BottomBarRect.top,
                                              BottomBarRect.right - BottomBarRect.left, BottomBarRect.bottom - BottomBarRect.top,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
            HANDLES(EndDeferWindowPos(hDWP));
        }

        if (HHScrollBar != NULL)
        {
            RECT newBR;
            GetWindowRect(BottomBar.HWindow, &newBR);
            if (oldBR.left == newBR.left && oldBR.top == newBR.top &&
                oldBR.right == newBR.right && oldBR.bottom == newBR.bottom)
                BottomBar.LayoutChilds(); // call Layout explicitly if the size did not change
        }

        if (updateAndCheck)
        {
            int oldEntireItemsInColumn = EntireItemsInColumn;
            int oldEntireColumnsCount = EntireColumnsCount;

            if (ViewMode == vmDetailed) // recompute Name column width in smart-mode detail view
            {
                BOOL leftPanel = (Parent == MainWindow->LeftPanel);
                CColumn* nameCol = &Parent->Columns[0];
                if (nameCol->FixedWidth == 0 &&
                    (leftPanel && Parent->ViewTemplate->LeftSmartMode ||
                     !leftPanel && Parent->ViewTemplate->RightSmartMode))
                {
                    DWORD newNameWidth = Parent->GetResidualColumnWidth(Parent->FullWidthOfNameCol);
                    DWORD minWidth = Parent->WidthOfMostOfNames;
                    if (minWidth > (DWORD)(0.75 * (FilesRect.right - FilesRect.left)))
                        minWidth = (DWORD)(0.75 * (FilesRect.right - FilesRect.left));
                    if (minWidth < nameCol->MinWidth)
                        minWidth = nameCol->MinWidth;
                    if (newNameWidth < minWidth)
                        newNameWidth = minWidth;
                    if (newNameWidth != nameCol->Width) // Name column width changed
                    {
                        int delta = newNameWidth - nameCol->Width;
                        nameCol->Width = newNameWidth;
                        Parent->NarrowedNameColumn = nameCol->Width < Parent->FullWidthOfNameCol;
                        SetItemWidthHeight(ItemWidth + delta, ItemHeight);

                        // perform a complete redraw of header line and files box
                        // NOTE: this could be optimized (shift columns with ScrollWindowEx as when
                        //        changing column width from the header line), but this is fast and flicker-free enough
                        InvalidateRect(HeaderLine.HWindow, NULL, FALSE);
                        InvalidateRect(HWindow, &FilesRect, FALSE);
                    }
                }
            }

            UpdateInternalData();
            SetupScrollBars();

            switch (ViewMode)
            {
            case vmBrief:
            {
                if (oldEntireItemsInColumn != EntireItemsInColumn)
                {
                    TopIndex = (TopIndex / EntireItemsInColumn) * EntireItemsInColumn;
                    InvalidateRect(HWindow, &FilesRect, FALSE);
                }
                break;
            }

            case vmIcons:
            case vmThumbnails:
            case vmTiles:
            {
                if (oldEntireColumnsCount != EntireColumnsCount)
                {
                    //            TopIndex = (TopIndex / EntireItemsInColumn) * EntireItemsInColumn;
                    InvalidateRect(HWindow, &FilesRect, FALSE);
                }
                break;
            }
            }
            CheckAndCorrectBoundaries();
        }
        if (ItemsCount == 0)
            InvalidateRect(HWindow, &FilesRect, FALSE); // ensure the "empty panel" text is drawn
    }
    Parent->VisibleItemsArray.InvalidateArr();
    Parent->VisibleItemsArraySurround.InvalidateArr();
}

void CFilesBox::PaintHeaderLine()
{
    if (HeaderLine.HWindow != NULL)
    {
        HDC hDC = HANDLES(GetDC(HeaderLine.HWindow));
        HeaderLine.PaintAllItems(hDC, NULL);
        HANDLES(ReleaseDC(HeaderLine.HWindow, hDC));
    }
}
