// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//*****************************************************************************
//
// MultiMonCenterWindow
//

HWND GetTopVisibleParent(HWND hParent)
{
    // hledame parenta, ktery uz neni child window (jde o POPUP/OVERLAPPED window)
    HWND hIterator = hParent;
    while ((GetWindowLongPtr(hIterator, GWL_STYLE) & WS_CHILD) &&
           (hIterator = ::GetParent(hIterator)) != NULL &&
           IsWindowVisible(hIterator))
        hParent = hIterator;
    return hParent;
}

void MultiMonGetClipRectByRect(const RECT* rect, RECT* workClipRect, RECT* monitorClipRect)
{
    HMONITOR hMonitor = MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    *workClipRect = mi.rcWork;
    if (monitorClipRect != NULL)
        *monitorClipRect = mi.rcMonitor;
}

void MultiMonGetClipRectByWindow(HWND hByWnd, RECT* workClipRect, RECT* monitorClipRect)
{
    HMONITOR hMonitor; // na tento monitor okno umistime
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);

    if (hByWnd != NULL && IsWindowVisible(hByWnd) && !IsIconic(hByWnd)) // pozor, tato podminka je take v MultiMonCenterWindow
    {
        hMonitor = MonitorFromWindow(hByWnd, MONITOR_DEFAULTTONEAREST);
        // vytahneme working area desktopu
        GetMonitorInfo(hMonitor, &mi);
    }
    else
    {
        // pokud nalezneme foreground okno patrici nasi aplikaci,
        // centrujeme okno na stejny desktop
        HWND hForegroundWnd = GetForegroundWindow();
        DWORD processID;
        GetWindowThreadProcessId(hForegroundWnd, &processID);
        if (hForegroundWnd != NULL && processID == GetCurrentProcessId())
        {
            hMonitor = MonitorFromWindow(hForegroundWnd, MONITOR_DEFAULTTONEAREST);
        }
        else
        {
            // jinak okno centrujeme k primarnimu desktopu
            POINT pt;
            pt.x = 0; // primarni monitor
            pt.y = 0;
            hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
        }

        // vytahneme working area desktopu
        GetMonitorInfo(hMonitor, &mi);
    }
    *workClipRect = mi.rcWork;
    if (monitorClipRect != NULL)
        *monitorClipRect = mi.rcMonitor;
}

void MultiMonCenterWindowByRect(HWND hWindow, const RECT& clipR, const RECT& byR)
{
    if (hWindow == NULL)
    {
        // pri praci s NULL hwnd dochazi k nechtenemu blikani oken
        TRACE_E("MultiMonCenterWindowByRect: hWindow == NULL");
        return;
    }

    if (IsZoomed(hWindow))
    {
        // s maximalizovany oknem nebudeme hybat
        return;
    }

    RECT wndRect;
    GetWindowRect(hWindow, &wndRect);
    int wndWidth = wndRect.right - wndRect.left;
    int wndHeight = wndRect.bottom - wndRect.top;

    // vycentrujeme
    wndRect.left = byR.left + (byR.right - byR.left - wndWidth) / 2;
    wndRect.top = byR.top + (byR.bottom - byR.top - wndHeight) / 2;
    wndRect.right = wndRect.left + wndWidth;
    wndRect.bottom = wndRect.top + wndHeight;

    // ohlidame hranice
    if (wndRect.left < clipR.left) // pokud je okno vetsi nez clipR, nechame zobrazit jeho levou cast
    {
        wndRect.left = clipR.left;
        wndRect.right = wndRect.left + wndWidth;
    }

    if (wndRect.top < clipR.top) // pokud je okno vetsi nez clipR, nechame zobrazit jeho hotni cast
    {
        wndRect.top = clipR.top;
        wndRect.bottom = wndRect.top + wndHeight;
    }

    if (wndWidth <= clipR.right - clipR.left)
    {
        // pokud je okno mensi nez clipR, osetrime aby nelezlo vpravo za hranici clipR
        if (wndRect.right >= clipR.right)
        {
            wndRect.left = clipR.right - wndWidth;
            wndRect.right = wndRect.left + wndWidth;
        }
    }
    else
    {
        // pokud je okno vetsi nez clipR
        if (wndRect.left > clipR.left)
            wndRect.left = clipR.left; // vyuzijeme maximalne prostor
    }

    if (wndHeight <= clipR.bottom - clipR.top)
    {
        // pokud je okno mensi nez clipR, osetrime aby nelezlo dole za hranici clipR
        if (wndRect.bottom >= clipR.bottom)
        {
            wndRect.top = clipR.bottom - wndHeight;
            wndRect.bottom = wndRect.top + wndHeight;
        }
    }
    else
    {
        // pokud je okno vetsi nez clipR
        if (wndRect.top > clipR.top)
            wndRect.top = clipR.top; // vyuzijeme maximalne prostor
    }

    SetWindowPos(hWindow, NULL, wndRect.left, wndRect.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void MultiMonCenterWindow(HWND hWindow, HWND hByWnd, BOOL findTopWindow)
{
    if (hWindow == NULL)
    {
        // pri praci s NULL hwnd dochazi k nechtenemu blikani oken
        TRACE_E("MultiMonCenterWindow: hWindow == NULL");
        return;
    }

    if (IsZoomed(hWindow))
    {
        // s maximalizovany oknem nebudeme hybat
        return;
    }

    // mame dohledat top-level window
    if (findTopWindow)
    {
        if (hByWnd != NULL)
            hByWnd = GetTopVisibleParent(hByWnd);
        else
            TRACE_E("MultiMonCenterWindow: hByWnd == NULL and findTopWindow is TRUE");
    }

    RECT clipR;
    MultiMonGetClipRectByWindow(hByWnd, &clipR, NULL);
    RECT byR;
    if (hByWnd != NULL && IsWindowVisible(hByWnd) && !IsIconic(hByWnd)) // pozor, tato podminka je take v MultiMonGetClipRectByWindow
        GetWindowRect(hByWnd, &byR);
    else
        byR = clipR;

    MultiMonCenterWindowByRect(hWindow, clipR, byR);
}

//*****************************************************************************
//
// CMainDialog
//

CMainDialog::CMainDialog(HINSTANCE hInstance, int resID, BOOL minidumpOnOpen)
    : CDialog(hInstance, resID, NULL, ooAllocated)
{
    HBoldFont = NULL;
    Compressing = FALSE;
    Uploading = FALSE;
    Minidumping = FALSE;
    ZeroMemory(&CompressParams, sizeof(CompressParams));
    ZeroMemory(&UploadParams, sizeof(UploadParams));
    CurrentProgressText[0] = 0;
    MinidumpOnOpen = minidumpOnOpen;
}

CMainDialog::~CMainDialog()
{
    if (HBoldFont != NULL)
    {
        DeleteObject(HBoldFont);
        HBoldFont = NULL;
    }
}

void CMainDialog::ShowChilds(CDialogTaskEnum task, BOOL show)
{
    int ids[] = {IDD_SALMON_MAIN,
                 IDC_SALMON_INTRO,
                 IDC_SALMON_PRIVACY,
                 IDC_SALMON_VIEW,
                 IDC_SALMON_DESCRIPTION,
                 IDC_SALMON_ACTION_LABEL,
                 IDC_SALMON_ACTION,
                 IDC_SALMON_EMAIL_LABEL,
                 IDC_SALMON_EMAIL,
                 IDC_SALMON_RESTART,
                 IDOK,
                 IDCANCEL,
                 -1};
    for (int i = 0; ids[i] != -1; i++)
        ShowWindow(GetDlgItem(HWindow, ids[i]), show ? SW_SHOW : SW_HIDE);
    if (!show)
    {
        // nastavim rozmer child okna podle obsahu
        HWND hChild = GetDlgItem(HWindow, IDC_SALMON_UPLOADING);
        char buff[200];
        int resID = 0;
        switch (task)
        {
        case dteCompress:
            resID = IDS_SALMON_COMPRESSING;
            break;
        case dteUpload:
            resID = IDS_SALMON_UPLOADING;
            break;
        case dteMinidump:
            resID = IDS_SALMON_MINIDUMP;
            break;
        }
        if (resID != 0)
        {
            strcpy(buff, LoadStr(resID, HLanguage));
            strcpy(CurrentProgressText, buff);
            SetWindowText(hChild, buff);
        }
        HFONT hFont = (HFONT)SendMessage(hChild, WM_GETFONT, 0, 0);
        HDC hDC = HANDLES(GetDC(HWindow));
        HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
        RECT tR;
        tR.left = 0;
        tR.top = 0;
        tR.right = 1;
        tR.bottom = 1;
        DrawText(hDC, buff, -1, &tR, DT_CALCRECT | DT_CENTER | DT_NOPREFIX);
        SelectObject(hDC, hOldFont);
        HANDLES(ReleaseDC(HWindow, hDC));
        SIZE sz = {tR.right - tR.left, tR.bottom - tR.top};
        sz.cx += 3; // pro jistotu
        sz.cy += 1;
        SetWindowPos(hChild, NULL, 0, 0, sz.cx, sz.cy, SWP_NOZORDER | SWP_NOMOVE);
        CenterControl(IDC_SALMON_UPLOADING);
    }
    ShowWindow(GetDlgItem(HWindow, IDC_SALMON_UPLOADING), show ? SW_HIDE : SW_SHOW);
}

void CMainDialog::CenterControl(int resID)
{
    RECT wR;
    GetClientRect(HWindow, &wR);
    RECT cR;
    GetClientRect(GetDlgItem(HWindow, resID), &cR);
    SetWindowPos(GetDlgItem(HWindow, resID), NULL, (wR.right - cR.right) / 2, (wR.bottom - cR.bottom) / 2, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void StripWhiteSpaces(char* buff)
{
    char* s = buff;
    while (*s == ' ' || *s == '\t')
        s++;
    if (s > buff)
        memmove(buff, s, strlen(s) + 1);

    s = buff + strlen(buff);
    while (*s == ' ' || *s == '\t' || *s == 0)
        s--;
    s++;
    *s = 0;
}

BOOL ValidateEmail(const char* buff) // primitivni kontrola syntakticke validity emailu
{
    // predpoklada, ze jsou orezany white spacy na zacatku/konci
    const char* s = buff;
    // string nesmi byt prazdny
    if (*s == 0)
        return FALSE;
    // nejaky znak jiny nez zavinac musi byt pred zacinacem
    if (*s == '@')
        return FALSE;
    // hledame zacinac
    while (*s != '@' && *s != 0)
        s++;
    // pokud jsme ho nenasli, neni email validni
    if (*s != '@')
        return FALSE;
    s++;
    // email nemuze koncit zavinacem
    if (*s == 0)
        return FALSE;
    // dalsi zavinac uz nesmi byt
    while (*s != '@' && *s != 0)
        s++;
    if (*s == '@')
        return FALSE;
    return TRUE;
}

void CMainDialog::Validate(CTransferInfo& ti)
{
    char buff[EMAIL_SIZE];
    ti.EditLine(IDC_SALMON_EMAIL, buff, EMAIL_SIZE);
    StripWhiteSpaces(buff);
    if (*buff != 0 && !ValidateEmail(buff))
    {
        MessageBox(HWindow, LoadStr(IDS_SALMON_INVALIDEMAIL, HLanguage), LoadStr(IDS_SALMON_TITLE, HLanguage), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        ti.ErrorOn(IDC_SALMON_EMAIL);
    }
}

void CMainDialog::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDC_SALMON_ACTION, Config.Description, DESCRIPTION_SIZE);
    ti.EditLine(IDC_SALMON_EMAIL, Config.Email, EMAIL_SIZE);
}

BOOL CMainDialog::StartUploadIndex(int index)
{
    ZeroMemory(&UploadParams, sizeof(UploadParams));
    strcpy(UploadParams.FileName, BugReportPath);
    strcat(UploadParams.FileName, BugReports[index].Name);
    strcat(UploadParams.FileName, ".7Z");
    BOOL ret = StartUploadThread(&UploadParams);
    ShowChilds(dteUpload, FALSE);
    return ret;
}

INT_PTR
CMainDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // od monitorovaneho procesu mame pridelene povoleni pro zavolani SetForegroundWindow
        // podstatne je, ze jiz bezi nase message loop, jinak to osklive zlobilo (dokud jsme SetForegroundWindow
        // volali z OpenMainDialog) - pri spusteni monitorovaneho softu pres StartMenu a padu jsme zustavali dole,
        // ProtMon okno nebylo aktivni, nedostalo focus, dokud monitorovany soft nezavrel sve okno
        SetForegroundWindow(HWindow);

        MultiMonCenterWindow(HWindow, NULL, FALSE);

        ShowChilds(dteDialog, TRUE);
        CenterControl(IDC_SALMON_UPLOADING);

        LOGFONT lf;
        HFONT hFont = (HFONT)SendMessage(GetDlgItem(HWindow, IDC_SALMON_DESCRIPTION), WM_GETFONT, 0, 0);
        GetObject(hFont, sizeof(lf), &lf);
        lf.lfWeight = FW_BOLD;
        HBoldFont = CreateFontIndirect(&lf);

        SendMessage(GetDlgItem(HWindow, IDC_SALMON_DESCRIPTION), WM_SETFONT, (WPARAM)HBoldFont, TRUE);

        EnableWindow(GetDlgItem(HWindow, IDC_SALMON_RESTART), MinidumpOnOpen);
        if (MinidumpOnOpen)
            CheckDlgButton(HWindow, IDC_SALMON_RESTART, BST_CHECKED);

        SetTimer(HWindow, 666, 250, NULL);

        if (MinidumpOnOpen)
        {
            ZeroMemory(&MinidumpParams, sizeof(MinidumpParams));
            Minidumping = StartMinidumpThread(&MinidumpParams);
            ShowChilds(dteMinidump, FALSE);
        }

        // focus do pole s popisem padu
        SetFocus(GetDlgItem(HWindow, IDC_SALMON_ACTION));
        SendMessage(HWindow, DM_SETDEFID, IDC_SALMON_ACTION, 0);

        CDialog::DialogProc(uMsg, wParam, lParam);
        return FALSE;
    }

    case WM_DESTROY:
    {
        KillTimer(HWindow, 666);
        break;
    }

    case WM_CLOSE:
    {
        return 0;
    }

    case WM_TIMER:
    {
        if (!AppIsBusy && wParam == 666) // chranime se pred stavem, kdy uz mame zobrazeny msgbox; nechceme dalsi
        {
            if (Compressing || Uploading || Minidumping)
            {
                static DWORD counter = 0;
                counter++;
                char buff[200];
                strcpy(buff, CurrentProgressText);
                if (strlen(buff) > 3 && strcmp(buff + strlen(buff) - 3, "...") == 0)
                    buff[strlen(CurrentProgressText) - 3 + (counter % 4)] = 0;
                HWND hChild = GetDlgItem(HWindow, IDC_SALMON_UPLOADING);
                SetWindowText(hChild, buff);
            }

            if (Minidumping && !IsMinidumpThreadRunning())
            {
                Minidumping = FALSE;

                // pokud selhalo generovani minidumpu, pouze vyhlasime chybu, ale pokracujeme dale (neco se mohlo povest ulozit)
                if (!MinidumpParams.Result)
                {
                    char msg[2 * MAX_PATH];
                    sprintf(msg, LoadStr(IDS_SALMON_BUGREPORT_PROBLEM, HLanguage), MinidumpParams.ErrorMessage);
                    MessageBox(HWindow, msg, LoadStr(IDS_SALMON_TITLE, HLanguage), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
                }

                if (GetBugReportNames() && GetUniqueBugReportCount() > 1)
                {
                    // reportu je vic, doptame se zda je mame poslat vsechny
                    int res = MessageBox(HWindow, LoadStr(IDS_SALMON_MORE_REPORTS, HLanguage), LoadStr(IDS_SALMON_TITLE, HLanguage), MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND);
                    ReportOldBugs = (res == IDYES);
                }
                // zobrazime hlavni dialog s dotazem
                ShowChilds(dteDialog, TRUE);
            }

            if (Compressing && !IsCompressThreadRunning())
            {
                Compressing = FALSE;

                if (CompressParams.Result)
                {
                    // spustime upload vlakno
                    UploadingIndex = 0;
                    Uploading = StartUploadIndex(UploadingIndex);
                }
                else
                {
                    char msg[2 * MAX_PATH];
                    sprintf(msg, LoadStr(IDS_SALMON_COMPRESSFAILED, HLanguage), CompressParams.ErrorMessage);
                    MessageBox(HWindow, msg, LoadStr(IDS_SALMON_TITLE, HLanguage), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
                    OpenFolder(NULL, BugReportPath);
                    PostQuitMessage(0);
                }
            }

            if (Uploading && !IsUploadThreadRunning())
            {
                Uploading = FALSE;
                ShowChilds(dteDialog, TRUE);

                if (UploadParams.Result)
                {
                    if (UploadingIndex + 1 < BugReports.Count && ReportOldBugs)
                    {
                        // zacneme uploadit dalsi soubor
                        UploadingIndex++;
                        Uploading = StartUploadIndex(UploadingIndex);
                    }
                    else
                    {
                        MessageBox(HWindow, LoadStr(IDS_SALMON_UPLOADSUCCESS, HLanguage), LoadStr(IDS_SALMON_TITLE, HLanguage), MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                        CleanBugReportsDirectory(FALSE); // napred promazneme, jinak bude Salam pri startu nadavat
                        if (IsDlgButtonChecked(HWindow, IDC_SALMON_RESTART) == BST_CHECKED)
                            RestartSalamander(HWindow);
                        PostQuitMessage(0);
                    }
                }
                else
                {
                    char msg[2 * MAX_PATH];
                    sprintf(msg, LoadStr(IDS_SALMON_UPLOADFAILED, HLanguage), UploadParams.ErrorMessage);
                    MessageBox(HWindow, msg, LoadStr(IDS_SALMON_TITLE, HLanguage), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
                    CleanBugReportsDirectory(TRUE); // promazeme reporty, nechame jen archivy
                    OpenFolder(NULL, BugReportPath);
                    PostQuitMessage(0);
                }
            }
        }
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            if (!ValidateData() || !TransferData(ttDataFromWindow))
                return TRUE;
            SaveDescriptionAndEmail();

            ZeroMemory(&CompressParams, sizeof(CompressParams));
            Compressing = StartCompressThread(&CompressParams);
            ShowChilds(dteCompress, FALSE);
            return 0;
        }

        case IDCANCEL:
        {
            if (Compressing || Uploading)
            {
                MessageBox(HWindow, LoadStr(IDS_SALMON_WAITFORUPLOAD, HLanguage), LoadStr(IDS_SALMON_TITLE, HLanguage), MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                return 0;
            }
            int ret = MessageBox(HWindow, LoadStr(IDS_SALMON_CONFIRMEXIT, HLanguage), LoadStr(IDS_SALMON_TITLE, HLanguage), MB_OKCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND);
            if (ret == IDCANCEL)
                return 0;

            CleanBugReportsDirectory(FALSE);

            if (IsDlgButtonChecked(HWindow, IDC_SALMON_RESTART) == BST_CHECKED)
                RestartSalamander(HWindow);

            PostQuitMessage(0);
            break;
        }

        case IDC_SALMON_VIEW:
        {
            OpenFolder(HWindow, BugReportPath);
            break;
        }
        }
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}
