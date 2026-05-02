// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef MAKEQWORD
typedef unsigned __int64 QWORD;
#define LODWORD(qw) ((DWORD)qw)
#define HIDWORD(qw) ((DWORD)(qw >> 32))
#define MAKEQWORD(lo, hi) (((QWORD)hi << 32) | lo)
#endif

enum CAccessType
{
    atRead,
    atReadWrite,
    atWriteCopy
};

class CFileMapping
{
    HANDLE File;
    QWORD FileSize;
    CAccessType Access;
    HANDLE MapObject;
    void* View;
    QWORD ViewOffset;
    DWORD ViewSize;
    DWORD MinViewSize;
    DWORD AllocationGranularity;

public:
    CFileMapping(DWORD minViewSize = 512 * 1024);
    ~CFileMapping() { Destroy(); }
    void Destroy();
    void Release();
    BOOL SetFile(HANDLE file, CAccessType access);
    void SetMinViewSize(DWORD size) { MinViewSize = size; }
    DWORD GetMinViewSize() { return MinViewSize; }
    void* MapViewOfFile(QWORD offset, DWORD size);
    QWORD GetFileSize() { return FileSize; }
    BOOL IsFileIOException(EXCEPTION_POINTERS* e);
};
