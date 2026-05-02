// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	waitwndaut.h
	WaitWindow automation object.
*/

#pragma once

#include "dispimpl.h"

class CSalamanderWaitWndAutomation : public CDispatchImpl<CSalamanderWaitWndAutomation, ISalamanderWaitWindow>
{
private:
    bool m_bShown;
    _bstr_t m_strTitle;
    _bstr_t m_strText;
    bool m_bCancelEnabled;
    int m_nDelay;

public:
    DECLARE_DISPOBJ_NAME(L"Salamander.WaitWindow")

    CSalamanderWaitWndAutomation();
    ~CSalamanderWaitWndAutomation();

    // ISalamanderWaitWindow

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Show(void);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Hide(void);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Title(
        /* [retval][out] */ BSTR* title);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Title(
        /* [in] */ BSTR title);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Text(
        /* [retval][out] */ BSTR* text);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Text(
        /* [in] */ BSTR text);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_IsCancelled(
        /* [retval][out] */ VARIANT_BOOL* cancelled);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_CanCancel(
        /* [retval][out] */ VARIANT_BOOL* enabled);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_CanCancel(
        /* [in] */ VARIANT_BOOL enabled);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Delay(
        /* [retval][out] */ int* ms);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Delay(
        /* [in] */ int ms);
};
