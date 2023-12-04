// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#pragma warning(push)
#pragma warning(disable : 4091) // disable typedef warning without variable declaration
#include <dbghelp.h>
#pragma warning(pop)

BOOL GenerateMiniDump(CMinidumpParams* minidumpParams, CSalmonSharedMemory* mem, BOOL smallMinidump, BOOL* overSize)
{
    BOOL ret = FALSE;
    *overSize = FALSE;
    char szPath[MAX_PATH];
    ::GetModuleFileName(NULL, szPath, MAX_PATH);
    *(strrchr(szPath, '\\')) = 0;      // bezime z utils\salmon.exe
    strcat_s(szPath, "\\dbghelp.dll"); // chceme novou verzi, nejmene 6.1 a ta na starsich W2K/XP neni
    static HMODULE hDbgHelp;
    hDbgHelp = LoadLibrary(szPath);
    if (hDbgHelp != NULL)
    {
        typedef BOOL(WINAPI * MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, CONST PMINIDUMP_EXCEPTION_INFORMATION,
                                                   CONST PMINIDUMP_USER_STREAM_INFORMATION, CONST PMINIDUMP_CALLBACK_INFORMATION);
        static MiniDumpWriteDump_t funcMiniDumpWriteDump;
        typedef BOOL(WINAPI * MakeSureDirectoryPathExists_t)(PCSTR);
        static MakeSureDirectoryPathExists_t funcMakeSureDirectoryPathExists;
        funcMiniDumpWriteDump = (MiniDumpWriteDump_t)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
        funcMakeSureDirectoryPathExists = (MakeSureDirectoryPathExists_t)GetProcAddress(hDbgHelp, "MakeSureDirectoryPathExists");
        if (funcMiniDumpWriteDump != NULL && funcMakeSureDirectoryPathExists != NULL)
        {
            char szFileName[MAX_PATH];
            strcpy(szFileName, mem->BugPath); // cesta je zakoncena zpetnym lomitkem
            int bugPathLen = (int)strlen(mem->BugPath);
            if (bugPathLen > 0 && mem->BugPath[bugPathLen - 1] != '\\')
                strcat(szFileName, "\\");
            strcat(szFileName, mem->BaseName);
            strcat(szFileName, ".DMP");

            // cesta jeste nemusi existovat - vytvorime ji
            funcMakeSureDirectoryPathExists(szFileName); // nazev souboru je ignorovan

            HANDLE hDumpFile;
            hDumpFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
            if (hDumpFile != INVALID_HANDLE_VALUE)
            {
                EXCEPTION_POINTERS ePtrs;
                MINIDUMP_EXCEPTION_INFORMATION expParam;
                ePtrs.ContextRecord = &mem->ContextRecord;
                ePtrs.ExceptionRecord = &mem->ExceptionRecord;
                expParam.ThreadId = mem->ThreadId;
                expParam.ExceptionPointers = &ePtrs;
                expParam.ClientPointers = FALSE;

                // skvele vysvetleni flagu (lepsi nez na MSDN): http://www.debuginfo.com/articles/effminidumps.html#minidumptypes
                static MINIDUMP_TYPE dumpType;
                // nektere z flagu vyzaduji dbghelp.dll 6.1 - proto mame u Salamandera vlastni
                if (smallMinidump)
                {
                    dumpType = (MINIDUMP_TYPE)(MiniDumpWithProcessThreadData |
                                               MiniDumpWithDataSegs |
                                               MiniDumpWithFullMemoryInfo |
                                               MiniDumpWithThreadInfo |
                                               MiniDumpWithUnloadedModules |
                                               MiniDumpIgnoreInaccessibleMemory); // v zadnem pripade si neprejeme selhani funkce
                }
                else
                {
                    dumpType = (MINIDUMP_TYPE)(MiniDumpWithPrivateReadWriteMemory |
                                               MiniDumpWithDataSegs |
                                               MiniDumpWithHandleData |
                                               MiniDumpWithFullMemoryInfo |
                                               MiniDumpWithThreadInfo |
                                               MiniDumpWithUnloadedModules |
                                               MiniDumpIgnoreInaccessibleMemory); // v zadnem pripade si neprejeme selhani funkce
                }

                BOOL bMiniDumpSuccessful;
                bMiniDumpSuccessful = funcMiniDumpWriteDump(mem->Process, mem->ProcessId,
                                                            hDumpFile, dumpType,
                                                            &expParam,
                                                            NULL, NULL);
                if (bMiniDumpSuccessful)
                {
                    ret = TRUE;
                }
                else
                {
                    // generovani selhava pod W7 pri x64/Debug verzi spustene z MSVC; pokud ji spustim mimo MSVC, vse slape
                    DWORD err = GetLastError();
                    sprintf(minidumpParams->ErrorMessage, LoadStr(IDS_SALMON_MINIDUMP_CALL, HLanguage), err);
                }
                // at uz vratilo generovani minidumpu TRUE nebo FALSE, omrknem velikost vyprodukovaneho dumpu
                DWORD sizeHigh = 0;
                DWORD sizeLow = GetFileSize(hDumpFile, &sizeHigh);
                if (sizeLow != INVALID_FILE_SIZE && (sizeHigh > 0 || sizeLow > 50 * 1000 * 1024))
                    *overSize = TRUE; // pokud je vysledek vetsi nez 50MB, dame to vedet ven, aby se zkusila jeste mensi verze
                CloseHandle(hDumpFile);
            }
            else
            {
                DWORD err = GetLastError();
                sprintf(minidumpParams->ErrorMessage, LoadStr(IDS_SALMON_MINIDUMP_CREATE, HLanguage), szFileName, err);
            }
        }
        else
        {
            sprintf(minidumpParams->ErrorMessage, LoadStr(IDS_SALMON_LOAD_FAILED, HLanguage), szPath);
        }
    }
    else
    {
        sprintf(minidumpParams->ErrorMessage, LoadStr(IDS_SALMON_LOAD_FAILED, HLanguage), szPath);
    }

    return ret;
}

extern BOOL DirExists(const char* dirName);

// na zaklade aktualniho casu a kratke verze Salamandera nageneruje nazev (bez pripony)
// ze ktereho se nasledne odvodi nazev pro textovy bug reportu a pro minidump
void GetReportBaseName(char* name, int nameSize, const char* targetPath, const char* shortName, DWORD64 uid, SYSTEMTIME lt)
{
    static char year[10];
    static WORD y;

    y = lt.wYear;
    if (y >= 2000 && y < 2100)
        sprintf_s(year, "%02u", (BYTE)(y - 2000));
    else
        sprintf_s(year, "%04u", y);

    sprintf_s(name, nameSize, "%I64X-%s-%s%02u%02u-%02u%02u%02u",
              uid, shortName, year, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, lt.wSecond);
    CharUpperBuff(name, nameSize); // x64/x86 je lowercase, chceme vse upcase

    // pokud cilova cesta existuje, mohlo by dojit ke kolizi (nepravdepodobne diky casu v nazvu)
    if (targetPath != NULL && DirExists(targetPath))
    {
        static char findPath[MAX_PATH];
        static char findMask[MAX_PATH];
        int i;
        for (i = 0; i < 100; i++) // resime 1 - 99, pak to vzdavame
        {
            strcpy(findMask, name);
            if (i > 0)
                wsprintf(findMask + strlen(findMask), "-%d", i);
            lstrcat(findMask, "*");
            lstrcpy(findPath, targetPath);
            int findPathLen = lstrlen(findPath);
            if (findPathLen > 0 && findPath[findPathLen - 1] != '\\')
                lstrcat(findPath, "\\");
            lstrcat(findPath, findMask);
            WIN32_FIND_DATA find;
            HANDLE hFind = NOHANDLES(FindFirstFile(findPath, &find));
            if (hFind != INVALID_HANDLE_VALUE)
                NOHANDLES(FindClose(hFind));
            else
                break; // konflikt nenalezen
        }
        if (i > 0)
            sprintf(name + strlen(name), "-%d", i);
    }
}

DWORD WINAPI MinidumpThreadF(void* param)
{
    CMinidumpParams* minidumpParams = (CMinidumpParams*)param;

    SYSTEMTIME lt;
    GetLocalTime(&lt);

    //char baseName[MAX_PATH];
    GetReportBaseName(SalmonSharedMemory->BaseName, sizeof(SalmonSharedMemory->BaseName),
                      SalmonSharedMemory->BugPath, SalmonSharedMemory->BugName,
                      SalmonSharedMemory->UID, lt);

    // nagenerujeme minidump
    BOOL overSize;
    BOOL ret = GenerateMiniDump(minidumpParams, SalmonSharedMemory, FALSE, &overSize);

    if (!ret || overSize)
    {
        GetReportBaseName(SalmonSharedMemory->BaseName, sizeof(SalmonSharedMemory->BaseName),
                          SalmonSharedMemory->BugPath, SalmonSharedMemory->BugName,
                          SalmonSharedMemory->UID, lt);

        // nagenerujeme minidump
        ret = GenerateMiniDump(minidumpParams, SalmonSharedMemory, TRUE, &overSize);
    }

    // dame do Salamandera vedet, ze je minidump vytvoreny
    // v tutu chvili se Salamander pokusi ulozit na disk textovy bug report a ukonci se
    SetEvent(SalmonSharedMemory->Done);

    // pockame az Salamander report ulozi nebo prestane existovat; muze byt ve spatnem stavu, takze cekame jen omezenou dobu
    DWORD res = WaitForSingleObject(SalmonSharedMemory->Process, 10000);

    minidumpParams->Result = ret;
    return EXIT_SUCCESS;
}

HANDLE HMinidumpThread = NULL;

BOOL StartMinidumpThread(CMinidumpParams* params)
{
    if (HMinidumpThread != NULL)
        return FALSE;
    DWORD id;
    HMinidumpThread = CreateThread(NULL, 0, MinidumpThreadF, params, 0, &id);
    return HMinidumpThread != NULL;
}

BOOL IsMinidumpThreadRunning()
{
    if (HMinidumpThread == NULL)
        return FALSE;
    DWORD res = WaitForSingleObject(HMinidumpThread, 0);
    if (res != WAIT_TIMEOUT)
    {
        CloseHandle(HMinidumpThread);
        HMinidumpThread = NULL;
        return FALSE;
    }
    return TRUE;
}
