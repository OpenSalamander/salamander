// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CVarString
{
public:
    // tridy
    class CVariable
    {
    public:
        static CVariable* Alloc() { return NULL; };
        virtual ~CVariable() { ; }

        virtual BOOL SetArguments(const char* argStart, const char* argEnd,
                                  int& error, const char*& errorPos1, const char*& errorPos2);
        virtual int Expand(char*& string, char* end, LPVOID param) = 0;
    };

    struct CVariableEntry
    {
        const char* Name;
        CVariable* (*Alloc)();
    };

    // metody
    CVarString();

    BOOL IsGood() { return Error == 0; }
    int GetError() { return Error; }

    BOOL Compile(const char* varText, int& error, int& errorPos1, int& errorPos2,
                 CVariableEntry* variables);

    int Execute(char* buffer, int max, LPVOID param);

protected:
    TIndirectArray<CVariable> Stack;
    TBuffer<char> Temp;
    int Error;
};
