// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CCachedFile
{
    HANDLE File;
    QWORD FileSize;
    LPBYTE Buffer;
    DWORD BufferSize;    // Size of allocated Buffer
    DWORD DataInBufSize; // Size of valid data in Buffer
    QWORD BufferOffset;  // Offset of Buffer in file

public:
    CCachedFile(/*DWORD minViewSize = 512*1024*/);
    ~CCachedFile() { Destroy(); }
    void Destroy();
    int SetFile(HANDLE file);
    DWORD GetMinViewSize();
    LPBYTE ReadBuffer(QWORD offset, DWORD size, const int& CancelFlag);
    QWORD GetFileSize() { return FileSize; }
};
