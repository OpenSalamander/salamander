// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <assert.h>

#define SECTOR_SIZE 4096              // We seek at offsets being multiples of SECTOR_SIZE
#define BUFFER_SIZE (2 * 1024 * 1024) // We read at most this amount from a file at a sequence of BLOCK_READ_SIZE reads
#define BLOCK_READ_SIZE (32 * 1024)   // Atomic size of one ReadFile. BUFFER_SIZE must be a multiple of BLOCK_READ_SIZE

CCachedFile::CCachedFile(/*DWORD minViewSize*/)
{
    CALL_STACK_MESSAGE1("CCachedFile::CCachedFile" /*, minViewSize*/);
    File = INVALID_HANDLE_VALUE;
    BufferOffset = FileSize = 0;
    BufferSize = DataInBufSize = 0;
    Buffer = NULL;
}

void CCachedFile::Destroy()
{
    CALL_STACK_MESSAGE1("CCachedFile::Destroy()");
    if (File != INVALID_HANDLE_VALUE)
    {
        CloseHandle(File);
        File = INVALID_HANDLE_VALUE;
    }
    if (Buffer)
    {
        free(Buffer);
        Buffer = NULL;
        BufferSize = 0;
    }
}

DWORD CCachedFile::GetMinViewSize()
{
    return BLOCK_READ_SIZE;
}

int CCachedFile::SetFile(HANDLE file)
{
    BY_HANDLE_FILE_INFORMATION fi;

    CALL_STACK_MESSAGE1("CCachedFile::SetFile(, )");
    Destroy();
    if (!GetFileInformationByHandle(file, &fi))
    {
        return 1;
    }
    // Allocate slightly more to accomodate aligned seeking in ReadBuffer
    Buffer = (LPBYTE)malloc(BUFFER_SIZE + SECTOR_SIZE);
    if (!Buffer)
    {
        return 2;
    }
    BufferSize = BUFFER_SIZE;
    FileSize = MAKEQWORD(fi.nFileSizeLow, fi.nFileSizeHigh);
    if (!DuplicateHandle(GetCurrentProcess(), file, GetCurrentProcess(), &File, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        return 1;
    }
    return 0;
}

LPBYTE CCachedFile::ReadBuffer(QWORD offset, DWORD size, const int& CancelFlag)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CCachedFile::ReadBuffer(, 0x%X)", size);
    if (FileSize == 0)
        return NULL;

    if ((offset < BufferOffset) || (offset + size > BufferOffset + DataInBufSize))
    {
        LARGE_INTEGER li;
        ;
        DWORD err;

        // Space for optimization: Instead of dummy SetFilePointer and possibly partially rereading
        // what we already have, we can do some memcpy. But then we might not read from sector boundary
        BufferOffset = offset / SECTOR_SIZE * SECTOR_SIZE; // Align to "cluster" size
        li.QuadPart = BufferOffset;
        DataInBufSize = 0;
        err = SetFilePointer(File, li.LowPart, &li.HighPart, FILE_BEGIN);
        if ((0xFFFFFFFF == err) && (GetLastError() != NO_ERROR))
        {
            return NULL;
        }

        size_t nOfs = 0;
        DWORD nTotalToRead = (DWORD)__max(size + (offset - BufferOffset), (DWORD)__min(BufferSize, FileSize - BufferOffset));
        DWORD initialTicks = GetTickCount();

        // Reading 32KB chunks bases on code for File Compare in Salamand.exe
        // Reading 2MB in 32KB blocks is reportedly the fastest approach on W2K & WXP,
        // but slightly slower on Vista, when both files reside on the same HDD,
        // than reading 2MB at once.
        // Reading small chunks also improves performance on slow networks.
        while (nTotalToRead > 0)
        {
            DWORD nBytesRead, nBytesToRead = __min(nTotalToRead, BLOCK_READ_SIZE);
            DWORD ticks = GetTickCount();
            if (!ReadFile(File, Buffer + nOfs, nBytesToRead, &nBytesRead, NULL) || (nBytesRead != nBytesToRead))
            {
                return NULL;
            }
            nOfs += nBytesRead;
            nTotalToRead -= nBytesRead;
            DataInBufSize += nBytesRead;
            if (CancelFlag)
            {
                return NULL;
            }
            if ((GetTickCount() - ticks > 200) || (GetTickCount() - initialTicks > 2000))
            {
                // Too slow network (or FDD or ...)
                // We have read enough -> break
                assert(offset - BufferOffset + size <= DataInBufSize);
                if (offset - BufferOffset + size <= DataInBufSize)
                {
                    break;
                }
            }
        }
    }
    return Buffer + (offset - BufferOffset);
}
