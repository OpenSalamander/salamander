// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dbviewer.rh"
#include "dbviewer.rh2"
#include "lang\lang.rh"
#include "data.h"
#include "renderer.h"
#include "dialogs.h"
#include "dbviewer.h"

//****************************************************************************
//
// CRendererWindow
//

void CRendererWindow::PaintTopMargin(HDC hDC, HRGN hUpdateRgn, const RECT* clipRect, BOOL selChangeOnly)
{
    RECT r;
    r.top = 0;
    r.bottom = RowHeight - 1;

    char textBuffer[10000];

    COLORREF oldTextColor = SetTextColor(hDC, GetSysColor(COLOR_BTNTEXT));
    COLORREF normalBkColor = GetSysColor(COLOR_BTNFACE);
    COLORREF selectionBkColor = RGB(182, 190, 210);
    COLORREF oldBkColor = SetBkColor(hDC, normalBkColor);
    HPEN hOldPen = (HPEN)SelectObject(hDC, HGrayPen);

    BOOL leftExcluded = FALSE;
    int x = 0;

    if (!((x < clipRect->left && x + RowHeight <= clipRect->left) ||
          (x >= clipRect->right && x + RowHeight > clipRect->right)))
    {
        if (!selChangeOnly)
        {
            // nakreslime nulty sloupec
            SelectClipRgn(hDC, hUpdateRgn);
            r.left = x;
            r.right = x + RowHeight - 1;
            ExtTextOut(hDC, x + LeftTextMargin, TopTextMargin, ETO_OPAQUE,
                       &r, "", 0, NULL);
            // nakreslime oddelovaci cary
            MoveToEx(hDC, r.left, r.bottom, NULL);
            LineTo(hDC, r.right, r.bottom);
            LineTo(hDC, r.right, r.top - 1);
            ExcludeClipRect(hDC, 0, 0, RowHeight, RowHeight);
            leftExcluded = TRUE;
        }
    }
    x = RowHeight - XOffset;

    int visibleIndex = 0;
    int i;
    for (i = 0; i < Database.GetColumnCount(); i++)
    {
        const CDatabaseColumn* column = Database.GetColumn(i);
        if (column->Visible)
        {
            if (!((x < clipRect->left && x + column->Width <= clipRect->left) ||
                  (x >= clipRect->right && x + column->Width > clipRect->right)))
            {
                if (!selChangeOnly || Selection.ChangedColumn(&OldSelection, visibleIndex))
                {
                    BOOL inSelection = FALSE;
                    if (Selection.ContainsColumn(visibleIndex))
                    {
                        SetBkColor(hDC, selectionBkColor);
                        SelectObject(hDC, HSelectionPen);
                        inSelection = TRUE;
                    }

                    // zobrazime text

                    if (selChangeOnly && x < RowHeight && !leftExcluded)
                    {
                        ExcludeClipRect(hDC, 0, 0, RowHeight, RowHeight);
                        leftExcluded = TRUE;
                    }

                    r.left = x;
                    r.right = x + column->Width - 1;

                    if (!Database.GetIsUnicode())
                    {
                        size_t textLen = strlen(column->Name);
                        if (textLen > 0)
                        {
                            memcpy(textBuffer, column->Name, min(textLen, 9999));
                            textBuffer[9999] = 0;
                            CodeCharacters(textBuffer, textLen);
                        }

                        ExtTextOutA(hDC, x + LeftTextMargin, TopTextMargin, ETO_OPAQUE,
                                    &r, textBuffer, (UINT)textLen, NULL);
                    }
                    else
                    {
                        LPCWSTR colName = (LPCWSTR)column->Name;

                        ExtTextOutW(hDC, x + LeftTextMargin, TopTextMargin, ETO_OPAQUE,
                                    &r, colName, (UINT)wcslen(colName), NULL);
                    }

                    // nakreslime oddelovaci cary
                    MoveToEx(hDC, r.left, r.bottom, NULL);
                    LineTo(hDC, r.right, r.bottom);
                    LineTo(hDC, r.right, r.top - 1);

                    if (inSelection)
                    {
                        SetBkColor(hDC, normalBkColor);
                        SelectObject(hDC, HGrayPen);
                    }
                }
            }
            x += column->Width;
            visibleIndex++;
        }
    }

    if (x < Width)
    {
        // dokreslime prazdne misto za hlavickou
        r.left = x;
        r.right = Width;
        r.bottom++;
        SetBkColor(hDC, GetSysColor(COLOR_WINDOW));
        ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);
    }

    SelectObject(hDC, hOldPen);
    SetTextColor(hDC, oldTextColor);
    SetTextColor(hDC, oldBkColor);
}

void CRendererWindow::PaintBody(HDC hDC, HRGN hUpdateRgn, const RECT* clipRect, BOOL selChangeOnly)
{
    COLORREF oldTextColor = SetTextColor(hDC, GetSysColor(COLOR_BTNTEXT));
    COLORREF normalBkColor = GetSysColor(COLOR_WINDOW);
    COLORREF selectionBkColor = RGB(182, 190, 210);
    COLORREF oldBkColor = SetBkColor(hDC, normalBkColor);
    HPEN hOldPen = (HPEN)GetCurrentObject(hDC, OBJ_PEN);

    char textBuffer[10000];

    RECT r;
    r.top = RowHeight;
    r.bottom = 2 * RowHeight - 1;

    int fetchedIndex = -1;

    int i;
    for (i = TopIndex; i < Database.GetRowCount(); i++)
    {
        if (!((r.top <= clipRect->top && r.bottom < clipRect->top) ||
              (r.top > clipRect->bottom && r.bottom > clipRect->bottom)))
        {
            BOOL leftExcluded = FALSE;
            int x = 0;

            if (!((x < clipRect->left && x + RowHeight <= clipRect->left) ||
                  (x >= clipRect->right && x + RowHeight > clipRect->right)))
            {
                if (!selChangeOnly || Selection.ChangedRow(&OldSelection, i))
                {
                    // nakreslim levy sloupec
                    COLORREF oldBkColor2;
                    if (Selection.ContainsRow(i))
                    {
                        oldBkColor2 = SetBkColor(hDC, selectionBkColor);
                        SelectObject(hDC, HSelectionPen);
                    }
                    else
                    {
                        oldBkColor2 = SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
                        SelectObject(hDC, HGrayPen);
                    }
                    SelectClipRgn(hDC, hUpdateRgn);
                    r.left = 0;
                    r.right = RowHeight - 1;
                    ExtTextOut(hDC, 0, r.top, ETO_OPAQUE, &r, "", 0, NULL);
                    if (fetchedIndex != i)
                    {
                        if (!Database.FetchRecord(HWindow, i))
                        {
                            // Fetching the record failed -> do not draw anything
                            continue;
                        }
                        fetchedIndex = i;
                    }
                    if (Database.IsRecordDeleted())
                    {
                        if (HDeleteIcon == NULL)
                            HDeleteIcon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_DELETED), IMAGE_ICON, 16, 16, SalGeneral->GetIconLRFlags());
                        DrawIconEx(hDC, (RowHeight - 16) / 2 + 1, r.top + (RowHeight - 16) / 2 + 1,
                                   HDeleteIcon, 0, 0, 0, NULL, DI_NORMAL);
                    }
                    // nakreslime oddelovaci cary
                    MoveToEx(hDC, r.left, r.bottom, NULL);
                    LineTo(hDC, r.right, r.bottom);
                    LineTo(hDC, r.right, r.top - 1);
                    SetBkColor(hDC, oldBkColor2);
                    ExcludeClipRect(hDC, 0, RowHeight, RowHeight, Height);
                    leftExcluded = TRUE;
                }
            }

            SelectObject(hDC, HLtGrayPen);
            x = RowHeight - XOffset;

            // nakreslim viditelne sloupce
            int visibleIndex = 0;
            int j;
            for (j = 0; j < Database.GetColumnCount(); j++)
            {
                const CDatabaseColumn* column = Database.GetColumn(j);
                if (column->Visible)
                {
                    if (!((x < clipRect->left && x + column->Width <= clipRect->left) ||
                          (x >= clipRect->right && x + column->Width > clipRect->right)))
                    {
                        if (!selChangeOnly || Selection.Changed(&OldSelection, visibleIndex, i))
                        {
                            //              TRACE_I("Paint row="<<i<<" col="<<j);
                            // vytahneme text
                            if (fetchedIndex != i)
                            {
                                if (!Database.FetchRecord(HWindow, i))
                                {
                                    // Fetching the record failed -> do not draw anything
                                    continue;
                                }
                                fetchedIndex = i;
                            }
                            if (selChangeOnly && x < RowHeight && !leftExcluded)
                            {
                                ExcludeClipRect(hDC, 0, RowHeight, RowHeight, Height);
                                leftExcluded = TRUE;
                            }

                            r.left = x;
                            r.right = x + column->Width - 1;

                            size_t textLen;
                            SIZE sz;
                            LPCSTR text;
                            LPCWSTR textW;

                            if (!Database.GetIsUnicode())
                            {
                                text = Database.GetCellText(column, &textLen);
                                textLen = min(textLen, 9999);

                                if (textLen > 0)
                                {
                                    memcpy(textBuffer, text, textLen);
                                    textBuffer[textLen] = 0;
                                    CodeCharacters(textBuffer, textLen);
                                }

                                // namerime sirku textu
                                GetTextExtentPoint32A(hDC, textBuffer, (int)textLen, &sz);
                            }
                            else
                            {
                                textW = Database.GetCellTextW(column, &textLen);
                                GetTextExtentPoint32W(hDC, textW, (int)textLen, &sz);
                            }

                            // nakreslime text
                            int xOffset = 0;
                            if (!column->LeftAlign)
                                xOffset = r.right - r.left - 2 * LeftTextMargin - sz.cx;

                            BOOL inSelection = FALSE;
                            if (Selection.Contains(visibleIndex, i))
                            {
                                SetBkColor(hDC, selectionBkColor);
                                inSelection = TRUE;
                            }
                            if (!Database.GetIsUnicode())
                                ExtTextOutA(hDC, x + LeftTextMargin + xOffset, r.top + TopTextMargin,
                                            ETO_OPAQUE | ETO_CLIPPED, &r, textBuffer, (UINT)textLen, NULL);
                            else
                                ExtTextOutW(hDC, x + LeftTextMargin + xOffset, r.top + TopTextMargin,
                                            ETO_OPAQUE | ETO_CLIPPED, &r, textW, (UINT)textLen, NULL);
                            if (Bookmarks.IsMarked(visibleIndex, i))
                            {
                                if (HMarkedIcon == NULL)
                                    HMarkedIcon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_MARKED),
                                                                   IMAGE_ICON, 16, 16, SalGeneral->GetIconLRFlags());
                                DrawIconEx(hDC, r.right - 16, r.top,
                                           HMarkedIcon, 0, 0, 0, NULL, DI_NORMAL);
                            }

                            if (inSelection)
                            {
                                SetBkColor(hDC, normalBkColor);
                                if (Selection.IsFocus(visibleIndex, i))
                                {
                                    SelectObject(hDC, HBlackPen);
                                    MoveToEx(hDC, r.left, r.top, NULL);
                                    LineTo(hDC, r.left, r.bottom - 1);
                                    LineTo(hDC, r.right - 1, r.bottom - 1);
                                    LineTo(hDC, r.right - 1, r.top);
                                    LineTo(hDC, r.left, r.top);
                                    SelectObject(hDC, HLtGrayPen);
                                }
                            }

                            // nakreslime oddelovaci cary
                            MoveToEx(hDC, r.left, r.bottom, NULL);
                            LineTo(hDC, r.right, r.bottom);
                            LineTo(hDC, r.right, r.top - 1);
                        }
                    }
                    x += column->Width;
                    visibleIndex++;
                }
            }

            if (x < Width)
            {
                // dokreslime zbytek prazdneho radku
                RECT r2;
                r2 = r;
                r2.left = x;
                r2.right = Width;
                r2.bottom++;
                ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &r2, "", 0, NULL);
            }
        }

        r.top += RowHeight;
        r.bottom += RowHeight;

        if (r.top >= Height)
            break;
    }

    if (r.top < Height)
    {
        // dokreslime prazdny prostor pod tabulkou
        SelectClipRgn(hDC, hUpdateRgn);
        r.left = 0;
        r.right = Width;
        r.bottom = Height;
        ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);
    }

    SelectObject(hDC, hOldPen);
    SetTextColor(hDC, oldTextColor);
    SetTextColor(hDC, oldBkColor);
}

void CRendererWindow::Paint(HDC hDC, HRGN hUpdateRgn, BOOL selChangeOnly)
{
    if (Database.IsOpened())
    {
        BOOL releaseDC = FALSE;
        if (hDC == NULL)
        {
            hDC = GetDC(HWindow);
            releaseDC = TRUE;

            if (hUpdateRgn != NULL)
                SelectClipRgn(hDC, hUpdateRgn);
        }

        RECT clipRect;
        int clipRectType = GetClipBox(hDC, &clipRect);
        if (clipRectType != SIMPLEREGION && clipRectType != COMPLEXREGION)
        {
            clipRect.left = 0;
            clipRect.top = 0;
            clipRect.right = Width;
            clipRect.bottom = Height;
        }

        HFONT hOldFont = (HFONT)SelectObject(hDC, HFont);
        PaintTopMargin(hDC, hUpdateRgn, &clipRect, selChangeOnly);
        PaintBody(hDC, hUpdateRgn, &clipRect, selChangeOnly);
        SelectObject(hDC, hOldFont);

        if (hUpdateRgn != NULL)
            SelectClipRgn(hDC, NULL); // vykopneme clip region, pokud jsme ho nastavili

        if (releaseDC)
            ReleaseDC(HWindow, hDC);

        if (!selChangeOnly)
            ValidateRect(HWindow, NULL);
    }
}
