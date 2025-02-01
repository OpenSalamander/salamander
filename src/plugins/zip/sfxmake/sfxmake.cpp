// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <windows.h>
#include <shlwapi.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "array2.h"
#include "resedit.h"
#include "crc32.h"
#include "sfxmake.h"

#include "..\config.h"
#include "..\deflate.h"

HANDLE SfxFile;
char Buffer[64 * 1024];
char* BufPos;
DWORD BytesLeft;

int Error(char* error)
{
    int lastErr = GetLastError();
    if (lastErr != ERROR_SUCCESS)
    {
        char buf[1024]; //temp variable
        *buf = 0;
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 1024, NULL);
        printf("%s\n%s", error, buf);
    }
    else
        printf("%s\n", error);

    return 1;
}

BOOL Read(HANDLE file, void* buffer, DWORD size)
{
    DWORD read;
    if (!ReadFile(file, buffer, size, &read, NULL) || read != size)
        return FALSE;
    return TRUE;
}

BOOL Write(HANDLE file, void* buffer, DWORD size)
{
    DWORD written;
    if (!WriteFile(file, buffer, size, &written, NULL))
        return FALSE;
    return TRUE;
}

BOOL GetChar(HANDLE file, char& c)
{
    DWORD read;
    //  char prev = -1;

    //  while (1)
    //  {
    if (!ReadFile(file, &c, 1, &read, NULL) || read != 1)
    {
        Error("Unable read text file.");
        return FALSE;
    }
    /*
    switch (c)
    {
      case '\r': break; //dump all CRLF sequences
      case '\n':
        if (prev == '\r') break;
        else return TRUE;

      case '\\': break; // read next (control) character

      case 'n':  //process all \n ad  sequences
        if (prev == '\\') c = '\n';
        return TRUE;

      case 'r': //process all \r ad  sequences
        if (prev == '\\') c = '\r';
        return TRUE;

      default: return TRUE;
    }
    */
    return TRUE;
    // prev = c;
    //  }
}

BOOL GetNextString(HANDLE file, char* buffer)
{
    char c;
    while (1)
    {
        if (!GetChar(file, c))
            return FALSE;
        if (c == '"')
            break;
    }
    char* b = buffer;
    while (1)
    {
        if (!GetChar(file, c))
            return FALSE;
        if (c == '"')
            break;
        *b++ = c;
    }
    *b = 0;
    return TRUE;
}

int ReadInput(char* buffer, unsigned size, int* error, void* user)
{
    if (BytesLeft < size)
        size = BytesLeft;
    BytesLeft -= size;
    if (size)
    {
        memcpy(buffer, BufPos, size);
        BufPos += size;
        return size;
    }
    return EOF;
}

int WriteOutput(char* buffer, unsigned size, void* user)
{
    if (!Write(SfxFile, buffer, size))
    {
        Error("Unable to write SFX package file.");
        return 3;
    }
    return 0;
}

inline int
HexToNumber(char c)
{
    if (!isxdigit(c))
        return 0;
    return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
}

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

int main(int argc, char* argv[])
{
    EnableExceptionsOn64();

    printf("\nSFXMake 1.2\tCopyright (c) 1997-2025 Open Salamander Authors\t\n\n");
    if (argc < 5 || argc > 5)
    {
        printf("Usage:\tsfxmake <sfxname> <textfile> <compatible version> <current version>\n\n"
               "<sfxname>\tFile name of a SFX package to be created.\n"
               "<textfile>\tFile with the default texts.\n"
               "<compatible version>\tVersion number of compatible SFX package.\n"
               "<current version>\tVersion of created SFX package.\n");
        return 1;
    }

    const bool useUPX = false;
    if (useUPX)
    {
        //zabalime exace UPXem
        printf("shrinking executables...\n");
        //nejprve odebereme resourcy
        CResEdit re;
        if (!re.BeginUpdateResource("Release\\sfxsmall.exe", FALSE))
            return Error("BeginUpdateResource failed.");
        CResDir* dir = re.GetResourceDirectory();
        if (!dir)
            return Error("Low memory.");
        re.RemoveAllResources();
        if (!re.EndUpdateResource(FALSE))
            return Error("EndUpdateResource failed.");
        //potom zkomprimujeme exac
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        if (!CreateProcess(NULL, "upx.exe --best Release\\sfxsmall.exe", NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi))
            Error("CreateProcess failed");
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            Error("wait failed");
        DWORD exitCode;
        if (!GetExitCodeProcess(pi.hProcess, &exitCode) || exitCode != 0)
            Error("bad exit code");
        //vratime resourcy
        if (!re.BeginUpdateResource("Release\\sfxsmall.exe", FALSE))
            return Error("BeginUpdateResource failed.");
        if (!re.SetResourceDirectory(dir))
            return Error("Low memory.");
        re.FreeResourceDirectory(dir);
        if (!re.EndUpdateResource(FALSE))
            return Error("EndUpdateResource failed.");

        //jeste druhy exac
        if (!re.BeginUpdateResource("ReleaseEx\\sfxbig.exe", FALSE))
            return Error("BeginUpdateResource failed.");
        dir = re.GetResourceDirectory();
        if (!dir)
            return Error("Low memory.");
        re.RemoveAllResources();
        if (!re.EndUpdateResource(FALSE))
            return Error("EndUpdateResource failed.");
        //potom zkomprimujeme exac
        memset(&si, 0, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        if (!CreateProcess(NULL, "upx.exe --best ReleaseEx\\sfxbig.exe", NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi))
            Error("CreateProcess failed");
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            Error("wait failed");
        if (!GetExitCodeProcess(pi.hProcess, &exitCode) || exitCode != 0)
            Error("bad exit code");
        //vratime resourcy
        if (!re.BeginUpdateResource("ReleaseEx\\sfxbig.exe", FALSE))
            return Error("BeginUpdateResource failed.");
        if (!re.SetResourceDirectory(dir))
            return Error("Low memory.");
        re.FreeResourceDirectory(dir);
        if (!re.EndUpdateResource(FALSE))
            return Error("EndUpdateResource failed.");
    }

    //open sfx package file
    SfxFile = CreateFile(argv[1], GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (SfxFile == INVALID_HANDLE_VALUE)
        return Error("Unable to create SFX package file.");

    //open file with default texts
    HANDLE textFile = CreateFile(argv[2], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (textFile == INVALID_HANDLE_VALUE)
        return Error("Unable to open text file.");

    CSfxFileHeader header;

    header.Signature = SFX_SIGNATURE;
    header.CompatibleVersion = atoi(argv[3]);
    header.CurrentVersion = atoi(argv[3]);
    if (!header.CompatibleVersion)
        return Error("Incorrect compatible version number.");
    if (!header.CurrentVersion)
        return Error("Incorrect current version number.");
    //read language ID
    char lang[2];
    if (!Read(textFile, lang, 2))
        return Error("Unable to read text file.");
    header.LangID = (HexToNumber(lang[0]) << 4) + HexToNumber(lang[1]);

    //write default text to package
    printf("Writing texts...\n");
    int offset = sizeof(CSfxFileHeader);
    if (SetFilePointer(SfxFile, offset, NULL, FILE_BEGIN) == 0xFFFFFFFF)
        return Error("Unable to seek SFX package file.");

    //pripravime si texty do bufferu
    int size = 0;
    for (int i = 0; i < LENARRAYSIZE; i++)
    {
        if (!GetNextString(textFile, Buffer + size))
            return Error("Unable to read texts file.");
        header.TextLen[i] = lstrlen(Buffer + size);
        size += header.TextLen[i];
    }

    CDeflate* defObj = new CDeflate();
    if (!defObj)
        return Error("Low Memory");

    header.TextsCRC = UpdateCrc((__UINT8*)Buffer, size, INIT_CRC, StaticCrcTab);

    ush internAttr = -1;
    ush flag = 0x08; //to aby ho to nahodou neulozilo jako stored
    int method = 8;
    ullg csize;
    BufPos = Buffer;
    BytesLeft = size;
    switch (defObj->Deflate(&internAttr, &method, 9, &flag,
                            &csize, WriteOutput, ReadInput, NULL))
    {
    case 0:
        break;
    case 1:
        return Error("Bad pack level.");
    case 2:
        return Error("Block vanished.");
    case 3:
        return 1;
    default:
        return Error("Unknown error while packing.");
    }
    header.TotalTextSize = (DWORD)csize;

    offset += (DWORD)csize;
    CloseHandle(textFile);

    //write sfxsmall.exe
    printf("Writing sfxsmall.exe...\n");
    HANDLE file = CreateFile("Release\\sfxsmall.exe", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return Error("Unable to open sfxsmall.exe.");

    header.SmallSfxOffset = offset;
    size = GetFileSize(file, NULL);
    if (size == 0xFFFFFFFF)
        return Error("Unable to get sfxsmall.exe file size.");
    if (size > 64 * 1024)
        return Error("Unable continue, sfxsmall.exe is to big.");

    if (!Read(file, Buffer, size))
        return Error("Unable to sfxsmall.exe.");
    header.SmallSfxCRC = UpdateCrc((__UINT8*)Buffer, size, INIT_CRC, StaticCrcTab);
    internAttr = -1;
    flag = 0x08; //to aby ho to nahodou neulozilo jako stored
    method = 8;
    csize = 0;
    BufPos = Buffer;
    BytesLeft = size;
    switch (defObj->Deflate(&internAttr, &method, 9, &flag,
                            &csize, WriteOutput, ReadInput, NULL))
    {
    case 0:
        break;
    case 1:
        return Error("Bad pack level.");
    case 2:
        return Error("Block vanished.");
    case 3:
        return 1;
    default:
        return Error("Unknown error while packing.");
    }
    header.SmallSfxSize = (DWORD)csize;
    offset += (DWORD)csize;

    CloseHandle(file);

    //write sfxbig.exe
    printf("Writing sfxbig.exe...\n");
    file = CreateFile("ReleaseEx\\sfxbig.exe", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return Error("Unable to open sfxbig.exe.");

    header.BigSfxOffset = offset;
    size = GetFileSize(file, NULL);
    if (size == 0xFFFFFFFF)
        return Error("Unable to get sfxbig.exe. file size.");
    if (size > 64 * 1024)
        return Error("Unable continue, sfxbig.exe. is to big.");

    if (!Read(file, Buffer, size))
        return Error("Unable to sfxbig.exe.");
    header.BigSfxCRC = UpdateCrc((__UINT8*)Buffer, size, INIT_CRC, StaticCrcTab);
    internAttr = -1;
    flag = 0x08; //to aby ho to nahodou neulozilo jako stored
    method = 8;
    csize = 0;
    BufPos = Buffer;
    BytesLeft = size;
    switch (defObj->Deflate(&internAttr, &method, 9, &flag,
                            &csize, WriteOutput, ReadInput, NULL))
    {
    case 0:
        break;
    case 1:
        return Error("Bad pack level.");
    case 2:
        return Error("Block vanished.");
    case 3:
        return 1;
    default:
        return Error("Unknown error while packing.");
    }
    header.BigSfxSize = (DWORD)csize;

    CloseHandle(file);

    delete defObj;

    //write package header
    header.HeaderCRC = UpdateCrc((__UINT8*)&header, sizeof(CSfxFileHeader) - sizeof(DWORD), INIT_CRC, StaticCrcTab);
    printf("Writing package header...\n");
    if (SetFilePointer(SfxFile, 0, NULL, FILE_BEGIN) == 0xFFFFFFFF)
        return Error("Unable to seek SFX package file.");
    if (!Write(SfxFile, &header, sizeof(CSfxFileHeader)))
        return Error("Unable to write SFX package file.");

    CloseHandle(SfxFile);

    printf("Operation completed successfuly.\nBye\n");

    return 0;
}
