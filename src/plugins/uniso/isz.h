// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "BlockedFile.h"
#include "hfs.h" // To get UInt64 and other types

//****************************************************************************
//
// CISZFile - Reading from compressed ISO image
//            NOTE: only Deflating (ZLIB) compression is supported
//
//

#define ISZ_HEADER_MAGIC 0x215a7349 // 'IsZ!'

#define ISZ_BLOCKTYPE_ZERO 0
#define ISZ_BLOCKTYPE_COPY 0x40
#define ISZ_BLOCKTYPE_ZLIB 0x80
#define ISZ_BLOCKTYPE_BZIP2 0xC0

// Value of has_password
#define ISZ_CRYPT_PLAIN 0    // no encryption
#define ISZ_CRYPT_PASSWORD 1 // password protected (not used)
#define ISZ_CRYPT_AES128 2   // AES128 encryption
#define ISZ_CRYPT_AES192 3   // AES192 encryption
#define ISZ_CRYPT_AES256 4   // AES256 encryption

class CISZFile : public CBlockedFile
{

private:
#pragma pack(push, 1)
    struct ISZHeader
    {
        UInt32 magic;      // ISZ_HEADER_MAGIC='IsZ!'
        UInt8 header_size; // header size in bytes
        char ver;          // version number
        UInt32 vsn;        // volume serial number

        UInt16 sect_size;     // sector size in bytes
        UInt32 total_sectors; // total sectors of ISO image

        char has_password; // is Password protected?

        SInt64 segment_size; // size of segments in bytes

        UInt32 nblocks;    // number of chunks in image
        UInt32 block_size; // chunk size in bytes (must be multiple of sector_size)
        UInt8 ptr_len;     // chunk pointer length

        char seg_no; // segment number of this segment file, max 99

        UInt32 ptr_offs; // offset of chunk pointers, zero = none

        UInt32 seg_offs; // offset of segment pointers, zero = none

        UInt32 data_offs; // data offset

        char reserved;
    };

    struct ISZSegmentInfo
    {                       // Member of Segment Definition Table (SDT) pointed to by seg_offs
        SInt64 size;        // segment size in bytes
        SInt32 num_chks;    // number of chunks in current file
        SInt32 first_chkno; // first chunk number in current file
        SInt32 chk_off;     // offset to first chunk in current file
        SInt32 left_size;   // uncompltete chunk bytes in next file
    };

#pragma pack(pop)

public:
    CISZFile(HANDLE hFile, BOOL quiet);
    virtual ~CISZFile();

    friend class CBlockedFile;
};
