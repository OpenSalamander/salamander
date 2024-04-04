// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "usermenu.h"
#include "snooper.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "tasklist.h"
#include "salmoncl.h"

#ifndef CALLSTK_DISABLE

// The order here is important.
// Section names must be 8 characters or less.
// The sections with the same name before the $
// are merged into one section. The order that
// they are merged is determined by sorting
// the characters after the $.
// i_callstk and i_callstk_end are used to set
// boundaries so we can find the real functions
// that we need to call for initialization.

#pragma warning(disable : 4075) // we want to define the order of module initialization

typedef void(__cdecl* _PVFV)(void);

#pragma section(".i_cst$a", read)
__declspec(allocate(".i_cst$a")) const _PVFV i_callstk = (_PVFV)1; // at the beginning of section .i_cst we will use variable i_callstk

#pragma section(".i_cst$z", read)
__declspec(allocate(".i_cst$z")) const _PVFV i_callstk_end = (_PVFV)1; // and at the end of section .i_cst we will use the variable i_callstk_end

void Initialize__CallStk()
{
    const _PVFV* x = &i_callstk;
    for (++x; x < &i_callstk_end; ++x)
        if (*x != NULL)
            (*x)();
}

#pragma init_seg(".i_cst$m")

BOOL CCallStack::SectionOK = FALSE;
DWORD CCallStack::TlsIndex = 0;
CRITICAL_SECTION CCallStack::Section;
TIndirectArray<CCallStack> CCallStack::CallStacks(10, 10, dtNoDelete);
BOOL CCallStack::ExceptionExists = FALSE;
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
LARGE_INTEGER CCallStack::SavedPerfFreq = {1};
DWORD CCallStack::SpeedBenchmark = 0;
BOOL __CallStk_T = TRUE;
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

CCallStack MainThreadStack; // for the call-stack object to be created before calling the constructor in the main thread

//
// ****************************************************************************
// PreventSetUnhandledExceptionFilter
//

#if defined _M_X64 || defined _M_IX86
LPTOP_LEVEL_EXCEPTION_FILTER WINAPI
MyDummySetUnhandledExceptionFilter(
    LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
    return NULL;
}
#else
#error "This code works only for x86 and x64!"
#endif

BOOL PreventSetUnhandledExceptionFilterAux()
{
    HMODULE hKernel32 = LoadLibrary(_T("kernel32.dll"));
    if (hKernel32 == NULL)
        return FALSE;
    void* pOrgEntry = GetProcAddress(hKernel32, "SetUnhandledExceptionFilter");
    if (pOrgEntry == NULL)
        return FALSE;

    DWORD dwOldProtect = 0;
    SIZE_T jmpSize = 5;
#ifdef _M_X64
    jmpSize = 13;
#endif
    BOOL bProt = VirtualProtect(pOrgEntry, jmpSize,
                                PAGE_EXECUTE_READWRITE, &dwOldProtect);
    BYTE newJump[20];
    void* pNewFunc = &MyDummySetUnhandledExceptionFilter;
#ifdef _M_IX86
    DWORD dwOrgEntryAddr = (DWORD)pOrgEntry;
    dwOrgEntryAddr += jmpSize; // add 5 for 5 op-codes for jmp rel32
    DWORD dwNewEntryAddr = (DWORD)pNewFunc;
    DWORD dwRelativeAddr = dwNewEntryAddr - dwOrgEntryAddr;
    // JMP rel32: Jump near, relative, displacement relative to next instruction.
    newJump[0] = 0xE9; // JMP rel32
    memcpy(&newJump[1], &dwRelativeAddr, sizeof(pNewFunc));
#elif _M_X64
    // We must use R10 or R11, because these are "scratch" registers
    // which need not to be preserved accross function calls
    // For more info see: Register Usage for x64 64-Bit
    // http://msdn.microsoft.com/en-us/library/ms794547.aspx
    // Thanks to Matthew Smith!!!
    newJump[0] = 0x49; // MOV R11, ...
    newJump[1] = 0xBB; // ...
    memcpy(&newJump[2], &pNewFunc, sizeof(pNewFunc));
    //pCur += sizeof (ULONG_PTR);
    newJump[10] = 0x41; // JMP R11, ...
    newJump[11] = 0xFF; // ...
    newJump[12] = 0xE3; // ...
#endif
    SIZE_T bytesWritten;
    BOOL bRet = WriteProcessMemory(GetCurrentProcess(),
                                   pOrgEntry, newJump, jmpSize, &bytesWritten);

    if (bProt != FALSE)
    {
        DWORD dwBuf;
        VirtualProtect(pOrgEntry, jmpSize, dwOldProtect, &dwBuf);
        // Does it make sense? http://chadaustin.me/2009/03/disabling-functions/
        //FlushInstructionCache(GetCurrentProcess(), pOrgEntry, 20);
    }
    return bRet;
}

BOOL PreventSetUnhandledExceptionFilter()
{
    __try
    {
        PreventSetUnhandledExceptionFilterAux();
        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        TRACE_E("PreventSetUnhandledExceptionFilterAux failed!");
        return FALSE;
    }
}

//
// ****************************************************************************
// CCallStack
//

struct CTBRData
{
    HANDLE TerminateEvent;
    HANDLE Event;
    HANDLE EventProcessed;
    EXCEPTION_POINTERS* Exception;
    BOOL ExitProcess;
    BOOL EventProcessedRet;
    DWORD CurrentThreadID;
    DWORD ShellExtCrashID; // if different from -1, it is an exception in shell execute
    const char* IconOvrlsHanName;
    const char* BugReportPath;
};

CTBRData TBRData = {0};
HANDLE BugReportThread = NULL;
DWORD BugReportThreadID;

void BenchmarkCallStkTestFunction(int counter, const char* string)
{
    CALL_STACK_MESSAGE3("BenchmarkCallStkTestFunction(%d, %s)", counter, string);
}

LONG WINAPI TopLevelExceptionFilter(LPEXCEPTION_POINTERS exception)
{
    int ret = CCallStack::HandleException(exception);
    if (ret == EXCEPTION_EXECUTE_HANDLER) // user clicked on Terminate
    {
        TRACE_I("TopLevelExceptionFilter: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
    }
    return ret;
}

LPTOP_LEVEL_EXCEPTION_FILTER OldUnhandledExceptionFilter = NULL;

CCallStack::CCallStack(BOOL dontSuspend)
{
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    PushesCounter = 0;
    PushPerfTimeCounter.QuadPart = 0;
    IgnoredPushPerfTimeCounter.QuadPart = 0;
    MonitoredItems = NULL;
    MonitoredItemsSize = 0;
    NewestItemIndex = -1;
    OldestItemIndex = -1;
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

    DontSuspend = dontSuspend;
    FirstCallstack = (TlsIndex == 0);
    if (TlsIndex == 0)
        TlsIndex = HANDLES(TlsAlloc());
    TlsSetValue(TlsIndex, this);

    if (!SectionOK)
    {
        HANDLES(InitializeCriticalSection(&Section));
        SectionOK = TRUE;
    }

    ThreadID = GetCurrentThreadId();

    if (HANDLES(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                                GetCurrentProcess(), &ThreadHandle,
                                0, FALSE, DUPLICATE_SAME_ACCESS)) == 0)
    {
        TRACE_E("DuplicateHandle failed.");
        ThreadHandle = NULL;
    }

    PluginDLLName = NULL;
    PluginDLLNameUses = 0;

    HANDLES(EnterCriticalSection(&Section));
    CallStacks.Add(this);
    HANDLES(LeaveCriticalSection(&Section));

    Text[0] = 0;
    End = Text;
    Skipped = 0;
    Reset();

    if (FirstCallstack)
    {
        // create a thread for generating bug reports; in PROTECH/VKO I saw the state during infinite recursion,
        // when the bug report was generated only if it was created in a helper thread
        TBRData.TerminateEvent = NOHANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
        TBRData.Event = NOHANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
        TBRData.EventProcessed = NOHANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
        if (TBRData.TerminateEvent == NULL || TBRData.Event == NULL || TBRData.EventProcessed == NULL)
            TRACE_E("Error during creating events.");
        else // Start the thread only if the events were successfully created
        {
            BugReportThread = NOHANDLES(CreateThread(NULL, 0, ThreadBugReportF, &TBRData, 0, &BugReportThreadID));
            if (BugReportThread == NULL)
                TRACE_E("CreateThread failed!");
            else
                SetThreadPriority(BugReportThread, THREAD_PRIORITY_ABOVE_NORMAL);
        }
        if (BugReportThread == NULL)
        {
            TRACE_E("Unable to start BugReport thread. Error: " << _sys_errlist[errno]);
        }

        /*      else
    {
      showInfo = FALSE;
      WaitForSingleObject(BugReport, INFINITE);  // wait for the thread to finish
      NOHANDLES(CloseHandle(BugReport));
    }*/

        OldUnhandledExceptionFilter = SetUnhandledExceptionFilter(TopLevelExceptionFilter);

        // We will try to disable the unhandled exception filter for the next call.
        // If a DLL library (plugin, shell extension) is loaded into the Salamander process
        // uses a different RTL than ours, during its initialization this RTL will take over this filter.
        // Additionally, starting from MSVC2005, exceptions are no longer thrown for various sanity checks, but instead
        // prints a message and calls UnhandledExceptionFilter(), which opens the Watson window.
        // For example, this is how Salam was falling without a bug report with TortoiseSVN shell extension.
        // Thanks to the following call, we should receive all unhandled exceptions
        // sanity check from MSVC2005.
        //
        // More on this topic see "A proposal to make Dr.Watson invocation configurable":
        // http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=101337
        // and then two hard-core solutions:
        // http://www.debuginfo.com/articles/debugfilters.html
        // http://blog.kalmbachnet.de/?postid=75
        // 3/2012 - update for x64: http://blog.kalmbach-software.de/2008/04/02/unhandled-exceptions-in-vc8-and-above-for-x86-and-x64/
        PreventSetUnhandledExceptionFilter();
    }

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    if (CCallStack::SavedPerfFreq.QuadPart == 1)
    {
        if (!QueryPerformanceFrequency(&CCallStack::SavedPerfFreq))
            CCallStack::SavedPerfFreq.QuadPart = 1;
        if (CCallStack::SavedPerfFreq.QuadPart == 0)
            CCallStack::SavedPerfFreq.QuadPart = 1; // probably impossible, but let's check (dividing by this value)
    }
    static BOOL doBenchmark = TRUE;
    if (doBenchmark)
    {
        doBenchmark = FALSE;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL); // let's try to attract the attention of the system (so we don't measure, for example, swapping of another process)
        LARGE_INTEGER startTime, ti, endTime;
        if (QueryPerformanceCounter(&startTime))
        {
            endTime.QuadPart = CALLSTK_BENCHMARKTIME * CCallStack::SavedPerfFreq.QuadPart / 1000 + startTime.QuadPart;
            int counter = 0;
            while (1)
            {
                int i;
                for (i = 0; i < 1000; i++)
                    BenchmarkCallStkTestFunction(i, "this is just speed measurement");
                counter++;
                QueryPerformanceCounter(&ti);
                if (ti.QuadPart >= endTime.QuadPart)
                {
                    SpeedBenchmark = (DWORD)((__int64)counter * 1000 * CALLSTK_BENCHMARKTIME / (((ti.QuadPart - startTime.QuadPart) * 1000) / CCallStack::SavedPerfFreq.QuadPart));
                    TRACE_I("CCallStack::CCallStack(): Speed Benchmark: " << SpeedBenchmark << " calls in " << CALLSTK_BENCHMARKTIME << "ms");
                    break;
                }
            }
        }
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
        if (SpeedBenchmark == 0)
            TRACE_E("CCallStack::CCallStack(): unable to compute Speed Benchmark!");
    }
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    static BOOL doBenchmark = TRUE;
    if (doBenchmark)
    {
        doBenchmark = FALSE;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL); // let's try to attract the attention of the system (so we don't measure, for example, swapping of another process)
        LARGE_INTEGER startTime, ti, endTime, freq;
        if (QueryPerformanceCounter(&startTime) && QueryPerformanceFrequency(&freq))
        {
            endTime.QuadPart = 100 * freq.QuadPart / 1000 + startTime.QuadPart;
            int counter = 0;
            while (1)
            {
                int i;
                for (i = 0; i < 1000; i++)
                    BenchmarkCallStkTestFunction(i, "this is just speed measurement");
                counter++;
                QueryPerformanceCounter(&ti);
                if (ti.QuadPart >= endTime.QuadPart)
                {
                    DWORD speedBenchmark = (DWORD)((__int64)counter * 1000 * 100 / (((ti.QuadPart - startTime.QuadPart) * 1000) / freq.QuadPart));
                    TRACE_I("CCallStack::CCallStack(): Speed Benchmark: " << speedBenchmark << " calls in " << 100 << "ms");
                    break;
                }
            }
        }
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    }
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
}

CCallStack::~CCallStack()
{
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    if (MonitoredItems != NULL)
    {
        //    TRACE_I("CCallStack::~CCallStack(): MonitoredItemsSize=" << MonitoredItemsSize);
        if (OldestItemIndex != -1)
        {
            while (1) // we will go through the queue and finally print the states of critical call-stack calls
            {
                if (!MonitoredItems[OldestItemIndex].NotAlreadyReported) // if reported, we report final values
                {
                    TRACE_I("Info for Too Frequently Used Call Stack Message: call address: 0x" << std::hex << MonitoredItems[OldestItemIndex].CallerAddress << std::dec << ", total push-time: " << (DWORD)(MonitoredItems[OldestItemIndex].PushesPerfTime * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, total number of calls in monitored period: " << MonitoredItems[OldestItemIndex].NumberOfCalls);
                }
                if (OldestItemIndex == NewestItemIndex)
                {
                    OldestItemIndex = NewestItemIndex = -1; // queue has emptied
                    break;
                }
                OldestItemIndex++;
                if (OldestItemIndex >= MonitoredItemsSize)
                    OldestItemIndex = 0;
            }
        }
        free(MonitoredItems);
        MonitoredItemsSize = 0;
    }
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    HANDLES(EnterCriticalSection(&Section));
    int i;
    for (i = 0; i < CallStacks.Count; i++)
    {
        if (CallStacks[i] == this)
        {
            CallStacks.Delete(i);
            i = -1;
            break;
        }
    }
    if (i == CallStacks.Count)
        TRACE_E("This should never happen.");
    if (ThreadHandle != NULL)
    {
        HANDLES(CloseHandle(ThreadHandle));
        ThreadHandle = NULL;
    }
    HANDLES(LeaveCriticalSection(&Section));
    if (FirstCallstack)
    {
        SetUnhandledExceptionFilter(OldUnhandledExceptionFilter);

        if (BugReportThread != NULL)
        {
            // When terminating this thread, the code will enter here once again, because there was also a CCallStack exit for it...
            SetEvent(TBRData.TerminateEvent);           // Finish bug report thread
            WaitForSingleObject(BugReportThread, 1000); // Wait for a maximum of one second before the thread finishes
            NOHANDLES(CloseHandle(BugReportThread));
            BugReportThread = NULL;
            NOHANDLES(CloseHandle(TBRData.TerminateEvent));
            NOHANDLES(CloseHandle(TBRData.Event));
            NOHANDLES(CloseHandle(TBRData.EventProcessed));
        }

        HANDLES(DeleteCriticalSection(&Section));
        SectionOK = FALSE;
        HANDLES(TlsFree(TlsIndex));
        TlsIndex = 0;
    }
}

void CCallStack::ReleaseBeforeExitThread()
{
    CCallStack* stack = CCallStack::GetThis();
    if (stack != NULL)
        stack->ReleaseBeforeExitThreadBody(); // uncaught threads have TLS set to NULL
    else
    {
        TRACE_E("Incorrect call to CCallStack::ReleaseBeforeExitThread(): call-stack object was not defined in this thread.");
    }
}

void CCallStack::ReleaseBeforeExitThreadBody()
{
    while (!DontSuspend && CCallStack::ExceptionExists)
        Sleep(1000); // place of suspend-thread in exception handler
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    if (MonitoredItems != NULL)
    {
        //    TRACE_I("CCallStack::~CCallStack(): MonitoredItemsSize=" << MonitoredItemsSize);
        if (OldestItemIndex != -1)
        {
            while (1) // we will go through the queue and finally print the states of critical call-stack calls
            {
                if (!MonitoredItems[OldestItemIndex].NotAlreadyReported) // if reported, we report final values
                {
                    TRACE_I("Info for Too Frequently Used Call Stack Message: call address: 0x" << std::hex << MonitoredItems[OldestItemIndex].CallerAddress << std::dec << ", total push-time: " << (DWORD)(MonitoredItems[OldestItemIndex].PushesPerfTime * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, total number of calls in monitored period: " << MonitoredItems[OldestItemIndex].NumberOfCalls);
                }
                if (OldestItemIndex == NewestItemIndex)
                {
                    OldestItemIndex = NewestItemIndex = -1; // queue has emptied
                    break;
                }
                OldestItemIndex++;
                if (OldestItemIndex >= MonitoredItemsSize)
                    OldestItemIndex = 0;
            }
        }
        free(MonitoredItems);
        MonitoredItemsSize = 0;
    }
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    HANDLES(EnterCriticalSection(&Section));
    int i;
    for (i = 0; i < CallStacks.Count; i++)
    {
        if (CallStacks[i] == this)
        {
            CallStacks.Delete(i);
            i = -1;
            break;
        }
    }
    if (i == CallStacks.Count)
        TRACE_E("This should never happen.");
    if (ThreadHandle != NULL)
    {
        HANDLES(CloseHandle(ThreadHandle));
        ThreadHandle = NULL;
    }
    HANDLES(LeaveCriticalSection(&Section));
}

void CCallStack::Push(const char* format, va_list args)
{
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    PushesCounter++;
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    while (!DontSuspend && CCallStack::ExceptionExists)
        Sleep(1000); // place of suspend-thread in exception handler
    if (STACK_CALLS_BUF_SIZE - (End - Text) >= STACK_CALLS_MAX_MESSAGE_LEN + 4)
    {
        char* backup = End;
        int ret;
        __try
        {
            ret = _vsnprintf_s(End, STACK_CALLS_MAX_MESSAGE_LEN + 1, _TRUNCATE, format, args);
            if (ret >= 0)
                End += ret + 1;
            else
            {
                strcpy(backup, "vsprintf error in: ");
                int len = (int)strlen(backup);
                lstrcpyn(backup + len, format, STACK_CALLS_MAX_MESSAGE_LEN + 1 - len);
                ret = (int)strlen(backup);
                End = backup + ret + 1;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy(backup, "exception in: ");
            int len = (int)strlen(backup);
            lstrcpyn(backup + len, format, STACK_CALLS_MAX_MESSAGE_LEN + 1 - len);
            ret = (int)strlen(backup);
            End = backup + ret + 1;
        }
        *(short*)End = (short)ret;
        End += 2;
        *End = 0; // double null terminated
    }
    else
    {
        Skipped++;
    }
}

void
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
CCallStack::Pop(BOOL printCallStackTop)
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
CCallStack::Pop()
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
{
    while (!DontSuspend && CCallStack::ExceptionExists)
        Sleep(1000); // place of suspend-thread in exception handler
    if (Skipped == 0)
    {
        if (End > Text)
        {
            End -= 2;
            End -= 1 + *(short*)End;
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
            if (printCallStackTop)
                TRACE_I("Top of Call Stack: " << End);
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
            *End = 0;
        }
        else
            TRACE_E("Incorrect call to CCallStack::Pop()!");
    }
    else
    {
        Skipped--;
    }
}

const char*
CCallStack::GetNextLine()
{
    if (Enum < End)
    {
        char* s = Enum;
        while (*Enum++ != 0)
            ;
        Enum += 2;
        return s;
    }
    else
    {
        return NULL;
    }
}

void InformAboutIconOvrlsHanCrash(const char* iconOvrlsHanName)
{
    __try
    {
        static char buf[900];
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_ICONOVRLS_CRASH), iconOvrlsHanName);

        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = NULL;
        params.Flags = MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_SETFOREGROUND;
        params.Caption = SALAMANDER_TEXT_VERSION;
        params.Text = buf;
        static char aliasBtnNames[200];
        /* used for the export_mnu.py script, which generates salmenu.mnu for Translator
   we will let the collision of hotkeys for message box buttons be solved by simulating that it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_ICONOVRLS_DISTHIS
  {MNTT_IT, IDS_ICONOVRLS_DISALL
  {MNTT_PE, 0
};*/
        sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                DIALOG_ABORT, LoadStr(IDS_ICONOVRLS_DISTHIS),
                DIALOG_RETRY, LoadStr(IDS_ICONOVRLS_DISALL));
        params.AliasBtnNames = aliasBtnNames;
        int res = SalMessageBoxEx(&params);
        switch (res)
        {
        case DIALOG_ABORT:
        {
            if (!IsNameInListOfDisabledCustomIconOverlays(iconOvrlsHanName))
                AddToListOfDisabledCustomIconOverlays(iconOvrlsHanName);
            break;
        }

        case DIALOG_RETRY:
        {
            Configuration.EnableCustomIconOverlays = FALSE;
            break;
        }
        }
        if (res != DIALOG_IGNORE)
        {
            HKEY hSalamander;
            if (OpenKey(HKEY_CURRENT_USER, SalamanderConfigurationRoots[0], hSalamander)) // we will write data only if the configuration exists in the registry (which is hopefully always)
            {
                HKEY actKey;
                if (OpenKey(hSalamander, SALAMANDER_CONFIG_REG, actKey)) // we will write data only if the configuration exists in the registry (which is hopefully always)
                {
                    CloseKey(actKey);
                    if (CreateKey(hSalamander, SALAMANDER_CONFIG_REG, actKey))
                    {
                        // WARNING: these values are normally written in CMainWindow::SaveConfig()
                        SetValue(actKey, CONFIG_ENABLECUSTICOVRLS_REG, REG_DWORD,
                                 &Configuration.EnableCustomIconOverlays, sizeof(DWORD));
                        SetValue(actKey, CONFIG_DISABLEDCUSTICOVRLS_REG, REG_SZ,
                                 Configuration.DisabledCustomIconOverlays != NULL ? Configuration.DisabledCustomIconOverlays : "", -1);

                        CloseKey(actKey);
                    }
                }
                CloseKey(hSalamander);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

DWORD WINAPI
CCallStack::ThreadBugReportF(void* param)
{
    CCallStack stack(TRUE); // to have a place to put texts from called functions
    CALL_STACK_MESSAGE1("ThreadBugReportF()");

    SetThreadNameInVC("ThreadBugReport");

    CTBRData* data = (CTBRData*)param;

    HANDLE events[2];
    events[0] = data->TerminateEvent;
    events[1] = data->Event;

    BOOL loop = TRUE;
    while (loop)
    {
        DWORD waitRet = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        switch (waitRet)
        {
        case WAIT_OBJECT_0 + 0: // TerminateEvent
        {
            loop = FALSE;
            break;
        }

        case WAIT_OBJECT_0 + 1: // tasklist->Event
        {
            // after finishing, the thread must wait for a command again
            ResetEvent(data->Event);

            // save error records to disk
            BOOL ret = CreateBugReportFile(data->Exception, data->CurrentThreadID, data->ShellExtCrashID, data->BugReportPath);

            if (HLanguage != NULL) // to display the msgbox we need the SLG module already loaded
            {
                if (data->IconOvrlsHanName != NULL)
                    InformAboutIconOvrlsHanCrash(data->IconOvrlsHanName);

                if (data->ShellExtCrashID != -1)
                {
                    // crash shell extension
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = NULL;
                    params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MB_SETFOREGROUND;
                    params.Caption = SALAMANDER_TEXT_VERSION;
                    params.Text = LoadStr(IDS_SHELLEXTCRASH);
                    SalMessageBoxEx(&params);
                }
            }
            data->ExitProcess = TRUE; // Finish the thread

            data->EventProcessedRet = ret;
            SetEvent(data->EventProcessed);
            break;
        }

        default: // this should not happen
        {
            TRACE_E("Wrong event!");
            Sleep(50); // to avoid eating CPU
            break;
        }
        }
    }
    return 0;
}

void PrintBugReportLine(void* param, const char* txt, BOOL tab)
{
    static int called = 0;
    static DWORD d;
    if (tab)
        WriteFile((HANDLE)param, "  ", 2, &d, NULL);
    WriteFile((HANDLE)param, txt, lstrlen(txt), &d, NULL);
    WriteFile((HANDLE)param, "\r\n", 2, &d, NULL);
    if (++called % 5 == 0)
        FlushFileBuffers((HANDLE)param);
}

BOOL CCallStack::CreateBugReportFile(EXCEPTION_POINTERS* Exception, DWORD threadID, DWORD ShellExtCrashID, const char* bugReportFileName)
{
    // We will try to create a file with error information, using as few library functions as possible
    // calling, the smaller the probability that it will crash during this function (libraries can
    // flat kicked -> Salamander obviously falls for some reason)
    BOOL ret = TRUE;

    __try
    {
        {
            // creating a file
            static HANDLE file;
            file = NOHANDLES(CreateFile(bugReportFileName, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL, NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                __try
                {
                    PrintBugReportLine((void*)file, "Open Salamander Bug Report File", FALSE);
                    PrintBugReportLine((void*)file, "", FALSE);

#ifndef CALLSTK_DISABLE
                    static CCallStack* stack;
                    stack = CCallStack::GetThis();
                    static BOOL oldDontSuspend;
                    if (stack != NULL)
                    {
                        oldDontSuspend = stack->DontSuspend;
                        stack->DontSuspend = TRUE; // to allow calling stacked functions
                    }
#endif // CALLSTK_DISABLE
                    CCallStack::PrintBugReport(Exception, threadID, ShellExtCrashID, PrintBugReportLine, (void*)file);
#ifndef CALLSTK_DISABLE
                    if (stack != NULL)
                        stack->DontSuspend = oldDontSuspend;
#endif // CALLSTK_DISABLE
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    PrintBugReportLine((void*)file, "(exception in CCallStack::CreateBugReportFile)", FALSE);
                    ret = FALSE;
                }

                NOHANDLES(CloseHandle(file));
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ret = FALSE;
    }

    return ret;
}

int CCallStack::HandleException(EXCEPTION_POINTERS* e, DWORD shellExtCrashID, const char* iconOvrlsHanName)
{
    // WARNING - an exception occurred and we would like to signal it to salmon.exe (to generate a minidump)
    // only the absolute minimum API calls should be made (the system may be in critical sections, the restrictions are even greater than in DllMain)

    // In DEBUG version we take care of the debugger, in SDK/RELEASE version we prefer not to do this test
#if defined(_DEBUG) && !defined(ENABLE_BUGREPORT_DEBUGGING)
    if (IsDebuggerPresent())              // If the software is currently being debugged, it doesn't make sense to display the Bug Report window, etc.
        return EXCEPTION_CONTINUE_SEARCH; // throw an exception further ... debugger catches it
#endif

    static char bugReportPath[MAX_PATH];

    // ask salmon to generate a minidump
    SalmonFireAndWait(e, bugReportPath);

    // delayed behind the minidump, but more certain that there is a greater chance for a minidump
    SalamanderExceptionTime = GetTickCount();

    static DWORD curThreadID;
    curThreadID = GetCurrentThreadId();
    TRACE_I("Exception 0x" << std::hex << e->ExceptionRecord->ExceptionCode << " in address " << e->ExceptionRecord->ExceptionAddress << ", thread ID = " << std::dec << curThreadID);

    // Wait until our exception can be processed
    while (CCallStack::ExceptionExists)
        Sleep(1000);
    CCallStack::ExceptionExists = TRUE; // now our exception is being processed

    /*  // It's about opening dialog windows, so it cannot be used
  // First, we stop other threads to keep their call stack in a usable state
  int i;
  for (i = 0; i < CCallStack::CallStacks.Count; i++)
  {
    CCallStack *stack = CCallStack::CallStacks[i];
    if (stack->ThreadID != curThreadID && stack->ThreadHandle != NULL)
    {
      SuspendThread(stack->ThreadHandle);
    }
  }*/

    TBRData.Exception = e;
    TBRData.ExitProcess = FALSE; // unnecessary, output parameter, but ...
    TBRData.CurrentThreadID = curThreadID;
    TBRData.ShellExtCrashID = shellExtCrashID;
    TBRData.IconOvrlsHanName = iconOvrlsHanName;
    TBRData.BugReportPath = bugReportPath;

    // if the bugreport thread is running, we will not generate a report in it
    BOOL reportInThisThread = TRUE;

    if (BugReportThread != NULL)
    {
        ResetEvent(TBRData.EventProcessed);
        SetEvent(TBRData.Event);
        DWORD waitRet = WaitForSingleObject(TBRData.EventProcessed,
#if defined(_DEBUG) && defined(ENABLE_BUGREPORT_DEBUGGING)
                                            INFINITE); // wait, otherwise the software terminates when stepping through in the debugger
#else
                                            6000); // give fiber in the background 6s to generate a report
#endif
        if (waitRet != WAIT_OBJECT_0)
            reportInThisThread = TRUE; // in case of an error or timeout, we will try to generate a report here
        else
            reportInThisThread = TBRData.EventProcessedRet == FALSE; // if thread generation failed, we'll try here
    }

    // if the bug report thread did not run or it was not possible to generate a report in it, we will try again in this thread
    if (reportInThisThread)
    {
        // generating two reports concurrently caused issues for me (both .BUG and .DMP were incomplete with a timeout of 200ms) - stopping the background thread
        if (BugReportThread)
            SuspendThread(BugReportThread);

        // We will try to generate the report in this thread
        CreateBugReportFile(e, curThreadID, shellExtCrashID, bugReportPath); // create a record of the error on the disk

        if (BugReportThread)
            ResumeThread(BugReportThread);
    }

    TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
    // we won't get there anymore
    return EXCEPTION_CONTINUE_SEARCH; // throw an exception further ... crash or debugger
}

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
void CCallStack::CheckCallFrequency(DWORD_PTR callerAddress, LARGE_INTEGER* pushTime, LARGE_INTEGER* afterPushTime)
{
    __int64 monitoringPeriod = CALLSTACK_MONITORINGPERIOD * CCallStack::SavedPerfFreq.QuadPart / 1000;
    __int64 oldestPerfTime = pushTime->QuadPart - monitoringPeriod;
    if (NewestItemIndex != -1) // try to find a record of the previous call of the same call-stack macro
    {
        int act = NewestItemIndex;
        while (1)
        {
            if (MonitoredItems[act].CallerAddress == callerAddress) // found
            {
                CCallStackMonitoredItem* item = &MonitoredItems[act];
                if (item->MonitoringStartPerfTime >= oldestPerfTime) // we still need to watch him
                {
                    item->NumberOfCalls++;
                    item->PushesPerfTime += afterPushTime->QuadPart - pushTime->QuadPart;
                    __int64 totalTime = pushTime->QuadPart - item->MonitoringStartPerfTime;
                    if (item->NotAlreadyReported &&
                        totalTime > monitoringPeriod / 2 && item->PushesPerfTime > totalTime / CALLSTK_MONITOR_SHOWINFORATIO)
                    {                                                                                                                                                                                 // we print this trace only once (when critical time is exceeded, we will print the final state later)
                        DWORD aproxMinCallsCount = (DWORD)((SpeedBenchmark * totalTime / (CALLSTK_BENCHMARKTIME * CCallStack::SavedPerfFreq.QuadPart / 1000)) / (3 * CALLSTK_MONITOR_SHOWINFORATIO)); // we calculate 3 times slower than the measured call-stack macro
                        if (SpeedBenchmark != 0 && item->NumberOfCalls > aproxMinCallsCount)
                        { // suppression of cases of increased time when replanning processes/threads (able to measure 9ms for 4 call stack calls)
                            item->NotAlreadyReported = FALSE;
                            if (item->PushesPerfTime > totalTime / CALLSTK_MONITOR_SHOWERRORRATIO)
                            {
                                TRACE_E("Too Frequently Used Call Stack Message: time ratio callstack/total: " << (totalTime > 0 ? (DWORD)(100 * item->PushesPerfTime / totalTime) : 0) << "%, call address: 0x" << std::hex << item->CallerAddress << std::dec << " (see next line in trace-server for text), current push-time: " << (DWORD)(item->PushesPerfTime * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, current total time: " << (DWORD)(totalTime * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, current number of calls in monitored period: " << item->NumberOfCalls);
                            }
                            else
                            {
                                TRACE_I("Maybe Too Frequently Used Call Stack Message: time ratio callstack/total: " << (totalTime > 0 ? (DWORD)(100 * item->PushesPerfTime / totalTime) : 0) << "%, call address: 0x" << std::hex << item->CallerAddress << std::dec << " (see next line in trace-server for text), current push-time: " << (DWORD)(item->PushesPerfTime * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, current total time: " << (DWORD)(totalTime * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, current number of calls in monitored period: " << item->NumberOfCalls);
                            }
                            if (Skipped == 0 && End > Text) // print the text of the last push
                            {
                                char* end = End;
                                end -= 2;
                                end -= 1 + *(short*)end;
                                TRACE_I("Top of Call Stack: " << end);
                            }
                        }
                    }
                    return; // done
                }
                else
                    break; // we are going to add the call back to the queue again
            }
            if (act == OldestItemIndex)
                break; // not found
            act--;
            if (act < 0)
                act = MonitoredItemsSize - 1;
        }
    }
    if (OldestItemIndex != -1) // first we discard outdated items (make room for a new item)
    {
        while (1)
        {
            if (MonitoredItems[OldestItemIndex].MonitoringStartPerfTime >= oldestPerfTime) // we still need to watch him
                break;
            else
            {
                if (!MonitoredItems[OldestItemIndex].NotAlreadyReported) // if reported, we report final values
                {
                    TRACE_I("Info for Too Frequently Used Call Stack Message: call address: 0x" << std::hex << MonitoredItems[OldestItemIndex].CallerAddress << std::dec << ", total push-time: " << (DWORD)(MonitoredItems[OldestItemIndex].PushesPerfTime * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, total number of calls in monitored period: " << MonitoredItems[OldestItemIndex].NumberOfCalls);
                }
            }
            if (OldestItemIndex == NewestItemIndex)
            {
                OldestItemIndex = NewestItemIndex = -1; // queue has emptied
                break;
            }
            OldestItemIndex++;
            if (OldestItemIndex >= MonitoredItemsSize)
                OldestItemIndex = 0;
        }
    }
    if (MonitoredItemsSize == 0) // queue has not been allocated yet, we are allocating it now
    {
        MonitoredItems = (CCallStackMonitoredItem*)malloc(CALLSTACK_MONITOREDITEMS_BASE * sizeof(CCallStackMonitoredItem));
        if (MonitoredItems == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return;
        }
        MonitoredItemsSize = CALLSTACK_MONITOREDITEMS_BASE;
    }
    if (NewestItemIndex != -1 &&
        (NewestItemIndex < MonitoredItemsSize - 1 && NewestItemIndex + 1 == OldestItemIndex ||
         NewestItemIndex == MonitoredItemsSize - 1 && OldestItemIndex == 0))
    { // the queue is full, it needs to be enlarged
        CCallStackMonitoredItem* newMonitoredItems = (CCallStackMonitoredItem*)malloc((MonitoredItemsSize +
                                                                                       CALLSTACK_MONITOREDITEMS_DELTA) *
                                                                                      sizeof(CCallStackMonitoredItem));
        if (newMonitoredItems == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return;
        }

        memcpy(newMonitoredItems, MonitoredItems, (NewestItemIndex + 1) * sizeof(CCallStackMonitoredItem));
        if (OldestItemIndex > 0)
        {
            memcpy(newMonitoredItems + OldestItemIndex + CALLSTACK_MONITOREDITEMS_DELTA,
                   MonitoredItems + OldestItemIndex, (MonitoredItemsSize - OldestItemIndex) * sizeof(CCallStackMonitoredItem));
            OldestItemIndex += CALLSTACK_MONITOREDITEMS_DELTA;
        }
        free(MonitoredItems);
        MonitoredItems = newMonitoredItems;
        MonitoredItemsSize += CALLSTACK_MONITOREDITEMS_DELTA;
    }
    if (NewestItemIndex == -1)
        NewestItemIndex = OldestItemIndex = 0;
    else
    {
        NewestItemIndex++;
        if (NewestItemIndex == MonitoredItemsSize)
            NewestItemIndex = 0;
    }
    CCallStackMonitoredItem* item = &MonitoredItems[NewestItemIndex];
    item->MonitoringStartPerfTime = pushTime->QuadPart;
    item->CallerAddress = callerAddress;
    item->NumberOfCalls = 1;
    item->PushesPerfTime = afterPushTime->QuadPart - pushTime->QuadPart;
    item->NotAlreadyReported = TRUE;
}
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

//
// ****************************************************************************
// CCallStackMessage
//

// to avoid including the entire intrin.h
extern "C"
{
    void* _AddressOfReturnAddress(void);
}

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
CCallStackMessage::CCallStackMessage(BOOL doNotMeasureTimes, int dummy, const char* format, ...)
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
CCallStackMessage::CCallStackMessage(const char* format, ...)
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
{
    va_list args;
    va_start(args, format);
    CCallStack* stack = CCallStack::GetThis();
    if (stack != NULL)
    {
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        LARGE_INTEGER pushTime;
        QueryPerformanceCounter(&pushTime);
#endif                             // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        stack->Push(format, args); // uncaught threads have TLS set to NULL
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        QueryPerformanceCounter(&StartTime);
        PushesCounterStart = stack->PushesCounter;
        if (!doNotMeasureTimes)
            stack->PushPerfTimeCounter.QuadPart += StartTime.QuadPart - pushTime.QuadPart;
        else
            stack->IgnoredPushPerfTimeCounter.QuadPart += StartTime.QuadPart - pushTime.QuadPart;
        PushPerfTimeCounterStart.QuadPart = stack->PushPerfTimeCounter.QuadPart;
        IgnoredPushPerfTimeCounterStart.QuadPart = stack->IgnoredPushPerfTimeCounter.QuadPart;

        __try
        {
            PushCallerAddress = *(DWORD_PTR*)_AddressOfReturnAddress();
            if (!doNotMeasureTimes)
                stack->CheckCallFrequency(PushCallerAddress, &pushTime, &StartTime);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            PushCallerAddress = 0;
        }
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    }
    else
    {
        TRACE_E("Invalid use of CALL_STACK_MESSAGE: call-stack object was not defined in this thread. Format=\"" << format << "\"");
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        PushesCounterStart = 0;
        StartTime.QuadPart = 0;
        PushCallerAddress = 0;
        PushPerfTimeCounterStart.QuadPart = 0;
        IgnoredPushPerfTimeCounterStart.QuadPart = 0;
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    }
    va_end(args);
}

CCallStackMessage::~CCallStackMessage()
{
    DWORD savedLastErr = GetLastError();
    CCallStack* stack = CCallStack::GetThis();
    if (stack != NULL)
    {
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        BOOL printCallStackTop = FALSE;
        __int64 internalPushPerfTime = stack->PushPerfTimeCounter.QuadPart - PushPerfTimeCounterStart.QuadPart;
        if (internalPushPerfTime > 0) // only if there were any nested Push calls
        {
            LARGE_INTEGER endTime;
            QueryPerformanceCounter(&endTime);
            __int64 realPerfTime = (endTime.QuadPart - StartTime.QuadPart) -
                                   (stack->IgnoredPushPerfTimeCounter.QuadPart - IgnoredPushPerfTimeCounterStart.QuadPart); // subtract the times of ignored (unmeasured) pushes, otherwise they would just meaninglessly improve the ratio
            if (realPerfTime / internalPushPerfTime < CALLSTK_MINRATIO && realPerfTime > 0 &&
                ((realPerfTime * 1000) / CCallStack::SavedPerfFreq.QuadPart) > CALLSTK_MINWARNTIME)
            {
                DWORD aproxMinCallsCount = (DWORD)(((CCallStack::SpeedBenchmark * (internalPushPerfTime + stack->IgnoredPushPerfTimeCounter.QuadPart - IgnoredPushPerfTimeCounterStart.QuadPart) * 1000) /
                                                    (CALLSTK_BENCHMARKTIME * CCallStack::SavedPerfFreq.QuadPart)) /
                                                   3); // we calculate 3 times slower than the measured call-stack macro
                if (CCallStack::SpeedBenchmark != 0 && aproxMinCallsCount < stack->PushesCounter - PushesCounterStart)
                { // suppression of cases of time increase when re-scheduling processes/threads (able to measure 50ms for 280 call stack invocations)
                    TRACE_E("Call Stack Messages Slowdown Detected: time ratio callstack/total: " << (DWORD)(100 * internalPushPerfTime / realPerfTime) << "%, total time: " << (DWORD)((realPerfTime * 1000) / CCallStack::SavedPerfFreq.QuadPart) << "ms, push time: " << (DWORD)(internalPushPerfTime * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, ignored pushes: " << (DWORD)((stack->IgnoredPushPerfTimeCounter.QuadPart - IgnoredPushPerfTimeCounterStart.QuadPart) * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, call address: 0x" << std::hex << PushCallerAddress << std::dec << " (see next line in trace-server for text), count: " << (stack->PushesCounter - PushesCounterStart));
                    printCallStackTop = TRUE;
                }
            }
        }
        stack->Pop(printCallStackTop); // uncaught threads have TLS set to NULL
#else                                  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        stack->Pop(); // uncaught threads have TLS set to NULL
#endif                                 // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    }
    else
    {
        TRACE_E("Invalid use of CALL_STACK_MESSAGE: call-stack object was not defined in this thread.");
    }
    SetLastError(savedLastErr);
}

#endif // CALLSTK_DISABLE
