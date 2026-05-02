// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

BYTE LowerCase[256];

#ifndef INSIDE_SETUP
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
#endif //INSIDE_SETUP

//************************************************************************************
//
// MoveFileOnReboot
//

char* mystrstr(char* a, char* b)
{
    int len_of_a = lstrlen(a) - lstrlen(b);
    while (*a && len_of_a >= 0)
    {
        char *t = a, *u = b;
        while (*t && *t == *u)
        {
            t++;
            u++;
        }
        if (!*u)
            return a;
        a++;
        len_of_a--;
    }
    return NULL;
}

void* mini_memcpy(void* out, const void* in, int len)
{
    char* c_out = (char*)out;
    char* c_in = (char*)in;
    while (len-- > 0)
    {
        *c_out++ = *c_in++;
    }
    return out;
}

BOOL GetFolderPath(int nFolder, LPTSTR pszPath)
{
    HRESULT ret;
    *pszPath = 0;
    ret = SHGetFolderPath(NULL, nFolder, NULL, 0, pszPath);
    return (ret == S_OK);
}

/*
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;
BOOL Is64BitWindows()
{
  BOOL bIsWow64 = FALSE;
  #ifdef _WIN64
  return TRUE;  // 64-bit programs run only on Win64
  #endif
  //IsWow64Process is not available on all supported versions of Windows.
  //Use GetModuleHandle to get a handle to the DLL that contains the function
  //and GetProcAddress to get a pointer to the function if available.
  fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
  if(NULL != fnIsWow64Process)
  {
    if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
    {
      //handle error
    }
  }
  return bIsWow64;
}
*/

void InitUtils()
{
    int i;
    for (i = 0; i < 256; i++)
        LowerCase[i] = (char)CharLower((LPTSTR)(DWORD_PTR)i);
}
