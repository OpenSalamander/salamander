// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

int DialogWidth;
int DialogHeight;
BOOL Maximized;

//*********************************************************************************
//
// CFindDialog
//

CFindDialog::CFindDialog(LPWSTR lookInInit)
    : CDialogEx(IDD_SEARCH, IDD_SEARCH, NULL, SG->GetMainWindowHWND()),
      LookInList(4, 4)
{
    CALL_STACK_MESSAGE1("CFindDialog::CFindDialog()");
    LookInInit = lookInInit;
    MinDlgW = 0;
    MinDlgH = 0;
    HMargin = 0;
    VMargin = 0;
    CombosH = 0;
    CombosX = 0;
    FindNowW = 0;
    FindNowY = 0;
    OptionsY = 0;
    OptionsW = 0;
    ResultsY = 0;
    StatusHeight = EnvFontHeight + 5;
    AddY = 0;
    FoundItemsH = 0;
    StatusBar = NULL;

    ZeroOnDestroy = NULL;

    *Pattern = L'\0';
    IncludeSubkeys = TRUE;
    LookAtKeys = TRUE;
    LookAtValues = TRUE;
    LookAtData = TRUE;
    Hex = FALSE;
    CaseSensitive = FALSE;
    WholeWords = FALSE;
    RegExp = FALSE;
    UseMinTime = UseMaxTime = FALSE;

    SearchInProgress = FALSE;
    CloseWhenSearchFinishes = FALSE;
    ShowOptions = FALSE;
}

void CFindDialog::GetLayoutParams()
{
    CALL_STACK_MESSAGE1("CFindDialog::GetLayoutParams()");
    RECT wr;
    GetWindowRect(HWindow, &wr);
    MinDlgW = wr.right - wr.left;
    MinDlgH = wr.bottom - wr.top;

    RECT cr;
    GetClientRect(HWindow, &cr);

    RECT br;
    GetWindowRect(GetDlgItem(HWindow, IDC_RESULTS), &br);
    int windowMargin = ((wr.right - wr.left) - (cr.right)) / 2;
    HMargin = br.left - wr.left - windowMargin;
    int captionH = wr.bottom - wr.top - cr.bottom - windowMargin;
    ResultsY = br.top - wr.top - captionH;

    GetWindowRect(GetDlgItem(HWindow, IDC_PATTERN), &br);
    CombosX = br.left - wr.left - windowMargin;

    GetWindowRect(GetDlgItem(HWindow, IDOK), &br);
    FindNowW = br.right - br.left;
    FindNowY = br.top - wr.top - captionH;
    VMargin = br.top - wr.top - captionH;

    GetWindowRect(GetDlgItem(HWindow, IDHELP), &br);
    AddY = br.top - wr.top - captionH;

    GetWindowRect(GetDlgItem(HWindow, IDC_OPTIONS), &br);
    OptionsY = br.top - wr.top - captionH;
    OptionsH = br.bottom - br.top;
    OptionsW = br.right - br.left;

    GetWindowRect(GetDlgItem(HWindow, IDC_FOUND_FILES), &br);
    FoundItemsH = br.bottom - br.top;

    //GetWindowRect(StatusBar->HWindow, &br);
    //StatusHeight = br.bottom - br.top;
}

void CFindDialog::LayoutControls(BOOL showOrHideControls)
{
    CALL_STACK_MESSAGE2("CFindDialog::LayoutControls(%d)", showOrHideControls);
    if (showOrHideControls)
    {
        int show = ShowOptions ? SW_SHOW : SW_HIDE;
        ShowWindow(GetDlgItem(HWindow, IDC_SUBDIRS), show);
        ShowWindow(GetDlgItem(HWindow, IDS_LOOKAT), show);
        ShowWindow(GetDlgItem(HWindow, IDC_KEYS), show);
        ShowWindow(GetDlgItem(HWindow, IDC_VALUES), show);
        ShowWindow(GetDlgItem(HWindow, IDC_DATA), show);
        ShowWindow(GetDlgItem(HWindow, IDC_HEX), show);
        ShowWindow(GetDlgItem(HWindow, IDC_CASE), show);
        ShowWindow(GetDlgItem(HWindow, IDC_WHOLEWORDS), show);
        ShowWindow(GetDlgItem(HWindow, IDC_REGEXP), show);
        ShowWindow(GetDlgItem(HWindow, IDC_MINTIME), show);
        ShowWindow(GetDlgItem(HWindow, IDC_MAXTIME), show);
        ShowWindow(GetDlgItem(HWindow, IDS_OPTIONS), show);
        ShowWindow(GetDlgItem(HWindow, IDS_NEWER), show);
        ShowWindow(GetDlgItem(HWindow, IDS_OLDER), show);
        ShowWindow(GetDlgItem(HWindow, IDS_FORMAT1), show);
        ShowWindow(GetDlgItem(HWindow, IDS_FORMAT2), show);

        if (ShowOptions && !IsIconic(HWindow) && !IsZoomed(HWindow))
        {
            RECT wr;
            GetWindowRect(HWindow, &wr);
            if (wr.bottom - wr.top < GetMinDlgH())
            {
                SetWindowPos(HWindow, NULL, 0, 0, wr.right - wr.left, GetMinDlgH(),
                             SWP_NOMOVE | SWP_NOZORDER);
            }
        }
    }

    RECT clientRect;

    if (CombosH == 0)
    {
        RECT r;
        GetWindowRect(GetDlgItem(HWindow, IDC_PATTERN), &r);
        if (r.bottom - r.top != 0)
            CombosH = r.bottom - r.top;
    }

    GetClientRect(HWindow, &clientRect);
    clientRect.bottom -= StatusHeight;

    int resultsY = ShowOptions ? ResultsY : (int)(OptionsY + OptionsH * 1.1);

    HDWP hdwp = BeginDeferWindowPos(8);
    if (hdwp != NULL)
    {
        // umistim Status Bar
        hdwp = DeferWindowPos(hdwp, StatusBar->HWindow, NULL,
                              0, clientRect.bottom, clientRect.right, StatusHeight,
                              SWP_NOZORDER);

        // natahnu combobox Search For
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_PATTERN), NULL,
                              0, 0, clientRect.right - CombosX - HMargin - FindNowW - HMargin, CombosH,
                              SWP_NOZORDER | SWP_NOMOVE);

        // umistim tlacitko Find Now
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDOK), NULL, //IDC_FIND_FINDNOW
                              clientRect.right - HMargin - FindNowW, FindNowY, 0, 0,
                              SWP_NOZORDER | SWP_NOSIZE);

        // natahnu combobox Look In
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_LOOKIN), NULL,
                              0, 0, clientRect.right - CombosX - HMargin - FindNowW - HMargin, CombosH,
                              SWP_NOZORDER | SWP_NOMOVE);

        // umistim tlacitko Add
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDHELP), NULL, //IDC_FIND_FINDNOW
                              clientRect.right - HMargin - FindNowW, AddY, 0, 0,
                              SWP_NOZORDER | SWP_NOSIZE);

        // umistim a natahnu list view
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_RESULTS), NULL,
                              HMargin, resultsY, clientRect.right - 2 * HMargin,
                              clientRect.bottom - resultsY - VMargin,
                              SWP_NOZORDER);

        // umistim a titulek nad list view
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FOUND_FILES), NULL,
                              HMargin, resultsY - FoundItemsH - 2, 0, 0,
                              SWP_NOZORDER | SWP_NOSIZE);

        // umistim checkbox Options
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_OPTIONS), NULL,
                              clientRect.right - HMargin - OptionsW, OptionsY, 0, 0,
                              SWP_NOZORDER | SWP_NOSIZE);

        EndDeferWindowPos(hdwp);
    }
}

void CFindDialog::EnableControls(BOOL enable)
{
    CALL_STACK_MESSAGE2("CFindDialog::EnableControls(%d)", enable);
    EnableWindow(GetDlgItem(HWindow, IDC_PATTERN), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_LOOKIN), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_SUBDIRS), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_KEYS), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_VALUES), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_DATA), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_HEX),
                 enable && SendDlgItemMessage(HWindow, IDC_REGEXP, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    EnableWindow(GetDlgItem(HWindow, IDC_CASE), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_WHOLEWORDS),
                 enable && SendDlgItemMessage(HWindow, IDC_HEX, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    EnableWindow(GetDlgItem(HWindow, IDC_REGEXP), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_MINTIME), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_MAXTIME), enable);

    if (enable)
        return;

    SetFocus(GetDlgItem(HWindow, IDOK));
    SendMessage(HWindow, DM_SETDEFID, IDOK, 0);
    PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
}

void CFindDialog::UpdateListViewItems()
{
    CALL_STACK_MESSAGE1("CFindDialog::UpdateListViewItems()");
    if (List != NULL)
    {
        int count = List->GetCount();

        // reknu listview novy pocet polozek
        ListView_SetItemCountEx(List->HWindow, count, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
        // pokud jde o prvni pridana data, vyberem nultou polozku vyberu
        // a nastavime fokus do listview
        if (FoundVisibleCount == 0 && count > 0)
        {
            ListView_SetItemState(List->HWindow, 0,
                                  LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED) if (GetFocus() != List->HWindow)
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)List->HWindow, TRUE);
        }

        // napisu pocet polozek nad listview
        char buff[50];
        SalPrintf(buff, 50, LoadStr(IDS_FOUNDITEMS2), count);
        SetWindowText(GetDlgItem(HWindow, IDC_FOUND_FILES), buff);

        /*
    // pokud jsme minimalizovany, zobrazim pocet polozek do hlavicky
    if (IsIconic(HWindow))
    {
      char buf[MAX_PATH+100];
      if (SearchInProgress)
      {
        _snprintf_s(buf, _TRUNCATE, MINIMIZED_FINDING_CAPTION, FoundFilesListView->GetCount(),
                    LoadStr(IDS_FF_NAME), LoadStr(IDS_FF_NAMED), SearchForData[0]->MasksGroup.GetMasksString());
      }
      else
        lstrcpy(buf, LoadStr(IDS_FF_NAME));
      SetWindowText(HWindow, buf);
    }
    */

        // slouzi pro hledaci thread - aby vedel, kdy nas ma priste upozornit
        FoundVisibleCount = count;
        NextUpdate = GetTickCount() + 500;
    }
}

void CFindDialog::UpdateStatusText(BOOL searchFinished)
{
    CALL_STACK_MESSAGE2("CFindDialog::UpdateStatusText(%d)", searchFinished);
    HWND focus = GetFocus(), lookFor = GetDlgItem(HWindow, IDC_PATTERN);

    if (focus == lookFor || GetParent(focus) == lookFor)
    {
        int id = IDS_SUBSTRINGHELP;
        if (SendDlgItemMessage(HWindow, IDC_REGEXP, BM_GETCHECK, 0, 0) == BST_CHECKED)
        {
            id = IDS_REGEXPHELP;
        }
        else
        {
            if (SendDlgItemMessage(HWindow, IDC_HEX, BM_GETCHECK, 0, 0) == BST_CHECKED)
                id = IDS_HEXHELP;
        }

        StatusBar->SetBase(LoadStrW(id));

        return;
    }

    int index = -1;
    index = ListView_GetNextItem(List->HWindow, index, LVNI_SELECTED);
    if (index != -1)
    {
        CFoundFilesData* file = List->At(index);
        WCHAR fullName[MAX_FULL_KEYNAME];
        PathAppend(lstrcpynW(fullName, file->Path, MAX_FULL_KEYNAME), file->Name, MAX_FULL_KEYNAME);
        StatusBar->SetBase(fullName, TRUE);
    }
    else
    {
        if (List->GetCount() == 0 && searchFinished)
            StatusBar->SetBase(LoadStrW(IDS_NOFILESFOUND), TRUE);
        else
            StatusBar->SetBase(L"", TRUE);
    }
}

void CFindDialog::OnEnterIdle()
{
    CALL_STACK_MESSAGE1("CFindDialog::OnEnterIdle()");
    StatusBar->OnEnterIdle();
}

void CFindDialog::StartSearch()
{
    CALL_STACK_MESSAGE1("CFindDialog::StartSearch()");
    List->DestroyMembers();
    ListView_SetItemCountEx(List->HWindow, 0, 0);
    UpdateWindow(List->HWindow);
    SetWindowText(GetDlgItem(HWindow, IDC_FOUND_FILES), LoadStr(IDS_FOUNDITEMS1));
    FoundVisibleCount = 0;
    NextUpdate = GetTickCount();

    // pripravime vzorek pro hledani
    char patternA[MAX_KEYNAME];
    char patternW[MAX_KEYNAME * 2];
    int patternALen, patternWLen;
    BOOL useNumber;
    QWORD number;
    if (Hex)
    {
        ConvertHexToString(Pattern, patternA, patternALen);
        memcpy(patternW, patternA, patternALen);
        patternWLen = patternALen;
    }
    else
    {
        if (RegExp)
        {
            patternALen = WStrToStr(patternA, MAX_KEYNAME, Pattern);
            patternWLen = 0;
        }
        else
        {
            patternALen = WStrToStr(patternA, MAX_KEYNAME, Pattern) - 1;
            wcscpy((LPWSTR)patternW, Pattern);
            patternWLen = (int)wcslen(Pattern) * 2;
            useNumber = DecStringToNumber(patternA, number) ||
                        HexStringToNumber(patternA, number);
        }
    }

    CancelEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    BOOL ok = FALSE;

    CFindThread* t = new CFindThread(LookInList,
                                     patternA, patternALen, patternW, patternWLen,
                                     IncludeSubkeys, LookAtKeys, LookAtValues,
                                     LookAtData, CaseSensitive, WholeWords, RegExp,
                                     number, useNumber,
                                     UseMinTime, UseMaxTime,
                                     MinTime, MaxTime,
                                     this, CancelEvent);
    if (t)
    {
        if (!t->Create(ThreadQueue))
            delete t;
        else
            ok = TRUE;
    }
    else
        Error(IDS_LOWMEM);

    if (ok)
    {
        SearchInProgress = TRUE;
        Stopped = FALSE;
        SetWindowText(GetDlgItem(HWindow, IDOK), LoadStr(IDS_STOP));
        SetTimer(HWindow, IDT_REFRESH_LISTVIEW, 500, NULL);
        //UpdateStatusText();
        EnableControls(FALSE);
    }
    else
        CloseHandle(CancelEvent);

    /*
  SearchInProgress = FALSE;
  SetWindowText( GetDlgItem(HWindow, IDOK), LoadStr(IDS_START));
  UpdateListViewItems();
  //UpdateStatusText();
  //EnableControls(TRUE);
  
  if (CloseWhenSearchFinishes) DestroyWindow(HWindow);
  else
  {
    BOOL b = IsIconic(HWindow);
    if (b && MinBeepWhenDone) MessageBeep(0);
  }
  */
}

void CFindDialog::StopSearch()
{
    CALL_STACK_MESSAGE1("CFindDialog::StopSearch()");
    SetEvent(CancelEvent);
    Stopped = TRUE;
}

BOOL ValidateTimeString(char* time)
{
    CALL_STACK_MESSAGE1("ValidateTimeString()");
    char* s = time + strlen(time);

    // orezeme mezery z konce
    while (isspace(s[-1]))
        s--;
    *s = 0;

    s = time;

    // nacteme cteme cas
    if (s[1] == ':' || s[2] == ':')
    {
        // hh
        if (!isdigit(*s++))
            return FALSE;
        if (*s != ':' && (!isdigit(*s++) || *s != ':'))
            return FALSE;
        s++;
        // mm
        if (!isdigit(*s++))
            return FALSE;
        if (*s != ':' && (!isdigit(*s++) || *s != ':'))
            return FALSE;
        s++;
        // ss
        if (!isdigit(*s++))
            return FALSE;
        if (!isspace(*s) && (!isdigit(*s++) || *s != ' '))
            return FALSE;
        s++;
    }

    // nacteme datum

    // dd
    if (!isdigit(*s++))
        return FALSE;
    if (*s != '.' && (!isdigit(*s++) || *s != '.'))
        return FALSE;
    s++;
    // mm
    if (!isdigit(*s++))
        return FALSE;
    if (*s != '.' && (!isdigit(*s++) || *s != '.'))
        return FALSE;
    s++;
    // yyyy
    if (!isdigit(*s++))
        return FALSE;
    if (*s != '\0')
    {
        if (!isdigit(*s++))
            return FALSE;
        if (*s != '\0')
        {
            if (!isdigit(*s++))
                return FALSE;
            if (*s != '\0')
            {
                if (!isdigit(*s++))
                    return FALSE;
            }
        }
    }

    if (*s != '\0')
        return FALSE;
    return TRUE;
}

BOOL ParseTimeString(const char* timeString, SYSTEMTIME& st, BOOL maxTime)
{
    CALL_STACK_MESSAGE3("ParseTimeString(%s, , %d)", timeString, maxTime);
    const char* s = timeString;

    memset(&st, 0, sizeof(st));

    // nacteme cteme cas
    if (s[1] == ':' || s[2] == ':')
    {
        // hh
        st.wHour = *s++ - '0';
        if (*s != ':')
            st.wHour = st.wHour * 10 + *s++ - '0';
        s++;
        // mm
        st.wMinute = *s++ - '0';
        if (*s != ':')
            st.wMinute = st.wMinute * 10 + *s++ - '0';
        s++;
        // ss
        st.wSecond = *s++ - '0';
        if (!isspace(*s))
            st.wSecond = st.wSecond * 10 + *s++ - '0';
        s++;
    }
    else
    {
        if (maxTime)
        {
            st.wHour = 23;
            st.wMinute = 59;
            st.wSecond = 59;
        }
    }

    // nacteme datum

    // dd
    st.wDay = st.wDay + *s++ - '0';
    if (*s != '.')
        st.wDay = st.wDay * 10 + *s++ - '0';
    s++;
    // mm
    st.wMonth = *s++ - '0';
    if (*s != '.')
        st.wMonth = st.wMonth * 10 + *s++ - '0';
    s++;
    // yyyy
    st.wYear = *s++ - '0';
    if (*s != '\0')
        st.wYear = st.wYear * 10 + *s++ - '0';
    if (*s != '\0')
        st.wYear = st.wYear * 10 + *s++ - '0';
    if (*s != '\0')
        st.wYear = st.wYear * 10 + *s++ - '0';

    TRACE_I("ParseTimeString: " << st.wHour << " " << st.wMinute << " " << st.wSecond
                                << " " << st.wDay << " " << st.wMonth << " " << st.wYear);

    // overime spravnost zadaneho casu
    FILETIME ft;
    if (SystemTimeToFileTime(&st, &ft))
        return TRUE;

    return FALSE;
}

void CFindDialog::Validate(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CFindDialog::Validate()");
    /*
  WCHAR buffer[MAX_KEYNAME];
  ti.EditLineW(IDC_PATTERN, buffer, MAX_KEYNAME);
  
  if (strlen(buffer) == 0)
  {
    SG->SalMessageBox(HWindow, LoadStr(IDS_EMPTY), LoadStr(IDS_ERROR), MB_ICONEXCLAMATION);
    ti.ErrorOn(IDC_PATTERN);
  }
  */

    BOOL b;
    ti.CheckBox(IDC_HEX, b);
    if (b)
    {
        WCHAR buffer[MAX_KEYNAME];
        ti.EditLineW(IDC_PATTERN, buffer, MAX_KEYNAME);
        if (!ValidateHexString(buffer))
        {
            SG->SalMessageBox(GetParent(), LoadStr(IDS_BADHEXSTRING), LoadStr(IDS_ERROR), MB_OK);
            ti.ErrorOn(IDC_PATTERN);
            return;
        }
    }

    // validujeme casy
    char buffer[20];
    ti.EditLine(IDC_MINTIME, buffer, MAX_KEYNAME);
    if (strlen(buffer) && !ValidateTimeString(buffer))
    {
        SG->SalMessageBox(GetParent(), LoadStr(IDS_BADTIMEFORMAT), LoadStr(IDS_ERROR), MB_OK);
        ti.ErrorOn(IDC_MINTIME);
        return;
    }

    ti.EditLine(IDC_MAXTIME, buffer, MAX_KEYNAME);
    if (strlen(buffer) && !ValidateTimeString(buffer))
    {
        SG->SalMessageBox(GetParent(), LoadStr(IDS_BADTIMEFORMAT), LoadStr(IDS_ERROR), MB_OK);
        ti.ErrorOn(IDC_MAXTIME);
        return;
    }
}

void CFindDialog::Transfer(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CFindDialog::Transfer()");
    ti.CheckBox(IDC_OPTIONS, ShowOptions);
    WCHAR lookIn[1024];
    lookIn[0] = 0;
    HistoryComboBox(ti, IDC_PATTERN, Pattern, MAX_KEYNAME, MAX_HISTORY_ENTRIES, PatternHistory);
    HistoryComboBox(ti, IDC_LOOKIN, lookIn, 1024, MAX_HISTORY_ENTRIES, LookInHistory);
    ti.CheckBox(IDC_SUBDIRS, IncludeSubkeys);
    ti.CheckBox(IDC_KEYS, LookAtKeys);
    ti.CheckBox(IDC_VALUES, LookAtValues);
    ti.CheckBox(IDC_DATA, LookAtData);
    ti.CheckBox(IDC_HEX, Hex);
    ti.CheckBox(IDC_CASE, CaseSensitive);
    ti.CheckBox(IDC_WHOLEWORDS, WholeWords);
    ti.CheckBox(IDC_REGEXP, RegExp);
    if (ti.Type == ttDataToWindow)
    {
        //SendDlgItemMessage(HWindow, IDC_PATH, CB_SETCURSEL, 0, 0);
        wcscpy(lookIn, LookInInit);
        if (DuplicateChar(L';', lookIn, 1024))
            ti.EditLineW(IDC_LOOKIN, lookIn, 1024);
        else
            Error(IDS_LONGNAME);
    }
    else
    {
        // nacteme z LookIn jednotlive polozky
        LookInList.DestroyMembers();
        BOOL more = TRUE;
        LPWSTR end = lookIn;
        LPWSTR start;
        while (more)
        {
            start = end;
            while (*end != L'\0')
            {
                if (*end == L';')
                {
                    if (end[1] == L';')
                        end++;
                    else
                        break;
                }
                end++;
            }
            more = *end != '\0';
            *end = '\0';
            if (end > start)
                LookInList.Add(DupStr(UnDuplicateChar(L';', start)));
            end++;
        }

        // nacteme casy
        UseMinTime = UseMaxTime = FALSE; // def hodnoty
        char buffer[20];
        ti.EditLine(IDC_MINTIME, buffer, MAX_KEYNAME);
        if (strlen(buffer))
        {
            if (!ValidateTimeString(buffer) || !ParseTimeString(buffer, MinTime, FALSE))
            {
                SG->SalMessageBox(GetParent(), LoadStr(IDS_BADTIMEFORMAT), LoadStr(IDS_ERROR), MB_OK);
                ti.ErrorOn(IDC_MINTIME);
                return;
            }
            UseMinTime = TRUE;
        }

        ti.EditLine(IDC_MAXTIME, buffer, MAX_KEYNAME);
        if (strlen(buffer))
        {
            if (!ValidateTimeString(buffer) || !ParseTimeString(buffer, MaxTime, TRUE))
            {
                SG->SalMessageBox(GetParent(), LoadStr(IDS_BADTIMEFORMAT), LoadStr(IDS_ERROR), MB_OK);
                ti.ErrorOn(IDC_MAXTIME);
                return;
            }
            UseMaxTime = TRUE;
        }
    }
}

INT_PTR
CFindDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CFindDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                             wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        //DialogStackPush(HWindow);

        //InstallWordBreakProc(GetDlgItem(HWindow, IDC_PATTERN), TRUE); // instalujeme WordBreakProc do comboboxu

        // priradim oknu ikonku
        HICON findIcon = NULL;
        HINSTANCE iconsDLL = NULL;
        if (WindowsVistaAndLater)
            iconsDLL = LoadLibraryEx("imageres.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
        else
            iconsDLL = LoadLibraryEx("shell32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (iconsDLL != NULL)
        {
            if (WindowsVistaAndLater)
                findIcon = LoadIcon(iconsDLL, MAKEINTRESOURCE(8));
            else
                findIcon = LoadIcon(iconsDLL, MAKEINTRESOURCE(134));
        }
        if (findIcon == NULL)
            findIcon = LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_FIND));
        SendMessage(HWindow, WM_SETICON, ICON_BIG, (LPARAM)findIcon);
        if (iconsDLL != NULL)
            FreeLibrary(iconsDLL);

        // konstrukce listview
        List = new CFoundFilesListView(this);
        if (List == NULL)
        {
            TRACE_E("Low memory");
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            break;
        }

        List->AttachToControl(HWindow, IDC_RESULTS);

        // vytvorim status bar
        //StatusBar = CStatusBar::Create(HWindow, IDC_STATUS);
        StatusBar = new CStatusBar();
        if (StatusBar == NULL)
        {
            TRACE_E("Low memory");
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            break;
        }
        StatusBar->CreateEx(0,
                            CWINDOW_CLASSNAME,
                            (LPCTSTR)NULL,
                            WS_CHILD | WS_VISIBLE,
                            0, 0, 0, 0,
                            HWindow,
                            (HMENU)IDC_STATUS,
                            DLLInstance,
                            StatusBar);

        // nactu paramatry pro layoutovani okna
        GetLayoutParams();

        if (DialogWidth != -1 && DialogHeight != -1)
        {
            SetWindowPos(HWindow, NULL, 0, 0, DialogWidth, DialogHeight, SWP_NOMOVE | SWP_NOZORDER);
            if (Maximized)
                ShowWindow(HWindow, SW_MAXIMIZE);
        }

        LayoutControls(TRUE);
        List->InitColumns();

        ListView_SetItemCount(List->HWindow, 0);

        //CenterWindow(HWindow, SG->GetMainWindowHWND(), TRUE);

        // zmenou pozice okna dojde k distribuci zprav, takze uz musi existovat objekty konstruovane vejs, jinak to hazelo exception
        if (AlwaysOnTop)
            SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        break;
    }

    case WM_SIZE:
    {
        LayoutControls(FALSE);
        break;
    }

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinDlgW;
        lpmmi->ptMinTrackSize.y = GetMinDlgH();
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            if (SearchInProgress) // jde o Stop?
            {
                if (IsIconic(HWindow))
                {
                    if (MinBeepWhenDone)
                        MessageBeep(0);
                }
                StopSearch();
                return TRUE;
            }
            else // ne, jde o start
            {
                if (!ValidateData() || !TransferData(ttDataFromWindow))
                    return TRUE;
                StartSearch();
                return TRUE;
            }
        }

        case IDCANCEL:
        {
            if (SearchInProgress)
            {
                StopSearch();
                return TRUE;
            }
            break;
        }

        case IDC_PATTERN:
        {
            if (HIWORD(wParam) == CBN_SETFOCUS || HIWORD(wParam) == CBN_KILLFOCUS)
                UpdateStatusText();
            break;
        }

        case IDC_OPTIONS:
        {
            ShowOptions = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
            LayoutControls(TRUE);
            break;
        }

        case IDC_HEX:
        {
            if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                SendDlgItemMessage(HWindow, IDC_CASE, BM_SETCHECK, BST_CHECKED, 0);
                SendDlgItemMessage(HWindow, IDC_WHOLEWORDS, BM_SETCHECK, BST_UNCHECKED, 0);
            }
            EnableControls(TRUE);
            break;
        }

        case IDC_REGEXP:
        {
            if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                SendDlgItemMessage(HWindow, IDC_HEX, BM_SETCHECK, BST_UNCHECKED, 0);
            }
            EnableControls(TRUE);
            break;
        }

        case CMD_FINDFOCUS:
        {
            int count = ListView_GetSelectedCount(List->HWindow);
            if (count == 0)
                break;

            int index = -1;
            index = ListView_GetNextItem(List->HWindow, index, LVNI_SELECTED);
            if (index != -1)
            {
                CFoundFilesData* data = List->At(index);

                // podivame se zda existuje
                WCHAR pathW[MAX_FULL_KEYNAME + 1] = L"\\";
                wcscpy(pathW + 1, data->Path);
                if (data->IsDir)
                    PathAppend(pathW, data->Name, MAX_FULL_KEYNAME + 1);

                WCHAR* key;
                int root;
                if (ParseFullPath(pathW, key, root))
                {
                    HKEY hKey;
                    int ret = RegOpenKeyExW(PredefinedHKeys[root].HKey, key,
                                            0, KEY_QUERY_VALUE, &hKey);
                    if (ret == ERROR_SUCCESS && !data->IsDir)
                    {
                        DWORD existType;
                        ret = RegQueryValueExW(hKey,
                                               data->Default ? NULL : data->Name, 0, &existType, NULL, 0);
                    }

                    if (ret != ERROR_FILE_NOT_FOUND)
                    {
                        char path[MAX_FULL_KEYNAME + 1] = "\\";
                        char name[MAX_KEYNAME];
                        WStrToStr(path + 1, MAX_FULL_KEYNAME, data->Path);
                        WStrToStr(name, MAX_KEYNAME, data->Name);
                        if (!InterfaceForMenuExt.PostFocusCommand(path, name))
                            SG->SalMessageBox(HWindow, LoadStr(IDS_BUSY), LoadStr(IDS_PLUGINNAME),
                                              MB_ICONINFORMATION);
                    }
                    else
                    {
                        TRACE_I("RET=" << ret);
                        ErrorL(ret, IDS_OPEN); //TODO
                    }
                }
            }

            break;
        }
        }
    }

    case WM_NOTIFY:
    {
        if (wParam == IDC_RESULTS)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case NM_DBLCLK:
            {
                PostMessage(HWindow, WM_COMMAND, CMD_FINDFOCUS, 0);
                break;
            }

            case NM_RCLICK:
            {
                DWORD pos = GetMessagePos();
                //OnCtxMenu(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
                break;
            }

            case LVN_COLUMNCLICK:
            {
                int subItem = ((NM_LISTVIEW*)lParam)->iSubItem;
                if (subItem != CI_DATA && subItem < 7)
                    List->SortItems(subItem);
                break;
            }

            case LVN_GETDISPINFOW:
            {
                NMLVDISPINFOW* info = (NMLVDISPINFOW*)lParam;
                CFoundFilesData* item = List->At(info->item.iItem);
                if (info->item.mask & LVIF_IMAGE)
                {
                    if (item->IsDir)
                    {
                        info->item.iImage = 0;
                    }
                    else
                    {
                        switch (item->Type)
                        {
                        case REG_EXPAND_SZ:
                        case REG_MULTI_SZ:
                        case REG_SZ:
                            info->item.iImage = 1;
                            break;

                        case REG_BINARY:
                        case REG_DWORD:
                        case REG_DWORD_BIG_ENDIAN:
                        case REG_QWORD:
                        case REG_LINK:
                        case REG_RESOURCE_LIST:
                        case REG_FULL_RESOURCE_DESCRIPTOR:
                        case REG_RESOURCE_REQUIREMENTS_LIST:
                            info->item.iImage = 2;
                            break;

                        case REG_NONE:
                        default:
                            info->item.iImage = 3;
                        }
                    }
                }
                if (info->item.mask & LVIF_TEXT)
                    info->item.pszText = item->GetText(info->item.iSubItem, LVItemTextBuffer);
                break;
            }

            case LVN_ITEMCHANGED:
            {
                if (!SearchInProgress)
                    UpdateStatusText();
                break;
            }

            case LVN_KEYDOWN:
            {
                NMLVKEYDOWN* kd = (NMLVKEYDOWN*)lParam;
                BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                DWORD cmd = 0;
                switch (kd->wVKey)
                {
                case VK_RETURN:
                {
                    //if (!controlPressed && !altPressed && !shiftPressed) cmd = CM_OPEN;
                    //if (controlPressed && !altPressed && !shiftPressed) cmd = CM_EXACTSEARCH;
                    break;
                }

                case VK_SPACE:
                {
                    if (!controlPressed && !altPressed && !shiftPressed)
                        cmd = CMD_FINDFOCUS;
                    break;
                }

                case VK_INSERT:
                {
                    //if (!controlPressed && altPressed && !shiftPressed) cmd = CM_COPYFULLNAME;
                    //if (!controlPressed && altPressed && shiftPressed) cmd = CM_COPYFILENAME;
                    //if (controlPressed && altPressed && !shiftPressed) cmd = CM_COPYFULLPATH;
                    break;
                }

                case VK_F10:
                    if (!shiftPressed)
                        break;
                case VK_APPS:
                {
                    POINT pt;
                    List->GetContextMenuPos(&pt);
                    //OnCtxMenu(pt.x, pt.y);
                    break;
                }
                }
                if (cmd != 0)
                    PostMessage(HWindow, WM_COMMAND, cmd, 0);
            }
            }
        }
        break;
    }

    case WM_TIMER:
    {
        if ((int)(GetTickCount() - NextUpdate) > 0 && List->GetCount() > FoundVisibleCount)
        {
            UpdateListViewItems();
        }
        return TRUE;
    }

    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(List->HWindow, GetSysColor(COLOR_WINDOW));
        break;
    }

    case WM_USER_ADDFILE:
    {
        UpdateListViewItems();
        return TRUE;
    }

    case WM_USER_BUTTONS:
    {
        if (lParam)
        {
            PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
        }
        else
        {
            if (wParam)
            {
                //SendMessage(HWindow, DM_SETDEFID, GetDlgCtrlID((HWND)wParam), 0);
                PostMessage((HWND)wParam, BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
            }
            else
            {
                SendMessage(HWindow, DM_SETDEFID, IDOK, 0);
                PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
            }
        }
        return TRUE;
    }

    case WM_USER_SEARCH_FINISHED:
    {
        // hledani zkoncilo
        SearchInProgress = FALSE;
        SetWindowText(GetDlgItem(HWindow, IDOK), LoadStr(IDS_START));
        CloseHandle(CancelEvent); // uz ho nebudeme potrebovat
        UpdateListViewItems();
        //UpdateStatusText();
        EnableControls(TRUE);
        KillTimer(HWindow, IDT_REFRESH_LISTVIEW);

        if (CloseWhenSearchFinishes)
            DestroyWindow(HWindow);
        else
        {
            BOOL b = IsIconic(HWindow);
            if (b && MinBeepWhenDone)
                MessageBeep(0);
            if (Stopped)
                StatusBar->SetBase(LoadStrW(IDS_STOPPED));
            else
                UpdateStatusText(TRUE);
        }
        return TRUE;
    }

        /*
    case WM_USER_CLEARHISTORY:
    {
      char buffer[MAX_PATTERN];
      SendMessage(GetDlgItem(HWindow, IDC_PATTERN), WM_GETTEXT, MAX_PATTERN, (LPARAM)buffer);
      SendMessage(GetDlgItem(HWindow, IDC_PATTERN), CB_RESETCONTENT, 0, 0);
      SendMessage(GetDlgItem(HWindow, IDC_PATTERN), WM_SETTEXT, 0, (LPARAM)buffer);
      break;
    }
    */

    case WM_CLOSE:
    {
        if (SearchInProgress)
        {
            StopSearch();
            CloseWhenSearchFinishes = TRUE;
            return TRUE;
        }
        DestroyWindow(HWindow);
        return TRUE;
    }

    case WM_DESTROY:
    {
        if (SearchInProgress)
            TRACE_E("to se nemelo stat");

        // ulozime si rozmery okna
        WINDOWPLACEMENT wndpl;
        wndpl.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(HWindow, &wndpl))
        {
            DialogWidth = wndpl.rcNormalPosition.right - wndpl.rcNormalPosition.left;
            DialogHeight = wndpl.rcNormalPosition.bottom - wndpl.rcNormalPosition.top;
            Maximized = wndpl.showCmd == SW_SHOWMAXIMIZED;
        }

        // uvolnime handle, jinak by ho ListView vzalo s sebou do pekel
        ListView_SetImageList(List->HWindow, NULL, LVSIL_SMALL);

        // uvolnime data nez se zavre okno, jinak by se mohlo stat, ze by nas
        // thread salamander sestrelil driv nez zkonci
        List->DestroyMembers();
        ListView_SetItemCountEx(List->HWindow, 0, 0);

        // DialogStackPop();

        if (ZeroOnDestroy != NULL)
            *ZeroOnDestroy = NULL;
        break;
    }
    }
    return CDialogEx::DialogProc(uMsg, wParam, lParam);
}

unsigned
CFindDialogThread::Body()
{
    CALL_STACK_MESSAGE1("CFindDialogThread::Body()");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    CFindDialog* dlg = new CFindDialog(LookIn);
    if (!dlg)
        return Error(IDS_LOWMEM);

    HWND wnd = dlg->Create();
    if (!wnd)
    {
        TRACE_E("Nepodarilo se vytvorit CFindDialog");
        delete dlg;
        return 0;
    }
    dlg->SetZeroOnDestroy(&dlg); // pri WM_DESTROY bude ukazatel vynulovan
                                 // obrana pred pristupem na neplatny ukazatel
                                 // z message loopy po destrukci okna

    SetForegroundWindow(wnd);

    if (!WindowQueue.Add(new CWindowQueueItem(wnd)))
    {
        TRACE_E("Low memory");
        DestroyWindow(wnd);
    }

    MSG msg;
    BOOL haveMSG = FALSE; // FALSE pokud se ma volat GetMessage() v podmince cyklu
    while (haveMSG || IsWindow(wnd) && GetMessage(&msg, NULL, 0, 0))
    {
        haveMSG = FALSE;

        if (!IsDialogMessage(wnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;      // ekvivalent situace, kdy GetMessage() vraci FALSE
            haveMSG = TRUE; // mame zpravu, jdeme ji zpracovat (bez volani GetMessage())
        }
        else // pokud ve fronte neni zadna message, provedeme Idle processing
        {
            if (dlg != NULL)
                dlg->OnEnterIdle();
        }
    }

    WindowQueue.Remove(wnd);

    return 0;
}
