// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guibutton.h
	Implements the push button component.
*/

#pragma once

#include "dispimpl.h"

class CSalamanderGuiButton : public CSalamanderGuiComponentImpl<CSalamanderGuiButton, ISalamanderGuiButton>
{
private:
    int m_nDialogResult;

protected:
    /// Overrriden.
    /// \see CSalamanderGuiComponentBase::CreateHwnd
    virtual HWND CreateHwnd(HWND hWndParent);

public:
    CSalamanderGuiButton(
        __in_opt VARIANT* text = NULL,
        __in_opt VARIANT* result = NULL);

    ~CSalamanderGuiButton();

    DECLARE_DISPOBJ_NAME(L"Salamander.Button")

    // ISalamanderGuiComponentInternal

    virtual void STDMETHODCALLTYPE RecalcBounds(
        HWND hWnd,
        HDC hDC);

    SALGUI_CLASS STDMETHODCALLTYPE GetClass(void)
    {
        return SALGUI_BUTTON;
    }

    // ISalamanderGuiButton

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_DialogResult(
        /* [retval][out] */ int* val);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_DialogResult(
        /* [in] */ int val);
};
