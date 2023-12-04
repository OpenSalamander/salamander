// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <Tlhelp32.h>

#include "mainwnd.h"
#include "drivelst.h"
#include "plugins.h"
#include "stswnd.h"
#include "editwnd.h"
#include "fileswnd.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "worker.h"
#include "snooper.h"
#include "gui.h"

char BugReportReasonBreak[BUG_REPORT_REASON_MAX]; // pokud je neprazdne, zobrazi se pod "Break" hlaskou

static struct CBugReportReasonBreak_Init
{
    CBugReportReasonBreak_Init() { BugReportReasonBreak[0] = 0; }
} __BugReportReasonBreak_Init;

TIndirectArray<char> GlobalModulesStore(50, 20);
TDirectArray<DWORD> GlobalModulesListTimeStore(50, 20); // x64_OK

//
// ****************************************************************************
// GetProcessorSpeed
//

BOOL GetProcessorSpeed(DWORD* mhz)
{
    __try
    {
        LARGE_INTEGER t1, t2, t3, t4, f;
        unsigned __int64 c1, c2;
        int tp, pp;
        HANDLE t, p;

        if (!QueryPerformanceFrequency(&f))
            return FALSE; // installed hardware doesn't supports a high-resolution performance counter

        // docasne zvedneme prioritu
        t = GetCurrentThread();
        tp = GetThreadPriority(t);
        SetThreadPriority(t, THREAD_PRIORITY_TIME_CRITICAL);
        p = GetCurrentProcess();
        pp = GetPriorityClass(p);
        SetPriorityClass(p, REALTIME_PRIORITY_CLASS);

        // vytahneme countery
        c1 = __rdtsc();
        QueryPerformanceCounter(&t1);

        //    Sleep(50);
        __int64 divider = 20;   // 1 / 20 s = 50 ms
        if (f.QuadPart == 1000) // pouzivaji se GetTickCount, napocitame delsi vzorek
            divider = 2;        // 0.5 s

        QueryPerformanceCounter(&t3);
        t4 = t3;
        while (t4.QuadPart < t3.QuadPart + f.QuadPart / divider)
            QueryPerformanceCounter(&t4);

        c2 = __rdtsc();
        QueryPerformanceCounter(&t2);

        // vratime puvodni prioritu
        SetThreadPriority(t, tp);
        SetPriorityClass(p, pp);

        // vypocitame frekvenci
        *mhz = (DWORD)(f.QuadPart * (c2 - c1) / (t2.QuadPart - t1.QuadPart) / 1000000);

        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }
}

HANDLE WINAPI GetThreadHandleFromID(DWORD dwThreadID, BOOL bInherit)
{
    return OpenThread(THREAD_ALL_ACCESS, bInherit, dwThreadID);
}

struct VS_VERSIONINFO_HEADER
{
    WORD wLength;
    WORD wValueLength;
    WORD wType;
};

// VERSIONINFO by bylo mozne tahat ven pomoci GetFileVersionInfo, ale pouzivame tvrdy pristup do pameti,
// ktery by mohl byt po padu spolehlivejsi
BOOL GetModuleVersion(HINSTANCE hModule, char* buffer, int bufferLen)
{
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
    if (hRes == NULL)
    {
        lstrcpyn(buffer, "unknown", bufferLen);
        return FALSE;
    }

    HGLOBAL hVer = LoadResource(hModule, hRes);
    if (hVer == NULL)
    {
        lstrcpyn(buffer, "unknown", bufferLen);
        return FALSE;
    }

    DWORD resSize = SizeofResource(hModule, hRes);
    const BYTE* first = (BYTE*)LockResource(hVer);
    if (resSize == 0 || first == 0)
    {
        lstrcpyn(buffer, "unknown", bufferLen);
        return FALSE;
    }
    const BYTE* iterator = first + sizeof(VS_VERSIONINFO_HEADER);

    DWORD signature = 0xFEEF04BD;

    while (memcmp(iterator, &signature, 4) != 0)
    {
        iterator++;
        if (iterator + 4 >= first + resSize)
        {
            lstrcpyn(buffer, "unknown", bufferLen);
            return FALSE;
        }
    }

    VS_FIXEDFILEINFO* ffi = (VS_FIXEDFILEINFO*)iterator;

    char buff[200];
    sprintf(buff, "%u.%u.%u.%u", HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS),
            HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS));
    lstrcpyn(buffer, buff, bufferLen);
    return TRUE;
}

#define VER_NT_WORKSTATION 0x0000001
#define VER_NT_DOMAIN_CONTROLLER 0x0000002
#define VER_NT_SERVER 0x0000003

#define VER_SUITE_ENTERPRISE 0x00000002
#define VER_SUITE_DATACENTER 0x00000080
#define VER_SUITE_PERSONAL 0x00000200

struct MonitorEnumProcData
{
    int Index;
    FPrintLine PrintLine;
    void* Param;
};

BOOL CALLBACK
MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    char buf[1024];
    MonitorEnumProcData* data = (MonitorEnumProcData*)dwData;

    MONITORINFOEX mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    RECT r = mi.rcMonitor;
    sprintf(buf, "Monitor %d: (%d,%d)-(%d,%d) %dx%d pixels", data->Index, r.left, r.top, r.right, r.bottom,
            r.right - r.left, r.bottom - r.top);

    // hdcMonitor je NULL, musime vytvorit DC
    HDC hdc = NOHANDLES(CreateDC(mi.szDevice, mi.szDevice, NULL, NULL));
    int planes = GetDeviceCaps(hdc, PLANES);
    int bitsPixels = GetDeviceCaps(hdc, BITSPIXEL);
    NOHANDLES(DeleteDC(hdc));
    sprintf(buf + strlen(buf), ", planes: %d, bits per pixel: %d", planes, bitsPixels);

    if (mi.dwFlags & MONITORINFOF_PRIMARY)
        strcat(buf, ", primary display");
    data->PrintLine(data->Param, buf, TRUE);

    DISPLAY_DEVICE dd;
    dd.cb = sizeof(dd);
    int index = 0;
    while (EnumDisplayDevices(NULL, index, &dd, 0))
    {
        if (StrICmp((const char*)dd.DeviceName, mi.szDevice) == 0)
        {
            sprintf(buf, "Monitor %d Device Name: %s", data->Index, dd.DeviceString);
            data->PrintLine(data->Param, buf, TRUE);
            break;
        }
        index++;
    }

    data->Index++;
    return TRUE;
}

typedef BOOL(WINAPI* FEnumProcessModules)(
    HANDLE hProcess,
    HMODULE* lphModule,
    DWORD cb,
    LPDWORD lpcbNeeded);

typedef DWORD(WINAPI* FGetModuleFileNameExA)(
    HANDLE hProcess,
    HMODULE hModule,
    LPSTR lpFilename,
    DWORD nSize);

#pragma pack(4)
typedef struct _MODULEINFO
{
    LPVOID lpBaseOfDll;
    DWORD SizeOfImage;
    LPVOID EntryPoint;
} MODULEINFO, *LPMODULEINFO;
#pragma pack()

typedef BOOL(WINAPI* FGetModuleInformation)(
    HANDLE hProcess,
    HMODULE hModule,
    LPMODULEINFO lpmodinfo,
    DWORD cb);

BOOL FindInGlobalModulesStore(const char* str, int& index)
{
    if (GlobalModulesStore.Count == 0)
    {
        index = 0;
        return FALSE;
    }

    int l = 0, r = GlobalModulesStore.Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        int res = strcmp(str, GlobalModulesStore[m]);
        if (res == 0) // nalezeno
        {
            index = m;
            return TRUE;
        }
        else if (res < 0)
        {
            if (l == r || l > m - 1) // nenalezeno
            {
                index = m;
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // nenalezeno
            {
                index = m + 1;
                return FALSE;
            }
            l = m + 1;
        }
    }
}

// struktura slouzi k digestum ze stacku jednotlivych threadu
// struktury generujeme pri enumeraci modulu a
struct CModuleInfo
{
    char* Name;        // nazev modulu (bez cesty)
    void* BaseAddress; // base address modulu
    DWORD Size;        // jeho velikost v pameti
};

class CModulesInfo
{
public:
    TDirectArray<CModuleInfo> Modules;

public:
    CModulesInfo() : Modules(1, 10)
    {
    }

    ~CModulesInfo()
    {
        Clean();
    }

    void Clean()
    {
        int i;
        for (i = 0; i < Modules.Count; i++)
        {
            CModuleInfo* module = &Modules[i];
            if (module->Name != NULL)
            {
                free(module->Name);
                module->Name = NULL;
            }
        }
        Modules.DetachMembers();
    }

    BOOL Add(const char* name, void* baseAdderess, DWORD size)
    {
        CModuleInfo module;

        const char* myName = strrchr(name, '\\');
        if (myName != NULL)
            myName++;
        else
            myName = name;
        module.Name = DupStr(myName);
        if (module.Name == NULL)
            return FALSE;
        module.BaseAddress = baseAdderess;
        module.Size = size;
        Modules.Add(module);
        if (!Modules.IsGood())
        {
            Modules.ResetState();
            free(module.Name);
            return FALSE;
        }
        return TRUE;
    }

    CModuleInfo* Find(void* address)
    {
        int i;
        for (i = 0; i < Modules.Count; i++)
        {
            CModuleInfo* module = &Modules[i];
            if (address >= module->BaseAddress && address < (BYTE*)module->BaseAddress + module->Size)
                return module;
        }
        return NULL;
    }
};

#define PRODUCT_ULTIMATE 0x00000001
#define PRODUCT_HOME_BASIC 0x00000002
#define PRODUCT_HOME_PREMIUM 0x00000003
#define PRODUCT_ENTERPRISE 0x00000004
#define PRODUCT_HOME_BASIC_N 0x00000005
#define PRODUCT_BUSINESS 0x00000006
#define PRODUCT_STANDARD_SERVER 0x00000007
#define PRODUCT_DATACENTER_SERVER 0x00000008
#define PRODUCT_SMALLBUSINESS_SERVER 0x00000009
#define PRODUCT_ENTERPRISE_SERVER 0x0000000A
#define PRODUCT_STARTER 0x0000000B
#define PRODUCT_DATACENTER_SERVER_CORE 0x0000000C
#define PRODUCT_STANDARD_SERVER_CORE 0x0000000D
#define PRODUCT_ENTERPRISE_SERVER_CORE 0x0000000E
#define PRODUCT_ENTERPRISE_SERVER_IA64 0x0000000F
#define PRODUCT_BUSINESS_N 0x00000010
#define PRODUCT_WEB_SERVER 0x00000011
#define PRODUCT_CLUSTER_SERVER 0x00000012
#define PRODUCT_HOME_SERVER 0x00000013
#define PRODUCT_STORAGE_EXPRESS_SERVER 0x00000014
#define PRODUCT_STORAGE_STANDARD_SERVER 0x00000015
#define PRODUCT_STORAGE_WORKGROUP_SERVER 0x00000016
#define PRODUCT_STORAGE_ENTERPRISE_SERVER 0x00000017
#define PRODUCT_SERVER_FOR_SMALLBUSINESS 0x00000018
#define PRODUCT_SMALLBUSINESS_SERVER_PREMIUM 0x00000019

#define VER_SUITE_COMPUTE_SERVER 0x00004000
#define VER_SUITE_STORAGE_SERVER 0x00002000
#define VER_SUITE_WH_SERVER 0x00008000

typedef void(WINAPI* PGNSI)(LPSYSTEM_INFO);
#define BUFSIZE 80
#define SM_SERVERR2 89

BOOL PrintSystemVersion(FPrintLine PrintLine, void* param, char* buf, char* avbuf)
{
    static OSVERSIONINFOEX osvi;
    static SYSTEM_INFO si;
    static BOOL bOsVersionInfoEx;
    static PGNSI pGNSI;
    pGNSI = NULL;

    PrintLine(param, "System Version:", FALSE);

    // vyhneme se deprecated warningu (do budoucna zrejme zruseni funkce v SDK)
    typedef BOOL(WINAPI * FDynGetVersionExA)(LPOSVERSIONINFOA lpVersionInformation);
    FDynGetVersionExA DynGetVersionExA = (FDynGetVersionExA)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetVersionExA");
    if (DynGetVersionExA != NULL)
    {
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

        // Try calling GetVersionEx using the OSVERSIONINFOEX structure.
        // If that fails, try using the OSVERSIONINFO structure.
        if ((bOsVersionInfoEx = DynGetVersionExA((LPOSVERSIONINFO)&osvi)) == 0)
        {
            // If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.
            osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            if (!DynGetVersionExA((OSVERSIONINFO*)&osvi))
                return FALSE;
        }

        sprintf(buf, "GetVersionEx Version %u.%u (Build %u)", osvi.dwMajorVersion,
                osvi.dwMinorVersion, osvi.dwBuildNumber & 0xFFFF);
        if (bOsVersionInfoEx)
        {
            sprintf(buf + strlen(buf), " SP %u.%u, %s", osvi.wServicePackMajor,
                    osvi.wServicePackMinor, osvi.szCSDVersion);
        }
        PrintLine(param, buf, TRUE);
    }

    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    SalGetVersionEx(&osvi, FALSE); // !!! SLOW, original GetVersionEx() is deprecated !!!
    sprintf(buf, "SalGetVersionEx Version %u.%u (Build %u)", osvi.dwMajorVersion,
            osvi.dwMinorVersion, osvi.dwBuildNumber & 0xFFFF);
    sprintf(buf + strlen(buf), " SP %u.%u, SMask %u, PType %u, PlatId %u", osvi.wServicePackMajor,
            osvi.wServicePackMinor, osvi.wSuiteMask, osvi.wProductType, osvi.dwPlatformId);
    PrintLine(param, buf, TRUE);

    // Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.
    pGNSI = (PGNSI)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetNativeSystemInfo"); // Min: XP
    if (pGNSI != NULL)
    {
        pGNSI(&si);
        sprintf(buf, "GetNativeSystemInfo architecture ");
        if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            sprintf(buf + strlen(buf), "64-bit (x64)");
        else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
            sprintf(buf + strlen(buf), "32-bit (x86)");
        else
            sprintf(buf + strlen(buf), "%d (unknown)", si.wProcessorArchitecture);
        PrintLine(param, buf, TRUE);
    }

    GetSystemDirectory(avbuf, MAX_PATH);
    sprintf(buf, "System directory: %s", avbuf);
    PrintLine(param, buf, TRUE);
    GetWindowsDirectory(avbuf, MAX_PATH);
    sprintf(buf, "Windows directory: %s", avbuf);
    PrintLine(param, buf, TRUE);

    return TRUE;
}

const char* FindModuleName(char* buf, void* address, BOOL unloadedName = FALSE)
{
    int i;
    for (i = 0; i < GlobalModulesStore.Count; i++)
    {
        if (GlobalModulesStore[i] != NULL)
        {
            char* s = GlobalModulesStore[i];
            if (*s == '0' && *(s + 1) == 'x')
            { // format radku: sprintf(buf, "0x%p (size: 0x%X) (ver: %s): %s", hInstance, iterator->SizeOfImage, ver, szModName);
                char* addr;
                DWORD size;
                sscanf(s, "0x%p (size: 0x%X)", &addr, &size);
                if (addr <= address && address < addr + size)
                {
                    s = strstr(s, "): ");
                    if (s != NULL)
                    {
                        if (unloadedName)
                        {
                            strcpy(buf, "(UNLOADED: ");
                            lstrcpyn(buf + 11, s + 3, MAX_PATH - 12);
                            s = strstr(buf + 11, " (");
                            if (s != NULL)
                                *s = 0; // pokud je modul uvedeny ve formatu: "%s (%s)", name, fullName -- orizneme fullName
                            strcat(buf, ")");
                        }
                        else
                        {
                            strcpy(buf, " (");
                            lstrcpyn(buf + 2, s + 3, MAX_PATH - 3);
                            s = strstr(buf + 2, " (");
                            if (s != NULL)
                                *s = 0; // pokud je modul uvedeny ve formatu: "%s (%s)", name, fullName -- orizneme fullName
                            if (strlen(buf) < MAX_PATH - 15)
                                sprintf(buf + strlen(buf), ": 0x%X", (DWORD)((char*)address - addr));
                            strcat(buf, ")");
                        }
                        return buf;
                    }
                }
            }
        }
    }
    return "";
}

static CModulesInfo ModulesInfo;

void CCallStack::PrintBugReport(EXCEPTION_POINTERS* Exception, DWORD ThreadID, DWORD ShellExtCrashID,
                                FPrintLine PrintLine, void* param)
{
    //  static TDirectArray<DWORD> knownThreads(20, 10);
    if (Exception != NULL)
    {
        ModulesInfo.Clean();
        //    knownThreads.DetachMembers();
    }

    static char buf[1024];
    static char num[50];
    static char avbuf[MAX_PATH];
    static char nameBuf[MAX_PATH];

    strcpy(buf, SALAMANDER_TEXT_VERSION);
#ifdef _DEBUG
    strcat(buf, " (DEBUG)");
#endif //_DEBUG
    PrintLine(param, buf, FALSE);

    __try
    {
        PrintLine(param, "", FALSE);
        if (Exception != NULL)
        {
            PrintLine(param, "Information About Exception:", FALSE);

            const char* message = NULL;
            switch (Exception->ExceptionRecord->ExceptionCode)
            {
            case EXCEPTION_BREAKPOINT:
                message = "debug breakpoint";
                break;

            case EXCEPTION_ACCESS_VIOLATION:
            {
                lstrcpy(avbuf, "access violation");
                if (Exception->ExceptionRecord->NumberParameters >= 2)
                {
                    sprintf(avbuf + strlen(avbuf), ": %s on 0x%p",
                            (Exception->ExceptionRecord->ExceptionInformation[0] == 0) ? "read" : "write",
                            (void*)Exception->ExceptionRecord->ExceptionInformation[1]);
                }
                message = avbuf;
                break;
            }

            case EXCEPTION_STACK_OVERFLOW:
                message = "stack overflow";
                break;
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
                message = "divide by zero";
                break;
            case 0xE06D7363:
                message = "microsoft C++ exception";
                break;
            case OPENSAL_EXCEPTION_RTC:
                message = "open salamander rtc exception";
                break;
            case OPENSAL_EXCEPTION_BREAK:
                message = "open salamander break exception";
                break;
            }
            sprintf(num, "code 0x%X", Exception->ExceptionRecord->ExceptionCode);
            if (message == NULL)
                message = num;

            sprintf(buf, "Exception: %s", message);
            PrintLine(param, buf, TRUE);
            sprintf(buf, "Exception origin: thread ID = 0x%X, execution address = 0x%p%s", ThreadID,
                    Exception->ExceptionRecord->ExceptionAddress,
                    FindModuleName(nameBuf, Exception->ExceptionRecord->ExceptionAddress));
            PrintLine(param, buf, TRUE);
            if (IsDebuggerPresent())
            {
                // x64/Debug verze pod W7 spustene z MSVC debuggeru negenerovala validni minidump (funkce selhala, dump nebyl komplet)
                // mimo MSVC vse slapalo korektne - vlozime si poznamku, ze bezime z debuggeru
                sprintf(buf, "Debugger is present!");
                PrintLine(param, buf, TRUE);
            }
            PrintLine(param, "", FALSE);
        }
        else
        {
#ifndef CALLSTK_DISABLE
            CCallStack* stack = CCallStack::GetThis();
            if (stack != NULL)
            {
                stack->DontSuspend = TRUE;
            }
            CCallStack::ExceptionExists = TRUE;
#endif // CALLSTK_DISABLE
        }

        if (BugReportReasonBreak[0] != 0)
        {
            PrintLine(param, "Break was used.", FALSE);
            PrintLine(param, BugReportReasonBreak, FALSE);
            PrintLine(param, "", FALSE);
        }

        if (ShellExtCrashID != -1)
        {
            sprintf(buf, "Some faulty shell extension throwed exception.\r\nLook at HandleException(GetExceptionInformation(), %u)", ShellExtCrashID);
            PrintLine(param, buf, FALSE);
            PrintLine(param, "", FALSE);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    //#ifdef MSVC_RUNTIME_CHECKS
    __try
    {
        if (RTCErrorDescription[0] != 0)
        {
            PrintLine(param, "RTC Error", FALSE);
            PrintLine(param, RTCErrorDescription, FALSE); // odsazeni je primo ve stringu, je multiline
            PrintLine(param, "", FALSE);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }
    //#endif // MSVC_RUNTIME_CHECKS

#ifndef CALLSTK_DISABLE
    BOOL threadNotFound = FALSE;
    __try
    {
        // vypiseme call-stack threadu, ve kterem je exceptiona
        PrintLine(param, "Call Stacks:", FALSE);
        int excepThreadIndex = -1;
        if (Exception != NULL)
        {
            int i;
            for (i = 0; i < CCallStack::CallStacks.Count; i++)
            {
                CCallStack* stack = CCallStack::CallStacks[i];
                if (ThreadID == stack->ThreadID)
                {
                    sprintf(buf, "Thread with Exception (ID: 0x%X)", stack->ThreadID);
                    char* term = buf + strlen(buf);
                    const char* dll = stack->GetPluginDLLName();
                    if (dll != NULL)
                    {
                        __try
                        {
                            CPluginData* data = Plugins.GetPluginData(dll);
                            if (data != NULL)
                                sprintf(term, ": in %s", data->Name);
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER)
                        {
                            strcpy(term, ": in (exception)");
                        }
                    }
                    PrintLine(param, buf, TRUE);

                    stack->Reset();
                    const char* s;
                    while ((s = stack->GetNextLine()) != NULL)
                    {
                        PrintLine(param, s, TRUE);
                    }
                    if (stack->Skipped > 0)
                    {
                        sprintf(buf, "Number of skipped records: %d", stack->Skipped);
                        PrintLine(param, buf, TRUE);
                    }
                    if (CCallStack::CallStacks.Count > 1)
                        PrintLine(param, "----", TRUE);
                    excepThreadIndex = i;
                    break;
                }
            }
            if (i == CCallStack::CallStacks.Count)
            {
                PrintLine(param, "Thread with Exception was NOT FOUND (it is not registered for call-stacks)!", TRUE);
                PrintLine(param, "It can also be Find thread and Exception was generated during exiting of thread.", TRUE);
                PrintLine(param, "", FALSE);
                threadNotFound = TRUE;
            }
        }
        // vypiseme zbyle call-stacky
        int i;
        for (i = 0; i < CCallStack::CallStacks.Count; i++)
        {
            if (i != excepThreadIndex)
            {
                CCallStack* stack = CCallStack::CallStacks[i];
                sprintf(buf, "Thread ID: 0x%X", stack->ThreadID);
                PrintLine(param, buf, TRUE);
                stack->Reset();
                const char* s;
                while ((s = stack->GetNextLine()) != NULL)
                {
                    PrintLine(param, s, TRUE);
                }
                if (stack->Skipped > 0)
                {
                    sprintf(buf, "Number of skipped records: %d", stack->Skipped);
                    PrintLine(param, buf, TRUE);
                }
                if (i + 1 < CCallStack::CallStacks.Count &&
                    (i + 1 != CCallStack::CallStacks.Count - 1 || excepThreadIndex != CCallStack::CallStacks.Count - 1))
                {
                    PrintLine(param, "----", TRUE);
                }
            }
        }
        PrintLine(param, "", FALSE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }
#endif // CALLSTK_DISABLE

#ifdef _WIN64
    __try
    {
        // vypiseme dostupne registry a pokud to nehodi exception, tak take stack
        if (Exception != NULL && Exception->ContextRecord != NULL)
        {
            static DWORD64 pointersForDump[32 + 64 + 8 + 8]; // stack, eax, ebx, ecx, edx, esi, edi, ebp, eip ;;; esp-32, esp-28, ... esp-4
            ZeroMemory(&pointersForDump, sizeof(pointersForDump));
            PrintLine(param, "Registers:", FALSE);
            PCONTEXT ctxtRec = Exception->ContextRecord;
            DWORD idx = 32 + 64;
            if (ctxtRec->ContextFlags & CONTEXT_INTEGER)
            {
                sprintf(buf, "RAX = 0x%p  RBX = 0x%p", (void*)ctxtRec->Rax, (void*)ctxtRec->Rbx);
                PrintLine(param, buf, TRUE);
                sprintf(buf, "RCX = 0x%p  RDX = 0x%p", (void*)ctxtRec->Rcx, (void*)ctxtRec->Rdx);
                PrintLine(param, buf, TRUE);
                sprintf(buf, "RSI = 0x%p  RDI = 0x%p", (void*)ctxtRec->Rsi, (void*)ctxtRec->Rdi);
                PrintLine(param, buf, TRUE);
                pointersForDump[idx++] = ctxtRec->Rax;
                pointersForDump[idx++] = ctxtRec->Rbx;
                pointersForDump[idx++] = ctxtRec->Rcx;
                pointersForDump[idx++] = ctxtRec->Rdx;
                pointersForDump[idx++] = ctxtRec->Rsi;
                pointersForDump[idx++] = ctxtRec->Rdi;
                pointersForDump[idx++] = ctxtRec->Rbp;
                pointersForDump[idx++] = ctxtRec->Rip;
            }
            if (ctxtRec->ContextFlags & CONTEXT_CONTROL)
            {
                int jj;
                for (jj = 8; jj > 0; jj--)
                    pointersForDump[idx++] = ctxtRec->Rsp - jj * 4;
                sprintf(buf, "RIP = 0x%p  RSP = 0x%p", (void*)ctxtRec->Rip, (void*)ctxtRec->Rsp);
                PrintLine(param, buf, TRUE);
                sprintf(buf, "RBP = 0x%p  RFL = 0x%X", (void*)ctxtRec->Rbp, ctxtRec->EFlags);
                PrintLine(param, buf, TRUE);
            }
            PrintLine(param, "", FALSE);
#ifndef _DEBUG // aby nam to v debug verzi nezastavovalo debuger
            if (ctxtRec->ContextFlags & CONTEXT_CONTROL)
            {
                DWORD64* rsp = (DWORD64*)ctxtRec->Rsp;
                int i = 0;
                int validStackCount = 0;
                while (i < 32 + 64)
                {
                    // esp muze ukazovat nekam do haje - pojistime se
                    __try
                    {
                        pointersForDump[i] = *(rsp + i);
                        i++;
                        validStackCount++;
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        i = 32 + 64; // vypadneme
                    }
                }

                i = 32 + 64 + 8;
                int jj = 0;
                int validStackCount2 = 32 + 64 + 8;
                while (i < 32 + 64 + 8 + 8)
                {
                    // esp muze ukazovat nekam do haje - pojistime se
                    __try
                    {
                        pointersForDump[i] = *(rsp - 8 + jj);
                        i++;
                        jj++;
                        validStackCount2++;
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        i = 32 + 64 + 8 + 8; // vypadneme
                    }
                }

                PrintLine(param, "Stack & Memory Dump:", FALSE);
                i = 0;
                while (i < 32 + 64 + 8 + 8)
                {
                    if (i < 32 + 64 && i < validStackCount || i >= 32 + 64 && i < validStackCount2)
                    {
                        if (i == 32 + 64)
                            PrintLine(param, "---- RAX, RBX, RCX, RDX, RSI, RDI, RBP, RIP:", TRUE);
                        if (i == 32 + 64 + 8)
                            PrintLine(param, "---- RSP-32, RSP-28, RSP-24, RSP-20, RSP-16, RSP-12, RSP-8, RSP-4:", TRUE);
                        __try
                        {
                            if (i < 32 || i >= 32 + 64)
                            {
                                buf[0] = 0;
                                sprintf(buf, "%p: ", (void*)pointersForDump[i]);
                                BYTE* iterator = (BYTE*)(pointersForDump[i]);
                                int j = 0;
                                while (j < 28)
                                {
                                    // mezery zerou misto, zakazeme je a radeji pridame hodnoty
                                    //                if (j > 0)
                                    //                  lstrcat(buf, " ");
                                    __try
                                    {
                                        sprintf(buf + lstrlen(buf), "%02X", *(iterator + j));
                                        j++;
                                    }
                                    __except (EXCEPTION_EXECUTE_HANDLER)
                                    {
                                        lstrcat(buf, "(exception)");
                                        j = 28;
                                    }
                                }
                                PrintLine(param, buf, TRUE);
                                i++;
                            }
                            else
                            {
                                if (i + 4 < validStackCount)
                                {
                                    sprintf(buf, "%p  %p  %p  %p", (void*)pointersForDump[i], (void*)pointersForDump[i + 1],
                                            (void*)pointersForDump[i + 2], (void*)pointersForDump[3 + i]);
                                    PrintLine(param, buf, TRUE);
                                }
                                i += 4;
                            }
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER)
                        {
                            PrintLine(param, "(exception)", TRUE);
                            i = 32 + 64 + 8 + 8; // vypadneme
                        }
                    }
                    else
                        i++;
                }
                PrintLine(param, "", FALSE);
            }
#endif //_DEBUG  // aby nam to v debug verzi nezastavovalo debuger
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }
#else          // _WIN64
    __try
    {
        // vypiseme dostupne registry a pokud to nehodi exception, tak take stack
        if (Exception != NULL && Exception->ContextRecord != NULL)
        {
            static DWORD pointersForDump[32 + 64 + 8 + 8]; // stack, eax, ebx, ecx, edx, esi, edi, ebp, eip ;;; esp-32, esp-28, ... esp-4
            ZeroMemory(&pointersForDump, sizeof(pointersForDump));
            PrintLine(param, "Registers:", FALSE);
            PCONTEXT ctxtRec = Exception->ContextRecord;
            DWORD idx = 32 + 64;
            if (ctxtRec->ContextFlags & CONTEXT_INTEGER)
            {
                sprintf(buf, "EAX = 0x%p  EBX = 0x%p", (void*)ctxtRec->Eax, (void*)ctxtRec->Ebx);
                PrintLine(param, buf, TRUE);
                sprintf(buf, "ECX = 0x%p  EDX = 0x%p", (void*)ctxtRec->Ecx, (void*)ctxtRec->Edx);
                PrintLine(param, buf, TRUE);
                sprintf(buf, "ESI = 0x%p  EDI = 0x%p", (void*)ctxtRec->Esi, (void*)ctxtRec->Edi);
                PrintLine(param, buf, TRUE);
                pointersForDump[idx++] = ctxtRec->Eax;
                pointersForDump[idx++] = ctxtRec->Ebx;
                pointersForDump[idx++] = ctxtRec->Ecx;
                pointersForDump[idx++] = ctxtRec->Edx;
                pointersForDump[idx++] = ctxtRec->Esi;
                pointersForDump[idx++] = ctxtRec->Edi;
                pointersForDump[idx++] = ctxtRec->Ebp;
                pointersForDump[idx++] = ctxtRec->Eip;
            }
            if (ctxtRec->ContextFlags & CONTEXT_CONTROL)
            {
                int jj;
                for (jj = 8; jj > 0; jj--)
                    pointersForDump[idx++] = ctxtRec->Esp - jj * 4;
                sprintf(buf, "EIP = 0x%p  ESP = 0x%p", (void*)ctxtRec->Eip, (void*)ctxtRec->Esp);
                PrintLine(param, buf, TRUE);
                sprintf(buf, "EBP = 0x%p  EFL = 0x%X", (void*)ctxtRec->Ebp, ctxtRec->EFlags);
                PrintLine(param, buf, TRUE);
            }
            PrintLine(param, "", FALSE);
#ifndef _DEBUG // aby nam to v debug verzi nezastavovalo debuger
            if (ctxtRec->ContextFlags & CONTEXT_CONTROL)
            {
                DWORD* esp = (DWORD*)ctxtRec->Esp;
                int i = 0;
                int validStackCount = 0;
                while (i < 32 + 64)
                {
                    // esp muze ukazovat nekam do haje - pojistime se
                    __try
                    {
                        pointersForDump[i] = *(esp + i);
                        i++;
                        validStackCount++;
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        i = 32 + 64; // vypadneme
                    }
                }

                i = 32 + 64 + 8;
                int jj = 0;
                int validStackCount2 = 32 + 64 + 8;
                while (i < 32 + 64 + 8 + 8)
                {
                    // esp muze ukazovat nekam do haje - pojistime se
                    __try
                    {
                        pointersForDump[i] = *(esp - 8 + jj);
                        i++;
                        jj++;
                        validStackCount2++;
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        i = 32 + 64 + 8 + 8; // vypadneme
                    }
                }

                PrintLine(param, "Stack & Memory Dump:", FALSE);
                i = 0;
                while (i < 32 + 64 + 8 + 8)
                {
                    if (i < 32 + 64 && i < validStackCount || i >= 32 + 64 && i < validStackCount2)
                    {
                        if (i == 32 + 64)
                            PrintLine(param, "---- EAX, EBX, ECX, EDX, ESI, EDI, EBP, EIP:", TRUE);
                        if (i == 32 + 64 + 8)
                            PrintLine(param, "---- ESP-32, ESP-28, ESP-24, ESP-20, ESP-16, ESP-12, ESP-8, ESP-4:", TRUE);
                        __try
                        {
                            if (i < 32 || i >= 32 + 64)
                            {
                                buf[0] = 0;
                                sprintf(buf, "%p: ", (void*)pointersForDump[i]);
                                BYTE* iterator = (BYTE*)(pointersForDump[i]);
                                int j = 0;
                                while (j < 28)
                                {
                                    // mezery zerou misto, zakazeme je a radeji pridame hodnoty
                                    //                if (j > 0)
                                    //                  lstrcat(buf, " ");
                                    __try
                                    {
                                        sprintf(buf + lstrlen(buf), "%02X", *(iterator + j));
                                        j++;
                                    }
                                    __except (EXCEPTION_EXECUTE_HANDLER)
                                    {
                                        lstrcat(buf, "(exception)");
                                        j = 28;
                                    }
                                }
                                PrintLine(param, buf, TRUE);
                                i++;
                            }
                            else
                            {
                                if (i + 4 < validStackCount)
                                {
                                    sprintf(buf, "%p  %p  %p  %p", (void*)pointersForDump[i], (void*)pointersForDump[i + 1],
                                            (void*)pointersForDump[i + 2], (void*)pointersForDump[3 + i]);
                                    PrintLine(param, buf, TRUE);
                                }
                                i += 4;
                            }
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER)
                        {
                            PrintLine(param, "(exception)", TRUE);
                            i = 32 + 64 + 8 + 8; // vypadneme
                        }
                    }
                    else
                        i++;
                }
                PrintLine(param, "", FALSE);
            }
#endif         //_DEBUG  // aby nam to v debug verzi nezastavovalo debuger
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }
#endif         // _WIN64

    __try
    {
        if (MainThreadID == ThreadID && MessagesKeeper.GetCount() > 0)
        {
            PrintLine(param, "Main Thread Messages:", FALSE);
            int count = MessagesKeeper.GetCount();
            int i;
            for (i = 0; i < count; i++)
            {
                MessagesKeeper.Print(buf, 500, i);
                PrintLine(param, buf, TRUE);
            }
            PrintLine(param, "", FALSE);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        if (Exception != NULL)
        {
            PrintLine(param, "Window Handles:", FALSE);

            lstrcpy(buf, "MainWindow=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "LeftPanel=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->LeftPanel);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "LeftFilesBox=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->LeftPanel->ListBox);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "LeftDirectoryLine=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->LeftPanel->DirectoryLine);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "LeftToolBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->LeftPanel->DirectoryLine->ToolBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "LeftStatusLine=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->LeftPanel->StatusLine);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "RightPanel=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->RightPanel);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "RightFilesBox=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->RightPanel->ListBox);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "RightDirectoryLine=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->RightPanel->DirectoryLine);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "RightToolBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->RightPanel->DirectoryLine->ToolBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "RightStatusLine=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->RightPanel->StatusLine);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "TopRebar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->HTopRebar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "MenuBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->MenuBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "TopToolBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->TopToolBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "MiddleToolBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->MiddleToolBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "UMToolBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->UMToolBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "HPToolBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->HPToolBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "PluginsBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->PluginsBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "DriveBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->DriveBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "DriveBar2=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->DriveBar2);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "BottomToolBar=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->BottomToolBar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "EditWindow=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->EditWindow);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            lstrcpy(buf, "EditLine=");
            __try
            {
                sprintf(buf + lstrlen(buf), "0x%p", MainWindow->EditWindow->GetEditLine());
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                lstrcat(buf, "(exception)");
            }
            PrintLine(param, buf, TRUE);

            PrintLine(param, "", FALSE);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        if (Exception == NULL)
        {
#ifndef CALLSTK_DISABLE
            CCallStack::ExceptionExists = FALSE;
            CCallStack* stack = CCallStack::GetThis();
            if (stack != NULL)
            {
                stack->DontSuspend = FALSE;
            }
#endif // CALLSTK_DISABLE
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        PrintLine(param, "Global Variables:", FALSE);
        sprintf(buf, "StopRefresh = %d", StopRefresh);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "SnooperSuspended = %d", SnooperSuspended);
        PrintLine(param, buf, TRUE);
        if (MainWindow != NULL)
        {
            sprintf(buf, "ActivateSuspMode = %d", MainWindow->ActivateSuspMode);
            PrintLine(param, buf, TRUE);
        }
        sprintf(buf, "FullRowSelect = %d", Configuration.FullRowSelect);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "OnlyOneInstance = %d", Configuration.OnlyOneInstance);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "StatusArea = %d", Configuration.StatusArea);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "UseRecycleBin = %d", Configuration.UseRecycleBin);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "CnfrmDragDrop = %d", Configuration.CnfrmDragDrop);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "AlwaysOnTop = %d", Configuration.AlwaysOnTop);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "SortUsesLocale = %d", Configuration.SortUsesLocale);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "SortDetectNumbers = %d", Configuration.SortDetectNumbers);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "SortNewerOnTop = %d", Configuration.SortNewerOnTop);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "SortDirsByName = %d", Configuration.SortDirsByName);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "SortDirsByExt = %d", Configuration.SortDirsByExt);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "EHasOccured = %d, %d, %d, %d, %d, %d, %d, %d", MenuNewExceptionHasOccured,
                FGIExceptionHasOccured, ICExceptionHasOccured, QCMExceptionHasOccured,
                OCUExceptionHasOccured, GTDExceptionHasOccured, SHLExceptionHasOccured,
                RelExceptionHasOccured);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "ConfigWasImported = %d", Configuration.ConfigWasImported);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "UseSalOpen = %d", Configuration.UseSalOpen);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "NetwareFastDirMove = %d", Configuration.NetwareFastDirMove);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "UseAsyncCopyAlg = %d", Configuration.UseAsyncCopyAlg);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "ReloadEnvVariables = %d", Configuration.ReloadEnvVariables);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "AutoSave = %d", Configuration.AutoSave);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "IfPathIsInaccessibleGoTo (isMyDocs = %d) = %s", Configuration.IfPathIsInaccessibleGoToIsMyDocs, Configuration.IfPathIsInaccessibleGoTo);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "NoDrives = 0x%08X", SystemPolicies.GetNoDrives());
        PrintLine(param, buf, TRUE);
        sprintf(buf, "NoFind = %u", SystemPolicies.GetNoFind());
        PrintLine(param, buf, TRUE);
        sprintf(buf, "VisibleDrives = 0x%08X", Configuration.VisibleDrives);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "EnableCustomIconOverlays = %d", Configuration.EnableCustomIconOverlays);
        PrintLine(param, buf, TRUE);
        _snprintf_s(buf, _TRUNCATE, "DisabledCustomIconOverlays = %s", Configuration.DisabledCustomIconOverlays);
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        PrintLine(param, "Panels:", FALSE);
        BOOL printEOL = TRUE;
        int panelNum;
        for (panelNum = 0; MainWindow != NULL && panelNum < 2; panelNum++)
        {
            CFilesWindow* panel = panelNum == 0 ? MainWindow->LeftPanel : MainWindow->RightPanel;
            if (panel != NULL)
            {
                sprintf(buf, "%s panel (%s):", panelNum == 0 ? "Left" : "Right",
                        panel == MainWindow->GetActivePanel() ? "source" : "target");
                PrintLine(param, buf, TRUE);
                switch (panel->GetPanelType())
                {
                case ptDisk:
                {
                    sprintf(buf, "Path = %s", panel->GetPath());
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "PanelType = %d", panel->GetPanelType());
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "Dirs = %d", panel->Dirs != NULL ? panel->Dirs->Count : -1);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "Files = %d", panel->Files != NULL ? panel->Files->Count : -1);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "SelectedCount = %d", panel->SelectedCount);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "FileBasedCompression = %d", panel->FileBasedCompression);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "FileBasedEncryption = %d", panel->FileBasedEncryption);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "SupportACLS = %d", panel->SupportACLS);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "Network Drive = %d", panel->GetNetworkDrive());
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "DeviceNotification = 0x%p", panel->DeviceNotification);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "UseSystemIcons = %d", panel->UseSystemIcons);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "UseThumbnails = %d", panel->UseThumbnails);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "SuppressAutoRefresh = %d", panel->GetSuppressAutoRefresh());
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "MonitorChanges = %d", panel->GetMonitorChanges());
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "IconCacheValid = %d", panel->IconCacheValid);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "AutomaticRefresh = %d", panel->AutomaticRefresh);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "ViewMode = %d", panel->GetViewMode());
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "SortType = %d", panel->SortType);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "ReverseSort = %d", panel->ReverseSort);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "SortedWithRegSet = %d", panel->SortedWithRegSet);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "SortedWithDetectNum = %d", panel->SortedWithDetectNum);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "NextFocusName = %s", panel->NextFocusName);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "FocusFirstNewItem = %d", panel->FocusFirstNewItem);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "HiddenDirsFilesReason = %u", panel->HiddenDirsFilesReason);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "HiddenDirsCount = %d", panel->HiddenDirsCount);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "HiddenFilesCount = %d", panel->HiddenFilesCount);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "FilterEnabled = %d", panel->FilterEnabled);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "QuickSearchMode = %d", panel->QuickSearchMode);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "FocusedIndex = %d", panel->FocusedIndex);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "FocusVisible = %d", panel->FocusVisible);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "TrackingSingleClick = %d", panel->TrackingSingleClick);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "UserWorkedOnThisPath = %d", panel->UserWorkedOnThisPath);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "StopThumbnailLoading = %d", panel->StopThumbnailLoading);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "EnumFileNamesSourceUID = %d", panel->EnumFileNamesSourceUID);
                    PrintLine(param, buf, TRUE);
                    break;
                }

                case ptZIPArchive:
                {
                    sprintf(buf, "Archive = %s", panel->GetZIPArchive());
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "ArcPath = %s", panel->GetZIPPath());
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "Dirs = %d", panel->Dirs != NULL ? panel->Dirs->Count : -1);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "Files = %d", panel->Files != NULL ? panel->Files->Count : -1);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "SelectedCount = %d", panel->SelectedCount);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "AssocUsed = %d", panel->AssocUsed);
                    PrintLine(param, buf, TRUE);
                    break;
                }

                case ptPluginFS:
                {
                    sprintf(buf, "FSPath = %s:", panel->GetPluginFS()->GetPluginFSName());
                    __try
                    {
                        if (!panel->GetPluginFS()->NotEmpty() ||
                            !panel->GetPluginFS()->GetCurrentPath(buf + lstrlen(buf)))
                        {
                            lstrcat(buf, !panel->GetPluginFS()->NotEmpty() ? "(empty)" : "(error)");
                        }
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        lstrcat(buf, "(exception)");
                    }
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "Plugin DLL is ");
                    __try
                    {
                        if (panel->GetPluginFS()->NotEmpty())
                        {
                            CPluginData* data = Plugins.GetPluginData(panel->GetPluginFS()->GetPluginInterfaceForFS()->GetInterface());
                            if (data != NULL)
                                sprintf(buf + lstrlen(buf), "%s v. %s", data->DLLName, data->Version);
                            else
                                lstrcat(buf, "(error)");
                        }
                        else
                            lstrcat(buf, "(empty)");
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        lstrcat(buf, "(exception)");
                    }
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "Dirs = %d", panel->Dirs != NULL ? panel->Dirs->Count : -1);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "Files = %d", panel->Files != NULL ? panel->Files->Count : -1);
                    PrintLine(param, buf, TRUE);
                    sprintf(buf, "SelectedCount = %d", panel->SelectedCount);
                    PrintLine(param, buf, TRUE);
                    break;
                }

                default:
                {
                    sprintf(buf, "Unknown panel type: %d", panel->GetPanelType());
                    PrintLine(param, buf, TRUE);
                    break;
                }
                }
                PrintLine(param, "", FALSE);
                printEOL = FALSE;
            }
        }
        if (printEOL)
            PrintLine(param, "", FALSE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    BOOL* isModuleLoaded = NULL;
    int globalModulesStoreCount = 0;
    __try
    {
        PrintLine(param, "Modules:", FALSE);
        globalModulesStoreCount = GlobalModulesStore.Count; // pro pripad zmeny GlobalModulesStore.Count udelame kopii
        if (globalModulesStoreCount > 0)
            isModuleLoaded = (BOOL*)malloc(sizeof(BOOL) * globalModulesStoreCount);
        if (isModuleLoaded != NULL)
            memset(isModuleLoaded, 0, sizeof(BOOL) * globalModulesStoreCount);
        int foundIndex;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
        if (snap != (HANDLE)-1)
        {
            static MODULEENTRY32 module;
            module.dwSize = sizeof(module);
            if (Module32First(snap, &module))
            {
                do
                {
                    if ((int)module.dwSize >= ((char*)(&(module.szExePath)) - (char*)(&(module.dwSize))))
                    {
                        static char modulePath[MAX_PATH];
                        if (module.dwSize == sizeof(module))
                        {
                            lstrcpy(modulePath, module.szExePath);
                        }
                        else
                            lstrcpy(modulePath, "(unknown)");
                        lstrcpy(nameBuf, module.szModule);
                        static char ver[100];
                        GetModuleVersion((HINSTANCE)module.modBaseAddr, ver, 100);
                        sprintf(buf, "0x%p (size: 0x%X) (ver: %s): %s (%s)", module.modBaseAddr, module.modBaseSize, ver,
                                nameBuf, modulePath);
                        if (isModuleLoaded != NULL && FindInGlobalModulesStore(buf, foundIndex) &&
                            foundIndex < globalModulesStoreCount)
                        {
                            isModuleLoaded[foundIndex] = TRUE;
                        }
                        if (Exception != NULL)
                            ModulesInfo.Add(modulePath, module.modBaseAddr, module.modBaseSize);
                    }
                    else
                        sprintf(buf, "unknown module (dwSize = %u)", module.dwSize);
                    PrintLine(param, buf, TRUE);
                    module.dwSize = sizeof(module);
                } while (Module32Next(snap, &module));
            }
            NOHANDLES(CloseHandle(snap));
        }
        PrintLine(param, "", FALSE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        PrintLine(param, "Unloaded Modules:", FALSE);
        BOOL unloadedModuleExist = FALSE;
        int i;
        for (i = 0; i < GlobalModulesStore.Count; i++)
        {
            if (isModuleLoaded == NULL || i >= globalModulesStoreCount || !isModuleLoaded[i])
            {
                if (GlobalModulesStore[i] != NULL)
                {
                    sprintf(buf, "(%u ms) %s", GlobalModulesListTimeStore[i] - SalamanderStartTime, GlobalModulesStore[i]);
                    PrintLine(param, buf, TRUE);
                    unloadedModuleExist = TRUE;
                }
                else
                    PrintLine(param, "(null)", TRUE);
            }
        }
        if (isModuleLoaded != NULL)
            free(isModuleLoaded);
        if (!unloadedModuleExist)
        {
            sprintf(buf, "NONE");
            PrintLine(param, buf, TRUE);
        }
        PrintLine(param, "", FALSE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        PrintLine(param, "Plugins:", FALSE);
        int pluginIndex = 0;
        CPluginData* plugin;
        while ((plugin = Plugins.Get(pluginIndex++)) != NULL)
        {
            sprintf(buf, "%s: %s v. %s", plugin->Name, plugin->DLLName, plugin->Version);
            if (plugin->DLL != NULL)
                sprintf(buf + strlen(buf), ", loaded (0x%p)", plugin->DLL);
            else
                sprintf(buf + strlen(buf), ", not loaded");
            PrintLine(param, buf, TRUE);
        }
        PrintLine(param, "", FALSE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        // vypiseme aktualni casy
        static SYSTEMTIME st;
        GetLocalTime(&st);
        sprintf(buf, "Bug Report Time: %u.%u.%u - %02u:%02u:%02u",
                st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
        PrintLine(param, buf, FALSE);
        DWORD ti = SalamanderExceptionTime - SalamanderStartTime;
        sprintf(buf, "Salamander Started Before: %u:%02u:%02u:%02u.%03u",
                ti / (24 * 60 * 60 * 1000), (ti / (60 * 60 * 1000)) % 24,
                (ti / (60 * 1000)) % 60, (ti / (1000)) % 60, ti % 1000);
        PrintLine(param, buf, FALSE);
        ti = GetTickCount();
        sprintf(buf, "System Started Before: %u:%02u:%02u:%02u",
                ti / (24 * 60 * 60 * 1000), (ti / (60 * 60 * 1000)) % 24,
                (ti / (60 * 1000)) % 60, (ti / (1000)) % 60);
        PrintLine(param, buf, FALSE);
        PrintLine(param, "", FALSE);

        lstrcpy(buf, "Module Name: ");
        GetModuleFileName(HInstance, buf + strlen(buf), 1000);
        PrintLine(param, buf, FALSE);

        const char* cmdline = GetCommandLine();
        _snprintf_s(buf, _TRUNCATE, "Command Line: %s", cmdline);
        PrintLine(param, buf, FALSE);

        sprintf(buf, "Built: %s", __DATE__ ", " __TIME__);
        PrintLine(param, buf, FALSE);

        LCID lcid = GetThreadLocale();

        lstrcpy(buf, "Country: ");
        GetLocaleInfo(lcid, LOCALE_ICOUNTRY, buf + strlen(buf), 100);
        lstrcat(buf, " (");
        GetLocaleInfo(lcid, LOCALE_SENGCOUNTRY, buf + strlen(buf), 100);
        lstrcat(buf, ")");
        PrintLine(param, buf, FALSE);

        lstrcpy(buf, "Language: ");
        GetLocaleInfo(lcid, LOCALE_ILANGUAGE, buf + strlen(buf), 100);
        lstrcat(buf, " (");
        GetLocaleInfo(lcid, LOCALE_SENGLANGUAGE, buf + strlen(buf), 100);
        lstrcat(buf, ")");
        PrintLine(param, buf, FALSE);

        lstrcpy(buf, "Code Page: ");
        GetLocaleInfo(lcid, LOCALE_IDEFAULTANSICODEPAGE, buf + strlen(buf), 100);
        PrintLine(param, buf, FALSE);

        PrintLine(param, "", FALSE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        PrintSystemVersion(PrintLine, param, buf, avbuf);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    HKEY hKey;
    __try
    {
        if (NOHANDLES(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Internet Explorer", 0,
                                   KEY_READ, &hKey)) == ERROR_SUCCESS)
        {
            static char iver[50];
            static char build[50];
            static char version[50];
            iver[0] = 0;
            build[0] = 0;
            version[0] = 0;
            GetValueAux(NULL, hKey, "IVer", REG_SZ, iver, 50);
            GetValueAux(NULL, hKey, "Build", REG_SZ, build, 50);
            GetValueAux(NULL, hKey, "Version", REG_SZ, version, 50);

            if (iver[0] != 0 || build[0] != 0 || version[0] != 0)
            {
                lstrcpy(buf, "IE ");
                if (version[0] != 0)
                {
                    lstrcat(buf, "Version: ");
                    lstrcat(buf, version);
                    lstrcat(buf, " ");
                }
                if (build[0] != 0)
                {
                    lstrcat(buf, "Build: ");
                    lstrcat(buf, build);
                    lstrcat(buf, " ");
                }
                if (iver[0] != 0)
                {
                    lstrcat(buf, "IVer: ");
                    lstrcat(buf, iver);
                }
                PrintLine(param, buf, TRUE);
            }

            NOHANDLES(RegCloseKey(hKey));
        }

        sprintf(buf, "COMCTL32.DLL Version: %u.%u", CCVerMajor, CCVerMinor);
        PrintLine(param, buf, TRUE);

        if (NOHANDLES(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                   "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                                   KEY_READ, &hKey)) == ERROR_SUCCESS)
        {
            char myBuff[100];

            if (GetValueAux(NULL, hKey, "ProductName", REG_SZ, myBuff, 100))
            {
                sprintf(buf, "ProductName (from registry): %s", myBuff);
                PrintLine(param, buf, TRUE);
            }
            if (GetValueAux(NULL, hKey, "CurrentVersion", REG_SZ, myBuff, 100))
            {
                sprintf(buf, "CurrentVersion (from registry): %s", myBuff);
                PrintLine(param, buf, TRUE);
            }
            NOHANDLES(RegCloseKey(hKey));
        }

        // zkusime najit hlavni okno litestepu
#define LM_GETREVID 9265
#define LS_BUFSIZE 2048
        HWND hLiteStepWnd = FindWindow("TApplication", "LiteStep");
        if (hLiteStepWnd != NULL)
        {
            static char buffer[LS_BUFSIZE];
            buffer[0] = 0;

            lstrcpy(buf, "LiteStep");

            int msgflags = 0;
            msgflags |= (LS_BUFSIZE << 4);

            // zkusime vytahnout verzi
            if (SendMessage(hLiteStepWnd, LM_GETREVID, msgflags, (LPARAM)buffer))
            {
                // LiteStepove tam valej hromadu radku - odrizneme si pouze prvni
                char* p = buffer;
                while (*p != 0 && *p != '\n')
                    p++;
                *p = 0;
                lstrcat(buf, " ");
                lstrcat(buf, buffer);
                // Do whatever
            }
            else
            {
                // nepovedlo se - tak alespon napiseme, ze bezi
                lstrcat(buf, " is present");
            }
            PrintLine(param, buf, TRUE);
        }

        PrintLine(param, "", FALSE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        // OTHER INFORMATION
        PrintLine(param, "Other Information:", FALSE);

        strcpy(buf, "User is Admin: ");
        strcat(buf, IsUserAdmin() ? "yes" : "no");
        PrintLine(param, buf, TRUE);

        DWORD integrityLevel;
        if (GetProcessIntegrityLevel(&integrityLevel))
        {
            sprintf(buf, "Integrity Level: 0x%X", integrityLevel);
            PrintLine(param, buf, TRUE);
        }

        strcpy(buf, "Terminal Services: ");
        strcat(buf, IsRemoteSession() ? "running in a remote session" : "running on the console");
        PrintLine(param, buf, TRUE);

        PrintLine(param, "", FALSE);

        // HARDWARE INFORMATION
        static SYSTEM_INFO si;
        GetSystemInfo(&si);
        PrintLine(param, "Hardware Information:", FALSE);
        sprintf(buf, "OEM ID: 0x%X", si.dwOemId);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "Number of Processors: %u", si.dwNumberOfProcessors);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "Processor Type: %u", si.dwProcessorType);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "Processor Architecture: %u", si.wProcessorArchitecture);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "Processor Level: %u", si.wProcessorLevel);
        PrintLine(param, buf, TRUE);
        if (NOHANDLES(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Hardware\\Description\\System\\CentralProcessor\\0", 0,
                                   KEY_READ, &hKey)) == ERROR_SUCCESS)
        {
            static char processorName[200];
            static char vendorName[200];
            DWORD mhz;

            if (!GetValueAux(NULL, hKey, "ProcessorNameString", REG_SZ, processorName, 200))
                if (!GetValueAux(NULL, hKey, "Identifier", REG_SZ, processorName, 200)) // pod W2K+ asi zbytecny
                    processorName[0] = 0;
            if (!GetValueAux(NULL, hKey, "VendorIdentifier", REG_SZ, vendorName, 200))
                vendorName[0] = 0;
            if (!GetValueAux(NULL, hKey, "~MHz", REG_DWORD, &mhz, sizeof(DWORD)))
            {
                if (!GetProcessorSpeed(&mhz))
                    mhz = 0;
            }
            if (vendorName[0] != 0)
                sprintf(buf, "Processor Vendor Name: %s", vendorName);
            PrintLine(param, buf, TRUE);
            if (processorName[0] != 0)
            {
                char* ss = processorName;
                while (*ss == ' ')
                    ss++; // Intel vklada pred nazev procesoru mezery, aby to lepe vypadalo v System okne z control panelu
                sprintf(buf, "Processor Name: %s", ss);
                PrintLine(param, buf, TRUE);
            }
            if (mhz != 0)
                sprintf(buf, "Processor Speed: ~%u MHz", mhz);
            PrintLine(param, buf, TRUE);

            NOHANDLES(RegCloseKey(hKey));
        }

        sprintf(buf, "Page size: %u", si.dwPageSize);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "Minimum app address: 0x%p", si.lpMinimumApplicationAddress);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "Maximum app address: 0x%p", si.lpMaximumApplicationAddress);
        PrintLine(param, buf, TRUE);
        sprintf(buf, "Active processor mask: 0x%IX", si.dwActiveProcessorMask);
        PrintLine(param, buf, TRUE);
        BOOL fResult = GetSystemMetrics(SM_MOUSEPRESENT);
        if (fResult)
            sprintf(buf, "Mouse is installed");
        else
            sprintf(buf, "No mouse installed");
        PrintLine(param, buf, TRUE);

        static MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms) != 0)
        {
            sprintf(buf, "Total Physical Memory: %I64u KB", ms.ullTotalPhys / 1024);
            PrintLine(param, buf, TRUE);
            sprintf(buf, "Free Physical Memory: %I64u KB", ms.ullAvailPhys / 1024);
            PrintLine(param, buf, TRUE);
            sprintf(buf, "Total Virtual Memory: %I64u KB", ms.ullTotalVirtual / 1024);
            PrintLine(param, buf, TRUE);
            sprintf(buf, "Free Virtual Memory: %I64u KB", ms.ullAvailVirtual / 1024);
            PrintLine(param, buf, TRUE);
        }

        if (NOHANDLES(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Hardware\\Description\\System", 0,
                                   KEY_READ, &hKey)) == ERROR_SUCCESS)
        {
            static char bios[200];

            DWORD bufferSize = 200;
            bios[0] = 0;
            SalRegQueryValueEx(hKey, "SystemBiosVersion", NULL, NULL, (BYTE*)bios, &bufferSize);
            bios[_countof(bios) - 1] = 0; // aspon zakoncime buffer nulou
            if (bios[0] != 0)
            {
                sprintf(buf, "BIOS Version: %s", bios);
                PrintLine(param, buf, TRUE);
            }

            bufferSize = 200;
            bios[0] = 0;
            SalRegQueryValueEx(hKey, "SystemBiosDate", NULL, NULL, (BYTE*)bios, &bufferSize);
            bios[_countof(bios) - 1] = 0; // aspon zakoncime buffer nulou
            if (bios[0] != 0)
            {
                sprintf(buf, "BIOS Date: %s", bios);
                PrintLine(param, buf, TRUE);
            }
            NOHANDLES(RegCloseKey(hKey));
        }

        MonitorEnumProcData enumData;
        enumData.Index = 1;
        enumData.PrintLine = PrintLine;
        enumData.Param = param;
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&enumData);
        PrintLine(param, "", FALSE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        if (Exception != NULL)
        {
            // vytahneme CONTEXT jednotlivych threadu
            // a zobrazime EIP + call stack
            PrintLine(param, "Stack Back Trace:", FALSE);
            BOOL firstTime = TRUE;
            int i;
            for (i = 0; i < CCallStack::CallStacks.Count; i++)
            {
                // !!! pristupy do pole CCallStack::CallStacks by chtelo ohranici kritickou sekci
                // takto riskujeme, ze sahneme do neexistujici pameti
                CCallStack* stack;
                stack = CCallStack::CallStacks[i];

                HANDLE hThread = NULL;
                if (stack != NULL)
                {
                    __try // trochu hloupa pojistka proti zmene CCallStack::CallStacks, zaroven se jistime pred funkci GetThreadHandleFromID
                    {
                        hThread = stack->ThreadHandle;
                        if (hThread == NULL)
                        {
                            // pokud je hThread NULL, zkusime thread priradit pomoci GetThreadHandleFromID
                            hThread = GetThreadHandleFromID(stack->ThreadID, FALSE);
                        }
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        PrintLine(param, "(GetThreadHandleFromID exception)", TRUE);
                        hThread = NULL;
                    }
                }
                if (stack != NULL && hThread != NULL)
                {
                    BOOL threadWithException = (Exception != NULL && Exception->ContextRecord != NULL && stack->ThreadID == ThreadID);
                    if (stack->ThreadID != GetCurrentThreadId() || threadWithException)
                    {
                        // pokud se nejedna o nas thread, uspime ho, abychom mohli vytahnout context
                        if (threadWithException || SuspendThread(hThread) != -1)
                        {
                            if (!firstTime)
                                PrintLine(param, "----", TRUE);
                            else
                                firstTime = FALSE;
                            sprintf(buf, "Thread ID: 0x%X", stack->ThreadID);
                            if (ThreadID == stack->ThreadID)
                                strcat(buf, " (Thread with Exception)");
                            PrintLine(param, buf, TRUE);

                            //            if (knownThreads.IsGood())
                            //              knownThreads.Add(stack->ThreadID);

                            // ===============================================
                            // === Argument Passing and Naming Conventions ===
                            // ===============================================
                            //
                            // name        Stack cleanup  Parameter passing                             name decoration                        int func(int a, double b)
                            // -----------------------------------------------------------------------------------------------------------------------------------------
                            // __cdecl     Caller         Pushes parameters on the stack, in reverse    prefix: "_"                            _func
                            //                            order (right to left)
                            //                            (ebp+0x8 is param0, ebp+0xC is param1 ...)
                            //
                            // __stdcall   Callee         Pushes parameters on the stack, in reverse    prefix: "_"; suffix: "@" followed      _func@12
                            //                            order (right to left)                         by the number of bytes (in decimal)
                            //                            (ebp+0x8 is param0, ebp+0xC is param1 ...)    in the argument list.
                            //
                            // __fastcall  Callee         The first two DWORD or smaller arguments are  prefix: "@"; suffix: "@" followed by   @func@12
                            //                            passed in ecx and edx registers; all other    the number of bytes (in decimal) in
                            //                            arguments are passed on stack,                the argument list.
                            //                            reverse order / right to left
                            //                            (ebp+0x8 is param2, ebp+0xC is param3 ...)
                            //
                            // thiscall    Callee         Pushed on stack; this pointer stored in ECX   (C++ mangling)
                            //
                            //
                            // ==========================================================
                            // === Stack frame layout (no frame pointer optimization) ===
                            // ==========================================================
                            //
                            // this function's ebp
                            // points here ------------\
              //     ebp-0x8   ebp-0x4   ebp+0x0   ebp+0x4   ebp+0x8   ebp+0xC
                            //                         caller's  return
                            //     local1    local0    ebp       addr      param0    param1
                            // ... 00000000  00000000  00000000  00000000  00000000  00000000 ...

                            // vytahneme context threadu
                            CONTEXT ctx;
                            ZeroMemory(&ctx, sizeof(ctx));
                            if (threadWithException)
                                memcpy(&ctx, Exception->ContextRecord, sizeof(ctx));
                            else
                                ctx.ContextFlags = CONTEXT_SEGMENTS | CONTEXT_INTEGER | CONTEXT_CONTROL;
                            if (threadWithException || GetThreadContext(hThread, &ctx))
                            {
#ifdef _WIN64
                                BOOL firstPc = TRUE;
                                // Unwind until IP is 0, sp is at the stack top, and callee IP is in kernel32.
                                while (TRUE)
                                {
                                    DWORD64 controlPc = ctx.Rip;

                                    CModuleInfo* modInfo = ModulesInfo.Find((void*)controlPc);
                                    const char* name;
                                    if (modInfo == NULL)
                                    {
                                        name = FindModuleName(nameBuf, (void*)controlPc, TRUE);
                                        if (*name == 0)
                                            name = "(unknown module)";
                                    }
                                    else
                                        name = modInfo->Name;

                                    if (firstPc)
                                    {
                                        sprintf(buf, "RIP = 0x%p %s", (void*)controlPc, name);
                                        firstPc = FALSE;
                                    }
                                    else
                                        sprintf(buf, "0x%p %s", (void*)controlPc, name);
                                    PrintLine(param, buf, TRUE);

                                    DWORD64 ImageBase;
                                    RUNTIME_FUNCTION* pFunctionEntry = RtlLookupFunctionEntry(
                                        controlPc,
                                        &ImageBase,
                                        NULL);
                                    if (pFunctionEntry)
                                    {
                                        PVOID HandlerData;
                                        DWORD64 EstablisherFrame;

                                        RtlVirtualUnwind(
                                            0,
                                            ImageBase,
                                            controlPc,
                                            pFunctionEntry,
                                            &ctx,
                                            &HandlerData,
                                            &EstablisherFrame,
                                            NULL);

                                        DWORD64 mewControlPc = ctx.Rip;
                                        if (mewControlPc == 0)
                                            break;
                                    }
                                    else
                                    {
                                        // Nested functions that do not use any stack space or nonvolatile
                                        // registers are not required to have unwind info (ex.
                                        // USER32!ZwUserCreateWindowEx).
                                        ctx.Rip = *(DWORD64*)(ctx.Rsp);
                                        ctx.Rsp += sizeof(DWORD64);
                                    }
                                }
#else
                                CModuleInfo* modInfo = ModulesInfo.Find((void*)ctx.Eip);
                                const char* name;
                                if (modInfo == NULL)
                                {
                                    name = FindModuleName(nameBuf, (void*)ctx.Eip, TRUE);
                                    if (*name == 0)
                                        name = "(unknown module)";
                                }
                                else
                                    name = modInfo->Name;
                                sprintf(buf, "EIP = 0x%p %s", (void*)ctx.Eip, name);
                                PrintLine(param, buf, TRUE);
                                // esp muze ukazovat nekam do haje - pojistime se
                                __try
                                {
                                    // *(ebp) je 'frame pointer' funkce, ktera nas volala
                                    // *(ebp + 4) je navratova hodnota (misto ve funkci, ktera nas volala, kde bude
                                    //            pokracovat beh az se vratime z teto funkce)
                                    DWORD* ebp = (DWORD*)ctx.Ebp;

                                    int j;
                                    for (j = 0; j < 50; j++)
                                    {
                                        DWORD retAddr = ebp[1]; // navratova hodnota *(ebp + 4)
                                        if (retAddr == 0)
                                            break;
                                        modInfo = ModulesInfo.Find((void*)retAddr);
                                        if (modInfo == NULL)
                                        {
                                            name = FindModuleName(nameBuf, (void*)retAddr, TRUE);
                                            if (*name == 0)
                                                name = "(unknown module)";
                                        }
                                        else
                                            name = modInfo->Name;
                                        sprintf(buf, "0x%p %s", (void*)retAddr, name);

                                        PrintLine(param, buf, TRUE);

                                        // presuneme se na stack frame funkce, ktera nas zavolala (*ebp)
                                        ebp = (DWORD*)ebp[0];
                                    }
                                }
                                __except (EXCEPTION_EXECUTE_HANDLER)
                                {
                                    PrintLine(param, "(exception)", TRUE);
                                }
#endif // _WIN64
                            }
                            if (stack->ThreadID != GetCurrentThreadId())
                                ResumeThread(hThread);
                        }
                    }
                }
            }

            if (threadNotFound)
            {
                HANDLE hThread = NULL;
                __try
                {
                    hThread = GetThreadHandleFromID(ThreadID, FALSE);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    PrintLine(param, "(GetThreadHandleFromID exception)", TRUE);
                    hThread = NULL;
                }
                if (hThread != NULL)
                {
                    if (!firstTime)
                        PrintLine(param, "----", TRUE);
                    else
                        firstTime = FALSE;
                    sprintf(buf, "Thread ID: 0x%X", ThreadID);
                    strcat(buf, " (Thread with Exception)");
                    PrintLine(param, buf, TRUE);

                    // vytahneme context threadu
                    CONTEXT ctx;
                    ZeroMemory(&ctx, sizeof(ctx));
                    memcpy(&ctx, Exception->ContextRecord, sizeof(ctx));
                    if (TRUE)
                    {
#ifdef _WIN64
                        BOOL firstPc = TRUE;
                        // Unwind until IP is 0, sp is at the stack top, and callee IP is in kernel32.
                        while (TRUE)
                        {
                            DWORD64 controlPc = ctx.Rip;

                            CModuleInfo* modInfo = ModulesInfo.Find((void*)controlPc);
                            const char* name;
                            if (modInfo == NULL)
                            {
                                name = FindModuleName(nameBuf, (void*)controlPc, TRUE);
                                if (*name == 0)
                                    name = "(unknown module)";
                            }
                            else
                                name = modInfo->Name;

                            if (firstPc)
                            {
                                sprintf(buf, "RIP = 0x%p %s", (void*)controlPc, name);
                                firstPc = FALSE;
                            }
                            else
                                sprintf(buf, "0x%p %s", (void*)controlPc, name);
                            PrintLine(param, buf, TRUE);

                            DWORD64 ImageBase;
                            RUNTIME_FUNCTION* pFunctionEntry = RtlLookupFunctionEntry(
                                controlPc,
                                &ImageBase,
                                NULL);
                            if (pFunctionEntry)
                            {
                                PVOID HandlerData;
                                DWORD64 EstablisherFrame;

                                RtlVirtualUnwind(
                                    0,
                                    ImageBase,
                                    controlPc,
                                    pFunctionEntry,
                                    &ctx,
                                    &HandlerData,
                                    &EstablisherFrame,
                                    NULL);

                                DWORD64 mewControlPc = ctx.Rip;
                                if (mewControlPc == 0)
                                    break;
                            }
                            else
                            {
                                // Nested functions that do not use any stack space or nonvolatile
                                // registers are not required to have unwind info (ex.
                                // USER32!ZwUserCreateWindowEx).
                                ctx.Rip = *(DWORD64*)(ctx.Rsp);
                                ctx.Rsp += sizeof(DWORD64);
                            }
                        }
#else
                        CModuleInfo* modInfo = ModulesInfo.Find((void*)ctx.Eip);
                        const char* name;
                        if (modInfo == NULL)
                        {
                            name = FindModuleName(nameBuf, (void*)ctx.Eip, TRUE);
                            if (*name == 0)
                                name = "(unknown module)";
                        }
                        else
                            name = modInfo->Name;
                        sprintf(buf, "EIP = 0x%p %s", (void*)ctx.Eip, name);
                        PrintLine(param, buf, TRUE);
                        // esp muze ukazovat nekam do haje - pojistime se
                        __try
                        {
                            // *(ebp) je 'frame pointer' funkce, ktera nas volala
                            // *(ebp + 4) je navratova hodnota (misto ve funkci, ktera nas volala, kde bude
                            //            pokracovat beh az se vratime z teto funkce)
                            DWORD* ebp = (DWORD*)ctx.Ebp;

                            int j;
                            for (j = 0; j < 50; j++)
                            {
                                DWORD retAddr = ebp[1]; // navratova hodnota *(ebp + 4)
                                if (retAddr == 0)
                                    break;
                                modInfo = ModulesInfo.Find((void*)retAddr);
                                if (modInfo == NULL)
                                {
                                    name = FindModuleName(nameBuf, (void*)retAddr, TRUE);
                                    if (*name == 0)
                                        name = "(unknown module)";
                                }
                                else
                                    name = modInfo->Name;
                                sprintf(buf, "0x%p %s", (void*)retAddr, name);

                                PrintLine(param, buf, TRUE);

                                // presuneme se na stack frame funkce, ktera nas zavolala (*ebp)
                                ebp = (DWORD*)ebp[0];
                            }
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER)
                        {
                            PrintLine(param, "(exception)", TRUE);
                        }
#endif // _WIN64
                    }
                }
            }
            PrintLine(param, "", FALSE);

            /*
  // zatim necham enumeraci threadu nedokoncenou, protoze Petr prisel s napadem zobrazit
  // back trace pro thread, ve kterem doslo k padu (zname jeho ID)

      // pole knownThreads obsahuje seznam zobrazenych threadu;
      // nyni se pokusime dohledat ty nezobrazene
      if (knownThreads.IsGood())
      {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != (HANDLE)-1)
        {
          static THREADENTRY32 thread;
          thread.dwSize = sizeof(thread);
          if (Thread32First(snap, &thread))
          {
            do
            {
              if (thread.th32OwnerProcessID ????)
              int a= 5;
            } while (Thread32Next(snap, &thread));
          }
          else
            DWORD err = GetLastError();
          NOHANDLES(CloseHandle(snap));
        }
      }
  */
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    __try
    {
        // DRIVES information
        if (TRUE)
        {
            PrintLine(param, "Drives:", FALSE);
            // dostupne disky
            // nejnizsi bit odpovida 'A', druhy bit 'B', ...
            DWORD netDrives; // bitove pole network disku
            static char bufForGetNetworkDrives[10000];
            __try
            {
                GetNetworkDrivesBody(netDrives, NULL, bufForGetNetworkDrives);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                PrintLine(param, "(exception in GetNetworkDrives)", TRUE);
                netDrives = 0;
            }
            DWORD mask = GetLogicalDrives();
            int i = 1;
            char drive = 'A';
            char root[10] = " :\\";
            while (i != 0)
            {
                if ((mask & i) || (netDrives & i)) // disk je pristupny
                {
                    BOOL accessible = (mask & i) != 0;
                    root[0] = drive;
                    UINT driveType = MyGetDriveType(root);
                    strcpy(buf, root);
                    if (driveType <= 6)
                    {
                        const char* drvTypeStrp[7] = {"unknown", "no root dir", "removable", "fixed", "remote", "cdrom", "ramdisk"};
                        sprintf(buf + strlen(buf), "  [%s]", drvTypeStrp[driveType]);
                    }
                    PrintLine(param, buf, TRUE);

                    BOOL getMoreInfo = TRUE;
                    if (driveType == 2)
                    {
                        // removable
                        int drv = drive - 'A' + 1;
                        DWORD medium = GetDriveFormFactor(drv);
                        if (medium == 350 || medium == 525 || medium == 800 || medium == 1)
                            getMoreInfo = FALSE; // nebudeme cvakat s disketou behem MyGetVolumeInformation()
                    }

                    if (getMoreInfo)
                    {
                        //---  GetVolumeInformation
                        static char volumeName[1000]; // pouzivano dale jako buffer
                                                      //        static char buff[300];
                        DWORD volumeSerialNumber;
                        DWORD maximumComponentLength;
                        DWORD fileSystemFlags;
                        static char fileSystemNameBuffer[100];
                        BOOL err = (MyGetVolumeInformation(root, NULL, NULL, NULL, volumeName, 200,
                                                           &volumeSerialNumber, &maximumComponentLength, &fileSystemFlags,
                                                           fileSystemNameBuffer, 100) == 0);
                        if (!err)
                        {
                            buf[0] = 0;
                            if (*volumeName != 0)
                                sprintf(buf + strlen(buf), "  LB: %s", volumeName);
                            sprintf(buf + strlen(buf), "  SN: %04X-%04X", HIWORD(volumeSerialNumber), LOWORD(volumeSerialNumber));
                            sprintf(buf + strlen(buf), "  FL: 0x%08X", fileSystemFlags);
                            sprintf(buf + strlen(buf), "  FS: %s", fileSystemNameBuffer);
                            sprintf(buf + strlen(buf), "  LN: %s", maximumComponentLength > 100 ? "yes" : "no");

                            PrintLine(param, buf, TRUE);
                        }

                        //---  GetDiskFreeSpace
                        DWORD sectorsPerCluster;
                        DWORD bytesPerSector;
                        DWORD numberOfFreeClusters;
                        DWORD totalNumberOfClusters;
                        err = (MyGetDiskFreeSpace(root, &sectorsPerCluster, &bytesPerSector,
                                                  &numberOfFreeClusters, &totalNumberOfClusters) == 0);

                        CQuadWord diskTotalBytes = CQuadWord(-1, -1), diskFreeBytes;
                        ULARGE_INTEGER availBytes, totalBytes, freeBytes;
                        if (GetDiskFreeSpaceEx(root, &availBytes, &totalBytes, &freeBytes))
                        {
                            diskTotalBytes.Value = (unsigned __int64)totalBytes.QuadPart;
                            diskFreeBytes.Value = (unsigned __int64)freeBytes.QuadPart;
                        }
                        if (diskTotalBytes == CQuadWord(-1, -1) && !err)
                        {
                            diskTotalBytes = CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0) *
                                             CQuadWord(totalNumberOfClusters, 0);
                            diskFreeBytes = CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0) *
                                            CQuadWord(numberOfFreeClusters, 0);
                        }
                        //---  GetDiskFreeSpace - zobrazeni
                        if (!err)
                        {
                            strcpy(buf, "  BytePerSec: ");
                            NumberToStr(buf + strlen(buf), CQuadWord(bytesPerSector, 0));
                            strcat(buf, "  SecPerClus: ");
                            NumberToStr(buf + strlen(buf), CQuadWord(sectorsPerCluster, 0));
                            if (CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0) != CQuadWord(0, 0))
                                strcat(buf, "  Clusters: ");
                            NumberToStr(buf + strlen(buf), diskTotalBytes / (CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0)));
                            PrintLine(param, buf, TRUE);
                        }
                        if (diskTotalBytes != CQuadWord(-1, -1))
                        {
                            strcpy(buf, "  Capacity: ");
                            PrintDiskSize(buf + strlen(buf), diskTotalBytes, 0);
                            strcat(buf, "  Free: ");
                            PrintDiskSize(buf + strlen(buf), diskFreeBytes, 0);
                            diskTotalBytes -= diskFreeBytes;
                            strcat(buf, "  Used: ");
                            PrintDiskSize(buf + strlen(buf), diskTotalBytes, 0);
                            PrintLine(param, buf, TRUE);
                        }
                    }
                    /*
          int driveType;
          if (mask & i)
            driveType = GetDriveType(root);
          else
            driveType = DRIVE_REMOTE;
          */
                    if (MyQueryDosDevice(drive - 'A', nameBuf, MAX_PATH))
                    {
                        sprintf(buf, "  Device: %s", nameBuf);
                        PrintLine(param, buf, TRUE);
                    }
                }
                drive++;
                i <<= 1;
            }
            PrintLine(param, "", FALSE);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        PrintLine(param, buf, TRUE);
        PrintLine(param, "", FALSE);
    }

    PrintLine(param, "End.", FALSE); // pro detekci uplnosti bug reportu

    if (Exception != NULL)
        ModulesInfo.Clean(); // neni treba, ale uvolnime pamet
}

void AddUniqueToGlobalModulesStore(const char* str)
{
    if (GlobalModulesStore.Count == 0)
    {
        char* s = DupStr(str);
        if (s != NULL)
        {
            GlobalModulesListTimeStore.Add(GetTickCount());
            if (!GlobalModulesListTimeStore.IsGood())
                GlobalModulesListTimeStore.ResetState();
            else
            {
                GlobalModulesStore.Add(s);
                if (!GlobalModulesStore.IsGood())
                {
                    GlobalModulesListTimeStore.Delete(0);
                    if (!GlobalModulesListTimeStore.IsGood())
                        GlobalModulesListTimeStore.ResetState();
                    GlobalModulesStore.ResetState();
                }
            }
        }
        return;
    }

    int l = 0, r = GlobalModulesStore.Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        int res = strcmp(str, GlobalModulesStore[m]);
        if (res == 0) // nalezeno
        {
            return;
        }
        else if (res < 0)
        {
            if (l == r || l > m - 1) // nenalezeno
            {
                char* s = DupStr(str);
                if (s != NULL)
                {
                    GlobalModulesListTimeStore.Insert(m, GetTickCount());
                    if (!GlobalModulesListTimeStore.IsGood())
                        GlobalModulesListTimeStore.ResetState();
                    else
                    {
                        GlobalModulesStore.Insert(m, s);
                        if (!GlobalModulesStore.IsGood())
                        {
                            GlobalModulesListTimeStore.Delete(m);
                            if (!GlobalModulesListTimeStore.IsGood())
                                GlobalModulesListTimeStore.ResetState();
                            GlobalModulesStore.ResetState();
                        }
                    }
                }
                return;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // nenalezeno
            {
                char* s = DupStr(str);
                if (s != NULL)
                {
                    GlobalModulesListTimeStore.Insert(m + 1, GetTickCount());
                    if (!GlobalModulesListTimeStore.IsGood())
                        GlobalModulesListTimeStore.ResetState();
                    else
                    {
                        GlobalModulesStore.Insert(m + 1, s);
                        if (!GlobalModulesStore.IsGood())
                        {
                            GlobalModulesListTimeStore.Delete(m + 1);
                            if (!GlobalModulesListTimeStore.IsGood())
                                GlobalModulesListTimeStore.ResetState();
                            GlobalModulesStore.ResetState();
                        }
                    }
                }
                return;
            }
            l = m + 1;
        }
    }
}

void AddNewlyLoadedModulesToGlobalModulesStore()
{
    char buf[500];
    __try
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
        if (snap != (HANDLE)-1)
        {
            MODULEENTRY32 module;
            module.dwSize = sizeof(module);
            if (Module32First(snap, &module))
            {
                do
                {
                    if ((int)module.dwSize >= ((char*)(&(module.szExePath)) - (char*)(&(module.dwSize))))
                    {
                        char moduleName[MAX_PATH];
                        char modulePath[MAX_PATH];
                        if (module.dwSize == sizeof(module))
                        {
                            lstrcpy(modulePath, module.szExePath);
                        }
                        else
                            lstrcpy(modulePath, "(unknown)");
                        lstrcpy(moduleName, module.szModule);
                        char ver[100];
                        GetModuleVersion((HINSTANCE)module.modBaseAddr, ver, 100);
                        sprintf(buf, "0x%p (size: 0x%X) (ver: %s): %s (%s)", module.modBaseAddr, module.modBaseSize, ver,
                                moduleName, modulePath);
                    }
                    else
                        sprintf(buf, "unknown module (dwSize = %u)", module.dwSize);
                    AddUniqueToGlobalModulesStore(buf);
                    module.dwSize = sizeof(module);
                } while (Module32Next(snap, &module));
            }
            NOHANDLES(CloseHandle(snap));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf(buf, "some exception has occured...");
        AddUniqueToGlobalModulesStore(buf);
    }
}
