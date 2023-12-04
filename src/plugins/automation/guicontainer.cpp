// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guicontainer.cpp
	Implements the component container class.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "guicomponent.h"
#include "guicontainer.h"
#include "aututils.h"
#include "lang\lang.rh"

CSalamanderGuiContainerBase::CSalamanderGuiContainerBase() : m_tempMembers(1, 4)
{
    m_dispidCounter = DISPID_FIRST_MEMBER;
}

CSalamanderGuiContainerBase::~CSalamanderGuiContainerBase()
{
}

HRESULT CSalamanderGuiContainerBase::ContainerGetDispIDs(
    __in_ecount(cNames) LPOLESTR* rgszNames,
    UINT cNames,
    __out_ecount(cNames) DISPID* pdispid)
{
    HRESULT hr = S_OK;

    while (cNames--)
    {
        // Lookup name among existing members.
        if (LookupMember(*rgszNames, pdispid) == LookupNotFound)
        {
            // Member does not exist, we will maybe need
            // to create a new one. Insert the information
            // about the member into the temporary list.
            *pdispid = InsertMember(*rgszNames);
        }

        ++rgszNames;
        ++pdispid;
    }

    return hr;
}

CSalamanderGuiContainerBase::LookupResult
CSalamanderGuiContainerBase::LookupMember(
    LPCOLESTR pszName,
    __out DISPID* dispid,
    __out_opt ISalamanderGuiComponentInternal** ppComponent)
{
    ISalamanderGuiComponentInternal *pIter, *pTemp;

    *dispid = 0;
    if (ppComponent)
    {
        *ppComponent = NULL;
    }

    // Check the temporary members.
    for (int i = 0; i < m_tempMembers.Count; i++)
    {
        if (wcscmp(pszName, m_tempMembers.At(i).name) == 0)
        {
            *dispid = m_tempMembers.At(i).dispid;
            return LookupTemporary;
        }
    }

    pIter = GetChildNoAddRef();
    while (pIter != NULL)
    {
        if (wcscmp(pszName, pIter->GetMemberName()) == 0)
        {
            if (ppComponent)
            {
                *ppComponent = pIter;
                pIter->AddRef();
            }
            *dispid = pIter->GetMemberId();
            return LookupOk;
        }

        pIter->GetSibling(&pTemp);
        pIter = pTemp;
    }

    return LookupNotFound;
}

CSalamanderGuiContainerBase::LookupResult
CSalamanderGuiContainerBase::LookupMember(
    DISPID dispid,
    __out ISalamanderGuiComponentInternal** ppComponent,
    __out_opt int* pTempMemberIndex)
{
    ISalamanderGuiComponentInternal* pIter;

    _ASSERTE(ppComponent != NULL);
    *ppComponent = NULL;

    if (pTempMemberIndex != NULL)
    {
        *pTempMemberIndex = -1;
    }

    // Check the temporary members.
    for (int i = 0; i < m_tempMembers.Count; i++)
    {
        if (m_tempMembers.At(i).dispid == dispid)
        {
            if (pTempMemberIndex)
            {
                *pTempMemberIndex = i;
            }
            return LookupTemporary;
        }
    }

    pIter = GetChildNoAddRef();
    while (pIter != NULL)
    {
        if (pIter->GetMemberId() == dispid)
        {
            *ppComponent = pIter;
            pIter->AddRef();
            return LookupOk;
        }

        pIter->GetSibling(&pIter);
    }

    return LookupNotFound;
}

DISPID CSalamanderGuiContainerBase::InsertMember(LPCOLESTR pszName)
{
    int i = m_tempMembers.Add(MEMBER(m_dispidCounter));
    m_tempMembers.At(i).name = SysAllocString(pszName);
    return m_dispidCounter++;
}

HRESULT CSalamanderGuiContainerBase::ContainerInvoke(
    __in DISPID dispid,
    __in WORD wFlags,
    __in DISPPARAMS* pdp,
    __out_opt VARIANT* pvarRes,
    __out_opt EXCEPINFO* pei)
{
    _ASSERTE(dispid >= DISPID_FIRST_MEMBER);

    if (wFlags & DISPATCH_PROPERTYGET)
    {
        return ContainerGetMember(dispid, pdp, pvarRes, pei);
    }
    else if (wFlags & DISPATCH_PROPERTYPUT)
    {
        return ContainerPutMember(dispid, pdp, pvarRes, pei);
    }
    else
    {
        _ASSERTE(0);
    }

    return DISP_E_MEMBERNOTFOUND;
}

HRESULT CSalamanderGuiContainerBase::ContainerGetMember(
    __in DISPID dispid,
    __in DISPPARAMS* pdp,
    __out_opt VARIANT* pvarRes,
    __out_opt EXCEPINFO* pei)
{
    ISalamanderGuiComponentInternal* pComponent;
    LookupResult lookup;
    HRESULT hr;

    lookup = LookupMember(dispid, &pComponent);
    if (lookup == LookupOk)
    {
        hr = pComponent->QueryInterface(IID_IDispatch, (void**)&V_DISPATCH(pvarRes));
        _ASSERTE(SUCCEEDED(hr));
        if (SUCCEEDED(hr))
        {
            V_VT(pvarRes) = VT_DISPATCH;
        }
        else
        {
            V_VT(pvarRes) = VT_EMPTY;
        }
        pComponent->Release();
    }
    else
    {
        _ASSERTE(0);
        hr = DISP_E_MEMBERNOTFOUND;
    }

    return hr;
}

HRESULT CSalamanderGuiContainerBase::ContainerPutMember(
    __in DISPID dispid,
    __in DISPPARAMS* pdp,
    __out_opt VARIANT* pvarRes,
    __out_opt EXCEPINFO* pei)
{
    ISalamanderGuiComponentInternal* pComponent;
    ISalamanderGuiComponentInternal* pParam;
    LookupResult lookup;
    HRESULT hr = S_OK;
    int iTempMember;

    _ASSERTE(pdp->cArgs == 1);
    if (pdp->cArgs < 1)
    {
        return DISP_E_BADPARAMCOUNT;
    }

    lookup = LookupMember(dispid, &pComponent, &iTempMember);

    // Check for special case - assigning NULL to the property.
    // JScript: container.member = null;
    // VBScript: container.member = Nothing;
    if (!IsArgumentPresent(&pdp->rgvarg[0]))
    {
        if (lookup == LookupOk)
        {
            // Unlink regular member.
            _ASSERT(pComponent != NULL);
            _ASSERT(pComponent->GetMemberId() == dispid);
            pComponent->RemoveSelf();
        }
        else if (lookup == LookupTemporary)
        {
            // Remove temporary member.
            _ASSERT(pComponent == NULL);
            m_tempMembers.Delete(iTempMember);
        }
        else
        {
            // Setting non-existent member to NULL is no-op.
            _ASSERT(pComponent == NULL);
        }

        if (pComponent)
        {
            pComponent->Release();
        }

        return S_OK;
    }

    if (lookup == LookupNotFound)
    {
        return DISP_E_MEMBERNOTFOUND;
    }

    hr = S_OK;

    // Only components can be added to the container.
    if (V_VT(&pdp->rgvarg[0]) != VT_DISPATCH &&
        V_VT(&pdp->rgvarg[0]) != VT_UNKNOWN)
    {
        hr = DISP_E_BADVARTYPE;
    }

    if (SUCCEEDED(hr))
    {
        hr = V_UNKNOWN(&pdp->rgvarg[0])->QueryInterface(__uuidof(ISalamanderGuiComponentInternal), (void**)&pParam);
    }

    if (FAILED(hr))
    {
        // It's not a component.

        if (pComponent)
        {
            pComponent->Release();
        }

        return ::RaiseError(pei, IDS_E_NOTCOMPONENT, SALAUT_E_NOTCOMPONENT, GetContainerProgId());
    }

    // Forms are special top-level components and cannot
    // be added to the container either.
    ISalamanderGuiForm* pForm;
    if (SUCCEEDED(V_UNKNOWN(&pdp->rgvarg[0])->QueryInterface(__uuidof(ISalamanderGuiForm), (void**)&pForm)))
    {
        pForm->Release();
        pParam->Release();
        if (pComponent)
        {
            pComponent->Release();
        }
        return ::RaiseError(pei, IDS_E_FORMPUT, SALAUT_E_NOTCOMPONENT, GetContainerProgId());
    }

    if (lookup == LookupOk)
    {
        // The regular member already exist, unlink it first.
        _ASSERTE(pComponent != NULL);
        _ASSERT(pComponent->GetMemberId() == dispid);

        pParam->SetMember(pComponent->GetMemberName(), dispid);

        pComponent->RemoveSelf();
        pComponent->Release();
    }
    else
    {
        // Turn the temporary member into the regular one.
        _ASSERTE(lookup == LookupTemporary);
        _ASSERTE(pComponent == NULL);
        _ASSERTE(iTempMember != -1);

        pParam->SetMember(m_tempMembers.At(iTempMember).name, dispid);
        m_tempMembers.Delete(iTempMember);
    }

    InternalAddComponent(pParam);

    pParam->Release();

    return S_OK;
}

void CSalamanderGuiContainerBase::CreateChildren()
{
    ISalamanderGuiComponentInternal* pChild = GetChildNoAddRef();

    while (pChild)
    {
        pChild->Create();
        OnChildCreated(pChild);
        pChild->GetSibling(&pChild);
    }
}
