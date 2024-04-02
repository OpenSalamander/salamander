// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include <windows.h>
#include <ostream>
#include <tchar.h>
#include <new.h>

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "trace.h"

#ifndef ALLOCHAN_DISABLE

#ifndef SAFE_ALLOC
#pragma message(__FILE__ "(" __TRACE_STR(__LINE__) "): Maybe Error: Macro SAFE_ALLOC is not defined! You should define this macro when using allochan.* module to optimize other modules.")
#endif // SAFE_ALLOC

// The order here is important.
// Section names must be 8 characters or less.
// The sections with the same name before the $
// are merged into one section. The order that
// they are merged is determined by sorting
// the characters after the $.
// i_allochan and i_allochan_end are used to set
// boundaries so we can find the real functions
// that we need to call for initialization.

#pragma warning(disable : 4075) // we want to define the order of module initialization

typedef void(__cdecl* _PVFV)(void);

#pragma section(".i_alc$a", read)
__declspec(allocate(".i_alc$a")) const _PVFV i_allochan = (_PVFV)1; // at the beginning of the section, we will use the variable i_allochan for .i_alc

#pragma section(".i_alc$z", read)
__declspec(allocate(".i_alc$z")) const _PVFV i_allochan_end = (_PVFV)1; // and at the end of the section .i_alc we will use the variable i_allochan_end

void Initialize__Allochan()
{
    const _PVFV* x = &i_allochan;
    for (++x; x < &i_allochan_end; ++x)
        if (*x != NULL)
            (*x)();
}

#pragma init_seg(".i_alc$m")

class C__AllocHandlerInit
{
public:
    static int AltapNewHandler(size_t size);

    C__AllocHandlerInit()
    {
        InitializeCriticalSection(&CriticalSection);
        OldNewHandler = _set_new_handler(AltapNewHandler); // operator new should call our new-handler when out of memory
        OldNewMode = _set_new_mode(1);                     // malloc should call our new-handler when out of memory
    }
    ~C__AllocHandlerInit()
    {
        _set_new_mode(OldNewMode);
        _set_new_handler(OldNewHandler);
        DeleteCriticalSection(&CriticalSection);
    }

private:
    CRITICAL_SECTION CriticalSection;
    _PNH OldNewHandler;
    int OldNewMode;
} __AllocHandlerInit;

TCHAR __AllocHandlerMessage[500] = _T("Insufficient memory to allocate %Iu bytes. Try to release some memory (e.g. ")
                                   _T("close some running application) and click Retry. If it does not help, you can ")
                                   _T("click Ignore to pass memory allocation error to this application or click Abort ")
                                   _T("to terminate this application.");
TCHAR __AllocHandlerTitle[200] = _T("Error");
TCHAR __AllocHandlerWarningIgnore[500] = _T("Do you really want to pass memory allocation error to this application?\n\n")
                                         _T("WARNING: Application may crash and then all unsaved data will be lost!\n")
                                         _T("HINT: We recommend to risk this action only if the application is trying to ")
                                         _T("allocate extra large block of memory (i.e. more than 500 MB).");
TCHAR __AllocHandlerWarningAbort[200] = _T("Do you really want to terminate this application?\n\nWARNING: All unsaved data will be lost!");

void SetAllocHandlerMessage(const TCHAR* message, const TCHAR* title, const TCHAR* warningIgnore, const TCHAR* warningAbort)
{
    if (message != NULL)
        lstrcpyn(__AllocHandlerMessage, message, 500);
    if (title != NULL)
        lstrcpyn(__AllocHandlerTitle, title, 200);
    if (warningIgnore != NULL)
        lstrcpyn(__AllocHandlerWarningIgnore, warningIgnore, 500);
    if (warningAbort != NULL)
        lstrcpyn(__AllocHandlerWarningAbort, warningAbort, 200);
}

int C__AllocHandlerInit::AltapNewHandler(size_t size)
{
    TRACE_ET(_T("AltapNewHandler: not enough memory to allocate ") << size << _T(" bytes!"));
    int ret = 1;
    int ti = GetTickCount();
    EnterCriticalSection(&__AllocHandlerInit.CriticalSection);
    if (GetTickCount() - ti <= 500) // message-box will be shown only if we haven't forced the user to solve the same problem in another thread just a moment ago
    {
        TCHAR buf[550];
        _sntprintf_s(buf, _countof(buf) - 1, __AllocHandlerMessage, size);
        int res;
        do
        {
            res = MessageBox(NULL, buf, __AllocHandlerTitle, MB_ICONERROR | MB_TASKMODAL | MB_ABORTRETRYIGNORE | MB_DEFBUTTON2);
            if (res == 0)
            {
                TRACE_ET(_T("AltapNewHandler: unable to open message-box!"));
                Sleep(1000); // Let the machine rest and try to show the msgbox again
            }
        } while (res == 0);
        if (res == IDABORT) // terminate
        {
            do
            {
                res = MessageBox(NULL, __AllocHandlerWarningAbort, __AllocHandlerTitle, MB_ICONQUESTION | MB_TASKMODAL | MB_YESNO | MB_DEFBUTTON2);
                if (res == 0)
                {
                    TRACE_ET(_T("AltapNewHandler: unable to open message-box with abort-warning!"));
                    Sleep(1000); // Let the machine rest and try to show the msgbox again
                }
            } while (res == 0);
            if (res == IDYES)
                TerminateProcess(GetCurrentProcess(), 777); // Hard exit (ExitProcess still calls something)
        }
        else
        {
            if (res == IDIGNORE) // uzivatel chce poslat problem alokace do aplikace (return NULL), mista, kde se alokuji velke bloky by mely byt osetrene (jinak to spadne pri pristupu na NULL)
            {
                do
                {
                    res = MessageBox(NULL, __AllocHandlerWarningIgnore, __AllocHandlerTitle, MB_ICONQUESTION | MB_TASKMODAL | MB_YESNO | MB_DEFBUTTON2);
                    if (res == 0)
                    {
                        TRACE_ET(_T("AltapNewHandler: unable to open message-box with ignore-warning!"));
                        Sleep(1000); // Let the machine rest and try to show the msgbox again
                    }
                } while (res == 0);
                if (res == IDYES)
                    ret = 0; // return NULL to the application
            }
        }
    }
    LeaveCriticalSection(&__AllocHandlerInit.CriticalSection);
    return ret; // retry or NULL
}

#endif // ALLOCHAN_DISABLE
