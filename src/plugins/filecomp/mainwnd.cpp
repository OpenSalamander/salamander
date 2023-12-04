// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

using namespace std;

// ****************************************************************************
//
// CMainWindow
//

HCURSOR HWaitCursor;

HPALETTE Palette = NULL; // paleta pro 256-ti barevny display
BOOL UsePalette = FALSE; // TRUE pokud mame jen 256 barev

CBandParams BandsParams[2];

const char* MAINWINDOW_CLASSNAME = "SFC Window Class";

CMainWindow::CMainWindow(char* path1, char* path2, CCompareOptions* options, UINT showCmd)
{
    CALL_STACK_MESSAGE1("CMainWindow::CMainWindow(, , )");
    Path1 = path1;
    Path2 = path2;
    FileView[fviLeft] = NULL;
    FileView[fviRight] = NULL;
    Active = 0;
    Initialized = FALSE;
    SelectedDifference = -1;
    DataValid = FALSE;
    FirstCompare = TRUE;
    Options = *options;
    DifferencesCount = 0;
    ViewMode = ::Configuration.ViewMode;
    ShowWhiteSpace = ::Configuration.ShowWhiteSpace;
    ShowCaret = FALSE;
    WorkerEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    OptionsChanged = FALSE;
    DetailedDifferences = ::Configuration.DetailedDifferences;
    InputEnabled = TRUE;
    OutOfRange = FALSE; // Used only in binary/hex view
    ShowCmd = showCmd;
    bOptionsChangedBeingHandled = FALSE;
}

CMainWindow::~CMainWindow()
{
    CALL_STACK_MESSAGE1("CMainWindow::~CMainWindow()");
    DataValid = FALSE;
    if (WorkerEvent)
        CloseHandle(WorkerEvent);
    MainWindowQueue.Remove(HWindow);
    ::Configuration.ViewMode = ViewMode;
    ::Configuration.ShowWhiteSpace = ShowWhiteSpace;
    //::Configuration.DetailedDifferences = DetailedDifferences;
}

BOOL CMainWindow::Init()
{
    CALL_STACK_MESSAGE1("CMainWindow::Init()");
    RECT r;
    GetClientRect(HWindow, &r);
    Height = r.bottom;
    Width = r.right;
    PrevSplitProp = SplitProp = 0.5;
    HeaderHeight = EnvFontHeight + 4;

    TBBUTTON buttons[7];
    int i;
    for (i = 0; i < SizeOf(buttons); i++)
    {
        buttons[i].fsStyle = TBSTYLE_BUTTON;

        switch (i)
        {
        case 0:
            buttons[i].iBitmap = 5;
            buttons[i].idCommand = CM_RECOMPARE;
            break;
        case 1:
            buttons[i].iBitmap = 0;
            buttons[i].idCommand = CM_COPY;
            break;
        case 2:
            buttons[i].iBitmap = 2;
            buttons[i].idCommand = 0;
            buttons[i].fsStyle = TBSTYLE_SEP;
            break;
        default:
            buttons[i].iBitmap = i - 2;
            buttons[i].idCommand = CM_COPY + i - 2;
        }
        buttons[i].fsState = TBSTATE_ENABLED;
        buttons[i].iString = 0;
    }
    COLORMAP cm;
    cm.from = 0x00FF00FF;
    cm.to = GetSysColor(COLOR_BTNFACE);
    HToolbar = CreateToolbarEx(HWindow,
                               WS_VISIBLE | WS_CHILD |
                                   WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                                   CCS_NORESIZE | CCS_NODIVIDER | CCS_NOPARENTALIGN |
                                   TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
                               IDC_TOOLBAR,
                               8,
                               0, //DLLInstance,
                               (UINT_PTR)CreateMappedBitmap(DLLInstance, IDB_TOOLBAR, 0, &cm, 1),
                               buttons,
                               SizeOf(buttons),
                               16,
                               16,
                               16,
                               16,
                               sizeof(TBBUTTON));
    if (!HToolbar)
    {
        TRACE_E("CreateToolbarEx has failed; last error: " << GetLastError());
        return FALSE;
    }

    ComboBox = new CComboBox();
    if (!ComboBox)
        return Error(HWND(NULL), IDS_LOWMEM);
    if (!ComboBox->CreateEx(0,
                            "ComboBox",
                            "",
                            WS_CHILD | WS_VSCROLL | WS_CLIPSIBLINGS | WS_VISIBLE |
                                CBS_DROPDOWN | CBS_AUTOHSCROLL,
                            //CBS_DROPDOWNLIST,
                            0, 0, 150, Height / 2,
                            HWindow,             // parent
                            (HMENU)IDC_DIFFLIST, // id
                            DLLInstance,
                            ComboBox))
    {
        TRACE_E("CreateWindowEx has failed; last error: " << GetLastError());
        return FALSE;
    }
    HWND editHWnd = GetWindow(ComboBox->HWindow, GW_CHILD);
    SendMessage(editHWnd, EM_SETREADONLY, TRUE, 0);
    SendMessage(ComboBox->HWindow, WM_SETFONT, (WPARAM)EnvFont, FALSE);

    CComboBoxEdit* edit = new CComboBoxEdit();
    if (edit)
        edit->AttachToWindow(editHWnd);
    else
        TRACE_E("LOW MEMORY");

    Rebar = new CRebar();
    if (!Rebar)
        return Error(HWND(NULL), IDS_LOWMEM);
    if (!Rebar->CreateEx(WS_EX_TOOLWINDOW,
                         REBARCLASSNAME,
                         "",
                         WS_VISIBLE | WS_BORDER | WS_CHILD |
                             WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                             RBS_VARHEIGHT | CCS_NODIVIDER |
                             RBS_BANDBORDERS | CCS_NOPARENTALIGN |
                             RBS_AUTOSIZE | RBS_DBLCLKTOGGLE,
                         0, 0, 0, 0,       // dummy
                         HWindow,          // parent
                         (HMENU)IDC_REBAR, // id
                         DLLInstance,
                         Rebar))
    {
        TRACE_E("CreateWindowEx on " << REBARCLASSNAME);
        return FALSE;
    }

    // nechceme vizualni styly pro rebar
    if (SalGUI->DisableWindowVisualStyles(Rebar->HWindow))
    {
        // vynutime si WS_BORDER, ktery se nekam "ztratil"
        DWORD style = GetWindowLong(Rebar->HWindow, GWL_STYLE);
        style |= WS_BORDER;
        SetWindowLong(Rebar->HWindow, GWL_STYLE, style);
    }

    // Initialize and send the REBARINFO structure.
    REBARINFO rbi;
    rbi.cbSize = sizeof(REBARINFO); // Required when using this struct.
    rbi.fMask = 0;
    rbi.himl = (HIMAGELIST)NULL;
    if (!SendMessage(Rebar->HWindow, RB_SETBARINFO, 0, (LPARAM)&rbi))
        return NULL;

    REBARBANDINFO rbbi;
    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE |
                 RBBIM_ID;
    rbbi.fStyle = RBBS_GRIPPERALWAYS;
    rbbi.hwndChild = HToolbar;
    rbbi.wID = IDC_TOOLBAR;
    rbbi.cxMinChild = 0;
    rbbi.cyMinChild = HIWORD(SendMessage(HToolbar, TB_GETBUTTONSIZE, 0, 0));

    Rebar->InsertBand(-1, &rbbi);

    //SendMessage(Rebar->HWindow, RB_GETBANDBORDERS, (WPARAM)0, (LPARAM)&r);
    //rbbi.fMask  = RBBIM_SIZE;
    //rbbi.cx = size.cx + r.left;
    //SendMessage(Rebar->HWindow, RB_SETBANDINFO, (WPARAM)0, (LPARAM)&rbbi);

    rbbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE |
                 RBBIM_SIZE | RBBIM_ID | RBBIM_TEXT;
    rbbi.lpText = LoadStr(IDS_DIFFERENCES);
    rbbi.hwndChild = ComboBox->HWindow;
    rbbi.wID = IDC_DIFFLIST;
    rbbi.cxMinChild = LOWORD(GetDialogBaseUnits()) * 4;
    GetWindowRect(ComboBox->HWindow, &r);
    rbbi.cyMinChild = r.bottom - r.top;
    //rbbi.cx         = 200;

    Rebar->InsertBand(-1, &rbbi);

    RestoreRebarLayout();

    RebarHeight = LONG(SendMessage(Rebar->HWindow, RB_GETBARHEIGHT, 0, 0)) + 4 + REBAR_BORDER;

    // vytvorime caption okna
    LeftHeader = new CFileHeaderWindow("");
    if (!LeftHeader)
        return Error(HWND(NULL), IDS_LOWMEM);
    if (!LeftHeader->CreateEx(WS_EX_STATICEDGE,
                              CWINDOW_CLASSNAME,
                              "",
                              WS_VISIBLE | WS_CHILD,
                              0, 0, 0, 0,
                              HWindow, // parent
                              NULL,    // menu
                              DLLInstance,
                              LeftHeader))
    {
        TRACE_E("CreateWindowEx has failed; last error: " << GetLastError());
        return FALSE;
    }

    RightHeader = new CFileHeaderWindow("");
    if (!RightHeader)
        return Error(HWND(NULL), IDS_LOWMEM);
    if (!RightHeader->CreateEx(WS_EX_STATICEDGE,
                               CWINDOW_CLASSNAME,
                               "",
                               WS_VISIBLE | WS_CHILD,
                               0, 0, 0, 0,
                               HWindow, // parent
                               NULL,    // menu
                               DLLInstance,
                               RightHeader))
    {
        TRACE_E("CreateWindowEx has failed; last error: " << GetLastError());
        return FALSE;
    }

    // vytvorime file view okna
    LeftFileViewHWnd = CreateWindowEx(WS_EX_STATICEDGE,
                                      FILEVIEWWINDOW_CLASSNAME,
                                      "",
                                      WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL,
                                      0, 0, 0, 0,
                                      HWindow,                 // parent
                                      (HMENU)IDC_LEFTFILEVIEW, // menu
                                      DLLInstance,
                                      NULL);
    if (!LeftFileViewHWnd)
    {
        TRACE_E("CreateWindowEx has failed; last error: " << GetLastError());
        return FALSE;
    }

    RightFileViewHWnd = CreateWindowEx(WS_EX_STATICEDGE,
                                       FILEVIEWWINDOW_CLASSNAME,
                                       "",
                                       WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL,
                                       0, 0, 0, 0,
                                       HWindow,                  // parent
                                       (HMENU)IDC_RIGHTFILEVIEW, // menu
                                       DLLInstance,
                                       NULL);
    if (!RightFileViewHWnd)
    {
        TRACE_E("CreateWindowEx has failed; last error: " << GetLastError());
        return FALSE;
    }

    // disablujem scrollbary
    EnableScrollBar(LeftFileViewHWnd, SB_BOTH, ESB_DISABLE_BOTH);
    EnableScrollBar(RightFileViewHWnd, SB_BOTH, ESB_DISABLE_BOTH);

    // vytvorime splitbaru
    SplitBar = new CSplitBarWindow(Configuration.HorizontalView ? sbHorizontal : sbVertical);
    if (!SplitBar)
        return Error(HWND(NULL), IDS_LOWMEM);
    if (!SplitBar->Create(SPLITBARWINDOW_CLASSNAME,
                          "",
                          WS_VISIBLE | WS_CHILD,
                          0, 0, 0, 0,
                          HWindow, // parent
                          NULL,    // menu
                          DLLInstance,
                          SplitBar))
    {
        TRACE_E("CreateWindowEx has failed; last error: " << GetLastError());
        return FALSE;
    }

    UpdateToolbarButtons(UTB_ALL);

    if (InputEnabled)
        DragAcceptFiles(HWindow, TRUE);

    Initialized = TRUE;

    // zajistime spusteni workeru
    PostMessage(HWindow, WM_COMMAND, CM_RECOMPARE, 0);

    // aktivujeme hlavni okno
    PostMessage(HWindow, WM_USER_ACTIVATEWINDOW, 0, 0);

    return TRUE;
}

void CMainWindow::EnableInput(BOOL enable)
{
    CALL_STACK_MESSAGE2("CMainWindow::EnableInput(%d)", enable);
    InputEnabled = enable;
    EnableWindow(LeftFileViewHWnd, enable);
    EnableWindow(RightFileViewHWnd, enable);
    EnableWindow(ComboBox->HWindow, enable);
    UpdateToolbarButtons(UTB_ALL | (enable ? 0 : UTB_FORCEDISABLE));
    if (enable)
        SetFocus(LeftFileViewHWnd);
    // disablovane okno nebude akceptovat drag&drop pozadavky
    DragAcceptFiles(HWindow, enable);
}

void CMainWindow::LayoutChilds()
{
    CALL_STACK_MESSAGE1("CMainWindow::LayoutChilds()");
    HDWP hdwp = BeginDeferWindowPos(6);
    if (hdwp)
    {
        LONG splitPos, height;

        if (sbVertical == SplitBar->GetType())
        {
            splitPos = (LONG)(SplitProp * Width - 1);
            if (splitPos < 2)
                splitPos = 2;
            if (splitPos > Width - 5)
                splitPos = Width - 5;
            height = 0;
        }
        else
        {
            height = Height - RebarHeight - 2 * HeaderHeight - 2;
            splitPos = (LONG)(SplitProp * height - 1);
            if (splitPos < 2)
                splitPos = 2;
            if (splitPos > height - 5)
                splitPos = height - 5;
        }
        hdwp = DeferWindowPos(hdwp, Rebar->HWindow, 0, 0, 0, Width, RebarHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
        if (sbVertical == SplitBar->GetType())
        {
            hdwp = DeferWindowPos(hdwp, LeftHeader->HWindow, 0, 0, RebarHeight, splitPos, HeaderHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            hdwp = DeferWindowPos(hdwp, RightHeader->HWindow, 0, splitPos + 3, RebarHeight, Width - splitPos - 3, HeaderHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            hdwp = DeferWindowPos(hdwp, LeftFileViewHWnd, 0, 0, RebarHeight + HeaderHeight + 2, splitPos, Height - HeaderHeight - RebarHeight - 2, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            hdwp = DeferWindowPos(hdwp, RightFileViewHWnd, 0, splitPos + 3, RebarHeight + HeaderHeight + 2, Width - splitPos - 3, Height - HeaderHeight - RebarHeight - 2, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            hdwp = DeferWindowPos(hdwp, SplitBar->HWindow, 0, splitPos, RebarHeight, 3, Height - RebarHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
        }
        else
        {
            hdwp = DeferWindowPos(hdwp, LeftHeader->HWindow, 0, 0, RebarHeight, Width, HeaderHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            hdwp = DeferWindowPos(hdwp, RightHeader->HWindow, 0, 0, RebarHeight + HeaderHeight + 2 + splitPos + 3, Width, HeaderHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            hdwp = DeferWindowPos(hdwp, LeftFileViewHWnd, 0, 0, RebarHeight + HeaderHeight + 2, Width, splitPos, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            hdwp = DeferWindowPos(hdwp, RightFileViewHWnd, 0, 0, RebarHeight + 2 * HeaderHeight + 2 + splitPos + 3, Width, height - splitPos - 3, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            hdwp = DeferWindowPos(hdwp, SplitBar->HWindow, 0, 0, RebarHeight + HeaderHeight + 2 + splitPos, Width, 3, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
        }
        EndDeferWindowPos(hdwp);
        if (DataValid)
        {
            FileView[fviLeft]->UpdateScrollBars();
            FileView[fviRight]->UpdateScrollBars();
        }
    }
}

void CMainWindow::SelectDifference(int i, int cmd, BOOL setSel, BOOL center)
{
    CALL_STACK_MESSAGE2("CMainWindow::SelectDifference(%d)", i);
    if (!DataValid)
        return;
    if (FileView[fviLeft]->Is(fvtText))
    {
        if (i < 0 || i >= DifferencesCount)
        {
            if (!DifferencesCount)
                return;
            i = DifferencesCount - 1;
        }
        int len = int(ChangesLengths[i]);
        ((CTextFileViewWindowBase*)FileView[fviLeft])->SelectDifference(int(ChangesToLines[ViewMode][i]), len, center);
        ((CTextFileViewWindowBase*)FileView[fviRight])->SelectDifference(int(ChangesToLines[ViewMode][i]), len, center);
        if (setSel)
            SendMessage(ComboBox->HWindow, CB_SETCURSEL, i, 0);
        SelectedDifference = i;
        UpdateToolbarButtons(UTB_DIFFSSELECTION);
    }
    else
    {
        QWORD offset;
        QWORD length = 0;

        //if (DifferencesCount && (i < 0 || i >= DifferencesCount)) i = DifferencesCount - 1;

        if (i < 0 && int(Changes.size()) < MaxBinChanges)
            i = int(Changes.size()) - 1;

        if (i >= 0 && i < int(Changes.size()) && !OutOfRange)
        {
            offset = Changes[i].Offset;
            length = Changes[i].Length;
        }
        else
        {
            if (i < 0 || Changes.empty() || OutOfRange || Changes.size() >= MaxBinChanges)
            {
                OutOfRange = TRUE;
                length = ((CHexFileViewWindow*)FileView[fviLeft])->FindDifference(cmd, &offset);
            }
        }

        if (length)
        {
            ((CHexFileViewWindow*)FileView[fviLeft])->SelectDifference(offset, length, center);
            ((CHexFileViewWindow*)FileView[fviRight])->SelectDifference(offset, length, center);

            if (OutOfRange)
                SelectedDifference = FindDifference(offset);
            else
                SelectedDifference = i;

            if (SelectedDifference == -1)
            {
                OutOfRange = TRUE;
                char buf1[32];
                char buf2[32];
                char report[200], fmt[128];
                SG->ExpandPluralString(fmt, sizeof(fmt), LoadStr(IDS_BINREPORT2), 1, &CQuadWord().SetUI64(length));
                sprintf(report, fmt, _ui64toa(length, buf1, 10),
                        QWord2Ascii(offset, buf2, FileView[fviLeft]->GetLineNumDigits()));
                SendMessage(ComboBox->HWindow, WM_SETTEXT, 0, (WPARAM)report);
                SendMessage(GetWindow(ComboBox->HWindow, GW_CHILD), EM_SETSEL, 0, -1);
            }
            else
            {
                OutOfRange = FALSE;
                if (setSel)
                    SendMessage(ComboBox->HWindow, CB_SETCURSEL, SelectedDifference, 0);
            }
        }
        UpdateToolbarButtons(UTB_DIFFSSELECTION);
    }
}

void CMainWindow::SelectDifferenceByLine(int line, BOOL center)
{
    CALL_STACK_MESSAGE2("CMainWindow::SelectDifferenceByLine(%d)", line);
    int i = int(LinesToChanges[line]);
    if (i != -1)
        SelectDifference(i, 0, TRUE, center);
}

BOOL CMainWindow::GetDifferenceRange(int line, int* start, int* end)
{
    CALL_STACK_MESSAGE2("CMainWindow::GetDifferenceRange(%d, , )", line);
    int i = int(LinesToChanges[line]);
    if (i == -1)
        return FALSE;
    *start = int(ChangesToLines[ViewMode][i]);
    *end = *start + int(ChangesLengths[i]) - 1;
    return TRUE;
}

void CMainWindow::SelectDifferenceByOffset(QWORD offset, BOOL center)
{
    CALL_STACK_MESSAGE1("CMainWindow::SelectDifferenceByOffset()");

    if (Changes.size() == 0 ||
        Changes[Changes.size() - 1].Offset +
                Changes[Changes.size() - 1].Length <=
            offset)
        return;

    int l = 0;
    int r = int(Changes.size()) - 1;
    int mid;

    while (l <= r)
    {
        mid = ((l + r) / 2);

        if (Changes[mid].Offset <= offset &&
            Changes[mid].Offset + Changes[mid].Length > offset)
        {
            OutOfRange = FALSE;
            SelectDifference(mid, 0, TRUE, center);
            return; // nasli jsme
        }
        if (Changes[mid].Offset < offset)
            l = mid + 1;
        else
            r = mid - 1;
    }
}

int CMainWindow::FindDifference(QWORD offset)
{
    CALL_STACK_MESSAGE1("CMainWindow::FindDifference()");
    if (Changes.size() == 0 ||
        Changes[Changes.size() - 1].Offset < offset)
        return -1;

    int l = 0;
    int r = int(Changes.size()) - 1;
    int mid;

    // vime ze tam je, hledame dokud nenajdem
    while (1)
    {
        mid = ((l + r) / 2);

        if (Changes[mid].Offset == offset)
            return mid;
        if (Changes[mid].Offset < offset)
            l = mid + 1;
        else
            r = mid - 1;
    }
}

BOOL CMainWindow::EnableToolbarButton(UINT cmd, BOOL enable)
{
    CALL_STACK_MESSAGE3("CMainWindow::EnableToolbarButton(0x%X, %d)", cmd, enable);
    if (SendMessage(HToolbar, TB_ISBUTTONENABLED, cmd, 0) == enable)
        return TRUE;
    return SendMessage(HToolbar, TB_ENABLEBUTTON, cmd, enable) != 0;
}

void CMainWindow::UpdateToolbarButtons(DWORD flags)
{
    CALL_STACK_MESSAGE2("CMainWindow::UpdateToolbarButtons(0x%X)", flags);
    HMENU menu = GetMenu(HWindow);
    if (DataValid && (flags & UTB_FORCEDISABLE) == 0)
    {
        if (flags & UTB_TEXTSELECTION)
        {
            BOOL bIsSelection = FileView[fviLeft]->IsSelected() || FileView[fviRight]->IsSelected();
            EnableToolbarButton(CM_COPY, bIsSelection);
            EnableMenuItem(menu, CM_COPY, bIsSelection ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED));
        }
        if (flags & UTB_DIFFSSELECTION)
        {
            EnableToolbarButton(CM_FIRSTDIFF, SelectedDifference != 0 && SelectedDifference != -1 || OutOfRange);
            EnableToolbarButton(CM_PREVDIFF, SelectedDifference != 0 && SelectedDifference != -1 || OutOfRange);
            EnableToolbarButton(CM_NEXTDIFF, DifferencesCount != 0 && SelectedDifference != DifferencesCount - 1);
            EnableToolbarButton(CM_LASTDIFF, DifferencesCount != 0 && SelectedDifference != DifferencesCount - 1);
            EnableMenuItem(menu, CM_FIRSTDIFF, MF_BYCOMMAND | (SelectedDifference != 0 && SelectedDifference != -1 || OutOfRange ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(menu, CM_PREVDIFF, MF_BYCOMMAND | (SelectedDifference != 0 && SelectedDifference != -1 || OutOfRange ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(menu, CM_NEXTDIFF, MF_BYCOMMAND | ((DifferencesCount != 0 && SelectedDifference != DifferencesCount - 1) ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(menu, CM_LASTDIFF, MF_BYCOMMAND | ((DifferencesCount != 0 && SelectedDifference != DifferencesCount - 1) ? MF_ENABLED : MF_GRAYED));
        }
        if (flags & UTB_MENU)
        {
            bool bHorizontalView = SplitBar->GetType() == sbHorizontal;

            CheckMenuItem(menu, CM_VIEW_HORIZONTAL, bHorizontalView ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
            if (bHorizontalView)
            {
                ModifyMenu(menu, CM_MAXLEFTVIEW, MF_BYCOMMAND | MF_STRING, CM_MAXLEFTVIEW, LoadStr(IDS_MAXTOPVIEW));
                ModifyMenu(menu, CM_MAXRIGHTVIEW, MF_BYCOMMAND | MF_STRING, CM_MAXRIGHTVIEW, LoadStr(IDS_MAXBOTTOMVIEW));
            }
            EnableMenuItem(menu, CM_RECOMPARE, MF_BYCOMMAND | MF_ENABLED);
            EnableToolbarButton(CM_RECOMPARE, TRUE);
            if (FileView[fviLeft] == NULL || FileView[fviLeft]->Is(fvtText))
            {
                CheckMenuItem(menu, CM_ONLYDIFFERENCES, MF_BYCOMMAND | (ViewMode == fvmStandard ? MF_UNCHECKED : MF_CHECKED));
                EnableMenuItem(menu, CM_ONLYDIFFERENCES, MF_BYCOMMAND | MF_ENABLED);
                CheckMenuItem(menu, CM_SHOWWHITESPACE, MF_BYCOMMAND | (ShowWhiteSpace ? MF_CHECKED : MF_UNCHECKED));
                EnableMenuItem(menu, CM_SHOWWHITESPACE, MF_BYCOMMAND | MF_ENABLED);
                CheckMenuItem(menu, CM_DETAILDIFF, MF_BYCOMMAND | (DetailedDifferences ? MF_CHECKED : MF_UNCHECKED));
                EnableMenuItem(menu, CM_DETAILDIFF, MF_BYCOMMAND | MF_ENABLED);
                CheckMenuItem(menu, CM_SHOWCARET, MF_BYCOMMAND | (ShowCaret ? MF_CHECKED : MF_UNCHECKED));
                EnableMenuItem(menu, CM_SHOWCARET, MF_BYCOMMAND | MF_ENABLED);
            }
            else
            {
                CheckMenuItem(menu, CM_ONLYDIFFERENCES, MF_BYCOMMAND | MF_UNCHECKED);
                EnableMenuItem(menu, CM_ONLYDIFFERENCES, MF_BYCOMMAND | MF_GRAYED);
                CheckMenuItem(menu, CM_SHOWWHITESPACE, MF_BYCOMMAND | MF_UNCHECKED);
                EnableMenuItem(menu, CM_SHOWWHITESPACE, MF_BYCOMMAND | MF_GRAYED);
                CheckMenuItem(menu, CM_DETAILDIFF, MF_BYCOMMAND | MF_UNCHECKED);
                EnableMenuItem(menu, CM_DETAILDIFF, MF_BYCOMMAND | MF_GRAYED);
                CheckMenuItem(menu, CM_SHOWCARET, MF_BYCOMMAND | MF_UNCHECKED);
                EnableMenuItem(menu, CM_SHOWCARET, MF_BYCOMMAND | MF_GRAYED);
            }
            CheckMenuItem(menu, CM_AUTOCOPY, Configuration.AutoCopy ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
        }
    }
    else
    {
        if (flags & UTB_TEXTSELECTION)
        {
            EnableToolbarButton(CM_COPY, FALSE);
            EnableMenuItem(menu, CM_COPY, MF_BYCOMMAND | MF_GRAYED);
        }
        if (flags & UTB_DIFFSSELECTION)
        {
            EnableToolbarButton(CM_FIRSTDIFF, FALSE);
            EnableToolbarButton(CM_PREVDIFF, FALSE);
            EnableToolbarButton(CM_NEXTDIFF, FALSE);
            EnableToolbarButton(CM_LASTDIFF, FALSE);
            EnableMenuItem(menu, CM_FIRSTDIFF, MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(menu, CM_PREVDIFF, MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(menu, CM_NEXTDIFF, MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(menu, CM_LASTDIFF, MF_BYCOMMAND | MF_GRAYED);
        }
        if (flags & UTB_MENU)
        {
            EnableMenuItem(menu, CM_RECOMPARE, MF_BYCOMMAND | MF_GRAYED);
            EnableToolbarButton(CM_RECOMPARE, FALSE);
            CheckMenuItem(menu, CM_ONLYDIFFERENCES, MF_BYCOMMAND | MF_UNCHECKED);
            EnableMenuItem(menu, CM_ONLYDIFFERENCES, MF_BYCOMMAND | MF_GRAYED);
            CheckMenuItem(menu, CM_SHOWWHITESPACE, MF_BYCOMMAND | MF_UNCHECKED);
            EnableMenuItem(menu, CM_SHOWWHITESPACE, MF_BYCOMMAND | MF_GRAYED);
            CheckMenuItem(menu, CM_SHOWCARET, MF_BYCOMMAND | MF_UNCHECKED);
            EnableMenuItem(menu, CM_SHOWCARET, MF_BYCOMMAND | MF_GRAYED);
            CheckMenuItem(menu, CM_SHOWCARET, MF_BYCOMMAND | MF_UNCHECKED);
            EnableMenuItem(menu, CM_SHOWCARET, MF_BYCOMMAND | MF_GRAYED);
        }
    }

    if (flags & UTB_MENU)
    {
        UINT enable = InputEnabled ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED);

        EnableMenuItem(menu, CM_COMPARE, enable);
        //EnableMenuItem(menu, CM_RECOMPARE, enable);
        EnableMenuItem(menu, CM_MAXIMIZE, enable);
        EnableMenuItem(menu, CM_VIEW_HORIZONTAL, enable);
        EnableMenuItem(menu, CM_OPTIONS, enable);
        EnableMenuItem(menu, CM_HELP, enable);
        EnableMenuItem(menu, CM_HELPKEYBOARD, enable);
        EnableMenuItem(menu, CM_ABOUT, enable);
    }
}

BOOL CMainWindow::RebuildFileViewScripts(BOOL& cancel)
{
    CALL_STACK_MESSAGE1("CMainWindow::RebuildFileViewScripts()");

    DataValid = FALSE;
    cancel = FALSE;

    CTextFileViewWindowBase* leftFileView = (CTextFileViewWindowBase*)FileView[fviLeft];
    CTextFileViewWindowBase* rightFileView = (CTextFileViewWindowBase*)FileView[fviRight];

    int maxWidth = 0;
    if (leftFileView->RebuildScript(ChangesLengths, ChangesToLines, ViewMode,
                                    TRUE, maxWidth, cancel) &&
        rightFileView->RebuildScript(ChangesLengths, ChangesToLines, ViewMode,
                                     FALSE, maxWidth, cancel) &&
        leftFileView->SetMaxWidth(maxWidth) &&
        rightFileView->SetMaxWidth(maxWidth))
    {
        // compute LinesToChanges
        LinesToChanges.resize(leftFileView->GetScriptSize());
        fill(LinesToChanges.begin(), LinesToChanges.end(), -1);
        DWORD i;
        for (i = 0; i < ChangesLengths.size(); ++i)
        {
            fill(
                LinesToChanges.begin() + ChangesToLines[ViewMode][i],
                LinesToChanges.begin() + ChangesToLines[ViewMode][i] + ChangesLengths[i],
                i);
        }

        ((CTextFileViewWindowBase*)FileView[fviLeft])->SetDetailedDifferences(DetailedDifferences);
        ((CTextFileViewWindowBase*)FileView[fviRight])->SetDetailedDifferences(DetailedDifferences);

        DataValid = TRUE;

        if (SelectedDifference != -1)
            SelectDifference(SelectedDifference);

        // zajistime prekresleni vsech oken
        InvalidateRect(LeftFileViewHWnd, NULL, FALSE);
        UpdateWindow(LeftFileViewHWnd);
        InvalidateRect(RightFileViewHWnd, NULL, FALSE);
        UpdateWindow(RightFileViewHWnd);
        ShowWindow(HWindow, SW_SHOW);
        SetForegroundWindow(HWindow);
        UpdateWindow(HWindow);

        return TRUE; // ok
    }

    // nekde se stala chyba
    FileView[fviLeft]->InvalidateData(); // zajistime prekresleni oken
    FileView[fviRight]->InvalidateData();
    LeftHeader->SetText("");
    RightHeader->SetText("");
    return FALSE;
}

void CMainWindow::ResetComboBox(BOOL* cancel)
{
    CALL_STACK_MESSAGE1("CMainWindow::ResetComboBox()");
    SendMessage(ComboBox->HWindow, WM_SETREDRAW, FALSE, 0);
    SendMessage(ComboBox->HWindow, CB_RESETCONTENT, 0, 0);
    if (ChangesToLines[ViewMode].size())
    {
        GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help

        size_t i;
        for (i = 0; i < TextChanges.size();)
        {
            size_t deleted = TextChanges[i].Deleted;   // # lines of file 0 changed here.
            size_t inserted = TextChanges[i].Inserted; // # lines of file 1 changed here.
            size_t line0 = TextChanges[i].DeletePos;   // Line number of 1st deleted line.
            size_t line1 = TextChanges[i].InsertPos;   // Line number of 1st inserted line.
            const char* path0 = SG->SalPathFindFileName(Path1);
            const char* path1 = SG->SalPathFindFileName(Path2);
            char buf[MAX_PATH * 3];

            ++i;

            if (deleted)
            {
                if (inserted)
                {
                    // change
                    char buf2[MAX_PATH * 2];
                    if (deleted > 1)
                        sprintf(buf2, LoadStr(IDS_CHANGEFROM2), i, line0 + 1, line0 + deleted, path0);
                    else
                        sprintf(buf2, LoadStr(IDS_CHANGEFROM1), i, line0 + 1, path0);
                    if (inserted > 1)
                        sprintf(buf, LoadStr(IDS_CHANGETO2), buf2, line1 + 1, line1 + inserted, path1);
                    else
                        sprintf(buf, LoadStr(IDS_CHANGETO1), buf2, line1 + 1, path1);
                }
                else
                {
                    // delete
                    if (deleted > 1)
                        sprintf(buf, LoadStr(IDS_DELETE2), i, line0 + 1, line0 + deleted, path0);
                    else
                        sprintf(buf, LoadStr(IDS_DELETE1), i, line0 + 1, path0);
                }
            }
            else
            {
                // insert
                if (inserted > 1)
                    sprintf(buf, LoadStr(IDS_ADD2), i, line1 + 1, line1 + inserted, path0, line0, path1);
                else
                    sprintf(buf, LoadStr(IDS_ADD1), i, line1 + 1, path0, line0, path1);
            }

            LRESULT ret = SendMessage(ComboBox->HWindow, CB_ADDSTRING, 0, (LPARAM)buf);
            if (ret == CB_ERR || ret == CB_ERRSPACE)
            {
                TRACE_E("CB_ADDSTRING has failed, i = " << DWORD(i));
                break;
            }

            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == HWindow)
            {
                MSG msg; // vyhodime nabufferovany ESC
                while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                    ;
                if (cancel)
                {
                    *cancel = TRUE;
                    break;
                }
            }
        }
    }
    SendMessage(ComboBox->HWindow, WM_SETREDRAW, TRUE, 0);
}

void CMainWindow::SaveRebarLayout()
{
    CALL_STACK_MESSAGE1("CMainWindow::SaveRebarLayout()");
    // ulozime informace o bandu s toolbarou
    Rebar->GetBandParams(IDC_TOOLBAR, BandsParams + BI_TOOLBAR);
    // ulozime informace o bandu s combacem
    Rebar->GetBandParams(IDC_DIFFLIST, BandsParams + BI_DIFFLIST);
}

void CMainWindow::RestoreRebarLayout()
{
    CALL_STACK_MESSAGE1("CMainWindow::RestoreRebarLayout()");
    if (BandsParams[0].Width == -1)
    {
        // rozmery nejsou ulozene v konfiguraci
        // nastavime defaultni layout a rozmery napocitame
        SIZE size;
        RECT r, r2;
        LRESULT index = SendMessage(Rebar->HWindow, RB_IDTOINDEX, IDC_TOOLBAR, 0);
        SendMessage(HToolbar, TB_GETMAXSIZE, (WPARAM)0, (LPARAM)&size);
        SendMessage(Rebar->HWindow, RB_GETBANDBORDERS, (WPARAM)index, (LPARAM)&r);
        GetClientRect(HWindow, &r2);
        r2.right -= r2.left;
        // Petr: upraveno, aby pri default konfiguraci zabiral Differences band co nejvetsi casy rebaru (az na toolbaru a jeji spacer)
        BandsParams[BI_TOOLBAR].Width = size.cx + r.left + 50 /* spacer */;
        BandsParams[BI_TOOLBAR].Style = 0;
        BandsParams[BI_TOOLBAR].Index = 0;
        if (r2.right > (LONG)BandsParams[BI_TOOLBAR].Width &&
            r2.right - BandsParams[BI_TOOLBAR].Width > 2 * BandsParams[BI_TOOLBAR].Width)
        {
            BandsParams[BI_DIFFLIST].Width = r2.right - BandsParams[BI_TOOLBAR].Width;
        }
        else
        {
            BandsParams[BI_DIFFLIST].Width = 2 * BandsParams[BI_TOOLBAR].Width;
        }
        BandsParams[BI_DIFFLIST].Style = 0;
        BandsParams[BI_DIFFLIST].Index = 1;
    }
    // band s toolbarou
    Rebar->SetBandParams(IDC_TOOLBAR, BandsParams + BI_TOOLBAR);
    // band s combacem
    Rebar->SetBandParams(IDC_DIFFLIST, BandsParams + BI_DIFFLIST);
}

void CMainWindow::SpawnWorker(const char* path1, const char* path2,
                              BOOL recompare, const CCompareOptions& options)
{
    CALL_STACK_MESSAGE3("CMainWindow::SpawnWorker(%s, %s, )", path1, path2);

    Recompare = recompare;

    // v pripade, ze jeste bezi worker, ukoncime ho
    CancelWorker = CW_SILENT;
    TRACE_I("SpawnWorker: waiting 'til worker finished");
    while (1)
    {
        if (MsgWaitForMultipleObjects(1, &WorkerEvent, FALSE, INFINITE, QS_ALLEVENTS | QS_SENDMESSAGE) == WAIT_OBJECT_0)
            break;
        TRACE_I("HaveMessage");
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(HWindow, HAccels, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    TRACE_I("SpawnWorker: worker has finished");

    CalculatingDetailedDifferences = FALSE;
    CancelWorker = CW_CONTINUE;
    CCompareOptions opt = options;
    opt.DetailedDifferences = DetailedDifferences ? 1 : 0;
    CFilecompWorker* worker = new CFilecompWorker(HWindow, HWindow, path1,
                                                  path2, opt, CancelWorker, WorkerEvent);
    if (!worker)
        Error(HWindow, IDS_LOWMEM);
    else
    {
        ResetEvent(WorkerEvent);
        if (!worker->Create(ThreadQueue))
        {
            delete worker;
            SetEvent(WorkerEvent); // aby jsme na nej marne necekali
            TRACE_I("Nepovedlo se spustit diff worker thread.");
        }
        else
        {
            EnableInput(FALSE);
            char buf[MAX_PATH * 2 + 200];
            sprintf(buf, LoadStr(IDS_MAINWNDHEADERCOMPUTING), SG->SalPathFindFileName(path1),
                    SG->SalPathFindFileName(path2));
            SetWindowText(HWindow, buf);
            SetWait(TRUE);
        }
    }
}

void CMainWindow::SetWait(BOOL wait)
{
    CALL_STACK_MESSAGE2("CMainWindow::SetWait(%d)", wait);
    if (Wait != wait)
    {
        Wait = wait;
        POINT pt;
        GetCursorPos(&pt);

        // zajistime spravne prekresli kursoru
        if (Wait)
            SetCursor(HWaitCursor);
        else
            SetCursorPos(pt.x, pt.y);
    }
}

template <class CChar>
bool CMainWindow::TextFilesDiffer(CTextCompareResults<CChar>* res, char* message, UINT& type, const char* (&encoding)[2])
{
    DataValid = FALSE;

    // ulozime vysledky
    LinesToChanges.clear();
    ChangesToLines[0].clear();
    ChangesToLines[1].clear();
    res->ChangesToLines.swap(ChangesToLines[0]);
    res->ChangesLengths.swap(ChangesLengths);
    res->Changes.swap(TextChanges);

    // zajistime spravny typ file view controlu
    if (FileView[fviLeft])
    {
        FileView[fviLeft]->DetachWindow();
        delete FileView[fviLeft];
    }
    FileView[fviLeft] = new TTextFileViewWindow<CChar>(fviLeft, ShowWhiteSpace, this);
    if (!FileView[fviLeft])
    {
        strcpy(message, LoadStr(IDS_LOWMEM));
        return FALSE; // nebudeme deallokovat buffery predane v CTextCompareResults
    }
    FileView[fviLeft]->AttachToWindow(LeftFileViewHWnd);
    // zajistime incializaci okna
    SendMessage(LeftFileViewHWnd, WM_USER_CREATE, 0, 0);

    if (FileView[fviRight])
    {
        FileView[fviRight]->DetachWindow();
        delete FileView[fviRight];
    }
    FileView[fviRight] = new TTextFileViewWindow<CChar>(fviRight, ShowWhiteSpace, this);
    if (!FileView[fviRight])
    {
        strcpy(message, LoadStr(IDS_LOWMEM));
        return FALSE; // nebudeme deallokovat buffery predane v CTextCompareResults
    }
    FileView[fviRight]->AttachToWindow(RightFileViewHWnd);
    // zajistime incializaci okna
    SendMessage(RightFileViewHWnd, WM_USER_CREATE, 0, 0);

    FileView[fviLeft]->SetSiblink(FileView[fviRight]);
    FileView[fviRight]->SetSiblink(FileView[fviLeft]);

    TTextFileViewWindow<CChar>* leftFileView = (TTextFileViewWindow<CChar>*)FileView[fviLeft];
    TTextFileViewWindow<CChar>* rightFileView = (TTextFileViewWindow<CChar>*)FileView[fviRight];

    // prebiram data, odted uz jsem zodpovedny za dealokaci dat
    // predanych v CTextCompareResults
    leftFileView->SetData(res->Files[0].Text, res->Files[0].Lines, res->Files[0].LineScript);
    rightFileView->SetData(res->Files[1].Text, res->Files[1].Lines, res->Files[1].LineScript);

    BOOL success = FALSE;
    BOOL cancel;
    if (!Recompare)
        SelectedDifference = -1;
    Options = res->Options;
    if (RebuildFileViewScripts(cancel))
    {
        DifferencesCount = int(ChangesToLines[ViewMode].size());

        strcpy(Path1, res->Files[0].Name);
        strcpy(Path2, res->Files[1].Name);
        LeftHeader->SetText(Path1);
        RightHeader->SetText(Path2);

        ResetComboBox(&cancel);
        if (!cancel)
        {
            SetFocus(FileView[Active]->HWindow);

            SelectDifference(max(SelectedDifference, 0));

            success = TRUE;
        }
    }

    if (cancel)
    {
        strcpy(message, LoadStr(IDS_CANCELED));
        type = MB_ICONEXCLAMATION;
    }
    if (!success)
    {
        SendMessage(ComboBox->HWindow, CB_RESETCONTENT, 0, 0);
        DataValid = FALSE;
        LeftHeader->SetText("");
        RightHeader->SetText("");
        leftFileView->InvalidateData(); // zajistime prekresleni oken
        rightFileView->InvalidateData();
    }

    encoding[0] = res->Files[0].Encoding;
    encoding[1] = res->Files[1].Encoding;

    return TRUE;
}

LRESULT
CMainWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CMainWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                             wParam, lParam);
    //TRACE_I("CMainWindow::WindowProc(uMsg " << (void *)uMsg << " wParam " << wParam << " lParam " << lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        if (!Init())
            return -1;
        return 0;
    }

    case WM_ERASEBKGND:
    {
        return 1;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(HWindow, &ps);

        RECT r1, r2, r3;
        GetClientRect(HWindow, &r1);
        GetClientRect(LeftHeader->HWindow, &r2);
        // vykreslime mezeru mezi rebarou a headerem
        r3.top = RebarHeight - REBAR_BORDER;
        r3.bottom = RebarHeight;
        r3.left = 0;
        r3.right = r1.right;
        FillRect(dc, &r3, (HBRUSH)(COLOR_BTNFACE + 1));
        // vykreslime mezeru mezi headerem a textovym oknem
        r3.top = RebarHeight + r2.bottom + 2;
        r3.bottom = r3.top + 2;
        FillRect(dc, &r3, (HBRUSH)(COLOR_BTNFACE + 1));

        EndPaint(HWindow, &ps);
        return 0;
    }

    case WM_SIZE:
    {
        Height = HIWORD(lParam);
        Width = LOWORD(lParam);
        LayoutChilds();
        return 0;
    }

    case WM_ACTIVATE:
    {
        if (InputEnabled && LOWORD(wParam) != WA_INACTIVE && DataValid)
        {
            SetFocus(FileView[Active]->HWindow);
            return 0;
        }
        break;
    }

    case WM_TIMER:
    {
        if (wParam == 666) // mame si poslat CM_EXIT, blokujici dialog uz je snad sestreleny
        {
            KillTimer(HWindow, 666);
            PostMessage(HWindow, WM_COMMAND, CM_EXIT, 0);
            return 0;
        }
        break;
    }

    case WM_COMMAND:
    {
        if (!InputEnabled)
        {
            if (LOWORD(wParam) == CM_CANCEL)
                CancelWorker = CW_CANCEL;
            if (LOWORD(wParam) == CM_EXIT)
                CancelWorker = CW_EXIT;
            return 0;
        }
        switch (LOWORD(wParam))
        {
        case CM_MAXIMIZE:
        {
            WPARAM wParam2;
            if (IsZoomed(HWindow))
                wParam2 = SC_RESTORE;
            else
                wParam2 = SC_MAXIMIZE;
            SendMessage(HWindow, WM_SYSCOMMAND, wParam2, 0);
            return 0;
        }

        case CM_VIEW_HORIZONTAL:
        {
            bool bHoriz = SplitBar->GetType() != sbHorizontal;
            HMENU hMenu = GetMenu(HWindow);

            SplitBar->SetType(bHoriz ? sbHorizontal : sbVertical);
            CheckMenuItem(hMenu, CM_VIEW_HORIZONTAL, MF_BYCOMMAND | (bHoriz ? MF_CHECKED : MF_UNCHECKED));
            ModifyMenu(hMenu, CM_MAXLEFTVIEW, MF_BYCOMMAND | MF_STRING, CM_MAXLEFTVIEW, LoadStr(bHoriz ? IDS_MAXTOPVIEW : IDS_MAXLEFTVIEW));
            ModifyMenu(hMenu, CM_MAXRIGHTVIEW, MF_BYCOMMAND | MF_STRING, CM_MAXRIGHTVIEW, LoadStr(bHoriz ? IDS_MAXBOTTOMVIEW : IDS_MAXRIGHTVIEW));
            LayoutChilds();
            return 0;
        }

        case CM_CANCEL:
        {
            HWND focus = GetFocus();
            if ((focus == ComboBox->HWindow ||
                 focus == GetWindow(ComboBox->HWindow, GW_CHILD)) &&
                DataValid)
            {
                SetFocus(FileView[Active]->HWindow);
                return 0;
            }
        }
            // jedeme dal ...
        case CM_EXIT:
            if (!IsWindowEnabled(HWindow))
            { // zavreme postupne vsechny dialogy nad timto dialogem (posleme jim WM_CLOSE, a pak sem posleme znovu CM_EXIT)
                SG->CloseAllOwnedEnabledDialogs(HWindow);
                SetTimer(HWindow, 666, 100, NULL);
                return 0;
            }

            DestroyWindow(HWindow);
            return 0;

        case CM_COPY:
        {
            if (DataValid)
            {
                FileView[Active]->CopySelection();
            }
            return 0;
        }

        case CM_ALTUP:
        {
            if (!DataValid)
                return 0;
            HWND focus = GetFocus();
            if (focus == ComboBox->HWindow || focus == GetWindow(ComboBox->HWindow, GW_CHILD))
            {
                BOOL state = SendMessage(ComboBox->HWindow, CB_GETDROPPEDSTATE, 0, 0) != 0;
                SendMessage(ComboBox->HWindow, CB_SHOWDROPDOWN, !state, 0);
                return 0;
            }
            UINT state = GetMenuState(GetMenu(HWindow), CM_PREVDIFF, MF_BYCOMMAND);
            if (state & (MF_DISABLED | MF_GRAYED))
            {
                if (DifferencesCount > 0) // Petr: kdyz odroluju view kamsi pryc, chci na Alt+sipku focus difference (hledat ji rucne je oser), kdyz je jen jedna neslo to vubec, jinak to slo pres prepnuti dalsi/predchozi
                    SelectDifference(SelectedDifference, CM_PREVDIFF);
                return 0;
            }
        }
            // jedeme dal

        case CM_PREVDIFF:
            if (DataValid)
                SelectDifference(max(SelectedDifference - 1, 0), CM_PREVDIFF);
            return 0;

        case CM_FIRSTDIFF:
            if (DataValid)
                SelectDifference(0, CM_FIRSTDIFF);
            return 0;

        case CM_ALTDOWN:
        {
            if (!DataValid)
                return 0;
            HWND focus = GetFocus();
            if (focus == ComboBox->HWindow || focus == GetWindow(ComboBox->HWindow, GW_CHILD))
            {
                BOOL state = SendMessage(ComboBox->HWindow, CB_GETDROPPEDSTATE, 0, 0) != 0;
                SendMessage(ComboBox->HWindow, CB_SHOWDROPDOWN, !state, 0);
                return 0;
            }
            UINT state = GetMenuState(GetMenu(HWindow), CM_NEXTDIFF, MF_BYCOMMAND);
            if (state & (MF_DISABLED | MF_GRAYED))
            {
                if (DifferencesCount > 0) // Petr: kdyz odroluju view kamsi pryc, chci na Alt+sipku focus difference (hledat ji rucne je oser), kdyz je jen jedna neslo to vubec, jinak to slo pres prepnuti predchozi/dalsi
                    SelectDifference(SelectedDifference, CM_NEXTDIFF);
                return 0;
            }
        }
            // jedeme dal

        case CM_NEXTDIFF:
            if (DataValid)
                SelectDifference(SelectedDifference + 1, CM_NEXTDIFF); // test na DifferencesCount ohlidame pozdeji
            return 0;

        case CM_LASTDIFF:
            if (DataValid)
                SelectDifference(-1, CM_LASTDIFF);
            return 0;

        case CM_COMPARE:
        {
            char path1[MAX_PATH];
            char path2[MAX_PATH];
            if (DataValid)
            {
                strcpy(path1, Path1);
                strcpy(path2, Path2);
            }
            else
            {
                *path1 = 0;
                *path2 = 0;
            }
            BOOL succes = FALSE;
            CCompareOptions options = DefCompareOptions;
            CCompareFilesDialog dlg(HWindow, path1, path2, succes, &options);
            dlg.Execute();
            if (succes)
                SpawnWorker(path1, path2, FALSE, options);
            return 0;
        }

        case CM_RECOMPARE:
        {
            if (DataValid || FirstCompare)
            {
                if (OptionsChanged)
                {
                    // zeptame, zda chce pouzit nove volby
                    int ret = SG->SalMessageBox(HWindow, LoadStr(IDS_OPTIONSCHANGED), LoadStr(IDS_PLUGINNAME),
                                                MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
                    switch (ret)
                    {
                    case IDYES:
                    {
                        OptionsChanged = FALSE;
                        Options = DefCompareOptions;
                        break;
                    }
                    case IDNO:
                        OptionsChanged = FALSE;
                        break;
                    default:
                        return 0;
                    }
                }

                // pustime workera
                SpawnWorker(Path1, Path2, TRUE, Options);
            }
            return 0;
        }

        case CM_ONLYDIFFERENCES:
        {
            if (DataValid)
            {
                if (ViewMode == fvmStandard)
                    ViewMode = fvmOnlyDifferences;
                else
                    ViewMode = fvmStandard;
                ::Configuration.ViewMode = ViewMode;
                CheckMenuItem(GetMenu(HWindow), CM_ONLYDIFFERENCES, MF_BYCOMMAND | (ViewMode == fvmStandard ? MF_UNCHECKED : MF_CHECKED));

                // postavime znova skripty
                SetWait(TRUE);
                BOOL cancel;
                if (!RebuildFileViewScripts(cancel))
                {
                    if (cancel)
                    {
                        SG->SalMessageBox(HWindow, LoadStr(IDS_CANCELED), LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION);
                    }
                    SendMessage(ComboBox->HWindow, CB_RESETCONTENT, 0, 0);
                    SetWindowText(HWindow, LoadStr(IDS_PLUGINNAME));
                }
                SetWait(FALSE);
            }

            return 0;
        }

        case CM_SHOWWHITESPACE:
        {
            ::Configuration.ShowWhiteSpace = ShowWhiteSpace = !ShowWhiteSpace;
            CheckMenuItem(GetMenu(HWindow), CM_SHOWWHITESPACE, MF_BYCOMMAND | (ShowWhiteSpace ? MF_CHECKED : MF_UNCHECKED));

            if (DataValid && FileView[fviLeft]->Is(fvtText))
            {
                ((CTextFileViewWindowBase*)FileView[fviLeft])->SetShowWhiteSpace(ShowWhiteSpace);
                ((CTextFileViewWindowBase*)FileView[fviRight])->SetShowWhiteSpace(ShowWhiteSpace);
            }

            // prekreslime obrazovku
            InvalidateRect(LeftFileViewHWnd, NULL, FALSE);
            UpdateWindow(LeftFileViewHWnd);
            InvalidateRect(RightFileViewHWnd, NULL, FALSE);
            UpdateWindow(RightFileViewHWnd);

            return 0;
        }

        case CM_SHOWCARET:
        {
            if (DataValid && FileView[fviLeft]->Is(fvtText))
            {
                ShowCaret = !ShowCaret;
                CheckMenuItem(GetMenu(HWindow), CM_SHOWCARET, MF_BYCOMMAND | (ShowCaret ? MF_CHECKED : MF_UNCHECKED));
                ((CTextFileViewWindowBase*)FileView[fviLeft])->SetCaret(ShowCaret);
                ((CTextFileViewWindowBase*)FileView[fviRight])->SetCaret(ShowCaret);
            }

            return 0;
        }

        case CM_DETAILDIFF:
        {
            /*::Configuration.DetailedDifferences =*/DetailedDifferences = !DetailedDifferences;
            if (DataValid && FileView[fviLeft]->Is(fvtText))
            {
                if (DetailedDifferences)
                {
                    if (Options.DetailedDifferences)
                    {
                        ((CTextFileViewWindowBase*)FileView[fviLeft])->SetDetailedDifferences(TRUE);
                        ((CTextFileViewWindowBase*)FileView[fviRight])->SetDetailedDifferences(TRUE);
                    }
                    else
                    {
                        // let worker calculate it again with detailed differences
                        PostMessage(HWindow, WM_COMMAND, CM_RECOMPARE, 0);
                    }
                }
                else
                {
                    ((CTextFileViewWindowBase*)FileView[fviLeft])->SetDetailedDifferences(FALSE);
                    ((CTextFileViewWindowBase*)FileView[fviRight])->SetDetailedDifferences(FALSE);
                }

                CheckMenuItem(GetMenu(HWindow), CM_DETAILDIFF, MF_BYCOMMAND | (DetailedDifferences ? MF_CHECKED : MF_UNCHECKED));

                // prekreslime obrazovku
                InvalidateRect(LeftFileViewHWnd, NULL, FALSE);
                UpdateWindow(LeftFileViewHWnd);
                InvalidateRect(RightFileViewHWnd, NULL, FALSE);
                UpdateWindow(RightFileViewHWnd);
            }

            return 0;
        }

        case CM_OPTIONS:
        {
            DWORD flag;
            CConfigurationDialog dlg(HWindow, &Configuration, Colors, &DefCompareOptions, &flag);
            dlg.Execute();
            if (flag)
                MainWindowQueue.BroadcastMessage(WM_USER_CFGCHNG, flag, 0);
            return 0;
        }

        case CM_AUTOCOPY:
        {
            Configuration.AutoCopy = !Configuration.AutoCopy;
            MainWindowQueue.BroadcastMessage(WM_USER_AUTOCOPY_CHANGED, 0, 0);
            return 0;
        }

        case CM_CBACTIVATE:
        {
            HWND focus = GetFocus();
            if (focus != ComboBox->HWindow && focus != GetWindow(ComboBox->HWindow, GW_CHILD))
            {
                SetFocus(ComboBox->HWindow);
            }
            return 0;
        }

        case CM_SWITCHWINDOW:
        {
            if (DataValid && InputEnabled && GetFocus() == FileView[Active]->HWindow &&
                FileView[Active]->Is(fvtText))
            {
                CFileViewWindow* siblink = FileView[Active]->GetSiblink();
                if (siblink && ShowCaret)
                {
                    int y;
                    int x = ((CTextFileViewWindowBase*)FileView[Active])->GetCaretPos(y);
                    ((CTextFileViewWindowBase*)siblink)->SetCaretPos(x, y);
                }
                SetFocus(siblink->HWindow);
            }
            return 0;
        }

        case CM_CLOSEFILES:
        {
            DataValid = FALSE;
            DifferencesCount = 0;

            FileView[fviLeft]->InvalidateData();
            FileView[fviRight]->InvalidateData();

            LinesToChanges.clear();
            ChangesToLines[0].clear();
            ChangesToLines[1].clear();
            ChangesLengths.clear();
            TextChanges.clear();

            LeftHeader->SetText("");
            RightHeader->SetText("");

            ResetComboBox();

            SetWindowText(HWindow, LoadStr(IDS_PLUGINNAME));

            UpdateToolbarButtons(UTB_ALL);

            if (SG->SalMessageBox(HWindow, LoadStr(IDS_CLOSEDIFF), LoadStr(IDS_PLUGINNAME),
                                  MB_YESNOCANCEL | MB_ICONQUESTION) == IDYES)
            {
                PostMessage(HWindow, WM_COMMAND, CM_EXIT, 0);
            }

            return 0;
        }

        case CM_HELP:
        {
            SG->OpenHtmlHelp(HWindow, HHCDisplayContext, IDH_INTRODUCTION, TRUE);
            SG->OpenHtmlHelp(HWindow, HHCDisplayTOC, 0, FALSE); // nechceme dva messageboxy za sebou
            return 0;
        }

        case CM_HELPKEYBOARD:
        {
            SG->OpenHtmlHelp(HWindow, HHCDisplayContext, IDH_KEYBOARD, FALSE);
            return 0;
        }

        case CM_ABOUT:
        {
            PluginInterface.About(HWindow);
        }

        case IDC_DIFFLIST:
        {
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                LRESULT i = SendMessage(ComboBox->HWindow, CB_GETCURSEL, 0, 0);
                OutOfRange = FALSE;
                if (i != CB_ERR && DataValid)
                    SelectDifference(int(i), 0, FALSE);
                return 0;
            }
            break;
        }

        case CM_MAXLEFTVIEW:
        {
            if (SplitProp == 1)
            {
                SplitProp = PrevSplitProp;
            }
            else
            {
                if (SplitProp > 0)
                    PrevSplitProp = SplitProp;
                SplitProp = 1;
            }
            LayoutChilds();
            break;
        }

        case CM_MAXRIGHTVIEW:
        {
            if (SplitProp == 0)
            {
                SplitProp = PrevSplitProp;
            }
            else
            {
                if (SplitProp < 1)
                    PrevSplitProp = SplitProp;
                SplitProp = 0;
            }
            LayoutChilds();
            break;
        }

        case CM_CYCLEVIEW: // left->right->split->left->...
        {
            if (SplitProp == 1)
            {
                SplitProp = 0;
            }
            else if (SplitProp == 0)
            {
                SplitProp = PrevSplitProp;
            }
            else
            {
                PrevSplitProp = SplitProp;
                SplitProp = 1;
            }
            LayoutChilds();
            break;
        }

        case CM_ANTICYCLEVIEW: // right->left->split->right->...
        {
            if (SplitProp == 0)
            {
                SplitProp = 1;
            }
            else if (SplitProp == 1)
            {
                SplitProp = PrevSplitProp;
            }
            else
            {
                PrevSplitProp = SplitProp;
                SplitProp = 0;
            }
            LayoutChilds();
            break;
        }

        case CM_SAMESIZEVIEW:
        {
            SplitProp = 0.5;
            LayoutChilds();
            break;
        }
        }
        break;
    }

    case WM_DROPFILES:
    {
        HDROP drop = (HDROP)wParam;
        if (InputEnabled)
        {
            POINT pt;
            int count, view = -1;
            char path1[MAX_PATH], path2[MAX_PATH];
            CCompareOptions options = DefCompareOptions;

            if (DragQueryPoint(drop, &pt))
            {
                HWND child = ChildWindowFromPoint(HWindow, pt);
                if (child == LeftHeader->HWindow || child == LeftFileViewHWnd)
                    view = 0;
                if (child == RightHeader->HWindow || child == RightFileViewHWnd)
                    view = 1;
            }

            // kolik souboru nam hodili
            count = DragQueryFile(drop, 0xFFFFFFFF, NULL, 0);
            if (count < 1)
                goto LDROPERROR;
            if (count >= 1)
            {
                DragQueryFile(drop, 0, path1, MAX_PATH);
                if (SG->SalGetFileAttributes(path1) & FILE_ATTRIBUTE_DIRECTORY)
                {
                    Error(HWindow, IDS_NOTVALIDFILE, path1);
                    goto LDROPERROR;
                }
            }
            if (count >= 2)
            {
                DragQueryFile(drop, 1, path2, MAX_PATH);
                if (SG->SalGetFileAttributes(path1) & FILE_ATTRIBUTE_DIRECTORY)
                {
                    Error(HWindow, IDS_NOTVALIDFILE, path2);
                    goto LDROPERROR;
                }
            }
            else
            {
                if (DataValid && view >= 0)
                {
                    if (view == 0)
                        strcpy(path2, Path2);
                    else
                    {
                        strcpy(path2, path1);
                        strcpy(path1, Path1);
                    }
                    // operace je podobna recompare tak pouzijeme aktuali nastaveni,
                    // pokud ovsem uzivatem mezitim nezmenil defaultni nastaveni v
                    // konfiguraci, aby ho nematlo, ze neporovnava podle
                    // nokonfigurovanych hodnot
                    if (!OptionsChanged)
                        options = Options;
                }
                else
                    *path2 = 0;
            }

            BOOL succes = FALSE;
            if (count > 2 || *path2 == 0)
            {
                // nechame potvrdit uzivatelem
                CCompareFilesDialog dlg(HWindow, path1, path2, succes, &options);
                dlg.Execute();
            }
            else
                succes = TRUE;

            if (succes)
                SpawnWorker(path1, path2, FALSE, options);
        }

    LDROPERROR:

        DragFinish(drop);
        return 0;
    }

    case WM_HELP:
    {
        SG->OpenHtmlHelp(HWindow, HHCDisplayContext, IDH_INTRODUCTION, FALSE);
        SG->OpenHtmlHelp(HWindow, HHCDisplayTOC, 0, TRUE); // nechceme dva messageboxy za sebou
        return TRUE;
    }

    case WM_NOTIFY:
    {

        LPNMHDR pnmh = (LPNMHDR)lParam;
        switch (wParam)
        {
        case IDC_REBAR:
        {
            switch (pnmh->code)
            {
            case RBN_HEIGHTCHANGE:
            {
                RebarHeight = LONG(SendMessage(Rebar->HWindow, RB_GETBARHEIGHT, 0, 0)) + 4 + REBAR_BORDER;
                if (Initialized)
                    LayoutChilds();
                return 0;
            }
            }
            break;
        }

        case CM_COPY:
        case CM_RECOMPARE:
        case CM_FIRSTDIFF:
        case CM_PREVDIFF:
        case CM_NEXTDIFF:
        case CM_LASTDIFF:
        {
            switch (pnmh->code)
            {
            case TTN_GETDISPINFO:
            {
                LPNMTTDISPINFO lpnmtdi = (LPNMTTDISPINFO)lParam;

                switch (wParam)
                {
                case CM_COPY:
                    lpnmtdi->lpszText = LoadStr(IDS_COPYCLIP);
                    break;
                case CM_RECOMPARE:
                    lpnmtdi->lpszText = LoadStr(IDS_RECOMPARE);
                    break;
                case CM_FIRSTDIFF:
                    lpnmtdi->lpszText = LoadStr(IDS_FIRSTDIFF);
                    break;
                case CM_PREVDIFF:
                    lpnmtdi->lpszText = LoadStr(IDS_PREVDIFF);
                    break;
                case CM_NEXTDIFF:
                    lpnmtdi->lpszText = LoadStr(IDS_NEXTDIFF);
                    break;
                case CM_LASTDIFF:
                    lpnmtdi->lpszText = LoadStr(IDS_LASTDIFF);
                    break;
                }

                return 0;
            }
            }
            break;
        }
        }
        break;
    }

    case WM_SYSCHAR:
    {
        // aby nam system nepipal, kdyz se prepina na kombac
        if ((wParam == 'd' || wParam == 'D') && InputEnabled)
            return 0;
        break;
    }

    case WM_CLOSE:
    {
        PostMessage(HWindow, WM_COMMAND, CM_EXIT, 0);
        return 0;
    }

    case WM_SYSCOLORCHANGE:
    {
        UpdateDefaultColors(Colors, Palette);
        ComboBox->ChangeColors();
        if (DataValid)
        {
            FileView[fviLeft]->ReloadConfiguration(CC_COLORS, TRUE);
            FileView[fviRight]->ReloadConfiguration(CC_COLORS, TRUE);
        }
        SendMessage(Rebar->HWindow, uMsg, wParam, lParam);
        if (UsePalette)
        {
            PostMessage(HWindow, WM_QUERYNEWPALETTE, 0, 0);
        }
        break;
    }

    case WM_SETTINGCHANGE:
    {
        // mohli se zmenit rozmery scrollbar, zajistime, aby si kombac
        // napocital nove rozmery tlacitka
        RECT r;
        GetWindowRect(ComboBox->HWindow, &r);
        SetWindowPos(ComboBox->HWindow, 0, 0, 0,
                     r.right - r.left - 1, r.bottom - r.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE |
                         SWP_NOREDRAW);
        SetWindowPos(ComboBox->HWindow, 0, 0, 0,
                     r.right - r.left, r.bottom - r.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
        break;
    }

    case WM_QUERYNEWPALETTE:
    {
        if (UsePalette)
        {
            HDC dc = GetDC(HWindow);
            HPALETTE oldPalette = SelectPalette(dc, Palette, FALSE);
            RealizePalette(dc);
            SelectPalette(dc, oldPalette, FALSE);
            ReleaseDC(HWindow, dc);
            InvalidateRect(LeftFileViewHWnd, NULL, FALSE);
            UpdateWindow(LeftFileViewHWnd);
            InvalidateRect(RightFileViewHWnd, NULL, FALSE);
            UpdateWindow(RightFileViewHWnd);
            return TRUE;
        }
        return FALSE;
    }

    case WM_PALETTECHANGED:
    {
        if (UsePalette && (HWND)wParam != HWindow)
        {
            HDC dc = GetDC(HWindow);
            HPALETTE oldPalette = SelectPalette(dc, Palette, FALSE);
            RealizePalette(dc);
            SelectPalette(dc, oldPalette, FALSE);
            ReleaseDC(HWindow, dc);

            InvalidateRect(LeftFileViewHWnd, NULL, FALSE);
            UpdateWindow(LeftFileViewHWnd);
            InvalidateRect(RightFileViewHWnd, NULL, FALSE);
            UpdateWindow(RightFileViewHWnd);
        }
        return 0;
    }

    case WM_SETCURSOR:
    {
        if (Wait)
        {
            SetCursor(HWaitCursor);
            return TRUE;
        }
        else
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(HWindow, &pt);

            HWND child = ChildWindowFromPoint(HWindow, pt);
            if (child)
            {
                if (SendMessage(child, WM_USER_SETCURSOR, wParam, lParam))
                    return TRUE;
            }
        }
        break;
    }

    case WM_USER_WORKERNOTIFIES:
    {
        BOOL ret = TRUE;
        TCHAR message[1024];
        *message = 0;
        UINT type = MB_ICONERROR;
        LPCTSTR encoding[2] = {_T(""), _T("")};
        switch (wParam)
        {
        case WN_ERROR:
            _tcscpy(message, (LPCTSTR)lParam);
            break;

        case WN_WORKER_CANCELED:
        {
            TRACE_I("WN_WORKER_CANCELED");
            switch (CancelWorker)
            {
            case CW_SILENT:
                return 0;
            case CW_EXIT:
                PostMessage(HWindow, WM_COMMAND, CM_EXIT, 0);
                break;
            default:
            {
                if (DataValid)
                {
                    if (!Options.DetailedDifferences)
                    {
                        DetailedDifferences = FALSE;
                        CheckMenuItem(GetMenu(HWindow), CM_DETAILDIFF, MF_BYCOMMAND | (DetailedDifferences ? MF_CHECKED : MF_UNCHECKED));
                    }
                }

                if ((FirstCompare || !Recompare) && CalculatingDetailedDifferences)
                {
                    if (SG->SalMessageBox(HWindow, LoadStr(IDS_DETAILCANCEL),
                                          LoadStr(IDS_PLUGINNAME), MB_YESNOCANCEL) == IDYES)
                    {
                        // turn off DetailedDifferences
                        DetailedDifferences = FALSE;
                        CheckMenuItem(GetMenu(HWindow), CM_DETAILDIFF, MF_BYCOMMAND | (DetailedDifferences ? MF_CHECKED : MF_UNCHECKED));
                        // and compare again
                        SpawnWorker(Path1, Path2, FALSE, Options);
                        return 0;
                    }
                }

                _tcscpy(message, LoadStr(IDS_CANCELED));
                type = MB_ICONEXCLAMATION;
            }
            }
            break;
        }

        case WN_NO_DIFFERENCE:
            // NOTE: previously sent WN_TEXT_FILES_DIFFER did provide encoding...
            _tcscpy(message, LoadStr(IDS_NODIFFERRENCE));
            type = MB_ICONINFORMATION;
            break;

        case WN_NO_ALL_DIFFS_IGNORED:
            // NOTE: previously sent WN_TEXT_FILES_DIFFER did provide encoding...
            _tcscpy(message, LoadStr(IDS_ALLDIFFSIGNORED));
            type = MB_ICONINFORMATION;
            break;

        case WN_BINARY_FILES_DIFFER:
        {
            CBinaryCompareResults* res = (CBinaryCompareResults*)lParam;

            DataValid = FALSE;

            // file view data musi byt zrusena drive
            if (FileView[fviLeft])
                FileView[fviLeft]->DestroyData();
            if (FileView[fviRight])
                FileView[fviRight]->DestroyData();

            LinesToChanges.clear();
            ChangesToLines[0].clear();
            ChangesToLines[1].clear();
            ChangesLengths.clear();
            TextChanges.clear();
            Changes.clear();

            // zajistime spravny typ file view controlu
            if (FileView[fviLeft] && !FileView[fviLeft]->Is(fvtHex))
            {
                FileView[fviLeft]->DetachWindow();
                delete FileView[fviLeft];
                FileView[fviLeft] = NULL;
            }
            if (!FileView[fviLeft])
            {
                FileView[fviLeft] = new CHexFileViewWindow(fviLeft);
                if (!FileView[fviLeft])
                {
                    ret = FALSE; // nebudeme deallokovat buffery predane v CDiffResults
                    _tcscpy(message, LoadStr(IDS_LOWMEM));
                    break;
                }
                FileView[fviLeft]->AttachToWindow(LeftFileViewHWnd);
                // zajistime incializaci okna
                SendMessage(LeftFileViewHWnd, WM_USER_CREATE, 0, 0);
            }

            if (FileView[fviRight] && !FileView[fviRight]->Is(fvtHex))
            {
                FileView[fviRight]->DetachWindow();
                delete FileView[fviRight];
                FileView[fviRight] = NULL;
            }
            if (!FileView[fviRight])
            {
                FileView[fviRight] = new CHexFileViewWindow(fviRight);
                if (!FileView[fviRight])
                {
                    ret = FALSE; // nebudeme deallokovat buffery predane v CDiffResults
                    _tcscpy(message, LoadStr(IDS_LOWMEM));
                    break;
                }
                FileView[fviRight]->AttachToWindow(RightFileViewHWnd);
                // zajistime incializaci okna
                SendMessage(RightFileViewHWnd, WM_USER_CREATE, 0, 0);
            }

            FileView[fviLeft]->SetSiblink(FileView[fviRight]);
            FileView[fviRight]->SetSiblink(FileView[fviLeft]);

            DifferencesCount = 0;

            if (
                ((CHexFileViewWindow*)FileView[fviLeft])->SetData(res->FirstChange, res->Files[0].Name, res->Files[1].Size) &&
                ((CHexFileViewWindow*)FileView[fviRight])->SetData(res->FirstChange, res->Files[1].Name, res->Files[0].Size))
            {
                Options = res->Options;
                strcpy(Path1, res->Files[0].Name);
                strcpy(Path2, res->Files[1].Name);
                LeftHeader->SetText(Path1);
                RightHeader->SetText(Path2);

                ResetComboBox();

                // zajistime prekresleni vsech oken
                InvalidateRect(LeftFileViewHWnd, NULL, FALSE);
                UpdateWindow(LeftFileViewHWnd);
                InvalidateRect(RightFileViewHWnd, NULL, FALSE);
                UpdateWindow(RightFileViewHWnd);
                ShowWindow(HWindow, SW_SHOW);
                SetForegroundWindow(HWindow);
                UpdateWindow(HWindow);

                OutOfRange = FALSE;

                DataValid = TRUE;

                if (res->FirstChange != (QWORD)-1) // select 1st difference if there are any
                    SelectDifference(0, CM_FIRSTDIFF);
            }
            else
            {
                LeftHeader->SetText("");
                RightHeader->SetText("");
                FileView[fviLeft]->InvalidateData(); // zajistime prekresleni oken
                FileView[fviRight]->InvalidateData();
                ret = FALSE; // nebudeme hledat difference
            }

            break;
        }

        case WN_SETCHANGES:
        {
            // tady to schvalne kopirujem, nevolame swap
            Changes = *(CBinaryChanges*)lParam;
            if (Changes.size() < MaxBinChanges)
                DifferencesCount = int(Changes.size());
            else
                DifferencesCount = MaxBinChanges;
            //ResetComboBox();
            return (LRESULT)ComboBox->HWindow;
        }

        case WN_GET_CHANGESCOMBO:
            DifferencesCount++;
            OutOfRange = FALSE;
            if ((DifferencesCount < 3) || (SelectedDifference >= DifferencesCount - 2))
                UpdateToolbarButtons(UTB_DIFFSSELECTION);
            return (LRESULT)ComboBox->HWindow;

        case WN_ADD_CHANGE:
        {
            CBinaryChange* change = (CBinaryChange*)lParam;
            if (SelectedDifference == -1)
                SelectedDifference++;                         // Hack to select the first (and so far the only) difference
            if ((size_t)SelectedDifference >= Changes.size()) // NOTE: SelectedDifference can be -1, but size_t is unsigned
                UpdateToolbarButtons(UTB_DIFFSSELECTION);
            Changes.push_back(*change);
            return 0;
        }

        case WN_SET_PROGRESS:
        {
            TCHAR buf[MAX_PATH * 2 + 400], fmt[128];
            if (HIWORD(lParam))
            {
                CQuadWord qLPARAM(LOWORD(lParam), 0);
                SG->ExpandPluralString(fmt, sizeof(fmt), LoadStr(IDS_MAINWNDHEADERCOMPUTING_PROGRESS_FOUND), 1, &qLPARAM);
            }
            else
            {
                _tcscpy(fmt, LoadStr(IDS_MAINWNDHEADERCOMPUTING_PROGRESS));
            }
            _stprintf(buf, fmt, SG->SalPathFindFileName(Path1), SG->SalPathFindFileName(Path2),
                      LOWORD(lParam), HIWORD(lParam));
            SetWindowText(HWindow, buf);
            return 0;
        }

        case WN_CBINIT_FINISHED:
        {
            if (DataValid)
            {
                QWORD offs = ((CHexFileViewWindow*)FileView[fviLeft])->GetFocusedDifference(NULL);
                if (offs != -1)
                {
                    int i = FindDifference(offs);
                    if (i != -1)
                    {
                        OutOfRange = FALSE;
                        SelectDifference(i);
                    }
                }
            }
            return 0;
        }

        case WN_TEXT_FILES_DIFFER:
        {
            _CrtCheckMemory();
            // Patera 2009.05.23: Set to 0 to avoid GPF if files become equal after recompare
            DifferencesCount = 0;
            ret = TextFilesDiffer((CTextCompareResults<char>*)lParam, message, type, encoding);
            _CrtCheckMemory();
            break;
        }

        case WN_UNICODE_FILES_DIFFER:
        {
            // Patera 2009.05.23: Set to 0 to avoid GPF if files become equal after recompare
            DifferencesCount = 0;
            ret = TextFilesDiffer((CTextCompareResults<wchar_t>*)lParam, message, type, encoding);
            break;
        }

        case WN_CALCULATING_DETAILS:
        {
            CalculatingDetailedDifferences = TRUE;
            return 0;
        }

        case WN_COMPARE_FINISHED:
        {
            FirstCompare = FALSE;
            return 0;
        }
        }

        if (*message)
        {
            if (FirstCompare)
            {
                type |= MB_YESNO | MSGBOXEX_ESCAPEENABLED;
                _tcscat(message, _T("\n"));
                _tcscat(message, LoadStr(IDS_CLOSEDIFF));
            }
            if (SG->SalMessageBox(HWindow, message, LoadStr(IDS_PLUGINNAME), type) == IDYES)
                PostMessage(HWindow, WM_COMMAND, CM_EXIT, 0);
        }

        TCHAR buf[MAX_PATH * 2 + 400];
        if (DataValid)
        {
            //        if (DifferencesCount || (WN_NO_DIFFERENCE == wParam))
            //        { // je-li 0, nastavime caption pozdeji, az najdeme vsechny binarni difference
            TCHAR fmt[128];
            if (DifferencesCount)
            {
                CQuadWord qDC(DifferencesCount, 0);
                SG->ExpandPluralString(fmt, SizeOf(fmt), LoadStr(IDS_MAINWNDHEADER), 1, &qDC);
            }
            else
                _tcscpy(fmt, LoadStr((WN_NO_DIFFERENCE == wParam) ? IDS_MAINWNDHEADER_NODIF : IDS_MAINWNDHEADERCOMPUTING2));
            _stprintf(buf, fmt, SG->SalPathFindFileName(Path1), encoding[0], SG->SalPathFindFileName(Path2), encoding[1], DifferencesCount);
            //        }
            //        else
            //        {
            //          _stprintf(buf, LoadStr(IDS_MAINWNDHEADERCOMPUTING2),
            //            SG->SalPathFindFileName(Path1), SG->SalPathFindFileName(Path2));
            //        }
        }
        else
            _tcscpy(buf, LoadStr(IDS_PLUGINNAME));

        SetWindowText(HWindow, buf);

        if ((wParam != WN_TEXT_FILES_DIFFER) && (wParam != WN_UNICODE_FILES_DIFFER) && (wParam != WN_BINARY_FILES_DIFFER))
        {
            FirstCompare = FALSE;
        }

        if (!DataValid)
        {
            EnableScrollBar(LeftFileViewHWnd, SB_BOTH, ESB_DISABLE_BOTH);
            EnableScrollBar(RightFileViewHWnd, SB_BOTH, ESB_DISABLE_BOTH);
        }

        EnableInput(TRUE);

        SetWait(FALSE);

        return ret;
    }

    case WM_USER_CFGCHNG:
    {
        if (wParam & CC_COLORS)
        {
            UpdateDefaultColors(Colors, Palette);
            if (UsePalette)
            {
                HDC dc = GetDC(HWindow);
                HPALETTE oldPalette = SelectPalette(dc, Palette, FALSE);
                RealizePalette(dc);
                SelectPalette(dc, oldPalette, FALSE);
                ReleaseDC(HWindow, dc);
            }
        }

        if (DataValid)
        {
            BOOL recompare = FALSE;

            if ((wParam & CC_DEFOPTIONS) &&
                ((wParam & CC_HAVEHWND) == 0 || (HWND)lParam != HWindow))
            {
                // Avoid several dlgs when multiple windows are opened and def opts are changed several times
                if (bOptionsChangedBeingHandled)
                    return 0;

                bOptionsChangedBeingHandled = TRUE;
                int ret = SG->SalMessageBox(HWindow, LoadStr(IDS_OPTIONSCHANGED), LoadStr(IDS_PLUGINNAME),
                                            MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION);
                bOptionsChangedBeingHandled = FALSE;
                if (ret == IDYES)
                {
                    recompare = TRUE;
                    Options = DefCompareOptions;
                    PostMessage(HWindow, WM_COMMAND, CM_RECOMPARE, 0);
                }
                else if (ret <= 0)
                {
                    return 0; // AS is being closed, this has been freed!!!!
                }
                else
                {
                    OptionsChanged = TRUE; // aby pri nasledujicim recompare, zeptal na naloadeni novych options
                }
            }

            if (!recompare)
            {
                if ((wParam & CC_CONTEXT) | (wParam & CC_TABSIZE))
                {
                    // postavime znova skripty
                    if (FileView[fviLeft]->Is(fvtText))
                    {
                        SetWait(TRUE);
                        BOOL cancel;
                        if (!RebuildFileViewScripts(cancel))
                        {
                            if (cancel)
                            {
                                SG->SalMessageBox(HWindow, LoadStr(IDS_CANCELED), LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION);
                            }
                            SendMessage(ComboBox->HWindow, CB_RESETCONTENT, 0, 0);
                            SetWindowText(HWindow, LoadStr(IDS_PLUGINNAME));
                        }
                        SetWait(FALSE);
                    }
                }
            }

            FileView[fviLeft]->ReloadConfiguration(DWORD(wParam), TRUE);
            FileView[fviRight]->ReloadConfiguration(DWORD(wParam), TRUE);
        }
        return 0;
    }

    case WM_USER_SLITPOSCHANGED:
    {
        SplitProp = *(double*)wParam;
        LayoutChilds();
        return 0;
    }

    case WM_USER_GETREBARHEIGHT:
        return RebarHeight;

    case WM_USER_GETHEADERHEIGHT:
        return HeaderHeight;

    case WM_USER_ACTIVATEWINDOW:
    {
        // pokud je okno disabled, pravdepodobne je nad nim zobrazen msgbox a vytazeni
        // okna nahoru by zpusobilo jeho aktivaci (to byl problem v jedne starsi verzi SS,
        // kdyz uzivatel nechal porovnat dva shodne soubory -- zobrazilo se toto okno,
        // nasledne vyskocil msgbox s informaci o shode a nasledne se dorucila tato
        // zprava, ktera msgboxu ukradla focus a ktivovala okno pod nim)
        if (IsWindowEnabled(HWindow))
        {
            ShowWindow(HWindow, ShowCmd /*SW_SHOW*/);
            SetForegroundWindow(HWindow);
        }
        UpdateWindow(HWindow);
        return 0;
    }

    case WM_USER_AUTOCOPY_CHANGED:
        CheckMenuItem(GetMenu(HWindow), CM_AUTOCOPY, Configuration.AutoCopy ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
        break;

    case WM_DESTROY:
    {
        DragAcceptFiles(HWindow, FALSE);

        // zrusime pole drive nez okno, kdyz je pole velike, okno se zavre, ale thread zije dal
        // (probiha v nem destrukce prvku pole), to vede k tomu, ze je thread zakillovan pri
        // releasu pluginu drive nez sam zkonci
        if (FileView[fviLeft])
            FileView[fviLeft]->DestroyData();
        if (FileView[fviRight])
            FileView[fviRight]->DestroyData();

        // uvolnime velike struktury nyni, viz poznamka vyse
        LinesToChanges.clear();
        ChangesToLines[0].clear();
        ChangesToLines[1].clear();
        ChangesLengths.clear();
        TextChanges.clear();

        SaveRebarLayout();

        // v pripade, ze jeste bezi worker, ukoncime ho
        CancelWorker = CW_SILENT;
        TRACE_I("Waiting 'til worker finished");
        while (1)
        {
            if (MsgWaitForMultipleObjects(1, &WorkerEvent, FALSE, INFINITE, QS_ALLEVENTS | QS_SENDMESSAGE) == WAIT_OBJECT_0)
                break;
            TRACE_I("HaveMessage");
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (!TranslateAccelerator(HWindow, HAccels, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
        TRACE_I("Worker has finished");
        PostQuitMessage(0);
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
