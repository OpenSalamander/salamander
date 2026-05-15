// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

//*****************************************************************************
//*****************************************************************************
//
// original regexp.h
//
//*****************************************************************************
//*****************************************************************************

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
#define NSUBEXP 10
typedef struct regexp
{
    char* startp[NSUBEXP];
    char* endp[NSUBEXP];
    char regstart;   /* Internal use only. */
    char reganch;    /* Internal use only. */
    char* regmust;   /* Internal use only. */
    int regmlen;     /* Internal use only. */
    char program[1]; /* Unwarranted chumminess with compiler. */
} regexp;

regexp* regcomp(char* exp, const char*& lastErrorText);
int regexec(regexp* prog, char* string, int offset);
void regerror(const char* error);

//*****************************************************************************
//*****************************************************************************
//
// my section of regexp.h
//
//*****************************************************************************
//*****************************************************************************

// Errors that may occur during regexp compilation and search.
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
    reeInvalidRange,
    reeUnmatchedBracket,
    reeFollowsNothing,
    reeTrailingBackslash,
    reeInternalDisaster,
};

// Function that returns the error text.
const char* RegExpErrorText(CRegExpErrors err);

// search flags
#define sfCaseSensitive 0x01 // 0. bit = 1
#define sfForward 0x02       // 1. bit = 1

//*****************************************************************************
//
// CRegularExpression
//

class CRegularExpression
{
public:
    static const char* LastError; // Last error text.

protected:
    const char* LastErrorText;
    char* OriginalPattern;
    regexp* Expression; // Compiled regular expression.
    WORD Flags;

    char* Line;                // Line buffer.
    const char* OrigLineStart; // Pointer to the start of the original text (passed to SetLine() as 'start').
    int Allocated;             // Number of allocated bytes.
    int LineLength;            // Current line length.

public:
    CRegularExpression()
    {
        Expression = NULL;
        OriginalPattern = NULL;
        Flags = sfCaseSensitive | sfForward;
        Line = NULL;
        OrigLineStart = NULL;
        Allocated = 0;
        LineLength = 0;
        LastErrorText = NULL;
    }

    ~CRegularExpression()
    {
        if (Expression != NULL)
            free(Expression);
        if (OriginalPattern != NULL)
            free(OriginalPattern);
        if (Line != NULL)
            free(Line);
    }

    BOOL IsGood() const { return OriginalPattern != NULL && Expression != NULL; }
    const char* GetPattern() const { return OriginalPattern; }

    const char* GetLastErrorText() const { return LastErrorText; }
    BOOL Set(const char* pattern, WORD flags); // Returns FALSE on error (call GetLastErrorText).
    BOOL SetFlags(WORD flags);                 // Returns FALSE on error (call GetLastErrorText).

    BOOL SetLine(const char* start, const char* end); // Sets the line of text to search in; returns FALSE on error (call GetLastErrorText).

    int SearchForward(int start, int& foundLen);
    int SearchBackward(int length, int& foundLen);

    // Replaces variables \1 ... \9 with the text captured by the corresponding groups.
    // 'pattern' is the replacement pattern for the matched text, 'buffer' is the output
    // buffer, 'bufSize' is the maximum buffer size including the terminating NULL
    // character, and 'count' receives the number of characters copied to 'buffer'
    // Returns TRUE if the entire expanded text fit in 'buffer'
    BOOL ExpandVariables(char* pattern, char* buffer,
                         int bufSize, int* count);

    // Return values
    //
    // 0 the text was not found, nothing was copied to 'buffer'
    // 1 the text was replaced successfully
    // 2 'buffer' is too small
    int ReplaceForward(int start, char* pattern, BOOL global,
                       char* buffer, int bufSize);

protected:
    // Reverses the regular expression for backward search.
    // THE EXPRESSION MUST BE SYNTACTICALLY CORRECT, OR IT WILL NOT WORK PROPERLY!
    // e.g. "a)b(d)(" -> "((d)b)a", which is incorrect
    void ReverseRegExp(char*& dstExpEnd, char* srcExp, char* srcExpEnd);
};
