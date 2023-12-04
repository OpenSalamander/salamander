// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <iterator>

#undef min
#undef max

using namespace std;

HCURSOR HIbeamCursor;
HCURSOR HArrowCursor;

const char* FILEVIEWWINDOW_CLASSNAME = "SFC File View Window Class";

// ****************************************************************************
//
// CFileViewWindow
//

BOOL TestHScrollWParam(WPARAM wParam)
{
    CALL_STACK_MESSAGE_NONE
    switch (LOWORD(wParam))
    {
    case SB_RIGHT:
    case SB_LINERIGHT:
    case SB_LINELEFT:
    case SB_PAGERIGHT:
    case SB_PAGELEFT:
    case SB_THUMBTRACK:
    case SB_LEFT:

        return TRUE;

    default:
        return FALSE;
    }
}

CFileViewWindow::CFileViewWindow(CFileViewID id, CFileViewType type)
{
    CALL_STACK_MESSAGE1("CFileViewWindow::CFileViewWindow()");
    ID = id;
    Type = type;
    DataValid = FALSE;
    HFont = NULL;
    FontWidth = 0;
    LineNumDigits = 0;
    LineNumWidth = (LineNumDigits + 1) * FontWidth + BORDER_WIDTH;

    // nastavime barevna schemata
    memset(LineColors, 0, sizeof(LineColors));

    Siblink = NULL;

    Tracking = FALSE;

    ResetMouseWheelAccumulator();
}

CFileViewWindow::~CFileViewWindow()
{
    CALL_STACK_MESSAGE1("CFileViewWindow::~CFileViewWindow()");
    DestroyData();
    if (HFont)
        DeleteObject(HFont);

    int i;
    for (i = 0; i < 3; i++)
    {
        if (LineColors[i].LineNumBorderPen)
            DeleteObject(LineColors[i].LineNumBorderPen);
        if (LineColors[i].BorderPen)
            DeleteObject(LineColors[i].BorderPen);
    }
}

void CFileViewWindow::InvalidateData()
{
    CALL_STACK_MESSAGE_NONE
    DestroyData();
    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
}

void CFileViewWindow::ReloadConfiguration(DWORD flags, BOOL updateWindow)
{
    CALL_STACK_MESSAGE3("CFileViewWindow::ReloadConfiguration(0x%X, %d)", flags,
                        updateWindow);
    if (flags & CC_COLORS)
    {
        // nastavime barevna schemata
        int j;
        for (j = 0; j < 3; j++)
        {
            if (LineColors[j].LineNumBorderPen)
                DeleteObject(LineColors[j].LineNumBorderPen);
            if (LineColors[j].BorderPen)
                DeleteObject(LineColors[j].BorderPen);
        }

        LineColors[LC_NORMAL].LineNumFgColor = GetPALETTERGB(Colors[LINENUM_FG_NORMAL]);
        LineColors[LC_NORMAL].LineNumBkColor = GetPALETTERGB(Colors[LINENUM_BK_NORMAL]);
        LineColors[LC_NORMAL].LineNumBorderPen = CreatePen(PS_DOT, 0, GetPALETTERGB(Colors[LINENUM_FG_NORMAL]));
        LineColors[LC_NORMAL].FgColor = GetPALETTERGB(Colors[TEXT_FG_NORMAL]);
        LineColors[LC_NORMAL].BkColor = GetPALETTERGB(Colors[TEXT_BK_NORMAL]);
        LineColors[LC_NORMAL].BorderPen = CreatePen(PS_DOT, 0, GetPALETTERGB(Colors[TEXT_FG_NORMAL]));

        int i = ID == fviLeft ? 0 : 1;
        LineColors[LC_CHANGE].LineNumFgColor = GetPALETTERGB(Colors[LINENUM_FG_LEFT_CHANGE + i]);
        LineColors[LC_CHANGE].LineNumBkColor = GetPALETTERGB(Colors[LINENUM_BK_LEFT_CHANGE + i]);
        LineColors[LC_CHANGE].FgColor = GetPALETTERGB(Colors[TEXT_FG_LEFT_CHANGE + i]);
        LineColors[LC_CHANGE].BkColor = GetPALETTERGB(Colors[TEXT_BK_LEFT_CHANGE + i]);
        LineColors[LC_CHANGE].FgCommon = GetPALETTERGB(Colors[TEXT_FG_NORMAL]);
        LineColors[LC_CHANGE].BkCommon = GetPALETTERGB(Colors[TEXT_BK_NORMAL]);

        LineColors[LC_FOCUS].LineNumFgColor = GetPALETTERGB(Colors[LINENUM_FG_LEFT_CHANGE_FOCUSED + i]);
        LineColors[LC_FOCUS].LineNumBkColor = GetPALETTERGB(Colors[LINENUM_BK_LEFT_CHANGE_FOCUSED + i]);
        LineColors[LC_FOCUS].LineNumBorderPen = CreatePen(PS_SOLID, 0, GetPALETTERGB(Colors[LINENUM_LEFT_BORDER + i]));
        LineColors[LC_FOCUS].FgColor = GetPALETTERGB(Colors[TEXT_FG_LEFT_CHANGE_FOCUSED + i]);
        LineColors[LC_FOCUS].BkColor = GetPALETTERGB(Colors[TEXT_BK_LEFT_CHANGE_FOCUSED + i]);
        LineColors[LC_FOCUS].BorderPen = CreatePen(PS_SOLID, 0, GetPALETTERGB(Colors[TEXT_LEFT_BORDER + i]));
        LineColors[LC_FOCUS].FgCommon = GetPALETTERGB(Colors[TEXT_FG_LEFT_FOCUSED + i]);
        LineColors[LC_FOCUS].BkCommon = GetPALETTERGB(Colors[TEXT_BK_LEFT_FOCUSED + i]);
    }

    if (flags & CC_FONT)
    {
        // nacteme font
        HDC hdc = GetDC(NULL);
        if (HFont)
            DeleteObject(HFont);
        HFont = CreateFontIndirect(&Configuration.FileViewLogFont);
        HFONT oldFont = (HFONT)SelectObject(hdc, HFont);
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        FontHeight = tm.tmHeight + tm.tmExternalLeading;
        FontWidth = tm.tmAveCharWidth;
        SelectObject(hdc, oldFont);
        ReleaseDC(NULL, hdc);

        MappedASCII8TextOut.FontHasChanged(&Configuration.FileViewLogFont, HFont, FontWidth, FontHeight);

        LineNumWidth = (LineNumDigits + 1) * FontWidth + BORDER_WIDTH;

        if (DataValid)
            UpdateScrollBars(FALSE);
    }

    if (updateWindow)
    {
        InvalidateRect(HWindow, NULL, FALSE);
        UpdateWindow(HWindow);
    }
}

void CFileViewWindow::ResetMouseWheelAccumulatorHandler(UINT uMsg, WPARAM wParam, LPARAM lParam)
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

#define WM_MOUSEHWHEEL 0x020E

LRESULT
CFileViewWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CFileViewWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                             wParam, lParam);
    if (CheckMouseWheelMsg(HWindow, uMsg, wParam, lParam))
        return 0;
    switch (uMsg)
    {
    case WM_USER_CREATE:
    {
        RECT cr;
        GetClientRect(HWindow, &cr);
        SiblinkWidth = Width = cr.right;
        Height = cr.bottom;
        ReloadConfiguration(CC_ALL, FALSE);

        return 0;
    }

    case WM_PAINT:
    {
        if (DataValid)
            Paint();
        else
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(HWindow, &ps);
            RECT r;
            GetClientRect(HWindow, &r);
            SetBkColor(dc, LineColors[LC_NORMAL].BkColor);
            ExtTextOut(dc, 0, 0, ETO_OPAQUE /*| ETO_CLIPPED*/, &r, NULL, 0, NULL);
            EndPaint(HWindow, &ps);
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
    {
        Width = LOWORD(lParam);
        if ((Width != 0) || (((CMainWindow*)WindowsManager.GetWindowPtr(GetParent(HWindow)))->SplitBar->GetType() != sbVertical))
        {
            // When one panel is maximized, the other one gets height higher by the header
            // -> scrolling gets screwed up -> we ignore the new height
            Height = HIWORD(lParam);
        }
        return 0;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        DWORD wScrollNotify = -1;
        UINT msg = WM_VSCROLL;

        switch (wParam)
        {
        // vertikalni scrollovani
        case 'K':
        case VK_UP:
            if (!shiftPressed && !altPressed)
                wScrollNotify = SB_LINEUP;
            break;

        case 'B':
        case VK_PRIOR:
            if (!ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_PAGEUP;
            if (ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_TOP;
            break;

        case 'F':
        case VK_NEXT:
            if (!ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_PAGEDOWN;
            if (ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_BOTTOM;
            break;

        case 'J':
        case VK_DOWN:
            if (!shiftPressed && !altPressed)
                wScrollNotify = SB_LINEDOWN;
            break;

        case VK_HOME:
            if (!ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_TOP;
            if (ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_TOP;
            break;

        case 'G':
            if (!ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_TOP;
            if (!ctrlPressed && shiftPressed && !altPressed)
                wScrollNotify = SB_BOTTOM;
            break;

        case VK_END:
            if (!ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_BOTTOM;
            if (ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_BOTTOM;
            break;

        // horizontalni scrollovani
        case 'L':
        case VK_RIGHT:
            if (!ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_LINERIGHT;
            if (ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_FASTRIGHT;
            msg = WM_USER_HSCROLL;
            break;

        case 'H':
        case VK_LEFT:
            if (!ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_LINELEFT;
            if (ctrlPressed && !shiftPressed && !altPressed)
                wScrollNotify = SB_FASTLEFT;
            msg = WM_USER_HSCROLL;
            break;
        }

        if (wScrollNotify != -1)
        {
            SendMessage(HWindow, msg, MAKELONG(wScrollNotify, FALSE), 0);
            if (msg != WM_VSCROLL)
                SendMessage(Siblink->HWindow, msg, MAKELONG(wScrollNotify, FALSE), 0);
            return 0;
        }

        break;
    }

    case WM_USER_SETCURSOR:
    {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(HWindow, &pt);
        if (pt.x >= LineNumWidth && pt.x <= Width && pt.y >= 0 && pt.y <= Height && HIWORD(lParam) != 0)
        {
            SetCursor(HIbeamCursor);
            return TRUE;
        }
        /*else
      {
        SetCursor(HArrowCursor);
      }*/
        return FALSE;
    }

    case WM_USER_MOUSEWHEEL:
    {
        short zDelta = (short)HIWORD(wParam);
        if ((zDelta < 0 && MouseWheelAccumulator > 0) || (zDelta > 0 && MouseWheelAccumulator < 0))
            ResetMouseWheelAccumulator(); // pri zmene smeru otaceni kolecka je potreba nulovat akumulator

        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        // ctrl+wheel -- skakani po diferencich
        if (controlPressed && !altPressed && !shiftPressed)
        {
            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerDiff = max(1, (1000 * WHEEL_DELTA));
            int diffsToScroll = MouseWheelAccumulator / stepsPerDiff;
            if (diffsToScroll != 0)
            {
                MouseWheelAccumulator -= diffsToScroll * stepsPerDiff;
                PostMessage(GetParent(HWindow), WM_COMMAND, diffsToScroll > 0 ? CM_PREVDIFF : CM_NEXTDIFF, 0);
            }
        }

        // wheel bez modifikatoru: standardni scroll
        if (!controlPressed && !altPressed && !shiftPressed)
        {
            DWORD wheelScroll = SG->GetMouseWheelScrollLines(); // muze byt az WHEEL_PAGESCROLL(0xffffffff)
            DWORD pageHeight = (DWORD)(Height / FontHeight);
            wheelScroll = max(1, (int)min(wheelScroll, pageHeight - 1)); // omezime maximalne na delku stranky

            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerLine = max(1, (int)((1000 * WHEEL_DELTA) / wheelScroll));
            int linesToScroll = MouseWheelAccumulator / stepsPerLine;
            if (linesToScroll != 0)
            {
                MouseWheelAccumulator -= linesToScroll * stepsPerLine;

                if ((DWORD)abs(linesToScroll) >= pageHeight)
                {
                    PostMessage(HWindow, WM_VSCROLL, linesToScroll > 0 ? SB_PAGEUP : SB_PAGEDOWN, 0);
                }
                else
                {
                    DWORD i;
                    for (i = 0; i < (DWORD)abs(linesToScroll); i++)
                        PostMessage(HWindow, WM_VSCROLL, linesToScroll > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
                }
            }
        }

        // shift+wheel: horizontalni scroll
        if (!controlPressed && !altPressed && shiftPressed)
        {
            DWORD wheelScroll = SG->GetMouseWheelScrollLines(); // muze byt az WHEEL_PAGESCROLL(0xffffffff)
            DWORD pageWidth = max(1, int(Width / FontWidth));
            wheelScroll = max(1, (int)min((int)wheelScroll, (int)(pageWidth - 1))); // omezime maximalne na delku stranky

            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerChar = max(1, (int)((1000 * WHEEL_DELTA) / wheelScroll));
            int charsToScroll = MouseWheelAccumulator / stepsPerChar;
            if (charsToScroll != 0)
            {
                MouseWheelAccumulator -= charsToScroll * stepsPerChar;

                if ((DWORD)abs(charsToScroll) >= pageWidth)
                {
                    PostMessage(HWindow, WM_HSCROLL, charsToScroll > 0 ? SB_PAGEUP : SB_PAGEDOWN, 0);
                }
                else
                {
                    DWORD i;
                    for (i = 0; i < (DWORD)abs(charsToScroll); i++)
                        PostMessage(HWindow, WM_HSCROLL, charsToScroll > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
                }
            }
        }
        return FALSE;
    }

    case WM_MOUSEHWHEEL:
    {
        short zDelta = (short)HIWORD(wParam);

        if ((zDelta < 0 && MouseHWheelAccumulator > 0) || (zDelta > 0 && MouseHWheelAccumulator < 0))
            ResetMouseWheelAccumulator(); // pri zmene smeru naklapeni kolecka je potreba nulovat akumulator

        DWORD wheelScroll = SG->GetMouseWheelScrollChars(); // muze byt az WHEEL_PAGESCROLL(0xffffffff)
        DWORD pageWidth = max(1, int(Width / FontWidth));
        wheelScroll = max(1, (int)min((int)wheelScroll, (int)(pageWidth - 1))); // omezime maximalne na delku stranky

        MouseHWheelAccumulator += 1000 * zDelta;
        int stepsPerChar = max(1, (int)((1000 * WHEEL_DELTA) / wheelScroll));
        int charsToScroll = MouseHWheelAccumulator / stepsPerChar;
        if (charsToScroll != 0)
        {
            MouseHWheelAccumulator -= charsToScroll * stepsPerChar;

            if ((DWORD)abs(charsToScroll) >= pageWidth)
            {
                PostMessage(HWindow, WM_HSCROLL, charsToScroll < 0 ? SB_PAGEUP : SB_PAGEDOWN, 0);
            }
            else
            {
                DWORD i;
                for (i = 0; i < (DWORD)abs(charsToScroll); i++)
                    SendMessage(HWindow, WM_HSCROLL, charsToScroll < 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            }
        }
        return TRUE; // udalost jsme zpracovali a nemame zajem o emulaci pomoci klikani na scrollbar (stane se v pripade vraceni FALSE)
    }

    case WM_RBUTTONUP:
    {
        if (DataValid)
        {
            HMENU hMenu, hSubMenu;
            POINT p;

            hMenu = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_CTX_MENU));
            hSubMenu = GetSubMenu(hMenu, 0);
            GetCursorPos(&p);
            EnableMenuItem(hSubMenu, CM_COPY, IsSelected() ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED));
            EnableMenuItem(hSubMenu, CM_SELECTALL, MF_BYCOMMAND | (ViewMode == fvmStandard ? MF_ENABLED : MF_GRAYED));
            HCURSOR oldCursor = SetCursor(HArrowCursor);
            TrackPopupMenuEx(hSubMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, p.x, p.y, HWindow, NULL);
            if (oldCursor)
                SetCursor(oldCursor);
            DestroyMenu(hMenu);
        }
        return 0;
    }

    case WM_TIMER:
    {
        if (wParam != IDT_SCROLLTEXT)
            break;
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(HWindow, &p);
        lParam = MAKELONG(p.x, p.y);
        // fall through...
    }

    case WM_MOUSEMOVE:
    {
        if (Tracking)
        {
            BOOL setTimer = FALSE;
            int x = (short)LOWORD(lParam);
            int y = (short)HIWORD(lParam);
            if (x < LineNumWidth)
            {
                SendMessage(HWindow, WM_HSCROLL, SB_LINELEFT, NULL);
                setTimer = TRUE;
            }
            if (x > Width)
            {
                SendMessage(HWindow, WM_HSCROLL, SB_LINERIGHT, NULL);
                setTimer = TRUE;
            }
            if (y < 0)
            {
                SendMessage(HWindow, WM_VSCROLL, SB_LINEUP, NULL);
                setTimer = TRUE;
            }
            if (y > (Height / FontHeight) * FontHeight)
            {
                SendMessage(HWindow, WM_VSCROLL, SB_LINEDOWN, NULL);
                setTimer = TRUE;
            }
            UpdateSelection(x, y);
            if (setTimer)
                SetTimer(HWindow, IDT_SCROLLTEXT, 20, NULL);
            else
                KillTimer(HWindow, IDT_SCROLLTEXT);
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CTextFileViewWindowBase
//

CTextFileViewWindowBase::CTextFileViewWindowBase(CFileViewID id, BOOL showWhiteSpace, CMainWindow* mainWindow)
    : CFileViewWindow(id, fvtText)
{
    CALL_STACK_MESSAGE1("CTextFileViewWindowBase::CTextFileViewWindowBase()");

    SelectDifferenceOnButtonUp = FALSE;

    FirstVisibleLine = 0;
    FirstVisibleChar = 0;

    DisplayCaret = FALSE;
    CaretXPos = 0;
    CaretYPos = 0;

    ShowWhiteSpace = showWhiteSpace;

    DetailedDifferences = FALSE;

    TabSize = Configuration.TabSize;

    MainWindow = mainWindow;

    // it contains just copies of pointers in Script[0]
    Script[1].release();
}

CTextFileViewWindowBase::~CTextFileViewWindowBase()
{
    CALL_STACK_MESSAGE1("CTextFileViewWindowBase::~CTextFileViewWindowBase()");
    DestroyData();
}

void CTextFileViewWindowBase::DestroyData()
{
    CALL_STACK_MESSAGE1("CTextFileViewWindowBase::DestroyData()");
    DataValid = FALSE;
    Script[0].clear();
    Script[1].clear();
}

BOOL CTextFileViewWindowBase::RebuildScript(
    const CIntIndexes& changesLengths,
    CIntIndexes* changesToLines,
    CFileViewMode viewMode,
    BOOL updateChangesToLines,
    int& maxWidth,
    BOOL& cancel)
{
    CALL_STACK_MESSAGE2("CTextFileViewWindowBase::RebuildScript(, %d, , , )", maxWidth);

    DataValid = FALSE;
    cancel = FALSE;
    ViewMode = viewMode;
    TabSize = Configuration.TabSize;

    GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help

    if (viewMode == fvmOnlyDifferences &&
        (Script[1].empty() || Context != Configuration.Context) && !changesToLines[0].empty())
    {
        // rebuild Script[1]
        Context = Configuration.Context;

        Script[1].clear();
        if (updateChangesToLines)
            changesToLines[1].clear();

        CIntIndexes::iterator change = changesToLines[0].begin();
        CIntIndexes::const_iterator length = changesLengths.begin();
        while (1)
        {
            // context before
            size_t line;
            if (Script[0][*change].IsBlank())
            { // find first real line preceeding change
                size_t si = *change;
                while (Script[0][si].IsBlank() && si > 0)
                    --si;
                line = Script[0][si].IsBlank() ? 0 : Script[0][si].GetLine() + 1;
            }
            else
                line = Script[0][*change].GetLine();

            size_t context = Context;
            if (line < size_t(Context)) // sometimes there is not enough lines
            {
                size_t sline;
                if (((CTextFileViewWindowBase*)Siblink)->Script[0][*change].IsBlank())
                { // find first real line preceeding change
                    size_t si = *change;
                    while (((CTextFileViewWindowBase*)Siblink)->Script[0][si].IsBlank() && si > 0)
                        --si;
                    sline = ((CTextFileViewWindowBase*)Siblink)->Script[0][si].IsBlank() ? 0 : ((CTextFileViewWindowBase*)Siblink)->Script[0][si].GetLine() + 1;
                }
                else
                    sline = ((CTextFileViewWindowBase*)Siblink)->Script[0][*change].GetLine();

                context = max(min(sline, size_t(Context)), line);
                if (line < context)
                {
                    fill_n(back_inserter(Script[1]),
                           context - line,
                           CLineSpec(CLineSpec::lcBlank));
                    context = line;
                }
            }
            size_t i;
            for (i = line - context; i < line; ++i)
                Script[1].push_back(CLineSpec(CLineSpec::lcCommon, int(i)));

            if (updateChangesToLines)
                changesToLines[1].push_back((int)Script[1].size());

            // change
            copy(
                Script[0].begin() + *change,
                Script[0].begin() + *change + *length,
                back_inserter(Script[1]));

            // context after
            size_t si = *change + *length;
            // Patera 2009.04.13: Added this if to avoid GPF when the only difference
            // is in EOL at EOF of one of the files
            if (si < Script[0].size())
            {
                while (Script[0][si].IsBlank() && si < Script[0].size() - 1)
                    ++si; // find first real line after change
                line = Script[0][si].IsBlank() ? GetLines() : Script[0][si].GetLine();
                context = Context;
                if (int(line + Context) > GetLines())
                {
                    si = *change + *length;
                    while (((CTextFileViewWindowBase*)Siblink)->Script[0][si].IsBlank() &&
                           si < ((CTextFileViewWindowBase*)Siblink)->Script[0].size() - 1)
                        ++si; // find first real line after change
                    size_t sline = ((CTextFileViewWindowBase*)Siblink)->Script[0][si].IsBlank() ? GetLines() : ((CTextFileViewWindowBase*)Siblink)->Script[0][si].GetLine();
                    context = max(
                        min(
                            ((CTextFileViewWindowBase*)Siblink)->GetLines() - sline,
                            size_t(Context)),
                        GetLines() - line);
                }
                for (i = line; i < line + context; ++i)
                {
                    if (int(i) < GetLines())
                        Script[1].push_back(CLineSpec(CLineSpec::lcCommon, int(i)));
                    else
                        Script[1].push_back(CLineSpec(CLineSpec::lcBlank));
                }
            }

            ++change, ++length;
            if (change >= changesToLines[0].end())
                break;

            // insert separator
            Script[1].push_back(CLineSpec(CLineSpec::lcSeparator));

            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == GetParent(HWindow))
            {
                MSG msg; // vyhodime nabufferovany ESC
                while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                    ;
                cancel = TRUE;
                //SG->SalMessageBox(GetParent(HWindow), LoadStr(IDS_CANCELED), LoadStr(IDS_DIFF), MB_ICONINFORMATION);
                return FALSE;
            }
        }
    }

    // urcime maxWidth
    int counter = 1000; // testujeme na cancel kazdy tisicy pruchod
    for (CLineScript::iterator i = Script[ViewMode].begin(); i < Script[ViewMode].end();
         ++i)
    {
        if (!i->IsBlank())
        {
            int len = MeasureLine(i->GetLine(), -1);
            maxWidth = max(len, maxWidth);
        }

        if (counter-- == 0)
        {
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == GetParent(HWindow))
            {
                MSG msg; // vyhodime nabufferovany ESC
                while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                    ;
                cancel = TRUE;
                //SG->SalMessageBox(GetParent(HWindow), LoadStr(IDS_CANCELED), LoadStr(IDS_DIFF), MB_ICONINFORMATION);
                return FALSE;
            }
            counter = 1000;
        }
    }

    // zjistime velikost mista pro cisla radku
    LineNumDigits = 1;
    int j = max(GetLines(), ((CTextFileViewWindowBase*)Siblink)->GetLines());
    while (j /= 10)
        LineNumDigits++;
    LineNumWidth = (LineNumDigits + 1) * FontWidth + BORDER_WIDTH;

    // nastavime scrollbaru
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_PAGE | SIF_RANGE;
    si.nMin = 0;
    si.nMax = int(Script[ViewMode].size()) > 0 ? int(Script[ViewMode].size()) - 1 : 0;
    si.nPage = Height / FontHeight;
    si.nPos = 0;
    SetScrollInfo(HWindow, SB_VERT, &si, TRUE);
    FirstVisibleLine = 0;

    SelectDiffBegin = -1;
    SelectDiffEnd = -1;

    RemoveSelection(FALSE);

    CaretXPos = 0;
    CaretYPos = 0;

    DataValid = TRUE;

    if (GetFocus() == HWindow && DisplayCaret)
    {
        DestroyCaret();
        ShowCaret();
    }

    return TRUE;
}

BOOL CTextFileViewWindowBase::SetMaxWidth(int maxWidth)
{
    CALL_STACK_MESSAGE2("CTextFileViewWindowBase::SetMaxWidth(%d)", maxWidth);
    FirstVisibleChar = 0;
    MaxWidth = maxWidth;

    if (!ReallocLineBuffer())
        return FALSE;

    // nastavime scrollbaru
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nMin = 0;
    si.nMax = maxWidth > 0 ? maxWidth - 1 : 0;
    LONG width = min(Width, SiblinkWidth);
    si.nPage = (width - LineNumWidth) / FontWidth;
    if (si.nPage < 0)
        si.nPage = 0;
    si.nPos = 0;
    SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);
    return TRUE;
}

void CTextFileViewWindowBase::SetSiblink(CFileViewWindow* wnd)
{
    CALL_STACK_MESSAGE1("CTextFileViewWindowBase::SetSiblink()");
    Siblink = wnd;
    RECT r;
    GetClientRect(Siblink->HWindow, &r);
    SiblinkWidth = r.right;

    LONG width = min(Width, SiblinkWidth);
    if (width < SiblinkWidth)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS;
        si.nPage = (width - LineNumWidth) / FontWidth;
        if (si.nPage < 0)
            si.nPage = 0;
        int maxScroolPos = MaxWidth - (int)si.nPage > 0 ? MaxWidth - si.nPage : 0;
        if (FirstVisibleChar > maxScroolPos)
            FirstVisibleChar = maxScroolPos;
        si.nPos = FirstVisibleChar;
        SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);
    }
}

void CTextFileViewWindowBase::ReloadConfiguration(DWORD flags, BOOL updateWindow)
{
    CALL_STACK_MESSAGE3("CTextFileViewWindowBase::ReloadConfiguration(0x%X, %d)",
                        flags, updateWindow);
    CFileViewWindow::ReloadConfiguration(flags, updateWindow);
    if (GetFocus() == HWindow && DisplayCaret)
    {
        DestroyCaret();
        ShowCaret();
    }
}

void CTextFileViewWindowBase::UpdateScrollBars(BOOL repaint)
{
    CALL_STACK_MESSAGE1("CTextFileViewWindowBase::UpdateScrollBars()");
    if (Siblink)
    {
        RECT r;
        GetClientRect(Siblink->HWindow, &r);
        SiblinkWidth = r.right;
    }

    LONG width = min(Width, SiblinkWidth);
    int vPos = FirstVisibleLine;
    int pageHeight = Height / FontHeight;
    int maxVPos = int(Script[ViewMode].size()) > pageHeight ? int(Script[ViewMode].size()) - pageHeight : 0;
    int hPos = GetScrollPos(HWindow, SB_HORZ);
    ;
    int pageWidth = (width - LineNumWidth) / FontWidth;
    if (pageWidth < 0)
    {
        SiblinkWidth = Width = max(Width, SiblinkWidth);
        pageWidth = (Width - LineNumWidth) / FontWidth;
        if (pageWidth < 0)
            pageWidth = 0;
    }
    int maxHPos = MaxWidth - pageWidth > 0 ? MaxWidth - pageWidth : 0;

    if (vPos > maxVPos)
        vPos = maxVPos;
    if (hPos > maxHPos)
        hPos = maxHPos;

    int dx = (FirstVisibleChar - hPos) * FontWidth;
    int dy = (FirstVisibleLine - vPos) * FontHeight;

    if (dy || dx)
    {
        if (repaint)
        {
            if (!(abs(FirstVisibleLine - maxVPos) < pageHeight - 1 &&
                  abs(FirstVisibleChar - maxHPos) < pageWidth - 1))
            {
                InvalidateRect(HWindow, NULL, FALSE);
            }
            else
            {
                //InvalidateRect(HWindow, NULL, FALSE);
                if (!dy) // && dx
                {
                    RECT r;
                    r.left = LineNumWidth;
                    r.right = Width;
                    r.top = 0;
                    r.bottom = Height;
                    ScrollWindowEx(HWindow, dx, 0, &r, &r, NULL, NULL, SW_INVALIDATE);
                }
                else
                {
                    if (dx)
                    {
                        RECT r;
                        r.left = 0;
                        r.right = LineNumWidth;
                        r.top = 0;
                        r.bottom = Height;
                        ScrollWindowEx(HWindow, 0, dy, &r, NULL, NULL, NULL, SW_INVALIDATE);
                        r.left = r.right;
                        r.right = Width;
                        ScrollWindowEx(HWindow, dx, dy, &r, &r, NULL, NULL, SW_INVALIDATE);
                        InvalidateRect(HWindow, NULL, FALSE);
                    }
                    else
                    {
                        ScrollWindowEx(HWindow, 0, dy, NULL, NULL, NULL, NULL, SW_INVALIDATE);
                    }
                }
            }
        }

        FirstVisibleLine = vPos;
        FirstVisibleChar = hPos;

        if (repaint)
            UpdateWindow(HWindow);
    }

    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS;
    si.nPage = pageHeight;
    si.nPos = FirstVisibleLine;
    SetScrollInfo(HWindow, SB_VERT, &si, TRUE);
    si.nPage = pageWidth;
    si.nPos = hPos;
    SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);
}

void CTextFileViewWindowBase::SelectDifference(int line, int count, BOOL center)
{
    CALL_STACK_MESSAGE3("CTextFileViewWindowBase::SelectDifference(%d, %d)", line, count);
    int page = Height / FontHeight;

    if (center &&
        (count >= page && (line + count - 1 < FirstVisibleLine || line >= FirstVisibleLine + page) ||
         count < page && (line < FirstVisibleLine || line + count - 1 >= FirstVisibleLine + page)))
    {
        // difference neni viditelna na obrazovce
        int pos = FirstVisibleLine;
        if (count > page)
            pos = line; // zobrazime differenci u hornoho okraje okna
        else
        {
            pos = line - (page - count) / 2; // vycentrujeme differenci k oknu
            if (pos < 0)
                pos = 0;
        }

        int maxPos = int(Script[ViewMode].size()) > page ? int(Script[ViewMode].size()) - page : 0;
        if (pos > maxPos)
            pos = maxPos;

        FirstVisibleLine = pos;
        SetScrollPos(HWindow, SB_VERT, pos, TRUE);
        InvalidateRect(HWindow, NULL, FALSE);
    }
    else
    {
        RECT r;
        r.left = 0;
        r.right = Width;
        if (SelectDiffBegin != line)
        {
            if (SelectDiffEnd >= FirstVisibleLine && SelectDiffBegin <= FirstVisibleLine + page)
            {
                // zajistime prekresleni drive vybrane difference
                r.top = (SelectDiffBegin - FirstVisibleLine) * FontHeight;
                if (r.top < 0)
                    r.top = 0;
                r.bottom = (SelectDiffEnd - FirstVisibleLine + 1) * FontHeight + 1;
                if (r.bottom > Height)
                    r.bottom = Height;
                InvalidateRect(HWindow, &r, FALSE);
            }
            // zajistime vykresleni nyni vybrane difference
            r.top = (line - FirstVisibleLine) * FontHeight;
            if (r.top < 0)
                r.top = 0;
            r.bottom = (line + count - FirstVisibleLine) * FontHeight + 1;
            if (r.bottom > Height)
                r.bottom = Height;
            InvalidateRect(HWindow, &r, FALSE);
        }
    }
    SelectDiffBegin = line;
    SelectDiffEnd = line + count - 1;
    UpdateWindow(HWindow);
    UpdateSelection();
    UpdateCaretPos(TRUE);
}

void CTextFileViewWindowBase::UpdateSelection()
{
    SLOW_CALL_STACK_MESSAGE1("CTextFileViewWindowBase::UpdateSelection()");
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(HWindow, &pt);
    UpdateSelection(pt.x, pt.y);
}

void CTextFileViewWindowBase::UpdateSelection(int x, int y)
{
    CALL_STACK_MESSAGE3("CTextFileViewWindowBase::UpdateSelection(%d, %d)", x, y);
    if (!DataValid || !Tracking)
        return; // jen tak pro jistotu
    if (x < LineNumWidth)
        x = LineNumWidth;
    if (y < 0)
        y = 0;
    if (x >= Width)
        x = Width - 1;
    if (y >= Height)
        y = Height - 1;

    //x += FontWidth/2;

    int line = FirstVisibleLine + y / FontHeight;
    if (line >= int(Script[ViewMode].size()))
        line = int(Script[ViewMode].size() - 1);

    int l = Script[ViewMode][line].GetLine();
    int len = LineLength(l);
    int end;

    if (Script[ViewMode][line].IsBlank())
        end = 0;
    else
    {
        //end = FirstVisibleChar + (x - LineNumWidth)/FontWidth;
        //if (len < end) end = len;
        end = TransformXOrg(x, line);
    }

    if (TrackingLineBegin != line || TrackingCharBegin != end)
    {
        // aby se nam na WM_LBUTTONUP nevyfokusovala difference
        SelectDifferenceOnButtonUp = FALSE;
    }

    int oldLineBegin = SelectionLineBegin;
    int oldLineEnd = SelectionLineEnd;
    int oldCharBegin = SelectionCharBegin;
    int oldCharEnd = SelectionCharEnd;

    // nastavime aktualni vyber
    BOOL reverse;
    if (TrackingLineBegin < line ||
        TrackingLineBegin == line && TrackingCharBegin < end)
    {
        SelectionLineBegin = TrackingLineBegin;
        SelectionCharBegin = TrackingCharBegin;
        SelectionLineEnd = line;
        SelectionCharEnd = end;
        reverse = FALSE;
    }
    else
    {
        SelectionLineBegin = line;
        SelectionCharBegin = end;
        SelectionLineEnd = TrackingLineBegin;
        SelectionCharEnd = TrackingCharBegin;
        reverse = TRUE;
    }

    if ((SelectionLineBegin != SelectionLineEnd || SelectionCharBegin != SelectionCharEnd) &&
        (GetKeyState(VK_CONTROL) & 0x8000) != 0) //ctrlPressed
    {
        // vybirame cele radky
        SelectionCharBegin = 0;
        if (ViewMode != fvmOnlyDifferences || Script[ViewMode][SelectionLineEnd].GetClass() != CLineSpec::lcSeparator)
        {
            if (SelectionLineEnd + 1 < int(Script[ViewMode].size()))
            {
                SelectionLineEnd++;
                SelectionCharEnd = 0;
            }
            else
                SelectionCharEnd = len;
        }
    }

    if (ViewMode == fvmOnlyDifferences)
    {
        // ohlidame aby slo vybirat jen v ramci jedne difference
        if (TrackingLineBegin < line)
        {
            if (Script[ViewMode][SelectionLineBegin].GetClass() == CLineSpec::lcSeparator)
            {
                SelectionLineBegin++;
                SelectionCharBegin = 0;
            }
            int i;
            for (i = SelectionLineBegin; i <= SelectionLineEnd; i++)
            {
                if (Script[ViewMode][i].GetClass() == CLineSpec::lcSeparator)
                {
                    SelectionLineEnd = i;
                    SelectionCharEnd = 0;
                    break;
                }
            }
        }
        else
        {
            int i;
            for (i = SelectionLineEnd - 1; i >= SelectionLineBegin; i--)
            {
                if (Script[ViewMode][i].GetClass() == CLineSpec::lcSeparator)
                {
                    SelectionLineBegin = i + 1;
                    SelectionCharBegin = 0;
                    break;
                }
            }
        }
    }

    // posuneme kurzor na konec vyberu
    if (reverse)
        SetCaretPos(SelectionCharBegin, SelectionLineBegin);
    else
        SetCaretPos(SelectionCharEnd, SelectionLineEnd);

    // zajistime prekresleni postizenych casti
    RepaintSelection(oldLineBegin, oldLineEnd, oldCharBegin, oldCharEnd);

    // enablujem/disablujem tlacitko pro COPY v toolbare
    //  CMainWindow * parent = (CMainWindow *) WindowsManager.GetWindowPtr(GetParent(HWindow));
    if (MainWindow /*parent*/)
        MainWindow /*parent*/->UpdateToolbarButtons(UTB_TEXTSELECTION);
}

void CTextFileViewWindowBase::RepaintSelection(int oldLineBegin, int oldLineEnd,
                                               int oldCharBegin, int oldCharEnd)
{
    CALL_STACK_MESSAGE5("CTextFileViewWindowBase::RepaintSelection(%d, %d, %d, %d)",
                        oldLineBegin, oldLineEnd, oldCharBegin, oldCharEnd);
    // zajistime prekresleni jen postizenych casti
    RECT r;
    r.left = LineNumWidth;
    r.right = Width;
    if (SelectionLineBegin != oldLineBegin)
    {
        r.top = (__min(SelectionLineBegin, oldLineBegin) - FirstVisibleLine) * FontHeight;
        r.bottom = (max(SelectionLineBegin, oldLineBegin) - FirstVisibleLine + 1) * FontHeight;
        if (r.top < Height && r.bottom > 0)
        {
            if (r.top < 0)
                r.top = 0;
            if (r.bottom > Height)
                r.bottom = Height;

            InvalidateRect(HWindow, &r, TRUE);
        }
    }

    if (SelectionLineEnd != oldLineEnd)
    {
        r.top = (__min(SelectionLineEnd, oldLineEnd) - FirstVisibleLine) * FontHeight;
        r.bottom = (max(SelectionLineEnd, oldLineEnd) - FirstVisibleLine + 1) * FontHeight;
        if (r.top < Height && r.bottom > 0)
        {
            if (r.top < 0)
                r.top = 0;
            if (r.bottom > Height)
                r.bottom = Height;

            InvalidateRect(HWindow, &r, TRUE);
        }
    }

    if (SelectionLineBegin == oldLineBegin && SelectionCharBegin != oldCharBegin)
    {
        r.top = (SelectionLineBegin - FirstVisibleLine) * FontHeight;
        r.bottom = (SelectionLineBegin - FirstVisibleLine + 1) * FontHeight;
        /*int l = Script[SelectionLineBegin] & MAX_LINES;
    int len1 = MeasureLine(Linbuf[l], Linbuf[l] + SelectionCharBegin);
    int len2 = MeasureLine(Linbuf[l], Linbuf[l] + oldCharBegin);
    r.left = (min(len1, len2) - FirstVisibleChar) * FontWidth + LineNumWidth;
    r.right = (max(len1, len2) - FirstVisibleChar + 1) * FontWidth + LineNumWidth;
    */
        //r.left = (min(SelectionCharBegin, oldCharBegin) - FirstVisibleChar) * FontWidth + LineNumWidth;
        //r.right = (max(SelectionCharBegin, oldCharBegin) - FirstVisibleChar + 1) * FontWidth + LineNumWidth;
        int l = Script[ViewMode][SelectionLineBegin].GetLine();
        r.left = (MeasureLine(l, __min(SelectionCharBegin, oldCharBegin)) - FirstVisibleChar) * FontWidth + LineNumWidth;
        r.right = (MeasureLine(l, max(SelectionCharBegin, oldCharBegin)) - FirstVisibleChar + 1) * FontWidth + LineNumWidth;
        if (r.top < Height && r.bottom > 0 &&
            r.left < Width && r.right > LineNumWidth)
        {
            if (r.top < 0)
                r.top = 0;
            if (r.bottom > Height)
                r.bottom = Height;
            if (r.left < LineNumWidth)
                r.left = LineNumWidth;
            if (r.right > Width)
                r.right = Width;

            InvalidateRect(HWindow, &r, TRUE);
        }
    }

    if (SelectionLineEnd == oldLineEnd && SelectionCharEnd != oldCharEnd)
    {
        r.top = (SelectionLineEnd - FirstVisibleLine) * FontHeight;
        r.bottom = (SelectionLineEnd - FirstVisibleLine + 1) * FontHeight;
        int l = Script[ViewMode][SelectionLineEnd].GetLine();
        r.left = (MeasureLine(l, __min(SelectionCharEnd, oldCharEnd)) - FirstVisibleChar) * FontWidth + LineNumWidth;
        r.right = (MeasureLine(l, max(SelectionCharEnd, oldCharEnd)) - FirstVisibleChar + 1) * FontWidth + LineNumWidth;
        if (r.top < Height && r.bottom > 0 &&
            r.left < Width && r.right > LineNumWidth)
        {
            if (r.top < 0)
                r.top = 0;
            if (r.bottom > Height)
                r.bottom = Height;
            if (r.left < LineNumWidth)
                r.left = LineNumWidth;
            if (r.right > Width)
                r.right = Width;

            InvalidateRect(HWindow, &r, TRUE);
        }
    }

    UpdateWindow(HWindow);
}

void CTextFileViewWindowBase::RemoveSelection(BOOL repaint)
{
    CALL_STACK_MESSAGE2("CTextFileViewWindowBase::RemoveSelection(%d)", repaint);
    int oldLineBegin = SelectionLineBegin;
    int oldLineEnd = SelectionLineEnd;
    int oldCharBegin = SelectionCharBegin;
    int oldCharEnd = SelectionCharEnd;
    SelectionLineBegin = -1;
    SelectionCharBegin = -1;
    SelectionLineEnd = -1;
    SelectionCharEnd = -1;
    if (repaint)
        RepaintSelection(oldLineBegin, oldLineEnd, oldCharBegin, oldCharEnd);

    //  CMainWindow * parent = (CMainWindow *) WindowsManager.GetWindowPtr(GetParent(HWindow));
    if (MainWindow /*parent*/)
        MainWindow /*parent*/->UpdateToolbarButtons(UTB_TEXTSELECTION);
}

void CTextFileViewWindowBase::SetCaretPos(int xPos, int yPos)
{
    CALL_STACK_MESSAGE3("CTextFileViewWindowBase::SetCaretPos(%d, %d)", xPos, yPos);
    if (!DataValid)
        return;

    if (yPos < 0)
        yPos = 0;
    if (yPos >= int(Script[ViewMode].size()))
    {
        yPos = int(Script[ViewMode].size()) - 1;
        if (yPos < 0)
        {
            // Showing 2 identical files in "Show Only Differences" mode
            ::SetCaretPos(0, 0);
            return;
        }
    }

    if (xPos < 0 || Script[ViewMode][yPos].IsBlank())
        xPos = 0;
    else
    {
        int l = Script[ViewMode][yPos].GetLine();
        int len = LineLength(l);
        if (xPos > len)
            xPos = len;
    }

    CaretXPos = xPos;
    CaretYPos = yPos;

    if (GetFocus() == HWindow && DisplayCaret)
    {
        int x, y = (CaretYPos - FirstVisibleLine) * FontHeight;
        if (Script[ViewMode][CaretYPos].IsBlank())
            x = LineNumWidth - FirstVisibleChar * FontWidth;
        else
        {
            int l = Script[ViewMode][CaretYPos].GetLine();
            x = LineNumWidth + (MeasureLine(l, CaretXPos) - FirstVisibleChar) * FontWidth;
        }
        if (x < LineNumWidth)
            x = -1; // mensi finta, aby caret zmizel za roh
        ::SetCaretPos(x, y);
    }
}

/*
int 
CTextFileViewWindowBase::TransformTabs(int oldx, int oldline, int line)
{
  if (!DataValid) return 0;
  
  if (line < 0) line = 0;
  if (line >= Script.Count) line = Script.Count - 1;

  if (IsBlank(Script[line])) return 0;

  int x = 0;
  if (!IsBlank(Script[oldline]))
  {
    int l = Script[oldline] & MAX_LINES;
    x = MeasureLine(Linbuf[l], Linbuf[l] + oldx);
  }

  int l = Script[line] & MAX_LINES;
  const char * start = Linbuf[l];
  const char * iterator = start;
  const char * end = Linbuf[l+1] - 1;
  int tabChars = 0;
  
  while (iterator < end)
  {
    if (*iterator == '\t')
    {
      int i = TabSize - ((iterator - start) + tabChars) % TabSize;
      tabChars += i - 1;
    }
    if (iterator - start + tabChars >= x) break;
    iterator++;
  }
  return iterator - start;
}
*/

void CTextFileViewWindowBase::UpdateCaretPos(BOOL enablePosChange)
{
    CALL_STACK_MESSAGE2("CTextFileViewWindowBase::UpdateCaretPos(%d)", enablePosChange);
    if (GetFocus() != HWindow || !DisplayCaret || !DataValid)
        return;

    int yPos = CaretYPos;
    if (enablePosChange)
    {
        if (CaretYPos < FirstVisibleLine)
            yPos = FirstVisibleLine;
        if (CaretYPos > FirstVisibleLine + Height / FontHeight - 1)
            yPos = __max(FirstVisibleLine + Height / FontHeight - 1, 0);
    }
    MoveCaret(yPos);
}

void CTextFileViewWindowBase::SetCaret(BOOL displayCaret)
{
    CALL_STACK_MESSAGE2("CTextFileViewWindowBase::SetCaret(%d)", displayCaret);
    if (DisplayCaret == displayCaret || !DataValid)
        return;

    DisplayCaret = displayCaret;

    if (GetFocus() == HWindow)
    {
        if (DisplayCaret)
            ShowCaret();
        else
            DestroyCaret();
    }
}

void CTextFileViewWindowBase::ShowCaret()
{
    CALL_STACK_MESSAGE1("CTextFileViewWindowBase::ShowCaret()");
    if (!DisplayCaret || !DataValid)
        return;

    CreateCaret(HWindow, NULL, CaretWidth, FontHeight);

    int yPos = CaretYPos;
    if (CaretYPos < FirstVisibleLine)
        yPos = FirstVisibleLine;
    if (CaretYPos > FirstVisibleLine + Height / FontHeight - 1)
        yPos = __max(FirstVisibleLine + Height / FontHeight - 1, 0);

    int xPos = CaretXPos;

    SetCaretPos(CaretXPos, yPos);

    int x = 0;
    // NOTE: Script[fvmOnlyDifferences] is empty when showing identical files
    if (!Script[ViewMode].empty() && !Script[ViewMode][CaretYPos].IsBlank())
    {
        int l = Script[ViewMode][CaretYPos].GetLine();
        x = MeasureLine(l, xPos);
    }
    if (x < FirstVisibleChar)
        xPos = GetLeftmostVisibleChar(CaretYPos);
    if (x > FirstVisibleChar + (Width - LineNumWidth) / FontWidth - 1)
        xPos = GetRightmostVisibleChar(CaretYPos);

    SetCaretPos(xPos, CaretYPos);

    ::ShowCaret(HWindow);
}
