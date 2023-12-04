// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>

#include "sfx7zip.h"

#include "7zip.h"
#include "extract.h"
#include "resource.h"
#include "extract.h"
#include "7zip\archive\7zHeader.h"

#define INF_BUFFER_SIZE 5000

int HandleError(int titleID, int messageID, unsigned long err, const char* fileName)
{
    char title[500];
    char message[1000];
    LoadString(GetModuleHandle(NULL), titleID, title, 500);
    LoadString(GetModuleHandle(NULL), messageID, message, 500);
    if (err)
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message + lstrlen(message),
                      500 - lstrlen(message), NULL);
    if (fileName != NULL)
    {
        char* p = message + lstrlen(message);
        *p = '\n';
        if (p > message && *(p - 1) != '\n')
        {
            p++;
            *p = '\n';
        }
        lstrcpy(p + 1, fileName);
    }
    MessageBox(DlgWin, message, title, MB_OK | MB_ICONEXCLAMATION);
    return 1;
}

int HandleErrorW(int titleID, int messageID, unsigned long err, const wchar_t* fileName)
{
    wchar_t title[500];
    wchar_t message[1000];
    LoadStringW(GetModuleHandle(NULL), titleID, title, 500);
    LoadStringW(GetModuleHandle(NULL), messageID, message, 500);
    if (err)
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message + lstrlenW(message),
                       500 - lstrlenW(message), NULL);
    if (fileName != NULL)
    {
        wchar_t* p = message + lstrlenW(message);
        *p = L'\n';
        if (p > message && *(p - 1) != L'\n')
        {
            p++;
            *p = L'\n';
        }
        lstrcpyW(p + 1, fileName);
    }
    MessageBoxW(DlgWin, message, title, MB_OK | MB_ICONEXCLAMATION);
    return 1;
}

int InitExtraction(const char* name, struct SCabinet* cabinet)
{
    unsigned char data[200];
    unsigned long sectionOffset, sectionSize, direntries;
    int sections;
    DWORD read;

    //
    // otevreme soubor s archivem (exe) a namapujeme do pameti
    //
    cabinet->file = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (cabinet->file == INVALID_HANDLE_VALUE)
        return HandleError(ERROR_TITLE, ERROR_INFOPEN, GetLastError(), name);
    //
    // mame soubor, ted zanalyzujeme header exe fajlu
    //

    // zkontroluj ze je tam MZ signatura
    if (!ReadFile(cabinet->file, data, sizeof(IMAGE_DOS_HEADER), &read, NULL) ||
        read != sizeof(IMAGE_DOS_HEADER))
        if (read != sizeof(IMAGE_DOS_HEADER))
            return HandleError(ERROR_TITLE, ARC_INEOF, 0, name);
        else
            return HandleError(ERROR_TITLE, ERROR_INFREAD, GetLastError(), name);
    if (((IMAGE_DOS_HEADER*)data)->e_magic != ('M' + ('Z' << 8)))
        return HandleError(ERROR_TITLE, ARC_NOEXESIG, 0, name);
    if (SetFilePointer(cabinet->file, ((IMAGE_DOS_HEADER*)data)->e_lfanew, NULL, FILE_BEGIN) == 0xFFFFFFFF)
        return HandleError(ERROR_TITLE, ERROR_INFSEEK, GetLastError(), name);
    // precti headery
    if (!ReadFile(cabinet->file, data, sizeof(unsigned long) + sizeof(IMAGE_FILE_HEADER) + 24 * 4, &read, NULL) ||
        read != sizeof(unsigned long) + sizeof(IMAGE_FILE_HEADER) + 24 * 4)
        if (read != sizeof(unsigned long) + sizeof(IMAGE_FILE_HEADER) + 24 * 4)
            return HandleError(ERROR_TITLE, ARC_INEOF, 0, name);
        else
            return HandleError(ERROR_TITLE, ERROR_INFREAD, GetLastError(), name);
    if (*(unsigned long*)data != ('P' + ('E' << 8) + ('\0' << 16) + ('\0' << 24)))
        return HandleError(ERROR_TITLE, ARC_NOPESIG, 0, name);
    // number of sections
    sections = ((IMAGE_FILE_HEADER*)(data + sizeof(unsigned long)))->NumberOfSections;
    // number of directory entries
    direntries = ((IMAGE_OPTIONAL_HEADER*)(data + sizeof(unsigned long) + sizeof(IMAGE_FILE_HEADER)))->NumberOfRvaAndSizes;
    // preskoc dir entries
    if (SetFilePointer(cabinet->file, direntries * sizeof(IMAGE_DATA_DIRECTORY), NULL, FILE_CURRENT) == 0xFFFFFFFF)
        return HandleError(ERROR_TITLE, ERROR_INFSEEK, GetLastError(), name);
    sectionOffset = 0;
    sectionSize = 0;
    // precti hlavicky vsech sekci
    for (; sections > 0; sections--)
    {
        if (!ReadFile(cabinet->file, data, sizeof(IMAGE_SECTION_HEADER), &read, NULL) || read != sizeof(IMAGE_SECTION_HEADER))
            if (read != sizeof(IMAGE_SECTION_HEADER))
                return HandleError(ERROR_TITLE, ARC_INEOF, 0, name);
            else
                return HandleError(ERROR_TITLE, ERROR_INFREAD, GetLastError(), name);
        // get position and size of the last section
        if (sectionOffset < ((IMAGE_SECTION_HEADER*)data)->PointerToRawData)
        {
            sectionOffset = ((IMAGE_SECTION_HEADER*)data)->PointerToRawData;
            sectionSize = ((IMAGE_SECTION_HEADER*)data)->SizeOfRawData;
        }
    }
    // skoc na zacatek archivu
    if (SetFilePointer(cabinet->file, sectionOffset + sectionSize, NULL, FILE_BEGIN) == 0xFFFFFFFF)
        return HandleError(ERROR_TITLE, ERROR_INFSEEK, GetLastError(), name);
    //
    // Pripravime dekompresi
    //
    if (!DecompressInit(cabinet))
        return HandleError(ERROR_TITLE, ARC_INEOF, 0, name);
    else
        return 0;
}

void FinalWait()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (DlgWin == NULL || !IsWindow(DlgWin) || !IsDialogMessage(DlgWin, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    Sleep(500); // dame prilezitost k zobrazeni 100%
}

DWORD WINAPI ExtractArchive(LPVOID cabinet)
{
    int ret;
    //
    // Provedeme dekompresi
    //

    while (DlgWin == NULL)
        Sleep(50);

    ret = UnpackArchive((struct SCabinet*)cabinet);

    if (DlgWin != NULL && ret == 0)
    {
        ProgressPos = 200;
        SendMessage(DlgWin, WM_USER_UPDATEPROGRESS, 0, 0);
        UpdateWindow(DlgWin);

        // na 500ms pribrzdime, aby se zobrazilo 100%
        // nebude to zadna ztrata, stejne srotuje hard jak jsme se na vyvalili do TEMPu
        FinalWait();
    }

    PostMessage(DlgWin, WM_USER_THREADEXIT, 0, 0);

    return ret;
}

void RefreshProgress(unsigned long inend, unsigned long inptr, int diskSavePart)
{
    unsigned long pos = inptr / (inend / 100);
    if (diskSavePart != 0)
        pos += 100;
    if (ProgressPos != pos)
    {
        ProgressPos = pos;
        if (DlgWin != NULL)
            PostMessage(DlgWin, WM_USER_UPDATEPROGRESS, 0, 0);
    }
}
/*
void RefreshName(unsigned char *current_file_name)
{
  unsigned short i = 0;
  while (current_file_name[i] != '\0' && i < 100)
  {
    CurrentName[i] = current_file_name[i];
    i++;
  }
  CurrentName[i] = '\0';
  if (DlgWin != NULL)
    PostMessage(DlgWin, WM_USER_UPDATEFNAME, 0, 0);
}
*/
