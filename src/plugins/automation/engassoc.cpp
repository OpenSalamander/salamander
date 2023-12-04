// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	engassoc.cpp
	Holds information about mapping between script file extensions and
	script engines.
*/

#include "precomp.h"
#include "engassoc.h"

CScriptEngineAssociations g_oScriptAssociations;

bool CScriptEngineAssociations::FindEngineByExt(
    PCTSTR pszExt,
    __out_opt CLSID* clsidEngine)
{
    SCRIPT_ENGINE_ASSOCIATION* pExistingAssoc;
    bool bFound = false;

    _ASSERTE(*pszExt == _T('.'));

    pExistingAssoc = FindAssoc(pszExt);
    if (pExistingAssoc)
    {
        if (!IsEqualCLSID(pExistingAssoc->clsidEngine, CLSID_NULL))
        {
            if (clsidEngine != NULL)
            {
                memcpy(clsidEngine, &pExistingAssoc->clsidEngine, sizeof(CLSID));
            }

            bFound = true;
        }
    }
    else
    {
        SCRIPT_ENGINE_ASSOCIATION sNewAssoc;
        HRESULT hr;
        HKEY hkFileType;
        HKEY hkScriptEngine;
        WCHAR szEngineProgId[128];

        StringCchCopy(sNewAssoc.szExt, _countof(sNewAssoc.szExt), pszExt);
        sNewAssoc.clsidEngine = CLSID_NULL;

        hr = AssocQueryKey(ASSOCF_INIT_IGNOREUNKNOWN, ASSOCKEY_CLASS,
                           pszExt, NULL, &hkFileType);
        if (SUCCEEDED(hr))
        {
            hr = HRESULT_FROM_WIN32(RegOpenKeyEx(hkFileType,
                                                 _T("ScriptEngine"), 0, KEY_READ, &hkScriptEngine));
            if (SUCCEEDED(hr))
            {
                DWORD dwType;
                DWORD cbData = sizeof(szEngineProgId) - sizeof(szEngineProgId[0]);
                hr = HRESULT_FROM_WIN32(RegQueryValueExW(
                    hkScriptEngine, NULL, NULL, &dwType,
                    (LPBYTE)szEngineProgId, &cbData));
                if (SUCCEEDED(hr))
                {
                    if (dwType == REG_SZ)
                    {
                        // ensure nul-terminator
                        szEngineProgId[cbData / sizeof(szEngineProgId[0])] = _T('\0');
                        hr = CLSIDFromProgID(szEngineProgId, &sNewAssoc.clsidEngine);
                    }
                    else
                    {
                        hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                    }
                }

                RegCloseKey(hkScriptEngine);
            }

            RegCloseKey(hkFileType);
        }

        if (SUCCEEDED(hr))
        {
            if (clsidEngine != NULL)
            {
                memcpy(clsidEngine, &sNewAssoc.clsidEngine, sizeof(CLSID));
            }

            bFound = true;
        }

        Add(sNewAssoc);
    }

    return bFound;
}

SCRIPT_ENGINE_ASSOCIATION* CScriptEngineAssociations::FindAssoc(PCTSTR pszExt)
{
    for (int i = 0; i < Count; i++)
    {
        SCRIPT_ENGINE_ASSOCIATION& assoc = At(i);
        if (_tcsicmp(pszExt, assoc.szExt) == 0)
        {
            return &assoc;
        }
    }

    return NULL;
}
