// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

//
// ****************************************************************************
// CThreadQueue
//

struct CThreadQueueItem
{
    HANDLE Thread;
    DWORD ThreadID; // debugging only (to find the thread in the debugger thread list)
    int Locks;      // pocet zamku, je-li > 0 nesmime zavrit 'Thread'
    CThreadQueueItem* Next;

    CThreadQueueItem(HANDLE thread, DWORD tid)
    {
        Thread = thread;
        ThreadID = tid;
        Next = NULL;
        Locks = 0;
    }
};

class CThreadQueue
{
protected:
    const char* QueueName; // queue name (debugging only)
    CThreadQueueItem* Head;
    HANDLE Continue; // must wait until the data is handed off to the started thread

    struct CCS // access from multiple threads -> synchronization required
    {
        CRITICAL_SECTION cs;

        CCS() { InitializeCriticalSection(&cs); }
        ~CCS() { DeleteCriticalSection(&cs); }

        void Enter() { EnterCriticalSection(&cs); }
        void Leave() { LeaveCriticalSection(&cs); }
    } CS;

public:
    CThreadQueue(const char* queueName /* e.g. "DemoPlug Viewers" */);
    ~CThreadQueue();

    // starts function 'body' with parameter 'param' in a newly created thread with a stack
    // o velikosti 'stack_size' (0 = default); vraci handle threadu nebo NULL pri chybe,
    // also writes the result to 'threadHandle' before the thread is resumed
    // (if not NULL); use the returned thread handle only for NULL checks and for calling
    // CThreadQueue methods WaitForExit() and KillThread(); the thread handle is closed by
    // this queue object
    // POZOR: -thread se muze spustit se zpozdenim az po navratu ze StartThread()
    //         (if 'param' points to a structure stored on the stack, it is necessary to
    //          synchronize the handoff of data from 'param' - the main thread must wait
    //          until the new thread takes ownership of the data)
    //        -the returned thread handle may already be closed if the thread finishes before
    //         StartThread() returns and another thread calls StartThread() or
    //         KillAll()
    // can be called from any thread
    HANDLE StartThread(unsigned(WINAPI* body)(void*), void* param, unsigned stack_size = 0,
                       HANDLE* threadHandle = NULL, DWORD* threadID = NULL);

    // ceka na ukonceni threadu z teto fronty; 'thread' je handle threadu, ktery jiz muze
    // byt i zavreny (zavira tento objekt pri volani StartThread a KillAll); pokud se
    // docka ukonceni threadu, vyradi thread z fronty a zavre jeho handle
    BOOL WaitForExit(HANDLE thread, int milliseconds = INFINITE);

    // zabije thread z teto fronty (pres TerminateThread()); 'thread' je handle threadu,
    // ktery jiz muze byt i zavreny (zavira tento objekt pri volani StartThread a KillAll);
    // pokud thread najde, zabije ho, vyradi z fronty a zavre jeho handle (objekt threadu
    // se nedealokuje, protoze jeho stav je neznamy, mozna nekonzistentni)
    void KillThread(HANDLE thread, DWORD exitCode = 666);

    // overi, ze vsechny thready skoncily; je-li 'force' TRUE a nejaky thread jeste bezi,
    // waits 'forceWaitTime' (ms) for all threads to finish, then kills any threads still running
    // (their objects are not deallocated because their state is unknown and may be inconsistent);
    // vraci TRUE, jsou-li vsechny thready ukoncene, pri 'force' TRUE vzdy vraci TRUE;
    // if 'force' is FALSE and some thread is still running, waits 'waitTime' (ms) for all
    // vsech threadu, pokud pak jeste stale neco bezi, vraci FALSE; cas INFINITE = neomezene
    // waiting
    // can be called from any thread
    BOOL KillAll(BOOL force, int waitTime = 1000, int forceWaitTime = 200, DWORD exitCode = 666);

protected:                                                 // internal unsynchronized methods
    BOOL Add(CThreadQueueItem* item);                      // adds an item to the queue; returns success
    BOOL FindAndLockItem(HANDLE thread);                   // finds the queue item for 'thread' and locks it
    void UnlockItem(HANDLE thread, BOOL deleteIfUnlocked); // unlocks the queue item for 'thread', or deletes it
    void ClearFinishedThreads();                           // removes threads that have already finished from the queue
    static DWORD WINAPI ThreadBase(void* param);           // universal thread body
};

//
// ****************************************************************************
// CThread
//
// POZOR: musi se alokovat (neni mozne mit CThread jen na stacku); dealokuje se sam
//        jen v pripade uspesneho vytvoreni threadu metodou Create()

class CThread
{
public:
    // handle threadu (NULL = thread jeste nebezi/nebezel), POZOR: po ukonceni threadu se
    // sam zavira (je neplatny), navic tento objekt uz je dealokovany
    HANDLE Thread;

protected:
    char Name[101]; // buffer for the thread name (used in TRACE and CALL-STACK to identify the thread)
                    // POZOR: pokud budou data threadu obsahovat odkazy na stack nebo jine docasne objekty,
                    //        je potreba zajistit, aby se s temito odkazy pracovalo jen po dobu jejich platnosti

public:
    CThread(const char* name = NULL);
    virtual ~CThread() {} // so destructors of derived classes are called correctly

    // creates (starts) a thread in thread queue 'queue'; 'stack_size' is the stack size of the
    // noveho threadu v bytech (0 = default); vraci handle noveho threadu nebo NULL pri chybe;
    // the 'queue' object closes the handle; if the thread is created successfully, this object is
    // deallocated when the thread exits; if thread startup fails, the caller deallocates the object
    // POZOR: bez pridani synchronizace muze thread dobehnout jeste pred navratem z Create() ->
    //        therefore, after a successful call to Create(), the "this" pointer must be considered invalid,
    //        the same applies to the returned thread handle (use it only for NULL checks and for calling
    //        CThreadQueue methods WaitForExit() and KillThread())
    // can be called from any thread
    HANDLE Create(CThreadQueue& queue, unsigned stack_size = 0, DWORD* threadID = NULL);

    // returns 'Thread'; see above
    HANDLE GetHandle() { return Thread; }

    // returns the thread name
    const char* GetName() { return Name; }

    // this method implements the thread body
    virtual unsigned Body() = 0;

protected:
    static unsigned WINAPI UniversalBody(void* param); // helper method for starting threads
};
