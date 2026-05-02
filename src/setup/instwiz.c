// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <shlobj.h>
#include <Shlwapi.h>

#include "infinst.h"
#include "resource.h"

#pragma comment(lib, "Shlwapi.lib")

// from windowsx.h
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

// global license flag
BOOL gLicenseAccepted = FALSE;
HWND HWizardDialog = NULL;
HWND HProgressDialog = NULL;

//
// constants
//
#define NUM_PAGES 8
#define MAX_BUF 5000
#define MAX_LINE 512
int PagesCount;

struct CWizardPage
{
    int ResID;
    DLGPROC DlgProc;
    char Title[100];
    HWND HWindow;
};

struct CWizardPage WizardPages[NUM_PAGES] = {0};

#define _TPD_IDC_BACK 5
#define _TPD_IDC_NEXT IDOK
#define _TPD_IDC_EXIT IDCANCEL

#define _TPD_LEFTMARGIN 4    // left margin padding for the wizard button bar
#define _TPD_BOTTOMMARGIN 26 // height of the bottom strip beneath the dialogs
#define _TPD_BUTTON_W 50     // width of the buttons
#define _TPD_BUTTON_H 14     // height of the buttons

RECT ChildDialogRect;      // position of the child dialog within the wizard
HWND HChildDialog = NULL;  // current child dialog
int CurrentPageIndex = -1; // current page number
HFONT HBoldFont = NULL;
HFONT HULFont = NULL;

BOOL RunningInLowColors = FALSE; // are we running in 256 colors or fewer?

BOOL CloseWizardDlgOnDeact = FALSE;

BOOL SelectPage(int pageIndex);

void NextIsDefault()
{
    SendDlgItemMessage(HWizardDialog, _TPD_IDC_BACK, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
    SendDlgItemMessage(HWizardDialog, _TPD_IDC_NEXT, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
    SendDlgItemMessage(HWizardDialog, _TPD_IDC_EXIT, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
}

void RemoveIconIfNoColors(HWND hDlg)
{
    // the high-DPI version of the icon is missing; without it the UI looks better anyway - always drop it
    DestroyWindow(GetDlgItem(hDlg, IDC_INSTALL_ICON));
    return;
    //if (RunningInLowColors)
    //  DestroyWindow(GetDlgItem(hDlg, IDC_INSTALL_ICON));
    //else
    //{
    //  HWND hItem = GetDlgItem(hDlg, IDC_INSTALL_ICON);
    //  SetParent(hItem, GetDlgItem(hDlg, IDC_WHITE_RECT));
    //}
}

//************************************************************************************
//
// CommonControlsDlgProc
//

INT_PTR CALLBACK
CommonControlsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        char buff[500];
        char buff2[500];
        wsprintf(buff, "%d.%d", CCMajorVer, CCMinorVer);
        SetDlgItemText(hDlg, IDC_CC_VERSION, buff);

        GetDlgItemText(hDlg, IDC_CC_VERSION_NEED, buff, 500);
        wsprintf(buff2, buff, CCMajorVerNeed, CCMinorVerNeed);
        SetDlgItemText(hDlg, IDC_CC_VERSION_NEED, buff2);

        CenterWindow(hDlg);

        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
        case IDOK:
        {
            EndDialog(hDlg, wParam);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE;
}

//************************************************************************************
//
// WelcomeDlgProc
//

void DrawLogoBitmap(HDC hDC, RECT r)
{
    BITMAP bmp;
    HBITMAP hBitmap = LoadBitmap(HInstance, MAKEINTRESOURCE(IDB_WELCOME));
    HDC hMemDC = CreateCompatibleDC(NULL);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    GetObject(hBitmap, sizeof(bmp), &bmp);
    BitBlt(hDC, r.right - bmp.bmWidth, 0, bmp.bmWidth, bmp.bmHeight, hMemDC, 0, 0, SRCCOPY);
    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
}

INT_PTR CALLBACK
WelcomeDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SendMessage(HWizardDialog, DM_SETDEFID, _TPD_IDC_NEXT, 0);
        InsertAppName(hDlg, IDC_WLC_A, FALSE);
        InsertAppName(hDlg, IDC_WLC_B, TRUE);
        if (SetupInfo.DisplayWelcomeWarning == 1 || SetupInfo.DisplayWelcomeWarning == 2)
            SetDlgItemText(hDlg, IDC_WLC_WARNING, LoadStr(SetupInfo.DisplayWelcomeWarning == 1 ? IDS_PREVIEWBUILDWARNING : IDS_BETAWARNING));
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        if ((HWND)lParam == GetDlgItem(hDlg, IDC_WLC_A))
            SelectObject((HDC)wParam, HBoldFont);
        if ((HWND)lParam == GetDlgItem(hDlg, IDC_WLC_WARNING))
            SelectObject((HDC)wParam, HBoldFont);
        return (INT_PTR)GetStockObject(WHITE_BRUSH);
    }

    case WM_ERASEBKGND:
    {
        HDC hDC = (HDC)wParam;
        RECT r;
        GetClientRect(hDlg, &r);
        FillRect(hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
        DrawLogoBitmap(hDC, r);
        return TRUE; // background is erased
    }

    case WM_NOTIFY:
    {
        NMHDR* hdr = (NMHDR*)lParam;
        switch (hdr->code)
        {
        case PSN_SETACTIVE:
        {
            SetDlgItemText(HWizardDialog, _TPD_IDC_BACK, LoadStr(IDS_WZR_BACK));
            SetDlgItemText(HWizardDialog, _TPD_IDC_NEXT, LoadStr(IDS_WZR_NEXT));
            SetDlgItemText(HWizardDialog, _TPD_IDC_EXIT, LoadStr(IDS_WZR_EXIT));
            NextIsDefault();
            SetFocus(GetDlgItem(HWizardDialog, _TPD_IDC_NEXT));
            break;
        }
        }
        break;
    }
    }

    return FALSE;
}

//************************************************************************************
//
// ReadmeDlgProc
//

WNDPROC PrevWndProc = NULL;

LRESULT CALLBACK
MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
    {
        if (wParam == VK_RETURN)
            PostMessage(HWizardDialog, WM_COMMAND, _TPD_IDC_NEXT, 0);
        break;
    }
    }
    if (PrevWndProc == NULL)
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    else
        return CallWindowProc(PrevWndProc, hWnd, uMsg, wParam, lParam);
}

INT_PTR CALLBACK
ReadmeDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RemoveIconIfNoColors(hDlg);
        PrevWndProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg, IDC_SLA_LICENSE), GWLP_WNDPROC, (LPARAM)MainWindowProc);
        SetWindowText(GetDlgItem(hDlg, IDC_SLA_LICENSE), SetupInfo.FirstReadme);
        InsertAppName(hDlg, IDC_SLA_BOTTOM, FALSE);
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        int childID = (int)(DWORD_PTR)GetMenu((HWND)lParam);

        if (childID == IDC_BOLDWHITE_TEXT)
            SelectObject((HDC)wParam, HBoldFont);

        if (childID == IDC_WHITE_RECT || childID == IDC_BOLDWHITE_TEXT || childID == IDC_NORMALWHITE_TEXT)
            return (INT_PTR)GetStockObject(WHITE_BRUSH);
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDCANCEL)
            PostMessage(HWizardDialog, WM_COMMAND, IDCANCEL, 0);
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* hdr = (NMHDR*)lParam;
        switch (hdr->code)
        {
        case PSN_SETACTIVE:
        {
            NextIsDefault();
            SetFocus(GetDlgItem(hDlg, IDC_SLA_LICENSE));
            break;
        }
        }
        break;
    }
    }
    return FALSE;
}

//************************************************************************************
//
// LicenseDlgProc
//

/*
WNDPROC PrevWndProc = NULL;
LRESULT CALLBACK
MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_KEYDOWN:
    {
      if (wParam == VK_RETURN)
        PostMessage(HWizardDialog, WM_COMMAND, _TPD_IDC_NEXT, 0);
      break;
    }
  }
  if (PrevWndProc == NULL)
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
  else
    return CallWindowProc(PrevWndProc, hWnd, uMsg, wParam, lParam);
}
*/
INT_PTR CALLBACK
LicenseDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RemoveIconIfNoColors(hDlg);
        PrevWndProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg, IDC_SLA_LICENSE), GWLP_WNDPROC, (LPARAM)MainWindowProc);
        SetWindowText(GetDlgItem(hDlg, IDC_SLA_LICENSE), SetupInfo.License);
        InsertAppName(hDlg, IDC_SLA_BOTTOM, FALSE);
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        int childID = (int)(DWORD_PTR)GetMenu((HWND)lParam);

        if (childID == IDC_BOLDWHITE_TEXT)
            SelectObject((HDC)wParam, HBoldFont);

        if (childID == IDC_WHITE_RECT || childID == IDC_BOLDWHITE_TEXT || childID == IDC_NORMALWHITE_TEXT)
            return (INT_PTR)GetStockObject(WHITE_BRUSH);
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDCANCEL)
            PostMessage(HWizardDialog, WM_COMMAND, IDCANCEL, 0);
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* hdr = (NMHDR*)lParam;
        switch (hdr->code)
        {
        case PSN_SETACTIVE:
        {
            SetDlgItemText(HWizardDialog, _TPD_IDC_NEXT, LoadStr(IDS_WZR_YES));
            SetDlgItemText(HWizardDialog, _TPD_IDC_EXIT, LoadStr(IDS_WZR_NO));
            NextIsDefault();
            SetFocus(GetDlgItem(hDlg, IDC_SLA_LICENSE));
            break;
        }

        case PSN_KILLACTIVE:
        {
            SetDlgItemText(HWizardDialog, _TPD_IDC_NEXT, LoadStr(IDS_WZR_NEXT));
            SetDlgItemText(HWizardDialog, _TPD_IDC_EXIT, LoadStr(IDS_WZR_EXIT));
            break;
        }
        }
        break;
    }
    }
    return FALSE;
}

//************************************************************************************
//
// DestinationDlgProc
//

struct CBrowseData
{
    const char* Title;
    const char* InitDir;
};

int CALLBACK DirectoryBrowse(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    if (uMsg == BFFM_INITIALIZED)
    {
        CenterWindow(hwnd);

        // set the header
        SetWindowText(hwnd, ((struct CBrowseData*)lpData)->Title);
        if (((struct CBrowseData*)lpData)->InitDir != NULL)
        {
            char path[MAX_PATH];
            lstrcpy(path, ((struct CBrowseData*)lpData)->InitDir);
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)path);
        }
    }
    if (uMsg == BFFM_SELCHANGED)
    {
        if ((ITEMIDLIST*)lParam != NULL)
        {
            char path[MAX_PATH];
            BOOL ret = SHGetPathFromIDList((ITEMIDLIST*)lParam, path);
            SendMessage(hwnd, BFFM_ENABLEOK, 0, ret);
        }
    }
    return 0;
}

BOOL GetTargetDirectory(HWND parent, const char* title, const char* comment,
                        char* path, BOOL onlyNet, const char* initDir)
{
    IMalloc* alloc;
    BOOL ret;
    LPITEMIDLIST res;
    struct CBrowseData bd;
    BROWSEINFO bi;
    char display[MAX_PATH];
    ITEMIDLIST* pidl; // select the root folder
    if (onlyNet)
        SHGetSpecialFolderLocation(parent, CSIDL_NETWORK, &pidl);
    else
        pidl = NULL;

    // open the dialog
    bi.hwndOwner = parent;
    bi.pidlRoot = pidl;
    bi.pszDisplayName = display;
    bi.lpszTitle = comment;
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    bi.lpfn = DirectoryBrowse;
    bd.Title = title;
    bd.InitDir = initDir;
    bi.lParam = (LPARAM)&bd;
    res = SHBrowseForFolder(&bi);
    ret = FALSE; // return value
    if (res != NULL)
    {
        SHGetPathFromIDList(res, path);
        ret = TRUE;
    }
    // release the item ID lists
    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
    {
        if (alloc->lpVtbl->DidAlloc(alloc, pidl) == 1)
            alloc->lpVtbl->Free(alloc, pidl);
        if (alloc->lpVtbl->DidAlloc(alloc, res) == 1)
            alloc->lpVtbl->Free(alloc, res);
        alloc->lpVtbl->Release(alloc);
    }
    return ret;
}

void GetRootPath(char* root, const char* path)
{
    if (path[0] == '\\' && path[1] == '\\') // UNC
    {
        int len;
        const char* s = path + 2;
        while (*s != 0 && *s != '\\')
            s++;
        s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        len = 0;
        while (len < s - path)
        {
            *root = *(path + len);
            len++;
        }
        //    MoveMemory(root, path, s - path);
        root[s - path] = '\\';
        root[(s - path) + 1] = 0;
    }
    else
    {
        lstrcpy(root, " :\\");
        root[0] = path[0];
    }
}

unsigned __int64 MyGetFileSize(const char* fileName)
{
    unsigned __int64 size = 0;
    HANDLE hFile = CreateFile(fileName, GENERIC_READ,
                              FILE_SHARE_READ, NULL,
                              OPEN_EXISTING,
                              FILE_FLAG_SEQUENTIAL_SCAN,
                              NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD sizeDW = GetFileSize(hFile, NULL);
        size = (unsigned __int64)sizeDW;
        CloseHandle(hFile);
    }
    return size;
}

unsigned __int64 GetRequiredSpace()
{
    char src[MAX_PATH];
    const char* from;
    const char* to;
    int i;
    unsigned __int64 size;

    from = SetupInfo.CopyFrom;
    to = SetupInfo.CopyTo;
    size = 0;

    for (i = 0; i < SetupInfo.CopyCount; i++)
    {
        lstrcpy(src, from);
        ExpandPath(src);
        size += MyGetFileSize(src);

        from += lstrlen(from) + 1;
        to += lstrlen(to) + 1;
    }
    return size;
}

BOOL FindOutIfThisIsUpgrade(HWND hDlg)
{
    BOOL sameOrOlderVersion;
    BOOL sameVersion;
    BOOL foundRemoveLog;

    SetupInfo.UninstallExistingVersion = FALSE;
    SetupInfo.TheExistingVersionIsSame = FALSE;

    // verify whether the target contains an EXE with the same name
    if (FindConflictWithAnotherVersion(&sameOrOlderVersion, &sameVersion, &foundRemoveLog))
    {
        // if it is a newer version of Salamander, we do not know how it should be uninstalled and it makes no sense
        // to consider the presence of a remove log, because it might be a different uninstall technology -- show IDS_CONFLICTWITHNEWER

        // if it is the same or an older version of Salamander and no remove log is found, overwrite it without asking
        if (!(sameOrOlderVersion && !foundRemoveLog))
        {
            int cnfrmRet = IDOK;
            if (!SetupInfo.Silent)
            {
                int textID;
                char text[1024];
                if (sameOrOlderVersion)
                    textID = sameVersion ? IDS_CONFLICTSAME : IDS_CONFLICTPRIOR;
                else
                    textID = IDS_CONFLICTWITHNEWER;
                wsprintf(text, LoadStr(textID), SetupInfo.DefaultDirectory);
                cnfrmRet = MessageBox(hDlg, text, MAINWINDOW_TITLE, (sameOrOlderVersion ? MB_OKCANCEL : MB_OK) | MB_ICONINFORMATION);
            }
            if (!sameOrOlderVersion || cnfrmRet == IDCANCEL)
            {
                return FALSE;
            }
            else
            {
                SetupInfo.UninstallExistingVersion = TRUE;
                SetupInfo.TheExistingVersionIsSame = sameVersion;
            }
        }
    }
    return TRUE;
}

INT_PTR CALLBACK
DestinationDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RemoveIconIfNoColors(hDlg);
        InsertAppName(hDlg, IDC_CDL_1, FALSE);
        SendDlgItemMessage(hDlg, IDC_CDL_PATH, WM_SETTEXT, 0,
                           (LPARAM)SetupInfo.DefaultDirectory);
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        int childID = (int)(DWORD_PTR)GetMenu((HWND)lParam);

        if (childID == IDC_BOLDWHITE_TEXT)
            SelectObject((HDC)wParam, HBoldFont);

        if (childID == IDC_WHITE_RECT || childID == IDC_BOLDWHITE_TEXT || childID == IDC_NORMALWHITE_TEXT)
            return (INT_PTR)GetStockObject(WHITE_BRUSH);
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam == IDC_CDL_BROWSE))
        {
            char buff[MAX_PATH];
            char* s;
            GetDlgItemText(hDlg, IDC_CDL_PATH, buff, MAX_PATH);
            SendDlgItemMessage(hDlg, IDC_CDL_PATH, WM_GETTEXT, MAX_PATH, (LPARAM)buff);

            s = LoadStr(IDS_CHOOSEDIR);
            if (GetTargetDirectory(hDlg, s, s, buff, FALSE, buff))
            {
                SendDlgItemMessage(hDlg, IDC_CDL_PATH, WM_SETTEXT, 0, (LPARAM)buff);
            }
            return 0;
        }
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* hdr = (NMHDR*)lParam;
        switch (hdr->code)
        {
        case PSN_SETACTIVE:
        {
            SendDlgItemMessage(hDlg, IDC_CDL_PATH, EM_SETSEL, 0, -1);
            NextIsDefault();
            SetFocus(GetDlgItem(hDlg, IDC_CDL_PATH));
            break;
        }

        case PSN_KILLACTIVE:
        {
            char text[1024];
            if (wParam == 1)
            {
                BOOL exist;
                //            BOOL bResult;
                int ret;
                char buff[MAX_PATH];
                char curDir[MAX_PATH];
                char driveSpec[MAX_PATH];
                char backupDefaultDirectory[MAX_PATH];
                char backupCreateDirectory[MAX_PATH];

                DWORD bytesPerSector;
                DWORD sectorsPerCluster;
                DWORD numberOfFreeClusters;
                DWORD dummy;
                unsigned __int64 freeSpace;
                unsigned __int64 requiredSpace;

                // grab the path from the edit box
                SendDlgItemMessage(hDlg, IDC_CDL_PATH, WM_GETTEXT, MAX_PATH, (LPARAM)buff);

                if (SetupInfo.EnsureSalamander25Dir[0] != 0)
                {
                    char langPath[MAX_PATH];
                    char* end;
                    lstrcpy(langPath, buff);
                    end = langPath + lstrlen(langPath);
                    if (end > langPath && *(end - 1) != '\\')
                    {
                        *end = '\\';
                        end++;
                    }
                    lstrcpy(end, "lang");

                    GetCurrentDirectory(MAX_PATH, curDir);
                    exist = SetCurrentDirectory(langPath);
                    SetCurrentDirectory(curDir);

                    if (!exist)
                    {
                        MessageBox(hDlg, SetupInfo.EnsureSalamander25Dir, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
                        SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
                        return 1;
                    }
                }

                if (lstrcmp(SetupInfo.CreateDirectory, buff) != 0)
                {
                    GetCurrentDirectory(MAX_PATH, curDir);
                    exist = SetCurrentDirectory(buff);
                    SetCurrentDirectory(curDir);

                    ret = IDOK;
                    // JRY - removed the confirmation for creating a non-existent directory; other installers do not ask
                    /*
              if (!exist)
              {
                wsprintf(text, LoadStr(IDS_NODIR), buff);
                ret = MessageBox(hDlg, text, MAINWINDOW_TITLE, MB_YESNO | MB_ICONQUESTION);
              }
              if (ret == IDNO)
              {
                SetWindowLongPtr(hDlg,	DWLP_MSGRESULT, TRUE);
                return 1;
              }
              */
                }

                //            if(!bResult)
                //            {
                //              SetWindowLong(hDlg,	DWL_MSGRESULT, TRUE);
                //              return 1;
                //            }

                // backup in case of failure
                lstrcpy(backupDefaultDirectory, SetupInfo.DefaultDirectory);
                lstrcpy(backupCreateDirectory, SetupInfo.CreateDirectory);

                // need to set the globals so that paths expand correctly during the conflict check
                lstrcpy(SetupInfo.DefaultDirectory, buff);
                lstrcpy(SetupInfo.CreateDirectory, buff);

                if (!FindOutIfThisIsUpgrade(hDlg)) // returns FALSE only if the user does not wish to continue or it is impossible (we cannot upgrade a newer version)
                {
                    // select the path in the edit line to highlight that it should be changed
                    SetFocus(GetDlgItem(hDlg, IDC_CDL_PATH));

                    // if we would overwrite a newer version or the user does not want to upgrade the existing one, do not allow proceeding
                    SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
                    // restore the previous values instead
                    lstrcpy(SetupInfo.DefaultDirectory, backupDefaultDirectory);
                    lstrcpy(SetupInfo.CreateDirectory, backupCreateDirectory);
                    return 1;
                }

                GetRootPath(driveSpec, buff);

                if (!GetDiskFreeSpace(driveSpec, &sectorsPerCluster, &bytesPerSector,
                                      &numberOfFreeClusters, &dummy))
                {
                    wsprintf(text, LoadStr(IDS_INVALIDPATH), buff);
                    MessageBox(hDlg, text, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
                    SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
                    return 1;
                }

                freeSpace = (unsigned __int64)bytesPerSector *
                            (unsigned __int64)sectorsPerCluster *
                            (unsigned __int64)numberOfFreeClusters;

                requiredSpace = GetRequiredSpace();
                requiredSpace += 100000; // 100KB for the uninstall log (should be more than enough)

                if (freeSpace < requiredSpace)
                {
                    // I downloaded it to try your product but it's failing to install because it thinks there
                    // isn't enough HD space. It reports space req 2812901, avail 652800. This is wrong since
                    // MS explorer reports 2.64 GB used, 16.0 GB free.
                    // My system is new, running Win Pro 2000. Do you know what the problem is? Fix?
                    //
                    // John: based on this email I am adding the option to ignore a lack of disk space
                    wsprintf(text, LoadStr(IDS_NOSPACE), driveSpec, (DWORD)requiredSpace, (DWORD)freeSpace);
                    ret = MessageBox(hDlg, text, MAINWINDOW_TITLE, MB_OKCANCEL | MB_ICONINFORMATION);
                    if (ret == IDOK)
                    {
                        SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
                        return 1;
                    }
                }

                if (lstrlen(buff) > 0)
                    if (buff[lstrlen(buff) - 1] == '\\')
                        buff[lstrlen(buff) - 1] = 0;

                SetWindowLongPtr(hDlg, DWLP_MSGRESULT, FALSE);
            }
            else
                SetWindowLongPtr(hDlg, DWLP_MSGRESULT, FALSE);
            return 1;
        }
        }
        break;
    }
    }
    return FALSE;
}

//************************************************************************************
//
// FolderDlgProc
//

void GetFoldersPaths()
{
    if (SfxDirectoriesValid) // Vista or later: take the paths stored in the non-elevated process (SFX); when elevating we might switch to the admin user and the paths would differ from what we need
    {
        lstrcpyn(DesktopDirectory, SetupInfo.CommonFolders ? SfxDirectories[0] : SfxDirectories[1], MAX_PATH);
        lstrcpyn(StartMenuDirectory, SetupInfo.CommonFolders ? SfxDirectories[2] : SfxDirectories[3], MAX_PATH);
        lstrcpyn(StartMenuProgramDirectory, SetupInfo.CommonFolders ? SfxDirectories[4] : SfxDirectories[5], MAX_PATH);
        lstrcpyn(QuickLaunchDirectory, SfxDirectories[6], MAX_PATH);
    }
    else
    {
        GetSpecialFolderPath(SetupInfo.CommonFolders ? CSIDL_COMMON_DESKTOPDIRECTORY : CSIDL_DESKTOPDIRECTORY, DesktopDirectory);
        GetSpecialFolderPath(SetupInfo.CommonFolders ? CSIDL_COMMON_STARTMENU : CSIDL_STARTMENU, StartMenuDirectory);
        GetSpecialFolderPath(SetupInfo.CommonFolders ? CSIDL_COMMON_PROGRAMS : CSIDL_PROGRAMS, StartMenuProgramDirectory);
        // we can obtain CSIDL_COMMON_APPDATA, but adding a shortcut to Quick Launch has no effect, see the NEWS
        GetSpecialFolderPath(CSIDL_APPDATA, QuickLaunchDirectory); // always use the user's Quick Launch toolbar
    }
    if (lstrlen(QuickLaunchDirectory) > 0)
    {
        if (QuickLaunchDirectory[lstrlen(QuickLaunchDirectory) - 1] == '\\')
            QuickLaunchDirectory[lstrlen(QuickLaunchDirectory) - 1] = 0;
        lstrcat(QuickLaunchDirectory, "\\Microsoft\\Internet Explorer\\Quick Launch");
    }
}

void EnableFolderButtons(HWND hDlg)
{
    BOOL enable = SetupInfo.ShortcutInDesktop |
                  SetupInfo.ShortcutInStartMenu;
    EnableWindow(GetDlgItem(hDlg, IDC_SPF_PERSONAL), enable);
    EnableWindow(GetDlgItem(hDlg, IDC_SPF_COMMON), enable);
    //EnableWindow(GetDlgItem(hDlg, IDC_SPF_PINTOTASKBAR), SetupInfo.ShortcutInStartMenu);
    //if (!SetupInfo.ShortcutInStartMenu)
    //  CheckDlgButton(hDlg, IDC_SPF_PINTOTASKBAR, BST_UNCHECKED);
}

INT_PTR CALLBACK
FolderDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RemoveIconIfNoColors(hDlg);
        ShowWindow(GetDlgItem(hDlg, IDC_SPF_DESKTOP), SetupInfo.DesktopPresent);
        ShowWindow(GetDlgItem(hDlg, IDC_SPF_LINK), SetupInfo.StartMenuPresent);
        //ShowWindow(GetDlgItem(hDlg, IDC_SPF_PINTOTASKBAR), SetupInfo.StartMenuPresent);

        ShowWindow(GetDlgItem(hDlg, IDC_SPF_FOLDERS), TRUE);
        ShowWindow(GetDlgItem(hDlg, IDC_SPF_COMMON), TRUE);
        ShowWindow(GetDlgItem(hDlg, IDC_SPF_PERSONAL), TRUE);
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        int childID = (int)(DWORD_PTR)GetMenu((HWND)lParam);

        if (childID == IDC_BOLDWHITE_TEXT)
            SelectObject((HDC)wParam, HBoldFont);

        if (childID == IDC_WHITE_RECT || childID == IDC_BOLDWHITE_TEXT || childID == IDC_NORMALWHITE_TEXT)
            return (INT_PTR)GetStockObject(WHITE_BRUSH);
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            SetupInfo.ShortcutInDesktop = SetupInfo.DesktopPresent && IsDlgButtonChecked(hDlg, IDC_SPF_DESKTOP) == BST_CHECKED;
            SetupInfo.ShortcutInStartMenu = SetupInfo.StartMenuPresent && IsDlgButtonChecked(hDlg, IDC_SPF_LINK) == BST_CHECKED;
            //SetupInfo.PinToTaskbar = SetupInfo.StartMenuPresent && SetupInfo.ShortcutInStartMenu &&
            //                         IsDlgButtonChecked(hDlg, IDC_SPF_PINTOTASKBAR) == BST_CHECKED;

            SetupInfo.CommonFolders = IsDlgButtonChecked(hDlg, IDC_SPF_COMMON) == BST_CHECKED;
            EnableFolderButtons(hDlg);
        }
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* hdr = (NMHDR*)lParam;
        switch (hdr->code)
        {
        case PSN_SETACTIVE:
        {
            CheckDlgButton(hDlg, IDC_SPF_DESKTOP, SetupInfo.ShortcutInDesktop ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_SPF_LINK, SetupInfo.ShortcutInStartMenu ? BST_CHECKED : BST_UNCHECKED);
            //CheckDlgButton(hDlg, IDC_SPF_PINTOTASKBAR, SetupInfo.PinToTaskbar ? BST_CHECKED : BST_UNCHECKED);

            CheckDlgButton(hDlg, IDC_SPF_PERSONAL, SetupInfo.CommonFolders ? BST_UNCHECKED : BST_CHECKED);
            CheckDlgButton(hDlg, IDC_SPF_COMMON, SetupInfo.CommonFolders ? BST_CHECKED : BST_UNCHECKED);

            EnableFolderButtons(hDlg);
            SetFocus(GetDlgItem(HWizardDialog, _TPD_IDC_NEXT));
            NextIsDefault();
            break;
        }

        case PSN_KILLACTIVE:
        {
            SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(0, BN_CLICKED), 0);

            GetFoldersPaths(); // extract the folders according to SetupInfo.CommonFolders
            break;
        }
        }
        break;
    }
    }
    return FALSE;
}

//************************************************************************************
//
// FinishDlgProc
//

void AddString(HWND hEdit, char* text)
{
    SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
}

INT_PTR CALLBACK
FinishDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RemoveIconIfNoColors(hDlg);
        InsertAppName(hDlg, IDC_NORMALWHITE_TEXT, FALSE);
        SetupInfo.ShortcutInDesktop &= SetupInfo.DesktopPresent;
        SetupInfo.ShortcutInStartMenu &= SetupInfo.StartMenuPresent;
        //SetupInfo.PinToTaskbar &= SetupInfo.StartMenuPresent;
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        int childID = (int)(DWORD_PTR)GetMenu((HWND)lParam);

        if (childID == IDC_BOLDWHITE_TEXT)
            SelectObject((HDC)wParam, HBoldFont);

        if (childID == IDC_WHITE_RECT || childID == IDC_BOLDWHITE_TEXT || childID == IDC_NORMALWHITE_TEXT)
            return (INT_PTR)GetStockObject(WHITE_BRUSH);
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* hdr = (NMHDR*)lParam;
        switch (hdr->code)
        {
        case PSN_SETACTIVE:
        {
            BOOL folders;
            HWND hEdit = GetDlgItem(hDlg, IDC_CYS_TEXT);

            SetWindowText(hEdit, "");
            AddString(hEdit, LoadStr(SetupInfo.UninstallExistingVersion ? IDS_TARGETDIRUPGRADE : IDS_TARGETDIR));
            AddString(hEdit, "\r\n");
            AddString(hEdit, "        ");
            AddString(hEdit, SetupInfo.DefaultDirectory);
            AddString(hEdit, "\r\n");
            AddString(hEdit, "\r\n");

            folders = SetupInfo.ShortcutInDesktop |
                      SetupInfo.ShortcutInStartMenu;
            if (folders)
            {
                if (SetupInfo.CommonFolders)
                    AddString(hEdit, LoadStr(IDS_COMMON));
                else
                    AddString(hEdit, LoadStr(IDS_PERSONAL));
                AddString(hEdit, "\r\n");

                if (SetupInfo.ShortcutInDesktop)
                {
                    AddString(hEdit, "        ");
                    AddString(hEdit, LoadStr(IDS_INSERT_DESKTOP));
                    AddString(hEdit, "\r\n");
                }

                if (SetupInfo.ShortcutInStartMenu)
                {
                    AddString(hEdit, "        ");
                    AddString(hEdit, LoadStr(IDS_INSERT_MENU));
                    AddString(hEdit, "\r\n");
                }

                AddString(hEdit, "\r\n");
            }

            SetDlgItemText(HWizardDialog, _TPD_IDC_NEXT, LoadStr(IDS_WZR_INSTALL));
            NextIsDefault();
            SetFocus(GetDlgItem(HWizardDialog, _TPD_IDC_NEXT));
            break;
        }

        case PSN_KILLACTIVE:
        {
            if (wParam == 0) // back
            {
                SetDlgItemText(HWizardDialog, _TPD_IDC_NEXT, LoadStr(IDS_WZR_NEXT));
                break;
            }
        }
        }
        break;
    }
    }
    return FALSE;
}

//************************************************************************************
//
// UninstallDlgProc
//

BOOL SaveLastDirectory();

INT_PTR CALLBACK
UninstallDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // save immediately after starting the installation, because if a restart is required we want to push to the correct location next time
        SaveLastDirectory();

        RemoveIconIfNoColors(hDlg);
        PreviousVerOfFileToIncrementContent[0] = 0;
        if (!SetupInfo.UninstallExistingVersion)
        {
            DeleteAutoImportConfigMarker(SetupInfo.AutoImportConfig);
            ShowWindow(hDlg, SW_HIDE);
            PostMessage(HWizardDialog, WM_COMMAND, _TPD_IDC_NEXT, 0);
        }
        else
        {
            ShowWindow(hDlg, SW_SHOW);
            PostMessage(hDlg, WM_USER_STARTUNINSTALL, 0, 0);
        }
        break;
    }

    case WM_USER_STARTUNINSTALL:
    {
        BOOL needRestart = FALSE;
        HProgressDialog = hDlg;
        UpdateWindow(hDlg);
        if (!DoUninstall(hDlg, &needRestart))
        {
            char buff[1024];
            if (needRestart)
            {
                wsprintf(buff, LoadStr(IDS_INSTNEEDRESTART), SetupInfo.ApplicationName);
                MessageBox(hDlg, buff, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                wsprintf(buff, LoadStr(IDS_INSTFAILED), SetupInfo.ApplicationName);
                MessageBox(hDlg, buff, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
            }
            PostQuitMessage(0);
            return 0;
        }
        PostMessage(HWizardDialog, WM_COMMAND, _TPD_IDC_NEXT, 0);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        int childID = (int)(DWORD_PTR)GetMenu((HWND)lParam);

        if (childID == IDC_BOLDWHITE_TEXT)
            SelectObject((HDC)wParam, HBoldFont);

        if (childID == IDC_WHITE_RECT || childID == IDC_BOLDWHITE_TEXT || childID == IDC_NORMALWHITE_TEXT)
            return (INT_PTR)GetStockObject(WHITE_BRUSH);
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* hdr = (NMHDR*)lParam;
        switch (hdr->code)
        {
        case PSN_SETACTIVE:
        {
            ShowWindow(GetDlgItem(HWizardDialog, _TPD_IDC_BACK), SW_HIDE);
            ShowWindow(GetDlgItem(HWizardDialog, _TPD_IDC_NEXT), SW_HIDE);
            //          SetDlgItemText(HWizardDialog, _TPD_IDC_EXIT, LoadStr(IDS_WZR_FINISH));
            EnableWindow(GetDlgItem(HWizardDialog, _TPD_IDC_EXIT), FALSE);

            SendMessage(HWizardDialog, DM_SETDEFID, _TPD_IDC_EXIT, 0);
            SendDlgItemMessage(HWizardDialog, _TPD_IDC_EXIT, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
            SetFocus(GetDlgItem(HWizardDialog, _TPD_IDC_EXIT));
            break;
        }
        }
        break;
    }
    }
    return FALSE;
}

//************************************************************************************
//
// ProgressDlgProc
//

INT_PTR CALLBACK
ProgressDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RemoveIconIfNoColors(hDlg);
        PostMessage(hDlg, WM_USER_STARTINSTALL, 0, 0);
        break;
    }

    case WM_USER_STARTINSTALL:
    {
        HProgressDialog = hDlg;
        UpdateWindow(hDlg);
        if (!DoInstallation())
        {
            char buff[1024];
            wsprintf(buff, LoadStr(IDS_INSTFAILED), SetupInfo.ApplicationName);
            MessageBox(hDlg, buff, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
            PostQuitMessage(0);
            return 0;
        }
        PostMessage(HWizardDialog, WM_COMMAND, _TPD_IDC_NEXT, 0);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        int childID = (int)(DWORD_PTR)GetMenu((HWND)lParam);

        if (childID == IDC_BOLDWHITE_TEXT)
            SelectObject((HDC)wParam, HBoldFont);

        if (childID == IDC_WHITE_RECT || childID == IDC_BOLDWHITE_TEXT || childID == IDC_NORMALWHITE_TEXT)
            return (INT_PTR)GetStockObject(WHITE_BRUSH);
        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* hdr = (NMHDR*)lParam;
        switch (hdr->code)
        {
        case PSN_SETACTIVE:
        {
            ShowWindow(GetDlgItem(HWizardDialog, _TPD_IDC_BACK), SW_HIDE);
            ShowWindow(GetDlgItem(HWizardDialog, _TPD_IDC_NEXT), SW_HIDE);
            //          SetDlgItemText(HWizardDialog, _TPD_IDC_EXIT, LoadStr(IDS_WZR_FINISH));
            EnableWindow(GetDlgItem(HWizardDialog, _TPD_IDC_EXIT), FALSE);

            SendMessage(HWizardDialog, DM_SETDEFID, _TPD_IDC_EXIT, 0);
            SendDlgItemMessage(HWizardDialog, _TPD_IDC_EXIT, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
            SetFocus(GetDlgItem(HWizardDialog, _TPD_IDC_EXIT));
            break;
        }
        }
        break;
    }
    }
    return FALSE;
}

void SetProgressMax(int max)
{
    if (SetupInfo.Silent)
        return;
    SendDlgItemMessage(HProgressDialog, IDC_PGS_PGS, PBM_SETRANGE, 0, MAKELPARAM(0, max));
    UpdateWindow(HProgressDialog);
}

void SetProgressPos(int pos)
{
    if (SetupInfo.Silent)
        return;
    SendDlgItemMessage(HProgressDialog, IDC_PGS_PGS, PBM_SETPOS, pos, 0);
    UpdateWindow(HProgressDialog);
}

void SetFromTo(const char* from, const char* to)
{
    if (SetupInfo.Silent)
        return;

    // "From" can be long because of the TEMP path and its end used to be clipped.
    // Switch to smarter functions that can shorten the path correctly.
    PathSetDlgItemPath(HProgressDialog, IDC_PGS_FROM, from);
    PathSetDlgItemPath(HProgressDialog, IDC_PGS_TO, to);
    //SendDlgItemMessage(HProgressDialog, IDC_PGS_FROM, WM_SETTEXT, 0, (LPARAM)from);
    //SendDlgItemMessage(HProgressDialog, IDC_PGS_TO, WM_SETTEXT, 0, (LPARAM)to);
}

//************************************************************************************
//
// MyCreateProcess
//

void MyCreateProcess(const char* fileName, BOOL parseCurDir, BOOL addQuotes)
{
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi;
    char buf[MAX_PATH];
    char buf2[MAX_PATH + 2];

    if (lstrlen(fileName) >= MAX_PATH)
        return;

    if (parseCurDir)
    {
        char* p;
        lstrcpy(buf, fileName);
        p = buf + lstrlen(buf) - 1;
        while (p > buf && *p != '\\')
            p--;
        if (p >= buf && *p == '\\')
            *p = 0;
    }

    if (addQuotes)
        wsprintf(buf2, "\"%s\"", fileName);
    else
        lstrcpy(buf2, fileName);
    si.cb = sizeof(STARTUPINFO);
    CreateProcess(NULL, buf2, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL, parseCurDir ? buf : NULL, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

//************************************************************************************
//
// DoneDlgProc
//

typedef BOOL(WINAPI* FAllowSetForegroundWindow)(DWORD dwProcessId);

INT_PTR CALLBACK
DoneDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        BOOL viewVisible;
        BOOL runVisible;
        RECT r;

        // pull the data out of the INF file
        GetPrivateProfileString(INF_PRIVATE_SECTION, INF_VIEWREADME, "",
                                SetupInfo.ViewReadmePath, MAX_PATH, InfFileName);
        GetPrivateProfileString(INF_PRIVATE_SECTION, INF_RUNPROGRAM, "",
                                SetupInfo.RunProgramPath, MAX_PATH, InfFileName);
        GetPrivateProfileString(INF_PRIVATE_SECTION, INF_RUNPROGRAMQUIET, "",
                                SetupInfo.RunProgramQuietPath, MAX_PATH, InfFileName);

        ExpandPath(SetupInfo.ViewReadmePath);
        ExpandPath(SetupInfo.RunProgramPath);
        ExpandPath(SetupInfo.RunProgramQuietPath);

        viewVisible = SetupInfo.ViewReadmePath[0] != 0 && SfxDirectoriesValid;
        runVisible = SetupInfo.RunProgramPath[0] != 0 && SfxDirectoriesValid;

        SetupInfo.RunProgramQuiet = SetupInfo.RunProgramQuietPath[0] != 0;

        ShowWindow(GetDlgItem(hDlg, IDC_DONE_README), viewVisible);
        ShowWindow(GetDlgItem(hDlg, IDC_DONE_RUN), runVisible);

        InsertAppName(hDlg, IDC_DONE_TEXT, FALSE);
        if (viewVisible || runVisible)
            InsertAppName(hDlg, IDC_DONE_TEXT2, FALSE);
        else
            SetDlgItemText(hDlg, IDC_DONE_TEXT2, "");

        if (viewVisible)
            CheckDlgButton(hDlg, IDC_DONE_README, SetupInfo.ViewReadme ? BST_CHECKED : BST_UNCHECKED);
        else
            SetupInfo.ViewReadme = FALSE;

        if (runVisible)
        {
            InsertAppName(hDlg, IDC_DONE_RUN, FALSE);
            CheckDlgButton(hDlg, IDC_DONE_RUN, SetupInfo.RunProgram ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            SetupInfo.RunProgram = FALSE;

        if (GetWindowRect(GetDlgItem(HWizardDialog, _TPD_IDC_EXIT), &r))
        {
            POINT p;
            p.x = r.left;
            p.y = r.top;
            ScreenToClient(HWizardDialog, &p);
            MoveWindow(GetDlgItem(HWizardDialog, _TPD_IDC_NEXT), p.x, p.y, r.right - r.left, r.bottom - r.top, FALSE);
        }
        ShowWindow(GetDlgItem(HWizardDialog, _TPD_IDC_EXIT), SW_HIDE);
        ShowWindow(GetDlgItem(HWizardDialog, _TPD_IDC_NEXT), SW_SHOW);
        SetDlgItemText(HWizardDialog, _TPD_IDC_NEXT, LoadStr(IDS_WZR_FINISH));

        SendMessage(HWizardDialog, DM_SETDEFID, _TPD_IDC_NEXT, 0);
        SendDlgItemMessage(HWizardDialog, _TPD_IDC_NEXT, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
        SetFocus(GetDlgItem(HWizardDialog, _TPD_IDC_NEXT));

        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        if ((HWND)lParam == GetDlgItem(hDlg, IDC_WLC_A))
            SelectObject((HDC)wParam, HBoldFont);
        return (INT_PTR)GetStockObject(WHITE_BRUSH);
    }

    case WM_ERASEBKGND:
    {
        HDC hDC = (HDC)wParam;
        RECT r;
        GetClientRect(hDlg, &r);
        FillRect(hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
        DrawLogoBitmap(hDC, r);
        return TRUE; // background is erased
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            SetupInfo.ViewReadme = IsDlgButtonChecked(hDlg, IDC_DONE_README) == BST_CHECKED;
            SetupInfo.RunProgram = IsDlgButtonChecked(hDlg, IDC_DONE_RUN) == BST_CHECKED;
        }

        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            // so the temp directory underneath us can be deleted
            char buf[MAX_PATH];
            GetFolderPath(CSIDL_SYSTEM, buf);
            SetCurrentDirectory(buf); // we must leave the directory; otherwise it cannot be deleted

            wsprintf(buf, "notepad.exe \"%s\"", SetupInfo.ViewReadmePath);
            // Vista+: the SFX process runs unelevated; when elevating we may switch to the admin account and the programs would start under a different user
            if (SfxDirectoriesValid)
            {
                StoreExecuteInfo(SetupInfo.ViewReadmePath, SetupInfo.RunProgramPath,
                                 SetupInfo.ViewReadme, SetupInfo.RunProgram);
                if (SfxDlg != NULL && (SetupInfo.ViewReadme || SetupInfo.RunProgram))
                {
                    DWORD sfxPID;
                    FAllowSetForegroundWindow allowSetForegroundWindow;

                    GetWindowThreadProcessId(SfxDlg, &sfxPID);
                    // allow the activated process to call SetForegroundWindow; otherwise it would not be able to bring itself to the front
                    allowSetForegroundWindow = (FAllowSetForegroundWindow)GetProcAddress(GetModuleHandle("user32.dll"), "AllowSetForegroundWindow");
                    if (allowSetForegroundWindow != NULL)
                        allowSetForegroundWindow(sfxPID);
                    // show and activate the SFX7ZIP dialog
                    SendMessage(SfxDlg, WM_USER_SHOWACTSFX7ZIP, 0, 0);
                }
            }

            PostQuitMessage(0);
            break;
        }
        }

        break;
    }

    case WM_TIMER:
    {
        if (wParam == WM_USER_CLOSEWIZARDDLG)
            PostQuitMessage(0); // we can no longer delay closing the wizard dialog, so exit
        break;
    }
    }
    return FALSE;
}

//************************************************************************************
//
// WizardDlgProc
//

INT_PTR CALLBACK
WizardDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HDC hdc;
        HFONT hFont;
        LOGFONT lf;
        char buff[500];
        HMENU hMenu;
        HWND hwnd;
        POINT p;
        RECT separatorRect;
        HWizardDialog = hDlg;
        CenterWindow(hDlg);
        hwnd = GetDlgItem(HWizardDialog, IDC_WIZARD_SEPARATOR);
        GetClientRect(hDlg, &ChildDialogRect);
        GetWindowRect(hwnd, &separatorRect);
        p.x = separatorRect.right;
        p.y = separatorRect.top;
        ScreenToClient(HWizardDialog, &p);
        ChildDialogRect.bottom = p.y;

        // assign an icon to the window
        SendMessage(hDlg, WM_SETICON, ICON_BIG,
                    (LPARAM)LoadIcon(HInstance, MAKEINTRESOURCE(EXE_ICON)));

        // disable items in the system menu
        hMenu = GetSystemMenu(hDlg, FALSE);
        if (hMenu != NULL)
        {
            EnableMenuItem(hMenu, SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(hMenu, SC_RESTORE, MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(hMenu, SC_SIZE, MF_BYCOMMAND | MF_GRAYED);
        }

        // assign text to the window
        wsprintf(buff, LoadStr(IDS_WIZ_TITLE), SetupInfo.ApplicationName);
        SetWindowText(hDlg, buff);

        // create a bold font
        hFont = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);
        GetObject(hFont, sizeof(lf), &lf);
        lf.lfWeight = FW_BOLD;
        HBoldFont = CreateFontIndirect(&lf);

        lf.lfWeight = FW_NORMAL;
        lf.lfUnderline = TRUE;
        HULFont = CreateFontIndirect(&lf);

        // if we run in 256 colors or fewer, do not show the symbol in the top-right corner
        hdc = GetDC(hDlg);
        RunningInLowColors = GetDeviceCaps(hdc, BITSPIXEL) <= 8;
        ReleaseDC(hDlg, hdc);

        SelectPage(0);
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        if ((HWND)lParam == GetDlgItem(hDlg, IDC_WWW))
        {
            SelectObject((HDC)wParam, HULFont);
            SetTextColor((HDC)wParam, RGB(0, 0, 255));
            SetBkColor((HDC)wParam, GetSysColor(COLOR_BTNFACE));
            return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    }

    case WM_LBUTTONDOWN:
    case WM_SETCURSOR:
    {
        DWORD pos;
        POINT p;
        BOOL hit;
        pos = GetMessagePos();
        p.x = GET_X_LPARAM(pos);
        p.y = GET_Y_LPARAM(pos);
        ScreenToClient(hDlg, &p);

        hit = ChildWindowFromPoint(hDlg, p) == GetDlgItem(hDlg, IDC_WWW);
        if (hit)
        {
            if (uMsg == WM_LBUTTONDOWN)
            {
                ShellExecute(NULL, "open", "https://www.altap.cz", NULL, NULL, 0);
            }
            else
            {
                SetCursor(LoadCursor(HInstance, MAKEINTRESOURCE(IDC_HANDICON)));
                return TRUE;
            }
        }
        break;
    }

    case WM_ACTIVATE:
    {
        if (LOWORD(wParam) == WA_INACTIVE && CloseWizardDlgOnDeact)
            PostQuitMessage(0); // they are already deactivating us; the wizard dialog is no longer needed, so exit
        break;
    }

    case WM_DESTROY:
    {
        if (HBoldFont != NULL)
        {
            DeleteObject(HBoldFont);
            HBoldFont = NULL;
        }
        if (HULFont != NULL)
        {
            DeleteObject(HULFont);
            HULFont = NULL;
        }
        break;
    }

    case WM_SYSCOMMAND:
    {
        if (wParam == SC_CLOSE && !IsWindowVisible(GetDlgItem(HWizardDialog, _TPD_IDC_EXIT)))
        { // clicking the window's close button or pressing Alt+F4 on the last page (Installation Finished): the user does not want to launch Salamander or open the readme, so exit
            PostQuitMessage(0);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_STOPINSTALL:
        {
            PostQuitMessage(0);
            return 0;
        }

        case _TPD_IDC_BACK:
        {
            NMHDR nmhdr;
            LONG_PTR res;
            HWND hPage = WizardPages[CurrentPageIndex].HWindow;
            nmhdr.hwndFrom = HWizardDialog;
            nmhdr.idFrom = 0;
            nmhdr.code = PSN_KILLACTIVE;
            SendMessage(hPage, WM_NOTIFY, 0, (LPARAM)&nmhdr);
            res = GetWindowLongPtr(hPage, DWLP_MSGRESULT);
            if (res != 0)
                return TRUE;

            SelectPage(CurrentPageIndex - 1);
            return TRUE;
        }

        case _TPD_IDC_NEXT:
        {
            NMHDR nmhdr;
            LONG_PTR res;
            HWND hPage = WizardPages[CurrentPageIndex].HWindow;
            nmhdr.hwndFrom = HWizardDialog;
            nmhdr.idFrom = 0;
            nmhdr.code = PSN_KILLACTIVE;
            SendMessage(hPage, WM_NOTIFY, 1, (LPARAM)&nmhdr);
            res = GetWindowLongPtr(hPage, DWLP_MSGRESULT);
            if (res != 0)
                return TRUE;

            if (CurrentPageIndex == PagesCount - 1)
                PostMessage(hPage, WM_COMMAND, IDOK, 0);
            else
                SelectPage(CurrentPageIndex + 1);
            return TRUE;
        }

        case _TPD_IDC_EXIT:
        {
            if (CurrentPageIndex >= PagesCount || StopInstalling() == IDOK)
                PostMessage(HWizardDialog, WM_COMMAND, ID_STOPINSTALL, 0);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE;
}

//************************************************************************************
//
// StopInstalling
//

BOOL StopInstalling()
{
    int ret;
    ret = MessageBox(HWizardDialog, LoadStr(IDS_INSTALL_STOP),
                     LoadStr(IDS_INSTALL_STOP_TITLE), MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
    return ret == IDYES;
}

//************************************************************************************
//
// AddWizardPage
//

void AddWizardPage(int index, int dlgResID, DLGPROC pfnDlgProc)
{
    WizardPages[index].ResID = dlgResID;
    WizardPages[index].DlgProc = pfnDlgProc;
}

//************************************************************************************
//
// EnableButtons
//

void EnableButtons()
{
    if (CurrentPageIndex == 0)
    {
        ShowWindow(GetDlgItem(HWizardDialog, _TPD_IDC_BACK), FALSE);
        SendDlgItemMessage(HWizardDialog, _TPD_IDC_NEXT, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
    }
    if (CurrentPageIndex == 1)
    {
        ShowWindow(GetDlgItem(HWizardDialog, _TPD_IDC_BACK), TRUE);
        SendDlgItemMessage(HWizardDialog, _TPD_IDC_BACK, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
    }
}

//************************************************************************************
//
// SelectPage
//

BOOL SelectPage(int pageIndex)
{
    HWND hHideWindow;
    NMHDR nmhdr;

    if (pageIndex < 0 || pageIndex >= PagesCount)
        return FALSE;
    hHideWindow = NULL;
    if (HChildDialog != NULL)
    {
        hHideWindow = HChildDialog;
        HChildDialog = NULL;
    }

    if (pageIndex != CurrentPageIndex)
    {
        struct CWizardPage* page;
        page = &WizardPages[pageIndex];
        if (page->HWindow == NULL)
        {
            page->HWindow = CreateDialog(HInstance, MAKEINTRESOURCE(page->ResID),
                                         HWizardDialog, page->DlgProc);
            nmhdr.hwndFrom = HWizardDialog;
            nmhdr.idFrom = 0;
            nmhdr.code = PSN_SETACTIVE;
            SendMessage(page->HWindow, WM_NOTIFY, 0, (LPARAM)&nmhdr);
        }
        HChildDialog = page->HWindow;

        nmhdr.hwndFrom = HWizardDialog;
        nmhdr.idFrom = 0;
        nmhdr.code = PSN_SETACTIVE;
        SendMessage(page->HWindow, WM_NOTIFY, 0, (LPARAM)&nmhdr);

        if (hHideWindow != NULL)
            SetWindowPos(hHideWindow, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW | SWP_HIDEWINDOW);

        SetWindowPos(page->HWindow, HWND_TOP,
                     ChildDialogRect.left, ChildDialogRect.top,
                     ChildDialogRect.right - ChildDialogRect.left,
                     ChildDialogRect.bottom - ChildDialogRect.top,
                     SWP_SHOWWINDOW);
        CurrentPageIndex = pageIndex;
        EnableButtons();
        UpdateWindow(page->HWindow);
        UpdateWindow(HWizardDialog);
    }
    return TRUE;
}

//************************************************************************************
//
// GetCurDispLangID
//

typedef LANGID(WINAPI* FGetUserDefaultUILanguage)();

WORD GetCurDispLangID()
{
    WORD langID;
    HMODULE KERNEL32DLL;

    langID = GetUserDefaultLangID();

    // adjust the langID on newer Windows versions
    KERNEL32DLL = LoadLibrary("kernel32.dll");
    if (KERNEL32DLL != NULL)
    {
        FGetUserDefaultUILanguage proc = (FGetUserDefaultUILanguage)GetProcAddress(KERNEL32DLL, "GetUserDefaultUILanguage");
        if (proc != NULL)
            langID = proc();
        FreeLibrary(KERNEL32DLL);
    }

    return langID;
}

//************************************************************************************
//
// CreateWizard
//

BOOL CreateWizard()
{
    PagesCount = 0;
    AddWizardPage(PagesCount++, IDD_WELCOME, WelcomeDlgProc);
    if (SetupInfo.UseFirstReadme)
        AddWizardPage(PagesCount++, IDD_README, ReadmeDlgProc);
    if (SetupInfo.UseLicenseFile)
        AddWizardPage(PagesCount++, IDD_LICENSE, LicenseDlgProc);
    if (!SetupInfo.SkipChooseDir)
        AddWizardPage(PagesCount++, IDD_DESTINATION, DestinationDlgProc);
    if (SetupInfo.DesktopPresent | SetupInfo.StartMenuPresent)
        AddWizardPage(PagesCount++, IDD_FOLDER, FolderDlgProc);
    AddWizardPage(PagesCount++, IDD_FINISH, FinishDlgProc);
    AddWizardPage(PagesCount++, IDD_UNINSTALL, UninstallDlgProc);
    AddWizardPage(PagesCount++, IDD_PROGRESS, ProgressDlgProc);
    AddWizardPage(PagesCount++, IDD_DONE, DoneDlgProc);
    // !!!WARNING!!! increase NUM_PAGES if we add a page

    HWizardDialog = CreateDialog(HInstance, MAKEINTRESOURCE(IDD_WIZARD), NULL, WizardDlgProc);
    if (HWizardDialog == NULL)
        return FALSE;
    ShowWindow(HWizardDialog, SW_SHOW);

    return TRUE;
}

//************************************************************************************
//
// InstallDone
//
/*
INT_PTR CALLBACK
FillFormDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      CenterWindow(hDlg);
      break;
    }
  }
  return FALSE;   
}
*/
BOOL MyGetMultilineText(char* srcData, char buff[100][MAX_PATH], int* lines)
{
    char line[10000];
    char* end;
    char* p = srcData;
    int index = 0;

    while (*p != '[' && *p != 0)
    {
        while (*p == '\r' || *p == '\n')
            p++;

        end = p;
        while (*end != '\r' && *end != '\n' && *end != 0)
            end++;

        while (*p == ' ')
            p++;

        lstrcpyn(line, p, (int)(end - p + 1));

        if (*line != ';' && *line != 0)
        {
            int len;
            if (index < 99)
                lstrcpy(buff[index], line);
            len = lstrlen(line);
            index++;
        }
        while (*end == '\r' || *end == '\n')
            end++;
        p = end;
    }
    buff[index][0] = 0;
    *lines = index;
    return TRUE;
}

char PreviousVerOfFileToIncrementContent[10000];

void ReadPreviousVerOfFileToIncrementContent()
{
    HANDLE hFile;
    DWORD read;
    char* end;
    char fileName[MAX_PATH];

    end = SetupInfo.IncrementFileContentDst;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(fileName, SetupInfo.IncrementFileContentDst, (int)(end - SetupInfo.IncrementFileContentDst + 1));
    ExpandPath(fileName); // let the path expand
    if (fileName[0] == 0)
        return; // failed to obtain even the file name

    // if the file exists, it will be opened; otherwise it will be created
    hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    if (ReadFile(hFile, PreviousVerOfFileToIncrementContent, 10000, &read, NULL) && read > 0)
        PreviousVerOfFileToIncrementContent[read] = 0;
    else
        PreviousVerOfFileToIncrementContent[0] = 0;

    CloseHandle(hFile);
}

BOOL IncrementFileContent()
{
    HANDLE hFile;
    DWORD read;
    char* begin;
    char* end;
    int index, i;
    DWORD cfgVersion;
    int ifcLinesCount = 0;
    char ifcLines[100][MAX_PATH]; // unpack the IncrementFileContent entries here;
                                  // the zeroth line will be the file name; the list ends
                                  // with a line containing only the character 0
    int fileLinesCount = 0;
    char fileLines[100][MAX_PATH]; // unpack lines from the existing file here
    char fileBuff[10000];

    DWORD registryVal;

    // first fetch the current number from the registry
    if (!GetRegistryDWORDValue(SetupInfo.IncrementFileContentSrc, &registryVal))
    { // the installed version has no registry entry (it has not run on this account yet)
        if (!SetupInfo.UninstallExistingVersion || !SetupInfo.TheExistingVersionIsSame)
            return FALSE;
        registryVal = 0;
    }

    begin = end = SetupInfo.IncrementFileContentDst;
    index = 0;

    while (*begin != 0)
    {
        while (*end != 0 && *end != ',')
            end++;
        lstrcpyn(ifcLines[index], begin, (int)(end - begin + 1));
        ExpandPath(ifcLines[index]); // expand all paths
        if (*end == ',')
            end++;
        begin = end;
        index++;
    }
    ifcLinesCount = index;
    ifcLines[index][0] = 0;

    if (index == 0)
        return FALSE; // failed to obtain even the file name

    // if the file exists, it will be opened; otherwise it will be created
    hFile = CreateFile(ifcLines[0], GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    // if the file exists, read and analyze its contents
    fileLines[0][0] = 0;
    if (PreviousVerOfFileToIncrementContent[0] != 0) // when reinstalling the same version of Salamander, reuse the contents of the recently uninstalled plugins.ver (e.g. newly installed plugins must be carried over into the new plugins.ver; otherwise launching Salamander from another account would not add them automatically because the config version is not increased—only increasing it triggers auto-installation of all plugins)
        MyGetMultilineText(PreviousVerOfFileToIncrementContent, fileLines, &fileLinesCount);
    else
    {
        if (ReadFile(hFile, fileBuff, 10000, &read, NULL) && read > 0)
        {
            fileBuff[read] = 0;
            MyGetMultilineText(fileBuff, fileLines, &fileLinesCount);
        }
    }

    // if we are creating a new file, start it with value one
    cfgVersion = 1;

    // if the file contained a number, increment it by one (installing plugins from another account and not running Salamander on this account leads to plugins.ver having a higher version number than the registry)
    if (fileLinesCount > 0)
        cfgVersion = MyStrToDWORD(fileLines[0]) + 1;

    // optionally use the registry value increased by one (we want Salamander to find and load the new version of plugins.ver on the next start)
    if (registryVal + 1 > cfgVersion)
        cfgVersion = registryVal + 1;

    wsprintf(fileBuff, "%d\r\n", cfgVersion);

    // write the new data into the file based on IncrementFileContent
    SetFilePointer(hFile, 0, 0, FILE_BEGIN);
    if (!WriteFile(hFile, fileBuff, lstrlen(fileBuff), &read, NULL) || read != (DWORD)lstrlen(fileBuff))
    {
        DWORD error = GetLastError();
        wsprintf(fileBuff, LoadStr(ERROR_CF_WRITEFILE), ifcLines[0]);
        HandleErrorM(IDS_MAINWINDOWTITLE, fileBuff, error);
        SetEndOfFile(hFile);
        CloseHandle(hFile);
        return FALSE;
    }

    for (index = 1; index < ifcLinesCount; index++)
    {
        wsprintf(fileBuff, "%d:%s\r\n", cfgVersion, ifcLines[index]);
        if (!WriteFile(hFile, fileBuff, lstrlen(fileBuff), &read, NULL) || read != (DWORD)lstrlen(fileBuff))
        {
            DWORD error = GetLastError();
            wsprintf(fileBuff, LoadStr(ERROR_CF_WRITEFILE), ifcLines[0]);
            HandleErrorM(IDS_MAINWINDOWTITLE, fileBuff, error);
            SetEndOfFile(hFile);
            CloseHandle(hFile);
            return FALSE;
        }
    }

    // scan the original file contents and add plugins that were not added now
    for (i = 1; i < fileLinesCount; i++) // the first line is the overall number - skip it
    {
        char* p = fileLines[i];
        while (*p != ':' && *p != 0)
            p++;
        if (*p == ':')
        {
            p++;
            if (*p != 0)
            {
                BOOL found;
                found = FALSE;
                for (index = 1; index < ifcLinesCount; index++)
                {
                    if (StrICmp(p, ifcLines[index]) == 0)
                    {
                        found = TRUE;
                        break;
                    }
                }
                if (!found)
                {
                    wsprintf(fileBuff, "%s\r\n", fileLines[i]);
                    if (!WriteFile(hFile, fileBuff, lstrlen(fileBuff), &read, NULL) || read != (DWORD)lstrlen(fileBuff))
                    {
                        DWORD error = GetLastError();
                        wsprintf(fileBuff, LoadStr(ERROR_CF_WRITEFILE), ifcLines[0]);
                        HandleErrorM(IDS_MAINWINDOWTITLE, fileBuff, error);
                        SetEndOfFile(hFile);
                        CloseHandle(hFile);
                        return FALSE;
                    }
                }
            }
        }
    }

    SetEndOfFile(hFile);
    CloseHandle(hFile);

    return TRUE;
}
