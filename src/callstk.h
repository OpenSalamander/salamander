// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// CALLSTK_DISABLE macro - completely disables this module
// CALLSTK_MEASURETIMES macro - enables measuring the time spent preparing
//                              call-stack reports (the ratio is measured against
//                              the total execution time of functions). NOTE: must also be
//                              enabled separately for each plugin
// CALLSTK_DISABLEMEASURETIMES macro - suppresses the time measurement for preparing call-stack reports in DEBUG builds

// overview of macro types (all are non-empty unless CALLSTK_DISABLE is defined)
// CALL_STACK_MESSAGE - standard call-stack macro
// SLOW_CALL_STACK_MESSAGE - call-stack macro that ignores any peformance slowdown (Use at
//                           points where we know it significantly slows down execution but we
//                           still need it there.)
// DEBUG_SLOW_CALL_STACK_MESSAGE - call-stack macro that is empty in release builds; behaves like
//                                 SLOW_CALL_STACK_MESSAGE in DEBUG builds. (used in places where we are
//                                 only willing to tolerate slowdown in debug builds, so release version stays fast)

//
// ****************************************************************************

// object holds a list of called functions (the call stack) - lines are added via CCallStackMessage

typedef void (*FPrintLine)(void* param, const char* txt, BOOL tab);

BOOL StartSalmonProcess(BOOL enableRestartAS);

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
#define CALLSTACK_MONITORINGPERIOD 100    // in miliseconds: how long to monitor how many times a single call-stack macro was called
#define CALLSTACK_MONITOREDITEMS_BASE 30  // initial number of items to allocate for the queue of monitored call-stack macro calls
#define CALLSTACK_MONITOREDITEMS_DELTA 20 // how many items to increase the monitored call queue by when more space is needed
struct CCallStackMonitoredItem
{
    __int64 MonitoringStartPerfTime; // since when we have been monitoring this call-stack macro invocation
    DWORD_PTR CallerAddress;         // address where the call-stack macro was invoked
    DWORD NumberOfCalls;             // number of calls during the last CALLSTACK_TRACETIME milliseconds
    __int64 PushesPerfTime;          // total "time" of Push operations for this call-stack macro
    BOOL NotAlreadyReported;         // TRUE = if the issue has not been reported
                                     // (to avoid flooding the Trace Server)
};
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

class CCallStack
{
#ifndef CALLSTK_DISABLE
protected:
    DWORD ThreadID;                  // ID of the current thread
    HANDLE ThreadHandle;             // handle of the current thread; used in
                                     // CCallStack::PrintBugReport via GetThreadContext
    char Text[STACK_CALLS_BUF_SIZE]; // double-null-terminated list; after the
                                     // terminating null, the length of the
                                     // previous string is always stored (two bytes)
                                     // followed immediately by the next string
    char* End;                       // pointer to the last two zeros
    int Skipped;                     // number of messages that could not be stored
    char* Enum;                      // pointer to the last printed text
    BOOL FirstCallstack;             // are we the first instance?

    const char* PluginDLLName; // plug-in DLL currently running in the thread
                               // (NULL if it is salamand.exe)
    int PluginDLLNameUses;     // the Pop() operation count at which PluginDLLName should be set to NULL
                               // (nesting level)

    static DWORD TlsIndex;                        // TLS index always storing 'this'
    static BOOL SectionOK;                        // is the critical section initialized?
    static CRITICAL_SECTION Section;              // critical section for accessing CallStacks
    static TIndirectArray<CCallStack> CallStacks; // array of all call-stacks
    static BOOL ExceptionExists;                  // is an exception already active? (should we suspend?)

    BOOL DontSuspend; // marks a thread displaying bug-report windows

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
#define CALLSTK_MONITOR_SHOWINFORATIO 20     // minimum ratio of actual time to the time spent storing the call-stack message (higher = logs TRACE_I)
#define CALLSTK_MONITOR_SHOWERRORRATIO 10    // minimum ratio of actual time to the time spent storing the call-stack message (higher = logs TRACE_E)
    CCallStackMonitoredItem* MonitoredItems; // circular queue of monitored call-stack macro invocations
    int MonitoredItemsSize;                  // current size of MonitoredItems queue
    int NewestItemIndex;                     // index of the newest item in the MonitoredItems queue; -1 = queue is empty
    int OldestItemIndex;                     // index of the oldest item in the MonitoredItems queue; -1 = queue is empty

public:
    DWORD PushesCounter;                      // counter of Push calls invoked in this object (in one thread)
    LARGE_INTEGER PushPerfTimeCounter;        // total time spent in Push calls invoked in this object (one thread)
    LARGE_INTEGER IgnoredPushPerfTimeCounter; // total time spent in unmeasured (ignored) Push calls invoked here (one thread)
    static LARGE_INTEGER SavedPerfFreq;       // nonzero result of QueryPerformanceFrequency
#define CALLSTK_BENCHMARKTIME 100             // time in milliseconds during which the call-stack speed is measured
    static DWORD SpeedBenchmark;              // how many measured call-stack macros can be pushed+poped within CALLSTK_BENCHMARKTIME milliseconds
#endif                                        // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

public:
    CCallStack(BOOL dontSuspend = FALSE);
    ~CCallStack();

    static CCallStack* GetThis()
    {
        return (CCallStack*)TlsGetValue(TlsIndex);
    }

    void Push(const char* format, va_list args);

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    void Pop(BOOL printCallStackTop);
    void CheckCallFrequency(DWORD_PTR callerAddress, LARGE_INTEGER* pushTime, LARGE_INTEGER* afterPushTime);
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    void Pop();
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

    // called only when storing a call-stack message from a plug-in
    void PushPluginDLLName(const char* dllName)
    {
        if (PluginDLLName == NULL)
        {
            PluginDLLName = dllName;
            PluginDLLNameUses = 1;
        }
        else
            PluginDLLNameUses++;
    }

    // called only when retrieving a call-stack message from a plug-in
    void PopPluginDLLName()
    {
        if (PluginDLLNameUses <= 1)
        {
            PluginDLLName = NULL;
            PluginDLLNameUses = 0;
        }
        else
            PluginDLLNameUses--;
    }

    const char* GetPluginDLLName() { return PluginDLLName; }

    void Reset() // start enumerating lines
    {
        Enum = Text;
    }

    const char* GetNextLine(); // returns the next line or NULL if none remain

    static void ReleaseBeforeExitThread(); // release call-stack object data in the current thread (used before triggering an exit inside a monitored region)
    void ReleaseBeforeExitThreadBody();    // called from ReleaseBeforeExitThread() after locating the call-stack object in TLS

    static int HandleException(EXCEPTION_POINTERS* e, DWORD shellExtCrashID = -1,
                               const char* iconOvrlsHanName = NULL); // called from the exception handler
    static DWORD WINAPI ThreadBugReportF(void* exitProcess);         // thread that opens the bug report dialog
    // calls PrintBugReport into the bug report
    static BOOL CreateBugReportFile(EXCEPTION_POINTERS* Exception, DWORD ThreadID, DWORD ShellExtCrashID, const char* bugReportFileName);

    // function for printing exception information (used both to a file and to a window)
    // when Exception==NULL, it's not an exception (user manually opened the Bug Report dialog)
    static void PrintBugReport(EXCEPTION_POINTERS* Exception, DWORD ThreadID, DWORD ShellExtCrashID,
                               FPrintLine PrintLine, void* param);
#endif // CALLSTK_DISABLE
};

// stores the message on the call stack and removes it upon leaving the block; parameters are the same as print functions

#ifndef CALLSTK_DISABLE

class CCallStackMessage
{
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
public:
#define CALLSTK_MINWARNTIME 200                    // minimum time between Push and Pop in milliseconds to report a warning
#define CALLSTK_MINRATIO 10                        // minimum allowed ratio of actual time to time spent storing messages on the call-stack (10 = less than 1/10 of total execution time was spent generating the call stack; otherwise, it triggers an alert)
    DWORD PushesCounterStart;                      // starting value of the Push counter called in this thread
    LARGE_INTEGER PushPerfTimeCounterStart;        // starting value of the performance time counter for Push methods called in this thread
    LARGE_INTEGER IgnoredPushPerfTimeCounterStart; // starting value of the performance time counter for unmeasured (ignored) Push methods in this thread
    LARGE_INTEGER StartTime;                       // timestamp when this call-stack macro was pushed
    DWORD_PTR PushCallerAddress;                   // address where CALL_STACK_MESSAGE macro was invoked (address of the Push)
#endif                                             // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

public:
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    // 'doNotMeasureTimes'==TRUE means the Push of this call-stack macro should
    // not be measured (it probably slows things down a lot, but
    // we don’t want to remove it, as it is too important for debugging)
    CCallStackMessage(BOOL doNotMeasureTimes, int dummy, const char* format, ...);
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    CCallStackMessage(const char* format, ...);
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

    ~CCallStackMessage();
};

#define CALLSTK_TOKEN(x, y) x##y
#define CALLSTK_TOKEN2(x, y) CALLSTK_TOKEN(x, y)
#define CALLSTK_UNIQUE(varname) CALLSTK_TOKEN2(varname, __COUNTER__)

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

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

#else // _DEBUG

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

// empty macro: informs CheckStk that we do not want a call-stack message for
// this function
#define CALL_STACK_MESSAGE_NONE
