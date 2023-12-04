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
//#include "memapi.h"

/* modified by Lukas in February 2000

/*---------------------------------------------------------------------------

  unshrink.c                     version 1.21                     23 Nov 95


       NOTE:  This code may or may not infringe on the so-called "Welch
       patent" owned by Unisys.  (From reading the patent, it appears
       that a pure LZW decompressor is *not* covered, but this claim has
       not been tested in court, and Unisys is reported to believe other-
       wise.)  It is therefore the responsibility of the user to acquire
       whatever license(s) may be required for legal use of this code.

       THE INFO-ZIP GROUP DISCLAIMS ALL LIABILITY FOR USE OF THIS CODE
       IN VIOLATION OF APPLICABLE PATENT LAW.


  Shrinking is basically a dynamic LZW algorithm with allowed code sizes of
  up to 13 bits; in addition, there is provision for partial clearing of
  leaf nodes.  PKWARE uses the special code 256 (decimal) to indicate a
  change in code size or a partial clear of the code tree:  256,1 for the
  former and 256,2 for the latter.  [Note that partial clearing can "orphan"
  nodes:  the parent-to-be can be cleared before its new child is added,
  but the child is added anyway (as an orphan, as though the parent still
  existed).  When the tree fills up to the point where the parent node is
  reused, the orphan is effectively "adopted."  Versions prior to 1.05 were
  affected more due to greater use of pointers (to children and siblings
  as well as parents).]

  This replacement version of unshrink.c was written from scratch.  It is
  based only on the algorithms described in Mark Nelson's _The Data Compres-
  sion Book_ and in Terry Welch's original paper in the June 1984 issue of
  IEEE _Computer_; no existing source code, including any in Nelson's book,
  was used.

  Memory requirements have been reduced in this version and are now no more
  than the original Sam Smith code.  This is still larger than any of the
  other algorithms:  at a minimum, 8K+8K+16K (stack+values+parents) assuming
  16-bit short ints, and this does not even include the output buffer (the
  other algorithms leave the uncompressed data in the work area, typically
  called slide[]).  For machines with a 64KB data space this is a problem,
  particularly when text conversion is required and line endings have more
  than one character.  UnZip's solution is to use two roughly equal halves
  of outbuf for the ASCII conversion in such a case; the "unshrink" argument
  to flush() signals that this is the case.

  For large-memory machines, a second outbuf is allocated for translations,
  but only if unshrinking and only if translations are required.

              | binary mode  |        text mode
    ---------------------------------------------------
    big mem   |  big outbuf  | big outbuf + big outbuf2  <- malloc'd here
    small mem | small outbuf | half + half small outbuf

  Copyright 1994, 1995 Greg Roelofs.  See the accompanying file "COPYING"
  in UnZip 5.20 (or later) source or binary distributions.

  ---------------------------------------------------------------------------*/

void partial_clear(short* parent);

//#define MAX_BITS    13                 /* used in unshrink() */
//#define HSIZE       (1 << MAX_BITS)    /* size of global work area */

#define HSIZE (1 << 13)
#define BOGUSCODE 256
#define FLAG_BITS parent       /* upper bits of parent[] used as flag bits */
#define CODE_MASK (HSIZE - 1)  /* 0x1fff (lower bits are parent's index) */
#define FREE_CODE HSIZE        /* 0x2000 (code is unused or was cleared) */
#define HAS_CHILD (HSIZE << 1) /* 0x4000 (code has a child--do not clear) */

//#define parent G.area.shrink.Parent
//#define Value  G.area.shrink.value /* "value" conflicts with Pyramid ioctl.h */
//#define stack  G.area.shrink.Stack

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

#define READBITS(dec, done, nbits, zdest) \
    { \
        if (nbits > (dec)->Input->BitCount2) \
        { \
            int temp; \
            done = 1; \
            while ((dec)->Input->BitCount2 <= 8 * (int)(sizeof(ulg) - 1) && (temp = NextByte2(dec)) != EOF) \
            { \
                (dec)->Input->BitBuf |= (ulg)temp << (dec)->Input->BitCount2; \
                (dec)->Input->BitCount2 += 8; \
                done = 0; \
            } \
            if ((dec)->Input->Error) \
                return 1; \
        } \
        zdest = (short)((ush)(dec)->Input->BitBuf & mask_bits[nbits]); \
        (dec)->Input->BitBuf >>= nbits; \
        (dec)->Input->BitCount2 -= nbits; \
    }

/***********************/
/* Function unshrink() */
/***********************/

int Unshrink(CDecompressionObject* decompress)
{
    /* !!tady mnely nekde chybu!! (asi HSIZE+1)
    G.area.shrink.Parent = (shrint *)G.area.Slide;
    G.area.shrink.value = G.area.Slide + (sizeof(shrint)*(HSIZE+1));
    G.area.shrink.Stack = G.area.Slide +
                           (sizeof(shrint) + sizeof(uch))*(HSIZE+1);
    */
    short* parent = (short*)decompress->Output->SlideWin;
    uch* Value = decompress->Output->SlideWin + sizeof(short) * (HSIZE);
    uch* stack = decompress->Output->SlideWin +
                 (sizeof(short) + sizeof(uch)) * (HSIZE);
    int offset = (HSIZE - 1);
    uch* stacktop = stack + offset;
    uch* newstr;
    int codesize = 9, len, KwKwK;
    short code, oldcode, freecode, curcode;
    short lastfreecode;
    unsigned int outbufsiz;
    uch* outptr;
    unsigned outcnt;
    int done = false;

    decompress->Input->BitCount2 = 0;
    decompress->Input->BitBuf = 0;

    /*---------------------------------------------------------------------------
    Initialize various variables.
  ---------------------------------------------------------------------------*/

    lastfreecode = BOGUSCODE;

    for (code = 0; code < BOGUSCODE; ++code)
    {
        Value[code] = (uch)code;
        parent[code] = BOGUSCODE;
    }
    for (code = BOGUSCODE + 1; code < HSIZE; ++code)
        parent[code] = FREE_CODE;

    outbufsiz = decompress->Output->BufSize;
    outptr = decompress->Output->OutBuf;
    outcnt = 0L;

    /*---------------------------------------------------------------------------
    Get and output first code, then loop over remaining ones.
  ---------------------------------------------------------------------------*/

    READBITS(decompress, done, codesize, oldcode)
    if (!done)
    {
        *outptr++ = (uch)oldcode;
        ++outcnt;
    }

    do
    {
        READBITS(decompress, done, codesize, code)
        if (done)
            break;
        if (code == BOGUSCODE)
        { /* possible to have consecutive escapes? */
            READBITS(decompress, done, codesize, code)
            if (code == 1)
            {
                ++codesize;
            }
            else if (code == 2)
            {
                partial_clear(parent);    /* clear leafs (nodes with no children) */
                lastfreecode = BOGUSCODE; /* reset start of free-node search */
            }
            continue;
        }

        /*-----------------------------------------------------------------------
        Translate code:  traverse tree from leaf back to root.
      -----------------------------------------------------------------------*/

        newstr = stacktop;
        curcode = code;

        if (parent[curcode] == FREE_CODE)
        {
            /* or (FLAG_BITS[curcode] & FREE_CODE)? */
            KwKwK = TRUE;
            --newstr; /* last character will be same as first character */
            curcode = oldcode;
        }
        else
            KwKwK = FALSE;

        do
        {
            *newstr-- = Value[curcode];
            curcode = (short)(parent[curcode] & CODE_MASK);
        } while (curcode != BOGUSCODE);

        len = (int)(stacktop - newstr++);
        if (KwKwK)
            *stacktop = *newstr;

        /*-----------------------------------------------------------------------
        Write expanded string in reverse order to output buffer.
      -----------------------------------------------------------------------*/

        {
            uch* p;

            for (p = newstr; p < newstr + len; ++p)
            {
                *outptr++ = *p;
                if (++outcnt == outbufsiz)
                {
                    if (decompress->Output->Flush(outcnt, decompress))
                    {
                        TRACE_E("inflate_codes: flush returned error");
                        return 2;
                    }
                    outptr = decompress->Output->OutBuf;
                    outcnt = 0L;
                }
            }
        }

        /*-----------------------------------------------------------------------
        Add new leaf (first character of newstr) to tree as child of oldcode.
      -----------------------------------------------------------------------*/

        /* search for freecode */
        freecode = (short)(lastfreecode + 1);
        /* add if-test before loop for speed? */
        while (parent[freecode] != FREE_CODE)
            ++freecode;
        lastfreecode = freecode;

        Value[freecode] = *newstr;
        parent[freecode] = oldcode;
        oldcode = code;

    } while (!done);

    /*---------------------------------------------------------------------------
    Flush any remaining data and return to sender...
  ---------------------------------------------------------------------------*/

    if (outcnt > 0 && decompress->Output->Flush(outcnt, decompress))
    {
        TRACE_E("inflate_codes: flush returned error");
        return 2;
    }

    return 0;

} /* end function unshrink() */

/****************************/
/* Function partial_clear() */ /* no longer recursive... */
/****************************/

void partial_clear(short* parent)
{
    short code;

    /* clear all nodes which have no children (i.e., leaf nodes only) */

    /* first loop:  mark each parent as such */
    for (code = BOGUSCODE + 1; code < HSIZE; ++code)
    {
        short cparent = (short)(parent[code] & CODE_MASK);

        if (cparent > BOGUSCODE && cparent != FREE_CODE)
            FLAG_BITS[cparent] |= HAS_CHILD; /* set parent's child-bit */
    }

    /* second loop:  clear all nodes *not* marked as parents; reset flag bits */
    for (code = BOGUSCODE + 1; code < HSIZE; ++code)
    {
        if (FLAG_BITS[code] & HAS_CHILD) /* just clear child-bit */
            FLAG_BITS[code] &= ~HAS_CHILD;
        else
        { /* leaf:  lose it */
            parent[code] = FREE_CODE;
        }
    }

    return;
}

#undef READBITS

#pragma runtime_checks("", restore)
