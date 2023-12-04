// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	itemcoll.h
	Panel items automation collection.
*/

#pragma once

#include "dispimpl.h"

class CSalamanderPanelItemCollection : public CDispatchImpl<CSalamanderPanelItemCollection, ISalamanderPanelItemCollection>
{
public:
    enum CollectionType
    {
        ItemCollection,
        SelectionCollection
    };

    static int GetCount(int nPanel, CollectionType type);

private:
    int m_nPanel;
    CollectionType m_collType;

    int GetCount()
    {
        return GetCount(m_nPanel, m_collType);
    }

public:
    DECLARE_DISPOBJ_NAME(L"Salamander.ItemCollection")

    CSalamanderPanelItemCollection(int nPanel, CollectionType type);
    ~CSalamanderPanelItemCollection();

    // ISalamanderPanelItemCollection

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Item(
        /* [in] */ VARIANT key,
        /* [retval][out] */ ISalamanderPanelItem** item);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Count(
        /* [retval][out] */ long* count);

    virtual /* [hidden][restricted][propget][id] */ HRESULT STDMETHODCALLTYPE get__NewEnum(
        /* [retval][out] */ IUnknown** ppenum);
};

class CSalamanderPanelItemEnumerator : public CUnknownImpl,
                                       public IEnumVARIANT
{
private:
    int m_nPanel;
    CSalamanderPanelItemCollection::CollectionType m_collType;
    int m_iItem;

    bool FetchItem(VARIANT* pItem);

public:
    CSalamanderPanelItemEnumerator(int nPanel, CSalamanderPanelItemCollection::CollectionType type);
    ~CSalamanderPanelItemEnumerator();

    // IUnknown

    STDMETHOD_(ULONG, AddRef)
    ()
    {
        return __super::AddRef();
    }

    STDMETHOD_(ULONG, Release)
    ()
    {
        return __super::Release();
    }

    STDMETHOD(QueryInterface)
    (REFIID iid, __out void** ppvObject);

    // IEnumVARIANT

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Next(
        /* [in] */ ULONG celt,
        /* [length_is][size_is][out] */ VARIANT* rgVar,
        /* [out] */ ULONG* pCeltFetched);

    virtual HRESULT STDMETHODCALLTYPE Skip(
        /* [in] */ ULONG celt);

    virtual HRESULT STDMETHODCALLTYPE Reset(void);

    virtual HRESULT STDMETHODCALLTYPE Clone(
        /* [out] */ __RPC__deref_out_opt IEnumVARIANT** ppenum);
};
