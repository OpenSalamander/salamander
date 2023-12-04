// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "../dlldefs.h"
#include "../fileio.h"
#include "gzip.h"

#include "..\tar.rh"
#include "..\tar.rh2"
#include "..\lang\lang.rh"

// Constants
// If BMAX needs to be larger than 16, then h and x[] should be ulg.
#define BMAX 16   // maximum bit length of any code (16 for explode)
#define N_MAX 288 // maximum number of codes in any set

// Huffman code lookup table entry--this entry is four bytes for machines
// that have 16-bit pointers (e.g. PC's in the small or medium model).
// Valid extra bits are 0..13.  e == 15 is EOB (end of block), e == 16
// means that v is a literal, 16 < e < 32 means that v is a pointer to
// the next table, which codes e - 16 bits, and lastly e == 99 indicates
// an unused code.  If a code with e == 99 is looked up, this implies an
// error in the data.
struct SHufTable
{
    unsigned char e; // number of extra bits or operation
    unsigned char b; // number of bits in this code or subcode
    union
    {
        unsigned short n; // literal, length base, or distance base
        SHufTable* t;     // pointer to next level of table
    } v;
};

// CRCs
unsigned long crc_32_tab[] =
    {
        0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
        0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
        0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
        0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
        0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
        0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
        0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
        0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
        0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
        0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
        0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
        0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
        0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
        0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
        0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
        0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
        0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
        0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
        0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
        0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
        0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
        0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
        0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
        0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
        0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
        0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
        0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
        0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
        0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
        0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
        0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
        0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
        0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
        0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
        0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
        0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
        0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
        0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
        0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
        0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
        0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
        0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
        0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
        0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
        0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
        0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
        0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
        0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
        0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
        0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
        0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
        0x2d02ef8dL};

// Tables for deflate from PKZIP's appnote.txt.
static unsigned long border[] = // Order of the bit length code lengths
    {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static unsigned short cplens[] = // Copy lengths for literal codes 257..285
    {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
     35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
// note: see note #13 above about the 258 in this list.
static unsigned short cplext[] = // Extra bits for literal codes 257..285
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
     3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 99, 99}; // 99 == invalid
static unsigned short cpdist[] =                     // Copy offsets for distance codes 0..29
    {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
     257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
     8193, 12289, 16385, 24577};
static unsigned short cpdext[] = // Extra bits for distance codes
    {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
     7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
     12, 12, 13, 13};

static unsigned short mask_bits[] =
    {
        0x0000,
        0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
        0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff};

// Macros for inflate() bit peeking and grabbing.
// The usage is:
//
//      NEEDBITS(j)
//      x = b & mask_bits[j];
//      DUMPBITS(j)
//
// where NEEDBITS makes sure that b has at least j bits in it, and
// DUMPBITS removes the bits from b.  The macros use the variable k
// for the number of bits in b.  Normally, b and k are register
// variables for speed, and are initialized at the beginning of a
// routine that uses these macros from a global bit buffer and count.
//
// If we assume that EOB will be the longest code, then we will never
// ask for bits with NEEDBITS that are beyond the end of the stream.
// So, NEEDBITS should not read any more bytes than are needed to
// meet the request.  Then no bytes need to be "returned" to the buffer
// at the end of the last block.
//
// However, this assumption is not true for fixed blocks--the EOB code
// is 7 bits, but the other literal/length codes can be 8 or 9 bits.
// (The EOB code is shorter than other codes because fixed blocks are
// generally short.  So, while a block always has an EOB, many other
// literal/length codes have a significantly lower probability of
// showing up at all.)  However, by making the first table have a
// lookup of seven bits, the EOB code will be found in that first
// lookup, and so will not require that too many bits be pulled from
// the stream.

#define NEEDBITS(n) \
    { \
        unsigned char needbits_tmp; \
        while (k < (n)) \
        { \
            needbits_tmp = FReadByte(); \
            if (!Ok) \
                return FALSE; \
            b |= ((unsigned long)needbits_tmp) << k; \
            k += 8; \
        } \
    }

#define DUMPBITS(n) \
    { \
        b >>= (n); \
        k -= (n); \
    }

// CRC computation
unsigned long UpdateCRC(unsigned long crc, const unsigned char* s, unsigned int n)
{
    unsigned long c = crc; // temporary variable

    if (s == NULL)
        c = 0;
    else
    {
        if (n)
            do
            {
                c = crc_32_tab[((int)c ^ (*s++)) & 0xff] ^ (c >> 8);
            } while (--n);
    }
    return c;
}

void MemCopy(unsigned char* dest, const unsigned char* src, int num)
{
    int c;

    if (src - dest >= (signed int)sizeof(long)) // NOTE: must remain as signed comparison
    {
        c = num / sizeof(long); // kolik intU muzem prenest ?
        while (c--)
        {
            *((unsigned long*)dest) = *((const unsigned long*)src);
            dest += sizeof(long);
            src += sizeof(long);
        }
        num = (num & (sizeof(long) - 1)); // kolik dela zbytek (jiz nutny presun po BYTU)
    }
    while (num--)
        *dest++ = *src++;
}

// Huffman code decoding is performed using a multi-level table lookup.
// The fastest way to decode is to simply build a lookup table whose
// size is determined by the longest code.  However, the time it takes
// to build this table can also be a factor if the data being decoded
// is not very long.  The most common codes are necessarily the
// shortest codes, so those codes dominate the decoding time, and hence
// the speed.  The idea is you can have a shorter table that decodes the
// shorter, more probable codes, and then point to subsidiary tables for
// the longer codes.  The time it costs to decode the longer codes is
// then traded against the time it takes to make longer tables.
//
// This results of this trade are in the variables lbits and dbits
// below.  lbits is the number of bits the first level table for literal/
// length codes can decode in one step, and dbits is the same thing for
// the distance codes.  Subsequent tables are also less than or equal to
// those sizes.  These values may be adjusted either when all of the
// codes are shorter than that, in which case the longest code length in
// bits is used, or when the shortest code is *longer* than the requested
// table size, in which case the length of the shortest code in bits is
// used.
//
// There are two different values for the two tables, since they code a
// different number of possibilities each.  The literal/length table
// codes 286 possible values, or in a flat code, a little over eight
// bits.  The distance table codes 30 possible values, or a little less
// than five bits, flat.  The optimum values for speed end up being
// about one bit more than those, so lbits is 8+1 and dbits is 5+1.
// The optimum values may differ though from machine to machine, and
// possibly even between compilers.  Your mileage may vary.

const int lbits = 9; // bits in base literal/length lookup table
const int dbits = 6; // bits in base distance lookup table

// Free the malloc'ed tables built by huft_build(), which makes a linked
// list of the tables it made, with the links in a dummy first entry of
// each table.
void CGZip::HufTableFree(SHufTable* t)
{
    SHufTable *p, *q;

    // Go through linked list, freeing from the malloced (t[-1]) address
    p = t;
    while (p != (SHufTable*)NULL)
    {
        q = (--p)->v.t;
        free(p);
        p = q;
    }
}

// Given a list of code lengths and a maximum table size, make a set of
// tables to decode that set of codes.  Return zero on success, one if
// the given code set is incomplete (the tables are still built in this
// case), two if the input is invalid (all zero length codes or an
// oversubscribed set of lengths), and three if not enough memory.
// The code with value 256 is special, and the tables are constructed
// so that no bits beyond that code are fetched when that code is
// decoded.
//    unsigned *codeLens;     code lengths in bits (all assumed <= BMAX)
//    unsigned codesNum;      number of codes (assumed <= N_MAX)
//    unsigned svcNum;        number of simple-valued codes (0..s-1)
//    ush *baseValues;        list of base values for non-simple codes
//    ush *extraBits;         list of extra bits for non-simple codes
//    struct huft **table;    result: starting table
//    int *lookupBits;        maximum lookup bits, returns actual
int CGZip::HufTableBuild(unsigned int* codeLens, unsigned int codesNum,
                         unsigned int svcNum, unsigned short* baseValues,
                         unsigned short* extraBits, SHufTable** table, int* lookupBits)
{
    unsigned a;           // counter for codes of length k
    unsigned c[BMAX + 1]; // bit length count table
    unsigned el;          // length of EOB code (value 256)
    unsigned f;           // i repeats in table every f entries
    int g;                // maximum code length
    int h;                // table level
    unsigned i;           // counter, current code
    unsigned j;           // counter
    int k;                // number of bits in current code
    int lx[BMAX + 1];     // memory for l[-1..BMAX-1]
    int* l = lx + 1;      // stack of bits per table
    unsigned* p;          // pointer into c[], b[], or v[]
    SHufTable* q;         // points to current table
    SHufTable r;          // table entry for structure assignment
    SHufTable* u[BMAX];   // table stack
    unsigned v[N_MAX];    // values in order of bit length
    int w;                // bits before this table == (l * h)
    unsigned x[BMAX + 1]; // bit offsets, then code stack
    unsigned* xp;         // pointer into x
    int y;                // number of dummy codes added
    unsigned z;           // number of entries in current table

    // Generate counts for each bit length
    *table = NULL;
    el = codesNum > 256 ? codeLens[256] : BMAX; // set length of EOB code, if any
    memset((char*)c, 0, sizeof(c));
    p = codeLens;
    i = codesNum;
    do
    {
        c[*p]++; // assume all entries <= BMAX
        p++;     // Can't combine with above line (Solaris bug)
    } while (--i);

    if (c[0] == codesNum) // null input--all zero length codes
    {
        *lookupBits = 0;
        return 0;
    }

    // Find minimum and maximum length, bound *lookupBits by those
    for (j = 1; j <= BMAX; j++)
        if (c[j])
            break;
    k = j; // minimum code length
    if ((unsigned)*lookupBits < j)
        *lookupBits = j;
    for (i = BMAX; i; i--)
        if (c[i])
            break;
    g = i; // maximum code length
    if ((unsigned)*lookupBits > i)
        *lookupBits = i;

    // Adjust last length count to fill out codes, if needed
    for (y = 1 << j; j < i; j++, y <<= 1)
        if ((y -= c[j]) < 0)
            return 2; // bad input: more codes than bits
    if ((y -= c[i]) < 0)
        return 2;
    c[i] += y;

    // Generate starting offsets into the value table for each length
    x[1] = j = 0;
    p = c + 1;
    xp = x + 2;
    while (--i) // note that i == g from above
        *xp++ = (j += *p++);

    // Make a table of values in order of bit lengths
    memset((char*)v, 0, sizeof(v));
    p = codeLens;
    i = 0;
    do
    {
        if ((j = *p++) != 0)
            v[x[j]++] = i;
    } while (++i < codesNum);
    codesNum = x[g]; // set n to length of v

    // Generate the Huffman codes and for each, make the table entries
    x[0] = i = 0;  // first Huffman code is zero
    p = v;         // grab values in bit order
    h = -1;        // no tables yet--level -1
    w = l[-1] = 0; // no bits decoded yet
    u[0] = NULL;   // clear unused variables
    q = NULL;
    z = 0;

    // go through the bit lengths (k already is bits in shortest code)
    for (; k <= g; k++)
    {
        a = c[k];
        while (a--)
        {
            // here i is the Huffman code of length k bits for value *p
            // make tables up to required level
            while (k > w + l[h])
            {
                w += l[h++]; // add bits already decoded

                // compute minimum size table less than or equal to l bits
                z = (z = g - w) > (unsigned)*lookupBits ? *lookupBits : z; // upper limit on table size
                if ((f = 1 << (j = k - w)) > a + 1)                        // try a k-w bit table
                {                                                          // too few codes for k-w bit table
                    f -= a + 1;                                            // deduct codes from patterns left
                    xp = c + k;
                    while (++j < z) // try smaller tables up to z bits
                    {
                        if ((f <<= 1) <= *++xp)
                            break; // enough codes to use up j bits
                        f -= *xp;  // else deduct codes from patterns
                    }
                }
                if ((unsigned)w + j > el && (unsigned)w < el)
                    j = el - w; // make EOB code end at table
                z = 1 << j;     // table entries for j-bit table
                l[h] = j;       // set table size in stack

                // allocate and link in new table
                if ((q = (SHufTable*)malloc((z + 1) * sizeof(SHufTable))) == NULL)
                {
                    Ok = FALSE;
                    ErrorCode = IDS_ERR_MEMORY;
                    if (h)
                        HufTableFree(u[0]);
                    return 3; // not enough memory
                }
                *table = q + 1; // link to list for huft_free()
                table = &(q->v.t);
                *table = NULL;
                u[h] = ++q; // table starts after link

                // connect to last table, if there is one
                if (h)
                {
                    x[h] = i;                      // save pattern for backing up
                    r.b = (unsigned char)l[h - 1]; // bits to dump before this table
                    r.e = (unsigned char)(16 + j); // bits in this table
                    r.v.t = q;                     // pointer to this table
                    j = (i & ((1 << w) - 1)) >> (w - l[h - 1]);
                    u[h - 1][j] = r; // connect to last table
                }
            }

            // set up table entry in r
            r.b = (unsigned char)(k - w);
            if (p >= v + codesNum)
                r.e = 99; // out of values--invalid code
            else if (*p < svcNum)
            {
                r.e = (unsigned char)(*p < 256 ? 16 : 15); // 256 is end-of-block code
                r.v.n = (unsigned short)(*p);              // simple code is just the value
                p++;                                       // one compiler does not like *p++
            }
            else
            {
                if (extraBits == NULL || baseValues == NULL)
                    return 2;
                r.e = (unsigned char)extraBits[*p - svcNum]; // non-simple--look up in lists
                r.v.n = baseValues[*p++ - svcNum];
            }

            // fill code-like entries with r
            f = 1 << (k - w);
            for (j = i >> w; j < z; j += f)
                q[j] = r;

            // backwards increment the k-bit code i
            for (j = 1 << (k - 1); i & j; j >>= 1)
                i ^= j;
            i ^= j;

            // backup over finished tables
            while ((i & ((1 << w) - 1)) != x[h])
            {
                h--; // don't need to update q
                w -= l[h];
            }
        }
    }

    // return actual size of base table
    *lookupBits = l[0];

    // Return true (1) if we were given an incomplete table
    return (y != 0 && g != 1) ? 1 : 0;
}

// decompress an inflated type 2 (dynamic Huffman codes) block
BOOL CGZip::InflateDynamicInit()
{
    int i; // temporary variables
    unsigned j;
    unsigned l;            // last length
    unsigned m;            // mask for bit lengths table
    unsigned n;            // number of lengths to get
    unsigned nb;           // number of bit length codes
    unsigned nl;           // number of literal/length codes
    unsigned nd;           // number of distance codes
    unsigned ll[286 + 30]; // literal/length and distance code lengths
    unsigned long b;       // bit buffer
    unsigned k;            // number of bits in bit buffer

    // make local bit buffer
    b = BitBuffer;
    k = BitCount;

    // read in table lengths
    NEEDBITS(5);
    nl = 257 + ((unsigned)b & 0x1f); // number of literal/length codes
    DUMPBITS(5);
    NEEDBITS(5);
    nd = 1 + ((unsigned)b & 0x1f); // number of distance codes
    DUMPBITS(5);
    NEEDBITS(4);
    nb = 4 + ((unsigned)b & 0xf); // number of bit length codes
    DUMPBITS(4);
    if (nl > 286 || nd > 32)
    {
        Ok = FALSE;
        ErrorCode = IDS_ERR_CORRUPT;
        return FALSE;
    }

    // read in bit-length-code lengths
    for (j = 0; j < nb; j++)
    {
        NEEDBITS(3);
        ll[border[j]] = (unsigned)b & 7;
        DUMPBITS(3);
    }
    for (; j < 19; j++)
        ll[border[j]] = 0;

    // build decoding table for trees--single level, 7 bit lookup
    LiteralBits = 7;
    i = HufTableBuild(ll, 19, 19, NULL, NULL, &LiteralTable, &LiteralBits);
    if (LiteralBits == 0)
        i = 1;
    if (i != 0)
    {
        if (i == 1) // incomplete code set
            HufTableFree(LiteralTable);
        LiteralTable = NULL;
        if (i != 3)
        {
            Ok = FALSE;
            ErrorCode = IDS_ERR_CORRUPT;
        }
        return FALSE;
    }

    // read in literal and distance code lengths
    n = nl + nd;
    m = mask_bits[LiteralBits];
    i = l = 0;
    SHufTable* tmpTable;
    while ((unsigned)i < n)
    {
        NEEDBITS((unsigned)LiteralBits);
        j = (tmpTable = LiteralTable + ((unsigned)b & m))->b;
        DUMPBITS(j);
        j = (tmpTable)->v.n;
        if (j < 16)          // length of code in bits (0..15)
            ll[i++] = l = j; // save last length in l
        else if (j == 16)    // repeat last length 3 to 6 times
        {
            NEEDBITS(2);
            j = 3 + ((unsigned)b & 3);
            DUMPBITS(2);
            if ((unsigned)i + j > n)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_CORRUPT;
                return FALSE;
            }
            while (j--)
                ll[i++] = l;
        }
        else
        {
            if (j == 17) // 3 to 10 zero length codes
            {
                NEEDBITS(3);
                j = 3 + ((unsigned)b & 7);
                DUMPBITS(3);
            }
            else // j == 18: 11 to 138 zero length codes
            {
                NEEDBITS(7);
                j = 11 + ((unsigned)b & 0x7f);
                DUMPBITS(7);
            }
            if ((unsigned)i + j > n)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_CORRUPT;
                return FALSE;
            }
            while (j--)
                ll[i++] = 0;
            l = 0;
        }
    }

    // free decoding table for trees
    HufTableFree(LiteralTable);
    LiteralTable = NULL;

    // restore the global bit buffer
    BitBuffer = b;
    BitCount = k;

    // build the decoding tables for literal/length and distance codes
    LiteralBits = lbits;
    i = HufTableBuild(ll, nl, 257, cplens, cplext, &LiteralTable, &LiteralBits);
    if (LiteralBits == 0)
        i = 1;
    if (i != 0)
    {
        if (i == 1)
            HufTableFree(LiteralTable);
        LiteralTable = NULL;
        if (i != 3)
        {
            Ok = FALSE;
            ErrorCode = IDS_ERR_CORRUPT;
        }
        return FALSE;
    }
    DistanceBits = dbits;
    i = HufTableBuild(ll + nl, nd, 0, cpdist, cpdext, &DistanceTable, &DistanceBits);
    if (DistanceBits == 0 && nl > 257) // lengths but no distances
    {
        Ok = FALSE;
        ErrorCode = IDS_ERR_CORRUPT;
        return FALSE;
    }
    if (i != 0)
    {
        if (i == 1)
            HufTableFree(DistanceTable);
        DistanceTable = NULL;
        if (i != 3)
        {
            Ok = FALSE;
            ErrorCode = IDS_ERR_CORRUPT;
        }
        return FALSE;
    }
    return TRUE;
}

// "decompress" an inflated type 0 (stored) block
BOOL CGZip::InflateStoredInit()
{
    unsigned long b; // bit buffer
    unsigned k;      // number of bits in bit buffer

    // make local copies of globals
    b = BitBuffer; // initialize bit buffer
    k = BitCount;

    // go to byte boundary
    StoredLen = k & 7;
    DUMPBITS(StoredLen);

    // get the length and its complement
    NEEDBITS(16);
    StoredLen = ((unsigned)b & 0xffff);
    DUMPBITS(16);
    NEEDBITS(16);
    if (StoredLen != (unsigned)((~b) & 0xffff))
    {
        ErrorCode = IDS_ERR_CORRUPT;
        Ok = FALSE;
        return FALSE;
    }
    DUMPBITS(16);

    // restore the globals from the locals
    BitBuffer = b; // restore global bit buffer
    BitCount = k;
    return TRUE;
}

// decompress an inflated type 1 (fixed Huffman codes) block.  We should
// either replace this with a custom decoder, or at least precompute the
// Huffman tables.
BOOL CGZip::InflateFixedInit()
{
    int i;           // temporary variable
    unsigned l[288]; // length list for huft_build

    // set up literal table
    for (i = 0; i < 144; i++)
        l[i] = 8;
    for (; i < 256; i++)
        l[i] = 9;
    for (; i < 280; i++)
        l[i] = 7;
    for (; i < 288; i++) // make a complete, but wrong code set
        l[i] = 8;
    FixedLiteralBits = 7;
    if ((i = HufTableBuild(l, 288, 257, cplens, cplext, &FixedLiteralTable, &FixedLiteralBits)) != 0)
    {
        if (i == 1) // incomplete code set
            HufTableFree(FixedLiteralTable);
        FixedLiteralTable = NULL;
        if (i != 3)
        {
            ErrorCode = IDS_ERR_CORRUPT;
            Ok = FALSE;
        }
        return FALSE;
    }

    // set up distance table
    for (i = 0; i < 30; i++) // make an incomplete code set
        l[i] = 5;
    FixedDistanceBits = 5;
    if ((i = HufTableBuild(l, 30, 0, cpdist, cpdext, &FixedDistanceTable, &FixedDistanceBits)) > 1)
    {
        if (i == 1) // incomplete code set
            HufTableFree(FixedDistanceTable);
        FixedDistanceTable = NULL;
        HufTableFree(FixedLiteralTable);
        FixedLiteralTable = NULL;
        if (i != 3)
        {
            ErrorCode = IDS_ERR_CORRUPT;
            Ok = FALSE;
        }
        return FALSE;
    }
    return TRUE;
}

// inflate (decompress) the codes in a deflated (compressed) block.
// Return an error code or zero if it all goes ok.
int CGZip::InflateCodes(SHufTable* literalTable, int literalBits,
                        SHufTable* distanceTable, int distanceBits)
{
    unsigned int e;            // table entry flag/number of extra bits
    unsigned int w;            // current window position
    SHufTable* t;              // pointer to table entry
    unsigned int literalMask;  // mask for bl bits
    unsigned int distanceMask; // mask for bd bits
    unsigned long b;           // bit buffer
    unsigned int k;            // number of bits in bit buffer

    // make local copies of globals
    b = BitBuffer; // initialize bit buffer
    k = BitCount;
    w = (unsigned int)(ExtrEnd - Window); // initialize window position

    // inflate the coded data
    literalMask = mask_bits[literalBits]; // precompute masks for speed
    distanceMask = mask_bits[distanceBits];

    if (CopyInProgress)
    {
        do
        {
            CopyCount -= (e = (e = BUFSIZE - ((CopyDistance &= BUFSIZE - 1) > w ? CopyDistance : w)) > CopyCount ? CopyCount : e);
            MemCopy(Window + w, Window + CopyDistance, e);
            w += e;
            CopyDistance += e;
        } while (CopyCount && w < BUFSIZE);
        if (CopyCount == BUFSIZE)
        {
            // restore the globals from the locals
            ExtrEnd = Window + w; // restore global window pointer
            BitBuffer = b;        // restore global bit buffer
            BitCount = k;
            return -1;
        }
        CopyInProgress = 0;
    }

    for (; w < BUFSIZE;)
    {
        NEEDBITS((unsigned)literalBits);
        if ((e = (t = literalTable + ((unsigned)b & literalMask))->e) > 16)
            do
            {
                if (e == 99)
                {
                    Ok = FALSE;
                    ErrorCode = IDS_ERR_CORRUPT;
                    return 0;
                }
                DUMPBITS(t->b);
                e -= 16;
                NEEDBITS(e);
            } while ((e = (t = t->v.t + ((unsigned)b & mask_bits[e]))->e) > 16);
        DUMPBITS(t->b);
        if (e == 16) // then it's a literal
            Window[w++] = (unsigned char)t->v.n;
        else // it's an EOB or a length
        {
            // exit if end of block
            if (e == 15)
                break;

            // get length of block to copy
            NEEDBITS(e);
            CopyCount = t->v.n + ((unsigned)b & mask_bits[e]);
            DUMPBITS(e);

            // decode distance of block to copy
            NEEDBITS((unsigned)distanceBits);
            if ((e = (t = distanceTable + ((unsigned)b & distanceMask))->e) > 16)
                do
                {
                    if (e == 99)
                    {
                        Ok = FALSE;
                        ErrorCode = IDS_ERR_CORRUPT;
                        return 0;
                    }
                    DUMPBITS(t->b);
                    e -= 16;
                    NEEDBITS(e);
                } while ((e = (t = t->v.t + ((unsigned)b & mask_bits[e]))->e) > 16);
            DUMPBITS(t->b);
            NEEDBITS(e);
            CopyDistance = w - t->v.n - ((unsigned)b & mask_bits[e]);
            DUMPBITS(e);

            // do the copy
            do
            {
                CopyCount -= (e = (e = BUFSIZE - ((CopyDistance &= BUFSIZE - 1) > w ? CopyDistance : w)) > CopyCount ? CopyCount : e);
                MemCopy(Window + w, Window + CopyDistance, e);
                w += e;
                CopyDistance += e;
            } while (CopyCount && w < BUFSIZE);
            if (w == BUFSIZE)
                CopyInProgress = 1;
        }
    }

    // restore the globals from the locals
    ExtrEnd = Window + w; // restore global window pointer
    BitBuffer = b;        // restore global bit buffer
    BitCount = k;

    if (w == BUFSIZE)
        return -1; // in progress

    // done
    return 1;
}

// "decompress" an inflated type 0 (stored) block
int CGZip::InflateStored()
{
    unsigned char* w; // current window position
    unsigned long b;  // bit buffer
    unsigned int k;   // number of bits in bit buffer

    // make local copies of globals
    b = BitBuffer; // initialize bit buffer
    k = BitCount;
    w = ExtrEnd; // initialize window position

    // read and output the compressed data
    while (StoredLen && w < Window + BUFSIZE)
    {
        StoredLen--;
        NEEDBITS(8);
        *(w++) = (unsigned char)b;
        DUMPBITS(8);
    }

    // restore the globals from the locals
    ExtrEnd = w;   // restore global window pointer
    BitBuffer = b; // restore global bit buffer
    BitCount = k;
    return StoredLen > 0 ? -1 : 1;
}

// decompress an inflated block
BOOL CGZip::InflateBlock()
{
    int ret;

    if (!InProgress)
    {
        unsigned long b; // bit buffer
        unsigned k;      // number of bits in bit buffer

        // make local bit buffer
        b = BitBuffer;
        k = BitCount;

        // read in last block bit
        NEEDBITS(1);
        LastBlock = (int)b & 1;
        DUMPBITS(1);

        // read in block type
        NEEDBITS(2);
        BlockType = (unsigned)b & 3;
        DUMPBITS(2);

        // restore the global bit buffer
        BitBuffer = b;
        BitCount = k;

        // inflate that block type
        switch (BlockType)
        {
        case 0: // stored - no init
            if (!InflateStoredInit())
                return FALSE;
            break;
        case 1: // fixed Huffman code
            if (FixedLiteralTable == NULL || FixedDistanceTable == NULL)
                if (!InflateFixedInit())
                    return FALSE;
            break;
        case 2: // dynamic Huffman code
            if (!InflateDynamicInit())
                return FALSE;
            break;
        default: // bad block type
            Ok = FALSE;
            ErrorCode = IDS_GZERR_BADBLOCKTYPE;
            return FALSE;
        }
        InProgress = TRUE;
    }

    unsigned char* begin = ExtrEnd;
    switch (BlockType)
    {
    case 0: // stored
        ret = InflateStored();
        break;
    case 1: // fixed Huffman code
        ret = InflateCodes(FixedLiteralTable, FixedLiteralBits, FixedDistanceTable, FixedDistanceBits);
        break;
    case 2: // dynamic Huffman code
        ret = InflateCodes(LiteralTable, LiteralBits, DistanceTable, DistanceBits);
        break;
    default: // bad block type
        Ok = FALSE;
        ErrorCode = IDS_GZERR_BADBLOCKTYPE;
        return FALSE;
    }

    // update crc and extracted size
    unsigned short extracted = (unsigned short)(ExtrEnd - begin);
    TotalCnt += CQuadWord(extracted, 0);
    crc = UpdateCRC(crc, begin, extracted);

    if (ret != -1)
    {
        HufTableFree(LiteralTable);
        LiteralTable = NULL;
        HufTableFree(DistanceTable);
        DistanceTable = NULL;
        HufTableFree(FixedLiteralTable);
        FixedLiteralTable = NULL;
        HufTableFree(FixedDistanceTable);
        FixedDistanceTable = NULL;
        InProgress = FALSE;

        // nasli jsme posledni blok gzip archivu
        if (LastBlock)
        {
            // zkontrolujeme CRC
            Cleanup();

            // mame dost dat?
            if (DataEnd - DataStart < sizeof(SGZipHeader))
            {
                // nacteme dalsi
                FReadBlock(sizeof(SGZipHeader));
                // ale od-oznacime je jako prectene, oznacene budou v Initialize metode
                DataStart -= sizeof(SGZipHeader);
                StreamPos -= CQuadWord(sizeof(SGZipHeader), 0);
            }

            // update crc and extracted size
            if (Ok)
            {
                // check for next GZIP block
                LastBlock = !Initialize(ErrorCode);
            }
        }
    }

    return ret;
}

// class constructor
CGZip::CGZip(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize) : CZippedFile(filename, file, buffer, start, read, inputSize), CopyInProgress(FALSE), LastBlock(FALSE), CopyCount(0),
                                                                                                                                       CopyDistance(0), BlockType(0), LiteralTable(NULL), LiteralBits(0), DistanceTable(NULL),
                                                                                                                                       DistanceBits(0), FixedLiteralTable(NULL), FixedLiteralBits(0), FixedDistanceTable(NULL),
                                                                                                                                       FixedDistanceBits(0), StoredLen(0), BitBuffer(0), BitCount(0), InProgress(FALSE)
{
    CALL_STACK_MESSAGE2("CGZip::CGZip(%s, , , )", filename);

    if (Ok)
    {
        Ok = Initialize(ErrorCode);
        if (!Ok)
            FreeBufAndFile = FALSE;
    }
}

BOOL CGZip::Initialize(unsigned int& errorCode)
{
    CALL_STACK_MESSAGE1("CGZip::Initialize()");

    // no error by default
    errorCode = 0;

    // clear byte counter
    TotalCnt.Set(0, 0);
    // clear CRC
    crc = 0xffffffffL;

    // nemuze to byt gzip, pokud mame min nez hlavicku...
    if (DataEnd - DataStart < sizeof(SGZipHeader))
        return FALSE;

    // pokud neni "magicke cislo" na zacatku, nejde o gzip
    SGZipHeader* header = (SGZipHeader*)DataStart;
    if (header->Magic != GZIP_MAGIC &&
        header->Magic != OLD_GZIP_MAGIC)
        return FALSE;

    // mame gzip, potvrdime precteny header
    FReadBlock(sizeof(SGZipHeader));

    // ted zjistujeme vlstnosti archivu
    if (header->Method != METHOD_DEFLATE)
    {
        // supported are only deflated archives
        errorCode = IDS_GZERR_BADMETHOD;
        return FALSE;
    }
    // flagy si zapamatujeme, abychom mohli dal cist ze vstupu (sdilime buffer)
    unsigned char flags = header->Flags;
    if ((flags & ENCRYPTED) != 0)
    {
        // encryptene archivy neumime
        errorCode = IDS_GZERR_ENCRYPTED;
        return FALSE;
    }
    if ((flags & CONTINUATION) != 0)
    {
        // multipart archivy neumime
        errorCode = IDS_GZERR_MULTIPART;
        return FALSE;
    }
    if ((flags & RESERVED) != 0)
    {
        // divny flagy - ruce pryc...
        errorCode = IDS_GZERR_BADFLAGS;
        return FALSE;
    }
    // pokud je pritomen continuation header, musime ho preskocit
    if ((flags & CONTINUATION) != 0)
    {
        // Multi-part input !!!!
        SGzipContinuationHeader* contHeader = (SGzipContinuationHeader*)FReadBlock(sizeof(SGzipContinuationHeader));
        if (contHeader == NULL)
        {
            errorCode = ErrorCode;
            return FALSE;
        }
    }
    // pokud je pritomen extra header, musime ho preskocit
    if ((flags & EXTRA_FIELD) != 0)
    {
        // nacteme zacatek extra headeru
        SGzipExtraHeader* exHeader = (SGzipExtraHeader*)FReadBlock(sizeof(SGzipExtraHeader));
        if (exHeader == NULL)
        {
            errorCode = ErrorCode;
            return FALSE;
        }
        // a preskocime zbytek
        unsigned short skip = exHeader->FieldLen; // muze byt vetsi nez BUFSIZE, v tom pripade preskakujeme postupne po BUFSIZE
        while (skip > 0)
        {
            const unsigned char* tmp = FReadBlock(skip > BUFSIZE ? BUFSIZE : skip);
            if (tmp == NULL)
            {
                errorCode = ErrorCode;
                return FALSE;
            }
            skip -= skip > BUFSIZE ? BUFSIZE : skip;
        }
    }
    // nacteme puvodni jmeno, pokud je pritomno
    if ((flags & ORIG_NAME) != 0)
    {
        char buffer[MAX_PATH], *tmp;
        unsigned char src;
        tmp = buffer;
        while (Ok && (src = FReadByte()) != '\0' && tmp - buffer < sizeof(buffer) - 1)
        {
            if (src != '/')
            {
                // FIXME: Remove all invalid characters
                *tmp++ = src == ':' ? '_' : src;
            }
            else
            {
                // If path is given (e.g. /var/mailman/archives/private/grass5/2005-July.txt), take only file name
                tmp = buffer;
            }
        }
        *tmp = '\0';
        // prectem zbytek nazvu, kterej je na nas moc dlouhej
        while (Ok && src != '\0')
            src = FReadByte();
        if (!Ok)
        {
            errorCode = ErrorCode;
            return FALSE;
        }
        SetOldName(buffer);
    }
    // preskocime komentar, pokud je pritomen
    if ((flags & COMMENT) != 0)
    {
        unsigned char src;
        while (Ok && (src = FReadByte()) != '\0')
            ;
        if (!Ok)
        {
            errorCode = ErrorCode;
            return FALSE;
        }
    }
    // hotovo
    return TRUE;
}

// class cleanup
CGZip::~CGZip()
{
    CALL_STACK_MESSAGE1("CGZip::~CGZip()");

    HufTableFree(LiteralTable);
    HufTableFree(DistanceTable);
    HufTableFree(FixedLiteralTable);
    HufTableFree(FixedDistanceTable);
}

void CGZip::Cleanup()
{
    CALL_STACK_MESSAGE1("CGZip::Cleanup()");

    // check crc
    uint32_t orig_crc; // original crc
    uint32_t orig_len; // original uncompressed length

    if (IsOk())
    {
        // Undo too much lookahead. The next read will be byte aligned so we
        // can discard unused bits in the last meaningful byte.
        orig_crc = 0;
        int tmpCount = 0;
        while (BitCount >= 8)
        {
            orig_crc <<= 8;
            orig_crc |= BitBuffer & 0xFF;
            BitBuffer >>= 8;
            BitCount -= 8;
            tmpCount++;
        }
        BitBuffer = 0;
        BitCount = 0;

        // read in the crc stored at the end of the file
        while (tmpCount < 4)
        {
            unsigned char tmp = FReadByte();
            if (!IsOk())
                break;
            orig_crc = (orig_crc >> 8) | (tmp << 24);
            tmpCount++;
        }
        // check CRC and extracted size
        if (IsOk())
        {
            uint32_t* tmp = (uint32_t*)FReadBlock(sizeof(uint32_t));
            if (tmp != NULL)
            {
                orig_len = *tmp;
                if (orig_crc != (crc ^ 0xffffffffL)) // (instead of ~c for 64-bit machines)
                {
                    Ok = FALSE;
                    ErrorCode = IDS_GZERR_CRC;
                }
                else
                    // NOTE: GZIP stores orig_len as only 32bit value
                    // -> to avoid false warnings on files over 4GB we compare only lower 32 bits here
                    if (orig_len != TotalCnt.LoDWord)
                    {
                        Ok = FALSE;
                        ErrorCode = IDS_ERR_EOF;
                    }
            }
        }
    }
    if (ErrorCode != 0)
        SalamanderGeneral->ShowMessageBox(LoadErr(ErrorCode, LastError), LoadStr(IDS_GZERR_TITLE),
                                          MSGBOX_ERROR);
}

void CGZip::GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr)
{
    CALL_STACK_MESSAGE1("CGZip::GetFileInfo(,,)");

    // Neexistuje spolehliva cesta jak zjistit velikost rozbalenych dat,
    // informace na konci souboru byt nemusi, pokud se jedna o vice-souborovy
    // gzip, pak bychom museli rozbalit cely archiv abychom nasli vsechny
    // slozky. Navic ulozena velikost je jen modulo 2^32. Proste velikost neni...
    fileSize.Set(0, 0);

    // zjistime zbytek
    BY_HANDLE_FILE_INFORMATION fileinfo;
    if (GetFileInformationByHandle(File, &fileinfo))
    {
        lastWrite = fileinfo.ftLastWriteTime;
        fileAttr = fileinfo.dwFileAttributes;
    }
    else
    {
        fileAttr = FILE_ATTRIBUTE_ARCHIVE;
        lastWrite.dwLowDateTime = 0;
        lastWrite.dwHighDateTime = 0;
    }
}

BOOL CGZip::CompactBuffer()
{
    CALL_STACK_MESSAGE1("CGZip::CompactBuffer()");

    // musime soupnout datama tak, abychom meli dost mista
    // ale nesmime premazat ani ta stara - deflate z gzipu je muze pouzit
    // nejjednodussi je realokaci uplne jinam :-)
    unsigned char* tmp = (unsigned char*)malloc(BUFSIZE);
    if (tmp == NULL)
    {
        ErrorCode = IDS_ERR_MEMORY;
        Ok = FALSE;
        return FALSE;
    }
    unsigned int diff = (unsigned int)(ExtrStart - Window);
    memcpy(tmp, ExtrStart, BUFSIZE - diff);
    memcpy(tmp + (BUFSIZE - diff), Window, diff);
    // pokud je dekomprese uprostred bloku, updatneme pointery
    if (CopyInProgress)
    {
        if (CopyDistance < diff)
            CopyDistance += BUFSIZE;
        CopyDistance -= diff;
    }
    // pust stary buffer
    free(Window);
    // a pouzij novy
    ExtrEnd = tmp + (ExtrEnd - ExtrStart);
    ExtrStart = tmp;
    Window = tmp;
    return TRUE;
}

BOOL CGZip::DecompressBlock(unsigned short needed)
{
    unsigned char* begin = ExtrEnd;
    // doplnime buffer az do konce...
    while ((InProgress || !LastBlock) && ExtrEnd < Window + BUFSIZE)
    {
        if (!InflateBlock())
            return FALSE;
    }
    return (ExtrEnd - ExtrStart >= needed);
}
