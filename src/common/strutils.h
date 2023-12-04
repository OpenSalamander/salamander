// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// makro SAFE_ALLOC odstranuje kod, ve kterem se testuje, jestli se povedla alokace pameti (viz allochan.*)

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

// prevod ANSI multibytoveho stringu na Unicodovy string (UTF-16); 'src' je ANSI string;
// 'srcLen' je delka ANSI stringu (bez zakoncujici nuly; pri zadani -1 se delka urci
// podle zakoncujici nuly); 'bufSize' (musi byt vetsi nez 0) je velikost ciloveho bufferu
// 'buf' pro Unicodovy string; 'codepage' je kodova stranka ANSI stringu;
// vraci pocet znaku zapsanych do 'buf' (vcetne zakoncujici nuly); pri chybe vraci nulu
// (detaily viz GetLastError()); vzdy zajisti nulou zakonceny 'buf' (i pri chybe);
// je-li 'buf' maly, vraci funkce nulu, ale v 'buf' je prevedena aspon cast stringu
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

// drzi ukazatel na alokovanou pamet, postara se o jeji uvolneni pri prepisu jinym ukazatelem na
// alokovanou pamet a pri sve destrukci
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

// drzi alokovany string, postara se o uvolneni pri prepisu jinym stringem (tez alokovanym)
// a pri sve destrukci
typedef CAllocP<WCHAR> CStrP;
