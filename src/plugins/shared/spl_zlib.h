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
#pragma pack(push, enter_include_spl_zlib) // Ensure structure packing regardless of compiler settings
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

/***********************************************************************
 * Simplified interface to the ZLIB library provided by Salamander.
 * To understand the flush types and returned values, it is recommended
 * to read zlib.h from the free ZLIB library.
 ***********************************************************************/

/* Values for flush argument of Deflate()/Inflate() */
#define SAL_Z_NO_FLUSH 0
#define SAL_Z_SYNC_FLUSH 2
#define SAL_Z_FULL_FLUSH 3
#define SAL_Z_FINISH 4
#define SAL_Z_BLOCK 5

/* Return codes for Deflate and Inflate functions. Negative
 * values are errors, positive values are used for special but normal events.
 */
#define SAL_Z_OK 0
#define SAL_Z_STREAM_END 1
#define SAL_Z_NEED_DICT 2
#define SAL_Z_ERRNO (-1)
#define SAL_Z_STREAM_ERROR (-2)
#define SAL_Z_DATA_ERROR (-3)
#define SAL_Z_MEM_ERROR (-4)
#define SAL_Z_BUF_ERROR (-5)
#define SAL_Z_VERSION_ERROR (-6)

/* Control structure associated with each de/compression task */
struct CSalZLIB
{
    BYTE* next_in;  /* next input byte */
    UINT avail_in;  /* number of bytes available at next_in */
    ULONG total_in; /* total number of input bytes read so far */

    BYTE* next_out;  /* next output byte should be put there */
    UINT avail_out;  /* remaining free space at next_out */
    ULONG total_out; /* total number of bytes output so far */

    LPVOID internal; /* Used internally by the interface */
};

/* Any function of CSalamanderZLIBAbstract can be called from any thread.
   Single instance can be used many times, using different zlibInfo structs.
   For usage howtos, see the comment at the top of this file. */

class CSalamanderZLIBAbstract
{
public:
    /* Compression functions */
    virtual int WINAPI DeflateInit(CSalZLIB* zlibInfo, int compressLevel) = 0;
    virtual int WINAPI Deflate(CSalZLIB* zlibInfo, int flush) = 0;
    virtual int WINAPI DeflateEnd(CSalZLIB* zlibInfo) = 0;

    /* Decompression functions */
    virtual int WINAPI InflateInit(CSalZLIB* zlibInfo) = 0;
    virtual int WINAPI Inflate(CSalZLIB* zlibInfo, int flush) = 0;
    virtual int WINAPI InflateEnd(CSalZLIB* zlibInfo) = 0;
    virtual int WINAPI InflateInit2(CSalZLIB* zlibInfo, int windowBits) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_zlib)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
