// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CFile
{
public:
    virtual ~CFile(){};
    virtual BOOL Read(LPVOID lpBuffer, DWORD nBytesToRead, DWORD* pnBytesRead, const char* fileName, HWND parent) = 0;
    virtual BOOL Write(LPCVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten, char* fileName, HWND parent) = 0;
    virtual BOOL Close(LPCTSTR fileName, HWND parent) = 0;
    virtual __int64 Seek(__int64 lDistanceToMove, DWORD dwMoveMethod) = 0;

    virtual DWORD GetFileSize(LPDWORD lpFileSizeHigh) = 0;

    virtual BOOL SetFileTime(const FILETIME* lpCreationTime, const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime);
    virtual BOOL GetFileTime(LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime);

    HANDLE GetFileHandle() { return File; }

protected:
    HANDLE File;
};

//****************************************************************************
//
// BufferedFile
//
// pro bufferovane cteni/zapis ze/do souboru
//

class CBufferedFile : public CFile
{
public:
    CBufferedFile(DWORD bufferSize = 32768);
    CBufferedFile(HANDLE hFile, DWORD dwDesiredAccess, DWORD bufferSize = 32768);
    virtual ~CBufferedFile();

    BOOL Create(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDispostion, DWORD dwFlagsAndAttributes);

    virtual BOOL Read(LPVOID lpBuffer, DWORD nBytesToRead, DWORD* pnBytesRead, const char* fileName, HWND parent);
    virtual BOOL Write(LPCVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten, char* fileName, HWND parent);
    virtual BOOL Close(LPCTSTR fileName, HWND parent);
    virtual __int64 Seek(__int64 lDistanceToMove, DWORD dwMoveMethod);

    virtual DWORD GetFileSize(LPDWORD lpFileSizeHigh);
    /*    void Lock();
    void Unlock();*/

protected:
    BOOL EndOfFile;
    BYTE* Buffer;
    DWORD BufferSize;    // size of the whole buffer
    DWORD BufferPos;     // internal pointer to the buffer (read/write)
    DWORD BufferFilled;  // number of valid bytes in the buffer (read)
    __int64 BufferStart; // position within the file where the buffer starts

    DWORD Access; // desired access to the file

    BOOL EmptyCache();

    //    CRITICAL_SECTION CS;
};

// file helpers
BOOL SafeReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToRead, DWORD* pnBytesRead, const char* fileName, HWND parent);
BOOL SafeWriteFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten, const char* fileName, HWND parent);

__int64 FileSeek(HANDLE hf, __int64 distance, DWORD moveMethod);
