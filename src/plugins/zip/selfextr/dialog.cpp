// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <shlobj.h>

#include "strings.h"
#include "comdefs.h"
#include "selfextr.h"
#include "extract.h"
#include "dialog.h"
#include "resource.h"
#include "extended.h"

HWND DlgWin = NULL;
HWND ProgressWin = NULL;
__int64 Progress = 0;
__int64 ProgressTotalSize = 1;
WNDPROC OrigProgressControlProc;
WNDPROC OrigTextControlProc;
HANDLE Thread;
HWND WebLinkControl;
int ShortPathControlWidth;
int ShortPathControlHeigth;
int DlgWinWidth;
int DlgWinHeigth;
int DlgWinAboutHeigth; //heigth with the about showed
bool AboutShowed;

//bool Started = false;

void CenterDialog(HWND dlg)
{
    int width = GetSystemMetrics(SM_CXSCREEN);
    int heigth = GetSystemMetrics(SM_CYSCREEN);
    /*  if (width > 0 && heigth > 0)
  {*/
    RECT r;
    GetWindowRect(dlg, &r);
    int x = ((width) - (r.right - r.left)) / 2;
    int y = ((heigth) - (r.bottom - r.top)) / 2;
    SetWindowPos(dlg, 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    //}
}

BOOL WINAPI OvewriteDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static bool readOnly;
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        COverwriteDlgInfo* info = (COverwriteDlgInfo*)lParam;
        OrigTextControlProc = (WNDPROC)SetWindowLong(GetDlgItem(hDlg, IDC_SOURCENAME), GWL_WNDPROC, (LONG)TextControlProc);
        SetWindowLong(GetDlgItem(hDlg, IDC_TARGETNAME), GWL_WNDPROC, (LONG)TextControlProc);
        SetDlgItemText(hDlg, IDC_TARGETNAME, info->Target);
        SetDlgItemText(hDlg, IDC_TARGETATTR, info->TargetAttr);
        if ((readOnly = info->ReadOnly) != 0)
        {
            ShowWindow(GetDlgItem(hDlg, IDC_WITHFILE), SW_HIDE);
            info->Source = StringTable[STR_OVERWRITE_RO];
        }
        else
        {
            SetDlgItemText(hDlg, IDC_SOURCEATTR, info->SourceAttr);
        }
        SetDlgItemText(hDlg, IDC_SOURCENAME, info->Source);
        char title[200];
        if (DlgWin)
            *title = 0;
        else
        {
            lstrcpy(title, Title);
            lstrcat(title, " - ");
            CenterDialog(hDlg);
        }
        lstrcat(title, StringTable[STR_OVERWRITEDLGTITLE]);
        SetWindowText(hDlg, title);
        SetForegroundWindow(hDlg);
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_ALL:
            if (readOnly)
                Silent |= SF_OVEWRITEALL_RO;
            else
                Silent |= SF_OVEWRITEALL;
        case IDYES:
            EndDialog(hDlg, IDYES);
            return FALSE;

        case IDC_SKIPALL:
            Silent |= SF_SKIPALL;
        case IDC_SKIP:
            EndDialog(hDlg, IDC_SKIP);
            return FALSE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return FALSE;
        }

    case WM_DESTROY:
        SetWindowLong(GetDlgItem(hDlg, IDC_SOURCENAME), GWL_WNDPROC, (LONG)OrigTextControlProc);
        SetWindowLong(GetDlgItem(hDlg, IDC_TARGETNAME), GWL_WNDPROC, (LONG)OrigTextControlProc);
        return FALSE;
    }
    return FALSE;
}

int OverwriteDialog(const char* source, const char* sourceAttr, const char* target, const char* targetAttr, DWORD attr)
{
    if (Silent & SF_SKIPALL)
        return IDC_SKIP;
    COverwriteDlgInfo info;
    info.ReadOnly = false;
    if (Silent & SF_OVEWRITEALL)
    {
        if (attr & FILE_ATTRIBUTE_READONLY || attr & FILE_ATTRIBUTE_HIDDEN ||
            attr & FILE_ATTRIBUTE_SYSTEM)
        {
            if (Silent & SF_OVEWRITEALL_RO)
                return IDYES;
            info.ReadOnly = true;
        }
        else
            return IDYES;
    }
    info.Source = source;
    info.SourceAttr = sourceAttr;
    info.Target = target;
    info.TargetAttr = targetAttr;
    return DialogBoxParam(HInstance, MAKEINTRESOURCE(IDD_OVERWRITE),
                          DlgWin, OvewriteDlgProc, (LPARAM)&info);
}

BOOL WINAPI ErrorDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        int defID = IDC_RETRY;
        CErrorDlgInfo* info = (CErrorDlgInfo*)lParam;
        SetWindowText(hDlg, info->Title);
        OrigTextControlProc = (WNDPROC)SetWindowLong(GetDlgItem(hDlg, IDC_FILENAME), GWL_WNDPROC, (LONG)TextControlProc);
        SetDlgItemText(hDlg, IDC_FILENAME, info->File);
        SetDlgItemText(hDlg, IDC_ERROR, info->Error);
        if (!info->Retry)
        {
            EnableWindow(GetDlgItem(hDlg, IDC_RETRY), FALSE);
            defID = IDC_SKIP;
        }
        if (info->NoSkip)
        {
            EnableWindow(GetDlgItem(hDlg, IDC_SKIP), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_SKIPALL), FALSE);
            if (!info->Retry)
                defID = IDCANCEL;
        }
        SendMessage(hDlg, DM_SETDEFID, defID, 0);
        if (!DlgWin)
            CenterDialog(hDlg);
        SetForegroundWindow(hDlg);
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_RETRY:
            EndDialog(hDlg, IDC_RETRY);
            return FALSE;

        case IDC_SKIP:
            EndDialog(hDlg, IDC_SKIP);
            return FALSE;

        case IDC_SKIPALL:
            EndDialog(hDlg, IDC_SKIPALL);
            return FALSE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return FALSE;
        }
    case WM_DESTROY:
        SetWindowLong(GetDlgItem(hDlg, IDC_FILENAME), GWL_WNDPROC, (LONG)OrigTextControlProc);
        return FALSE;
    }
    return FALSE;
}

int ErrorDialog(const char* title, const char* file, const char* error, bool retry, bool noSkip)
{
    CErrorDlgInfo info;
    info.Title = title;
    info.File = file;
    info.Error = error;
    info.Retry = retry;
    info.NoSkip = noSkip;
    return DialogBoxParam(HInstance, MAKEINTRESOURCE(IDD_ERROR),
                          DlgWin, ErrorDlgProc, (LPARAM)&info);
}

BOOL WINAPI LongMessageDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HWND cancel = GetDlgItem(hDlg, IDCANCEL);
        HWND ok = GetDlgItem(hDlg, IDOK);
        const char *okText, *cancelText;
        switch (ArchiveStart->MBoxStyle)
        {
        case SE_MBOK:
        {
            RECT r;
            GetWindowRect(cancel, &r);
            MapWindowPoints((HWND)NULL, hDlg, (LPPOINT)&r, 2);
            MoveWindow(ok, r.left, r.top, r.right - r.left, r.bottom - r.top, FALSE);
            ShowWindow(cancel, SW_HIDE);
        }
        case SE_MBOKCANCEL:
            okText = StringTable[STR_OK];
            cancelText = StringTable[STR_CANCEL];
            break;
        case SE_MBYESNO:
            okText = StringTable[STR_YES];
            cancelText = StringTable[STR_NO];
            break;
        case SE_MBAGREEDISAGREE:
            okText = StringTable[STR_AGREE];
            cancelText = StringTable[STR_DISAGREE];
            break;
        }
        SetWindowText(ok, okText);
        SetWindowText(cancel, cancelText);
        SetDlgItemText(hDlg, IDE_MESSAGE, (char*)ArchiveStart + ArchiveStart->MBoxTextOffs);
        SetWindowText(hDlg, (char*)ArchiveStart + ArchiveStart->MBoxTitleOffs);
        CenterDialog(hDlg);
        return FALSE;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, wParam);
            return TRUE;
        }
    }
    }
    return FALSE;
}

LRESULT CALLBACK ProgressControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //static CProgressControlInfo info;
    static int Width;
    static int Height;
    static HBITMAP Bitmap;
    static HBRUSH SelectBrush;
    static HBRUSH NormalBrush;
    static HBRUSH BlackBrush;
    static HBRUSH LightBrush;
    static __int64 DisplayedProgress;
    static bool AlreadyWaitForRefresh;
    static DWORD LastRedrawTime;
    switch (uMsg)
    {
    case WM_USER_INITCONTROL:
    {
        RECT r;
        GetClientRect(hWnd, &r);
        Width = r.right - r.left;
        Height = r.bottom - r.top;
        Progress = 0;
        Bitmap = NULL;
        SelectBrush = CreateSolidBrush(PGC_BKGND_SELECT);
        NormalBrush = CreateSolidBrush(PGC_BKGND_NORMAL);
        BlackBrush = GetSysColorBrush(COLOR_3DDKSHADOW);
        DWORD c = GetSysColor(COLOR_3DHILIGHT);
        if (c == PGC_BKGND_NORMAL)
            c = GetSysColor(COLOR_3DLIGHT);
        LightBrush = CreateSolidBrush(c);
        HDC dc = GetDC(hWnd);
        if (dc != NULL)
        {
            Bitmap = (HBITMAP)CreateCompatibleBitmap(dc, Width, Height);
            ReleaseDC(hWnd, dc);
        }
        DisplayedProgress = 0;
        AlreadyWaitForRefresh = false;
        LastRedrawTime = 0;
        return 0;
    }

    case WM_USER_REFRESHPROGRESS:
    {
        DWORD time = GetTickCount();
        Progress += wParam;
        if (Progress == ProgressTotalSize ||
            (Progress - DisplayedProgress) * 100 / ProgressTotalSize > 10)
        {
            time = LastRedrawTime + 101;
        }
        if (time - LastRedrawTime > 100)
        {
            InvalidateRect(hWnd, NULL, FALSE);
            UpdateWindow(hWnd);
        }
        else
        {
            if (!AlreadyWaitForRefresh)
            {
                SetTimer(hWnd, IDT_REPAINT, 100, NULL);
                AlreadyWaitForRefresh = TRUE;
            }
        }
        return 0;
    }

    case WM_TIMER:
    {
        if (wParam == IDT_REPAINT)
        {
            KillTimer(hWnd, IDT_REPAINT);
            InvalidateRect(hWnd, NULL, FALSE);
            UpdateWindow(hWnd);
        }
        return 0;
    }

    case WM_PAINT:
    {
        LastRedrawTime = GetTickCount();
        AlreadyWaitForRefresh = FALSE;
        DisplayedProgress = Progress;

        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        /*
      if (!Started)
      {
        EndPaint(hWnd, &ps);
        return 0;
      }
*/
        HDC dc = NULL;
        BOOL memDC;
        HBITMAP oldBmp;
        if (Bitmap != NULL)
        {
            dc = CreateCompatibleDC(ps.hdc);
            if (dc != NULL)
            {
                oldBmp = (HBITMAP)SelectObject(dc, Bitmap);
                memDC = TRUE;
            }
        }
        if (dc == NULL)
        {
            dc = ps.hdc;
            memDC = FALSE;
        }

        if (SelectBrush != NULL && NormalBrush != NULL && LightBrush != NULL && BlackBrush != NULL)
        {
            __int64 progress = Progress;
            RECT r;
            r.left = 0;
            r.top = 0;
            r.right = Width;
            r.bottom = Height;
            FrameRect(dc, &r, BlackBrush);
            r.left = 1;
            r.top = 1;
            r.right = Width - 1;
            r.bottom = Height - 1;
            FrameRect(dc, &r, LightBrush);
            r.left = 2;
            LONG right = r.right = 2 + (int)((Width - 4) * progress / ProgressTotalSize);
            r.top = 2;
            r.bottom = Height - 2;
            FillRect(dc, &r, SelectBrush);
            r.left = r.right;
            r.right = Width - 2;
            if (r.left < r.right)
                FillRect(dc, &r, NormalBrush);

            char txt[10];
            wsprintf(txt, "%d %%", (int)((progress * 1000 / ProgressTotalSize /*+ 5*/)) / 10); // nezaokrouhlujeme (100% musi byt az pri 100% a ne pri 99.5%)
            r.left = 2;                                                                        /*
        r.right = Width - 2;
        r.top = 2;
        r.bottom = Height - 2;*/

            int prevBkMode = SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, PGC_FONT_NORMAL);
            DrawText(dc, txt, lstrlen(txt), &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

            HRGN region = CreateRectRgn(2, 2, right, Height - 2);
            if (region != NULL)
            {
                ExtSelectClipRgn(dc, region, RGN_AND); // budem kreslit jen do leve casti
                SetTextColor(dc, PGC_FONT_SELECT);
                DrawText(dc, txt, lstrlen(txt), &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                DeleteObject(region);
            }

            SetBkMode(dc, prevBkMode);
        }

        if (memDC)
        {
            BitBlt(ps.hdc, 0, 0, Width, Height, dc, 0, 0, SRCCOPY);
            SelectObject(dc, oldBmp);
            DeleteDC(dc);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_USER_DESTROY:
        if (SelectBrush)
            DeleteObject(SelectBrush);
        if (NormalBrush)
            DeleteObject(NormalBrush);
        break;
    }
    return CallWindowProc(OrigProgressControlProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK TextControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        RECT r;
        PAINTSTRUCT ps;
        char txt[MAX_PATH];
        DWORD c;
        UINT format = DT_SINGLELINE | DT_BOTTOM | DT_NOPREFIX;

        GetClientRect(hWnd, &r);
        BeginPaint(hWnd, &ps);
        HBRUSH DialogBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        if (DialogBrush)
        {
            FillRect(ps.hdc, &r, DialogBrush);
            DeleteObject(DialogBrush);
        }
        HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
        if (WebLinkControl == hWnd)
        {
            LOGFONT lf;
            GetObject(hFont, sizeof(lf), &lf);
            lf.lfUnderline = TRUE;
            hFont = CreateFontIndirect(&lf);
            c = PGC_FONT;
            format |= DT_RIGHT;
        }
        else
        {
            c = GetSysColor(COLOR_BTNTEXT);
            format |= DT_PATH_ELLIPSIS;
        }
        HFONT hOldFont = (HFONT)SelectObject(ps.hdc, hFont);
        SetTextColor(ps.hdc, c);
        int prevBkMode = SetBkMode(ps.hdc, TRANSPARENT);
        int len = GetWindowText(hWnd, txt, MAX_PATH);
        DrawText(ps.hdc, txt, len, &r, format);
        SetBkMode(ps.hdc, prevBkMode);
        SelectObject(ps.hdc, hOldFont);
        if (WebLinkControl == hWnd)
            DeleteObject(hFont);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_SETCURSOR:
        SetCursor(LoadCursor(HInstance, MAKEINTRESOURCE(IDCURSOR_HAND)));
        return FALSE;

    case WM_LBUTTONDOWN:
    {
        char link[MAX_PATH];
        GetWindowText(hWnd, link, MAX_PATH);
        ShellExecute(hWnd, "open", link, NULL, NULL, SW_SHOWNORMAL);
    }
    }
    return CallWindowProc(OrigTextControlProc, hWnd, uMsg, wParam, lParam);
}

int TestFreeSpace(bool* unpack)
{
    __int64 clusterSize, free, needed = 0;

    char root[MAX_PATH];
    lstrcpyn(root, TargetPath, GetRootLen(TargetPath) + 1);
    PathAddBackslash(root);

    *unpack = true;
    DWORD sectorsPerCluster, bytesPerSector, numberOfFreeClusters, totalNumberOfClusters;
    if (GetDiskFreeSpace(root, &sectorsPerCluster, &bytesPerSector,
                         &numberOfFreeClusters, &totalNumberOfClusters))
    {
        clusterSize = (__int64)bytesPerSector * (__int64)sectorsPerCluster;
        free = (__int64)numberOfFreeClusters * clusterSize;
    }
    else
    {
        clusterSize = 1;
        free = 0xFFFFFFFF; // to staci sfx nikdy nebude vetsi:))
    }

    //compute estimated free space
    CFileHeader* header;
    unsigned totalEntries;
    ProgressTotalSize = 0;
    if (GetFirstFile(&header, &totalEntries, NULL))
        return 1;
    while (1)
    {
        ProgressTotalSize += header->Size + 1;
        needed += ((header->Size + clusterSize - 1) / clusterSize) * clusterSize;
        if (!--totalEntries)
            break;
        if (GetNextFile(&header, NULL))
            return 1;
    }

    if (free < needed)
    {
        char buf1[50];
        char buf2[50];
        char buf3[500];
        wsprintf(buf3, StringTable[STR_NOTENOUGHTSPACE], NumberToStr(buf1, needed), NumberToStr(buf2, free), DlgWin ? "" : StringTable[STR_ADVICE]);
        switch (MessageBox(DlgWin, buf3, DlgWin ? StringTable[STR_QUESTION] : Title, MB_YESNOCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND))
        {
        case IDNO:
            if (DlgWin)
            {
                *unpack = false;
                return 0;
            }
        default:
            CloseMapping();
            return 1;
        }
    }
    return 0;
}

void EnablePathControl(BOOL enable)
{
    ShowWindow(GetDlgItem(DlgWin, IDC_BROWSE), enable ? SW_SHOW : SW_HIDE);
    SendDlgItemMessage(DlgWin, IDC_PATH, EM_SETREADONLY, !enable, 0);
    HWND hwndPath = GetDlgItem(DlgWin, IDC_PATH);
    LONG style = GetWindowLong(hwndPath, GWL_EXSTYLE);
    if (style)
    {
        int w;
        if (enable)
        {
            style = style & ~(LONG)WS_EX_STATICEDGE | WS_EX_CLIENTEDGE;
            w = ShortPathControlWidth;
        }
        else
        {
            style = style & ~(LONG)WS_EX_CLIENTEDGE | WS_EX_STATICEDGE;
            RECT r;
            GetWindowRect(ProgressWin, &r);
            w = r.right - r.left;
        }
        SetWindowLong(hwndPath, GWL_EXSTYLE, style);
        SetWindowPos(hwndPath, 0, 0, 0, w, ShortPathControlHeigth, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

int StartExtracting()
{
    char buf[MAX_PATH];
    if (DlgWin)
    {
        SendDlgItemMessage(DlgWin, IDC_PATH, WM_GETTEXT, MAX_PATH, (LPARAM)buf);
        GetCurrentDirectory(MAX_PATH, TargetPath);
        if (!PathMerge(TargetPath, buf))
        {
            MessageBox(DlgWin, StringTable[STR_ERROR_TOOLONGNAME], DlgWin ? StringTable[STR_ERROR] : Title, MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
            goto notOK;
        }
    }

    //check drive or network share if exists
    lstrcpy(buf, TargetPath);
    PathStripToRoot(buf);
    DWORD attr;
    attr = SalGetFileAttributes(buf);
    if (attr == 0xFFFFFFFF)
    {
        MessageBox(DlgWin, StringTable[STR_BADDRIVE], DlgWin ? StringTable[STR_ERROR] : Title, MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        goto notOK;
    }
    attr = SalGetFileAttributes(TargetPath);
    if (attr == 0xFFFFFFFF || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        if ((ArchiveStart->Flags & SE_AUTODIR) ||
            MessageBox(DlgWin, StringTable[STR_TARGET_NOEXIST], DlgWin ? StringTable[STR_QUESTION] : Title, MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) == IDYES)
        {
            if (CheckAndCreateDirectory(TargetPath, true) == INVALID_HANDLE_VALUE)
                goto notOK; //return DlgWin ? 0 : (CloseMapping(), 1);
        }
        else
            goto notOK;
    }

#ifdef EXT_VER
    if (MVMode == MV_PROCESS_ARCHIVE)
    {
        if (ReadCentralDirectory())
            return 1;
    }
    else
    {
        RAData = (unsigned char*)ArchiveStart + ArchiveStart->HeaderSize;
        RASize = Size - ((unsigned char*)ArchiveStart - ArchiveBase) - ArchiveStart->HeaderSize;
        ECRec = (CEOCentrDirRecord*)((char*)ArchiveStart + ArchiveStart->HeaderSize +
                                     ArchiveStart->EOCentrDirOffs);
        DiskNum = ECRec->DiskNum + 1;
        CentrDir = (CFileHeader*)((char*)ArchiveStart + ArchiveStart->HeaderSize + ECRec->CentrDirOffs);
        if ((unsigned char*)CentrDir + ECRec->CentrDirSize > ArchiveBase + Size)
        {
            return HandleError(STR_ERROR_BADDATA, 0);
        }
    }

#endif //EXT_VER

    bool unpack;
    if (TestFreeSpace(&unpack))
        return 1;
    if (unpack)
    {
        if (ArchiveStart->Flags & SE_TEMPDIR)
        {
            char tempPath[MAX_PATH];
            lstrcpy(tempPath, TargetPath);
            if (GetTempFileName(tempPath, "SFX", 0, TargetPath) == 0)
            {
                HandleError(STR_ERROR_TEMPNAME, GetLastError());
                goto notOK;
            }
            else
            {
                if (DeleteFile(TargetPath) == 0)
                {
                    HandleError(STR_ERROR_DELFILE, GetLastError());
                    goto notOK;
                }
                else
                {
                    if (CreateDirectory(TargetPath, NULL) == 0)
                    {
                        HandleError(STR_ERROR_MKDIR, GetLastError());
                        goto notOK;
                    }
                    else
                    {
                        if (DlgWin)
                            SetDlgItemText(DlgWin, IDC_PATH, TargetPath);
                        RemoveTemp = ArchiveStart->Flags & SE_TEMPDIR && ArchiveStart->Flags & SE_REMOVEAFTER;
                        /*if (RemoveTemp)
            {
              if (RegCreateKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
                  &RunOnceHKey) == ERROR_SUCCESS)
              {
                wsprintf(RunOnceValue, "Self-extraxtor%X", GetTickCount());
                char buf[1024];
                wsprintf(buf, "rmdir /s /q \"%s\"", TargetPath);
                RegSetValueEx(RunOnceHKey, RunOnceValue, 0, REG_SZ, (unsigned char *)buf, lstrlen(buf) + 1);
              }
            }*/
                    }
                }
            }
        }
        //    Started = true;
        if (DlgWin)
        {
            SendMessage(DlgWin, DM_SETDEFID, IDCANCEL, 0);
            SetFocus(GetDlgItem(DlgWin, IDCANCEL));
            EnablePathControl(FALSE);
            EnableWindow(GetDlgItem(DlgWin, IDOK), FALSE);
            SendDlgItemMessage(DlgWin, IDC_PATH, EM_SETREADONLY, TRUE, 0);
            SetDlgItemText(DlgWin, IDCANCEL, StringTable[STR_STOP]);
            ShowWindow(GetDlgItem(DlgWin, IDC_TEXT), SW_HIDE);
            ShowWindow(GetDlgItem(DlgWin, IDC_FILE), SW_SHOW);
            ShowWindow(GetDlgItem(DlgWin, IDC_FILENAME), SW_SHOW);
            ShowWindow(GetDlgItem(DlgWin, IDC_PROGRESS), SW_SHOW);
            InvalidateRect(DlgWin, NULL, FALSE);
            UpdateWindow(DlgWin);
        }
        DWORD threadId;
        Thread = CreateThread(NULL, 0, ExtractThreadProc, 0, 0, &threadId);
        if (!Thread)
            return HandleError(STR_ERROR_TCREATE, GetLastError());
    }
    else
    {

    notOK:
        if (DlgWin)
            EnablePathControl(TRUE);
        else
        {
            CloseMapping();
            return 1;
        }
    }

    return 0;
}

int CALLBACK DirectoryBrowse(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    if (uMsg == BFFM_INITIALIZED)
    {
        SetWindowText(hwnd, StringTable[STR_BROWSEDIRTITLE]);
        if (GetRootLen(TargetPath) < lstrlen(TargetPath)) // neni to root-dir
            PathRemoveBackslash(TargetPath);
        else
            PathAddBackslash(TargetPath);
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)TargetPath);
    }
    return 0;
}

void OnBrowse()
{
    SendDlgItemMessage(DlgWin, IDC_PATH, WM_GETTEXT, MAX_PATH, (LPARAM)TargetPath);

    char display[MAX_PATH];
    BROWSEINFO bi;
    bi.hwndOwner = DlgWin;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = display;
    bi.lpszTitle = StringTable[STR_BROWSEDIRCOMMENT];
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    bi.lpfn = DirectoryBrowse;
    bi.lParam = 0;
    ITEMIDLIST* res = SHBrowseForFolder(&bi);
    if (res != NULL)
    {
        SHGetPathFromIDList(res, TargetPath);
        SetDlgItemText(DlgWin, IDC_PATH, TargetPath);
    }
    // uvolneni item-id-listu
    IMalloc* alloc;
    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
    {
        if (alloc->DidAlloc(res) == 1)
            alloc->Free(res);
        alloc->Release();
    }
}

BOOL SfxOnInit(WPARAM wParam, LPARAM lParam)
{
    SendMessage(ProgressWin, WM_USER_INITCONTROL, 0, 0);
    EnableMenuItem(GetSystemMenu(DlgWin, FALSE), SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(GetSystemMenu(DlgWin, FALSE), SC_SIZE, MF_BYCOMMAND | MF_GRAYED);
    HICON icon = LoadIcon(HInstance, MAKEINTRESOURCE(SE_IDI_ICON));
    SendMessage(DlgWin, WM_SETICON, ICON_BIG, (LPARAM)icon);
    SetWindowText(DlgWin, Title);
    SetDlgItemText(DlgWin, IDC_TARGETPATH,
                   StringTable[ArchiveStart->Flags & SE_TEMPDIR ? STR_TEMPDIR : STR_EXTRDIR]);
    SendDlgItemMessage(DlgWin, IDC_PATH, EM_SETLIMITTEXT, MAX_PATH - 1, 0);
    SetDlgItemText(DlgWin, IDC_PATH, TargetPath);
    SetDlgItemText(DlgWin, IDOK, (char*)ArchiveStart + ArchiveStart->ExtractBtnTextOffs);
    SetDlgItemText(DlgWin, IDC_ABOUT, StringTable[STR_ABOUT1]);
    SetDlgItemText(DlgWin, IDC_VENDOR, (char*)ArchiveStart + ArchiveStart->VendorOffs);
    SetDlgItemText(DlgWin, IDC_WEBLINK, (char*)ArchiveStart + ArchiveStart->WWWOffs);
    ShowWindow(GetDlgItem(DlgWin, IDC_FILE), SW_HIDE);
    ShowWindow(GetDlgItem(DlgWin, IDC_PROGRESS), SW_HIDE);
    ShowWindow(GetDlgItem(DlgWin, IDC_FILENAME), SW_HIDE);
    SetDlgItemText(DlgWin, IDC_TEXT, (char*)ArchiveStart + ArchiveStart->TextOffs);
    //LONG style = GetWindowLong(WebLinkControl, GWL_STYLE);
    //if (style)
    //{
    //  style = style | SS_NOTIFY;
    //  SetWindowLong(WebLinkControl, GWL_STYLE, style);
    //  //SetWindowPos(WebLinkControl, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    //}
    SetDlgItemText(DlgWin, IDC_ABOUTTEXT, (char*)ArchiveStart + ArchiveStart->AboutOffs);
    CenterDialog(DlgWin);
    SetFocus(GetDlgItem(DlgWin, IDOK));

    RECT r;
    GetWindowRect(GetDlgItem(DlgWin, IDC_PATH), &r);
    ShortPathControlWidth = r.right - r.left;
    ShortPathControlHeigth = r.bottom - r.top;

    GetWindowRect(DlgWin, &r);
    DlgWinWidth = r.right - r.left;
    DlgWinAboutHeigth = r.bottom - r.top;
    RECT r2;
    GetWindowRect(GetDlgItem(DlgWin, IDC_SEPARATOR), &r2);
    // aplikace prelozena s novejsim platform toolset pracuje jinak s velikosti oken, vice viz
    // https://social.msdn.microsoft.com/Forums/vstudio/en-US/7ca548b5-8931-41dc-ac1d-ed9aed223d7a/different-dialog-box-position-and-size-with-visual-c-2012
    // https://connect.microsoft.com/VisualStudio/feedback/details/768135/different-dialog-box-size-and-position-when-compiled-in-visual-c-2012-vs-2010-2008
    // proto nefunguje puvodni reseni (ve vc2012+ je okno nizsi nez ve vc2010-): DlgWinHeigth = r2.top - r.top + 1;
    // reseni: DlgWinHeigth = plna vyska okna minus rozdil vysky client-area dialogu a umisteni separatoru
    //         v ramci client-area dialogu
    POINT p;
    p.x = r2.left;
    p.y = r2.top;
    ScreenToClient(DlgWin, &p);
    GetClientRect(DlgWin, &r2);
    DlgWinHeigth = DlgWinAboutHeigth - (r2.bottom - p.y) - 2; // -2 pro dosazeni puvodni vysky z vc2010-

    SetWindowPos(DlgWin, 0, 0, 0, DlgWinWidth, DlgWinHeigth, SWP_NOMOVE | SWP_NOZORDER);
    AboutShowed = false;

    if (ArchiveStart->Flags & SE_NOTALLOWCHANGE && !SafeMode)
        EnablePathControl(FALSE);
    if (ArchiveStart->Flags & SE_AUTO && !SafeMode)
    {
        PostMessage(DlgWin, WM_USER_STARTEXTRACTING, 0, 0);
    }
    return FALSE;
}

BOOL WINAPI SfxDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static bool processingCommand = false;
    static HWND text;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        DlgWin = hDlg;
        ProgressWin = GetDlgItem(hDlg, IDC_PROGRESS);
        text = GetDlgItem(hDlg, IDC_FILENAME);
        WebLinkControl = GetDlgItem(hDlg, IDC_WEBLINK);
        OrigProgressControlProc = (WNDPROC)SetWindowLong(ProgressWin, GWL_WNDPROC, (LONG)ProgressControlProc);
        OrigTextControlProc = (WNDPROC)SetWindowLong(text, GWL_WNDPROC, (LONG)TextControlProc);
        OrigTextControlProc = (WNDPROC)SetWindowLong(WebLinkControl, GWL_WNDPROC, (LONG)TextControlProc);
        return SfxOnInit(wParam, lParam);

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (!processingCommand)
            {
                processingCommand = true;
                if (StartExtracting())
                    EndDialog(DlgWin, 1);
                processingCommand = false;
            }
            return FALSE;

        case IDCANCEL:
            if (!processingCommand)
            {
                if (Extracting)
                {
                    SuspendThread(Thread);
                    processingCommand = true;
                    if (MessageBox(hDlg, StringTable[STR_ERROR_BREAK], StringTable[STR_QUESTION], MB_YESNO | MB_ICONQUESTION) == IDYES)
                    {
                        Stop = true;
                        EnableWindow(GetDlgItem(DlgWin, IDCANCEL), FALSE);
                    }
                    ResumeThread(Thread);
                }
                else
                {
                    EndDialog(DlgWin, 1);
                }
                processingCommand = false;
            }
            return FALSE;

        case IDC_BROWSE:
            if (!processingCommand)
            {
                processingCommand = true;
                OnBrowse();
                processingCommand = false;
            }
            return FALSE;

        case IDC_ABOUT:
            if (!processingCommand)
            {
                processingCommand = true;
                if (AboutShowed)
                {
                    SetWindowPos(DlgWin, 0, 0, 0, DlgWinWidth, DlgWinHeigth, SWP_NOMOVE | SWP_NOZORDER);
                    SetDlgItemText(DlgWin, IDC_ABOUT, StringTable[STR_ABOUT1]);
                    AboutShowed = false;
                }
                else
                {
                    SetWindowPos(DlgWin, 0, 0, 0, DlgWinWidth, DlgWinAboutHeigth, SWP_NOMOVE | SWP_NOZORDER);
                    SetDlgItemText(DlgWin, IDC_ABOUT, StringTable[STR_ABOUT2]);
                    AboutShowed = true;
                }
                processingCommand = false;
            }
            return FALSE;
        }
        break;

    case WM_DESTROY:
        SendMessage(ProgressWin, WM_USER_DESTROY, 0, 0);
        SetWindowLong(ProgressWin, GWL_WNDPROC, (LONG)OrigProgressControlProc);
        SetWindowLong(text, GWL_WNDPROC, (LONG)OrigTextControlProc);
        SetWindowLong(WebLinkControl, GWL_WNDPROC, (LONG)OrigTextControlProc);
        DlgWin = NULL;
        return FALSE;

    case WM_USER_STARTEXTRACTING:
        if (!processingCommand)
        {
            processingCommand = true;
            if (StartExtracting())
                EndDialog(DlgWin, 1);
            processingCommand = false;
        }
        return FALSE;

    case WM_USER_REFRESHFILENAME:
    {
        SetWindowText(text, (char*)wParam);
        InvalidateRect(text, NULL, FALSE);
        UpdateWindow(text);
        return FALSE;
    }
    }
    return FALSE;
}
