// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "salmoncl.h"

CSalmonSharedMemory* SalmonSharedMemory = NULL;
HANDLE SalmonFileMapping = NULL;
HANDLE HSalmonProcess = NULL;

//****************************************************************************

// POZOR: bezime z entry point, pred inicializaci RTL, globalnich objektu, atd
// nevolat TRACE, HANDLES, RTL, ...

HANDLE GetBugReporterRegistryMutex()
{
    // prava uplne otevrena pro vsechny procesy
    SECURITY_ATTRIBUTES secAttr;
    char secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
    secAttr.nLength = sizeof(secAttr);
    secAttr.bInheritHandle = FALSE;
    secAttr.lpSecurityDescriptor = &secDesc;
    InitializeSecurityDescriptor(secAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(secAttr.lpSecurityDescriptor, TRUE, 0, FALSE);
    // do nazvu mutexu by bylo sikovne pridat SID, protoze procesy s ruznym SID bezi s jinym HKCU stromem
    // ale pro jednoduchost na to kaslem a mutex bude opravdu globalni
    const char* MUTEX_NAME = "Global\\AltapSalamanderBugReporterRegistryMutex";
    HANDLE hMutex = NOHANDLES(CreateMutex(&secAttr, FALSE, MUTEX_NAME));
    if (hMutex == NULL) // uz create umi otevrite existujici mutex, ale muze selhat, proto pak zkusime jeste open
        hMutex = NOHANDLES(OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME));
    return hMutex;
}

BOOL SalmonGetBugReportUID(DWORD64* uid)
{
    const char* BUG_REPORTER_KEY = "Software\\Open Salamander\\Bug Reporter";
    const char* BUG_REPORTER_UID = "ID";

    // tato sekce se pousti pri startu Salamandera a teoreticky muze dojit k soucasnemu ctani/zapisu registry
    // proto budeme pristup hlidat globalnim mutexem
    HANDLE hMutex = GetBugReporterRegistryMutex();
    if (hMutex != NULL)
        WaitForSingleObject(hMutex, INFINITE);
    *uid = 0;
    HKEY hKey;
    LONG res = NOHANDLES(RegOpenKeyEx(HKEY_CURRENT_USER, BUG_REPORTER_KEY, 0, KEY_READ, &hKey));
    if (res == ERROR_SUCCESS)
    {
        // pokusime se nacist stary udaj, pokud existuje
        DWORD gettedType;
        DWORD bufferSize = sizeof(*uid);
        res = RegQueryValueEx(hKey, BUG_REPORTER_UID, 0, &gettedType, (BYTE*)uid, &bufferSize);
        if (res != ERROR_SUCCESS || gettedType != REG_QWORD)
            *uid = 0;
        NOHANDLES(RegCloseKey(hKey));
    }
    // pokud UID zatim neexistuje, vytvorime ho a ulozime
    if (*uid == 0)
    {
        GUID guid;
        if (CoCreateGuid(&guid) == S_OK)
        {
            // nebudeme ukladat a odesilat cely GUID, staci jeho polovina, xornuta s druhou
            DWORD64* dw64 = (DWORD64*)&guid;
            *uid = dw64[0] ^ dw64[1];

            // pokusime se ho ulozit
            DWORD createType;
            LONG res2 = NOHANDLES(RegCreateKeyEx(HKEY_CURRENT_USER, BUG_REPORTER_KEY, 0, NULL, REG_OPTION_NON_VOLATILE,
                                                 KEY_READ | KEY_WRITE, NULL, &hKey, &createType));
            if (res2 == ERROR_SUCCESS)
            {
                res2 = RegSetValueEx(hKey, BUG_REPORTER_UID, 0, REG_QWORD, (BYTE*)uid, sizeof(*uid));
                if (res2 != ERROR_SUCCESS)
                    *uid = 0; // pri selhani chceme nulu
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
    SECURITY_ATTRIBUTES sa; // povolime dedeni handlu do child procesu (mohou pak pracovat primo s nasim eventem)
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    RtlFillMemory(mem, sizeof(CSalmonSharedMemory), 0);

    mem->Version = SALMON_SHARED_MEMORY_VERSION;
    // salmon bude spusten jako child proces s bInheritHandles==TRUE, takze muze pristupovat primo k temto handlum
    mem->ProcessId = GetCurrentProcessId();
    mem->Process = NOHANDLES(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, TRUE, mem->ProcessId));
    mem->Fire = NOHANDLES(CreateEvent(&sa, TRUE, FALSE, NULL));      // "nonsignaled" state, manual
    mem->Done = NOHANDLES(CreateEvent(&sa, TRUE, FALSE, NULL));      // "nonsignaled" state, manual
    mem->SetSLG = NOHANDLES(CreateEvent(&sa, TRUE, FALSE, NULL));    // "nonsignaled" state, manual
    mem->CheckBugs = NOHANDLES(CreateEvent(&sa, TRUE, FALSE, NULL)); // "nonsignaled" state, manual

    // cestu pro bug reporty nove davame do LOCAL_APPDATA, kam standardne minidumpy uklada Windows WER
    // cestu hned nevytvarime, o to se postarame az v okamziku padu
    if (SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, mem->BugPath) == S_OK)
    {
        int len = lstrlen(mem->BugPath);
        if (len > 0 && mem->BugPath[len - 1] == '\\') // radeji overime zpetne lomitko na konci cesty
            mem->BugPath[len - 1] = 0;
        lstrcat(mem->BugPath, "\\Open Salamander");
    }

    // zaklad jmena pro bug reporty
    strcpy(mem->BugName, "AS" VERSINFO_SAL_SHORT_VERSION);

    return (mem->Process != NULL && mem->Fire != NULL && mem->Done != NULL && mem->SetSLG != NULL &&
            mem->CheckBugs != NULL && mem->BugPath[0] != 0);
}

void GetStartupSLGName(char* slgName, DWORD slgNameMax)
{
    // vytahneme z registry nazev SLG, ktere se pravdepodobne bude pouzivat
    // dale za behu Salamandera muze dojit k volbe jineho, ktere se dodatecne zmeni
    // toto slouzi pouze jako default; pokud zaznam nenalezneme, predame prazdny retezec
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
    AddDoubleQuotesIfNeeded(cmd, MAX_PATH); // CreateProcess chce mit jmeno s mezerama v uvozovkach (jinak zkousi ruzny varianty, viz help)
    GetStartupSLGName(slgName, sizeof(slgName));
    wsprintf(cmd + strlen(cmd), " \"%s\" \"%s\"", fileMappingName, slgName); // slgName muze byt prazdny retezec, pokud neexistuje konfigurace
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.wShowWindow = SW_SHOWNORMAL;
    GetModuleFileName(NULL, rtlDir, MAX_PATH);
    *(strrchr(rtlDir, '\\') + 1) = 0;
    GetCurrentDirectory(MAX_PATH, oldCurDir);

    // dalsi pokus o reseni problemu nez rozdelime SALMON.EXE na EXE+DLL
    // zkusime pro child process (SALMON.EXE) rozsirit PATH env promennou o cestu k RTL
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

    // puvodne jsme pouze predavali rtlDir do CreateProcess, ale v nekterych kombinacich s UAC salmon.exe nebylo mozne spustit,
    // protoze nevidel RTL: https://forum.altap.cz/viewtopic.php?f=2&t=6957&p=26548#p26548
    // zkusime jeste navic nastavit current directory
    // pokud nezabere, muzeme zkusit do CreateProcess misto rtlDir predat NULL, pak by se podle MSDN mel podedit current directory od spoustejiciho procesu
    SetCurrentDirectory(rtlDir);
    // EDIT 4/2014: udelal jsem nekolik testu s Support@bluesware.ch a chr.mue@gmail.com viz maily
    // Vidim dve mozna reseni: zkusime rozsirit pro child proces PATH env promennou k SALRTL.
    // Druha moznost je rozdelit SALMON.EXE na EXE bez RTL a DLL s implicitne linkovanym RTL. Pred loadem SALMON.DLL
    // by jiz z beziciho SALMON.EXE bylo mozne nastavit current dir a SALMON.DLL naloadit runtime, coz by snad proslo.
    // ----
    // Na mem pocitaci kazde ze tri pouzitych nastaveni cesty funguje samo o sobe (ENV PATH, SetCurrentDirectory a rtlDir parametr ve volani CreateProcess
    if (NOHANDLES(CreateProcess(NULL, cmd, NULL, NULL, TRUE, //bInheritHandles==TRUE, potrebuje predat handly eventu!
                                CREATE_DEFAULT_ERROR_MODE | HIGH_PRIORITY_CLASS, NULL,
                                rtlDir, &si, &pi)))
    {
        HSalmonProcess = pi.hProcess;                           // handle salmon procesu potrebujeme, abychom byli schopni detekovat, ze zije
        AllowSetForegroundWindow(SalGetProcessId(pi.hProcess)); // pustime salmon nad nas
        //NOHANDLES(CloseHandle(pi.hProcess)); // nechame je s klidem leaknout, stejne by slo o posledni uvolnovane handly pred koncem procesu
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

// Chceme se dozvedet o SEH Exceptions i na x64 Windows 7 SP1 a dal
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
    // alokace sdileneho mista v pagefile.sys
    DWORD ti = (GetTickCount() >> 3) & 0xFFF;
    while (TRUE) // hledame unikatni jmeno pro file-mapping
    {
        wsprintf(salmonFileMappingName, "Salmon%X", ti++);
        SalmonFileMapping = NOHANDLES(CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
                                                        sizeof(CSalmonSharedMemory), salmonFileMappingName));
        if (SalmonFileMapping == NULL || GetLastError() != ERROR_ALREADY_EXISTS)
            break;
        NOHANDLES(CloseHandle(SalmonFileMapping));
    }
    if (SalmonFileMapping != NULL)
    {
        SalmonSharedMemory = (CSalmonSharedMemory*)NOHANDLES(MapViewOfFile(SalmonFileMapping, FILE_MAP_WRITE, 0, 0, 0)); // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
        if (SalmonSharedMemory != NULL)
        {
            ZeroMemory(SalmonSharedMemory, sizeof(CSalmonSharedMemory));
            if (SalmonSharedMemInit(SalmonSharedMemory))
            {
                SalmonGetBugReportUID(&SalmonSharedMemory->UID);

                // pokud se nezdari salmon spustit, stale vratime TRUE - problem bude oznamen pozdeji jiz po nacteni SLG
                SalmonStartProcess(salmonFileMappingName);
                return TRUE;
            }
        }
    }
    // doslo k zavazne (a nepredpokladane) chybe, zablokujeme spusteni Salamander, hlaska bude anglicky (nemame slg)
    return FALSE;
}

// info ze nebezi salmon staci zobrazit jednou
static BOOL SalmonNotRunningReported = FALSE;

void SalmonSetSLG(const char* slgName)
{
    ResetEvent(SalmonSharedMemory->Done);

    strcpy(SalmonSharedMemory->SLGName, slgName);
    SetEvent(SalmonSharedMemory->SetSLG);

    // cekame na signal ze Salmon, ze zpracoval ukol (event Done) nebo na pripad, kdy nekdo Salmon sestrelil
    HANDLE arr[2];
    arr[0] = HSalmonProcess;
    arr[1] = SalmonSharedMemory->Done;
    DWORD waitRet = WaitForMultipleObjects(2, arr, FALSE, INFINITE);
    if (waitRet != WAIT_OBJECT_0 + 1) // nekdo sestrelil salmon nebo se neco podelalo v komunikaci
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

    // cekame na signal ze Salmon, ze zpracoval ukol (event Done) nebo na pripad, kdy nekdo Salmon sestrelil
    HANDLE arr[2];
    arr[0] = HSalmonProcess;
    arr[1] = SalmonSharedMemory->Done;
    DWORD waitRet = WaitForMultipleObjects(2, arr, FALSE, INFINITE);
    if (waitRet != WAIT_OBJECT_0 + 1) // nekdo sestrelil salmon nebo se neco podelalo v komunikaci
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

    // cekame na signal ze Salmon, ze zpracoval ukol (event Done) nebo na pripad, kdy nekdo Salmon sestrelil
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