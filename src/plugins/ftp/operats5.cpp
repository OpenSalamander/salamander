// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CFTPWorkersList
//

CFTPWorkersList::CFTPWorkersList() : Workers(5, 10)
{
    HANDLES(InitializeCriticalSection(&WorkersListCritSect));
    NextWorkerID = 1;
    LastFoundErrorOccurenceTime = -1;
}

CFTPWorkersList::~CFTPWorkersList()
{
    if (Workers.Count > 0)
        TRACE_E("Unexpected situation in CFTPWorkersList::~CFTPWorkersList(): operation is destructed, but its workers still exist! count=" << Workers.Count);
    HANDLES(DeleteCriticalSection(&WorkersListCritSect));
}

BOOL CFTPWorkersList::AddWorker(CFTPWorker* newWorker)
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::AddWorker()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = TRUE;
    newWorker->SetID(NextWorkerID++);
    Workers.Add(newWorker);
    if (!Workers.IsGood())
    {
        Workers.ResetState();
        ret = FALSE;
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

BOOL CFTPWorkersList::InformWorkersAboutStop(int workerInd, CFTPWorker** victims,
                                             int maxVictims, int* foundVictims)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::InformWorkersAboutStop(%d, , ,)", workerInd);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = FALSE;
    if (workerInd != -1)
    {
        if (workerInd >= 0 && workerInd < Workers.Count)
        {
            if (*foundVictims < maxVictims)
            {
                CFTPWorker* worker = Workers[workerInd];
                if (worker->InformAboutStop()) // pridame ho mezi obeti (pozdeji se na ne zavola CloseDataConnectionOrPostShouldStop())
                    *(victims + (*foundVictims)++) = worker;
            }
            else
                ret = TRUE;
        }
    }
    else
    {
        int i = 0;
        while (*foundVictims < maxVictims && i < Workers.Count)
        {
            CFTPWorker* worker = Workers[i++];
            if (worker->InformAboutStop()) // pridame ho mezi obeti (pozdeji se na ne zavola CloseDataConnectionOrPostShouldStop())
                *(victims + (*foundVictims)++) = worker;
        }
        ret = i < Workers.Count;
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

BOOL CFTPWorkersList::InformWorkersAboutPause(int workerInd, CFTPWorker** victims,
                                              int maxVictims, int* foundVictims, BOOL pause)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::InformWorkersAboutPause(%d, , ,)", workerInd);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = FALSE;
    if (workerInd != -1)
    {
        if (workerInd >= 0 && workerInd < Workers.Count)
        {
            if (*foundVictims < maxVictims)
            {
                CFTPWorker* worker = Workers[workerInd];
                if (worker->InformAboutPause(pause)) // pridame ho mezi obeti (pozdeji se na ne zavola PostShouldPauseOrResume())
                    *(victims + (*foundVictims)++) = worker;
            }
            else
                ret = TRUE;
        }
    }
    else
    {
        int i = 0;
        while (*foundVictims < maxVictims && i < Workers.Count)
        {
            CFTPWorker* worker = Workers[i++];
            if (worker->InformAboutPause(pause)) // pridame ho mezi obeti (pozdeji se na ne zavola PostShouldPauseOrResume())
                *(victims + (*foundVictims)++) = worker;
        }
        ret = i < Workers.Count;
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

BOOL CFTPWorkersList::CanCloseWorkers(int workerInd)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::CanCloseWorkers(%d)", workerInd);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = TRUE;
    if (workerInd != -1)
    {
        if (workerInd >= 0 && workerInd < Workers.Count)
        {
            CFTPWorker* worker = Workers[workerInd];
            if (!worker->SocketClosedAndDataConDoesntExist() || worker->HaveWorkInDiskThread())
            {
                ret = FALSE;
            }
        }
    }
    else
    {
        int i;
        for (i = 0; i < Workers.Count; i++)
        {
            CFTPWorker* worker = Workers[i];
            if (!worker->SocketClosedAndDataConDoesntExist() || worker->HaveWorkInDiskThread())
            {
                ret = FALSE;
                break;
            }
        }
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

BOOL CFTPWorkersList::ForceCloseWorkers(int workerInd, CFTPWorker** victims,
                                        int maxVictims, int* foundVictims)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::ForceCloseWorkers(%d, , ,)", workerInd);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = FALSE;
    if (workerInd != -1)
    {
        if (workerInd >= 0 && workerInd < Workers.Count)
        {
            CFTPWorker* worker = Workers[workerInd];
            worker->ForceCloseDiskWork();
            if (*foundVictims < maxVictims)
            {
                if (!worker->SocketClosedAndDataConDoesntExist()) // pridame ho mezi obeti (pozdeji se na ne zavola ForceClose())
                    *(victims + (*foundVictims)++) = worker;
            }
            else
                ret = TRUE;
        }
    }
    else
    {
        int i = 0;
        while (*foundVictims < maxVictims && i < Workers.Count)
        {
            CFTPWorker* worker = Workers[i++];
            worker->ForceCloseDiskWork();
            if (!worker->SocketClosedAndDataConDoesntExist()) // pridame ho mezi obeti (pozdeji se na ne zavola ForceClose())
                *(victims + (*foundVictims)++) = worker;
        }
        ret = i < Workers.Count;
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

BOOL CFTPWorkersList::DeleteWorkers(int workerInd, CFTPWorker** victims,
                                    int maxVictims, int* foundVictims,
                                    CUploadWaitingWorker** uploadFirstWaitingWorker)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::DeleteWorkers(%d, , , ,)", workerInd);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = FALSE;
    if (workerInd != -1)
    {
        if (workerInd >= 0 && workerInd < Workers.Count)
        {
            if (*foundVictims < maxVictims)
            {
                CFTPWorker* worker = Workers[workerInd];
                worker->ReleaseData(uploadFirstWaitingWorker);
                Workers.Detach(workerInd);
                if (!Workers.IsGood())
                    Workers.ResetState(); // odpojeni musi vzdy vyjit (chyba = neni mozne zmensit pole)
                if (worker->CanDeleteFromDelWorkers())
                    *(victims + (*foundVictims)++) = worker; // pridame ho mezi obeti (pozdeji se na ne zavola DeleteSocket())
            }
            else
                ret = TRUE;
        }
    }
    else
    {
        while (*foundVictims < maxVictims && Workers.Count > 0)
        {
            CFTPWorker* worker = Workers[Workers.Count - 1];
            worker->ReleaseData(uploadFirstWaitingWorker);
            Workers.Detach(Workers.Count - 1);
            if (!Workers.IsGood())
                Workers.ResetState(); // odpojeni musi vzdy vyjit (chyba = neni mozne zmensit pole)
            if (worker->CanDeleteFromDelWorkers())
                *(victims + (*foundVictims)++) = worker; // pridame ho mezi obeti (pozdeji se na ne zavola DeleteSocket())
        }
        ret = Workers.Count > 0;
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

int CFTPWorkersList::GetCount()
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::GetCount()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    int ret = Workers.Count;
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

int CFTPWorkersList::GetFirstErrorIndex()
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::GetFirstErrorIndex()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    int ret = -1;
    int i;
    for (i = 0; i < Workers.Count; i++)
    {
        if (Workers[i]->HaveError())
        { // nalezeno
            ret = i;
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

int CFTPWorkersList::GetWorkerIndex(int workerID)
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::GetWorkerIndex()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    int ret = -1;
    int i;
    for (i = 0; i < Workers.Count; i++)
    {
        if (Workers[i]->GetID() == workerID)
        { // nalezeno
            ret = i;
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

void CFTPWorkersList::GetListViewDataFor(int index, NMLVDISPINFO* lvdi, char* buf, int bufSize)
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::GetListViewDataFor()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    LVITEM* itemData = &(lvdi->item);
    if (index >= 0 && index < Workers.Count) // index je platny
    {
        Workers[index]->GetListViewData(itemData, buf, bufSize);
    }
    else // pri neplatnem indexu (listview jeste nestihlo refresh) musime vratit aspon prazdnou polozku
    {
        if (itemData->mask & LVIF_IMAGE)
            itemData->iImage = 0; // zatim mame jedinou ikonu
        if (itemData->mask & LVIF_TEXT)
        {
            if (bufSize > 0)
                buf[0] = 0;
            itemData->pszText = buf;
        }
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
}

int CFTPWorkersList::GetWorkerID(int index)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::GetWorkerID(%d)", index);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    int id = -1;
    if (index >= 0 && index < Workers.Count) // index je platny
    {
        id = Workers[index]->GetID();
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return id;
}

int CFTPWorkersList::GetLogUID(int index)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::GetLogUID(%d)", index);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    int uid = -1;
    if (index >= 0 && index < Workers.Count) // index je platny
    {
        uid = Workers[index]->GetLogUID();
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return uid;
}

BOOL CFTPWorkersList::HaveError(int index)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::HaveError(%d)", index);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = FALSE;
    if (index >= 0 && index < Workers.Count) // index je platny
    {
        ret = Workers[index]->HaveError();
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

BOOL CFTPWorkersList::IsPaused(int index, BOOL* isWorking)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::IsPaused(%d,)", index);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = FALSE;
    if (index >= 0 && index < Workers.Count) // index je platny
    {
        ret = Workers[index]->IsPaused(isWorking);
    }
    else
        *isWorking = FALSE;
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

BOOL CFTPWorkersList::SomeWorkerIsWorking(BOOL* someIsWorkingAndNotPaused)
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::SomeWorkerIsWorking()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = FALSE;
    *someIsWorkingAndNotPaused = FALSE;
    BOOL isPaused, isWorking;
    int i;
    for (i = 0; i < Workers.Count; i++)
    {
        isPaused = Workers[i]->IsPaused(&isWorking);
        if (isWorking)
        {
            ret = TRUE;
            if (!isPaused)
            {
                *someIsWorkingAndNotPaused = TRUE;
                break; // dale uz neni co zjistovat
            }
        }
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

BOOL CFTPWorkersList::GetErrorDescr(int index, char* buf, int bufSize, CCertificate** unverifiedCertificate)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::GetErrorDescr(%d, ,)", index);

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    if (unverifiedCertificate != NULL)
        *unverifiedCertificate = NULL;
    BOOL ret = FALSE;
    CFTPWorker* worker = NULL;
    BOOL postActivate = FALSE;
    int msg = -1;
    int uid = -1;
    if (index >= 0 && index < Workers.Count) // index je platny
    {
        worker = Workers[index];
        ret = worker->GetErrorDescr(buf, bufSize, &postActivate, unverifiedCertificate);
        uid = worker->GetCopyOfUID();
        msg = worker->GetCopyOfMsg();
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    if (postActivate)
        SocketsThread->PostSocketMessage(msg, uid, WORKER_ACTIVATE, NULL);
    return ret;
}

void CFTPWorkersList::ActivateWorkers()
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::ActivateWorkers()");

    int i = 0;
    while (1)
    {
        HANDLES(EnterCriticalSection(&WorkersListCritSect));
        CFTPWorker* worker = (i < Workers.Count) ? Workers[i] : NULL;
        int msg = -1;
        int uid = -1;
        if (worker != NULL)
        {
            uid = worker->GetCopyOfUID();
            msg = worker->GetCopyOfMsg();
        }
        HANDLES(LeaveCriticalSection(&WorkersListCritSect));
        if (worker != NULL)
            SocketsThread->PostSocketMessage(msg, uid, WORKER_ACTIVATE, NULL);
        else
            break; // konec smycky
        i++;
    }
}

void CFTPWorkersList::PostLoginChanged(int workerID)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::PostLoginChanged(%d)", workerID);

    int i = 0;
    while (1)
    {
        HANDLES(EnterCriticalSection(&WorkersListCritSect));
        CFTPWorker* worker = (i < Workers.Count) ? Workers[i] : NULL;
        int uid = -1;
        int msg = -1;
        BOOL send = FALSE;
        if (worker != NULL)
        {
            if ((workerID == -1 || worker->GetID() == workerID) && // vsechny nebo s ID 'workerID'
                worker->GetState() == fwsConnectionError)          // jen pokud je ve stavu fwsConnectionError
            {
                uid = worker->GetCopyOfUID();
                msg = worker->GetCopyOfMsg();
                send = TRUE;
            }
        }
        HANDLES(LeaveCriticalSection(&WorkersListCritSect));
        if (send)
            SocketsThread->PostSocketMessage(msg, uid, WORKER_NEWLOGINPARAMS, NULL);
        if (worker == NULL)
            break; // konec smycky
        i++;
    }
}

BOOL CFTPWorkersList::GiveWorkToSleepingConWorker(CFTPWorker* sourceWorker)
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::GiveWorkToSleepingConWorker()");

    // POZOR: mozna uz jsme v sekci CSocketsThread::CritSect (a CSocket::SocketCritSect) !!!

    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL postActivate = FALSE;
    CFTPWorker* worker;
    int sourceWorkerUID = -1;
    int sourceWorkerMsg = -1;
    int workerUID = -1;
    int workerMsg = -1;
    int i;
    for (i = 0; i < Workers.Count; i++)
    {
        worker = Workers[i];
        if (worker != sourceWorker)
        {
            BOOL openCon, receivingWakeup;
            if (worker->IsSleeping(&openCon, &receivingWakeup) && openCon && !receivingWakeup) // "sleeping" worker s otevrenou connectionou
            {
                worker->GiveWorkToSleepingConWorker(sourceWorker);
                postActivate = TRUE;
                sourceWorkerUID = sourceWorker->GetCopyOfUID();
                sourceWorkerMsg = sourceWorker->GetCopyOfMsg();
                workerUID = worker->GetCopyOfUID();
                workerMsg = worker->GetCopyOfMsg();
                ret = TRUE;
                break;
            }
        }
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    if (postActivate)
    {
        SocketsThread->PostSocketMessage(sourceWorkerMsg, sourceWorkerUID, WORKER_ACTIVATE, NULL);
        SocketsThread->PostSocketMessage(workerMsg, workerUID, WORKER_ACTIVATE, NULL);
    }
    return ret;
}

void CFTPWorkersList::AddCurrentDownloadSize(CQuadWord* downloaded)
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::AddCurrentDownloadSize()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    int i;
    for (i = 0; i < Workers.Count; i++)
        Workers[i]->AddCurrentDownloadSize(downloaded);
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
}

void CFTPWorkersList::AddCurrentUploadSize(CQuadWord* uploaded)
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::AddCurrentUploadSize()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    int i;
    for (i = 0; i < Workers.Count; i++)
        Workers[i]->AddCurrentUploadSize(uploaded);
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
}

BOOL CFTPWorkersList::SearchWorkerWithNewError(int* index, DWORD lastErrorOccurenceTime)
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::SearchWorkerWithNewError()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL res = FALSE;
    if (LastFoundErrorOccurenceTime + 1 < lastErrorOccurenceTime + 1) // +1 je zde kvuli pouziti -1 jako inicializacnich hodnot
    {                                                                 // ma smysl hledat
        int foundIndex = -1;
        DWORD foundErrorOccurenceTime = -1;
        int i;
        for (i = 0; i < Workers.Count; i++)
        {
            CFTPWorker* worker = Workers[i];
            DWORD workerErrorOccurenceTime = worker->GetErrorOccurenceTime();
            if (workerErrorOccurenceTime != -1 &&                                         // worker obsahuje chybu (krome chyby vnucene userem pri reseni login/password behem cekani na reconnect)
                workerErrorOccurenceTime >= LastFoundErrorOccurenceTime + 1 &&            // je to "nova" chyba
                (foundIndex == -1 || foundErrorOccurenceTime > workerErrorOccurenceTime)) // zatim prvni nalezena nebo "nejstarsi" (chyby resime poporade jak nastaly)
            {
                foundErrorOccurenceTime = workerErrorOccurenceTime;
                foundIndex = i;
            }
        }
        if (foundIndex == -1)
            LastFoundErrorOccurenceTime = lastErrorOccurenceTime; // nenalezeno -> upravime LastFoundErrorOccurenceTime tak, aby se priste hledaly jen "novejsi" chyby
        else
        {
            *index = foundIndex;
            LastFoundErrorOccurenceTime = foundErrorOccurenceTime;
            res = TRUE;
        }
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return res;
}

void CFTPWorkersList::PostNewWorkAvailable(BOOL onlyOneItem)
{
    CALL_STACK_MESSAGE2("CFTPWorkersList::PostNewWorkAvailable(%d)", onlyOneItem);

    // POZOR: mozna uz jsme v sekci CSocketsThread::CritSect (a CSocket::SocketCritSect) !!!

    if (onlyOneItem)
    {
        // nejdrive zkusime najit "sleeping" workera, prioritneji s otevrenou connectionou
        HANDLES(EnterCriticalSection(&WorkersListCritSect));
        CFTPWorker* worker = NULL;
        int i, found = -1;
        for (i = 0; i < Workers.Count; i++)
        {
            worker = Workers[i];
            BOOL openCon, receivingWakeup;
            if (worker->IsSleeping(&openCon, &receivingWakeup) && !receivingWakeup)
            {
                if (openCon)
                    break; // nalezen a s connectionou, dal nema smysl hledat
                else
                {
                    if (found == -1)
                        found = i; // schovame si prvniho "sleeping" workera bez connectiony a hledame dal
                }
            }
        }
        if (i == Workers.Count)
        {
            if (found != -1)
                worker = Workers[found];
            else
                worker = NULL;
        }
        int msg = -1;
        int uid = -1;
        if (worker != NULL)
        {
            msg = worker->GetCopyOfMsg();
            uid = worker->GetCopyOfUID();
            worker->SetReceivingWakeup(TRUE);
        }
        HANDLES(LeaveCriticalSection(&WorkersListCritSect));

        if (worker != NULL) // pokud existuje aspon nejaky "sleeping" worker, postneme mu "wake-up"
            SocketsThread->PostSocketMessage(msg, uid, WORKER_WAKEUP, NULL);
    }
    else
    {
        BOOL doSecRound = FALSE;
        int r;
        for (r = 0; r < 2; r++)
        {
            int i = 0;
            while (1)
            {
                BOOL openCon, receivingWakeup;
                CFTPWorker* worker = NULL;
                HANDLES(EnterCriticalSection(&WorkersListCritSect));
                while (i < Workers.Count)
                {
                    worker = Workers[i];
                    if (worker->IsSleeping(&openCon, &receivingWakeup) && !receivingWakeup)
                    {
                        if (r == 1 || openCon)
                            break; // nejprve hledame ty s otevrenou connectionou, pak vsechny
                        else
                            doSecRound = TRUE;
                    }
                    i++;
                }
                if (i == Workers.Count)
                    worker = NULL;
                int msg = -1;
                int uid = -1;
                if (worker != NULL)
                {
                    msg = worker->GetCopyOfMsg();
                    uid = worker->GetCopyOfUID();
                    worker->SetReceivingWakeup(TRUE);
                }
                HANDLES(LeaveCriticalSection(&WorkersListCritSect));

                if (worker == NULL)
                    break; // v poli uz neni zadny worker
                SocketsThread->PostSocketMessage(msg, uid, WORKER_WAKEUP, NULL);
                i++;
            }
            if (!doSecRound)
                break;
        }
    }
}

BOOL CFTPWorkersList::EmptyOrAllShouldStop()
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::EmptyOrAllShouldStop()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = TRUE;
    int i;
    for (i = 0; i < Workers.Count; i++)
    {
        if (!Workers[i]->GetShouldStop())
        {
            ret = FALSE;
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

BOOL CFTPWorkersList::AtLeastOneWorkerIsWaitingForUser()
{
    CALL_STACK_MESSAGE1("CFTPWorkersList::AtLeastOneWorkerIsWaitingForUser()");

    HANDLES(EnterCriticalSection(&WorkersListCritSect));
    BOOL ret = FALSE;
    int i;
    for (i = 0; i < Workers.Count; i++)
    {
        if (Workers[i]->GetState() == fwsConnectionError)
        {
            ret = TRUE;
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&WorkersListCritSect));
    return ret;
}

//
// ****************************************************************************
// CReturningConnections
//

CReturningConnections::CReturningConnections(int base, int delta) : Data(base, delta)
{
    HANDLES(InitializeCriticalSection(&RetConsCritSect));
}

CReturningConnections::~CReturningConnections()
{
    if (Data.Count > 0)
        TRACE_E("Unexpected situation in CReturningConnections::~CReturningConnections(): array is not empty!");
    HANDLES(DeleteCriticalSection(&RetConsCritSect));
}

BOOL CReturningConnections::Add(int controlConUID, CFTPWorker* workerWithCon)
{
    HANDLES(EnterCriticalSection(&RetConsCritSect));
    BOOL ok = TRUE;
    CReturningConnectionData* newItem = new CReturningConnectionData(controlConUID, workerWithCon);
    if (newItem != NULL)
    {
        Data.Add(newItem);
        if (!Data.IsGood())
        {
            delete newItem;
            Data.ResetState();
            ok = FALSE;
        }
    }
    else
    {
        TRACE_E(LOW_MEMORY);
        ok = FALSE;
    }
    HANDLES(LeaveCriticalSection(&RetConsCritSect));
    return ok;
}

BOOL CReturningConnections::GetFirstCon(int* controlConUID, CFTPWorker** workerWithCon)
{
    HANDLES(EnterCriticalSection(&RetConsCritSect));
    BOOL ok = TRUE;
    if (Data.Count > 0)
    {
        *controlConUID = Data[0]->ControlConUID;
        *workerWithCon = Data[0]->WorkerWithCon;
        Data.Delete(0);
        if (!Data.IsGood())
            Data.ResetState();
    }
    else
    {
        *controlConUID = -1;
        *workerWithCon = NULL;
        ok = FALSE;
    }
    HANDLES(LeaveCriticalSection(&RetConsCritSect));
    return ok;
}

void CReturningConnections::CloseData()
{
    HANDLES(EnterCriticalSection(&RetConsCritSect));
    while (Data.Count > 0)
    {
        CFTPWorker* worker = Data[Data.Count - 1]->WorkerWithCon;
        Data.Delete(Data.Count - 1);
        if (!Data.IsGood())
            Data.ResetState();
        HANDLES(LeaveCriticalSection(&RetConsCritSect));

        worker->ForceClose(); // tvrde zavreme socket (sice by na close socketu nic nemelo cekat (stacilo by volat CloseSocket()), ale sychrujeme se - SocketClosed se hned po pridani do ReturningConnections dava na TRUE)
        if (worker->CanDeleteFromRetCons())
            DeleteSocket(worker);

        HANDLES(EnterCriticalSection(&RetConsCritSect));
    }
    HANDLES(LeaveCriticalSection(&RetConsCritSect));
}

//
// ****************************************************************************
// CFTPFileToClose
//

CFTPFileToClose::CFTPFileToClose(const char* path, const char* name, HANDLE file, BOOL deleteIfEmpty,
                                 BOOL setDateAndTime, const CFTPDate* date, const CFTPTime* time,
                                 BOOL deleteFile, CQuadWord* setEndOfFile)
{
    File = file;
    DeleteIfEmpty = deleteIfEmpty;
    lstrcpyn(FileName, path, MAX_PATH);
    if (!SalamanderGeneral->SalPathAppend(FileName, name, MAX_PATH))
        TRACE_E("Unexpected situation in CFTPFileToClose::CFTPFileToClose(): too long file name!");
    SetDateAndTime = setDateAndTime;
    if (SetDateAndTime && date != NULL)
        Date = *date;
    else
        memset(&Date, 0, sizeof(Date));
    if (SetDateAndTime && time != NULL)
        Time = *time;
    else
    {
        memset(&Time, 0, sizeof(Time));
        Time.Hour = 24;
    }
    AlwaysDeleteFile = deleteFile;
    if (setEndOfFile != NULL)
        EndOfFile = *setEndOfFile;
    else
        EndOfFile.Set(-1, -1);
}

//
// ****************************************************************************
// CDiskListingItem
//

CDiskListingItem::CDiskListingItem(const char* name, BOOL isDir, const CQuadWord& size)
{
    Name = SalamanderGeneral->DupStr(name);
    IsDir = isDir;
    Size = size;
}

//
// ****************************************************************************
// CFTPDiskThread
//

void CFTPDiskWork::CopyFrom(CFTPDiskWork* work)
{
    SocketMsg = work->SocketMsg;
    SocketUID = work->SocketUID;
    MsgID = work->MsgID;

    Type = work->Type;

    lstrcpyn(Path, work->Path, MAX_PATH);
    lstrcpyn(Name, work->Name, MAX_PATH);

    ForceAction = work->ForceAction;
    AlreadyRenamedName = work->AlreadyRenamedName;

    CannotCreateDir = work->CannotCreateDir;
    DirAlreadyExists = work->DirAlreadyExists;
    CannotCreateFile = work->CannotCreateFile;
    FileAlreadyExists = work->FileAlreadyExists;
    RetryOnCreatedFile = work->RetryOnCreatedFile;
    RetryOnResumedFile = work->RetryOnResumedFile;

    CheckFromOffset = work->CheckFromOffset;
    WriteOrReadFromOffset = work->WriteOrReadFromOffset;
    FlushDataBuffer = work->FlushDataBuffer;
    ValidBytesInFlushDataBuffer = work->ValidBytesInFlushDataBuffer;
    EOLsInFlushDataBuffer = work->EOLsInFlushDataBuffer;
    WorkFile = work->WorkFile;

    ProblemID = work->ProblemID;
    WinError = work->WinError;
    State = work->State;
    NewTgtName = work->NewTgtName;
    OpenedFile = work->OpenedFile;
    FileSize = work->FileSize;
    CanOverwrite = work->CanOverwrite;
    CanDeleteEmptyFile = work->CanDeleteEmptyFile;
    DiskListing = work->DiskListing;
}

CFTPDiskThread::CFTPDiskThread() : CThread("FTP Disk Thread"), Work(20, 50, dtNoDelete), FilesToClose(20, 50)
{
    HANDLES(InitializeCriticalSection(&DiskCritSect));
    ContEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL)); // manual, nonsignaled
    if (ContEvent == NULL)
        TRACE_E("CFTPDiskThread::CFTPDiskThread(): Unable to create synchronization event object.");
    ShouldTerminate = FALSE;
    WorkIsInProgress = FALSE;
    NextFileCloseIndex = 0;
    DoneFileCloseIndex = -1;
    FileClosedEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL)); // manual, nonsignaled
    if (FileClosedEvent == NULL)
        TRACE_E("CFTPDiskThread::CFTPDiskThread(): Unable to create FileClosedEvent object.");
}

CFTPDiskThread::~CFTPDiskThread()
{
    while (Work.Count > 0 && Work[0] == NULL)
    {
        Work.Detach(0);
        if (!Work.IsGood())
            Work.ResetState();
    }
    if (Work.Count > 0)
        TRACE_E("Unexpected situation in CFTPDiskThread::~CFTPDiskThread(): array with work is not empty!");
    if (FilesToClose.Count > 0)
        TRACE_I("CFTPDiskThread::~CFTPDiskThread(): array with files to close is not empty!");
    if (ContEvent != NULL)
        HANDLES(CloseHandle(ContEvent));
    if (FileClosedEvent != NULL)
        HANDLES(CloseHandle(FileClosedEvent));
    FileClosedEvent = NULL;
    HANDLES(DeleteCriticalSection(&DiskCritSect));
}

void CFTPDiskThread::Terminate()
{
    HANDLES(EnterCriticalSection(&DiskCritSect));
    ShouldTerminate = TRUE;
    SetEvent(ContEvent);
    HANDLES(LeaveCriticalSection(&DiskCritSect));
}

BOOL CFTPDiskThread::AddWork(CFTPDiskWork* work)
{
    CALL_STACK_MESSAGE1("CFTPDiskThread::AddWork()");
    HANDLES(EnterCriticalSection(&DiskCritSect));
    BOOL ret = TRUE;
    Work.Add(work);
    if (!Work.IsGood())
    {
        Work.ResetState();
        ret = FALSE;
    }
    if (Work.Count == 1)
        SetEvent(ContEvent);
    HANDLES(LeaveCriticalSection(&DiskCritSect));
    return ret;
}

BOOL CFTPDiskThread::CancelWork(const CFTPDiskWork* work, BOOL* workIsInProgress)
{
    CALL_STACK_MESSAGE1("CFTPDiskThread::CancelWork()");
    HANDLES(EnterCriticalSection(&DiskCritSect));
    BOOL ret = FALSE; // nenalezeno = prace uz je hotova a vyhozena z pole Work
    if (workIsInProgress != NULL)
        *workIsInProgress = FALSE;
    int i;
    for (i = 0; i < Work.Count; i++)
    {
        if (Work[i] == work)
        {
            ret = TRUE;
            if (i == 0)
            {
                Work[0] = NULL; // prvni polozka se muze prave zpracovavat, nelze ji vyhodit z pole (pro detekci jejiho cancelu se prepisuje na NULL)
                if (workIsInProgress != NULL)
                    *workIsInProgress = WorkIsInProgress;
            }
            else // prace se jeste urcite nedostala ke zpracovani, muzeme ji jednoduse vyhodit
            {
                Work.Detach(i);
                if (!Work.IsGood())
                    Work.ResetState();
            }
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&DiskCritSect));
    return ret;
}

BOOL CFTPDiskThread::AddFileToClose(const char* path, const char* name, HANDLE file, BOOL deleteIfEmpty,
                                    BOOL setDateAndTime, const CFTPDate* date, const CFTPTime* time,
                                    BOOL deleteFile, CQuadWord* setEndOfFile, int* fileCloseIndex)
{
    CALL_STACK_MESSAGE1("CFTPDiskThread::AddFileToClose()");
    HANDLES(EnterCriticalSection(&DiskCritSect));
    BOOL ret = FALSE;
    if (fileCloseIndex != NULL)
        *fileCloseIndex = -1;
    CFTPFileToClose* n = new CFTPFileToClose(path, name, file, deleteIfEmpty, setDateAndTime,
                                             date, time, deleteFile, setEndOfFile);
    if (n != NULL)
    {
        FilesToClose.Add(n);
        if (!FilesToClose.IsGood())
        {
            FilesToClose.ResetState();
            delete n;
        }
        else
        {
            ret = TRUE;
            if (fileCloseIndex != NULL)
                *fileCloseIndex = NextFileCloseIndex;
            NextFileCloseIndex++;
        }
        if (FilesToClose.Count == 1)
            SetEvent(ContEvent);
    }
    else
        TRACE_E(LOW_MEMORY);
    HANDLES(LeaveCriticalSection(&DiskCritSect));
    return ret;
}

BOOL CFTPDiskThread::WaitForFileClose(int fileCloseIndex, DWORD timeout)
{
    CALL_STACK_MESSAGE3("CFTPDiskThread::WaitForFileClose(%d, %u)", fileCloseIndex, timeout);
    DWORD ti = GetTickCount();
    BOOL closed = FALSE;
    while (1)
    {
        HANDLES(EnterCriticalSection(&DiskCritSect));
        if (DoneFileCloseIndex != -1 && fileCloseIndex <= DoneFileCloseIndex)
            closed = TRUE;
        HANDLES(LeaveCriticalSection(&DiskCritSect));
        if (closed)
            break;
        DWORD elapsed = GetTickCount() - ti;
        if (FileClosedEvent != NULL)
        {
            DWORD res = WaitForSingleObject(FileClosedEvent, timeout == INFINITE ? INFINITE : (elapsed < timeout ? timeout - elapsed : 0));
            if (res != WAIT_OBJECT_0)
            {
                TRACE_I("CFTPDiskThread::WaitForFileClose(): waiting for file closure has timed out!");
                break; // timeout nebo abadoned (nemelo by byt potreba)
            }
        }
        else // always false (jen kdyby se nepodarilo vytvorit FileClosedEvent) -> pouzijeme "aktivni" cekani
        {
            if (timeout != INFINITE && elapsed >= timeout)
                break; // timeout
            Sleep(100);
        }
    }
    return closed;
}

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES (-1)
#endif // INVALID_FILE_ATTRIBUTES

void DoCreateDir(CFTPDiskWork& localWork, char* fullName, BOOL& workDone, BOOL& needCopyBack,
                 char* nameBackup, char* suffix)
{
    BOOL isValid = SalamanderGeneral->SalIsValidFileNameComponent(localWork.Name);
    DWORD winErr = NO_ERROR;
    BOOL alreadyExists = FALSE;
    BOOL tooLongName = FALSE;
    if (isValid)
    {
        strcpy(fullName, localWork.Path);
        if (!SalamanderGeneral->SalPathAppend(fullName, localWork.Name, PATH_MAX_PATH))
        {
            winErr = ERROR_FILENAME_EXCED_RANGE; // "file name is too long"
            tooLongName = TRUE;
        }
        else
        {
            DWORD e;
            if (!SalamanderGeneral->SalCreateDirectoryEx(fullName, &e))
            {
                winErr = e;
                if (winErr != ERROR_ALREADY_EXISTS && winErr != ERROR_FILE_EXISTS)
                { // overime jestli chybu nezpusobil existujici soubor/adresar, pripadne pokracujeme ve hledani jmena pres autorename
                    DWORD attr = SalamanderGeneral->SalGetFileAttributes(fullName);
                    if (attr != INVALID_FILE_ATTRIBUTES)
                        winErr = ERROR_ALREADY_EXISTS;
                }
                if (winErr == ERROR_ALREADY_EXISTS || winErr == ERROR_FILE_EXISTS)
                {
                    DWORD attr = SalamanderGeneral->SalGetFileAttributes(fullName);
                    if (attr == INVALID_FILE_ATTRIBUTES)
                        winErr = GetLastError();
                    else
                    {
                        if (attr & FILE_ATTRIBUTE_DIRECTORY)
                            alreadyExists = TRUE;
                    }
                }
            }
            else
                workDone = TRUE;
        }
    }
    else
        winErr = ERROR_INVALID_NAME; // incorrect syntax of dir name

    if (winErr != NO_ERROR)
    {
        int action = 0;                                                   // 0 - nic, 1 - autorename
        if (localWork.ForceAction == fqiaUseAutorename ||                 // zkusime autorename
            localWork.ForceAction == fqiaUseExistingDir && alreadyExists) // adresar jiz existuje a forcujeme "use existing dir" -> vratime uspech
        {
            if (localWork.ForceAction == fqiaUseAutorename)
                action = 1;
        }
        else
        {
            if (alreadyExists)
            { // dir already exists
                switch (localWork.DirAlreadyExists)
                {
                case DIRALREADYEXISTS_USERPROMPT:
                {
                    localWork.ProblemID = ITEMPR_TGTDIRALREADYEXISTS;
                    localWork.State = sqisUserInputNeeded;
                    needCopyBack = TRUE;
                    break;
                }

                case DIRALREADYEXISTS_AUTORENAME:
                    action = 1;
                    break;
                case DIRALREADYEXISTS_JOIN:
                    break; // vratime uspech

                case DIRALREADYEXISTS_SKIP:
                {
                    localWork.ProblemID = ITEMPR_TGTDIRALREADYEXISTS;
                    localWork.State = sqisSkipped;
                    needCopyBack = TRUE;
                    break;
                }
                }
            }
            else // cannot create dir
            {
                switch (localWork.CannotCreateDir)
                {
                case CANNOTCREATENAME_USERPROMPT:
                {
                    localWork.ProblemID = ITEMPR_CANNOTCREATETGTDIR;
                    localWork.WinError = winErr;
                    localWork.State = sqisUserInputNeeded;
                    needCopyBack = TRUE;
                    break;
                }

                case CANNOTCREATENAME_AUTORENAME:
                    action = 1;
                    break;

                case CANNOTCREATENAME_SKIP:
                {
                    localWork.ProblemID = ITEMPR_CANNOTCREATETGTDIR;
                    localWork.WinError = winErr;
                    localWork.State = sqisSkipped;
                    needCopyBack = TRUE;
                    break;
                }
                }
            }
        }

        if (action == 1) // autorename (pri already exists, cannot create i force action)
        {
            BOOL ok = FALSE;
            if (!isValid)
            {
                SalamanderGeneral->SalMakeValidFileNameComponent(localWork.Name);
                needCopyBack = TRUE;
            }
            strcpy(fullName, localWork.Path);
            char* pathEnd = fullName + strlen(fullName);
            if (pathEnd > fullName && *(pathEnd - 1) != '\\')
                *pathEnd++ = '\\';
            int rest = PATH_MAX_PATH - (int)(pathEnd - fullName);
            if (rest < 0)
                rest = 0; // teoreticky by nemelo nikdy nastat, ale rozsireni Windows to umi (viz cesty zacinajici na "\\?\")
            int nameLen = (int)strlen(localWork.Name);
            memcpy(nameBackup, localWork.Name, nameLen + 1);
            BOOL firstRound = TRUE;
            int itemProblem = ITEMPR_CANNOTCREATETGTDIR;
            int suffixCounter = 1;
            while (1)
            {
                if (firstRound && !isValid && nameLen < rest) // testneme "zvalidnele" jmeno
                    memcpy(pathEnd, localWork.Name, nameLen + 1);
                else // vlozime cislovani na konec jmena (pripona u adresare neexistuje) (napr. "(2)") + pripadne orizneme aby se jmeno veslo do plne cesty
                {
                    if (firstRound && (tooLongName || !isValid) ||
                        winErr == ERROR_FILE_EXISTS || winErr == ERROR_ALREADY_EXISTS)
                    {
                        if (firstRound && tooLongName)
                            suffix[0] = 0;
                        else
                            sprintf(suffix, " (%d)", ++suffixCounter);
                        int suffixLen = (int)strlen(suffix);
                        if (suffixLen + 1 < rest) // musi se vejit aspon 1 znak ze jmena + suffix
                        {
                            // vygenerujeme nove jmeno
                            memcpy(localWork.Name, nameBackup, nameLen);
                            needCopyBack = TRUE;
                            if (nameLen + suffixLen < rest)
                                memcpy(localWork.Name + nameLen, suffix, suffixLen + 1);
                            else
                                memcpy(localWork.Name + rest - (suffixLen + 1), suffix, suffixLen + 1);
                            if (!SalamanderGeneral->SalIsValidFileNameComponent(localWork.Name))
                            { // vzniklo jmeno, ktere neni OK, musime ho upravit
                                int newLen = (int)strlen(localWork.Name);
                                if (newLen + 1 >= rest)
                                {
                                    if (newLen > 1)
                                        localWork.Name[newLen - 1] = 0;
                                    else
                                        localWork.Name[0] = '_';
                                }
                                SalamanderGeneral->SalMakeValidFileNameComponent(localWork.Name);
                            }
                            lstrcpyn(pathEnd, localWork.Name, rest); // vytvorime pro nove jmeno plne jmeno
                        }
                        else
                        {
                            winErr = ERROR_FILENAME_EXCED_RANGE; // "file name is too long"
                            break;
                        }
                    }
                    else
                        break; // dalsi pokus o vytvareni adresare nema smysl (nejde o kolizi jmen ani o syntakticky chybne jmeno ani prilis dlouhe jmeno), vratime chybu
                }
                // naalokujeme nove jmeno
                if (localWork.NewTgtName != NULL)
                    free(localWork.NewTgtName);
                localWork.NewTgtName = SalamanderGeneral->DupStr(localWork.Name);
                if (localWork.NewTgtName == NULL)
                {
                    itemProblem = ITEMPR_LOWMEM;
                    winErr = NO_ERROR;
                    break;
                }
                // zkusime dalsi jmeno adresare
                if (!CreateDirectory(fullName, NULL))
                {
                    winErr = GetLastError();
                    if (winErr != ERROR_ALREADY_EXISTS && winErr != ERROR_FILE_EXISTS)
                    { // overime jestli chybu nezpusobil existujici soubor/adresar, pripadne pokracujeme ve hledani jmena pres autorename
                        DWORD attr = SalamanderGeneral->SalGetFileAttributes(fullName);
                        if (attr != INVALID_FILE_ATTRIBUTES)
                            winErr = ERROR_ALREADY_EXISTS;
                    }
                }
                else
                {
                    workDone = TRUE;
                    ok = TRUE;
                    break; // uspech, vracime OK + nove jmeno
                }
                firstRound = FALSE;
            }

            if (!ok)
            {
                if (localWork.NewTgtName != NULL)
                    free(localWork.NewTgtName);
                localWork.NewTgtName = NULL;
                localWork.ProblemID = itemProblem;
                localWork.WinError = winErr;
                localWork.State = sqisFailed;                 // muze se jeste upravit v kodu o par radek nize
                if (itemProblem == ITEMPR_CANNOTCREATETGTDIR) // hlasena chyba je "unable to create or open file"
                {
                    switch (localWork.CannotCreateDir)
                    {
                    case CANNOTCREATENAME_USERPROMPT:
                        localWork.State = sqisUserInputNeeded;
                        break;
                    // case CANNOTCREATENAME_AUTORENAME: localWork.State = sqisFailed; break;
                    case CANNOTCREATENAME_SKIP:
                        localWork.State = sqisSkipped;
                        break;
                    }
                }
                needCopyBack = TRUE;
            }
        }
    }
}

CFTPQueueItemState DoCreateFileGetWantedErrorState(CFTPDiskWork& localWork)
{                                          // pomocna funkce pro DoCreateFile()
    CFTPQueueItemState state = sqisFailed; // muze se jeste upravit v kodu o par radek nize
    switch (localWork.CannotCreateFile)    // hlasena chyba je "unable to create or open file"
    {
    case CANNOTCREATENAME_USERPROMPT:
        state = sqisUserInputNeeded;
        break;
    // case CANNOTCREATENAME_AUTORENAME: state = sqisFailed; break;
    case CANNOTCREATENAME_SKIP:
        state = sqisSkipped;
        break;
    }
    return state;
}

void DoCreateFile(CFTPDiskWork& localWork, char* fullName, BOOL& workDone, BOOL& needCopyBack,
                  char* nameBackup, char* suffix)
{
    BOOL isValid = SalamanderGeneral->SalIsValidFileNameComponent(localWork.Name);
    DWORD winErr = NO_ERROR;
    BOOL alreadyExists = FALSE;
    BOOL tooLongName = FALSE;
    HANDLE file = NULL;
    if (isValid)
    {
        strcpy(fullName, localWork.Path);
        if (!SalamanderGeneral->SalPathAppend(fullName, localWork.Name, MAX_PATH))
        {
            winErr = ERROR_FILENAME_EXCED_RANGE; // "file name is too long"
            tooLongName = TRUE;
        }
        else
        {
            DWORD salCrErr;
            file = SalamanderGeneral->SalCreateFileEx(fullName, GENERIC_WRITE, FILE_SHARE_READ, FILE_FLAG_SEQUENTIAL_SCAN, &salCrErr);
            SetLastError(salCrErr);
            HANDLES_ADD_EX(__otQuiet, file != INVALID_HANDLE_VALUE, __htFile,
                           __hoCreateFile, file, salCrErr, TRUE);
            if (file == INVALID_HANDLE_VALUE) // nelze vytvorit novy soubor
            {
                winErr = GetLastError();
                if (winErr != ERROR_ALREADY_EXISTS && winErr != ERROR_FILE_EXISTS)
                { // overime jestli chybu nezpusobil existujici soubor/adresar, pripadne pokracujeme ve hledani jmena pres autorename
                    DWORD attr = SalamanderGeneral->SalGetFileAttributes(fullName);
                    if (attr != INVALID_FILE_ATTRIBUTES)
                        winErr = ERROR_ALREADY_EXISTS;
                }
                if (winErr == ERROR_ALREADY_EXISTS || winErr == ERROR_FILE_EXISTS)
                {
                    DWORD attr = SalamanderGeneral->SalGetFileAttributes(fullName);
                    if (attr == INVALID_FILE_ATTRIBUTES)
                        winErr = GetLastError();
                    else
                    {
                        if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
                            alreadyExists = TRUE;
                    }
                }
            }
            else
            {
                workDone = TRUE; // pokud dojde ke cancelu operace, nove vytvoreny soubor smazeme
                localWork.OpenedFile = file;
                localWork.FileSize.Set(0, 0);
                localWork.CanOverwrite = TRUE;       // soubor byl nove vytvoren (nebyl resumnuty)
                localWork.CanDeleteEmptyFile = TRUE; // soubor byl nove vytvoren (nebyl resumnuty)
                needCopyBack = TRUE;
            }
        }
    }
    else
        winErr = ERROR_INVALID_NAME; // incorrect syntax of file name

    if (winErr != NO_ERROR)
    {
        int action = 0; // 0 - nic, 1 - autorename, 2 - resume, 3 - resume or overwrite, 4 - overwrite
        BOOL reduceFileSize = FALSE;
        if (localWork.ForceAction == fqiaUseAutorename)
            action = 1;
        else
        {
            if (alreadyExists)
            {
                switch (localWork.ForceAction)
                {
                case fqiaResume:
                    action = 2;
                    break;
                case fqiaResumeOrOverwrite:
                    action = 3;
                    break;
                case fqiaOverwrite:
                    action = 4;
                    break;
                case fqiaReduceFileSizeAndResume:
                    action = 2;
                    reduceFileSize = TRUE;
                    break;
                }
            }
        }
        if (action == 0)
        {
            if (alreadyExists)
            { // file already exists
                switch (localWork.Type)
                {
                case fdwtCreateFile:
                {
                    switch (localWork.FileAlreadyExists)
                    {
                    case FILEALREADYEXISTS_USERPROMPT:
                    {
                        localWork.ProblemID = ITEMPR_TGTFILEALREADYEXISTS;
                        localWork.State = sqisUserInputNeeded;
                        needCopyBack = TRUE;
                        break;
                    }

                    case FILEALREADYEXISTS_AUTORENAME:
                        action = 1;
                        break;
                    case FILEALREADYEXISTS_RESUME:
                        action = 2;
                        break;
                    case FILEALREADYEXISTS_RES_OVRWR:
                        action = 3;
                        break;
                    case FILEALREADYEXISTS_OVERWRITE:
                        action = 4;
                        break;

                    case FILEALREADYEXISTS_SKIP:
                    {
                        localWork.ProblemID = ITEMPR_TGTFILEALREADYEXISTS;
                        localWork.State = sqisSkipped;
                        needCopyBack = TRUE;
                        break;
                    }
                    }
                    break;
                }

                case fdwtRetryCreatedFile:
                {
                    switch (localWork.RetryOnCreatedFile)
                    {
                    case RETRYONCREATFILE_USERPROMPT:
                    {
                        localWork.ProblemID = ITEMPR_RETRYONCREATFILE;
                        localWork.State = sqisUserInputNeeded;
                        needCopyBack = TRUE;
                        break;
                    }

                    case RETRYONCREATFILE_AUTORENAME:
                        action = 1;
                        break;
                    case RETRYONCREATFILE_RESUME:
                        action = 2;
                        break;
                    case RETRYONCREATFILE_RES_OVRWR:
                        action = 3;
                        break;
                    case RETRYONCREATFILE_OVERWRITE:
                        action = 4;
                        break;

                    case RETRYONCREATFILE_SKIP:
                    {
                        localWork.ProblemID = ITEMPR_RETRYONCREATFILE;
                        localWork.State = sqisSkipped;
                        needCopyBack = TRUE;
                        break;
                    }
                    }
                    break;
                }

                case fdwtRetryResumedFile:
                {
                    switch (localWork.RetryOnResumedFile)
                    {
                    case RETRYONRESUMFILE_USERPROMPT:
                    {
                        localWork.ProblemID = ITEMPR_RETRYONRESUMFILE;
                        localWork.State = sqisUserInputNeeded;
                        needCopyBack = TRUE;
                        break;
                    }

                    case RETRYONRESUMFILE_AUTORENAME:
                        action = 1;
                        break;
                    case RETRYONRESUMFILE_RESUME:
                        action = 2;
                        break;
                    case RETRYONRESUMFILE_RES_OVRWR:
                        action = 3;
                        break;
                    case RETRYONRESUMFILE_OVERWRITE:
                        action = 4;
                        break;

                    case RETRYONRESUMFILE_SKIP:
                    {
                        localWork.ProblemID = ITEMPR_RETRYONRESUMFILE;
                        localWork.State = sqisSkipped;
                        needCopyBack = TRUE;
                        break;
                    }
                    }
                    break;
                }
                }
            }
            else // cannot create file
            {
                switch (localWork.CannotCreateFile)
                {
                case CANNOTCREATENAME_USERPROMPT:
                {
                    localWork.ProblemID = ITEMPR_CANNOTCREATETGTFILE;
                    localWork.WinError = winErr;
                    localWork.State = sqisUserInputNeeded;
                    needCopyBack = TRUE;
                    break;
                }

                case CANNOTCREATENAME_AUTORENAME:
                    action = 1;
                    break;

                case CANNOTCREATENAME_SKIP:
                {
                    localWork.ProblemID = ITEMPR_CANNOTCREATETGTFILE;
                    localWork.WinError = winErr;
                    localWork.State = sqisSkipped;
                    needCopyBack = TRUE;
                    break;
                }
                }
            }
        }

        DWORD attr = 0;
        BOOL readonly = FALSE;
        switch (action)
        {
        case 1: // autorename (pri already exists, transfer failed, cannot create i force action)
        {
            BOOL ok = FALSE;
            if (!isValid)
            {
                SalamanderGeneral->SalMakeValidFileNameComponent(localWork.Name);
                needCopyBack = TRUE;
            }
            strcpy(fullName, localWork.Path);
            char* pathEnd = fullName + strlen(fullName);
            if (pathEnd > fullName && *(pathEnd - 1) != '\\')
                *pathEnd++ = '\\';
            int rest = MAX_PATH - (int)(pathEnd - fullName);
            if (rest < 0)
                rest = 0; // teoreticky by nemelo nikdy nastat, ale rozsireni Windows to umi (viz cesty zacinajici na "\\?\")
            int suffixCounter = 1;
            BOOL firstRound = TRUE;
            if (isValid && localWork.AlreadyRenamedName) // druhe kolo prejmenovani: zajistime "name (2)"->"name (3)" misto ->"name (2) (2)"
            {
                char* s = localWork.Name + strlen(localWork.Name);
                while (--s >= localWork.Name)
                {
                    if (*s == ')') // hledame odzadu " (cislo)"
                    {
                        char* end = s + 1;
                        int num = 0;
                        int digit = 1;
                        while (--s >= localWork.Name && *s >= '0' && *s <= '9')
                        {
                            num += digit * (*s - '0');
                            digit *= 10;
                        }
                        if (s > localWork.Name && *s == '(' && *(s - 1) == ' ')
                        {
                            memmove(s - 1, end, strlen(end) + 1);
                            suffixCounter = num;
                            break;
                        }
                    }
                }
                firstRound = FALSE; // chceme hned zkouset suffixy (i kdyz ho "jiz prejmenovane jmeno" neobsahuje)
            }
            int nameLen = (int)strlen(localWork.Name);
            int extOffset = nameLen;
            char* ext = strrchr(localWork.Name, '.');
            //        if (ext != NULL && ext != localWork.Name) // ".cvspass" ve Windows je pripona ...
            if (ext != NULL)
                extOffset = (int)(ext - localWork.Name);
            memcpy(nameBackup, localWork.Name, nameLen + 1);
            int itemProblem = ITEMPR_CANNOTCREATETGTFILE;
            while (1)
            {
                if (firstRound && !isValid && nameLen < rest) // testneme "zvalidnele" jmeno
                    memcpy(pathEnd, localWork.Name, nameLen + 1);
                else // vlozime cislovani na konec jmena (pripona u adresare neexistuje) (napr. "(2)") + pripadne orizneme aby se jmeno veslo do plne cesty
                {
                    if (firstRound && (tooLongName || !isValid) ||
                        winErr == ERROR_FILE_EXISTS || winErr == ERROR_ALREADY_EXISTS)
                    {
                        if (firstRound && tooLongName)
                            suffix[0] = 0;
                        else
                            sprintf(suffix, " (%d)", ++suffixCounter);
                        int suffixLen = (int)strlen(suffix);
                        if (suffixLen + 1 < rest) // musi se vejit aspon 1 znak ze jmena + suffix
                        {
                            // vygenerujeme nove jmeno
                            needCopyBack = TRUE;
                            if (suffixLen + 1 + (nameLen - extOffset) < rest) // pokud se vejde aspon 1 znak ze jmena + suffix + pripona
                            {                                                 // sestavime: co nejvetsi cast jmena + suffix + pripona
                                int off = (nameLen + suffixLen < rest) ? extOffset : (rest - 1 - suffixLen - (nameLen - extOffset));
                                memcpy(localWork.Name, nameBackup, off);
                                memcpy(localWork.Name + off, suffix, suffixLen);
                                memcpy(localWork.Name + off + suffixLen, nameBackup + extOffset, nameLen - extOffset + 1);
                            }
                            else // cela pripona se nevejde, zkratime jmeno bez rozliseni pripony
                            {
                                memcpy(localWork.Name, nameBackup, rest - (suffixLen + 1));
                                memcpy(localWork.Name + rest - (suffixLen + 1), suffix, suffixLen + 1);
                            }
                            if (!SalamanderGeneral->SalIsValidFileNameComponent(localWork.Name))
                            { // vzniklo jmeno, ktere neni OK, musime ho upravit
                                int newLen = (int)strlen(localWork.Name);
                                if (newLen + 1 >= rest)
                                {
                                    if (newLen > 1)
                                        localWork.Name[newLen - 1] = 0;
                                    else
                                        localWork.Name[0] = '_';
                                }
                                SalamanderGeneral->SalMakeValidFileNameComponent(localWork.Name);
                            }
                            lstrcpyn(pathEnd, localWork.Name, rest); // vytvorime pro nove jmeno plne jmeno
                        }
                        else
                        {
                            winErr = ERROR_FILENAME_EXCED_RANGE; // "file name is too long"
                            break;
                        }
                    }
                    else
                        break; // dalsi pokus o vytvareni souboru nema smysl (nejde o kolizi jmen ani o syntakticky chybne jmeno ani prilis dlouhe jmeno), vratime chybu
                }
                // naalokujeme nove jmeno
                if (localWork.NewTgtName != NULL)
                    free(localWork.NewTgtName);
                localWork.NewTgtName = SalamanderGeneral->DupStr(localWork.Name);
                if (localWork.NewTgtName == NULL)
                {
                    itemProblem = ITEMPR_LOWMEM;
                    winErr = NO_ERROR;
                    break;
                }
                // zkusime dalsi jmeno souboru
                file = HANDLES_Q(CreateFile(fullName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                            CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                if (file == INVALID_HANDLE_VALUE)
                {
                    winErr = GetLastError();
                    if (winErr != ERROR_ALREADY_EXISTS && winErr != ERROR_FILE_EXISTS)
                    { // overime jestli chybu nezpusobil existujici soubor/adresar, pripadne pokracujeme ve hledani jmena pres autorename
                        DWORD attr2 = SalamanderGeneral->SalGetFileAttributes(fullName);
                        if (attr2 != INVALID_FILE_ATTRIBUTES)
                            winErr = ERROR_ALREADY_EXISTS;
                    }
                }
                else
                {
                    workDone = TRUE; // pokud dojde ke cancelu operace, nove vytvoreny soubor smazeme
                    ok = TRUE;
                    localWork.OpenedFile = file;
                    localWork.FileSize.Set(0, 0);
                    localWork.CanOverwrite = TRUE;       // soubor byl nove vytvoren (nebyl resumnuty)
                    localWork.CanDeleteEmptyFile = TRUE; // soubor byl nove vytvoren (nebyl resumnuty)
                    needCopyBack = TRUE;
                    break; // uspech, vracime OK + nove jmeno
                }
                firstRound = FALSE;
            }

            if (!ok)
            {
                if (localWork.NewTgtName != NULL)
                    free(localWork.NewTgtName);
                localWork.NewTgtName = NULL;
                localWork.ProblemID = itemProblem;
                localWork.WinError = winErr;
                localWork.State = (itemProblem == ITEMPR_CANNOTCREATETGTFILE ? DoCreateFileGetWantedErrorState(localWork) : sqisFailed);
                needCopyBack = TRUE;
            }
            break;
        }

        case 2: // resume (pri already exists, transfer failed i force action) + je-li reduceFileSize==TRUE, pak mame tez zkratit soubor
        case 3: // resume or overwrite (pri already exists, transfer failed i force action)
        {
            file = HANDLES_Q(CreateFile(fullName,
                                        GENERIC_READ /* budeme cist a kontrolovat prekryv */ |
                                            GENERIC_WRITE,
                                        FILE_SHARE_READ, NULL,
                                        OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
            if (file == INVALID_HANDLE_VALUE) // nelze otevrit soubor
            {
                winErr = GetLastError();
                // overime jestli neni nahodou read-only (jen pres atribut)
                attr = SalamanderGeneral->SalGetFileAttributes(fullName);
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY))
                { // zkusime read-only shodit a znovu otevrit soubor
                    readonly = TRUE;
                    SetFileAttributes(fullName, attr & (~FILE_ATTRIBUTE_READONLY));
                    file = HANDLES_Q(CreateFile(fullName,
                                                GENERIC_READ /* budeme cist a kontrolovat prekryv */ |
                                                    GENERIC_WRITE,
                                                FILE_SHARE_READ, NULL,
                                                OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                    winErr = GetLastError();
                }
            }
            BOOL denyOverwrite = FALSE;
            if (file != INVALID_HANDLE_VALUE) // soubor je otevreny
            {
                denyOverwrite = TRUE; // soubor se podarilo otevrit, nema smysl ho prepisovat pri chybe zjisteni velikosti souboru
                CQuadWord size;
                size.LoDWord = GetFileSize(file, &size.HiDWord);
                if (size.LoDWord != INVALID_FILE_SIZE || (winErr = GetLastError()) == NO_ERROR)
                {
                    BOOL ok = TRUE;
                    if (reduceFileSize) // jeste mame zkratit soubor, takze jdeme na to
                    {
                        DWORD resumeOverlap = Config.GetResumeOverlap();
                        if (resumeOverlap == 0)
                            resumeOverlap = 1; // alespon o jeden byte (hrozi jen pokud to prave prepsal user v konfiguraci)
                        if (size < CQuadWord(resumeOverlap, 0))
                            resumeOverlap = (DWORD)size.Value;
                        size -= CQuadWord(resumeOverlap, 0);

                        CQuadWord curSeek = size;
                        curSeek.LoDWord = SetFilePointer(file, curSeek.LoDWord, (LONG*)&curSeek.HiDWord, FILE_BEGIN);
                        if (curSeek.LoDWord == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ||
                            curSeek != size)
                        { // chyba: nelze nastavit seek v souboru
                            ok = FALSE;
                            winErr = GetLastError();
                            HANDLES(CloseHandle(file)); // soubor neni treba mazat, protoze jeste pred okamzikem existoval (da se ocekavat, ze nezmizel => nevytvorili jsme ho)
                        }
                        else
                        {
                            if (!SetEndOfFile(file))
                            { // chyba: soubor nelze zkratit
                                ok = FALSE;
                                winErr = GetLastError();
                                HANDLES(CloseHandle(file)); // soubor neni treba mazat, protoze jeste pred okamzikem existoval (da se ocekavat, ze nezmizel => nevytvorili jsme ho)
                            }
                            else
                            {
                                curSeek.Set(0, 0);
                                curSeek.LoDWord = SetFilePointer(file, curSeek.LoDWord, (LONG*)&curSeek.HiDWord, FILE_BEGIN);
                                if (curSeek.LoDWord == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ||
                                    curSeek != CQuadWord(0, 0))
                                { // chyba: nelze seeknout zpet na zacatek souboru
                                    ok = FALSE;
                                    winErr = GetLastError();
                                    HANDLES(CloseHandle(file)); // soubor neni treba mazat, protoze jeste pred okamzikem existoval (da se ocekavat, ze nezmizel => nevytvorili jsme ho)
                                }
                            }
                        }
                    }

                    if (ok)
                    {
                        // workDone = TRUE;  // soubor pri cancelu operace nebudeme mazat, jde o resume
                        localWork.OpenedFile = file;
                        localWork.FileSize = size;
                        localWork.CanOverwrite = (action == 3 /* resume or overwrite */); // FALSE = soubor byl resumnuty
                        localWork.CanDeleteEmptyFile = FALSE;                             // soubor byl resumnuty (nesmazeme ho ani pokud ma nulovou velikost)
                        needCopyBack = TRUE;
                        break; // uspech, koncime
                    }
                }
                else // chyba pri zjistovani velikosti souboru, zavreme soubor a koncime s chybou
                {
                    HANDLES(CloseHandle(file)); // soubor neni treba mazat, protoze jeste pred okamzikem existoval (da se ocekavat, ze nezmizel => nevytvorili jsme ho)
                }
            }

            if (action == 2 /* resume */ || denyOverwrite)
            {
                if (readonly)
                    SetFileAttributes(fullName, attr); // vratime read-only atribut (soubor se nepovedl otevrit)

                localWork.ProblemID = ITEMPR_CANNOTCREATETGTFILE;
                localWork.WinError = winErr;
                localWork.State = DoCreateFileGetWantedErrorState(localWork);
                needCopyBack = TRUE;
                break; // chyba resume, koncime
            }
            // break;  // resume or overwrite - pri chybe resume se jde zkusit jeste overwrite
        }
        // case 3:  // resume or overwrite - pri chybe resume se jde zkusit jeste overwrite
        case 4: // overwrite (pri already exists, transfer failed i force action)
        {
            if (action == 4) // overime jestli neni nahodou read-only (jen pres atribut), jen pokud uz jsme to nedelali
            {
                attr = SalamanderGeneral->SalGetFileAttributes(fullName);
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY))
                { // zkusime read-only shodit
                    readonly = TRUE;
                    SetFileAttributes(fullName, attr & (~FILE_ATTRIBUTE_READONLY));
                }
            }
            file = HANDLES_Q(CreateFile(fullName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                        CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
            if (file == INVALID_HANDLE_VALUE) // nelze otevrit soubor
            {
                winErr = GetLastError();

                // resi situaci, kdy je potreba prepsat soubor na Sambe:
                // soubor ma 440+jinyho_vlastnika a je v adresari, kam ma akt. user zapis
                // (smazat lze, ale primo prepsat ne (nelze otevrit pro zapis) - obchazime:
                //  smazeme+vytvorime soubor znovu)
                // (na Sambe lze povolit mazani read-only, coz umozni delete read-only souboru,
                //  jinak nelze smazat, protoze Windows neumi smazat read-only soubor a zaroven
                //  u toho souboru nelze shodit "read-only" atribut, protoze akt. user neni vlastnik)
                if (DeleteFile(fullName)) // je-li read-only, pujde smazat jedine na Sambe s povolenym "delete readonly"
                {
                    file = HANDLES_Q(CreateFile(fullName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                                CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                    winErr = GetLastError();
                }
            }

            if (file != INVALID_HANDLE_VALUE) // soubor je otevreny
            {
                workDone = TRUE; // pokud dojde ke cancelu operace, nove vytvoreny soubor smazeme
                localWork.OpenedFile = file;
                localWork.FileSize.Set(0, 0);
                localWork.CanOverwrite = TRUE;
                localWork.CanDeleteEmptyFile = TRUE; // soubor byl nove vytvoren (nebyl resumnuty)
                needCopyBack = TRUE;
            }
            else
            {
                if (readonly)
                    SetFileAttributes(fullName, attr); // vratime read-only atribut (soubor se nepovedl smazat)

                localWork.ProblemID = ITEMPR_CANNOTCREATETGTFILE;
                localWork.WinError = winErr;
                localWork.State = DoCreateFileGetWantedErrorState(localWork);
                needCopyBack = TRUE;
            }
            break;
        }
        }
    }
}

void DoCheckOrWriteToFile(CFTPDiskWork& localWork, BOOL& needCopyBack)
{
    if (localWork.WorkFile != NULL)
    {
        CQuadWord fileSize;
        fileSize.LoDWord = GetFileSize(localWork.WorkFile, &fileSize.HiDWord);
        if (fileSize.LoDWord == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
        { // chyba: nelze zjistit velikost souboru
            localWork.State = sqisFailed;
            localWork.ProblemID = ITEMPR_TGTFILEREADERROR;
            localWork.WinError = GetLastError();
            needCopyBack = TRUE;
        }
        else
        {
            BOOL writeFile = TRUE;
            BOOL skipSetSeekForWrite = FALSE;
            int flushBufOffset = 0;
            if (localWork.CheckFromOffset < localWork.WriteOrReadFromOffset) // jdeme zkontrolovat obsah souboru
            {
                writeFile = FALSE;
                if (localWork.CheckFromOffset >= fileSize)
                { // neocekavana chyba: mame testovat obsah souboru za koncem souboru - nejspis se soubor nedavno zmenil (nemelo by nastat, je otevreny "share-read-only")
                    localWork.State = sqisFailed;
                    localWork.ProblemID = ITEMPR_RESUMETESTFAILED;
                    needCopyBack = TRUE;
                }
                else // nastavime seek v souboru
                {
                    CQuadWord curSeek = localWork.CheckFromOffset;
                    curSeek.LoDWord = SetFilePointer(localWork.WorkFile, curSeek.LoDWord, (LONG*)&curSeek.HiDWord, FILE_BEGIN);
                    if (curSeek.LoDWord == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ||
                        curSeek != localWork.CheckFromOffset)
                    { // chyba: nelze nastavit seek v souboru
                        localWork.State = sqisFailed;
                        localWork.ProblemID = ITEMPR_TGTFILEREADERROR;
                        localWork.WinError = GetLastError();
                        needCopyBack = TRUE;
                    }
                    else // overime pozadovanou cast souboru proti obsahu bufferu
                    {
                        char buf[4096];                                                                           // buffer pro cteni z disku
                        int bytesToCheck = (localWork.WriteOrReadFromOffset - localWork.CheckFromOffset).LoDWord; // velikost overovaneho konce souboru je pod 1GB, takze toto oriznuti je mozne
                        if (bytesToCheck > localWork.ValidBytesInFlushDataBuffer)                                 // pripadne orizneme velikosti flush bufferu (nemusime overovat cely usek najednou)
                            bytesToCheck = localWork.ValidBytesInFlushDataBuffer;
                        while (bytesToCheck > 0)
                        {
                            DWORD check = min(bytesToCheck, 4096);
                            DWORD readBytes;
                            if (!ReadFile(localWork.WorkFile, buf, check, &readBytes, NULL) ||
                                check != readBytes)
                            { // chyba cteni souboru
                                localWork.State = sqisFailed;
                                localWork.ProblemID = ITEMPR_TGTFILEREADERROR;
                                localWork.WinError = GetLastError();
                                needCopyBack = TRUE;
                                break;
                            }
                            else
                            {
                                if (memcmp(buf, localWork.FlushDataBuffer + flushBufOffset, check) == 0)
                                { // shoda souboru a flush bufferu, vse OK
                                    bytesToCheck -= check;
                                    flushBufOffset += check;
                                }
                                else // v souboru je neco jineho nez ve flush bufferu (resume: soubor se od posledne zmenil)
                                {
                                    localWork.State = sqisFailed;
                                    localWork.ProblemID = ITEMPR_RESUMETESTFAILED;
                                    needCopyBack = TRUE;
                                    break;
                                }
                            }
                        }
                        writeFile = (bytesToCheck == 0); // pokud selhal check konce souboru, nema smysl zapisovat
                        skipSetSeekForWrite = TRUE;
                    }
                }
            }

            if (writeFile && flushBufOffset < localWork.ValidBytesInFlushDataBuffer) // jdeme zapsat flushovana data do souboru
            {
                if (localWork.WriteOrReadFromOffset > fileSize)
                { // neocekavana chyba: mame zapsat az kus za konec souboru (ten by se naplnil nahodnymi hodnotami, nepripustne) - nejspis se soubor nedavno zmenil (nemelo by nastat, je otevreny "share-read-only")
                    localWork.State = sqisFailed;
                    localWork.ProblemID = ITEMPR_RESUMETESTFAILED;
                    needCopyBack = TRUE;
                }
                else // nastavime seek v souboru
                {
                    if (!skipSetSeekForWrite)
                    {
                        CQuadWord curSeek = localWork.WriteOrReadFromOffset;
                        curSeek.LoDWord = SetFilePointer(localWork.WorkFile, curSeek.LoDWord, (LONG*)&curSeek.HiDWord, FILE_BEGIN);
                        if (curSeek.LoDWord == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ||
                            curSeek != localWork.WriteOrReadFromOffset)
                        { // chyba: nelze nastavit seek v souboru
                            localWork.State = sqisFailed;
                            localWork.ProblemID = ITEMPR_TGTFILEWRITEERROR;
                            localWork.WinError = GetLastError();
                            needCopyBack = TRUE;
                            writeFile = FALSE;
                        }
                    }

                    if (writeFile)
                    {
                        DWORD writtenBytes;
                        if (!WriteFile(localWork.WorkFile, localWork.FlushDataBuffer + flushBufOffset,
                                       localWork.ValidBytesInFlushDataBuffer - flushBufOffset, &writtenBytes, NULL) ||
                            writtenBytes != (DWORD)(localWork.ValidBytesInFlushDataBuffer - flushBufOffset))
                        {
                            localWork.State = sqisFailed;
                            localWork.ProblemID = ITEMPR_TGTFILEWRITEERROR;
                            localWork.WinError = GetLastError();
                            needCopyBack = TRUE;
                        }
                        else // uspesne zapsano, mame uspesne hotovo
                        {
                            if (localWork.WriteOrReadFromOffset + CQuadWord(writtenBytes, 0) < fileSize)
                            {                                     // pokud zapis skoncil pred koncem souboru, zavolame jeste SetEndOfFile (orez nechtenych starych dat, ktere se maji prepsat)
                                SetEndOfFile(localWork.WorkFile); // uspech netestujeme, vlastne na nem nezalezi
                            }
                        }
                    }
                }
            }
        }
    }
    else // ohlasime chybu
    {
        TRACE_E("DoCheckOrWriteToFile(): localWork.WorkFile may not be NULL!");
        localWork.State = sqisFailed;
        localWork.ProblemID = ITEMPR_LOWMEM;
        needCopyBack = TRUE;
    }
}

void DoCreateAndWriteFile(CFTPDiskWork& localWork, BOOL& needCopyBack, BOOL& workDone)
{
    HANDLE file = NULL;
    if (localWork.WorkFile == NULL) // soubor zatim nebyl vytvoren
    {
        SetFileAttributes(localWork.Name, FILE_ATTRIBUTE_NORMAL); // aby sel prepsat i read-only soubor
        HANDLE f = HANDLES_Q(CreateFile(localWork.Name, GENERIC_WRITE,
                                        FILE_SHARE_READ, NULL,
                                        CREATE_ALWAYS,
                                        FILE_FLAG_SEQUENTIAL_SCAN,
                                        NULL));
        if (f != INVALID_HANDLE_VALUE)
        {
            file = f;
            localWork.OpenedFile = f;
            workDone = TRUE;     // pokud dojde ke cancelu, zavreme handle souboru a smazeme soubor
            needCopyBack = TRUE; // vracime handle vytvoreneho souboru
        }
        else // chyba pri vytvareni souboru
        {
            localWork.State = sqisFailed;
            localWork.WinError = GetLastError();
            needCopyBack = TRUE; // vracime chybu
        }
    }
    else
        file = localWork.WorkFile; // jen zapis

    if (file != NULL && localWork.ValidBytesInFlushDataBuffer > 0) // provedeme zapis do souboru
    {
        DWORD writtenBytes;
        if (!WriteFile(file, localWork.FlushDataBuffer, localWork.ValidBytesInFlushDataBuffer,
                       &writtenBytes, NULL) ||
            writtenBytes != (DWORD)localWork.ValidBytesInFlushDataBuffer)
        {
            localWork.State = sqisFailed;
            localWork.WinError = GetLastError();
            needCopyBack = TRUE; // vracime chybu
        }
        // else;  // uspesne zapsano, mame uspesne hotovo
    }
}

void DoListDirectory(CFTPDiskWork& localWork, BOOL& needCopyBack)
{
    char srcPath[MAX_PATH + 10];
    lstrcpyn(srcPath, localWork.Path, MAX_PATH);
    if (SalamanderGeneral->SalPathAppend(srcPath, localWork.Name, MAX_PATH))
    {
        localWork.DiskListing = new TIndirectArray<CDiskListingItem>(100, 500);
        if (localWork.DiskListing != NULL && localWork.DiskListing->IsGood())
        {
            SalamanderGeneral->SalPathAppend(srcPath, "*.*", MAX_PATH + 10); // nemuze selhat
            char* srcPathEnd = strrchr(srcPath, '\\');                       // tez nemuze selhat
            WIN32_FIND_DATA fileData;
            HANDLE search = HANDLES_Q(FindFirstFile(srcPath, &fileData));
            if (search == INVALID_HANDLE_VALUE)
            {
                DWORD err = GetLastError();
                if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES) // jde o chybu - aneb neni jen prazdny listing
                {
                    localWork.State = sqisFailed;
                    localWork.ProblemID = ITEMPR_UPLOADCANNOTLISTSRCPATH;
                    localWork.WinError = err;
                }
            }
            else
            {
                do
                {
                    char* s = fileData.cFileName;
                    if (*s == '.' && (*(s + 1) == 0 || *(s + 1) == '.' && *(s + 2) == 0))
                        continue; // "." a ".." preskocime
                    // linky: size == 0, velikost souboru se musi ziskat pres SalGetFileSize2() dodatecne
                    BOOL isDir = (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    CQuadWord size(fileData.nFileSizeLow, fileData.nFileSizeHigh);
                    if (!isDir && (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                        srcPathEnd != NULL && (srcPathEnd + 1) - srcPath + strlen(fileData.cFileName) < MAX_PATH)
                    { // jde o link a plne jmeno linku neni prilis dlouhe
                        strcpy(srcPathEnd + 1, fileData.cFileName);
                        DWORD err; // ziskame velikost ciloveho souboru
                        if (!SalamanderGeneral->SalGetFileSize2(srcPath, size, &err))
                        { // chyby ignorujeme, projevi se pozdeji + velikost nutne nepotrebujeme, pro ladici ucely aspon TRACE_E
                            TRACE_E("DoListDirectory(): unable to get link target file size, name: " << srcPath << ", error: " << SalamanderGeneral->GetErrorText(err));
                            size.Set(fileData.nFileSizeLow, fileData.nFileSizeHigh);
                        }
                    }
                    CDiskListingItem* item = new CDiskListingItem(s, isDir, size);
                    if (item != NULL && item->IsGood())
                    {
                        localWork.DiskListing->Add(item);
                        if (!localWork.DiskListing->IsGood())
                        {
                            delete item;
                            localWork.DiskListing->ResetState();
                            localWork.State = sqisFailed;
                            localWork.ProblemID = ITEMPR_LOWMEM;
                            break;
                        }
                    }
                    else
                    {
                        if (item != NULL)
                            delete item;
                        else
                            TRACE_E(LOW_MEMORY);
                        localWork.State = sqisFailed;
                        localWork.ProblemID = ITEMPR_LOWMEM;
                        break;
                    }
                } while (FindNextFile(search, &fileData));
                DWORD err = GetLastError();
                HANDLES(FindClose(search));
                if (localWork.State != sqisFailed && err != ERROR_NO_MORE_FILES)
                {
                    localWork.State = sqisFailed;
                    localWork.ProblemID = ITEMPR_UPLOADCANNOTLISTSRCPATH;
                    localWork.WinError = err;
                }
            }
        }
        else // low memory
        {
            TRACE_E(LOW_MEMORY);
            localWork.State = sqisFailed;
            localWork.ProblemID = ITEMPR_LOWMEM;
        }
    }
    else // prilis dlouha zdrojova cesta na disku
    {
        localWork.State = sqisFailed;
        localWork.ProblemID = ITEMPR_INVALIDPATHTODIR;
    }
    if (localWork.State == sqisFailed && localWork.DiskListing != NULL)
    {
        delete localWork.DiskListing;
        localWork.DiskListing = NULL;
    }
    needCopyBack = TRUE; // vracime listing nebo chybu
}

void DoDeleteDir(CFTPDiskWork& localWork, BOOL& needCopyBack)
{
    char delPath[MAX_PATH + 10];
    lstrcpyn(delPath, localWork.Path, MAX_PATH);
    if (SalamanderGeneral->SalPathAppend(delPath, localWork.Name, MAX_PATH))
    {
        DWORD attr = SalamanderGeneral->SalGetFileAttributes(delPath);
        BOOL chAttrs = SalamanderGeneral->ClearReadOnlyAttr(delPath, attr); // aby sel smazat ...
        if (!RemoveDirectory(delPath))
        {
            DWORD err = GetLastError();
            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND)
            { // pokud jiz adresar neexistuje, je vse OK, jinak vypiseme chybu:
                if (chAttrs)
                    SetFileAttributes(delPath, attr); // smazat nesel, tak mu aspon zkusime nastavit atributy zpet
                localWork.State = sqisFailed;
                localWork.ProblemID = ITEMPR_UNABLETODELETEDISKDIR;
                localWork.WinError = err;
                needCopyBack = TRUE; // vracime chybu
            }
        }
    }
    else // prilis dlouha cesta na disku
    {
        localWork.State = sqisFailed;
        localWork.ProblemID = ITEMPR_INVALIDPATHTODIR;
        needCopyBack = TRUE; // vracime chybu
    }
}

void DoDeleteFile(CFTPDiskWork& localWork, BOOL& needCopyBack)
{
    char delPath[MAX_PATH + 10];
    lstrcpyn(delPath, localWork.Path, MAX_PATH);
    if (SalamanderGeneral->SalPathAppend(delPath, localWork.Name, MAX_PATH))
    {
        DWORD attr = SalamanderGeneral->SalGetFileAttributes(delPath);
        BOOL chAttrs = SalamanderGeneral->ClearReadOnlyAttr(delPath, attr); // aby sel smazat ...
        if (!DeleteFile(delPath))
        {
            DWORD err = GetLastError();
            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND)
            { // pokud jiz soubor neexistuje, je vse OK, jinak vypiseme chybu:
                if (chAttrs)
                    SetFileAttributes(delPath, attr); // smazat nesel, tak mu aspon zkusime nastavit atributy zpet
                localWork.State = sqisFailed;
                localWork.ProblemID = ITEMPR_UNABLETODELETEDISKFILE;
                localWork.WinError = err;
                needCopyBack = TRUE; // vracime chybu
            }
        }
    }
    else // prilis dlouha cesta na disku (nemelo by nikdy nastat - soubor se prave podarilo otevrit a precist)
    {
        localWork.State = sqisFailed;
        localWork.ProblemID = ITEMPR_UNABLETODELETEDISKFILE;
        localWork.WinError = ERROR_FILENAME_EXCED_RANGE; // "file name is too long"
        needCopyBack = TRUE;                             // vracime chybu
    }
}

void DoOpenFileForReading(CFTPDiskWork& localWork, BOOL& needCopyBack)
{
    char fileName[MAX_PATH];
    lstrcpyn(fileName, localWork.Path, MAX_PATH);
    DWORD winError = NO_ERROR;
    BOOL ok = FALSE;
    if (SalamanderGeneral->SalPathAppend(fileName, localWork.Name, MAX_PATH))
    {
        HANDLE in = HANDLES_Q(CreateFile(fileName, GENERIC_READ,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
        if (in != INVALID_HANDLE_VALUE)
        {
            CQuadWord size;
            size.LoDWord = GetFileSize(in, &size.HiDWord);
            if (size.LoDWord == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
            {
                winError = GetLastError();
                HANDLES(CloseHandle(in));
            }
            else
            {
                ok = TRUE;
                localWork.OpenedFile = in;
                localWork.FileSize = size;
            }
        }
        else
            winError = GetLastError();
    }
    else
        winError = ERROR_FILENAME_EXCED_RANGE; // "file name is too long"
    if (!ok)
    {
        localWork.State = sqisFailed;
        localWork.ProblemID = ITEMPR_UPLOADCANNOTOPENSRCFILE;
        localWork.WinError = winError;
    }
    needCopyBack = TRUE; // vracime chybu nebo handle+info o souboru
}

void DoReadFile(CFTPDiskWork& localWork, BOOL& needCopyBack, BOOL isASCIITrMode)
{
    localWork.ValidBytesInFlushDataBuffer = 0;
    localWork.EOLsInFlushDataBuffer = 0;
    if (localWork.WorkFile != NULL)
    {
        CQuadWord curSeek = localWork.WriteOrReadFromOffset;
        curSeek.LoDWord = SetFilePointer(localWork.WorkFile, curSeek.LoDWord, (LONG*)&curSeek.HiDWord, FILE_BEGIN);
        if (curSeek.LoDWord == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ||
            curSeek != localWork.WriteOrReadFromOffset)
        { // chyba: nelze nastavit seek v souboru
            localWork.State = sqisFailed;
            localWork.ProblemID = ITEMPR_SRCFILEREADERROR;
            localWork.WinError = GetLastError();
        }
        else // overime pozadovanou cast souboru proti obsahu bufferu
        {
            DWORD read;
            if (isASCIITrMode) // textovy rezim prenosu = CRLF pro vsechny EOLy, budeme prevadet
            {
                char buf[DATACON_UPLOADFLUSHBUFFERSIZE]; // buffer pro cteni z disku
                if (ReadFile(localWork.WorkFile, buf, DATACON_UPLOADFLUSHBUFFERSIZE, &read, NULL))
                {
                    char* textBuf = localWork.FlushDataBuffer;
                    char* textBufEndMinOne = textBuf + DATACON_UPLOADFLUSHBUFFERSIZE - 1;
                    char* s = buf;
                    char* end = s + read;
                    while (s < end && textBuf < textBufEndMinOne)
                    {
                        if (*s == '\n') // LF
                        {
                            *textBuf++ = '\r';
                            *textBuf++ = '\n';
                            localWork.EOLsInFlushDataBuffer++;
                        }
                        else
                        {
                            if (*s == '\r')
                            {
                                if (s + 1 < end)
                                {
                                    if (*(s + 1) == '\n') // CRLF
                                    {
                                        s++;
                                        *textBuf++ = '\r';
                                        *textBuf++ = '\n';
                                        localWork.EOLsInFlushDataBuffer++;
                                    }
                                    else // CR
                                    {
                                        *textBuf++ = '\r';
                                        *textBuf++ = '\n';
                                        localWork.EOLsInFlushDataBuffer++;
                                    }
                                }
                                else // CR + EOF
                                {
                                    *textBuf++ = '\r';
                                    *textBuf++ = '\n';
                                    localWork.EOLsInFlushDataBuffer++;
                                }
                            }
                            else
                                *textBuf++ = *s; // normalni znak, jen zkopirujeme
                        }
                        s++;
                    }
                    localWork.ValidBytesInFlushDataBuffer = (int)(textBuf - localWork.FlushDataBuffer);
                    localWork.WriteOrReadFromOffset += CQuadWord((DWORD)(s - buf), 0);
                }
                else // chyba cteni
                {
                    localWork.State = sqisFailed;
                    localWork.ProblemID = ITEMPR_SRCFILEREADERROR;
                    localWork.WinError = GetLastError();
                }
            }
            else
            {
                if (ReadFile(localWork.WorkFile, localWork.FlushDataBuffer, DATACON_UPLOADFLUSHBUFFERSIZE, &read, NULL))
                {
                    localWork.ValidBytesInFlushDataBuffer = read;
                    localWork.WriteOrReadFromOffset += CQuadWord(read, 0);
                }
                else // chyba cteni
                {
                    localWork.State = sqisFailed;
                    localWork.ProblemID = ITEMPR_SRCFILEREADERROR;
                    localWork.WinError = GetLastError();
                }
            }
        }
    }
    else // ohlasime chybu
    {
        TRACE_E("DoReadFile(): localWork.WorkFile may not be NULL!");
        localWork.State = sqisFailed;
        localWork.ProblemID = ITEMPR_LOWMEM;
    }
    needCopyBack = TRUE;
}

unsigned
CFTPDiskThread::Body()
{
    CALL_STACK_MESSAGE1("CFTPDiskThread::Body()");

    CFTPDiskWork localWork;
    localWork.NewTgtName = NULL;
    localWork.OpenedFile = NULL;
    localWork.DiskListing = NULL;
    char fullName[MAX_PATH];
    char nameBackup[MAX_PATH];
    char suffix[20];
#ifdef TRACE_ENABLE
    char errBuf[300];
#endif // TRACE_ENABLE
    while (1)
    {
        // zjistime jestli je nejaka prace nebo jestli se ma thread ukoncit
        HANDLES(EnterCriticalSection(&DiskCritSect));
        BOOL wait = !ShouldTerminate && Work.Count == 0 && FilesToClose.Count == 0;
        if (wait)
            ResetEvent(ContEvent);
        HANDLES(LeaveCriticalSection(&DiskCritSect));

        if (wait) // pockame pokud neni zadna prace ani se nema thread ukoncit
        {
            CALL_STACK_MESSAGE1("CFTPDiskThread::Body(): waiting...");
            WaitForSingleObject(ContEvent, INFINITE);
        }

        // vyzvedneme praci nebo zjistime pozadavek na ukonceni threadu
        HANDLES(EnterCriticalSection(&DiskCritSect));
        BOOL endThread = ShouldTerminate;
        CFTPFileToClose* fileToClose = NULL;
        CFTPDiskWork* work = NULL;
        if (FilesToClose.Count > 0) // nejvyssi prioritu ma zavirani souboru, pak je ukonceni threadu, a nakonec bezna prace
        {
            fileToClose = FilesToClose[0];
            FilesToClose.Detach(0);
            endThread = FALSE;
        }
        else
        {
            if (!endThread && Work.Count > 0)
            {
                work = Work[0];
                if (work != NULL)
                {
                    localWork.CopyFrom(work);
                    WorkIsInProgress = TRUE;
                }
            }
        }
        HANDLES(LeaveCriticalSection(&DiskCritSect));
        if (endThread)
            break; // ukoncime thread

        if (fileToClose != NULL) // provedeme zavreni souboru + pripadne smazani prazdneho souboru
        {
            CQuadWord size;
            BOOL delFile = FALSE;
            if (fileToClose->AlwaysDeleteFile)
                delFile = TRUE;
            else
            {
                if (fileToClose->DeleteIfEmpty)
                {
                    size.LoDWord = GetFileSize(fileToClose->File, &size.HiDWord);
                    delFile = (size == CQuadWord(0, 0)); // pri chybe GetFileSize nedojde k vymazu souboru
                }
            }
            if (!delFile && fileToClose->SetDateAndTime &&
                (fileToClose->Date.Day != 0 || fileToClose->Time.Hour != 24)) // jen pokud se bude nastavovat aspon neco (jinak nasledujici blok nema smysl)
            {
                SYSTEMTIME st;
                FILETIME ft, ft2;
                if ((fileToClose->Date.Day == 0 || fileToClose->Time.Hour == 24) && // datum nebo cas jsou "prazdne hodnoty" (musime si je opatrit ze souboru)
                    (!GetFileTime(fileToClose->File, NULL, NULL, &ft) ||
                     !FileTimeToLocalFileTime(&ft, &ft2) ||
                     !FileTimeToSystemTime(&ft2, &st)))
                {
                    GetLocalTime(&st); // nelze cist datum&cas ze souboru, vezmeme tedy aspon aktualni cas (nejak ty "prazdne hodnoty" musime naplnit)
                }
                if (fileToClose->Date.Day != 0) // pokud datum neni "prazdna hodnota"
                {
                    st.wYear = fileToClose->Date.Year;
                    st.wMonth = fileToClose->Date.Month;
                    st.wDayOfWeek = 0;
                    st.wDay = fileToClose->Date.Day;
                }
                if (fileToClose->Time.Hour != 24) // pokud cas neni "prazdna hodnota"
                {
                    st.wHour = fileToClose->Time.Hour;
                    st.wMinute = fileToClose->Time.Minute;
                    st.wSecond = fileToClose->Time.Second;
                    st.wMilliseconds = fileToClose->Time.Millisecond;
                }
                if (!SystemTimeToFileTime(&st, &ft2) ||
                    !LocalFileTimeToFileTime(&ft2, &ft))
                {
                    DWORD err = GetLastError();
                    TRACE_E("CFTPDiskThread::Body(): SystemTimeToFileTime() or LocalFileTimeToFileTime() failed: " << FTPGetErrorText(err, errBuf, 300));
                }
                else
                {
                    if (!SetFileTime(fileToClose->File, NULL, NULL, &ft))
                    {
                        DWORD err = GetLastError();
                        TRACE_E("CFTPDiskThread::Body(): SetFileTime() failed: " << FTPGetErrorText(err, errBuf, 300));
                    }
                }
            }
            if (!delFile && fileToClose->EndOfFile != CQuadWord(-1, -1))
            {
                size.LoDWord = GetFileSize(fileToClose->File, &size.HiDWord);
                if (size.LoDWord != INVALID_FILE_SIZE || GetLastError() == NO_ERROR)
                {
                    if (fileToClose->EndOfFile <= size)
                    {
                        CQuadWord curSeek = fileToClose->EndOfFile;
                        curSeek.LoDWord = SetFilePointer(fileToClose->File, curSeek.LoDWord, (LONG*)&curSeek.HiDWord, FILE_BEGIN);
                        if ((curSeek.LoDWord != INVALID_SET_FILE_POINTER || GetLastError() == NO_ERROR) &&
                            curSeek == fileToClose->EndOfFile)
                        {
                            if (SetEndOfFile(fileToClose->File) == 0)
                            {
                                DWORD err = GetLastError();
                                TRACE_E("CFTPDiskThread::Body(): SetEndOfFile failed: " << FTPGetErrorText(err, errBuf, 300));
                            }
                        }
                        else
                        {
                            DWORD err = GetLastError();
                            TRACE_E("CFTPDiskThread::Body(): SetFilePointer failed: " << FTPGetErrorText(err, errBuf, 300));
                        }
                    }
                    else
                        TRACE_E("CFTPDiskThread::Body(): fileToClose->EndOfFile > size!");
                }
                else
                {
                    DWORD err = GetLastError();
                    TRACE_E("CFTPDiskThread::Body(): GetFileSize failed: " << FTPGetErrorText(err, errBuf, 300));
                }
            }
            HANDLES(CloseHandle(fileToClose->File));
            if (delFile)
                DeleteFile(fileToClose->FileName);
            delete fileToClose;

            HANDLES(EnterCriticalSection(&DiskCritSect));
            DoneFileCloseIndex++; // z -1 (zadny se zatim nezavrel) jdeme na nulu, pak po jedne dale
            HANDLES(LeaveCriticalSection(&DiskCritSect));
            if (FileClosedEvent != NULL)
                PulseEvent(FileClosedEvent);
        }
        else
        {
            // provedeme pozadovanou praci
            BOOL needCopyBack = FALSE;
            BOOL workDone = FALSE;
            if (work != NULL) // POZOR: na objekt 'work' se nesmi pristupovat, jiz davno nemusi existovat (lze testovat jen hodnotu pointeru, ne obsah pameti kam ukazuje)
            {
                switch (localWork.Type)
                {
                case fdwtCreateDir:
                {
                    DoCreateDir(localWork, fullName, workDone, needCopyBack, nameBackup, suffix);
                    break;
                }

                case fdwtCreateFile:
                case fdwtRetryCreatedFile:
                case fdwtRetryResumedFile:
                {
                    DoCreateFile(localWork, fullName, workDone, needCopyBack, nameBackup, suffix);
                    break;
                }

                case fdwtCheckOrWriteFile:
                {
                    DoCheckOrWriteToFile(localWork, needCopyBack);
                    break;
                }

                case fdwtCreateAndWriteFile:
                {
                    DoCreateAndWriteFile(localWork, needCopyBack, workDone);
                    break;
                }

                case fdwtListDir:
                {
                    DoListDirectory(localWork, needCopyBack);
                    break;
                }

                case fdwtDeleteDir:
                {
                    DoDeleteDir(localWork, needCopyBack);
                    break;
                }

                case fdwtOpenFileForReading:
                {
                    DoOpenFileForReading(localWork, needCopyBack);
                    break;
                }

                case fdwtReadFile:
                case fdwtReadFileInASCII:
                {
                    DoReadFile(localWork, needCopyBack, localWork.Type == fdwtReadFileInASCII);
                    break;
                }

                case fdwtDeleteFile:
                {
                    DoDeleteFile(localWork, needCopyBack);
                    break;
                }

                default:
                    TRACE_E("CFTPDiskThread::Body(): unknown type of work: " << localWork.Type);
                    break;
                }
            }

            // zjistime jestli je treba provest cancel prace
            HANDLES(EnterCriticalSection(&DiskCritSect));
            BOOL doCancel = FALSE;
            if (work != NULL && Work.Count > 0 && work == Work[0]) // nedoslo ke cancelu provadene prace
            {
                if (needCopyBack)
                {
                    work->CopyFrom(&localWork); // prevezmeme vysledky prace
                    localWork.NewTgtName = NULL;
                    localWork.OpenedFile = NULL;
                    localWork.DiskListing = NULL;
                }
            }
            else
                doCancel = work != NULL;
            if (Work.Count > 0) // smazneme zpracovanou polozku nebo NULL (pokud byla zcancelovana)
            {
                Work.Detach(0);
                if (!Work.IsGood())
                    Work.ResetState();
            }
            WorkIsInProgress = FALSE;
            HANDLES(LeaveCriticalSection(&DiskCritSect));

            if (work != NULL)
            {
                if (localWork.NewTgtName != NULL) // pri cancelu dealokujeme alokovane nove jmeno adresare
                {
                    free(localWork.NewTgtName);
                    localWork.NewTgtName = NULL;
                }
                if (localWork.OpenedFile != NULL) // pri cancelu zavreme otevreny handle souboru
                {
                    HANDLES(CloseHandle(localWork.OpenedFile));
                    localWork.OpenedFile = NULL;
                }
                if (localWork.DiskListing != NULL) // pri cancelu dealokujeme alokovany listing
                {
                    delete localWork.DiskListing;
                    localWork.DiskListing = NULL;
                }
                if (!doCancel) // informujeme workera o vysledcich prace
                {
                    SocketsThread->PostSocketMessage(localWork.SocketMsg, localWork.SocketUID, localWork.MsgID, work);
                }
                else // doslo ke cancelu prace, provedeme uklid (jako by se prace skutecne neprovedla)
                {
                    switch (localWork.Type)
                    {
                    case fdwtCreateDir:
                    {
                        if (workDone)
                        {
                            if (!RemoveDirectory(fullName))
                                TRACE_E("CFTPDiskThread::Body(): cancelling disk operation: unable to remove directory: " << fullName);
                        }
                        break;
                    }

                    case fdwtCreateFile:
                    case fdwtRetryCreatedFile:
                    case fdwtRetryResumedFile:
                    {
                        if (workDone)
                        {
                            if (!DeleteFile(fullName)) // vytvoreny soubor nemuze mit read-only atribut, jinak by nesel otevrit pro zapis
                                TRACE_E("CFTPDiskThread::Body(): cancelling disk operation: unable to remove file: " << fullName);
                        }
                        break;
                    }

                    case fdwtCreateAndWriteFile:
                    {
                        if (workDone)
                        {
                            if (!DeleteFile(localWork.Name)) // vytvoreny soubor nemuze mit read-only atribut
                                TRACE_E("CFTPDiskThread::Body(): cancelling disk operation: unable to remove target file: " << localWork.Name);
                        }
                        // break; // tady schvalne neni break!
                    }
                    case fdwtCheckOrWriteFile:
                    {
                        if (localWork.FlushDataBuffer != NULL) // buffer nesel uvolnit z workera, protoze jsme s nim pracovali, takze ho uvolnime ted
                        {
                            free(localWork.FlushDataBuffer);
                            localWork.FlushDataBuffer = NULL;
                        }
                        break;
                    }

                    case fdwtListDir:
                        break; // pri cancelu listovani adresare neni co delat
                    case fdwtDeleteDir:
                        break; // pri cancelu mazani adresare neni co delat
                    case fdwtOpenFileForReading:
                        break; // pri cancelu otevirani souboru pro cteni neni co delat

                    case fdwtReadFile:
                    case fdwtReadFileInASCII:
                    {
                        if (localWork.FlushDataBuffer != NULL) // buffer nesel uvolnit z workera, protoze jsme s nim pracovali, takze ho uvolnime ted
                        {
                            free(localWork.FlushDataBuffer);
                            localWork.FlushDataBuffer = NULL;
                        }
                        break;
                    }

                    case fdwtDeleteFile:
                        break; // pri cancelu mazani souboru neni co delat

                    default:
                        TRACE_E("CFTPDiskThread::Body(), cancel: unknown type of work: " << localWork.Type);
                        break;
                    }
                }
            }
        }
    }
    return 0;
}
