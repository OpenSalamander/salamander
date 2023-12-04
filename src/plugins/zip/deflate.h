// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

typedef int (*FWriteData)(char*, unsigned, void*);
typedef int (*FReadData)(char*, unsigned, int*, void*);

// deflate error codes
// 0 success
// 1 bad pack level
// 2 block vanished

#ifndef WIN32
#define WIN32
#endif
#define WINDLL

//********* ZIP *************

#define MIN_MATCH 3
#define MAX_MATCH 258
/* The minimum and maximum match lengths */

#ifndef WSIZE
#define WSIZE (0x8000)
#endif
/* Maximum window size = 32K. If you are really short of memory, compile
 * with a smaller WSIZE but this reduces the compression ratio for files
 * of size > WSIZE. WSIZE must be a power of two in the current implementation.
 */

#define MIN_LOOKAHEAD (MAX_MATCH + MIN_MATCH + 1)
/* Minimum amount of lookahead, except at the end of the input file.
 * See deflate.c for comments about the MIN_MATCH+1.
 */

#define MAX_DIST (WSIZE - MIN_LOOKAHEAD)
/* In order to simplify the code, particularly on 16 bit machines, match
 * distances are limited to MAX_DIST instead of WSIZE.
 */

/* internal file attribute */
#define UNKNOWN (-1)
#define BINARY 0
#define ASCII 1

#define STORE 0   /* Store method */
#define DEFLATE 8 /* Deflation method*/

#ifdef ZIP_ASSERT
#define Assert(cond, msg) \
    { \
        if (!(cond)) \
            TRACE_E(msg); \
    }
#else
#define Assert(cond, msg)
#endif

//#define TRACE_C(c,x) {if (c) TRACE_I(x);}

//********* BITS ************

#define Buf_size (8 * 2 * sizeof(char))
/* Number of bits used within bi_buf. (bi_buf might be implemented on
 * more than 16 bits on some systems.)
 */

/* Output a 16 bit value to the bit stream, lower (oldest) byte first */
#define PUTSHORT(w) \
    { \
        if (out_offset < out_size - 1) \
        { \
            out_buf[out_offset++] = (char)((w)&0xff); \
            out_buf[out_offset++] = (char)((ush)(w) >> 8); \
        } \
        else \
        { \
            flush_outbuf((w), 2); \
        } \
    }

#define PUTBYTE(b) \
    { \
        if (out_offset < out_size) \
        { \
            out_buf[out_offset++] = (char)(b); \
        } \
        else \
        { \
            flush_outbuf((b), 1); \
        } \
    }

//********* TREES ************

/* ===========================================================================
 * Constants
 */

#define MAX_BITS 15
/* All codes must not exceed MAX_BITS bits */

#define MAX_BL_BITS 7
/* Bit length codes must not exceed MAX_BL_BITS bits */

#define LENGTH_CODES 29
/* number of length codes, not counting the special END_BLOCK code */

#define LITERALS 256
/* number of literal bytes 0..255 */

#define END_BLOCK 256
/* end of block literal code */

#define L_CODES (LITERALS + 1 + LENGTH_CODES)
/* number of Literal or Length codes, including the END_BLOCK code */

#define D_CODES 30
/* number of distance codes */

#define BL_CODES 19
/* number of codes used to transfer the bit lengths */

#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES 2
/* The three kinds of block type */

#ifndef LIT_BUFSIZE
#ifdef SMALL_MEM
#define LIT_BUFSIZE 0x2000
#else
#ifdef MEDIUM_MEM
#define LIT_BUFSIZE 0x4000
#else
#define LIT_BUFSIZE 0x8000
#endif
#endif
#endif
#define DIST_BUFSIZE LIT_BUFSIZE
/* Sizes of match buffers for literals/lengths and distances.  There are
 * 4 reasons for limiting LIT_BUFSIZE to 64K:
 *   - frequencies can be kept in 16 bit counters
 *   - if compression is not successful for the first block, all input data is
 *     still in the window so we can still emit a stored block even when input
 *     comes from standard input.  (This can also be done for all blocks if
 *     LIT_BUFSIZE is not greater than 32K.)
 *   - if compression is not successful for a file smaller than 64K, we can
 *     even emit a stored file instead of a stored block (saving 5 bytes).
 *   - creating new Huffman trees less frequently may not provide fast
 *     adaptation to changes in the input data statistics. (Take for
 *     example a binary file with poorly compressible code followed by
 *     a highly compressible string table.) Smaller buffer sizes give
 *     fast adaptation but have of course the overhead of transmitting trees
 *     more frequently.
 *   - I can't count above 4
 * The current code is general and allows DIST_BUFSIZE < LIT_BUFSIZE (to save
 * memory at the expense of compression). Some optimizations would be possible
 * if we rely on DIST_BUFSIZE == LIT_BUFSIZE.
 */

#define REP_3_6 16
/* repeat previous bit length 3-6 times (2 bits of repeat count) */

#define REPZ_3_10 17
/* repeat a zero length 3-10 times  (3 bits of repeat count) */

#define REPZ_11_138 18
/* repeat a zero length 11-138 times  (7 bits of repeat count) */

#define Freq fc.freq
#define Code fc.code
#define Dad dl.dad
#define Len dl.len

#define HEAP_SIZE (2 * L_CODES + 1)
/* maximum heap size */

/* Data structure describing a single value and its code string. */
typedef struct ct_data
{
    union
    {
        ush freq; /* frequency count */
        ush code; /* bit string */
    } fc;
    union
    {
        ush dad; /* father node in Huffman tree */
        ush len; /* length of bit string */
    } dl;
} ct_data;

typedef struct tree_desc
{
    ct_data near* dyn_tree;    /* the dynamic tree */
    ct_data near* static_tree; /* corresponding static tree or NULL */
    int near* extra_bits;      /* extra bits for each code or NULL */
    int extra_base;            /* base index for extra_bits */
    int elems;                 /* max number of elements in the tree */
    int max_length;            /* max bit length for the codes */
    int max_code;              /* largest code with non zero frequency */
} tree_desc;

#define send_code(c, tree) send_bits(tree[c].Code, tree[c].Len)
/* Send a code of the given tree. c and tree must not have side effects */

#define d_code(dist) \
    ((dist) < 256 ? dist_code[dist] : dist_code[256 + ((dist) >> 7)])
/* Mapping from a distance to a distance code. dist is the distance - 1 and
 * must not have side effects. dist_code[256] and dist_code[257] are never
 * used.
 */

#define Max(a, b) (a >= b ? a : b)
/* the arguments must not have side effects */

#define SMALLEST 1
/* Index within the heap array of least frequent node in the Huffman tree */

/* ===========================================================================
 * Remove the smallest element from the heap and recreate the heap with
 * one less element. Updates heap and heap_len.
 */
#define pqremove(tree, top) \
    { \
        top = heap[SMALLEST]; \
        heap[SMALLEST] = heap[heap_len--]; \
        pqdownheap(tree, SMALLEST); \
    }

/* ===========================================================================
 * Compares to subtrees, using the tree depth as tie breaker when
 * the subtrees have equal frequency. This minimizes the worst case length.
 */
#define smaller(tree, n, m) \
    (tree[n].Freq < tree[m].Freq || \
     (tree[n].Freq == tree[m].Freq && depth[n] <= depth[m]))

//********** DEFLATE ************

/* ===========================================================================
 * Configuration parameters
 */

/* Compile with MEDIUM_MEM to reduce the memory requirements or
 * with SMALL_MEM to use as little memory as possible. Use BIG_MEM if the
 * entire input file can be held in memory (not possible on 16 bit systems).
 * Warning: defining these symbols affects HASH_BITS (see below) and thus
 * affects the compression ratio. The compressed output
 * is still correct, and might even be smaller in some cases.
 */

#ifdef SMALL_MEM
#define HASH_BITS 13 /* Number of bits used to hash strings */
#endif
#ifdef MEDIUM_MEM
#define HASH_BITS 14
#endif
#ifndef HASH_BITS
#define HASH_BITS 15
/* For portability to 16 bit machines, do not use values above 15. */
#endif

#define HASH_SIZE (unsigned)(1 << HASH_BITS)
#define HASH_MASK (HASH_SIZE - 1)
#define WMASK (WSIZE - 1)
/* HASH_SIZE and WSIZE must be powers of two */

#define NIL 0
/* Tail of hash chains */

#define FAST 4
#define SLOW 2
/* speed options for the general purpose bit flag */

#ifndef TOO_FAR
#define TOO_FAR 4096
#endif
/* Matches of length 3 are discarded if their distance exceeds TOO_FAR */

#ifdef MEMORY16
#define MAXSEG_64K
#endif

/* ===========================================================================
 * Local data used by the "longest match" routines.
 */

#if defined(BIG_MEM) || defined(MMAP)
typedef unsigned Pos; /* must be at least 32 bits */
#else
typedef ush Pos;
#endif
typedef unsigned IPos;
/* A Pos is an index in the character window. We use short instead of int to
 * save space in the various tables. IPos is used only for parameter passing.
 */

#define H_SHIFT ((HASH_BITS + MIN_MATCH - 1) / MIN_MATCH)
/* Number of bits by which ins_h and del_h must be shifted at each
 * input step. It must be such that after MIN_MATCH steps, the oldest
 * byte no longer takes part in the hash key, that is:
 *   H_SHIFT * MIN_MATCH >= HASH_BITS
 */

#define max_insert_length max_lazy_match
/* Insert new strings in the hash table only if the match length
 * is not greater than this length. This saves time but degrades compression.
 * max_insert_length is used only for compression levels <= 3.
 */

typedef struct config
{
    ush good_length; /* reduce lazy search above this match length */
    ush max_lazy;    /* do not perform lazy search above this match length */
    ush nice_length; /* quit search above this match length */
    ush max_chain;
} config;

#ifdef FULL_SEARCH
#define nice_match MAX_MATCH
#endif

#define EQUAL 0
/* result of memcmp for equal strings */

/* ===========================================================================
 * Update a hash value with the given input byte
 * IN  assertion: all calls to to UPDATE_HASH are made with consecutive
 *    input characters, so that a running hash key can be computed from the
 *    previous key instead of complete recalculation each time.
 */
#define UPDATE_HASH(h, c) (h = (((h) << H_SHIFT) ^ (c)) & HASH_MASK)

/* ===========================================================================
 * Insert string s in the dictionary and set match_head to the previous head
 * of the hash chain (the most recent string with same hash key). Return
 * the previous length of the hash chain.
 * IN  assertion: all calls to to INSERT_STRING are made with consecutive
 *    input characters and the first MIN_MATCH bytes of s are valid
 *    (except for the last MIN_MATCH-1 bytes of the input file).
 */
#define INSERT_STRING(s, match_head) \
    (UPDATE_HASH(ins_h, window[(s) + (MIN_MATCH - 1)]), \
     prev[(s)&WMASK] = match_head = head[ins_h], \
     head[ins_h] = (s))

/* ===========================================================================
 * Flush the current block, with given end-of-file flag.
 * IN assertion: strstart is set to the end of the current match.
 */
#define FLUSH_BLOCK(eof) \
    flush_block(block_start >= 0L ? (char*)&window[(unsigned)block_start] : (char*)NULL, (long)strstart - block_start, (eof))

class CDeflate
{

    int level;
    /* 0=fastest compression, 9=best compression */

    //********* BITS ************

    /*

     Copyright (C) 1990-1997 Mark Adler, Richard B. Wales, Jean-loup Gailly,
     Kai Uwe Rommel, Onno van der Linden and Igor Mandrichenko.
     Permission is granted to any individual or institution to use, copy, or
     redistribute this software so long as all of the original files are included,
     that it is not sold for profit, and that this copyright notice is retained.

    */

    /*
     *  bits.c by Jean-loup Gailly and Kai Uwe Rommel.
     *
     *  This is a new version of im_bits.c originally written by Richard B. Wales
     *
     *  PURPOSE
     *
     *      Output variable-length bit strings. Compression can be done
     *      to a file or to memory.
     *
     *  DISCUSSION
     *
     *      The PKZIP "deflate" file format interprets compressed file data
     *      as a sequence of bits.  Multi-bit strings in the file may cross
     *      byte boundaries without restriction.
     *
     *      The first bit of each byte is the low-order bit.
     *
     *      The routines in this file allow a variable-length bit value to
     *      be output right-to-left (useful for literal values). For
     *      left-to-right output (useful for code strings from the tree routines),
     *      the bits must have been reversed first with bi_reverse().
     *
     *      For in-memory compression, the compressed bit stream goes directly
     *      into the requested output buffer. The input data is read in blocks
     *      by the mem_read() function. The buffer is limited to 64K on 16 bit
     *      machines.
     *
     *  INTERFACE
     *
     *      void bi_init (FILE *zipfile)
     *          Initialize the bit string routines.
     *
     *      void send_bits (int value, int length)
     *          Write out a bit string, taking the source bits right to
     *          left.
     *
     *      int bi_reverse (int value, int length)
     *          Reverse the bits of a bit string, taking the source bits left to
     *          right and emitting them right to left.
     *
     *      void bi_windup (void)
     *          Write out any remaining bits in an incomplete byte.
     *
     *      void copy_block(char far *buf, unsigned len, int header)
     *          Copy a stored block to the zip file, storing first the length and
     *          its one's complement if requested.
     *
     *      int seekable(void)
     *          Return true if the zip file can be seeked.
     *
     *      ulg memcompress (char *tgt, ulg tgtsize, char *src, ulg srcsize);
     *          Compress the source buffer src into the target buffer tgt.
     */

    /* ===========================================================================
     * Local data used by the "bit string" routines.
     */

    unsigned short bi_buf;
    /* Output buffer. bits are inserted starting at the bottom (least significant
     * bits).
     */

    int bi_valid;
    /* Number of valid bits in bi_buf.  All bits above the last valid bit
     * are always zero.
     */

    char file_outbuf[1024];
    /* Output buffer for compression to file */

    char *in_buf, *out_buf;
    /* Current input and output buffers. in_buf is used only for in-memory
     * compression.
     */

    unsigned in_offset, out_offset;
    /* Current offset in input and output buffers. in_offset is used only for
     * in-memory compression. On 16 bit machines, the buffer is limited to 64K.
     */

    unsigned in_size, out_size;
    /* Size of current input and output buffers */

    int read_buf(char* buf, unsigned size);
    /* Current input function. Set to mem_read for in-memory compression */

    /* ===========================================================================
     * Initialize the bit string routines.
     */
    void bi_init();

    /* ===========================================================================
     * Send a value on a given number of bits.
     * IN assertion: length <= 16 and value fits in length bits.
     */
    void send_bits(int value, int length);

    /* ===========================================================================
     * Reverse the first len bits of a code, using straightforward code (a faster
     * method would use a table)
     * IN assertion: 1 <= len <= 15
     */
    unsigned bi_reverse(unsigned code, int len);

    /* ===========================================================================
     * Flush the current output buffer.
     */
    void flush_outbuf(unsigned w, unsigned bytes);

    /* ===========================================================================
     * Write out any remaining bits in an incomplete byte.
     */
    void bi_windup();

    /* ===========================================================================
     * Copy a stored block to the zip file, storing first the length and its
     * one's complement if requested.
     */
    void copy_block(char* block, unsigned len, int header);

    /* ===========================================================================
     * Return true if the zip file can be seeked. This is used to check if
     * the local header can be re-rewritten. This function always returns
     * true for in-memory compression.
     * IN assertion: the local header has already been written (ftell() > 0).
     */
    int seekable()
    {
        return 1;
    }

    //********** TREES ***********

    /*

     Copyright (C) 1990-1997 Mark Adler, Richard B. Wales, Jean-loup Gailly,
     Kai Uwe Rommel, Onno van der Linden and Igor Mandrichenko.
     Permission is granted to any individual or institution to use, copy, or
     redistribute this software so long as all of the original files are included,
     that it is not sold for profit, and that this copyright notice is retained.

    */

    /*
     *  trees.c by Jean-loup Gailly
     *
     *  This is a new version of im_ctree.c originally written by Richard B. Wales
     *  for the defunct implosion method.
     *
     *  PURPOSE
     *
     *      Encode various sets of source values using variable-length
     *      binary code trees.
     *
     *  DISCUSSION
     *
     *      The PKZIP "deflation" process uses several Huffman trees. The more
     *      common source values are represented by shorter bit sequences.
     *
     *      Each code tree is stored in the ZIP file in a compressed form
     *      which is itself a Huffman encoding of the lengths of
     *      all the code strings (in ascending order by source values).
     *      The actual code strings are reconstructed from the lengths in
     *      the UNZIP process, as described in the "application note"
     *      (APPNOTE.TXT) distributed as part of PKWARE's PKZIP program.
     *
     *  REFERENCES
     *
     *      Lynch, Thomas J.
     *          Data Compression:  Techniques and Applications, pp. 53-55.
     *          Lifetime Learning Publications, 1985.  ISBN 0-534-03418-7.
     *
     *      Storer, James A.
     *          Data Compression:  Methods and Theory, pp. 49-50.
     *          Computer Science Press, 1988.  ISBN 0-7167-8156-5.
     *
     *      Sedgewick, R.
     *          Algorithms, p290.
     *          Addison-Wesley, 1983. ISBN 0-201-06672-6.
     *
     *  INTERFACE
     *
     *      void ct_init (ush *attr, int *method)
     *          Allocate the match buffer, initialize the various tables and save
     *          the location of the internal file attribute (ascii/binary) and
     *          method (DEFLATE/STORE)
     *
     *      void ct_tally (int dist, int lc);
     *          Save the match info and tally the frequency counts.
     *
     *      long flush_block (char *buf, ulg stored_len, int eof)
     *          Determine the best encoding for the current block: dynamic trees,
     *          static trees or store, and output the encoded block to the zip
     *          file. Returns the total compressed length for the file so far.
     *
     */

    int extra_lbits[LENGTH_CODES]; /* extra bits for each length code */
    int extra_dbits[D_CODES];      /* extra bits for each distance code */
    int extra_blbits[BL_CODES];    /* extra bits for each bit length code */

    ct_data dyn_ltree[HEAP_SIZE];       /* literal and length tree */
    ct_data dyn_dtree[2 * D_CODES + 1]; /* distance tree */

    ct_data static_ltree[L_CODES + 2];
    /* The static literal tree. Since the bit lengths are imposed, there is no
     * need for the L_CODES extra codes used during heap construction. However
     * The codes 286 and 287 are needed to build a canonical tree (see ct_init
     * below).
     */

    ct_data static_dtree[D_CODES];
    /* The static distance tree. (Actually a trivial tree since all codes use
     * 5 bits.)
     */

    ct_data bl_tree[2 * BL_CODES + 1];
    /* Huffman tree for the bit lengths */

    tree_desc l_desc;
    tree_desc d_desc;
    tree_desc bl_desc;

    ush bl_count[MAX_BITS + 1];
    /* number of codes at each bit length for an optimal tree */

    uch bl_order[BL_CODES];
    /* The lengths of the bit length codes are sent in order of decreasing
     * probability, to avoid transmitting the lengths for unused bit length codes.
     */

    int heap[2 * L_CODES + 1]; /* heap used to build the Huffman trees */
    int heap_len;              /* number of elements in the heap */
    int heap_max;              /* element of largest frequency */
    /* The sons of heap[n] are heap[2*n] and heap[2*n+1]. heap[0] is not used.
     * The same heap array is used to build all trees.
     */

    uch depth[2 * L_CODES + 1];
    /* Depth of each subtree used as tie breaker for trees of equal frequency */

    uch length_code[MAX_MATCH - MIN_MATCH + 1];
    /* length code for each normalized match length (0 == MIN_MATCH) */

    uch dist_code[512];
    /* distance codes. The first 256 values correspond to the distances
     * 3 .. 258, the last 256 values correspond to the top 8 bits of
     * the 15 bit distances.
     */

    int near base_length[LENGTH_CODES];
    /* First normalized length for each code (0 = MIN_MATCH) */

    int near base_dist[D_CODES];
    /* First normalized distance for each code (0 = distance of 1) */

#ifndef DYN_ALLOC
    uch l_buf[LIT_BUFSIZE];  /* buffer for literals/lengths */
    ush d_buf[DIST_BUFSIZE]; /* buffer for distances */
#else
    uch* l_buf;
    ush* d_buf;
#endif

    uch flag_buf[(LIT_BUFSIZE / 8)];
    /* flag_buf is a bit array distinguishing literals from lengths in
     * l_buf, and thus indicating the presence or absence of a distance.
     */

    unsigned last_lit;   /* running index in l_buf */
    unsigned last_dist;  /* running index in d_buf */
    unsigned last_flags; /* running index in flag_buf */
    uch flags;           /* current flags not yet saved in flag_buf */
    uch flag_bit;        /* current bit used in flags */
    /* bits are filled in flags starting at bit 0 (least significant).
     * Note: these flags are overkill in the current code since we don't
     * take advantage of DIST_BUFSIZE == LIT_BUFSIZE.
     */

    ulg opt_len;    /* bit length of current block with optimal trees */
    ulg static_len; /* bit length of current block with static trees */

    ullg cmpr_bytelen; /* total byte length of compressed file */
    ulg cmpr_len_bits; /* number of bits past 'cmpr_bytelen' */

    ulg input_len; /* total byte length of input file */
    /* input_len is for debugging only since we can get it by other means. */

    ush* file_type;   /* pointer to UNKNOWN, BINARY or ASCII */
    int* file_method; /* pointer to DEFLATE or STORE */

    /* ===========================================================================
     * Allocate the match buffer, initialize the various tables and save the
     * location of the internal file attribute (ascii/binary) and method
     * (DEFLATE/STORE).
     */
    void ct_init(ush* attr, int* method);

    /* ===========================================================================
     * Initialize a new block.
     */
    void init_block();

    /* ===========================================================================
     * Restore the heap property by moving down the tree starting at node k,
     * exchanging a node with the smallest of its two sons if necessary, stopping
     * when the heap property is re-established (each father smaller than its
     * two sons).
     */
    void pqdownheap(ct_data* tree, int k);

    /* ===========================================================================
     * Compute the optimal bit lengths for a tree and update the total bit length
     * for the current block.
     * IN assertion: the fields freq and dad are set, heap[heap_max] and
     *    above are the tree nodes sorted by increasing frequency.
     * OUT assertions: the field len is set to the optimal bit length, the
     *     array bl_count contains the frequencies for each bit length.
     *     The length opt_len is updated; static_len is also updated if stree is
     *     not null.
     */
    void gen_bitlen(tree_desc* desc);

    /* ===========================================================================
     * Generate the codes for a given tree and bit counts (which need not be
     * optimal).
     * IN assertion: the array bl_count contains the bit length statistics for
     * the given tree and the field len is set for all tree elements.
     * OUT assertion: the field code is set for all tree elements of non
     *     zero code length.
     */
    void gen_codes(ct_data* tree, int max_code);

    /* ===========================================================================
     * Construct one Huffman tree and assigns the code bit strings and lengths.
     * Update the total bit length for the current block.
     * IN assertion: the field freq is set for all tree elements.
     * OUT assertions: the fields len and code are set to the optimal bit length
     *     and corresponding code. The length opt_len is updated; static_len is
     *     also updated if stree is not null. The field max_code is set.
     */
    void build_tree(tree_desc* desc);

    /* ===========================================================================
     * Scan a literal or distance tree to determine the frequencies of the codes
     * in the bit length tree. Updates opt_len to take into account the repeat
     * counts. (The contribution of the bit length codes will be added later
     * during the construction of bl_tree.)
     */
    void scan_tree(ct_data* tree, int max_code);

    /* ===========================================================================
     * Send a literal or distance tree in compressed form, using the codes in
     * bl_tree.
     */
    void send_tree(ct_data* tree, int max_code);

    /* ===========================================================================
     * Construct the Huffman tree for the bit lengths and return the index in
     * bl_order of the last bit length code to send.
     */
    int build_bl_tree();

    /* ===========================================================================
     * Send the header for a block using dynamic Huffman trees: the counts, the
     * lengths of the bit length codes, the literal tree and the distance tree.
     * IN assertion: lcodes >= 257, dcodes >= 1, blcodes >= 4.
     */
    void send_all_trees(int lcodes, int dcodes, int blcodes);

    /* ===========================================================================
     * Determine the best encoding for the current block: dynamic trees, static
     * trees or store, and output the encoded block to the zip file. This function
     * returns the total compressed length for the file so far.
     */
    ullg flush_block(char* buf, ulg stored_len, int eof);

    /* ===========================================================================
     * Save the match info and tally the frequency counts. Return true if
     * the current block must be flushed.
     */
    int ct_tally(int dist, int lc);

    /* ===========================================================================
     * Send the block data compressed using the given Huffman trees
     */
    void compress_block(ct_data* ltree, ct_data near* dtree);

    /* ===========================================================================
     * Set the file type to ASCII or BINARY, using a crude approximation:
     * binary if more than 20% of the bytes are <= 6 or >= 128, ascii otherwise.
     * IN assertion: the fields freq of dyn_ltree are set and the total of all
     * frequencies does not exceed 64K (to fit in an int on 16 bit machines).
     */
    void set_file_type();

    //********* DEFLATE **********

    /*

     Copyright (C) 1990-1997 Mark Adler, Richard B. Wales, Jean-loup Gailly,
     Kai Uwe Rommel, Onno van der Linden and Igor Mandrichenko.
     Permission is granted to any individual or institution to use, copy, or
     redistribute this software so long as all of the original files are included,
     that it is not sold for profit, and that this copyright notice is retained.

    */

    /*
     *  deflate.c by Jean-loup Gailly.
     *
     *  PURPOSE
     *
     *      Identify new text as repetitions of old text within a fixed-
     *      length sliding window trailing behind the new text.
     *
     *  DISCUSSION
     *
     *      The "deflation" process depends on being able to identify portions
     *      of the input text which are identical to earlier input (within a
     *      sliding window trailing behind the input currently being processed).
     *
     *      The most straightforward technique turns out to be the fastest for
     *      most input files: try all possible matches and select the longest.
     *      The key feature of this algorithm is that insertions into the string
     *      dictionary are very simple and thus fast, and deletions are avoided
     *      completely. Insertions are performed at each input character, whereas
     *      string matches are performed only when the previous match ends. So it
     *      is preferable to spend more time in matches to allow very fast string
     *      insertions and avoid deletions. The matching algorithm for small
     *      strings is inspired from that of Rabin & Karp. A brute force approach
     *      is used to find longer strings when a small match has been found.
     *      A similar algorithm is used in comic (by Jan-Mark Wams) and freeze
     *      (by Leonid Broukhis).
     *         A previous version of this file used a more sophisticated algorithm
     *      (by Fiala and Greene) which is guaranteed to run in linear amortized
     *      time, but has a larger average cost, uses more memory and is patented.
     *      However the F&G algorithm may be faster for some highly redundant
     *      files if the parameter max_chain_length (described below) is too large.
     *
     *  ACKNOWLEDGEMENTS
     *
     *      The idea of lazy evaluation of matches is due to Jan-Mark Wams, and
     *      I found it in 'freeze' written by Leonid Broukhis.
     *      Thanks to many info-zippers for bug reports and testing.
     *
     *  REFERENCES
     *
     *      APPNOTE.TXT documentation file in PKZIP 1.93a distribution.
     *
     *      A description of the Rabin and Karp algorithm is given in the book
     *         "Algorithms" by R. Sedgewick, Addison-Wesley, p252.
     *
     *      Fiala,E.R., and Greene,D.H.
     *         Data Compression with Finite Windows, Comm.ACM, 32,4 (1989) 490-595
     *
     *  INTERFACE
     *
     *      void lm_init (int pack_level, ush *flags)
     *          Initialize the "longest match" routines for a new file
     *
     *      ulg deflate (void)
     *          Processes a new input file and return its compressed length. Sets
     *          the compressed length, crc, deflate flags and internal file
     *          attributes.
     */

#ifndef DYN_ALLOC
    uch window[2L * WSIZE];
    /* Sliding window. Input bytes are read into the second half of the window,
       * and move to the first half later to keep a dictionary of at least WSIZE
       * bytes. With this organization, matches are limited to a distance of
       * WSIZE-MAX_MATCH bytes, but this ensures that IO is always
       * performed with a length multiple of the block size. Also, it limits
       * the window size to 64K, which is quite useful on MSDOS.
       * To do: limit the window size to WSIZE+CBSZ if SMALL_MEM (the code would
       * be less efficient since the data would have to be copied WSIZE/CBSZ times)
       */
    Pos prev[WSIZE];
    /* Link to older string with same hash index. To limit the size of this
       * array to 64K, this link is maintained only for the last 32K strings.
       * An index in this array is thus a window index modulo 32K.
       */
    Pos head[HASH_SIZE];
    /* Heads of the hash chains or NIL. If your compiler thinks that
       * HASH_SIZE is a dynamic value, recompile with -DDYN_ALLOC.
       */
#else
    uch far* near window = NULL;
    Pos far* near prev = NULL;
    Pos far* near head;
#endif
    ulg window_size;
    /* window size, 2*WSIZE except for MMAP or BIG_MEM, where it is the
     * input file length plus MIN_LOOKAHEAD.
     */

    long block_start;
    /* window position at the beginning of the current output block. Gets
     * negative when the window is moved backwards.
     */

    int sliding;
    /* Set to false when the input file is already in memory */

    unsigned ins_h; /* hash index of string to be inserted */

    unsigned int near prev_length;
    /* Length of the best match at previous step. Matches not greater than this
     * are discarded. This is used in the lazy match evaluation.
     */

    unsigned strstart;    /* start of string to insert */
    unsigned match_start; /* start of matching string */
    int eofile;           /* flag set at end of input file */
    unsigned lookahead;   /* number of valid bytes ahead in window */

    unsigned max_chain_length;
    /* To speed up deflation, hash chains are never searched beyond this length.
     * A higher limit improves compression ratio but degrades the speed.
     */

    unsigned int max_lazy_match;
    /* Attempt to find a better match only when the current match is strictly
     * smaller than this value. This mechanism is used only for compression
     * levels >= 4.
     */

    unsigned near good_match;
    /* Use a faster search when the previous match is longer than this */

    /* Values for max_lazy_match, good_match and max_chain_length, depending on
     * the desired pack level (0..9). The values given below have been tuned to
     * exclude worst case performance for pathological files. Better values may be
     * found for specific files.
     */

    config configuration_table[10];

    /* Note: the deflate() code requires max_lazy >= MIN_MATCH and max_chain >= 4
     * For deflate_fast() (levels <= 3) good is ignored and lazy has a different
     * meaning.
     */

#ifndef FULL_SEARCH
    int nice_match; /* Stop searching when current match exceeds this */
#endif

    /* ===========================================================================
     * Initialize the "longest match" routines for a new file
     *
     * IN assertion: window_size is > 0 if the input file is already read or
     *    mmap'ed in the window[] array, 0 otherwise. In the first case,
     *    window_size is sufficient to contain the whole input file plus
     *    MIN_LOOKAHEAD bytes (to avoid referencing memory beyond the end
     *    of window[] when looking for matches towards the end).
     */
    void lm_init(int pack_level, ush* flagbits);

    /* ===========================================================================
     * Free the window and hash table
     */
    void lm_free();

    /* ===========================================================================
     * Set match_start to the longest match starting at the given string and
     * return its length. Matches shorter or equal to prev_length are discarded,
     * in which case the result is equal to prev_length and match_start is
     * garbage.
     * IN assertions: cur_match is the head of the hash chain for the current
     *   string (strstart) and its distance is <= MAX_DIST, and prev_length >= 1
     */

    int longest_match(IPos cur_match);

    void check_match(IPos start, IPos match, int length) {}

    /* ===========================================================================
     * Fill the window when the lookahead becomes insufficient.
     * Updates strstart and lookahead, and sets eofile if end of input file.
     *
     * IN assertion: lookahead < MIN_LOOKAHEAD && strstart + lookahead > 0
     * OUT assertions: strstart <= window_size-MIN_LOOKAHEAD
     *    At least one byte has been read, or eofile is set; file reads are
     *    performed for at least two bytes (required for the translate_eol option).
     */
    void fill_window();

    /* ===========================================================================
     * Processes a new input file and return its compressed length. This
     * function does not perform lazy evaluationof matches and inserts
     * new strings in the dictionary only for unmatched strings or for short
     * matches. It is used only for the fast compression options.
     */
    ullg deflate_fast();

    /* ===========================================================================
     * Same as above, but achieves better compression. We use a lazy
     * evaluation for matches: a match is finally adopted only if there is
     * no better match at the next window position.
     */
    ullg deflate();

    void* UserData;
    FWriteData WriteData;
    FReadData ReadData;
    //bool        encrypted;
    ush Flag;

public:
    CDeflate();

    void Error(int errorID)
    {
        //TRACE_I("throw, errorID = " << errorID);
        throw(errorID);
    }

    int Deflate(ush* internAttr, int* compMethod, int compLevel,
                ush* flag, ullg* compLen, FWriteData writeFunc,
                FReadData readFunc, void* userData);
};
