// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "utils.h"
#include "process.h"

BOOL SetDebugPrivilege(BOOL enable);

//************************************************************************************
//
// Global data
//

char* ProcessCache = NULL;
int PCAllocated = 0;
int PCUsed = 0;

#define PC_DELTA 10 // for realocation

//************************************************************************************
//
// EnumProcesses, FindProcess, FreeProcesses
//

#define TH32CS_SNAPPROCESS 0x00000002
#define TH32CS_SNAPMODULE 0x00000008

typedef struct tagPROCESSENTRY32
{
    DWORD dwSize;
    DWORD cntUsage;
    DWORD th32ProcessID; // this process
    DWORD th32DefaultHeapID;
    DWORD th32ModuleID; // associated exe
    DWORD cntThreads;
    DWORD th32ParentProcessID; // this process's parent process
    LONG pcPriClassBase;       // Base priority of process's threads
    DWORD dwFlags;
    CHAR szExeFile[MAX_PATH]; // Path
} PROCESSENTRY32;
typedef PROCESSENTRY32* PPROCESSENTRY32;
typedef PROCESSENTRY32* LPPROCESSENTRY32;

#define MAX_MODULE_NAME32 255

typedef struct tagMODULEENTRY32
{
    DWORD dwSize;
    DWORD th32ModuleID;  // This module
    DWORD th32ProcessID; // owning process
    DWORD GlblcntUsage;  // Global usage count on the module
    DWORD ProccntUsage;  // Module usage count in th32ProcessID's context
    BYTE* modBaseAddr;   // Base address of module in th32ProcessID's context
    DWORD modBaseSize;   // Size in bytes of module starting at modBaseAddr
    HMODULE hModule;     // The hModule of this module in th32ProcessID's context
    char szModule[MAX_MODULE_NAME32 + 1];
    char szExePath[MAX_PATH];
} MODULEENTRY32;
typedef MODULEENTRY32* PMODULEENTRY32;
typedef MODULEENTRY32* LPMODULEENTRY32;

typedef HANDLE(WINAPI* PFNCREATETOOLHELP32SNAPSHOT)(DWORD dwFlags, DWORD th32ProcessID);
typedef BOOL(WINAPI* PFNPROCESS32FIRST)(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
typedef BOOL(WINAPI* PFNPROCESS32NEXT)(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
typedef BOOL(WINAPI* PFNMODULE32FIRST)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
typedef BOOL(WINAPI* PFNMODULE32NEXT)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);

static HMODULE hModKERNEL32 = NULL;
static PFNCREATETOOLHELP32SNAPSHOT CreateToolhelp32Snapshot = NULL;
static PFNPROCESS32FIRST Process32First = NULL;
static PFNPROCESS32NEXT Process32Next = NULL;
static PFNMODULE32FIRST Module32First = NULL;
static PFNMODULE32NEXT Module32Next = NULL;

BOOL BindSnapshotFunctions()
{
    if (hModKERNEL32 == NULL)
    {
        hModKERNEL32 = GetModuleHandle("KERNEL32.DLL");
        CreateToolhelp32Snapshot = (PFNCREATETOOLHELP32SNAPSHOT)GetProcAddress(hModKERNEL32, "CreateToolhelp32Snapshot");
        Process32First = (PFNPROCESS32FIRST)GetProcAddress(hModKERNEL32, "Process32First");
        Process32Next = (PFNPROCESS32NEXT)GetProcAddress(hModKERNEL32, "Process32Next");
        Module32First = (PFNMODULE32FIRST)GetProcAddress(hModKERNEL32, "Module32First");
        Module32Next = (PFNMODULE32NEXT)GetProcAddress(hModKERNEL32, "Module32Next");
    }
    return (CreateToolhelp32Snapshot != NULL &&
            Process32First != NULL && Process32Next != NULL &&
            Module32First != NULL && Module32Next != NULL);
}

BOOL GetProcessModule(DWORD dwPID, const char* exeName,
                      LPMODULEENTRY32 lpMe32, DWORD cbMe32)
{
    BOOL bRet = FALSE;
    BOOL bFound = FALSE;
    HANDLE hModuleSnap = NULL;
    MODULEENTRY32 me32 = {0};

    // Take a snapshot of all modules in the specified process.

    hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPID);
    if (hModuleSnap == INVALID_HANDLE_VALUE)
        return (FALSE);

    // Fill the size of the structure before using it.

    me32.dwSize = sizeof(MODULEENTRY32);

    // Walk the module list of the process, and find the module of
    // interest. Then copy the information to the buffer pointed
    // to by lpMe32 so that it can be returned to the caller.

    if (Module32First(hModuleSnap, &me32))
    {
        do
        {
            if (StrICmp(me32.szModule, exeName) == 0)
            {
                mini_memcpy(lpMe32, &me32, cbMe32);
                bFound = TRUE;
            }
        } while (!bFound && Module32Next(hModuleSnap, &me32));

        bRet = bFound; // if this sets bRet to FALSE, dwModuleID
                       // no longer exists in specified process
    }
    else
        bRet = FALSE; // could not walk module list

    // Do not forget to clean up the snapshot object.

    CloseHandle(hModuleSnap);

    return (bRet);
}

BOOL AddProcess(const char* exeName)
{
    if (PCUsed == PCAllocated)
    {
        char* p = (char*)LocalReAlloc(ProcessCache, (PCAllocated + PC_DELTA) * MAX_PATH,
                                      LMEM_MOVEABLE | LMEM_ZEROINIT);
        if (p != NULL)
        {
            ProcessCache = p;
            PCAllocated += PC_DELTA;
        }
    }
    if (PCUsed < PCAllocated)
    {
        char shortName[MAX_PATH];
        // when conversion to short name fails, ignore item
        if (GetShortPathName(exeName, shortName, MAX_PATH) != 0)
        {
            lstrcpyn(ProcessCache + PCUsed * MAX_PATH, shortName, MAX_PATH);
            PCUsed++;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL EnumProcessesToolHelp32()
{
    HINSTANCE hSnapshot;
    BOOL ret = FALSE;
    DWORD id = 0;

    // bind function to kernel32.dll
    if (!BindSnapshotFunctions())
        return FALSE; // not working under NT4, but who cares...

    SetDebugPrivilege(TRUE);

    //  Take a snapshot of all processes in the system.
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe32;
        MODULEENTRY32 me32;
        BOOL ok;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        ok = Process32First(hSnapshot, &pe32);
        while (ok)
        {
            if (GetProcessModule(pe32.th32ProcessID, pe32.szExeFile, &me32, sizeof(me32)))
            {
                // found module, add it to the cache
                AddProcess(me32.szExePath);
            }

            pe32.dwSize = sizeof(pe32);
            ok = Process32Next(hSnapshot, &pe32);
        }
        if (!ok && GetLastError() == ERROR_NO_MORE_FILES)
            ret = TRUE;
        CloseHandle(hSnapshot);
    }

    SetDebugPrivilege(FALSE);

    return ret;
}

BOOL FindProcess(const char* fileName)
{
    int i;
    for (i = 0; i < PCUsed; i++)
    {
        const char* pName = ProcessCache + i * MAX_PATH;
        if (StrICmp(fileName, pName) == 0)
            return TRUE;
    }
    return FALSE;
}

void FreeProcesses()
{
    if (ProcessCache != NULL)
    {
        LocalFree(ProcessCache);
        ProcessCache = NULL;
    }
    PCAllocated = 0;
    PCUsed = 0;
}

//************************************************************************************
//
// SetDebugPrivilege
//

BOOL SetPrivilege(
    HANDLE hToken,        // token handle
    LPCTSTR Privilege,    // Privilege to enable/disable
    BOOL bEnablePrivilege // TRUE to enable.  FALSE to disable
)
{
    TOKEN_PRIVILEGES tp;
    LUID luid;
    TOKEN_PRIVILEGES tpPrevious;
    DWORD cbPrevious = sizeof(TOKEN_PRIVILEGES);

    if (!LookupPrivilegeValue(NULL, Privilege, &luid))
        return FALSE;

    //
    // first pass.  get current privilege setting
    //
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        &tpPrevious,
        &cbPrevious);

    if (GetLastError() != ERROR_SUCCESS)
        return FALSE;

    //
    // second pass.  set privilege based on previous setting
    //
    tpPrevious.PrivilegeCount = 1;
    tpPrevious.Privileges[0].Luid = luid;

    if (bEnablePrivilege)
    {
        tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
    }
    else
    {
        tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED &
                                                tpPrevious.Privileges[0].Attributes);
    }

    AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tpPrevious,
        cbPrevious,
        NULL,
        NULL);

    if (GetLastError() != ERROR_SUCCESS)
        return FALSE;

    return TRUE;
}

BOOL SetDebugPrivilege(BOOL enable)
{
    HANDLE hToken;

    if (!OpenProcessToken(
            GetCurrentProcess(),
            TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
            &hToken))
        return FALSE;

    // enable SeDebugPrivilege
    if (!SetPrivilege(hToken, SE_DEBUG_NAME, enable))
    {
        // close token handle
        CloseHandle(hToken);

        // indicate failure
        return FALSE;
    }

    CloseHandle(hToken);
    return TRUE;
}

//************************************************************************************
//
// Populate the module list using PSAPI (works on all NT based windows)
//

typedef BOOL(WINAPI* PFNENUMPROCESSES)(DWORD* lpidProcess, DWORD cb, DWORD* cbNeeded);
typedef BOOL(WINAPI* PFNENUMPROCESSMODULES)(HANDLE hProcess, HMODULE* lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD(WINAPI* PFNGETMODULEFILENAMEEXA)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize);

HMODULE hModPSAPI = NULL;
PFNENUMPROCESSES pfnEnumProcesses = NULL;
PFNENUMPROCESSMODULES pfnEnumProcessModules = NULL;
PFNGETMODULEFILENAMEEXA pfnGetModuleFileNameExA = NULL;

BOOL EnumProcessesPSAPI()
{
    DWORD pidArray[1024];
    DWORD cbNeeded;
    DWORD nProcesses;
    unsigned i, j;

    //
    // Hook up to the 3 functions in PSAPI.DLL dynamically.  We can't
    // just call the functions implicitly, since that would make this program
    // require the presence of PSAPI.DLL
    //
    if (hModPSAPI == NULL)
        hModPSAPI = LoadLibrary("PSAPI.DLL");

    if (hModPSAPI == NULL)
        return FALSE;

    pfnEnumProcesses = (PFNENUMPROCESSES)GetProcAddress(hModPSAPI, "EnumProcesses");
    pfnEnumProcessModules = (PFNENUMPROCESSMODULES)GetProcAddress(hModPSAPI, "EnumProcessModules");
    pfnGetModuleFileNameExA = (PFNGETMODULEFILENAMEEXA)GetProcAddress(hModPSAPI, "GetModuleFileNameExA");
    if (pfnEnumProcesses == NULL || pfnEnumProcessModules == NULL || pfnGetModuleFileNameExA == NULL)
        return FALSE;

    // If we get to this point, we've successfully hooked up to the PSAPI APIs

    // EnumProcesses returns an array of process IDs
    if (!pfnEnumProcesses(pidArray, sizeof(pidArray), &cbNeeded))
        return FALSE;

    nProcesses = cbNeeded / sizeof(DWORD); // Determine number of processes

    // Iterate through each process in the array
    for (i = 0; i < nProcesses; i++)
    {
        HMODULE hModuleArray[1024];
        HANDLE hProcess;
        DWORD pid = pidArray[i];
        DWORD nModules;

        // Using the process ID, open up a handle to the process
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProcess == NULL)
            continue;

        // EnumProcessModules returns an array of HMODULEs for the process
        if (!pfnEnumProcessModules(hProcess, hModuleArray, sizeof(hModuleArray), &cbNeeded))
        {
            CloseHandle(hProcess);
            continue;
        }

        // Calculate number of modules in the process
        nModules = cbNeeded / sizeof(hModuleArray[0]);

        // Iterate through each of the process's modules
        for (j = 0; j < nModules; j++)
        {
            HMODULE hModule = hModuleArray[j];
            char szModuleName[MAX_PATH];

            // GetModuleFileNameEx is like GetModuleFileName, but works
            // in other process address spaces
            pfnGetModuleFileNameExA(hProcess, hModule, szModuleName, sizeof(szModuleName));

            if (j == 0) // First module is the EXE.  Just add it to the map
            {
                //pidNameMap.Add( pid, szModuleName );
                AddProcess(szModuleName);
            }
            else // Not the first module.  It's a DLL
            {
            }
        }
        CloseHandle(hProcess); // We're done with this process handle
    }
    return TRUE;
}

//************************************************************************************
//
// EnumProcesses
//

BOOL EnumProcesses()
{
    BOOL ret;

    FreeProcesses();
    ProcessCache = (char*)LocalAlloc(LPTR, PC_DELTA * MAX_PATH);
    if (ProcessCache == NULL)
        return FALSE;
    else
        PCAllocated = PC_DELTA;

    ret = EnumProcessesToolHelp32(); // try ToolHelp32 method
    if (!ret)
        ret = EnumProcessesPSAPI(); // if fails, we try PSAPI

    return ret;
}

// for VS2008
void* memset(void* dest, int val, size_t len)
{
    register unsigned char* ptr = (unsigned char*)dest;
    while (len-- > 0)
        *ptr++ = val;
    return dest;
}
