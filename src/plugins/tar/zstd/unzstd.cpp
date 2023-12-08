// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "../dlldefs.h"
#include "../fileio.h"
#include "unzstd.h"

#include "..\tar.rh"
#include "..\tar.rh2"
#include "..\lang\lang.rh"

#include <zstd_errors.h>

CZStd::CZStd(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize) : CZippedFile(filename, file, buffer, start, read, inputSize)
{
    CALL_STACK_MESSAGE2("CZStd::CZStd(%s, , , )", filename);

    // pokud neprosel konstruktor parenta, balime to rovnou
    if (!Ok)
        return;

    // nemuze to byt zstd, pokud mame min nez hlavicku...
    if (DataEnd - DataStart < 4)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // pokud neni "magicke cislo" na zacatku, nejde o zstd
    if (DataStart[0] != 0x28 || DataStart[1] != 0xB5 ||
        DataStart[2] != 0x2F || DataStart[3] != 0xFD)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // mame zstd, ale precteny header nepotvrzujeme, knihovna ho bude overovat znovu...

    m_DContext = ZSTD_createDCtx();
    if (!m_DContext)
    {
        ErrorCode = IDS_ERR_INTERNAL;
        return;
    }

    // hotovo
}

CZStd::~CZStd()
{
    CALL_STACK_MESSAGE1("CZStd::~CZStd()");
    if (m_DContext)
        ZSTD_freeDCtx(m_DContext);
}

BOOL CZStd::DecompressBlock(unsigned short needed)
{
    if (EndReached)
        return TRUE;
    size_t lastRet = 1;
    while (lastRet != 0 && ExtrEnd < Window + BUFSIZE)
    {
        unsigned char* src = DataStart;
        // aspon jeden byte musi byt v bufferu
        if (DataEnd == DataStart)
            src = (unsigned char*)FReadBlock(0);
        if (src == NULL)
            return FALSE;

        ZSTD_inBuffer input = {DataStart, GetUnreadInputBufferSize(), 0};

        while (lastRet != 0 && input.pos < input.size)
        {
            ZSTD_outBuffer output = {ExtrEnd, BUFSIZE - (unsigned int)(ExtrEnd - Window), 0};
            lastRet = ZSTD_decompressStream(m_DContext, &output, &input);

            if (ZSTD_isError(lastRet))
            {
                Ok = FALSE;
                switch (ZSTD_getErrorCode(lastRet))
                {
                case ZSTD_error_prefix_unknown:
                case ZSTD_error_dstSize_tooSmall:
                case ZSTD_error_corruption_detected:
                case ZSTD_error_checksum_wrong:
                    ErrorCode = IDS_ERR_CORRUPT;
                    break;
                case ZSTD_error_memory_allocation:
                    ErrorCode = IDS_ERR_MEMORY;
                    break;
                default:
                    ErrorCode = IDS_ERR_INTERNAL;
                    break;
                };
                return FALSE;
            }

            // commitnu prectena data ze vstupu
            FReadBlock((unsigned int)(((unsigned char*)input.src + input.pos) - (unsigned char*)DataStart));
            unsigned short extracted = (unsigned short)(((unsigned char*)output.dst + output.pos) - ExtrEnd);
            ExtrEnd = (unsigned char*)output.dst + output.pos;

            // vystupni buffer je plny, nechme ho zpracovat
            if (output.pos == output.size)
                break;
        }
    }

    if (lastRet == 0)
        EndReached = TRUE;

    return TRUE;
}
