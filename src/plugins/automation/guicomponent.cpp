// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guicomponent.cpp
	Implements the component base class.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "guicomponent.h"
#include "aututils.h"

const TCHAR CSalamanderGuiComponentBase::s_szInstancePropName[] = _T("AutomationComponent");

CSalamanderGuiComponentBase::CSalamanderGuiComponentBase(__in_opt VARIANT* text)
{
    m_bounds.Default();
    m_strText = NULL;
    m_pParent = NULL;
    m_pChild = NULL;
    m_pSibling = NULL;
    m_strName = NULL;
    m_dispid = 0;
    m_hWnd = NULL;
    m_pfnPrevWndProc = NULL;
    m_uStyle = SALGUI_STYLE_AUTOLAYOUT;

    if (IsArgumentPresent(text))
    {
        m_strText = _bstr_t(_variant_t(text)).copy();
    }
}

CSalamanderGuiComponentBase::~CSalamanderGuiComponentBase()
{
    if (m_strText != NULL)
    {
        SysFreeString(m_strText);
    }

    if (m_strName != NULL)
    {
        SysFreeString(m_strName);
    }
}

LPCOLESTR STDMETHODCALLTYPE CSalamanderGuiComponentBase::GetMemberName(void)
{
    _ASSERTE(m_strName);
    return m_strName;
}

DISPID STDMETHODCALLTYPE CSalamanderGuiComponentBase::GetMemberId(void)
{
    _ASSERTE(m_dispid);
    return m_dispid;
}

HRESULT STDMETHODCALLTYPE CSalamanderGuiComponentBase::GetSibling(
    /* [out] */ ISalamanderGuiComponentInternal** ppSibling)
{
    _ASSERTE(ppSibling);

    *ppSibling = m_pSibling;
    /*if (m_pSibling)
	{
		m_pSibling->AddRef();
		return S_OK;
	}

	return S_FALSE;*/
    return S_OK;
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::GetBounds(
    /* [out] */ struct SALGUI_BOUNDS* bounds)
{
    *bounds = m_bounds;
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::SetBounds(
    const struct SALGUI_BOUNDS* bounds)
{
    m_bounds = *bounds;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiComponentBase::get_Text(
    /* [retval][out] */ BSTR* text)
{
    if (m_hWnd == NULL)
    {
        *text = SysAllocString(m_strText ? m_strText : L"");
    }
    else
    {
        BSTR str = TransferTextFromWindowWorker();
        *text = str ? str : SysAllocString(L"");
    }

    return S_OK;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiComponentBase::put_Text(
    /* [in] */ BSTR text)
{
    if (m_hWnd == NULL)
    {
        if (m_strText != NULL)
        {
            SysFreeString(m_strText);
            m_strText = NULL;
        }

        if (text != NULL)
        {
            m_strText = SysAllocString(text);
        }

        if (GetStyle() & SALGUI_STYLE_AUTOLAYOUT)
        {
            AutosizeComponent(this);
        }
    }
    else
    {
        SetWindowTextW(m_hWnd, text);
    }

    return S_OK;
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::SetMember(
    LPCOLESTR pszName,
    DISPID dispid)
{
    _ASSERTE(m_strName == NULL);
    _ASSERTE(m_dispid == 0);
    _ASSERTE(pszName != NULL);
    _ASSERTE(dispid != 0);

    m_strName = SysAllocString(pszName);
    m_dispid = dispid;
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::RemoveSelf(void)
{
    // Unlink from parent first.
    if (m_pParent != NULL)
    {
        m_pParent->RemoveComponent(this);
        m_pParent = NULL;
    }

    _ASSERT(m_pSibling == NULL);

    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = NULL;
    }

    SysFreeString(m_strName);
    m_strName = NULL;

    m_dispid = 0;
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::RemoveComponent(
    ISalamanderGuiComponentInternal* pComponent)
{
    _ASSERTE(pComponent != NULL);
    _ASSERTE(m_pChild != NULL);

    ISalamanderGuiComponentInternal *pIter, *pPrev = NULL;

    pIter = m_pChild;
    while (pIter != pComponent)
    {
        pPrev = pIter;
        pIter->GetSibling(&pIter);
        _ASSERTE(pIter != NULL);
    }

    if (pPrev == NULL)
    {
        pIter->GetSibling(&m_pChild);
    }
    else
    {
        ISalamanderGuiComponentInternal* pTmp;

        pIter->GetSibling(&pTmp);
        pPrev->SetSibling(pTmp);
    }

    pIter->Release();
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::AddComponent(
    ISalamanderGuiComponentInternal* pComponent)
{
    pComponent->SetParent(this);

    if (m_pChild)
    {
        // Link into the sibling chain.

        ISalamanderGuiComponentInternal *pIter, *pPrev;
        pIter = m_pChild;
        pPrev = NULL;

        while (pIter != NULL)
        {
            pPrev = pIter;
            pIter->GetSibling(&pIter);
        }

        pPrev->SetSibling(pComponent);
    }
    else
    {
        // First child.
        m_pChild = pComponent;
    }

    pComponent->AddRef();
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::SetParent(
    ISalamanderGuiComponentInternal* pParent)
{
    // Do not AddRef the parent since it creates circular reference.
    // This is ok because parent's lifetime is longer than the lifetime
    // of the child.

    m_pParent = pParent;
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::SetSibling(
    ISalamanderGuiComponentInternal* pSibling)
{
    // Do not AddRef the sibling since it creates circular reference.
    // The lifetime of the sibling is controlled by the parent (i.e.
    // container).

    m_pSibling = pSibling;
}

HWND STDMETHODCALLTYPE CSalamanderGuiComponentBase::Create(void)
{
    HWND hWnd;

    _ASSERTE(m_hWnd == NULL);

    hWnd = CreateHwnd(m_pParent->GetHwnd());
    if (hWnd != NULL)
    {
        // Subclass the window.
        SetProp(hWnd, s_szInstancePropName, this);
        m_pfnPrevWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
            hWnd,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(SubclassWndProc)));
        m_hWnd = hWnd;

        TransferDataToWindow();
    }

    return hWnd;
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::TransferDataToWindow(void)
{
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::TransferDataFromWindow(void)
{
}

HWND STDMETHODCALLTYPE CSalamanderGuiComponentBase::GetHwnd(void)
{
    _ASSERTE(IsWindow(m_hWnd));
    return m_hWnd;
}

LRESULT STDMETHODCALLTYPE CSalamanderGuiComponentBase::OnMessage(
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    LRESULT lres;

    lres = CallWindowProc(m_pfnPrevWndProc, m_hWnd, uMsg, wParam, lParam);

    if (uMsg == WM_DESTROY)
    {
        // Give the component chance to store data attributes
        // before the window ceases to exist.
        TransferDataFromWindow();
    }
    else if (uMsg == WM_NCDESTROY)
    {
        // Final message, unsubclass.
        WNDPROC pfnWndProc;

        pfnWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
            m_hWnd,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(m_pfnPrevWndProc)));

        // If this assertion fails, it means that the component
        // was sublassed more than once and that the subclass
        // chain was corrupted.
        _ASSERTE(pfnWndProc == SubclassWndProc);

        SetProp(m_hWnd, s_szInstancePropName, NULL);
        m_pfnPrevWndProc = NULL;
        m_hWnd = NULL;
    }

    return lres;
}

LRESULT CALLBACK CSalamanderGuiComponentBase::SubclassWndProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    LRESULT lres;
    CSalamanderGuiComponentBase* that;

    that = reinterpret_cast<CSalamanderGuiComponentBase*>(
        GetProp(hWnd, s_szInstancePropName));
    _ASSERTE(that);
    _ASSERTE(that->m_hWnd == hWnd);

    lres = that->OnMessage(uMsg, wParam, lParam);

    return lres;
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::RecalcBounds(
    HWND hWnd,
    HDC hDC)
{
    // Should be overriden in inherited class.
    _ASSERTE(0);
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::BoundsToPixels(
    /* [out][in] */ struct SALGUI_BOUNDS* bounds)
{
    _ASSERTE(m_pParent);

    // This will stop at the form, which overrides this method.
    return m_pParent->BoundsToPixels(bounds);
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::PixelsToBounds(
    /* [out][in] */ struct SALGUI_BOUNDS* bounds)
{
    _ASSERTE(m_pParent);

    // This will stop at the form, which overrides this method.
    return m_pParent->BoundsToPixels(bounds);
}

BSTR CSalamanderGuiComponentBase::TransferTextFromWindowWorker()
{
    BSTR str = NULL;

    _ASSERTE(IsWindow(m_hWnd));

    int cch = GetWindowTextLengthW(m_hWnd);
    if (cch > 0)
    {
        str = SysAllocStringLen(NULL, cch);
        if (str)
        {
            GetWindowTextW(m_hWnd, str, cch + 1);
        }
    }

    return str;
}

void CSalamanderGuiComponentBase::TransferTextFromWindow()
{
    if (m_strText != NULL)
    {
        SysFreeString(m_strText);
        m_strText = NULL;
    }

    m_strText = TransferTextFromWindowWorker();
}

void STDMETHODCALLTYPE CSalamanderGuiComponentBase::AutosizeComponent(
    ISalamanderGuiComponentInternal* pComponent)
{
    // This is implemented in the form, so we only
    // forward this call to the parent until we hit the form.
    if (m_pParent)
    {
        m_pParent->AutosizeComponent(pComponent);
    }
}

void CSalamanderGuiComponentBase::GetTextSize(HDC hDC, LPCOLESTR pszText, SIZE& sz)
{
    CBounds pxbounds;
    RECT rc = {
        0,
    };

    DrawTextW(hDC, pszText ? pszText : L"", -1, &rc, DT_CALCRECT);

    pxbounds.x = 0;
    pxbounds.y = 0;
    pxbounds.cx = rc.right - rc.left;
    pxbounds.cy = rc.bottom - rc.top;

    m_pParent->PixelsToBounds(&pxbounds);

    sz.cx = pxbounds.cx;
    sz.cy = pxbounds.cy;
}

UINT STDMETHODCALLTYPE CSalamanderGuiComponentBase::GetStyle()
{
    return m_uStyle;
}
