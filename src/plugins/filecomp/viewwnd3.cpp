// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CHexFileViewWindow
//

const char* Bin2ASCII = "0123456789ABCDEF";

char* QWord2Ascii(QWORD qw, char* buffer, int digits)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("QWord2Ascii(, , %d)", digits);
    int i = digits;
    int shift = 0;
    while (i)
    {
        buffer[--i] = Bin2ASCII[(qw >> (shift)) & 0x0F];
        shift += 4;
    }
    buffer[digits] = 0;
    return buffer;
}

int ComputeAddressCharWidth(QWORD size1, QWORD size2)
{
    CALL_STACK_MESSAGE1("ComputeAddressCharWidth(, )");
    int i = 1;
    QWORD mask = -1;
    QWORD size = __max(size1, size2);
    while (size & (mask <<= 4))
        i++;
    return i;
}

CHexFileViewWindow::CHexFileViewWindow(CFileViewID id) : CFileViewWindow(id, fvtHex)
{
    CALL_STACK_MESSAGE1("CHexFileViewWindow::CHexFileViewWindow()");
    SelectDifferenceOnButtonUp = FALSE;
    SelectedOffset = -1;
    SelectedLength = 0;
    FocusedDiffOffset = -1;
    ViewMode = fvmStandard;
}

CHexFileViewWindow::~CHexFileViewWindow()
{
    CALL_STACK_MESSAGE1("CHexFileViewWindow::~CHexFileViewWindow()");
    DestroyData();
}

void CHexFileViewWindow::DestroyData()
{
    CALL_STACK_MESSAGE1("CHexFileViewWindow::DestroyData()");
    DataValid = FALSE;

    Mapping.Destroy();
}

void CHexFileViewWindow::SetSiblink(CFileViewWindow* wnd)
{
    CALL_STACK_MESSAGE1("CHexFileViewWindow::SetSiblink()");
    Siblink = wnd;
    SiblinkWidth = ((CHexFileViewWindow*)Siblink)->Width;

    UpdateScrollBars(TRUE);
}

/*
void 
CHexFileViewWindow::CalculateBytesPerLine(QWORD siblinkSize)
{
  BytesPerLine = max(max(Width, SiblinkWidth) - LineNumWidth) / FontWidth / 16, 1) * 4;
}
*/

void CHexFileViewWindow::UpdateScrollBars(BOOL repaint)
{
    CALL_STACK_MESSAGE2("CHexFileViewWindow::UpdateScrollBars(%d)", repaint);
    if (Siblink)
    {
        SiblinkWidth = ((CHexFileViewWindow*)Siblink)->Width;
        if (!SiblinkWidth)
            SiblinkWidth = Width;
    }
    //CalculateBytesPerLine(siblinkSize);
    int bpr = BytesPerLine;
    int width = (__max(Width, SiblinkWidth) - LineNumWidth) / FontWidth;
    BytesPerLine = __max((width) / 17, 1) * 4;

    if (!IsZoomed(GetParent(HWindow)) &&
        (BytesPerLine + 4) / 4 * 17 - width <= (BytesPerLine + 4) / 2)
    {
        BytesPerLine += 4;
    }

    QWORD size = __max(Mapping.GetFileSize(), SiblinkSize);
    ScrollScale = __min((double)INT_MAX / (double)(__int64)((size - 1) / BytesPerLine), 1);

    BOOL update = FALSE;
    int vPage = Height / FontHeight;
    __int64 vSize = __max(Mapping.GetFileSize(), SiblinkSize);
    __int64 maxVPos = __max((vSize - 1 + BytesPerLine - 1) / BytesPerLine - vPage, 0);
    __int64 curVPos = ViewOffset / BytesPerLine;
    __int64 vPos = __min(curVPos, maxVPos);
    int hPage = (__min(Width, SiblinkWidth) - LineNumWidth) / FontWidth;
    if (hPage < 0)
    { // maximized pane
        Width = SiblinkWidth = __max(Width, SiblinkWidth);
        hPage = (Width - LineNumWidth) / FontWidth;
    }
    int hMax = BytesPerLine / 4 * 17 - 1;
    int hOffs = __min(HScrollOffs, __max(hMax - hPage + 1, 0));
    if (curVPos != vPos)
    {
        if (repaint && bpr == BytesPerLine && hOffs == HScrollOffs)
        {
            if (Abs64(vPos - curVPos) < vPage - 1 && vPos % BytesPerLine == curVPos % BytesPerLine)
            {
                ScrollWindowEx(HWindow, 0, (int)((curVPos - vPos) * FontHeight), NULL, NULL, NULL, NULL, SW_INVALIDATE);
            }
            else
                InvalidateRect(HWindow, NULL, FALSE);
            update = TRUE;
        }
        ViewOffset = vPos * BytesPerLine;
    }

    if (bpr != BytesPerLine || hOffs != HScrollOffs)
    {
        InvalidateRect(HWindow, NULL, FALSE);
        update = TRUE;
    }

    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nMin = 0;
    si.nMax = (int)((__int64)((size - 1) / BytesPerLine) * ScrollScale + 0.5);
    si.nPage = vPage;
    si.nPos = (int)(vPos * ScrollScale + 0.5);
    SetScrollInfo(HWindow, SB_VERT, &si, TRUE);
    si.nPage = hPage;
    si.nMin = 0;
    si.nMax = hMax;
    si.nPos = HScrollOffs = hOffs;
    SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);

    if (update)
        UpdateWindow(HWindow);
}

BOOL CHexFileViewWindow::SetData(QWORD firstDiff, const char* path, QWORD siblinkSize)
{
    CALL_STACK_MESSAGE2("CHexFileViewWindow::SetData(, %s, )", path);
    DestroyData();

    // FILE_SHARE_WRITE : See also CFilecompWorker::GuardedBody()
    HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    strcpy(Path, path); // Path may be needed in Retry dialog upon WM_USER_HANDLEFILEERROR
    if (hFile == INVALID_HANDLE_VALUE)
        return Error(GetParent(HWindow), IDS_OPEN, path);

    Mapping.SetFile(hFile, atRead);

    if (firstDiff != (QWORD)-1)
        FirstDiff = firstDiff;
    else
        FirstDiff = 0; // Files are actually equal
    SiblinkSize = siblinkSize;

    // zjistime velikost mista pro offset
    LineNumDigits = ComputeAddressCharWidth(Mapping.GetFileSize(), SiblinkSize);
    LineNumWidth = (LineNumDigits + 1) * FontWidth + BORDER_WIDTH;

    ViewOffset = 0;
    HScrollOffs = 0;

    EnablePaint();

    UpdateScrollBars(TRUE);

    DataValid = TRUE;

    return TRUE;
}

void CHexFileViewWindow::Paint()
{
    CALL_STACK_MESSAGE_NONE

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

    RECT r1; // cislo radky
    r1.left = 0;
    r1.right = LineNumDigits * FontWidth + 2 * BORDER_WIDTH;
    r1.bottom = 0; // abychom meli pozdeji platana data, kdyby kreslici smycka vubec neprobehla
    RECT r2;       // tri znaky + mezera v hexa oblasti (napr "1F ")
    RECT r3;       // ascii oblast
    //r2.left = r1.right;
    //r2.right = Width;
    //int text_x = r2.left;
    int text_y;
    int colorScheme;
    char* data;
    int size;
    char* siblinkData;
    int siblinkSize;
    RECT r;

    if (!PaintEnabled)
        goto LERASE_WINDOW;

    if (ViewOffset + clipLastRow * BytesPerLine > Mapping.GetFileSize())
    {
        if (Mapping.GetFileSize() >= ViewOffset)
            clipLastRow = (int)((Mapping.GetFileSize() - ViewOffset + BytesPerLine - 1) / BytesPerLine);
        else
            clipLastRow = 0;
    }

    if (clipFirstRow < clipLastRow)
    {
        size = (int)(__min(ViewOffset + clipLastRow * BytesPerLine + 1, Mapping.GetFileSize()) -
                     (ViewOffset + clipFirstRow * BytesPerLine));
        data = (char*)Mapping.MapViewOfFile(ViewOffset + clipFirstRow * BytesPerLine, size);
        if (!data)
        {
            HandleFileError(Path, GetLastError());
            goto LERASE_WINDOW;
        }

        if (ViewOffset + clipFirstRow * BytesPerLine < SiblinkSize)
        {
            siblinkSize = (int)(__min(ViewOffset + clipLastRow * BytesPerLine + 1, SiblinkSize) -
                                (ViewOffset + clipFirstRow * BytesPerLine));
            siblinkData = (char*)((CHexFileViewWindow*)Siblink)->Mapping.MapViewOfFile(ViewOffset + clipFirstRow * BytesPerLine, siblinkSize);
            if (!siblinkData)
            {
                HandleFileError(((CHexFileViewWindow*)Siblink)->GetPath(), GetLastError());
                goto LERASE_WINDOW;
            }
        }
        else
            siblinkSize = 0;
    }

    __try
    {
        int i;
        // vykreslime mezeru offsetem a textem
        r.top = clipRect.top;
        r.bottom = clipRect.bottom;
        r.left = r1.right;
        r.right = LineNumWidth;
        SetBkColor(dc, LineColors[LC_NORMAL].BkColor);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
        // vykreslime mezeru mezi sloupci s DWORDy mezery
        for (i = HScrollOffs / 13; i < BytesPerLine / 4; i++)
        {
            r.left = LineNumWidth + FontWidth * (11 + i * 13) - HScrollOffs * FontWidth;
            r.right = r.left + FontWidth * 2;
            if (r.left < LineNumWidth)
                r.left = LineNumWidth;
            ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
        }
        // vykreslime zbytek
        for (i = clipFirstRow; i < clipLastRow; i++)
        {
            text_y = r1.top = r2.top = r3.top = i * FontHeight;
            r1.bottom = r3.bottom = r2.bottom = r1.top + FontHeight;

            colorScheme = LC_NORMAL;

            // vykreslime cislo radky najednou s pozadim
            char num[32];
            QWord2Ascii(ViewOffset + i * BytesPerLine, num, LineNumDigits);
            SetTextColor(dc, LineColors[colorScheme].LineNumFgColor);
            SetBkColor(dc, LineColors[colorScheme].LineNumBkColor);
            MappedASCII8TextOut.DoTextOut(dc, BORDER_WIDTH, text_y, ETO_OPAQUE | ETO_CLIPPED, &r1, num, LineNumDigits, NULL);

            r2.left = LineNumWidth - HScrollOffs * FontWidth;
            r3.left = LineNumWidth + (BytesPerLine / 4 * 13) * FontWidth - HScrollOffs * FontWidth;

            int maxj = __min((i - clipFirstRow + 1) * BytesPerLine, size);
            int j;
            for (j = (i - clipFirstRow) * BytesPerLine; j < maxj; j++)
            {
                r2.right = r2.left + FontWidth * (j % 4 == 3 ? 2 : 3);
                r3.right = r3.left + FontWidth;

                if ((j + ViewOffset < SelectedOffset) || (j + ViewOffset >= SelectedOffset + SelectedLength))
                {
                    if (j >= siblinkSize || siblinkData[j] != data[j])
                    {
                        QWORD currentAddr = ViewOffset + clipFirstRow * BytesPerLine + j;
                        if (currentAddr >= FocusedDiffOffset &&
                            currentAddr < FocusedDiffOffset + FocusedDiffLength)
                        {
                            colorScheme = LC_FOCUS;
                        }
                        else
                        {
                            colorScheme = LC_CHANGE;
                        }
                    }
                    else
                        colorScheme = LC_NORMAL;

                    SetTextColor(dc, LineColors[colorScheme].FgColor);
                    SetBkColor(dc, LineColors[colorScheme].BkColor);
                }
                else
                {
                    SetTextColor(dc, GetPALETTERGB(Colors[TEXT_FG_SELECTION]));
                    SetBkColor(dc, GetPALETTERGB(Colors[TEXT_BK_SELECTION]));
                }

                // kreslime ASCII cast
                if (r3.left >= LineNumWidth)
                {
                    MappedASCII8TextOut.DoTextOut(dc, r3.left, text_y, ETO_OPAQUE | ETO_CLIPPED, &r3, data + j, 1, NULL);
                }

                // kreslime hex cast
                if (r2.right > LineNumWidth)
                {
                    char buf[2];
                    buf[0] = Bin2ASCII[(unsigned char)data[j] >> 4];
                    buf[1] = Bin2ASCII[data[j] & 0x0F];

                    int k = j + 1;
                    if (colorScheme == LC_NORMAL ||
                        k < maxj && (k >= siblinkSize || siblinkData[k] != data[k]))
                    {
                        // vykreslime celou trojici ('XX ') v jedne barve
                        r = r2;
                        if (r.left < LineNumWidth)
                            r.left = LineNumWidth;
                        MappedASCII8TextOut.DoTextOut(dc, r2.left, text_y, ETO_OPAQUE | ETO_CLIPPED, &r, buf, 2, NULL);
                    }
                    else
                    {
                        // vykreslime prvni dva znaky
                        r = r2;
                        r.right = r2.left + 2 * FontWidth;
                        if (r2.right > LineNumWidth)
                        {
                            if (r.left < LineNumWidth)
                                r.left = LineNumWidth;
                            MappedASCII8TextOut.DoTextOut(dc, r2.left, text_y, ETO_OPAQUE | ETO_CLIPPED, &r, buf, 2, NULL);
                        }
                        // vykreslime oddelujici mezeru
                        if (r.right < r2.right)
                        {
                            SetTextColor(dc, LineColors[LC_NORMAL].FgColor);
                            SetBkColor(dc, LineColors[LC_NORMAL].BkColor);
                            r.left = r.right;
                            r.right = r2.right;
                            if (r.left < LineNumWidth)
                                r.left = LineNumWidth;
                            ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
                        }
                    }
                }

                r2.left = r2.right;
                if (j % 4 == 3)
                    r2.left += FontWidth * 2;
                r3.left = r3.right;
            }

            if (r3.left < Width)
            {
                r3.right = Width;
                SetBkColor(dc, LineColors[LC_NORMAL].BkColor);
                ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r3, NULL, 0, NULL);
            }
        }
    }
    __except (HandleFileException(GetExceptionInformation()))
    {
        // chyba v souboru
        goto LERASE_WINDOW;
    }

    if (clipFirstRow < clipLastRow &&
        r2.left < LineNumWidth + (BytesPerLine / 4 * 13) * FontWidth - HScrollOffs * FontWidth)
    {
        r2.right = LineNumWidth + (BytesPerLine / 4 * 13) * FontWidth - HScrollOffs * FontWidth;
        SetBkColor(dc, LineColors[LC_NORMAL].BkColor);
        if (r2.right > LineNumWidth)
        {
            if (r2.left < LineNumWidth)
                r2.left = LineNumWidth;
            ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r2, NULL, 0, NULL);
        }
    }

    if (r1.bottom < clipRect.bottom)
    {
        // jeste vykreslime prazdny prostor od posledniho platneho
        // radku do spodniho okraje okna

        r1.top = r2.top = r1.bottom;
        r1.bottom = r2.bottom = clipRect.bottom;
        r2.left = LineNumWidth;
        r2.right = Width;

        // vykreslime jen pozadi u cisla radky
        SetBkColor(dc, LineColors[LC_NORMAL].LineNumBkColor);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r1, NULL, 0, NULL);

        // vykreslime prazdnou radku
        SetBkColor(dc, LineColors[LC_NORMAL].BkColor);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r2, NULL, 0, NULL);
    }

    if (!PaintEnabled)
    {
    LERASE_WINDOW:
        RECT cr;
        GetClientRect(HWindow, &cr);
        r1.top = r2.top = cr.top;
        r1.bottom = r2.bottom = cr.bottom;
        r2.left = r1.right;
        r2.right = Width;

        // vykreslime jen pozadi u cisla radky
        SetBkColor(dc, LineColors[LC_NORMAL].LineNumBkColor);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r1, NULL, 0, NULL);

        // vykreslime prazdnou radku
        SetBkColor(dc, LineColors[LC_NORMAL].BkColor);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r2, NULL, 0, NULL);
    }

    SelectObject(dc, oldFont);
    if (UsePalette)
        SelectPalette(dc, oldPalette, FALSE);
    EndPaint(HWindow, &ps);
}

int CHexFileViewWindow::HandleFileException(EXCEPTION_POINTERS* e)
{
    CALL_STACK_MESSAGE1("CHexFileViewWindow::HandleFileException()");
    if (Mapping.IsFileIOException(e))
    {
        HandleFileError(Path, ERROR_SUCCESS);
        return EXCEPTION_EXECUTE_HANDLER; // spustime __except blok
    }
    if (((CHexFileViewWindow*)Siblink)->Mapping.IsFileIOException(e))
    {
        HandleFileError(((CHexFileViewWindow*)Siblink)->GetPath(), ERROR_SUCCESS);
        return EXCEPTION_EXECUTE_HANDLER; // spustime __except blok
    }
    return EXCEPTION_CONTINUE_SEARCH; // hodime vyjimku dale ... call-stacku
}

void CHexFileViewWindow::HandleFileError(const char* path, int error)
{
    CALL_STACK_MESSAGE3("CHexFileViewWindow::HandleFileError(%s, %d)", path, error);
    PostMessage(HWindow, WM_USER_HANDLEFILEERROR, (WPARAM)path, error);
    DisablePaint();
    ((CHexFileViewWindow*)Siblink)->DisablePaint();
}

QWORD
CHexFileViewWindow::FindDifference(int cmd, QWORD* pOffset)
{
    CALL_STACK_MESSAGE_NONE
    if (cmd == 0)
        return 0;

    __try
    {
        unsigned int blokSize = Mapping.GetMinViewSize();
        DWORD size;
        QWORD remain;
        QWORD offset;
        char *ptr0, *ptr1, *start, *end;

        GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help

        if (cmd == CM_FIRSTDIFF || cmd == CM_NEXTDIFF ||
            cmd == CM_PREVDIFF && FocusedDiffOffset == -1)
        {
            if (cmd == CM_FIRSTDIFF || FocusedDiffOffset == -1)
            {
                *pOffset = offset = FirstDiff;
            }
            else
            {
                offset = FocusedDiffOffset + FocusedDiffLength;
                if (offset >= __min(Mapping.GetFileSize(), SiblinkSize))
                    return 0;
                remain = __min(Mapping.GetFileSize(), SiblinkSize) - offset;

                while (remain)
                {
                    size = (unsigned int)__min(blokSize, remain);

                    ptr0 = (char*)Mapping.MapViewOfFile(offset, size);
                    if (!ptr0)
                    {
                        HandleFileError(Path, GetLastError());
                        return 0;
                    }

                    ptr1 = (char*)((CHexFileViewWindow*)Siblink)->Mapping.MapViewOfFile(offset, size);
                    if (!ptr1)
                    {
                        HandleFileError(((CHexFileViewWindow*)Siblink)->GetPath(), GetLastError());
                        return 0;
                    }

                    start = ptr0;
                    end = ptr0 + size;
                    if (size > 3)
                    {
                        end -= 3;
                        while (ptr0 < end && *(int*)ptr0 == *(int*)ptr1)
                            ptr0 += sizeof(int), ptr1 += sizeof(int);
                        end += 3;
                    }

                    while (ptr0 < end && *ptr0 == *ptr1)
                        ptr0++, ptr1++;

                    if (ptr0 < end)
                        break;

                    remain -= size;
                    offset += size;

                    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == GetParent(HWindow))
                    {
                        MSG msg; // vyhodime nabufferovany ESC
                        while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                            ;
                        return 0;
                    }
                }

                if (remain)
                {
                    *pOffset = offset = ptr0 - start + offset;
                }
                else
                {
                    if (Mapping.GetFileSize() != SiblinkSize)
                    {
                        *pOffset = __min(Mapping.GetFileSize(), SiblinkSize);
                        return __max(Mapping.GetFileSize(), SiblinkSize) - *pOffset;
                    }
                    else
                        return 0;
                }
            }

            remain = __min(Mapping.GetFileSize(), SiblinkSize) - offset;
            while (remain)
            {
                size = (unsigned int)__min(blokSize, remain);

                ptr0 = (char*)Mapping.MapViewOfFile(offset, size);
                if (!ptr0)
                {
                    HandleFileError(Path, GetLastError());
                    return 0;
                }

                ptr1 = (char*)((CHexFileViewWindow*)Siblink)->Mapping.MapViewOfFile(offset, size);
                if (!ptr1)
                {
                    HandleFileError(((CHexFileViewWindow*)Siblink)->GetPath(), GetLastError());
                    return 0;
                }

                start = ptr0;
                end = ptr0 + size;

                while (ptr0 < end && *ptr0 != *ptr1)
                    ptr0++, ptr1++;

                if (ptr0 < end)
                    break;

                remain -= size;
                offset += size;

                if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == GetParent(HWindow))
                {
                    MSG msg; // vyhodime nabufferovany ESC
                    while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                        ;
                    return 0;
                }
            }

            if (remain)
            {
                return (ptr0 - start + offset) - *pOffset;
            }
            else
            {
                if (Mapping.GetFileSize() != SiblinkSize)
                {
                    return __max(Mapping.GetFileSize(), SiblinkSize) - *pOffset;
                }
                else
                {
                    return Mapping.GetFileSize() - *pOffset;
                }
            }
        }
        else
        {
            // hledame pozpatku
            QWORD diffEnd;

            if (cmd == CM_PREVDIFF || cmd == CM_LASTDIFF && Mapping.GetFileSize() == SiblinkSize)
            {
                remain = cmd == CM_PREVDIFF ? FocusedDiffOffset : Mapping.GetFileSize();
                offset = remain - 1;

                while (remain)
                {
                    size = (unsigned int)__min(blokSize, remain);

                    ptr0 = (char*)Mapping.MapViewOfFile(offset - size + 1, size);
                    if (!ptr0)
                    {
                        HandleFileError(Path, GetLastError());
                        return 0;
                    }
                    ptr0 += size - 1;

                    ptr1 = (char*)((CHexFileViewWindow*)Siblink)->Mapping.MapViewOfFile(offset - size + 1, size);
                    if (!ptr1)
                    {
                        HandleFileError(((CHexFileViewWindow*)Siblink)->GetPath(), GetLastError());
                        return 0;
                    }
                    ptr1 += size - 1;

                    start = ptr0;
                    end = ptr0 - size;
                    if (size > 3)
                    {
                        end += 3;
                        ptr0 -= 3;
                        ptr1 -= 3;
                        while (ptr0 > end && *(int*)ptr0 == *(int*)ptr1)
                            ptr0 -= sizeof(int), ptr1 -= sizeof(int);
                        end -= 3;
                        ptr0 += 3;
                        ptr1 += 3;
                    }

                    while (ptr0 > end && *ptr0 == *ptr1)
                        ptr0--, ptr1--;

                    if (ptr0 > end)
                        break;

                    remain -= size;
                    offset -= size;

                    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == GetParent(HWindow))
                    {
                        MSG msg; // vyhodime nabufferovany ESC
                        while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                            ;
                        return 0;
                    }
                }

                if (!remain)
                    return 0; // jsou shodny

                offset = offset - (start - ptr0);
                diffEnd = remain = offset + 1;
            }
            else
            {
                diffEnd = __max(Mapping.GetFileSize(), SiblinkSize);
                remain = __min(Mapping.GetFileSize(), SiblinkSize);
                offset = remain - 1;
            }

            while (remain)
            {
                size = (unsigned int)__min(blokSize, remain);

                ptr0 = (char*)Mapping.MapViewOfFile(offset - size + 1, size);
                if (!ptr0)
                {
                    HandleFileError(Path, GetLastError());
                    return 0;
                }
                ptr0 += size - 1;

                ptr1 = (char*)((CHexFileViewWindow*)Siblink)->Mapping.MapViewOfFile(offset - size + 1, size);
                if (!ptr1)
                {
                    HandleFileError(((CHexFileViewWindow*)Siblink)->GetPath(), GetLastError());
                    return 0;
                }
                ptr1 += size - 1;

                start = ptr0;
                end = ptr0 - size;

                while (ptr0 > end && *ptr0 != *ptr1)
                    ptr0--, ptr1--;

                if (ptr0 > end)
                    break;

                remain -= size;
                offset -= size;

                if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == GetParent(HWindow))
                {
                    MSG msg; // vyhodime nabufferovany ESC
                    while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                        ;
                    return 0;
                }
            }

            if (remain)
                *pOffset = offset - (start - ptr0) + 1;
            else
                *pOffset = 0;

            return diffEnd - *pOffset;
        }
    }
    __except (HandleFileException(GetExceptionInformation()))
    {
        return 0;
    }
    return 0;
}

void CHexFileViewWindow::SelectDifference(QWORD offset, QWORD length, BOOL center)
{
    CALL_STACK_MESSAGE2("CHexFileViewWindow::SelectDifference(, , %d)", center);
    FocusedDiffOffset = offset;
    FocusedDiffLength = length;

    int page = Height / FontHeight;
    QWORD pos = offset / BytesPerLine;
    QWORD lines = (offset + length + BytesPerLine - 1) / BytesPerLine - pos;
    __int64 size = __max(Mapping.GetFileSize(), SiblinkSize);
    QWORD maxPos = __max((size - 1 + BytesPerLine - 1) / BytesPerLine - page, 0);

    if (center &&
        (lines >= page && (ViewOffset >= offset + length || ViewOffset + page * BytesPerLine <= offset) ||
         lines < page && (ViewOffset >= offset || ViewOffset + page * BytesPerLine <= offset + length)))
    {
        /*
    if (lines > page)
    {
      ViewOffset = pos*BytesPerLine;
    }
    else
    {
      ViewOffset = __max(((__int64)pos - (page - (__int64)lines)/2), 0) *BytesPerLine; // vycentrujeme differenci k oknu
    }
    */
        if (lines <= page)
        {
            pos = __max(((__int64)pos - (page - (__int64)lines) / 2), 0);
        }
        if (pos > maxPos)
            pos = maxPos;
        ViewOffset = pos * BytesPerLine;
        SetScrollPos(HWindow, SB_VERT, (int)(((__int64)pos) * ScrollScale + 0.5), TRUE);
    }

    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
}

QWORD
CHexFileViewWindow::CalculateOffset(int x, int y)
{
    if (y < 0)
    {
        y = 0;
        x = LineNumWidth - HScrollOffs * FontWidth; // aby vyslo hOffs nula
    }
    QWORD offset = ViewOffset + y / FontHeight * BytesPerLine;
    int hOffs = (x - LineNumWidth) + HScrollOffs * FontWidth;
    if (hOffs < 0)
        hOffs = 0;

    if ((hOffs + FontWidth / 2) / FontWidth < BytesPerLine / 4 * 13)
    {
        // Inside hexa block
        //offset += (hOffs + FontWidth/2)/FontWidth/3;
        int i = ((hOffs + FontWidth / 2) / FontWidth + 2) / 13;
        int j = ((hOffs + FontWidth / 2 - i * 13 * FontWidth) / FontWidth + 1) / 3;
        if (j < 0)
            j = 0;
        if (j > 3)
            j = 3;
        offset += i * 4 + j;
    }
    else
    {
        // Inside ASCII block
        int i = __max(hOffs - BytesPerLine / 4 * 13 * FontWidth + FontWidth / 2, 0);
        offset += __min(BytesPerLine, i / FontWidth);
    }
    if (offset > Mapping.GetFileSize())
        offset = Mapping.GetFileSize();
    return offset;
}

BOOL CHexFileViewWindow::CopySelection()
{
    CALL_STACK_MESSAGE1("CHexFileViewWindow::CopySelection()");
    if (DataValid && (SelectedOffset != -1))
    {
        if (SelectedLength < (DWORD)-1)
        {
            char* data = (char*)Mapping.MapViewOfFile(SelectedOffset, (DWORD)SelectedLength);

            if (data)
            {
                if (!CopyTextToClipboard(data, (DWORD)SelectedLength, FALSE, NULL))
                {
                    SG->SalMessageBox(HWindow, LoadStr(IDS_ERROR_COPY_FAILED), LoadStr(IDS_ERROR), MB_ICONERROR);
                }
            }
            else
            {
                SG->SalMessageBox(HWindow, LoadStr(IDS_ERROR_COPY_OOM), LoadStr(IDS_ERROR), MB_ICONERROR);
            }
        }
        else
        {
            SG->SalMessageBox(HWindow, LoadStr(IDS_ERROR_COPY_LARGE_SELECTION), LoadStr(IDS_ERROR), MB_ICONERROR);
        }
    }
    return FALSE;
}

void CHexFileViewWindow::RemoveSelection(BOOL repaint)
{
    if (SelectedOffset != -1)
    {
        SelectedOffset = -1;
        SelectedLength = 0;
        if (repaint)
        {
            InvalidateRect(HWindow, NULL, TRUE);
            UpdateWindow(HWindow);
        }
    }
}

void CHexFileViewWindow::UpdateSelection(int x, int y)
{
    QWORD offset = CalculateOffset(x, y);

    if (offset != TrackingOffsetBegin)
    {
        SelectDifferenceOnButtonUp = FALSE;
        if (offset < TrackingOffsetBegin)
        {
            SelectedOffset = offset;
            SelectedLength = TrackingOffsetBegin - offset;
        }
        else
        {
            SelectedOffset = TrackingOffsetBegin;
            SelectedLength = offset - TrackingOffsetBegin;
        }
        InvalidateRect(HWindow, NULL, TRUE);
        UpdateWindow(HWindow);
        // enablujem/disablujem tlacitko pro COPY v toolbare
        CMainWindow* parent = (CMainWindow*)WindowsManager.GetWindowPtr(GetParent(HWindow));
        if (parent)
            parent->UpdateToolbarButtons(UTB_TEXTSELECTION);
    }
}

LRESULT
CHexFileViewWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CHexFileViewWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);

    // dame prilezitost k resetu akumulatoru pro mouse wheel
    ResetMouseWheelAccumulatorHandler(uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_SETFOCUS:
    {
        // informujeme parenta, ze jsme prevzali fokus
        CMainWindow* parent = (CMainWindow*)WindowsManager.GetWindowPtr(GetParent(HWindow));
        if (parent)
            parent->SetActiveFileView(ID);

        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (GetFocus() != HWindow)
            SetFocus(HWindow);
        if (DataValid)
        {
            Siblink->RemoveSelection(TRUE);
            RemoveSelection(TRUE);
            if (LOWORD(lParam) >= LineNumWidth)
            {
                QWORD offset = CalculateOffset(LOWORD(lParam), HIWORD(lParam));
                // zkusime vybrat differenci
                SelectDifferenceOnButtonUp = TRUE;
                TrackingOffsetBegin = offset;
                Tracking = TRUE;
                SetCapture(HWindow);
            }
        }
        return 0;
    }

    case WM_CANCELMODE:
    {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(HWindow, &p);
        lParam = MAKELONG(p.x, p.y);
        // fall through...
    }

    case WM_LBUTTONUP:
    {
        if (Tracking)
        {
            ReleaseCapture();
            Tracking = FALSE;
            KillTimer(HWindow, IDT_SCROLLTEXT);
            UpdateSelection((short)LOWORD(lParam), (short)HIWORD(lParam));
        }
        if (DataValid && (TrackingOffsetBegin) && SelectDifferenceOnButtonUp)
        {
            CMainWindow* parent = (CMainWindow*)WindowsManager.GetWindowPtr(GetParent(HWindow));
            if (parent)
                parent->SelectDifferenceByOffset(TrackingOffsetBegin, FALSE);
        }
        if (DataValid && Configuration.AutoCopy)
            CopySelection();
        return 0;
    }

    case WM_RBUTTONDOWN:
    {
        if (GetFocus() != HWindow)
            SetFocus(HWindow);
        return 0;
    }

    case WM_VSCROLL:
    {
        __int64 pos = ViewOffset / BytesPerLine;
        __int64 curPos = pos;
        int page = Height / FontHeight;
        __int64 size = __max(Mapping.GetFileSize(), SiblinkSize);
        __int64 maxPos = __max((size - 1 + BytesPerLine - 1) / BytesPerLine - page, 0);
        switch (LOWORD(wParam))
        {
        case SB_BOTTOM:
            pos = maxPos;
            break;

        case SB_LINEDOWN:
            if (pos < maxPos)
                pos++;
            break;

        case SB_LINEUP:
            if (pos > 0)
                pos--;
            break;

        case SB_PAGEDOWN:
            if (pos + page - 1 < maxPos)
                pos += (page - 1);
            else
                pos = maxPos;
            break;

        case SB_PAGEUP:
            if (pos - (page - 1) > 0)
                pos -= (page - 1);
            else
                pos = 0;
            break;

        case SB_THUMBTRACK:
        {
            SCROLLINFO si;
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_TRACKPOS;
            GetScrollInfo(HWindow, SB_VERT, &si);
            pos = __int64(((double)si.nTrackPos + 0.5) / ScrollScale);
            //          lParam = si.nTrackPos;
            //          pos = __int64 (((double)lParam + 0.5)/ScrollScale);
            break;
        }

        case SB_TOP:
            pos = 0;
            break;

        default:
            return 0;
        }
        SendMessage(Siblink->HWindow, WM_USER_VSCROLL, MAKELONG(SB_THUMBTRACK, pos >> 32), (LPARAM)pos);
        lParam = (LPARAM)pos;
        wParam = (WPARAM)(pos >> 32);
    }
        // pokracujem dal ...

    case WM_USER_VSCROLL:
    {
        __int64 pos = lParam + ((__int64)HIWORD(wParam) << 32);
        __int64 curPos = ViewOffset / BytesPerLine;
        int page = Height / FontHeight;
        if (pos != curPos)
        {
            if (Abs64(pos - curPos) < page - 1 && pos % BytesPerLine == curPos % BytesPerLine)
            {
                ScrollWindowEx(HWindow, 0, (int)((curPos - pos) * FontHeight), NULL, NULL, NULL, NULL, SW_INVALIDATE);
            }
            else
                InvalidateRect(HWindow, NULL, FALSE);
            ViewOffset = pos * BytesPerLine;
            SetScrollPos(HWindow, SB_VERT, (int)(pos * ScrollScale + 0.5), TRUE);
            UpdateWindow(HWindow);
        }
        return 0;
    }

    case WM_HSCROLL:
        // aby nedochazelo ke konfliktu SB_FASTLEFT a SB_FASTRIGHT
        // se standardnimi parametry
        if (!TestHScrollWParam(wParam))
            return 0;
        if (LOWORD(wParam) == SB_THUMBTRACK)
        {
            SCROLLINFO si;
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_TRACKPOS;
            GetScrollInfo(HWindow, SB_HORZ, &si);
            lParam = si.nTrackPos;
        }
        wParam &= 0xFFFF;
        SendMessage(Siblink->HWindow, WM_USER_HSCROLL, wParam, lParam);
        // pokracujem dal ...

    case WM_USER_HSCROLL:
    {
        int pos = GetScrollPos(HWindow, SB_HORZ);
        int width = __min(Width, SiblinkWidth) - LineNumWidth;
        int page = width / FontWidth;
        if (page < 1)
        {
            page = 1;
        }
        int maxPos = __max(BytesPerLine / 4 * 17 - page, 0);
        switch (LOWORD(wParam))
        {
        case SB_RIGHT:
            pos = maxPos;
            break;

        case SB_LINERIGHT:
            if (pos < maxPos)
                ++pos;
            break;

        case SB_FASTRIGHT:
            pos = pos + FAST_LEFTRIGHT < maxPos ? pos + FAST_LEFTRIGHT : maxPos;
            break;

        case SB_LINELEFT:
            if (pos > 0)
                pos--;
            break;

        case SB_FASTLEFT:
            pos = pos > FAST_LEFTRIGHT ? pos - FAST_LEFTRIGHT : 0;
            break;

        case SB_PAGERIGHT:
            if (page == 1)
                page++;
            if (pos + page - 1 < maxPos)
                pos += (page - 1);
            else
                pos = maxPos;
            break;

        case SB_PAGELEFT:
            if (page == 1)
                page++;
            if (pos - (page - 1) > 0)
                pos -= (page - 1);
            else
                pos = 0;
            break;

        case SB_THUMBTRACK:
            pos = int(lParam);
            break;

        case SB_LEFT:
            pos = 0;
            break;

        default:
            return 0;
        }
        if (pos != HScrollOffs)
        {
            if (abs(pos - HScrollOffs) < page - 1)
            {
                RECT r;
                r.left = LineNumWidth;
                r.right = Width;
                r.top = 0;
                r.bottom = Height;
                ScrollWindowEx(HWindow, (HScrollOffs - pos) * FontWidth, 0, &r, &r, NULL, NULL, SW_INVALIDATE);
            }
            else
                InvalidateRect(HWindow, NULL, FALSE);
            HScrollOffs = pos;
            UpdateWindow(HWindow);
        }
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE;
        si.nMin = 0;
        si.nMax = BytesPerLine / 4 * 17 - 1;
        si.nPos = HScrollOffs;
        SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);
        //SetScrollPos(HWindow, SB_HORZ, pos, TRUE);
        return 0;
    }

    case WM_USER_HANDLEFILEERROR:
    {
        char buf[1024];
        strcpy(buf, LoadStr(IDS_ACCESFILE2));
        if (lParam != ERROR_SUCCESS)
        {
            int l = lstrlen(buf);
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, DWORD(lParam),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
        }
        if (SG->DialogError(GetParent(HWindow), BUTTONS_RETRYCANCEL, (char*)wParam, buf, LoadStr(IDS_ERROR)) != DIALOG_RETRY)
        {
            PostMessage(GetParent(HWindow), WM_COMMAND, CM_CLOSEFILES, 0);
            return 0;
        }
        EnablePaint();
        ((CHexFileViewWindow*)Siblink)->EnablePaint();
        InvalidateRect(HWindow, NULL, FALSE);
        UpdateWindow(HWindow);
        InvalidateRect(Siblink->HWindow, NULL, FALSE);
        UpdateWindow(Siblink->HWindow);
        return 0;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case CM_SELECTALL:
        {
            if (DataValid)
            {
                Siblink->RemoveSelection(TRUE);
                SelectedOffset = 0;
                SelectedLength = Mapping.GetFileSize();
                InvalidateRect(HWindow, NULL, FALSE);
                UpdateWindow(HWindow);

                CMainWindow* parent = (CMainWindow*)WindowsManager.GetWindowPtr(GetParent(HWindow));
                if (parent)
                    parent->UpdateToolbarButtons(UTB_TEXTSELECTION);
                if (Configuration.AutoCopy)
                    CopySelection();
            }
            return 0;
        }

        case CM_COPY:
            CopySelection();
            return 0;
        }
    }
    }

    return CFileViewWindow::WindowProc(uMsg, wParam, lParam);
} /* CHexFileViewWindow::WindowProc */
