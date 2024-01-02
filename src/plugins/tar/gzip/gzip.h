// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// gzip magic numbers
#define GZIP_MAGIC 0x8B1F     // Magic header for gzip files, 1F 8B
#define OLD_GZIP_MAGIC 0x9E1F // Magic header for gzip 0.5 = freeze 1.x

// gzip methods
#define METHOD_DEFLATE 0x08

// gzip flag byte
#define ASCII_FLAG 0x01   // bit 0 set: file probably ascii text
#define CONTINUATION 0x02 // bit 1 set: continuation of multi-part gzip file (or CRC in RFC)
#define EXTRA_FIELD 0x04  // bit 2 set: extra field present
#define ORIG_NAME 0x08    // bit 3 set: original file name present
#define COMMENT 0x10      // bit 4 set: file comment present
#define ENCRYPTED 0x20    // bit 5 set: file is encrypted
#define RESERVED 0xC0     // bit 6,7:   reserved

// tyto struktury odpovidaji datum v souboru, nesmi se proto menit velikost
#pragma pack(push, 1)
// struktury hlavicky gzipu
struct SGZipHeader
{
    unsigned short Magic;
    unsigned char Method;
    unsigned char Flags;
    unsigned long MTime;
    unsigned char XFlags;
    unsigned char OS;
};

struct SGzipContinuationHeader
{
    unsigned short PartNum;
};

struct SGzipExtraHeader
{
    unsigned short FieldLen;
    // zbytek struktury ma nezname delky
    //  ? bytes  optional extra field
    //  ? bytes  optional original file name, zero terminated
    //  ? bytes  optional file comment, zero terminated
    // 12 bytes optional encryption header
    //  ? bytes  compressed data
    //  4 bytes  crc32
    //  4 bytes  uncompressed input size modulo 2^32
};
// dale muzeme mit opet alignment libovolny
#pragma pack(pop)

// forward declaration
struct SHufTable;

class CGZip : public CZippedFile
{
public:
    CGZip(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize);
    virtual ~CGZip();
    BOOL Initialize(unsigned int& errorCode);
    void Cleanup();

    virtual void GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr);

protected:
    CQuadWord TotalCnt; // total number of bytes extracted from archive

    BOOL InProgress;     // current block is not finished yet
    BOOL CopyInProgress; // current operation is copy
    BOOL LastBlock;      // is the current block the last one ?
    unsigned int CopyCount;
    unsigned int CopyDistance; // length and index for copy
    unsigned BlockType;        // block type
    SHufTable* LiteralTable;
    int LiteralBits;
    SHufTable* DistanceTable;
    int DistanceBits; // Huffman tables
    SHufTable* FixedLiteralTable;
    int FixedLiteralBits;
    SHufTable* FixedDistanceTable;
    int FixedDistanceBits;  // Huffman tables for fixed-length blocks
    unsigned int StoredLen; // number of bytes which are not compressed, only stored

    unsigned long BitBuffer; // bit buffer
    unsigned long BitCount;  // bits in bit buffer
    unsigned long crc;

    // internal, private functions
    BOOL InflateBlock();
    BOOL InflateStoredInit();
    int InflateStored();
    BOOL InflateFixedInit();
    BOOL InflateDynamicInit();
    int InflateCodes(SHufTable* literalTable, int literalBits,
                     SHufTable* distanceTable, int distanceBits);
    void HufTableFree(SHufTable* t);
    int HufTableBuild(unsigned int* b, unsigned int n, unsigned int s,
                      unsigned short* d, unsigned short* e, SHufTable** t, int* m);

    BOOL CompactBuffer() override;
    BOOL DecompressBlock(unsigned short needed) override;
};
