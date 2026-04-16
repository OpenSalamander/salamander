// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// macro TRACE_ENABLE - enables sending messages to the server
// macro MULTITHREADED_TRACE_ENABLE - enables remapping TID to UTID
// macro TRACE_TO_FILE - enables writing messages to a file in TEMP (requires TRACE_ENABLE to be defined)
// macro TRACE_IGNORE_AUTOCLEAR - prevents Trace Server from clearing all messages when this process connects,
//                                even if that is enabled in the settings (useful for utilities started while the main
//                                program is running, where clearing messages is undesirable)
// macro __TRACESERVER - included from trace-server

// TRACE module is ready for multi-threaded applications

// WARNING: TRACE_C must not be used in a library's DllMain, or in any code that
//        is called from DllMain, otherwise a deadlock will occur; see the
//        implementation of C__Trace::SendMessageToServer

#if defined(__TRACESERVER) || defined(TRACE_ENABLE)

enum C__MessageType
{
    // message type
    __mtInformation,
    __mtError,

    // setting the process / thread name
    __mtSetProcessName,
    __mtSetThreadName,

    // message type - Unicode message variants
    __mtInformationW,
    __mtErrorW,

    // setting the process / thread name - Unicode message variants
    __mtSetProcessNameW,
    __mtSetThreadNameW,

    // prevents Trace Server from clearing all messages when this process connects, even if that is
    // enabled in the settings (useful for utilities started while the main program is running, where
    // clearing messages is undesirable)
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

#define __PIPE_SIZE 100 // maximum data in the pipe (in kB)
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
// This structure is used by the client and server to communicate through the pipe

// For Type == __mtInformation || Type == __mtError
// the variables have the following meanings:
struct C__PipeDataHeader
{
    int Type;                // message type (C__MessageType)
    DWORD ThreadID;          // thread ID for additional identification
    DWORD UniqueThreadID;    // unique thread number (system IDs are reused)
    SYSTEMTIME Time;         // message creation time
    DWORD MessageSize;       // length of the buffer needed to receive the text
    DWORD MessageTextOffset; // offset of the text in the buffer shared with the file name
    DWORD Line;              // line number
    double Counter;          // high-resolution counter in ms
};

#define __SIZEOF_PIPEDATAHEADER 48

// For Type == __mtSetProcessName
// C__MessageType Type;              // message type
// DWORD          MessageSize        // length of the buffer needed to receive the name

// Pro Type == __mtSetThreadName
// C__MessageType Type;              // message type
// DWORD          UniqueThreadID;    // Unique Thread ID
// DWORD          MessageSize        // length of the buffer needed to receive the name

// Pro Type == __mtIgnoreAutoClear
// C__MessageType Type;              // message type
// DWORD      ThreadID;              // 0 = neignorovat, 1 = ignorovat auto-clear na Trace Serveru

// current client version (compared with the server version)
#define TRACE_CLIENT_VERSION 7

#endif // defined(__TRACESERVER) || defined(TRACE_ENABLE)

#ifndef TRACE_ENABLE

// to avoid problems with semicolons in the macros defined below
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
// when the program crashes through DebugBreak(), it is impossible to trace where
// TRACE_C/TRACE_MC was called, because the exception address ends up somewhere in ntdll.dll,
// and the Stack Back Trace section of the bug report may contain nonsense if
// the function calling TRACE_C/TRACE_MC does not use the old simple model for
// saving and using EBP/ESP; even then, only the address from which
// that function was called is available (not the TRACE_C/TRACE_MC address itself),
// so at least for now we use the old primitive way of crashing
// by writing to NULL
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

// info-trace, manually specified file position
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

// error-trace, manually specified file position
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

// fatal-error-trace (CRASHING TRACE), manually specified file location;
// stop the program in the debugger to make the problem that just occurred easier to debug;
// the release build crashes, and the problem will hopefully be clear from the call stack in the bug report;
// we do not use DebugBreak(), because when the program crashes through DebugBreak(), it is impossible to trace where
// TRACE_C/MC was called: the exception address ends up somewhere in ntdll.dll,
// and the Stack Back Trace section of the bug report may contain nonsense if
// the function that calls TRACE_C/MC does not use the old simple model for
// saving and using EBP/ESP (this depends on the compiler and enabled optimizations), so
// at least for now we use the old primitive way of crashing by writing to NULL
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
    DWORD CacheUID[__TRACE_CACHE_SIZE]; // value -1 -> invalid record

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
    HANDLE HWritePipe;                  // write end of the pipe
    HANDLE HPipeSemaphore;              // used to allocate space in the pipe (1x wait = 1 kB)
    DWORD BytesAllocatedForWriteToPipe; // amount of write space currently allocated in the pipe

#ifdef TRACE_TO_FILE
    HANDLE HTraceFile; // file opened for writing in TEMP; all messages are stored there
#ifdef __TRACESERVER
    WCHAR TraceFileName[MAX_PATH]; // HTraceFile file name
#endif // TRACE_TO_FILE
#endif // TRACE_TO_FILE

    LARGE_INTEGER StartPerformanceCounter; // initial value of the high-resolution counter
    LARGE_INTEGER PerformanceFrequency;    // high-resolution counter frequency
    BOOL SupportPerformanceFrequency;

    const char* File;                    // helper variables for passing the file name (ANSI)
    const WCHAR* FileW;                  // helper variables for passing the file name (Unicode)
    int Line;                            // and the line number from which TRACE_X() is called
    C__StringStreamBuf TraceStringBuf;   // string buffer drzici data trace streamu (ANSI)
    C__StringStreamBufW TraceStringBufW; // string buffer drzici data trace streamu (unicode)
    C__TraceStream TraceStrStream;       // vlastni trace stream (ANSI)
    C__TraceStreamW TraceStrStreamW;     // vlastni trace stream (unicode)
    DWORD StoredLastError;               // GetLastError() value before the TRACE_? macro

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
