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

#pragma warning(disable : 4075) // chceme definovat poradi inicializace modulu

typedef void(__cdecl* _PVFV)(void);

#pragma section(".i_trc$a", read)
__declspec(allocate(".i_trc$a")) const _PVFV i_trace = (_PVFV)1; // na zacatek sekce .i_trc si dame promennou i_trace

#pragma section(".i_trc$z", read)
__declspec(allocate(".i_trc$z")) const _PVFV i_trace_end = (_PVFV)1; // a na konec sekce .i_trc si dame promennou i_trace_end

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
        if (hw == tid) // nalezeno
        {
            index = m;
            return TRUE;
        }
        else if (hw > tid)
        {
            if (l == r || l > m - 1) // nenalezeno
            {
                index = m; // mel by byt na teto pozici
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // nenalezeno
            {
                index = m + 1; // mel by byt az za touto pozici
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
        if (Available == Count) // je plno, vyhazime mrtvy thready
        {
            DWORD code;
            for (int i = Count - 1; i >= 0; i--)
            {
                if (!GetExitCodeThread(Data[i].Handle, &code) || code != STILL_ACTIVE)
                {
                    DWORD id = Data[i].TID;                         // aktualizace cache:
                    if (CacheUID[__TraceCacheGetIndex(id)] != -1 && // platny zaznam
                        CacheTID[__TraceCacheGetIndex(id)] == id)   // shodny TID
                    {
                        CacheUID[__TraceCacheGetIndex(id)] = -1; // zneplatneni
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

        Move(1, index, Count - index); // vlozime novy zaznam
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
    // aktualizace cache
    CacheTID[__TraceCacheGetIndex(tid)] = tid;
    CacheUID[__TraceCacheGetIndex(tid)] = UniqueThreadID++;

    return TRUE;
}

DWORD
C__TraceThreadCache::GetUniqueThreadId(DWORD tid)
{
    if (CacheUID[__TraceCacheGetIndex(tid)] != -1 && // je-li platny zaznam
        CacheTID[__TraceCacheGetIndex(tid)] == tid)  // a je-li shodny s tid
    {
        return CacheUID[__TraceCacheGetIndex(tid)]; // uid je v cache
    }

    int index;
    if (GetIndex(tid, index))
    {
        CacheTID[__TraceCacheGetIndex(tid)] = Data[index].TID;
        CacheUID[__TraceCacheGetIndex(tid)] = Data[index].UID;
        return Data[index].UID;
    }
    else
        return -1; // nenalezeno -> to by se nemelo stat
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
    // nove streamy pouzivaji interne locales, ktere maji implementovany
    // jednotlive "facets" pomoci lazy creation - jsou alokovany na heapu
    // kdyz jsou potreba, tedy kdyz nekdo posle do streamu neco, co ma
    // formatovani zavisle na lokalich pravidlech, treba cislo, datum,
    // nebo boolean. Tyto "facets" jsou pak dealokovany pri exitu
    // programu s prioritou compiler, tzn. po nasi kontrole memory leaku.
    // Takze pokud nekdo pouzije stream k vypisu cehokoli lokalizovatelneho,
    // nas debug heap zacne hlasit memory leaky, i kdyz zadne nejsou. Abychom
    // tomu predesli, donutime locales vytvorit vsechny "facets" ted, dokud
    // jeste nehlidame heap.
    // Zatim pouzivame pouze vystupni stream a pouze se stringy (bez konverze)
    // a cisly. Takze poslat cislo do stringstreamu by melo stacit. Pokud
    // v budoucnu zacneme pouzivat streamy vic a debug heap zacne hlasit
    // leaky, budeme zde muset pridat vic vstupu/vystupu.
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

            if ((s - tmpDir) + 15 < MAX_PATH) // dost mista pro pripojeni "_2000000000.log"
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
                        break; // neocekavana chyba (a neni ji kam vypsat)
                }
                if (HTraceFile == INVALID_HANDLE_VALUE)
                    HTraceFile = NULL;
                else // zapis headeru do souboru (identifikace sloupcu)
                {
                    DWORD wr;
                    const WCHAR* fileHeader = L"\xFEFF" /* BOM */ L"Type\tTID\t"
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

    if (HWritePipe != NULL) // test spojeni se serverem, jestli je down, HWritePipe se zavre a nasledne zkusime reconnect
        TRACE_I("Connect request received when already connected to Trace Server.");

    BOOL ret = FALSE;
    if (HWritePipe != NULL)
        ret = TRUE; // pokud je jiz spojeni navazano
    else
    {
        // pokusim se otevrit Mutex pro pristup ke sdilene pameti
        HANDLE hOpenConnectionMutex;
        hOpenConnectionMutex = OpenMutex(/*MUTEX_ALL_ACCESS*/ SYNCHRONIZE, FALSE, __OPEN_CONNECTION_MUTEX);
        if (hOpenConnectionMutex != NULL) // server nalezen
        {
            // prevezmu ConnectionMutex
            DWORD waitRet;
            while (1)
            {
                waitRet = WaitForSingleObject(hOpenConnectionMutex,
                                              __COMMUNICATION_WAIT_TIMEOUT);
                if (waitRet != WAIT_ABANDONED)
                    break;
            }
            if (waitRet == WAIT_OBJECT_0) // podaril se prevzit
            {
                // otevru FileMapping
                HANDLE hFileMapping;
                hFileMapping = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, __FILE_MAPPING_NAME);
                if (hFileMapping != NULL)
                {
                    // namapuju spolecnou pamet
                    char* mapAddress;
                    mapAddress = (char*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
                                                      0, 0, __SIZEOF_CLIENTSERVERINITDATA);
                    if (mapAddress != NULL)
                    {
                        BytesAllocatedForWriteToPipe = 0;
                        HPipeSemaphore = CreateSemaphore(NULL, __PIPE_SIZE, __PIPE_SIZE, NULL);
                        if (HPipeSemaphore != NULL)
                        {
                            HANDLE HReadPipe;

                            // vytvorim anonymous pipe
                            SECURITY_ATTRIBUTES sa;
                            char secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
                            sa.nLength = sizeof(sa);
                            sa.bInheritHandle = FALSE;
                            sa.lpSecurityDescriptor = &secDesc;
                            InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
                            // give the security descriptor a NULL DACL, done using the  "TRUE, (PACL)NULL" here
                            SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, 0, FALSE);
                            if (CreatePipe(&HReadPipe, &HWritePipe, &sa, __PIPE_SIZE * 1024))
                            {
                                // zapisu do sdilene pameti handle pro cteni z pipy
                                int expectedServerVer = TRACE_CLIENT_VERSION;
                                *(int*)&mapAddress[0] = expectedServerVer - 1;               // Version (zkousime nejdrive starsi zpusob connectu)
                                *(DWORD*)&mapAddress[4] = GetCurrentProcessId();             // ClientOrServerProcessId (tady jde o PID klienta)
                                *(DWORD*)&mapAddress[8] = (DWORD)(DWORD_PTR)HReadPipe;       // HReadOrWritePipe (tady jde o HReadPipe)
                                *(DWORD*)&mapAddress[12] = (DWORD)(DWORD_PTR)HPipeSemaphore; // HPipeSemaphore

                                // otevru hReadyEvent
                                HANDLE hReadyEvent;
                                hReadyEvent = OpenEvent(EVENT_ALL_ACCESS, TRUE, __CONNECT_DATA_READY_EVENT_NAME);
                                // otevru hAcceptedEvent
                                HANDLE hAcceptedEvent;
                                hAcceptedEvent = OpenEvent(EVENT_ALL_ACCESS, TRUE, __CONNECT_DATA_ACCEPTED_EVENT_NAME);
                                if (hReadyEvent != NULL && hAcceptedEvent != NULL)
                                {
                                    ResetEvent(hAcceptedEvent); // chci jen cerstvou odpoved serveru

                                    while (1)
                                    {
                                        SetEvent(hReadyEvent); // reknu serveru, ze jsem pripravil data

                                        // pockam, az server data zpracuje
                                        waitRet = WaitForSingleObject(hAcceptedEvent, __COMMUNICATION_WAIT_TIMEOUT);
                                        if (waitRet == WAIT_OBJECT_0)
                                        {
                                            // hlaska vypisovana na starem neunikodovem Trace Serveru (musi byt ANSI)
                                            const char* oldTraceServerA = "Disconnecting: this is not Unicode version of Trace Server.";

                                            // podivam se na vysledek ze serveru
                                            if (*((BOOL*)mapAddress) == TRUE)
                                            {
                                                if (expectedServerVer == TRACE_CLIENT_VERSION) // hura, povedlo se pripojit na novy Trace Server!
                                                {
#ifdef TRACE_IGNORE_AUTOCLEAR
                                                    ret = SendIgnoreAutoClear(TRUE); // ignorovat, pri chybe provedeme disconnect
#else                                                                                // TRACE_IGNORE_AUTOCLEAR
                                                    ret = SendIgnoreAutoClear(FALSE);         // neignorovat, pri chybe provedeme disconnect
#endif                                                                               // TRACE_IGNORE_AUTOCLEAR
                                                }
                                                else
                                                    TRACE_E(oldTraceServerA);
                                            }
                                            else // nepovedlo se: zkusime pipe vytvorit na strane serveru
                                            {
                                                // zapisu do sdilene pameti novou verzi, tim pozadam server o poslani handlu pro zapis do pipy
                                                *(int*)&mapAddress[0] = expectedServerVer; // Version

                                                SetEvent(hReadyEvent); // reknu serveru, ze jsem pripravil data

                                                // pockam, az server data zpracuje
                                                waitRet = WaitForSingleObject(hAcceptedEvent, __COMMUNICATION_WAIT_TIMEOUT);
                                                if (waitRet == WAIT_OBJECT_0 && *((BOOL*)mapAddress) == TRUE) // podivam se na vysledek ze serveru
                                                {
                                                    HANDLE hWritePipeFromSrv = NULL;
                                                    HANDLE hPipeSemaphoreFromSrv = NULL;

                                                    // ziskani handlu server procesu
                                                    HANDLE hServerProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE,
                                                                                        *(DWORD*)&mapAddress[4] /* ClientOrServerProcessId (tady jde o PID serveru) */);
                                                    // ziskani handlu pipy a semaforu
                                                    if (hServerProcess != NULL &&
                                                        DuplicateHandle(hServerProcess, (HANDLE)(DWORD_PTR)(*(DWORD*)&mapAddress[8]) /* HReadOrWritePipe (tady jde o HWritePipe) */, // server
                                                                        GetCurrentProcess(), &hWritePipeFromSrv,                                                                     // klient
                                                                        GENERIC_WRITE, FALSE, 0) &&
                                                        DuplicateHandle(hServerProcess, (HANDLE)(DWORD_PTR)(*(DWORD*)&mapAddress[12]) /* HPipeSemaphore */, // server
                                                                        GetCurrentProcess(), &hPipeSemaphoreFromSrv,                                        // klient
                                                                        0, FALSE, DUPLICATE_SAME_ACCESS))
                                                    {
                                                        *((int*)mapAddress) = 3;                         // zapis vysledku -> 3 = povedlo se, handly mame
                                                        *(DWORD*)&mapAddress[4] = GetCurrentProcessId(); // ClientOrServerProcessId (tady jde o PID klienta)
                                                    }
                                                    else
                                                    {
                                                        *((BOOL*)mapAddress) = FALSE; // zapis vysledku -> nepovedlo se
                                                    }
                                                    if (hServerProcess != NULL)
                                                        CloseHandle(hServerProcess);

                                                    SetEvent(hReadyEvent); // reknu serveru, ze jsem data precetl a zapsal vysledek

                                                    // pri uspechu: pockam, az server nastartuje thread cteni dat z pipy a posle vysledek
                                                    // pri neuspechu: dame serveru vedet, ze se to nepovedlo, vrati nam opet neuspech
                                                    waitRet = WaitForSingleObject(hAcceptedEvent, __COMMUNICATION_WAIT_TIMEOUT);
                                                    if (waitRet == WAIT_OBJECT_0 && // podivam se na vysledek ze serveru
                                                        *((int*)mapAddress) == 2 /* 2 = uspesne nastartovany cteci thread v serveru */)
                                                    {
                                                        CloseHandle(HPipeSemaphore);
                                                        HPipeSemaphore = hPipeSemaphoreFromSrv; // pouzijeme semafor ze serveru (ten klientsky zavreme)

                                                        CloseHandle(HWritePipe);
                                                        HWritePipe = hWritePipeFromSrv; // pouzijeme pipu ze serveru (tu klientskou zavreme)

                                                        if (expectedServerVer == TRACE_CLIENT_VERSION) // hura, povedlo se pripojit na novy Trace Server!
                                                        {
#ifdef TRACE_IGNORE_AUTOCLEAR
                                                            ret = SendIgnoreAutoClear(TRUE); // ignorovat, pri chybe provedeme disconnect
#else                                                                                        // TRACE_IGNORE_AUTOCLEAR
                                                            ret = SendIgnoreAutoClear(FALSE); // neignorovat, pri chybe provedeme disconnect
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
                                                else // connect se nepovedl ani jednim zpusobem (asi stary Trace Server)
                                                {
                                                    if (expectedServerVer == TRACE_CLIENT_VERSION) // zkouseli jsme novou verzi serveru
                                                    {
                                                        expectedServerVer = TRACE_CLIENT_VERSION - 2; // ted zkusime starsi verzi serveru
                                                        // zapisu do sdilene pameti verzi, kterou zvlada stara verze serveru
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
                ReleaseMutex(hOpenConnectionMutex); // muzou se connectit jini clienti
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
            if (res == WAIT_TIMEOUT) // timeout, zkontrolujem, jestli pipe na server jeste zije
            {
                if (!WriteFile(HWritePipe, lpBuffer, 0, &numberOfBytesWritten, NULL))
                    return FALSE;
            }
            else // jina chyba, radsi koncime, at z toho neni nekonecny cyklus
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
    *(DWORD*)&data[4] = ignore ? 1 : 0;    // ThreadID: 0 = neignorovat, 1 = ignorovat auto-clear na Trace Serveru
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
    if (ThreadCache.GetUniqueThreadId(GetCurrentThreadId()) != -1) // jen s pridelenym UID, jinak by vsechny "unknown" byly najednou pojmenovany timto jmenem
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
    if (ThreadCache.GetUniqueThreadId(GetCurrentThreadId()) != -1) // jen s pridelenym UID, jinak by vsechny "unknown" byly najednou pojmenovany timto jmenem
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
    char* Msg;        // alokovany text hlasky
    const char* File; // jen odkaz na staticky string
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
    WCHAR* Msg;        // alokovany text hlasky
    const WCHAR* File; // jen odkaz na staticky string
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
    // flushnuti do bufferu
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
        if (lastPC != 0 && lastPC > perfCounter.QuadPart) // counter musi stale rust, snizeni je chyba (na vicejadrovych procesorech se tahle chyba objevuje, resenim je nastaveni affinity na jedine jadro pro ladeny proces v Task Manageru)
        {
            perfCounter.QuadPart = lastPC + 1; // umele zvysime hodnotu counteru na posledni hodnotu plus jedna (jen aby se nesnizil a nedoslo k uplne spatnemu zarazeni v Trace Serveru)
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
        swprintf_s(bufW, unicode ? L"%s\t%d\t" // jmeno souboru ve FileW (unicode)
#ifdef MULTITHREADED_TRACE_ENABLE
                                   L"%d\t"
#endif // MULTITHREADED_TRACE_ENABLE
                                   L"%d.%d.%d\t%d:%02d:%02d.%03d\t%.3lf\t%s\t%d\t"
                                 : L"%s\t%d\t" // jmeno souboru ve File (ANSI)
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
        FlushFileBuffers(HTraceFile); // flushneme data na disk

#ifdef __TRACESERVER
        // pro ladeni Trace Serveru: TRACE hlasky jdou jen do souboru, kdyz prijde TRACE_E, upozornime msgboxem
        if (!crash && (type == __mtError || type == __mtErrorW))
        {
            swprintf_s(bufW, L"Error message from Trace Server has been written to file with traces:\n%s", TraceFileName);

            // vypiseme hlasku v jinem threadu, aby nepumpovala zpravy aktualniho threadu
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
                                         (unicode ? sizeof(WCHAR) : 1) * pcWarningLen) || // na zacatek zpravy vypisu chybu PC, pri ladeni je to dost zasadni vec (zpravy jsou mimo realne poradi)
            !WritePipe(unicode ? (void*)TraceStringBufW.c_str() : (void*)TraceStringBuf.c_str(),
                       (unicode ? sizeof(WCHAR) : 1) * textSize))
        {
            CloseWritePipeAndSemaphore();
        }
    }
    // jen je-li crash==TRUE:
    // vyrobime kopii dat, start threadu pro msgbox totiz muze vyvolat dalsi TRACE
    // hlasky (napr. v DllMain reakce na DLL_THREAD_ATTACH), pokud bysme neopustili
    // CriticalSection, nastal by deadlock;
    // v DllMain se nesmi pouzivat TRACE_C, jinak dojde k deadlocku:
    //   - pokud se da do DLL_THREAD_ATTACH: chce si otevrit novy thread pro msgbox
    //     a to je z DllMainu blokovane
    //   - pokud se da do DLL_THREAD_DETACH: pri cekani na zavreni threadu s msgboxem
    //     predesleho TRACE_C zachytime TRACE_C z DLL_THREAD_DETACH a nechame ho
    //     cekat v nekonecnem cyklu, viz nize
    // navic zavadime obranu proti mnozeni msgboxu pri vice TRACE_C zaroven, pusobilo
    // by to jen zmatky, ted se otevre msgbox jen pro prvni a ten po uzavreni vyvola
    // padacku, ostatni TRACE_C zustanou chyceny v nekonecne cekaji smycce, viz nize
    static BOOL msgBoxOpened = FALSE;
    C__TraceMsgBoxThreadData threadData;
    C__TraceMsgBoxThreadDataW threadDataW;
    if (crash) // break/padacka po vypisu TRACE error hlasky (TRACE_C a TRACE_MC)
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
        TraceStringBufW.erase(); // priprava pro dalsi trace
    else
        TraceStringBuf.erase();
    LeaveCriticalSection(&CriticalSection);
    if (crash)
    {
        if (unicode && threadDataW.Msg != NULL || // break/padacka po vypisu TRACE error hlasky (TRACE_C a TRACE_MC)
            !unicode && threadData.Msg != NULL)
        {
            // vypiseme hlasku v jinem threadu, aby nepumpovala zpravy aktualniho threadu
            DWORD id;
            HANDLE msgBoxThread = CreateThread(NULL, 0, unicode ? __TraceMsgBoxThreadW : __TraceMsgBoxThread,
                                               unicode ? (void*)&threadDataW : (void*)&threadData, 0, &id);
            if (msgBoxThread != NULL)
            {
                WaitForSingleObject(msgBoxThread, INFINITE); // pokud se da TRACE_C do DllMain do DLL_THREAD_ATTACH, dojde k deadlocku - silne nepravdepodobne, neresime
                CloseHandle(msgBoxThread);
            }
            msgBoxOpened = FALSE;
            GlobalFree(unicode ? (HGLOBAL)threadDataW.Msg : (HGLOBAL)threadData.Msg);
            // pad softu vyvolame primo v kodu, kde je umisteny TRACE_C/TRACE_MC, aby
            // bylo v bug reportu videt presne kde makra lezi; padacka tedy nasleduje
            // po dokonceni teto metody
        }
        else // ostatni thready s TRACE_C zablokujeme, az se zavre msgbox otevreny pro
        {    // prvni TRACE_C, tak to tam i spadne, at v tom neni bordel
            if (msgBoxOpened)
            {
                while (1)
                    Sleep(1000); // blokace vede na deadlock napr. kdyz je (a nema byt) TRACE_C v DLL_THREAD_DETACH
            }
        }
    }
    return *this;
}

#endif // TRACE_ENABLE

// pasticka na vlastni definici techto "zakazanych" operatoru (aby fungovala kontrola
// zakazanych kombinaci stringu WCHAR / char v jednom TRACE nebo MESSAGE makru,
// nesmi byt nasledujici operatory definovany v jinych modulech - jinak by linker
// neohlasil chybu - idea: v DEBUG verzi chytame chyby linkeru, v RELEASE verzi chytame
// chyby vlastni definice operatoru; aby se otestovaly obe veci, musi byt povolene
// TRACE_ENABLE v DEBUG i RELEASE verzi, coz napr. u Salamandera splnuje SDK build;
// nejbeznejsi model je v DEBUG verzi mit povolene TRACE_ENABLE a v RELEASE ne, v tomto
// pripade se provede jen prvni test, ktery je dulezitejsi (zakazane kombinace stringu
// WCHAR / char))
#ifndef _DEBUG

#include <ostream>

std::ostream& operator<<(std::ostream& out, const wchar_t* str)
{
    return out << (void*)str;
}
std::wostream& operator<<(std::wostream& out, const char* str) { return out << (void*)str; }

#endif // _DEBUG
