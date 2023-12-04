// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//#define ENABLE_TRACE_X
// TRACE_X is used for listing of found files, could be enabled with custom define
#if !defined(ENABLE_TRACE_X) || !defined(TRACE_ENABLE)
inline void __TraceXEmptyFunction(){};
#define TRACE_X(x) __TraceXEmptyFunction()
#define TRACE_XW(x) __TraceXEmptyFunction()
#else
#define TRACE_X(x) TRACE_I(x)
#define TRACE_XW(x) TRACE_IW(x)
#endif

// ***************************************************************************
//
//  Auxiliary functions
//

template <typename CHAR>
class String
{
private:
    static const unsigned short STRBUFSIZE;
    static CHAR* StringBuffer;
    static CHAR* StrAct;

public:
    // initialization
    static BOOL InitStr();
    static void ReleaseStr();

    // string functions
    static void SPrintF(CHAR* buffer, const CHAR* pattern, ...);
    static void VSPrintF(CHAR* buffer, const CHAR* pattern, va_list& marker);
    static int StrICmp(const CHAR* string1, const CHAR* string2);
    static int StrCmp(const CHAR* string1, const CHAR* string2);
    static CHAR* StrCpy(CHAR* text1, const CHAR* text2);
    static CHAR* StrCat(CHAR* text1, const CHAR* text2);
    static errno_t StrCat_s(CHAR* strDestination, size_t numberOfElements, const CHAR* strSource);
    static CHAR ToUpper(CHAR c);
    static size_t StrLen(const CHAR* text);
    static CHAR* NewStr(const CHAR* str);
    static CHAR* NewFromASCII(const char* src);
    static CHAR* NewFromUnicode(const wchar_t* src, unsigned long srclen);
    static CHAR* CopyFromASCII(CHAR* dest, const char* src, unsigned long srclen, unsigned long destlen);
    static CHAR* CopyFromUnicode(CHAR* dest, const wchar_t* src, unsigned long srclen, unsigned long destlen);
    static wchar_t* CopyToUnicode(wchar_t* dest, const CHAR* src, unsigned long srclen, unsigned long destlen);

    static CHAR* LoadStr(int resID);
    static CHAR* AddNumberSuffix(CHAR* filename, int n);

    static BOOL SysError(int title, int error, ...);
    static BOOL Error(int title, int error, ...);

    static int PartialRestore(int checkBoxText, BOOL* checkBoxValue, int title, int text, ...);
};

// ***************************************************************************
//
//  Template methods implementation
//

template <typename CHAR>
BOOL String<CHAR>::InitStr()
{
    if (!StringBuffer)
    {
        StringBuffer = new CHAR[STRBUFSIZE];
        StrAct = StringBuffer;
    }
    if (!StringBuffer)
    {
        MessageBoxA(NULL, "Insufficient memory", "Undelete", MB_OK | MB_ICONWARNING);
        return FALSE;
    }
    return TRUE;
}

template <typename CHAR>
void String<CHAR>::ReleaseStr()
{
    delete[] StringBuffer;
    StringBuffer = NULL;
}

template <typename CHAR>
void String<CHAR>::SPrintF(CHAR* buffer, const CHAR* pattern, ...)
{
    va_list marker;
    va_start(marker, pattern);
    VSPrintF(buffer, pattern, marker);
    va_end(marker);
}

template <typename CHAR>
CHAR* String<CHAR>::NewStr(const CHAR* str)
{
    CHAR* ret = new CHAR[StrLen(str) + 1];
    if (ret != NULL)
        StrCpy(ret, str);
    return ret;
}
