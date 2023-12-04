// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guichkbox.h
	Implements the checkbox component.
*/

#pragma once

#include "dispimpl.h"

class CSalamanderGuiCheckBox : public CSalamanderGuiComponentImpl<CSalamanderGuiCheckBox, ISalamanderGuiCheckBox>
{
private:
    bool m_bChecked;

protected:
    /// Overrriden.
    /// \see CSalamanderGuiComponentBase::CreateHwnd
    virtual HWND CreateHwnd(HWND hWndParent);

public:
    CSalamanderGuiCheckBox(__in_opt VARIANT* text = NULL);
    ~CSalamanderGuiCheckBox();

    DECLARE_DISPOBJ_NAME(L"Salamander.CheckBox")

    // ISalamanderGuiComponentInternal

    virtual void STDMETHODCALLTYPE RecalcBounds(
        HWND hWnd,
        HDC hDC);

    virtual void STDMETHODCALLTYPE TransferDataToWindow(void);

    virtual void STDMETHODCALLTYPE TransferDataFromWindow(void);

    SALGUI_CLASS STDMETHODCALLTYPE GetClass(void)
    {
        return SALGUI_CHECKBOX;
    }

    // ISalamanderGuiCheckBox

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Checked(
        /* [retval][out] */ VARIANT_BOOL* val);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Checked(
        /* [in] */ VARIANT_BOOL val);
};
