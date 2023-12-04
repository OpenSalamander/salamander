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

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#define TIMER_SCROLL_ID 1

BOOL IsAlphaNumeric[256]; // pole TRUE/FALSE pro znaky (FALSE = neni pismeno ani cislice)
BOOL IsAlpha[256];

//****************************************************************************
//
// CSelection
//

CSelection::CSelection()
{
    FocusX = 0;
    FocusY = 0;
    AnchorX = 0;
    AnchorY = 0;
    Normalize();
}

CSelection&
CSelection::operator=(const CSelection& s)
{
    FocusX = s.FocusX;
    FocusY = s.FocusY;
    AnchorX = s.AnchorX;
    AnchorY = s.AnchorY;
    Rect = s.Rect;
    return *this;
}

void CSelection::Normalize()
{
    if (FocusX <= AnchorX)
    {
        Rect.left = FocusX;
        Rect.right = AnchorX;
    }
    else
    {
        Rect.left = AnchorX;
        Rect.right = FocusX;
    }
    if (FocusY <= AnchorY)
    {
        Rect.top = FocusY;
        Rect.bottom = AnchorY;
    }
    else
    {
        Rect.top = AnchorY;
        Rect.bottom = FocusY;
    }
}

//****************************************************************************
//
// CBookmarkList
//

CBookmarkList::CBookmarkList()
    : Bookmarks(10, 10)
{
}

void CBookmarkList::Toggle(int x, int y)
{
    int index;
    if (GetIndex(x, y, &index))
    {
        Bookmarks.Delete(index);
    }
    else
    {
        CBookmark bookmark;
        bookmark.X = x;
        bookmark.Y = y;
        Bookmarks.Add(bookmark);
    }
}

BOOL CBookmarkList::GetNext(int x, int y, int* newX, int* newY, BOOL next)
{
    int count = Bookmarks.Count;
    int index;
    if (!GetIndex(x, y, &index))
    {
        if (count > 0)
        {
            *newX = Bookmarks[next ? 0 : count - 1].X;
            *newY = Bookmarks[next ? 0 : count - 1].Y;
            return TRUE;
        }
        else
            return FALSE;
    }
    else
    {
        if (count < 2)
            return FALSE;
        int newIndex = index + (next ? 1 : -1);
        if (newIndex >= count)
            newIndex = 0;
        if (newIndex < 0)
            newIndex = count - 1;
        *newX = Bookmarks[newIndex].X;
        *newY = Bookmarks[newIndex].Y;
        return TRUE;
    }
}

BOOL CBookmarkList::IsMarked(int x, int y)
{
    int index;
    return GetIndex(x, y, &index);
}

void CBookmarkList::ClearAll()
{
    Bookmarks.DestroyMembers();
}

BOOL CBookmarkList::GetIndex(int x, int y, int* index)
{
    int count = Bookmarks.Count;
    int i;
    for (i = 0; i < count; i++)
    {
        CBookmark* bookmark = &Bookmarks[i];
        if (bookmark->X == x && bookmark->Y == y)
        {
            *index = i;
            return TRUE;
        }
    }
    return FALSE;
}

//****************************************************************************
//
// CRendererWindow
//

CRendererWindow::CRendererWindow(int enumFilesSourceUID, int enumFilesCurrentIndex)
    : CWindow(ooStatic),
      EnumFilesSourceUID(enumFilesSourceUID), EnumFilesCurrentIndex(enumFilesCurrentIndex)
{
    Database.SetRenderer(this);
    HGrayPen = NULL;
    HLtGrayPen = NULL;
    HSelectionPen = NULL;
    HBlackPen = NULL;
    HFont = NULL;
    HDeleteIcon = NULL;
    HMarkedIcon = NULL;
    RowHeight = 0;
    CharAvgWidth = 0;
    TopTextMargin = 0;
    LeftTextMargin = 0;
    Width = 0;
    Height = 0;
    TopIndex = 0;
    XOffset = 0;
    RowsOnPage = 1;
    DragMode = dsmNone;
    ScrollTimerID = 0;
    DragColumn = -1;

    AutoSelect = CfgAutoSelect;
    lstrcpy(DefaultCoding, CfgDefaultCoding);
    UseCodeTable = FALSE;
    Coding[0] = 0;

    Creating = TRUE;

    CreateGraphics();

    ResetMouseWheelAccumulator();
}

CRendererWindow::~CRendererWindow()
{
    ReleaseGraphics();
    Database.Close();
    if (HDeleteIcon != NULL)
    {
        DestroyIcon(HDeleteIcon);
        HDeleteIcon = NULL;
    }
    if (HMarkedIcon != NULL)
    {
        DestroyIcon(HMarkedIcon);
        HMarkedIcon = NULL;
    }
}

void CRendererWindow::OnFileOpen()
{
    char file[MAX_PATH];
    file[0] = 0;
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = HWindow;
    char* s = LoadStr(IDS_VIEWERFILTER);
    ofn.lpstrFilter = s;
    while (*s != 0) // vytvoreni double-null terminated listu
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (SalGeneral->SafeGetOpenFileName(&ofn))
    {
        EnumFilesSourceUID = -1;
        OpenFile(file, TRUE);
    }
}

void CRendererWindow::OnFileReOpen()
{
    if (!Database.IsOpened())
        return;

    char path[MAX_PATH];
    lstrcpy(path, Database.GetFileName());
    OpenFile(path, FALSE);
}

void CRendererWindow::OnGoto()
{
    if (Viewer->Enablers[vweDBOpened])
    {
        int x, y, count = Database.GetRowCount();

        Selection.GetFocus(&x, &y);
        CGoToDialog dlg(HWindow, &y, count);

        if ((dlg.Execute() == IDOK) && (y != count))
        {
            Selection.SetFocusAndAnchor(x, y);
            Viewer->UpdateRowNumberOnToolBar(y, count);
            EnsureRowIsVisible(y);
            Paint(NULL, NULL, FALSE);
        }
    }
}

void CRendererWindow::SetViewerTitle()
{
    char title[MAX_PATH + 300];
    if (Database.IsOpened())
    {
        sprintf(title, "%s - %s", Database.GetFileName(), LoadStr(IDS_PLUGINNAME));
        if (UseCodeTable || Database.GetIsUnicode())
            sprintf(title + strlen(title), " - [%s]", Coding);
    }
    else
        sprintf(title, "%s", LoadStr(IDS_PLUGINNAME));

    SetWindowText(GetParent(HWindow), title);
}

BOOL CRendererWindow::OpenFile(const char* name, BOOL useDefaultConfig)
{
    CALL_STACK_MESSAGE3("CRendererWindow::OpenFile(%s, %d)", name, useDefaultConfig);

    if (useDefaultConfig)
        Viewer->CfgCSV = CfgDefaultCSV;
    Bookmarks.ClearAll();
    // pokud pri OpenFile dojde k chybe, bude podmazano pozadi
    InvalidateRect(HWindow, NULL, TRUE);

    BOOL ret = Database.Open(name);

    if (ret)
    {
        Viewer->UpdateEnablers(); // Update the Unicode flag
        if (AutoSelect || Database.GetIsUnicode())
            RecognizeCodePage();
        else
        {
            UseCodeTable = FALSE;
            Coding[0] = 0;
            if (DefaultCoding[0] != 0)
            {
                char codeTable[256];
                if (SalGeneral->GetConversionTable(HWindow, codeTable, DefaultCoding))
                {
                    memcpy(CodeTable, codeTable, 256);
                    UseCodeTable = TRUE;
                    strcpy(Coding, DefaultCoding);
                }
            }
        }
    }

    TopIndex = 0;
    XOffset = 0;
    Selection.SetFocusAndAnchor(0, 0);
    // No row is active when there are no rows in the database
    Viewer->UpdateRowNumberOnToolBar(Database.GetRowCount() ? 0 : -1, Database.GetRowCount());
    OldSelection = Selection;

    SetViewerTitle();

    SetupScrollBars();
    InvalidateRect(HWindow, NULL, TRUE);

    Viewer->UpdateEnablers();

    return ret;
}

void CRendererWindow::OnToggleBookmark()
{
    int focusX, focusY;
    Selection.GetFocus(&focusX, &focusY);
    Bookmarks.Toggle(focusX, focusY);
    Paint(NULL, NULL, FALSE);
}

void CRendererWindow::OnNextBookmark(BOOL next)
{
    int focusX, focusY;
    Selection.GetFocus(&focusX, &focusY);
    int x, y;
    if (Bookmarks.GetNext(focusX, focusY, &x, &y, next))
    {
        OldSelection = Selection;
        Selection.SetFocusAndAnchor(x, y);
        Viewer->UpdateRowNumberOnToolBar(y, Database.GetRowCount());
        EnsureRowIsVisible(y);
        EnsureColumnIsVisible(x);
        Paint(NULL, NULL, FALSE);
    }
}

void CRendererWindow::OnClearBookmarks()
{
    if (Bookmarks.GetCount() < 1)
        return;
    Bookmarks.ClearAll();
    Paint(NULL, NULL, FALSE);
}

void CRendererWindow::RecognizeCodePage()
{
    if (!Database.IsOpened())
        return;

    UseCodeTable = FALSE;

    if (Database.GetIsUnicode())
    {
        strcpy(Coding, Database.GetIsUTF8() ? "UTF-8" : "UTF-16");
        return;
    }

    Coding[0] = 0;

    char winCodePage[101];
    SalGeneral->GetWindowsCodePage(HWindow, winCodePage);
    if (winCodePage[0] != 0) // jen pokud je WindowsCodePage znama
    {
        char pattern[10000];
        size_t spaceLeft = 9999;
        char* iter = pattern;

        int i;
        for (i = 0; i < min(100, Database.GetRowCount()); i++)
        {
            if (!Database.FetchRecord(HWindow, i))
                goto STOP_FETCHING;
            int j;
            for (j = 0; j < Database.GetVisibleColumnCount(); j++)
            {
                const CDatabaseColumn* column = Database.GetVisibleColumn(j);
                size_t textLen;
                const char* text = Database.GetCellText(column, &textLen);
                size_t copyChars = min(textLen, spaceLeft);
                memcpy(iter, text, copyChars);
                spaceLeft -= copyChars;
                iter += copyChars;
                if (spaceLeft == 0)
                    goto STOP_FETCHING;
            }
        }

    STOP_FETCHING:

        if (iter > pattern)
        {
            *iter = 0;
            char codePage[101];
            SalGeneral->RecognizeFileType(HWindow, pattern, (int)(iter - pattern), TRUE, NULL, codePage);
            if (codePage[0] != 0)
            {
                char conversion[205];
                lstrcpy(conversion, codePage);
                lstrcat(conversion, winCodePage);

                char codeTable[256];
                if (lstrcmp(codePage, winCodePage) != 0 &&
                    SalGeneral->GetConversionTable(HWindow, codeTable, conversion))
                {
                    memcpy(CodeTable, codeTable, 256);
                    UseCodeTable = TRUE;
                    sprintf(Coding, "%s - %s", codePage, winCodePage);
                }
            }
        }
    }
} /* CRendererWindow::RecognizeCodePage */

void CRendererWindow::CodeCharacters(char* text, size_t textLen)
{
    if (UseCodeTable)
    {
        unsigned char* iter = (unsigned char*)text;
        while (textLen-- > 0)
        {
            *(iter++) = CodeTable[*iter];
        }
    }
}

void CRendererWindow::SelectConversion(const char* conversion)
{
    if (conversion == NULL)
    {
        UseCodeTable = FALSE;
        Coding[0] = 0;
    }
    else
    {
        char codeTable[256];
        if (SalGeneral->GetConversionTable(HWindow, codeTable, conversion))
        {
            memcpy(CodeTable, codeTable, 256);
            UseCodeTable = TRUE;
            lstrcpy(Coding, conversion);
        }
    }
    InvalidateRect(HWindow, NULL, TRUE);
    SetViewerTitle();
}

void CRendererWindow::Find(BOOL forward, BOOL wholeWords,
                           CSalamanderBMSearchData* bmSearchData,
                           CSalamanderREGEXPSearchData* regexpSearchData)
{
    if ((bmSearchData == NULL && regexpSearchData == NULL) ||
        (bmSearchData != NULL && regexpSearchData != NULL))
    {
        TRACE_E("Parameters mismatch");
        return;
    }

    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    int row;
    int col;
    char* buf = NULL;
    BOOL skip = TRUE; // prvni nalez preskocim
    int patLen = bmSearchData ? bmSearchData->GetLength() : 0;
    Selection.GetFocus(&col, &row);
    int bufSize = 0;

    if (UseCodeTable || Database.GetIsUnicode())
    {
        // we must do code-page conversion on the fly
        // first we determine the maximum column width
        // and then allocate appropriate memory block
        int i;
        for (i = 0; i < Database.GetVisibleColumnCount(); i++)
        {
            const CDatabaseColumn* column = Database.GetVisibleColumn(i);
            bufSize = max(bufSize, column->Length);
        }
        buf = (char*)malloc(bufSize);
    }
    do
    {
        if (!Database.FetchRecord(HWindow, row))
            break;
        do
        {
            if (skip)
            {
                skip = FALSE;
            }
            else
            {
                const CDatabaseColumn* column = Database.GetVisibleColumn(col);

                // too narrow columns are not searched in non-regexp mode
                // other possible optimizations: do not search DBF_FTYPE_INT_V7,
                // DBF_FTYPE_TSTAMP, DBF_FTYPE_AUTOINC & DBF_FTYPE_DOUBLE columns if
                // the search pattern contains non-numeric characters
                if (!bmSearchData || (column->Length >= patLen))
                {
                    int found; // -1=not found
                    size_t textLen;
                    const char* text;
                    int offset = 0;

                    if (!Database.GetIsUnicode())
                    {
                        text = Database.GetCellText(column, &textLen);

                        if (UseCodeTable)
                        {
                            memcpy(buf, text, textLen);
                            CodeCharacters(buf, textLen);
                            text = buf;
                        }
                    }
                    else
                    {
                        LPCWSTR textW = Database.GetCellTextW(column, &textLen);
                        textLen = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, textW, (int)textLen, buf, bufSize, NULL, NULL);
                        if (textLen < 0)
                            textLen = 0; // Error - swallow it ;-)
                        text = buf;
                    }
                    do
                    {
                        int foundLen;
                        if (bmSearchData != NULL)
                        {
                            found = bmSearchData->SearchForward(text, (int)textLen, offset);
                        }
                        else
                        {
                            regexpSearchData->SetLine(text, text + textLen);
                            found = regexpSearchData->SearchForward(offset, foundLen);
                        }
                        if (found == -1 || !wholeWords)
                            break;
                        if (bmSearchData != NULL)
                            foundLen = bmSearchData->GetLength();
                        if ((found == 0 || !IsAlphaNumeric[*(text + found - 1)]) &&
                            (found + foundLen >= (int)textLen || !IsAlphaNumeric[*(text + found + foundLen)]))
                            break;
                        offset++;
                    } while (1);
                    if (found != -1)
                    {
                        SetCursor(hOldCursor);
                        OldSelection = Selection;
                        Selection.SetFocusAndAnchor(col, row);
                        Viewer->UpdateRowNumberOnToolBar(row, Database.GetRowCount());
                        EnsureRowIsVisible(row);
                        EnsureColumnIsVisible(col);
                        Paint(NULL, NULL, FALSE);
                        if (buf)
                        {
                            free(buf);
                        }
                        return;
                    }
                } // of if (!bmSearchData || (column->Length >= patLen))
            }

            if (forward)
            {
                col++;
                if (col >= Database.GetVisibleColumnCount())
                    break;
            }
            else
            {
                col--;
                if (col < 0)
                    break;
            }

        } while (1);

        if (forward)
        {
            row++;
            if (row >= Database.GetRowCount())
                break;
            col = 0;
        }
        else
        {
            row--;
            if (row < 0)
                break;
            col = Database.GetVisibleColumnCount() - 1;
        }
    } while (1);

    if (buf)
    {
        free(buf);
    }

    SetCursor(hOldCursor);

    char text[1000];
    if (bmSearchData != NULL)
        sprintf(text, LoadStr(IDS_FIND_NOMATCH), bmSearchData->GetPattern());
    else
        sprintf(text, LoadStr(IDS_FIND_NOREGEXPMATCH), regexpSearchData->GetPattern());
    SalGeneral->SalMessageBox(HWindow, text, LoadStr(IDS_FIND), MB_ICONINFORMATION);
} /* CRendererWindow::Find */

void CRendererWindow::CreateGraphics()
{
    LOGFONT lf;
    if (CfgUseCustomFont)
        lf = CfgLogFont;
    else
        GetDefaultLogFont(&lf);
    HFont = CreateFontIndirect(&lf);

    HDC hDC = GetDC(NULL);
    HFONT oldFont = (HFONT)SelectObject(hDC, HFont);
    TEXTMETRIC tm;
    GetTextMetrics(hDC, &tm);
    RowHeight = tm.tmHeight + 4;
    TopTextMargin = 2;
    LeftTextMargin = 3;

    SIZE sz;
    GetTextExtentPoint32(hDC, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
                         52, &sz);
    CharAvgWidth = (sz.cx / 26 + 1) / 2;

    SelectObject(hDC, oldFont);
    ReleaseDC(NULL, hDC);

    HGrayPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNSHADOW));
    HLtGrayPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNFACE));
    HSelectionPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_ACTIVECAPTION));
    HBlackPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNTEXT));
}

void CRendererWindow::ReleaseGraphics()
{
    if (HFont != NULL)
    {
        DeleteObject(HFont);
        HFont = NULL;
    }
    if (HGrayPen != NULL)
    {
        DeleteObject(HGrayPen);
        HGrayPen = NULL;
    }
    if (HLtGrayPen != NULL)
    {
        DeleteObject(HLtGrayPen);
        HLtGrayPen = NULL;
    }
    if (HBlackPen != NULL)
    {
        DeleteObject(HBlackPen);
        HBlackPen = NULL;
    }
    if (HSelectionPen != NULL)
    {
        DeleteObject(HSelectionPen);
        HSelectionPen = NULL;
    }
}

void CRendererWindow::RebuildGraphics()
{
    ReleaseGraphics();
    CreateGraphics();
}

void CRendererWindow::SetupScrollBars(DWORD update)
{
    if (!Database.IsOpened())
        return;
    if (update & UPDATE_HORZ_SCROLL)
    {
        int rowWidth = Database.GetVisibleColumnsWidth();

        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE | SIF_PAGE;
        si.nMin = 0;
        si.nMax = rowWidth - 1;
        si.nPage = Width - RowHeight; // sloupec vlevo
        si.nPos = XOffset;
        // nastavime rolovatka
        SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);
    }

    if (update & UPDATE_VERT_SCROLL)
    {
        int totalRows = Database.GetRowCount();
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE | SIF_PAGE;
        si.nMin = 0;
        si.nMax = totalRows - 1;
        si.nPage = RowsOnPage;
        si.nPos = TopIndex;
        SetScrollInfo(HWindow, SB_VERT, &si, TRUE);
    }
}

BOOL CRendererWindow::GetColumnInfo(int visibleIndex, int* index, int* xPos)
{
    int visibleI = 0;
    int x = 0;
    int i;
    for (i = 0; i < Database.GetColumnCount(); i++)
    {
        const CDatabaseColumn* column = Database.GetColumn(i);
        if (column->Visible)
        {
            if (visibleI == visibleIndex)
            {
                *xPos = x;
                *index = i;
                return TRUE;
            }
            x += column->Width;
            visibleI++;
        }
    }
    return FALSE;
}

void CRendererWindow::EnsureColumnIsVisible(int x)
{
    if (x < 0 || x >= Database.GetVisibleColumnCount())
    {
        TRACE_E("Wrong column: x=" << x);
        return;
    }
    int newXOffset = XOffset;

    int colX = 0;
    int index = 0;
    if (!GetColumnInfo(x, &index, &colX))
        return;
    int colWidth = Database.GetColumn(index)->Width;
    if (colWidth > Width - RowHeight)
        colWidth = Width - RowHeight;

    if (colX < newXOffset)
        newXOffset = colX;
    else
    {
        if (colX + RowHeight + colWidth > XOffset + Width)
            newXOffset = colX - (Width - RowHeight - colWidth);
    }

    if (newXOffset != XOffset)
    {
        HRGN hUpdateRgn = CreateRectRgn(0, 0, 0, 0);
        RECT r;
        r.left = RowHeight;
        r.top = 0;
        r.right = Width;
        r.bottom = Height;
        ScrollWindowEx(HWindow, XOffset - newXOffset, 0,
                       &r, &r, hUpdateRgn, NULL, 0);
        XOffset = newXOffset;
        SetupScrollBars(UPDATE_HORZ_SCROLL);
        Paint(NULL, hUpdateRgn, FALSE);
        DeleteObject(hUpdateRgn);
    }
}

void CRendererWindow::EnsureRowIsVisible(int y)
{
    if (y < 0 || y >= Database.GetRowCount())
    {
        TRACE_E("Wrong row: y=" << y);
        return;
    }
    int newTopIndex = TopIndex;
    if (y < TopIndex)
        newTopIndex = y;
    else
    {
        if (y > TopIndex + RowsOnPage - 1)
        {
            newTopIndex = y - RowsOnPage + 1;
        }
    }
    if (newTopIndex != TopIndex)
    {
        HRGN hUpdateRgn = CreateRectRgn(0, 0, 0, 0);
        RECT r;
        r.left = 0;
        r.top = RowHeight;
        r.right = Width;
        r.bottom = Height;
        ScrollWindowEx(HWindow, 0, RowHeight * (TopIndex - newTopIndex),
                       &r, &r, hUpdateRgn, NULL, 0);
        TopIndex = newTopIndex;
        SetupScrollBars(UPDATE_VERT_SCROLL);
        Paint(NULL, hUpdateRgn, FALSE);
        DeleteObject(hUpdateRgn);
    }
}

BOOL CRendererWindow::HitTest(int x, int y, int* column, int* row, BOOL getNearest)
{
    int cellX = -1;
    int cellY = -1;

    int colX = 0;
    int visibleIndex = 0;
    int i;
    for (i = 0; i < Database.GetColumnCount(); i++)
    {
        const CDatabaseColumn* col = Database.GetColumn(i);
        if (col->Visible)
        {
            if ((x - RowHeight >= colX - XOffset) &&
                (x - RowHeight < colX - XOffset + col->Width))
            {
                cellX = visibleIndex;
                break;
            }
            colX += col->Width;
            visibleIndex++;
        }
    }
    cellY = TopIndex + (y - RowHeight) / RowHeight;

    if (!getNearest)
    {
        if (cellX == -1 || cellY < 0 || cellY >= Database.GetRowCount())
            return FALSE;
    }

    if (cellX == -1)
    {
        if (x < RowHeight)
            cellX = 0;
        else
            cellX = Database.GetVisibleColumnCount() - 1;
    }

    if (cellY < 0)
        cellY = 0;
    if (cellY >= Database.GetRowCount())
        cellY = Database.GetRowCount() - 1;

    *column = cellX;
    *row = cellY;

    return TRUE;
}

BOOL CRendererWindow::HitTestRow(int y, int* row, BOOL getNearest)
{
    int cellY = -1;
    cellY = TopIndex + (y - RowHeight) / RowHeight;

    if (!getNearest)
    {
        if (cellY < 0 || cellY >= Database.GetRowCount())
            return FALSE;
    }

    if (cellY < 0)
        cellY = 0;
    if (cellY >= Database.GetRowCount())
        cellY = Database.GetRowCount() - 1;

    *row = cellY;

    return TRUE;
}

BOOL CRendererWindow::HitTestColumn(int x, int* column, BOOL getNearest)
{
    int cellX = -1;

    int colX = 0;
    int visibleIndex = 0;
    int i;
    for (i = 0; i < Database.GetVisibleColumnCount(); i++)
    {
        const CDatabaseColumn* col = Database.GetVisibleColumn(i);
        if (col->Visible)
        {
            if ((x - RowHeight >= colX - XOffset) &&
                (x - RowHeight < colX - XOffset + col->Width))
            {
                cellX = visibleIndex;
                break;
            }
            colX += col->Width;
            visibleIndex++;
        }
    }

    if (!getNearest)
    {
        if (cellX == -1)
            return FALSE;
    }

    if (cellX == -1)
    {
        if (x < RowHeight)
            cellX = 0;
        else
            cellX = Database.GetVisibleColumnCount() - 1;
    }

    *column = cellX;

    return TRUE;
}

BOOL CRendererWindow::HitTestColumnSplit(int x, int* column, int* offset)
{
    int colX = RowHeight - XOffset;
    int count = Database.GetVisibleColumnCount();
    int i;
    for (i = 0; i <= count; i++)
    {
        if (i > 0 && x >= colX - 3 && x <= colX)
        {
            // pokud se nejedna o levou stranu nulteho sloupce a pod lezi v sodahu predelu, mame ho
            if (column != NULL)
                *column = i - 1;
            if (offset != NULL)
                *offset = x - colX;
            return TRUE;
        }
        if (i < count) // nesmime sahnout mimo pole
            colX += Database.GetVisibleColumn(i)->Width;
    }
    return FALSE;
}

/*
void
CRendererWindow::OnEditCell()
{
  int focusX, focusY;
  Selection.GetFocus(&focusX, &focusY);
  int cellX = 0;
  int colIndex = 0;
  if (!GetColumnInfo(focusX, &colIndex, &cellX))
    return;

  const CDbfColumn *col = Database.GetColumn(colIndex);

  if (!Database.FetchRecord(HWindow, focusY))
    return;

  char buff[66000];
  lstrcpyn(buff, Database.GetRecord() + col->PosInRecord, min(col->FieldSize + 1, 66000));

  cellX -= XOffset;
  int cellY = (focusY - TopIndex) * RowHeight;

  cellX += RowHeight;
  cellY += RowHeight;
  
  CWindow *EditWindow = new CWindow(ooAllocated);
  EditWindow->CreateEx(0,
                       "edit",
                       "",
                       WS_CHILD | ES_READONLY,
                       0, 0, 0, 0,
                       HWindow,
                       NULL,
                       DLLInstance,
                       EditWindow);
  SendMessage(EditWindow->HWindow, WM_SETFONT, (WPARAM)HFont, TRUE);
  SetWindowText(EditWindow->HWindow, buff);
  SendMessage(EditWindow->HWindow, EM_SETMARGINS, EC_LEFTMARGIN, 2);
  SendMessage(EditWindow->HWindow, EM_SETMARGINS, EC_LEFTMARGIN, 2);
  SetWindowPos(EditWindow->HWindow, NULL, cellX + 1, cellY + 1,
               col->Width - 3, RowHeight - 3,
               SWP_NOZORDER | SWP_SHOWWINDOW);
  InvalidateRect(EditWindow->HWindow, NULL, TRUE);
  SetFocus(EditWindow->HWindow);
  UpdateWindow(EditWindow->HWindow);
}
*/

void CRendererWindow::BeginSelectionDrag(CDragSelectionMode mode)
{
    if (DragMode != dsmNone)
    {
        TRACE_E("DragMode != dsmNone");
        return;
    }
    DragMode = mode;
    SetCapture(HWindow);
    ScrollTimerID = SetTimer(HWindow, TIMER_SCROLL_ID, 150, NULL);
}

void CRendererWindow::EndSelectionDrag()
{
    DragMode = dsmNone;
    if (GetCapture() == HWindow)
        ReleaseCapture();
    if (ScrollTimerID != 0)
    {
        KillTimer(HWindow, ScrollTimerID);
        ScrollTimerID = 0;
    }
}

void CRendererWindow::EndColumnDrag()
{
    DragColumn = -1;
    if (GetCapture() == HWindow)
        ReleaseCapture();
}

void CRendererWindow::OnTimer(WPARAM wParam)
{
    if (ScrollTimerID != 0 && wParam == ScrollTimerID)
    {
        DWORD msgPos = GetMessagePos();
        POINT p;
        p.x = GET_X_LPARAM(msgPos);
        p.y = GET_Y_LPARAM(msgPos);
        ScreenToClient(HWindow, &p);
        int xDelta = 0;
        int yDelta = 0;

        if (DragMode != dsmRows)
        {
            if (p.x < RowHeight)
                xDelta = -(RowHeight - p.x);

            if (p.x > Width)
                xDelta = (p.x - Width);
        }

        if (DragMode != dsmColumns)
        {
            if (p.y < RowHeight)
                yDelta = -(RowHeight - p.y) * 2;

            if (p.y > Height)
                yDelta = (p.y - Height) * 2;
        }

        if (xDelta != 0)
        {

            int mX;
            mX = xDelta;
            int nPos = XOffset + mX;
            OnHScroll(SB_THUMBPOSITION, nPos);
        }

        if (yDelta != 0)
        {
            int mY;
            mY = yDelta / 10;
            if (mY == 0)
                mY = yDelta < 0 ? -1 : 1;

            int nPos = TopIndex + mY;
            OnVScroll(SB_THUMBPOSITION, nPos);
        }

        if (xDelta != 0 || yDelta != 0)
            PostMessage(HWindow, WM_MOUSEMOVE, MK_LBUTTON | MK_RBUTTON, MAKEWPARAM(p.x, p.y));
    }
}

void CRendererWindow::OnVScroll(int scrollCode, int pos)
{
    int newTopIndex = TopIndex;
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
        int delta = RowsOnPage - 1;
        if (delta <= 0)
            delta = 1;
        newTopIndex -= delta;
        break;
    }

    case SB_PAGEDOWN:
    {
        int delta = RowsOnPage - 1;
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
    if (newTopIndex >= Database.GetRowCount() - RowsOnPage + 1)
        newTopIndex = Database.GetRowCount() - RowsOnPage;
    if (newTopIndex < 0)
        newTopIndex = 0;
    if (newTopIndex != TopIndex)
    {
        HRGN hUpdateRgn = CreateRectRgn(0, 0, 0, 0);
        RECT r;
        r.left = 0;
        r.top = RowHeight;
        r.right = Width;
        r.bottom = Height;
        ScrollWindowEx(HWindow, 0, RowHeight * (TopIndex - newTopIndex),
                       &r, &r, hUpdateRgn, NULL, 0);
        TopIndex = newTopIndex;
        Paint(NULL, hUpdateRgn, FALSE);
        DeleteObject(hUpdateRgn);
    }
    if (scrollCode != SB_THUMBTRACK)
        SetupScrollBars(UPDATE_VERT_SCROLL);
}

void CRendererWindow::OnHScroll(int scrollCode, int pos)
{
    int newXOffset = XOffset;
    switch (scrollCode)
    {
    case SB_LINEUP:
    {
        newXOffset -= RowHeight;
        break;
    }

    case SB_LINEDOWN:
    {
        newXOffset += RowHeight;
        break;
    }

    case SB_PAGEUP:
    {
        newXOffset -= Width - RowHeight;
        break;
    }

    case SB_PAGEDOWN:
    {
        newXOffset += Width - RowHeight;
        break;
    }

    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
    {
        newXOffset = pos;
        break;
    }
    }
    int columnsWidth = Database.GetVisibleColumnsWidth();
    if (newXOffset >= columnsWidth - (Width - RowHeight) + 1)
        newXOffset = columnsWidth - (Width - RowHeight);
    if (newXOffset < 0)
        newXOffset = 0;
    if (newXOffset != XOffset)
    {
        HRGN hUpdateRgn = CreateRectRgn(0, 0, 0, 0);
        RECT r;
        r.left = RowHeight;
        r.top = 0;
        r.right = Width;
        r.bottom = Height;
        ScrollWindowEx(HWindow, XOffset - newXOffset, 0,
                       &r, &r, hUpdateRgn, NULL, 0);
        XOffset = newXOffset;
        Paint(NULL, hUpdateRgn, FALSE);
        DeleteObject(hUpdateRgn);
    }
    if (scrollCode != SB_THUMBTRACK)
        SetupScrollBars(UPDATE_HORZ_SCROLL);
}

void CRendererWindow::CopySelectionToClipboard()
{
    // napocitame velikost pameti potrebne pro ulozeni dat
    DWORD size = 0;
    RECT r;
    BOOL bUnicode = Database.GetIsUnicode();
    Selection.GetNormalizedSelection(&r);

    int i;
    for (i = r.top; i <= r.bottom; i++)
    {
        if (!Database.FetchRecord(HWindow, i))
            return;
        int j;
        for (j = r.left; j <= r.right; j++)
        {
            const CDatabaseColumn* column = Database.GetVisibleColumn(j);
            size_t textLen;
            if (!bUnicode)
                Database.GetCellText(column, &textLen);
            else
                Database.GetCellTextW(column, &textLen);
            size += (DWORD)textLen;
            if (j < r.right)
                size++; // separator
        }
        if (i < r.bottom)
            size += 2; // eol
    }

    // naalokujeme potrebny prostor
    char* buff = (char*)malloc(size * (!bUnicode ? 1 : 2));
    if (buff == NULL)
    {
        SalGeneral->SalMessageBox(HWindow, LoadStr(IDS_DBFE_OOM),
                                  LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    // naladujeme do nej data
    char* iter = buff;
    LPWSTR iterW = (LPWSTR)buff;
    int k;
    for (k = r.top; k <= r.bottom; k++)
    {
        if (!Database.FetchRecord(HWindow, k))
        {
            free(buff);
            return;
        }
        int j;
        for (j = r.left; j <= r.right; j++)
        {
            const CDatabaseColumn* column = Database.GetVisibleColumn(j);
            size_t textLen;
            if (!bUnicode)
            {
                const char* text = Database.GetCellText(column, &textLen);
                memcpy(iter, text, textLen);
                CodeCharacters(iter, textLen);
                iter += textLen;
                if (j < r.right)
                {
                    *iter++ = '\t'; // separator
                }
            }
            else
            {
                LPCWSTR textW = Database.GetCellTextW(column, &textLen);
                memcpy(iterW, textW, textLen * sizeof(WCHAR));
                iterW += textLen;
                if (j < r.right)
                {
                    *iterW++ = '\t'; // separator
                }
            }
        }
        if (k < r.bottom)
        {
            if (!bUnicode)
            {
                *iter++ = '\r'; // eol
                *iter++ = '\n';
            }
            else
            {
                *iterW++ = '\r'; // eol
                *iterW++ = '\n';
            }
        }
    }

    if (!bUnicode)
        SalGeneral->CopyTextToClipboard(buff, size, FALSE, HWindow);
    else
        SalGeneral->CopyTextToClipboardW((LPWSTR)buff, size, FALSE, HWindow);
    free(buff);
} /* CRendererWindow::CopySelectionToClipboard */

void CRendererWindow::SelectAll()
{
    OldSelection = Selection;
    Selection.SetFocus(0, 0);
    Viewer->UpdateRowNumberOnToolBar(0, Database.GetRowCount());
    Selection.SetAnchor(max(0, Database.GetVisibleColumnCount() - 1),
                        max(0, Database.GetRowCount() - 1));
    Paint(NULL, NULL, TRUE); // prekreslime zmeny
}

void CRendererWindow::CheckAndCorrectBoundaries()
{
    if (!Database.IsOpened())
        return;
    if (Width - RowHeight > 0 && Height - RowHeight > 0)
    {
        // zajistim odrolovani v pripade, ze se zvetsilo okno, vpravo nebo dole jsme na dorazu a jeste muzeme rolovat
        int rowWidth = Database.GetVisibleColumnsWidth();
        int newXOffset = XOffset;
        if (newXOffset > 0 && rowWidth - newXOffset < (Width - RowHeight) + 1)
            newXOffset = rowWidth - (Width - RowHeight);
        if (rowWidth < (Width - RowHeight) + 1)
            newXOffset = 0;

        int rowCount = Database.GetRowCount();
        int newTopIndex = TopIndex;
        if (newTopIndex > 0 && rowCount - newTopIndex < RowsOnPage + 1)
            newTopIndex = rowCount - RowsOnPage;
        if (rowCount < RowsOnPage + 1)
            newTopIndex = 0;

        if (newXOffset != XOffset || newTopIndex != TopIndex)
        {
            XOffset = newXOffset;
            TopIndex = newTopIndex;
            Paint(NULL, NULL, FALSE);
        }
    }
}

void CRendererWindow::ColumnsWasChanged()
{
    // ohlidame selection
    int clipX = max(0, Database.GetVisibleColumnCount() - 1);
    Selection.Clip(clipX);
    OldSelection.Clip(clipX);
    Bookmarks.ClearAll();
    InvalidateRect(HWindow, NULL, TRUE);
    SetupScrollBars(UPDATE_HORZ_SCROLL);
    CheckAndCorrectBoundaries();
}

void CRendererWindow::GetContextMenuPos(POINT* p)
{
    int x, y;
    Selection.GetFocus(&x, &y);

    int colX = RowHeight - XOffset;
    int i;
    for (i = 0; i <= min(x, Database.GetVisibleColumnCount() - 1); i++)
        colX += Database.GetVisibleColumn(i)->Width;

    p->x = colX - 1;
    p->y = RowHeight + (y - TopIndex) * RowHeight + RowHeight - 1;
    if (p->x < RowHeight)
        p->x = RowHeight;
    if (p->y < RowHeight)
        p->y = RowHeight;
    if (p->x > Width)
        p->x = Width;
    if (p->y > Height)
        p->y = Height;
    ClientToScreen(HWindow, p);
}

void CRendererWindow::OnContextMenu(const POINT* p)
{
    CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
    if (popup != NULL)
    {
        popup->LoadFromTemplate(HLanguage, PopupMenuTemplate, Viewer->Enablers, Viewer->HGrayToolBarImageList, Viewer->HHotToolBarImageList);
        popup->SetStyle(MENU_POPUP_UPDATESTATES);
        popup->Track(MENU_TRACK_RIGHTBUTTON, p->x, p->y, Viewer->HWindow, NULL);
        SalamanderGUI->DestroyMenuPopup(popup);
    }
}

void CRendererWindow::ResetMouseWheelAccumulatorHandler(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        // pokud je SHIFT stisteny, chodi autorepeat, ale nas zajima jen ten prvni stisk
        BOOL firstPress = (lParam & 0x40000000) == 0;
        if (!firstPress)
            break;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_NCLBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
        ResetMouseWheelAccumulator();
        break;
    }
    }
}

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif // WM_MOUSEHWHEEL

LRESULT
CRendererWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ResetMouseWheelAccumulatorHandler(uMsg, wParam, lParam);

    if (uMsg == WM_MOUSEWHEEL)
    {
        short zDelta = (short)HIWORD(wParam);
        if ((zDelta < 0 && MouseWheelAccumulator > 0) || (zDelta > 0 && MouseWheelAccumulator < 0))
            ResetMouseWheelAccumulator(); // pri zmene smeru otaceni kolecka je potreba nulovat akumulator

        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        // standardni scrolovani bez modifikacnich klaves
        if (!controlPressed && !altPressed && !shiftPressed)
        {
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
            GetScrollInfo(HWindow, SB_VERT, &si);

            DWORD wheelScroll = SalGeneral->GetMouseWheelScrollLines(); // muze byt az WHEEL_PAGESCROLL(0xffffffff)
            wheelScroll = max(1, min(wheelScroll, si.nPage - 1));       // omezime maximalne na delku stranky

            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int linesToScroll = MouseWheelAccumulator / stepsPerLine;
            if (linesToScroll != 0)
            {
                MouseWheelAccumulator -= linesToScroll * stepsPerLine;
                OnVScroll(SB_THUMBPOSITION, si.nPos - linesToScroll);
            }
        }

        // SHIFT: horizontalni rolovani
        if (!controlPressed && !altPressed && shiftPressed)
        {
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
            GetScrollInfo(HWindow, SB_HORZ, &si);

            DWORD wheelScroll = RowHeight * SalGeneral->GetMouseWheelScrollLines(); // 'delta' muze byt az WHEEL_PAGESCROLL(0xffffffff)
            wheelScroll = max(1, min(wheelScroll, si.nPage));                       // omezime maximalne na sirku stranky

            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int linesToScroll = MouseWheelAccumulator / stepsPerLine;
            if (linesToScroll != 0)
            {
                MouseWheelAccumulator -= linesToScroll * stepsPerLine;
                OnHScroll(SB_THUMBPOSITION, si.nPos - linesToScroll);
            }
        }

        return 0;
    }

    if (uMsg == WM_MOUSEHWHEEL) // horizontall scroll, supported from Windows Vista
    {
        short zDelta = (short)HIWORD(wParam);
        if ((zDelta < 0 && MouseHWheelAccumulator > 0) || (zDelta > 0 && MouseHWheelAccumulator < 0))
            ResetMouseWheelAccumulator(); // pri zmene smeru naklapeni kolecka je potreba nulovat akumulator

        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
        GetScrollInfo(HWindow, SB_HORZ, &si);

        DWORD wheelScroll = RowHeight * SalGeneral->GetMouseWheelScrollChars();
        wheelScroll = max(1, min(wheelScroll, si.nPage - 1)); // omezime maximalne na delku stranky

        MouseHWheelAccumulator += 1000 * zDelta;
        int stepsPerChar = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
        int charsToScroll = MouseHWheelAccumulator / stepsPerChar;
        if (charsToScroll != 0)
        {
            MouseHWheelAccumulator -= charsToScroll * stepsPerChar;
            OnHScroll(SB_THUMBPOSITION, si.nPos + charsToScroll);
        }
        return TRUE; // udalost jsme zpracovali a nemame zajem o emulaci pomoci klikani na scrollbar (stane se v pripade vraceni FALSE)
    }

    switch (uMsg)
    {
    case WM_CREATE:
    {
        DragAcceptFiles(HWindow, TRUE);
        break;
    }

    case WM_DESTROY:
    {
        DragAcceptFiles(HWindow, FALSE);
        break;
    }

    case WM_DROPFILES:
    {
        UINT drag;
        char path[MAX_PATH];

        drag = DragQueryFile((HDROP)wParam, 0xFFFFFFFF, NULL, 0); // kolik souboru nam hodili
        if (drag > 0)
        {
            DragQueryFile((HDROP)wParam, 0, path, MAX_PATH);
            OpenFile(path, TRUE);
        }
        DragFinish((HDROP)wParam);
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(HWindow, &ps);
        if (hDC != NULL)
            Paint(hDC, NULL, FALSE);
        EndPaint(HWindow, &ps);
        return 0;
    }

    case WM_SIZE:
    {
        RECT r;
        GetClientRect(HWindow, &r);
        Width = r.right;
        Height = r.bottom;

        RowsOnPage = (Height - RowHeight) / RowHeight; // odecteme headeline
        if (RowsOnPage < 1)
            RowsOnPage = 1;

        SetupScrollBars();
        CheckAndCorrectBoundaries();

        break;
    }

    case WM_ERASEBKGND:
    {
        if (Creating || Database.IsOpened())
            return 1;
        else
            break;
    }

    case WM_VSCROLL:
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_TRACKPOS;
        GetScrollInfo(HWindow, SB_VERT, &si);
        int pos = si.nTrackPos;
        int scrollCode = (int)LOWORD(wParam);
        OnVScroll(scrollCode, pos);
        break;
    }

    case WM_HSCROLL:
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_TRACKPOS;
        GetScrollInfo(HWindow, SB_HORZ, &si);
        int pos = si.nTrackPos;
        int scrollCode = (int)LOWORD(wParam);
        OnHScroll(scrollCode, pos);
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        if (DragMode != dsmNone || DragColumn != -1)
            break;

        if (wParam == VK_ESCAPE)
        {
            PostMessage(GetParent(HWindow), uMsg, wParam, lParam);
            break;
        }

        if (!Database.IsOpened())
            break;

        if (Database.GetVisibleColumnCount() == 0 || Database.GetRowCount() == 0)
            break;

        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL resetSelection = FALSE;

        if ((wParam == VK_F10 && shiftPressed || wParam == VK_APPS))
        {
            POINT p;
            GetContextMenuPos(&p);
            OnContextMenu(&p);
            break;
        }

        int x, y;
        if (shiftPressed)
            Selection.GetAnchor(&x, &y);
        else
            Selection.GetFocus(&x, &y);
        int oldX = x;
        int oldY = y;
        int topIndex = TopIndex;
        switch (wParam)
        {
        case VK_RIGHT:
        {
            if (x < Database.GetVisibleColumnCount() - 1)
                x++;
            if (!shiftPressed)
                resetSelection = TRUE;
            break;
        }

        case VK_LEFT:
        {
            if (x > 0)
                x--;
            if (!shiftPressed)
                resetSelection = TRUE;
            break;
        }

        case VK_UP:
        {
            if (y > 0)
                y--;
            if (!shiftPressed)
                resetSelection = TRUE;
            break;
        }

        case VK_DOWN:
        {
            if (y < Database.GetRowCount() - 1)
                y++;
            if (!shiftPressed)
                resetSelection = TRUE;
            break;
        }

        case VK_PRIOR:
        {
            if (controlPressed)
                y = 0;
            else
            {
                topIndex -= RowsOnPage;
                if (topIndex < 0)
                    topIndex = 0;
                /*          if (shiftPressed)
          {*/
                y -= RowsOnPage;
                if (y < 0)
                    y = 0;
                /*          }
          else
          {
            if (y >= RowsOnPage)
              y -= RowsOnPage;
          }*/
            }
            if (!shiftPressed)
                resetSelection = TRUE;
            break;
        }

        case VK_NEXT:
        {
            if (controlPressed)
                y = Database.GetRowCount() - 1;
            else
            {
                topIndex += RowsOnPage;
                if (topIndex > Database.GetRowCount() - RowsOnPage)
                    topIndex = Database.GetRowCount() - RowsOnPage;
                if (topIndex < 0)
                    topIndex = 0;
                /*          if (shiftPressed)
          {*/
                y += RowsOnPage;
                if (y >= Database.GetRowCount())
                    //            if (y > Database.GetRowCount() - RowsOnPage)
                    y = Database.GetRowCount() - 1;
                if (y < 0)
                    y = 0;
                /*          }
          else
          {
            if (y < Database.GetRowCount() - RowsOnPage)
              y += RowsOnPage;
          }*/
            }
            if (!shiftPressed)
                resetSelection = TRUE;
            break;
        }

        case VK_HOME:
        {
            if (controlPressed)
                y = 0;
            else
                x = 0;
            break;
        }

        case VK_END:
        {
            if (controlPressed)
                y = Database.GetRowCount() - 1;
            else
                x = Database.GetVisibleColumnCount() - 1;
            break;
        }
        }
        if (x != oldX || y != oldY || topIndex != TopIndex ||
            (resetSelection && !Selection.FocusIsAnchor()))
        {
            OldSelection = Selection;
            if (shiftPressed)
                Selection.SetAnchor(x, y);
            else
            {
                Selection.SetFocusAndAnchor(x, y);
                Viewer->UpdateRowNumberOnToolBar(y, Database.GetRowCount());
            }
            BOOL selOnly = topIndex == TopIndex;
            TopIndex = topIndex;
            Paint(NULL, NULL, selOnly); // prekreslime zmeny
            if (y < Database.GetRowCount())
                EnsureRowIsVisible(y);
            EnsureColumnIsVisible(x);
            SetupScrollBars(UPDATE_VERT_SCROLL | UPDATE_HORZ_SCROLL);
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        if (!Database.IsOpened())
            break;

        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);

        if (xPos > RowHeight && yPos < RowHeight)
        {
            // nejde o zacatek tazeni sirky sloupce?
            int colIndex;
            int offset;
            if (HitTestColumnSplit(xPos, &colIndex, &offset))
            {
                DragColumn = colIndex;
                DragColumnOffset = offset;
                SetCapture(HWindow);
                break;
            }
        }

        if (xPos < RowHeight && yPos < RowHeight)
        {
            // select all
            SelectAll();
            break;
        }

        if (xPos < RowHeight)
        {
            // select pres radky
            int y;
            if (HitTestRow(yPos, &y, FALSE))
            {
                OldSelection = Selection;
                Selection.SetFocus(0, y);
                Viewer->UpdateRowNumberOnToolBar(y, Database.GetRowCount());
                Selection.SetAnchor(Database.GetVisibleColumnCount() - 1, y);
                Paint(NULL, NULL, TRUE); // prekreslime zmeny
                BeginSelectionDrag(dsmRows);
            }
            break;
        }

        if (yPos < RowHeight)
        {
            // select pres sloupce
            int x;
            if (HitTestColumn(xPos, &x, FALSE))
            {
                OldSelection = Selection;
                Selection.SetFocus(x, 0);
                Viewer->UpdateRowNumberOnToolBar(0, Database.GetRowCount());
                Selection.SetAnchor(x, Database.GetRowCount() - 1);
                Paint(NULL, NULL, TRUE); // prekreslime zmeny
                BeginSelectionDrag(dsmColumns);
            }
            break;
        }

        if (xPos >= RowHeight && yPos >= RowHeight)
        {
            // select pres bunky
            int x, y;
            if (HitTest(xPos, yPos, &x, &y, FALSE))
            {
                OldSelection = Selection;
                Selection.SetFocusAndAnchor(x, y);
                Viewer->UpdateRowNumberOnToolBar(y, Database.GetRowCount());
                Paint(NULL, NULL, TRUE); // prekreslime zmeny
                BeginSelectionDrag(dsmNormal);
            }
            break;
        }
        break;
    }

    case WM_RBUTTONDOWN:
    {
        if (!Database.IsOpened())
            break;

        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);

        if (xPos >= RowHeight && yPos >= RowHeight)
        {
            // select pres bunky
            int x, y;
            if (HitTest(xPos, yPos, &x, &y, FALSE))
            {
                if (!Selection.Contains(x, y))
                {
                    OldSelection = Selection;
                    Selection.SetFocusAndAnchor(x, y);
                    Viewer->UpdateRowNumberOnToolBar(y, Database.GetRowCount());
                    Paint(NULL, NULL, TRUE); // prekreslime zmeny
                }
                POINT p;
                GetCursorPos(&p);
                OnContextMenu(&p);
            }
            break;
        }
        break;
    }

    case WM_LBUTTONUP:
    {
        if (DragColumn != -1)
        {
            CheckAndCorrectBoundaries();
            SetupScrollBars(UPDATE_HORZ_SCROLL);
            EndColumnDrag();
        }
        if (DragMode != dsmNone)
            EndSelectionDrag();
        break;
    }

    case WM_CAPTURECHANGED:
    {
        if (DragColumn != -1)
        {
            CheckAndCorrectBoundaries();
            SetupScrollBars(UPDATE_HORZ_SCROLL);
            EndColumnDrag();
        }
        if (DragMode != dsmNone)
            EndSelectionDrag();
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (!Database.IsOpened())
            break;
        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);
        if (DragColumn != -1)
        {
            const CDatabaseColumn* column = Database.GetVisibleColumn(DragColumn);
            int x = Database.GetVisibleColumnX(DragColumn);
            if (column != NULL && x != -1)
            {
                int newWidth = xPos + XOffset - RowHeight - x - DragColumnOffset;
                if (newWidth < 10)
                    newWidth = 10;
                if (newWidth != column->Width)
                {
                    CDatabaseColumn col = *column;
                    col.Width = newWidth;
                    Database.SetVisibleColumn(DragColumn, &col);
                    // presunuto az na ukonceni tazeni, aby nedochazelo k rychlym
                    // zmenam rozmeru sloupce pri zmensovani s odrolovanim vpravo
                    //            CheckAndCorrectBoundaries();
                    //            SetupScrollBars(UPDATE_HORZ_SCROLL);
                    Paint(NULL, NULL, FALSE); // trochu si zablikame
                }
            }
        }
        if (DragMode == dsmNormal)
        {
            int x, y;
            if (HitTest(xPos, yPos, &x, &y, TRUE))
            {
                OldSelection = Selection;
                Selection.SetAnchor(x, y);
                Paint(NULL, NULL, TRUE); // prekreslime zmeny
            }
        }
        if (DragMode == dsmColumns)
        {
            int x;
            if (HitTestColumn(xPos, &x, TRUE))
            {
                OldSelection = Selection;
                Selection.SetAnchor(x, Database.GetRowCount() - 1);
                Paint(NULL, NULL, TRUE); // prekreslime zmeny
            }
        }
        if (DragMode == dsmRows)
        {
            int y;
            if (HitTestRow(yPos, &y, TRUE))
            {
                OldSelection = Selection;
                Selection.SetAnchor(Database.GetVisibleColumnCount() - 1, y);
                Paint(NULL, NULL, TRUE); // prekreslime zmeny
            }
        }

        break;
    }

    case WM_TIMER:
    {
        OnTimer(wParam);
        return 0;
    }

    case WM_SETCURSOR:
    {
        if (!Database.IsOpened())
            break;
        if (LOWORD(lParam) == HTCLIENT)
        {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(HWindow, &p);

            if (p.x - RowHeight >= Database.GetVisibleColumnsWidth() - XOffset ||
                p.y - RowHeight >= RowHeight * (Database.GetRowCount() - TopIndex))
            {
                // pokud je kurzor mimo validni obdelnik dat, dame sipku
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            }
            else
            {
                // pokud je v datech, rozlisime moznost, ze je nad oddelovacem sloupcu v header line
                BOOL hitSplit = FALSE;
                if (p.y < RowHeight && p.x > RowHeight)
                {
                    if (HitTestColumnSplit(p.x, NULL, NULL))
                        hitSplit = TRUE;
                }

                if (hitSplit)
                    SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_SPLIT)));
                else
                    SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_SELECT)));
            }
            return TRUE;
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
