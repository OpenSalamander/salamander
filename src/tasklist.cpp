// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "tasklist.h"
#include "plugins.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"

#pragma warning(disable : 4074)
#pragma init_seg(compiler) // inicializaci provedeme co nejdrive

#define NOHANDLES(function) function // obrana proti zanaseni maker HANDLES do zdrojaku pomoci CheckHnd

CTaskList TaskList;

BOOL FirstInstance_3_or_later = FALSE;

// process list je sdileny skrz vsechny salamander v lokani session
// od AS 3.0 menime pojeti "Break" udalosti - vyvola v cili exception, takze mame "plnotucny" bug report, ale zaroven tim cil konci
// proto menim nasledujici konstanty "AltapSalamander*" -> "AltapSalamander3*", abychom byli oddeleni od starsich verzi

// POZOR: pri zmene je potreba upravit salbreak.exe, proste posli mi prosim info ... dik, Petr

const char* AS_PROCESSLIST_NAME = "AltapSalamander3bProcessList";                               // sdilena pamet CProcessList
const char* AS_PROCESSLIST_MUTEX_NAME = "AltapSalamander3bProcessListMutex";                    // synchronizace pro pristup do sdilene pameti
const char* AS_PROCESSLIST_EVENT_NAME = "AltapSalamander3bProcessListEvent";                    // odpaleni udalosti (co se ma delat je ulozeno ve sdilene pameti)
const char* AS_PROCESSLIST_EVENT_PROCESSED_NAME = "AltapSalamander3bProcessListEventProcessed"; // odpalena udalost byla zpracovana

const char* FIRST_SALAMANDER_MUTEX_NAME = "AltapSalamanderFirstInstance";     // zavedeno od AS 2.52 beta 1
const char* LOADSAVE_REGISTRY_MUTEX_NAME = "AltapSalamanderLoadSaveRegistry"; // zavedeno od AS 2.52 beta 1

// cesta, kam ulozimi bug report a minidump; pozdeji je salmon zabali do 7z a uploadne na server
char BugReportPath[MAX_PATH] = "";

CRITICAL_SECTION CommandLineParamsCS;
CCommandLineParams CommandLineParams;
HANDLE CommandLineParamsProcessed;

// handle hlavniho okna (neni dobre z control threadu pristupovat na MainWindow, ktere se nam muze pod rukama nastavit na NULL)
HWND HSafeMainWindow = NULL;

void RaiseBreakException()
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif                                                   // CALLSTK_DISABLE
    RaiseException(OPENSAL_EXCEPTION_BREAK, 0, 0, NULL); // nase vlastni "break" exception
                                                         // sem uz se kod nedostane
}

//
// ****************************************************************************
// CTaskList
//

DWORD WINAPI FControlThread(void* param)
{
    // toto vlakno neni volane s nasim CCallStack - narazil jsem pri leaknutem handlu, ze mi pri pokusu o jeho
    // vypis (pri ukonceni Salamandera) padal Salam

    CTaskList* tasklist = (CTaskList*)param;

    SetThreadNameInVC("ControlThread");

    HANDLE arr[3];
    arr[0] = tasklist->TerminateEvent;
    arr[1] = tasklist->Event;
    arr[2] = SalShExtDoPasteEvent;

    DWORD lastTodoUID = 0;

    DWORD ourPID = GetCurrentProcessId();

    BOOL loop = TRUE;
    while (loop)
    {
        DWORD waitRet = WaitForMultipleObjects(arr[2] == NULL ? 2 : 3, arr, FALSE, INFINITE);
        switch (waitRet)
        {
        case WAIT_OBJECT_0 + 0: // tasklist->TerminateEvent
        {
            loop = FALSE;
            break;
        }

        case WAIT_OBJECT_0 + 1: // tasklist->Event
        {
            // zabereme ProcessList
            waitRet = WaitForSingleObject(tasklist->FMOMutex, TASKLIST_TODO_TIMEOUT);
            if (waitRet == WAIT_FAILED)
                Sleep(50); // abychom nezrali CPU
            if (waitRet == WAIT_FAILED || waitRet == WAIT_TIMEOUT)
                break;

            // ochrana proti cykleni po provedeni commandu
            if (tasklist->ProcessList->TodoUID <= lastTodoUID)
            {
                // uvolnime ProcessList
                ReleaseMutex(tasklist->FMOMutex);
                Sleep(50); // dame prilezitost dalsim procesum
                break;
            }
            else
                lastTodoUID = tasklist->ProcessList->TodoUID;

            // mame zabrany ProcessList
            DWORD pid = tasklist->ProcessList->PID;
            if (pid != ourPID) // pokud se udalost netyka nas
            {
                // uvolnime ProcessList
                ReleaseMutex(tasklist->FMOMutex);
                Sleep(50); // dame prilezitost dalsim procesum
                break;
            }

            // nyni jiz bezime v procesu, ktery mel zpravu obdrzet; zaroven jsme ve vedlejsim vlakne, takze
            // pripadnou komunikaci s hlavnim vlaknem je treba resit dalsi synchronizaci

            // resetneme Event, protoze ted uz vime, ze patril nam a je zbytecne nechat bezet control thready ostatnich procesu
            ResetEvent(tasklist->Event);

            // overime z timestampu, zda jsme jiz neprosvihli dobu, kterou jsme meli k dispozici pro odbaveni prikazu
            DWORD tickCount = GetTickCount();
            if (tickCount - tasklist->ProcessList->TodoTimestamp >= TASKLIST_TODO_TIMEOUT)
            {
                // TIMEOUT
                // uvolnime ProcessList
                ReleaseMutex(tasklist->FMOMutex);
                break;
            }

            // poridime si kopii zabraneho ProcessList
            CProcessList processList;
            memcpy(&processList, tasklist->ProcessList, sizeof(CProcessList));
            // a uvolnime sdilenou pamet
            ReleaseMutex(tasklist->FMOMutex);

            switch (processList.Todo)
            {
            case TASKLIST_TODO_HIGHLIGHT:
            {
                SetEvent(tasklist->EventProcessed); // zprava pro proces-zadavatele: mame hotovo
                if (HSafeMainWindow != NULL)
                    PostMessage(HSafeMainWindow, WM_USER_FLASHWINDOW, 0, 0);
                break;
            }

            case TASKLIST_TODO_BREAK:
            {
                SetEvent(tasklist->EventProcessed); // zprava pro proces-zadavatele: mame hotovo

                RaiseBreakException();
                // sem uz se kod nedostane

                break;
            }

            case TASKLIST_TODO_TERMINATE:
            {
                SetEvent(tasklist->EventProcessed); // zprava pro proces-zadavatele: mame hotovo

                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                if (h != NULL)
                {
                    TerminateProcess(h, 666);
                    CloseHandle(h);
                }
                break;
            }

            case TASKLIST_TODO_ACTIVATE:
            {
                // nakopirujeme ProcessList do globalni promenne CommandLineParams,
                // kterou monitoruje hlavni thread pri vstupu od idle;
                NOHANDLES(EnterCriticalSection(&CommandLineParamsCS));
                memcpy(&CommandLineParams, &processList.CommandLineParams, sizeof(CCommandLineParams));
                ResetEvent(CommandLineParamsProcessed);
                NOHANDLES(LeaveCriticalSection(&CommandLineParamsCS));

                // pro pripad, ze je hlavni thread v IDLE do nej strcime a vnutime mu kontrolu CommandLineParams::RequestUID
                // pokud v IDLE neni, neco prave resi a zpravu odbavi ve chvili, kdy do IDLE vstoupi (pokud se dockame)
                if (HSafeMainWindow != NULL)
                    PostMessage(HSafeMainWindow, WM_USER_WAKEUP_FROM_IDLE, 0, 0);

                // pockame 5 vterin, zda se hlavni thread ozve (zatim nevstoupime do kriticke sekce, aby tam mohl on)
                WaitForSingleObject(CommandLineParamsProcessed, TASKLIST_TODO_TIMEOUT);

                // nyni jiz do kriticke sekce muzeme vstoupit
                NOHANDLES(EnterCriticalSection(&CommandLineParamsCS));
                CommandLineParams.RequestUID = 0;                             // zakazeme hlavnimu threadu pripadne dalsi akce
                waitRet = WaitForSingleObject(CommandLineParamsProcessed, 0); // preptame se, jak vypada aktualni stav eventu
                if (waitRet == WAIT_OBJECT_0)
                    SetEvent(tasklist->EventProcessed); // zprava pro proces-zadavatele: mame hotovo
                NOHANDLES(LeaveCriticalSection(&CommandLineParamsCS));
                break;
            }

            default:
            {
                TRACE_E("FControlThread: unknown todo=" << processList.Todo);
                break;
            }
            }
            break;
        }

        case WAIT_OBJECT_0 + 2: // SalShExtDoPasteEvent
        {
            BOOL sleep = TRUE;
            if (SalShExtSharedMemMutex != NULL)
            {
                WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                if (HSafeMainWindow != NULL && SalShExtSharedMemView != NULL &&
                    SalShExtSharedMemView->SalamanderMainWnd == (UINT64)(DWORD_PTR)HSafeMainWindow)
                {
                    ResetEvent(SalShExtDoPasteEvent); // "zdrojovy" Salamander uz se nasel, dalsi hledani je zbytecne
                    sleep = FALSE;
                    PostMessage(HSafeMainWindow, WM_USER_SALSHEXT_PASTE, SalShExtSharedMemView->PostMsgIndex, 0);
                }
                ReleaseMutex(SalShExtSharedMemMutex);
            }
            if (sleep)
                Sleep(50); // dame sanci dalsim Salamanderum
            break;
        }

        default: // toto by nemelo nastat
        {
            Sleep(50); // abychom nezrali CPU
            break;
        }
        }
    }

    return 0;
}

CTaskList::CTaskList()
{
    // bezime ve skupine 'compiler', tedy pred ms_init
    OK = FALSE;
    FMO = NULL;
    ProcessList = NULL;
    FMOMutex = NULL;
    Event = NULL;
    EventProcessed = NULL;
    TerminateEvent = NULL;
    ControlThread = NULL;
    // vnitrni synchronizace mezi ControlThread a hlavnim vlaknem
    NOHANDLES(InitializeCriticalSection(&CommandLineParamsCS));
    CommandLineParamsProcessed = NULL;
}

BOOL CTaskList::Init()
{
    OK = FALSE;

    PSID psidEveryone;
    PACL paclNewDacl;
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES* saPtr = CreateAccessableSecurityAttributes(&sa, &sd, GENERIC_ALL, &psidEveryone, &paclNewDacl);

    //---  nejdrive takova bokovka: pod Vista+ vytvorime event pro komunikaci s copy-hookem (ceka se na nej v control-threadu)
    if (WindowsVistaAndLater)
    {
        SalShExtDoPasteEvent = NOHANDLES(CreateEvent(saPtr, TRUE, FALSE, SALSHEXT_DOPASTEEVENTNAME));
        if (SalShExtDoPasteEvent == NULL)
            SalShExtDoPasteEvent = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, SALSHEXT_DOPASTEEVENTNAME));
        if (SalShExtDoPasteEvent == NULL)
            TRACE_E("CTaskList::Init(): unable to create event object for communicating with copy-hook shell extension!");
    }

    //---  pokusime se pripojit na FMO-mutex - zaroven test jestli uz nejaky Salamander bezi
    FMOMutex = NOHANDLES(OpenMutex(SYNCHRONIZE, FALSE, AS_PROCESSLIST_MUTEX_NAME));
    if (FMOMutex == NULL) // jsme prvni Salamander 3.0 nebo novejsi v lokalni sessione
    {
        //---  vytvoreni systemovych objektu pro komunikaci, zabereme FMO
        FMOMutex = NOHANDLES(CreateMutex(saPtr, TRUE, AS_PROCESSLIST_MUTEX_NAME)); // task list je platny pouze pro danou session, mutex patri do local namespace
        if (FMOMutex == NULL)
            return FALSE; // fail
        FMO = NOHANDLES(CreateFileMapping(INVALID_HANDLE_VALUE, saPtr, PAGE_READWRITE | SEC_COMMIT,
                                          0, sizeof(CProcessList), AS_PROCESSLIST_NAME));
        if (FMO == NULL)
            return FALSE; // fail
        ProcessList = (CProcessList*)NOHANDLES(MapViewOfFile(FMO, FILE_MAP_WRITE, 0, 0, 0));
        if (ProcessList == NULL)
            return FALSE; // fail
        Event = NOHANDLES(CreateEvent(saPtr, TRUE, FALSE, AS_PROCESSLIST_EVENT_NAME));
        if (Event == NULL)
            return FALSE; // fail
        EventProcessed = NOHANDLES(CreateEvent(saPtr, TRUE, FALSE, AS_PROCESSLIST_EVENT_PROCESSED_NAME));
        if (EventProcessed == NULL)
            return FALSE; // fail

        //---  inicializace sdilene pameti
        ZeroMemory(ProcessList, sizeof(CProcessList));
        ProcessList->Version = 1; // 3.0 beta 4

        ProcessList->ItemsCount = 1;
        ProcessList->ItemsStateUID++;
        ProcessList->Items[0] = CProcessListItem();

        //---  uvolnime FMO
        ReleaseMutex(FMOMutex);
    }
    else // dalsi instance, jen se pripojime ...
    {
        //---  zabereme FMO
        DWORD waitRet = WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT);
        if (waitRet == WAIT_TIMEOUT)
            return FALSE; // fail

        //---  pripojime se na ostatni systemove objekty pro komunikaci
        FMO = NOHANDLES(OpenFileMapping(FILE_MAP_WRITE, FALSE, AS_PROCESSLIST_NAME));
        if (FMO == NULL)
            return FALSE; // fail
        ProcessList = (CProcessList*)NOHANDLES(MapViewOfFile(FMO, FILE_MAP_WRITE, 0, 0, 0));
        if (ProcessList == NULL)
            return FALSE; // fail
        // aby na event bylo mozne volat SetEvent(), musi mit nahozeny EVENT_MODIFY_STATE, pro Wait* potrebuje SYNCHRONIZE
        Event = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_NAME));
        if (Event == NULL)
            return FALSE; // fail
        EventProcessed = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_PROCESSED_NAME));
        if (EventProcessed == NULL)
            return FALSE; // fail

        //---  pridame zaznam do sdilene pameti
        BOOL attempt = 0;
    AGAIN:
        int c = ProcessList->ItemsCount;
        if (c < MAX_TL_ITEMS) // pokud jich neni prilis, pridame tento proces
        {
            ProcessList->ItemsCount++;
            ProcessList->ItemsStateUID++;
            ProcessList->Items[c] = CProcessListItem();
        }
        else
        {
            if (attempt == 0)
            {
                // pole je plne, zkusime ho setrast (nektery z procesu mohl chcipnout a nedat nam vedet)
                RemoveKilledItems(NULL);
                attempt++;
                goto AGAIN;
            }
        }

        //---  uvolnime FMO
        ReleaseMutex(FMOMutex);
    }

    // detekce ostatnich instanci Salamandera
    LPTSTR sid = NULL;
    if (!GetStringSid(&sid))
        sid = NULL;
    char mutexName[1000];
    if (sid == NULL)
    {
        // chyba v ziskani SID -- lokalni name space, bez pripojeneho SID
        _snprintf_s(mutexName, _TRUNCATE, "%s", FIRST_SALAMANDER_MUTEX_NAME);
    }
    else
    {
        _snprintf_s(mutexName, _TRUNCATE, "Global\\%s_%s", FIRST_SALAMANDER_MUTEX_NAME, sid);
        LocalFree(sid);
    }
    HANDLE hMutex = NOHANDLES(CreateMutex(saPtr, FALSE, mutexName));
    DWORD lastError = GetLastError();
    if (hMutex != NULL)
    {
        FirstInstance_3_or_later = (lastError != ERROR_ALREADY_EXISTS);
    }
    else
    {
        hMutex = NOHANDLES(OpenMutex(SYNCHRONIZE, FALSE, mutexName));
        lastError = GetLastError();
        if (hMutex != NULL)
            FirstInstance_3_or_later = FALSE;
    }

    if (psidEveryone != NULL)
        FreeSid(psidEveryone);
    if (paclNewDacl != NULL)
        LocalFree(paclNewDacl);

    TerminateEvent = NOHANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
    if (TerminateEvent == NULL)
        return FALSE; // fail

    // vnitrni synchronizace mezi ControlThread a hlavnim vlaknem
    CommandLineParamsProcessed = CreateEvent(NULL, TRUE, FALSE, NULL); // manual, nonsignaled
    if (CommandLineParamsProcessed == NULL)
        return FALSE; // failed

    // nelze pouzit _beginthreadex, protoze jeste nemusi byt inicializovana knihovna
    DWORD id;
    ControlThread = NOHANDLES(CreateThread(NULL, 0, FControlThread, this, 0, &id));
    if (ControlThread == NULL)
        return FALSE; // fail
    // tenhle thread se musi dostat k lizu i kdyby na chleba nebylo ...
    SetThreadPriority(ControlThread, THREAD_PRIORITY_TIME_CRITICAL);

    OK = TRUE;
    return TRUE;
}

CTaskList::~CTaskList()
{
    if (ControlThread != NULL)
    {
        SetEvent(TerminateEvent);                     // terminuj se!
        WaitForSingleObject(ControlThread, INFINITE); // pockame nez se thread dokonci
        NOHANDLES(CloseHandle(ControlThread));
    }
    if (TerminateEvent != NULL)
        NOHANDLES(CloseHandle(TerminateEvent));

    // vyradime se ze seznamu
    if (OK)
    {
        //---  zabereme FMO
        if (WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT) != WAIT_TIMEOUT)
        {
            CProcessListItem* ptr = ProcessList->Items;
            int c = ProcessList->ItemsCount;

            //---  vyhodime aktualni proces, ukoncuje se ...
            DWORD PID = GetCurrentProcessId();
            int i;
            for (i = 0; i < c; i++)
            {
                if (PID == ptr[i].PID)
                {
                    //---  vykopneme proces ze seznamu
                    memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CProcessListItem));
                    c--;
                    i--;
                }
            }
            ProcessList->ItemsCount = c;
            ProcessList->ItemsStateUID++;

            //---  uvolnime FMO
            ReleaseMutex(FMOMutex);
        }
    }

    if (ProcessList != NULL)
        NOHANDLES(UnmapViewOfFile(ProcessList));
    if (FMO != NULL)
        NOHANDLES(CloseHandle(FMO));
    if (FMOMutex != NULL)
        NOHANDLES(CloseHandle(FMOMutex));
    if (Event != NULL)
        NOHANDLES(CloseHandle(Event));
    if (EventProcessed != NULL)
        NOHANDLES(CloseHandle(EventProcessed));
    if (CommandLineParamsProcessed != NULL)
        NOHANDLES(CloseHandle(CommandLineParamsProcessed));
    NOHANDLES(DeleteCriticalSection(&CommandLineParamsCS));

    if (SalShExtDoPasteEvent != NULL)
        NOHANDLES(CloseHandle(SalShExtDoPasteEvent));
    SalShExtDoPasteEvent = NULL;
}

BOOL CTaskList::SetProcessState(DWORD processState, HWND hMainWindow, BOOL* timeouted)
{
    if (timeouted != NULL)
        *timeouted = FALSE;

    HSafeMainWindow = hMainWindow;

    if (OK)
    {
        DWORD ret = WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT);
        if (ret != WAIT_FAILED && ret != WAIT_TIMEOUT)
        {
            // dohledame se v seznamu procesu a nastavime processState a hMainWindow
            CProcessListItem* ptr = ProcessList->Items;
            int c = ProcessList->ItemsCount;
            DWORD PID = GetCurrentProcessId();
            int i;
            for (i = 0; i < c; i++)
            {
                if (PID == ptr[i].PID)
                {
                    ptr[i].ProcessState = processState;
                    ptr[i].HMainWindow = (UINT64)(DWORD_PTR)hMainWindow; // 64b pro x64/x86 kompatibilitu
                    break;
                }
            }
            ReleaseMutex(FMOMutex);
            return TRUE;
        }
        else
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            TRACE_E("SetProcessState(): WaitForSingleObject failed!");
        }
    }
    return FALSE;
}

int CTaskList::GetItems(CProcessListItem* items, DWORD* itemsStateUID, BOOL* timeouted)
{
    if (timeouted != NULL)
        *timeouted = FALSE;
    if (OK)
    {
        BOOL changed = FALSE;
        //---  zabereme FMO
        if (WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT) == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return 0; // fail
        }

        CProcessListItem* ptr = ProcessList->Items;

        //---  vyhodime killnute procesy
        RemoveKilledItems(&changed);

        //---  navratove hodnoty
        if (items != NULL)
            memcpy(items, ptr, ProcessList->ItemsCount * sizeof(CProcessListItem));
        if (changed)
            ProcessList->ItemsStateUID++;
        if (itemsStateUID != NULL)
            *itemsStateUID = ProcessList->ItemsStateUID;

        int count = ProcessList->ItemsCount;
        //---  uvolnime FMO
        ReleaseMutex(FMOMutex);
        return count;
    }
    else
        return 0;
}

BOOL CTaskList::FireEvent(DWORD todo, DWORD pid, BOOL* timeouted)
{
    if (timeouted != NULL)
        *timeouted = FALSE;
    if (OK)
    {
        // zabereme ProcessList
        DWORD waitRet = WaitForSingleObject(FMOMutex, 2000);
        if (waitRet == WAIT_FAILED)
            return FALSE;
        if (waitRet == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return FALSE; // fail
        }

        // nastavime predavane parametry
        ProcessList->Todo = todo;
        ProcessList->TodoUID++;
        ProcessList->TodoTimestamp = GetTickCount();
        ProcessList->PID = pid;

        // pri breaknuti jine instance Salamandera pustime jeho Salmon nad nas
        if (todo == TASKLIST_TODO_BREAK)
        {
            for (DWORD i = 0; i < ProcessList->ItemsCount; i++)
            {
                if (ProcessList->Items[i].PID == pid)
                {
                    AllowSetForegroundWindow(ProcessList->Items[i].PID);       // radeji povolime i vlastniho Salamandera, i kdyz to je asi zbytecne...
                    AllowSetForegroundWindow(ProcessList->Items[i].SalmonPID); // rozhodne musime pustit nad nas jeho Salmon
                    break;
                }
            }
        }

        // uvolnime ProcessList
        ReleaseMutex(FMOMutex);

        // spustime kontrolu ve vsech Salamanderech
        ResetEvent(EventProcessed);
        SetEvent(Event);

        //---  dame chvilku na reakci (behem teto doby by se nekdo mel "chytnout" a ukol splnit)
        BOOL ret = (WaitForSingleObject(EventProcessed, 1000) == WAIT_OBJECT_0);

        //---  rekneme vsem Salamanderum, at se pripravi na dalsi command
        ResetEvent(Event);

        //---  nastavime zpatky break-PID
        //    ProcessList->Todo = 0;
        //    ProcessList->PID = 0;

        //---  uvolnime FMO

        return ret;
    }
    return FALSE;
}

BOOL CTaskList::ActivateRunningInstance(const CCommandLineParams* cmdLineParams, BOOL* timeouted)
{
    if (timeouted != NULL)
        *timeouted = FALSE;

    if (!OK)
        return FALSE;

    CProcessListItem ourProcessInfo;

    // dohledame bezici proces v nasi tride, pripadne startujici (na ktery chvili pockame jestli se rozebehne)
    int firstStarting = -1; // index procesu, ktery je z nasi tridy (shodny Integrity Level a SID) ale nema jeste hlavni okno
    int firstRunnig = -1;   // index procesu, ktery je z nasi tridy (shodny Integrity Level a SID) a jiz bezi (ma hlavni okno)
    DWORD timeStamp = GetTickCount();
    do
    {
        firstStarting = -1;
        firstRunnig = -1;
        DWORD ret = WaitForSingleObject(FMOMutex, 200);
        if (ret == WAIT_FAILED)
            return FALSE;
        if (ret != WAIT_TIMEOUT) // obdrzeli jsme mutex
        {
            int i;
            for (i = 0; i < (int)ProcessList->ItemsCount; i++)
            {
                CProcessListItem* item = &ProcessList->Items[i];
                // hledam pouze procesy v nasi tride (shodny IntegrityLevel a SID)
                if (item->PID != ourProcessInfo.PID &&
                    item->IntegrityLevel == ourProcessInfo.IntegrityLevel &&
                    memcmp(item->SID_MD5, ourProcessInfo.SID_MD5, 16) == 0)
                {
                    if (item->ProcessState == PROCESS_STATE_RUNNING)
                    {
                        firstRunnig = i;
                        break; // pokud jsme nasli bezici instanci, nemusime uz startujici hledat
                    }
                    if (item->ProcessState == PROCESS_STATE_STARTING && firstStarting == -1)
                        firstStarting = i;
                }
            }

            if (firstRunnig == -1) // zadny proces z nasi tridy zatim nema hlavni okno
            {
                ReleaseMutex(FMOMutex); // takze uvolnime pamet ostatnim
                if (firstStarting == -1)
                    return FALSE; // nenasli jsme zadneho startujiciho kandidata, vypadneme
                else
                    Sleep(200); // nasli jsme startujiciho kandidata, na 200ms se odmlcime, aby mel sanci zavolat SetProcessState()
            }
        }
    } while (firstRunnig == -1 && (GetTickCount() - timeStamp < TASKLIST_TODO_TIMEOUT)); // na bezici instanci cekame maximalne 5s

    // pokud jsme nenasli zadnou instanci z nasi tridy, co by mela hlavni okno, pripadne pokud nam cekani zabralo 5s, zabalime to
    if (firstRunnig == -1)
        return FALSE;

    CProcessListItem* item = &ProcessList->Items[firstRunnig];

    // nastavime Todo, PID a parametry
    ProcessList->Todo = TASKLIST_TODO_ACTIVATE;
    ProcessList->TodoUID++; // rekneme procesum, ze se bude zpracovavat novy command
    ProcessList->TodoTimestamp = GetTickCount();
    ProcessList->PID = item->PID;

    // prebereme parametry z command-line
    memcpy(&ProcessList->CommandLineParams, cmdLineParams, sizeof(CCommandLineParams));
    // a nastavime nase vnitrni promenne
    ProcessList->CommandLineParams.Version = 1;
    ProcessList->CommandLineParams.RequestUID = ProcessList->TodoUID;
    ProcessList->CommandLineParams.RequestTimestamp = ProcessList->TodoTimestamp;

    // povolime aktivovanemu procesu volani SetForegroundWindow, jinak se nebude schopny vytahnout nahoru
    AllowSetForegroundWindow(item->PID);

    // spustime kontrolu ve vsech Salamanderech
    // uvolnime sdilenou pamet
    ReleaseMutex(FMOMutex);

    ResetEvent(EventProcessed);
    SetEvent(Event);

    // dame chvilku na reakci (behem teto doby by se nekdo mel "chytnout" a ukol splnit)
    // 500ms je nase rezerva, abychom bezpecne prekryli podrizena vlakna
    BOOL ret = (WaitForSingleObject(EventProcessed, TASKLIST_TODO_TIMEOUT + 500) == WAIT_OBJECT_0);

    // rekneme vsem Salamanderum, at se pripravi na dalsi command (reseti se i v control threadu, pokud nektery proces provadi todo)
    ResetEvent(Event);

    // vynulujeme todo
    // ProcessList->Todo = 0; // meli bychom si napred zabrat FMOMutex, ale v tomto pripade neni co pokazit a muzeme hodnoty vynulovat
    // ProcessList->PID = 0;

    return ret;
}

BOOL CTaskList::RemoveKilledItems(BOOL* changed)
{
    if (!OK)
        return FALSE;

    if (changed != NULL)
        *changed = FALSE;
    CProcessListItem* ptr = ProcessList->Items;
    int c = ProcessList->ItemsCount;

    int i;
    for (i = 0; i < c; i++)
    {
        HANDLE h = NOHANDLES(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, ptr[i].PID));
        if (h != NULL)
        {
            // na starsich Windows ziskame handle i pro ukonceny proces
            // je proto potreba se jeste doptat na exitcode; od W2K zrejme zbytecne
            BOOL cont = FALSE;
            DWORD exitcode;
            if (!GetExitCodeProcess(h, &exitcode) || exitcode == STILL_ACTIVE)
                cont = TRUE;
            NOHANDLES(CloseHandle(h));
            if (cont)
                continue; // nechame proces v seznamu
        }
        else
        {
            DWORD lastError = GetLastError();
            if (lastError == ERROR_ACCESS_DENIED)
            {
                continue; // nechame proces v seznamu
            }
        }
        memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CProcessListItem));
        c--;
        i--;
        if (changed != NULL)
            *changed = TRUE;
    }
    ProcessList->ItemsCount = c;

    /*
// neslape pod XP pokud jsou procesy v ramci jedne session spusteny pod ruznymi uzivateli
// nemame pravo otevrit hande jineho procesu
//---  vyhodime killnuty procesy
int i;
    for (i = 0; i < c; i++)
    {
      HANDLE h = NOHANDLES(OpenProcess(PROCESS_TERMINATE, FALSE, ptr[i].PID));
      if (h != NULL)
      {
        BOOL cont = FALSE;
        DWORD exitcode;
        if (!GetExitCodeProcess(h, &exitcode) || exitcode == STILL_ACTIVE) cont = TRUE;
        NOHANDLES(CloseHandle(h));
        if (cont) continue;  // nechame proces v seznamu
      }
//---  vykopneme proces ze seznamu
      memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CTLItem));
      c--;
      i--;
    }
    ((DWORD *)SharedMem)[0] = c;   // items-count
    memcpy(items, ptr, c * sizeof(CTLItem));
*/

    return TRUE;
}