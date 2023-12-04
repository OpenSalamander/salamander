// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	scriptinfoaut.h
	ScriptInfo automation object.
*/

#pragma once

#include "dispimpl.h"

class CSalamanderScriptInfoAutomation : public CDispatchImpl<CSalamanderScriptInfoAutomation, ISalamanderScriptInfo>
{
private:
    _bstr_t m_strPath;
    _bstr_t m_strName;

public:
    DECLARE_DISPOBJ_NAME(L"Salamander.ScriptInfo")

    CSalamanderScriptInfoAutomation(class CScriptInfo* pScript);
    ~CSalamanderScriptInfoAutomation();

    // ISalamanderScriptInfo

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Name(
        /* [retval][out] */ BSTR* name);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Path(
        /* [retval][out] */ BSTR* path);
};
