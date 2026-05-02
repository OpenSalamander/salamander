// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	raiserr.cpp
	Utility routines to throw exceptions.
*/

#include "precomp.h"
#include "raiserr.h"

extern HINSTANCE g_hLangInst;

void RaiseError(LPCOLESTR pszDescription, REFIID riid, LPCOLESTR pszProgId)
{
    HRESULT hr;
    ICreateErrorInfo* pcei;
    IErrorInfo* pei;

    hr = CreateErrorInfo(&pcei);
    _ASSERTE(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return;
    }

    pcei->SetDescription(const_cast<LPOLESTR>(pszDescription));
    pcei->SetGUID(riid);
    if (pszProgId != NULL)
    {
        pcei->SetSource(const_cast<LPOLESTR>(pszProgId));
    }
    else
    {
        static wchar_t buff[] = L"Salamander";
        pcei->SetSource(buff);
    }

    hr = pcei->QueryInterface(&pei);
    _ASSERTE(SUCCEEDED(hr) && pei != NULL);

    SetErrorInfo(0, pei);
    pei->Release();
    pcei->Release();
}

void RaiseError(int nIdDescription, REFIID riid, LPCOLESTR pszProgId)
{
    WCHAR szDescription[256];

    LoadStringW(g_hLangInst, nIdDescription, szDescription, _countof(szDescription));
    RaiseError(szDescription, riid, pszProgId);
}

void RaiseDosError(DWORD dwErrCode, REFIID riid, LPCOLESTR pszProgId)
{
    LPWSTR pwzError;
    DWORD cch;

    cch = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        (LPCVOID)NULL, // source
        dwErrCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        (LPWSTR)&pwzError,
        0,     // size
        NULL); // va_list
    if (cch > 0)
    {
        // trim whitespace
        while (iswspace(pwzError[cch - 1]))
        {
            --cch;
            pwzError[cch] = L'\0';
        }

        RaiseError(pwzError, riid, pszProgId);
        LocalFree((HLOCAL)pwzError);
    }
    else
    {
        _ASSERTE(0);
    }
}

void RaiseErrorFmtV(LPCOLESTR pszFormat, REFIID riid, LPCOLESTR pszProgId, va_list ap)
{
    WCHAR szDescription[256];

    StringCchVPrintfW(szDescription, _countof(szDescription), pszFormat, ap);
    RaiseError(szDescription, riid, pszProgId);
}

void RaiseErrorFmtV(int nIdFormat, REFIID riid, LPCOLESTR pszProgId, va_list ap)
{
    WCHAR szFormat[256];

    LoadStringW(g_hLangInst, nIdFormat, szFormat, _countof(szFormat));
    RaiseErrorFmtV(szFormat, riid, pszProgId, ap);
}

HRESULT RaiseError(
    __out EXCEPINFO* ei,
    LPCOLESTR pszDescription,
    HRESULT scode,
    LPCOLESTR pszProgId)
{
    memset(ei, 0, sizeof(EXCEPINFO));

    ei->bstrDescription = SysAllocString(pszDescription);
    ei->bstrSource = SysAllocString(pszProgId ? pszProgId : L"Salamander");
    ei->scode = scode;

    return DISP_E_EXCEPTION;
}

HRESULT RaiseError(
    __out EXCEPINFO* ei,
    int nIdDescription,
    HRESULT scode,
    LPCOLESTR pszProgId)
{
    if (ei != NULL)
    {
        WCHAR szDescription[256];

        LoadStringW(g_hLangInst, nIdDescription, szDescription, _countof(szDescription));
        return RaiseError(ei, szDescription, scode, pszProgId);
    }

    return DISP_E_EXCEPTION;
}
