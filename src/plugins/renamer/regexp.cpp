// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * regcomp and regexec -- regsub and regerror are elsewhere
 *
 *  Copyright (c) 1986 by University of Toronto.
 *  Written by Henry Spencer.  Not derived from licensed software.
 *
 *  Permission is granted to anyone to use this software for any
 *  purpose on any computer system, and to redistribute it freely,
 *  subject to the following restrictions:
 *
 *  1. The author is not responsible for the consequences of use of
 *    this software, no matter how awful, even if they arise
 *    from defects in it.
 *
 *  2. The origin of this software must not be misrepresented, either
 *    by explicit claim or by omission.
 *
 *  3. Altered versions must be plainly marked as such, and must not
 *    be misrepresented as being the original software.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */

#include "precomp.h"

#include <process.h>

#include "regexp.h"

//    - moznost definice svych vlastnich hlasek, jinak staci nakopirovat do kodu

const char* RegExpErrorText(CRegExpErrors err)
{
    CALL_STACK_MESSAGE_NONE
    switch (err)
    {
    case reeNoError:
        return "No error.";
    case reeLowMemory:
        return "Low memory.";
    case reeEmpty:
        return "Regular expression is empty.";
    case reeTooBig:
        return "Regular expression is too big.";
    case reeTooManyParenthesises:
        return "Too many ().";
    case reeUnmatchedParenthesis:
        return "Unmatched ().";
    case reeOperandCouldBeEmpty:
        return "*+ operand could be empty.";
    case reeNested:
        return "Nested *?+.";
    //case reeInvalidRange: return "Invalid [] range.";
    case reeUnmatchedBracket:
        return "Unmatched [].";
    case reeFollowsNothing:
        return "?+* follows nothing.";
    case reeTrailingBackslash:
        return "Trailing \\.";
    case reeInternalDisaster:
        return "Internal disaster.";
    case reeExpectingExtendedPattern1:
        return "Expecting one of :=!< after (?";
    case reeExpectingExtendedPattern2:
        return "Expecting one of =! after (?<";
    case reeInvalidPosixClass:
        return "Unknown POSIX class name.";
    case reeExpectingXDigit:
        return "Expecting two hexadecimal digits after \\x.";
    case reeStackOverflow:
        return "Stack overflow";
    case reeNoPattern:
        return "Compiled expression required.";
    case reeErrorStartingThread:
        return "Unable to start new thread.";
    case reeBadFixedWidthLookBehind:
        return "Look-behind pattern has not fixed width.";
    default:
        return "Unknown error.";
    }
}

//*****************************************************************************
//*****************************************************************************
//
// puvodni regexp.cpp
//
//*****************************************************************************
//*****************************************************************************

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define MAGIC ((char)0234)

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart char that must begin a match; '\0' if none obvious
 * reganch  is the match anchored (at beginning-of-line only)?
 * regmust  string (pointer into program) that match must include, or NULL
 * regmlen  length of regmust string
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that regcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in regexec() needs it and regcomp() is computing
 * it anyway.
 */

/*
 * Structure for regexp "program".  This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.  (Here we
 * have one of the subtle syntax dependencies:  an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:  the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

/* definition number  opnd? meaning */
#define END 0      /* no End of program. */
#define BOL 1      /* no Match "" at beginning of line. */
#define EOL 2      /* no Match "" at end of line. */
#define BOS 3      /* no Match "" at beginning of string. */
#define EOS 4      /* no Match "" at end of string, */
                   /* or before the newline at the end. */
#define VEOS 5     /* no Match "" at the very end of string. */
#define WORDB 6    /* no Match "" at a word boundary. */
#define NOTWORDB 7 /* no Match "" at a non-(word boundary). */
#define ANY 8      /* no Match any one character except newline. */
#define ANYDOT 9   /* no Match any one character including newline. */
#define ANYOF 10   /* bitmap(32B) Match any character in this class. */
//#define ANYBUT  9 /* str  Match any character not in this string. */
#define BRANCH 11   /* node Match this alternative, or the next... */
#define UGBRANCH 12 /* node Match the next or this alternative. */
                    /* it's used by ungreedy variants of ?*+ */
#define BACK 13     /* no Match "", "next" ptr points backward. */
#define EXACTLY 14  /* str  Match this string. */
#define NOTHING 15  /* no Match empty string. */
#define STAR 16     /* node Match this (simple) thing 0 or more times. */
#define PLUS 17     /* node Match this (simple) thing 1 or more times. */
#define UGSTAR 18   /* node Match this (simple) thing 0 or more times. Ungreedy variant. */
#define UGPLUS 19   /* node Match this (simple) thing 1 or more times. Ungreedy variant. */
#define OPEN 20     /* n Mark this point in input as start of #n. */
#define CLOSE 21    /* no Analogous to OPEN. */
#define PLOOKA 22   /* node Positive look-ahead, try to match node. */
#define NLOOKA 23   /* node Negative look-ahead, try to match node. */
#define PLOOKB 24   /* len, node Positive look-behind, try to match node. */
#define NLOOKB 25   /* len, node Negative look-behind, try to match node. */

/*
 * Opcode notes:
 *
 * BRANCH The set of branches constituting a single choice are hooked
 *    together with their "next" pointers, since precedence prevents
 *    anything being concatenated to any individual branch.  The
 *    "next" pointer of the last BRANCH in a choice points to the
 *    thing following the whole choice.  This is also where the
 *    final "next" pointer of each individual branch points; each
 *    branch starts with the operand node of a BRANCH node.
 *
 * BACK   Normal "next" pointers all implicitly point forward; BACK
 *    exists to make loop structures possible.
 *
 * STAR,PLUS  '?', and complex '*' and '+', are implemented as circular
 *    BRANCH structures using BACK.  Simple cases (one character
 *    per match) are implemented with STAR and PLUS for speed
 *    and to minimize recursive plunges.
 *
 * OPEN,CLOSE ...are numbered at compile time.
 */

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 */
#define OP(p) (*(p))
#define NEXT(p) (((*((p) + 1) & 0377) << 8) + (*((p) + 2) & 0377))
#define OPERAND(p) ((p) + 3)

/*
 * Utility definitions.
 */
#ifndef CHARBITS
#define UCHARAT(p) ((int)*(unsigned char*)(p))
#else
#define UCHARAT(p) ((int)*(p)&CHARBITS)
#endif

#define FAIL(e) \
    { \
        State = e; \
        return (NULL); \
    }
#define ISMULT(c) ((c) == '*' || (c) == '+' || (c) == '?')
#define META "^$.[()|?+*\\"

#define INCLASS(b, c) (b[c >> 3] & (1 << (c & 0x07)))

/*
 * Flags to be passed up and down.
 */
#define HASWIDTH 01 /* Known never to match null string. */
#define SIMPLE 02   /* Simple enough to be STAR/PLUS operand. */
#define SPSTART 04  /* Starts with * or +. */
#define WORST 0     /* Worst case. */

// often used POSIX character classes
const char* Word = "word:]";
const char* NonWord = "^word:]";
const char* Space = "space:]";
const char* NonSpace = "^space:]";
const char* Digit = "digit:]";
const char* NonDigit = "^digit:]";

/*
 - regcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.)
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.
 */

BOOL CRegExp::RegComp(char* exp, unsigned options)
{
    CALL_STACK_MESSAGE_NONE
    char* scan;
    char* longest;
    int len;
    int flags;
    int fixedw;

    Compiled = 0;
    State = reeNoError;

    Options = options;
    Caseles = Options & RE_CASELES;

    /* First pass: determine size, legality. */
    RegParse = exp;
    SubExpCount = 1;
    RegSize = 0L;
    RegCode = &RegDummy;
    RegC(MAGIC);
    if (RegExt(0, &flags, &fixedw) == NULL)
    {
        return FALSE;
    }

    /* Small enough for pointer-storage convention? */
    if (RegSize >= 32767L) /* Probably could be 65535L. */
    {
        State = reeTooBig;
        return FALSE;
    }

    /* Allocate space. */
    Program = (char*)realloc(Program, (unsigned)RegSize);
    Startp = (char**)realloc(Startp, sizeof(char*) * SubExpCount * 2);
    if (!Program || !Startp)
    {
        State = reeLowMemory;
        return FALSE;
    }
    Endp = Startp + SubExpCount;

    /* Second pass: emit code. */
    RegParse = exp;
    SubExpCount = 1;
    RegCode = Program;
    RegC(MAGIC);
    if (RegExt(0, &flags, &fixedw) == NULL)
    {
        return FALSE;
    }

    /* Dig out information for optimizations. */
    RegStart = '\0'; /* Worst-case defaults. */
    RegAnch = 0;
    RegMust = NULL;
    RegMLen = 0;
    scan = Program + 1; /* First BRANCH. */
    int op = OP(RegNext(scan));
    if (OP(RegNext(scan)) == END)
    { /* Only one top-level choice. */
        scan = OPERAND(scan);

        /* Starting-point info. */
        switch (OP(scan))
        {
        case EXACTLY:
            RegStart = *OPERAND(scan);
            break;
        case BOS:
            RegAnch++;
            break;
        case STAR:
        case PLUS:
        case UGSTAR:
        case UGPLUS:
            if (OP(OPERAND(scan)) == ANYDOT)
                RegAnch++;
            break;
        }

        /*
     * If there's something expensive in the r.e., find the
     * longest literal string that must appear and make it the
     * regmust.  Resolve ties in favor of later strings, since
     * the regstart check works with the beginning of the r.e.
     * and avoiding duplication strengthens checking.  Not a
     * strong reason, but sufficient in the absence of others.
     */
        if (flags & SPSTART)
        {
            longest = NULL;
            len = 0;
            for (; scan != NULL; scan = RegNext(scan))
                if (OP(scan) == EXACTLY && (int)strlen(OPERAND(scan)) >= len)
                {
                    longest = OPERAND(scan);
                    len = (int)strlen(OPERAND(scan));
                }
            RegMust = longest;
            RegMLen = len;
        }
    }

    State = reeNoError; // success
    Compiled = 1;
    return TRUE;
}

/*
 - RegExt - extended regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
char* CRegExp::RegExt(int paren /* Parenthesized? */, int* flagp, int* fixedwp)
{
    CALL_STACK_MESSAGE_NONE
    char* ret;
    char* reg;
    char* ender;
    char c;

    /* Check for extended pattern. */
    if (paren && *RegParse == '?')
    {
        RegParse++;
        c = *RegParse++;
        switch (c)
        {
        case '<':
        {
            c = *RegParse++;
            if (c == '=')
                ret = RegNode(PLOOKB);
            else if (c == '!')
                ret = RegNode(NLOOKB);
            else
                FAIL(reeExpectingExtendedPattern2);

            /* reserve space for pattern width */
            if (RegCode != &RegDummy)
                RegCode += 2;
            else
                RegSize += 2;
            reg = Reg(paren, 1, flagp, fixedwp);
            if (reg == NULL)
                return NULL;
            ender = RegNode(END);
            RegTail(reg, ender);

            if (*fixedwp < 0 || *fixedwp > 0xFFFF)
                FAIL(reeBadFixedWidthLookBehind);
            if (RegCode != &RegDummy)
            {
                *(unsigned short*)OPERAND(ret) = (unsigned short)*fixedwp;
                // (ret+3)[0] = (char) (*fixedwp & 0xFF);
                // (ret+3)[1] = (char) ((*fixedwp >> 8) & 0xFF);
            }
            *flagp &= ~HASWIDTH;
            *fixedwp = 0;
            break;
        }
        case '=':
            ret = RegNode(PLOOKA);
            goto LOOKA;
        case '!':
            ret = RegNode(NLOOKA);
        LOOKA:
            reg = Reg(paren, 1, flagp, fixedwp);
            if (reg == NULL)
                return NULL;
            ender = RegNode(END);
            RegTail(reg, ender);
            *flagp &= ~HASWIDTH;
            *fixedwp = 0;
            break;
        case ':':
            ret = Reg(paren, 1, flagp, fixedwp);
            break;
        default:
            FAIL(reeExpectingExtendedPattern1);
        }
    }
    else
        ret = Reg(paren, 0, flagp, fixedwp);

    return ret;
}
/*
 - Reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
char* CRegExp::Reg(int paren /* Parenthesized? */,
                   int nocapture /* Not capturing parenthesis*/, int* flagp, int* fixedwp)
{
    CALL_STACK_MESSAGE_NONE
    char* ret;
    char* br;
    char* ender;
    int parno;
    int flags;
    int fixedw;

    *flagp = HASWIDTH; /* Tentatively. */
    *fixedwp = 0;

    /* Make an OPEN node, if parenthesized. */
    if (paren)
    {
        if (nocapture == 0)
        {
            nocapture = 0;
            if (SubExpCount >= MAX_SUBEXP)
                FAIL(reeTooManyParenthesises);
            parno = SubExpCount;
            SubExpCount++;
            ret = RegNode((char)(OPEN));
            RegC(parno);
        }
        else
            ret = NULL;
    }
    else
        ret = NULL;

    /* Pick up the branches, linking them together. */
    br = RegBranch(&flags, fixedwp);
    if (br == NULL)
        return (NULL);
    if (ret != NULL)
        RegTail(ret, br); /* OPEN -> first. */
    else
        ret = br;
    if (!(flags & HASWIDTH))
        *flagp &= ~HASWIDTH;
    *flagp |= flags & SPSTART;
    while (*RegParse == '|')
    {
        RegParse++;
        br = RegBranch(&flags, &fixedw);
        if (br == NULL)
            return (NULL);
        RegTail(ret, br); /* BRANCH -> BRANCH. */
        if (!(flags & HASWIDTH))
            *flagp &= ~HASWIDTH;
        *flagp |= flags & SPSTART;
        if (fixedw != *fixedwp)
            *fixedwp = -1;
    }

    /* Make a closing node, and hook it on the end. */
    if (paren)
    {
        if (nocapture)
            ender = RegNode(NOTHING);
        else
        {
            ender = RegNode(CLOSE);
            RegC(parno);
        }
    }
    else
        ender = RegNode(END);
    RegTail(ret, ender);

    /* Hook the tails of the branches to the closing node. */
    for (br = ret; br != NULL; br = RegNext(br))
        RegOpTail(br, ender);

    /* Check for proper termination. */
    if (paren && *RegParse++ != ')')
    {
        FAIL(reeUnmatchedParenthesis);
    }
    else if (!paren && *RegParse != '\0')
    {
        if (*RegParse == ')')
        {
            FAIL(reeUnmatchedParenthesis);
        }
    }

    return (ret);
}

/*
 - RegBranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
char* CRegExp::RegBranch(int* flagp, int* fixedwp)
{
    CALL_STACK_MESSAGE_NONE
    char* ret;
    char* chain;
    char* latest;
    int flags;
    int fixedw;

    *flagp = WORST; /* Tentatively. */
    *fixedwp = 0;

    ret = RegNode(BRANCH);
    chain = NULL;
    while (*RegParse != '\0' && *RegParse != '|' && *RegParse != ')')
    {
        latest = RegPiece(&flags, &fixedw);
        if (latest == NULL)
            return (NULL);
        *flagp |= flags & HASWIDTH;
        if (fixedw >= 0)
        {
            if (*fixedwp >= 0)
                *fixedwp += fixedw;
        }
        else
            *fixedwp = -1;
        if (chain == NULL) /* First piece. */
            *flagp |= flags & SPSTART;
        else
            RegTail(chain, latest);
        chain = latest;
    }
    if (chain == NULL) /* Loop ran zero times. */
        (void)RegNode(NOTHING);

    return (ret);
}

/*
 - RegPiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
char* CRegExp::RegPiece(int* flagp, int* fixedw)
{
    CALL_STACK_MESSAGE_NONE
    char* ret;
    char op;
    char* next;
    int flags;
    int greedy;

    ret = RegAtom(&flags, fixedw);
    if (ret == NULL)
        return (NULL);

    op = *RegParse;
    if (!ISMULT(op))
    {
        *flagp = flags;
        return (ret);
    }

    if (!(flags & HASWIDTH) && op != '?')
        FAIL(reeOperandCouldBeEmpty);
    *flagp = (op != '+') ? (WORST | SPSTART) : (WORST | HASWIDTH);
    *fixedw = -1;

    if (RegParse[1] == '?')
    {
        greedy = 0;
        RegParse++;
    }
    else
        greedy = 1;

    if (op == '*' && (flags & SIMPLE))
        RegInsert(greedy ? STAR : UGSTAR, ret);
    else if (op == '*')
    {
        /* Emit x* as (x&|), where & means "self". */
        RegInsert(greedy ? BRANCH : UGBRANCH, ret); /* Either x */
        RegOpTail(ret, RegNode(BACK));              /* and loop */
        RegOpTail(ret, ret);                        /* back */
        RegTail(ret, RegNode(BRANCH));              /* or */
        RegTail(ret, RegNode(NOTHING));             /* null. */
    }
    else if (op == '+' && (flags & SIMPLE))
        RegInsert(greedy ? PLUS : UGPLUS, ret);
    else if (op == '+')
    {
        /* Emit x+ as x(&|), where & means "self". */
        next = RegNode(greedy ? BRANCH : UGBRANCH); /* Either */
        RegTail(ret, next);
        RegTail(RegNode(BACK), ret);    /* loop back */
        RegTail(next, RegNode(BRANCH)); /* or */
        RegTail(ret, RegNode(NOTHING)); /* null. */
    }
    else if (op == '?')
    {
        /* Emit x? as (x|) */
        RegInsert(greedy ? BRANCH : UGBRANCH, ret); /* Either x */
        RegTail(ret, RegNode(BRANCH));              /* or */
        next = RegNode(NOTHING);                    /* null. */
        RegTail(ret, next);
        RegOpTail(ret, next);
    }
    RegParse++;
    if (ISMULT(*RegParse))
        FAIL(reeNested);

    return (ret);
}

/*
 - RegAtom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 */
char* CRegExp::RegAtom(int* flagp, int* fixedw)
{
    CALL_STACK_MESSAGE_NONE
    char* ret;
    int flags;

    *flagp = WORST; /* Tentatively. */
    *fixedw = 0;

    switch (*RegParse++)
    {
    case '^':
        ret = RegNode((Options & RE_MULTILINE) ? BOL : BOS);
        break;
    case '$':
        ret = RegNode((Options & RE_MULTILINE) ? EOL : EOS);
        break;
    case '.':
        ret = RegNode((Options & RE_SINGLELINE) ? ANYDOT : ANY);
        *flagp |= HASWIDTH | SIMPLE;
        *fixedw = 1;
        break;
    case '[':
    {
        char _class;
        char classend;
        int comp;
        int lastchar;

        lastchar = -1;
        comp = 0;
        if (*RegParse == '^')
        { /* Complement of range. */
            comp = 1;
            RegParse++;
        }
        ret = RegNode(ANYOF);
        if (RegCode != &RegDummy)
            memset(RegCode, 0, 32);
        if (*RegParse == ':' && IsPosixClass(RegParse + 1))
        {
            if (!RegPosixClass(RegParse + 1))
                return NULL;
            RegParse = strchr(RegParse, ']') + 1;
        }
        else
        {
            if (*RegParse == ']' || *RegParse == '-')
            {
                lastchar = *RegParse;
                RegClass(*RegParse++, RegCode);
            }
            while (*RegParse != '\0' && *RegParse != ']')
            {
                if (*RegParse == '-')
                {
                    RegParse++;
                    if (*RegParse == ']' || *RegParse == '\0' || lastchar == -1)
                        RegClass('-', RegCode);
                    else
                    {
                        if (*RegParse == '\0')
                            FAIL(reeUnmatchedBracket);
                        _class = lastchar + 1;
                        if (*RegParse == '\\')
                        {
                            RegParse++;
                            classend = GetEscapedChar();
                            if (!IsGood())
                                FAIL(State);
                        }
                        else
                            classend = *RegParse + 1;
                        for (; _class != classend; _class++)
                        {
                            RegClass(_class, RegCode);
                            if (Caseles)
                                RegClass(InvertCase(_class), RegCode);
                        }
                        RegParse++;
                        lastchar = -1;
                    }
                }
                else
                {
                    if (*RegParse == '[' && RegParse[1] == ':' && IsPosixClass(RegParse + 2))
                    {
                        if (!RegPosixClass(RegParse + 2))
                            return FALSE;
                        RegParse = strchr(RegParse, ']') + 1;
                        lastchar = -1;
                    }
                    else
                    {
                        if (*RegParse == '\\')
                        {
                            RegParse++;
                            const char* classname;
                            switch (*RegParse)
                            {
                            case 'w':
                                classname = Word;
                                goto LCLASS2;
                            case 'W':
                                classname = NonWord;
                                goto LCLASS2;
                            case 's':
                                classname = Space;
                                goto LCLASS2;
                            case 'S':
                                classname = NonSpace;
                                goto LCLASS2;
                            case 'd':
                                classname = Digit;
                                goto LCLASS2;
                            case 'D':
                            {
                                classname = NonDigit;
                            LCLASS2:
                                RegPosixClass(classname);
                                lastchar = -1;
                                break;
                            }
                            default:
                                lastchar = GetEscapedChar();
                                if (!IsGood())
                                    FAIL(State);
                            }
                        }
                        else
                            lastchar = *RegParse;
                        if (lastchar != -1)
                        {
                            RegClass((char)lastchar, RegCode);
                            if (Caseles)
                                RegClass(InvertCase((char)lastchar), RegCode);
                        }
                        RegParse++;
                    }
                }
            }
            if (*RegParse != ']')
                FAIL(reeUnmatchedBracket);
            RegParse++;
        }
        if (RegCode != &RegDummy)
        {
            if (comp)
            {
                int i;
                for (i = 31; i >= 0; i--)
                    RegCode[i] = ~RegCode[i];
            }
            RegCode += 32;
        }
        else
            RegSize += 32;
        *flagp |= HASWIDTH | SIMPLE;
        *fixedw = 1;
    }
    break;
    case '(':
        ret = RegExt(1, &flags, fixedw);
        if (ret == NULL)
            return (NULL);
        *flagp |= flags & (HASWIDTH | SPSTART);
        break;
    case '\0':
    case '|':
    case ')': //FAIL("internal urp"); /* Supposed to be caught earlier. */
    case '?':
    case '+':
    case '*':
        FAIL(reeFollowsNothing);
    case '\\':
        char c;
        const char* classname;
        if (*RegParse == '\0')
            FAIL(reeTrailingBackslash);
        switch (*RegParse)
        {
        case 'w':
            classname = Word;
            goto LCLASS;
        case 'W':
            classname = NonWord;
            goto LCLASS;
        case 's':
            classname = Space;
            goto LCLASS;
        case 'S':
            classname = NonSpace;
            goto LCLASS;
        case 'd':
            classname = Digit;
            goto LCLASS;
        case 'D':
        {
            classname = NonDigit;
        LCLASS:
            ret = RegNode(ANYOF);
            if (RegCode != &RegDummy)
                memset(RegCode, 0, 32);
            RegPosixClass(classname);
            if (RegCode != &RegDummy)
                RegCode += 32;
            else
                RegSize += 32;
            goto LENDSLASH;
        }

        case 'b':
            c = WORDB;
            goto LASSERT;
        case 'B':
            c = NOTWORDB;
            goto LASSERT;
        case 'A':
            c = BOS;
            goto LASSERT;
        case 'Z':
            c = EOS;
            goto LASSERT;
        case 'z':
        {
            c = VEOS;
        LASSERT:
            RegParse++;
            return RegNode(c);
        }

        default:
            c = GetEscapedChar();
            if (!IsGood())
                FAIL(State);
        }
        ret = RegNode(EXACTLY);
        RegC(Caseles ? LowerCase[c] : c);
        RegC('\0');
    LENDSLASH:
        RegParse++;
        *flagp |= HASWIDTH | SIMPLE;
        *fixedw = 1;
        break;
    default:
    {
        int len;
        char ender;

        RegParse--;
        len = (int)strcspn(RegParse, META);
        if (len <= 0)
            FAIL(reeInternalDisaster);
        ender = *(RegParse + len);
        if (len > 1 && ISMULT(ender))
            len--; /* Back off clear of ?+* operand. */
        *flagp |= HASWIDTH;
        if (len == 1)
            *flagp |= SIMPLE;
        *fixedw = len;
        ret = RegNode(EXACTLY);
        while (len > 0)
        {
            RegC(Caseles ? LowerCase[*RegParse] : *RegParse);
            RegParse++;
            len--;
        }
        RegC('\0');
    }
    break;
    }

    return (ret);
}

/*
 - RegNode - emit a node
 */
char* CRegExp::RegNode(char op) /* Location. */
{
    CALL_STACK_MESSAGE_NONE
    char* ret;
    char* ptr;

    ret = RegCode;
    if (ret == &RegDummy)
    {
        RegSize += 3;
        return (ret);
    }

    ptr = ret;
    *ptr++ = op;
    *ptr++ = '\0'; /* Null "next" pointer. */
    *ptr++ = '\0';
    RegCode = ptr;

    return (ret);
}

/*
 - RegC - emit (if appropriate) a byte of code
 */
void CRegExp::RegC(char b)
{
    CALL_STACK_MESSAGE_NONE
    if (RegCode != &RegDummy)
        *RegCode++ = b;
    else
        RegSize++;
}

/*
 - RegInsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 */
void CRegExp::RegInsert(char op, char* opnd)
{
    CALL_STACK_MESSAGE_NONE
    char* src;
    char* dst;
    char* place;

    if (RegCode == &RegDummy)
    {
        RegSize += 3;
        return;
    }

    src = RegCode;
    RegCode += 3;
    dst = RegCode;
    while (src > opnd)
        *--dst = *--src;

    place = opnd; /* Op node, where operand used to be. */
    *place++ = op;
    *place++ = '\0';
    *place++ = '\0';
}

/*
 - RegTail - set the next-pointer at the end of a node chain
 */
void CRegExp::RegTail(char* p, char* val)
{
    CALL_STACK_MESSAGE_NONE
    char* scan;
    char* temp;
    int offset;

    if (p == &RegDummy)
        return;

    /* Find last node. */
    scan = p;
    for (;;)
    {
        temp = RegNext(scan);
        if (temp == NULL)
            break;
        scan = temp;
    }

    if (OP(scan) == BACK)
        offset = (int)(scan - val);
    else
        offset = (int)(val - scan);
    *(scan + 1) = (char)((offset >> 8) & 0377);
    *(scan + 2) = (char)(offset & 0377);
}

/*
 - RegOpTail - RegTail on operand of first argument; nop if operandless
 */
void CRegExp::RegOpTail(char* p, char* val)
{
    CALL_STACK_MESSAGE_NONE
    /* "Operandless" and "op != BRANCH" are synonymous in practice. */
    if (p == NULL || p == &RegDummy || OP(p) != BRANCH && OP(p) != UGBRANCH)
        return;
    RegTail(OPERAND(p), val);
}

int CRegExp::IsPosixClass(char* scan)
{
    CALL_STACK_MESSAGE_NONE
    char* iterator;
    if (*scan == '^')
        scan++;
    iterator = scan;
    while (IsAlpha(*iterator++))
        ;
    if (iterator > scan && iterator[-1] == ':' && iterator[0] == ']')
        return 1;
    return 0;
}

struct CPosixClass
{
    const char* Name;
    unsigned Type;
    int ID;
};

#define PC_ASCII 1
#define PC_WORD 2

CPosixClass PosixClasses[] =
    {
        {"alnum:]", C1_ALPHA | C1_DIGIT, 0},
        {"alpha:]", C1_ALPHA, 0},
        {"ascii:]", 0, PC_ASCII},
        {"cntrl:]", C1_CNTRL, 0},
        {Digit, C1_DIGIT, 0},
        {"graph:]", C1_PUNCT | C1_ALPHA | C1_DIGIT, 0},
        {"lower:]", C1_LOWER, 0},
        {"print:]", C1_PUNCT | C1_ALPHA | C1_DIGIT | C1_BLANK, 0},
        {"punct:]", C1_PUNCT, 0},
        {Space, C1_SPACE, 0},
        {"upper:]", C1_UPPER, 0},
        {Word, C1_ALPHA | C1_DIGIT, PC_WORD},
        {"xdigit:]", C1_XDIGIT, 0},
        {NULL, C1_XDIGIT, 0}};

int CRegExp::RegPosixClass(const char* scan)
{
    CALL_STACK_MESSAGE_NONE
    char tmpclass[32];
    int comp = 0;

    if (*scan == '^')
    {
        scan++;
        comp = 1;
    }

    CPosixClass* cls = PosixClasses;
    while (cls->Name != NULL && strncmp(scan, cls->Name, strlen(cls->Name)) != 0)
        cls++;
    if (cls->Name == NULL)
    {
        State = reeInvalidPosixClass;
        return 0;
    }

    memset(tmpclass, 0, 32);
    int c;
    for (c = 0; c < 256; c++)
        if (CType[c] & cls->Type)
        {
            RegClass((char)c, tmpclass);
            if (Caseles)
                RegClass(InvertCase(c), tmpclass);
        }

    switch (cls->ID)
    {
    case PC_ASCII:
    {
        int i;
        for (i = 0; i < 16; i++)
            tmpclass[i] = 0xFF;
        break;
    }
    case PC_WORD:
    {
        RegClass('_', tmpclass);
        break;
    }
    }

    if (comp)
    {
        int i;
        for (i = 31; i >= 0; i--)
            tmpclass[i] = ~tmpclass[i];
    }

    if (RegCode != &RegDummy)
    {
        int i;
        for (i = 31; i >= 0; i--)
            RegCode[i] |= tmpclass[i];
    }
    return 1;
}

inline int
HexToNumber(char c)
{
    CALL_STACK_MESSAGE_NONE
    return IsDigit(c) ? c - '0' : LowerCase[c] - 'a' + 10;
}

char CRegExp::GetEscapedChar()
{
    CALL_STACK_MESSAGE_NONE
    switch (*RegParse)
    {
    case 'b':
        return '\b';
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 'f':
        return '\f';
    case 'a':
        return '\a';
    case 'x':
    {
        char ret;
        RegParse++;
        if (!IsXdigit(*RegParse) || !IsXdigit(RegParse[1]))
        {
            State = reeExpectingXDigit;
            return 0;
        }
        ret = HexToNumber(*RegParse) << 4;
        RegParse++;
        ret += HexToNumber(*RegParse);
        return ret;
    }
    }
    return *RegParse;
}

/*
 * regexec and friends
 */

/*
 - regexec - match a regexp against a string
 */

struct CRegExecData
{
    CRegExp* RegExp;
    char* String;
    int Length;
    int Offset;
};

BOOL CRegExp::RegExec(char* string, int length, int offset, BOOL separateThread)
{
    CALL_STACK_MESSAGE_NONE
    State = reeNoError;

    if (!Compiled)
    {
        State = reeNoPattern;
        return FALSE;
    }

    if (offset >= length)
        return FALSE;

    if (separateThread)
    {
        DWORD i;
        CRegExecData data;
        data.RegExp = this;
        data.String = string;
        data.Length = length;
        data.Offset = offset;
        HANDLE thread = CreateThread(NULL, 0, RegExecThread, &data, 0, &i);
        if (thread == 0)
        {
            State = reeErrorStartingThread;
            return FALSE;
        }
        WaitForSingleObject(thread, INFINITE);
        GetExitCodeThread(thread, &i);
        CloseHandle(thread);
        return i;
    }

    __try
    {
        char* s;

        /* Check validity of program. */
        if (UCHARAT(Program) != MAGIC)
        {
            State = reeInternalDisaster;
            return FALSE;
        }

        /* Mark beginning of line for ^ . */
        RegBol = string;
        // ulozime si ukazatel za posledni znak radky
        RegEol = string + length;

        /* If there is a "must appear" string, look for it. */
        if (RegMust != NULL)
        {
            s = string + offset;
            while (s + RegMLen <= RegEol &&
                   (s = (Caseles ? MemChrI(s, RegMust[0], (int)(RegEol - s)) : (char*)memchr(s, RegMust[0], RegEol - s))) != NULL)
            {
                if ((Caseles ? MemCmpI(s, RegMust, RegMLen) : memcmp(s, RegMust, RegMLen)) == 0)
                    break; /* Found it. */
                s++;
            }
            if (s == NULL || s + RegMLen > RegEol) /* Not present. */
            {
                return FALSE;
            }
        }

        /* Simplest case:  anchored match need be tried only once. */
        if (RegAnch)
        {
            return (RegTry(string + offset) > 0);
        }

        /* Messy cases:  unanchored match. */
        s = string + offset;
        if (RegStart != '\0')
            /* We know what char it must start with. */
            while ((s = (Caseles ? MemChrI(s, RegStart, (int)(RegEol - s)) : (char*)memchr(s, RegStart, (int)(RegEol - s)))) != NULL)
            {
                if (RegTry(s))
                {
                    return TRUE;
                }
                s++;
            }
        else
            /* We don't -- general case. */
            do
            {
                if (RegTry(s))
                {
                    return TRUE;
                }
            } while (++s < RegEol);

        /* Failure. */
        return FALSE;
    }
    __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW)
    {
        State = reeStackOverflow;
        return FALSE;
    }
}

DWORD WINAPI
CRegExp::RegExecThread(void* data)
{
    CALL_STACK_MESSAGE_NONE
    CRegExecData* d = (CRegExecData*)data;
    return d->RegExp->RegExec(d->String, d->Length, d->Offset, FALSE);
}

/*
 - regtry - try match at specific point
 */
int CRegExp::RegTry(char* string) /* 0 failure, 1 success */
{
    CALL_STACK_MESSAGE_NONE
    int i;
    char** sp;
    char** ep;

    RegInput = string;
    RegStartp = Startp;
    RegEndp = Endp;

    sp = Startp;
    ep = Endp;
    for (i = SubExpCount; i > 0; i--)
    {
        *sp++ = NULL;
        *ep++ = NULL;
    }
    if (RegMatch(Program + 1))
    {
        Startp[0] = string;
        Endp[0] = RegInput;
        return (1);
    }
    else
        return (0);
}

/*
 - RegMatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
int CRegExp::RegMatch(char* prog) /* 0 failure, 1 success */
{
    CALL_STACK_MESSAGE_NONE
    char* scan; /* Current node. */
    char* next; /* Next node. */

    scan = prog;
    while (scan != NULL)
    {
        next = RegNext(scan);

        switch (OP(scan))
        {
        case BOL:
            if (RegInput != RegBol)
            {
                if ((Options & RE_LF) && RegInput[-1] == '\n')
                    break;
                if ((Options & RE_CR) && RegInput[-1] == '\r')
                    break;
                if ((Options & RE_NULL) && RegInput[-1] == '\0')
                    break;
                if ((Options & RE_CRLF) && RegInput > RegBol + 1 &&
                    RegInput[-1] == '\n' && RegInput[-2] == '\r')
                    break;
                return 0;
            }
            break;
        case EOL:
            if (!RegMatchEol(RegInput))
                return 0;
            break;
        case BOS:
            if (RegInput != RegBol)
                return (0);
            break;
        case EOS:
            if (RegInput != RegEol)
            {
                if (RegInput + 1 == RegBol)
                {
                    if ((Options & RE_LF) && *RegInput == '\n')
                        break;
                    if ((Options & RE_CR) && *RegInput == '\r')
                        break;
                    if ((Options & RE_NULL) && *RegInput == '\0')
                        break;
                }
                if (RegInput + 2 == RegBol)
                {
                    if ((Options & RE_CRLF) && RegInput + 1 < RegEol &&
                        RegInput[0] == '\r' && RegInput[1] == '\n')
                        break;
                }
                return (0);
            }
            break;
        case VEOS:
            if (RegInput != RegEol)
                return (0);
            break;
        case WORDB:
        {
            int prev = RegInput > RegBol && IsWord(RegInput[-1]) > 0;
            int cur = RegInput < RegEol && IsWord(*RegInput) > 0;
            if (prev == cur)
                return 0;
            break;
        }
        case NOTWORDB:
        {
            int prev = RegInput > RegBol && IsWord(RegInput[-1]) > 0;
            int cur = RegInput < RegEol && IsWord(*RegInput) > 0;
            if (prev != cur)
                return 0;
            break;
        }
        case ANY:
            if (RegMatchEol(RegInput))
                return (0);
            RegInput++;
            break;
        case ANYDOT:
            if (RegInput == RegEol)
                return (0);
            RegInput++;
            break;
        case EXACTLY:
        {
            int len;
            char* opnd;

            opnd = OPERAND(scan);
            /* Inline the first character, for speed. */
            if (RegInput == RegEol || *opnd != (Caseles ? LowerCase[*RegInput] : *RegInput))
                return (0);
            len = (int)strlen(opnd);
            if (len > RegEol - RegInput || len > 1 &&
                                               (Caseles ? MemCmpI(RegInput, opnd, len) : memcmp(RegInput, opnd, len)) != 0)
                return (0);
            RegInput += len;
        }
        break;
        case ANYOF:
            if (RegInput == RegEol || !INCLASS(OPERAND(scan), *RegInput))
                return (0);
            RegInput++;
            break;
        case NOTHING:
            break;
        case BACK:
            break;
        case OPEN:
        {
            int no;
            char* save;

            no = *OPERAND(scan);
            save = RegInput;

            if (RegMatch(next))
            {
                /*
             * Don't set startp if some later
             * invocation of the same parentheses
             * already has.
             */
                if (RegStartp[no] == NULL)
                    RegStartp[no] = save;
                return (1);
            }
            else
                return (0);
        }
        case CLOSE:
        {
            int no;
            char* save;

            no = *OPERAND(scan);
            save = RegInput;

            if (RegMatch(next))
            {
                /*
             * Don't set endp if some later
             * invocation of the same parentheses
             * already has.
             */
                if (RegEndp[no] == NULL)
                    RegEndp[no] = save;
                return (1);
            }
            else
                return (0);
        }
        case BRANCH:
        {
            char* save;

            if (OP(next) != BRANCH)   /* No choice. */
                next = OPERAND(scan); /* Avoid recursion. */
            else
            {
                do
                {
                    save = RegInput;
                    if (RegMatch(OPERAND(scan)))
                        return (1);
                    RegInput = save;
                    scan = RegNext(scan);
                } while (scan != NULL && OP(scan) == BRANCH);
                return (0);
                /* NOTREACHED */
            }
        }
        break;
        case UGBRANCH:
        {
            char* save;

            save = RegInput;
            if (RegMatch(next)) /* Try next */
                return (1);
            RegInput = save; /* or an alternative */
            next = OPERAND(scan);
        }
        break;
        case STAR:
        case PLUS:
        {
            int nextch;
            int no;
            char* save;
            int min;

            /*
           * Lookahead to avoid useless match attempts
           * when we know what character comes next.
           */
            nextch = -1;
            if (OP(next) == EXACTLY)
                nextch = *OPERAND(next);
            min = (OP(scan) == STAR) ? 0 : 1;
            save = RegInput;
            no = RegRepeat(OPERAND(scan));
            while (no >= min)
            {
                /* If it could work, try it. */
                if (nextch == -1 ||
                    (Caseles ? LowerCase[*RegInput] : *RegInput) == (char)nextch)
                    if (RegMatch(next))
                        return (1);
                /* Couldn't or didn't -- back up. */
                no--;
                RegInput = save + no;
            }
            return (0);
        }

        case UGSTAR:
        case UGPLUS:
        {
            int nextch;
            char* save;
            char op;
            char* opnd;

            nextch = -1;
            if (OP(next) == EXACTLY)
                nextch = *OPERAND(next);
            opnd = OPERAND(OPERAND(scan));
            op = OP(OPERAND(scan));

            if (OP(scan) == UGPLUS)
            {
                if (RegMatchSimple(op, opnd))
                    RegInput++;
                else
                    return 0;
            }
            do
            {
                save = RegInput;
                /* If it could work, try it. */
                if (nextch == -1 ||
                    (Caseles ? LowerCase[*RegInput] : *RegInput) == (char)nextch)
                    if (RegMatch(next))
                        return 1;
                RegInput = save;
                if (RegMatchSimple(op, opnd))
                    RegInput++; // eat one character more
                else
                    break;
            } while (1);

            return 0; // not matched
        }

        case PLOOKA:
        {
            char* save;

            save = RegInput;
            if (!RegMatch(OPERAND(scan)))
                return (0);
            RegInput = save;
        }
        break;

        case NLOOKA:
        {
            char* save;

            save = RegInput;
            if (RegMatch(OPERAND(scan)))
                return (0);
            RegInput = save;
        }
        break;

        case PLOOKB:
        {
            char* save;

            save = RegInput;
            RegInput -= *(unsigned short*)(OPERAND(scan));
            if (RegInput < RegBol || !RegMatch(OPERAND(scan) + 2))
                return (0);
            RegInput = save;
        }
        break;

        case NLOOKB:
        {
            char* save;

            save = RegInput;
            RegInput -= *(unsigned short*)(OPERAND(scan));
            if (RegInput >= RegBol && RegMatch(OPERAND(scan) + 2))
                return (0);
            RegInput = save;
        }
        break;

        case END:
            return (1); /* Success! */
        default:
            return (0); /* memory corruption */
        }

        scan = next;
    }

    /*
   * We get here only if there's trouble -- normally "case END" is
   * the terminating point.
   */
    //  regerror("corrupted pointers");
    return (0);
}

/*
 - RegRepeat - repeatedly match something simple, report how many
 */
int CRegExp::RegRepeat(char* p)
{
    CALL_STACK_MESSAGE_NONE
    int count = 0;
    char* scan;
    char* opnd;

    scan = RegInput;
    opnd = OPERAND(p);
    switch (OP(p))
    {
    case ANY:
        while (!RegMatchEol(scan))
        {
            scan++;
        }
        break;
    case ANYDOT:
        scan = RegEol;
        break;
    case EXACTLY:
        if (Caseles)
            while (scan < RegEol && *opnd == LowerCase[*scan])
            {
                scan++;
            }
        else
            while (scan < RegEol && *opnd == *scan)
            {
                scan++;
            }
        break;
    case ANYOF:
        while (scan < RegEol && INCLASS(opnd, *scan))
        {
            scan++;
        }
        break;
    default:       /* Oh dear.  Called inappropriately. */
                   //    regerror("internal foulup");
        count = 0; /* Best compromise. */
        break;
    }
    count = (int)(scan - RegInput);
    RegInput = scan;

    return (count);
}

/*
 - RegMatchSimple - match something simple
 */
int CRegExp::RegMatchSimple(char op, char* opnd)
{
    CALL_STACK_MESSAGE_NONE
    if (RegInput >= RegEol)
        return 0;
    switch (op)
    {
    case ANY:
        return !RegMatchEol(RegInput);
    case ANYDOT:
        return 1;
    case EXACTLY:
        return *opnd == (Caseles ? LowerCase[*RegInput] : *RegInput);
    case ANYOF:
        return INCLASS(opnd, *RegInput);
    }
    // Oh dear.  Called inappropriately.
    return 0; // Best Compromise
}

/*
 - RegNext - dig the "next" pointer out of a node
 */
char* CRegExp::RegNext(char* p)
{
    CALL_STACK_MESSAGE_NONE
    int offset;

    if (p == &RegDummy)
        return (NULL);

    offset = NEXT(p);
    if (offset == 0)
        return (NULL);

    if (OP(p) == BACK)
        return (p - offset);
    else
        return (p + offset);
}

//*****************************************************************************

// Creates CRegExp object. Returns NULL in case of low memory
CRegExpAbstract* CreateRegExp()
{
    CALL_STACK_MESSAGE_NONE
    return new CRegExp;
}

// Release CRegExp object.
void ReleaseRegExp(CRegExpAbstract* regExp)
{
    CALL_STACK_MESSAGE_NONE
    delete regExp;
}
