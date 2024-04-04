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
//#include <commctrl.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif // _MSC_VER

// Suppressing warning C4996: This function or variable may be unsafe. Consider using strcat_s instead.
// Reason: lstrcat and other Windows routines are simply not safe, so it doesn't make sense to solve it here
#pragma warning(push)
#pragma warning(disable : 4996)

#ifdef __cplusplus
extern "C"
#endif
    LPSTR
    _sal_lstrcpyA(LPSTR lpString1, LPCSTR lpString2)
{
    return strcpy(lpString1, lpString2);
}

#ifdef __cplusplus
extern "C"
#endif
    LPWSTR
    _sal_lstrcpyW(LPWSTR lpString1, LPCWSTR lpString2)
{
    return wcscpy(lpString1, lpString2);
}

#ifdef __cplusplus
extern "C"
#endif
    LPSTR
    _sal_lstrcpynA(LPSTR lpString1, LPCSTR lpString2, int iMaxLength)
{
    if (iMaxLength <= 0)
        return lpString1;
    LPSTR ret = lpString1;
    LPSTR end = lpString1 + iMaxLength - 1;
    while (lpString1 < end && *lpString2 != 0)
        *lpString1++ = *lpString2++;
    *lpString1 = 0;
    return ret;
}

#ifdef __cplusplus
extern "C"
#endif
    LPWSTR
    _sal_lstrcpynW(LPWSTR lpString1, LPCWSTR lpString2, int iMaxLength)
{
    if (iMaxLength <= 0)
        return lpString1;
    LPWSTR ret = lpString1;
    LPWSTR end = lpString1 + iMaxLength - 1;
    while (lpString1 < end && *lpString2 != 0)
        *lpString1++ = *lpString2++;
    *lpString1 = 0;
    return ret;
}

#ifdef __cplusplus
extern "C"
#endif
    int
    _sal_lstrlenA(LPCSTR lpString)
{
    if (lpString == NULL)
        return 0;
    return (int)strlen(lpString);
}

#ifdef __cplusplus
extern "C"
#endif
    int
    _sal_lstrlenW(LPCWSTR lpString)
{
    if (lpString == NULL)
        return 0;
    return (int)wcslen(lpString);
}

#ifdef __cplusplus
extern "C"
#endif
    LPSTR
    _sal_lstrcatA(LPSTR lpString1, LPCSTR lpString2)
{
    return strcat(lpString1, lpString2);
}

#ifdef __cplusplus
extern "C"
#endif
    LPWSTR
    _sal_lstrcatW(LPWSTR lpString1, LPCWSTR lpString2)
{
    return wcscat(lpString1, lpString2);
}

#pragma warning(pop)

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES) && !defined(INSIDE_SALAMANDER) && !defined(__BORLANDC__)

BOOL __CallStk_T = TRUE;

#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES) && !defined(INSIDE_SALAMANDER) && !defined(__BORLANDC__)

#if defined(TRACE_ENABLE) && !defined(INSIDE_SALAMANDER)

#include <ostream>
#include <streambuf>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "spl_base.h"
#include "dbg.h"

#pragma warning(disable : 4074)
#pragma init_seg(compiler)

C__Trace __Trace;

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
    InitializeCriticalSection(&CriticalSection);
}

C__Trace::~C__Trace()
{
    DeleteCriticalSection(&CriticalSection);
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

#ifdef __BORLANDC__
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif // __BORLANDC__

DWORD WINAPI __TraceMsgBoxThread(void* param)
{
    C__TraceMsgBoxThreadData* data = (C__TraceMsgBoxThreadData*)param;
    char msg[1000];
    wsprintf(msg, "TRACE_C message received!\n\n"
                  "File: %s\n"
                  "Line: %d\n\n"
                  "Message: ",
             data->File, data->Line);
    const char* appendix = "\n\nTRACE_C message means that fatal error has occured. "
                           "Application will be crashed by \"access violation\" exception after "
                           "clicking OK. Please send us bug report to help us fix this problem. "
                           "If you want to copy this message to clipboard, use Ctrl+C key.";
    lstrcpyn(msg + (int)strlen(msg), data->Msg, _countof(msg) - (int)strlen(msg) - (int)strlen(appendix));
    lstrcpyn(msg + (int)strlen(msg), appendix, _countof(msg) - (int)strlen(msg));
    MessageBox(NULL, msg, "Debug Message", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
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
    wsprintfW(msg, L"TRACE_C message received!\n\n"
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

void C__Trace::SendMessageToServer(BOOL information, BOOL unicode, BOOL crash)
{
    // flushing to buffer
    if (unicode)
        TraceStrStreamW.flush();
    else
        TraceStrStream.flush();
    if (SalamanderDebug != NULL)
    {
        if (unicode)
        {
            if (information)
                SalamanderDebug->TraceIW(FileW, Line, TraceStringBufW.c_str());
            else
                SalamanderDebug->TraceEW(FileW, Line, TraceStringBufW.c_str());
        }
        else
        {
            if (information)
                SalamanderDebug->TraceI(File, Line, TraceStringBuf.c_str());
            else
                SalamanderDebug->TraceE(File, Line, TraceStringBuf.c_str());
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
    // If an exception occurs, other TRACE_C calls will be caught in an infinite waiting loop, see below
    static BOOL msgBoxOpened = FALSE;
    C__TraceMsgBoxThreadData threadData;
    C__TraceMsgBoxThreadDataW threadDataW;
    if (unicode)
        memset(&threadDataW, 0, sizeof(threadDataW));
    else
        memset(&threadData, 0, sizeof(threadData));
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
                    lstrcpyn(threadData.Msg, TraceStringBuf.c_str(), (int)TraceStringBuf.length() + 1);
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
}

//*****************************************************************************

#ifdef _DEBUG
#undef memcpy
void* _sal_safe_memcpy(void* dest, const void* src, size_t count)
{
    if ((char*)dest + count > src && (char*)src + count > dest)
    {
        TRACE_C("_sal_safe_memcpy: source and destination of memcpy overlap!");
    }
    return memcpy(dest, src, count);
}
#endif // _DEBUG

#endif // defined(TRACE_ENABLE) && !defined(INSIDE_SALAMANDER)

// template for defining these "forbidden" operators (to enable control
// prohibited combinations of WCHAR / char strings in TRACE macros must not be the following
// operators defined in other modules - otherwise the linker would not report an error - idea:
// in DEBUG version we catch linker errors, in RELEASE version we catch errors in our own definition
// operator)
#if !defined(_DEBUG) && !defined(INSIDE_SALAMANDER) && !defined(__BORLANDC__)

#include <ostream>

std::ostream& operator<<(std::ostream& out, const wchar_t* str)
{
    return out << (void*)str;
}
std::wostream& operator<<(std::wostream& out, const char* str) { return out << (void*)str; }

#endif // _DEBUG
