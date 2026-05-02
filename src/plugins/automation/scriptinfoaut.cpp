// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	scriptinfoaut.cpp
	ScriptInfo automation object.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "scriptinfoaut.h"
#include "scriptlist.h"
#include "automationplug.h"

extern CAutomationPluginInterface g_oAutomationPlugin;

CSalamanderScriptInfoAutomation::CSalamanderScriptInfoAutomation(
    CScriptInfo* pScript)
{
    TCHAR szExpanded[MAX_PATH];

    _ASSERTE(pScript != NULL);

    m_strName = pScript->GetDisplayName();

    if (g_oAutomationPlugin.ExpandPath(pScript->GetFileName(), szExpanded,
                                       _countof(szExpanded)))
    {
        m_strPath = szExpanded;
    }
}

CSalamanderScriptInfoAutomation::~CSalamanderScriptInfoAutomation()
{
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderScriptInfoAutomation::get_Name(
    /* [retval][out] */ BSTR* name)
{
    *name = m_strName.copy();
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderScriptInfoAutomation::get_Path(
    /* [retval][out] */ BSTR* path)
{
    *path = m_strPath.copy();
    return S_OK;
}
