// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

char LowerCase[256];
char UpperCase[256];
WORD CType[256];

// ****************************************************************************
//
// automaticka inicializace modulu
//

class CSTR
{
public:
    CSTR();
} __STR;

CSTR::CSTR()
{
    CALL_STACK_MESSAGE_NONE
    uintptr_t i;
    char charTable[256];
    for (i = 0; i < 256; i++)
    {
        LowerCase[i] = (char)(UINT_PTR)CharLowerA((LPSTR)i);
        UpperCase[i] = (char)(UINT_PTR)CharUpperA((LPSTR)i);
        charTable[i] = (unsigned char)i;
    }
    if (!GetStringTypeA(LOCALE_USER_DEFAULT, CT_CTYPE1, charTable, 256, CType))
    {
        //TRACE_E("GetStringTypeA failed. Last Error = " << GetLastError());
    }
}

// ****************************************************************************

// optimalizace: předpokládá, že s2 je v lowercase

int MemCmpI(const char* s1, const char* s2, int n)
{
    CALL_STACK_MESSAGE_NONE
    int res;
    while (n--)
    {
        res = (unsigned)LowerCase[(unsigned)*s1++] - (unsigned)*s2++;
        if (res != 0)
            return (res < 0) ? -1 : 1; // < a >
    }
    return 0;
}

// optimalizace: předpokládá, že s je v lowercase

char* MemChrI(char* s, char c, int n)
{
    CALL_STACK_MESSAGE_NONE
    while (n--)
    {
        if (LowerCase[(unsigned)*s] == c)
            return s;
        s++;
    }
    return NULL;
}

char* StrNCat(char* dest, const char* sour, int destSize)
{
    CALL_STACK_MESSAGE_NONE
    if (destSize == 0)
        return dest;
    char* start = dest;
    while (*dest)
        dest++;
    destSize -= int(dest - start);
    while (*sour && --destSize)
        *dest++ = *sour++;
    *dest = NULL;
    return start;
}
