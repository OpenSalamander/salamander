// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guicomponent.h
	Implements the component base class.
*/

#pragma once

#include "dispimpl.h"
#include "guidefs.h"

/// Component base class.
/// You never use this class directly, you must create inherited class.
class CSalamanderGuiComponentBase : public ISalamanderGuiComponentInternal
{
private:
    /// Member (property) name of the control in the container.
    BSTR m_strName;

    /// Dispatch ID of the member.
    DISPID m_dispid;

    /// Original window procedure before installation of the sublass hook.
    WNDPROC m_pfnPrevWndProc;

    /// Our subclass window procedure.
    static LRESULT CALLBACK SubclassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    /// Location (in client coordinates) and size of the
    /// control, in dialog units.
    CBounds m_bounds;

    /// Text property of the control.
    BSTR m_strText;

    /// Parent component (form or another container).
    ISalamanderGuiComponentInternal* m_pParent;

    /// First child component (only valid for containers).
    ISalamanderGuiComponentInternal* m_pChild;

    /// Next sibling control.
    ISalamanderGuiComponentInternal* m_pSibling;

    /// Handle to the window.
    HWND m_hWnd;

    /// Component style flags, combination of values from
    /// SALGUI_STYLE enumeration.
    UINT m_uStyle;

    static const TCHAR s_szInstancePropName[];

    virtual HWND CreateHwnd(HWND hWndParent)
    {
        // Must be overriden.
        _ASSERTE(0);
        return NULL;
    }

    BSTR TransferTextFromWindowWorker();

    /// Stores text of the window into the Text property.
    void TransferTextFromWindow();

    void GetTextSize(HDC hDC, LPCOLESTR pszText, SIZE& sz);

    void GetTextSize(HDC hDC, SIZE& sz)
    {
        GetTextSize(hDC, m_strText, sz);
    }

public:
    CSalamanderGuiComponentBase(__in_opt VARIANT* text = NULL);
    ~CSalamanderGuiComponentBase();

    // ISalamanderGuiComponent

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Text(
        /* [retval][out] */ BSTR* text);

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Text(
        /* [in] */ BSTR text);

    // ISalamanderGuiComponentInternal

    virtual LPCOLESTR STDMETHODCALLTYPE GetMemberName(void);

    virtual DISPID STDMETHODCALLTYPE GetMemberId(void);

    virtual void STDMETHODCALLTYPE SetMember(
        LPCOLESTR pszName,
        DISPID dispid);

    virtual HRESULT STDMETHODCALLTYPE GetSibling(
        /* [out] */ ISalamanderGuiComponentInternal** ppComponent);

    virtual void STDMETHODCALLTYPE GetBounds(
        /* [out] */ struct SALGUI_BOUNDS* bounds);

    virtual void STDMETHODCALLTYPE SetBounds(
        const struct SALGUI_BOUNDS* bounds);

    virtual void STDMETHODCALLTYPE RemoveSelf(void);

    virtual void STDMETHODCALLTYPE RemoveComponent(
        ISalamanderGuiComponentInternal* pComponent);

    virtual void STDMETHODCALLTYPE AddComponent(
        ISalamanderGuiComponentInternal* pComponent);

    virtual void STDMETHODCALLTYPE SetParent(
        ISalamanderGuiComponentInternal* pParent);

    virtual void STDMETHODCALLTYPE SetSibling(
        ISalamanderGuiComponentInternal* pSibling);

    virtual HWND STDMETHODCALLTYPE Create(void);

    virtual HWND STDMETHODCALLTYPE GetHwnd(void);

    virtual LRESULT STDMETHODCALLTYPE OnMessage(
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);

    virtual void STDMETHODCALLTYPE TransferDataToWindow(void);

    virtual void STDMETHODCALLTYPE TransferDataFromWindow(void);

    virtual void STDMETHODCALLTYPE RecalcBounds(
        HWND hWnd,
        HDC hDC);

    virtual void STDMETHODCALLTYPE BoundsToPixels(
        /* [out][in] */ struct SALGUI_BOUNDS* bounds);

    virtual void STDMETHODCALLTYPE PixelsToBounds(
        /* [out][in] */ struct SALGUI_BOUNDS* bounds);

    virtual void STDMETHODCALLTYPE AutosizeComponent(
        ISalamanderGuiComponentInternal* pComponent);

    virtual UINT STDMETHODCALLTYPE GetStyle();
};

template <typename T, typename Interface>
class CSalamanderGuiComponentImpl : public CSalamanderGuiComponentBase,
                                    public CDispatchImpl<T, Interface>
{
public:
    CSalamanderGuiComponentImpl<T, Interface>(
        __in_opt VARIANT* text = NULL) : CSalamanderGuiComponentBase(text)
    {
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
        if (IsEqualIID(iid, __uuidof(ISalamanderGuiComponent)))
        {
            AddRef();
            *ppvObject = (ISalamanderGuiComponent*)this;
            return S_OK;
        }
        else if (IsEqualIID(iid, __uuidof(ISalamanderGuiComponentInternal)))
        {
            AddRef();
            *ppvObject = (ISalamanderGuiComponentInternal*)this;
            return S_OK;
        }

        return __super::QueryInterface(iid, ppvObject);
    }

    // ISalamanderGuiComponent

    virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Text(
        /* [retval][out] */ BSTR* text)
    {
        return CSalamanderGuiComponentBase::get_Text(text);
    }

    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Text(
        /* [in] */ BSTR text)
    {
        return CSalamanderGuiComponentBase::put_Text(text);
    }
};
