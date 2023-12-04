// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guicontainer.h
	Implements the component container class.
*/

#pragma once

class CSalamanderGuiContainerBase
{
protected:
    enum
    {
        DISPID_FIRST_MEMBER = 1024
    };

    struct MEMBER
    {
        DISPID dispid;
        BSTR name;

        MEMBER()
        {
            dispid = 0;
            name = NULL;
        }

        MEMBER(DISPID _dispid)
        {
            dispid = _dispid;
            name = NULL;
        }

        ~MEMBER()
        {
            if (name != NULL)
            {
                SysFreeString(name);
            }
        }

        /// Copy constructor.
        MEMBER(const MEMBER& m)
        {
            dispid = m.dispid;
            _ASSERTE(m.name == NULL);
            name = NULL;
        }

        /// Not implemented to prevent use.
        MEMBER& operator=(const MEMBER&);
    };

    enum LookupResult
    {
        /// The member was not found.
        LookupNotFound,

        /// The regular member exists.
        LookupOk,

        /// The member exists, but it's not regular member.
        LookupTemporary
    };

    DISPID m_dispidCounter;

    /// List of members temporarily created during GetIDsOfNames.
    /// Once the member is assigned a value (prop put Invoke is called
    /// for the member in this list), the member gets really created.
    TDirectArray<MEMBER> m_tempMembers;

    HRESULT ContainerGetDispIDs(
        __in_ecount(cNames) LPOLESTR* rgszNames,
        UINT cNames,
        __out_ecount(cNames) DISPID* pdispid);

    HRESULT ContainerInvoke(
        __in DISPID dispid,
        __in WORD wFlags,
        __in DISPPARAMS* pdp,
        __out_opt VARIANT* pvarRes,
        __out_opt EXCEPINFO* pei);

    LookupResult LookupMember(
        LPCOLESTR pszName,
        __out DISPID* dispid,
        __out_opt ISalamanderGuiComponentInternal** ppComponent = NULL);

    LookupResult LookupMember(
        DISPID dispid,
        __out ISalamanderGuiComponentInternal** ppComponent,
        __out_opt int* pTempMemberIndex = NULL);

    /// Inserts temporary member.
    /// \param pszName Name of the member.
    /// \return Returs DISPID assigned to the member.
    DISPID InsertMember(LPCOLESTR pszName);

    HRESULT ContainerGetMember(
        __in DISPID dispid,
        __in DISPPARAMS* pdp,
        __out_opt VARIANT* pvarRes,
        __out_opt EXCEPINFO* pei);

    HRESULT ContainerPutMember(
        __in DISPID dispid,
        __in DISPPARAMS* pdp,
        __out_opt VARIANT* pvarRes,
        __out_opt EXCEPINFO* pei);

    void CreateChildren();

    virtual LPCOLESTR GetContainerProgId() = 0;
    virtual ISalamanderGuiComponentInternal* GetChildNoAddRef() = 0;
    virtual void InternalAddComponent(ISalamanderGuiComponentInternal* pComponent) = 0;
    virtual void OnChildCreated(ISalamanderGuiComponentInternal* pComponent) {}

public:
    CSalamanderGuiContainerBase();
    ~CSalamanderGuiContainerBase();
};

template <typename T, typename Interface>
class CSalamanderGuiContainerImpl : public CSalamanderGuiContainerBase,
                                    public CSalamanderGuiComponentImpl<T, Interface>
{
protected:
    virtual LPCOLESTR GetContainerProgId()
    {
        return T::GetProgId();
    }

    /// Implemented - overriden.
    virtual ISalamanderGuiComponentInternal* GetChildNoAddRef()
    {
        return this->m_pChild;
    }

    void InternalAddComponent(ISalamanderGuiComponentInternal* pComponent)
    {
        this->AddComponent(pComponent);
    }

    /// Overriden - inherited from CUnknownImpl.
    void FinalRelease()
    {
        ISalamanderGuiComponentInternal* pChild = this->m_pChild;

        while (pChild)
        {
            ISalamanderGuiComponentInternal* pTmp;
            pChild->GetSibling(&pTmp);

            this->RemoveComponent(pChild);

            pChild = pTmp;
        }
    }

public:
    CSalamanderGuiContainerImpl<T, Interface>(__in_opt VARIANT* text = NULL) : CSalamanderGuiComponentImpl<T, Interface>(text)
    {
    }

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
    (REFIID iid, __out void** ppvObject)
    {
        if (IsEqualIID(iid, __uuidof(ISalamanderGuiContainer)))
        {
            AddRef();
            *ppvObject = (ISalamanderGuiContainer*)this;
            return S_OK;
        }

        return __super::QueryInterface(iid, ppvObject);
    }

    // IDispatch

    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(
        /* [in] */ __RPC__in REFIID riid,
        /* [size_is][in] */ __RPC__in_ecount_full(cNames) LPOLESTR* rgszNames,
        /* [range][in] */ __RPC__in_range(0, 16384) UINT cNames,
        /* [in] */ LCID lcid,
        /* [size_is][out] */ __RPC__out_ecount_full(cNames) DISPID* rgDispId)
    {
        HRESULT hr;

        if (!IsEqualIID(riid, IID_NULL))
        {
            _ASSERTE(0);
            return E_INVALIDARG;
        }

        hr = __super::GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);
        if (SUCCEEDED(hr))
        {
            return hr;
        }

        return ContainerGetDispIDs(rgszNames, cNames, rgDispId);
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Invoke(
        /* [in] */ DISPID dispIdMember,
        /* [in] */ REFIID riid,
        /* [in] */ LCID lcid,
        /* [in] */ WORD wFlags,
        /* [out][in] */ DISPPARAMS* pDispParams,
        /* [out] */ VARIANT* pVarResult,
        /* [out] */ EXCEPINFO* pExcepInfo,
        /* [out] */ UINT* puArgErr)
    {
        if (dispIdMember < DISPID_FIRST_MEMBER)
        {
            return __super::Invoke(
                dispIdMember,
                riid,
                lcid,
                wFlags,
                pDispParams,
                pVarResult,
                pExcepInfo,
                puArgErr);
        }

        return ContainerInvoke(dispIdMember, wFlags, pDispParams,
                               pVarResult, pExcepInfo);
    }

    // IDispatchEx

    virtual HRESULT STDMETHODCALLTYPE GetDispID(
        /* [in] */ __RPC__in BSTR bstrName,
        /* [in] */ DWORD grfdex,
        /* [out] */ __RPC__out DISPID* pid)
    {
        HRESULT hr;

        hr = __super::GetDispID(bstrName, grfdex, pid);
        if (SUCCEEDED(hr))
        {
            return hr;
        }

        return ContainerGetDispIDs(&bstrName, 1, pid);
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE InvokeEx(
        /* [annotation][in] */
        __in DISPID id,
        /* [annotation][in] */
        __in LCID lcid,
        /* [annotation][in] */
        __in WORD wFlags,
        /* [annotation][in] */
        __in DISPPARAMS* pdp,
        /* [annotation][out] */
        __out_opt VARIANT* pvarRes,
        /* [annotation][out] */
        __out_opt EXCEPINFO* pei,
        /* [annotation][unique][in] */
        __in_opt IServiceProvider* pspCaller)
    {
        if (id < DISPID_FIRST_MEMBER)
        {
            return __super::InvokeEx(
                id,
                lcid,
                wFlags,
                pdp,
                pvarRes,
                pei,
                pspCaller);
        }

        return ContainerInvoke(id, wFlags, pdp,
                               pvarRes, pei);
    }
};
