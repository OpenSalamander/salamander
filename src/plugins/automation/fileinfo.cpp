// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	fileinfo.cpp
	Information about the file object.
*/

#include "precomp.h"
#include "fileinfo.h"
#include "aututils.h"

CFileInfo::CFileInfo()
{
    m_pszPath = NULL;
    m_pszInfo = NULL;
}

CFileInfo::~CFileInfo()
{
    Clear();
}

void CFileInfo::Clear()
{
    delete[] m_pszPath;
    m_pszPath = NULL;

    delete[] m_pszInfo;
    m_pszInfo = NULL;
}

HRESULT CFileInfo::FromVariant(VARIANT* var)
{
    HRESULT hr;

    if (V_VT(var) == VT_BSTR)
    {
        return FromString(V_BSTR(var));
    }

    if (V_VT(var) == VT_DISPATCH || V_VT(var) == VT_UNKNOWN)
    {
        IDispatch* pdisp;
        hr = V_UNKNOWN(var)->QueryInterface(IID_IDispatch, (void**)&pdisp);
        if (SUCCEEDED(hr))
        {
            hr = FromDispatch(pdisp);
            pdisp->Release();
            return hr;
        }
    }

    try
    {
        return FromString(_bstr_t(_variant_t(var)));
    }
    catch (_com_error& e)
    {
        return e.Error();
    }
}

HRESULT CFileInfo::FromString(PCWSTR s)
{
    PCWSTR end;

    Clear();

    if (s == NULL || *s == L'\0')
    {
        SetPath(NULL, 0);
        SetInfo(NULL, 0);
        return S_OK;
    }

    // trim whitespace
    while (*s && iswspace(*s))
    {
        ++s;
    }

    // find end of the path
    end = s;
    while (*end && *end != L'\r' && *end != '\n')
    {
        ++end;
    }

    SetPath(s, (int)(end - s));

    s = end;
    while (*s && iswspace(*s))
    {
        ++s;
    }

    // find end of the info
    end = s;
    while (*end && *end != L'\r' && *end != '\n')
    {
        ++end;
    }

    SetInfo(s, (int)(end - s));

    return S_OK;
}

HRESULT CFileInfo::FromDispatch(IDispatch* pdisp)
{
    HRESULT hr;
    _variant_t var;
    UINT uValidFields = 0;
    CQuadWord size;
    DATE date;

    Clear();

    hr = DispPropGet(pdisp, L"Path", &var);
    if (hr == DISP_E_MEMBERNOTFOUND)
    {
        var.Clear();
        hr = DispPropGet(pdisp, L"Name", &var);
    }

    if (FAILED(hr))
    {
        return DISP_E_TYPEMISMATCH;
    }

    try
    {
        SetPath(_bstr_t(var));
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    var.Clear();
    hr = DispPropGet(pdisp, L"Size", &var);
    if (SUCCEEDED(hr))
    {
        LONGLONG val64;

        try
        {
            val64 = var;
            size.SetUI64(val64);
            uValidFields |= VALID_DATA_SIZE;
        }
        catch (_com_error)
        {
        }
    }

    var.Clear();
    hr = DispPropGet(pdisp, L"DateLastModified", &var);
    if (SUCCEEDED(hr))
    {
        try
        {
            var.ChangeType(VT_DATE);
            date = var;
            uValidFields |= VALID_DATA_DATE | VALID_DATA_TIME;
        }
        catch (_com_error)
        {
        }
    }

    SetInfo(size, date, uValidFields);

    return S_OK;
}

void CFileInfo::SetInfo(const CQuadWord& size, DATE date, UINT uValidFields)
{
    size_t len = 0;
    const size_t buffSize = 256;

    _ASSERTE(m_pszInfo == NULL);

    m_pszInfo = new TCHAR[buffSize];
    m_pszInfo[0] = _T('\0');

    if (uValidFields & VALID_DATA_SIZE)
    {
        SalamanderGeneral->NumberToStr(m_pszInfo, size);
        len = _tcslen(m_pszInfo);
    }

    if (uValidFields & (VALID_DATA_DATE | VALID_DATA_TIME))
    {
        SYSTEMTIME time;

        if (VariantTimeToSystemTime(date, &time))
        {
            if (len > 0)
            {
                StringCchCat(m_pszInfo, buffSize, TEXT(", "));
                len += 2;
            }

            len += GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &time,
                                 NULL, m_pszInfo + len, buffSize - (int)len) -
                   1;

            StringCchCat(m_pszInfo, buffSize, TEXT(", "));
            len += 2;

            len += GetTimeFormat(LOCALE_USER_DEFAULT, 0, &time, NULL,
                                 m_pszInfo + len, buffSize - (int)len) -
                   1;
        }
    }
}

void CFileInfo::SetPath(PCWSTR pszPath, int len)
{
    _ASSERTE(m_pszPath == NULL);
    m_pszPath = DupWideStr(pszPath, len);
}

void CFileInfo::SetInfo(PCWSTR pszInfo, int len)
{
    _ASSERTE(m_pszInfo == NULL);
    m_pszInfo = DupWideStr(pszInfo, len);
}

#ifndef _UNICODE
PTSTR CFileInfo::DupWideStr(PCWSTR s, int len)
{
    char* dup;
    int cch;

    if (len == 0 || s == NULL || (len < 0 && *s == L'\0'))
    {
        return EmptyStr();
    }

    cch = WideCharToMultiByte(CP_ACP, 0, s, len, NULL, 0, NULL, NULL);
    if (cch <= 0)
    {
        _ASSERTE(0);
        return EmptyStr();
    }

    if (len < 0)
    {
        dup = new char[cch];
        cch = WideCharToMultiByte(CP_ACP, 0, s, len, dup, cch, NULL, NULL);
    }
    else
    {
        dup = new char[cch + 1];
        cch = WideCharToMultiByte(CP_ACP, 0, s, len, dup, cch + 1, NULL, NULL);
        if (cch > 0)
        {
            dup[cch] = _T('\0');
        }
    }

    if (cch <= 0)
    {
        dup[0] = _T('\0');
    }

    return dup;
}
#endif
