// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	aututils.h
	Miscelaneous utility routines.
*/

#pragma once

/// Checks for Visual Basic Nothing object.
static inline bool IsNothing(const VARIANT* arg)
{
    return (V_VT(arg) == VT_DISPATCH || V_VT(arg) == VT_UNKNOWN) &&
           V_DISPATCH(arg) == NULL;
}

/// Determines whether the VARIANT structure passed as an optional parameter
/// to the IDispatch method actually represents some value.
static inline bool IsArgumentPresent(const VARIANT* arg)
{
    return arg != NULL &&
           V_VT(arg) != VT_ERROR && // DISP_E_PARAMNOTFOUND
           V_VT(arg) != VT_EMPTY &&
           V_VT(arg) != VT_NULL &&
           !IsNothing(arg);
}

/// Displays message box with details about the exception.
/// \param pError Pointer to the IActiveScriptError interface that will be
///        queried for the details about the exception.
/// \param pShim Shim for the scripting engine. This will be used
///        to workaround some of the bugs of the known engines.
/// \param bDebug If true, the message box will have the additional Debug
///        button that offers the user to debug the script.
/// \return If the user pressed the Debug button, the return value is positive.
///         If the user choosed not to debug the script, the return value is zero.
///         If there was a hard error (e.g. stack overflow), the return value is negative.
int DisplayException(IActiveScriptError* pError, class CScriptEngineShim* pShim, bool bDebug = false);

/// Displays message box with details about the exception.
/// \param ei The EXCEPINFO structure containing details about the exception.
/// \param bFree If true, the resources associated with the EXCEPINFO
///        structure will be released.
void DisplayException(EXCEPINFO& ei, bool bFree = true);

/// Releases resources associated with the EXCEPINFO structure.
void FreeException(EXCEPINFO& ei);

/// Formats textual description of the system error code.
void FormatErrorText(
    HRESULT hrCode,
    TCHAR* pszBuffer,
    UINT cchMax);

/// For the given string computes SDBM hash value.
UINT HashString(__in_z PCTSTR s);

/// Converts 64b integer to proper OLE automation data type.
/// The function selects appropriate data type (either I4 or R8) to
/// hold the 64b value.
void QuadWordToVariant(LARGE_INTEGER q, __out VARIANT* var);
void QuadWordToVariant(const CQuadWord& q, __out VARIANT* var);

/// Retrieved property value from the IDispatch interface.
/// \param pdisp Pointer to the IDispatch interface.
/// \param name Name of the property to retrieve.
/// \param result Property value.
HRESULT DispPropGet(IDispatch* pdisp, PCWSTR name, VARIANT* result);
HRESULT DispPropGet(IUnknown* punk, PCWSTR name, VARIANT* result);

static const UINT MAX_EXIT_BUTTONS = 19;

/// Converts specific *huge* button codes to friendly numbers.
int ButtonToFriendlyNumber(int button);

/// Releases Ctrl/Alt/Shift keys. This is used to workaround problems
/// with script shortcut keys interferring with WshShell.SendKeys.
/// See CScriptInfo::ExecuteWorker for detailed explanation.
void ResetKeyboardState();

/// Checks if the script execution was aborted and if so, fills appropriately
/// the exception information.
HRESULT CheckAbort(__in class CScriptInfo* pScriptInfo, __out EXCEPINFO* pExcepInfo);
