// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CTextFileViewWindowBase::WindowProc
//

LRESULT
CTextFileViewWindowBase::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CTextFileViewWindowBase::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                             wParam, lParam);

    // dame prilezitost k resetu akumulatoru pro mouse wheel
    ResetMouseWheelAccumulatorHandler(uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_SETFOCUS:
    {
        if (Siblink)
        {
            // zrusime vyber textu v druhem okne
            if (DisplayCaret)
                Siblink->RemoveSelection(TRUE);
        }
        ShowCaret();

        // informujeme parenta, ze jsme prevzali fokus
        //      CMainWindow * parent = (CMainWindow *) WindowsManager.GetWindowPtr(GetParent(HWindow));
        if (MainWindow /*parent*/)
            MainWindow /*parent*/->SetActiveFileView(ID);

        return 0;
    }

    case WM_KILLFOCUS:
    {
        if (DisplayCaret)
            DestroyCaret();
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (GetFocus() != HWindow)
            SetFocus(HWindow);
        // zrusime vyber textu v druhem okne
        if (!DisplayCaret)
            Siblink->RemoveSelection(TRUE);
        if (DataValid)
        {
            int line = FirstVisibleLine + HIWORD(lParam) / FontHeight;
            if (line < int(Script[ViewMode].size()))
            {
                // pri nasledujicim WM_LBUTTONUP fokusujeme differenci
                // tento flag se shodi v UpdateSelection, kdyz user vybere nejaky text
                SelectDifferenceOnButtonUp = TRUE;

                TrackingLineBegin = line;
                if (LOWORD(lParam) >= LineNumWidth)
                {
                    // nastavime novy vyber textu
                    if (Script[ViewMode][line].IsBlank())
                        TrackingCharBegin = 0;
                    else
                        TrackingCharBegin = TransformXOrg(LOWORD(lParam), line);

                    //            CMainWindow * parent = (CMainWindow *) WindowsManager.GetWindowPtr(GetParent(HWindow));
                    if (MainWindow /*parent*/)
                        MainWindow /*parent*/->UpdateToolbarButtons(UTB_TEXTSELECTION);

                    Tracking = TRUE;

                    UpdateSelection(LOWORD(lParam), HIWORD(lParam));

                    SetCapture(HWindow);
                }
            }
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        if (DataValid)
        {
            int line = FirstVisibleLine + HIWORD(lParam) / FontHeight;
            if (line < int(Script[ViewMode].size()))
            {
                int start, end;
                BOOL wholeLines = TRUE;
                //          CMainWindow * parent = (CMainWindow *) WindowsManager.GetWindowPtr(GetParent(HWindow));
                if (LOWORD(lParam) < LineNumWidth)
                {
                    // zkusime vybrat celou differenci do bloku
                    if (!MainWindow /*parent*/ || !MainWindow /*parent*/->GetDifferenceRange(line, &start, &end))
                        return 0;
                }
                else
                {
                    if ((GetKeyState(VK_CONTROL) & 0x8000) == 0)
                        wholeLines = FALSE; // vyber celeho slova
                    else
                        start = end = line; //vyber celeho radku
                }

                // ulozime stary vyber textu
                int oldLineBegin = SelectionLineBegin;
                int oldLineEnd = SelectionLineEnd;
                int oldCharBegin = SelectionCharBegin;
                int oldCharEnd = SelectionCharEnd;

                // nastavime novy vyber textu
                if (wholeLines)
                {
                    //vybirame cele radky
                    SelectionLineBegin = start;
                    SelectionCharBegin = 0;
                    if (end + 1 < int(Script[ViewMode].size()))
                    {
                        SelectionLineEnd = end + 1;
                        SelectionCharEnd = 0;
                    }
                    else
                    {
                        SelectionLineEnd = end;
                        if (Script[ViewMode][end].IsBlank())
                            SelectionCharEnd = 0;
                        else
                        {
                            int l = Script[ViewMode][end].GetLine();
                            SelectionCharEnd = LineLength(l);
                        }
                    }
                }
                else
                {
                    //vyber slova
                    if (Script[ViewMode][line].IsBlank())
                        return 0; // neni co vybirat

                    int l = Script[ViewMode][line].GetLine();
                    start = end = TransformXOrg(LOWORD(lParam), line);
                    SelectWord(l, start, end);

                    if (start == end)
                        return 0; // neni co vybirat

                    SelectionLineBegin = SelectionLineEnd = line;
                    SelectionCharBegin = start;
                    SelectionCharEnd = end;
                }
                SetCaretPos(SelectionCharEnd, SelectionLineEnd);
                RepaintSelection(oldLineBegin, oldLineEnd, oldCharBegin, oldCharEnd);
                if (MainWindow /*parent*/)
                    MainWindow /*parent*/->UpdateToolbarButtons(UTB_TEXTSELECTION);
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

        if (DataValid && TrackingLineBegin < int(Script[ViewMode].size()) && SelectDifferenceOnButtonUp)
        {
            // zkusime vybrat differenci
            //        CMainWindow * parent = (CMainWindow *) WindowsManager.GetWindowPtr(GetParent(HWindow));
            if (MainWindow /*parent*/)
                MainWindow /*parent*/->SelectDifferenceByLine(TrackingLineBegin, FALSE);
            SelectDifferenceOnButtonUp = FALSE;
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

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        if (wParam == VK_CONTROL)
        {
            if ((lParam & 0x40000000) == 0)
            {
                UpdateSelection();
                return 0;
            }
            break;
        }

        if (DisplayCaret)
        {
            BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
            DWORD wScrollNotify = -1;
            UINT msg = WM_VSCROLL;
            int oldXPos = CaretXPos, oldYPos = CaretYPos;
            int updateSelection = shiftPressed ? USF_EXPAND : USF_REMOVE;
            BOOL caretOnScreen = CaretYPos >= FirstVisibleLine && CaretYPos <= FirstVisibleLine + Height / FontHeight - 1;

            switch (wParam)
            {
            // vertikalni scrollovani
            case 'K':
            case VK_UP:
                if (!ctrlPressed && !altPressed)
                {
                    if (!shiftPressed && IsSelected())
                    {
                        SetCaretPos(SelectionCharBegin, SelectionLineBegin); // presuneme se na zacatek vyberu
                    }
                    // ohlidame aby slo vybirat jen v ramci jedne difference
                    // NOTE: Script[fvmOnlyDifferences] is empty when showing identical files
                    if (!Script[ViewMode].empty() && (ViewMode != fvmOnlyDifferences || !shiftPressed ||
                                                      CaretYPos == 0 || Script[ViewMode][CaretYPos - 1].GetClass() != CLineSpec::lcSeparator))
                    {
                        MoveCaret(CaretYPos - 1);
                    }
                }
                if (ctrlPressed && !shiftPressed && !altPressed)
                {
                    wScrollNotify = SB_LINEUP;
                    //updateSelection = USF_NOCHANGE;
                }
                break;

            case 'B':
            case VK_PRIOR:
                if (!ctrlPressed && !altPressed)
                {
                    if (ViewMode == fvmOnlyDifferences && shiftPressed)
                    {
                        // ohlidame, aby slo vybirat jen v ramci jedne difference
                        int i;
                        for (i = CaretYPos; i > CaretYPos - Height / FontHeight + 1 && i > 0; i--)
                        {
                            if (Script[ViewMode][i - 1].GetClass() == CLineSpec::lcSeparator)
                                break;
                        }
                        if (i > CaretYPos - Height / FontHeight + 1)
                        {
                            SetCaretPos(0, i);
                            break;
                        }
                    }
                    if (caretOnScreen)
                        wScrollNotify = SB_PAGEUP;
                    MoveCaret(CaretYPos - Height / FontHeight + 1);
                }
                if (ctrlPressed && !altPressed)
                    goto LHOME;
                break;

            case 'F':
            case VK_NEXT:
                if (!ctrlPressed && !altPressed)
                {
                    if (ViewMode == fvmOnlyDifferences && shiftPressed)
                    {
                        // ohlidame, aby slo vybirat jen v ramci jedne difference
                        int i;
                        for (i = CaretYPos; i < CaretYPos + Height / FontHeight + 1 && i < int(Script[ViewMode].size()) - 1; i++)
                        {
                            if (Script[ViewMode][i].GetClass() == CLineSpec::lcSeparator)
                                break;
                        }
                        if (i < CaretYPos + Height / FontHeight + 1)
                        {
                            SetCaretPos(INT_MAX, i);
                            break;
                        }
                    }
                    if (caretOnScreen)
                        wScrollNotify = SB_PAGEDOWN;
                    MoveCaret(CaretYPos + Height / FontHeight - 1);
                }
                if (ctrlPressed && !altPressed)
                    goto LEND;
                break;

            case 'J':
            case VK_DOWN:
                if (!ctrlPressed && !altPressed)
                {
                    if (!shiftPressed && IsSelected())
                    {
                        SetCaretPos(SelectionCharEnd, SelectionLineEnd); // presuneme se na konec vyberu
                    }
                    // ohlidame aby slo vybirat jen v ramci jedne difference
                    // NOTE: Script[fvmOnlyDifferences] is empty when showing identical files
                    if (!Script[ViewMode].empty() && (ViewMode != fvmOnlyDifferences || !shiftPressed ||
                                                      Script[ViewMode][CaretYPos].GetClass() != CLineSpec::lcSeparator))
                    {
                        MoveCaret(CaretYPos + 1);
                    }
                }
                if (ctrlPressed && !shiftPressed && !altPressed)
                {
                    wScrollNotify = SB_LINEDOWN;
                    //updateSelection = USF_NOCHANGE;
                }
                break;

            case 'G':
                if (!ctrlPressed && !shiftPressed && !altPressed)
                {
                    updateSelection = USF_REMOVE;
                    SetCaretPos(0, 0);
                }
                if (!ctrlPressed && shiftPressed && !altPressed)
                {
                    updateSelection = USF_REMOVE;
                    SetCaretPos(INT_MAX, int(Script[ViewMode].size()) - 1);
                }
                break;

            case VK_HOME:
                if (!ctrlPressed && !altPressed)
                    SetCaretPos(0, CaretYPos);
                if (ctrlPressed && !altPressed)
                {
                LHOME:

                    // ohlidame aby slo vybirat jen v ramci jedne difference
                    int i = 0;
                    if (ViewMode == fvmOnlyDifferences && shiftPressed)
                    {
                        i = CaretYPos;
                        for (; i > 0; i--)
                        {
                            if (Script[ViewMode][i - 1].GetClass() == CLineSpec::lcSeparator)
                                break;
                        }
                    }
                    SetCaretPos(0, i);
                }
                break;

            case VK_END:
                if (!ctrlPressed && !altPressed)
                    SetCaretPos(INT_MAX, CaretYPos);
                if (ctrlPressed && !altPressed)
                {
                LEND:

                    // ohlidame aby slo vybirat jen v ramci jedne difference
                    int i = int(Script[ViewMode].size()) - 1;
                    if (ViewMode == fvmOnlyDifferences && shiftPressed)
                    {
                        i = CaretYPos;
                        for (; i < int(Script[ViewMode].size()) - 1; i++)
                        {
                            if (Script[ViewMode][i].GetClass() == CLineSpec::lcSeparator)
                                break;
                        }
                    }
                    SetCaretPos(INT_MAX, i);
                }
                break;

            // horizontalni scrollovani
            case 'L':
            case VK_RIGHT:
            {
                if (altPressed)
                    break;

                if (!ctrlPressed && !shiftPressed && IsSelected())
                {
                    SetCaretPos(SelectionCharEnd, SelectionLineEnd); // presuneme se na konec vyberu
                    break;
                }

                // NOTE: Script[fvmOnlyDifferences] is empty when showing identical files
                if (Script[ViewMode].empty())
                    break;

                int l, len = 0;
                if (!Script[ViewMode][CaretYPos].IsBlank())
                {
                    l = Script[ViewMode][CaretYPos].GetLine();
                    len = LineLength(l);
                }

                if (CaretXPos == len)
                {
                    // presuneme se na nasledujici radek
                    // ohlidame aby slo vybirat jen v ramci jedne difference
                    if (ViewMode != fvmOnlyDifferences || !shiftPressed ||
                        Script[ViewMode][CaretYPos].GetClass() != CLineSpec::lcSeparator)
                    {
                        if (CaretYPos < int(Script[ViewMode].size()) - 1)
                            SetCaretPos(0, CaretYPos + 1);
                    }
                    break;
                }

                if (!ctrlPressed)
                    SetCaretPos(CaretXPos + 1, CaretYPos); // presuneme se na nasledujici znak
                else
                {
                    // presuneme se na konec slova/mezery/bloku jinych znaku
                    int xpos = CaretXPos;
                    NextCharBlock(l, xpos);

                    SetCaretPos(xpos, CaretYPos);
                }
                break;
            }

            case 'H':
            case VK_LEFT:
            {
                if (altPressed)
                    break;

                if (!ctrlPressed && !shiftPressed && IsSelected())
                {
                    SetCaretPos(SelectionCharBegin, SelectionLineBegin); // presuneme se na zacatek vyberu
                    break;
                }

                if (CaretXPos == 0)
                {
                    // posuneme se na predchozi radek
                    // ohlidame aby slo vybirat jen v ramci jedne difference
                    if (ViewMode != fvmOnlyDifferences || !shiftPressed ||
                        CaretYPos == 0 || Script[ViewMode][CaretYPos - 1].GetClass() != CLineSpec::lcSeparator)
                    {
                        if (CaretYPos > 0)
                            SetCaretPos(INT_MAX, CaretYPos - 1);
                    }
                    break;
                }

                if (!ctrlPressed)
                    SetCaretPos(CaretXPos - 1, CaretYPos); // posuneme se na predchozi znak
                else
                {
                    // presuneme se na konec slova/mezery/bloku jinych znaku
                    int l = Script[ViewMode][CaretYPos].GetLine();
                    int xpos = CaretXPos;
                    PrevCharBlock(l, xpos);

                    SetCaretPos(xpos, CaretYPos);
                }
                break;
            }

            case VK_SPACE:
            {
                //            CMainWindow * parent = (CMainWindow *) WindowsManager.GetWindowPtr(GetParent(HWindow));
                if (MainWindow /*parent*/)
                    MainWindow /*parent*/->SelectDifferenceByLine(CaretYPos, TRUE);
                if (shiftPressed)
                {
                    // zkusime vybrat celou differenci do bloku
                    int start, end;
                    if (MainWindow /*parent*/ && MainWindow /*parent*/->GetDifferenceRange(CaretYPos, &start, &end))
                    {
                        // ulozime stary vyber textu
                        int oldLineBegin = SelectionLineBegin;
                        int oldLineEnd = SelectionLineEnd;
                        int oldCharBegin = SelectionCharBegin;
                        int oldCharEnd = SelectionCharEnd;

                        // nastavime novy vyber textu, vybirame cele radky
                        SelectionLineBegin = start;
                        SelectionCharBegin = 0;
                        if (end + 1 < int(Script[ViewMode].size()))
                        {
                            SelectionLineEnd = end + 1;
                            SelectionCharEnd = 0;
                        }
                        else
                        {
                            SelectionLineEnd = end;
                            if (Script[ViewMode][end].IsBlank())
                                SelectionCharEnd = 0;
                            else
                            {
                                int l = Script[ViewMode][end].GetLine();
                                SelectionCharEnd = LineLength(l);
                            }
                        }

                        SetCaretPos(SelectionCharEnd, SelectionLineEnd);
                        RepaintSelection(oldLineBegin, oldLineEnd, oldCharBegin, oldCharEnd);
                        if (MainWindow /*parent*/)
                            MainWindow /*parent*/->UpdateToolbarButtons(UTB_TEXTSELECTION);
                        updateSelection = USF_NOCHANGE;
                    }
                }
                break;
            }

            default:
                updateSelection = USF_NOCHANGE;
            }

            switch (updateSelection)
            {
            case USF_REMOVE:
                RemoveSelection(TRUE);
                break;

            case USF_EXPAND:
            {
                int x1, x2, y1, y2;
                if (oldXPos == SelectionCharEnd && oldYPos == SelectionLineEnd)
                {
                    x1 = SelectionCharBegin;
                    y1 = SelectionLineBegin;
                    x2 = CaretXPos;
                    y2 = CaretYPos;
                }
                else
                {
                    if (IsSelected())
                    {
                        x1 = CaretXPos;
                        y1 = CaretYPos;
                        x2 = SelectionCharEnd;
                        y2 = SelectionLineEnd;
                    }
                    else
                    {
                        x1 = oldXPos;
                        y1 = oldYPos;
                        x2 = CaretXPos;
                        y2 = CaretYPos;
                    }
                }
                int oldLineBegin = SelectionLineBegin;
                int oldLineEnd = SelectionLineEnd;
                int oldCharBegin = SelectionCharBegin;
                int oldCharEnd = SelectionCharEnd;
                if (y1 < y2 ||
                    y1 == y2 && x1 < x2)
                {
                    SelectionLineBegin = y1;
                    SelectionCharBegin = x1;
                    SelectionLineEnd = y2;
                    SelectionCharEnd = x2;
                }
                else
                {
                    SelectionLineBegin = y2;
                    SelectionCharBegin = x2;
                    SelectionLineEnd = y1;
                    SelectionCharEnd = x1;
                }
                // zajistime prekresleni postizenych casti
                RepaintSelection(oldLineBegin, oldLineEnd, oldCharBegin, oldCharEnd);
                //            CMainWindow * parent = (CMainWindow *) WindowsManager.GetWindowPtr(GetParent(HWindow));
                if (MainWindow /*parent*/)
                    MainWindow /*parent*/->UpdateToolbarButtons(UTB_TEXTSELECTION);
            }
            case USF_NOCHANGE:; // nic nedelame
            }

            if (wScrollNotify != -1)
            {
                SendMessage(HWindow, msg, MAKELONG(wScrollNotify, TRUE), 0);
                if (msg != WM_VSCROLL)
                    SendMessage(Siblink->HWindow, msg, MAKELONG(wScrollNotify, TRUE), 0);
            }

            if (CaretYPos < FirstVisibleLine)
            {
                SendMessage(HWindow, WM_USER_VSCROLL, MAKELONG(SB_THUMBTRACK, TRUE), CaretYPos);
                SendMessage(Siblink->HWindow, WM_USER_VSCROLL, MAKELONG(SB_THUMBTRACK, TRUE), CaretYPos);
            }

            if (CaretYPos > FirstVisibleLine + Height / FontHeight - 1)
            {
                SendMessage(HWindow, WM_USER_VSCROLL, MAKELONG(SB_THUMBTRACK, TRUE), CaretYPos - Height / FontHeight + 1);
                SendMessage(Siblink->HWindow, WM_USER_VSCROLL, MAKELONG(SB_THUMBTRACK, TRUE), CaretYPos - Height / FontHeight + 1);
            }

            int width = Width - LineNumWidth;

            int x = 0;
            // NOTE: Script[fvmOnlyDifferences] is empty when showing identical files
            if (!Script[ViewMode].empty() && !Script[ViewMode][CaretYPos].IsBlank())
            {
                int l = Script[ViewMode][CaretYPos].GetLine();
                x = MeasureLine(l, CaretXPos);
            }

            if (x < FirstVisibleChar)
            {
                SendMessage(HWindow, WM_USER_HSCROLL, MAKELONG(SB_THUMBTRACK, TRUE), __max(x - FAST_LEFTRIGHT, 0));
                SendMessage(Siblink->HWindow, WM_USER_HSCROLL, MAKELONG(SB_THUMBTRACK, TRUE), __max(x - FAST_LEFTRIGHT, 0));
            }

            if (x > FirstVisibleChar + width / FontWidth - 1)
            {
                int page = __max(width / FontWidth, 0);
                int maxPos = __max(MaxWidth - page, 0);
                int pos = __min(x - page + FAST_LEFTRIGHT, maxPos);
                SendMessage(HWindow, WM_USER_HSCROLL, MAKELONG(SB_THUMBTRACK, TRUE), pos);
                SendMessage(Siblink->HWindow, WM_USER_HSCROLL, MAKELONG(SB_THUMBTRACK, TRUE), pos);
            }

            UpdateSelection();

            return CWindow::WindowProc(uMsg, wParam, lParam);
        }
        else
        {
            LRESULT ret = CFileViewWindow::WindowProc(uMsg, wParam, lParam);
            UpdateSelection();
            return ret;
        }

        break;
    }

    case WM_KEYUP:
    {
        switch (wParam)
        {
        case VK_CONTROL:
            UpdateSelection();
            break;
        case VK_SHIFT:
            if (DisplayCaret && Configuration.AutoCopy)
                CopySelection();
            break;
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case CM_SELECTALL:
        {
            if (DataValid)
            {
                int oldLineBegin, oldLineEnd;
                int oldCharBegin, oldCharEnd;

                if (Siblink)
                {
                    // zrusime stary vyber textu
                    Siblink->RemoveSelection(TRUE);
                }

                oldLineBegin = SelectionLineBegin;
                oldLineEnd = SelectionLineEnd;
                oldCharBegin = SelectionCharBegin;
                oldCharEnd = SelectionCharEnd;

                SelectionLineBegin = 0;
                SelectionCharBegin = 0;
                SelectionLineEnd = int(Script[ViewMode].size()) - 1;
                int l = Script[ViewMode][SelectionLineEnd].GetLine();
                if (Script[ViewMode][SelectionLineEnd].IsBlank())
                    SelectionCharEnd = 0;
                else
                {
                    l = Script[ViewMode][SelectionLineEnd].GetLine();
                    SelectionCharEnd = LineLength(l);
                }
                RepaintSelection(oldLineBegin, oldLineEnd, oldCharBegin, oldCharEnd);
                //            CMainWindow * parent = (CMainWindow *) WindowsManager.GetWindowPtr(GetParent(HWindow));
                if (MainWindow /*parent*/)
                    MainWindow /*parent*/->UpdateToolbarButtons(UTB_TEXTSELECTION);
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

    case WM_VSCROLL:
    {
        int pos = FirstVisibleLine;
        int page = Height / FontHeight;
        int maxPos = int(Script[ViewMode].size()) > page ? int(Script[ViewMode].size()) - page : 0;
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
            pos = si.nTrackPos;
            //          pos = int(lParam);
            break;
        }

        case SB_TOP:
            pos = 0;
            break;

        default:
            return 0;
        }
        SendMessage(Siblink->HWindow, WM_USER_VSCROLL, SB_THUMBTRACK, pos);
        lParam = pos;
    }
        // pokracujem dal ...

    case WM_USER_VSCROLL:
    {
        int pos = (int)lParam;
        if (pos != FirstVisibleLine)
        {
            int page = Height / FontHeight;
            if (abs(pos - FirstVisibleLine) < page - 1)
            {
                ScrollWindowEx(HWindow, 0, (FirstVisibleLine - pos) * FontHeight, NULL, NULL, NULL, NULL, SW_INVALIDATE);
            }
            else
                InvalidateRect(HWindow, NULL, FALSE);
            FirstVisibleLine = pos;
            SetScrollPos(HWindow, SB_VERT, pos, TRUE);
            UpdateWindow(HWindow);
            UpdateCaretPos(HIWORD(wParam));
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
        LONG width = __min(Width, SiblinkWidth) - LineNumWidth;
        int page = width / FontWidth;
        if (page < 1)
        {
            page = 1;
        }
        int maxPos = MaxWidth - page > 0 ? MaxWidth - page : 0;
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
        if (pos != FirstVisibleChar)
        {
            if (abs(pos - FirstVisibleChar) < page - 1)
            {
                RECT r;
                r.left = LineNumWidth;
                r.right = Width;
                r.top = 0;
                r.bottom = Height;
                ScrollWindowEx(HWindow, (FirstVisibleChar - pos) * FontWidth, 0, &r, &r, NULL, NULL, SW_INVALIDATE);
            }
            else
                InvalidateRect(HWindow, NULL, FALSE);
            FirstVisibleChar = pos;
            UpdateWindow(HWindow);
            UpdateCaretPos(FALSE);
        }
        SetScrollPos(HWindow, SB_HORZ, pos, TRUE);
        return 0;
    }
    }
    return CFileViewWindow::WindowProc(uMsg, wParam, lParam);
}
