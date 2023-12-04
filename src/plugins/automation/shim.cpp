// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	shim.cpp
	Shims for various known bugs of some scripting engines.
*/

#include "precomp.h"
#include "scriptlist.h"
#include "shim.h"
#include "knownengines.h"

CScriptEngineShim::CScriptEngineShim(CScriptInfo* pScript)
{
    _ASSERTE(pScript != NULL);
    m_pScript = pScript;
    m_bExecuting = false;
    m_bOnEnterScriptAlreadyCalled = false;
    m_bOnLeaveScriptAlreadyCalled = false;
    m_cNestedEnterScript = 0;
    m_bInScript = false;
}

CScriptEngineShim::~CScriptEngineShim()
{
}

CScriptEngineShim* CScriptEngineShim::Create(CScriptInfo* pScript)
{
    CScriptEngineShim* pShim = NULL;
    REFCLSID clsidEngine = pScript->GetEngineCLSID();

    if (clsidEngine == CLSID_PHPScript)
    {
        pShim = CPhpScriptEngineShim::Create(pScript);
    }
    else if (clsidEngine == CLSID_Python)
    {
        pShim = CPythonScriptEngineShim::Create(pScript);
    }
    else if (clsidEngine == CLSID_GlobalRubyScript)
    {
        pShim = CGlobalRubyScriptEngineShim::Create(pScript);
    }

    if (pShim == NULL)
    {
        // Default shim.
        pShim = new CScriptEngineShim(pScript);
    }

    return pShim;
}

void CScriptEngineShim::AdjustSourcePosition(LONG& linePos, LONG& columnPos)
{
    // JScript and VBScript are zero-based,
    // so threat this as default behavior
    linePos += 1;
    columnPos += 1;
}

HRESULT CScriptEngineShim::ReleaseEngine(IActiveScript* pScript)
{
    ULONG dummy;
    return ReleaseEngineCore(pScript, dummy);
}

HRESULT CScriptEngineShim::ReleaseEngineCore(IActiveScript* pScript, ULONG& cRef)
{
    HRESULT hr;

    hr = pScript->Close();
    _ASSERTE(hr == S_OK);
    cRef = pScript->Release();

    return hr;
}

void CScriptEngineShim::Site_OnEnterScript()
{
    if (m_cNestedEnterScript == 0 && m_bExecuting)
    {
        ScriptEnter();
    }

    ++m_cNestedEnterScript;
    m_bOnEnterScriptAlreadyCalled = true;
}

void CScriptEngineShim::Site_OnLeaveScript()
{
    _ASSERTE(m_cNestedEnterScript > 0);
    --m_cNestedEnterScript;

    if (m_cNestedEnterScript == 0 && m_bExecuting)
    {
        ScriptLeave();
    }

    m_bOnLeaveScriptAlreadyCalled = true;
}

void CScriptEngineShim::BeginExecution()
{
    _ASSERTE(!m_bExecuting);
    m_bExecuting = true;

    if (m_bOnEnterScriptAlreadyCalled && (!m_bOnLeaveScriptAlreadyCalled || m_cNestedEnterScript > 0))
    {
        ScriptEnter();
    }
}

void CScriptEngineShim::EndExecution()
{
    _ASSERTE(m_bExecuting);
    m_bExecuting = false;

    if (m_cNestedEnterScript > 0 || m_bInScript)
    {
        ScriptLeave();
    }
}

HRESULT CScriptEngineShim::InterruptScript(IActiveScript* pScript)
{
    HRESULT hr;
    EXCEPINFO ei;

    InitAbortException(&ei);
    hr = pScript->InterruptScriptThread(SCRIPTTHREADID_BASE, &ei, SCRIPTINTERRUPT_RAISEEXCEPTION);

    return hr;
}

void CScriptEngineShim::ScriptEnter()
{
    _ASSERTE(!m_bInScript);
    m_pScript->ScriptEnter();
    m_bInScript = true;
}

void CScriptEngineShim::ScriptLeave()
{
    _ASSERTE(m_bInScript);
    m_pScript->ScriptLeave();
    m_bInScript = false;
}

bool CScriptEngineShim::DisplayErrorHook(__in EXCEPINFO* ei, __in_opt BSTR src, __in bool bDebug)
{
    UNREFERENCED_PARAMETER(src);
    UNREFERENCED_PARAMETER(bDebug);

    // Some engines (Python, Ruby) does not report abortion thru our special
    // SALAUT_E_ABORT we pass into the engine, instead they raise COM
    // exception. Since this shim is general and should not influence
    // anything, I decided to put it here in the base shim.
    if (ei && ei->scode == DISP_E_EXCEPTION && m_pScript->IsAbortPending())
    {
        return false;
    }

    return true;
}

void CScriptEngineShim::InitAbortException(__in EXCEPINFO* ei)
{
    _ASSERTE(ei != NULL);

    memset(ei, 0, sizeof(*ei));
    ei->scode = SALAUT_E_ABORT;
}

////////////////////////////////////////////////////////////////////////////////

CPhpScriptEngineShim::CPhpScriptEngineShim(CScriptInfo* pScript)
    : CScriptEngineShim(pScript)
{
}

CScriptEngineShim* CPhpScriptEngineShim::Create(CScriptInfo* pScript)
{
    return new CPhpScriptEngineShim(pScript);
}

void CPhpScriptEngineShim::AdjustSourcePosition(LONG& linePos, LONG& columnPos)
{
    // PHP starts counting lines from one and char position
    // always sets to zero.
    columnPos = -1;
}

HRESULT CPhpScriptEngineShim::ReleaseEngine(IActiveScript* pScript)
{
    HRESULT hr;
    ULONG cRef;

    // This is needed for the buggy PHP script engine to revoke
    // instance from the global interface table.
    hr = pScript->SetScriptSite(NULL);
    // Behaved engines return E_POINTER, unlike PHP, which returns S_OK
    // and ActivePython, which returns E_FAIL.
    _ASSERTE(SUCCEEDED(hr));

    hr = ReleaseEngineCore(pScript, cRef);

    // Workaround for the buggy PHP script engine
    // to kill the script thread and delete the engine object
    if (cRef == 1)
    {
        cRef = pScript->Release();
    }

    return hr;
}

////////////////////////////////////////////////////////////////////////////////

CPythonScriptEngineShim::CPythonScriptEngineShim(CScriptInfo* pScript)
    : CScriptEngineShim(pScript)
{
}

CScriptEngineShim* CPythonScriptEngineShim::Create(CScriptInfo* pScript)
{
    return new CPythonScriptEngineShim(pScript);
}

void CPythonScriptEngineShim::Site_OnEnterScript()
{
    // BUG 816:
    // Python engine calls OnEnterScript/OnLeaveScript in strange order:
    //   1. OnEnterScript (compilation phase)
    //   2. OnLeaveScript (compilation phase)
    //   3. OnEnterScript (execution phase)
    //   4. ...executing script...
    //   5. OnLeaveScript (execution phase)
    if (!m_bOnLeaveScriptAlreadyCalled)
    {
        __super::Site_OnEnterScript();
    }
}

void CPythonScriptEngineShim::Site_OnLeaveScript()
{
    if (!m_bOnLeaveScriptAlreadyCalled)
    {
        m_bOnLeaveScriptAlreadyCalled = true;
    }
    else
    {
        __super::Site_OnLeaveScript();
    }
}

////////////////////////////////////////////////////////////////////////////////

CGlobalRubyScriptEngineShim::CGlobalRubyScriptEngineShim(CScriptInfo* pScript)
    : CScriptEngineShim(pScript)
{
}

CScriptEngineShim* CGlobalRubyScriptEngineShim::Create(CScriptInfo* pScript)
{
    return new CGlobalRubyScriptEngineShim(pScript);
}

void CGlobalRubyScriptEngineShim::AdjustSourcePosition(LONG& linePos, LONG& columnPos)
{
    // Ruby seems not using position information at all,
    // both the line and column is always set to zero,
    // the line number is mentioned in the description
    // and is one-based.
    linePos = columnPos = -1;
}

void CGlobalRubyScriptEngineShim::Site_OnEnterScript()
{
    // BUG 103: Ruby calls OnEnterScript during parsing before BeginExecution.
    if (m_cNestedEnterScript++ == 0)
    {
        ScriptEnter();
    }
}

void CGlobalRubyScriptEngineShim::Site_OnLeaveScript()
{
    _ASSERTE(m_cNestedEnterScript > 0);
    if (--m_cNestedEnterScript == 0)
    {
        ScriptLeave();
    }
}

HRESULT CGlobalRubyScriptEngineShim::InterruptScript(IActiveScript* pScript)
{
    // Redmine #817:
    // Calling InterruptScriptThread on Ruby engine is a suicidal mission.
    // What is going on?
    // 1. Ruby suspends the thread executing the script (i.e. Salamander's
    //    main thread).
    // 2. It sets the thread context, forcing the instruction pointer to
    //    be address of the internal rb_raise function.
    // 3. The engine resumes the thread.
    // 4. The thread's context is completely messed up, executing the
    //    rb_raise function (which does longjmp according to search results
    //    on Google) without correctly unwinding the stack.
    // I still cannot believe this. To make the long story short, we just
    // prevent calling InterruptScriptThread when running Ruby script and
    // we are leaving the script to be interrupted cooperatively.
    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}
