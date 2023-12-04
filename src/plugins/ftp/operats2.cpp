// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// COperationDlgThread
//

class COperationDlgThread : public CThread
{
protected:
    COperationDlg* OperDlg;
    BOOL AlwaysOnTop;
    HWND DropTargetWnd;

public:
    COperationDlgThread(COperationDlg* operDlg, HWND dropTargetWnd) : CThread("Operation Dialog")
    {
        OperDlg = operDlg;
        AlwaysOnTop = FALSE;
        SalamanderGeneral->GetConfigParameter(SALCFG_ALWAYSONTOP, &AlwaysOnTop, sizeof(AlwaysOnTop), NULL);
        DropTargetWnd = dropTargetWnd;
    }

    virtual unsigned Body()
    {
        CALL_STACK_MESSAGE1("COperationDlgThread::Body()");

        // 'sendWMClose': dialog nastavi na TRUE v pripade, ze doslo k prijeti WM_CLOSE
        // v okamziku, kdy je otevreny modalni dialog nad dialogem operace - az se tento
        // modalni dialog ukonci, posle se WM_CLOSE znovu do dialogu operace
        BOOL sendWMClose = FALSE;
        OperDlg->SendWMClose = &sendWMClose;
        if (OperDlg->Create() == NULL || OperDlg->CloseDlg)
        {
            if (!OperDlg->CloseDlg)
                OperDlg->Oper->SetOperationDlg(NULL);
            if (OperDlg->HWindow != NULL)
                DestroyWindow(OperDlg->HWindow); // WM_CLOSE nemuze dojit, protoze ho nema co dorucit (message-loopa zatim nebezi)
        }
        else
        {
            HWND dlg = OperDlg->HWindow; // bezpecne ulozeny handle okna (platny i po destrukci OperDlg)
            if (AlwaysOnTop)             // always-on-top osetrime aspon "staticky" (neni v system menu)
                SetWindowPos(dlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            SetForegroundWindow(dlg);

            // aby se pri drag&dropu aktivoval drop-target a ne dialog operace
            if (DropTargetWnd != NULL)
                SalamanderGeneral->ActivateDropTarget(DropTargetWnd, dlg);

            // message-loopa - pockame do ukonceni nemodalniho dialogu
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0))
            {
                if (!IsDialogMessage(dlg, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                if (sendWMClose)
                {
                    sendWMClose = FALSE;
                    PostMessage(dlg, WM_CLOSE, 0, 0);
                }
            }
        }
        delete OperDlg;
        return 0;
    }
};

//
// ****************************************************************************
// CFTPOperation
//

BOOL CFTPOperation::SetConnection(CFTPProxyServer* proxyServer, const char* host, unsigned short port,
                                  const char* user, const char* password, const char* account,
                                  const char* initFTPCommands, BOOL usePassiveMode,
                                  const char* listCommand, DWORD serverIP,
                                  const char* serverSystem, const char* serverFirstReply,
                                  BOOL useListingsCache, DWORD hostIP)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetConnection()");

    BOOL err = (host == NULL || *host == 0);
    Host = SalamanderGeneral->DupStr(host);
    Port = port;
    User = SalamanderGeneral->DupStr((user != NULL && *user == 0) ? NULL : user); // je-li NULL, zustane NULL
    Password = SalamanderGeneral->DupStr((password != NULL && *password == 0) ? NULL : password);
    Account = SalamanderGeneral->DupStr((account != NULL && *account == 0) ? NULL : account);
    InitFTPCommands = SalamanderGeneral->DupStr((initFTPCommands != NULL && *initFTPCommands == 0) ? NULL : initFTPCommands);
    UsePassiveMode = usePassiveMode;
    SizeCmdIsSupported = TRUE;
    ListCommand = SalamanderGeneral->DupStr(listCommand);
    ServerSystem = SalamanderGeneral->DupStr(serverSystem);
    ServerFirstReply = SalamanderGeneral->DupStr(serverFirstReply);
    ServerIP = serverIP;
    UseListingsCache = useListingsCache;
    HostIP = hostIP;

    if (proxyServer != NULL)
    {
        ProxyServer = proxyServer->MakeCopy();
        if (ProxyServer == NULL)
            err = TRUE;
    }
    CFTPProxyServerType proxyType = fpstNotUsed;
    if (ProxyServer != NULL)
        proxyType = ProxyServer->ProxyType;
    if (proxyType == fpstOwnScript)
        ProxyScriptText = ProxyServer->ProxyScript;
    else
    {
        ProxyScriptText = GetProxyScriptText(proxyType, FALSE);
        if (ProxyScriptText[0] == 0)
            ProxyScriptText = GetProxyScriptText(fpstNotUsed, FALSE); // nedefinovany skript = "not used (direct connection)" skript - SOCKS 4/4A/5, HTTP 1.1
    }
    if (!err)
    {
        CProxyScriptParams proxyScriptParams(ProxyServer, Host, Port, User, Password, Account,
                                             Password == NULL || Password[0] == 0);
        char connectToHost[HOST_MAX_SIZE];
        char errBuf[300];
        if (ProcessProxyScript(ProxyScriptText, &ProxyScriptStartExecPoint, -1,
                               &proxyScriptParams, connectToHost, &ConnectToPort,
                               NULL, NULL, errBuf, NULL))
        {
            if (proxyScriptParams.NeedUserInput()) // teoreticky by nemelo nastat (navic jiz overene spustenim v panelu)
            {
                err = TRUE;
                TRACE_E("CFTPOperation::SetConnection(): unexpected situation: proxy script needs user input!");
            }
            else
                ConnectToHost = SalamanderGeneral->DupStr(connectToHost);
        }
        else // teoreticky by nikdy nemelo nastat (ulozene skripty jsou validovane, navic jiz overene spustenim v panelu)
        {
            err = TRUE;
            TRACE_E("CFTPOperation::SetConnection(): proxy script error: " << errBuf);
        }
    }
    return !err;
}

void CFTPOperation::SetBasicData(char* operationSubject, const char* listingServerType)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetBasicData()");

    OperationSubject = SalamanderGeneral->DupStr(operationSubject);
    ListingServerType = SalamanderGeneral->DupStr(listingServerType); // je-li NULL, zustane NULL
}

void CFTPOperation::SetOperationDelete(const char* sourcePath, char srcPathSeparator,
                                       BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                                       int confirmDelOnNonEmptyDir, int confirmDelOnHiddenFile,
                                       int confirmDelOnHiddenDir)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetOperationDelete()");

    Type = fotDelete;
    SourcePath = SalamanderGeneral->DupStr(sourcePath);
    SrcPathSeparator = srcPathSeparator;
    SrcPathCanChange = srcPathCanChange;
    SrcPathCanChangeInclSubdirs = srcPathCanChangeInclSubdirs;
    ConfirmDelOnNonEmptyDir = confirmDelOnNonEmptyDir;
    ConfirmDelOnHiddenFile = confirmDelOnHiddenFile;
    ConfirmDelOnHiddenDir = confirmDelOnHiddenDir;
}

BOOL CFTPOperation::SetOperationCopyMoveDownload(BOOL isCopy, const char* sourcePath,
                                                 char srcPathSeparator, BOOL srcPathCanChange,
                                                 BOOL srcPathCanChangeInclSubdirs, const char* targetPath,
                                                 char tgtPathSeparator, BOOL tgtPathCanChange,
                                                 BOOL tgtPathCanChangeInclSubdirs, const char* asciiFileMasks,
                                                 int autodetectTrMode, int useAsciiTransferMode,
                                                 int cannotCreateFile, int cannotCreateDir,
                                                 int fileAlreadyExists, int dirAlreadyExists, int retryOnCreatedFile,
                                                 int retryOnResumedFile, int asciiTrModeButBinFile)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetOperationCopyMoveDownload()");

    Type = isCopy ? fotCopyDownload : fotMoveDownload;
    BOOL err = FALSE;
    SourcePath = SalamanderGeneral->DupStr(sourcePath);
    SrcPathSeparator = srcPathSeparator;
    SrcPathCanChange = srcPathCanChange;
    SrcPathCanChangeInclSubdirs = srcPathCanChangeInclSubdirs;
    TargetPath = SalamanderGeneral->DupStr(targetPath);
    TgtPathSeparator = tgtPathSeparator;
    TgtPathCanChange = tgtPathCanChange;
    TgtPathCanChangeInclSubdirs = tgtPathCanChangeInclSubdirs;
    if (asciiFileMasks != NULL) // neprazdna maska, jinak nema smysl tvorit group-mask objekt
    {
        ASCIIFileMasks = SalamanderGeneral->AllocSalamanderMaskGroup();
        if (ASCIIFileMasks != NULL)
        {
            ASCIIFileMasks->SetMasksString(asciiFileMasks, FALSE);
            int errorPos;
            if (!ASCIIFileMasks->PrepareMasks(errorPos))
                err = TRUE;
        }
        else
            err = TRUE;
    }
    AutodetectTrMode = autodetectTrMode;
    UseAsciiTransferMode = useAsciiTransferMode;
    CannotCreateFile = cannotCreateFile;
    CannotCreateDir = cannotCreateDir;
    FileAlreadyExists = fileAlreadyExists;
    DirAlreadyExists = dirAlreadyExists;
    RetryOnCreatedFile = retryOnCreatedFile;
    RetryOnResumedFile = retryOnResumedFile;
    AsciiTrModeButBinFile = asciiTrModeButBinFile;
    return !err;
}

BOOL CFTPOperation::SetOperationCopyMoveUpload(BOOL isCopy, const char* sourcePath, char srcPathSeparator,
                                               BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                                               const char* targetPath, char tgtPathSeparator,
                                               BOOL tgtPathCanChange, BOOL tgtPathCanChangeInclSubdirs,
                                               const char* asciiFileMasks, int autodetectTrMode,
                                               int useAsciiTransferMode, int uploadCannotCreateFile,
                                               int uploadCannotCreateDir, int uploadFileAlreadyExists,
                                               int uploadDirAlreadyExists, int uploadRetryOnCreatedFile,
                                               int uploadRetryOnResumedFile, int uploadAsciiTrModeButBinFile)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetOperationCopyMoveUpload()");

    Type = isCopy ? fotCopyUpload : fotMoveUpload;
    BOOL err = FALSE;
    SourcePath = SalamanderGeneral->DupStr(sourcePath);
    SrcPathSeparator = srcPathSeparator;
    SrcPathCanChange = srcPathCanChange;
    SrcPathCanChangeInclSubdirs = srcPathCanChangeInclSubdirs;
    TargetPath = SalamanderGeneral->DupStr(targetPath);
    TgtPathSeparator = tgtPathSeparator;
    TgtPathCanChange = tgtPathCanChange;
    TgtPathCanChangeInclSubdirs = tgtPathCanChangeInclSubdirs;
    if (asciiFileMasks != NULL) // neprazdna maska, jinak nema smysl tvorit group-mask objekt
    {
        ASCIIFileMasks = SalamanderGeneral->AllocSalamanderMaskGroup();
        if (ASCIIFileMasks != NULL)
        {
            ASCIIFileMasks->SetMasksString(asciiFileMasks, FALSE);
            int errorPos;
            if (!ASCIIFileMasks->PrepareMasks(errorPos))
                err = TRUE;
        }
        else
            err = TRUE;
    }
    AutodetectTrMode = autodetectTrMode;
    UseAsciiTransferMode = useAsciiTransferMode;
    UploadCannotCreateFile = uploadCannotCreateFile;
    UploadCannotCreateDir = uploadCannotCreateDir;
    UploadFileAlreadyExists = uploadFileAlreadyExists;
    UploadDirAlreadyExists = uploadDirAlreadyExists;
    UploadRetryOnCreatedFile = uploadRetryOnCreatedFile;
    UploadRetryOnResumedFile = uploadRetryOnResumedFile;
    UploadAsciiTrModeButBinFile = uploadAsciiTrModeButBinFile;
    return !err;
}

void CFTPOperation::SetOperationChAttr(const char* sourcePath, char srcPathSeparator,
                                       BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                                       WORD attrAnd, WORD attrOr, int chAttrOfFiles, int chAttrOfDirs,
                                       int unknownAttrs)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetOperationChAttr()");

    Type = fotChangeAttrs;
    SourcePath = SalamanderGeneral->DupStr(sourcePath);
    SrcPathSeparator = srcPathSeparator;
    SrcPathCanChange = srcPathCanChange;
    SrcPathCanChangeInclSubdirs = srcPathCanChangeInclSubdirs;
    AttrAnd = attrAnd;
    AttrOr = attrOr;
    ChAttrOfFiles = chAttrOfFiles;
    ChAttrOfDirs = chAttrOfDirs;
    UnknownAttrs = unknownAttrs;
}

void CFTPOperation::SetQueue(CFTPQueue* queue)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetQueue()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (Queue == NULL)
        Queue = queue;
    else
    {
        TRACE_E("Unexpected situation in CFTPOperation::SetQueue(): queue already exists!");
        delete queue;
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

CFTPQueue*
CFTPOperation::GetQueue()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetQueue()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (Queue == NULL)
        TRACE_E("Unexpected situation in CFTPOperation::GetQueue(): queue doesn't exist!");
    CFTPQueue* ret = Queue;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

CFTPWorker*
CFTPOperation::AllocNewWorker()
{
    CALL_STACK_MESSAGE1("CFTPOperation::AllocNewWorker()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    CFTPWorker* ret = new CFTPWorker(this, Queue, Host, Port, User);
    if (ret == NULL)
        TRACE_E(LOW_MEMORY);
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::SendHeaderToLog(int logUID)
{
    CALL_STACK_MESSAGE2("CFTPOperation::SendHeaderToLog(%d)", logUID);

    HANDLES(EnterCriticalSection(&OperCritSect));
    char buf[500];
    char timeBuf[100];
    BOOL ok = TRUE;
    // sestaveni titulku okna
    int titleResID = 0;
    switch (Type)
    {
    case fotDelete:
        titleResID = IDS_OPERDLGDELETETITLE;
        break;
    case fotCopyDownload:
        titleResID = IDS_OPERDLGCOPYDOWNLOADTITLE;
        break;
    case fotMoveDownload:
        titleResID = IDS_OPERDLGMOVEDOWNLOADTITLE;
        break;
    case fotCopyUpload:
        titleResID = IDS_OPERDLGCOPYUPLOADTITLE;
        break;
    case fotMoveUpload:
        titleResID = IDS_OPERDLGMOVEUPLOADTITLE;
        break;
    case fotChangeAttrs:
        titleResID = IDS_OPERDLGCHATTRSTITLE;
        break;
    default:
        TRACE_E("CFTPOperation::SendHeaderToLog(): unknown operation type!");
        ok = FALSE;
        break;
    }
    if (ok)
    {
        _snprintf_s(buf, 498, _TRUNCATE, LoadStr(titleResID), OperationSubject, Host);
        strcat(buf, "\r\n");
        Logs.LogMessage(logUID, buf, -1);

        SYSTEMTIME st;
        GetLocalTime(&st);
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, timeBuf, 50) == 0)
            sprintf(timeBuf, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        strcat(timeBuf, " - ");
        if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, timeBuf + strlen(timeBuf), 50) == 0)
            sprintf(timeBuf + strlen(timeBuf), "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        sprintf(buf, LoadStr(IDS_WORKERLOGHEADER), Host, Port, logUID, timeBuf);
        Logs.LogMessage(logUID, buf, -1);
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetChildItems(int notDone, int skipped, int failed, int uiNeeded)
{
    CALL_STACK_MESSAGE5("CFTPOperation::SetChildItems(%d, %d, %d, %d)",
                        notDone, skipped, failed, uiNeeded);

    HANDLES(EnterCriticalSection(&OperCritSect));
    ChildItemsNotDone = notDone;
    ChildItemsSkipped = skipped;
    ChildItemsFailed = failed;
    ChildItemsUINeeded = uiNeeded;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::AddToNotDoneSkippedFailed(int notDone, int skipped, int failed, int uiNeeded,
                                              BOOL onlyUINeededOrFailedToSkipped)
{
    SLOW_CALL_STACK_MESSAGE1("CFTPOperation::AddToNotDoneSkippedFailed()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    ChildItemsNotDone += notDone;
    ChildItemsSkipped += skipped;
    ChildItemsFailed += failed;
    ChildItemsUINeeded += uiNeeded;
    if (ChildItemsNotDone < 0 || ChildItemsSkipped < 0 || ChildItemsFailed < 0 || ChildItemsUINeeded < 0)
    {
        TRACE_E("Unexpected situation in CFTPOperation::AddToNotDoneSkippedFailed(): some counter is negative! "
                "NotDone="
                << ChildItemsNotDone << ", Skipped=" << ChildItemsSkipped << ", Failed=" << ChildItemsFailed << ", UINeeded=" << ChildItemsUINeeded);
    }
    COperationState state = GetOperationState(FALSE);
    if (LastReportedOperState != state)
    {
        if (state == opstInProgress) // doslo k retry, vracime se k praci, musime vynulovat globalni meric rychlosti (nemuzeme pocitat prumer za dobu, kdy se cekalo na usera)
        {
            GlobalTransferSpeedMeter.Clear();
            GlobalTransferSpeedMeter.JustConnected();
            GlobalLastActivityTime.Set(GetTickCount()); // retry je aktivita
        }
        ReportOperationStateChange();
    }
    if (state == opstSuccessfullyFinished ||
        !onlyUINeededOrFailedToSkipped && (state == opstFinishedWithSkips || state == opstFinishedWithErrors))
    {
        BOOL softRefresh = state == opstFinishedWithErrors || // FIXME: az bude existovat okno s frontou operaci, budeme muset OperationDlg->DlgWillCloseIfOpFinWithSkips nahradit jinou detekci jestli dojde k zavreni workeru (predani connectiony zpet do panelu)
                           state == opstFinishedWithSkips && (OperationDlg != NULL ? !OperationDlg->DlgWillCloseIfOpFinWithSkips : TRUE);
        PostChangeOnPathNotifications(softRefresh); // uz je volna linka (aspon co se tyce teto operace), muzeme si dovolit refreshe listingu
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

BOOL CFTPOperation::IsASCIIFile(const char* name, const char* ext)
{
    CALL_STACK_MESSAGE_NONE

    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = ASCIIFileMasks != NULL && ASCIIFileMasks->AgreeMasks(name, ext);
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::AddToTotalSize(const CQuadWord& size, BOOL sizeInBytes)
{
    CALL_STACK_MESSAGE1("CFTPOperation::AddToTotalSize(,)");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (sizeInBytes)
        TotalSizeInBytes += size;
    else
        TotalSizeInBlocks += size;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SubFromTotalSize(const CQuadWord& size, BOOL sizeInBytes)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SubFromTotalSize(,)");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (sizeInBytes)
        TotalSizeInBytes -= size;
    else
        TotalSizeInBlocks -= size;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetOperationDlg(COperationDlg* operDlg)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetOperationDlg()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    OperationDlg = operDlg;
    ReportChangeInWorkerID = -2;  // zmena dialogu, resetneme reporteni zmen
    ReportProgressChange = FALSE; // zmena dialogu, resetneme reporteni zmen
    ReportChangeInItemUID = -3;   // zmena dialogu, resetneme reporteni zmen
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

BOOL CFTPOperation::ActivateOperationDlg(HWND dropTargetWnd)
{
    CALL_STACK_MESSAGE1("CFTPOperation::ActivateOperationDlg()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = FALSE;
    if (OperationDlg != NULL)
    {
        if (OperationDlg->HWindow != NULL) // "always true" (jinak: dialog se teprve otevre, cimz se sam aktivuje)
        {
            if (IsIconic(OperationDlg->HWindow))
                ShowWindow(OperationDlg->HWindow, SW_RESTORE);
            SetForegroundWindow(OperationDlg->HWindow);
        }
        ret = TRUE;
    }
    else
    {
        OperationDlg = new COperationDlg(NULL, SalamanderGeneral->GetMainWindowHWND(),
                                         this, Queue, &WorkersList);
        ReportChangeInWorkerID = -2;  // zmena dialogu, resetneme reporteni zmen
        ReportProgressChange = FALSE; // zmena dialogu, resetneme reporteni zmen
        ReportChangeInItemUID = -3;   // zmena dialogu, resetneme reporteni zmen
        if (OperationDlg != NULL)
        {
            COperationDlgThread* t = new COperationDlgThread(OperationDlg, dropTargetWnd);
            if (t != NULL)
            {
                if ((OperationDlgThread = t->Create(AuxThreadQueue)) == NULL)
                { // thread se nepustil, error
                    delete t;
                    delete OperationDlg;
                    OperationDlg = NULL;
                    ReportChangeInWorkerID = -2;  // zmena dialogu, resetneme reporteni zmen
                    ReportProgressChange = FALSE; // zmena dialogu, resetneme reporteni zmen
                    ReportChangeInItemUID = -3;   // zmena dialogu, resetneme reporteni zmen
                }
                else
                    ret = TRUE; // uspech
            }
            else // malo pameti, error
            {
                delete OperationDlg;
                OperationDlg = NULL;
                ReportChangeInWorkerID = -2;  // zmena dialogu, resetneme reporteni zmen
                ReportProgressChange = FALSE; // zmena dialogu, resetneme reporteni zmen
                ReportChangeInItemUID = -3;   // zmena dialogu, resetneme reporteni zmen
                TRACE_E(LOW_MEMORY);
            }
        }
        else
            TRACE_E(LOW_MEMORY); // malo pameti, error
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::CloseOperationDlg(HANDLE* dlgThread)
{
    CALL_STACK_MESSAGE1("CFTPOperation::CloseOperationDlg()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (OperationDlg != NULL)
    {
        OperationDlg->CloseDlg = TRUE; // k CloseDlg neni synchronizovany pristup, snad zbytecne
        if (OperationDlg->HWindow != NULL)
            PostMessage(OperationDlg->HWindow, WM_CLOSE, 0, 0);
        OperationDlg = NULL;          // dealokaci zajisti thread dialogu
        ReportChangeInWorkerID = -2;  // zmena dialogu, resetneme reporteni zmen
        ReportProgressChange = FALSE; // zmena dialogu, resetneme reporteni zmen
        ReportChangeInItemUID = -3;   // zmena dialogu, resetneme reporteni zmen
    }
    if (dlgThread != NULL)
        *dlgThread = OperationDlgThread;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

BOOL CFTPOperation::AddWorker(CFTPWorker* newWorker)
{
    CALL_STACK_MESSAGE1("CFTPOperation::AddWorker()");
    BOOL ret = WorkersList.AddWorker(newWorker); // synchronizace je uvnitr WorkersList (sekce OperCritSect zde neni potreba)
    if (ret)
    {
        GlobalLastActivityTime.Set(GetTickCount()); // pridani workera je aktivita
        OperationStatusMaybeChanged();
    }
    return ret;
}

void CFTPOperation::OperationStatusMaybeChanged()
{
    CALL_STACK_MESSAGE1("CFTPOperation::OperationStatusMaybeChanged()");

    BOOL someIsWorkingAndNotPaused;
    WorkersList.SomeWorkerIsWorking(&someIsWorkingAndNotPaused);
    BOOL paused = !someIsWorkingAndNotPaused;
    BOOL stopping = WorkersList.EmptyOrAllShouldStop();

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (!paused &&
        ChildItemsNotDone - ChildItemsSkipped - ChildItemsFailed - ChildItemsUINeeded <= 0)
    { // workeri bezi, ale polozky nebezi => operace nebezi
        paused = TRUE;
    }
    if (paused || stopping) // operace ma byt "paused"
    {
        if (OperationEnd == -1) // operace bezi
        {
            OperationEnd = GetTickCount();
            if (OperationEnd == -1)
                OperationEnd++; // zamezime kolizi s hodnotou -1 (posuneme cas o 1 ms)
        }
    }
    else // operace ma byt "resumed"
    {
        if (OperationEnd != -1) // operace nebezi
        {
            GlobalTransferSpeedMeter.Clear();
            GlobalTransferSpeedMeter.JustConnected();

            OperationStart = GetTickCount() - (OperationEnd - OperationStart);
            OperationEnd = -1;
        }
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

BOOL CFTPOperation::InformWorkersAboutStop(int workerInd, CFTPWorker** victims,
                                           int maxVictims, int* foundVictims)
{
    CALL_STACK_MESSAGE1("CFTPOperation::InformWorkersAboutStop()");
    return WorkersList.InformWorkersAboutStop(workerInd, victims, maxVictims, foundVictims); // synchronizace je uvnitr WorkersList (sekce OperCritSect zde neni potreba)
}

BOOL CFTPOperation::InformWorkersAboutPause(int workerInd, CFTPWorker** victims,
                                            int maxVictims, int* foundVictims, BOOL pause)
{
    CALL_STACK_MESSAGE1("CFTPOperation::InformWorkersAboutPause()");
    return WorkersList.InformWorkersAboutPause(workerInd, victims, maxVictims, foundVictims, pause); // synchronizace je uvnitr WorkersList (sekce OperCritSect zde neni potreba)
}

BOOL CFTPOperation::CanCloseWorkers(int workerInd)
{
    CALL_STACK_MESSAGE1("CFTPOperation::CanCloseWorkers()");
    return WorkersList.CanCloseWorkers(workerInd); // synchronizace je uvnitr WorkersList (sekce OperCritSect zde neni potreba)
}

BOOL CFTPOperation::ForceCloseWorkers(int workerInd, CFTPWorker** victims,
                                      int maxVictims, int* foundVictims)
{
    CALL_STACK_MESSAGE1("CFTPOperation::ForceCloseWorkers()");
    return WorkersList.ForceCloseWorkers(workerInd, victims, maxVictims, foundVictims); // synchronizace je uvnitr WorkersList (sekce OperCritSect zde neni potreba)
}

BOOL CFTPOperation::DeleteWorkers(int workerInd, CFTPWorker** victims,
                                  int maxVictims, int* foundVictims,
                                  CUploadWaitingWorker** uploadFirstWaitingWorker)
{
    CALL_STACK_MESSAGE1("CFTPOperation::DeleteWorkers()");
    BOOL ret = WorkersList.DeleteWorkers(workerInd, victims, maxVictims, foundVictims, uploadFirstWaitingWorker); // synchronizace je uvnitr WorkersList (sekce OperCritSect zde neni potreba)
    OperationStatusMaybeChanged();
    return ret;
}

BOOL CFTPOperation::InitOperDlg(COperationDlg* dlg)
{
    CALL_STACK_MESSAGE1("CFTPOperation::InitOperDlg()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ok = TRUE;

    // sestaveni titulku okna
    int titleResID = 0;
    switch (Type)
    {
    case fotDelete:
        titleResID = IDS_OPERDLGDELETETITLE;
        break;
    case fotCopyDownload:
        titleResID = IDS_OPERDLGCOPYDOWNLOADTITLE;
        break;
    case fotMoveDownload:
        titleResID = IDS_OPERDLGMOVEDOWNLOADTITLE;
        break;
    case fotCopyUpload:
        titleResID = IDS_OPERDLGCOPYUPLOADTITLE;
        break;
    case fotMoveUpload:
        titleResID = IDS_OPERDLGMOVEUPLOADTITLE;
        break;
    case fotChangeAttrs:
        titleResID = IDS_OPERDLGCHATTRSTITLE;
        break;
    default:
        TRACE_E("CFTPOperation::InitOperDlg(): unknown operation type!");
        ok = FALSE;
        break;
    }
    if (ok)
    {
        char title[500];
        _snprintf_s(title, _TRUNCATE, LoadStr(titleResID), OperationSubject, Host);
        dlg->TitleText = SalamanderGeneral->DupStr(title);
        if (dlg->TitleText == NULL)
            ok = FALSE;
    }

    // nastaveni zdrojove a cilove cesty
    if (ok)
    {
        dlg->Source->SetPathSeparator(SrcPathSeparator);
        if (Type == fotCopyDownload || Type == fotMoveDownload ||
            Type == fotCopyUpload || Type == fotMoveUpload)
        {
            SetDlgItemText(dlg->HWindow, IDT_OPSOURCETITLE, LoadStr(IDS_OPERDLGSRCPATH));
            SetDlgItemText(dlg->HWindow, IDT_OPTARGETTITLE, LoadStr(IDS_OPERDLGTGTPATH));
            dlg->Target->SetPathSeparator(TgtPathSeparator);
            if (!dlg->Source->SetText(SourcePath) ||
                !dlg->Target->SetText(TargetPath))
                ok = FALSE;
        }
        else // move + ch-attrs
        {
            SetDlgItemText(dlg->HWindow, IDT_OPSOURCETITLE, LoadStr(IDS_OPERDLGPATH));
            if (!dlg->Source->SetText(SourcePath))
                ok = FALSE;
        }
    }

    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ok;
}

BOOL CFTPOperation::GetServerAddress(DWORD* serverIP, char* host, int hostBufSize)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetServerAddress(, ,)");
    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = TRUE;
    *serverIP = ServerIP;
    if (ServerIP == INADDR_NONE) // IP adresa neni znama, vracime host-name
    {
        lstrcpyn(host, ConnectToHost, hostBufSize);
        ret = FALSE;
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::SetServerIP(DWORD serverIP)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetServerIP()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    if (ServerIP != serverIP && ServerIP != INADDR_NONE)
        TRACE_E("Unexpected situation in CFTPOperation::SetServerIP(): two different IP addresses for one host!");
    ServerIP = serverIP;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::GetConnectInfo(DWORD* serverIP, unsigned short* port, char* host,
                                   CFTPProxyServerType* proxyType, DWORD* hostIP, unsigned short* hostPort,
                                   char* proxyUser, char* proxyPassword)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetConnectInfo(, , ,)");
    HANDLES(EnterCriticalSection(&OperCritSect));
    *serverIP = ServerIP;
    *port = ConnectToPort;
    lstrcpyn(host, HandleNULLStr(Host), HOST_MAX_SIZE);
    *hostIP = HostIP;
    *hostPort = Port;
    *proxyType = fpstNotUsed;
    if (ProxyServer != NULL)
    {
        *proxyType = ProxyServer->ProxyType;
        lstrcpyn(proxyUser, HandleNULLStr(ProxyServer->ProxyUser), USER_MAX_SIZE);
        lstrcpyn(proxyPassword, HandleNULLStr(ProxyServer->ProxyPlainPassword), PASSWORD_MAX_SIZE);
    }
    else
    {
        proxyUser[0] = 0;
        proxyPassword[0] = 0;
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::GetConnectLogMsg(BOOL isReconnect, char* buf, int bufSize, int attemptNumber, const char* dateTime)
{
    CALL_STACK_MESSAGE2("CFTPOperation::GetConnectLogMsg(%d, , , ,)", isReconnect);
    HANDLES(EnterCriticalSection(&OperCritSect));
    in_addr srvAddr;
    srvAddr.s_addr = ServerIP;
    if (bufSize > 0)
    {
        if (isReconnect)
        {
            if (ProxyServer != NULL)
            {
                _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_PRXSRVRECONLOGHEADER), Host, Port, HandleNULLStr(User),
                            ProxyServer->ProxyName, GetProxyTypeName(ProxyServer->ProxyType),
                            HandleNULLStr(ProxyServer->ProxyHost), ProxyServer->ProxyPort,
                            HandleNULLStr(ProxyServer->ProxyUser), ConnectToHost, inet_ntoa(srvAddr),
                            ConnectToPort, attemptNumber, dateTime);
            }
            else
            {
                _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_RECONLOGHEADER), ConnectToHost, inet_ntoa(srvAddr), ConnectToPort,
                            attemptNumber, dateTime);
            }
        }
        else
        {
            if (ProxyServer != NULL)
            {
                _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_PRXSRVWORKERCONLOGHDR), Host, Port, HandleNULLStr(User),
                            ProxyServer->ProxyName, GetProxyTypeName(ProxyServer->ProxyType),
                            HandleNULLStr(ProxyServer->ProxyHost), ProxyServer->ProxyPort,
                            HandleNULLStr(ProxyServer->ProxyUser), ConnectToHost, inet_ntoa(srvAddr), ConnectToPort);
            }
            else
            {
                _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_WORKERCONLOGHDR), ConnectToHost, inet_ntoa(srvAddr), ConnectToPort);
            }
        }
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetServerSystem(const char* reply, int replySize)
{
    CALL_STACK_MESSAGE2("CFTPOperation::SetServerSystem(, %d)", replySize);
    HANDLES(EnterCriticalSection(&OperCritSect));
    if (ServerSystem == NULL)
    {
        char buf[700];
        CopyStr(buf, 700, reply, replySize); // ulozime prvni odpoved serveru (zdroj informaci o verzi serveru)
        ServerSystem = SalamanderGeneral->DupStr(buf);
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetServerFirstReply(const char* reply, int replySize)
{
    CALL_STACK_MESSAGE2("CFTPOperation::SetServerFirstReply(, %d)", replySize);
    HANDLES(EnterCriticalSection(&OperCritSect));
    if (ServerFirstReply == NULL)
    {
        char buf[700];
        CopyStr(buf, 700, reply, replySize); // ulozime prvni odpoved serveru (zdroj informaci o verzi serveru)
        ServerFirstReply = SalamanderGeneral->DupStr(buf);
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

BOOL CFTPOperation::PrepareNextScriptCmd(char* buf, int bufSize, char* logBuf, int logBufSize, int* cmdLen,
                                         const char** proxyScriptExecPoint, int proxyScriptLastCmdReply,
                                         char* errDescrBuf, BOOL* needUserInput)
{
    CALL_STACK_MESSAGE1("CFTPOperation::PrepareNextScriptCmd()");
    HANDLES(EnterCriticalSection(&OperCritSect));

    if (bufSize > 0)
        buf[0] = 0;
    if (logBufSize > 0)
        logBuf[0] = 0;
    *cmdLen = 0;
    errDescrBuf[0] = 0;
    *needUserInput = FALSE;

    CProxyScriptParams proxyScriptParams(ProxyServer, Host, Port, User, Password, Account,
                                         Password == NULL || Password[0] == 0);
    char proxySendCmdBuf[FTPCOMMAND_MAX_SIZE];
    char proxyLogCmdBuf[FTPCOMMAND_MAX_SIZE];
    BOOL ret = TRUE;
    if (*proxyScriptExecPoint == NULL)
        *proxyScriptExecPoint = ProxyScriptStartExecPoint; // mame pripravit prvni prikaz skriptu
    if (ProcessProxyScript(ProxyScriptText, proxyScriptExecPoint, proxyScriptLastCmdReply,
                           &proxyScriptParams, NULL, NULL, proxySendCmdBuf,
                           proxyLogCmdBuf, errDescrBuf, NULL))
    {
        if (proxyScriptParams.NeedUserInput()) // je potreba zadat nejake udaje (user, password, atd.)
        {
            *needUserInput = TRUE;
            int resID = 0;
            if (proxyScriptParams.NeedProxyHost)
            {
                resID = IDS_WORKERUNKNOWNPROXYHOST; // sice napiseme, ze ho potrebujeme, ale user ho neni schopen zadat - jestli to nekdy bude potreba (zatim by nemelo hrozit, protoze v panelu uz login probehl i bez ProxyHost, takze je predpoklad, ze ani tady nebude potreba), dopsat dialog pro zadavani ProxyHost...
                TRACE_E("CFTPOperation::PrepareNextScriptCmd(): unexpected situation: ProxyHost is empty!");
            }
            if (proxyScriptParams.NeedProxyPassword)
                resID = IDS_WORKERUNKNOWNPROXYPASSWORD;
            if (proxyScriptParams.NeedUser)
                TRACE_E("CFTPOperation::PrepareNextScriptCmd(): unexpected situation: User is empty!");
            if (proxyScriptParams.NeedPassword)
                resID = IDS_WORKERUNKNOWNPASSWD;
            if (proxyScriptParams.NeedAccount)
                resID = IDS_WORKERUNKNOWNACCOUNT;
            if (resID != 0)
                lstrcpyn(errDescrBuf, LoadStr(resID), 300);
        }
        else
        {
            if (proxySendCmdBuf[0] != 0) // mame prikaz k odeslani na server
            {
                lstrcpyn(buf, proxySendCmdBuf, bufSize);
                if (bufSize > 0)
                    *cmdLen = (int)strlen(buf);
                lstrcpyn(logBuf, proxyLogCmdBuf, logBufSize);
            }
            // else ; // konec login skriptu
        }
    }
    else // teoreticky by nikdy nemelo nastat (ulozene skripty jsou validovane)
    {
        ret = FALSE;
    }

    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::AllocProxyForDataCon(CFTPProxyForDataCon** newDataConProxyServer)
{
    CALL_STACK_MESSAGE1("CFTPOperation::AllocProxyForDataCon()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    *newDataConProxyServer = ProxyServer == NULL ? NULL : ProxyServer->AllocProxyForDataCon(ServerIP, Host, HostIP, Port);
    BOOL ret = ProxyServer == NULL || *newDataConProxyServer != NULL;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::GetRetryLoginWithoutAsking()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetRetryLoginWithoutAsking()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = RetryLoginWithoutAsking;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::GetInitFTPCommands(char* buf, int bufSize)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetInitFTPCommands(,)");
    HANDLES(EnterCriticalSection(&OperCritSect));
    lstrcpyn(buf, InitFTPCommands != NULL ? InitFTPCommands : "", bufSize);
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::GetLoginErrorDlgInfo(char* user, int userBufSize, char* password, int passwordBufSize,
                                         char* account, int accountBufSize, BOOL* retryLoginWithoutAsking,
                                         BOOL* proxyUsed, char* proxyUser, int proxyUserBufSize,
                                         char* proxyPassword, int proxyPasswordBufSize)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetLoginErrorDlgInfo()");
    char anonymousPasswd[PASSWORD_MAX_SIZE];
    Config.GetAnonymousPasswd(anonymousPasswd, PASSWORD_MAX_SIZE);
    HANDLES(EnterCriticalSection(&OperCritSect));
    lstrcpyn(user, User != NULL ? User : FTP_ANONYMOUS, userBufSize);
    lstrcpyn(password, Password != NULL ? Password : (User == NULL ? anonymousPasswd : ""), passwordBufSize);
    lstrcpyn(account, Account != NULL ? Account : "", accountBufSize);
    *proxyUsed = ProxyServer != NULL;
    if (ProxyServer != NULL)
    {
        lstrcpyn(proxyUser, HandleNULLStr(ProxyServer->ProxyUser), proxyUserBufSize);
        lstrcpyn(proxyPassword, HandleNULLStr(ProxyServer->ProxyPlainPassword), proxyPasswordBufSize);
    }
    else
    {
        if (proxyUserBufSize > 0)
            proxyUser[0] = 0;
        if (proxyPasswordBufSize > 0)
            proxyPassword[0] = 0;
    }
    *retryLoginWithoutAsking = RetryLoginWithoutAsking;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetLoginErrorDlgInfo(const char* password, const char* account, BOOL retryLoginWithoutAsking,
                                         BOOL proxyUsed, const char* proxyUser, const char* proxyPassword)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetLoginErrorDlgInfo()");
    char* n1 = SalamanderGeneral->DupStr((password != NULL && *password == 0) ? NULL : password);
    char* n2 = SalamanderGeneral->DupStr((account != NULL && *account == 0) ? NULL : account);
    HANDLES(EnterCriticalSection(&OperCritSect));
    if (Password != NULL)
        SalamanderGeneral->Free(Password);
    Password = n1;
    if (Account != NULL)
        SalamanderGeneral->Free(Account);
    Account = n2;
    RetryLoginWithoutAsking = retryLoginWithoutAsking;
    if (proxyUsed && ProxyServer != NULL)
    {
        ProxyServer->SetProxyUser(proxyUser);
        ProxyServer->SetProxyPassword(proxyPassword);
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::ReportWorkerChange(int workerID, BOOL reportProgressChange)
{
    CALL_STACK_MESSAGE1("CFTPOperation::ReportWorkerChange()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (OperationDlg != NULL && OperationDlg->HWindow != NULL) // jen pokud je dialog otevreny
    {
        if (reportProgressChange)
            ReportProgressChange = TRUE;
        if (ReportChangeInWorkerID == -2) // zatim nehlasime zadne zmeny
        {
            ReportChangeInWorkerID = workerID;
            PostMessage(OperationDlg->HWindow, WM_APP_WORKERCHANGEREP, 0, 0); // dame vedet dialogu, ze si ma prijit pro zmeny
        }
        else
        {
            if (ReportChangeInWorkerID != workerID)
                ReportChangeInWorkerID = -1; // zmeny ve vice nez jednom workerovi
        }
    }
    else
        ReportChangeInWorkerID = -2; // dialog neni otevreny, nema smysl reportit zmeny
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

int CFTPOperation::GetChangedWorker(BOOL* reportProgressChange)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetChangedWorker()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = ReportChangeInWorkerID == -2 ? -1 : ReportChangeInWorkerID; // -2 by nemelo nastat (ale pro sychr: vratime "zmena ve vsech")
    ReportChangeInWorkerID = -2;
    if (reportProgressChange != NULL)
        *reportProgressChange = ReportProgressChange;
    ReportProgressChange = FALSE;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::ReportItemChange(int itemUID)
{
    CALL_STACK_MESSAGE1("CFTPOperation::ReportItemChange()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (OperationDlg != NULL && OperationDlg->HWindow != NULL) // jen pokud je dialog otevreny
    {
        if (ReportChangeInItemUID == -3) // zatim nehlasime zadne zmeny
        {
            if (itemUID != -1)
            {
                ReportChangeInItemUID2 = itemUID;
                ReportChangeInItemUID = -2;
            }
            else
                ReportChangeInItemUID = -1;                                 // hlasime vice zmen
            PostMessage(OperationDlg->HWindow, WM_APP_ITEMCHANGEREP, 0, 0); // dame vedet dialogu, ze si ma prijit pro zmeny
        }
        else
        {
            if (itemUID != -1)
            {
                if (ReportChangeInItemUID == -2) // zatim hlasime jednu zmenu
                {
                    if (ReportChangeInItemUID2 != itemUID)
                        ReportChangeInItemUID = itemUID; // uz hlasime dve zmeny
                }
                else
                {
                    if (ReportChangeInItemUID != itemUID && ReportChangeInItemUID2 != itemUID)
                        ReportChangeInItemUID = -1; // zmeny ve vice nez dvou polozkach
                }
            }
            else
                ReportChangeInItemUID = -1; // hlasime vice zmen
        }
    }
    else
        ReportChangeInItemUID = -3; // dialog neni otevreny, nema smysl reportit zmeny
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::GetChangedItems(int* firstUID, int* secondUID)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetChangedItems()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (ReportChangeInItemUID == -1 || ReportChangeInItemUID == -3)
    { // -3 by nemelo nastat (ale pro sychr: vratime "zmena ve vsech")
        *firstUID = -1;
        *secondUID = -1;
    }
    else
    {
        if (ReportChangeInItemUID == -2)
        {
            *firstUID = ReportChangeInItemUID2;
            *secondUID = -1;
        }
        else
        {
            *firstUID = ReportChangeInItemUID2;
            *secondUID = ReportChangeInItemUID;
        }
    }
    ReportChangeInItemUID = -3;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::ReportOperationStateChange()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFTPOperation::ReportOperationStateChange()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (OperationDlg != NULL && OperationDlg->HWindow != NULL && !OperStateChangedPosted)
    {                                                                     // jen pokud je dialog otevreny a jeste nedoslo k reportu (postmessage)
        PostMessage(OperationDlg->HWindow, WM_APP_OPERSTATECHANGE, 0, 0); // dame vedet dialogu, ze si ma prijit pro zmeny
        OperStateChangedPosted = TRUE;
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

COperationState
CFTPOperation::GetOperationState(BOOL calledFromSetupCloseButton)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CFTPOperation::GetOperationState()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (calledFromSetupCloseButton)
        OperStateChangedPosted = FALSE; // uz zase ma smysl posilat zpravy o zmenach stavu
    COperationState state;
    if (ChildItemsNotDone - ChildItemsSkipped - ChildItemsFailed - ChildItemsUINeeded > 0)
        state = opstInProgress;
    else if (ChildItemsFailed + ChildItemsUINeeded > 0)
        state = opstFinishedWithErrors;
    else
    {
        if (ChildItemsSkipped > 0)
            state = opstFinishedWithSkips; // neni-li Skip na urovni operace, musi byt na urovni operace ForcedToFail -> vysledek je opstFinishedWithErrors
        else
            state = opstSuccessfullyFinished;
    }
    if (calledFromSetupCloseButton)
        LastReportedOperState = state;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return state;
}

void CFTPOperation::DebugGetCounters(int* childItemsNotDone, int* childItemsSkipped,
                                     int* childItemsFailed, int* childItemsUINeeded)
{
    CALL_STACK_MESSAGE1("CFTPOperation::DebugGetCounters()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    *childItemsNotDone = ChildItemsNotDone;
    *childItemsSkipped = ChildItemsSkipped;
    *childItemsFailed = ChildItemsFailed;
    *childItemsUINeeded = ChildItemsUINeeded;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::PostChangeOnPathNotifications(BOOL softRefresh)
{
    CALL_STACK_MESSAGE1("CFTPOperation::PostChangeOnPathNotifications()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (SrcPathCanChange)
    {
        TRACE_I("PostChangeOnPathNotification: soft: " << softRefresh << ", src: " << SourcePath << ": " << SrcPathCanChangeInclSubdirs);
        BOOL isDiskPath = SourcePath[0] == '\\' && SourcePath[1] == '\\' ||
                          SourcePath[0] != 0 && SourcePath[1] == ':';
        SalamanderGeneral->PostChangeOnPathNotification(SourcePath, SrcPathCanChangeInclSubdirs | (isDiskPath ? 0 : (softRefresh ? 0x02 /* soft refresh */ : 0)));
    }
    if (TgtPathCanChange)
    {
        TRACE_I("PostChangeOnPathNotification: soft: " << softRefresh << ", tgt: " << TargetPath << ": " << TgtPathCanChangeInclSubdirs);
        BOOL isDiskPath = TargetPath[0] == '\\' && TargetPath[1] == '\\' ||
                          TargetPath[0] != 0 && TargetPath[1] == ':';
        SalamanderGeneral->PostChangeOnPathNotification(TargetPath, TgtPathCanChangeInclSubdirs | (isDiskPath ? 0 : (softRefresh ? 0x02 /* soft refresh */ : 0)));
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

CFTPOperationType
CFTPOperation::GetOperationType()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetOperationType()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    CFTPOperationType type = Type;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return type;
}

int CFTPOperation::GetCopyProgress(CQuadWord* downloaded, CQuadWord* total, CQuadWord* waiting,
                                   int* unknownSizeCount, int* errorsCount, int* doneOrSkippedCount,
                                   int* totalCount, CFTPQueue* queue)
{
    int progress = -1;
    total->Set(0, 0);
    waiting->Set(0, 0);
    CQuadWord totalWithoutErrors;
    queue->GetCopyProgressInfo(downloaded, unknownSizeCount, &totalWithoutErrors,
                               errorsCount, doneOrSkippedCount, totalCount, this);
    WorkersList.AddCurrentDownloadSize(downloaded);
    if (totalWithoutErrors >= *downloaded)
        *waiting = totalWithoutErrors - *downloaded;
    // else; nastava u serveru, kde neni znama velikost v bytech (download probiha, ale jeste nezname odhad celkove velikosti)

    HANDLES(EnterCriticalSection(&OperCritSect));
    if (TotalSizeInBlocks != CQuadWord(0, 0))
    {
        if (GetApproxByteSize(total, TotalSizeInBlocks))
            *total += TotalSizeInBytes;
    }
    else
        *total += TotalSizeInBytes;
    if (*total != CQuadWord(0, 0))
    {
        if (*total < *downloaded)
            *total = *downloaded;
        progress = (int)((CQuadWord(1000, 0) * *downloaded) / *total).Value;
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return progress;
}

int CFTPOperation::GetCopyUploadProgress(CQuadWord* uploaded, CQuadWord* total, CQuadWord* waiting,
                                         int* unknownSizeCount, int* errorsCount, int* doneOrSkippedCount,
                                         int* totalCount, CFTPQueue* queue)
{
    int progress = -1;
    total->Set(0, 0);
    waiting->Set(0, 0);
    CQuadWord totalWithoutErrors;
    queue->GetCopyUploadProgressInfo(uploaded, unknownSizeCount, &totalWithoutErrors,
                                     errorsCount, doneOrSkippedCount, totalCount, this);
    WorkersList.AddCurrentUploadSize(uploaded);
    if (totalWithoutErrors >= *uploaded)
        *waiting = totalWithoutErrors - *uploaded;
    // else; nastava napriklad u ASCII prenosu, kde zdrojovy soubor je mensi nez cilovy (soubor s LF konci radku uploadeny na windowsovy ftp server)

    HANDLES(EnterCriticalSection(&OperCritSect));
    *total = TotalSizeInBytes;
    if (*total != CQuadWord(0, 0))
    {
        if (*total < *uploaded)
            *total = *uploaded;
        progress = (int)((CQuadWord(1000, 0) * *uploaded) / *total).Value;
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return progress;
}

void CFTPOperation::AddBlkSizeInfo(CQuadWord const& sizeInBytes, CQuadWord const& sizeInBlocks)
{
    CALL_STACK_MESSAGE1("CFTPOperation::AddBlkSizeInfo()");

    // velikost bloku lze zjistit jen u souboru o velikosti nad jeden blok (jinak hrozi urceni mensi
    // velikosti bloku - napr. pri velikosti 1 byte se pise velikost 1 blok u libovolne velikosti bloku)
    if (sizeInBlocks > CQuadWord(1, 0))
    {
        HANDLES(EnterCriticalSection(&OperCritSect));
        BlkSizeTotalInBytes += sizeInBytes;
        BlkSizeTotalInBlocks += sizeInBlocks;
        if (BlkSizeActualValue == -1)
            BlkSizeActualValue = 1024; // nejobvyklejsi hodnota
        // provedeme upravu stavajici velikosti tak, aby slo o nejblizsi vyssi mocninu dvojky (1, 2, 4, 8, 16, 32, ...)
        while (1) // BlkSizeTotalInBlocks nesmi byt nula (jinak jde o nekonecny cyklus), coz neni protoze sizeInBlocks > CQuadWord(1, 0) a BlkSizeTotalInBlocks se inicializuje na nulu
        {
            if (CQuadWord(BlkSizeActualValue / 2, 0) * BlkSizeTotalInBlocks >= BlkSizeTotalInBytes)
                BlkSizeActualValue /= 2;
            else
            {
                if (CQuadWord(BlkSizeActualValue, 0) * BlkSizeTotalInBlocks < BlkSizeTotalInBytes)
                    BlkSizeActualValue *= 2;
                else
                    break;
            }
        }
        HANDLES(LeaveCriticalSection(&OperCritSect));
    }
}

BOOL CFTPOperation::GetApproxByteSize(CQuadWord* sizeInBytes, CQuadWord const& sizeInBlocks)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetApproxByteSize()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = BlkSizeActualValue != -1;
    if (ret)
        *sizeInBytes = sizeInBlocks * CQuadWord(BlkSizeActualValue, 0);
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::IsBlkSizeKnown()
{
    CALL_STACK_MESSAGE1("CFTPOperation::IsBlkSizeKnown()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = BlkSizeActualValue != -1;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

DWORD
CFTPOperation::GetElapsedSeconds()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetElapsedSeconds()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    DWORD ret;
    if (OperationEnd == -1)
        ret = GetTickCount() - OperationStart;
    else
        ret = OperationEnd - OperationStart;
    ret /= 1000;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::GetDataActivityInLastPeriod()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetDataActivityInLastPeriod()");
    return (GetTickCount() - GlobalLastActivityTime.Get()) <= WORKER_STATUSUPDATETIMEOUT;
}

void CFTPOperation::GetTargetPath(char* buf, int bufSize)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetDiskOperDefaults()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    lstrcpyn(buf, TargetPath != NULL ? TargetPath : "", bufSize);
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::GetDiskOperDefaults(CFTPDiskWork* diskWork)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetDiskOperDefaults()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    diskWork->CannotCreateDir = CannotCreateDir;
    diskWork->DirAlreadyExists = DirAlreadyExists;
    diskWork->CannotCreateFile = CannotCreateFile;
    diskWork->FileAlreadyExists = FileAlreadyExists;
    diskWork->RetryOnCreatedFile = RetryOnCreatedFile;
    diskWork->RetryOnResumedFile = RetryOnResumedFile;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

int CFTPOperation::GetCannotCreateDir()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetCannotCreateDir()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = CannotCreateDir;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetDirAlreadyExists()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetDirAlreadyExists()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = DirAlreadyExists;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetCannotCreateFile()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetCannotCreateFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = CannotCreateFile;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetFileAlreadyExists()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetFileAlreadyExists()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = FileAlreadyExists;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetRetryOnCreatedFile()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetRetryOnCreatedFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = RetryOnCreatedFile;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetRetryOnResumedFile()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetRetryOnResumedFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = RetryOnResumedFile;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetUnknownAttrs()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUnknownAttrs()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = UnknownAttrs;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::GetResumeIsNotSupported()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetResumeIsNotSupported()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = ResumeIsNotSupported;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::GetDataConWasOpenedForAppendCmd()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetDataConWasOpenedForAppendCmd()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = DataConWasOpenedForAppendCmd;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetAsciiTrModeButBinFile()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetAsciiTrModeButBinFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = AsciiTrModeButBinFile;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetUploadCannotCreateDir()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUploadCannotCreateDir()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = UploadCannotCreateDir;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetUploadDirAlreadyExists()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUploadDirAlreadyExists()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = UploadDirAlreadyExists;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetUploadCannotCreateFile()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUploadCannotCreateFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = UploadCannotCreateFile;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetUploadFileAlreadyExists()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUploadFileAlreadyExists()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = UploadFileAlreadyExists;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetUploadRetryOnCreatedFile()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUploadRetryOnCreatedFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = UploadRetryOnCreatedFile;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetUploadRetryOnResumedFile()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUploadRetryOnResumedFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = UploadRetryOnResumedFile;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetUploadAsciiTrModeButBinFile()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUploadAsciiTrModeButBinFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret = UploadAsciiTrModeButBinFile;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::SetCertificate(CCertificate* certificate)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetCertificate()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    CCertificate* old = pCertificate; // duvodem je zajisteni volani AddRef pres Release (pro pripad, ze je pCertificate == certificate)
    pCertificate = certificate;
    if (pCertificate)
        pCertificate->AddRef();
    if (old)
        old->Release();
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

CCertificate*
CFTPOperation::GetCertificate()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetCertificate()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    CCertificate* ret = pCertificate;
    if (ret != NULL)
        ret->AddRef();
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::SetCannotCreateDir(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetCannotCreateDir()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    CannotCreateDir = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetDirAlreadyExists(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetDirAlreadyExists()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    DirAlreadyExists = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetCannotCreateFile(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetCannotCreateFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    CannotCreateFile = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetFileAlreadyExists(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetFileAlreadyExists()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    FileAlreadyExists = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetRetryOnCreatedFile(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetRetryOnCreatedFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    RetryOnCreatedFile = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetRetryOnResumedFile(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetRetryOnResumedFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    RetryOnResumedFile = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetUnknownAttrs(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetUnknownAttrs()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    UnknownAttrs = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetResumeIsNotSupported(BOOL value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetResumeIsNotSupported()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    ResumeIsNotSupported = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetDataConWasOpenedForAppendCmd(BOOL value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetDataConWasOpenedForAppendCmd()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    DataConWasOpenedForAppendCmd = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetAsciiTrModeButBinFile(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetAsciiTrModeButBinFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    AsciiTrModeButBinFile = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetUploadCannotCreateDir(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetUploadCannotCreateDir()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    UploadCannotCreateDir = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetUploadDirAlreadyExists(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetUploadDirAlreadyExists()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    UploadDirAlreadyExists = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetUploadCannotCreateFile(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetUploadCannotCreateFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    UploadCannotCreateFile = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetUploadFileAlreadyExists(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetUploadFileAlreadyExists()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    UploadFileAlreadyExists = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetUploadRetryOnCreatedFile(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetUploadRetryOnCreatedFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    UploadRetryOnCreatedFile = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetUploadRetryOnResumedFile(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetUploadRetryOnResumedFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    UploadRetryOnResumedFile = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetUploadAsciiTrModeButBinFile(int value)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetUploadAsciiTrModeButBinFile()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    UploadAsciiTrModeButBinFile = value;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::PostNewWorkAvailable(BOOL onlyOneItem)
{
    CALL_STACK_MESSAGE1("CFTPOperation::PostNewWorkAvailable(,)");
    WorkersList.PostNewWorkAvailable(onlyOneItem); // synchronizace je uvnitr WorkersList (sekce OperCritSect zde neni potreba)
}

BOOL CFTPOperation::GiveWorkToSleepingConWorker(CFTPWorker* sourceWorker)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GiveWorkToSleepingConWorker()");
    return WorkersList.GiveWorkToSleepingConWorker(sourceWorker); // synchronizace je uvnitr WorkersList (sekce OperCritSect zde neni potreba)
}

CFTPServerPathType
CFTPOperation::GetFTPServerPathType(const char* path)
{
    CALL_STACK_MESSAGE2("CFTPOperation::GetFTPServerPathType(%s)", path);

    HANDLES(EnterCriticalSection(&OperCritSect));
    CFTPServerPathType type = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, path);
    HANDLES(LeaveCriticalSection(&OperCritSect));

    return type;
}

BOOL CFTPOperation::IsServerSystem(const char* systemName)
{
    CALL_STACK_MESSAGE1("CFTPOperation::IsServerSystem()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    char sysName[201];
    FTPGetServerSystem(ServerSystem, sysName);
    HANDLES(LeaveCriticalSection(&OperCritSect));

    return _stricmp(sysName, systemName) == 0;
}

BOOL CFTPOperation::IsAlreadyExploredPath(const char* path)
{
    CALL_STACK_MESSAGE2("CFTPOperation::IsAlreadyExploredPath(%s)", path);
    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = ExploredPaths.ContainsPath(path);
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::AddToExploredPaths(const char* path)
{
    CALL_STACK_MESSAGE2("CFTPOperation::AddToExploredPaths(%s)", path);
    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = ExploredPaths.AddPath(path);
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::GetUsePassiveMode()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUsePassiveMode()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = UsePassiveMode;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::SetUsePassiveMode(BOOL usePassiveMode)
{
    CALL_STACK_MESSAGE2("CFTPOperation::SetUsePassiveMode(%d)", usePassiveMode);
    HANDLES(EnterCriticalSection(&OperCritSect));
    UsePassiveMode = usePassiveMode;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

BOOL CFTPOperation::GetSizeCmdIsSupported()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetSizeCmdIsSupported()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = SizeCmdIsSupported;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::SetSizeCmdIsSupported(BOOL sizeCmdIsSupported)
{
    CALL_STACK_MESSAGE2("CFTPOperation::SetSizeCmdIsSupported(%d)", sizeCmdIsSupported);
    HANDLES(EnterCriticalSection(&OperCritSect));
    SizeCmdIsSupported = sizeCmdIsSupported;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::GetListCommand(char* buf, int bufSize)
{
    CALL_STACK_MESSAGE2("CFTPOperation::GetListCommand(, %d)", bufSize);
    HANDLES(EnterCriticalSection(&OperCritSect));
    lstrcpyn(buf, (ListCommand != NULL && *ListCommand != 0 ? ListCommand : LIST_CMD_TEXT), bufSize);
    if (bufSize > 2 && bufSize > (int)strlen(buf) + 2)
        strcat(buf, "\r\n");
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

BOOL CFTPOperation::GetUseListingsCache()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUseListingsCache()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = UseListingsCache;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::GetUser(char* buf, int bufSize)
{
    CALL_STACK_MESSAGE2("CFTPOperation::GetUser(, %d)", bufSize);
    HANDLES(EnterCriticalSection(&OperCritSect));
    lstrcpyn(buf, User != NULL ? User : FTP_ANONYMOUS, bufSize);
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

char* CFTPOperation::AllocServerSystemReply()
{
    CALL_STACK_MESSAGE1("CFTPOperation::AllocServerSystemReply()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    char* ret = SalamanderGeneral->DupStr(HandleNULLStr(ServerSystem));
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

char* CFTPOperation::AllocServerFirstReply()
{
    CALL_STACK_MESSAGE1("CFTPOperation::AllocServerFirstReply()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    char* ret = SalamanderGeneral->DupStr(HandleNULLStr(ServerFirstReply));
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::GetListingServerType(char* buf)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetListingServerType()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    BOOL ret = ListingServerType != NULL;
    if (ret)
        lstrcpyn(buf, ListingServerType, SERVERTYPE_MAX_SIZE);
    else
        buf[0] = 0;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

int CFTPOperation::GetTransferMode()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetTransferMode()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    int ret;
    if (AutodetectTrMode)
        ret = trmAutodetect;
    else
    {
        if (UseAsciiTransferMode)
            ret = trmASCII;
        else
            ret = trmBinary;
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::GetParamsForChAttrsOper(BOOL* selFiles, BOOL* selDirs, BOOL* includeSubdirs,
                                            DWORD* attrAndMask, DWORD* attrOrMask,
                                            int* operationsUnknownAttrs)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetParamsForChAttrsOper()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    *selFiles = ChAttrOfFiles;
    *selDirs = ChAttrOfDirs;
    *includeSubdirs = SrcPathCanChangeInclSubdirs;
    *attrAndMask = AttrAnd;
    *attrOrMask = AttrOr;
    *operationsUnknownAttrs = UnknownAttrs;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::AddToItemOrOperationCounters(int itemDir, int childItemsNotDone,
                                                 int childItemsSkipped, int childItemsFailed,
                                                 int childItemsUINeeded, BOOL onlyUINeededOrFailedToSkipped)
{
    SLOW_CALL_STACK_MESSAGE1("CFTPOperation::AddToItemOrOperationCounters()");
    if (childItemsNotDone != 0 || childItemsSkipped != 0 || childItemsFailed != 0 || childItemsUINeeded != 0)
    {
        if (itemDir == -1)
        {
            AddToNotDoneSkippedFailed(childItemsNotDone, childItemsSkipped, childItemsFailed,
                                      childItemsUINeeded, onlyUINeededOrFailedToSkipped);
        }
        else
        {
            HANDLES(EnterCriticalSection(&OperCritSect));
            CFTPQueue* queue = Queue;
            HANDLES(LeaveCriticalSection(&OperCritSect));
            if (queue != NULL) // "always true"
            {
                queue->AddToNotDoneSkippedFailed(itemDir, childItemsNotDone,
                                                 childItemsSkipped, childItemsFailed, childItemsUINeeded, this);
            }
            else
                TRACE_E("Unexpected situation in CFTPOperation::AddToItemOrOperationCounters(): Queue is NULL!");
        }
    }
}

void CFTPOperation::GetParamsForDeleteOper(int* confirmDelOnNonEmptyDir, int* confirmDelOnHiddenFile,
                                           int* confirmDelOnHiddenDir)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetParamsForDeleteOper()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    if (confirmDelOnNonEmptyDir != NULL)
        *confirmDelOnNonEmptyDir = ConfirmDelOnNonEmptyDir;
    if (confirmDelOnHiddenFile != NULL)
        *confirmDelOnHiddenFile = ConfirmDelOnHiddenFile;
    if (confirmDelOnHiddenDir != NULL)
        *confirmDelOnHiddenDir = ConfirmDelOnHiddenDir;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

void CFTPOperation::SetParamsForDeleteOper(int* confirmDelOnNonEmptyDir, int* confirmDelOnHiddenFile,
                                           int* confirmDelOnHiddenDir)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SetParamsForDeleteOper()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    if (confirmDelOnNonEmptyDir != NULL)
        ConfirmDelOnNonEmptyDir = *confirmDelOnNonEmptyDir;
    if (confirmDelOnHiddenFile != NULL)
        ConfirmDelOnHiddenFile = *confirmDelOnHiddenFile;
    if (confirmDelOnHiddenDir != NULL)
        ConfirmDelOnHiddenDir = *confirmDelOnHiddenDir;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

DWORD
CFTPOperation::GiveLastErrorOccurenceTime()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GiveLastErrorOccurenceTime()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    DWORD ret = ++LastErrorOccurenceTime;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::SearchWorkerWithNewError(int* index)
{
    CALL_STACK_MESSAGE1("CFTPOperation::SearchWorkerWithNewError()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    DWORD lastErrorOccurenceTime = LastErrorOccurenceTime;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return WorkersList.SearchWorkerWithNewError(index, lastErrorOccurenceTime);
}

BOOL CFTPOperation::CanMakeChangesOnPath(const char* user, const char* host, unsigned short port,
                                         const char* path, CFTPServerPathType pathType,
                                         int userLength)
{
    CALL_STACK_MESSAGE1("CFTPOperation::CanMakeChangesOnPath()");
    char buf[FTP_USERPART_SIZE];
    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&OperCritSect));
    if (SrcPathCanChange)
    {
        BOOL isFTP = SalamanderGeneral->StrNICmp(SourcePath, AssignedFSName, AssignedFSNameLen) == 0 &&          // jde o nase fs-name (FTP)
                     SourcePath[AssignedFSNameLen] == ':';                                                       // nase fs-name neni jen prefix
        BOOL isFTPS = SalamanderGeneral->StrNICmp(SourcePath, AssignedFSNameFTPS, AssignedFSNameLenFTPS) == 0 && // jde o nase fs-name (FTPS)
                      SourcePath[AssignedFSNameLenFTPS] == ':';                                                  // nase fs-name neni jen prefix
        if (isFTP || isFTPS)
        {
            lstrcpyn(buf, SourcePath + (isFTP ? AssignedFSNameLen : AssignedFSNameLenFTPS) + 1, FTP_USERPART_SIZE);
            char *user2, *host2, *portStr2, *pathStr2;
            FTPSplitPath(buf, &user2, NULL, &host2, &portStr2, &pathStr2, NULL, userLength);
            const char* pathPart2 = NULL;
            if (pathStr2 != NULL && pathStr2 > buf)
                pathPart2 = SourcePath + (isFTP ? AssignedFSNameLen : AssignedFSNameLenFTPS) + 1 + (pathStr2 - buf) - 1;
            int port2 = portStr2 != NULL ? atoi(portStr2) : IPPORT_FTP;
            if (user2 != NULL && strcmp(user2, FTP_ANONYMOUS) == 0)
                user2 = NULL;
            if (host2 != NULL && host != NULL && pathPart2 != NULL &&
                SalamanderGeneral->StrICmp(host2, host) == 0 &&
                (user2 == NULL && user == NULL ||
                 user != NULL && user2 != NULL && strcmp(user2, user) == 0) &&
                port2 == port &&
                FTPIsPrefixOfServerPath(pathType, FTPGetLocalPath(pathPart2, pathType),
                                        path, !SrcPathCanChangeInclSubdirs))
            {
                ret = TRUE;
            }
        }
    }
    if (!ret && TgtPathCanChange)
    {
        BOOL isFTP = SalamanderGeneral->StrNICmp(TargetPath, AssignedFSName, AssignedFSNameLen) == 0 &&          // jde o nase fs-name (FTP)
                     TargetPath[AssignedFSNameLen] == ':';                                                       // nase fs-name neni jen prefix
        BOOL isFTPS = SalamanderGeneral->StrNICmp(TargetPath, AssignedFSNameFTPS, AssignedFSNameLenFTPS) == 0 && // jde o nase fs-name (FTPS)
                      TargetPath[AssignedFSNameLenFTPS] == ':';                                                  // nase fs-name neni jen prefix
        if (isFTP || isFTPS)
        {
            lstrcpyn(buf, TargetPath + (isFTP ? AssignedFSNameLen : AssignedFSNameLenFTPS) + 1, FTP_USERPART_SIZE);
            char *user2, *host2, *portStr2, *pathStr2;
            FTPSplitPath(buf, &user2, NULL, &host2, &portStr2, &pathStr2, NULL, userLength);
            const char* pathPart2 = NULL;
            if (pathStr2 != NULL && pathStr2 > buf)
                pathPart2 = TargetPath + (isFTP ? AssignedFSNameLen : AssignedFSNameLenFTPS) + 1 + (pathStr2 - buf) - 1;
            int port2 = portStr2 != NULL ? atoi(portStr2) : IPPORT_FTP;
            if (user2 != NULL && strcmp(user2, FTP_ANONYMOUS) == 0)
                user2 = NULL;
            if (host2 != NULL && host != NULL && pathPart2 != NULL &&
                SalamanderGeneral->StrICmp(host2, host) == 0 &&
                (user2 == NULL && user == NULL ||
                 user != NULL && user2 != NULL && strcmp(user2, user) == 0) &&
                port2 == port &&
                FTPIsPrefixOfServerPath(pathType, FTPGetLocalPath(pathPart2, pathType),
                                        path, !TgtPathCanChangeInclSubdirs))
            {
                ret = TRUE;
            }
        }
    }
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

BOOL CFTPOperation::IsUploadingToServer(const char* user, const char* host, unsigned short port,
                                        int userLength)
{
    CALL_STACK_MESSAGE1("CFTPOperation::IsUploadingToServer()");
    char buf[FTP_USERPART_SIZE];
    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&OperCritSect));

    if (Type == fotCopyUpload || Type == fotMoveUpload) // jde o Upload
    {
        BOOL isFTP = SalamanderGeneral->StrNICmp(TargetPath, AssignedFSName, AssignedFSNameLen) == 0 &&          // jde o nase fs-name (FTP)
                     TargetPath[AssignedFSNameLen] == ':';                                                       // nase fs-name neni jen prefix
        BOOL isFTPS = SalamanderGeneral->StrNICmp(TargetPath, AssignedFSNameFTPS, AssignedFSNameLenFTPS) == 0 && // jde o nase fs-name (FTPS)
                      TargetPath[AssignedFSNameLenFTPS] == ':';                                                  // nase fs-name neni jen prefix
        if (isFTP || isFTPS)
        {
            lstrcpyn(buf, TargetPath + (isFTP ? AssignedFSNameLen : AssignedFSNameLenFTPS) + 1, FTP_USERPART_SIZE);
            char *user2, *host2, *portStr2;
            FTPSplitPath(buf, &user2, NULL, &host2, &portStr2, NULL, NULL, userLength);
            int port2 = portStr2 != NULL ? atoi(portStr2) : IPPORT_FTP;
            if (user2 != NULL && strcmp(user2, FTP_ANONYMOUS) == 0)
                user2 = NULL;
            if (host2 != NULL && host != NULL &&
                SalamanderGeneral->StrICmp(host2, host) == 0 &&
                (user2 == NULL && user == NULL ||
                 user != NULL && user2 != NULL && strcmp(user2, user) == 0) &&
                port2 == port)
            {
                ret = TRUE;
            }
        }
    }

    HANDLES(LeaveCriticalSection(&OperCritSect));
    return ret;
}

void CFTPOperation::GetUserHostPort(char* user, char* host, unsigned short* port)
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUserHostPort()");
    HANDLES(EnterCriticalSection(&OperCritSect));
    if (host != NULL)
    {
        if (Host != NULL)
            lstrcpyn(host, Host, HOST_MAX_SIZE);
        else
            host[0] = 0;
    }
    if (user != NULL)
    {
        if (User != NULL)
            lstrcpyn(user, User, USER_MAX_SIZE);
        else
            user[0] = 0;
    }
    if (port != NULL)
        *port = Port;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}

//
// ****************************************************************************
// CFTPQueueItem
//

CFTPQueueItem::CFTPQueueItem()
{
    HANDLES(EnterCriticalSection(&NextItemUIDCritSect));
    UID = NextItemUID++;
    HANDLES(LeaveCriticalSection(&NextItemUIDCritSect));

    ParentUID = -1;
    Type = fqitNone;
    ProblemID = ITEMPR_OK;
    WinError = NO_ERROR;
    ErrAllocDescr = NULL;

    ErrorOccurenceTime = -1;

    ForceAction = fqiaNone;

    Path = NULL;
    Name = NULL;
}

CFTPQueueItem::~CFTPQueueItem()
{
    if (Path != NULL)
        SalamanderGeneral->Free(Path);
    if (Name != NULL)
        SalamanderGeneral->Free(Name);
    if (ErrAllocDescr != NULL)
        SalamanderGeneral->Free(ErrAllocDescr);
}

void CFTPQueueItem::SetItem(int parentUID, CFTPQueueItemType type, CFTPQueueItemState state,
                            DWORD problemID, const char* path, const char* name)
{
    ParentUID = parentUID;
    Type = type;
    SetStateInternal(state);
    ProblemID = problemID;
    Path = SalamanderGeneral->DupStr(path);
    Name = SalamanderGeneral->DupStr(name);
}

BOOL CFTPQueueItem::HasErrorToSolve(BOOL* canSkip, BOOL* canRetry)
{
    BOOL solvableErr = ProblemID != ITEMPR_INVALIDPATHTODIR && // nejde o neresitelny problem (zadne Retry nemuze pomoct)
                       ProblemID != ITEMPR_DIREXPLENDLESSLOOP &&
                       ProblemID != ITEMPR_INVALIDPATHTOLINK;
    if (canSkip != NULL)
    {
        *canSkip = solvableErr &&
                   (GetItemState() == sqisWaiting || GetItemState() == sqisFailed ||
                    GetItemState() == sqisUserInputNeeded);
    }
    if (GetItemState() >= sqisSkipped /* sqisSkipped, sqisFailed, sqisForcedToFail nebo sqisUserInputNeeded */ &&
        GetItemState() != sqisForcedToFail /* nelze resit primo, musi se resit pres child polozky */ &&
        solvableErr)
    {
        if (canRetry != NULL)
            *canRetry = TRUE;
        return ProblemID != ITEMPR_SKIPPEDBYUSER; // "Solve Error" pro "skipped by user" nema smysl
    }
    else
    {
        if (canRetry != NULL)
            *canRetry = FALSE;
        return FALSE;
    }
}

void CFTPQueueItem::GetProblemDescr(char* buf, int bufSize)
{
    char errBuf[300];
    BOOL addErrAllocDescr = FALSE; // TRUE = pokud mame nejakou odpoved serveru v ErrAllocDescr, pridame jeji prvni radek do hlasky
    if (bufSize > 0)
    {
        switch (ProblemID)
        {
        case ITEMPR_OK:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_OK));
            break;
        case ITEMPR_LOWMEM:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_LOWMEM));
            break;

        case ITEMPR_CANNOTCREATETGTFILE:
        case ITEMPR_CANNOTCREATETGTDIR:
        case ITEMPR_TGTFILEREADERROR:
        case ITEMPR_SRCFILEREADERROR:
        case ITEMPR_TGTFILEWRITEERROR:
        case ITEMPR_UPLOADCANNOTLISTSRCPATH:
        case ITEMPR_UNABLETODELETEDISKDIR:
        case ITEMPR_UNABLETODELETEDISKFILE:
        case ITEMPR_UPLOADCANNOTOPENSRCFILE:
        {
            if (WinError != NO_ERROR)
                FTPGetErrorText(WinError, errBuf, 300);
            else
                lstrcpyn(errBuf, LoadStr(IDS_UNKNOWNERROR), 300);
            char* s = errBuf + strlen(errBuf); // orizneme EOL a '.'
            while (--s >= errBuf && (*s == '\r' || *s == '\n' || *s == '.'))
                ;
            *(s + 1) = 0;
            int resID = 0;
            switch (ProblemID)
            {
            case ITEMPR_CANNOTCREATETGTFILE:
                resID = IDS_OPERDOPPR_CANTCRTGTFILE;
                break;
            case ITEMPR_CANNOTCREATETGTDIR:
                resID = IDS_OPERDOPPR_CANTCRTGTDIR;
                break;
            case ITEMPR_TGTFILEREADERROR:
                resID = IDS_OPERDOPPR_TGTFILEREADERROR;
                break;
            case ITEMPR_SRCFILEREADERROR:
                resID = IDS_OPERDOPPR_SRCFILEREADERROR;
                break;
            case ITEMPR_TGTFILEWRITEERROR:
                resID = IDS_OPERDOPPR_TGTFILEWRITEERROR;
                break;
            case ITEMPR_UPLOADCANNOTLISTSRCPATH:
                resID = IDS_OPERDOPPR_UPLCANTLISTSRCPATH;
                break;
            case ITEMPR_UNABLETODELETEDISKDIR:
                resID = IDS_OPERDOPPR_UPLCANTDELSRCDISKDIR;
                break;
            case ITEMPR_UPLOADCANNOTOPENSRCFILE:
                resID = IDS_OPERDOPPR_UPLCANNOTOPENSRCFILE;
                break;
            case ITEMPR_UNABLETODELETEDISKFILE:
                resID = IDS_OPERDOPPR_UPLCANTDELSRCDISKFILE;
                break;
            }
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(resID), errBuf);
            break;
        }

        case ITEMPR_TGTFILEALREADYEXISTS:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_TGTFILEEXISTS));
            break;
        case ITEMPR_TGTDIRALREADYEXISTS:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_TGTDIREXISTS));
            break;

        case ITEMPR_RETRYONCREATFILE:
        case ITEMPR_RETRYONRESUMFILE:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_TRANSFERFAILED));
            break;

        case ITEMPR_ASCIITRFORBINFILE:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_ASCIITRBINFILE));
            break;

        case ITEMPR_UNKNOWNATTRS:
        {
            const char* attrs;
            switch (Type)
            {
            case fqitChAttrsFile:
                attrs = ((CFTPQueueItemChAttr*)this)->OrigRights;
                break;
            case fqitChAttrsDir:
                attrs = ((CFTPQueueItemChAttrDir*)this)->OrigRights;
                break;
            case fqitChAttrsExploreDir:
                attrs = ((CFTPQueueItemChAttrExplore*)this)->OrigRights;
                break;

            default:
            {
                TRACE_E("Unexpected situation in CFTPQueueItem::GetProblemDescr(): ITEMPR_UNKNOWNATTRS with unknown original attributes!");
                attrs = "???";
                break;
            }
            }
            if (attrs == NULL)
                attrs = LoadStr(IDS_OPERDOPPR_UNKEXISTATTR);
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UNKNOWNATTRS), attrs); // attrs muze byt i asi NULL (pri chybe), sprintf se z toho vzpamatuje
            break;
        }

        case ITEMPR_INVALIDPATHTODIR:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_INVALIDPATHTODIR));
            break;

        case ITEMPR_INVALIDPATHTOLINK:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_INVALIDPATHTOLINK));
            break;

        case ITEMPR_UNABLETOCWD:
        case ITEMPR_UNABLETOCWDONLYPATH:
        {
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UNABLETOCWD));
            addErrAllocDescr = TRUE;
            break;
        }

        case ITEMPR_UNABLETOPWD:
        {
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UNABLETOPWD));
            addErrAllocDescr = TRUE;
            break;
        }

        case ITEMPR_DIREXPLENDLESSLOOP:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_DIREXPLENDLESSLOOP));
            break;

        case ITEMPR_LISTENFAILURE:
        {
            if (WinError != NO_ERROR)
                FTPGetErrorText(WinError, errBuf, 300);
            else
                lstrcpyn(errBuf, LoadStr(IDS_UNKNOWNERROR), 300);
            char* s = errBuf + strlen(errBuf); // orizneme EOL a '.'
            while (--s >= errBuf && (*s == '\r' || *s == '\n' || *s == '.'))
                ;
            *(s + 1) = 0;
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_LISTENFAILURE), errBuf);
            break;
        }

        case ITEMPR_UPLOADCANNOTLISTTGTPATH:
        case ITEMPR_INCOMPLETELISTING:
        case ITEMPR_INCOMPLETEDOWNLOAD:
        case ITEMPR_INCOMPLETEUPLOAD:
        {
            if (ErrAllocDescr != NULL)
            {
                _snprintf_s(buf, bufSize, _TRUNCATE,
                            LoadStr(ProblemID == ITEMPR_INCOMPLETELISTING ? IDS_OPERDOPPR_INCOMPLLISTING2 : ProblemID == ITEMPR_INCOMPLETEDOWNLOAD ? IDS_OPERDOPPR_INCOMPLDOWNLOAD2
                                                                                                        : ProblemID == ITEMPR_INCOMPLETEUPLOAD     ? IDS_OPERDOPPR_INCOMPLUPLOAD2
                                                                                                                                                   : IDS_OPERDOPPR_UPLCANTLISTTGTPATH2));
                addErrAllocDescr = TRUE;
            }
            else
            {
                if (WinError != NO_ERROR)
                    FTPGetErrorText(WinError, errBuf, 300);
                else
                {
                    if (ProblemID == ITEMPR_UPLOADCANNOTLISTTGTPATH)
                        errBuf[0] = 0;
                    else
                        lstrcpyn(errBuf, LoadStr(IDS_UNKNOWNERROR), 300);
                }
                char* s = errBuf + strlen(errBuf); // orizneme EOL a '.'
                while (--s >= errBuf && (*s == '\r' || *s == '\n' || *s == '.'))
                    ;
                *(s + 1) = 0;
                _snprintf_s(buf, bufSize, _TRUNCATE,
                            LoadStr(ProblemID == ITEMPR_INCOMPLETELISTING ? IDS_OPERDOPPR_INCOMPLLISTING1 : ProblemID == ITEMPR_INCOMPLETEDOWNLOAD ? IDS_OPERDOPPR_INCOMPLDOWNLOAD1
                                                                                                        : ProblemID == ITEMPR_INCOMPLETEUPLOAD     ? IDS_OPERDOPPR_INCOMPLUPLOAD1
                                                                                                        : errBuf[0] == 0                           ? IDS_OPERDOPPR_UPLCANTLISTTGTPATH
                                                                                                                                                   : IDS_OPERDOPPR_UPLCANTLISTTGTPATH3),
                            errBuf);
            }
            break;
        }

        case ITEMPR_UNABLETOPARSELISTING:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UNABLETOPARSELISTING));
            break;

        case ITEMPR_DIRISHIDDEN:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_DIRISHIDDEN));
            break;
        case ITEMPR_DIRISNOTEMPTY:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_DIRISNOTEMPTY));
            break;
        case ITEMPR_FILEISHIDDEN:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_FILEISHIDDEN));
            break;

        case ITEMPR_UNABLETORESOLVELNK:
        {
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UNABLETORESOLVELNK));
            addErrAllocDescr = TRUE;
            break;
        }

        case ITEMPR_UNABLETODELETEFILE:
        case ITEMPR_UNABLETODELSRCFILE:
        {
            _snprintf_s(buf, bufSize, _TRUNCATE,
                        LoadStr(ProblemID == ITEMPR_UNABLETODELETEFILE ? IDS_OPERDOPPR_UNABLETODELFILE : IDS_OPERDOPPR_UNABLETODELSRCFILE));
            addErrAllocDescr = TRUE;
            break;
        }

        case ITEMPR_UNABLETODELETEDIR:
        {
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UNABLETODELDIR));
            addErrAllocDescr = TRUE;
            break;
        }

        case ITEMPR_UNABLETOCHATTRS:
        {
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UNABLETOCHATTRS));
            addErrAllocDescr = TRUE;
            break;
        }

        case ITEMPR_UNABLETORESUME:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UNABLETORESUME));
            break;
        case ITEMPR_RESUMETESTFAILED:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_RESUMETESTFAILED));
            break;

        case ITEMPR_UPLOADCANNOTCREATETGTDIR:
        {
            if (ErrAllocDescr == NULL)
            {
                _snprintf_s(buf, bufSize, _TRUNCATE,
                            LoadStr(WinError == ERROR_ALREADY_EXISTS ? IDS_OPERDOPPR_UPLCANTCRTGTDIRFILEEX : IDS_OPERDOPPR_UPLCANTCRTGTDIRINV));
            }
            else
            {
                _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPLCANTCRTGTDIR));
                addErrAllocDescr = TRUE;
            }
            break;
        }

        case ITEMPR_UPLOADTGTDIRALREADYEXISTS:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPLTGTDIREXISTS));
            break;

        case ITEMPR_UPLOADCRDIRAUTORENFAILED:
        {
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPLCRDIRAUTORENFAILED));
            addErrAllocDescr = TRUE;
            break;
        }

        case ITEMPR_UPLOADFILEAUTORENFAILED:
        {
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPLFILEAUTORENFAILED));
            addErrAllocDescr = TRUE;
            break;
        }

        case ITEMPR_UPLOADCANNOTCREATETGTFILE:
        {
            if (ErrAllocDescr == NULL)
            {
                _snprintf_s(buf, bufSize, _TRUNCATE,
                            LoadStr(WinError == ERROR_ALREADY_EXISTS ? IDS_OPERDOPPR_UPLCANTCRTGTFILEDIREX : IDS_OPERDOPPR_UPLCANTCRTGTFILEINV));
            }
            else
            {
                _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPLCANTCRTGTFILE));
                addErrAllocDescr = TRUE;
            }
            break;
        }

        case ITEMPR_UPLOADTGTFILEALREADYEXISTS:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPLTGTFILEEXISTS));
            break;
        case ITEMPR_SRCFILEINUSE:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_SRCFILEINUSE));
            break;
        case ITEMPR_TGTFILEINUSE:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_TGTFILEINUSE));
            break;
        case ITEMPR_UPLOADASCIIRESUMENOTSUP:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPLASCIIRESNOTSUP));
            break;
        case ITEMPR_UPLOADUNABLETORESUMEUNKSIZ:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPUNABLERESUNKSIZ));
            break;
        case ITEMPR_UPLOADUNABLETORESUMEBIGTGT:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPUNABLERESBIGTGT));
            break;
        case ITEMPR_SKIPPEDBYUSER:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_SKIPPEDBYUSER));
            break;
        case ITEMPR_UPLOADTESTIFFINISHEDNOTSUP:
            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPPR_UPLTESTIFFINNOTSUP));
            break;

        default:
        {
            TRACE_E("Unexpected situation in CFTPQueueItem::GetProblemDescr(): unknown ProblemID!");
            buf[0] = 0;
        }
        }
    }
    if (addErrAllocDescr && ErrAllocDescr != NULL && bufSize > 0)
    { // pokud mame nejakou odpoved serveru, pridame jeji prvni radek do hlasky
        char* end = buf + bufSize - 1;
        char* s = buf + strlen(buf);
        char* src = ErrAllocDescr;
        if (s < end)
            *s++ = ' ';
        while (s < end && *src >= ' ')
            *s++ = *src++;
        *s = 0;
    }
}

//
// ****************************************************************************
// CFTPQueueItemAncestor
//

void CFTPQueueItemAncestor::ChangeStateAndCounters(CFTPQueueItemState state, CFTPOperation* oper,
                                                   CFTPQueue* queue)
{
    if (State == state)
        return; // neni co delat
    int childItemsNotDone = 1;
    int childItemsFailed = 0;
    int childItemsSkipped = 0;
    int childItemsUINeeded = 0;
    switch (State)
    {
    case sqisDone:
        childItemsNotDone = 0;
        break;
    case sqisSkipped:
        childItemsSkipped = 1;
        break;
    case sqisUserInputNeeded:
        childItemsUINeeded = 1;
        break;

    case sqisFailed:
    case sqisForcedToFail:
        childItemsFailed = 1;
        break;
    }
    BOOL onlyUINeededOrFailedToSkipped = FALSE;
    switch (state)
    {
    case sqisDone:
    {
        childItemsNotDone = 0 - childItemsNotDone;
        childItemsFailed = 0 - childItemsFailed;
        childItemsSkipped = 0 - childItemsSkipped;
        childItemsUINeeded = 0 - childItemsUINeeded;
        break;
    }

    case sqisSkipped:
    {
        onlyUINeededOrFailedToSkipped = State == sqisUserInputNeeded || State == sqisFailed;
        childItemsNotDone = 1 - childItemsNotDone;
        childItemsFailed = 0 - childItemsFailed;
        childItemsSkipped = 1 - childItemsSkipped;
        childItemsUINeeded = 0 - childItemsUINeeded;
        break;
    }

    case sqisUserInputNeeded:
    {
        childItemsNotDone = 1 - childItemsNotDone;
        childItemsFailed = 0 - childItemsFailed;
        childItemsSkipped = 0 - childItemsSkipped;
        childItemsUINeeded = 1 - childItemsUINeeded;
        break;
    }

    case sqisFailed:
    case sqisForcedToFail:
    {
        childItemsNotDone = 1 - childItemsNotDone;
        childItemsFailed = 1 - childItemsFailed;
        childItemsSkipped = 0 - childItemsSkipped;
        childItemsUINeeded = 0 - childItemsUINeeded;
        break;
    }

    default: // sqisWaiting, sqisProcessing, sqisDelayed
    {
        childItemsNotDone = 1 - childItemsNotDone;
        childItemsFailed = 0 - childItemsFailed;
        childItemsSkipped = 0 - childItemsSkipped;
        childItemsUINeeded = 0 - childItemsUINeeded;
        break;
    }
    }
    queue->UpdateCounters((CFTPQueueItem*)this, FALSE); // zmenu stavu resime tak, ze polozku jakoby odebereme a po zmene stavu zase pridame
    State = state;
    queue->UpdateCounters((CFTPQueueItem*)this, TRUE);
    if (((CFTPQueueItem*)this)->IsItemInSimpleErrorState())
        ((CFTPQueueItem*)this)->ErrorOccurenceTime = queue->GiveLastErrorOccurenceTime();
    if (childItemsNotDone != 0 || childItemsFailed != 0 || childItemsSkipped != 0 || childItemsUINeeded != 0)
    {
        oper->AddToItemOrOperationCounters(((CFTPQueueItem*)this)->ParentUID, childItemsNotDone,
                                           childItemsSkipped, childItemsFailed, childItemsUINeeded,
                                           onlyUINeededOrFailedToSkipped);
    }
}

//
// ****************************************************************************
// CFTPQueueItemDir
//

CFTPQueueItemDir::CFTPQueueItemDir()
{
    ChildItemsNotDone = 0;
    ChildItemsSkipped = 0;
    ChildItemsFailed = 0;
    ChildItemsUINeeded = 0;
}

BOOL CFTPQueueItemDir::SetItemDir(int childItemsNotDone, int childItemsSkipped, int childItemsFailed,
                                  int childItemsUINeeded)
{
    ChildItemsNotDone = childItemsNotDone;
    ChildItemsSkipped = childItemsSkipped;
    ChildItemsFailed = childItemsFailed;
    ChildItemsUINeeded = childItemsUINeeded;
    return TRUE;
}

void CFTPQueueItemDir::SetStateAndNotDoneSkippedFailed(int childItemsNotDone, int childItemsSkipped,
                                                       int childItemsFailed, int childItemsUINeeded)
{
    ChildItemsNotDone = childItemsNotDone;
    ChildItemsSkipped = childItemsSkipped;
    ChildItemsFailed = childItemsFailed;
    ChildItemsUINeeded = childItemsUINeeded;
    if (GetItemState() == sqisWaiting) // pokud je polozka pripravena ke zpracovani, zkontrolujeme jestli
    {                                  // ji neni treba zpozdit nebo jestli nemusi failnout kvuli child polozkam
        if (ChildItemsNotDone - ChildItemsSkipped - ChildItemsFailed - ChildItemsUINeeded > 0)
            SetStateInternal(sqisDelayed);
        else
        {
            if (ChildItemsSkipped + ChildItemsFailed + ChildItemsUINeeded > 0)
                SetStateInternal(sqisForcedToFail);
        }
    }
}

CFTPQueueItemState
CFTPQueueItemDir::GetStateFromCounters()
{
    if (ChildItemsNotDone - ChildItemsSkipped - ChildItemsFailed - ChildItemsUINeeded > 0)
        return sqisDelayed;
    else
    {
        if (ChildItemsSkipped + ChildItemsFailed + ChildItemsUINeeded > 0)
            return sqisForcedToFail;
        else
            return sqisWaiting;
    }
}

//
// ****************************************************************************
// CFTPQueueItemDel
//

CFTPQueueItemDel::CFTPQueueItemDel()
{
    IsHiddenFile = 0;
}

BOOL CFTPQueueItemDel::SetItemDel(int isHiddenFile)
{
    IsHiddenFile = isHiddenFile;
    return TRUE;
}

//
// ****************************************************************************
// CFTPQueueItemDelExplore
//

CFTPQueueItemDelExplore::CFTPQueueItemDelExplore()
{
    IsTopLevelDir = 0;
    IsHiddenDir = 0;
}

BOOL CFTPQueueItemDelExplore::SetItemDelExplore(int isTopLevelDir, int isHiddenDir)
{
    IsTopLevelDir = isTopLevelDir;
    IsHiddenDir = isHiddenDir;
    return TRUE;
}

//
// ****************************************************************************
// CFTPQueueItemCopyOrMove
//

CFTPQueueItemCopyOrMove::CFTPQueueItemCopyOrMove()
{
    TgtPath = NULL;
    TgtName = NULL;
    Size.SetUI64(0);
    AsciiTransferMode = 0;
    IgnoreAsciiTrModeForBinFile = 0;
    SizeInBytes = 0;
    TgtFileState = 0;
    DateAndTimeValid = 0;
    memset(&Date, 0, sizeof(Date));
    memset(&Time, 0, sizeof(Time));
}

CFTPQueueItemCopyOrMove::~CFTPQueueItemCopyOrMove()
{
    if (TgtPath != NULL)
        SalamanderGeneral->Free(TgtPath);
    if (TgtName != NULL)
        SalamanderGeneral->Free(TgtName);
}

void CFTPQueueItemCopyOrMove::SetItemCopyOrMove(const char* tgtPath, const char* tgtName, const CQuadWord& size,
                                                int asciiTransferMode, int sizeInBytes, int tgtFileState,
                                                BOOL dateAndTimeValid, const CFTPDate& date, const CFTPTime& time)
{
    TgtPath = SalamanderGeneral->DupStr(tgtPath);
    TgtName = SalamanderGeneral->DupStr(tgtName);
    Size = size;
    AsciiTransferMode = asciiTransferMode;
    SizeInBytes = sizeInBytes;
    TgtFileState = tgtFileState;
    DateAndTimeValid = (dateAndTimeValid != FALSE);
    Date = date;
    Time = time;
}

//
// ****************************************************************************
// CFTPQueueItemCopyOrMoveUpload
//

CFTPQueueItemCopyOrMoveUpload::CFTPQueueItemCopyOrMoveUpload()
{
    TgtPath = NULL;
    TgtName = NULL;
    Size.SetUI64(0);
    SizeWithCRLF_EOLs.SetUI64(0);
    NumberOfEOLs.SetUI64(0);
    AutorenamePhase = 0;
    RenamedName = NULL;
    AsciiTransferMode = 0;
    IgnoreAsciiTrModeForBinFile = 0;
    TgtFileState = 0;
}

CFTPQueueItemCopyOrMoveUpload::~CFTPQueueItemCopyOrMoveUpload()
{
    if (TgtPath != NULL)
        SalamanderGeneral->Free(TgtPath);
    if (TgtName != NULL)
        SalamanderGeneral->Free(TgtName);
    if (RenamedName != NULL)
    {
        TRACE_E("Unexpected situation in CFTPQueueItemCopyOrMoveUpload::~CFTPQueueItemCopyOrMoveUpload(): RenamedName != NULL");
        free(RenamedName);
    }
}

void CFTPQueueItemCopyOrMoveUpload::SetItemCopyOrMoveUpload(const char* tgtPath, const char* tgtName,
                                                            const CQuadWord& size, int asciiTransferMode,
                                                            int tgtFileState)
{
    TgtPath = SalamanderGeneral->DupStr(tgtPath);
    TgtName = SalamanderGeneral->DupStr(tgtName);
    Size = size;
    AsciiTransferMode = asciiTransferMode;
    TgtFileState = tgtFileState;
}

//
// ****************************************************************************
// CFTPQueueItemCopyMoveExplore
//

CFTPQueueItemCopyMoveExplore::CFTPQueueItemCopyMoveExplore()
{
    TgtPath = NULL;
    TgtName = NULL;
    TgtDirState = 0;
}

CFTPQueueItemCopyMoveExplore::~CFTPQueueItemCopyMoveExplore()
{
    if (TgtPath != NULL)
        SalamanderGeneral->Free(TgtPath);
    if (TgtName != NULL)
        SalamanderGeneral->Free(TgtName);
}

void CFTPQueueItemCopyMoveExplore::SetItemCopyMoveExplore(const char* tgtPath, const char* tgtName,
                                                          int tgtDirState)
{
    TgtPath = SalamanderGeneral->DupStr(tgtPath);
    TgtName = SalamanderGeneral->DupStr(tgtName);
    TgtDirState = tgtDirState;
}

//
// ****************************************************************************
// CFTPQueueItemCopyMoveUploadExplore
//

CFTPQueueItemCopyMoveUploadExplore::CFTPQueueItemCopyMoveUploadExplore()
{
    TgtPath = NULL;
    TgtName = NULL;
    TgtDirState = 0;
}

CFTPQueueItemCopyMoveUploadExplore::~CFTPQueueItemCopyMoveUploadExplore()
{
    if (TgtPath != NULL)
        SalamanderGeneral->Free(TgtPath);
    if (TgtName != NULL)
        SalamanderGeneral->Free(TgtName);
}

void CFTPQueueItemCopyMoveUploadExplore::SetItemCopyMoveUploadExplore(const char* tgtPath,
                                                                      const char* tgtName,
                                                                      int tgtDirState)
{
    TgtPath = SalamanderGeneral->DupStr(tgtPath);
    TgtName = SalamanderGeneral->DupStr(tgtName);
    TgtDirState = tgtDirState;
}

//
// ****************************************************************************
// CFTPQueueItemChAttr
//

CFTPQueueItemChAttr::CFTPQueueItemChAttr()
{
    Attr = 0;
    AttrErr = FALSE;
    OrigRights = NULL;
}

CFTPQueueItemChAttr::~CFTPQueueItemChAttr()
{
    if (OrigRights != NULL)
        free(OrigRights);
}

void CFTPQueueItemChAttr::SetItemChAttr(WORD attr, const char* origRights, BYTE attrErr)
{
    Attr = attr;
    AttrErr = attrErr;
    OrigRights = SalamanderGeneral->DupStr(origRights);
}

//
// ****************************************************************************
// CFTPQueueItemChAttrDir
//

CFTPQueueItemChAttrDir::CFTPQueueItemChAttrDir()
{
    Attr = 0;
    AttrErr = FALSE;
    OrigRights = NULL;
}

CFTPQueueItemChAttrDir::~CFTPQueueItemChAttrDir()
{
    if (OrigRights != NULL)
        free(OrigRights);
}

void CFTPQueueItemChAttrDir::SetItemChAttrDir(WORD attr, const char* origRights, BYTE attrErr)
{
    Attr = attr;
    AttrErr = attrErr;
    OrigRights = SalamanderGeneral->DupStr(origRights);
}

//
// ****************************************************************************
// CFTPQueueItemChAttrExplore
//

CFTPQueueItemChAttrExplore::CFTPQueueItemChAttrExplore()
{
    OrigRights = NULL;
}

CFTPQueueItemChAttrExplore::~CFTPQueueItemChAttrExplore()
{
    if (OrigRights != NULL)
        free(OrigRights);
}

void CFTPQueueItemChAttrExplore::SetItemChAttrExplore(const char* origRights)
{
    OrigRights = SalamanderGeneral->DupStr(origRights);
}

//
// ****************************************************************************
// CUIDArray
//

CUIDArray::CUIDArray(int base, int delta) : Data(base, delta)
{
    HANDLES(InitializeCriticalSection(&UIDListCritSect));
}

CUIDArray::~CUIDArray()
{
    HANDLES(DeleteCriticalSection(&UIDListCritSect));
}

BOOL CUIDArray::Add(int uid)
{
    HANDLES(EnterCriticalSection(&UIDListCritSect));
    BOOL ok = TRUE;
    Data.Add(uid);
    if (!Data.IsGood())
    {
        Data.ResetState();
        ok = FALSE;
    }
    HANDLES(LeaveCriticalSection(&UIDListCritSect));
    return ok;
}

BOOL CUIDArray::GetFirstUID(int& uid)
{
    HANDLES(EnterCriticalSection(&UIDListCritSect));
    BOOL ok = TRUE;
    if (Data.Count > 0)
    {
        uid = Data[0];
        Data.Delete(0);
        if (!Data.IsGood())
            Data.ResetState();
    }
    else
    {
        uid = -1;
        TRACE_E("Unexpected situation in CUIDArray::GetFirstUID(): no data!");
        ok = FALSE;
    }
    HANDLES(LeaveCriticalSection(&UIDListCritSect));
    return ok;
}
