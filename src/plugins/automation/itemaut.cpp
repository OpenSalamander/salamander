// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	itemaut.cpp
	Panel item automation object.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "itemaut.h"
#include "aututils.h"

extern CSalamanderGeneralAbstract* SalamanderGeneral;

CSalamanderPanelItemAutomation::CSalamanderPanelItemAutomation()
{
    _ctor();
}

CSalamanderPanelItemAutomation::CSalamanderPanelItemAutomation(const CFileData* pData, PCTSTR pszPath)
{
    _ctor();
    Set(pData, pszPath);
}

CSalamanderPanelItemAutomation::CSalamanderPanelItemAutomation(const CFileData* pData, int nPanel)
{
    _ctor();
    Set(pData, nPanel);
}

CSalamanderPanelItemAutomation::~CSalamanderPanelItemAutomation()
{
    delete[] m_pszFullPath;
}

void CSalamanderPanelItemAutomation::_ctor()
{
    m_pszFullPath = NULL;
    m_cchFullPath = 0;
    m_pszName = NULL;
    m_size.QuadPart = 0;
    m_dwAttributes = INVALID_FILE_ATTRIBUTES;
    m_dateLastModified = 0;
}

void CSalamanderPanelItemAutomation::Set(const CFileData* pData, PCTSTR pszPath)
{
    size_t cchNewLen;
    bool bHasSlash = false;
    size_t cchPathLenA;
    size_t cchNameLenA;
    size_t cchPathLenW;
    size_t cchNameLenW;
    FILETIME ftLocal;
    SYSTEMTIME st;

    _ASSERTE(pszPath != NULL && *pszPath != 0);

    cchPathLenA = _tcslen(pszPath);
    if (cchPathLenA > 0)
    {
        bHasSlash = pszPath[cchPathLenA - 1] == _T('\\');
    }

    cchNameLenA = _tcslen(pData->Name);

    cchPathLenW = MultiByteToWideChar(CP_ACP, 0, pszPath, (int)cchPathLenA, NULL, 0);
    cchNameLenW = MultiByteToWideChar(CP_ACP, 0, pData->Name, (int)cchNameLenA, NULL, 0);

    cchNewLen = cchPathLenW + cchNameLenW + (bHasSlash ? 0 : 1) + 1;
    if (cchNewLen > m_cchFullPath)
    {
        cchNewLen = (cchNewLen + 127) & ~0x7F; // align to multiple of 128 bytes

        delete[] m_pszFullPath;
        m_pszFullPath = m_pszName = NULL;

        m_pszFullPath = new WCHAR[cchNewLen];
        m_cchFullPath = cchNewLen;
    }

    cchPathLenW = MultiByteToWideChar(CP_ACP, 0, pszPath, (int)cchPathLenA, m_pszFullPath, (int)m_cchFullPath);
    if (!bHasSlash)
    {
        m_pszFullPath[cchPathLenW] = L'\\';
        ++cchPathLenW;
    }

    cchNameLenW = MultiByteToWideChar(CP_ACP, 0, pData->Name, (int)cchNameLenA, m_pszFullPath + cchPathLenW, (int)(m_cchFullPath - cchPathLenW));
    m_pszFullPath[cchPathLenW + cchNameLenW] = L'\0';

    m_pszName = PathFindFileNameW(m_pszFullPath);

    m_dwAttributes = pData->Attr;
    m_size.QuadPart = pData->Size.Value;

    m_dateLastModified = 0;
    if (FileTimeToLocalFileTime(&pData->LastWrite, &ftLocal))
    {
        if (FileTimeToSystemTime(&ftLocal, &st))
        {
            SystemTimeToVariantTime(&st, &m_dateLastModified);
        }
    }
}

void CSalamanderPanelItemAutomation::Set(const CFileData* pData, int nPanel)
{
    TCHAR szPath[MAX_PATH];

    SalamanderGeneral->GetPanelPath(nPanel, szPath, _countof(szPath), NULL, NULL);

    Set(pData, szPath);
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelItemAutomation::get_Path(
    /* [retval][out] */ BSTR* path)
{
    *path = SysAllocString(m_pszFullPath);
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelItemAutomation::get_Name(
    /* [retval][out] */ BSTR* name)
{
    *name = SysAllocString(m_pszName);
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelItemAutomation::get_Size(
    /* [retval][out] */ VARIANT* size)
{
    QuadWordToVariant(m_size, size);
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelItemAutomation::get_DateLastModified(
    /* [retval][out] */ DATE* date)
{
    *date = m_dateLastModified;
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelItemAutomation::get_Attributes(
    /* [retval][out] */ int* attrs)
{
    *attrs = m_dwAttributes;
    return S_OK;
}
