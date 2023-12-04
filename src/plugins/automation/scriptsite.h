// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	scriptsite.h
	Implements the site for the script engine.
*/

#pragma once

class CScriptSite : public IActiveScriptSite,
                    public IActiveScriptSiteWindow,
                    public IActiveScriptSiteDebug32,
                    public IActiveScriptSiteDebug64,
                    public IActiveScriptSiteInterruptPoll,
                    public IDebugDocumentHost,
                    public ICanHandleException,
                    public CUnknownImpl
{
protected:
    class CSalamanderAutomation* m_pSalamander;
    CScriptInfo* m_pChief;
    CScriptInfo::EXECUTION_INFO* m_pExecInfo;

    int DisplayException(IActiveScriptError* pError, class CScriptEngineShim* pShim, bool bDebug);

public:
    CScriptSite(CScriptInfo* pChief);
    ~CScriptSite();

    void SetExecutionInfo(CScriptInfo::EXECUTION_INFO* info);

    // IUnknown

    STDMETHOD(QueryInterface)
    (REFIID iid, __out void** ppvObject);

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

    // IActiveScriptSite

    virtual HRESULT STDMETHODCALLTYPE GetLCID(
        /* [out] */ __RPC__out LCID* plcid);

    virtual HRESULT STDMETHODCALLTYPE GetItemInfo(
        /* [in] */ __RPC__in LPCOLESTR pstrName,
        /* [in] */ DWORD dwReturnMask,
        /* [out] */ __RPC__deref_out_opt IUnknown** ppiunkItem,
        /* [out] */ __RPC__deref_out_opt ITypeInfo** ppti);

    virtual HRESULT STDMETHODCALLTYPE GetDocVersionString(
        /* [out] */ __RPC__deref_out_opt BSTR* pbstrVersion);

    virtual HRESULT STDMETHODCALLTYPE OnScriptTerminate(
        /* [in] */ __RPC__in const VARIANT* pvarResult,
        /* [in] */ __RPC__in const EXCEPINFO* pexcepinfo);

    virtual HRESULT STDMETHODCALLTYPE OnStateChange(
        /* [in] */ SCRIPTSTATE ssScriptState);

    virtual HRESULT STDMETHODCALLTYPE OnScriptError(
        /* [in] */ __RPC__in_opt IActiveScriptError* pscripterror);

    virtual HRESULT STDMETHODCALLTYPE OnEnterScript(void);

    virtual HRESULT STDMETHODCALLTYPE OnLeaveScript(void);

    // IActiveScriptSiteWindow

    virtual HRESULT STDMETHODCALLTYPE GetWindow(
        /* [out] */ __RPC__deref_out_opt HWND* phwnd);

    virtual HRESULT STDMETHODCALLTYPE EnableModeless(
        /* [in] */ BOOL fEnable);

    // IActiveScriptSiteDebug32

    virtual HRESULT STDMETHODCALLTYPE GetDocumentContextFromPosition(
        /* [in] */ DWORD dwSourceContext,
        /* [in] */ ULONG uCharacterOffset,
        /* [in] */ ULONG uNumChars,
        /* [out] */ IDebugDocumentContext** ppsc);

    virtual HRESULT STDMETHODCALLTYPE GetApplication(
        /* [out] */ IDebugApplication32** ppda);

    virtual HRESULT STDMETHODCALLTYPE GetRootApplicationNode(
        /* [out] */ IDebugApplicationNode** ppdanRoot);

    virtual HRESULT STDMETHODCALLTYPE OnScriptErrorDebug(
        /* [in] */ IActiveScriptErrorDebug* pErrorDebug,
        /* [out] */ BOOL* pfEnterDebugger,
        /* [out] */ BOOL* pfCallOnScriptErrorWhenContinuing);

    // IActiveScriptSiteDebug64

    virtual HRESULT STDMETHODCALLTYPE GetDocumentContextFromPosition(
        /* [in] */ DWORDLONG dwSourceContext,
        /* [in] */ ULONG uCharacterOffset,
        /* [in] */ ULONG uNumChars,
        /* [out] */ IDebugDocumentContext** ppsc);

    virtual HRESULT STDMETHODCALLTYPE GetApplication(
        /* [out] */ IDebugApplication64** ppda);

    //virtual HRESULT STDMETHODCALLTYPE GetRootApplicationNode(
    //	/* [out] */ IDebugApplicationNode **ppdanRoot);

    //virtual HRESULT STDMETHODCALLTYPE OnScriptErrorDebug(
    //	/* [in] */ IActiveScriptErrorDebug *pErrorDebug,
    //	/* [out] */ BOOL *pfEnterDebugger,
    //	/* [out] */ BOOL *pfCallOnScriptErrorWhenContinuing);

    // IDebugDocumentHost

    virtual HRESULT STDMETHODCALLTYPE GetDeferredText(
        /* [in] */ DWORD dwTextStartCookie,
        /* [size_is][length_is][out][in] */ __RPC__inout_ecount_part(cMaxChars, *pcNumChars) WCHAR* pcharText,
        /* [size_is][length_is][out][in] */ __RPC__inout_ecount_part(cMaxChars, *pcNumChars) SOURCE_TEXT_ATTR* pstaTextAttr,
        /* [out][in] */ __RPC__inout ULONG* pcNumChars,
        /* [in] */ ULONG cMaxChars);

    virtual HRESULT STDMETHODCALLTYPE GetScriptTextAttributes(
        /* [size_is][in] */ __RPC__in_ecount_full(uNumCodeChars) LPCOLESTR pstrCode,
        /* [in] */ ULONG uNumCodeChars,
        /* [in] */ __RPC__in LPCOLESTR pstrDelimiter,
        /* [in] */ DWORD dwFlags,
        /* [size_is][out][in] */ __RPC__inout_ecount_full(uNumCodeChars) SOURCE_TEXT_ATTR* pattr);

    virtual HRESULT STDMETHODCALLTYPE OnCreateDocumentContext(
        /* [out] */ __RPC__deref_out_opt IUnknown** ppunkOuter);

    virtual HRESULT STDMETHODCALLTYPE GetPathName(
        /* [out] */ __RPC__deref_out_opt BSTR* pbstrLongName,
        /* [out] */ __RPC__out BOOL* pfIsOriginalFile);

    virtual HRESULT STDMETHODCALLTYPE GetFileName(
        /* [out] */ __RPC__deref_out_opt BSTR* pbstrShortName);

    virtual HRESULT STDMETHODCALLTYPE NotifyChanged(void);

    // ICanHandleException

    virtual HRESULT STDMETHODCALLTYPE CanHandleException(
        __in EXCEPINFO* pExcepInfo,
        __in_opt VARIANT* pvar);

    // IActiveScriptSiteInterruptPoll

    virtual HRESULT STDMETHODCALLTYPE QueryContinue(void);
};
