// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <crtdbg.h>

#define __MODUL_MESSAGES_CPP

#ifndef MESSAGES_DISABLE

// The order here is important.
// Section names must be 8 characters or less.
// The sections with the same name before the $
// are merged into one section. The order that
// they are merged is determined by sorting
// the characters after the $.
// i_messages and i_messages_end are used to set
// boundaries so we can find the real functions
// that we need to call for initialization.

#pragma warning(disable : 4075) // we want to define the order of module initialization

typedef void(__cdecl* _PVFV)(void);

#pragma section(".i_msg$a", read)
__declspec(allocate(".i_msg$a")) const _PVFV i_messages = (_PVFV)1; // At the beginning of the section, we will assign the variable i_messages to the variable i_msg.

#pragma section(".i_msg$z", read)
__declspec(allocate(".i_msg$z")) const _PVFV i_messages_end = (_PVFV)1; // a na konec sekce .i_msg si dame promennou i_messages_end

void Initialize__Messages()
{
    const _PVFV* x = &i_messages;
    for (++x; x < &i_messages_end; ++x)
        if (*x != NULL)
            (*x)();
}

#pragma init_seg(".i_msg$m")

#include <ostream>
#include <stdio.h>
#ifdef _DEBUG
#include <sstream>
#endif // _DEBUG

#ifndef TRACE_ENABLE
#define __MESSAGES_STR2(x) #x
#define __MESSAGES_STR(x) __MESSAGES_STR2(x)
//#pragma message(__FILE__ "(" __MESSAGES_STR(__LINE__) "): Warning: macro TRACE_ENABLE not defined - unable to show errors")
#endif // TRACE_ENABLE

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "trace.h"
#include "messages.h"

char __ResourceStringBuffer[__RESOURCE_STRING_BUFFER_SIZE] = "";
WCHAR __ResourceStringBufferW[__RESOURCE_STRING_BUFFER_SIZE] = L"";
char __SPrintFBuffer[__SPRINTF_BUFFER_SIZE] = "";
WCHAR __SPrintFBufferW[__SPRINTF_BUFFER_SIZE] = L"";
char __ErrorBuffer[__ERROR_BUFFER_SIZE] = "";
WCHAR __ErrorBufferW[__ERROR_BUFFER_SIZE] = L"";

const char* __MessagesTitle = "Message";
const WCHAR* __MessagesTitleW = L"Message";
HWND __MessagesParent = NULL;

char __MessagesTitleBuf[200];
WCHAR __MessagesTitleBufW[200];

#ifdef MULTITHREADED_MESSAGES_ENABLE

// critical section for the entire module - monitor
CRITICAL_SECTION __MessagesCriticalSection;
// handle the current owner thread
DWORD __MessagesOwnerThreadID = 0;
// number of nested calls to EnterMessagesModule (within the current owning thread)
int __MessagesModulBlockCount = 0;

#ifdef MESSAGES_DEBUG

// call from a thread that does not have access to the data and functions of the module (did not lock)
const char* __MessagesBadCall = "Incorrect call to function from modul MESSAGES.";

#endif // MESSAGES_DEBUG

#endif // MULTITHREADED_MESSAGES_ENABLE

C__Messages __Messages;
C__MessagesW __MessagesW;

#ifdef MULTITHREADED_MESSAGES_ENABLE

const char* __MessagesLowMemory = "Insufficient memory.";
const WCHAR* __MessagesLowMemoryW = L"Insufficient memory.";

//*****************************************************************************
//
// EnterMessagesModule
//

void EnterMessagesModul()
{
    EnterCriticalSection(&__MessagesCriticalSection);
    __MessagesOwnerThreadID = GetCurrentThreadId();
    __MessagesModulBlockCount++;
}

//*****************************************************************************
//
// LeaveMessagesModule
//

void LeaveMessagesModul()
{
    if (__MessagesModulBlockCount > 0 &&
        __MessagesOwnerThreadID == GetCurrentThreadId())
    {
        if (--__MessagesModulBlockCount == 0)
            __MessagesOwnerThreadID = 0;
        LeaveCriticalSection(&__MessagesCriticalSection);
    }
}

#endif // MULTITHREADED_MESSAGES_ENABLE

//*****************************************************************************
//
// C__Messages
//

C__Messages::C__Messages() : MessagesStrStream(&MessagesStringBuf)
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
#endif // _DEBUG
#ifdef MULTITHREADED_MESSAGES_ENABLE
    InitializeCriticalSection(&__MessagesCriticalSection);
}

C__Messages::~C__Messages()
{
    DeleteCriticalSection(&__MessagesCriticalSection);
#endif // MULTITHREADED_MESSAGES_ENABLE
}

struct C__MessageBoxData
{
    const char* Text;
    const char* Caption;
    UINT Type;
    int Return;
};

int CALLBACK __MessagesMessageBoxThreadF(C__MessageBoxData* data)
{ // Cannot wait for a response from the calling thread, as it will not respond
    // thus if parent==NULL -> no disabling of windows etc.
    data->Return = MessageBoxA(NULL, data->Text, data->Caption, data->Type | MB_SETFOREGROUND);
    return 0;
}

int C__Messages::MessageBoxT(const char* lpCaption, UINT uType)
{
    C__MessageBoxData data;
    data.Caption = lpCaption;
    data.Type = uType;
    data.Return = 0;

    MessagesStrStream.flush(); // flushing to the buffer (in lpText)

#ifndef MULTITHREADED_MESSAGES_ENABLE
    data.Text = MessagesStringBuf.c_str();
    MessagesStringBuf.erase(); // Preparing for the next message
#else                          // MULTITHREADED_MESSAGES_ENABLE
    int len = (int)MessagesStringBuf.length() + 1;
    HGLOBAL message = GlobalAlloc(GMEM_FIXED, len); // Backup text
    if (message != NULL)
    {
        memcpy((char*)message, MessagesStringBuf.c_str(), len); // it is FIXED -> HANDLE==PTR
        data.Text = (char*)message;
    }
    else
        data.Text = __MessagesLowMemory;
    MessagesStringBuf.erase(); // Preparing for the next message
    LeaveMessagesModul();      // now other threads can start messing around + message loopy
#endif                         // MULTITHREADED_MESSAGES_ENABLE

    // MessageBox will be launched in a new thread to prevent it from blocking the current thread
    DWORD threadID;
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)__MessagesMessageBoxThreadF, &data, 0, &threadID);
    if (thread != NULL)
    {
        WaitForSingleObject(thread, INFINITE); // wait until the user unlocks it
        CloseHandle(thread);
    }
    else
        TRACE_E("Unable to show MessageBox: " << data.Caption << ": " << data.Text);

#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (message != NULL)
        GlobalFree(message);
#endif // MULTITHREADED_MESSAGES_ENABLE

    return data.Return;
}

int C__Messages::MessageBox(HWND hWnd, const char* lpCaption, UINT uType)
{
    int ret;
    MessagesStrStream.flush(); // flushing to the buffer (in lpText)

#ifndef MULTITHREADED_MESSAGES_ENABLE
    if (!IsWindow(hWnd))
        hWnd = NULL;
    ret = ::MessageBoxA(hWnd, MessagesStringBuf.c_str(), lpCaption, uType);
    MessagesStringBuf.erase(); // Preparing for the next message
#else                          // MULTITHREADED_MESSAGES_ENABLE
    size_t len = MessagesStringBuf.length() + 1;
    HGLOBAL message = GlobalAlloc(GMEM_FIXED, len); // Backup text
    char* txt;
    txt = (char*)message; // it is FIXED -> HANDLE==PTR
    if (txt != NULL)
        memcpy(txt, MessagesStringBuf.c_str(), len);
    else
        txt = (char*)__MessagesLowMemory;
    MessagesStringBuf.erase(); // Preparing for the next message
    LeaveMessagesModul();      // now other threads can start messing around + message loopy

    if (!IsWindow(hWnd))
        hWnd = NULL;
    ret = ::MessageBoxA(hWnd, txt, lpCaption, uType);

    if (message != NULL)
        GlobalFree(message);
#endif                         // MULTITHREADED_MESSAGES_ENABLE

    return ret;
}

//*****************************************************************************
//
// C__MessagesW
//

C__MessagesW::C__MessagesW() : MessagesStrStream(&MessagesStringBuf)
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
    std::wstringstream s;
    s << 1;
#endif // _DEBUG
}

struct C__MessageBoxDataW
{
    const WCHAR* Text;
    const WCHAR* Caption;
    UINT Type;
    int Return;
};

int CALLBACK __MessagesWMessageBoxThreadF(C__MessageBoxDataW* data)
{ // Cannot wait for a response from the calling thread, as it will not respond
    // thus if parent==NULL -> no disabling of windows etc.
    data->Return = MessageBoxW(NULL, data->Text, data->Caption, data->Type | MB_SETFOREGROUND);
    return 0;
}

int C__MessagesW::MessageBoxT(const WCHAR* lpCaption, UINT uType)
{
    C__MessageBoxDataW data;
    data.Caption = lpCaption;
    data.Type = uType;
    data.Return = 0;

    MessagesStrStream.flush(); // flushing to the buffer (in lpText)

#ifndef MULTITHREADED_MESSAGES_ENABLE
    data.Text = MessagesStringBuf.c_str();
    MessagesStringBuf.erase(); // Preparing for the next message
#else                          // MULTITHREADED_MESSAGES_ENABLE
    int len = (int)MessagesStringBuf.length() + 1;
    HGLOBAL message = GlobalAlloc(GMEM_FIXED, sizeof(WCHAR) * len); // Backup text
    if (message != NULL)
    {
        memcpy((WCHAR*)message, MessagesStringBuf.c_str(), sizeof(WCHAR) * len); // it is FIXED -> HANDLE==PTR
        data.Text = (WCHAR*)message;
    }
    else
        data.Text = __MessagesLowMemoryW;
    MessagesStringBuf.erase(); // Preparing for the next message
    LeaveMessagesModul();      // now other threads can start messing around + message loopy
#endif                         // MULTITHREADED_MESSAGES_ENABLE

    // MessageBox will be launched in a new thread to prevent it from blocking the current thread
    DWORD threadID;
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)__MessagesWMessageBoxThreadF, &data, 0, &threadID);
    if (thread != NULL)
    {
        WaitForSingleObject(thread, INFINITE); // wait until the user unlocks it
        CloseHandle(thread);
    }
    else
        TRACE_EW(L"Unable to show MessageBox: " << data.Caption << L": " << data.Text);

#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (message != NULL)
        GlobalFree(message);
#endif // MULTITHREADED_MESSAGES_ENABLE

    return data.Return;
}

int C__MessagesW::MessageBox(HWND hWnd, const WCHAR* lpCaption, UINT uType)
{
    int ret;
    MessagesStrStream.flush(); // flushing to the buffer (in lpText)

#ifndef MULTITHREADED_MESSAGES_ENABLE
    if (!IsWindow(hWnd))
        hWnd = NULL;
    ret = ::MessageBoxW(hWnd, MessagesStringBuf.c_str(), lpCaption, uType);
    MessagesStringBuf.erase(); // Preparing for the next message
#else                          // MULTITHREADED_MESSAGES_ENABLE
    size_t len = MessagesStringBuf.length() + 1;
    HGLOBAL message = GlobalAlloc(GMEM_FIXED, sizeof(WCHAR) * len); // Backup text
    WCHAR* txt;
    txt = (WCHAR*)message; // it is FIXED -> HANDLE==PTR
    if (txt != NULL)
        memcpy(txt, MessagesStringBuf.c_str(), sizeof(WCHAR) * len);
    else
        txt = (WCHAR*)__MessagesLowMemoryW;
    MessagesStringBuf.erase(); // Preparing for the next message
    LeaveMessagesModul();      // now other threads can start messing around + message loopy

    if (!IsWindow(hWnd))
        hWnd = NULL;
    ret = ::MessageBoxW(hWnd, txt, lpCaption, uType);

    if (message != NULL)
        GlobalFree(message);
#endif                         // MULTITHREADED_MESSAGES_ENABLE

    return ret;
}

//*****************************************************************************
//
// Resource
//

const char* rsc(int resID)
{
#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (__MessagesModulBlockCount > 0 &&
        __MessagesOwnerThreadID == GetCurrentThreadId())
    {
#endif // MULTITHREADED_MESSAGES_ENABLE
        if (LoadStringA(HInstance, resID, __ResourceStringBuffer,
                        __RESOURCE_STRING_BUFFER_SIZE) == 0)
        {
            TRACE_E("Unable to load string from resource (resource ID is " << resID << ")");
            __ResourceStringBuffer[0] = 0;
        }
        return __ResourceStringBuffer;
#ifdef MULTITHREADED_MESSAGES_ENABLE
    }
    else
    {
#ifdef MESSAGES_DEBUG
        TRACE_E(__MessagesBadCall);
#endif // MESSAGES_DEBUG
        return NULL;
    }
#endif // MULTITHREADED_MESSAGES_ENABLE
}

const WCHAR* rscW(int resID)
{
#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (__MessagesModulBlockCount > 0 &&
        __MessagesOwnerThreadID == GetCurrentThreadId())
    {
#endif // MULTITHREADED_MESSAGES_ENABLE
        if (LoadStringW(HInstance, resID, __ResourceStringBufferW,
                        __RESOURCE_STRING_BUFFER_SIZE) == 0)
        {
            TRACE_E("Unable to load string from resource (resource ID is " << resID << ")");
            __ResourceStringBufferW[0] = 0;
        }
        return __ResourceStringBufferW;
#ifdef MULTITHREADED_MESSAGES_ENABLE
    }
    else
    {
#ifdef MESSAGES_DEBUG
        TRACE_E(__MessagesBadCall);
#endif // MESSAGES_DEBUG
        return NULL;
    }
#endif // MULTITHREADED_MESSAGES_ENABLE
}

//*****************************************************************************
//
// spf
//

const char* spf(const char* formatString, ...)
{
#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (__MessagesModulBlockCount > 0 &&
        __MessagesOwnerThreadID == GetCurrentThreadId())
    {
#endif // MULTITHREADED_MESSAGES_ENABLE
        va_list params;
        va_start(params, formatString);
        _vsnprintf_s(__SPrintFBuffer, _TRUNCATE, formatString, params);
        va_end(params);
        return __SPrintFBuffer;
#ifdef MULTITHREADED_MESSAGES_ENABLE
    }
    else
    {
#ifdef MESSAGES_DEBUG
        TRACE_E(__MessagesBadCall);
#endif // MESSAGES_DEBUG
        return NULL;
    }
#endif // MULTITHREADED_MESSAGES_ENABLE
}

const WCHAR* spfW(const WCHAR* formatString, ...)
{
#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (__MessagesModulBlockCount > 0 &&
        __MessagesOwnerThreadID == GetCurrentThreadId())
    {
#endif // MULTITHREADED_MESSAGES_ENABLE
        va_list params;
        va_start(params, formatString);
        _vsnwprintf_s(__SPrintFBufferW, _TRUNCATE, formatString, params);
        va_end(params);
        return __SPrintFBufferW;
#ifdef MULTITHREADED_MESSAGES_ENABLE
    }
    else
    {
#ifdef MESSAGES_DEBUG
        TRACE_E(__MessagesBadCall);
#endif // MESSAGES_DEBUG
        return NULL;
    }
#endif // MULTITHREADED_MESSAGES_ENABLE
}

//*****************************************************************************
//
// spf
//

const char* spf(int formatStringResID, ...)
{
#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (__MessagesModulBlockCount > 0 &&
        __MessagesOwnerThreadID == GetCurrentThreadId())
    {
#endif // MULTITHREADED_MESSAGES_ENABLE
        va_list params;
        va_start(params, formatStringResID);
        _vsnprintf_s(__SPrintFBuffer, _TRUNCATE, rsc(formatStringResID), params);
        va_end(params);
        return __SPrintFBuffer;
#ifdef MULTITHREADED_MESSAGES_ENABLE
    }
    else
    {
#ifdef MESSAGES_DEBUG
        TRACE_E(__MessagesBadCall);
#endif // MESSAGES_DEBUG
        return NULL;
    }
#endif // MULTITHREADED_MESSAGES_ENABLE
}

const WCHAR* spfW(int formatStringResID, ...)
{
#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (__MessagesModulBlockCount > 0 &&
        __MessagesOwnerThreadID == GetCurrentThreadId())
    {
#endif // MULTITHREADED_MESSAGES_ENABLE
        va_list params;
        va_start(params, formatStringResID);
        _vsnwprintf_s(__SPrintFBufferW, _TRUNCATE, rscW(formatStringResID), params);
        va_end(params);
        return __SPrintFBufferW;
#ifdef MULTITHREADED_MESSAGES_ENABLE
    }
    else
    {
#ifdef MESSAGES_DEBUG
        TRACE_E(__MessagesBadCall);
#endif // MESSAGES_DEBUG
        return NULL;
    }
#endif // MULTITHREADED_MESSAGES_ENABLE
}

//*****************************************************************************
//
// error
//

const char* err(DWORD error)
{
#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (__MessagesModulBlockCount > 0 &&
        __MessagesOwnerThreadID == GetCurrentThreadId())
    {
#endif // MULTITHREADED_MESSAGES_ENABLE
        wsprintfA(__ErrorBuffer, "(%d) ", error);
        int len = (int)strlen(__ErrorBuffer);
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL,
                       error,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       __ErrorBuffer + len,
                       __ERROR_BUFFER_SIZE - len,
                       NULL);
        return __ErrorBuffer;
#ifdef MULTITHREADED_MESSAGES_ENABLE
    }
    else
    {
#ifdef MESSAGES_DEBUG
        TRACE_E(__MessagesBadCall);
#endif // MESSAGES_DEBUG
        return NULL;
    }
#endif // MULTITHREADED_MESSAGES_ENABLE
}

const WCHAR* errW(DWORD error)
{
#ifdef MULTITHREADED_MESSAGES_ENABLE
    if (__MessagesModulBlockCount > 0 &&
        __MessagesOwnerThreadID == GetCurrentThreadId())
    {
#endif // MULTITHREADED_MESSAGES_ENABLE
        wsprintfW(__ErrorBufferW, L"(%d) ", error);
        int len = (int)wcslen(__ErrorBufferW);
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL,
                       error,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       __ErrorBufferW + len,
                       __ERROR_BUFFER_SIZE - len,
                       NULL);
        return __ErrorBufferW;
#ifdef MULTITHREADED_MESSAGES_ENABLE
    }
    else
    {
#ifdef MESSAGES_DEBUG
        TRACE_E(__MessagesBadCall);
#endif // MESSAGES_DEBUG
        return NULL;
    }
#endif // MULTITHREADED_MESSAGES_ENABLE
}

//*****************************************************************************
//
// SetMessagesTitle
//

void SetMessagesTitle(const char* title)
{
    lstrcpynA(__MessagesTitleBuf, title, _countof(__MessagesTitleBuf));
    __MessagesTitle = __MessagesTitleBuf;
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, __MessagesTitleBuf, -1,
                        __MessagesTitleBufW, _countof(__MessagesTitleBufW));
    __MessagesTitleBufW[_countof(__MessagesTitleBufW) - 1] = 0;
    __MessagesTitleW = __MessagesTitleBufW;
}

void SetMessagesTitleW(const WCHAR* title)
{
    lstrcpynW(__MessagesTitleBufW, title, _countof(__MessagesTitleBufW));
    __MessagesTitleW = __MessagesTitleBufW;
    WideCharToMultiByte(CP_ACP, 0, __MessagesTitleBufW, -1,
                        __MessagesTitleBuf, _countof(__MessagesTitleBuf), NULL, NULL);
    __MessagesTitleBuf[_countof(__MessagesTitleBuf) - 1] = 0;
    __MessagesTitle = __MessagesTitleBuf;
}

//*****************************************************************************
//
// SetMessagesParent
//

void SetMessagesParent(HWND parent)
{
    __MessagesParent = parent;
}

#endif // MESSAGES_DISABLE
