// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <aclapi.h>
#include <crtdbg.h>
#include <process.h>
#include <ostream>
#include <stdio.h>
#include <commctrl.h>
#include <limits.h>

#ifndef __TRACESERVER
#pragma error "macro __TRACESERVER not defined";
#endif // __TRACESERVER

#include "lstrfix.h"
#include "trace.h"
#include "messages.h"
#include "handles.h"
#include "array.h"
#include "str.h"
#include "strutils.h"
#include "winlib.h"
#include "tablist.h"
#include "tserver.h"
#include "openedit.h"
#include "registry.h"
#include "config.h"
#include "allochan.h"

#include "tserver.rh"
#include "tserver.rh2"

#pragma comment(lib, "UxTheme.lib")

#ifndef MULTITHREADED_HEAP_ENABLE
#pragma error "macro MULTITHREADED_HEAP_ENABLE not defined";
#endif // MULTITHREADED_HEAP_ENABLE

#ifdef __HEAP_DISABLE
#pragma error "macro __HEAP_DISABLE defined";
#endif // __HEAP_DISABLE

DWORD CGlobalDataMessage::StaticIndex = 0;

BOOL UseMaxMessagesCount = FALSE;
int MaxMessagesCount = 10000;

BOOL WindowsVistaAndLater = FALSE;

// texty v about dialogu
WCHAR AboutText1[] = L"Version 2.03";

CMainWindow* MainWindow = NULL;

// nazev aplikace
const WCHAR* MAINWINDOW_NAME = L"Trace Server";

// nazvy souboru
//WCHAR *TRACE_FILENAME = L"tserver.trs";

CGlobalData Data;

// mutex - vlastni ho client proces, ktery zapisuje do sdilene pameti
HANDLE OpenConnectionMutex = NULL;
// event - signaled -> sdilena pamet obsahuje pozadovana data
HANDLE ConnectDataReadyEvent = NULL;
// event - signaled -> server prijal data ze sdilene pameti
HANDLE ConnectDataAcceptedEvent = NULL;
BOOL ConnectDataAcceptedEventMayBeSignaled = FALSE;

// event - manual reset - signaled -> server konci -> vsechny thready by meli koncit
HANDLE TerminateEvent = NULL;
// event - pouzity pri startu ReadPipeThreadu pro nacteni vstupnich dat
HANDLE ContinueEvent = NULL;
// event - manual reset - nahodit po flushnuti message cache
HANDLE MessagesFlushDoneEvent = NULL;

// thread, ktery zajistuje pripojeni k serveru
HANDLE ConnectingThread = NULL;

BOOL IconControlEnable = TRUE;

// pole aktivnich pipe threadu
struct CReadPipeThreadInfo
{
    HANDLE Thread;
    DWORD ClientPID;
};
TSynchronizedDirectArray<CReadPipeThreadInfo> ActiveReadPipeThreads(10, 5);

// funkce connecting threadu
unsigned __stdcall ConnectingThreadF(void* mainWndPtr);

//****************************************************************************
//
// InitializeServer
//

BOOL InitializeServer(HWND mainWnd)
{
    // pripravime "NULL PACL", tedy z hlediska prav naprosto otevreny deskriptor
    // cizi proces tak muze naprilad nastavovat prava takto vytvorenych objektu
    // v nasem pripade (TraceServer) nam to ale nevadi a zaroven je to jednodussi
    char secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = &secDesc;
    InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    // give the security descriptor a NULL DACL, done using the  "TRUE, (PACL)NULL" here
    SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, 0, FALSE);
    SECURITY_ATTRIBUTES* saPtr = &sa;

    OpenConnectionMutex = HANDLES_Q(CreateMutex(saPtr, TRUE, __OPEN_CONNECTION_MUTEX));
    ConnectDataReadyEvent = HANDLES_Q(CreateEvent(saPtr, FALSE, FALSE,
                                                  __CONNECT_DATA_READY_EVENT_NAME));
    ConnectDataAcceptedEvent = HANDLES_Q(CreateEvent(saPtr, FALSE, FALSE,
                                                     __CONNECT_DATA_ACCEPTED_EVENT_NAME));

    ContinueEvent = HANDLES(CreateEvent(saPtr, FALSE, FALSE, NULL));
    TerminateEvent = HANDLES(CreateEvent(saPtr, TRUE, FALSE, NULL));         // manual reset
    MessagesFlushDoneEvent = HANDLES(CreateEvent(saPtr, TRUE, FALSE, NULL)); // manual reset

    if (OpenConnectionMutex == NULL || ConnectDataReadyEvent == NULL ||
        ConnectDataAcceptedEvent == NULL || TerminateEvent == NULL ||
        ContinueEvent == NULL || MessagesFlushDoneEvent == NULL)
    {
        MESSAGE_EW(NULL, L"Unable to create synchronization objects.", MB_OK);
        return FALSE;
    }

    unsigned dummyID;
    ConnectingThread = (HANDLE)HANDLES(_beginthreadex(NULL, 1000,
                                                      ConnectingThreadF,
                                                      mainWnd, 0, &dummyID));
    if (ConnectingThread == NULL)
    {
        MESSAGE_EW(NULL, L"Unable to create connecting thread.", MB_OK);
        return FALSE;
    }
    return TRUE; // je-li ConnectingThread != NULL musi vratit TRUE !!!
}

//****************************************************************************
//
// ReleaseServer
//

void ReleaseServer()
{
    if (ConnectingThread != NULL)
    {
        SetEvent(TerminateEvent);
        WaitForSingleObject(ConnectingThread, INFINITE);
        HANDLES(CloseHandle(ConnectingThread));

        ActiveReadPipeThreads.BlockArray();
        int count = ActiveReadPipeThreads.GetCount();
        for (int i = 0; i < count; i++)
        {
            TerminateThread(ActiveReadPipeThreads[i].Thread, 0);
            WaitForSingleObject(ActiveReadPipeThreads[i].Thread, INFINITE);
            HANDLES(CloseHandle(ActiveReadPipeThreads[i].Thread));
        }
        ActiveReadPipeThreads.UnBlockArray();
    }
    if (OpenConnectionMutex != NULL)
        HANDLES(CloseHandle(OpenConnectionMutex));
    if (ConnectDataReadyEvent != NULL)
        HANDLES(CloseHandle(ConnectDataReadyEvent));
    if (ConnectDataAcceptedEvent != NULL)
        HANDLES(CloseHandle(ConnectDataAcceptedEvent));
    if (ContinueEvent != NULL)
        HANDLES(CloseHandle(ContinueEvent));
    if (TerminateEvent != NULL)
        HANDLES(CloseHandle(TerminateEvent));
    if (MessagesFlushDoneEvent != NULL)
        HANDLES(CloseHandle(MessagesFlushDoneEvent));
}

//****************************************************************************
//
// ReadPipeThreadF
//

BOOL ReadPipe(HANDLE pipeSemaphore, DWORD& readBytesFromPipe, HANDLE hFile,
              LPVOID lpBuffer, DWORD nNumberOfBytesToRead, BOOL& showSemaphoreErr)
{
    DWORD read;
    DWORD totalBytesToRead = nNumberOfBytesToRead;
    DWORD numberOfBytesRead = 0;
    while (ReadFile(hFile, (((char*)lpBuffer) + numberOfBytesRead),
                    nNumberOfBytesToRead, &read, NULL))
    {
        readBytesFromPipe += read;
        if (readBytesFromPipe >= 1024)
        {
            if (ReleaseSemaphore(pipeSemaphore, readBytesFromPipe / 1024, NULL))
            {
                readBytesFromPipe %= 1024;
            }
            else
            {
                if (showSemaphoreErr) // ma smysl zorazit jen poprve pro kazdou pajpu
                {
                    MESSAGE_TEW(L"Invalid state of pipe semaphore.", MB_OK);
                    showSemaphoreErr = FALSE;
                }
            }
        }

        numberOfBytesRead += read;
        nNumberOfBytesToRead -= read;
        if (nNumberOfBytesToRead <= 0)
            return numberOfBytesRead == totalBytesToRead;
    }
    numberOfBytesRead += read;
    return FALSE;
}

struct CReadPipeData
{
    static DWORD StaticUniqueProcessID;

    HWND MainWnd;
    HANDLE ReadPipe;
    HANDLE PipeSemaphore;
    HANDLE Thread;
    DWORD ProcessID;
    DWORD UniqueProcessID;
    BOOL SendProcessConnected;
    BOOL ShowSemaphoreErr; // diky chybe ve starych klientech se semafor jen snizuje az vznikne chyba, zname, nehlasime
};

DWORD CReadPipeData::StaticUniqueProcessID = 0;

unsigned __stdcall ReadPipeThreadF(void* dataPtr)
{
    CReadPipeData* data = (CReadPipeData*)dataPtr;
    //---  nacteni vstupnich dat
    HWND mainWnd = data->MainWnd;
    HANDLE readPipe = data->ReadPipe;
    HANDLE pipeSemaphore = data->PipeSemaphore;
    HANDLE thread = data->Thread;
    DWORD processID = data->ProcessID;
    DWORD uniqueProcessID = data->UniqueProcessID;
    BOOL sendProcessConnected = data->SendProcessConnected;
    BOOL showSemaphoreErr = data->ShowSemaphoreErr;
    SetEvent(ContinueEvent);
    //---  od teto chvile je pointer na data neplatny
    data = NULL;
    //---  nacitani zprav z pipy
    DWORD readBytesFromPipe = 0;
    CGlobalDataMessage message;
    message.ProcessID = processID;

    C__PipeDataHeader pipeData;
    BOOL error = FALSE;

    while (1)
    {
        if (ReadPipe(pipeSemaphore, readBytesFromPipe, readPipe, &pipeData, sizeof(pipeData), showSemaphoreErr))
        {
            switch (pipeData.Type)
            {
            case __mtSetProcessName:
            case __mtSetThreadName:
            case __mtSetProcessNameW:
            case __mtSetThreadNameW:
            {
                BOOL unicode = (pipeData.Type == __mtSetProcessNameW || pipeData.Type == __mtSetThreadNameW);
                char* name = (char*)malloc((unicode ? sizeof(WCHAR) : 1) * pipeData.MessageSize);
                if (name != NULL)
                {
                    if (ReadPipe(pipeSemaphore, readBytesFromPipe, readPipe, name,
                                 (unicode ? sizeof(WCHAR) : 1) * pipeData.MessageSize, showSemaphoreErr))
                    {
                        WCHAR* nameW = unicode ? (WCHAR*)name : ConvertAllocA2U(name, pipeData.MessageSize - 1);
                        if (!unicode)
                            free(name);
                        name = NULL;

                        if (nameW != NULL)
                        {
                            if (pipeData.Type == __mtSetProcessName || pipeData.Type == __mtSetProcessNameW)
                            {
                                // v pipeData.Line prisel ProcessID - viz komentar v hlavickaci
                                Data.Processes.BlockArray();
                                int index = Data.FindProcessNameIndex(uniqueProcessID);
                                if (index != -1)
                                {
                                    free(Data.Processes[index].Name);
                                    Data.Processes[index].Name = nameW;
                                }
                                else
                                {
                                    CProcessInformation processInformation;
                                    processInformation.UniqueProcessID = uniqueProcessID;
                                    processInformation.Name = nameW;
                                    // pridam do pole
                                    Data.Processes.Add(processInformation);
                                }
                                Data.Processes.UnBlockArray();
                                PostMessage(mainWnd, WM_USER_PROCESSES_CHANGE, 0, 0);
                            }
                            else
                            {
                                Data.Threads.BlockArray();
                                int index = Data.FindThreadNameIndex(uniqueProcessID,
                                                                     pipeData.UniqueThreadID);
                                if (index != -1)
                                {
                                    free(Data.Threads[index].Name);
                                    Data.Threads[index].Name = nameW;
                                }
                                else
                                {
                                    CThreadInformation threadInformation;
                                    threadInformation.UniqueProcessID = uniqueProcessID;
                                    threadInformation.UniqueThreadID = pipeData.UniqueThreadID;
                                    threadInformation.Name = nameW;

                                    // pridam do pole
                                    Data.Threads.Add(threadInformation);
                                }
                                Data.Threads.UnBlockArray();
                                PostMessage(mainWnd, WM_USER_THREADS_CHANGE, 0, 0);
                            }
                        }
                        else
                        {
                            PostMessage(mainWnd, WM_USER_SHOWERROR, EC_LOW_MEMORY, 0);
                            error = TRUE;
                            SetLastError(ERROR_BROKEN_PIPE); // kvuli podmince nize
                        }
                    }
                    else
                    {
                        DWORD err = GetLastError();
                        free(name);
                        error = TRUE;
                        if (err == ERROR_SUCCESS)
                            SetLastError(ERROR_BROKEN_PIPE); // kvuli podmince nize
                    }
                }
                else
                {
                    PostMessage(mainWnd, WM_USER_SHOWERROR, EC_LOW_MEMORY, 0);
                    error = TRUE;
                    SetLastError(ERROR_BROKEN_PIPE); // kvuli podmince nize
                }
                break;
            }

            case __mtInformation:
            case __mtError:
            case __mtInformationW:
            case __mtErrorW:
            {
                BOOL unicode = pipeData.Type == __mtInformationW || pipeData.Type == __mtErrorW;

                message.ThreadID = pipeData.ThreadID;
                message.Type = (C__MessageType)pipeData.Type;
                message.Time = pipeData.Time;
                message.Counter = pipeData.Counter;
                message.Line = pipeData.Line;
                message.UniqueProcessID = uniqueProcessID;
                message.UniqueThreadID = pipeData.UniqueThreadID;

                char* file = (char*)malloc((unicode ? sizeof(WCHAR) : 1) * pipeData.MessageSize);
                if (file != NULL)
                {
                    if (ReadPipe(pipeSemaphore, readBytesFromPipe, readPipe, file,
                                 (unicode ? sizeof(WCHAR) : 1) * pipeData.MessageSize, showSemaphoreErr))
                    {
                        message.File = unicode ? (WCHAR*)file : ConvertAllocA2U(file, pipeData.MessageSize - 1);
                        if (!unicode)
                            free(file);
                        file = NULL;
                        if (message.File != NULL)
                        {
                            message.Message = message.File + (unicode ? pipeData.MessageTextOffset : wcslen(message.File) + 1);

                            while (1)
                            {
                                BOOL breakCycle;
                                Data.MessagesCache.BlockArray();
                                if (Data.MessagesCache.GetCount() >= MESSAGES_CACHE_MAX)
                                {
                                    if (!Data.MessagesFlushInProgress)
                                    {
                                        ResetEvent(MessagesFlushDoneEvent);
                                        PostMessage(mainWnd, WM_USER_FLUSH_MESSAGES_CACHE, 0, 0);
                                        Data.MessagesFlushInProgress = TRUE;
                                    }
                                    breakCycle = FALSE;
                                }
                                else
                                {
                                    Data.MessagesCache.Add(message);
                                    breakCycle = TRUE;
                                }
                                Data.MessagesCache.UnBlockArray();

                                if (breakCycle)
                                    break;
                                else
                                    WaitForSingleObject(MessagesFlushDoneEvent, INFINITE);
                            } // neni to dokonale, ale bude to muset stacit (nepresahne maximum tak na 99%)
                        }
                        else
                        {
                            PostMessage(mainWnd, WM_USER_SHOWERROR, EC_LOW_MEMORY, 0);
                            error = TRUE;
                            SetLastError(ERROR_BROKEN_PIPE); // kvuli podmince nize
                        }
                    }
                    else
                    {
                        DWORD err = GetLastError();
                        free(file);
                        error = TRUE;
                        if (err == ERROR_SUCCESS)
                            SetLastError(ERROR_BROKEN_PIPE); // kvuli podmince nize
                    }
                }
                else
                {
                    PostMessage(mainWnd, WM_USER_SHOWERROR, EC_LOW_MEMORY, 0);
                    error = TRUE;
                    SetLastError(ERROR_BROKEN_PIPE); // kvuli podmince nize
                }
                break;
            }

            case __mtIgnoreAutoClear:
            {
                if (sendProcessConnected && pipeData.ThreadID == 0) // 0 = neignorovat, 1 = ignorovat auto-clear na Trace Serveru
                    SendMessage(mainWnd, WM_USER_PROCESS_CONNECTED, 0, 0);
                break;
            }

            default:
            {
                PostMessage(mainWnd, WM_USER_SHOWERROR, EC_UNKNOWN_MESSAGE_TYPE, 0);
                error = TRUE;
                SetLastError(ERROR_BROKEN_PIPE); // kvuli podmince nize
                break;
            }
            }
        }
        else
            error = TRUE;

        if (error)
        {
            if (GetLastError() != ERROR_BROKEN_PIPE)
                PostMessage(mainWnd, WM_USER_SHOWSYSTEMERROR, GetLastError(), 0);
            break;
        }
    }
    //---  doslo k odpojeni processu
    PostMessage(mainWnd, WM_USER_PROCESS_DISCONNECTED, processID, 0);
    HANDLES(CloseHandle(readPipe));
    HANDLES(CloseHandle(pipeSemaphore));

    ActiveReadPipeThreads.BlockArray();
    int count = ActiveReadPipeThreads.GetCount();
    int i = 0;
    for (; i < count; i++)
    {
        if (ActiveReadPipeThreads[i].Thread == thread)
        {
            HANDLES(CloseHandle(thread));
            ActiveReadPipeThreads.Delete(i);
            break;
        }
    }
    if (i == count)
    {
        MESSAGE_TEW(L"Thread handle " << thread << L" was not found in array ActiveReadPipeThreads.", MB_OK);
    }
    ActiveReadPipeThreads.UnBlockArray();
    _endthreadex(0);
    return 0;
}

//****************************************************************************
//
// IsReadPipeThreadForNewProcess
//

BOOL IsReadPipeThreadForNewProcess(DWORD clientPID)
{
    ActiveReadPipeThreads.BlockArray();
    BOOL isNewProcess = TRUE;
    int count = ActiveReadPipeThreads.GetCount();
    for (int i = 0; i < count; i++)
    {
        if (ActiveReadPipeThreads[i].ClientPID == clientPID)
        {
            isNewProcess = FALSE;
            break;
        }
    }
    ActiveReadPipeThreads.UnBlockArray();
    return isNewProcess;
}

//****************************************************************************
//
// ConnectingThreadF
//

unsigned __stdcall ConnectingThreadF(void* mainWndPtr)
{
    HWND mainWnd = (HWND)mainWndPtr;
    //---  vytvoreni sdilene pameti

    // pripravime "NULL PACL", tedy z hlediska prav naprosto otevreny deskriptor
    // cizi proces tak muze naprilad nastavovat prava takto vytvorenych objektu
    // v nasem pripade (TraceServer) nam to ale nevadi a zaroven je to jednodussi
    char secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = &secDesc;
    InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    // give the security descriptor a NULL DACL, done using the  "TRUE, (PACL)NULL" here
    SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, 0, FALSE);
    SECURITY_ATTRIBUTES* saPtr = &sa;

    HANDLE hFileMapping = HANDLES_Q(CreateFileMapping((HANDLE)0xFFFFFFFF,
                                                      saPtr,
                                                      PAGE_READWRITE,
                                                      0, sizeof(C__ClientServerInitData),
                                                      __FILE_MAPPING_NAME));
    if (hFileMapping == NULL)
    {
        PostMessage(mainWnd, WM_USER_CT_TERMINATED, 0, 0);
        _endthreadex(CT_UNABLE_TO_CREATE_FILE_MAPPING);
        return CT_UNABLE_TO_CREATE_FILE_MAPPING;
    }

    void* mapAddress = HANDLES(MapViewOfFile(hFileMapping,
                                             FILE_MAP_ALL_ACCESS,
                                             0,
                                             0,
                                             sizeof(C__ClientServerInitData)));
    if (mapAddress == NULL)
    {
        HANDLES(CloseHandle(hFileMapping));
        PostMessage(mainWnd, WM_USER_CT_TERMINATED, 0, 0);
        _endthreadex(CT_UNABLE_TO_MAP_VIEW_OF_FILE);
        return CT_UNABLE_TO_MAP_VIEW_OF_FILE;
    }
    //---  vykona cast
    PostMessage(mainWnd, WM_USER_CT_OPENCONNECTION, 0, 0);

    HANDLE handles[2];
    handles[0] = TerminateEvent;
    handles[1] = ConnectDataReadyEvent;
    DWORD wait, run = TRUE;

    while (run)
    {
        wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        switch (wait)
        {
        case WAIT_OBJECT_0:
            run = FALSE;
            break; // terminate

        case WAIT_OBJECT_0 + 1: // data ready
        {
            C__ClientServerInitData data = *((C__ClientServerInitData*)mapAddress);
            if (data.Version == TRACE_SERVER_VERSION - 1 || // klient vytvoril pipu a semafor, mame je prevzit
                data.Version == TRACE_SERVER_VERSION - 3)   // stary klient, nechame ho pripojit (ale bez __mtIgnoreAutoClear)
            {
                HANDLE readPipe = NULL;
                HANDLE pipeSemaphore = NULL;
                // ziskani handlu client procesu
                DWORD clientPID = data.ClientOrServerProcessId;
                HANDLE clientProcess = HANDLES_Q(OpenProcess(PROCESS_DUP_HANDLE, FALSE, clientPID));
                // ziskani handlu pipy a semaforu
                if (clientProcess != NULL &&
                    HANDLES(DuplicateHandle(clientProcess, data.HReadOrWritePipe, // client
                                            GetCurrentProcess(), &readPipe,       // server
                                            GENERIC_READ, FALSE, 0)) &&
                    HANDLES(DuplicateHandle(clientProcess, data.HPipeSemaphore,  // client
                                            GetCurrentProcess(), &pipeSemaphore, // server
                                            0, FALSE, DUPLICATE_SAME_ACCESS)))
                {
                    BOOL newProcess = IsReadPipeThreadForNewProcess(clientPID);
                    unsigned threadID;
                    CReadPipeData readPipeData;
                    readPipeData.MainWnd = mainWnd;
                    readPipeData.ReadPipe = readPipe;
                    readPipeData.PipeSemaphore = pipeSemaphore;
                    readPipeData.ProcessID = clientPID;
                    readPipeData.SendProcessConnected = newProcess && data.Version == TRACE_SERVER_VERSION - 1;
                    readPipeData.ShowSemaphoreErr = data.Version == TRACE_SERVER_VERSION - 1; // hlasime jen jednou a jen u novych klientu
                    // pokud dojde ke dvema connectum z jednoho procesu (napr. u POBu: Test a POB.dll),
                    // umyslne pridelime dva unikatni PIDy, aby fungovalo pojmenovani procesu
                    // v Trace Serveru, proste aby bylo videt, kdo tu hlasku poslal (napr. Test nebo POB.dll)
                    readPipeData.UniqueProcessID = readPipeData.StaticUniqueProcessID++;
                    ResetEvent(ContinueEvent);
                    HANDLE thread = (HANDLE)HANDLES(_beginthreadex(NULL, 1000,
                                                                   ReadPipeThreadF,
                                                                   &readPipeData,
                                                                   CREATE_SUSPENDED,
                                                                   &threadID));
                    if (thread != NULL)
                    {
                        readPipeData.Thread = thread; // dodame threadu jeho HANDLE
                        CReadPipeThreadInfo rpti;
                        rpti.ClientPID = clientPID;
                        rpti.Thread = thread;
                        ActiveReadPipeThreads.BlockArray();
                        ActiveReadPipeThreads.Add(rpti); // zaradime ho mezi aktivni
                        ActiveReadPipeThreads.UnBlockArray();
                        if (newProcess && data.Version == TRACE_SERVER_VERSION - 3) // stary server, jedeme bez __mtIgnoreAutoClear
                            SendMessage(mainWnd, WM_USER_PROCESS_CONNECTED, 0, 0);
                        ResumeThread(thread); // spustime readPipeThread

                        WaitForSingleObject(ContinueEvent, INFINITE);

                        *((BOOL*)mapAddress) = TRUE; // zapis vysledku
                    }
                    else
                    {
                        HANDLES(CloseHandle(readPipe));
                        HANDLES(CloseHandle(pipeSemaphore));
                        PostMessage(mainWnd, WM_USER_SHOWERROR,
                                    EC_CANNOT_CREATE_READ_PIPE_THREAD, 0);
                        *((BOOL*)mapAddress) = FALSE; // zapis vysledku
                    }
                }
                else
                {
                    if (readPipe != NULL)
                        HANDLES(CloseHandle(readPipe));
                    if (pipeSemaphore != NULL)
                        HANDLES(CloseHandle(pipeSemaphore));
                    *((BOOL*)mapAddress) = FALSE; // zapis vysledku -> nepovedlo se
                }
                if (clientProcess != NULL)
                    HANDLES(CloseHandle(clientProcess));
            }
            else
            {
                if (data.Version == TRACE_SERVER_VERSION ||   // mame vytvorit pipu a semafor a poslat je klientovi
                    data.Version == TRACE_SERVER_VERSION - 2) // stary klient, nechame ho pripojit (ale bez __mtIgnoreAutoClear)
                {
                    // pripravime "NULL PACL", tedy z hlediska prav naprosto otevreny deskriptor
                    // cizi proces tak muze naprilad nastavovat prava takto vytvorenych objektu
                    // v nasem pripade (TraceServer) nam to ale nevadi a zaroven je to jednodussi
                    char sd[SECURITY_DESCRIPTOR_MIN_LENGTH];
                    SECURITY_ATTRIBUTES sa;
                    sa.nLength = sizeof(sa);
                    sa.bInheritHandle = FALSE;
                    sa.lpSecurityDescriptor = &sd;
                    InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
                    // give the security descriptor a NULL DACL, done using the  "TRUE, (PACL)NULL" here
                    SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, 0, FALSE);
                    SECURITY_ATTRIBUTES* saPtr = &sa;

                    C__ClientServerInitData* dataWr = (C__ClientServerInitData*)mapAddress;
                    HANDLE pipeSemaphore = HANDLES(CreateSemaphore(saPtr, __PIPE_SIZE, __PIPE_SIZE, NULL));
                    HANDLE readPipe = NULL;
                    HANDLE writePipe = NULL;
                    if (pipeSemaphore != NULL && HANDLES(CreatePipe(&readPipe, &writePipe, saPtr, __PIPE_SIZE * 1024)))
                    {
                        // zapisu do sdilene pameti handle pro zapis do pipy (pro klienta)
                        dataWr->Version = TRUE;                                  // BOOL hodnota: TRUE = mame pipu
                        dataWr->ClientOrServerProcessId = GetCurrentProcessId(); // tady jde o PID serveru
                        dataWr->HReadOrWritePipe = writePipe;
                        dataWr->HPipeSemaphore = pipeSemaphore;

                        SetEvent(ConnectDataAcceptedEvent); // predani dat klientovi, vysledky jsou ulozeny
                        ConnectDataAcceptedEventMayBeSignaled = TRUE;

                        // pockam, az server data zpracuje
                        DWORD waitRet = WaitForSingleObject(ConnectDataReadyEvent, __COMMUNICATION_WAIT_TIMEOUT);
                        if (waitRet == WAIT_OBJECT_0 && dataWr->Version == 3 /* 3 = uspech, klient prevzal handly */) // podivam se na vysledek od klienta
                        {
                            DWORD clientPID = dataWr->ClientOrServerProcessId; /* PID klienta */
                            BOOL newProcess = IsReadPipeThreadForNewProcess(clientPID);
                            unsigned threadID;
                            CReadPipeData readPipeData;
                            readPipeData.MainWnd = mainWnd;
                            readPipeData.ReadPipe = readPipe;
                            readPipeData.PipeSemaphore = pipeSemaphore;
                            readPipeData.ProcessID = clientPID;
                            readPipeData.SendProcessConnected = newProcess && data.Version == TRACE_SERVER_VERSION;
                            readPipeData.ShowSemaphoreErr = data.Version == TRACE_SERVER_VERSION; // hlasime jen jednou a jen u novych klientu
                            // pokud dojde ke dvema connectum z jednoho procesu (napr. u POBu: Test a POB.dll),
                            // umyslne pridelime dva unikatni PIDy, aby fungovalo pojmenovani procesu
                            // v Trace Serveru, proste aby bylo videt, kdo tu hlasku poslal (napr. Test nebo POB.dll)
                            readPipeData.UniqueProcessID = readPipeData.StaticUniqueProcessID++;
                            ResetEvent(ContinueEvent);
                            HANDLE thread = (HANDLE)HANDLES(_beginthreadex(NULL, 1000,
                                                                           ReadPipeThreadF,
                                                                           &readPipeData,
                                                                           CREATE_SUSPENDED,
                                                                           &threadID));
                            if (thread != NULL)
                            {
                                readPipeData.Thread = thread; // dodame threadu jeho HANDLE
                                CReadPipeThreadInfo rpti;
                                rpti.ClientPID = clientPID;
                                rpti.Thread = thread;
                                ActiveReadPipeThreads.BlockArray();
                                ActiveReadPipeThreads.Add(rpti); // zaradime ho mezi aktivni
                                ActiveReadPipeThreads.UnBlockArray();
                                if (newProcess && data.Version == TRACE_SERVER_VERSION - 2) // stary server, jedeme bez __mtIgnoreAutoClear
                                    SendMessage(mainWnd, WM_USER_PROCESS_CONNECTED, 0, 0);
                                ResumeThread(thread); // spustime readPipeThread

                                WaitForSingleObject(ContinueEvent, INFINITE);
                                dataWr->Version = 2; // 2 = uspesne nastartovany thread, komunikace navazana!

                                readPipe = NULL; // vynulujeme tyto promenne, aby se nam handly nezavrely (uz se pouzivaji v threadu)
                                pipeSemaphore = NULL;
                            }
                            else
                            {
                                PostMessage(mainWnd, WM_USER_SHOWERROR, EC_CANNOT_CREATE_READ_PIPE_THREAD, 0);
                                dataWr->Version = FALSE; // BOOL hodnota: FALSE = hlasime neuspech, konec komunikace
                            }
                        }
                    }
                    else
                        dataWr->Version = FALSE; // BOOL hodnota: FALSE = hlasime neuspech, konec komunikace
                    if (readPipe != NULL)
                        HANDLES(CloseHandle(readPipe));
                    if (writePipe != NULL)
                        HANDLES(CloseHandle(writePipe));
                    if (pipeSemaphore != NULL)
                        HANDLES(CloseHandle(pipeSemaphore));
                }
                else
                {
                    *((BOOL*)mapAddress) = FALSE; // zapis vysledku -> nepovedlo se
                    PostMessage(mainWnd, WM_USER_INCORRECT_VERSION,
                                data.Version, data.ClientOrServerProcessId);
                }
            }
            SetEvent(ConnectDataAcceptedEvent); // konec akce, vysledek je ulozen
            ConnectDataAcceptedEventMayBeSignaled = TRUE;
            break;
        }
        }
    }
    //---  uvolneni sdilene pameti
    HANDLES(UnmapViewOfFile(mapAddress));
    HANDLES(CloseHandle(hFileMapping));
    _endthreadex(CT_SUCCESS);
    return CT_SUCCESS;
}

//*****************************************************************************
//
// CGlobalData
//

CGlobalData::CGlobalData()
    : Processes(10, 5), Threads(10, 5), MessagesCache(100, 50), Messages(1000, 500)
{
    MessagesFlushInProgress = FALSE;
    EditorConnected = FALSE;
}

CGlobalData::~CGlobalData()
{
    Processes.BlockArray();
    int count = Processes.GetCount();
    for (int i = 0; i < count; i++)
        free(Processes[i].Name);
    Processes.UnBlockArray();

    Threads.BlockArray();
    count = Threads.GetCount();
    for (int i = 0; i < count; i++)
        free(Threads[i].Name);
    Threads.UnBlockArray();

    MessagesCache.BlockArray();
    count = MessagesCache.GetCount();
    for (int i = 0; i < count; i++)
    { // Message je jen offset -> nedealokovat
        if (MessagesCache[i].File != NULL)
        {
            free(MessagesCache[i].File);
            MessagesCache[i].File = NULL;
        }
    }
    MessagesCache.UnBlockArray();

    for (int i = 0; i < Messages.Count; i++)
    {
        if (Messages[i].File != NULL)
        {
            free(Messages[i].File);
            Messages[i].File = NULL;
        }
    }
    /* jr - prechod na MSVC
  if (EditorConnected) EDITDisconnect();
*/
}

int CGlobalData::FindProcessNameIndex(DWORD uniqueProcessID)
{
    if (Processes.CroakIfNotBlocked())
        return -1;
    for (int i = 0; i < Processes.GetCount(); i++)
    {
        if (Processes[i].UniqueProcessID == uniqueProcessID)
            return i;
    }
    return -1;
}

int CGlobalData::FindThreadNameIndex(DWORD uniqueProcessID, DWORD uniqueThreadID)
{
    if (Threads.CroakIfNotBlocked())
        return -1;
    for (int i = 0; i < Threads.GetCount(); i++)
    {
        if (Threads[i].UniqueProcessID == uniqueProcessID && Threads[i].UniqueThreadID == uniqueThreadID)
            return i;
    }
    return -1;
}

void CGlobalData::GetProcessName(DWORD uniqueProcessID, WCHAR* buff, int buffLen)
{
    Processes.BlockArray();
    int index = FindProcessNameIndex(uniqueProcessID);
    wcsncpy_s(buff, buffLen, index != -1 ? Processes[index].Name : L"Unknown", buffLen - 1);
    Processes.UnBlockArray();
}

void CGlobalData::GetThreadName(DWORD uniqueProcessID,
                                DWORD uniqueThreadID, WCHAR* buff, int buffLen)
{
    Threads.BlockArray();
    int index = FindThreadNameIndex(uniqueProcessID, uniqueThreadID);
    wcsncpy_s(buff, buffLen, index != -1 ? Threads[index].Name : L"Unknown", buffLen - 1);
    Threads.UnBlockArray();
}

void CGlobalData::GotoEditor(int index)
{
    OpenFileInMSVC(Messages[index].File, Messages[index].Line);
}

//*****************************************************************************
//
// CGlobalDataMessage
//

BOOL CGlobalDataMessage::operator<(const CGlobalDataMessage& message)
{
    if (Counter == 0)
    {
        if (Time.wYear == message.Time.wYear)
        {
            if (Time.wMonth == message.Time.wMonth)
            {
                if (Time.wDay == message.Time.wDay)
                {
                    if (Time.wHour == message.Time.wHour)
                    {
                        if (Time.wMinute == message.Time.wMinute)
                        {
                            if (Time.wSecond == message.Time.wSecond)
                            {
                                if (Time.wMilliseconds == message.Time.wMilliseconds)
                                {
                                    return (Index < message.Index);
                                }
                                else
                                    return (Time.wMilliseconds < message.Time.wMilliseconds);
                            }
                            else
                                return (Time.wSecond < message.Time.wSecond);
                        }
                        else
                            return (Time.wMinute < message.Time.wMinute);
                    }
                    else
                        return (Time.wHour < message.Time.wHour);
                }
                else
                    return (Time.wDay < message.Time.wDay);
            }
            else
                return (Time.wMonth < message.Time.wMonth);
        }
        else
            return (Time.wYear < message.Time.wYear);
    }
    else
        return Counter < message.Counter;
}

//*****************************************************************************
//
// BuildFonts
//

BOOL BuildFonts()
{
    return TRUE;
}

void DeleteFonts()
{
}

//*****************************************************************************
//
// WinMain
//

int WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*cmdLine*/, int /*cmdShow*/)
{
    HInstance = hInstance;

    SetMessagesTitleW(MAINWINDOW_NAME);

    // u Trace Serveru nema smysl, loguje se jen do souboru...
    //SetTraceProcessNameW(MAINWINDOW_NAME);
    //SetTraceThreadNameW(L"Main");

    // nastavime lokalizovane hlasky do modulu ALLOCHAN (zajistuje pri nedostatku pameti hlaseni uzivateli + Retry button + kdyz vse selze tak i Cancel pro terminate softu)
    SetAllocHandlerMessage(NULL, MAINWINDOW_NAME, NULL, NULL);

    HWND hPrevWindow = FindWindow(WC_MAINWINDOW, MAINWINDOW_NAME);
    if (hPrevWindow != NULL)
    {
        if (IsIconic(hPrevWindow))
            ShowWindow(hPrevWindow, SW_RESTORE);
        ShowWindow(hPrevWindow, SW_SHOW);
        SetForegroundWindow(hPrevWindow);
        const char* msg = "Other instance of Trace Server is already running.";
        TRACE_I(msg);
        DMESSAGE_TI(msg, MB_OK);
        return 0;
    }

    TRACE_I("Begin.");

    WindowsVistaAndLater = TServerIsWindowsVersionOrGreater(6, 0, 0);

    // aby bylo mozne pod Vistou pristupovat na TServer z procesu beziciho pod jinym uzivatelem
    // (runas), bylo potreba povolit otevreni handlu procesu; povoluji zde tedy vse
    HANDLE hProcess = GetCurrentProcess();
    DWORD err = SetSecurityInfo(hProcess, SE_KERNEL_OBJECT,
                                DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                NULL, NULL, NULL, NULL);

    // pokud bezi na Viste TServer AsAdmin a snazime se na proces napojit z jineho uctu (Salamander spustenej
    // pomoci runas /user:test salamand.exe), nechtel se Salamander pripojit; kod jsem nasel tady:
    // http://www.vistax64.com/vista-security/72588-openprocess-process_set_information-protected-processes.html#post357171
    // Manik odkazuje na knizku http://www.amazon.com/gp/product/0470101555?ie=UTF8&tag=protectyourwi-20
    // (Windows Vista Security: Securing Vista Against Malicious Attacks )
    if (WindowsVistaAndLater)
    {
        TOKEN_PRIVILEGES tp;
        LUID luid;

        if (LookupPrivilegeValue(NULL,                // lookup privilege on local system
                                 L"SeDebugPrivilege", // privilege to lookup
                                 &luid))              // receives LUID of privilege
        {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            // Enable the privilege or disable all privileges.
            HANDLE currProc = GetCurrentProcess();
            HANDLE procToken;
            if (OpenProcessToken(currProc, TOKEN_ADJUST_PRIVILEGES, &procToken))
            {
                AdjustTokenPrivileges(procToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, NULL);
                CloseHandle(procToken);
            }
            CloseHandle(currProc);
        }
    }

    ConfigData.Register(Registry);
    Registry.Load();

    //--- inicializace knihovny
    InitializeWinLib();

    if (CWindow::RegisterUniversalClass(CS_DBLCLKS,
                                        0, 0, NULL, NULL, NULL,
                                        NULL, WC_TABLIST, NULL))
    {
        HICON hIcon = LoadIcon(HInstance, MAKEINTRESOURCE(IC_TSERVER_1));
        if (CWindow::RegisterUniversalClass(CS_DBLCLKS,
                                            0, 0, hIcon, NULL, NULL,
                                            NULL, WC_MAINWINDOW, NULL))
        {
            UseMaxMessagesCount = ConfigData.UseMaxMessagesCount;
            MaxMessagesCount = min(1000000, max(100, ConfigData.MaxMessagesCount));
            if (BuildFonts())
            {
                MainWindow = new CMainWindow;
                if (MainWindow != NULL)
                {
                    HMENU hMainMenu = LoadMenu(HInstance, MAKEINTRESOURCE(IDM_MAIN));
                    DWORD exStyle;
                    exStyle = ConfigData.UseToolbarCaption ? WS_EX_TOOLWINDOW : 0;
                    if (MainWindow->CreateEx(exStyle,
                                             WC_MAINWINDOW,
                                             MAINWINDOW_NAME,
                                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                             CW_USEDEFAULT,
                                             CW_USEDEFAULT,
                                             CW_USEDEFAULT,
                                             CW_USEDEFAULT,
                                             NULL,
                                             hMainMenu,
                                             HInstance,
                                             MainWindow))
                    {
                        if (MainWindow->TaskBarAddIcon())
                        {
                            if (ConfigData.AlwaysOnTop)
                                SetWindowPos(MainWindow->HWindow, HWND_TOPMOST, 0, 0, 0, 0,
                                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREPOSITION);

                            if (ConfigData.MainWindowPlacement.length != 0)
                            {
                                if (ConfigData.UseToolbarCaption && ConfigData.MainWindowHidden)
                                    ConfigData.MainWindowPlacement.showCmd = SW_HIDE;
                                SetWindowPlacement(MainWindow->HWindow, &ConfigData.MainWindowPlacement);
                            }
                            else
                            {
                                // konfigurace v Registry neexistuje, pouzijeme defaulty
                                ShowWindow(MainWindow->HWindow, ConfigData.MainWindowHidden ? SW_HIDE : SW_SHOW);
                            }
                            /*
              WINDOWPLACEMENT wp;
              GetWindowPlacement(MainWindow->HWindow, &wp);
              if (ConfigData.MainWindowPlacement.length != 0)
                if (ConfigData.UseToolbarCaption && ConfigData.MainWindowHidden)
                  wp.showCmd = SW_HIDE;
              wp.rcNormalPosition.right++;
              SetWindowPlacement(MainWindow->HWindow, &wp);
              wp.rcNormalPosition.right--;
              SetWindowPlacement(MainWindow->HWindow, &wp);
*/
                            SetMessagesParent(MainWindow->HWindow);

                            if (InitializeServer(MainWindow->HWindow))
                            {
                                //--- aplikacni smycka
                                MSG msg;
                                while (GetMessage(&msg, NULL, 0, 0))
                                {
                                    CWindowsObject* wnd = WindowsManager.GetWindowPtr(GetActiveWindow());
                                    if (wnd == NULL || !wnd->Is(otDialog) || !IsDialogMessage(wnd->HWindow, &msg))
                                    {
                                        TranslateMessage(&msg);
                                        DispatchMessage(&msg);
                                    }
                                }

                                // ulozim config
                                Registry.Save();

                                ReleaseServer();
                            }
                        }
                        else
                        {
                            delete MainWindow;
                            MainWindow = NULL;
                        }
                    }
                    else
                    {
                        delete MainWindow;
                        MainWindow = NULL;
                    }
                }
                else
                {
                    TRACE_EW(L"Out of memory.");
                }
                DeleteFonts();
            }
            else
            {
                TRACE_EW(L"Font creation failed.");
            }
        }
    }
    ReleaseWinLib();
    TRACE_I("End.");
    return 0;
}
