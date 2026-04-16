// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// The SAFE_ALLOC macro removes the code that checks whether memory allocation succeeded (see allochan.*)

// Converts a Unicode (UTF-16) string to an ANSI multibyte string; 'src' is the Unicode string;
// 'srcLen' is the length of the Unicode string (excluding the terminating null; if -1 is passed,
// the length is determined from the terminating null); 'bufSize' (must be greater than 0) is the size of the destination buffer
// 'buf' for the ANSI string; if 'compositeCheck' is TRUE, the WC_COMPOSITECHECK flag is used
// (see MSDN); it must not be used for file names (NTFS distinguishes between names written as
// precomposed and composite, i.e. it does not normalize names); 'codepage' is the code page of the
// ANSI string; returns the number of characters written to 'buf' (including the terminating null); on error
// returns 0 (see GetLastError()); always ensures 'buf' is null-terminated (even on error);
// if 'buf' is too small, the function returns 0, but at least part of the string is converted into 'buf'
int ConvertU2A(const WCHAR* src, int srcLen, char* buf, int bufSize,
               BOOL compositeCheck = FALSE, UINT codepage = CP_ACP);

// Converts a Unicode (UTF-16) string to an allocated ANSI multibyte string (the caller is
// responsible for freeing the string); 'src' is the Unicode string; 'srcLen' is the length of the Unicode
// string (excluding the terminating null; if -1 is passed, the length is determined from the terminating null);
// if 'compositeCheck' is TRUE, the WC_COMPOSITECHECK flag is used (see MSDN); it must not be used
// for file names (NTFS distinguishes between names written as precomposed and composite, i.e.
// it does not normalize names); 'codepage' is the code page of the ANSI string; returns the allocated
// ANSI string; on error returns NULL (see GetLastError())
char* ConvertAllocU2A(const WCHAR* src, int srcLen, BOOL compositeCheck = FALSE, UINT codepage = CP_ACP);

// Converts an ANSI multibyte string to a Unicode (UTF-16) string; 'src' is the ANSI string;
// 'srcLen' is the length of the ANSI string (excluding the terminating null; if -1 is passed,
// the length is determined from the terminating null); 'bufSize' (must be greater than 0) is the size of the destination buffer
// 'buf' for the Unicode string; 'codepage' is the code page of the ANSI string;
// returns the number of characters written to 'buf' (including the terminating null); on error returns 0
// (see GetLastError()); always ensures 'buf' is null-terminated (even on error);
// if 'buf' is too small, the function returns 0, but at least part of the string is converted into 'buf'
int ConvertA2U(const char* src, int srcLen, WCHAR* buf, int bufSizeInChars,
               UINT codepage = CP_ACP);

// Converts an ANSI multibyte string to an allocated Unicode (UTF-16) string (the caller is responsible for freeing
// the string); 'src' is the ANSI string; 'srcLen' is the length of the ANSI
// string (excluding the terminating null; if -1 is passed, the length is determined from the terminating null);
// 'codepage' is the code page of the ANSI string; returns the allocated Unicode string; on
// error returns NULL (see GetLastError())
WCHAR* ConvertAllocA2U(const char* src, int srcLen, UINT codepage = CP_ACP);

// Copies 'txt' into a newly allocated string; returns NULL on out of memory (only possible when
// allochan.* is not used) or if 'txt'==NULL
WCHAR* DupStr(const WCHAR* txt);

// Holds a pointer to allocated memory and frees it when overwritten by another pointer to allocated memory
// and when destroyed
template <class PTR_TYPE>
class CAllocP
{
public:
    PTR_TYPE* Ptr;

public:
    CAllocP(PTR_TYPE* ptr = NULL) { Ptr = ptr; }
    ~CAllocP()
    {
        if (Ptr != NULL)
            free(Ptr);
    }

    PTR_TYPE* GetAndClear()
    {
        PTR_TYPE* p = Ptr;
        Ptr = NULL;
        return p;
    }

    operator PTR_TYPE*() { return Ptr; }
    PTR_TYPE* operator=(PTR_TYPE* p)
    {
        if (Ptr != NULL)
            free(Ptr);
        return Ptr = p;
    }
};

// Holds an allocated string and frees it when overwritten by another allocated string
// and when destroyed
typedef CAllocP<WCHAR> CStrP;
