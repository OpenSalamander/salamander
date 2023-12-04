// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CSimpleDlgControlWindow
//

LRESULT
CSimpleDlgControlWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN && HandleKeys)
    {
        switch (wParam)
        {
        case VK_ESCAPE:
        case VK_RETURN:
        {
            PostMessage(GetParent(HWindow), WM_COMMAND, wParam == VK_RETURN ? IDOK : IDCANCEL, 0);
            break;
        }

        case VK_TAB: // TAB / Shift+TAB
        {
            PostMessage(GetParent(HWindow), WM_NEXTDLGCTL,
                        (GetKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE, FALSE); // focus next / previous
            break;
        }
        }
    }
    else
    {
        if (uMsg == WM_GETDLGCODE) // postarame se, aby nam porad neoznacovali text v editboxu
        {
            LRESULT ret = CWindow::WindowProc(uMsg, wParam, lParam);
            return (ret & (~DLGC_HASSETSEL));
        }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CWelcomeMsgDlg
//

CWelcomeMsgDlg::CWelcomeMsgDlg(HWND parent, const char* text, BOOL serverReply,
                               const char* sentCommand, BOOL rawListing, int textSize)
    : CCenteredDialog(HLanguage, IDD_WELCOMEMSGDLG, parent)
{
    Text = text;
    TextSize = textSize;
    SizeBox = NULL;
    ServerReply = serverReply;
    RawListing = rawListing;
    SentCommand = ServerReply ? sentCommand : NULL;

    MinDlgHeight = 0;
    MinDlgWidth = 0;
    EditBorderWidth = 0;
    EditBorderHeight = 0;
    ButtonWidth = 0;
    ButtonBottomBorder = 0;
    SizeBoxWidth = 0;
    SizeBoxHeight = 0;
    SaveAsButtonWidth = 0;
    SaveAsButtonOffset = 0;
}

INT_PTR
CWelcomeMsgDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CWelcomeMsgDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (ServerReply)
        {
            if (SentCommand == NULL)
                TRACE_E("Unexpected situation in CWelcomeMsgDlg::DialogProc().");
            char buf[300];
            _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_SERVERREPLYTITLE), (SentCommand == NULL ? "" : SentCommand));
            SetWindowText(HWindow, buf);
        }
        if (RawListing)
            SetWindowText(HWindow, LoadStr(IDS_RAWLISTINGTITLE));
        else
            ShowWindow(GetDlgItem(HWindow, IDB_SAVEMSGAS), SW_HIDE);

        if (FixedFont != NULL)
            SendDlgItemMessage(HWindow, IDE_WELCOMEMSG, WM_SETFONT, (WPARAM)FixedFont, TRUE);
        if (TextSize == -1)
            SetDlgItemText(HWindow, IDE_WELCOMEMSG, Text);
        else
        {
            char* t = (char*)malloc(TextSize + 1); // neda se nic delat, musime to naalokovat i s koncovou nulu
            if (t != NULL)
            {
                memcpy(t, Text, TextSize);
                t[TextSize] = 0;
                SetDlgItemText(HWindow, IDE_WELCOMEMSG, t);
                free(t);
            }
        }

        // nechame i u ServerReply a RawListing -> potlacuje se tak oznaceni v editboxu
        CSimpleDlgControlWindow* wnd = new CSimpleDlgControlWindow(HWindow, IDE_WELCOMEMSG,
                                                                   !ServerReply && !RawListing);
        if (wnd != NULL && wnd->HWindow == NULL)
            delete wnd; // nepodarilo se attachnout - nedealokuje se samo

        if (!ServerReply && !RawListing)
        {
            wnd = new CSimpleDlgControlWindow(HWindow, IDOK);
            if (wnd != NULL && wnd->HWindow == NULL)
                delete wnd; // nepodarilo se attachnout - nedealokuje se samo
        }

        RECT r1, r2, r3;
        GetWindowRect(HWindow, &r1);
        GetClientRect(GetDlgItem(HWindow, IDE_WELCOMEMSG), &r2);
        MinDlgHeight = r1.bottom - r1.top - r2.bottom; // edit nechame zmensit na 0 bodu client area
        GetWindowRect(GetDlgItem(HWindow, IDOK), &r3);
        ButtonWidth = r3.right - r3.left;
        MinDlgWidth = r1.right - r1.left - r2.right + r3.right - r3.left; // do client area editu nechame vejit aspon OK button

        if (RawListing)
        {
            GetWindowRect(GetDlgItem(HWindow, IDB_SAVEMSGAS), &r1);
            SaveAsButtonWidth = r1.right - r1.left;
            SaveAsButtonOffset = r1.left - r3.right;
            MinDlgWidth += SaveAsButtonWidth + SaveAsButtonOffset; // nechame vejit jeste button "save as"
        }
        else
            SaveAsButtonWidth = SaveAsButtonOffset = 0;

        GetClientRect(HWindow, &r1);
        POINT p;
        p.x = r3.left;
        p.y = r3.top;
        ScreenToClient(HWindow, &p);
        ButtonBottomBorder = r1.bottom - p.y;
        GetWindowRect(GetDlgItem(HWindow, IDE_WELCOMEMSG), &r2);
        EditBorderWidth = r1.right - (r2.right - r2.left);
        EditBorderHeight = r1.bottom - (r2.bottom - r2.top);

        // do praveho spodniho ruzku vlozime znacek pro resize okna
        SizeBox = CreateWindowEx(0,
                                 "scrollbar",
                                 "",
                                 WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE |
                                     WS_GROUP | SBS_SIZEBOX | SBS_SIZEGRIP | SBS_SIZEBOXBOTTOMRIGHTALIGN,
                                 0, 0, r1.right, r1.bottom,
                                 HWindow,
                                 NULL,
                                 DLLInstance,
                                 NULL);
        GetClientRect(SizeBox, &r2);
        SizeBoxWidth = r2.right;
        SizeBoxHeight = r2.bottom;
        // break;  // tady byt nema -> pokracujeme do WM_SIZE
    }
    case WM_SIZE:
    {
        RECT clientRect;
        GetClientRect(HWindow, &clientRect);
        HWND edit = GetDlgItem(HWindow, IDE_WELCOMEMSG);
        HWND button = GetDlgItem(HWindow, IDOK);
        HWND saveAs = GetDlgItem(HWindow, IDB_SAVEMSGAS);

        HDWP hdwp = HANDLES(BeginDeferWindowPos(5));
        if (hdwp != NULL)
        {
            hdwp = HANDLES(DeferWindowPos(hdwp, edit, NULL,
                                          0, 0, clientRect.right - EditBorderWidth, clientRect.bottom - EditBorderHeight,
                                          SWP_NOZORDER | SWP_NOMOVE));

            if (RawListing)
            {
                hdwp = HANDLES(DeferWindowPos(hdwp, button, NULL,
                                              (clientRect.right - ButtonWidth - SaveAsButtonWidth - SaveAsButtonOffset) / 2, clientRect.bottom - ButtonBottomBorder, 0, 0,
                                              SWP_NOZORDER | SWP_NOSIZE));
                hdwp = HANDLES(DeferWindowPos(hdwp, saveAs, NULL,
                                              (clientRect.right - ButtonWidth + SaveAsButtonWidth + SaveAsButtonOffset) / 2, clientRect.bottom - ButtonBottomBorder, 0, 0,
                                              SWP_NOZORDER | SWP_NOSIZE));
            }
            else
            {
                hdwp = HANDLES(DeferWindowPos(hdwp, button, NULL,
                                              (clientRect.right - ButtonWidth) / 2, clientRect.bottom - ButtonBottomBorder, 0, 0,
                                              SWP_NOZORDER | SWP_NOSIZE));
            }

            if (SizeBox != NULL)
            {
                hdwp = HANDLES(DeferWindowPos(hdwp, SizeBox, NULL, clientRect.right - SizeBoxWidth,
                                              clientRect.bottom - SizeBoxHeight, SizeBoxWidth, SizeBoxHeight, SWP_NOZORDER));
                if (uMsg != WM_INITDIALOG)
                {
                    // pry nejde show/hide kombinovat se zmenou velikosti a posunem
                    hdwp = HANDLES(DeferWindowPos(hdwp, SizeBox, NULL, 0, 0, 0, 0,
                                                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | (wParam == SIZE_RESTORED ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)));
                }
            }

            HANDLES(EndDeferWindowPos(hdwp));
        }
        break; // jak pro WM_INITDIALOG, tak pro WM_SIZE
    }

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinDlgWidth;
        lpmmi->ptMinTrackSize.y = MinDlgHeight;
        break;
    }

    case WM_COMMAND:
    {
        if ((LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) &&
            !Modal && !IsWindowEnabled(Parent) && SalamanderGeneral->GetMainWindowHWND() == Parent)
        {
            SetForegroundWindow(Parent);
        }
        if (LOWORD(wParam) == IDB_SAVEMSGAS)
            OnSaveTextAs();
        break;
    }

    case WM_DESTROY: // zavira se - vyhodime ho z pole
    {
        if (!Modal) // z pole budeme vyhazovat jen nemodalni dialogy
        {
            int i;
            for (i = ModelessDlgs.Count - 1; i >= 0; i--) // hledame od zadu (optimalizace pro ReleaseFS())
            {
                if (ModelessDlgs[i] == this)
                {
                    ModelessDlgs.Delete(i);
                    if (!ModelessDlgs.IsGood())
                        ModelessDlgs.ResetState();
                    break;
                }
            }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

void CWelcomeMsgDlg::OnSaveTextAs()
{
    static char initDir[MAX_PATH] = "";
    if (initDir[0] == 0)
        GetMyDocumentsPath(initDir);
    char fileName[MAX_PATH];
    strcpy(fileName, "listing.txt");

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = HWindow;
    char* s = LoadStr(IDS_RAWLISTINGFILTER);
    ofn.lpstrFilter = s;
    while (*s != 0) // vytvoreni double-null terminated listu
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initDir;
    ofn.lpstrDefExt = "txt";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = LoadStr(IDS_RAWLISTSAVEASTITLE);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT |
                OFN_NOTESTFILECREATE | OFN_HIDEREADONLY;

    char buf[200 + MAX_PATH];
    if (SalamanderGeneral->SafeGetSaveFileName(&ofn))
    {
        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

        s = strrchr(fileName, '\\');
        if (s != NULL)
        {
            memcpy(initDir, fileName, s - fileName);
            initDir[s - fileName] = 0;
        }

        if (SalamanderGeneral->SalGetFileAttributes(fileName) != 0xFFFFFFFF) // aby sel prepsat i read-only soubor
            SetFileAttributes(fileName, FILE_ATTRIBUTE_ARCHIVE);
        HANDLE file = HANDLES_Q(CreateFile(fileName, GENERIC_WRITE,
                                           FILE_SHARE_READ, NULL,
                                           CREATE_ALWAYS,
                                           FILE_FLAG_SEQUENTIAL_SCAN,
                                           NULL));
        if (file != INVALID_HANDLE_VALUE)
        {
            // zapis listingu
            ULONG written;
            BOOL success;
            DWORD err = NO_ERROR;
            DWORD size = TextSize != -1 ? TextSize : (DWORD)strlen(Text);
            if ((success = WriteFile(file, Text, size, &written, NULL)) == 0 || written != size)
            {
                if (!success)
                    err = GetLastError();
                else
                    err = ERROR_DISK_FULL;
            }

            HANDLES(CloseHandle(file));
            SetCursor(oldCur);
            if (err != NO_ERROR) // vypis chyby
            {
                sprintf(buf, LoadStr(IDS_RAWLISTSAVEERROR), SalamanderGeneral->GetErrorText(err));
                SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                DeleteFile(fileName); // pri chybe soubor smazem
            }
        }
        else
        {
            DWORD err = GetLastError();
            SetCursor(oldCur);
            sprintf(buf, LoadStr(IDS_RAWLISTSAVEERROR), SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
        }

        // ohlasime zmenu na ceste (mozna pribyl nas soubor)
        SalamanderGeneral->CutDirectory(fileName);
        SalamanderGeneral->PostChangeOnPathNotification(fileName, FALSE);
    }
}

//
// ****************************************************************************
// CLogsDlg
//

CLogsDlg::CLogsDlg(HWND parent, HWND centerToWnd, int showLogUID)
    : CDialog(HLanguage, IDD_LOGSDLG, parent, ooStatic)
{
    HasDelayedUpdateTimer = FALSE;
    DelayedUpdateLogUID = -1;

    SendWMClose = NULL;
    CloseDlg = FALSE;
    CenterToWnd = centerToWnd;
    SizeBox = NULL;
    ShowLogUID = showLogUID;
    Empty = TRUE;
    LastLogUID = -1;

    MinDlgHeight = 0;
    MinDlgWidth = 0;
    ComboBorderWidth = 0;
    ComboHeight = 0;
    EditBorderWidth = 0;
    EditBorderHeight = 0;
    SizeBoxWidth = 0;
    SizeBoxHeight = 0;
    LineHeight = 0;
}

void CLogsDlg::LoadListOfLogs(BOOL update)
{
    int prevUID = -1;
    HWND combo = GetDlgItem(HWindow, IDC_LISTOFLOGS);
    if (update)
        prevUID = LastLogUID; // bereme posledni log v editu (nelze brat cur-selection z comba, pri dropped down a fokusu mysi zlobi)

    MSG msg; // odstranime dalsi zpravy zadajici o update listu (prave jeden probehne)
    while (PeekMessage(&msg, NULL, WM_APP_UPDATELISTOFLOGS, WM_APP_UPDATELISTOFLOGS, PM_REMOVE))
        ;

    int index;
    Logs.AddLogsToCombo(combo, prevUID, &index, &Empty);
    if (index == -1) // nenasel prevUID -> zmena logu (jinak edit neprekreslujeme, je v nem stale stejny log)
    {
        PostMessage(HWindow, WM_COMMAND, MAKELONG(IDC_LISTOFLOGS, CBN_SELCHANGE), 0);
        if (index == -1)
            index = 0;
    }
    SendMessage(combo, CB_SETCURSEL, index, 0);
}

void CLogsDlg::LoadLog(int updateUID)
{
    HWND edit = GetDlgItem(HWindow, IDE_SERVERLOG);
    int actLogUID = -1;
    if (updateUID == -1) // zmena logu
    {
        HWND combo = GetDlgItem(HWindow, IDC_LISTOFLOGS);
        int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
        if (i != CB_ERR)
            actLogUID = (int)SendMessage(combo, CB_GETITEMDATA, i, 0); // x64: log UID je index, nejde o ukazatel
        LastLogUID = actLogUID;
    }
    else
        actLogUID = LastLogUID; // jen update (necteme cur-selection, protoze pri dropped stavu vraci aktualni fokus mysi - neodpovida indexu v combu)

    if (updateUID != -1) // ma se updatnout log s UID 'updateUID'
    {
        BOOL found = (actLogUID == updateUID); // TRUE = v combu je vybran updatovany log s UID 'actLogUID'
        MSG msg;                               // odstranime dalsi zpravy zadajici o update logu (provedeme update aktualniho logu)
        while (PeekMessage(&msg, NULL, WM_APP_UPDATELOG, WM_APP_UPDATELOG, PM_REMOVE))
        {
            if (!found) // pokud jeste neni jasne, ze update se ma provest
                found = (actLogUID == (int)msg.wParam);
        }
        if (found)
            Logs.SetLogToEdit(edit, actLogUID, TRUE);
    }
    else // v combu se zmenil oznaceny log -> ukazeme text noveho logu
    {
        if (actLogUID != -1)
            Logs.SetLogToEdit(edit, actLogUID, FALSE);
        else
            SetWindowText(edit, "");
    }
}

INT_PTR
CLogsDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CLogsDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SendMessage(HWindow, WM_SETICON, ICON_BIG, (LPARAM)FTPLogIconBig);
        SendMessage(HWindow, WM_SETICON, ICON_SMALL, (LPARAM)FTPLogIcon);

        if (FixedFont != NULL)
            SendDlgItemMessage(HWindow, IDE_SERVERLOG, WM_SETFONT, (WPARAM)FixedFont, TRUE);

        CSimpleDlgControlWindow* wnd = new CSimpleDlgControlWindow(HWindow, IDC_LISTOFLOGS);
        if (wnd != NULL && wnd->HWindow == NULL)
            delete wnd; // nepodarilo se attachnout - nedealokuje se samo
        wnd = new CSimpleDlgControlWindow(HWindow, IDE_SERVERLOG);
        if (wnd != NULL && wnd->HWindow == NULL)
            delete wnd; // nepodarilo se attachnout - nedealokuje se samo

        RECT r1, r2;
        GetWindowRect(HWindow, &r1);
        GetClientRect(GetDlgItem(HWindow, IDE_SERVERLOG), &r2);
        MinDlgHeight = r1.bottom - r1.top - r2.bottom; // edit nechame zmensit na 0 bodu client area
        MinDlgWidth = r1.right - r1.left - r2.right;   // edit nechame zmensit na 0 bodu client area

        GetClientRect(HWindow, &r1);
        GetWindowRect(GetDlgItem(HWindow, IDC_LISTOFLOGS), &r2);
        ComboBorderWidth = r1.right - (r2.right - r2.left);
        ComboHeight = r2.bottom - r2.top;
        GetWindowRect(GetDlgItem(HWindow, IDE_SERVERLOG), &r2);
        EditBorderWidth = r1.right - (r2.right - r2.left);
        EditBorderHeight = r1.bottom - (r2.bottom - r2.top);
        GetWindowRect(GetDlgItem(HWindow, IDC_MENULINE), &r2);
        LineHeight = r2.bottom - r2.top;

        // do praveho spodniho ruzku vlozime znacek pro resize okna
        SizeBox = CreateWindowEx(0,
                                 "scrollbar",
                                 "",
                                 WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE |
                                     WS_GROUP | SBS_SIZEBOX | SBS_SIZEGRIP | SBS_SIZEBOXBOTTOMRIGHTALIGN,
                                 0, 0, r1.right, r1.bottom,
                                 HWindow,
                                 NULL,
                                 DLLInstance,
                                 NULL);
        GetClientRect(SizeBox, &r2);
        SizeBoxWidth = r2.right;
        SizeBoxHeight = r2.bottom;

        LoadListOfLogs(FALSE);

        if (ShowLogUID != -1)
            SendMessage(HWindow, WM_APP_ACTIVATELOG, ShowLogUID, 0);

        if (Config.LogsDlgPlacement.length != 0) // pozice je platna
        {
            WINDOWPLACEMENT place = Config.LogsDlgPlacement;
            // GetWindowPlacement cti Taskbar, takze pokud je Taskbar nahore nebo vlevo,
            // jsou hodnoty posunute o jeho rozmery. Provedeme korekci.
            RECT monitorRect;
            RECT workRect;
            SalamanderGeneral->MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
            OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
                       workRect.top - monitorRect.top);
            SalamanderGeneral->MultiMonEnsureRectVisible(&place.rcNormalPosition, TRUE);
            MoveWindow(HWindow, place.rcNormalPosition.left,
                       place.rcNormalPosition.top,
                       place.rcNormalPosition.right - place.rcNormalPosition.left,
                       place.rcNormalPosition.bottom - place.rcNormalPosition.top, FALSE);
            if (place.showCmd == SW_MAXIMIZE || place.showCmd == SW_SHOWMAXIMIZED)
                ShowWindow(HWindow, SW_MAXIMIZE);
        }
        else
        {
            if (CenterToWnd != NULL)
            {
                // horizontalni i vertikalni vycentrovani dialogu k 'CenterToWnd'
                SalamanderGeneral->MultiMonCenterWindow(HWindow, CenterToWnd, TRUE);
            }
        }
        break;
    }

    case WM_CLOSE:
    {
        if (SendWMClose != NULL &&     // pokud je mozne nechat si poslat WM_CLOSE +
            !IsWindowEnabled(HWindow)) // pokud mame otevreny modalni dialog, musime ho najit a zavrit,
        {                              // pak si nechame poslat znovu WM_CLOSE (zajisti "vynoreni"
                                       // z messageloopy toho modalniho dialogu)
            SalamanderGeneral->CloseAllOwnedEnabledDialogs(HWindow);
            *SendWMClose = TRUE; // nechame si znovu poslat WM_CLOSE
            return TRUE;         // dale nezpracovavat (close byl zakazan)
        }
        break;
    }

    case WM_APP_UPDATELISTOFLOGS:
        LoadListOfLogs(TRUE);
        return TRUE; // dale nezpracovavat

    case WM_TIMER:
    {
        if (wParam == LOGSDLG_DELAYEDUPDATETIMER) // provedeme zpozdeny refresh
        {
            if (DelayedUpdateLogUID != -1) // behem doby "hajeni" bylo potreba refreshnout, timer nechame
            {
                int uid = DelayedUpdateLogUID;
                DelayedUpdateLogUID = -1;
                LoadLog(uid);
            }
            else // behem doby "hajeni" nebylo potreba refreshnout, zrusime timer
            {
                HasDelayedUpdateTimer = FALSE;
                KillTimer(HWindow, LOGSDLG_DELAYEDUPDATETIMER);
            }
            return TRUE; // dale nezpracovavat
        }
        break;
    }

    case WM_APP_UPDATELOG:
    {
        if (!HasDelayedUpdateTimer)
        {
            LoadLog((int)wParam); // provedeme refresh

            // dalsi refresh provedeme nejdrive za 1/10 sekundy
            HasDelayedUpdateTimer = (SetTimer(HWindow, LOGSDLG_DELAYEDUPDATETIMER, 100, NULL) != 0);
            DelayedUpdateLogUID = -1; // pro sychr
        }
        else
        {
            if (DelayedUpdateLogUID != -1 && DelayedUpdateLogUID != (int)wParam)
            { // rozdilne updatovane UID logu, provedeme refresh predchoziho UID (aby doslo k doruceni poporade)
                LoadLog(DelayedUpdateLogUID);
            }
            DelayedUpdateLogUID = (int)wParam;
        }
        return TRUE; // dale nezpracovavat
    }

    case WM_APP_ACTIVATELOG:
    {
        HWND combo = GetDlgItem(HWindow, IDC_LISTOFLOGS);
        int count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0);
        int i;
        for (i = 0; i < count; i++)
        {
            if (SendMessage(combo, CB_GETITEMDATA, i, 0) == (int)wParam) // najdeme UID // FIXME_X64 podezrele pretypovani
            {                                                            // (bohuzel nejde hledat primo v Logs - pri zmene poctu logu by index nesedel)
                if (SendMessage(combo, CB_GETCURSEL, 0, 0) != i)
                {
                    SendMessage(combo, CB_SETCURSEL, i, 0);
                    // x64 - log UID je index, nejde o ukazatel
                    LastLogUID = (int)SendMessage(combo, CB_GETITEMDATA, i, 0); // musi se nastavit, jinak LoadListOfLogs zmeni nastaveni indexu v 'combo'
                    PostMessage(HWindow, WM_COMMAND, MAKELONG(IDC_LISTOFLOGS, CBN_SELCHANGE), 0);
                }
                if (GetForegroundWindow() == HWindow || !IsWindowVisible(HWindow)) // aktivni nebo se prave otevira
                {
                    HWND edit = GetDlgItem(HWindow, IDE_SERVERLOG);
                    if (GetFocus() != edit)
                        PostMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)edit, TRUE);
                }
                break;
            }
        }
        return TRUE; // dale nezpracovavat
    }

    case WM_INITMENU:
    {
        HMENU hMenu = GetMenu(HWindow);
        if (hMenu != NULL)
        {
            MyEnableMenuItem(hMenu, CM_COPYLOG, !Empty);
            MyEnableMenuItem(hMenu, CM_COPYALLLOGS, !Empty);
            MyEnableMenuItem(hMenu, CM_SAVELOG, !Empty);
            MyEnableMenuItem(hMenu, CM_SAVEALLLOGS, !Empty);
            MyEnableMenuItem(hMenu, CM_CLEARLOG, !Empty);
            MyEnableMenuItem(hMenu, CM_REMOVELOG, !Empty);
            MyEnableMenuItem(hMenu, CM_REMOVEALLLOGS, !Empty);
            CheckMenuItem(hMenu, CM_SHOWLOGFORPANEL, MF_BYCOMMAND | (Config.AlwaysShowLogForActPan ? MF_CHECKED : MF_UNCHECKED));
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
            return TRUE; // dale nezpracovavat, Enter nesmi ukoncovat dialog

        case IDC_LISTOFLOGS:
        {
            if (HIWORD(wParam) == CBN_SELCHANGE)
                LoadLog(-1);
            break;
        }

            // ***************************************************************************
            // POZOR: aby sla modalni okna "nenasilne" zavrit (napr. pri unloadu pluginu),
            //        musi vsechna otevrena modalni okna umet skoncit na WM_CLOSE
            //        (asi kdyz umi skoncit na klavesu Esc) + nesmi se vyvolavat
            //        vic modalnich oken po sobe, pokud predchazejici modalni
            //        okno bylo ukonceno pres Esc
            // ***************************************************************************

        case CM_SAVELOG:
        case CM_COPYLOG:
        case CM_CLEARLOG:
        case CM_REMOVELOG:
        {
            char buf[300];
            buf[0] = 0;
            HWND combo = GetDlgItem(HWindow, IDC_LISTOFLOGS);
            int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (i != CB_ERR && SendMessage(combo, CB_GETLBTEXTLEN, i, 0) < 300)
            {
                SendMessage(combo, CB_GETLBTEXT, i, (LPARAM)buf);
                int uid = (int)SendMessage(combo, CB_GETITEMDATA, i, 0); // x64 - log UID je index, nejde o ukazatel
                switch (LOWORD(wParam))
                {
                case CM_SAVELOG:
                    Logs.SaveLog(HWindow, buf, uid);
                    break;
                case CM_COPYLOG:
                    Logs.CopyLog(HWindow, buf, uid);
                    break;
                case CM_CLEARLOG:
                    Logs.ClearLog(HWindow, buf, uid);
                    break;
                case CM_REMOVELOG:
                    Logs.RemoveLog(HWindow, buf, uid);
                    break;
                }
            }
            break;
        }

        case CM_SAVEALLLOGS:
            Logs.SaveAllLogs(HWindow);
            break;
        case CM_COPYALLLOGS:
            Logs.CopyAllLogs(HWindow);
            break;
        case CM_REMOVEALLLOGS:
            Logs.RemoveAllLogs(HWindow);
            break;
        case CM_SHOWLOGFORPANEL:
            Config.AlwaysShowLogForActPan = !Config.AlwaysShowLogForActPan;
            break;
        case CM_HELPFORLOGSWND:
            SalamanderGeneral->OpenHtmlHelp(HWindow, HHCDisplayContext, IDH_FTPLOGSWINDOW, FALSE);
            break;
        }

        break;
    }

    case WM_QUERYDRAGICON:
        return (BOOL)(INT_PTR)FTPLogIconBig; // nejpis zbytecne

    case WM_SIZE:
    {
        RECT clientRect;
        GetClientRect(HWindow, &clientRect);
        HWND line = GetDlgItem(HWindow, IDC_MENULINE);
        HWND combo = GetDlgItem(HWindow, IDC_LISTOFLOGS);
        HWND edit = GetDlgItem(HWindow, IDE_SERVERLOG);

        HDWP hdwp = HANDLES(BeginDeferWindowPos(5));
        if (hdwp != NULL)
        {
            hdwp = HANDLES(DeferWindowPos(hdwp, line, NULL,
                                          0, 0, clientRect.right, LineHeight,
                                          SWP_NOZORDER | SWP_NOMOVE));

            hdwp = HANDLES(DeferWindowPos(hdwp, combo, NULL,
                                          0, 0, clientRect.right - ComboBorderWidth, ComboHeight,
                                          SWP_NOZORDER | SWP_NOMOVE));

            hdwp = HANDLES(DeferWindowPos(hdwp, edit, NULL,
                                          0, 0, clientRect.right - EditBorderWidth, clientRect.bottom - EditBorderHeight,
                                          SWP_NOZORDER | SWP_NOMOVE));

            if (SizeBox != NULL)
            {
                hdwp = HANDLES(DeferWindowPos(hdwp, SizeBox, NULL, clientRect.right - SizeBoxWidth,
                                              clientRect.bottom - SizeBoxHeight, SizeBoxWidth, SizeBoxHeight, SWP_NOZORDER));
                // pry nejde show/hide kombinovat se zmenou velikosti a posunem
                hdwp = HANDLES(DeferWindowPos(hdwp, SizeBox, NULL, 0, 0, 0, 0,
                                              SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | (wParam == SIZE_RESTORED ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)));
            }

            HANDLES(EndDeferWindowPos(hdwp));
        }
        break;
    }

    case WM_HELP:
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
            (GetKeyState(VK_SHIFT) & 0x8000) == 0)
        {
            SalamanderGeneral->OpenHtmlHelp(HWindow, HHCDisplayContext, IDH_FTPLOGSWINDOW, FALSE);
            break;
        }
        return TRUE; // F1 nenechame propadnout do parenta
    }

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinDlgWidth;
        lpmmi->ptMinTrackSize.y = MinDlgHeight;
        break;
    }

    case WM_DESTROY:
    {
        if (HasDelayedUpdateTimer)
            KillTimer(HWindow, LOGSDLG_DELAYEDUPDATETIMER);
        HasDelayedUpdateTimer = FALSE;

        Logs.SaveLogsDlgPos();
        if (!CloseDlg)
            Logs.SetLogsDlg(NULL); // od ted jiz objekt dialogu neni platny
        PostQuitMessage(0);
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CSetWaitCursorWindow
//

void CSetWaitCursorWindow::AttachToWindow(HWND hWnd)
{
    DefWndProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
    if (DefWndProc == NULL)
    {
        TRACE_E("Spatny handle okna. hWnd = " << hWnd);
        DefWndProc = DefWindowProc;
        return;
    }
    if (!SetProp(hWnd, (LPCTSTR)AtomObject2, (HANDLE)this))
    {
        TRACE_E("Chyba pri pripojovani objektu na okno.");
        DefWndProc = DefWindowProc;
        return;
    }
    HWindow = hWnd;
    SetWindowLongPtr(HWindow, GWLP_WNDPROC, (LONG_PTR)CSetWaitCursorWindow::CWindowProc);

    if (DefWndProc == CSetWaitCursorWindow::CWindowProc) // to by byla rekurze
    {
        TRACE_E("Tak tohle by se vubec nemelo stat.");
        DefWndProc = DefWindowProc;
    }
}

void CSetWaitCursorWindow::DetachWindow()
{
    if (HWindow != NULL)
    {
        RemoveProp(HWindow, (LPCTSTR)AtomObject2);
        SetWindowLongPtr(HWindow, GWLP_WNDPROC, (LONG_PTR)DefWndProc);

        POINT p;
        RECT r;
        if (GetWindowRect(HWindow, &r) && GetCursorPos(&p) && PtInRect(&r, p))
        {
            DWORD ht = (DWORD)SendMessage(HWindow, WM_NCHITTEST, 0, MAKELONG(p.x, p.y));
            SendMessage(HWindow, WM_SETCURSOR, (WPARAM)HWindow, MAKELONG(ht, WM_MOUSEMOVE));
        }
        HWindow = NULL;
    }
}

LRESULT CALLBACK
CSetWaitCursorWindow::CWindowProc(HWND hwnd, UINT uMsg,
                                  WPARAM wParam, LPARAM lParam)
{
    CSetWaitCursorWindow* wnd = (CSetWaitCursorWindow*)GetProp(hwnd, (LPCTSTR)AtomObject2);
    if (uMsg == WM_DESTROY && wnd != NULL) // posledni zprava - odpojeni objektu od okna
    {
        LRESULT res = CallWindowProc((WNDPROC)wnd->DefWndProc, hwnd, uMsg, wParam, lParam);
        // ted uz zase do stare procedury (kvuli subclassingu)
        RemoveProp(hwnd, (LPCTSTR)AtomObject2);
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)wnd->DefWndProc);
        if (res == 0)
            return 0; // aplikace ji zpracovala
        wnd = NULL;
    }
    //--- zavolani metody WindowProc(...) prislusneho objektu okna
    if (wnd != NULL)
    {
        if (uMsg == WM_SETCURSOR)
        {
            BOOL activePopupExists = FALSE;
            if (!IsWindowEnabled(hwnd)) // pokud mame otevreny modalni dialog, nemuzeme provadet aktivaci tohoto okna (jeho parenta)
            {
                HWND dlg = GetLastActivePopup(hwnd);
                activePopupExists = dlg != NULL && dlg != hwnd && IsWindowEnabled(dlg) && IsWindowVisible(dlg);
            }
            if (!activePopupExists)
            {
                SetCursor(LoadCursor(NULL, IDC_WAIT));
                if (HIWORD(lParam) == WM_LBUTTONDOWN || HIWORD(lParam) == WM_RBUTTONDOWN)
                    SetForegroundWindow(hwnd);
                return TRUE;
            }
        }
        return CallWindowProc((WNDPROC)wnd->DefWndProc, hwnd, uMsg, wParam, lParam);
    }
    else
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

//****************************************************************************
//
// CWaitWindow
//

void ShowWaitWindow(HWND hwnd, HWND parent)
{
    // tato silenost zavedene jen kvuli (neuspesnemu) pokusu o otevreni Logs dialogu v hl. threadu
    // (jako nemodalni dialog); klidne mozne nahradit ShowWindow(hwnd, SW_SHOWNA)

    HWND p = GetNextWindow(parent, GW_HWNDPREV);
    UINT flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOOWNERZORDER;
    if (p == hwnd) // nad parentem visi primo wait-okenko, jen ho ukazeme
    {
        flags |= SWP_NOZORDER;
        p = NULL;
    }
    else // nad parentem je jine nemodalni okno - wait-okenko ukazeme tesne nad parentem
    {
        if (p == NULL)
            p = HWND_TOP; // pro jistotu, nemelo by asi vubec nastat
    }
    SetWindowPos(hwnd, p, 0, 0, 0, 0, flags);
}

CWaitWindow::CWaitWindow(HWND hParent, BOOL showCloseButton) : CWindow(ooStatic)
{
    HParent = hParent;
    Text = NULL;
    Caption = NULL;
    ShowCloseButton = showCloseButton;
    WindowClosePressed = FALSE;
    HasTimer = FALSE;
    Visible = FALSE;
    NeedWrap = FALSE;
}

CWaitWindow::~CWaitWindow()
{
    if (Text != NULL)
        SalamanderGeneral->Free(Text);
    if (Caption != NULL)
        SalamanderGeneral->Free(Caption);
}

void CWaitWindow::SetText(const char* text)
{
    if (Text != NULL)
        SalamanderGeneral->Free(Text);
    Text = SalamanderGeneral->DupStr(text);
    if (HWindow != NULL && IsWindowVisible(HWindow))
    {
        InvalidateRect(HWindow, NULL, TRUE);
        UpdateWindow(HWindow);
    }
}

void CWaitWindow::SetCaption(const char* text)
{
    if (Caption != NULL)
        SalamanderGeneral->Free(Caption);
    Caption = SalamanderGeneral->DupStr(text);
}

#define WAITWINDOW_HMARGIN 25
#define WAITWINDOW_VMARGIN 18

HWND CWaitWindow::Create(DWORD showTime)
{
    CALL_STACK_MESSAGE2("CWaitWindow::Create(%u)", showTime);
    if (HWindow != NULL)
    {
        TRACE_E("Unexpected situation in CWaitWindow::Create(): window already exists!");
        return HWindow;
    }

    WindowClosePressed = FALSE;
    HasTimer = FALSE;
    if (Text == NULL)
    {
        TRACE_E("Unexpected situation in CWaitWindow::Create(): text is not defined!");
        return NULL;
    }

    HWND hCenterWnd = (HParent != NULL ? HParent : SalamanderGeneral->GetMainWindowHWND());

    RECT clipRect;
    SalamanderGeneral->MultiMonGetClipRectByWindow(hCenterWnd, &clipRect, NULL);
    int scrW = clipRect.right - clipRect.left;
    int scrH = clipRect.bottom - clipRect.top;

    RECT parentRect;
    int parW = scrW - 10;
    if (!IsZoomed(hCenterWnd) && !IsIconic(hCenterWnd))
    {
        GetWindowRect(hCenterWnd, &parentRect);
        parW = (int)(parentRect.right - parentRect.left) - 10; // -10 jako rezerva, at to neni pres cely okno
    }
    if (parW < scrW / 3)
        parW = scrW / 3;
    if (parW > scrW - 10)
        parW = scrW - 10; // -10 jako rezerva, at to neni pres celou obrazovku

    // napocitame velikost textu -> velikost okna
    NeedWrap = FALSE;
    HDC dc = HANDLES(GetDC(NULL));
    if (dc != NULL)
    {
        HFONT old = NULL;
        if (SystemFont != NULL)
            old = (HFONT)SelectObject(dc, SystemFont);
        RECT tR;
        tR.left = 0;
        tR.top = 0;
        tR.right = 1;
        tR.bottom = 1;
        DrawText(dc, Text, -1, &tR, DT_CALCRECT | DT_LEFT | DT_NOPREFIX);
        if (tR.right + 2 * WAITWINDOW_HMARGIN > parW)
        {
            tR.right = parW - 2 * WAITWINDOW_HMARGIN;
            tR.bottom = 1;
            DrawText(dc, Text, -1, &tR, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
            NeedWrap = TRUE;
        }
        TextSize.cx = tR.right;
        TextSize.cy = tR.bottom;
        if (old != NULL)
            SelectObject(dc, old);
        HANDLES(ReleaseDC(NULL, dc));
    }

    int width = TextSize.cx + 2 * WAITWINDOW_HMARGIN;
    int height = TextSize.cy + 2 * WAITWINDOW_VMARGIN;
    height += GetSystemMetrics(SM_CYSMCAPTION);

    CreateEx(WS_EX_DLGMODALFRAME /*| WS_EX_TOOLWINDOW*/,
             SAVEBITS_CLASSNAME,
             Caption == NULL ? LoadStr(IDS_FTPPLUGINTITLE) : Caption,
             WS_BORDER | WS_OVERLAPPED | (ShowCloseButton ? WS_SYSMENU : 0),
             0, 0, width, height,
             HParent,
             NULL,
             DLLInstance,
             this);
    SetVisible(FALSE);

    if (HWindow != NULL)
    {
        SalamanderGeneral->MultiMonCenterWindow(HWindow, hCenterWnd, TRUE);
        if (showTime != 0)
        {
            if (SetTimer(HWindow, WAITWND_SHOWTIMER, showTime, NULL))
                HasTimer = TRUE;
            else
                TRACE_E("CWaitWindow::Create(): SetTimer has failed, showing window immediately.");
        }
        if (!HasTimer)
        {
            ShowWaitWindow(HWindow, HParent);
            SetVisible(TRUE);
        }
    }

    return HWindow;
}

void CWaitWindow::Destroy()
{
    if (HWindow != NULL)
        SendMessage(HWindow, WM_CLOSE, 0, 0);
}

void CWaitWindow::Show(BOOL show)
{
    CALL_STACK_MESSAGE2("CWaitWindow::Show(%d)", show);
    if (HWindow != NULL)
    {
        if (HasTimer)
        {
            // zabijeme timer, aby se nedorucilo pripadne "show"
            KillTimer(HWindow, WAITWND_SHOWTIMER);
            // vycistime message-queue od pripadnych WM_TIMER
            MSG msg;
            while (PeekMessage(&msg, HWindow, WM_TIMER, WM_TIMER, PM_REMOVE))
                ;
            HasTimer = FALSE;
        }

        if (show)
        {
            if (!Visible)
            {
                ShowWaitWindow(HWindow, HParent);
                SetVisible(TRUE);
            }
        }
        else
        {
            if (Visible)
            {
                ShowWindow(HWindow, SW_HIDE);
                SetVisible(FALSE);
            }
        }
    }
}

LRESULT
CWaitWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CWaitWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_TIMER:
    {
        if (wParam == WAITWND_SHOWTIMER)
        {
            KillTimer(HWindow, WAITWND_SHOWTIMER);
            ShowWaitWindow(HWindow, HParent);
            SetVisible(TRUE);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
    {
        if (HasTimer)
            KillTimer(HWindow, WAITWND_SHOWTIMER); // pro jistotu ho zrusime sami
        if (GetForegroundWindow() == HWindow)      // predame fokus parentovi
        {
            HWND hActivate = (HParent != NULL ? HParent : SalamanderGeneral->GetMainWindowHWND());
            SetForegroundWindow(hActivate);
        }
        break;
    }

    case WM_NCACTIVATE:
    {
        wParam = FALSE; // vzdy kreslit "inactive" caption
        break;
    }

    case WM_APP_ACTIVATEPARENT:
    {
        HWND hActivate = (HParent != NULL ? HParent : SalamanderGeneral->GetMainWindowHWND());
        SetForegroundWindow(hActivate);
        return 0;
    }

    case WM_ACTIVATEAPP:
    {
        // deaktivace aplikace + ma se zobrazit wait-okno (neni uz schovane kvuli dialogu)
        if (!Visible && wParam == FALSE && HasTimer)
            Show(TRUE); // je nutne okno ukazat, aby se dalo do Salama vratit pres Alt+Tab
        break;          // WM_ACTIVATEAPP chodi, i kdyz neni wait-okenko ani jeho parent aktivni (aktivni
    }                   // jakykoliv nemodalni dialog bez parenta - drive Logs dialog) - nelze aktivovat wait-okenko

    case WM_ACTIVATE:
    {
        if (Visible &&                                    // wait okenko je viditelne +
            uMsg == WM_ACTIVATE && wParam != WA_INACTIVE) // jakakoliv aktivace -> predame fokus parentovi
        {
            PostMessage(HWindow, WM_APP_ACTIVATEPARENT, 0, 0); // na W2K+ uz je asi zbytecne
            if (uMsg == WM_ACTIVATE && wParam == WA_CLICKACTIVE)
            {
                // pokud user kliknul na Close tlacitko, nastavime globalni promennou
                if (CWindow::WindowProc(WM_NCHITTEST, NULL, GetMessagePos()) == HTCLOSE)
                    WindowClosePressed = TRUE;
            }
        }
        break;
    }

    case WM_ERASEBKGND:
    {
        LRESULT ret = CWindow::WindowProc(uMsg, wParam, lParam);
        if (Text != NULL)
        {
            HDC dc = (HDC)wParam;
            RECT r;
            GetClientRect(HWindow, &r);

            r.left = (r.right - TextSize.cx) / 2;
            r.top = (r.bottom - TextSize.cy) / 2;
            r.right = r.left + TextSize.cx;
            r.bottom = r.top + TextSize.cy;

            HFONT hOldFont = NULL;
            if (SystemFont != NULL)
                hOldFont = (HFONT)SelectObject(dc, SystemFont);
            int prevBkMode = SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
            // nebudeme klipovat, abychom ustali drobne prodlouzeni textu, ke kteremu
            // muze dojit behem volani SetText
            DrawText(dc, Text, (int)strlen(Text), &r, DT_LEFT | DT_NOPREFIX | DT_NOCLIP | (NeedWrap ? DT_WORDBREAK : 0));
            SetBkMode(dc, prevBkMode);
            if (hOldFont != NULL)
                SelectObject(dc, hOldFont);
        }
        return ret;
    }

    case WM_NCHITTEST:
    {
        // zabranime presouvani okna pomoci tazeni za titulek
        // zaroven zablokujeme tooltip nad Close tlacitkem
        return HTCLIENT;
    }

    case WM_NCLBUTTONDBLCLK:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    {
        HWND hActivate = (HParent != NULL ? HParent : SalamanderGeneral->GetMainWindowHWND());
        SetForegroundWindow(hActivate);

        if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP ||
            uMsg == WM_NCLBUTTONDOWN || uMsg == WM_NCLBUTTONUP)
        {
            // pokud user kliknul na Close tlacitko, nastavime globalni promennou
            if (CWindow::WindowProc(WM_NCHITTEST, NULL, GetMessagePos()) == HTCLOSE)
                WindowClosePressed = TRUE;
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CListWaitWindow
//

CListWaitWindow::CListWaitWindow(HWND hParent, CDataConnectionSocket* dataConnection,
                                 BOOL* aborted) : CWaitWindow(hParent, TRUE)
{
    Aborted = aborted;
    DataConnection = dataConnection;

    PathOnFTPText = NULL;
    EstimatedTimeText = NULL;
    ElapsedTimeText = NULL;
    OperStatusText = NULL;
    OperProgressBar = NULL;

    Path = NULL;
    PathType = ftpsptEmpty;

    Status[0] = 0;
    TimeLeft[0] = 0;
    TimeElapsed[0] = 0;
    HasRefreshStatusTimer = FALSE;
    HasDelayedUpdateTimer = FALSE;
    NeedDelayedUpdate = FALSE;

    LastTimeEstimation = -1;
    ElapsedTime = 0;
    LastTickCountForElapsedTime = -1;
}

CListWaitWindow::~CListWaitWindow()
{
    if (Path != NULL)
        SalamanderGeneral->Free(Path);
}

void CListWaitWindow::SetText(const char* text)
{
    if (Text != NULL)
        SalamanderGeneral->Free(Text);
    Text = SalamanderGeneral->DupStr(text);
    if (HWindow != NULL && Text != NULL)
        SendDlgItemMessage(HWindow, IDT_ACTION, WM_SETTEXT, 0, (LPARAM)Text);
}

void CListWaitWindow::SetPath(const char* path, CFTPServerPathType pathType)
{
    if (Path != NULL)
        SalamanderGeneral->Free(Path);
    Path = SalamanderGeneral->DupStr(path);
    PathType = pathType;
    if (PathOnFTPText != NULL && Path != NULL)
    {
        PathOnFTPText->SetPathSeparator(FTPGetPathDelimiter(PathType));
        PathOnFTPText->SetText(Path);
    }
}

// ***************************************************************************

class CGetSizesOfListWaitDlg : public CDialog
{
public:
    int Width;
    int Height;
    HWND NewParent; // kam se maji presunout controly

public:
    CGetSizesOfListWaitDlg(HWND newParent);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

CGetSizesOfListWaitDlg::CGetSizesOfListWaitDlg(HWND newParent)
    : CDialog(HLanguage, IDD_LISTWAITDLG, NULL, ooStatic)
{
    Width = Height = -1;
    NewParent = newParent;
}

INT_PTR
CGetSizesOfListWaitDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CGetSizesOfListWaitDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    if (uMsg == WM_INITDIALOG)
    {
        RECT r;
        GetWindowRect(HWindow, &r);
        Width = r.right - r.left;
        Height = r.bottom - r.top;

        // presuneme child okna dialogu (odpojime je od tohoto dialogu)
        ::SetParent(GetDlgItem(HWindow, IDT_ACTION), NewParent);
        ::SetParent(GetDlgItem(HWindow, IDT_PATHONFTP1), NewParent);
        ::SetParent(GetDlgItem(HWindow, IDT_PATHONFTP2), NewParent);
        ::SetParent(GetDlgItem(HWindow, IDT_ESTIMATEDTIME1), NewParent);
        ::SetParent(GetDlgItem(HWindow, IDT_ESTIMATEDTIME2), NewParent);
        ::SetParent(GetDlgItem(HWindow, IDT_ELAPSEDTIME1), NewParent);
        ::SetParent(GetDlgItem(HWindow, IDT_ELAPSEDTIME2), NewParent);
        ::SetParent(GetDlgItem(HWindow, IDT_OPERSTATUS1), NewParent);
        ::SetParent(GetDlgItem(HWindow, IDT_OPERSTATUS2), NewParent);
        ::SetParent(GetDlgItem(HWindow, IDC_OPERPROGRESS), NewParent);

        EndDialog(HWindow, IDABORT); // rozmery a childy uz mame, ukoncime dialog
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

// ***************************************************************************

HWND CListWaitWindow::Create(DWORD showTime)
{
    CALL_STACK_MESSAGE2("CListWaitWindow::Create(%u)", showTime);
    if (HWindow != NULL)
    {
        TRACE_E("Unexpected situation in CListWaitWindow::Create(): window already exists!");
        return HWindow;
    }

    WindowClosePressed = FALSE;
    HasTimer = FALSE;
    Status[0] = 0;
    TimeLeft[0] = 0;
    TimeElapsed[0] = 0;
    HasRefreshStatusTimer = FALSE;

    ElapsedTime = 0;
    LastTickCountForElapsedTime = GetTickCount();

    CreateEx(WS_EX_DLGMODALFRAME /*| WS_EX_TOOLWINDOW*/,
             SAVEBITS_CLASSNAME,
             Caption == NULL ? LoadStr(IDS_FTPPLUGINTITLE) : Caption,
             WS_BORDER | WS_OVERLAPPED | (ShowCloseButton ? WS_SYSMENU : 0),
             0, 0, 0, 0,
             HParent,
             NULL,
             DLLInstance,
             this);

    if (HWindow != NULL)
    {
        CGetSizesOfListWaitDlg getSizesOfListWaitDlg(HWindow); // presuneme controly z dialogu do noveho okna
        getSizesOfListWaitDlg.Execute();                       // dialog se zrusi jeste pred svym otevrenim

        // napocitame umisteni okenka; prioritne centrujeme k HParent; v druhe rade k MainWindow
        HWND hCenterWnd = (HParent != NULL ? HParent : SalamanderGeneral->GetMainWindowHWND());
        MoveWindow(HWindow, 0, 0, getSizesOfListWaitDlg.Width, getSizesOfListWaitDlg.Height, FALSE);
        SalamanderGeneral->MultiMonCenterWindow(HWindow, hCenterWnd, TRUE);

        if (SystemFont != NULL)
        {
            SendDlgItemMessage(HWindow, IDT_ACTION, WM_SETFONT, (WPARAM)SystemFont, TRUE);
            SendDlgItemMessage(HWindow, IDT_PATHONFTP1, WM_SETFONT, (WPARAM)SystemFont, TRUE);
            SendDlgItemMessage(HWindow, IDT_PATHONFTP2, WM_SETFONT, (WPARAM)SystemFont, TRUE);
            SendDlgItemMessage(HWindow, IDT_ESTIMATEDTIME1, WM_SETFONT, (WPARAM)SystemFont, TRUE);
            SendDlgItemMessage(HWindow, IDT_ESTIMATEDTIME2, WM_SETFONT, (WPARAM)SystemFont, TRUE);
            SendDlgItemMessage(HWindow, IDT_ELAPSEDTIME1, WM_SETFONT, (WPARAM)SystemFont, TRUE);
            SendDlgItemMessage(HWindow, IDT_ELAPSEDTIME2, WM_SETFONT, (WPARAM)SystemFont, TRUE);
            SendDlgItemMessage(HWindow, IDT_OPERSTATUS1, WM_SETFONT, (WPARAM)SystemFont, TRUE);
            SendDlgItemMessage(HWindow, IDT_OPERSTATUS2, WM_SETFONT, (WPARAM)SystemFont, TRUE);
            SendDlgItemMessage(HWindow, IDC_OPERPROGRESS, WM_SETFONT, (WPARAM)SystemFont, TRUE);
        }

        PathOnFTPText = SalamanderGUI->AttachStaticText(HWindow, IDT_PATHONFTP2, STF_PATH_ELLIPSIS);
        EstimatedTimeText = SalamanderGUI->AttachStaticText(HWindow, IDT_ESTIMATEDTIME2, STF_CACHED_PAINT);
        ElapsedTimeText = SalamanderGUI->AttachStaticText(HWindow, IDT_ELAPSEDTIME2, STF_CACHED_PAINT);
        OperStatusText = SalamanderGUI->AttachStaticText(HWindow, IDT_OPERSTATUS2, STF_CACHED_PAINT);
        OperProgressBar = SalamanderGUI->AttachProgressBar(HWindow, IDC_OPERPROGRESS);
        if (OperProgressBar != NULL)
        {
            OperProgressBar->SetSelfMoveTime(1000);
            OperProgressBar->SetSelfMoveSpeed(100);
        }

        if (Text != NULL)
            SendDlgItemMessage(HWindow, IDT_ACTION, WM_SETTEXT, 0, (LPARAM)Text);
        if (PathOnFTPText != NULL && Path != NULL)
        {
            PathOnFTPText->SetPathSeparator(FTPGetPathDelimiter(PathType));
            PathOnFTPText->SetText(Path);
        }
        DataConnection->SetWindowWithStatus(HWindow, WM_APP_STATUSCHANGED);
        RefreshTimeAndStatusAndProgress(FALSE);
    }

    SetVisible(FALSE);

    if (HWindow != NULL)
    {
        if (showTime != 0)
        {
            if (SetTimer(HWindow, WAITWND_SHOWTIMER, showTime, NULL))
                HasTimer = TRUE;
            else
                TRACE_E("CWaitWindow::Create(): SetTimer has failed, showing window immediately.");
        }
        if (!HasTimer)
        {
            ShowWaitWindow(HWindow, HParent);
            SetVisible(TRUE);
        }
    }

    return HWindow;
}

void CListWaitWindow::SetVisible(BOOL visible)
{
    Visible = visible;
    if (Visible)
    {
        if (!HasRefreshStatusTimer)
        {
            HasRefreshStatusTimer = (SetTimer(HWindow, LISTWAITWND_AUTOUPDATETIMER,
                                              DATACON_ACTSPEEDSTEP, NULL) != 0); // zaridime timer od ted za 1 sekundu
        }
    }
    else
    {
        if (HasRefreshStatusTimer)
        {
            KillTimer(HWindow, LISTWAITWND_AUTOUPDATETIMER);
            HasRefreshStatusTimer = FALSE;
        }
    }
}

void CListWaitWindow::RefreshTimeAndStatusAndProgress(BOOL fromTimer)
{
    CALL_STACK_MESSAGE2("CListWaitWindow::RefreshTimeAndStatusAndProgress(%d)", fromTimer);

    if (*Aborted)
    { // pokud doslo k abortu, nebudeme jiz nic obnovovat (nesmyslne klesa rychlost prenosu, atd.)
        if (HasRefreshStatusTimer)
        {
            KillTimer(HWindow, LISTWAITWND_AUTOUPDATETIMER);
            HasRefreshStatusTimer = FALSE;
        }
        return;
    }

    int asciiTrForBinFileHowToSolve = 0;
    if (Visible && DataConnection->IsAsciiTrForBinFileProblem(&asciiTrForBinFileHowToSolve))
    {                                         // byl detekovan problem "ascii transfer mode for binary file", optame se co s tim
        if (asciiTrForBinFileHowToSolve == 0) // mame se zeptat uzivatele
        {
            Show(FALSE);
            INT_PTR res = CViewErrAsciiTrForBinFileDlg(HParent).Execute();
            if (res == IDOK)
                asciiTrForBinFileHowToSolve = 1;
            else
            {
                if (res == IDIGNORE)
                    asciiTrForBinFileHowToSolve = 3;
                else
                {
                    asciiTrForBinFileHowToSolve = 2;
                    SalamanderGeneral->WaitForESCRelease(); // opatreni, aby se neprerusovala dalsi akce po kazdem ESC v predeslem messageboxu
                }
            }
            DataConnection->SetAsciiTrModeForBinFileHowToSolve(asciiTrForBinFileHowToSolve);
            if (asciiTrForBinFileHowToSolve != 3)
            { // mame pouzit binarni rezim nebo zrusit viewovani, abortneme nasilne download (pokud nezabere, musi user dat ESC pro preruseni control-connectiony, to uz automaticky neresime, temer by to nemelo byt potreba)
                SetText(LoadStr(IDS_LISTWNDDOWNLFILEABORTING));
                if (DataConnection->IsTransfering(NULL) || DataConnection->IsFlushingDataToDisk())
                {
                    DataConnection->CancelConnectionAndFlushing(); // zavreme "data connection", system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
                    Logs.LogMessage(DataConnection->GetLogUID(), LoadStr(IDS_LOGMSGDATACONTERMINATED), -1, TRUE);
                }
                *Aborted = TRUE;
            }
            Show(TRUE);
        }
    }

    DWORD downloaded, total, connectionIdleTime, speed;
    DataConnection->StatusMessageReceived(); // posledni okamzik pred nactenim novych hodnot, dalsi zmeny uz je treba opet hlasit
    CQuadWord qwDownloaded, qwTotal;
    DataConnection->GetStatus(&qwDownloaded, &qwTotal, &connectionIdleTime, &speed);
    downloaded = (DWORD)qwDownloaded.Value; // listing nikdy nebude mit vic nez 2GB
    total = (DWORD)qwTotal.Value;           // listing nikdy nebude mit vic nez 2GB

    if (!fromTimer) // je-li  tento refresh na zaklade timeru, neni treba timer upravovat (automaticky se ozve zase za 1 sekundu)
    {
        if (HasRefreshStatusTimer)
            KillTimer(HWindow, LISTWAITWND_AUTOUPDATETIMER);
        if (Visible)
            HasRefreshStatusTimer = (SetTimer(HWindow, LISTWAITWND_AUTOUPDATETIMER, DATACON_ACTSPEEDSTEP, NULL) != 0); // zaridime timer od ted za 1 sekundu
        else
            HasRefreshStatusTimer = FALSE;
    }

    char num1[100];
    char num2[100];
    char num3[100];
    if (connectionIdleTime <= 30)
    {
        SalamanderGeneral->PrintDiskSize(num1, CQuadWord(speed, 0), 0);
        _snprintf_s(num3, _TRUNCATE, LoadStr(IDS_LISTWNDDOWNLOADSPEED), num1);
    }
    else
    {
        SalamanderGeneral->PrintTimeLeft(num1, CQuadWord(connectionIdleTime, 0));
        _snprintf_s(num3, _TRUNCATE, LoadStr(IDS_LISTWNDCONNECTIONIDLE), num1);
    }
    SalamanderGeneral->PrintDiskSize(num1, CQuadWord(downloaded, 0), 0); // pozor num1 pouzito u tvorby num3
    char buf[100];
    if (total != -1)
    {
        SalamanderGeneral->PrintDiskSize(num2, CQuadWord(total, 0), 0);
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LISTWNDSTATUS1), num1, num2, num3);
    }
    else
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LISTWNDSTATUS2), num1, num3);

    if (OperStatusText != NULL && strcmp(Status, buf) != 0)
    {
        strcpy(Status, buf);
        OperStatusText->SetText(Status);
    }

    DWORD ti = GetTickCount();
    if (ti - LastTickCountForElapsedTime >= 1000)
    {
        ti = (ti - LastTickCountForElapsedTime) / 1000;
        ElapsedTime += ti;
        LastTickCountForElapsedTime += ti * 1000;
    }
    SalamanderGeneral->PrintTimeLeft(buf, CQuadWord(ElapsedTime, 0));
    if (ElapsedTimeText != NULL && strcmp(TimeElapsed, buf) != 0)
    {
        strcpy(TimeElapsed, buf);
        ElapsedTimeText->SetText(TimeElapsed);
    }

    if (downloaded <= total && total != -1 && speed > 0)
    {
        DWORD secs = (total - downloaded) / speed; // odhad zbyvajicich sekund
        secs++;                                    // jedna vterina navic, abysme koncili operaci s "time left: 1 sec" (misto 0 sec)
        if (LastTimeEstimation != -1)
            secs = (2 * secs + LastTimeEstimation) / 3;
        // vypocet zaokrouhleni (zhruba 10% chyba + zaokrouhlujeme po hezkych cislech 1,2,5,10,20,40)
        int dif = (secs + 5) / 10;
        int expon = 0;
        while (dif >= 50)
        {
            dif /= 60;
            expon++;
        }
        if (dif <= 1)
            dif = 1;
        else if (dif <= 3)
            dif = 2;
        else if (dif <= 7)
            dif = 5;
        else if (dif < 15)
            dif = 10;
        else if (dif < 30)
            dif = 20;
        else
            dif = 40;
        while (expon--)
            dif *= 60;
        secs = ((secs + dif / 2) / dif) * dif; // zaokrouhlime 'secs' na 'dif' sekund
        SalamanderGeneral->PrintTimeLeft(buf, CQuadWord(secs, 0));
        LastTimeEstimation = secs;
    }
    else
        lstrcpyn(buf, LoadStr(IDS_LISTWNDESTIMTIMEUNKNOWN), 20);
    if (EstimatedTimeText != NULL && strcmp(TimeLeft, buf) != 0)
    {
        strcpy(TimeLeft, buf);
        EstimatedTimeText->SetText(TimeLeft);
    }

    if (OperProgressBar != NULL)
    {
        if (total != -1)
        {
            OperProgressBar->SetProgress(total == 0 ? 1000 : (int)((CQuadWord(downloaded <= total ? downloaded : total, 0) * CQuadWord(1000, 0)) / CQuadWord(total, 0)).Value, NULL);
        }
        else
        {
            if (!fromTimer)
                OperProgressBar->SetProgress(-1, NULL);
        }
    }
}

LRESULT
CListWaitWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CListWaitWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return CWindow::WindowProc(uMsg, wParam, lParam);

    case WM_TIMER:
    {
        if (wParam == LISTWAITWND_AUTOUPDATETIMER)
        {
            RefreshTimeAndStatusAndProgress(TRUE);
            return 0;
        }
        else
        {
            if (wParam == LISTWAITWND_DELAYEDUPDATETIMER) // provedeme zpozdeny refresh
            {
                if (NeedDelayedUpdate) // behem doby "hajeni" bylo potreba refreshnout, timer nechame
                {
                    NeedDelayedUpdate = FALSE;
                    RefreshTimeAndStatusAndProgress(FALSE);
                }
                else // behem doby "hajeni" nebylo potreba refreshnout, zrusime timer
                {
                    HasDelayedUpdateTimer = FALSE;
                    KillTimer(HWindow, LISTWAITWND_DELAYEDUPDATETIMER);
                }
                return 0;
            }
        }
        break;
    }

    case WM_APP_STATUSCHANGED:
    {
        if (!HasDelayedUpdateTimer)
        {
            RefreshTimeAndStatusAndProgress(FALSE); // provedeme refresh

            // dalsi refresh provedeme nejdrive za 1/10 sekundy
            HasDelayedUpdateTimer = (SetTimer(HWindow, LISTWAITWND_DELAYEDUPDATETIMER, 100, NULL) != 0);
            NeedDelayedUpdate = FALSE; // pro sychr
        }
        else
            NeedDelayedUpdate = TRUE;
        return 0;
    }

    case WM_DESTROY:
    {
        // pro jistotu zrusime timery
        if (HasRefreshStatusTimer)
            KillTimer(HWindow, LISTWAITWND_AUTOUPDATETIMER);
        if (HasDelayedUpdateTimer)
            KillTimer(HWindow, LISTWAITWND_DELAYEDUPDATETIMER);
        HasRefreshStatusTimer = HasDelayedUpdateTimer = FALSE;

        DataConnection->SetWindowWithStatus(NULL, 0);
        break; // odpojime okno od "data connection"
    }
    }
    return CWaitWindow::WindowProc(uMsg, wParam, lParam);
}
