// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

HIMAGELIST HSymbolsImageList = NULL;
char DirText[100];

CPreviewWindow::CPreviewWindow(CRenamerDialog* renamerDialog)
    : RenamerOptions(renamerDialog->RenamerOptions),
      Renamer(renamerDialog->Root, renamerDialog->RootLen),
      Root(renamerDialog->Root),
      RootLen(renamerDialog->RootLen),
      SourceFiles(renamerDialog->SourceFiles),
      SourceFilesValid(renamerDialog->SourceFilesValid)
{
    CALL_STACK_MESSAGE1("CPreviewWindow::CPreviewWindow()");
    RenamerDialog = renamerDialog;
    TransferError = FALSE;
    Dirty = FALSE;
    Renamer.SetOptions(&RenamerOptions);
    CachedItem = -1;
    Static = 0;
    State = -1;
}

CPreviewWindow::~CPreviewWindow()
{
    CALL_STACK_MESSAGE1("CPreviewWindow::~CPreviewWindow()");
}

BOOL CPreviewWindow::InitColumns()
{
    CALL_STACK_MESSAGE1("CPreviewWindow::InitColumns()");

    LV_COLUMN lvc;
    int header[] =
        {IDS_ORIGNAME_COLUMN, IDS_NEWNAME_COLUMN, IDS_SIZE_COLUMN, IDS_DATE_COLUMN,
         IDS_TIME_COLUMN, IDS_PATH_COLUMN, -1};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; header[i] != -1; i++) // vytvorim sloupce
    {
        lvc.pszText = (LPSTR)LoadStr(header[i]);
        lvc.iSubItem = i;
        if (i == 2)
            lvc.fmt = LVCFMT_RIGHT;
        if (i == 5)
            lvc.fmt = LVCFMT_LEFT;
        if (ListView_InsertColumn(HWindow, i, &lvc) == -1)
            return FALSE;
    }

    RECT r;
    GetClientRect(HWindow, &r);
    DWORD cx = r.right - r.left + (1 ? -1 : 1);
    ListView_SetColumnWidth(HWindow, CI_TIME, ListView_GetStringWidth(HWindow, "00:00:00") + 20);
    ListView_SetColumnWidth(HWindow, CI_DATE, ListView_GetStringWidth(HWindow, "00.00.0000") + 20);
    ListView_SetColumnWidth(HWindow, CI_SIZE, ListView_GetStringWidth(HWindow, "000000") + 20);

    cx -= ListView_GetColumnWidth(HWindow, CI_TIME) + ListView_GetColumnWidth(HWindow, CI_DATE) +
          ListView_GetColumnWidth(HWindow, CI_SIZE) + GetSystemMetrics(SM_CXHSCROLL) - 1;

    ListView_SetColumnWidth(HWindow, CI_OLDNAME, cx / 3);
    ListView_SetColumnWidth(HWindow, CI_NEWNAME, cx / 3);

    cx -= ListView_GetColumnWidth(HWindow, CI_OLDNAME) +
          ListView_GetColumnWidth(HWindow, CI_NEWNAME);

    ListView_SetColumnWidth(HWindow, CI_PATH, cx);

    ListView_SetImageList(HWindow, HSymbolsImageList, LVSIL_SMALL);

    return TRUE;
}

void CPreviewWindow::Update(BOOL force)
{
    CALL_STACK_MESSAGE2("CPreviewWindow::Update(%d)", force);
    if (!Dirty && !force)
        return;

    if (RenamerDialog->TransferForPreview())
    {
        TransferError = FALSE;
        Renamer.SetOptions(&RenamerOptions);
    }
    else
        TransferError = TRUE;

    CachedItem = -1;

    RECT cl;
    GetClientRect(HWindow, &cl);

    RECT hr;
    GetWindowRect(ListView_GetHeader(HWindow), &hr);

    cl.top += hr.bottom - hr.top;
    if (!force)
    {
        cl.left = ListView_GetColumnWidth(HWindow, 0);
        cl.right = cl.left + ListView_GetColumnWidth(HWindow, 1);
        cl.left = 0; // aby se prekreslila i ikona
    }

    InvalidateRect(HWindow, &cl, FALSE);

    // ListView_RedrawItems(HWindow, 0, SourceFiles.Count - 1);

    UpdateWindow(HWindow);
    Dirty = FALSE;
}

void CPreviewWindow::GetDispInfo(LV_DISPINFO* info)
{
    CALL_STACK_MESSAGE1("CPreviewWindow::GetDispInfo()");
    // TRACE_I("get-disp-info item=" << info->item.iItem <<
    //     " subitem=" << info->item.iSubItem <<
    //     (info->item.mask & LVIF_IMAGE ? " image" : "") <<
    //     (info->item.mask & LVIF_TEXT ? " text" : ""));

    if (info->item.iItem < SourceFiles.Count)
    {
        if (info->item.mask & LVIF_IMAGE)
        {
            if (CachedItem != info->item.iItem)
                GetItemText(info->item.iItem, CI_NEWNAME);
            info->item.iImage =
                NewNameValid ? (SourceFiles[info->item.iItem]->IsDir ? ILS_DIRECTORY : ILS_FILE) : ILS_WARNING;
        }
        if (info->item.mask & LVIF_TEXT)
            info->item.pszText = GetItemText(info->item.iItem, info->item.iSubItem);
    }
    else
    {
        static char emptyBuffer[] = "";
        if (info->item.mask & LVIF_IMAGE)
            info->item.iImage = ILS_FILE;
        if (info->item.mask & LVIF_TEXT)
            info->item.pszText = emptyBuffer;
    }
}

char* CPreviewWindow::GetItemText(int index, int subItem)
{
    CALL_STACK_MESSAGE3("CPreviewWindow::GetItemText(%d, %d)", index, subItem);
    CSourceFile* item = SourceFiles[index];
    static char emptyBuffer[] = "";
    char* ret = emptyBuffer;
    switch (subItem)
    {
    case CI_OLDNAME:
        switch (RenamerOptions.Spec)
        {
        case rsFileName:
            ret = item->Name;
            break;
        case rsRelativePath:
            ret = StripRoot(item->FullName, RootLen);
            break;
        case rsFullPath:
            ret = item->FullName;
            break;
        }
        break;

    case CI_NEWNAME:
        if (CachedItem != index)
        {
            NewNameValid = FALSE;
            if (RenamerDialog->ManualMode)
            {
                // optimalizace
                // int pos = SendDlgItemMessage(RenamerDialog->HWindow, IDE_MANUAL, EM_LINEINDEX, index, 0);
                // if (pos < 0)
                // {
                //   SalPrintf(NewNameCache, MAX_PATH, LoadStr(IDS_GENERICERR), LoadStr(IDS_MISLINES));
                // }
                // else
                // {
                //   int l = SendDlgItemMessage(RenamerDialog->HWindow, IDE_MANUAL, EM_LINELENGTH, pos, 0);

                //   if (l >= MAX_PATH)
                //   {
                //     SalPrintf(NewNameCache, MAX_PATH, LoadStr(IDS_GENERICERR), LoadStr(IDS_EXP_SMALLBUFFER));
                //   }
                //   else
                //   {
                *LPWORD(NewNameCache) = MAX_PATH;
                int l = (int)SendMessage(RenamerDialog->ManualEdit->HWindow, EM_GETLINE,
                                         index, (LPARAM)NewNameCache);
                NewNameCache[l] = 0; // pro jistotu

                NewNameValid = ValidateFileName(NewNameCache, l, RenamerOptions.Spec, NULL, NULL);
                //  }
                // }
            }
            else
            {
                if (TransferError)
                    strcpy(NewNameCache, LoadStr(IDS_TRANSFERERROR));
                else
                {
                    if (Renamer.IsGood())
                    {
                        int l = Renamer.Rename(item, index, NewNameCache, FALSE);
                        if (l < 0)
                        {
                            SalPrintf(NewNameCache, MAX_PATH, LoadStr(IDS_GENERICERR), LoadStr(IDS_EXP_SMALLBUFFER));
                        }
                        else
                        {
                            NewNameValid = ValidateFileName(NewNameCache, l, RenamerOptions.Spec, NULL, NULL);
                        }
                    }
                    else
                    {
                        int error, errorPos1, errorPos2;
                        CRenamerErrorType errorType;
                        Renamer.GetError(error, errorPos1, errorPos2, errorType);
                        int et;
                        switch (errorType)
                        {
                        case retNewName:
                            et = IDS_NEWNAMEERR;
                            break;
                        case retBMSearch:
                            et = IDS_BMERR;
                            break;
                        case retRegExp:
                            et = IDS_REGEXPERR;
                            break;
                        case retReplacePattern:
                            et = IDS_REPLACEERR;
                            break;
                        default:
                            et = IDS_GENERICERR;
                            break;
                        }
                        SalPrintf(NewNameCache, MAX_PATH, LoadStr(et), LoadStr(error));
                    }
                }
            }
            CachedItem = index;
        }
        ret = NewNameCache;
        break;

    case CI_PATH:
        switch (RenamerOptions.Spec)
        {
        case rsFileName:
        {
            lstrcpyn(TextBuffer, item->FullName, _countof(TextBuffer));
            if (item->NameLen < _countof(TextBuffer) ||
                strchr(item->FullName + _countof(TextBuffer) - 1, '\\') == NULL)
            {
                SG->CutDirectory(TextBuffer);
            }
            ret = TextBuffer;
            break;
        }
        case rsRelativePath:
            ret = Root;
            break;
        case rsFullPath:
            break;
        }
        break;

    case CI_SIZE:
    {
        if (item->IsDir)
        {
            ret = DirText;
        }
        else
        {
            SG->NumberToStr(TextBuffer, item->Size);
            ret = TextBuffer;
        }
        break;
    }

    case CI_DATE:
    {
        // TODO: jaky cas dostanu ze salamandera? jaky cas dostanu z FindXXFile?
        SYSTEMTIME st;
        FileTimeToSystemTime(&item->LastWrite, &st);
        if (!GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, TextBuffer, 100))
            SalPrintf(TextBuffer, 100, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        ret = TextBuffer;
        break;
    }

    case CI_TIME:
    {
        SYSTEMTIME st;
        FileTimeToSystemTime(&item->LastWrite, &st);
        if (!GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, TextBuffer, 100))
            SalPrintf(TextBuffer, 100, "%d:%d:%d", st.wHour, st.wMinute, st.wSecond);
        ret = TextBuffer;
        break;
    }
    }

    return ret;
}

BOOL CPreviewWindow::CustomDraw(LPNMLVCUSTOMDRAW cd, LRESULT& result)
{
    CALL_STACK_MESSAGE_NONE
    int item = (int)cd->nmcd.dwItemSpec; // x64 - dwItemSpec: The item number
    int subItem = cd->iSubItem;

    switch (cd->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
    {
        result = CDRF_NOTIFYITEMDRAW;
        return TRUE;
    }

    case CDDS_ITEMPREPAINT:
    {
        // pozadame si o zaslani notifikace CDDS_ITEMPREPAINT | CDDS_SUBITEM
        result = CDRF_NOTIFYSUBITEMDRAW;
        return TRUE;
    }

    case (CDDS_ITEMPREPAINT | CDDS_SUBITEM):
    {
        if (item >= SourceFiles.Count)
        {
            // dostali jsme dve padacky, kdy byl pocet prvku v poli SourceFiles rovno 1 a zaroven se mela kreslit polozka s indexem 1
            // komentar od jedne z padacek je:
            // 5ADF745CD736D5EC-AS30B1PB87X64-120909-215934-A8CF45D9.7Z / mkolka@gmail.com
            // Hromadne premenovanie suborov, s najdenymi duplictnymi subormi.
            // problem se mi bohuzel nepodarilo dohledat ani reprodukovat, takze pouze osetrim tuto situaci, abychom nepadali
            TRACE_E("CPreviewWindow::CustomDraw() item=" << item << " Count=" << SourceFiles.Count);
            break;
        }

        if (subItem == CI_NEWNAME &&
            strcmp(GetItemText(item, CI_OLDNAME), GetItemText(item, CI_NEWNAME)) == 0)
            cd->clrText = GetSysColor(COLOR_GRAYTEXT);
        else
            cd->clrText = GetSysColor(COLOR_WINDOWTEXT);
        result = CDRF_NEWFONT;
        return TRUE;
    }
    }

    return FALSE;
}

int CPreviewWindow::CompareFunc(CSourceFile* f1, CSourceFile* f2, int sortBy)
{
    CALL_STACK_MESSAGE2("CPreviewWindow::CompareFunc(, , %d)", sortBy);
    int res;
    int next = 0, sortByIndex;
    static int sortKeys[] = {CI_OLDNAME, CI_PATH, CI_SIZE, CI_TIME};
    switch (sortBy)
    {
    case CI_OLDNAME:
        next = 0;
        break;
    case CI_NEWNAME:
        next = 0;
        break;
    case CI_SIZE:
        next = 2;
        break;
    case CI_DATE:
        next = 3;
        break;
    case CI_TIME:
        next = 3;
        break;
    case CI_PATH:
        next = 1;
        break;
    }
    sortByIndex = next;
    do
    {
        switch (sortKeys[next])
        {
        case CI_OLDNAME:
        {
            if (f1->IsDir == f2->IsDir)
            {
                switch (RenamerOptions.Spec)
                {
                case rsFileName:
                    res = SG->RegSetStrICmp(f1->Name, f2->Name);
                    if (!res)
                        res = SG->RegSetStrCmp(f1->Name, f2->Name);
                    break;

                case rsFullPath:
                case rsRelativePath:
                    res = SG->RegSetStrICmp(f1->FullName, f2->FullName);
                    if (!res)
                        res = SG->RegSetStrCmp(f1->FullName, f2->FullName);
                    break;
                }
            }
            else
                res = f1->IsDir ? -1 : 1;
            break;
        }

        case CI_PATH:
        {
            switch (RenamerOptions.Spec)
            {
            case rsFileName:
            {
                res = SG->RegSetStrICmpEx(f1->FullName, (int)(f1->FullName - f1->Name),
                                          f2->FullName, (int)(f2->FullName - f2->Name), NULL);
                if (!res)
                    res = SG->RegSetStrCmpEx(f1->FullName, (int)(f1->FullName - f1->Name),
                                             f2->FullName, (int)(f2->FullName - f2->Name), NULL);
                break;
            }
            case rsRelativePath:
            case rsFullPath:
                res = 0;
                break;
            }
            break;
        }

        case CI_SIZE:
        {
            if (f1->IsDir == f2->IsDir)
            {
                if (f1->IsDir)
                    res = 0;
                else
                {
                    if (f1->Size < f2->Size)
                        res = -1;
                    else
                    {
                        if (f1->Size == f2->Size)
                            res = 0;
                        else
                            res = 1;
                    }
                }
            }
            else
                res = f1->IsDir ? -1 : 1;

            break;
        }

        case CI_TIME:
        {
            if (f1->IsDir == f2->IsDir)
                res = CompareFileTime(&f1->LastWrite, &f2->LastWrite);
            else
                res = f1->IsDir ? -1 : 1;
            break;
        }
        }
        if (next == sortByIndex)
        {
            if (sortByIndex != 0)
                next = 0;
            else
                next = 1;
        }
        else if (next + 1 != sortByIndex)
            next++;
        else
            next += 2;
    } while (res == 0 && next < 4);

    return res;
}

void CPreviewWindow::QuickSort(int left, int right, int sortBy)
{
    CALL_STACK_MESSAGE4("CFoundFilesListView::QuickSort(%d, %d, %d)", left,
                        right, sortBy);

LABEL_QuickSort:

    int i = left, j = right;
    CSourceFile* pivot = SourceFiles[(i + j) / 2];

    do
    {
        while (CompareFunc(SourceFiles[i], pivot, sortBy) < 0 && i < right)
            i++;
        while (CompareFunc(pivot, SourceFiles[j], sortBy) < 0 && j > left)
            j--;

        if (i <= j)
        {
            CSourceFile* swap = SourceFiles[i];
            SourceFiles[i] = SourceFiles[j];
            SourceFiles[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) QuickSort(left, j, sortBy);
    //  if (i < right) QuickSort(i, right, sortBy);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                QuickSort(left, j, sortBy);
                left = i;
                goto LABEL_QuickSort;
            }
            else
            {
                QuickSort(i, right, sortBy);
                right = j;
                goto LABEL_QuickSort;
            }
        }
        else
        {
            right = j;
            goto LABEL_QuickSort;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_QuickSort;
        }
    }
}

void CPreviewWindow::SortItems(int sortBy)
{
    CALL_STACK_MESSAGE2("CPreviewWindow::SortItems(%d)", sortBy);

    HCURSOR hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    if (SourceFiles.Count > 0)
    {
        // ulozime si pozici vybrane polozky
        // int focusIndex = ListView_GetNextItem(HWindow, -1, LVNI_FOCUSED);

        // seradim pole podle pozadovaneho kriteria
        QuickSort(0, SourceFiles.Count - 1, sortBy);

        // obnovime vyber
        // if (focusIndex != -1) ListView_EnsureVisible(HWindow, focusIndex, FALSE);
        Update(TRUE);
    }

    SetCursor(hCursor);
}

int CPreviewWindow::GetSortCommand(int column)
{
    CALL_STACK_MESSAGE_NONE
    int cmd = 0;
    switch (column)
    {
    case CI_OLDNAME:
        cmd = CMD_SORTOLD;
        break;
    //case CI_NEWNAME: cmd = CMD_SORTOLD; break;
    case CI_SIZE:
        cmd = CMD_SORTSIZE;
        break;
    case CI_DATE:
        cmd = CMD_SORTTIME;
        break;
    case CI_TIME:
        cmd = CMD_SORTTIME;
        break;
    case CI_PATH:
        cmd = CMD_SORTPATH;
        break;
    }
    return cmd;
}

void CPreviewWindow::SetItemCount(int count, DWORD flags, int state)
{
    CALL_STACK_MESSAGE4("CPreviewWindow::SetItemCount(%d, 0x%X, %d)", count,
                        flags, state);

    CachedItem = -1;

    int oldCount = ListView_GetItemCount(HWindow);
    if (oldCount == count) // aby to zbytecne neblikalo
    {
        InvalidateRect(HWindow, NULL, FALSE);
        UpdateWindow(HWindow);
    }
    else
        ListView_SetItemCountEx(HWindow, count, flags);

    if (count == 0)
    {
        int message;
        switch (state)
        {
        case 1:
            message = IDS_EMPTYMSG_NOMATCH;
            break;
        case 2:
            message = IDS_EMPTYMSG_BADMASK;
            break;
        case 3:
            message = IDS_EMPTYMSG_GENERATING;
            break;
        case 4:
            message = IDS_EMPTYMSG_ERROR;
            break;
        default:
            message = IDS_EMPTYMSG_DEF;
            break;
        }
        if (!Static)
        {
            RECT hr;
            GetWindowRect(ListView_GetHeader(HWindow), &hr);

            RECT cl;
            GetClientRect(HWindow, &cl);

            Static = ::CreateWindow(
                "Static",
                LoadStr(message),
                SS_LEFT | WS_VISIBLE | WS_CHILD, // style
                4, 4 + hr.bottom - hr.top, cl.right - 4, cl.bottom - (hr.bottom - hr.top) - 4,
                HWindow,
                (HMENU)666,
                DLLInstance,
                NULL);

            HFONT font = (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0);
            SendMessage(Static, WM_SETFONT, WPARAM(font), 0);
        }
        else
        {
            if (State != state)
                SetWindowText(Static, LoadStr(message));
        }
    }
    else
    {
        if (Static)
        {
            DestroyWindow(Static);
            Static = NULL;
        }
    }
    State = state;
}

LRESULT
CPreviewWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPreviewWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_CTLCOLORSTATIC:
        if (HWND(lParam) == Static)
        {
            SetTextColor((HDC)wParam, GetSysColor(COLOR_WINDOWTEXT));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
        break;

        // case WM_ERASEBKGND:
        // {
        //   if (ListView_GetItemCount(HWindow) > 0)
        //   {
        //     int top = ListView_GetTopIndex(HWindow);

        //     RECT ir;
        //     ListView_GetItemRect(HWindow, top, &ir, LVIR_BOUNDS);

        //     RECT cl;
        //     GetClientRect(HWindow, &cl);
        //     int right = cl.right;
        //     cl.right = ir.left;
        //     FillRect((HDC)wParam, &cl, (HBRUSH) (COLOR_WINDOW+1));

        //     cl.left = ir.right;
        //     cl.right = right;
        //     FillRect((HDC)wParam, &cl, (HBRUSH) (COLOR_WINDOW+1));
        //     return 1;
        //   }
        //   break;
        // }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
