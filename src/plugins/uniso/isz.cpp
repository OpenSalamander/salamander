// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "isz.h"

#include "uniso.h"
#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

static int ISZ2BF_BlockType(int blockType)
{
    switch (blockType)
    {
    case ISZ_BLOCKTYPE_ZERO:
        return BF_BLOCKTYPE_ZERO;
    case ISZ_BLOCKTYPE_COPY:
        return BF_BLOCKTYPE_COPY;
    case ISZ_BLOCKTYPE_ZLIB:
        return BF_BLOCKTYPE_ZLIB;
    case ISZ_BLOCKTYPE_BZIP2:
        return BF_BLOCKTYPE_BZIP2;
    default:
        return BF_BLOCKTYPE_UNKNOWN;
    }
}

static void DecodeBlock(void* buf, int size)
{
    static UInt8 ISZ_KEY[] = "IsZ!";
    UInt8* ptr = (UInt8*)buf;
    UInt8* key = ISZ_KEY;

    while (size-- > 0)
    {
        *ptr++ ^= ~*key++;
        if (!*key)
            key = ISZ_KEY;
    }
}

CISZFile::CISZFile(HANDLE hFile, BOOL quiet)
{
    CALL_STACK_MESSAGE2("CISZFile::CISZFile(%p,)", hFile);
    ISZHeader header;
    DWORD dwBytesRead, posLo, size;
    UInt64 inPos, outPos;
    UInt8 *pChunks, *pCurChunk;

    File = hFile;
    pBlocks = NULL;
    nBlocks = 0;
    pCurBlock = 0;
    memset(BlockCache, 0, sizeof(BlockCache));
    CurrentPos = FileSize = 0;
    MRUCachedBlock = 0;

    posLo = SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if (!ReadFile(hFile, &header, sizeof(header), &dwBytesRead, NULL) || (sizeof(header) != dwBytesRead))
    {
        // Too short file
        Error(IDS_ERR_ISZ_READ_HDR, FALSE);
        return;
    }
    if ((header.magic != ISZ_HEADER_MAGIC) || !header.ptr_offs)
    {
        // Not an ISZ or damaged file
        Error(IDS_ERR_ISZ_NO_HEADER, quiet);
        return;
    }
    if (header.has_password != ISZ_CRYPT_PLAIN)
    {
        Error(IDS_ERR_ISZ_ENCRYPTED, quiet);
        return;
    }
    if (header.seg_offs != 0)
    {
        // NOTE: the data at seg_offs have to be decoded using DecodeBlock()
        Error(IDS_ERR_ISZ_SEGMENTED, quiet);
        return;
    }
    posLo = SetFilePointer(hFile, header.ptr_offs, NULL, FILE_BEGIN);
    if (posLo != header.ptr_offs)
    {
        Error(IDS_ERR_BF_SEEK, quiet, header.ptr_offs);
        return;
    }
    nBlocks = header.nblocks;
    pBlocks = (BlockInfo*)malloc(nBlocks * sizeof(BlockInfo));
    if (!pBlocks)
    {
        Error(IDS_INSUFFICIENT_MEMORY, quiet);
        return;
    }
    inPos = header.data_offs;
    outPos = 0;
    size = nBlocks * header.ptr_len;
    pCurChunk = pChunks = (UInt8*)malloc(size);
    if (!pChunks)
    {
        Error(IDS_INSUFFICIENT_MEMORY, quiet);
        free(pBlocks);
        return;
    }
    if (!ReadFile(hFile, pChunks, size, &dwBytesRead, NULL) || (size != dwBytesRead))
    {
        // Not an ISZ or damaged file
        Error(IDS_ERR_ISZ_NO_HEADER, quiet);
        free(pChunks);
        free(pBlocks);
        pBlocks = NULL;
        return;
    }
    DecodeBlock(pChunks, size);
    int i;
    for (i = 0; i < nBlocks; i++)
    {
        pBlocks[i].inSize = (pCurChunk[header.ptr_len - 1] & 0x3f) << ((header.ptr_len - 1) * 8);
        int j;
        for (j = 0; j < header.ptr_len - 1; j++)
        {
            pBlocks[i].inSize += ((UInt32)pCurChunk[j]) << (j * 8);
        }
        pBlocks[i].inPos = inPos;
        pBlocks[i].outPos = outPos;
        pBlocks[i].outSize = header.block_size;
        pBlocks[i].blockType = ISZ2BF_BlockType(pCurChunk[header.ptr_len - 1] & 0xC0);
        if (pBlocks[i].blockType != ISZ_BLOCKTYPE_ZERO)
        {
            inPos += pBlocks[i].inSize;
        }
        outPos += header.block_size;
        pCurChunk += header.ptr_len;
    }
    size = (header.block_size / header.sect_size) * nBlocks;
    if (size > header.total_sectors)
    {
        // The image has less sectors than is contained in all chunks
        size -= header.total_sectors;
        size *= header.sect_size;
        // size is the extra size in all blocks
        // make the last block smaller by that size. We assume that there are no extra (empty) blocks
        if (size < header.block_size)
        {
            pBlocks[nBlocks - 1].outSize -= size;
            outPos -= size;
        }
    }
    FileSize = outPos;
    free(pChunks);
}

CISZFile::~CISZFile()
{
    Close(NULL, NULL);
}
