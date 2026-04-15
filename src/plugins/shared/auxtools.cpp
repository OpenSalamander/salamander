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
    ClearFinishedThreads(); // no critical section needed; only one thread should be using this now
    if (Continue != NULL)
        CloseHandle(Continue);
    if (Head != NULL)
        TRACE_E("Some thread is still in " << QueueName << " queue!"); // after terminating a thread that is waiting for, or is just terminating, another thread from the queue; otherwise this should not happen...
}

void CThreadQueue::ClearFinishedThreads()
{
    CThreadQueueItem* last = NULL;
    CThreadQueueItem* act = Head;
    while (act != NULL)
    {
        DWORD ec;
        if (act->Locks == 0 && (!GetExitCodeThread(act->Thread, &ec) || ec != STILL_ACTIVE))
        { // this thread is not locked and has already finished; remove it from the list
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
    // first remove threads that have already finished
    ClearFinishedThreads();

    // add the new thread
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

    CThreadQueueItem* act = Head; // try to find an open thread handle
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
    CThreadQueueItem* act = Head; // try to find an open thread handle
    while (act != NULL)
    {
        if (act->Thread == thread)
            break;
        last = act;
        act = act->Next;
    }
    if (act != NULL) // always true (it was locked, so it could not be deleted)
    {
        if (act->Locks <= 0)
            TRACE_E("CThreadQueue::UnlockItem(): thread has not locks!");
        else
        {
            if (--(act->Locks) == 0 && deleteIfUnlocked) // thread is no longer locked and should be deleted
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
        TRACE_E("CThreadQueue::UnlockItem(): unable to find thread!"); // wasn't it already unlocked and therefore deleted?

    CS.Leave();
}

BOOL CThreadQueue::WaitForExit(HANDLE thread, int milliseconds)
{
    CALL_STACK_MESSAGE2("CThreadQueue::WaitForExit(, %d)", milliseconds);
    BOOL ret = TRUE;
    if (thread != NULL)
    {
        if (FindAndLockItem(thread)) // thread handle found and locked; we can wait for it, then remove it
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
        if (FindAndLockItem(thread)) // thread handle found and locked; we can terminate it, then remove it
        {
            TerminateThread(thread, exitCode);
            WaitForSingleObject(thread, INFINITE); // wait until the thread actually exits; sometimes this takes quite a while

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

    // terminate all threads that will not exit on their own
    CThreadQueueItem* prevItem = NULL;
    CThreadQueueItem* item = Head;
    while (item != NULL)
    {
        BOOL leaveCS = FALSE;
        DWORD ec;
        if (GetExitCodeThread(item->Thread, &ec) && ec == STILL_ACTIVE)
        { // thread is probably still running
            DWORD t = GetTickCount() - ti;
            if (w == INFINITE || t < w) // still need to wait
            {
                // release the queue for other threads (for example so they can wait for a queued thread to finish and then exit themselves)
                CS.Leave();

                if (w == INFINITE || 50 < w - t)
                    Sleep(50);
                else
                {
                    Sleep(w - t);
                    ti -= w; // skip the wait test next time
                }

                CS.Enter();
                item = Head;
                prevItem = NULL;
                continue; // start over from the beginning (the loop condition will be rechecked)
            }
            if (force) // terminate it
            {
                TRACE_E("Thread has not ended itself, we must terminate it (" << QueueName << " queue).");
                TerminateThread(item->Thread, exitCode);
                WaitForSingleObject(item->Thread, INFINITE); // wait until the thread actually exits; sometimes this takes quite a while
                // pokud nejaky thread ceka na ukonceni prave zabiteho threadu, pustime pro nej na chvilku
                // frontu, jinak zustane zasekly v UnlockItem()
                leaveCS = item->Locks > 0;
            }
            else // without force, just report that something is still running
            {
                TRACE_I("KillAll(): At least one thread is still running in " << QueueName << " queue.");
                ClearFinishedThreads(); // just for clarity while debugging
                CS.Leave();
                return FALSE;
            }
        }
        CThreadQueueItem* delItem = item;
        item = item->Next;
        if (delItem->Locks == 0) // the handle can be closed; delete the item
        {
            if (Head == delItem)
                Head = item;
            else
                prevItem->Next = item;
            CloseHandle(delItem->Thread);
            delete delItem;
        }
        else
            prevItem = delItem; // we have to leave the handle alone, so the item has to stay too

        if (leaveCS)
        {
            // release the queue for other threads (for example so they can wait for a queued thread to finish and then exit themselves)
            CS.Leave();

            Sleep(50); // give it a moment to take over the queue and possibly finish before we kill it like the others

            CS.Enter();
            item = Head;
            prevItem = NULL;
            continue; // start over from the beginning (the loop condition will be rechecked)
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

    // copy the data to the stack ('d' becomes invalid after 'Continue')
    unsigned(WINAPI * threadBody)(void*) = d->Body;
    void* threadParam = d->Param;

    SetEvent(d->Continue); // let the main thread continue
    d = NULL;

    // run the thread body
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

    // spustime thread, nepouzivame _beginthreadex(), protoze ten ma od VC2015 side-effect v podobe
    // dalsiho loadu tohoto modulu (pluginu), ktery sice pri beznem ukonceni zase uvolni,
    // ale kdyz pouzijeme TerminateThread(), zustane modul naloadeny az do ukonceni procesu
    // Salamandera, pak se teprve spusti destruktory globalnich objektu a to muze vest
    // k necekanym padum, protoze uz jsou vsechny pluginove rozhrani uvolnene (napr.
    // SalamanderDebug)
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
        // add the thread to this plugin's thread queue
        if (!Add(new CThreadQueueItem(thread, tid)))
        {
            TRACE_E("Unable to add thread to the queue.");
            TerminateThread(thread, 666);          // it is suspended, so it cannot be in any critical section, etc.
            WaitForSingleObject(thread, INFINITE); // wait until the thread actually exits; sometimes this takes quite a while
            CloseHandle(thread);

            CS.Leave();

            return NULL;
        }

        // store the handle before the thread starts running (guarantees it has not finished and its object has not been deallocated)
        if (threadHandle != NULL)
            *threadHandle = thread;
        if (threadID != NULL)
            *threadID = tid;

        SalamanderDebug->TraceAttachThread(thread, tid);
        ResumeThread(thread);

        WaitForSingleObject(Continue, INFINITE); // wait until the data has been handed off to CThreadQueue::ThreadBase

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

    unsigned ret = thread->Body(); // run the thread body

    delete thread; // destroy the thread object
    return ret;
}

HANDLE
CThread::Create(CThreadQueue& queue, unsigned stack_size, DWORD* threadID)
{
    // POZOR: po volani StartThread() muze byt 'this' neplatny (proto je zapis do 'Thread' uvnitr)
    return queue.StartThread(UniversalBody, this, stack_size, &Thread, threadID);
}
