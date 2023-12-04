// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	progressaut.cpp
	ProgressDialog automation object.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "progressaut.h"
#include "lang\lang.rh"
#include "aututils.h"

extern CSalamanderGeneralAbstract* SalamanderGeneral;
extern HINSTANCE g_hLangInst;

CSalamanderProgressAutomation::CSalamanderProgressAutomation(CSalamanderForOperationsAbstract* pOperation)
{
    m_style = StyleOneBar;
    m_bShown = false;
    m_bCancelEnabled = true;

    _ASSERTE(pOperation != NULL);
    m_pOperation = pOperation;

    m_nMax.SetUI64(100);
    m_nPos.SetUI64(0);

    m_nTotalMax.SetUI64(100);
    m_nTotalPos.SetUI64(0);

    WCHAR szDefTitle[64];
    LoadStringW(g_hLangInst, IDS_PROGRESSTITLE, szDefTitle, _countof(szDefTitle));
    m_strTitle = szDefTitle;
}

CSalamanderProgressAutomation::~CSalamanderProgressAutomation()
{
    if (m_bShown)
    {
        m_pOperation->CloseProgressDialog();
    }
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::Show(void)
{
    if (m_bShown)
    {
        return S_OK;
    }

    m_pOperation->OpenProgressDialog(m_strTitle, m_style == StyleTwoBar, NULL, m_nTotalMax.Value == 0);
    if (m_style == StyleOneBar)
    {
        const CQuadWord invalid(-1, -1);
        m_pOperation->ProgressSetTotalSize(m_nMax, invalid);
        m_pOperation->ProgressSetSize(m_nPos, invalid, FALSE);
    }
    else
    {
        m_pOperation->ProgressSetTotalSize(m_nMax, m_nTotalMax);
        m_pOperation->ProgressSetSize(m_nPos, m_nTotalPos, FALSE);
    }

    m_pOperation->ProgressEnableCancel(m_bCancelEnabled);

    m_bShown = true;

    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::Hide(void)
{
    if (!m_bShown)
    {
        return S_OK;
    }

    m_pOperation->CloseProgressDialog();
    m_bShown = false;

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::get_Title(
    /* [retval][out] */ BSTR* title)
{
    *title = m_strTitle.copy();
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::put_Title(
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

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::AddText(
    /* [in] */ BSTR text)
{
    if (!m_bShown)
    {
        RaiseErrorFmt(IDS_E_READONLYWHILEPROGRESSHIDDEN, L"AddText");
        return SALAUT_E_READONLYWHILEPROGRESSSHOWN;
    }

    m_pOperation->ProgressDialogAddText(_bstr_t(text), FALSE);

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::get_IsCancelled(
    /* [retval][out] */ VARIANT_BOOL* cancelled)
{
    if (!m_bShown)
    {
        *cancelled = VARIANT_FALSE;
        return S_OK;
    }

    // ProgressAddSize return non-zero if the operation should continue
    // our logic is quite oposite - we return true, if the dialogg has been
    // cancelled and the operation should be aborted
    *cancelled = m_pOperation->ProgressAddSize(0, TRUE) ? VARIANT_FALSE : VARIANT_TRUE;

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::get_Position(
    /* [retval][out] */ VARIANT* progress)
{
    if (!m_bShown)
    {
        V_VT(progress) = VT_INT;
        V_INT(progress) = 0;
        return S_OK;
    }

    QuadWordToVariant(m_nPos, progress);
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::put_Position(
    /* [in] */ VARIANT* progress)
{
    LONGLONG val64;

    if (!m_bShown)
    {
        return S_OK;
    }

    try
    {
        val64 = _variant_t(progress);
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    m_nPos.SetUI64(val64);

    if (m_style == StyleOneBar)
    {
        const CQuadWord invalid(-1, -1);
        m_pOperation->ProgressSetSize(m_nPos, invalid, FALSE);
    }
    else
    {
        m_pOperation->ProgressSetSize(m_nPos, m_nTotalPos, FALSE);
    }

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::get_TotalPosition(
    /* [retval][out] */ VARIANT* progress)
{
    QuadWordToVariant(m_nTotalPos, progress);
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::put_TotalPosition(
    /* [in] */ VARIANT* progress)
{
    LONGLONG val64;

    if (!m_bShown || m_style != StyleTwoBar)
    {
        return S_OK;
    }

    try
    {
        val64 = _variant_t(progress);
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    m_nTotalPos.SetUI64(val64);

    m_pOperation->ProgressSetSize(m_nPos, m_nTotalPos, FALSE);

    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::Step(
    /* [in] */ int step)
{
    if (!m_bShown)
    {
        RaiseErrorFmt(IDS_E_READONLYWHILEPROGRESSHIDDEN, L"Step");
        return SALAUT_E_READONLYWHILEPROGRESSSHOWN;
    }

    m_pOperation->ProgressAddSize(step, FALSE);

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::get_CanCancel(
    /* [retval][out] */ VARIANT_BOOL* enabled)
{
    *enabled = m_bCancelEnabled ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::put_CanCancel(
    /* [in] */ VARIANT_BOOL enabled)
{
    m_bCancelEnabled = (enabled == VARIANT_TRUE);
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::get_Style(
    /* [retval][out] */ int* barcount)
{
    *barcount = m_style;
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::put_Style(
    /* [in] */ int barcount)
{
    if (barcount != StyleOneBar && barcount != StyleTwoBar)
    {
        return E_INVALIDARG;
    }

    if (m_bShown)
    {
        RaiseErrorFmt(IDS_E_READONLYWHILEPROGRESSSHOWN, L"Style");
        return SALAUT_E_READONLYWHILEPROGRESSSHOWN;
    }

    m_style = (ProgressStyle)barcount;
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::get_Maximum(
    /* [retval][out] */ VARIANT* max)
{
    QuadWordToVariant(m_nMax, max);
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::put_Maximum(
    /* [in] */ VARIANT* max)
{
    LONGLONG val64;

    try
    {
        val64 = _variant_t(max);
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    m_nMax.SetUI64(val64);

    if (m_bShown)
    {
        if (m_style == StyleOneBar)
        {
            const CQuadWord invalid(-1, -1);
            m_pOperation->ProgressSetTotalSize(m_nMax, invalid);
        }
        else
        {
            m_pOperation->ProgressSetTotalSize(m_nMax, m_nTotalMax);
        }
    }

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::get_TotalMaximum(
    /* [retval][out] */ VARIANT* max)
{
    QuadWordToVariant(m_nTotalMax, max);
    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderProgressAutomation::put_TotalMaximum(
    /* [in] */ VARIANT* max)
{
    LONGLONG val64;

    try
    {
        val64 = _variant_t(max);
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    m_nTotalMax.SetUI64(val64);

    if (m_bShown)
    {
        if (m_style == StyleOneBar)
        {
            const CQuadWord invalid(-1, -1);
            m_pOperation->ProgressSetTotalSize(m_nMax, invalid);
        }
        else
        {
            m_pOperation->ProgressSetTotalSize(m_nMax, m_nTotalMax);
        }
    }

    return S_OK;
}
