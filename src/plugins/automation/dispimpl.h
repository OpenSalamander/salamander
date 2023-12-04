// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	dispimpl.h
	Helper template for IDispatch and IDispatchEx implementations.
*/

#pragma once

#include "unkimpl.h"
#include "saltypelib.h"
#include "raiserr.h"

#define DECLARE_DISPOBJ_NAME(name) \
    static inline LPCOLESTR GetProgId() \
    { \
        return name; \
    }

/*
	We must implement IDispatchEx for the very stupid VBScript engine.
	We used to use simple StdDispatch implementation, but VB insists
	on IDispatchEx and if it doesn't get it it simply !!!CASTS!!!
	the interface pointer to the IDispatch which is not very good
	with the aggregated StdDispatch interface and it caused strange
	crashes.
*/

template <typename T, typename Interface>
class CDispatchImpl : public Interface,
                      public IDispatchEx,
                      public ISupportErrorInfo,
                      public CUnknownImpl
{
protected:
    ITypeInfo* m_pTypeInfo;

    virtual HRESULT InvokeFilter(__out EXCEPINFO* pExcepInfo)
    {
        UNREFERENCED_PARAMETER(pExcepInfo);
        return S_OK;
    }

public:
    CDispatchImpl()
    {
        m_pTypeInfo = NULL;

        ITypeLib* pTypeLib;
        SalamanderTypeLib.Get(&pTypeLib);
        pTypeLib->GetTypeInfoOfGuid(__uuidof(Interface), &m_pTypeInfo);
        pTypeLib->Release();

        _ASSERTE(m_pTypeInfo);
    }

    ~CDispatchImpl()
    {
        if (m_pTypeInfo != NULL)
        {
            m_pTypeInfo->Release();
        }
    }

    void RaiseError(LPCOLESTR pszDescription)
    {
        T* pT = static_cast<T*>(this);
        ::RaiseError(pszDescription, __uuidof(Interface), pT->GetProgId());
    }

    /// \param nIdDescription Identifier of the string in the language
    ///        module that describes the error.
    void RaiseError(int nIdDescription)
    {
        T* pT = static_cast<T*>(this);
        ::RaiseError(nIdDescription, __uuidof(Interface), pT->GetProgId());
    }

    void RaiseErrorFmt(int nIdDescription, ...)
    {
        T* pT = static_cast<T*>(this);
        va_list ap;
        va_start(ap, nIdDescription);
        ::RaiseErrorFmtV(nIdDescription, __uuidof(Interface), pT->GetProgId(), ap);
        va_end(ap);
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
        if (IsEqualIID(iid, __uuidof(IDispatch)))
        {
            AddRef();
            *ppvObject = (IDispatch*)this;
            return S_OK;
        }
        else if (IsEqualIID(iid, __uuidof(IDispatchEx)))
        {
            AddRef();
            *ppvObject = (IDispatchEx*)this;
            return S_OK;
        }
        else if (IsEqualIID(iid, __uuidof(Interface)))
        {
            AddRef();
            *ppvObject = (Interface*)this;
            return S_OK;
        }
        else if (IsEqualIID(iid, __uuidof(ISupportErrorInfo)))
        {
            AddRef();
            *ppvObject = (ISupportErrorInfo*)this;
            return S_OK;
        }

        return __super::QueryInterface(iid, ppvObject);
    }

    // IDispatch

    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(
        /* [out] */ __RPC__out UINT* pctinfo)
    {
        *pctinfo = 1;
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(
        /* [in] */ UINT iTInfo,
        /* [in] */ LCID lcid,
        /* [out] */ __RPC__deref_out_opt ITypeInfo** ppTInfo)
    {
        UNREFERENCED_PARAMETER(lcid);

        _ASSERTE(iTInfo == 0);
        if (iTInfo != 0)
        {
            return DISP_E_BADINDEX;
        }

        *ppTInfo = m_pTypeInfo;
        m_pTypeInfo->AddRef();

        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(
        /* [in] */ __RPC__in REFIID riid,
        /* [size_is][in] */ __RPC__in_ecount_full(cNames) LPOLESTR* rgszNames,
        /* [range][in] */ __RPC__in_range(0, 16384) UINT cNames,
        /* [in] */ LCID lcid,
        /* [size_is][out] */ __RPC__out_ecount_full(cNames) DISPID* rgDispId)
    {
        UNREFERENCED_PARAMETER(riid);
        UNREFERENCED_PARAMETER(lcid);

        _ASSERTE(riid == IID_NULL);

        return DispGetIDsOfNames(m_pTypeInfo, rgszNames, cNames, rgDispId);
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
        HRESULT hr;

        UNREFERENCED_PARAMETER(lcid);

        hr = InvokeFilter(pExcepInfo);

        if (SUCCEEDED(hr))
        {
            hr = DispInvoke(
                this,
                m_pTypeInfo,
                dispIdMember,
                wFlags,
                pDispParams,
                pVarResult,
                pExcepInfo,
                puArgErr);
        }

        if (SUCCEEDED(hr) && pVarResult != NULL && V_VT(pVarResult) == VT_DISPATCH)
        {
            // it seems that std DispInvoke is not able
            // to coerce return value to proper pointer to
            // IDispatch in case of dual interface objects

            IDispatch* pdisp;
            HRESULT hrdisp;
            hrdisp = V_DISPATCH(pVarResult)->QueryInterface(&pdisp);
            _ASSERTE(SUCCEEDED(hrdisp));
            //_ASSERTE(V_DISPATCH(pVarResult) == pdisp);
            if (SUCCEEDED(hrdisp))
            {
                V_DISPATCH(pVarResult)->Release();
                V_DISPATCH(pVarResult) = pdisp;
            }
        }

        return hr;
    }

    // IDispatchEx

    virtual HRESULT STDMETHODCALLTYPE GetDispID(
        /* [in] */ __RPC__in BSTR bstrName,
        /* [in] */ DWORD grfdex,
        /* [out] */ __RPC__out DISPID* pid)
    {
        // JScript passes fdexNameEnsure for properties
        if (grfdex & ~(fdexNameCaseSensitive | fdexNameCaseInsensitive | fdexNameEnsure))
        {
            *pid = DISPID_UNKNOWN;
            return E_NOTIMPL;
        }

        return DispGetIDsOfNames(m_pTypeInfo, &bstrName, 1, pid);
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
        UNREFERENCED_PARAMETER(lcid);
        UNREFERENCED_PARAMETER(pspCaller);

        return DispInvoke(
            this,
            m_pTypeInfo,
            id,
            wFlags,
            pdp,
            pvarRes,
            pei,
            NULL);
    }

    virtual HRESULT STDMETHODCALLTYPE DeleteMemberByName(
        /* [in] */ __RPC__in BSTR bstrName,
        /* [in] */ DWORD grfdex)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE DeleteMemberByDispID(
        /* [in] */ DISPID id)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetMemberProperties(
        /* [in] */ DISPID id,
        /* [in] */ DWORD grfdexFetch,
        /* [out] */ __RPC__out DWORD* pgrfdex)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetMemberName(
        /* [in] */ DISPID id,
        /* [out] */ __RPC__deref_out_opt BSTR* pbstrName)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetNextDispID(
        /* [in] */ DWORD grfdex,
        /* [in] */ DISPID id,
        /* [out] */ __RPC__out DISPID* pid)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetNameSpaceParent(
        /* [out] */ __RPC__deref_out_opt IUnknown** ppunk)
    {
        return E_NOTIMPL;
    }

    // ISupportErrorInfo

    STDMETHOD(InterfaceSupportsErrorInfo)
    (
        REFIID iid)
    {
        if (IsEqualIID(iid, __uuidof(Interface)))
        {
            return S_OK;
        }

        return S_FALSE;
    }
};
