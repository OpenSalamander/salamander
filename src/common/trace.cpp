// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <crtdbg.h>
#include <tchar.h>

#if defined(__TRACESERVER) || defined(TRACE_ENABLE)

// The order here is important.
// Section names must be 8 characters or less.
// The sections with the same name before the $
// are merged into one section. The order that
// they are merged is determined by sorting
// the characters after the $.
// i_trace and i_trace_end are used to set
// boundaries so we can find the real functions
// that we need to call for initialization.

#pragma warning(disable : 4075) // we want to define the order of module initialization

typedef void(__cdecl* _PVFV)(void);

#pragma section(".i_trc$a", read)
__declspec(allocate(".i_trc$a")) const _PVFV i_trace = (_PVFV)1; // at the beginning of the section, we will declare a variable i_trace for ourselves

#pragma section(".i_trc$z", read)
__declspec(allocate(".i_trc$z")) const _PVFV i_trace_end = (_PVFV)1; // and at the end of the section .i_trc we will use the variable i_trace_end

void Initialize__Trace()
{
    const _PVFV* x = &i_trace;
    for (++x; x < &i_trace_end; ++x)
        if (*x != NULL)
            (*x)();
}

#pragma init_seg(".i_trc$m")

const TCHAR* __FILE_MAPPING_NAME = _T("TraceServerMappingName");
const TCHAR* __OPEN_CONNECTION_MUTEX = _T("TraceServerOpenConnectionMutex");
const TCHAR* __CONNECT_DATA_READY_EVENT_NAME = _T("TraceServerConnectDataReadyEvent");
const TCHAR* __CONNECT_DATA_ACCEPTED_EVENT_NAME = _T("TraceServerConnectDataAcceptedEvent");

#endif // defined(__TRACESERVER) || defined(TRACE_ENABLE)

#ifdef TRACE_ENABLE

#include <ostream>
#include <streambuf>
#include <stdio.h>
#if defined(__HEADER_TRACE_H) && !defined(_INC_PROCESS)
#error "Your precomp.h includes trace.h, so it must also earlier include process.h."
#endif // defined(__HEADER_TRACE_H) && !defined(_INC_PROCESS)
#include <process.h>
#ifdef _DEBUG
#include <sstream>
#endif // _DEBUG

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "trace.h"

#ifdef MULTITHREADED_TRACE_ENABLE

#undef _beginthreadex
#undef CreateThread

#endif // MULTITHREADED_TRACE_ENABLE

C__Trace __Trace;

#ifdef MULTITHREADED_TRACE_ENABLE

//*****************************************************************************
//
// C__TraceThreadCache
//

#define __TRACE_CACHE_DELTA 16

C__TraceThreadCache::C__TraceThreadCache()
{
    UniqueThreadID = 0;
    Available = Count = 0;
    Data = (C__TraceCacheData*)GlobalAlloc(GMEM_FIXED, __TRACE_CACHE_DELTA *
                                                           sizeof(C__TraceCacheData));
    if (Data != NULL)
        Available = __TRACE_CACHE_DELTA;

    for (int i = 0; i < __TRACE_CACHE_SIZE; i++)
        CacheUID[i] = -1;
}

C__TraceThreadCache::~C__TraceThreadCache()
{
    if (Data != NULL)
    {
        for (int i = 0; i < Count; i++)
            CloseHandle(Data[i].Handle);
        GlobalFree((HGLOBAL)Data);
        Data = NULL;
        Count = 0;
        Available = 0;
    }
}

BOOL C__TraceThreadCache::EnlargeArray()
{
    if (Data == NULL)
        return FALSE;
    C__TraceCacheData* New = (C__TraceCacheData*)GlobalReAlloc((HGLOBAL)Data,
                                                               (Available + __TRACE_CACHE_DELTA) *
                                                                   sizeof(C__TraceCacheData),
                                                               GMEM_MOVEABLE);
    if (New == NULL)
        return FALSE;
    else
    {
        Data = New;
        Available += __TRACE_CACHE_DELTA;
        return TRUE;
    }
}

BOOL C__TraceThreadCache::Move(int direction, DWORD first, DWORD count)
{
    if (count == 0)
    {
        if (direction == 1 && Available == Count)
            return EnlargeArray();
        return TRUE;
    }
    if (direction == 1) // down
    {
        if (Available == Count && !EnlargeArray())
            return FALSE;
        memmove(Data + first + 1, Data + first, count * sizeof(C__TraceCacheData));
    }
    else // Up
        memmove(Data + first - 1, Data + first, count * sizeof(C__TraceCacheData));
    return TRUE;
}

BOOL C__TraceThreadCache::GetIndex(DWORD tid, int& index)
{
    if (Count == 0)
    {
        index = 0;
        return FALSE;
    }

    int l = 0, r = Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        DWORD hw = Data[m].TID;
        if (hw == tid) // found
        {
            index = m;
            return TRUE;
        }
        else if (hw > tid)
        {
            if (l == r || l > m - 1) // not found
            {
                index = m; // should be at this position
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // not found
            {
                index = m + 1; // should be after this position
                return FALSE;
            }
            l = m + 1;
        }
    }
}

BOOL C__TraceThreadCache::Add(HANDLE handle, DWORD tid)
{
    int index;
    BOOL found = GetIndex(tid, index);
    if (!found)
    {
        if (Available == Count) // it's full, we'll throw away dead threads
        {
            DWORD code;
            for (int i = Count - 1; i >= 0; i--)
            {
                if (!GetExitCodeThread(Data[i].Handle, &code) || code != STILL_ACTIVE)
                {
                    DWORD id = Data[i].TID;                         // update cache:
                    if (CacheUID[__TraceCacheGetIndex(id)] != -1 && // valid record
                        CacheTID[__TraceCacheGetIndex(id)] == id)   // identical TID
                    {
                        CacheUID[__TraceCacheGetIndex(id)] = -1; // invalidation
                    }

                    CloseHandle(Data[i].Handle);
                    Move(0, i + 1, Count - i - 1);
                    Count--;

                    if (index > i)
                        index--;
                }
            }
        }
        if (Available == Count && !EnlargeArray())
            return FALSE;

        Move(1, index, Count - index); // insert new record
        Data[index].Handle = handle;
        Data[index].TID = tid;
        Data[index].UID = UniqueThreadID;
        Count++;
    }
    else
    {
        CloseHandle(Data[index].Handle);
        Data[index].Handle = handle;
        Data[index].UID = UniqueThreadID;
    }
    // update cache
    CacheTID[__TraceCacheGetIndex(tid)] = tid;
    CacheUID[__TraceCacheGetIndex(tid)] = UniqueThreadID++;

    return TRUE;
}

DWORD
C__TraceThreadCache::GetUniqueThreadId(DWORD tid)
{
    if (CacheUID[__TraceCacheGetIndex(tid)] != -1 && // if the record is valid
        CacheTID[__TraceCacheGetIndex(tid)] == tid)  // and if it is identical to tid
    {
        return CacheUID[__TraceCacheGetIndex(tid)]; // uid is in cache
    }

    int index;
    if (GetIndex(tid, index))
    {
        CacheTID[__TraceCacheGetIndex(tid)] = Data[index].TID;
        CacheUID[__TraceCacheGetIndex(tid)] = Data[index].UID;
        return Data[index].UID;
    }
    else
        return -1; // not found -> this should not happen
}

//*****************************************************************************
//
// __TRACECreateThread
//

HANDLE __TRACECreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                           DWORD dwStackSize,
                           LPTHREAD_START_ROUTINE lpStartAddress,
                           LPVOID lpParameter, DWORD dwCreationFlags,
                           LPDWORD lpThreadId)
{
    DWORD tid;
    HANDLE ret = CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress,
                              lpParameter, dwCreationFlags | CREATE_SUSPENDED, &tid);
    if (ret != NULL)
    {
        HANDLE handle;
        if (DuplicateHandle(GetCurrentProcess(), ret, GetCurrentProcess(),
                            &handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
        {
            EnterCriticalSection(&__Trace.CriticalSection);
            if (!__Trace.ThreadCache.Add(handle, tid))
                CloseHandle(handle);
            LeaveCriticalSection(&__Trace.CriticalSection);
        }
        if ((dwCreationFlags & CREATE_SUSPENDED) == 0)
            ResumeThread(ret);
    }
    if (lpThreadId != NULL)
        *lpThreadId = tid;
    return ret;
}

//*****************************************************************************
//
// __TRACE_beginthreadex
//

uintptr_t __TRACE_beginthreadex(void* security, unsigned stack_size,
                                unsigned(__stdcall* start_address)(void*),
                                void* arglist, unsigned initflag,
                                unsigned* thrdid)
{
    unsigned tid;
    uintptr_t ret = _beginthreadex(security, stack_size, start_address,
                                   arglist, initflag | CREATE_SUSPENDED, &tid);
    if (ret != NULL)
    {
        HANDLE handle;
        if (DuplicateHandle(GetCurrentProcess(), (HANDLE)ret, GetCurrentProcess(),
                            &handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
        {
            EnterCriticalSection(&__Trace.CriticalSection);
            if (!__Trace.ThreadCache.Add(handle, tid))
                CloseHandle(handle);
            LeaveCriticalSection(&__Trace.CriticalSection);
            if ((initflag & CREATE_SUSPENDED) == 0)
                ResumeThread((HANDLE)ret);
        }
    }
    if (thrdid != NULL)
        *thrdid = tid;
    return ret;
}

#endif // MULTITHREADED_TRACE_ENABLE

// ****************************************************************************
//
// CWStr
//

CWStr::CWStr(const char* s)
{
    IsOK = TRUE;
    Str = NULL;
    if (s == NULL)
        AllocBuf = NULL;
    else
    {
        IsOK = FALSE;
        int len = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
        if (len == 0)
            AllocBuf = NULL; // MultiByteToWideChar failed
        else
        {
            AllocBuf = (WCHAR*)malloc(len * sizeof(WCHAR));
            if (AllocBuf != NULL)
            {
                int res = MultiByteToWideChar(CP_ACP, 0, s, -1, AllocBuf, len);
                if (res > 0 && res <= len)
                {
                    AllocBuf[res - 1] = 0; // success, ensure zero terminated string
                    IsOK = TRUE;
                }
                else // MultiByteToWideChar failed
                {
                    free(AllocBuf);
                    AllocBuf = NULL;
                }
            }
        }
    }
}

//*****************************************************************************
//
// C__Trace
//

C__Trace::C__Trace() : TraceStrStream(&TraceStringBuf), TraceStrStreamW(&TraceStringBufW)
{
#ifdef _DEBUG
    // new streams internally use locales that are implemented
    // individual "facets" using lazy creation - are allocated on the heap
    // when needed, so when someone sends something to the stream that has
    // Formatting dependent on locale rules, such as number, date,
    // or boolean. These "facets" are deallocated on exit
    // program with compiler priority, i.e. after our memory leak check.
    // So if someone uses a stream to output anything localizable,
    // our debug heap starts reporting memory leaks even though there are none. In order to
    // the previous one, we force locales to create all "facets" now, until
    // we are not yet guarding the heap.
    // Currently we are only using the output stream and only with strings (without conversion)
    // a cisly. Takze poslat cislo do stringstreamu by melo stacit. Pokud
    // In the future, we will start using streams more and the debug heap will start reporting
    // leaky, we will need to add more input/output here.
    std::stringstream s;
    s << 1;
    std::wstringstream s2;
    s2 << 1;
#endif // _DEBUG

    InitializeCriticalSection(&CriticalSection);
    HWritePipe = NULL;
    HPipeSemaphore = NULL;
    BytesAllocatedForWriteToPipe = 0;
#ifdef TRACE_TO_FILE
    HTraceFile = NULL;
#ifdef __TRACESERVER
    TraceFileName[0] = 0;
#endif // __TRACESERVER
#endif // TRACE_TO_FILE
    ::QueryPerformanceFrequency(&PerformanceFrequency);
    SupportPerformanceFrequency = (PerformanceFrequency.QuadPart != 0);
    if (SupportPerformanceFrequency)
        ::QueryPerformanceCounter(&StartPerformanceCounter);
    else
        StartPerformanceCounter.QuadPart = 0;

#ifdef MULTITHREADED_TRACE_ENABLE
    HANDLE handle;
    if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                        GetCurrentProcess(), &handle,
                        0, FALSE, DUPLICATE_SAME_ACCESS) &&
        !ThreadCache.Add(handle, GetCurrentThreadId()))
    {
        CloseHandle(handle);
    }
#endif // MULTITHREADED_TRACE_ENABLE

    Connect(FALSE);
}

C__Trace::~C__Trace()
{
    Disconnect();
    DeleteCriticalSection(&CriticalSection);
}

BOOL C__Trace::Connect(BOOL onUserRequest)
{
    EnterCriticalSection(&CriticalSection);
    DWORD storedLastError = GetLastError();

#ifdef TRACE_TO_FILE
    if (HTraceFile == NULL)
    {
        WCHAR tmpDir[MAX_PATH + 10];
        WCHAR* end = tmpDir + MAX_PATH + 10;
        if (GetTempPathW(MAX_PATH, tmpDir))
        {
            WCHAR* s = tmpDir + wcslen(tmpDir);
            if (s > tmpDir && *(s - 1) != L'\\')
                *s++ = L'\\';
            lstrcpynW(s, L"altap_traces", int(end - s));
            s += wcslen(s);

            if ((s - tmpDir) + 15 < MAX_PATH) // enough space for the connection "_2000000000.log"
            {
                int num = 1;
                while (1)
                {
                    swprintf_s(s, _countof(tmpDir) - (s - tmpDir), L"_%d.log", num++);
                    HTraceFile = CreateFileW(tmpDir, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (HTraceFile != INVALID_HANDLE_VALUE)
                    {
#ifdef __TRACESERVER
                        lstrcpynW(TraceFileName, tmpDir, _countof(TraceFileName));
#endif // __TRACESERVER
                        break;
                    }
                    DWORD err = GetLastError();
                    if (err != ERROR_FILE_EXISTS && err != ERROR_ALREADY_EXISTS)
                        break; // unexpected error (and there is nowhere to output it)
                }
                if (HTraceFile == INVALID_HANDLE_VALUE)
                    HTraceFile = NULL;
                else // write the header to the file (column identification)
                {
                    DWORD wr;
                    const WCHAR* fileHeader = L"\xFEFF" /* BOM*/ L"Type\tTID\t"
#ifdef MULTITHREADED_TRACE_ENABLE
                                              L"UTID\t"
#endif // MULTITHREADED_TRACE_ENABLE
                                              L"Date\tTime\tCounter [ms]\tModule\tLine\tMessage\r\n";
                    WriteFile(HTraceFile, fileHeader, (int)(sizeof(WCHAR) * wcslen(fileHeader)), &wr, NULL);
                    TRACE_I("Opening log file" << (onUserRequest ? " on user's request." : "."));
                }
            }
        }
    }
#endif // TRACE_TO_FILE

    if (HWritePipe != NULL) // test connection to the server, if it is down, HWritePipe will be closed and then we will try to reconnect
        TRACE_I("Connect request received when already connected to Trace Server.");

    BOOL ret = FALSE;
    if (HWritePipe != NULL)
        ret = TRUE; // if the connection is already established
    else
    {
        // trying to open Mutex for accessing shared memory
        HANDLE hOpenConnectionMutex;
        hOpenConnectionMutex = OpenMutex(/*MUTEX_ALL_ACCESS*/ SYNCHRONIZE, FALSE, __OPEN_CONNECTION_MUTEX);
        if (hOpenConnectionMutex != NULL) // server found
        {
            // acquire ConnectionMutex
            DWORD waitRet;
            while (1)
            {
                waitRet = WaitForSingleObject(hOpenConnectionMutex,
                                              __COMMUNICATION_WAIT_TIMEOUT);
                if (waitRet != WAIT_ABANDONED)
                    break;
            }
            if (waitRet == WAIT_OBJECT_0) // succeeded in taking over
            {
                // open FileMapping
                HANDLE hFileMapping;
                hFileMapping = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, __FILE_MAPPING_NAME);
                if (hFileMapping != NULL)
                {
                    // map the shared memory
                    char* mapAddress;
                    mapAddress = (char*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, // FIXME_X64 are we passing x86/x64 incompatible data?
                                                      0, 0, __SIZEOF_CLIENTSERVERINITDATA);
                    if (mapAddress != NULL)
                    {
                        BytesAllocatedForWriteToPipe = 0;
                        HPipeSemaphore = CreateSemaphore(NULL, __PIPE_SIZE, __PIPE_SIZE, NULL);
                        if (HPipeSemaphore != NULL)
                        {
                            HANDLE HReadPipe;

                            // create an anonymous pipe
                            SECURITY_ATTRIBUTES sa;
                            char secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
                            sa.nLength = sizeof(sa);
                            sa.bInheritHandle = FALSE;
                            sa.lpSecurityDescriptor = &secDesc;
                            InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
                            // give the security descriptor a NULL DACL, done using the "TRUE, (PACL)NULL" here
                            SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, 0, FALSE);
                            if (CreatePipe(&HReadPipe, &HWritePipe, &sa, __PIPE_SIZE * 1024))
                            {
                                // write to shared memory handle for reading from pipe
                                int expectedServerVer = TRACE_CLIENT_VERSION;
                                *(int*)&mapAddress[0] = expectedServerVer - 1;               // Version (trying the older way of connecting first)
                                *(DWORD*)&mapAddress[4] = GetCurrentProcessId();             // ClientOrServerProcessId (here is the PID of the client)
                                *(DWORD*)&mapAddress[8] = (DWORD)(DWORD_PTR)HReadPipe;       // HReadOrWritePipe (this is about HReadPipe here)
                                *(DWORD*)&mapAddress[12] = (DWORD)(DWORD_PTR)HPipeSemaphore; // HPipeSemaphore

                                // Open hReadyEvent
                                HANDLE hReadyEvent;
                                hReadyEvent = OpenEvent(EVENT_ALL_ACCESS, TRUE, __CONNECT_DATA_READY_EVENT_NAME);
                                // open the hAcceptedEvent
                                HANDLE hAcceptedEvent;
                                hAcceptedEvent = OpenEvent(EVENT_ALL_ACCESS, TRUE, __CONNECT_DATA_ACCEPTED_EVENT_NAME);
                                if (hReadyEvent != NULL && hAcceptedEvent != NULL)
                                {
                                    ResetEvent(hAcceptedEvent); // I just want a fresh response from the server

                                    while (1)
                                    {
                                        SetEvent(hReadyEvent); // I will tell the server that I have prepared the data

                                        // Wait until the server processes the data
                                        waitRet = WaitForSingleObject(hAcceptedEvent, __COMMUNICATION_WAIT_TIMEOUT);
                                        if (waitRet == WAIT_OBJECT_0)
                                        {
                                            // message printed on the old non-Unicode Trace Server (must be ANSI)
                                            const char* oldTraceServerA = "Disconnecting: this is not Unicode version of Trace Server.";

                                            // Check the result from the server
                                            if (*((BOOL*)mapAddress) == TRUE)
                                            {
                                                if (expectedServerVer == TRACE_CLIENT_VERSION) // Hooray, successfully connected to the new Trace Server!
                                                {
#ifdef TRACE_IGNORE_AUTOCLEAR
                                                    ret = SendIgnoreAutoClear(TRUE); // ignore, in case of error we will disconnect
#else                                                                                // TRACE_IGNORE_AUTOCLEAR
                                                    ret = SendIgnoreAutoClear(FALSE);         // Do not ignore, in case of an error we will disconnect
#endif                                                                               // TRACE_IGNORE_AUTOCLEAR
                                                }
                                                else
                                                    TRACE_E(oldTraceServerA);
                                            }
                                            else // Failed: let's try to create a pipe on the server side
                                            {
                                                // Write a new version to shared memory, thus requesting the server to send a handle for writing to the pipe
                                                *(int*)&mapAddress[0] = expectedServerVer; // Version

                                                SetEvent(hReadyEvent); // I will tell the server that I have prepared the data

                                                // Wait until the server processes the data
                                                waitRet = WaitForSingleObject(hAcceptedEvent, __COMMUNICATION_WAIT_TIMEOUT);
                                                if (waitRet == WAIT_OBJECT_0 && *((BOOL*)mapAddress) == TRUE) // Check the result from the server
                                                {
                                                    HANDLE hWritePipeFromSrv = NULL;
                                                    HANDLE hPipeSemaphoreFromSrv = NULL;

                                                    // acquiring trade server process
                                                    HANDLE hServerProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE,
                                                                                        *(DWORD*)&mapAddress[4] /* ClientOrServerProcessId (here is the PID of the server)*/);
                                                    // acquiring trade of pipes and semaphore
                                                    if (hServerProcess != NULL &&
                                                        DuplicateHandle(hServerProcess, (HANDLE)(DWORD_PTR)(*(DWORD*)&mapAddress[8]) /* HReadOrWritePipe (here it's about HWritePipe)*/, // server
                                                                        GetCurrentProcess(), &hWritePipeFromSrv,                                                                     // client
                                                                        GENERIC_WRITE, FALSE, 0) &&
                                                        DuplicateHandle(hServerProcess, (HANDLE)(DWORD_PTR)(*(DWORD*)&mapAddress[12]) /* HPipeSemaphore*/, // server
                                                                        GetCurrentProcess(), &hPipeSemaphoreFromSrv,                                        // client
                                                                        0, FALSE, DUPLICATE_SAME_ACCESS))
                                                    {
                                                        *((int*)mapAddress) = 3;                         // write result -> 3 = success, we have handles
                                                        *(DWORD*)&mapAddress[4] = GetCurrentProcessId(); // ClientOrServerProcessId (here is the PID of the client)
                                                    }
                                                    else
                                                    {
                                                        *((BOOL*)mapAddress) = FALSE; // write result -> failed
                                                    }
                                                    if (hServerProcess != NULL)
                                                        CloseHandle(hServerProcess);

                                                    SetEvent(hReadyEvent); // I will tell the server that I have read the data and written the result

                                                    // upon success: wait for the server to start the thread reading data from the pipe and send the result
                                                    // in case of failure: we inform the server that it failed, it returns failure to us again
                                                    waitRet = WaitForSingleObject(hAcceptedEvent, __COMMUNICATION_WAIT_TIMEOUT);
                                                    if (waitRet == WAIT_OBJECT_0 && // Check the result from the server
                                                        *((int*)mapAddress) == 2 /* 2 = successfully started reading thread in the server*/)
                                                    {
                                                        CloseHandle(HPipeSemaphore);
                                                        HPipeSemaphore = hPipeSemaphoreFromSrv; // we will use the semaphore from the server (we will close the client one)

                                                        CloseHandle(HWritePipe);
                                                        HWritePipe = hWritePipeFromSrv; // We will use the pipe from the server (we will close the client one).

                                                        if (expectedServerVer == TRACE_CLIENT_VERSION) // Hooray, successfully connected to the new Trace Server!
                                                        {
#ifdef TRACE_IGNORE_AUTOCLEAR
                                                            ret = SendIgnoreAutoClear(TRUE); // ignore, in case of error we will disconnect
#else                                                                                        // TRACE_IGNORE_AUTOCLEAR
                                                            ret = SendIgnoreAutoClear(FALSE); // Do not ignore, in case of an error we will disconnect
#endif                                                                                       // TRACE_IGNORE_AUTOCLEAR
                                                        }
                                                        else
                                                            TRACE_E(oldTraceServerA);
                                                    }
                                                    else
                                                    {
                                                        if (hWritePipeFromSrv != NULL)
                                                            CloseHandle(hWritePipeFromSrv);
                                                        if (hPipeSemaphoreFromSrv != NULL)
                                                            CloseHandle(hPipeSemaphoreFromSrv);
                                                    }
                                                }
                                                else // connect failed in any way (probably an old Trace Server)
                                                {
                                                    if (expectedServerVer == TRACE_CLIENT_VERSION) // we tried a new version of the server
                                                    {
                                                        expectedServerVer = TRACE_CLIENT_VERSION - 2; // Now we will try an older version of the server
                                                        // write to shared memory a version that can be handled by an old version of the server
                                                        *(int*)&mapAddress[0] = expectedServerVer - 1; // Version
                                                        continue;
                                                    }
                                                }
                                            }
                                        }
                                        break;
                                    }
                                }
                                if (hReadyEvent != NULL)
                                    CloseHandle(hReadyEvent);
                                if (hAcceptedEvent != NULL)
                                    CloseHandle(hAcceptedEvent);
                                if (!ret)
                                {
                                    CloseHandle(HWritePipe);
                                    HWritePipe = NULL;
                                }
                                CloseHandle(HReadPipe);
                            }
                            if (!ret)
                            {
                                CloseHandle(HPipeSemaphore);
                                HPipeSemaphore = NULL;
                            }
                        }
                        UnmapViewOfFile(mapAddress);
                    }
                    CloseHandle(hFileMapping);
                }
                ReleaseMutex(hOpenConnectionMutex); // other clients can connect
            }
            CloseHandle(hOpenConnectionMutex);
        }
        if (ret)
        {
            TRACE_I("Connected" << (onUserRequest ? " on user's request." : "."));
#ifdef TRACE_TO_FILE
            if (HTraceFile != NULL)
                TRACE_I("TRACE MESSAGES ARE ALSO WRITTEN TO FILE IN TEMP DIRECTORY.");
#endif // TRACE_TO_FILE
        }
    }
    SetLastError(storedLastError);
    LeaveCriticalSection(&CriticalSection);
    return ret;
}

void C__Trace::Disconnect()
{
    EnterCriticalSection(&CriticalSection);
    DWORD storedLastError = GetLastError();
    if (HWritePipe != NULL)
    {
        TRACE_I("Disconnected.");
        CloseWritePipeAndSemaphore();
    }
#ifdef TRACE_TO_FILE
    if (HTraceFile != NULL)
    {
        TRACE_I("Closing log file.");
        CloseHandle(HTraceFile);
        HTraceFile = NULL;
#ifdef __TRACESERVER
        TraceFileName[0] = 0;
#endif // __TRACESERVER
    }
#endif // TRACE_TO_FILE
    SetLastError(storedLastError);
    LeaveCriticalSection(&CriticalSection);
}

#ifdef TRACE_TO_FILE
void C__Trace::CloseTraceFile()
{
    EnterCriticalSection(&CriticalSection);
    DWORD storedLastError = GetLastError();
    if (HTraceFile != NULL)
    {
        TRACE_I("Closing log file on user's request.");
        CloseHandle(HTraceFile);
        HTraceFile = NULL;
#ifdef __TRACESERVER
        TraceFileName[0] = 0;
#endif // __TRACESERVER
    }
    SetLastError(storedLastError);
    LeaveCriticalSection(&CriticalSection);
}
#endif // TRACE_TO_FILE

BOOL C__Trace::WritePipe(LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite)
{
    DWORD numberOfBytesWritten = 0;
    while (BytesAllocatedForWriteToPipe < nNumberOfBytesToWrite)
    {
        DWORD res = WaitForSingleObject(HPipeSemaphore, 500);
        if (res == WAIT_OBJECT_0)
            BytesAllocatedForWriteToPipe += 1024;
        else
        {
            if (res == WAIT_TIMEOUT) // timeout, check if the pipe to the server is still alive
            {
                if (!WriteFile(HWritePipe, lpBuffer, 0, &numberOfBytesWritten, NULL))
                    return FALSE;
            }
            else // Another error, let's finish it rather, so there is no infinite loop
                return FALSE;
        }
    }
    if (WriteFile(HWritePipe, lpBuffer, nNumberOfBytesToWrite, &numberOfBytesWritten, NULL) &&
        numberOfBytesWritten == nNumberOfBytesToWrite)
    {
        BytesAllocatedForWriteToPipe -= nNumberOfBytesToWrite;
        return TRUE;
    }
    return FALSE;
}

void C__Trace::CloseWritePipeAndSemaphore()
{
    if (HWritePipe != NULL)
        CloseHandle(HWritePipe);
    HWritePipe = NULL;
    if (HPipeSemaphore != NULL)
        CloseHandle(HPipeSemaphore);
    HPipeSemaphore = NULL;
}

void C__Trace::SendSetNameMessageToServer(const char* name, const WCHAR* nameW, C__MessageType type)
{
    if (HWritePipe != NULL)
    {
        BOOL unicode = (type == __mtSetProcessNameW || type == __mtSetThreadNameW);
        char data[__SIZEOF_PIPEDATAHEADER];
        *(int*)&data[0] = type;                   // Type
        *(DWORD*)&data[4] = GetCurrentThreadId(); // ThreadID
#ifdef MULTITHREADED_TRACE_ENABLE                 // UniqueThreadID
        *(DWORD*)&data[8] = ThreadCache.GetUniqueThreadId(*(DWORD*)&data[4]);
#else                                                                                      // MULTITHREADED_TRACE_ENABLE
        *(DWORD*)&data[8] = *(DWORD*)&data[4];
#endif                                                                                     // MULTITHREADED_TRACE_ENABLE
        memset(data + 12, 0, 16);                                                          // Time
        *(DWORD*)&data[28] = unicode ? (DWORD)wcslen(nameW) + 1 : (DWORD)strlen(name) + 1; // MessageSize
        *(DWORD*)&data[32] = 0;                                                            // MessageTextOffset
        *(DWORD*)&data[36] = 0;                                                            // Line
        *(double*)&data[40] = 0.0;                                                         // Counter

        if (!WritePipe(data, __SIZEOF_PIPEDATAHEADER) ||
            !WritePipe(unicode ? (void*)nameW : (void*)name, (unicode ? sizeof(WCHAR) : 1) * (*(DWORD*)&data[28])))
        {
            CloseWritePipeAndSemaphore();
        }
    }
}

BOOL C__Trace::SendIgnoreAutoClear(BOOL ignore)
{
    char data[__SIZEOF_PIPEDATAHEADER];
    *(int*)&data[0] = __mtIgnoreAutoClear; // Type
    *(DWORD*)&data[4] = ignore ? 1 : 0;    // ThreadID: 0 = do not ignore, 1 = ignore auto-clear on Trace Server
    return WritePipe(data, __SIZEOF_PIPEDATAHEADER);
}

void C__Trace::SetProcessName(const char* name)
{
    EnterCriticalSection(&CriticalSection);
    DWORD storedLastError = GetLastError();
    SendSetNameMessageToServer(name, NULL, __mtSetProcessName);
    SetLastError(storedLastError);
    LeaveCriticalSection(&CriticalSection);
}

void C__Trace::SetProcessNameW(const WCHAR* name)
{
    EnterCriticalSection(&CriticalSection);
    DWORD storedLastError = GetLastError();
    SendSetNameMessageToServer(NULL, name, __mtSetProcessNameW);
    SetLastError(storedLastError);
    LeaveCriticalSection(&CriticalSection);
}

void C__Trace::SetThreadName(const char* name)
{
    EnterCriticalSection(&CriticalSection);
    DWORD storedLastError = GetLastError();
#ifdef MULTITHREADED_TRACE_ENABLE
    if (ThreadCache.GetUniqueThreadId(GetCurrentThreadId()) != -1) // only with assigned UID, otherwise all "unknown" would be suddenly named with this name
        SendSetNameMessageToServer(name, NULL, __mtSetThreadName);
#else  // MULTITHREADED_TRACE_ENABLE
    SendSetNameMessageToServer(name, NULL, __mtSetThreadName);
#endif // MULTITHREADED_TRACE_ENABLE
    SetLastError(storedLastError);
    LeaveCriticalSection(&CriticalSection);
}

void C__Trace::SetThreadNameW(const WCHAR* name)
{
    EnterCriticalSection(&CriticalSection);
    DWORD storedLastError = GetLastError();
#ifdef MULTITHREADED_TRACE_ENABLE
    if (ThreadCache.GetUniqueThreadId(GetCurrentThreadId()) != -1) // only with assigned UID, otherwise all "unknown" would be suddenly named with this name
        SendSetNameMessageToServer(NULL, name, __mtSetThreadNameW);
#else  // MULTITHREADED_TRACE_ENABLE
    SendSetNameMessageToServer(NULL, name, __mtSetThreadNameW);
#endif // MULTITHREADED_TRACE_ENABLE
    SetLastError(storedLastError);
    LeaveCriticalSection(&CriticalSection);
}

C__Trace&
C__Trace::SetInfo(const char* file, int line)
{
    File = file;
    FileW = NULL;
    Line = line;
    return *this;
}

C__Trace&
C__Trace::SetInfoW(const WCHAR* file, int line)
{
    File = NULL;
    FileW = file;
    Line = line;
    return *this;
}

struct C__TraceMsgBoxThreadData
{
    char* Msg;        // allocated text of the message
    const char* File; // just a reference to a static string
    int Line;
};

DWORD WINAPI __TraceMsgBoxThread(void* param)
{
    C__TraceMsgBoxThreadData* data = (C__TraceMsgBoxThreadData*)param;
    char msg[1000];
    sprintf_s(msg, "TRACE_C message received!\n\n"
                   "File: %s\n"
                   "Line: %d\n\n"
                   "Message: ",
              data->File, data->Line);
    const char* appendix = "\n\nTRACE_C message means that fatal error has occured. "
                           "Application will be crashed by \"access violation\" exception after "
                           "clicking OK. Please send us bug report to help us fix this problem. "
                           "If you want to copy this message to clipboard, use Ctrl+C key.";
    lstrcpynA(msg + (int)strlen(msg), data->Msg, _countof(msg) - (int)strlen(msg) - (int)strlen(appendix));
    lstrcpynA(msg + (int)strlen(msg), appendix, _countof(msg) - (int)strlen(msg));
    MessageBoxA(NULL, msg, "Debug Message", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return 0;
}

struct C__TraceMsgBoxThreadDataW
{
    WCHAR* Msg;        // allocated text of the message
    const WCHAR* File; // just a reference to a static string
    int Line;
};

DWORD WINAPI __TraceMsgBoxThreadW(void* param)
{
    C__TraceMsgBoxThreadDataW* data = (C__TraceMsgBoxThreadDataW*)param;
    WCHAR msg[1000];
    swprintf_s(msg, L"TRACE_C message received!\n\n"
                    L"File: %s\n"
                    L"Line: %d\n\n"
                    L"Message: ",
               data->File, data->Line);
    const WCHAR* appendix = L"\n\nTRACE_C message means that fatal error has occured. "
                            L"Application will be crashed by \"access violation\" exception after "
                            L"clicking OK. Please send us bug report to help us fix this problem. "
                            L"If you want to copy this message to clipboard, use Ctrl+C key.";
    lstrcpynW(msg + (int)wcslen(msg), data->Msg, _countof(msg) - (int)wcslen(msg) - (int)wcslen(appendix));
    lstrcpynW(msg + (int)wcslen(msg), appendix, _countof(msg) - (int)wcslen(msg));
    MessageBoxW(NULL, msg, L"Debug Message", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return 0;
}

#if defined(TRACE_TO_FILE) && defined(__TRACESERVER)

DWORD WINAPI __TraceMsgBoxThreadErrInTS(void* param)
{
    MessageBoxW(NULL, (WCHAR*)param, L"Trace Server", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return 0;
}

#endif // defined(TRACE_TO_FILE) && defined(__TRACESERVER)

C__Trace&
C__Trace::SendMessageToServer(C__MessageType type, BOOL crash)
{
    BOOL unicode = type == __mtInformationW || type == __mtErrorW;
    // flushing to buffer
    if (unicode)
        TraceStrStreamW.flush();
    else
        TraceStrStream.flush();

    SYSTEMTIME st;
    GetLocalTime(&st);

    BOOL writePCWarning = FALSE;
    const char* pcWarning = "[Performance Counter BUG detected! Using last good PC value]: ";
    const WCHAR* pcWarningW = L"[Performance Counter BUG detected! Using last good PC value]: ";
    int pcWarningLen;
    double performanceCounterValue;
    DWORD addToMessageSize = 0;
    if (SupportPerformanceFrequency)
    {
        LARGE_INTEGER perfCounter;
        ::QueryPerformanceCounter(&perfCounter);

        static LONGLONG lastPC = 0;
        if (lastPC != 0 && lastPC > perfCounter.QuadPart) // the counter must always increase, a decrease is an error (this error occurs on multi-core processors, the solution is to set the affinity to a single core for the debugged process in Task Manager)
        {
            perfCounter.QuadPart = lastPC + 1; // artificially increase the value of the counter to the last value plus one (just to prevent it from decreasing and to avoid completely wrong classification in the Trace Server)
            pcWarningLen = unicode ? (int)wcslen(pcWarningW) : (int)strlen(pcWarning);
            addToMessageSize = pcWarningLen;
            writePCWarning = TRUE;
        }
        lastPC = perfCounter.QuadPart;

        performanceCounterValue = (double)perfCounter.QuadPart / PerformanceFrequency.QuadPart * 1000.0;
    }
    else
        performanceCounterValue = 0.0;

#ifdef TRACE_TO_FILE
    if (HTraceFile != NULL)
    {
        DWORD wr;
        WCHAR bufW[5000];
        swprintf_s(bufW, unicode ? L"%s\t%d\t" // file name in FileW (unicode)
#ifdef MULTITHREADED_TRACE_ENABLE
                                   L"%d\t"
#endif // MULTITHREADED_TRACE_ENABLE
                                   L"%d.%d.%d\t%d:%02d:%02d.%03d\t%.3lf\t%s\t%d\t"
                                 : L"%s\t%d\t" // File name in File (ANSI)
#ifdef MULTITHREADED_TRACE_ENABLE
                                   L"%d\t"
#endif // MULTITHREADED_TRACE_ENABLE
                                   L"%d.%d.%d\t%d:%02d:%02d.%03d\t%.3lf\t%S\t%d\t",
                   type == __mtInformation || type == __mtInformationW ? L"Info" : L"Error", GetCurrentThreadId(),
#ifdef MULTITHREADED_TRACE_ENABLE
                   ThreadCache.GetUniqueThreadId(GetCurrentThreadId()),
#endif // MULTITHREADED_TRACE_ENABLE
                   st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                   performanceCounterValue, unicode ? (void*)FileW : (void*)File, Line);
        WriteFile(HTraceFile, bufW, sizeof(WCHAR) * (int)wcslen(bufW), &wr, NULL);
        if (writePCWarning)
            WriteFile(HTraceFile, pcWarningW, sizeof(WCHAR) * (int)wcslen(pcWarningW), &wr, NULL);
        if (unicode)
            WriteFile(HTraceFile, TraceStringBufW.c_str(), sizeof(WCHAR) * (int)TraceStringBufW.length(), &wr, NULL);
        else
        {
            if (TraceStringBuf.length() > 0)
            {
                // Convert the ANSI string to UNICODE
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, TraceStringBuf.c_str(),
                                    (int)TraceStringBuf.length() + 1, bufW, _countof(bufW));
                bufW[_countof(bufW) - 1] = 0;
                WriteFile(HTraceFile, bufW, sizeof(WCHAR) * (int)wcslen(bufW), &wr, NULL);
            }
        }
        WriteFile(HTraceFile, L"\r\n", sizeof(WCHAR) * 2, &wr, NULL);
        FlushFileBuffers(HTraceFile); // flush the data to disk

#ifdef __TRACESERVER
        // for debugging Trace Server: TRACE messages go only to a file, when TRACE_E comes, we notify with a msgbox
        if (!crash && (type == __mtError || type == __mtErrorW))
        {
            swprintf_s(bufW, L"Error message from Trace Server has been written to file with traces:\n%s", TraceFileName);

            // Print a message in a different thread to avoid flooding messages of the current thread
            DWORD id;
            HANDLE msgBoxThread = CreateThread(NULL, 0, __TraceMsgBoxThreadErrInTS, bufW, 0, &id);
            if (msgBoxThread != NULL)
            {
                WaitForSingleObject(msgBoxThread, INFINITE);
                CloseHandle(msgBoxThread);
            }
        }
#endif // __TRACESERVER
    }
#endif // TRACE_TO_FILE

    if (HWritePipe != NULL)
    {
        DWORD fileSize = (DWORD)((unicode ? wcslen(FileW) : strlen(File)) + 1);
        DWORD textSize = (DWORD)((unicode ? TraceStringBufW.length() : TraceStringBuf.length()) + 1);

        char data[__SIZEOF_PIPEDATAHEADER];
        *(int*)&data[0] = type;                   // Type
        *(DWORD*)&data[4] = GetCurrentThreadId(); // ThreadID, UniqueThreadID
#ifdef MULTITHREADED_TRACE_ENABLE
        *(DWORD*)&data[8] = ThreadCache.GetUniqueThreadId(*(DWORD*)&data[4]);
#else                                                                         // MULTITHREADED_TRACE_ENABLE
        *(DWORD*)&data[8] = *(DWORD*)&data[4];
#endif                                                                        // MULTITHREADED_TRACE_ENABLE
        *(SYSTEMTIME*)(data + 12) = st;                                       // Time
        *(DWORD*)&data[28] = (DWORD)(fileSize + textSize + addToMessageSize); // MessageSize
        *(DWORD*)&data[32] = fileSize;                                        // MessageTextOffset
        *(DWORD*)&data[36] = Line;                                            // Line
        *(double*)&data[40] = performanceCounterValue;

        if (!WritePipe(data, __SIZEOF_PIPEDATAHEADER) ||
            !WritePipe(unicode ? (void*)FileW : (void*)File, (DWORD)((unicode ? sizeof(WCHAR) : 1) * fileSize)) ||
            writePCWarning && !WritePipe(unicode ? (void*)pcWarningW : (void*)pcWarning,
                                         (unicode ? sizeof(WCHAR) : 1) * pcWarningLen) || // At the beginning of the message, I output the PC error, it is quite a crucial thing during debugging (messages are out of the real order)
            !WritePipe(unicode ? (void*)TraceStringBufW.c_str() : (void*)TraceStringBuf.c_str(),
                       (unicode ? sizeof(WCHAR) : 1) * textSize))
        {
            CloseWritePipeAndSemaphore();
        }
    }
    // only if crash==TRUE:
    // create a copy of the data, starting a thread for the msgbox can trigger additional TRACE
    // messages (e.g. in DllMain response to DLL_THREAD_ATTACH), if we didn't exit
    // CriticalSection, deadlock would occur;
    // TRACE_C must not be used in DllMain, otherwise a deadlock will occur:
    //   - if it is in DLL_THREAD_ATTACH: it wants to open a new thread for msgbox
    //     and this is blocked from DllMain
    //   - if it is possible to do in DLL_THREAD_DETACH: wait for the thread to close with a message box
    //     predesleho TRACE_C zachytime TRACE_C z DLL_THREAD_DETACH a nechame ho
    //     wait in an infinite loop, see below
    // Additionally, we are implementing defense against multiplying msgboxes when multiple TRACE_C are active at the same time.
    // by to jen zmatky, ted se otevre msgbox jen pro prvni a ten po uzavreni vyvola
    // If an exception occurs, other TRACE_C will be caught in an infinite waiting loop, see below
    static BOOL msgBoxOpened = FALSE;
    C__TraceMsgBoxThreadData threadData;
    C__TraceMsgBoxThreadDataW threadDataW;
    if (crash) // break/crash after printing TRACE error messages (TRACE_C and TRACE_MC)
    {
        if (!msgBoxOpened)
        {
            if (unicode)
            {
                threadDataW.Msg = (WCHAR*)GlobalAlloc(GMEM_FIXED, sizeof(WCHAR) * (TraceStringBufW.length() + 1));
                if (threadDataW.Msg != NULL)
                {
                    lstrcpynW(threadDataW.Msg, TraceStringBufW.c_str(), (int)(TraceStringBufW.length() + 1));
                    threadDataW.File = FileW;
                    threadDataW.Line = Line;
                    msgBoxOpened = TRUE;
                }
            }
            else
            {
                threadData.Msg = (char*)GlobalAlloc(GMEM_FIXED, TraceStringBuf.length() + 1);
                if (threadData.Msg != NULL)
                {
                    lstrcpynA(threadData.Msg, TraceStringBuf.c_str(), (int)(TraceStringBuf.length() + 1));
                    threadData.File = File;
                    threadData.Line = Line;
                    msgBoxOpened = TRUE;
                }
            }
        }
        else
        {
            if (unicode)
                threadDataW.Msg = NULL;
            else
                threadData.Msg = NULL;
        }
    }
    if (unicode)
        TraceStringBufW.erase(); // preparation for the next trace
    else
        TraceStringBuf.erase();
    LeaveCriticalSection(&CriticalSection);
    if (crash)
    {
        if (unicode && threadDataW.Msg != NULL || // break/crash after printing TRACE error messages (TRACE_C and TRACE_MC)
            !unicode && threadData.Msg != NULL)
        {
            // Print a message in a different thread to avoid flooding messages of the current thread
            DWORD id;
            HANDLE msgBoxThread = CreateThread(NULL, 0, unicode ? __TraceMsgBoxThreadW : __TraceMsgBoxThread,
                                               unicode ? (void*)&threadDataW : (void*)&threadData, 0, &id);
            if (msgBoxThread != NULL)
            {
                WaitForSingleObject(msgBoxThread, INFINITE); // if TRACE_C is placed in DllMain in DLL_THREAD_ATTACH, a deadlock will occur - highly unlikely, not addressed
                CloseHandle(msgBoxThread);
            }
            msgBoxOpened = FALSE;
            GlobalFree(unicode ? (HGLOBAL)threadDataW.Msg : (HGLOBAL)threadData.Msg);
            // Software crash will be triggered directly in the code where TRACE_C/TRACE_MC is located, in order to
            // It was possible to see exactly where the macros are located in the bug report; therefore, the crash follows
            // after finishing this method
        }
        else // Other threads with TRACE_C will be blocked until the msgbox opened for
        {    // first TRACE_C, so it crashes there, let's not make a mess of it
            if (msgBoxOpened)
            {
                while (1)
                    Sleep(1000); // Deadlock can occur due to blocking, for example when TRACE_C is (and should not be) in DLL_THREAD_DETACH
            }
        }
    }
    return *this;
}

#endif // TRACE_ENABLE

// template for defining these "forbidden" operators (to enable control
// prohibited combinations of WCHAR / char string in one TRACE or MESSAGE macro,
// the following operators must not be defined in other modules - otherwise the linker
// did not report an error - idea: in DEBUG version we catch linker errors, in RELEASE version we catch
// errors in your own operator definitions; to test both things, they must be enabled
// TRACE_ENABLE in DEBUG and RELEASE versions, which for example meets the SDK build in Salamander;
// the most common model is to have TRACE_ENABLE enabled in DEBUG version and disabled in RELEASE version, in this
// In this case, only the first test, which is more important, will be executed (forbidden string combinations)
// WCHAR / char))
#ifndef _DEBUG

#include <ostream>

std::ostream& operator<<(std::ostream& out, const wchar_t* str)
{
    return out << (void*)str;
}
std::wostream& operator<<(std::wostream& out, const char* str) { return out << (void*)str; }

#endif // _DEBUG
