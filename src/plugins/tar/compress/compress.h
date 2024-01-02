// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// magic numbers
#define LZW_MAGIC 0x9D1F // Magic header for lzw files, compress

#define PACK_MAGIC 0x1E1F      // Magic header for packed files
#define LZH_MAGIC 0xA01F       // Magic header for SCO LZH Compress files
#define PKZIP_MAGIC 0x04034B50 // Magic header for pkzip files

#define BITNUM_MASK 0x1F
#define BLCKMODE_MASK 0x80
#define RESERVED_MASK 0x60

#define BITS 16     // Maximal number of bits per code
#define INIT_BITS 9 // Initial number of bits per code

#define CLEAR 256         // flush the dictionary
#define FIRST (CLEAR + 1) // first free entry

#define INBUF_EXTRA 64
#define OUTBUF_EXTRA 2048

class CCompress : public CZippedFile
{
public:
    CCompress(const char* filename, HANDLE file, unsigned char* buffer, unsigned long read, CQuadWord inputSize);
    ~CCompress() override;

    BOOL IsCompressed() const override { return TRUE; }

protected:
    unsigned short* PrefixTab;
    unsigned char* SuffixTab;
    BOOL BlockMode;
    unsigned char MaxBitsNumber;
    int BitsNumber;
    unsigned long BitMask;
    unsigned long FreeEnt;
    unsigned long MaxCode;
    unsigned long MaxMaxCode;
    BOOL Finished;
    unsigned char* DecoStack;
    unsigned char* StackTop;
    unsigned char* StackPtr;
    long OldCode;
    unsigned char FInChar;
    unsigned long InputData;
    unsigned char ReadyBits;
    unsigned char UsedBytes;

    BOOL DecompressBlock(unsigned short needed) override;
};
