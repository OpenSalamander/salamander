// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// COperationDlg
//

BOOL CALLBACK CorrectDisabledButtons(HWND hwnd, LPARAM lParam)
{ // obehneme disablovane buttony a kdyz narazime na "default" styl, zrusime ho (chyba windows, ze ho tam nechaly)
    BOOL cont = TRUE;
    if (hwnd != NULL && !IsWindowEnabled(hwnd))
    {
        char className[31];
        if (GetClassName(hwnd, className, 31) && _stricmp(className, "button") == 0)
        {
            LONG style = GetWindowLong(hwnd, GWL_STYLE);
            if ((style & BS_CHECKBOX) == 0 && (style & BS_DEFPUSHBUTTON) != 0)
            { // mame ho, zmenime ho na "ne-default" a tim koncime (dva defaultni byt nemuzou)
                PostMessage(hwnd, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
                cont = FALSE;
            }
        }
    }
    return cont;
}

INT_PTR
COperationDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("COperationDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    int sysCmdShowCmd; // pomocna promenna
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        BOOL preventSystemFromSettingFocus = FALSE;
        GetDlgItemText(HWindow, IDB_PAUSERESUME, PauseButtonPauseText, 50);
        GetDlgItemText(HWindow, IDB_OPCONSPAUSERESUME, ConPauseButtonPauseText, 50);
        LastFocusedControl = GetDlgItem(HWindow, IDB_SHOWDETAILS);
        CFTPOperationType operType = Oper->GetOperationType();
        if (operType == fotCopyDownload || operType == fotMoveDownload)
        {
            // nahodime thread pro ziskavani volneho mista na disku
            char pathBuf[MAX_PATH];
            Oper->GetTargetPath(pathBuf, MAX_PATH);
            GetDiskFreeSpaceThread = new CGetDiskFreeSpaceThread(pathBuf, HWindow);
            if (GetDiskFreeSpaceThread != NULL && GetDiskFreeSpaceThread->IsGood())
            {
                if (GetDiskFreeSpaceThread->Create(AuxThreadQueue) == NULL)
                {
                    delete GetDiskFreeSpaceThread; // thread se nepustil, error
                    GetDiskFreeSpaceThread = NULL;
                }
            }
            else
            {
                if (GetDiskFreeSpaceThread == NULL)
                    TRACE_E(LOW_MEMORY); // malo pameti, error
                else
                {
                    delete GetDiskFreeSpaceThread;
                    GetDiskFreeSpaceThread = NULL;
                }
            }
        }

        SendMessage(HWindow, WM_SETICON, ICON_BIG, (LPARAM)FTPOperIconBig);
        SendMessage(HWindow, WM_SETICON, ICON_SMALL, (LPARAM)FTPOperIcon);

        DlgWillCloseIfOpFinWithSkips = Config.CloseOperationDlgWhenOperFinishes ||
                                       Config.CloseOperationDlgIfSuccessfullyFinished;
        CheckDlgButton(HWindow, IDC_OPCLOSEWINWHENDONE,
                       DlgWillCloseIfOpFinWithSkips ? BST_CHECKED : BST_UNCHECKED);

        Source = SalamanderGUI->AttachStaticText(HWindow, IDT_OPSOURCETEXT, STF_PATH_ELLIPSIS);
        Target = SalamanderGUI->AttachStaticText(HWindow, IDT_OPTARGETTEXT, STF_PATH_ELLIPSIS);
        TimeLeft = SalamanderGUI->AttachStaticText(HWindow, IDT_OPTIMELEFT, 0);
        ElapsedTime = SalamanderGUI->AttachStaticText(HWindow, IDT_OPELAPSEDTIME, 0);
        Status = SalamanderGUI->AttachStaticText(HWindow, IDT_OPSTATUS, STF_END_ELLIPSIS);
        Progress = SalamanderGUI->AttachProgressBar(HWindow, IDC_OPPROGRESS);
        ConsListView = GetDlgItem(HWindow, IDL_CONNECTIONS);
        ItemsListView = GetDlgItem(HWindow, IDL_OPERATIONS);

        LowDiskSpaceHint = SalamanderGUI->AttachHyperLink(HWindow, IDT_ERRORMSG, STF_DOTUNDERLINE);
        SendDlgItemMessage(HWindow, IDI_ERRORICON, STM_SETICON, (WPARAM)WarningIcon, 0);

        char buf[100];
        if (GetWindowText(GetDlgItem(HWindow, IDT_OPERATIONSTEXT), buf, 100))
        {
            if (OperationsTextOrig != NULL)
                SalamanderGeneral->Free(OperationsTextOrig); // pro strycka prihodu...
            OperationsTextOrig = SalamanderGeneral->DupStr(buf);
        }

        if (Source == NULL || Target == NULL || TimeLeft == NULL || Status == NULL ||
            Progress == NULL || ConsListView == NULL || ItemsListView == NULL ||
            ElapsedTime == NULL || OperationsTextOrig == NULL ||
            !Oper->InitOperDlg(this))
        {
            DestroyWindow(HWindow); // chyba -> neotevreme dialog
            return FALSE;           // konec zpracovani
        }

        Progress->SetSelfMoveTime(OPERDLG_STATUSUPDATEPERIOD);
        Progress->SetSelfMoveSpeed(100);

        SetDlgTitle(-1, NULL);

        // init listviewu
        ListView_SetExtendedListViewStyle(ConsListView, ListView_GetExtendedListViewStyle(ConsListView) |
                                                            LVS_EX_FULLROWSELECT); // 4.70
        ConsListViewObj.Attach(ConsListView, this, TRUE);
        ListView_SetExtendedListViewStyle(ItemsListView, ListView_GetExtendedListViewStyle(ItemsListView) |
                                                             LVS_EX_FULLROWSELECT); // 4.70
        ItemsListViewObj.Attach(ItemsListView, this, FALSE);

        RECT r1, r2;
        GetWindowRect(HWindow, &r1);
        GetWindowRect(GetDlgItem(HWindow, IDC_DLGSPLITBAR), &r2);
        MinDlgHeight1 = r2.top - r1.top + 1;                 // vyska simple dialogu
        LastDlgHeight1 = MinDlgHeight2 = r1.bottom - r1.top; // vyska detailed dialogu
        MinDlgWidth = r1.right - r1.left;                    // sirka kompletniho dialogu

        GetClientRect(HWindow, &r1);
        MinClientHeight = r1.bottom - r1.top;
        GetWindowRect(GetDlgItem(HWindow, IDT_OPSOURCETEXT), &r2);
        SourceBorderWidth = r1.right - (r2.right - r2.left);
        SourceHeight = r2.bottom - r2.top;
        GetWindowRect(GetDlgItem(HWindow, IDC_OPPROGRESS), &r2);
        ProgressBorderWidth = r1.right - (r2.right - r2.left);
        ProgressHeight = r2.bottom - r2.top;
        GetWindowRect(GetDlgItem(HWindow, IDB_SHOWDETAILS), &r2);
        POINT p;
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        DetailsXROffset = r1.right - p.x;
        DetailsYOffset = p.y;
        GetWindowRect(GetDlgItem(HWindow, IDB_SHOWERRORS), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        NextErrXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDB_HIDE), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        HideXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDB_PAUSERESUME), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        PauseXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDCANCEL), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        CancelXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDHELP), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        HelpXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDI_ERRORICON), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        ErrIconXROffset = r1.right - p.x;
        ErrIconYOffset = p.y;
        RECT r3;
        GetWindowRect(GetDlgItem(HWindow, IDT_ERRORMSG), &r3);
        ErrIconToHintWidth = r3.right - r2.left;
        p.x = r3.left;
        p.y = r3.top;
        ScreenToClient(HWindow, &p);
        ErrHintXROffset = r1.right - p.x;
        ErrHintYOffset = p.y;
        GetWindowRect(GetDlgItem(HWindow, IDC_DLGSPLITBAR), &r2);
        SplitBorderWidth = r1.right - (r2.right - r2.left);
        SplitHeight = r2.bottom - r2.top;
        GetWindowRect(ConsListView, &r2);
        ConnectionsBorderWidth = r1.right - (r2.right - r2.left);
        ConnectionsActHeight = ConnectionsHeight = r2.bottom - r2.top;
        ConnectionsActWidth = r2.right - r2.left;
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        ConnectionsYOffset = p.y;
        GetWindowRect(GetDlgItem(HWindow, IDB_OPCONSADD), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        ConsAddXROffset = r1.right - p.x;
        ConsAddActYOffset = ConsAddYOffset = p.y;
        GetWindowRect(GetDlgItem(HWindow, IDB_OPCONSSOLVEERROR), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        ConsShowErrXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDB_OPCONSSTOP), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        ConsStopXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDB_OPCONSPAUSERESUME), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        ConsPauseXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDT_OPERATIONSTEXT), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        OperTxtXOffset = p.x;
        OperTxtYOffset = p.y;
        GetWindowRect(ItemsListView, &r2);
        OperationsBorderWidth = r1.right - (r2.right - r2.left);
        OperationsHeight = r2.bottom - r2.top;
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        OperationsXOffset = p.x;
        OperationsYOffset = p.y;
        ConnectionsActHeightLimit = OperationsHeight + ConnectionsHeight;
        OperationsEdges = r2.right - r2.left;
        GetClientRect(ItemsListView, &r2);
        OperationsEdges -= r2.right - r2.left;
        GetWindowRect(GetDlgItem(HWindow, IDB_OPOPERSSOLVEERROR), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        OpersShowErrXROffset = r1.right - p.x;
        OpersShowErrYOffset = p.y;
        GetWindowRect(GetDlgItem(HWindow, IDB_OPOPERSRETRY), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        OpersRetryXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDB_OPOPERSSKIP), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        OpersSkipXROffset = r1.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDB_OPOPERSSHOWONLYERR), &r2);
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(HWindow, &p);
        ShowOnlyErrXOffset = p.x;
        ShowOnlyErrYOffset = p.y;

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

        InitColumns();
        SetColumnWidths();
        RefreshConnections(TRUE);
        RefreshItems(TRUE);
        EnablePauseButton();
        EnableErrorsButton();

        CheckDlgButton(HWindow, IDB_OPOPERSSHOWONLYERR, ShowOnlyErrors ? BST_CHECKED : BST_UNCHECKED);

        ListviewSplitRatio = Config.OperDlgSplitPos;

        SetShowLowDiskWarning(ShowLowDiskWarning);

        if (Config.OperDlgPlacement.length != 0) // pozice je platna
        {
            BOOL oldSimpleLook = SimpleLook;
            SimpleLook = FALSE;
            WINDOWPLACEMENT place = Config.OperDlgPlacement;
            // GetWindowPlacement cti Taskbar, takze pokud je Taskbar nahore nebo vlevo,
            // jsou hodnoty posunute o jeho rozmery. Provedeme korekci.
            RECT monitorRect;
            RECT workRect;
            SalamanderGeneral->MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
            OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
                       workRect.top - monitorRect.top);
            SalamanderGeneral->MultiMonEnsureRectVisible(&place.rcNormalPosition, TRUE);

            int height = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
            MoveWindow(HWindow, place.rcNormalPosition.left, place.rcNormalPosition.top,
                       place.rcNormalPosition.right - place.rcNormalPosition.left, height, FALSE);
            if (LastDlgHeight1 < height)
                LastDlgHeight1 = height;
            SimpleLook = oldSimpleLook;
            if (CenterToWnd != NULL)
            {
                // horizontalni i vertikalni vycentrovani dialogu k 'CenterToWnd'
                SalamanderGeneral->MultiMonCenterWindow(HWindow, CenterToWnd, TRUE);
            }
            if (place.showCmd == SW_MAXIMIZE || place.showCmd == SW_SHOWMAXIMIZED)
            {
                if (SimpleLook)
                    ToggleSimpleLook(); // forcneme detailed mode
                else
                    ShowControlsAndChangeSize(SimpleLook); // ukazame/schovame "detailed" cast dialogu
                LastFocusedControl = GetDlgItem(HWindow, IDCANCEL);
                SetFocus(LastFocusedControl);
                preventSystemFromSettingFocus = TRUE;
                EnableWindow(GetDlgItem(HWindow, IDB_SHOWDETAILS), FALSE);
                ShowWindow(HWindow, SW_MAXIMIZE);
            }
            else
                ShowControlsAndChangeSize(SimpleLook); // ukazame/schovame "detailed" cast dialogu
        }
        else
        {
            if (CenterToWnd != NULL)
            {
                // horizontalni i vertikalni vycentrovani dialogu k 'CenterToWnd'
                SalamanderGeneral->MultiMonCenterWindow(HWindow, CenterToWnd, TRUE);
            }
            // ukazame/schovame "detailed" cast dialogu
            ShowControlsAndChangeSize(SimpleLook);
        }
        PostMessage(HWindow, WM_APP_ACTIVATEWORKERS, 0, 0);
        SetupCloseButton(FALSE);
#ifdef _DEBUG
        SetTimer(HWindow, OPERDLG_CHECKCOUNTERSTIMER, OPERDLG_CHECKCOUNTERSPERIOD, NULL);
#endif

        // nahodime pravidelny update progresu/statusu + prvni update naplanujeme hned
        SetTimer(HWindow, OPERDLG_STATUSUPDATETIMER, OPERDLG_STATUSUPDATEPERIOD, NULL);
        PostMessage(HWindow, WM_TIMER, OPERDLG_STATUSUPDATETIMER, 0);

        // nahodime timer pro test jestli se nema ukazat nejaky Show Error dialog
        SetTimer(HWindow, OPERDLG_AUTOSHOWERRTIMER, OPERDLG_AUTOSHOWERRPERIOD, NULL);
        SetTimer(HWindow, OPERDLG_AUTOSHOWERRTIMER2, 200, 0); // aby byla "okamzita" reakce na "rychle" chyby

        // FIXME: az budeme mit okno fronty operaci, zase muzeme Hide zviditelnit (+vyhodit "disabled" z resourcu)
        // zaroven pak zase vratit text v resourcich:  IDS_OPERDLGCONFIRMCANCEL, "Do you really want to cancel operation?\n\nNote: use Hide button to close window without cancelling operation."
        ShowWindow(GetDlgItem(HWindow, IDB_HIDE), SW_HIDE);

        if (preventSystemFromSettingFocus)
        {
            CDialog::DialogProc(uMsg, wParam, lParam);
            return FALSE; // nechceme od systemu nastavovat focus v dialogu
        }
        break;
    }

    case WM_APP_HAVEDISKFREESPACE:
    {
        if (GetDiskFreeSpaceThread != NULL)
        {
            CQuadWord freeSpace = GetDiskFreeSpaceThread->GetResult();
            BOOL showLowDiskWarning = FALSE;
            if (freeSpace != CQuadWord(-1, -1) &&           // pokud zname volne misto na cilovem disku
                LastNeededDiskSpace != CQuadWord(-1, -1) && // pokud zname potrebne misto na disku
                LastNeededDiskSpace > freeSpace)
            {
                char num1[100];
                char num2[100];
                char num3[100];
                showLowDiskWarning = TRUE;
                SalamanderGeneral->PrintDiskSize(num1, LastNeededDiskSpace, 1);
                SalamanderGeneral->PrintDiskSize(num2, freeSpace, 1);
                SalamanderGeneral->PrintDiskSize(num3, LastNeededDiskSpace - freeSpace, 1);
                char buf[300];
                _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LOWDISKSPACEONTGTPATH), num1, num2, num3);
                if (LowDiskSpaceHint != NULL)
                    LowDiskSpaceHint->SetActionShowHint(buf);
            }
            if (showLowDiskWarning != ShowLowDiskWarning)
            {
                SetShowLowDiskWarning(showLowDiskWarning);
                LayoutDialog(SizeBox != NULL ? IsWindowVisible(SizeBox) : 0);
            }
        }
        break;
    }

    case WM_APP_DISABLEDETAILED:
    {
        HWND details = GetDlgItem(HWindow, IDB_SHOWDETAILS);
        if (GetFocus() == details)
        {
            LastFocusedControl = GetDlgItem(HWindow, IDCANCEL);
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
        }
        EnableWindow(details, FALSE);
        return TRUE; // dale nezpracovavat
    }

    case WM_APP_ACTIVATEWORKERS:
    {
        WorkersList->ActivateWorkers();
        return TRUE; // dale nezpracovavat
    }

    case WM_APP_WORKERCHANGEREP:
    {
        if (GetTickCount() - LastUpdateOfProgressByWorker >= OPERDLG_STATUSMINIDLETIME)
            IsDirtyProgress = TRUE;
        IsDirtyConsListView = TRUE;
        ScheduleDelayedUpdate();
        return TRUE; // dale nezpracovavat
    }

    case WM_APP_ITEMCHANGEREP:
    {
        IsDirtyItemsListView = TRUE;
        ScheduleDelayedUpdate();
        return TRUE; // dale nezpracovavat
    }

    case WM_APP_OPERSTATECHANGE:
    {
        SetupCloseButton(TRUE);
        IsDirtyStatus = TRUE;
        ScheduleDelayedUpdate();
        return TRUE; // dale nezpracovavat
    }

    case WM_SYSCOMMAND:
    {
        HWND lastFocus = GetFocus();
        if (lastFocus != NULL)
            LastFocusedControl = lastFocus;
        LastActivityTime = GetTickCount();
        if (wParam == SC_MAXIMIZE || wParam == SC_MINIMIZE || wParam == SC_RESTORE ||
            wParam == SC_MAXIMIZE + 2 || wParam == SC_MINIMIZE + 2 || wParam == SC_RESTORE + 2)
        {
            DWORD cmd = (wParam & 0xFFFFFFF0);
            sysCmdShowCmd = SW_SHOWNORMAL;
            if (cmd == SC_MINIMIZE)
            {
                sysCmdShowCmd = SW_SHOWMINIMIZED;
                if (IsZoomed(HWindow))
                    RestoreToMaximized = TRUE;
            }
            if (cmd == SC_MAXIMIZE ||
                cmd == SC_RESTORE && RestoreToMaximized)
            {
                sysCmdShowCmd = SW_SHOWMAXIMIZED;
                RestoreToMaximized = FALSE;
                if (SimpleLook)
                    ToggleSimpleLook(); // v maximize jde jen detailed mode
                PostMessage(HWindow, WM_APP_DISABLEDETAILED, 0, 0);
            }
            else
            {
                if (cmd == SC_RESTORE && !RestoreToMaximized)
                    EnableWindow(GetDlgItem(HWindow, IDB_SHOWDETAILS), TRUE);
            }
        }
        else
        {
            if (wParam == SC_CLOSE)
            {
                PostMessage(HWindow, WM_COMMAND, IDCANCEL, 0);
                return TRUE; // dale nezpracovavat
            }
            break;
        }
        // break;  // neni tu schvalne, aby se ukladala pozice i pri maximize/restore/minimize
        // POZOR: prochazi tudy hodnota 'sysCmdShowCmd'
    }
    case WM_EXITSIZEMOVE:
    {
        HWND lastFocus = GetFocus();
        if (lastFocus != NULL)
            LastFocusedControl = lastFocus;
        LastActivityTime = GetTickCount();
        Config.OperDlgPlacement.length = sizeof(WINDOWPLACEMENT); // ulozime pozici okna
        GetWindowPlacement(HWindow, &Config.OperDlgPlacement);
        if (SimpleLook) // simple-look se nam nehodi, ukladame velikost otevreneho dialogu
            Config.OperDlgPlacement.rcNormalPosition.bottom = Config.OperDlgPlacement.rcNormalPosition.top + LastDlgHeight1;
        else
        {
            if (uMsg == WM_EXITSIZEMOVE && !IsZoomed(HWindow))
                LastDlgHeight1 = Config.OperDlgPlacement.rcNormalPosition.bottom - Config.OperDlgPlacement.rcNormalPosition.top;
        }
        if (uMsg == WM_SYSCOMMAND)
            Config.OperDlgPlacement.showCmd = sysCmdShowCmd;
        break;
    }

    case WM_APP_CLOSEDLG:
        wParam = IDCANCEL;
        // break;  // tady break nechybi!
    case WM_COMMAND:
    {
        if (IsWindowEnabled(HWindow))
        {
            char buf[200];
            if (uMsg == WM_COMMAND)
                SetUserWasActive();
            HWND lastFocus = GetFocus();
            if (lastFocus != NULL)
                LastFocusedControl = lastFocus;
            LastActivityTime = GetTickCount();
            switch (LOWORD(wParam))
            {
            case IDOK:
                return TRUE; // dale nezpracovavat, Enter nesmi ukoncovat dialog

            case IDC_OPCLOSEWINWHENDONE:
            {
                if (HIWORD(wParam) == BN_CLICKED)
                {
                    if (GetTickCount() - ClearChkboxTime < 300)
                    { // pokud doslo k vycisteni checkboxu kvuli kliknuti na checkbox, byl hned zase nastaven, takze ho musime zase vycistit
                        CheckDlgButton(HWindow, IDC_OPCLOSEWINWHENDONE, BST_UNCHECKED);
                    }
                    DlgWillCloseIfOpFinWithSkips = (IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED);
                    // zapamatujeme si novou hodnotu pro dalsi dialogy
                    // zakomentovano, protoze pamet tohoto checkboxu na me pusobi divne - smysl checkboxu ted vidim
                    // v tom, mit moznost nechat dialog zavrit po dokonceni operace (coz nezavisi na predchozi operaci)
                    // Config.CloseOperationDlgWhenOperFinishes = (IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED);
                }
                break;
            }

            case IDB_SHOWERRORS:
            case IDB_SHOWDETAILS:
            {
                if (LOWORD(wParam) == IDB_SHOWDETAILS || SimpleLook)
                    ToggleSimpleLook();
                if (LOWORD(wParam) == IDB_SHOWERRORS)
                {
                    if (ConErrorIndex != -1)
                    {
                        ListView_SetItemState(ConsListView, ConErrorIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                        ListView_EnsureVisible(ConsListView, ConErrorIndex, FALSE);
                    }
                    if (EnableShowOnlyErrors && !ShowOnlyErrors)
                    {
                        ShowOnlyErrors = TRUE;
                        CheckDlgButton(HWindow, IDB_OPOPERSSHOWONLYERR, BST_CHECKED);
                        RefreshItems(FALSE);
                        EnableErrorsButton();
                    }
                    SendMessage(HWindow, WM_NEXTDLGCTL,
                                (WPARAM)(ConErrorIndex != -1 ? ConsListView : ItemsListView), TRUE);
                }
                break;
            }

                // ***************************************************************************
                // POZOR: aby sla modalni okna "nenasilne" zavrit (napr. pri unloadu pluginu),
                //        musi vsechna otevrena modalni okna umet skoncit na WM_CLOSE
                //        (asi kdyz umi skoncit na klavesu Esc) + nesmi se vyvolavat
                //        vic modalnich oken po sobe, pokud predchazejici modalni
                //        okno bylo ukonceno pres Esc
                // ***************************************************************************

            case IDCANCEL:
            {
                COperationState state = Oper->GetOperationState(FALSE);
                switch (state)
                {
                case opstInProgress:
                case opstFinishedWithErrors:
                {
                    HWND focus = GetForegroundWindow() == HWindow ? GetFocus() : NULL;
                    DlgWillCloseIfOpFinWithSkips = FALSE;
                    int res = SalamanderGeneral->SalMessageBox(HWindow, LoadStr(state == opstInProgress ? IDS_OPERDLGCONFIRMCANCEL : IDS_OPERDLGCONFIRMCLOSE),
                                                               TitleText != NULL ? TitleText : "",
                                                               MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                                   MB_ICONQUESTION);
                    DlgWillCloseIfOpFinWithSkips = (IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED);
                    CorrectLookOfPrevFocusedDisabledButton(focus);
                    if (res != IDYES)
                        return TRUE; // zadny Cancel/Close nebude
                    break;
                }

                case opstFinishedWithSkips:
                case opstSuccessfullyFinished:
                    break; // umoznime tichy odchod
                }

                // ukoncime a zrusime vsechny workery operace
                int operUID = Oper->GetUID();
                FTPOperationsList.StopWorkers(HWindow, operUID, -1 /* vsechny workery */);

                SalamanderGeneral->WaitForESCRelease(); // opatreni, aby se neprerusovala dalsi akce pokud jsme pro Close dialogu pouzili ESC

                // zaridime uvolneni operace po zavreni dialogu operace
                if (!CanceledOperations.Add(operUID))
                    return TRUE;                                                      // nelze zrusit operaci, musime nechat dialog otevreny
                SalamanderGeneral->PostMenuExtCommand(FTPCMD_CANCELOPERATION, FALSE); // spustime prikaz co nejdrive
                break;
            }

            case IDB_HIDE:
            {
                PostMessage(HWindow, WM_CLOSE, 0, 0); // zavre okno, operace bezi dale
                return TRUE;                          // dale nezpracovavat
            }

            case IDB_PAUSERESUME:
            {
                if (IsWindowEnabled(HWindow))
                {
                    int operUID = Oper->GetUID();
                    FTPOperationsList.PauseWorkers(HWindow, operUID, -1, !PauseButtonIsResume); // pause/resume vsech workeru
                    RefreshConnections(FALSE);
                    EnablePauseButton();
                    EnableErrorsButton();
                    IsDirtyStatus = TRUE;
                    ScheduleDelayedUpdate(); // pokud jde o zacatek/konec "paused" rezimu, hned to vykreslime
                }
                return TRUE; // dale nezpracovavat
            }

            case IDB_OPOPERSSHOWONLYERR:
            {
                if (HIWORD(wParam) == BN_CLICKED)
                {
                    ShowOnlyErrors = IsDlgButtonChecked(HWindow, IDB_OPOPERSSHOWONLYERR) == BST_CHECKED;
                    RefreshItems(FALSE);
                    EnableErrorsButton();
                }
                break;
            }

            case IDB_OPOPERSSOLVEERROR:
            {
                HWND focus = GetForegroundWindow() == HWindow ? GetFocus() : NULL;
                SolveErrorOnItem(FocusedItemUID);
                CorrectLookOfPrevFocusedDisabledButton(focus);
                return TRUE; // dale nezpracovavat
            }

            case IDB_OPOPERSRETRY:
            {
                int changedIndex = Queue->RetryItem(FocusedItemUID, Oper);
                if (!ShowOnlyErrors)
                    RefreshItems(FALSE, changedIndex); // refreshneme zmenenou polozku
                else
                    RefreshItems(FALSE); // refreshneme vse (jsou-li zobrazeny jen chyby, polozka zmizne)
                EnableErrorsButton();
                WorkersList->PostNewWorkAvailable(TRUE); // informujeme pripadneho prvniho spiciho workera o pripadne nove praci (retry se nemusel povest)
                return TRUE;                             // dale nezpracovavat
            }

            case IDB_OPOPERSSKIP:
            {
                int changedIndex = Queue->SkipItem(FocusedItemUID, Oper);
                if (!ShowOnlyErrors)
                    RefreshItems(FALSE, changedIndex); // refreshneme zmenenou polozku
                else
                    RefreshItems(FALSE); // refreshneme vse (jinak by se musel dohledat index v listview)
                EnableErrorsButton();
                return TRUE; // dale nezpracovavat
            }

            case IDB_OPCONSSOLVEERROR:
            {
                int index = ListView_GetNextItem(ConsListView, -1, LVIS_FOCUSED);
                if (index >= 0)
                {
                    HWND focus = GetForegroundWindow() == HWindow ? GetFocus() : NULL;
                    SolveErrorOnConnection(index);
                    CorrectLookOfPrevFocusedDisabledButton(focus);
                }
                return TRUE; // dale nezpracovavat
            }

            case IDB_OPCONSADD:
            {
                CFTPWorker* newWorker = Oper->AllocNewWorker();
                if (newWorker != NULL)
                {
                    if (!SocketsThread->AddSocket(newWorker) ||   // pridani do sockets-threadu
                        !newWorker->RefreshCopiesOfUIDAndMsg() || // obnovime kopie UID+Msg (doslo k jejich zmene)
                        !Oper->AddWorker(newWorker))              // pridani mezi workery operace
                    {
                        DeleteSocket(newWorker);
                    }
                    else
                    {
                        RefreshConnections(FALSE, WorkersList->GetCount() - 1);
                        EnablePauseButton();
                        EnableErrorsButton();
                        IsDirtyStatus = TRUE;
                        ScheduleDelayedUpdate();      // pokud jde o konec "stopped" rezimu, hned to vykreslime
                        newWorker->PostActivateMsg(); // aktivujeme pridaneho workera
                    }
                }
                return TRUE; // dale nezpracovavat
            }

            case IDB_OPCONSSTOP:
            {
                if (IsWindowEnabled(HWindow))
                {
                    int index = ListView_GetNextItem(ConsListView, -1, LVIS_FOCUSED);
                    if (index >= 0)
                    {
                        sprintf(buf, LoadStr(IDS_WANTTOSTOPCON), WorkersList->GetWorkerID(index));
                        HWND focus = GetForegroundWindow() == HWindow ? GetFocus() : NULL;
                        DlgWillCloseIfOpFinWithSkips = FALSE;
                        int res = SalamanderGeneral->SalMessageBox(HWindow, buf,
                                                                   TitleText != NULL ? TitleText : "",
                                                                   MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                                       MSGBOXEX_SILENT | MB_ICONQUESTION);
                        CorrectLookOfPrevFocusedDisabledButton(focus);
                        if (res == IDYES)
                        {
                            int operUID = Oper->GetUID();
                            FTPOperationsList.StopWorkers(HWindow, operUID, index); // zastavime vybraneho workera
                            WorkersList->PostNewWorkAvailable(TRUE);                // informujeme pripadneho prvniho spiciho workera o pripadne vracene praci
                            RefreshConnections(FALSE);
                            EnablePauseButton();
                            EnableErrorsButton();
                            IsDirtyStatus = TRUE;
                            ScheduleDelayedUpdate(); // pokud jde o zacatek "stopped" rezimu, hned to vykreslime
                        }
                        DlgWillCloseIfOpFinWithSkips = (IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED);
                    }
                }
                return TRUE; // dale nezpracovavat
            }

            case IDB_OPCONSPAUSERESUME:
            {
                if (IsWindowEnabled(HWindow))
                {
                    int index = ListView_GetNextItem(ConsListView, -1, LVIS_FOCUSED);
                    if (index >= 0)
                    {
                        int operUID = Oper->GetUID();
                        FTPOperationsList.PauseWorkers(HWindow, operUID, index, !ConPauseButtonIsResume); // pause/resume vybraneho workera
                        RefreshConnections(FALSE, -1, index);
                        EnablePauseButton();
                        EnableErrorsButton();
                        IsDirtyStatus = TRUE;
                        ScheduleDelayedUpdate(); // pokud jde o zacatek/konec "paused" rezimu, hned to vykreslime
                    }
                }
                return TRUE; // dale nezpracovavat
            }
            }
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (((LPNMHDR)lParam)->code == HDN_ENDTRACK)
        {
            ConsListViewObj.HideToolTip();
            ItemsListViewObj.HideToolTip();
        }

        if (wParam == IDL_OPERATIONS)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case LVN_GETDISPINFO:
            {
                int index = ((NMLVDISPINFO*)lParam)->item.iItem;
                if (ShowOnlyErrors && index >= 0 && index < ErrorsIndexes.Count)
                    index = ErrorsIndexes[index];
                Queue->GetListViewDataFor(index, (NMLVDISPINFO*)lParam, ItemsTextBuf[ItemsActTextBuf], OPERDLG_ITEMSTEXTBUFSIZE);
                if (++ItemsActTextBuf > 2)
                    ItemsActTextBuf = 0;
                return FALSE; // zpracovavat dale
            }

            case LVN_ITEMCHANGED:
            {
                if (EnableChangeFocusedItemUID)
                {
                    LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                    if (nmhi->uNewState & LVIS_FOCUSED)
                    {
                        SetUserWasActive();
                        LastActivityTime = GetTickCount();
                        EnableRetryItem(nmhi->iItem);
                        int focusIndex = nmhi->iItem;
                        if (ShowOnlyErrors && focusIndex >= 0 && focusIndex < ErrorsIndexes.Count)
                            focusIndex = ErrorsIndexes[focusIndex];
                        FocusedItemUID = Queue->GetItemUID(focusIndex);
                    }
                    ItemsListViewObj.HideToolTip();
                }
                break;
            }

            case LVN_ODFINDITEM:
            {
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, -1);
                return TRUE; // nezpracovavat dale
            }

            case NM_DBLCLK:
            {
                if (IsWindowEnabled(GetDlgItem(HWindow, IDB_OPOPERSSOLVEERROR)))
                    PostMessage(HWindow, WM_COMMAND, IDB_OPOPERSSOLVEERROR, 0);
                break;
            }
            }
        }
        else
        {
            if (wParam == IDL_CONNECTIONS)
            {
                LPNMHDR nmh = (LPNMHDR)lParam;
                switch (nmh->code)
                {
                case LVN_GETDISPINFO:
                {
                    WorkersList->GetListViewDataFor(((NMLVDISPINFO*)lParam)->item.iItem, (NMLVDISPINFO*)lParam,
                                                    ConsTextBuf[ConsActTextBuf], OPERDLG_CONSTEXTBUFSIZE);
                    if (++ConsActTextBuf > 2)
                        ConsActTextBuf = 0;
                    return FALSE; // zpracovavat dale
                }

                case LVN_ITEMCHANGED:
                {
                    if (EnableChangeFocusedCon)
                    {
                        LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                        if (nmhi->uNewState & LVIS_FOCUSED)
                        {
                            SetUserWasActive();
                            LastActivityTime = GetTickCount();
                            int logUID = WorkersList->GetLogUID(nmhi->iItem);
                            if (logUID != -1 && Config.AlwaysShowLogForActPan)
                                Logs.ActivateLog(logUID);
                            EnableSolveConError(nmhi->iItem);
                            EnablePauseConButton(nmhi->iItem);
                        }
                        ConsListViewObj.HideToolTip();
                    }
                    break;
                }

                case NM_SETFOCUS:
                {
                    int index = ListView_GetNextItem(ConsListView, -1, LVIS_FOCUSED);
                    int logUID = WorkersList->GetLogUID(index);
                    if (logUID != -1 && Config.AlwaysShowLogForActPan)
                        Logs.ActivateLog(logUID);
                    break;
                }

                case LVN_ODFINDITEM:
                {
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, -1);
                    return TRUE; // nezpracovavat dale
                }

                case NM_DBLCLK:
                {
                    if (IsWindowEnabled(GetDlgItem(HWindow, IDB_OPCONSSOLVEERROR)))
                        PostMessage(HWindow, WM_COMMAND, IDB_OPCONSSOLVEERROR, 0);
                    break;
                }
                }
            }
        }
        break;
    }

    case WM_ACTIVATE:
    {
        if (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE)
        {
            LastGetDiskFreeSpace = GetTickCount() - OPERDLG_GETDISKSPACEPERIOD;
            IsDirtyStatus = TRUE;
            ScheduleDelayedUpdate();
            SetTimer(HWindow, OPERDLG_CORRECTBTNSTIMER, 100, 0); // pockame az se dialog aktivuje, pak zkontrolujeme stav tlacitek (disablovane s rameckem nechceme)
        }

        if (wParam == WA_INACTIVE &&
            (ConsListViewObj.HToolTip == NULL || (HWND)lParam != ConsListViewObj.HToolTip) && // deaktivace prepnutim do tooltipu zlobila, deaktivovalo se okno operacniho dialogu + ukazoval se nechtene Solve Error dialog
            (ItemsListViewObj.HToolTip == NULL || (HWND)lParam != ItemsListViewObj.HToolTip))
        {
            ConsListViewObj.HideToolTip();
            ItemsListViewObj.HideToolTip();
            SetTimer(HWindow, OPERDLG_AUTOSHOWERRTIMER2, 100, 0); // aby byla "okamzita" reakce na deaktivaci dialogu operace
        }
        break;
    }

    case WM_TIMER:
    {
        switch (wParam)
        {
        case OPERDLG_CORRECTBTNSTIMER:
        {
            KillTimer(HWindow, OPERDLG_CORRECTBTNSTIMER);
            HWND focus = GetForegroundWindow() == HWindow ? GetFocus() : NULL;
            if (focus != NULL)
            {
                char className[31];
                if (GetClassName(focus, className, 31) && _stricmp(className, "button") == 0)
                {
                    SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)focus, TRUE);
                    EnumChildWindows(HWindow, CorrectDisabledButtons, NULL);
                }
            }
            break;
        }

        case OPERDLG_UPDATETIMER: // provedeme zpozdeny refresh
        {
            if (!UpdateDataInDialog())
            { // behem doby "hajeni" nebylo potreba refreshnout, zrusime timer
                HasDelayedUpdateTimer = FALSE;
                KillTimer(HWindow, OPERDLG_UPDATETIMER);
            }
            return TRUE; // dale nezpracovavat
        }

#ifdef _DEBUG
        case OPERDLG_CHECKCOUNTERSTIMER:
        {
            KillTimer(HWindow, OPERDLG_CHECKCOUNTERSTIMER);
            //        TRACE_I("Calling: Queue->DebugCheckCounters()...");
            DWORD ti = GetTickCount();
            Queue->DebugCheckCounters(Oper);
            DWORD timeout = 10 * (GetTickCount() - ti); // aspon 10x vic casu at to maka nez kontroluje, at se to samo nezahlti
                                                        //        TRACE_I("Finished: Queue->DebugCheckCounters()...");
            SetTimer(HWindow, OPERDLG_CHECKCOUNTERSTIMER, max(timeout, OPERDLG_CHECKCOUNTERSPERIOD), NULL);
            break;
        }
#endif

        case OPERDLG_STATUSUPDATETIMER:
        {
            if (GetTickCount() - LastUpdateOfProgressByWorker >= OPERDLG_STATUSMINIDLETIME)
            {
                IsDirtyStatus = TRUE;
                ScheduleDelayedUpdate();
            }
            break;
        }

        case OPERDLG_AUTOSHOWERRTIMER2:
            KillTimer(HWindow, OPERDLG_AUTOSHOWERRTIMER2); // tady break nechybi!
        case OPERDLG_AUTOSHOWERRTIMER:
        {
            if (Config.OpenSolveErrIfIdle &&
                IsWindowEnabled(HWindow) &&                                // jen pokud uz neni otevrene jine modalni okno
                !ConsListViewObj.Scrolling && !ItemsListViewObj.Scrolling) // a jen pokud user zrovna netaha scrollbaru ani jednoho listview
            {
                BOOL dlgIsInactive = GetForegroundWindow() != HWindow;
                if (!UserWasActive && !DelayAfterCancel || dlgIsInactive || GetTickCount() - LastActivityTime >= OPERDLG_SHOWERRMINIDLETIME)
                {
                    DelayAfterCancel = FALSE;
                    int workerIndex = -1;
                    int itemUID = -1;
                    int itemIndex = -1;
                    if (Oper->SearchWorkerWithNewError(&workerIndex) ||
                        Queue->SearchItemWithNewError(&itemUID, &itemIndex))
                    {
                        /*  // zakomentovano, aby Solve Error dialogy nevypinaly auto-close okna operace
                if (!UserWasActive)
                {
                  SetUserWasActive();
                  LastActivityTime = GetTickCount() - OPERDLG_SHOWERRMINIDLETIME;  // simulace "idle"
                }
*/
                        KillTimer(HWindow, OPERDLG_AUTOSHOWERRTIMER);
                        if (IsIconic(HWindow)) // minimalizovane okno musim restornout (bez aktivace), jinak nebude po aktivaci/zavreni Solve Error dialogu videt (bude stale minimalizovane)
                            ShowWindow(HWindow, SW_SHOWNOACTIVATE);
                        if (SimpleLook)
                            ToggleSimpleLook(); // dialog musime zobrazit v Detailed podobe
                        if (workerIndex != -1)
                        {
                            // provedeme focus workera, u ktereho jdeme resit chybu
                            BOOL oldEnableChangeFocusedCon = EnableChangeFocusedCon;
                            EnableChangeFocusedCon = FALSE;
                            ListView_SetItemState(ConsListView, workerIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                            ListView_EnsureVisible(ConsListView, workerIndex, FALSE);
                            EnableChangeFocusedCon = oldEnableChangeFocusedCon;
                            EnableSolveConError(-1);
                            EnablePauseConButton(-1);
                            SolveErrorOnConnection(workerIndex);
                        }
                        else
                        {
                            if (itemUID != -1)
                            {
                                if (ShowOnlyErrors)
                                {
                                    int i;
                                    for (i = 0; i < ErrorsIndexes.Count; i++)
                                    {
                                        if (ErrorsIndexes[i] == (DWORD)itemIndex)
                                        {
                                            itemIndex = i;
                                            break;
                                        }
                                    }
                                    if (i == ErrorsIndexes.Count)
                                        itemIndex = -1;
                                }
                                if (itemIndex != -1) // provedeme focus polozky, u ktere jdeme resit chybu
                                {
                                    BOOL oldEnableChangeFocusedItemUID = EnableChangeFocusedItemUID;
                                    EnableChangeFocusedItemUID = FALSE;
                                    ListView_SetItemState(ItemsListView, itemIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                                    ListView_EnsureVisible(ItemsListView, itemIndex, FALSE);
                                    int focusIndex = itemIndex;
                                    if (ShowOnlyErrors && itemIndex < ErrorsIndexes.Count)
                                        focusIndex = ErrorsIndexes[itemIndex];
                                    FocusedItemUID = Queue->GetItemUID(focusIndex);
                                    EnableChangeFocusedItemUID = oldEnableChangeFocusedItemUID;
                                    EnableRetryItem(-1);
                                }
                                SolveErrorOnItem(itemUID);
                            }
                            else
                                TRACE_E("Unexpected situation in COperationDlg::DialogProc::OPERDLG_AUTOSHOWERRTIMER!");
                        }
                        SetTimer(HWindow, OPERDLG_AUTOSHOWERRTIMER2, 100, 0);                         // aby byla "okamzita" reakce na dalsi chybu (PostMessage zde nejde, protoze dalsi okno z nejakeho duvodu pipa, nevyzkoumal jsem proc, zpusobuje to volani WorkersList->PostNewWorkAvailable)
                        SetTimer(HWindow, OPERDLG_AUTOSHOWERRTIMER, OPERDLG_AUTOSHOWERRPERIOD, NULL); // obnovime pravidelne testovani chyb
                    }
                }
            }
            break;
        }
        }
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        DWORD color = GetSysColor(COLOR_WINDOW);
        ListView_SetBkColor(ConsListView, color);
        ListView_SetBkColor(ItemsListView, color);
        break;
    }

    case WM_QUERYDRAGICON:
        return (BOOL)(INT_PTR)FTPOperIconBig; // nejpis zbytecne

    case WM_SIZE:
        LayoutDialog(wParam == SIZE_RESTORED);
        break;

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinDlgWidth;
        lpmmi->ptMinTrackSize.y = SimpleLook ? MinDlgHeight1 : MinDlgHeight2;
        if (SimpleLook)
            lpmmi->ptMaxTrackSize.y = MinDlgHeight1;
        break;
    }

    case WM_CTLCOLORBTN:
    {
        if (GetForegroundWindow() == HWindow)
        {
            HWND lastFocus = GetFocus();
            if (lastFocus != NULL && LastFocusedControl != lastFocus)
            {
                SetUserWasActive();
                LastActivityTime = GetTickCount();
            }
            if (lastFocus != NULL)
                LastFocusedControl = lastFocus;
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (Captured)
        {
            int height = ConnectionsActHeight + (y - DragOriginY);
            if (height < 0)
                height = 0;
            if (height > ConnectionsActHeightLimit)
                height = ConnectionsActHeightLimit;
            double oldListviewSplitRatio = ListviewSplitRatio;
            if (ConnectionsActHeightLimit > 0)
                ListviewSplitRatio = (double)height / ConnectionsActHeightLimit;
            else
                ListviewSplitRatio = 0.5;

            if (!SimpleLook)
            {
                RECT clientRect;
                GetClientRect(HWindow, &clientRect);
                HDWP hdwp = HANDLES(BeginDeferWindowPos(7));
                if (hdwp != NULL)
                {
                    int dy = ConnectionsHeight + OperationsHeight + clientRect.bottom - MinClientHeight;
                    int yDiff = (int)(ListviewSplitRatio * dy) - ConnectionsHeight;
                    if (yDiff < 0)
                        yDiff = 0;
                    if (clientRect.bottom - MinClientHeight - yDiff < 0)
                        yDiff = clientRect.bottom - MinClientHeight;

                    // prepocitame pomer zpet podle realne situace (aby to pri resize neblblo)
                    if (dy > 0 /* always true */)
                        ListviewSplitRatio = (yDiff + ConnectionsHeight) / (double)dy;

                    int yDiffRest = clientRect.bottom - MinClientHeight - yDiff;
                    hdwp = HANDLES(DeferWindowPos(hdwp, ConsListView, NULL,
                                                  0, 0, clientRect.right - ConnectionsBorderWidth, ConnectionsHeight + yDiff,
                                                  SWP_NOZORDER | SWP_NOMOVE));
                    hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_OPCONSSOLVEERROR), NULL,
                                                  clientRect.right - ConsShowErrXROffset, ConsAddYOffset + yDiff, 0, 0,
                                                  SWP_NOZORDER | SWP_NOSIZE));
                    hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_OPCONSADD), NULL,
                                                  clientRect.right - ConsAddXROffset, ConsAddYOffset + yDiff, 0, 0,
                                                  SWP_NOZORDER | SWP_NOSIZE));
                    hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_OPCONSSTOP), NULL,
                                                  clientRect.right - ConsStopXROffset, ConsAddYOffset + yDiff, 0, 0,
                                                  SWP_NOZORDER | SWP_NOSIZE));
                    hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_OPCONSPAUSERESUME), NULL,
                                                  clientRect.right - ConsPauseXROffset, ConsAddYOffset + yDiff, 0, 0,
                                                  SWP_NOZORDER | SWP_NOSIZE));
                    hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDT_OPERATIONSTEXT), NULL,
                                                  OperTxtXOffset, OperTxtYOffset + yDiff, 0, 0,
                                                  SWP_NOZORDER | SWP_NOSIZE));
                    hdwp = HANDLES(DeferWindowPos(hdwp, ItemsListView, NULL,
                                                  OperationsXOffset, OperationsYOffset + yDiff,
                                                  clientRect.right - OperationsBorderWidth, OperationsHeight + yDiffRest,
                                                  SWP_NOZORDER));

                    // aby neproblikavaly scrollbary v listviewech pri zmensovani okna
                    SendMessage(ConsListView, WM_SETREDRAW, FALSE, 0);
                    SendMessage(ItemsListView, WM_SETREDRAW, FALSE, 0);

                    HANDLES(EndDeferWindowPos(hdwp)); // provedeme premisteni listviewu a tlacitek

                    SendMessage(ConsListView, WM_SETREDRAW, TRUE, 0);
                    SendMessage(ItemsListView, WM_SETREDRAW, TRUE, 0);
                }
            }
            if (ListviewSplitRatio != oldListviewSplitRatio)
                SetUserWasActive();
            return TRUE; // dale nezpracovavat
        }
        InListViewSplit = !SimpleLook &&
                          (x >= OperationsXOffset && x <= OperationsXOffset + ConnectionsActWidth &&
                           y > ConnectionsYOffset + ConnectionsActHeight && y < ConsAddActYOffset);
        break;
    }

    case WM_SETCURSOR:
    {
        if (InListViewSplit)
        {
            DWORD pos = GetMessagePos(); // provedeme detekci znovu, mys nad buttony negeneruje WM_MOUSEMOVE
            POINT p;
            p.x = GET_X_LPARAM(pos);
            p.y = GET_Y_LPARAM(pos);
            ScreenToClient(HWindow, &p);
            InListViewSplit = !SimpleLook &&
                              (p.x >= OperationsXOffset && p.x <= OperationsXOffset + ConnectionsActWidth &&
                               p.y > ConnectionsYOffset + ConnectionsActHeight && p.y < ConsAddActYOffset);
            if (InListViewSplit)
            {
                SetCursor(LoadCursor(NULL, IDC_SIZENS));
                return TRUE; // dale nezpracovavat
            }
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        HWND lastFocus = GetFocus();
        if (lastFocus != NULL)
            LastFocusedControl = lastFocus;
        LastActivityTime = GetTickCount();
        if (InListViewSplit)
        {
            SetCapture(HWindow);
            Captured = TRUE;
            DragOriginY = GET_Y_LPARAM(lParam);
            return TRUE; // dale nezpracovavat
        }
        break;
    }

    case WM_LBUTTONUP:
    case WM_CANCELMODE:
    {
        if (Captured)
        {
            ReleaseCapture();
            Captured = FALSE;

            Config.OperDlgSplitPos = ListviewSplitRatio;

            RECT r;
            GetWindowRect(ConsListView, &r);
            int yDiff = (r.bottom - r.top) - ConnectionsHeight;
            ConnectionsActHeight = ConnectionsHeight + yDiff;
            ConsAddActYOffset = ConsAddYOffset + yDiff;
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
        DestroyWindow(HWindow); // zrusime okno (std. varianta se snazi o IDCANCEL, to se nam nehodi)
        return TRUE;            // dale nezpracovavat
    }

    case WM_DESTROY:
    {
        if (HasDelayedUpdateTimer)
            KillTimer(HWindow, OPERDLG_UPDATETIMER);
        HasDelayedUpdateTimer = FALSE;

        ConsListViewObj.DetachWindow();
        ItemsListViewObj.DetachWindow();

        // uvolnime image-list z list-view, chceme image-list uvolnit sami
        ListView_SetImageList(ConsListView, NULL, LVSIL_SMALL);
        if (ConsImageList != NULL)
        {
            ImageList_Destroy(ConsImageList);
            ConsImageList = NULL; // simulujeme stav, kdy se imagelist nepodarilo vytvorit
        }
        ListView_SetImageList(ItemsListView, NULL, LVSIL_SMALL);
        if (ItemsImageList != NULL)
        {
            ImageList_Destroy(ItemsImageList);
            ItemsImageList = NULL; // simulujeme stav, kdy se imagelist nepodarilo vytvorit
        }

        if (!CloseDlg)
            Oper->SetOperationDlg(NULL); // od ted jiz objekt dialogu neni platny
        PostQuitMessage(0);
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void COperationDlg::SetupCloseButton(BOOL flashTitle)
{
    BOOL winStaysOpened = TRUE;
    COperationState state = Oper->GetOperationState(TRUE);
    switch (state)
    {
    case opstInProgress:
        SetDlgItemText(HWindow, IDCANCEL, LoadStr(IDS_OPERDLGCLOSEBUTTON2));
        break;

    case opstFinishedWithSkips:
    case opstSuccessfullyFinished:
        if (Config.CloseOperationDlgIfSuccessfullyFinished && !UserWasActive ||
            IsWindowEnabled(HWindow) && // pokud neni zrovna otevreny nejaky Solve Error dialog, zavreme na prani usera dialog
                IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED)
        {
            winStaysOpened = FALSE;
            PostMessage(HWindow, WM_APP_CLOSEDLG, 0, 0);
        }
        else // okno zustane otevrene
        {
            if (state == opstSuccessfullyFinished && Queue->AllItemsDone()) // vse je "done" -> ukoncime vsechny workery operace, uz nebudou k nicemu potreba
            {
                int operUID = Oper->GetUID();
                FTPOperationsList.StopWorkers(HWindow, operUID, -1 /* vsechny workery */);
                DisableAddWorkerButton = TRUE;

                // Add button disablujeme hned
                HWND button = GetDlgItem(HWindow, IDB_OPCONSADD);
                if (GetFocus() == button)
                    SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
                EnableWindow(button, FALSE);

                // disablujeme checkbox "Close this dialog when operation finishes"
                button = GetDlgItem(HWindow, IDC_OPCLOSEWINWHENDONE);
                if (GetFocus() == button)
                    SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
                EnableWindow(button, FALSE);
            }
        }
        // tady umyslne neni break;
    case opstFinishedWithErrors:
    {
        SetDlgItemText(HWindow, IDCANCEL, LoadStr(IDS_OPERDLGCLOSEBUTTON1));
        if (flashTitle)
        {
            HWND wnd = GetForegroundWindow();
            HWND parent;
            while (wnd != HWindow && (parent = GetParent(wnd)) != NULL)
                wnd = parent;
            if (wnd != HWindow)
            {
                if (winStaysOpened)
                    FlashWindow(HWindow, TRUE);
                if (InactiveBeepWhenDone)
                    MessageBeep(0);
            }
        }
        break;
    }
    }
}
