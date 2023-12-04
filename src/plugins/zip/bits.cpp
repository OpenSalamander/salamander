// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>

#ifdef ZIP_DLL
#include "spl_base.h"
#include "dbg.h"
#endif //ZIP_DLL

#include "config.h"
#include "deflate.h"

#pragma runtime_checks("c", off)

const int _extra_lbits[LENGTH_CODES] =
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
/* extra bits for each length code */
const int _extra_dbits[D_CODES] =
    {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
/* extra bits for each distance code */
const int _extra_blbits[BL_CODES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7};
/* extra bits for each bit length code */

const uch _bl_order[BL_CODES] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
/* The lengths of the bit length codes are sent in order of decreasing
 * probability, to avoid transmitting the lengths for unused bit length codes.
 */

/* Values for max_lazy_match, good_match and max_chain_length, depending on
 * the desired pack level (0..9). The values given below have been tuned to
 * exclude worst case performance for pathological files. Better values may be
 * found for specific files.
 */
const config _configuration_table[10] =
    {
        /*      good lazy nice chain */
        /* 0 */ {0, 0, 0, 0}, /* store only */
        /* 1 */ {4, 4, 8, 4}, /* maximum speed, no lazy matches */
        /* 2 */ {4, 5, 16, 8},
        /* 3 */ {4, 6, 32, 32},

        /* 4 */ {4, 4, 16, 16}, /* lazy matches */
        /* 5 */ {8, 16, 32, 32},
        /* 6 */ {8, 16, 128, 128},
        /* 7 */ {8, 32, 128, 256},
        /* 8 */ {32, 128, 258, 1024},
        /* 9 */ {32, 258, 258, 4096} /* maximum compression */
};

CDeflate::CDeflate()
{
#ifdef ZIP_DLL
    CALL_STACK_MESSAGE1("CDeflate::CDeflate()");
#endif //ZIP_DLL
    int i;
    //initialization of deflate object

    //******* TREES ********
    for (i = 0; i < LENGTH_CODES; i++)
        extra_lbits[i] = _extra_lbits[i];
    for (i = 0; i < D_CODES; i++)
        extra_dbits[i] = _extra_dbits[i];
    for (i = 0; i < BL_CODES; i++)
        extra_blbits[i] = _extra_blbits[i];

    l_desc.dyn_tree = dyn_ltree;
    l_desc.static_tree = static_ltree;
    l_desc.extra_bits = extra_lbits;
    l_desc.extra_base = LITERALS + 1;
    l_desc.elems = L_CODES;
    l_desc.max_length = MAX_BITS;
    l_desc.max_code = 0;

    d_desc.dyn_tree = dyn_dtree;
    d_desc.static_tree = static_dtree;
    d_desc.extra_bits = extra_dbits;
    d_desc.extra_base = 0;
    d_desc.elems = D_CODES;
    d_desc.max_length = MAX_BITS;
    d_desc.max_code = 0;

    bl_desc.dyn_tree = bl_tree;
    bl_desc.static_tree = NULL;
    bl_desc.extra_bits = extra_blbits;
    bl_desc.extra_base = 0;
    bl_desc.elems = BL_CODES;
    bl_desc.max_length = MAX_BL_BITS;
    bl_desc.max_code = 0;

    for (i = 0; i < BL_CODES; i++)
        bl_order[i] = _bl_order[i];

    //******* DEFLATE ******
    for (i = 0; i < 10; i++)
        configuration_table[i] = _configuration_table[i];

    static_dtree[0].Len = 0; //ct_init not called
}

#define GPF_ENCRYPTED 0x01

int CDeflate::Deflate(ush* internAttr, int* compMethod, int compLevel,
                      ush* flag, ullg* compLen, FWriteData writeFunc,
                      FReadData readFunc, void* userData)
{
#ifdef ZIP_DLL
    CALL_STACK_MESSAGE2("CDeflate::Deflate( , , %d, , , , , )", compLevel);
#endif //ZIP_DLL
    try
    {
        UserData = userData;
        WriteData = writeFunc;
        ReadData = readFunc;
        Flag = *flag;
        level = compLevel;
        //encrypted = *flag & GPF_ENCRYPTED ? true  : false;
        bi_init();
        ct_init(internAttr, compMethod);
        lm_init(level, flag);
        *compLen = deflate();
        return 0;
    }
    catch (int errorID)
    {
        return errorID;
    }
}

void CDeflate::bi_init() /* output zip file, NULL for in-memory compression */
{
    bi_buf = 0;
    bi_valid = 0;

    /* Set the defaults for file compression. They are set by memcompress
     * for in-memory compression.
     */
    out_buf = file_outbuf;
    out_size = sizeof(file_outbuf);
    out_offset = 0;
}

void CDeflate::send_bits(int value,  /* value to send */
                         int length) /* number of bits */
{
    /* If not enough room in bi_buf, use (valid) bits from bi_buf and
     * (16 - bi_valid) bits from value, leaving (width - (16-bi_valid))
     * unused bits in value.
     */
    if (bi_valid > (int)Buf_size - length)
    {
        bi_buf |= (value << bi_valid);
        PUTSHORT(bi_buf);
        bi_buf = (ush)value >> (Buf_size - bi_valid);
        bi_valid += length - Buf_size;
    }
    else
    {
        bi_buf |= value << bi_valid;
        bi_valid += length;
    }
}

/* ===========================================================================
 * Reverse the first len bits of a code, using straightforward code (a faster
 * method would use a table)
 * IN assertion: 1 <= len <= 15
 */
unsigned CDeflate::bi_reverse(unsigned code, /* the value to invert */
                              int len)       /* its bit length */
{
    unsigned res = 0;
    do
    {
        res |= code & 1;
        code >>= 1, res <<= 1;
    } while (--len > 0);
    return res >> 1;
}

/* ===========================================================================
 * Flush the current output buffer.
 */
void CDeflate::flush_outbuf(unsigned w,     /* value to flush */
                            unsigned bytes) /* number of bytes to flush (0, 1 or 2) */
{
    int errorID;
    /* Encrypt and write the output buffer: */
    if (out_offset != 0)
    {
        errorID = WriteData(out_buf, out_offset, UserData);
        if (errorID)
            Error(errorID);
    }
    out_offset = 0;
    if (bytes == 2)
    {
        PUTSHORT(w);
    }
    else if (bytes == 1)
    {
        out_buf[out_offset++] = (char)(w & 0xff);
    }
}

/* ===========================================================================
 * Write out any remaining bits in an incomplete byte.
 */
void CDeflate::bi_windup()
{
    if (bi_valid > 8)
    {
        PUTSHORT(bi_buf);
    }
    else if (bi_valid > 0)
    {
        PUTBYTE(bi_buf);
    }
    flush_outbuf(0, 0);
    bi_buf = 0;
    bi_valid = 0;
}

/* ===========================================================================
 * Copy a stored block to the zip file, storing first the length and its
 * one's complement if requested.
 */
void CDeflate::copy_block(char* block,  /* the input data */
                          unsigned len, /* its length */
                          int header)   /* true if block header must be written */
{
    int errorID;

    bi_windup(); /* align on byte boundary */

    if (header)
    {
        PUTSHORT((ush)len);
        PUTSHORT((ush)~len);
    }

    flush_outbuf(0, 0);
    errorID = WriteData(block, len, UserData);
    if (errorID)
        Error(errorID);
}
