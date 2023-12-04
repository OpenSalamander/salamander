// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guilabel.cpp
	Implements the label component.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "guicomponent.h"
#include "guilabel.h"

extern HINSTANCE g_hInstance;

CSalamanderGuiLabel::CSalamanderGuiLabel(__in_opt VARIANT* text) : CSalamanderGuiComponentImpl<CSalamanderGuiLabel, ISalamanderGuiComponent>(text)
{
    m_uStyle |= SALGUI_STYLE_AUTOLAYOUT;
}

CSalamanderGuiLabel::~CSalamanderGuiLabel()
{
}

HWND CSalamanderGuiLabel::CreateHwnd(HWND hWndParent)
{
    HWND hWnd;
    SALGUI_BOUNDS pxbounds = m_bounds;

    m_pParent->BoundsToPixels(&pxbounds);

    hWnd = CreateWindowEx(
        WS_EX_NOPARENTNOTIFY,
        _T("static"),
        OLE2T(m_strText),
        WS_CHILD | WS_VISIBLE,
        pxbounds.x,
        pxbounds.y,
        pxbounds.cx,
        pxbounds.cy,
        hWndParent,
        (HMENU)NULL, // menu
        g_hInstance,
        NULL); // param

    return hWnd;
}

void STDMETHODCALLTYPE CSalamanderGuiLabel::RecalcBounds(
    HWND hWnd,
    HDC hDC)
{
    SIZE sz;

    GetTextSize(hDC, sz);

    m_bounds.cx = sz.cx + 4;
    m_bounds.cy = sz.cy;
}
