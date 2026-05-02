// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guinamespace.cpp
	Implements the Salamander.Forms namespace.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "guicomponent.h"
#include "guicontainer.h"
#include "guiform.h"
#include "guibutton.h"
#include "guichkbox.h"
#include "guitxtbox.h"
#include "guilabel.h"
#include "guinamespace.h"
#include "aututils.h"

CSalamanderGuiNamespace::CSalamanderGuiNamespace(CScriptInfo* pScript)
{
    _ASSERTE(pScript != NULL);
    m_pScript = pScript;
}

CSalamanderGuiNamespace::~CSalamanderGuiNamespace()
{
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiNamespace::Form(
    /* [optional][in] */ VARIANT* text,
    /* [retval][out] */ ISalamanderGuiComponent** component)
{
    CSalamanderGuiForm* poForm;

    try
    {
        poForm = new CSalamanderGuiForm(m_pScript, text);
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    *component = poForm;
    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiNamespace::Button(
    /* [optional][in] */ VARIANT* text,
    /* [optional][in] */ VARIANT* result,
    /* [retval][out] */ ISalamanderGuiComponent** component)
{
    CSalamanderGuiButton* poButton;

    try
    {
        poButton = new CSalamanderGuiButton(text, result);
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    *component = poButton;
    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiNamespace::CheckBox(
    /* [optional][in] */ VARIANT* text,
    /* [retval][out] */ ISalamanderGuiComponent** component)
{
    CSalamanderGuiCheckBox* poCheckBox;

    try
    {
        poCheckBox = new CSalamanderGuiCheckBox(text);
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    *component = poCheckBox;
    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiNamespace::TextBox(
    /* [optional][in] */ VARIANT* text,
    /* [retval][out] */ ISalamanderGuiComponent** component)
{
    CSalamanderGuiTextBox* poTextBox;

    try
    {
        poTextBox = new CSalamanderGuiTextBox(text);
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    *component = poTextBox;
    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiNamespace::Label(
    /* [optional][in] */ VARIANT* text,
    /* [retval][out] */ ISalamanderGuiComponent** component)
{
    CSalamanderGuiLabel* poLabel;

    try
    {
        poLabel = new CSalamanderGuiLabel(text);
    }
    catch (_com_error& e)
    {
        return e.Error();
    }

    *component = poLabel;
    return S_OK;
}
