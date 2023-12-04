// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// error codes
#define AE_SUCCESS 0
#define AE_OPEN 1
#define AE_ACCESS 2
#define AE_EOF 3
#define AE_BADARC 4
#define AE_BADDATA 5
#define AE_BADVERSION 6
#define AE_ENCRYPT 7
#define AE_METHOD 8
#define AE_UNKNTYPE 9
#define AE_CRC 10
#define AE_BADVOL 11

// operace pro  ProcessFile()
#define PFO_SKIP 0
#define PFO_EXTRACT 1

// mody pro ARJChangeVolProc()
#define CVM_NOTIFY 0
#define CVM_ASK 1

// flagy pro ARJErrorProc()
#define EF_RETRY 0x01

// callbacky z dllka
typedef BOOL(WINAPI* FARJChangeVolProc)(char* volName, char* prevName, int mode);
typedef BOOL(WINAPI* FARJProcessDataProc)(const void* buffer, DWORD size);
typedef BOOL(WINAPI* FARJErrorProc)(int error, BOOL flags);

struct CARJOpenData
{
    //input fields
    const char* ArcName;
    FARJChangeVolProc ARJChangeVolProc;
    FARJProcessDataProc ARJProcessDataProc;
    FARJErrorProc ARJErrorProc;
    CDynamicString* AchiveVolumes;
};

#define ARJ_MAX_PATH 512

// flags
#define FF_ENCRYPTED 0x01 // =garbled
#define FF_VOLUME 0x04
#define FF_EXTFILE 0x08
#define FF_PATHSYM 0x10
#define FF_BACKUP 0x20

// file type
#define FT_BINARY 0
#define FT_TEXT 1
#define FT_DIRECTORY 3
#define FT_VOLUMELABEL 4

struct CARJHeaderData
{
    BYTE ArcVer;
    BYTE ArcVerMin;
    BYTE HostOS;
    BYTE Flags;
    BYTE Method;
    BYTE FileType;
    FILETIME Time;
    DWORD Size;
    DWORD CompSize;
    DWORD Attr;
    char FileName[ARJ_MAX_PATH];
};

BOOL WINAPI ARJOpenArchive(CARJOpenData* openData);
BOOL WINAPI ARJCloseArchive();
BOOL WINAPI ARJReadHeader(CARJHeaderData* headerData);
BOOL WINAPI ARJProcessFile(int operation, LPDWORD size = NULL);
