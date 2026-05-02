// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guitxtbox.cpp
	Implements the text box component.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "guicomponent.h"
#include "guitxtbox.h"

extern HINSTANCE g_hInstance;

CSalamanderGuiTextBox::CSalamanderGuiTextBox(__in_opt VARIANT* text) : CSalamanderGuiComponentImpl<CSalamanderGuiTextBox, ISalamanderGuiComponent>(text)
{
    m_uStyle &= ~SALGUI_STYLE_AUTOLAYOUT;
    m_uStyle |= SALGUI_STYLE_FULLWIDTH;
}

CSalamanderGuiTextBox::~CSalamanderGuiTextBox()
{
}

HWND CSalamanderGuiTextBox::CreateHwnd(HWND hWndParent)
{
    HWND hWnd;
    SALGUI_BOUNDS pxbounds = m_bounds;

    m_pParent->BoundsToPixels(&pxbounds);

    hWnd = CreateWindowEx(
        WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE,
        _T("edit"),
        OLE2T(m_strText),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_LEFT,
        pxbounds.x,
        pxbounds.y,
        pxbounds.cx,
        pxbounds.cy,
        hWndParent,
        NULL, // menu
        g_hInstance,
        NULL); // param

    if (hWnd != NULL)
    {
        // There seems to be some problem with long strings in
        // Win7 as reported by Honza (#26). For now we limit
        // the text to 2K characters that should be enough for
        // scripts.
        SendMessage(hWnd, EM_LIMITTEXT, 2048, 0);
    }

    return hWnd;
}

void STDMETHODCALLTYPE CSalamanderGuiTextBox::RecalcBounds(
    HWND hWnd,
    HDC hDC)
{
    m_bounds.cx = 120;
    m_bounds.cy = 12;
}

void STDMETHODCALLTYPE CSalamanderGuiTextBox::TransferDataToWindow(void)
{
}

void STDMETHODCALLTYPE CSalamanderGuiTextBox::TransferDataFromWindow(void)
{
    TransferTextFromWindow();
}
