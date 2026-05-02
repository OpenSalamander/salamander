// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	strconv.h
	Convenience classes to convert ANSI strings to OLE strings
	and vice versa.
*/

#pragma once

#define STRCONV_STATIC_BUFFER_SIZE 64

class OLE2A
{
private:
    LPCOLESTR m_pstrOleString;
    char m_szStaticBuffer[STRCONV_STATIC_BUFFER_SIZE];
    char* m_pszDynamicBuffer;

public:
    OLE2A(LPCOLESTR s);
    ~OLE2A();

    operator const char*();
};

class A2OLE
{
private:
    const char* m_pszAnsiString;
    OLECHAR m_achStaticBuffer[STRCONV_STATIC_BUFFER_SIZE];
    LPOLESTR m_pstrDynamicBuffer;

public:
    A2OLE(const char* s);
    ~A2OLE();

    operator LPCOLESTR();
};

#ifdef _UNICODE
#define OLE2T(s) s
#define T2OLE(s) s
#else
#define OLE2T(s) OLE2A(s)
#define T2OLE(s) A2OLE(s)
#endif
