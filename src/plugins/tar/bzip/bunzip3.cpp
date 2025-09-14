// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "../dlldefs.h"
#include "../fileio.h"
#include "bzip3.h"
#include "bzip3format.h"

#include "..\tar.rh"
#include "..\tar.rh2"
#include "..\lang\lang.rh"

#define KiB(x) ((x) * 1024)
#define MiB(x) ((x) * 1024 * 1024)

CBZip3::CBZip3(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize) : CZippedFile(filename, file, buffer, start, read, inputSize)
{
    CALL_STACK_MESSAGE2("CBZip3::CBZip3(%s, , , )", filename);

    // check if parent has initialized successfully
    if (!Ok)
        return;

    // need at least size of buffer to read from input
    if (DataEnd - DataStart < static_cast<ptrdiff_t>(sizeof(BZ3_Header)))
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }

    // verify file signature
    const auto header = reinterpret_cast<BZ3_Header*>(DataStart);
    if (memcmp(header->Signature, BZ3_Header::SIGNATURE, BZ3_Header::SIGNATURE_LEN) != 0)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }

    // this is a bzip3 file, check also block size limits
    m_BlockSize = header->BlockSize;
    if (m_BlockSize < KiB(65) || m_BlockSize > MiB(511))
    {
        Ok = FALSE;
        ErrorCode = IDS_ERR_CORRUPT;
        FreeBufAndFile = FALSE;
        return;
    }
    // commit read data from input
    FReadBlock(sizeof(BZ3_Header));

    // instantiate bzip3 block decoder
    m_State = bz3_new(m_BlockSize);
    if (!m_State)
    {
        Ok = FALSE;
        ErrorCode = IDS_ERR_MEMORY;
        FreeBufAndFile = FALSE;
        return;
    }

    // alloc internal buffer for processing bzip3 blocks
    m_BufferSize = bz3_bound(m_BlockSize);
    m_Buffer = (BYTE*) malloc(m_BufferSize);
    if (!m_Buffer)
    {
        Ok = FALSE;
        ErrorCode = IDS_ERR_MEMORY;
        FreeBufAndFile = FALSE;
        return;
    }

    // initialization done
}

CBZip3::~CBZip3()
{
    CALL_STACK_MESSAGE1("CBZip3::~CBZip3()");
    if (m_Buffer)
    {
        free(m_Buffer);
        m_Buffer = nullptr;
        m_BufferSize = 0;
    }
    if (m_State)
    {
        bz3_free(m_State);
        m_State = nullptr;
    }
}

BOOL CBZip3::DecompressBlock(unsigned short needed)
{
    if (EndReached)
        return TRUE;

    // NOTE: decompression stages:
    // 1. read compressed bzip3 block data from input into block buffer
    // 2. decompress the bzip3 block, bz3_decode_block() decompress it to the same buffer
    // 3. write the decompressed data to output (by chunks)
    // 4. continue with the next block, if not EOF

    if (m_BufferWrittenDataLen == 0)
    {
        // found the next bzip3 block

        // read input data if input buffer is empty
        if (DataEnd == DataStart)
        {
            const auto src = FReadBlock(0);
            if (src == NULL)
                return FALSE;
        }
        if (DataEnd == DataStart)
        {
            Ok = FALSE;
            ErrorCode = IDS_ERR_EOF;
            return FALSE;
        }

        // get block header
        const auto block = *reinterpret_cast<BZ3_Block*>(DataStart);
        // commit read data from input
        FReadBlock(sizeof(BZ3_Block));

        // block decompression will run on the same buffer, that has to be
        // long enough for both compressed and decompressed data
        if (block.OrigSize > m_BufferSize || block.CompressedSize > m_BufferSize)
        {
            // inconsistent header
            Ok = FALSE;
            ErrorCode = IDS_ERR_CORRUPT;
            return FALSE;
        }

        // read whole bzip3 block from input
        while (true)
        {
            // if input buffer is empty, read next chunk
            auto UnreadBufferSize = GetUnreadInputBufferSize();
            if (UnreadBufferSize == 0)
            {
                if (!FReadBlock(0))
                    return FALSE;
                UnreadBufferSize = GetUnreadInputBufferSize();
            }

            // read compressed data from input into the internal buffer
            const auto BytesToRead = min(UnreadBufferSize, block.CompressedSize - m_BufferWrittenDataLen);
            memcpy_s(m_Buffer + m_BufferWrittenDataLen, m_BufferSize - m_BufferWrittenDataLen, DataStart, BytesToRead);
            m_BufferWrittenDataLen += BytesToRead;

            // commit read data from input
            if (!FReadBlock(BytesToRead))
                return FALSE;
            if (block.CompressedSize == m_BufferWrittenDataLen)
                break;
        }

        // bzip3 block is read, we can start decoding
        const auto ret = bz3_decode_block(m_State, m_Buffer, m_BufferSize, block.CompressedSize, block.OrigSize);
        const auto errcode = bz3_last_error(m_State);
        if (ret == -1 || errcode != BZ3_OK)
        {
            Ok = FALSE;
            switch (errcode)
            {
            case BZ3_ERR_CRC:
            case BZ3_ERR_MALFORMED_HEADER:
            case BZ3_ERR_DATA_TOO_BIG:
                ErrorCode = IDS_ERR_CORRUPT;
                break;
            default:
                ErrorCode = IDS_ERR_INTERNAL;
                break;
            }
            return FALSE;
        }

        // update buffer data length to the decompressed size
        m_BufferWrittenDataLen = block.OrigSize;
    }

    if (m_BufferReadDataLen < m_BufferWrittenDataLen)
    {
        // write the decompressed data to the output by `needed` chunk size
        const auto OutSize = min(BUFSIZE - (unsigned int)(ExtrStart - Window), needed);
        const auto ReadLen = min(OutSize, m_BufferWrittenDataLen - m_BufferReadDataLen);
        memcpy_s(ExtrStart, OutSize, m_Buffer + m_BufferReadDataLen, ReadLen);
        ExtrEnd = ExtrStart + ReadLen;
        m_BufferReadDataLen += ReadLen;
    }

    if (m_BufferReadDataLen == m_BufferWrittenDataLen)
    {
        // reached end of a bzip3 block
        m_BufferWrittenDataLen = 0;
        m_BufferReadDataLen = 0;

        // check for EOF
        if (GetUnreadInputBufferSize() == 0)
        {
            EndReached = TRUE;
        }
    }

    return TRUE;
}
