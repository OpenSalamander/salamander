// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
//#include <wtypes.h>

#include "plugins.h"
#include "zlib\zlib.h"

//
// ****************************************************************************
// CSalamanderZLIB
//

class CSalamanderZLIB : public CSalamanderZLIBAbstract
{
public:
    virtual int WINAPI DeflateInit(CSalZLIB* zlibInfo, int compressLevel);
    virtual int WINAPI Deflate(CSalZLIB* zlibInfo, int flush);
    virtual int WINAPI DeflateEnd(CSalZLIB* zlibInfo);

    virtual int WINAPI InflateInit(CSalZLIB* zlibInfo);
    virtual int WINAPI Inflate(CSalZLIB* zlibInfo, int flush);
    virtual int WINAPI InflateEnd(CSalZLIB* zlibInfo);

    virtual int WINAPI InflateInit2(CSalZLIB* zlibInfo, int windowBits);
};

CSalamanderZLIB SalamanderZLIB;

int CSalamanderZLIB::DeflateInit(CSalZLIB* zlibInfo, int compressLevel)
{
    memset(zlibInfo, 0, sizeof(CSalZLIB));
    zlibInfo->internal = calloc(sizeof(z_stream), 1);
    if (!zlibInfo->internal)
        return SAL_Z_MEM_ERROR;
    int ret = deflateInit((z_stream*)zlibInfo->internal, compressLevel);
    if (Z_OK != ret)
    {
        free(zlibInfo->internal);
        zlibInfo->internal = NULL;
    }
    return ret;
}

int CSalamanderZLIB::Deflate(CSalZLIB* zlibInfo, int flush)
{
    z_stream* strm = (z_stream*)zlibInfo->internal;
    strm->next_in = zlibInfo->next_in;
    strm->avail_in = zlibInfo->avail_in;
    strm->next_out = zlibInfo->next_out;
    strm->avail_out = zlibInfo->avail_out;
    int ret = deflate(strm, flush);
    zlibInfo->next_in = strm->next_in;
    zlibInfo->avail_in = strm->avail_in;
    zlibInfo->total_in = strm->total_in;
    zlibInfo->next_out = strm->next_out;
    zlibInfo->avail_out = strm->avail_out;
    zlibInfo->total_out = strm->total_out;
    return ret;
}

int CSalamanderZLIB::DeflateEnd(CSalZLIB* zlibInfo)
{
    int ret = deflateEnd((z_stream*)zlibInfo->internal);
    if (zlibInfo->internal != NULL)
        free(zlibInfo->internal);
    return ret;
}

int CSalamanderZLIB::InflateInit(CSalZLIB* zlibInfo)
{
    memset(zlibInfo, 0, sizeof(CSalZLIB));
    zlibInfo->internal = calloc(sizeof(z_stream), 1);
    if (!zlibInfo->internal)
        return SAL_Z_MEM_ERROR;
    int ret = inflateInit((z_stream*)zlibInfo->internal);
    if (Z_OK != ret)
    {
        free(zlibInfo->internal);
        zlibInfo->internal = NULL;
    }
    return ret;
}

int CSalamanderZLIB::Inflate(CSalZLIB* zlibInfo, int flush)
{
    z_stream* strm = (z_stream*)zlibInfo->internal;
    strm->next_in = zlibInfo->next_in;
    strm->avail_in = zlibInfo->avail_in;
    strm->next_out = zlibInfo->next_out;
    strm->avail_out = zlibInfo->avail_out;
    int ret = inflate(strm, flush);
    zlibInfo->next_in = strm->next_in;
    zlibInfo->avail_in = strm->avail_in;
    zlibInfo->total_in = strm->total_in;
    zlibInfo->next_out = strm->next_out;
    zlibInfo->avail_out = strm->avail_out;
    zlibInfo->total_out = strm->total_out;
    return ret;
}

int CSalamanderZLIB::InflateEnd(CSalZLIB* zlibInfo)
{
    int ret = inflateEnd((z_stream*)zlibInfo->internal);
    free(zlibInfo->internal);
    return ret;
}

int CSalamanderZLIB::InflateInit2(CSalZLIB* zlibInfo, int windowBits)
{
    memset(zlibInfo, 0, sizeof(CSalZLIB));
    zlibInfo->internal = calloc(sizeof(z_stream), 1);
    if (!zlibInfo->internal)
        return SAL_Z_MEM_ERROR;
    int ret = inflateInit2((z_stream*)zlibInfo->internal, windowBits);
    if (Z_OK != ret)
    {
        free(zlibInfo->internal);
        zlibInfo->internal = NULL;
    }
    return ret;
}

//
// ****************************************************************************
// CSalamanderGeneral
//

CSalamanderZLIBAbstract*
CSalamanderGeneral::GetSalamanderZLIB()
{
    CALL_STACK_MESSAGE_NONE
    return &SalamanderZLIB;
}
