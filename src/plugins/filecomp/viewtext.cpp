// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#undef min
#undef max

using namespace std;

// ****************************************************************************
//
// TTextFileViewWindow<CChar>
//

template <class CChar>
TTextFileViewWindow<CChar>::TTextFileViewWindow(CFileViewID id, BOOL showWhiteSpace, CMainWindow* mainWindow) : CTextFileViewWindowBase(id, showWhiteSpace, mainWindow)
{
    Text = NULL;
    LineBuffer = NULL;
}

template <class CChar>
void TTextFileViewWindow<CChar>::DestroyData()
{
    if (LineBuffer)
        free(LineBuffer);
    if (Text)
        free(Text);
    Text = NULL;
    LineBuffer = NULL;
    Lines.clear();
    super::DestroyData();
}

template <class CChar>
int TTextFileViewWindow<CChar>::GetLines()
{
    return int(Lines.size()) - 1;
}

template <class CChar>
void TTextFileViewWindow<CChar>::ReloadConfiguration(DWORD flags, BOOL updateWindow)
{
    CALL_STACK_MESSAGE3("TTextFileViewWindow<CChar>::ReloadConfiguration(0x%X, %d)",
                        flags, updateWindow);

    CTextFileViewWindowBase::ReloadConfiguration(flags, FALSE);

    WhiteSpaceChar = TCharSpecific<CChar>::ConvertANSI8Char(Configuration.WhiteSpace);

    if (flags & CC_FONT)
    {
        MappedTextOut.FontHasChanged(&Configuration.FileViewLogFont, HFont, FontWidth, FontHeight);
    }

    if (updateWindow)
    {
        InvalidateRect(HWindow, NULL, FALSE);
        UpdateWindow(HWindow);
    }
}

template <class CChar>
int TTextFileViewWindow<CChar>::MeasureLine(int line, int length)
{
    CALL_STACK_MESSAGE_NONE
    const CChar* start = Lines[line];
    const CChar* end = length < 0 ? Lines[line + 1] - 1 : start + length;
    int tabChars = 0;
    const CChar* iterator = start;
    while (iterator < end)
    {
        if (*iterator == '\t')
            tabChars += TabSize - (int(iterator - start) + tabChars) % TabSize - 1;
        iterator++;
    }
    return int(iterator - start) + tabChars;
}

template <class CChar>
int TTextFileViewWindow<CChar>::FormatLine(HDC hDC, CChar* buffer, const CChar* start, const CChar* end)
{
    SLOW_CALL_STACK_MESSAGE1("FormatLine( , , , )");
    CChar* dest = buffer;
    int i;
    if (MappedTextOut.NeedMapping())
    {
        if (sizeof(CChar) > 1)
            MappedTextOut.CalcMappingIfNeeded(hDC, start, (int)(end - start));
        if (ShowWhiteSpace)
        {
            while (start < end)
            {
                if (*start == '\t')
                {
                    i = TabSize - int(dest - buffer) % TabSize - 1;
                    *dest++ = MappedTextOut.MapChar(WhiteSpaceChar);
                    while (i--)
                    {
                        *dest++ = MappedTextOut.MapChar(' ');
                    }
                    start++;
                    continue;
                }
                if (IsSpaceX(*start))
                {
                    *dest++ = MappedTextOut.MapChar(WhiteSpaceChar);
                    start++;
                    continue;
                }
                *dest++ = MappedTextOut.MapChar(*start++);
            }
        }
        else
        {
            while (start < end)
            {
                if (*start == '\t')
                {
                    i = TabSize - int(dest - buffer) % TabSize;
                    while (i--)
                    {
                        *dest++ = MappedTextOut.MapChar(' ');
                    }
                    start++;
                    continue;
                }
                *dest++ = MappedTextOut.MapChar(*start++);
            }
        }
    }
    else
    {
        if (ShowWhiteSpace)
        {
            while (start < end)
            {
                if (*start == '\t')
                {
                    i = TabSize - int(dest - buffer) % TabSize - 1;
                    *dest++ = WhiteSpaceChar;
                    while (i--)
                    {
                        *dest++ = ' ';
                    }
                    start++;
                    continue;
                }
                if (IsSpaceX(*start))
                {
                    *dest++ = WhiteSpaceChar;
                    start++;
                    continue;
                }
                *dest++ = *start++;
            }
        }
        else
        {
            while (start < end)
            {
                if (*start == '\t')
                {
                    i = TabSize - int(dest - buffer) % TabSize;
                    while (i--)
                    {
                        *dest++ = ' ';
                    }
                    start++;
                    continue;
                }
                *dest++ = *start++;
            }
        }
    }
    return int(dest - buffer);
}

template <class CChar>
void TTextFileViewWindow<CChar>::SetData(CChar* text, CLineBuffer& lines, CLineScript& script)
{
    CALL_STACK_MESSAGE1("CTextFileViewWindowBase::SetData(, , )");

    DestroyData();
    Text = text;
    Lines.swap(lines);
    Script[0].swap(script); // TODO nemelo by se tohle presunout do parenta?
    // resize POLYTEXT buffers
    int maxc = 0;
    for (CLineScript::iterator ls = Script[0].begin(); ls < Script[0].end(); ++ls)
        if (ls->GetChanges())
            maxc = max(ls->GetChanges()[0], maxc);
    PTCommon.resize(maxc / 2 + 1);
    PTChange.resize(maxc / 2);
    PTZeroChange.resize(maxc / 2);
    LineChanges.resize(maxc);
}

template <class CChar>
BOOL TTextFileViewWindow<CChar>::ReallocLineBuffer()
{
    // NOTE: ANSI: realloc(NULL, 0) returns non-NULL, but realloc(non-NULL, 0) returns NULL!
    void* ptr = realloc(LineBuffer, max(MaxWidth, 1) * sizeof(CChar));
    if (!ptr)
        return Error(GetParent(HWindow), IDS_LOWMEM);
    LineBuffer = (CChar*)ptr;
    return TRUE;
}

template <class CChar>
int TTextFileViewWindow<CChar>::TransformXOrg(int x, int line)
{
    CALL_STACK_MESSAGE3("CTextFileViewWindowBase::TransformXOrg(%d, %d)", x, line);
    x = x - LineNumWidth + FirstVisibleChar * FontWidth;

    int l = Script[ViewMode][line].GetLine();
    const CChar* start = Lines[l];
    const CChar* iterator = start;
    const CChar* end = Lines[l + 1] - 1;
    int j = 0;
    int testX = x + FontWidth / 2;
    int tabChars = 0;

    while (iterator < end)
    {
        if (*iterator == '\t')
        {
            int i = TabSize - (int(iterator - start) + tabChars) % TabSize;
            tabChars += i - 1;
            int testX2 = x + (i * FontWidth) / 2;
            while (i--)
            {
                j += FontWidth;
                if (j >= testX2)
                    return int(iterator - start);
            }
            iterator++;
            continue;
        }
        j += FontWidth;
        if (j >= testX)
            break;
        iterator++;
    }
    return int(iterator - start);
}

template <class CChar>
BOOL TTextFileViewWindow<CChar>::CopySelection()
{
    CALL_STACK_MESSAGE1("CTextFileViewWindowBase::CopySelection()");
    if (!IsSelected())
        return FALSE;

    int lineBegin, lineEnd;
    int charBegin, charEnd;
    int l1, l2;

    lineBegin = SelectionLineBegin;
    while (lineBegin < SelectionLineEnd && Script[ViewMode][lineBegin].IsBlank())
        lineBegin++;

    lineEnd = SelectionLineEnd;
    while (lineEnd > lineBegin && Script[ViewMode][lineEnd].IsBlank())
        lineEnd--;

    if (Script[ViewMode][lineBegin].IsBlank() || Script[ViewMode][lineEnd].IsBlank())
        return FALSE;

    l1 = Script[ViewMode][lineBegin].GetLine();
    l2 = Script[ViewMode][lineEnd].GetLine();

    if (lineBegin == SelectionLineBegin)
        charBegin = SelectionCharBegin;
    else
        charBegin = 0;

    if (lineEnd == SelectionLineEnd)
        charEnd = SelectionCharEnd;
    else
    {
        charEnd = int(Lines[l2 + 1] - Lines[l2]);
        if (size_t(l2 + 1) >= Lines.size())
            charEnd--;
    }

    int len = int((Lines[l2] + charEnd) - (Lines[l1] + charBegin));
    if (len)
    {
        // nahradime LF za CRLF
        const CChar* sour = Lines[l1] + charBegin;
        int s = 0, d = 0, i;
        int size = (int)(1.1 * len);
        CChar* buffer = (CChar*)malloc(size * sizeof(CChar));
        if (!buffer)
            return FALSE;

        while (s < len)
        {
            CChar* ptr = (CChar*)MemChr(sour + s, '\n', len - s);
            i = ptr ? int(ptr - (sour + s)) : len - s;
            if (size < d + i + 2)
            {
                size = max(2 * size, size + d + i + 2);
                ptr = (CChar*)realloc(buffer, size * sizeof(CChar));
                if (!ptr)
                {
                    free(buffer);
                    return FALSE;
                }
                buffer = ptr;
            }
            memcpy(buffer + d, sour + s, i * sizeof(CChar));
            d += i;
            s += i + 1; // preskocime LF
            if (s <= len)
            {
                buffer[d++] = '\r';
                buffer[d++] = '\n';
            }
        }

        BOOL ret = CopyTextToClipboard(buffer, d, FALSE, NULL);
        free(buffer);
        return ret;
    }
    else
        return FALSE;
}

template <class CChar>
void TTextFileViewWindow<CChar>::MoveCaret(int yPos)
{
    CALL_STACK_MESSAGE2("CTextFileViewWindowBase::MoveCaret(%d)", yPos);
    if (!DataValid)
        return;

    if (yPos < 0)
        yPos = 0;
    if (size_t(yPos) >= int(Script[ViewMode].size()))
        yPos = int(Script[ViewMode].size()) - 1;

    int xPos;
    if (Script[ViewMode].empty() || Script[ViewMode][yPos].IsBlank() || Script[ViewMode][CaretYPos].IsBlank())
        xPos = 0;
    else
    {
        int l = Script[ViewMode][CaretYPos].GetLine();
        int x = MeasureLine(l, CaretXPos);

        l = Script[ViewMode][yPos].GetLine();
        const CChar* start = Lines[l];
        const CChar* iterator = start;
        const CChar* end = Lines[l + 1] - 1;
        int tabChars = 0;

        while (iterator < end)
        {
            if (*iterator == '\t')
            {
                int i = TabSize - (int(iterator - start) + tabChars) % TabSize;
                tabChars += i - 1;
            }
            if (iterator - start + tabChars >= x)
                break;
            iterator++;
        }
        xPos = int(iterator - start);
    }

    SetCaretPos(xPos, yPos);
}

template <class CChar>
int TTextFileViewWindow<CChar>::GetLeftmostVisibleChar(int line)
{
    CALL_STACK_MESSAGE2("CTextFileViewWindowBase::GetLeftmostVisibleChar(%d)", line);
    if (Script[ViewMode][line].IsBlank())
        return 0;

    int l = Script[ViewMode][line].GetLine();
    const CChar* start = Lines[l];
    const CChar* iterator = start;
    const CChar* end = Lines[l + 1] - 1;
    int tabChars = 0;

    while (iterator < end && iterator - start + tabChars < FirstVisibleChar)
    {
        if (*iterator == '\t')
        {
            int i = TabSize - (int(iterator - start) + tabChars) % TabSize;
            tabChars += i - 1;
        }
        iterator++;
    }
    return int(iterator - start);
}

template <class CChar>
int TTextFileViewWindow<CChar>::GetRightmostVisibleChar(int line)
{
    CALL_STACK_MESSAGE2("CTextFileViewWindowBase::GetRightmostVisibleChar(%d)", line);
    if (Script[ViewMode][line].IsBlank())
        return 0;

    int l = Script[ViewMode][line].GetLine();
    const CChar* start = Lines[l];
    const CChar* iterator = start;
    const CChar* end = Lines[l + 1] - 1;
    int tabChars = 0;

    while (iterator < end && iterator - start + tabChars < FirstVisibleChar + (Width - LineNumWidth) / FontWidth)
    {
        if (*iterator == '\t')
        {
            int i = TabSize - (int(iterator - start) + tabChars) % TabSize;
            tabChars += i - 1;
        }
        iterator++;
    }
    if (iterator != start)
        iterator--;
    return int(iterator - start);
}

template <class CChar>
int TTextFileViewWindow<CChar>::LineLength(int line)
{
    return int(Lines[line + 1] - Lines[line]) - 1;
}

template <class CChar>
void TTextFileViewWindow<CChar>::SelectWord(int line, int& start, int& end)
{
    int len = int(Lines[line + 1] - Lines[line]) - 1;
    if (start <= len)
    {
        while (start > 0 && IsWordX(Lines[line][start - 1]))
            start--;
        while (end < len && IsWordX(Lines[line][end]))
            end++;
    }
}

template <class CChar>
int TTextFileViewWindow<CChar>::GetCharCat(CChar c)
{
    CALL_STACK_MESSAGE_NONE
    if (IsWordX(c))
        return 0;
    if (IsSpaceX(c))
        return 1;
    return 2;
}

template <class CChar>
void TTextFileViewWindow<CChar>::NextCharBlock(int line, int& xpos)
{
    int len = int(Lines[line + 1] - Lines[line]) - 1;
    // presuneme se na konec slova/mezery/bloku jinych znaku
    int cat = GetCharCat(Lines[line][xpos]);

    while (xpos < len && GetCharCat(Lines[line][xpos]) == cat)
        xpos++;
}

template <class CChar>
void TTextFileViewWindow<CChar>::PrevCharBlock(int line, int& xpos)
{
    int cat = GetCharCat(Lines[line][xpos - 1]);

    while (xpos > 0 && GetCharCat(Lines[line][xpos - 1]) == cat)
        xpos--;
}

template <class CChar>
void TTextFileViewWindow<CChar>::TransformOffsets(const int* changes, int size, const CChar* line)
{
    SLOW_CALL_STACK_MESSAGE1("TextFileViewWindow::TransformOffsets( , )");

    int i = 0, j = 0;
    int tabChars = 0;

    while (1)
    {
        while (changes[i] == j)
        {
            LineChanges[i] = changes[i] + tabChars;
            i++;
            if (i == size)
                return;
        }
        if (line[j] == '\t')
        {
            int t = TabSize - (j + tabChars) % TabSize;
            tabChars += t - 1;
        }
        j++;
    }
}

// PolyTextOut has problems with long strings, trim invisible portions of text,
// that would be other-wise clipped and may cause PolyTextOut to fail.
template <class CChar>
void TTextFileViewWindow<CChar>::PolytextLengthFixup(typename std::vector<POLYTEXT>::iterator begin, typename std::vector<POLYTEXT>::iterator end)
{
    for (; begin < end; ++begin)
    {
        if (begin->x < 0)
        {
            int offset = min(-int(begin->x) / int(FontWidth), int(begin->n));
            begin->x += offset * FontWidth;
            begin->lpstr += offset;
            begin->n -= offset;
        }
        // TODO na 4K displayich už 1024 nemusí stačit!!
        begin->n = min(begin->n, UINT(1024));
    }
}

template <class CChar>
void TTextFileViewWindow<CChar>::Paint()
{
    CALL_STACK_MESSAGE1("CTextFileViewWindow::Paint()");
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(HWindow, &ps);
    HPALETTE oldPalette;
    if (UsePalette)
        oldPalette = SelectPalette(dc, Palette, FALSE);
    HFONT oldFont = (HFONT)SelectObject(dc, HFont);

    // urcim interval radku, ktery je treba prekreslit
    RECT clipRect;
    int clipRet = GetClipBox(dc, &clipRect);
    int clipFirstRow = 0;
    int clipLastRow = Height / FontHeight + 1;
    if (clipRet == SIMPLEREGION || clipRet == COMPLEXREGION)
    {
        clipFirstRow = clipRect.top / FontHeight;
        clipLastRow = clipRect.bottom / FontHeight + 1;
    }

    if (size_t(FirstVisibleLine + clipLastRow) >= Script[ViewMode].size())
    {
        // skoncime posledni platnou radkou ve skriptu
        clipLastRow = int(Script[ViewMode].size() - FirstVisibleLine);
    }

    RECT r1; // cislo radky
    r1.left = 0;
    r1.right = LineNumWidth;
    r1.bottom = 0; // abychom meli pozdeji platana data, kdyby kreslici smycka vubec neprobehla
    RECT r2;       // vlastni radek textu
    r2.left = r1.right;
    r2.right = Width;
    //int text_x = r2.left;
    int text_y;
    int line;
    int colorScheme;
    HPEN hOldPen;
    DWORD type;
    int i;

    for (i = clipFirstRow; i < clipLastRow; i++)
    {
        text_y = r1.top = r2.top = i * FontHeight;
        r1.bottom = r2.bottom = r1.top + FontHeight;
        line = FirstVisibleLine + i;

        switch (type = Script[ViewMode][line].GetClass())
        {
        case CLineSpec::lcSeparator:
        {
            colorScheme = LC_NORMAL;

            // vykreslime lajnu oddelujici difference uprostred radky
            // hrana ve sloupci s cisly radek
            SetBkColor(dc, LineColors[colorScheme].LineNumBkColor);
            hOldPen = (HPEN)SelectObject(dc, LineColors[colorScheme].LineNumBorderPen);
            MoveToEx(dc, r1.left, (r1.top + r1.bottom) / 2, NULL);
            LineTo(dc, r1.right, (r1.top + r1.bottom) / 2);
            ExcludeClipRect(dc, r1.left, (r1.top + r1.bottom) / 2, r1.right, (r1.top + r1.bottom) / 2 + 1);

            // hrana v casti s textem souboru
            SetBkColor(dc, LineColors[colorScheme].BkColor);
            (HPEN) SelectObject(dc, LineColors[colorScheme].BorderPen);
            // posouvame offset zacatku souradnice aby na carky spravne navazovali
            // pri scrollu
            MoveToEx(dc, r1.left - FirstVisibleChar * FontWidth, (r1.top + r1.bottom) / 2, NULL);
            LineTo(dc, r2.right, (r1.top + r1.bottom) / 2);
            SelectObject(dc, hOldPen);

            ExcludeClipRect(dc, r2.left, (r1.top + r1.bottom) / 2, r2.right, (r1.top + r1.bottom) / 2 + 1);
            goto LDRAWBLANK;
        }

        case CLineSpec::lcBlank:
        {
            if (line >= SelectDiffBegin && line <= SelectDiffEnd)
                colorScheme = LC_FOCUS;
            else
            {
                // aby se nam barevne nezvyraznily rozdili v prazdnych radcich pri 'ignore blank lines'
                if (((TTextFileViewWindow<CChar>*)Siblink)->Script[ViewMode][line].GetClass() == CLineSpec::lcCommon)
                    colorScheme = LC_NORMAL;
                else
                    colorScheme = LC_CHANGE;
            }

        LDRAWBLANK:

            if (line == SelectDiffBegin || line > 0 && line - 1 == SelectDiffEnd)
            {
                // vykreslime cast ohraniceni aktivni difference na hornim okraji radku
                // hrana ve sloupci s cisly radek
                hOldPen = (HPEN)SelectObject(dc, LineColors[LC_FOCUS].LineNumBorderPen);
                MoveToEx(dc, r1.left, r1.top, NULL);
                LineTo(dc, r1.right, r1.top);
                // hrana v casti s textem souboru
                (HPEN) SelectObject(dc, LineColors[LC_FOCUS].BorderPen);
                LineTo(dc, r2.right, r1.top);
                SelectObject(dc, hOldPen);
                r1.top++;
                r2.top++;
            }

            // vykreslime jen pozadi u cisla radky
            SetBkColor(dc, LineColors[colorScheme].LineNumBkColor);
            ExtTextOut(dc, 0, 0, ETO_OPAQUE /*| ETO_CLIPPED*/, &r1, NULL, 0, NULL);

            // vykreslime prazdnou radku
            if (line >= SelectionLineBegin && line < SelectionLineEnd)
                SetBkColor(dc, GetPALETTERGB(Colors[TEXT_BK_SELECTION]));
            else
            {
                if (type == CLineSpec::lcBlank && colorScheme != LC_NORMAL && DetailedDifferences &&
                    Script[ViewMode][MainWindow->GetChangeFirstLine(line)].GetClass() != CLineSpec::lcBlank)
                {
                    SetBkColor(dc, LineColors[colorScheme].BkCommon);
                }
                else
                {
                    SetBkColor(dc, LineColors[colorScheme].BkColor);
                }
            }
            ExtTextOut(dc, 0, 0, ETO_OPAQUE /*| ETO_CLIPPED*/, &r2, NULL, 0, NULL);

            break;
        }

        case CLineSpec::lcChange:
            if (line >= SelectDiffBegin && line <= SelectDiffEnd)
                colorScheme = LC_FOCUS;
            else
                colorScheme = LC_CHANGE;
            goto LDRAWLINE;

        case CLineSpec::lcCommon:
        {
            colorScheme = LC_NORMAL;

        LDRAWLINE:

            if (line == SelectDiffBegin || line > 0 && line - 1 == SelectDiffEnd)
            {
                // vykreslime cast ohraniceni aktivni difference na hornim okraji radku
                // hrana ve sloupci s cisly radek
                hOldPen = (HPEN)SelectObject(dc, LineColors[LC_FOCUS].LineNumBorderPen);
                MoveToEx(dc, r1.left, r1.top, NULL);
                LineTo(dc, r1.right, r1.top);
                // hrana ve casti s textem souboru
                (HPEN) SelectObject(dc, LineColors[LC_FOCUS].BorderPen);
                LineTo(dc, r2.right, r1.top);
                SelectObject(dc, hOldPen);
                r1.top++;
                r2.top++;
            }

            // vykreslime cislo radky najednou s pozadim
            int l = Script[ViewMode][line].GetLine();
            char num[numeric_limits<size_t>::digits10 + 1];
            sprintf(num, "%*d", LineNumDigits, l + 1);
            SetTextColor(dc, LineColors[colorScheme].LineNumFgColor);
            SetBkColor(dc, LineColors[colorScheme].LineNumBkColor);
            MappedASCII8TextOut.DoTextOut(dc, BORDER_WIDTH, text_y, ETO_OPAQUE | ETO_CLIPPED, &r1, num, LineNumDigits, NULL);

            // vykreslime radku texu najednou s pozadim
            int len = FormatLine(dc, LineBuffer, Lines[l], Lines[l + 1] - 1);
            if (line > SelectionLineBegin && line < SelectionLineEnd)
            {
                // cely radek je vybrany
                SetTextColor(dc, GetPALETTERGB(Colors[TEXT_FG_SELECTION]));
                SetBkColor(dc, GetPALETTERGB(Colors[TEXT_BK_SELECTION]));
                goto LNORMAL_PAINT;
            }
            else
            {
                if (line == SelectionLineBegin || line == SelectionLineEnd)
                {
                    // je vybrana jen cast radku
                    // vykreslime jen vybranou cast a vyjmeme ji z clipping regionu,
                    // potom pres ni vykreslime radku normalnima barvama

                    int offset1 = 0,
                        offset2 = len - FirstVisibleChar;
                    //if (line == SelectionLineBegin) offset1 = SelectionCharBegin - FirstVisibleChar;
                    //if (line == SelectionLineEnd) offset2 = SelectionCharEnd - FirstVisibleChar;
                    if (line == SelectionLineBegin)
                        offset1 = MeasureLine(l, SelectionCharBegin) - FirstVisibleChar;
                    if (line == SelectionLineEnd)
                        offset2 = MeasureLine(l, SelectionCharEnd) - FirstVisibleChar;

                    if (sizeof(CChar) == 2)
                    {
                        // Compensate for combining diacritics left of visible area
                        CChar* s = LineBuffer;
                        int j;
                        for (j = min(len, FirstVisibleChar); j > 0; j--, s++)
                        {
                            if (IS_COMBINING_DIACRITIC(*s))
                            {
                                offset1++;
                                offset2++;
                            }
                        }
                    }
                    if (offset1 < 0)
                        offset1 = 0;
                    if (offset2 < 0)
                        offset2 = 0;

                    if (offset2 > offset1 ||
                        line == SelectionLineBegin && line != SelectionLineEnd)
                    {
                        RECT r3 = r2;
                        r3.left = r2.left + offset1 * FontWidth;
                        if (line == SelectionLineEnd)
                            r3.right = r2.left + offset2 * FontWidth;
                        if (sizeof(CChar) == 2)
                        {
                            CChar* s = LineBuffer + FirstVisibleChar + offset1;
                            // Skip combining diacritics at the start of the selection
                            if (IS_COMBINING_DIACRITIC(*s) && (offset1 < offset2))
                            {
                                s++, offset1++;
                                r3.left += FontWidth;
                            }
                            s = LineBuffer + FirstVisibleChar + offset1;
                            if (line == SelectionLineEnd)
                            {
                                // Include in selection combining diacritics next to the selection
                                while ((FirstVisibleChar + offset2 < len) && IS_COMBINING_DIACRITIC(LineBuffer[FirstVisibleChar + offset2]))
                                {
                                    offset2++;
                                    r3.right += FontWidth;
                                }
                                // Compensate box width for combining diacritics inside the selection
                                int j;
                                for (j = offset2 - offset1; j > 0; s++, j--)
                                {
                                    if (IS_COMBINING_DIACRITIC(*s))
                                    {
                                        r3.right -= FontWidth;
                                    }
                                }
                            }
                            s = LineBuffer + FirstVisibleChar + offset1 - 1;
                            int j;
                            for (j = offset1; j > 0; s--, j--)
                            {
                                if (IS_COMBINING_DIACRITIC(*s))
                                {
                                    // Compensate for combining diacritics in visible area left of selection
                                    r3.left -= FontWidth;
                                    if (line == SelectionLineEnd)
                                        r3.right -= FontWidth;
                                }
                            }
                        }

                        SetTextColor(dc, GetPALETTERGB(Colors[TEXT_FG_SELECTION]));
                        SetBkColor(dc, GetPALETTERGB(Colors[TEXT_BK_SELECTION]));
                        /*              char ss[128]; sprintf(ss, "%d %d %d, %d %d\n", offset1, offset2, r3.left, r3.right, clipRect.right,
                 LineBuffer[FirstVisibleChar + offset2-2], LineBuffer[FirstVisibleChar + offset2-1]);
              OutputDebugString(ss);*/

                        // Windows XP seems to fail on len ~ 200 000. 1024 should be enough for everybody
                        // TODO na 4K displayich už 1024 nemusí stačit!!
                        ExtTextOutX(dc, r3.left, text_y, ETO_OPAQUE | ETO_CLIPPED, &r3,
                                    LineBuffer + FirstVisibleChar + offset1, min(1024, offset2 - offset1), NULL);

                        ExcludeClipRect(dc, r3.left, r3.top, r3.right, r3.bottom);
                    }
                }

                // nastavime normalni barvy
                SetTextColor(dc, LineColors[colorScheme].FgColor);
                SetBkColor(dc, LineColors[colorScheme].BkColor);
            }

            if (DetailedDifferences && type == CLineSpec::lcChange)
            {
                int c = 0, d = 0, z = 0;
                size_t j = 0;
                int offs = 0;
                BOOL adjusted = FALSE;
                size_t count;

                if (((TTextFileViewWindow<CChar>*)Siblink)->Script[ViewMode][MainWindow->GetChangeFirstLine(line)].GetClass() == CLineSpec::lcBlank)
                    goto LNORMAL_PAINT; // pouze add, nebo delete

                if (Script[ViewMode][line].GetChanges())
                {
                    // tranfrom offsets (due tabs) in line changes script
                    int* script = Script[ViewMode][line].GetChanges();
                    count = *script;
                    TransformOffsets(script + 1, *script, Lines[l]);
                    //copy(script + 1, script + 1 + *script, LineChanges.begin());
                }
                else
                    goto LCOMMON; // jde o celoradkovy blok bez differenci

                while (j < count)
                {
                    // spolecne znaky
                    if (LineChanges[j] != offs && LineChanges[j] > FirstVisibleChar)
                    {
                        PTCommon[c].x = r2.left + FontWidth * (offs - FirstVisibleChar);
                        PTCommon[c].y = text_y;
                        PTCommon[c].n = int(LineChanges[j]) - offs;
                        PTCommon[c].lpstr = LineBuffer + offs;
                        PTCommon[c].uiFlags = ETO_OPAQUE | ETO_CLIPPED;
                        PTCommon[c].rcl.top = r2.top;
                        PTCommon[c].rcl.bottom = r2.bottom;
                        if (offs < FirstVisibleChar)
                        {
                            // zacatek bloku neni videt
                            PTCommon[c].rcl.left = LineNumWidth;
                            adjusted = FALSE; // vynulujem flag
                        }
                        else
                        {
                            PTCommon[c].rcl.left = r2.left + FontWidth * (offs - FirstVisibleChar);
                            if (adjusted)
                            {
                                PTCommon[c].rcl.left += CaretWidth;
                                adjusted = FALSE; // vynulujem flag
                            }
                        }
                        PTCommon[c].rcl.right = r2.left + FontWidth * (LineChanges[j] - FirstVisibleChar);
                        PTCommon[c].pdx = 0;
                        c++;
                    }
                    // zmenene znaky
                    offs = int(LineChanges[j]);
                    j++;
                    if (offs < LineChanges[j])
                    {
                        // normalni blok
                        if (LineChanges[j] > FirstVisibleChar)
                        {
                            PTChange[d].x = r2.left + FontWidth * (offs - FirstVisibleChar);
                            PTChange[d].y = text_y;
                            PTChange[d].n = int(LineChanges[j] - offs);
                            PTChange[d].lpstr = LineBuffer + offs;
                            PTChange[d].uiFlags = ETO_OPAQUE | ETO_CLIPPED;
                            PTChange[d].rcl.top = r2.top;
                            PTChange[d].rcl.bottom = r2.bottom;
                            if (offs < FirstVisibleChar)
                                PTChange[d].rcl.left = LineNumWidth;
                            else
                                PTChange[d].rcl.left = r2.left + FontWidth * (offs - FirstVisibleChar);
                            PTChange[d].rcl.right = r2.left + FontWidth * (LineChanges[j] - FirstVisibleChar);
                            PTChange[d].pdx = 0;
                            d++;
                        }
                    }
                    else
                    {
                        // svisly prouzek pro oznaceni mista insertion/deletion v druhem panelu
                        if (offs >= FirstVisibleChar)
                        {
                            PTZeroChange[z].x = r2.left + FontWidth * (offs - FirstVisibleChar);
                            PTZeroChange[z].y = text_y;
                            PTZeroChange[z].lpstr = LineBuffer + offs;
                            PTZeroChange[z].uiFlags = ETO_OPAQUE | ETO_CLIPPED;
                            PTZeroChange[z].rcl.top = r2.top;
                            PTZeroChange[z].rcl.bottom = r2.bottom;
                            PTZeroChange[z].rcl.left = r2.left + FontWidth * (offs - FirstVisibleChar);
                            PTZeroChange[z].n = offs < len ? 1 : 0; // osetrime pripad na konci radky
                            PTZeroChange[z].rcl.right = PTZeroChange[z].rcl.left + CaretWidth;
                            adjusted = TRUE;
                            PTZeroChange[z].pdx = 0;
                            z++;
                        }
                    }
                    offs = int(LineChanges[j]);
                    j++;
                }
            // spolecne znaky - od poslendi zmeny do konce radku

            /*if (len - offs || c > 0)
          {*/
            LCOMMON:
                PTCommon[c].x = r2.left + FontWidth * (offs - FirstVisibleChar);
                PTCommon[c].y = text_y;
                PTCommon[c].n = len - offs;
                PTCommon[c].lpstr = LineBuffer + offs;
                PTCommon[c].uiFlags = ETO_OPAQUE | ETO_CLIPPED;
                PTCommon[c].rcl.top = r2.top;
                PTCommon[c].rcl.bottom = r2.bottom;
                if (offs < FirstVisibleChar)
                {
                    // zacatek bloku neni videt
                    PTCommon[c].rcl.left = LineNumWidth;
                }
                else
                {
                    PTCommon[c].rcl.left = r2.left + FontWidth * (offs - FirstVisibleChar);
                    if (adjusted)
                    {
                        PTCommon[c].rcl.left = min(PTCommon[c].rcl.left + LONG(CaretWidth), r2.right);
                    }
                }
                PTCommon[c].rcl.right = r2.right;
                PTCommon[c].pdx = 0;
                c++;
                /*}
          else
          {
            PTChange[d].x = 0;
            PTChange[d].y = 0;
            PTChange[d].n = 0;
            PTChange[d].lpstr = NULL;
            PTChange[d].uiFlags = ETO_OPAQUE | ETO_CLIPPED;
            PTChange[d].rcl.top = r2.top;
            PTChange[d].rcl.bottom = r2.bottom;
            PTChange[d].rcl.left = r2.left + FontWidth*len;
            PTChange[d].rcl.right = r2.right;
            PTChange[d].pdx = 0;
            d++;
          }*/

                /*if (len - offs)
          {
            PTCommon[c].x = r2.left + FontWidth*offs;
            PTCommon[c].y = text_y;
            PTCommon[c].n = len - offs;
            PTCommon[c].lpstr = LineBuffer + offs;
            PTCommon[c].uiFlags = ETO_OPAQUE | ETO_CLIPPED;
            PTCommon[c].rcl.top = r2.top;
            PTCommon[c].rcl.bottom = r2.bottom;
            PTCommon[c].rcl.left = r2.left + FontWidth*offs;
            PTCommon[c].rcl.right = PTCommon[c].rcl.left + FontWidth*PTCommon[c].n;
            if (adjusted)
            {
              PTCommon[c].rcl.left = min(PTCommon[c].rcl.left + 2, r2.right);
              adjusted = FALSE;
            }              
            PTCommon[c].pdx = 0;
            c++;
          }
          
          PTChange[d].x = 0;
          PTChange[d].y = 0;
          PTChange[d].n = 0;
          PTChange[d].lpstr = NULL;
          PTChange[d].uiFlags = ETO_OPAQUE | ETO_CLIPPED;
          PTChange[d].rcl.top = r2.top;
          PTChange[d].rcl.bottom = r2.bottom;
          PTChange[d].rcl.left = r2.left + FontWidth*len;
          PTChange[d].rcl.right = r2.right;
          PTChange[d].pdx = 0;
          d++;*/

                if (d > 0)
                {
                    PolytextLengthFixup(PTChange.begin(), PTChange.begin() + d);
                    PolyTextOutX(dc, &PTChange.front(), d);
                }

                SetTextColor(dc, LineColors[colorScheme].FgCommon);
                if (z > 0)
                    PolyTextOutX(dc, &PTZeroChange.front(), z);

                SetBkColor(dc, LineColors[colorScheme].BkCommon);
                PolytextLengthFixup(PTCommon.begin(), PTCommon.begin() + c);
                PolyTextOutX(dc, &PTCommon.front(), c);
            }
            else
            {
            LNORMAL_PAINT:
                if (FirstVisibleChar > len)
                    len = FirstVisibleChar; // jen vymazeme pozadi
                // Windows XP seems to fail on len ~ 200 000. 1024 should be enough for everybody
                len -= FirstVisibleChar;

                // TODO na 4K displayich už 1024 nemusí stačit!!
                ExtTextOutX(dc, r2.left, text_y, ETO_OPAQUE | ETO_CLIPPED, &r2,
                            LineBuffer + FirstVisibleChar, min(1024, len), NULL);
            }
            break;
        }
        }
    }

    if (r1.bottom < clipRect.bottom)
    {
        // jeste vykreslime prazdny prostor od posledniho platneho
        // radku  ve skriptu do spodniho okraje okna

        r1.top = r2.top = r1.bottom;
        r1.bottom = r2.bottom = clipRect.bottom;

        if (FirstVisibleLine + i - 1 == SelectDiffEnd)
        {
            // vykreslime cast ohraniceni aktivni difference na hornim okraji radku
            hOldPen = (HPEN)SelectObject(dc, LineColors[LC_FOCUS].LineNumBorderPen);
            MoveToEx(dc, r1.left, r1.top, NULL);
            LineTo(dc, r1.right, r1.top);
            // hrana v casti s textem souboru
            (HPEN) SelectObject(dc, LineColors[LC_FOCUS].BorderPen);
            LineTo(dc, r2.right, r1.top);
            SelectObject(dc, hOldPen);
            r1.top++;
            r2.top++;
        }

        // vykreslime jen pozadi u cisla radky
        SetBkColor(dc, LineColors[LC_NORMAL].LineNumBkColor);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE /*| ETO_CLIPPED*/, &r1, NULL, 0, NULL);

        // vykreslime prazdnou radku
        SetBkColor(dc, LineColors[LC_NORMAL].BkColor);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE /*| ETO_CLIPPED*/, &r2, NULL, 0, NULL);
    }

    SelectObject(dc, oldFont);
    if (UsePalette)
        SelectPalette(dc, oldPalette, FALSE);
    EndPaint(HWindow, &ps);
}
