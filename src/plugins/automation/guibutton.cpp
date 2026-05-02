// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guibutton.cpp
	Implements the push button component.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "guicomponent.h"
#include "guibutton.h"
#include "aututils.h"

extern HINSTANCE g_hInstance;

CSalamanderGuiButton::CSalamanderGuiButton(
    __in_opt VARIANT* text,
    __in_opt VARIANT* result) : CSalamanderGuiComponentImpl<CSalamanderGuiButton, ISalamanderGuiButton>(text)
{
    m_nDialogResult = 0;

    if (IsArgumentPresent(result))
    {
        UNK_PROTECT_CONSTRUCTOR_BEGIN

        int n = _variant_t(result);
        HRESULT hr = put_DialogResult(n);
        if (FAILED(hr))
        {
            _com_raise_error(hr);
        }

        UNK_PROTECT_CONSTRUCTOR_END
    }

    m_uStyle &= ~SALGUI_STYLE_AUTOLAYOUT;
}

CSalamanderGuiButton::~CSalamanderGuiButton()
{
}

HWND CSalamanderGuiButton::CreateHwnd(HWND hWndParent)
{
    HWND hWnd;
    SALGUI_BOUNDS pxbounds = m_bounds;

    m_pParent->BoundsToPixels(&pxbounds);

    hWnd = CreateWindowEx(
        WS_EX_NOPARENTNOTIFY,
        _T("button"),
        OLE2T(m_strText),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        pxbounds.x,
        pxbounds.y,
        pxbounds.cx,
        pxbounds.cy,
        hWndParent,
        (HMENU)(INT_PTR)m_nDialogResult, // menu
        g_hInstance,
        NULL); // param

    return hWnd;
}

void STDMETHODCALLTYPE CSalamanderGuiButton::RecalcBounds(
    HWND hWnd,
    HDC hDC)
{
    m_bounds.cx = 50;
    m_bounds.cy = 14;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiButton::get_DialogResult(
    /* [retval][out] */ int* val)
{
    *val = m_nDialogResult;
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiButton::put_DialogResult(
    /* [in] */ int val)
{
    if (val < 0)
    {
        return E_INVALIDARG;
    }

    m_nDialogResult = val;
    return S_OK;
}
