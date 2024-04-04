// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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

//****************************************************************************
//
// CBottomBar
//

CBottomBar::CBottomBar()
    : CWindow(ooStatic)
{
    HScrollBar = NULL;
    VertScrollSpace = FALSE;
    RelayWindow = NULL;
}

CBottomBar::~CBottomBar()
{
    if (HWindow != NULL)
        DetachWindow();
}

void CBottomBar::LayoutChilds()
{
    if (HScrollBar != NULL)
    {
        RECT cR;
        GetClientRect(HWindow, &cR);

        if (VertScrollSpace)
            cR.right -= GetSystemMetrics(SM_CXVSCROLL);
        SetWindowPos(HScrollBar, NULL, 0, 0, cR.right, cR.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

LRESULT
CBottomBar::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        /*      case WM_ERASEBKGND:
    {
      if (VertScrollSpace)
      {
        RECT r;
        GetClientRect(HWindow, &r);
        r.left = r.right - GetSystemMetrics(SM_CXVSCROLL);
        FillRect((HDC)wParam, &r, DialogBrush);
      }
      return TRUE;
    }*/
    case WM_ERASEBKGND:
    {
        return TRUE;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
        RECT r;
        GetClientRect(HWindow, &r);
        r.left = r.right - GetSystemMetrics(SM_CXVSCROLL);
        FillRect((HDC)hDC, &r, HDialogBrush);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_SIZE:
    {
        LayoutChilds();
        return 0;
    }

    case WM_USER_MOUSEWHEEL:
    case WM_USER_MOUSEHWHEEL:
    {
        // and the wheel also scrolls above the horizontal scrollbar
        return SendMessage(GetParent(HWindow), uMsg, wParam, lParam);
    }

    case WM_HSCROLL:
    {
        if (RelayWindow != NULL)
            return RelayWindow->WindowProc(uMsg, wParam, lParam);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CHeaderLine
//

CHeaderLine::CHeaderLine()
    : CWindow(ooStatic)
{
    Parent = NULL;
    Columns = NULL;
    HotIndex = -1; // no columns are lit
    HotExt = FALSE;
    DownIndex = -1; // no columns are compressed
    DownVisible = FALSE;
    DragIndex = -1; // no column changes width
    MouseIsTracked = FALSE;
    OldDragColWidth = -1;
}

CHeaderLine::~CHeaderLine()
{
    if (HWindow != NULL)
        DetachWindow();
}

void CHeaderLine::PaintAllItems(HDC hDC, HRGN hUpdateRgn)
{
    if (hUpdateRgn != NULL)
        SelectClipRgn(hDC, hUpdateRgn);

    int left = Parent->XOffset;
    int right = Parent->XOffset;

    // This optimization was causing a problem during the FTP client connect;
    // the displayed welcome window was redrawn and that was
    // clipped window; however, the window had CS_SAVEBITS, so after its
    // Columns displayed were nonsensical.
    /*    RECT clipRect;
  int clipRectType = GetClipBox(hDC, &clipRect);
  if (clipRectType == SIMPLEREGION || clipRectType == COMPLEXREGION)
  {
    left += clipRect.left;
    right += clipRect.right;
  }
  else*/
    right += Width;
    //  TRACE_I("PaintAllItems left:"<<left<<" right"<<right);

    int x = 0;
    int width = 0;
    int columnsCount = Columns->Count;
    int i;
    for (i = 0; i <= columnsCount; i++)
    {
        if (i < columnsCount)
            width = Columns->At(i).Width;
        else
            width = 60000; // the right side will definitely be around the corner and drawing will occur
        if (!((x < left && x + width <= left) || (x >= right && x + width > right)))
            PaintItem(hDC, i, x);
        x += width; // we are not interested in the last addition - we are leaving the loop anyway
    }

    if (hUpdateRgn != NULL)
        SelectClipRgn(hDC, NULL); // we kick out the clip region if we have set it
    ValidateRect(HWindow, NULL);
}

#define SORT_BITMAP_W 8 // width of the bitmap for sorting
#define SORT_BITMAP_H 8 // height of the bitmap for sorting

void CHeaderLine::PaintItem(HDC hDC, int index, int x)
{
    if (index > Columns->Count)
    {
        TRACE_E("CHeaderLine::PaintItem() index=" << index << " out of range! count=" << Columns->Count);
        return;
    }

    if (x == -1)
    {
        x = 0;
        int i;
        for (i = 0; i < index; i++)
            x += Columns->At(i).Width;
    }
    x -= Parent->XOffset;

    BOOL first = index == 0;
    BOOL last = index == Columns->Count;

    //  if (!last)
    //    TRACE_I("PaintItem index:"<<index<<"   x:"<<x<<"   width:"<<Columns->At(index).Width);

    RECT r;
    r.top = 0;
    r.left = 0;
    r.bottom = Height;
    if (!last)
        r.right = Columns->At(index).Width;
    else
        r.right = Width;

    // grease the base
    RECT r2;
    r2 = r;
    r2.top++;
    r2.bottom--;
    FillRect(ItemBitmap.HMemDC, &r2, HDialogBrush);

    if (!last)
    {
        CColumn* column = &Columns->At(index);
        BOOL hot1 = HotIndex == index && (index > 0 || !HotExt);
        BOOL hot2 = HotIndex == index && index == 0 && HotExt;

        // column name
        HFONT hOldFont = (HFONT)SelectObject(ItemBitmap.HMemDC, hot1 && Configuration.SingleClick ? FontUL : Font);
        int oldMode = SetBkMode(ItemBitmap.HMemDC, TRANSPARENT);
        COLORREF oldColor = SetTextColor(ItemBitmap.HMemDC, hot1 ? GetCOLORREF(CurrentColors[HOT_PANEL]) : GetSysColor(COLOR_BTNTEXT));
        int nameLen = lstrlen(column->Name);
        TextOut(ItemBitmap.HMemDC, r.left + 3, (r.bottom - FontCharHeight) / 2, column->Name, nameLen);

        SIZE sz;
        sz.cx = 0;
        HDC hMemDC = NULL;
        BOOL sort = FALSE;
        CFilesWindow* panel = Parent->Parent;
        if (column->ID == COLUMN_ID_NAME && panel->SortType == stName)
            sort = TRUE;
        else if (column->ID == COLUMN_ID_SIZE && panel->SortType == stSize)
            sort = TRUE;
        else if ((column->ID == COLUMN_ID_DATE || column->ID == COLUMN_ID_TIME) && panel->SortType == stTime)
            sort = TRUE;
        else if (column->ID == COLUMN_ID_EXTENSION && panel->SortType == stExtension)
            sort = TRUE;
        else if (column->ID == COLUMN_ID_ATTRIBUTES && panel->SortType == stAttr)
            sort = TRUE;

        // arrow pointing in the direction of shifting
        GetTextExtentPoint32(ItemBitmap.HMemDC, column->Name, nameLen, &sz);
        hMemDC = HANDLES(CreateCompatibleDC(hDC));
        SelectObject(hMemDC, HHeaderSort);
        if (sort)
            BitBlt(ItemBitmap.HMemDC,
                   r.left + 3 + sz.cx + SORT_BITMAP_W,
                   (r.bottom - SORT_BITMAP_H) / 2,
                   SORT_BITMAP_W, SORT_BITMAP_H,
                   hMemDC, panel->ReverseSort ? SORT_BITMAP_W : 0, 0, SRCCOPY);

        // "Ext" ve sloupci "Name"
        if (index == 0 && !Parent->Parent->IsExtensionInSeparateColumn() &&
            (Parent->Parent->ValidFileData & VALID_DATA_EXTENSION)) //p.s.: I've uncommented the test for the extension so that "Ext" can be hidden from the "Name" column (when highlighting "Ext" this condition is already met)
        {
            int textLeft = r.left + 3 + sz.cx + 3 * SORT_BITMAP_W;
            if (sz.cx == 0) // if we haven't measured yet, we'll do it now
                GetTextExtentPoint32(ItemBitmap.HMemDC, column->Name, nameLen, &sz);
            SelectObject(ItemBitmap.HMemDC, hot2 && Configuration.SingleClick ? FontUL : Font);
            SetTextColor(ItemBitmap.HMemDC, hot2 ? GetCOLORREF(CurrentColors[HOT_PANEL]) : GetSysColor(COLOR_BTNTEXT));
            char* colExtStr = column->Name + nameLen + 1; // the text "Ext" is stored behind the name (in the same buffer)
            int colExtStrLen = (int)strlen(colExtStr);
            TextOut(ItemBitmap.HMemDC, textLeft, (r.bottom - FontCharHeight) / 2,
                    colExtStr, colExtStrLen);
            if (panel->SortType == stExtension)
            {
                // arrow pointing in the direction of shifting
                GetTextExtentPoint32(ItemBitmap.HMemDC, colExtStr, colExtStrLen, &sz);

                BitBlt(ItemBitmap.HMemDC,
                       textLeft + sz.cx + SORT_BITMAP_W,
                       (r.bottom - SORT_BITMAP_H) / 2,
                       SORT_BITMAP_W, SORT_BITMAP_H,
                       hMemDC, panel->ReverseSort ? SORT_BITMAP_W : 0, 0, SRCCOPY);
            }
        }
        if (hMemDC != NULL)
            HANDLES(DeleteDC(hMemDC));

        SetTextColor(ItemBitmap.HMemDC, oldColor);
        SetBkMode(ItemBitmap.HMemDC, oldMode);
        SelectObject(ItemBitmap.HMemDC, hOldFont);
    }

    // draw perimeter lines
    HPEN hOldPen = (HPEN)SelectObject(ItemBitmap.HMemDC, BtnHilightPen);
    MoveToEx(ItemBitmap.HMemDC, r.left, r.top, NULL);
    LineTo(ItemBitmap.HMemDC, r.right, r.top);
    if (first)
    {
        MoveToEx(ItemBitmap.HMemDC, r.left, r.top, NULL);
        LineTo(ItemBitmap.HMemDC, r.left, r.bottom);
    }
    else
    {
        MoveToEx(ItemBitmap.HMemDC, r.left, r.top + 2, NULL);
        LineTo(ItemBitmap.HMemDC, r.left, r.bottom - 2);
    }

    SelectObject(ItemBitmap.HMemDC, BtnShadowPen);
    MoveToEx(ItemBitmap.HMemDC, r.left, r.bottom - 1, NULL);
    LineTo(ItemBitmap.HMemDC, r.right, r.bottom - 1);
    if (!last)
    {
        MoveToEx(ItemBitmap.HMemDC, r.right - 1, r.top + 2, NULL);
        LineTo(ItemBitmap.HMemDC, r.right - 1, r.bottom - 2);
    }
    SelectObject(ItemBitmap.HMemDC, hOldPen);

    if (DownVisible && DownIndex == index)
        InvertRect(ItemBitmap.HMemDC, &r);

    BitBlt(hDC, x + r.left, r.top,
           r.right - r.left, r.bottom - r.top,
           ItemBitmap.HMemDC, 0, 0, SRCCOPY);
}

void CHeaderLine::PaintItem2(int index)
{
    HDC hDC = HANDLES(GetDC(HWindow));
    PaintItem(hDC, index);
    HANDLES(ReleaseDC(HWindow, hDC));
}

void CHeaderLine::SetMinWidths()
{
    int columnsCount = Columns->Count;

    HDC hDC = Parent->HPrivateDC;
    HFONT hOldFont = (HFONT)SelectObject(hDC, Font);

    SIZE sz;
    int i;
    for (i = 0; i < columnsCount; i++)
    {
        CColumn* column = &Columns->At(i);
        GetTextExtentPoint32(hDC, column->Name, lstrlen(column->Name), &sz);
        column->MinWidth = 1 + 3 + sz.cx + 3 + 1;

        // if advising by column, I add the width of the bitmap and the space around it
        if (column->SupportSorting == 1)
            column->MinWidth += 2 * SORT_BITMAP_W;

        // if we are measuring the "Name" column and the next column is not "Ext" and also
        // Integration of "Ext" into "Name" is allowed, adding the width "Ext"
        if (i == 0 && !Parent->Parent->IsExtensionInSeparateColumn() &&
            (Parent->Parent->ValidFileData & VALID_DATA_EXTENSION))
        {
            char* colExtStr = column->Name + strlen(column->Name) + 1; // the text "Ext" is stored behind the name (in the same buffer)
            int colExtStrLen = (int)strlen(colExtStr);
            GetTextExtentPoint32(hDC, colExtStr, colExtStrLen, &sz);
            // "Ext" column supports sorting
            column->MinWidth += SORT_BITMAP_W + sz.cx + 2 * SORT_BITMAP_W;
        }
    }
    SelectObject(hDC, hOldFont);
}

CHeaderHitTestEnum
CHeaderLine::HitTest(int xPos, int yPos, int& index, BOOL& extInName)
{
    extInName = FALSE;
    if (xPos < 0 || xPos > Width || yPos < 0 || yPos > Height)
        return hhtNone;

    int left = -Parent->XOffset;
    int right;
    int columnsCount = Columns->Count;
    int i;
    for (i = 0; i < columnsCount; i++)
    {
        CColumn* column = &Columns->At(i);
        right = left + column->Width;
        if (xPos >= left && xPos < right)
        {
            // test the separators
            if (xPos < left + 2 && i > 0 && Columns->At(i - 1).FixedWidth == 1)
            {
                index = i - 1;
                return hhtDivider;
            }
            if (xPos >= right - 2 && column->FixedWidth == 1)
            {
                index = i;
                return hhtDivider;
            }
            index = i;
            if (index == 0 && (Parent->Parent->ValidFileData & VALID_DATA_EXTENSION) &&
                !Parent->Parent->IsExtensionInSeparateColumn())
            {
                // Measure the base text
                SIZE sz;
                HFONT hOldFont = (HFONT)SelectObject(ItemBitmap.HMemDC, Font);
                GetTextExtentPoint32(ItemBitmap.HMemDC, column->Name, lstrlen(column->Name), &sz);
                SelectObject(ItemBitmap.HMemDC, hOldFont);
                if (xPos - left >= 1 + 3 + sz.cx + 3 * SORT_BITMAP_W)
                    extInName = TRUE;
            }
            return hhtItem;
        }
        left = right;
    }
    return hhtNone;
}

void CHeaderLine::Cancel()
{
    SetCurrentToolTip(NULL, 0);
    MouseIsTracked = FALSE;
    if (DownVisible && DownIndex != -1)
    {
        DownVisible = FALSE;
        PaintItem2(DownIndex);
    }
    DragIndex = -1;
    DownIndex = -1;
    if (HotIndex != -1)
    {
        int hi = HotIndex;
        HotIndex = -1;
        PaintItem2(hi);
    }
    if (MainWindow->HDisabledKeyboard == HWindow)
        MainWindow->HDisabledKeyboard = NULL;
    if (OldDragColWidth != -1)
    {
        Parent->Parent->OnHeaderLineColWidthChanged();
        OldDragColWidth = -1;
    }
}

LRESULT
CHeaderLine::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
    {
        return TRUE;
    }

    case WM_SIZE:
    {
        ItemBitmap.Enlarge(1, HIWORD(lParam));
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
        PaintAllItems(hDC, NULL); // draw the area that needs it
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_SETCURSOR:
    {
        if (MouseIsTracked)
            return TRUE;
        break;
    }

    case WM_CAPTURECHANGED:
    {
        HWND hwndNewCapture = (HWND)lParam;
        if (hwndNewCapture != HWindow)
            Cancel();
        break;
    }

    case WM_USER_MOUSEWHEEL:
    case WM_USER_MOUSEHWHEEL:
    {
        // and the wheel also spins above the header line
        return SendMessage(Parent->HWindow, uMsg, wParam, lParam);
    }

    case WM_MOUSELEAVE:
    {
        Cancel();
        break;
    }

    case WM_DESTROY: // if the cursor is above the headerline + the item is highlighted, we want to remove the highlighting before closing the window
    case WM_CANCELMODE:
    {
        if (GetCapture() == HWindow)
            ReleaseCapture();
        Cancel();
        break;
    }

    case WM_USER_TTGETTEXT:
    {
        /*        DWORD id = wParam;
      char *text = (char *)lParam;
      if (id != 0xFFFF)  // divider
      {
        WORD index = id & 0x0000FFFF;
        BOOL ext = id & 0xFFFF0000;
        if (!ext && index < Columns->Count)
          lstrcpy(text, Columns->At(i).Description);
        if (ext && Columns->Count >= 2)
          lstrcpy(text, Columns->At(1).Description); // extract description for the torn extension
      }*/
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        int xPos = (short)LOWORD(lParam);
        int yPos = (short)HIWORD(lParam);
        BOOL mPressed = (wParam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) != 0;
        int index;
        BOOL extInName;
        CHeaderHitTestEnum ht = HitTest(xPos, yPos, index, extInName);
        if (DownIndex != -1)
        {
            BOOL oldDownVisible = DownVisible;
            if (DownVisible && (ht == hhtNone || ht == hhtDivider || index != DownIndex))
                DownVisible = FALSE;
            else if (!DownVisible && ht == hhtItem && index == DownIndex)
                DownVisible = TRUE;
            if (DownVisible != oldDownVisible)
            {
                PaintItem2(DownIndex);
            }
        }
        if (DragIndex != -1)
        {
            int x = -Parent->XOffset;
            int i;
            for (i = 0; i < DragIndex; i++)
                x += Columns->At(i).Width;
            int newWidth = xPos - x;
            CColumn* column = &Columns->At(DragIndex);
            if (newWidth < (int)column->MinWidth)
                newWidth = column->MinWidth;
            if (newWidth != (int)column->Width)
            {
                int delta = newWidth - (int)column->Width;
                column->Width = newWidth;
                // redraw header line
                HDC hDC = HANDLES(GetDC(HWindow));
                HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
                RECT r;
                r.left = x + column->Width;
                r.top = 0;
                r.right = Width;
                r.bottom = Height;
                //          TRACE_I("left:"<<r.left<<" right:"<<r.right<<" delta:"<<delta);
                ScrollWindowEx(HWindow, delta, 0,
                               &r, &r, hUpdateRgn, NULL, 0);
                // to the area to be redrawn, I will add a pulled column
                HRGN hColRgn = HANDLES(CreateRectRgn(x, r.top, x + column->Width, r.bottom));
                CombineRgn(hUpdateRgn, hUpdateRgn, hColRgn, RGN_OR);
                HANDLES(DeleteObject(hColRgn));
                // let's redraw the header
                PaintAllItems(hDC, hUpdateRgn);
                HANDLES(DeleteObject(hUpdateRgn));
                HANDLES(ReleaseDC(HWindow, hDC));

                int totalWidth = Parent->ItemWidth + delta;
                Parent->SetItemWidthHeight(totalWidth, Parent->ItemHeight);
                Parent->SetupScrollBars(UPDATE_HORZ_SCROLL);
                Parent->CheckAndCorrectBoundaries();

                // redraw panel
                r = Parent->FilesRect;
                r.left = x + column->Width;
                hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
                ScrollWindowEx(Parent->HWindow, delta, 0,
                               &r, &r, hUpdateRgn, NULL, 0);
                // to the area to be redrawn, I will add a pulled column
                hColRgn = HANDLES(CreateRectRgn(x, r.top, x + column->Width, r.bottom));
                CombineRgn(hUpdateRgn, hUpdateRgn, hColRgn, RGN_OR);
                HANDLES(DeleteObject(hColRgn));
                // let the panel be redrawn
                Parent->PaintAllItems(hUpdateRgn, 0);
                HANDLES(DeleteObject(hUpdateRgn));
            }
        }
        if (DownIndex == -1 && DragIndex == -1)
        {
            if (ht == hhtItem)
                SetCurrentToolTip(HWindow, HotIndex | HotExt << 16);
            else
                SetCurrentToolTip(HWindow, 0xFFFF);
            int oldHotIndex = HotIndex;
            BOOL oldHotExt = HotExt;
            if (!mPressed && (ht == hhtItem || ht == hhtDivider))
            {
                if (!MouseIsTracked)
                {
                    TRACKMOUSEEVENT tme;
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = HWindow;
                    TrackMouseEvent(&tme);
                    MouseIsTracked = TRUE;
                }
                if (ht == hhtItem)
                {
                    if (Columns->At(index).SupportSorting == 1)
                    {
                        HotIndex = index;
                        HotExt = extInName;
                        if (Configuration.SingleClick)
                            SetHandCursor();
                        else
                            SetCursor(LoadCursor(NULL, IDC_ARROW));
                    }
                    else
                    {
                        HotIndex = -1;
                        HotExt = FALSE;
                        SetCursor(LoadCursor(NULL, IDC_ARROW));
                    }
                }
                else
                {
                    HotIndex = -1;
                    SetCursor(LoadCursor(HInstance, MAKEINTRESOURCE(IDC_SPLIT)));
                }
            }
            if (ht == hhtNone)
            {
                SetCurrentToolTip(NULL, 0);
                if (MouseIsTracked)
                {
                    if (GetCapture() == HWindow)
                        ReleaseCapture();
                    HotIndex = -1;
                    if (Configuration.SingleClick)
                        SetCursor(LoadCursor(NULL, IDC_ARROW));
                }
            }
            if (HotIndex != oldHotIndex || oldHotExt != HotExt)
            {
                HDC hDC = HANDLES(GetDC(HWindow));
                if (oldHotIndex != -1 && (HotIndex != oldHotIndex))
                    PaintItem(hDC, oldHotIndex);
                if (HotIndex != -1)
                    PaintItem(hDC, HotIndex);
                HANDLES(ReleaseDC(HWindow, hDC));
            }
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    {
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
        SetCurrentToolTip(NULL, 0);
        int xPos = (short)LOWORD(lParam);
        int yPos = (short)HIWORD(lParam);
        int index;
        BOOL extInName;
        CHeaderHitTestEnum ht = HitTest(xPos, yPos, index, extInName);
        if (ht == hhtItem && Columns->At(index).SupportSorting == 1)
        {
            HotIndex = -1;
            DownIndex = index;
            DownVisible = TRUE;
            SetCapture(HWindow);
            MainWindow->HDisabledKeyboard = HWindow;

            PaintItem2(DownIndex);
        }
        if (ht == hhtDivider)
        {
            if (uMsg == WM_LBUTTONDOWN)
            {
                OldDragColWidth = Columns->At(index).Width;
                SetCapture(HWindow);
                DragIndex = index;
                MainWindow->HDisabledKeyboard = HWindow;
            }
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        SetCurrentToolTip(NULL, 0);
        int xPos = (short)LOWORD(lParam);
        int yPos = (short)HIWORD(lParam);
        int index;
        BOOL extInName;
        CHeaderHitTestEnum ht = HitTest(xPos, yPos, index, extInName);
        if (DownVisible && GetCapture() == HWindow)
        {
            ReleaseCapture();
            Cancel();
            if (ht == hhtItem && index >= 0 && index < Columns->Count)
            {
                CColumn* column = &Columns->At(index);
                CSortType st = stName;
                if (index == 0 && extInName)
                    st = stExtension;
                else if (column->ID == COLUMN_ID_NAME)
                    st = stName;
                else if (column->ID == COLUMN_ID_EXTENSION)
                    st = stExtension;
                else if (column->ID == COLUMN_ID_SIZE)
                    st = stSize;
                else if (column->ID == COLUMN_ID_DATE || column->ID == COLUMN_ID_TIME)
                    st = stTime;
                else if (column->ID == COLUMN_ID_ATTRIBUTES)
                    st = stAttr;
                Parent->Parent->ChangeSortType(st, TRUE);
            }
        }
        else
        {
            if (GetCapture() == HWindow)
                ReleaseCapture();
            Cancel();
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
