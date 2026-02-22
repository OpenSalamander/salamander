// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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

    if (Config.EnableLogging && !Config.DisableLoggingOfWorkers) // without synchronization, not needed
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
    if (WorkerCritSect.RecursionCount == 0 /* does not catch the situation where
      another thread uses the section */
    )
        TRACE_E("Incorrect call to CFTPWorker::CorrectErrorDescr(): not from section WorkerCritSect!");
#endif

    // translate CR+LF to spaces
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
    // drop spaces and periods from the end of the string
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
    if (SocketCritSect.RecursionCount == 0 /* does not catch the situation where
      another thread uses the section */
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
        itemData->iImage = 0; // we only have a single icon for now
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
                    break; // no text in these cases

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
                                SalamanderGeneral->PrintDiskSize(num1, transferredSize, 0); // careful, num1 is used when creating num3
                                if (StatusTotal != CQuadWord(-1, -1))
                                {
                                    if (StatusType == wstUploadStatus && StatusTotal == transferredSize)
                                        num3[0] = 0; // nothing else will be uploaded
                                    int off = 0;
                                    SalamanderGeneral->PrintDiskSize(num2, StatusTotal, 0);
                                    off = _snprintf_s(bufRest, bufRestSize, _TRUNCATE, LoadStr(num3[0] != 0 ? IDS_LISTWNDSTATUS1 : IDS_OPERDLGSTATUS2),
                                                      num1, num2, num3);
                                    if (off < 0)
                                        off = bufRestSize;
                                    if (StatusTotal > CQuadWord(0, 0))
                                    {
                                        int progress = ((int)((CQuadWord(1000, 0) * transferredSize) / StatusTotal).Value /*+ 5*/) / 10; // do not round (100% must appear only at 100%, not at 99.5%)
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
                                                CQuadWord secs = waiting / CQuadWord(StatusSpeed, 0); // estimate of remaining seconds
                                                secs.Value++;                                         // add one more second so the operation ends with "time left: 1 sec" (instead of 0 sec)
                                                if (LastTimeEstimation != -1)
                                                    secs = (CQuadWord(2, 0) * secs + CQuadWord(LastTimeEstimation, 0)) / CQuadWord(3, 0);
                                                // rounding calculation (roughly 10% error + we round to nice numbers 1,2,5,10,20,40)
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
                                                lstrcpyn(timeLeftText, LoadStr(IDS_OPERDLGCOACT_TIMELEFT), 200);
                                                int len = (int)strlen(timeLeftText);
                                                if (len < 99) // total of 200, so if 100 characters must remain for the time value, len must be < 99
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
                            *(bufRest - 2) = 0; // the prefix does not need ": "
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
            State = fwsConnectionError; // so the worker does not start another reconnect while the user resolves the problem
            operStatusMaybeChanged = TRUE;
            // ErrorOccurenceTime is not set here - this is not an error that happened on its own - the user forced it
            SubState = fwssNone;
            *postActivate = TRUE; // fweActivate is posted after the method finishes; in fwsConnectionError the item must return to the queue
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
    ReturnToControlCon = FALSE; // we already asked from CReturningConnections methods
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

BOOL CFTPWorker::CanDeleteFromDelWorkers()
{
    CALL_STACK_MESSAGE1("CFTPWorker::CanDeleteFromDelWorkers()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = !ReturnToControlCon;
    CanDeleteSocket = TRUE; // we already asked from DeleteWorkers
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    return ret;
}

BOOL CFTPWorker::InformAboutStop()
{
    CALL_STACK_MESSAGE1("CFTPWorker::InformAboutStop()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    BOOL ret = FALSE;
    if (!ShouldStop) // if this is not a repeated call
    {
        ShouldStop = TRUE;
        Oper->ReportWorkerChange(ID, FALSE);
        if (!SocketClosed && (State == fwsSleeping || DiskWorkIsUsed ||
                              State == fwsWorking && (SubState == fwssWorkUploadWaitForListing ||
                                                      SubState == fwssWorkExplWaitForListen ||
                                                      SubState == fwssWorkCopyWaitForListen ||
                                                      SubState == fwssWorkUploadWaitForListen) ||
                              State == fwsLookingForWork && ShouldBePaused))
        { // "idle" state with an open connection -> we need to post WORKER_SHOULDSTOP
            ret = TRUE;
        }
        else
        { // if a "data-connection" exists and we are not waiting for the server connection, we need to close the "data-connection"
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
    // if a "data-connection" exists and we are not waiting for the server connection, we need to close the "data-connection"
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
        if (workerDataCon->IsConnected()) // close the "data connection"; the system will attempt a "graceful" shutdown
        {
            workerDataCon->CloseSocketEx(NULL); // shutdown (we do not learn the result)
            postShouldStop = TRUE;              // we must "get the worker moving" so it terminates (WORKER_DATACON_CLOSED will not arrive)
        }
        workerDataCon->FreeFlushData();
        DeleteSocket(workerDataCon);
    }
    if (workerUploadDataCon != NULL)
    {
        if (workerUploadDataCon->IsConnected()) // close the "data connection"; the system will attempt a "graceful" shutdown
        {
            workerUploadDataCon->CloseSocketEx(NULL); // shutdown (we do not learn the result)
            postShouldStop = TRUE;                    // we must "get the worker moving" so it terminates (WORKER_UPLDATACON_CLOSED will not arrive)
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
    if (ShouldBePaused != pause) // if this is not a redundant or repeated call
    {
        Logs.LogMessage(LogUID, LoadStr(pause ? IDS_LOGMSGPAUSE : IDS_LOGMSGRESUME), -1, TRUE);
        ShouldBePaused = pause;
        Oper->ReportWorkerChange(ID, FALSE);
        ret = TRUE; // we will call PostShouldPauseOrResume()
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
            if (WorkerDataCon->IsTransfering(NULL)) // data transfer is still in progress
            {                                       // obtain status information from the data connection
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
                // obtain status information from the data connection
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
    BOOL ret = SocketClosed && WorkerDataConState == wdcsDoesNotExist; // cannot use IsConnected(); must not enter the CSocket::SocketCritSect section
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
        if (workerDataCon->IsConnected())       // close the "data connection"; the system will attempt a "graceful" shutdown
            workerDataCon->CloseSocketEx(NULL); // shutdown (we do not learn the result)
        workerDataCon->FreeFlushData();
        DeleteSocket(workerDataCon);
    }
    if (workerUploadDataCon != NULL)
    {
        if (workerUploadDataCon->IsConnected())       // close the "data connection"; the system will attempt a "graceful" shutdown
            workerUploadDataCon->CloseSocketEx(NULL); // shutdown (we do not learn the result)
        workerUploadDataCon->FreeBufferedData();
        DeleteSocket(workerUploadDataCon);
    }

    HANDLES(EnterCriticalSection(&SocketCritSect)); // optimization: ensure IsConnected and CloseSocket run together
    if (IsConnected())
        CloseSocket(NULL); // close the socket
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    // simulate the server closing the socket (for safety always do it, not only with an open socket)
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    SocketClosed = TRUE;
    int logUID = LogUID; // worker's log UID
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
    Logs.SetIsConnected(logUID, IsConnected());
    Logs.RefreshListOfLogsInLogsDlg(); // show the "connection inactive" notification
    ReportWorkerMayBeClosed();         // announce the socket closure (for other waiting threads)
}

void CFTPWorker::ForceCloseDiskWork()
{
    CALL_STACK_MESSAGE1("CFTPWorker::ForceCloseDiskWork()");
    // WARNING: may be called repeatedly (see calls from CFTPWorkersList::ForceCloseWorkers())
    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (DiskWorkIsUsed)
    {
        BOOL workIsInProgress;
        if (FTPDiskThread->CancelWork(&DiskWork, &workIsInProgress))
        {
            if (workIsInProgress)
                DiskWork.FlushDataBuffer = NULL; // work is in progress; we cannot free the buffer with data being written/verified (or for read data), leave it to the disk-work thread (see the cancellation section) - we may write into DiskWork because after Cancel the disk thread must no longer access it (for example, it might no longer exist)
            else
            { // the work was cancelled before the disk thread started executing it - deallocate the flush buffer (for both download and upload)
                if (DiskWork.FlushDataBuffer != NULL)
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }
            }
            DiskWorkIsUsed = FALSE; // if the work is already finished, wait until the worker terminates on its own (otherwise interrupt the disk work)
        }
    }
    HANDLES(LeaveCriticalSection(&WorkerCritSect));
}

void CFTPWorker::ReleaseData(CUploadWaitingWorker** uploadFirstWaitingWorker)
{
    CALL_STACK_MESSAGE1("CFTPWorker::ReleaseData()");

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (CommandState == fwcsWaitForCmdError) // the worker was terminated before the WORKER_CMDERRORTIMERID timer fired, so we must print the error message here
    {
        if (WaitForCmdErrError != NO_ERROR) // if there is something to show, display it
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
        { // listing failed; inform waiting workers about it
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
                    // if we do not know how deleting the file/link/directory ended, invalidate the listing in the cache
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
                        // if we do not know how creating the directory ended, invalidate the listing in the cache
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
                        // if we do not know how creating the directory ended, invalidate the listing in the cache
                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                        CFTPQueueItemCopyMoveUploadExplore* curItem = (CFTPQueueItemCopyMoveUploadExplore*)CurItem;
                        UploadListingCache.ReportCreateDirs(userBuf, hostBuf, port, curItem->TgtPath,
                                                            Oper->GetFTPServerPathType(curItem->TgtPath),
                                                            UploadAutorenameNewName, TRUE);
                    }
                    break;
                }

                case fwssWorkUploadActivateDataCon: // intermediate state between fwssWorkUploadSendSTORCmd and fwssWorkUploadWaitForSTORRes
                case fwssWorkUploadWaitForSTORRes:
                {
                    if (CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile) // "always true"
                    {
                        // STOR command result is unknown; invalidate the listing
                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                        CFTPQueueItemCopyOrMoveUpload* curItem = (CFTPQueueItemCopyOrMoveUpload*)CurItem;
                        UploadListingCache.ReportFileUploaded(userBuf, hostBuf, port, curItem->TgtPath,
                                                              Oper->GetFTPServerPathType(curItem->TgtPath),
                                                              curItem->TgtName, UPLOADSIZE_UNKNOWN, TRUE);

                        // except when STOR reports "cannot create target file name" (i.e. STOR reports an error and nothing was uploaded) we consider sending the STOR/APPE command as completion
                        // forced actions: "overwrite", "resume", and "resume or overwrite"
                        if (CurItem->ForceAction != fqiaNone) // the forced action stops applying
                            Queue->UpdateForceAction(CurItem, fqiaNone);
                    }
                    break;
                }

                case fwssWorkUploadWaitForDELERes:
                case fwssWorkUploadDelForOverWaitForDELERes:
                {
                    if (CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile) // "always true"
                    {
                        // if we do not know how deleting the file/link/directory ended, invalidate the listing in the cache
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
                Queue->ChangeTgtNameToRenamedName((CFTPQueueItemCopyOrMoveUpload*)CurItem); // even if STOR has not yet run, it is still more accurate that the file was stored under the new name than under the original one - obvious for overwriting an existing file
            else
                Queue->UpdateRenamedName((CFTPQueueItemCopyOrMoveUpload*)CurItem, NULL);
        }
        ReturnCurItemToQueue(); // return the item to the queue
    }
    // clear the worker data
    State = fwsStopped; // no need to call Oper->OperationStatusMaybeChanged(); CFTPOperation::DeleteWorkers() will call it
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
    int logUID = LogUID; // worker's log UID
    HANDLES(LeaveCriticalSection(&WorkerCritSect));

    Logs.SetIsConnected(logUID, IsConnected());
    Logs.RefreshListOfLogsInLogsDlg(); // show the "connection inactive" notification

    HandleSocketEvent(fwseClose, error, 0);

    ReportWorkerMayBeClosed(); // announce the socket closure (for other waiting threads)
}

void CFTPWorker::ReturnCurItemToQueue()
{
#ifdef _DEBUG
    if (WorkerCritSect.RecursionCount == 0 /* does not catch the situation where
      another thread uses the section */
    )
        TRACE_E("Incorrect call to CFTPWorker::ReturnCurItemToQueue(): not from section WorkerCritSect!");
#endif
    if (CurItem != NULL)
    {
        int uid = CurItem->UID;
        Queue->ReturnToWaitingItems(CurItem, Oper);
        Oper->ReportItemChange(uid); // request to redraw the item
        CurItem = NULL;
    }
    else
        TRACE_E("Useless call to CFTPWorker::ReturnCurItemToQueue()");
}

void CFTPWorker::CloseOpenedFile(BOOL transferAborted, BOOL setDateAndTime, const CFTPDate* date,
                                 const CFTPTime* time, BOOL deleteFile, CQuadWord* setEndOfFile)
{
#ifdef _DEBUG
    if (WorkerCritSect.RecursionCount == 0 /* does not catch the situation where
      another thread uses the section */
    )
        TRACE_E("Incorrect call to CFTPWorker::CloseOpenedFile(): not from section WorkerCritSect!");
#endif
    if (OpenedFile != NULL)
    {
        if (CurItem != NULL)
        {
            switch (CurItem->Type)
            {
            case fqitCopyFileOrFileLink: // copying a file or link to a file (object of class CFTPQueueItemCopyOrMove)
            case fqitMoveFileOrFileLink: // moving a file or link to a file (object of class CFTPQueueItemCopyOrMove)
            {
                // let the file be closed (if adding to the disk thread fails the file remains open,
                // because we cannot close it directly; the disk thread might be using its handle)
                BOOL delEmptyFile = (transferAborted ? CanDeleteEmptyFile : FALSE);
                FTPDiskThread->AddFileToClose(((CFTPQueueItemCopyOrMove*)CurItem)->TgtPath,
                                              ((CFTPQueueItemCopyOrMove*)CurItem)->TgtName,
                                              OpenedFile, delEmptyFile, setDateAndTime, date,
                                              time, deleteFile, setEndOfFile, NULL);
                if (deleteFile || delEmptyFile && OpenedFileSize == CQuadWord(0, 0)) // the file will almost certainly be deleted - either by direct command or because the transfer never started, so reset TgtFileState (avoid bothering with "transfer has failed")
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
    if (WorkerCritSect.RecursionCount == 0 /* does not catch the situation where
      another thread uses the section */
    )
        TRACE_E("Incorrect call to CFTPWorker::CloseOpenedInFile(): not from section WorkerCritSect!");
#endif
    if (OpenedInFile != NULL)
    {
        if (CurItem != NULL)
        {
            switch (CurItem->Type)
            {
            case fqitUploadCopyFile: // upload: copying a file (object of class CFTPQueueItemCopyOrMoveUpload)
            case fqitUploadMoveFile: // upload: moving a file (object of class CFTPQueueItemCopyOrMoveUpload)
            {
                // let the file be closed (if adding to the disk thread fails the file remains open,
                // because we cannot close it directly; the disk thread might be using its handle)
                FTPDiskThread->AddFileToClose(CurItem->Path, CurItem->Name, OpenedInFile, FALSE, FALSE, NULL,
                                              NULL, FALSE, NULL, NULL);
                OpenedInFile = NULL;
                OpenedInFileSize.Set(0, 0);
                OpenedInFileCurOffset.Set(0, 0);
                // OpenedInFileNumberOfEOLs.Set(0, 0);      // used after closing the file, so we must not zero it
                // OpenedInFileSizeWithCRLF_EOLs.Set(0, 0); // used after closing the file, so we must not zero it
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
    UploadType = utNone; // just for completeness (resetting is not necessary, but ...)
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
    sourceWorker->State = fwsLookingForWork; // post activate should transition to fwsSleeping (unless some work appeared in the meantime)
    sourceWorker->SubState = fwssNone;       // no need to call Oper->OperationStatusMaybeChanged(); handing over the work cannot change the operation state (it is not paused and will not be after the swap)
    sourceWorker->CloseOpenedFile(TRUE, FALSE, NULL, NULL, FALSE, NULL);
    sourceWorker->CloseOpenedInFile();
    CFTPQueueItem* curItem = sourceWorker->CurItem;
    sourceWorker->CurItem = NULL;
    Oper->ReportWorkerChange(sourceWorker->ID, FALSE); // request to redraw the worker
    HANDLES(LeaveCriticalSection(&(sourceWorker->WorkerCritSect)));

    HANDLES(EnterCriticalSection(&WorkerCritSect));
    if (State != fwsSleeping || SocketClosed)
        TRACE_E("Unexpected situation in CFTPWorker::GiveWorkToSleepingConWorker(): target worker is not in state fwsSleeping or is not connected!");
    State = fwsPreparing;
    SubState = fwssNone;
    CurItem = curItem;
    Oper->ReportWorkerChange(ID, FALSE); // request to redraw the worker
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

    if (bytesToWrite == 0) // writing an empty buffer
    {
        if (allBytesWritten != NULL)
            *allBytesWritten = TRUE;
        return TRUE;
    }

    HANDLES(EnterCriticalSection(&SocketCritSect));

    BOOL ret = FALSE;
    if (Socket != INVALID_SOCKET) // socket is connected
    {
        if (BytesToWriteCount == BytesToWriteOffset) // nothing is waiting to be sent; we can send
        {
            if (BytesToWriteCount != 0)
                TRACE_E("Unexpected value of BytesToWriteCount.");

            int len = 0;
            if (!SSLConn)
            {
                while (1) // loop needed because 'send' does not emit FD_WRITE when 'sentLen' < 'bytesToWrite'
                {
                    // WARNING: if the TELNET protocol is introduced again, sending IAC+IP must be reworked
                    // before aborting the command in SendFTPCommand()

                    if (!SSLConn)
                    {
                        int sentLen = send(Socket, buffer + len, bytesToWrite - len, 0);
                        if (sentLen != SOCKET_ERROR) // at least something was successfully sent (or rather accepted by Windows; delivery is uncertain)
                        {
                            len += sentLen;
                            if (len >= bytesToWrite) // has everything been sent?
                            {
                                ret = TRUE;
                                break; // stop sending (nothing left)
                            }
                        }
                        else
                        {
                            DWORD err = WSAGetLastError();
                            if (err == WSAEWOULDBLOCK) // nothing else can be sent (Windows no longer has buffer space)
                            {
                                ret = TRUE;
                                break; // stop sending (will finish after FD_WRITE)
                            }
                            else // send error
                            {
                                if (error != NULL)
                                    *error = err;
                                break; // return the error
                            }
                        }
                    }
                    else
                    {
                        int sentLen = SSLLib.SSL_write(SSLConn, buffer + len, bytesToWrite - len);
                        if (sentLen >= 0) // at least something was successfully sent (or rather accepted by Windows; delivery is uncertain)
                        {
                            len += sentLen;
                            if (len >= bytesToWrite) // has everything been sent?
                            {
                                ret = TRUE;
                                break; // stop sending (nothing left)
                            }
                        }
                        else
                        {
                            DWORD err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                            if (err == WSAEWOULDBLOCK) // nothing else can be sent (Windows no longer has buffer space)
                            {
                                ret = TRUE;
                                break; // stop sending (will finish after FD_WRITE)
                            }
                            else // send error
                            {
                                if (error != NULL)
                                    *error = err;
                                break; // return the error
                            }
                        }
                    }
                }
            }
            else
            {
                while (1) // loop needed because 'send' does not emit FD_WRITE when 'sentLen' < 'bytesToWrite'
                {
                    // WARNING: if the TELNET protocol is introduced again, sending IAC+IP must be reworked
                    // before aborting the command in SendFTPCommand()

                    int sentLen = SSLLib.SSL_write(SSLConn, buffer + len, bytesToWrite - len);
                    if (sentLen > 0) // at least something was successfully sent (or rather accepted by Windows; delivery is uncertain)
                    {
                        len += sentLen;
                        if (len >= bytesToWrite) // has everything been sent?
                        {
                            ret = TRUE;
                            break; // stop sending (nothing left)
                        }
                    }
                    else
                    {
                        DWORD err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                        if (err == WSAEWOULDBLOCK) // nothing else can be sent (Windows no longer has buffer space)
                        {
                            ret = TRUE;
                            break; // stop sending (will finish after FD_WRITE)
                        }
                        else // send error
                        {
                            if (error != NULL)
                                *error = err;
                            break; // return the error
                        }
                    }
                }
            }

            if (ret) // successfully sent; 'len' is the number of sent bytes (the rest will be sent after FD_WRITE)
            {
                if (allBytesWritten != NULL)
                    *allBytesWritten = (len >= bytesToWrite);
                if (len < bytesToWrite) // put the remainder into the 'BytesToWrite' buffer
                {
                    const char* buf = buffer + len;
                    int size = bytesToWrite - len;

                    if (BytesToWriteAllocatedSize - BytesToWriteCount < size) // not enough space in the 'BytesToWrite' buffer
                    {
                        int newSize = BytesToWriteCount + size + FTPWORKER_BYTESTOWRITEONSOCKETPREALLOC;
                        char* newBuf = (char*)realloc(BytesToWrite, newSize);
                        if (newBuf != NULL)
                        {
                            BytesToWrite = newBuf;
                            BytesToWriteAllocatedSize = newSize;
                        }
                        else // insufficient memory to store the data in our buffer (only TRACE reports the error)
                        {
                            TRACE_E(LOW_MEMORY);
                            ret = FALSE;
                        }
                    }

                    if (ret) // we can write (there is enough space in the buffer)
                    {
                        memcpy(BytesToWrite + BytesToWriteCount, buf, size);
                        BytesToWriteCount += size;
                    }
                }
            }
        }
        else // not everything was sent yet -> incorrect use of Write
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
    if (SocketCritSect.RecursionCount == 0 /* does not catch the situation where
      another thread uses the section */
    )
        TRACE_E("Incorrect call to CFTPWorker::ReadFTPReply: not from section SocketCritSect!");
#endif

    return FTPReadFTPReply(ReadBytes, ReadBytesCount, ReadBytesOffset, reply, replySize, replyCode);
}

void CFTPWorker::SkipFTPReply(int replySize)
{
    CALL_STACK_MESSAGE2("CFTPWorker::SkipFTPReply(%d)", replySize);

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount == 0 /* does not catch the situation where
      another thread uses the section */
    )
        TRACE_E("Incorrect call to CFTPWorker::SkipFTPReply: not from section SocketCritSect!");
#endif

    ReadBytesOffset += replySize;
    if (ReadBytesOffset >= ReadBytesCount) // we have already read everything - reset the buffer
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
    case FD_CLOSE: // sometimes arrives before the last FD_READ, so we must first try FD_READ and if it succeeds, post FD_CLOSE again (another FD_READ may succeed before it)
    case FD_READ:
    {
        BOOL sendFDCloseAgain = FALSE; // TRUE = FD_CLOSE arrived and there was data to read (handled as FD_READ) => post FD_CLOSE again (the current FD_CLOSE was a false alarm)
        HANDLES(EnterCriticalSection(&SocketCritSect));

        if (!EventConnectSent) // if FD_READ arrived before FD_CONNECT, post fwseConnect before reading
        {
            EventConnectSent = TRUE;
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            HandleSocketEvent(fwseConnect, eventError, 0); // post the event with the connection result
            HANDLES(EnterCriticalSection(&SocketCritSect));
        }

        BOOL ret = FALSE;
        DWORD err = NO_ERROR;
        BOOL genEvent = FALSE;
        if (eventError == NO_ERROR)
        {
            if (Socket != INVALID_SOCKET) // socket is connected
            {
                BOOL lowMem = FALSE;
                if (ReadBytesAllocatedSize - ReadBytesCount < FTPWORKER_BYTESTOREADONSOCKET) // small 'ReadBytes' buffer
                {
                    if (ReadBytesOffset > 0) // can we shift data inside the buffer?
                    {
                        memmove(ReadBytes, ReadBytes + ReadBytesOffset, ReadBytesCount - ReadBytesOffset);
                        ReadBytesCount -= ReadBytesOffset;
                        ReadBytesOffset = 0;
                    }

                    if (ReadBytesAllocatedSize - ReadBytesCount < FTPWORKER_BYTESTOREADONSOCKET) // still a small 'ReadBytes' buffer
                    {
                        int newSize = ReadBytesCount + FTPWORKER_BYTESTOREADONSOCKET +
                                      FTPWORKER_BYTESTOREADONSOCKETPREALLOC;
                        char* newBuf = (char*)realloc(ReadBytes, newSize);
                        if (newBuf != NULL)
                        {
                            ReadBytes = newBuf;
                            ReadBytesAllocatedSize = newSize;
                        }
                        else // insufficient memory to store the data in our buffer (only TRACE reports the error)
                        {
                            TRACE_E(LOW_MEMORY);
                            lowMem = TRUE;
                        }
                    }
                }

                if (!lowMem)
                { // read as many bytes as possible into the buffer; do not read cyclically so the data is processed sequentially
                    // (smaller buffers are enough); if there is still data to read we receive FD_READ again
                    if (!SSLConn)
                    {
                        int len = recv(Socket, ReadBytes + ReadBytesCount, ReadBytesAllocatedSize - ReadBytesCount, 0);
                        if (len != SOCKET_ERROR) // we may have read something (0 = connection already closed)
                        {
                            if (len > 0)
                            {
                                ReadBytesCount += len; // adjust the number of bytes already read by the newly read ones
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
                                genEvent = TRUE; // generate an event with the error
                        }
                    }
                    else
                    {
                        if (SSLLib.SSL_pending(SSLConn) > 0) // if the internal SSL buffer is not empty recv() is never called, so no additional FD_READ arrives; post it ourselves, otherwise the transfer stops
                            PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_READ);
                        int len = SSLLib.SSL_read(SSLConn, ReadBytes + ReadBytesCount, ReadBytesAllocatedSize - ReadBytesCount);
                        if (len >= 0) // we may have read something (0 = connection already closed)
                        {
                            if (len > 0)
                            {
                                ReadBytesCount += len; // adjust the number of bytes already read by the newly read ones
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
                                genEvent = TRUE; // generate an event with the error
                        }
                    }
                }
            }
            else
            {
                // can happen: the main or operation-dialog thread manages to call CloseSocket() before FD_READ is delivered
                // TRACE_E("Unexpected situation in CFTPWorker::ReceiveNetEvent(FD_READ): Socket is not connected.");
                // we will not generate an event for this unexpected error (solution: user presses ESC)
            }
        }
        else // FD_READ error report (documentation says only WSAENETDOWN)
        {
            if (WSAGETSELECTEVENT(lParam) != FD_CLOSE) // FD_CLOSE handles its own error
            {
                genEvent = TRUE;
                err = eventError;
            }
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (genEvent) // generate the fwseNewBytesRead event
        {
            HandleSocketEvent(fwseNewBytesRead, (!ret ? err : NO_ERROR), 0);
        }

        // now process FD_CLOSE
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
        {
            if (sendFDCloseAgain) // FD_CLOSE acted instead of FD_READ => post FD_CLOSE again
            {
                PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                            (WPARAM)GetSocket(), lParam);
            }
            else // proper FD_CLOSE
            {
                CSocket::ReceiveNetEvent(lParam, index); // call the base method
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
            if (BytesToWriteCount > BytesToWriteOffset) // some data remains; send the rest from the 'BytesToWrite' buffer
            {
                if (Socket != INVALID_SOCKET) // socket is connected
                {
                    int len = 0;
                    if (!SSLConn)
                    {
                        while (1) // loop needed because 'send' does not emit FD_WRITE when 'sentLen' < 'bytesToWrite'
                        {
                            int sentLen = send(Socket, BytesToWrite + BytesToWriteOffset + len,
                                               BytesToWriteCount - BytesToWriteOffset - len, 0);
                            if (sentLen != SOCKET_ERROR) // at least something was successfully sent (or rather accepted by Windows; delivery is uncertain)
                            {
                                len += sentLen;
                                if (len >= BytesToWriteCount - BytesToWriteOffset) // has everything been sent?
                                {
                                    ret = TRUE;
                                    break; // stop sending (nothing left)
                                }
                            }
                            else
                            {
                                err = WSAGetLastError();
                                if (err == WSAEWOULDBLOCK) // nothing else can be sent (Windows no longer has buffer space)
                                {
                                    ret = TRUE;
                                    break; // stop sending (will finish after FD_WRITE)
                                }
                                else // another error - reset the buffer
                                {
                                    BytesToWriteOffset = 0;
                                    BytesToWriteCount = 0;
                                    break; // return the error
                                }
                            }
                        }
                    }
                    else
                    {
                        while (1) // loop needed because 'send' does not emit FD_WRITE when 'sentLen' < 'bytesToWrite'
                        {
                            int sentLen = SSLLib.SSL_write(SSLConn, BytesToWrite + BytesToWriteOffset + len,
                                                           BytesToWriteCount - BytesToWriteOffset - len);
                            if (sentLen >= 0) // at least something was successfully sent (or rather accepted by Windows; delivery is uncertain)
                            {
                                len += sentLen;
                                if (len >= BytesToWriteCount - BytesToWriteOffset) // has everything been sent?
                                {
                                    ret = TRUE;
                                    break; // stop sending (nothing left)
                                }
                            }
                            else
                            {
                                err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                                if (err == WSAEWOULDBLOCK) // nothing else can be sent (Windows no longer has buffer space)
                                {
                                    ret = TRUE;
                                    break; // stop sending (will finish after FD_WRITE)
                                }
                                else // another error - reset the buffer
                                {
                                    BytesToWriteOffset = 0;
                                    BytesToWriteCount = 0;
                                    break; // return the error
                                }
                            }
                        }
                    }

                    if (ret && len > 0) // something was sent -> adjust 'BytesToWriteOffset'
                    {
                        BytesToWriteOffset += len;
                        if (BytesToWriteOffset >= BytesToWriteCount) // everything sent, reset the buffer
                        {
                            BytesToWriteOffset = 0;
                            BytesToWriteCount = 0;
                        }
                    }

                    genEvent = (!ret || BytesToWriteCount == BytesToWriteOffset); // error or everything has already been sent
                }
                else
                {
                    // can happen: the main or operation-dialog thread manages to call CloseSocket() before FD_WRITE is delivered
                    //TRACE_E("Unexpected situation in CFTPWorker::ReceiveNetEvent(FD_WRITE): Socket is not connected.");
                    BytesToWriteCount = 0; // error -> reset the buffer
                    BytesToWriteOffset = 0;
                    // we will not generate an event for this unexpected error (solution: user presses ESC)
                }
            }
        }
        else // FD_WRITE error report (documentation says only WSAENETDOWN)
        {
            genEvent = TRUE;
            err = eventError;
            BytesToWriteCount = 0; // error -> reset the buffer
            BytesToWriteOffset = 0;
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (genEvent) // generate the fwseWriteDone event
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
            HandleSocketEvent(fwseConnect, eventError, 0); // post the event with the connection result
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
    { // perform regular status updates in the operation dialog and cancel them when no longer needed
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
                if (WorkerDataCon->IsTransfering(NULL)) // data transfer is still in progress
                {
                    // obtain status information from the data connection
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
                    // obtain status information from the data connection
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

                // request to redraw the worker (to display the newly obtained status)
                Oper->ReportWorkerChange(ID, TRUE);

                // request another status update cycle
                clearStatusType = FALSE;

                // since we are already in the CSocketsThread::CritSect section, this call
                // is possible even from the CSocket::SocketCritSect and CFTPWorker::WorkerCritSect sections (no deadlock risk)
                SocketsThread->AddTimer(Msg, UID, GetTickCount() + WORKER_STATUSUPDATETIMEOUT,
                                        WORKER_STATUSUPDATETIMID, NULL); // ignore errors; at worst the status will not update
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
        // since we are already in the CSocketsThread::CritSect section, this call
        // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
        if (WorkerDataCon != NULL && (int)(INT_PTR)param == WorkerDataCon->GetUID()) // message came from the current data connection (discard others immediately)
            HandleEvent(fweDataConConnectedToServer, NULL, 0, 0);
        //      else   // may arrive after WorkerDataCon is closed (if the LIST reply arrives immediately and the data connection is also immediately closed - short listing)
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_DATACON_CONNECTED for nonexistent data-connection!");
        break;
    }

    case WORKER_DATACON_CLOSED:
    {
        // since we are already in the CSocketsThread::CritSect section, this call
        // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
        if (WorkerDataCon != NULL && (int)(INT_PTR)param == WorkerDataCon->GetUID()) // message came from the current data connection (discard others immediately)
            HandleEvent(fweDataConConnectionClosed, NULL, 0, 0);
        //      else   // may arrive after WorkerDataCon is closed (if the LIST reply arrives immediately and the data connection is also immediately closed - short listing)
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_DATACON_CLOSED for nonexistent data-connection!");
        break;
    }

    case WORKER_DATACON_FLUSHDATA:
    {
        // since we are already in the CSocketsThread::CritSect section, this call
        // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
        if (WorkerDataCon != NULL && (int)(INT_PTR)param == WorkerDataCon->GetUID()) // message came from the current data connection (discard others immediately)
            HandleEvent(fweDataConFlushData, NULL, 0, 0);
        //      else   // may arrive after WorkerDataCon has been closed
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_DATACON_FLUSHDATA for nonexistent data-connection!");
        break;
    }

    case WORKER_DATACON_LISTENINGFORCON:
    {
        // since we are already in the CSocketsThread::CritSect section, this call
        // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
        if (WorkerDataCon != NULL && (int)(INT_PTR)param == WorkerDataCon->GetUID()) // message came from the current data connection (discard others immediately)
            HandleEvent(fweDataConListeningForCon, NULL, 0, 0);
        else
            TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_DATACON_LISTENINGFORCON for nonexistent data-connection!");
        break;
    }

    case WORKER_UPLDATACON_LISTENINGFORCON:
    {
        // since we are already in the CSocketsThread::CritSect section, this call
        // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
        if (WorkerUploadDataCon != NULL && (int)(INT_PTR)param == WorkerUploadDataCon->GetUID()) // message came from the current data connection (discard others immediately)
            HandleEvent(fweUplDataConListeningForCon, NULL, 0, 0);
        else
            TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_UPLDATACON_LISTENINGFORCON for nonexistent data-connection!");
        break;
    }

    case WORKER_UPLDATACON_CONNECTED:
    {
        // since we are already in the CSocketsThread::CritSect section, this call
        // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
        if (WorkerUploadDataCon != NULL && (int)(INT_PTR)param == WorkerUploadDataCon->GetUID()) // message came from the current upload data connection (discard others immediately)
            HandleEvent(fweUplDataConConnectedToServer, NULL, 0, 0);
        //      else  // may arrive after WorkerUploadDataCon has been closed
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_UPLDATACON_CONNECTED for nonexistent data-connection!");
        break;
    }

    case WORKER_UPLDATACON_CLOSED:
    {
        // since we are already in the CSocketsThread::CritSect section, this call
        // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
        if (WorkerUploadDataCon != NULL && (int)(INT_PTR)param == WorkerUploadDataCon->GetUID()) // message came from the current upload data connection (discard others immediately)
            HandleEvent(fweUplDataConConnectionClosed, NULL, 0, 0);
        //      else  // may arrive after WorkerUploadDataCon has been closed
        //        TRACE_E("CFTPWorker::ReceivePostMessage(): received WORKER_UPLDATACON_CLOSED for nonexistent data-connection!");
        break;
    }

    case WORKER_UPLDATACON_PREPAREDATA:
    {
        // since we are already in the CSocketsThread::CritSect section, this call
        // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
        if (WorkerUploadDataCon != NULL && (int)(INT_PTR)param == WorkerUploadDataCon->GetUID()) // message came from the current upload data connection (discard others immediately)
            HandleEvent(fweUplDataConPrepareData, NULL, 0, 0);
        //      else  // may arrive after WorkerUploadDataCon has been closed
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
    if (SocketCritSect.RecursionCount == 0 /* does not catch the situation where
      another thread uses the section */
    )
        TRACE_E("Incorrect call to CFTPWorker::ReadFTPErrorReplies: not from section SocketCritSect!");
    if (WorkerCritSect.RecursionCount == 0 /* does not catch the situation where
      another thread uses the section */
    )
        TRACE_E("Incorrect call to CFTPWorker::ReadFTPErrorReplies: not from section WorkerCritSect!");
#endif

    char* reply;
    int replySize;
    int replyCode;
    while (ReadFTPReply(&reply, &replySize, &replyCode)) // as long as we have a server reply
    {                                                    // log any error messages from the server
        Logs.LogMessage(LogUID, reply, replySize, TRUE);
        if (ErrorDescr[0] == 0 &&                               // we do not have an error description yet
            (replyCode == -1 ||                                 // not an FTP response
             FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // transient error description
             FTP_DIGIT_1(replyCode) == FTP_D1_ERROR))           // error description
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

    if (event == fwseIPReceived) // store the IP address in the operation and call HandleEvent
    {
        CFTPWorkerEvent resEvent = fweIPReceived;
        if (data1 != INADDR_NONE)
            Oper->SetServerIP(data1); // store the IP address; all workers fetch it from the operation
        else                          // error, store it in ErrorDescr for later use
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
        if (event == fwseConnect) // receive the result of the connection attempt
        {
            CFTPWorkerEvent resEvent = fweConnected;
            if (data1 != NO_ERROR) // error, store it in ErrorDescr for later use
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
        else // handling of other messages
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
            case fwcsIdle: // unexpected reply, either an error ("timeout, closing connection", etc.) or a server bug (duplicate reply, etc.)
            {
                switch (event)
                {
                case fwseNewBytesRead: // if it is an error, we want its text
                {
                    ReadFTPErrorReplies();
                    break;
                }

                case fwseClose:
                {
                    if (data1 != NO_ERROR) // only if we have an error
                    {
                        FTPGetErrorTextForLog(data1, errBuf, 300);
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);
                    }
                    if (ErrorDescr[0] == 0) // when closing the connection we must fill ErrorDescr (even with "unknown error")
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
                    while (BytesToWriteOffset == BytesToWriteCount &&    // test "write done" (we wait for the server reply only after the full command is sent)
                           ReadFTPReply(&reply, &replySize, &replyCode)) // as long as we have some server reply (one read may contain several at once)
                    {
                        if (!firstRound)
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                        firstRound = FALSE;
                        Logs.LogMessage(LogUID, reply, replySize, CommandState == fwcsIdle); // put the command reply into the log (time is printed for "unexpected replies")
                        if (FTP_DIGIT_1(replyCode) != FTP_D1_MAYBESUCCESS)                   // command reply
                        {                                                                    // this completes the command
                            CommandState = fwcsIdle;                                         // HandleEvent() will probably send another command, so switch to "idle" immediately
                            CommandTransfersData = FALSE;
                        }
                        ErrorDescr[0] = 0; // guard against duplicate messages (receiving the expected reply = connection OK, so discard the supposed error text)
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        if (FTP_DIGIT_1(replyCode) != FTP_D1_MAYBESUCCESS) // command reply
                        {
                            // since we are already in the CSocketsThread::CritSect section, this call
                            // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
                            SocketsThread->DeleteTimer(uid, WORKER_TIMEOUTTIMERID); // HandleEvent() will probably send another command, so delete the timer "in advance"

                            HandleEvent(fweCmdReplyReceived, reply, replySize, replyCode);
                        }
                        else // server is sending information (1xx replies)
                        {
                            HandleEvent(fweCmdInfoReceived, reply, replySize, replyCode);
                        }
                        SkipFTPReply(replySize); // discard the server reply from the buffer (already processed)
                    }
                    if (firstRound)
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    leaveSect = FALSE;
                    break;
                }

                case fwseClose:
                {
                    if (data1 != NO_ERROR) // only if we have an error
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

                case fwseTimeout: // timeout, close the connection and report the closure
                {
                    if (CommandTransfersData && (WorkerDataCon != NULL || WorkerUploadDataCon != NULL))
                    {
                        // set a new timeout timer
                        int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                        if (serverTimeout < 1000)
                            serverTimeout = 1000; // at least one second

                        BOOL trFinished;
                        DWORD start;
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // since we are already in the CSocketsThread::CritSect section, this call
                        // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
                        BOOL isTransfering = WorkerDataCon != NULL ? WorkerDataCon->IsTransfering(&trFinished) : WorkerUploadDataCon->IsTransfering(&trFinished);
                        if (isTransfering)
                        { // waiting for data, so this is not a timeout
                            // since we are already in the CSocketsThread::CritSect section, this call
                            // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
                            SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                                    WORKER_TIMEOUTTIMERID, NULL); // ignore the error; at worst the user will press Stop
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                            break;
                        }
                        else
                        {
                            if (trFinished)
                            {
                                // the timeout is measured from closing the connection (moment when the server can react - it also learns
                                // about the connection closing)
                                // since we are already in the CSocketsThread::CritSect section, this call
                                // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
                                start = WorkerDataCon != NULL ? WorkerDataCon->GetSocketCloseTime() : WorkerUploadDataCon->GetSocketCloseTime();
                                if ((GetTickCount() - start) < (DWORD)serverTimeout) // the timeout since closing the connection has not yet expired
                                {
                                    // since we are already in the CSocketsThread::CritSect section, this call
                                    // is possible even from the CSocket::SocketCritSect section (no deadlock risk)
                                    SocketsThread->AddTimer(Msg, UID, start + serverTimeout,
                                                            WORKER_TIMEOUTTIMERID, NULL); // ignore the error; at worst the user will press Stop
                                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                                    break;
                                }
                                // else ;  // the timeout since closing the connection already expired -> time out
                            }
                            // else ;  // the connection has not opened yet -> time out

                            if (WorkerDataCon != NULL ? WorkerDataCon->GetProxyTimeoutDescr(errText, 200) : WorkerUploadDataCon->GetProxyTimeoutDescr(errText, 200))
                            { // if we have any timeout description for the data connection, write it to the log
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

                    ForceClose(); // manually close the socket

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
                    if (data1 != NO_ERROR) // only if we have an error
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

                case fwseWaitForCmdErr: // timeout, close the connection manually
                {
                    if (WaitForCmdErrError != NO_ERROR) // only if we have an error
                    {
                        FTPGetErrorTextForLog(WaitForCmdErrError, errBuf, 300);
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);
                    }

                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    leaveSect = FALSE;

                    ForceClose(); // manually close the socket

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
                HandleEvent(fweCmdConClosed, NULL, 0, 0); // report the socket closure to HandleEvent()
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            }
        }
    }
}
