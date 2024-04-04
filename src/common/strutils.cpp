// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "strutils.h"

int ConvertU2A(const WCHAR* src, int srcLen, char* buf, int bufSize, BOOL compositeCheck, UINT codepage)
{
    if (buf == NULL || bufSize <= 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    buf[0] = 0;
    if (src == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (srcLen != -1 && srcLen <= 0)
    {
        if (srcLen == 0)
            return 1;
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    int res = WideCharToMultiByte(codepage, compositeCheck ? WC_COMPOSITECHECK : 0, src, srcLen, buf, bufSize, NULL, NULL);
    if (srcLen != -1 && res > 0)
        res++;
    if (compositeCheck && res == 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER) // Some codepages do not support WC_COMPOSITECHECK
    {
        res = WideCharToMultiByte(codepage, 0, src, srcLen, buf, bufSize, NULL, NULL);
        if (srcLen != -1 && res > 0)
            res++;
    }
    if (res > 0 && res <= bufSize)
        buf[res - 1] = 0; // success, terminate the string with a null character
    else
    {
        if (res > bufSize || res == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            buf[bufSize - 1] = 0; // small buffer, return an error, but leave the partially translated string in the buffer
        }
        else
            buf[0] = 0; // Another error, let's ensure an empty buffer
        res = 0;
    }
    return res;
}

char* ConvertAllocU2A(const WCHAR* src, int srcLen, BOOL compositeCheck, UINT codepage)
{
    if (src == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if (srcLen != -1 && srcLen <= 0)
    {
        if (srcLen == 0)
        {
            char* txt = (char*)malloc(1);
#ifndef SAFE_ALLOC
            if (txt == NULL)
                SetLastError(ERROR_OUTOFMEMORY);
            else
            {
#endif // SAFE_ALLOC
                *txt = 0;
#ifndef SAFE_ALLOC
            }
#endif // SAFE_ALLOC
            return txt;
        }
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    DWORD flags;
    int len = WideCharToMultiByte(codepage, flags = (compositeCheck ? WC_COMPOSITECHECK : 0), src, srcLen, NULL, 0, NULL, NULL);
    if (srcLen != -1 && len > 0)
        len++;
    if (compositeCheck && len == 0) // Some codepages do not support WC_COMPOSITECHECK
    {
        len = WideCharToMultiByte(codepage, flags = 0, src, srcLen, NULL, 0, NULL, NULL);
        if (srcLen != -1 && len > 0)
            len++;
    }
    if (len == 0)
        return NULL;
    char* txt = (char*)malloc(len);
#ifndef SAFE_ALLOC
    if (txt == NULL)
        SetLastError(ERROR_OUTOFMEMORY);
    else
    {
#endif // SAFE_ALLOC
        int res = WideCharToMultiByte(codepage, flags, src, srcLen, txt, len, NULL, NULL);
        if (srcLen != -1 && res > 0)
            res++;
        if (res > 0 && res <= len)
            txt[res - 1] = 0; // success, terminate the string with a null character
        else
        {
            DWORD err = GetLastError();
            free(txt);
            txt = NULL;
            SetLastError(err);
        }
#ifndef SAFE_ALLOC
    }
#endif // SAFE_ALLOC
    return txt;
}

int ConvertA2U(const char* src, int srcLen, WCHAR* buf, int bufSizeInChars, UINT codepage)
{
    if (buf == NULL || bufSizeInChars <= 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    buf[0] = 0;
    if (src == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (srcLen != -1 && srcLen <= 0)
    {
        if (srcLen == 0)
            return 1;
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    int res = MultiByteToWideChar(codepage, 0, src, srcLen, buf, bufSizeInChars);
    if (srcLen != -1 && res > 0)
        res++;
    if (res > 0 && res <= bufSizeInChars)
        buf[res - 1] = 0; // success, terminate the string with a null character
    else
    {
        if (res > bufSizeInChars || res == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            buf[bufSizeInChars - 1] = 0; // small buffer, return an error, but leave the partially translated string in the buffer
        }
        else
            buf[0] = 0; // Another error, let's ensure an empty buffer
        res = 0;
    }
    return res;
}

WCHAR* ConvertAllocA2U(const char* src, int srcLen, UINT codepage)
{
    if (src == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if (srcLen != -1 && srcLen <= 0)
    {
        if (srcLen == 0)
        {
            WCHAR* txt = (WCHAR*)malloc(1 * sizeof(WCHAR));
#ifndef SAFE_ALLOC
            if (txt == NULL)
                SetLastError(ERROR_OUTOFMEMORY);
            else
            {
#endif // SAFE_ALLOC
                *txt = 0;
#ifndef SAFE_ALLOC
            }
#endif // SAFE_ALLOC
            return txt;
        }
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    int len = MultiByteToWideChar(codepage, 0, src, srcLen, NULL, 0);
    if (srcLen != -1 && len > 0)
        len++;
    if (len == 0)
        return NULL;
    WCHAR* txt = (WCHAR*)malloc(len * sizeof(WCHAR));
#ifndef SAFE_ALLOC
    if (txt == NULL)
        SetLastError(ERROR_OUTOFMEMORY);
    else
    {
#endif // SAFE_ALLOC
        int res = MultiByteToWideChar(codepage, 0, src, srcLen, txt, len);
        if (srcLen != -1 && res > 0)
            res++;
        if (res > 0 && res <= len)
            txt[res - 1] = 0; // success, terminate the string with a null character
        else
        {
            DWORD err = GetLastError();
            free(txt);
            txt = NULL;
            SetLastError(err);
        }
#ifndef SAFE_ALLOC
    }
#endif // SAFE_ALLOC
    return txt;
}

WCHAR* DupStr(const WCHAR* txt)
{
    if (txt == NULL)
        return NULL;
    int len = lstrlenW(txt) + 1;
    WCHAR* ret = (WCHAR*)malloc(len * sizeof(WCHAR));
#ifndef SAFE_ALLOC
    if (ret == NULL)
        return NULL;
#endif // SAFE_ALLOC
    return (WCHAR*)memcpy(ret, txt, len * sizeof(WCHAR));
}

/*  LPTSTR FindString( // Return value: pointer to matched substring of text, or null pointer
                  LCID Locale, // locale identifier for CompareString
                  DWORD dwCmpFlags, // CompareString flags
                  LPCTSTR text, // text to be searched
                  int ltext, // number of TCHARs in text, or negative number if null terminated
                  LPCTSTR str, // string to look for
                  int lstr, // number of TCHARs in string, or negative number if null terminated
                  int *lsubstr // pointer to int that receives the number of TCHARs in the substring
                  )
{
  int i, j; // start/end index of substring being analyzed

  i = 0;
  // for i=0..end, compare text.SubString(i, end) with str
  do {
    switch (CompareString(Locale, dwCmpFlags, &text[i],
                          ltext < 0 ? ltext : ltext-i, str, lstr))
    {
      case 0:
        return 0;

      case CSTR_LESS_THAN:
        continue;
    }
    j = i;
    // if greater: for j=i..end, compare text.SubString(i, j) with str
    do {
      switch (CompareString(Locale, dwCmpFlags, &text[i],
                            j-i, str, lstr))
      {
        case 0:
          return 0;

        case CSTR_LESS_THAN:
          continue;

        case CSTR_EQUAL:
          if (lsubstr) *lsubstr = j-i;
          return (LPTSTR)&text[i];
      }
      // if greater: break to outer loop
      break;
    } while (ltext < 0 ? text[j++] : j++ < ltext);
  } while (ltext < 0 ? text[i++] : i++ < ltext);

  SetLastError(ERROR_SUCCESS);
  return 0;
} 



  int res;
  char buf[10];
  WCHAR *s1, *s2, *s3 = L"D:\\Á";
  res = ConvertU2A(s1 = L"", -1, buf, 10, TRUE);
  res = ConvertU2A(s1 = L"ahoj", 0, buf, 10, TRUE);
  res = ConvertU2A(s1 = L"ahoj", -1, buf, 5, TRUE);
  res = ConvertU2A(s1 = L"ahoj", -1, buf, 4, TRUE);
  res = ConvertU2A(s1 = L"ahoj", 4, buf, 5, TRUE);
  res = ConvertU2A(s1 = L"ahoj", 4, buf, 4, TRUE);
  res = ConvertU2A(s1 = L"ahoj", 4, buf, 3, TRUE);
  res = ConvertU2A(s1 = L"D:\\\x0061\x0308", -1, buf, 10, TRUE);  // L"D:\\\x00e4"
  res = ConvertU2A(s2 = L"D:\\á", -1, buf, 4);
  res = ConvertU2A(s1 = L"D:\\\xfb01-\x0061\x0308-\x00e4.txt", -1, buf, 10, TRUE);
  res = ConvertU2A(s2 = L"fi", -1, buf, 4);

  int fLen;
  WCHAR *f = FindString(LOCALE_USER_DEFAULT, 0, s1, -1, L"fi", -1, &fLen);
  f = FindString(LOCALE_USER_DEFAULT, 0, s1, -1, L"f", -1, &fLen);
  f = FindString(LOCALE_USER_DEFAULT, 0, s1, -1, L"\x00e4", -1, &fLen);
  f = FindString(LOCALE_USER_DEFAULT, 0, s1, -1, L"a", -1, &fLen);  // DOES NOT WORK !!!
  WCHAR *ss = wcsstr(s1, L"\x00e4");
/ *  
Read X:\ZUMPA\!\unicode\ch05.pdf - how should the Unicode search look like at all ???

Somewhere to swap + check over time + debug:
Some time ago I implemented a FindString function which in most cases takes only O(n) time (around 1.5*n CompareString
calls most of which return immediately). In the end I didn't use it because I wasn't sure whether the relevant statement
in the CompareString documentation can be relied on in a strict sense: "If the two strings are of different lengths,
they are compared up to the length of the shortest one. If they are equal to that point, then the return value will
indicate that the longer string is greater." More specifically, the function fails for TCHAR strings that are lexically
before any of their substrings (from the beginning). For example, when looking for "á" = {U+00E1}, the function will not
find the "a?" = {U+0061, U+0301} representation, if it sorts before "a" = {U+0061} in the specified locale. In other
words: The function assumes that CompareString(lcid, flags, string, m, string, n never returns CSTR_GREATER_THAN if
m <= n and the strings agree in the first m TCHARs.

It might be useful to use the "StringInfo Class", which can break down a string
into displayable characters (sequences of WCHARs corresponding to one displayed character).

* /

  WCHAR wbuf[50];
  WCHAR wbuf2[50];
  res = FoldString(MAP_FOLDCZONE | MAP_EXPAND_LIGATURES, s1, -1, wbuf, 50);
  res = FoldString(MAP_FOLDCZONE | MAP_PRECOMPOSED, wbuf, -1, wbuf2, 50);

  HANDLE file = CreateFile(s1, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file != INVALID_HANDLE_VALUE) CloseHandle(file);

  res = CompareString(LOCALE_USER_DEFAULT, 0, s1, -1, s2, -1);

  res = ConvertA2U("Âëŕäčěčđ", -1, wbuf, 10, 1251);
  res = ConvertA2U("ahoj", 0, wbuf, 10);
  res = ConvertA2U("ahoj", -1, wbuf, 5);
  res = ConvertA2U("ahoj", -1, wbuf, 4);
  res = ConvertA2U("ahoj", 4, wbuf, 5);
  res = ConvertA2U("ahoj", 4, wbuf, 4);
  res = ConvertA2U("ahoj", 4, wbuf, 3);


  res = CompareString(LOCALE_USER_DEFAULT, 0, s2, -1, s3, -1);

  {
    char *res;
    res = ConvertU2A(L"", -1, TRUE);
    res = ConvertU2A(L"ahoj", 0, TRUE);
    res = ConvertU2A(L"ahoj", 2, TRUE);
    res = ConvertU2A(L"ahoj", -1, TRUE);
    res = ConvertU2A(L"D:\\\x0061\x0308", -1, TRUE);
    res = ConvertU2A(L"D:\\\x0061\x0308", -1);
    res = ConvertU2A(L"D:\\\xfb01-\x0061\x0308-\x00e4.txt", -1, TRUE);

    WCHAR *wres;
    wres = ConvertA2U("Âëŕäčěčđ", -1, 1251);
    wres = ConvertA2U("", -1);
    wres = ConvertA2U("ahoj čěšťíňká", 0);
    wres = ConvertA2U("ahoj čěšťíňká", 2);
    wres = ConvertA2U("ahoj čěšťíňká", -1);
  }*/
