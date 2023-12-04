// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

LPWSTR PatternHistory[MAX_HISTORY_ENTRIES];
LPWSTR LookInHistory[MAX_HISTORY_ENTRIES];

#ifdef _DEBUG
// statiskita cetnosti typu hodnot
DWORD TypeStat[12];
#endif

//*********************************************************************************
//
// CStatusBar
//

CStatusBar::CStatusBar()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CStatusBar::CStatusBar()");
    *Text = L'\0';
    BaseLen = 0;
    Dirty = 0;
    Width = 0;
    TextWidth = 0;
    Height = 0;
    HBitmap = NULL;
}

CStatusBar::~CStatusBar()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CStatusBar::~CStatusBar()");
    if (HBitmap != NULL)
        DeleteObject(HBitmap);
}

void CStatusBar::AllocateBitmap()
{
    CALL_STACK_MESSAGE1("CStatusBar::AllocateBitmap()");
    HDC screenDC = GetDC(HWindow);
    if (screenDC != NULL)
    {
        if (HBitmap != NULL)
            DeleteObject(HBitmap);
        HBitmap = (HBITMAP)CreateCompatibleBitmap(screenDC, Width, Height);
        // predkreslime si do bitmapy ramecek a sizing grip
        HDC dc = CreateCompatibleDC(screenDC);
        if (dc)
        {
            HBITMAP oldBmp = (HBITMAP)SelectObject(dc, HBitmap);
            RECT r;
            SetRect(&r, 0, 0, Width, Height);

            DrawEdge(dc, &r, BDR_SUNKENOUTER, BF_RECT);

            r.top++;
            r.bottom--;
            r.left = TextWidth;
            r.right--;

            DrawFrameControl(dc, &r, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
            HPEN pen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNFACE));
            HPEN oldPen = (HPEN)SelectObject(dc, pen);
            MoveToEx(dc, r.left + 2, r.bottom, NULL);
            LineTo(dc, r.right, r.bottom);
            LineTo(dc, r.right, r.bottom - GetSystemMetrics(SM_CYVSCROLL) + 3);
            SelectObject(dc, oldPen);
            DeleteObject(pen);

            SelectObject(dc, oldBmp);
            DeleteDC(dc);
        }
        ReleaseDC(HWindow, screenDC);
    }
}

void CStatusBar::SetBase(LPCWSTR text, BOOL updateInIdle)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CStatusBar::SetBase(, %d)", updateInIdle);
    Section.Enter();
    lstrcpynW(Text, text, MAX_FULL_KEYNAME + 50);
    BaseLen = (int)wcslen(Text);
    // neceka se na prekresleni
    if (!Dirty)
    {
        if (updateInIdle)
            UpdateInIdle = TRUE;
        else
        {
            Dirty = TRUE;
            RECT r;
            SetRect(&r, 1, 1, TextWidth, Height - 1);
            InvalidateRect(HWindow, &r, FALSE);
            PostMessage(HWindow, WM_PAINT, 0, 0);
        }
    }
    Section.Leave();
}

void CStatusBar::Set(LPCWSTR text, BOOL updateInIdle)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CStatusBar::Set(, %d)", updateInIdle);
    Section.Enter();
    lstrcpynW(Text + BaseLen, text, MAX_FULL_KEYNAME + 49 - BaseLen);
    // neceka se na prekresleni
    if (!Dirty)
    {
        if (updateInIdle)
            UpdateInIdle = TRUE;
        else
        {
            Dirty = TRUE;
            RECT r;
            SetRect(&r, 1, 1, TextWidth, Height - 1);
            InvalidateRect(HWindow, &r, FALSE);
            PostMessage(HWindow, WM_PAINT, 0, 0);
        }
    }
    Section.Leave();
}

void CStatusBar::OnEnterIdle()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CStatusBar::OnEnterIdle()");
    Section.Enter();
    // ma se v idle provadet update?
    if (UpdateInIdle)
    {
        UpdateInIdle = FALSE;
        // neceka se na prekresleni
        if (!Dirty)
        {
            Dirty = TRUE;
            RECT r;
            SetRect(&r, 1, 1, TextWidth, Height - 1);
            InvalidateRect(HWindow, &r, FALSE);
            PostMessage(HWindow, WM_PAINT, 0, 0);
        }
    }
    Section.Leave();
}

LRESULT
CStatusBar::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CStatusBar::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    switch (uMsg)
    {
    case WM_SIZE:
    {
        Width = LOWORD(lParam);
        TextWidth = Width - GetSystemMetrics(SM_CXHSCROLL) + 1;
        Height = HIWORD(lParam);
        AllocateBitmap();
        return 0;
    }

    case WM_NCHITTEST:
    {
        RECT r;
        POINT pt;
        pt.x = TextWidth;
        pt.y = 0; //Height - GetSystemMetrics(SM_CYVSCROLL);
        ClientToScreen(HWindow, &pt);
        ;
        r.left = pt.x;
        r.top = pt.y;
        pt.x = Width;
        pt.y = Height;
        ClientToScreen(HWindow, &pt);
        ;
        r.right = pt.x;
        r.bottom = pt.y;

        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);
        if (PtInRect(&r, pt))
            return HTBOTTOMRIGHT;
        break;
    }

    case WM_ERASEBKGND:
    {
        return TRUE;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(HWindow, &ps);
        if (HBitmap)
        {
            HDC dc = CreateCompatibleDC(ps.hdc);
            if (dc)
            {
                HBITMAP oldBmp = (HBITMAP)SelectObject(dc, HBitmap);

                HFONT oldFont = (HFONT)SelectObject(dc, EnvFont);
                SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
                SetBkColor(dc, GetSysColor(COLOR_BTNFACE));

                Section.Enter();
                RECT r;
                SetRect(&r, 1, 1, TextWidth, Height - 1);
                ExtTextOutW(dc, 2, (r.top + r.bottom - EnvFontHeight) / 2, ETO_OPAQUE | ETO_CLIPPED, &r, Text, (UINT)wcslen(Text), NULL);
                Dirty = FALSE;
                Section.Leave();

                SelectObject(dc, oldFont);

                BitBlt(ps.hdc, 0, 0, Width, Height, dc, 0, 0, SRCCOPY);
                SelectObject(dc, oldBmp);
                DeleteDC(dc);
            }
        }

        EndPaint(HWindow, &ps);

        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//*********************************************************************************
//
// CFoundFilesData
//

LPCWSTR Bin2ASCIIW = L"0123456789abcdef";

void PrintHexValueW(unsigned char* data, int size, LPWSTR buffer, int bufSize)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE3("PrintHexValueW(, %d, , %d)", size, bufSize);
    int i;
    for (i = 0; i < min(size, (bufSize) / 3); i++)
    {
        buffer[i * 3] = Bin2ASCIIW[data[i] >> 4];
        buffer[i * 3 + 1] = Bin2ASCIIW[data[i] & 0x0F];
        if (i + 1 < min(size, (bufSize) / 3))
            buffer[i * 3 + 2] = L' ';
    }
    if (i != size)
    {
        // neveslo se, doplnime elipsou
        wcscpy(buffer + bufSize - 4, L"...");
    }
    else
        buffer[i * 3 - 1] = L'\0';
}

CFoundFilesData::CFoundFilesData(LPWSTR name, int root, LPWSTR key, DWORD type,
                                 DWORD size, unsigned char* data,
                                 FILETIME time, BOOL isDir)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE5("CFoundFilesData::CFoundFilesData(, %d, , 0x%X, 0x%X, , , "
    //                      "%d)", root, type, size, isDir);
    Name = DupStr(*name == L'\0' ? LoadStrW(IDS_DEFAULTVALUE) : name);
    WCHAR path[MAX_KEYNAME];
    PathAppend(wcscpy(path, PredefinedHKeys[root].KeyName), key, MAX_KEYNAME);
    Path = DupStr(path);
    Type = type;
    Size = size;
    Data = NULL;
    Allocated = 0;
    Time = time;
    IsDir = isDir;
    Default = *name == L'\0' ? 1 : 0;

    if (!IsDir && data)
    {
        // upravime data pro zobrazeni
        switch (type)
        {
        case REG_MULTI_SZ:
        {
            // nahradime separacni NULL charactery mezerama
            WCHAR* ptr = (WCHAR*)data;
            while (ptr < (WCHAR*)data + min(size / 2, MAX_DATASIZE))
            {
                if (*ptr == L'\0')
                {
                    if (ptr + 2 >= (WCHAR*)data + size / 2)
                    {
                        size -= 2; // nepozitame posledni '\0', jsou tam dva
                        break;
                    }
                    else
                    {
                        *ptr = L' ';
                    }
                }
                ptr++;
            }
            // pokracujem dal
        }
        case REG_EXPAND_SZ:
        case REG_SZ:
        {
            DWORD truncated = min(size, MAX_DATASIZE * 2);
            if (size)
            {
                Allocated = 1;
                Data = (DWORD_PTR)malloc(truncated);
                memcpy((void*)Data, data, truncated);
                if (truncated < size)
                    wcscpy((LPWSTR)Data + truncated / 2 - 4, L"...");
            }
            break;
        }

        case REG_QWORD:
            if (size > 0)
            {
                Allocated = 1;
                Data = (DWORD_PTR)malloc(8);
                memcpy((void*)Data, data, 8);
            }
            break;

        case REG_DWORD:
            Data = *(LPDWORD)data;
            break;

        case REG_DWORD_BIG_ENDIAN:
            Data = (DWORD)data[3] | (data[2] << 8) | (data[1] << 16) | (data[0] << 24);
            break;

        default:
            if (size > 0)
            {
                Allocated = 1;
                int hexsize = (unsigned char)min(size * 3 - 1, MAX_DATASIZE) + 1;
                Data = (DWORD_PTR)malloc(hexsize * 2);
                PrintHexValueW(data, size, (LPWSTR)Data, hexsize);
            }
        }
    }
}

/*
CFoundFilesData::CFoundFilesData(CFoundFilesData & orig)
{
  Name = orig.Name;
  Path = orig.Path;
  Type = orig.Type;
  Size = orig.Size;
  Data = orig.Data;
  Allocated = orig.Allocated;
  Time = orig.Time;
  IsDir = orig.IsDir;
  orig.Name = NULL;
  orig.Path = NULL;
  orig.Data = 0;
  orig.Data = NULL;
}
*/

/*
BOOL 
|CFoundFilesData::Set(const char * name, const char * volume, const char * path,
                     QWORD size, FILETIME time, DWORD attributes, BOOL isDir)
{
  if ((Name = SG->DupStr(name)) == NULL) return FALSE;
  if ((Volume = SG->DupStr(volume)) == NULL) return FALSE;
  if ((Path = SG->DupStr(path)) == NULL) return FALSE;
  Size = size;
  Time = time;
  Attributes = attributes;
  IsDir = isDir;
  return TRUE;
}
*/

LPWSTR
CFoundFilesData::GetText(int i, LPWSTR buffer)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CFoundFilesData::GetText(%d, )", i);
    char buf[100];
    static wchar_t emptyBuffer[] = L"";
    LPWSTR ret = emptyBuffer; //"?";
    switch (i)
    {
    case CI_NAME:
        ret = Name;
        break;
    case CI_PATH:
        ret = Path;
        break;

    case CI_SIZE:
    {
        if (IsDir)
        {
            StrToWStr(buffer, 100, KeyText);
            ret = buffer;
        }
        else
        {
            SG->NumberToStr(buf, CQuadWord().Set(Size, 0));
            StrToWStr(buffer, 100, buf);
            ret = buffer;
        }
        break;
    }

    case CI_DATE:
    {
        SYSTEMTIME st;
        FileTimeToSystemTime(&Time, &st);
        SalPrintfW(buffer, 100, L"%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        ret = buffer;
        break;
    }

    case CI_TIME:
    {
        SYSTEMTIME st;
        FileTimeToSystemTime(&Time, &st);
        SalPrintfW(buffer, 100, L"%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        ret = buffer;
        break;
    }

    case CI_TYPE:
    {
        if (!IsDir)
        {
            // jeto hodnota vypiseme typ
            switch (Type)
            {
            case REG_BINARY:
                StrToWStr(buffer, 100, Str_REG_BINARY);
                break;
            case REG_DWORD:
                StrToWStr(buffer, 100, Str_REG_DWORD);
                break;
            case REG_DWORD_BIG_ENDIAN:
                StrToWStr(buffer, 100, Str_REG_DWORD_BIG_ENDIAN);
                break;
            case REG_QWORD:
                StrToWStr(buffer, 100, Str_REG_QWORD);
                break;
            case REG_EXPAND_SZ:
                StrToWStr(buffer, 100, Str_REG_EXPAND_SZ);
                break;
            case REG_LINK:
                StrToWStr(buffer, 100, Str_REG_LINK);
                break;
            case REG_MULTI_SZ:
                StrToWStr(buffer, 100, Str_REG_MULTI_SZ);
                break;
            case REG_NONE:
                StrToWStr(buffer, 100, Str_REG_NONE);
                break;
            case REG_RESOURCE_LIST:
                StrToWStr(buffer, 100, Str_REG_RESOURCE_LIST);
                break;
            case REG_SZ:
                StrToWStr(buffer, 100, Str_REG_SZ);
                break;
            case REG_FULL_RESOURCE_DESCRIPTOR:
                StrToWStr(buffer, 100, Str_REG_FULL_RESOURCE_DESCRIPTOR);
                break;
            case REG_RESOURCE_REQUIREMENTS_LIST:
                StrToWStr(buffer, 100, Str_REG_RESOURCE_REQUIREMENTS_LIST);
                break;
            default:
                TRACE_E("unknown value type");
                wcscpy(buffer, L"?");
            }
            ret = buffer;
        }
        break;
    }

    case CI_DATA:
    {
        if (!IsDir)
        {
            if (Size > 0)
            {
                switch (Type)
                {
                case REG_MULTI_SZ:
                case REG_EXPAND_SZ:
                case REG_SZ:
                    if (Data)
                        ret = (LPWSTR)Data;
                    break;

                case REG_DWORD_BIG_ENDIAN:
                case REG_DWORD:
                    if (Size == 4)
                    {
                        SalPrintfW(buffer, 100, L"0x%08x (%u)", Data, Data);
                        ret = buffer;
                    }
                    break;

                case REG_QWORD:
                    if (Size == 8)
                    {
                        SalPrintfW(buffer, 100, L"0x%016I64x (%I64u)", *(LPQWORD)Data, *(LPQWORD)Data);
                        ret = buffer;
                    }
                    break;

                default:
                    if (Data)
                        ret = (LPWSTR)Data;
                }
            }
        }
        break;
    }
    }

    return ret;
}

//*********************************************************************************
//
// CFoundFilesListView
//

CFoundFilesListView::CFoundFilesListView(CFindDialog* searchDialog)
    : Data(100, 100)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFoundFilesListView::CFoundFilesListView()");
    SearchDialog = searchDialog;
}

CFoundFilesListView::~CFoundFilesListView()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFoundFilesListView::~CFoundFilesListView()");
}

void CFoundFilesListView::StoreItemsState()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFoundFilesListView::StoreItemsState()");
    int count = GetCount();
    int i;
    for (i = 0; i < count; i++)
        Data[i]->State = ListView_GetItemState(HWindow, i, LVIS_FOCUSED | LVIS_SELECTED);
}

void CFoundFilesListView::RestoreItemsState()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFoundFilesListView::RestoreItemsState()");
    int count = GetCount();
    int i;
    for (i = 0; i < count; i++)
        ListView_SetItemState(HWindow, i, Data[i]->State, LVIS_FOCUSED | LVIS_SELECTED);
}

int CFoundFilesListView::CompareFunc(CFoundFilesData* f1, CFoundFilesData* f2, int sortBy)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CFoundFilesListView::CompareFunc(, , %d)", sortBy);
    int res;
    int next = 0, sortByIndex;
    static int sortKeys[] = {CI_NAME, CI_PATH, CI_TYPE, CI_TIME, CI_SIZE};
    switch (sortBy)
    {
    case CI_NAME:
        next = 0;
        break;
    case CI_PATH:
        next = 1;
        break;
    case CI_TYPE:
        next = 2;
        break;
    case CI_DATE:
        next = 3;
        break;
    case CI_TIME:
        next = 3;
        break;
    case CI_SIZE:
        next = 4;
        break;
    }
    sortByIndex = next;
    do
    {
        switch (sortKeys[next])
        {
        case CI_NAME:
        {
            if (f1->IsDir == f2->IsDir)
            {
                res = CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, f1->Name, -1, f2->Name, -1) - 2;
                if (res == 0)
                    res = CompareStringW(LOCALE_USER_DEFAULT, 0, f1->Name, -1, f2->Name, -1) - 2;
            }
            else
                res = f1->IsDir ? -1 : 1;
            break;
        }

        case CI_PATH:
        {
            res = CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, f1->Path, -1, f2->Path, -1) - 2;
            if (res == 0)
                res = CompareStringW(LOCALE_USER_DEFAULT, 0, f1->Path, -1, f2->Path, -1) - 2;
            break;
        }

        case CI_TYPE:
        {
            if (f1->IsDir == f2->IsDir)
            {
                if (f1->IsDir)
                    res = 0;
                else
                {
                    if (f1->Type < f2->Type)
                        res = -1;
                    else
                    {
                        if (f1->Type == f2->Type)
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
                res = CompareFileTime(&f1->Time, &f2->Time);
            else
                res = f1->IsDir ? -1 : 1;
            break;
        }
        }
        if (next == sortByIndex)
        {
            if (sortBy != 0)
                next = 0;
            else
                next = 1;
        }
        else if (next + 1 != sortByIndex)
            next++;
        else
            next += 2;
    } while (res == 0 && next <= 4);

    return res;
}

void CFoundFilesListView::QuickSort(int left, int right, int sortBy)
{
    CALL_STACK_MESSAGE_NONE

LABEL_QuickSort:

    //  CALL_STACK_MESSAGE4("CFoundFilesListView::QuickSort(%d, %d, %d)", left,
    //                      right, sortBy);  // Petr: prilis pomaly call-stack
    int i = left, j = right;
    CFoundFilesData* pivot = Data[(i + j) / 2];

    do
    {
        while (CompareFunc(Data[i], pivot, sortBy) < 0 && i < right)
            i++;
        while (CompareFunc(pivot, Data[j], sortBy) < 0 && j > left)
            j--;

        if (i <= j)
        {
            CFoundFilesData* swap = Data[i];
            Data[i] = Data[j];
            Data[j] = swap;
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

void CFoundFilesListView::SortItems(int sortBy)
{
    CALL_STACK_MESSAGE2("CFoundFilesListView::SortItems(%d)", sortBy);
    HCURSOR hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    DataCriticalSection.Enter();

    // pokud mame nejake polozky v datech a nejsou v listview, preneseme je
    SearchDialog->UpdateListViewItems();

    if (Data.Count > 0)
    {
        // ulozime stav slected a focused polozek
        StoreItemsState();

        // seradim pole podle pozadovaneho kriteria
        QuickSort(0, Data.Count - 1, sortBy);

        RestoreItemsState();
        int focusIndex = ListView_GetNextItem(HWindow, -1, LVNI_FOCUSED);
        if (focusIndex != -1)
            ListView_EnsureVisible(HWindow, focusIndex, FALSE);
        ListView_RedrawItems(HWindow, 0, Data.Count - 1);
        UpdateWindow(HWindow);
    }

    DataCriticalSection.Leave();
    SetCursor(hCursor);
}

CFoundFilesData*
CFoundFilesListView::At(int index)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CFoundFilesListView::At(%d)", index);  // Petr: prilis pomaly call-stack
    CFoundFilesData* ptr;
    DataCriticalSection.Enter();
    ptr = Data[index];
    DataCriticalSection.Leave();
    return ptr;
}

void CFoundFilesListView::DestroyMembers()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFoundFilesListView::DestroyMembers()");
    //  DataCriticalSection.Enter();
    Data.DestroyMembers();
    //  DataCriticalSection.Leave();
}

int CFoundFilesListView::GetCount()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFoundFilesListView::GetCount()");
    int count;
    DataCriticalSection.Enter();
    count = Data.Count;
    DataCriticalSection.Leave();
    return count;
}

int CFoundFilesListView::Add(CFoundFilesData* item)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFoundFilesListView::Add()"); // Petr: prilis pomaly call-stack
    int index;
    DataCriticalSection.Enter();
    index = Data.Add(item);
    DataCriticalSection.Leave();
    return index;
}

BOOL CFoundFilesListView::IsGood()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFoundFilesListView::IsGood()");
    BOOL isGood;
    DataCriticalSection.Enter();
    isGood = Data.IsGood();
    DataCriticalSection.Leave();
    return isGood;
}

BOOL CFoundFilesListView::InitColumns()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFoundFilesListView::InitColumns()");

    LV_COLUMN lvc;
    int header[] = {IDS_NAME_COLUMN, IDS_TYPE_COLUMN, IDS_DATA_COLUMN, IDS_PATH_COLUMN,
                    IDS_SIZE_COLUMN, IDS_DATE_COLUMN, IDS_TIME_COLUMN};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; i < 7; i++) // vytvorim sloupce
    {
        lvc.pszText = (LPSTR)LoadStr(header[i]);
        lvc.iSubItem = i;
        if (ListView_InsertColumn(HWindow, i, &lvc) == -1)
            return FALSE;
    }

    RECT r;
    GetClientRect(HWindow, &r);
    DWORD cx = r.right - r.left + (1 ? -1 : 1);
    ListView_SetColumnWidth(HWindow, CI_TIME, ListView_GetStringWidth(HWindow, "00:00:00") + 20);
    ListView_SetColumnWidth(HWindow, CI_DATE, ListView_GetStringWidth(HWindow, "00.00.0000") + 20);
    ListView_SetColumnWidth(HWindow, CI_SIZE, ListView_GetStringWidth(HWindow, "000000") + 20);
    ListView_SetColumnWidth(HWindow, CI_PATH, ListView_GetStringWidth(HWindow, "X") * 16 + 20);
    ListView_SetColumnWidth(HWindow, CI_TYPE, ListView_GetStringWidth(HWindow, "EXPAND_SZ") + 20);
    ListView_SetColumnWidth(HWindow, CI_NAME, 20 + ListView_GetStringWidth(HWindow, "X") * 8 + 20);
    cx -= ListView_GetColumnWidth(HWindow, CI_NAME) + ListView_GetColumnWidth(HWindow, CI_TYPE) +
          ListView_GetColumnWidth(HWindow, CI_PATH) // + ListView_GetColumnWidth(HWindow, CI_SIZE) +
          //ListView_GetColumnWidth(HWindow, CI_DATE) + ListView_GetColumnWidth(HWindow, CI_TIME)
          //+ ListView_GetColumnWidth(HWindow, CI_ATTRIBUTES)
          + GetSystemMetrics(SM_CXHSCROLL) - 1;
    ListView_SetColumnWidth(HWindow, CI_DATA, cx);
    ListView_SetImageList(HWindow, ImageList, LVSIL_SMALL);

    return TRUE;
}

void CFoundFilesListView::GetContextMenuPos(POINT* p)
{
    CALL_STACK_MESSAGE1("CFoundFilesListView::GetContextMenuPos()");
    if (ListView_GetItemCount(HWindow) == 0)
    {
        p->x = 0;
        p->y = 0;
        ClientToScreen(HWindow, p);
        return;
    }
    int focIndex = ListView_GetNextItem(HWindow, -1, LVNI_FOCUSED);
    if (focIndex != -1)
    {
        if ((ListView_GetItemState(HWindow, focIndex, LVNI_SELECTED) & LVNI_SELECTED) == 0)
            focIndex = ListView_GetNextItem(HWindow, -1, LVNI_SELECTED);
    }
    RECT cr;
    GetClientRect(HWindow, &cr);
    RECT r;
    ListView_GetItemRect(HWindow, 0, &r, LVIR_LABEL);
    p->x = r.left;
    if (p->x < 0)
        p->x = 0;
    if (focIndex != -1)
        ListView_GetItemRect(HWindow, focIndex, &r, LVIR_BOUNDS);
    if (focIndex == -1 || r.bottom < 0 || r.bottom > cr.bottom)
        r.bottom = 0;
    p->y = r.bottom;
    ClientToScreen(HWindow, p);
}

LRESULT
CFoundFilesListView::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CFoundFilesListView::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_GETDLGCODE:
    {
        if (lParam != NULL)
        {
            // pokud jde o Enter, tak ho chceme zpracovat
            MSG* msg = (LPMSG)lParam;
            if (msg->message == WM_KEYDOWN && msg->wParam == VK_RETURN)
                return DLGC_WANTMESSAGE;
        }
        return DLGC_WANTCHARS | DLGC_WANTARROWS;
    }

    case WM_MOUSEACTIVATE:
    {
        // pokud je Find neaktivni a uzivatel chce pres drag&drop odtahnout
        // nekterou z polozek, nesmi Find vyskocit nahoru
        return MA_NOACTIVATE;
    }

    case WM_SETFOCUS:
    {
        SendMessage(GetParent(HWindow), WM_USER_BUTTONS, 0, 1);
        break;
    }

    case WM_KILLFOCUS:
    {
        HWND next = (HWND)wParam;
        BOOL nextIsButton;
        if (next != NULL)
        {
            char className[30];
            WORD wl = LOWORD(GetWindowLong(next, GWL_STYLE)); // jen BS_...
            nextIsButton = (GetClassName(next, className, 30) != 0 &&
                            SG->StrICmp(className, "BUTTON") == 0 &&
                            (wl == BS_PUSHBUTTON || wl == BS_DEFPUSHBUTTON));
        }
        else
            nextIsButton = FALSE;
        SendMessage(GetParent(HWindow), WM_USER_BUTTONS, nextIsButton ? wParam : 0, 0);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

/*
//******************************************************************************
//
// CComboboxEdit
//

CComboboxEdit::CComboboxEdit()
 : CWindow(ooAllocated)
{
    SelStart = 0;
    SelEnd = -1;
}

LRESULT
CComboboxEdit::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CALL_STACK_MESSAGE4("CComboboxEdit::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
  switch (uMsg)
  {
    case WM_KILLFOCUS:
    {
      SendMessage(HWindow, EM_GETSEL, (WPARAM)&SelStart, (LPARAM)&SelEnd);
      break;
    }

    case EM_REPLACESEL:
    {
      LRESULT res = CWindow::WindowProc(uMsg, wParam, lParam);
      SendMessage(HWindow, EM_GETSEL, (WPARAM)&SelStart, (LPARAM)&SelEnd);
      return res;
    }
  }
  return CWindow::WindowProc(uMsg, wParam, lParam);
}

void
CComboboxEdit::GetSel(DWORD *start, DWORD *end)
{
  if (GetFocus() == HWindow)
    SendMessage(HWindow, EM_GETSEL, (WPARAM)start, (LPARAM)end);
  else
  {
    *start = SelStart;
    *end = SelEnd;
  }
}

void
CComboboxEdit::ReplaceText(const char *text)
{
  // musime ozivit selection, protoze dementni combobox ji zapomel
  SendMessage(HWindow, EM_SETSEL, SelStart, SelEnd);
  SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM)text);
}

  */

//*********************************************************************************
//
// CFindThread
//

BOOL CFindThread::TestTime(FILETIME& ft)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFindThread::TestTime()");  // Petr: prilis pomaly call-stack

    if (!UseMinTime && !UseMaxTime)
        return TRUE;

    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    if (UseMinTime)
    {
        if (st.wYear < MinTime.wYear ||
            st.wYear == MinTime.wYear &&
                (st.wMonth < MinTime.wMonth ||
                 st.wMonth == MinTime.wMonth &&
                     (st.wDay < MinTime.wDay ||
                      st.wDay == MinTime.wDay &&
                          (st.wHour < MinTime.wHour ||
                           st.wHour == MinTime.wHour &&
                               (st.wMinute < MinTime.wMinute ||
                                st.wMinute == MinTime.wMinute &&
                                    (st.wSecond < MinTime.wSecond))))))
        {
            return FALSE;
        }
    }
    if (UseMaxTime)
    {
        if (st.wYear > MaxTime.wYear ||
            st.wYear == MaxTime.wYear &&
                (st.wMonth > MaxTime.wMonth ||
                 st.wMonth == MaxTime.wMonth &&
                     (st.wDay > MaxTime.wDay ||
                      st.wDay == MaxTime.wDay &&
                          (st.wHour > MaxTime.wHour ||
                           st.wHour == MaxTime.wHour &&
                               (st.wMinute > MaxTime.wMinute ||
                                st.wMinute == MaxTime.wMinute &&
                                    (st.wSecond > MaxTime.wSecond))))))
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL CFindThread::Test(char* text, int len, BOOL name, DWORD type)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE4("CFindThread::Test(, %d, %d, 0x%X)", len, name, type);
    // prazdny retezec je obsazen v kazdem retezu
    if (PatternALen == 0 && PatternWLen == 0)
        return TRUE;

    if (RegExp)
    {
        // prelozime retezec na ascii
        if (name || type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ)
        {
            if (!name)
            {
                if (type == REG_SZ || type == REG_EXPAND_SZ)
                {
                    len = max(0, len - 2); // orizneme zakoncovaci NULL
                }
                else
                {
                    len = max(0, len - 4); // orizneme zakoncovaci NULL
                }
            }

            len /= 2;

            if (len > 0)
            {
                if (!AsciiBuffer.Reserve(len))
                {
                    TRACE_E("LOW_MEMORY");
                    return FALSE;
                }
                WStrToStr(AsciiBuffer.Get(), AsciiBuffer.GetSize(), (LPWSTR)text, len);
                text = AsciiBuffer.Get();
            }
        }

        // rozparsujem jednotlive radky a otestujem regexp
        char* start;
        char* end = text;
        do
        {
            start = end;

            while (end < text + len && *end != '\n' && *end != '\r' && *end != '\0')
                end++;

            // otestujem radku
            SalRegExp->SetLine(start, end);
            int pos = 0;
            do
            {
                int matchLen;
                if ((pos = SalRegExp->SearchForward(pos, matchLen)) != -1)
                {
                    if (!WholeWords ||
                        matchLen > 0 && (pos == 0 || isspace(text[pos - 1])) &&
                            (pos + matchLen == end - start || isspace(text[pos + matchLen])))
                        return TRUE;
                    pos++; // zkusime to o kousek dal, treba to vyjde
                }
                else
                    break;
            } while (pos < end - start);

            // pro CRLF je nutne preskocit dva znaky
            if (end + 1 < text + len && end[0] == '\r' && end[1] == '\n')
                end++;

            // prejdem na zatek dalsi radky
            end++;
        } while (end < text + len);
    }
    else
    {
        if (name)
        {
            // jmeno prohledavame jen proti unicodovemu vzoru, v pripade hex
            // searche je unicode vzor stejny jako ASCII
            int pos = 0;
            do
            {
                if ((pos = BMForPatternW->SearchForward(text, len, pos)) != -1)
                {
                    if (!WholeWords ||
                        (pos < 2 || iswspace(*(LPWSTR)(text + pos - 2))) &&
                            (pos + PatternWLen > len - 2 || iswspace(*(LPWSTR)(text + pos + PatternWLen))))
                        return TRUE;
                    pos += 2; // zkusime to o kousek dal, treba to vyjde
                }
                else
                    break;
            } while (pos <= len - PatternWLen);
        }
        else
        {
            switch (type)
            {
            case REG_MULTI_SZ:
            case REG_EXPAND_SZ:
            case REG_SZ:
            {
                // SZ prohledavame jen proti unicodovemu vzoru, v pripade hex
                // searche je unicode vzor stejny jako ASCII
                int pos = 0;
                do
                {
                    if ((pos = BMForPatternW->SearchForward(text, len, pos)) != -1)
                    {
                        if (!WholeWords ||
                            (pos < 2 || iswspace(*(LPWSTR)(text + pos - 2))) &&
                                (pos + PatternWLen > len - 2 || iswspace(*(LPWSTR)(text + pos + PatternWLen))))
                            return TRUE;
                        pos += 2; // zkusime to o kousek dal, treba to vyjde
                    }
                    else
                        break;
                } while (pos <= len - PatternWLen);
                break;
            }

            case REG_DWORD_BIG_ENDIAN:
                // cisla porovnaname s ciselnou hodnotou
                if (UseNumber)
                    return (DWORD)(text[3] | (text[2] << 8) | (text[1] << 16) | (text[0] << 24)) == Number;
                break;

            case REG_DWORD:
                // cisla porovnaname s ciselnou hodnotou
                if (UseNumber)
                    return *(LPDWORD)text == Number;
                break;

            case REG_QWORD:
                // cisla porovnaname s ciselnou hodnotou
                if (UseNumber)
                    return *(LPQWORD)text == Number;
                break;

            default:
            {
                // v binarnich hodnotach zkousime hledat jak unicode a tak ASCI vzor
                int pos = 0;
                do
                {
                    if ((pos = BMForPatternW->SearchForward(text, len, pos)) != -1)
                    {
                        if (!WholeWords ||
                            (pos < 2 || iswspace(*(LPWSTR)(text + pos - 2))) &&
                                (pos + PatternWLen > len - 2 || iswspace(*(LPWSTR)(text + pos + PatternWLen))))
                            return TRUE;
                        pos += 2; // zkusime to o kousek dal, treba to vyjde
                    }
                    else
                        break;
                } while (pos <= len - PatternWLen);

                if (BMForPatternA != BMForPatternW)
                {
                    // jeste zkusime ASCII vzor
                    do
                    {
                        if ((pos = BMForPatternA->SearchForward(text, len, pos)) != -1)
                        {
                            if (!WholeWords ||
                                (pos == 0 || isspace(text[pos - 1])) &&
                                    (pos + PatternALen == len || isspace(text[pos + PatternALen])))
                                return TRUE;
                            pos++; // zkusime to o kousek dal, treba to vyjde
                        }
                        else
                            break;
                    } while (pos <= len - PatternALen);
                }
                break;
            }
            }
        }
    }
    return FALSE;
}

BOOL CFindThread::ScanKeyAux(int root, LPWSTR key, BOOL& skip, BOOL& skipAllErrors,
                             LPWSTR nameBuffer, TIndirectArray<WCHAR>& stack)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE4("CFindThread::ScanKeyAux(%d, , %d, %d, , )", root, skip,
    //                           skipAllErrors);
    // status text
    if ((int)(GetTickCount() - NextStatusUpdate) > 0)
    {
        FindDialog->StatusBar->Set(key);
        NextStatusUpdate = GetTickCount() + 1;
    }

    // test na preruseni uzivatelem
    if (WaitForSingleObject(CancelEvent, 0) == WAIT_OBJECT_0)
        return skip = FALSE;

    HKEY hKey; // aktualni prohledavany klic

    // otevreme prohledavany klic
    if (!SafeOpenKey(root, key, KEY_READ, hKey, IDS_SEARCHERROR, &skip, &skipAllErrors))
        return FALSE;

    // otestujeme hodnoty
    FILETIME time, localFileTime;
    DWORD maxData;
    void* data = NULL;

    // nacteme si max data a cas klice
    if (!SafeQueryInfoKey(hKey, root, key, NULL, &maxData, &time, IDS_SEARCHERROR, skip, skipAllErrors))
    {
        RegCloseKey(hKey);
        return FALSE;
    }
    FileTimeToLocalFileTime(&time, &localFileTime);

    if (LookAtData)
    {
        // MS nekdy vraci polovicni velikost (pozorovano na MULTI_SZ v
        // klici HKEY_LOCAL_MACHINE\SYSTEM\ControlSet002\Services\NetBT\Linkage
        maxData *= 2;
        data = malloc(maxData);
        if (!data)
        {
            skip = FALSE;
            RegCloseKey(hKey);
            return Error(IDS_LOWMEM);
        }
    }

    DWORD index = 0;
    LPWSTR name = nameBuffer;
    DWORD type, size;
    BOOL noMore = TRUE;
    BOOL someFileSkipped = FALSE;

    if ((LookAtValues || LookAtData) && TestTime(localFileTime))
    {
        while (SafeEnumValue(hKey, root, key, index, name, type, data, size = maxData,
                             IDS_SEARCHERROR, skip, skipAllErrors, noMore) ||
               skip)
        {
            if (!skip)
            {
                if ((LookAtValues && Test((LPSTR)name, (int)wcslen(name) * 2, TRUE, 0) ||
                     LookAtData && Test((LPSTR)data, size, FALSE, type)))
                {
#ifdef _DEBUG
                    // statiskita cetnosti typu hodnot
                    if (type >= 0 && type < 12) // Petr: mam na masine value s type==0x397e7d0 (HKEY_CURRENT_USER\Software\Microsoft\Internet Explorer\Explorer Bars\{32683183-48a0-441b-a342-7c2a440a9478}\BarSize)
                        TypeStat[type]++;
#endif

                    // pridame polozku do listview

                    CFoundFilesData* fd =
                        new CFoundFilesData(name,
                                            root, key, type, size, (unsigned char*)data,
                                            localFileTime, FALSE);
                    if (!fd || FindDialog->List->Add(fd) == ULONG_MAX)
                    {
                        if (fd)
                            delete fd;
                        if (data)
                            free(data);
                        skip = FALSE;
                        RegCloseKey(hKey);
                        return Error(IDS_LOWMEM);
                    }
                    // po kazdych 100 pridanych polozkach pozadam listview o prekresleni
                    if (FindDialog->FoundVisibleCount + 100 < FindDialog->List->GetCount())
                        PostMessage(FindDialog->HWindow, WM_USER_ADDFILE, 0, 0);
                }
            }
            else
                skip = FALSE;

            // test na preruseni uzivatelem
            if (WaitForSingleObject(CancelEvent, 0) == WAIT_OBJECT_0)
            {
                if (data)
                    free(data);
                RegCloseKey(hKey);
                return skip = FALSE;
            }
        }
    }

    if (data)
        free(data);

    if (!noMore)
    {
        RegCloseKey(hKey);
        return FALSE;
    }

    // jeste rekurzivne prohledame podklice

    // nejprve naenumerujeme vsechny klice na stack
    int len = (int)wcslen(key);
    int maxSubkey = MAX_KEYNAME - len - 2;
    index = 0;
    int top = stack.Count; // ulozime si ukazatel na vrsek zasobniku
    while (SafeEnumKey(hKey, root, key, index, name, &time,
                       IDS_SEARCHERROR, skip, skipAllErrors, noMore) ||
           skip)
    {
        if (!skip)
        {
            int nameLen = (int)wcslen(name);
            if (nameLen > maxSubkey)
            {
                if (skipAllErrors)
                {
                    someFileSkipped = TRUE;
                    continue;
                }

                char nameA[MAX_KEYNAME];
                WStrToStr(nameA, MAX_KEYNAME, name);

                int res = SG->DialogError(GetParent(), BUTTONS_SKIPCANCEL, nameA, LoadStr(IDS_LONGNAME), LoadStr(IDS_SEARCHERROR));
                switch (res)
                {
                case DIALOG_SKIPALL:
                    skipAllErrors = TRUE;
                case DIALOG_SKIP:
                    continue;

                default:
                    skip = FALSE;
                    RegCloseKey(hKey);
                    return FALSE; // DIALOG_CANCEL
                }
            }

            FileTimeToLocalFileTime(&time, &localFileTime);
            if (LookAtKeys && TestTime(localFileTime) && Test((LPSTR)name, (int)wcslen(name) * 2, TRUE, 0))
            {
                // pridame klic do vysledku
                CFoundFilesData* fd =
                    new CFoundFilesData(name, root, key, 0, 0, NULL, localFileTime, TRUE);
                if (!fd || FindDialog->List->Add(fd) == ULONG_MAX)
                {
                    if (fd)
                        delete fd;
                    skip = FALSE;
                    RegCloseKey(hKey);
                    return Error(IDS_LOWMEM);
                }
                // po kazdych 100 pridanych polozkach pozadam listview o prekresleni
                if (FindDialog->FoundVisibleCount + 100 < FindDialog->List->GetCount())
                    PostMessage(FindDialog->HWindow, WM_USER_ADDFILE, 0, 0);
            }

            if (IncludeSubkeys)
            {
                LPWSTR ptr = new WCHAR[nameLen + 1];
                if (!ptr || stack.Add(ptr) == ULONG_MAX)
                {
                    if (ptr)
                        delete[] ptr;
                    skip = FALSE;
                    RegCloseKey(hKey);
                    return Error(IDS_LOWMEM);
                }
                wcscpy(ptr, name);
            }
        }
        else
            skip = FALSE;

        // test na preruseni uzivatelem
        if (WaitForSingleObject(CancelEvent, 0) == WAIT_OBJECT_0)
        {
            RegCloseKey(hKey);
            return skip = FALSE;
        }
    }

    if (!noMore)
    {
        RegCloseKey(hKey);
        return FALSE;
    }

    LPWSTR subkey = key + len;
    if (len > 0)
        *subkey++ = L'\\';

    // prohledame klice ulozene na stacku
    int i;
    for (i = stack.Count - 1; i >= top; i--)
    {
        wcscpy(subkey, stack[i]);

        if (!ScanKeyAux(root, key, skip, skipAllErrors, nameBuffer, stack))
        {
            if (!skip)
            {
                RegCloseKey(hKey);
                return FALSE;
            }
        }
        stack.Delete(i);
    }

    if (len > 0)
        *--subkey = L'\0';

    RegCloseKey(hKey);

    return TRUE;
}

BOOL CFindThread::ScanKey(int root, LPWSTR key, BOOL& skip, BOOL& skipAllErrors,
                          LPWSTR nameBuffer, TIndirectArray<WCHAR>& stack)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE4("CFindThread::ScanKey(%d, , %d, %d, , )", root, skip,
    //                      skipAllErrors);
    WCHAR base[50 + MAX_PREDEF_KEYNAME];
    SalPrintfW(base, 50, LoadStrW(IDS_SEARCHING), PredefinedHKeys[root].KeyName);
    FindDialog->StatusBar->SetBase(base);
    return ScanKeyAux(root, key, skip, skipAllErrors, nameBuffer, stack);
}

BOOL CFindThread::ScanRegistry(BOOL& skipAllErrors)
{
    CALL_STACK_MESSAGE2("CFindThread::ScanRegistry(%d)", skipAllErrors);
    BOOL skip = FALSE;
    WCHAR keyBuffer[MAX_KEYNAME];
    WCHAR nameBuffer[MAX_KEYNAME];
    TIndirectArray<WCHAR> stack(100, 100);
    int i = 0;
    while (PredefinedHKeys[i].HKey != NULL)
    {
        if (RegQueryInfoKeyW(PredefinedHKeys[i].HKey, NULL, NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        {
            *keyBuffer = L'\0';
            if (!ScanKey(i, keyBuffer, skip, skipAllErrors, nameBuffer, stack) && !skip)
                return FALSE;
        }
        i++;
    }
    return TRUE;
}

BOOL CFindThread::Init()
{
    CALL_STACK_MESSAGE1("CFindThread::Init()");
    if (PatternALen || PatternWLen)
    {
        // inicializujeme search objekty
        WORD flags = SASF_FORWARD | (CaseSensitive ? SASF_CASESENSITIVE : 0);
        if (RegExp)
        {
            SalRegExp = SG->AllocSalamanderREGEXPSearchData();
            if (!SalRegExp)
                return Error(IDS_LOWMEM);

            if (!SalRegExp->Set(PatternA, flags))
            {
                const char* err = SalRegExp->GetLastErrorText();
                if (err)
                    Error(GetParent(), err);
                return FALSE;
            }
        }
        else
        {
            BMForPatternA = SG->AllocSalamanderBMSearchData();
            if (!BMForPatternA)
                return Error(IDS_LOWMEM);

            BMForPatternA->Set(PatternA, PatternALen, flags);
            if (!BMForPatternA->IsGood())
                return Error(IDS_LOWMEM);

            if (PatternALen == PatternWLen)
                BMForPatternW = BMForPatternA;
            else
            {
                BMForPatternW = SG->AllocSalamanderBMSearchData();
                if (!BMForPatternW)
                    return Error(IDS_LOWMEM);

                BMForPatternW->Set(PatternW, PatternWLen, flags);
                if (!BMForPatternW->IsGood())
                    return Error(IDS_LOWMEM);
            }
        }
    }
    NextStatusUpdate = GetTickCount();
    return TRUE;
}

unsigned
CFindThread::Body()
{
    CALL_STACK_MESSAGE1("CFindThread::Body()");
    Sleep(50); // nez petr ostrani chybu v auxtools

    PARENT(FindDialog->HWindow);
    TRACE_I("Starting search for files");

#ifdef _DEBUG
    // statistika cetnosti typu hodnot
    int i;
    for (i = 0; i < 12; i++)
        TypeStat[i] = 0;
#endif

    if (Init())
    {
        TIndirectArray<WCHAR> stack(100, 100);
        BOOL skipAllErrors = FALSE;
        int root;
        LPWSTR key;
        int j;
        for (j = 0; j < LookIn.Count; j++)
        {
            if (!RemoveFSNameFromPath(LookIn[j]))
            {
                Error(IDS_NOTREGEDTPATH);
                continue;
            }
            if (!ParseFullPath(LookIn[j], key, root))
            {
                Error(IDS_BADPATH);
                continue;
            }

            if (root == -1)
            {
                // prohledame cely registry
                if (!ScanRegistry(skipAllErrors))
                    break;
            }
            else
            {
                // prohledame vybrany klic
                BOOL skip = FALSE;
                WCHAR keyBuffer[MAX_KEYNAME];
                WCHAR nameBuffer[MAX_KEYNAME];
                wcscpy(keyBuffer, key);
                if (!ScanKey(root, keyBuffer, skip, skipAllErrors, nameBuffer, stack) && !skip)
                    break;
            }
        }
    }

#ifdef _DEBUG
    // statiskita cetnosti typu hodnot
    for (i = 0; i < 12; i++)
        TRACE_I("Type " << i << " frequency = " << TypeStat[i]);
#endif

    TRACE_I("Ending search for files");

    PostMessage(FindDialog->HWindow, WM_USER_SEARCH_FINISHED, 0, 0);
    return TRUE;
}
