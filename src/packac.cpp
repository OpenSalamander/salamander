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

// typ polozky v tabulce extenzi pakovacu
struct SPackAssocItem
{
    const char* Ext; // extenze pakovace
    int nextIndex;   // index dalsiho pakovace pro stejny archiv, -1 pokud jiny neexistuje
};

// tabluka extenzi asociaci pakovacu
// prvni polozka je string masek, druha je index dalsiho pakovace stejneho typu
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

// dialog procedura hlavniho dialogu
INT_PTR
CPackACDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CPackACDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // konstrukce listview
        ListView = new CPackACListView(this);
        if (ListView == NULL)
        {
            TRACE_E(LOW_MEMORY);
            PostMessage(HWindow, WM_COMMAND, IDCANCEL, 0);
            break;
        }
        // subclass listview
        ListView->AttachToControl(HWindow, IDC_ACLIST);
        // vytvorim status bar
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
        // tlacitko Stop/Restart bude tlacitko start
        SetDlgItemText(HWindow, IDB_ACSTOP, LoadStr(IDS_ACBUTTON_RESCAN));
        // zakazeme OK a povolime Drives
        EnableWindow(GetDlgItem(HWindow, IDB_ACDRIVES), TRUE);
        EnableWindow(GetDlgItem(HWindow, IDOK), FALSE);
        // tlacitko OK ted nebude default push button
        PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
        // tlacitko Start ted bude default push button
        PostMessage(HWindow, DM_SETDEFID, IDB_ACSTOP, NULL);
        // nactu parametry pro layoutovani okna
        GetLayoutParams();
        // layoutuji okno
        LayoutControls();
        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_ACSTOP:
        {
            // pokud hledame, jde o STOP tlacitko, jinak Rescan
            if (SearchRunning)
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_WANTTOSTOP), LoadStr(IDS_QUESTION),
                                  MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
                    return TRUE;
                // zastavime hledani na disku
                SetEvent(StopSearch);
                return TRUE;
            }
            else
            {
                // Restart tlacitko
                // tlacitko Stop/Restart bude zase tlacitko stop
                SetDlgItemText(HWindow, IDB_ACSTOP, LoadStr(IDS_ACBUTTON_STOP));
                // zakazeme OK a Drives
                EnableWindow(GetDlgItem(HWindow, IDB_ACDRIVES), FALSE);
                EnableWindow(GetDlgItem(HWindow, IDOK), FALSE);
                // tlacitko OK ted nebude default push button
                PostMessage(GetDlgItem(HWindow, IDOK), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
                // tlacitko STOP bude default push button
                HWND focus = GetFocus();
                PostMessage(HWindow, DM_SETDEFID, IDB_ACSTOP, NULL);
                if (focus != ListView->HWindow && focus != GetDlgItem(HWindow, IDC_ACMODCUSTOM))
                {
                    // pokud byl focus na nejakem z tlacitek, musime ho tam vratit
                    PostMessage(GetDlgItem(HWindow, IDB_ACSTOP), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
                    PostMessage(focus, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
                }
                // vytvorime znovu event na signalizaci preruseni hledani
                StopSearch = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
                // a spustime prohledavaci thread
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
            // pokud bezi druhy thread, ptame se...
            if (SearchRunning)
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_CANCELOPERATION), LoadStr(IDS_QUESTION),
                                  MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
                    return TRUE;
                // zastavime hledani na disku
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
                // zobrazime polozku a jeji stav (data si drzime my, ne listview)
                LV_DISPINFO* info = (LV_DISPINFO*)lParam;
                int index;
                CPackACPacker* packer = ListView->GetPacker(info->item.iItem, &index);
                // pokud byl pozadovan text, vytahneme ho
                if (info->item.mask & LVIF_TEXT)
                    info->item.pszText = (char*)packer->GetText(index, info->item.iSubItem);
                // pokud ikonka checkboxu, vytahnem i ji
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
        // minimalni velikost okna
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinDlgW;
        lpmmi->ptMinTrackSize.y = MinDlgH;
        break;
    }
    case WM_USER_ACADDFILE:
    {
        // vyhledavaci thread pridal soubor, musime prekreslit postizene misto
        UpdateListViewItems((int)wParam);
        return TRUE;
    }
    case WM_USER_ACERROR:
    {
        // mame problem, tak at o tom user vi...
        // jsme li ikonka, skaceme nahoru
        if (IsIconic(HWindow))
            ShowWindow(HWindow, SW_RESTORE);
        // a zobrazime error
        MessageBox(HWindow, (char*)wParam, LoadStr(IDS_ERRORFINDINGFILE), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }
    case WM_USER_ACSEARCHING:
    {
        // vyhledavaci thread nam poslal novou cestu, na ktere pracuje, tak urizneme slash na konci...
        if (((char*)wParam)[lstrlen((char*)wParam) - 1] == '\\')
            ((char*)wParam)[lstrlen((char*)wParam) - 1] = '\0';
        // a zobrazime ji
        SetDlgItemText(HWindow, IDC_ACSTATUS, (char*)wParam);
        // a uvolnime pamet...
        HANDLES(GlobalFree((HGLOBAL)wParam));
        return TRUE;
    }
    case WM_USER_ACFINDFINISHED:
    {
        // pockame na skutecne dokonceni vyhledavaciho threadu a uklidime
        if (HSearchThread != NULL)
        {
            WaitForSingleObject(HSearchThread, INFINITE);
            HANDLES(CloseHandle(HSearchThread));
            HSearchThread = NULL;
        }
        // uvolnime signalizacni event
        if (StopSearch != NULL)
        {
            HANDLES(CloseHandle(StopSearch));
            StopSearch = NULL;
        }
        // pokud thread skoncil, protoze zavirame okno, pokracuj se zaviranim
        if (WillExit)
            PostMessage(HWindow, WM_COMMAND, MAKELONG(IDCANCEL, 0), 0);
        else
        {
            // uvedeme vse do poradku
            SetDlgItemText(HWindow, IDB_ACSTOP, LoadStr(IDS_ACBUTTON_RESCAN));
            SetDlgItemText(HWindow, IDC_ACSTATUS, LoadStr(IDS_ACSTATUSDONE));
            EnableWindow(GetDlgItem(HWindow, IDB_ACDRIVES), TRUE);
            EnableWindow(GetDlgItem(HWindow, IDOK), TRUE);
            // opravime default push button
            HWND focus = GetFocus();
            PostMessage(HWindow, DM_SETDEFID, IDOK, NULL);
            if (focus != ListView->HWindow && focus != GetDlgItem(HWindow, IDC_ACMODCUSTOM))
            {
                // pokud byl focus na nejakem z tlacitek, musime ho tam vratit
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
    // default zpracovani messagi
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

// ulozi pocatecni rozmery dialogu pro resize
void CPackACDialog::GetLayoutParams()
{
    CALL_STACK_MESSAGE1("CPackACDialog::GetLayoutParams()");
    // minimalni rozmery dialogu
    RECT wr;
    GetWindowRect(HWindow, &wr);
    MinDlgW = wr.right - wr.left;
    MinDlgH = wr.bottom - wr.top;

    // klientska cast okna
    RECT cr;
    GetClientRect(HWindow, &cr);

    // pomocne rozmery pro prepocet non-client a client souradnic
    int windowMargin = ((wr.right - wr.left) - (cr.right)) / 2;
    int captionH = wr.bottom - wr.top - cr.bottom - windowMargin;

    // prostor vlevo a vpravo mezi rameckem dialogu a controly
    RECT br;
    GetWindowRect(GetDlgItem(HWindow, IDC_ACLIST), &br);
    HMargin = br.left - wr.left - windowMargin;

    // prostor dole mezi tlacitky a status barou
    VMargin = HMargin;

    // rozmery tlacitek
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

    // vyska status bary
    GetWindowRect(HStatusBar, &br);
    StatusHeight = br.bottom - br.top;

    // vyska checkboxu
    CheckH = br.bottom - br.top;

    // umisteni seznamu vysledku
    GetWindowRect(GetDlgItem(HWindow, IDC_ACLIST), &br);
    ListY = br.top - wr.top - captionH;
}

// resizuje okno dialogu
void CPackACDialog::LayoutControls()
{
    CALL_STACK_MESSAGE1("CPackACDialog::LayoutControls()");
    RECT clientRect;

    // vezmi velikost, co mame k dispozici
    GetClientRect(HWindow, &clientRect);
    clientRect.bottom -= StatusHeight;

    // a ted to sjedem v jednom bloku
    HDWP hdwp = HANDLES(BeginDeferWindowPos(8));
    if (hdwp != NULL)
    {
        // umistim Status Bar
        hdwp = HANDLES(DeferWindowPos(hdwp, HStatusBar, NULL,
                                      0, clientRect.bottom, clientRect.right, StatusHeight,
                                      SWP_NOZORDER));

        // umistim tlacitko Help
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDHELP), NULL,
                                      clientRect.right - ButtonW5 - HMargin,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // umistim tlacitko Cancel
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDCANCEL), NULL,
                                      clientRect.right - (ButtonW4 + ButtonW5) - HMargin - ButtonMargin,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // umistim tlacitko OK
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDOK), NULL,
                                      clientRect.right - (ButtonW3 + ButtonW4 + ButtonW5) - HMargin - ButtonMargin * 2,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // umistim tlacitko Stop/Rescan
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_ACSTOP), NULL,
                                      clientRect.right - (ButtonW2 + ButtonW3 + ButtonW4 + ButtonW5) - HMargin - ButtonMargin * 3,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // umistim tlacitko Drives
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_ACDRIVES), NULL,
                                      clientRect.right - (ButtonW1 + ButtonW2 + ButtonW3 + ButtonW4 + ButtonW5) - HMargin - ButtonMargin * 4,
                                      clientRect.bottom - ButtonH - VMargin,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // umitstim check-box Custom archivers
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_ACMODCUSTOM), NULL,
                                      HMargin, clientRect.bottom - VMargin - ButtonH / 2 - CheckH / 2,
                                      0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // umistim a natahnu list view
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_ACLIST), NULL,
                                      HMargin, ListY, clientRect.right - HMargin * 2,
                                      clientRect.bottom - ListY - ButtonH - VMargin * 2,
                                      SWP_NOZORDER));
        // blok hotov
        HANDLES(EndDeferWindowPos(hdwp));
    }
    // a upravime sirku sloupcu v listview
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

// vlastni vykonna (a rekurzivni :-) ) funkce pro prohledavani disku
BOOL CPackACDialog::DirectorySearch(char* path)
{
    CALL_STACK_MESSAGE2("CPackACDialog::DirectorySearch(%s)", path);

    // dame zpravu sefovi o tom, na cem delame
    DWORD pathLen = (DWORD)strlen(path);
    char* workName = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, pathLen + 1));
    if (workName == NULL)
    {
        SendMessage(HWindow, WM_USER_ACERROR, (WPARAM)LoadStr(IDS_PACKERR_NOMEM), NULL);
        TRACE_E(LOW_MEMORY);
        return TRUE;
    }
    strcpy(workName, path);
    // posleme nazev, uvolneni pameti udela adresat
    if (!PostMessage(HWindow, WM_USER_ACSEARCHING, (WPARAM)workName, 0))
        // pokud se to nepovedlo, kaslem na to...
        HANDLES(GlobalFree((HGLOBAL)workName));

    // alokujeme buffer pro masku hledani
    char* fileName = (char*)HANDLES(GlobalAlloc(GMEM_FIXED, pathLen + 3 + 1));
    if (fileName == NULL)
    {
        SendMessage(HWindow, WM_USER_ACERROR, (WPARAM)LoadStr(IDS_PACKERR_NOMEM), NULL);
        TRACE_E(LOW_MEMORY);
        return TRUE;
    }
    // pripravime si masku pro hledani
    strcpy(fileName, path);
    strcat(fileName, "*");

    // nastavime nejake promenne
    BOOL mustStop = FALSE;
    WIN32_FIND_DATA findData;
    // zkusime najit prvni soubor
    HANDLE fileFind = HANDLES_Q(FindFirstFile(fileName, &findData));
    if (fileFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            unsigned int nameLen = (unsigned int)strlen(findData.cFileName);
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                // je to soubor, ted zkontrolujem, jestli exac
                if (nameLen > 4 && pathLen + nameLen < MAX_PATH &&
                    (findData.cFileName[nameLen - 1] == 'e' || findData.cFileName[nameLen - 1] == 'E') &&
                    (findData.cFileName[nameLen - 2] == 'x' || findData.cFileName[nameLen - 2] == 'X') &&
                    (findData.cFileName[nameLen - 3] == 'e' || findData.cFileName[nameLen - 3] == 'E') &&
                    findData.cFileName[nameLen - 4] == '.')
                {
                    // zjistime typ programu
                    char fullName[MAX_PATH];
                    DWORD type;
                    strcpy(fullName, path);
                    strcat(fullName, findData.cFileName);

                    if (!MyGetBinaryType(fullName, &type))
                    {
                        TRACE_I("Invalid executable or error getting type: " << fullName);
                        continue;
                    }
                    // a koukneme se, jestli o nej stojime...
                    mustStop |= ListView->ConsiderItem(path, findData.cFileName,
                                                       findData.ftLastWriteTime,
                                                       CQuadWord(findData.nFileSizeLow,
                                                                 findData.nFileSizeHigh),
                                                       type == SCS_32BIT_BINARY ? EXE_32BIT : EXE_16BIT);
                }
            }
            else
            {
                // mame adresar - vyloucime '.' a '..'
                if (findData.cFileName[0] != 0 &&
                    (findData.cFileName[0] != '.' ||
                     (findData.cFileName[1] != '\0' &&
                      (findData.cFileName[1] != '.' || findData.cFileName[2] != '\0'))) &&
                    pathLen + 1 + nameLen < MAX_PATH)
                {
                    // vytvorime nazev adresare k prohledavani
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
                        // a rekurzivne ho prohledame
                        DirectorySearch(newPath);
                        HANDLES(GlobalFree((HGLOBAL)newPath));
                    }
                }
            }
            // zkontroluje, jestli neni signalizovano preruseni
            DWORD ret = WaitForSingleObject(StopSearch, 0);
            if (ret != WAIT_TIMEOUT)
                mustStop = TRUE;
        } while (FindNextFile(fileFind, &findData) && !mustStop);
        HANDLES(FindClose(fileFind));
    }
    HANDLES(GlobalFree((HGLOBAL)fileName));
    // a konec
    return mustStop;
}

// vyvolani rekurzivniho vyhledavani pro vsechny lokalni pevne disky
DWORD
CPackACDialog::DiskSearch()
{
    CALL_STACK_MESSAGE1("CPackACDialog::DiskSearch()");
    // inicializujeme trace pro novy thread
    SetThreadNameInVCAndTrace("AutoConfig");
    TRACE_I("Begin");

    // sanity checking
    if (DrivesList == NULL || *DrivesList == NULL || **DrivesList == '\0')
        TRACE_I("The list of drives is empty...");
    else
    {
        char* drive = *DrivesList;
        // a projedem vsechny drajvy co mame...
        BOOL mustStop = FALSE;
        while (*drive != '\0' && !mustStop)
        {
            // a jedeme z kopce...
            TRACE_I("Searching drive " << drive);
            mustStop = DirectorySearch(drive);
            // posunem se na dalsi v seznamu
            while (*drive != '\0')
                drive++;
            // a preskocime koncovy null
            drive++;
        }
    }
    // oznamime ukonceni hledani
    PostMessage(HWindow, WM_USER_ACFINDFINISHED, 0, 0);
    // uz nehledame
    SearchRunning = FALSE;
    TRACE_I("End");
    return 0;
}

// wraper pro vyhledavaci funkci s exception handlingem
unsigned int
CPackACDialog::PackACDiskSearchThreadEH(void* instance)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        // vyvolani vlastniho hledani
        return ((CPackACDialog*)instance)->DiskSearch();
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("PackACDiskSearchThreadEH: calling ExitProcess(1).");
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

// wrapper pro wrapper pro vyhledavani - tentokrat static funkce volana z CreateThread
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

    // pridame extenzi a mozna i custom packer, pokud neexistuje
    // pakovac je pridan, pokud neexistuje maska souboru odpovidajici nalezenemu pakovaci
    // pokud existuje a odkazuje na jiny nez na plugin nebo 32-bit, presmerujeme na nalezeny
    char buffer[10];
    const char* ptr = PackACExtensions[packerIndex].Ext;
    int found;
    do
    {
        // hledame, kdo nas pouziva
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
        // sestavili jsme "nazev archivu", ted jestli ho mame v extenzich...
        found = PackerFormatConfig.PackIsArchive(buffer);
        if (found != 0)
        {
            int pos;
            CPackACPacker* p = NULL;
            // pokud pracujeme s pakovacem, opravime zaznam pro pakovac
            if (foundPacker->GetPackerType() == Packer_Packer || foundPacker->GetPackerType() == Packer_Standalone)
            {
                // pokud jsme pouzivali pakovac, zjistime, jestli jsme ho nasli
                if (PackerFormatConfig.GetUsePacker(found - 1))
                {
                    pos = PackerFormatConfig.GetPackerIndex(found - 1);
                    // zjistime, jestli jsme ho nasli
                    int k;
                    for (k = 0; k < ListView->GetPackersCount(); k++)
                    {
                        p = ListView->GetPacker(k);
                        if (p->GetArchiverIndex() == pos && p->GetPackerType() != Packer_Unpacker)
                            break;
                    }
                }
                // pakovac nahradime, pokud:
                // 1) pakovac jsme vubec nepouzivali
                // 2) nebo pouzivali jsme externi pakovac, jiny nez ktery kontrolujem a
                // 3) puvodni pakovac nebyl nalezen nebo mam 32bitovy (jsme lepsi)
                // pokud jsme packer predtim nepouzivali, nahodime ho
                if (!PackerFormatConfig.GetUsePacker(found - 1) ||
                    (pos >= 0 && pos != packerIndex &&
                     (p == NULL || p->GetSelectedFullName() == NULL || foundPacker->GetExeType() == EXE_32BIT)))
                {
                    TRACE_I("Setting packer for extension " << PackACExtensions[packerIndex].Ext << " to the new one.");
                    PackerFormatConfig.SetPackerIndex(found - 1, packerIndex);
                    PackerFormatConfig.SetUsePacker(found - 1, TRUE);
                    // a obnovime tabulku
                    PackerFormatConfig.BuildArray();
                }
            }
            // pokud pracujeme s rozpakovavacem, upravime zaznam pro rozpakovavac
            if (foundPacker->GetPackerType() == Packer_Unpacker || foundPacker->GetPackerType() == Packer_Standalone)
            {
                // pokud jsme nasli, zkontrolujem nastaveni
                pos = PackerFormatConfig.GetUnpackerIndex(found - 1);
                // zjistime, jestli jsme stavajici unpacker nasli
                int k;
                for (k = 0; k < ListView->GetPackersCount(); k++)
                {
                    p = ListView->GetPacker(k);
                    if (p->GetArchiverIndex() == pos && p->GetPackerType() != Packer_Packer)
                        break;
                }
                // pokud stavajici pakovac neni plugin, nebo jsme ho nenasli, nebo je jiny a my mame 32bit
                if (pos >= 0 && pos != packerIndex &&
                    (p == NULL || p->GetSelectedFullName() == NULL || foundPacker->GetExeType() == EXE_32BIT))
                {
                    TRACE_I("Changing unpacker for extension " << PackACExtensions[packerIndex].Ext << " to the new one.");
                    PackerFormatConfig.SetUnpackerIndex(found - 1, packerIndex);
                    // a obnovime tabulku
                    PackerFormatConfig.BuildArray();
                }
            }
        }
    } while (*ptr != '\0');
    // prohledali jsme vsechny extenze, a pokud jsme nenasli, musime pridat
    if (found == 0)
    {
        // pokud mame jen packer, pak musime mit i unpacker (plugin bychom uz nasli)
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

    // vyhodime asociaci z extenzi
    // asociace je vyhozena, jestlize viewer nebyl nalezen nebo byl nalezen, ale nebyl vybran (fullName == NULL)
    // pakovac je prepnut na --not supported--, pokud packer nebyl nalezen nebo byl nalezen, ale nebyl vybran (fullName == NULL)

    // prohledame extenze
    int i;
    for (i = PackerFormatConfig.GetFormatsCount() - 1; i >= 0; i--)
    {
        // pokud nas asociace pouziva jako viewer, zkusime prehodit na jiny, jinak rezem
        if (packerIndex == PackerFormatConfig.GetUnpackerIndex(i))
        {
            // najdeme alternativu
            CPackACPacker* p = NULL;
            if (PackACExtensions[packerIndex].nextIndex >= 0)
                p = ListView->GetPacker(PackACExtensions[packerIndex].nextIndex);
            // pokud je to jen packer (zip), vezmeme unpacker
            if (p != NULL && p->GetPackerType() == Packer_Packer)
                p = ListView->GetPacker(PackACExtensions[packerIndex].nextIndex + 1);
            if (p == NULL || p->GetSelectedFullName() == NULL)
            {
                // pokud neni alternativa, rezem
                TRACE_I("Removing extensions " << PackerFormatConfig.GetExt(i));
                PackerFormatConfig.DeleteFormat(i);
                PackerFormatConfig.BuildArray();
            }
            else
            {
                // mame alternativu, kterou jsme nasli, prehodime
                TRACE_I("Changing viewer for extensions " << PackerFormatConfig.GetExt(i) << " to another.");
                PackerFormatConfig.SetUnpackerIndex(i, p->GetArchiverIndex());
                PackerFormatConfig.BuildArray();
            }
        }
        else
        {
            // pokud nas asociace pouziva jen jako editor, zkusime presmerovat, jinak nastavime na --not supported--
            if (PackerFormatConfig.GetUsePacker(i) && PackerFormatConfig.GetPackerIndex(i) == packerIndex)
            {
                CPackACPacker* p = NULL;
                if (PackACExtensions[packerIndex].nextIndex >= 0)
                    p = ListView->GetPacker(PackACExtensions[packerIndex].nextIndex);
                if (p == NULL || p->GetSelectedFullName() == NULL)
                {
                    // alternativa neexistuje
                    TRACE_I("Setting packer to --not supported-- for extension " << PackerFormatConfig.GetExt(i));
                    // jiny unpacker je tu, nahodime pro packer --not supported--
                    PackerFormatConfig.SetUsePacker(i, FALSE);
                    PackerFormatConfig.BuildArray();
                }
                else
                {
                    // mame alternativu, kterou jsme nasli, prehodime
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

    // pridame custom packery pro nove nalezeny pakovac
    char variable[50];
    int i;
    BOOL found1 = FALSE, found2 = FALSE;
    sprintf(variable, "$(%s)", ArchiverConfig->GetPackerVariable(packerIndex));
    // prohledame custom packers, jestli uz tam nahodou neni
    for (i = 0; i < PackerConfig.GetPackersCount(); i++)
    {
        // bereme jen externi
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

    // pridame custom unpackery pro nove nalezeny pakovac
    if (!ArchiverConfig->ArchiverExesAreSame(packerIndex))
        sprintf(variable, "$(%s)", ArchiverConfig->GetUnpackerVariable(packerIndex));
    found1 = FALSE;
    // prohledame custom packers, jestli uz tam nahodou neni
    for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
    {
        // bereme jen externi
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

    // custom packer/unpacker je vyhozen, jestlize:
    //   1) pakovac nebyl nalezen nebo byl nalezen, ale nebyl vybran (fullName == NULL)
    //   2) volany program je promenna odpovidajici tomuto pakovaci

    CPackACPacker* p = NULL;
    char variable[50];
    sprintf(variable, "$(%s)", ArchiverConfig->GetPackerVariable(packerIndex));
    int i;
    // prohledame custom packers (odzadu, abychom mohli odmazavat)
    for (i = PackerConfig.GetPackersCount() - 1; i >= 0; i--)
        // bereme jen externi
        if (PackerConfig.GetPackerType(i) >= 0)
        {
            const char* cmd = PackerConfig.GetPackerCmdExecCopy(i);
            if (!strcmp(cmd, variable))
            {
                // tenhle pakovac vola program, ktery jsme nenasli - podriznout...
                TRACE_I("Removing custom packer which uses not found packer: " << variable);
                PackerConfig.DeletePacker(i);
                // na move cmd uz se nemusime kouka, packer uz neexistuje
            }
            else if (PackerConfig.GetPackerSupMove(i))
            {
                cmd = PackerConfig.GetPackerCmdExecMove(i);
                if (!strcmp(cmd, variable))
                {
                    // vola na move cmd program, ktery neexistuje - vypnout
                    TRACE_I("Disabling Move command for custom packer which uses not found packer: " << variable);
                    PackerConfig.SetPackerSupMove(i, FALSE);
                }
            }
        }
    if (!ArchiverConfig->ArchiverExesAreSame(packerIndex))
        sprintf(variable, "$(%s)", ArchiverConfig->GetUnpackerVariable(packerIndex));
    // prohledame custom unpackers
    for (i = UnpackerConfig.GetUnpackersCount() - 1; i >= 0; i--)
        // bereme jen externi
        if (UnpackerConfig.GetUnpackerType(i) >= 0)
        {
            const char* cmd = UnpackerConfig.GetUnpackerCmdExecExtract(i);
            if (!strcmp(cmd, variable))
            {
                // tenhle rozpakovavac vola program, ktery jsme nenasli - podriznout...
                TRACE_I("Removing custom unpacker which uses not found packer: " << variable);
                UnpackerConfig.DeleteUnpacker(i);
            }
        }
}

// funkce volana pred startem a po ukonceni dialogu pro transfer dat
void CPackACDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPackACDialog::Transfer()");
    // zaciname nebo koncime ?
    if (ti.Type == ttDataToWindow)
    {
        // vytvorime tabulku hledanych pakovacu
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
        // soupneme ji do listview
        ListView->Initialize(table);
        // checkbox defaultne zacheckovany
        int value = 1;
        ti.CheckBox(IDC_ACMODCUSTOM, value);
        // ted nehledame
        SearchRunning = FALSE;
    }
    else
    {
        int doCustom;
        // zjistime hodnotu checkboxu
        ti.CheckBox(IDC_ACMODCUSTOM, doCustom);
        int i;
        // projedem vsechny pakovace - nakonfigurujeme cesty a vyhodime asociace tem, ktere jsme nenasli
        for (i = 0; i < ListView->GetPackersCount(); i++)
        {
            // rekneme si o pakovac
            CPackACPacker* packer = ListView->GetPacker(i);
            int index = packer->GetArchiverIndex();
            const char* fullName = packer->GetSelectedFullName();
            // pokud jsme ho nenasli, nechamu puvodni hodnotu
            if (fullName != NULL)
            {
                // ulozime cestu do konfigurace
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
        // projedem vsechny pakovace znovu, tentokrat jen pridame nove asociace
        for (i = 0; i < ListView->GetPackersCount(); i++)
        {
            // rekneme si o pakovac
            CPackACPacker* packer = ListView->GetPacker(i);
            int index = packer->GetArchiverIndex();
            const char* fullName = packer->GetSelectedFullName();
            // pokud jsme ho nenasli, nechamu puvodni hodnotu
            if (fullName != NULL)
            {
                AddToExtensions(i, index, packer);
                if (doCustom)
                    AddToCustom(i, index, packer);
            }
        }
    }
}

// vyridi pozadavek na pridani polozky do listview
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

// vraci pocet pakovacu v arrayi
int CPackACPacker::GetCount()
{
    SLOW_CALL_STACK_MESSAGE1("CPackACPacker::GetCount()");
    int count;
    HANDLES(EnterCriticalSection(&FoundDataCriticalSection));
    count = Found.Count;
    HANDLES(LeaveCriticalSection(&FoundDataCriticalSection));
    return count;
}

// otoci oznaceni souboru (inverze checkboxu)
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

// vrati text patrici do daneho sloupce
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

// vrati zda je polozka vybrana ci ne (ci jde o nadpis)
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
            // je vybrana
            ret = 2;
        else
            // neni vybrana
            ret = 1;
    }
    else
        // nadpis
        ret = 0;
    return ret;
}

// zjisti, zda stojime o dany program a pokud ano, prida ho
int CPackACPacker::CheckAndInsert(const char* path, const char* fileName, FILETIME lastWriteTime,
                                  const CQuadWord& size, EPackExeType exeType)
{
    CALL_STACK_MESSAGE3("CPackACPacker::CheckAndInsert(%s, %s, , , )", path, fileName);
    const char* ref = Name;
    const char* act = fileName;
    // sedi jmeno ?
    while (*ref != '\0' && *act != '\0' && tolower(*ref) == tolower(*act))
    {
        ref++;
        act++;
    }
    // priponu jsme uz zkontrolovali, ted jen jestli jsme na konci stringu
    // a jestli sedi ostatni pozadavky (zatim jen typ, uvidime v budoucnu...)
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
        // mame novou nalezenou polozku, tak zkontrolujem, jestli je pro nas nova
        int i;
        for (i = 0; i < Found.Count; i++)
        {
            char* n2 = Found.At(i)->FullName;
            // pokud uz ji mame, navrat
            if (!strcmp(fullName, n2))
            {
                free(fullName);
                return 0;
            }
        }
        // a pridame ji
        CPackACFound* newItem = new CPackACFound;
        if (newItem != NULL)
        {
            // zinicializujeme ji
            BOOL good = newItem->Set(fullName, size, lastWriteTime);
            free(fullName);
            int index;
            if (good)
            {
                HANDLES(EnterCriticalSection(&FoundDataCriticalSection));
                // a pridame do pole
                index = Found.AddAndCheck(newItem);
                if (!Found.IsGood())
                {
                    Found.ResetState();
                    good = FALSE;
                }
                HANDLES(LeaveCriticalSection(&FoundDataCriticalSection));
            }
            // ted uz jen kontrola, jak jsme dopadli
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

// zajisti vyselekteni pouze nejnovejsi polozky a prida ji do pole
int CPackACArray::AddAndCheck(CPackACFound* member)
{
    CALL_STACK_MESSAGE1("CPackACArray::AddAndCheck()");
    // je-li prvni, bude vybrana
    if (Count == 0)
        member->Selected = TRUE;
    else
    {
        int i;
        for (i = 0; i < Count; i++)
            // porovname s polozkou v soucasnosti vybranou
            if (At(i)->Selected && CompareFileTime(&At(i)->LastWrite, &member->LastWrite) == -1)
            {
                // pokud je novejsi, nahradime vyber
                At(i)->Selected = FALSE;
                member->Selected = TRUE;
            }
    }
    // a vlozime do pole
    return TIndirectArray<CPackACFound>::Add(member);
}

// otoci stav checkboxu polozky
void CPackACArray::InvertSelect(int index)
{
    CALL_STACK_MESSAGE2("CPackACArray::InvertSelect(%d)", index);
    // musime uzivateli umoznit nevybrat nic
    if (At(index)->Selected)
        At(index)->Selected = FALSE;
    else
    {
        // ale vybrana muze byt jen jedna polozka
        int i;
        for (i = 0; i < Count; i++)
            At(i)->Selected = (i == index);
    }
}

// vrati FullName polozky, ktera byla vybrana
const char*
CPackACArray::GetSelectedFullName()
{
    CALL_STACK_MESSAGE1("CPackACArray::GetSelectedFullName()");
    // prohledej vsechny a vrat vybranou
    int i;
    for (i = 0; i < Count; i++)
        if (At(i)->Selected)
            return At(i)->FullName;
    // pokud neni vybrana zadna, vrat NULL
    return NULL;
}

//****************************************************************************
//
// CPackACListView
//

// vraci pocet jiz nalezenych pakovacu (vcetne nadpisu)
int CPackACListView::GetCount()
{
    CALL_STACK_MESSAGE1("CPackACListView::GetCount()");
    // pokud tabulka neni, mame 0 prvku
    if (PackersTable == NULL)
        return 0;
    // projedem vsechny prvky tabulky
    int count = 0;
    int i;
    for (i = 0; i < PackersTable->Count; i++)
    {
        // a kazdeho se zeptame, kolik toho ma, a pricteme i nadpis
        count += PackersTable->At(i)->GetCount() + 1;
    }
    // vratime vysledek
    return count;
}

// 'otoci' stav checkboxu dane polozky
void CPackACListView::InvertSelect(int index)
{
    CALL_STACK_MESSAGE2("CPackACListView::InvertSelect(%d)", index);
    unsigned int archiver, arcIndex;
    // zjistime indexy vybraneho archiveru
    if (PackersTable != NULL && !FindArchiver(index, &archiver, &arcIndex))
    {
        // 'otocime' stav checkboxu s kontrolou ostatnich ve skupine
        PackersTable->At(archiver)->InvertSelect(arcIndex);
        // prekreslime polozky, kterych se zmena mohla dotknout
        ListView_RedrawItems(HWindow, index - arcIndex,
                             index - arcIndex + PackersTable->At(archiver)->GetCount() - 1);
        // posuneme cvaknutou polozku do visitelne oblasti
        ListView_EnsureVisible(HWindow, index, FALSE);
        // a prekreslime, co je nutne
        UpdateWindow(HWindow);
    }
}

// vrati nalezeny archiver podle polozky z listview
CPackACPacker*
CPackACListView::GetPacker(int item, int* index)
{
    CALL_STACK_MESSAGE2("CPackACListView::GetPacker(%d, )", item);
    // kontrola konzistence
    if (PackersTable == NULL)
        return NULL;
    unsigned int archiver, arcIndex;
    // zkusime najit archiver
    if (FindArchiver(item, &archiver, &arcIndex))
        // je to nadpis
        *index = -1;
    else
        // je to archiver
        *index = arcIndex;
    // vratime pozadovany archiver
    return PackersTable->At(archiver);
}

// hleda archiver podle indexu v listview
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

// zinicializuje list view
void CPackACListView::Initialize(APackACPackersTable* table)
{
    CALL_STACK_MESSAGE1("CPackACListView::Initialize()");
    // konecne mame data
    PackersTable = table;
    // nastavim sloupce
    InitColumns();
    // nastavim pocatecni pocet prvku v listview
    ListView_SetItemCount(HWindow, GetCount());
    // nastavim focus na prvni polozku listview
    ListView_SetItemState(HWindow, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
}

// nastavi nadpisy a zakladni sirku sloupcu listview
BOOL CPackACListView::InitColumns()
{
    CALL_STACK_MESSAGE1("CPackACListView::InitColumns()");
    LV_COLUMN lvc;
    // tabulka nazvu
    int header[4] = {IDS_ACCOLUMN1, IDS_ACCOLUMN2, IDS_ACCOLUMN3, IDS_ACCOLUMN4};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    // vytvorim vsechny sloupce
    int i;
    for (i = 0; i < 4; i++)
    {
        // FullName je zarovnan vlevo, ostatni vpravo
        if (i == 0)
            lvc.fmt = LVCFMT_LEFT;
        else
            lvc.fmt = LVCFMT_RIGHT;
        // nadpis
        lvc.pszText = LoadStr(header[i]);
        // cislo sloupce
        lvc.iSubItem = i;
        // a vytvorime sloupec
        if (ListView_InsertColumn(HWindow, i, &lvc) == -1)
            return FALSE;
    }

    // nastavime inicialni sirky sloupcu velikosti, data a casu
    ListView_SetColumnWidth(HWindow, 3, ListView_GetStringWidth(HWindow, "00:00:00") + 20);
    ListView_SetColumnWidth(HWindow, 2, ListView_GetStringWidth(HWindow, "00.00.0000") + 20);
    ListView_SetColumnWidth(HWindow, 1, ListView_GetStringWidth(HWindow, "000 000") + 20);
    // dopocitame sloupec fullname
    SetColumnWidth();

    // zapneme checkboxy
    ListView_SetExtendedListViewStyle(HWindow, LVS_EX_CHECKBOXES);
    // a stav checkboxu drzime my, wokna si o nej rikaji
    ListView_SetCallbackMask(HWindow, LVIS_STATEIMAGEMASK);
    // done
    return TRUE;
}

// nastavi sirku sloupcu v listview podle velikosti okna
void CPackACListView::SetColumnWidth()
{
    CALL_STACK_MESSAGE1("CPackACListView::SetColumnWidth()");
    RECT r;
    // zjistime velikost, do ktere se musime vejit
    GetClientRect(HWindow, &r);
    // sirka vseho
    DWORD cx = r.right - r.left - 1;
    // odecteme velikosti fixnich sloupcu (velikost, datum, cas)
    cx -= ListView_GetColumnWidth(HWindow, 1) + ListView_GetColumnWidth(HWindow, 2) +
          ListView_GetColumnWidth(HWindow, 3) - 1;
    // odecteme scrollbar
    DWORD style = (DWORD)GetWindowLongPtr(HWindow, GWL_STYLE);
    if (!(style & WS_VSCROLL))
        cx -= GetSystemMetrics(SM_CXHSCROLL);
    // a nastavime sirku variabilniho sloupce (fullname)
    ListView_SetColumnWidth(HWindow, 0, cx);
}

// zkontroluj nalezeny soubor, a pokud je to archiver, ktery chceme, pridej ho do pole Found
BOOL CPackACListView::ConsiderItem(const char* path, const char* fileName, FILETIME lastWriteTime,
                                   const CQuadWord& size, EPackExeType type)
{
    CALL_STACK_MESSAGE4("CPackACListView::ConsiderItem(%s, %s, , , %d)", path, fileName, type);

    // kontrola konzistence
    if (PackersTable == NULL)
        return TRUE;
    // inicializace
    BOOL stop = FALSE;
    int totalCount = 0;
    // projedem vsechny pakovace, jestli to neni nektery z hledanych
    int i;
    for (i = 0; i < PackersTable->Count; i++)
    {
        // vezmem pakovac
        CPackACPacker* item = PackersTable->At(i);
        // chceme ho ?
        int ret = item->CheckAndInsert(path, fileName, lastWriteTime, size, type);
        if (ret < 0)
        {
            // nejaky problem s arrayem
            SendMessage(ACDialog->HWindow, WM_USER_ACERROR, (WPARAM)LoadStr(IDS_CANTSHOWRESULTS), NULL);
            TRACE_E("Problems with array detected.");
            stop = TRUE;
        }
        else if (ret > 0)
        {
            TRACE_I("Packer " << fileName << " in location " << path << " found and added");
            // pridali jsme polozku, prekreslime od zacatku kategorie dal (bez nadpisu)
            SendMessage(ACDialog->HWindow, WM_USER_ACADDFILE, (WPARAM)(totalCount + 1), 0);
            // dalsi uz nehledame
            break;
        }
        // a pocitame celkovy pocet kvuli indexu v listview
        totalCount += 1 + item->GetCount();
    }
    // nastala chyba a musime zastavit ?
    return stop;
}

// Window procedura pro listview s nalezenymi archivery
LRESULT
CPackACListView::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPackACListView::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_KEYDOWN:
    {
        // zpracovani mezery, aby delala select/unselect checkboxu
        if (wParam == VK_SPACE)
        {
            // najdeme aktualni prvek
            int index = ListView_GetNextItem(HWindow, -1, LVNI_FOCUSED);
            // a pokud to neni titulek, zmenime checkbox
            if (index > -1)
                InvertSelect(index);
        }
        break;
    }
    case WM_LBUTTONDOWN:
    {
        // zpracovani checkboxu od mysi
        LV_HITTESTINFO htInfo;
        htInfo.pt.x = LOWORD(lParam);
        htInfo.pt.y = HIWORD(lParam);
        ListView_HitTest(HWindow, &htInfo);
        // pokud bylo kliknuto do checkboxu, zmenime jeho stav
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

// vrati text ktery prijde do daneho sloupce
char* CPackACFound::GetText(int column)
{
    CALL_STACK_MESSAGE2("CPackACFound::GetText(%d)", column);
    static char text[100];
    switch (column)
    {
    // Jmeno
    case 0:
        return FullName;
    // Velikost
    case 1:
    {
        NumberToStr(text, Size);
        return text;
    }
    // Datum
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
    // Cas
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

// nastavi prvek na pozadovane hodnoty
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

// dialog procedura dialogu pro vyber disku k prohledani
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
            // uvolnim z pameti kopii seznamu disku
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
                    // odrizneme pocatecni mezery
                    while (*txt == ' ' || *txt == '\t')
                        txt++;
                    // zjistime delku bez koncovych mezer
                    int len = (int)strlen(txt) - 1; // pokud je txt prazdny retezec, bude len==-1
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
              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // povolim prohozeni
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
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // povolime zmenu
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                Dirty = TRUE;
                free((char*)dispInfo->ItemID);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // povolim smazani
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
    // default zpracovani messagi
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
        // naleju seznam disku
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
            // zjistim velikost potrebneho mista pro seznam disku
            for (i = 0; i < EditLB->GetCount(); i++)
            {
                INT_PTR itemID;
                EditLB->GetItemID(i, itemID);
                unsigned long pathlen = (unsigned long)strlen((char*)itemID);
                len += pathlen + 1;
                // pokud cesta nekonci backslashem, musim s nim pocitat
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
                // a vytahnu seznam ven
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

// spousti autokonfiguraci
void PackAutoconfig(HWND parent)
{
    CALL_STACK_MESSAGE1("PackAutoconfig()");

    // zastavime refreshe v hlavnim okne
    BeginStopRefresh();
    // zjistime potrbny buffer pro disky k prohledavani
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
            // vyhazime non-fixed disky...
            char* srcDrive = sysDrives;
            while (*srcDrive != '\0')
            {
                // co je zac ? stoji nam za to ?
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
            // otevreme vyhledavaci dialog
            CPackACDialog(HLanguage, IDD_AUTOCONF, IDD_AUTOCONF, parent, &ArchiverConfig, &drives).Execute();
        }
        HANDLES(GlobalFree((HGLOBAL)drives));
    }
    // dialog zavren, hlavni okno pokracuje v obnovovani
    EndStopRefresh();
}
