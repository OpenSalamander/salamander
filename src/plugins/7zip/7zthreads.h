// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define WM_7ZIP WM_USER + 234

#define WM_7ZIP_PROGRESS 0
#define WM_7ZIP_SETTOTAL 1
#define WM_7ZIP_ADDTEXT 2
#define WM_7ZIP_CREATEFILE 3
#define WM_7ZIP_SHOWMBOXEX 4
#define WM_7ZIP_DIALOGERROR 5
#define WM_7ZIP_PASSWORD 6

// Argument to WM_7ZIP_CREATEFILE
struct CCreateFileParams
{
    const char* FileInfo;
    const char* FileName;
    const char* Name;
    DWORD* pSilent;
    BOOL* pSkip;
};

// Argument to WM_7ZIP_DIALOGERROR
struct CDialogErrorParams
{
    DWORD Flags;
    const char* FileName;
    const char* Error;
};

//  Structures to launch 7-zip tasks in a new threads

struct CDecompressParamObject
{
    IInArchive* Archive;
    UINT32* FileIndex;
    int Count;
    bool Test;
    IArchiveExtractCallback* Callback;
};

struct CUpdateParamObject
{
    IOutArchive* Archive;
    IOutStream* Stream;
    int Count;
    IArchiveUpdateCallback* Callback;
};

HRESULT DoDecompress(CSalamanderForOperationsAbstract* salamander, CDecompressParamObject* dpo);
HRESULT DoUpdate(CSalamanderForOperationsAbstract* salamander, CUpdateParamObject* upo);
