// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "viewer.h"
#include "dialogs.h"
#include "shellib.h"
#include "mainwnd.h"
#include "codetbl.h"

BOOL ViewerActive(HWND hwnd)
{
    return GetForegroundWindow() == hwnd;
}

// proportional to the width of the window, it's an "estimate"
#define FAST_LEFTRIGHT max(1, (Width - BORDER_WIDTH) / CharWidth / 6)
#define MAKEVIS_LEFTRIGHT max(0, (Width - BORDER_WIDTH) / CharWidth / 6)

void CViewerWindow::SetViewerCaption()
{
    char caption[MAX_PATH + 300];
    if (Caption == NULL)
    {
        if (FileName != NULL)
            lstrcpyn(caption, FileName, MAX_PATH); // caption according to file
        else
            caption[0] = 0;
    }
    else
        lstrcpyn(caption, Caption, MAX_PATH); // caption according to the plugin's request
    if (Caption == NULL || !WholeCaption)
    {
        if (caption[0] != 0)
            strcat(caption, " - ");
        strcat(caption, LoadStr(IDS_VIEWERTITLE));
        if (CodeType > 0)
        {
            char codeName[200];
            CodeTables.GetCodeName(CodeType, codeName, 200);
            RemoveAmpersands(codeName);
            char* s = codeName + strlen(codeName);
            while (s > codeName && *(s - 1) == ' ')
                s--;
            *s = 0; // trimming excess spaces
            sprintf(caption + strlen(caption), " - [%s]", codeName);
        }
    }
    SetWindowText(HWindow, caption);
}

//
//*****************************************************************************
// CViewerWindow
//

void CViewerWindow::SetCodeType(int c)
{
    CodeType = c;
    UseCodeTable = CodeTables.GetCode(CodeTable, CodeType);

    // invalidate buffer
    Seek = 0;
    Loaded = 0;

    SetViewerCaption();
}

void CViewerWindow::OnVScroll()
{
    if (VScrollWParam != -1 && VScrollWParam != VScrollWParamOld)
    {
        VScrollWParamOld = VScrollWParam;
        __int64 oldSeekY = SeekY;
        EndSelectionRow = -1; // disable optimization
        EnableSetScroll = ((int)LOWORD(VScrollWParam) == SB_THUMBPOSITION);
        SeekY = (__int64)(ScrollScaleY * ((short)HIWORD(VScrollWParam)) + 0.5);
        SeekY = min(SeekY, MaxSeekY);
        BOOL fatalErr = FALSE;

        // Smarter buffer loading when seeking randomly - new read:
        // 1/6 before, 2/6 after SeekY (Prepare reads only 1/2 of the buffer)
        __int64 readFrom;
        if (SeekY > VIEW_BUFFER_SIZE / 6)
            readFrom = SeekY - VIEW_BUFFER_SIZE / 6;
        else
            readFrom = 0;
        Prepare(NULL, readFrom, VIEW_BUFFER_SIZE / 2, fatalErr);
        BOOL oldForceTextMode = ForceTextMode;
        __int64 newSeekY = 0;
        if (!fatalErr)
            newSeekY = FindBegin(SeekY, fatalErr);
        if (fatalErr)
            FatalFileErrorOccured();
        if (fatalErr || ExitTextMode)
        {
            EnableSetScroll = TRUE;
            return;
        }
        SeekY = newSeekY;

        if (EnableSetScroll || SeekY != oldSeekY)
        {
            ResetFindOffsetOnNextPaint = TRUE;
            InvalidateRect(HWindow, NULL, FALSE);
            UpdateWindow(HWindow); // to calculate the ViewSize for the next PageDown

            // Alignment to a long line, msgbox for switching to HEX (in FindBegin above or in Paint),
            // Answer No - ends with an unset scrollbar, this hack solves it (probably just this would be enough
            // invalidate scrollbar
            if (ForceTextMode != oldForceTextMode)
                InvalidateRect(HWindow, NULL, FALSE);
        }
        else
        {
            FindOffset = SeekY;
            if (!FindDialog.Forward)
                FindOffset += ViewSize;
        }
    }
    // It is common to shoot at SB_THUMBPOSITION, but for example when unpacking a msgbox during
    // Dragging (too long text line, switch to HEX) still works even with an open msgbox
    if (GetCapture() == NULL)
    {
        VScrollWParam = -1;
        EnableSetScroll = TRUE;
    }
    if (VScrollWParam == -1)
        KillTimer(HWindow, IDT_THUMBSCROLL);
}

void CViewerWindow::PostMouseMove()
{
    // we will ensure the block is redrawn
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    ScreenToClient(HWindow, &cursorPos);
    PostMessage(HWindow, WM_MOUSEMOVE, NULL, MAKELPARAM(cursorPos.x, cursorPos.y));
}

BOOL CViewerWindow::GetXFromOffsetInText(__int64* x, __int64 offset, int lineInView, __int64 lineBegOff,
                                         __int64 lineCharLen, __int64 lineEndOff)
{
    BOOL fatalErr = FALSE;
    if (lineInView != -1)
    {
        if (lineInView < 0 || lineInView >= LineOffset.Count / 3)
            TRACE_C("Unexpected in CViewerWindow::GetXFromOffsetInText().");
        lineBegOff = LineOffset[3 * lineInView];
        lineEndOff = LineOffset[3 * lineInView + 1];
        lineCharLen = LineOffset[3 * lineInView + 2];
    }
    GetOffsetOrXAbs(0, NULL, NULL, lineBegOff, lineCharLen, lineEndOff, fatalErr, NULL, TRUE, offset, x);
    if (fatalErr)
        FatalFileErrorOccured();
    return !fatalErr && !ExitTextMode;
}

BOOL CViewerWindow::GetOffsetFromXInText(__int64* x, __int64* offset, __int64 suggestedX, int lineInView,
                                         __int64 lineBegOff, __int64 lineCharLen, __int64 lineEndOff)
{
    BOOL fatalErr = FALSE;
    if (lineInView != -1)
    {
        if (lineInView < 0 || lineInView >= LineOffset.Count / 3)
            TRACE_C("Unexpected in CViewerWindow::GetOffsetFromXInText().");
        lineBegOff = LineOffset[3 * lineInView];
        lineEndOff = LineOffset[3 * lineInView + 1];
        lineCharLen = LineOffset[3 * lineInView + 2];
    }
    GetOffsetOrXAbs(suggestedX, offset, x, lineBegOff, lineCharLen, lineEndOff, fatalErr, NULL);
    if (fatalErr)
        FatalFileErrorOccured();
    return !fatalErr && !ExitTextMode;
}

BOOL CViewerWindow::ScrollViewLineUp(DWORD repeatCmd, BOOL* scrolled, BOOL repaint, __int64* firstLineEndOff,
                                     __int64* firstLineCharLen)
{
    if (scrolled != NULL)
        *scrolled = FALSE;
    if (firstLineEndOff != NULL)
        *firstLineEndOff = -1;
    if (firstLineCharLen != NULL)
        *firstLineCharLen = -1;
    if (SeekY > 0)
    {
        __int64 oldSeekY = SeekY;
        BOOL fatalErr = FALSE;
        SeekY -= ZeroLineSize(fatalErr, firstLineEndOff, firstLineCharLen);
        if (SeekY < 0)
            SeekY = 0;
        if (fatalErr)
            FatalFileErrorOccured(repeatCmd);
        if (fatalErr || ExitTextMode)
            return FALSE;
        if (oldSeekY != SeekY)
        {
            if (scrolled != NULL)
                *scrolled = TRUE;
            if (repaint)
            {
                ::ScrollWindow(HWindow, 0, CharHeight, NULL, NULL); //scroll the window
                UpdateWindow(HWindow);
                if (EndSelectionRow != -1)
                    EndSelectionRow++;
            }
        }
        else
            TRACE_E("Unexpected situation when scrolling view up.");
    }
    return TRUE;
}

BOOL CViewerWindow::ScrollViewLineDown(BOOL fullRedraw)
{
    if (SeekY < MaxSeekY)
    {
        __int64 oldSeekY = SeekY;
        SeekY = min(MaxSeekY, SeekY + FirstLineSize);
        if (oldSeekY != SeekY)
        {
            if (!fullRedraw)
                ::ScrollWindow(HWindow, 0, -CharHeight, NULL, NULL); //scroll the window
            UpdateWindow(HWindow);
            if (EndSelectionRow != -1)
                EndSelectionRow--;
            return TRUE;
        }
    }
    return FALSE;
}

__int64
CViewerWindow::GetMaxVisibleLineLen(__int64 newFirstLineLen, BOOL ignoreFirstLine)
{
    __int64 max = 0;
    switch (Type)
    {
    case vtText:
    {
        int lineOffsetCount = LineOffset.Count;
        if (newFirstLineLen != -1) // scenario: should be scrolled down by one line (will be the new first line)
        {
            max = newFirstLineLen;
            if (lineOffsetCount >= 3)
                lineOffsetCount -= 3; // we will skip the last line (instead of it, there is a new first line)
        }
        // 'ignoreFirstLine' is TRUE: it should scroll up by one line (the new last line will be), so
        // We will skip the first line (we don't have a new last line, we'll ignore it, it will only be partially visible, enough in Paint())
        for (int i = ignoreFirstLine ? 3 + 2 : 2; i < lineOffsetCount; i += 3)
            if (max < LineOffset[i])
                max = LineOffset[i];
        break;
    }

    case vtHex:
        max = 62 + 16 - 8 + HexOffsetLength;
        break;
    }
    return max;
}

__int64
CViewerWindow::GetMaxOriginX(__int64 newFirstLineLen, BOOL ignoreFirstLine, __int64 maxLineLen)
{
    __int64 maxLL = maxLineLen != -1 ? maxLineLen : GetMaxVisibleLineLen(newFirstLineLen, ignoreFirstLine);
    int columns = (Width - BORDER_WIDTH) / CharWidth;
    return maxLL > columns ? maxLL - columns : 0;
}

void CViewerWindow::InvalidateRows(int minRow, int maxRow, BOOL update)
{
    RECT r;
    r.left = 0;
    r.top = minRow * CharHeight;
    r.right = Width;
    r.bottom = maxRow * CharHeight + CharHeight;
    InvalidateRect(HWindow, &r, FALSE);
    if (update)
        UpdateWindow(HWindow);
}

void CViewerWindow::EnsureXVisibleInView(__int64 x, BOOL showPrevChar, BOOL& fullRedraw,
                                         __int64 newFirstLineLen, BOOL ignoreFirstLine, __int64 maxLineLen)
{
    fullRedraw = FALSE;
    int columns = (Width - BORDER_WIDTH) / CharWidth;
    if (x > 0 && showPrevChar)
        x--;
    if (x >= OriginX + columns)
    {
        __int64 maxOX = GetMaxOriginX(newFirstLineLen, ignoreFirstLine, maxLineLen);
        // Block pulled from back to front with the end at the end of the longest line (wider than the view)
        // the EOL-terminated (not wrapped) 'x' position is beyond the end of the view (the above condition is met)
        // ale 'OriginX' uz nelze zvetsit (je uz 'maxOX'), zabranime zbytecnemu prekresleni
        // whole view
        if (maxOX > OriginX) // only if it is possible to move the view to the right
        {
            OriginX = x - columns + MAKEVIS_LEFTRIGHT;
            if (OriginX > maxOX)
                OriginX = maxOX;
            fullRedraw = TRUE;
        }
    }
    if (x >= 0 && x < OriginX)
    {
        if (x > MAKEVIS_LEFTRIGHT)
            OriginX = x - MAKEVIS_LEFTRIGHT;
        else
            OriginX = 0;
        fullRedraw = TRUE;
    }
}

#define WM_MOUSEHWHEEL 0x020E

LRESULT
CViewerWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CViewerWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    if (WaitForViewerRefresh && // state "fatal error", waiting for repair using WM_USER_VIEWERREFRESH
        uMsg != WM_SETCURSOR && // these messages are handled in both states (ok and fatal) identically
        uMsg != WM_DESTROY)
    {
        switch (uMsg)
        {
        case WM_ACTIVATE:
        {
            // Ensure hiding/displaying the Wait window (if it exists)
            ShowSafeWaitWindow(LOWORD(wParam) != WA_INACTIVE);
            break;
        }

        case WM_USER_VIEWERREFRESH:
        {
            WaitForViewerRefresh = FALSE;
            ExitTextMode = FALSE;
            ForceTextMode = FALSE;
            if (FileName != NULL)
            {
                BOOL fatalErr = FALSE;
                FileChanged(NULL, FALSE, fatalErr, FALSE);
                if (!fatalErr && ExitTextMode)
                    SeekY = LastSeekY; // we want to be at approximately the same position in HEX as before in Text mode
                if (!fatalErr && !ExitTextMode)
                {
                    SeekY = min(MaxSeekY, LastSeekY); // Restore LastSeekY in the new version of the file
                    __int64 newSeekY = FindBegin(SeekY, fatalErr);
                    if (!fatalErr && !ExitTextMode)
                        SeekY = newSeekY;
                }
                if (fatalErr)
                {
                    FatalFileErrorOccured();
                    //    // I commented out this block because otherwise the "Retry" button in the error messagebox opened from LoadBehind() and LoadBefore() wouldn't work
                    //            if (Caption != NULL)
                    //            {
                    //              free(Caption);
                    //              Caption = NULL;
                    //            }
                    //            if (FileName != NULL) free(FileName);
                    //            if (Lock != NULL)
                    //            {
                    //              SetEvent(Lock);
                    //              Lock = NULL; // now it's just on the disk-cache
                    //            }
                    //            FileName = NULL;
                    //            Seek = 0;
                    //            Loaded = 0;
                    //            OriginX = 0;
                    //            SeekY = 0;
                    //            MaxSeekY = 0;
                    //            FileSize = 0;
                    //            ViewSize = 0;
                    //            FirstLineSize = 0;
                    //            LastLineSize = 0;
                    //            StartSelection = -1;
                    //            EndSelection = -1;
                    //            ReleaseMouseDrag();
                    //            FindOffset = 0;
                    //            LastFindSeekY = 0;
                    //            LastFindOffset = 0;
                    //            ScrollToSelection = FALSE;
                    //            LineOffset.DestroyMembers();
                    //            EnableSetScroll = TRUE;
                    //            SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE));
                    //            InvalidateRect(HWindow, NULL, FALSE);
                }
                else
                {
                    if (!ExitTextMode)
                    {
                        OriginX = LastOriginX;
                        InvalidateRect(HWindow, NULL, FALSE);
                        UpdateWindow(HWindow); // to calculate the ViewSize for the next PageDown
                        if (RepeatCmdAfterRefresh != -1)
                            PostMessage(HWindow, WM_COMMAND, RepeatCmdAfterRefresh, 0);
                    }
                }
            }
            else
            {
                EnableSetScroll = TRUE;
                InvalidateRect(HWindow, NULL, FALSE);
                UpdateWindow(HWindow);
            }
            RepeatCmdAfterRefresh = -1;
            return 0;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case CM_EXIT:
                DestroyWindow(HWindow);
                return 0;
            }
            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HANDLES(BeginPaint(HWindow, &ps));
            HANDLES(EndPaint(HWindow, &ps));
            return 0;
        }

        case WM_ERASEBKGND:
            return TRUE; // do not erase the background

        case WM_SIZE:
        {
            Width = LOWORD(lParam);
            if (Width < 0)
                Width = 0;
            Height = HIWORD(lParam);
            if (Height < 0)
                Height = 0;
            Bitmap.Enlarge(Width, CharHeight);
            if (HToolTip != NULL)
            {
                TOOLINFO ti;
                ti.cbSize = sizeof(ti);
                ti.hwnd = HWindow;
                ti.uId = 1;
                GetClientRect(HWindow, &ti.rect);
                SendMessage(HToolTip, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);
            }
            break;
        }
        }
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }

    if (uMsg == WM_MOUSEWHEEL)
    {
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        short zDelta = (short)HIWORD(wParam);
        if ((zDelta < 0 && MouseWheelAccumulator > 0) || (zDelta > 0 && MouseWheelAccumulator < 0))
            ResetMouseWheelAccumulator(); // It is necessary to reset the accumulator when changing the direction of rotation of the wheel

        DWORD delta = GetMouseWheelScrollLines(); // 'delta' can be up to WHEEL_PAGESCROLL(0xffffffff)

        // standard scrolling without modifier keys
        if (!controlPressed && !altPressed && !shiftPressed)
        {
            DWORD wheelScroll = GetMouseWheelScrollLines(); // can be up to WHEEL_PAGESCROLL(0xffffffff)
            DWORD pageHeight = max(1, (DWORD)Height / CharHeight);
            wheelScroll = max(1, min(wheelScroll, pageHeight - 1)); // limit the maximum page length

            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int linesToScroll = MouseWheelAccumulator / stepsPerLine;
            if (linesToScroll != 0)
            {
                MouseWheelAccumulator -= linesToScroll * stepsPerLine;
                if ((DWORD)abs(linesToScroll) > pageHeight - 1)
                {
                    SendMessage(HWindow, WM_COMMAND, zDelta > 0 ? CM_PAGEUP : CM_PAGEDOWN, 0);
                }
                else
                {
                    int i;
                    for (i = 0; i < abs(linesToScroll); i++)
                        SendMessage(HWindow, WM_COMMAND, zDelta > 0 ? CM_LINEUP : CM_LINEDOWN, 0);
                }
            }
        }

        // SHIFT: horizontal scrolling
        if (!controlPressed && !altPressed && shiftPressed)
        {
            // note: also called from WM_MOUSEHWHEEL
            zDelta = (short)HIWORD(wParam);

            DWORD wheelScroll = GetMouseWheelScrollLines(); // 'delta' can be up to WHEEL_PAGESCROLL(0xffffffff)
            DWORD pageWidth = max(1, (DWORD)(Width - BORDER_WIDTH) / CharWidth);
            wheelScroll = max(1, min(wheelScroll, pageWidth - 1)); // Limit to the maximum width of the page

            MouseHWheelAccumulator += 1000 * zDelta;
            int stepsPerChar = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int charsToScroll = MouseHWheelAccumulator / stepsPerChar;
            if (charsToScroll != 0)
            {
                MouseHWheelAccumulator -= charsToScroll * stepsPerChar;
                if (abs(charsToScroll) < abs((int)pageWidth - 1))
                {
                    int i;
                    for (i = 0; i < abs(charsToScroll); i++)
                        SendMessage(HWindow, WM_HSCROLL, zDelta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
                }
                else
                    SendMessage(HWindow, WM_HSCROLL, zDelta > 0 ? SB_PAGEUP : SB_PAGEDOWN, 0);
            }
        }

        return 0;
    }

    switch (uMsg)
    {
    case WM_CREATE:
    {
        ViewerWindowQueue.Add(new CWindowQueueItem(HWindow));

        FindDialog.SetParent(HWindow);
        EraseBkgnd = TRUE;
        if (FileName != NULL)
            SetViewerCaption();

        HToolTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL, TTS_NOPREFIX,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  NULL, NULL, HInstance, NULL);

        if (HToolTip != NULL)
        {
            TOOLINFO ti;
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_SUBCLASS;
            ti.hwnd = HWindow;
            ti.uId = 1;
            ti.hinst = HInstance;
            GetClientRect(HWindow, &ti.rect);
            ti.lpszText = LPSTR_TEXTCALLBACK;
            SendMessage(HToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            SendMessage(HToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, 500);
            SendMessage(HToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 10000);
            SetWindowPos(HToolTip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }

        DragAcceptFiles(HWindow, TRUE);
        break;
    }

    case WM_DROPFILES:
    {
        UINT drag;
        char path[MAX_PATH];

        drag = DragQueryFile((HDROP)wParam, 0xFFFFFFFF, NULL, 0); // how many files did they throw at us
        if (drag > 0)
        {
            DragQueryFile((HDROP)wParam, 0, path, MAX_PATH);
            if (SalGetFullName(path))
            {
                if (Lock != NULL)
                {
                    SetEvent(Lock);
                    Lock = NULL; // now it's just on the disk cache
                }
                OpenFile(path, NULL, FALSE);
            }
        }
        DragFinish((HDROP)wParam);
        break;
    }

    case WM_KILLFOCUS:
    {
        if (MainWindow != NULL)
        {
            // When deactivating the window, we set SkipOneActivateRefresh=TRUE for a moment, we cannot
            // to determine whether it switches to the main window or elsewhere
            // main window will not refresh when switching from the viewer
            SkipOneActivateRefresh = TRUE;
            PostMessage(MainWindow->HWindow, WM_USER_SKIPONEREFRESH, 0, 0);
        }
        break;
    }

    case WM_PAINT:
    {
        EraseBkgnd = FALSE;
        PAINTSTRUCT ps;
        HANDLES(BeginPaint(HWindow, &ps));
        Paint(ps.hdc);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_ERASEBKGND:
    {
        if (!EraseBkgnd)
            return TRUE; // do not erase the background
        else
        {
            RECT r;
            GetClientRect(HWindow, &r);
            FillRect((HDC)wParam, &r, BkgndBrush);
            return TRUE;
        }
    }

    case WM_SIZE:
    {
        if (IsWindowVisible(HWindow)) // the last WM_SIZE comes when closing the window, we don't care about that anymore (error messages without the viewer window are highly undesirable)
        {
            SetToolTipOffset(-1);
            BOOL widthChanged = (Width != LOWORD(lParam));
            Width = LOWORD(lParam);
            Bitmap.Enlarge(Width, CharHeight);
            if (Width < 0)
                Width = 0;
            if (Height != HIWORD(lParam) ||
                widthChanged && Type == vtText && WrapText)
            {
                BOOL fatalErr = FALSE;
                Height = HIWORD(lParam);
                if (Height < 0)
                    Height = 0;
                if (MaxSeekY == -1)
                    FileChanged(NULL, FALSE, fatalErr, TRUE);
                else
                {
                    BOOL calledHeightChanged;
                    FileChanged(NULL, TRUE, fatalErr, FALSE, &calledHeightChanged);
                    if (!fatalErr && !ExitTextMode && !calledHeightChanged) // initialize new file
                    {
                        HeightChanged(fatalErr);
                        if (!fatalErr && !ExitTextMode)
                            FindNewSeekY(SeekY, fatalErr);
                    }
                }
                if (fatalErr)
                    FatalFileErrorOccured();
            }
            else
            {
                if (FileName != NULL)
                {
                    // limit the shift according to the longest visible line
                    __int64 maxOX = GetMaxOriginX();
                    if (OriginX > maxOX)
                    {
                        OriginX = maxOX;
                        InvalidateRect(HWindow, NULL, FALSE); // for safety
                    }
                }
            }
            if (HToolTip != NULL)
            {
                TOOLINFO ti;
                ti.cbSize = sizeof(ti);
                ti.hwnd = HWindow;
                ti.uId = 1;
                GetClientRect(HWindow, &ti.rect);
                SendMessage(HToolTip, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);
            }
        }
        break;
    }

    case WM_USER_CFGCHANGED:
    {
        ReleaseViewerBrushs();
        CreateViewerBrushs();
        SetViewerFont();
        InvalidateRect(HWindow, NULL, TRUE);
        ConfigHasChanged();
        return 0;
    }

    case WM_USER_CLEARHISTORY:
    {
        // we need to trim the history in the Find dialog if it is open
        if (FindDialog.HWindow != NULL)
            SendMessage(FindDialog.HWindow, WM_USER_CLEARHISTORY, wParam, lParam);
        return 0;
    }

    case WM_VSCROLL:
    {
        if (FileName != NULL)
        {
            ResetMouseWheelAccumulator();
            switch ((int)LOWORD(wParam))
            {
            case SB_LINEUP:
                SendMessage(HWindow, WM_COMMAND, CM_LINEUP, 0);
                break;
            case SB_PAGEUP:
                SendMessage(HWindow, WM_COMMAND, CM_PAGEUP, 0);
                break;
            case SB_PAGEDOWN:
                SendMessage(HWindow, WM_COMMAND, CM_PAGEDOWN, 0);
                break;
            case SB_LINEDOWN:
                SendMessage(HWindow, WM_COMMAND, CM_LINEDOWN, 0);
                break;

            case SB_THUMBPOSITION:
            {
                // end of dragging, we must call OnVScroll() from here, otherwise the scrollbar will flicker at the old position
                VScrollWParam = wParam;
                KillTimer(HWindow, IDT_THUMBSCROLL);
                MSG msg; // we don't want any more timers, we will clean the queue
                while (PeekMessage(&msg, HWindow, WM_TIMER, WM_TIMER, PM_REMOVE))
                    ;
                OnVScroll();
                VScrollWParam = -1;
                break;
            }

            case SB_THUMBTRACK:
            {
                // Own scrolling code moved to timer because of USB mice and MS scrollbars
                // creating incredible things: if the viewer is fullscreen, render the whole
                // the screen already takes up something and the stupid scrollbar is waiting to be rendered, so
                // It drags like a snail; not even posting a message and postponing drawing helped
                // timer is the only way we have come up with
                if (VScrollWParam == -1)
                {
                    VScrollWParam = wParam;
                    VScrollWParamOld = -1;
                    SetTimer(HWindow, IDT_THUMBSCROLL, 20, NULL);
                }
                else
                    VScrollWParam = wParam;
                break;
            }
            }
        }
        return 0;
    }

    case WM_HSCROLL:
    {
        if (FileName != NULL)
        {
            ResetMouseWheelAccumulator();
            switch ((int)LOWORD(wParam))
            {
            case SB_LINELEFT:
                SendMessage(HWindow, WM_COMMAND, CM_LEFT, 0);
                break;
            case SB_LINERIGHT:
                SendMessage(HWindow, WM_COMMAND, CM_RIGHT, 0);
                break;
            case SB_PAGELEFT:
            case SB_PAGERIGHT:
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION:
            {
                switch ((int)LOWORD(wParam))
                {
                case SB_PAGELEFT:
                {
                    int step = ((Width - BORDER_WIDTH) / CharWidth);
                    if (step > 1)
                        step--;
                    OriginX -= step;
                    if (OriginX < 0)
                        OriginX = 0;
                    break;
                }

                case SB_THUMBTRACK:
                case SB_THUMBPOSITION:
                case SB_PAGERIGHT:
                {
                    if ((int)LOWORD(wParam) == SB_PAGERIGHT)
                    {
                        int step = ((Width - BORDER_WIDTH) / CharWidth);
                        if (step > 1)
                            step--;
                        OriginX += step;
                    }
                    else // SB_THUMBTRACK and SB_THUMBPOSITION
                    {
                        EnableSetScroll = ((int)LOWORD(wParam) == SB_THUMBPOSITION);
                        OriginX = (__int64)(ScrollScaleX * ((short)HIWORD(wParam)) + 0.5);
                    }

                    // limit the shift according to the longest visible line
                    __int64 maxOX = GetMaxOriginX();
                    if (OriginX > maxOX)
                        OriginX = maxOX;
                    break;
                }
                }
                ResetFindOffsetOnNextPaint = TRUE;
                InvalidateRect(HWindow, NULL, FALSE);
                UpdateWindow(HWindow); // to calculate the ViewSize for the next PageDown
            }
            }
        }
        return 0;
    }

    case WM_COMMAND:
    {
        if (!IsWindowEnabled(HWindow)) // measures against idiotic software that activates our main window while we have a modal dialog open (e.g. ClipMate)
            return 0;
        BOOL ch = FALSE;
        switch (LOWORD(wParam))
        {
        case CM_EXIT:
            DestroyWindow(HWindow);
            return 0;

        case CM_VIEWER_CONFIG:
        {
            if (!SalamanderBusy)
                PostMessage(MainWindow->HWindow, WM_USER_VIEWERCONFIG, (WPARAM)HWindow, 0);
            return 0;
        }

        case CM_OPENFILE:
        {
            if (MouseDrag)
                return 0;
            char file[MAX_PATH];
            file[0] = 0;
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = HWindow;
            char* s = LoadStr(IDS_VIEWERFILTER);
            ofn.lpstrFilter = s;
            while (*s != 0) // creating a double-null terminated list
            {
                if (*s == '|')
                    *s = 0;
                s++;
            }
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.nFilterIndex = 1;
            ofn.lpstrInitialDir = CurrentDir[0] != 0 ? CurrentDir : NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

            if (SafeGetOpenFileName(&ofn))
            {
                if (SalGetFullName(file))
                {
                    if (Lock != NULL)
                    {
                        SetEvent(Lock);
                        Lock = NULL; // now it's just on the disk cache
                    }
                    OpenFile(file, NULL, FALSE);
                }
            }
            return 0;
        }

        case CM_PREVFILE:
        case CM_NEXTFILE:
        case CM_PREVSELFILE:
        case CM_NEXTSELFILE:
        case CM_FIRSTFILE:
        case CM_LASTFILE:
        {
            BOOL ok = FALSE;
            BOOL srcBusy = FALSE;
            BOOL noMoreFiles = FALSE;
            char fileName[MAX_PATH];
            fileName[0] = 0;
            int enumFileNamesLastFileIndex = EnumFileNamesLastFileIndex;
            if (LOWORD(wParam) == CM_PREVFILE || LOWORD(wParam) == CM_PREVSELFILE || LOWORD(wParam) == CM_LASTFILE)
            {
                if (LOWORD(wParam) == CM_LASTFILE)
                    enumFileNamesLastFileIndex = -1;
                ok = GetPreviousFileNameForViewer(EnumFileNamesSourceUID,
                                                  &enumFileNamesLastFileIndex,
                                                  FileName, LOWORD(wParam) == CM_PREVSELFILE, TRUE,
                                                  fileName, &noMoreFiles,
                                                  &srcBusy, NULL);
                if (ok && LOWORD(wParam) == CM_PREVSELFILE) // we only take selected files
                {
                    BOOL isSrcFileSel = FALSE;
                    ok = IsFileNameForViewerSelected(EnumFileNamesSourceUID, enumFileNamesLastFileIndex,
                                                     fileName, &isSrcFileSel, &srcBusy);
                    if (ok && !isSrcFileSel)
                        ok = FALSE;
                }
            }
            else
            {
                if (LOWORD(wParam) == CM_FIRSTFILE)
                    enumFileNamesLastFileIndex = -1;
                ok = GetNextFileNameForViewer(EnumFileNamesSourceUID,
                                              &enumFileNamesLastFileIndex,
                                              FileName, LOWORD(wParam) == CM_NEXTSELFILE, TRUE,
                                              fileName, &noMoreFiles,
                                              &srcBusy, NULL);
                if (ok && LOWORD(wParam) == CM_NEXTSELFILE) // we only take selected files
                {
                    BOOL isSrcFileSel = FALSE;
                    ok = IsFileNameForViewerSelected(EnumFileNamesSourceUID, enumFileNamesLastFileIndex,
                                                     fileName, &isSrcFileSel, &srcBusy);
                    if (ok && !isSrcFileSel)
                        ok = FALSE;
                }
            }

            if (ok) // we have a new name
            {
                if (Lock != NULL)
                {
                    SetEvent(Lock);
                    Lock = NULL; // now it's just on the disk cache
                }
                OpenFile(fileName, NULL, FALSE);

                // Set the index even in case of failure so that the user can move to the next/previous file
                EnumFileNamesLastFileIndex = enumFileNamesLastFileIndex;
            }
            else
            {
                if (noMoreFiles)
                    TRACE_I("Next/previous file does not exist.");
                else
                {
                    if (srcBusy)
                        TRACE_I("Connected panel or Find window is busy, please try to repeat your request later.");
                    else
                    {
                        if (EnumFileNamesSourceUID == -1)
                            TRACE_I("This service is not available from archive nor file system path.");
                        else
                            TRACE_I("Connected panel or Find window does not contain original list of files.");
                    }
                }
            }
            return 0;
        }

        case CM_VIEW_FULLSCREEN:
        {
            if (IsZoomed(HWindow))
                ShowWindow(HWindow, SW_RESTORE);
            else
                ShowWindow(HWindow, SW_MAXIMIZE);
            return 0;
        }

        case CM_FINDNEXT:
        case CM_FINDPREV:
        case CM_FINDSET:
        {
            if (MouseDrag)
                return 0;
            if (LastFindSeekY == SeekY && LastFindOffset != FindOffset) // Restore FindOffset after moving back and forth
            {
                FindOffset = LastFindOffset;
            }

            if (LOWORD(wParam) == CM_FINDSET || FindDialog.Text[0] == 0)
            {
                int forw = FindDialog.Forward;
                if (FindDialog.Execute() != IDOK || FindDialog.Text[0] == 0)
                    return 0;
                else
                {
                    InitFindDialog(FindDialog);
                    if (FindDialog.Forward != forw)
                    {
                        FindOffset = SeekY;
                        if (!FindDialog.Forward)
                            FindOffset += ViewSize;
                    }
                }
            }
            BOOL forward = (LOWORD(wParam) != CM_FINDPREV) ^ (!FindDialog.Forward);
            WORD flags = (WORD)((FindDialog.CaseSensitive ? sfCaseSensitive : 0) |
                                (forward ? sfForward : 0));
            int found = -1;
            __int64 oldFindOffset = FindOffset;
            if (StartSelection != EndSelection && SelectionIsFindResult &&
                (FindOffset == StartSelection || FindOffset == EndSelection))
            { // We will ensure that when changing the search direction (F3/Shift+F3), the first search is not unnecessary.
                if (forward)
                    FindOffset = max(StartSelection, EndSelection);
                else
                    FindOffset = min(StartSelection, EndSelection);
            }
            BOOL noNotFound = FALSE;
            BOOL escPressed = FALSE;

            BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
            HCURSOR oldCur;
            if (setWait)
                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            CreateSafeWaitWindow(LoadStr(IDS_SEARCHINGTEXTESC), LoadStr(IDS_VIEWERTITLE), 1000, TRUE, HWindow);
            GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - see help

            // we will only open the file once in the Prepare() call and close it ourselves at the end
            // Repetitive opening/closing was delaying file searching on the network drive (1.5MB)
            HANDLE hFile = NULL;

            BOOL fatalErr = FALSE;
            FindingSoDonotSwitchToHex = TRUE; // During the search, we will disable switching to "hex" when there are more than 10000 characters per line
            if (FindDialog.Regular)
            {
                if (RegExp.SetFlags(flags))
                {
                    BOOL oldConfigEOL_NULL = Configuration.EOL_NULL;
                    Configuration.EOL_NULL = TRUE; // I don't have regex for binary strings :(
                    __int64 len;
                    __int64 lineEnd;
                    __int64 lineBegin;
                    if (forward)
                    {
                        __int64 nextLineBegin;
                        if (!FindPreviousEOL(&hFile, FindOffset, FindOffset - FIND_LINE_LEN,
                                             lineBegin, nextLineBegin /* dummy*/, FALSE, TRUE, fatalErr, NULL))
                        { // beginning out of sight
                            lineBegin = FindOffset;
                        }

                        if (!fatalErr /*&& !ExitTextMode*/) // FindingSoDonotSwitchToHex is TRUE, ExitTextMode cannot occur
                        {
                            while (lineBegin < FileSize)
                            {
                                __int64 maxSeek = min(FileSize, lineBegin + FIND_LINE_LEN);
                                if (!FindNextEOL(&hFile, lineBegin, maxSeek, lineEnd, nextLineBegin, fatalErr))
                                { // end nowhere in sight
                                    lineEnd = nextLineBegin = maxSeek;
                                }
                                if (fatalErr)
                                    break;

                                if (lineBegin < lineEnd) // block of text from lineBegin to lineEnd
                                {
                                    len = Prepare(&hFile, lineBegin, lineEnd - lineBegin, fatalErr);
                                    if (fatalErr)
                                        break;
                                    if (len == lineEnd - lineBegin)
                                    {
                                        if (RegExp.SetLine((char*)(Buffer + (lineBegin - Seek)),
                                                           (char*)(Buffer + (lineEnd - Seek))))
                                        {
                                            int start;
                                            if (FindOffset > lineBegin)
                                                start = (int)(FindOffset - lineBegin);
                                            else
                                                start = 0;
                                            int foundLen;

                                        REGEXP_FIND_NEXT_FORWARD:

                                            found = RegExp.SearchForward(start, foundLen);

                                            if (found != -1 && FindDialog.WholeWords)
                                            {
                                                BOOL fail = FALSE;
                                                if (found > 0)
                                                {
                                                    if (Prepare(&hFile, lineBegin + found - 1, 1, fatalErr) == 1 && !fatalErr)
                                                    {
                                                        char c = *(Buffer + (lineBegin + found - 1 - Seek));
                                                        fail |= (c == '_' || IsCharAlpha(c) || IsCharAlphaNumeric(c));
                                                    }
                                                    if (fatalErr)
                                                        break;
                                                }
                                                if (found + foundLen < lineEnd - lineBegin &&
                                                    Prepare(&hFile, lineBegin + found + foundLen, 1, fatalErr) == 1 && !fatalErr)
                                                {
                                                    char c = *(Buffer + (lineBegin + found + foundLen - Seek));
                                                    fail |= (c == '_' || IsCharAlpha(c) || IsCharAlphaNumeric(c));
                                                }
                                                if (fatalErr)
                                                    break;
                                                if (fail)
                                                {
                                                    start = found + 1;
                                                    if (start < lineEnd - lineBegin)
                                                        goto REGEXP_FIND_NEXT_FORWARD;
                                                    found = -1;
                                                }
                                            }

                                            if (found != -1)
                                            {
                                                if (foundLen == 0)
                                                {
                                                    SalMessageBox(HWindow, LoadStr(IDS_EMPTYMATCH),
                                                                  LoadStr(IDS_FINDTITLE),
                                                                  MB_OK | MB_ICONINFORMATION);
                                                    noNotFound = TRUE;
                                                    break;
                                                }
                                                StartSelection = lineBegin + found;
                                                FindOffset = EndSelection = StartSelection + foundLen;
                                                SelectionIsFindResult = TRUE;
                                                break; // found!
                                            }
                                        }
                                        else // error - low memory
                                        {
                                            SalMessageBox(HWindow, RegExp.GetLastErrorText(), LoadStr(IDS_FINDTITLE),
                                                          MB_OK | MB_ICONEXCLAMATION);
                                            noNotFound = TRUE;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        TRACE_E("Unable to read a line - unexpected error.");
                                    }
                                }

                                lineBegin = nextLineBegin;

                                if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && ViewerActive(HWindow) ||
                                    GetSafeWaitWindowClosePressed())
                                {
                                    escPressed = TRUE;
                                    break;
                                }
                            }
                        }
                    }
                    else // backward
                    {
                        __int64 previousLineEnd;
                        if (!FindNextEOL(&hFile, FindOffset, FindOffset + FIND_LINE_LEN,
                                         lineEnd, previousLineEnd /* dummy*/, fatalErr))
                        { // end nowhere in sight
                            lineEnd = FindOffset;
                        }

                        if (!fatalErr)
                        {
                            while (lineEnd > 0)
                            {
                                if (!FindPreviousEOL(&hFile, lineEnd, lineEnd - FIND_LINE_LEN,
                                                     lineBegin, previousLineEnd, FALSE, TRUE, fatalErr, NULL))
                                { // beginning out of sight
                                    lineBegin = previousLineEnd = lineEnd - FIND_LINE_LEN;
                                }
                                if (fatalErr /*|| ExitTextMode*/)
                                    break; // FindingSoDonotSwitchToHex is TRUE, ExitTextMode cannot occur

                                if (lineBegin < lineEnd) // block of text from lineBegin to lineEnd
                                {
                                    len = Prepare(&hFile, lineBegin, lineEnd - lineBegin, fatalErr);
                                    if (fatalErr)
                                        break;
                                    if (len == lineEnd - lineBegin)
                                    {
                                        if (RegExp.SetLine((char*)(Buffer + (lineBegin - Seek)),
                                                           (char*)(Buffer + (lineEnd - Seek))))
                                        {
                                            int length;
                                            if (FindOffset < lineEnd)
                                                length = (int)(FindOffset - lineBegin);
                                            else
                                                length = (int)(lineEnd - lineBegin);
                                            int foundLen;

                                        REGEXP_FIND_NEXT_BACKWARD:

                                            found = RegExp.SearchBackward(length, foundLen);

                                            if (found != -1 && FindDialog.WholeWords)
                                            {
                                                BOOL fail = FALSE;
                                                if (found > 0)
                                                {
                                                    if (Prepare(&hFile, lineBegin + found - 1, 1, fatalErr) == 1 && !fatalErr)
                                                    {
                                                        char c = *(Buffer + (lineBegin + found - 1 - Seek));
                                                        fail |= (c == '_' || IsCharAlpha(c) || IsCharAlphaNumeric(c));
                                                    }
                                                    if (fatalErr)
                                                        break;
                                                }
                                                if (found + foundLen < lineEnd - lineBegin &&
                                                    Prepare(&hFile, lineBegin + found + foundLen, 1, fatalErr) == 1 && !fatalErr)
                                                {
                                                    char c = *(Buffer + (lineBegin + found + foundLen - Seek));
                                                    fail |= (c == '_' || IsCharAlpha(c) || IsCharAlphaNumeric(c));
                                                }
                                                if (fatalErr)
                                                    break;
                                                if (fail)
                                                {
                                                    length = found + foundLen - 1;
                                                    if (length > 0)
                                                        goto REGEXP_FIND_NEXT_BACKWARD;
                                                    found = -1;
                                                }
                                            }

                                            if (found != -1)
                                            {
                                                if (foundLen == 0)
                                                {
                                                    SalMessageBox(HWindow, LoadStr(IDS_EMPTYMATCH),
                                                                  LoadStr(IDS_FINDTITLE),
                                                                  MB_OK | MB_ICONINFORMATION);
                                                    noNotFound = TRUE;
                                                    break;
                                                }
                                                FindOffset = StartSelection = lineBegin + found;
                                                EndSelection = StartSelection + foundLen;
                                                SelectionIsFindResult = TRUE;
                                                break; // found!
                                            }
                                        }
                                        else // error - low memory
                                        {
                                            SalMessageBox(HWindow, RegExp.GetLastErrorText(), LoadStr(IDS_FINDTITLE),
                                                          MB_OK | MB_ICONEXCLAMATION);
                                            noNotFound = TRUE;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        TRACE_E("Unable to read a line - unexpected error.");
                                    }
                                }

                                lineEnd = previousLineEnd;

                                if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && ViewerActive(HWindow) ||
                                    GetSafeWaitWindowClosePressed())
                                {
                                    escPressed = TRUE;
                                    break;
                                }
                            }
                        }
                    }
                    Configuration.EOL_NULL = oldConfigEOL_NULL;
                }
                else
                {
                    char buf[500];
                    if (RegExp.GetPattern() != NULL)
                        sprintf(buf, LoadStr(IDS_INVALIDREGEXP), RegExp.GetPattern(), RegExp.GetLastErrorText());
                    else
                        strcpy(buf, RegExp.GetLastErrorText());
                    SalMessageBox(HWindow, buf, LoadStr(IDS_FINDTITLE), MB_OK | MB_ICONEXCLAMATION);
                    noNotFound = TRUE;
                }
            }
            else
            {
                SearchData.SetFlags(flags);
                if (SearchData.IsGood())
                {
                    if (forward)
                    {
                        while (1)
                        {
                            __int64 len = Prepare(&hFile, FindOffset, FIND_LINE_LEN, fatalErr);
                            if (fatalErr)
                                break;
                            if (len >= SearchData.GetLength())
                            {
                                found = SearchData.SearchForward((char*)(Buffer + (FindOffset - Seek)),
                                                                 (int)len, 0);
                                if (found != -1 && FindDialog.WholeWords)
                                {
                                    BOOL fail = FALSE;
                                    if (FindOffset + found > 0)
                                    {
                                        if (Prepare(&hFile, FindOffset + found - 1, 1, fatalErr) == 1 && !fatalErr)
                                        {
                                            char c = *(Buffer + (FindOffset + found - 1 - Seek));
                                            fail |= (c == '_' || IsCharAlpha(c) || IsCharAlphaNumeric(c));
                                        }
                                        if (fatalErr)
                                            break;
                                    }
                                    if (Prepare(&hFile, FindOffset + found + SearchData.GetLength(), 1, fatalErr) == 1 && !fatalErr)
                                    {
                                        char c = *(Buffer + (FindOffset + found + SearchData.GetLength() - Seek));
                                        fail |= (c == '_' || IsCharAlpha(c) || IsCharAlphaNumeric(c));
                                    }
                                    if (fatalErr)
                                        break;
                                    if (fail)
                                    {
                                        len = found + SearchData.GetLength();
                                        found = -1;
                                    }
                                }
                                if (found != -1)
                                {
                                    StartSelection = FindOffset + found;
                                    FindOffset = EndSelection = StartSelection + SearchData.GetLength();
                                    SelectionIsFindResult = TRUE;
                                    break;
                                }
                                len -= SearchData.GetLength() - 1;
                                if (len >= 0)
                                    FindOffset += len;
                                else
                                    break; // end of file
                            }
                            else
                                break; // end of file

                            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && ViewerActive(HWindow) ||
                                GetSafeWaitWindowClosePressed())
                            {
                                escPressed = TRUE;
                                break;
                            }
                        }
                    }
                    else
                    {
                        while (1)
                        {
                            __int64 off, len;
                            if (FindOffset > 0)
                            {
                                off = FindOffset - FIND_LINE_LEN;
                                len = FIND_LINE_LEN;
                                if (off < 0)
                                {
                                    len += off;
                                    off = 0;
                                }
                            }
                            else
                                break; // start of file
                            len = Prepare(&hFile, off, len, fatalErr);
                            if (fatalErr)
                                break;
                            if (len >= SearchData.GetLength())
                            {
                                found = SearchData.SearchBackward((char*)(Buffer + (off - Seek)), (int)len);
                                if (found != -1 && FindDialog.WholeWords)
                                {
                                    BOOL fail = FALSE;
                                    if (off + found > 0)
                                    {
                                        if (Prepare(&hFile, off + found - 1, 1, fatalErr) == 1 && !fatalErr)
                                        {
                                            char c = *(Buffer + (off + found - 1 - Seek));
                                            fail |= (c == '_' || IsCharAlpha(c) || IsCharAlphaNumeric(c));
                                        }
                                        if (fatalErr)
                                            break;
                                    }
                                    if (Prepare(&hFile, off + found + SearchData.GetLength(), 1, fatalErr) == 1 && !fatalErr)
                                    {
                                        char c = *(Buffer + (off + found + SearchData.GetLength() - Seek));
                                        fail |= (c == '_' || IsCharAlpha(c) || IsCharAlphaNumeric(c));
                                    }
                                    if (fatalErr)
                                        break;
                                    if (fail)
                                    {
                                        len -= found;
                                        found = -1;
                                    }
                                }
                                if (found != -1)
                                {
                                    FindOffset = StartSelection = off + found;
                                    EndSelection = StartSelection + SearchData.GetLength();
                                    SelectionIsFindResult = TRUE;
                                    break;
                                }
                                len -= SearchData.GetLength() - 1;
                                if (len >= 0)
                                    FindOffset -= len;
                                else
                                    break; // start of file
                            }
                            else
                                break; // start of file

                            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && ViewerActive(HWindow) ||
                                GetSafeWaitWindowClosePressed())
                            {
                                escPressed = TRUE;
                                break;
                            }
                        }
                    }
                }
            }
            FindingSoDonotSwitchToHex = FALSE;

            // if the file was successfully opened, closing is our responsibility
            if (hFile != NULL)
                HANDLES(CloseHandle(hFile));

            DestroySafeWaitWindow();
            if (setWait)
                SetCursor(oldCur);
            if (fatalErr)
            {
                FatalFileErrorOccured();
                return 0;
            }

            if (escPressed)
            {
                MSG msg; // we discard the buffered ESC
                while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                    ;
                SalMessageBox(HWindow, LoadStr(IDS_FINDTERMINATEDBYUSER),
                              LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION | MSGBOXEX_SILENT);
                found = -1;
                noNotFound = TRUE;
            }

            if (found == -1)
            {
                EndSelection = StartSelection;
                FindOffset = oldFindOffset;
                if (!noNotFound)
                {
                    char buff[5000];
                    sprintf(buff, LoadStr(FindDialog.Regular ? IDS_FIND_NOREGEXPMATCH : IDS_FIND_NOMATCH), FindDialog.Text);
                    SalMessageBox(HWindow, buff, LoadStr(IDS_FINDTITLE), MB_OK | MB_ICONINFORMATION | MSGBOXEX_SILENT);
                }
            }
            else
            {
                __int64 startSel = StartSelection == -1 ? 0 : StartSelection;
                __int64 endSel = EndSelection == -1 ? 0 : EndSelection;
                if (startSel == endSel)
                    startSel = endSel = 0;
                if (startSel < SeekY || endSel > SeekY + ViewSize)
                { // display selection - if possible, scroll down by three lines
                    __int64 lineOff = FindBegin(startSel, fatalErr);
                    if (fatalErr)
                        FatalFileErrorOccured();
                    if (fatalErr || ExitTextMode)
                        return 0;

                    int line = 0;
                    while (line < (Height / CharHeight - 1) / 2 && line < 3)
                    {
                        SeekY = lineOff;
                        lineOff -= ZeroLineSize(fatalErr);
                        if (fatalErr)
                            FatalFileErrorOccured();
                        if (fatalErr || ExitTextMode)
                            return 0;
                        if (lineOff <= 0)
                        {
                            lineOff = 0;
                            break;
                        }
                        line++;
                    }
                    SeekY = min(lineOff, MaxSeekY);
                }
                ScrollToSelection = TRUE;
            }
            InvalidateRect(HWindow, NULL, FALSE);
            // remembering the position of the last search, for detecting intermittent motion
            LastFindSeekY = SeekY;
            LastFindOffset = FindOffset;

            return 0;
        }

        case CM_COPYTOCLIP:
        {
            if (MouseDrag)
                return 0;
            if (StartSelection != EndSelection &&
                CheckSelectionIsNotTooBig(HWindow))
            {
                BOOL fatalErr = FALSE;
                HGLOBAL h = GetSelectedText(fatalErr);
                if (h != NULL)
                {
                    __int64 startSel = min(StartSelection, EndSelection);
                    // if (startSel == -1) startSel = 0; // cannot happen (-1 can only be both at the same time and that won't happen here)
                    __int64 endSel = max(StartSelection, EndSelection);
                    // if (endSel == -1) endSel = 0; // cannot happen (-1 can only be both at the same time and that won't happen here)
                    if (fatalErr || !CopyHTextToClipboard(h, (int)(endSel - startSel)))
                        NOHANDLES(GlobalFree(h));
                }
                if (fatalErr)
                    FatalFileErrorOccured();
            }
            return 0;
        }

        case CM_COPYTOFILE:
        {
            if (MouseDrag)
                return 0;
            if (FileName != NULL)
            {
                __int64 start = min(StartSelection, EndSelection);
                // if (startSel == -1) startSel = 0; // unnecessary to solve, it will be solved a few lines below
                __int64 end = max(StartSelection, EndSelection);
                // if (endSel == -1) endSel = 0;     // unnecessary to solve, it will be solved a few lines below
                if (StartSelection == EndSelection)
                {
                    start = 0;
                    end = FileSize;
                }

            ENTER_AGAIN:

                char fileName[MAX_PATH];
                strcpy(fileName, FileName);
                OPENFILENAME ofn;
                memset(&ofn, 0, sizeof(OPENFILENAME));
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = HWindow;
                char* s = LoadStr(IDS_VIEWERFILTER);
                ofn.lpstrFilter = s;
                while (*s != 0) // creating a double-null terminated list
                {
                    if (*s == '|')
                        *s = 0;
                    s++;
                }
                ofn.lpstrFile = fileName;
                ofn.nMaxFile = MAX_PATH;
                ofn.nFilterIndex = 1;
                ofn.lpstrTitle = LoadStr(IDS_VIEWERCOPYTOFILE);
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_NOCHANGEDIR;

                if (SafeGetSaveFileName(&ofn))
                {
                    int errTextID;
                    if (!SalGetFullName(fileName, &errTextID))
                    {
                        SalMessageBox(HWindow, LoadStr(errTextID), LoadStr(IDS_ERRORTITLE),
                                      MB_OK | MB_ICONEXCLAMATION);
                        goto ENTER_AGAIN;
                    }

                    DWORD attr;
                    attr = SalGetFileAttributes(fileName);

                    if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_NAMEALREADYUSEDFORDIR),
                                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        goto ENTER_AGAIN;
                    }
                    if (attr != 0xFFFFFFFF)
                    {
                        char text[300];
                        sprintf(text, LoadStr(IDS_FILEALREADYEXIST), fileName);
                        int res = SalMessageBox(HWindow, text, LoadStr(IDS_VIEWERTITLE),
                                                MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
                        if (res == IDNO)
                            goto ENTER_AGAIN;
                        if (res == IDCANCEL)
                            return 0;
                    }

                    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                    char tmpFile[MAX_PATH];
                    char* endBackSlash = strrchr(fileName, '\\');
                    char path[MAX_PATH];
                    if (endBackSlash != NULL)
                    {
                        if (attr != 0xFFFFFFFF) // file overwrite -> we will perform via a tmp file (due to self-overwrite)
                        {
                            memcpy(path, fileName, endBackSlash - fileName + 1);
                            path[endBackSlash - fileName + 1] = 0;
                        }
                        else
                            strcpy(tmpFile, fileName);
                        if (attr == 0xFFFFFFFF || SalGetTempFileName(path, "sal", tmpFile, TRUE))
                        {
                            HANDLE file = HANDLES_Q(CreateFile(tmpFile, GENERIC_WRITE,
                                                               FILE_SHARE_READ, NULL,
                                                               CREATE_ALWAYS,
                                                               FILE_FLAG_SEQUENTIAL_SCAN,
                                                               NULL));
                            if (file != INVALID_HANDLE_VALUE)
                            {
                                __int64 off = start, len;
                                ULONG written;
                                BOOL fatalErr = FALSE;
                                while (off < end)
                                {
                                    len = Prepare(NULL, off, min(VIEW_BUFFER_SIZE, end - off), fatalErr);
                                    if (fatalErr)
                                        break;
                                    if (len == 0)
                                        break; // read error
                                    if (!WriteFile(file, Buffer + (off - Seek), (int)len, &written, NULL) ||
                                        written != len)
                                    {
                                        DWORD err = GetLastError();
                                        SetCursor(oldCur);
                                        SalMessageBox(HWindow, GetErrorText(err),
                                                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                                        break;
                                    }
                                    off += len;
                                }
                                HANDLES(CloseHandle(file));
                                if (fatalErr || off != end)
                                    DeleteFile(tmpFile); // delete it in case of an error
                                else
                                {
                                    if (attr != 0xFFFFFFFF) // overwrite: tmp -> fileName
                                    {
                                        BOOL setAttr = ClearReadOnlyAttr(fileName, attr); // for the case of a read-only file
                                        if (DeleteFile(fileName))
                                        {
                                            if (!SalMoveFile(tmpFile, fileName))
                                            {
                                                DWORD err = GetLastError();
                                                SetCursor(oldCur);
                                                SalMessageBox(HWindow, GetErrorText(err),
                                                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                                            }
                                        }
                                        else
                                        {
                                            DWORD err = GetLastError();
                                            if (setAttr)
                                                SetFileAttributes(fileName, attr); // restore original attributes
                                            DeleteFile(tmpFile);                   // File cannot be overwritten, let's delete the tmp file (it's useless)
                                            SetCursor(oldCur);
                                            SalMessageBox(HWindow, GetErrorText(err),
                                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                                        }
                                    }
                                }

                                // we report a change on the road (a new file has been added)
                                lstrcpyn(path, fileName, MAX_PATH);
                                CutDirectory(path);
                                if (MainWindow != NULL)
                                    MainWindow->PostChangeOnPathNotification(path, FALSE);

                                if (fatalErr)
                                    FatalFileErrorOccured();
                            }
                            else
                            {
                                DWORD err = GetLastError();
                                if (attr != 0xFFFFFFFF)
                                    DeleteFile(tmpFile);
                                SetCursor(oldCur);
                                SalMessageBox(HWindow, GetErrorText(err),
                                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                            }
                            SetCursor(oldCur);
                        }
                        else
                        {
                            DWORD err = GetLastError();
                            SetCursor(oldCur);
                            SalMessageBox(HWindow, GetErrorText(err),
                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        }
                    }
                }
            }
            return 0;
        }

        case CM_SELECTALLTEXT:
        {
            if (MouseDrag)
                return 0;
            StartSelection = 0;
            EndSelection = FileSize;
            SelectionIsFindResult = FALSE;
            InvalidateRect(HWindow, NULL, FALSE);
            UpdateWindow(HWindow);

            if (Configuration.AutoCopySelection && StartSelection != EndSelection)
                PostMessage(HWindow, WM_COMMAND, CM_COPYTOCLIP, 0);
            return 0;
        }

        case CM_TO_HEX:
        {
            if (MouseDrag)
                return 0;
            ExitTextMode = FALSE;
            ForceTextMode = FALSE;
            if (FileName != NULL)
            {
                if (Type != vtHex)
                {
                    OriginX = 0;
                    ChangeType(vtHex);
                }
            }
            else
                Type = vtHex;
            return 0;
        }

        case CM_TO_TEXT:
        {
            if (MouseDrag)
                return 0;
            ExitTextMode = FALSE;
            ForceTextMode = FALSE;
            if (FileName != NULL)
            {
                if (Type != vtText)
                {
                    OriginX = 0;
                    ChangeType(vtText);
                }
            }
            else
                Type = vtText;
            return 0;
        }

        case CM_VIEW_AUTOSEL:
        {
            if (DefViewMode == 0)
            {
                if (Type == vtText)
                    DefViewMode = 1;
                else
                    DefViewMode = 2;
            }
            else
                DefViewMode = 0;
            return 0;
        }

        case CM_VIEW_SETDEFAULT:
        {
            if (Type == vtText)
                DefViewMode = 1;
            else
                DefViewMode = 2;
            return 0;
        }

        case CM_WRAPED:
        {
            if (MouseDrag)
                return 0;
            if (FileName != NULL)
            {
                if (Type == vtText)
                {
                    WrapText = !WrapText;
                    OriginX = 0;
                    ChangeType(vtText);
                }
            }
            return 0;
        }

        case CM_GOTOOFFSET:
        {
            if (MouseDrag || FileName == NULL)
                return 0;
            __int64 offset = SeekY;
            if (CViewerGoToOffsetDialog(HWindow, &offset).Execute() == IDOK)
            {
                EndSelectionRow = -1; // disable optimization
                SeekY = offset;
                SeekY = min(SeekY, MaxSeekY);

                BOOL fatalErr = FALSE;
                __int64 newSeekY = FindBegin(SeekY, fatalErr);
                if (fatalErr)
                    FatalFileErrorOccured();
                if (fatalErr || ExitTextMode)
                    return 0;
                SeekY = newSeekY;

                ResetFindOffsetOnNextPaint = TRUE;
                InvalidateRect(HWindow, NULL, FALSE);
                UpdateWindow(HWindow); // to calculate the ViewSize for the next PageDown
            }
            return 0;
        }

        case CM_RECOGNIZE_CODEPAGE:
        {
            CodePageAutoSelect = !CodePageAutoSelect;
            DefaultConvert[0] = 0;
            return 0;
        }

        case CM_SETDEFAULT_CODING:
        {
            CodePageAutoSelect = FALSE;
            if (!CodeTables.GetCodeName(CodeType, DefaultConvert, 200))
                DefaultConvert[0] = 0;
            return 0;
        }

        case CM_VIEWER_AUTOCOPY:
        {
            Configuration.AutoCopySelection = !Configuration.AutoCopySelection;

            if (MainWindow != NULL && MainWindow->HWindow != NULL) // we export the value to the plugin as SALCFG_AUTOCOPYSELTOCLIPBOARD, thus we notify about its change
                PostMessage(MainWindow->HWindow, WM_USER_DISPACHCFGCHANGE, 0, 0);

            return 0;
        }

        case CM_REREADFILE:
        {
            if (MouseDrag)
                return 0;
            ExitTextMode = FALSE;
            ForceTextMode = FALSE;
            if (FileName != NULL)
            {
                OriginX = 0;
                ChangeType(Type);
            }
            return 0;
        }

        case CM_VIEWERHLP_KEYBOARD:
        {
            OpenHtmlHelp(NULL, HWindow, HHCDisplayContext, IDH_VIEWERKEYBOARD, FALSE);
            return 0;
        }

        case CM_VIEWERHLP_INTRO:
        {
            OpenHtmlHelp(NULL, HWindow, HHCDisplayTOC, 0, TRUE); // We don't want two message boxes in a row
            OpenHtmlHelp(NULL, HWindow, HHCDisplayContext, IDH_VIEWERINTRO, FALSE);
            return 0;
        }

        case CM_NEXTCODING:
        {
            if (MouseDrag)
                return 0;
            CodeTables.Next(CodeType);
            PostMessage(HWindow, WM_COMMAND, CM_CODING_MIN + CodeType, 0);
            return 0;
        }

        case CM_PREVCODING:
        {
            CodeTables.Previous(CodeType);
            if (MouseDrag)
                return 0;
            PostMessage(HWindow, WM_COMMAND, CM_CODING_MIN + CodeType, 0);
            return 0;
        }

        default:
        {
            int c = LOWORD(wParam) - CM_CODING_MIN;
            if (!MouseDrag && CodeTables.Valid(c))
            {
                SetCodeType(c);
                BOOL fatalErr = FALSE;
                __int64 newSeekY = FindBegin(SeekY, fatalErr);
                if (fatalErr)
                    FatalFileErrorOccured();
                if (fatalErr || ExitTextMode)
                    return 0;
                SeekY = newSeekY;
                FileChanged(NULL, FALSE, fatalErr, FALSE);
                if (fatalErr)
                    FatalFileErrorOccured();
                if (fatalErr || ExitTextMode)
                    return 0;
                OriginX = 0;
                ResetFindOffsetOnNextPaint = TRUE;
                InvalidateRect(HWindow, NULL, FALSE);
                UpdateWindow(HWindow); // to calculate the ViewSize for the next PageDown
            }
            break;
        }
        }

        if (FileName != NULL)
        {
            BOOL extSelCh = FALSE;
            BOOL updateView = TRUE;
            BOOL skipCmd = FALSE;
            switch (LOWORD(wParam))
            {
            case CM_LINEUP:
            {
                if (!ScrollViewLineUp(CM_LINEUP))
                    return 0;
                break;
            }

            case CM_LINEDOWN:
            {
                ScrollViewLineDown();
                break;
            }

            case CM_PAGEUP:
            {
                if (SeekY > 0)
                {
                    EndSelectionRow = -1; // disable optimization
                    switch (Type)
                    {
                    case vtHex:
                    {
                        __int64 len = ViewSize - LastLineSize;
                        if ((len % 16) != 0)
                            len += 16 - (len % 16);
                        SeekY = max(0, SeekY - max(16, len));
                        break;
                    }

                    case vtText:
                    {
                        BOOL fatalErr = FALSE;
                        __int64 newSeekY = FindSeekBefore(SeekY, max(2, Height / CharHeight), fatalErr);
                        if (fatalErr)
                            FatalFileErrorOccured(CM_PAGEUP);
                        if (fatalErr || ExitTextMode)
                            return 0;
                        SeekY = newSeekY;
                        break;
                    }
                    }
                    ch = TRUE;
                }
                break;
            }

            case CM_PAGEDOWN:
            {
                if (SeekY < MaxSeekY)
                {
                    EndSelectionRow = -1; // disable optimization
                    __int64 size = max(0, ViewSize - LastLineSize);
                    if (size == 0) // not a single complete line -> emulation of arrow down
                    {
                        SeekY = min(MaxSeekY, SeekY + FirstLineSize);
                    }
                    else // classic page down
                    {
                        SeekY = min(MaxSeekY, SeekY + size);
                    }
                    ch = TRUE;
                }
                break;
            }

            case CM_EXTSEL_FILEBEG:
            {
                if (StartSelection == EndSelection || MouseDrag)
                {
                    skipCmd = TRUE;
                    break;
                }
                EndSelectionPrefX = -1;
                if (EndSelection != 0)
                    ChangingSelWithShiftKey = TRUE;
                EndSelection = 0; // EndSelectionRow is not used because MouseDrag==FALSE

                extSelCh = TRUE;
                // break; // break is intentionally missing here
            }
            case CM_FILEBEGIN:
            {
                if (FindDialog.Forward) // Disrupting detection of searching here and there, it's pleasant here...
                {
                    LastFindSeekY = -1;
                    LastFindOffset = 0;
                }
                if (SeekY != 0 || OriginX != 0)
                {
                    EndSelectionRow = -1; // disable optimization
                    SeekY = 0;
                    OriginX = 0;
                    ch = TRUE;
                }
                break;
            }

            case CM_FILEEND:
            case CM_EXTSEL_FILEEND:
            {
                __int64 newOriginX = 0;
                if (LOWORD(wParam) == CM_EXTSEL_FILEEND)
                {
                    if (StartSelection == EndSelection || MouseDrag)
                    {
                        skipCmd = TRUE;
                        break;
                    }

                    if (Type == vtText && !WrapText)
                    {
                        BOOL fatalErr;
                        __int64 lineBegOff, previousLineEnd, lineCharLen;
                        FindPreviousEOL(NULL, FileSize, 0, lineBegOff, previousLineEnd, TRUE, FALSE,
                                        fatalErr, NULL, NULL, &lineCharLen);
                        if (fatalErr)
                            FatalFileErrorOccured();
                        if (fatalErr || ExitTextMode)
                            return 0;

                        if (lineCharLen > 0) // Ensure visibility of the end of the last line (end of the selection)
                        {
                            __int64 oldOX = OriginX;
                            if (SeekY != MaxSeekY)
                                OriginX = 0;
                            BOOL fullRedraw;
                            EnsureXVisibleInView(lineCharLen, TRUE, fullRedraw, -1, -1, lineCharLen);
                            newOriginX = OriginX;
                            OriginX = oldOX;
                        }
                    }

                    EndSelectionPrefX = -1;
                    if (EndSelection != FileSize)
                        ChangingSelWithShiftKey = TRUE;
                    EndSelection = FileSize; // EndSelectionRow is not used because MouseDrag==FALSE

                    extSelCh = TRUE;
                }

                if (!FindDialog.Forward) // Disrupting detection of searching here and there, it's pleasant here...
                {
                    LastFindSeekY = -1;
                    LastFindOffset = 0;
                }
                if (SeekY != MaxSeekY || OriginX != newOriginX)
                {
                    EndSelectionRow = -1; // disable optimization
                    SeekY = MaxSeekY;
                    OriginX = newOriginX;
                    ch = TRUE;
                }
                break;
            }

            case CM_LEFT:
            case CM_FASTLEFT:
            {
                if (OriginX > 0)
                {
                    OriginX -= LOWORD(wParam) == CM_LEFT ? 1 : FAST_LEFTRIGHT;
                    if (OriginX < 0)
                        OriginX = 0;
                    ch = TRUE;
                }
                break;
            }

            case CM_RIGHT:
            case CM_FASTRIGHT:
            {
                __int64 maxOX = GetMaxOriginX();
                if (OriginX < maxOX)
                {
                    OriginX += LOWORD(wParam) == CM_RIGHT ? 1 : FAST_LEFTRIGHT;
                    if (OriginX > maxOX)
                        OriginX = maxOX;
                    ch = TRUE;
                }
                break;
            }

            case CM_EXTSEL_LEFT:
            case CM_EXTSEL_RIGHT:
            case CM_EXTSEL_HOME:
            case CM_EXTSEL_END:
                EndSelectionPrefX = -1; // the break is not missing here!
            case CM_EXTSEL_UP:
            case CM_EXTSEL_DOWN:
            {
                if (StartSelection == EndSelection || MouseDrag || Type != vtText)
                {
                    skipCmd = TRUE;
                    break;
                }

                BOOL viewAlreadyMovedToSel = FALSE;
                int endSelLineIndex = -1;
                while (1)
                {
                    for (int i = 0; i + 3 < LineOffset.Count; i += 3)
                    { // finding the end of the selection in LineOffset without the last line (if it is only partially visible,
                        // it's OK if it's fully visible, we will find the end of the selection in it, see below)
                        // for wrap mode: if the block is forward (pulled from the beginning to the end of the file) and ends at the end
                        // wrap lines, draw towards the end of the line wrap and not towards the beginning of the next line (those two positions
                        // if the block is backward (pulled in the opposite direction), it is drawn for the same offset
                        // from the beginning of the line and not from the end of the previous line = select 'endSelLineIndex' must respect this
                        if ((EndSelection > LineOffset[i] ||
                             // only wrap mode: if the view starts with a continuation of a wrapped line and the forward block ends at an offset
                             // At the beginning of the view, we need the previous line where the drawn block ends ("the cursor" is outside the view)
                             EndSelection == LineOffset[i] &&
                                 (i > 0 || !WrapText || StartSelection > EndSelection || !WrapIsBeforeFirstLine)) &&
                            (EndSelection < LineOffset[i + 3] ||
                             WrapText && StartSelection < EndSelection && EndSelection == LineOffset[i + 1]))
                        {
                            endSelLineIndex = i / 3;
                            break;
                        }
                    }
                    if (endSelLineIndex == -1 && LineOffset.Count >= 3 &&    // before the last line we did not find the end of the selection,
                        LineOffset.Count / 3 <= Height / CharHeight &&       // we are only interested in the fully visible last row,
                        (EndSelection > LineOffset[LineOffset.Count - 3] ||  // End of selection is in the last line of the file (does not
                         EndSelection == LineOffset[LineOffset.Count - 3] && // EOL, otherwise it wouldn't be the last) - we will handle it specially
                             (!WrapText || LineOffset.Count >= 6 ||          // end of forward block at the end of the previous wrapped line
                              StartSelection > EndSelection || !WrapIsBeforeFirstLine)) &&
                        EndSelection <= LineOffset[LineOffset.Count - 2])
                    {
                        endSelLineIndex = LineOffset.Count / 3 - 1;
                    }
                    if (endSelLineIndex == -1 && // the end of the selection is not in a fully visible row, the view needs to be moved first
                        !viewAlreadyMovedToSel)  // Trying it only once, dry run to prevent looping
                    {
                        viewAlreadyMovedToSel = TRUE;
                        // Move the view so that the end of the selection is on the last/first line
                        int lines = EndSelection > SeekY ? Height / CharHeight : 1;
                        if (lines <= 0)
                            break; // if not a single line is visible, we are not working
                        BOOL fatalErr;
                        __int64 newSeekY = FindSeekBefore(EndSelection, lines, fatalErr, NULL, NULL, EndSelection > StartSelection);
                        if (fatalErr)
                            FatalFileErrorOccured();
                        if (fatalErr || ExitTextMode)
                            return 0;
                        SeekY = newSeekY;
                        OriginX = 0;
                        InvalidateRect(HWindow, NULL, FALSE);
                        UpdateWindow(HWindow); // let's have LineOffset recalculated
                        continue;
                    }
                    break;
                }
                if (endSelLineIndex == -1) // something went wrong, unexpectedly we have to end
                {
                    skipCmd = TRUE;
                    break;
                }

                __int64 oldEndSel = EndSelection;
                __int64 curX = -1; // x-coordinate of the end of the block, we need to ensure its visibility (adjust OriginX)
                int minRow = endSelLineIndex;
                int maxRow = endSelLineIndex;

                switch (LOWORD(wParam))
                {
                case CM_EXTSEL_LEFT:
                case CM_EXTSEL_HOME:
                {
                    BOOL scrollUp = FALSE;
                    BOOL moveIsDone = FALSE;
                    if (LOWORD(wParam) == CM_EXTSEL_HOME || EndSelection > LineOffset[3 * endSelLineIndex])
                    {
                        if (LOWORD(wParam) == CM_EXTSEL_HOME)
                            EndSelection = LineOffset[3 * endSelLineIndex]; // jump to the beginning
                        else
                            EndSelection--; // move in a row

                        // wrap mode: specially handle the end of the forward block at the end of the previous wrapped line
                        // (offset is the same as the beginning of this line)
                        if (WrapText && StartSelection < EndSelection &&
                            EndSelection == LineOffset[3 * endSelLineIndex] &&
                            (endSelLineIndex > 0 && EndSelection == LineOffset[3 * (endSelLineIndex - 1) + 1] ||
                             endSelLineIndex == 0 && WrapIsBeforeFirstLine))
                        {
                            if (minRow > 0)
                            {
                                minRow--;
                                if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex - 1))
                                    return 0;
                            }
                            else // row is out of view, view needs to be moved up by one row
                            {
                                scrollUp = TRUE;
                                moveIsDone = TRUE;
                                // to prevent this line from just scrolling, but to redraw itself (change of marking at the beginning)
                                InvalidateRows(minRow, maxRow, FALSE);
                            }
                        }
                        else
                        {
                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex))
                                return 0;
                        }
                    }
                    else // go to the end of the previous line
                    {
                        if (endSelLineIndex > 0)
                        {
                            __int64 newEndSel = LineOffset[3 * (endSelLineIndex - 1) + 1];
                            // there is no offset difference between the beginning and end of the wrapped line, let's create it
                            // artificial (the top row is wrapped = we can definitely go one character to the left on it),
                            // must be a closing brace, otherwise the end of the block would be at the end of the previous line
                            if (WrapText && newEndSel == EndSelection && newEndSel > 0)
                                newEndSel--; // always should be > 0
                            EndSelection = newEndSel;

                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex - 1))
                                return 0;

                            minRow--;
                            maxRow--;
                        }
                        else
                            scrollUp = TRUE; // It is necessary to scroll the view up by one row
                    }
                    if (scrollUp)
                    {
                        BOOL scrolled;
                        __int64 firstLineEndOff;
                        __int64 firstLineCharLen;
                        if (!ScrollViewLineUp(-1, &scrolled, FALSE, &firstLineEndOff, &firstLineCharLen))
                            return 0;
                        if (scrolled) // We have scrolled, not redrawn, the first line is from SeekY to firstLineEndOff
                        {
                            if (!moveIsDone &&
                                firstLineEndOff != -1 &&  // -1 = unknown end of first line offset (risk when changing file)
                                SeekY <= firstLineEndOff) // just for fun
                            {
                                __int64 newEndSel = firstLineEndOff;
                                // there is no offset difference between the beginning and end of the wrapped line, let's create it
                                // artificial (the top row is wrapped = we can definitely go one character to the left on it),
                                // must be a closing brace, otherwise the end of the block would be at the end of the previous line
                                if (WrapText && newEndSel == EndSelection && newEndSel > 0)
                                    newEndSel--; // always should be > 0
                                EndSelection = newEndSel;
                            }

                            if (!GetXFromOffsetInText(&curX, EndSelection, -1, SeekY, firstLineCharLen, firstLineEndOff))
                                return 0;

                            BOOL fullRedraw = FALSE; // Ensure visibility of the new end-of-block position
                            EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, firstLineCharLen);
                            if (fullRedraw)
                                InvalidateRect(HWindow, NULL, FALSE);
                            else
                                ::ScrollWindow(HWindow, 0, CharHeight, NULL, NULL); //scroll the window
                            UpdateWindow(HWindow);
                        }
                        else // the previous line does not exist, we are probably at the beginning of the file
                        {
                            if (moveIsDone)
                                UpdateWindow(HWindow);
                            else
                                skipCmd = TRUE;
                        }
                        updateView = FALSE; // We have redrawn, there is no need to do it again
                    }
                    break;
                }

                case CM_EXTSEL_RIGHT:
                case CM_EXTSEL_END:
                {
                    if (LOWORD(wParam) == CM_EXTSEL_END || EndSelection < LineOffset[3 * endSelLineIndex + 1])
                    {
                        if (LOWORD(wParam) == CM_EXTSEL_END)
                            EndSelection = LineOffset[3 * endSelLineIndex + 1];
                        else
                            EndSelection++; // move in a row

                        // wrap mode: specially handle the end of the wrap block at the beginning of the next wrap line
                        // (offset equals the end of this line)
                        if (WrapText && EndSelection < StartSelection && EndSelection == LineOffset[3 * endSelLineIndex + 1] &&
                            endSelLineIndex + 1 < LineOffset.Count / 3 &&
                            EndSelection == LineOffset[3 * (endSelLineIndex + 1)])
                        {
                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex + 1))
                                return 0; // curX=0 (start of line)
                            if (endSelLineIndex + 1 >= Height / CharHeight)
                            {
                                BOOL fullRedraw = FALSE; // here always FALSE: wrap => OriginX==0 + curX==0
                                EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, -1, TRUE);
                                if (fullRedraw)
                                    InvalidateRect(HWindow, NULL, FALSE);
                                else
                                    InvalidateRows(minRow, maxRow, FALSE);
                                if (!ScrollViewLineDown(fullRedraw))
                                    UpdateWindow(HWindow); // unexpected
                                updateView = FALSE;        // We have redrawn, there is no need to do it again
                            }
                        }
                        else
                        {
                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex))
                                return 0;
                        }
                    }
                    else // we are going to the beginning of the next line
                    {
                        // LineOffset always contains the line below the last fully visible one (even if nothing is visible from it),
                        // but only if the line in the file exists naturally
                        if (endSelLineIndex + 1 < LineOffset.Count / 3)
                        {
                            __int64 newEndSel = LineOffset[3 * (endSelLineIndex + 1)];
                            // there is no offset difference between the end of a wrapped line and the beginning of the next one, let's create it
                            // artificial (wrapping ensures that we can move at least one character to the right on the bottom line),
                            // must be a forward block, otherwise the end of the block would be at the beginning of the next line
                            if (WrapText && newEndSel == EndSelection && newEndSel < LineOffset[3 * (endSelLineIndex + 1) + 1])
                            {
                                newEndSel++; // should always be < LineOffset[3 * (endSelLineIndex + 1) + 1]
                                maxRow++;
                            }
                            EndSelection = newEndSel;

                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex + 1))
                                return 0; // curX=0/1 (start of line)
                            if (endSelLineIndex + 1 == Height / CharHeight)
                            {                            // We have moved to a line that is only partially visible, it is necessary to scroll the view down by one line
                                BOOL fullRedraw = FALSE; // OriginX can be > 0/1 because it may not be a wrap mode, then it is necessary to shift OriginX and do a full-redraw
                                EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, -1, TRUE);
                                if (fullRedraw)
                                    InvalidateRect(HWindow, NULL, FALSE);
                                else
                                    InvalidateRows(minRow, maxRow, FALSE);
                                if (!ScrollViewLineDown(fullRedraw))
                                    UpdateWindow(HWindow); // unexpected
                                updateView = FALSE;        // We have redrawn, there is no need to do it again
                            }
                        }
                        else
                            skipCmd = TRUE; // next line does not exist = we are at the end of the file, further is not possible
                    }
                    break;
                }

                case CM_EXTSEL_UP:
                case CM_EXTSEL_DOWN:
                {
                    if (EndSelectionPrefX == -1) // I do not have a preferred x-coordinate, I initialize it to the coordinate of the end of the block
                    {
                        if (!GetXFromOffsetInText(&EndSelectionPrefX, EndSelection, endSelLineIndex))
                            return 0;
                    }

                    __int64 curOff = -1;
                    if (LOWORD(wParam) == CM_EXTSEL_UP)
                    {
                        if (endSelLineIndex > 0)
                        {
                            if (!GetOffsetFromXInText(&curX, &curOff, EndSelectionPrefX, endSelLineIndex - 1))
                                return 0;
                            EndSelection = curOff;
                            minRow--;
                        }
                        else // It is necessary to scroll the view up by one row
                        {
                            BOOL scrolled;
                            __int64 firstLineEndOff;
                            __int64 firstLineCharLen;
                            if (!ScrollViewLineUp(-1, &scrolled, FALSE, &firstLineEndOff, &firstLineCharLen))
                                return 0;
                            if (scrolled) // We have scrolled, not redrawn, the first line is from SeekY to firstLineEndOff
                            {
                                if (firstLineEndOff != -1 &&  // -1 = unknown end of first line offset (risk when changing file)
                                    firstLineCharLen != -1 && // -1 = unknown length of the first row in characters
                                    SeekY <= firstLineEndOff) // just for fun
                                {
                                    if (!GetOffsetFromXInText(&curX, &curOff, EndSelectionPrefX, -1, SeekY, firstLineCharLen,
                                                              firstLineEndOff))
                                        return 0;
                                    EndSelection = curOff;
                                    // to prevent this line from just scrolling, but to redraw itself (change of marking)
                                    InvalidateRows(minRow, maxRow, FALSE);
                                }

                                BOOL fullRedraw = FALSE; // Ensure visibility of the new end-of-block position
                                if (curX != -1)
                                    EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, firstLineCharLen);
                                if (fullRedraw)
                                    InvalidateRect(HWindow, NULL, FALSE);
                                else
                                    ::ScrollWindow(HWindow, 0, CharHeight, NULL, NULL); //scroll the window
                                UpdateWindow(HWindow);
                                updateView = FALSE; // We have redrawn, there is no need to do it again
                            }
                            else
                                skipCmd = TRUE; // the previous line does not exist, we are probably at the beginning of the file
                        }
                    }
                    else // CM_EXTSEL_DOWN
                    {
                        // LineOffset always contains the line below the last fully visible one (even if nothing is visible from it),
                        // but only if the line in the file exists naturally
                        if (endSelLineIndex + 1 < LineOffset.Count / 3)
                        {
                            if (!GetOffsetFromXInText(&curX, &curOff, EndSelectionPrefX, endSelLineIndex + 1))
                                return 0;
                            EndSelection = curOff;
                            maxRow++;

                            if (endSelLineIndex + 1 == Height / CharHeight)
                            { // We have moved to a line that is only partially visible, it is necessary to scroll the view down by one line
                                BOOL fullRedraw = FALSE;
                                EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, -1, TRUE);
                                if (fullRedraw)
                                    InvalidateRect(HWindow, NULL, FALSE);
                                else
                                    InvalidateRows(minRow, maxRow, FALSE);
                                if (!ScrollViewLineDown(fullRedraw))
                                    UpdateWindow(HWindow); // unexpected
                                updateView = FALSE;        // We have redrawn, there is no need to do it again
                            }
                        }
                        else
                            skipCmd = TRUE; // next line does not exist = we are at the end of the file, further is not possible
                    }
                    break;
                }
                }

                if (skipCmd)
                    break;

                if (updateView)
                {
                    BOOL fullRedraw = FALSE;
                    if (curX != -1)
                        EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw); // Ensure visibility of the new end-of-block position
                    if (fullRedraw)
                    {
                        InvalidateRect(HWindow, NULL, FALSE);
                        UpdateWindow(HWindow);
                    }
                    else
                        InvalidateRows(minRow, maxRow); // calculate the rectangle that needs to be redrawn
                    updateView = FALSE;                 // SeekY does not change, so just invalidate is enough, we will skip redrawing the entire view
                }

                if (oldEndSel != EndSelection)
                    ChangingSelWithShiftKey = TRUE;
                extSelCh = TRUE;
                break;
            }
            }

            if (skipCmd)
                break;

            if (ch || extSelCh)
            {
                if (extSelCh)
                {
                    SelectionIsFindResult = FALSE;
                    LastFindSeekY = -1;
                    LastFindOffset = 0;
                    FindOffset = EndSelection != -1 ? EndSelection : 0;
                }
                else
                    ResetFindOffsetOnNextPaint = TRUE;

                if (updateView)
                {
                    InvalidateRect(HWindow, NULL, FALSE);
                    UpdateWindow(HWindow); // to calculate the ViewSize for the next PageDown
                }
            }
            else
            {
                FindOffset = SeekY;
                if (!FindDialog.Forward)
                    FindOffset += ViewSize;
            }
        }
        break;
    }

    case WM_SETCURSOR:
    {
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(LoadCursor(NULL, IDC_IBEAM));
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDBLCLK:
    {
        BOOL wholeLine = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        __int64 off;
        BOOL fatalErr = FALSE;
        if (GetOffset((short)LOWORD(lParam), (short)HIWORD(lParam), off, fatalErr) && !fatalErr)
        {
            __int64 selStart = 0;
            __int64 seek = off;
            BOOL breakOnCR = FALSE; // to find '\r' from '\r\n' end of line
            while (seek > 0)
            {
                __int64 len = min(APROX_LINE_LEN, seek);
                len = Prepare(NULL, seek - len, len, fatalErr);
                if (fatalErr)
                {
                    FatalFileErrorOccured();
                    return 0;
                }
                if (len == 0)
                    return 0; // error
                unsigned char* s = Buffer + (seek - Seek - 1);
                unsigned char* end = s - len;

                if (!wholeLine) // looking for the beginning of a word
                {
                    while (s > end && (IsCharAlphaNumeric(*s) || *s == '_'))
                        s--;
                }
                else // looking for the beginning of the line
                {
                    if (breakOnCR && s > end && *s == '\r')
                        s++; // to find the end of '\r\n' and not the end of '\r'
                    else
                    {
                        breakOnCR = FALSE;
                        while (s > end)
                        {
                            if (Configuration.EOL_CR && *s == '\r')
                                break; // '\r'
                            if (*s == '\n')
                            {
                                if (Configuration.EOL_LF)
                                    break; // '\n'
                                if (Configuration.EOL_CRLF)
                                {
                                    if (s - 1 > end)
                                    {
                                        if (*(s - 1) == '\r')
                                            break; // '\r\n'
                                    }
                                    else
                                        breakOnCR = TRUE; // in the next pass, we will test if '\r' will be before this '\n'
                                }
                            }
                            if (Configuration.EOL_NULL && *s == 0)
                                break; // '\0'
                            s--;
                        }
                    }
                }
                if (s != end)
                {
                    selStart = seek - len + (s - end);
                    break;
                }
                seek -= len;
            }

            __int64 selEnd = FileSize;
            seek = off;
            BOOL breakOnLF = FALSE; // to find '\n' from '\r\n' end of line
            while (seek < FileSize)
            {
                __int64 len = Prepare(NULL, seek, APROX_LINE_LEN, fatalErr);
                if (fatalErr)
                {
                    FatalFileErrorOccured();
                    return 0;
                }
                if (len == 0)
                    return 0; // error
                unsigned char* s = Buffer + (seek - Seek);
                unsigned char* end = s + len;

                if (!wholeLine) // looking for the end of the word
                {
                    while (s < end && (IsCharAlphaNumeric(*s) || *s == '_'))
                        s++;
                }
                else // looking for end of line
                {
                    if (breakOnLF)
                    {
                        if (s < end && *s == '\n')
                        {
                            s++; // to find the end of '\r\n'
                            selEnd = seek + len - (end - s);
                            break; // end of search
                        }
                        else
                        {
                            if (Configuration.EOL_CR)
                                ; // kdyz uz nevysel '\r\n', bereme aspon '\r' (nedelame nic)
                            else
                                breakOnLF = FALSE; // neither '\r\n' nor '\r' was read, continue reading...
                        }
                    }

                    if (!breakOnLF)
                    {
                        BOOL eol = FALSE;
                        while (s < end)
                        {
                            if (Configuration.EOL_LF && *s == '\n')
                            {
                                s++;        // to find the end of '\n'
                                eol = TRUE; // end search
                                break;      // '\n'
                            }
                            if (*s == '\r')
                            {
                                BOOL testCR = TRUE;
                                if (Configuration.EOL_CRLF)
                                {
                                    if (s + 1 < end)
                                    {
                                        if (*(s + 1) == '\n')
                                        {
                                            s += 2;     // to find the end of '\r\n'
                                            eol = TRUE; // end search
                                            break;      // '\r\n'
                                        }
                                    }
                                    else
                                    {
                                        breakOnLF = TRUE; // in the next pass, we will test if '\n' will be after this '\r'
                                        testCR = FALSE;
                                    }
                                }
                                if (testCR)
                                {
                                    if (Configuration.EOL_CR)
                                    {
                                        s++;        // to find the end of '\r'
                                        eol = TRUE; // end search
                                        break;      // '\r'
                                    }
                                }
                            }
                            if (Configuration.EOL_NULL && *s == 0)
                            {
                                s++;        // aby naslo konec '\0'
                                eol = TRUE; // end search
                                break;      // '\0'
                            }
                            s++;
                        }
                        if (eol)
                        {
                            selEnd = seek + len - (end - s);
                            break; // end of search
                        }
                    }
                }
                if (s != end)
                {
                    selEnd = seek + len - (end - s);
                    break; // end of search
                }
                seek += len;
            }

            StartSelection = selStart;
            EndSelection = selEnd;
            SelectionIsFindResult = FALSE;
            InvalidateRect(HWindow, NULL, FALSE);

            if (Configuration.AutoCopySelection && StartSelection != EndSelection)
                PostMessage(HWindow, WM_COMMAND, CM_COPYTOCLIP, 0);
        }
        else
        {
            if (fatalErr)
                FatalFileErrorOccured();
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        ResetMouseWheelAccumulator();
        SetToolTipOffset(-1);
        BOOL shiftPressed = (wParam & MK_SHIFT) != 0;
        __int64 off;
        BOOL fatalErr = FALSE;
        BOOL onHexNum;
        if (shiftPressed ||
            GetOffset((short)LOWORD(lParam), (short)HIWORD(lParam), off, fatalErr, TRUE, &onHexNum) &&
                !fatalErr)
        {
            if (!shiftPressed &&
                StartSelection != EndSelection &&                 // there is some block
                (off >= StartSelection || off >= EndSelection) && // [x,y] inside the block
                (off < StartSelection || off < EndSelection) &&
                (Type != vtHex || // Start of block in hex mode only if [x, y] is a hex digit
                 (off != StartSelection && off != EndSelection) || onHexNum))
            {
                BOOL msgBoxDisplayed;
                if (CheckSelectionIsNotTooBig(HWindow, &msgBoxDisplayed) &&
                    !msgBoxDisplayed) // after displaying the query, I can no longer start D&D (the user probably doesn't even hold the left mouse button)
                {
                    POINT p1;
                    GetCursorPos(&p1);

                    HGLOBAL h = GetSelectedText(fatalErr);
                    if (!fatalErr && h != NULL)
                    {
                        CImpIDropSource* dropSource = new CImpIDropSource(FALSE);
                        IDataObject* dataObject = new CTextDataObject(h);
                        if (dataObject != NULL && dropSource != NULL)
                        {
                            DWORD dwEffect;
                            DoDragDrop(dataObject, dropSource, DROPEFFECT_COPY, &dwEffect);
                        }
                        if (dataObject != NULL)
                            dataObject->Release();
                        if (dropSource != NULL)
                            dropSource->Release();
                    }
                    if (fatalErr)
                    {
                        if (h != NULL)
                            NOHANDLES(GlobalFree(h));
                        FatalFileErrorOccured();
                        break;
                    }

                    POINT p2;
                    GetCursorPos(&p2);
                    if (abs(p1.x - p2.x) < 2 && abs(p1.y - p2.y) < 2)
                    {
                        LastFindSeekY = -1;
                        FindOffset = off;

                        EndSelection = StartSelection;
                        InvalidateRect(HWindow, NULL, FALSE);
                    }
                }
            }
            else
            {
                if (shiftPressed && StartSelection != -1 ||
                    GetOffset((short)LOWORD(lParam), (short)HIWORD(lParam), off, fatalErr) && !fatalErr)
                { // we need to determine the start/end position of the pull - leftMost must be FALSE
                    SetCapture(HWindow);
                    MouseDrag = TRUE;
                    SelectionIsFindResult = FALSE;
                    ChangingSelWithShiftKey = FALSE;
                    if (shiftPressed && StartSelection != -1) // change end of block (Shift+click)
                    {
                        EndSelectionRow = -1; // is not valid yet and we will not look for where the current block ends
                        PostMouseMove();
                    }
                    else
                    {
                        StartSelection = off;
                        EndSelection = off;
                        EndSelectionRow = (short)HIWORD(lParam) / CharHeight;
                        InvalidateRect(HWindow, NULL, FALSE);
                    }
                }
                else
                {
                    if (fatalErr)
                        FatalFileErrorOccured();
                }
            }
        }
        else
        {
            if (fatalErr)
                FatalFileErrorOccured();
        }
        break;
    }

    case WM_RBUTTONDOWN:
    {
        ResetMouseWheelAccumulator();
        SetToolTipOffset(-1);
        HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_VIEWERCONTEXTMENU));
        if (main == NULL)
            TRACE_E("Unable to load context menu for viewer.");
        else
        {
            ReleaseMouseDrag();
            HMENU subMenu = GetSubMenu(main, 0);
            if (subMenu != NULL)
            {
                BOOL enable = (FileName != NULL && StartSelection != EndSelection);
                EnableMenuItem(subMenu, CM_COPYTOCLIP, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
                EnableMenuItem(subMenu, CM_COPYTOFILE, MF_BYCOMMAND | (FileName != NULL ? MF_ENABLED : MF_GRAYED));
                CheckMenuRadioItem(subMenu, CM_TO_HEX, CM_TO_TEXT,
                                   (Type == vtHex) ? CM_TO_HEX : CM_TO_TEXT, MF_BYCOMMAND);
                CheckMenuItem(subMenu, CM_WRAPED, MF_BYCOMMAND | (WrapText ? MF_CHECKED : MF_UNCHECKED));
                EnableMenuItem(subMenu, CM_GOTOOFFSET, MF_BYCOMMAND | (FileName != NULL ? MF_ENABLED : MF_GRAYED));
                EnableMenuItem(subMenu, CM_WRAPED, MF_BYCOMMAND | ((Type == vtText) ? MF_ENABLED : MF_GRAYED));

                POINT p;
                GetCursorPos(&p);
                DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                             p.x, p.y, HWindow, NULL);
                if (cmd != 0)
                    PostMessage(HWindow, WM_COMMAND, cmd, 0);
            }
            DestroyMenu(main);
        }
        break;
    }

    case WM_CANCELMODE:
    case WM_LBUTTONUP:
    {
        SetToolTipOffset(-1);
        if (MouseDrag)
        {
            ReleaseMouseDrag();

            LastFindSeekY = -1;
            FindOffset = EndSelection != -1 ? EndSelection : 0;

            if (Configuration.AutoCopySelection && StartSelection != EndSelection)
                PostMessage(HWindow, WM_COMMAND, CM_COPYTOCLIP, 0);
        }
        break;
    }

    case WM_TIMER:
    {
        if (wParam == IDT_THUMBSCROLL)
        {
            OnVScroll();
            return 0;
        }

        if (wParam != IDT_AUTOSCROLL)
            break;
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(HWindow, &p);
        lParam = MAKELONG(p.x, p.y);
    }
    case WM_MOUSEMOVE:
    {
        if (MouseDrag)
        {
            __int64 off;
            __int64 x = (short)LOWORD(lParam);
            __int64 y = (short)HIWORD(lParam);
            BOOL wait = FALSE;
            if (y < 0)
            {
                y = 0;
                wait = TRUE;
                SendMessage(HWindow, WM_COMMAND, CM_LINEUP, 0);
            }
            if (y > (Height / CharHeight) * CharHeight)
            {
                y = (Height / CharHeight) * CharHeight - 1;
                if (y < 0)
                    y = 0;
                wait = TRUE;
                SendMessage(HWindow, WM_COMMAND, CM_LINEDOWN, 0);
            }
            if (Type == vtText && SeekY == 0 && y / CharHeight >= LineOffset.Count / 3)
            {
                y = (LineOffset.Count / 3) * CharHeight - 1;
                if (y < 0)
                    y = 0;
            }
            if (Type == vtHex && SeekY == 0 && y / CharHeight >= (FileSize - 1) / 16 + 1)
            {
                y = ((FileSize - 1) / 16 + 1) * CharHeight - 1;
                if (y < 0)
                    y = 0;
            }
            // jr: drive was the condition if (x < 0) { x = 0; ...}, but people reported an error to us that if the window
            // viewer maximized, unable to scroll left; because we have an empty strip on the left (text is not
            // completely glued), we can afford to roll for x < BORDER_WIDTH
            if (x < BORDER_WIDTH)
            {
                x = BORDER_WIDTH;
                wait = TRUE;
                SendMessage(HWindow, WM_COMMAND, CM_LEFT, 0);
            }
            if (x > Width)
            {
                x = Width - 1;
                wait = TRUE;
                SendMessage(HWindow, WM_COMMAND, CM_RIGHT, 0);
            }
            BOOL fatalErr = FALSE;
            if (GetOffset(x, y, off, fatalErr) && !fatalErr)
            {
                if (EndSelection != off)
                {
                    // Optimization introduced: detecting the changed area when pulling the block
                    // and we will only redraw this rectangle
                    BOOL optimalize = TRUE; // Will we only redraw selected lines?
                    RECT r;
                    int endSelectionRow = 0;
                    endSelectionRow = (int)(min(Height, y) / CharHeight);
                    int minRow = min(endSelectionRow, EndSelectionRow);
                    int maxRow = max(endSelectionRow, EndSelectionRow);
                    // In wrap mode, there is no offset difference between the beginning and end of wrapped lines, so the block
                    // Ending at the beginning of a line after line wrapping is drawn so that it ends at the end of the previous line
                    // lines (missing black-end on the right), when stretching the end of the block further, we must redraw
                    // previous line to be redrawn by the black-end (redraws the previous line unnecessarily)
                    // outside the described situation, we are not worried about it, precise detection of the situation is unnecessarily complicated)
                    if (WrapText && StartSelection < EndSelection && off > EndSelection && minRow > 0)
                        minRow--;
                    // when shortening the block after entering the left edge of the view (beginning of the line after wrapping the line)
                    // redrew the end of the previous line (the black-end was deleted)
                    if (WrapText && StartSelection < EndSelection && off < EndSelection &&
                        (x - BORDER_WIDTH + CharWidth / 2) / CharWidth <= Configuration.TabSize / 2 && minRow > 0)
                    {
                        minRow--;
                    }
                    // calculate the rectangle that needs to be redrawn
                    r.left = 0;
                    r.top = minRow * CharHeight;
                    r.right = Width;
                    r.bottom = maxRow * CharHeight + CharHeight;
                    if (EndSelectionRow == -1)
                        optimalize = FALSE;
                    EndSelectionRow = endSelectionRow;

                    EndSelection = off;
                    // SelectionIsFindResult = FALSE;  // unnecessary, it is set at the beginning of block dragging (and continuation via Shift+click)
                    InvalidateRect(HWindow, optimalize ? &r : NULL, FALSE);
                }
            }
            if (!fatalErr && wait)
                SetTimer(HWindow, IDT_AUTOSCROLL, 20, NULL);
            else
            {
                KillTimer(HWindow, IDT_AUTOSCROLL);
                if (fatalErr)
                    FatalFileErrorOccured();
            }
        }
        else
        {
            if (Type == vtHex)
            {
                __int64 offset = -1;
                int x = (short)LOWORD(lParam);
                int y = (short)HIWORD(lParam);
                if (x >= 0 && y >= 0 && x < Width && y < Height)
                {
                    x = (int)((x - BORDER_WIDTH) / CharWidth + OriginX);
                    y = y / CharHeight;
                    if (x > 9 - 8 + HexOffsetLength && x < 61 - 8 + HexOffsetLength)
                    {
                        x -= 9 - 8 + HexOffsetLength;
                        int col = x / 13;
                        int subCol = (x % 13);
                        if ((subCol % 3) >= 1)
                        {
                            col = col * 4 + subCol / 3;
                            offset = SeekY + y * 16 + col;
                        }
                    }
                }
                if (offset != -1 && offset < FileSize)
                    SetToolTipOffset(offset);
                else
                    SetToolTipOffset(-1);
            }
        }
        break;
    }

    case WM_MOUSEHWHEEL:
    {
        // note: also called from WM_USER_MOUSEWHEEL when Shift is held
        short zDelta = (short)HIWORD(wParam);
        if ((zDelta < 0 && MouseHWheelAccumulator > 0) || (zDelta > 0 && MouseHWheelAccumulator < 0))
            ResetMouseWheelAccumulator(); // When changing the direction of the wheel tilt, it is necessary to reset the accumulator

        DWORD wheelScroll = GetMouseWheelScrollChars();
        DWORD pageWidth = max(1, (DWORD)(Width - BORDER_WIDTH) / CharWidth);
        wheelScroll = max(1, min(wheelScroll, pageWidth - 1)); // limit the maximum page length

        MouseHWheelAccumulator += 1000 * zDelta;
        int stepsPerChar = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
        int charsToScroll = MouseHWheelAccumulator / stepsPerChar;
        if (charsToScroll != 0)
        {
            MouseHWheelAccumulator -= charsToScroll * stepsPerChar;
            if (abs(charsToScroll) < abs((int)pageWidth - 1))
            {
                int i;
                for (i = 0; i < abs(charsToScroll); i++)
                    SendMessage(HWindow, WM_HSCROLL, zDelta < 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            }
            else
                SendMessage(HWindow, WM_HSCROLL, zDelta < 0 ? SB_PAGEUP : SB_PAGEDOWN, 0);
        }
        return TRUE;
    }

    case WM_NOTIFY:
    {
        if (((LPNMHDR)lParam)->code == TTN_NEEDTEXT)
        {
            if (ToolTipOffset != -1)
            {
                LPTOOLTIPTEXT ptr = (LPTOOLTIPTEXT)lParam;
                char number[100];
                int dummy;
                PrintHexOffset(number, ToolTipOffset, GetHexOffsetMode(FileSize, dummy));
                strcat_s(number, " (");
                NumberToStr(number + strlen(number), CQuadWord().SetUI64(ToolTipOffset));
                strcat_s(number, ")");
                sprintf(ptr->szText, LoadStr(IDS_VIEWEROFFSETTIP), number);
            }
            else
                ((LPTOOLTIPTEXT)lParam)->szText[0] = 0;
            return 0;
        }
        break;
    }

    case WM_INITMENU:
    {
        HMENU main = GetMenu(HWindow);
        if (main == NULL)
            TRACE_E("Main window of viewer has no menu?");
        else
        {
            HMENU subMenu = GetSubMenu(main, VIEWER_FILE_MENU_INDEX);
            if (subMenu != NULL)
            {
                HMENU othFilesMenu = GetSubMenu(subMenu, VIEWER_FILE_MENU_OTHFILESINDEX);
                if (othFilesMenu != NULL)
                {
                    BOOL prevFile = FALSE;
                    BOOL nextFile = FALSE;
                    BOOL prevSelFile = FALSE;
                    BOOL nextSelFile = FALSE;
                    BOOL firstLastFile = FALSE;

                    BOOL ok = FALSE;
                    BOOL srcBusy = FALSE;
                    BOOL noMoreFiles = FALSE;
                    char fileName[MAX_PATH];
                    fileName[0] = 0;
                    int enumFileNamesLastFileIndex = EnumFileNamesLastFileIndex;
                    ok = GetPreviousFileNameForViewer(EnumFileNamesSourceUID,
                                                      &enumFileNamesLastFileIndex,
                                                      FileName, FALSE, TRUE,
                                                      fileName, &noMoreFiles,
                                                      &srcBusy, NULL);

                    prevFile = ok || srcBusy;                     // only if there is a previous file (or if Salam is busy, the user has no choice but to try again later)
                    firstLastFile = ok || srcBusy || noMoreFiles; // Jumping to the first or last file is only possible if the connection with the source is not interrupted (or if Salam is busy, the only option is to ask the user to try again later)
                    if (firstLastFile)
                    {
                        enumFileNamesLastFileIndex = EnumFileNamesLastFileIndex;
                        ok = GetPreviousFileNameForViewer(EnumFileNamesSourceUID,
                                                          &enumFileNamesLastFileIndex,
                                                          FileName, TRUE /* prefer selected*/, TRUE,
                                                          fileName, &noMoreFiles,
                                                          &srcBusy, NULL);
                        BOOL isSrcFileSel = FALSE;
                        if (ok)
                        {
                            ok = IsFileNameForViewerSelected(EnumFileNamesSourceUID, enumFileNamesLastFileIndex,
                                                             fileName, &isSrcFileSel, &srcBusy);
                            prevSelFile = ok && isSrcFileSel || srcBusy; // only if the previous file is really selected (or if Salam is busy, there is nothing left but to ask the user to try again later)
                        }

                        enumFileNamesLastFileIndex = EnumFileNamesLastFileIndex;
                        ok = GetNextFileNameForViewer(EnumFileNamesSourceUID,
                                                      &enumFileNamesLastFileIndex,
                                                      FileName, FALSE, TRUE,
                                                      fileName, &noMoreFiles,
                                                      &srcBusy, NULL);
                        nextFile = ok || srcBusy; // only if there is another file (or if Salam is busy, the only option is to ask the user to try again later)

                        enumFileNamesLastFileIndex = EnumFileNamesLastFileIndex;
                        ok = GetNextFileNameForViewer(EnumFileNamesSourceUID,
                                                      &enumFileNamesLastFileIndex,
                                                      FileName, TRUE /* prefer selected*/, TRUE,
                                                      fileName, &noMoreFiles,
                                                      &srcBusy, NULL);
                        isSrcFileSel = FALSE;
                        if (ok)
                        {
                            ok = IsFileNameForViewerSelected(EnumFileNamesSourceUID, enumFileNamesLastFileIndex,
                                                             fileName, &isSrcFileSel, &srcBusy);
                            nextSelFile = ok && isSrcFileSel || srcBusy; // only if the next file is really selected (or if Salam is busy, there is nothing left but to let the user try again later)
                        }
                    }

                    EnableMenuItem(othFilesMenu, CM_PREVFILE, MF_BYCOMMAND | (prevFile ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(othFilesMenu, CM_NEXTFILE, MF_BYCOMMAND | (nextFile ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(othFilesMenu, CM_PREVSELFILE, MF_BYCOMMAND | (prevSelFile ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(othFilesMenu, CM_NEXTSELFILE, MF_BYCOMMAND | (nextSelFile ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(othFilesMenu, CM_FIRSTFILE, MF_BYCOMMAND | (firstLastFile ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(othFilesMenu, CM_LASTFILE, MF_BYCOMMAND | (firstLastFile ? MF_ENABLED : MF_GRAYED));
                }
            }
            subMenu = GetSubMenu(main, VIEW_MENU_INDEX);
            if (subMenu != NULL)
            {
                CheckMenuItem(subMenu, CM_VIEW_AUTOSEL, MF_BYCOMMAND | (DefViewMode == 0 ? MF_CHECKED : MF_UNCHECKED));
                int uItem = -1;
                if (DefViewMode == 1)
                    uItem = CM_TO_TEXT;
                else if (DefViewMode == 2)
                    uItem = CM_TO_HEX;
                SetMenuDefaultItem(subMenu, uItem, FALSE);
                CheckMenuRadioItem(subMenu, CM_TO_HEX, CM_TO_TEXT,
                                   (Type == vtHex) ? CM_TO_HEX : CM_TO_TEXT, MF_BYCOMMAND);
                CheckMenuItem(subMenu, CM_WRAPED, MF_BYCOMMAND | (WrapText ? MF_CHECKED : MF_UNCHECKED));
                EnableMenuItem(subMenu, CM_WRAPED, MF_BYCOMMAND | ((Type == vtText) ? MF_ENABLED : MF_GRAYED));
                BOOL zoomed = IsZoomed(HWindow);
                CheckMenuItem(subMenu, CM_VIEW_FULLSCREEN, MF_BYCOMMAND | (zoomed ? MF_CHECKED : MF_UNCHECKED));
                EnableMenuItem(subMenu, CM_GOTOOFFSET, MF_BYCOMMAND | (FileName != NULL ? MF_ENABLED : MF_GRAYED));
            }
            subMenu = GetSubMenu(main, VIEWER_EDIT_MENU_INDEX);
            if (subMenu != NULL)
            {
                BOOL enable = (FileName != NULL && StartSelection != EndSelection);
                EnableMenuItem(subMenu, CM_COPYTOCLIP, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
                EnableMenuItem(subMenu, CM_COPYTOFILE, MF_BYCOMMAND | (FileName != NULL ? MF_ENABLED : MF_GRAYED));
            }
            subMenu = GetSubMenu(main, OPTIONS_MENU_INDEX);
            if (subMenu != NULL)
            {
                BOOL enable = FALSE;
                enable = !SalamanderBusy;
                EnableMenuItem(subMenu, CM_VIEWER_CONFIG, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
            }
            subMenu = GetSubMenu(main, CODING_MENU_INDEX);
            if (subMenu != NULL)
            {
                BOOL firstTime = GetMenuItemCount(subMenu) == 0;
                // the menu will be filled at the first call and at all subsequent calls
                // the radio will be set to CodeType
                CodeTables.InitMenu(subMenu, CodeType);

                if (CodePageAutoSelect)
                {
                    // When auto-select is on, none of the items are default
                    SetMenuDefaultItem(subMenu, -1, FALSE);
                }
                else
                {
                    // if auto-select is not enabled, some item must be set as default
                    int defCodeType;
                    CodeTables.GetCodeType(DefaultConvert, defCodeType);
                    SetMenuDefaultItem(subMenu, CM_CODING_MIN + defCodeType, FALSE);
                }

                if (firstTime)
                {
                    // we will connect our commands
                    int count = GetMenuItemCount(subMenu);

                    MENUITEMINFO mi;
                    memset(&mi, 0, sizeof(mi));
                    mi.cbSize = sizeof(mi);

                    /* used for the export_mnu.py script, which generates salmenu.mnu for the Translator
   to keep synchronized with the InsertMenuItem() calls below...
MENU_TEMPLATE_ITEM ViewerCodingMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_VIEWERAUTOCODING
  {MNTT_IT, IDS_VIEWERSETDEFAULTCODING
  {MNTT_IT, IDS_VIEWERNEXTCODING
  {MNTT_IT, IDS_VIEWERPREVIOUSCODING
  {MNTT_PE, 0
};*/

                    // Recognize a separator at the very top
                    mi.fMask = MIIM_TYPE | MIIM_ID;
                    mi.fType = MFT_STRING;
                    mi.wID = CM_RECOGNIZE_CODEPAGE;
                    mi.dwTypeData = LoadStr(IDS_VIEWERAUTOCODING);
                    InsertMenuItem(subMenu, 0, TRUE, &mi);
                    count++;

                    mi.fMask = MIIM_TYPE;
                    mi.fType = MFT_SEPARATOR;
                    InsertMenuItem(subMenu, 1, TRUE, &mi);
                    count++;

                    // down separator
                    InsertMenuItem(subMenu, count++, TRUE, &mi);

                    // and commands
                    mi.fMask = MIIM_TYPE | MIIM_ID;
                    mi.fType = MFT_STRING;

                    mi.wID = CM_SETDEFAULT_CODING;
                    mi.dwTypeData = LoadStr(IDS_VIEWERSETDEFAULTCODING);
                    InsertMenuItem(subMenu, count++, TRUE, &mi);

                    mi.wID = CM_NEXTCODING;
                    mi.dwTypeData = LoadStr(IDS_VIEWERNEXTCODING);
                    InsertMenuItem(subMenu, count++, TRUE, &mi);

                    mi.wID = CM_PREVCODING;
                    mi.dwTypeData = LoadStr(IDS_VIEWERPREVIOUSCODING);
                    InsertMenuItem(subMenu, count++, TRUE, &mi);
                }

                CheckMenuItem(subMenu, CM_RECOGNIZE_CODEPAGE, MF_BYCOMMAND | (CodePageAutoSelect ? MF_CHECKED : MF_UNCHECKED));
            }
            subMenu = GetSubMenu(main, OPTIONS_MENU_INDEX);
            if (subMenu != NULL)
            {
                CheckMenuItem(subMenu, CM_VIEWER_AUTOCOPY, MF_BYCOMMAND | (Configuration.AutoCopySelection ? MF_CHECKED : MF_UNCHECKED));
            }
        }
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        UINT cmd = 0;
        switch (wParam)
        {
        case VK_SHIFT:
            ChangingSelWithShiftKey = FALSE;
            break; // Pressing the second Shift during changing the selection via Shift+arrows/Home/End is unexpected, when it happens, it disrupts our plans (the selection is not copied to the clipboard), but I don't care about that...

        case VK_UP:
        {
            if (!ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_LINEUP;
            if (!ctrlPressed && shiftPressed && !altPressed)
                cmd = CM_EXTSEL_UP;
            break;
        }

        case VK_DOWN:
        {
            if (!ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_LINEDOWN;
            if (!ctrlPressed && shiftPressed && !altPressed)
                cmd = CM_EXTSEL_DOWN;
            break;
        }

        case VK_LEFT:
        {
            if (!ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_LEFT;
            if (ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_FASTLEFT;
            if (!ctrlPressed && shiftPressed && !altPressed)
                cmd = CM_EXTSEL_LEFT;
            break;
        }

        case VK_RIGHT:
        {
            if (!ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_RIGHT;
            if (ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_FASTRIGHT;
            if (!ctrlPressed && shiftPressed && !altPressed)
                cmd = CM_EXTSEL_RIGHT;
            break;
        }

        case VK_NEXT:
        {
            if (!ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_PAGEDOWN;
            if (ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_FILEEND;
            break;
        }

        case VK_PRIOR:
        {
            if (!ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_PAGEUP;
            if (ctrlPressed && !shiftPressed && !altPressed)
                cmd = CM_FILEBEGIN;
            break;
        }

        case VK_HOME:
        {
            if (!shiftPressed && !altPressed)
                cmd = CM_FILEBEGIN;
            if (!ctrlPressed && shiftPressed && !altPressed)
                cmd = CM_EXTSEL_HOME;
            if (ctrlPressed && shiftPressed && !altPressed)
                cmd = CM_EXTSEL_FILEBEG;
            break;
        }

        case VK_END:
        {
            if (!shiftPressed && !altPressed)
                cmd = CM_FILEEND;
            if (!ctrlPressed && shiftPressed && !altPressed)
                cmd = CM_EXTSEL_END;
            if (ctrlPressed && shiftPressed && !altPressed)
                cmd = CM_EXTSEL_FILEEND;
            break;
        }

        case VK_BACK:
        {
            int cm = 0;
            if (!ctrlPressed && !altPressed && !shiftPressed)
                cm = CM_PREVFILE;
            else
            {
                if (ctrlPressed && !altPressed && !shiftPressed)
                    cm = CM_PREVSELFILE;
                else
                {
                    if (!ctrlPressed && !altPressed && shiftPressed)
                        cm = CM_FIRSTFILE;
                }
            }
            if (cm != 0)
            {
                PostMessage(HWindow, WM_COMMAND, cm, 0);
                return 0;
            }
            break;
        }

        case VK_SPACE:
        {
            int cm = 0;
            if (!ctrlPressed && !altPressed && !shiftPressed)
                cm = CM_NEXTFILE;
            else
            {
                if (ctrlPressed && !altPressed && !shiftPressed)
                    cm = CM_NEXTSELFILE;
                else
                {
                    if (!ctrlPressed && !altPressed && shiftPressed)
                        cm = CM_LASTFILE;
                }
            }
            if (cm != 0)
            {
                PostMessage(HWindow, WM_COMMAND, cm, 0);
                return 0;
            }
            break;
        }
        }
        if (cmd != 0)
        {
            SendMessage(HWindow, WM_COMMAND, cmd, 0);
            if (MouseDrag)
                PostMouseMove();
            return 0;
        }

        if (ctrlPressed && !shiftPressed && !altPressed)
        {
            LPARAM cm;
            switch (wParam)
            {
            case 'A':
                cm = CM_SELECTALLTEXT;
                break;
            case 'C':
                cm = CM_COPYTOCLIP;
                break;
            case 'F':
                cm = CM_FINDSET;
                break;
            case 'G':
                cm = CM_GOTOOFFSET;
                break;
            case 'L':
            case 'N':
                cm = CM_FINDNEXT;
                break;
            case 'O':
                cm = CM_OPENFILE;
                break;
            case 'P':
                cm = CM_FINDPREV;
                break;
            case 'H':
                cm = CM_TO_HEX;
                break;
            case 'T':
                cm = CM_TO_TEXT;
                break;
            case 'W':
                cm = CM_WRAPED;
                break;
            case 'R':
                cm = CM_REREADFILE;
                break;
            case 'S':
                cm = CM_COPYTOFILE;
                break;
            default:
                cm = 0;
            }
            if (cm != 0)
            {
                PostMessage(HWindow, WM_COMMAND, cm, 0);
                return 0;
            }
        }
        break;
    }

    case WM_KEYUP:
    {
        if (wParam == VK_SHIFT && ChangingSelWithShiftKey)
        {
            ChangingSelWithShiftKey = FALSE;
            if (Configuration.AutoCopySelection && StartSelection != EndSelection)
                PostMessage(HWindow, WM_COMMAND, CM_COPYTOCLIP, 0);
        }
        break;
    }

    case WM_DESTROY:
    {
        DragAcceptFiles(HWindow, FALSE);
        if (HToolTip != NULL)
        {
            DestroyWindow(HToolTip);
            HToolTip = NULL;
        }

        if (Configuration.SavePosition)
        {
            Configuration.WindowPlacement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(HWindow, &Configuration.WindowPlacement);
        }
        Configuration.DefViewMode = DefViewMode;
        GlobalFindDialog = FindDialog;
        if (Configuration.WrapText != WrapText ||
            Configuration.CodePageAutoSelect != CodePageAutoSelect ||
            strcmp(Configuration.DefaultConvert, DefaultConvert) != 0)
        {
            Configuration.WrapText = WrapText;
            Configuration.CodePageAutoSelect = CodePageAutoSelect;
            strcpy(Configuration.DefaultConvert, DefaultConvert);
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_DISPACHCFGCHANGE, 0, 0);
        }
        SetMenu(HWindow, NULL);
        ViewerWindowQueue.Remove(HWindow);
        PostQuitMessage(0);
        return 0;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}
