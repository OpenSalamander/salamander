// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	waitwndaut.cpp
	WaitWindow automation object.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "waitwndaut.h"
#include "lang\lang.rh"

extern CSalamanderGeneralAbstract* SalamanderGeneral;

CSalamanderWaitWndAutomation::CSalamanderWaitWndAutomation()
{
    m_bShown = false;
    //m_strTitle;
    //m_strText;
    m_bCancelEnabled = true;
    m_nDelay = 500;
}

CSalamanderWaitWndAutomation::~CSalamanderWaitWndAutomation()
{
    if (m_bShown)
    {
        SalamanderGeneral->DestroySafeWaitWindow();
    }
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::Show(void)
{
    if (m_bShown)
    {
        return S_OK;
    }

    // "init" GetAsyncKeyState for the IsCancelled method
    // (resets the latch status for the key, see MSDN documentation)
    GetAsyncKeyState(VK_ESCAPE);

    SalamanderGeneral->CreateSafeWaitWindow(m_strText, m_strTitle,
                                            m_nDelay, m_bCancelEnabled, SalamanderGeneral->GetMsgBoxParent());
    m_bShown = true;

    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::Hide(void)
{
    if (!m_bShown)
    {
        return S_OK;
    }

    SalamanderGeneral->DestroySafeWaitWindow();
    m_bShown = false;

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::get_Title(
    /* [retval][out] */ BSTR* title)
{
    *title = m_strTitle.copy();
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::put_Title(
    /* [in] */ BSTR title)
{
    if (m_bShown)
    {
        RaiseErrorFmt(IDS_E_READONLYWHILEPROGRESSSHOWN, L"Title");
        return SALAUT_E_READONLYWHILEPROGRESSSHOWN;
    }

    m_strTitle = title;
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::get_Text(
    /* [retval][out] */ BSTR* text)
{
    *text = m_strText.copy();
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::put_Text(
    /* [in] */ BSTR text)
{
    m_strText = text;

    if (m_bShown)
    {
        SalamanderGeneral->SetSafeWaitWindowText(m_strText);
    }

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::get_IsCancelled(
    /* [retval][out] */ VARIANT_BOOL* cancelled)
{
    if (!m_bShown)
    {
        *cancelled = VARIANT_FALSE;
        return S_OK;
    }

    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) &&
        GetForegroundWindow() == SalamanderGeneral->GetMsgBoxParent())
    {
        *cancelled = VARIANT_TRUE;

        // pump the message out of the queue
        MSG msg;
        while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
        {
        }
    }
    else
    {
        *cancelled = SalamanderGeneral->GetSafeWaitWindowClosePressed() ? VARIANT_TRUE : VARIANT_FALSE;
    }

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::get_CanCancel(
    /* [retval][out] */ VARIANT_BOOL* enabled)
{
    *enabled = m_bCancelEnabled ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::put_CanCancel(
    /* [in] */ VARIANT_BOOL enabled)
{
    m_bCancelEnabled = (enabled == VARIANT_TRUE);
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::get_Delay(
    /* [retval][out] */ int* ms)
{
    *ms = m_nDelay;
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderWaitWndAutomation::put_Delay(
    /* [in] */ int ms)
{
    if (ms < 0)
    {
        return E_INVALIDARG;
    }

    m_nDelay = ms;
    return S_OK;
}
