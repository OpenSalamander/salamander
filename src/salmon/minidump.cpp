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
    *(strrchr(szPath, '\\')) = 0;      // we are running from utils\\salmon.exe
    strcat_s(szPath, "\\dbghelp.dll"); // we want a newer version, at least 6.1, which older W2K/XP do not have
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
            strcpy(szFileName, mem->BugPath); // the path ends with a trailing backslash
            int bugPathLen = (int)strlen(mem->BugPath);
            if (bugPathLen > 0 && mem->BugPath[bugPathLen - 1] != '\\')
                strcat(szFileName, "\\");
            strcat(szFileName, mem->BaseName);
            strcat(szFileName, ".DMP");

            // the path may not exist yet - create it
            funcMakeSureDirectoryPathExists(szFileName); // the file name is ignored

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

                // great explanation of the flags (better than on MSDN): http://www.debuginfo.com/articles/effminidumps.html#minidumptypes
                static MINIDUMP_TYPE dumpType;
                // some of the flags require dbghelp.dll 6.1 - that is why Salamander ships its own copy
                if (smallMinidump)
                {
                    dumpType = (MINIDUMP_TYPE)(MiniDumpWithProcessThreadData |
                                               MiniDumpWithDataSegs |
                                               MiniDumpWithFullMemoryInfo |
                                               MiniDumpWithThreadInfo |
                                               MiniDumpWithUnloadedModules |
                                               MiniDumpIgnoreInaccessibleMemory); // under no circumstances do we want the function to fail
                }
                else
                {
                    dumpType = (MINIDUMP_TYPE)(MiniDumpWithPrivateReadWriteMemory |
                                               MiniDumpWithDataSegs |
                                               MiniDumpWithHandleData |
                                               MiniDumpWithFullMemoryInfo |
                                               MiniDumpWithThreadInfo |
                                               MiniDumpWithUnloadedModules |
                                               MiniDumpIgnoreInaccessibleMemory); // under no circumstances do we want the function to fail
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
                    // generation fails on W7 with the x64/Debug build launched from MSVC; if I run it outside MSVC, everything works fine
                    DWORD err = GetLastError();
                    sprintf(minidumpParams->ErrorMessage, LoadStr(IDS_SALMON_MINIDUMP_CALL, HLanguage), err);
                }
                // regardless of whether minidump generation returned TRUE or FALSE, check the size of the produced dump
                DWORD sizeHigh = 0;
                DWORD sizeLow = GetFileSize(hDumpFile, &sizeHigh);
                if (sizeLow != INVALID_FILE_SIZE && (sizeHigh > 0 || sizeLow > 50 * 1000 * 1024))
                    *overSize = TRUE; // if the result exceeds 50 MB, report it so a smaller version can be tried
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

// based on the current time and the short Salamander version, generate a name (without an extension)
// from which the names for the text bug report and for the minidump are derived
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
    CharUpperBuff(name, nameSize); // x64/x86 is lowercase, we want everything uppercased

    // if the target path exists, there could be a collision (unlikely thanks to the timestamp in the name)
    if (targetPath != NULL && DirExists(targetPath))
    {
        static char findPath[MAX_PATH];
        static char findMask[MAX_PATH];
        int i;
        for (i = 0; i < 100; i++) // cover 1 - 99, then give up
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
                break; // no conflict found
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

    // char baseName[MAX_PATH];
    GetReportBaseName(SalmonSharedMemory->BaseName, sizeof(SalmonSharedMemory->BaseName),
                      SalmonSharedMemory->BugPath, SalmonSharedMemory->BugName,
                      SalmonSharedMemory->UID, lt);

    // generate the minidump
    BOOL overSize;
    BOOL ret = GenerateMiniDump(minidumpParams, SalmonSharedMemory, FALSE, &overSize);

    if (!ret || overSize)
    {
        GetReportBaseName(SalmonSharedMemory->BaseName, sizeof(SalmonSharedMemory->BaseName),
                          SalmonSharedMemory->BugPath, SalmonSharedMemory->BugName,
                          SalmonSharedMemory->UID, lt);

        // generate the minidump
        ret = GenerateMiniDump(minidumpParams, SalmonSharedMemory, TRUE, &overSize);
    }

    // let Salamander know that the minidump has been created
    // at this moment Salamander attempts to write the text bug report to disk and then exits
    SetEvent(SalmonSharedMemory->Done);

    // wait until Salamander saves the report or terminates; it may be in a bad state, so wait only for a limited time
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
