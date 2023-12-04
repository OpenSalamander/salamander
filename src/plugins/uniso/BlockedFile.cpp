// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "BlockedFile.h"
#include "dmg.h"

#include "uniso.h"
#include "uniso.rh"
#include "uniso.rh2"
#include "lang/lang.rh"

CBlockedFile::CCachedBlock::CCachedBlock()
{
    PosInBuf = 0;
}

CBlockedFile::CZeroBlock::CZeroBlock(BlockInfo* blockInfo)
{
    pBlockInfo = blockInfo;
}

CBlockedFile::CZeroBlock::~CZeroBlock()
{
}

DWORD CBlockedFile::CZeroBlock::Read(char* buf, DWORD BytesToRead)
{
    memset(buf, 0, BytesToRead);
    PosInBuf += BytesToRead;
    return BytesToRead;
}

CBlockedFile::CZLIBBlock::CZLIBBlock(BlockInfo* blockInfo, HANDLE hFile)
{
    CALL_STACK_MESSAGE3("CZLIBBlock::CZLIBBlock(%p, %p)", blockInfo, hFile);
    CSalZLIB zi;
    BYTE* inBuf;
    LONG posHi;
    DWORD dwBytesRead;
    int err;

    pBlockInfo = blockInfo;

    Buffer = (char*)malloc((size_t)blockInfo->outSize);
    if (!Buffer)
    {
        Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        return;
    }
    inBuf = (BYTE*)malloc((size_t)blockInfo->inSize);
    if (!inBuf)
    {
        free(Buffer);
        Buffer = NULL;
        Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        return;
    }
    posHi = (LONG)(blockInfo->inPos >> 32);
    if ((0xFFFFFFFF == SetFilePointer(hFile, (DWORD)(blockInfo->inPos & 0xFFFFFFFF), &posHi, FILE_BEGIN)) || !ReadFile(hFile, inBuf, (DWORD)blockInfo->inSize, &dwBytesRead, NULL) || (blockInfo->inSize != dwBytesRead))
    {
        free(inBuf);
        free(Buffer);
        Buffer = NULL;
        Error(IDS_ERR_BF_READ, FALSE, blockInfo->inPos);
        return;
    }
    if (SAL_Z_OK != SalZLIB->InflateInit(&zi))
    {
        free(inBuf);
        free(Buffer);
        Buffer = NULL;
        Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        return;
    }
    zi.avail_in = (UINT)blockInfo->inSize;
    zi.next_in = inBuf;
    zi.next_out = (BYTE*)Buffer;
    zi.avail_out = (UINT)blockInfo->outSize;
    err = SalZLIB->Inflate(&zi, SAL_Z_FINISH);
    SalZLIB->InflateEnd(&zi);
    free(inBuf);
    if ((err != SAL_Z_STREAM_END) || (zi.avail_out != 0))
    {
        free(Buffer);
        Buffer = NULL;
        Error(IDS_ERR_BF_ZLIB, FALSE);
    }
}

CBlockedFile::CZLIBBlock::~CZLIBBlock()
{
    if (Buffer)
        free(Buffer);
}

DWORD CBlockedFile::CZLIBBlock::Read(char* buf, DWORD BytesToRead)
{
    if (!Buffer)
        return 0;

    memcpy(buf, Buffer + PosInBuf, BytesToRead);
    PosInBuf += BytesToRead;
    return BytesToRead;
}

CBlockedFile::CBZIP2Block::CBZIP2Block(BlockInfo* blockInfo, HANDLE hFile)
{
    CALL_STACK_MESSAGE3("CBZIP2Block::CBZIP2Block(%p, %p)", blockInfo, hFile);
    CSalBZIP2 bzi;
    BYTE* inBuf;
    LONG posHi;
    DWORD dwBytesRead;
    int err;

    pBlockInfo = blockInfo;

    Buffer = (char*)malloc((size_t)blockInfo->outSize);
    if (!Buffer)
    {
        Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        return;
    }
    inBuf = (BYTE*)malloc((size_t)blockInfo->inSize);
    if (!inBuf)
    {
        free(Buffer);
        Buffer = NULL;
        Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        return;
    }
    posHi = (LONG)(blockInfo->inPos >> 32);
    if ((0xFFFFFFFF == SetFilePointer(hFile, (DWORD)(blockInfo->inPos & 0xFFFFFFFF), &posHi, FILE_BEGIN)) || !ReadFile(hFile, inBuf, (DWORD)blockInfo->inSize, &dwBytesRead, NULL) || (blockInfo->inSize != dwBytesRead))
    {
        free(inBuf);
        free(Buffer);
        Buffer = NULL;
        Error(IDS_ERR_BF_READ, FALSE, blockInfo->inPos);
        return;
    }
    if ((dwBytesRead > 3) && !memcmp(inBuf, "ISz", 3))
    {
        // ISZ files use proprietary magic instead of the default one :-(
        memcpy(inBuf, "BZh", 3);
    }
    if (SAL_BZ_OK != SalBZIP2->DecompressInit(&bzi, false))
    {
        free(inBuf);
        free(Buffer);
        Buffer = NULL;
        Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        return;
    }
    bzi.avail_in = (UINT)blockInfo->inSize;
    bzi.next_in = inBuf;
    bzi.next_out = (BYTE*)Buffer;
    bzi.avail_out = (UINT)blockInfo->outSize;
    err = SalBZIP2->Decompress(&bzi);
    SalBZIP2->DecompressEnd(&bzi);
    free(inBuf);
    if ((err != SAL_BZ_STREAM_END) || (bzi.avail_out != 0))
    {
        free(Buffer);
        Buffer = NULL;
        Error(IDS_ERR_BF_BZIP2, FALSE);
    }
}

CBlockedFile::CBZIP2Block::~CBZIP2Block()
{
    if (Buffer)
        free(Buffer);
}

DWORD CBlockedFile::CBZIP2Block::Read(char* buf, DWORD BytesToRead)
{
    if (!Buffer)
        return 0;

    memcpy(buf, Buffer + PosInBuf, BytesToRead);
    PosInBuf += BytesToRead;
    return BytesToRead;
}

CBlockedFile::CCopyBlock::CCopyBlock(BlockInfo* blockInfo, HANDLE hFile) : File(hFile)
{
    CALL_STACK_MESSAGE3("CCopyBlock::CCopyBlock(%p, %p)", blockInfo, hFile);
    pBlockInfo = blockInfo;
}

CBlockedFile::CCopyBlock::~CCopyBlock()
{
}

DWORD CBlockedFile::CCopyBlock::Read(char* buf, DWORD BytesToRead)
{
    UInt64 pos = pBlockInfo->inPos + PosInBuf;
    LONG posHi = (DWORD)(pos >> 32);
    DWORD dwBytesRead;

    if ((0xFFFFFFFF == SetFilePointer(File, (DWORD)(pos & 0xffffffff), &posHi, FILE_BEGIN)) && (GetLastError() != NO_ERROR))
    {
        Error(IDS_ERR_BF_SEEK, FALSE, pos);
        return 0;
    }
    if (!ReadFile(File, buf, BytesToRead, &dwBytesRead, NULL))
    {
        Error(IDS_ERR_BF_READ, FALSE);
        return 0;
    }
    PosInBuf += dwBytesRead;
    return dwBytesRead;
}

CBlockedFile::CBlockedFile()
{
    pBlocks = NULL;
    nBlocks = 0;

    MRUCachedBlock = 0;
    pCurBlock = NULL;

    bUnknownBlockErrShown = false;
}

CBlockedFile::~CBlockedFile()
{
    if (pBlocks)
        free(pBlocks);
    for (int i = 0; i < SizeOf(BlockCache); i++)
    {
        delete BlockCache[i];
    }
}

BOOL CBlockedFile::IsOK()
{
    return pBlocks ? TRUE : FALSE;
}

BOOL CBlockedFile::Read(LPVOID lpBuffer, DWORD nBytesToRead, DWORD* pnBytesRead, const char* fileName, HWND parent)
{
    char* buf = (char*)lpBuffer;

    if (pnBytesRead)
        *pnBytesRead = 0;

    while (nBytesToRead > 0)
    {
        if (!pCurBlock || (CurrentPos < pCurBlock->pBlockInfo->outPos) || (CurrentPos >= pCurBlock->pBlockInfo->outPos + pCurBlock->pBlockInfo->outSize))
        {
            pCurBlock = LoadBlock(CurrentPos);
            if (!pCurBlock)
            {
                return FALSE;
            }
            pCurBlock->PosInBuf = (size_t)(CurrentPos - pCurBlock->pBlockInfo->outPos);
        }
        DWORD nBytes = (DWORD)min(nBytesToRead, pCurBlock->pBlockInfo->outSize - pCurBlock->PosInBuf);
        DWORD nBytesRead = pCurBlock->Read(buf, nBytes);
        CurrentPos += nBytesRead;
        nBytesToRead -= nBytesRead;
        buf += nBytesRead;
        if (pnBytesRead)
            *pnBytesRead += nBytesRead;
        if (nBytes != nBytesRead)
        {
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CBlockedFile::Write(LPCVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten, char* fileName, HWND parent)
{
    // Not implemented
    return FALSE;
}

__int64 CBlockedFile::Seek(__int64 lDistanceToMove, DWORD dwMoveMethod)
{
    switch (dwMoveMethod)
    {
    case FILE_BEGIN:
        CurrentPos = lDistanceToMove;
        break;
    case FILE_CURRENT:
        CurrentPos += lDistanceToMove;
        break;

    case FILE_END:
    default:
        // Unsupported
        Error(IDS_ERR_BF_UNSUPPORTED_SEEK_METHOD, FALSE);
        return 0;
    }
    if (CurrentPos > FileSize)
    {
        return 0;
    }
    if (pCurBlock)
    {
        if ((CurrentPos < pCurBlock->pBlockInfo->outPos) || (CurrentPos >= pCurBlock->pBlockInfo->outPos + pCurBlock->pBlockInfo->outSize))
        {
            pCurBlock = NULL;
        }
        else
        {
            pCurBlock->PosInBuf = (size_t)(CurrentPos - pCurBlock->pBlockInfo->outPos);
        }
    }
    return CurrentPos;
}

CBlockedFile::CCachedBlock* CBlockedFile::LoadBlock(UInt64 pos)
{
    CALL_STACK_MESSAGE2("CBlockedFile::LoadBlock(%I64u)", pos);
    for (int i = MRUCachedBlock - 1; i >= 0; i--)
    {
        if ((pos >= BlockCache[i]->pBlockInfo->outPos) && (pos < BlockCache[i]->pBlockInfo->outPos + BlockCache[i]->pBlockInfo->outSize))
        {
            // Found -> put to the end of the list
            CBlockedFile::CCachedBlock* pCachedBlock = BlockCache[i];
            memmove(&BlockCache[i], &BlockCache[i + 1], sizeof(void*) * (MRUCachedBlock - i - 1));
            BlockCache[MRUCachedBlock - 1] = pCachedBlock;
            return pCachedBlock;
        }
    }
    for (int j = 0; j < nBlocks; j++)
    {
        // TODO: better search algorithm
        if ((pos >= pBlocks[j].outPos) && (pos < pBlocks[j].outPos + pBlocks[j].outSize))
        {
            CBlockedFile::CCachedBlock* pCachedBlock;
            CBlockedFile::BlockInfo* pBlock = &pBlocks[j];

            switch (pBlock->blockType)
            {
            case BF_BLOCKTYPE_ZLIB:
            {
                CBlockedFile::CZLIBBlock* pZLIBBlock = new CBlockedFile::CZLIBBlock(pBlock, File);

                if (pZLIBBlock && !pZLIBBlock->IsOK())
                {
                    delete pZLIBBlock;
                    return NULL; // Error reported in CZLIBBlock::CZLIBBlock()
                }
                pCachedBlock = pZLIBBlock;
                break;
            }

            case BF_BLOCKTYPE_BZIP2:
            {
                CBlockedFile::CBZIP2Block* pBZIP2Block = new CBlockedFile::CBZIP2Block(pBlock, File);

                if (pBZIP2Block && !pBZIP2Block->IsOK())
                {
                    delete pBZIP2Block;
                    return NULL; // Error reported in CBZIP2Block::CBZIP2Block()
                }
                pCachedBlock = pBZIP2Block;
                break;
            }

            case BF_BLOCKTYPE_COPY:
                pCachedBlock = new CBlockedFile::CCopyBlock(pBlock, File);
                break;

            case BF_BLOCKTYPE_ADC:
                pCachedBlock = new CDMGFile::CADCBlock(pBlock, File);
                break;

            case BF_BLOCKTYPE_ZERO:
                pCachedBlock = new CBlockedFile::CZeroBlock(pBlock);
                break;

            default: // Should not happen
                Error(IDS_ERR_BF_BLOCK_UNK_TYPE, bUnknownBlockErrShown);
                bUnknownBlockErrShown = true; // Don't show the same error multiple times
                return NULL;
            }
            if (!pCachedBlock)
            {
                Error(IDS_INSUFFICIENT_MEMORY, FALSE);
                return NULL;
            }
            if (MRUCachedBlock >= SizeOf(BlockCache))
            {
                delete BlockCache[0];
                memmove(BlockCache, &BlockCache[1], sizeof(void*) * --MRUCachedBlock);
            }
            BlockCache[MRUCachedBlock++] = pCachedBlock;
            return pCachedBlock;
        }
    }
    Error(IDS_ERR_DMG_BLOCK_UNDEFINED, FALSE, pos);
    return NULL;
}

BOOL CBlockedFile::Close(LPCTSTR fileName, HWND parent)
{
    BOOL ret = TRUE;

    if (File != INVALID_HANDLE_VALUE)
    {
        ret = CloseHandle(File);
        File = INVALID_HANDLE_VALUE;
    }
    return ret;
}

DWORD CBlockedFile::GetFileSize(LPDWORD lpFileSizeHigh)
{
    if (lpFileSizeHigh)
        *lpFileSizeHigh = (DWORD)((FileSize >> 32) & 0xffffffff);
    return (DWORD)(FileSize & 0xffffffff);
}
