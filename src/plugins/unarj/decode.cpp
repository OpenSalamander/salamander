// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/* DECODE.C, UNARJ, R JUNG, 01/22/94
 * Decode ARJ archive
 * Copyright (c) 1991-97 by ARJ Software, Inc.  All rights reserved.
 *
 *   This code may be freely used in programs that are NOT ARJ archivers
 *   (both compress and extract ARJ archives).
 *
 *   If you wish to distribute a modified version of this program, you
 *   MUST indicate that it is a modified version both in the program and
 *   source code.
 *
 *   If you modify this program, we would appreciate a copy of the new
 *   source code.  We are holding the copyright on the source code, so
 *   please do not delete our name from the program files or from the
 *   documentation.
 *
 * Modification history:
 * Date      Programmer  Description of modification.
 * 04/05/91  R. Jung     Rewrote code.
 * 04/23/91  M. Adler    Portabilized.
 * 04/29/91  R. Jung     Made GETBIT independent of short size.
 * 05/04/91  R. Jung     Simplified use of start[len].
 * 08/28/91  R. Jung     Added KEEP_WINDOW for systems with low memory.
 * 02/17/93  R. Jung     Added extra test for bad data to make_table().
 *                       Added PTABLESIZE defines.
 * 01/22/94  R. Jung     Changed copyright message.
 *
 */

#include "precomp.h"

#pragma runtime_checks("c", off)

#include "unarjdll.h"
#include "unarj.h"

#define THRESHOLD 3
#define MAXDICBIT 16
#define MATCHBIT 8
#define MAXMATCH 256
#define NC (UCHAR_MAX + MAXMATCH + 2 - THRESHOLD)
#define NP (MAXDICBIT + 1)
#define CBIT 9
#define NT (CODE_BIT + 3)
#define PBIT 5
#define TBIT 5

#if NT > NP
#define NPT NT
#else
#define NPT NP
#endif

#define CTABLESIZE 4096
#define PTABLESIZE 256

#define STRTP 9
#define STOPP 13

#define STRTL 0
#define STOPL 7

/* Local functions */
void __fastcall fillbuf(int n);
ushort __fastcall getbits(int n);
void init_getbits(void);

static void make_table(int nchar, uchar* bitlen, int tablebits, ushort* table, int tablesize);
static void read_pt_len(int nn, int nbit, int i_special);
static void read_c_len(void);
static ushort decode_c(void);
static ushort decode_p(void);
static void decode_start(void);
static short decode_ptr(void);
static short decode_len(void);

/* Local variables */

uchar text[DDICSIZ];

static short getlen;
static short getbuf;

static ushort left[2 * NC - 1];
static ushort right[2 * NC - 1];
static uchar c_len[NC];
static uchar pt_len[NPT];

static ushort c_table[CTABLESIZE];
static ushort pt_table[PTABLESIZE];
static ushort blocksize;

void init_getbits()
{
    bitbuf = 0;
    subbitbuf = 0;
    bitcount = 0;
    fillbuf(2 * CHAR_BIT);
}

void __fastcall fillbuf(int n) // Shift bitbuf n bits left, read n bits
{
    bitbuf = (bitbuf << n) & 0xFFFF; // lose the first n bits
    while (n > bitcount)
    {
        bitbuf |= subbitbuf << (n -= bitcount);
        if (compsize != 0)
        {
            compsize--;
            READ_BYTE(subbitbuf)
        }
        else
            subbitbuf = 0;
        bitcount = CHAR_BIT;
    }
    bitbuf |= subbitbuf >> (bitcount -= n);
}

ushort __fastcall getbits(int n)
{
    ushort x;

    x = bitbuf >> (2 * CHAR_BIT - n);
    fillbuf(n);
    return x;
}

/* Huffman decode routines */

static void
make_table(int nchar,
           uchar* bitlen,
           int tablebits,
           ushort* table,
           int tablesize)
{
    ushort count[17], weight[17], start[18], *p;
    uint i, k, len, ch, jutbits, avail, nextcode, mask;

    for (i = 1; i <= 16; i++)
        count[i] = 0;
    for (i = 0; (int)i < nchar; i++)
        count[bitlen[i]]++;

    start[1] = 0;
    for (i = 1; i <= 16; i++)
        start[i + 1] = start[i] + (count[i] << (16 - i));
    if (start[17] != (ushort)(1 << 16))
        ErrorSkip(AE_BADDATA);

    jutbits = 16 - tablebits;
    for (i = 1; (int)i <= tablebits; i++)
    {
        start[i] >>= jutbits;
        weight[i] = 1 << (tablebits - i);
    }
    while (i <= 16)
    {
        weight[i] = 1 << (16 - i);
        i++;
    }

    i = start[tablebits + 1] >> jutbits;
    if (i != (ushort)(1 << 16))
    {
        k = 1 << tablebits;
        while (i != k)
            table[i++] = 0;
    }

    avail = nchar;
    mask = 1 << (15 - tablebits);
    for (ch = 0; (int)ch < nchar; ch++)
    {
        if ((len = bitlen[ch]) == 0)
            continue;
        k = start[len];
        nextcode = k + weight[len];
        if ((int)len <= tablebits)
        {
            if (nextcode > (uint)tablesize)
                ErrorSkip(AE_BADDATA);
            for (i = start[len]; i < nextcode; i++)
                table[i] = ch;
        }
        else
        {
            p = &table[k >> jutbits];
            i = len - tablebits;
            while (i != 0)
            {
                if (*p == 0)
                {
                    right[avail] = left[avail] = 0;
                    *p = avail++;
                }
                if (k & mask)
                    p = &right[*p];
                else
                    p = &left[*p];
                k <<= 1;
                i--;
            }
            *p = ch;
        }
        start[len] = nextcode;
    }
}

static void
read_pt_len(int nn,
            int nbit,
            int i_special)
{
    int i, n;
    short c;
    ushort mask;

    n = getbits(nbit);
    if (n == 0)
    {
        c = getbits(nbit);
        for (i = 0; i < nn; i++)
            pt_len[i] = 0;
        for (i = 0; i < 256; i++)
            pt_table[i] = c;
    }
    else
    {
        i = 0;
        while (i < n)
        {
            c = bitbuf >> (13);
            if (c == 7)
            {
                mask = 1 << (12);
                while (mask & bitbuf)
                {
                    mask >>= 1;
                    c++;
                }
            }
            fillbuf((c < 7) ? 3 : (int)(c - 3));
            pt_len[i++] = (uchar)c;
            if (i == i_special)
            {
                c = getbits(2);
                while (--c >= 0)
                    pt_len[i++] = 0;
            }
        }
        while (i < nn)
            pt_len[i++] = 0;
        make_table(nn, pt_len, 8, pt_table, sizeof(pt_table));
    }
}

static void
read_c_len()
{
    short i, c, n;
    ushort mask;

    n = getbits(CBIT);
    if (n == 0)
    {
        c = getbits(CBIT);
        for (i = 0; i < NC; i++)
            c_len[i] = 0;
        for (i = 0; i < CTABLESIZE; i++)
            c_table[i] = c;
    }
    else
    {
        i = 0;
        while (i < n)
        {
            c = pt_table[bitbuf >> (8)];
            if (c >= NT)
            {
                mask = 1 << (7);
                do
                {
                    if (bitbuf & mask)
                        c = right[c];
                    else
                        c = left[c];
                    mask >>= 1;
                } while (c >= NT);
            }
            fillbuf((int)(pt_len[c]));
            if (c <= 2)
            {
                if (c == 0)
                    c = 1;
                else if (c == 1)
                    c = getbits(4) + 3;
                else
                    c = getbits(CBIT) + 20;
                while (--c >= 0)
                    c_len[i++] = 0;
            }
            else
                c_len[i++] = (uchar)(c - 2);
        }
        while (i < NC)
            c_len[i++] = 0;
        make_table(NC, c_len, 12, c_table, sizeof(c_table));
    }
}

static ushort
decode_c()
{
    ushort j, mask;

    if (blocksize == 0)
    {
        blocksize = getbits(16);
        read_pt_len(NT, TBIT, 3);
        read_c_len();
        read_pt_len(NP, PBIT, -1);
    }
    blocksize--;
    j = c_table[bitbuf >> 4];
    if (j >= NC)
    {
        mask = 1 << (3);
        do
        {
            if (bitbuf & mask)
                j = right[j];
            else
                j = left[j];
            mask >>= 1;
        } while (j >= NC);
    }
    fillbuf((int)(c_len[j]));
    return j;
}

static ushort
decode_p()
{
    ushort j, mask;

    j = pt_table[bitbuf >> (8)];
    if (j >= NP)
    {
        mask = 1 << (7);
        do
        {
            if (bitbuf & mask)
                j = right[j];
            else
                j = left[j];
            mask >>= 1;
        } while (j >= NP);
    }
    fillbuf((int)(pt_len[j]));
    if (j != 0)
    {
        j--;
        j = (1 << j) + getbits((int)j);
    }
    return j;
}

static void
decode_start()
{
    blocksize = 0;
    init_getbits();
}

void decode()
{
    short i;
    short j;
    short c;
    short r;
    long count;

    decode_start();
    count = 0;
    r = 0;

    while (count < origsize)
    {
        if ((c = decode_c()) <= UCHAR_MAX)
        {
            text[r] = (uchar)c;
            count++;
            if (++r >= DDICSIZ)
            {
                r = 0;
                WriteTxtCrc(text, DDICSIZ);
            }
        }
        else
        {
            j = c - (UCHAR_MAX + 1 - THRESHOLD);
            count += j;
            i = decode_p();
            if ((i = r - i - 1) < 0)
                i += DDICSIZ;
            if (r > i && r < DDICSIZ - MAXMATCH - 1)
            {
                while (--j >= 0)
                    text[r++] = text[i++];
            }
            else
            {
                while (--j >= 0)
                {
                    text[r] = text[i];
                    if (++r >= DDICSIZ)
                    {
                        r = 0;
                        WriteTxtCrc(text, DDICSIZ);
                    }
                    if (++i >= DDICSIZ)
                        i = 0;
                }
            }
        }
    }
    if (r != 0)
        WriteTxtCrc(text, r);
}

/* Macros */

#define BFIL \
    { \
        getbuf |= bitbuf >> getlen; \
        fillbuf(CODE_BIT - getlen); \
        getlen = CODE_BIT; \
    }
#define GETBIT(c) \
    { \
        if (getlen <= 0) \
            BFIL c = (getbuf & 0x8000) != 0; \
        getbuf <<= 1; \
        getlen--; \
    }
#define BPUL(l) \
    { \
        getbuf <<= l; \
        getlen -= l; \
    }
#define GETBITS(c, l) \
    { \
        if (getlen < l) \
            BFIL c = (ushort)getbuf >> (CODE_BIT - l); \
        BPUL(l) \
    }

static short
decode_ptr()
{
    short c;
    short width;
    short plus;
    short pwr;

    plus = 0;
    pwr = 1 << (STRTP);
    for (width = (STRTP); width < (STOPP); width++)
    {
        GETBIT(c);
        if (c == 0)
            break;
        plus += pwr;
        pwr <<= 1;
    }
    if (width != 0)
        GETBITS(c, width);
    c += plus;
    return c;
}

static short
decode_len()
{
    short c;
    short width;
    short plus;
    short pwr;

    plus = 0;
    pwr = 1 << (STRTL);
    for (width = (STRTL); width < (STOPL); width++)
    {
        GETBIT(c);
        if (c == 0)
            break;
        plus += pwr;
        pwr <<= 1;
    }
    if (width != 0)
        GETBITS(c, width);
    c += plus;
    return c;
}

void decode_f()
{
    short i;
    short j;
    short c;
    short r;
    short pos;
    long count;

    init_getbits();
    getlen = getbuf = 0;
    count = 0;
    r = 0;

    while (count < origsize)
    {
        c = decode_len();
        if (c == 0)
        {
            GETBITS(c, CHAR_BIT);
            text[r] = (uchar)c;
            count++;
            if (++r >= DDICSIZ)
            {
                r = 0;
                WriteTxtCrc(text, DDICSIZ);
            }
        }
        else
        {
            j = c - 1 + THRESHOLD;
            count += j;
            pos = decode_ptr();
            if ((i = r - pos - 1) < 0)
                i += DDICSIZ;
            while (j-- > 0)
            {
                text[r] = text[i];
                if (++r >= DDICSIZ)
                {
                    r = 0;
                    WriteTxtCrc(text, DDICSIZ);
                }
                if (++i >= DDICSIZ)
                    i = 0;
            }
        }
    }
    if (r != 0)
        WriteTxtCrc(text, r);
}

/* end DECODE.C */
