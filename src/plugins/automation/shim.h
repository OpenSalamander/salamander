// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	shim.h
	Shims for various known bugs of some scripting engines.
*/

#pragma once

class CScriptInfo;

/// Base class providing default shims.
class CScriptEngineShim
{
protected:
    CScriptInfo* m_pScript;
    bool m_bExecuting;
    bool m_bOnEnterScriptAlreadyCalled;
    bool m_bOnLeaveScriptAlreadyCalled;
    int m_cNestedEnterScript;
    bool m_bInScript;

    CScriptEngineShim(CScriptInfo* pScript);

    HRESULT ReleaseEngineCore(IActiveScript* Script, ULONG& cRef);

    void ScriptEnter();
    void ScriptLeave();

public:
    virtual ~CScriptEngineShim();

    // Shim methods.

    /// Adjust line position in the source file.
    /// \remarks The adjusted positions should be one-based.
    virtual void AdjustSourcePosition(LONG& linePos, LONG& columnPos);

    /// Releases the scripting engine.
    virtual HRESULT ReleaseEngine(IActiveScript* pScript);

    virtual void Site_OnEnterScript();
    virtual void Site_OnLeaveScript();

    virtual void BeginExecution();
    virtual void EndExecution();

    /// Tries to interrupt the running script without fatal consequences.
    virtual HRESULT InterruptScript(IActiveScript* pScript);

    /// Allows the shim to control, whether particular error will be displayed
    /// or supressed.
    /// \return If the error should be displayed, the return value is true.
    virtual bool DisplayErrorHook(__in EXCEPINFO* ei, __in_opt BSTR src, __in bool bDebug);

    /// Initializes the EXCEPINFO structure with the exception information
    /// about script abortion.
    virtual void InitAbortException(__in EXCEPINFO* ei);

public:
    // Factory method.

    /// Creates shim instance based on the scripting engine.
    /// \param pScript CLSID that identifies the scripting engine the
    ///        shim is being created for.
    /// \return Returns pointer to the specific shim instance.
    /// \remarks The factory method should be called after the scripting
    ///          engine is created, since some shims may need to access
    ///          the engine's module to e.g. query specific version
    ///          information.
    static CScriptEngineShim* Create(CScriptInfo* pScript);
};

class CPhpScriptEngineShim : public CScriptEngineShim
{
private:
    CPhpScriptEngineShim(CScriptInfo* pScript);

public:
    // Shim overrides.

    virtual void AdjustSourcePosition(LONG& linePos, LONG& columnPos);
    virtual HRESULT ReleaseEngine(IActiveScript* pScript);

    static CScriptEngineShim* Create(CScriptInfo* pScript);
};

class CPythonScriptEngineShim : public CScriptEngineShim
{
private:
    CPythonScriptEngineShim(CScriptInfo* pScript);

public:
    // Shim overrides.

    virtual void Site_OnEnterScript();
    virtual void Site_OnLeaveScript();

    static CScriptEngineShim* Create(CScriptInfo* pScript);
};

class CGlobalRubyScriptEngineShim : public CScriptEngineShim
{
private:
    CGlobalRubyScriptEngineShim(CScriptInfo* pScript);

public:
    // Shim overrides.

    virtual void Site_OnEnterScript();
    virtual void Site_OnLeaveScript();
    virtual void AdjustSourcePosition(LONG& linePos, LONG& columnPos);
    virtual HRESULT InterruptScript(IActiveScript* pScript);

    static CScriptEngineShim* Create(CScriptInfo* pScript);
};
