// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guinamespace.h
	Implements the Salamander.Forms namespace.
*/

#pragma once

#include "dispimpl.h"

class CScriptInfo;

class CSalamanderGuiNamespace : public CDispatchImpl<CSalamanderGuiNamespace, ISalamanderGui>
{
private:
    CScriptInfo* m_pScript;

public:
    DECLARE_DISPOBJ_NAME(L"Salamander.Forms")

    CSalamanderGuiNamespace(CScriptInfo* pScript);
    ~CSalamanderGuiNamespace();

    // ISalamanderGui

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Form(
        /* [optional][in] */ VARIANT* text,
        /* [retval][out] */ ISalamanderGuiComponent** component);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Button(
        /* [optional][in] */ VARIANT* text,
        /* [optional][in] */ VARIANT* result,
        /* [retval][out] */ ISalamanderGuiComponent** component);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE CheckBox(
        /* [optional][in] */ VARIANT* text,
        /* [retval][out] */ ISalamanderGuiComponent** component);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE TextBox(
        /* [optional][in] */ VARIANT* text,
        /* [retval][out] */ ISalamanderGuiComponent** component);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Label(
        /* [optional][in] */ VARIANT* text,
        /* [retval][out] */ ISalamanderGuiComponent** component);
};
