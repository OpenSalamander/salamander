// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <tchar.h>
#include <crtdbg.h>

#include "regparse.h"

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
        FIsWow64Process isWow64 = (FIsWow64Process)GetProcAddress(hDLL, "IsWow64Process");
        FSetProcessUserModeExceptionPolicy set = (FSetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "SetProcessUserModeExceptionPolicy");
        FGetProcessUserModeExceptionPolicy get = (FGetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "GetProcessUserModeExceptionPolicy");
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

int _tmain(int argc, TCHAR* argv[])
{
    EnableExceptionsOn64();

    HANDLE hFile;
    LPTSTR buf;
    DWORD size, nBytesRead;
    CSalamanderRegistryExAbstract* pRegistry;
    int ret = 0;

    _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);

    if ((argc != 2) && (argc != 3))
    {
        printf("RegParser reads in MBCS Reg4.0 and UTF16 Reg5.0 file and stores its content in Registry or second file.\n\n"
               "Usage:\n  RegParser.exe {InFile.reg [OutFile.reg]} | {branch OutFile.reg}\n\n"
               "1 argument - InFile.reg copied to system registry\n"
               "2 file arguments - InFile.reg parsed and resaved to OutFile.reg\n"
               "branch + 1 file argument - branch in system registry copied to OutFile.reg\n"
               "                           The branch must be enclosed in []\n\n");
        return 0;
    }

    if ((argc == 3) && (argv[1][0] == '['))
    {
        CSalamanderRegistryExAbstract* pSysRegistry = REG_SysRegistryFactory();
        pRegistry = REG_MemRegistryFactory();

        if (pSysRegistry && pRegistry)
        {
            // Copy system registry to file
            TCHAR branch[MAX_PATH];

            _tcscpy(branch, argv[1] + 1);
            size_t len = _tcslen(branch);
            if (len && (branch[len - 1] == ']'))
            {
                branch[len - 1] = 0;
                eRPE_ERROR regerr = CopyBranch(branch, pSysRegistry, pRegistry);
                if (RPE_OK == regerr)
                {
                    if (!pRegistry->Dump(argv[2], NULL))
                    {
                        _tprintf(_T("Dumping branch %s to file %s failed\n"), branch, argv[1]);
                        ret = 11;
                    }
                }
                else
                {
                    printf("Error %d: Could not copy branch %s from system registry\n", regerr, branch);
                    ret = 12;
                }
            }
            else
            {
                _tprintf(_T("Invalid branch name %s\n"), argv[2]);
                ret = 13;
            }
        }
        else
        {
            printf("Error: Could not instantiate registry classes\n");
            ret = 14;
        }
        if (pRegistry)
            pRegistry->Release();
        if (pSysRegistry)
            pSysRegistry->Release();
        return ret;
    }

    hFile = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        _tprintf(_T("Error %d: Could not open %s\n"), errno, argv[1]);
        return 1;
    }
    size = GetFileSize(hFile, NULL);
    if (0xFFFFFFFF == size)
    {
        printf("Error %d: Could not obtain input file size\n", GetLastError());
        CloseHandle(hFile);
        return 2;
    }
    buf = (LPTSTR)malloc(size + sizeof(WCHAR));
    if (!buf)
    {
        printf("Error: Could not allocate %d bytes\n", size);
        CloseHandle(hFile);
        return 3;
    }
    if (!ReadFile(hFile, buf, size, &nBytesRead, NULL) || (size != nBytesRead))
    {
        printf("Error: Could not read %d bytes (oonly %d bytes was read)\n", size, nBytesRead);
        free(buf);
        CloseHandle(hFile);
        return 4;
    }

    *(WCHAR*)((LPBYTE)buf + size) = 0; // safety net for too short file
    size = ConvertIfNeeded(&buf, size);
    if (!size)
    {
        printf("Error: Could not allocate memory\n");
        free(buf);
        CloseHandle(hFile);
        return 5;
    }

    pRegistry = (argc == 2) ? REG_SysRegistryFactory() : REG_MemRegistryFactory();
    if (pRegistry)
    {
        eRPE_ERROR regerr = Parse(buf, pRegistry, FALSE);
        if (RPE_OK == regerr)
        {
            if (!pRegistry->Dump(argv[2], NULL))
            {
                _tprintf(_T("Dumping to file %s failed\n"), argv[2]);
                ret = 6;
            }
        }
        else
        {
            _tprintf(_T("Loading file %s failed: error %d\n"), argv[1], regerr);
            ret = 7;
        }
        pRegistry->Release();
    }
    else
    {
        printf("Error: Could not instantiate registry class\n");
        ret = 8;
    }
    free(buf);
    CloseHandle(hFile);
#if defined(_DEBUG)
    _CrtDumpMemoryLeaks();
#endif
    return ret;
}