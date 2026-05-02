// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

char* TCharSpecific<char>::LowerCase = ::LowerCase;
unsigned short* TCharSpecific<char>::CType = ::CType;

wchar_t TCharSpecific<wchar_t>::LowerCase[256 * 256];
unsigned short TCharSpecific<wchar_t>::CType[256 * 256];

wchar_t
TCharSpecific<wchar_t>::ConvertANSI8Char(char c)
{
    wchar_t ret = ' ';
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, &c, 1, &ret, 1);
    return ret;
}

void InitXUnicode()
{
    _ASSERT(TCharSpecific<char>::CType == ::CType);
    _ASSERT(TCharSpecific<char>::LowerCase == ::LowerCase);

    _ASSERT(sizeof(wchar_t) == 2);

    wchar_t charTable[256 * 256];
    uintptr_t i;
    for (i = 0; i < 256 * 256; i++)
    {
        // special handling for surrogates
        if (i >= 0xD800 && i <= 0xDFFF)
        {
            charTable[i] = 0;
            TCharSpecific<wchar_t>::LowerCase[i] = (wchar_t)i;
        }
        else
        {
            charTable[i] = wchar_t(i);
            TCharSpecific<wchar_t>::LowerCase[i] = (wchar_t)(UINT_PTR)CharLowerW((LPWSTR)i);
        }
    }

    if (!GetStringTypeW(CT_CTYPE1, charTable, 256 * 256, TCharSpecific<wchar_t>::CType))
    {
        TRACE_E("GetStringTypeW failed. Last Error = " << GetLastError());
    }

    // sanitize surrogates in CType table
    for (i = 0xD800; i <= 0xDFFF; i++)
        TCharSpecific<wchar_t>::CType[i] = 0;
}
