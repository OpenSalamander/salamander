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

#include "precomp.h"
//#include <windows.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif // _MSC_VER
#include <limits.h>
#include <process.h>
//#include <commctrl.h>
#include <ostream>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "spl_com.h"
#include "spl_base.h"
#include "dbg.h"
#include "auxtools.h"

//
// ****************************************************************************
// CThreadQueue
//

CThreadQueue::CThreadQueue(const char* queueName)
{
    QueueName = queueName;
    Head = NULL;
    Continue = CreateEvent(NULL, FALSE, FALSE, NULL);
}

CThreadQueue::~CThreadQueue()
{
    ClearFinishedThreads(); // no need to use sections, only one thread should use this queue
    if (Continue != NULL)
        CloseHandle(Continue);
    if (Head != NULL)
        TRACE_E("Some thread is still in " << QueueName << " queue!"); // after terminating thread which is waiting for another thread (or is terminating it) from this queue, otherwise it should not happen...
}

void CThreadQueue::ClearFinishedThreads()
{
    CThreadQueueItem* last = NULL;
    CThreadQueueItem* act = Head;
    while (act != NULL)
    {
        DWORD ec;
        if (act->Locks == 0 && (!GetExitCodeThread(act->Thread, &ec) || ec != STILL_ACTIVE))
        { // this thread is not locked and it has finished, remove it from the list
            if (last != NULL)
                last->Next = act->Next;
            else
                Head = act->Next;
            CloseHandle(act->Thread);
            delete act;
            act = (last != NULL ? last->Next : Head);
        }
        else
        {
            last = act;
            act = act->Next;
        }
    }
}

BOOL CThreadQueue::Add(CThreadQueueItem* item)
{
    // first, remove finished threads
    ClearFinishedThreads();

    // add new thread
    if (item != NULL)
    {
        item->Next = Head;
        Head = item;
        return TRUE;
    }
    return FALSE;
}

BOOL CThreadQueue::FindAndLockItem(HANDLE thread)
{
    CS.Enter();

    CThreadQueueItem* act = Head; // we wil try to find opened handle of the thread
    while (act != NULL)
    {
        if (act->Thread == thread)
        {
            act->Locks++;
            break;
        }
        act = act->Next;
    }

    CS.Leave();

    return act != NULL; // NULL = not found
}

void CThreadQueue::UnlockItem(HANDLE thread, BOOL deleteIfUnlocked)
{
    CS.Enter();

    CThreadQueueItem* last = NULL;
    CThreadQueueItem* act = Head; // we will try to find opened handle of the thread
    while (act != NULL)
    {
        if (act->Thread == thread)
            break;
        last = act;
        act = act->Next;
    }
    if (act != NULL) // always true (was locked, could not be deleted)
    {
        if (act->Locks <= 0)
            TRACE_E("CThreadQueue::UnlockItem(): thread has not locks!");
        else
        {
            if (--(act->Locks) == 0 && deleteIfUnlocked) // thread is not locked anymore and we should delete it
            {
                if (last != NULL)
                    last->Next = act->Next;
                else
                    Head = act->Next;
                CloseHandle(act->Thread);
                delete act;
            }
        }
    }
    else
        TRACE_E("CThreadQueue::UnlockItem(): unable to find thread!"); // wasn't it locked, that it was deleted?

    CS.Leave();
}

BOOL CThreadQueue::WaitForExit(HANDLE thread, int milliseconds)
{
    CALL_STACK_MESSAGE2("CThreadQueue::WaitForExit(, %d)", milliseconds);
    BOOL ret = TRUE;
    if (thread != NULL)
    {
        if (FindAndLockItem(thread)) // thread handle found and locked - we can wait for it and then delete it
        {
            ret = WaitForSingleObject(thread, milliseconds) != WAIT_TIMEOUT;

            UnlockItem(thread, ret);
        }
    }
    else
        TRACE_E("CThreadQueue::WaitForExit(): Nothing to wait for (parameter 'thread'==NULL)!");
    return ret;
}

void CThreadQueue::KillThread(HANDLE thread, DWORD exitCode)
{
    CALL_STACK_MESSAGE2("CThreadQueue::KillThread(, %d)", exitCode);
    if (thread != NULL)
    {
        if (FindAndLockItem(thread)) // thread handle found and locked - we can terminate it and then delete it
        {
            TerminateThread(thread, exitCode);
            WaitForSingleObject(thread, INFINITE); // waiting for thread to finish for sure, sometimes it takes a long time
            UnlockItem(thread, TRUE);
        }
    }
    else
        TRACE_E("CThreadQueue::KillThread(): Nothing to kill (parameter 'thread'==NULL)!");
}

BOOL CThreadQueue::KillAll(BOOL force, int waitTime, int forceWaitTime, DWORD exitCode)
{
    CALL_STACK_MESSAGE5("CThreadQueue::KillAll(%d, %d, %d, %d)", force, waitTime, forceWaitTime, exitCode);
    DWORD ti = GetTickCount();
    DWORD w = force ? forceWaitTime : waitTime;

    CS.Enter();

    // kill all threads which don't want to finish themselves
    CThreadQueueItem* prevItem = NULL;
    CThreadQueueItem* item = Head;
    while (item != NULL)
    {
        BOOL leaveCS = FALSE;
        DWORD ec;
        if (GetExitCodeThread(item->Thread, &ec) && ec == STILL_ACTIVE)
        { // most probably, thread is still running
            DWORD t = GetTickCount() - ti;
            if (w == INFINITE || t < w) // we should still wait
            {
                // release queue for other threads (so they can wait for thread from this queue to finish and then finish themselves)
                CS.Leave();

                if (w == INFINITE || 50 < w - t)
                    Sleep(50);
                else
                {
                    Sleep(w - t);
                    ti -= w; // for next time, we will exclude the test for waiting
                }

                CS.Enter();
                item = Head;
                prevItem = NULL;
                continue; // we will start from the beginning (condition of the cycle will be tested)
            }
            if (force) // we will kill it
            {
                TRACE_E("Thread has not ended itself, we must terminate it (" << QueueName << " queue).");
                TerminateThread(item->Thread, exitCode);
                WaitForSingleObject(item->Thread, INFINITE); // we will wait until thread really finishes, sometimes it takes a long time
                // if some thread is waiting for the end of just killed thread, we will release queue for a while
                // otherwise, it will be locked in UnlockItem()
                leaveCS = item->Locks > 0;
            }
            else // without 'force', we will only report that something is still running
            {
                TRACE_I("KillAll(): At least one thread is still running in " << QueueName << " queue.");
                ClearFinishedThreads(); // to make this more clear during debugging
                CS.Leave();
                return FALSE;
            }
        }
        CThreadQueueItem* delItem = item;
        item = item->Next;
        if (delItem->Locks == 0) // handle can be closed, item can be deleted
        {
            if (Head == delItem)
                Head = item;
            else
                prevItem->Next = item;
            CloseHandle(delItem->Thread);
            delete delItem;
        }
        else
            prevItem = delItem; // handle must be kept, so we will keep item too

        if (leaveCS)
        {
            // release queue for other threads (so they can wait for thread from this queue to finish and then finish themselves)
            CS.Leave();

            Sleep(50); // a while for takeover of the queue and possible finishing of the thread (before we kill it as well as all other threads)

            CS.Enter();
            item = Head;
            prevItem = NULL;
            continue; // we will start from the beginning (condition of the cycle will be tested)
        }
    }

    CS.Leave();
    return TRUE;
}

struct CThreadBaseData
{
    unsigned(WINAPI* Body)(void*);
    void* Param;
    HANDLE Continue;
};

DWORD WINAPI
CThreadQueue::ThreadBase(void* param)
{
    CThreadBaseData* d = (CThreadBaseData*)param;

    // backup data to stack ('d' stops to be valid after 'Continue')
    unsigned(WINAPI * threadBody)(void*) = d->Body;
    void* threadParam = d->Param;

    SetEvent(d->Continue); // we let main thread to continue
    d = NULL;

    // executing our thread
    return SalamanderDebug->CallWithCallStack(threadBody, threadParam);
}

HANDLE
CThreadQueue::StartThread(unsigned(WINAPI* body)(void*), void* param, unsigned stack_size,
                          HANDLE* threadHandle, DWORD* threadID)
{
    CALL_STACK_MESSAGE2("CThreadQueue::StartThread(, , %d, ,)", stack_size);
    if (threadHandle != NULL)
        *threadHandle = NULL;
    if (threadID != NULL)
        *threadID = 0;
    if (Continue == NULL)
    {
        TRACE_E("Unable to start thread, because Continue event was not created.");
        return NULL;
    }

    CS.Enter();

    CThreadBaseData data;
    data.Body = body;
    data.Param = param;
    data.Continue = Continue;

    // execute thread (we don't use _beginthreadex(), because it has side-effect in VC2015
    // in form of another load of this module (plugin), which is released when thread
    // finishes normally, but when we use TerminateThread(), module stays loaded until
    // Salamander process ends, then destructors of global objects are executed and
    // it can lead to unexpected crashes, because all plugin interfaces are already
    // released (e.g. SalamanderDebug)
    DWORD tid;
    HANDLE thread = CreateThread(NULL, stack_size, CThreadQueue::ThreadBase, &data, CREATE_SUSPENDED, &tid);
    if (thread == NULL)
    {
        TRACE_E("Unable to start thread.");

        CS.Leave();

        return NULL;
    }
    else
    {
        // adding thread to the queue of this plugin
        if (!Add(new CThreadQueueItem(thread, tid)))
        {
            TRACE_E("Unable to add thread to the queue.");
            TerminateThread(thread, 666);          // it's suspended, so it will not be in any critical section, etc.
            WaitForSingleObject(thread, INFINITE); // wait for thread to finish, sometimes it takes a long time
            CloseHandle(thread);

            CS.Leave();

            return NULL;
        }

        // save before thread is not running (it assures that thread is not finished and its object is not deallocated)
        // Assign thread handle and ID before starting the thread to ensure safe initialization.
        // This avoids race conditions and ensures thread resources are correctly tracked and not prematurely deallocated.
        if (threadHandle != NULL)
            *threadHandle = thread;
        if (threadID != NULL)
            *threadID = tid;

        SalamanderDebug->TraceAttachThread(thread, tid);
        ResumeThread(thread);

        WaitForSingleObject(Continue, INFINITE); // wait for passing data to CThreadQueue::ThreadBase

        CS.Leave();

        return thread;
    }
}

//
// ****************************************************************************
// CThread
//

CThread::CThread(const char* name)
{
    if (name != NULL)
        lstrcpyn(Name, name, 101);
    else
        Name[0] = 0;
    Thread = NULL;
}

unsigned WINAPI
CThread::UniversalBody(void* param)
{
    CThread* thread = (CThread*)param;
    CALL_STACK_MESSAGE2("CThread::UniversalBody(thread name = \"%s\")", thread->Name);
    SalamanderDebug->SetThreadNameInVCAndTrace(thread->Name);

    unsigned ret = thread->Body(); // executing body of the thread

    delete thread; // destroying object of the thread
    return ret;
}

HANDLE
CThread::Create(CThreadQueue& queue, unsigned stack_size, DWORD* threadID)
{
    // CAUTION: after calling StartThread(), 'this' can be invalid (that's why we write to 'Thread' inside)
    return queue.StartThread(UniversalBody, this, stack_size, &Thread, threadID);
}
