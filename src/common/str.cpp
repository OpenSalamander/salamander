// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h> // I need LPCOLORMAP

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#ifndef STR_DISABLE

#ifdef INSIDE_SPL // for use in plugins
#include "spl_base.h"
#include "dbg.h"
#else                     //INSIDE_SPL
#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression
#include "trace.h"
#include "messages.h"
#include "handles.h"
#endif //INSIDE_SPL

#include "str.h"

#ifndef INSIDE_SPL // except for use in plugins

// The order here is important.
// Section names must be 8 characters or less.
// The sections with the same name before the $
// are merged into one section. The order that
// they are merged is determined by sorting
// the characters after the $.
// i_str and i_str_end are used to set
// boundaries so we can find the real functions
// that we need to call for initialization.

#pragma warning(disable : 4075) // we want to define the order of module initialization

typedef void(__cdecl* _PVFV)(void);

#pragma section(".i_str$a", read)
__declspec(allocate(".i_str$a")) const _PVFV i_str = (_PVFV)1; // at the beginning of the section, we assign the variable i_str to .i_str

#pragma section(".i_str$z", read)
__declspec(allocate(".i_str$z")) const _PVFV i_str_end = (_PVFV)1; // a na konec sekce .i_str si dame promennou i_str_end

void Initialize__Str()
{
    const _PVFV* x = &i_str;
    for (++x; x < &i_str_end; ++x)
        if (*x != NULL)
            (*x)();
}

#pragma init_seg(".i_str$m")

#endif //INSIDE_SPL

BYTE LowerCase[256];
BYTE UpperCase[256];

void InitializeCase();

class C__STR_module // automatic module initialization
{
public:
    C__STR_module() { InitializeCase(); }
} __STR_module;

// ****************************************************************************

char* DupStr(const char* txt)
{
    if (txt == NULL)
        return NULL;
    int l = (int)strlen(txt);
    char* s = (char*)malloc(l + 1);
    if (s == NULL)
    {
        TRACE_E("Low memory.");
        return NULL;
    }
    memcpy(s, txt, l + 1);
    return s;
}

char* DupStrEx(const char* str, BOOL& err)
{
    char* s = DupStr(str);
    if (str != NULL && s == NULL)
        err = TRUE;
    return s;
}

// ****************************************************************************

char* StrNCat(char* dst, const char* src, int dstSize)
{
    int i = lstrlenA(dst);
    lstrcpynA(dst + i, src, dstSize - i);
    return dst;
}

// ****************************************************************************

void InitializeCase()
{
    int i;
    for (i = 0; i < 256; i++)
        LowerCase[i] = (char)(UINT_PTR)CharLowerA((LPSTR)(UINT_PTR)i);
    for (i = 0; i < 256; i++)
        UpperCase[i] = (char)(UINT_PTR)CharUpperA((LPSTR)(UINT_PTR)i);
}

//
//*****************************************************************************

int StrICpy(char* dest, const char* src)
{
    const char* s = src;
    while (*src != 0)
        *dest++ = LowerCase[*src++];
    *dest = 0;
    return (int)(src - s); // return the number of copied characters
}

//
//*****************************************************************************

#ifdef _WIN64
// Even in VC11 MS did not provide x64 ASM versions of string operations, so for now we also stay in C++

// original function
int StrICmp(const char* s1, const char* s2)
{
    int res;
    while (1)
    {
        res = (unsigned)LowerCase[*s1] - (unsigned)LowerCase[*s2++];
        if (res != 0)
            return (res < 0) ? -1 : 1; // < a >
        if (*s1++ == 0)
            return 0; // ==
    }
}

#else  // _WIN64

int StrICmp(const char* s1, const char* s2)
{
    const BYTE* table = LowerCase;
    __asm {
        // load up arguments
    mov     esi,[s2] // esi = s2
    mov     edi,[s1] // edi = s1
    mov     edx,table // edx = LowerCase

    mov     eax,255 // fall into loop
    xor     ebx,ebx

    align   4

            // compare
chk_null:
    or      al,al // not that if al == 0, then eax == 0!
    jz      short done

    mov     al,[esi] // al = *s2
    inc     esi
    mov     bl,[edi] // bl = *s1
    inc     edi

    cmp     al,bl // first try case-sensitive comparison
    je      short chk_null // match

    mov     al,[edx + eax] // convert *s2 char to lower case
    mov     bl,[edx + ebx] // convert *s1 char to lower case

    cmp     bl,al
    je      chk_null
                // s1 < s2       s1 > s2
    sbb     eax,eax // AL=-1, CY=1   AL=0, CY=0
    sbb     eax,-1          // AL=-1         AL=1

done:

    }

    // the return value is in eax to bypass the compiler warning
    // For functions without a return value, we will do the following monkey business
    BOOL retVal;
    __asm {mov retVal, eax}
    return retVal;
}
#endif // _WIN64

//
//*****************************************************************************

/*  // original function
// caution, there is a bug StrNICmp("a", "aa", 2) returns 0
int StrNICmp(const char *s1, const char *s2, int n)
{
  int res;
  while (n--)
  {
    res = (unsigned)LowerCase[*s1++] - (unsigned)LowerCase[*s2++];
    if (res != 0) return (res < 0) ? -1 : 1;               // < and >
    if (*s1 == 0) return 0;                                // ==
  }
  return 0;
}*/

#ifdef _WIN64
// Even in VC11 MS did not provide x64 ASM versions of string operations, so for now we also stay in C++

// fixed version
int StrNICmp(const char* s1, const char* s2, int n)
{
    int res;
    while (n--)
    {
        res = (unsigned)LowerCase[*s1] - (unsigned)LowerCase[*s2++];
        if (res != 0)
            return (res < 0) ? -1 : 1; // < a >
        if (*s1++ == 0)
            return 0; // ==
    }
    return 0;
}

#else // _WIN64

int StrNICmp(const char* s1, const char* s2, int n)
{
    const BYTE* table = LowerCase;
    __asm {
        // load up arguments
    mov     ecx,[n] // ecx = byte count
    or      ecx,ecx        
    jz      toend // if count = 0, we are done
                           
    mov     esi,s1 // esi = s1
    mov     edi,s2 // edi = s2
    mov     edx,table // edx = LowerCase
                           
    xor     eax,eax        
    xor     ebx,ebx        
                           
    align   4              
                           
lupe:                      
    mov     al,[esi] // al = *s1
                           
    or      eax,eax // see if *s1 is null
                           
    mov     bl,[edi] // bl = *s2
                           
    jz      short eject // jump if *s1 is null

    or      ebx,ebx // see if *s2 is null
    jz      short eject // jump if so

    inc     esi
    inc     edi

    mov     al,[edx + eax] // convert *s1 char to lower case
    mov     bl,[edx + ebx] // convert *s2 char to lower case

    cmp     eax,ebx // now equal?
    jne     short differ

    dec     ecx
    jnz     short lupe

eject:
    xor     ecx,ecx
    cmp     eax,ebx
    je      short toend

differ:
    mov     ecx,-1 // return -1 if s1 < s2
    jb      short toend

    neg     ecx // return 1

toend:
    mov     eax,ecx // move return value to eax
    }
    // the return value is in eax to bypass the compiler warning
    // For functions without a return value, we will do the following monkey business
    BOOL retVal;
    __asm {mov retVal, eax}
    return retVal;
}

#endif // _WIN64

//
//*****************************************************************************

#ifdef _WIN64
// Even in VC11 MS did not provide x64 ASM versions of string operations, so for now we also stay in C++
int MemICmp(const void* buf1, const void* buf2, int n)
{
    int ret = _memicmp(buf1, buf2, n);
    // we normalize the return value according to our specification
    if (ret == 0)
        return 0;
    return (ret < 0) ? -1 : 1;
}
#else  // _WIN64

int MemICmp(const void* buf1, const void* buf2, int n)
{
    const BYTE* table = LowerCase;
    __asm {
        // load up arguments
    mov     ecx,[n] // ecx = byte count
    or      ecx,ecx        
    jz      toend // if count = 0, we are done
                           
    mov     esi,buf1 // esi = buf1
    mov     edi,buf2 // edi = buf2
    mov     edx,table // edx = LowerCase
                           
    xor     eax,eax        
    xor     ebx,ebx        
                           
    align   4              
                           
lupe:                      
    mov     al,[esi] // al = *buf1
    inc     esi
    mov     bl,[edi] // bl = *buf2
    inc     edi

    cmp     al,bl // test for equality BEFORE converting case
    je      short dolupe

    mov     al,[edx + eax] // convert *buf1 char to lower case
    mov     bl,[edx + ebx] // convert *buf2 char to lower case

    cmp     al,bl // now equal?
    jne     short differ

dolupe:
    dec     ecx
    jnz     lupe

    jmp     short toend

differ:
    mov     ecx,-1 // return -1 if buf1 < buf2
    jb      short toend

    neg     ecx // return 1

toend:
    mov     eax,ecx // move return value to eax
    }
    // the return value is in eax to bypass the compiler warning
    // For functions without a return value, we will do the following monkey business
    BOOL retVal;
    __asm {mov retVal, eax}
    return retVal;
}
#endif // _WIN64

//
//*****************************************************************************

#ifdef _WIN64
// Even in VC11 MS did not provide x64 ASM versions of string operations, so for now we also stay in C++

// original function
int StrICmpEx(const char* s1, int l1, const char* s2, int l2)
{
    int res, l = (l1 < l2) ? l1 : l2;
    while (l--)
    {
        res = (unsigned)LowerCase[*s1++] - (unsigned)LowerCase[*s2++];
        if (res != 0)
            return (res < 0) ? -1 : 1; // < a >
    }
    if (l1 != l2)
        return (l1 < l2) ? -1 : 1; // < a >
    else
        return 0;
}

/*  // principle of asm function
int StrICmpEx(const char *s1, int l1, const char *s2, int l2)
{
  int l = (l1 < l2) ? l1 : l2;

  if (l > 0)
  {
    int res = MemICmp(s1, s2, l);
    if (res != 0) return (res < 0) ? -1 : 1;    // < and >
  }

  if (l1 != l2) return (l1 < l2) ? -1 : 1;
  else return 0;
}*/
#else  // _WIN64

int StrICmpEx(const char* s1, int l1, const char* s2, int l2)
{
    int l = (l1 < l2) ? l1 : l2;

    if (l > 0)
    {
        // MemICmp
        const BYTE* table = LowerCase;
        __asm {
            // load up arguments
      mov     ecx,[l] // ecx = byte count
      or      ecx,ecx        
      jz      toend // if count = 0, we are done
                           
      mov     esi,s1 // esi = buf1
      mov     edi,s2 // edi = buf2
      mov     edx,table // edx = LowerCase
                           
      xor     eax,eax        
      xor     ebx,ebx        
                           
      align   4              
                           
  lupe:                      
      mov     al,[esi] // al = *buf1
      inc     esi
      mov     bl,[edi] // bl = *buf2
      inc     edi

      cmp     al,bl // test for equality BEFORE converting case
      je      short dolupe

      mov     al,[edx + eax] // convert *buf1 char to lower case
      mov     bl,[edx + ebx] // convert *buf2 char to lower case

      cmp     al,bl // now equal?
      jne     short differ

  dolupe:
      dec     ecx
      jnz     lupe

      jmp     short toend

  differ:
      mov     ecx,-1 // return -1 if buf1 < buf2
      jb      short toend

      neg     ecx // return 1

  toend:
      mov     eax,ecx // move return value to eax
        }
        // the return value is in eax to bypass the compiler warning
        // For functions without a return value, we will do the following monkey business
        BOOL retVal;
        __asm {mov retVal, eax}
        if (retVal != 0) return retVal;
    }

    if (l1 != l2)
        return (l1 < l2) ? -1 : 1;
    else
        return 0;
}
#endif // _WIN64

//
//*****************************************************************************

/*  // original function
int StrCmpEx(const char *s1, int l1, const char *s2, int l2)
{
  int res, l = (l1 < l2) ? l1 : l2;
  while (l--)
  {
    res = (unsigned)(*s1++) - (unsigned)(*s2++);
    if (res != 0) return (res < 0) ? -1 : 1;    // < a >
  }
  if (l1 != l2) return (l1 < l2) ? -1 : 1;    // < a >
  else return 0;
}*/

// faster version
int StrCmpEx(const char* s1, int l1, const char* s2, int l2)
{
    int l = (l1 < l2) ? l1 : l2;

    if (l > 0)
    {
        int res = memcmp(s1, s2, l);
        if (res != 0)
            return (res < 0) ? -1 : 1; // < a >
    }

    if (l1 != l2)
        return (l1 < l2) ? -1 : 1; // < a >
    else
        return 0;
}

//
//*****************************************************************************

const char* StrIStr(const char* txt, const char* pattern)
{
    if (txt == NULL || pattern == NULL)
        return NULL;

    const char* s = txt;
    int len = (int)strlen(pattern);
    int txtLen = (int)strlen(txt);
    while (txtLen >= len)
    {
        if (StrNICmp(s, pattern, len) == 0)
            return s;
        s++;
        txtLen--;
    }
    return NULL;
}

const char* StrIStr(const char* txtStart, const char* txtEnd,
                    const char* patternStart, const char* patternEnd)
{
    if (txtStart == NULL || patternStart == NULL)
        return NULL;

    const char* s = txtStart;
    int len = (int)(patternEnd - patternStart);
    int txtLen = (int)(txtEnd - txtStart);
    while (txtLen >= len)
    {
        if (StrNICmp(s, patternStart, len) == 0)
            return s;
        s++;
        txtLen--;
    }
    return NULL;
}

//
//*****************************************************************************

/*  int StrLen(const char *str)
{
  const char *s = str;
  while (1)
  {
    if ((((*(DWORD *)s) & 0xF0F0F0F0) - 0x10101010) & 0x8F0F0F0F)
    {                            // character < 16 is there
      if (*s != 0) s++;
      else break;
      if (*s != 0) s++;
      else break;
      if (*s != 0) s++;
      else break;
      if (*s != 0) s++;
      else break;
    }
    else s += 4;
  }
  return s - str;
}*/

/*  //*****************************************************************************
//
// Tabulky pro prevod kodu Kamenickych do MS Windows
//

BYTE KodKamenickych[CONVERT_TAB_CHARS] =
{
0x84,0x8e,
0xa0,0x8f,
0x87,0x80,
0x83,0x85,
0x82,0x90,
0x88,0x89,
0xa1,0x8b,
0x8d,0x8a,
0x8c,0x9c,
0xa4,0xa5,
0xa2,0x95,
0x94,0x99,
0x93,0xa7,
0xaa,0xab,
0xa9,0x9e,
0xa8,0x9b,
0x9f,0x86,
0xa3,0x97,
0x96,0xa6,
0x81,0x9a,
0x98,0x9d,
0x91,0x92,
}; //done

BYTE KodWindows[CONVERT_TAB_CHARS] =
{
0xe4,0xc4,
0xe1,0xc1,
0xe8,0xc8,
0xef,0xcf,
0xe9,0xc9,
0xec,0xcc,
0xed,0xcd,
0xe5,0xc5,
0xbe,0xbc,
0xf2,0xd2,
0xf3,0xd3,
0xf6,0xd6,
0xf4,0xd4,
0xe0,0xc0,
0xf8,0xd8,
0x9a,0x8a,
0x9d,0x8d,
0xfa,0xda,
0xf9,0xd9,
0xfc,0xdc,
0xfd,0xdd,
0x9e,0x8e
}; //done

CConvertTab::CConvertTab()
{
  WORD i;
  for(i = 0; i < CONVERT_TAB_MAX_CHARS; i++)
    Data[i] = (BYTE)i;
  for(i = 0; i < CONVERT_TAB_CHARS; i++)
    Data[KodKamenickych[i]] = KodWindows[i];
}

void
CConvertTab::Convert(char *str)
{
  char *ptr = str;
  while(*ptr != 0) *ptr++ = Data[*ptr];
}

CConvertTab ConvertTab;
*/

#endif // STR_DISABLE
