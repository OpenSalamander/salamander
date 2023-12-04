// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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

struct tagCDecompressionObject;
typedef tagCDecompressionObject CDecompressionObject;

//input manager for decompression

typedef void (*FRefillInBuffer)(CDecompressionObject*);

typedef struct
{
    //public fields, should be intialized before calling Inflate()
    uch* NextByte;      //pointer to next byte in the input buffer
    unsigned BytesLeft; //number of bytes left in buffer
                        //i.e. number of valid bytes pointed
                        //by NextByte
    void* UserData;     //pointer to a user data
    int Error;          //error state (0 = no error), could be set
                        //by arbitrary funcion at every time
                        //checked in NextByte() function (inflate.cpp)

    //internal fields
    ulg BitBuf;        //bit buffer
    unsigned BitCount; //number of bits in bit buffer */

    //user function
    FRefillInBuffer Refill; //should fill input buffer with new data and
                            //reset NextByte and BytesLeft variables
    //unshrink and unreduce
    int BitCount2; //number of bits in bit buffer */
} CInputManager;

//output manager for decompression

typedef int (*FFlushOutput)(unsigned, CDecompressionObject*);
//first parameter is number of bytes to be flushed
//return zero if succesfull, non zero value if failed

typedef struct
{
    //public fields, should be intialized before calling Inflate()
    uch* SlideWin;    //circular buffer
    unsigned WinSize; //size of sliding window,
                      //should be at least 32K
    void* UserData;   //pointer to a user data

    //internal fields
    unsigned WinPos; //current position in sliding window

    //user function
    FFlushOutput Flush; //should write bytes in the input

    //unshrink
    uch* OutBuf;
    unsigned BufSize;
} COutputManager;

//decopmression object

typedef int (*FProgressMonitor)(CDecompressionObject*);
//if return zero value decompression is aborted

struct tagCDecompressionObject
{
    CInputManager* Input;
    COutputManager* Output;
    void* UserData; //pointer to a user data
    void* HeapInfo; //passed to memory functions

    //inflate: fixed hufman trees
    struct huft* fixed_tl64; // !! must be NULL initialized
    struct huft* fixed_td64; //before calling inflate
    int fixed_bl64,
        fixed_bd64;
    struct huft* fixed_tl32; // !! must be NULL initialized
    struct huft* fixed_td32; //before calling inflate
    int fixed_bl32,
        fixed_bd32;

    //explode + unreduce
    unsigned __int64 ucsize; //uncompressed size

    //explode
    unsigned __int64 csize; //compressed size
    unsigned Flag;          //general purpose bit flag

    //unshrink + unreduce
    unsigned __int64 CompBytesLeft; //bytes left to decompress

    //unreduce
    int Method;

    // internal variables for inflate
    struct huft* fixed_tl;
    struct huft* fixed_td;
    int fixed_bl,
        fixed_bd;

    const ush* cplens;
    const uch* cplext;
    const uch* cpdext;
};

int Inflate(CDecompressionObject* decompress, int deflate64);
int FreeFixedHufman(CDecompressionObject* decompress);

//used in explode
extern const ush mask_bits[17];

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
    if (decompress->Input->BytesLeft)
    {
        decompress->Input->BytesLeft--;
        return *decompress->Input->NextByte++;
    }
    else
    {
        decompress->Input->Refill(decompress);
        decompress->Input->BytesLeft--;
        return *decompress->Input->NextByte++;
    }
}
#endif // _DEBUG
