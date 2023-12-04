// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	raiserr.h
	Utility routines to throw exceptions.
*/

#pragma once

void RaiseError(LPCOLESTR pszDescription, REFIID riid, LPCOLESTR pszProgId);

/// \param nIdDescription Idenfier of the string in the language
///        module that describes the error.
void RaiseError(int nIdDescription, REFIID riid, LPCOLESTR pszProgId);

void RaiseDosError(DWORD dwErrCode, REFIID riid, LPCOLESTR pszProgId);

void RaiseErrorFmtV(LPCOLESTR pszFormat, REFIID riid, LPCOLESTR pszProgId, va_list ap);
void RaiseErrorFmtV(int nIdFormat, REFIID riid, LPCOLESTR pszProgId, va_list ap);

HRESULT RaiseError(
    __out EXCEPINFO* ei,
    LPCOLESTR pszDescription,
    HRESULT scode,
    LPCOLESTR pszProgId);

HRESULT RaiseError(
    __out EXCEPINFO* ei,
    int nIdDescription,
    HRESULT scode,
    LPCOLESTR pszProgId);
