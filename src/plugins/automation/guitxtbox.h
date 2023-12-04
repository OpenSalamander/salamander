// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guitxtbox.h
	Implements the text box component.
*/

#pragma once

#include "dispimpl.h"

class CSalamanderGuiTextBox : public CSalamanderGuiComponentImpl<CSalamanderGuiTextBox, ISalamanderGuiComponent>
{
private:
protected:
    /// Overrriden.
    /// \see CSalamanderGuiComponentBase::CreateHwnd
    virtual HWND CreateHwnd(HWND hWndParent);

public:
    CSalamanderGuiTextBox(__in_opt VARIANT* text = NULL);
    ~CSalamanderGuiTextBox();

    DECLARE_DISPOBJ_NAME(L"Salamander.TextBox")

    // ISalamanderGuiComponentInternal

    virtual void STDMETHODCALLTYPE RecalcBounds(
        HWND hWnd,
        HDC hDC);

    virtual void STDMETHODCALLTYPE TransferDataToWindow(void);

    virtual void STDMETHODCALLTYPE TransferDataFromWindow(void);

    SALGUI_CLASS STDMETHODCALLTYPE GetClass(void)
    {
        return SALGUI_TEXTBOX;
    }

    // ISalamanderGuiTextBox
};
