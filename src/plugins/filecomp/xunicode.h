// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define IS_COMBINING_DIACRITIC(c) (((c) >= 0x300) && ((c) <= 0x36f))

template <class CChar>
class TCharSpecific
{
};

template <>
class TCharSpecific<char>
{
public:
    typedef POLYTEXTA POLYTEXT;
    typedef unsigned char Unsigned;

    static char* LowerCase;
    static unsigned short* CType;

    static const bool IsUnicode() { return false; }
    static char ConvertANSI8Char(char c) { return c; }
    static bool IsBOM(char c) { return false; }
    static int CharCount() { return 256; }
    static bool IsValidChar(char c) { return true; }
};

template <>
class TCharSpecific<wchar_t>
{
public:
    typedef POLYTEXTW POLYTEXT;
    typedef unsigned short Unsigned;

    static wchar_t LowerCase[256 * 256];
    static unsigned short CType[256 * 256];

    static const bool IsUnicode() { return true; }
    static wchar_t ConvertANSI8Char(char c);
    static bool IsBOM(wchar_t c) { return c == 0xFEFF; }
    static int CharCount() { return 256 * 256; }
    static bool IsValidChar(wchar_t c) { return c != 0xFFFE && c != 0xFFFF && (c < 0xD800 || c > 0xDFFF); }
};

inline const char*
MemChr(const char* buf, char c, size_t count)
{
    return (const char*)memchr(buf, c, count);
}

inline const wchar_t*
MemChr(const wchar_t* buf, wchar_t c, size_t count)
{
    size_t i = 0;
    while (i < count && buf[i] != c)
        i++;

    return buf + i;
}

template <class CChar>
inline CChar ToLowerX(CChar c)
{
    return TCharSpecific<CChar>::LowerCase[TCharSpecific<CChar>::Unsigned(c)];
}

template <class CChar>
inline int IsSpaceX(CChar c)
{
    return TCharSpecific<CChar>::CType[TCharSpecific<CChar>::Unsigned(c)] & C1_SPACE;
}

template <class CChar>
inline int IsWordX(CChar c)
{
    return (TCharSpecific<CChar>::CType[TCharSpecific<CChar>::Unsigned(c)] & (C1_ALPHA | C1_DIGIT)) || c == '_';
}

inline BOOL
ExtTextOutX(HDC hdc, int X, int Y, UINT fuOptions, CONST RECT* lprc, LPCSTR lpString, UINT cbCount, CONST INT* lpDx)
{
    return ExtTextOutA(hdc, X, Y, fuOptions, lprc, lpString, cbCount, lpDx);
}

inline BOOL
ExtTextOutX(HDC hdc, int X, int Y, UINT fuOptions, CONST RECT* lprc, LPCWSTR lpString, UINT cbCount, CONST INT* lpDx)
{
    return ExtTextOutW(hdc, X, Y, fuOptions, lprc, lpString, cbCount, lpDx);
}

inline int
DrawTextExX(HDC hdc, LPSTR lpchText, int cchText, LPRECT lprc, UINT dwDTFormat,
            LPDRAWTEXTPARAMS lpDTParams)
{
    return DrawTextExA(hdc, lpchText, cchText, lprc, dwDTFormat, lpDTParams);
}

inline int
DrawTextExX(HDC hdc, LPWSTR lpchText, int cchText, LPRECT lprc, UINT dwDTFormat,
            LPDRAWTEXTPARAMS lpDTParams)
{
    return DrawTextExW(hdc, lpchText, cchText, lprc, dwDTFormat, lpDTParams);
}

inline BOOL
PolyTextOutX(HDC hdc, CONST POLYTEXTA* pptxt, int cStrings)
{
    return PolyTextOutA(hdc, pptxt, cStrings);
}

inline BOOL
PolyTextOutX(HDC hdc, CONST POLYTEXTW* pptxt, int cStrings)
{
    return PolyTextOutW(hdc, pptxt, cStrings);
}

inline BOOL
CopyTextToClipboard(const char* text, int textLen, BOOL showEcho, HWND echoParent)
{
    return SG->CopyTextToClipboard(text, textLen, showEcho, echoParent);
}

inline BOOL
CopyTextToClipboard(wchar_t* text, int textLen, BOOL showEcho, HWND echoParent)
{
    return SG->CopyTextToClipboardW(text, textLen, showEcho, echoParent);
}

void InitXUnicode();
