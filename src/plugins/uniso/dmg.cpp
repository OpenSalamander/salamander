// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dmg.h"

#include "uniso.h"
#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#define PLIST_START "<?xml version="
#define PLIST_ARRAY "<array>"
#define PLIST_ARRAY_END "</array>"
#define PLIST_DATA "<data>"
#define PLIST_DATA_END "</data>"
#define PLIST_KEY_BLKX "<key>blkx</key>"
#define PLIST_APPLE_HFS "Apple_HFS"
#define PLIST_APPLE_APFS "Apple_APFS"

// Partially based on information acquired from dmg2iso by vu1tur
// ADC compression scheme description taken from http://www.macdisk.com/dmgen.php

static void RemoveWS(char* in)
{
    char* out = in;
    while (*in)
    {
        if ((*in != 9) && (*in != 10) && (*in != 13) && (*in != 32))
            *out++ = *in;
        in++;
    }
    *out = 0;
}

static UInt8 UnBase64Char(char c)
{
    if ((c >= 'A') && (c <= 'Z'))
        return c - 'A';
    if ((c >= 'a') && (c <= 'z'))
        return c - 'a' + 26;
    if ((c >= '0') && (c <= '9'))
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '=')
        return 0;
    return 63;
}

#pragma runtime_checks("c", off)

static int UnBase64(char* s)
{
    UInt8 *orig, *out;

    orig = out = (UInt8*)s;

    while (*s)
    {
        UInt8 b1 = UnBase64Char(*s++);
        UInt8 b2 = UnBase64Char(*s++);
        UInt8 b3 = UnBase64Char(*s++);
        UInt8 b4 = UnBase64Char(*s++);
        *out++ = (b2 >> 4) | (b1 << 2);
        *out++ = (b3 >> 2) | (b2 << 4);
        *out++ = b4 | (b3 << 6);
    }
    // NOTE: we don't care about the last extra 1 or 2 or 3 bytes we added
    return (int)(out - orig);
}

#pragma runtime_checks("", restore)

static int DMG2BF_BlockType(int bt)
{
    switch (bt)
    {
    case DMG_BLOCKTYPE_ZERO:
    case DMG_BLOCKTYPE_ZERO2:
        return BF_BLOCKTYPE_ZERO;
    case DMG_BLOCKTYPE_ADC:
        return BF_BLOCKTYPE_ADC;
    case DMG_BLOCKTYPE_ZLIB:
        return BF_BLOCKTYPE_ZLIB;
    case DMG_BLOCKTYPE_BZIP2:
        return BF_BLOCKTYPE_BZIP2;
    case DMG_BLOCKTYPE_COPY:
        return BF_BLOCKTYPE_COPY;
    default:
        return BF_BLOCKTYPE_UNKNOWN;
    }
}

CDMGFile::CDMGFile(HANDLE hFile, BOOL quiet)
{
    CALL_STACK_MESSAGE2("CDMGFile::CDMGFile(%p,)", hFile);
    DMGFooter footer;
    DWORD posLo, dwBytesRead;
    LONG posHi = -1 /*hi part of 64bit -512*/;
    UInt64 ofs;
    char *plist, *partList;
    size_t plistSize;

    File = hFile;
    pBlocks = NULL;
    nBlocks = 0;
    pCurBlock = 0;
    memset(BlockCache, 0, sizeof(BlockCache));
    CurrentPos = FileSize = 0;
    MRUCachedBlock = 0;

    posLo = SetFilePointer(hFile, -512, &posHi, FILE_END);
    if (!ReadFile(hFile, &footer, sizeof(footer), &dwBytesRead, NULL) || (sizeof(footer) != dwBytesRead))
    {
        // Too short file
        Error(IDS_ERR_DMG_READ_HDR, FALSE);
        return;
    }
    if (footer.magic != DMG_FOOTER_MAGIC)
    {
        if (!quiet)
        {
            // Check if encrypted
            char cdsaencr[8];

            posHi = -1;
            SetFilePointer(hFile, -8, &posHi, FILE_END);
            ReadFile(hFile, cdsaencr, sizeof(cdsaencr), &dwBytesRead, NULL);
            if (!memcmp(cdsaencr, "cdsaencr", sizeof("cdsaencr") - 1))
            {
                Error(IDS_ERR_DMG_ENCRYPTED, quiet);
                return;
            }
        }
        // Not a DMG or damaged file
        Error(IDS_ERR_DMG_NO_FOOTER, quiet);
        return;
    }
    ofs = FromM64(footer.ofsPLIST);
    plistSize = (size_t)FromM64(footer.sizePLIST);
    if (!plistSize)
    {
        plistSize = (size_t)(((UInt64)posHi << 32) + posLo - ofs);
    }
    else
    {
        plistSize -= (size_t)ofs;
    }
    plist = (char*)malloc(plistSize + 1);
    if (!plist)
    {
        Error(IDS_INSUFFICIENT_MEMORY, quiet);
        return;
    }
    posHi = (LONG)(ofs >> 32);
    SetFilePointer(hFile, (DWORD)(ofs & 0xffffffff), &posHi, FILE_BEGIN);
    if (!ReadFile(hFile, plist, (DWORD)plistSize, &dwBytesRead, NULL) || (plistSize != dwBytesRead))
    {
        free(plist);
        Error(IDS_ERR_DMG_PLIST, quiet);
        return;
    }
    plist[plistSize] = -1;
    if (memcmp(plist, PLIST_START, sizeof(PLIST_START) - 1))
    {
        // XMP PLIST is not where expected to be
        free(plist);
        Error(IDS_ERR_DMG_PLIST, quiet);
        return;
    }

    // Partitions are listed in <array> for "blkx" key
    char* blkxArray = strstr(plist, PLIST_KEY_BLKX);
    if (!blkxArray)
    {
        // Can this happen? No list of partitions?
        blkxArray = plist;
    }
    partList = strstr(blkxArray, PLIST_ARRAY);
    if (!partList)
        partList = blkxArray;

    char* s = strstr(partList, PLIST_ARRAY_END);
    if (s)
    {
        *s = 0; // Avoid parsing additional <array>'s
        // E.g. Adobe creates DMG with EFI file system. There are lots of various partitions,
        // one among them is HFS+ or APFS. To avoid dealing with EFI, we rather extract only the HFS+/APFS one.
        // We assume there are no HFS+ & APFS partitions at the same time. If so, we take the HFS+ one.
        bool bAPFS = false;
        s = strstr(partList, PLIST_APPLE_HFS);
        if (!s)
        {
            // No HFS+? Search for APFS
            s = strstr(partList, PLIST_APPLE_APFS);
            bAPFS = true;
        }
        if (s)
        {
            // At least one partition is marked as Apple_HFS or Apple_APFS
            char* s1 = strstr(s + 1, bAPFS ? PLIST_APPLE_APFS : PLIST_APPLE_HFS);
            if (s1)
            {
                // Either two such partitions or a single part labeled twice (once before and once after <data>)
                char* s2 = strstr(s, PLIST_DATA);
                if (s2 && (s2 < s1))
                {
                    s2 = strstr(s2 + 1, PLIST_DATA);
                    if (!s2 || (s2 > s1))
                    {
                        // Single <data> between Apple_HFS/APFS -> single partition -> take the later Apple_HFS/APFS
                        s = s1;
                    }
                }
            }
            *s = 0;
            // Find the last <data> before Apple_HFS/APFS
            s1 = s = partList + 1;
            while (NULL != (s1 = strstr(s, PLIST_DATA)))
            {
                s = s1 + 1;
            }
            partList = s - 1;
        }
    }
    UInt64 partOfs = 0; // Offset of partition in decompressed image
    // Scan over all <data>'s. Ideally when Apple_HFS/APFS was found, only over one
    for (;;)
    {
        char *partData, *partDataEnd;
        int partDataLen;

        partData = strstr(partList, PLIST_DATA);
        if (!partData)
        {
            // No more partitions
            break;
        }
        partData += sizeof(PLIST_DATA) - 1;
        partDataEnd = strstr(partData, PLIST_DATA_END);
        if (!partDataEnd)
        {
            // Invalid data???
            break;
        }
        partDataEnd[0] = partDataEnd[1] = partDataEnd[2] = partDataEnd[3] = 0;
        RemoveWS(partData);
        partDataLen = UnBase64(partData);

        // Check that the decoded <data> is 'mish' describing a partition
        if (*(UInt32*)partData == DMG_PARTITION_INFO_MAGIC)
        {
            // This is a partition info
            int maxBlocks = (partDataLen - 0xCC) / 0x28;
            UInt64 addOfs = FromM64(*(UInt64*)(partData + 24)); // Start of partition in DMG

            DMGBlockInfo* pBlock = (DMGBlockInfo*)(partData + 0xCC);

            pBlocks = (BlockInfo*)realloc(pBlocks, sizeof(BlockInfo) * (nBlocks + maxBlocks));
            if (!pBlocks)
            {
                break;
            }
            while (maxBlocks-- > 0)
            {
                int bt = FromM32(pBlock->blockType);
                pBlocks[nBlocks].blockType = DMG2BF_BlockType(bt);
                if (bt == DMG_BLOCKTYPE_END)
                {
                    break;
                }
                pBlocks[nBlocks].outPos = partOfs + FromM64(pBlock->outPos) * 0x200;
                pBlocks[nBlocks].inPos = FromM64(pBlock->inPos) + addOfs;
                pBlocks[nBlocks].outSize = FromM64(pBlock->outSize) * 0x200;
                pBlocks[nBlocks].inSize = FromM64(pBlock->inSize);
                pBlock++;
                if (pBlocks[nBlocks].outSize > 0)
                {
                    if ((DMG_BLOCKTYPE_COMMENT != bt) && (DMG_BLOCKTYPE_ZLIB != bt) && (DMG_BLOCKTYPE_COPY != bt) && (DMG_BLOCKTYPE_ZERO != bt) && (DMG_BLOCKTYPE_ADC != bt) && (DMG_BLOCKTYPE_ZERO2 != bt) && (DMG_BLOCKTYPE_BZIP2 != bt))
                    {
                        free(plist);
                        free(pBlocks);
                        pBlocks = NULL;
                        Error(IDS_ERR_BF_BLOCK_UNK_TYPE, quiet);
                        return;
                    }
                    nBlocks++;
                }
            }
            if (nBlocks > 0)
                partOfs = pBlocks[nBlocks - 1].outPos + pBlocks[nBlocks - 1].outSize;
        }
        partList = partDataEnd + 4;
    }
    FileSize = partOfs;
    free(plist);
}

CDMGFile::~CDMGFile()
{
    Close(NULL, NULL);
}

// Apple Data Compression, see http://www.macdisk.com/dmgen.php
CDMGFile::CADCBlock::CADCBlock(BlockInfo* blockInfo, HANDLE hFile)
{
    CALL_STACK_MESSAGE3("CADCBlock::CADCBlock(%p, %p)", blockInfo, hFile);
    BYTE *inBuf, *in, *out;
    LONG posHi;
    DWORD dwBytesRead;
    int size;

    pBlockInfo = blockInfo;

    Buffer = (char*)malloc((size_t)blockInfo->outSize);
    if (!Buffer)
    {
        Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        return;
    }
    in = inBuf = (BYTE*)malloc((size_t)blockInfo->inSize);
    if (!inBuf)
    {
        free(Buffer);
        Buffer = NULL;
        Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        return;
    }
    size = (int)blockInfo->inSize;
    posHi = (LONG)(blockInfo->inPos >> 32);
    if ((0xFFFFFFFF == SetFilePointer(hFile, (DWORD)(blockInfo->inPos & 0xFFFFFFFF), &posHi, FILE_BEGIN)) || !ReadFile(hFile, inBuf, size, &dwBytesRead, NULL) || (blockInfo->inSize != dwBytesRead))
    {
        free(inBuf);
        free(Buffer);
        Buffer = NULL;
        Error(IDS_ERR_BF_READ, FALSE, blockInfo->inPos);
        return;
    }
    out = (BYTE*)Buffer;
    while (size > 0)
    {
        if (*in & 0x80)
        {
            int len = (*in++ & 0x7f) + 1;
            memcpy(out, in, len);
            out += len;
            in += len;
            size -= len + 1;
        }
        else if (*in & 0x40)
        {
            int len = (*in++ & 0x3f) + 4;
            int ofs = in[1] + ((DWORD)in[0] << 8) + 1;
            BYTE* src = out - ofs;
            in += 2;
            if (len <= ofs)
            {
                memcpy(out, src, len);
                out += len;
            }
            else
            {
                // NOTE: memcpy & memmove cannot be used in general, they read by dwords
                // NOTE: http://www.macdisk.com/dmgen.php says something about memset... but it doesn't work - see SymArt_Demo.dmg
                while (len-- > 0)
                    *out++ = *src++;
            }
            size -= 3;
        }
        else
        {
            int len = (*in >> 2) + 3;
            int ofs = ((DWORD)(in[0] & 3) << 8) + in[1] + 1;
            BYTE* src = out - ofs;
            in += 2;
            if (len <= ofs)
            {
                memcpy(out, src, len);
                out += len;
            }
            else
            {
                // NOTE: memcpy & memmove cannot be used in general, they read by dwords
                // NOTE: http://www.macdisk.com/dmgen.php says something about memset... but it doesn't work - see SymArt_Demo.dmg
                while (len-- > 0)
                    *out++ = *src++;
            }
            size -= 2;
        }
    }

    free(inBuf);
}

CDMGFile::CADCBlock::~CADCBlock()
{
    if (Buffer)
        free(Buffer);
}

DWORD CDMGFile::CADCBlock::Read(char* buf, DWORD BytesToRead)
{
    if (!Buffer)
        return 0;

    memcpy(buf, Buffer + PosInBuf, BytesToRead);
    PosInBuf += BytesToRead;
    return BytesToRead;
}
