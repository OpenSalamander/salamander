// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//------------------------------------------------------------------------------------------------
//
// CompresBugReports()
//

BOOL CompresBugReports(CCompressParams* compressParams)
{
    BOOL ret = FALSE;
    compressParams->ErrorMessage[0] = 0;
    const char* WRAPPER_DLL = "..\\plugins\\7zip\\7zwrapper.dll";
    HINSTANCE h7zwrapper = LoadLibrary(WRAPPER_DLL);
    if (h7zwrapper != NULL)
    {
        typedef BOOL(WINAPI * CompressFiles_t)(const char* archiveName7z, const char* sourceDir, const char* filter, char* errorMessage, int errorMessageSize);
        CompressFiles_t CompressFiles;
        CompressFiles = (CompressFiles_t)GetProcAddress(h7zwrapper, "CompressFiles");
        if (CompressFiles != NULL)
        {
            char oldCurrentDir[MAX_PATH];
            GetCurrentDirectory(MAX_PATH, oldCurrentDir);

            ret = TRUE;

            char error[10000];
            for (int i = 0; i < BugReports.Count; i++)
            {
                SetCurrentDirectory(BugReportPath);

                char mask[MAX_PATH];
                strcpy(mask, BugReports[i].Name);
                strcat(mask, ".*");

                char archive[MAX_PATH];
                strcpy(archive, BugReports[i].Name);
                strcat(archive, ".7Z");
                DeleteFile(archive); // aby neselhala nasledna komprese

                error[0] = 0;
                BOOL res = CompressFiles(archive, BugReportPath, mask, error, 10000);
                if (!res)
                    lstrcpyn(compressParams->ErrorMessage, error, 2 * MAX_PATH - 1);
                ret &= res;
                if (!ReportOldBugs)
                    break;
            }

            SetCurrentDirectory(oldCurrentDir);
        }
        else
        {
            sprintf(compressParams->ErrorMessage, LoadStr(IDS_SALMON_LOAD_FAILED, HLanguage), WRAPPER_DLL);
        }
        FreeLibrary(h7zwrapper);
    }
    else
    {
        sprintf(compressParams->ErrorMessage, LoadStr(IDS_SALMON_LOAD_FAILED, HLanguage), WRAPPER_DLL);
    }
    return ret;
}

DWORD WINAPI CompressThreadF(void* param)
{
    CCompressParams* compressParams = (CCompressParams*)param;
    compressParams->Result = CompresBugReports(compressParams);
    return EXIT_SUCCESS;
}

HANDLE HCompressThread = NULL;

BOOL StartCompressThread(CCompressParams* params)
{
    if (HCompressThread != NULL)
        return FALSE;
    DWORD id;
    HCompressThread = CreateThread(NULL, 0, CompressThreadF, params, 0, &id);
    return HCompressThread != NULL;
}

BOOL IsCompressThreadRunning()
{
    if (HCompressThread == NULL)
        return FALSE;
    DWORD res = WaitForSingleObject(HCompressThread, 0);
    if (res != WAIT_TIMEOUT)
    {
        CloseHandle(HCompressThread);
        HCompressThread = NULL;
        return FALSE;
    }
    return TRUE;
}
