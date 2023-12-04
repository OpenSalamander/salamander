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

#include <windows.h>
#include <crtdbg.h>
#include <stdio.h>
#include <string.h>
#include <ostream>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

//#include "trace.h" aby to slo pripojit i k pluginum, stejne tu zatim zadny TRACE neni
#include "str.h"
#include "regexp.h"

//*****************************************************************************
//*****************************************************************************
//
// moje cast regexp.cpp
//
//*****************************************************************************
//*****************************************************************************

class C__RegExpSection
{
public:
    CRITICAL_SECTION CriticalSection;

    C__RegExpSection() { InitializeCriticalSection(&CriticalSection); }
    ~C__RegExpSection() { DeleteCriticalSection(&CriticalSection); }

    void Enter() { EnterCriticalSection(&CriticalSection); }
    void Leave() { LeaveCriticalSection(&CriticalSection); }
};

C__RegExpSection __RegExpSection;

/*    - moznost definice svych vlastnich hlasek, jinak staci nakopirovat do kodu

const char *RegExpErrorText(CRegExpErrors err)
{
  switch (err)
  {
    case reeNoError: return "No error.";
    case reeLowMemory: return "Low memory.";
    case reeEmpty: return "Regular expression is empty.";
    case reeTooBig: return "Regular expression is too big.";
    case reeTooManyParenthesises: return "Too many ().";
    case reeUnmatchedParenthesis: return "Unmatched ().";
    case reeOperandCouldBeEmpty: return "*+ operand could be empty.";
    case reeNested: return "Nested *?+.";
    case reeInvalidRange: return "Invalid [] range.";
    case reeUnmatchedBracket: return "Unmatched [].";
    case reeFollowsNothing: return "?+* follows nothing.";
    case reeTrailingBackslash: return "Trailing \\.";
    case reeInternalDisaster: return "Internal disaster.";
    default: return "Unknown error.";
  }
}
*/
const char* CRegularExpression::LastError = NULL;

void regerror(const char* s)
{
    CRegularExpression::LastError = s;
}

//*****************************************************************************
//
// CRegularExpression
//

BOOL CRegularExpression::Set(const char* pattern, WORD flags)
{
    if (OriginalPattern != NULL)
        free(OriginalPattern);
    if (pattern == NULL)
    {
        LastError = LastErrorText = RegExpErrorText(reeEmpty);
        OriginalPattern = NULL;
        return FALSE;
    }
    int length = (int)strlen(pattern);
    OriginalPattern = (char*)malloc(length + 1);
    if (OriginalPattern != NULL)
        memcpy(OriginalPattern, pattern, length + 1);
    else
    {
        LastError = LastErrorText = RegExpErrorText(reeLowMemory);
        return FALSE;
    }

    return SetFlags(flags);
}

BOOL CRegularExpression::SetFlags(WORD flags)
{
    Flags = flags;
    char* pattern;
    if ((Flags & sfCaseSensitive) == 0)
    {
        pattern = (char*)malloc(strlen(OriginalPattern) + 1);
        if (pattern != NULL)
        {
            char* s1 = pattern;
            char* s2 = OriginalPattern;
            while (*s2 != 0)
                *s1++ = LowerCase[*s2++];
            *s1 = 0;
        }
        else
        {
            LastError = LastErrorText = RegExpErrorText(reeLowMemory);
            return FALSE;
        }
    }
    else
    {
        if (OriginalPattern == NULL)
        {
            LastError = LastErrorText = RegExpErrorText(reeEmpty);
            return FALSE;
        }
        pattern = OriginalPattern;
    }

    if (Expression != NULL)
        free(Expression);
    Expression = regcomp(pattern, LastErrorText);

    if (Expression != NULL && (Flags & sfForward) == 0)
    { // vyraz je syntakticky o.k. + backward search
        if (Expression != NULL)
            free(Expression);
        Expression = NULL;
        int len = (int)strlen(pattern);
        char* backwardPat = (char*)malloc(len + 1);
        if (backwardPat != NULL)
        {
            char* end = backwardPat + len;
            *end = 0;
            ReverseRegExp(end, pattern, pattern + len);
            Expression = regcomp(backwardPat, LastErrorText);
            free(backwardPat);
        }
        else
            LastError = LastErrorText = RegExpErrorText(reeLowMemory);
    }

    if ((Flags & sfCaseSensitive) == 0)
        free(pattern);
    return Expression != NULL && LastErrorText == NULL;
}

BOOL CRegularExpression::SetLine(const char* start, const char* end)
{
    if (Allocated < (end - start) + 1)
    {
        char* newLine = (char*)realloc(Line, (end - start) + 1);
        if (newLine != NULL)
        {
            Line = newLine;
            Allocated = (int)(end - start) + 1;
        }
        else
        {
            LastError = LastErrorText = RegExpErrorText(reeLowMemory);
            return FALSE;
        }
    }

    OrigLineStart = start;
    LineLength = (int)(end - start);
    if (Flags & sfForward)
    {
        if (Flags & sfCaseSensitive)
        {
            memcpy(Line, start, LineLength);
            Line[LineLength] = 0;
        }
        else // insensitive
        {
            char* l = Line;
            while (start < end)
                *l++ = LowerCase[*start++];
            *l = 0;
        }
    }
    else // backward
    {
        if (Flags & sfCaseSensitive)
        {
            char* l = Line;
            while (start < end)
                *l++ = *--end;
            *l = 0;
        }
        else // insensitive
        {
            char* l = Line;
            while (start < end)
                *l++ = LowerCase[*--end];
            *l = 0;
        }
    }
    LastErrorText = NULL;
    return TRUE;
}

int CRegularExpression::SearchForward(int start, int& foundLen)
{
    if (start <= LineLength && regexec(Expression, Line, start) == 1)
    {
        foundLen = (int)(Expression->endp[0] - Expression->startp[0]);
        return (int)(Expression->startp[0] - Line);
    }
    else
        return -1;
}

int CRegularExpression::SearchBackward(int length, int& foundLen)
{
    if (length >= 0 && regexec(Expression, Line, LineLength - length) == 1)
    {
        foundLen = (int)(Expression->endp[0] - Expression->startp[0]);
        return (int)(LineLength - (Expression->endp[0] - Line));
    }
    else
        return -1;
}

BOOL CRegularExpression::ExpandVariables(char* pattern, char* buffer, int bufSize, int* count)
{
    char* sour = pattern;
    char* dest = buffer;
    bufSize--; //rezervujeme si misto pro NULL
    while (*sour)
    {
        if (!bufSize)
            return FALSE; //dosel nam buffer
        if (*sour == '\\')
        {
            sour++;
            if (!*sour)
                break; // tady bych asi mnel hodit chybu takovato sekvence neni definovana
            if (*sour >= '1' && *sour <= '9')
            {
                int n = *sour - '0';
                int len = (int)(Expression->endp[n] - Expression->startp[n]);
                if (len)
                {
                    int i = len > bufSize ? i = bufSize : i = len;
                    memcpy(dest, OrigLineStart + (Expression->startp[n] - Line), i);
                    dest += i;
                    if (len > bufSize)
                    {
                        *dest = 0;
                        *count = (int)(dest - buffer);
                        return FALSE;
                    }
                }
                sour++;
                continue;
            }
        }
        *dest++ = *sour++;
        bufSize--;
    }
    *dest = 0;
    *count = (int)(dest - buffer);
    return TRUE;
}

int CRegularExpression::ReplaceForward(int start, char* pattern, BOOL global,
                                       char* buffer, int bufSize)
{
    BOOL ret = FALSE;
    char* output = buffer;
    int len;
    while (start <= LineLength && regexec(Expression, Line, start) == 1 &&
           Expression->endp[0] - Expression->startp[0] > 0 /*zero sized match neberem*/)
    {
        //zkopirujeme nezmeny text, ktery predchazi match
        len = (int)((Expression->startp[0] - Line) - start);
        if (len + 1 > bufSize)
        {
            return FALSE;
        }
        memcpy(output, OrigLineStart + start, len);
        output += len;
        bufSize -= len;
        //nahradime co jsme nasli
        if (!ExpandVariables(pattern, output, bufSize, &len))
        {
            return FALSE;
        }
        output += len;
        bufSize -= len;
        start = (int)(Expression->endp[0] - Line);
        ret = TRUE;
        if (!global)
            break;
    }

    if (ret && start < LineLength)
    {
        //dokopirujeme text nasledujici match
        if (LineLength - start + 1 > bufSize)
        {
            return FALSE;
        }
        memcpy(output, OrigLineStart + start, LineLength - start + 1);
    }
    return ret;
}

void CRegularExpression::ReverseRegExp(char*& dstExpEnd, char* srcExp, char* srcExpEnd)
{
    char* s = srcExp;

    while (s < srcExpEnd)
    {
        //---  hledani konce atomu - pro zopakovani '*', '+' a '?'
        char* ss;    // ukazuje za atom
        BOOL addPar; // paruji zavorky? (ma se pridat zavorka do paru)
        switch (*s)
        {
        case '\\':
            ss = (*(s + 1) != 0) ? (s + 2) : (s + 1);
            break;

        case '(':
        case '[':
        {
            int parNum = (*s == '(') ? 1 : 0; // pocty zavorek
            int braNum = (*s == '[') ? 1 : 0; // pocty zavorek
            char* lastBra = (*s == '[') ? s : NULL;
            ss = s + 1;
            while (*ss != 0 && (parNum != 0 || braNum != 0))
            {
                switch (*ss++)
                {
                case '(':
                    if (braNum == 0)
                        parNum++;
                    break; // [..(..] je povoleno
                case '[':
                {
                    if (braNum == 0) // [..[..] je povoleno
                    {
                        braNum++;
                        lastBra = ss - 1;
                    }
                    break;
                }

                case ')':
                    if (braNum == 0 && parNum > 0)
                        parNum--;
                    break; // [..)..] je povoleno
                case ']':
                {
                    if (braNum != 0) // ..].. je povoleno
                    {
                        if (ss - 2 != lastBra &&                     // []..] je povoleno
                            (ss - 3 != lastBra || *(ss - 2) != '^')) // [^]..] je take povoleno
                        {
                            braNum--;
                        }
                    }
                    break;
                }

                case '\\':
                {
                    if (*ss != 0)
                        ss++;
                    break; // znak za '\\' nemuze byt brany jako zavorka
                }
                }
            }
            addPar = (parNum == 0 && braNum == 0);
            break;
        }

            //      case '|':
            //      case '.':
            //      case '^':
            //      case '$':
        default:
            ss = s + 1;
            break;
        }

        //---  nakopirovani vsech '*', '+' a '?' obsazenych za atomem
        char* oldSS = ss;
        while (*ss == '*' || *ss == '?' || *ss == '+')
            *--dstExpEnd = *ss++;

        //--- nakopirovani obraceneho atomu
        switch (*s)
        {
        case '\\':
        {
            if (*(s + 1) != 0)
                *--dstExpEnd = *(s + 1);
            *--dstExpEnd = '\\';
            break;
        }

        case '(':
        case '[':
        {
            if (!addPar)
                *--dstExpEnd = *s;
            else
                *--dstExpEnd = (*s == '(') ? ')' : ']';

            if (oldSS - s >= 2) // pokud vyraz nekonci otevrenou zavorkou
            {
                if (*s == '(')
                { // kopie reversovaneho vyrazu - ohraniceni
                    ReverseRegExp(dstExpEnd, s + 1, oldSS - 1);
                }
                else // prosta kopie vnitrku - mnozina
                {
                    dstExpEnd -= (oldSS - 1) - (s + 1);
                    memcpy(dstExpEnd, s + 1, (oldSS - 1) - (s + 1));
                }
                if (addPar)
                    *--dstExpEnd = *s;
            }
            break;
        }

        case '^':
            *--dstExpEnd = '$';
            break;
        case '$':
            *--dstExpEnd = '^';
            break;
            //      case '|':
            //      case '.':
        default:
            *--dstExpEnd = *s;
            break;
        }

        //---  prechod na dalsi atom
        s = ss;
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
#define END 0     /* no End of program. */
#define BOL 1     /* no Match "" at beginning of line. */
#define EOL 2     /* no Match "" at end of line. */
#define ANY 3     /* no Match any one character. */
#define ANYOF 4   /* str  Match any character in this string. */
#define ANYBUT 5  /* str  Match any character not in this string. */
#define BRANCH 6  /* node Match this alternative, or the next... */
#define BACK 7    /* no Match "", "next" ptr points backward. */
#define EXACTLY 8 /* str  Match this string. */
#define NOTHING 9 /* no Match empty string. */
#define STAR 10   /* node Match this (simple) thing 0 or more times. */
#define PLUS 11   /* node Match this (simple) thing 1 or more times. */
#define OPEN 20   /* no Mark this point in input as start of #n. */
                  /*  OPEN+1 is number 1, etc. */
#define CLOSE 30  /* no Analogous to OPEN. */

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
 * See regmagic.h for one further detail of program structure.
 */

/*
 * Utility definitions.
 */
#ifndef CHARBITS
#define UCHARAT(p) ((int)*(unsigned char*)(p))
#else
#define UCHARAT(p) ((int)*(p)&CHARBITS)
#endif

#define FAIL(m) \
    { \
        regerror(m); \
        return (NULL); \
    }
#define ISMULT(c) ((c) == '*' || (c) == '+' || (c) == '?')
#define META "^$.[()|?+*\\"

/*
 * Flags to be passed up and down.
 */
#define HASWIDTH 01 /* Known never to match null string. */
#define SIMPLE 02   /* Simple enough to be STAR/PLUS operand. */
#define SPSTART 04  /* Starts with * or +. */
#define WORST 0     /* Worst case. */

/*
 * Global work variables for regcomp().
 */
char* regparse; /* Input-scan pointer. */
int regnpar;    /* () count. */
char regdummy;
char* regcode; /* Code-emit pointer; &regdummy = don't. */
long regsize;  /* Code size. */

/*
 * Forward declarations for regcomp()'s friends.
 */
char* reg(int paren, int* flagp);
char* regbranch(int* flagp);
char* regpiece(int* flagp);
char* regatom(int* flagp);
char* regnode(char op);
char* regnext(char* p);
void regc(char b);
void reginsert(char op, char* opnd);
void regtail(char* p, char* val);
void regoptail(char* p, char* val);

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
regexp* regcomp(char* exp, const char*& lastErrorText)
{
    __RegExpSection.Enter();
    CRegularExpression::LastError = NULL;

    regexp* r;
    char* scan;
    char* longest;
    int len;
    int flags;

    /* First pass: determine size, legality. */
    regparse = exp;
    regnpar = 1;
    regsize = 0L;
    regcode = &regdummy;
    regc(MAGIC);
    if (reg(0, &flags) == NULL)
    {
        lastErrorText = CRegularExpression::LastError;
        __RegExpSection.Leave();
        return (NULL);
    }

    /* Small enough for pointer-storage convention? */
    if (regsize >= 32767L) /* Probably could be 65535L. */
    {
        regerror(RegExpErrorText(reeTooBig));
        lastErrorText = CRegularExpression::LastError;
        __RegExpSection.Leave();
        return (NULL);
    }

    /* Allocate space. */
    r = (regexp*)malloc(sizeof(regexp) + (unsigned)regsize);
    if (r == NULL)
    {
        regerror(RegExpErrorText(reeLowMemory));
        lastErrorText = CRegularExpression::LastError;
        __RegExpSection.Leave();
        return (NULL);
    }

    /* Second pass: emit code. */
    regparse = exp;
    regnpar = 1;
    regcode = r->program;
    regc(MAGIC);
    if (reg(0, &flags) == NULL)
    {
        lastErrorText = CRegularExpression::LastError;
        __RegExpSection.Leave();
        return (NULL);
    }

    /* Dig out information for optimizations. */
    r->regstart = '\0'; /* Worst-case defaults. */
    r->reganch = 0;
    r->regmust = NULL;
    r->regmlen = 0;
    scan = r->program + 1; /* First BRANCH. */
    if (OP(regnext(scan)) == END)
    { /* Only one top-level choice. */
        scan = OPERAND(scan);

        /* Starting-point info. */
        if (OP(scan) == EXACTLY)
            r->regstart = *OPERAND(scan);
        else if (OP(scan) == BOL)
            r->reganch++;

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
            for (; scan != NULL; scan = regnext(scan))
                if (OP(scan) == EXACTLY && (int)strlen(OPERAND(scan)) >= len)
                {
                    longest = OPERAND(scan);
                    len = (int)strlen(OPERAND(scan));
                }
            r->regmust = longest;
            r->regmlen = len;
        }
    }

    lastErrorText = NULL; // uspesny navrat
    __RegExpSection.Leave();
    return (r);
}

/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
char* reg(int paren /* Parenthesized? */, int* flagp)
{
    char* ret;
    char* br;
    char* ender;
    int parno;
    int flags;

    *flagp = HASWIDTH; /* Tentatively. */

    /* Make an OPEN node, if parenthesized. */
    if (paren)
    {
        if (regnpar >= NSUBEXP)
            FAIL(RegExpErrorText(reeTooManyParenthesises));
        parno = regnpar;
        regnpar++;
        ret = regnode((char)(OPEN + parno));
    }
    else
        ret = NULL;

    /* Pick up the branches, linking them together. */
    br = regbranch(&flags);
    if (br == NULL)
        return (NULL);
    if (ret != NULL)
        regtail(ret, br); /* OPEN -> first. */
    else
        ret = br;
    if (!(flags & HASWIDTH))
        *flagp &= ~HASWIDTH;
    *flagp |= flags & SPSTART;
    while (*regparse == '|')
    {
        regparse++;
        br = regbranch(&flags);
        if (br == NULL)
            return (NULL);
        regtail(ret, br); /* BRANCH -> BRANCH. */
        if (!(flags & HASWIDTH))
            *flagp &= ~HASWIDTH;
        *flagp |= flags & SPSTART;
    }

    /* Make a closing node, and hook it on the end. */
    ender = regnode((char)((paren) ? CLOSE + parno : END));
    regtail(ret, ender);

    /* Hook the tails of the branches to the closing node. */
    for (br = ret; br != NULL; br = regnext(br))
        regoptail(br, ender);

    /* Check for proper termination. */
    if (paren && *regparse++ != ')')
    {
        FAIL(RegExpErrorText(reeUnmatchedParenthesis));
    }
    else if (!paren && *regparse != '\0')
    {
        if (*regparse == ')')
        {
            FAIL(RegExpErrorText(reeUnmatchedParenthesis));
        }
    }

    return (ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
char* regbranch(int* flagp)
{
    char* ret;
    char* chain;
    char* latest;
    int flags;

    *flagp = WORST; /* Tentatively. */

    ret = regnode(BRANCH);
    chain = NULL;
    while (*regparse != '\0' && *regparse != '|' && *regparse != ')')
    {
        latest = regpiece(&flags);
        if (latest == NULL)
            return (NULL);
        *flagp |= flags & HASWIDTH;
        if (chain == NULL) /* First piece. */
            *flagp |= flags & SPSTART;
        else
            regtail(chain, latest);
        chain = latest;
    }
    if (chain == NULL) /* Loop ran zero times. */
        (void)regnode(NOTHING);

    return (ret);
}

/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
char* regpiece(int* flagp)
{
    char* ret;
    char op;
    char* next;
    int flags;

    ret = regatom(&flags);
    if (ret == NULL)
        return (NULL);

    op = *regparse;
    if (!ISMULT(op))
    {
        *flagp = flags;
        return (ret);
    }

    if (!(flags & HASWIDTH) && op != '?')
        FAIL(RegExpErrorText(reeOperandCouldBeEmpty));
    *flagp = (op != '+') ? (WORST | SPSTART) : (WORST | HASWIDTH);

    if (op == '*' && (flags & SIMPLE))
        reginsert(STAR, ret);
    else if (op == '*')
    {
        /* Emit x* as (x&|), where & means "self". */
        reginsert(BRANCH, ret);         /* Either x */
        regoptail(ret, regnode(BACK));  /* and loop */
        regoptail(ret, ret);            /* back */
        regtail(ret, regnode(BRANCH));  /* or */
        regtail(ret, regnode(NOTHING)); /* null. */
    }
    else if (op == '+' && (flags & SIMPLE))
        reginsert(PLUS, ret);
    else if (op == '+')
    {
        /* Emit x+ as x(&|), where & means "self". */
        next = regnode(BRANCH); /* Either */
        regtail(ret, next);
        regtail(regnode(BACK), ret);    /* loop back */
        regtail(next, regnode(BRANCH)); /* or */
        regtail(ret, regnode(NOTHING)); /* null. */
    }
    else if (op == '?')
    {
        /* Emit x? as (x|) */
        reginsert(BRANCH, ret);        /* Either x */
        regtail(ret, regnode(BRANCH)); /* or */
        next = regnode(NOTHING);       /* null. */
        regtail(ret, next);
        regoptail(ret, next);
    }
    regparse++;
    if (ISMULT(*regparse))
        FAIL(RegExpErrorText(reeNested));

    return (ret);
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 */
char* regatom(int* flagp)
{
    char* ret;
    int flags;

    *flagp = WORST; /* Tentatively. */

    switch (*regparse++)
    {
    case '^':
        ret = regnode(BOL);
        break;
    case '$':
        ret = regnode(EOL);
        break;
    case '.':
        ret = regnode(ANY);
        *flagp |= HASWIDTH | SIMPLE;
        break;
    case '[':
    {
        int _class;
        int classend;

        if (*regparse == '^')
        { /* Complement of range. */
            ret = regnode(ANYBUT);
            regparse++;
        }
        else
            ret = regnode(ANYOF);
        if (*regparse == ']' || *regparse == '-')
            regc(*regparse++);
        while (*regparse != '\0' && *regparse != ']')
        {
            if (*regparse == '-')
            {
                regparse++;
                if (*regparse == ']' || *regparse == '\0')
                    regc('-');
                else
                {
                    _class = UCHARAT(regparse - 2) + 1;
                    classend = UCHARAT(regparse);
                    if (_class > classend + 1)
                        FAIL(RegExpErrorText(reeInvalidRange));
                    for (; _class <= classend; _class++)
                        regc((char)_class);
                    regparse++;
                }
            }
            else
                regc(*regparse++);
        }
        regc('\0');
        if (*regparse != ']')
            FAIL(RegExpErrorText(reeUnmatchedBracket));
        regparse++;
        *flagp |= HASWIDTH | SIMPLE;
    }
    break;
    case '(':
        ret = reg(1, &flags);
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
        FAIL(RegExpErrorText(reeFollowsNothing));
    case '\\':
        if (*regparse == '\0')
            FAIL(RegExpErrorText(reeTrailingBackslash));
        ret = regnode(EXACTLY);
        regc(*regparse++);
        regc('\0');
        *flagp |= HASWIDTH | SIMPLE;
        break;
    default:
    {
        int len;
        char ender;

        regparse--;
        len = (int)strcspn(regparse, META);
        if (len <= 0)
            FAIL(RegExpErrorText(reeInternalDisaster));
        ender = *(regparse + len);
        if (len > 1 && ISMULT(ender))
            len--; /* Back off clear of ?+* operand. */
        *flagp |= HASWIDTH;
        if (len == 1)
            *flagp |= SIMPLE;
        ret = regnode(EXACTLY);
        while (len > 0)
        {
            regc(*regparse++);
            len--;
        }
        regc('\0');
    }
    break;
    }

    return (ret);
}

/*
 - regnode - emit a node
 */
char* regnode(char op) /* Location. */
{
    char* ret;
    char* ptr;

    ret = regcode;
    if (ret == &regdummy)
    {
        regsize += 3;
        return (ret);
    }

    ptr = ret;
    *ptr++ = op;
    *ptr++ = '\0'; /* Null "next" pointer. */
    *ptr++ = '\0';
    regcode = ptr;

    return (ret);
}

/*
 - regc - emit (if appropriate) a byte of code
 */
void regc(char b)
{
    if (regcode != &regdummy)
        *regcode++ = b;
    else
        regsize++;
}

/*
 - reginsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 */
void reginsert(char op, char* opnd)
{
    char* src;
    char* dst;
    char* place;

    if (regcode == &regdummy)
    {
        regsize += 3;
        return;
    }

    src = regcode;
    regcode += 3;
    dst = regcode;
    while (src > opnd)
        *--dst = *--src;

    place = opnd; /* Op node, where operand used to be. */
    *place++ = op;
    *place++ = '\0';
    *place++ = '\0';
}

/*
 - regtail - set the next-pointer at the end of a node chain
 */
void regtail(char* p, char* val)
{
    char* scan;
    char* temp;
    int offset;

    if (p == &regdummy)
        return;

    /* Find last node. */
    scan = p;
    for (;;)
    {
        temp = regnext(scan);
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
 - regoptail - regtail on operand of first argument; nop if operandless
 */
void regoptail(char* p, char* val)
{
    /* "Operandless" and "op != BRANCH" are synonymous in practice. */
    if (p == NULL || p == &regdummy || OP(p) != BRANCH)
        return;
    regtail(OPERAND(p), val);
}

/*
 * regexec and friends
 */

/*
 * Global work variables for regexec().
 */
char* reginput;   /* String-input pointer. */
char* regbol;     /* Beginning of input, for ^ check. */
char** regstartp; /* Pointer to startp array. */
char** regendp;   /* Ditto for endp. */

/*
 * Forwards.
 */
int regtry(regexp* prog, char* string);
int regmatch(char* prog);
int regrepeat(char* p);

/*
 - regexec - match a regexp against a string
 */
int regexec(regexp* prog, char* string, int offset)
{
    __RegExpSection.Enter();
    char* s;

    /* Check validity of program. */
    if (UCHARAT(prog->program) != MAGIC)
    {
        __RegExpSection.Leave();
        return (0);
    }

    /* If there is a "must appear" string, look for it. */
    if (prog->regmust != NULL)
    {
        s = string + offset;
        while ((s = strchr(s, prog->regmust[0])) != NULL)
        {
            if (strncmp(s, prog->regmust, prog->regmlen) == 0)
                break; /* Found it. */
            s++;
        }
        if (s == NULL) /* Not present. */
        {
            __RegExpSection.Leave();
            return (0);
        }
    }

    /* Mark beginning of line for ^ . */
    regbol = string;

    /* Simplest case:  anchored match need be tried only once. */
    if (prog->reganch)
    {
        if (regtry(prog, string + offset))
        {
            __RegExpSection.Leave();
            return (1);
        }
        else
        {
            __RegExpSection.Leave();
            return (0);
        }
    }

    /* Messy cases:  unanchored match. */
    s = string + offset;
    if (prog->regstart != '\0')
        /* We know what char it must start with. */
        while ((s = strchr(s, prog->regstart)) != NULL)
        {
            if (regtry(prog, s))
            {
                __RegExpSection.Leave();
                return (1);
            }
            s++;
        }
    else
        /* We don't -- general case. */
        do
        {
            if (regtry(prog, s))
            {
                __RegExpSection.Leave();
                return (1);
            }
        } while (*s++ != '\0');

    /* Failure. */
    __RegExpSection.Leave();
    return (0);
}

/*
 - regtry - try match at specific point
 */
int regtry(regexp* prog, char* string) /* 0 failure, 1 success */
{
    int i;
    char** sp;
    char** ep;

    reginput = string;
    regstartp = prog->startp;
    regendp = prog->endp;

    sp = prog->startp;
    ep = prog->endp;
    for (i = NSUBEXP; i > 0; i--)
    {
        *sp++ = NULL;
        *ep++ = NULL;
    }
    if (regmatch(prog->program + 1))
    {
        prog->startp[0] = string;
        prog->endp[0] = reginput;
        return (1);
    }
    else
        return (0);
}

/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
int regmatch(char* prog) /* 0 failure, 1 success */
{
    char* scan; /* Current node. */
    char* next; /* Next node. */

    scan = prog;
    while (scan != NULL)
    {
        next = regnext(scan);

        switch (OP(scan))
        {
        case BOL:
            if (reginput != regbol)
                return (0);
            break;
        case EOL:
            if (*reginput != '\0')
                return (0);
            break;
        case ANY:
            if (*reginput == '\0')
                return (0);
            reginput++;
            break;
        case EXACTLY:
        {
            int len;
            char* opnd;

            opnd = OPERAND(scan);
            /* Inline the first character, for speed. */
            if (*opnd != *reginput)
                return (0);
            len = (int)strlen(opnd);
            if (len > 1 && strncmp(opnd, reginput, len) != 0)
                return (0);
            reginput += len;
        }
        break;
        case ANYOF:
            if (*reginput == '\0' || strchr(OPERAND(scan), *reginput) == NULL)
                return (0);
            reginput++;
            break;
        case ANYBUT:
            if (*reginput == '\0' || strchr(OPERAND(scan), *reginput) != NULL)
                return (0);
            reginput++;
            break;
        case NOTHING:
            break;
        case BACK:
            break;
        case OPEN + 1:
        case OPEN + 2:
        case OPEN + 3:
        case OPEN + 4:
        case OPEN + 5:
        case OPEN + 6:
        case OPEN + 7:
        case OPEN + 8:
        case OPEN + 9:
        {
            int no;
            char* save;

            no = OP(scan) - OPEN;
            save = reginput;

            if (regmatch(next))
            {
                /*
             * Don't set startp if some later
             * invocation of the same parentheses
             * already has.
             */
                if (regstartp[no] == NULL)
                    regstartp[no] = save;
                return (1);
            }
            else
                return (0);
        }
        case CLOSE + 1:
        case CLOSE + 2:
        case CLOSE + 3:
        case CLOSE + 4:
        case CLOSE + 5:
        case CLOSE + 6:
        case CLOSE + 7:
        case CLOSE + 8:
        case CLOSE + 9:
        {
            int no;
            char* save;

            no = OP(scan) - CLOSE;
            save = reginput;

            if (regmatch(next))
            {
                /*
             * Don't set endp if some later
             * invocation of the same parentheses
             * already has.
             */
                if (regendp[no] == NULL)
                    regendp[no] = save;
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
                    save = reginput;
                    if (regmatch(OPERAND(scan)))
                        return (1);
                    reginput = save;
                    scan = regnext(scan);
                } while (scan != NULL && OP(scan) == BRANCH);
                return (0);
                /* NOTREACHED */
            }
        }
        break;
        case STAR:
        case PLUS:
        {
            char nextch;
            int no;
            char* save;
            int min;

            /*
           * Lookahead to avoid useless match attempts
           * when we know what character comes next.
           */
            nextch = '\0';
            if (OP(next) == EXACTLY)
                nextch = *OPERAND(next);
            min = (OP(scan) == STAR) ? 0 : 1;
            save = reginput;
            no = regrepeat(OPERAND(scan));
            while (no >= min)
            {
                /* If it could work, try it. */
                if (nextch == '\0' || *reginput == nextch)
                    if (regmatch(next))
                        return (1);
                /* Couldn't or didn't -- back up. */
                no--;
                reginput = save + no;
            }
            return (0);
        }

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
 - regrepeat - repeatedly match something simple, report how many
 */
int regrepeat(char* p)
{
    int count = 0;
    char* scan;
    char* opnd;

    scan = reginput;
    opnd = OPERAND(p);
    switch (OP(p))
    {
    case ANY:
        count = (int)strlen(scan);
        scan += count;
        break;
    case EXACTLY:
        while (*opnd == *scan)
        {
            count++;
            scan++;
        }
        break;
    case ANYOF:
        while (*scan != '\0' && strchr(opnd, *scan) != NULL)
        {
            count++;
            scan++;
        }
        break;
    case ANYBUT:
        while (*scan != '\0' && strchr(opnd, *scan) == NULL)
        {
            count++;
            scan++;
        }
        break;
    default:       /* Oh dear.  Called inappropriately. */
                   //    regerror("internal foulup");
        count = 0; /* Best compromise. */
        break;
    }
    reginput = scan;

    return (count);
}

/*
 - regnext - dig the "next" pointer out of a node
 */
char* regnext(char* p)
{
    int offset;

    if (p == &regdummy)
        return (NULL);

    offset = NEXT(p);
    if (offset == 0)
        return (NULL);

    if (OP(p) == BACK)
        return (p - offset);
    else
        return (p + offset);
}
