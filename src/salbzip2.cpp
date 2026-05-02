// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "plugins.h"
#include "bzip2/bzlib.h"

//
// ****************************************************************************
// CSalamanderBZIP2
//

class CSalamanderBZIP2 : public CSalamanderBZIP2Abstract
{
public:
    virtual int WINAPI CompressInit(CSalBZIP2* bzip2Info, int blockSize100k, int workFactor);
    virtual int WINAPI Compress(CSalBZIP2* bzip2Info, int action);
    virtual int WINAPI CompressEnd(CSalBZIP2* bzip2Info);

    virtual int WINAPI DecompressInit(CSalBZIP2* bzip2Info, BOOL bConserveMemory);
    virtual int WINAPI Decompress(CSalBZIP2* bzip2Info);
    virtual int WINAPI DecompressEnd(CSalBZIP2* bzip2Info);
};

CSalamanderBZIP2 SalamanderBZIP2;

int CSalamanderBZIP2::CompressInit(CSalBZIP2* bzip2Info, int blockSize100k, int workFactor)
{
    memset(bzip2Info, 0, sizeof(CSalBZIP2));
    bzip2Info->internal = calloc(sizeof(bz_stream), 1);
    if (!bzip2Info->internal)
        return SAL_BZ_MEM_ERROR;
    int ret = BZ2_bzCompressInit((bz_stream*)bzip2Info->internal, blockSize100k, 0 /*verbosity*/, workFactor);
    if (BZ_OK != ret)
    {
        free(bzip2Info->internal);
        bzip2Info->internal = NULL;
    }
    return ret;
}

int CSalamanderBZIP2::Compress(CSalBZIP2* bzip2Info, int action)
{
    bz_stream* strm = (bz_stream*)bzip2Info->internal;
    strm->next_in = (char*)bzip2Info->next_in;
    strm->avail_in = bzip2Info->avail_in;
    strm->next_out = (char*)bzip2Info->next_out;
    strm->avail_out = bzip2Info->avail_out;
    int ret = BZ2_bzCompress(strm, action);
    bzip2Info->next_in = (BYTE*)strm->next_in;
    bzip2Info->avail_in = strm->avail_in;
    bzip2Info->total_in.Set(strm->total_in_lo32, strm->total_in_hi32);
    bzip2Info->next_out = (BYTE*)strm->next_out;
    bzip2Info->avail_out = strm->avail_out;
    bzip2Info->total_out.Set(strm->total_out_lo32, strm->total_out_hi32);
    return ret;
}

int CSalamanderBZIP2::CompressEnd(CSalBZIP2* bzip2Info)
{
    int ret = BZ2_bzCompressEnd((bz_stream*)bzip2Info->internal);
    if (bzip2Info->internal != NULL)
        free(bzip2Info->internal);
    return ret;
}

int CSalamanderBZIP2::DecompressInit(CSalBZIP2* bzip2Info, BOOL bConserveMemory)
{
    memset(bzip2Info, 0, sizeof(CSalBZIP2));
    bzip2Info->internal = calloc(sizeof(bz_stream), 1);
    if (!bzip2Info->internal)
        return SAL_BZ_MEM_ERROR;
    int ret = BZ2_bzDecompressInit((bz_stream*)bzip2Info->internal, 0 /*verbosity*/, bConserveMemory ? 1 : 0);
    if (BZ_OK != ret)
    {
        free(bzip2Info->internal);
        bzip2Info->internal = NULL;
    }
    return ret;
}

int CSalamanderBZIP2::Decompress(CSalBZIP2* bzip2Info)
{
    bz_stream* strm = (bz_stream*)bzip2Info->internal;
    strm->next_in = (char*)bzip2Info->next_in;
    strm->avail_in = bzip2Info->avail_in;
    strm->next_out = (char*)bzip2Info->next_out;
    strm->avail_out = bzip2Info->avail_out;
    int ret = BZ2_bzDecompress(strm);
    bzip2Info->next_in = (BYTE*)strm->next_in;
    bzip2Info->avail_in = strm->avail_in;
    bzip2Info->total_in.Set(strm->total_in_lo32, strm->total_in_hi32);
    ;
    bzip2Info->next_out = (BYTE*)strm->next_out;
    bzip2Info->avail_out = strm->avail_out;
    bzip2Info->total_out.Set(strm->total_out_lo32, strm->total_out_hi32);
    return ret;
}

int CSalamanderBZIP2::DecompressEnd(CSalBZIP2* bzip2Info)
{
    int ret = BZ2_bzDecompressEnd((bz_stream*)bzip2Info->internal);
    free(bzip2Info->internal);
    return ret;
}

//
// ****************************************************************************
// CSalamanderGeneral
//

CSalamanderBZIP2Abstract*
CSalamanderGeneral::GetSalamanderBZIP2()
{
    CALL_STACK_MESSAGE_NONE
    return &SalamanderBZIP2;
}
