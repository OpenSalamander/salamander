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

// proporcionalne k sirce okna, jde o "odhad"
#define FAST_LEFTRIGHT max(1, (Width - BORDER_WIDTH) / CharWidth / 6)
#define MAKEVIS_LEFTRIGHT max(0, (Width - BORDER_WIDTH) / CharWidth / 6)

void CViewerWindow::SetViewerCaption()
{
    char caption[MAX_PATH + 300];
    if (Caption == NULL)
    {
        if (FileName != NULL)
            lstrcpyn(caption, FileName, MAX_PATH); // caption podle souboru
        else
            caption[0] = 0;
    }
    else
        lstrcpyn(caption, Caption, MAX_PATH); // caption podle zadosti plug-inu
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
            *s = 0; // oriznuti prebytecnych mezer
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

    // zneplatnime buffer
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
        EndSelectionRow = -1; // vyradime optimalizaci
        EnableSetScroll = ((int)LOWORD(VScrollWParam) == SB_THUMBPOSITION);
        SeekY = (__int64)(ScrollScaleY * ((short)HIWORD(VScrollWParam)) + 0.5);
        SeekY = min(SeekY, MaxSeekY);
        BOOL fatalErr = FALSE;

        // inteligentnejsi nacteni bufferu pri nahodnem seekovani - nove cteni:
        // 1/6 pred, 2/6 za SeekY (Prepare cte jen 1/2 bufferu)
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
            UpdateWindow(HWindow); // aby se napocitalo ViewSize pro dalsi PageDown

            // dorolovani na dlouhy radek, msgbox pro prepnuti do HEX (ve FindBegin vyse nebo az v Paintu),
            // odpoved Ne - konci nenastavenou scrollbarou, tenhle hack to resi (asi by stacil i jen
            // invalidate scrollbary)
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
    // bezne se postrili pri SB_THUMBPOSITION, ale napr. pri vybaleni msgboxu behem
    // tazeni (too long text line, switch to HEX) zabira tohle jeste behem otevreneho msgboxu
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
    // zajistime prekresleni bloku
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
                ::ScrollWindow(HWindow, 0, CharHeight, NULL, NULL); //odscrollujeme okno
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
                ::ScrollWindow(HWindow, 0, -CharHeight, NULL, NULL); //odscrollujeme okno
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
        if (newFirstLineLen != -1) // situace: ma se odrolovat o jeden radek dolu (bude nova prvni radka)
        {
            max = newFirstLineLen;
            if (lineOffsetCount >= 3)
                lineOffsetCount -= 3; // posledni radku vynechame (misto ni je nova prvni radka)
        }
        // 'ignoreFirstLine' je TRUE: ma se odrolovat o jeden radek nahoru (bude nova posledni radka), takze
        // prvni radku vynechame (novou posledni radku nemame, kasleme na ni, bude jen castecne viditelna, staci v Paint())
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
        // u bloku tazeneho odzadu dopredu s koncem na konci nejdelsi radky (sirsi nez view)
        // ukoncene EOLem (ne wrapem) je 'x' pozice za koncem view (podminka vyse je splnena)
        // ale 'OriginX' uz nelze zvetsit (je uz 'maxOX'), zabranime zbytecnemu prekresleni
        // celeho view
        if (maxOX > OriginX) // jen pokud je mozne view jeste posunout vpravo
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

    if (WaitForViewerRefresh && // stav "fatal error", cekame na opravu pomoci WM_USER_VIEWERREFRESH
        uMsg != WM_SETCURSOR && // tyto zpravy jsou osetreny v obou stavech (o.k. a fatal) shodne
        uMsg != WM_DESTROY)
    {
        switch (uMsg)
        {
        case WM_ACTIVATE:
        {
            // zajistime schovani/zobrazeni Wait okenka (pokud existuje)
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
                    SeekY = LastSeekY; // chceme v HEX byt cca na stejne pozici jako predtim v Textovem rezimu
                if (!fatalErr && !ExitTextMode)
                {
                    SeekY = min(MaxSeekY, LastSeekY); // obnova LastSeekY v nove verzi souboru
                    __int64 newSeekY = FindBegin(SeekY, fatalErr);
                    if (!fatalErr && !ExitTextMode)
                        SeekY = newSeekY;
                }
                if (fatalErr)
                {
                    FatalFileErrorOccured();
                    //    // tenhle blok jsem zakomentoval, protoze jinak neslape "Retry" button v messageboxu s chybou otevrenem z LoadBehind() a LoadBefore()
                    //            if (Caption != NULL)
                    //            {
                    //              free(Caption);
                    //              Caption = NULL;
                    //            }
                    //            if (FileName != NULL) free(FileName);
                    //            if (Lock != NULL)
                    //            {
                    //              SetEvent(Lock);
                    //              Lock = NULL;     // ted uz je to jen na disk-cache
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
                        UpdateWindow(HWindow); // aby se napocitalo ViewSize pro dalsi PageDown
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
            return TRUE; // nemazem pozadi

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
            ResetMouseWheelAccumulator(); // pri zmene smeru otaceni kolecka je potreba nulovat akumulator

        DWORD delta = GetMouseWheelScrollLines(); // 'delta' muze byt az WHEEL_PAGESCROLL(0xffffffff)

        // standardni scrolovani bez modifikacnich klaves
        if (!controlPressed && !altPressed && !shiftPressed)
        {
            DWORD wheelScroll = GetMouseWheelScrollLines(); // muze byt az WHEEL_PAGESCROLL(0xffffffff)
            DWORD pageHeight = max(1, (DWORD)Height / CharHeight);
            wheelScroll = max(1, min(wheelScroll, pageHeight - 1)); // omezime maximalne na delku stranky

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

        // SHIFT: horizontalni rolovani
        if (!controlPressed && !altPressed && shiftPressed)
        {
            // poznamka: zaroven je volano z WM_MOUSEHWHEEL
            zDelta = (short)HIWORD(wParam);

            DWORD wheelScroll = GetMouseWheelScrollLines(); // 'delta' muze byt az WHEEL_PAGESCROLL(0xffffffff)
            DWORD pageWidth = max(1, (DWORD)(Width - BORDER_WIDTH) / CharWidth);
            wheelScroll = max(1, min(wheelScroll, pageWidth - 1)); // omezime maximalne na sirku stranky

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

        drag = DragQueryFile((HDROP)wParam, 0xFFFFFFFF, NULL, 0); // kolik souboru nam hodili
        if (drag > 0)
        {
            DragQueryFile((HDROP)wParam, 0, path, MAX_PATH);
            if (SalGetFullName(path))
            {
                if (Lock != NULL)
                {
                    SetEvent(Lock);
                    Lock = NULL; // ted uz je to jen na disk-cache
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
            // pri deaktivaci okna dame SkipOneActivateRefresh=TRUE, na chvilku, nedokazeme
            // totiz zjistit, jestli se prepina do hlavniho okna nebo jinam
            // hlavni okno pri prepnuti z viewru nebude delat refresh
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
            return TRUE; // nemazem pozadi
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
        if (IsWindowVisible(HWindow)) // posledni WM_SIZE prijde pri zavirani okna, o ten uz nestojime (error hlasky bez okna vieweru jsou silne nezadouci)
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
                    if (!fatalErr && !ExitTextMode && !calledHeightChanged) // inicializace noveho souboru
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
                    // omezime posun podle nejdelsi viditelne radky
                    __int64 maxOX = GetMaxOriginX();
                    if (OriginX > maxOX)
                    {
                        OriginX = maxOX;
                        InvalidateRect(HWindow, NULL, FALSE); // pro jistotu
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
        // musime proriznout historii v Find dialogu, je-li otevreny
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
                // ukonceni tazeni, musime OnVScroll() zavolat odtud, jinak problikne scrollbar na stare pozici
                VScrollWParam = wParam;
                KillTimer(HWindow, IDT_THUMBSCROLL);
                MSG msg; // nechceme zadny dalsi timer, vycistime queue
                while (PeekMessage(&msg, HWindow, WM_TIMER, WM_TIMER, PM_REMOVE))
                    ;
                OnVScroll();
                VScrollWParam = -1;
                break;
            }

            case SB_THUMBTRACK:
            {
                // vlastni rolovaci kod odsunut na timer, protoze USB mysi a MS scrollbary
                // vytvareji neskutecne veci: pokud je viewer fullscreen, vykresleni cele
                // obrazovky uz neco zabere a debilni scrollbar na vykresleni ceka, takze
                // se tahne jak zvejkacka; nepomohlo ani posteni message a odlozene kresleni
                // timer je jediny zpusob, na ktery jsme prisli
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
                    else // SB_THUMBTRACK a SB_THUMBPOSITION
                    {
                        EnableSetScroll = ((int)LOWORD(wParam) == SB_THUMBPOSITION);
                        OriginX = (__int64)(ScrollScaleX * ((short)HIWORD(wParam)) + 0.5);
                    }

                    // omezime posun podle nejdelsi viditelne radky
                    __int64 maxOX = GetMaxOriginX();
                    if (OriginX > maxOX)
                        OriginX = maxOX;
                    break;
                }
                }
                ResetFindOffsetOnNextPaint = TRUE;
                InvalidateRect(HWindow, NULL, FALSE);
                UpdateWindow(HWindow); // aby se napocitalo ViewSize pro dalsi PageDown
            }
            }
        }
        return 0;
    }

    case WM_COMMAND:
    {
        if (!IsWindowEnabled(HWindow)) // opatreni proti debilnim softum, ktery nam aktivuji hlavni okno ve stavu, kdy mame otevreny modalni dialog (napr. ClipMate)
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
            while (*s != 0) // vytvoreni double-null terminated listu
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
                        Lock = NULL; // ted uz je to jen na disk-cache
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
                if (ok && LOWORD(wParam) == CM_PREVSELFILE) // bereme jen selected soubory
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
                if (ok && LOWORD(wParam) == CM_NEXTSELFILE) // bereme jen selected soubory
                {
                    BOOL isSrcFileSel = FALSE;
                    ok = IsFileNameForViewerSelected(EnumFileNamesSourceUID, enumFileNamesLastFileIndex,
                                                     fileName, &isSrcFileSel, &srcBusy);
                    if (ok && !isSrcFileSel)
                        ok = FALSE;
                }
            }

            if (ok) // mame nove jmeno
            {
                if (Lock != NULL)
                {
                    SetEvent(Lock);
                    Lock = NULL; // ted uz je to jen na disk-cache
                }
                OpenFile(fileName, NULL, FALSE);

                // index nastavime i v pripade neuspechu, aby user mohl prejit na dalsi/predchozi soubor
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
            if (LastFindSeekY == SeekY && LastFindOffset != FindOffset) // obnova FindOffset po pohybu sem-tam
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
            { // postarame se, aby pri zmenach smeru hledani (F3/Shift+F3) nebylo 1. hledani zbytecne
                if (forward)
                    FindOffset = max(StartSelection, EndSelection);
                else
                    FindOffset = min(StartSelection, EndSelection);
            }
            BOOL noNotFound = FALSE;
            BOOL escPressed = FALSE;

            BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // ceka uz ?
            HCURSOR oldCur;
            if (setWait)
                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            CreateSafeWaitWindow(LoadStr(IDS_SEARCHINGTEXTESC), LoadStr(IDS_VIEWERTITLE), 1000, TRUE, HWindow);
            GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help

            // soubor nechame ve volani Prepare() otevrit pouze jednou a sami si ho na konci zavreme
            // opakovani otevirani/zavirani zdrzovalo prohledavani souboru na sitovem disku (1.5MB)
            HANDLE hFile = NULL;

            BOOL fatalErr = FALSE;
            FindingSoDonotSwitchToHex = TRUE; // behem hledani zakazeme prepinani do "hex" pri vice nez 10000 znacich na radku
            if (FindDialog.Regular)
            {
                if (RegExp.SetFlags(flags))
                {
                    BOOL oldConfigEOL_NULL = Configuration.EOL_NULL;
                    Configuration.EOL_NULL = TRUE; // nemam regexp na bin. stringy :(
                    __int64 len;
                    __int64 lineEnd;
                    __int64 lineBegin;
                    if (forward)
                    {
                        __int64 nextLineBegin;
                        if (!FindPreviousEOL(&hFile, FindOffset, FindOffset - FIND_LINE_LEN,
                                             lineBegin, nextLineBegin /* dummy */, FALSE, TRUE, fatalErr, NULL))
                        { // zacatek v nedohlednu
                            lineBegin = FindOffset;
                        }

                        if (!fatalErr /*&& !ExitTextMode*/) // FindingSoDonotSwitchToHex je TRUE, ExitTextMode nemuze vzniknout
                        {
                            while (lineBegin < FileSize)
                            {
                                __int64 maxSeek = min(FileSize, lineBegin + FIND_LINE_LEN);
                                if (!FindNextEOL(&hFile, lineBegin, maxSeek, lineEnd, nextLineBegin, fatalErr))
                                { // konec v nedohlednu
                                    lineEnd = nextLineBegin = maxSeek;
                                }
                                if (fatalErr)
                                    break;

                                if (lineBegin < lineEnd) // radek textu od lineBegin do lineEnd
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
                                                break; // nalezeno!
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
                                         lineEnd, previousLineEnd /* dummy */, fatalErr))
                        { // konec v nedohlednu
                            lineEnd = FindOffset;
                        }

                        if (!fatalErr)
                        {
                            while (lineEnd > 0)
                            {
                                if (!FindPreviousEOL(&hFile, lineEnd, lineEnd - FIND_LINE_LEN,
                                                     lineBegin, previousLineEnd, FALSE, TRUE, fatalErr, NULL))
                                { // zacatek v nedohlednu
                                    lineBegin = previousLineEnd = lineEnd - FIND_LINE_LEN;
                                }
                                if (fatalErr /*|| ExitTextMode*/)
                                    break; // FindingSoDonotSwitchToHex je TRUE, ExitTextMode nemuze vzniknout

                                if (lineBegin < lineEnd) // radek textu od lineBegin do lineEnd
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
                                                break; // nalezeno!
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
                                    break; // konec souboru
                            }
                            else
                                break; // konec souboru

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
                                break; // zacatek souboru
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
                                    break; // zacatek souboru
                            }
                            else
                                break; // zacatek souboru

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

            // pokud se soubor podarilo otevrit, je zavreni na nas
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
                MSG msg; // vyhodime nabufferovany ESC
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
                { // zobrazit selection - pujde-li, odrolujeme o tri radky
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
            // zapamatovani pozice posledniho hledani, pro detekci pohybu sem-tam
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
                    // if (startSel == -1) startSel = 0; // nemuze nastat (-1 muzou byt jen obe zaroven a to sem nedojde)
                    __int64 endSel = max(StartSelection, EndSelection);
                    // if (endSel == -1) endSel = 0; // nemuze nastat (-1 muzou byt jen obe zaroven a to sem nedojde)
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
                // if (startSel == -1) startSel = 0; // zbytecne resit, resi se o par radek nize
                __int64 end = max(StartSelection, EndSelection);
                // if (endSel == -1) endSel = 0;     // zbytecne resit, resi se o par radek nize
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
                while (*s != 0) // vytvoreni double-null terminated listu
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
                        if (attr != 0xFFFFFFFF) // file overwrite -> provedeme pres tmp-soubor (kvuli self-overwrite)
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
                                        break; // chyba cteni
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
                                    DeleteFile(tmpFile); // pri chybe ho smazem
                                else
                                {
                                    if (attr != 0xFFFFFFFF) // overwrite: tmp -> fileName
                                    {
                                        BOOL setAttr = ClearReadOnlyAttr(fileName, attr); // pro pripad read-only souboru
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
                                                SetFileAttributes(fileName, attr); // vratime puvodni atributy
                                            DeleteFile(tmpFile);                   // soubor nejde prepsat, smazeme tmp-soubor (je k nicemu)
                                            SetCursor(oldCur);
                                            SalMessageBox(HWindow, GetErrorText(err),
                                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                                        }
                                    }
                                }

                                // ohlasime zmenu na ceste (pribyl nas soubor)
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
                EndSelectionRow = -1; // vyradime optimalizaci
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
                UpdateWindow(HWindow); // aby se napocitalo ViewSize pro dalsi PageDown
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

            if (MainWindow != NULL && MainWindow->HWindow != NULL) // hodnotu vyvazime do pluginu jako SALCFG_AUTOCOPYSELTOCLIPBOARD, tedy upozornime na jeji zmenu
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
            OpenHtmlHelp(NULL, HWindow, HHCDisplayTOC, 0, TRUE); // nechceme dva messageboxy za sebou
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
                UpdateWindow(HWindow); // aby se napocitalo ViewSize pro dalsi PageDown
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
                    EndSelectionRow = -1; // vyradime optimalizaci
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
                    EndSelectionRow = -1; // vyradime optimalizaci
                    __int64 size = max(0, ViewSize - LastLineSize);
                    if (size == 0) // ani jeden cely radek -> emulace sipky dolu
                    {
                        SeekY = min(MaxSeekY, SeekY + FirstLineSize);
                    }
                    else // klasicky page down
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
                EndSelection = 0; // EndSelectionRow se nepouziva, protoze MouseDrag==FALSE

                extSelCh = TRUE;
                // break; // tady break chybi umyslne
            }
            case CM_FILEBEGIN:
            {
                if (FindDialog.Forward) // ruseni detekce hledani sem-tam, zde je to prijemne...
                {
                    LastFindSeekY = -1;
                    LastFindOffset = 0;
                }
                if (SeekY != 0 || OriginX != 0)
                {
                    EndSelectionRow = -1; // vyradime optimalizaci
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

                        if (lineCharLen > 0) // zajistime viditelnost konce posledniho radku (konce selectiony)
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
                    EndSelection = FileSize; // EndSelectionRow se nepouziva, protoze MouseDrag==FALSE

                    extSelCh = TRUE;
                }

                if (!FindDialog.Forward) // ruseni detekce hledani sem-tam, zde je to prijemne...
                {
                    LastFindSeekY = -1;
                    LastFindOffset = 0;
                }
                if (SeekY != MaxSeekY || OriginX != newOriginX)
                {
                    EndSelectionRow = -1; // vyradime optimalizaci
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
                EndSelectionPrefX = -1; // tady break nechybi!
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
                    { // hledani konce selectiony v LineOffset bez posledniho radku (pokud je jen castecne viditelny,
                        // je to OK, pokud je plne viditelny, dohledame konec selectiony jeste v nem, viz nize)
                        // pro wrap rezim: je-li blok dopredny (tazeny od zacatku ke konci souboru) a konci na konci
                        // wrap radku, kresli se ke konci wrap radku a ne k zacatku nasledujiciho radku (ty dve pozice
                        // maji stejny offset), je-li blok zpetny (tazeny v opacnem smeru) kresli se pro stejny offset
                        // od zacatku radky a ne od konce predchoziho radku = vyber 'endSelLineIndex' musi toto respektovat
                        if ((EndSelection > LineOffset[i] ||
                             // jen wrap rezim: pokud view zacina pokracovanim wrap radku a dopredny blok konci na offsetu
                             // zacatku view, potrebujeme predchozi radku, kde konci nakresleny blok ("kurzor" je mimo view)
                             EndSelection == LineOffset[i] &&
                                 (i > 0 || !WrapText || StartSelection > EndSelection || !WrapIsBeforeFirstLine)) &&
                            (EndSelection < LineOffset[i + 3] ||
                             WrapText && StartSelection < EndSelection && EndSelection == LineOffset[i + 1]))
                        {
                            endSelLineIndex = i / 3;
                            break;
                        }
                    }
                    if (endSelLineIndex == -1 && LineOffset.Count >= 3 &&    // pred poslednim radkem jsme konec selectiony nenasli,
                        LineOffset.Count / 3 <= Height / CharHeight &&       // zajima nas pouze plne viditelny posledni radek,
                        (EndSelection > LineOffset[LineOffset.Count - 3] ||  // konec selectiony je v poslednim radku souboru (nema
                         EndSelection == LineOffset[LineOffset.Count - 3] && // EOL, jinak by nebyl posledni) - specialne osetrime
                             (!WrapText || LineOffset.Count >= 6 ||          // konec dopredneho bloku na konci predchozi wrap radky
                              StartSelection > EndSelection || !WrapIsBeforeFirstLine)) &&
                        EndSelection <= LineOffset[LineOffset.Count - 2])
                    {
                        endSelLineIndex = LineOffset.Count / 3 - 1;
                    }
                    if (endSelLineIndex == -1 && // konec selectiony neni v plne viditelnem radku, je nejdrive potreba posunout view
                        !viewAlreadyMovedToSel)  // zkousime to jen jednou, sychr proti zacykleni
                    {
                        viewAlreadyMovedToSel = TRUE;
                        // posuneme view tak, aby na poslednim/prvnim radku byl konec selectiony
                        int lines = EndSelection > SeekY ? Height / CharHeight : 1;
                        if (lines <= 0)
                            break; // kdyz neni videt ani jeden radek, nefungujeme
                        BOOL fatalErr;
                        __int64 newSeekY = FindSeekBefore(EndSelection, lines, fatalErr, NULL, NULL, EndSelection > StartSelection);
                        if (fatalErr)
                            FatalFileErrorOccured();
                        if (fatalErr || ExitTextMode)
                            return 0;
                        SeekY = newSeekY;
                        OriginX = 0;
                        InvalidateRect(HWindow, NULL, FALSE);
                        UpdateWindow(HWindow); // nechame nove napocitat LineOffset
                        continue;
                    }
                    break;
                }
                if (endSelLineIndex == -1) // cosi se nepovedlo, necekane musime koncit
                {
                    skipCmd = TRUE;
                    break;
                }

                __int64 oldEndSel = EndSelection;
                __int64 curX = -1; // x-ova souradnice konce bloku, musime zajistit jeji viditelnost (upravit OriginX)
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
                            EndSelection = LineOffset[3 * endSelLineIndex]; // skok na zacatek
                        else
                            EndSelection--; // pohyb v radce

                        // wrap rezim: specialne osetrime konec dopredneho bloku na konci predchozi wrap radky
                        // (offset ma shodny se zacatkem teto radky)
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
                            else // radek je mimo view, je potreba posunout view o radek nahoru
                            {
                                scrollUp = TRUE;
                                moveIsDone = TRUE;
                                // aby tento radek jen neodroloval, ale prekreslil se (zmena oznaceni na zacatku)
                                InvalidateRows(minRow, maxRow, FALSE);
                            }
                        }
                        else
                        {
                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex))
                                return 0;
                        }
                    }
                    else // jdeme na konec predchozi radky
                    {
                        if (endSelLineIndex > 0)
                        {
                            __int64 newEndSel = LineOffset[3 * (endSelLineIndex - 1) + 1];
                            // mezi zacatkem a koncem wrapnute radky neni rozdil v offsetu, vyrobime ho
                            // umele (horni radek je wrapnuty = urcite na nem muzeme jit o jeden znak vlevo),
                            // musi jit o zpetny blok, jinak by byl konec bloku na konci predchozi radky
                            if (WrapText && newEndSel == EndSelection && newEndSel > 0)
                                newEndSel--; // vzdy by melo byt > 0
                            EndSelection = newEndSel;

                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex - 1))
                                return 0;

                            minRow--;
                            maxRow--;
                        }
                        else
                            scrollUp = TRUE; // je nutne odrolovat view o radek nahoru
                    }
                    if (scrollUp)
                    {
                        BOOL scrolled;
                        __int64 firstLineEndOff;
                        __int64 firstLineCharLen;
                        if (!ScrollViewLineUp(-1, &scrolled, FALSE, &firstLineEndOff, &firstLineCharLen))
                            return 0;
                        if (scrolled) // mame odrolovano, neprekresleno, prvni radka je od SeekY do firstLineEndOff
                        {
                            if (!moveIsDone &&
                                firstLineEndOff != -1 &&  // -1 = nezname offset konce prvni radky (hrozi pri zmene souboru)
                                SeekY <= firstLineEndOff) // jen tak pro sychr
                            {
                                __int64 newEndSel = firstLineEndOff;
                                // mezi zacatkem a koncem wrapnute radky neni rozdil v offsetu, vyrobime ho
                                // umele (horni radek je wrapnuty = urcite na nem muzeme jit o jeden znak vlevo),
                                // musi jit o zpetny blok, jinak by byl konec bloku na konci predchozi radky
                                if (WrapText && newEndSel == EndSelection && newEndSel > 0)
                                    newEndSel--; // vzdy by melo byt > 0
                                EndSelection = newEndSel;
                            }

                            if (!GetXFromOffsetInText(&curX, EndSelection, -1, SeekY, firstLineCharLen, firstLineEndOff))
                                return 0;

                            BOOL fullRedraw = FALSE; // zajistime viditelnost nove pozice konce bloku
                            EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, firstLineCharLen);
                            if (fullRedraw)
                                InvalidateRect(HWindow, NULL, FALSE);
                            else
                                ::ScrollWindow(HWindow, 0, CharHeight, NULL, NULL); //odscrollujeme okno
                            UpdateWindow(HWindow);
                        }
                        else // predchozi radek neexistuje, jsme asi na zacatku souboru
                        {
                            if (moveIsDone)
                                UpdateWindow(HWindow);
                            else
                                skipCmd = TRUE;
                        }
                        updateView = FALSE; // mame prekresleno, znovu neni potreba
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
                            EndSelection++; // pohyb v radce

                        // wrap rezim: specialne osetrime konec zpetneho bloku na zacatku dalsi wrap radky
                        // (offset ma shodny s koncem teto radky)
                        if (WrapText && EndSelection < StartSelection && EndSelection == LineOffset[3 * endSelLineIndex + 1] &&
                            endSelLineIndex + 1 < LineOffset.Count / 3 &&
                            EndSelection == LineOffset[3 * (endSelLineIndex + 1)])
                        {
                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex + 1))
                                return 0; // curX=0 (zac. radky)
                            if (endSelLineIndex + 1 >= Height / CharHeight)
                            {
                                BOOL fullRedraw = FALSE; // tady zrovna vzdy FALSE: wrap => OriginX==0 + curX==0
                                EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, -1, TRUE);
                                if (fullRedraw)
                                    InvalidateRect(HWindow, NULL, FALSE);
                                else
                                    InvalidateRows(minRow, maxRow, FALSE);
                                if (!ScrollViewLineDown(fullRedraw))
                                    UpdateWindow(HWindow); // neocekavane
                                updateView = FALSE;        // mame prekresleno, znovu neni potreba
                            }
                        }
                        else
                        {
                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex))
                                return 0;
                        }
                    }
                    else // jdeme na zacatek dalsi radky
                    {
                        // LineOffset vzdy obsahuje radek pod poslednim plne viditelnym (i kdyz z nej neni nic videt),
                        // ale prirozene jen pokud radek v souboru existuje
                        if (endSelLineIndex + 1 < LineOffset.Count / 3)
                        {
                            __int64 newEndSel = LineOffset[3 * (endSelLineIndex + 1)];
                            // mezi koncem wrapnute radky a zacatkem dalsi neni rozdil v offsetu, vyrobime ho
                            // umele (wrapnuti zarucuje, ze na dolnim radku muzeme jit aspon o jeden znak vpravo),
                            // musi jit o dopredny blok, jinak by byl konec bloku na zacatku dalsi radky
                            if (WrapText && newEndSel == EndSelection && newEndSel < LineOffset[3 * (endSelLineIndex + 1) + 1])
                            {
                                newEndSel++; // vzdy by melo byt < LineOffset[3 * (endSelLineIndex + 1) + 1]
                                maxRow++;
                            }
                            EndSelection = newEndSel;

                            if (!GetXFromOffsetInText(&curX, EndSelection, endSelLineIndex + 1))
                                return 0; // curX=0/1 (zac. radky)
                            if (endSelLineIndex + 1 == Height / CharHeight)
                            {                            // presli jsme na radek, ktery je jen castecne viditelny, je nutne odrolovat view o radek dolu
                                BOOL fullRedraw = FALSE; // OriginX muze byt > 0/1, protoze nemusi jit o wrap rezim, pak je potreba posun OriginX a full-redraw
                                EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, -1, TRUE);
                                if (fullRedraw)
                                    InvalidateRect(HWindow, NULL, FALSE);
                                else
                                    InvalidateRows(minRow, maxRow, FALSE);
                                if (!ScrollViewLineDown(fullRedraw))
                                    UpdateWindow(HWindow); // neocekavane
                                updateView = FALSE;        // mame prekresleno, znovu neni potreba
                            }
                        }
                        else
                            skipCmd = TRUE; // dalsi radek neexistuje = jsme na konci souboru, dal to nepujde
                    }
                    break;
                }

                case CM_EXTSEL_UP:
                case CM_EXTSEL_DOWN:
                {
                    if (EndSelectionPrefX == -1) // nemam preferovanou x-ovou souradnici, inicializuji na souradnici konce bloku
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
                        else // je nutne odrolovat view o radek nahoru
                        {
                            BOOL scrolled;
                            __int64 firstLineEndOff;
                            __int64 firstLineCharLen;
                            if (!ScrollViewLineUp(-1, &scrolled, FALSE, &firstLineEndOff, &firstLineCharLen))
                                return 0;
                            if (scrolled) // mame odrolovano, neprekresleno, prvni radka je od SeekY do firstLineEndOff
                            {
                                if (firstLineEndOff != -1 &&  // -1 = nezname offset konce prvni radky (hrozi pri zmene souboru)
                                    firstLineCharLen != -1 && // -1 = nezname delku prvni radky ve znacich
                                    SeekY <= firstLineEndOff) // jen tak pro sychr
                                {
                                    if (!GetOffsetFromXInText(&curX, &curOff, EndSelectionPrefX, -1, SeekY, firstLineCharLen,
                                                              firstLineEndOff))
                                        return 0;
                                    EndSelection = curOff;
                                    // aby tento radek jen neodroloval, ale prekreslil se (zmena oznaceni)
                                    InvalidateRows(minRow, maxRow, FALSE);
                                }

                                BOOL fullRedraw = FALSE; // zajistime viditelnost nove pozice konce bloku
                                if (curX != -1)
                                    EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, firstLineCharLen);
                                if (fullRedraw)
                                    InvalidateRect(HWindow, NULL, FALSE);
                                else
                                    ::ScrollWindow(HWindow, 0, CharHeight, NULL, NULL); //odscrollujeme okno
                                UpdateWindow(HWindow);
                                updateView = FALSE; // mame prekresleno, znovu neni potreba
                            }
                            else
                                skipCmd = TRUE; // predchozi radek neexistuje, jsme asi na zacatku souboru
                        }
                    }
                    else // CM_EXTSEL_DOWN
                    {
                        // LineOffset vzdy obsahuje radek pod poslednim plne viditelnym (i kdyz z nej neni nic videt),
                        // ale prirozene jen pokud radek v souboru existuje
                        if (endSelLineIndex + 1 < LineOffset.Count / 3)
                        {
                            if (!GetOffsetFromXInText(&curX, &curOff, EndSelectionPrefX, endSelLineIndex + 1))
                                return 0;
                            EndSelection = curOff;
                            maxRow++;

                            if (endSelLineIndex + 1 == Height / CharHeight)
                            { // presli jsme na radek, ktery je jen castecne viditelny, je nutne odrolovat view o radek dolu
                                BOOL fullRedraw = FALSE;
                                EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw, -1, TRUE);
                                if (fullRedraw)
                                    InvalidateRect(HWindow, NULL, FALSE);
                                else
                                    InvalidateRows(minRow, maxRow, FALSE);
                                if (!ScrollViewLineDown(fullRedraw))
                                    UpdateWindow(HWindow); // neocekavane
                                updateView = FALSE;        // mame prekresleno, znovu neni potreba
                            }
                        }
                        else
                            skipCmd = TRUE; // dalsi radek neexistuje = jsme na konci souboru, dal to nepujde
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
                        EnsureXVisibleInView(curX, EndSelection > StartSelection, fullRedraw); // zajistime viditelnost nove pozice konce bloku
                    if (fullRedraw)
                    {
                        InvalidateRect(HWindow, NULL, FALSE);
                        UpdateWindow(HWindow);
                    }
                    else
                        InvalidateRows(minRow, maxRow); // napocitam obdelnik, ktery je treba prekreslit
                    updateView = FALSE;                 // nemeni se SeekY, proto staci jen invalidate, prekresleni celeho view si odpustime
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
                    UpdateWindow(HWindow); // aby se napocitalo ViewSize pro dalsi PageDown
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
            BOOL breakOnCR = FALSE; // pro dohledani '\r' od '\r\n' konce radku
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
                    return 0; // chyba
                unsigned char* s = Buffer + (seek - Seek - 1);
                unsigned char* end = s - len;

                if (!wholeLine) // hledame zacatek slova
                {
                    while (s > end && (IsCharAlphaNumeric(*s) || *s == '_'))
                        s--;
                }
                else // hledame zacatek radky
                {
                    if (breakOnCR && s > end && *s == '\r')
                        s++; // aby naslo konec '\r\n' a ne konec '\r'
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
                                        breakOnCR = TRUE; // pri pristim pruchodu otestujeme, jestli bude '\r' pred timto '\n'
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
            BOOL breakOnLF = FALSE; // pro dohledani '\n' od '\r\n' konce radku
            while (seek < FileSize)
            {
                __int64 len = Prepare(NULL, seek, APROX_LINE_LEN, fatalErr);
                if (fatalErr)
                {
                    FatalFileErrorOccured();
                    return 0;
                }
                if (len == 0)
                    return 0; // chyba
                unsigned char* s = Buffer + (seek - Seek);
                unsigned char* end = s + len;

                if (!wholeLine) // hledame konec slova
                {
                    while (s < end && (IsCharAlphaNumeric(*s) || *s == '_'))
                        s++;
                }
                else // hledame konec radky
                {
                    if (breakOnLF)
                    {
                        if (s < end && *s == '\n')
                        {
                            s++; // aby naslo konec '\r\n'
                            selEnd = seek + len - (end - s);
                            break; // konec hledani
                        }
                        else
                        {
                            if (Configuration.EOL_CR)
                                ; // kdyz uz nevysel '\r\n', bereme aspon '\r' (nedelame nic)
                            else
                                breakOnLF = FALSE; // nevyslo ani '\r\n' ani '\r', pokracujeme ve cteni...
                        }
                    }

                    if (!breakOnLF)
                    {
                        BOOL eol = FALSE;
                        while (s < end)
                        {
                            if (Configuration.EOL_LF && *s == '\n')
                            {
                                s++;        // aby naslo konec '\n'
                                eol = TRUE; // ukoncit hledani
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
                                            s += 2;     // aby naslo konec '\r\n'
                                            eol = TRUE; // ukoncit hledani
                                            break;      // '\r\n'
                                        }
                                    }
                                    else
                                    {
                                        breakOnLF = TRUE; // pri pristim pruchodu otestujeme, jestli bude '\n' za timto '\r'
                                        testCR = FALSE;
                                    }
                                }
                                if (testCR)
                                {
                                    if (Configuration.EOL_CR)
                                    {
                                        s++;        // aby naslo konec '\r'
                                        eol = TRUE; // ukoncit hledani
                                        break;      // '\r'
                                    }
                                }
                            }
                            if (Configuration.EOL_NULL && *s == 0)
                            {
                                s++;        // aby naslo konec '\0'
                                eol = TRUE; // ukoncit hledani
                                break;      // '\0'
                            }
                            s++;
                        }
                        if (eol)
                        {
                            selEnd = seek + len - (end - s);
                            break; // konec hledani
                        }
                    }
                }
                if (s != end)
                {
                    selEnd = seek + len - (end - s);
                    break; // konec hledani
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
                StartSelection != EndSelection &&                 // je nejaky blok
                (off >= StartSelection || off >= EndSelection) && // [x,y] uvnitr bloku
                (off < StartSelection || off < EndSelection) &&
                (Type != vtHex || // zacatek bloku v hex rezimu jen pokud je [x,y] na hex. cislici
                 (off != StartSelection && off != EndSelection) || onHexNum))
            {
                BOOL msgBoxDisplayed;
                if (CheckSelectionIsNotTooBig(HWindow, &msgBoxDisplayed) &&
                    !msgBoxDisplayed) // po zobrazeni dotazu uz nemuzu rozjet D&D (user uz asi ani nedrzi levy tlacitko mysi)
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
                { // musime zjistit pozici zacatku/konce tazeni - leftMost musi byt FALSE
                    SetCapture(HWindow);
                    MouseDrag = TRUE;
                    SelectionIsFindResult = FALSE;
                    ChangingSelWithShiftKey = FALSE;
                    if (shiftPressed && StartSelection != -1) // zmena konce bloku (Shift+klik)
                    {
                        EndSelectionRow = -1; // zatim neni platny a nebudeme hledat, kde konci soucasny blok
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
            // jr: drive byla podminka if (x < 0) { x = 0; ...}, ale lidi nam nahlasili chybu, ze pokud je okno
            // vieweru maximalizovane, nejsou schopni rolovat vlevo; protoze mame vlevo prazdny pruh (text neni
            // uplne nalepeny), muzeme si dovolit rolovat pro x < BORDER_WIDTH
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
                    // zavedena optimalizce: detekujeme zmenenou oblast pri tazeni bloku
                    // a nechame prekreslit pouze tento obdelnik
                    BOOL optimalize = TRUE; // budeme prekreslovat jen vybrane radky?
                    RECT r;
                    int endSelectionRow = 0;
                    endSelectionRow = (int)(min(Height, y) / CharHeight);
                    int minRow = min(endSelectionRow, EndSelectionRow);
                    int maxRow = max(endSelectionRow, EndSelectionRow);
                    // ve wrap rezimu neni mezi zacatkem a koncem wrapnute radky rozdil v offsetu, takze blok
                    // koncici na zacatku radky za wrapem radky je nakresleny tak, ze konci na konci predchozi
                    // radky (chybi black-end vpravo), pri protazeni konce bloku dale musime prekreslit
                    // predchozi radku, aby se ji dokreslil black-end (prekresluje zbytecne predchozi radek
                    // mimo popsane situace, to nas ale netrapi, presna detekce situace je zbytecne slozita)
                    if (WrapText && StartSelection < EndSelection && off > EndSelection && minRow > 0)
                        minRow--;
                    // aby se pri zkracovani bloku po najeti na levy kraj view (zacatek radky za wrapem radky)
                    // prekreslil konec predchozi radky (smazal se mu black-end)
                    if (WrapText && StartSelection < EndSelection && off < EndSelection &&
                        (x - BORDER_WIDTH + CharWidth / 2) / CharWidth <= Configuration.TabSize / 2 && minRow > 0)
                    {
                        minRow--;
                    }
                    // napocitam obdelnik, ktery je treba prekreslit
                    r.left = 0;
                    r.top = minRow * CharHeight;
                    r.right = Width;
                    r.bottom = maxRow * CharHeight + CharHeight;
                    if (EndSelectionRow == -1)
                        optimalize = FALSE;
                    EndSelectionRow = endSelectionRow;

                    EndSelection = off;
                    // SelectionIsFindResult = FALSE;  // zbytecne, nastavuje se pri zacatku tazeni bloku (i pokracovani pres Shift+klik)
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
        // poznamka: zaroven je volano z WM_USER_MOUSEWHEEL pri drzeni Shift
        short zDelta = (short)HIWORD(wParam);
        if ((zDelta < 0 && MouseHWheelAccumulator > 0) || (zDelta > 0 && MouseHWheelAccumulator < 0))
            ResetMouseWheelAccumulator(); // pri zmene smeru naklapeni kolecka je potreba nulovat akumulator

        DWORD wheelScroll = GetMouseWheelScrollChars();
        DWORD pageWidth = max(1, (DWORD)(Width - BORDER_WIDTH) / CharWidth);
        wheelScroll = max(1, min(wheelScroll, pageWidth - 1)); // omezime maximalne na delku stranky

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

                    prevFile = ok || srcBusy;                     // jen pokud existuje nejaky predchazejici soubor (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)
                    firstLastFile = ok || srcBusy || noMoreFiles; // skok na prvni nebo posledni soubor jde jen pokud neni prerusena vazba se zdrojem (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)
                    if (firstLastFile)
                    {
                        enumFileNamesLastFileIndex = EnumFileNamesLastFileIndex;
                        ok = GetPreviousFileNameForViewer(EnumFileNamesSourceUID,
                                                          &enumFileNamesLastFileIndex,
                                                          FileName, TRUE /* prefer selected */, TRUE,
                                                          fileName, &noMoreFiles,
                                                          &srcBusy, NULL);
                        BOOL isSrcFileSel = FALSE;
                        if (ok)
                        {
                            ok = IsFileNameForViewerSelected(EnumFileNamesSourceUID, enumFileNamesLastFileIndex,
                                                             fileName, &isSrcFileSel, &srcBusy);
                            prevSelFile = ok && isSrcFileSel || srcBusy; // jen pokud je predchazejici soubor opravdu selected (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)
                        }

                        enumFileNamesLastFileIndex = EnumFileNamesLastFileIndex;
                        ok = GetNextFileNameForViewer(EnumFileNamesSourceUID,
                                                      &enumFileNamesLastFileIndex,
                                                      FileName, FALSE, TRUE,
                                                      fileName, &noMoreFiles,
                                                      &srcBusy, NULL);
                        nextFile = ok || srcBusy; // jen pokud existuje nejaky dalsi soubor (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)

                        enumFileNamesLastFileIndex = EnumFileNamesLastFileIndex;
                        ok = GetNextFileNameForViewer(EnumFileNamesSourceUID,
                                                      &enumFileNamesLastFileIndex,
                                                      FileName, TRUE /* prefer selected */, TRUE,
                                                      fileName, &noMoreFiles,
                                                      &srcBusy, NULL);
                        isSrcFileSel = FALSE;
                        if (ok)
                        {
                            ok = IsFileNameForViewerSelected(EnumFileNamesSourceUID, enumFileNamesLastFileIndex,
                                                             fileName, &isSrcFileSel, &srcBusy);
                            nextSelFile = ok && isSrcFileSel || srcBusy; // jen pokud je dalsi soubor opravdu selected (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)
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
                // pri prvnim volani bude menu naplneno a pri vsech volanich
                // bude radiak nastaven na CodeType
                CodeTables.InitMenu(subMenu, CodeType);

                if (CodePageAutoSelect)
                {
                    // pri auto-select neni zadna z polozek default
                    SetMenuDefaultItem(subMenu, -1, FALSE);
                }
                else
                {
                    // pokud neni auto-select, musi byt nektera polozka default
                    int defCodeType;
                    CodeTables.GetCodeType(DefaultConvert, defCodeType);
                    SetMenuDefaultItem(subMenu, CM_CODING_MIN + defCodeType, FALSE);
                }

                if (firstTime)
                {
                    // pripojime nase prikazy
                    int count = GetMenuItemCount(subMenu);

                    MENUITEMINFO mi;
                    memset(&mi, 0, sizeof(mi));
                    mi.cbSize = sizeof(mi);

                    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertMenuItem() dole...
MENU_TEMPLATE_ITEM ViewerCodingMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_VIEWERAUTOCODING
  {MNTT_IT, IDS_VIEWERSETDEFAULTCODING
  {MNTT_IT, IDS_VIEWERNEXTCODING
  {MNTT_IT, IDS_VIEWERPREVIOUSCODING
  {MNTT_PE, 0
};
*/

                    // uplne nahoru Recognize a separator
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

                    // dolu separator
                    InsertMenuItem(subMenu, count++, TRUE, &mi);

                    // a prikazy
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
            break; // stisk druheho Shift behem zmeny oznaceni pres Shift+sipky/Home/End neocekavame, kdyz k nemu dojde, rusi nam plany (nedojde ke zkopirovani oznaceni do schranky), ale na to kaslu...

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
