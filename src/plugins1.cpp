// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"
#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "stswnd.h"
#include "mainwnd.h"
#include "zip.h"
#include "pack.h"
#include "cache.h"
#include <uxtheme.h>
#include "dialogs.h"

CPlugins Plugins;

// global "time" (counter) for obtaining the "time" of FS creation
DWORD CPluginFSInterfaceEncapsulation::PluginFSTime = 1; // zero is used as "uninitialized time"

// ****************************************************************************

int AlreadyInPlugin = 0;

void EnterPlugin()
{
    if (AlreadyInPlugin == 0)
    {
#ifdef _DEBUG
        // Checksum for the method CSalamanderGeneral::GetMsgBoxParent()
        // in case of an error, it is necessary to add another window for PluginMsgBoxParent,
        // If the following code changes, it is necessary to change it in CSalamanderGeneral::GetMsgBoxParent as well
        HWND wnd = PluginProgressDialog != NULL ? PluginProgressDialog : PluginMsgBoxParent;
        if (!IsWindowEnabled(wnd))
        {
            TRACE_E("CSalamanderGeneral::GetMsgBoxParent() will return incorrect value (0x" << wnd << ")!");
        }
#endif // _DEBUG

        AllowChangeDirectory(FALSE); // We do not want the current directory to change automatically
        BeginStopRefresh(TRUE);      // We do not want the panels to refresh during the plug-in call
        AlreadyInPlugin = 1;
    }
    else
    {
        //    TRACE_I("Warning! Plugin was entered multiple times.");
        AlreadyInPlugin++;
    }
}

void LeavePlugin()
{
    if (AlreadyInPlugin == 1)
    {
        // measures to prevent the interruption of the listing in the panel after each ESC in the plugin (mainly
        // closing modal dialogs)
        WaitForESCReleaseBeforeTestingESC = TRUE;

        EndStopRefresh(TRUE, TRUE);
        AllowChangeDirectory(TRUE);
        AlreadyInPlugin = 0;

        if (MainWindow != NULL && MainWindow->NeedToResentDispachChangeNotif &&
            StopRefresh == 0) // if you haven't left the stop-refresh mode yet, sending a message doesn't make sense
        {
            MainWindow->NeedToResentDispachChangeNotif = FALSE;

            // Request sending messages about road changes
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            PostMessage(MainWindow->HWindow, WM_USER_DISPACHCHANGENOTIF, 0, t1);
        }
    }
    else
    {
        if (AlreadyInPlugin == 0)
            TRACE_E("Unmatched call to LeavePlugin()!");
        else
            AlreadyInPlugin--;
    }
}

//
// ****************************************************************************
// CPluginFSInterfaceEncapsulation
//

BOOL CPluginFSInterfaceEncapsulation::IsFSNameFromSamePluginAsThisFS(const char* fsName, int& fsNameIndex)
{
    CALL_STACK_MESSAGE4("CPluginFSInterfaceEncapsulation::IsFSNameFromSamePluginAsThisFS(%s,) (%s v. %s)",
                        fsName, DLLName, Version);

    return Plugins.AreFSNamesFromSamePlugin(PluginFSName, fsName, fsNameIndex);
}

BOOL CPluginFSInterfaceEncapsulation::IsPathFromThisFS(const char* fsName, const char* fsUserPart)
{
    CALL_STACK_MESSAGE5("CPluginFSInterfaceEncapsulation::IsPathFromThisFS(%s, %s) (%s v. %s)",
                        fsName, fsUserPart, DLLName, Version);

    int fsNameIndex;
    if (IsFSNameFromSamePluginAsThisFS(fsName, fsNameIndex)) // Is the name of FS from the same plugin?
    {
        return IsOurPath(PluginFSNameIndex, fsNameIndex, fsUserPart); // Is the user-part correct?
    }
    return FALSE; // not our path
}

BOOL CPluginFSInterfaceEncapsulation::ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                                      CPluginDataInterfaceAbstract*& pluginData,
                                                      int& iconsType, BOOL forceRefresh)
{
    CALL_STACK_MESSAGE4("CPluginFSInterfaceEncapsulation::ListCurrentPath(, , , %d) (%s v. %s)",
                        forceRefresh, DLLName, Version);
    EnterPlugin();
    //TRACE_I("list path: begin");
    BOOL r = Interface->ListCurrentPath(dir, pluginData, iconsType, forceRefresh);
    //TRACE_I("list path: end");
#ifdef _DEBUG
    if (r && pluginData != NULL) // increase OpenedPDCounter
    {
        CPluginData* data = Plugins.GetPluginData(Iface);
        if (data != NULL)
            data->OpenedPDCounter++;
        else
            TRACE_E("Unexpected situation in CPluginFSInterfaceEncapsulation::ListCurrentPath()");
    }
#endif // _DEBUG
    CALL_STACK_MESSAGE1("CPluginFSInterface::GetSupportedServices()");
    SupportedServices = Interface->GetSupportedServices();
    LeavePlugin();
    return r;
}

BOOL CPluginFSInterfaceEncapsulation::GetChangeDriveOrDisconnectItem(const char* fsName, char*& title,
                                                                     HICON& icon, BOOL& destroyIcon)
{
    CALL_STACK_MESSAGE4("CPluginFSInterfaceEncapsulation::GetChangeDriveOrDisconnectItem(%s, , ,) (%s v. %s)",
                        fsName, DLLName, Version);
    if (IsServiceSupported(FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM))
    {
        EnterPlugin();
        BOOL r = Interface->GetChangeDriveOrDisconnectItem(fsName, title, icon, destroyIcon);
        if (r && icon != NULL && destroyIcon) // add handle to 'icon' to HANDLES
            HANDLES_ADD(__htIcon, __hoLoadImage, icon);
        LeavePlugin();
        return r;
    }
    else
        return FALSE;
}

HICON
CPluginFSInterfaceEncapsulation::GetFSIcon(BOOL& destroyIcon)
{
    CALL_STACK_MESSAGE3("CPluginFSInterfaceEncapsulation::GetFSIcon() (%s v. %s)",
                        DLLName, Version);
    if (IsServiceSupported(FS_SERVICE_GETFSICON))
    {
        EnterPlugin();
        HICON r = Interface->GetFSIcon(destroyIcon);
        if (r != NULL && destroyIcon) // add handle to the returned icon to HANDLES
            HANDLES_ADD(__htIcon, __hoLoadImage, r);
        LeavePlugin();
        return r;
    }
    else
        return NULL;
}

//
// ****************************************************************************
// CPluginInterfaceEncapsulation
//

void CPluginInterfaceForFSEncapsulation::CloseFS(CPluginFSInterfaceAbstract* fs)
{
    CPluginData* data = Plugins.GetPluginData(Interface);
    if (data == NULL || !data->GetLoaded())
    {
        TRACE_E("Incorrect call to CPluginInterfaceForFSEncapsulation::CloseFS()");
        return;
    }
#ifdef _DEBUG
    if (--data->OpenedFSCounter < 0)
    {
        TRACE_E("OpenedFSCounter is negative number - too much calls to CloseFS()");
    }
#endif // _DEBUG
    CALL_STACK_MESSAGE3("CPluginInterfaceForFSEncapsulation::CloseFS() (%s v. %s)",
                        data->DLLName, data->Version);
    EnterPlugin();
    Interface->CloseFS(fs);
    Plugins.KillPluginFSTimer(fs, TRUE, 0); // we need to cancel timers of the closed FS (they would not be delivered, TRACE_E would appear)
    LeavePlugin();

    if (MainWindow != NULL)
    {
        MainWindow->ClearPluginFSFromHistory(fs);
        if (MainWindow->LeftPanel != NULL)
            MainWindow->LeftPanel->ClearPluginFSFromHistory(fs);
        if (MainWindow->RightPanel != NULL)
            MainWindow->RightPanel->ClearPluginFSFromHistory(fs);
    }
}

void CPluginInterfaceForFSEncapsulation::ExecuteChangeDriveMenuItem(int panel)
{
    EnterPlugin();
    Interface->ExecuteChangeDriveMenuItem(panel);
    LeavePlugin();
}

BOOL CPluginInterfaceForFSEncapsulation::ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                                        CPluginFSInterfaceAbstract* pluginFS,
                                                                        const char* pluginFSName, int pluginFSNameIndex,
                                                                        BOOL isDetachedFS, BOOL& refreshMenu,
                                                                        BOOL& closeMenu, int& postCmd, void*& postCmdParam)
{
    EnterPlugin();
    BOOL r = Interface->ChangeDriveMenuItemContextMenu(parent, panel, x, y, pluginFS,
                                                       pluginFSName, pluginFSNameIndex,
                                                       isDetachedFS, refreshMenu,
                                                       closeMenu, postCmd, postCmdParam);
    LeavePlugin();
    return r;
}

void CPluginInterfaceForFSEncapsulation::ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam)
{
    CPluginData* data = Plugins.GetPluginData(Interface);
    if (data == NULL || !data->GetLoaded())
    {
        TRACE_E("Incorrect call to CPluginInterfaceForFSEncapsulation::ExecuteChangeDrivePostCommand()");
        return;
    }
    CALL_STACK_MESSAGE5("CPluginInterfaceForFSEncapsulation::ExecuteChangeDrivePostCommand(%d, %d, ) (%s v. %s)",
                        panel, postCmd, data->DLLName, data->Version);
    EnterPlugin();
    Interface->ExecuteChangeDrivePostCommand(panel, postCmd, postCmdParam);
    LeavePlugin();
}

void CPluginInterfaceForFSEncapsulation::ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                                     const char* pluginFSName, int pluginFSNameIndex,
                                                     CFileData& file, int isDir)
{
    CPluginData* data = Plugins.GetPluginData(Interface);
    if (data == NULL || !data->GetLoaded())
    {
        TRACE_E("Incorrect call to CPluginInterfaceForFSEncapsulation::ExecuteOnFS()");
        return;
    }
    CALL_STACK_MESSAGE7("CPluginInterfaceForFSEncapsulation::ExecuteOnFS(%d, , %s, %d, , %d) (%s v. %s)",
                        panel, pluginFSName, pluginFSNameIndex, isDir, data->DLLName, data->Version);
    EnterPlugin();
    Interface->ExecuteOnFS(panel, pluginFS, pluginFSName, pluginFSNameIndex, file, isDir);
    LeavePlugin();
}

BOOL CPluginInterfaceForFSEncapsulation::DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                                      CPluginFSInterfaceAbstract* pluginFS,
                                                      const char* pluginFSName, int pluginFSNameIndex)
{
    CPluginData* data = Plugins.GetPluginData(Interface);
    if (data == NULL || !data->GetLoaded())
    {
        TRACE_E("Incorrect call to CPluginInterfaceForFSEncapsulation::DisconnectFS()");
        return FALSE;
    }
    CALL_STACK_MESSAGE7("CPluginInterfaceForFSEncapsulation::DisconnectFS(, %d, %d, , %s, %d) (%s v. %s)",
                        isInPanel, panel, pluginFSName, pluginFSNameIndex, data->DLLName, data->Version);
    EnterPlugin();
    BOOL ret = Interface->DisconnectFS(parent, isInPanel, panel, pluginFS, pluginFSName, pluginFSNameIndex);
    LeavePlugin();
    return ret;
}

void CPluginInterfaceForFSEncapsulation::ConvertPathToInternal(const char* fsName, int fsNameIndex,
                                                               char* fsUserPart)
{
    CPluginData* data = Plugins.GetPluginData(Interface);
    if (data == NULL || !data->GetLoaded())
    {
        TRACE_E("Incorrect call to CPluginInterfaceForFSEncapsulation::ConvertPathToInternal()");
        return;
    }
    CALL_STACK_MESSAGE6("CPluginInterfaceForFSEncapsulation::ConvertPathToInternal(%s, %d, %s) (%s v. %s)",
                        fsName, fsNameIndex, fsUserPart, data->DLLName, data->Version);
    EnterPlugin();
    Interface->ConvertPathToInternal(fsName, fsNameIndex, fsUserPart);
    LeavePlugin();
}

void CPluginInterfaceForFSEncapsulation::ConvertPathToExternal(const char* fsName, int fsNameIndex,
                                                               char* fsUserPart)
{
    CPluginData* data = Plugins.GetPluginData(Interface);
    if (data == NULL || !data->GetLoaded())
    {
        TRACE_E("Incorrect call to CPluginInterfaceForFSEncapsulation::ConvertPathToExternal()");
        return;
    }
    CALL_STACK_MESSAGE6("CPluginInterfaceForFSEncapsulation::ConvertPathToExternal(%s, %d, %s) (%s v. %s)",
                        fsName, fsNameIndex, fsUserPart, data->DLLName, data->Version);
    EnterPlugin();
    Interface->ConvertPathToExternal(fsName, fsNameIndex, fsUserPart);
    LeavePlugin();
}

void CPluginInterfaceEncapsulation::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
    CPluginData* data = Plugins.GetPluginData(Interface);
    if (data == NULL || !data->GetLoaded())
    {
        TRACE_E("Incorrect call to CPluginInterfaceEncapsulation::ReleasePluginDataInterface()");
        return;
    }
#ifdef _DEBUG
    if (--data->OpenedPDCounter < 0)
    {
        TRACE_E("OpenedPDCounter is negative number - too much calls to ReleasePluginDataInterface()");
    }
#endif // _DEBUG
    CALL_STACK_MESSAGE3("CPluginInterfaceEncapsulation::ReleasePluginDataInterface() (%s v. %s)",
                        data->DLLName, data->Version);
    EnterPlugin();
    Interface->ReleasePluginDataInterface(pluginData);
    LeavePlugin();
}

//
// ****************************************************************************
// CPluginDataInterfaceEncapsulation
//

void CPluginDataInterfaceEncapsulation::ReleaseFilesOrDirs(CFilesArray* filesOrDirs, BOOL areDirs)
{
    SLOW_CALL_STACK_MESSAGE4("CPluginDataInterfaceEncapsulation::ReleaseFilesOrDirs(, %d) (%s v. %s)",
                             areDirs, DLLName, Version);
    EnterPlugin();
    int i;
    for (i = 0; i < filesOrDirs->Count; i++)
    {
        Interface->ReleasePluginData(filesOrDirs->At(i), areDirs);
    }
    LeavePlugin();
}

//
// ****************************************************************************
// CSalamanderDebug
//

void CSalamanderDebug::TraceAttachThread(HANDLE thread, unsigned tid)
{
#if defined(MULTITHREADED_TRACE_ENABLE) && defined(TRACE_ENABLE)
    HANDLE handle;
    if (NOHANDLES(DuplicateHandle(GetCurrentProcess(), thread, GetCurrentProcess(), // Cannot use HANDLES -> module
                                  &handle, 0, FALSE, DUPLICATE_SAME_ACCESS)))       // TRACE HANDLES not used
    {
        HANDLES(EnterCriticalSection(&__Trace.CriticalSection));
        if (!__Trace.ThreadCache.Add(handle, tid))
            NOHANDLES(CloseHandle(handle));
        HANDLES(LeaveCriticalSection(&__Trace.CriticalSection));
    }
#endif // defined(MULTITHREADED_TRACE_ENABLE) && defined(TRACE_ENABLE)
}

void CSalamanderDebug::TraceSetThreadName(const char* name)
{
    SetTraceThreadName(name);
}

void CSalamanderDebug::TraceSetThreadNameW(const WCHAR* name)
{
    SetTraceThreadNameW(name);
}

void CSalamanderDebug::SetThreadNameInVC(const char* name)
{
    ::SetThreadNameInVC(name);
}

void CSalamanderDebug::SetThreadNameInVCAndTrace(const char* name)
{
    ::SetThreadNameInVCAndTrace(name);
}

void CSalamanderDebug::TraceConnectToServer()
{
    ConnectToTraceServer();
}

void CSalamanderDebug::AddModuleWithPossibleMemoryLeaks(const char* fileName)
{
#ifdef _DEBUG
    ::AddModuleWithPossibleMemoryLeaks(fileName);
#endif // _DEBUG
}

void CSalamanderDebug::TraceI(const char* file, int line, const char* str)
{
    TRACE_MI(file, line, str);
}

void CSalamanderDebug::TraceIW(const WCHAR* file, int line, const WCHAR* str)
{
    TRACE_MIW(file, line, str);
}

void CSalamanderDebug::TraceE(const char* file, int line, const char* str)
{
    TRACE_ME(file, line, str);
}

void CSalamanderDebug::TraceEW(const WCHAR* file, int line, const WCHAR* str)
{
    TRACE_MEW(file, line, str);
}

unsigned CallWithCallStackEHBody(const char* dllName, const char* version,
                                 unsigned(WINAPI* threadBody)(void*), void* param)
{
    CALL_STACK_MESSAGE3("Plugin Thread (%s v. %s)", dllName, version);
    return threadBody(param);
}

unsigned
CSalamanderDebug::CallWithCallStackEH(unsigned(WINAPI* threadBody)(void*), void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return CallWithCallStackEHBody(DLLName, Version, threadBody, param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread address " << threadBody << ": calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

unsigned
CSalamanderDebug::CallWithCallStack(unsigned(WINAPI* threadBody)(void*), void* param)
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return CallWithCallStackEH(threadBody, param);
}

// to avoid including the entire intrin.h
extern "C"
{
    void* _AddressOfReturnAddress(void);
}

void CSalamanderDebug::Push(const char* format, va_list args, CCallStackMsgContext* callStackMsgContext,
                            BOOL doNotMeasureTimes)
{
#ifndef CALLSTK_DISABLE
    CCallStack* stack = CCallStack::GetThis();
    if (stack != NULL)
    {
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        LARGE_INTEGER pushTime;
        QueryPerformanceCounter(&pushTime);
#endif                             // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        stack->Push(format, args); // uncaught threads have TLS set to NULL
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        if (callStackMsgContext != NULL)
        {
            QueryPerformanceCounter(&callStackMsgContext->StartTime);
            callStackMsgContext->PushesCounterStart = stack->PushesCounter;
            if (!doNotMeasureTimes)
                stack->PushPerfTimeCounter.QuadPart += callStackMsgContext->StartTime.QuadPart - pushTime.QuadPart;
            else
                stack->IgnoredPushPerfTimeCounter.QuadPart += callStackMsgContext->StartTime.QuadPart - pushTime.QuadPart;
            callStackMsgContext->PushPerfTimeCounterStart.QuadPart = stack->PushPerfTimeCounter.QuadPart;
            callStackMsgContext->IgnoredPushPerfTimeCounterStart.QuadPart = stack->IgnoredPushPerfTimeCounter.QuadPart;

            __try
            {
                callStackMsgContext->PushCallerAddress = *(DWORD_PTR*)_AddressOfReturnAddress();
                if (!doNotMeasureTimes)
                {
                    stack->CheckCallFrequency(callStackMsgContext->PushCallerAddress,
                                              &pushTime, &callStackMsgContext->StartTime);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                callStackMsgContext->PushCallerAddress = 0;
            }
        }
        else
        {
            TRACE_I("CSalamanderDebug::Push(): callStackMsgContext == NULL! (not DEBUG version or missing macro CALLSTK_MEASURETIMES)");
        }
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        stack->PushPluginDLLName(DLLName);
    }
    else
    {
        TRACE_E("Invalid use of CALL_STACK_MESSAGE: call-stack object was not defined in this thread. Format=\"" << format << "\"");
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        if (callStackMsgContext != NULL)
        {
            callStackMsgContext->PushesCounterStart = 0;
            callStackMsgContext->StartTime.QuadPart = 0;
            callStackMsgContext->PushCallerAddress = 0;
            callStackMsgContext->PushPerfTimeCounterStart.QuadPart = 0;
            callStackMsgContext->IgnoredPushPerfTimeCounterStart.QuadPart = 0;
        }
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    }
    va_end(args);
#endif // CALLSTK_DISABLE
}

void CSalamanderDebug::Pop(CCallStackMsgContext* callStackMsgContext)
{
#ifndef CALLSTK_DISABLE
    CCallStack* stack = CCallStack::GetThis();
    if (stack != NULL)
    {
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        BOOL printCallStackTop = FALSE;
        if (callStackMsgContext != NULL)
        {
            __int64 internalPushPerfTime = stack->PushPerfTimeCounter.QuadPart - callStackMsgContext->PushPerfTimeCounterStart.QuadPart;
            if (internalPushPerfTime > 0) // only if there were any nested Push calls
            {
                LARGE_INTEGER endTime;
                QueryPerformanceCounter(&endTime);
                __int64 realPerfTime = (endTime.QuadPart - callStackMsgContext->StartTime.QuadPart) -
                                       (stack->IgnoredPushPerfTimeCounter.QuadPart - callStackMsgContext->IgnoredPushPerfTimeCounterStart.QuadPart); // subtract the times of ignored (unmeasured) pushes, otherwise they would just meaninglessly improve the ratio
                if (realPerfTime / internalPushPerfTime < CALLSTK_MINRATIO && realPerfTime > 0 &&
                    ((realPerfTime * 1000) / CCallStack::SavedPerfFreq.QuadPart) > CALLSTK_MINWARNTIME)
                {
                    DWORD aproxMinCallsCount = (DWORD)(((CCallStack::SpeedBenchmark * (internalPushPerfTime + stack->IgnoredPushPerfTimeCounter.QuadPart - callStackMsgContext->IgnoredPushPerfTimeCounterStart.QuadPart) * 1000) /
                                                        (CALLSTK_BENCHMARKTIME * CCallStack::SavedPerfFreq.QuadPart)) /
                                                       3); // we calculate 3 times slower than the measured call-stack macro
                    if (CCallStack::SpeedBenchmark != 0 && aproxMinCallsCount < stack->PushesCounter - callStackMsgContext->PushesCounterStart)
                    { // suppression of cases of time increase when re-scheduling processes/threads (able to measure 50ms for 280 call stack invocations)
                        TRACE_E("Call Stack Messages Slowdown Detected: time ratio callstack/total: " << (DWORD)(100 * internalPushPerfTime / realPerfTime) << "%, total time: " << (DWORD)((realPerfTime * 1000) / CCallStack::SavedPerfFreq.QuadPart) << "ms, push time: " << (DWORD)(internalPushPerfTime * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, ignored pushes: " << (DWORD)((stack->IgnoredPushPerfTimeCounter.QuadPart - callStackMsgContext->IgnoredPushPerfTimeCounterStart.QuadPart) * 1000 / CCallStack::SavedPerfFreq.QuadPart) << "ms, call address: 0x" << std::hex << callStackMsgContext->PushCallerAddress << std::dec << " (see next line in trace-server for text), count: " << (stack->PushesCounter - callStackMsgContext->PushesCounterStart));
                        printCallStackTop = TRUE;
                    }
                }
            }
        }
        else
        {
            TRACE_I("CSalamanderDebug::Pop(): callStackMsgContext == NULL! (not DEBUG version or missing macro CALLSTK_MEASURETIMES)");
        }
        stack->Pop(printCallStackTop); // uncaught threads have TLS set to NULL
#else                                  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        stack->Pop(); // uncaught threads have TLS set to NULL
#endif                                 // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
        stack->PopPluginDLLName();
    }
    else
    {
        TRACE_E("Invalid use of CALL_STACK_MESSAGE: call-stack object was not defined in this thread.");
    }
#endif // CALLSTK_DISABLE
}

//
// ****************************************************************************
// CSalamanderConnect
//

void CSalamanderConnect::AddCustomPacker(const char* title, const char* defaultExtension, BOOL update)
{
    CALL_STACK_MESSAGE4("CSalamanderConnect::AddCustomPacker(%s, %s, %d)", title, defaultExtension, update);
    if (CustomPack)
    {
        int i = PackerConfig.AddPacker(TRUE);
        if (i != -1)
            PackerConfig.SetPacker(i, -Index - 1, title, defaultExtension, FALSE);
    }
    else
    {
        if (update)
        {
            int i;
            for (i = 0; i < PackerConfig.GetPackersCount(); i++)
            {
                if (PackerConfig.GetPackerType(i) == -Index - 1) // found, we will perform data recovery
                {
                    PackerConfig.SetPacker(i, -Index - 1, title, defaultExtension, FALSE);
                    return;
                }
            }
        }
    }
}

void CSalamanderConnect::AddCustomUnpacker(const char* title, const char* masks, BOOL update)
{
    CALL_STACK_MESSAGE4("CSalamanderConnect::AddCustomUnpacker(%s, %s, %d)", title, masks, update);
    if (CustomUnpack)
    {
        int i = UnpackerConfig.AddUnpacker(TRUE);
        if (i != -1)
            UnpackerConfig.SetUnpacker(i, -Index - 1, title, masks, FALSE);
    }
    else
    {
        if (update)
        {
            int i;
            for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
            {
                if (UnpackerConfig.GetUnpackerType(i) == -Index - 1) // found, we will perform data recovery
                {
                    UnpackerConfig.SetUnpacker(i, -Index - 1, title, masks, FALSE);
                    return;
                }
            }
        }
    }
}

void CSalamanderConnect::AddViewer(const char* masks, BOOL force)
{
    CALL_STACK_MESSAGE3("CSalamanderConnect::AddViewer(%s, %d)", masks, force);
    if (strchr(masks, '|') != NULL)
    {
        TRACE_E("CSalamanderConnect::AddViewer(): you can not use character '|', sorry"); // '|' is negation in group-masks, combining masks in GetViewersAssoc is not prepared for that
        return;
    }
    if (Viewer || force)
    {
        char ext[300];        // copy masks (';' replaced by '\0')
        char ext2[300];       // for breaking down found masks (';' replaced by '\0'), also for storing the result in "force"
        if (!Viewer && force) // This is an update, not an installation, we will check if it is not already on the list
        {
            int len = (int)strlen(masks);
            if (len > 299)
                len = 299;
            memcpy(ext, masks, len);
            ext[len] = 0;
            TDirectArray<char*> extArray(10, 5); // array of suffixes from masks
            char* s = ext + len;
            while (s > ext)
            {
                while (--s >= ext)
                {
                    if (*s == ';')
                    {
                        char* p = s;
                        while (--p >= ext && *p == ';')
                            ;
                        if (((s - p) & 1) == 1)
                            break;
                        s = p + 1;
                    }
                }
                if (s >= ext)
                    *s = 0;
                char* ss = s + 1 + strlen(s + 1);
                while (--ss >= s + 1 && *ss <= ' ')
                    ;
                *(ss + 1) = 0; // trim spaces at the end of the mask
                ss = s + 1;
                while (*ss != 0 && *ss <= ' ')
                    ss++;     // skip spaces at the beginning of the mask
                if (*ss != 0) // if the mask is not empty, we add it to the array
                {
                    extArray.Insert(0, ss); // some of the extensions in the array, O(n^2) to preserve the order
                    if (!extArray.IsGood())
                        extArray.ResetState();
                }
            }

            // We will trim the list of extensions (masks) to those already registered...
            int i;
            for (i = 0; i < MainWindow->ViewerMasks->Count; i++)
            {
                if (MainWindow->ViewerMasks->At(i)->ViewerType == -Index - 1) // correct plugin
                {
                    const char* m = MainWindow->ViewerMasks->At(i)->Masks->GetMasksString();
                    len = (int)strlen(m);
                    if (len > 299)
                        len = 299;
                    memcpy(ext2, m, len);
                    ext2[len] = 0;
                    s = ext2 + len;
                    while (s > ext2)
                    {
                        while (--s >= ext2)
                        {
                            if (*s == ';')
                            {
                                char* p = s;
                                while (--p >= ext2 && *p == ';')
                                    ;
                                if (((s - p) & 1) == 1)
                                    break;
                                s = p + 1;
                            }
                        }
                        if (s >= ext2)
                            *s = 0;
                        char* ss = s + 1 + strlen(s + 1);
                        while (--ss >= s + 1 && *ss <= ' ')
                            ;
                        *(ss + 1) = 0; // trim spaces at the end of the mask
                        ss = s + 1;
                        while (*ss != 0 && *ss <= ' ')
                            ss++; // skip spaces at the beginning of the mask
                        int k;
                        for (k = 0; k < extArray.Count; k++)
                        {
                            if (StrICmp(ss, extArray[k]) == 0) // we already have this mask, we won't add it
                            {
                                extArray.Delete(k);
                                if (!extArray.IsGood())
                                    extArray.ResetState();
                                k--;
                            }
                        }
                    }
                }
            }
            if (extArray.Count == 0)
                return; // empty mask -> nothing to do
            // reconnecting the mask 'masks' (useful residue)
            s = ext2;
            int k;
            for (k = 0; k < extArray.Count; k++)
            {
                if (extArray[k][0] == ';' && s != ext2)
                    *s++ = ' '; // Space is necessary (otherwise the previous ';' will not be a separator, but will be joined with this ';')
                strcpy(s, extArray[k]);
                if (k + 1 < extArray.Count)
                    strcat(s, ";");
                s += strlen(s);
            }
            masks = ext2;
        }

        if (Viewer && !force || // Installing the plug-in
            !Viewer && force)   // update the plug-in, but not during its installation
        {
            CViewerMasksItem* item = new CViewerMasksItem(masks, "", "", "", -Index - 1, FALSE);
            if (item != NULL && item->IsGood())
            {
                MainWindow->EnterViewerMasksCS();
                MainWindow->ViewerMasks->Insert(0, item);
                if (MainWindow->ViewerMasks->IsGood())
                    item = NULL;
                else
                    MainWindow->ViewerMasks->ResetState();
                MainWindow->LeaveViewerMasksCS();
            }
            if (item != NULL)
                delete item;
        }
    }
}

int StrICmpIgnoreSpacesOnStartAndEnd(const char* s1, const char* s2)
{
    while (*s1 != 0 && *s1 <= ' ')
        s1++;
    while (*s2 != 0 && *s2 <= ' ')
        s2++;
    while (*s1 != 0 && LowerCase[*s1] == LowerCase[*s2])
    {
        s1++;
        s2++;
    }
    while (*s1 != 0 && *s1 <= ' ')
        s1++;
    while (*s2 != 0 && *s2 <= ' ')
        s2++;
    if (*s1 == 0 && *s2 == 0)
        return 0;
    if ((unsigned)LowerCase[*s1] < (unsigned)LowerCase[*s2])
        return -1;
    else
        return 1;
}

void CSalamanderConnect::ForceRemoveViewer(const char* mask)
{
    CALL_STACK_MESSAGE2("CSalamanderConnect::ForceRemoveViewer(%s)", mask);
    char ext2[300]; // for breaking down found masks (';' replaced by '\0')
    int i;
    for (i = 0; i < MainWindow->ViewerMasks->Count; i++)
    {
        if (MainWindow->ViewerMasks->At(i)->ViewerType == -Index - 1) // correct plugin
        {
            const char* m = MainWindow->ViewerMasks->At(i)->Masks->GetMasksString();
            int len = (int)strlen(m);
            if (len > 299)
                len = 299;
            memcpy(ext2, m, len);
            ext2[len] = 0;
            char* s = ext2 + len;
            // finding and removing the 'mask' suffix, side-effect of removing ';' (replaced with 0)
            while (s > ext2)
            {
                while (--s >= ext2)
                {
                    if (*s == ';')
                    {
                        char* p = s;
                        while (--p >= ext2 && *p == ';')
                            ;
                        if (((s - p) & 1) == 1)
                            break;
                        s = p + 1;
                    }
                }
                if (s >= ext2)
                    *s = 0;
                if (StrICmpIgnoreSpacesOnStartAndEnd(s + 1, mask) == 0) // we are looking for this mask, we will delete
                {
                    int sLen = (int)strlen(s + 1);
                    memmove(s + 1, s + 1 + sLen + 1, len - ((s + 1) - ext2) - sLen);
                    if (len > sLen + 1)
                        len -= sLen + 1;
                    else
                        ext2[(len = 0)] = 0;
                }
            }
            // recovery ';'
            s = ext2;
            while (s - ext2 < len)
            {
                if (*s == 0)
                    *s = ';';
                s++;
            }
            if (ext2[0] != 0) // change mask
            {
                if (strcmp(ext2, m) != 0)
                    MainWindow->ViewerMasks->At(i)->Set(ext2, "", "", "");
            }
            else // deleting record (last mask deleted)
            {
                MainWindow->EnterViewerMasksCS();
                MainWindow->ViewerMasks->Delete(i);
                if (!MainWindow->ViewerMasks->IsGood())
                    MainWindow->ViewerMasks->ResetState();
                MainWindow->LeaveViewerMasksCS();
                i--;
            }
        }
    }
}

void CSalamanderConnect::AddPanelArchiver(const char* extensions, BOOL edit, BOOL updateExts)
{
    CALL_STACK_MESSAGE3("CSalamanderConnect::AddPanelArchiver(%s, %d)", extensions, edit);

    if (!PanelView && (!edit || !PanelEdit) && !updateExts)
        return; // There is nothing to do (neither upgrade the plug-in, nor update the extension)

    char ext[300]; // copy extensions (';' replaced by '\0')
    int len = (int)strlen(extensions);
    if (len > 299)
        len = 299;
    memcpy(ext, extensions, len);
    ext[len] = 0;
    TDirectArray<char*> extArray(10, 5); // array of extensions suffixes
    char* s = ext + len;
    while (s > ext)
    {
        while (--s >= ext && *s != ';')
            ;
        if (s >= ext)
            *s = 0;
        extArray.Insert(0, s + 1); // some of the extensions in the array, O(n^2) to preserve the order
        if (!extArray.IsGood())
            extArray.ResetState();
    }

    int index = -1; // index of the searched extension or record where at least one plug-in is
                    // as "view" when updating attachments (plugin extends/overrides existing record)
    char ext2[300]; // Copy extensions from PackerFormatConfig (';' replaced with '\0')
    int i;
    for (i = 0; i < PackerFormatConfig.GetFormatsCount(); i++)
    {
        BOOL found = FALSE; // TRUE if this plug-in provides at least "view" during the update of extensions
        if (updateExts)     // when it comes to updating the extension
        {
            if (PackerFormatConfig.GetUnpackerIndex(i) == -Index - 1) // and if the plug-in is set up at least for "view"
            {
                found = TRUE;
            }
            else
                continue; // It's about upgrading extensions, we're not looking for intersections...
        }

        len = (int)strlen(PackerFormatConfig.GetExt(i));
        if (len > 299)
            len = 299;
        memcpy(ext2, PackerFormatConfig.GetExt(i), len);
        ext2[len] = 0;
        s = ext2 + len;
        while (s > ext2)
        {
            while (--s >= ext2 && *s != ';')
                ;
            if (s >= ext2)
                *s = 0;
            int j;
            for (j = 0; j < extArray.Count; j++)
            {
                if (found || StrICmp(s + 1, extArray[j]) == 0) // upgrade or plural suffixes have a non-empty intersection
                {
                    index = i;

                    if (!found || StrICmp(s + 1, extArray[j]) == 0) // only if the suffixes match
                    {
                        extArray.Delete(j); // It is already in ext2, it doesn't have to be in ext
                        if (!extArray.IsGood())
                            extArray.ResetState();
                    }
                    // we will search the rest of ext2 and remove matching extensions from ext
                    BOOL firstRound = TRUE;
                    while (s > ext2)
                    {
                        if (!firstRound || !found)
                        {
                            while (--s >= ext2 && *s != ';')
                                ;
                            if (s >= ext2)
                                *s = 0;
                        }
                        for (j = 0; j < extArray.Count; j++)
                        {
                            if (StrICmp(s + 1, extArray[j]) == 0) // another same extension
                            {
                                extArray.Delete(j); // It is already in ext2, it doesn't have to be in ext
                                if (!extArray.IsGood())
                                    extArray.ResetState();
                                break;
                            }
                        }
                        firstRound = FALSE;
                    }
                    break;
                }
            }
        }
        if (index != -1)
            break; // index found
    }

    BOOL newItem = FALSE;
    if (index == -1) // in extensions are completely new extensions, later we may add a new record
    {
        newItem = TRUE;
        ext2[0] = 0;
    }
    else
        strcpy(ext2, PackerFormatConfig.GetExt(index));

    // unify old extensions and extensions in ext2 or only extensions (new extensions)
    len = (int)strlen(ext2);
    if (len > 0 && ext2[len - 1] == ';')
        len--;
    for (i = 0; i < extArray.Count; i++)
    {
        int len2 = (int)strlen(extArray[i]);
        if (len + len2 + 1 <= 300) // if there is enough space, we will add the extension to ext2
        {
            if (len != 0)
                ext2[len] = ';';
            else
                len--;
            strcpy(ext2 + len + 1, extArray[i]);
            len += len2 + 1;
        }
    }

    BOOL usePacker;
    int packerIndex;
    int unpackerIndex;
    if (!newItem)
    {
        if (!PackerFormatConfig.GetUsePacker(index))
            usePacker = FALSE;
        else
        {
            usePacker = TRUE;
            packerIndex = PackerFormatConfig.GetPackerIndex(index);
        }
        unpackerIndex = PackerFormatConfig.GetUnpackerIndex(index);
    }
    else
        usePacker = FALSE;

    BOOL change = FALSE; // Are there any changes in the view and/or edit settings?
    if (PanelView && edit && PanelEdit)
    {
        packerIndex = unpackerIndex = -Index - 1; // view & edit
        usePacker = TRUE;
        change = TRUE;
    }
    else
    {
        if (PanelView) // view
        {
            /*    // zakomentoval jsem, protoze jinak staci v ::Connect pluginu pri volani AddPanelArchiver
      // dat parametr 'edit'==TRUE a i plugin, ktery "panel edit" neposkytuje se zde oznaci
      // jako "panel edit" pro dane pripony, coz je prirozene chyba
      if (newItem && edit)
      {
        packerIndex = -Index - 1;
        usePacker = TRUE;
      }
*/
            unpackerIndex = -Index - 1;
            change = TRUE;
        }
        else
        {
            if (edit && PanelEdit) // edit
            {
                usePacker = TRUE;
                packerIndex = -Index - 1;
                change = TRUE;
            }
            //      if (newItem)     // the user probably does not want it, so we will not add it automatically
            //      {
            //        unpackerIndex = -Index - 1;
            //        change = TRUE;
            //      }
        }
    }
    if (updateExts && newItem) // All records of the user plug-in have been deleted, let's add at least the new ones
    {
        CPluginData* p = Plugins.Get(Index);
        if (p != NULL && p->SupportPanelView)
        {
            packerIndex = unpackerIndex = -Index - 1; // view and maybe even edit
            usePacker = p->SupportPanelEdit;
            change = TRUE;
        }
    }
    if (updateExts && !newItem &&          // Update the list of attachments (only if the record exists)
            -Index - 1 == unpackerIndex || // and this plug-in at least does "view" (otherwise it's not worth updating extensions)
        change)                            // only when changed (otherwise unwanted completion would occur)
    {                                      // supported archive file extensions)
        if (newItem)                       // it is necessary to add a new record
        {
            index = PackerFormatConfig.AddFormat();
            if (index == -1)
                return; // error
        }
        PackerFormatConfig.SetFormat(index, ext2, usePacker, usePacker ? packerIndex : -1, unpackerIndex, FALSE);
        PackerFormatConfig.BuildArray();
    }
}

void CSalamanderConnect::ForceRemovePanelArchiver(const char* extension)
{
    CALL_STACK_MESSAGE2("CSalamanderConnect::ForceRemovePanelArchiver(%s)", extension);
    BOOL needBuild = FALSE;

NEXT_ROUND:

    for (int i = 0; i < PackerFormatConfig.GetFormatsCount(); i++)
    {
        if (PackerFormatConfig.GetUnpackerIndex(i) == -Index - 1) // if the plug-in is set at least for "view"
        {
            char ext[300];
            lstrcpyn(ext, PackerFormatConfig.GetExt(i), _countof(ext));
            char* s = ext + strlen(ext);
            char* extEnd = NULL;
            while (s > ext)
            {
                while (--s >= ext && *s != ';')
                    ;
                if (extEnd != NULL)
                    *extEnd = 0;
                if (StrICmp(s + 1, extension) == 0) // desired extension found
                {
                    if (s < ext)
                    {
                        if (extEnd != NULL)
                            memmove(ext, extEnd + 1, strlen(extEnd + 1) + 1);
                        else
                            ext[0] = 0;
                    }
                    else
                    {
                        if (extEnd != NULL)
                            memmove(s + 1, extEnd + 1, strlen(extEnd + 1) + 1);
                        else
                            *s = 0;
                    }
                    if (ext[0] != 0)
                    {
                        PackerFormatConfig.SetFormat(i, ext,
                                                     PackerFormatConfig.GetUsePacker(i),
                                                     PackerFormatConfig.GetPackerIndex(i),
                                                     PackerFormatConfig.GetUnpackerIndex(i),
                                                     PackerFormatConfig.GetOldType(i));
                    }
                    else
                        PackerFormatConfig.DeleteFormat(i);
                    needBuild = TRUE;
                    goto NEXT_ROUND; // we will start the whole search again (small problem = no optimizations make sense)
                }
                if (extEnd != NULL)
                    *extEnd = ';';
                extEnd = s;
            }
        }
    }
    if (needBuild)
        PackerFormatConfig.BuildArray();
}

void CSalamanderConnect::AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                     DWORD state_or, DWORD state_and, DWORD skillLevel)
{
    CALL_STACK_MESSAGE9("CSalamanderConnect::AddMenuItem(%d, %s, %u, %d, 0x%X, 0x%X, 0x%X, 0x%X)",
                        iconIndex, name, hotKey, id, callGetState, state_or, state_and, skillLevel);
    if (iconIndex < -1)
        iconIndex = -1;
    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
    {
        if (!p->SupportDynMenuExt)
        {
            if (p->GetPluginInterfaceForMenuExt()->NotEmpty())
            {
                p->AddMenuItem(iconIndex, name, hotKey, id, callGetState, state_or, state_and,
                               skillLevel, pmitItemOrSeparator);
            }
            else
            {
                TRACE_E("Unable to add new menu item. The plug-in didn't provide interface for "
                        "menu extensions (see GetInterfaceForMenuExt).");
            }
        }
        else
            TRACE_E("CSalamanderConnect::AddMenuItem(): call ignored because you have dynamic menu (see FUNCTION_DYNAMICMENUEXT).");
    }
}

void CSalamanderConnect::AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                         DWORD state_or, DWORD state_and, DWORD skillLevel)
{
    CALL_STACK_MESSAGE8("CSalamanderConnect::AddSubmenuStart(%d, %s, %d, %d, 0x%X, 0x%X, 0x%X)",
                        iconIndex, name, id, callGetState, state_or, state_and, skillLevel);
    if (name == NULL)
    {
        TRACE_E("CSalamanderConnect::AddSubmenuStart(): 'name' may not be NULL!");
        return;
    }
    SubmenuLevel++;
    if (iconIndex < -1)
        iconIndex = -1;
    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
    {
        if (!p->SupportDynMenuExt)
        {
            if (p->GetPluginInterfaceForMenuExt()->NotEmpty())
            {
                p->AddMenuItem(iconIndex, name, 0, id, callGetState, state_or, state_and,
                               skillLevel, pmitStartSubmenu);
            }
            else
            {
                TRACE_E("Unable to add new menu item. The plugin didn't provide interface for "
                        "menu extensions (see GetInterfaceForMenuExt).");
            }
        }
        else
            TRACE_E("CSalamanderConnect::AddSubmenuStart(): call ignored because you have dynamic menu (see FUNCTION_DYNAMICMENUEXT).");
    }
}

void CSalamanderConnect::AddSubmenuEnd()
{
    CALL_STACK_MESSAGE1("CSalamanderConnect::AddSubmenuEnd()");
    if (SubmenuLevel > 0)
    {
        SubmenuLevel--;
        CPluginData* p = Plugins.Get(Index);
        if (p != NULL)
        {
            if (!p->SupportDynMenuExt)
            {
                if (p->GetPluginInterfaceForMenuExt()->NotEmpty())
                {
                    p->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL, pmitEndSubmenu);
                }
                else
                {
                    TRACE_E("Unable to add new menu item. The plugin didn't provide interface for "
                            "menu extensions (see GetInterfaceForMenuExt).");
                }
            }
            else
                TRACE_E("CSalamanderConnect::AddSubmenuEnd(): call ignored because you have dynamic menu (see FUNCTION_DYNAMICMENUEXT).");
        }
    }
    else
        TRACE_E("Incorrect call to CSalamanderConnect::AddSubmenuEnd(): no submenu is opened!");
}

void CSalamanderConnect::SetChangeDriveMenuItem(const char* title, int iconIndex)
{
    CALL_STACK_MESSAGE3("CSalamanderConnect::SetChangeDriveMenuItem(%s, %d)", title, iconIndex);
    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
    {
        if (p->SupportFS)
        {
            if (p->ChDrvMenuFSItemName != NULL)
                free(p->ChDrvMenuFSItemName);
            p->ChDrvMenuFSItemName = NULL;
            p->ChDrvMenuFSItemIconIndex = -1;

            if (title == NULL)
            {
                TRACE_E("CSalamanderConnect::SetChangeDriveMenuItem(): 'title' may not be NULL!");
                return;
            }

            p->ChDrvMenuFSItemName = DupStr(title);
            if (p->ChDrvMenuFSItemName != NULL)
                p->ChDrvMenuFSItemIconIndex = iconIndex;
        }
        else
        {
            TRACE_E("Unable to set Change Drive menu item. The plugin didn't provide interface for "
                    "file-system (see GetInterfaceForFS).");
        }
    }
}

void CSalamanderConnect::SetThumbnailLoader(const char* masks)
{
    if (masks == NULL || *masks == 0)
    {
        TRACE_E("CSalamanderConnect::SetThumbnailLoader(): unexpected parameter value (NULL or empty string).");
        return;
    }

    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
    {
        if (p->GetPluginInterfaceForThumbLoader()->NotEmpty())
        {
            p->ThumbnailMasks.SetMasksString(masks);
            int err;
            if (!p->ThumbnailMasks.PrepareMasks(err)) // error
            {
                TRACE_E("Unable to set thumbnail loader masks. Error in group mask (syntactical).");
                p->ThumbnailMasks.SetMasksString("");
            }
        }
        else
        {
            TRACE_E("Unable to set thumbnail loader masks. The plugin didn't provide interface for "
                    "thumbnail loader (see GetPluginInterfaceForThumbLoader).");
        }
    }
}

void CSalamanderConnect::SetBitmapWithIcons(HBITMAP bitmap)
{
    if (bitmap == NULL)
    {
        TRACE_E("CSalamanderConnect::SetBitmapWithIcons(): 'bitmap' may not be NULL!");
        return;
    }

    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
    {
        // we copy the bitmap 'bitmap' to our DIB, to which
        // we will have access to RAW data level
        BITMAP bmp;
        GetObject(bitmap, sizeof(bmp), &bmp);

        if (p->PluginIcons != NULL)
        {
            delete p->PluginIcons;
            p->PluginIcons = NULL;
        }
        if (p->PluginIconsGray != NULL)
        {
            delete p->PluginIconsGray;
            p->PluginIconsGray = NULL;
        }

        p->PluginIcons = new CIconList();
        if (p->PluginIcons != NULL)
        {
            if (p->PluginIcons->CreateFromBitmap(bitmap, bmp.bmWidth / 16, RGB(255, 0, 255)))
            {
                p->PluginIconsGray = new CIconList();
                if (p->PluginIconsGray != NULL && !p->PluginIconsGray->CreateAsCopy(p->PluginIcons, TRUE))
                {
                    delete p->PluginIconsGray;
                    p->PluginIconsGray = NULL;
                }
            }
            else
            {
                delete p->PluginIcons;
                p->PluginIcons = NULL;
            }
        }
    }
}

void CSalamanderConnect::SetPluginIcon(int iconIndex)
{
    if (iconIndex < 0)
    {
        TRACE_E("CSalamanderConnect::SetPluginIcon(): 'iconIndex' may not be negative number!");
        return;
    }

    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
        p->PluginIconIndex = iconIndex;
}

void CSalamanderConnect::SetPluginMenuAndToolbarIcon(int iconIndex)
{
    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
    {
        p->PluginSubmenuIconIndex = iconIndex;
    }
}

void CSalamanderConnect::SetIconListForGUI(CGUIIconListAbstract* iconList)
{
    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
    {
        if (p->PluginIcons != NULL)
        {
            delete p->PluginIcons;
            p->PluginIcons = NULL;
        }
        if (p->PluginIconsGray != NULL)
        {
            delete p->PluginIconsGray;
            p->PluginIconsGray = NULL;
        }
        p->PluginIcons = (CIconList*)iconList;
        if (p->PluginIcons != NULL)
        {
            p->PluginIconsGray = new CIconList();
            if (p->PluginIconsGray != NULL && !p->PluginIconsGray->CreateAsCopy(p->PluginIcons, TRUE))
            {
                delete p->PluginIconsGray;
                p->PluginIconsGray = NULL;
            }
        }
    }
}

//
// ****************************************************************************
// CSalamanderBuildMenu
//

void CSalamanderBuildMenu::AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                       DWORD state_or, DWORD state_and, DWORD skillLevel)
{
    CALL_STACK_MESSAGE9("CSalamanderBuildMenu::AddMenuItem(%d, %s, %u, %d, 0x%X, 0x%X, 0x%X, 0x%X)",
                        iconIndex, name, hotKey, id, callGetState, state_or, state_and, skillLevel);
    if (iconIndex < -1)
        iconIndex = -1;
    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
    {
        if (p->GetPluginInterfaceForMenuExt()->NotEmpty())
        {
            p->AddMenuItem(iconIndex, name, hotKey, id, callGetState, state_or, state_and,
                           skillLevel, pmitItemOrSeparator);
        }
        else
        {
            TRACE_E("Unable to add new menu item. The plug-in didn't provide interface for "
                    "menu extensions (see GetInterfaceForMenuExt).");
        }
    }
}

void CSalamanderBuildMenu::AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                           DWORD state_or, DWORD state_and, DWORD skillLevel)
{
    CALL_STACK_MESSAGE8("CSalamanderBuildMenu::AddSubmenuStart(%d, %s, %d, %d, 0x%X, 0x%X, 0x%X)",
                        iconIndex, name, id, callGetState, state_or, state_and, skillLevel);
    SubmenuLevel++;
    CPluginData* p = Plugins.Get(Index);
    if (name == NULL)
    {
        TRACE_E("CSalamanderBuildMenu::AddSubmenuStart(): 'name' may not be NULL!");
        return;
    }
    if (iconIndex < -1)
        iconIndex = -1;
    if (p != NULL)
    {
        if (p->GetPluginInterfaceForMenuExt()->NotEmpty())
        {
            p->AddMenuItem(iconIndex, name, 0, id, callGetState, state_or, state_and,
                           skillLevel, pmitStartSubmenu);
        }
        else
        {
            TRACE_E("Unable to add new menu item. The plugin didn't provide interface for "
                    "menu extensions (see GetInterfaceForMenuExt).");
        }
    }
}

void CSalamanderBuildMenu::AddSubmenuEnd()
{
    CALL_STACK_MESSAGE1("CSalamanderBuildMenu::AddSubmenuEnd()");
    if (SubmenuLevel > 0)
    {
        SubmenuLevel--;
        CPluginData* p = Plugins.Get(Index);
        if (p != NULL)
        {
            if (p->GetPluginInterfaceForMenuExt()->NotEmpty())
            {
                p->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL, pmitEndSubmenu);
            }
            else
            {
                TRACE_E("Unable to add new menu item. The plugin didn't provide interface for "
                        "menu extensions (see GetInterfaceForMenuExt).");
            }
        }
    }
    else
        TRACE_E("Incorrect call to CSalamanderBuildMenu::AddSubmenuEnd(): no submenu is opened!");
}

void CSalamanderBuildMenu::SetIconListForMenu(CGUIIconListAbstract* iconList)
{
    CPluginData* p = Plugins.Get(Index);
    if (p != NULL)
    {
        p->ReleasePluginDynMenuIcons();
        p->PluginDynMenuIcons = (CIconList*)iconList;
    }
}

//
// ****************************************************************************
// CSalamanderRegistry
//

BOOL CSalamanderRegistry::ClearKey(HKEY key)
{
    CALL_STACK_MESSAGE1("CSalamanderRegistry::ClearKey()");
    return ::ClearKey(key);
}

BOOL CSalamanderRegistry::CreateKey(HKEY key, const char* name, HKEY& createdKey)
{
    CALL_STACK_MESSAGE1("CSalamanderRegistry::CreateKey()");
    return ::CreateKey(key, name, createdKey);
}

BOOL CSalamanderRegistry::OpenKey(HKEY key, const char* name, HKEY& openedKey)
{
    CALL_STACK_MESSAGE1("CSalamanderRegistry::OpenKey()");
    return ::OpenKey(key, name, openedKey);
}

void CSalamanderRegistry::CloseKey(HKEY key)
{
    CALL_STACK_MESSAGE1("CSalamanderRegistry::CloseKey()");
    ::CloseKey(key);
}

BOOL CSalamanderRegistry::DeleteKey(HKEY key, const char* name)
{
    CALL_STACK_MESSAGE1("CSalamanderRegistry::DeleteKey()");
    return ::DeleteKey(key, name);
}

BOOL CSalamanderRegistry::GetValue(HKEY key, const char* name, DWORD type, void* buffer, DWORD bufferSize)
{
    SLOW_CALL_STACK_MESSAGE1("CSalamanderRegistry::GetValue()");
    return ::GetValue(key, name, type, buffer, bufferSize);
}

BOOL CSalamanderRegistry::SetValue(HKEY key, const char* name, DWORD type, const void* data, DWORD dataSize)
{
    SLOW_CALL_STACK_MESSAGE1("CSalamanderRegistry::SetValue()");
    return ::SetValue(key, name, type, data, dataSize);
}

BOOL CSalamanderRegistry::DeleteValue(HKEY key, const char* name)
{
    CALL_STACK_MESSAGE1("CSalamanderRegistry::DeleteValue()");
    return ::DeleteValue(key, name);
}

BOOL CSalamanderRegistry::GetSize(HKEY key, const char* name, DWORD type, DWORD& bufferSize)
{
    SLOW_CALL_STACK_MESSAGE3("CSalamanderRegistry::GetSize(, %s, 0x%x, )", name, type);
    return ::GetSize(key, name, type, bufferSize);
}

//
// ****************************************************************************
// CSalamanderPluginEntry
//

BOOL CSalamanderPluginEntry::SetBasicPluginData(const char* pluginName, DWORD functions,
                                                const char* version, const char* copyright,
                                                const char* description, const char* regKeyName,
                                                const char* extensions, const char* fsName)
{
    CALL_STACK_MESSAGE9("CSalamanderPluginEntry::SetBasicPluginData(%s, 0x%X, %s, %s, %s, %s, %s, %s)",
                        pluginName, functions, version, copyright, description, regKeyName, extensions,
                        fsName);

    if (Valid)
    {
        TRACE_E("CSalamanderPluginEntry::SetBasicPluginData(): this method can be called only once!");
        return FALSE;
    }

    BOOL supportPanelView = (functions & FUNCTION_PANELARCHIVERVIEW) != 0;
    BOOL supportPanelEdit = (functions & FUNCTION_PANELARCHIVEREDIT) != 0;
    BOOL supportCustomPack = (functions & FUNCTION_CUSTOMARCHIVERPACK) != 0;
    BOOL supportCustomUnpack = (functions & FUNCTION_CUSTOMARCHIVERUNPACK) != 0;
    BOOL supportConfiguration = (functions & FUNCTION_CONFIGURATION) != 0;
    BOOL supportLoadSave = (functions & FUNCTION_LOADSAVECONFIGURATION) != 0;
    BOOL supportViewer = (functions & FUNCTION_VIEWER) != 0;
    BOOL supportFS = (functions & FUNCTION_FILESYSTEM) != 0;
    BOOL supportDynMenuExt = (functions & FUNCTION_DYNAMICMENUEXT) != 0;

    if (pluginName == NULL || version == NULL || copyright == NULL || description == NULL ||
        supportLoadSave && (regKeyName == NULL || regKeyName[0] == 0) ||
        (supportPanelView || supportPanelEdit) && extensions == NULL ||
        supportFS && fsName == NULL)
    {
        TRACE_E("CSalamanderPluginEntry::SetBasicPluginData(): Invalid parameter (NULL or empty string)!");
        Error = TRUE;
        return FALSE;
    }

    // it is about loading an installed plug-in or adding a new plug-in,
    // we will perform data check and update (new plug-in should always pass tests)
    if (Plugin->SupportPanelView && !supportPanelView ||
        Plugin->SupportPanelEdit && !supportPanelEdit ||
        Plugin->SupportCustomPack && !supportCustomPack ||
        Plugin->SupportCustomUnpack && !supportCustomUnpack ||
        Plugin->SupportViewer && !supportViewer ||
        Plugin->SupportFS && !supportFS)
    { // downgrade options are not possible ...
        char bufText[MAX_PATH + 200];
        sprintf(bufText, LoadStr(IDS_REINSTALLPLUGIN), Plugin->Name, Plugin->DLLName);
        SalMessageBox(Parent, bufText, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONERROR);
        Error = TRUE;
        return FALSE;
    }

    // update data
    Plugin->SupportPanelView = supportPanelView;
    Plugin->SupportPanelEdit = supportPanelEdit;
    Plugin->SupportCustomPack = supportCustomPack;
    Plugin->SupportCustomUnpack = supportCustomUnpack;
    Plugin->SupportConfiguration = supportConfiguration;
    Plugin->SupportLoadSave = supportLoadSave;
    Plugin->SupportViewer = supportViewer;
    Plugin->SupportFS = supportFS;
    Plugin->SupportDynMenuExt = supportDynMenuExt;

    char* s = DupStr(pluginName);
    if (s != NULL)
    {
        free(Plugin->Name);
        Plugin->Name = s;
    }
    s = DupStr(version);
    if (s != NULL)
    {
        free(Plugin->Version);
        Plugin->Version = s;
    }
    Plugin->SalamanderDebug.Init(Plugin->DLLName, Plugin->Version);
    Plugin->SalamanderPasswordManager.Init(Plugin->DLLName);
    s = DupStr(copyright);
    if (s != NULL)
    {
        free(Plugin->Copyright);
        Plugin->Copyright = s;
    }
    s = DupStr((supportPanelView || supportPanelEdit) ? extensions : "");
    if (s != NULL)
    {
        free(Plugin->Extensions);
        Plugin->Extensions = s;
    }
    s = DupStr(description);
    if (s != NULL)
    {
        free(Plugin->Description);
        Plugin->Description = s;
    }
    if (supportLoadSave)
    {
        if (Plugin->RegKeyName[0] == 0)
        { // new plug-in with load/save - set a new key name in the registry
            char uniqueKeyName[MAX_PATH];
            Plugins.GetUniqueRegKeyName(uniqueKeyName, regKeyName);
            s = DupStr(uniqueKeyName);
            if (s != NULL)
            {
                free(Plugin->RegKeyName);
                Plugin->RegKeyName = s;
            }
        }
    }
    else // does not support load/save
    {
        s = DupStr("");
        if (s != NULL)
        {
            free(Plugin->RegKeyName);
            Plugin->RegKeyName = s;
        }
    }
    if (Plugin->SupportFS)
    {
        OldFSNames.DestroyMembers();
        OldFSNames.Add(Plugin->FSNames.GetData(), Plugin->FSNames.Count);
        if (OldFSNames.IsGood())
        {
            Plugin->FSNames.DetachMembers(); // strings are now in OldFSNames
            if (!Plugin->FSNames.IsGood())
                Plugin->FSNames.ResetState(); // to delete, we are not interested anymore

            char uniqueFSName[MAX_PATH];
            Plugins.GetUniqueFSName(uniqueFSName, fsName, NULL, &OldFSNames);
            s = DupStr(uniqueFSName);
            if (s != NULL)
            {
                Plugin->FSNames.Add(s);
                if (!Plugin->FSNames.IsGood()) // cannot happen (adding the first element of the array), just dry...
                {
                    Plugin->FSNames.ResetState();
                    free(s);
                }
            }
            else // Cannot use a new unique name, let's at least keep the old one
            {
                if (OldFSNames.Count > 0)
                {
                    Plugin->FSNames.Add(OldFSNames[0]);
                    if (!Plugin->FSNames.IsGood())
                        Plugin->FSNames.ResetState(); // it cannot happen, only dry...
                    OldFSNames.Detach(0);
                    if (!OldFSNames.IsGood())
                        OldFSNames.ResetState(); // disconnection occurred, we are not interested anymore
                }
            }
        }
        else
        {
            OldFSNames.ResetState();
            int i;
            for (i = Plugin->FSNames.Count - 1; i > 0; i--)
            {
                Plugin->FSNames.Delete(i); // we discard all fs-names except the first one
                if (!Plugin->FSNames.IsGood())
                    Plugin->FSNames.ResetState(); // to delete, we are not interested anymore
            }
        }
    }
    else // does not support FS
    {
        Plugin->FSNames.DestroyMembers();
    }

    Valid = TRUE;
    return TRUE; // Successful data retrieval
}

HINSTANCE
CSalamanderPluginEntry::LoadLanguageModule(HWND parent, const char* pluginName)
{
    HINSTANCE lang = NULL;
    char path[MAX_PATH];
    char errorText[MAX_PATH + 300];

    // get the path to the LANG directory of the plugin
    char* s = Plugin->DLLName;
    if ((*s != '\\' || *(s + 1) != '\\') && // not UNC
        (*s == 0 || *(s + 1) != ':'))       // not "c:" -> relative path to the subdirectory plugins
    {
        GetModuleFileName(HInstance, path, MAX_PATH);
        s = strrchr(path, '\\') + 1;
        strcpy(s, "plugins\\");
        strcat(s, Plugin->DLLName);
    }
    else
        lstrcpyn(path, s, MAX_PATH);
    s = strrchr(path, '\\') + 1;
    lstrcpyn(s, "lang\\", MAX_PATH - (int)(s - path));
    char* slgName = path + strlen(path);
    int slgNameBufSize = MAX_PATH - (int)(slgName - path);

    // First, we will try to load an SLG of the same language in which Salamander is currently running
    lstrcpyn(slgName, Configuration.LoadedSLGName, slgNameBufSize);
    lang = HANDLES_Q(LoadLibrary(path));
    WORD languageID = 0;
    if (lang == NULL || !IsSLGFileValid(Plugin->GetPluginDLL(), lang, languageID, NULL))
    { // SLG does not exist or it is not the expected SLG (completely different file or at least a different version)
        if (lang != NULL)
            HANDLES(FreeLibrary(lang));
        lang = NULL;
        if (Plugin->LastSLGName == NULL ||               // we have no .slg selected during the last plugin load
            _stricmp(slgName, Plugin->LastSLGName) == 0) // we have already tried this .slg
        {
            if (!Configuration.DoNotDispCantLoadPluginSLG)
            {
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = parent;
                params.Flags = MB_OK | MB_ICONERROR;
                params.Caption = pluginName;
                _snprintf_s(errorText, _TRUNCATE, LoadStr(IDS_CANTLOADPLUGINSLG1), path);
                params.Text = errorText;
                params.CheckBoxText = LoadStr(IDS_DONOTSHOWCANTLOADPLUGINSLG);
                params.CheckBoxValue = &Configuration.DoNotDispCantLoadPluginSLG;
                SalMessageBoxEx(&params);
            }
        }
        else // try to load the .slg selected during the last plugin load
        {
            lstrcpyn(slgName, Plugin->LastSLGName, slgNameBufSize);
            lang = HANDLES_Q(LoadLibrary(path));
            if (lang == NULL || !IsSLGFileValid(Plugin->GetPluginDLL(), lang, languageID, NULL))
            { // SLG does not exist or it is not the expected SLG (completely different file or at least a different version)
                if (lang != NULL)
                    HANDLES(FreeLibrary(lang));
                lang = NULL;
                if (!Configuration.DoNotDispCantLoadPluginSLG2)
                {
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = parent;
                    params.Flags = MB_OK | MB_ICONERROR;
                    params.Caption = pluginName;
                    _snprintf_s(errorText, _TRUNCATE, LoadStr(IDS_CANTLOADPLUGINSLG2), path);
                    params.Text = errorText;
                    params.CheckBoxText = LoadStr(IDS_DONOTSHOWCANTLOADPLUGINSLG);
                    params.CheckBoxValue = &Configuration.DoNotDispCantLoadPluginSLG2;
                    SalMessageBoxEx(&params);
                }
            }
        }
        if (lang == NULL) // find all .slg files on the disk in the plugin folder + optionally let the user choose (if there is more than one .slg)
        {
            char selSLGName[MAX_PATH];
            selSLGName[0] = 0;
            CLanguageSelectorDialog slgDialog(parent, selSLGName, pluginName);
            lstrcpyn(slgName, "*.slg", slgNameBufSize);
            slgDialog.Initialize(path, Plugin->GetPluginDLL());
            if (slgDialog.GetLanguagesCount() == 0)
                SalMessageBox(parent, LoadStr(IDS_PLUGINSLGNOTFOUND), pluginName, MB_OK | MB_ICONERROR);
            else
            {
                if (slgDialog.GetLanguagesCount() == 1)
                    slgDialog.GetSLGName(selSLGName); // if there is only one language, we will use it
                else
                {
                    if (Configuration.UseAsAltSLGInOtherPlugins &&
                        slgDialog.SLGNameExists(Configuration.AltPluginSLGName))
                    {
                        lstrcpy(selSLGName, Configuration.AltPluginSLGName);
                    }
                    else
                    {
                        if (Configuration.UseAsAltSLGInOtherPlugins) // Although the replacement language is defined, it is not available in this plugin, so we will let the user choose another replacement language
                            Configuration.UseAsAltSLGInOtherPlugins = FALSE;
                        slgDialog.Execute();
                    }
                }
                lstrcpyn(slgName, selSLGName, slgNameBufSize);
                lang = HANDLES_Q(LoadLibrary(path));
                if (lang == NULL || !IsSLGFileValid(Plugin->GetPluginDLL(), lang, languageID, NULL))
                { // theoretically should not occur (dialog verifies the validity of the .SLG module)
                    if (lang != NULL)
                        HANDLES(FreeLibrary(lang));
                    lang = NULL;
                    TRACE_E("CSalamanderPluginEntry::LoadLanguageModule(): unexpected situation: SLG module is invalid!");
                }
            }
        }
    }
    if (Plugin->LastSLGName != NULL)
    {
        free(Plugin->LastSLGName);
        Plugin->LastSLGName = NULL;
    }
    if (lang != NULL)
    {
        if (_stricmp(slgName, Configuration.LoadedSLGName) != 0)
            Plugin->LastSLGName = DupStr(slgName);
        if (Plugin->SalamanderGeneral.LanguageModule == NULL)
            Plugin->SalamanderGeneral.LanguageModule = lang;
        else
        {
            HANDLES(FreeLibrary(lang));
            lang = NULL;
            TRACE_E("CSalamanderPluginEntry::LoadLanguageModule(): you can call this method only once!");
        }
    }
    return lang;
}

void CSalamanderPluginEntry::SetPluginHomePageURL(const char* url)
{
    CALL_STACK_MESSAGE2("CSalamanderPluginEntry::SetPluginHomePageURL(%s)", url);
    if (Plugin->PluginHomePageURL != NULL)
        free(Plugin->PluginHomePageURL);
    Plugin->PluginHomePageURL = DupStr(url);
}

BOOL CSalamanderPluginEntry::AddFSName(const char* fsName, int* newFSNameIndex)
{
    CALL_STACK_MESSAGE2("CSalamanderPluginEntry::AddFSName(%s,)", fsName);
    if (fsName == NULL || newFSNameIndex == NULL)
    {
        TRACE_E("CSalamanderPluginEntry::AddFSName(): invalid parameter (NULL)!");
        return FALSE;
    }

    if (!Valid)
    {
        TRACE_E("CSalamanderPluginEntry::AddFSName(): called before SetBasicPluginData()!");
        return FALSE;
    }

    if (!Plugin->SupportFS)
    {
        TRACE_E("CSalamanderPluginEntry::AddFSName(): unexpected call: FUNCTION_FILESYSTEM is not supported by this plugin!");
        return FALSE;
    }

    if (Plugin->FSNames.Count == 0)
    {
        TRACE_E("CSalamanderPluginEntry::AddFSName(): unable to add fs-name, first fs-name is missing!");
        return FALSE;
    }

    char uniqueFSName[MAX_PATH];
    Plugins.GetUniqueFSName(uniqueFSName, fsName, NULL, &OldFSNames);
    char* s = DupStr(uniqueFSName);
    if (s != NULL)
    {
        int i = Plugin->FSNames.Add(s);
        if (!Plugin->FSNames.IsGood())
        {
            Plugin->FSNames.ResetState();
            free(s);
        }
        else
        {
            *newFSNameIndex = i;
            return TRUE;
        }
    }
    return FALSE;
}

//
// ****************************************************************************
// CPluginMenuItem
//

CPluginMenuItem::CPluginMenuItem(int iconIndex, const char* name, DWORD hotKey, DWORD stateMask,
                                 int id, DWORD skillLevel, CPluginMenuItemType type)
{
    Type = type;
    IconIndex = iconIndex;
    if (name != NULL)
        Name = DupStr(name); // in case of an error, we will become a separator ;-)
    else
        Name = NULL;
#ifdef _DEBUG
    if (name != NULL && (hotKey & HOTKEY_HINT) == 0)
    {
        // Support for hot keys introduced since version 2.5 beta 7
        // The hotkey must not be part of the text
        char* p = Name;
        while (*p != 0)
        {
            if (*p == '\t')
            {
                if (Configuration.ConfigVersion >= 25) // we will tear apart on newer configurations
                    TRACE_E("Plugin menu item contains hot key (" << name << "). Use the AddMenuItem/'hotKey' parameter instead.");
                *p = 0;
                break;
            }
            p++;
        }
    }
#endif // _DEBUG
    StateMask = stateMask;
    if ((skillLevel & ~MENU_SKILLLEVEL_ALL) != 0 ||
        (skillLevel & MENU_SKILLLEVEL_ALL) == 0)
    {
        TRACE_E("CPluginMenuItem::CPluginMenuItem wrong skillLevel=" << skillLevel);
        // let's make a correction
        skillLevel = MENU_SKILLLEVEL_ALL;
    }
    SkillLevel = skillLevel;
    ID = id;
    SUID = -1;
    HotKey = hotKey; // no hotkey, user can set it
}

//
// ****************************************************************************
// CPluginData
//

CPluginData::CPluginData(const char* name, const char* dllName, BOOL supportPanelView,
                         BOOL supportPanelEdit, BOOL supportCustomPack, BOOL supportCustomUnpack,
                         BOOL supportConfiguration, BOOL supportLoadSave, BOOL supportViewer,
                         BOOL supportFS, BOOL supportDynMenuExt, const char* version, const char* copyright,
                         const char* description, const char* regKeyName, const char* extensions,
                         TIndirectArray<char>* fsNames, BOOL loadOnStart, char* lastSLGName,
                         const char* pluginHomePageURL)
    : MenuItems(10, 5), Commands(1, 5), FSNames(1, 10), PluginIfaceForFS(NULL, 0),
      PluginIfaceForMenuExt(NULL, 0)
{
    CALL_STACK_MESSAGE20("CPluginData::CPluginData(%s, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s, %s, %s, %s, %s, , %d, %s, %s)",
                         name, dllName, supportPanelView, supportPanelEdit, supportCustomPack,
                         supportCustomUnpack, supportConfiguration, supportLoadSave, supportViewer,
                         supportFS, supportDynMenuExt, version, copyright, description, regKeyName,
                         extensions, loadOnStart, lastSLGName, pluginHomePageURL);
    ArcCacheHaveInfo = FALSE;
    ArcCacheTmpPath = NULL;
    ArcCacheOwnDelete = FALSE;
    ArcCacheCacheCopies = TRUE;
    PluginIcons = NULL;
    PluginIconsGray = NULL;
    PluginDynMenuIcons = NULL;
    PluginIconIndex = -1;
    PluginSubmenuIconIndex = -1;
    ShowSubmenuInPluginsBar = TRUE;
    ThumbnailMasksDisabled = FALSE;
    DynMenuWasAlreadyBuild = FALSE;
    BugReportMessage = NULL;
    BugReportEMail = NULL;
    SubMenu = NULL;
    DLL = NULL;
    BuiltForVersion = 0;
    Name = DupStr(name);
    DLLName = DupStr(dllName);
    Version = DupStr(version);
    Copyright = DupStr(copyright);
    Extensions = DupStr(extensions);
    Description = DupStr(description);
    RegKeyName = DupStr(regKeyName);
    SupportFS = supportFS;
    BOOL lowMem = FALSE;
    if (SupportFS && fsNames != NULL)
    {
        int i;
        for (i = 0; i < fsNames->Count; i++)
        {
            char* s = DupStr(fsNames->At(i));
            if (s != NULL)
            {
                FSNames.Add(s);
                if (!FSNames.IsGood())
                {
                    free(s);
                    break;
                }
            }
            else
            {
                lowMem = TRUE;
                break;
            }
        }
    }
    LastSLGName = (lastSLGName == NULL || lastSLGName[0] == 0) ? NULL : DupStr(lastSLGName);
    PluginHomePageURL = pluginHomePageURL != NULL ? DupStr(pluginHomePageURL) : NULL;
    if (Name == NULL || DLLName == NULL || Version == NULL || Copyright == NULL ||
        Description == NULL || RegKeyName == NULL || Extensions == NULL ||
        !FSNames.IsGood() || lowMem ||
        lastSLGName != NULL && lastSLGName[0] != 0 && LastSLGName == NULL)
    {
        if (Name != NULL)
            free(Name);
        if (DLLName != NULL)
            free(DLLName);
        if (Version != NULL)
            free(Version);
        if (Copyright != NULL)
            free(Copyright);
        if (Extensions != NULL)
            free(Extensions);
        if (Description != NULL)
            free(Description);
        if (RegKeyName != NULL)
            free(RegKeyName);
        if (!FSNames.IsGood())
            FSNames.ResetState();
        FSNames.DestroyMembers();
        if (!FSNames.IsGood())
            FSNames.ResetState();
        if (LastSLGName != NULL)
            free(LastSLGName);
        if (PluginHomePageURL != NULL)
            free(PluginHomePageURL);
        Extensions = Name = DLLName = Version = Copyright = Description = RegKeyName =
            LastSLGName = PluginHomePageURL = NULL;
    }
    SalamanderDebug.Init(DLLName, Version);
    SalamanderPasswordManager.Init(DLLName);
    SupportPanelView = supportPanelView;
    SupportPanelEdit = supportPanelEdit;
    SupportCustomPack = supportCustomPack;
    SupportCustomUnpack = supportCustomUnpack;
    SupportConfiguration = supportConfiguration;
    SupportLoadSave = supportLoadSave;
    SupportViewer = supportViewer;
    SupportDynMenuExt = supportDynMenuExt;
    LoadOnStart = loadOnStart;
    ChDrvMenuFSItemName = NULL;
    ChDrvMenuFSItemVisible = TRUE;
    ChDrvMenuFSItemIconIndex = -1;
    ShouldUnload = FALSE;
    ShouldRebuildMenu = FALSE;
#ifdef _DEBUG
    OpenedFSCounter = 0;
    OpenedPDCounter = 0;
#endif // _DEBUG
    OpenPackDlg = FALSE;
    PackDlgDelFilesAfterPacking = 0;
    OpenUnpackDlg = FALSE;
    UnpackDlgUnpackMask = NULL;
    PluginIsNethood = FALSE;
    PluginUsesPasswordManager = FALSE;
    IconOverlaysCount = 0;
    IconOverlays = NULL;
}

CPluginData::~CPluginData()
{
    CALL_STACK_MESSAGE1("CPluginData::~CPluginData()");
#ifdef _DEBUG
    if (OpenedFSCounter != 0)
    {
        TRACE_E("OpenedFSCounter is " << OpenedFSCounter << " - mismatch in calls to CloseFS().");
    }
    if (OpenedPDCounter != 0)
    {
        TRACE_E("OpenedPDCounter is " << OpenedPDCounter << " - mismatch in calls to ReleasePluginDataInterface().");
    }
#endif // _DEBUG
    if (PluginIface.NotEmpty())
        PluginIface.Release(NULL, TRUE);
    if (DLL != NULL)
    {
        TRACE_E("CPluginData::~CPluginData(): unexpected situation (2)!");
        HANDLES(FreeLibrary(DLL));
    }
    if (Name != NULL)
        free(Name);
    if (DLLName != NULL)
        free(DLLName);
    if (Version != NULL)
        free(Version);
    if (Copyright != NULL)
        free(Copyright);
    if (Extensions != NULL)
        free(Extensions);
    if (Description != NULL)
        free(Description);
    if (RegKeyName != NULL)
        free(RegKeyName);
    FSNames.DestroyMembers();
    if (ChDrvMenuFSItemName != NULL)
        free(ChDrvMenuFSItemName);
    if (BugReportMessage != NULL)
        free(BugReportMessage);
    if (BugReportEMail != NULL)
        free(BugReportEMail);
    if (LastSLGName != NULL)
        free(LastSLGName);
    if (PluginHomePageURL != NULL)
        free(PluginHomePageURL);
    if (ArcCacheTmpPath != NULL)
        free(ArcCacheTmpPath);
    if (UnpackDlgUnpackMask != NULL)
        free(UnpackDlgUnpackMask);
    if (PluginIcons != NULL)
        delete PluginIcons;
    if (PluginIconsGray != NULL)
        delete PluginIconsGray;
    if (PluginDynMenuIcons != NULL)
    {
        TRACE_E("CPluginData::~CPluginData(): PluginDynMenuIcons is not NULL, please contact Petr Solin");
        delete PluginDynMenuIcons;
    }
    if (IconOverlaysCount != 0 || IconOverlays != NULL)
        TRACE_E("CPluginData::~CPluginData(): IconOverlaysCount is not 0 or IconOverlays is not NULL, please contact Petr Solin");
}

BOOL CPluginData::InitDLL(HWND parent, BOOL quiet, BOOL waitCursor, BOOL showUnsupOnX64, BOOL releaseDynMenuIcons)
{
    CALL_STACK_MESSAGE8("CPluginData::InitDLL(0x%p, %d, %d, %d, %d) (%s v. %s)",
                        parent, quiet, waitCursor, showUnsupOnX64,
                        releaseDynMenuIcons, DLLName, Version);

    char bufText[MAX_PATH + 200];

#ifdef _WIN64 // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
    const char* pluginNameEN;
    if (IsPluginUnsupportedOnX64(DLLName, &pluginNameEN))
    {
        if (showUnsupOnX64) // Let's write to the user that this plugin is available only in the 32-bit version (x86)
        {
            sprintf(bufText, LoadStr(IDS_PLUGINISX86ONLY), Name == NULL || Name[0] == 0 ? pluginNameEN : Name);
            SalMessageBox(parent, bufText, LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
        }
        return FALSE;
    }
#endif // _WIN64

    if (DLL == NULL && IsGood())
    {
        BOOL refreshUNCRootPaths = FALSE;

        // get the full name of the DLL
        char buf[MAX_PATH];
        char* s = DLLName;
        if ((*s != '\\' || *(s + 1) != '\\') && // not UNC
            (*s == 0 || *(s + 1) != ':'))       // not "c:" -> relative path to the subdirectory plugins
        {
            GetModuleFileName(HInstance, buf, MAX_PATH);
            s = strrchr(buf, '\\') + 1;
            strcpy(s, "plugins\\");
            strcat(s, DLLName);
            s = buf;
        }

        // Load the DLL
        HCURSOR oldCur;
        if (waitCursor)
            oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
        DLL = HANDLES(LoadLibrary(s));
        if (waitCursor)
            SetCursor(oldCur);
        if (DLL == NULL) // error
        {
            DWORD err = GetLastError();
            sprintf(bufText, LoadStr(IDS_UNABLETOLOADPLUGIN), Name, s, GetErrorText(err));
            if (!quiet)
                SalMessageBox(parent, bufText, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONERROR);
        }
        else // connect to the DLL
        {
            FSalamanderPluginEntry entry = (FSalamanderPluginEntry)GetProcAddress(DLL, "SalamanderPluginEntry"); // entry point of the plugin
            if (entry != NULL)
            {
#ifdef _DEBUG
                AddModuleWithPossibleMemoryLeaks(s);
#endif // _DEBUG

                // variables for determining functions that are "upgraded" (FALSE->TRUE)
                BOOL supportPanelView = SupportPanelView;
                BOOL supportPanelEdit = SupportPanelEdit;
                BOOL supportCustomPack = SupportCustomPack;
                BOOL supportCustomUnpack = SupportCustomUnpack;
                BOOL supportViewer = SupportViewer;

                CSalamanderPluginEntry salamander(parent, (CPluginData*)this);
                // we will construct the flag returned via CSalamanderPluginEntry::GetLoadInformation()
                salamander.AddLoadInfo(Plugins.LoadInfoBase); // base (nothing/auto-install/new-plugins.ver)
                                                              //        if (LoadOnStart) salamander.AddLoadInfo(LOADINFO_LOADONSTART);  // "load on start" flag

                // we will remove commands inherited from the last load (the array will be empty in 99.9% of cases)
                Commands.DestroyMembers();

                // we will remove icon-overlays (only for sychr, it should be empty)
                ReleaseIconOverlays();

                // we will disable the disk cache setting for the archiver, we need to retrieve it again
                ArcCacheHaveInfo = FALSE;
                if (ArcCacheTmpPath != NULL)
                    free(ArcCacheTmpPath);
                ArcCacheTmpPath = NULL;
                ArcCacheOwnDelete = FALSE;
                ArcCacheCacheCopies = TRUE;

                FSalamanderPluginGetReqVer getReqVer = (FSalamanderPluginGetReqVer)GetProcAddress(DLL, "SalamanderPluginGetReqVer"); // Plugin function
                BuiltForVersion = -1;                                                                                                // -1 = plugin is made for a version older than 2.5 beta 2 (it does not have the "SalamanderPluginGetReqVer" export)
                if (getReqVer != NULL && (BuiltForVersion = getReqVer()) >= PLUGIN_REQVER)
                {
                    FSalamanderPluginGetSDKVer getSDKVer = (FSalamanderPluginGetSDKVer)GetProcAddress(DLL, "SalamanderPluginGetSDKVer"); // Plugin function
                    if (getSDKVer != NULL)                                                                                               // if the plugin exports this function, it probably wants to increase BuiltForVersion (it looks old for compatibility with older versions of Salamander, but with newer versions of Salamander it wants to use new services)
                    {
                        int verSDK = getSDKVer();
                        if (BuiltForVersion <= verSDK)
                            BuiltForVersion = verSDK;
                        else
                            TRACE_E("CPluginData::InitDLL(): nonsense: SalamanderPluginGetSDKVer() returns older version than SalamanderPluginGetReqVer()");
                    }
                }
                BOOL oldVer = BuiltForVersion < PLUGIN_REQVER;
                if (!oldVer)
                {
                    if (PluginHomePageURL != NULL)
                    { // URL pluginu smazneme az tesne pred volanim entry-pointu, aby URL nezmizelo pokud se load pluginu nezdari kvuli stare verzi pluginu (URL muze user pouzit pro ziskani nove verze pluginu)
                        free(PluginHomePageURL);
                        PluginHomePageURL = NULL;
                    }

                    BOOL oldPluginIsNethood = PluginIsNethood;
                    BOOL oldPluginUsesPasswordManager = PluginUsesPasswordManager;
                    PluginIsNethood = FALSE;
                    PluginUsesPasswordManager = FALSE;

                    EnterPlugin(); // for the entry-point of the plug-in
                    Plugins.EnterDataCS();
                    PluginIface.Init((CPluginInterfaceAbstract*)-1, BuiltForVersion); // to be able to use SetFlagLoadOnSalamanderStart from the entry-point
                    Plugins.LeaveDataCS();
                    SalamanderGeneral.Init((CPluginInterfaceAbstract*)-1); // to be able to use SetFlagLoadOnSalamanderStart from the entry-point

                    // !!! CALLING THE ENTRY-POINT of the plug-in !!!
                    CPluginInterfaceAbstract* resIface = entry(&salamander);

                    Plugins.EnterDataCS();
                    PluginIface.Init(resIface, BuiltForVersion);
                    Plugins.LeaveDataCS();
                    LeavePlugin();
                    SalamanderGeneral.Init(PluginIface.GetInterface());

                    if (!PluginIface.NotEmpty())
                    {
                        PluginIsNethood = oldPluginIsNethood; // returning old value in case of entry-point failure
                        PluginUsesPasswordManager = oldPluginUsesPasswordManager;
                    }
                    else
                    {
                        if (PluginIsNethood != oldPluginIsNethood)
                            refreshUNCRootPaths = TRUE;
                    }
                }
                else // probably unnecessary, just for fun
                {
                    Plugins.EnterDataCS();
                    PluginIface.Init(NULL, 0);
                    Plugins.LeaveDataCS();
                    SalamanderGeneral.Init(NULL);
                }

                if (!oldVer && PluginIface.NotEmpty() && salamander.DataValid())
                { // we will extract interfaces of other parts of the plug-in interface
                    PluginIfaceForArchiver.Init(PluginIface.GetInterfaceForArchiver());
                    PluginIfaceForViewer.Init(PluginIface.GetInterfaceForViewer());
                    PluginIfaceForMenuExt.Init(PluginIface.GetInterfaceForMenuExt(), BuiltForVersion);
                    PluginIfaceForFS.Init(PluginIface.GetInterfaceForFS(), BuiltForVersion);
                    PluginIfaceForThumbLoader.Init(PluginIface.GetInterfaceForThumbLoader(), DLLName, Version);
                }
                else // We will also reset other parts of the plug-in interface
                {
                    if (salamander.ShowError() && oldVer) // old version and still didn't report it...
                    {
                        if (Name == NULL || Name[0] == 0)
                            sprintf(bufText, LoadStr(IDS_OLDPLUGINVERSION2), s);
                        else
                            sprintf(bufText, LoadStr(IDS_OLDPLUGINVERSION), Name, s);
                        if (!quiet)
                            SalMessageBox(parent, bufText, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONERROR);
                    }

                    PluginIfaceForArchiver.Init(NULL);
                    PluginIfaceForViewer.Init(NULL);
                    PluginIfaceForMenuExt.Init(NULL, 0);
                    PluginIfaceForFS.Init(NULL, 0);
                    PluginIfaceForThumbLoader.Init(NULL, NULL, NULL);
                }

                BOOL archiverOK = (!SupportPanelView && !SupportPanelEdit && !SupportCustomPack &&
                                       !SupportCustomUnpack ||
                                   PluginIfaceForArchiver.NotEmpty());
                BOOL viewerOK = !SupportViewer || PluginIfaceForViewer.NotEmpty();
                // menuExtOK cannot be determined - if the menu-ext iface is not available, we will not allow menu items to be added
                BOOL FSOK = !SupportFS || PluginIfaceForFS.NotEmpty();

                if (!oldVer && PluginIface.NotEmpty() && salamander.DataValid() &&
                    archiverOK && viewerOK && FSOK)
                {
                    supportPanelView = (!supportPanelView && SupportPanelView);
                    supportPanelEdit = (!supportPanelEdit && SupportPanelEdit);
                    supportCustomPack = (!supportCustomPack && SupportCustomPack);
                    supportCustomUnpack = (!supportCustomUnpack && SupportCustomUnpack);
                    supportViewer = (!supportViewer && SupportViewer);

                    BOOL loaded = FALSE;
                    if (SupportLoadSave) // if load/save from registry is supported, we will perform it
                    {
                        LoadSaveToRegistryMutex.Enter();
                        HKEY hSal;
                        if (SALAMANDER_ROOT_REG != NULL &&
                            OpenKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, hSal))
                        {
                            HKEY actKey;
                            if (OpenKey(hSal, SALAMANDER_PLUGINSCONFIG, actKey))
                            {
                                HKEY regKey;
                                if (OpenKey(actKey, RegKeyName, regKey))
                                {
                                    CSalamanderRegistry registry;
                                    {
                                        CALL_STACK_MESSAGE3("1.PluginIface.LoadConfiguration(, ,) (%s v. %s)", DLLName, Version);
                                        PluginIface.LoadConfiguration(parent, regKey, &registry);
                                    }
                                    loaded = TRUE;
                                    CloseKey(regKey);
                                }
                                CloseKey(actKey);
                            }
                            CloseKey(hSal);
                        }
                        LoadSaveToRegistryMutex.Leave();
                    }
                    // if the registry load did not succeed, we will load default values
                    if (!loaded)
                    {
                        CSalamanderRegistry registry;
                        {
                            CALL_STACK_MESSAGE3("2.PluginIface.LoadConfiguration(, ,) (%s v. %s)", DLLName, Version);
                            PluginIface.LoadConfiguration(parent, NULL, &registry);
                        }
                    }

                    ThumbnailMasks.SetMasksString(""); // Get rid of masks for thumbnail loader, only new ones apply ...
                    ThumbnailMasksDisabled = FALSE;
                    if (PluginIcons != NULL)
                    {
                        delete PluginIcons; // we will get rid of the bitmap with icons, only the new one is valid ...
                        PluginIcons = NULL;
                    }
                    if (PluginIconsGray != NULL)
                    {
                        delete PluginIconsGray; // we will get rid of the bitmap with icons, only the new one is valid ...
                        PluginIconsGray = NULL;
                    }
                    if (PluginDynMenuIcons != NULL)
                        TRACE_E("CPluginData::InitDLL(): PluginDynMenuIcons is not NULL, please contact Petr Solin");
                    ReleasePluginDynMenuIcons(); // if by any chance it exists, we will destroy it, it is unnecessary
                    PluginIconIndex = -1;        // we will get rid of the icon index, only the new one is valid ...
                    PluginSubmenuIconIndex = -1; // we will get rid of the icon index, only the new one is valid ...
                    //j.r. we will not overwrite this value; it was set in the constructor, possibly changed by the user
                    //     and set when reading the configuration of the plugin
                    //ShowSubmenuInPluginsBar = FALSE;  // get rid of the button in the toolbar, it will only be shown if the plugin wishes so again...

                    // Instead of destruction, we back up the old array
                    TIndirectArray<CPluginMenuItem> oldMenuItems(max(1, MenuItems.Count), 1); // copy of the menu for synchronizing hot keys
                    if (!SupportDynMenuExt)                                                   // Dynamic menu is not created in Connect, so we will leave it for later
                    {
                        oldMenuItems.Add(MenuItems.GetData(), MenuItems.Count); // if the copy fails, IsGood() will return FALSE
                        if (oldMenuItems.IsGood())
                            MenuItems.DetachMembers(); // destruction will be performed after synchronization
                        else
                            MenuItems.DestroyMembers(); // we will remove all items from the menu, only the new one will be valid ...
                    }

                    // Remove the FS command from the change-drive menu, only the new one applies...
                    if (ChDrvMenuFSItemName != NULL)
                        free(ChDrvMenuFSItemName);
                    ChDrvMenuFSItemName = NULL;
                    ChDrvMenuFSItemIconIndex = -1;

                    CSalamanderConnect salConnect(Plugins.GetIndexJustForConnect(this), supportCustomPack, supportCustomUnpack,
                                                  supportPanelView, supportPanelEdit, supportViewer);
                    {
                        CALL_STACK_MESSAGE3("PluginIface.Connect(,) (%s v. %s)", DLLName, Version);
                        PluginIface.Connect(parent, &salConnect); // call the connect plug-in
                        if (!SupportDynMenuExt)
                            HotKeysMerge(&oldMenuItems); // Synchronize hot keys
                        HotKeysEnsureIntegrity();        // zabrani konfliktum se Salamanderem nebo s jinym pluginem
                    }
                    if (oldMenuItems.IsGood())
                        oldMenuItems.DestroyMembers(); // now we can shoot down the old field

                    if (SupportDynMenuExt)
                    {
                        BuildMenu(parent, TRUE);
                        if (releaseDynMenuIcons)
                            ReleasePluginDynMenuIcons(); // no one needs this object (for the next menu display, everything is retrieved again)
                    }
                }
                else
                {
                    if (PluginIface.NotEmpty())
                    {
                        if (!archiverOK)
                        {
                            TRACE_E("The plugin didn't provide interface for archiver (see GetInterfaceForArchiver).");
                        }
                        if (!viewerOK)
                        {
                            TRACE_E("The plugin didn't provide interface for viewer (see GetInterfaceForViewer).");
                        }
                        if (!FSOK)
                        {
                            TRACE_E("The plugin didn't provide interface for file-system (see GetInterfaceForFS).");
                        }
                        {
                            CALL_STACK_MESSAGE3("PluginIface.Release(,) (%s v. %s)", DLLName, Version);
                            PluginIface.Release(parent, TRUE);
                        }
                        Plugins.EnterDataCS();
                        PluginIface.Init(NULL, 0);
                        Plugins.LeaveDataCS();
                        PluginIfaceForArchiver.Init(NULL);
                        PluginIfaceForViewer.Init(NULL);
                        PluginIfaceForMenuExt.Init(NULL, 0);
                        PluginIfaceForFS.Init(NULL, 0);
                        PluginIfaceForThumbLoader.Init(NULL, NULL, NULL);
                        SalamanderGeneral.Init(NULL);
                    }
                    SalamanderGeneral.Clear();
                    HANDLES(FreeLibrary(DLL));
                    DLL = NULL;
                    BuiltForVersion = 0;

                    // we will remove icon-overlays (if the plugin managed to set them up at all)
                    ReleaseIconOverlays();

                    if (!oldVer && salamander.ShowError())
                    { // the plug-in is the correct version and did not successfully call SetBasicPluginData
                        if (Name == NULL || Name[0] == 0)
                            sprintf(bufText, LoadStr(IDS_PLUGININVALID2), s);
                        else
                            sprintf(bufText, LoadStr(IDS_PLUGININVALID), Name, s);
                        if (!quiet)
                            SalMessageBox(parent, bufText, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONERROR);
                    }
                }
            }
            else // plug-in does not have Salamander Plugin Entry Point ...
            {
                HANDLES(FreeLibrary(DLL));
                DLL = NULL;

                sprintf(bufText, LoadStr(IDS_UNABLETOFINDPLUGINENTRY), s);
                if (!quiet)
                    SalMessageBox(parent, bufText, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONERROR);
            }
        }
        // inform the main window about loading the plugin
        if (DLL != NULL)
            MainWindow->OnPluginsStateChanged();

        if (refreshUNCRootPaths && MainWindow != NULL &&
            MainWindow->LeftPanel != NULL && MainWindow->RightPanel != NULL)
        { // Change of return value of CPlugins::GetFirstNethoodPluginFSName() - affects root of UNC paths (is/is not up-dir), refresh necessary
            if (MainWindow->LeftPanel->Is(ptDisk) && IsUNCRootPath(MainWindow->LeftPanel->GetPath()))
            {
                HANDLES(EnterCriticalSection(&TimeCounterSection));
                int t1 = MyTimeCounter++;
                HANDLES(LeaveCriticalSection(&TimeCounterSection));
                PostMessage(MainWindow->LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            }
            if (MainWindow->RightPanel->Is(ptDisk) && IsUNCRootPath(MainWindow->RightPanel->GetPath()))
            {
                HANDLES(EnterCriticalSection(&TimeCounterSection));
                int t1 = MyTimeCounter++;
                HANDLES(LeaveCriticalSection(&TimeCounterSection));
                PostMessage(MainWindow->RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            }
            if ((MainWindow->LeftPanel->Is(ptDisk) || MainWindow->LeftPanel->Is(ptZIPArchive)) &&
                IsUNCPath(MainWindow->LeftPanel->GetPath()) &&
                MainWindow->LeftPanel->DirectoryLine != NULL)
            {
                MainWindow->LeftPanel->DirectoryLine->BuildHotTrackItems();
            }
            if ((MainWindow->RightPanel->Is(ptDisk) || MainWindow->RightPanel->Is(ptZIPArchive)) &&
                IsUNCPath(MainWindow->RightPanel->GetPath()) &&
                MainWindow->RightPanel->DirectoryLine != NULL)
            {
                MainWindow->RightPanel->DirectoryLine->BuildHotTrackItems();
            }
            PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
        }
    }
    return DLL != NULL;
}

void CPluginData::GetDisplayName(char* buf, int bufSize)
{
    const char* add = LoadStr(IDS_PLUGINSUFFIX);
    int l = (int)strlen(add);
    if (l + 1 < bufSize)
    {
        lstrcpyn(buf, Name, bufSize - l);
        lstrcpyn(buf + strlen(buf), add, l + 1);
    }
    else
    {
        if (bufSize > 0)
            buf[0] = 0;
    }
}

void CPluginData::AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                              DWORD state_or, DWORD state_and, DWORD skillLevel,
                              CPluginMenuItemType type)
{
    CALL_STACK_MESSAGE12("CPluginData::AddMenuItem(%d, %s, %u, %d, %d, 0x%X, 0x%X, 0x%X, %d) (%s v. %s)",
                         iconIndex, name, hotKey, id, callGetState, state_or, state_and,
                         skillLevel, (int)type, DLLName, Version);
    DWORD state = 0;
    if (callGetState)
        state = -1;
    else
    {
        if (name != NULL)                                               // it is not a separator
            state = ((state_or & 0x7FFF) << 16) | (state_and & 0x7FFF); // to never return -1 (0xFFFFFFFF)
        else
            id = 0; // separator does not have 'id' if 'callGetState'==TRUE
    }
    if (type == pmitEndSubmenu)
    {
        name = NULL;
        state = 0;
        id = 0;
        skillLevel = MENU_SKILLLEVEL_ALL;
    }
    if (name == NULL)
        iconIndex = -1;
    //  if (type == pmitStartSubmenu && state != -1) id = 0;  // Petr: nevim proc to tu bylo, kazdopadne Shift+F1 pro disablovane submenu jinak neumim (napr. FTP Client/Transfer Mode)
    CPluginMenuItem* item = new CPluginMenuItem(iconIndex, name, hotKey, state, id, skillLevel, type);
    if (item != NULL)
    {
        MenuItems.Add(item);
        if (MenuItems.IsGood())
            item = NULL;
        else
            MenuItems.ResetState();
    }
    else
        TRACE_E(LOW_MEMORY);
    if (item != NULL)
        delete item;
}

BOOL CPluginData::GetMenuItemHotKey(int id, WORD* hotKey, char* hotKeyText, int hotKeyTextSize)
{
    int i;
    for (i = 0; i < MenuItems.Count; i++)
    {
        CPluginMenuItem* item = MenuItems[i];
        if (item->ID == id)
        {
            if (hotKey != NULL)
                *hotKey = HOTKEY_GET(item->HotKey);
            if (hotKeyText != NULL)
            {
                char buff[200];
                GetHotKeyText(HOTKEY_GET(item->HotKey), buff);
                lstrcpyn(hotKeyText, buff, hotKeyTextSize);
            }
            return TRUE;
        }
    }
    return FALSE;
}

void CPluginData::ClearSUID()
{
    int i;
    for (i = 0; i < MenuItems.Count; i++)
        MenuItems[i]->SUID = -1;
}

BOOL CPluginData::Remove(HWND parent, int index, BOOL canDelPluginRegKey)
{
    CALL_STACK_MESSAGE6("CPluginData::Remove(0x%p, %d, %d) (%s v. %s)",
                        parent, index, canDelPluginRegKey, DLLName, Version);
    BOOL unloaded = !GetLoaded();
    if (!unloaded)
    {
        if (MainWindow == NULL || MainWindow->CanUnloadPlugin(parent, PluginIface.GetInterface()))
        {
            // maybe there will be an unload of the plugin: we will let the plugin delete previously undeleted tmp files from the disk cache
            CPluginInterfaceAbstract* unloadedPlugin = PluginIface.GetInterface();
            DeleteManager.PluginMayBeUnloaded(parent, this);

            // the plug-in is no longer used by Salamander, it can be unloaded
            if (PluginIface.Release(parent, FALSE))
                unloaded = TRUE; // it will be unloaded and it will be possible to remove it
            else
            {
                char buf[MAX_PATH + 100];
                sprintf(buf, LoadStr(IDS_PLUGINFORCEUNLOAD), Name);
                if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION), MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    PluginIface.Release(parent, TRUE);
                    unloaded = TRUE; // it will be unloaded and it will be possible to remove it
                }
            }
            if (unloaded)
            {
                // we will perform the unload SPL+SLG and clean the interface
                SalamanderGeneral.Clear();
                if (DLL != NULL)
                    HANDLES(FreeLibrary(DLL));
                DLL = NULL;
                BuiltForVersion = 0;
                Plugins.EnterDataCS();
                PluginIface.Init(NULL, 0);
                Plugins.LeaveDataCS();
                PluginIfaceForArchiver.Init(NULL);
                PluginIfaceForViewer.Init(NULL);
                PluginIfaceForMenuExt.Init(NULL, 0);
                PluginIfaceForFS.Init(NULL, 0);
                PluginIfaceForThumbLoader.Init(NULL, NULL, NULL);
                SalamanderGeneral.Init(NULL);

                // When unloading the plugin, we remove its icon overlays
                ReleaseIconOverlays();

                // disconnect the unloaded plugin from the delete manager and disk cache
                DeleteManager.PluginWasUnloaded(this, unloadedPlugin);
            }
        }
    }

    if (unloaded)
    {
        // Modify "file viewer" - delete records about this plug-in + measures to prevent crashing of the Plugins array
        CViewerMasks* viewerMasks;
        MainWindow->EnterViewerMasksCS();
        int k;
        for (k = 0; k < 2; k++)
        {
            if (k == 0)
                viewerMasks = MainWindow->ViewerMasks;
            else
                viewerMasks = MainWindow->AltViewerMasks;
            int i;
            for (i = 0; i < viewerMasks->Count; i++)
            {
                int type = viewerMasks->At(i)->ViewerType;
                if (type < 0) // It's not about external or internal -> plug-in
                {
                    type = -type - 1;
                    if (type == index)
                        viewerMasks->Delete(i--); // delete this plug-in record
                    else
                    {
                        if (type > index) // the Plugins array will collapse -> we will decrease 'type' by one
                        {
                            type--;
                            viewerMasks->At(i)->ViewerType = -type - 1;
                        }
                    }
                }
            }
        }
        MainWindow->LeaveViewerMasksCS();

        // Modify "custom pack" - delete records of this plug-in + measures to prevent the Plugins array from crashing
        int i;
        for (i = 0; i < PackerConfig.GetPackersCount(); i++)
        {
            int type = PackerConfig.GetPackerType(i);
            if (type != CUSTOMPACKER_EXTERNAL) // it's not about external
            {
                type = -type - 1;
                if (type == index)
                    PackerConfig.DeletePacker(i--); // delete this plug-in record
                else
                {
                    if (type > index) // the Plugins array will collapse -> we will decrease 'type' by one
                    {
                        type--;
                        PackerConfig.SetPackerType(i, -type - 1);
                    }
                }
            }
        }

        // Modification "custom unpack" - we will delete records about this plug-in + measures against the crash of the Plugins array
        for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
        {
            int type = UnpackerConfig.GetUnpackerType(i);
            if (type != CUSTOMUNPACKER_EXTERNAL) // it's not about external
            {
                type = -type - 1;
                if (type == index)
                    UnpackerConfig.DeleteUnpacker(i--); // delete this plug-in record
                else
                {
                    if (type > index) // the Plugins array will collapse -> we will decrease 'type' by one
                    {
                        type--;
                        UnpackerConfig.SetUnpackerType(i, -type - 1);
                    }
                }
            }
        }

        // modify "panel view/edit" - we will modify/delete records about this plug-in
        // + measures due to the collapse of the Plugins field
        for (i = 0; i < PackerFormatConfig.GetFormatsCount(); i++)
        {
            BOOL usePack = PackerFormatConfig.GetUsePacker(i);
            int pack;
            if (usePack)
                pack = PackerFormatConfig.GetPackerIndex(i);
            int unpack = PackerFormatConfig.GetUnpackerIndex(i);
            BOOL removePack = FALSE;
            BOOL removeUnpack = FALSE;
            if (unpack < 0) // It's not an external "view"
            {
                unpack = -unpack - 1;
                if (unpack == index)
                    removeUnpack = TRUE; // delete this plug-in record
                else
                {
                    if (unpack > index) // Plugins array will collapse -> decrease 'unpack' by one
                    {
                        unpack--;
                        PackerFormatConfig.SetUnpackerIndex(i, -unpack - 1);
                    }
                }
            }
            if (usePack && pack < 0) // It's not about an external "edit"
            {
                pack = -pack - 1;
                if (pack == index)
                    removePack = TRUE; // delete this plug-in record
                else
                {
                    if (pack > index) // the Plugins array will collapse -> we will decrease 'pack' by one
                    {
                        pack--;
                        PackerFormatConfig.SetPackerIndex(i, -pack - 1);
                    }
                }
            }

            if (removePack || removeUnpack) // it is necessary to find a replacement for "view" and/or "edit"
            {
                // we will search for an archiver that can "view" and/or "edit" for some of the extensions
                int newView, newEdit;
                BOOL viewFound, editFound;
                Plugins.FindViewEdit(PackerFormatConfig.GetExt(i), index, viewFound, newView, editFound, newEdit);
                if (newView < 0 && -newView - 1 > index)
                    newView++; // There will be a collapse of the Plugins field -> necessary adjustment
                if (newEdit < 0 && -newEdit - 1 > index)
                    newEdit++; // There will be a collapse of the Plugins field -> necessary adjustment

                if (removeUnpack) // need to replace "view"
                {
                    if (viewFound)
                        PackerFormatConfig.SetUnpackerIndex(i, newView); // we will use a new "view"
                    else
                        PackerFormatConfig.DeleteFormat(i--); // it is not possible to function without "view"
                }
                if (removePack &&                 // "edit" needs to be replaced and
                    (!removeUnpack || viewFound)) // record was not deleted
                {
                    if (editFound)
                        PackerFormatConfig.SetPackerIndex(i, newEdit); // we will use the new "edit"
                    else
                        PackerFormatConfig.SetUsePacker(i, FALSE); // not "edit"
                }
            }
        }
        PackerFormatConfig.BuildArray();

        if (SupportLoadSave && canDelPluginRegKey) // Does the plugin support loading/saving configuration + can we delete its key in the registry (this is not about importing configuration from a previous version of Salamander)
        {                                          // We will try to open a private key in the registry, if successful we will delete it, it will no longer be needed
            BOOL shouldDelete = FALSE;
            LoadSaveToRegistryMutex.Enter();
            HKEY salamander;
            if (SALAMANDER_ROOT_REG != NULL &&
                OpenKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
            {
                HKEY actKey;
                if (OpenKey(salamander, SALAMANDER_PLUGINSCONFIG, actKey))
                {
                    HKEY regKey;
                    if (OpenKey(actKey, RegKeyName, regKey))
                    {
                        shouldDelete = TRUE;
                        CloseKey(regKey);
                    }
                    CloseKey(actKey);
                }
                CloseKey(salamander);
            }
            if (shouldDelete)
            {
                if (SALAMANDER_ROOT_REG != NULL &&
                    CreateKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander)) // to have write permissions
                {
                    HKEY actKey;
                    if (CreateKey(salamander, SALAMANDER_PLUGINSCONFIG, actKey))
                    {
                        HKEY regKey;
                        if (CreateKey(actKey, RegKeyName, regKey))
                        {
                            ClearKey(regKey);
                            CloseKey(regKey);
                        }
                        DeleteKey(actKey, RegKeyName);
                        CloseKey(actKey);
                    }
                    CloseKey(salamander);
                }
            }
            LoadSaveToRegistryMutex.Leave();
        }
        return TRUE;
    }
    ThumbnailMasksDisabled = FALSE; // remove was interrupted
    return FALSE;
}

void CPluginData::Save(HWND parent, HKEY regKeyConfig)
{
    CALL_STACK_MESSAGE5("CPluginData::Save(0x%p, 0x%p) (%s v. %s)", parent, regKeyConfig, DLLName, Version);
    if (SupportLoadSave && InitDLL(parent))
    {
        HKEY regKey;
        if (CreateKey(regKeyConfig, RegKeyName, regKey))
        {
            CSalamanderRegistry registry;
            PluginIface.SaveConfiguration(parent, regKey, &registry);
            CloseKey(regKey);
        }
    }
}

void CPluginData::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE4("CPluginData::Configuration(0x%p) (%s v. %s)", parent, DLLName, Version);
    if (InitDLL(parent))
    {
        PluginIface.Configuration(parent);
    }
}

void CPluginData::Event(int event, DWORD param)
{
    CALL_STACK_MESSAGE4("CPluginData::Event(%d,) (%s v. %s)", event, DLLName, Version);
    if (GetLoaded() && PluginIface.NotEmpty()) // we only call if the plug-in is loaded (it's just a "notification")
    {
        PluginIface.Event(event, param);
    }
}

void CPluginData::ClearHistory(HWND parent)
{
    CALL_STACK_MESSAGE3("CPluginData::ClearHistory() (%s v. %s)", DLLName, Version);
    if (InitDLL(parent))
    {
        PluginIface.ClearHistory(parent);
    }
}

void CPluginData::AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs)
{
    CALL_STACK_MESSAGE3("CPluginData::AcceptChangeOnPathNotification() (%s v. %s)", DLLName, Version);
    if (GetLoaded() && PluginIface.NotEmpty()) // we only call if the plug-in is loaded (it's just a "notification")
    {
        PluginIface.AcceptChangeOnPathNotification(path, includingSubdirs);
    }
}

void CPluginData::PasswordManagerEvent(HWND parent, int event)
{
    CALL_STACK_MESSAGE4("CPluginData::PasswordManagerEvent(, %d) (%s v. %s)", event, DLLName, Version);
    if (GetLoaded() && PluginUsesPasswordManager) // in case the plugin stops using the Password Manager (does not call SetPluginUsesPasswordManager())
        PluginIface.PasswordManagerEvent(parent, event);
}

void CPluginData::About(HWND parent)
{
    CALL_STACK_MESSAGE4("CPluginData::About(0x%p) (%s v. %s)", parent, DLLName, Version);
    if (InitDLL(parent))
    {
        PluginIface.About(parent);
    }
}

void CPluginData::CallLoadOrSaveConfiguration(BOOL load,
                                              FSalLoadOrSaveConfiguration loadOrSaveFunc,
                                              void* param)
{ // It is called from the plug-in (there is no need to print DLLName + Version)
    CALL_STACK_MESSAGE2("CPluginData::CallLoadOrSaveConfiguration(%d, ,)", load);
    if (load) // load
    {
        BOOL loaded = FALSE;
        if (SupportLoadSave) // if it supports load/save from registry
        {
            LoadSaveToRegistryMutex.Enter();
            HKEY salamander;
            if (SALAMANDER_ROOT_REG != NULL &&
                OpenKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
            {
                HKEY actKey;
                if (OpenKey(salamander, SALAMANDER_PLUGINSCONFIG, actKey))
                {
                    HKEY regKey;
                    if (OpenKey(actKey, RegKeyName, regKey)) // try to open the private key of the plug-in
                    {
                        CSalamanderRegistry registry;
                        {
                            CALL_STACK_MESSAGE1("1.CPluginData::CallLoadOrSaveConfiguration::loadOrSaveFunc()");
                            loadOrSaveFunc(load, regKey, &registry, param);
                        }
                        loaded = TRUE;
                        CloseKey(regKey);
                    }
                    CloseKey(actKey);
                }
                CloseKey(salamander);
            }
            LoadSaveToRegistryMutex.Leave();
        }

        // otherwise load default configuration
        if (!loaded)
        {
            CSalamanderRegistry registry;
            {
                CALL_STACK_MESSAGE1("2.CPluginData::CallLoadOrSaveConfiguration::loadOrSaveFunc()");
                loadOrSaveFunc(load, NULL, &registry, param);
            }
        }
    }
    else // save
    {
        if (SupportLoadSave) // otherwise there is nowhere to save
        {
            LoadSaveToRegistryMutex.Enter();
            HKEY salamander;
            if (SALAMANDER_ROOT_REG != NULL &&
                OpenKeyAux(NULL, HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander)) // test if the key Salama even exists (otherwise we don't save anything)
            {                                                                         // OpenKeyAux, because I don't want the message about Load Configuration
                CloseKeyAux(salamander);
                if (CreateKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
                {
                    BOOL cfgIsOK = TRUE;
                    BOOL deleteSALAMANDER_SAVE_IN_PROGRESS = !IsSetSALAMANDER_SAVE_IN_PROGRESS;
                    if (deleteSALAMANDER_SAVE_IN_PROGRESS)
                    {
                        DWORD saveInProgress = 1;
                        if (GetValueAux(NULL, salamander, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD)))
                        {                    // GetValueAux because I don't want a message about Load Configuration
                            cfgIsOK = FALSE; // It is about a damaged configuration, it is not fixed by saving (it is not saved completely)
                            TRACE_E("CPluginData::CallLoadOrSaveConfiguration(): unable to save configuration, configuration key in registry is corrupted, plugin: " << Name);
                        }
                        else
                        {
                            saveInProgress = 1;
                            SetValue(salamander, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD));
                            IsSetSALAMANDER_SAVE_IN_PROGRESS = TRUE;
                        }
                    }
                    if (cfgIsOK)
                    {
                        HKEY actKey;
                        if (CreateKey(salamander, SALAMANDER_PLUGINSCONFIG, actKey))
                        {
                            HKEY regKey;
                            if (CreateKey(actKey, RegKeyName, regKey))
                            {
                                CSalamanderRegistry registry;
                                {
                                    CALL_STACK_MESSAGE1("3.CPluginData::CallLoadOrSaveConfiguration::loadOrSaveFunc()");
                                    loadOrSaveFunc(load, regKey, &registry, param);
                                }
                                CloseKey(regKey);
                            }
                            CloseKey(actKey);
                        }
                        if (deleteSALAMANDER_SAVE_IN_PROGRESS)
                        {
                            DeleteValue(salamander, SALAMANDER_SAVE_IN_PROGRESS);
                            IsSetSALAMANDER_SAVE_IN_PROGRESS = FALSE;
                        }
                    }
                    CloseKey(salamander);
                }
            }
            LoadSaveToRegistryMutex.Leave();
        }
    }
}

BOOL CPluginData::Unload(HWND parent, BOOL ask)
{
    CALL_STACK_MESSAGE5("CPluginData::Unload(0x%p, %d) (%s v. %s)", parent, ask, DLLName, Version);
    BOOL ret = FALSE;
    if (DLL != NULL)
    {
        if (MainWindow == NULL || MainWindow->CanUnloadPlugin(parent, PluginIface.GetInterface()))
        { // the plug-in is no longer used by Salamander, it can be unloaded
            char buf[MAX_PATH + 300];
            BOOL skipUnload = FALSE;
            if (SupportLoadSave && ::Configuration.AutoSave)
            { // wants to save the configuration when "save on exit" is set to "on"?
                sprintf(buf, LoadStr(IDS_PLUGINSAVECONFIG), Name);
                if (!ask || SalMessageBox(parent, buf, LoadStr(IDS_QUESTION), MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    LoadSaveToRegistryMutex.Enter();
                    BOOL salKeyDoesNotExist = FALSE;
                    HKEY salamander;
                    if (SALAMANDER_ROOT_REG != NULL &&
                        OpenKeyAux(NULL, HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander)) // test if the key Salama even exists (otherwise we don't save anything)
                    {                                                                         // OpenKeyAux, because I don't want the message about Load Configuration
                        CloseKeyAux(salamander);
                        if (CreateKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
                        {
                            BOOL cfgIsOK = TRUE;
                            BOOL deleteSALAMANDER_SAVE_IN_PROGRESS = !IsSetSALAMANDER_SAVE_IN_PROGRESS;
                            if (deleteSALAMANDER_SAVE_IN_PROGRESS)
                            {
                                DWORD saveInProgress = 1;
                                if (GetValueAux(NULL, salamander, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD)))
                                {                    // GetValueAux because I don't want a message about Load Configuration
                                    cfgIsOK = FALSE; // It is about a damaged configuration, it is not fixed by saving (it is not saved completely)
                                    salKeyDoesNotExist = TRUE;
                                    TRACE_E("CPluginData::Unload(): unable to save configuration, configuration key in registry is corrupted, plugin: " << Name);
                                }
                                else
                                {
                                    saveInProgress = 1;
                                    SetValue(salamander, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD));
                                    IsSetSALAMANDER_SAVE_IN_PROGRESS = TRUE;
                                }
                            }
                            if (cfgIsOK)
                            {
                                HKEY actKey;
                                if (CreateKey(salamander, SALAMANDER_PLUGINSCONFIG, actKey))
                                {
                                    Save(parent, actKey);
                                    CloseKey(actKey);
                                }
                                if (deleteSALAMANDER_SAVE_IN_PROGRESS)
                                {
                                    DeleteValue(salamander, SALAMANDER_SAVE_IN_PROGRESS);
                                    IsSetSALAMANDER_SAVE_IN_PROGRESS = FALSE;
                                }
                            }
                            CloseKey(salamander);
                        }
                        else
                            salKeyDoesNotExist = TRUE;
                    }
                    else
                        salKeyDoesNotExist = TRUE;
                    LoadSaveToRegistryMutex.Leave();

                    if (ask && salKeyDoesNotExist)
                    {
                        sprintf(buf, LoadStr(IDS_PLUGINSAVEFAILED), Name);
                        skipUnload = SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                                   MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO;
                    }
                }
                if (GlobalSaveWaitWindow != NULL)
                    GlobalSaveWaitWindow->SetProgressPos(++GlobalSaveWaitWindowProgress);
            }

            if (!skipUnload && PluginIface.NotEmpty())
            {
                // maybe there will be an unload of the plugin: we will let the plugin delete previously undeleted tmp files from the disk cache
                CPluginInterfaceAbstract* unloadedPlugin = PluginIface.GetInterface();
                DeleteManager.PluginMayBeUnloaded(parent, this);

                if (PluginIface.Release(parent, CriticalShutdown) || CriticalShutdown)
                    ret = TRUE;
                else
                {
                    sprintf(buf, LoadStr(IDS_PLUGINFORCEUNLOAD), Name);
                    if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION), MB_YESNO | MB_ICONQUESTION) == IDYES)
                    {
                        PluginIface.Release(parent, TRUE);
                        ret = TRUE;
                    }
                }

                if (ret)
                {
                    // we will perform the unload SPL+SLG and clean the interface
                    SalamanderGeneral.Clear();
                    if (DLL != NULL)
                        HANDLES(FreeLibrary(DLL));
                    DLL = NULL;
                    BuiltForVersion = 0;
                    Plugins.EnterDataCS();
                    PluginIface.Init(NULL, 0);
                    Plugins.LeaveDataCS();
                    PluginIfaceForArchiver.Init(NULL);
                    PluginIfaceForViewer.Init(NULL);
                    PluginIfaceForMenuExt.Init(NULL, 0);
                    PluginIfaceForFS.Init(NULL, 0);
                    PluginIfaceForThumbLoader.Init(NULL, NULL, NULL);
                    SalamanderGeneral.Init(NULL);

                    // When unloading the plugin, we remove its icon overlays
                    ReleaseIconOverlays();

                    // disconnect the unloaded plugin from the delete manager and disk cache
                    DeleteManager.PluginWasUnloaded(this, unloadedPlugin);
                }
            }
        }
        ThumbnailMasksDisabled = FALSE; // unload was completed (we can now safely let the plugin load again)
    }
    return ret;
}

BOOL CPluginData::GetMenuItemStateType(int pluginIndex, int menuItemIndex, MENU_ITEM_INFO* mii)
{
    CPluginMenuItem* item = MenuItems[menuItemIndex];

    DWORD mask = GetMaskForMenuItems(pluginIndex);

    if (item->StateMask == -1) // Retrieve the status of an item directly from the plug-in?
    {
        DWORD state = 0;
        if (PluginIfaceForMenuExt.NotEmpty())
            state = PluginIfaceForMenuExt.GetMenuItemState(item->ID, mask);
        else
            TRACE_E("PluginIfaceForMenuExt is not initialized!");

        //    if (state & MENU_ITEM_STATE_HIDDEN) hidden = TRUE;
        if (state & MENU_ITEM_STATE_ENABLED)
            mii->State = 0;
        else
            mii->State = MENU_STATE_GRAYED;
        if (state & MENU_ITEM_STATE_CHECKED)
        {
            mii->State |= MENU_STATE_CHECKED;
            if (state & MENU_ITEM_STATE_RADIO)
                mii->Type |= MENU_TYPE_RADIOCHECK;
        }
    }
    else // item state is calculated from and and or masks
    {
        if ((mask & HIWORD(item->StateMask)) != 0 &&                     // bitwise OR mask
            (mask & LOWORD(item->StateMask)) == LOWORD(item->StateMask)) // bitwise AND operator
        {
            mii->State = 0;
        }
        else
        {
            mii->State = MENU_STATE_GRAYED;
        }
    }
    return (mii->State != MENU_STATE_GRAYED);
}

void CPluginData::AddMenuItemsToSubmenuAux(CMenuPopup* menu, int& i, int count, DWORD mask)
{
    CALL_STACK_MESSAGE4("CPluginData::AddMenuItemsToSubmenuAux(, %d, %d, 0x%X)", i, count, mask);
    char buff[500];
    for (; i < MenuItems.Count; i++)
    {
        CPluginMenuItem* item = MenuItems[i];
        if (item->Type == pmitEndSubmenu)
            return; // Return from submenu
        BOOL skipSubMenu = FALSE;
        if (item->SkillLevel & CfgSkillLevelToMenu(::Configuration.SkillLevel)) // we will project the skill-level reduction menu
        {
            BOOL hidden = FALSE;
            MENU_ITEM_INFO mi;
            if (item->Name == NULL) // separator or allocation error in the start-submenu name
            {
                if (item->Type == pmitStartSubmenu)
                    skipSubMenu = TRUE; // Allocation error in the start-submenu name: insert separator + skip the rest of the submenu
                mi.Mask = MENU_MASK_TYPE | MENU_MASK_SKILLLEVEL;
                mi.Type = MENU_TYPE_SEPARATOR;
                mi.SkillLevel = 0;
                if (item->SkillLevel & MENU_SKILLLEVEL_BEGINNER)
                    mi.SkillLevel |= MENU_LEVEL_BEGINNER;
                if (item->SkillLevel & MENU_SKILLLEVEL_INTERMEDIATE)
                    mi.SkillLevel |= MENU_LEVEL_INTERMEDIATE;
                if (item->SkillLevel & MENU_SKILLLEVEL_ADVANCED)
                    mi.SkillLevel |= MENU_LEVEL_ADVANCED;
                if (item->StateMask == -1) // Determine the visibility of the separator directly from the plugin?
                {
                    DWORD state = PluginIfaceForMenuExt.GetMenuItemState(item->ID, mask);

                    if (state & MENU_ITEM_STATE_HIDDEN)
                        hidden = TRUE;
                }
            }
            else // normal menu item or submenu
            {
                mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID |
                          MENU_MASK_STRING | MENU_MASK_SKILLLEVEL | MENU_MASK_IMAGEINDEX |
                          (item->Type == pmitStartSubmenu ? MENU_MASK_SUBMENU : 0);
                mi.Type = MENU_TYPE_STRING;
                lstrcpyn(buff, item->Name, 400);
                mi.String = buff;

                if (HOTKEY_GET(item->HotKey) != 0)
                {
                    // If we have a hint in the text, we remove it
                    if ((item->HotKey & HOTKEY_HINT) != 0)
                    {
                        char* p = buff;
                        while (*p != 0)
                        {
                            if (*p == '\t')
                            {
                                *p = 0;
                                break;
                            }
                            p++;
                        }
                    }

                    strcat(buff, "\t");
                    GetHotKeyText(LOWORD(item->HotKey), buff + strlen(buff));
                }
                mi.ImageIndex = item->IconIndex;
                mi.SkillLevel = 0;
                if (item->SkillLevel & MENU_SKILLLEVEL_BEGINNER)
                    mi.SkillLevel |= MENU_LEVEL_BEGINNER;
                if (item->SkillLevel & MENU_SKILLLEVEL_INTERMEDIATE)
                    mi.SkillLevel |= MENU_LEVEL_INTERMEDIATE;
                if (item->SkillLevel & MENU_SKILLLEVEL_ADVANCED)
                    mi.SkillLevel |= MENU_LEVEL_ADVANCED;
                if (item->StateMask == -1) // Retrieve the status of an item directly from the plug-in?
                {
                    DWORD state = PluginIfaceForMenuExt.GetMenuItemState(item->ID, mask);

                    if (state & MENU_ITEM_STATE_HIDDEN)
                        hidden = TRUE;
                    if (state & MENU_ITEM_STATE_ENABLED)
                        mi.State = 0;
                    else
                        mi.State = MENU_STATE_GRAYED;
                    if (state & MENU_ITEM_STATE_CHECKED)
                    {
                        mi.State |= MENU_STATE_CHECKED;
                        if (state & MENU_ITEM_STATE_RADIO)
                            mi.Type |= MENU_TYPE_RADIOCHECK;
                    }
                }
                else // item state is calculated from and and or masks
                {
                    if ((mask & HIWORD(item->StateMask)) != 0 &&                     // bitwise OR mask
                        (mask & LOWORD(item->StateMask)) == LOWORD(item->StateMask)) // bitwise AND operator
                    {
                        mi.State = 0;
                    }
                    else
                    {
                        mi.State = MENU_STATE_GRAYED;
                    }
                }

                mi.ID = Plugins.LastSUID;      // next unique number within Salamander (SUID)
                item->SUID = Plugins.LastSUID; // we will remember which SUID was assigned
                if (Plugins.LastSUID < CM_PLUGINCMD_MAX)
                    Plugins.LastSUID++; // generating another SUID
                else
                    TRACE_E("Too much commands in plugins.");

                if (item->Type == pmitStartSubmenu && !hidden) // let's populate the submenu
                {
                    mi.SubMenu = new CMenuPopup();
                    if ((mi.State & MENU_STATE_GRAYED) == 0 && mi.SubMenu != NULL)
                    {
                        i++;
                        AddMenuItemsToSubmenuAux((CMenuPopup*)mi.SubMenu, i, 0, mask);
                        if (i >= MenuItems.Count)
                            TRACE_E("CPluginData::AddMenuItemsToSubmenuAux(): missing symbol of end of submenu - see CSalamanderConnectAbstract::AddSubmenuEnd()");
                    }
                    else
                    {
                        if (mi.SubMenu == NULL)
                        {
                            mi.Mask &= ~MENU_MASK_SUBMENU; // we will turn a submenu into a regular item that can be executed (by running command number 0, hopefully that's okay...)
                            TRACE_E(LOW_MEMORY);
                        }
                        skipSubMenu = TRUE; // submenu failed to allocate or is disabled - let's skip it
                    }
                }
            }
            if (!hidden)
            {
                if (!menu->InsertItem(count++, TRUE, &mi) && (mi.Mask & MENU_MASK_SUBMENU) && mi.SubMenu != NULL)
                    delete mi.SubMenu;
            }
            else
                skipSubMenu = TRUE; // item hidden thanks to the hidden state
        }
        else
            skipSubMenu = TRUE; // item hidden thanks to skill level

        if (skipSubMenu && item->Type == pmitStartSubmenu)
        { // if it is a submenu, we skip nested items and submenu (they will not be touched at all)
            int level = 1;
            for (i++; i < MenuItems.Count; i++)
            {
                CPluginMenuItemType type = MenuItems[i]->Type;
                if (type == pmitStartSubmenu)
                    level++;
                else
                {
                    if (type == pmitEndSubmenu && --level == 0)
                        break; // end of submenu found
                }
            }
        }
    }
}

DWORD
CPluginData::GetMaskForMenuItems(int index)
{
    DWORD mask = Plugins.StateCache.ActualStateMask;
    if (index == Plugins.StateCache.ActiveUnpackerIndex || index == Plugins.StateCache.ActivePackerIndex)
    {
        mask |= MENU_EVENT_THIS_PLUGIN_ARCH;
    }
    if (index == Plugins.StateCache.NonactiveUnpackerIndex || index == Plugins.StateCache.NonactivePackerIndex)
    {
        mask |= MENU_EVENT_TARGET_THIS_PLUGIN_ARCH;
    }
    if (index == Plugins.StateCache.ActiveFSIndex)
        mask |= MENU_EVENT_THIS_PLUGIN_FS;
    if (index == Plugins.StateCache.NonactiveFSIndex)
        mask |= MENU_EVENT_TARGET_THIS_PLUGIN_FS;
    if (index == Plugins.StateCache.FileUnpackerIndex || index == Plugins.StateCache.FilePackerIndex)
    {
        mask |= MENU_EVENT_ARCHIVE_FOCUSED;
    }
    return mask;
}

void CPluginData::InitMenuItems(HWND parent, int index, CMenuPopup* menu)
{
    CALL_STACK_MESSAGE4("CPluginData::InitMenuItems(, %d, ) (%s v. %s)", index, DLLName, Version);
    int count = menu->GetItemCount();
    if (count == 0) // Initialization of the submenu is needed
    {

    CHECK_MENU_AGAIN:

        BOOL ok = TRUE;
        if (SupportDynMenuExt) // dynamic menu: prebuild if changed to static during plugin load, check it below
        {
            if (GetLoaded())
                BuildMenu(parent, FALSE); // if we are already loaded, we will rebuild the "manual" menu
            else
                InitDLL(parent, FALSE, TRUE, TRUE, FALSE); // if we are not, the menu will be rebuilt automatically when the plugin is loaded
            if (!GetLoaded() || SupportDynMenuExt && !PluginIfaceForMenuExt.NotEmpty())
                ok = FALSE;
            else
            {
                if (SupportDynMenuExt && MenuItems.Count == 0)
                    TRACE_I("Plugin has dynamic menu which is empty (unexpected situation). We will not open submenu.");
            }
        }
        // static menu: we will determine if there is a reason to load the plugin (state of the item determined through
        // GetMenuItemState) and also whether in this case the plugin returns PluginIfaceForMenuExt (mandatory)
        if (ok && !SupportDynMenuExt)
        {
            DWORD mask = GetMaskForMenuItems(index);
            int i;
            for (i = 0; i < MenuItems.Count; i++)
            {
                CPluginMenuItem* item = MenuItems[i];
                BOOL skipSubMenu = FALSE;
                if (item->SkillLevel & CfgSkillLevelToMenu(::Configuration.SkillLevel)) // we will project the skill-level reduction menu
                {
                    if (item->StateMask == -1)
                    {
                        if (GetLoaded()) // if loaded, we will check if we have the menu-ext iface
                        {
                            if (!PluginIfaceForMenuExt.NotEmpty())
                                TRACE_E("Plugin has menu with items whose state is determined by calling CPluginInterfaceForMenuExtAbstract::GetMenuItemState so it must have menu extension interface (see CPluginInterfaceAbstract::GetInterfaceForMenuExt).");
                        }
                        else // the DLL will be loaded -> the menu items will be updated, we need to run the whole test again
                        {
                            if (InitDLL(parent))
                                goto CHECK_MENU_AGAIN;
                        }
                        ok = GetLoaded() && PluginIfaceForMenuExt.NotEmpty();
                        break;
                    }
                    else
                    {
                        if ((mask & HIWORD(item->StateMask)) == 0 ||                     // bitwise OR mask
                            (mask & LOWORD(item->StateMask)) != LOWORD(item->StateMask)) // bitwise AND operator
                        {                                                                // disabled item
                            skipSubMenu = TRUE;
                        }
                    }
                }
                else
                    skipSubMenu = TRUE; // item hidden thanks to skill level

                if (skipSubMenu && item->Type == pmitStartSubmenu)
                { // if it is a submenu, we skip nested items and submenu (they will not be touched at all)
                    int level = 1;
                    for (i++; i < MenuItems.Count; i++)
                    {
                        CPluginMenuItemType type = MenuItems[i]->Type;
                        if (type == pmitStartSubmenu)
                            level++;
                        else
                        {
                            if (type == pmitEndSubmenu && --level == 0)
                                break; // end of submenu found
                        }
                    }
                }
            }
        }

        if (ok)
        {
            int i = 0;
            DWORD mask = GetMaskForMenuItems(index); // If the plugin was loaded, something could have changed
            AddMenuItemsToSubmenuAux(menu, i, count, mask);
            if (i < MenuItems.Count)
                TRACE_E("CPluginData::InitMenuItems(): superfluous symbol of end of submenu - see CSalamanderConnectAbstract::AddSubmenuEnd()");
        }
    }
}

BOOL CPluginData::ExecuteMenuItem(CFilesWindow* panel, HWND parent, int index, int suid, BOOL& unselect)
{
    CALL_STACK_MESSAGE5("CPluginData::ExecuteMenuItem(, , %d, %d, ) (%s v. %s)", index, suid, DLLName, Version);
    unselect = FALSE;
    int id;
    int i;
    for (i = 0; i < MenuItems.Count; i++)
    {
        if (MenuItems[i]->SUID == suid) // Comparison of the SUID menu item and the executed command
        {
            id = MenuItems[i]->ID;
            if (InitDLL(parent) && PluginIfaceForMenuExt.NotEmpty())
            {
                DWORD mask = GetMaskForMenuItems(index);
                CSalamanderForOperations sm(panel);
                unselect = PluginIfaceForMenuExt.ExecuteMenuItem(&sm, parent, id, mask);
            }
            Plugins.SetLastPlgCmd(DLLName, id); // save the last command
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CPluginData::ExecuteMenuItem2(CFilesWindow* panel, HWND parent, int index, int id, BOOL& unselect)
{
    CALL_STACK_MESSAGE5("CPluginData::ExecuteMenuItem2(, , %d, %d, ) (%s v. %s)", index, id, DLLName, Version);
    unselect = FALSE;
    int i;
    for (i = 0; i < MenuItems.Count; i++)
    {
        if (MenuItems[i]->ID == id) // comparison of the menu item ID and the executed command
        {
            if (PluginIfaceForMenuExt.NotEmpty())
            {
                DWORD mask = GetMaskForMenuItems(index);
                CSalamanderForOperations sm(panel);
                unselect = PluginIfaceForMenuExt.ExecuteMenuItem(&sm, parent, id, mask);
            }
            else
                TRACE_E("PluginIfaceForMenuExt is not initialized!");
            Plugins.SetLastPlgCmd(DLLName, id); // save the last command
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CPluginData::HelpForMenuItem(HWND parent, int index, int suid, BOOL& helpDisplayed)
{
    CALL_STACK_MESSAGE5("CPluginData::HelpForMenuItem(, %d, %d, ) (%s v. %s)", index, suid, DLLName, Version);
    helpDisplayed = FALSE;
    int id;
    int i;
    for (i = 0; i < MenuItems.Count; i++)
    {
        if (MenuItems[i]->SUID == suid) // Comparison of the SUID menu item and the executed command
        {
            id = MenuItems[i]->ID;
            if (InitDLL(parent) && PluginIfaceForMenuExt.NotEmpty())
                helpDisplayed = PluginIfaceForMenuExt.HelpForMenuItem(parent, id);
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CPluginData::BuildMenu(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE3("CPluginData::BuildMenu() (%s v. %s)", DLLName, Version);
    if (GetLoaded() && SupportDynMenuExt && (!DynMenuWasAlreadyBuild || force))
    {
        DynMenuWasAlreadyBuild = TRUE; // Defense against unnecessary menu rebuild: mainly for the situation when the menu is built for Last Command, and then again when opening a submenu of a plugin with a command in Last Command
        if (PluginDynMenuIcons != NULL)
            TRACE_E("CPluginData::BuildMenu(): PluginDynMenuIcons is not NULL, please contact Petr Solin");
        ReleasePluginDynMenuIcons(); // if it happens to exist, let's get rid of it, it's unnecessary
        if (PluginIfaceForMenuExt.NotEmpty())
        {
            // Instead of destruction, we back up the old array
            TIndirectArray<CPluginMenuItem> oldMenuItems(max(1, MenuItems.Count), 1); // copy of the menu for synchronizing hot keys
            oldMenuItems.Add(MenuItems.GetData(), MenuItems.Count);                   // if the copy fails, IsGood() will return FALSE
            if (oldMenuItems.IsGood())
                MenuItems.DetachMembers(); // destruction will be performed after synchronization
            else
                MenuItems.DestroyMembers(); // we will remove all items from the menu, only the new one will be valid ...

            CSalamanderBuildMenu salBuildMenu(Plugins.GetIndexJustForConnect(this));
            {
                CALL_STACK_MESSAGE3("PluginIfaceForMenuExt.BuildMenu(,) (%s v. %s)", DLLName, Version);
                PluginIfaceForMenuExt.BuildMenu(parent, &salBuildMenu); // call the BuildMenu plugin
                HotKeysMerge(&oldMenuItems);                            // Synchronize hot keys
                HotKeysEnsureIntegrity();                               // zabrani konfliktum se Salamanderem nebo s jinym pluginem
            }
            if (oldMenuItems.IsGood())
                oldMenuItems.DestroyMembers(); // now we can shoot down the old field
        }
        else
            TRACE_E("Plugin has dynamic menu so it must have menu extension interface (see CPluginInterfaceAbstract::GetInterfaceForMenuExt).");
    }
    return GetLoaded() && (!SupportDynMenuExt || PluginIfaceForMenuExt.NotEmpty());
}

BOOL CPluginData::ListArchive(CFilesWindow* panel, const char* archiveFileName, CSalamanderDirectory& dir,
                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE4("CPluginData::ListArchive(, %s, ,) (%s v. %s)", archiveFileName, DLLName, Version);
    BOOL ret = FALSE;
    if (InitDLL(MainWindow->HWindow))
    {
        CSalamanderForOperations sc(panel);
        ret = PluginIfaceForArchiver.ListArchive(&sc, archiveFileName, &dir, pluginData);
#ifdef _DEBUG
        if (ret && pluginData != NULL)
            OpenedPDCounter++; // increase OpenedPDCounter
#endif
    }
    return ret;
}

BOOL CPluginData::UnpackArchive(CFilesWindow* panel, const char* archiveFileName,
                                CPluginDataInterfaceAbstract* pluginData,
                                const char* targetDir, const char* archiveRoot,
                                SalEnumSelection nextName, void* param)
{
    CALL_STACK_MESSAGE6("CPluginData::UnpackArchive(, %s, , %s, %s, ,) (%s v. %s)", archiveFileName,
                        targetDir, archiveRoot, DLLName, Version);
    BOOL ret = FALSE;
    if (InitDLL(MainWindow->HWindow))
    {
        CSalamanderForOperations sc(panel);
        ret = PluginIfaceForArchiver.UnpackArchive(&sc, archiveFileName, pluginData, targetDir,
                                                   archiveRoot, nextName, param);
    }
    return ret;
}

BOOL CPluginData::UnpackOneFile(CFilesWindow* panel, const char* archiveFileName,
                                CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                                const CFileData* fileData, const char* targetDir,
                                const char* newFileName, BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE7("CPluginData::UnpackOneFile(, %s, , %s, , %s, %s, ) (%s v. %s)", archiveFileName,
                        nameInArchive, targetDir, newFileName, DLLName, Version);
    BOOL ret = FALSE;
    if (InitDLL(MainWindow->HWindow))
    {
        CSalamanderForOperations sc(panel);
        CreateSafeWaitWindow(LoadStr(IDS_UNPACKINGFILEFROMARC), NULL, 2000, FALSE, MainWindow->HWindow);
        ret = PluginIfaceForArchiver.UnpackOneFile(&sc, archiveFileName, pluginData, nameInArchive,
                                                   fileData, targetDir, newFileName, renamingNotSupported);
        DestroySafeWaitWindow();
    }
    return ret;
}

BOOL CPluginData::PackToArchive(CFilesWindow* panel, const char* archiveFileName,
                                const char* archiveRoot, BOOL move, const char* sourceDir,
                                SalEnumSelection2 nextName, void* param)
{
    CALL_STACK_MESSAGE7("CPluginData::PackToArchive(, %s, %s, %d, %s, ,) (%s v. %s)", archiveFileName,
                        archiveRoot, move, sourceDir, DLLName, Version);
    BOOL ret = FALSE;
    if (InitDLL(MainWindow->HWindow))
    {
        CSalamanderForOperations sc(panel);
        ret = PluginIfaceForArchiver.PackToArchive(&sc, archiveFileName, archiveRoot, move, sourceDir, nextName, param);
    }
    return ret;
}

BOOL CPluginData::DeleteFromArchive(CFilesWindow* panel, const char* archiveFileName,
                                    CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                                    SalEnumSelection nextName, void* param)
{
    CALL_STACK_MESSAGE5("CPluginData::DeleteFromArchive(, %s, , %s, ,) (%s v. %s)",
                        archiveFileName, archiveRoot, DLLName, Version);
    BOOL ret = FALSE;
    if (InitDLL(MainWindow->HWindow))
    {
        CSalamanderForOperations sc(panel);
        ret = PluginIfaceForArchiver.DeleteFromArchive(&sc, archiveFileName, pluginData, archiveRoot, nextName, param);
    }
    return ret;
}

BOOL CPluginData::UnpackWholeArchive(CFilesWindow* panel, const char* archiveFileName, const char* mask,
                                     const char* targetDir, BOOL delArchiveWhenDone, CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE7("CPluginData::UnpackWholeArchive(, %s, %s, %s, %d,) (%s v. %s)", archiveFileName,
                        mask, targetDir, delArchiveWhenDone, DLLName, Version);
    BOOL ret = FALSE;
    if (InitDLL(MainWindow->HWindow))
    {
        CSalamanderForOperations sc(panel);
        ret = PluginIfaceForArchiver.UnpackWholeArchive(&sc, archiveFileName, mask, targetDir,
                                                        delArchiveWhenDone, archiveVolumes);
    }
    return ret;
}

BOOL CPluginData::CanCloseArchive(CFilesWindow* panel, const char* archiveFileName, BOOL force)
{
    CALL_STACK_MESSAGE5("CPluginData::CanCloseArchive(, %s, %d) (%s v. %s)", archiveFileName,
                        force, DLLName, Version);
    BOOL ret = TRUE;
    if (InitDLL(MainWindow->HWindow))
    {
        CSalamanderForOperations sc(panel);
        ret = PluginIfaceForArchiver.CanCloseArchive(&sc, archiveFileName, force,
                                                     (panel == MainWindow->LeftPanel) ? PANEL_LEFT : PANEL_RIGHT);
        if (force)
            ret = TRUE;
    }
    return ret;
}

BOOL CPluginData::CanViewFile(const char* name)
{
    CALL_STACK_MESSAGE4("CPluginData::CanViewFile(%s) (%s v. %s)", name, DLLName, Version);
    BOOL ret = FALSE;
    if (InitDLL(MainWindow->HWindow)
        /*&& PluginIfaceForViewer.NotEmpty()*/) // unnecessary, because downgrade is not possible and InitDLL ifacy checks
    {
        ret = PluginIfaceForViewer.CanViewFile(name);
    }
    return ret;
}

BOOL CPluginData::ViewFile(const char* name, int left, int top, int width, int height,
                           UINT showCmd, BOOL alwaysOnTop, BOOL returnLock,
                           HANDLE* lock, BOOL* lockOwner, int enumFilesSourceUID,
                           int enumFilesCurrentIndex)
{
    CALL_STACK_MESSAGE13("CPluginData::ViewFile(%s, %d, %d, %d, %d, %u, %d, %d, , , %d, %d) (%s v. %s)",
                         name, left, top, width, height, showCmd, alwaysOnTop, returnLock,
                         enumFilesSourceUID, enumFilesCurrentIndex, DLLName, Version);
    BOOL ret = FALSE;
    if (InitDLL(MainWindow->HWindow)
        /*&& PluginIfaceForViewer.NotEmpty()*/) // unnecessary, because downgrade is not possible and InitDLL ifacy checks
    {
        ret = PluginIfaceForViewer.ViewFile(name, left, top, width, height, showCmd, alwaysOnTop,
                                            returnLock, lock, lockOwner, NULL, enumFilesSourceUID,
                                            enumFilesCurrentIndex);
        if (ret && returnLock && *lock != NULL && *lockOwner)
        { // add handle to 'lock' to HANDLES (disk-cache will want to close it - it will be looking for it)
            HANDLES_ADD(__htEvent, __hoCreateEvent, *lock);
        }
    }
    return ret;
}

CPluginFSInterfaceAbstract*
CPluginData::OpenFS(const char* fsName, int fsNameIndex)
{
    CALL_STACK_MESSAGE5("CPluginData::OpenFS(%s, %d) (%s v. %s)", fsName, fsNameIndex, DLLName, Version);
    CPluginFSInterfaceAbstract* ret = NULL;
    if (InitDLL(MainWindow->HWindow)
        /*&& PluginIfaceForFS.NotEmpty()*/) // unnecessary, because downgrade is not possible and InitDLL ifacy checks
    {
        ret = PluginIfaceForFS.OpenFS(fsName, fsNameIndex);
#ifdef _DEBUG
        if (ret != NULL)
            OpenedFSCounter++; // increase OpenedFSCounter
#endif
    }
    return ret;
}

void CPluginData::ExecuteChangeDriveMenuItem(int panel)
{
    CALL_STACK_MESSAGE4("CPluginData::ExecuteChangeDriveMenuItem(%d) (%s v. %s)", panel, DLLName, Version);
    if (InitDLL(MainWindow->HWindow) &&
        ChDrvMenuFSItemName != NULL         // in case the plug-in cancels the item right at this load
        /*&& PluginIfaceForFS.NotEmpty()*/) // unnecessary, because downgrade is not possible and InitDLL ifacy checks
    {
        PluginIfaceForFS.ExecuteChangeDriveMenuItem(panel);
    }
}

BOOL CPluginData::ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                 CPluginFSInterfaceAbstract* pluginFS,
                                                 const char* pluginFSName, int pluginFSNameIndex,
                                                 BOOL isDetachedFS, BOOL& refreshMenu,
                                                 BOOL& closeMenu, int& postCmd, void*& postCmdParam)
{
    CALL_STACK_MESSAGE9("CPluginData::ChangeDriveMenuItemContextMenu(, %d, %d, %d, , %s, %d, %d, , , ,) (%s v. %s)",
                        panel, x, y, pluginFSName, pluginFSNameIndex, isDetachedFS, DLLName, Version);
    if (InitDLL(parent) &&
        (pluginFS != NULL || ChDrvMenuFSItemName != NULL) // in case the plug-in cancels the item right at this load
        /*&& PluginIfaceForFS.NotEmpty()*/)               // unnecessary, because downgrade is not possible and InitDLL ifacy checks
    {
        return PluginIfaceForFS.ChangeDriveMenuItemContextMenu(parent, panel, x, y, pluginFS,
                                                               pluginFSName, pluginFSNameIndex,
                                                               isDetachedFS, refreshMenu,
                                                               closeMenu, postCmd, postCmdParam);
    }
    return FALSE; // error, so we return "return parameters should be ignored"
}

void CPluginData::EnsureShareExistsOnServer(HWND parent, int panel, const char* server, const char* share)
{
    CALL_STACK_MESSAGE6("CPluginData::EnsureShareExistsOnServer(, %d, %s, %s) (%s v. %s)",
                        panel, server, share, DLLName, Version);
    if (InitDLL(parent, TRUE) &&     // We don't want to report possible errors during loading, EnsureShareExistsOnServer provides only additional information (if not called, almost nothing will happen)
        PluginIsNethood &&           // in case the plugin stops replacing Network (did not call SetPluginIsNethood()) right at this load
        PluginIfaceForFS.NotEmpty()) // PluginIsNethood does not have a connection to PluginIfaceForFS, so we will check it separately
    {
        PluginIfaceForFS.EnsureShareExistsOnServer(panel, server, share);
    }
}

void CPluginData::GetCacheInfo(char* arcCacheTmpPath, BOOL* arcCacheOwnDelete, BOOL* arcCacheCacheCopies)
{
    CALL_STACK_MESSAGE3("CPluginData::GetCacheInfo(, ,) (%s v. %s)", DLLName, Version);
    if (InitDLL(MainWindow->HWindow) &&
        PluginIfaceForArchiver.NotEmpty()) // this part of the condition is probably "always true"
    {
        if (ArcCacheHaveInfo) // we have the settings backed up, we don't have to bother with the plugin anymore
        {
            if (ArcCacheTmpPath != NULL)
                strcpy(arcCacheTmpPath, ArcCacheTmpPath);
            else
                arcCacheTmpPath[0] = 0;
            *arcCacheOwnDelete = ArcCacheOwnDelete;
            *arcCacheCacheCopies = ArcCacheCacheCopies;
        }
        else
        {
            if (!PluginIfaceForArchiver.GetCacheInfo(arcCacheTmpPath, arcCacheOwnDelete, arcCacheCacheCopies))
            {                            // use standard values
                ArcCacheHaveInfo = TRUE; // Default values are set from InitDLL()
                arcCacheTmpPath[0] = 0;
                *arcCacheOwnDelete = FALSE;
                *arcCacheCacheCopies = TRUE;
            }
            else
            {
                int l = (int)strlen(arcCacheTmpPath);
                char* p = NULL;
                if (l > 0)
                {
                    p = (char*)malloc(l + 1);
                    if (p != NULL)
                        strcpy(p, arcCacheTmpPath);
                    else
                        TRACE_E(LOW_MEMORY);
                }
                ArcCacheOwnDelete = *arcCacheOwnDelete; // is set in any case, reason: method IsArchiverAndHaveOwnDelete()
                if (l == 0 || p != NULL)                // When there is a lack of memory, it is not possible to back up the settings -> we will burden the plugin multiple times
                {
                    ArcCacheHaveInfo = TRUE;
                    ArcCacheTmpPath = p;
                    ArcCacheCacheCopies = *arcCacheCacheCopies;
                }
            }
        }
    }
}

void CPluginData::DeleteTmpCopy(const char* fileName, BOOL firstFile)
{
    CALL_STACK_MESSAGE5("CPluginData::DeleteTmpCopy(%s, %d) (%s v. %s)",
                        fileName, firstFile, DLLName, Version);
    if (PluginIfaceForArchiver.NotEmpty())
        PluginIfaceForArchiver.DeleteTmpCopy(fileName, firstFile);
    else
        TRACE_E("Unexpected situation in CPluginData::DeleteTmpCopy(): plugin has not interface for archiver or is not loaded!");
}

BOOL CPluginData::PrematureDeleteTmpCopy(HWND parent, int copiesCount)
{
    CALL_STACK_MESSAGE4("CPluginData::PrematureDeleteTmpCopy(, %d) (%s v. %s)",
                        copiesCount, DLLName, Version);
    if (PluginIfaceForArchiver.NotEmpty())
    {
        return PluginIfaceForArchiver.PrematureDeleteTmpCopy(parent, copiesCount);
    }
    else
    {
        TRACE_E("Unexpected situation in CPluginData::PrematureDeleteTmpCopy(): plugin has not interface for archiver or is not loaded!");
        return FALSE;
    }
}

HIMAGELIST
CPluginData::CreateImageList(BOOL gray)
{
    CIconList* srcList = NULL;
    BOOL deleteSrcList = FALSE;
    if (SupportDynMenuExt)
    {
        if (gray && PluginDynMenuIcons != NULL)
        {
            deleteSrcList = TRUE;
            srcList = new CIconList();
            if (srcList != NULL && !srcList->CreateAsCopy(PluginDynMenuIcons, TRUE))
            {
                delete srcList;
                srcList = NULL;
            }
        }
        else
            srcList = PluginDynMenuIcons;
    }
    else
        srcList = gray ? PluginIconsGray : PluginIcons;
    if (srcList == NULL)
        return NULL; // plugin does not have an assigned bitmap

    HIMAGELIST ret = srcList->GetImageList();
    if (deleteSrcList)
        delete srcList;
    return ret;
}

void CPluginData::HotKeysMerge(TIndirectArray<CPluginMenuItem>* oldMenuItems)
{
    CALL_STACK_MESSAGE1("CPluginData::HotKeysMergeAndTestIntegrity()");
    if (!oldMenuItems->IsGood())
        return;

    // if the old menu had some hot keys "dirty", we will transfer them to the new one according to their ID
    int i;
    for (i = 0; i < oldMenuItems->Count; i++)
    {
        CPluginMenuItem* oldItem = oldMenuItems->At(i);
        if (oldItem->HotKey & HOTKEY_DIRTY)
        {
            // We found a dirty item, we will try to locate it in the new IDs
            int j;
            for (j = 0; j < MenuItems.Count; j++)
            {
                CPluginMenuItem* item = MenuItems[j];
                if (item->ID == oldItem->ID)
                {
                    // transfer the hot key
                    item->HotKey = oldItem->HotKey;
                    break;
                }
            }
        }
    }
}

void CPluginData::HotKeysEnsureIntegrity()
{
    CALL_STACK_MESSAGE1("CPluginData::HotKeysEnsureIntegrity()");

    int i;
    for (i = 0; i < MenuItems.Count; i++)
    {
        CPluginMenuItem* item = MenuItems[i];
        WORD hotKey = HOTKEY_GET(item->HotKey);
        if (hotKey == 0)
            continue;
        BOOL dirty = HOTKEY_GETDIRTY(item->HotKey);
        if (IsSalHotKey(hotKey))
        {
            // The hot key must not belong to the Salamander
            item->HotKey = 0;
            TRACE_E("CPluginData::HotKeysEnsureIntegrity() hot key is already assigned to Salamander; item:" << item->Name);
        }
        else
        {
            // Hotkey must not belong to another plugin
            int pluginIndex;
            int menuItemIndex;
            if (Plugins.FindHotKey(hotKey, TRUE, this, &pluginIndex, &menuItemIndex))
            {
                if (dirty) // if we have a predefined hot key, we prefer to shoot it down at the competitor
                    Plugins.Get(pluginIndex)->MenuItems[menuItemIndex]->HotKey = 0;
                else
                    item->HotKey = 0;
            }
        }
        // Within one menu, the hotkey must not be repeated
        if (item->HotKey != 0)
        {
            int j;
            for (j = 0; j < MenuItems.Count; j++)
            {
                if (j == i)
                    continue;
                CPluginMenuItem* item2 = MenuItems[j];
                if (HOTKEY_GET(item2->HotKey) == hotKey)
                {
                    if (dirty)
                        item2->HotKey = 0;
                    else
                        item->HotKey = 0;
                }
            }
        }
    }
}

void CPluginData::ReleasePluginDynMenuIcons()
{
    if (PluginDynMenuIcons != NULL)
    {
        delete PluginDynMenuIcons;
        PluginDynMenuIcons = NULL;
    }
}

void CPluginData::ReleaseIconOverlays()
{
    if (IconOverlaysCount > 0)
    {
        if (IconOverlays != NULL)
            for (int i = 0; i < IconOverlaysCount; i++)
            {
                if (IconOverlays[i * 3 + 0] != NULL)
                    HANDLES(DestroyIcon(IconOverlays[i * 3 + 0])); // 16x16
                if (IconOverlays[i * 3 + 1] != NULL)
                    HANDLES(DestroyIcon(IconOverlays[i * 3 + 1])); // 32x32
                if (IconOverlays[i * 3 + 2] != NULL)
                    HANDLES(DestroyIcon(IconOverlays[i * 3 + 2])); // 48x48
            }
        else
            TRACE_E("CPluginData::ReleaseIconOverlays(): unexpected situation: IconOverlaysCount is greater then 0 and IconOverlays is NULL.");
        IconOverlaysCount = 0;
    }
    if (IconOverlays != NULL)
    {
        free(IconOverlays);
        IconOverlays = NULL;
    }
}
