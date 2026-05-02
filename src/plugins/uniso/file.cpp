// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "file.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif // INVALID_SET_FILE_POINTER

__int64 FileSeek(HANDLE hf, __int64 distance, DWORD moveMethod)
{
    LARGE_INTEGER li;

    li.QuadPart = distance;

    li.LowPart = ::SetFilePointer(hf, li.LowPart, &li.HighPart, moveMethod);

    if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
    {
        li.QuadPart = -1;
    }

    return li.QuadPart;
}

BOOL SafeReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToRead, DWORD* pnBytesRead, const char* fileName, HWND parent)
{
    while (!ReadFile(hFile, lpBuffer, nBytesToRead, pnBytesRead, NULL))
    {
        int lastErr = GetLastError();
        char error[1024];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error, 1024, NULL);
        if (SalamanderGeneral->DialogError(parent == NULL ? SalamanderGeneral->GetMsgBoxParent() : parent, BUTTONS_RETRYCANCEL,
                                           fileName, error, LoadStr(IDS_READERROR)) != DIALOG_RETRY)
            return FALSE;
    }
    return TRUE;
}

BOOL SafeWriteFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten, LPCTSTR fileName, HWND parent)
{
    while (!WriteFile(hFile, lpBuffer, nBytesToWrite, pnBytesWritten, NULL))
    {
        int lastErr = GetLastError();
        char error[1024];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error, 1024, NULL);
        if (SalamanderGeneral->DialogError(parent == NULL ? SalamanderGeneral->GetMsgBoxParent() : parent, BUTTONS_RETRYCANCEL,
                                           fileName, error, LoadStr(IDS_WRITEERROR)) != DIALOG_RETRY)
            return FALSE;
    }
    return TRUE;
}

//
// BufferedFile
//

CBufferedFile::CBufferedFile(DWORD bufferSize /* = 32768*/)
{
    File = NULL;

    BufferSize = bufferSize;
    Buffer = new BYTE[BufferSize];

    //  ::InitializeCriticalSection(&CS);
}

CBufferedFile::CBufferedFile(HANDLE hFile, DWORD dwDesiredAccess, DWORD bufferSize /* = 32768*/)
{
    File = hFile;

    Access = dwDesiredAccess;

    BufferSize = bufferSize;
    Buffer = new BYTE[BufferSize];

    //  ::InitializeCriticalSection(&CS);

    EmptyCache();
    BufferStart = 0;
}

CBufferedFile::~CBufferedFile()
{
    //  ::DeleteCriticalSection(&CS);

    if (File != NULL)
        this->Close("", SalamanderGeneral->GetMainWindowHWND());

    delete[] Buffer;
}

/*void CBufferedFile::Lock() {
  ::EnterCriticalSection(&CS);
}

void CBufferedFile::Unlock() {
  ::LeaveCriticalSection(&CS);
}*/

BOOL CBufferedFile::Create(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDispostion, DWORD dwFlagsAndAttributes)
{
    File = CreateFile(lpFileName, dwDesiredAccess, dwShareMode, NULL, dwCreationDispostion, dwFlagsAndAttributes, NULL);
    if (File == INVALID_HANDLE_VALUE)
    {
        File = NULL;
        return FALSE;
    }

    Access = dwDesiredAccess;
    EmptyCache();
    BufferStart = 0;

    return TRUE;
}

__int64 CBufferedFile::Seek(__int64 lDistanceToMove, DWORD dwMoveMethod)
{
    __int64 newPos = 0;
    switch (dwMoveMethod)
    {
    case FILE_CURRENT:
        newPos = BufferStart + BufferPos + lDistanceToMove;
        newPos = FileSeek(File, newPos, FILE_BEGIN);
        break;

    default:
        newPos = FileSeek(File, lDistanceToMove, dwMoveMethod);
        break;
    }

    if (BufferStart <= newPos && newPos < BufferStart + BufferFilled)
    {
        BufferPos = (DWORD)(newPos - BufferStart);
        // Ensure next Read reads from the correct position
        FileSeek(File, BufferStart + BufferFilled, FILE_BEGIN);
    }
    else
    {
        EmptyCache();
        BufferStart = newPos;
    }

    return newPos;
}

BOOL CBufferedFile::EmptyCache()
{
    BufferPos = 0;
    EndOfFile = FALSE;
    BufferFilled = 0;

    return TRUE;
}

BOOL CBufferedFile::Read(LPVOID lpBuffer, DWORD nBytesToRead, DWORD* pnBytesRead, const char* fileName, HWND parent)
{
    DWORD nRemain = nBytesToRead;
    BYTE* lpDest = (BYTE*)lpBuffer;
    *pnBytesRead = 0;

    BOOL ret = TRUE;
    while (nRemain > 0)
    {
        if (BufferPos + nRemain <= BufferFilled)
        {
            memcpy(lpDest, Buffer + BufferPos, nRemain);
            BufferPos += nRemain;
            *pnBytesRead += nRemain;
            nRemain = 0;
        }
        else
        {
            int nToRead = BufferFilled - BufferPos;
            if (nToRead <= 0)
            {
                DWORD read;
                LONG offsetHigh = 0;
                BufferStart = ::SetFilePointer(File, 0, &offsetHigh, FILE_CURRENT);
                BufferStart += ((__int64)(DWORD)offsetHigh) << 32;
                ret = SafeReadFile(File, Buffer, BufferSize, &read, fileName, parent);
                BufferFilled = read;
                BufferPos = 0;

                if (!ret || read == 0)
                {
                    // reading beyond the end of the file
                    ret = FALSE;
                    break;
                }
            }
            else
            {
                memcpy(lpDest, Buffer + BufferPos, nToRead);
                *pnBytesRead += nToRead;

                nRemain -= nToRead;
                lpDest += nToRead;
                BufferPos += nToRead;
            }
        }
    }

    return ret;
}

BOOL CBufferedFile::Write(LPCVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten, char* fileName, HWND parent)
{
    DWORD nRemain = nBytesToWrite;
    BYTE* lpSrc = (BYTE*)lpBuffer;
    *pnBytesWritten = 0;

    while (nRemain > 0)
    {
        if (BufferPos + nRemain < BufferSize)
        {
            memcpy(Buffer + BufferPos, lpSrc, nRemain);
            *pnBytesWritten += nRemain;
            BufferPos += nRemain;
            nRemain = 0;
        }
        else
        {
            int nToWrite = BufferSize - BufferPos;
            memcpy(Buffer + BufferPos, lpSrc, nToWrite);
            BufferPos = 0;
            nRemain -= nToWrite;
            lpSrc += nToWrite;
            *pnBytesWritten += nToWrite;

            LONG offsetHigh = 0;
            BufferStart = ::SetFilePointer(File, 0, &offsetHigh, FILE_CURRENT);
            BufferStart += ((__int64)(DWORD)offsetHigh) << 32;
            DWORD nWritten;
            if (!SafeWriteFile(File, Buffer, BufferSize, &nWritten, fileName, parent))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

BOOL CBufferedFile::Close(LPCTSTR fileName, HWND parent)
{
    // flush buffers
    if ((Access & GENERIC_WRITE) && Buffer != NULL && BufferPos > 0)
    {
        DWORD written;
        if (!SafeWriteFile(File, Buffer, BufferPos, &written, fileName, parent))
            return FALSE;
        BufferPos = 0;
    }

    BOOL ret = CloseHandle(File);
    File = NULL;

    return ret;
}

BOOL CFile::SetFileTime(const FILETIME* lpCreationTime, const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime)
{
    return ::SetFileTime(File, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}

BOOL CFile::GetFileTime(LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime)
{
    return ::GetFileTime(File, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}

DWORD CBufferedFile::GetFileSize(LPDWORD lpFileSizeHigh)
{
    return ::GetFileSize(File, lpFileSizeHigh);
}
