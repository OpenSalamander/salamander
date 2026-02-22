// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "cfgdlg.h"
#include "salamand.h"
#include "mainwnd.h"
#include "plugins.h"
#include "zip.h"
#include "usermenu.h"
#include "execute.h"
#include "pack.h"
#include "fileswnd.h"
#include "edtlbwnd.h"

// item type in the packer extensions table
struct SPackAssocItem
{
    const char* Ext; // packer extension
    int nextIndex;   // index of another packer for the same archive, -1 if none exists
};

// table of packer extension associations
// first item is the string of masks, second is the index of the next packer of the same type
SPackAssocItem PackACExtensions[] = {
    {"j", 5},           // 0
    {"rar;r##", 6},     // 1
    {"arj;a##", 10},    // 2
    {"lzh", -1},        // 3
    {"uc2", -1},        // 4
    {"j", 0},           // 5
    {"rar;r##", 1},     // 6
    {"zip;pk3;jar", 8}, // 7
    {"zip;pk3;jar", 7}, // 8, 9
    {"arj;a##", 2},     // 10
    {"ace;c##", 12},    // 11
    {"ace;c##", 11},    // 12
};

//
// ****************************************************************************
// CPackACDialog
//

// dialog procedure of the main dialog
INT_PTR
CPackACDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CPackACDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // construct the listview
        ListView = new CPackACListView(this);
        if (ListView == NULL)
        {
            TRACE_E(LOW_MEMORY);
            PostMessage(HWindow, WM_COMMAND, IDCANCEL, 0);
            break;
        }
        // subclass listview
        ListView->AttachToControl(HWindow, IDC_ACLIST);
        // create status bar
        HStatusBar = CreateWindowEx(0, STATUSCLASSNAME, (LPCTSTR)NULL,
                                    SBARS_SIZEGRIP | WS_CHILD | CCS_BOTTOM | WS_VISIBLE | WS_GROUP,
                                    0, 0, 0, 0, HWindow, (HMENU)IDC_ACSTATUS,
                                    HInstance, NULL);
        if (HStatusBar == NULL)
        {
            TRACE_E("Error creating StatusBar");
            PostMessage(HWindow, WM_COMMAND, IDCANCEL, 0);
            break;
        }
        // the Stop/Restart button becomes the Start button
        SetDlgItemText(HWindow, IDB_ACSTOP, LoadStr(IDS_ACBUTTON_RESCAN));
        // disable OK and enable Drives
        EnableWindow(GetDlgItem(HWindow, IDB_ACDRIVES), TRUE);
        EnableWindow(GetDlgItem(HWindow, IDOK), FALSE);
        // the OK button will not be the default push button now
        PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
        // the Start button becomes the default push button
        PostMessage(HWindow, DM_SETDEFID, IDB_ACSTOP, NULL);
        // load parameters for window layout
        GetLayoutParams();
        // layout the window
        LayoutControls();
        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_ACSTOP:
        {
            // if we are searching, it’s the STOP button; otherwise, Rescan
            if (SearchRunning)
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_WANTTOSTOP), LoadStr(IDS_QUESTION),
                                  MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
                    return TRUE;
                // stop searching on disk
                SetEvent(StopSearch);
                return TRUE;
            }
            else
            {
                // Restart button
                // the Stop/Restart button becomes the Stop button again
                SetDlgItemText(HWindow, IDB_ACSTOP, LoadStr(IDS_ACBUTTON_STOP));
                // disable OK and Drives
                EnableWindow(GetDlgItem(HWindow, IDB_ACDRIVES), FALSE);
                EnableWindow(GetDlgItem(HWindow, IDOK), FALSE);
                // the OK button will not be the default push button now
                PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
                // the STOP button becomes the default push button
                HWND focus = GetFocus();
                PostMessage(HWindow, DM_SETDEFID, IDB_ACSTOP, NULL);
                if (focus != ListView->HWindow && focus != GetDlgItem(HWindow, IDC_ACMODCUSTOM))
                {
                    // if the focus was on one of the buttons, we must return it there
                    PostMessage(GetDlgItem(HWindow, IDB_ACSTOP), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
                    PostMessage(focus, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
                }
                // create the event again to signal interruption of searching
                StopSearch = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
                // and start the searching thread
                SearchRunning = TRUE;
                DWORD threadID;
                if (StopSearch != NULL)
                    HSearchThread = HANDLES_Q(CreateThread(NULL, 0, PackACDiskSearchThread, this, 0, &threadID));
                else
                {
                    TRACE_E("Unable to create stop searching event, so also cannot create searching thread.");
                    HSearchThread = NULL;
                }
                if (HSearchThread == NULL)
                {
                    if (StopSearch != NULL)
                        TRACE_E("Unable to create searching thread.");
                    SearchRunning = FALSE;
                    PostMessage(HWindow, WM_USER_ACFINDFINISHED, 0, 0);
                }
            }
            break;
        }
        case IDCANCEL:
        {
            // if the second thread is running, ask the user...
            if (SearchRunning)
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_CANCELOPERATION), LoadStr(IDS_QUESTION),
                                  MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
                    return TRUE;
                // stop searching on disk
                SetEvent(StopSearch);
                WillExit = TRUE;
                return TRUE;
            }
            break;
        }
        case IDB_ACDRIVES:
        {
            CPackACDrives(HLanguage, IDD_ACDRIVES, IDD_ACDRIVES, HWindow, DrivesList).Execute();
            break;
        }
        }
        break;
    }
    case WM_NOTIFY:
    {
        if (wParam == IDC_ACLIST)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case LVN_GETDISPINFO:
            {
                // show the item and its state (we hold the data, not the listview)
                LV_DISPINFO* info = (LV_DISPINFO*)lParam;
                int index;
                CPackACPacker* packer = ListView->GetPacker(info->item.iItem, &index);
                // if text was requested, provide it
                if (info->item.mask & LVIF_TEXT)
                    info->item.pszText = (char*)packer->GetText(index, info->item.iSubItem);
                // if the checkbox icon was requested, provide it too
                if ((info->item.mask & LVIF_STATE) &&
                    (info->item.stateMask & LVIS_STATEIMAGEMASK))
                {
                    info->item.state &= ~LVIS_STATEIMAGEMASK;
                    info->item.state |= packer->GetSelectState(index) << 12;
                }
                break;
            }
            }
        }
        break;
    }
    case WM_SIZE:
    {
        LayoutControls();
        break;
    }
    case WM_GETMINMAXINFO:
    {
        // minimum window size
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinDlgW;
        lpmmi->ptMinTrackSize.y = MinDlgH;
        break;
    }
    case WM_USER_ACADDFILE:
    {
        // the searching thread added a file, repaint the affected area
        UpdateListViewItems((int)wParam);
        return TRUE;
    }
    case WM_USER_ACERROR:
    {
        // we have a problem, so let the user know...
        // if we are an icon, jump up, restore the window
        if (IsIconic(HWindow))
            ShowWindow(HWindow, SW_RESTORE);
        // and display the error
        MessageBox(HWindow, (char*)wParam, LoadStr(IDS_ERRORFINDINGFILE), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }
    case WM_USER_ACSEARCHING:
    {
        // the searching thread sent us a new path it is working on, so we trim the trailing slash
        if (((char*)wParam)[lstrlen((char*)wParam) - 1] == '\\')
            ((char*)wParam)[lstrlen((char*)wParam) - 1] = '\0';
        // display it
        SetDlgItemText(HWindow, IDC_ACSTATUS, (char*)wParam);
        // and free the memory
        HANDLES(GlobalFree((HGLOBAL)wParam));
        return TRUE;
    }
    case WM_USER_ACFINDFINISHED:
    {
        // wait for the searching thread to really finish and clean up
        if (HSearchThread != NULL)
        {
            WaitForSingleObject(HSearchThread, INFINITE);
            HANDLES(CloseHandle(HSearchThread));
            HSearchThread = NULL;
        }
        // release the signaling event
        if (StopSearch != NULL)
        {
            HANDLES(CloseHandle(StopSearch));
            StopSearch = NULL;
        }
        // if the thread ended because we are closing the window, continue with closing
        if (WillExit)
            PostMessage(HWindow, WM_COMMAND, MAKELONG(IDCANCEL, 0), 0);
        else
        {
            // restore everything back to normal
            SetDlgItemText(HWindow, IDB_ACSTOP, LoadStr(IDS_ACBUTTON_RESCAN));
            SetDlgItemText(HWindow, IDC_ACSTATUS, LoadStr(IDS_ACSTATUSDONE));
            EnableWindow(GetDlgItem(HWindow, IDB_ACDRIVES), TRUE);
            EnableWindow(GetDlgItem(HWindow, IDOK), TRUE);
            // fix the default push button
            HWND focus = GetFocus();
            PostMessage(HWindow, DM_SETDEFID, IDOK, NULL);
            if (focus != ListView->HWindow && focus != GetDlgItem(HWindow, IDC_ACMODCUSTOM))
            {
                // if the focus was on one of the buttons, restore it there
                PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
                PostMessage(focus, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
            }
        }
        return TRUE;
    }

    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(GetDlgItem(HWindow, IDC_ACLIST), GetSysColor(COLOR_WINDOW));
        break;
    }
    }
    // default message processing
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

// store initial dialog dimensions for resizing
void CPackACDialog::GetLayoutParams()
{
    CALL_STACK_MESSAGE1("CPackACDialog::GetLayoutParams()");
    // minimum dialog dimensions
    RECT wr;
    GetWindowRect(HWindow, &wr);
    MinDlgW = wr.right - wr.left;
    MinDlgH = wr.bottom - wr.top;

    // client area of the window
    RECT cr;
    GetClientRect(HWindow, &cr);

    // helper dimensions for converting non-client and client coordinates
    int windowMargin = ((wr.right - wr.left) - (cr.right)) / 2;
    int captionH = wr.bottom - wr.top - cr.bottom - windowMargin;

    // space on the left and right between the dialog frame and controls
    RECT br;
    GetWindowRect(GetDlgItem(HWindow, IDC_ACLIST), &br);
    HMargin = br.left - wr.left - windowMargin;

    // space at the bottom between buttons and the status bar
    VMargin = HMargin;

    // button dimensions
    GetWindowRect(GetDlgItem(HWindow, IDB_ACDRIVES), &br);
    ButtonW1 = br.right - br.left;
    GetWindowRect(GetDlgItem(HWindow, IDB_ACSTOP), &br);
    ButtonW2 = br.right - br.left;
    GetWindowRect(GetDlgItem(HWindow, IDOK), &br);
    ButtonW3 = br.right - br.left;
    ButtonH = br.bottom - br.top;
    GetWindowRect(GetDlgItem(HWindow, IDCANCEL), &br);
    ButtonW4 = br.right - br.left;
    ButtonMargin = br.right;
    GetWindowRect(GetDlgItem(HWindow, IDHELP), &br);
    ButtonW5 = br.right - br.left;
    ButtonMargin = br.left - ButtonMargin;

    // status bar height
    GetWindowRect(HStatusBar, &br);
    StatusHeight = br.bottom - br.top;

    // checkbox height
    CheckH = br.bottom - br.top;

    // position of the results list
    GetWindowRect(GetDlgItem(HWindow, IDC_ACLIST), &br);
    ListY = br.top - wr.top - captionH;
}

// resize the dialog window
void CPackACDialog::LayoutControls()
{
    CALL_STACK_MESSAGE1("CPackACDialog::LayoutControls()");
    RECT clientRect;

    // take the available size
    GetClientRect(HWindow, &clientRect);
    clientRect.bottom -= StatusHeight;

    // then perform all changes in one block
    HDWP hdwp = HANDLES(BeginDeferWindowPos(8));
    if (hdwp != NULL)
    {
        // position the status bar
        hdwp = HANDLES(DeferWindowPos(hdwp, HStatusBar, NULL,
                                      0, clientRect.bottom, clientRect.right, StatusHeight,
                                      SWP_NOZORDER));

        // position the Help button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDHELP), NULL,
                                      clientRect.right - ButtonW5 - HMargin,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // position the Cancel button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDCANCEL), NULL,
                                      clientRect.right - (ButtonW4 + ButtonW5) - HMargin - ButtonMargin,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // position the OK button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDOK), NULL,
                                      clientRect.right - (ButtonW3 + ButtonW4 + ButtonW5) - HMargin - ButtonMargin * 2,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // position the Stop/Rescan button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_ACSTOP), NULL,
                                      clientRect.right - (ButtonW2 + ButtonW3 + ButtonW4 + ButtonW5) - HMargin - ButtonMargin * 3,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // position the Drives button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_ACDRIVES), NULL,
                                      clientRect.right - (ButtonW1 + ButtonW2 + ButtonW3 + ButtonW4 + ButtonW5) - HMargin - ButtonMargin * 4,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // position the Custom archivers check box
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_ACMODCUSTOM), NULL,
                                      HMargin, clientRect.bottom - VMargin - ButtonH / 2 - CheckH / 2,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // position and stretch the list view
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_ACLIST), NULL,
                                      HMargin, ListY, clientRect.right - HMargin * 2,
                                      clientRect.bottom - ListY - ButtonH - VMargin * 2,
                                      SWP_NOZORDER));
        // block finished
        HANDLES(EndDeferWindowPos(hdwp));
    }
    // and adjust the column width in the list view
    ListView->SetColumnWidth();
}

BOOL CPackACDialog::MyGetBinaryType(LPCSTR filename, LPDWORD lpBinaryType)
{
    CALL_STACK_MESSAGE2("CPackACDialog::MyGetBinaryType(%s, )", filename);

    BOOL ret = FALSE;
    // open the file for reading
    HANDLE hfile = HANDLES_Q(CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL));
    if (hfile != INVALID_HANDLE_VALUE)
    {
        IMAGE_DOS_HEADER mz_header;
        DWORD len;
        // Seek to the start of the file and read the DOS header information.
        if (SetFilePointer(hfile, 0, NULL, FILE_BEGIN) != -1 &&
            ReadFile(hfile, &mz_header, sizeof(mz_header), &len, NULL) &&
            len == sizeof(mz_header))
        {
            // Now that we have the header check the e_magic field to see if this is a dos image.
            if (mz_header.e_magic == IMAGE_DOS_SIGNATURE)
            {
                char magic[4];
                BOOL lfanewValid = FALSE;
                // We do have a DOS image so we will now try to seek into the file by the amount indicated by the field
                // "Offset to extended header" and read in the "magic" field information at that location.
                // This will tell us if there is more header information to read or not.
                //
                // But before we do we will make sure that header structure encompasses the "Offset to extended header" field.
                if ((mz_header.e_cparhdr << 4) >= sizeof(IMAGE_DOS_HEADER))
                    if ((mz_header.e_crlc == 0) ||
                        (mz_header.e_lfarlc >= sizeof(IMAGE_DOS_HEADER)))
                        if (mz_header.e_lfanew >= sizeof(IMAGE_DOS_HEADER) &&
                            SetFilePointer(hfile, mz_header.e_lfanew, NULL, FILE_BEGIN) != -1 &&
                            ReadFile(hfile, magic, sizeof(magic), &len, NULL) &&
                            len == sizeof(magic))
                            lfanewValid = TRUE;

                if (!lfanewValid)
                {
                    // If we cannot read this "extended header" we will assume that we have a simple DOS executable.
                    *lpBinaryType = SCS_DOS_BINARY;
                    ret = TRUE;
                }
                else
                {
                    // Reading the magic field succeeded so we will try to determine what type it is.
                    if (*(DWORD*)magic == IMAGE_NT_SIGNATURE)
                    {
                        // This is an NT signature.
                        *lpBinaryType = SCS_32BIT_BINARY;
                        ret = TRUE;
                    }
                    else if (*(WORD*)magic == IMAGE_OS2_SIGNATURE)
                    {
                        // The IMAGE_OS2_SIGNATURE indicates that the "extended header is a Windows executable (NE)
                        // header."  This can mean either a 16-bit OS/2 or a 16-bit Windows or even a DOS program
                        // (running under a DOS extender).  To decide which, we'll have to read the NE header.
                        IMAGE_OS2_HEADER ne;
                        if (SetFilePointer(hfile, mz_header.e_lfanew, NULL, FILE_BEGIN) != -1 &&
                            ReadFile(hfile, &ne, sizeof(ne), &len, NULL) &&
                            len == sizeof(ne))
                        {
                            switch (ne.ne_exetyp)
                            {
                            case 2:
                                // Win 16 executable
                                *lpBinaryType = SCS_WOW_BINARY;
                                ret = TRUE;
                                break;
                            case 5:
                                // DOS executable
                                *lpBinaryType = SCS_DOS_BINARY;
                                ret = TRUE;
                                break;
                            default:
                            {
                                // Check whether a file is an OS/2 or a very old Windows executable by testing on import of KERNEL.
                                *lpBinaryType = SCS_OS216_BINARY;
                                LPWORD modtab = NULL;
                                LPSTR nametab = NULL;

                                // read modref table
                                if ((SetFilePointer(hfile, mz_header.e_lfanew + ne.ne_modtab, NULL, FILE_BEGIN) == -1) ||
                                    ((modtab = (LPWORD)HANDLES(GlobalAlloc(GMEM_FIXED, ne.ne_cmod * sizeof(WORD)))) == NULL) ||
                                    (!(ReadFile(hfile, modtab, ne.ne_cmod * sizeof(WORD), &len, NULL))) ||
                                    (len != ne.ne_cmod * sizeof(WORD)))
                                    ret = FALSE;
                                else
                                {
                                    // read imported names table
                                    if ((SetFilePointer(hfile, mz_header.e_lfanew + ne.ne_imptab, NULL, FILE_BEGIN) == -1) ||
                                        ((nametab = (LPSTR)HANDLES(GlobalAlloc(GMEM_FIXED, ne.ne_enttab - ne.ne_imptab))) == NULL) ||
                                        (!(ReadFile(hfile, nametab, ne.ne_enttab - ne.ne_imptab, &len, NULL))) ||
                                        (len != (WORD)(ne.ne_enttab - ne.ne_imptab)))
                                        ret = FALSE;
                                    else
                                    {
                                        ret = TRUE;
                                        int i;
                                        for (i = 0; i < ne.ne_cmod; i++)
                                        {
                                            LPSTR module = &nametab[modtab[i]];
                                            if (!(strncmp(&module[1], "KERNEL", module[0])))
                                            {
                                                // very old Windows file
                                                *lpBinaryType = SCS_WOW_BINARY;
                                                break;
                                            }
                                        }
                                    }
                                }
                                if (modtab != NULL)
                                    HANDLES(GlobalFree(modtab));
                                if (nametab != NULL)
                                    HANDLES(GlobalFree(nametab));
                            }
                            break;
                            }
                        }
                        else
                            // Couldn't read header, so abort.
                            ret = FALSE;
                    }
                    else
                    {
                        // Unknown extended header, but this file is nonetheless DOS-executable.
                        *lpBinaryType = SCS_DOS_BINARY;
                        ret = TRUE;
                    }
                }
            }
            else
            {
                // If we get here, we don't even have a correct MZ header. Try to check the file extension for known types ...
                const char* ptr;
                ptr = strrchr(filename, '.');
                if (ptr &&
                    !strchr(ptr, '\\') &&
                    !strchr(ptr, '/'))
                {
                    if (!stricmp(ptr, ".COM"))
                    {
                        *lpBinaryType = SCS_DOS_BINARY;
                        ret = TRUE;
                    }
                    else if (!stricmp(ptr, ".PIF"))
                    {
                        *lpBinaryType = SCS_PIF_BINARY;
                        ret = TRUE;
                    }
                    else
                        ret = FALSE;
                }
            }
        }
        // Close the file.
        HANDLES(CloseHandle(hfile));
    }
    return ret;
}

// the actual (and recursive :-) ) function for disk searching
BOOL CPackACDialog::DirectorySearch(char* path)
{
    CALL_STACK_MESSAGE2("CPackACDialog::DirectorySearch(%s)", path);

    // report to the boss what we are working on
    DWORD pathLen = (DWORD)strlen(path);
    char* workName = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, pathLen + 1));
    if (workName == NULL)
    {
        SendMessage(HWindow, WM_USER_ACERROR, (WPARAM)LoadStr(IDS_PACKERR_NOMEM), NULL);
        TRACE_E(LOW_MEMORY);
        return TRUE;
    }
    strcpy(workName, path);
    // send the name; the receiver will free the memory
    if (!PostMessage(HWindow, WM_USER_ACSEARCHING, (WPARAM)workName, 0))
        // if it failed, never mind...
        HANDLES(GlobalFree((HGLOBAL)workName));

    // allocate the buffer for the search mask
    char* fileName = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, pathLen + 3 + 1));
    if (fileName == NULL)
    {
        SendMessage(HWindow, WM_USER_ACERROR, (WPARAM)LoadStr(IDS_PACKERR_NOMEM), NULL);
        TRACE_E(LOW_MEMORY);
        return TRUE;
    }
    // prepare the mask for searching
    strcpy(fileName, path);
    strcat(fileName, "*");

    // set up some variables
    BOOL mustStop = FALSE;
    WIN32_FIND_DATA findData;
    // try to find the first file
    HANDLE fileFind = HANDLES_Q(FindFirstFile(fileName, &findData));
    if (fileFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            unsigned int nameLen = (unsigned int)strlen(findData.cFileName);
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                // it is a file; now check if it is an exe
                if (nameLen > 4 && pathLen + nameLen < MAX_PATH &&
                    (findData.cFileName[nameLen - 1] == 'e' || findData.cFileName[nameLen - 1] == 'E') &&
                    (findData.cFileName[nameLen - 2] == 'x' || findData.cFileName[nameLen - 2] == 'X') &&
                    (findData.cFileName[nameLen - 3] == 'e' || findData.cFileName[nameLen - 3] == 'E') &&
                    findData.cFileName[nameLen - 4] == '.')
                {
                    // determine the program type
                    char fullName[MAX_PATH];
                    DWORD type;
                    strcpy(fullName, path);
                    strcat(fullName, findData.cFileName);

                    if (!MyGetBinaryType(fullName, &type))
                    {
                        TRACE_I("Invalid executable or error getting type: " << fullName);
                        continue;
                    }
                    // and see whether we are interested in it
                    mustStop |= ListView->ConsiderItem(path, findData.cFileName,
                                                       findData.ftLastWriteTime,
                                                       CQuadWord(findData.nFileSizeLow,
                                                                 findData.nFileSizeHigh),
                                                       type == SCS_32BIT_BINARY ? EXE_32BIT : EXE_16BIT);
                }
            }
            else
            {
                // we have a directory - exclude '.' and '..'
                if (findData.cFileName[0] != 0 &&
                    (findData.cFileName[0] != '.' ||
                     (findData.cFileName[1] != '\0' &&
                      (findData.cFileName[1] != '.' || findData.cFileName[2] != '\0'))) &&
                    pathLen + 1 + nameLen < MAX_PATH)
                {
                    // create the directory name to search
                    char* newPath = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, pathLen + 1 + nameLen + 1));
                    if (newPath == NULL)
                    {
                        SendMessage(HWindow, WM_USER_ACERROR, (WPARAM)LoadStr(IDS_PACKERR_NOMEM), NULL);
                        mustStop = TRUE;
                        TRACE_E(LOW_MEMORY);
                    }
                    else
                    {
                        strcpy(newPath, path);
                        strcat(newPath, findData.cFileName);
                        strcat(newPath, "\\");
                        // and search it recursively
                        DirectorySearch(newPath);
                        HANDLES(GlobalFree((HGLOBAL)newPath));
                    }
                }
            }
            // check whether an interruption has been signaled
            DWORD ret = WaitForSingleObject(StopSearch, 0);
            if (ret != WAIT_TIMEOUT)
                mustStop = TRUE;
        } while (FindNextFile(fileFind, &findData) && !mustStop);
        HANDLES(FindClose(fileFind));
    }
    HANDLES(GlobalFree((HGLOBAL)fileName));
    // and done
    return mustStop;
}

// start recursive search for all local fixed drives
DWORD
CPackACDialog::DiskSearch()
{
    CALL_STACK_MESSAGE1("CPackACDialog::DiskSearch()");
    // initialize tracing for the new thread
    SetThreadNameInVCAndTrace("AutoConfig");
    TRACE_I("Begin");

    // sanity checking
    if (DrivesList == NULL || *DrivesList == NULL || **DrivesList == '\0')
        TRACE_I("The list of drives is empty...");
    else
    {
        char* drive = *DrivesList;
        // go through all drives we have
        BOOL mustStop = FALSE;
        while (*drive != '\0' && !mustStop)
        {
            // and off we go...
            TRACE_I("Searching drive " << drive);
            mustStop = DirectorySearch(drive);
            // move to the next one in the list
            while (*drive != '\0')
                drive++;
            // skip the terminating null
            drive++;
        }
    }
    // notify that searching has finished
    PostMessage(HWindow, WM_USER_ACFINDFINISHED, 0, 0);
    // we are no longer searching
    SearchRunning = FALSE;
    TRACE_I("End");
    return 0;
}

// wrapper for the search function with exception handling
unsigned int
CPackACDialog::PackACDiskSearchThreadEH(void* instance)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        // call the actual search
        return ((CPackACDialog*)instance)->DiskSearch();
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("PackACDiskSearchThreadEH: calling ExitProcess(1).");
        TerminateProcess(GetCurrentProcess(), 1); // harder exit (this call still performs some operations)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

// wrapper for the wrapper for searching - this time a static function called from CreateThread
DWORD WINAPI
CPackACDialog::PackACDiskSearchThread(void* param)
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return PackACDiskSearchThreadEH(param);
}

void CPackACDialog::AddToExtensions(int foundIndex, int packerIndex, CPackACPacker* foundPacker)
{
    CALL_STACK_MESSAGE3("CPackACDialog::AddToExtensions(%d, %d, )", foundIndex, packerIndex);

    // add the extension and possibly a custom packer if it does not exist
    // the packer is added if there is no file mask corresponding to the found packer
    // if one exists and refers to something other than a plugin or 32-bit version, redirect to the found one
    char buffer[10];
    const char* ptr = PackACExtensions[packerIndex].Ext;
    int found;
    do
    {
        // look for who is using us
        buffer[0] = '.';
        int j = 1;
        while (*ptr != ';' && *ptr != '\0')
        {
            if (*ptr == '#')
                buffer[j++] = '1';
            else
                buffer[j++] = *ptr;
            ptr++;
        }
        if (*ptr == ';')
            ptr++;
        buffer[j] = '\0';
        // we built the "archive name", now check whether we have it in extensions...
        found = PackerFormatConfig.PackIsArchive(buffer);
        if (found != 0)
        {
            int pos;
            CPackACPacker* p = NULL;
            // if we are working with a packer adjust the record for the packer
            if (foundPacker->GetPackerType() == Packer_Packer || foundPacker->GetPackerType() == Packer_Standalone)
            {
                // if we used a packer, find out whether we found it
                if (PackerFormatConfig.GetUsePacker(found - 1))
                {
                    pos = PackerFormatConfig.GetPackerIndex(found - 1);
                    // determine whether we found it
                    int k;
                    for (k = 0; k < ListView->GetPackersCount(); k++)
                    {
                        p = ListView->GetPacker(k);
                        if (p->GetArchiverIndex() == pos && p->GetPackerType() != Packer_Unpacker)
                            break;
                    }
                }
                // replace the packer if:
                // 1) we did not use a packer at all
                // 2) or we used an external packer different from the one being checked and
                // 3) the original packer was not found or we have a 32-bit one (better)
                // if we did not use the packer before, enable it
                if (!PackerFormatConfig.GetUsePacker(found - 1) ||
                    (pos >= 0 && pos != packerIndex &&
                     (p == NULL || p->GetSelectedFullName() == NULL || foundPacker->GetExeType() == EXE_32BIT)))
                {
                    TRACE_I("Setting packer for extension " << PackACExtensions[packerIndex].Ext << " to the new one.");
                    PackerFormatConfig.SetPackerIndex(found - 1, packerIndex);
                    PackerFormatConfig.SetUsePacker(found - 1, TRUE);
                    // rebuild the table
                    PackerFormatConfig.BuildArray();
                }
            }
            // if we are working with an unpacker adjust the record for it
            if (foundPacker->GetPackerType() == Packer_Unpacker || foundPacker->GetPackerType() == Packer_Standalone)
            {
                // if found, check the settings
                pos = PackerFormatConfig.GetUnpackerIndex(found - 1);
                // determine whether the current unpacker was found
                int k;
                for (k = 0; k < ListView->GetPackersCount(); k++)
                {
                    p = ListView->GetPacker(k);
                    if (p->GetArchiverIndex() == pos && p->GetPackerType() != Packer_Packer)
                        break;
                }
                // if the existing packer is not a plugin, was not found, or is different and we have a 32-bit one
                if (pos >= 0 && pos != packerIndex &&
                    (p == NULL || p->GetSelectedFullName() == NULL || foundPacker->GetExeType() == EXE_32BIT))
                {
                    TRACE_I("Changing unpacker for extension " << PackACExtensions[packerIndex].Ext << " to the new one.");
                    PackerFormatConfig.SetUnpackerIndex(found - 1, packerIndex);
                    // rebuild the table
                    PackerFormatConfig.BuildArray();
                }
            }
        }
    } while (*ptr != '\0');
    // we searched all extensions and if none were found we must add one
    if (found == 0)
    {
        // if we only have a packer, we must also have an unpacker (we would have found the plugin already)
        if (foundPacker->GetPackerType() != Packer_Packer || ListView->GetPacker(foundIndex + 1)->GetSelectedFullName() != NULL)
        {
            TRACE_I("Adding extensions " << PackACExtensions[packerIndex].Ext);
            int idx = PackerFormatConfig.AddFormat();
            if (idx >= 0)
            {
                PackerFormatConfig.SetFormat(idx, PackACExtensions[packerIndex].Ext,
                                             foundPacker->GetPackerType() != Packer_Unpacker, packerIndex, packerIndex, FALSE);
                PackerFormatConfig.BuildArray();
            }
        }
        else
            TRACE_I("Skipping packer for which I have no unpacker: " << foundPacker->GetSelectedFullName());
    }
}

void CPackACDialog::RemoveFromExtensions(int foundIndex, int packerIndex, CPackACPacker* foundPacker)
{
    CALL_STACK_MESSAGE3("CPackACDialog::RemoveFromExtensions(%d, %d, )", foundIndex, packerIndex);

    // remove the association from extensions
    // the association is removed if the viewer was not found or was found but not selected (fullName == NULL)
    // the packer is switched to --not supported-- if the packer was not found or was found but not selected (fullName == NULL)

    // search the extensions
    int i;
    for (i = PackerFormatConfig.GetFormatsCount() - 1; i >= 0; i--)
    {
        // if the association uses us as viewer, try to switch to another one, otherwise remove it
        if (packerIndex == PackerFormatConfig.GetUnpackerIndex(i))
        {
            // try to find an alternative
            CPackACPacker* p = NULL;
            if (PackACExtensions[packerIndex].nextIndex >= 0)
                p = ListView->GetPacker(PackACExtensions[packerIndex].nextIndex);
            // if it is only a packer (zip), use the unpacker
            if (p != NULL && p->GetPackerType() == Packer_Packer)
                p = ListView->GetPacker(PackACExtensions[packerIndex].nextIndex + 1);
            if (p == NULL || p->GetSelectedFullName() == NULL)
            {
                // if there is no alternative, remove it
                TRACE_I("Removing extensions " << PackerFormatConfig.GetExt(i));
                PackerFormatConfig.DeleteFormat(i);
                PackerFormatConfig.BuildArray();
            }
            else
            {
                // we found an alternative, switch to it
                TRACE_I("Changing viewer for extensions " << PackerFormatConfig.GetExt(i) << " to another.");
                PackerFormatConfig.SetUnpackerIndex(i, p->GetArchiverIndex());
                PackerFormatConfig.BuildArray();
            }
        }
        else
        {
            // if the association uses us only as editor, try to redirect, otherwise set to --not supported--
            if (PackerFormatConfig.GetUsePacker(i) && PackerFormatConfig.GetPackerIndex(i) == packerIndex)
            {
                CPackACPacker* p = NULL;
                if (PackACExtensions[packerIndex].nextIndex >= 0)
                    p = ListView->GetPacker(PackACExtensions[packerIndex].nextIndex);
                if (p == NULL || p->GetSelectedFullName() == NULL)
                {
                    // the alternative does not exist
                    TRACE_I("Setting packer to --not supported-- for extension " << PackerFormatConfig.GetExt(i));
                    // another unpacker is present, set the packer to --not supported--
                    PackerFormatConfig.SetUsePacker(i, FALSE);
                    PackerFormatConfig.BuildArray();
                }
                else
                {
                    // we have an alternative that we found, switch to it
                    TRACE_I("Changing editor for extensions " << PackerFormatConfig.GetExt(i) << " to another.");
                    PackerFormatConfig.SetPackerIndex(i, p->GetArchiverIndex());
                    PackerFormatConfig.BuildArray();
                }
            }
        }
    }
}

void CPackACDialog::AddToCustom(int foundIndex, int packerIndex, CPackACPacker* foundPacker)
{
    CALL_STACK_MESSAGE3("CPackACDialog::AddToCustom(%d, %d, )", foundIndex, packerIndex);

    // add custom packers for the newly found packer
    char variable[50];
    int i;
    BOOL found1 = FALSE, found2 = FALSE;
    sprintf(variable, "$(%s)", ArchiverConfig->GetPackerVariable(packerIndex));
    // search custom packers to see if it is already present
    for (i = 0; i < PackerConfig.GetPackersCount(); i++)
    {
        // consider only external ones
        if (PackerConfig.GetPackerType(i) >= 0)
        {
            const char* cmd = PackerConfig.GetPackerCmdExecCopy(i);
            const char* args = PackerConfig.GetPackerCmdArgsCopy(i);
            if (!strcmp(cmd, variable) && !strcmp(args, CustomPackers[packerIndex].CopyArgs[0]))
                found1 = TRUE;
            else if (CustomPackers[packerIndex].CopyArgs[1] != NULL && !strcmp(cmd, variable) && !strcmp(args, CustomPackers[packerIndex].CopyArgs[1]))
                found2 = TRUE;
        }
    }
    if (!found1 && foundPacker->GetPackerType() != Packer_Unpacker)
    {
        TRACE_I("Adding custom packer " << LoadStr(CustomPackers[packerIndex].Title[0]));
        int idx = PackerConfig.AddPacker();
        PackerConfig.SetPacker(idx, 0, LoadStr(CustomPackers[packerIndex].Title[0]), CustomPackers[packerIndex].Ext,
                               FALSE, CustomPackers[packerIndex].SupLN, TRUE, variable, CustomPackers[packerIndex].CopyArgs[0],
                               variable, CustomPackers[packerIndex].MoveArgs[0], CustomPackers[packerIndex].Ansi);
    }
    if (CustomPackers[packerIndex].CopyArgs[1] != NULL && !found2 && foundPacker->GetPackerType() != Packer_Unpacker)
    {
        TRACE_I("Adding custom packer " << LoadStr(CustomPackers[packerIndex].Title[1]));
        int idx = PackerConfig.AddPacker();
        PackerConfig.SetPacker(idx, 0, LoadStr(CustomPackers[packerIndex].Title[1]), CustomPackers[packerIndex].Ext,
                               FALSE, CustomPackers[packerIndex].SupLN, TRUE, variable,
                               CustomPackers[packerIndex].CopyArgs[1], variable, CustomPackers[packerIndex].MoveArgs[1],
                               CustomPackers[packerIndex].Ansi);
    }

    // add custom unpackers for the newly found packer
    if (!ArchiverConfig->ArchiverExesAreSame(packerIndex))
        sprintf(variable, "$(%s)", ArchiverConfig->GetUnpackerVariable(packerIndex));
    found1 = FALSE;
    // search custom packers to see if it is already present
    for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
    {
        // consider only external ones
        if (UnpackerConfig.GetUnpackerType(i) >= 0)
        {
            const char* cmd = UnpackerConfig.GetUnpackerCmdExecExtract(i);
            const char* args = UnpackerConfig.GetUnpackerCmdArgsExtract(i);
            if (!strcmp(cmd, variable) && !strcmp(args, CustomUnpackers[packerIndex].Args))
                found1 = TRUE;
        }
    }
    if (!found1 && foundPacker->GetPackerType() != Packer_Packer)
    {
        TRACE_I("Adding custom unpacker " << LoadStr(CustomUnpackers[packerIndex].Title));
        int idx = UnpackerConfig.AddUnpacker();
        UnpackerConfig.SetUnpacker(idx, 0, LoadStr(CustomUnpackers[packerIndex].Title), CustomUnpackers[packerIndex].Ext,
                                   FALSE, CustomUnpackers[packerIndex].SupLN, variable, CustomUnpackers[packerIndex].Args,
                                   CustomUnpackers[packerIndex].Ansi);
    }
}

void CPackACDialog::RemoveFromCustom(int foundIndex, int packerIndex)
{
    CALL_STACK_MESSAGE3("CPackACDialog::RemoveFromCustom(%d, %d)", foundIndex, packerIndex);

    // a custom packer/unpacker is removed if:
    //   1) the packer wasn't found or was found but not selected (fullName == NULL)
    //   2) the invoked program is a variable corresponding to this packer

    CPackACPacker* p = NULL;
    char variable[50];
    sprintf(variable, "$(%s)", ArchiverConfig->GetPackerVariable(packerIndex));
    int i;
    // search custom packers (backwards so we can remove entries)
    for (i = PackerConfig.GetPackersCount() - 1; i >= 0; i--)
        // consider only external ones
        if (PackerConfig.GetPackerType(i) >= 0)
        {
            const char* cmd = PackerConfig.GetPackerCmdExecCopy(i);
            if (!strcmp(cmd, variable))
            {
                // this packer calls a program we did not find - remove it
                TRACE_I("Removing custom packer which uses not found packer: " << variable);
                PackerConfig.DeletePacker(i);
                // no need to check move cmd, the packer no longer exists
            }
            else if (PackerConfig.GetPackerSupMove(i))
            {
                cmd = PackerConfig.GetPackerCmdExecMove(i);
                if (!strcmp(cmd, variable))
                {
                    // calls the move cmd program that does not exist - disable it
                    TRACE_I("Disabling Move command for custom packer which uses not found packer: " << variable);
                    PackerConfig.SetPackerSupMove(i, FALSE);
                }
            }
        }
    if (!ArchiverConfig->ArchiverExesAreSame(packerIndex))
        sprintf(variable, "$(%s)", ArchiverConfig->GetUnpackerVariable(packerIndex));
    // search custom unpackers
    for (i = UnpackerConfig.GetUnpackersCount() - 1; i >= 0; i--)
        // consider only external ones
        if (UnpackerConfig.GetUnpackerType(i) >= 0)
        {
            const char* cmd = UnpackerConfig.GetUnpackerCmdExecExtract(i);
            if (!strcmp(cmd, variable))
            {
                // this unpacker calls a program we did not find - remove it
                TRACE_I("Removing custom unpacker which uses not found packer: " << variable);
                UnpackerConfig.DeleteUnpacker(i);
            }
        }
}

// function called before starting and after closing the dialog for data transfer
void CPackACDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPackACDialog::Transfer()");
    // are we starting or ending?
    if (ti.Type == ttDataToWindow)
    {
        // create a table of packers to search for
        APackACPackersTable* table = new APackACPackersTable(20, 10);
        int i;
        for (i = 0; i < ArchiverConfig->GetArchiversCount(); i++)
        {
            if (ArchiverConfig->ArchiverExesAreSame(i))
            {
                table->Add(new CPackACPacker(i, Packer_Standalone, ArchiverConfig->GetPackerVariable(i),
                                             ArchiverConfig->GetPackerExecutable(i),
                                             ArchiverConfig->GetArchiverType(i)));
            }
            else
            {
                table->Add(new CPackACPacker(i, Packer_Packer, ArchiverConfig->GetPackerVariable(i),
                                             ArchiverConfig->GetPackerExecutable(i),
                                             ArchiverConfig->GetArchiverType(i)));
                table->Add(new CPackACPacker(i, Packer_Unpacker, ArchiverConfig->GetUnpackerVariable(i),
                                             ArchiverConfig->GetUnpackerExecutable(i),
                                             ArchiverConfig->GetArchiverType(i)));
            }
        }
        // put it into the list view
        ListView->Initialize(table);
        // checkbox checked by default
        int value = 1;
        ti.CheckBox(IDC_ACMODCUSTOM, value);
        // not searching yet
        SearchRunning = FALSE;
    }
    else
    {
        int doCustom;
        // read the value of the checkbox
        ti.CheckBox(IDC_ACMODCUSTOM, doCustom);
        int i;
        // go through all packers - configure paths and remove associations for those that were not found
        for (i = 0; i < ListView->GetPackersCount(); i++)
        {
            // ask for the packer
            CPackACPacker* packer = ListView->GetPacker(i);
            int index = packer->GetArchiverIndex();
            const char* fullName = packer->GetSelectedFullName();
            // if we didn't find it leave the original value
            if (fullName != NULL)
            {
                // store the path in the configuration
                if (packer->GetPackerType() == Packer_Unpacker)
                    ArchiverConfig->SetUnpackerExeFile(index, fullName);
                else
                    ArchiverConfig->SetPackerExeFile(index, fullName);
            }
            else
            {
                RemoveFromExtensions(i, index, packer);
                if (doCustom)
                    RemoveFromCustom(i, index);
            }
        }
        // go through all packers again, this time only adding new associations
        for (i = 0; i < ListView->GetPackersCount(); i++)
        {
            // ask for the packer
            CPackACPacker* packer = ListView->GetPacker(i);
            int index = packer->GetArchiverIndex();
            const char* fullName = packer->GetSelectedFullName();
            // if we didn't find it, leave the original value
            if (fullName != NULL)
            {
                AddToExtensions(i, index, packer);
                if (doCustom)
                    AddToCustom(i, index, packer);
            }
        }
    }
}

// handle a request to add an item to the list view
void CPackACDialog::UpdateListViewItems(int index)
{
    CALL_STACK_MESSAGE2("CPackACDialog::UpdateListViewItems(%d)", index);
    if (ListView != NULL)
    {
        LV_ITEM item;
        item.mask = 0;
        item.iItem = index;
        item.iSubItem = 0;
        ListView_InsertItem(ListView->HWindow, &item);
    }
}

//****************************************************************************
//
// CPackACPacker
//

// return the number of packers in the array
int CPackACPacker::GetCount()
{
    SLOW_CALL_STACK_MESSAGE1("CPackACPacker::GetCount()");
    int count;
    HANDLES(EnterCriticalSection(&FoundDataCriticalSection));
    count = Found.Count;
    HANDLES(LeaveCriticalSection(&FoundDataCriticalSection));
    return count;
}

// toggle the file selection (invert the checkbox)
void CPackACPacker::InvertSelect(int index)
{
    CALL_STACK_MESSAGE2("CPackACPacker::InvertSelect(%d)", index);
    if (index >= 0)
    {
        HANDLES(EnterCriticalSection(&FoundDataCriticalSection));
        Found.InvertSelect(index);
        HANDLES(LeaveCriticalSection(&FoundDataCriticalSection));
    }
}

// return the text that belongs in the given column
const char*
CPackACPacker::GetText(int index, int column)
{
    CALL_STACK_MESSAGE3("CPackACPacker::GetText(%d, %d)", index, column);
    const char* ret;
    if (index >= 0)
    {
        HANDLES(EnterCriticalSection(&FoundDataCriticalSection));
        ret = Found[index]->GetText(column);
        HANDLES(LeaveCriticalSection(&FoundDataCriticalSection));
    }
    else
    {
        if (column == 0)
            ret = Title;
        else
            ret = "";
    }
    return ret;
}

// return whether the item is selected or not (or if it is a header)
int CPackACPacker::GetSelectState(int index)
{
    CALL_STACK_MESSAGE2("CPackACPacker::GetSelectState(%d)", index);
    int ret;
    if (index >= 0)
    {
        HANDLES(EnterCriticalSection(&FoundDataCriticalSection));
        BOOL sel = Found[index]->IsSelected();
        HANDLES(LeaveCriticalSection(&FoundDataCriticalSection));
        if (sel)
            // selected
            ret = 2;
        else
            // not selected
            ret = 1;
    }
    else
        // header
        ret = 0;
    return ret;
}

// check whether we want the given program and if so, add it
int CPackACPacker::CheckAndInsert(const char* path, const char* fileName, FILETIME lastWriteTime,
                                  const CQuadWord& size, EPackExeType exeType)
{
    CALL_STACK_MESSAGE3("CPackACPacker::CheckAndInsert(%s, %s, , , )", path, fileName);
    const char* ref = Name;
    const char* act = fileName;
    // does the name match?
    while (*ref != '\0' && *act != '\0' && tolower(*ref) == tolower(*act))
    {
        ref++;
        act++;
    }
    // the extension has been checked already; now verify if we are at the end of the string
    // and whether other requirements are met (currently only the type, we will see in the future...)
    if (*ref == '\0' && act == &fileName[lstrlen(fileName) - 4] &&
        exeType == Type)
    {
        char* fullName = (char*)malloc(strlen(path) + strlen(fileName) + 1);
        if (fullName == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        strcpy(fullName, path);
        strcat(fullName, fileName);
        // we found a new item, check whether it is new for us
        int i;
        for (i = 0; i < Found.Count; i++)
        {
            char* n2 = Found.At(i)->FullName;
            // if we already have it, return
            if (!strcmp(fullName, n2))
            {
                free(fullName);
                return 0;
            }
        }
        // and add it
        CPackACFound* newItem = new CPackACFound;
        if (newItem != NULL)
        {
            // initialize it
            BOOL good = newItem->Set(fullName, size, lastWriteTime);
            free(fullName);
            int index;
            if (good)
            {
                HANDLES(EnterCriticalSection(&FoundDataCriticalSection));
                // and add it to the array
                index = Found.AddAndCheck(newItem);
                if (!Found.IsGood())
                {
                    Found.ResetState();
                    good = FALSE;
                }
                HANDLES(LeaveCriticalSection(&FoundDataCriticalSection));
            }
            // now check how we ended up
            if (good)
                return index + 1;
            else
            {
                delete newItem;
                newItem = NULL;
            }
        }
        free(fullName);
        if (newItem == NULL)
            return -1;
    }
    return 0;
}

//****************************************************************************
//
// CPackACArray
//

// ensures  only the newest item is selected and add it to the array
int CPackACArray::AddAndCheck(CPackACFound* member)
{
    CALL_STACK_MESSAGE1("CPackACArray::AddAndCheck()");
    // if it is the first one, it will be selected
    if (Count == 0)
        member->Selected = TRUE;
    else
    {
        int i;
        for (i = 0; i < Count; i++)
            // compare with the currently selected item
            if (At(i)->Selected && CompareFileTime(&At(i)->LastWrite, &member->LastWrite) == -1)
            {
                // if it is newer, replace the selection
                At(i)->Selected = FALSE;
                member->Selected = TRUE;
            }
    }
    // and insert into the array
    return TIndirectArray<CPackACFound>::Add(member);
}

// toggle the checkbox state of an item
void CPackACArray::InvertSelect(int index)
{
    CALL_STACK_MESSAGE2("CPackACArray::InvertSelect(%d)", index);
    // the user must be allowed to select nothing
    if (At(index)->Selected)
        At(index)->Selected = FALSE;
    else
    {
        // but only one item can be selected
        int i;
        for (i = 0; i < Count; i++)
            At(i)->Selected = (i == index);
    }
}

// return the FullName of the selected item
const char*
CPackACArray::GetSelectedFullName()
{
    CALL_STACK_MESSAGE1("CPackACArray::GetSelectedFullName()");
    // search all and return the selected one
    int i;
    for (i = 0; i < Count; i++)
        if (At(i)->Selected)
            return At(i)->FullName;
    // if none is selected, return NULL
    return NULL;
}

//****************************************************************************
//
// CPackACListView
//

// return the number of already found packers (including headings)
int CPackACListView::GetCount()
{
    CALL_STACK_MESSAGE1("CPackACListView::GetCount()");
    // if the table is missing ,there are 0 items
    if (PackersTable == NULL)
        return 0;
    // go through all table items
    int count = 0;
    int i;
    for (i = 0; i < PackersTable->Count; i++)
    {
        // ask each how many entries it has and add the heading as well
        count += PackersTable->At(i)->GetCount() + 1;
    }
    // return the result
    return count;
}

// toggle the checkbox state of the given item
void CPackACListView::InvertSelect(int index)
{
    CALL_STACK_MESSAGE2("CPackACListView::InvertSelect(%d)", index);
    unsigned int archiver, arcIndex;
    // find indices of the selected archiver
    if (PackersTable != NULL && !FindArchiver(index, &archiver, &arcIndex))
    {
        // toggle the checkbox state with control of the rest of the group
        PackersTable->At(archiver)->InvertSelect(arcIndex);
        // redraw the items that may have changed
        ListView_RedrawItems(HWindow, index - arcIndex,
                             index - arcIndex + PackersTable->At(archiver)->GetCount() - 1);
        // scroll the clicked item into the visible area
        ListView_EnsureVisible(HWindow, index, FALSE);
        // and redraw what is necessary
        UpdateWindow(HWindow);
    }
}

// return the found archiver based on the item from the listview
CPackACPacker*
CPackACListView::GetPacker(int item, int* index)
{
    CALL_STACK_MESSAGE2("CPackACListView::GetPacker(%d, )", item);
    // consistency check
    if (PackersTable == NULL)
        return NULL;
    unsigned int archiver, arcIndex;
    // try to find the archiver
    if (FindArchiver(item, &archiver, &arcIndex))
        // it is a heading
        *index = -1;
    else
        // it is an archiver
        *index = arcIndex;
    // return the desired archiver
    return PackersTable->At(archiver);
}

// find an archiver by the index in the list view
BOOL CPackACListView::FindArchiver(unsigned int listViewIndex,
                                   unsigned int* archiver, unsigned int* arcIndex)
{
    CALL_STACK_MESSAGE2("CPackACListView::FindArchiver(%u, , )", listViewIndex);
    unsigned int totalCount = 0;
    *archiver = 0;
    while (*archiver < (unsigned int)PackersTable->Count &&
           totalCount + 1 + PackersTable->At(*archiver)->GetCount() <= listViewIndex)
    {
        totalCount += PackersTable->At(*archiver)->GetCount() + 1;
        (*archiver)++;
    }
    if (*archiver >= (unsigned int)PackersTable->Count)
    {
        TRACE_E("Index is out of range - unable to find appropriate record");
        *archiver = 0;
        *arcIndex = 0;
        return TRUE;
    }
    *arcIndex = listViewIndex - totalCount;
    if (*arcIndex == 0)
        return TRUE;
    else
    {
        (*arcIndex)--;
        return FALSE;
    }
}

// initialize the list view
void CPackACListView::Initialize(APackACPackersTable* table)
{
    CALL_STACK_MESSAGE1("CPackACListView::Initialize()");
    // finally we have the data
    PackersTable = table;
    // set the columns
    InitColumns();
    // set the initial number of items in the listview
    ListView_SetItemCount(HWindow, GetCount());
    // set focus on the first listview item
    ListView_SetItemState(HWindow, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
}

// set the headers and basic widths of the listview columns
BOOL CPackACListView::InitColumns()
{
    CALL_STACK_MESSAGE1("CPackACListView::InitColumns()");
    LV_COLUMN lvc;
    // header table
    int header[4] = {IDS_ACCOLUMN1, IDS_ACCOLUMN2, IDS_ACCOLUMN3, IDS_ACCOLUMN4};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    // create all columns
    int i;
    for (i = 0; i < 4; i++)
    {
        // FullName is left aligned, others are right aligned
        if (i == 0)
            lvc.fmt = LVCFMT_LEFT;
        else
            lvc.fmt = LVCFMT_RIGHT;
        // header
        lvc.pszText = LoadStr(header[i]);
        // column number
        lvc.iSubItem = i;
        // and create the column
        if (ListView_InsertColumn(HWindow, i, &lvc) == -1)
            return FALSE;
    }

    // set initial widths of the size, date and time columns
    ListView_SetColumnWidth(HWindow, 3, ListView_GetStringWidth(HWindow, "00:00:00") + 20);
    ListView_SetColumnWidth(HWindow, 2, ListView_GetStringWidth(HWindow, "00.00.0000") + 20);
    ListView_SetColumnWidth(HWindow, 1, ListView_GetStringWidth(HWindow, "000 000") + 20);
    // calculate the width of the fullname column
    SetColumnWidth();

    // enable checkboxes
    ListView_SetExtendedListViewStyle(HWindow, LVS_EX_CHECKBOXES);
    // we keep the checkbox state, windows ask for it
    ListView_SetCallbackMask(HWindow, LVIS_STATEIMAGEMASK);
    // done
    return TRUE;
}

// set the column widths in the listview based on the window size
void CPackACListView::SetColumnWidth()
{
    CALL_STACK_MESSAGE1("CPackACListView::SetColumnWidth()");
    RECT r;
    // find out the size we must fit into
    GetClientRect(HWindow, &r);
    // total width
    DWORD cx = r.right - r.left - 1;
    // subtract the sizes of the fixed columns (size, date, time)
    cx -= ListView_GetColumnWidth(HWindow, 1) + ListView_GetColumnWidth(HWindow, 2) +
          ListView_GetColumnWidth(HWindow, 3) - 1;
    // subtract the scrollbar
    DWORD style = (DWORD)GetWindowLongPtr(HWindow, GWL_STYLE);
    if (!(style & WS_VSCROLL))
        cx -= GetSystemMetrics(SM_CXHSCROLL);
    // and set the width of the variable column (fullname)
    ListView_SetColumnWidth(HWindow, 0, cx);
}

// check the found file and if it is an archiver we want, add it to the Found array
BOOL CPackACListView::ConsiderItem(const char* path, const char* fileName, FILETIME lastWriteTime,
                                   const CQuadWord& size, EPackExeType type)
{
    CALL_STACK_MESSAGE4("CPackACListView::ConsiderItem(%s, %s, , , %d)", path, fileName, type);

    // consistency check
    if (PackersTable == NULL)
        return TRUE;
    // initialization
    BOOL stop = FALSE;
    int totalCount = 0;
    // go through all packers to see if it is the one we are looking for
    int i;
    for (i = 0; i < PackersTable->Count; i++)
    {
        // take the packer
        CPackACPacker* item = PackersTable->At(i);
        // do we want it?
        int ret = item->CheckAndInsert(path, fileName, lastWriteTime, size, type);
        if (ret < 0)
        {
            // some problem with the array
            SendMessage(ACDialog->HWindow, WM_USER_ACERROR, (WPARAM)LoadStr(IDS_CANTSHOWRESULTS), NULL);
            TRACE_E("Problems with array detected.");
            stop = TRUE;
        }
        else if (ret > 0)
        {
            TRACE_I("Packer " << fileName << " in location " << path << " found and added");
            // we added an item, redraw from the start of the category onward (without the heading)
            SendMessage(ACDialog->HWindow, WM_USER_ACADDFILE, (WPARAM)(totalCount + 1), 0);
            // we won't search further
            break;
        }
        // keep total count for the listview index
        totalCount += 1 + item->GetCount();
    }
    // did an error occur and do we have to stop?
    return stop;
}

// window procedure for the listview with found archivers
LRESULT
CPackACListView::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPackACListView::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_KEYDOWN:
    {
        // handle space bar to toggle the checkbox selection
        if (wParam == VK_SPACE)
        {
            // find the current item
            int index = ListView_GetNextItem(HWindow, -1, LVNI_FOCUSED);
            // if it is not a header, change the checkbox
            if (index > -1)
                InvertSelect(index);
        }
        break;
    }
    case WM_LBUTTONDOWN:
    {
        // handle checkbox interaction from the mouse
        LV_HITTESTINFO htInfo;
        htInfo.pt.x = LOWORD(lParam);
        htInfo.pt.y = HIWORD(lParam);
        ListView_HitTest(HWindow, &htInfo);
        // if it was clicked into the checkbox, change its state
        if (htInfo.flags & LVHT_ONITEMSTATEICON)
            InvertSelect(htInfo.iItem);
        break;
    }
    }
    // default processing
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CPackACFound
//

// returns the text that belongs to the given column
char* CPackACFound::GetText(int column)
{
    CALL_STACK_MESSAGE2("CPackACFound::GetText(%d)", column);
    static char text[100];
    switch (column)
    {
    // Name
    case 0:
        return FullName;
    // Size
    case 1:
    {
        NumberToStr(text, Size);
        return text;
    }
    // Date
    case 2:
    {
        SYSTEMTIME st;
        FILETIME ft;
        if (FileTimeToLocalFileTime(&LastWrite, &ft) &&
            FileTimeToSystemTime(&ft, &st))
        {
            if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, text, 100) == 0)
                sprintf(text, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        }
        else
            strcpy(text, LoadStr(IDS_INVALID_DATEORTIME));
        return text;
    }
    // Time
    default:
    {
        SYSTEMTIME st;
        FILETIME ft;
        if (FileTimeToLocalFileTime(&LastWrite, &ft) &&
            FileTimeToSystemTime(&ft, &st))
        {
            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, text, 100) == 0)
                sprintf(text, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        }
        else
            strcpy(text, LoadStr(IDS_INVALID_DATEORTIME));
        return text;
    }
    }
}

// set the item to the desired values
BOOL CPackACFound::Set(const char* fullName, const CQuadWord& size, FILETIME lastWrite)
{
    CALL_STACK_MESSAGE2("CPackACFound::Set(%s, , )", fullName);
    FullName = (char*)malloc(strlen(fullName) + 1);
    if (FullName != NULL)
        strcpy(FullName, fullName);
    else
        return FALSE;
    Size = size;
    LastWrite = lastWrite;
    return TRUE;
}

//
// ****************************************************************************
// CPackACDrives
//

// dialog procedure for the disk selection dialog
INT_PTR
CPackACDrives::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPackACDrives::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        EditLB = new CEditListBox(HWindow, IDC_ACDRVLIST);
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        else
        {
            EditLB->MakeHeader(IDS_ACDRVHDR);
            EditLB->EnableDrag(HWindow);
        }
        break;
    }
    case WM_USER_EDIT:
    {
        SetFocus(GetDlgItem(HWindow, IDC_ACDRVLIST));
        EditLB->OnBeginEdit((int)wParam, (int)lParam);
        return 0;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_ACDRVLIST:
        {
            if (HIWORD(wParam) == LBN_SELCHANGE)
                EditLB->OnSelChanged();
            break;
        }
        case IDCANCEL:
        {
            // free the memory copy of the drive list
            int i;
            for (i = 0; i < EditLB->GetCount(); i++)
            {
                INT_PTR itemID;
                EditLB->GetItemID(i, itemID);
                free((char*)itemID);
            }
            EditLB->DeleteAllItems();
            break;
        }
        }
        break;
    }
    case WM_NOTIFY:
    {
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDC_ACDRVLIST:
        {
            EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                if (dispInfo->ToDo == edtlbGetData)
                {
                    strncpy_s(dispInfo->Buffer, dispInfo->BufferLen, (char*)dispInfo->ItemID, _TRUNCATE);
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                }
                else
                {
                    Dirty = TRUE;
                    char* txt = dispInfo->Buffer;
                    // cut off leading spaces
                    while (*txt == ' ' || *txt == '\t')
                        txt++;
                    // determine the length without trailing spaces
                    int len = (int)strlen(txt) - 1; // if txt is an empty string, len == -1
                    while (len > 0 && (*(txt + len) == ' ' || *(txt + len) == '\t'))
                        len--;
                    len++;
                    char* newPath = (char*)malloc(len + 1);
                    if (newPath == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                        return TRUE;
                    }
                    memcpy(newPath, txt, len);
                    *(newPath + len) = '\0';
                    if (dispInfo->ItemID == -1)
                        EditLB->SetItemData((INT_PTR)newPath);
                    else
                    {
                        free((char*)dispInfo->ItemID);
                        EditLB->SetItemID(dispInfo->Index, (INT_PTR)newPath);
                    }
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                }
                return TRUE;
            }
            /*
            case EDTLBN_MOVEITEM:
            {
              int srcItemID, dstItemID;
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              int srcIndex = dispInfo->Index;
              int dstIndex = dispInfo->Index + (dispInfo->Up ? -1 : 1);
              EditLB->GetItemID(srcIndex, srcItemID);
              EditLB->GetItemID(dstIndex, dstItemID);
              EditLB->SetItemID(srcIndex, dstItemID);
              EditLB->SetItemID(dstIndex, srcItemID);
              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // allow swapping
              return TRUE;
            }
            */
            case EDTLBN_MOVEITEM2:
            {
                Dirty = TRUE;
                EDTLB_DISPINFO* dispInfo2 = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);

                int srcIndex = index;
                int dstIndex = dispInfo2->NewIndex;

                INT_PTR tmpItemID, tmpItemID2;
                EditLB->GetItemID(srcIndex, tmpItemID);
                if (srcIndex < dstIndex)
                {
                    int i;
                    for (i = srcIndex; i < dstIndex; i++)
                    {
                        EditLB->GetItemID(i + 1, tmpItemID2);
                        EditLB->SetItemID(i, tmpItemID2);
                    }
                }
                else
                {
                    int i;
                    for (i = srcIndex; i > dstIndex; i--)
                    {
                        EditLB->GetItemID(i - 1, tmpItemID2);
                        EditLB->SetItemID(i, tmpItemID2);
                    }
                }
                EditLB->SetItemID(dstIndex, tmpItemID);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow the change
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                Dirty = TRUE;
                free((char*)dispInfo->ItemID);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow deletion
                return TRUE;
            }
            }
            break;
        }
        }
        break;
    }
    case WM_DRAWITEM:
    {
        int idCtrl = (int)wParam;
        if (idCtrl == IDC_ACDRVLIST)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }
    }
    // default message processing
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

void CPackACDrives::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPackACDrives::Validate()");
    if (Dirty)
    {
        int i;
        for (i = 0; i < EditLB->GetCount(); i++)
        {
            INT_PTR itemID;
            EditLB->GetItemID(i, itemID);
            DWORD attr = SalGetFileAttributes((char*)itemID);
            if (attr == -1 || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                EditLB->SetCurSel(i);
                SalMessageBox(HWindow, LoadStr(IDS_ACBADDRIVE), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(IDC_ACDRVLIST);
                PostMessage(HWindow, WM_USER_EDIT, 0, strlen((char*)itemID));
                return;
            }
        }
    }
}

void CPackACDrives::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPackACDrives::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        Dirty = FALSE;
        // pour the list of drives into the control
        char* path = *DrivesList;
        while (*path != '\0')
        {
            unsigned int len = (unsigned int)strlen(path) + 1;
            char* newPath = (char*)malloc(len);
            if (newPath == NULL)
            {
                TRACE_E(LOW_MEMORY);
                return;
            }
            strcpy(newPath, path);
            EditLB->AddItem((INT_PTR)newPath);
            path += len;
        }
        EditLB->SetCurSel(0);
    }
    else
    {
        if (Dirty)
        {
            int i;
            unsigned long len = 0;
            // determine the required size for the list of drives
            for (i = 0; i < EditLB->GetCount(); i++)
            {
                INT_PTR itemID;
                EditLB->GetItemID(i, itemID);
                unsigned long pathlen = (unsigned long)strlen((char*)itemID);
                len += pathlen + 1;
                // if the path does not end with a backslash, we need to account for it
                if (*((char*)itemID + pathlen - 1) != '\\')
                    len++;
            }
            HANDLES(GlobalFree((HGLOBAL)*DrivesList));
            *DrivesList = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, len + 1));
            if (*DrivesList == NULL)
                TRACE_E(LOW_MEMORY);
            else
            {
                char* path = *DrivesList;
                // and extract the list
                for (i = 0; i < EditLB->GetCount(); i++)
                {
                    INT_PTR itemID;
                    EditLB->GetItemID(i, itemID);
                    unsigned long pathlen = (unsigned long)strlen((char*)itemID);
                    memcpy(path, (char*)itemID, pathlen + 1);
                    if (*(path + pathlen - 1) != '\\')
                    {
                        *(path++ + pathlen) = '\\';
                        *(path + pathlen) = '\0';
                    }
                    path += pathlen + 1;
                    free((char*)itemID);
                }
                EditLB->DeleteAllItems();
                *path = '\0';
            }
        }
    }
}

//
// ****************************************************************************
// Autoconfig
//

// start autoconfiguration
void PackAutoconfig(HWND parent)
{
    CALL_STACK_MESSAGE1("PackAutoconfig()");

    // stop refreshes in the main window
    BeginStopRefresh();
    // determine the buffer needed for drives to search
    DWORD size = GetLogicalDriveStrings(0, NULL);
    char* sysDrives = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, size));
    char* drives = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, size));
    if (sysDrives == NULL || drives == NULL)
    {
        TRACE_E(LOW_MEMORY);
        if (sysDrives != NULL)
            HANDLES(GlobalFree((HGLOBAL)sysDrives));
        if (drives != NULL)
            HANDLES(GlobalFree((HGLOBAL)drives));
    }
    else
    {
        DWORD newSize = GetLogicalDriveStrings(size, sysDrives);
        if (newSize > size)
            TRACE_E("The drives buffer size requested by system is too small...");
        else
        {
            char* dstDrive = drives;
            // skip non-fixed drives...
            char* srcDrive = sysDrives;
            while (*srcDrive != '\0')
            {
                // what kind is it? is it worth our time?
                if (GetDriveType(srcDrive) == DRIVE_FIXED)
                {
                    strcpy(dstDrive, srcDrive);
                    dstDrive += strlen(dstDrive) + 1;
                }
                else
                    TRACE_I("Skipping drive " << srcDrive << ", not fixed.");
                srcDrive += strlen(srcDrive) + 1;
            }
            *dstDrive = '\0';
            HANDLES(GlobalFree((HGLOBAL)sysDrives));
            // open the search dialog
            CPackACDialog(HLanguage, IDD_AUTOCONF, IDD_AUTOCONF, parent, &ArchiverConfig, &drives).Execute();
        }
        HANDLES(GlobalFree((HGLOBAL)drives));
    }
    // dialog closed, the main window resumes refreshing
    EndStopRefresh();
}
