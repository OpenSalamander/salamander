// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __BZIP3_H__
#define __BZIP3_H__

#include <libbz3.h>

class CBZip3 : public CZippedFile
{
public:
    CBZip3(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize);
    ~CBZip3() override;

protected:
    BOOL EndReached{FALSE}; // set, when all data was extracted

    bz3_state* m_State{};
    uint32_t m_BlockSize{0};
    BYTE* m_Buffer{}; // buffer for bzip3 block, compressed data are decompressed to the same buffer
    size_t m_BufferSize{0}; // buffer size where bzip3 block has to fit
    size_t m_BufferWrittenDataLen{0}; // length of compressed data read from input
    size_t m_BufferReadDataLen{0}; // length of decompressed data written to output
    BOOL DecompressBlock(unsigned short needed) override;
};

#endif // __BZIP3_H__
