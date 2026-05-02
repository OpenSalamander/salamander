// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "translator.h"
#include "wndlayt.h"
#include "trlipc.h"

const char* SHARED_MEMORY_NAME = "Local\\AltapTranslatorSharedMemory";

#define SHARED_MEMORY_VERSION 1

BOOL CreateSharedMemory(HANDLE* sharedFileHandle, CSharedMemory** sharedMemory, DWORD sharedMemorySize)
{
    HANDLE hMapFile;
    LPCTSTR pBuf;

    sharedMemorySize += sizeof(CSharedMemory);

    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE, // use paging file
        NULL,                 // default security
        PAGE_READWRITE,       // read/write access
        0,                    // maximum object size (high-order DWORD)
        sharedMemorySize,     // maximum object size (low-order DWORD)
        SHARED_MEMORY_NAME);  // name of mapping object

    if (hMapFile == NULL)
    {
        DWORD err = GetLastError();
        TRACE_E("Could not create file mapping object. err=" << err);
        return FALSE;
    }
    pBuf = (LPTSTR)MapViewOfFile(hMapFile,            // handle to map object
                                 FILE_MAP_ALL_ACCESS, // read/write permission
                                 0,
                                 0,
                                 sharedMemorySize);

    if (pBuf == NULL)
    {
        DWORD err = GetLastError();
        TRACE_E("Could not map view of file. err=" << err);
        CloseHandle(hMapFile);
        return FALSE;
    }

    *sharedFileHandle = hMapFile;
    *sharedMemory = (CSharedMemory*)pBuf;

    return TRUE;
}

BOOL ReadSharedMemory()
{
    HANDLE hMapFile;
    LPCTSTR pBuf;

    hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS, // read/write access
        FALSE,               // do not inherit the name
        SHARED_MEMORY_NAME); // name of mapping object

    if (hMapFile == NULL)
    {
        DWORD err = GetLastError();
        TRACE_E("Could not open file mapping object. err=" << err);
        return FALSE;
    }

    pBuf = (LPTSTR)MapViewOfFile(hMapFile,            // handle to map object
                                 FILE_MAP_ALL_ACCESS, // read/write permission
                                 0,
                                 0,
                                 0);

    if (pBuf == NULL)
    {
        DWORD err = GetLastError();
        TRACE_E("Could not map view of file. err=" << err);
        CloseHandle(hMapFile);
        return FALSE;
    }

    CSharedMemory* sharedMemory = (CSharedMemory*)pBuf;
    if (sharedMemory->Version != SHARED_MEMORY_VERSION)
    {
        TRACE_E("Invalid shared memory version: " << sharedMemory->Version);
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        return FALSE;
    }
    if (sharedMemory->Taken)
    {
        TRACE_E("Shared memory already taken!");
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        return FALSE;
    }

    DWORD sharedMemorySize = sharedMemory->Size;
    SharedMemoryCopy = (BYTE*)malloc(sharedMemorySize);
    memcpy(SharedMemoryCopy, sharedMemory, sharedMemorySize);

    sharedMemory->Taken = TRUE;

    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    return TRUE;
}

void FreeSharedMemory()
{
    if (SharedMemoryCopy != NULL)
    {
        free(SharedMemoryCopy);
        SharedMemoryCopy = NULL;
    }
}

void CloseSharedMemory(HANDLE sharedFileHandle, CSharedMemory* sharedMemory)
{
    UnmapViewOfFile(sharedMemory);
    CloseHandle(sharedFileHandle);
}

BOOL CLayoutEditor::StartNewTranslatorWithLayoutEditor()
{
    CDialogData* dialogData = TransformStack[CurrentDialog];

    char exeName[MAX_PATH];
    GetModuleFileName(HInstance, exeName, MAX_PATH);

    char prjFullName[2 * MAX_PATH];
    strcpy_s(prjFullName, Data.ProjectFile);
    char* p = strrchr(prjFullName, '\\');
    if (p == NULL)
    {
        TRACE_E("Invalid project file name!");
        return FALSE;
    }
    char prjName[MAX_PATH];
    strcpy_s(prjName, p + 1);
    *p = 0;
    p = strrchr(prjFullName, '\\');
    if (p == NULL)
    {
        TRACE_E("Invalid project file name!");
        return FALSE;
    }
    sprintf_s(p + 1, MAX_PATH, "%s\\%s", "Czech", prjName);

    char cmdLine[3 * MAX_PATH];
    sprintf_s(cmdLine, "\"%s\" -open-layout-editor %d \"%s\"", exeName, dialogData->ID, prjFullName);

    DWORD sharedMemorySize = GetTransformStackStreamSize();
    HANDLE sharedFileHandle;
    CSharedMemory* sharedMemory;
    if (!CreateSharedMemory(&sharedFileHandle, &sharedMemory, sharedMemorySize))
    {
        return FALSE;
    }
    sharedMemory->Version = SHARED_MEMORY_VERSION;
    sharedMemory->Taken = FALSE;
    sharedMemory->Size = sizeof(CSharedMemory) + sharedMemorySize;
    WriteTransformStackStream((BYTE*)sharedMemory + sizeof(CSharedMemory));

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    if (!HANDLES(CreateProcess(NULL, cmdLine, NULL, NULL, FALSE,
                               NORMAL_PRIORITY_CLASS, NULL, /*expInitDir*/ NULL, &si, &pi)))
    {
        CloseSharedMemory(sharedFileHandle, sharedMemory);
        return FALSE;
    }
    HANDLES(CloseHandle(pi.hProcess));
    HANDLES(CloseHandle(pi.hThread));

    for (int i = 0; i < 500 && !sharedMemory->Taken; i++)
        Sleep(10);
    if (!sharedMemory->Taken)
    {
        TRACE_E("Shared memory timeout!");
    }

    CloseSharedMemory(sharedFileHandle, sharedMemory);
    return TRUE;
}
