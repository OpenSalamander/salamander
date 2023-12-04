// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

const char* GetWorkerErrorTxt(int error, char* errBuf, int errBufSize)
{
    char* e;
    if (error != NO_ERROR)
        e = FTPGetErrorText(error, errBuf, errBufSize);
    else
        e = LoadStr(IDS_UNKNOWNERROR);
    return e;
}

//
// ****************************************************************************
// CFTPWorker
//

CFTPWorker::CFTPWorker(CFTPOperation* oper, CFTPQueue* queue, const char* host,
                       unsigned short port, const char* user)
{
    ControlConnectionUID = -1;

    HaveWorkingPath = FALSE;
    WorkingPath[0] = 0;
    CurrentTransferMode = ctrmUnknown;

    EventConnectSent = FALSE;

    BytesToWrite = NULL;
    BytesToWriteCount = 0;
    BytesToWriteOffset = 0;
    BytesToWriteAllocatedSize = 0;

    ReadBytes = NULL;
    ReadBytesCount = 0;
    ReadBytesOffset = 0;
    ReadBytesAllocatedSize = 0;

    WorkerDataCon = NULL;
    WorkerUploadDataCon = NULL;
    WorkerDataConState = wdcsDoesNotExist;
    DataConAllDataTransferred = FALSE;

    HANDLES(InitializeCriticalSection(&WorkerCritSect));
    CopyOfUID = UID;
    CopyOfMsg = Msg;
    ID = 0;
    LogUID = -1;
    State = fwsLookingForWork;
    SubState = fwssNone;
    CurItem = NULL;
    ErrorDescr[0] = 0;
    ConnectAttemptNumber = 0;
    UnverifiedCertificate = NULL;

    ErrorOccurenceTime = -1;

    ShouldStop = FALSE;
    SocketClosed = TRUE;
    ShouldBePaused = FALSE;
    CommandState = fwcsIdle;
    CommandTransfersData = FALSE;
    CommandReplyTimeout = FALSE;
    WaitForCmdErrError = NO_ERROR;

    CanDeleteSocket = TRUE;
    ReturnToControlCon = FALSE;

    Oper = oper;
    Queue = queue;

    IPRequestUID = 0;
    NextInitCmd = 0;

    memset(&DiskWork, 0, sizeof(DiskWork));
    DiskWorkIsUsed = FALSE;

    ReceivingWakeup = FALSE;

    ProxyScriptExecPoint = NULL;
    ProxyScriptLastCmdReply = -1;

    OpenedFile = NULL;
    OpenedFileSize.Set(0, 0);
    OpenedFileOriginalSize.Set(0, 0);
    CanDeleteEmptyFile = FALSE;
    OpenedFileCurOffset.Set(0, 0);
    OpenedFileResumedAtOffset.Set(0, 0);
    ResumingOpenedFile = FALSE;
    memset(&StartTimeOfListing, 0, sizeof(StartTimeOfListing));
    StartLstTimeOfListing = 0;
    ListCmdReplyCode = -1;
    ListCmdReplyText = NULL;

    OpenedInFile = NULL;
    OpenedInFileSize.Set(0, 0);
    OpenedInFileCurOffset.Set(0, 0);
    OpenedInFileNumberOfEOLs.Set(0, 0);
    OpenedInFileSizeWithCRLF_EOLs.Set(0, 0);
    FileOnServerResumedAtOffset.Set(0, 0);
    ResumingFileOnServer = FALSE;

    LockedFileUID = 0;

    StatusType = wstNone;
    StatusConnectionIdleTime = 0;
    StatusSpeed = 0;
    StatusTransferred.Set(0, 0);
    StatusTotal.Set(-1, -1);

    LastTimeEstimation = -1;

    FlushDataError = fderNone;
    PrepareDataError = pderNone;

    UploadDirGetTgtPathListing = FALSE;

    UploadAutorenamePhase = 0;
    UploadAutorenameNewName[0] = 0;
    UploadType = utNone;
    UseDeleteForOverwrite = FALSE;

    if (Config.EnableLogging && !Config.DisableLoggingOfWorkers) // bez synchronizace, neni potreba
    {
        Logs.CreateLog(&LogUID, host, port, user, NULL, FALSE, TRUE);
        Oper->SendHeaderToLog(LogUID);
    }
}

CFTPWorker::~CFTPWorker()
{
    if (UnverifiedCertificate != NULL)
        UnverifiedCertificate->Release();
    if (ListCmdReplyText != NULL)
        SalamanderGeneral->Free(ListCmdReplyText);
    if (WorkerDataConState != wdcsDoesNotExist)
        TRACE_E("Unexpected situation in CFTPWorker::~CFTPWorker(): WorkerDataConState is not wdcsDoesNotExist!");
    if (WorkerDataCon != NULL)
        TRACE_E("Unexpected situation in CFTPWorker::~CFTPWorker(): WorkerDataCon is not NULL!");
    if (WorkerUploadDataCon != NULL)
        TRACE_E("Unexpected situation in CFTPWorker::~CFTPWorker(): WorkerUploadDataCon is not NULL!");
    if (OpenedFile != NULL)
        TRACE_E("Unexpected situation in CFTPWorker::~CFTPWorker(): OpenedFile is not NULL!");
    if (OpenedInFile != NULL)
        TRACE_E("Unexpected situation in CFTPWorker::~CFTPWorker(): OpenedInFile is not NULL!");
    if (DiskWorkIsUsed)
        TRACE_E("Unexpected situation in CFTPWorker::~CFTPWorker(): DiskWorkIsUsed is TRUE!");
    if (CurItem != NULL)
        TRACE_E("Unexpected situation in CFTPWorker::~CFTPWorker(): CurItem is not NULL!");
    HANDLES(DeleteCriticalSection(&WorkerCritSect));
    if (BytesToWrite != NULL)
        free(BytesToWrite);
    if (ReadBytes != NULL)
        free(ReadBytes);
    if (LogUID != -1)
        Logs.ClosingConnection(LogUID);
}

int CFTPWorker::GetID()
{
    CALL_STACK_MESSAGE1("CFTPWorker::GetID()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    int id = ID;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return id;
}

int CFTPWorker::GetLogUID()
{
    CALL_STACK_MESSAGE1("CFTPWorker::GetLogUID()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    int uid = LogUID;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return uid;
}

BOOL CFTPWorker::GetShouldStop()
{
    CALL_STACK_MESSAGE1("CFTPWorker::GetShouldStop()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = ShouldStop;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

void CFTPWorker::SetID(int id)
{
    CALL_STACK_MESSAGE1("CFTPWorker::SetID()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    ID = id;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
}

CFTPWorkerState
CFTPWorker::GetState()
{
    CALL_STACK_MESSAGE1("CFTPWorker::GetState()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    CFTPWorkerState state = State;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return state;
}

BOOL CFTPWorker::IsPaused(BOOL* isWorking)
{
    CALL_STACK_MESSAGE1("CFTPWorker::IsPaused()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = ShouldBePaused;
    *isWorking = State != fwsSleeping && State != fwsConnectionError && State != fwsStopped;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

BOOL CFTPWorker::RefreshCopiesOfUIDAndMsg()
{
    CALL_STACK_MESSAGE1("CFTPWorker::RefreshCopiesOfUIDAndMsg()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    CopyOfUID = UID;
    CopyOfMsg = Msg;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return TRUE;
}

int CFTPWorker::GetCopyOfUID()
{
    CALL_STACK_MESSAGE1("CFTPWorker::GetCopyOfUID()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    int uid = CopyOfUID;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return uid;
}

int CFTPWorker::GetCopyOfMsg()
{
    CALL_STACK_MESSAGE1("CFTPWorker::GetCopyOfMsg()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    int msg = CopyOfMsg;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return msg;
}

void CFTPWorker::CorrectErrorDescr()
{
#ifdef _DEBUG
    if (WorkerCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CFTPWorker::CorrectErrorDescr(): not from section WorkerCritSect!");
#endif

    // CR+LF prelozime na mezery
    char* s = ErrorDescr;
    char* end = ErrorDescr + FTPWORKER_ERRDESCR_BUFSIZE - 1;
    while (s < end && *s != 0)
    {
        if (*s == '\r')
            *s = ' ';
        if (*s == '\n')
            *s = ' ';
        s++;
    }
    // mezery a tecky z konce retezce vyhazime
    end = s;
    while (end > ErrorDescr && (*(end - 1) == '.' || *(end - 1) == ' '))
        end--;
    *end = 0;
}

void CFTPWorker::InitDiskWork(DWORD msgID, CFTPDiskWorkType type, const char* path, const char* name,
                              CFTPQueueItemAction forceAction, BOOL alreadyRenamedName,
                              char* flushDataBuffer, CQuadWord const* checkFromOffset,
                              CQuadWord const* writeOrReadFromOffset, int validBytesInFlushDataBuffer,
                              HANDLE workFile)
{
#ifdef _DEBUG
    if (SocketCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CFTPWorker::InitDiskWork(): not from section SocketCritSect!");
#endif

    DiskWork.SocketMsg = Msg;
    DiskWork.SocketUID = UID;
    DiskWork.MsgID = msgID;
    DiskWork.Type = type;
    if (path != NULL)
        lstrcpyn(DiskWork.Path, path, MAX_PATH);
    else
        DiskWork.Path[0] = 0;
    if (name != NULL)
        lstrcpyn(DiskWork.Name, name, MAX_PATH);
    else
        DiskWork.Name[0] = 0;
    DiskWork.ForceAction = forceAction;
    DiskWork.AlreadyRenamedName = alreadyRenamedName;
    Oper->GetDiskOperDefaults(&DiskWork);
    DiskWork.ProblemID = ITEMPR_OK;
    DiskWork.WinError = NO_ERROR;
    DiskWork.State = sqisNone;
    if (DiskWork.NewTgtName != NULL)
        TRACE_E("CFTPWorker::InitDiskWork(): DiskWork.NewTgtName is not empty!");
    DiskWork.NewTgtName = NULL;
    if (DiskWork.OpenedFile != NULL)
        TRACE_E("CFTPWorker::InitDiskWork(): DiskWork.OpenedFile is not NULL!");
    DiskWork.OpenedFile = NULL;
    DiskWork.FileSize.Set(0, 0);
    DiskWork.CanOverwrite = FALSE;
    DiskWork.CanDeleteEmptyFile = FALSE;
    if (DiskWork.DiskListing != NULL)
        TRACE_E("CFTPWorker::InitDiskWork(): DiskWork.DiskListing is not NULL!");
    DiskWork.DiskListing = NULL;

    if (DiskWork.FlushDataBuffer != NULL)
        TRACE_E("CFTPWorker::InitDiskWork(): DiskWork.FlushDataBuffer must be NULL!");
    if (flushDataBuffer != NULL)
    {
        DiskWork.FlushDataBuffer = flushDataBuffer;
        if (checkFromOffset != NULL)
            DiskWork.CheckFromOffset = *checkFromOffset;
        else
            DiskWork.CheckFromOffset.Set(0, 0);
        DiskWork.WriteOrReadFromOffset = *writeOrReadFromOffset;
        DiskWork.ValidBytesInFlushDataBuffer = validBytesInFlushDataBuffer;
        DiskWork.WorkFile = workFile;
    }
    else
    {
        DiskWork.FlushDataBuffer = NULL;
        DiskWork.CheckFromOffset.Set(0, 0);
        DiskWork.WriteOrReadFromOffset.Set(0, 0);
        DiskWork.ValidBytesInFlushDataBuffer = 0;
        DiskWork.WorkFile = NULL;
    }
    DiskWork.EOLsInFlushDataBuffer = 0;
}

void CFTPWorker::GetListViewData(LVITEM* itemData, char* buf, int bufSize)
{
    CALL_STACK_MESSAGE1("CFTPWorker::GetListViewData()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (itemData->mask & LVIF_IMAGE)
        itemData->iImage = 0; // zatim mame jedinou ikonu
    if ((itemData->mask & LVIF_TEXT) && bufSize > 0)
    {
        switch (itemData->iSubItem)
        {
        case 0: // ID
        {
            _snprintf_s(buf, bufSize, _TRUNCATE, "%d", ID);
            break;
        }

        case 1: // Action
        {
            if (ShouldStop)
                _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_STOPPING));
            else
            {
                switch (State)
                {
                case fwsLookingForWork:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_LOOKFORWORK));
                    break;
                case fwsSleeping:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_SLEEPING));
                    break;
                case fwsConnectionError:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_WAITFORUSER));
                    break;
                case fwsStopped:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_STOPPING));
                    break;

                case fwsPreparing:
                case fwsConnecting:
                case fwsWaitingForReconnect:
                case fwsWorking:
                {
                    if (CurItem != NULL) // "always true"
                    {
                        int strResID = -1;
                        switch (CurItem->Type)
                        {
                        case fqitDeleteFile:
                            strResID = IDS_OPERDLGCOACT_DELFILE;
                            break;

                        case fqitCopyFileOrFileLink:
                        case fqitUploadCopyFile:
                            strResID = IDS_OPERDLGCOACT_COPY;
                            break;

                        case fqitMoveFileOrFileLink:
                        case fqitUploadMoveFile:
                            strResID = IDS_OPERDLGCOACT_MOVE;
                            break;

                        case fqitChAttrsFile:
                        case fqitChAttrsDir:
                            strResID = IDS_OPERDLGCOACT_CHATTR;
                            break;

                        case fqitMoveDeleteDirLink:
                        case fqitDeleteLink:
                            strResID = IDS_OPERDLGCOACT_DELLINK;
                            break;

                        case fqitMoveDeleteDir:
                        case fqitUploadMoveDeleteDir:
                        case fqitDeleteDir:
                            strResID = IDS_OPERDLGCOACT_DELDIR;
                            break;

                        case fqitChAttrsExploreDirLink:
                        case fqitMoveExploreDirLink:
                        case fqitCopyExploreDir:
                        case fqitMoveExploreDir:
                        case fqitDeleteExploreDir:
                        case fqitChAttrsExploreDir:
                        case fqitUploadCopyExploreDir:
                        case fqitUploadMoveExploreDir:
                            strResID = IDS_OPERDLGCOACT_EXPLDIR;
                            break;

                        case fqitCopyResolveLink:
                        case fqitMoveResolveLink:
                        case fqitChAttrsResolveLink:
                            strResID = IDS_OPERDLGCOACT_RESLINK;
                            break;

                        default:
                        {
                            TRACE_E("Unexpected situation in CFTPWorker::GetListViewData(): unknown active operation item type!");
                            break;
                        }
                        }
                        if (strResID != -1)
                            _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(strResID), CurItem->Name);
                        else
                            buf[0] = 0;
                    }
                    else
                    {
                        TRACE_E("Unexpected situation in CFTPWorker::GetListViewData(): missing active operation item!");
                        buf[0] = 0;
                    }
                    break;
                }

                default:
                {
                    TRACE_E("Unexpected situation in CFTPWorker::GetListViewData(): unknown worker state!");
                    buf[0] = 0;
                    break;
                }
                }
            }
            break;
        }

        case 2: // Status
        {
            buf[0] = 0;
            if (!ShouldStop)
            {
                switch (State)
                {
                case fwsLookingForWork:
                {
                    if (ShouldBePaused)
                        _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_PAUSED));
                    break;
                }

                case fwsSleeping:
                case fwsStopped:
                    break; // zadny text v techto pripadech

                case fwsPreparing:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_PREPARING));
                    break;
                case fwsConnecting:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_CONNECTING));
                    break;

                case fwsWaitingForReconnect:
                {
                    if (ShouldBePaused)
                        _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_PAUSED));
                    else
                    {
                        _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_WAITRECON),
                                    Config.GetDelayBetweenConRetries(), ConnectAttemptNumber,
                                    Config.GetConnectRetries() + 1, ErrorDescr);
                    }
                    break;
                }

                case fwsConnectionError:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_CONERROR), ErrorDescr);
                    break;
                }

                case fwsWorking:
                {
                    if (CurItem != NULL) // "always true"
                    {
                        char* bufRest = buf;
                        int bufRestSize = bufSize;
                        int prefixLen = -1;
                        if (ShouldBePaused)
                        {
                            prefixLen = _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_PAUSED));
                            if (prefixLen >= 0 && prefixLen + 2 < bufSize)
                            {
                                buf[prefixLen] = ':';
                                buf[prefixLen + 1] = ' ';
                                buf[prefixLen + 2] = 0;
                                bufRest += prefixLen + 2;
                                bufRestSize -= prefixLen + 2;
                            }
                            else
                            {
                                prefixLen = -1;
                                buf[0] = 0;
                            }
                        }
                        if (SubState == fwssWorkUploadWaitForListing)
                            _snprintf_s(bufRest, bufRestSize, _TRUNCATE, LoadStr(IDS_OPERDLGCOACT_WAITFORLIST));
                        else
                        {
                            if ((StatusType == wstDownloadStatus || StatusType == wstUploadStatus) && bufRestSize > 0 &&
                                (StatusTransferred > CQuadWord(0, 0) || !ShouldBePaused && StatusConnectionIdleTime > 30))
                            {
                                char num1[100];
                                char num2[100];
                                char num3[100];
                                if (ShouldBePaused)
                                    num3[0] = 0;
                                else
                                {
                                    if (StatusConnectionIdleTime <= 30)
                                    {
                                        if (StatusSpeed > 0)
                                        {
                                            SalamanderGeneral->PrintDiskSize(num1, CQuadWord(StatusSpeed, 0), 0);
                                            _snprintf_s(num3, _TRUNCATE, LoadStr(IDS_LISTWNDDOWNLOADSPEED), num1);
                                        }
                                        else
                                            num3[0] = 0;
                                    }
                                    else
                                    {
                                        SalamanderGeneral->PrintTimeLeft(num1, CQuadWord(StatusConnectionIdleTime, 0));
                                        _snprintf_s(num3, _TRUNCATE, LoadStr(IDS_LISTWNDCONNECTIONIDLE), num1);
                                    }
                                }
                                CQuadWord transferredSize;
                                if (StatusType == wstDownloadStatus)
                                {
                                    transferredSize = ResumingOpenedFile ? OpenedFileResumedAtOffset + StatusTransferred : StatusTransferred;
                                }
                                else // wstUploadStatus
                                {
                                    transferredSize = ResumingFileOnServer ? FileOnServerResumedAtOffset + StatusTransferred : StatusTransferred;
                                }
                                SalamanderGeneral->PrintDiskSize(num1, transferredSize, 0); // pozor num1 pouzito u tvorby num3
                                if (StatusTotal != CQuadWord(-1, -1))
                                {
                                    if (StatusType == wstUploadStatus && StatusTotal == transferredSize)
                                        num3[0] = 0; // uz se nic neuploadi
                                    int off = 0;
                                    SalamanderGeneral->PrintDiskSize(num2, StatusTotal, 0);
                                    off = _snprintf_s(bufRest, bufRestSize, _TRUNCATE, LoadStr(num3[0] != 0 ? IDS_LISTWNDSTATUS1 : IDS_OPERDLGSTATUS2),
                                                      num1, num2, num3);
                                    if (off < 0)
                                        off = bufRestSize;
                                    if (StatusTotal > CQuadWord(0, 0))
                                    {
                                        int progress = ((int)((CQuadWord(1000, 0) * transferredSize) / StatusTotal).Value /*+ 5*/) / 10; // nezaokrouhlujeme (100% musi byt az pri 100% a ne pri 99.5%)
                                        if (progress > 100)
                                            progress = 100;
                                        if (progress < 0)
                                            progress = 0;

                                        char timeLeftText[200];
                                        timeLeftText[0] = 0;
                                        if (!ShouldBePaused && (StatusSpeed > 0 || StatusConnectionIdleTime > 30))
                                        {
                                            if (StatusTotal > CQuadWord(0, 0) && StatusSpeed > 0 && StatusConnectionIdleTime <= 30 &&
                                                StatusTotal > transferredSize)
                                            {
                                                CQuadWord waiting = StatusTotal - transferredSize;
                                                CQuadWord secs = waiting / CQuadWord(StatusSpeed, 0); // odhad zbyvajicich sekund
                                                secs.Value++;                                         // jedna vterina navic, abysme koncili operaci s "time left: 1 sec" (misto 0 sec)
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
                                                lstrcpyn(timeLeftText, LoadStr(IDS_OPERDLGCOACT_TIMELEFT), 200);
                                                int len = (int)strlen(timeLeftText);
                                                if (len < 99) // celkem 200, takze pokud ma zbyt 100 znaku pro casovy udaj, musi byt len < 99
                                                {
                                                    timeLeftText[len++] = ' ';
                                                    SalamanderGeneral->PrintTimeLeft(timeLeftText + len, secs);
                                                }
                                                LastTimeEstimation = (int)secs.Value;
                                            }
                                            else
                                            {
                                                if (StatusConnectionIdleTime > 30)
                                                {
                                                    char idleTime[100];
                                                    SalamanderGeneral->PrintTimeLeft(idleTime, CQuadWord(StatusConnectionIdleTime, 0));
                                                    _snprintf_s(timeLeftText, _TRUNCATE, LoadStr(IDS_OPERDLGCONNECTIONSIDLE), idleTime);
                                                }
                                            }
                                        }
                                        if (off < bufRestSize)
                                        {
                                            if (timeLeftText[0] != 0)
                                                _snprintf_s(bufRest + off, bufRestSize - off, _TRUNCATE, ", %d %%, %s", progress, timeLeftText);
                                            else
                                                _snprintf_s(bufRest + off, bufRestSize - off, _TRUNCATE, ", %d %%", progress);
                                        }
                                    }
                                }
                                else
                                {
                                    if (num3[0] != 0)
                                        _snprintf_s(bufRest, bufRestSize, _TRUNCATE, LoadStr(IDS_LISTWNDSTATUS2), num1, num3);
                                    else
                                        lstrcpyn(bufRest, num1, bufRestSize);
                                }
                            }
                        }
                        if (prefixLen != -1 && *bufRest == 0)
                            *(bufRest - 2) = 0; // na prefixem neni potreba ": "
                    }
                    else
                        TRACE_E("Unexpected situation 2 in CFTPWorker::GetListViewData(): missing active operation item!");
                    break;
                }

                default:
                {
                    TRACE_E("Unexpected situation 2 in CFTPWorker::GetListViewData(): unknown worker state!");
                    break;
                }
                }
            }
            break;
        }
        }
        itemData->pszText = buf;
    }
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
}

void CFTPWorker::ReportWorkerMayBeClosed()
{
    HANDLES(EnterCriticalSection(&WorkerMayBeClosedStateCS));
    WorkerMayBeClosedState++;
    HANDLES(LeaveCriticalSection(&WorkerMayBeClosedStateCS));
    PulseEvent(WorkerMayBeClosedEvent);
}

BOOL CFTPWorker::HaveError()
{
    CALL_STACK_MESSAGE1("CFTPWorker::HaveError()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = State == fwsConnectionError || State == fwsWaitingForReconnect;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

BOOL CFTPWorker::GetErrorDescr(char* buf, int bufSize, BOOL* postActivate, CCertificate** unverifiedCertificate)
{
    CALL_STACK_MESSAGE1("CFTPWorker::GetErrorDescr(,)");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (unverifiedCertificate != NULL)
        *unverifiedCertificate = NULL;
    BOOL operStatusMaybeChanged = FALSE;
    BOOL ret = HaveError();
    if (ret)
    {
        if (State == fwsWaitingForReconnect)
        {
            State = fwsConnectionError; // aby behem userova reseni problemu nezacal provadet dalsi reconnect
            operStatusMaybeChanged = TRUE;
            // ErrorOccurenceTime se zde nenastavuje - nejde o chybu, ktera by nastala sama - user si ji vynutil
            SubState = fwssNone;
            *postActivate = TRUE; // fweActivate se postne po dokonceni metody, v fwsConnectionError se musi vratit polozka do fronty
            Oper->ReportWorkerChange(ID, FALSE);
        }
        lstrcpyn(buf, ErrorDescr, bufSize);
        if (unverifiedCertificate != NULL && UnverifiedCertificate != NULL)
        {
            *unverifiedCertificate = UnverifiedCertificate;
            (*unverifiedCertificate)->AddRef();
        }
    }
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    if (operStatusMaybeChanged)
        Oper->OperationStatusMaybeChanged();
    return ret;
}

BOOL CFTPWorker::CanDeleteFromRetCons()
{
    CALL_STACK_MESSAGE1("CFTPWorker::CanDeleteFromRetCons()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = CanDeleteSocket;
    ReturnToControlCon = FALSE; // uz jsme se ptali z metod CReturningConnections
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

BOOL CFTPWorker::CanDeleteFromDelWorkers()
{
    CALL_STACK_MESSAGE1("CFTPWorker::CanDeleteFromDelWorkers()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = !ReturnToControlCon;
    CanDeleteSocket = TRUE; // uz jsme se ptali z DeleteWorkers
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

BOOL CFTPWorker::InformAboutStop()
{
    CALL_STACK_MESSAGE1("CFTPWorker::InformAboutStop()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = FALSE;
    if (!ShouldStop) // pokud nejde o opakovane volani
    {
        ShouldStop = TRUE;
        Oper->ReportWorkerChange(ID, FALSE);
        if (!SocketClosed && (State == fwsSleeping || DiskWorkIsUsed ||
                              State == fwsWorking && (SubState == fwssWorkUploadWaitForListing ||
                                                      SubState == fwssWorkExplWaitForListen ||
                                                      SubState == fwssWorkCopyWaitForListen ||
                                                      SubState == fwssWorkUploadWaitForListen) ||
                              State == fwsLookingForWork && ShouldBePaused))
        { // "idle" stav s otevrenym spojenim -> potrebujeme postnout WORKER_SHOULDSTOP
            ret = TRUE;
        }
        else
        { // pokud existuje "data-connection" a necekame na spojeni se serverem, potrebujeme "data-connection" zavrit
            ret = WorkerDataConState == wdcsOnlyAllocated || WorkerDataConState == wdcsTransferingData ||
                  WorkerDataConState == wdcsTransferFinished;
        }
    }
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

void CFTPWorker::CloseDataConnectionOrPostShouldStop()
{
    CALL_STACK_MESSAGE1("CFTPWorker::CloseDataConnectionOrPostShouldStop()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    // pokud existuje "data-connection" a necekame na spojeni se serverem, potrebujeme "data-connection" zavrit
    CDataConnectionSocket* workerDataCon = NULL;
    CUploadDataConnectionSocket* workerUploadDataCon = NULL;
    if (WorkerDataConState == wdcsOnlyAllocated || WorkerDataConState == wdcsTransferingData ||
        WorkerDataConState == wdcsTransferFinished)
    {
        workerDataCon = WorkerDataCon;
        WorkerDataCon = NULL;
        workerUploadDataCon = WorkerUploadDataCon;
        WorkerUploadDataCon = NULL;
        WorkerDataConState = wdcsDoesNotExist;
    }
    BOOL postShouldStop = (!SocketClosed && (State == fwsSleeping || DiskWorkIsUsed ||
                                             State == fwsWorking && (SubState == fwssWorkUploadWaitForListing ||
                                                                     SubState == fwssWorkExplWaitForListen ||
                                                                     SubState == fwssWorkCopyWaitForListen ||
                                                                     SubState == fwssWorkUploadWaitForListen) ||
                                             State == fwsLookingForWork && ShouldBePaused));
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (workerDataCon != NULL)
    {
        if (workerDataCon->IsConnected()) // zavreme "data connection", system se pokusi o "graceful"
        {
            workerDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
            postShouldStop = TRUE;              // musime "rozhybat" workera, aby se ukoncil (WORKER_DATACON_CLOSED nedorazi)
        }
        workerDataCon->FreeFlushData();
        DeleteSocket(workerDataCon);
    }
    if (workerUploadDataCon != NULL)
    {
        if (workerUploadDataCon->IsConnected()) // zavreme "data connection", system se pokusi o "graceful"
        {
            workerUploadDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
            postShouldStop = TRUE;                    // musime "rozhybat" workera, aby se ukoncil (WORKER_UPLDATACON_CLOSED nedorazi)
        }
        workerUploadDataCon->FreeBufferedData();
        DeleteSocket(workerUploadDataCon);
    }

    if (postShouldStop)
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        int msg = Msg;
        int uid = UID;
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        SocketsThread->PostSocketMessage(msg, uid, WORKER_SHOULDSTOP, NULL);
    }
}

BOOL CFTPWorker::InformAboutPause(BOOL pause)
{
    CALL_STACK_MESSAGE1("CFTPWorker::InformAboutPause()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = FALSE;
    if (ShouldBePaused != pause) // pokud nejde o zbytecne nebo opakovane volani
    {
        Logs.LogMessage(LogUID, LoadStr(pause ? IDS_LOGMSGPAUSE : IDS_LOGMSGRESUME), -1, TRUE);
        ShouldBePaused = pause;
        Oper->ReportWorkerChange(ID, FALSE);
        ret = TRUE; // budeme volat PostShouldPauseOrResume()
    }
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

void CFTPWorker::PostShouldPauseOrResume()
{
    CALL_STACK_MESSAGE1("CFTPWorker::PostShouldPauseOrResume()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL paused = ShouldBePaused;
    BOOL leaveWorkerCS = TRUE;
    if (paused && State == fwsWorking)
    {
        DWORD statusConnectionIdleTime;
        DWORD statusSpeed;
        CQuadWord statusTransferred;
        CQuadWord statusTotal;
        BOOL writeNewStatus = FALSE;
        if (StatusType == wstDownloadStatus && WorkerDataCon != NULL)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            leaveWorkerCS = FALSE;
            if (WorkerDataCon->IsTransfering(NULL)) // prenos dat jeste probiha
            {                                       // ziskame status informace z data-connectiony
                WorkerDataCon->GetStatus(&statusTransferred, &statusTotal, &statusConnectionIdleTime, &statusSpeed);
                writeNewStatus = TRUE;
            }
        }
        else
        {
            if (StatusType == wstUploadStatus && WorkerUploadDataCon != NULL)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                leaveWorkerCS = FALSE;
                // ziskame status informace z data-connectiony
                WorkerUploadDataCon->GetStatus(&statusTransferred, &statusTotal, &statusConnectionIdleTime, &statusSpeed);
                writeNewStatus = TRUE;
            }
        }
        if (writeNewStatus)
        {
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            leaveWorkerCS = TRUE;
            StatusConnectionIdleTime = statusConnectionIdleTime;
            StatusSpeed = statusSpeed;
            StatusTransferred = statusTransferred;
            StatusTotal = statusTotal;
        }
    }
    if (leaveWorkerCS)
        HANDLES(LeaveCriticalSection(&WorkerCritSect));
    int msg = Msg;
    int uid = UID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    SocketsThread->PostSocketMessage(msg, uid, paused ? WORKER_SHOULDPAUSE : WORKER_SHOULDRESUME, NULL);
}

BOOL CFTPWorker::SocketClosedAndDataConDoesntExist()
{
    CALL_STACK_MESSAGE1("CFTPWorker::SocketClosedAndDataConDoesntExist()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = SocketClosed && WorkerDataConState == wdcsDoesNotExist; // nelze pouzit IsConnected(), nesmi vstoupit do sekce CSocket::SocketCritSect
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

BOOL CFTPWorker::HaveWorkInDiskThread()
{
    CALL_STACK_MESSAGE1("CFTPWorker::HaveWorkInDiskThread()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = DiskWorkIsUsed;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

void CFTPWorker::ForceClose()
{
    CALL_STACK_MESSAGE1("CFTPWorker::ForceClose()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    CDataConnectionSocket* workerDataCon = WorkerDataCon;
    WorkerDataCon = NULL;
    CUploadDataConnectionSocket* workerUploadDataCon = WorkerUploadDataCon;
    WorkerUploadDataCon = NULL;
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    WorkerDataConState = wdcsDoesNotExist;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (workerDataCon != NULL)
    {
        if (workerDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
            workerDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
        workerDataCon->FreeFlushData();
        DeleteSocket(workerDataCon);
    }
    if (workerUploadDataCon != NULL)
    {
        if (workerUploadDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
            workerUploadDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
        workerUploadDataCon->FreeBufferedData();
        DeleteSocket(workerUploadDataCon);
    }

    HANDLES(EnterCriticalSection(&SocketCritSect)); // optimalizace: zajisteni prubehu IsConnected a CloseSocket najednou
    if (IsConnected())
        CloseSocket(NULL); // zavreme socket
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    // simulace zavreni socketu ze strany serveru (pro sychr udelame vzdy, nejen pri otevrenem socketu)
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    SocketClosed = TRUE;
    int logUID = LogUID; // UID logu workera
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    Logs.SetIsConnected(logUID, IsConnected());
    Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"
    ReportWorkerMayBeClosed();         // ohlasime zavreni socketu (pro ostatni cekajici thready)
}

void CFTPWorker::ForceCloseDiskWork()
{
    CALL_STACK_MESSAGE1("CFTPWorker::ForceCloseDiskWork()");
    // POZOR: muze se volat opakovane (viz volani CFTPWorkersList::ForceCloseWorkers())
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (DiskWorkIsUsed)
    {
        BOOL workIsInProgress;
        if (FTPDiskThread->CancelWork(&DiskWork, &workIsInProgress))
        {
            if (workIsInProgress)
                DiskWork.FlushDataBuffer = NULL; // prace je rozdelana, nemuzeme uvolnit buffer se zapisovanymi/testovanymi daty (nebo pro nacitana data), nechame to na disk-work threadu (viz cast cancelovani prace) - do DiskWork muzeme zapisovat, protoze po Cancelu do nej uz disk-thread nesmi pristupovat (napr. uz vubec nemusi existovat)
            else
            { // prace byla zcanclovana pred tim, nez ji disk-thread zacal provadet - provedeme dealokaci flush bufferu (pro download i pro upload)
                if (DiskWork.FlushDataBuffer != NULL)
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }
            }
            DiskWorkIsUsed = FALSE; // pokud je jiz prace hotova, pockame az se worker ukonci sam o sobe (jinak praci na disku prerusime)
        }
    }
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
}

void CFTPWorker::ReleaseData(CUploadWaitingWorker** uploadFirstWaitingWorker)
{
    CALL_STACK_MESSAGE1("CFTPWorker::ReleaseData()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (CommandState == fwcsWaitForCmdError) // worker byl ukoncen drive nez stihl dojit timer WORKER_CMDERRORTIMERID, chybove hlaseni musime tedy vypsat zde
    {
        if (WaitForCmdErrError != NO_ERROR) // je-li co zobrazit, zobrazime to
        {
            char errBuf[300];
            FTPGetErrorTextForLog(WaitForCmdErrError, errBuf, 300);
            Logs.LogMessage(LogUID, errBuf, -1, TRUE);
        }
        CommandState = fwcsIdle;
        CommandTransfersData = FALSE;
        WaitForCmdErrError = NO_ERROR;
    }
    if (!SocketClosed || !ShouldStop)
        TRACE_E("Unexpected situation in CFTPWorker::ReleaseData(): !SocketClosed || !ShouldStop");
    if (WorkerDataConState != wdcsDoesNotExist)
        TRACE_E("Unexpected situation in CFTPWorker::ReleaseData(): WorkerDataConState != wdcsDoesNotExist");
    CloseOpenedFile(TRUE, FALSE, NULL, NULL, FALSE, NULL);
    CloseOpenedInFile();
    if (LockedFileUID != 0)
    {
        FTPOpenedFiles.CloseFile(LockedFileUID);
        LockedFileUID = 0;
    }
    if (CurItem != NULL)
    {
        char userBuf[USER_MAX_SIZE];
        char hostBuf[HOST_MAX_SIZE];
        unsigned short port;
        if (UploadDirGetTgtPathListing)
        { // listovani selhalo, informujeme o tom pripadne cekajici workery
            UploadDirGetTgtPathListing = FALSE;
            Oper->GetUserHostPort(userBuf, hostBuf, &port);
            char* tgtPath = NULL;
            if (CurItem->Type == fqitUploadCopyExploreDir || CurItem->Type == fqitUploadMoveExploreDir)
                tgtPath = ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath;
            else
            {
                if (CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile)
                    tgtPath = ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtPath;
                else
                    TRACE_E("CFTPWorker::ReleaseData(): UploadDirGetTgtPathListing: unknown CurItem->Type: " << CurItem->Type);
            }
            CFTPServerPathType pathType = Oper->GetFTPServerPathType(tgtPath);
            UploadListingCache.ListingFailed(userBuf, hostBuf, port, tgtPath,
                                             pathType, FALSE, uploadFirstWaitingWorker, NULL);
        }
        else
        {
            if (State == fwsWorking)
            {
                switch (SubState)
                {
                case fwssWorkDelFileWaitForDELERes:
                case fwssWorkDelDirWaitForRMDRes:
                case fwssWorkCopyMoveWaitForDELERes:
                {
                    // pokud nevime jak dopadl vymaz souboru/linku/adresare, zneplatnime listing v cache
                    Oper->GetUserHostPort(userBuf, hostBuf, &port);
                    UploadListingCache.ReportDelete(userBuf, hostBuf, port, CurItem->Path,
                                                    Oper->GetFTPServerPathType(CurItem->Path),
                                                    CurItem->Name, TRUE);
                    break;
                }

                case fwssWorkUploadCrDirWaitForMKDRes:
                {
                    if (CurItem->Type == fqitUploadCopyExploreDir || CurItem->Type == fqitUploadMoveExploreDir) // "always true"
                    {
                        // pokud nevime jak dopadlo vytvoreni adresare, zneplatnime listing v cache
                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                        CFTPQueueItemCopyMoveUploadExplore* curItem = (CFTPQueueItemCopyMoveUploadExplore*)CurItem;
                        UploadListingCache.ReportCreateDirs(userBuf, hostBuf, port, curItem->TgtPath,
                                                            Oper->GetFTPServerPathType(curItem->TgtPath),
                                                            curItem->TgtName, TRUE);
                    }
                    break;
                }

                case fwssWorkUploadAutorenDirWaitForMKDRes:
                {
                    if (CurItem->Type == fqitUploadCopyExploreDir || CurItem->Type == fqitUploadMoveExploreDir) // "always true"
                    {
                        // pokud nevime jak dopadlo vytvoreni adresare, zneplatnime listing v cache
                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                        CFTPQueueItemCopyMoveUploadExplore* curItem = (CFTPQueueItemCopyMoveUploadExplore*)CurItem;
                        UploadListingCache.ReportCreateDirs(userBuf, hostBuf, port, curItem->TgtPath,
                                                            Oper->GetFTPServerPathType(curItem->TgtPath),
                                                            UploadAutorenameNewName, TRUE);
                    }
                    break;
                }

                case fwssWorkUploadActivateDataCon: // je mezistav mezi fwssWorkUploadSendSTORCmd a fwssWorkUploadWaitForSTORRes
                case fwssWorkUploadWaitForSTORRes:
                {
                    if (CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile) // "always true"
                    {
                        // vysledek prikazu STOR nezname, invalidatneme listing
                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                        CFTPQueueItemCopyOrMoveUpload* curItem = (CFTPQueueItemCopyOrMoveUpload*)CurItem;
                        UploadListingCache.ReportFileUploaded(userBuf, hostBuf, port, curItem->TgtPath,
                                                              Oper->GetFTPServerPathType(curItem->TgtPath),
                                                              curItem->TgtName, UPLOADSIZE_UNKNOWN, TRUE);

                        // krome pripadu, kdy STOR hlasi chybu "cannot create target file name" (coz je kdyz STOR hlasi
                        // nejakou chybu + nic se neuploadlo) povazujeme poslani prikazu STOR/APPE za dokonceni
                        // forcovane akce: "overwrite", "resume" a "resume or overwrite"
                        if (CurItem->ForceAction != fqiaNone) // vynucena akce timto prestava platit
                            Queue->UpdateForceAction(CurItem, fqiaNone);
                    }
                    break;
                }

                case fwssWorkUploadWaitForDELERes:
                case fwssWorkUploadDelForOverWaitForDELERes:
                {
                    if (CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile) // "always true"
                    {
                        // pokud nevime jak dopadl vymaz souboru/linku/adresare, zneplatnime listing v cache
                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                        CFTPQueueItemCopyOrMoveUpload* curItem = (CFTPQueueItemCopyOrMoveUpload*)CurItem;
                        UploadListingCache.ReportDelete(userBuf, hostBuf, port, curItem->TgtPath,
                                                        Oper->GetFTPServerPathType(curItem->TgtPath),
                                                        curItem->TgtName, TRUE);
                    }
                    break;
                }
                }
            }
        }
        if ((CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile) &&
            ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->RenamedName != NULL)
        {
            if (State == fwsWorking && SubState == fwssWorkUploadWaitForSTORRes)
                Queue->ChangeTgtNameToRenamedName((CFTPQueueItemCopyOrMoveUpload*)CurItem); // i kdyby jeste STOR neprobehl, porad je vic pravda, ze se soubor ukladal pod novym jmenem, nez pod puvodnim - napr. u prepisu existujiciho souboru je to zrejme
            else
                Queue->UpdateRenamedName((CFTPQueueItemCopyOrMoveUpload*)CurItem, NULL);
        }
        ReturnCurItemToQueue(); // vratime polozku do fronty
    }
    // vycistime data workera
    State = fwsStopped; // neni nutne volat Oper->OperationStatusMaybeChanged(), zavola se z CFTPOperation::DeleteWorkers()
    SubState = fwssNone;
    ErrorDescr[0] = 0;
    if (UnverifiedCertificate != NULL)
        UnverifiedCertificate->Release();
    UnverifiedCertificate = NULL;
    Oper->ReportWorkerChange(ID, FALSE);
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
}

void CFTPWorker::SocketWasClosed(DWORD error)
{
    CALL_STACK_MESSAGE2("CFTPWorker::SocketWasClosed(%u)", error);

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    SocketClosed = TRUE;
    int logUID = LogUID; // UID logu workera
    HANDLES(LeaveCriticalSection(&WorkerCritSect));

    Logs.SetIsConnected(logUID, IsConnected());
    Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"

    HandleSocketEvent(fwseClose, error, 0);

    ReportWorkerMayBeClosed(); // ohlasime zavreni socketu (pro ostatni cekajici thready)
}

void CFTPWorker::ReturnCurItemToQueue()
{
#ifdef _DEBUG
    if (WorkerCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CFTPWorker::ReturnCurItemToQueue(): not from section WorkerCritSect!");
#endif
    if (CurItem != NULL)
    {
        int uid = CurItem->UID;
        Queue->ReturnToWaitingItems(CurItem, Oper);
        Oper->ReportItemChange(uid); // pozadame o redraw polozky
        CurItem = NULL;
    }
    else
        TRACE_E("Useless call to CFTPWorker::ReturnCurItemToQueue()");
}

void CFTPWorker::CloseOpenedFile(BOOL transferAborted, BOOL setDateAndTime, const CFTPDate* date,
                                 const CFTPTime* time, BOOL deleteFile, CQuadWord* setEndOfFile)
{
#ifdef _DEBUG
    if (WorkerCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CFTPWorker::CloseOpenedFile(): not from section WorkerCritSect!");
#endif
    if (OpenedFile != NULL)
    {
        if (CurItem != NULL)
        {
            switch (CurItem->Type)
            {
            case fqitCopyFileOrFileLink: // kopirovani souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
            case fqitMoveFileOrFileLink: // presun souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
            {
                // nechame soubor zavrit (pri chybe pridani do disk-threadu soubor zustane otevreny,
                // protoze ho nemuzeme zavrit primo z duvodu, ze disk-thread muze jeho handle prave pouzivat)
                BOOL delEmptyFile = (transferAborted ? CanDeleteEmptyFile : FALSE);
                FTPDiskThread->AddFileToClose(((CFTPQueueItemCopyOrMove*)CurItem)->TgtPath,
                                              ((CFTPQueueItemCopyOrMove*)CurItem)->TgtName,
                                              OpenedFile, delEmptyFile, setDateAndTime, date,
                                              time, deleteFile, setEndOfFile, NULL);
                if (deleteFile || delEmptyFile && OpenedFileSize == CQuadWord(0, 0)) // temer jiste dojde k vymazu souboru - bud na primy prikaz nebo prenos vubec nezacal, takze resetneme stav v TgtFileState (nebudeme otravovat s "transfer has failed")
                    Queue->UpdateTgtFileState((CFTPQueueItemCopyOrMove*)CurItem, TGTFILESTATE_UNKNOWN);
                OpenedFile = NULL;
                OpenedFileSize.Set(0, 0);
                OpenedFileOriginalSize.Set(0, 0);
                CanDeleteEmptyFile = FALSE;
                OpenedFileCurOffset.Set(0, 0);
                OpenedFileResumedAtOffset.Set(0, 0);
                ResumingOpenedFile = FALSE;
                break;
            }

            default:
            {
                TRACE_E("Unexpected situation in CFTPWorker::CloseOpenedFile(): CurItem->Type is unknown!");
                break;
            }
            }
        }
        else
            TRACE_E("Unexpected situation in CFTPWorker::CloseOpenedFile(): CurItem is NULL!");
    }
}

void CFTPWorker::CloseOpenedInFile()
{
#ifdef _DEBUG
    if (WorkerCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CFTPWorker::CloseOpenedInFile(): not from section WorkerCritSect!");
#endif
    if (OpenedInFile != NULL)
    {
        if (CurItem != NULL)
        {
            switch (CurItem->Type)
            {
            case fqitUploadCopyFile: // upload: kopirovani souboru (objekt tridy CFTPQueueItemCopyOrMoveUpload)
            case fqitUploadMoveFile: // upload: presun souboru (objekt tridy CFTPQueueItemCopyOrMoveUpload)
            {
                // nechame soubor zavrit (pri chybe pridani do disk-threadu soubor zustane otevreny,
                // protoze ho nemuzeme zavrit primo z duvodu, ze disk-thread muze jeho handle prave pouzivat)
                FTPDiskThread->AddFileToClose(CurItem->Path, CurItem->Name, OpenedInFile, FALSE, FALSE, NULL,
                                              NULL, FALSE, NULL, NULL);
                OpenedInFile = NULL;
                OpenedInFileSize.Set(0, 0);
                OpenedInFileCurOffset.Set(0, 0);
                // OpenedInFileNumberOfEOLs.Set(0, 0);      // pouziva se po zavreni souboru, takze nesmime nulovat
                // OpenedInFileSizeWithCRLF_EOLs.Set(0, 0); // pouziva se po zavreni souboru, takze nesmime nulovat
                FileOnServerResumedAtOffset.Set(0, 0);
                ResumingFileOnServer = FALSE;
                break;
            }

            default:
            {
                TRACE_E("Unexpected situation in CFTPWorker::CloseOpenedInFile(): CurItem->Type is unknown!");
                break;
            }
            }
        }
        else
            TRACE_E("Unexpected situation in CFTPWorker::CloseOpenedInFile(): CurItem is NULL!");
    }
    UploadType = utNone; // jen tak pro poradek (nulovani neni nutne, ale...)
}

void CFTPWorker::PostActivateMsg()
{
    CALL_STACK_MESSAGE1("CFTPWorker::PostActivateMsg()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int msg = Msg;
    int uid = UID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    SocketsThread->PostSocketMessage(msg, uid, WORKER_ACTIVATE, NULL);
}

BOOL CFTPWorker::IsSleeping(BOOL* hasOpenedConnection, BOOL* receivingWakeup)
{
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = State == fwsSleeping;
    if (ret)
    {
        *hasOpenedConnection = !SocketClosed;
        *receivingWakeup = ReceivingWakeup;
    }
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

void CFTPWorker::SetReceivingWakeup(BOOL receivingWakeup)
{
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (State == fwsSleeping)
        ReceivingWakeup = receivingWakeup;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
}

void CFTPWorker::GiveWorkToSleepingConWorker(CFTPWorker* sourceWorker)
{
    CALL_STACK_MESSAGE1("CFTPWorker::GiveWorkToSleepingConWorker()");

    HANDLES(EnterCriticalSection(&(sourceWorker->WorkerCritSect)));
    if (sourceWorker->State != fwsConnecting)
        TRACE_E("Unexpected situation in CFTPWorker::GiveWorkToSleepingConWorker(): source worker is not in state fwsConnecting!");
    sourceWorker->State = fwsLookingForWork; // post activate by mel provest prechod na fwsSleeping (pokud mezitim nevznikla nejaka prace)
    sourceWorker->SubState = fwssNone;       // neni nutne volat Oper->OperationStatusMaybeChanged(), protoze predani prace nemuze zmenit stav operace (neni paused a nebude ani po prohozeni)
    sourceWorker->CloseOpenedFile(TRUE, FALSE, NULL, NULL, FALSE, NULL);
    sourceWorker->CloseOpenedInFile();
    CFTPQueueItem* curItem = sourceWorker->CurItem;
    sourceWorker->CurItem = NULL;
    Oper->ReportWorkerChange(sourceWorker->ID, FALSE); // pozadame o redraw workera
    HANDLES(LeaveCriticalSection(&(sourceWorker->WorkerCritSect)));

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (State != fwsSleeping || SocketClosed)
        TRACE_E("Unexpected situation in CFTPWorker::GiveWorkToSleepingConWorker(): target worker is not in state fwsSleeping or is not connected!");
    State = fwsPreparing;
    SubState = fwssNone;
    CurItem = curItem;
    Oper->ReportWorkerChange(ID, FALSE); // pozadame o redraw workera
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
}

void CFTPWorker::AddCurrentDownloadSize(CQuadWord* downloaded)
{
    CALL_STACK_MESSAGE1("CFTPWorker::AddCurrentDownloadSize()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (State == fwsWorking && StatusType == wstDownloadStatus &&
        CurItem != NULL && (CurItem->Type == fqitCopyFileOrFileLink || CurItem->Type == fqitMoveFileOrFileLink))
    {
        *downloaded += ResumingOpenedFile ? OpenedFileResumedAtOffset + StatusTransferred : StatusTransferred;
    }
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
}

void CFTPWorker::AddCurrentUploadSize(CQuadWord* uploaded)
{
    CALL_STACK_MESSAGE1("CFTPWorker::AddCurrentUploadSize()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (State == fwsWorking && StatusType == wstUploadStatus &&
        CurItem != NULL && (CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile))
    {
        *uploaded += ResumingFileOnServer ? FileOnServerResumedAtOffset + StatusTransferred : StatusTransferred;
    }
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
}

DWORD
CFTPWorker::GetErrorOccurenceTime()
{
    CALL_STACK_MESSAGE1("CFTPWorker::GetErrorOccurenceTime()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    DWORD ret = -1;
    if (State == fwsConnectionError)
        ret = ErrorOccurenceTime;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

void CFTPWorker::ResetBuffersAndEvents()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));

    EventConnectSent = FALSE;

    BytesToWriteCount = 0;
    BytesToWriteOffset = 0;

    ReadBytesCount = 0;
    ReadBytesOffset = 0;

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    CommandState = fwcsIdle;
    CommandTransfersData = FALSE;
    WaitForCmdErrError = NO_ERROR;
    HANDLES(LeaveCriticalSection(&WorkerCritSect));

    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CFTPWorker::Write(const char* buffer, int bytesToWrite, DWORD* error, BOOL* allBytesWritten)
{
    CALL_STACK_MESSAGE2("CFTPWorker::Write(, %d, ,)", bytesToWrite);
    if (bytesToWrite == -1)
        bytesToWrite = (int)strlen(buffer);
    if (error != NULL)
        *error = NO_ERROR;
    if (allBytesWritten != NULL)
        *allBytesWritten = FALSE;

    if (bytesToWrite == 0) // zapis prazdneho bufferu
    {
        if (allBytesWritten != NULL)
            *allBytesWritten = TRUE;
        return TRUE;
    }

    HANDLES(EnterCriticalSection(&SocketCritSect));

    BOOL ret = FALSE;
    if (Socket != INVALID_SOCKET) // socket je pripojeny
    {
        if (BytesToWriteCount == BytesToWriteOffset) // nic neceka na poslani, muzeme posilat
        {
            if (BytesToWriteCount != 0)
                TRACE_E("Unexpected value of BytesToWriteCount.");

            int len = 0;
            if (!SSLConn)
            {
                while (1) // cyklus nutny kvuli funkci 'send' (neposila FD_WRITE pri 'sentLen' < 'bytesToWrite')
                {
                    // POZOR: pokud se nekdy zase bude zavadet TELNET protokol, je nutne predelat posilani IAC+IP
                    // pred abortovanim prikazu v metode SendFTPCommand()

                    if (!SSLConn)
                    {
                        int sentLen = send(Socket, buffer + len, bytesToWrite - len, 0);
                        if (sentLen != SOCKET_ERROR) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                        {
                            len += sentLen;
                            if (len >= bytesToWrite) // uz je poslano vsechno?
                            {
                                ret = TRUE;
                                break; // prestaneme posilat (jiz neni co)
                            }
                        }
                        else
                        {
                            DWORD err = WSAGetLastError();
                            if (err == WSAEWOULDBLOCK) // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                            {
                                ret = TRUE;
                                break; // prestaneme posilat (dodela se po FD_WRITE)
                            }
                            else // chyba posilani
                            {
                                if (error != NULL)
                                    *error = err;
                                break; // vratime chybu
                            }
                        }
                    }
                    else
                    {
                        int sentLen = SSLLib.SSL_write(SSLConn, buffer + len, bytesToWrite - len);
                        if (sentLen >= 0) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                        {
                            len += sentLen;
                            if (len >= bytesToWrite) // uz je poslano vsechno?
                            {
                                ret = TRUE;
                                break; // prestaneme posilat (jiz neni co)
                            }
                        }
                        else
                        {
                            DWORD err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                            if (err == WSAEWOULDBLOCK) // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                            {
                                ret = TRUE;
                                break; // prestaneme posilat (dodela se po FD_WRITE)
                            }
                            else // chyba posilani
                            {
                                if (error != NULL)
                                    *error = err;
                                break; // vratime chybu
                            }
                        }
                    }
                }
            }
            else
            {
                while (1) // cyklus nutny kvuli funkci 'send' (neposila FD_WRITE pri 'sentLen' < 'bytesToWrite')
                {
                    // POZOR: pokud se nekdy zase bude zavadet TELNET protokol, je nutne predelat posilani IAC+IP
                    // pred abortovanim prikazu v metode SendFTPCommand()

                    int sentLen = SSLLib.SSL_write(SSLConn, buffer + len, bytesToWrite - len);
                    if (sentLen > 0) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                    {
                        len += sentLen;
                        if (len >= bytesToWrite) // uz je poslano vsechno?
                        {
                            ret = TRUE;
                            break; // prestaneme posilat (jiz neni co)
                        }
                    }
                    else
                    {
                        DWORD err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                        if (err == WSAEWOULDBLOCK) // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                        {
                            ret = TRUE;
                            break; // prestaneme posilat (dodela se po FD_WRITE)
                        }
                        else // chyba posilani
                        {
                            if (error != NULL)
                                *error = err;
                            break; // vratime chybu
                        }
                    }
                }
            }

            if (ret) // uspesne odeslani, v 'len' je pocet poslanych bytu (zbytek posleme po prijeti FD_WRITE)
            {
                if (allBytesWritten != NULL)
                    *allBytesWritten = (len >= bytesToWrite);
                if (len < bytesToWrite) // zbytek vlozime do bufferu 'BytesToWrite'
                {
                    const char* buf = buffer + len;
                    int size = bytesToWrite - len;

                    if (BytesToWriteAllocatedSize - BytesToWriteCount < size) // malo mista v bufferu 'BytesToWrite'
                    {
                        int newSize = BytesToWriteCount + size + FTPWORKER_BYTESTOWRITEONSOCKETPREALLOC;
                        char* newBuf = (char*)realloc(BytesToWrite, newSize);
                        if (newBuf != NULL)
                        {
                            BytesToWrite = newBuf;
                            BytesToWriteAllocatedSize = newSize;
                        }
                        else // nedostatek pameti pro ulozeni dat v nasem bufferu (chybu hlasi jen TRACE)
                        {
                            TRACE_E(LOW_MEMORY);
                            ret = FALSE;
                        }
                    }

                    if (ret) // muzeme zapsat (v bufferu je dost mista)
                    {
                        memcpy(BytesToWrite + BytesToWriteCount, buf, size);
                        BytesToWriteCount += size;
                    }
                }
            }
        }
        else // jeste nebylo odeslano vse -> chybne pouziti Write
        {
            TRACE_E("Incorrect use of CFTPWorker::Write(): called again before waiting for fwseWriteDone event.");
        }
    }
    else
        TRACE_I("CFTPWorker::Write(): Socket is already closed.");

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return ret;
}

BOOL CFTPWorker::ReadFTPReply(char** reply, int* replySize, int* replyCode)
{
    CALL_STACK_MESSAGE1("CFTPWorker::ReadFTPReply(, ,)");

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CFTPWorker::ReadFTPReply: not from section SocketCritSect!");
#endif

    return FTPReadFTPReply(ReadBytes, ReadBytesCount, ReadBytesOffset, reply, replySize, replyCode);
}

void CFTPWorker::SkipFTPReply(int replySize)
{
    CALL_STACK_MESSAGE2("CFTPWorker::SkipFTPReply(%d)", replySize);

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CFTPWorker::SkipFTPReply: not from section SocketCritSect!");
#endif

    ReadBytesOffset += replySize;
    if (ReadBytesOffset >= ReadBytesCount) // uz jsme precetli vse - resetneme buffer
    {
        if (ReadBytesOffset > ReadBytesCount)
            TRACE_E("Error in call to CFTPWorker::SkipFTPReply(): trying to skip more bytes than is read");
        ReadBytesOffset = 0;
        ReadBytesCount = 0;
    }
}

void CFTPWorker::ReceiveHostByAddress(DWORD ip, int hostUID, int err)
{
    CALL_STACK_MESSAGE1("CFTPWorker::ReceiveHostByAddress(, ,)");

    if (hostUID == IPRequestUID)
        HandleSocketEvent(fwseIPReceived, ip, err);
}

void CFTPWorker::ReceiveNetEvent(LPARAM lParam, int index)
{
    CALL_STACK_MESSAGE3("CFTPWorker::ReceiveNetEvent(0x%IX, %d)", lParam, index);
    DWORD eventError = WSAGETSELECTERROR(lParam); // extract error code of event
    switch (WSAGETSELECTEVENT(lParam))            // extract event
    {
    case FD_CLOSE: // nekdy chodi pred poslednim FD_READ, nezbyva tedy nez napred zkusit FD_READ a pokud uspeje, poslat si FD_CLOSE znovu (muze pred nim znovu uspet FD_READ)
    case FD_READ:
    {
        BOOL sendFDCloseAgain = FALSE; // TRUE = prisel FD_CLOSE + bylo co cist (provedl se jako FD_READ) => posleme si znovu FD_CLOSE (soucasny FD_CLOSE byl plany poplach)
        HANDLES(EnterCriticalSection(&SocketCritSect));

        if (!EventConnectSent) // pokud prisla FD_READ pred FD_CONNECT, posleme fwseConnect jeste pred ctenim
        {
            EventConnectSent = TRUE;
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            HandleSocketEvent(fwseConnect, eventError, 0); // posleme si udalost s vysledkem pripojeni
            HANDLES(EnterCriticalSection(&SocketCritSect));
        }

        BOOL ret = FALSE;
        DWORD err = NO_ERROR;
        BOOL genEvent = FALSE;
        if (eventError == NO_ERROR)
        {
            if (Socket != INVALID_SOCKET) // socket je pripojeny
            {
                BOOL lowMem = FALSE;
                if (ReadBytesAllocatedSize - ReadBytesCount < FTPWORKER_BYTESTOREADONSOCKET) // maly buffer 'ReadBytes'
                {
                    if (ReadBytesOffset > 0) // je mozny sesun dat v bufferu?
                    {
                        memmove(ReadBytes, ReadBytes + ReadBytesOffset, ReadBytesCount - ReadBytesOffset);
                        ReadBytesCount -= ReadBytesOffset;
                        ReadBytesOffset = 0;
                    }

                    if (ReadBytesAllocatedSize - ReadBytesCount < FTPWORKER_BYTESTOREADONSOCKET) // stale maly buffer 'ReadBytes'
                    {
                        int newSize = ReadBytesCount + FTPWORKER_BYTESTOREADONSOCKET +
                                      FTPWORKER_BYTESTOREADONSOCKETPREALLOC;
                        char* newBuf = (char*)realloc(ReadBytes, newSize);
                        if (newBuf != NULL)
                        {
                            ReadBytes = newBuf;
                            ReadBytesAllocatedSize = newSize;
                        }
                        else // nedostatek pameti pro ulozeni dat v nasem bufferu (chybu hlasi jen TRACE)
                        {
                            TRACE_E(LOW_MEMORY);
                            lowMem = TRUE;
                        }
                    }
                }

                if (!lowMem)
                { // precteme co nejvice bytu do bufferu, necteme cyklicky, aby se data zpracovavala postupne
                    // (staci mensi buffery); je-li jeste neco ke cteni, dostaneme znovu FD_READ
                    if (!SSLConn)
                    {
                        int len = recv(Socket, ReadBytes + ReadBytesCount, ReadBytesAllocatedSize - ReadBytesCount, 0);
                        if (len != SOCKET_ERROR) // mozna jsme neco precetli (0 = spojeni uz je zavrene)
                        {
                            if (len > 0)
                            {
                                ReadBytesCount += len; // upravime pocet jiz nactenych bytu o nove nactene
                                ret = TRUE;
                                genEvent = TRUE;
                                if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                    sendFDCloseAgain = TRUE;
                            }
                        }
                        else
                        {
                            err = WSAGetLastError();
                            if (err != WSAEWOULDBLOCK)
                                genEvent = TRUE; // budeme generovat udalost s chybou
                        }
                    }
                    else
                    {
                        if (SSLLib.SSL_pending(SSLConn) > 0) // je-li neprazdny interni SSL buffer nedojde vubec k volani recv() a tudiz neprijde dalsi FD_READ, tedy musime si ho poslat sami, jinak se prenos dat zastavi
                            PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_READ);
                        int len = SSLLib.SSL_read(SSLConn, ReadBytes + ReadBytesCount, ReadBytesAllocatedSize - ReadBytesCount);
                        if (len >= 0) // mozna jsme neco precetli (0 = spojeni uz je zavrene)
                        {
                            if (len > 0)
                            {
                                ReadBytesCount += len; // upravime pocet jiz nactenych bytu o nove nactene
                                ret = TRUE;
                                genEvent = TRUE;
                                if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                    sendFDCloseAgain = TRUE;
                            }
                        }
                        else
                        {
                            err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, len));
                            if (err != WSAEWOULDBLOCK)
                                genEvent = TRUE; // budeme generovat udalost s chybou
                        }
                    }
                }
            }
            else
            {
                // muze nastat: hlavni nebo operation-dialog thread stihne zavolat CloseSocket() pred dorucenim FD_READ
                // TRACE_E("Unexpected situation in CFTPWorker::ReceiveNetEvent(FD_READ): Socket is not connected.");
                // udalost s touto necekanou chybou nebudeme generovat (reseni: user pouzije ESC)
            }
        }
        else // hlaseni chyby v FD_READ (podle helpu jen WSAENETDOWN)
        {
            if (WSAGETSELECTEVENT(lParam) != FD_CLOSE) // chybu si osetri FD_CLOSE sam
            {
                genEvent = TRUE;
                err = eventError;
            }
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (genEvent) // vygenerujeme udalost fwseNewBytesRead
        {
            HandleSocketEvent(fwseNewBytesRead, (!ret ? err : NO_ERROR), 0);
        }

        // ted zpracujeme FD_CLOSE
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
        {
            if (sendFDCloseAgain) // FD_CLOSE byl misto FD_READ => posleme FD_CLOSE znovu
            {
                PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                            (WPARAM)GetSocket(), lParam);
            }
            else // korektni FD_CLOSE
            {
                CSocket::ReceiveNetEvent(lParam, index); // zavolame metodu predka
            }
        }
        break;
    }

    case FD_WRITE:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));

        BOOL ret = FALSE;
        DWORD err = NO_ERROR;
        BOOL genEvent = FALSE;
        if (eventError == NO_ERROR)
        {
            if (BytesToWriteCount > BytesToWriteOffset) // mame nejaky restiky, budeme posilat zbyla data z bufferu 'BytesToWrite'
            {
                if (Socket != INVALID_SOCKET) // socket je pripojeny
                {
                    int len = 0;
                    if (!SSLConn)
                    {
                        while (1) // cyklus nutny kvuli funkci 'send' (neposila FD_WRITE pri 'sentLen' < 'bytesToWrite')
                        {
                            int sentLen = send(Socket, BytesToWrite + BytesToWriteOffset + len,
                                               BytesToWriteCount - BytesToWriteOffset - len, 0);
                            if (sentLen != SOCKET_ERROR) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                            {
                                len += sentLen;
                                if (len >= BytesToWriteCount - BytesToWriteOffset) // uz je poslano vsechno?
                                {
                                    ret = TRUE;
                                    break; // prestaneme posilat (jiz neni co)
                                }
                            }
                            else
                            {
                                err = WSAGetLastError();
                                if (err == WSAEWOULDBLOCK) // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                                {
                                    ret = TRUE;
                                    break; // prestaneme posilat (dodela se po FD_WRITE)
                                }
                                else // jina chyba - resetneme buffer
                                {
                                    BytesToWriteOffset = 0;
                                    BytesToWriteCount = 0;
                                    break; // vratime chybu
                                }
                            }
                        }
                    }
                    else
                    {
                        while (1) // cyklus nutny kvuli funkci 'send' (neposila FD_WRITE pri 'sentLen' < 'bytesToWrite')
                        {
                            int sentLen = SSLLib.SSL_write(SSLConn, BytesToWrite + BytesToWriteOffset + len,
                                                           BytesToWriteCount - BytesToWriteOffset - len);
                            if (sentLen >= 0) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                            {
                                len += sentLen;
                                if (len >= BytesToWriteCount - BytesToWriteOffset) // uz je poslano vsechno?
                                {
                                    ret = TRUE;
                                    break; // prestaneme posilat (jiz neni co)
                                }
                            }
                            else
                            {
                                err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                                if (err == WSAEWOULDBLOCK) // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                                {
                                    ret = TRUE;
                                    break; // prestaneme posilat (dodela se po FD_WRITE)
                                }
                                else // jina chyba - resetneme buffer
                                {
                                    BytesToWriteOffset = 0;
                                    BytesToWriteCount = 0;
                                    break; // vratime chybu
                                }
                            }
                        }
                    }

                    if (ret && len > 0) // aspon neco bylo odeslano -> zmena 'BytesToWriteOffset'
                    {
                        BytesToWriteOffset += len;
                        if (BytesToWriteOffset >= BytesToWriteCount) // vsechno poslano, reset bufferu
                        {
                            BytesToWriteOffset = 0;
                            BytesToWriteCount = 0;
                        }
                    }

                    genEvent = (!ret || BytesToWriteCount == BytesToWriteOffset); // chyba nebo se jiz podarilo poslat vse
                }
                else
                {
                    // muze nastat: hlavni nebo operation-dialog thread stihne zavolat CloseSocket() pred dorucenim FD_WRITE
                    //TRACE_E("Unexpected situation in CFTPWorker::ReceiveNetEvent(FD_WRITE): Socket is not connected.");
                    BytesToWriteCount = 0; // chyba -> resetneme buffer
                    BytesToWriteOffset = 0;
                    // udalost s touto necekanou chybou nebudeme generovat (reseni: user pouzije ESC)
                }
            }
        }
        else // hlaseni chyby v FD_WRITE (podle helpu jen WSAENETDOWN)
        {
            genEvent = TRUE;
            err = eventError;
            BytesToWriteCount = 0; // chyba -> resetneme buffer
            BytesToWriteOffset = 0;
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (genEvent) // vygenerujeme udalost fwseWriteDone
        {
            HandleSocketEvent(fwseWriteDone, (!ret ? err : NO_ERROR), 0);
        }
        break;
    }

    case FD_CONNECT:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        BOOL call = FALSE;
        if (!EventConnectSent)
        {
            EventConnectSent = TRUE;
            call = TRUE;
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        if (call)
            HandleSocketEvent(fwseConnect, eventError, 0); // posleme si udalost s vysledkem pripojeni
        break;
    }
    }
}

void CFTPWorker::ReceiveTimer(DWORD id, void* param)
{
    CALL_STACK_MESSAGE2("CFTPWorker::ReceiveTimer(%u,)", id);

    switch (id)
    {
    case WORKER_TIMEOUTTIMERID:
        HandleSocketEvent(fwseTimeout, 0, 0);
        break;
    case WORKER_CMDERRORTIMERID:
        HandleSocketEvent(fwseWaitForCmdErr, 0, 0);
        break;

    case WORKER_CONTIMEOUTTIMID:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        HandleEvent(fweConTimeout, NULL, 0, 0);
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }

    case WORKER_RECONTIMEOUTTIMID:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        HandleEvent(fweReconTimeout, NULL, 0, 0);
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }

    case WORKER_STATUSUPDATETIMID:
    { // postarame se o pravidelny update statusu v dialogu operace + pripadne o zruseni tohoto pravidelneho updatu v pripade, ze uz neni potreba
        HANDLES(EnterCriticalSection(&SocketCritSect));
        HANDLES(EnterCriticalSection(&WorkerCritSect));
        BOOL clearStatusType = TRUE;
        if (State == fwsWorking)
        {
            DWORD statusConnectionIdleTime;
            DWORD statusSpeed;
            CQuadWord statusTransferred;
            CQuadWord statusTotal;
            BOOL writeNewStatus = FALSE;
            if (StatusType == wstDownloadStatus && WorkerDataCon != NULL)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                if (WorkerDataCon->IsTransfering(NULL)) // prenos dat jeste probiha
                {
                    // ziskame status informace z data-connectiony
                    WorkerDataCon->GetStatus(&statusTransferred, &statusTotal, &statusConnectionIdleTime, &statusSpeed);
                    writeNewStatus = TRUE;
                }
                HANDLES(EnterCriticalSection(&WorkerCritSect));
            }
            else
            {
                if (StatusType == wstUploadStatus && WorkerUploadDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // ziskame status informace z data-connectiony
                    WorkerUploadDataCon->GetStatus(&statusTransferred, &statusTotal, &statusConnectionIdleTime, &statusSpeed);
                    writeNewStatus = TRUE;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
            }
            if (writeNewStatus)
            {
                StatusConnectionIdleTime = statusConnectionIdleTime;
                StatusSpeed = statusSpeed;
                StatusTransferred = statusTransferred;
                StatusTotal = statusTotal;

                // pozadame o redraw workera (zobrazeni prave ziskaneho statusu)
                Oper->ReportWorkerChange(ID, TRUE);

                // vyzadame si dalsi cyklus updatu statusu
                clearStatusType = FALSE;

                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                SocketsThread->AddTimer(Msg, UID, GetTickCount() + WORKER_STATUSUPDATETIMEOUT,
                                        WORKER_STATUSUPDATETIMID, NULL); // chybu ignorujeme, max. se nebude updatit status
            }
        }
        if (clearStatusType)
        {
            StatusType = wstNone;
            LastTimeEstimation = -1;
        }
        HANDLES(LeaveCriticalSection(&WorkerCritSect));
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }

    case WORKER_DATACONSTARTTIMID:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        HandleEvent(fweDataConStartTimeout, NULL, 0, 0);
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }

    case WORKER_DELAYEDAUTORETRYTIMID:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        HandleEvent(fweDelayedAutoRetry, NULL, 0, 0);
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }

    case WORKER_LISTENTIMEOUTTIMID:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        HandleEvent(fweDataConListenTimeout, NULL, 0, 0);
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }
    }
}

void CFTPWorker::ReceivePostMessage(DWORD id, void* param)
{
    SLOW_CALL_STACK_MESSAGE2("CFTPWorker::ReceivePostMessage(%u,)", id);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    switch (id)
    {
    case WORKER_ACTIVATE:
        HandleEvent(fweActivate, NULL, 0, 0);
        break;
    case WORKER_SHOULDSTOP:
        HandleEvent(fweWorkerShouldStop, NULL, 0, 0);
        break;
    case WORKER_SHOULDPAUSE:
        HandleEvent(fweWorkerShouldPause, NULL, 0, 0);
        break;
    case WORKER_SHOULDRESUME:
        HandleEvent(fweWorkerShouldResume, NULL, 0, 0);
        break;
    case WORKER_NEWLOGINPARAMS:
        HandleEvent(fweNewLoginParams, NULL, 0, 0);
        break;

    case WORKER_WAKEUP:
    {
        HandleEvent(fweWakeUp, NULL, 0, 0);
        HANDLES(EnterCriticalSection(&WorkerCritSect));
        ReceivingWakeup = FALSE;
        HANDLES(LeaveCriticalSection(&WorkerCritSect));
        break;
    }

    case WORKER_DISKWORKFINISHED:
        HandleEvent(fweDiskWorkFinished, NULL, 0, 0);
        break;
    case WORKER_DISKWORKWRITEFINISHED:
        HandleEvent(fweDiskWorkWriteFinished, NULL, 0, 0);
        break;
    case WORKER_DISKWORKLISTFINISHED:
        HandleEvent(fweDiskWorkListFinished, NULL, 0, 0);
        break;
    case WORKER_DISKWORKREADFINISHED:
        HandleEvent(fweDiskWorkReadFinished, NULL, 0, 0);
        break;
    case WORKER_DISKWORKDELFILEFINISHED:
        HandleEvent(fweDiskWorkDelFileFinished, NULL, 0, 0);
        break;

    case WORKER_DATACON_CONNECTED:
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        if (WorkerDataCon != NULL && (int)(INT_PTR)param == WorkerDataCon->GetUID()) // prisla zprava od aktualni data-connectiony (jine rovnou zahazujeme)
            HandleEvent(fweDataConConnectedToServer, NULL, 0, 0);
        //      else   // muze prijit az po zavreni WorkerDataCon (pokud prijde odpoved na LIST hned a je take hned zavrena data-connectina - kratky listing)
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_DATACON_CONNECTED for nonexistent data-connection!");
        break;
    }

    case WORKER_DATACON_CLOSED:
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        if (WorkerDataCon != NULL && (int)(INT_PTR)param == WorkerDataCon->GetUID()) // prisla zprava od aktualni data-connectiony (jine rovnou zahazujeme)
            HandleEvent(fweDataConConnectionClosed, NULL, 0, 0);
        //      else   // muze prijit az po zavreni WorkerDataCon (pokud prijde odpoved na LIST hned a je take hned zavrena data-connectina - kratky listing)
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_DATACON_CLOSED for nonexistent data-connection!");
        break;
    }

    case WORKER_DATACON_FLUSHDATA:
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        if (WorkerDataCon != NULL && (int)(INT_PTR)param == WorkerDataCon->GetUID()) // prisla zprava od aktualni data-connectiony (jine rovnou zahazujeme)
            HandleEvent(fweDataConFlushData, NULL, 0, 0);
        //      else   // muze prijit az po zavreni WorkerDataCon
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_DATACON_FLUSHDATA for nonexistent data-connection!");
        break;
    }

    case WORKER_DATACON_LISTENINGFORCON:
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        if (WorkerDataCon != NULL && (int)(INT_PTR)param == WorkerDataCon->GetUID()) // prisla zprava od aktualni data-connectiony (jine rovnou zahazujeme)
            HandleEvent(fweDataConListeningForCon, NULL, 0, 0);
        else
            TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_DATACON_LISTENINGFORCON for nonexistent data-connection!");
        break;
    }

    case WORKER_UPLDATACON_LISTENINGFORCON:
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        if (WorkerUploadDataCon != NULL && (int)(INT_PTR)param == WorkerUploadDataCon->GetUID()) // prisla zprava od aktualni data-connectiony (jine rovnou zahazujeme)
            HandleEvent(fweUplDataConListeningForCon, NULL, 0, 0);
        else
            TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_UPLDATACON_LISTENINGFORCON for nonexistent data-connection!");
        break;
    }

    case WORKER_UPLDATACON_CONNECTED:
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        if (WorkerUploadDataCon != NULL && (int)(INT_PTR)param == WorkerUploadDataCon->GetUID()) // prisla zprava od aktualni upload data-connectiony (jine rovnou zahazujeme)
            HandleEvent(fweUplDataConConnectedToServer, NULL, 0, 0);
        //      else  // muze prijit az po zavreni WorkerUploadDataCon
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_UPLDATACON_CONNECTED for nonexistent data-connection!");
        break;
    }

    case WORKER_UPLDATACON_CLOSED:
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        if (WorkerUploadDataCon != NULL && (int)(INT_PTR)param == WorkerUploadDataCon->GetUID()) // prisla zprava od aktualni upload data-connectiony (jine rovnou zahazujeme)
            HandleEvent(fweUplDataConConnectionClosed, NULL, 0, 0);
        //      else  // muze prijit az po zavreni WorkerUploadDataCon
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_UPLDATACON_CLOSED for nonexistent data-connection!");
        break;
    }

    case WORKER_UPLDATACON_PREPAREDATA:
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        if (WorkerUploadDataCon != NULL && (int)(INT_PTR)param == WorkerUploadDataCon->GetUID()) // prisla zprava od aktualni upload data-connectiony (jine rovnou zahazujeme)
            HandleEvent(fweUplDataConPrepareData, NULL, 0, 0);
        //      else  // muze prijit az po zavreni WorkerUploadDataCon
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_UPLDATACON_PREPAREDATA for nonexistent data-connection!");
        break;
    }

    case WORKER_TGTPATHLISTINGFINISHED:
        HandleEvent(fweTgtPathListingFinished, NULL, 0, 0);
        break;
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CFTPWorker::ReadFTPErrorReplies()
{
    CALL_STACK_MESSAGE1("CFTPWorker::ReadFTPErrorReplies(, ,)");

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CFTPWorker::ReadFTPErrorReplies: not from section SocketCritSect!");
    if (WorkerCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CFTPWorker::ReadFTPErrorReplies: not from section WorkerCritSect!");
#endif

    char* reply;
    int replySize;
    int replyCode;
    while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
    {                                                    // vypiseme pripadne error hlasky ze serveru do logu
        Logs.LogMessage(LogUID, reply, replySize, TRUE);
        if (ErrorDescr[0] == 0 &&                               // zatim nemame popis chyby
            (replyCode == -1 ||                                 // neni FTP odpoved
             FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // popis docasne chyby
             FTP_DIGIT_1(replyCode) == FTP_D1_ERROR))           // popis chyby
        {
            CopyStr(ErrorDescr, FTPWORKER_ERRDESCR_BUFSIZE, reply, replySize);
        }
        SkipFTPReply(replySize);
    }
}

void CFTPWorker::HandleSocketEvent(CFTPWorkerSocketEvent event, DWORD data1, DWORD data2)
{
    CALL_STACK_MESSAGE2("CFTPWorker::HandleSocketEvent(%d, ,)", (int)event);

    char errBuf[300];
    char errText[200];

    if (event == fwseIPReceived) // ulozime IP adresu do operace a vyvolame HandleEvent
    {
        CFTPWorkerEvent resEvent = fweIPReceived;
        if (data1 != INADDR_NONE)
            Oper->SetServerIP(data1); // ulozime IP adresu, vsichni workeri si ji vyzvedavaji z operace
        else                          // chyba, ulozime ji do ErrorDescr pro dalsi pouziti
        {
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_WORKERGETIPERROR),
                        GetWorkerErrorTxt(data2, errBuf, 300));
            CorrectErrorDescr();
            resEvent = fweIPRecFailure;
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
        }
        HANDLES(EnterCriticalSection(&SocketCritSect));
        HandleEvent(resEvent, NULL, 0, 0);
        HANDLES(LeaveCriticalSection(&SocketCritSect));
    }
    else
    {
        if (event == fwseConnect) // prijmeme vysledek pokusu o navazani spojeni
        {
            CFTPWorkerEvent resEvent = fweConnected;
            if (data1 != NO_ERROR) // chyba, ulozime ji do ErrorDescr pro dalsi pouziti
            {
                if (!GetProxyError(errBuf, 300, NULL, 0, TRUE))
                    GetWorkerErrorTxt(data1, errBuf, 300);
                HANDLES(EnterCriticalSection(&WorkerCritSect));
                _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_WORKEROPENCONERR), errBuf);
                CorrectErrorDescr();
                resEvent = fweConnectFailure;
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
            }
            HANDLES(EnterCriticalSection(&SocketCritSect));
            HandleEvent(resEvent, NULL, 0, 0);
            HANDLES(LeaveCriticalSection(&SocketCritSect));
        }
        else // zpracovani ostatnich zprav
        {
            HANDLES(EnterCriticalSection(&SocketCritSect));
            int uid = UID;
            HANDLES(EnterCriticalSection(&WorkerCritSect));

            BOOL leaveSect = TRUE;
            BOOL handleClose = FALSE;
            BOOL isTimeout = FALSE;
            BOOL deleteTimerShowErr = FALSE;
            BOOL deleteTimerTimeout = FALSE;
            switch (CommandState)
            {
            case fwcsIdle: // neocekavana odpoved, bud error ("timeout, closing connection", atp.) nebo interni chyba serveru (zdvojena odpoved, atp.)
            {
                switch (event)
                {
                case fwseNewBytesRead: // pokud je to error, chceme znat jeho text
                {
                    ReadFTPErrorReplies();
                    break;
                }

                case fwseClose:
                {
                    if (data1 != NO_ERROR) // jen pokud mame nejakou chybu
                    {
                        FTPGetErrorTextForLog(data1, errBuf, 300);
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);
                    }
                    if (ErrorDescr[0] == 0) // pri zavreni spojeni musime naplnit ErrorDescr (i kdyby hlaskou "unknown error")
                    {
                        lstrcpyn(ErrorDescr, GetWorkerErrorTxt(data1, errBuf, 300), FTPWORKER_ERRDESCR_BUFSIZE);
                        CorrectErrorDescr();
                    }
                    break;
                }
                }
                break;
            }

            case fwcsWaitForCmdReply:
            case fwcsWaitForLoginPrompt:
            {
                switch (event)
                {
                case fwseNewBytesRead:
                case fwseWriteDone:
                {
                    char* reply;
                    int replySize;
                    int replyCode;
                    BOOL firstRound = TRUE;
                    while (BytesToWriteOffset == BytesToWriteCount &&    // test "write done" (odpoved serveru cekame az po odeslani kompletniho prikazu)
                           ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru (v jednom cteni jich muze byt vic najednou)
                    {
                        if (!firstRound)
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                        firstRound = FALSE;
                        Logs.LogMessage(LogUID, reply, replySize, CommandState == fwcsIdle); // odpoved na prikaz dame do logu (cas se vypisuje u "unexpected replies")
                        if (FTP_DIGIT_1(replyCode) != FTP_D1_MAYBESUCCESS)                   // odpoved na prikaz
                        {                                                                    // timto je prikaz dokoncen
                            CommandState = fwcsIdle;                                         // v HandleEvent() se nejspis posle dalsi prikaz, musime prejit do "idle" jiz nyni
                            CommandTransfersData = FALSE;
                        }
                        ErrorDescr[0] = 0; // opatreni proti zdvojenym hlaskam (prichod ocekavane odpovedi = connectina je OK, takze text domnele chyby zlikvidujeme)
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        if (FTP_DIGIT_1(replyCode) != FTP_D1_MAYBESUCCESS) // odpoved na prikaz
                        {
                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                            SocketsThread->DeleteTimer(uid, WORKER_TIMEOUTTIMERID); // v HandleEvent() se nejspis posle dalsi prikaz, timer musime smazat "predem"

                            HandleEvent(fweCmdReplyReceived, reply, replySize, replyCode);
                        }
                        else // server posila informace (odpovedi typu 1xx)
                        {
                            HandleEvent(fweCmdInfoReceived, reply, replySize, replyCode);
                        }
                        SkipFTPReply(replySize); // odpoved serveru vyhodime z bufferu (uz je zpracovana)
                    }
                    if (firstRound)
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    leaveSect = FALSE;
                    break;
                }

                case fwseClose:
                {
                    if (data1 != NO_ERROR) // jen pokud mame nejakou chybu
                    {
                        FTPGetErrorTextForLog(data1, errBuf, 300);
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);
                    }
                    deleteTimerTimeout = TRUE;
                    handleClose = TRUE;
                    lstrcpyn(ErrorDescr, LoadStr(IDS_CONNECTIONLOSTERROR), FTPWORKER_ERRDESCR_BUFSIZE);
                    CorrectErrorDescr();
                    break;
                }

                case fwseTimeout: // timeout, zavreme connectionu a ohlasime zavreni
                {
                    if (CommandTransfersData && (WorkerDataCon != NULL || WorkerUploadDataCon != NULL))
                    {
                        // nastavime novy timeout timer
                        int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                        if (serverTimeout < 1000)
                            serverTimeout = 1000; // aspon sekundu

                        BOOL trFinished;
                        DWORD start;
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        BOOL isTransfering = WorkerDataCon != NULL ? WorkerDataCon->IsTransfering(&trFinished) : WorkerUploadDataCon->IsTransfering(&trFinished);
                        if (isTransfering)
                        { // cekame na data, takze to neni timeout
                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                            SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                                    WORKER_TIMEOUTTIMERID, NULL); // chybu ignorujeme, maximalne si user da Stop
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                            break;
                        }
                        else
                        {
                            if (trFinished)
                            {
                                // timeout se meri od zavreni connectiony (okamzik odkdy muze server reagovat - take se dozvi
                                // o zavreni connectiony)
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                start = WorkerDataCon != NULL ? WorkerDataCon->GetSocketCloseTime() : WorkerUploadDataCon->GetSocketCloseTime();
                                if ((GetTickCount() - start) < (DWORD)serverTimeout) // od zavreni connectiony jeste neubehnul timeout
                                {
                                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                    SocketsThread->AddTimer(Msg, UID, start + serverTimeout,
                                                            WORKER_TIMEOUTTIMERID, NULL); // chybu ignorujeme, maximalne si user da Stop
                                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                                    break;
                                }
                                // else ;  // od zavreni connectiony uz ubehnul timeout -> timeoutneme
                            }
                            // else ;  // spojeni se jeste neotevrelo -> timeoutneme

                            if (WorkerDataCon != NULL ? WorkerDataCon->GetProxyTimeoutDescr(errText, 200) : WorkerUploadDataCon->GetProxyTimeoutDescr(errText, 200))
                            { // pokud mame nejaky popis timeoutu na data-connectione, vypiseme ho do logu
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                sprintf(errBuf, LoadStr(IDS_LOGMSGDATCONERROR), errText);
                                Logs.LogMessage(LogUID, errBuf, -1, TRUE);
                            }
                            else
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                        }
                    }

                    Logs.LogMessage(LogUID, LoadStr(CommandState == fwcsWaitForLoginPrompt ? IDS_WORKERWAITLOGTIM : IDS_LOGMSGCMDTIMEOUT), -1, TRUE);

                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    leaveSect = FALSE;

                    ForceClose(); // "rucne" zavreme socket

                    handleClose = TRUE;
                    isTimeout = TRUE;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    lstrcpyn(ErrorDescr, LoadStr(CommandState == fwcsWaitForLoginPrompt ? IDS_WORKERWAITLOGTIM : IDS_LOGMSGCMDTIMEOUT),
                             FTPWORKER_ERRDESCR_BUFSIZE);
                    CorrectErrorDescr();
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    break;
                }
                }
                break;
            }

            case fwcsWaitForCmdError:
            {
                switch (event)
                {
                case fwseNewBytesRead:
                {
                    ReadFTPErrorReplies();
                    break;
                }

                case fwseClose:
                {
                    if (data1 != NO_ERROR) // jen pokud mame nejakou chybu
                    {
                        FTPGetErrorTextForLog(data1, errBuf, 300);
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);
                    }
                    deleteTimerShowErr = TRUE;
                    handleClose = TRUE;
                    if (ErrorDescr[0] == 0)
                    {
                        lstrcpyn(ErrorDescr, GetWorkerErrorTxt(data1, errBuf, 300), FTPWORKER_ERRDESCR_BUFSIZE);
                        CorrectErrorDescr();
                    }
                    break;
                }

                case fwseWaitForCmdErr: // timeout, zavreme connectionu "rucne"
                {
                    if (WaitForCmdErrError != NO_ERROR) // jen pokud mame nejakou chybu
                    {
                        FTPGetErrorTextForLog(WaitForCmdErrError, errBuf, 300);
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);
                    }

                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    leaveSect = FALSE;

                    ForceClose(); // "rucne" zavreme socket

                    handleClose = TRUE;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    if (ErrorDescr[0] == 0)
                    {
                        lstrcpyn(ErrorDescr, GetWorkerErrorTxt(WaitForCmdErrError, errBuf, 300),
                                 FTPWORKER_ERRDESCR_BUFSIZE);
                        CorrectErrorDescr();
                    }
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    break;
                }
                }
                break;
            }
            }

            if (leaveSect)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            }
            if (deleteTimerShowErr)
                SocketsThread->DeleteTimer(uid, WORKER_CMDERRORTIMERID);
            if (deleteTimerTimeout)
                SocketsThread->DeleteTimer(uid, WORKER_TIMEOUTTIMERID);
            if (handleClose)
            {
                HANDLES(EnterCriticalSection(&WorkerCritSect));
                CommandState = fwcsIdle;
                CommandTransfersData = FALSE;
                CommandReplyTimeout = isTimeout;
                WaitForCmdErrError = NO_ERROR;
                HANDLES(LeaveCriticalSection(&WorkerCritSect));

                HANDLES(EnterCriticalSection(&SocketCritSect));
                HandleEvent(fweCmdConClosed, NULL, 0, 0); // oznamime zavreni socketu do HandleEvent()
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            }
        }
    }
}
