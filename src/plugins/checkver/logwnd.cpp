// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "checkver.h"
#include "checkver.rh"
#include "checkver.rh2"
#include "lang\lang.rh"

#define LEFT_MARGIN 8
#define TOP_MARGIN 4

// from windowsx.h
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

HFONT HNormalFont = NULL; // normalni
HFONT HBoldFont = NULL;   // tucny
int LineHeight = 0;
HCURSOR HHandCursor = NULL; // kurzor - ruka
HBRUSH HBkBrush = NULL;
BOOL ClassIsRegistred = FALSE;

HWND HWindow = NULL;
RECT ClientRect;
int YOffset = 0;

const char* LOGWINDOW_CLASSNAME = "CheckVerLogWindow";

TDirectArray<char*> LogLines(100, 50); // radky drzene v log okne

LRESULT APIENTRY LogWindowProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ReleaseLogLines()
{
    int i;
    for (i = 0; i < LogLines.Count; i++)
        free(LogLines[i]);
    LogLines.DestroyMembers();
}

BOOL RegisterLogClass()
{
    if (ClassIsRegistred)
        return TRUE;
    WNDCLASS wc;
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = LogWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = HLanguage;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = LOGWINDOW_CLASSNAME;
    if (RegisterClass(&wc) == 0)
    {
        DWORD err = GetLastError();
        TRACE_E("RegisterClass has failed error=" << err);
        return FALSE;
    }
    else
        ClassIsRegistred = TRUE;
    return TRUE;
}

void UnregisterLogClass()
{
    if (ClassIsRegistred)
    {
        if (!UnregisterClass(LOGWINDOW_CLASSNAME, HLanguage))
        {
            DWORD err = GetLastError();
            TRACE_E("UnregisterClass(LOGWINDOW_CLASSNAME) has failed error=" << err);
        }
        ClassIsRegistred = FALSE;
    }
}

void SetVScrollBarInfo(BOOL redraw)
{
    SCROLLINFO sc;
    sc.cbSize = sizeof(sc);
    sc.fMask = SIF_DISABLENOSCROLL | SIF_RANGE | SIF_PAGE;
    sc.nMin = 0;
    sc.nMax = LogLines.Count * LineHeight + 2 * TOP_MARGIN;
    sc.nPage = ClientRect.bottom - ClientRect.top + 1;
    SetScrollInfo(HWindow, SB_VERT, &sc, redraw);
}

// alokuje prostredky potrbne pro zobrazeni vieweru
BOOL InitializeLogWindow(HWND hWindow)
{
    LOGFONT lf;
    GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);

    HNormalFont = CreateFontIndirect(&lf);
    lf.lfWeight = FW_BOLD;
    HBoldFont = CreateFontIndirect(&lf);

    TEXTMETRIC tm;
    HDC hdc = GetDC(hWindow);
    HFONT hOldFont = (HFONT)SelectObject(hdc, HNormalFont);
    GetTextMetrics(hdc, &tm);
    SelectObject(hdc, hOldFont);
    ReleaseDC(hWindow, hdc);
    LineHeight = tm.tmHeight + 2;

    HHandCursor = LoadCursor(NULL, IDC_HAND);
    HBkBrush = GetSysColorBrush(COLOR_WINDOW); // neni treba uvolnovat

    if (HNormalFont == NULL || HBoldFont == NULL ||
        HHandCursor == NULL || HBkBrush == NULL)
    {
        TRACE_E("Unable to initialize resources");
        return FALSE;
    }

    SetVScrollBarInfo(TRUE);
    GetClientRect(hWindow, &ClientRect);
    YOffset = 0;

    HWindow = hWindow;

    return TRUE;
}

void ReleaseLogWindow(HWND hWindow)
{
    if (HNormalFont != NULL)
    {
        DeleteObject(HNormalFont);
        HNormalFont = NULL;
    }
    if (HBoldFont != NULL)
    {
        DeleteObject(HBoldFont);
        HBoldFont = NULL;
    }
    if (HHandCursor != NULL)
    {
        DestroyCursor(HHandCursor);
        HHandCursor = NULL;
    }
    ReleaseLogLines();
    YOffset = 0;

    HWindow = NULL;
}

//****************************************************************************
//
// FlexWriteText
//

BOOL FlexWriteText(HDC hDC, int x, int y, const char* text, int cchText, int* hitIndex)
{
    //schovej nastavení
    HFONT hOldFont = (HFONT)SelectObject(hDC, HNormalFont);
    COLORREF oldColor = SetTextColor(hDC, GetSysColor(COLOR_BTNTEXT));
    SIZE size;

    BOOL link = FALSE;
    int hitX = x;
    if (hitIndex != NULL)
        x = 0;

    int len = 0; // pocet znaku k vykresleni

    int ix;
    for (ix = 0; text[ix] != '\0'; ix++)
    {
        if (text[ix] != '\t')
        {
            len = 0;
            while (*(text + ix + len) != '\t' && *(text + ix + len) != 0)
                len++;
            GetTextExtentPoint32(hDC, text + ix, len, &size);
            if (hitIndex != NULL)
            {
                if (hitX >= x && hitX < x + size.cx && link)
                {
                    *hitIndex = ix;
                    return TRUE;
                }
            }
            else
                TextOut(hDC, x, y, text + ix, len);
            ix += len - 1;
            x += size.cx;
        }
        else
        {
            ix++;
            switch (text[ix])
            {
            case 'n':
            {
                SelectObject(hDC, HNormalFont);
                SetTextColor(hDC, GetSysColor(COLOR_BTNTEXT));
                link = FALSE;
                break;
            }

            case 'b':
            {
                SelectObject(hDC, HBoldFont);
                SetTextColor(hDC, GetSysColor(COLOR_BTNTEXT));
                link = FALSE;
                break;
            }

            case 'u':
            {
                SelectObject(hDC, HNormalFont);
                SetTextColor(hDC, RGB(0, 0, 255));
                link = TRUE;
                break;
            }

            case 'l':
            {
                // link - sezeru URL
                while (text[ix] != 0 && text[ix + 1] != '\t')
                    ix++;
                if (text[ix] != 0 && text[ix + 1] == '\t' && text[ix + 2] == 'i')
                {
                    // sezereme item name
                    ix += 2;
                    while (text[ix] != 0 && text[ix + 1] != '\t')
                        ix++;
                }
                break;
            }

            case '\t':
            {
                len = 0;
                while (*(text + ix + len) != '\t' && *(text + ix + len) != 0)
                    len++;
                GetTextExtentPoint32(hDC, text + ix, len, &size);
                if (hitIndex != NULL)
                {
                    if (hitX >= x && hitX < x + size.cx && link)
                    {
                        *hitIndex = ix;
                        return TRUE;
                    }
                }
                else
                    TextOut(hDC, x, y, text + ix, len);
                ix += len - 1;
                x += size.cx;
                break;
            }
            }
        }
    }

    SetTextColor(hDC, oldColor);
    SelectObject(hDC, hOldFont);
    return FALSE;
}

void DrawLine(HDC hDC, int lineIndex, int yOffset)
{
    const char* line = LogLines[lineIndex];
    FlexWriteText(hDC, LEFT_MARGIN, yOffset + 1, line, lstrlen(line), NULL);
}

void OnPaint(HDC hDC, BOOL lastOnly)
{
    HFONT hOldFont = (HFONT)SelectObject(hDC, HNormalFont);
    int oldBkMode = SetBkMode(hDC, TRANSPARENT);
    int yOffset = -YOffset + TOP_MARGIN;
    int height = ClientRect.bottom - ClientRect.top;

    int top = 0;
    int bottom = height;
    RECT clipRect;
    int clipRectType = GetClipBox(hDC, &clipRect);
    if (clipRectType == SIMPLEREGION || clipRectType == COMPLEXREGION)
    {
        top = clipRect.top;
        bottom = clipRect.bottom;
    }

    int i;
    for (i = 0; i < LogLines.Count; i++)
    {
        if ((yOffset + LineHeight) > top && yOffset < bottom &&
            (!lastOnly || i == LogLines.Count - 2 || i == LogLines.Count - 1))
        {
            DrawLine(hDC, i, yOffset);
        }
        yOffset += LineHeight;
        if (yOffset > height)
            break;
    }
    SetBkMode(hDC, oldBkMode);
    SelectObject(hDC, hOldFont);
}

void ClearLogWindow()
{
    ReleaseLogLines();
    SetVScrollBarInfo(FALSE);
    SetScrollPos(HWindow, SB_VERT, 0, TRUE); //nastavime pozici scrolleru
    YOffset = 0;
    InvalidateRect(HWindow, NULL, TRUE);
    UpdateWindow(HWindow);
}

void AddLogLine(const char* line, BOOL scrollToEnd)
{
    int len = lstrlen(line);
    char* tmp = (char*)malloc(len + 1);
    if (tmp == NULL)
        return;
    memmove(tmp, line, len + 1);
    LogLines.Add(tmp);

    int yOffset = -YOffset + TOP_MARGIN;
    yOffset += (LogLines.Count - 1) * LineHeight;
    RECT r;
    r.left = 0;
    r.top = yOffset;
    r.right = ClientRect.right;
    r.bottom = r.top + LineHeight;
    InvalidateRect(HWindow, &r, TRUE);
    r.left = 0;
    r.top -= LineHeight; // sejmeme sipku
    r.right = LEFT_MARGIN;
    r.bottom -= LineHeight;
    InvalidateRect(HWindow, &r, TRUE);
    UpdateWindow(HWindow);
    SetVScrollBarInfo(!scrollToEnd);
    if (scrollToEnd)
        SendMessage(HWindow, WM_VSCROLL, SB_THUMBPOSITION | 0xFFFF0000, 0);
}

int GetLineIndexFromPoint(int y)
{
    int yOffset = -YOffset + TOP_MARGIN;
    int height = ClientRect.bottom - ClientRect.top;
    int i;
    for (i = 0; i < LogLines.Count; i++)
    {
        if (y >= yOffset && y < yOffset + LineHeight)
            return i;
        yOffset += LineHeight;
    }
    return -1;
}

BOOL LinkHitTest(int x, int y, char* link, char* item)
{
    if (link != NULL)
        *link = 0;
    if (item != NULL)
        *item = 0;
    BOOL ret = FALSE;
    int lineIndex = GetLineIndexFromPoint(y);
    if (lineIndex != -1)
    {
        const char* line = LogLines[lineIndex];
        int charIndex;
        HDC hDC = GetDC(HWindow);
        if (FlexWriteText(hDC, x - LEFT_MARGIN, 0, line, lstrlen(line), &charIndex))
        {
            //      TRACE_I("LineIndex="<<lineIndex<<" CharIndex="<<charIndex);
            if (link != NULL)
            {
                // najdu "\t"
                const char* begin = line + charIndex;
                while (*begin != 0 && *begin != '\t')
                    begin++;
                if (*begin == '\t')
                    begin += 2;
                {
                    const char* end = begin;
                    while (*end != 0 && *end != '\t')
                        end++;
                    int len = (int)(end - begin);
                    if (len > 0)
                    {
                        memmove(link, begin, len);
                        link[len] = 0;
                        if (item != NULL && *end == '\t')
                        {
                            end++;
                            if (*end == 'i')
                            {
                                end++;
                                begin = end;
                                while (*end != 0 && *end != '\t')
                                    end++;
                                len = (int)(end - begin);
                                if (len > 0)
                                {
                                    memmove(item, begin, len);
                                    item[len] = 0;
                                }
                            }
                        }
                    }
                    else
                        TRACE_E("Link was not found");
                }
            }
            ret = TRUE;
        }
        ReleaseDC(HWindow, hDC);
    }
    return ret;
}

LRESULT APIENTRY
LogWindowProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE5("LogWindowProc(0x%p, 0x%X, 0x%IX, 0x%IX)", hWindow, uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_ERASEBKGND:
    {
        FillRect(HDC(wParam), &ClientRect, HBkBrush);
        return TRUE;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hWindow, &ps);
        OnPaint(hDC, FALSE);
        EndPaint(hWindow, &ps);
        return 0;
    }

    case WM_SETCURSOR:
    {
        DWORD pos = GetMessagePos();
        POINT p;
        p.x = GET_X_LPARAM(pos);
        p.y = GET_Y_LPARAM(pos);
        ScreenToClient(HWindow, &p);
        BOOL link = LinkHitTest(p.x, p.y, NULL, NULL);
        if (link && GetCapture() == NULL)
            SetCursor(HHandCursor);
        else
            SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    }

    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        char buff[1024];
        char item[1024];
        BOOL link = LinkHitTest(x, y, buff, item);
        if (link)
        {
            if (strcmp(buff, "DETAILS") != 0 && strcmp(buff, "FILTER") != 0)
                ShellExecute(hWindow, "open", buff, NULL, NULL, SW_SHOWNORMAL);
            else
            {
                if (ModulesHasCorrectData())
                {
                    if (strcmp(buff, "DETAILS") == 0)
                    {
                        int index = atoi(item);
                        ModulesChangeShowDetails(index);
                        ClearLogWindow(); // vycisteni logu pred vypisem novych modulu
                        ModulesCreateLog(NULL, FALSE);
                    }
                    else
                    {
                        char* itemVer = strchr(item, '|');
                        if (itemVer != NULL)
                            *itemVer = ' ';
                        char buff2[1024];
                        sprintf(buff2, LoadStr(IDS_FILTERVER_CONFIRM), item);
                        if (itemVer != NULL)
                            *itemVer = '|';
                        int ret = SalGeneral->SalMessageBox(HWindow, buff2, LoadStr(IDS_PLUGINNAME),
                                                            MB_ICONQUESTION | MB_YESNO);
                        if (ret == IDYES)
                        {
                            AddUniqueFilter(item);
                            if (ModulesHasCorrectData())
                            {
                                ClearLogWindow(); // vycisteni logu pred vypisem novych modulu
                                ModulesCreateLog(NULL, FALSE);
                            }
                        }
                    }
                }
            }
        }
        break;
    }

    case WM_RBUTTONDOWN:
    {
        POINT p;
        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
        char link[1024];
        char item[1024];
        LinkHitTest(p.x, p.y, link, item);
        char* itemVer = strchr(item, '|');
        if (itemVer != NULL)
            *itemVer = 0;
        if (link[0] != 0 && strcmp(link, "DETAILS") != 0 && strcmp(link, "FILTER") != 0)
        {
            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani AppendMenu() dole...
MENU_TEMPLATE_ITEM LogWindowMenu1[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_CTXMENU_OPEN
	{MNTT_IT, IDS_CTXMENU_COPY
	{MNTT_IT, IDS_CTXMENU_FILTER
	{MNTT_PE, 0
};
MENU_TEMPLATE_ITEM LogWindowMenu2[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_CTXMENU_DOWNLOAD
	{MNTT_IT, IDS_CTXMENU_COPY
	{MNTT_IT, IDS_CTXMENU_FILTER
	{MNTT_PE, 0
};
*/
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING | MF_ENABLED, 1, LoadStr(item[0] == 0 ? IDS_CTXMENU_OPEN : IDS_CTXMENU_DOWNLOAD));
            AppendMenu(hMenu, MF_STRING | MF_ENABLED, 2, LoadStr(IDS_CTXMENU_COPY));
            if (item[0] != 0)
            {
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
                char buff[1024];
                sprintf(buff, LoadStr(IDS_CTXMENU_FILTER), item);
                AppendMenu(hMenu, MF_STRING | (HConfigurationDialog != NULL ? MF_GRAYED : MF_ENABLED), 3, buff);
                if (itemVer != NULL)
                {
                    *itemVer = ' ';
                    sprintf(buff, LoadStr(IDS_CTXMENU_FILTER), item);
                    *itemVer = 0;
                    AppendMenu(hMenu, MF_STRING | (HConfigurationDialog != NULL ? MF_GRAYED : MF_ENABLED), 4, buff);
                }
            }
            ClientToScreen(HWindow, &p);
            BOOL cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, HWindow, NULL);
            DestroyMenu(hMenu);
            if (cmd != 0)
            {
                switch (cmd)
                {
                case 1:
                {
                    ShellExecute(hWindow, "open", link, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }

                case 2:
                {
                    SalGeneral->CopyTextToClipboard(link, -1, TRUE, HWindow);
                    break;
                }

                case 3:
                case 4:
                {
                    char buff[1024];
                    if (cmd == 4)
                        *itemVer = ' ';
                    sprintf(buff, LoadStr(cmd == 4 ? IDS_FILTERVER_CONFIRM : IDS_FILTER_CONFIRM), item);
                    if (cmd == 4)
                        *itemVer = '|';
                    int ret = SalGeneral->SalMessageBox(HWindow, buff, LoadStr(IDS_PLUGINNAME),
                                                        MB_ICONQUESTION | MB_YESNO);
                    if (ret == IDYES)
                    {
                        AddUniqueFilter(item);
                        if (ModulesHasCorrectData())
                        {
                            ClearLogWindow(); // vycisteni logu pred vypisem novych modulu
                            ModulesCreateLog(NULL, FALSE);
                        }
                    }
                    break;
                }
                }
                return 0;
            }
        }
        break;
    }

    case WM_VSCROLL:
    {
        int yPos = GetScrollPos(hWindow, SB_VERT);
        int oldYPos = yPos;

        int line = LineHeight;
        int page = ClientRect.bottom - ClientRect.top - LineHeight;

        switch (LOWORD(wParam))
        {
        case SB_LINEDOWN:
            yPos += line;
            break;
        case SB_LINEUP:
            yPos -= line;
            break;
        case SB_PAGEDOWN:
            yPos += page;
            break;
        case SB_PAGEUP:
            yPos -= page;
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            yPos = HIWORD(wParam);
            break;
        }

        if (yPos != oldYPos)
        {
            SetScrollPos(hWindow, SB_VERT, yPos, TRUE); //nastavime pozici scrolleru
            int newPos = GetScrollPos(hWindow, SB_VERT);
            YOffset = newPos;
            if (LogLines.Count * LineHeight + 2 * TOP_MARGIN <=
                ClientRect.bottom - ClientRect.top)
                YOffset = 0;
            int delta = oldYPos - newPos;
            if (delta != 0)
            {
                ScrollWindow(hWindow, 0, delta, NULL, NULL); //odscrollujeme okno
                UpdateWindow(hWindow);
            }
        }
        return 0;
    }

    case WM_SIZE:
    {
        GetClientRect(hWindow, &ClientRect);
        break;
    }
    }

    return DefWindowProc(hWindow, uMsg, wParam, lParam);
}
