// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

typedef unsigned __int8 uch;  //8-bit unsigned type
typedef unsigned __int16 ush; //16-bit unsigned type
typedef unsigned int ulg;     //at least 32-bit type (usually processor register size)

//PKZIP 1.93a problem--live with it
#define PKZIP_BUG_WORKAROUND

//check Error field of CInputManager after calling NextByte() in
//NEEDBITS() macro
//2-3% speed penalty
#define CHECK_INPUT_ERROR

struct huft
{
    uch e; /* number of extra bits or operation */
    uch b; /* number of bits in this code or subcode */
    union
    {
        ush n;          /* literal, length base, or distance base */
        struct huft* t; /* pointer to next level of table */
    } v;
};

//decompression object

struct CDecompressionObject
{
    const char* Data;    // zacatek dat
    const char* DataPtr; // aktualni pozice v datech
    const char* DataEnd; // konec dat

    DWORD Crc; // CRC32 vypakovanych dat

    char* OutputMem;     // buffer pro vybalena data
    char* OutputMemPtr;  // aktualni pozice v bufferu pro vybalena data
    DWORD OutputMemSize; // velikost bufferu pro vybalena data

    //public fields, should be intialized before calling Inflate()
    uch* SlideWin;    //circular buffer
    unsigned WinSize; //size of sliding window, should be at least 32K

    //internal fields
    unsigned WinPos; //current position in sliding window

    ulg BitBuf;        //bit buffer
    unsigned BitCount; //number of bits in bit buffer */

    // internal variables for inflate
    struct huft* fixed_tl;
    struct huft* fixed_td;
    int fixed_bl,
        fixed_bd;

    int Flush(unsigned bytes);
};

int Inflate(CDecompressionObject* decompress);
int FreeFixedHufman(CDecompressionObject* decompress);

int huft_build(CDecompressionObject* decompress,
               const unsigned* b, /* code lengths in bits (all assumed <= BMAX) */
               unsigned n,        /* number of codes (assumed <= N_MAX) */
               unsigned s,        /* number of simple-valued codes (0..s-1) */
               const ush* d,      /* list of base values for non-simple codes */
               const uch* e,      /* list of extra bits for non-simple codes */
               struct huft** t,   /* result: starting table */
               int* m);           /* maximum lookup bits, returns actual */

int huft_free(CDecompressionObject* decompress, struct huft* t);

#ifdef _DEBUG
uch NextByte(CDecompressionObject* decompress);
#else
__forceinline uch NextByte(CDecompressionObject* decompress)
{
    if (decompress->DataPtr < decompress->DataEnd)
        return *decompress->DataPtr++;
    else
    {
        if (decompress->DataPtr == decompress->DataEnd)
            decompress->DataPtr++; // chybovy stav je az decompress->DataPtr > decompress->DataEnd
        return 0 /* chyba */;
    }
}
#endif //_DEBUG
