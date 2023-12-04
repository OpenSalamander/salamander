// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guilabel.h
	Implements the label component.
*/

#pragma once

#include "dispimpl.h"

class CSalamanderGuiLabel : public CSalamanderGuiComponentImpl<CSalamanderGuiLabel, ISalamanderGuiComponent>
{
protected:
    /// Overrriden.
    /// \see CSalamanderGuiComponentBase::CreateHwnd
    virtual HWND CreateHwnd(HWND hWndParent);

public:
    CSalamanderGuiLabel(__in_opt VARIANT* text = NULL);
    ~CSalamanderGuiLabel();

    DECLARE_DISPOBJ_NAME(L"Salamander.Label")

    // ISalamanderGuiComponentInternal

    virtual void STDMETHODCALLTYPE RecalcBounds(
        HWND hWnd,
        HDC hDC);

    SALGUI_CLASS STDMETHODCALLTYPE GetClass(void)
    {
        return SALGUI_LABEL;
    }
};
