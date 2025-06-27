// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED
//
//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

// definitions of macros TRACE_I, TRACE_IW, TRACE_E, TRACE_EW, TRACE_C, TRACE_CW a CALL_STACK_MESSAGEXXX for plugins,
// in plugin you have to define variable SalamanderDebug (type see below) and
// in SalamanderPluginEntry initialize this variable:
// SalamanderDebug = salamander->GetSalamanderDebug();
//
// TRACE is enabled by defining TRACE_ENABLE macro
// CALL-STACK is disabled by defining CALLSTK_DISABLE macro

// CAUTION: TRACE_C can't be used in DllMain of libraries, nor in any code called from DllMain,
//         otherwise deadlock occurs, see implementation of C__Trace::SendMessageToServer

// macro CALLSTK_MEASURETIMES - enables measuring of time spent by preparing call-stack messages (ratio against
//                              total time spent by functions is measured)
//                              CAUTION: must be enabled for each plugin separately
// macro CALLSTK_DISABLEMEASURETIMES - supresses measuring of time spent by preparing call-stack messages in DEBUG version

// overview of macro types: (all are non-empty only if CALLSTK_DISABLE is not defined)
// CALL_STACK_MESSAGE - normal call-stack macro
// SLOW_CALL_STACK_MESSAGE - call-stack macro, which ignores any slowdown of code (use on places, where we know
//                           that it slows down code, but we need it on this place)
// DEBUG_SLOW_CALL_STACK_MESSAGE - call-stack macro, which is empty in release version, in DEBUG version it behaves
//                                 as SLOW_CALL_STACK_MESSAGE (use on places, where we are willing to ignore
//                                 slowdown only in debug version, release version is fast)

// global variable with interface CSalamanderDebugAbstract
extern CSalamanderDebugAbstract* SalamanderDebug;

// a copy from spl_com.h: global variable with version of Salamander, in which this plugin is loaded
extern int SalamanderVersion;

#ifndef __WFILE__
#define __WFILE__WIDEN2(x) L##x
#define __WFILE__WIDEN(x) __WFILE__WIDEN2(x)
#define __WFILE__ __WFILE__WIDEN(__FILE__)
#endif // __WFILE__

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
#ifdef __BORLANDC__
                traits_type::copy(ptr, pbase(), oldsize);
#else  // __BORLANDC__
                traits_type::_Copy_s(ptr, newsize, pbase(), oldsize);
#endif // __BORLANDC__
                GlobalFree(pbase());
            }

            // update pointers
            setp(ptr, ptr + newsize);
            pbump((int)oldsize); // FIXME_X64 - je pretypovani na (int) OK?
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
#ifdef __BORLANDC__
                traits_type::copy(ptr, pbase(), oldsize);
#else  // __BORLANDC__
                traits_type::_Copy_s(ptr, newsize, pbase(), oldsize);
#endif // __BORLANDC__
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

    std::ostream& operator<<(bool v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(char v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(unsigned char v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(short v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(unsigned short v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(int v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(unsigned int v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(long v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(unsigned long v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(__int64 v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(unsigned __int64 v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(float v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(double v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(long double v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(const void* v) { return static_cast<std::ostream&>(*this) << v; }
    std::ostream& operator<<(const char* v) { return static_cast<std::ostream&>(*this) << v; }
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

    std::wostream& operator<<(bool v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(char v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(unsigned char v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(wchar_t v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(short v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(unsigned short v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(int v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(unsigned int v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(long v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(unsigned long v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(__int64 v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(unsigned __int64 v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(float v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(double v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(long double v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(const void* v) { return static_cast<std::wostream&>(*this) << v; }
    std::wostream& operator<<(const wchar_t* v) { return static_cast<std::wostream&>(*this) << v; }
};

// NOT SUPPORTED (just to produce link error)
// if error occurs, it may be invalid combination of WCHAR / char usage in TRACE macros
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

//
// ****************************************************************************
// TRACE
//

#ifndef TRACE_ENABLE

// so that no issues with semicolons occur in the macros defined below
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
// when the software is crashed by DebugBreak(), it is not possible to trace
// where the call to TRACE_C/TRACE_MC is located, because the address of the
// exception is somewhere in ntdll.dll and the Stack Back Trace section of
// the bug report may contain nonsense, if the function calling TRACE_C/TRACE_MC
// does not use the old simple model of saving and working with EBP/ESP, but
// even in that case there is only the address from which this function was
// called (not directly the address of TRACE_C/TRACE_MC), therefore we use
// the old primitive way of crashing by writing to NULL
//#define TRACE_MC(file, line, str) DebugBreak()
//#define TRACE_MCW(file, line, str) DebugBreak()
//#define TRACE_C(str) DebugBreak()
//#define TRACE_CW(str) DebugBreak()
#define TRACE_MC(file, line, str) (*((int*)NULL) = 0x666)
#define TRACE_MCW(file, line, str) (*((int*)NULL) = 0x666)
#define TRACE_C(str) (*((int*)NULL) = 0x666)
#define TRACE_CW(str) (*((int*)NULL) = 0x666)
#define ConnectToTraceServer() __TraceEmptyFunction()
#define SetTraceThreadName(name) __TraceEmptyFunction()
#define SetTraceThreadNameW(name) __TraceEmptyFunction()

#else // TRACE_ENABLE

// info-trace, manually specified position in file
#define TRACE_MI(file, line, str) \
    (::EnterCriticalSection(&__Trace.CriticalSection), __Trace.OStream() << str, \
     __Trace) \
        .SetInfo(file, line) \
        .SendMessageToServer(TRUE)

#define TRACE_MIW(file, line, str) \
    (::EnterCriticalSection(&__Trace.CriticalSection), __Trace.OStreamW() << str, \
     __Trace) \
        .SetInfoW(file, line) \
        .SendMessageToServer(TRUE, TRUE)

// info-trace
#define TRACE_I(str) TRACE_MI(__FILE__, __LINE__, str)
#define TRACE_IW(str) TRACE_MIW(__WFILE__, __LINE__, str)

// warning-trace (obsolete)
#define TRACE_W(str) TRACE_I(str)
#define TRACE_WW(str) TRACE_IW(str)

// error-trace, manually specified position in file
#define TRACE_ME(file, line, str) \
    (::EnterCriticalSection(&__Trace.CriticalSection), __Trace.OStream() << str, \
     __Trace) \
        .SetInfo(file, line) \
        .SendMessageToServer(FALSE)

#define TRACE_MEW(file, line, str) \
    (::EnterCriticalSection(&__Trace.CriticalSection), __Trace.OStreamW() << str, \
     __Trace) \
        .SetInfoW(file, line) \
        .SendMessageToServer(FALSE, TRUE)

// error-trace
#define TRACE_E(str) TRACE_ME(__FILE__, __LINE__, str)
#define TRACE_EW(str) TRACE_MEW(__WFILE__, __LINE__, str)

// fatal-error-trace (CRASHING TRACE), manually specified position in file
// stop the software in debugger, for easy debugging of the problem that just
// occurred, release version will crash and the problem will be clear from the
// call stack in the bug report; we do not use DebugBreak(), because it can't be
// traced where the call to DebugBreak() is located, because the address of the
// exception is somewhere in ntdll.dll and the Stack Back Trace section of
// the bug report may contain nonsense, if the function calling TRACE_C/TRACE_MC
// does not use the old simple model of saving and working with EBP/ESP (it depends
// on the compiler and enabled optimizations), therefore we use the old primitive
// way of crashing by writing to NULL
#define TRACE_MC(file, line, str) \
    ((::EnterCriticalSection(&__Trace.CriticalSection), __Trace.OStream() << str, \
      __Trace) \
         .SetInfo(file, line) \
         .SendMessageToServer(FALSE, FALSE, TRUE), \
     *((int*)NULL) = 0x666)

#define TRACE_MCW(file, line, str) \
    ((::EnterCriticalSection(&__Trace.CriticalSection), __Trace.OStreamW() << str, \
      __Trace) \
         .SetInfoW(file, line) \
         .SendMessageToServer(FALSE, TRUE, TRUE), \
     *((int*)NULL) = 0x666)

// fatal-error-trace (CRASHING TRACE)
#define TRACE_C(str) TRACE_MC(__FILE__, __LINE__, str)
#define TRACE_CW(str) TRACE_MCW(__WFILE__, __LINE__, str)

#define ConnectToTraceServer() SalamanderDebug->TraceConnectToServer()

#define SetTraceThreadName(name) SalamanderDebug->TraceSetThreadName(name)
#define SetTraceThreadNameW(name) SalamanderDebug->TraceSetThreadNameW(name)

class C__Trace
{
public:
    CRITICAL_SECTION CriticalSection;

protected:
    const char* File;                    // helper variables for passing file name (ANSI)
    const WCHAR* FileW;                  // helper variables for passing file name (unicode)
    int Line;                            // and the line number, where TRACE_X() is called
    C__StringStreamBuf TraceStringBuf;   // string buffer containing data of trace stream (ANSI)
    C__StringStreamBufW TraceStringBufW; // string buffer containing data of trace stream (unicode)
    C__TraceStream TraceStrStream;       // trace stream (ANSI)
    C__TraceStreamW TraceStrStreamW;     // trace stream (unicode)

public:
    C__Trace();
    ~C__Trace();

    C__Trace& SetInfo(const char* file, int line);
    C__Trace& SetInfoW(const WCHAR* file, int line);

    C__TraceStream& OStream() { return TraceStrStream; }
    C__TraceStreamW& OStreamW() { return TraceStrStreamW; }
    void SendMessageToServer(BOOL information, BOOL unicode = FALSE, BOOL crash = FALSE);
};

extern C__Trace __Trace;

#endif // TRACE_ENABLE

//
// ****************************************************************************
// CALL-STACK
//

#ifndef CALLSTK_DISABLE

class CCallStackMessage
{
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
protected:
    CCallStackMsgContext CallStkMsgContext;
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

public:
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    // 'doNotMeasureTimes'==TRUE = do not measure Push of this call-stack macro (probably slows down, but
    // we don't want to throw it away, it is too important for debugging)
#ifdef __BORLANDC__
    CCallStackMessage(BOOL doNotMeasureTimes, const char* format, ...)
#else  // __BORLANDC__
    CCallStackMessage(BOOL doNotMeasureTimes, int dummy, const char* format, ...)
#endif // __BORLANDC__
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    CCallStackMessage(const char* format, ...)
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    {
        va_list args;
        va_start(args, format);
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        SalamanderDebug->Push(format, args, &CallStkMsgContext, doNotMeasureTimes);
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        SalamanderDebug->Push(format, args, NULL, TRUE);
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        va_end(args);
    }

    ~CCallStackMessage()
    {
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        SalamanderDebug->Pop(&CallStkMsgContext);
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        SalamanderDebug->Pop(NULL);
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    }
};

#define CALLSTK_TOKEN(x, y) x##y
#define CALLSTK_TOKEN2(x, y) CALLSTK_TOKEN(x, y)
#ifdef __BORLANDC__
#define CALLSTK_UNIQUE(varname) CALLSTK_TOKEN2(varname, __LINE__) // __COUNTER__ is MSVC only
#else                                                             // __BORLANDC__
#define CALLSTK_UNIQUE(varname) CALLSTK_TOKEN2(varname, __COUNTER__)
#endif // __BORLANDC__

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

#ifndef __BORLANDC__

extern BOOL __CallStk_T; // always TRUE - just to check format string and type of parameters of call-stack macros

#define CALL_STACK_MESSAGE1(p1) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, 0, p1)
#define CALL_STACK_MESSAGE2(p1, p2) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2)), p1, p2)
#define CALL_STACK_MESSAGE3(p1, p2, p3) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3)), p1, p2, p3)
#define CALL_STACK_MESSAGE4(p1, p2, p3, p4) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4)), p1, p2, p3, p4)
#define CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5)), p1, p2, p3, p4, p5)
#define CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6)), p1, p2, p3, p4, p5, p6)
#define CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7)), p1, p2, p3, p4, p5, p6, p7)
#define CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8)), p1, p2, p3, p4, p5, p6, p7, p8)
#define CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9)), p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#define SLOW_CALL_STACK_MESSAGE1(p1) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, 0, p1)
#define SLOW_CALL_STACK_MESSAGE2(p1, p2) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2)), p1, p2)
#define SLOW_CALL_STACK_MESSAGE3(p1, p2, p3) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3)), p1, p2, p3)
#define SLOW_CALL_STACK_MESSAGE4(p1, p2, p3, p4) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4)), p1, p2, p3, p4)
#define SLOW_CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5)), p1, p2, p3, p4, p5)
#define SLOW_CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6)), p1, p2, p3, p4, p5, p6)
#define SLOW_CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7)), p1, p2, p3, p4, p5, p6, p7)
#define SLOW_CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8)), p1, p2, p3, p4, p5, p6, p7, p8)
#define SLOW_CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9)), p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define SLOW_CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define SLOW_CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define SLOW_CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define SLOW_CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define SLOW_CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define SLOW_CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define SLOW_CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define SLOW_CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define SLOW_CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define SLOW_CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define SLOW_CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define SLOW_CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#else // __BORLANDC__

#define CALL_STACK_MESSAGE1(p1) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1)
#define CALL_STACK_MESSAGE2(p1, p2) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2)
#define CALL_STACK_MESSAGE3(p1, p2, p3) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3)
#define CALL_STACK_MESSAGE4(p1, p2, p3, p4) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4)
#define CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5)
#define CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6)
#define CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7)
#define CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8)
#define CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#define SLOW_CALL_STACK_MESSAGE1(p1) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1)
#define SLOW_CALL_STACK_MESSAGE2(p1, p2) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2)
#define SLOW_CALL_STACK_MESSAGE3(p1, p2, p3) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3)
#define SLOW_CALL_STACK_MESSAGE4(p1, p2, p3, p4) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4)
#define SLOW_CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5)
#define SLOW_CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6)
#define SLOW_CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7)
#define SLOW_CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8)
#define SLOW_CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define SLOW_CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define SLOW_CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define SLOW_CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define SLOW_CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define SLOW_CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define SLOW_CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define SLOW_CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define SLOW_CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define SLOW_CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define SLOW_CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define SLOW_CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define SLOW_CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#endif // __BORLANDC__

#else // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

#define CALL_STACK_MESSAGE1(p1) CCallStackMessage CALLSTK_UNIQUE(_m)(p1)
#define CALL_STACK_MESSAGE2(p1, p2) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2)
#define CALL_STACK_MESSAGE3(p1, p2, p3) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3)
#define CALL_STACK_MESSAGE4(p1, p2, p3, p4) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4)
#define CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5)
#define CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6)
#define CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7)
#define CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8)
#define CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#define SLOW_CALL_STACK_MESSAGE1 CALL_STACK_MESSAGE1
#define SLOW_CALL_STACK_MESSAGE2 CALL_STACK_MESSAGE2
#define SLOW_CALL_STACK_MESSAGE3 CALL_STACK_MESSAGE3
#define SLOW_CALL_STACK_MESSAGE4 CALL_STACK_MESSAGE4
#define SLOW_CALL_STACK_MESSAGE5 CALL_STACK_MESSAGE5
#define SLOW_CALL_STACK_MESSAGE6 CALL_STACK_MESSAGE6
#define SLOW_CALL_STACK_MESSAGE7 CALL_STACK_MESSAGE7
#define SLOW_CALL_STACK_MESSAGE8 CALL_STACK_MESSAGE8
#define SLOW_CALL_STACK_MESSAGE9 CALL_STACK_MESSAGE9
#define SLOW_CALL_STACK_MESSAGE10 CALL_STACK_MESSAGE10
#define SLOW_CALL_STACK_MESSAGE11 CALL_STACK_MESSAGE11
#define SLOW_CALL_STACK_MESSAGE12 CALL_STACK_MESSAGE12
#define SLOW_CALL_STACK_MESSAGE13 CALL_STACK_MESSAGE13
#define SLOW_CALL_STACK_MESSAGE14 CALL_STACK_MESSAGE14
#define SLOW_CALL_STACK_MESSAGE15 CALL_STACK_MESSAGE15
#define SLOW_CALL_STACK_MESSAGE16 CALL_STACK_MESSAGE16
#define SLOW_CALL_STACK_MESSAGE17 CALL_STACK_MESSAGE17
#define SLOW_CALL_STACK_MESSAGE18 CALL_STACK_MESSAGE18
#define SLOW_CALL_STACK_MESSAGE19 CALL_STACK_MESSAGE19
#define SLOW_CALL_STACK_MESSAGE20 CALL_STACK_MESSAGE20
#define SLOW_CALL_STACK_MESSAGE21 CALL_STACK_MESSAGE21

#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

#else // CALLSTK_DISABLE

#define CALL_STACK_MESSAGE1(p1)
#define CALL_STACK_MESSAGE2(p1, p2)
#define CALL_STACK_MESSAGE3(p1, p2, p3)
#define CALL_STACK_MESSAGE4(p1, p2, p3, p4)
#define CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5)
#define CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6)
#define CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7)
#define CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8)
#define CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#define SLOW_CALL_STACK_MESSAGE1 CALL_STACK_MESSAGE1
#define SLOW_CALL_STACK_MESSAGE2 CALL_STACK_MESSAGE2
#define SLOW_CALL_STACK_MESSAGE3 CALL_STACK_MESSAGE3
#define SLOW_CALL_STACK_MESSAGE4 CALL_STACK_MESSAGE4
#define SLOW_CALL_STACK_MESSAGE5 CALL_STACK_MESSAGE5
#define SLOW_CALL_STACK_MESSAGE6 CALL_STACK_MESSAGE6
#define SLOW_CALL_STACK_MESSAGE7 CALL_STACK_MESSAGE7
#define SLOW_CALL_STACK_MESSAGE8 CALL_STACK_MESSAGE8
#define SLOW_CALL_STACK_MESSAGE9 CALL_STACK_MESSAGE9
#define SLOW_CALL_STACK_MESSAGE10 CALL_STACK_MESSAGE10
#define SLOW_CALL_STACK_MESSAGE11 CALL_STACK_MESSAGE11
#define SLOW_CALL_STACK_MESSAGE12 CALL_STACK_MESSAGE12
#define SLOW_CALL_STACK_MESSAGE13 CALL_STACK_MESSAGE13
#define SLOW_CALL_STACK_MESSAGE14 CALL_STACK_MESSAGE14
#define SLOW_CALL_STACK_MESSAGE15 CALL_STACK_MESSAGE15
#define SLOW_CALL_STACK_MESSAGE16 CALL_STACK_MESSAGE16
#define SLOW_CALL_STACK_MESSAGE17 CALL_STACK_MESSAGE17
#define SLOW_CALL_STACK_MESSAGE18 CALL_STACK_MESSAGE18
#define SLOW_CALL_STACK_MESSAGE19 CALL_STACK_MESSAGE19
#define SLOW_CALL_STACK_MESSAGE20 CALL_STACK_MESSAGE20
#define SLOW_CALL_STACK_MESSAGE21 CALL_STACK_MESSAGE21

#endif // CALLSTK_DISABLE

#ifdef _DEBUG

#define DEBUG_SLOW_CALL_STACK_MESSAGE1 SLOW_CALL_STACK_MESSAGE1
#define DEBUG_SLOW_CALL_STACK_MESSAGE2 SLOW_CALL_STACK_MESSAGE2
#define DEBUG_SLOW_CALL_STACK_MESSAGE3 SLOW_CALL_STACK_MESSAGE3
#define DEBUG_SLOW_CALL_STACK_MESSAGE4 SLOW_CALL_STACK_MESSAGE4
#define DEBUG_SLOW_CALL_STACK_MESSAGE5 SLOW_CALL_STACK_MESSAGE5
#define DEBUG_SLOW_CALL_STACK_MESSAGE6 SLOW_CALL_STACK_MESSAGE6
#define DEBUG_SLOW_CALL_STACK_MESSAGE7 SLOW_CALL_STACK_MESSAGE7
#define DEBUG_SLOW_CALL_STACK_MESSAGE8 SLOW_CALL_STACK_MESSAGE8
#define DEBUG_SLOW_CALL_STACK_MESSAGE9 SLOW_CALL_STACK_MESSAGE9
#define DEBUG_SLOW_CALL_STACK_MESSAGE10 SLOW_CALL_STACK_MESSAGE10
#define DEBUG_SLOW_CALL_STACK_MESSAGE11 SLOW_CALL_STACK_MESSAGE11
#define DEBUG_SLOW_CALL_STACK_MESSAGE12 SLOW_CALL_STACK_MESSAGE12
#define DEBUG_SLOW_CALL_STACK_MESSAGE13 SLOW_CALL_STACK_MESSAGE13
#define DEBUG_SLOW_CALL_STACK_MESSAGE14 SLOW_CALL_STACK_MESSAGE14
#define DEBUG_SLOW_CALL_STACK_MESSAGE15 SLOW_CALL_STACK_MESSAGE15
#define DEBUG_SLOW_CALL_STACK_MESSAGE16 SLOW_CALL_STACK_MESSAGE16
#define DEBUG_SLOW_CALL_STACK_MESSAGE17 SLOW_CALL_STACK_MESSAGE17
#define DEBUG_SLOW_CALL_STACK_MESSAGE18 SLOW_CALL_STACK_MESSAGE18
#define DEBUG_SLOW_CALL_STACK_MESSAGE19 SLOW_CALL_STACK_MESSAGE19
#define DEBUG_SLOW_CALL_STACK_MESSAGE20 SLOW_CALL_STACK_MESSAGE20
#define DEBUG_SLOW_CALL_STACK_MESSAGE21 SLOW_CALL_STACK_MESSAGE21

#else _DEBUG

#define DEBUG_SLOW_CALL_STACK_MESSAGE1(p1)
#define DEBUG_SLOW_CALL_STACK_MESSAGE2(p1, p2)
#define DEBUG_SLOW_CALL_STACK_MESSAGE3(p1, p2, p3)
#define DEBUG_SLOW_CALL_STACK_MESSAGE4(p1, p2, p3, p4)
#define DEBUG_SLOW_CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5)
#define DEBUG_SLOW_CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6)
#define DEBUG_SLOW_CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7)
#define DEBUG_SLOW_CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8)
#define DEBUG_SLOW_CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define DEBUG_SLOW_CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define DEBUG_SLOW_CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define DEBUG_SLOW_CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define DEBUG_SLOW_CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define DEBUG_SLOW_CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define DEBUG_SLOW_CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define DEBUG_SLOW_CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define DEBUG_SLOW_CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define DEBUG_SLOW_CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define DEBUG_SLOW_CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define DEBUG_SLOW_CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define DEBUG_SLOW_CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#endif // _DEBUG

// empty macro: tells CheckStk that we don't want call-stack message for this function
#define CALL_STACK_MESSAGE_NONE

//
// ****************************************************************************
// TraceAttachCurrentThread + SetThreadNameInVCAndTrace
//

// do not use if SalamanderDebug->TraceAttachThread was already called for this thread (directly
// or e.g. when starting thread via CThreadQueue::StartThread)
inline void TraceAttachCurrentThread() { SalamanderDebug->TraceAttachThread(GetCurrentThread(), GetCurrentThreadId()); }

inline void SetThreadNameInVCAndTrace(const char* name) { SalamanderDebug->SetThreadNameInVCAndTrace(name); }
