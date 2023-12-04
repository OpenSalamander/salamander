// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	abortmodal.h
	Wrapper to allow aborting modal windows.
*/

#pragma once

#define WM_SAL_ABORTMODAL (WM_APP + 0)

/// Wraps a modal dialog into the dialog that can be forcibly aborted by the user.
/// \param pfnDialogFn Function to display the modal dialog.
/// \param pContext User defined value to be passed to the dialog function.
/// \return If the modal dialog was successfully executed, the return value is S_OK.
///         If the modal dialog was aborted, the return value is SALAUT_E_ABORT.
HRESULT AbortableModalDialogWrapper(
    __in class CScriptInfo* pScript,
    __in_opt HWND hwndOwner,
    __in void(WINAPI* pfnDialogFn)(void*),
    __in_opt void* pContext);

void UninitializeAbortableModalDialogWrapper();
