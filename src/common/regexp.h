// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

//*****************************************************************************
//*****************************************************************************
//
// puvodni regexp.h
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
// moje cast regexp.h
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

    // nahradi promnene \1 ... \9 textem zachycenym odpovidajicima zavorkama
    // 'pattern' je vzor kterym se nahrazuje nalezeny match, 'buffer' buffer
    // pro vystup, 'bufSize' maximalni velikost textu vcetne ukoncovaciho NULL
    // znaku, v promnene 'count' vraci pocet znaku zkopirovanych do bufferu
    // vraci TRUE pokud se vyraz vesel cely do bufferu
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
    // Obraci regularni vyraz - pro hledani od zadu
    // VYRAZ MUSI BYT SYNTAKTICKY SPRAVNY ! JINAK NEFUNGUJE SPRAVNE !
    // napr. "a)b(d)(" -> "((d)b)a" coz je chybne
    void ReverseRegExp(char*& dstExpEnd, char* srcExp, char* srcExpEnd);
};
