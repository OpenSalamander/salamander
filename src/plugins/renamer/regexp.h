// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ******************************************************************
//
// Internal RegExp routines
//

#define MAX_SUBEXP 256

class CRegExp : public CRegExpAbstract
{
    CRegExpErrors State;
    unsigned Options;
    int Compiled;

    char RegStart; // Internal use only.
    char RegAnch;  // Internal use only.
    char* RegMust; // Internal use only.
    int RegMLen;   // Internal use only.
    char* Program; // Internal use only.

public:
    virtual BOOL RegComp(char* exp, unsigned options);
    virtual BOOL RegExec(char* string, int length, int offset, BOOL separateThread = FALSE);
    virtual BOOL IsGood() { return State == reeNoError; }
    virtual CRegExpErrors GetState() { return State; }
    virtual const char* GetStateText() { return RegExpErrorText(State); }

    CRegExp()
    {
        State = reeNoError, Program = NULL;
        Startp = NULL;
        Compiled = 0;
    }
    ~CRegExp()
    {
        if (Program)
            free(Program);
        if (Startp)
            free(Startp);
    }

private:
    int Caseles;

    // Global work variables for regcomp().

    char* RegParse; // Input-scan pointer.
    char RegDummy;
    char* RegCode; // Code-emit pointer; &regdummy = don't.
    long RegSize;  // Code size.

    // RegComp()'s friends.
    char* RegExt(int paren, int* flagp, int* fixedwp);
    char* Reg(int paren, int nocapture, int* flagp, int* fixedwp);
    char* RegBranch(int* flagp, int* fixedwp);
    char* RegPiece(int* flagp, int* fixedw);
    char* RegAtom(int* flagp, int* fixedw);
    char* RegNode(char op);
    char* RegNext(char* p);
    void RegC(char b);
    void RegInsert(char op, char* opnd);
    void RegTail(char* p, char* val);
    void RegOpTail(char* p, char* val);
    void RegClass(char c, char* cls) //insert character in the class
    {
        if (RegCode != &RegDummy)
            cls[c >> 3] |= 1 << (c & 0x07);
    }
    int IsPosixClass(char* scan);
    int RegPosixClass(const char* scan);
    char GetEscapedChar();

    // Global work variables for regexec().
    char* RegInput;   /* String-input pointer. */
    char* RegBol;     /* Beginning of input, for ^ check. */
    char* RegEol;     /* End of input */
    char** RegStartp; /* Pointer to startp array. */
    char** RegEndp;   /* Ditto for endp. */

    // RegExec()'s friends
    static DWORD WINAPI RegExecThread(void* data);
    int RegTry(char* string);
    int RegMatch(char* prog);
    int RegRepeat(char* p);
    int RegMatchSimple(char op, char* opnd);
    int RegMatchEol(char* scan)
    {
        if (scan == RegEol)
            return 1;
        if ((Options & RE_LF) && *scan == '\n')
            return 1;
        if ((Options & RE_CR) && *scan == '\r')
            return 1;
        if ((Options & RE_NULL) && *scan == '\0')
            return 1;
        if ((Options & RE_CRLF) && scan + 1 < RegEol &&
            scan[0] == '\r' && scan[1] == '\n')
            return 1;
        return 0;
    }

    int IsNewLine();
};
