// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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

// type of item in the extension table of packers
struct SPackAssocItem
{
    const char* Ext; // extenze packers
    int nextIndex;   // index of the next file for the same archive, -1 if no other exists
};

// table of extension associations of packers
// the first item is a string of masks, the second is the index of the next packer of the same type
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
        // listview construction
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
        // Button Stop/Restart will be the start button
        SetDlgItemText(HWindow, IDB_ACSTOP, LoadStr(IDS_ACBUTTON_RESCAN));
        // Disable OK and enable Drives
        EnableWindow(GetDlgItem(HWindow, IDB_ACDRIVES), TRUE);
        EnableWindow(GetDlgItem(HWindow, IDOK), FALSE);
        // the OK button will not be the default push button now
        PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
        // Button Start will now be the default push button
        PostMessage(HWindow, DM_SETDEFID, IDB_ACSTOP, NULL);
        // load parameters for window layout
        GetLayoutParams();
        // laying out the window
        LayoutControls();
        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_ACSTOP:
        {
            // if we are searching, it is the STOP button, otherwise Rescan
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
                // Button Stop/Restart will be again button stop
                SetDlgItemText(HWindow, IDB_ACSTOP, LoadStr(IDS_ACBUTTON_STOP));
                // Disable OK and Drives
                EnableWindow(GetDlgItem(HWindow, IDB_ACDRIVES), FALSE);
                EnableWindow(GetDlgItem(HWindow, IDOK), FALSE);
                // the OK button will not be the default push button now
                PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
                // the STOP button will be the default push button
                HWND focus = GetFocus();
                PostMessage(HWindow, DM_SETDEFID, IDB_ACSTOP, NULL);
                if (focus != ListView->HWindow && focus != GetDlgItem(HWindow, IDC_ACMODCUSTOM))
                {
                    // If the focus was on a button, we need to return it there
                    PostMessage(GetDlgItem(HWindow, IDB_ACSTOP), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
                    PostMessage(focus, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
                }
                // we will create the event again to signal the interruption of the search
                StopSearch = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
                // and start the search thread
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
            // if the second thread is running, we ask...
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
                // display the item and its state (we hold the data, not the listview)
                LV_DISPINFO* info = (LV_DISPINFO*)lParam;
                int index;
                CPackACPacker* packer = ListView->GetPacker(info->item.iItem, &index);
                // if the text was requested, we will extract it
                if (info->item.mask & LVIF_TEXT)
                    info->item.pszText = (char*)packer->GetText(index, info->item.iSubItem);
                // if the checkbox icon is checked, we will also pull it out
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
        // Search thread added a file, we need to redraw the affected area
        UpdateListViewItems((int)wParam);
        return TRUE;
    }
    case WM_USER_ACERROR:
    {
        // We have a problem, so let the user know about it...
        // we are an icon, jumping up
        if (IsIconic(HWindow))
            ShowWindow(HWindow, SW_RESTORE);
        // and display an error
        MessageBox(HWindow, (char*)wParam, LoadStr(IDS_ERRORFINDINGFILE), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }
    case WM_USER_ACSEARCHING:
    {
        // The search thread has sent us a new path it's working on, so we'll trim the slash at the end...
        if (((char*)wParam)[lstrlen((char*)wParam) - 1] == '\\')
            ((char*)wParam)[lstrlen((char*)wParam) - 1] = '\0';
        // and display it
        SetDlgItemText(HWindow, IDC_ACSTATUS, (char*)wParam);
        // and let's free the memory...
        HANDLES(GlobalFree((HGLOBAL)wParam));
        return TRUE;
    }
    case WM_USER_ACFINDFINISHED:
    {
        // Wait for the actual completion of the search thread and clean up
        if (HSearchThread != NULL)
        {
            WaitForSingleObject(HSearchThread, INFINITE);
            HANDLES(CloseHandle(HSearchThread));
            HSearchThread = NULL;
        }
        // Release the signaling event
        if (StopSearch != NULL)
        {
            HANDLES(CloseHandle(StopSearch));
            StopSearch = NULL;
        }
        // if the thread has finished because we are closing the window, continue with the closing
        if (WillExit)
            PostMessage(HWindow, WM_COMMAND, MAKELONG(IDCANCEL, 0), 0);
        else
        {
            // we will put everything in order
            SetDlgItemText(HWindow, IDB_ACSTOP, LoadStr(IDS_ACBUTTON_RESCAN));
            SetDlgItemText(HWindow, IDC_ACSTATUS, LoadStr(IDS_ACSTATUSDONE));
            EnableWindow(GetDlgItem(HWindow, IDB_ACDRIVES), TRUE);
            EnableWindow(GetDlgItem(HWindow, IDOK), TRUE);
            // Fix the default push button
            HWND focus = GetFocus();
            PostMessage(HWindow, DM_SETDEFID, IDOK, NULL);
            if (focus != ListView->HWindow && focus != GetDlgItem(HWindow, IDC_ACMODCUSTOM))
            {
                // If the focus was on a button, we need to return it there
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

// Stores the initial dimensions of the dialog for resizing
void CPackACDialog::GetLayoutParams()
{
    CALL_STACK_MESSAGE1("CPackACDialog::GetLayoutParams()");
    // minimum dimensions of the dialog
    RECT wr;
    GetWindowRect(HWindow, &wr);
    MinDlgW = wr.right - wr.left;
    MinDlgH = wr.bottom - wr.top;

    // client part of the window
    RECT cr;
    GetClientRect(HWindow, &cr);

    // auxiliary dimensions for converting non-client and client coordinates
    int windowMargin = ((wr.right - wr.left) - (cr.right)) / 2;
    int captionH = wr.bottom - wr.top - cr.bottom - windowMargin;

    // space to the left and right between the dialog frame and controls
    RECT br;
    GetWindowRect(GetDlgItem(HWindow, IDC_ACLIST), &br);
    HMargin = br.left - wr.left - windowMargin;

    // space below the buttons and above the status bar
    VMargin = HMargin;

    // dimensions of buttons
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

    // height of status bar
    GetWindowRect(HStatusBar, &br);
    StatusHeight = br.bottom - br.top;

    // height of the checkbox
    CheckH = br.bottom - br.top;

    // placement of the result list
    GetWindowRect(GetDlgItem(HWindow, IDC_ACLIST), &br);
    ListY = br.top - wr.top - captionH;
}

// resizes the dialog window
void CPackACDialog::LayoutControls()
{
    CALL_STACK_MESSAGE1("CPackACDialog::LayoutControls()");
    RECT clientRect;

    // take the size that we have available
    GetClientRect(HWindow, &clientRect);
    clientRect.bottom -= StatusHeight;

    // and now we will eat it in one go
    HDWP hdwp = HANDLES(BeginDeferWindowPos(8));
    if (hdwp != NULL)
    {
        // Place Status Bar
        hdwp = HANDLES(DeferWindowPos(hdwp, HStatusBar, NULL,
                                      0, clientRect.bottom, clientRect.right, StatusHeight,
                                      SWP_NOZORDER));

        // Place the Help button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDHELP), NULL,
                                      clientRect.right - ButtonW5 - HMargin,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // Place the Cancel button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDCANCEL), NULL,
                                      clientRect.right - (ButtonW4 + ButtonW5) - HMargin - ButtonMargin,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // place the OK button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDOK), NULL,
                                      clientRect.right - (ButtonW3 + ButtonW4 + ButtonW5) - HMargin - ButtonMargin * 2,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // Place the Stop/Rescan button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_ACSTOP), NULL,
                                      clientRect.right - (ButtonW2 + ButtonW3 + ButtonW4 + ButtonW5) - HMargin - ButtonMargin * 3,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // Place the button Drives
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_ACDRIVES), NULL,
                                      clientRect.right - (ButtonW1 + ButtonW2 + ButtonW3 + ButtonW4 + ButtonW5) - HMargin - ButtonMargin * 4,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // Check-box Custom archivers
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_ACMODCUSTOM), NULL,
                                      HMargin, clientRect.bottom - VMargin - ButtonH / 2 - CheckH / 2,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // place and stretch the list view
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_ACLIST), NULL,
                                      HMargin, ListY, clientRect.right - HMargin * 2,
                                      clientRect.bottom - ListY - ButtonH - VMargin * 2,
                                      SWP_NOZORDER));
        // block finished
        HANDLES(EndDeferWindowPos(hdwp));
    }
    // and adjust the width of the columns in the listview
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
            // Now that we have the header, check the e_magic field to see if this is a DOS image.
            if (mz_header.e_magic == IMAGE_DOS_SIGNATURE)
            {
                char magic[4];
                BOOL lfanewValid = FALSE;
                // We do have a DOS image so we will now try to seek into the file by the amount indicated by the field
                // "Offset to extended header" and read in the "magic" field information at that location.
                // This will tell us if there is more header information to read or not.
                //
                // But before we do, we will make sure that the header structure encompasses the "Offset to extended header" field.
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
                        // header." This can mean either a 16-bit OS/2 or a 16-bit Windows or even a DOS program
                        // (running under a DOS extender). To decide which, we'll have to read the NE header.
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

// own recursive (and recursive :-) ) function for disk searching
BOOL CPackACDialog::DirectorySearch(char* path)
{
    CALL_STACK_MESSAGE2("CPackACDialog::DirectorySearch(%s)", path);

    // Give the boss an update on what we are working on
    DWORD pathLen = (DWORD)strlen(path);
    char* workName = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, pathLen + 1));
    if (workName == NULL)
    {
        SendMessage(HWindow, WM_USER_ACERROR, (WPARAM)LoadStr(IDS_PACKERR_NOMEM), NULL);
        TRACE_E(LOW_MEMORY);
        return TRUE;
    }
    strcpy(workName, path);
    // send the name, memory release will be done by the recipient
    if (!PostMessage(HWindow, WM_USER_ACSEARCHING, (WPARAM)workName, 0))
        // if it didn't work out, forget about it...
        HANDLES(GlobalFree((HGLOBAL)workName));

    // allocate a buffer for the search mask
    char* fileName = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, pathLen + 3 + 1));
    if (fileName == NULL)
    {
        SendMessage(HWindow, WM_USER_ACERROR, (WPARAM)LoadStr(IDS_PACKERR_NOMEM), NULL);
        TRACE_E(LOW_MEMORY);
        return TRUE;
    }
    // prepare a mask for searching
    strcpy(fileName, path);
    strcat(fileName, "*");

    // set some variables
    BOOL mustStop = FALSE;
    WIN32_FIND_DATA findData;
    // let's try to find the first file
    HANDLE fileFind = HANDLES_Q(FindFirstFile(fileName, &findData));
    if (fileFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            unsigned int nameLen = (unsigned int)strlen(findData.cFileName);
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                // It's a file, now we will check if it's executable
                if (nameLen > 4 && pathLen + nameLen < MAX_PATH &&
                    (findData.cFileName[nameLen - 1] == 'e' || findData.cFileName[nameLen - 1] == 'E') &&
                    (findData.cFileName[nameLen - 2] == 'x' || findData.cFileName[nameLen - 2] == 'X') &&
                    (findData.cFileName[nameLen - 3] == 'e' || findData.cFileName[nameLen - 3] == 'E') &&
                    findData.cFileName[nameLen - 4] == '.')
                {
                    // determine the type of program
                    char fullName[MAX_PATH];
                    DWORD type;
                    strcpy(fullName, path);
                    strcat(fullName, findData.cFileName);

                    if (!MyGetBinaryType(fullName, &type))
                    {
                        TRACE_I("Invalid executable or error getting type: " << fullName);
                        continue;
                    }
                    // and let's see if we're interested in him...
                    mustStop |= ListView->ConsiderItem(path, findData.cFileName,
                                                       findData.ftLastWriteTime,
                                                       CQuadWord(findData.nFileSizeLow,
                                                                 findData.nFileSizeHigh),
                                                       type == SCS_32BIT_BINARY ? EXE_32BIT : EXE_16BIT);
                }
            }
            else
            {
                // We have a directory - we exclude '.' and '..'
                if (findData.cFileName[0] != 0 &&
                    (findData.cFileName[0] != '.' ||
                     (findData.cFileName[1] != '\0' &&
                      (findData.cFileName[1] != '.' || findData.cFileName[2] != '\0'))) &&
                    pathLen + 1 + nameLen < MAX_PATH)
                {
                    // create a directory name for searching
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
                        // and recursively search it
                        DirectorySearch(newPath);
                        HANDLES(GlobalFree((HGLOBAL)newPath));
                    }
                }
            }
            // check if an interrupt is signaled
            DWORD ret = WaitForSingleObject(StopSearch, 0);
            if (ret != WAIT_TIMEOUT)
                mustStop = TRUE;
        } while (FindNextFile(fileFind, &findData) && !mustStop);
        HANDLES(FindClose(fileFind));
    }
    HANDLES(GlobalFree((HGLOBAL)fileName));
    // and the end
    return mustStop;
}

// calling recursive search for all local fixed disks
DWORD
CPackACDialog::DiskSearch()
{
    CALL_STACK_MESSAGE1("CPackACDialog::DiskSearch()");
    // initialize trace for a new thread
    SetThreadNameInVCAndTrace("AutoConfig");
    TRACE_I("Begin");

    // sanity checking
    if (DrivesList == NULL || *DrivesList == NULL || **DrivesList == '\0')
        TRACE_I("The list of drives is empty...");
    else
    {
        char* drive = *DrivesList;
        // and we will go through all the drives we have...
        BOOL mustStop = FALSE;
        while (*drive != '\0' && !mustStop)
        {
            // and we're going downhill...
            TRACE_I("Searching drive " << drive);
            mustStop = DirectorySearch(drive);
            // move to the next in the list
            while (*drive != '\0')
                drive++;
            // and skip the trailing null
            drive++;
        }
    }
    // we will announce the end of the search
    PostMessage(HWindow, WM_USER_ACFINDFINISHED, 0, 0);
    // we are not searching anymore
    SearchRunning = FALSE;
    TRACE_I("End");
    return 0;
}

// Wrapper for search function with exception handling
unsigned int
CPackACDialog::PackACDiskSearchThreadEH(void* instance)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        // triggering custom search
        return ((CPackACDialog*)instance)->DiskSearch();
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("PackACDiskSearchThreadEH: calling ExitProcess(1).");
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

// Wrapper for wrapper for searching - this time a static function called from CreateThread
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

    // add extension and possibly a custom packer if it does not exist
    // packer is added if there is no file mask corresponding to the found packer
    // if it exists and points to something other than a plugin or 32-bit, redirect to the found
    char buffer[10];
    const char* ptr = PackACExtensions[packerIndex].Ext;
    int found;
    do
    {
        // we are looking for who is using us
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
        // we have assembled "archive name", now if we have it in extensions...
        found = PackerFormatConfig.PackIsArchive(buffer);
        if (found != 0)
        {
            int pos;
            CPackACPacker* p = NULL;
            // if we are working with a packer, we fix the record for the packer
            if (foundPacker->GetPackerType() == Packer_Packer || foundPacker->GetPackerType() == Packer_Standalone)
            {
                // if we were using compression, we will check if we found it
                if (PackerFormatConfig.GetUsePacker(found - 1))
                {
                    pos = PackerFormatConfig.GetPackerIndex(found - 1);
                    // let's find out if we have found him
                    int k;
                    for (k = 0; k < ListView->GetPackersCount(); k++)
                    {
                        p = ListView->GetPacker(k);
                        if (p->GetArchiverIndex() == pos && p->GetPackerType() != Packer_Unpacker)
                            break;
                    }
                }
                // replace packing if:
                // 1) we didn't use the packer at all
                // 2) or we used an external compressor, different from the one we are checking and
                // 3) The original packer was not found or I have a 32-bit one (we are better)
                // if we haven't used the packer before, we'll throw it on
                if (!PackerFormatConfig.GetUsePacker(found - 1) ||
                    (pos >= 0 && pos != packerIndex &&
                     (p == NULL || p->GetSelectedFullName() == NULL || foundPacker->GetExeType() == EXE_32BIT)))
                {
                    TRACE_I("Setting packer for extension " << PackACExtensions[packerIndex].Ext << " to the new one.");
                    PackerFormatConfig.SetPackerIndex(found - 1, packerIndex);
                    PackerFormatConfig.SetUsePacker(found - 1, TRUE);
                    // and we will refresh the table
                    PackerFormatConfig.BuildArray();
                }
            }
            // if we are working with a unpacker, we modify the record for the unpacker
            if (foundPacker->GetPackerType() == Packer_Unpacker || foundPacker->GetPackerType() == Packer_Standalone)
            {
                // if we found, we will check the settings
                pos = PackerFormatConfig.GetUnpackerIndex(found - 1);
                // check if we have found the current unpacker
                int k;
                for (k = 0; k < ListView->GetPackersCount(); k++)
                {
                    p = ListView->GetPacker(k);
                    if (p->GetArchiverIndex() == pos && p->GetPackerType() != Packer_Packer)
                        break;
                }
                // if the current packer is not a plugin, or we haven't found it, or it is different and we have 32bit
                if (pos >= 0 && pos != packerIndex &&
                    (p == NULL || p->GetSelectedFullName() == NULL || foundPacker->GetExeType() == EXE_32BIT))
                {
                    TRACE_I("Changing unpacker for extension " << PackACExtensions[packerIndex].Ext << " to the new one.");
                    PackerFormatConfig.SetUnpackerIndex(found - 1, packerIndex);
                    // and we will refresh the table
                    PackerFormatConfig.BuildArray();
                }
            }
        }
    } while (*ptr != '\0');
    // we searched all extensions, and if we didn't find any, we need to add
    if (found == 0)
    {
        // if we only have a packer, then we must also have an unpacker (we would already find a plugin)
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

    // we remove the association from the extension
    // association is thrown if the viewer was not found or was found but not selected (fullName == NULL)
    // pakovac je prepnut na --not supported--, pokud packer nebyl nalezen nebo byl nalezen, ale nebyl vybran (fullName == NULL)

    // search for extensions
    int i;
    for (i = PackerFormatConfig.GetFormatsCount() - 1; i >= 0; i--)
    {
        // if our association uses us as a viewer, we will try to switch to another one, otherwise we cut
        if (packerIndex == PackerFormatConfig.GetUnpackerIndex(i))
        {
            // find an alternative
            CPackACPacker* p = NULL;
            if (PackACExtensions[packerIndex].nextIndex >= 0)
                p = ListView->GetPacker(PackACExtensions[packerIndex].nextIndex);
            // if it's just a packer (zip), we'll take the unpacker
            if (p != NULL && p->GetPackerType() == Packer_Packer)
                p = ListView->GetPacker(PackACExtensions[packerIndex].nextIndex + 1);
            if (p == NULL || p->GetSelectedFullName() == NULL)
            {
                // if there is no alternative, we cut
                TRACE_I("Removing extensions " << PackerFormatConfig.GetExt(i));
                PackerFormatConfig.DeleteFormat(i);
                PackerFormatConfig.BuildArray();
            }
            else
            {
                // We have an alternative that we found, let's switch
                TRACE_I("Changing viewer for extensions " << PackerFormatConfig.GetExt(i) << " to another.");
                PackerFormatConfig.SetUnpackerIndex(i, p->GetArchiverIndex());
                PackerFormatConfig.BuildArray();
            }
        }
        else
        {
            // if our association is used only as an editor, we will try to redirect, otherwise we will set it to --not supported--
            if (PackerFormatConfig.GetUsePacker(i) && PackerFormatConfig.GetPackerIndex(i) == packerIndex)
            {
                CPackACPacker* p = NULL;
                if (PackACExtensions[packerIndex].nextIndex >= 0)
                    p = ListView->GetPacker(PackACExtensions[packerIndex].nextIndex);
                if (p == NULL || p->GetSelectedFullName() == NULL)
                {
                    // alternative does not exist
                    TRACE_I("Setting packer to --not supported-- for extension " << PackerFormatConfig.GetExt(i));
                    // another unpacker is here, we'll throw it for packer --not supported--
                    PackerFormatConfig.SetUsePacker(i, FALSE);
                    PackerFormatConfig.BuildArray();
                }
                else
                {
                    // We have an alternative that we found, let's switch
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

    // add custom packers for newly found packers
    char variable[50];
    int i;
    BOOL found1 = FALSE, found2 = FALSE;
    sprintf(variable, "$(%s)", ArchiverConfig->GetPackerVariable(packerIndex));
    // search for custom packers to see if there are any already there
    for (i = 0; i < PackerConfig.GetPackersCount(); i++)
    {
        // we only take external
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

    // we add custom unpackers for newly found packers
    if (!ArchiverConfig->ArchiverExesAreSame(packerIndex))
        sprintf(variable, "$(%s)", ArchiverConfig->GetUnpackerVariable(packerIndex));
    found1 = FALSE;
    // search for custom packers to see if there are any already there
    for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
    {
        // we only take external
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

    // custom packer/unpacker is thrown away if:
    //   1) the package was not found or was found but not selected (fullName == NULL)
    //   2) The called program is a variable corresponding to this package

    CPackACPacker* p = NULL;
    char variable[50];
    sprintf(variable, "$(%s)", ArchiverConfig->GetPackerVariable(packerIndex));
    int i;
    // search for custom packers (backwards so we can remove them)
    for (i = PackerConfig.GetPackersCount() - 1; i >= 0; i--)
        // we only take external
        if (PackerConfig.GetPackerType(i) >= 0)
        {
            const char* cmd = PackerConfig.GetPackerCmdExecCopy(i);
            if (!strcmp(cmd, variable))
            {
                // this packer calls a program that we did not find - to be continued...
                TRACE_I("Removing custom packer which uses not found packer: " << variable);
                PackerConfig.DeletePacker(i);
                // we don't have to look at the move cmd anymore, the packer doesn't exist anymore
            }
            else if (PackerConfig.GetPackerSupMove(i))
            {
                cmd = PackerConfig.GetPackerCmdExecMove(i);
                if (!strcmp(cmd, variable))
                {
                    // calls the move cmd program, which does not exist - turn off
                    TRACE_I("Disabling Move command for custom packer which uses not found packer: " << variable);
                    PackerConfig.SetPackerSupMove(i, FALSE);
                }
            }
        }
    if (!ArchiverConfig->ArchiverExesAreSame(packerIndex))
        sprintf(variable, "$(%s)", ArchiverConfig->GetUnpackerVariable(packerIndex));
    // search for custom unpackers
    for (i = UnpackerConfig.GetUnpackersCount() - 1; i >= 0; i--)
        // we only take external
        if (UnpackerConfig.GetUnpackerType(i) >= 0)
        {
            const char* cmd = UnpackerConfig.GetUnpackerCmdExecExtract(i);
            if (!strcmp(cmd, variable))
            {
                // this unpacker calls a program that we did not find - to be continued...
                TRACE_I("Removing custom unpacker which uses not found packer: " << variable);
                UnpackerConfig.DeleteUnpacker(i);
            }
        }
}

// Function called before starting and after finishing the data transfer dialog
void CPackACDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPackACDialog::Transfer()");
    // Are we starting or finishing?
    if (ti.Type == ttDataToWindow)
    {
        // create a table of searched packers
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
        // we will add it to the listview
        ListView->Initialize(table);
        // checkbox checked by default
        int value = 1;
        ti.CheckBox(IDC_ACMODCUSTOM, value);
        // we are not looking now
        SearchRunning = FALSE;
    }
    else
    {
        int doCustom;
        // get the value of the checkbox
        ti.CheckBox(IDC_ACMODCUSTOM, doCustom);
        int i;
        // we will go through all packages - configure paths and remove associations for those we did not find
        for (i = 0; i < ListView->GetPackersCount(); i++)
        {
            // let's ask for a package
            CPackACPacker* packer = ListView->GetPacker(i);
            int index = packer->GetArchiverIndex();
            const char* fullName = packer->GetSelectedFullName();
            // if we didn't find it, we leave the original value
            if (fullName != NULL)
            {
                // save the path to the configuration
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
        // we will go through all the packages again, this time just adding new associations
        for (i = 0; i < ListView->GetPackersCount(); i++)
        {
            // let's ask for a package
            CPackACPacker* packer = ListView->GetPacker(i);
            int index = packer->GetArchiverIndex();
            const char* fullName = packer->GetSelectedFullName();
            // if we didn't find it, we leave the original value
            if (fullName != NULL)
            {
                AddToExtensions(i, index, packer);
                if (doCustom)
                    AddToCustom(i, index, packer);
            }
        }
    }
}

// handles the request to add an item to the listview
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

// returns the number of packets in the array
int CPackACPacker::GetCount()
{
    SLOW_CALL_STACK_MESSAGE1("CPackACPacker::GetCount()");
    int count;
    HANDLES(EnterCriticalSection(&FoundDataCriticalSection));
    count = Found.Count;
    HANDLES(LeaveCriticalSection(&FoundDataCriticalSection));
    return count;
}

// Toggle file selection (checkbox inversion)
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

// returns the text belonging to the given column
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

// returns whether the item is selected or not (whether it is a header)
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
            // is selected
            ret = 2;
        else
            // not selected
            ret = 1;
    }
    else
        // title
        ret = 0;
    return ret;
}

// check if we are interested in the given program and if so, add it
int CPackACPacker::CheckAndInsert(const char* path, const char* fileName, FILETIME lastWriteTime,
                                  const CQuadWord& size, EPackExeType exeType)
{
    CALL_STACK_MESSAGE3("CPackACPacker::CheckAndInsert(%s, %s, , , )", path, fileName);
    const char* ref = Name;
    const char* act = fileName;
    // Is the name correct?
    while (*ref != '\0' && *act != '\0' && tolower(*ref) == tolower(*act))
    {
        ref++;
        act++;
    }
    // we have already checked the suffix, now just if we are at the end of the string
    // and if other requirements are met (for now just the type, we will see in the future...)
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
        // We have a new found item, so let's check if it's new for us
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
        // and we add it
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
                // and add to the array
                index = Found.AddAndCheck(newItem);
                if (!Found.IsGood())
                {
                    Found.ResetState();
                    good = FALSE;
                }
                HANDLES(LeaveCriticalSection(&FoundDataCriticalSection));
            }
            // now just a check to see how we did
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

// Ensures selecting only the latest items and adds them to the array
int CPackACArray::AddAndCheck(CPackACFound* member)
{
    CALL_STACK_MESSAGE1("CPackACArray::AddAndCheck()");
    // if it is the first, it will be selected
    if (Count == 0)
        member->Selected = TRUE;
    else
    {
        int i;
        for (i = 0; i < Count; i++)
            // compare with the currently selected item
            if (At(i)->Selected && CompareFileTime(&At(i)->LastWrite, &member->LastWrite) == -1)
            {
                // if it is newer, we replace the selection
                At(i)->Selected = FALSE;
                member->Selected = TRUE;
            }
    }
    // and insert into the array
    return TIndirectArray<CPackACFound>::Add(member);
}

// Toggle the state of the checkbox item
void CPackACArray::InvertSelect(int index)
{
    CALL_STACK_MESSAGE2("CPackACArray::InvertSelect(%d)", index);
    // we must allow the user to select nothing
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

// returns the FullName of the selected item
const char*
CPackACArray::GetSelectedFullName()
{
    CALL_STACK_MESSAGE1("CPackACArray::GetSelectedFullName()");
    // search all and return selected
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

// returns the number of already found packages (including the header)
int CPackACListView::GetCount()
{
    CALL_STACK_MESSAGE1("CPackACListView::GetCount()");
    // if the table is not present, we have 0 elements
    if (PackersTable == NULL)
        return 0;
    // iterate through all elements of the array
    int count = 0;
    int i;
    for (i = 0; i < PackersTable->Count; i++)
    {
        // and we will ask everyone how much they have, and we will also add a title
        count += PackersTable->At(i)->GetCount() + 1;
    }
    // return the result
    return count;
}

// 'toggle' the state of the checkbox of the given item
void CPackACListView::InvertSelect(int index)
{
    CALL_STACK_MESSAGE2("CPackACListView::InvertSelect(%d)", index);
    unsigned int archiver, arcIndex;
    // find the indexes of the selected archive
    if (PackersTable != NULL && !FindArchiver(index, &archiver, &arcIndex))
    {
        // 'toggle' the state of the checkbox with checking the others in the group
        PackersTable->At(archiver)->InvertSelect(arcIndex);
        // Redraw items that may have been affected by the change
        ListView_RedrawItems(HWindow, index - arcIndex,
                             index - arcIndex + PackersTable->At(archiver)->GetCount() - 1);
        // move the clicked item to the visible area
        ListView_EnsureVisible(HWindow, index, FALSE);
        // and redraw what is necessary
        UpdateWindow(HWindow);
    }
}

// returns the found archive according to the item from the listview
CPackACPacker*
CPackACListView::GetPacker(int item, int* index)
{
    CALL_STACK_MESSAGE2("CPackACListView::GetPacker(%d, )", item);
    // consistency check
    if (PackersTable == NULL)
        return NULL;
    unsigned int archiver, arcIndex;
    // let's try to find the archiver
    if (FindArchiver(item, &archiver, &arcIndex))
        // it's a heading
        *index = -1;
    else
        // it is an archiver
        *index = arcIndex;
    // return the requested archiver
    return PackersTable->At(archiver);
}

// searches for an archiver by index in the listview
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
    // finally we have data
    PackersTable = table;
    // set columns
    InitColumns();
    // set the initial number of items in the listview
    ListView_SetItemCount(HWindow, GetCount());
    // set focus to the first item in the listview
    ListView_SetItemState(HWindow, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
}

// set column headers and basic column width of the listview
BOOL CPackACListView::InitColumns()
{
    CALL_STACK_MESSAGE1("CPackACListView::InitColumns()");
    LV_COLUMN lvc;
    // table name
    int header[4] = {IDS_ACCOLUMN1, IDS_ACCOLUMN2, IDS_ACCOLUMN3, IDS_ACCOLUMN4};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    // create all columns
    int i;
    for (i = 0; i < 4; i++)
    {
        // FullName is aligned to the left, others to the right
        if (i == 0)
            lvc.fmt = LVCFMT_LEFT;
        else
            lvc.fmt = LVCFMT_RIGHT;
        // title
        lvc.pszText = LoadStr(header[i]);
        // column number
        lvc.iSubItem = i;
        // and we will create a column
        if (ListView_InsertColumn(HWindow, i, &lvc) == -1)
            return FALSE;
    }

    // set initial widths of columns for size, date, and time
    ListView_SetColumnWidth(HWindow, 3, ListView_GetStringWidth(HWindow, "00:00:00") + 20);
    ListView_SetColumnWidth(HWindow, 2, ListView_GetStringWidth(HWindow, "00.00.0000") + 20);
    ListView_SetColumnWidth(HWindow, 1, ListView_GetStringWidth(HWindow, "000 000") + 20);
    // calculate the column fullname
    SetColumnWidth();

    // turn on the checkboxes
    ListView_SetExtendedListViewStyle(HWindow, LVS_EX_CHECKBOXES);
    // and we hold the state of the checkbox, the windows call it
    ListView_SetCallbackMask(HWindow, LVIS_STATEIMAGEMASK);
    // done
    return TRUE;
}

// set the width of the columns in the listview according to the size of the window
void CPackACListView::SetColumnWidth()
{
    CALL_STACK_MESSAGE1("CPackACListView::SetColumnWidth()");
    RECT r;
    // we will determine the size we need to fit into
    GetClientRect(HWindow, &r);
    // width of everything
    DWORD cx = r.right - r.left - 1;
    // subtract the sizes of fixed columns (size, date, time)
    cx -= ListView_GetColumnWidth(HWindow, 1) + ListView_GetColumnWidth(HWindow, 2) +
          ListView_GetColumnWidth(HWindow, 3) - 1;
    // subtract the scrollbar
    DWORD style = (DWORD)GetWindowLongPtr(HWindow, GWL_STYLE);
    if (!(style & WS_VSCROLL))
        cx -= GetSystemMetrics(SM_CXHSCROLL);
    // and set the width of the variable column (fullname)
    ListView_SetColumnWidth(HWindow, 0, cx);
}

// check the found file, and if it is the archiver we want, add it to the Found array
BOOL CPackACListView::ConsiderItem(const char* path, const char* fileName, FILETIME lastWriteTime,
                                   const CQuadWord& size, EPackExeType type)
{
    CALL_STACK_MESSAGE4("CPackACListView::ConsiderItem(%s, %s, , , %d)", path, fileName, type);

    // consistency check
    if (PackersTable == NULL)
        return TRUE;
    // Initialization
    BOOL stop = FALSE;
    int totalCount = 0;
    // Iterate through all packages to see if any of them is the one being searched for
    int i;
    for (i = 0; i < PackersTable->Count; i++)
    {
        // take the packer
        CPackACPacker* item = PackersTable->At(i);
        // do we want him?
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
            // we added an item, let's redraw the entire category again (without the title)
            SendMessage(ACDialog->HWindow, WM_USER_ACADDFILE, (WPARAM)(totalCount + 1), 0);
            // We are not looking for another one anymore
            break;
        }
        // and we calculate the total count for the index in the listview
        totalCount += 1 + item->GetCount();
    }
    // an error occurred and we need to stop?
    return stop;
}

// Window procedure for listview with found archives
LRESULT
CPackACListView::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPackACListView::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_KEYDOWN:
    {
        // Handling space to toggle select/unselect checkbox
        if (wParam == VK_SPACE)
        {
            // find the current element
            int index = ListView_GetNextItem(HWindow, -1, LVNI_FOCUSED);
            // and if it's not a title, we'll change the checkbox
            if (index > -1)
                InvertSelect(index);
        }
        break;
    }
    case WM_LBUTTONDOWN:
    {
        // processing checkbox from mouse
        LV_HITTESTINFO htInfo;
        htInfo.pt.x = LOWORD(lParam);
        htInfo.pt.y = HIWORD(lParam);
        ListView_HitTest(HWindow, &htInfo);
        // if the checkbox was clicked, we will change its state
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

// returns the text that comes into the given column
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

// set the element to the desired values
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

// dialog procedure for selecting a disk to search
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
            // Release a copy of the disk list from memory
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
                    // remove leading spaces
                    while (*txt == ' ' || *txt == '\t')
                        txt++;
                    // find the length without trailing spaces
                    int len = (int)strlen(txt) - 1; // if txt is an empty string, len==-1
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
            /*              case EDTLBN_MOVEITEM:
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
            }*/
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
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow change
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
        // list of disks poured
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
            // calculate the size of the required space for the disk list
            for (i = 0; i < EditLB->GetCount(); i++)
            {
                INT_PTR itemID;
                EditLB->GetItemID(i, itemID);
                unsigned long pathlen = (unsigned long)strlen((char*)itemID);
                len += pathlen + 1;
                // if the path does not end with a backslash, I have to consider it
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
                // and I pull out the list
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

// starts auto-configuration
void PackAutoconfig(HWND parent)
{
    CALL_STACK_MESSAGE1("PackAutoconfig()");

    // stop refreshing in the main window
    BeginStopRefresh();
    // we will determine the necessary buffer for disks for searching
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
            // we will remove non-fixed disks...
            char* srcDrive = sysDrives;
            while (*srcDrive != '\0')
            {
                // What is it? Is it worth it?
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
            // open search dialog
            CPackACDialog(HLanguage, IDD_AUTOCONF, IDD_AUTOCONF, parent, &ArchiverConfig, &drives).Execute();
        }
        HANDLES(GlobalFree((HGLOBAL)drives));
    }
    // dialog closed, main window continues refreshing
    EndStopRefresh();
}
