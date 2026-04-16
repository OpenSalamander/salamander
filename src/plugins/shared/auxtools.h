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

    // starts the function 'body' with parameter 'param' in a newly created thread with a stack
    // of size 'stack_size' (0 = default); returns the thread handle or NULL on error,
    // and also stores the result in 'threadHandle' before the thread is resumed
    // (if it is not NULL); use the returned thread handle only for NULL checks and for calling
    // the CThreadQueue methods WaitForExit() and KillThread(); this queue object
    // closes the thread handle
    // WARNING: -the thread may start with a delay, only after StartThread() returns
    //           (if 'param' is a pointer to a structure stored on the stack, the data handoff
    //            from 'param' must be synchronized - the main thread must wait
    //            until the new thread takes over the data)
    //          -the returned thread handle may already be closed if the thread finishes before
    //           StartThread() returns and StartThread() or KillAll() is called from another
    //           thread
    // can be called from any thread
    HANDLE StartThread(unsigned(WINAPI* body)(void*), void* param, unsigned stack_size = 0,
                       HANDLE* threadHandle = NULL, DWORD* threadID = NULL);

    // waits for a thread from this queue to exit; 'thread' is the thread handle, which may already
    // be closed (this object closes it when StartThread or KillAll is called); if it detects that the
    // thread has exited, removes it from the queue and closes its handle
    BOOL WaitForExit(HANDLE thread, int milliseconds = INFINITE);

    // kills a thread from this queue (via TerminateThread()); 'thread' is the thread handle, which
    // may already be closed (this object closes it when StartThread or KillAll is called); if it
    // finds the thread, kills it, removes it from the queue, and closes its handle (the thread object
    // is not deallocated because its state is unknown and may be inconsistent)
    void KillThread(HANDLE thread, DWORD exitCode = 666);

    // verifies that all threads have finished; if 'force' is TRUE and some thread is still running,
    // waits 'forceWaitTime' (ms) for all threads to finish, then kills any still-running threads
    // (their objects are not deallocated because their state is unknown and may be inconsistent);
    // returns TRUE if all threads have finished; when 'force' is TRUE, it always returns TRUE;
    // if 'force' is FALSE and some thread is still running, waits 'waitTime' (ms) for all
    // threads to finish; if anything is still running after that, returns FALSE; INFINITE = unlimited wait
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
    // thread handle (NULL = thread is not running / has not run yet), WARNING: after the thread exits it
    // closes itself (becomes invalid), and this object is already deallocated
    HANDLE Thread;

protected:
    char Name[101]; // buffer for the thread name (used in TRACE and CALL-STACK to identify the thread)
                    // WARNING: if the thread data contains references to the stack or other temporary objects,
                    //        those references must be used only while they remain valid

public:
    CThread(const char* name = NULL);
    virtual ~CThread() {} // so destructors of derived classes are called correctly

    // creates (starts) a thread in the thread queue 'queue'; 'stack_size' is the stack size of the
    // new thread in bytes (0 = default); returns a handle to the new thread or NULL on error;
    // the 'queue' object closes the handle; if the thread is created successfully, this object is
    // deallocated when the thread exits; if thread startup fails, the caller deallocates the object
    // WARNING: without added synchronization, the thread may finish before Create() returns ->
    //          therefore, after a successful call to Create(), the "this" pointer must be considered invalid,
    //          and the same applies to the returned thread handle (use it only for NULL checks and for calling
    //          CThreadQueue methods WaitForExit() and KillThread())
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
