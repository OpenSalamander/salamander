// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>

#include "spl_base.h"
#include "dbg.h"

#include "config.h"
#include "inflate.h"
#include "zip.rh2"

/* modified by Lukas in February 2000

/*---------------------------------------------------------------------------

  unreduce.c

  The Reducing algorithm is actually a combination of two distinct algorithms.
  The first algorithm compresses repeated byte sequences, and the second al-
  gorithm takes the compressed stream from the first algorithm and applies a
  probabilistic compression method.

  ---------------------------------------------------------------------------*/

/**************************************/
/*  UnReduce Defines, Typedefs, etc.  */
/**************************************/

#define DLE 144

typedef uch f_array[64]; /* for followers[256][64] */

/******************************/
/*  UnReduce Local Functions  */
/******************************/

int LoadFollowers(CDecompressionObject* decompress, f_array* followers, uch* Slen);

/*******************************/
/*  UnReduce Global Constants  */
/*******************************/

static const short L_table[] =
    {0, 0x7f, 0x3f, 0x1f, 0x0f};

static const short D_shift[] =
    {0, 0x07, 0x06, 0x05, 0x04};
static const short D_mask[] =
    {0, 0x01, 0x03, 0x07, 0x0f};

static const short B_table[] =
    {8, 1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6,
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
     8, 8, 8, 8};

//this function should replace NEXTBYTE macro
__forceinline int NextByte2(CDecompressionObject* decompress)
{
    if (decompress->CompBytesLeft == 0)
        return EOF;
    decompress->CompBytesLeft--;
    if (decompress->Input->BytesLeft)
    {
        decompress->Input->BytesLeft--;
        return *decompress->Input->NextByte++;
    }
    else
    {
        decompress->Input->Refill(decompress);
        if (decompress->Input->Error)
            return EOF;
        decompress->Input->BytesLeft--;
        return *decompress->Input->NextByte++;
    }
}

#pragma runtime_checks("c", off)

#define READBITS(dec, nbits, zdest) \
    { \
        if (nbits > (dec)->Input->BitCount2) \
        { \
            int temp; \
            while ((dec)->Input->BitCount2 <= 8 * (int)(sizeof(ulg) - 1) && (temp = NextByte2(dec)) != EOF) \
            { \
                (dec)->Input->BitBuf |= (ulg)temp << (dec)->Input->BitCount2; \
                (dec)->Input->BitCount2 += 8; \
            } \
            if ((dec)->Input->Error) \
                return 1; \
            if (nbits > (dec)->Input->BitCount2) \
            { \
                (dec)->Input->Error = IDS_EOF; \
                return 1; \
            } \
        } \
        zdest = (short)((ush)(dec)->Input->BitBuf & mask_bits[nbits]); \
        (dec)->Input->BitBuf >>= nbits; \
        (dec)->Input->BitCount2 -= nbits; \
    }

/*************************/
/*  Function unreduce()  */
/*************************/

int Unreduce(CDecompressionObject* decompress) /* expand probabilistically reduced data */
{
    int lchar = 0;
    short nchar;
    short ExState = 0;
    short V = 0;
    short Len = 0;
    unsigned __int64 s = decompress->ucsize; /* number of bytes left to decompress */
    unsigned w = 0;                          /* position in output window slide[] */
    unsigned u = 1;                          /* true if slide[] unflushed */
    uch Slen[256];
    uch* slide = decompress->Output->SlideWin;

    decompress->Input->BitCount2 = 0;
    decompress->Input->BitBuf = 0;

    f_array* followers = (f_array*)(slide + 0x4000);
    int factor = decompress->Method - 1;

    if (LoadFollowers(decompress, followers, Slen))
        return 1;

    while (s > 0 /* && (!zipeof) */)
    {
        if (Slen[lchar] == 0)
            READBITS(decompress, 8, nchar) /* ; */
        else
        {
            READBITS(decompress, 1, nchar) /* ; */
            if (nchar != 0)
                READBITS(decompress, 8, nchar) /* ; */
            else
            {
                short follower;
                int bitsneeded = B_table[Slen[lchar]];

                READBITS(decompress, bitsneeded, follower) /* ; */
                nchar = followers[lchar][follower];
            }
        }
        /* expand the resulting byte */
        switch (ExState)
        {

        case 0:
            if (nchar != DLE)
            {
                s--;
                slide[w++] = (uch)nchar;
                if (w == 0x4000)
                {
                    if (decompress->Output->Flush(w, decompress))
                    {
                        TRACE_E("inflate_codes: flush returned error");
                        return 2;
                    }
                    w = u = 0;
                }
            }
            else
                ExState = 1;
            break;

        case 1:
            if (nchar != 0)
            {
                V = nchar;
                Len = V & L_table[factor];
                if (Len == L_table[factor])
                    ExState = 2;
                else
                    ExState = 3;
            }
            else
            {
                s--;
                slide[w++] = DLE;
                if (w == 0x4000)
                {
                    if (decompress->Output->Flush(w, decompress))
                    {
                        TRACE_E("inflate_codes: flush returned error");
                        return 2;
                    }
                    w = u = 0;
                }
                ExState = 0;
            }
            break;

        case 2:
        {
            Len += nchar;
            ExState = 3;
        }
        break;

        case 3:
        {
            unsigned e;
            unsigned n = Len + 3;
            unsigned d = w - ((((V >> D_shift[factor]) &
                                D_mask[factor])
                               << 8) +
                              nchar + 1);

            s -= n;
            do
            {
                n -= (e = (e = 0x4000 - ((d &= 0x3fff) > w ? d : w)) > n ? n : e);
                if (u && w <= d)
                {
                    memset(slide + w, 0, e);
                    w += e;
                    d += e;
                }
                else if (w - d < e) /* (assume unsigned comparison) */
                    do
                    { /* slow to avoid memcpy() overlap */
                        slide[w++] = slide[d++];
                    } while (--e);
                else
                {
                    memcpy(slide + w, slide + d, e);
                    w += e;
                    d += e;
                }
                if (w == 0x4000)
                {
                    if (decompress->Output->Flush(w, decompress))
                    {
                        TRACE_E("inflate_codes: flush returned error");
                        return 2;
                    }
                    w = u = 0;
                }
            } while (n);

            ExState = 0;
        }
        break;
        }

        /* store character for next iteration */
        lchar = nchar;
    }

    /* flush out slide */
    if (decompress->Output->Flush(w, decompress))
    {
        TRACE_E("inflate_codes: flush returned error");
        return 2;
    }
    return 0;
}

/******************************/
/*  Function LoadFollowers()  */
/******************************/
#pragma warning(disable : 4244) //aby to nervalo ze prirazuju short do charu

int LoadFollowers(CDecompressionObject* decompress,
                  f_array* followers,
                  uch* Slen)
{
    int x;
    int i;

    for (x = 255; x >= 0; x--)
    {
        READBITS(decompress, 6, Slen[x]) /* ; */
        for (i = 0; (uch)i < Slen[x]; i++)
            READBITS(decompress, 8, followers[x][i]) /* ; */
    }
    return 0;
}

#pragma warning(default : 4244)

#undef READBITS

#pragma runtime_checks("", restore)
