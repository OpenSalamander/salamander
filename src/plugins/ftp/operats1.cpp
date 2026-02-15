// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

CFTPOperationsList FTPOperationsList; // all FTP operations

CRITICAL_SECTION CFTPQueueItem::NextItemUIDCritSect; // critical section for accessing CFTPQueueItem::NextItemUID
int CFTPQueueItem::NextItemUID = 0;                  // global counter for item UIDs

int CFTPOperation::NextOrdinalNumber = 0;            // global counter for an operation's OrdinalNumber (access only within the NextOrdinalNumberCS section!)
CRITICAL_SECTION CFTPOperation::NextOrdinalNumberCS; // critical section for NextOrdinalNumber

CUIDArray CanceledOperations(5, 5); // array of operation UIDs that should be canceled (after receiving the FTPCMD_CANCELOPERATION command)

HANDLE WorkerMayBeClosedEvent = NULL;      // generates a pulse when a worker socket is closed
int WorkerMayBeClosedState = 0;            // increases with every worker socket closure
CRITICAL_SECTION WorkerMayBeClosedStateCS; // critical section for accessing WorkerMayBeClosedState

CReturningConnections ReturningConnections(5, 5); // array with returned connections (from workers back to the panel)

CFTPDiskThread* FTPDiskThread = NULL; // thread handling disk operations (reason: non-blocking calls)

CUploadListingCache UploadListingCache; // cache of path listings on servers - used during uploads to check whether the target file/directory already exists

CFTPOpenedFiles FTPOpenedFiles; // list of files currently opened from this Salamander on FTP servers

//
// ****************************************************************************
// CFTPOperationsList
//

CFTPOperationsList::CFTPOperationsList() : Operations(10, 10)
{
    HANDLES(InitializeCriticalSection(&OpListCritSect));
    FirstFreeIndexInOperations = -1;
    CallOK = FALSE;
}

CFTPOperationsList::~CFTPOperationsList()
{
    HANDLES(DeleteCriticalSection(&OpListCritSect));
}

BOOL CFTPOperationsList::IsEmpty()
{
    CALL_STACK_MESSAGE1("CFTPOperationsList::IsEmpty()");

    HANDLES(EnterCriticalSection(&OpListCritSect));
    BOOL ret = Operations.Count == 0;
    HANDLES(LeaveCriticalSection(&OpListCritSect));
    return ret;
}

BOOL CFTPOperationsList::AddOperation(CFTPOperation* newOper, int* newuid)
{
    CALL_STACK_MESSAGE1("CFTPOperationsList::AddOperation(,)");

    BOOL ret = TRUE;
    HANDLES(EnterCriticalSection(&OpListCritSect));
    if (FirstFreeIndexInOperations != -1) // do we have a place to insert the new object?
    {
        // insert 'newOper' into the array
        Operations[FirstFreeIndexInOperations] = newOper;
        if (newuid != NULL)
            *newuid = FirstFreeIndexInOperations;
        newOper->SetUID(FirstFreeIndexInOperations);

        // searching for the next free slot in the array
        int i;
        for (i = FirstFreeIndexInOperations + 1; i < Operations.Count; i++)
        {
            if (Operations[i] == NULL)
            {
                FirstFreeIndexInOperations = i;
                break;
            }
        }
        if (i >= Operations.Count)
            FirstFreeIndexInOperations = -1;
    }
    else
    {
        int index = Operations.Add(newOper);
        if (Operations.IsGood())
        {
            if (newuid != NULL)
                *newuid = index;
            newOper->SetUID(index);
        }
        else
        {
            if (newuid != NULL)
                *newuid = -1;
            Operations.ResetState();
            ret = FALSE;
        }
    }
    HANDLES(LeaveCriticalSection(&OpListCritSect));
    return ret;
}

void CFTPOperationsList::DeleteOperation(int uid, BOOL doNotPostChangeOnPathNotifications)
{
    CALL_STACK_MESSAGE3("CFTPOperationsList::DeleteOperation(%d, %d)", uid,
                        doNotPostChangeOnPathNotifications);

    BOOL uploadOperDeleted = FALSE;
    char uploadUser[USER_MAX_SIZE];
    char uploadHost[HOST_MAX_SIZE];
    unsigned short uploadPort;

    HANDLES(EnterCriticalSection(&OpListCritSect));
    if (uid >= 0 && uid < Operations.Count && Operations[uid] != NULL)
    {
        CFTPOperation* oper = Operations[uid];
        CFTPOperationType operType = oper->GetOperationType();
        if (operType == fotCopyUpload || operType == fotMoveUpload)
        { // if this is an upload operation, after discarding it try to clean up the upload listing cache
            uploadOperDeleted = TRUE;
            oper->GetUserHostPort(uploadUser, uploadHost, &uploadPort);
        }
        if (!doNotPostChangeOnPathNotifications)
        {
            COperationState state = oper->GetOperationState(FALSE);
            if (state != opstSuccessfullyFinished && state != opstFinishedWithSkips && state != opstFinishedWithErrors)
                oper->PostChangeOnPathNotifications(FALSE); // the change-on-path notification has not been sent yet, send it now (later it will no longer be possible)
        }
        delete oper;                     // deallocate the operation object
        Operations[uid] = NULL;          // free the slot in the array
        if (uid + 1 == Operations.Count) // last element, deallocate the empty end of the array
        {
            while (uid > 0 && Operations[uid - 1] == NULL)
                uid--;
            Operations.Delete(uid, Operations.Count - uid); // remove the empty end of the array (only NULL elements)
            if (!Operations.IsGood())
                Operations.ResetState();
            if (FirstFreeIndexInOperations >= Operations.Count)
                FirstFreeIndexInOperations = -1;
        }
        else // deleting inside the array, no array deallocation is possible
        {
            if (FirstFreeIndexInOperations == -1 || uid < FirstFreeIndexInOperations)
                FirstFreeIndexInOperations = uid;
        }
    }
    else
        TRACE_E("CFTPOperationsList::DeleteOperation(): Pokus o vymaz neplatneho prvku pole!");
    HANDLES(LeaveCriticalSection(&OpListCritSect));

    // if an upload operation was canceled and no other upload operation is working with
    // the server used by the canceled operation, we can release this server from the upload
    // listing cache
    if (uploadOperDeleted && !IsUploadingToServer(uploadUser, uploadHost, uploadPort))
        UploadListingCache.RemoveServer(uploadUser, uploadHost, uploadPort);
}

void CFTPOperationsList::CloseAllOperationDlgs()
{
    CALL_STACK_MESSAGE1("CFTPOperationsList::CloseAllOperationDlgs()");

    HANDLES(EnterCriticalSection(&OpListCritSect));
    int i;
    for (i = 0; i < Operations.Count; i++)
    {
        CFTPOperation* oper = Operations[i];
        if (oper != NULL)
        {
            HANDLE t = NULL;
            oper->CloseOperationDlg(&t);
            if (t != NULL)
            {
                HANDLES(LeaveCriticalSection(&OpListCritSect)); // we must leave the section; the dialog thread calls CFTPOperationsList::SetOperationDlg()
                CALL_STACK_MESSAGE1("AuxThreadQueue.WaitForExit()");
                AuxThreadQueue.WaitForExit(t, INFINITE);
                HANDLES(EnterCriticalSection(&OpListCritSect));
            }
        }
    }
    HANDLES(LeaveCriticalSection(&OpListCritSect));
}

void CFTPOperationsList::WaitForFinishOrESC(HWND parent, int milliseconds, CWaitWindow* waitWnd,
                                            CWorkerWaitSatisfiedReason& reason, BOOL& postWM_CLOSE,
                                            int& lastWorkerMayBeClosedState)
{
    CALL_STACK_MESSAGE2("CFTPOperationsList::WaitForFinishOrESC(, %d,)", milliseconds);

    const DWORD cycleTime = 200; // period for checking the ESC key in ms (200 = 5 times per second) - NOTE: also a safeguard against unintended races (PulseEvent(WorkerMayBeClosedEvent) may happen before entering the wait function)
    DWORD timeStart = GetTickCount();
    DWORD restOfWaitTime = milliseconds; // remaining waiting time

    GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - see help
    while (1)
    {
        HANDLES(EnterCriticalSection(&WorkerMayBeClosedStateCS));
        BOOL socketClosure = lastWorkerMayBeClosedState != WorkerMayBeClosedState;
        lastWorkerMayBeClosedState = WorkerMayBeClosedState;
        HANDLES(LeaveCriticalSection(&WorkerMayBeClosedStateCS));
        if (socketClosure) // the worker socket was closed (or this is the first call to this method and 'lastWorkerMayBeClosedState' was -1)
        {
            reason = wwsrWorkerSocketClosure;
            break; // report the worker socket closure
        }

        DWORD waitTime;
        if (milliseconds != INFINITE)
            waitTime = min(cycleTime, restOfWaitTime);
        else
            waitTime = cycleTime;
        DWORD waitRes = MsgWaitForMultipleObjects(1, &WorkerMayBeClosedEvent, FALSE, waitTime, QS_ALLINPUT);

        // check the ESC key first (so we do not ignore it for the user)
        if (milliseconds != 0 &&                                                          // if the timeout is zero we are only pumping messages, do not handle ESC
            ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == parent || // ESC pressed
             waitWnd != NULL && waitWnd->GetWindowClosePressed()))                        // close button in the wait window
        {
            MSG msg; // discard the buffered ESC
            while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                ;
            reason = wwsrEsc;
            break; // report ESC
        }
        if (waitRes != WAIT_OBJECT_0) // no additional worker was closed (we ignore this event; it only "wakes" the waiting)
        {
            if (waitRes == WAIT_OBJECT_0 + 1) // process Windows messages
            {
                MSG msg;
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_CLOSE) // we cannot deliver WM_CLOSE; post it only after enabling the parent
                    {
                        postWM_CLOSE = TRUE;
                        SetForegroundWindow(parent); // for now bring the disabled window into view so the user knows what they are waiting for
                    }
                    else
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }
            }
            else
            {
                if (waitRes == WAIT_TIMEOUT &&
                    restOfWaitTime == waitTime) // this is not just the ESC key test cycle timeout but the global timeout
                {
                    reason = wwsrTimeout;
                    break; // report the timeout
                }
            }
        }
        if (milliseconds != INFINITE) // recalculate the remaining wait time (according to real time)
        {
            DWORD t = GetTickCount() - timeStart; // works even when the tick counter overflows
            if (t < (DWORD)milliseconds)
                restOfWaitTime = (DWORD)milliseconds - t;
            else
                restOfWaitTime = 0; // let the timeout be reported (we must not do it ourselves - the worker socket closure has priority over the timeout)
        }
    }
}

#define WORKERS_VICTIMS_COUNT 50 // number of worker sockets processed in one cycle

void CFTPOperationsList::StopWorkers(HWND parent, int operUID, int workerInd)
{
    CALL_STACK_MESSAGE3("CFTPOperationsList::StopWorkers(, %d, %d)", operUID, workerInd);

    parent = FindPopupParent(parent);
    // remember the focus from 'parent' (store NULL if the focus is not within 'parent')
    HWND focusedWnd = GetFocus();
    HWND hwnd = focusedWnd;
    while (hwnd != NULL && hwnd != parent)
        hwnd = GetParent(hwnd);
    if (hwnd != parent)
        focusedWnd = NULL;
    // disable 'parent'; when enabling it restore the focus as well
    EnableWindow(parent, FALSE);

    // set the wait cursor over the parent; unfortunately we cannot do it differently
    CSetWaitCursorWindow* winParent = new CSetWaitCursorWindow;
    if (winParent != NULL)
        winParent->AttachToWindow(parent);

    // ensure the running workers learn that they should finish (if they have
    // a data connection, interrupt it)
    CFTPWorker* victims[WORKERS_VICTIMS_COUNT]; // necessary to avoid calling socket operations inside the OpListCritSect section
    int lastOpIndex = 0;                        // optimization (index of the last processed operation - start with it in the next round)
    while (1)
    {
        BOOL done = TRUE;
        int count = 0;
        HANDLES(EnterCriticalSection(&OpListCritSect));
        if (operUID != -1) // applies only to a single operation
        {
            if (operUID >= 0 && operUID < Operations.Count)
            {
                CFTPOperation* oper = Operations[operUID];
                if (oper != NULL)
                    done &= !oper->InformWorkersAboutStop(workerInd, victims, WORKERS_VICTIMS_COUNT, &count);
            }
        }
        else // applies to all operations and all workers
        {
            int i;
            for (i = lastOpIndex; count < WORKERS_VICTIMS_COUNT && i < Operations.Count; i++)
            {
                lastOpIndex = i;
                CFTPOperation* oper = Operations[i];
                if (oper != NULL)
                    done &= !oper->InformWorkersAboutStop(-1, victims, WORKERS_VICTIMS_COUNT, &count);
            }
            done &= (i >= Operations.Count);
        }
        HANDLES(LeaveCriticalSection(&OpListCritSect));

        int i;
        for (i = 0; i < count; i++)
            victims[i]->CloseDataConnectionOrPostShouldStop();
        if (done)
            break; // all operations reported they do not want another call
    }

    // ensure that the "elapsed time" stops in the operation dialogs of the halted operations
    HANDLES(EnterCriticalSection(&OpListCritSect));
    if (operUID != -1) // applies only to a single operation
    {
        if (operUID >= 0 && operUID < Operations.Count)
        {
            CFTPOperation* oper = Operations[operUID];
            if (oper != NULL)
                oper->OperationStatusMaybeChanged();
        }
    }
    else // applies to all operations and all workers
    {
        int i;
        for (i = 0; i < Operations.Count; i++)
        {
            CFTPOperation* oper = Operations[i];
            if (oper != NULL)
                oper->OperationStatusMaybeChanged();
        }
    }
    HANDLES(LeaveCriticalSection(&OpListCritSect));

    // open the wait window + wait for ESC + timeout + completion of worker shutdown + Windows messages
    // (process WM_CLOSE later + call SetForegroundWindow on 'parent' when received)
    int closConResID = operUID != -1 ? (workerInd != -1 ? IDS_CLOSINGOPERCONS1 : IDS_CLOSINGOPERCONS2) : IDS_CLOSINGOPERCONS3;
    int termConResID = operUID != -1 ? (workerInd != -1 ? IDS_TERMCONFOROPER1 : IDS_TERMCONFOROPER2) : IDS_TERMCONFOROPER3;
    BOOL postWM_CLOSE = FALSE;
    int lastWorkerMayBeClosedState = -1;
    CWaitWindow waitWnd(parent, TRUE);
    waitWnd.SetText(LoadStr(closConResID));
    waitWnd.Create(WAITWND_CLWORKCON);
    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
    if (serverTimeout < 1000)
        serverTimeout = 1000; // at least one second

    DWORD start = GetTickCount();
    while (1)
    {
        // wait for a worker socket to close or for ESC
        DWORD now = GetTickCount();
        if (now - start > (DWORD)serverTimeout)
            now = start + (DWORD)serverTimeout;

        CWorkerWaitSatisfiedReason reason;
        WaitForFinishOrESC(parent, serverTimeout - (now - start), &waitWnd,
                           reason, postWM_CLOSE, lastWorkerMayBeClosedState);
        BOOL terminate = FALSE;
        switch (reason)
        {
        case wwsrEsc:
        {
            waitWnd.Show(FALSE);
            if (SalamanderGeneral->SalMessageBox(parent, LoadStr(termConResID),
                                                 LoadStr(IDS_FTPPLUGINTITLE),
                                                 MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                     MB_ICONQUESTION) == IDYES)
            { // cancel
                terminate = TRUE;
            }
            else
            {
                SalamanderGeneral->WaitForESCRelease(); // safeguard so that subsequent actions are not interrupted after every ESC in the previous message box
                waitWnd.Show(TRUE);
            }
            break;
        }

        case wwsrTimeout:
        {
            terminate = TRUE;
            break;
        }

            // case wwsrWorkerSocketClosure: break;  // only check whether all worker sockets are already closed
        }

        if (terminate) // cancel or timeout, force workers to finish quickly and continue waiting for them to end
        {
            lastOpIndex = 0; // optimization (index of the last processed operation - start with it in the next round)
            while (1)
            {
                BOOL done = TRUE;
                int count = 0;
                HANDLES(EnterCriticalSection(&OpListCritSect));
                if (operUID != -1) // applies only to a single operation
                {
                    if (operUID >= 0 && operUID < Operations.Count)
                    {
                        CFTPOperation* oper = Operations[operUID];
                        if (oper != NULL)
                            done &= !oper->ForceCloseWorkers(workerInd, victims, WORKERS_VICTIMS_COUNT, &count);
                    }
                }
                else // applies to all operations and all workers
                {
                    int i;
                    for (i = lastOpIndex; count < WORKERS_VICTIMS_COUNT && i < Operations.Count; i++)
                    {
                        lastOpIndex = i;
                        CFTPOperation* oper = Operations[i];
                        if (oper != NULL)
                            done &= !oper->ForceCloseWorkers(-1, victims, WORKERS_VICTIMS_COUNT, &count);
                    }
                    done &= (i >= Operations.Count);
                }
                HANDLES(LeaveCriticalSection(&OpListCritSect));

                int i;
                for (i = 0; i < count; i++)
                    victims[i]->ForceClose();
                if (done)
                    break; // all operations reported they do not want another call
            }
        }

        // check whether we can already close all workers
        BOOL finished = TRUE;
        HANDLES(EnterCriticalSection(&OpListCritSect));
        if (operUID != -1) // applies only to a single operation
        {
            if (operUID >= 0 && operUID < Operations.Count)
            {
                CFTPOperation* oper = Operations[operUID];
                if (oper != NULL)
                    finished &= oper->CanCloseWorkers(workerInd);
            }
        }
        else // applies to all operations and all workers
        {
            int i;
            for (i = 0; i < Operations.Count; i++)
            {
                CFTPOperation* oper = Operations[i];
                if (oper != NULL)
                    finished &= oper->CanCloseWorkers(-1);
            }
        }
        HANDLES(LeaveCriticalSection(&OpListCritSect));
        if (!finished)
            Sleep(100); // if the worker sockets were tested, wait a moment before the next possible test (prevents unnecessary machine load)
        else
            break; // worker sockets are closed, we can finish
    }
    waitWnd.Destroy();

    // remove finished workers (first release their data - return the operation items back to the queue)
    CUploadWaitingWorker* uploadFirstWaitingWorker = NULL; // upload operation: list of workers waiting for listings retrieved by some of the terminated workers (i.e. workers that need to be informed that the workers they waited on have finished)
    lastOpIndex = 0;                                       // optimization (index of the last processed operation - start with it in the next round)
    while (1)
    {
        BOOL done = TRUE;
        int count = 0;
        HANDLES(EnterCriticalSection(&OpListCritSect));
        if (operUID != -1) // applies only to a single operation
        {
            if (operUID >= 0 && operUID < Operations.Count)
            {
                CFTPOperation* oper = Operations[operUID];
                if (oper != NULL)
                    done &= !oper->DeleteWorkers(workerInd, victims, WORKERS_VICTIMS_COUNT, &count, &uploadFirstWaitingWorker);
            }
        }
        else // applies to all operations and all workers
        {
            int i;
            for (i = lastOpIndex; count < WORKERS_VICTIMS_COUNT && i < Operations.Count; i++)
            {
                lastOpIndex = i;
                CFTPOperation* oper = Operations[i];
                if (oper != NULL)
                    done &= !oper->DeleteWorkers(-1, victims, WORKERS_VICTIMS_COUNT, &count, &uploadFirstWaitingWorker);
            }
            done &= (i >= Operations.Count);
        }
        HANDLES(LeaveCriticalSection(&OpListCritSect));

        // upload operation: inform workers waiting for listings obtained by the terminating workers
        while (uploadFirstWaitingWorker != NULL)
        {
            SocketsThread->PostSocketMessage(uploadFirstWaitingWorker->WorkerMsg,
                                             uploadFirstWaitingWorker->WorkerUID,
                                             WORKER_TGTPATHLISTINGFINISHED, NULL);

            CUploadWaitingWorker* del = uploadFirstWaitingWorker;
            uploadFirstWaitingWorker = uploadFirstWaitingWorker->NextWorker;
            delete del;
        }

        int i;
        for (i = 0; i < count; i++)
            DeleteSocket(victims[i]);
        if (done)
            break; // all operations reported they do not want another call
    }

    Logs.RefreshListOfLogsInLogsDlg(); // refresh the Log - workers are closed

    // drop the wait cursor over the parent
    if (winParent != NULL)
    {
        winParent->DetachWindow();
        delete winParent;
    }

    // enable 'parent'
    EnableWindow(parent, TRUE);
    // if 'parent' is active, restore the focus as well
    if (GetForegroundWindow() == parent)
    {
        if (parent == SalamanderGeneral->GetMainWindowHWND())
            SalamanderGeneral->RestoreFocusInSourcePanel();
        else
        {
            if (focusedWnd != NULL)
                SetFocus(focusedWnd);
        }
    }

    // if WM_CLOSE arrived while waiting for the workers to finish, post it to the queue now,
    // it can be processed now
    if (postWM_CLOSE)
        PostMessage(parent, WM_CLOSE, 0, 0);
}

void CFTPOperationsList::PauseWorkers(HWND parent, int operUID, int workerInd, BOOL pause)
{
    CALL_STACK_MESSAGE4("CFTPOperationsList::PauseWorkers(, %d, %d, %d)", operUID, workerInd, pause);

    // ensure the relevant workers learn that they should perform pause/resume
    CFTPWorker* victims[WORKERS_VICTIMS_COUNT]; // necessary to avoid calling socket operations inside the OpListCritSect section
    int lastOpIndex = 0;                        // optimization (index of the last processed operation - start with it in the next round)
    while (1)
    {
        BOOL done = TRUE;
        int count = 0;
        HANDLES(EnterCriticalSection(&OpListCritSect));
        if (operUID != -1) // applies only to a single operation
        {
            if (operUID >= 0 && operUID < Operations.Count)
            {
                CFTPOperation* oper = Operations[operUID];
                if (oper != NULL)
                    done &= !oper->InformWorkersAboutPause(workerInd, victims, WORKERS_VICTIMS_COUNT, &count, pause);
            }
        }
        else // applies to all operations and all workers
        {
            int i;
            for (i = lastOpIndex; count < WORKERS_VICTIMS_COUNT && i < Operations.Count; i++)
            {
                lastOpIndex = i;
                CFTPOperation* oper = Operations[i];
                if (oper != NULL)
                    done &= !oper->InformWorkersAboutPause(-1, victims, WORKERS_VICTIMS_COUNT, &count, pause);
            }
            done &= (i >= Operations.Count);
        }
        HANDLES(LeaveCriticalSection(&OpListCritSect));

        int i;
        for (i = 0; i < count; i++)
            victims[i]->PostShouldPauseOrResume();
        if (done)
            break; // all operations reported they do not want another call
    }

    // let every operation check whether a full pause/resume happened
    // (stop/start the timer + reset meters immediately after resuming)
    HANDLES(EnterCriticalSection(&OpListCritSect));
    if (operUID != -1) // applies only to a single operation
    {
        if (operUID >= 0 && operUID < Operations.Count)
        {
            CFTPOperation* oper = Operations[operUID];
            if (oper != NULL)
                oper->OperationStatusMaybeChanged();
        }
    }
    else // applies to all operations and all workers
    {
        int i;
        for (i = 0; i < Operations.Count; i++)
        {
            CFTPOperation* oper = Operations[i];
            if (oper != NULL)
                oper->OperationStatusMaybeChanged();
        }
    }
    HANDLES(LeaveCriticalSection(&OpListCritSect));
}

BOOL CFTPOperationsList::StartCall(int operUID)
{
    HANDLES(EnterCriticalSection(&OpListCritSect));
    CallOK = TRUE;
    if (operUID >= 0 && operUID < Operations.Count && Operations[operUID] != NULL)
        return TRUE;
    else
    {
        TRACE_E("Unexpected situation in CFTPOperationsList::StartCall(): operUID is invalid!");
        return CallOK = FALSE;
    }
}

BOOL CFTPOperationsList::EndCall()
{
    HANDLES(LeaveCriticalSection(&OpListCritSect));
    return CallOK;
}

BOOL CFTPOperationsList::ActivateOperationDlg(int operUID, BOOL& success, HWND dropTargetWnd)
{
    CALL_STACK_MESSAGE2("CFTPOperationsList::ActivateOperationDlg(%d, ,)", operUID);
    if (StartCall(operUID))
        success = Operations[operUID]->ActivateOperationDlg(dropTargetWnd);
    else
        success = FALSE;
    return EndCall();
}

BOOL CFTPOperationsList::CloseOperationDlg(int operUID, HANDLE* dlgThread)
{
    CALL_STACK_MESSAGE2("CFTPOperationsList::CloseOperationDlg(%d,)", operUID);
    if (StartCall(operUID))
        Operations[operUID]->CloseOperationDlg(dlgThread);
    return EndCall();
}

BOOL CFTPOperationsList::CanMakeChangesOnPath(const char* user, const char* host, unsigned short port,
                                              const char* path, CFTPServerPathType pathType,
                                              int ignoreOperUID)
{
    CALL_STACK_MESSAGE7("CFTPOperationsList::CanMakeChangesOnPath(%s, %s, %u, %s, %d, %d)",
                        user, host, port, path, pathType, ignoreOperUID);
    int userLength = 0;
    if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
        user = NULL;
    if (user != NULL)
        userLength = FTPGetUserLength(user);

    HANDLES(EnterCriticalSection(&OpListCritSect));
    BOOL ret = FALSE;
    int i;
    for (i = 0; i < Operations.Count; i++)
    {
        CFTPOperation* oper = Operations[i];
        if (oper != NULL && oper->GetUID() != ignoreOperUID &&
            oper->CanMakeChangesOnPath(user, host, port, path, pathType, userLength))
        {
            ret = TRUE;
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&OpListCritSect));
    return ret;
}

BOOL CFTPOperationsList::IsUploadingToServer(const char* user, const char* host, unsigned short port)
{
    CALL_STACK_MESSAGE4("CFTPOperationsList::IsUploadingToServer(%s, %s, %u)",
                        user, host, port);
    int userLength = 0;
    if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
        user = NULL;
    if (user != NULL)
        userLength = FTPGetUserLength(user);

    HANDLES(EnterCriticalSection(&OpListCritSect));
    BOOL ret = FALSE;
    int i;
    for (i = 0; i < Operations.Count; i++)
    {
        CFTPOperation* oper = Operations[i];
        if (oper != NULL && oper->IsUploadingToServer(user, host, port, userLength))
        {
            ret = TRUE;
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&OpListCritSect));
    return ret;
}

//
// ****************************************************************************
// CFTPQueue
//

CFTPQueue::CFTPQueue() : Items(100, 500)
{
    HANDLES(InitializeCriticalSection(&QueueCritSect));
    LastFoundUID = -1;
    LastFoundIndex = 0;
    FirstWaitingItemIndex = 0;
    GetOnlyExploreAndResolveItems = TRUE;
    ExploreAndResolveItemsCount = 0;
    DoneOrSkippedItemsCount = 0;
    WaitingOrProcessingOrDelayedItemsCount = 0;
    DoneOrSkippedByteSize.Set(0, 0);
    DoneOrSkippedBlockSize.Set(0, 0);
    WaitingOrProcessingOrDelayedByteSize.Set(0, 0);
    WaitingOrProcessingOrDelayedBlockSize.Set(0, 0);
    DoneOrSkippedUploadSize.Set(0, 0);
    WaitingOrProcessingOrDelayedUploadSize.Set(0, 0);
    CopyUnknownSizeCount = 0;
    CopyUnknownSizeCountIfUnknownBlockSize = 0;
    LastErrorOccurenceTime = -1;
    LastFoundErrorOccurenceTime = -1;
}

CFTPQueue::~CFTPQueue()
{
    HANDLES(DeleteCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateCounters(CFTPQueueItem* item, BOOL add)
{
    if (item->IsExploreOrResolveItem())
    {
        if (add)
            ExploreAndResolveItemsCount++;
        else
            ExploreAndResolveItemsCount--;
    }

    if (item->Type != fqitCopyFileOrFileLink && item->Type != fqitMoveFileOrFileLink &&
        item->Type != fqitUploadCopyFile && item->Type != fqitUploadMoveFile &&
        item->GetItemState() != sqisDone && item->GetItemState() != sqisSkipped)
    {
        if (add)
            CopyUnknownSizeCount++;
        else
            CopyUnknownSizeCount--;
    }

    switch (item->GetItemState())
    {
    case sqisDone:
    case sqisSkipped:
    {
        if (add)
            DoneOrSkippedItemsCount++;
        else
            DoneOrSkippedItemsCount--;

        if (item->Type == fqitCopyFileOrFileLink || item->Type == fqitMoveFileOrFileLink)
        {
            CFTPQueueItemCopyOrMove* copyItem = (CFTPQueueItemCopyOrMove*)item;
            if (copyItem->Size != CQuadWord(-1, -1))
            {
                if (copyItem->SizeInBytes)
                {
                    if (add)
                        DoneOrSkippedByteSize += copyItem->Size;
                    else
                        DoneOrSkippedByteSize -= copyItem->Size;
                }
                else
                {
                    if (add)
                        DoneOrSkippedBlockSize += copyItem->Size;
                    else
                        DoneOrSkippedBlockSize -= copyItem->Size;
                }
            }
        }
        else
        {
            if (item->Type == fqitUploadCopyFile || item->Type == fqitUploadMoveFile)
            {
                if (add)
                    DoneOrSkippedUploadSize += ((CFTPQueueItemCopyOrMoveUpload*)item)->Size;
                else
                    DoneOrSkippedUploadSize -= ((CFTPQueueItemCopyOrMoveUpload*)item)->Size;
            }
        }
        break;
    }

    case sqisWaiting:
    case sqisProcessing:
    case sqisDelayed:
    {
        if (add)
            WaitingOrProcessingOrDelayedItemsCount++;
        else
            WaitingOrProcessingOrDelayedItemsCount--;

        if (item->Type == fqitCopyFileOrFileLink || item->Type == fqitMoveFileOrFileLink)
        {
            CFTPQueueItemCopyOrMove* copyItem = (CFTPQueueItemCopyOrMove*)item;
            if (copyItem->Size != CQuadWord(-1, -1))
            {
                if (copyItem->SizeInBytes)
                {
                    if (add)
                        WaitingOrProcessingOrDelayedByteSize += copyItem->Size;
                    else
                        WaitingOrProcessingOrDelayedByteSize -= copyItem->Size;
                }
                else
                {
                    if (add)
                    {
                        WaitingOrProcessingOrDelayedBlockSize += copyItem->Size;
                        CopyUnknownSizeCountIfUnknownBlockSize++;
                    }
                    else
                    {
                        WaitingOrProcessingOrDelayedBlockSize -= copyItem->Size;
                        CopyUnknownSizeCountIfUnknownBlockSize--;
                    }
                }
            }
            else
            {
                if (add)
                    CopyUnknownSizeCount++;
                else
                    CopyUnknownSizeCount--;
            }
        }
        else
        {
            if (item->Type == fqitUploadCopyFile || item->Type == fqitUploadMoveFile)
            {
                if (add)
                    WaitingOrProcessingOrDelayedUploadSize += ((CFTPQueueItemCopyOrMoveUpload*)item)->Size;
                else
                    WaitingOrProcessingOrDelayedUploadSize -= ((CFTPQueueItemCopyOrMoveUpload*)item)->Size;
            }
        }
        break;
    }

    default: // sqisFailed, sqisUserInputNeeded, sqisForcedToFail
    {
        if (item->Type == fqitCopyFileOrFileLink || item->Type == fqitMoveFileOrFileLink)
        {
            CFTPQueueItemCopyOrMove* copyItem = (CFTPQueueItemCopyOrMove*)item;
            if (copyItem->Size == CQuadWord(-1, -1))
            {
                if (add)
                    CopyUnknownSizeCount++;
                else
                    CopyUnknownSizeCount--;
            }
            else
            {
                if (!copyItem->SizeInBytes)
                {
                    if (add)
                        CopyUnknownSizeCountIfUnknownBlockSize++;
                    else
                        CopyUnknownSizeCountIfUnknownBlockSize--;
                }
            }
        }
        break;
    }
    }
}

BOOL CFTPQueue::AddItem(CFTPQueueItem* newItem)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CFTPQueue::AddItem()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    BOOL ret = TRUE;
    Items.Add(newItem);
    if (Items.IsGood())
    {
        UpdateCounters(newItem, TRUE);
        if (newItem->IsItemInSimpleErrorState())
            newItem->ErrorOccurenceTime = GiveLastErrorOccurenceTime();
    }
    else
    {
        Items.ResetState();
        ret = FALSE;
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return ret;
}

BOOL CFTPQueue::ReplaceItemWithListOfItems(int itemUID, CFTPQueueItem** items, int itemsCount)
{
    CALL_STACK_MESSAGE3("CFTPQueue::ReplaceItemWithListOfItems(%d, , %d)", itemUID, itemsCount);

    HANDLES(EnterCriticalSection(&QueueCritSect));
    BOOL ret = FALSE;
    if (FindItemWithUID(itemUID) != NULL) // item found
    {
        if (itemsCount > 1)
        {
            Items.Insert(LastFoundIndex + 1, items + 1, itemsCount - 1);
            if (Items.IsGood())
            {
                UpdateCounters(Items[LastFoundIndex], FALSE);
                delete Items[LastFoundIndex];
                Items[LastFoundIndex] = *items;
                LastFoundUID = (*items)->UID;
                int i;
                for (i = LastFoundIndex; i < LastFoundIndex + itemsCount; i++)
                {
                    CFTPQueueItem* item = Items[i];
                    UpdateCounters(item, TRUE);
                    if (item->IsItemInSimpleErrorState())
                        item->ErrorOccurenceTime = GiveLastErrorOccurenceTime();
                }
                ret = TRUE;
                HandleFirstWaitingItemIndex(TRUE, // instead of searching the array we expect the worst case - an explore item at the first position in the array
                                            LastFoundIndex);
            }
            else
                Items.ResetState();
        }
        else
        {
            if (itemsCount == 1)
            {
                UpdateCounters(Items[LastFoundIndex], FALSE);
                delete Items[LastFoundIndex];
                Items[LastFoundIndex] = *items;
                LastFoundUID = (*items)->UID;
                UpdateCounters(*items, TRUE);
                if ((*items)->IsItemInSimpleErrorState())
                    (*items)->ErrorOccurenceTime = GiveLastErrorOccurenceTime();
                HandleFirstWaitingItemIndex((*items)->IsExploreOrResolveItem(), LastFoundIndex);
            }
            else // itemsCount == 0
            {
                if (FirstWaitingItemIndex > LastFoundIndex)
                    FirstWaitingItemIndex--; // the array will shift, adjust FirstWaitingItemIndex
                UpdateCounters(Items[LastFoundIndex], FALSE);
                Items.Delete(LastFoundIndex);
                if (!Items.IsGood())
                    Items.ResetState(); // maximum error when shrinking the array, but the deletion was surely executed
                LastFoundIndex = 0;
                LastFoundUID = -1;
            }
            ret = TRUE;
        }
    }
    else
        TRACE_E("Unexpected situation in CFTPQueue::ReplaceItemWithListOfItems(): item not found: UID=" << itemUID);
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return ret;
}

void CFTPQueue::AddToNotDoneSkippedFailed(int itemDirUID, int notDone, int skipped, int failed,
                                          int uiNeeded, CFTPOperation* oper)
{
    CALL_STACK_MESSAGE6("CFTPQueue::AddToNotDoneSkippedFailed(%d, %d, %d, %d, %d)",
                        itemDirUID, notDone, skipped, uiNeeded, failed);

    HANDLES(EnterCriticalSection(&QueueCritSect));
    CFTPQueueItem* found = FindItemWithUID(itemDirUID);
    if (found != NULL)
    {
        if (found->Type == fqitDeleteDir || found->Type == fqitMoveDeleteDir ||
            found->Type == fqitUploadMoveDeleteDir || found->Type == fqitMoveDeleteDirLink ||
            found->Type == fqitChAttrsDir)
        {
            CFTPQueueItemDir* itemDir = (CFTPQueueItemDir*)found;
            itemDir->ChildItemsNotDone += notDone;
            itemDir->ChildItemsSkipped += skipped;
            itemDir->ChildItemsFailed += failed;
            itemDir->ChildItemsUINeeded += uiNeeded;
            if (itemDir->ChildItemsNotDone < 0 || itemDir->ChildItemsSkipped < 0 ||
                itemDir->ChildItemsFailed < 0 || itemDir->ChildItemsUINeeded < 0)
            {
                TRACE_E("Unexpected situation in CFTPQueue::AddToNotDoneSkippedFailed(): some counter is negative! "
                        "NotDone="
                        << itemDir->ChildItemsNotDone << ", Skipped=" << itemDir->ChildItemsSkipped << ", Failed=" << itemDir->ChildItemsFailed << ", UINeeded=" << itemDir->ChildItemsUINeeded);
            }
            // if an automatic state change is possible, perform it
            if (itemDir->GetItemState() == sqisDelayed || itemDir->GetItemState() == sqisForcedToFail)
            {
                CFTPQueueItemState newState = itemDir->GetStateFromCounters();
                BOOL change = itemDir->GetItemState() != newState;
                if (change)
                {
                    if (newState == sqisWaiting)
                        HandleFirstWaitingItemIndex(FALSE, LastFoundIndex);
                    itemDir->ChangeStateAndCounters(newState, oper, this);
                    oper->ReportItemChange(itemDir->UID);
                }
            }
        }
        else
            TRACE_E("Unexpected situation in CFTPQueue::AddToNotDoneSkippedFailed(): parent item is type=" << found->Type);
    }
    else
        TRACE_E("Unexpected situation in CFTPQueue::AddToNotDoneSkippedFailed(): unknown parent item, UID=" << itemDirUID);
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

int CFTPQueue::GetCount()
{
    CALL_STACK_MESSAGE1("CFTPQueue::GetCount()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    int ret = Items.Count;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return ret;
}

void CFTPQueue::LockForMoreOperations()
{
    CALL_STACK_MESSAGE1("CFTPQueue::LockForMoreOperations()");
    HANDLES(EnterCriticalSection(&QueueCritSect));
}

void CFTPQueue::UnlockForMoreOperations()
{
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

int CFTPQueue::GetUserInputNeededCount(BOOL onlyUINeeded, TDirectArray<DWORD>* UINeededArr,
                                       int focusedItemUID, int* indexInAll, int* indexInUIN)
{
    CALL_STACK_MESSAGE1("CFTPQueue::GetUserInputNeededCount()");

    *indexInAll = -1;
    *indexInUIN = -1;

    HANDLES(EnterCriticalSection(&QueueCritSect));
    int c = 0;
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CFTPQueueItem* item = Items[i];
        if (item->UID == focusedItemUID)
        {
            *indexInAll = i;
            if (item->GetItemState() >= sqisSkipped /* sqisSkipped, sqisFailed, sqisForcedToFail nebo sqisUserInputNeeded */)
                *indexInUIN = c;
        }
        if (item->GetItemState() >= sqisSkipped /* sqisSkipped, sqisFailed, sqisForcedToFail nebo sqisUserInputNeeded */)
        {
            if (onlyUINeeded)
                UINeededArr->Add(i);
            c++;
        }
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    if (!UINeededArr->IsGood())
        UINeededArr->ResetState();
    return c;
}

int CFTPQueue::GetItemUID(int index)
{
    CALL_STACK_MESSAGE1("CFTPQueue::GetItemUID()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    int uid;
    if (index >= 0 && index < Items.Count)
        uid = Items[index]->UID;
    else
        uid = -1;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return uid;
}

int CFTPQueue::GetItemIndex(int itemUID)
{
    CALL_STACK_MESSAGE1("CFTPQueue::GetItemIndex()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    int index;
    if (FindItemWithUID(itemUID) != NULL)
        index = LastFoundIndex;
    else
        index = -1;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return index;
}

void CFTPQueue::GetListViewDataFor(int index, NMLVDISPINFO* lvdi, char* buf, int bufSize)
{
    CALL_STACK_MESSAGE1("CFTPQueue::GetListViewDataFor()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    LVITEM* itemData = &(lvdi->item);
    if (index >= 0 && index < Items.Count) // index is valid
    {
        CFTPQueueItem* item = Items[index];
        if (itemData->mask & LVIF_IMAGE)
        {
            switch (item->Type)
            {
            case fqitDeleteLink:
            case fqitDeleteFile:
            case fqitCopyResolveLink:
            case fqitMoveResolveLink:
            case fqitCopyFileOrFileLink:
            case fqitMoveFileOrFileLink:
            case fqitChAttrsFile:
            case fqitChAttrsResolveLink:
            case fqitUploadCopyFile:
            case fqitUploadMoveFile:
                itemData->iImage = 1;
                break; // file icon

                /*      case fqitDeleteDir:
        case fqitDeleteExploreDir:
        case fqitMoveDeleteDir:
        case fqitMoveDeleteDirLink:
        case fqitCopyExploreDir:
        case fqitMoveExploreDir:
        case fqitMoveExploreDirLink:
        case fqitChAttrsDir:
        case fqitChAttrsExploreDir:
        case fqitChAttrsExploreDirLink:
        case fqitUploadCopyExploreDir:
        case fqitUploadMoveExploreDir:
        case fqitUploadMoveDeleteDir: */
            default:
                itemData->iImage = 0;
                break; // directory icon
            }
        }
        if ((itemData->mask & LVIF_TEXT) && bufSize > 0)
        {
            char unixRights[20];
            char size[110];
            switch (itemData->iSubItem)
            {
            case 0: // description
            {
                switch (item->Type)
                {
                case fqitDeleteLink:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_DELLINK), item->Name, item->Path);
                    break;
                }

                case fqitDeleteFile:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_DELFILE), item->Name, item->Path);
                    break;
                }

                case fqitDeleteDir:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_DELDIR), item->Name, item->Path);
                    break;
                }

                case fqitDeleteExploreDir:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_DELEXPLDIR), item->Name, item->Path);
                    break;
                }

                case fqitChAttrsExploreDir:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_CHATTREXPLDIR), item->Name, item->Path);
                    break;
                }

                case fqitCopyResolveLink:
                case fqitMoveResolveLink:
                case fqitChAttrsResolveLink:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_RESLINK), item->Name, item->Path);
                    break;
                }

                case fqitCopyFileOrFileLink:
                case fqitMoveFileOrFileLink:
                {
                    if (((CFTPQueueItemCopyOrMove*)item)->Size != CQuadWord(-1, -1))
                    { // if the size is known
                        strcpy(size, " (");
                        if (((CFTPQueueItemCopyOrMove*)item)->SizeInBytes) // size in bytes
                            SalamanderGeneral->PrintDiskSize(size + 2, ((CFTPQueueItemCopyOrMove*)item)->Size, 0);
                        else // size in blocks
                        {
                            SalamanderGeneral->NumberToStr(size + 2, ((CFTPQueueItemCopyOrMove*)item)->Size);
                            strcat(size, " ");
                            strcat(size, LoadStr(IDS_OPERDOPDS_SIZEINBLOCKS));
                        }
                        strcat(size, ")");
                    }
                    else
                        size[0] = 0;
                    if (strcmp(((CFTPQueueItemCopyOrMove*)item)->TgtName, item->Name) == 0)
                    { // same target file name
                        _snprintf_s(buf, bufSize, _TRUNCATE,
                                    LoadStr(item->Type == fqitCopyFileOrFileLink ? IDS_OPERDOPDS_COPY1 : IDS_OPERDOPDS_MOVE1), item->Name, size,
                                    item->Path, ((CFTPQueueItemCopyOrMove*)item)->TgtPath,
                                    LoadStr(((CFTPQueueItemCopyOrMove*)item)->AsciiTransferMode ? IDS_OPERDOPDS_ASCIITRMODE : IDS_OPERDOPDS_BINARYTRMODE));
                    }
                    else // different target file name
                    {
                        _snprintf_s(buf, bufSize, _TRUNCATE,
                                    LoadStr(item->Type == fqitCopyFileOrFileLink ? IDS_OPERDOPDS_COPY2 : IDS_OPERDOPDS_MOVE2), item->Name, size,
                                    item->Path, ((CFTPQueueItemCopyOrMove*)item)->TgtPath,
                                    ((CFTPQueueItemCopyOrMove*)item)->TgtName,
                                    LoadStr(((CFTPQueueItemCopyOrMove*)item)->AsciiTransferMode ? IDS_OPERDOPDS_ASCIITRMODE : IDS_OPERDOPDS_BINARYTRMODE));
                    }
                    break;
                }

                case fqitUploadCopyFile:
                case fqitUploadMoveFile:
                {
                    strcpy(size, " (");
                    SalamanderGeneral->PrintDiskSize(size + 2, ((CFTPQueueItemCopyOrMoveUpload*)item)->Size, 0);
                    strcat(size, ")");

                    CFTPQueueItemCopyOrMoveUpload* uploadItem = (CFTPQueueItemCopyOrMoveUpload*)item;
                    char* name = uploadItem->RenamedName != NULL ? uploadItem->RenamedName : uploadItem->TgtName;
                    if (strcmp(name, item->Name) == 0)
                    { // same target file name
                        _snprintf_s(buf, bufSize, _TRUNCATE,
                                    LoadStr(item->Type == fqitUploadCopyFile ? IDS_OPERDOPDS_COPY1 : IDS_OPERDOPDS_MOVE1),
                                    item->Name, size, item->Path, uploadItem->TgtPath,
                                    LoadStr(uploadItem->AsciiTransferMode ? IDS_OPERDOPDS_ASCIITRMODE : IDS_OPERDOPDS_BINARYTRMODE));
                    }
                    else // different target file name
                    {
                        _snprintf_s(buf, bufSize, _TRUNCATE,
                                    LoadStr(item->Type == fqitUploadCopyFile ? IDS_OPERDOPDS_COPY2 : IDS_OPERDOPDS_MOVE2),
                                    item->Name, size, item->Path, uploadItem->TgtPath, name,
                                    LoadStr(uploadItem->AsciiTransferMode ? IDS_OPERDOPDS_ASCIITRMODE : IDS_OPERDOPDS_BINARYTRMODE));
                    }
                    break;
                }

                case fqitMoveDeleteDir:
                case fqitUploadMoveDeleteDir:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_MOVEDELDIR), item->Name, item->Path);
                    break;
                }

                case fqitMoveDeleteDirLink:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_MOVEDELDIRLNK), item->Name, item->Path);
                    break;
                }

                case fqitCopyExploreDir:
                case fqitUploadCopyExploreDir:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_COPYEXPLDIR), item->Name, item->Path,
                                ((CFTPQueueItemCopyMoveExplore*)item)->TgtPath,
                                ((CFTPQueueItemCopyMoveExplore*)item)->TgtName);
                    break;
                }

                case fqitMoveExploreDir:
                case fqitUploadMoveExploreDir:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_MOVEEXPLDIR), item->Name, item->Path,
                                ((CFTPQueueItemCopyMoveExplore*)item)->TgtPath,
                                ((CFTPQueueItemCopyMoveExplore*)item)->TgtName);
                    break;
                }

                case fqitMoveExploreDirLink:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_MOVEEXPLDIRLNK), item->Name, item->Path,
                                ((CFTPQueueItemCopyMoveExplore*)item)->TgtPath,
                                ((CFTPQueueItemCopyMoveExplore*)item)->TgtName);
                    break;
                }

                case fqitChAttrsFile:
                {
                    sprintf(unixRights, "%03o (", ((CFTPQueueItemChAttr*)item)->Attr);
                    GetUNIXRightsStr(unixRights + strlen(unixRights), 20, ((CFTPQueueItemChAttr*)item)->Attr);
                    strcat(unixRights, ")");
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_CHATTRFILE), item->Name, item->Path, unixRights);
                    break;
                }

                case fqitChAttrsDir:
                {
                    sprintf(unixRights, "%03o (", ((CFTPQueueItemChAttrDir*)item)->Attr);
                    GetUNIXRightsStr(unixRights + strlen(unixRights), 20, ((CFTPQueueItemChAttrDir*)item)->Attr);
                    strcat(unixRights, ")");
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_CHATTRDIR), item->Name, item->Path, unixRights);
                    break;
                }

                case fqitChAttrsExploreDirLink:
                {
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDOPDS_CHATTREXPLDIRLNK), item->Name, item->Path);
                    break;
                }

                default:
                {
                    TRACE_E("Unexpected situation in CFTPQueue::GetListViewDataFor(): unknown operation item type!");
                    buf[0] = 0;
                    break;
                }
                }
                break;
            }

            case 1: // status
            {
                char reason[500];
                switch (item->GetItemState())
                {
                case sqisDone:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGOPSTS_FINISHED));
                    break;

                case sqisSkipped:
                {
                    item->GetProblemDescr(reason, 500);
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGOPSTS_SKIPPED), reason);
                    break;
                }

                case sqisFailed:
                {
                    item->GetProblemDescr(reason, 500);
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGOPSTS_FAILED), reason);
                    break;
                }

                case sqisForcedToFail:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGOPSTS_FORCEDTOFAIL));
                    break;

                case sqisUserInputNeeded:
                {
                    item->GetProblemDescr(reason, 500);
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGOPSTS_WAITUSER), reason);
                    break;
                }

                case sqisWaiting:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGOPSTS_WAITING));
                    break;
                case sqisProcessing:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGOPSTS_PROCESSING));
                    break;
                case sqisDelayed:
                    _snprintf_s(buf, bufSize, _TRUNCATE, LoadStr(IDS_OPERDLGOPSTS_DELAYED));
                    break;

                default:
                {
                    TRACE_E("Unexpected situation in CFTPQueue::GetListViewDataFor(): unknown status!");
                    buf[0] = 0;
                    break;
                }
                }
                break;
            }
            }
            itemData->pszText = buf;
        }
    }
    else // for an invalid index (the list view has not refreshed yet) we must return at least an empty item
    {
        if (itemData->mask & LVIF_IMAGE)
            itemData->iImage = 1 /* the file icon is less pronounced */;
        if (itemData->mask & LVIF_TEXT)
        {
            if (bufSize > 0)
                buf[0] = 0;
            itemData->pszText = buf;
        }
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

BOOL CFTPQueue::IsItemWithErrorToSolve(int index, BOOL* canSkip, BOOL* canRetry)
{
    CALL_STACK_MESSAGE1("CFTPQueue::IsItemWithErrorToSolve(,,)");

    BOOL ret = FALSE;
    if (canSkip != NULL)
        *canSkip = FALSE;
    if (canRetry != NULL)
        *canRetry = FALSE;
    HANDLES(EnterCriticalSection(&QueueCritSect));
    if (index >= 0 && index < Items.Count) // index is valid
    {
        CFTPQueueItem* item = Items[index];
        ret = item->HasErrorToSolve(canSkip, canRetry);
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return ret;
}

CFTPQueueItem*
CFTPQueue::FindItemWithUID(int UID)
{
    if (LastFoundUID == UID && LastFoundIndex < Items.Count &&
        Items[LastFoundIndex]->UID == LastFoundUID)
    {
        return Items[LastFoundIndex]; // found in the cache; no need to traverse the entire array
    }
    else
    {
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CFTPQueueItem* item = Items[i];
            if (item->UID == UID)
            {
                LastFoundUID = UID;
                LastFoundIndex = i;
                return item;
            }
        }
    }
    return NULL;
}

void CFTPQueue::HandleFirstWaitingItemIndex(BOOL exploreOrResolveItem, int itemIndex)
{
    if (!GetOnlyExploreAndResolveItems && exploreOrResolveItem)
    {
        GetOnlyExploreAndResolveItems = TRUE;
        FirstWaitingItemIndex = itemIndex;
    }
    else
    {
        if ((!GetOnlyExploreAndResolveItems || exploreOrResolveItem) &&
            FirstWaitingItemIndex > itemIndex) // FirstWaitingItemIndex needs to be updated
        {
            FirstWaitingItemIndex = itemIndex;
        }
    }
}

int CFTPQueue::SkipItem(int UID, CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::SkipItem()");

    int ret = -1;
    HANDLES(EnterCriticalSection(&QueueCritSect));
    CFTPQueueItem* found = FindItemWithUID(UID);
    if (found != NULL)
    {
        BOOL canSkip;
        found->HasErrorToSolve(&canSkip, NULL);
        if (canSkip)
        {
            ret = LastFoundIndex;
            found->ChangeStateAndCounters(sqisSkipped, oper, this);
            if (found->ProblemID == ITEMPR_OK)
                found->ProblemID = ITEMPR_SKIPPEDBYUSER;
            found->ForceAction = fqiaNone; // just to be safe (probably not needed)
        }
        else
            TRACE_I("CFTPQueue::SkipItem(): cannot skip item because it's not in any of skip-possible states!");
    }
    else
        TRACE_I("CFTPQueue::SkipItem(): cannot skip item because it's not found!");
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return ret;
}

int CFTPQueue::RetryItem(int UID, CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::RetryItem()");

    int ret = -1;
    HANDLES(EnterCriticalSection(&QueueCritSect));
    CFTPQueueItem* found = FindItemWithUID(UID);
    if (found != NULL)
    {
        BOOL canRetry;
        found->HasErrorToSolve(NULL, &canRetry);
        if (canRetry)
        {
            if (found->Type == fqitUploadCopyExploreDir || found->Type == fqitUploadMoveExploreDir ||
                found->Type == fqitUploadCopyFile || found->Type == fqitUploadMoveFile)
            {
                char hostBuf[HOST_MAX_SIZE];
                char userBuf[USER_MAX_SIZE];
                unsigned short portBuf;
                oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                UploadListingCache.RemoveNotAccessibleListings(userBuf, hostBuf, portBuf);
            }
            CFTPQueueItemState newState = sqisWaiting;
            if (found->Type == fqitDeleteDir || found->Type == fqitMoveDeleteDir ||
                found->Type == fqitUploadMoveDeleteDir || found->Type == fqitMoveDeleteDirLink ||
                found->Type == fqitChAttrsDir)
            {
                newState = ((CFTPQueueItemDir*)found)->GetStateFromCounters();
            }
            if (newState == sqisWaiting)
                HandleFirstWaitingItemIndex(found->IsExploreOrResolveItem(), LastFoundIndex);
            ret = LastFoundIndex;
            found->ChangeStateAndCounters(newState, oper, this);
            if (found->ProblemID == ITEMPR_UNABLETORESUME)
                oper->SetResumeIsNotSupported(FALSE); // so Retry makes any sense at all
            oper->SetSizeCmdIsSupported(TRUE);        // so that the SIZE command is tried again if needed
            found->ProblemID = ITEMPR_OK;
            found->WinError = NO_ERROR;
            if (found->ErrAllocDescr != NULL)
            {
                SalamanderGeneral->Free(found->ErrAllocDescr);
                found->ErrAllocDescr = NULL;
            }
            found->ForceAction = fqiaNone; // to ensure Autorename is not performed instead of Retry
        }
        else
            TRACE_I("CFTPQueue::RetryItem(): cannot retry item because it's not in any of error states!");
    }
    else
        TRACE_I("CFTPQueue::RetryItem(): cannot retry item because it's not found!");
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return ret;
}

int CFTPQueue::SolveErrorOnItem(HWND parent, int UID, CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::SolveErrorOnItem()");

    int openDlgWithID = 0;
    char ftpPath[FTP_MAX_PATH];
    ftpPath[0] = 0;
    char ftpName[FTP_MAX_PATH];
    ftpName[0] = 0;
    char diskPath[MAX_PATH];
    diskPath[0] = 0;
    char diskName[MAX_PATH];
    diskName[0] = 0;
    char* newName = NULL;
    char origRightsBuf[100];
    origRightsBuf[0] = 0;
    char* origRights = NULL;
    WORD newAttr = 0;
    DWORD winError = NO_ERROR;
    BOOL applyToAll = FALSE;
    char errDescrBuf[500];
    errDescrBuf[0] = 0;
    BOOL isUploadItem = FALSE;
    char hostBuf[HOST_MAX_SIZE];
    char userBuf[USER_MAX_SIZE];
    unsigned short portBuf;

    HANDLES(EnterCriticalSection(&QueueCritSect));
    CFTPQueueItem* found = FindItemWithUID(UID);
    if (found != NULL)
    {
        if (found->HasErrorToSolve(NULL, NULL))
        {
            if (found->Type == fqitUploadCopyExploreDir || found->Type == fqitUploadMoveExploreDir)
            {
                isUploadItem = TRUE;
                lstrcpyn(ftpPath, ((CFTPQueueItemCopyMoveUploadExplore*)found)->TgtPath, FTP_MAX_PATH);
                lstrcpyn(ftpName, ((CFTPQueueItemCopyMoveUploadExplore*)found)->TgtName, FTP_MAX_PATH);
            }
            else
            {
                if (found->Type == fqitUploadCopyFile || found->Type == fqitUploadMoveFile)
                {
                    isUploadItem = TRUE;
                    lstrcpyn(ftpPath, ((CFTPQueueItemCopyOrMoveUpload*)found)->TgtPath, FTP_MAX_PATH);
                    lstrcpyn(ftpName, ((CFTPQueueItemCopyOrMoveUpload*)found)->TgtName, FTP_MAX_PATH);
                }
                else
                {
                    lstrcpyn(ftpPath, found->Path, FTP_MAX_PATH);
                    lstrcpyn(ftpName, found->Name, FTP_MAX_PATH);
                }
            }
            switch (found->ProblemID)
            {
            case ITEMPR_LOWMEM:
            {
                if (isUploadItem)
                {
                    lstrcpyn(diskPath, found->Path, MAX_PATH);
                    lstrcpyn(diskName, found->Name, MAX_PATH);
                }
                openDlgWithID = 1;
                break;
            }

            case ITEMPR_UNABLETOCWD:
            case ITEMPR_UNABLETOCWDONLYPATH:
            case ITEMPR_UNABLETOPWD:
            case ITEMPR_UNABLETORESOLVELNK:
            case ITEMPR_UNABLETODELETEFILE:
            case ITEMPR_UNABLETODELETEDIR:
            case ITEMPR_UNABLETOCHATTRS:
            case ITEMPR_UPLOADCANNOTLISTTGTPATH:
            case ITEMPR_UPLOADCANNOTLISTSRCPATH:
            case ITEMPR_UNABLETODELETEDISKDIR:
            case ITEMPR_UPLOADCANNOTOPENSRCFILE:
            case ITEMPR_SRCFILEREADERROR:
            case ITEMPR_UNABLETODELETEDISKFILE:
            {
                if (found->ProblemID == ITEMPR_UNABLETOCWDONLYPATH ||
                    found->ProblemID == ITEMPR_UPLOADCANNOTLISTTGTPATH)
                {
                    ftpName[0] = 0;
                }
                if (found->ProblemID == ITEMPR_UPLOADCANNOTLISTSRCPATH ||
                    found->ProblemID == ITEMPR_UNABLETODELETEDISKDIR ||
                    found->ProblemID == ITEMPR_UPLOADCANNOTOPENSRCFILE ||
                    found->ProblemID == ITEMPR_SRCFILEREADERROR ||
                    found->ProblemID == ITEMPR_UNABLETODELETEDISKFILE)
                {
                    ftpPath[0] = 0;
                    ftpName[0] = 0;
                    lstrcpyn(diskPath, found->Path, MAX_PATH);
                    lstrcpyn(diskName, found->Name, MAX_PATH);
                }
                openDlgWithID = found->ProblemID == ITEMPR_UNABLETOPWD ? 13 : found->ProblemID == ITEMPR_UNABLETOCWD || found->ProblemID == ITEMPR_UNABLETOCWDONLYPATH ? 12
                                                                          : found->ProblemID == ITEMPR_UNABLETORESOLVELNK                                              ? 17
                                                                          : found->ProblemID == ITEMPR_UNABLETODELETEFILE                                              ? 18
                                                                          : found->ProblemID == ITEMPR_UNABLETODELETEDIR                                               ? 19
                                                                          : found->ProblemID == ITEMPR_UNABLETOCHATTRS                                                 ? 20
                                                                          : found->ProblemID == ITEMPR_UPLOADCANNOTLISTTGTPATH                                         ? 30
                                                                          : found->ProblemID == ITEMPR_UPLOADCANNOTLISTSRCPATH                                         ? 31
                                                                          : found->ProblemID == ITEMPR_UNABLETODELETEDISKDIR                                           ? 33
                                                                          : found->ProblemID == ITEMPR_UPLOADCANNOTOPENSRCFILE                                         ? 35
                                                                          : found->ProblemID == ITEMPR_SRCFILEREADERROR                                                ? 39
                                                                                                                                                                       : 41;
                if (found->ErrAllocDescr != NULL)
                    lstrcpyn(errDescrBuf, found->ErrAllocDescr, 500);
                else
                {
                    if (found->WinError != NO_ERROR)
                        FTPGetErrorText(found->WinError, errDescrBuf, 500);
                    else
                    {
                        if (found->ProblemID == ITEMPR_UPLOADCANNOTLISTTGTPATH)
                            lstrcpyn(errDescrBuf, LoadStr(IDS_OPERDOPPR_UPLCANTLISTTGTPATH), 500);
                        else
                            lstrcpyn(errDescrBuf, LoadStr(IDS_UNKNOWNERROR), 500);
                    }
                }
                break;
            }

            case ITEMPR_INCOMPLETELISTING:
            {
                openDlgWithID = 14;
                if (found->ErrAllocDescr != NULL)
                    lstrcpyn(errDescrBuf, found->ErrAllocDescr, 500);
                else
                {
                    if (found->WinError != NO_ERROR)
                        FTPGetErrorText(found->WinError, errDescrBuf, 500);
                    else
                        lstrcpyn(errDescrBuf, LoadStr(IDS_UNKNOWNERROR), 500);
                }
                break;
            }

            case ITEMPR_LISTENFAILURE:
            {
                if (found->Type == fqitUploadCopyFile || found->Type == fqitUploadMoveFile ||
                    found->Type == fqitUploadCopyExploreDir || found->Type == fqitUploadMoveExploreDir)
                {
                    openDlgWithID = 49;
                    lstrcpyn(diskPath, found->Path, MAX_PATH);
                    lstrcpyn(diskName, found->Name, MAX_PATH);
                }
                else
                    openDlgWithID = 15;
                if (found->WinError != NO_ERROR)
                    FTPGetErrorText(found->WinError, errDescrBuf, 500);
                else
                    lstrcpyn(errDescrBuf, LoadStr(IDS_UNKNOWNERROR), 500);
                break;
            }

            case ITEMPR_UNABLETOPARSELISTING:
                openDlgWithID = 16;
                break;
            case ITEMPR_TGTFILEINUSE:
                openDlgWithID = 37;
                break;
            case ITEMPR_SRCFILEINUSE:
                openDlgWithID = 50;
                break;

            case ITEMPR_UNABLETORESUME:
            case ITEMPR_RESUMETESTFAILED:
            {
                if (found->Type == fqitCopyFileOrFileLink || found->Type == fqitMoveFileOrFileLink)
                {
                    lstrcpyn(diskPath, ((CFTPQueueItemCopyOrMove*)found)->TgtPath, MAX_PATH);
                    lstrcpyn(diskName, ((CFTPQueueItemCopyOrMove*)found)->TgtName, MAX_PATH);
                }
                if (found->Type == fqitUploadCopyFile || found->Type == fqitUploadMoveFile)
                {
                    lstrcpyn(diskPath, found->Path, MAX_PATH);
                    lstrcpyn(diskName, found->Name, MAX_PATH);
                }
                lstrcpyn(errDescrBuf, LoadStr(found->ProblemID == ITEMPR_UNABLETORESUME ? IDS_SSCD2_UNABLETORESUME : IDS_SSCD2_RESUMETESTFAILED), 500);
                openDlgWithID = found->ProblemID == ITEMPR_UNABLETORESUME ? (found->Type == fqitUploadCopyFile ||
                                                                                     found->Type == fqitUploadMoveFile
                                                                                 ? 45
                                                                                 : 21)
                                                                          : 22;
                break;
            }

            case ITEMPR_TGTFILEREADERROR:
            case ITEMPR_TGTFILEWRITEERROR:
            {
                if (found->Type == fqitCopyFileOrFileLink || found->Type == fqitMoveFileOrFileLink)
                {
                    lstrcpyn(diskPath, ((CFTPQueueItemCopyOrMove*)found)->TgtPath, MAX_PATH);
                    lstrcpyn(diskName, ((CFTPQueueItemCopyOrMove*)found)->TgtName, MAX_PATH);
                }
                if (found->WinError != NO_ERROR)
                    FTPGetErrorText(found->WinError, errDescrBuf, 500);
                else
                    lstrcpyn(errDescrBuf, LoadStr(IDS_UNKNOWNERROR), 500);
                openDlgWithID = found->ProblemID == ITEMPR_TGTFILEREADERROR ? 23 : 24;
                break;
            }

            case ITEMPR_INCOMPLETEDOWNLOAD:
            case ITEMPR_UNABLETODELSRCFILE:
            case ITEMPR_INCOMPLETEUPLOAD:
            case ITEMPR_UPLOADASCIIRESUMENOTSUP:
            case ITEMPR_UPLOADUNABLETORESUMEUNKSIZ:
            case ITEMPR_UPLOADUNABLETORESUMEBIGTGT:
            case ITEMPR_UPLOADTESTIFFINISHEDNOTSUP:
            {
                if (found->Type == fqitCopyFileOrFileLink || found->Type == fqitMoveFileOrFileLink)
                {
                    lstrcpyn(diskPath, ((CFTPQueueItemCopyOrMove*)found)->TgtPath, MAX_PATH);
                    lstrcpyn(diskName, ((CFTPQueueItemCopyOrMove*)found)->TgtName, MAX_PATH);
                }
                if (found->Type == fqitUploadCopyFile || found->Type == fqitUploadMoveFile)
                {
                    lstrcpyn(diskPath, found->Path, MAX_PATH);
                    lstrcpyn(diskName, found->Name, MAX_PATH);
                }
                switch (found->ProblemID)
                {
                case ITEMPR_UPLOADASCIIRESUMENOTSUP:
                    lstrcpyn(errDescrBuf, LoadStr(IDS_UPLERR_UPLCANTRESUMINASC), 500);
                    break;
                case ITEMPR_UPLOADUNABLETORESUMEUNKSIZ:
                    lstrcpyn(errDescrBuf, LoadStr(IDS_OPERDOPPR_UPUNABLERESUNKSIZ), 500);
                    break;
                case ITEMPR_UPLOADUNABLETORESUMEBIGTGT:
                    lstrcpyn(errDescrBuf, LoadStr(IDS_UPLERR_UPLCANTRESUMBIGTGT), 500);
                    break;
                case ITEMPR_UPLOADTESTIFFINISHEDNOTSUP:
                    lstrcpyn(errDescrBuf, LoadStr(IDS_UPLERR_UPLTESTIFFINNOTSUP), 500);
                    break;

                default:
                {
                    if (found->ErrAllocDescr != NULL)
                        lstrcpyn(errDescrBuf, found->ErrAllocDescr, 500);
                    else
                    {
                        if (found->WinError != NO_ERROR)
                            FTPGetErrorText(found->WinError, errDescrBuf, 500);
                        else
                            lstrcpyn(errDescrBuf, LoadStr(IDS_UNKNOWNERROR), 500);
                    }
                    break;
                }
                }
                openDlgWithID = found->ProblemID == ITEMPR_INCOMPLETEDOWNLOAD ? 25 : found->ProblemID == ITEMPR_UNABLETODELSRCFILE       ? 26
                                                                                 : found->ProblemID == ITEMPR_INCOMPLETEUPLOAD           ? 40
                                                                                 : found->ProblemID == ITEMPR_UPLOADASCIIRESUMENOTSUP    ? 44
                                                                                 : found->ProblemID == ITEMPR_UPLOADUNABLETORESUMEUNKSIZ ? 46
                                                                                 : found->ProblemID == ITEMPR_UPLOADUNABLETORESUMEBIGTGT ? 47
                                                                                                                                         : 51;
                break;
            }

            default:
            {
                switch (found->Type)
                {
                case fqitDeleteExploreDir: // explore a directory for delete (note: directory links are removed as a whole, the operation objective is met and nothing "extra" is deleted) (object of class CFTPQueueItemDelExplore)
                {
                    if (found->ProblemID == ITEMPR_DIRISHIDDEN)
                        openDlgWithID = 9;
                    else if (found->ProblemID == ITEMPR_DIRISNOTEMPTY)
                        openDlgWithID = 11;
                    break;
                }

                case fqitDeleteLink: // delete for a link (object of class CFTPQueueItemDel)
                case fqitDeleteFile: // delete for a file (object of class CFTPQueueItemDel)
                {
                    if (found->ProblemID == ITEMPR_FILEISHIDDEN)
                        openDlgWithID = 10;
                    break;
                }
                    /*
            case fqitCopyResolveLink:        // copying: determine whether it is a link to a file or a directory (object of class CFTPQueueItemCopyOrMove)
            case fqitMoveResolveLink:        // move: determine whether it is a link to a file or a directory (object of class CFTPQueueItemCopyOrMove)
            case fqitDeleteDir:              // delete for a directory (object of class CFTPQueueItemDir)
            case fqitMoveDeleteDir:          // delete a directory after moving its contents (object of class CFTPQueueItemDir)
            case fqitMoveDeleteDirLink:      // delete a directory link after moving its contents (object of class CFTPQueueItemDir)
            case fqitChAttrsExploreDir:      // explore a directory to change attributes (also adds an item to change the directory attributes) (object of class CFTPQueueItemChAttrExplore)
            case fqitChAttrsResolveLink:     // change attributes: determine whether it is a link to a directory (object of class CFTPQueueItem)
            case fqitChAttrsExploreDirLink:  // explore a link to a directory to change attributes (object of class CFTPQueueItem)
              break;
*/
                case fqitChAttrsFile: // change file attributes (note: attributes cannot be changed on links) (object of class CFTPQueueItemChAttr)
                case fqitChAttrsDir:  // change directory attributes (object of class CFTPQueueItemChAttrDir)
                {
                    if (found->ProblemID == ITEMPR_UNKNOWNATTRS)
                    {
                        if (found->Type == fqitChAttrsFile)
                        {
                            if (((CFTPQueueItemChAttr*)found)->OrigRights != NULL)
                            {
                                lstrcpyn(origRightsBuf, ((CFTPQueueItemChAttr*)found)->OrigRights, 100);
                                origRights = origRightsBuf;
                            }
                            newAttr = ((CFTPQueueItemChAttr*)found)->Attr;
                        }
                        else // fqitChAttrsDir
                        {
                            if (((CFTPQueueItemChAttrDir*)found)->OrigRights != NULL)
                            {
                                lstrcpyn(origRightsBuf, ((CFTPQueueItemChAttrDir*)found)->OrigRights, 100);
                                origRights = origRightsBuf;
                            }
                            newAttr = ((CFTPQueueItemChAttrDir*)found)->Attr;
                        }
                        openDlgWithID = 8;
                    }
                    break;
                }

                case fqitCopyExploreDir:     // explore a directory or a link to a directory for copying (object of class CFTPQueueItemCopyMoveExplore)
                case fqitMoveExploreDir:     // explore a directory for moving (deletes the directory after completion) (object of class CFTPQueueItemCopyMoveExplore)
                case fqitMoveExploreDirLink: // explore a link to a directory for moving (deletes the link to the directory after completion) (object of class CFTPQueueItemCopyMoveExplore)
                {
                    if (((CFTPQueueItemCopyMoveExplore*)found)->TgtDirState == TGTDIRSTATE_UNKNOWN)
                    {
                        lstrcpyn(diskPath, ((CFTPQueueItemCopyMoveExplore*)found)->TgtPath, MAX_PATH);
                        lstrcpyn(diskName, ((CFTPQueueItemCopyMoveExplore*)found)->TgtName, MAX_PATH);
                        winError = found->WinError;

                        switch (found->ProblemID)
                        {
                        case ITEMPR_CANNOTCREATETGTDIR:
                            openDlgWithID = 2;
                            break;
                        case ITEMPR_TGTDIRALREADYEXISTS:
                            openDlgWithID = 3;
                            break;
                        }
                    }
                    break;
                }

                case fqitUploadCopyExploreDir: // upload: explore a directory for copying (object of class CFTPQueueItemCopyMoveUploadExplore)
                case fqitUploadMoveExploreDir: // upload: explore a directory for moving (deletes the directory after completion) (object of class CFTPQueueItemCopyMoveUploadExplore)
                {
                    if (((CFTPQueueItemCopyMoveUploadExplore*)found)->TgtDirState == UPLOADTGTDIRSTATE_UNKNOWN)
                    {
                        lstrcpyn(diskPath, found->Path, MAX_PATH);
                        lstrcpyn(diskName, found->Name, MAX_PATH);

                        switch (found->ProblemID)
                        {
                        case ITEMPR_UPLOADCANNOTCREATETGTDIR:
                        {
                            if (found->ErrAllocDescr != NULL)
                                lstrcpyn(errDescrBuf, found->ErrAllocDescr, 500);
                            else
                            {
                                if (found->WinError == ERROR_ALREADY_EXISTS)
                                    lstrcpyn(errDescrBuf, LoadStr(IDS_UPLERR_CANTCRTGTDIRFILEEX), 500);
                                else
                                    lstrcpyn(errDescrBuf, LoadStr(IDS_UPLERR_CANTCRTGTDIRINV), 500);
                            }
                            openDlgWithID = 28;
                            break;
                        }

                        case ITEMPR_UPLOADTGTDIRALREADYEXISTS:
                            openDlgWithID = 29;
                            break;

                        case ITEMPR_UPLOADCRDIRAUTORENFAILED:
                        {
                            if (found->ErrAllocDescr != NULL)
                                lstrcpyn(errDescrBuf, found->ErrAllocDescr, 500);
                            openDlgWithID = 32;
                            break;
                        }
                        }
                    }
                    break;
                }

                case fqitCopyFileOrFileLink: // copying a file or a link to a file (object of class CFTPQueueItemCopyOrMove)
                case fqitMoveFileOrFileLink: // moving a file or a link to a file (object of class CFTPQueueItemCopyOrMove)
                {
                    if (((CFTPQueueItemCopyOrMove*)found)->TgtFileState != TGTFILESTATE_TRANSFERRED)
                    {
                        lstrcpyn(diskPath, ((CFTPQueueItemCopyOrMove*)found)->TgtPath, MAX_PATH);
                        lstrcpyn(diskName, ((CFTPQueueItemCopyOrMove*)found)->TgtName, MAX_PATH);
                        winError = found->WinError;

                        switch (found->ProblemID)
                        {
                        case ITEMPR_CANNOTCREATETGTFILE:
                            openDlgWithID = 4;
                            break;
                        case ITEMPR_TGTFILEALREADYEXISTS:
                            openDlgWithID = 5;
                            break;
                        case ITEMPR_RETRYONCREATFILE:
                            openDlgWithID = 6;
                            break;
                        case ITEMPR_RETRYONRESUMFILE:
                            openDlgWithID = 7;
                            break;
                        case ITEMPR_ASCIITRFORBINFILE:
                            openDlgWithID = 27;
                            break;
                        }
                    }
                    break;
                }

                case fqitUploadCopyFile: // upload: copying a file (object of class CFTPQueueItemCopyOrMoveUpload)
                case fqitUploadMoveFile: // upload: moving a file (object of class CFTPQueueItemCopyOrMoveUpload)
                {
                    if (((CFTPQueueItemCopyOrMoveUpload*)found)->TgtFileState != UPLOADTGTFILESTATE_TRANSFERRED)
                    {
                        lstrcpyn(diskPath, found->Path, MAX_PATH);
                        lstrcpyn(diskName, found->Name, MAX_PATH);

                        switch (found->ProblemID)
                        {
                        case ITEMPR_UPLOADCANNOTCREATETGTFILE:
                        {
                            if (found->ErrAllocDescr == NULL)
                            {
                                lstrcpyn(errDescrBuf, LoadStr(found->WinError == ERROR_ALREADY_EXISTS ? IDS_UPLERR_CANTCRTGTFILEDIREX : IDS_UPLERR_CANTCRTGTFILEINV), 500);
                            }
                            else
                                lstrcpyn(errDescrBuf, found->ErrAllocDescr, 500);
                            openDlgWithID = 34;
                            break;
                        }

                        case ITEMPR_UPLOADTGTFILEALREADYEXISTS:
                            openDlgWithID = 36;
                            break;
                        case ITEMPR_ASCIITRFORBINFILE:
                            openDlgWithID = 38;
                            break;
                        case ITEMPR_RETRYONCREATFILE:
                            openDlgWithID = 42;
                            break;
                        case ITEMPR_RETRYONRESUMFILE:
                            openDlgWithID = 43;
                            break;

                        case ITEMPR_UPLOADFILEAUTORENFAILED:
                        {
                            if (found->ErrAllocDescr != NULL)
                                lstrcpyn(errDescrBuf, found->ErrAllocDescr, 500);
                            openDlgWithID = 48;
                            break;
                        }
                        }
                    }
                    break;
                }
                }
                break;
            }
            }
        }
        else
        {
            TRACE_I("CFTPQueue::SolveErrorOnItem(): cannot solve error on item because it's not in any of error states!");
            openDlgWithID = -1;
        }
    }
    else
    {
        TRACE_I("CFTPQueue::SolveErrorOnItem(): cannot solve error on item because it's not found!");
        openDlgWithID = -1;
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));

    int ret = -2; // no change
    if (openDlgWithID > 0)
    {
        // open the Solve Error dialog
        INT_PTR dlgResult = IDCANCEL;
        switch (openDlgWithID)
        {
        case 1: // insufficient memory
        {
            CSolveLowMemoryErr dlg(parent, isUploadItem ? diskPath : ftpPath, isUploadItem ? diskName : ftpName, &applyToAll);
            dlgResult = dlg.Execute();
            break;
        }

        case 2: // error creating the target directory
        {
            CSolveItemErrorDlg dlg(parent, oper, winError, NULL, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName, sidtCannotCreateTgtDir);
            dlgResult = dlg.Execute();
            break;
        }

        case 3: // target directory already exists
        {
            CSolveItemErrorDlg dlg(parent, oper, winError, NULL, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName, sidtTgtDirAlreadyExists);
            dlgResult = dlg.Execute();
            break;
        }

        case 4: // error creating the target file
        {
            CSolveItemErrorDlg dlg(parent, oper, winError, NULL, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName, sidtCannotCreateTgtFile);
            dlgResult = dlg.Execute();
            break;
        }

        case 5: // target file already exists
        {
            CSolveItemErrorDlg dlg(parent, oper, winError, NULL, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName, sidtTgtFileAlreadyExists);
            dlgResult = dlg.Execute();
            break;
        }

        case 6:  // file transfer error, the file was created or overwritten or resumed with overwrite allowed
        case 42: // upload: file transfer error, the file was created or overwritten or resumed with overwrite allowed
        {
            CSolveItemErrorDlg dlg(parent, oper, winError, NULL, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName,
                                   openDlgWithID == 6 ? sidtTransferFailedOnCreatedFile : sidtUploadTransferFailedOnCreatedFile);
            dlgResult = dlg.Execute();
            break;
        }

        case 7:  // file transfer error, the file was resumed without the option to overwrite
        case 43: // upload: file transfer error, the file was resumed without the option to overwrite
        {
            CSolveItemErrorDlg dlg(parent, oper, winError, NULL, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName,
                                   openDlgWithID == 7 ? sidtTransferFailedOnResumedFile : sidtUploadTransferFailedOnResumedFile);
            dlgResult = dlg.Execute();
            break;
        }

        case 8: // error: the file/directory has unknown attributes we cannot preserve (permissions other than 'r'+'w'+'x')
        {
            CSolveItemErrUnkAttrDlg dlg(parent, oper, ftpPath, ftpName, origRights, newAttr, &applyToAll);
            dlgResult = dlg.Execute();
            if (dlgResult == CM_SIEA_SETNEWATTRS)
            {
                applyToAll = FALSE; // normally attributes are set only for one file/directory (not one set of attributes for all items)
                CSolveItemSetNewAttrDlg dlg2(parent, oper, ftpPath, ftpName, origRights, &newAttr, &applyToAll);
                if (dlg2.Execute() == IDCANCEL)
                    dlgResult = IDCANCEL;
            }
            break;
        }

        case 9: // delete directory error: the directory is hidden (more a confirmation than an error)
        {
            CSolveItemErrorSimpleDlg dlg(parent, oper, ftpPath, ftpName,
                                         &applyToAll, sisdtDelHiddenDir);
            dlgResult = dlg.Execute();
            break;
        }

        case 10: // delete file or link error: the file is hidden (more a confirmation than an error)
        {
            CSolveItemErrorSimpleDlg dlg(parent, oper, ftpPath, ftpName,
                                         &applyToAll, sisdtDelHiddenFile);
            dlgResult = dlg.Execute();
            break;
        }

        case 11: // delete directory error: the directory is not empty (more a confirmation than an error)
        {
            CSolveItemErrorSimpleDlg dlg(parent, oper, ftpPath, ftpName,
                                         &applyToAll, sisdtDelNonEmptyDir);
            dlgResult = dlg.Execute();
            break;
        }

        case 12: // error changing the working path on the server
        case 13: // error retrieving the working path on the server
        case 14: // unable to load the full list of files and directories from the server
        case 15: // error preparing to open the active data connection
        case 17: // error checking whether the link is a directory or a file
        case 18: // error while deleting a file or link
        case 19: // error while deleting a directory
        case 20: // error changing attributes (chmod)
        case 30: // upload: error listing the target path (cannot detect name collisions in the target directory)
        case 31: // upload: error listing the source path
        case 33: // upload: error deleting the emptied source directory (during Move)
        case 35: // upload: unable to open the source file
        case 39: // upload: error reading the source file
        case 41: // upload: unable to delete the source file on disk (Move)
        case 49: // upload: error preparing to open the active data connection
        {
            BOOL useDiskPathAndName = openDlgWithID == 31 || openDlgWithID == 33 || openDlgWithID == 35 ||
                                      openDlgWithID == 39 || openDlgWithID == 41 || openDlgWithID == 49;
            CSolveServerCmdErr dlg(parent,
                                   openDlgWithID == 12 ? IDS_SSCD_TITLE1 : openDlgWithID == 13                      ? IDS_SSCD_TITLE2
                                                                       : openDlgWithID == 14                        ? IDS_SSCD_TITLE3
                                                                       : openDlgWithID == 15 || openDlgWithID == 49 ? IDS_SSCD_TITLE4
                                                                       : openDlgWithID == 17                        ? IDS_SSCD_TITLE5
                                                                       : openDlgWithID == 18                        ? IDS_SSCD_TITLE7
                                                                       : openDlgWithID == 19                        ? IDS_SSCD_TITLE8
                                                                       : openDlgWithID == 30                        ? IDS_SSCD_TITLE9
                                                                       : openDlgWithID == 31                        ? IDS_SSCD_TITLE10
                                                                       : openDlgWithID == 33                        ? IDS_SSCD_TITLE8
                                                                       : openDlgWithID == 35                        ? IDS_SSCD_TITLE11
                                                                       : openDlgWithID == 39                        ? IDS_SSCD_TITLE12
                                                                       : openDlgWithID == 41                        ? IDS_SSCD2_TITLE5
                                                                                                                    : IDS_SSCD_TITLE6 /* openDlgWithID == 20 */,
                                   useDiskPathAndName ? diskPath : ftpPath,
                                   useDiskPathAndName ? diskName : ftpName,
                                   errDescrBuf, &applyToAll,
                                   openDlgWithID == 18 ? siscdtDeleteFile : openDlgWithID == 19 ? siscdtDeleteDir
                                                                                                : siscdtSimple);
            dlgResult = dlg.Execute();
            break;
        }

        case 16: // unknown format of the file and directory list (listing) from the server
        case 37: // target file or link is locked by another operation
        case 50: // source file or link is locked by another operation
        {
            CSolveLowMemoryErr dlg(parent, ftpPath, ftpName, &applyToAll, openDlgWithID == 16 ? IDS_SCRD_TITLE1 : openDlgWithID == 37 ? IDS_SCRD_TITLE2
                                                                                                                                      : IDS_SCRD_TITLE3);
            dlgResult = dlg.Execute();
            break;
        }

        case 21: // unable to resume (Copy/Move) - server does not support resuming
        case 22: // unable to resume (Copy/Move) - unexpected tail of file (file has changed)
        case 23: // error reading the target file
        case 24: // error writing the target file
        case 25: // Copy/Move: unable to retrieve file from server: %s
        case 26: // Move: unable to delete the source file
        case 44: // upload: resume in ASCII transfer mode is not supported (try resuming in binary mode)
        case 45: // upload: unable to resume (Copy/Move) - server does not support resuming
        case 46: // upload: unable to resume the file because the target file size is unknown
        case 47: // upload: unable to resume the file because the target file is larger than the source file
        case 51: // issue "unable to verify whether the file was uploaded successfully" (we sent the entire file and the server simply did not respond; the file is most likely OK but we cannot test it - reasons: ASCII transfer mode or we do not have the size in bytes (neither listing nor the SIZE command))
        {
            CSolveServerCmdErr2 dlg(parent,
                                    openDlgWithID == 21 || openDlgWithID == 22 || openDlgWithID == 44 ||
                                            openDlgWithID == 45 || openDlgWithID == 46 || openDlgWithID == 47
                                        ? IDS_SSCD2_TITLE1
                                    : openDlgWithID == 23 ? IDS_SSCD2_TITLE2
                                    : openDlgWithID == 24 ? IDS_SSCD2_TITLE3
                                    : openDlgWithID == 25 ? IDS_SSCD2_TITLE4
                                    : openDlgWithID == 26 ? IDS_SSCD2_TITLE5
                                                          : IDS_SSCD2_TITLE7,
                                    ftpPath, ftpName, diskPath,
                                    diskName, errDescrBuf, &applyToAll,
                                    openDlgWithID == 22 ? siscdt2ResumeTestFailed : openDlgWithID == 26                                                                    ? siscdt2Simple
                                                                                : openDlgWithID == 44 || openDlgWithID == 45 || openDlgWithID == 46 || openDlgWithID == 47 ? siscdt2UploadUnableToStore
                                                                                : openDlgWithID == 51                                                                      ? siscdt2UploadTestIfFinished
                                                                                                                                                                           : siscdt2ResumeFile);
            dlgResult = dlg.Execute();
            break;
        }

        case 27: // ASCII transfer mode for a binary file
        case 38: // upload: ASCII transfer mode for a binary file
        {
            CSolveItemErrorDlg dlg(parent, oper, NO_ERROR, NULL, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, NULL,
                                   openDlgWithID == 27 ? sidtASCIITrModeForBinFile : sidtUploadASCIITrModeForBinFile);
            dlgResult = dlg.Execute();
            break;
        }

        case 28: // upload: error creating the target directory
        {
            CSolveItemErrorDlg dlg(parent, oper, NO_ERROR, errDescrBuf, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName, sidtUploadCannotCreateTgtDir);
            dlgResult = dlg.Execute();
            break;
        }

        case 29: // upload: target directory already exists
        {
            CSolveItemErrorDlg dlg(parent, oper, NO_ERROR, NULL, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName, sidtUploadTgtDirAlreadyExists);
            dlgResult = dlg.Execute();
            break;
        }

        case 32: // upload: error creating the target directory with auto-rename (under a different name)
        case 48: // upload: error creating the target file with auto-rename (under a different name)
        case 40: // upload: unable to store file to server
        {
            CSolveItemErrorDlg dlg(parent, oper, NO_ERROR, errDescrBuf, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName,
                                   openDlgWithID == 32 ? sidtUploadCrDirAutoRenFailed : openDlgWithID == 48 ? sidtUploadFileAutoRenFailed
                                                                                                            : sidtUploadStoreFileFailed);
            dlgResult = dlg.Execute();
            break;
        }

        case 34: // upload: error creating the target file
        {
            CSolveItemErrorDlg dlg(parent, oper, NO_ERROR, errDescrBuf, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName, sidtUploadCannotCreateTgtFile);
            dlgResult = dlg.Execute();
            break;
        }

        case 36: // upload: target file already exists
        {
            CSolveItemErrorDlg dlg(parent, oper, NO_ERROR, NULL, ftpPath, ftpName, diskPath,
                                   diskName, &applyToAll, &newName, sidtUploadTgtFileAlreadyExists);
            dlgResult = dlg.Execute();
            break;
        }
        }

        if (dlgResult != IDCANCEL) // store the values entered by the user in the item
        {
            HANDLES(EnterCriticalSection(&QueueCritSect));
            found = FindItemWithUID(UID);
            if (found != NULL)
            {
                if (found->HasErrorToSolve(NULL, NULL))
                {
                    // save the dialog result to the items
                    CFTPQueueItem* item = found;
                    int itemIndex = LastFoundIndex;
                    int enumIndex = 0;
                    DWORD wantedProblemID = found->ProblemID;
                    BOOL notAccessibleListingsRemoved = FALSE;
                    oper->SetSizeCmdIsSupported(TRUE); // so that the SIZE command is tried again if needed
                    while (1)
                    {
                        if (!notAccessibleListingsRemoved &&
                            (item->Type == fqitUploadCopyExploreDir || item->Type == fqitUploadMoveExploreDir ||
                             item->Type == fqitUploadCopyFile || item->Type == fqitUploadMoveFile))
                        {
                            notAccessibleListingsRemoved = TRUE;
                            oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                            UploadListingCache.RemoveNotAccessibleListings(userBuf, hostBuf, portBuf);
                        }

                        if (openDlgWithID == 1 || openDlgWithID >= 12 && openDlgWithID <= 27 ||
                            openDlgWithID >= 30 && openDlgWithID <= 31 || openDlgWithID == 33 ||
                            openDlgWithID == 35 || openDlgWithID >= 37 && openDlgWithID <= 39 || openDlgWithID == 41 ||
                            openDlgWithID >= 44 && openDlgWithID <= 47 || openDlgWithID >= 49 && openDlgWithID <= 51)
                        {
                            switch (dlgResult)
                            {
                            case IDOK:                   // Retry or Use Binary Mode (only for openDlgWithID==27, 38)
                            case CM_SATR_RETRY:          // Retry (only for openDlgWithID==27, 38)
                            case CM_SATR_IGNORE:         // Ignore (only for openDlgWithID==27, 38)
                            case CM_SSCD_USEALTNAME:     // use alternate name (openDlgWithID: 21, 22, 23, 24, 25, 44, 45, 46, 47)
                            case CM_SSCD_RESUME:         // resume (openDlgWithID: 21, 22, 23, 24, 25, 44, 45, 46, 47)
                            case CM_SSCD_RESUMEOROVR:    // resume or overwrite (openDlgWithID: 21, 22, 23, 24, 25, 44, 45, 46, 47)
                            case CM_SSCD_OVERWRITE:      // overwrite (openDlgWithID: 21, 22, 23, 24, 25, 44, 45, 46, 47)
                            case CM_SSCD_REDUCEFILESIZE: // reduce file size and try to resume again (openDlgWithID: 22)
                            {
                                CFTPQueueItemState newState = sqisWaiting;
                                if (item->Type == fqitDeleteDir || item->Type == fqitMoveDeleteDir ||
                                    item->Type == fqitUploadMoveDeleteDir || item->Type == fqitMoveDeleteDirLink ||
                                    item->Type == fqitChAttrsDir)
                                {
                                    newState = ((CFTPQueueItemDir*)item)->GetStateFromCounters();
                                }
                                if (newState == sqisWaiting)
                                    HandleFirstWaitingItemIndex(item->IsExploreOrResolveItem(), itemIndex);

                                if (ret == -2)
                                    ret = itemIndex;
                                else
                                    ret = -1;

                                item->ChangeStateAndCounters(newState, oper, this);
                                if (openDlgWithID == 27 &&
                                    (item->Type == fqitCopyFileOrFileLink || item->Type == fqitMoveFileOrFileLink))
                                {
                                    if (dlgResult == IDOK) // Use Binary Mode (ASCII transfer mode for a binary file)
                                        UpdateAsciiTransferMode((CFTPQueueItemCopyOrMove*)item, FALSE);
                                    else
                                    {
                                        if (dlgResult == CM_SATR_IGNORE) // Ignore (ASCII transfer mode for a binary file)
                                            UpdateIgnoreAsciiTrModeForBinFile((CFTPQueueItemCopyOrMove*)item, TRUE);
                                    }
                                }
                                if (openDlgWithID == 38 &&
                                    (item->Type == fqitUploadCopyFile || item->Type == fqitUploadMoveFile))
                                {
                                    if (dlgResult == IDOK) // Use Binary Mode (ASCII transfer mode for a binary file)
                                        UpdateAsciiTransferMode((CFTPQueueItemCopyOrMoveUpload*)item, FALSE);
                                    else
                                    {
                                        if (dlgResult == CM_SATR_IGNORE) // Ignore (ASCII transfer mode for a binary file)
                                            UpdateIgnoreAsciiTrModeForBinFile((CFTPQueueItemCopyOrMoveUpload*)item, TRUE);
                                    }
                                }
                                if (openDlgWithID == 12 &&
                                    (item->Type == fqitUploadCopyExploreDir || item->Type == fqitUploadMoveExploreDir))
                                { // invalidate the target path listing (if an outdated listing containing the target directory is used, CWD into this directory reports "path not found" - with an up-to-date listing the directory is created via MKD)
                                    CFTPQueueItemCopyMoveUploadExplore* curItem = (CFTPQueueItemCopyMoveUploadExplore*)item;
                                    UpdateUploadTgtDirState(curItem, UPLOADTGTDIRSTATE_UNKNOWN);
                                    oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                                    CFTPServerPathType pathType = oper->GetFTPServerPathType(curItem->TgtPath);
                                    UploadListingCache.InvalidatePathListing(userBuf, hostBuf, portBuf, curItem->TgtPath, pathType);
                                }
                                if (found->ProblemID == ITEMPR_UNABLETORESUME)
                                    oper->SetResumeIsNotSupported(FALSE); // so Retry makes any sense at all
                                item->ProblemID = ITEMPR_OK;
                                item->WinError = NO_ERROR;
                                if (item->ErrAllocDescr != NULL)
                                {
                                    SalamanderGeneral->Free(item->ErrAllocDescr);
                                    item->ErrAllocDescr = NULL;
                                }
                                item->ForceAction = dlgResult == CM_SSCD_USEALTNAME ? fqiaUseAutorename : dlgResult == CM_SSCD_RESUME       ? fqiaResume
                                                                                                      : dlgResult == CM_SSCD_REDUCEFILESIZE ? fqiaReduceFileSizeAndResume
                                                                                                      : dlgResult == CM_SSCD_RESUMEOROVR    ? fqiaResumeOrOverwrite
                                                                                                      : dlgResult == CM_SSCD_OVERWRITE      ? fqiaOverwrite
                                                                                                                                            : fqiaNone;
                                break;
                            }

                            case IDB_SCRD_SKIP: // skip (openDlgWithID: 1, 16, 27, 37, 38, 50)
                            case IDB_SSCD_SKIP: // skip (openDlgWithID: 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 30, 31, 33, 44, 45, 46, 47)
                            {
                                if (ret == -2)
                                    ret = itemIndex;
                                else
                                    ret = -1;

                                item->ChangeStateAndCounters(sqisSkipped, oper, this);
                                item->ForceAction = fqiaNone; // just to be safe (probably not needed)
                                break;
                            }

                            case CM_SSCD_MARKASDELETED: // mark as deleted (openDlgWithID: 18, 19)
                            {
                                if (ret == -2)
                                    ret = itemIndex;
                                else
                                    ret = -1;

                                item->ChangeStateAndCounters(sqisDone, oper, this);
                                item->ProblemID = ITEMPR_OK;
                                item->WinError = NO_ERROR;
                                if (item->ErrAllocDescr != NULL)
                                {
                                    SalamanderGeneral->Free(item->ErrAllocDescr);
                                    item->ErrAllocDescr = NULL;
                                }
                                item->ForceAction = fqiaNone; // just to be safe (probably not needed)
                                break;
                            }

                            case CM_SSCD_MARKASUPLOADED: // mark file as successfully uploaded (openDlgWithID: 51)
                            {
                                if (ret == -2)
                                    ret = itemIndex;
                                else
                                    ret = -1;

                                ((CFTPQueueItemCopyOrMoveUpload*)item)->TgtFileState = UPLOADTGTFILESTATE_TRANSFERRED;
                                if (item->Type == fqitUploadMoveFile) // we still need to delete the source file
                                {
                                    HandleFirstWaitingItemIndex(item->IsExploreOrResolveItem(), itemIndex);
                                    item->ChangeStateAndCounters(sqisWaiting, oper, this);
                                }
                                else
                                    item->ChangeStateAndCounters(sqisDone, oper, this);
                                item->ProblemID = ITEMPR_OK;
                                item->ForceAction = fqiaNone; // just to be safe (probably not needed)
                                break;
                            }
                            }
                        }
                        else
                        {
                            switch (item->Type)
                            {
                            case fqitChAttrsFile: // change file attributes (note: attributes cannot be changed on links) (object of class CFTPQueueItemChAttr)
                            case fqitChAttrsDir:  // change directory attributes (object of class CFTPQueueItemChAttrDir)
                            {
                                if (openDlgWithID == 8) // error: the file/directory has unknown attributes we cannot preserve (permissions other than 'r'+'w'+'x')
                                {
                                    switch (dlgResult)
                                    {
                                    case IDOK:                // ignore
                                    case CM_SIEA_RETRY:       // retry
                                    case CM_SIEA_SETNEWATTRS: // wants to set a new attribute value
                                    {
                                        CFTPQueueItemState newState = sqisWaiting;
                                        if (item->Type == fqitChAttrsDir)
                                            newState = ((CFTPQueueItemDir*)item)->GetStateFromCounters();
                                        if (newState == sqisWaiting)
                                            HandleFirstWaitingItemIndex(item->IsExploreOrResolveItem(), itemIndex);

                                        if (ret == -2)
                                            ret = itemIndex;
                                        else
                                            ret = -1;

                                        if (dlgResult == IDOK) // ignore
                                        {
                                            if (item->Type == fqitChAttrsFile)
                                                ((CFTPQueueItemChAttr*)item)->AttrErr = FALSE;
                                            else
                                                ((CFTPQueueItemChAttrDir*)item)->AttrErr = FALSE; // fqitChAttrsDir
                                        }

                                        if (dlgResult == CM_SIEA_SETNEWATTRS) // wants to set a new attribute value
                                        {
                                            if (item->Type == fqitChAttrsFile)
                                            {
                                                ((CFTPQueueItemChAttr*)item)->Attr = newAttr;
                                                ((CFTPQueueItemChAttr*)item)->AttrErr = FALSE;
                                            }
                                            else // fqitChAttrsDir
                                            {
                                                ((CFTPQueueItemChAttrDir*)item)->Attr = newAttr;
                                                ((CFTPQueueItemChAttrDir*)item)->AttrErr = FALSE;
                                            }
                                        }
                                        item->ChangeStateAndCounters(newState, oper, this);
                                        item->ProblemID = ITEMPR_OK;
                                        item->WinError = NO_ERROR;
                                        if (item->ErrAllocDescr != NULL)
                                        {
                                            SalamanderGeneral->Free(item->ErrAllocDescr);
                                            item->ErrAllocDescr = NULL;
                                        }
                                        item->ForceAction = fqiaNone;
                                        break;
                                    }

                                    case IDB_SCRD_SKIP: // skip
                                    {
                                        if (ret == -2)
                                            ret = itemIndex;
                                        else
                                            ret = -1;

                                        item->ChangeStateAndCounters(sqisSkipped, oper, this);
                                        item->ForceAction = fqiaNone; // just to be safe (probably not needed)
                                        break;
                                    }
                                    }
                                    break;
                                }
                                break;
                            }

                            case fqitDeleteExploreDir: // explore a directory for delete (note: directory links are removed as a whole, the operation objective is met and nothing "extra" is deleted) (object of class CFTPQueueItemDelExplore)
                            case fqitDeleteLink:       // delete for a link (object of class CFTPQueueItemDel)
                            case fqitDeleteFile:       // delete for a file (object of class CFTPQueueItemDel)
                            {
                                if (openDlgWithID == 9 ||  // delete directory error: the directory is hidden (more a confirmation than an error)
                                    openDlgWithID == 10 || // delete file or link error: the file is hidden (more a confirmation than an error)
                                    openDlgWithID == 11)   // delete directory error: the directory is not empty (more a confirmation than an error)
                                {
                                    switch (dlgResult)
                                    {
                                    case IDOK:          // delete
                                    case CM_SISE_RETRY: // retry
                                    {
                                        HandleFirstWaitingItemIndex(item->IsExploreOrResolveItem(), itemIndex);

                                        if (ret == -2)
                                            ret = itemIndex;
                                        else
                                            ret = -1;

                                        if (dlgResult == IDOK) // delete
                                        {
                                            switch (item->Type)
                                            {
                                            case fqitDeleteExploreDir:
                                            {
                                                if (openDlgWithID == 9) // error: hidden directory
                                                    ((CFTPQueueItemDelExplore*)item)->IsHiddenDir = FALSE;
                                                else // error: directory not empty
                                                    ((CFTPQueueItemDelExplore*)item)->IsTopLevelDir = FALSE;
                                                break;
                                            }

                                            case fqitDeleteLink:
                                            case fqitDeleteFile:
                                                ((CFTPQueueItemDel*)item)->IsHiddenFile = FALSE;
                                                break;
                                            }
                                        }

                                        item->ChangeStateAndCounters(sqisWaiting, oper, this);
                                        item->ProblemID = ITEMPR_OK;
                                        item->WinError = NO_ERROR;
                                        if (item->ErrAllocDescr != NULL) // just to be safe (probably not needed)
                                        {
                                            SalamanderGeneral->Free(item->ErrAllocDescr);
                                            item->ErrAllocDescr = NULL;
                                        }
                                        item->ForceAction = fqiaNone;
                                        break;
                                    }

                                    case IDB_SISE_SKIP: // skip
                                    {
                                        if (ret == -2)
                                            ret = itemIndex;
                                        else
                                            ret = -1;

                                        item->ChangeStateAndCounters(sqisSkipped, oper, this);
                                        item->ForceAction = fqiaNone; // just to be safe (probably not needed)
                                        break;
                                    }
                                    }
                                    break;
                                }
                                break;
                            }

                            case fqitCopyExploreDir:       // explore a directory or a link to a directory for copying (object of class CFTPQueueItemCopyMoveExplore)
                            case fqitMoveExploreDir:       // explore a directory for moving (deletes the directory after completion) (object of class CFTPQueueItemCopyMoveExplore)
                            case fqitMoveExploreDirLink:   // explore a link to a directory for moving (deletes the link to the directory after completion) (object of class CFTPQueueItemCopyMoveExplore)
                            case fqitUploadCopyExploreDir: // upload: explore a directory for copying (object of class CFTPQueueItemCopyMoveUploadExplore)
                            case fqitUploadMoveExploreDir: // upload: explore a directory for moving (deletes the directory after completion) (object of class CFTPQueueItemCopyMoveUploadExplore)
                            {
                                switch (openDlgWithID)
                                {
                                case 2:  // error creating the target directory
                                case 3:  // target directory already exists
                                case 28: // upload: error creating the target directory
                                case 29: // upload: target directory already exists
                                case 32: // upload: error creating the target directory with auto-rename (under a different name)
                                {
                                    switch (dlgResult)
                                    {
                                    case IDOK:                   // retry
                                    case CM_SCRD_USEALTNAME:     // autorename
                                    case CM_SDEX_USEEXISTINGDIR: // use existing directory
                                    {
                                        if (newName != NULL)
                                        {
                                            if (item->Type == fqitUploadCopyExploreDir || item->Type == fqitUploadMoveExploreDir)
                                            {
                                                if (((CFTPQueueItemCopyMoveUploadExplore*)item)->TgtName != NULL)
                                                    SalamanderGeneral->Free(((CFTPQueueItemCopyMoveUploadExplore*)item)->TgtName);
                                                ((CFTPQueueItemCopyMoveUploadExplore*)item)->TgtName = newName;
                                            }
                                            else
                                            {
                                                if (((CFTPQueueItemCopyMoveExplore*)item)->TgtName != NULL)
                                                    SalamanderGeneral->Free(((CFTPQueueItemCopyMoveExplore*)item)->TgtName);
                                                ((CFTPQueueItemCopyMoveExplore*)item)->TgtName = newName;
                                            }
                                            newName = NULL;
                                        }

                                        HandleFirstWaitingItemIndex(item->IsExploreOrResolveItem(), itemIndex);
                                        if (ret == -2)
                                            ret = itemIndex;
                                        else
                                            ret = -1;

                                        if (openDlgWithID == 28 &&
                                            (item->Type == fqitUploadCopyExploreDir || item->Type == fqitUploadMoveExploreDir))
                                        { // invalidate the target path listing (if an outdated listing lacking the target directory is used, MKD for this directory reports "already exists" - with an up-to-date listing the change goes directly into this directory via CWD)
                                            CFTPQueueItemCopyMoveUploadExplore* curItem = (CFTPQueueItemCopyMoveUploadExplore*)item;
                                            UpdateUploadTgtDirState(curItem, UPLOADTGTDIRSTATE_UNKNOWN);
                                            oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                                            CFTPServerPathType pathType = oper->GetFTPServerPathType(curItem->TgtPath);
                                            UploadListingCache.InvalidatePathListing(userBuf, hostBuf, portBuf, curItem->TgtPath, pathType);
                                        }

                                        item->ChangeStateAndCounters(sqisWaiting, oper, this);
                                        item->ProblemID = ITEMPR_OK;
                                        item->WinError = NO_ERROR;
                                        if (item->ErrAllocDescr != NULL)
                                        {
                                            SalamanderGeneral->Free(item->ErrAllocDescr);
                                            item->ErrAllocDescr = NULL;
                                        }
                                        if (dlgResult == IDOK)
                                            item->ForceAction = fqiaNone; // just to be safe (probably not needed)
                                        else
                                        {
                                            if (dlgResult == CM_SCRD_USEALTNAME)
                                                item->ForceAction = fqiaUseAutorename; // force Autorename on the next attempt
                                            else
                                                item->ForceAction = fqiaUseExistingDir; // force Use Existing Directory on the next attempt
                                        }
                                        break;
                                    }

                                    case IDB_SCRD_SKIP: // skip
                                    {
                                        if (ret == -2)
                                            ret = itemIndex;
                                        else
                                            ret = -1;

                                        item->ChangeStateAndCounters(sqisSkipped, oper, this);
                                        item->ForceAction = fqiaNone; // just to be safe (probably not needed)
                                        break;
                                    }
                                    }
                                    break;
                                }
                                }
                                break;
                            }

                            case fqitCopyFileOrFileLink: // copying a file or a link to a file (object of class CFTPQueueItemCopyOrMove)
                            case fqitMoveFileOrFileLink: // moving a file or a link to a file (object of class CFTPQueueItemCopyOrMove)
                            case fqitUploadCopyFile:     // upload: copying a file (object of class CFTPQueueItemCopyOrMoveUpload)
                            case fqitUploadMoveFile:     // upload: moving a file (object of class CFTPQueueItemCopyOrMoveUpload)
                            {
                                switch (openDlgWithID)
                                {
                                case 4:  // error creating the target file
                                case 5:  // target file already exists
                                case 6:  // file transfer error, the file was created or overwritten or resumed with overwrite allowed
                                case 7:  // file transfer error, the file was resumed without the option to overwrite
                                case 34: // upload: error creating the target file
                                case 36: // upload: target file already exists
                                case 40: // upload: unable to store file to server
                                case 42: // upload: file transfer error, the file was created or overwritten or resumed with overwrite allowed
                                case 43: // upload: file transfer error, the file was resumed without the option to overwrite
                                case 48: // upload: unable to create the target file under any name
                                {
                                    switch (dlgResult)
                                    {
                                    case IDOK:                // retry
                                    case CM_SCRD_USEALTNAME:  // autorename
                                    case CM_SIED_RESUME:      // resume
                                    case CM_SIED_RESUMEOROVR: // resume or overwrite
                                    case CM_SIED_OVERWRITE:   // overwrite
                                        // case CM_SIED_OVERWRITEALL: // overwrite-all translates to overwrite + applyToAll==TRUE
                                        {
                                            if (newName != NULL)
                                            {
                                                if (item->Type == fqitCopyFileOrFileLink || item->Type == fqitMoveFileOrFileLink)
                                                {
                                                    if (((CFTPQueueItemCopyOrMove*)item)->TgtName != NULL)
                                                        SalamanderGeneral->Free(((CFTPQueueItemCopyOrMove*)item)->TgtName);
                                                    ((CFTPQueueItemCopyOrMove*)item)->TgtName = newName;
                                                }
                                                else
                                                {
                                                    if (((CFTPQueueItemCopyOrMoveUpload*)item)->TgtName != NULL)
                                                        SalamanderGeneral->Free(((CFTPQueueItemCopyOrMoveUpload*)item)->TgtName);
                                                    ((CFTPQueueItemCopyOrMoveUpload*)item)->TgtName = newName;
                                                }
                                                newName = NULL;
                                            }

                                            HandleFirstWaitingItemIndex(item->IsExploreOrResolveItem(), itemIndex);
                                            if (ret == -2)
                                                ret = itemIndex;
                                            else
                                                ret = -1;

                                            item->ChangeStateAndCounters(sqisWaiting, oper, this);
                                            item->ProblemID = ITEMPR_OK;
                                            item->WinError = NO_ERROR;
                                            if (item->ErrAllocDescr != NULL)
                                            {
                                                SalamanderGeneral->Free(item->ErrAllocDescr);
                                                item->ErrAllocDescr = NULL;
                                            }
                                            switch (dlgResult)
                                            {
                                            case IDOK:
                                                item->ForceAction = fqiaNone;
                                                break; // to make sure Retry is performed and not, for example, Autorename
                                            case CM_SCRD_USEALTNAME:
                                                item->ForceAction = fqiaUseAutorename;
                                                break; // force Autorename on the next attempt
                                            case CM_SIED_RESUME:
                                                item->ForceAction = fqiaResume;
                                                break; // force Resume on the next attempt
                                            case CM_SIED_RESUMEOROVR:
                                                item->ForceAction = fqiaResumeOrOverwrite;
                                                break; // force Resume or Overwrite on the next attempt
                                            case CM_SIED_OVERWRITE:
                                                item->ForceAction = fqiaOverwrite;
                                                break; // force Overwrite on the next attempt
                                                       // case CM_SIED_OVERWRITEALL: // overwrite-all translates to overwrite + applyToAll==TRUE
                                            }
                                            break;
                                        }

                                    case IDB_SCRD_SKIP: // skip
                                    {
                                        if (ret == -2)
                                            ret = itemIndex;
                                        else
                                            ret = -1;

                                        item->ChangeStateAndCounters(sqisSkipped, oper, this);
                                        item->ForceAction = fqiaNone; // just to be safe (probably not needed)
                                        break;
                                    }
                                    }
                                    break;
                                }
                                }
                                break;
                            }
                            }
                        }
                        if (!applyToAll)
                            break;
                        else
                        {
                            if (newName != NULL)
                            {
                                free(newName);
                                newName = NULL;
                            }
                            // try to find another item with the same error
                            BOOL nextItemFound = FALSE;
                            for (; enumIndex < Items.Count; enumIndex++)
                            {
                                item = Items[enumIndex];
                                if (item->HasErrorToSolve(NULL, NULL) &&
                                    item->ProblemID == wantedProblemID &&
                                    enumIndex != LastFoundIndex)
                                {
                                    itemIndex = enumIndex++;
                                    nextItemFound = TRUE;
                                    break;
                                }
                            }
                            if (!nextItemFound)
                                break; // no further item was found
                        }
                    }
                }
            }
            HANDLES(LeaveCriticalSection(&QueueCritSect));
        }
    }
    else
    {
        if (openDlgWithID == 0)
            TRACE_E("CFTPQueue::SolveErrorOnItem(): unknown type of error!");
    }
    if (newName != NULL)
        free(newName);
    return ret;
}

void CFTPQueue::UpdateItemState(CFTPQueueItem* item, CFTPQueueItemState state, DWORD problemID,
                                DWORD winError, char* errAllocDescr, CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateItemState()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    if (item != NULL)
    {
        if (state == sqisWaiting && FindItemWithUID(item->UID) != NULL) // item found ("always true")
            HandleFirstWaitingItemIndex(item->IsExploreOrResolveItem(), LastFoundIndex);

        item->ChangeStateAndCounters(state, oper, this); // item state change
        item->ProblemID = problemID;
        item->WinError = winError;
        if (item->ErrAllocDescr != NULL)
            SalamanderGeneral->Free(item->ErrAllocDescr); // free any previous value
        item->ErrAllocDescr = errAllocDescr;
    }
    else
    {
        if (errAllocDescr != NULL)
            free(errAllocDescr); // there is nowhere to store the value, so at least free it
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateRenamedName(CFTPQueueItemCopyOrMoveUpload* item, char* renamedName)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateRenamedName()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    if (item->RenamedName != NULL)
        free(item->RenamedName);
    item->RenamedName = renamedName;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateAutorenamePhase(CFTPQueueItemCopyOrMoveUpload* item, int autorenamePhase)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateAutorenamePhase()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->AutorenamePhase = autorenamePhase;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::ChangeTgtNameToRenamedName(CFTPQueueItemCopyOrMoveUpload* item)
{
    CALL_STACK_MESSAGE1("CFTPQueue::ChangeTgtNameToRenamedName()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    if (item->RenamedName != NULL)
    {
        SalamanderGeneral->Free(item->TgtName);
        item->TgtName = item->RenamedName;
        item->RenamedName = NULL;
    }
    else
        TRACE_E("Unexpected situation in CFTPQueue::ChangeTgtNameToRenamedName(): item->RenamedName == NULL");
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateTgtName(CFTPQueueItemCopyMoveExplore* item, char* tgtName)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateTgtName1()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    if (item->TgtName != NULL)
        SalamanderGeneral->Free(item->TgtName);
    item->TgtName = tgtName;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateTgtName(CFTPQueueItemCopyMoveUploadExplore* item, char* tgtName)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateTgtName2()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    if (item->TgtName != NULL)
        SalamanderGeneral->Free(item->TgtName);
    item->TgtName = tgtName;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateTgtName(CFTPQueueItemCopyOrMove* item, char* tgtName)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateTgtName3()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    if (item->TgtName != NULL)
        SalamanderGeneral->Free(item->TgtName);
    item->TgtName = tgtName;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateTgtDirState(CFTPQueueItemCopyMoveExplore* item, unsigned tgtDirState)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateTgtDirState()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->TgtDirState = tgtDirState;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateUploadTgtDirState(CFTPQueueItemCopyMoveUploadExplore* item, unsigned tgtDirState)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateUploadTgtDirState()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->TgtDirState = tgtDirState;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateForceAction(CFTPQueueItem* item, CFTPQueueItemAction forceAction)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateForceAction()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->ForceAction = forceAction;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateTgtFileState(CFTPQueueItemCopyOrMove* item, unsigned tgtFileState)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateTgtFileState()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->TgtFileState = tgtFileState;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateFileSize(CFTPQueueItemCopyOrMove* item, CQuadWord const& size,
                               BOOL sizeInBytes, CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateFileSize()");

    if (size == CQuadWord(-1, -1))
        TRACE_E("CFTPQueue::UpdateFileSize(): unexpected value of 'size' (-1)!");
    HANDLES(EnterCriticalSection(&QueueCritSect));
    if (item->Size != CQuadWord(-1, -1))
    {
        oper->SubFromTotalSize(item->Size, item->SizeInBytes);
        if (item->GetItemState() == sqisDone || item->GetItemState() == sqisSkipped)
        {
            if (item->SizeInBytes)
                DoneOrSkippedByteSize -= item->Size;
            else
                DoneOrSkippedBlockSize -= item->Size;
        }
        else
        {
            if (item->GetItemState() == sqisWaiting || item->GetItemState() == sqisProcessing ||
                item->GetItemState() == sqisDelayed)
            {
                if (item->SizeInBytes)
                    WaitingOrProcessingOrDelayedByteSize -= item->Size;
                else
                {
                    WaitingOrProcessingOrDelayedBlockSize -= item->Size;
                    CopyUnknownSizeCountIfUnknownBlockSize--;
                }
            }
            else
            {
                if (!item->SizeInBytes)
                    CopyUnknownSizeCountIfUnknownBlockSize--;
            }
        }
    }
    else
    {
        if (item->GetItemState() != sqisDone && item->GetItemState() != sqisSkipped)
            CopyUnknownSizeCount--;
    }
    item->Size = size;
    item->SizeInBytes = sizeInBytes;
    if (item->Size != CQuadWord(-1, -1))
    {
        oper->AddToTotalSize(item->Size, item->SizeInBytes);
        if (item->GetItemState() == sqisDone || item->GetItemState() == sqisSkipped)
        {
            if (item->SizeInBytes)
                DoneOrSkippedByteSize += item->Size;
            else
                DoneOrSkippedBlockSize += item->Size;
        }
        else
        {
            if (item->GetItemState() == sqisWaiting || item->GetItemState() == sqisProcessing ||
                item->GetItemState() == sqisDelayed)
            {
                if (item->SizeInBytes)
                    WaitingOrProcessingOrDelayedByteSize += item->Size;
                else
                {
                    WaitingOrProcessingOrDelayedBlockSize += item->Size;
                    CopyUnknownSizeCountIfUnknownBlockSize++;
                }
            }
            else
            {
                if (!item->SizeInBytes)
                    CopyUnknownSizeCountIfUnknownBlockSize++;
            }
        }
    }
    else
    {
        if (item->GetItemState() != sqisDone && item->GetItemState() != sqisSkipped)
            CopyUnknownSizeCount++;
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateTextFileSizes(CFTPQueueItemCopyOrMoveUpload* item, CQuadWord const& sizeWithCRLF_EOLs,
                                    CQuadWord const& numberOfEOLs)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateTextFileSizes()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->SizeWithCRLF_EOLs = sizeWithCRLF_EOLs;
    item->NumberOfEOLs = numberOfEOLs;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateAsciiTransferMode(CFTPQueueItemCopyOrMove* item, BOOL asciiTransferMode)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateAsciiTransferMode()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->AsciiTransferMode = asciiTransferMode;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateIgnoreAsciiTrModeForBinFile(CFTPQueueItemCopyOrMove* item,
                                                  BOOL ignoreAsciiTrModeForBinFile)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateIgnoreAsciiTrModeForBinFile()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->IgnoreAsciiTrModeForBinFile = ignoreAsciiTrModeForBinFile;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateAsciiTransferMode(CFTPQueueItemCopyOrMoveUpload* item, BOOL asciiTransferMode)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateAsciiTransferMode2()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->AsciiTransferMode = asciiTransferMode;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateIgnoreAsciiTrModeForBinFile(CFTPQueueItemCopyOrMoveUpload* item,
                                                  BOOL ignoreAsciiTrModeForBinFile)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateIgnoreAsciiTrModeForBinFile2()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->IgnoreAsciiTrModeForBinFile = ignoreAsciiTrModeForBinFile;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateTgtFileState(CFTPQueueItemCopyOrMoveUpload* item, unsigned tgtFileState)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateTgtFileState2()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->TgtFileState = tgtFileState;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateFileSize(CFTPQueueItemCopyOrMoveUpload* item, CQuadWord const& size,
                               CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateFileSize2()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    oper->SubFromTotalSize(item->Size, TRUE);
    if (item->GetItemState() == sqisDone || item->GetItemState() == sqisSkipped)
        DoneOrSkippedUploadSize -= item->Size;
    else
    {
        if (item->GetItemState() == sqisWaiting || item->GetItemState() == sqisProcessing ||
            item->GetItemState() == sqisDelayed)
        {
            WaitingOrProcessingOrDelayedUploadSize -= item->Size;
        }
    }
    item->Size = size;
    oper->AddToTotalSize(item->Size, TRUE);
    if (item->GetItemState() == sqisDone || item->GetItemState() == sqisSkipped)
        DoneOrSkippedUploadSize += item->Size;
    else
    {
        if (item->GetItemState() == sqisWaiting || item->GetItemState() == sqisProcessing ||
            item->GetItemState() == sqisDelayed)
        {
            WaitingOrProcessingOrDelayedUploadSize += item->Size;
        }
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateAttrErr(CFTPQueueItemChAttrDir* item, BYTE attrErr)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateAttrErr()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->AttrErr = attrErr;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateAttrErr(CFTPQueueItemChAttr* item, BYTE attrErr)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateAttrErr()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->AttrErr = attrErr;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateIsHiddenDir(CFTPQueueItemDelExplore* item, BOOL isHiddenDir)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateIsHiddenDir()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->IsHiddenDir = isHiddenDir;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::UpdateIsHiddenFile(CFTPQueueItemDel* item, BOOL isHiddenFile)
{
    CALL_STACK_MESSAGE1("CFTPQueue::UpdateIsHiddenFile()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    item->IsHiddenFile = isHiddenFile;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

int CFTPQueue::GetSimpleProgress(int* doneOrSkippedCount, int* totalCount, int* unknownSizeCount,
                                 int* waitingCount)
{
    CALL_STACK_MESSAGE1("CFTPQueue::GetSimpleProgress()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    int progress = 0;
    if (Items.Count > 0)
        progress = (1000 * DoneOrSkippedItemsCount) / Items.Count;
    if (doneOrSkippedCount != NULL)
        *doneOrSkippedCount = DoneOrSkippedItemsCount;
    if (totalCount != NULL)
        *totalCount = Items.Count;
    if (unknownSizeCount != NULL)
        *unknownSizeCount = ExploreAndResolveItemsCount;
    if (waitingCount != NULL)
        *waitingCount = WaitingOrProcessingOrDelayedItemsCount;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return progress;
}

void CFTPQueue::GetCopyProgressInfo(CQuadWord* downloaded, int* unknownSizeCount,
                                    CQuadWord* totalWithoutErrors, int* errorsCount,
                                    int* doneOrSkippedCount, int* totalCount,
                                    CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::GetCopyProgressInfo()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    *downloaded = DoneOrSkippedByteSize;
    *unknownSizeCount = CopyUnknownSizeCount;
    CQuadWord size;
    if (oper->GetApproxByteSize(&size, DoneOrSkippedBlockSize))
        *downloaded += size;
    else
        *unknownSizeCount += CopyUnknownSizeCountIfUnknownBlockSize; // block size is unknown, so add those with size in blocks to the unknown ones
    *totalWithoutErrors = WaitingOrProcessingOrDelayedByteSize;
    if (oper->GetApproxByteSize(&size, WaitingOrProcessingOrDelayedBlockSize))
        *totalWithoutErrors += size;
    *totalWithoutErrors += *downloaded;
    *errorsCount = Items.Count - DoneOrSkippedItemsCount - WaitingOrProcessingOrDelayedItemsCount;
    *doneOrSkippedCount = DoneOrSkippedItemsCount;
    *totalCount = Items.Count;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void CFTPQueue::GetCopyUploadProgressInfo(CQuadWord* uploaded, int* unknownSizeCount,
                                          CQuadWord* totalWithoutErrors, int* errorsCount,
                                          int* doneOrSkippedCount, int* totalCount,
                                          CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::GetCopyUploadProgressInfo()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    *uploaded = DoneOrSkippedUploadSize;
    *unknownSizeCount = CopyUnknownSizeCount;
    *totalWithoutErrors = WaitingOrProcessingOrDelayedUploadSize;
    *totalWithoutErrors += *uploaded;
    *errorsCount = Items.Count - DoneOrSkippedItemsCount - WaitingOrProcessingOrDelayedItemsCount;
    *doneOrSkippedCount = DoneOrSkippedItemsCount;
    *totalCount = Items.Count;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

BOOL CFTPQueue::AllItemsDone()
{
    CALL_STACK_MESSAGE1("CFTPQueue::AllItemsDone()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        if (Items[i]->GetItemState() != sqisDone)
            break;
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return i == Items.Count;
}

BOOL CFTPQueue::SearchItemWithNewError(int* itemUID, int* itemIndex)
{
    CALL_STACK_MESSAGE1("CFTPQueue::SearchItemWithNewError()");
    HANDLES(EnterCriticalSection(&QueueCritSect));
    BOOL res = FALSE;
    if (LastFoundErrorOccurenceTime + 1 < LastErrorOccurenceTime + 1) // +1 is here because -1 is used as the initial value
    {                                                                 // it makes sense to search
        int foundUID = -1;
        int foundIndex = -1;
        DWORD foundErrorOccurenceTime = -1;
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CFTPQueueItem* item = Items[i];
            DWORD itemErrorOccurenceTime = -1;
            if (item->IsItemInSimpleErrorState() && item->HasErrorToSolve(NULL, NULL))
                itemErrorOccurenceTime = item->ErrorOccurenceTime;
            if (itemErrorOccurenceTime != -1 &&                                       // the item contains an error
                itemErrorOccurenceTime >= LastFoundErrorOccurenceTime + 1 &&          // it is a "new" error
                (foundUID == -1 || foundErrorOccurenceTime > itemErrorOccurenceTime)) // so far the first found or the "oldest" (we resolve errors in the order they occurred)
            {
                foundErrorOccurenceTime = itemErrorOccurenceTime;
                foundUID = item->UID;
                foundIndex = i;
            }
        }
        if (foundUID == -1)
            LastFoundErrorOccurenceTime = LastErrorOccurenceTime; // not found -> adjust LastFoundErrorOccurenceTime so that only "newer" errors are searched for next time
        else
        {
            *itemUID = foundUID;
            *itemIndex = foundIndex;
            LastFoundErrorOccurenceTime = foundErrorOccurenceTime;
            res = TRUE;
        }
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return res;
}

#ifdef _DEBUG
void CFTPQueue::DebugCheckCounters(CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::DebugCheckCounters()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    int dirItems = 0;
    int exploreAndResolveItemsCount = 0;
    int doneOrSkippedItemsCount = 0;
    int waitingOrProcessingOrDelayedItemsCount = 0;
    CQuadWord doneOrSkippedByteSize(0, 0);
    CQuadWord doneOrSkippedBlockSize(0, 0);
    CQuadWord waitingOrProcessingOrDelayedByteSize(0, 0);
    CQuadWord waitingOrProcessingOrDelayedBlockSize(0, 0);
    CQuadWord doneOrSkippedUploadSize(0, 0);
    CQuadWord waitingOrProcessingOrDelayedUploadSize(0, 0);
    int unknownSizeCount = 0;
    BOOL blockSizeUnknown = !oper->IsBlkSizeKnown();
    int i;
    for (i = 0; i < Items.Count + 1; i++)
    {
        CFTPQueueItem* item = i < Items.Count ? Items[i] : NULL;

        if (item != NULL)
        {
            if (item->IsExploreOrResolveItem())
                exploreAndResolveItemsCount++;

            if (item->Type != fqitCopyFileOrFileLink && item->Type != fqitMoveFileOrFileLink &&
                item->Type != fqitUploadCopyFile && item->Type != fqitUploadMoveFile &&
                item->GetItemState() != sqisDone && item->GetItemState() != sqisSkipped)
            {
                unknownSizeCount++;
            }

            switch (item->GetItemState())
            {
            case sqisDone:
            case sqisSkipped:
            {
                doneOrSkippedItemsCount++;
                if (item->Type == fqitCopyFileOrFileLink || item->Type == fqitMoveFileOrFileLink)
                {
                    CFTPQueueItemCopyOrMove* copyItem = (CFTPQueueItemCopyOrMove*)item;
                    if (copyItem->Size != CQuadWord(-1, -1))
                    {
                        if (copyItem->SizeInBytes)
                            doneOrSkippedByteSize += copyItem->Size;
                        else
                            doneOrSkippedBlockSize += copyItem->Size;
                    }
                }
                else
                {
                    if (item->Type == fqitUploadCopyFile || item->Type == fqitUploadMoveFile)
                        doneOrSkippedUploadSize += ((CFTPQueueItemCopyOrMoveUpload*)item)->Size;
                }
                break;
            }

            case sqisWaiting:
            case sqisProcessing:
            case sqisDelayed:
            {
                waitingOrProcessingOrDelayedItemsCount++;
                if (item->Type == fqitCopyFileOrFileLink || item->Type == fqitMoveFileOrFileLink)
                {
                    CFTPQueueItemCopyOrMove* copyItem = (CFTPQueueItemCopyOrMove*)item;
                    if (copyItem->Size != CQuadWord(-1, -1))
                    {
                        if (copyItem->SizeInBytes)
                            waitingOrProcessingOrDelayedByteSize += copyItem->Size;
                        else
                        {
                            waitingOrProcessingOrDelayedBlockSize += copyItem->Size;
                            if (blockSizeUnknown)
                                unknownSizeCount++;
                        }
                    }
                    else
                        unknownSizeCount++;
                }
                else
                {
                    if (item->Type == fqitUploadCopyFile || item->Type == fqitUploadMoveFile)
                        waitingOrProcessingOrDelayedUploadSize += ((CFTPQueueItemCopyOrMoveUpload*)item)->Size;
                }
                break;
            }

            default:
            {
                if (item->Type == fqitCopyFileOrFileLink || item->Type == fqitMoveFileOrFileLink)
                {
                    CFTPQueueItemCopyOrMove* copyItem = (CFTPQueueItemCopyOrMove*)item;
                    if (copyItem->Size == CQuadWord(-1, -1))
                        unknownSizeCount++;
                    else
                    {
                        if (!copyItem->SizeInBytes && blockSizeUnknown)
                            unknownSizeCount++;
                    }
                }
                break;
            }
            }
        }

        if (item == NULL ||
            item->Type == fqitDeleteDir || item->Type == fqitMoveDeleteDir ||
            item->Type == fqitUploadMoveDeleteDir || item->Type == fqitMoveDeleteDirLink ||
            item->Type == fqitChAttrsDir)
        { // we examine only operations and directory items
            if (item != NULL)
                dirItems++;
            CFTPQueueItemDir* itemDir = item != NULL ? (CFTPQueueItemDir*)item : NULL;
            int parentUID = itemDir != NULL ? itemDir->UID : -1;

            int childItemsNotDone = 0;
            int childItemsSkipped = 0;
            int childItemsFailed = 0;
            int childItemsUINeeded = 0;
            int x;
            for (x = 0; x < Items.Count; x++)
            {
                CFTPQueueItem* actItem = Items[x];
                if (actItem->ParentUID == parentUID)
                {
                    switch (actItem->GetItemState())
                    {
                    case sqisDone:
                        break;
                    case sqisSkipped:
                        childItemsSkipped++;
                        break;
                    case sqisUserInputNeeded:
                        childItemsUINeeded++;
                        break;

                    case sqisFailed:
                    case sqisForcedToFail:
                        childItemsFailed++;
                        break;

                    default:
                        childItemsNotDone++;
                        break;
                    }
                }
            }
            childItemsNotDone += childItemsSkipped + childItemsFailed + childItemsUINeeded;

            if (itemDir != NULL)
            {
                if (itemDir->ChildItemsNotDone != childItemsNotDone ||
                    itemDir->ChildItemsSkipped != childItemsSkipped ||
                    itemDir->ChildItemsFailed != childItemsFailed ||
                    itemDir->ChildItemsUINeeded != childItemsUINeeded)
                {
                    TRACE_E("CFTPQueue::DebugCheckCounters(): problem found in item (UID=" << itemDir->UID << "): " << itemDir->ChildItemsNotDone << " : " << childItemsNotDone << ", " << itemDir->ChildItemsSkipped << " : " << childItemsSkipped << ", " << itemDir->ChildItemsFailed << " : " << childItemsFailed << ", " << itemDir->ChildItemsUINeeded << " : " << childItemsUINeeded);
                }
            }
            else
            {
                int operChildItemsNotDone;
                int operChildItemsSkipped;
                int operChildItemsFailed;
                int operChildItemsUINeeded;
                oper->DebugGetCounters(&operChildItemsNotDone, &operChildItemsSkipped,
                                       &operChildItemsFailed, &operChildItemsUINeeded);
                if (operChildItemsNotDone != childItemsNotDone ||
                    operChildItemsSkipped != childItemsSkipped ||
                    operChildItemsFailed != childItemsFailed ||
                    operChildItemsUINeeded != childItemsUINeeded)
                {
                    TRACE_E("CFTPQueue::DebugCheckCounters(): problem found in operation: " << operChildItemsNotDone << " : " << childItemsNotDone << ", " << operChildItemsSkipped << " : " << childItemsSkipped << ", " << operChildItemsFailed << " : " << childItemsFailed << ", " << operChildItemsUINeeded << " : " << childItemsUINeeded);
                }
            }
        }
    }
    if (exploreAndResolveItemsCount != ExploreAndResolveItemsCount)
        TRACE_E("ExploreAndResolveItemsCount has incorrect value: " << exploreAndResolveItemsCount << " != " << ExploreAndResolveItemsCount);
    if (doneOrSkippedItemsCount != DoneOrSkippedItemsCount)
        TRACE_E("DoneOrSkippedItemsCount has incorrect value: " << doneOrSkippedItemsCount << " != " << DoneOrSkippedItemsCount);
    if (waitingOrProcessingOrDelayedItemsCount != WaitingOrProcessingOrDelayedItemsCount)
        TRACE_E("WaitingOrProcessingOrDelayedItemsCount has incorrect value: " << waitingOrProcessingOrDelayedItemsCount << " != " << WaitingOrProcessingOrDelayedItemsCount);
    if (doneOrSkippedByteSize != DoneOrSkippedByteSize)
        TRACE_E("DoneOrSkippedByteSize has incorrect value: " << doneOrSkippedByteSize.GetDouble() << " != " << DoneOrSkippedByteSize.GetDouble());
    if (doneOrSkippedBlockSize != DoneOrSkippedBlockSize)
        TRACE_E("DoneOrSkippedBlockSize has incorrect value: " << doneOrSkippedBlockSize.GetDouble() << " != " << DoneOrSkippedBlockSize.GetDouble());
    if (waitingOrProcessingOrDelayedByteSize != WaitingOrProcessingOrDelayedByteSize)
        TRACE_E("WaitingOrProcessingOrDelayedByteSize has incorrect value: " << waitingOrProcessingOrDelayedByteSize.GetDouble() << " != " << WaitingOrProcessingOrDelayedByteSize.GetDouble());
    if (waitingOrProcessingOrDelayedBlockSize != WaitingOrProcessingOrDelayedBlockSize)
        TRACE_E("WaitingOrProcessingOrDelayedBlockSize has incorrect value: " << waitingOrProcessingOrDelayedBlockSize.GetDouble() << " != " << WaitingOrProcessingOrDelayedBlockSize.GetDouble());
    if (doneOrSkippedUploadSize != DoneOrSkippedUploadSize)
        TRACE_E("DoneOrSkippedUploadSize has incorrect value: " << doneOrSkippedUploadSize.GetDouble() << " != " << DoneOrSkippedUploadSize.GetDouble());
    if (waitingOrProcessingOrDelayedUploadSize != WaitingOrProcessingOrDelayedUploadSize)
        TRACE_E("WaitingOrProcessingOrDelayedUploadSize has incorrect value: " << waitingOrProcessingOrDelayedUploadSize.GetDouble() << " != " << WaitingOrProcessingOrDelayedUploadSize.GetDouble());
    if (unknownSizeCount != CopyUnknownSizeCount + (blockSizeUnknown ? CopyUnknownSizeCountIfUnknownBlockSize : 0))
    {
        TRACE_E("CopyUnknownSizeCount* has incorrect value: " << unknownSizeCount << " != " << CopyUnknownSizeCount + (blockSizeUnknown ? CopyUnknownSizeCountIfUnknownBlockSize : 0) << " (" << CopyUnknownSizeCount << ", " << CopyUnknownSizeCountIfUnknownBlockSize << ")");
    }

    //  TRACE_I("CFTPQueue::DebugCheckCounters(): items=" << Items.Count << ", dir-items=" << dirItems);
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}
#endif

CFTPQueueItem*
CFTPQueue::GetNextWaitingItem(CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::GetNextWaitingItem()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    BOOL getOnlyExploreAndResolveItems = GetOnlyExploreAndResolveItems;
    CFTPQueueItem* ret = NULL;
    while (1)
    {
        int i;
        for (i = FirstWaitingItemIndex; i < Items.Count; i++)
        {
            CFTPQueueItem* item = Items[i];
            if (item->GetItemState() == sqisWaiting &&
                (!getOnlyExploreAndResolveItems || item->IsExploreOrResolveItem())) // item found
            {
                FirstWaitingItemIndex = i + 1;
                item->ChangeStateAndCounters(sqisProcessing, oper, this); // item state change
                ret = item;
                break;
            }
        }
        if (ret != NULL || !getOnlyExploreAndResolveItems)
            break;
        else
        {
            // there are no more "explore" or "resolve" items, so in the second round we take all items
            getOnlyExploreAndResolveItems = GetOnlyExploreAndResolveItems = FALSE;
            FirstWaitingItemIndex = 0;
        }
    }
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return ret;
}

void CFTPQueue::ReturnToWaitingItems(CFTPQueueItem* item, CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CFTPQueue::ReturnToWaitingItems()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    if (item != NULL)
    {
        CFTPQueueItemState newState = sqisWaiting;
        if (item->Type == fqitDeleteDir || item->Type == fqitMoveDeleteDir ||
            item->Type == fqitUploadMoveDeleteDir || item->Type == fqitMoveDeleteDirLink ||
            item->Type == fqitChAttrsDir)
        {
            newState = ((CFTPQueueItemDir*)item)->GetStateFromCounters();
        }
        item->ChangeStateAndCounters(newState, oper, this); // item state change
        if (newState == sqisWaiting)
        {
            BOOL exploreOrResolve = item->IsExploreOrResolveItem();
            if (exploreOrResolve || !GetOnlyExploreAndResolveItems)
            {
                FirstWaitingItemIndex = 0; // we will not determine the index of the returned item (computationally as difficult as finding the first "waiting" item - which is done only once, unlike calling this function)
                if (exploreOrResolve)
                    GetOnlyExploreAndResolveItems = TRUE;
            }
        }
    }
    else
        TRACE_E("Invalid call to CFTPQueue::ReturnToWaitingItems(): 'item' may not be NULL!");
    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

//
// ****************************************************************************
// CFTPOperation
//

CFTPOperation::CFTPOperation()
{
    HANDLES(InitializeCriticalSection(&OperCritSect));
    UID = -1;
    HANDLES(EnterCriticalSection(&NextOrdinalNumberCS));
    OrdinalNumber = NextOrdinalNumber++;
    HANDLES(LeaveCriticalSection(&NextOrdinalNumberCS));
    Queue = NULL;
    OperationDlg = NULL;
    OperationDlgThread = NULL;

    ProxyServer = NULL;
    ProxyScriptText = NULL;
    ProxyScriptStartExecPoint = NULL;
    ConnectToHost = NULL;
    ConnectToPort = -1;
    HostIP = INADDR_NONE;
    Host = NULL;
    Port = -1;
    User = NULL;
    Password = NULL;
    Account = NULL;
    RetryLoginWithoutAsking = FALSE;
    InitFTPCommands = NULL;
    UsePassiveMode = FALSE;
    SizeCmdIsSupported = TRUE; // we will try it on the server at least once
    ListCommand = NULL;
    ServerIP = INADDR_NONE;
    ServerSystem = NULL;
    ServerFirstReply = NULL;
    UseListingsCache = FALSE;
    ListingServerType = NULL;

    ReportChangeInWorkerID = -2;
    ReportProgressChange = FALSE;
    ReportChangeInItemUID = -3;
    ReportChangeInItemUID2 = -1; // just for form's sake (pointless)
    OperStateChangedPosted = FALSE;
    LastReportedOperState = opstNone;

    Type = fotNone;
    OperationSubject = NULL;

    ChildItemsNotDone = 0;
    ChildItemsSkipped = 0;
    ChildItemsFailed = 0;
    ChildItemsUINeeded = 0;

    SourcePath = NULL;
    SrcPathSeparator = '/';
    SrcPathCanChange = FALSE;
    SrcPathCanChangeInclSubdirs = FALSE;

    LastErrorOccurenceTime = -1;

    AttrAnd = -1;
    AttrOr = 0;

    TargetPath = NULL;
    TgtPathSeparator = '\\';
    TgtPathCanChange = FALSE;
    TgtPathCanChangeInclSubdirs = FALSE;
    ASCIIFileMasks = NULL;

    TotalSizeInBytes.SetUI64(0);
    TotalSizeInBlocks.SetUI64(0);

    BlkSizeTotalInBytes.SetUI64(0);
    BlkSizeTotalInBlocks.SetUI64(0);
    BlkSizeActualValue = -1;

    ResumeIsNotSupported = FALSE;
    DataConWasOpenedForAppendCmd = FALSE;

    AutodetectTrMode = 0;
    UseAsciiTransferMode = 0;
    CannotCreateFile = 0;
    CannotCreateDir = 0;
    FileAlreadyExists = 0;
    DirAlreadyExists = 0;
    RetryOnCreatedFile = 0;
    RetryOnResumedFile = 0;
    AsciiTrModeButBinFile = 0;
    UploadCannotCreateFile = 0;
    UploadCannotCreateDir = 0;
    UploadFileAlreadyExists = 0;
    UploadDirAlreadyExists = 0;
    UploadRetryOnCreatedFile = 0;
    UploadRetryOnResumedFile = 0;
    UploadAsciiTrModeButBinFile = 0;
    ConfirmDelOnNonEmptyDir = 0;
    ConfirmDelOnHiddenFile = 0;
    ConfirmDelOnHiddenDir = 0;
    ChAttrOfFiles = 0;
    ChAttrOfDirs = 0;
    UnknownAttrs = 0;
    EncryptDataConnection = EncryptControlConnection = FALSE;
    pCertificate = NULL;
    CompressData = 0;

    GlobalTransferSpeedMeter.JustConnected();   // measure the global transfer speed from the start of the operation
    GlobalLastActivityTime.Set(GetTickCount()); // the first activity is the start of the operation

    OperationEnd = OperationStart = GetTickCount();
    if (OperationEnd == -1)
        OperationEnd++;
}

CFTPOperation::~CFTPOperation()
{
    if (OperationDlg != NULL)
        TRACE_E("Unexpected situation in CFTPOperation::~CFTPOperation(): operation is destructed, but its dialog still exists!");
    if (Queue != NULL)
        delete Queue;
    if (OperationSubject != NULL)
        SalamanderGeneral->Free(OperationSubject);
    if (ProxyServer != NULL)
        delete ProxyServer;
    if (ConnectToHost != NULL)
        SalamanderGeneral->Free(ConnectToHost);
    if (Host != NULL)
        SalamanderGeneral->Free(Host);
    if (User != NULL)
        SalamanderGeneral->Free(User);
    if (Password != NULL)
        SalamanderGeneral->Free(Password);
    if (Account != NULL)
        SalamanderGeneral->Free(Account);
    if (InitFTPCommands != NULL)
        SalamanderGeneral->Free(InitFTPCommands);
    if (ListCommand != NULL)
        SalamanderGeneral->Free(ListCommand);
    if (ServerSystem != NULL)
        SalamanderGeneral->Free(ServerSystem);
    if (ServerFirstReply != NULL)
        SalamanderGeneral->Free(ServerFirstReply);
    if (ListingServerType != NULL)
        SalamanderGeneral->Free(ListingServerType);
    if (SourcePath != NULL)
        SalamanderGeneral->Free(SourcePath);
    if (TargetPath != NULL)
        SalamanderGeneral->Free(TargetPath);
    if (ASCIIFileMasks != NULL)
        SalamanderGeneral->FreeSalamanderMaskGroup(ASCIIFileMasks);
    if (pCertificate != NULL)
        pCertificate->Release();

    HANDLES(DeleteCriticalSection(&OperCritSect));
}

int CFTPOperation::GetUID()
{
    CALL_STACK_MESSAGE1("CFTPOperation::GetUID()");

    HANDLES(EnterCriticalSection(&OperCritSect));
    int uid = UID;
    HANDLES(LeaveCriticalSection(&OperCritSect));
    return uid;
}

void CFTPOperation::SetUID(int uid)
{
    CALL_STACK_MESSAGE2("CFTPOperation::SetUID(%d)", uid);

    HANDLES(EnterCriticalSection(&OperCritSect));
    UID = uid;
    HANDLES(LeaveCriticalSection(&OperCritSect));
}
