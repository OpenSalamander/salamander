// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "..\undelete.rh2"

#include "miscstr.h"
#include "os.h"
#include "volume.h"
#include "snapshot.h"
#include "volenum.h"

#include "../dialogs.h"
#include "../undelete.h"

// ****************************************************************************
//
// LoadStr() - helper function for reading strings from resources
//

class C__StrCriticalSection
{
public:
    CRITICAL_SECTION cs;
    C__StrCriticalSection() { NOHANDLES(InitializeCriticalSection(&cs)); }
    ~C__StrCriticalSection() { NOHANDLES(DeleteCriticalSection(&cs)); }
};

// ensure critical section is initialized in the beginning
#pragma warning(disable : 4073)
#pragma init_seg(lib)
C__StrCriticalSection __StrCriticalSection;

const unsigned short String<char>::STRBUFSIZE = 10240;
char* String<char>::StringBuffer = NULL;
char* String<char>::StrAct = String<char>::StringBuffer;

const unsigned short String<wchar_t>::STRBUFSIZE = 10240;
wchar_t* String<wchar_t>::StringBuffer = NULL;
wchar_t* String<wchar_t>::StrAct = String<wchar_t>::StringBuffer;

extern HINSTANCE DLLInstance;

template <>
char* String<char>::LoadStr(int resID)
{
    HANDLES(EnterCriticalSection(&__StrCriticalSection.cs));

    char* ret;
    static char errorBuff[] = "ERROR LOADING STRING - INSUFFICIENT MEMORY";
    if (!StringBuffer)
        ret = errorBuff;
    else
    {
        if (STRBUFSIZE - (StrAct - StringBuffer) < 200)
            StrAct = StringBuffer;

        HINSTANCE hInstance = HLanguage;

#ifdef _DEBUG
        // ensure it is not called before handle is initialized
        if (hInstance == NULL)
            TRACE_E("LoadStr: hInstance == NULL");
#endif // _DEBUG

    RELOAD:
        int size = LoadStringA(hInstance, resID, StrAct, STRBUFSIZE - (int)(StrAct - StringBuffer));
        // size contains number of copied characters without terminator
        //    DWORD error = GetLastError();
        if (size != 0 /* || error == NO_ERROR*/) // error is NO_ERROR even if string doesn't exist, we cannot use it
        {
            if (STRBUFSIZE - (StrAct - StringBuffer) == size + 1 && StrAct > StringBuffer)
            {
                // if string was placed exactly on the buffer end, it could be incomplete
                // we will read it again to the beginning of buffer
                StrAct = StringBuffer;
                goto RELOAD;
            }
            else
            {
                ret = StrAct;
                StrAct += size + 1;
            }
        }
        else
        {
            TRACE_E("Error in LoadStr(" << resID << ")." /*"): " << GetErrorText(error)*/);
            static char errorBuff2[] = "ERROR LOADING STRING";
            ret = errorBuff2;
        }
    }

    HANDLES(LeaveCriticalSection(&__StrCriticalSection.cs));

    return ret;
}

template <>
wchar_t* String<wchar_t>::LoadStr(int resID)
{
    HANDLES(EnterCriticalSection(&__StrCriticalSection.cs));

    wchar_t* ret;
    static wchar_t errorBuff[] = L"ERROR LOADING STRING - INSUFFICIENT MEMORY";
    if (!StringBuffer)
        ret = errorBuff;
    else
    {
        if (STRBUFSIZE - (StrAct - StringBuffer) < 200)
            StrAct = StringBuffer;

        HINSTANCE hInstance = HLanguage;

#ifdef _DEBUG
        // ensure it is not called before handle is initialized
        if (hInstance == NULL)
            TRACE_E("LoadStr: hInstance == NULL");
#endif // _DEBUG

    RELOAD:
        int size = LoadStringW(hInstance, resID, StrAct, STRBUFSIZE - (int)(StrAct - StringBuffer));
        // size contains number of copied characters without terminator
        //    DWORD error = GetLastError();
        if (size != 0 /* || error == NO_ERROR*/) // error is NO_ERROR even if string doesn't exist, we cannot use it
        {
            if ((DWORD)(STRBUFSIZE - (StrAct - StringBuffer)) == (DWORD)(size + 1) && StrAct > StringBuffer)
            {
                // if string was placed exactly on the buffer end, it could be incomplete
                // we will read it again to the beginning of buffer
                StrAct = StringBuffer;
                goto RELOAD;
            }
            else
            {
                ret = StrAct;
                StrAct += size + 1;
            }
        }
        else
        {
            TRACE_E("Error in LoadStr(" << resID << ")." /*"): " << GetErrorText(error)*/);
            static wchar_t errorBuff2[] = L"ERROR LOADING STRING";
            ret = errorBuff2;
        }
    }

    HANDLES(LeaveCriticalSection(&__StrCriticalSection.cs));

    return ret;
}

template <>
void String<char>::VSPrintF(char* buffer, const char* pattern, va_list& marker)
{
    vsprintf(buffer, pattern, marker);
}

template <>
int String<char>::StrCmp(const char* string1, const char* string2)
{
    return strcmp(string1, string2);
}

template <>
int String<char>::StrICmp(const char* string1, const char* string2)
{
    return _stricmp(string1, string2);
}

template <>
size_t String<char>::StrLen(const char* text)
{
    return strlen(text);
}

template <>
char String<char>::ToUpper(char c)
{
    return toupper(c);
}

template <>
char* String<char>::StrCpy(char* text1, const char* text2)
{
    return strcpy(text1, text2);
}

template <>
char* String<char>::StrCat(char* text1, const char* text2)
{
    return strcat(text1, text2);
}

template <>
errno_t String<char>::StrCat_s(char* strDestination, size_t numberOfElements, const char* strSource)
{
    return strcat_s(strDestination, numberOfElements, strSource);
}

template <>
char* String<char>::AddNumberSuffix(char* filename, int n)
{
    char* ext = strrchr(filename, '.'); // ".cvspass" is extension in Windows
    if (ext != NULL && (ext - filename) != ((int)strlen(filename) - 4))
        ext = NULL;

    char temp[MAX_PATH + 50];
    if (ext == NULL)
    {
        sprintf(temp, "%s (%d)", filename, n++);
    }
    else
    {
        strncpy(temp, filename, ext - filename);
        sprintf(temp + (ext - filename), " (%d)%s", n++, ext);
    }

    return NewStr(temp);
}

template <>
char* String<char>::CopyFromASCII(char* dest, const char* src, unsigned long srclen, unsigned long destlen)
{
    if (destlen)
        dest[0] = 0;
    if (srclen)
    {
        if (destlen > srclen)
        {
            strncpy(dest, src, srclen);
            dest[srclen] = 0;
        }
        else
            TRACE_E("CopyFromASCII error: small dest buffer");
    }
    return dest;
}

template <>
char* String<char>::NewFromASCII(const char* src)
{
    return NewStr(src);
}

template <>
char* String<char>::NewFromUnicode(const wchar_t* src, unsigned long srclen)
{
    int destlen = WideCharToMultiByte(CP_ACP, 0, src, srclen, NULL, 0, NULL, NULL);
    char* dest = new char[destlen + 1];
    if (dest)
        return CopyFromUnicode(dest, src, srclen, destlen + 1);
    else
        return NULL;
}

template <>
char* String<char>::CopyFromUnicode(char* dest, const wchar_t* src, unsigned long srclen, unsigned long destlen)
{
    if (destlen)
        dest[0] = 0;
    if (srclen)
    {
        if (destlen > srclen)
        {
            if (WideCharToMultiByte(CP_ACP, 0, src, srclen, dest, destlen, NULL, NULL) == 0)
            {
                DWORD err = GetLastError();
                TRACE_E("WideCharToMultiByte error: " << err);
            }
            dest[srclen] = 0;
        }
        else
            TRACE_E("CopyFromUnicode error: small dest buffer");
    }
    return dest;
}

template <>
wchar_t* String<char>::CopyToUnicode(wchar_t* dest, const char* src, unsigned long srclen, unsigned long destlen)
{
    if (destlen)
        dest[0] = 0;
    if (srclen)
    {
        if (destlen > srclen)
        {
            if (MultiByteToWideChar(CP_ACP, 0, src, srclen, dest, destlen) == 0)
            {
                DWORD err = GetLastError();
                TRACE_E("MultiByteToWideChar error: " << err);
            }
            dest[srclen] = 0;
        }
        else
            TRACE_E("CopyToUnicode error: small dest buffer");
    }
    return dest;
}

// ****************************************************************************
//
//  Error message functions
//

extern HWND HProgressDlg;

HWND GetParentHWND()
{
    HWND hParent = SalamanderGeneral->GetMsgBoxParent();
    if (HProgressDlg != NULL)
        hParent = HProgressDlg;
    return hParent;
}

template <>
BOOL String<char>::SysError(int title, int error, ...)
{
    int lastErr = GetLastError();
    CALL_STACK_MESSAGE3("SysError(%d, %d, ...)", title, error);
    char buf[1024];
    *buf = 0;
    va_list arglist;
    va_start(arglist, error);
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    if (lastErr != ERROR_SUCCESS)
    {
        strcat(buf, " ");
        int l = (int)strlen(buf);
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    HWND hParent = GetParentHWND();
    SalamanderGeneral->SalMessageBox(hParent, buf, LoadStr(title), MB_OK | MB_ICONERROR);
    return FALSE;
}

template <>
BOOL String<char>::Error(int title, int error, ...)
{
    int lastErr = GetLastError();
    CALL_STACK_MESSAGE3("Error(%d, %d, ...)", title, error);
    char buf[1024];
    va_list arglist;
    va_start(arglist, error);
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    HWND hParent = GetParentHWND();
    SalamanderGeneral->SalMessageBox(hParent, buf, LoadStr(title), MB_OK | MB_ICONERROR);
    return FALSE;
}

template <>
int String<char>::PartialRestore(int checkBoxText, BOOL* checkBoxValue, int title, int text, ...)
{
    CALL_STACK_MESSAGE3("PartialRestore(%d, %d, ...)", title, text);
    char buf[1024];
    va_list arglist;
    va_start(arglist, text);
    vsprintf(buf, LoadStr(text), arglist);
    va_end(arglist);
    HWND hParent = GetParentHWND();

    MSGBOXEX_PARAMS mbep;
    memset(&mbep, 0, sizeof(mbep));
    mbep.HParent = hParent;
    mbep.Text = buf;
    mbep.Caption = String<char>::LoadStr(title);
    mbep.Flags = MB_YESNOCANCEL | MSGBOXEX_ICONQUESTION;
    if (checkBoxText != -1)
    {
        mbep.CheckBoxText = String<char>::LoadStr(checkBoxText);
        mbep.CheckBoxValue = checkBoxValue;
    }

    return SalamanderGeneral->SalMessageBoxEx(&mbep);
}
