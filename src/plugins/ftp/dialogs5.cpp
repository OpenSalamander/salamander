// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// COperationDlg
//

COperationDlg::COperationDlg(HWND parent, HWND centerToWnd, CFTPOperation* oper,
                             CFTPQueue* queue, CFTPWorkersList* workersList)
    : CDialog(HLanguage, IDD_OPERATIONDLG, IDD_OPERATIONDLG, parent, ooStatic), ErrorsIndexes(10, 200)
{
    Oper = oper;
    Queue = queue;
    WorkersList = workersList;
    SendWMClose = NULL;
    CloseDlg = FALSE;
    DlgWillCloseIfOpFinWithSkips = FALSE;

    CenterToWnd = centerToWnd;
    SizeBox = NULL;
    Source = NULL;
    Target = NULL;
    TimeLeft = NULL;
    ElapsedTime = NULL;
    Status = NULL;
    Progress = NULL;
    ConsListView = NULL;
    ItemsListView = NULL;

    ConsTextBuf[0][0] = 0;
    ConsTextBuf[1][0] = 0;
    ConsTextBuf[2][0] = 0;
    ConsActTextBuf = 0;
    ItemsTextBuf[0][0] = 0;
    ItemsTextBuf[1][0] = 0;
    ItemsTextBuf[2][0] = 0;
    ItemsActTextBuf = 0;

    ConsImageList = ImageList_Create(16, 16, ILC_MASK | SalamanderGeneral->GetImageListColorFlags(), 1, 0);
    if (ConsImageList != NULL)
    {
        ImageList_SetImageCount(ConsImageList, 1); // inicializace
        ImageList_ReplaceIcon(ConsImageList, 0, FTPIcon);
    }

    ItemsImageList = ImageList_Create(16, 16, ILC_MASK | SalamanderGeneral->GetImageListColorFlags(), 2, 0);
    if (ItemsImageList != NULL)
    {
        ImageList_SetImageCount(ItemsImageList, 2); // inicializace
        const char* Shell32DLLName = "shell32.dll";
        HINSTANCE iconsDLL;
        if (WindowsVistaAndLater)
            iconsDLL = HANDLES(LoadLibraryEx("imageres.dll", NULL, LOAD_LIBRARY_AS_DATAFILE));
        else
            iconsDLL = HANDLES(LoadLibraryEx(Shell32DLLName, NULL, LOAD_LIBRARY_AS_DATAFILE));
        BOOL err = FALSE;
        if (iconsDLL != NULL)
        {
            int i;
            for (i = 0; i < 2; i++)
            {
                int resID = (i == 0 ? 4 /* directory */ : 1 /* non-assoc. file */);
                int vistaResID = (i == 0 ? 4 /* directory */ : 2 /* non-assoc. file */);
                HICON hIcon = (HICON)HANDLES(LoadImage(iconsDLL, MAKEINTRESOURCE(WindowsVistaAndLater ? vistaResID : resID),
                                                       IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags()));
                if (hIcon != NULL)
                {
                    ImageList_ReplaceIcon(ItemsImageList, i, hIcon);
                    HANDLES(DestroyIcon(hIcon));
                }
                else
                    err = TRUE;
            }
            HANDLES(FreeLibrary(iconsDLL));
        }
        else
            err = TRUE;
        if (err)
        {
            ImageList_Destroy(ItemsImageList);
            ItemsImageList = NULL;
        }
    }

    SimpleLook = TRUE;

    TitleText = NULL;

    IsDirtyStatus = FALSE;
    IsDirtyProgress = FALSE;
    LastUpdateOfProgressByWorker = GetTickCount() - OPERDLG_STATUSUPDATEPERIOD;
    IsDirtyConsListView = FALSE;
    IsDirtyItemsListView = FALSE;
    HasDelayedUpdateTimer = FALSE;

    ConErrorIndex = -1;
    EnableChangeFocusedCon = TRUE;

    ShowOnlyErrors = FALSE;
    EnableShowOnlyErrors = FALSE;

    FocusedItemUID = -1;
    EnableChangeFocusedItemUID = TRUE;

    UserWasActive = FALSE;
    DelayAfterCancel = FALSE;
    CloseDlgWhenOperFin = Config.CloseOperationDlgWhenOperFinishes;
    ClearChkboxTime = GetTickCount() - 1000;
    LastFocusedControl = NULL;
    LastActivityTime = 0;

    LastTimeEstimation = -1;

    OperationsTextOrig = NULL;
    DisplayedDoneOrSkippedCount = -1;
    DisplayedTotalCount = -1;

    DisableAddWorkerButton = FALSE;

    ShowLowDiskWarning = FALSE;
    LastNeededDiskSpace.Set(0, 0);
    LastGetDiskFreeSpace = GetTickCount() - OPERDLG_GETDISKSPACEPERIOD;
    LowDiskSpaceHint = NULL;
    GetDiskFreeSpaceThread = NULL;

    CurrentFlashWnd = NULL;

    ProgressValue = 0;

    MinDlgHeight1 = 0;
    MinDlgHeight2 = 0;
    MinDlgWidth = 0;
    LastDlgHeight1 = 0;
    MinClientHeight = 0;
    SizeBoxWidth = 0;
    SizeBoxHeight = 0;
    SourceBorderWidth = 0;
    SourceHeight = 0;
    ProgressBorderWidth = 0;
    ProgressHeight = 0;
    DetailsXROffset = 0;
    DetailsYOffset = 0;
    NextErrXROffset = 0;
    HideXROffset = 0;
    PauseXROffset = 0;
    CancelXROffset = 0;
    HelpXROffset = 0;
    ErrIconXROffset = 0;
    ErrIconYOffset = 0;
    ErrHintXROffset = 0;
    ErrHintYOffset = 0;
    ErrIconToHintWidth = 0;
    SplitBorderWidth = 0;
    SplitHeight = 0;
    ConnectionsBorderWidth = 0;
    ConnectionsHeight = 0;
    ConnectionsYOffset = 0;
    ConsAddXROffset = 0;
    ConsAddYOffset = 0;
    ConsShowErrXROffset = 0;
    ConsStopXROffset = 0;
    ConsPauseXROffset = 0;
    OperTxtXOffset = 0;
    OperTxtYOffset = 0;
    OperationsBorderWidth = 0;
    OperationsHeight = 0;
    OperationsXOffset = 0;
    OperationsYOffset = 0;
    OperationsEdges = 0;
    OpersShowErrXROffset = 0;
    OpersShowErrYOffset = 0;
    OpersRetryXROffset = 0;
    OpersSkipXROffset = 0;
    ShowOnlyErrXOffset = 0;
    ShowOnlyErrYOffset = 0;

    ConnectionsActWidth = 0;
    ConnectionsActHeight = 0;
    ConsAddActYOffset = 0;
    ConnectionsActHeightLimit = 0;
    InListViewSplit = FALSE;
    Captured = FALSE;
    DragOriginY = 0;
    ListviewSplitRatio = 0.5;

    RestoreToMaximized = FALSE;

    PauseButtonIsEnabled = TRUE;
    PauseButtonIsResume = FALSE;
    PauseButtonPauseText[0] = 0; // plni se az v WM_INITDIALOG
    ConPauseButtonIsResume = FALSE;
    ConPauseButtonPauseText[0] = 0; // plni se az v WM_INITDIALOG
}

COperationDlg::~COperationDlg()
{
    if (GetDiskFreeSpaceThread != NULL)
    {
        HANDLE t = GetDiskFreeSpaceThread->GetHandle();
        GetDiskFreeSpaceThread->ScheduleTerminate();
        GetDiskFreeSpaceThread = NULL;       // thread se ukonci a pritom se necha dealokovat
        AuxThreadQueue.WaitForExit(t, 1000); // dame threadu sanci se ukoncit
    }
    if (ConsImageList != NULL)
        ImageList_Destroy(ConsImageList);
    if (ItemsImageList != NULL)
        ImageList_Destroy(ItemsImageList);
    if (TitleText != NULL)
        SalamanderGeneral->Free(TitleText);
    if (OperationsTextOrig != NULL)
        SalamanderGeneral->Free(OperationsTextOrig);
}

void COperationDlg::ShowControlsAndChangeSize(BOOL simple)
{
    if (simple)
    {
        ConsListViewObj.HideToolTip();
        ItemsListViewObj.HideToolTip();
    }

    int k;
    for (k = 0; k < 2; k++)
    {
        if (k == 1 && simple || k == 0 && !simple)
        {
            HWND focus = GetFocus();
            int show = simple ? SW_HIDE : SW_SHOW;
            static int id[] = {IDL_CONNECTIONS, IDB_OPCONSSOLVEERROR, IDB_OPCONSADD, IDB_OPCONSSTOP,
                               IDB_OPCONSPAUSERESUME, IDL_OPERATIONS, IDB_OPOPERSSOLVEERROR,
                               IDB_OPOPERSRETRY, IDB_OPOPERSSKIP, IDB_OPOPERSSHOWONLYERR,
                               IDT_CONNECTIONS, IDT_OPERATIONSTEXT, -1};
            int i;
            for (i = 0; id[i] != -1; i++)
            {
                HWND wnd = GetDlgItem(HWindow, id[i]);
                ShowWindow(wnd, show);
                EnableWindow(wnd, !simple);
            }
            if (!simple)
            {
                EnableWindow(GetDlgItem(HWindow, IDB_OPOPERSSHOWONLYERR), EnableShowOnlyErrors);
                EnableWindow(GetDlgItem(HWindow, IDB_OPCONSADD), !DisableAddWorkerButton);
                EnableWindow(GetDlgItem(HWindow, IDB_OPCONSSTOP), WorkersList->GetCount() > 0);
                EnableSolveConError(-1);
                EnablePauseConButton(-1);
                EnableRetryItem(-1);
            }
            if (focus != NULL && !IsWindowVisible(focus))
            {
                // Details musi byt enabled, jinak by tato akce nenastala, hodime na nej fokus
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDB_SHOWDETAILS), TRUE);
            }
        }
        else
        {
            BOOL visible = IsWindowVisible(HWindow);
            WINDOWPLACEMENT placement;
            placement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(HWindow, &placement);
            if (simple)
                placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + MinDlgHeight1;
            else
                placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + LastDlgHeight1;
            if (!visible)
                placement.showCmd = SW_HIDE; // visible muze byt FALSE jen pri volani z WM_INITDIALOG; resi se tu: pri maximized stavu pri otevirani se ukazovalo jako restored a pak teprve jako maximized (problikavalo)
            SetWindowPlacement(HWindow, &placement);
        }
    }
}

void COperationDlg::SetDlgTitle(int progressValue, const char* state)
{
    if (TitleText != NULL)
    {
        char txt[500];
        char txt2[500];
        char* text = txt;
        if (progressValue != -1)
        {
            _snprintf_s(txt, _TRUNCATE, "(%d %%) %s", (int)((progressValue /*+ 5*/) / 10), TitleText); // nezaokrouhlujeme (100% musi byt az pri 100% a ne pri 99.5%)
        }
        else
        {
            if (state != NULL)
                _snprintf_s(txt, _TRUNCATE, "(%s) %s", state, TitleText);
            else
                text = TitleText; // progress neznamy, status tez neznamy (ukazuje se jen cisty titulek)
        }
        if (!GetWindowText(HWindow, txt2, 500) || strcmp(text, txt2) != 0)
        {
            SetWindowText(HWindow, text);
            HWND foreground = GetForegroundWindow();
            while (foreground != HWindow && (foreground = ::GetParent(foreground)) != NULL)
                ;
            if (foreground != HWindow && CurrentFlashWnd != NULL)
                FlashWindow(CurrentFlashWnd, TRUE); // zmena titulku vede ke ztrate flashe, musime ho nastavit znovu
        }
    }
}

void COperationDlg::ScheduleDelayedUpdate()
{
    if (!HasDelayedUpdateTimer)
    {
        UpdateDataInDialog(); // provedeme refresh
        // dalsi refresh provedeme nejdrive za OPERDLG_UPDATEPERIOD milisekund
        HasDelayedUpdateTimer = (SetTimer(HWindow, OPERDLG_UPDATETIMER, OPERDLG_UPDATEPERIOD, NULL) != 0);
    }
}

BOOL COperationDlg::UpdateDataInDialog()
{
    BOOL reportProgressChange = TRUE;
    int workerID = -1;
    if (IsDirtyConsListView) // zjistime, co se vlastne zmenilo
        workerID = Oper->GetChangedWorker(&reportProgressChange);

    BOOL change = IsDirtyStatus || IsDirtyProgress && reportProgressChange ||
                  IsDirtyConsListView || IsDirtyItemsListView;

    BOOL listsChange = IsDirtyConsListView || IsDirtyItemsListView;
    if (IsDirtyConsListView)
    {
        IsDirtyConsListView = FALSE;

        if (workerID != -1) // zmena jednoho workera
        {
            // prekreslime zmeneneho workera
            int index = WorkersList->GetWorkerIndex(workerID);
            RefreshConnections(FALSE, -1, index);
        }
        else
            RefreshConnections(FALSE); // zmena ve vice nez dvou workerech -> refreshenem vsechny
        EnablePauseButton();
    }

    if (IsDirtyStatus || IsDirtyProgress && reportProgressChange)
    {
        if (IsDirtyProgress)
            LastUpdateOfProgressByWorker = GetTickCount();
        IsDirtyStatus = FALSE;
        IsDirtyProgress = FALSE;

        int progressValue = -1;
        char statusText[300];
        statusText[0] = 0;
        char timeLeftText[100];
        timeLeftText[0] = 0;
        CQuadWord transferred(0, 0);
        CQuadWord total(0, 0);
        CQuadWord waiting(0, 0);
        int doneOrSkippedCount = 0;
        int totalCount = 0;
        int unknownSizeCount = 0;
        int waitingCount = 0;
        int errorsCount = 0;

        CFTPOperationType operType = Oper->GetOperationType();
        if (operType == fotDelete || operType == fotChangeAttrs)
        {
            progressValue = Queue->GetSimpleProgress(&doneOrSkippedCount, &totalCount,
                                                     &unknownSizeCount, &waitingCount);
            errorsCount = totalCount - doneOrSkippedCount - waitingCount;
        }
        else
        {
            if (operType == fotCopyDownload || operType == fotMoveDownload)
            {
                progressValue = Oper->GetCopyProgress(&transferred, &total, &waiting,
                                                      &unknownSizeCount, &errorsCount,
                                                      &doneOrSkippedCount,
                                                      &totalCount, Queue);
            }
            else
            {
                if (operType == fotCopyUpload || operType == fotMoveUpload)
                {
                    progressValue = Oper->GetCopyUploadProgress(&transferred, &total, &waiting,
                                                                &unknownSizeCount, &errorsCount,
                                                                &doneOrSkippedCount,
                                                                &totalCount, Queue);
                }
            }
        }
        COperationState operState = Oper->GetOperationState(FALSE);
        BOOL someDataActivityInLastPeriod = Oper->GetDataActivityInLastPeriod();

        if (OperationsTextOrig != NULL &&
            (doneOrSkippedCount != DisplayedDoneOrSkippedCount ||
             totalCount != DisplayedTotalCount))
        {
            char buf[200];
            _snprintf_s(buf, _TRUNCATE, "%s (%d / %d)", OperationsTextOrig, doneOrSkippedCount, totalCount);
            SetWindowText(GetDlgItem(HWindow, IDT_OPERATIONSTEXT), buf);
            DisplayedDoneOrSkippedCount = doneOrSkippedCount;
            DisplayedTotalCount = totalCount;
        }

        BOOL reallyInProgress = FALSE;
        if (operState == opstInProgress) // zkontrolujeme jestli existuje aspon jeden nepausnuty worker, jinak se neda mluvit o probihajici operaci
            reallyInProgress = !WorkersList->EmptyOrAllShouldStop() && !PauseButtonIsResume && PauseButtonIsEnabled;

        char num1[100];
        char num2[100];
        if (operType == fotDelete || operType == fotChangeAttrs)
        {
            if (doneOrSkippedCount != 0 || totalCount != 0)
            {
                CQuadWord param(totalCount, 0);
                SalamanderGeneral->ExpandPluralString(num2, 100, LoadStr(IDS_OPERDLGSTATUS3), 1, &param);
                _snprintf_s(statusText, _TRUNCATE, num2, doneOrSkippedCount, totalCount);
            }
            if (errorsCount > 0)
            {
                CQuadWord param(errorsCount, 0);
                SalamanderGeneral->ExpandPluralString(num2, 100, LoadStr(IDS_OPERDLGSTATUS5), 1, &param);
                _snprintf_s(num1, _TRUNCATE, num2, errorsCount);

                int statusTextLen = (int)strlen(statusText);
                if (statusTextLen > 0 && statusTextLen + 2 < 300)
                {
                    statusText[statusTextLen++] = ',';
                    statusText[statusTextLen++] = ' ';
                }
                lstrcpyn(statusText + statusTextLen, num1, 300 - statusTextLen);
            }
            if (unknownSizeCount != 0)
            {
                CQuadWord param(unknownSizeCount, 0);
                SalamanderGeneral->ExpandPluralString(num2, 100, LoadStr(IDS_OPERDLGSTATUS4), 1, &param);
                _snprintf_s(num1, _TRUNCATE, num2, unknownSizeCount);

                int statusTextLen = (int)strlen(statusText);
                if (statusTextLen > 0 && statusTextLen + 2 < 300)
                {
                    statusText[statusTextLen++] = ',';
                    statusText[statusTextLen++] = ' ';
                }
                lstrcpyn(statusText + statusTextLen, num1, 300 - statusTextLen);
            }
            if (reallyInProgress) // status bude jen u probihajici operace
            {
                DWORD transferIdleTime;
                DWORD speed = Oper->GetGlobalTransferSpeedMeter()->GetSpeed(&transferIdleTime);
                if (speed > 0)
                {
                    if (totalCount > 0 && transferIdleTime <= 30)
                    {
                        DWORD secs = (SMPLCMD_APPROXBYTESIZE * waitingCount) / speed; // odhad zbyvajicich sekund
                        secs++;                                                       // jedna vterina navic, abysme koncili operaci s "time left: 1 sec" (misto 0 sec)
                        if (LastTimeEstimation != -1)
                            secs = (2 * secs + LastTimeEstimation) / 3;
                        // vypocet zaokrouhleni (zhruba 10% chyba + zaokrouhlujeme po hezkych cislech 1,2,5,10,20,40)
                        DWORD dif = (secs + 5) / 10;
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
                        SalamanderGeneral->PrintTimeLeft(timeLeftText, CQuadWord(secs, 0));
                        LastTimeEstimation = secs;
                    }

                    if (transferIdleTime > 30)
                    {
                        int statusTextLen = (int)strlen(statusText);
                        if (statusTextLen > 0 && statusTextLen + 2 < 300)
                        {
                            statusText[statusTextLen++] = ',';
                            statusText[statusTextLen++] = ' ';
                        }
                        SalamanderGeneral->PrintTimeLeft(num1, CQuadWord(transferIdleTime, 0));
                        _snprintf_s(statusText + statusTextLen, 300 - statusTextLen, _TRUNCATE, LoadStr(IDS_OPERDLGCONNECTIONSIDLE2), num1);
                    }
                }
                //        else progressValue = -1;  // zakomentovano, protoze pri Pause/Resume vypina nesmyslne hodnotu progresu
            }
        }
        else
        {
            if (operType == fotCopyDownload || operType == fotMoveDownload ||
                operType == fotCopyUpload || operType == fotMoveUpload) // status bude zatim jen u Copy a Move
            {
                if (total != CQuadWord(0, 0) || transferred != CQuadWord(0, 0))
                {
                    SalamanderGeneral->PrintDiskSize(num1, transferred, 0);
                    if (total != CQuadWord(0, 0))
                    {
                        SalamanderGeneral->PrintDiskSize(num2, total, 0);
                        _snprintf_s(statusText, _TRUNCATE, LoadStr(IDS_OPERDLGSTATUS2), num1, num2);
                    }
                    else
                        lstrcpyn(statusText, num1, 300);
                }
                if (errorsCount > 0)
                {
                    CQuadWord param(errorsCount, 0);
                    SalamanderGeneral->ExpandPluralString(num2, 100, LoadStr(IDS_OPERDLGSTATUS5), 1, &param);
                    _snprintf_s(num1, _TRUNCATE, num2, errorsCount);

                    int statusTextLen = (int)strlen(statusText);
                    if (statusTextLen > 0 && statusTextLen + 2 < 300)
                    {
                        statusText[statusTextLen++] = ',';
                        statusText[statusTextLen++] = ' ';
                    }
                    lstrcpyn(statusText + statusTextLen, num1, 300 - statusTextLen);
                }
                if (unknownSizeCount != 0)
                {
                    CQuadWord param(unknownSizeCount, 0);
                    SalamanderGeneral->ExpandPluralString(num2, 100, LoadStr(IDS_OPERDLGSTATUS4), 1, &param);
                    _snprintf_s(num1, _TRUNCATE, num2, unknownSizeCount);

                    int statusTextLen = (int)strlen(statusText);
                    if (statusTextLen > 0 && statusTextLen + 2 < 300)
                    {
                        statusText[statusTextLen++] = ',';
                        statusText[statusTextLen++] = ' ';
                    }
                    lstrcpyn(statusText + statusTextLen, num1, 300 - statusTextLen);
                }
                if (reallyInProgress) // status bude jen u probihajici operace
                {
                    DWORD transferIdleTime;
                    DWORD speed = Oper->GetGlobalTransferSpeedMeter()->GetSpeed(&transferIdleTime);
                    if (speed > 0 || transferIdleTime > 30)
                    {
                        if (total > CQuadWord(0, 0) && speed > 0 && transferIdleTime <= 30)
                        {
                            CQuadWord secs = waiting / CQuadWord(speed, 0); // odhad zbyvajicich sekund
                            secs.Value++;                                   // jedna vterina navic, abysme koncili operaci s "time left: 1 sec" (misto 0 sec)
                            if (LastTimeEstimation != -1)
                                secs = (CQuadWord(2, 0) * secs + CQuadWord(LastTimeEstimation, 0)) / CQuadWord(3, 0);
                            // vypocet zaokrouhleni (zhruba 10% chyba + zaokrouhlujeme po hezkych cislech 1,2,5,10,20,40)
                            CQuadWord dif = (secs + CQuadWord(5, 0)) / CQuadWord(10, 0);
                            int expon = 0;
                            while (dif >= CQuadWord(50, 0))
                            {
                                dif /= CQuadWord(60, 0);
                                expon++;
                            }
                            if (dif <= CQuadWord(1, 0))
                                dif = CQuadWord(1, 0);
                            else if (dif <= CQuadWord(3, 0))
                                dif = CQuadWord(2, 0);
                            else if (dif <= CQuadWord(7, 0))
                                dif = CQuadWord(5, 0);
                            else if (dif < CQuadWord(15, 0))
                                dif = CQuadWord(10, 0);
                            else if (dif < CQuadWord(30, 0))
                                dif = CQuadWord(20, 0);
                            else
                                dif = CQuadWord(40, 0);
                            while (expon--)
                                dif *= CQuadWord(60, 0);
                            secs = ((secs + dif / CQuadWord(2, 0)) / dif) * dif; // zaokrouhlime 'secs' na 'dif' sekund
                            SalamanderGeneral->PrintTimeLeft(timeLeftText, secs);
                            LastTimeEstimation = (int)secs.Value;
                        }

                        int statusTextLen = (int)strlen(statusText);
                        if (statusTextLen > 0 && statusTextLen + 2 < 300)
                        {
                            statusText[statusTextLen++] = ',';
                            statusText[statusTextLen++] = ' ';
                        }
                        if (transferIdleTime <= 30)
                        {
                            SalamanderGeneral->PrintDiskSize(num1, CQuadWord(speed, 0), 0);
                            _snprintf_s(statusText + statusTextLen, 300 - statusTextLen, _TRUNCATE, LoadStr(IDS_OPERDLGSTATUS1), num1);
                        }
                        else
                        {
                            SalamanderGeneral->PrintTimeLeft(num1, CQuadWord(transferIdleTime, 0));
                            _snprintf_s(statusText + statusTextLen, 300 - statusTextLen, _TRUNCATE, LoadStr(IDS_OPERDLGCONNECTIONSIDLE), num1);
                        }
                    }
                }
            }
        }

        Status->SetText(statusText);

        if (timeLeftText[0] == 0)
        {
            switch (operState)
            {
            case opstInProgress:
            {
                if (reallyInProgress || !WorkersList->EmptyOrAllShouldStop())
                    lstrcpyn(timeLeftText, LoadStr(reallyInProgress ? IDS_LISTWNDESTIMTIMEUNKNOWN : IDS_OPERDLGTIMLEFTWAIT), 100);
                break;
            }

            case opstFinishedWithSkips:
            case opstSuccessfullyFinished:
                lstrcpyn(timeLeftText, LoadStr(IDS_OPERDLGTIMLEFTDONE), 100);
                break;
            case opstFinishedWithErrors:
                lstrcpyn(timeLeftText, LoadStr(IDS_OPERDLGTIMLEFTWAIT), 100);
                break;
            }
            LastTimeEstimation = -1;
        }
        TimeLeft->SetText(timeLeftText);

        char elapsedTime[100];
        DWORD elapsedSecs = Oper->GetElapsedSeconds();
        SalamanderGeneral->PrintTimeLeft(elapsedTime, CQuadWord(elapsedSecs, 0));
        ElapsedTime->SetText(elapsedTime);

        if (progressValue > 1000)
        {
            TRACE_E("COperationDlg::UpdateDataInDialog(): attempt to set progress bigger than 1000! ... " << progressValue);
            progressValue = 1000;
        }
        if (progressValue < -1)
        {
            TRACE_E("COperationDlg::UpdateDataInDialog(): attempt to set progress smaller than 0 (and not -1)! ... " << progressValue);
            progressValue = 0; // -1 je hodnota pro "unknown progress"
        }
        if (reallyInProgress)
        {
            if (progressValue == -1)
            {
                num1[0] = 0;
                if ((operType == fotCopyDownload || operType == fotMoveDownload ||
                     operType == fotCopyUpload || operType == fotMoveUpload) &&
                    transferred > CQuadWord(0, 0))
                {
                    SalamanderGeneral->PrintDiskSize(num1, transferred, 0);
                }
                SetDlgTitle(-1, num1[0] != 0 ? num1 : NULL);
            }
            else
                SetDlgTitle(progressValue, NULL);
            if (progressValue == -1 && someDataActivityInLastPeriod ||
                (DWORD)progressValue != ProgressValue)
            {
                Progress->SetProgress(progressValue, NULL);
            }
        }
        else
        {
            const char* progressTxt = "";
            if (operState == opstFinishedWithErrors)
                SetDlgTitle(-1, (progressTxt = LoadStr(IDS_OPERDLGTITLE_ERRORS)));
            else
            {
                if (operState == opstSuccessfullyFinished || operState == opstFinishedWithSkips)
                    SetDlgTitle(-1, (progressTxt = LoadStr(IDS_OPERDLGTITLE_DONE)));
                else
                {
                    SetDlgTitle(-1, (progressTxt = LoadStr(WorkersList->EmptyOrAllShouldStop() ? IDS_OPERDLGTITLE_STOPPED : WorkersList->AtLeastOneWorkerIsWaitingForUser() ? IDS_OPERDLGTITLE_WAITING
                                                                                                                                                                            : IDS_OPERDLGTITLE_PAUSED))); // neni hotovo, ale nejsou pridany zadni workeri, takze operace nebezi...
                }
            }
            if ((int)ProgressValue < 0)
            {
                if (ProgressValue == -1)
                    Progress->Stop(); // zastavime okamzite pohyb progresu
                Progress->SetProgress(0, progressTxt);
                progressValue = -2;
            }
            else
            {
                if ((DWORD)progressValue != ProgressValue)
                    Progress->SetProgress(progressValue, NULL);
            }
        }
        ProgressValue = progressValue;

        if (operState == opstInProgress && // kontrola mista na disku ma smysl jen pokud jeste neni dokoncena operace
            (operType == fotCopyDownload || operType == fotMoveDownload) &&
            waiting > CQuadWord(0, 0) &&
            GetDiskFreeSpaceThread != NULL &&
            GetTickCount() - LastGetDiskFreeSpace >= OPERDLG_GETDISKSPACEPERIOD)
        {
            LastNeededDiskSpace = waiting;
            LastGetDiskFreeSpace = GetTickCount();
            GetDiskFreeSpaceThread->ScheduleGetDiskFreeSpace();
        }
        else
        {
            if (operState != opstInProgress && ShowLowDiskWarning)
            {
                LastNeededDiskSpace.Set(-1, -1); // zneplatneni
                SetShowLowDiskWarning(FALSE);
                LayoutDialog(SizeBox != NULL ? IsWindowVisible(SizeBox) : 0);
            }
        }
    }

    if (IsDirtyItemsListView)
    {
        IsDirtyItemsListView = FALSE;

        // zjistime, co se vlastne zmenilo
        int itemUID1, itemUID2;
        Oper->GetChangedItems(&itemUID1, &itemUID2);
        if (!ShowOnlyErrors && itemUID1 != -1) // zmena jedne polozky (pri ShowOnlyErrors neni povoleno, nutny refresh vsech polozek - mozne schovani/ukazani polozky)
        {
            // prekreslime zmenenou polozku
            int index1 = Queue->GetItemIndex(itemUID1);
            int index2 = itemUID2 != -1 ? Queue->GetItemIndex(itemUID2) : -1;
            if (itemUID2 != -1 && index2 == -1)
                index1 = -1;                     // nezname druha polozka, radsi refreshneme vse
            RefreshItems(FALSE, index1, index2); // index1==-1 je refresh vsech polozek (nastava: neznama polozka)
        }
        else
            RefreshItems(FALSE); // zmena ve vice nez dvou polozkach -> refreshenem vsechny
    }
    if (listsChange)
        EnableErrorsButton();
    return change;
}

void COperationDlg::InitColumns()
{
    CALL_STACK_MESSAGE1("COperationDlg::InitColumns()");
    LV_COLUMN lvc;
    int header[3] = {IDS_OPERDLGCONS_ID, IDS_OPERDLGCONS_ACTION, IDS_OPERDLGCONS_STATUS};
    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; i < 3; i++) // vytvorim sloupce
    {
        lvc.pszText = LoadStr(header[i]);
        lvc.iSubItem = i;
        ListView_InsertColumn(ConsListView, i, &lvc);
    }
    int header2[2] = {IDS_OPERDLGOPERS_DESCR, IDS_OPERDLGOPERS_STATUS};
    int j;
    for (j = 0; j < 2; j++) // vytvorim sloupce
    {
        lvc.pszText = LoadStr(header2[j]);
        lvc.iSubItem = j;
        ListView_InsertColumn(ItemsListView, j, &lvc);
    }
    if (ConsImageList != NULL)
        ListView_SetImageList(ConsListView, ConsImageList, LVSIL_SMALL);
    if (ItemsImageList != NULL)
        ListView_SetImageList(ItemsListView, ItemsImageList, LVSIL_SMALL);
}

void COperationDlg::SetColumnWidths()
{
    if (!IsIconic(HWindow)) // pokud je minimalizovane, zavola se po restore znovu
    {
        RECT r;
        GetWindowRect(ItemsListView, &r);
        r.right -= r.left + OperationsEdges; // sirku maji oba listview stejnou

        int cxID = ListView_GetStringWidth(ConsListView, "00") + 16 + 8; // + ikona + "padding"
        ListView_SetColumnWidth(ConsListView, 0, cxID);
        ListView_SetColumnWidth(ConsListView, 1, r.right * 0.3); // 30% mista zabere sloupec Action
        int cxScroll = GetSystemMetrics(SM_CXHSCROLL);
        int lastCX = r.right - cxID - (int)(r.right * 0.3) - cxScroll;
        ListView_SetColumnWidth(ConsListView, 2, lastCX); // zbytek zabere sloupec Status

        ListView_SetColumnWidth(ItemsListView, 0, r.right * 0.7); // 70% mista zabere sloupec Description
        lastCX = r.right - (int)(r.right * 0.7) - cxScroll;
        ListView_SetColumnWidth(ItemsListView, 1, lastCX); // zbytek zabere sloupec Status
    }
}

void COperationDlg::RefreshConnections(BOOL init, int newFocusIndex, int refreshOnlyIndex)
{
    if (refreshOnlyIndex == -1) // refresh vsech
    {
        EnableChangeFocusedCon = FALSE;
        int topIndex = ListView_GetTopIndex(ConsListView);
        int lastFocus = ListView_GetNextItem(ConsListView, -1, LVIS_FOCUSED);

        SendMessage(ConsListView, WM_SETREDRAW, FALSE, 0);

        // urcime a nastavime pocet polozek v listview + najdeme index prvni chyby
        int count = WorkersList->GetCount();
        ListView_SetItemCountEx(ConsListView, count, LVSICF_NOSCROLL);
        ConErrorIndex = WorkersList->GetFirstErrorIndex();

        // enablujeme button Add
        HWND button = GetDlgItem(HWindow, IDB_OPCONSADD);
        if (IsWindowEnabled(button) && DisableAddWorkerButton)
        {
            if (GetFocus() == button)
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
            EnableWindow(button, FALSE);
        }

        // enablujeme button Stop
        button = GetDlgItem(HWindow, IDB_OPCONSSTOP);
        if ((IsWindowEnabled(button) != 0) != (count > 0))
        {
            if (GetFocus() == button)
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, DisableAddWorkerButton ? IDCANCEL : IDB_OPCONSADD), TRUE);
            EnableWindow(button, count > 0);
        }

        // schovame pripadny tooltip
        ConsListViewObj.HideToolTip();

        // fokusneme zvolenou polozku
        if (count > 0)
        {
            if (init) // fokus 0. polozky
            {
                ListView_SetItemState(ConsListView, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                if (!ConsListViewObj.Scrolling)
                    ListView_EnsureVisible(ConsListView, 0, FALSE);
            }
            else // co nejsetrnejsi refresh listview (zachovava top-index + focusi podle UID nebo posledniho fokusu)
            {
                if (newFocusIndex != -1)
                    lastFocus = newFocusIndex;
                if (lastFocus >= count)
                    lastFocus = count - 1;
                if (lastFocus < 0)
                    lastFocus = 0;

                if (lastFocus > 0)
                {
                    if (topIndex >= count)
                        topIndex = count - 1;
                    if (topIndex < 0)
                        topIndex = 0;
                    // nahrazka za SetTopIndex u list-view
                    if (!ConsListViewObj.Scrolling)
                    {
                        ListView_EnsureVisible(ConsListView, count - 1, FALSE);
                        ListView_EnsureVisible(ConsListView, topIndex, FALSE);
                    }
                }

                ListView_SetItemState(ConsListView, lastFocus, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                if (!ConsListViewObj.Scrolling)
                    ListView_EnsureVisible(ConsListView, lastFocus, FALSE);
            }
        }

        EnableChangeFocusedCon = TRUE;
        SendMessage(ConsListView, WM_SETREDRAW, TRUE, 0);
    }
    else // refresh jedineho workera
    {
        // najdeme index prvni chyby (mohl se zmenit i se zmenou jednoho workera)
        ConErrorIndex = WorkersList->GetFirstErrorIndex();

        // schovame pripadny tooltip
        ConsListViewObj.HideToolTip(refreshOnlyIndex);

        // prekreslime zmeneneho workera
        ListView_RedrawItems(ConsListView, refreshOnlyIndex, refreshOnlyIndex);
    }

    EnableSolveConError(-1);  // jeste enablujeme Solve Error button (zmena fokusu)
    EnablePauseConButton(-1); // jeste nastavime Pause button (zmena fokusu)
}

void COperationDlg::RefreshItems(BOOL init, int refreshOnlyIndex1, int refreshOnlyIndex2)
{
    EnableChangeFocusedItemUID = FALSE;

    ErrorsIndexes.DestroyMembers();
    if (!ErrorsIndexes.IsGood())
        ErrorsIndexes.ResetState();
    int allIndex, errIndex;
    int errCount = Queue->GetUserInputNeededCount(ShowOnlyErrors, &ErrorsIndexes,
                                                  FocusedItemUID, &allIndex, &errIndex);
    // disablujeme "only errors" checkbox pokud nejsou polozky s errory ("user input neeeded")
    if (errCount <= 0 && ShowOnlyErrors)
    {
        ShowOnlyErrors = FALSE;
        CheckDlgButton(HWindow, IDB_OPOPERSSHOWONLYERR, BST_UNCHECKED);
    }
    HWND check = GetDlgItem(HWindow, IDB_OPOPERSSHOWONLYERR);
    EnableShowOnlyErrors = errCount > 0;
    if (!SimpleLook && (IsWindowEnabled(check) != 0) != EnableShowOnlyErrors)
    {
        if (GetFocus() == check)
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)ItemsListView, TRUE);
        EnableWindow(check, EnableShowOnlyErrors);
    }

    if (ShowOnlyErrors || refreshOnlyIndex1 == -1) // refresh vsech (pri filtrovani erroru nutnost)
    {
        int topIndex = ListView_GetTopIndex(ItemsListView);
        int lastFocus = ListView_GetNextItem(ItemsListView, -1, LVIS_FOCUSED);

        SendMessage(ItemsListView, WM_SETREDRAW, FALSE, 0);

        // urcime a nastavime pocet polozek v listview
        int count = ShowOnlyErrors ? errCount : Queue->GetCount();
        ListView_SetItemCountEx(ItemsListView, count, LVSICF_NOSCROLL);

        // schovame pripadny tooltip
        ItemsListViewObj.HideToolTip();

        // fokusneme zvolenou polozku
        if (count > 0)
        {
            if (init) // fokus 0. polozky
            {
                ListView_SetItemState(ItemsListView, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                if (!ItemsListViewObj.Scrolling)
                    ListView_EnsureVisible(ItemsListView, 0, FALSE);
                int focusIndex = 0;
                if (ShowOnlyErrors && ErrorsIndexes.Count > 0)
                    focusIndex = ErrorsIndexes[0];
                FocusedItemUID = Queue->GetItemUID(focusIndex);
            }
            else // co nejsetrnejsi refresh listview (zachovava top-index + focusi podle UID nebo posledniho fokusu)
            {
                int index = lastFocus >= 0 ? lastFocus : 0;
                if (ShowOnlyErrors)
                {
                    if (errIndex != -1)
                        index = errIndex;
                }
                else
                {
                    if (allIndex != -1)
                        index = allIndex;
                }
                if (index >= count)
                    index = count - 1;

                if (index > 0)
                {
                    if (topIndex >= count)
                        topIndex = count - 1;
                    if (topIndex < 0)
                        topIndex = 0;
                    // nahrazka za SetTopIndex u list-view
                    if (!ItemsListViewObj.Scrolling)
                    {
                        ListView_EnsureVisible(ItemsListView, count - 1, FALSE);
                        ListView_EnsureVisible(ItemsListView, topIndex, FALSE);
                    }
                }

                ListView_SetItemState(ItemsListView, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                if (!ItemsListViewObj.Scrolling)
                    ListView_EnsureVisible(ItemsListView, index, FALSE);
                int focusIndex = index;
                if (ShowOnlyErrors && index < ErrorsIndexes.Count)
                    focusIndex = ErrorsIndexes[index];
                FocusedItemUID = Queue->GetItemUID(focusIndex);
            }
        }
        else
            FocusedItemUID = -1;

        SendMessage(ItemsListView, WM_SETREDRAW, TRUE, 0);
    }
    else // refresh jedne nebo dvou polozek
    {
        // schovame pripadny tooltip
        ItemsListViewObj.HideToolTip(refreshOnlyIndex1);
        if (refreshOnlyIndex2 != -1)
            ItemsListViewObj.HideToolTip(refreshOnlyIndex2);

        // prekreslime zmenenou polozku
        ListView_RedrawItems(ItemsListView, refreshOnlyIndex1, refreshOnlyIndex1);
        if (refreshOnlyIndex2 != -1)
            ListView_RedrawItems(ItemsListView, refreshOnlyIndex2, refreshOnlyIndex2);
    }

    EnableChangeFocusedItemUID = TRUE;
    EnableRetryItem(-1); // jeste enablujeme Retry button (zmena fokusu)
}

void COperationDlg::EnableErrorsButton()
{
    HWND button = GetDlgItem(HWindow, IDB_SHOWERRORS);
    if ((IsWindowEnabled(button) != 0) != (EnableShowOnlyErrors || ConErrorIndex != -1))
    {
        if (GetFocus() == button)
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
        EnableWindow(button, EnableShowOnlyErrors || ConErrorIndex != -1);
    }
}

void COperationDlg::EnablePauseButton()
{
    BOOL someIsWorkingAndNotPaused;
    BOOL someIsWorking = WorkersList->SomeWorkerIsWorking(&someIsWorkingAndNotPaused);
    HWND button = GetDlgItem(HWindow, IDB_PAUSERESUME);
    BOOL showResume = someIsWorking && !someIsWorkingAndNotPaused;
    if (PauseButtonIsResume != showResume)
    {
        PauseButtonIsResume = showResume;
        SetWindowText(button, PauseButtonIsResume ? LoadStr(IDS_OPERDLGRESUMEBUTTON) : PauseButtonPauseText);
    }
    PauseButtonIsEnabled = someIsWorking;
    if ((IsWindowEnabled(button) != 0) != someIsWorking)
    {
        if (GetFocus() == button)
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
        EnableWindow(button, someIsWorking);
    }
}

void COperationDlg::EnableSolveConError(int index)
{
    if (index == -1)
        index = ListView_GetNextItem(ConsListView, -1, LVIS_FOCUSED);
    BOOL enable = WorkersList->HaveError(index);
    HWND button = GetDlgItem(HWindow, IDB_OPCONSSOLVEERROR);
    if ((IsWindowEnabled(button) != 0) != enable)
    {
        if (GetFocus() == button)
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
        EnableWindow(button, enable);
    }
}

void COperationDlg::EnablePauseConButton(int index)
{
    if (index == -1)
        index = ListView_GetNextItem(ConsListView, -1, LVIS_FOCUSED);
    BOOL isWorking;
    BOOL isPaused = WorkersList->IsPaused(index, &isWorking);
    HWND button = GetDlgItem(HWindow, IDB_OPCONSPAUSERESUME);
    BOOL showResume = isPaused && isWorking;
    if (ConPauseButtonIsResume != showResume)
    {
        ConPauseButtonIsResume = showResume;
        SetWindowText(button, ConPauseButtonIsResume ? LoadStr(IDS_OPERDLGRESUMECONBUTTON) : ConPauseButtonPauseText);
    }
    if ((IsWindowEnabled(button) != 0) != isWorking)
    {
        if (GetFocus() == button)
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
        EnableWindow(button, isWorking);
    }
}

void COperationDlg::EnableRetryItem(int index)
{
    if (index == -1)
        index = ListView_GetNextItem(ItemsListView, -1, LVIS_FOCUSED);
    if (ShowOnlyErrors && index >= 0 && index < ErrorsIndexes.Count)
        index = ErrorsIndexes[index];
    BOOL canSkip, canRetry;
    BOOL enable = Queue->IsItemWithErrorToSolve(index, &canSkip, &canRetry);
    HWND button = GetDlgItem(HWindow, IDB_OPOPERSRETRY);
    if ((IsWindowEnabled(button) != 0) != canRetry)
    {
        if (GetFocus() == button)
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
        EnableWindow(button, canRetry);
    }
    button = GetDlgItem(HWindow, IDB_OPOPERSSOLVEERROR);
    if ((IsWindowEnabled(button) != 0) != enable)
    {
        if (GetFocus() == button)
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
        EnableWindow(button, enable);
    }
    button = GetDlgItem(HWindow, IDB_OPOPERSSKIP);
    if ((IsWindowEnabled(button) != 0) != canSkip)
    {
        if (GetFocus() == button)
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
        EnableWindow(button, canSkip);
    }
}

void COperationDlg::ToggleSimpleLook()
{
    char text[100];
    GetDlgItemText(HWindow, IDB_SHOWDETAILS, text, 100);
    SimpleLook = !SimpleLook;
    char c = SimpleLook ? '>' : '<';
    int len = (int)strlen(text);
    if (len >= 2)
    {
        text[len - 1] = c;
        text[len - 2] = c;
    }
    SetDlgItemText(HWindow, IDB_SHOWDETAILS, text);
    ShowControlsAndChangeSize(SimpleLook);
}

void COperationDlg::LayoutDialog(BOOL showSizeBox)
{
    if (!IsIconic(HWindow))
    {
        RECT clientRect;
        GetClientRect(HWindow, &clientRect);
        HDWP hdwp = HANDLES(BeginDeferWindowPos(28));
        if (hdwp != NULL)
        {
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDT_OPSOURCETEXT), NULL,
                                          0, 0, clientRect.right - SourceBorderWidth, SourceHeight,
                                          SWP_NOZORDER | SWP_NOMOVE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDT_OPTARGETTEXT), NULL,
                                          0, 0, clientRect.right - SourceBorderWidth, SourceHeight,
                                          SWP_NOZORDER | SWP_NOMOVE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDT_OPTIMELEFT), NULL,
                                          0, 0, clientRect.right - SourceBorderWidth, SourceHeight,
                                          SWP_NOZORDER | SWP_NOMOVE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDT_OPELAPSEDTIME), NULL,
                                          0, 0, clientRect.right - SourceBorderWidth, SourceHeight,
                                          SWP_NOZORDER | SWP_NOMOVE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDT_OPSTATUS), NULL,
                                          0, 0, clientRect.right - SourceBorderWidth - (ShowLowDiskWarning ? ErrIconToHintWidth + 5 : 0),
                                          SourceHeight, SWP_NOZORDER | SWP_NOMOVE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_OPPROGRESS), NULL,
                                          0, 0, clientRect.right - ProgressBorderWidth, ProgressHeight,
                                          SWP_NOZORDER | SWP_NOMOVE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_SHOWDETAILS), NULL,
                                          clientRect.right - DetailsXROffset, DetailsYOffset, 0, 0,
                                          SWP_NOZORDER | SWP_NOSIZE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_SHOWERRORS), NULL,
                                          clientRect.right - NextErrXROffset, DetailsYOffset, 0, 0,
                                          SWP_NOZORDER | SWP_NOSIZE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_HIDE), NULL,
                                          clientRect.right - HideXROffset, DetailsYOffset, 0, 0,
                                          SWP_NOZORDER | SWP_NOSIZE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_PAUSERESUME), NULL,
                                          clientRect.right - PauseXROffset, DetailsYOffset, 0, 0,
                                          SWP_NOZORDER | SWP_NOSIZE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDCANCEL), NULL,
                                          clientRect.right - CancelXROffset, DetailsYOffset, 0, 0,
                                          SWP_NOZORDER | SWP_NOSIZE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDHELP), NULL,
                                          clientRect.right - HelpXROffset, DetailsYOffset, 0, 0,
                                          SWP_NOZORDER | SWP_NOSIZE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDI_ERRORICON), NULL,
                                          clientRect.right - ErrIconXROffset, ErrIconYOffset, 0, 0,
                                          SWP_NOZORDER | SWP_NOSIZE));
            hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDT_ERRORMSG), NULL,
                                          clientRect.right - ErrHintXROffset, ErrHintYOffset, 0, 0,
                                          SWP_NOZORDER | SWP_NOSIZE));
            ConnectionsActWidth = clientRect.right - ConnectionsBorderWidth;
            if (!SimpleLook)
            {
                int yDiff = (int)(ListviewSplitRatio * (ConnectionsHeight + OperationsHeight +
                                                        clientRect.bottom - MinClientHeight)) -
                            ConnectionsHeight;
                if (yDiff < 0)
                    yDiff = 0;
                if (clientRect.bottom - MinClientHeight - yDiff < 0)
                    yDiff = clientRect.bottom - MinClientHeight;
                int yDiffRest = clientRect.bottom - MinClientHeight - yDiff;
                ConnectionsActHeight = ConnectionsHeight + yDiff;
                ConsAddActYOffset = ConsAddYOffset + yDiff;
                ConnectionsActHeightLimit = ConnectionsActHeight + OperationsHeight + yDiffRest;
                hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_DLGSPLITBAR), NULL,
                                              0, 0, clientRect.right - SplitBorderWidth, SplitHeight,
                                              SWP_NOZORDER | SWP_NOMOVE));
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
                hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_OPOPERSSOLVEERROR), NULL,
                                              clientRect.right - OpersShowErrXROffset, OpersShowErrYOffset + yDiff + yDiffRest,
                                              0, 0, SWP_NOZORDER | SWP_NOSIZE));
                hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_OPOPERSRETRY), NULL,
                                              clientRect.right - OpersRetryXROffset, OpersShowErrYOffset + yDiff + yDiffRest,
                                              0, 0, SWP_NOZORDER | SWP_NOSIZE));
                hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_OPOPERSSKIP), NULL,
                                              clientRect.right - OpersSkipXROffset, OpersShowErrYOffset + yDiff + yDiffRest,
                                              0, 0, SWP_NOZORDER | SWP_NOSIZE));
                hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_OPOPERSSHOWONLYERR), NULL,
                                              ShowOnlyErrXOffset, ShowOnlyErrYOffset + yDiff + yDiffRest,
                                              0, 0, SWP_NOZORDER | SWP_NOSIZE));
            }

            if (SizeBox != NULL)
            {
                hdwp = HANDLES(DeferWindowPos(hdwp, SizeBox, NULL, clientRect.right - SizeBoxWidth,
                                              clientRect.bottom - SizeBoxHeight, SizeBoxWidth, SizeBoxHeight, SWP_NOZORDER));
                // pry nejde show/hide kombinovat se zmenou velikosti a posunem
                hdwp = HANDLES(DeferWindowPos(hdwp, SizeBox, NULL, 0, 0, 0, 0,
                                              SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | (showSizeBox ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)));
            }

            if (!SimpleLook) // aby neproblikavaly scrollbary v listviewech pri zmensovani okna
            {
                SendMessage(ConsListView, WM_SETREDRAW, FALSE, 0);
                SendMessage(ItemsListView, WM_SETREDRAW, FALSE, 0);
            }

            HANDLES(EndDeferWindowPos(hdwp)); // provedeme premisteni vsech prvku dialogu
            SetColumnWidths();                // jeste nastavime nove sire sloupcu v listviewech

            if (!SimpleLook)
            {
                SendMessage(ConsListView, WM_SETREDRAW, TRUE, 0);
                SendMessage(ItemsListView, WM_SETREDRAW, TRUE, 0);
            }

            // progress je treba invalidatnout, nejak nebere ohled na zmenu velikosti (posun cisla %)
            InvalidateRect(GetDlgItem(HWindow, IDC_OPPROGRESS), NULL, FALSE);
        }
    }
}

void COperationDlg::SetShowLowDiskWarning(BOOL show)
{
    ShowLowDiskWarning = show;

    HWND focus = GetFocus();
    ShowWindow(GetDlgItem(HWindow, IDI_ERRORICON), ShowLowDiskWarning);
    ShowWindow(GetDlgItem(HWindow, IDT_ERRORMSG), ShowLowDiskWarning);
    if (focus != NULL && !IsWindowVisible(focus))
        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
}

void COperationDlg::SolveErrorOnConnection(int index)
{
    int workerID = WorkersList->GetWorkerID(index);
    char errBuf[FTPWORKER_ERRDESCR_BUFSIZE];
    CCertificate* unverifiedCertificate;
    if (WorkersList->GetErrorDescr(index, errBuf, FTPWORKER_ERRDESCR_BUFSIZE, &unverifiedCertificate))
    {
        if (unverifiedCertificate != NULL) // SSL: doslo ke zmene certifikatu serveru a ten novy nelze overit duveryhodnou certifikacni autoritou, musime se tedy zeptat usera, jestli chce novemu certifikatu duverovat
        {
            char errBuf2[300];
            if (!unverifiedCertificate->CheckCertificate(errBuf2, 300)) // nechame znovu overit certifikat + ziskat prislusnou chybovou hlasku
            {
                INT_PTR dlgRes;
                do
                {
                    CurrentFlashWnd = SalamanderGeneral->GetWndToFlash(HWindow);
                    DlgWillCloseIfOpFinWithSkips = FALSE;
                    dlgRes = CCertificateErrDialog(HWindow, errBuf2).Execute();
                    DlgWillCloseIfOpFinWithSkips = (IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED);
                    if (CurrentFlashWnd != NULL)
                    {
                        FlashWindow(CurrentFlashWnd, FALSE); // pod W2K+ uz asi neni potreba: flashovani se musi odstranit rucne
                        CurrentFlashWnd = NULL;
                    }
                    switch (dlgRes)
                    {
                    case IDOK: // Accept once
                    {
                        LastActivityTime = GetTickCount() - OPERDLG_SHOWERRMINIDLETIME; // simulace "idle" (aby dalsi error mohl prijit hned)
                        Oper->SetCertificate(unverifiedCertificate);
                        // dame vedet vsem workerum s chybou connectiony
                        WorkersList->PostLoginChanged(-1);
                        break;
                    }

                    case IDCANCEL: // user dal Cancel, dalsi chybu ukazeme az po timeoutu
                    {
                        //              SetUserWasActive();  // zakomentovano, aby po ESC ze Solve Error dialogu okno operace bylo stale jeste "untouched"
                        DelayAfterCancel = TRUE;
                        LastActivityTime = GetTickCount();
                        break;
                    }

                    case IDB_CERTIFICATE_VIEW:
                    {
                        CurrentFlashWnd = SalamanderGeneral->GetWndToFlash(HWindow);
                        DlgWillCloseIfOpFinWithSkips = FALSE;
                        unverifiedCertificate->ShowCertificate(HWindow);
                        DlgWillCloseIfOpFinWithSkips = (IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED);
                        if (CurrentFlashWnd != NULL)
                        {
                            FlashWindow(CurrentFlashWnd, FALSE); // pod W2K+ uz asi neni potreba: flashovani se musi odstranit rucne
                            CurrentFlashWnd = NULL;
                        }
                        if (SendWMClose != NULL && *SendWMClose) // progress dialog se zavira -> koncime
                        {
                            dlgRes = -1;             // jen aby se necyklilo
                            DelayAfterCancel = TRUE; // simulujeme Cancel
                            LastActivityTime = GetTickCount();
                        }
                        else
                        {
                            if (unverifiedCertificate->CheckCertificate(errBuf2, 300))
                            {                // certifikat serveru uz je duveryhodny (uzivatel ho nejspis rucne importoval)
                                dlgRes = -1; // jen aby se necyklilo
                                unverifiedCertificate->SetVerified(true);
                                LastActivityTime = GetTickCount() - OPERDLG_SHOWERRMINIDLETIME; // simulace "idle" (aby dalsi error mohl prijit hned)
                                Oper->SetCertificate(unverifiedCertificate);
                                // dame vedet vsem workerum s chybou connectiony
                                WorkersList->PostLoginChanged(-1);
                            }
                        }
                        break;
                    }
                    }
                } while (dlgRes == IDB_CERTIFICATE_VIEW);
            }
            else // certifikat uz je v poradku (zobrazovat dialog nema smysl?)
            {
                unverifiedCertificate->SetVerified(true);
                LastActivityTime = GetTickCount() - OPERDLG_SHOWERRMINIDLETIME; // simulace "idle" (aby dalsi error mohl prijit hned)
                Oper->SetCertificate(unverifiedCertificate);
                // dame vedet vsem workerum s chybou connectiony
                WorkersList->PostLoginChanged(-1);
            }
            unverifiedCertificate->Release();
        }
        else
        {
            // pripravime docasne hodnoty pro editaci v dialogu
            BOOL retryLoginWithoutAsking;
            BOOL proxyUsed;
            CProxyScriptParams proxyScriptParams;
            Oper->GetLoginErrorDlgInfo(proxyScriptParams.User, USER_MAX_SIZE,
                                       proxyScriptParams.Password, PASSWORD_MAX_SIZE,
                                       proxyScriptParams.Account, ACCOUNT_MAX_SIZE,
                                       &retryLoginWithoutAsking,
                                       &proxyUsed, proxyScriptParams.ProxyUser, USER_MAX_SIZE,
                                       proxyScriptParams.ProxyPassword, PASSWORD_MAX_SIZE);
            CLoginErrorDlg dlg(HWindow, errBuf, &proxyScriptParams, LoadStr(IDS_SOLVEERRSUBJECT),
                               NULL, LoadStr(IDS_SOLVEERRRETRY), LoadStr(IDS_SOLVEERRREPLY),
                               TRUE, FALSE, proxyUsed);
            dlg.RetryWithoutAsking = retryLoginWithoutAsking;
            CurrentFlashWnd = SalamanderGeneral->GetWndToFlash(HWindow);
            DlgWillCloseIfOpFinWithSkips = FALSE;
            INT_PTR res = dlg.Execute();
            DlgWillCloseIfOpFinWithSkips = (IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED);
            if (CurrentFlashWnd != NULL)
            {
                FlashWindow(CurrentFlashWnd, FALSE); // pod W2K+ uz asi neni potreba: flashovani se musi odstranit rucne
                CurrentFlashWnd = NULL;
            }
            if (res == IDOK)
            {
                LastActivityTime = GetTickCount() - OPERDLG_SHOWERRMINIDLETIME; // simulace "idle" (aby dalsi error mohl prijit hned)
                Oper->SetLoginErrorDlgInfo(proxyScriptParams.Password, proxyScriptParams.Account, dlg.RetryWithoutAsking,
                                           proxyUsed, proxyScriptParams.ProxyUser, proxyScriptParams.ProxyPassword);
                // dame vedet prislusnemu workerovi nebo vsem workerum s chybou connectiony
                WorkersList->PostLoginChanged(dlg.ApplyToAll ? -1 : workerID);
            }
            else // user dal Cancel, dalsi chybu ukazeme az po timeoutu
            {
                //        SetUserWasActive();  // zakomentovano, aby po ESC ze Solve Error dialogu okno operace bylo stale jeste "untouched"
                DelayAfterCancel = TRUE;
                LastActivityTime = GetTickCount();
            }
        }
    }
}

void COperationDlg::SetUserWasActive()
{
    if (!UserWasActive)
    {
        UserWasActive = TRUE;
        // v pripade, ze je checkbox zatrzeny jen kvuli Config.CloseOperationDlgIfSuccessfullyFinished,
        // musime ted checkbox vycistit (doslo k akci usera v okne = uz nedojde k zavreni okna)
        if (!CloseDlgWhenOperFin && IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED)
        {
            CheckDlgButton(HWindow, IDC_OPCLOSEWINWHENDONE, BST_UNCHECKED);
            DlgWillCloseIfOpFinWithSkips = FALSE;
            ClearChkboxTime = GetTickCount();
        }
    }
}

void COperationDlg::SolveErrorOnItem(int itemUID)
{
    CurrentFlashWnd = SalamanderGeneral->GetWndToFlash(HWindow);
    DlgWillCloseIfOpFinWithSkips = FALSE;
    int changedIndex = Queue->SolveErrorOnItem(HWindow, itemUID, Oper);
    DlgWillCloseIfOpFinWithSkips = (IsDlgButtonChecked(HWindow, IDC_OPCLOSEWINWHENDONE) == BST_CHECKED);
    if (CurrentFlashWnd != NULL)
    {
        FlashWindow(CurrentFlashWnd, FALSE); // pod W2K+ uz asi neni potreba: flashovani se musi odstranit rucne
        CurrentFlashWnd = NULL;
    }
    if (changedIndex != -2) // pokud doslo aspon k nejake zmene
    {
        LastActivityTime = GetTickCount() - OPERDLG_SHOWERRMINIDLETIME; // simulace "idle" (aby dalsi error mohl prijit hned)
        if (!ShowOnlyErrors && changedIndex != -1)
            RefreshItems(FALSE, changedIndex); // refreshneme zmenenou polozku
        else
            RefreshItems(FALSE); // refreshneme vse (jsou-li zobrazeny jen chyby, polozka/y muzou zmizet)
        EnableErrorsButton();
        WorkersList->PostNewWorkAvailable(changedIndex != -1); // informujeme pripadneho prvniho nebo vsechny spici workery o pripadne nove praci
    }
    else // user dal nejspis Cancel, dalsi chybu ukazeme az po timeoutu
    {
        //    SetUserWasActive();  // zakomentovano, aby po ESC ze Solve Error dialogu okno operace bylo stale jeste "untouched"
        DelayAfterCancel = TRUE;
        LastActivityTime = GetTickCount();
    }
}

void COperationDlg::CorrectLookOfPrevFocusedDisabledButton(HWND prevFocus)
{
    if (prevFocus != NULL && !IsWindowEnabled(prevFocus))
    {
        char className[31];
        if (GetClassName(prevFocus, className, 31) && _stricmp(className, "button") == 0)
        {
            LONG style = GetWindowLong(prevFocus, GWL_STYLE);
            if ((style & BS_CHECKBOX) == 0 && (style & BS_DEFPUSHBUTTON) != 0)
                SendMessage(prevFocus, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
        }
        HWND focus = GetFocus();
        if (!IsWindowEnabled(focus))
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDCANCEL), TRUE);
        else
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)focus, TRUE);
    }
}
