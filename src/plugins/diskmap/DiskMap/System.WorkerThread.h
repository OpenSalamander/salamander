// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <process.h>
#include "System.Lock.h"
#include "System.RWLock.h"

class CWorkerThread;

typedef DWORD_PTR(WINAPI* PWORKERTHREAD_START_ROUTINE)(
    CWorkerThread* thread,
    LPVOID lpThreadParameter);

class CMyThreadQueue : public CThreadQueue
{
public:
    CMyThreadQueue(const char* queueName /* napr. "DemoPlug Viewers" */) : CThreadQueue(queueName) {}

    void Add(HANDLE hThread, DWORD tid) // prida polozku do fronty, vraci uspech
    {
        CS.Enter();
        CThreadQueue::Add(new CThreadQueueItem(hThread, tid)); // nemuze selhat
        CS.Leave();
    }
};

extern CMyThreadQueue ThreadQueue;

/************************************************************************/
/*  ?:  START                                                           */
/*                                                                      */
/*  0:  INIT                                                            */
/*                                                                      */
/*  1:  RUN / SUSPEND          -- 4:  ABORTING                          */
/*                                                                      */
/*  3:  FINISHED                  6:  ABORTED                           */
/*                                                                      */
/*                                                                      */
/*                                                                      */
/************************************************************************/

class CWorkerThread
{
protected:
    DWORD _threadId;
    HANDLE _hThread;
    PWORKERTHREAD_START_ROUTINE _proc;
    LPVOID _lpParam;
    HWND _owner;
    UINT _msg;
    LPVOID _msgLParam;
    volatile BOOL _doDelete;

    volatile BOOL _finished;
    volatile LONG _abort;

    static DWORD WINAPI s_ThreadProc(LPVOID lpParam)
    {
        DWORD_PTR result = NULL;
        CWorkerThread* self = (CWorkerThread*)lpParam;

        if (!self->Aborting())
            result = self->_proc(self, self->_lpParam);

        if (self->_owner)
            PostMessage(self->_owner, self->_msg, result, (LPARAM)self->_msgLParam);

        self->_innerlock->Enter();
        self->_finished = TRUE;
        if (self->_doDelete)
        {
            self->_innerlock->Leave();
            delete self;
        }
        else
        {
            self->_innerlock->Leave();
        }
        return 0;
    }

    CLock* _innerlock;
    CLock* _lock;
    CRWLock* _rwlock;

public:
    CWorkerThread(CWorkerThread** setToThis, PWORKERTHREAD_START_ROUTINE proc, LPVOID lpParam, HWND owner,
                  UINT msg, LPVOID msgLParam, BOOL selfDelete = TRUE, BOOL suspended = FALSE)
    {
        // Petr: nelze cekat na navrat z konstruktoru, externi ukazatel uz se pouziva v threadu, ktery
        // bezi jiz behem konstruktoru
        if (setToThis != NULL)
            *setToThis = this;

        this->_threadId = 0;
        this->_hThread = NULL;
        this->_proc = proc;
        this->_lpParam = lpParam;
        this->_owner = owner;
        this->_msg = msg;
        this->_msgLParam = msgLParam;

        this->_abort = FALSE;
        this->_finished = FALSE;
        this->_innerlock = new CLock();
        this->_lock = new CLock();
        this->_rwlock = new CRWLock();

        this->_doDelete = FALSE; // thread muze skoncit jeste pred navratem z CreateThread(), delete odlozime

        this->_hThread = CreateThread(
            NULL,                               // default security attributes
            0,                                  // use default stack size
            CWorkerThread::s_ThreadProc,        // thread function
            this,                               // argument to thread function
            (suspended) ? CREATE_SUSPENDED : 0, // use default creation flags
            &this->_threadId);                  // returns the thread identifier

        if (this->_hThread != NULL)
            ThreadQueue.Add(this->_hThread, this->_threadId);
        else
            this->_finished = TRUE; // thread se nepovedlo nastartovat = budeme se tvarit jako jiz ukonceny
        if (selfDelete)
            this->SetSelfDelete(selfDelete); // ted uz je delete objektu mozny
    }
    ~CWorkerThread()
    {
        this->_innerlock->Enter();
        this->_doDelete = FALSE;
        this->_innerlock->Leave();
        this->Abort(FALSE);

        while (!this->_finished)
            Sleep(0);
        this->_hThread = NULL; // zavirani handle threadu nechame na ThreadQueue
        delete this->_innerlock;
        delete this->_lock;
        delete this->_rwlock;
        //Beep(500, 100);
    }

    CLock* GetLock() { return this->_lock; }
    CRWLock* GetRWLock() { return this->_rwlock; }

    void SetSelfDelete(BOOL selfDelete) //po zavolani s TRUE jiz neni refernce na objekt bezpecna a mohla byt kdykoliv zrusena
    {
        this->_innerlock->Enter();
        if (this->_doDelete != selfDelete)
        {
            if (!this->_finished)
            {
                this->_doDelete = selfDelete;
            }
            else //pokud vlakno jiz skoncilo
            {
                if (selfDelete) //chce mne zrusit, tak zrusime...
                {
                    this->_innerlock->Leave(); //nejdriv uvolnit zamek
                    delete this;               //zrusit se
                    return;                    //a rychle pryc
                }
            }
        }
        this->_innerlock->Leave();
    }
    BOOL IsSelfDelete() const
    {
        return this->_doDelete;
    }

    inline BOOL Aborting() //test z worker threadu
    {
        return this->_abort;
    }
    BOOL Abort(BOOL wait = FALSE, DWORD maxwait = INFINITE) //z ridiciho vlakna pro zastaveni worker threadu
    {
        InterlockedExchange(&this->_abort, TRUE);
        if (wait)
        {
            return ThreadQueue.WaitForExit(this->_hThread, maxwait);
        }
        return TRUE;
    }
    BOOL IsActive()
    {
        if (!this->_hThread)
            return FALSE;
        return !ThreadQueue.WaitForExit(this->_hThread, 0);
    }
};
