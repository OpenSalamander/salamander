// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "worker.h"
#include "usermenu.h"
#include "execute.h"
#include "gui.h"
#include "menu.h"
#include "consts.h"

CConfiguration Configuration;

#define IDT_UPDATESTATUS_PERIOD 500 // interval for updating progress dialog status

//
// ****************************************************************************
// CChangeAttrDialog
//

CChangeAttrDialog::CChangeAttrDialog(HWND parent, DWORD attr, DWORD attrDiff,
                                     BOOL selectionContainsDirectory, BOOL fileBasedCompression,
                                     BOOL fileBasedEncryption,
                                     const SYSTEMTIME* timeModified,
                                     const SYSTEMTIME* timeCreated,
                                     const SYSTEMTIME* timeAccessed)
    : CCommonDialog(HLanguage, IDD_ATTRIBUTES, IDD_ATTRIBUTES, parent)
{
    SelectionContainsDirectory = selectionContainsDirectory;
    FileBasedCompression = fileBasedCompression;
    FileBasedEncryption = fileBasedEncryption;
    Archive = (attr & FILE_ATTRIBUTE_ARCHIVE) != 0;
    ReadOnly = (attr & FILE_ATTRIBUTE_READONLY) != 0;
    Hidden = (attr & FILE_ATTRIBUTE_HIDDEN) != 0;
    System = (attr & FILE_ATTRIBUTE_SYSTEM) != 0;
    Compressed = (attr & FILE_ATTRIBUTE_COMPRESSED) != 0;
    Encrypted = (attr & FILE_ATTRIBUTE_ENCRYPTED) != 0;
    if (attrDiff & FILE_ATTRIBUTE_ARCHIVE)
        Archive = 2;
    if (attrDiff & FILE_ATTRIBUTE_READONLY)
        ReadOnly = 2;
    if (attrDiff & FILE_ATTRIBUTE_HIDDEN)
        Hidden = 2;
    if (attrDiff & FILE_ATTRIBUTE_SYSTEM)
        System = 2;
    if (attrDiff & FILE_ATTRIBUTE_COMPRESSED)
        Compressed = 2;
    if (attrDiff & FILE_ATTRIBUTE_ENCRYPTED)
        Encrypted = 2;
    ArchiveDirty = FALSE;
    ReadOnlyDirty = FALSE;
    HiddenDirty = FALSE;
    SystemDirty = FALSE;
    CompressedDirty = FALSE;
    EncryptedDirty = FALSE;
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
        SalMessageBox(HWindow, LoadStr(IDS_INVALIDDATE), LoadStr(IDS_ERRORTITLE),
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
    ti.CheckBox(IDC_COMPRESSED, Compressed);
    ti.CheckBox(IDC_ENCRYPTED, Encrypted);
    ti.CheckBox(IDC_RECURSESUBDIRS, RecurseSubDirs);

    if (ti.Type == ttDataToWindow)
    {
        EnableWindow(GetDlgItem(HWindow, IDC_RECURSESUBDIRS), SelectionContainsDirectory);
        EnableWindow(GetDlgItem(HWindow, IDC_COMPRESSED), FileBasedCompression);
        EnableWindow(GetDlgItem(HWindow, IDC_ENCRYPTED), FileBasedEncryption);

        // first fill in the dates
        DateTime_SetSystemtime(HModifiedDate, GDT_VALID, &TimeModified);
        DateTime_SetSystemtime(HCreatedDate, GDT_VALID, &TimeCreated);
        DateTime_SetSystemtime(HAccessedDate, GDT_VALID, &TimeAccessed);
        // then disable them (cannot be done in a single step)
        DateTime_SetSystemtime(HModifiedDate, GDT_NONE, &TimeModified);
        DateTime_SetSystemtime(HCreatedDate, GDT_NONE, &TimeCreated);
        DateTime_SetSystemtime(HAccessedDate, GDT_NONE, &TimeAccessed);
        // fill in the times
        DateTime_SetSystemtime(HModifiedTime, GDT_VALID, &TimeModified);
        DateTime_SetSystemtime(HCreatedTime, GDT_VALID, &TimeCreated);
        DateTime_SetSystemtime(HAccessedTime, GDT_VALID, &TimeAccessed);
        // work around a common controls bug -- if SetFocus is not called,
        // all three controls appear focused
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

            case IDC_COMPRESSED:
            {
                CompressedDirty = TRUE;
                if (IsDlgButtonChecked(HWindow, IDC_COMPRESSED) == BST_CHECKED)
                {
                    EncryptedDirty = TRUE;
                    CheckDlgButton(HWindow, IDC_ENCRYPTED, BST_UNCHECKED); // Compressed & Encrypted are mutually exclusive
                }
                break;
            }

            case IDC_ENCRYPTED:
            {
                EncryptedDirty = TRUE;
                if (IsDlgButtonChecked(HWindow, IDC_ENCRYPTED) == BST_CHECKED)
                {
                    CompressedDirty = TRUE;
                    CheckDlgButton(HWindow, IDC_COMPRESSED, BST_UNCHECKED); // Compressed & Encrypted are mutually exclusive
                }
                break;
            }

            case IDC_RECURSESUBDIRS:
            {
                // when the user enables Include Subdirs we gray untouched check boxes
                // because we do not know what the selected directories contain
                // when it is turned off we restore the original check box states
                BOOL checked = IsDlgButtonChecked(HWindow, IDC_RECURSESUBDIRS) == BST_CHECKED;
                if (!ArchiveDirty)
                    CheckDlgButton(HWindow, IDC_ARCHIVE, checked ? 2 : Archive);
                if (!ReadOnlyDirty)
                    CheckDlgButton(HWindow, IDC_READONLY, checked ? 2 : ReadOnly);
                if (!HiddenDirty)
                    CheckDlgButton(HWindow, IDC_HIDDEN, checked ? 2 : Hidden);
                if (!SystemDirty)
                    CheckDlgButton(HWindow, IDC_SYSTEM, checked ? 2 : System);
                if (!CompressedDirty && FileBasedCompression)
                    CheckDlgButton(HWindow, IDC_COMPRESSED, checked ? 2 : Compressed);
                if (!EncryptedDirty && FileBasedEncryption)
                    CheckDlgButton(HWindow, IDC_ENCRYPTED, checked ? 2 : Encrypted);
                break;
            }
            case IDC_ATTR_CURRENT:
            {
                SYSTEMTIME st;
                GetLocalTime(&st); // get the current time

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

//
// ****************************************************************************
// CProgressDialog
//

unsigned ThreadProgressDlgBody(void* parameter)
{
    CALL_STACK_MESSAGE1("ThreadProgressDlgBody()");
    SetThreadNameInVCAndTrace("ProgrDlg");
    TRACE_I("Begin");
    CStartProgressDialogData* data = (CStartProgressDialogData*)parameter;
    CChangeAttrsData attrsDataCopy;
    if (data->AttrsData != NULL)
        attrsDataCopy = *data->AttrsData;
    CChangeAttrsData* attrsData = (data->AttrsData != NULL ? &attrsDataCopy : NULL);
    CConvertData convertDataCopy;
    if (data->ConvertData != NULL)
        convertDataCopy = *data->ConvertData;
    CConvertData* convertData = (data->ConvertData != NULL ? &convertDataCopy : NULL);
    char workPath1[MAX_PATH];
    lstrcpyn(workPath1, data->Script->WorkPath1, MAX_PATH);
    BOOL workPath1InclSubDirs = data->Script->WorkPath1InclSubDirs;
    char workPath2[MAX_PATH];
    lstrcpyn(workPath2, data->Script->WorkPath2, MAX_PATH);
    BOOL workPath2InclSubDirs = data->Script->WorkPath2InclSubDirs;

    CProgressDialog dlg(NULL, data->Script, data->Caption, attrsData, convertData, TRUE, data);
    INT_PTR res = dlg.Execute();
    if (res == 0 || res == -1 || res == IDABORT) // failed to open the dialog or worker thread
        SetEvent(data->ContEvent);               // let the main thread continue (opening the dialog or starting the operation failed)

    if (workPath1[0] != 0)
        MainWindow->PostChangeOnPathNotification(workPath1, workPath1InclSubDirs);
    if (workPath2[0] != 0)
        MainWindow->PostChangeOnPathNotification(workPath2, workPath2InclSubDirs);
    TRACE_I("End");
    return 0;
}

unsigned ThreadProgressDlgEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ThreadProgressDlgBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread Progress Dlg: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // harder exit (this one still calls some operations)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ThreadProgressDlg(void* param)
{
    CCallStack stack;
    return ThreadProgressDlgEH(param);
}

BOOL StartProgressDialog(COperations* script, const char* caption,
                         CChangeAttrsData* attrsData, CConvertData* convertData)
{
    BOOL ret = FALSE;
    ProgressDlgArray.RemoveFinishedDlgs();
    HANDLE contEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL)); // "nonsignaled" state, auto
    if (contEvent != NULL)
    {
        CProgressDlgArrItem* newDlg = ProgressDlgArray.PrepareNewDlg();
        if (newDlg != NULL)
        {
            CStartProgressDialogData data;
            data.Script = script;
            data.Caption = caption;
            data.AttrsData = attrsData;
            data.ConvertData = convertData;
            data.NewDlg = newDlg;
            data.OperationWasStarted = FALSE;
            data.ContEvent = contEvent;
            MultiMonGetClipRectByWindow(MainWindow->HWindow, &data.MainWndRectClipR, NULL);
            if (!IsIconic(MainWindow->HWindow))
                GetWindowRect(MainWindow->HWindow, &data.MainWndRectByR);
            else
                data.MainWndRectByR = data.MainWndRectClipR;

            DWORD threadID;
            HANDLE dlgThread = HANDLES(CreateThread(NULL, 0, ThreadProgressDlg, &data, 0, &threadID));
            if (dlgThread != NULL)
            {
                // wait until the dialog thread takes the data, opens the dialog and starts the worker thread and
                // passes the data to it
                WaitForSingleObject(contEvent, INFINITE);
                ProgressDlgArray.SetDlgData(newDlg, dlgThread, NULL);
                ret = data.OperationWasStarted;
            }
            else
            {
                TRACE_E("StartProgressDialog(): unable to start progress dialog thread!");
                ProgressDlgArray.RemoveDlg(newDlg);
            }
        }
        HANDLES(CloseHandle(contEvent));
    }
    else
        TRACE_E("StartProgressDialog(): unable to create 'contEvent' system event!");
    return ret;
}

CProgressDialog::CProgressDialog(HWND parent, COperations* script, const char* caption,
                                 CChangeAttrsData* attrsData, CConvertData* convertData,
                                 BOOL runningInOwnThread, CStartProgressDialogData* progrDlgData)
    : CCommonDialog(HLanguage, IDD_PROGRESSDLG, parent)
{
    RunningInOwnThread = runningInOwnThread;
    ProgrDlgData = progrDlgData;
    Worker = NULL;
    WContinue = NULL;
    WorkerNotSuspended = NULL;
    CancelWorker = FALSE;
    OperationProgress = 0;
    SummaryProgress = 0;
    strcpy(Caption, caption);
    Script = script;
    AttrsData = attrsData;
    AcceptCommands = TRUE;
    CanClose = TRUE;
    ConvertData = convertData;
    TimerIsRunning = FALSE;
    FirstUserSetDialog = TRUE;
    NextForegroundWindow = NULL;
    CacheIsDirty = FALSE;
    OperationCache[0] = 0;
    PrepositionCache[0] = 0;
    SourceCache[0] = 0;
    TargetCache[0] = 0;
    OperationProgressCacheIsDirty = FALSE;
    OperationProgressCache = 0;
    SummaryProgressCacheIsDirty = FALSE;
    SummaryProgressCache = 0;
    ShowPause = TRUE;
    DoNotBeepOnClose = FALSE;
    IsInQueue = FALSE;
    AutoPaused = FALSE;
    StatusPaused = FALSE;
    NextTimeLeftUpdateTime = GetTickCount();
    TimeLeftLastValue.SetUI64(0);
}

CProgressDialog::~CProgressDialog()
{
    if (Worker != NULL)
        TRACE_E("Unexpected situation in CProgressDialog::~CProgressDialog(): worker thread is still alive!");
}

char* RemapNames(char* name, int bufLen, char* source, COperations* script)
{
    CALL_STACK_MESSAGE3("RemapNames(, %d, %s,)", bufLen, source);
    char* s = strstr(source, script->RemapNameFrom);
    if (s != NULL)
    {
        int len = (int)strlen(source);
        if (len - script->RemapNameFromLen + script->RemapNameToLen < bufLen)
        {
            memcpy(name, source, s - source);
            char* st = name + (s - source);
            memcpy(st + script->RemapNameToLen, s + script->RemapNameFromLen,
                   len - ((s + script->RemapNameFromLen) - source) + 1);
            memcpy(st, script->RemapNameTo, script->RemapNameToLen);
            return name;
        }
        else
            return source;
    }
    else
        return source;
}

BOOL CProgressDialog::FlushCachedData()
{
    BOOL changed = CacheIsDirty | OperationProgressCacheIsDirty | SummaryProgressCacheIsDirty;
    // texts
    if (CacheIsDirty)
    {
        if (OperationText != NULL)
            OperationText->SetText(OperationCache);
        if (Source != NULL && Script != NULL)
        {
            if (Script->RemapNameFrom != NULL)
            {
                char name[MAX_PATH];
                Source->SetTextToDblQuotesIfNeeded(RemapNames(name, MAX_PATH, SourceCache, Script));
            }
            else
                Source->SetTextToDblQuotesIfNeeded(SourceCache);
        }
        SetWindowText(HPreposition, PrepositionCache);
        if (Target != NULL)
            Target->SetTextToDblQuotesIfNeeded(TargetCache);
        CacheIsDirty = FALSE;
    }

    // progress bar
    if (OperationProgressCacheIsDirty)
    {
        if (Operation != NULL)
            Operation->SetProgress(OperationProgressCache);
        OperationProgressCacheIsDirty = FALSE;
    }

    // progress bar + dialog title
    if (SummaryProgressCacheIsDirty)
    {
        SetDlgTitle(IsIconic(RunningInOwnThread ? HWindow : MainWindow->HWindow));
        if (Summary != NULL)
            Summary->SetProgress(SummaryProgressCache);
        TaskBarList3.SetProgressValue(SummaryProgressCache, 1000);
        SummaryProgressCacheIsDirty = FALSE;
    }

    return changed;
}

void CProgressDialog::SetDlgTitle(BOOL minimized)
{
    char buf[200];
    TaskBarList3.SetProgressState(ShowPause ? TBPF_NORMAL : TBPF_PAUSED);
    if (RunningInOwnThread)
    {
        if (ShowPause)
            sprintf(buf, "(%d %%) %s", (int)((min(1000, SummaryProgress) /*+ 5*/) / 10), Caption); // no rounding (100% must appear only at 100% and not at 99.5%)
        else
            sprintf(buf, "(%s) %s", LoadStr(AutoPaused ? IDS_PROGDLGQUEUEPAUSED : IDS_PROGDLGPAUSED),
                    AutoPaused && Script != NULL && Script->WaitInQueueSubject != NULL ? Script->WaitInQueueSubject : Caption);
        char oldCaption[200];
        ::GetWindowText(HWindow, oldCaption, 200);
        oldCaption[199] = 0;
        if (strcmp(oldCaption, buf) != 0)
            SetWindowText(HWindow, buf);
    }
    else
    {
        if (minimized)
        {
            if (ShowPause)
                sprintf(buf, "(%d %%) %s: %s", (int)((min(1000, SummaryProgress) /*+ 5*/) / 10), MAINWINDOW_NAME, Caption); // no rounding (100% must appear only at 100% and not at 99.5%)
            else
                sprintf(buf, "(%s) %s: %s", LoadStr(IDS_PROGDLGPAUSED), MAINWINDOW_NAME, Caption);

            MainWindow->SetWindowTitle(buf);
        }
        else
            MainWindow->SetWindowTitle();
    }
}

void CProgressDialog::SetWindowIcon()
{
    if (RunningInOwnThread)
    {
        // from XP the taskbar shows the application icon even for windows without an assigned icon
        // because we do not control which icon the OS shows (it probably uses the default EXE icon),
        // set the correct icon for the dialog to avoid this problem
        int resID = MainWindowIcons[Configuration.GetMainWindowIconIndex()].IconResID;
        SendMessage(HWindow, WM_SETICON, ICON_BIG,
                    (LPARAM)HANDLES(LoadIcon(HInstance, MAKEINTRESOURCE(resID))));
    }
}

INT_PTR
CProgressDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CProgressDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetDlgItemText(HWindow, IDB_PAUSERESUME, LoadStr(IDS_PROGDLGPAUSE));

        if (!RunningInOwnThread)
            SetWindowText(HWindow, Caption); // in the modal version of the dialog this is the only title setup

        SetWindowIcon();

        SetDlgTitle(FALSE);
        if ((OperationText = new CStaticText(HWindow, IDS_OPERATION, STF_CACHED_PAINT)) == NULL)
            TRACE_E(LOW_MEMORY);
        if ((Operation = new CProgressBar(HWindow, IDF_OPERATION)) == NULL)
            TRACE_E(LOW_MEMORY);
        if ((Summary = new CProgressBar(HWindow, IDF_SUMMARY)) == NULL)
            TRACE_E(LOW_MEMORY);
        if ((Source = new CStaticText(HWindow, IDS_SOURCE, STF_PATH_ELLIPSIS | STF_CACHED_PAINT)) == NULL)
            TRACE_E(LOW_MEMORY);
        if ((Target = new CStaticText(HWindow, IDS_TARGET, STF_PATH_ELLIPSIS | STF_CACHED_PAINT)) == NULL)
            TRACE_E(LOW_MEMORY);

        if (!Script->ShowStatus)
        {
            ShowWindow(GetDlgItem(HWindow, IDT_STATUS), SW_HIDE);
            Status = NULL;

            HDWP hdwp = HANDLES(BeginDeferWindowPos(3)); // shrink the dialog (remove the status line)
            if (hdwp != NULL)
            {
                RECT r;
                GetWindowRect(GetDlgItem(HWindow, IDT_STATUS), &r);
                int yOffset = r.bottom - r.top;
                int i;
                for (i = 0; i < 3; i++)
                {
                    HWND hCtrl = GetDlgItem(HWindow, i == 0 ? IDB_MINIMIZE : i == 1 ? IDB_PAUSERESUME
                                                                                    : IDCANCEL);
                    GetWindowRect(hCtrl, &r);
                    ScreenToClient(HWindow, (LPPOINT)&r);
                    hdwp = HANDLES(DeferWindowPos(hdwp, hCtrl, NULL, r.left, r.top - yOffset, 0, 0, SWP_NOSIZE | SWP_NOZORDER));
                }
                HANDLES(EndDeferWindowPos(hdwp));
                GetWindowRect(HWindow, &r);
                SetWindowPos(HWindow, NULL, 0, 0, r.right - r.left, r.bottom - r.top - yOffset,
                             SWP_NOZORDER | SWP_NOMOVE);
            }
        }
        else
        {
            if ((Status = new CStaticText(HWindow, IDT_STATUS, STF_CACHED_PAINT)) == NULL)
                TRACE_E(LOW_MEMORY);
            new CButton(HWindow, IDB_PAUSERESUME, BTF_DROPDOWN);
        }

        HPreposition = GetDlgItem(HWindow, IDS_PREPOSITION);

        PostMessage(HWindow, WM_USER_PROGRDLGSTART, 0, 0); // probably needless on W2K+: delay the start of the worker thread

        if (Parent == NULL && ProgrDlgData != NULL)
        {
            MultiMonCenterWindowByRect(HWindow, ProgrDlgData->MainWndRectClipR, ProgrDlgData->MainWndRectByR);
            return CDialog::DialogProc(uMsg, wParam, lParam); // skip CCommonDialog::DialogProc()
        }
        else
            break;
    }

    case WM_USER_PROGRDLGSTART: // probably needless on W2K+: just a delayed worker thread start
    {
        //--- creation of synchronization objects
        WContinue = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
        if (WContinue == NULL)
        {
            TRACE_E("Unable to create WContinue event.");
            EndDialog(HWindow, IDABORT); // fatal error
            break;
        }
        WorkerNotSuspended = HANDLES(CreateEvent(NULL, TRUE, TRUE, NULL));
        if (WorkerNotSuspended == NULL)
        {
            TRACE_E("Unable to create WorkerNotSuspended event.");
            HANDLES(CloseHandle(WContinue));
            WContinue = NULL;
            EndDialog(HWindow, IDABORT); // fatal error
            break;
        }
        BOOL startPaused = FALSE;
        if (Script->IsCopyOrMoveOperation && OperationsQueue.AddOperation(HWindow, Script->StartOnIdle, &startPaused))
        {
            IsInQueue = TRUE;
            if (startPaused)
            {
                AutoPaused = TRUE;
                ResetEvent(WorkerNotSuspended);
                ShowPause = FALSE;
                SetDlgItemText(HWindow, IDB_PAUSERESUME, LoadStr(ShowPause ? IDS_PROGDLGPAUSE : IDS_PROGDLGRESUME));
                SetDlgTitle(IsIconic(RunningInOwnThread ? HWindow : MainWindow->HWindow));

                if (Script->WaitInQueueFrom != NULL && Script->WaitInQueueTo != NULL)
                {
                    if (OperationText != NULL)
                        OperationText->SetText(LoadStr(IDS_COPYINGFROM));
                    if (Source != NULL)
                        Source->SetTextToDblQuotesIfNeeded(Script->WaitInQueueFrom);
                    SetWindowText(HPreposition, LoadStr(IDS_COPYINGTO));
                    if (Target != NULL)
                        Target->SetTextToDblQuotesIfNeeded(Script->WaitInQueueTo);
                }
            }
        }

        Worker = StartWorker(Script, HWindow, AttrsData, ConvertData, WContinue,
                             WorkerNotSuspended, &CancelWorker, &OperationProgress,
                             &SummaryProgress);
        if (Worker == NULL)
        {
            if (IsInQueue)
                OperationsQueue.OperationEnded(HWindow, TRUE, NULL);
            IsInQueue = FALSE;
            HANDLES(CloseHandle(WorkerNotSuspended));
            WorkerNotSuspended = NULL;
            HANDLES(CloseHandle(WContinue));
            WContinue = NULL;
            EndDialog(HWindow, IDABORT); // fatal error
        }
        else
        {
            if (RunningInOwnThread)
            {
                if (ProgrDlgData != NULL)
                {
                    ProgressDlgArray.SetDlgData(ProgrDlgData->NewDlg, NULL, HWindow);
                    ProgrDlgData->OperationWasStarted = TRUE;
                    SetEvent(ProgrDlgData->ContEvent); // let the main thread continue (it waits for the dialog to open and the operation to start)
                    ProgrDlgData = NULL;
                }
                if (Configuration.AlwaysOnTop) // handle always-on-top at least "statically" (not in the system menu)
                    SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                if (startPaused)
                    PostMessage(HWindow, WM_COMMAND, IDB_MINIMIZE, 0); // minimize the "waiting" operation immediately (nothing to watch, saves one step for the user)
                else
                    SetForegroundWindow(HWindow);
            }
            if (!startPaused && Status != NULL)
            {
                NextTimeLeftUpdateTime = GetTickCount();
                SetTimer(HWindow, IDT_UPDATESTATUS, IDT_UPDATESTATUS_PERIOD, NULL);
            }
        }
        return TRUE;
    }

        //--- setting controls and caption
    case WM_USER_SETDIALOG:
    {
        CProgressData* data = (CProgressData*)wParam;
        if (data != NULL)
        {
            // do not draw data immediately, only on the timer
            lstrcpyn(OperationCache, data->Operation, 100);
            lstrcpyn(PrepositionCache, data->Preposition, 100);
            lstrcpyn(SourceCache, data->Source, 2 * MAX_PATH);
            lstrcpyn(TargetCache, data->Target, 2 * MAX_PATH);
            CacheIsDirty = TRUE;
        }

        if (OperationProgress != OperationProgressCache)
        {
            OperationProgressCache = OperationProgress;
            OperationProgressCacheIsDirty = TRUE;
        }

        if (SummaryProgress != SummaryProgressCache)
        {
            SummaryProgressCache = SummaryProgress;
            SummaryProgressCacheIsDirty = TRUE;
        }

        if (FirstUserSetDialog)
        {
            FirstUserSetDialog = FALSE;
            UpdateWindow(HWindow);
        }

        if (!TimerIsRunning)
        {
            // for updating data
            SetTimer(HWindow, IDT_REPAINT, 100, NULL);
            TimerIsRunning = TRUE;
            FlushCachedData();
        }

        return TRUE;
    }
        //--- worker request to show a dialog
    case WM_USER_DIALOG:
    {
        if (CancelWorker)
            return TRUE; // should terminate, it should not request anything

        BOOL canFlash = RunningInOwnThread;
        if (IsIconic(RunningInOwnThread ? HWindow : MainWindow->HWindow))
        {
            SetDlgTitle(FALSE);
            if (RunningInOwnThread)
                ShowWindow(HWindow, SW_SHOWNOACTIVATE /*SW_RESTORE*/); // activate minimized dlg
            else
                RestoreApp(MainWindow->HWindow, HWindow);
            if (Worker != NULL)
                SetThreadPriority(Worker, THREAD_PRIORITY_NORMAL);
        }
        else
            canFlash = TRUE;
        if (canFlash && GetForegroundWindow() != HWindow)
            FlashWindow(RunningInOwnThread ? HWindow : MainWindow->HWindow, TRUE);
        else
            canFlash = FALSE;

        //      EndSuspendMode();  // work interruption, time to refresh
        //      MainWindow->RefreshDiskFreeSpace();

        if (Status != NULL)
        { // we must stop showing the status
            KillTimer(HWindow, IDT_UPDATESTATUS);
            StatusPaused = TRUE;                                 // so that time-left and speed are not displayed
            PostMessage(HWindow, WM_TIMER, IDT_UPDATESTATUS, 0); // send one more timer so that the "paused" status is displayed
        }

        char** data = (char**)lParam;
        switch (wParam)
        {
        case 0:
        {
            CFileErrorDlg dlg(HWindow, data[1], data[2], data[3]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 1:
        {
            char tmpName[MAX_PATH];
            char tmpName2[MAX_PATH];
            char *filename1, *filename2;
            if (Script != NULL && Script->RemapNameFrom != NULL)
            {
                filename1 = RemapNames(tmpName, MAX_PATH, data[1], Script);
                filename2 = RemapNames(tmpName2, MAX_PATH, data[3], Script);
            }
            else
            {
                filename1 = data[1];
                filename2 = data[3];
            }
            COverwriteDlg dlg(HWindow, filename1, data[2], filename2, data[4]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 2:
        {
            CHiddenOrSystemDlg dlg(HWindow, data[1], data[2], data[3]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 3:
        {
            CCannotMoveDlg dlg(HWindow, IDD_CANNOTMOVE, data[1], data[2], data[3]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 4:
        {
            CCannotMoveDlg dlg(HWindow, IDD_RENAMEDIR, data[1], data[2], data[3]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 5:
        {
            CFileErrorDlg dlg(HWindow, data[0], data[1], data[2], FALSE, IDD_ERROR3);
            dlg.Execute();
            break;
        }

        case 6:
        {
            CErrorReadingADSDlg dlg(HWindow, data[1], data[2]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 7:
        {
            COverwriteDlg dlg(HWindow, data[1], data[2], data[3], data[4], FALSE, TRUE);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 8:
        {
            CFileErrorDlg dlg(HWindow, data[1], data[2], data[3], FALSE, IDD_CANNOTOPENADS);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 9:
        {
            CErrorSettingAttrsDlg dlg(HWindow, data[1], (DWORD)(DWORD_PTR)data[2], (DWORD)(DWORD_PTR)data[3]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 10:
        {
            CErrorCopyingPermissionsDlg dlg(HWindow, data[1], data[2], (DWORD)(DWORD_PTR)data[3]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 11:
        {
            CErrorCopyingDirTimeDlg dlg(HWindow, data[1], (DWORD)(DWORD_PTR)data[2]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }

        case 12:
        {
            CConfirmEncryptionLossDlg dlg(HWindow, (BOOL)(INT_PTR)data[1], data[2], (BOOL)(INT_PTR)data[3]);
            *(int*)data[0] = (int)dlg.Execute();
            break;
        }
        }

        StatusPaused = FALSE; // now time-left and speed can be displayed again

        if (Status != NULL && ShowPause)
        { // the operation continues, show the status again
            NextTimeLeftUpdateTime = GetTickCount();
            SetTimer(HWindow, IDT_UPDATESTATUS, IDT_UPDATESTATUS_PERIOD, NULL);
            if (Script != NULL)
                Script->InitSpeedMeters(TRUE);
        }

        //      BeginSuspendMode();  // we are doing something again ...

        if (canFlash)
            FlashWindow(RunningInOwnThread ? HWindow : MainWindow->HWindow, FALSE);
        AcceptCommands = TRUE;
        return TRUE;
    }

    case WM_TIMER:
    {
        if (wParam == IDT_REPAINT)
        {
            if (!FlushCachedData()) // WM_USER_SETDIALOG did not arrive, we can safely cancel the timer
            {
                TimerIsRunning = FALSE;
                KillTimer(HWindow, IDT_REPAINT);
            }
            return 0;
        }
        if (wParam == IDT_UPDATESTATUS)
        {
            // text operation status (transfer speed, etc.)
            if (Status != NULL)
            {
                char buf[300];
                buf[0] = 0;
                char num1[100];
                char num2[100];
                if (Script != NULL)
                {
                    CQuadWord transferredFileSize, transferSpeed, progressSize, progressSpeed;
                    BOOL useSpeedLimit;
                    DWORD speedLimit;
                    Script->GetStatus(&transferredFileSize, &transferSpeed, &progressSize, &progressSpeed,
                                      &useSpeedLimit, &speedLimit);

                    if (!Script->FastMoveUsed)
                    {
                        PrintDiskSize(num1, transferredFileSize, 4);
                        if (transferredFileSize <= Script->TotalFileSize)
                        {
                            PrintDiskSize(num2, Script->TotalFileSize, 4);
                            sprintf(buf, LoadStr(Script->IsCopyOperation ? IDS_PROGDLGSTATUSCOPY : IDS_PROGDLGSTATUSMOVE),
                                    num1, num2);
                        }
                        else
                            strcpy(buf, num1);
                    }
                    int len = (int)strlen(buf);

                    if (!StatusPaused && ShowPause && transferSpeed.Value > 0)
                    {
                        if (len > 0)
                        {
                            buf[len++] = ',';
                            buf[len++] = ' ';
                        }
                        PrintDiskSize(num1, transferSpeed, 4);
                        if (useSpeedLimit)
                        {
                            PrintDiskSize(num2, CQuadWord(speedLimit, 0), 4);
                            sprintf(buf + len, LoadStr(IDS_PROGDLGTRRATELIM), num1, num2);
                        }
                        else
                            sprintf(buf + len, LoadStr(IDS_PROGDLGTRRATE), num1);
                        len = (int)strlen(buf);
                    }

                    DWORD ti = GetTickCount();
                    if (!StatusPaused && ShowPause && progressSpeed.Value > 0 && Script->TotalSize > progressSize)
                    {
                        if (len > 0)
                        {
                            buf[len++] = ',';
                            buf[len++] = ' ';
                        }

                        CQuadWord secs = (Script->TotalSize - progressSize) / progressSpeed; // estimate of remaining seconds
                                                                                             /*
              SYSTEMTIME st;
              GetLocalTime(&st);
              FILETIME ft;
              SystemTimeToFileTime(&st, &ft);
              *(unsigned __int64 *)&ft = *(unsigned __int64 *)&ft + secs.Value * 1000 * 1000 * 10;
*/
                        secs.Value++;                                                        // add one second so we finish the operation with "time left: 1 sec" (instead of 0 sec)

                        // rounding calculation (roughly 10% error + rounded to nice numbers 1,2,5,10,20,40)
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
                        secs = ((secs + dif / CQuadWord(2, 0)) / dif) * dif; // round 'secs' to 'dif' seconds

                        if ((int)(ti - NextTimeLeftUpdateTime) >= 0 ||                         // time to display a new value
                            (secs > (TimeLeftLastValue * CQuadWord(3, 0)) / CQuadWord(2, 0) || // or the new value differs by more than 50%
                             secs < TimeLeftLastValue / CQuadWord(2, 0)))
                        {
                            TimeLeftLastValue = secs;
                            // slow down updating the time-left information based on the calculated time (the longer the time, the fewer the updates)
                            if (secs.Value <= 10)
                                NextTimeLeftUpdateTime = 500;
                            else if (secs.Value <= 30)
                                NextTimeLeftUpdateTime = 1000;
                            else if (secs.Value <= 60)
                                NextTimeLeftUpdateTime = 2000;
                            else if (secs.Value <= 300)
                                NextTimeLeftUpdateTime = 5000;
                            else
                                NextTimeLeftUpdateTime = 10000;
                            NextTimeLeftUpdateTime += ti - IDT_UPDATESTATUS_PERIOD / 2;
                        }
                        else
                            secs = TimeLeftLastValue; // otherwise show the old value (to keep the time-left estimate from changing too often)

                        PrintTimeLeft(num1, secs);
                        sprintf(buf + len, LoadStr(IDS_PROGDLGTIMELEFT), num1);

                        /*
              len = strlen(buf);
              FileTimeToSystemTime(&ft, &st);
              sprintf(buf + len, " (done: %d:%02d:%02d)", st.wHour, st.wMinute, st.wSecond);
*/
                    }
                    else
                    {
                        TimeLeftLastValue.SetUI64(0);
                        NextTimeLeftUpdateTime = ti;
                    }
                }
                Status->SetText(buf);
            }
            return 0;
        }
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (!AcceptCommands || !CanClose || CancelWorker || Script == NULL)
            return 0;

        Script->ChangeSpeedLimit = TRUE; // the speed limit may change, let the worker stop at an "appropriate" point
        if (ShowPause)
            ResetEvent(WorkerNotSuspended);

        // show the cache so it does not redraw under menus or dialogs
        FlushCachedData();

        if (Status != NULL && !AutoPaused)
        { // we must stop showing the status
            KillTimer(HWindow, IDT_UPDATESTATUS);
            StatusPaused = TRUE;                                 // so that time-left and speed are not displayed
            SendMessage(HWindow, WM_TIMER, IDT_UPDATESTATUS, 0); // send one more timer so the "stopped" status is displayed
        }

        HWND hCtrl = GetDlgItem(HWindow, (int)wParam);
        RECT r;
        GetWindowRect(hCtrl, &r);

        BOOL setSpeedLimit = FALSE;
        CMenuPopup* popup = new CMenuPopup;
        if (popup != NULL)
        {
            /* used by the export_mnu.py script that generates salmenu.mnu for the Translator
   keep synchronized with the InsertItem() calls below...
MENU_TEMPLATE_ITEM ProgressDialogMenu1[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_PROGDLGPAUSE
  {MNTT_IT, IDS_PROGDLGSETSPLIM
  {MNTT_IT, IDS_PROGDLGAUTOPAUSE
  {MNTT_PE, 0
};
MENU_TEMPLATE_ITEM ProgressDialogMenu2[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_PROGDLGRESUME
  {MNTT_IT, IDS_PROGDLGSETSPLIM
  {MNTT_IT, IDS_PROGDLGAUTOPAUSE
  {MNTT_PE, 0
};
*/
            MENU_ITEM_INFO mii;
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID;
            mii.Type = MENU_TYPE_STRING;
            mii.String = LoadStr(ShowPause ? IDS_PROGDLGPAUSE : IDS_PROGDLGRESUME);
            mii.ID = 1;
            popup->InsertItem(-1, TRUE, &mii);

            mii.String = LoadStr(IDS_PROGDLGSETSPLIM);
            mii.ID = 2;
            popup->InsertItem(-1, TRUE, &mii);

            if (IsInQueue)
            {
                mii.Mask |= MENU_MASK_STATE;
                mii.State = OperationsQueue.GetNumOfOperations() > 1 ? 0 : MENU_STATE_GRAYED;
                mii.String = LoadStr(IDS_PROGDLGAUTOPAUSE);
                mii.ID = 3;
                popup->InsertItem(-1, TRUE, &mii);
                mii.Mask &= ~MENU_MASK_STATE;
            }

            BOOL selectMenuItem = LOWORD(lParam);
            DWORD flags = MENU_TRACK_RETURNCMD;
            if (selectMenuItem)
            {
                popup->SetSelectedItemIndex(0);
                flags |= MENU_TRACK_SELECT;
            }
            switch (popup->Track(flags, r.left, r.bottom, HWindow, &r))
            {
            case 1:
                PostMessage(HWindow, WM_COMMAND, IDB_PAUSERESUME, 0);
                break; // Pause / Resume
            case 2:
                setSpeedLimit = TRUE;
                break; // Set Speed Limit...

            case 3: // Wait Until All Other Copy/Move Operations are Finished
            {
                HWND activateOperDlg = NULL;
                OperationsQueue.AutoPauseOperation(HWindow, &activateOperDlg);
                AutoPaused = TRUE;
                ShowPause = FALSE;
                SetDlgItemText(HWindow, IDB_PAUSERESUME, LoadStr(IDS_PROGDLGRESUME));
                PostMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDB_MINIMIZE), TRUE);
                PostMessage(HWindow, WM_COMMAND, IDB_MINIMIZE, 0); // minimize the "waiting" operation immediately (nothing to watch, saves one step for the user)
                if (activateOperDlg != NULL)
                {
                    PostMessage(activateOperDlg, WM_USER_FOCUSPROGRDLG, 0, 0);
                    NextForegroundWindow = NULL;
                }
                break;
            }
            }
            delete popup;
        }

        if (setSpeedLimit && Script != NULL)
        {
            BOOL useSpeedLimit;
            DWORD speedLimit;
            Script->GetSpeedLimit(&useSpeedLimit, &speedLimit);
            if (!useSpeedLimit && speedLimit == 1)
                speedLimit = Configuration.LastUsedSpeedLimit; // on first enable of speed limit use its last used value
            CSetSpeedLimDialog dlg(HWindow, &useSpeedLimit, &speedLimit);
            if (!AutoPaused)
                SendMessage(HWindow, WM_TIMER, IDT_UPDATESTATUS, 0); // send one more timer so status gets updated (some data might have finished copying)
            if (dlg.Execute() == IDOK)
            {
                if (useSpeedLimit)
                    Configuration.LastUsedSpeedLimit = speedLimit;
                Script->SetSpeedLimit(useSpeedLimit, speedLimit);
            }
            if (!AutoPaused)
                SendMessage(HWindow, WM_TIMER, IDT_UPDATESTATUS, 0); // send one more timer so status gets updated (some data might have finished copying)
        }

        StatusPaused = FALSE; // now time-left and speed can be displayed again

        if (Status != NULL && ShowPause)
        { // the operation continues, show the status again
            NextTimeLeftUpdateTime = GetTickCount();
            SetTimer(HWindow, IDT_UPDATESTATUS, IDT_UPDATESTATUS_PERIOD, NULL);
            if (Script != NULL)
                Script->InitSpeedMeters(TRUE);
        }

        if (Script != NULL)
            Script->ChangeSpeedLimit = FALSE;
        if (ShowPause)
            SetEvent(WorkerNotSuspended);

        break;
    }

    case WM_USER_CANCELPROGRDLG:
    {
        if (CanClose)
        {
            if (!IsWindowEnabled(HWindow)) // there is a modal dialog above this dialog (a message box asking about operation canceling or reporting an error)
                CloseAllOwnedEnabledDialogs(HWindow);
            CancelWorker = TRUE; // set worker cancel
            EnableWindow(GetDlgItem(HWindow, IDB_PAUSERESUME), FALSE);
            if (WorkerNotSuspended != NULL)
                SetEvent(WorkerNotSuspended); // so that Cancel proceeds even after Pause is pressed
            DoNotBeepOnClose = TRUE;
        }
        return TRUE; // message processed
    }

    case WM_USER_PROGRDLG_UPDATEICON:
    {
        SetWindowIcon();
        return TRUE; // message processed
    }

    case WM_USER_FOCUSPROGRDLG:
    {
        if (IsIconic(RunningInOwnThread ? HWindow : MainWindow->HWindow))
        {
            SetDlgTitle(FALSE);
            if (RunningInOwnThread)
                ShowWindow(HWindow, SW_RESTORE); // activate minimized dlg
            else
                RestoreApp(MainWindow->HWindow, HWindow);
            if (Worker != NULL)
                SetThreadPriority(Worker, THREAD_PRIORITY_NORMAL);
            AcceptCommands = TRUE;
        }
        if (!IsWindowEnabled(HWindow)) // there is a modal window above this dialog (a message box asking about operation canceling or reporting an error)
        {
            HWND dlg = GetLastActivePopup(HWindow);
            if (dlg != NULL && dlg != HWindow)
                SetForegroundWindow(dlg);
        }
        else
            SetForegroundWindow(HWindow);
        return TRUE; // message processed
    }

    case WM_SYSCOMMAND:
    {
        if (wParam == SC_MINIMIZE || wParam == SC_MINIMIZE + 2)
        {
            PostMessage(HWindow, WM_COMMAND, IDB_MINIMIZE, 0);
            return TRUE;
        }
        if (RunningInOwnThread && (wParam == SC_RESTORE || wParam == SC_RESTORE + 2))
        { // window is being restored - we must react:
            SetDlgTitle(FALSE);
            if (Worker != NULL)
                SetThreadPriority(Worker, THREAD_PRIORITY_NORMAL);
            AcceptCommands = TRUE;
        }
        break;
    }

    case WM_USER_PROGRDLGEND: // probably needless on W2K+: we had to delay the dialog termination a bit
    {
        uMsg = WM_COMMAND;
        if (RunningInOwnThread && !Configuration.AlwaysOnTop && GetForegroundWindow() == HWindow)
        {
            // Prior to Windows Vista we only called GetNextWindow(), which was enough to reach the next window in the Z-order.
            // Vista introduced new hidden helper windows such as "MSCTFIME UI" and "Default IME" that sit between us and 
            // our window (main, viver, ect.). So we skip hidden windows here.
            BOOL valid;
            HWND hNext = HWindow;
            do
            {
                hNext = GetNextWindow(hNext, GW_HWNDNEXT);
                //          TRACE_I("WM_USER_PROGRDLGEND hNext=0x"<<std::hex<<hNext);
                if (hNext != NULL)
                    valid = IsWindowVisible(hNext) && IsWindowEnabled(hNext);
                else
                    valid = FALSE;
            } while (hNext != NULL && hNext != HWindow && !valid);
            NextForegroundWindow = hNext;
        }
        if (IsInQueue)
        {
            HWND activateOperDlg = NULL;
            OperationsQueue.OperationEnded(HWindow, FALSE, &activateOperDlg);
            if (activateOperDlg != NULL)
            {
                PostMessage(activateOperDlg, WM_USER_FOCUSPROGRDLG, 0, 0);
                NextForegroundWindow = NULL;
            }
        }
        IsInQueue = FALSE;
        break;
    }

    case WM_COMMAND:
    {
        if (WorkerNotSuspended == NULL || Worker == NULL)
            return TRUE; // the dialog has not fully started yet, ignore the command

        switch (LOWORD(wParam))
        {
        case IDOK: // operation finished, called only by the worker, must not come from the dialog
        {
            if (!IsWindowEnabled(HWindow)) // if a modal dialog is open we must locate and close it
                CloseAllOwnedEnabledDialogs(HWindow);
            if (InSendMessage())
                ReplyMessage(0);                   // let the worker continue
            Script = NULL;                         // script is freed in the worker thread, so we must not use it here anymore
            SetEvent(WContinue);                   // worker may start deleting the script
            WaitForSingleObject(Worker, INFINITE); // wait until it finishes and exits
            HANDLES(CloseHandle(Worker));
            Worker = NULL;
            HANDLES(CloseHandle(WorkerNotSuspended));
            WorkerNotSuspended = NULL;
            HANDLES(CloseHandle(WContinue));
            WContinue = NULL;
            if (RunningInOwnThread)
                ProgressDlgArray.ClearDlgWindow(HWindow);
            if (CancelWorker)
                wParam = IDCANCEL;

            if (!DoNotBeepOnClose && Configuration.MinBeepWhenDone && GetForegroundWindow() != HWindow)
                MessageBeep(0);
            PostMessage(HWindow, WM_USER_PROGRDLGEND, wParam, lParam); // probably needless on W2K+: delay ending the dialog a bit
            return TRUE;
        }

        case IDCANCEL: // abort
        {
            if (CanClose && !CancelWorker)
            {
                ResetEvent(WorkerNotSuspended);

                //            EndSuspendMode();  // work interruption, time to refresh
                //            MainWindow->RefreshDiskFreeSpace();

                // show the cache so it does not redraw under the message box
                FlushCachedData();

                if (Status != NULL)
                { // we must stop showing the status
                    KillTimer(HWindow, IDT_UPDATESTATUS);
                    StatusPaused = TRUE;                                 // so that time-left and speed are not displayed
                    PostMessage(HWindow, WM_TIMER, IDT_UPDATESTATUS, 0); // send one more timer so the "stopped" status is displayed
                }

                int ret = SalMessageBox(HWindow, LoadStr(IDS_CANCELOPERATION),
                                        LoadStr(IDS_QUESTION),
                                        MB_YESNO | MB_ICONQUESTION /*| MSGBOXEX_ESCAPEENABLED*/); // Escape key is not a good
                // idea -- Zarevak accidentally started deleting a large batch of files, then began hitting Escape (the machine was 
                // heavily loaded so it did not respond immediately) canceling the confirmation, therefore the confirmation can 
                // no longer be closed with Escape.

                //            BeginSuspendMode();  // we are doing something again ...

                StatusPaused = FALSE; // now time-left and speed can be displayed again

                if (ret == IDYES)
                {
                    CancelWorker = TRUE; // set cancel of the worker
                    EnableWindow(GetDlgItem(HWindow, IDB_PAUSERESUME), FALSE);
                }
                else
                {
                    if (Status != NULL && ShowPause)
                    { // the operation continues, show the status again
                        NextTimeLeftUpdateTime = GetTickCount();
                        SetTimer(HWindow, IDT_UPDATESTATUS, IDT_UPDATESTATUS_PERIOD, NULL);
                        if (Script != NULL)
                            Script->InitSpeedMeters(TRUE);
                    }
                }
                if ((CancelWorker || ShowPause) && // only if it's Cancel or the operation isn't paused
                    WorkerNotSuspended != NULL)
                {
                    SetEvent(WorkerNotSuspended); // may be NULL if the message box was closed from IDOK via WM_CLOSE
                }
            }
            return TRUE; // only the worker using IDOK will terminate the dialog ...
        }

        case CM_RESUMEOPER:   // resume posted from the operations queue
        case IDB_PAUSERESUME: // pause/resume
        {
            if ((LOWORD(wParam) == CM_RESUMEOPER || AcceptCommands) && CanClose && !CancelWorker)
            {
                AutoPaused = FALSE; // this may be a manual pause/resume or an automatic resume
                BOOL speedMetersInitCalled = FALSE;
                if (LOWORD(wParam) != CM_RESUMEOPER || IsWindowEnabled(HWindow)) // nothing open above the dialog (a Cancel question may pop up)
                {
                    if (ShowPause)
                        ResetEvent(WorkerNotSuspended);
                    else
                    {
                        if (Status != NULL && Script != NULL)
                        {
                            Script->InitSpeedMeters(TRUE);
                            speedMetersInitCalled = TRUE;
                        }
                        SetEvent(WorkerNotSuspended);
                    }
                    ShowPause = !ShowPause;
                }
                if (IsInQueue)
                    OperationsQueue.SetPaused(HWindow, !ShowPause ? 2 /* manually paused */ : 0 /* running */);
                SetDlgItemText(HWindow, IDB_PAUSERESUME, LoadStr(ShowPause ? IDS_PROGDLGPAUSE : IDS_PROGDLGRESUME));
                SetDlgTitle(IsIconic(RunningInOwnThread ? HWindow : MainWindow->HWindow));

                if (Status != NULL)
                {
                    if (ShowPause)
                    {
                        NextTimeLeftUpdateTime = GetTickCount();
                        SetTimer(HWindow, IDT_UPDATESTATUS, IDT_UPDATESTATUS_PERIOD, NULL);
                        if (!speedMetersInitCalled && Script != NULL)
                            Script->InitSpeedMeters(TRUE);
                    }
                    else // we must stop showing the status
                    {
                        KillTimer(HWindow, IDT_UPDATESTATUS);
                        PostMessage(HWindow, WM_TIMER, IDT_UPDATESTATUS, 0); // send one more timer so that the "paused" status is displayed
                    }
                }
            }
            return TRUE;
        }

        case IDB_MINIMIZE:
        {
            if (!AcceptCommands)
                return TRUE;
            AcceptCommands = FALSE;

            ResetEvent(WorkerNotSuspended);
            // must drain messages from the Worker, otherwise
            // MinimizeApp calls them and the app behaves incorrectly
            MSG msg; // cannot be called recursively (AcceptCommands == FALSE) -> ok
            BOOL oldCanClose = CanClose;
            CanClose = FALSE; // do not allow closing, we are inside a method
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            CanClose = oldCanClose;

            if (RunningInOwnThread)
                ShowWindow(HWindow, SW_MINIMIZE);
            else
                MinimizeApp(MainWindow->HWindow); // it has its own message loop (ShowWindow) !!!
            if (Worker != NULL)
                SetThreadPriority(Worker, THREAD_PRIORITY_BELOW_NORMAL);
            SetDlgTitle(TRUE);

            if (ShowPause)
                SetEvent(WorkerNotSuspended);

            return TRUE;
        }
        }
        break;
    }

    case WM_ACTIVATEAPP:
    {
        if (wParam == TRUE && IsIconic(RunningInOwnThread ? HWindow : MainWindow->HWindow))
        {
            SetDlgTitle(FALSE);
            if (!RunningInOwnThread)
                RestoreApp(MainWindow->HWindow, HWindow);
            if (Worker != NULL)
                SetThreadPriority(Worker, THREAD_PRIORITY_NORMAL);
            AcceptCommands = TRUE;
        }
        if (wParam == FALSE) // when deactivated we leave directories shown in the panels,
        {                    // so they can be deleted, disconnected, etc. from other software
            if (!RunningInOwnThread && CanChangeDirectory())
                SetCurrentDirectoryToSystem();
        }
        break;
    }

    case WM_DESTROY:
    {
        if (TimerIsRunning)
        {
            KillTimer(HWindow, IDT_REPAINT);
            TimerIsRunning = FALSE;
        }

        // on XP service pack 1 with a top-most window open after closing this dialog
        // system may not pick the correct foreground window (according to Z-order) and instead
        // brings an illogical top-most window to the foreground - the following workaround satisfies users;
        // foreground is set only to Salamander windows that are "main" (without a parent, or
        // parent disabled)
        //
        // note: under Windows Vista the problem occurs if we copy and above the progress
        // window a question pops up (overwrite confirmation, cancel confirmation, etc.) -- this then
        // messes up the Z-order and the system without the following hack after closing the progress window
        // activates the top-most window that is the highest; see \Source\zorder and
        // \Source\windowtest utilities.
        // Issue reported here: https://forum.altap.cz/viewtopic.php?t=2922&start=15
        if (NextForegroundWindow != NULL && NextForegroundWindow != GetForegroundWindow())
        {
            DWORD pid;
            GetWindowThreadProcessId(NextForegroundWindow, &pid);
            HWND parent = ::GetParent(NextForegroundWindow);
            if (pid == GetCurrentProcessId() && (parent == NULL || !IsWindowEnabled(parent)) &&
                IsWindowVisible(NextForegroundWindow))
            {
                SetForegroundWindow(NextForegroundWindow);
            }
        }

        if (!RunningInOwnThread)
            MainWindow->SetWindowTitle();
        break;
    }

    default:
    {
        if (TaskbarBtnCreatedMsg != 0 && uMsg == TaskbarBtnCreatedMsg)
            TaskBarList3.Init(HWindow);
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CFileErrorDlg
//

CFileErrorDlg::CFileErrorDlg(HWND parent, const char* caption, const char* file, const char* error,
                             BOOL noSkip, int altRes) : CCommonDialog(HLanguage, altRes == 0 ? (noSkip ? IDD_CREATEDIRERR : IDD_CANNOTOPEN) : altRes, parent)
{
    Caption = caption;
    File = file;
    Error = error;
}

INT_PTR
CFileErrorDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFileErrorDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetWindowText(HWindow, Caption);

        CStaticText* name;
        if ((name = new CStaticText(HWindow, IDS_FILENAME, STF_PATH_ELLIPSIS)) != NULL)
            name->SetTextToDblQuotesIfNeeded(File);
        else
            TRACE_E(LOW_MEMORY);

        if (ResID == IDD_CANNOTOPENADS)
            new CButton(HWindow, IDB_IGNORE, BTF_DROPDOWN);

        SetWindowText(GetDlgItem(HWindow, IDS_ERROR), Error);
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (wParam == IDB_IGNORE)
        {
            HWND hCtrl = GetDlgItem(HWindow, (int)wParam);
            RECT r;
            GetWindowRect(hCtrl, &r);

            CMenuPopup menu;
            MENU_ITEM_INFO mii;
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID | MENU_MASK_STATE;
            mii.Type = MENU_TYPE_STRING;
            mii.State = 0;

            char buf[100];
            if (GetDlgItemText(HWindow, IDB_IGNORE, buf, 100))
            {
                /* used by the export_mnu.py script that generates salmenu.mnu for the Translator
   keep synchronized with the InsertItem() calls below...
MENU_TEMPLATE_ITEM FileErrorDlgMenu[] = 
{
  {MNTT_PB, 0
  // TRANSLATOR_INSERT: Control: IDD_CANNOTOPENADS IDB_IGNORE
  {MNTT_IT, IDS_ERROPENADS_IGNOREALL
  {MNTT_PE, 0
};
*/
                mii.String = buf;
                mii.ID = 1;
                menu.InsertItem(-1, TRUE, &mii);

                mii.String = LoadStr(IDS_ERROPENADS_IGNOREALL);
                mii.ID = 2;
                menu.InsertItem(-1, TRUE, &mii);

                BOOL selectMenuItem = LOWORD(lParam);
                DWORD flags = MENU_TRACK_RETURNCMD;
                if (selectMenuItem)
                {
                    menu.SetSelectedItemIndex(0);
                    flags |= MENU_TRACK_SELECT;
                }
                switch (menu.Track(flags, r.left, r.bottom, HWindow, &r))
                {
                case 1:
                    PostMessage(HWindow, WM_COMMAND, IDB_IGNORE, 0);
                    break;
                case 2:
                    PostMessage(HWindow, WM_COMMAND, IDB_IGNOREALL, 0);
                    break;
                }
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_SKIP || LOWORD(wParam) == IDB_SKIPALL ||
            LOWORD(wParam) == IDRETRY || LOWORD(wParam) == IDB_IGNORE ||
            LOWORD(wParam) == IDB_IGNOREALL)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// COverwriteDlg
//

COverwriteDlg::COverwriteDlg(HWND parent, const char* sourceName, const char* sourceAttr,
                             const char* targetName, const char* targetAttr, BOOL yesnocancel,
                             BOOL dirOverwrite) : CCommonDialog(HLanguage,
                                                                dirOverwrite ? IDD_DIROVERWRITE : (yesnocancel ? IDD_OVERWRITE2 : IDD_OVERWRITE),
                                                                parent)
{
    SourceName = sourceName;
    SourceAttr = sourceAttr;
    TargetName = targetName;
    TargetAttr = targetAttr;
}

INT_PTR
COverwriteDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("COverwriteDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CStaticText *source, *target;
        if ((source = new CStaticText(HWindow, IDS_SOURCENAME, STF_PATH_ELLIPSIS)) != NULL)
            source->SetTextToDblQuotesIfNeeded(SourceName);
        else
            TRACE_E(LOW_MEMORY);
        if ((target = new CStaticText(HWindow, IDS_TARGETNAME, STF_PATH_ELLIPSIS)) != NULL)
            target->SetTextToDblQuotesIfNeeded(TargetName);
        else
            TRACE_E(LOW_MEMORY);

        SetWindowText(GetDlgItem(HWindow, IDS_SOURCEATTR), SourceAttr);
        SetWindowText(GetDlgItem(HWindow, IDS_TARGETATTR), TargetAttr);
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDNO || LOWORD(wParam) == IDB_SKIP ||
            LOWORD(wParam) == IDB_SKIPALL || LOWORD(wParam) == IDB_ALL ||
            LOWORD(wParam) == IDYES)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CHiddenOrSystemDlg
//

CHiddenOrSystemDlg::CHiddenOrSystemDlg(HWND parent, const char* caption, const char* name,
                                       const char* error, BOOL yesnocancel, BOOL yesallcancel) : CCommonDialog(HLanguage, yesnocancel ? (yesallcancel ? IDD_QUESTION3 : IDD_QUESTION2) : IDD_SYSTEMORHIDDEN, parent)
{
    Caption = caption;
    Name = name;
    Error = error;
}

INT_PTR
CHiddenOrSystemDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CHiddenOrSystemDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetWindowText(HWindow, Caption);

        CStaticText* name;
        if ((name = new CStaticText(HWindow, IDS_FILENAME, STF_PATH_ELLIPSIS)) != NULL)
            name->SetTextToDblQuotesIfNeeded(Name);
        else
            TRACE_E(LOW_MEMORY);

        SetWindowText(GetDlgItem(HWindow, IDS_ERROR), Error);
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_SKIP || LOWORD(wParam) == IDB_SKIPALL ||
            LOWORD(wParam) == IDB_ALL || LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCannotMoveDlg
//

CCannotMoveDlg::CCannotMoveDlg(HWND parent, int resID, char* sourceName,
                               char* targetName, char* error) : CCommonDialog(HLanguage, resID, parent)
{
    SourceName = sourceName;
    TargetName = targetName;
    Error = error;
}

INT_PTR
CCannotMoveDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCannotMoveDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CStaticText *source, *target;
        if ((source = new CStaticText(HWindow, IDS_SOURCENAME, STF_PATH_ELLIPSIS)) != NULL)
            source->SetTextToDblQuotesIfNeeded(SourceName);
        else
            TRACE_E(LOW_MEMORY);
        if ((target = new CStaticText(HWindow, IDS_TARGETNAME, STF_PATH_ELLIPSIS)) != NULL)
            target->SetTextToDblQuotesIfNeeded(TargetName);
        else
            TRACE_E(LOW_MEMORY);

        SetWindowText(GetDlgItem(HWindow, IDS_ERROR), Error);
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_SKIP || LOWORD(wParam) == IDB_SKIPALL ||
            LOWORD(wParam) == IDRETRY)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CFileListDialog
//

void BrowseFileName(HWND hParent, int editlineResID, const char* name)
{
    CALL_STACK_MESSAGE3("BrowseFileName(, %d, %s)", editlineResID, name);
    char file[MAX_PATH];
    strcpy(file, name);
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hParent;
    char* s = LoadStr(IDS_ALLFILTER);
    ofn.lpstrFilter = s;
    while (*s != 0) // creating a double-null terminated list
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.nFilterIndex = 1;
    //  ofn.lpstrFileTitle = file;
    //  ofn.nMaxFileTitle = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (SafeGetSaveFileName(&ofn))
    {
        if (SalGetFullName(file))
        {
            SendMessage(GetDlgItem(hParent, editlineResID), WM_SETTEXT, 0, (LPARAM)file);
        }
    }
}

CFileListDialog::CFileListDialog(HWND parent)
    : CCommonDialog(HLanguage, IDD_FILELIST, IDD_FILELIST, parent)
{
    EditLine = new CComboboxEdit();
}

CFileListDialog::~CFileListDialog()
{
}

void CFileListDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFileListDialog::Transfer()");
    ti.RadioButton(IDC_FL_CLIPBOARD, 0, Configuration.FileListDestination);
    ti.RadioButton(IDC_FL_VIEWER, 1, Configuration.FileListDestination);
    ti.RadioButton(IDC_FL_FILE, 2, Configuration.FileListDestination);
    ti.EditLine(IDC_FL_FILENAME, Configuration.FileListName, MAX_PATH);
    ti.CheckBox(IDC_FL_APPEND, Configuration.FileListAppend);

    char** history = Configuration.FileListHistory;
    HWND hWnd;
    if (ti.GetControl(hWnd, IDC_FL_LINE))
    {
        if (ti.Type == ttDataToWindow)
        {
            LoadComboFromStdHistoryValues(hWnd, history, FILELIST_HISTORY_SIZE);
            SendMessage(hWnd, CB_LIMITTEXT, MAX_PATH - 1, 0);
            const char* text = "";
            if (history[0] != NULL)
                text = history[0];
            SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)text);
        }
        else
        {
            char buff[MAX_PATH];
            SendMessage(hWnd, WM_GETTEXT, MAX_PATH, (LPARAM)buff);
            AddValueToStdHistoryValues(history, FILELIST_HISTORY_SIZE, buff, FALSE);
        }
    }

    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CFileListDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFileListDialog::Validate()");
    HWND hWnd;

    if (ti.GetControl(hWnd, IDC_FL_LINE))
    {
        char buff[MAX_PATH];
        SendMessage(hWnd, WM_GETTEXT, MAX_PATH, (LPARAM)buff);
        int errorPos1, errorPos2;
        if (!ValidateMakeFileList(HWindow, buff, errorPos1, errorPos2))
        {
            ti.ErrorOn(IDC_FL_LINE);
            PostMessage(EditLine->HWindow, EM_SETSEL, errorPos1, errorPos2);
            return;
        }
    }

    BOOL file = IsDlgButtonChecked(HWindow, IDC_FL_FILE);
    if (file)
    {
        if (ti.GetControl(hWnd, IDC_FL_FILENAME))
        {
            // DefaultDir restoration
            MainWindow->UpdateDefaultDir(TRUE);

            char buffFile[MAX_PATH];
            SendMessage(hWnd, WM_GETTEXT, MAX_PATH, (LPARAM)buffFile);
            int errTextID;
            if (!SalGetFullName(buffFile, &errTextID, MainWindow->GetActivePanel()->Is(ptDisk) ? MainWindow->GetActivePanel()->GetPath() : NULL))
            {
                SalMessageBox(HWindow, LoadStr(errTextID), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(IDC_FL_FILENAME);
                return;
            }
            if (!ValidatePathIsNotEmpty(HWindow, buffFile))
            {
                ti.ErrorOn(IDC_FL_FILENAME);
                return;
            }

            BOOL append;
            ti.CheckBox(IDC_FL_APPEND, append);

            // must not be a directory
            DWORD attr;
            attr = SalGetFileAttributes(buffFile);

            if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY))
            {
                SalMessageBox(HWindow, LoadStr(IDS_NAMEALREADYUSEDFORDIR),
                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(IDC_FL_FILENAME);
                return;
            }
            // if not appending, ask whether to overwrite
            if (!append && attr != 0xFFFFFFFF)
            {
                char text[300];
                sprintf(text, LoadStr(IDS_FILEALREADYEXIST), buffFile);
                if (SalMessageBox(HWindow, text, LoadStr(IDS_QUESTION),
                                  MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
                {
                    ti.ErrorOn(IDC_FL_FILENAME);
                    return;
                }
            }
        }
    }
}

void CFileListDialog::EnableControls()
{
    BOOL file = IsDlgButtonChecked(HWindow, IDC_FL_FILE);
    EnableWindow(GetDlgItem(HWindow, IDC_FL_FILENAME), file);
    EnableWindow(GetDlgItem(HWindow, IDC_FL_FNBROWSE), file);
    EnableWindow(GetDlgItem(HWindow, IDC_FL_APPEND), file);
}

INT_PTR
CFileListDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFileListDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CHyperLink* hl = new CHyperLink(HWindow, IDC_FL_LINE_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_FILELISTLINE_HINT));

        InstallWordBreakProc(GetDlgItem(HWindow, IDC_FL_FILENAME)); // install WordBreakProc into the editline

        HWND hCombo = GetDlgItem(HWindow, IDC_FL_LINE);
        EditLine->AttachToWindow(GetWindow(hCombo, GW_CHILD));

        ChangeToArrowButton(HWindow, IDC_FL_LINEBROWSE);
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
            EnableControls();

        switch (LOWORD(wParam))
        {
        case IDC_FL_FNBROWSE:
        {
            char buffFile[MAX_PATH];
            // DefaultDir restoration
            MainWindow->UpdateDefaultDir(TRUE);

            SendMessage(GetDlgItem(HWindow, IDC_FL_FILENAME), WM_GETTEXT, MAX_PATH, (LPARAM)buffFile);
            if (!SalGetFullName(buffFile, NULL, MainWindow->GetActivePanel()->Is(ptDisk) ? MainWindow->GetActivePanel()->GetPath() : NULL))
            { // we cannot do it, so let Windows Browse handle it however it wants...
                SendMessage(GetDlgItem(HWindow, IDC_FL_FILENAME), WM_GETTEXT, MAX_PATH, (LPARAM)buffFile);
            }

            BrowseFileName(HWindow, IDC_FL_FILENAME, buffFile);
            return 0;
        }

        case IDC_FL_LINEBROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_FL_LINEBROWSE, IDC_FL_LINE,
                             TRUE, MakeFileListItems);
            return 0;
        }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CBetaExpiredDialog
//

#ifdef USE_BETA_EXPIRATION_DATE

CBetaExpiredDialog::CBetaExpiredDialog(HWND parent)
    : CCommonDialog(HLanguage, IDD_BETAEXPIRED, parent)
{
    // our beta expired, but we will run for 14 more days with just a 3s wait; afterwards 30s
    SYSTEMTIME st;
    GetLocalTime(&st);
    __int64 currentFT;
    SystemTimeToFileTime(&st, (FILETIME*)&currentFT);

    __int64 expireFT;
    SystemTimeToFileTime(&BETA_EXPIRATION_DATE, (FILETIME*)&expireFT);

    if (currentFT > expireFT + (__int64)10000000 * 60 * 60 * 24 * 14) // 14 days
        Count = 30;
    else
        Count = 3;
}

void CBetaExpiredDialog::OnTimer()
{
    char buff[20];
    sprintf(buff, "%d", Count);
    SetDlgItemText(HWindow, IDOK, buff);
    Count--;
}

INT_PTR
CBetaExpiredDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CBetaExpiredDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CHyperLink* hl;
        hl = new CHyperLink(HWindow, IDC_BETAEXPIREDURL);
        if (hl != NULL)
        {
            // if the environment is Czech or Slovak, we will automatically show the Czech version of the web
            BOOL english = LanguageID != 0x405 /* Czech */ && LanguageID != 0x41B /* Slovak */;

            const char* url =
#ifndef THIS_IS_EAP_VERSION
                english ? "https://www.altap.cz/salamander/downloads/beta" : "https://www.altap.cz/cz/salamander/downloads/beta";
#else  // THIS_IS_EAP_VERSION
                english ? "https://www.altap.cz/salamander/downloads/eap" : "https://www.altap.cz/cz/salamander/downloads/eap";
#endif // THIS_IS_EAP_VERSION

            SetDlgItemText(HWindow, IDC_BETAEXPIREDURL, url + 8);
            hl->SetActionOpen(url);
        }
        char orig[200];
        GetDlgItemText(HWindow, IDC_BETAEXPIREDDATE, orig, 200);

        SYSTEMTIME st;
        GetLocalTime(&st);

        char buff[400];
        char today[100];
        char expired[100];
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, today, 100) == 0)
            sprintf(today, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &BETA_EXPIRATION_DATE, NULL, expired, 100) == 0)
            sprintf(expired, "%u.%u.%u", BETA_EXPIRATION_DATE.wDay, BETA_EXPIRATION_DATE.wMonth, BETA_EXPIRATION_DATE.wYear);

        sprintf(buff, orig, today, expired);
        SetDlgItemText(HWindow, IDC_BETAEXPIREDDATE, buff);

        // the OK button will show numbers counting down, store the original text
        GetDlgItemText(HWindow, IDOK, OldOK, 100);

        EnableWindow(GetDlgItem(HWindow, IDOK), FALSE);
        OnTimer();
        SetTimer(HWindow, 1, 1000, NULL);

        CCommonDialog::DialogProc(uMsg, wParam, lParam);
        return 0;
    }

    case WM_TIMER:
    {
        if (Count == 0)
        {
            KillTimer(HWindow, 1);
            SetDlgItemText(HWindow, IDOK, OldOK);
            EnableWindow(GetDlgItem(HWindow, IDOK), TRUE);
        }
        else
        {
            OnTimer();
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

#endif // USE_BETA_EXPIRATION_DATE

//
// ****************************************************************************
// CSetSpeedLimDialog
//

CSetSpeedLimDialog::CSetSpeedLimDialog(HWND parent, BOOL* useSpeedLim, DWORD* speedLimit)
    : CCommonDialog(HLanguage, IDD_SETSPEEDLIM, IDD_SETSPEEDLIM, parent)
{
    UseSpeedLim = useSpeedLim;
    SpeedLimit = speedLimit;
}

BOOL GetSpeedLimit(int sel, char* speedLimitText, DWORD* returnSpeedLimit); // is implemented in dialogs3.cpp

void CSetSpeedLimDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CSetSpeedLimDialog::Validate()");
    BOOL useSpeedLim;
    ti.RadioButton(IDC_SETSPLIMDONTUSE, FALSE, useSpeedLim);
    ti.RadioButton(IDC_SETSPLIMUSE, TRUE, useSpeedLim);
    if (useSpeedLim)
    {
        int sel = (int)SendDlgItemMessage(HWindow, IDC_SETSPLIMUNITS, CB_GETCURSEL, 0, 0);
        char speedLimitText[20];
        GetDlgItemText(HWindow, IDE_SETSPLIMNUMBER, speedLimitText, 20);
        if (!GetSpeedLimit(sel, speedLimitText, NULL))
        {
            SalMessageBox(HWindow, LoadStr(IDS_SPEEDLIMITSIZE), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_SETSPLIMNUMBER);
        }
    }
}

void CSetSpeedLimDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CSetSpeedLimDialog::Transfer()");
    ti.RadioButton(IDC_SETSPLIMDONTUSE, FALSE, *UseSpeedLim);
    ti.RadioButton(IDC_SETSPLIMUSE, TRUE, *UseSpeedLim);

    if (ti.Type == ttDataToWindow)
    {
        DWORD speedLimNum = *SpeedLimit;
        int speedLimUnits = 0;
        if (speedLimNum == 0xFFFFFFFF)
        {
            speedLimNum = 4;
            speedLimUnits = 3;
        }
        else
        {
            while (speedLimNum % 1024 == 0)
            {
                speedLimNum /= 1024;
                speedLimUnits++;
                if (speedLimNum == 0 || speedLimUnits > 3) // should not happen, just for peace of mind
                {
                    TRACE_E("CSetSpeedLimDialog::Transfer(): unexpected situation!");
                    speedLimNum = 4;
                    speedLimUnits = 3;
                    break;
                }
            }
        }

        HWND speedLimitUnits = GetDlgItem(HWindow, IDC_SETSPLIMUNITS);
        SendMessage(speedLimitUnits, CB_RESETCONTENT, 0, 0);
        SendMessage(speedLimitUnits, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SPEED_B_per_s));
        SendMessage(speedLimitUnits, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SPEED_KB_per_s));
        SendMessage(speedLimitUnits, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SPEED_MB_per_s));
        SendMessage(speedLimitUnits, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SPEED_GB_per_s));
        SendMessage(speedLimitUnits, CB_SETCURSEL, speedLimUnits, 0);

        HWND speedLimit = GetDlgItem(HWindow, IDE_SETSPLIMNUMBER);
        char num[20];
        sprintf(num, "%u", speedLimNum);
        SetWindowText(speedLimit, num);
        SendMessage(speedLimit, EM_LIMITTEXT, 19, 0);

        EnableControls();
    }
    else
    {
        if (*UseSpeedLim)
        {
            int sel = (int)SendDlgItemMessage(HWindow, IDC_SETSPLIMUNITS, CB_GETCURSEL, 0, 0);
            char speedLimitText[20];
            GetDlgItemText(HWindow, IDE_SETSPLIMNUMBER, speedLimitText, 20);
            if (!GetSpeedLimit(sel, speedLimitText, SpeedLimit))
                *UseSpeedLim = FALSE;
        }
    }
}

void CSetSpeedLimDialog::EnableControls()
{
    BOOL speedLimit = IsDlgButtonChecked(HWindow, IDC_SETSPLIMUSE);
    EnableWindow(GetDlgItem(HWindow, IDE_SETSPLIMNUMBER), speedLimit);
    EnableWindow(GetDlgItem(HWindow, IDC_SETSPLIMUNITS), speedLimit);
}

INT_PTR
CSetSpeedLimDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED &&
            (LOWORD(wParam) == IDC_SETSPLIMDONTUSE || LOWORD(wParam) == IDC_SETSPLIMUSE))
        {
            EnableControls();
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
