// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guichkbox.cpp
	Implements the checkbox component.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "guicomponent.h"
#include "guichkbox.h"

extern HINSTANCE g_hInstance;

CSalamanderGuiCheckBox::CSalamanderGuiCheckBox(__in_opt VARIANT* text) : CSalamanderGuiComponentImpl<CSalamanderGuiCheckBox, ISalamanderGuiCheckBox>(text)
{
    m_bChecked = false;
    m_uStyle |= SALGUI_STYLE_AUTOLAYOUT;
}

CSalamanderGuiCheckBox::~CSalamanderGuiCheckBox()
{
}

HWND CSalamanderGuiCheckBox::CreateHwnd(HWND hWndParent)
{
    HWND hWnd;
    SALGUI_BOUNDS pxbounds = m_bounds;

    m_pParent->BoundsToPixels(&pxbounds);

    hWnd = CreateWindowEx(
        WS_EX_NOPARENTNOTIFY,
        _T("button"),
        OLE2T(m_strText),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        pxbounds.x,
        pxbounds.y,
        pxbounds.cx,
        pxbounds.cy,
        hWndParent,
        NULL, // menu
        g_hInstance,
        NULL); // param

    return hWnd;
}

void STDMETHODCALLTYPE CSalamanderGuiCheckBox::RecalcBounds(
    HWND hWnd,
    HDC hDC)
{
    SIZE sz;

    GetTextSize(hDC, sz);

    m_bounds.cx = sz.cx + 18;
    m_bounds.cy = max(sz.cy, 10);
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiCheckBox::get_Checked(
    /* [retval][out] */ VARIANT_BOOL* val)
{
    *val = m_bChecked ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiCheckBox::put_Checked(
    /* [in] */ VARIANT_BOOL val)
{
    m_bChecked = (val != VARIANT_FALSE);
    return S_OK;
}

void STDMETHODCALLTYPE CSalamanderGuiCheckBox::TransferDataToWindow(void)
{
    SendMessage(m_hWnd, BM_SETCHECK, m_bChecked ? BST_CHECKED : BST_UNCHECKED, 0);
}

void STDMETHODCALLTYPE CSalamanderGuiCheckBox::TransferDataFromWindow(void)
{
    m_bChecked = (SendMessage(m_hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
}
