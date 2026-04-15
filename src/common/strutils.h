// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// The SAFE_ALLOC macro removes the code that checks whether memory allocation succeeded (see allochan.*)

// prevod Unicodoveho stringu (UTF-16) na ANSI multibytovy string; 'src' je Unicodovy string;
// 'srcLen' je delka Unicodoveho stringu (bez zakoncujici nuly; pri zadani -1 se delka urci
// podle zakoncujici nuly); 'bufSize' (musi byt vetsi nez 0) je velikost ciloveho bufferu
// 'buf' pro ANSI string; je-li 'compositeCheck' TRUE, pouziva flag WC_COMPOSITECHECK
// (viz MSDN), nesmi se pouzit pro jmena souboru (NTFS rozlisuje jmena zapsana jako
// precomposed a composite, aneb neprovadi normalizaci jmen); 'codepage' je kodova stranka
// ANSI stringu; vraci pocet znaku zapsanych do 'buf' (vcetne zakoncujici nuly); pri chybe
// vraci nulu (detaily viz GetLastError()); vzdy zajisti nulou zakonceny 'buf' (i pri chybe);
// je-li 'buf' maly, vraci funkce nulu, ale v 'buf' je prevedena aspon cast stringu
int ConvertU2A(const WCHAR* src, int srcLen, char* buf, int bufSize,
               BOOL compositeCheck = FALSE, UINT codepage = CP_ACP);

// prevod Unicodoveho stringu (UTF-16) na alokovany ANSI multibytovy string (volajici je
// odpovedny za dealokaci stringu); 'src' je Unicodovy string; 'srcLen' je delka Unicodoveho
// stringu (bez zakoncujici nuly; pri zadani -1 se delka urci podle zakoncujici nuly);
// je-li 'compositeCheck' TRUE, pouziva flag WC_COMPOSITECHECK (viz MSDN), nesmi se pouzit
// pro jmena souboru (NTFS rozlisuje jmena zapsana jako precomposed a composite, aneb
// neprovadi normalizaci jmen); 'codepage' je kodova stranka ANSI stringu; vraci alokovany
// ANSI string; pri chybe vraci NULL (detaily viz GetLastError())
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

// prevod ANSI multibytoveho stringu na alokovany (volajici je odpovedny za dealokaci
// stringu) Unicodovy string (UTF-16); 'src' je ANSI string; 'srcLen' je delka ANSI
// stringu (bez zakoncujici nuly; pri zadani -1 se delka urci podle zakoncujici nuly);
// 'codepage' je kodova stranka ANSI stringu; vraci alokovany Unicodovy string; pri
// chybe vraci NULL (detaily viz GetLastError())
WCHAR* ConvertAllocA2U(const char* src, int srcLen, UINT codepage = CP_ACP);

// nakopiruje string 'txt' do nove naalokovaneho stringu, NULL = malo pameti (hrozi jen pokud
// se nepouziva allochan.*) nebo 'txt'==NULL
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
