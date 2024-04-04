// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "salmoncl.h"

CSalmonSharedMemory* SalmonSharedMemory = NULL;
HANDLE SalmonFileMapping = NULL;
HANDLE HSalmonProcess = NULL;

//****************************************************************************

// WARNING: running from entry point, before RTL initialization, global objects, etc.
// do not call TRACE, HANDLES, RTL, ...

HANDLE GetBugReporterRegistryMutex()
{
    // rights completely open for all processes
    SECURITY_ATTRIBUTES secAttr;
    char secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
    secAttr.nLength = sizeof(secAttr);
    secAttr.bInheritHandle = FALSE;
    secAttr.lpSecurityDescriptor = &secDesc;
    InitializeSecurityDescriptor(secAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(secAttr.lpSecurityDescriptor, TRUE, 0, FALSE);
    // It would be useful to add the SID to the mutex name, as processes with different SIDs run with different HKCU trees
    // but for simplicity, I'll ignore that and the mutex will be truly global
    const char* MUTEX_NAME = "Global\\AltapSalamanderBugReporterRegistryMutex";
    HANDLE hMutex = NOHANDLES(CreateMutex(&secAttr, FALSE, MUTEX_NAME));
    if (hMutex == NULL) // uz create can open an existing mutex, but it may fail, so we will then try open
        hMutex = NOHANDLES(OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME));
    return hMutex;
}

BOOL SalmonGetBugReportUID(DWORD64* uid)
{
    const char* BUG_REPORTER_KEY = "Software\\Open Salamander\\Bug Reporter";
    const char* BUG_REPORTER_UID = "ID";

    // This section is executed when Salamander starts and theoretically can lead to simultaneous reading/writing of the registry
    // so we will protect access with a global mutex
    HANDLE hMutex = GetBugReporterRegistryMutex();
    if (hMutex != NULL)
        WaitForSingleObject(hMutex, INFINITE);
    *uid = 0;
    HKEY hKey;
    LONG res = NOHANDLES(RegOpenKeyEx(HKEY_CURRENT_USER, BUG_REPORTER_KEY, 0, KEY_READ, &hKey));
    if (res == ERROR_SUCCESS)
    {
        // we will try to load old data if it exists
        DWORD gettedType;
        DWORD bufferSize = sizeof(*uid);
        res = RegQueryValueEx(hKey, BUG_REPORTER_UID, 0, &gettedType, (BYTE*)uid, &bufferSize);
        if (res != ERROR_SUCCESS || gettedType != REG_QWORD)
            *uid = 0;
        NOHANDLES(RegCloseKey(hKey));
    }
    // if the UID does not exist yet, we will create it and save it
    if (*uid == 0)
    {
        GUID guid;
        if (CoCreateGuid(&guid) == S_OK)
        {
            // We will not save and send the whole GUID, just half of it, XORed with the other
            DWORD64* dw64 = (DWORD64*)&guid;
            *uid = dw64[0] ^ dw64[1];

            // we will try to save it
            DWORD createType;
            LONG res2 = NOHANDLES(RegCreateKeyEx(HKEY_CURRENT_USER, BUG_REPORTER_KEY, 0, NULL, REG_OPTION_NON_VOLATILE,
                                                 KEY_READ | KEY_WRITE, NULL, &hKey, &createType));
            if (res2 == ERROR_SUCCESS)
            {
                res2 = RegSetValueEx(hKey, BUG_REPORTER_UID, 0, REG_QWORD, (BYTE*)uid, sizeof(*uid));
                if (res2 != ERROR_SUCCESS)
                    *uid = 0; // we want zero on failure
                NOHANDLES(RegCloseKey(hKey));
            }
        }
    }
    if (hMutex != NULL)
    {
        ReleaseMutex(hMutex);
        NOHANDLES(CloseHandle(hMutex));
    }

    return TRUE;
}

BOOL SalmonSharedMemInit(CSalmonSharedMemory* mem)
{
    SECURITY_ATTRIBUTES sa; // Allowing inheritance of the trade to the child process (they can then work directly with our event)
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    RtlFillMemory(mem, sizeof(CSalmonSharedMemory), 0);

    mem->Version = SALMON_SHARED_MEMORY_VERSION;
    // salmon will be launched as a child process with bInheritHandles==TRUE, so it can directly access these handles
    mem->ProcessId = GetCurrentProcessId();
    mem->Process = NOHANDLES(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, TRUE, mem->ProcessId));
    mem->Fire = NOHANDLES(CreateEvent(&sa, TRUE, FALSE, NULL));      // "nonsignaled" state, manual
    mem->Done = NOHANDLES(CreateEvent(&sa, TRUE, FALSE, NULL));      // "nonsignaled" state, manual
    mem->SetSLG = NOHANDLES(CreateEvent(&sa, TRUE, FALSE, NULL));    // "nonsignaled" state, manual
    mem->CheckBugs = NOHANDLES(CreateEvent(&sa, TRUE, FALSE, NULL)); // "nonsignaled" state, manual

    // We are now putting the path for bug reports into LOCAL_APPDATA, where Windows WER typically saves minidumps.
    // We do not create the path immediately, we will take care of it only at the moment of the fall
    if (SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, mem->BugPath) == S_OK)
    {
        int len = lstrlen(mem->BugPath);
        if (len > 0 && mem->BugPath[len - 1] == '\\') // We prefer to verify the backslash at the end of the path
            mem->BugPath[len - 1] = 0;
        lstrcat(mem->BugPath, "\\Open Salamander");
    }

    // Base name for bug reports
    strcpy(mem->BugName, "AS" VERSINFO_SAL_SHORT_VERSION);

    return (mem->Process != NULL && mem->Fire != NULL && mem->Done != NULL && mem->SetSLG != NULL &&
            mem->CheckBugs != NULL && mem->BugPath[0] != 0);
}

void GetStartupSLGName(char* slgName, DWORD slgNameMax)
{
    // we will extract the name SLG from the registry, which will likely be used
    // Further along the Salamander's run, he can come to the choice of another, which will subsequently change
    // This serves only as a default; if the record is not found, we return an empty string
    slgName[0] = 0;

    char keyName[MAX_PATH];
    sprintf(keyName, "%s\\%s", SalamanderConfigurationRoots[0], SALAMANDER_CONFIG_REG);
    HKEY hKey;
    LONG res = NOHANDLES(RegOpenKeyEx(HKEY_CURRENT_USER, keyName, 0, KEY_READ, &hKey));
    if (res == ERROR_SUCCESS)
    {
        DWORD gettedType;
        res = SalRegQueryValueEx(hKey, CONFIG_LANGUAGE_REG, 0, &gettedType, (BYTE*)slgName, &slgNameMax);
        if (res != ERROR_SUCCESS || gettedType != REG_SZ)
            slgName[0] = 0;
        RegCloseKey(hKey);
    }
}

BOOL SalmonStartProcess(const char* fileMappingName) //Configuration.LoadedSLGName
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char cmd[2 * MAX_PATH];
    char rtlDir[MAX_PATH];
    char oldCurDir[MAX_PATH];
    char slgName[MAX_PATH];
#define MAX_ENV_PATH 32766
    char envPATH[MAX_ENV_PATH];
    BOOL ret;

    HSalmonProcess = NULL;

    ret = FALSE;
    GetModuleFileName(NULL, cmd, MAX_PATH);
    *(strrchr(cmd, '\\') + 1) = 0;
    lstrcat(cmd, "utils\\salmon.exe");
    AddDoubleQuotesIfNeeded(cmd, MAX_PATH); // CreateProcess wants the name with spaces in quotes (otherwise it tries various variants, see help)
    GetStartupSLGName(slgName, sizeof(slgName));
    wsprintf(cmd + strlen(cmd), " \"%s\" \"%s\"", fileMappingName, slgName); // slgName can be an empty string if the configuration does not exist
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.wShowWindow = SW_SHOWNORMAL;
    GetModuleFileName(NULL, rtlDir, MAX_PATH);
    *(strrchr(rtlDir, '\\') + 1) = 0;
    GetCurrentDirectory(MAX_PATH, oldCurDir);

    // another attempt to solve the problem before we split SALMON.EXE into EXE+DLL
    // Let's try to expand the PATH environment variable for the child process (SALMON.EXE) with the path to RTL
    if (GetEnvironmentVariable("PATH", envPATH, MAX_ENV_PATH) != 0)
    {
        if (lstrlen(envPATH) + 2 + lstrlen(rtlDir) < MAX_ENV_PATH)
        {
            char newPATH[MAX_ENV_PATH];
            lstrcpy(newPATH, envPATH);
            lstrcat(newPATH, ";");
            lstrcat(newPATH, rtlDir);
            SetEnvironmentVariable("PATH", newPATH);
        }
        else
            envPATH[0] = 0;
    }
    else
        envPATH[0] = 0;

    // Originally, we only passed rtlDir to CreateProcess, but in some combinations with UAC salmon.exe could not be started.
    // because he did not see RTL: https://forum.altap.cz/viewtopic.php?f=2&t=6957&p=26548#p26548
    // Let's also try to set the current directory additionally
    // if that doesn't work, we can try passing NULL to CreateProcess instead of rtlDir, then according to MSDN, it should inherit the current directory from the calling process
    SetCurrentDirectory(rtlDir);
    // EDIT 4/2014: I did several tests with Support@bluesware.ch and chr.mue@gmail.com, see emails
    // I see two possible solutions: we will try to expand the PATH environment variable for the child process to SALRTL.
    // The second option is to split SALMON.EXE into an EXE without RTL and DLL with implicitly linked RTL. Before loading SALMON.DLL
    // It should already be possible to set the current directory from the running SALMON.EXE and load SALMON.DLL at runtime, which might work.
    // ----
    // On my computer, each of the three used path settings works on its own (ENV PATH, SetCurrentDirectory, and the rtlDir parameter in the CreateProcess call).
    if (NOHANDLES(CreateProcess(NULL, cmd, NULL, NULL, TRUE, //bInheritHandles==TRUE, needs to pass event handles!
                                CREATE_DEFAULT_ERROR_MODE | HIGH_PRIORITY_CLASS, NULL,
                                rtlDir, &si, &pi)))
    {
        HSalmonProcess = pi.hProcess;                           // handle the salmon process, we need to be able to detect that it is alive
        AllowSetForegroundWindow(SalGetProcessId(pi.hProcess)); // Let's release the salmon over us
        //NOHANDLES(CloseHandle(pi.hProcess)); // let them leak in peace, they would be the last handles released before the end of the process anyway
        //NOHANDLES(CloseHandle(pi.hThread));
        ret = TRUE;
    }
    SetCurrentDirectory(oldCurDir);
    if (envPATH[0] != 0)
        SetEnvironmentVariable("PATH", envPATH);
    return ret;
}

//BOOL IsSalmonRunning()
//{
//  if (HSalmonProcess != NULL)
//  {
//    DWORD waitRet = WaitForSingleObject(HSalmonProcess, 0);
//    return waitRet == WAIT_TIMEOUT;
//  }
//  return FALSE;
//}

// We want to learn about SEH Exceptions on x64 Windows 7 SP1 and beyond
// http://blog.paulbetts.org/index.php/2010/07/20/the-case-of-the-disappearing-onload-exception-user-mode-callback-exceptions-in-x64/
// http://connect.microsoft.com/VisualStudio/feedback/details/550944/hardware-exceptions-on-x64-machines-are-silently-caught-in-wndproc-messages
// http://support.microsoft.com/kb/976038
void EnableExceptionsOn64()
{
    typedef BOOL(WINAPI * FSetProcessUserModeExceptionPolicy)(DWORD dwFlags);
    typedef BOOL(WINAPI * FGetProcessUserModeExceptionPolicy)(LPDWORD dwFlags);
    typedef BOOL(WINAPI * FIsWow64Process)(HANDLE, PBOOL);
#define PROCESS_CALLBACK_FILTER_ENABLED 0x1

    HINSTANCE hDLL = LoadLibrary("KERNEL32.DLL");
    if (hDLL != NULL)
    {
        FIsWow64Process isWow64 = (FIsWow64Process)GetProcAddress(hDLL, "IsWow64Process");                                                      // Min: XP SP2
        FSetProcessUserModeExceptionPolicy set = (FSetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "SetProcessUserModeExceptionPolicy"); // Min: Vista with hotfix
        FGetProcessUserModeExceptionPolicy get = (FGetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "GetProcessUserModeExceptionPolicy"); // Min: Vista with hotfix
        if (isWow64 != NULL && set != NULL && get != NULL)
        {
            BOOL bIsWow64;
            if (isWow64(GetCurrentProcess(), &bIsWow64) && bIsWow64)
            {
                DWORD dwFlags;
                if (get(&dwFlags))
                    set(dwFlags & ~PROCESS_CALLBACK_FILTER_ENABLED);
            }
        }
        FreeLibrary(hDLL);
    }
}

BOOL SalmonInit()
{
    EnableExceptionsOn64();

    SalmonSharedMemory = NULL;
    char salmonFileMappingName[SALMON_FILEMAPPIN_NAME_SIZE];
    // Allocation of shared space in pagefile.sys
    DWORD ti = (GetTickCount() >> 3) & 0xFFF;
    while (TRUE) // Looking for a unique name for file mapping
    {
        wsprintf(salmonFileMappingName, "Salmon%X", ti++);
        SalmonFileMapping = NOHANDLES(CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, // FIXME_X64 are we passing x86/x64 incompatible data?
                                                        sizeof(CSalmonSharedMemory), salmonFileMappingName));
        if (SalmonFileMapping == NULL || GetLastError() != ERROR_ALREADY_EXISTS)
            break;
        NOHANDLES(CloseHandle(SalmonFileMapping));
    }
    if (SalmonFileMapping != NULL)
    {
        SalmonSharedMemory = (CSalmonSharedMemory*)NOHANDLES(MapViewOfFile(SalmonFileMapping, FILE_MAP_WRITE, 0, 0, 0)); // FIXME_X64 are we passing x86/x64 incompatible data?
        if (SalmonSharedMemory != NULL)
        {
            ZeroMemory(SalmonSharedMemory, sizeof(CSalmonSharedMemory));
            if (SalmonSharedMemInit(SalmonSharedMemory))
            {
                SalmonGetBugReportUID(&SalmonSharedMemory->UID);

                // if we fail to start salmon, we still return TRUE - the problem will be reported later after SLG is loaded
                SalmonStartProcess(salmonFileMappingName);
                return TRUE;
            }
        }
    }
    // A serious (and unexpected) error has occurred, we will block the launch of Salamander, the message will be in English (we don't have slg)
    return FALSE;
}

// info that salmon is not running just needs to be displayed once
static BOOL SalmonNotRunningReported = FALSE;

void SalmonSetSLG(const char* slgName)
{
    ResetEvent(SalmonSharedMemory->Done);

    strcpy(SalmonSharedMemory->SLGName, slgName);
    SetEvent(SalmonSharedMemory->SetSLG);

    // we are waiting for a signal from Salmon that he has processed the task (event Done) or for the case when someone shot down Salmon
    HANDLE arr[2];
    arr[0] = HSalmonProcess;
    arr[1] = SalmonSharedMemory->Done;
    DWORD waitRet = WaitForMultipleObjects(2, arr, FALSE, INFINITE);
    if (waitRet != WAIT_OBJECT_0 + 1) // someone shot down the salmon or something went wrong in communication
    {
        if (!SalmonNotRunningReported && HLanguage != NULL)
        {
            MessageBox(NULL, LoadStr(IDS_SALMON_NOT_RUNNING), SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONERROR);
            SalmonNotRunningReported = TRUE;
        }
    }
    ResetEvent(SalmonSharedMemory->Done);
}

void SalmonCheckBugs()
{
    ResetEvent(SalmonSharedMemory->Done);
    SetEvent(SalmonSharedMemory->CheckBugs);

    // we are waiting for a signal from Salmon that he has processed the task (event Done) or for the case when someone shot down Salmon
    HANDLE arr[2];
    arr[0] = HSalmonProcess;
    arr[1] = SalmonSharedMemory->Done;
    DWORD waitRet = WaitForMultipleObjects(2, arr, FALSE, INFINITE);
    if (waitRet != WAIT_OBJECT_0 + 1) // someone shot down the salmon or something went wrong in communication
    {
        if (!SalmonNotRunningReported && HLanguage != NULL)
        {
            MessageBox(NULL, LoadStr(IDS_SALMON_NOT_RUNNING), SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONERROR);
            SalmonNotRunningReported = TRUE;
        }
    }
    ResetEvent(SalmonSharedMemory->Done);
}

BOOL SalmonFireAndWait(const EXCEPTION_POINTERS* e, char* bugReportPath)
{
    SalmonSharedMemory->ThreadId = GetCurrentThreadId();
    SalmonSharedMemory->ExceptionRecord = *e->ExceptionRecord;
    SalmonSharedMemory->ContextRecord = *e->ContextRecord;
    SetEvent(SalmonSharedMemory->Fire);

    // we are waiting for a signal from Salmon that he has processed the task (event Done) or for the case when someone shot down Salmon
    HANDLE arr[2];
    arr[0] = HSalmonProcess;
    arr[1] = SalmonSharedMemory->Done;
    WaitForMultipleObjects(2, arr, FALSE, INFINITE);
    ResetEvent(SalmonSharedMemory->Done);

    strcpy(bugReportPath, SalmonSharedMemory->BugPath);
    SalPathAppend(bugReportPath, SalmonSharedMemory->BaseName, MAX_PATH);
    strcat(bugReportPath, ".TXT");

    return TRUE;
}