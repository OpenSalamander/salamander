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
    DWORD ThreadID; // for debugging purposes only (finding thread in debugger's thread list)
    int Locks;      // number of locks, if > 0 we must not close 'Thread'
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
    const char* QueueName; // name of queue (for debugging purposes only)
    CThreadQueueItem* Head;
    HANDLE Continue; // we have to wait for data to be passed to the thread

    struct CCS // access from multiple threads -> synchronization needed
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

    // executes fuction 'body' with parameter 'param' in a new thread with stack size
    // 'stack_size' (0 = default); returns handle of the thread or NULL on error;
    // the result is also written before the thread is started (resume) to 'threadHandle'
    // (if not NULL); the returned handle should be used only for testing for NULL and
    // for calling methods CThreadQueue: WaitForExit() and KillThread(); closing the
    // handle of the thread is done by the queue object
    // CAUTION: -the thread can be started with delay after returning from StartThread()
    //          (if 'param' is a pointer to a structure stored on the stack, it is
    //          necessary to synchronize the passing of data from 'param' - the main
    //          thread must wait for the new thread to take over the data)
    //         -the returned handle of the thread can be already closed if the thread
    //          finishes before returning from StartThread() and StartThread() or
    //          KillAll() is called from another thread
    // can be called from any thread
    HANDLE StartThread(unsigned(WINAPI* body)(void*), void* param, unsigned stack_size = 0,
                       HANDLE* threadHandle = NULL, DWORD* threadID = NULL);

    // waits for the thread from this queue to finish; 'thread' is the handle of the
    // thread which can be already closed (this object closes it when calling StartThread
    // and KillAll); if the thread finishes, it is removed from the queue and its handle
    // is closed
    BOOL WaitForExit(HANDLE thread, int milliseconds = INFINITE);

    // kills the thread from this queue (via TerminateThread()); 'thread' is the handle
    // of the thread which can be already closed (this object closes it when calling
    // StartThread and KillAll); if the thread is found, it is killed, removed from the
    // queue and its handle is closed (the thread object is not deallocated because its
    // state is unknown, possibly inconsistent)
    void KillThread(HANDLE thread, DWORD exitCode = 666);

    // verifies that all threads have finished; if 'force' is TRUE and some thread is
    // still running, it waits 'forceWaitTime' (in ms) for all threads to finish, then
    // kills the running threads (their objects are not deallocated because their state
    // is unknown, possibly inconsistent); returns TRUE if all threads have finished,
    // always returns TRUE if 'force' is TRUE; if 'force' is FALSE and some thread is
    // still running, it waits 'waitTime' (in ms) for all threads to finish, if some
    // thread is still running after that, it returns FALSE; time INFINITE = wait forever
    // can be called from any thread
    BOOL KillAll(BOOL force, int waitTime = 1000, int forceWaitTime = 200, DWORD exitCode = 666);

protected:                                                 // internal unsynchronized methods
    BOOL Add(CThreadQueueItem* item);                      // adds item to the queue, returns success
    BOOL FindAndLockItem(HANDLE thread);                   // adds item for 'thread' to the queue and locks it
    void UnlockItem(HANDLE thread, BOOL deleteIfUnlocked); // unlocks item for 'thread' in the queue, possibly deletes it
    void ClearFinishedThreads();                           // removes finished threads from the queue
    static DWORD WINAPI ThreadBase(void* param);           // universal thread body
};

//
// ****************************************************************************
// CThread
//
// CAUTION: must be allocated (it is not possible to have CThread only on stack);
//          it is deallocated automatically only if the thread is successfully
//          created by Create()

class CThread
{
public:
    // thread handle (NULL = thread is not running yet/any more), CAUTION: after the thread
    // is finished, it is automatically closed (it is invalid), moreover this object is
    // already deallocated
    HANDLE Thread;

protected:
    char Name[101]; // a buffer for the thread name (used in TRACE and CALL-STACK to identify the thread)
                    // CAUTION: if the thread data contains references to the stack or other temporary
                    //          objects, it is necessary to ensure that these references are used only
                    //          for the duration of their validity

public:
    CThread(const char* name = NULL);
    virtual ~CThread() {} // so that the destructors of the descendants are called correctly

    // creating (start) of a thread in the queue 'queue'; 'stack_size' is the size of the stack
    // of the new thread in bytes (0 = default); returns handle of the new thread or NULL on error;
    // closing the handle is done by the 'queue' object; if the thread is successfully created,
    // this object is deallocated when the thread is finished, if the thread creation fails,
    // the object is deallocated by the caller
    // CAUTION: without adding synchronization, the thread can finish even before returning from
    //          Create() -> the pointer "this" must therefore be considered invalid after a
    //          successful call to Create(), the same applies to the returned thread handle
    //          (use only for NULL tests and for calling methods CThreadQueue: WaitForExit()
    //          and KillThread())
    // can be called from any thread
    HANDLE Create(CThreadQueue& queue, unsigned stack_size = 0, DWORD* threadID = NULL);

    // returns 'Thread' (see above)
    HANDLE GetHandle() { return Thread; }

    // returns thread name
    const char* GetName() { return Name; }

    // this method contains the body of the thread
    virtual unsigned Body() = 0;

protected:
    static unsigned WINAPI UniversalBody(void* param); // helper method to execude thread
};
