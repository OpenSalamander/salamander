// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	progressaut.h
	ProgressDialog automation object.
*/

#pragma once

#include "dispimpl.h"

class CSalamanderProgressAutomation : public CDispatchImpl<CSalamanderProgressAutomation, ISalamanderProgressDialog>
{
public:
    enum ProgressStyle
    {
        StyleOneBar = 1,
        StyleTwoBar = 2
    };

private:
    ProgressStyle m_style;
    bool m_bShown;
    _bstr_t m_strTitle;
    CSalamanderForOperationsAbstract* m_pOperation;
    CQuadWord m_nMax;
    CQuadWord m_nTotalMax;
    CQuadWord m_nPos;
    CQuadWord m_nTotalPos;
    bool m_bCancelEnabled;

public:
    DECLARE_DISPOBJ_NAME(L"Salamander.ProgressDialog")

    CSalamanderProgressAutomation(CSalamanderForOperationsAbstract* pOperation);
    ~CSalamanderProgressAutomation();

    // ISalamanderProgressDialog

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Show(void);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Hide(void);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Title(
        /* [retval][out] */ BSTR* title);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Title(
        /* [in] */ BSTR title);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE AddText(
        /* [in] */ BSTR text);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_IsCancelled(
        /* [retval][out] */ VARIANT_BOOL* cancelled);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Position(
        /* [retval][out] */ VARIANT* progress);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Position(
        /* [in] */ VARIANT* progress);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_TotalPosition(
        /* [retval][out] */ VARIANT* progress);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_TotalPosition(
        /* [in] */ VARIANT* progress);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Step(
        /* [in] */ int step);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_CanCancel(
        /* [retval][out] */ VARIANT_BOOL* enabled);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_CanCancel(
        /* [in] */ VARIANT_BOOL enabled);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Style(
        /* [retval][out] */ int* barcount);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Style(
        /* [in] */ int barcount);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Maximum(
        /* [retval][out] */ VARIANT* max);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Maximum(
        /* [in] */ VARIANT* max);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_TotalMaximum(
        /* [retval][out] */ VARIANT* max);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_TotalMaximum(
        /* [in] */ VARIANT* max);
};
