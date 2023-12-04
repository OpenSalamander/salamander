// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	scriptsite.cpp
	Implements the site for the script engine.
*/

#include "precomp.h"
#include "scriptlist.h"
#include "scriptsite.h"
#include "saltypelib.h"
#include "salamander_h.h"
#include "salamanderaut.h"
#include "aututils.h"
#include "abortpalette.h"
#include "shim.h"
#include "lang\lang.rh"

extern HINSTANCE g_hLangInst;

CScriptSite::CScriptSite(CScriptInfo* pChief)
{
    m_pChief = pChief;
    m_pSalamander = new CSalamanderAutomation(pChief);
    m_pExecInfo = NULL;
}

CScriptSite::~CScriptSite()
{
    m_pSalamander->Release();
}

STDMETHODIMP CScriptSite::QueryInterface(REFIID iid, __out void** ppvObject)
{
    if (IsEqualIID(iid, __uuidof(IActiveScriptSite)))
    {
        AddRef();
        *ppvObject = (IActiveScriptSite*)this;
        return S_OK;
    }
    if (IsEqualIID(iid, __uuidof(IActiveScriptSiteWindow)))
    {
        AddRef();
        *ppvObject = (IActiveScriptSiteWindow*)this;
        return S_OK;
    }
    else if (IsEqualIID(iid, __uuidof(IActiveScriptSiteDebug32)))
    {
        AddRef();
        *ppvObject = (IActiveScriptSiteDebug32*)this;
        return S_OK;
    }
    else if (IsEqualIID(iid, __uuidof(IActiveScriptSiteDebug64)))
    {
        AddRef();
        *ppvObject = (IActiveScriptSiteDebug64*)this;
        return S_OK;
    }
    else if (IsEqualIID(iid, __uuidof(IDebugDocumentHost)))
    {
        AddRef();
        *ppvObject = (IDebugDocumentHost*)this;
        return S_OK;
    }
    else if (IsEqualIID(iid, __uuidof(ICanHandleException)))
    {
        AddRef();
        *ppvObject = (ICanHandleException*)this;
        return S_OK;
    }
    else if (IsEqualIID(iid, __uuidof(IActiveScriptSiteInterruptPoll)))
    {
        AddRef();
        *ppvObject = (IActiveScriptSiteInterruptPoll*)this;
        return S_OK;
    }

    // IServiceProvider

#if defined(TRACE_ENABLE) && defined(_DEBUG)
    if (!IsEqualIID(iid, __uuidof(IUnknown)))
    {
        OLECHAR szIID[64];
        StringFromGUID2(iid, szIID, _countof(szIID));
        TRACE_I("CScriptSite::QueryInterface: unknown interface " << OLE2A(szIID));
    }
#endif

    return __super::QueryInterface(iid, ppvObject);
}

HRESULT CScriptSite::GetLCID(
    /* [out] */ __RPC__out LCID* plcid)
{
    return E_NOTIMPL;
}

HRESULT CScriptSite::GetItemInfo(
    /* [in] */ __RPC__in LPCOLESTR pstrName,
    /* [in] */ DWORD dwReturnMask,
    /* [out] */ __RPC__deref_out_opt IUnknown** ppiunkItem,
    /* [out] */ __RPC__deref_out_opt ITypeInfo** ppti)
{
    HRESULT hr = S_OK;

    if (dwReturnMask & SCRIPTINFO_ITYPEINFO)
    {
        if (!ppti)
        {
            return E_INVALIDARG;
        }
        *ppti = NULL;
    }

    if (dwReturnMask & SCRIPTINFO_IUNKNOWN)
    {
        if (!ppiunkItem)
        {
            return E_INVALIDARG;
        }
        *ppiunkItem = NULL;
    }

    // Global object
    if (_wcsicmp(L"Salamander", pstrName) == 0)
    {
        if (dwReturnMask & SCRIPTINFO_ITYPEINFO)
        {
            IDispatch* pDispatch;

            hr = m_pSalamander->QueryInterface(IID_IDispatch, (void**)&pDispatch);
            if (SUCCEEDED(hr))
            {
                pDispatch->GetTypeInfo(0, 0, ppti);
                pDispatch->Release();
            }
        }

        if (dwReturnMask & SCRIPTINFO_IUNKNOWN)
        {
            hr = m_pSalamander->QueryInterface(IID_IUnknown, (void**)ppiunkItem);
        }

        return hr;
    }

    return E_INVALIDARG;
}

HRESULT CScriptSite::GetDocVersionString(
    /* [out] */ __RPC__deref_out_opt BSTR* pbstrVersion)
{
    return E_NOTIMPL;
}

HRESULT CScriptSite::OnScriptTerminate(
    /* [in] */ __RPC__in const VARIANT* pvarResult,
    /* [in] */ __RPC__in const EXCEPINFO* pexcepinfo)
{
    return S_OK;
}

HRESULT CScriptSite::OnStateChange(
    /* [in] */ SCRIPTSTATE ssScriptState)
{
#if defined(TRACE_ENABLE)
    static const char* const s_apszStateNames[] =
        {
            "UNINITIALIZED",
            "STARTED",
            "CONNECTED",
            "DISCONNECTED",
            "CLOSED",
            "INITIALIZED"};

    if (ssScriptState < _countof(s_apszStateNames))
    {
        TRACE_I("CScriptSite::OnStateChange(SCRIPTSTATE_" << s_apszStateNames[ssScriptState] << ")");
    }
    else
    {
        TRACE_I("CScriptSite::OnStateChange(" << (int)ssScriptState << ")");
    }
#endif

    return S_OK;
}

HRESULT CScriptSite::OnScriptError(
    /* [in] */ __RPC__in_opt IActiveScriptError* pScriptError)
{
    // May be NULL if the error raises during the parsing phase.
    if (m_pExecInfo != NULL)
    {
        // The abort palette is no longer needed, hide it now.
        delete m_pExecInfo->pAbortPalette;
        m_pExecInfo->pAbortPalette = NULL;
    }

    EXCEPINFO ei = {
        0,
    };
    if (FAILED(pScriptError->GetExceptionInfo(&ei)))
    {
        m_pChief->SetHardError(pScriptError);
        return E_FAIL;
    }

    if (ei.scode == CTL_E_OUTOFSTACKSPACE || ei.scode == CTL_E_OUTOFMEMORY ||
        DisplayException(pScriptError, m_pChief->GetShim(), false) < 0)
    {
        m_pChief->SetHardError(pScriptError);
    }

    FreeException(ei);

    // Never ever try to return anything else than S_OK here.
    // If JScript.dll returns CTL_E_OUTOFMEMORY because of infinite
    // recursion (see Redmine #782 bug) it keeps calling OnScriptError
    // again and again if we return anything else than S_OK.

    return S_OK;
}

HRESULT CScriptSite::OnEnterScript(void)
{
    CScriptEngineShim* pShim = m_pChief->GetShim();
    if (pShim != NULL)
    {
        pShim->Site_OnEnterScript();
    }

    return S_OK;
}

HRESULT CScriptSite::OnLeaveScript(void)
{
    CScriptEngineShim* pShim = m_pChief->GetShim();
    if (pShim != NULL)
    {
        pShim->Site_OnLeaveScript();
    }

    return S_OK;
}

void CScriptSite::SetExecutionInfo(CScriptInfo::EXECUTION_INFO* info)
{
    m_pExecInfo = info;
    m_pSalamander->SetExecutionInfo(info);
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetDocumentContextFromPosition(
    /* [in] */ DWORDLONG dwSourceContext,
    /* [in] */ ULONG uCharacterOffset,
    /* [in] */ ULONG uNumChars,
    /* [out] */ IDebugDocumentContext** ppsc)
{
    HRESULT hr = E_NOTIMPL;
    CScriptInfo::DEBUG_INFO* pdi = m_pChief->GetDebugInfo();

    TRACE_I("IActiveScriptSiteDebug64::GetDocumentContextFromPosition");

    if (pdi->pDbgDocHelper != NULL)
    {
        ULONG ulStartPos = 0;

        pdi->pDbgDocHelper->GetScriptBlockInfo((DWORD)dwSourceContext, // FIXME_X64 potlacen warning C4244: 'argument' : conversion from 'DWORDLONG' to 'DWORD', possible loss of data
                                               NULL, &ulStartPos, NULL);
        hr = pdi->pDbgDocHelper->CreateDebugDocumentContext(
            ulStartPos + uCharacterOffset, uNumChars, ppsc);
    }

    if (FAILED(hr))
    {
        TRACE_E("IDebugDocumentContext not available");
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetDocumentContextFromPosition(
    /* [in] */ DWORD dwSourceContext,
    /* [in] */ ULONG uCharacterOffset,
    /* [in] */ ULONG uNumChars,
    /* [out] */ IDebugDocumentContext** ppsc)
{
    TRACE_I("IActiveScriptSiteDebug32::GetDocumentContextFromPosition");

    DWORDLONG dwSourceContext64 = dwSourceContext;

    return GetDocumentContextFromPosition(
        dwSourceContext64,
        uCharacterOffset,
        uNumChars,
        ppsc);
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetApplication(
    /* [out] */ IDebugApplication32** ppda)
{
    HRESULT hr = E_NOTIMPL;
    CScriptInfo::DEBUG_INFO* pdi = m_pChief->GetDebugInfo();

    TRACE_I("IActiveScriptSiteDebug32::GetApplication");

    if (pdi->pDbgApp != NULL)
    {
#ifdef _WIN64
#pragma message("Unimplemented IActiveScriptSiteDebug32::GetApplication")
#else
        *ppda = pdi->pDbgApp;
        pdi->pDbgApp->AddRef();
        hr = S_OK;
#endif
    }

    if (FAILED(hr))
    {
        TRACE_I("IDebugApplication not available");
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetApplication(
    /* [out] */ IDebugApplication64** ppda)
{
    HRESULT hr = E_NOTIMPL;
    CScriptInfo::DEBUG_INFO* pdi = m_pChief->GetDebugInfo();

    TRACE_I("IActiveScriptSiteDebug64::GetApplication");

    if (pdi->pDbgApp != NULL)
    {
#ifndef _WIN64
#pragma message("Unimplemented IActiveScriptSiteDebug64::GetApplication")
#else
        *ppda = pdi->pDbgApp;
        pdi->pDbgApp->AddRef();
        hr = S_OK;
#endif
    }

    if (FAILED(hr))
    {
        TRACE_I("IDebugApplication not available");
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetRootApplicationNode(
    /* [out] */ IDebugApplicationNode** ppdanRoot)
{
    HRESULT hr = E_NOTIMPL;
    CScriptInfo::DEBUG_INFO* pdi = m_pChief->GetDebugInfo();

    TRACE_I("IActiveScriptSiteDebug::GetRootApplicationNode");

    if (pdi->pDbgDocHelper != NULL)
    {
        hr = pdi->pDbgDocHelper->GetDebugApplicationNode(ppdanRoot);
    }

    if (FAILED(hr))
    {
        TRACE_E("IDebugApplicationNode not available");
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE CScriptSite::OnScriptErrorDebug(
    /* [in] */ IActiveScriptErrorDebug* pErrorDebug,
    /* [out] */ BOOL* pfEnterDebugger,
    /* [out] */ BOOL* pfCallOnScriptErrorWhenContinuing)
{
    // May be NULL if the error raises during the parsing phase.
    if (m_pExecInfo != NULL)
    {
        // The abort palette is no longer needed, hide it now.
        delete m_pExecInfo->pAbortPalette;
        m_pExecInfo->pAbortPalette = NULL;
    }

    // safe defaults
    *pfEnterDebugger = FALSE;
    *pfCallOnScriptErrorWhenContinuing = TRUE;

    EXCEPINFO ei = {
        0,
    };
    if (FAILED(pErrorDebug->GetExceptionInfo(&ei)))
    {
        return S_OK;
    }

    if (ei.scode == CTL_E_OUTOFSTACKSPACE || ei.scode == CTL_E_OUTOFMEMORY)
    {
        *pfCallOnScriptErrorWhenContinuing = TRUE;
    }
    else
    {
        if (DisplayException(pErrorDebug, m_pChief->GetShim(), true))
        {
            *pfEnterDebugger = TRUE;
        }
        *pfCallOnScriptErrorWhenContinuing = FALSE;
    }

    FreeException(ei);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetWindow(
    /* [out] */ __RPC__deref_out_opt HWND* phwnd)
{
    *phwnd = SalamanderGeneral->GetMsgBoxParent();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CScriptSite::EnableModeless(
    /* [in] */ BOOL fEnable)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetDeferredText(
    /* [in] */ DWORD dwTextStartCookie,
    /* [size_is][length_is][out][in] */ __RPC__inout_ecount_part(cMaxChars, *pcNumChars) WCHAR* pcharText,
    /* [size_is][length_is][out][in] */ __RPC__inout_ecount_part(cMaxChars, *pcNumChars) SOURCE_TEXT_ATTR* pstaTextAttr,
    /* [out][in] */ __RPC__inout ULONG* pcNumChars,
    /* [in] */ ULONG cMaxChars)
{
    TRACE_I("IDebugDocumentHost::GetDeferredText");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetScriptTextAttributes(
    /* [size_is][in] */ __RPC__in_ecount_full(uNumCodeChars) LPCOLESTR pstrCode,
    /* [in] */ ULONG uNumCodeChars,
    /* [in] */ __RPC__in LPCOLESTR pstrDelimiter,
    /* [in] */ DWORD dwFlags,
    /* [size_is][out][in] */ __RPC__inout_ecount_full(uNumCodeChars) SOURCE_TEXT_ATTR* pattr)
{
    TRACE_I("IDebugDocumentHost::GetScriptTextAttributes");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CScriptSite::OnCreateDocumentContext(
    /* [out] */ __RPC__deref_out_opt IUnknown** ppunkOuter)
{
    TRACE_I("IDebugDocumentHost::OnCreateDocumentContext");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetPathName(
    /* [out] */ __RPC__deref_out_opt BSTR* pbstrLongName,
    /* [out] */ __RPC__out BOOL* pfIsOriginalFile)
{
    TRACE_I("IDebugDocumentHost::GetPathName");
    *pbstrLongName = SysAllocString(A2OLE(m_pChief->GetFileName()));
    *pfIsOriginalFile = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CScriptSite::GetFileName(
    /* [out] */ __RPC__deref_out_opt BSTR* pbstrShortName)
{
    TRACE_I("IDebugDocumentHost::GetFileName");
    *pbstrShortName = SysAllocString(A2OLE(PathFindFileName(m_pChief->GetFileName())));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CScriptSite::NotifyChanged(void)
{
    TRACE_I("IDebugDocumentHost::NotifyChanged");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CScriptSite::CanHandleException(
    __in EXCEPINFO* pExcepInfo,
    __in_opt VARIANT* pvar)
{
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE CScriptSite::QueryContinue(void)
{
    //TRACE_I("IActiveScriptSiteInterruptPoll::QueryContinue");

    // Should the script continue (S_OK) or not (S_FALSE)?
    _ASSERTE(m_pChief != NULL);
    return m_pChief->IsAbortPending() ? S_FALSE : S_OK;
}

int CScriptSite::DisplayException(IActiveScriptError* pError, class CScriptEngineShim* pShim, bool bDebug)
{
    m_pChief->SetSiteError(pError);
    return ::DisplayException(pError, pShim, bDebug);
}
