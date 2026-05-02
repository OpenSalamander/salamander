// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	salamanderaut.h
	Salamander root automation object.
*/

#pragma once

#include "dispimpl.h"
#include "scriptlist.h"

class CSalamanderAutomation : public CDispatchImpl<CSalamanderAutomation, ISalamander>
{
private:
    class CSalamanderPanelAutomation* m_apPanels[2]; // 0=left, 1=right
    CScriptInfo* m_pScriptInfo;
    CScriptInfo::EXECUTION_INFO* m_pExecInfo;
    class CSalamanderProgressAutomation* m_pProgress;
    class CSalamanderWaitWndAutomation* m_pWaitWindow;
    class CSalamanderGuiNamespace* m_pGui;
    class CSalamanderScriptInfoAutomation* m_pScriptAut;

    HRESULT GetPanel(int nPanel, ISalamanderPanel** ppPanel);

    enum TraceLevel
    {
        TraceInfo,
        TraceError
    };

    HRESULT TraceWorker(
        __in TraceLevel level,
        __in BSTR message);

    void OnBeginExecution();
    void OnEndExecution();

protected:
    virtual HRESULT InvokeFilter(__out EXCEPINFO* pExcepInfo);

public:
    DECLARE_DISPOBJ_NAME(L"Salamander")

    CSalamanderAutomation(__in CScriptInfo* pScriptInfo);
    ~CSalamanderAutomation();

    void SetExecutionInfo(CScriptInfo::EXECUTION_INFO* info);

    // ISalamander

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE MsgBox(
        /* [in] */ BSTR prompt,
        /* [optional][in] */ VARIANT* buttons,
        /* [optional][in] */ VARIANT* title,
        /* [retval][out] */ int* result);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_LeftPanel(
        /* [retval][out] */ ISalamanderPanel** result);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_RightPanel(
        /* [retval][out] */ ISalamanderPanel** result);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_SourcePanel(
        /* [retval][out] */ ISalamanderPanel** result);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_TargetPanel(
        /* [retval][out] */ ISalamanderPanel** result);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Version(
        /* [retval][out] */ int* version);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE InputBox(
        /* [in] */ BSTR prompt,
        /* [optional][in] */ VARIANT* title,
        /* [optional][in] */ VARIANT* defresponse,
        /* [retval][out] */ BSTR* result);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_WindowsVersion(
        /* [retval][out] */ int* version);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_AutomationVersion(
        /* [retval][out] */ int* version);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Sleep(
        /* [in] */ int ms);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE AbortScript(void);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE TraceI(
        /* [in] */ BSTR message);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE TraceE(
        /* [in] */ BSTR message);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_ProgressDialog(
        /* [retval][out] */ ISalamanderProgressDialog** dialog);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_WaitWindow(
        /* [retval][out] */ ISalamanderWaitWindow** window);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE ViewFile(
        /* [in] */ BSTR file,
        /* [optional][in] */ VARIANT* temp);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE MatchesMask(
        /* [in] */ BSTR file,
        /* [in] */ BSTR mask,
        /* [retval][out] */ VARIANT_BOOL* match);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE DebugBreak(void);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE ErrorDialog(
        /* [in] */ VARIANT* file,
        /* [in] */ VARIANT* error,
        /* [in] */ int buttons,
        /* [optional][in] */ VARIANT* title,
        /* [retval][out] */ int* result);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE QuestionDialog(
        /* [in] */ VARIANT* file,
        /* [in] */ BSTR question,
        /* [in] */ int buttons,
        /* [optional][in] */ VARIANT* title,
        /* [retval][out] */ int* result);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE OverwriteDialog(
        /* [in] */ VARIANT* file1,
        /* [in] */ VARIANT* file2,
        /* [in] */ int buttons,
        /* [retval][out] */ int* result);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Forms(
        /* [retval][out] */ ISalamanderGui** gui);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE SetPersistentVal(
        /* [in] */ BSTR key,
        /* [in] */ VARIANT* val);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE GetPersistentVal(
        /* [in] */ BSTR key,
        /* [retval][out] */ VARIANT* val);

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Script(
        /* [retval][out] */ ISalamanderScriptInfo** info);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE GetFullPath(
        /* [in] */ BSTR path,
        /* [optional][in] */ VARIANT* panel,
        /* [out][retval] */ BSTR* result);
};
