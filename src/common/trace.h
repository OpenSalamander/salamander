// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// makro TRACE_ENABLE - zapoji vypis hlasek na server
// makro MULTITHREADED_TRACE_ENABLE - zapoji premapovavani TID na UTID
// makro TRACE_TO_FILE - zapoji vypis hlasek do souboru v TEMPu (vyzaduje definici TRACE_ENABLE)
// makro TRACE_IGNORE_AUTOCLEAR - zakaze Trace Serveru pri pripojeni tohoto procesu smazat vsechny zpravy,
//                                i kdyz to ma zaple v nastaveni (hodi se pro utilitky spoustene za behu
//                                hlavniho programu, u kterych je mazani zprav nezadouci)
// makro __TRACESERVER - includeno z trace-serveru

// modul TRACE je pripraven na multi-threadove aplikace

// POZOR: TRACE_C se nesmi pouzivat v DllMain knihoven, ani v zadnem kodu, ktery
//        se z DllMainu vola, jinak dojde k deadlocku, vice viz implementace
//        C__Trace::SendMessageToServer

#if defined(__TRACESERVER) || defined(TRACE_ENABLE)

enum C__MessageType
{
    // druh message
    __mtInformation,
    __mtError,

    // nastaveni nazvu procesu / threadu
    __mtSetProcessName,
    __mtSetThreadName,

    // druh message - unicodove varianty zprav
    __mtInformationW,
    __mtErrorW,

    // nastaveni nazvu procesu / threadu - unicodove varianty zprav
    __mtSetProcessNameW,
    __mtSetThreadNameW,

    // zakazeme Trace Serveru pri pripojeni tohoto procesu smazat vsechny zpravy, i kdyz to ma
    // zaple v nastaveni (hodi se pro utilitky spoustene za behu hlavniho programu, u kterych je
    // mazani zprav nezadouci)
    __mtIgnoreAutoClear,
};

#endif // defined(__TRACESERVER) || defined(TRACE_ENABLE)

#ifndef __WFILE__
#define __WFILE__WIDEN2(x) L##x
#define __WFILE__WIDEN(x) __WFILE__WIDEN2(x)
#define __WFILE__ __WFILE__WIDEN(__FILE__)
#endif // __WFILE__

#define __TRACE_STR2(x) #x
#define __TRACE_STR(x) __TRACE_STR2(x)

//*****************************************************************************
//
// C__StringStreamBuf
//

class C__StringStreamBuf : public std::streambuf
{
private:
    enum
    {
        MINSIZE = 32,
        STARTSIZE = 100
    };

public:
    // allocate new character array and setup pointers in base class
    C__StringStreamBuf()
    {
        char* ptr = static_cast<char*>(GlobalAlloc(GMEM_FIXED, STARTSIZE));
        setp(ptr, ptr + STARTSIZE);
    }

    // discard any allocated buffer and clear pointers
    virtual ~C__StringStreamBuf()
    {
        GlobalFree(pbase());
        setp(0, 0);
    }

    // return pointer to the beginning of the data, terminated by null
    const char* c_str()
    {
        // add trailing null
        sputc('\0');

        // decrement the pointer back, trailing null is not part of the data
        pbump(-1);

        // and return the data
        return static_cast<const char*>(pbase());
    }

    // return the length of the string currently in the buffer
    size_t length() const
    {
        return pptr() - pbase();
    }

    // just reset pointers to pretend the buffer is empty
    void erase()
    {
        pbump(-static_cast<int>(length()));
    }

protected:
    // store the element in the buffer, growing it if neccessary
    virtual int_type overflow(int_type element = traits_type::eof())
    {
        // if EOF, just return success
        if (traits_type::eq_int_type(traits_type::eof(), element))
            return traits_type::not_eof(element);

        // grow the buffer, if needed
        if (pptr() == 0 || epptr() <= pptr())
        {
            // grow by 50 per cent
            size_t oldsize = pptr() == 0 ? 0 : epptr() - pbase();
            size_t newsize = oldsize;
            size_t inc = newsize / 2 < MINSIZE ? MINSIZE : newsize / 2;

            // if increment causes overflow, halve it
            while (0 < inc && INT_MAX - inc < newsize)
                inc /= 2;

            // buffer too large
            if (0 >= inc)
                return traits_type::eof();

            // allocate new character array
            newsize += inc;
            char* ptr = static_cast<char*>(GlobalAlloc(GMEM_FIXED, newsize));
            if (ptr == 0)
                return traits_type::eof();

            // copy data and dealocate old buffer, if neccessary
            if (pbase())
            {
                traits_type::_Copy_s(ptr, newsize, pbase(), oldsize);
                GlobalFree(pbase());
            }

            // update pointers
            setp(ptr, ptr + newsize);
            pbump((int)oldsize);
        }

        // store the character
        *pptr() = traits_type::to_char_type(element);
        pbump(1);

        // return success
        return element;
    }
};

//*****************************************************************************
//
// C__StringStreamBufW
//

class C__StringStreamBufW : public std::wstreambuf
{
private:
    enum
    {
        MINSIZE = 32,
        STARTSIZE = 100
    };

public:
    // allocate new character array and setup pointers in base class
    C__StringStreamBufW()
    {
        wchar_t* ptr = static_cast<wchar_t*>(GlobalAlloc(GMEM_FIXED, sizeof(wchar_t) * STARTSIZE));
        setp(ptr, ptr + STARTSIZE);
    }

    // discard any allocated buffer and clear pointers
    virtual ~C__StringStreamBufW()
    {
        GlobalFree(pbase());
        setp(0, 0);
    }

    // return pointer to the beginning of the data, terminated by null
    const wchar_t* c_str()
    {
        // add trailing null
        sputc(L'\0');

        // decrement the pointer back, trailing null is not part of the data
        pbump(-1);

        // and return the data
        return static_cast<const wchar_t*>(pbase());
    }

    // return the length of the string currently in the buffer
    size_t length() const
    {
        return pptr() - pbase();
    }

    // just reset pointers to pretend the buffer is empty
    void erase()
    {
        pbump(-static_cast<int>(length()));
    }

protected:
    // store the element in the buffer, growing it if neccessary
    virtual int_type overflow(int_type element = traits_type::eof())
    {
        // if EOF, just return success
        if (traits_type::eq_int_type(traits_type::eof(), element))
            return traits_type::not_eof(element);

        // grow the buffer, if needed
        if (pptr() == 0 || epptr() <= pptr())
        {
            // grow by 50 per cent
            size_t oldsize = pptr() == 0 ? 0 : epptr() - pbase();
            size_t newsize = oldsize;
            size_t inc = newsize / 2 < MINSIZE ? MINSIZE : newsize / 2;

            // if increment causes overflow, halve it
            while (0 < inc && INT_MAX - inc < newsize)
                inc /= 2;

            // buffer too large
            if (0 >= inc)
                return traits_type::eof();

            // allocate new character array
            newsize += inc;
            wchar_t* ptr = static_cast<wchar_t*>(GlobalAlloc(GMEM_FIXED, sizeof(wchar_t) * newsize));
            if (ptr == 0)
                return traits_type::eof();

            // copy data and dealocate old buffer, if neccessary
            if (pbase())
            {
                traits_type::_Copy_s(ptr, newsize, pbase(), oldsize);
                GlobalFree(pbase());
            }

            // update pointers
            setp(ptr, ptr + newsize);
            pbump((int)oldsize);
        }

        // store the character
        *pptr() = traits_type::to_char_type(element);
        pbump(1);

        // return success
        return element;
    }
};

//*****************************************************************************
//
// C__TraceStream
//

class C__TraceStream : public std::ostream
{
private:
    std::ostream& operator<<(const wchar_t* str) { return *this; }

public:
    C__TraceStream(C__StringStreamBuf* buf) : std::ostream(buf) {}

    std::ostream& operator<<(char i) { return *(std::ostream*)this << i; }
    std::ostream& operator<<(unsigned char i) { return *(std::ostream*)this << i; }
    //std::ostream& operator<<(wchar_t i) { return *(std::ostream*)this << i; }
    std::ostream& operator<<(int i) { return *(std::ostream*)this << i; }
    std::ostream& operator<<(unsigned int i) { return *(std::ostream*)this << i; }
    std::ostream& operator<<(float i) { return *(std::ostream*)this << i; }
    std::ostream& operator<<(double i) { return *(std::ostream*)this << i; }
    std::ostream& operator<<(__int64 i) { return *(std::ostream*)this << i; }
    std::ostream& operator<<(unsigned __int64 i) { return *(std::ostream*)this << i; }
};

//*****************************************************************************
//
// C__TraceStreamW
//

class C__TraceStreamW : public std::wostream
{
private:
    std::wostream& operator<<(const char* str) { return *this; }

public:
    C__TraceStreamW(C__StringStreamBufW* buf) : std::wostream(buf) {}

    std::wostream& operator<<(char i) { return *(std::wostream*)this << i; }
    std::wostream& operator<<(unsigned char i) { return *(std::wostream*)this << i; }
    std::wostream& operator<<(wchar_t i) { return *(std::wostream*)this << i; }
    std::wostream& operator<<(int i) { return *(std::wostream*)this << i; }
    std::wostream& operator<<(unsigned int i) { return *(std::wostream*)this << i; }
    std::wostream& operator<<(float i) { return *(std::wostream*)this << i; }
    std::wostream& operator<<(double i) { return *(std::wostream*)this << i; }
    std::wostream& operator<<(__int64 i) { return *(std::wostream*)this << i; }
    std::wostream& operator<<(unsigned __int64 i) { return *(std::wostream*)this << i; }
};

// NOT SUPPORTED (just to produce link error)
// they are shared between TRACE and MESSAGES modules (if error occurs, it may
// be invalid combination of WCHAR / char usage in TRACE or MESSAGE macros)
std::ostream& operator<<(std::ostream& out, const wchar_t* str);
std::wostream& operator<<(std::wostream& out, const char* str);

//
// ****************************************************************************
// CWStr
//
// Helper class for using TRACE in templates with char and WCHAR string types.
// Converts both string types to WCHAR string (for char string it allocates WCHAR string,
// for WCHAR string it just passes same string).

class CWStr
{
protected:
    BOOL IsOK;
    WCHAR* AllocBuf;
    const WCHAR* Str;

public:
    CWStr(const char* s);
    CWStr(const WCHAR* s)
    {
        IsOK = TRUE;
        AllocBuf = NULL;
        Str = s;
    }
    ~CWStr()
    {
        if (AllocBuf != NULL)
            free(AllocBuf);
    }

    const WCHAR* c_str() { return IsOK ? (const WCHAR*)(AllocBuf != NULL ? AllocBuf : Str) : L"Error in CWStr()"; }
};

#if defined(__TRACESERVER) || defined(TRACE_ENABLE)

extern const TCHAR* __FILE_MAPPING_NAME;
extern const TCHAR* __OPEN_CONNECTION_MUTEX;
extern const TCHAR* __CONNECT_DATA_READY_EVENT_NAME;
extern const TCHAR* __CONNECT_DATA_ACCEPTED_EVENT_NAME;

#define __PIPE_SIZE 100 // maximum dat v pipe (v kB)
#define __COMMUNICATION_WAIT_TIMEOUT 5000

//****************************************************************************
//
// CClientServerInitData
//
// tato struktura se pri zahajeni komunikace predava od clienta do serveru

struct C__ClientServerInitData
{
    int Version;
    DWORD ClientOrServerProcessId;
    HANDLE HReadOrWritePipe;
    HANDLE HPipeSemaphore;
};

#define __SIZEOF_CLIENTSERVERINITDATA 16

//****************************************************************************
//
// CPipeDataHeader
//
// pomoci teto struktury komunikuje client se serverem pres pipu

// Pro Type == __mtInformation || Type == __mtError
// maji promenne tyto vyznamy:
struct C__PipeDataHeader
{
    int Type;                // typ zpravy (C__MessageType)
    DWORD ThreadID;          // pro upresneni jeste ID threadu
    DWORD UniqueThreadID;    // unikatni cislo threadu (systemove ID se opakuji)
    SYSTEMTIME Time;         // cas vzniku message
    DWORD MessageSize;       // delka bufferu potrebneho pro prijem textu
    DWORD MessageTextOffset; // offset textu ve spolecnem bufferu s filem
    DWORD Line;              // cislo radky
    double Counter;          // presne pocitadlo v ms
};

#define __SIZEOF_PIPEDATAHEADER 48

// Pro Type == __mtSetProcessName
// C__MessageType Type;              // typ zpravy
// DWORD          MessageSize        // delka bufferu potrebneho pro prijem nazvu

// Pro Type == __mtSetThreadName
// C__MessageType Type;              // typ zpravy
// DWORD          UniqueThreadID;    // Unique Thread ID
// DWORD          MessageSize        // delka bufferu potrebneho pro prijem nazvu

// Pro Type == __mtIgnoreAutoClear
// C__MessageType Type;              // typ zpravy
// DWORD      ThreadID;              // 0 = neignorovat, 1 = ignorovat auto-clear na Trace Serveru

// aktualni verze clientu (porovnava se s verzi serveru)
#define TRACE_CLIENT_VERSION 7

#endif // defined(__TRACESERVER) || defined(TRACE_ENABLE)

#ifndef TRACE_ENABLE

// aby nedochazelo k problemum se stredniky v nize nadefinovanych makrech
inline void __TraceEmptyFunction() {}

#define TRACE_MI(file, line, str) __TraceEmptyFunction()
#define TRACE_MIW(file, line, str) __TraceEmptyFunction()
#define TRACE_I(str) __TraceEmptyFunction()
#define TRACE_IW(str) __TraceEmptyFunction()
#define TRACE_W(str) __TraceEmptyFunction()
#define TRACE_WW(str) __TraceEmptyFunction()
#define TRACE_ME(file, line, str) __TraceEmptyFunction()
#define TRACE_MEW(file, line, str) __TraceEmptyFunction()
#define TRACE_E(str) __TraceEmptyFunction()
#define TRACE_EW(str) __TraceEmptyFunction()
// pri crashi softu pres DebugBreak() nejde vystopovat, kde lezi volani
// TRACE_C/TRACE_MC, protoze adresa exceptiony je kdesi v ntdll.dll
// a sekce Stack Back Trace bug reportu muze obsahovat nesmysly, pokud
// funkce volajici TRACE_C/TRACE_MC nepouziva stary jednoduchy model
// ukladani a prace s EBP/ESP, ovsem i v tom pripade je zde jen adresa
// odkud byla tato funkce volana (ne primo adresa TRACE_C/TRACE_MC),
// proto aspon prozatim pouzivame stary primitivni zpusob crashe
// zapisem na NULL
//#define TRACE_MC(file, line, str) DebugBreak()
//#define TRACE_MCW(file, line, str) DebugBreak()
//#define TRACE_C(str) DebugBreak()
//#define TRACE_CW(str) DebugBreak()
#define TRACE_MC(file, line, str) (*((int*)NULL) = 0x666)
#define TRACE_MCW(file, line, str) (*((int*)NULL) = 0x666)
#define TRACE_C(str) (*((int*)NULL) = 0x666)
#define TRACE_CW(str) (*((int*)NULL) = 0x666)
#define ConnectToTraceServer() __TraceEmptyFunction()
#define IsConnectedToTraceServer() FALSE
#define DisconnectFromTraceServer() __TraceEmptyFunction()
#ifdef TRACE_TO_FILE
#define CloseTraceMsgsFile() __TraceEmptyFunction()
#endif // TRACE_TO_FILE
#define SetTraceProcessName(name) __TraceEmptyFunction()
#define SetTraceProcessNameW(name) __TraceEmptyFunction()
#define SetTraceThreadName(name) __TraceEmptyFunction()
#define SetTraceThreadNameW(name) __TraceEmptyFunction()

#else // TRACE_ENABLE

#ifdef MULTITHREADED_TRACE_ENABLE

HANDLE __TRACECreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                           DWORD dwStackSize,
                           LPTHREAD_START_ROUTINE lpStartAddress,
                           LPVOID lpParameter, DWORD dwCreationFlags,
                           LPDWORD lpThreadId);

uintptr_t __TRACE_beginthreadex(void* security, unsigned stack_size,
                                unsigned(__stdcall* start_address)(void*),
                                void* arglist, unsigned initflag,
                                unsigned* thrdid);

#define CreateThread __TRACECreateThread
#define _beginthreadex __TRACE_beginthreadex
#define _beginthread TRACE_doesnt_support_beginthread__use_beginthreadex

#endif // MULTITHREADED_TRACE_ENABLE

// info-trace, manualne zadana pozice v souboru
#define TRACE_MI(file, line, str) \
    (::EnterCriticalSection(&__Trace.CriticalSection), __Trace.StoreLastError(), \
     __Trace.OStream() << str, __Trace) \
        .SetInfo(file, line) \
        .SendMessageToServer(__mtInformation) \
        .RestoreLastError()

#define TRACE_MIW(file, line, str) \
    (::EnterCriticalSection(&__Trace.CriticalSection), __Trace.StoreLastError(), \
     __Trace.OStreamW() << str, __Trace) \
        .SetInfoW(file, line) \
        .SendMessageToServer(__mtInformationW) \
        .RestoreLastError()

// info-trace
#define TRACE_I(str) TRACE_MI(__FILE__, __LINE__, str)
#define TRACE_IW(str) TRACE_MIW(__WFILE__, __LINE__, str)

// warning-trace (obsolete)
#define TRACE_W(str) TRACE_I(str)
#define TRACE_WW(str) TRACE_IW(str)

// error-trace, manualne zadana pozice v souboru
#define TRACE_ME(file, line, str) \
    (::EnterCriticalSection(&__Trace.CriticalSection), __Trace.StoreLastError(), \
     __Trace.OStream() << str, __Trace) \
        .SetInfo(file, line) \
        .SendMessageToServer(__mtError) \
        .RestoreLastError()

#define TRACE_MEW(file, line, str) \
    (::EnterCriticalSection(&__Trace.CriticalSection), __Trace.StoreLastError(), \
     __Trace.OStreamW() << str, __Trace) \
        .SetInfoW(file, line) \
        .SendMessageToServer(__mtErrorW) \
        .RestoreLastError()

// error-trace
#define TRACE_E(str) TRACE_ME(__FILE__, __LINE__, str)
#define TRACE_EW(str) TRACE_MEW(__WFILE__, __LINE__, str)

// fatal-error-trace (CRASHING TRACE), manualne zadana pozice v souboru;
// zastavime soft v debuggeru, pro snadne odladeni problemu, ktery prave vznikl,
// release verze spadne a problem snad bude jasny z call-stacku v bug-reportu;
// nepouzivame DebugBreak(), protoze pri crashi softu nejde vystopovat, kde
// lezi volani DebugBreak(), protoze adresa exceptiony je kdesi v ntdll.dll
// a sekce Stack Back Trace bug reportu muze obsahovat nesmysly, pokud
// funkce, ze ktere volame TRACE_C/MC, nepouziva stary jednoduchy model ukladani
// a prace s EBP/ESP (to zalezi na kompileru a zaplych optimalizacich), proto
// aspon prozatim pouzivame stary primitivni zpusob crashe zapisem na NULL
#define TRACE_MC(file, line, str) \
    ((::EnterCriticalSection(&__Trace.CriticalSection), __Trace.StoreLastError(), \
      __Trace.OStream() << str, __Trace) \
         .SetInfo(file, line) \
         .SendMessageToServer(__mtError, TRUE) \
         .RestoreLastError(), \
     *((int*)NULL) = 0x666)

#define TRACE_MCW(file, line, str) \
    ((::EnterCriticalSection(&__Trace.CriticalSection), __Trace.StoreLastError(), \
      __Trace.OStreamW() << str, __Trace) \
         .SetInfoW(file, line) \
         .SendMessageToServer(__mtErrorW, TRUE) \
         .RestoreLastError(), \
     *((int*)NULL) = 0x666)

// fatal-error-trace (CRASHING TRACE)
#define TRACE_C(str) TRACE_MC(__FILE__, __LINE__, str)
#define TRACE_CW(str) TRACE_MCW(__WFILE__, __LINE__, str)

#define ConnectToTraceServer() __Trace.Connect(TRUE)
#define IsConnectedToTraceServer() __Trace.IsConnected()
#define DisconnectFromTraceServer() __Trace.Disconnect()
#ifdef TRACE_TO_FILE
#define CloseTraceMsgsFile() __Trace.CloseTraceFile()
#endif // TRACE_TO_FILE

#define SetTraceProcessName(name) __Trace.SetProcessName(name)
#define SetTraceProcessNameW(name) __Trace.SetProcessNameW(name)
#define SetTraceThreadName(name) __Trace.SetThreadName(name)
#define SetTraceThreadNameW(name) __Trace.SetThreadNameW(name)

#ifdef MULTITHREADED_TRACE_ENABLE

#define __TRACE_CACHE_SIZE 16
inline int __TraceCacheGetIndex(DWORD tid)
{
    return tid & 0x0f;
}

struct C__TraceCacheData
{
    HANDLE Handle;
    DWORD TID;
    DWORD UID;
};

class C__TraceThreadCache
{
protected:
    C__TraceCacheData* Data;
    int Available;
    int Count;

    int UniqueThreadID;

    DWORD CacheTID[__TRACE_CACHE_SIZE];
    DWORD CacheUID[__TRACE_CACHE_SIZE]; // hodnota -1 -> invalidni zaznam

public:
    C__TraceThreadCache();
    ~C__TraceThreadCache();

    BOOL Add(HANDLE handle, DWORD tid);
    DWORD GetUniqueThreadId(DWORD tid);

    BOOL EnlargeArray();
    BOOL Move(int direction, DWORD first, DWORD count);

    inline BOOL GetIndex(DWORD tid, int& index);
};

#endif // MULTITHREADED_TRACE_ENABLE

class C__Trace
{
public:
    CRITICAL_SECTION CriticalSection;
#ifdef MULTITHREADED_TRACE_ENABLE
    C__TraceThreadCache ThreadCache;
#endif // MULTITHREADED_TRACE_ENABLE

protected:
    HANDLE HWritePipe;                  // zapisovy konec pipe
    HANDLE HPipeSemaphore;              // slouzi pro alokaci mista v pipe (1x wait = 1kB)
    DWORD BytesAllocatedForWriteToPipe; // kolik mista pro zapis je prave naalokovano v pipe

#ifdef TRACE_TO_FILE
    HANDLE HTraceFile;                  // soubor otevreny pro zapis v TEMPu, ukladaji se do nej vsechny message
#ifdef __TRACESERVER
    WCHAR TraceFileName[MAX_PATH];      // jmeno souboru HTraceFile
#endif // __TRACESERVER
#endif // TRACE_TO_FILE

    LARGE_INTEGER StartPerformanceCounter; // pro presny counter - uvodni hodnota
    LARGE_INTEGER PerformanceFrequency;    // pro presny counter
    BOOL SupportPerformanceFrequency;

    const char* File;                    // pomocne promenne pro predani jmena souboru (ANSI)
    const WCHAR* FileW;                  // pomocne promenne pro predani jmena souboru (unicode)
    int Line;                            // a cisla radku, odkud se vola TRACE_X()
    C__StringStreamBuf TraceStringBuf;   // string buffer drzici data trace streamu (ANSI)
    C__StringStreamBufW TraceStringBufW; // string buffer drzici data trace streamu (unicode)
    C__TraceStream TraceStrStream;       // vlastni trace stream (ANSI)
    C__TraceStreamW TraceStrStreamW;     // vlastni trace stream (unicode)
    DWORD StoredLastError;               // GetLastError() pred TRACE_? makrem

public:
    C__Trace();
    ~C__Trace();

    BOOL Connect(BOOL onUserRequest);
    BOOL IsConnected() { return HWritePipe != NULL; }
    void Disconnect();
#ifdef TRACE_TO_FILE
    void CloseTraceFile();
#endif // TRACE_TO_FILE

    void SetProcessName(const char* name);
    void SetProcessNameW(const WCHAR* name);
    void SetThreadName(const char* name);
    void SetThreadNameW(const WCHAR* name);

    C__Trace& SetInfo(const char* file, int line);
    C__Trace& SetInfoW(const WCHAR* file, int line);

    void StoreLastError() { StoredLastError = GetLastError(); }
    void RestoreLastError() { SetLastError(StoredLastError); }

    BOOL WritePipe(LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite);

    C__TraceStream& OStream() { return TraceStrStream; }
    C__TraceStreamW& OStreamW() { return TraceStrStreamW; }
    C__Trace& SendMessageToServer(C__MessageType type, BOOL crash = FALSE);

protected:
    void SendSetNameMessageToServer(const char* name, const WCHAR* nameW, C__MessageType type);
    void CloseWritePipeAndSemaphore();
    BOOL SendIgnoreAutoClear(BOOL ignore);
};

extern C__Trace __Trace;

#endif // TRACE_ENABLE

#define TRACE_MIT TRACE_MI

#ifndef UNICODE

#define TRACE_IT TRACE_I
#define TRACE_WT TRACE_W
#define TRACE_MET TRACE_ME
#define TRACE_ET TRACE_E
#define TRACE_MCT TRACE_MC
#define TRACE_CT TRACE_C
#define SetTraceProcessNameT SetTraceProcessName
#define SetTraceThreadNameT SetTraceThreadName

#else // UNICODE

#define TRACE_IT TRACE_IW
#define TRACE_WT TRACE_WW
#define TRACE_MET TRACE_MEW
#define TRACE_ET TRACE_EW
#define TRACE_MCT TRACE_MCW
#define TRACE_CT TRACE_CW
#define SetTraceProcessNameT SetTraceProcessNameW
#define SetTraceThreadNameT SetTraceThreadNameW

#endif // UNICODE
