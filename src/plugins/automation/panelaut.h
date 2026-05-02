// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	panelaut.h
	Panel automation object.
*/

#pragma once

#include "dispimpl.h"
#include "itemaut.h"

class CSalamanderPanelAutomation : public CDispatchImpl<CSalamanderPanelAutomation, ISalamanderPanel>
{
private:
    int m_nPanel; // PANEL_LEFT/PANEL_RIGHT
    CSalamanderPanelItemAutomation* m_pFocusedItem;

    // Raises error based on the CHPPFR_xxx error code returned
    // from ChangePanelPath method.
    HRESULT RaiseChPPErr(int nErr);

public:
    DECLARE_DISPOBJ_NAME(L"Salamander.Panel")

    CSalamanderPanelAutomation(int nPanel);
    ~CSalamanderPanelAutomation();

    // ISalamanderPanel

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Path(
        /* [in] */ BSTR path);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Path(
        /* [retval][out] */ BSTR* path);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_FocusedItem(
        /* [retval][out] */ ISalamanderPanelItem** item);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_SelectedItems(
        /* [retval][out] */ ISalamanderPanelItemCollection** coll);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Items(
        /* [retval][out] */ ISalamanderPanelItemCollection** coll);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE SelectAll(void);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE DeselectAll(
        /* [optional][in] */ VARIANT* save);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_PathType(
        /* [retval][out] */ int* type);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE StoreSelection(void);

    virtual /* [hidden][restricted][local][id] */ int STDMETHODCALLTYPE _GetPanelIndex(void);
};
