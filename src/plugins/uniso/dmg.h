// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "BlockedFile.h"
#include "hfs.h" // To get UInt64 and other types

//****************************************************************************
//
// CDMGFile - Reading from compressed Apple DMG package
//            NOTE: only Deflating (ZLIB) compression is supported
//
//

#define DMG_FOOTER_MAGIC 0x796c6f6b         // 'koly', 512 bytes from EOF
#define DMG_FOOTER_ENCR 0x72636e65          // 'encr', 4 bytes from EOF
#define DMG_PARTITION_INFO_MAGIC 0x6873696d /* 'mish' */

#define DMG_BLOCKTYPE_ZERO2 0x00000000 // Normally not used
#define DMG_BLOCKTYPE_ADC 0x80000004   // Since Mac OS 10.0.0
#define DMG_BLOCKTYPE_ZLIB 0x80000005  // Since Mac OS 10.1.0
#define DMG_BLOCKTYPE_BZIP2 0x80000006 // Since Mac OS 10.4.0
#define DMG_BLOCKTYPE_COPY 0x00000001
#define DMG_BLOCKTYPE_ZERO 0x00000002
#define DMG_BLOCKTYPE_COMMENT 0x7FFFFFFE
#define DMG_BLOCKTYPE_END 0xffffffff

class CDMGFile : public CBlockedFile
{

private:
#pragma pack(push, 1)
    struct DMGFooter
    {
        UInt32 magic; // DMG_FOOTER_MAGIC='koly'
        UInt32 unknown[7];
        UInt64 ofsPLIST;
        UInt64 sizePLIST; // Sometimes 0
                          // The rest upto 512 bytes in total is also unknown
    };

    struct DMGBlockInfo
    {
        UInt32 blockType;
        UInt32 unknown;
        UInt64 outPos;
        UInt64 outSize;
        UInt64 inPos;
        UInt64 inSize;
    };

#pragma pack(pop)

    class CADCBlock : public CBlockedFile::CCachedBlock
    { // Apple Data Compression
    public:
        CADCBlock(BlockInfo* blockInfo, HANDLE hFile);
        virtual ~CADCBlock();
        virtual DWORD Read(char* buf, DWORD BytesToRead);

        bool IsOK() { return Buffer ? true : false; };

    private:
        char* Buffer;
    };

public:
    CDMGFile(HANDLE hFile, BOOL quiet);
    virtual ~CDMGFile();

    friend class CBlockedFile;
};
