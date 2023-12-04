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
//#include "zip.rh2"
#include "deflate.h"

/* ===========================================================================
 * Initialize the "longest match" routines for a new file
 *
 * IN assertion: window_size is > 0 if the input file is already read or
 *    mmap'ed in the window[] array, 0 otherwise. In the first case,
 *    window_size is sufficient to contain the whole input file plus
 *    MIN_LOOKAHEAD bytes (to avoid referencing memory beyond the end
 *    of window[] when looking for matches towards the end).
 */
void CDeflate::lm_init(int pack_level, /* 0: store, 1: best speed, 9: best compression */
                       ush* flagbits)  /* general purpose bit flag */
{
    unsigned j;
    int errorID = 0;

    if (pack_level < 1 || pack_level > 9)
        Error(1);

    /* Do not slide the window if the whole input is already in memory
     * (window_size > 0)
     */

    sliding = 1;
    window_size = (ulg)2L * WSIZE;

    /* Use dynamic allocation if compiler does not like big static arrays: */
#ifdef DYN_ALLOC
    if (window == NULL)
    {
        window = (uch far*)zcalloc(WSIZE, 2 * sizeof(uch));
        if (window == NULL)
            ziperr(ZE_MEM, "window allocation");
    }
    if (prev == NULL)
    {
        prev = (Pos far*)zcalloc(WSIZE, sizeof(Pos));
        head = (Pos far*)zcalloc(HASH_SIZE, sizeof(Pos));
        if (prev == NULL || head == NULL)
        {
            ziperr(ZE_MEM, "hash table allocation");
        }
    }
#endif /* DYN_ALLOC */

    /* Initialize the hash table (avoiding 64K overflow for 16 bit systems).
     * prev[] will be initialized on the fly.
     */
    head[HASH_SIZE - 1] = NIL;
    memset((char*)head, NIL, (unsigned)(HASH_SIZE - 1) * sizeof(*head));

    /* Set the default configuration parameters:
     */
    max_lazy_match = configuration_table[pack_level].max_lazy;
    good_match = configuration_table[pack_level].good_length;
#ifndef FULL_SEARCH
    nice_match = configuration_table[pack_level].nice_length;
#endif
    max_chain_length = configuration_table[pack_level].max_chain;
    if (pack_level <= 2)
    {
        *flagbits |= FAST;
    }
    else if (pack_level >= 8)
    {
        *flagbits |= SLOW;
    }
    /* ??? reduce max_chain_length for binary files */

    strstart = 0;
    block_start = 0L;

    j = WSIZE;
#ifndef MAXSEG_64K
    if (sizeof(int) > 2)
        j <<= 1; /* Can read 64K in one step */
#endif
    lookahead = ReadData((char*)window, j, &errorID, UserData);
    if (errorID)
        Error(errorID);

    if (lookahead == 0 || lookahead == (unsigned)EOF)
    {
        eofile = 1, lookahead = 0;
        return;
    }
    eofile = 0;
    /* Make sure that we always have enough lookahead. This is important
     * if input comes from a device such as a tty.
     */
    if (lookahead < MIN_LOOKAHEAD)
        fill_window();

    ins_h = 0;
    for (j = 0; j < MIN_MATCH - 1; j++)
        UPDATE_HASH(ins_h, window[j]);
    /* If lookahead < MIN_MATCH, ins_h is garbage, but this is
     * not important since only literal bytes will be emitted.
     */
}

/* ===========================================================================
 * Free the window and hash table
 */
void CDeflate::lm_free()
{
#ifdef DYN_ALLOC
    if (window != NULL)
    {
        zcfree(window);
        window = NULL;
    }
    if (prev != NULL)
    {
        zcfree(prev);
        zcfree(head);
        prev = head = NULL;
    }
#endif /* DYN_ALLOC */
}

/* ===========================================================================
 * Set match_start to the longest match starting at the given string and
 * return its length. Matches shorter or equal to prev_length are discarded,
 * in which case the result is equal to prev_length and match_start is
 * garbage.
 * IN assertions: cur_match is the head of the hash chain for the current
 *   string (strstart) and its distance is <= MAX_DIST, and prev_length >= 1
 */

/* For 80x86 and 680x0 and ARM, an optimized version is in match.asm or
 * match.S. The code is functionally equivalent, so you can use the C version
 * if desired.
 */
int CDeflate::longest_match(IPos cur_match) /* current match */
{
    unsigned chain_length = max_chain_length; /* max hash chain length */
    uch far* scan = window + strstart;        /* current string */
    uch far* match;                           /* matched string */
    int len;                                  /* length of current match */
    int best_len = prev_length;               /* best match length so far */
    IPos limit = strstart > (IPos)MAX_DIST ? strstart - (IPos)MAX_DIST : NIL;
    /* Stop when cur_match becomes <= limit. To simplify the code,
     * we prevent matches with the string of window index 0.
     */

/* The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
 * It is easy to get rid of this optimization if necessary.
 */
#if HASH_BITS < 8 || MAX_MATCH != 258
error:
    Code too clever
#endif

#ifdef UNALIGNED_OK
        /* Compare two bytes at a time. Note: this is not always beneficial.
     * Try with and without -DUNALIGNED_OK to check.
     */
        register uch far* strend = window + strstart + MAX_MATCH - 1;
    register ush scan_start = *(ush far*)scan;
    register ush scan_end = *(ush far*)(scan + best_len - 1);
#else
    uch far* strend = window + strstart + MAX_MATCH;
    uch scan_end1 = scan[best_len - 1];
    uch scan_end = scan[best_len];
#endif

    /* Do not waste too much time if we already have a good match: */
    if (prev_length >= good_match)
    {
        chain_length >>= 2;
    }
    Assert(strstart <= window_size - MIN_LOOKAHEAD, "insufficient lookahead");

    do
    {
        Assert(cur_match < strstart, "no future");
        match = window + cur_match;

        /* Skip to next match if the match length cannot increase
         * or if the match length is less than 2:
         */
#if (defined(UNALIGNED_OK) && MAX_MATCH == 258)
        /* This code assumes sizeof(unsigned short) == 2. Do not use
         * UNALIGNED_OK if your compiler uses a different size.
         */
        if (*(ush far*)(match + best_len - 1) != scan_end ||
            *(ush far*)match != scan_start)
            continue;

        /* It is not necessary to compare scan[2] and match[2] since they are
         * always equal when the other bytes match, given that the hash keys
         * are equal and that HASH_BITS >= 8. Compare 2 bytes at a time at
         * strstart+3, +5, ... up to strstart+257. We check for insufficient
         * lookahead only every 4th comparison; the 128th check will be made
         * at strstart+257. If MAX_MATCH-2 is not a multiple of 8, it is
         * necessary to put more guard bytes at the end of the window, or
         * to check more often for insufficient lookahead.
         */
        scan++, match++;
        do
        {
        } while (*(ush far*)(scan += 2) == *(ush far*)(match += 2) &&
                 *(ush far*)(scan += 2) == *(ush far*)(match += 2) &&
                 *(ush far*)(scan += 2) == *(ush far*)(match += 2) &&
                 *(ush far*)(scan += 2) == *(ush far*)(match += 2) &&
                 scan < strend);
        /* The funny "do {}" generates better code on most compilers */

        /* Here, scan <= window+strstart+257 */
        Assert(scan <= window + (unsigned)(window_size - 1), "wild scan");
        if (*scan == *match)
            scan++;

        len = (MAX_MATCH - 1) - (int)(strend - scan);
        scan = strend - (MAX_MATCH - 1);

#else /* UNALIGNED_OK */

        if (match[best_len] != scan_end ||
            match[best_len - 1] != scan_end1 ||
            *match != *scan ||
            *++match != scan[1])
            continue;

        /* The check at best_len-1 can be removed because it will be made
         * again later. (This heuristic is not always a win.)
         * It is not necessary to compare scan[2] and match[2] since they
         * are always equal when the other bytes match, given that
         * the hash keys are equal and that HASH_BITS >= 8.
         */
        scan += 2, match++;

        /* We check for insufficient lookahead only every 8th comparison;
         * the 256th check will be made at strstart+258.
         */
        do
        {
        } while (*++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 scan < strend);

        Assert(scan <= window + (unsigned)(window_size - 1), "wild scan");

        len = MAX_MATCH - (int)(strend - scan);
        scan = strend - MAX_MATCH;

#endif /* UNALIGNED_OK */

        if (len > best_len)
        {
            match_start = cur_match;
            best_len = len;
            if (len >= nice_match)
                break;
#ifdef UNALIGNED_OK
            scan_end = *(ush far*)(scan + best_len - 1);
#else
            scan_end1 = scan[best_len - 1];
            scan_end = scan[best_len];
#endif
        }
    } while ((cur_match = prev[cur_match & WMASK]) > limit && --chain_length != 0);

    return best_len;
}

/* ===========================================================================
 * Fill the window when the lookahead becomes insufficient.
 * Updates strstart and lookahead, and sets eofile if end of input file.
 *
 * IN assertion: lookahead < MIN_LOOKAHEAD && strstart + lookahead > 0
 * OUT assertions: strstart <= window_size-MIN_LOOKAHEAD
 *    At least one byte has been read, or eofile is set; file reads are
 *    performed for at least two bytes (required for the translate_eol option).
 */
void CDeflate::fill_window()
{
    unsigned n, m;
    unsigned more; /* Amount of free space at the end of the window. */
    int errorID = 0;

    do
    {
        more = (unsigned)(window_size - (ulg)lookahead - (ulg)strstart);

        /* If the window is almost full and there is insufficient lookahead,
         * move the upper half to the lower one to make room in the upper half.
         */
        if (more == (unsigned)EOF)
        {
            /* Very unlikely, but possible on 16 bit machine if strstart == 0
             * and lookahead == 1 (input done one byte at time)
             */
            more--;

            /* For MMAP or BIG_MEM, the whole input file is already in memory
         * so we must not perform sliding. We must however call file_read in
         * order to compute the crc, update lookahead and possibly set eofile.
         */
        }
        else if (strstart >= WSIZE + MAX_DIST && sliding)
        {

            /* By the IN assertion, the window is not empty so we can't confuse
             * more == 0 with more == 64K on a 16 bit machine.
             */
            memcpy((char*)window, (char*)window + WSIZE, (unsigned)WSIZE);
            match_start -= WSIZE;
            strstart -= WSIZE; /* we now have strstart >= MAX_DIST: */

            block_start -= (long)WSIZE;

            for (n = 0; n < HASH_SIZE; n++)
            {
                m = head[n];
                head[n] = (Pos)(m >= WSIZE ? m - WSIZE : NIL);
            }
            for (n = 0; n < WSIZE; n++)
            {
                m = prev[n];
                prev[n] = (Pos)(m >= WSIZE ? m - WSIZE : NIL);
                /* If n is not on any hash chain, prev[n] is garbage but
                 * its value will never be used.
                 */
            }
            more += WSIZE;
            /*
#ifndef WINDLL
            if (verbose) putc('.', stderr);
#else
            if (verbose) fprintf(stdout,"%c",'.');
#endif
*/
        }
        if (eofile)
            return;

        /* If there was no sliding:
         *    strstart <= WSIZE+MAX_DIST-1 && lookahead <= MIN_LOOKAHEAD - 1 &&
         *    more == window_size - lookahead - strstart
         * => more >= window_size - (MIN_LOOKAHEAD-1 + WSIZE + MAX_DIST-1)
         * => more >= window_size - 2*WSIZE + 2
         * In the BIG_MEM or MMAP case (not yet supported in gzip),
         *   window_size == input_size + MIN_LOOKAHEAD  &&
         *   strstart + lookahead <= input_size => more >= MIN_LOOKAHEAD.
         * Otherwise, window_size == 2*WSIZE so more >= 2.
         * If there was sliding, more >= WSIZE. So in all cases, more >= 2.
         */
        Assert(more >= 2, "more < 2");

        n = ReadData((char*)window + strstart + lookahead, more, &errorID, UserData);
        if (errorID)
            Error(errorID);
        if (n == 0 || n == (unsigned)EOF)
        {
            eofile = 1;
        }
        else
        {
            lookahead += n;
        }
    } while (lookahead < MIN_LOOKAHEAD && !eofile);
}

/* ===========================================================================
 * Processes a new input file and return its compressed length. This
 * function does not perform lazy evaluationof matches and inserts
 * new strings in the dictionary only for unmatched strings or for short
 * matches. It is used only for the fast compression options.
 */
ullg CDeflate::deflate_fast()
{
    IPos hash_head;            /* head of the hash chain */
    int flush;                 /* set if current block must be flushed */
    unsigned match_length = 0; /* length of best match */

    prev_length = MIN_MATCH - 1;
    while (lookahead != 0)
    {
        /* Insert the string window[strstart .. strstart+2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */
        INSERT_STRING(strstart, hash_head);

        /* Find the longest match, discarding those <= prev_length.
         * At this point we have always match_length < MIN_MATCH
         */
        if (hash_head != NIL && strstart - hash_head <= MAX_DIST)
        {
            /* To simplify the code, we prevent matches with the string
             * of window index 0 (in particular we have to avoid a match
             * of the string with itself at the start of the input file).
             */
#ifndef HUFFMAN_ONLY
            match_length = longest_match(hash_head);
#endif
            /* longest_match() sets match_start */
            if (match_length > lookahead)
                match_length = lookahead;
        }
        if (match_length >= MIN_MATCH)
        {
            check_match(strstart, match_start, match_length);

            flush = ct_tally(strstart - match_start, match_length - MIN_MATCH);

            lookahead -= match_length;

            /* Insert new strings in the hash table only if the match length
             * is not too large. This saves time but degrades compression.
             */
            if (match_length <= max_insert_length)
            {
                match_length--; /* string at strstart already in hash table */
                do
                {
                    strstart++;
                    INSERT_STRING(strstart, hash_head);
                    /* strstart never exceeds WSIZE-MAX_MATCH, so there are
                     * always MIN_MATCH bytes ahead. If lookahead < MIN_MATCH
                     * these bytes are garbage, but it does not matter since
                     * the next lookahead bytes will be emitted as literals.
                     */
                } while (--match_length != 0);
                strstart++;
            }
            else
            {
                strstart += match_length;
                match_length = 0;
                ins_h = window[strstart];
                UPDATE_HASH(ins_h, window[strstart + 1]);
#if MIN_MATCH != 3
                Call UPDATE_HASH() MIN_MATCH - 3 more times
#endif
            }
        }
        else
        {
            /* No match, output a literal byte */
            //TRACE_I("No match, output a literal byte " << window[strstart]);
            flush = ct_tally(0, window[strstart]);
            lookahead--;
            strstart++;
        }
        if (flush)
            FLUSH_BLOCK(0), block_start = strstart;

        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need MAX_MATCH bytes
         * for the next match, plus MIN_MATCH bytes to insert the
         * string following the next match.
         */
        if (lookahead < MIN_LOOKAHEAD)
            fill_window();
    }
    return FLUSH_BLOCK(1); /* eof */
}

/* ===========================================================================
 * Same as above, but achieves better compression. We use a lazy
 * evaluation for matches: a match is finally adopted only if there is
 * no better match at the next window position.
 */
ullg CDeflate::deflate()
{
    IPos hash_head;                        /* head of hash chain */
    IPos prev_match;                       /* previous match */
    int flush;                             /* set if current block must be flushed */
    int match_available = 0;               /* set if previous match exists */
    unsigned match_length = MIN_MATCH - 1; /* length of best match */

    if (level <= 3)
        return deflate_fast(); /* optimized for speed */

    /* Process the input block. */
    while (lookahead != 0)
    {
        /* Insert the string window[strstart .. strstart+2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */
        INSERT_STRING(strstart, hash_head);

        /* Find the longest match, discarding those <= prev_length.
         */
        prev_length = match_length, prev_match = match_start;
        match_length = MIN_MATCH - 1;

        if (hash_head != NIL && prev_length < max_lazy_match &&
            strstart - hash_head <= MAX_DIST)
        {
            /* To simplify the code, we prevent matches with the string
             * of window index 0 (in particular we have to avoid a match
             * of the string with itself at the start of the input file).
             */
#ifndef HUFFMAN_ONLY
            match_length = longest_match(hash_head);
#endif
            /* longest_match() sets match_start */
            if (match_length > lookahead)
                match_length = lookahead;

#ifdef FILTERED
            /* Ignore matches of length <= 5 */
            if (match_length <= 5)
            {
#else
            /* Ignore a length 3 match if it is too distant: */
            if (match_length == MIN_MATCH && strstart - match_start > TOO_FAR)
            {
#endif
                /* If prev_match is also MIN_MATCH, match_start is garbage
                 * but we will ignore the current match anyway.
                 */
                match_length = MIN_MATCH - 1;
            }
        }
        /* If there was a match at the previous step and the current
         * match is not better, output the previous match:
         */
        if (prev_length >= MIN_MATCH && match_length <= prev_length)
        {

            check_match(strstart - 1, prev_match, prev_length);

            flush = ct_tally(strstart - 1 - prev_match, prev_length - MIN_MATCH);

            /* Insert in hash table all strings up to the end of the match.
             * strstart-1 and strstart are already inserted.
             */
            lookahead -= prev_length - 1;
            prev_length -= 2;
            do
            {
                strstart++;
                INSERT_STRING(strstart, hash_head);
                /* strstart never exceeds WSIZE-MAX_MATCH, so there are
                 * always MIN_MATCH bytes ahead. If lookahead < MIN_MATCH
                 * these bytes are garbage, but it does not matter since the
                 * next lookahead bytes will always be emitted as literals.
                 */
            } while (--prev_length != 0);
            match_available = 0;
            match_length = MIN_MATCH - 1;
            strstart++;

            if (flush)
                FLUSH_BLOCK(0), block_start = strstart;
        }
        else if (match_available)
        {
            /* If there was no match at the previous position, output a
             * single literal. If there was a match but the current match
             * is longer, truncate the previous match to a single literal.
             */
            //TRACE_I("No match, output a literal byte " << window[strstart - 1]);
            if (ct_tally(0, window[strstart - 1]))
            {
                FLUSH_BLOCK(0), block_start = strstart;
            }
            strstart++;
            lookahead--;
        }
        else
        {
            /* There is no previous match to compare with, wait for
             * the next step to decide.
             */
            match_available = 1;
            strstart++;
            lookahead--;
        }
        Assert(strstart <= isize && lookahead <= isize, "a bit too far");

        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need MAX_MATCH bytes
         * for the next match, plus MIN_MATCH bytes to insert the
         * string following the next match.
         */
        if (lookahead < MIN_LOOKAHEAD)
            fill_window();
    }
    if (match_available)
        ct_tally(0, window[strstart - 1]);

    return FLUSH_BLOCK(1); /* eof */
}
