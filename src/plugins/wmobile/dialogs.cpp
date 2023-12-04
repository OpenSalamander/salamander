// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//****************************************************************************
//
// CCommonDialog
//

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, hParent, origin)
{
}

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, int helpID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, helpID, hParent, origin)
{
}

INT_PTR
CCommonDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // horizontalni i vertikalni vycentrovani dialogu k parentu
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // chci focus od DefDlgProc
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CCommonDialog::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

// ****************************************************************************
// CProgressDlg
//

CProgressDlg::CProgressDlg(HWND parent, const char* title, const char* operation, CObjectOrigin origin, int resID)
    : CCommonDialog(HLanguage, resID ? resID : IDD_PROGRESSDLG, parent, origin)
{
    ProgressBar = NULL;
    WantCancel = FALSE;
    LastTickCount = 0;
    TextCache[0] = 0;
    TextCacheIsDirty = FALSE;
    ProgressCache = 0;
    ProgressCacheIsDirty = FALSE;
    ProgressTotalCache = 0;
    ProgressTotalCacheIsDirty = FALSE;

    strncpy_s(Title, title, _TRUNCATE);
    strncpy_s(Operation, operation, _TRUNCATE);
}

void CProgressDlg::Set(const char* fileName, DWORD progressTotal, BOOL dalayedPaint)
{
    lstrcpyn(TextCache, fileName != NULL ? fileName : "", MAX_PATH);
    TextCacheIsDirty = TRUE;

    if (progressTotal != ProgressTotalCache)
    {
        ProgressTotalCache = progressTotal;
        ProgressTotalCacheIsDirty = TRUE;
    }

    if (!dalayedPaint)
        FlushDataToControls();
}

void CProgressDlg::SetProgress(DWORD progressTotal, DWORD progress, BOOL dalayedPaint)
{
    if (progressTotal != ProgressTotalCache)
    {
        ProgressTotalCache = progressTotal;
        ProgressTotalCacheIsDirty = TRUE;
    }

    if (progress != ProgressCache)
    {
        ProgressCache = progress;
        ProgressCacheIsDirty = TRUE;
    }

    if (!dalayedPaint)
        FlushDataToControls();
}

void CProgressDlg::EnableCancel(BOOL enable)
{
    if (HWindow != NULL)
    {
        HWND cancel = GetDlgItem(HWindow, IDCANCEL);
        if (IsWindowEnabled(cancel) != enable)
        {
            EnableWindow(cancel, enable);
            if (enable)
                SetFocus(cancel);
            PostMessage(cancel, BM_SETSTYLE, enable ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, TRUE);

            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // chvilku venujeme userovi ...
            {
                if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
    }
}

BOOL CProgressDlg::GetWantCancel()
{
    // kazdych 100ms prekreslime zmenena data (text + progress bary)
    DWORD ticks = GetTickCount();
    if (ticks - LastTickCount > 100)
    {
        LastTickCount = ticks;
        FlushDataToControls();
    }

    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // chvilku venujeme userovi ...
    {
        if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return WantCancel;
}

void CProgressDlg::FlushDataToControls()
{
    if (HWindow == NULL)
        return;

    if (TextCacheIsDirty)
    {
        SetDlgItemText(HWindow, IDT_FILENAME, TextCache);
        TextCacheIsDirty = FALSE;
    }

    if (ProgressTotalCacheIsDirty)
    {
        char buf[100];
        if (ProgressTotalCache == -1)
            ::SetWindowText(HWindow, Title);
        else
        {
            sprintf(buf, "(%d %%) %s", (int)((ProgressTotalCache /*+ 5*/) / 10), Title); // nezaokrouhlujeme (100% musi byt az pri 100% a ne pri 99.5%)
            ::SetWindowText(HWindow, buf);
        }

        ProgressBar->SetProgress(ProgressTotalCache, NULL);
        ProgressTotalCacheIsDirty = FALSE;
    }
    else if (ProgressTotalCache == -1)
        ProgressBar->SetProgress(ProgressTotalCache, NULL);
}

INT_PTR
CProgressDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CProgressDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // pouzijeme Salamanderovsky progress bar
        ProgressBar = SalamanderGUI->AttachProgressBar(HWindow, IDP_PROGRESSBAR);
        if (ProgressBar == NULL)
        {
            DestroyWindow(HWindow); // chyba -> neotevreme dialog
            return FALSE;           // konec zpracovani
        }

        ::SetWindowText(HWindow, Title);
        SetDlgItemText(HWindow, IDT_OPERATION, Operation);

        break; // chci focus od DefDlgProc
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDCANCEL)
        {
            if (!WantCancel)
            {
                FlushDataToControls();

                if (SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_YESNO_CANCEL), TitleWMobile,
                                                     MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    WantCancel = TRUE;
                    EnableCancel(FALSE);
                }
            }
            return TRUE;
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
// CProgress2Dlg
//

CProgress2Dlg::CProgress2Dlg(HWND parent, const char* title, const char* operation, const char* operation2, CObjectOrigin origin, int resID)
    : CProgressDlg(parent, title, operation, origin, resID ? resID : IDD_PROGRESS2DLG)
{
    ProgressBar2 = NULL;
    TextCache2[0] = 0;
    TextCache2IsDirty = FALSE;

    strncpy_s(Operation2, operation2, _TRUNCATE);
}

void CProgress2Dlg::Set(const char* fileName, const char* fileName2, BOOL dalayedPaint)
{
    lstrcpyn(TextCache, fileName != NULL ? fileName : "", MAX_PATH);
    lstrcpyn(TextCache2, fileName2 != NULL ? fileName2 : "", MAX_PATH);
    TextCache2IsDirty = TextCacheIsDirty = TRUE;

    if (!dalayedPaint)
        FlushDataToControls();
}

void CProgress2Dlg::FlushDataToControls()
{
    if (HWindow == NULL)
        return;

    CProgressDlg::FlushDataToControls();

    if (TextCache2IsDirty)
    {
        SetDlgItemText(HWindow, IDT_FILENAME2, TextCache2);
        TextCache2IsDirty = FALSE;
    }

    if (ProgressCacheIsDirty)
    {
        ProgressBar2->SetProgress(ProgressCache, NULL);
        ProgressCacheIsDirty = FALSE;
    }
    else if (ProgressCache == -1)
        ProgressBar2->SetProgress(ProgressCache, NULL);
}

INT_PTR
CProgress2Dlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CProgress2Dlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // pouzijeme Salamanderovsky progress bar
        ProgressBar2 = SalamanderGUI->AttachProgressBar(HWindow, IDP_PROGRESSBAR2);
        if (ProgressBar2 == NULL)
        {
            DestroyWindow(HWindow); // chyba -> neotevreme dialog
            return FALSE;           // konec zpracovani
        }

        SetDlgItemText(HWindow, IDT_OPERATION2, Operation2);

        break; // chci focus od DefDlgProc
    }
    }
    return CProgressDlg::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CChangeAttrDialog
//

CChangeAttrDialog::CChangeAttrDialog(HWND parent, CObjectOrigin origin, DWORD attr, DWORD attrDiff,
                                     BOOL selectionContainsDirectory,
                                     const SYSTEMTIME* timeModified,
                                     const SYSTEMTIME* timeCreated,
                                     const SYSTEMTIME* timeAccessed)
    : CCommonDialog(HLanguage, IDD_ATTRIBUTES, IDD_ATTRIBUTES, parent, origin)
{
    SelectionContainsDirectory = selectionContainsDirectory;
    Archive = (attr & FILE_ATTRIBUTE_ARCHIVE) != 0;
    ReadOnly = (attr & FILE_ATTRIBUTE_READONLY) != 0;
    Hidden = (attr & FILE_ATTRIBUTE_HIDDEN) != 0;
    System = (attr & FILE_ATTRIBUTE_SYSTEM) != 0;
    if (attrDiff & FILE_ATTRIBUTE_ARCHIVE)
        Archive = 2;
    if (attrDiff & FILE_ATTRIBUTE_READONLY)
        ReadOnly = 2;
    if (attrDiff & FILE_ATTRIBUTE_HIDDEN)
        Hidden = 2;
    if (attrDiff & FILE_ATTRIBUTE_SYSTEM)
        System = 2;
    ArchiveDirty = FALSE;
    ReadOnlyDirty = FALSE;
    HiddenDirty = FALSE;
    SystemDirty = FALSE;
    RecurseSubDirs = FALSE;
    ChangeTimeModified = FALSE;
    ChangeTimeCreated = FALSE;
    ChangeTimeAccessed = FALSE;
    TimeModified = *timeModified;
    TimeCreated = *timeCreated;
    TimeAccessed = *timeAccessed;
    HModifiedDate = NULL;
    HModifiedTime = NULL;
    HCreatedDate = NULL;
    HCreatedTime = NULL;
    HAccessedDate = NULL;
    HAccessedTime = NULL;
}

BOOL CChangeAttrDialog::GetAndValidateTime(CTransferInfo* ti, int resIDDate, int resIDTime, SYSTEMTIME* time)
{
    HWND hDate = GetDlgItem(HWindow, resIDDate);
    HWND hTime = GetDlgItem(HWindow, resIDTime);

    SYSTEMTIME st;
    SYSTEMTIME st2;
    DateTime_GetSystemtime(hDate, &st);
    DateTime_GetSystemtime(hTime, &st2);
    st2.wYear = st.wYear;
    st2.wMonth = st.wMonth;
    st2.wDayOfWeek = st.wDayOfWeek;
    st2.wDay = st.wDay;
    st2.wMilliseconds = 0;

    FILETIME dummyFT;
    if (!SystemTimeToFileTime(&st2, &dummyFT))
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_ERR_INVALIDDATEFORMAT), TitleWMobileError,
                                         MB_OK | MB_ICONEXCLAMATION);
        ti->ErrorOn(resIDDate);
        return FALSE;
    }
    *time = st2;
    return TRUE;
}

void CChangeAttrDialog::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_ARCHIVE, Archive);
    ti.CheckBox(IDC_READONLY, ReadOnly);
    ti.CheckBox(IDC_HIDDEN, Hidden);
    ti.CheckBox(IDC_SYSTEM, System);
    ti.CheckBox(IDC_RECURSESUBDIRS, RecurseSubDirs);

    if (ti.Type == ttDataToWindow)
    {
        EnableWindow(GetDlgItem(HWindow, IDC_RECURSESUBDIRS), SelectionContainsDirectory);

        // napred naleju datumy
        DateTime_SetSystemtime(HModifiedDate, GDT_VALID, &TimeModified);
        DateTime_SetSystemtime(HCreatedDate, GDT_VALID, &TimeCreated);
        DateTime_SetSystemtime(HAccessedDate, GDT_VALID, &TimeAccessed);
        // potom nastavim stav na disabled (nelze provest v jedne operaci)
        DateTime_SetSystemtime(HModifiedDate, GDT_NONE, &TimeModified);
        DateTime_SetSystemtime(HCreatedDate, GDT_NONE, &TimeCreated);
        DateTime_SetSystemtime(HAccessedDate, GDT_NONE, &TimeAccessed);
        // naleju casy
        DateTime_SetSystemtime(HModifiedTime, GDT_VALID, &TimeModified);
        DateTime_SetSystemtime(HCreatedTime, GDT_VALID, &TimeCreated);
        DateTime_SetSystemtime(HAccessedTime, GDT_VALID, &TimeAccessed);
        // obejdeme chybu v common controls -- pokud nezavolam SetFocus,
        // tvarej se vsechny tri controly jako focused
        SetFocus(HModifiedDate);
        SetFocus(HCreatedDate);
        SetFocus(HAccessedDate);
    }
    if (ti.Type == ttDataFromWindow)
    {
        SYSTEMTIME dummy;
        ChangeTimeModified = DateTime_GetSystemtime(HModifiedDate, &dummy) == GDT_VALID;
        ChangeTimeCreated = DateTime_GetSystemtime(HCreatedDate, &dummy) == GDT_VALID;
        ChangeTimeAccessed = DateTime_GetSystemtime(HAccessedDate, &dummy) == GDT_VALID;
        BOOL ret = TRUE;
        if (ret & ChangeTimeModified)
            ret &= GetAndValidateTime(&ti, IDC_ATTR_MODIFIED_DATE, IDC_ATTR_MODIFIED_TIME, &TimeModified);
        if (ret & ChangeTimeCreated)
            ret &= GetAndValidateTime(&ti, IDC_ATTR_CREATED_DATE, IDC_ATTR_CREATED_TIME, &TimeCreated);
        if (ret & ChangeTimeAccessed)
            ret &= GetAndValidateTime(&ti, IDC_ATTR_ACCESSED_DATE, IDC_ATTR_ACCESSED_TIME, &TimeAccessed);
    }

    if (ti.Type == ttDataToWindow)
        EnableWindows();
}

void CChangeAttrDialog::EnableWindows()
{
    SYSTEMTIME st;
    BOOL enabledM = DateTime_GetSystemtime(HModifiedDate, &st) == GDT_VALID;
    EnableWindow(HModifiedTime, enabledM);
    BOOL enabledC = DateTime_GetSystemtime(HCreatedDate, &st) == GDT_VALID;
    EnableWindow(HCreatedTime, enabledC);
    BOOL enabledA = DateTime_GetSystemtime(HAccessedDate, &st) == GDT_VALID;
    EnableWindow(HAccessedTime, enabledA);
    EnableWindow(GetDlgItem(HWindow, IDC_ATTR_CURRENT), enabledM | enabledC | enabledA);
}

INT_PTR
CChangeAttrDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HModifiedDate = GetDlgItem(HWindow, IDC_ATTR_MODIFIED_DATE);
        HModifiedTime = GetDlgItem(HWindow, IDC_ATTR_MODIFIED_TIME);
        HCreatedDate = GetDlgItem(HWindow, IDC_ATTR_CREATED_DATE);
        HCreatedTime = GetDlgItem(HWindow, IDC_ATTR_CREATED_TIME);
        HAccessedDate = GetDlgItem(HWindow, IDC_ATTR_ACCESSED_DATE);
        HAccessedTime = GetDlgItem(HWindow, IDC_ATTR_ACCESSED_TIME);
        break;
    }
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_ARCHIVE:
                ArchiveDirty = TRUE;
                break;
            case IDC_READONLY:
                ReadOnlyDirty = TRUE;
                break;
            case IDC_HIDDEN:
                HiddenDirty = TRUE;
                break;
            case IDC_SYSTEM:
                SystemDirty = TRUE;
                break;

            case IDC_RECURSESUBDIRS:
            {
                // pokud user zapne Include Subdirs, nastavime neuspinene checkboxy na grayed,
                // protoze nevime, co vybrane adresare obsahuji
                // pokud user voblu vypne, vratime neuspinene checkboxy do puvodniho stavu
                BOOL checked = IsDlgButtonChecked(HWindow, IDC_RECURSESUBDIRS) == BST_CHECKED;
                if (!ArchiveDirty)
                    CheckDlgButton(HWindow, IDC_ARCHIVE, checked ? 2 : Archive);
                if (!ReadOnlyDirty)
                    CheckDlgButton(HWindow, IDC_READONLY, checked ? 2 : ReadOnly);
                if (!HiddenDirty)
                    CheckDlgButton(HWindow, IDC_HIDDEN, checked ? 2 : Hidden);
                if (!SystemDirty)
                    CheckDlgButton(HWindow, IDC_SYSTEM, checked ? 2 : System);
                break;
            }
            case IDC_ATTR_CURRENT:
            {
                SYSTEMTIME st;
                GetLocalTime(&st); // vytahneme aktualni case

                SYSTEMTIME dummy;
                if (DateTime_GetSystemtime(HModifiedDate, &dummy) == GDT_VALID)
                {
                    DateTime_SetSystemtime(HModifiedDate, GDT_VALID, &st);
                    DateTime_SetSystemtime(HModifiedTime, GDT_VALID, &st);
                }
                if (DateTime_GetSystemtime(HCreatedDate, &dummy) == GDT_VALID)
                {
                    DateTime_SetSystemtime(HCreatedDate, GDT_VALID, &st);
                    DateTime_SetSystemtime(HCreatedTime, GDT_VALID, &st);
                }
                if (DateTime_GetSystemtime(HAccessedDate, &dummy) == GDT_VALID)
                {
                    DateTime_SetSystemtime(HAccessedDate, GDT_VALID, &st);
                    DateTime_SetSystemtime(HAccessedTime, GDT_VALID, &st);
                }
                break;
            }
            }
        }
        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR nmh = (LPNMHDR)lParam;
        if (nmh->code == DTN_DATETIMECHANGE)
            EnableWindows();
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
