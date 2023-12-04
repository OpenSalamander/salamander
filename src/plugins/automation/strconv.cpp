// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	strconv.cpp
	Convenience classes to convert ANSI strings to OLE strings
	and vice versa.
*/

#include "precomp.h"
#include "strconv.h"

OLE2A::OLE2A(LPCOLESTR s)
{
    m_pstrOleString = s;
    m_pszDynamicBuffer = NULL;
}

OLE2A::~OLE2A()
{
    delete[] m_pszDynamicBuffer;
}

OLE2A::operator const char*()
{
    if (m_pstrOleString == NULL)
    {
        return NULL;
    }

    if (*m_pstrOleString == L'\0')
    {
        return "";
    }

    if (m_pszDynamicBuffer != NULL)
    {
        return m_pszDynamicBuffer;
    }

    int len = WideCharToMultiByte(CP_ACP, 0, m_pstrOleString, -1,
                                  m_szStaticBuffer, sizeof(m_szStaticBuffer), NULL, NULL);
    if (len > 0)
    {
        return m_szStaticBuffer;
    }

    len = WideCharToMultiByte(CP_ACP, 0, m_pstrOleString, -1,
                              NULL, 0, NULL, NULL);

    m_pszDynamicBuffer = new char[len];
    WideCharToMultiByte(CP_ACP, 0, m_pstrOleString, -1,
                        m_pszDynamicBuffer, len, NULL, NULL);

    return m_pszDynamicBuffer;
}

////////////////////////////////////////////////////////////////////////////////

A2OLE::A2OLE(const char* s)
{
    m_pszAnsiString = s;
    m_pstrDynamicBuffer = NULL;
}

A2OLE::~A2OLE()
{
    delete[] m_pstrDynamicBuffer;
}

A2OLE::operator LPCOLESTR()
{
    if (m_pszAnsiString == NULL)
    {
        return NULL;
    }

    if (*m_pszAnsiString == '\0')
    {
        return L"";
    }

    if (m_pstrDynamicBuffer != NULL)
    {
        return m_pstrDynamicBuffer;
    }

    int len = MultiByteToWideChar(CP_ACP, 0, m_pszAnsiString, -1,
                                  m_achStaticBuffer, _countof(m_achStaticBuffer));
    if (len > 0)
    {
        return m_achStaticBuffer;
    }

    len = MultiByteToWideChar(CP_ACP, 0, m_pszAnsiString, -1,
                              NULL, 0);

    m_pstrDynamicBuffer = new OLECHAR[len];
    MultiByteToWideChar(CP_ACP, 0, m_pszAnsiString, -1,
                        m_pstrDynamicBuffer, len);

    return m_pstrDynamicBuffer;
}
