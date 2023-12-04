// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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
    DWORD ThreadID; // jen pro ladici ucely (nalezeni threadu v seznamu threadu v debuggeru)
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
    const char* QueueName; // jmeno fronty (jen pro debugovaci ucely)
    CThreadQueueItem* Head;
    HANDLE Continue; // musime pockat na predani dat do startovaneho threadu

    struct CCS // pristup z vice threadu -> nutna synchronizace
    {
        CRITICAL_SECTION cs;

        CCS() { InitializeCriticalSection(&cs); }
        ~CCS() { DeleteCriticalSection(&cs); }

        void Enter() { EnterCriticalSection(&cs); }
        void Leave() { LeaveCriticalSection(&cs); }
    } CS;

public:
    CThreadQueue(const char* queueName /* napr. "DemoPlug Viewers" */);
    ~CThreadQueue();

    // spusti funkci 'body' s parametrem 'param' v nove vytvorenem threadu se zasobnikem
    // o velikosti 'stack_size' (0 = default); vraci handle threadu nebo NULL pri chybe,
    // zaroven vysledek zapise pred spustenim threadu (resume) i do 'threadHandle'
    // (neni-li NULL), vraceny handle threadu pouzivat jen na testy na NULL a pro volani
    // metod CThreadQueue: WaitForExit() a KillThread(); zavreni handlu threadu zajistuje
    // tento objekt fronty
    // POZOR: -thread se muze spustit se zpozdenim az po navratu ze StartThread()
    //         (je-li 'param' ukazatel na strukturu ulozenou na zasobniku, je nutne
    //          synchronizovat predani dat z 'param' - hl. thread musi pockat
    //          na prevzeti dat novym threadem)
    //        -vraceny handle threadu jiz muze byt zavreny pokud thread dobehne pred
    //         navratem ze StartThread() a z jineho threadu se vola StartThread() nebo
    //         KillAll()
    // mozne volat z libovolneho threadu
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
    // ceka 'forceWaitTime' (v ms) na ukonceni vsech threadu, pak bezici thready zabije
    // (jejich objekty se nedealokuji, protoze jejich stav je neznamy, mozna nekonzistentni);
    // vraci TRUE, jsou-li vsechny thready ukoncene, pri 'force' TRUE vzdy vraci TRUE;
    // je-li 'force' FALSE a nejaky thread jeste bezi, ceka 'waitTime' (v ms) na ukonceni
    // vsech threadu, pokud pak jeste stale neco bezi, vraci FALSE; cas INFINITE = neomezene
    // dlouhe cekani
    // mozne volat z libovolneho threadu
    BOOL KillAll(BOOL force, int waitTime = 1000, int forceWaitTime = 200, DWORD exitCode = 666);

protected:                                                 // vnitrni nesynchronizovane metody
    BOOL Add(CThreadQueueItem* item);                      // prida polozku do fronty, vraci uspech
    BOOL FindAndLockItem(HANDLE thread);                   // najde polozku pro 'thread' ve fronte a zamkne ji
    void UnlockItem(HANDLE thread, BOOL deleteIfUnlocked); // odemkne polozku pro 'thread' ve fronte, pripadne ji smaze
    void ClearFinishedThreads();                           // vyradi z fronty thready, ktere jiz dobehly
    static DWORD WINAPI ThreadBase(void* param);           // univerzalni body threadu
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
    char Name[101]; // buffer pro jmeno threadu (pouziva se v TRACE a CALL-STACK pro identifikaci threadu)
                    // POZOR: pokud budou data threadu obsahovat odkazy na stack nebo jine docasne objekty,
                    //        je potreba zajistit, aby se s temito odkazy pracovalo jen po dobu jejich platnosti

public:
    CThread(const char* name = NULL);
    virtual ~CThread() {} // aby se spravne volaly destruktory potomku

    // vytvoreni (start) threadu ve fronte threadu 'queue'; 'stack_size' je velikost zasobniku
    // noveho threadu v bytech (0 = default); vraci handle noveho threadu nebo NULL pri chybe;
    // zavreni handlu zajistuje objekt 'queue'; pokud se podari vytvorit thread, je tento objekt
    // dealokovan pri ukonceni threadu, pri chybe spousteni je dealokace objektu na volajicim
    // POZOR: bez pridani synchronizace muze thread dobehnout jeste pred navratem z Create() ->
    //        ukazatel "this" je proto po uspesnem volani Create() nutne povazovat za neplatny,
    //        to same plati pro vraceny handle threadu (pouzivat jen na testy na NULL a pro volani
    //        metod CThreadQueue: WaitForExit() a KillThread())
    // mozne volat z libovolneho threadu
    HANDLE Create(CThreadQueue& queue, unsigned stack_size = 0, DWORD* threadID = NULL);

    // vraci 'Thread' viz vyse
    HANDLE GetHandle() { return Thread; }

    // vraci jmeno threadu
    const char* GetName() { return Name; }

    // tato metoda obsahuje telo threadu
    virtual unsigned Body() = 0;

protected:
    static unsigned WINAPI UniversalBody(void* param); // pomocna metoda pro spousteni threadu
};
