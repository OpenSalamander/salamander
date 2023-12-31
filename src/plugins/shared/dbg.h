﻿// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

// definice maker TRACE_I, TRACE_IW, TRACE_E, TRACE_EW, TRACE_C, TRACE_CW a CALL_STACK_MESSAGEXXX pro pluginy,
// v pluginu je treba definovat promennou SalamanderDebug (typ viz nize) a
// v SalamanderPluginEntry tuto promennou inicializovat:
// SalamanderDebug = salamander->GetSalamanderDebug();
//
// TRACE se zapina definici makra TRACE_ENABLE
// CALL-STACK se vypina definici makra CALLSTK_DISABLE

// POZOR: TRACE_C se nesmi pouzivat v DllMain knihoven, ani v zadnem kodu, ktery
//        se z DllMainu vola, jinak dojde k deadlocku, vice viz implementace
//        C__Trace::SendMessageToServer

// makro CALLSTK_MEASURETIMES - zapne mereni casu straveneho pri priprave call-stack hlaseni (meri se pomer proti
//                              celkovemu casu behu funkci)
//                              POZOR: nutne zapnout tez pro kazdy plugin zvlast
// makro CALLSTK_DISABLEMEASURETIMES - potlaci mereni casu straveneho pri priprave call-stack hlaseni v DEBUG verzi

// prehled typu maker: (vsechna jsou neprazdna jen pokud neni definovano CALLSTK_DISABLE)
// CALL_STACK_MESSAGE - bezne call-stack makro
// SLOW_CALL_STACK_MESSAGE - call-stack makro, u ktereho se ignoruje libovolne zpomaleni kodu (pouziti
//                           na mistech, kde vime, ze vyrazne zpomaluje kod, ale presto ho na tomto
//                           miste potrebujeme)
// DEBUG_SLOW_CALL_STACK_MESSAGE - call-stack makro, ktere je v release verzi prazdne, v DEBUG verzi
//                                 se chova jako SLOW_CALL_STACK_MESSAGE (pouziti na mistech, kde jsme
//                                 ochotni ignorovat zpomaleni jen v debug verzi, release verze je rychla)

// globalni promenna s interfacem CSalamanderDebugAbstract
extern CSalamanderDebugAbstract* SalamanderDebug;

// kopie z spl_com.h: globalni promenna s verzi Salamandera, ve kterem je tento plugin nacteny
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
#define SetTraceThreadName(name) __TraceEmptyFunction()
#define SetTraceThreadNameW(name) __TraceEmptyFunction()

#else // TRACE_ENABLE

// info-trace, manualne zadana pozice v souboru
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

// error-trace, manualne zadana pozice v souboru
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
    const char* File;                    // pomocne promenne pro predani jmena souboru (ANSI)
    const WCHAR* FileW;                  // pomocne promenne pro predani jmena souboru (unicode)
    int Line;                            // a cisla radku, odkud se vola TRACE_X()
    C__StringStreamBuf TraceStringBuf;   // string buffer drzici data trace streamu (ANSI)
    C__StringStreamBufW TraceStringBufW; // string buffer drzici data trace streamu (unicode)
    C__TraceStream TraceStrStream;       // vlastni trace stream (ANSI)
    C__TraceStreamW TraceStrStreamW;     // vlastni trace stream (unicode)

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
    // 'doNotMeasureTimes'==TRUE = nemerit Push tohoto call-stack makra (zrejme dost zpomaluje, ale
    // nechceme ho vyhodit, je prilis dulezite pro debugovani)
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
#define CALLSTK_UNIQUE(varname) CALLSTK_TOKEN2(varname, __LINE__) // __COUNTER__ je MSVC only
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

// prazdne makro: oznamuje CheckStk, ze u teto funkce si call-stack message neprejeme
#define CALL_STACK_MESSAGE_NONE

//
// ****************************************************************************
// TraceAttachCurrentThread + SetThreadNameInVCAndTrace
//

// nepouzivat pokud uz se SalamanderDebug->TraceAttachThread pro tento thread volalo (primo
// nebo napr. pri startu threadu pres CThreadQueue::StartThread)
inline void TraceAttachCurrentThread() { SalamanderDebug->TraceAttachThread(GetCurrentThread(), GetCurrentThreadId()); }

inline void SetThreadNameInVCAndTrace(const char* name) { SalamanderDebug->SetThreadNameInVCAndTrace(name); }
