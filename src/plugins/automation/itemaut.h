// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	itemaut.h
	Panel item automation object.
*/

#pragma once

#include "dispimpl.h"

class CSalamanderPanelItemAutomation : public CDispatchImpl<CSalamanderPanelItemAutomation, ISalamanderPanelItem>
{
private:
    WCHAR* m_pszFullPath;
    size_t m_cchFullPath;
    WCHAR* m_pszName;
    LARGE_INTEGER m_size;
    DATE m_dateLastModified;
    DWORD m_dwAttributes;

    void _ctor();

public:
    DECLARE_DISPOBJ_NAME(L"Salamander.Item")

    CSalamanderPanelItemAutomation();
    CSalamanderPanelItemAutomation(const CFileData* pData, PCTSTR pszPath);
    CSalamanderPanelItemAutomation(const CFileData* pData, int nPanel);
    ~CSalamanderPanelItemAutomation();

    void Set(const CFileData* pData, PCTSTR pszPath);
    void Set(const CFileData* pData, int nPanel);

    // ISalamanderPanelItem

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Path(
        /* [retval][out] */ BSTR* path);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Name(
        /* [retval][out] */ BSTR* name);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Size(
        /* [retval][out] */ VARIANT* size);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_DateLastModified(
        /* [retval][out] */ DATE* date);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Attributes(
        /* [retval][out] */ int* attrs);
};
