// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "../dlldefs.h"
#include "../fileio.h"
#include "unlzma.h"

#include "..\tar.rh"
#include "..\tar.rh2"
#include "..\lang\lang.rh"

CLZMa::CLZMa(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize) : CZippedFile(filename, file, buffer, start, read, inputSize)
{
    CALL_STACK_MESSAGE2("CLZMa::CLZMa(%s, , , )", filename);

    // pokud neprosel konstruktor parenta, balime to rovnou
    if (!Ok)
        return;

    // nemuze to byt lzma, pokud mame min nez hlavicku...
    if (DataEnd - DataStart < 6)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // pokud neni "magicke cislo" na zacatku, nejde o lzma
    if (DataStart[0] != 0xFD || DataStart[1] != '7' ||
        DataStart[2] != 'z' || DataStart[3] != 'X' ||
        DataStart[4] != 'Z' || DataStart[5] != 0)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // mame lzma, ale precteny header nepotvrzujeme, knihovna ho bude overovat znovu...

    // inicializace streamu
    lzma_ret ret = lzma_stream_decoder(&m_strm, UINT64_MAX, LZMA_CONCATENATED);
    if (ret != LZMA_OK)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        switch (ret)
        {
        case LZMA_MEM_ERROR:
            ErrorCode = IDS_ERR_MEMORY;
            break;
        case LZMA_OPTIONS_ERROR:
        case LZMA_PROG_ERROR:
        default:
            ErrorCode = IDS_ERR_INTERNAL;
            break;
        }
        return;
    }

    // hotovo
}

CLZMa::~CLZMa()
{
    CALL_STACK_MESSAGE1("CLZMa::~CLZMa()");
    lzma_end(&m_strm);
}

BOOL CLZMa::DecompressBlock(unsigned short needed)
{
    if (EndReached)
        return TRUE;
    int ret = LZMA_OK;
    lzma_action action = LZMA_RUN;
    while (ret != LZMA_STREAM_END && ExtrEnd < Window + BUFSIZE)
    {
        unsigned char* src = DataStart;
        // aspon jeden byte musi byt v bufferu
        if (DataEnd == DataStart)
            src = (unsigned char*)FReadBlock(0);
        if (src == NULL)
            return FALSE;
        if (GetInputPos() == GetStreamSize())
            action = LZMA_FINISH;
        m_strm.next_in = (uint8_t*)DataStart;
        m_strm.avail_in = GetUnreadInputBufferSize();
        m_strm.next_out = (uint8_t*)ExtrEnd;
        m_strm.avail_out = BUFSIZE - (unsigned int)(ExtrEnd - Window);

        ret = lzma_code(&m_strm, action);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END)
        {
            Ok = FALSE;
            switch (ret)
            {
            case LZMA_DATA_ERROR:
            case LZMA_FORMAT_ERROR:
                ErrorCode = IDS_ERR_CORRUPT;
                break;
            case LZMA_MEM_ERROR:
            case LZMA_MEMLIMIT_ERROR:
                ErrorCode = IDS_ERR_MEMORY;
                break;
            case LZMA_PROG_ERROR:
            default:
                ErrorCode = IDS_ERR_INTERNAL;
                break;
            }
            return FALSE;
        }
        // commitnu prectena data ze vstupu
        FReadBlock((unsigned int)(m_strm.next_in - (uint8_t*)DataStart));
        unsigned short extracted = (unsigned short)((unsigned char*)m_strm.next_out - ExtrEnd);
        ExtrEnd = (unsigned char*)m_strm.next_out;
    }
    if (ret == LZMA_STREAM_END)
        EndReached = TRUE;
    return TRUE;
}
