// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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
    ClearFinishedThreads(); // neni treba sekce, uz by mel pouzivat jen jeden thread
    if (Continue != NULL)
        CloseHandle(Continue);
    if (Head != NULL)
        TRACE_E("Some thread is still in " << QueueName << " queue!"); // po terminovani threadu, ktery ceka na (nebo zrovna terminuje) jiny thread z fronty, jinak by nemelo nastat...
}

void CThreadQueue::ClearFinishedThreads()
{
    CThreadQueueItem* last = NULL;
    CThreadQueueItem* act = Head;
    while (act != NULL)
    {
        DWORD ec;
        if (act->Locks == 0 && (!GetExitCodeThread(act->Thread, &ec) || ec != STILL_ACTIVE))
        { // tento thread neni zamceny + uz skoncil, vyhodime ho ze seznamu
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
    // nejprve vyhodime thready, ktere se jiz ukoncily
    ClearFinishedThreads();

    // pridame novy thread
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

    CThreadQueueItem* act = Head; // zkusime najit otevreny handle threadu
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

    return act != NULL; // NULL = nenalezeno
}

void CThreadQueue::UnlockItem(HANDLE thread, BOOL deleteIfUnlocked)
{
    CS.Enter();

    CThreadQueueItem* last = NULL;
    CThreadQueueItem* act = Head; // zkusime najit otevreny handle threadu
    while (act != NULL)
    {
        if (act->Thread == thread)
            break;
        last = act;
        act = act->Next;
    }
    if (act != NULL) // always true (bylo zamknute, neslo smazat)
    {
        if (act->Locks <= 0)
            TRACE_E("CThreadQueue::UnlockItem(): thread has not locks!");
        else
        {
            if (--(act->Locks) == 0 && deleteIfUnlocked) // thread uz neni zamceny a mame ho smazat
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
        TRACE_E("CThreadQueue::UnlockItem(): unable to find thread!"); // to nebyl zamknuty, ze je smazany?

    CS.Leave();
}

BOOL CThreadQueue::WaitForExit(HANDLE thread, int milliseconds)
{
    CALL_STACK_MESSAGE2("CThreadQueue::WaitForExit(, %d)", milliseconds);
    BOOL ret = TRUE;
    if (thread != NULL)
    {
        if (FindAndLockItem(thread)) // handle threadu nalezen a uzamcen - muzeme na nej cekat, pak ho zrusime
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
        if (FindAndLockItem(thread)) // handle threadu nalezen a uzamcen - muzeme ho terminovat, pak ho zrusime
        {
            TerminateThread(thread, exitCode);
            WaitForSingleObject(thread, INFINITE); // pockame az thread skutecne skonci, nekdy mu to dost trva

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

    // vykillujeme vsechny thready, ktere nehodlaji koncit sami
    CThreadQueueItem* prevItem = NULL;
    CThreadQueueItem* item = Head;
    while (item != NULL)
    {
        BOOL leaveCS = FALSE;
        DWORD ec;
        if (GetExitCodeThread(item->Thread, &ec) && ec == STILL_ACTIVE)
        { // thread jeste nejspis bezi
            DWORD t = GetTickCount() - ti;
            if (w == INFINITE || t < w) // mame jeste cekat
            {
                // uvolnime frontu pro dalsi thready (aby se napr. dockaly ukonceni threadu z fronty a pak se sami ukoncily)
                CS.Leave();

                if (w == INFINITE || 50 < w - t)
                    Sleep(50);
                else
                {
                    Sleep(w - t);
                    ti -= w; // pro priste vyradime test na cekani
                }

                CS.Enter();
                item = Head;
                prevItem = NULL;
                continue; // zacneme pekne od zacatku (podminka cyklu se otestuje)
            }
            if (force) // zabijeme ho
            {
                TRACE_E("Thread has not ended itself, we must terminate it (" << QueueName << " queue).");
                TerminateThread(item->Thread, exitCode);
                WaitForSingleObject(item->Thread, INFINITE); // pockame az thread skutecne skonci, nekdy mu to dost trva
                // pokud nejaky thread ceka na ukonceni prave zabiteho threadu, pustime pro nej na chvilku
                // frontu, jinak zustane zasekly v UnlockItem()
                leaveCS = item->Locks > 0;
            }
            else // bez 'force' jen ohlasime, ze jeste neco bezi
            {
                TRACE_I("KillAll(): At least one thread is still running in " << QueueName << " queue.");
                ClearFinishedThreads(); // jen tak pro prehlednost pri debugovani
                CS.Leave();
                return FALSE;
            }
        }
        CThreadQueueItem* delItem = item;
        item = item->Next;
        if (delItem->Locks == 0) // handle je mozne zavrit, polozku smazat
        {
            if (Head == delItem)
                Head = item;
            else
                prevItem->Next = item;
            CloseHandle(delItem->Thread);
            delete delItem;
        }
        else
            prevItem = delItem; // handle musime nechat byt, takze i polozku

        if (leaveCS)
        {
            // uvolnime frontu pro dalsi thready (aby se napr. dockaly ukonceni threadu z fronty a pak se sami ukoncily)
            CS.Leave();

            Sleep(50); // chvilka na prevzeti fronty a prip. dobeh threadu (nez ho pujdeme zabit jako vsechny ostatni)

            CS.Enter();
            item = Head;
            prevItem = NULL;
            continue; // zacneme pekne od zacatku (podminka cyklu se otestuje)
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

    // zaloha dat na stack ('d' prestane byt platne po 'Continue')
    unsigned(WINAPI * threadBody)(void*) = d->Body;
    void* threadParam = d->Param;

    SetEvent(d->Continue); // pustime dal hl. thread
    d = NULL;

    // spustime nas thread
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
        // pridame thread do fronty threadu tohoto pluginu
        if (!Add(new CThreadQueueItem(thread, tid)))
        {
            TRACE_E("Unable to add thread to the queue.");
            TerminateThread(thread, 666);          // je suspended, takze nebude v zadne kriticke sekci, atd.
            WaitForSingleObject(thread, INFINITE); // pockame az thread skutecne skonci, nekdy mu to dost trva
            CloseHandle(thread);

            CS.Leave();

            return NULL;
        }

        // zapis dokud thread nebezi (zarucuje, ze uz nedobehl a jeho objekt neni dealokovany)
        if (threadHandle != NULL)
            *threadHandle = thread;
        if (threadID != NULL)
            *threadID = tid;

        SalamanderDebug->TraceAttachThread(thread, tid);
        ResumeThread(thread);

        WaitForSingleObject(Continue, INFINITE); // pockame na predani dat do CThreadQueue::ThreadBase

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

    unsigned ret = thread->Body(); // spusteni tela threadu

    delete thread; // likvidace objektu threadu
    return ret;
}

HANDLE
CThread::Create(CThreadQueue& queue, unsigned stack_size, DWORD* threadID)
{
    // POZOR: po volani StartThread() muze byt 'this' neplatny (proto je zapis do 'Thread' uvnitr)
    return queue.StartThread(UniversalBody, this, stack_size, &Thread, threadID);
}
