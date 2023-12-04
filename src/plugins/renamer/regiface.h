// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef _CHAR_UNSIGNED
#error Signed char required to compile regexp. Use /J compiler option (MS).
#endif

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

// ******************************************************************
//
// Error codes - errors can occur when compilig or matching expresion
//

enum CRegExpErrors
{
    reeNoError,
    reeLowMemory,
    reeEmpty,
    reeTooBig,
    reeTooManyParenthesises,
    reeUnmatchedParenthesis,
    reeOperandCouldBeEmpty,
    reeNested,
    //reeInvalidRange,
    reeUnmatchedBracket,
    reeFollowsNothing,
    reeTrailingBackslash,
    reeInternalDisaster,
    reeExpectingExtendedPattern1,
    reeExpectingExtendedPattern2,
    reeInvalidPosixClass,
    reeExpectingXDigit,
    reeStackOverflow,
    reeNoPattern,
    reeErrorStartingThread,
    reeBadFixedWidthLookBehind
};

// return text representation of CRegExpErrors
const char* RegExpErrorText(CRegExpErrors err);

// ******************************************************************
//
// Options for RegComp
//

// Do case-insensitive pattern matching.
//
// Equivalent to Perl's /i option.
#define RE_CASELES 0x01

// Treat string as multiple lines.  That is, change "^"
// and "$" from matching at only the very start or end of
// the string to the start or end of any line anywhere
// within the string,
//
// Equivalent to Perl's /m option.
#define RE_MULTILINE 0x02

// Treat string as single line.  That is, change "." to
// match any character whatsoever, even a newline, which
// it normally would not match.
//
// Equivalent to Perl's /s option.
#define RE_SINGLELINE 0x04 // Equivalent to Perl's /s option.

// Accepted newline characters.
#define RE_LF 0x0100
#define RE_CR 0x0200
#define RE_CRLF 0x0400
#define RE_NULL 0x0800
#define RE_ALLENDS (RE_LF | RE_CR | RE_CRLF | RE_NULL)

class CRegExpAbstract
{
public:
    // Number of subexpresions in regular expression + 1 for
    // whole expression, not counting the non-capturing subexpresions
    // specified with (?:pattern) instead of (pattern).
    // SubExpCount is computed at runtime when compiling expression
    // with RegComp().
    int SubExpCount;

    // Pointers to starts and ends of matched substrings. If
    // subexpression #n doesn't match any substring, Startp[n] and
    // Endp[n] are NULL.
    // Startp[0] and Endp[0] represent start and end of whole
    // matched string.
    char** Startp;
    char** Endp;

    // Compiles regular expression 'exp' into an internal form .
    // 'options' is any combination of RE_xxx options specified above.
    // While 'exp' is considered to be a zero terminated string it
    // cannot contain NULL characters.
    // Returns success.
    virtual BOOL RegComp(char* exp, unsigned options) = 0;

    // Match 'string' against previously compiled pattern. 'length'
    // is leghth of string. Matching starts at 'offest' in subject
    // string.
    // If 'separateThread' is TRUE, matching is executed in separate
    // thread, such matching is protected against stack overflow,
    // caused by deep recursion of internal matching routines. If stack
    // overflow occurs, matching thread simply terminates and RegExec
    // returns FALSE.
    // If matching success RegExec sets Startp and Endp pointers and
    // return TRUE.
    // If string matching fails or in case of error RegExec return FALSE.
    virtual BOOL RegExec(char* string, int length, int offset, BOOL separateThread = FALSE) = 0;

    // returns TRUE if internal error state is reeNoError
    virtual BOOL IsGood() = 0;
    // returns internal error state
    virtual CRegExpErrors GetState() = 0;
    // returns text representation of internal error state
    virtual const char* GetStateText() = 0;

    virtual ~CRegExpAbstract() { ; }
};

#ifdef DECLARE_REGIFACE_FUNCTIONS

// Creates CRegExp object. Returns NULL in case of low memory
CRegExpAbstract* CreateRegExp();

// Release CRegExp object.
void ReleaseRegExp(CRegExpAbstract* regExp);

#endif //DECLARE_REGIFACE_FUNCTIONS
