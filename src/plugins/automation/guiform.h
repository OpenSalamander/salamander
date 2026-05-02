// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guiform.h
	Implements the form class.
*/

#pragma once

class CScriptInfo;

class CSalamanderGuiForm : public CSalamanderGuiContainerImpl<CSalamanderGuiForm, ISalamanderGuiForm>
{
private:
    static LONG s_nClass;
    static const TCHAR s_szClassName[];

    static bool ClassNeeded();
    static void ClassRelease();

    /// Exit code for the caption bar close button.
    int m_nCloseCode;

    /// Dialog exit code (return value from the Execute method).
    int m_nExitCode;

    /// Dialog font.
    HFONT m_hFont;

    /// Destroy font on dialog exit?
    bool m_bDestroyFont;

    /// Base metrics for the dialog units calculations.
    SIZE m_dluBase;

    /// Was the dialog dismissed?
    bool m_bDismissed;

    CScriptInfo* m_pScript;

    bool m_bAborted;

    HWND HwndNeeded();
    void UnionBounds(const SALGUI_BOUNDS& bounds);

    /// Aligns buttons in the form and also auto-sets the default
    /// button if not set explicitly.
    void AlignButtons();

    void AlignButtonLine(
        ISalamanderGuiComponentInternal* pFirstBtn,
        int cButtons,
        const RECT& rcBounds);

    /// Resizes components with the SALGUI_STYLE_FULLWIDTH style.
    void ResizeFullWidthComponents();

    void DismissDialog(int nExitCode);

    static LRESULT CALLBACK WndProcThunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void InternalAddComponent(ISalamanderGuiComponentInternal* pComponent);
    virtual void OnChildCreated(ISalamanderGuiComponentInternal* pComponent);

    static void MoveWindowRelative(HWND hWnd, int dx, int dy);

public:
    CSalamanderGuiForm(__in CScriptInfo* pScript, __in_opt VARIANT* text = NULL);
    ~CSalamanderGuiForm();

    DECLARE_DISPOBJ_NAME(L"Salamander.Form")

    // ISalamanderGuiComponentInternal

    virtual SALGUI_CLASS STDMETHODCALLTYPE GetClass(void)
    {
        return SALGUI_FORM;
    }

    virtual void STDMETHODCALLTYPE RecalcBounds(
        HWND hWnd,
        HDC hDC);

    virtual void STDMETHODCALLTYPE BoundsToPixels(
        /* [out][in] */ struct SALGUI_BOUNDS* bounds);

    virtual void STDMETHODCALLTYPE PixelsToBounds(
        /* [out][in] */ struct SALGUI_BOUNDS* bounds);

    virtual void STDMETHODCALLTYPE AutosizeComponent(
        ISalamanderGuiComponentInternal* pComponent);

    // ISalamanderGuiForm

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE Execute(
        /* [retval][out] */ int* result);
};
