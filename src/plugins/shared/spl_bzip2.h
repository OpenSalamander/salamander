// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_bzip2) // Ensure structure packing regardless of compiler settings
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

/***********************************************************************
 * Simplified interface to the BZIP2 library provided by Salamander.
 * To understand the action types and return values, it is recommended
 * to read ther documentation at www.bzip.org.
 ***********************************************************************/

/* Values for action argument of Compress() */
#define SAL_BZ_RUN 0
#define SAL_BZ_FLUSH 1
#define SAL_BZ_FINISH 2

/* Return codes for all functions. Negative values are errors,
 * positive values are used for special but normal events.
 */
#define SAL_BZ_OK 0
#define SAL_BZ_RUN_OK 1
#define SAL_BZ_FLUSH_OK 2
#define SAL_BZ_FINISH_OK 3
#define SAL_BZ_STREAM_END 4
#define SAL_BZ_SEQUENCE_ERROR (-1)
#define SAL_BZ_PARAM_ERROR (-2)
#define SAL_BZ_MEM_ERROR (-3)
#define SAL_BZ_DATA_ERROR (-4)
#define SAL_BZ_DATA_ERROR_MAGIC (-5)
#define SAL_BZ_IO_ERROR (-6)
#define SAL_BZ_UNEXPECTED_EOF (-7)
#define SAL_BZ_OUTBUFF_FULL (-8)
#define SAL_BZ_CONFIG_ERROR (-9)

/* Control structure associated with each de/compression task */
struct CSalBZIP2
{
    BYTE* next_in;      /* Next input byte */
    UINT avail_in;      /* Number of bytes available at next_in */
    CQuadWord total_in; /* Total number of input bytes read so far */

    BYTE* next_out;      /* Next output byte should be put there */
    UINT avail_out;      /* Remaining free space at next_out */
    CQuadWord total_out; /* Total number of bytes output so far */

    LPVOID internal; /* Used internally by the interface */
};

/* Any function of CSalamanderBZIP2Abstract can be called from any thread.
   Single instance can be used many times, using different bzip2Info structs.
   For usage howtos, see the comment at the top of this file. */

class CSalamanderBZIP2Abstract
{
public:
    /* Compression functions */
    /* blockSize100k: 1-9; workFactor: 1-250 (default 30 when 0 is specified) */
    virtual int WINAPI CompressInit(CSalBZIP2* bzip2Info, int blockSize100k, int workFactor) = 0;
    virtual int WINAPI Compress(CSalBZIP2* bzip2Info, int action) = 0;
    virtual int WINAPI CompressEnd(CSalBZIP2* bzip2Info) = 0;

    /* Decompression functions */
    /* bConserveMemory: TRUE to use less memory & decompress slower */
    virtual int WINAPI DecompressInit(CSalBZIP2* bzip2Info, BOOL bConserveMemory) = 0;
    virtual int WINAPI Decompress(CSalBZIP2* bzip2Info) = 0;
    virtual int WINAPI DecompressEnd(CSalBZIP2* bzip2Info) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_bzip2)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
