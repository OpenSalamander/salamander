// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	saltypelib.cpp
	Type library encapsulation.
*/

#include "precomp.h"
#include "saltypelib.h"
#include "salamander_h.h"

#define REGISTER_LIB 0

CSalamanderTypeLib SalamanderTypeLib;

CSalamanderTypeLib::CSalamanderTypeLib()
{
    m_hrLoad = E_PENDING;
    m_pTypeLib = NULL;
}

CSalamanderTypeLib::~CSalamanderTypeLib()
{
    if (m_pTypeLib != NULL)
    {
        m_pTypeLib->Release();
    }
}

HRESULT CSalamanderTypeLib::Get(ITypeLib** ppTypeLib)
{
    for (;;)
    {
        if (m_pTypeLib == NULL)
        {
            if (m_hrLoad == E_PENDING)
            {
                Load();
            }
            else
            {
                *ppTypeLib = NULL;
                return m_hrLoad;
            }
        }
        else
        {
            *ppTypeLib = m_pTypeLib;
            m_pTypeLib->AddRef();
            return S_OK;
        }
    }
}

void CSalamanderTypeLib::Load()
{
    WCHAR szModule[MAX_PATH];
    extern HINSTANCE g_hInstance;

    _ASSERTE(m_pTypeLib == NULL);
    _ASSERTE(m_hrLoad == E_PENDING);

#if REGISTER_TYPELIB
    m_hrLoad = LoadRegTypeLib(LIBID_SalamanderLib, 1, 0, 0, &m_pTypeLib);
    if (FAILED(m_hrLoad))
    {
#endif

        GetModuleFileNameW(g_hInstance, szModule, _countof(szModule));
        m_hrLoad = LoadTypeLib(szModule, &m_pTypeLib);
        _ASSERTE(SUCCEEDED(m_hrLoad));

#if REGISTER_TYPELIB
        if (SUCCEEDED(m_hrLoad))
        {
            HRESULT hrRegister;
            hrRegister = RegisterTypeLib(m_pTypeLib, szModule, NULL);
            _ASSERTE(SUCCEEDED(hrRegister));
        }
    }
#endif
}
