// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	itemcoll.cpp
	Panel items automation collection.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "itemaut.h"
#include "itemcoll.h"

extern CSalamanderGeneralAbstract* SalamanderGeneral;

CSalamanderPanelItemCollection::CSalamanderPanelItemCollection(
    int nPanel,
    CollectionType type)
{
    m_nPanel = nPanel;
    m_collType = type;
}

CSalamanderPanelItemCollection::~CSalamanderPanelItemCollection()
{
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelItemCollection::get_Item(
    /* [in] */ VARIANT key,
    /* [retval][out] */ ISalamanderPanelItem** item)
{
    HRESULT hr = S_OK;
    int iKey;
    const CFileData* pData;
    int i = 0;

    _ASSERTE(item);

    try
    {
        iKey = _variant_t(key);
    }
    catch (_com_error& e)
    {
        hr = e.Error();
    }

    if (FAILED(hr))
    {
        return hr;
    }

    if (iKey < 0)
    {
        return DISP_E_BADINDEX;
    }

    do
    {
        switch (m_collType)
        {
        case ItemCollection:
            pData = SalamanderGeneral->GetPanelItem(m_nPanel, &i, NULL);
            break;

        case SelectionCollection:
            pData = SalamanderGeneral->GetPanelSelectedItem(m_nPanel, &i, NULL);
            break;

        default:
            _ASSERTE(0);
            return E_FAIL;
        }

        if (pData == NULL)
        {
            return DISP_E_BADINDEX;
        }
    } while (iKey--);

    _ASSERTE(pData);

    *item = new CSalamanderPanelItemAutomation(pData, m_nPanel);

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelItemCollection::get_Count(
    /* [retval][out] */ long* count)
{
    *count = GetCount();
    return S_OK;
}

/* [hidden][restricted][propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelItemCollection::get__NewEnum(
    /* [retval][out] */ IUnknown** ppenum)
{
    *ppenum = (IEnumVARIANT*)new CSalamanderPanelItemEnumerator(m_nPanel, m_collType);
    return S_OK;
}

/* static */ int CSalamanderPanelItemCollection::GetCount(int nPanel, CollectionType type)
{
    int count = 0;

    switch (type)
    {
    case ItemCollection:
    {
        // TODO: there must be better way to do this
        int i = 0;
        while (SalamanderGeneral->GetPanelItem(nPanel, &i, NULL))
        {
            ++count;
        }
        break;
    }

    case SelectionCollection:
    {
        int files, dirs;
        if (SalamanderGeneral->GetPanelSelection(nPanel, &files, &dirs))
        {
            count = files + dirs;
        }
        break;
    }

    default:
        _ASSERTE(0);
        break;
    }

    return count;
}

////////////////////////////////////////////////////////////////////////////////

CSalamanderPanelItemEnumerator::CSalamanderPanelItemEnumerator(
    int nPanel,
    CSalamanderPanelItemCollection::CollectionType type)
{
    m_nPanel = nPanel;
    m_collType = type;
    m_iItem = 0;
}

CSalamanderPanelItemEnumerator::~CSalamanderPanelItemEnumerator()
{
}

STDMETHODIMP CSalamanderPanelItemEnumerator::QueryInterface(REFIID iid, __out void** ppvObject)
{
    if (IsEqualIID(iid, __uuidof(IEnumVARIANT)))
    {
        AddRef();
        *ppvObject = (IEnumVARIANT*)this;
        return S_OK;
    }

    return __super::QueryInterface(iid, ppvObject);
}

/* [local] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelItemEnumerator::Next(
    /* [in] */ ULONG celt,
    /* [length_is][size_is][out] */ VARIANT* rgVar,
    /* [out] */ ULONG* pCeltFetched)
{
    if (pCeltFetched)
    {
        *pCeltFetched = 0;
    }

    for (; celt; celt--)
    {
        if (FetchItem(rgVar))
        {
            if (pCeltFetched)
            {
                ++(*pCeltFetched);
            }
            rgVar++;
        }
        else
        {
            return S_FALSE;
        }
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CSalamanderPanelItemEnumerator::Skip(
    /* [in] */ ULONG celt)
{
    _ASSERTE(0);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CSalamanderPanelItemEnumerator::Reset(void)
{
    m_iItem = 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CSalamanderPanelItemEnumerator::Clone(
    /* [out] */ __RPC__deref_out_opt IEnumVARIANT** ppenum)
{
    _ASSERTE(0);
    return E_NOTIMPL;
}

bool CSalamanderPanelItemEnumerator::FetchItem(VARIANT* pItem)
{
    const CFileData* pData = NULL;
    HRESULT hr;

    switch (m_collType)
    {
    case CSalamanderPanelItemCollection::ItemCollection:
        pData = SalamanderGeneral->GetPanelItem(m_nPanel, &m_iItem, FALSE);
        break;

    case CSalamanderPanelItemCollection::SelectionCollection:
        pData = SalamanderGeneral->GetPanelSelectedItem(m_nPanel, &m_iItem, NULL);
        break;

    default:
        _ASSERTE(0);
        break;
    }

    if (pData == NULL)
    {
        return false;
    }

    IUnknown* pItemUnk = (ISalamanderPanelItem*)new CSalamanderPanelItemAutomation(pData, m_nPanel);
    IDispatch* pItemDisp;

    hr = pItemUnk->QueryInterface(&pItemDisp);
    _ASSERTE(SUCCEEDED(hr));

    V_VT(pItem) = VT_DISPATCH;
    V_DISPATCH(pItem) = pItemDisp;

    pItemUnk->Release();

    return true;
}
