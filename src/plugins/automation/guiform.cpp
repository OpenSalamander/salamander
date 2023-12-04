// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guiform.cpp
	Implements the form class.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "guicomponent.h"
#include "guicontainer.h"
#include "guiform.h"
#include "aututils.h"
#include "scriptlist.h"
#include "lang\lang.rh"

extern HINSTANCE g_hInstance;

const TCHAR CSalamanderGuiForm::s_szClassName[] = _T("AutomationForm");
LONG CSalamanderGuiForm::s_nClass;

CSalamanderGuiForm::CSalamanderGuiForm(__in CScriptInfo* pScript, __in_opt VARIANT* text) : CSalamanderGuiContainerImpl<CSalamanderGuiForm, ISalamanderGuiForm>(text)
{
    m_hFont = NULL;
    m_bDestroyFont = false;
    m_dluBase.cx = m_dluBase.cy = -1;

    // We want autosize never be triggered for the form.
    m_uStyle &= ~SALGUI_STYLE_AUTOLAYOUT;

    _ASSERTE(pScript != NULL);
    m_pScript = pScript;

    m_bAborted = false;
}

CSalamanderGuiForm::~CSalamanderGuiForm()
{
    if (m_hWnd != NULL)
    {
        DestroyWindow(m_hWnd);
    }
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderGuiForm::Execute(
    /* [retval][out] */ int* result)
{
    DWORD style, exStyle;
    bool bCenter = (m_bounds.x == CW_USEDEFAULT) || (m_bounds.y == CW_USEDEFAULT);
    RECT rc;
    int cx, cy;
    MSG msg;
    HWND hwndParent;

    m_nExitCode = -1;
    m_nCloseCode = -1;
    m_bDismissed = false;

    HwndNeeded();

    // Resize form to accomodate all the child components.
    AutosizeComponent(this);

    cx = m_bounds.cx;
    if (cx == CW_USEDEFAULT || cx == 0)
    {
        cx = 256;
    }

    cy = m_bounds.cy;
    if (cy == CW_USEDEFAULT || cy == 0)
    {
        cy = 128;
    }

    CreateChildren();

    style = GetWindowLong(m_hWnd, GWL_STYLE);
    exStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);

    CBounds pxbounds = m_bounds;
    pxbounds.cx = cx + 8; // + right padding
    pxbounds.cy = cy + 8; // + bottom padding

    BoundsToPixels(&pxbounds);
    pxbounds.Get(rc);

    AdjustWindowRectEx(&rc, style, GetMenu(m_hWnd) != NULL, exStyle);

    SetWindowPos(m_hWnd, NULL, pxbounds.x, pxbounds.y, rc.right - rc.left,
                 rc.bottom - rc.top, SWP_NOZORDER);

    if (bCenter)
    {
        SalamanderGeneral->MultiMonCenterWindow(
            m_hWnd,
            SalamanderGeneral->GetMsgBoxParent(),
            FALSE);
    }

    AlignButtons();
    ResizeFullWidthComponents();

    HMENU hSysMenu = GetSystemMenu(m_hWnd, FALSE);

    // If the form does not have the Cancel button, disable also the
    // close button in the title bar.
    if (m_nCloseCode == -1)
    {
        if (hSysMenu != NULL)
        {
            EnableMenuItem(hSysMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
        }
    }

    if (hSysMenu != NULL)
    {
        if (!(style & WS_MAXIMIZEBOX))
        {
            DeleteMenu(hSysMenu, SC_MAXIMIZE, MF_BYCOMMAND);
            DeleteMenu(hSysMenu, SC_RESTORE, MF_BYCOMMAND);
        }

        if (!(style & WS_MINIMIZEBOX))
        {
            DeleteMenu(hSysMenu, SC_MINIMIZE, MF_BYCOMMAND);
            DeleteMenu(hSysMenu, SC_RESTORE, MF_BYCOMMAND);
        }

        if (!(style & WS_THICKFRAME))
        {
            DeleteMenu(hSysMenu, SC_SIZE, MF_BYCOMMAND);
        }

        // Remove separator between "Move" and "Close" if they
        // remained the only items in the sys menu.

        int iMove = -1;
        int iClose = -1;
        int cItems = GetMenuItemCount(hSysMenu);

        for (int i = 0; i < cItems; i++)
        {
            UINT uId = GetMenuItemID(hSysMenu, i);
            if (uId == SC_MOVE)
            {
                iMove = i;
            }
            else if (uId == SC_CLOSE)
            {
                iClose = i;
            }
        }

        if (iMove >= 0 && iClose >= 0 && abs(iClose - iMove) == 2)
        {
            MENUITEMINFO mii;
            int iMaybeSep;

            iMaybeSep = min(iMove, iClose) + 1;
            mii.cbSize = sizeof(MENUITEMINFO);
            mii.fMask = MIIM_FTYPE;

            if (GetMenuItemInfo(hSysMenu, iMaybeSep, TRUE, &mii) &&
                mii.fType == MFT_SEPARATOR)
            {
                DeleteMenu(hSysMenu, iMaybeSep, MF_BYPOSITION);
            }
        }
    }

    ShowWindow(m_hWnd, SW_SHOW);

    if (m_pChild != NULL)
    {
        // The default focus is the first item that is a valid tab-stop.
        HWND hWndDefFocus;
        hWndDefFocus = GetNextDlgTabItem(m_hWnd, NULL, FALSE);
        if (hWndDefFocus != NULL)
        {
            SendMessage(m_hWnd, WM_NEXTDLGCTL,
                        reinterpret_cast<WPARAM>(hWndDefFocus), TRUE);
        }
    }

    hwndParent = GetParent(m_hWnd);
    bool bWasEnabled = !EnableWindow(hwndParent, FALSE);

    HANDLE hAbortEvent = m_pScript->GetAbortEvent();
    _ASSERTE(hAbortEvent != NULL);

    // Run modal loop.
    while (!m_bDismissed)
    {
        DWORD dwWait = MsgWaitForMultipleObjects(1, &hAbortEvent, FALSE, INFINITE, QS_ALLINPUT);
        if (dwWait == WAIT_OBJECT_0)
        {
            m_bAborted = true;
            m_bDismissed = true;
        }
        else if (dwWait == WAIT_OBJECT_0 + 1)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    m_bDismissed = true;
                    break;
                }

                if (!IsDialogMessage(m_hWnd, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
        else
        {
            _ASSERTE(0);
            break;
        }
    }

    // Re-post any quit message we may have received so the next
    // outer modal loop can see it.
    if (msg.message == WM_QUIT)
    {
        PostQuitMessage((int)msg.wParam);
    }

    if (bWasEnabled)
    {
        EnableWindow(hwndParent, TRUE);
    }

    DestroyWindow(m_hWnd);
    m_hWnd = NULL;

    if (m_bAborted)
    {
        *result = 0;
        RaiseError(IDS_E_SCRIPTABORTED);
        return SALAUT_E_ABORT;
    }

    _ASSERTE(m_nExitCode > 0);
    *result = m_nExitCode;

    return S_OK;
}

/* static */ bool CSalamanderGuiForm::ClassNeeded()
{
    if (s_nClass == 0)
    {
        WNDCLASSEX wc = {
            0,
        };

        wc.cbSize = sizeof(wc);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = WndProcThunk;
        wc.cbWndExtra = DLGWINDOWEXTRA;
        wc.hInstance = g_hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = s_szClassName;

        if (!RegisterClassEx(&wc))
        {
            _ASSERTE(0);
            return false;
        }
    }

    ++s_nClass;
    return true;
}

/* static */ void CSalamanderGuiForm::ClassRelease()
{
    _ASSERTE(s_nClass > 0);
    if (--s_nClass == 0)
    {
        if (!UnregisterClass(s_szClassName, g_hInstance))
            TRACE_E("UnregisterClass(s_szClassName) has failed");
    }
}

HWND CSalamanderGuiForm::HwndNeeded()
{
    HWND hWnd;
    DWORD style, exStyle;

    if (m_hWnd != NULL)
    {
        return m_hWnd;
    }

    style = WS_OVERLAPPED | WS_CAPTION | WS_POPUP | WS_CLIPSIBLINGS |
            WS_SYSMENU;
    exStyle = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CONTROLPARENT;

    if (!ClassNeeded())
    {
        _ASSERTE(0);
        return NULL;
    }

    hWnd = CreateWindowEx(
        exStyle,
        s_szClassName,
        m_strText ? OLE2T(m_strText) : _T("Form"),
        style,
        0, 0, 0, 0,
        SalamanderGeneral->GetMainWindowHWND(), // parent
        NULL,                                   // menu
        g_hInstance,
        this);

    _ASSERTE(hWnd);

    m_hFont = NULL;
    m_bDestroyFont = false;

    if (SalIsWindowsVersionOrGreater(6, 0, 0))
    {
        LOGFONT lf = {
            0,
        };
        HDC hDC = GetDC(hWnd);

        lf.lfHeight = -MulDiv(9, GetDeviceCaps(hDC, LOGPIXELSY), 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfCharSet = ANSI_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        StringCchCopy(lf.lfFaceName, _countof(lf.lfFaceName), _T("Segoe UI"));

        ReleaseDC(hWnd, hDC);

        m_hFont = CreateFontIndirect(&lf);
        if (m_hFont)
        {
            m_bDestroyFont = true;
        }
    }

    if (m_hFont == NULL)
    {
        m_hFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }

    return hWnd;
}

/* static */ LRESULT CALLBACK CSalamanderGuiForm::WndProcThunk(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    CSalamanderGuiForm* that;
    LRESULT lres;

    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        that = reinterpret_cast<CSalamanderGuiForm*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
        _ASSERTE(that->m_hWnd == NULL);
        that->m_hWnd = hWnd;
    }
    else
    {
        that = reinterpret_cast<CSalamanderGuiForm*>(GetWindowLongPtrW(
            hWnd, GWLP_USERDATA));
    }

    if (that != NULL)
    {
        _ASSERT(that->m_hWnd == hWnd);
        lres = that->WndProc(uMsg, wParam, lParam);

        if (uMsg == WM_NCDESTROY)
        {
            // Final message.
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
            that->m_hWnd = NULL;
        }

        return lres;
    }

    return DefDlgProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CSalamanderGuiForm::WndProc(
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
    {
        _ASSERTE(m_nCloseCode != -1);
        DismissDialog(m_nCloseCode);
        return 0;
    }

    case WM_DESTROY:
    {
        _ASSERTE(m_nExitCode != -1 || m_bAborted);

        if (m_bDestroyFont)
        {
            _ASSERTE(m_hFont != NULL);
            DeleteObject(m_hFont);
            m_hFont = NULL;
            m_bDestroyFont = false;
        }
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            HWND hwndFrom = reinterpret_cast<HWND>(lParam);
            CSalamanderGuiComponentBase* pComponent = NULL;

            if (hwndFrom)
            {
                pComponent = reinterpret_cast<CSalamanderGuiComponentBase*>(
                    GetProp(hwndFrom, s_szInstancePropName));

                ISalamanderGuiButton* pButton;
                if (pComponent && SUCCEEDED(pComponent->QueryInterface(__uuidof(pButton), (void**)&pButton)))
                {
                    int nResult;

                    pButton->get_DialogResult(&nResult);
                    if (nResult > 0)
                    {
                        DismissDialog(nResult);
                    }

                    pButton->Release();
                }
            }
            else if (LOWORD(wParam) == IDCANCEL)
            {
                // This is command synthetized by the
                // dialog manager in reaction to the
                // Esc key when there is no button.
                if (m_nCloseCode != -1)
                {
                    DismissDialog(m_nCloseCode);
                }
            }
            else if (LOWORD(wParam) == IDOK)
            {
                // This is command synthetized by the
                // dialog manager in reaction to the
                // Enter key when there is no button.
            }
        }

        break;
    }
    }

    return DefDlgProc(m_hWnd, uMsg, wParam, lParam);
}

void CSalamanderGuiForm::InternalAddComponent(
    ISalamanderGuiComponentInternal* pComponent)
{
    int iTop = 0, iLeft = 8;
    ISalamanderGuiComponentInternal* pPrevComponent = NULL;

    // Find the previously added component.
    if (m_pChild != NULL)
    {
        ISalamanderGuiComponentInternal* pIter = m_pChild;

        while (pIter)
        {
            pPrevComponent = pIter;
            pIter->GetSibling(&pIter);
        }
    }

    __super::InternalAddComponent(pComponent);

    HwndNeeded();

    // Give the component chance to autosize.
    AutosizeComponent(pComponent);

    bool bModified = false;
    CBounds bounds;
    pComponent->GetBounds(&bounds);

    _ASSERTE(bounds.cx >= 0);
    _ASSERTE(bounds.cy >= 0);

    // Calculate automatic position of the component.
    // Put component bellow the previous by default and put
    // some spacing between them.
    int yspacing = 0;
    if (pPrevComponent != NULL)
    {
        CBounds prevBounds;
        pPrevComponent->GetBounds(&prevBounds);

        iTop = prevBounds.y + prevBounds.cy;

        if (pPrevComponent->GetClass() == SALGUI_LABEL &&
            pComponent->GetClass() != SALGUI_LABEL)
        {
            // If the previous component was label,
            // shrink spacing since the label is associated
            // with this component.
            yspacing = 2;
        }
        else
        {
            if (pComponent->GetClass() == SALGUI_BUTTON)
            {
                if (pPrevComponent->GetClass() == SALGUI_BUTTON)
                {
                    // Align successive button controls
                    // in a row.
                    iTop = prevBounds.y;
                    iLeft = prevBounds.x + prevBounds.cx + 8;
                }
                else
                {
                    // Put larger spacing above the buttons.
                    yspacing = 8;
                }
            }
            else
            {
                // Default spacing between components.
                yspacing = 4;
            }
        }

        // If some attributes of the previous component (most notably
        // the Text property) were changed, its dimension might
        // change as well. Expand bounds of the form to hold resized
        // component.
        UnionBounds(prevBounds);
    }
    else
    {
        iTop = 8;
    }

    iTop += yspacing;

    if (bounds.x == CW_USEDEFAULT)
    {
        bModified = true;
        bounds.x = iLeft;
    }

    if (bounds.y == CW_USEDEFAULT)
    {
        bModified = true;
        bounds.y = iTop;
    }

    if (bModified)
    {
        pComponent->SetBounds(&bounds);
    }

    UnionBounds(bounds);
}

void CSalamanderGuiForm::OnChildCreated(ISalamanderGuiComponentInternal* pComponent)
{
    // Assign nice font to the control.
    _ASSERTE(m_hFont != NULL);
    SendMessage(pComponent->GetHwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), 0);
}

void STDMETHODCALLTYPE CSalamanderGuiForm::BoundsToPixels(
    /* [out][in] */ struct SALGUI_BOUNDS* bounds)
{
    // Convert dialog units to pixels.
    _ASSERTE(m_dluBase.cx > 0);
    bounds->x = bounds->x * m_dluBase.cx / 4;
    bounds->y = bounds->y * m_dluBase.cy / 8;
    bounds->cx = bounds->cx * m_dluBase.cx / 4;
    bounds->cy = bounds->cy * m_dluBase.cy / 8;
}

void STDMETHODCALLTYPE CSalamanderGuiForm::PixelsToBounds(
    /* [out][in] */ struct SALGUI_BOUNDS* bounds)
{
    // Convert pixels to dialog units.
    _ASSERTE(m_dluBase.cx > 0);
    bounds->x = 4 * bounds->x / m_dluBase.cx;
    bounds->y = 8 * bounds->y / m_dluBase.cy;
    bounds->cx = 4 * bounds->cx / m_dluBase.cx;
    bounds->cy = 8 * bounds->cy / m_dluBase.cy;
}

void STDMETHODCALLTYPE CSalamanderGuiForm::AutosizeComponent(
    ISalamanderGuiComponentInternal* pComponent)
{
    _ASSERTE(m_hWnd != NULL);
    HDC hDC = GetDC(m_hWnd);
    _ASSERTE(hDC != NULL);

    HFONT hPrevFont = NULL;
    _ASSERTE(m_hFont);
    hPrevFont = reinterpret_cast<HFONT>(SelectObject(hDC, m_hFont));

    if (m_dluBase.cx < 0)
    {
        // Retrieve base metrics for dialog units.
        TEXTMETRIC tm;
        GetTextMetrics(hDC, &tm);
        m_dluBase.cx = tm.tmAveCharWidth;
        m_dluBase.cy = tm.tmHeight;
    }

    pComponent->RecalcBounds(m_hWnd, hDC);

    if (hPrevFont != NULL)
    {
        SelectObject(hDC, hPrevFont);
    }

    ReleaseDC(m_hWnd, hDC);
}

void CSalamanderGuiForm::UnionBounds(const SALGUI_BOUNDS& bounds)
{
    _ASSERTE(bounds.x != CW_USEDEFAULT);
    _ASSERTE(bounds.y != CW_USEDEFAULT);
    _ASSERTE(bounds.cx != CW_USEDEFAULT);
    _ASSERTE(bounds.cy != CW_USEDEFAULT);

    if (m_bounds.cx == CW_USEDEFAULT || bounds.x + bounds.cx > m_bounds.cx)
    {
        m_bounds.cx = bounds.x + bounds.cx;
    }

    if (m_bounds.cy == CW_USEDEFAULT || bounds.y + bounds.cy > m_bounds.cy)
    {
        m_bounds.cy = bounds.y + bounds.cy;
    }
}

void CSalamanderGuiForm::AlignButtons()
{
    // Contains pointers to standard buttons (have the DialogResult
    // property set) added on the form. The array is indexed by
    // the exit codes (IDOK, IDCANCEL...). Element zero is spare.
    // Elements 16, 17 and 18 are specific buttons, see
    // ButtonToFriendlyNumber function.
    ISalamanderGuiComponentInternal* apButtons[MAX_EXIT_BUTTONS] = {
        0,
    };
    bool bHasDefaultButton = false;
    ISalamanderGuiComponentInternal* pFirstExitBtn = NULL;
    int cExitButtons = 0;
    HWND hwndBtn;

    if (m_pChild)
    {
        ISalamanderGuiComponentInternal* pIter;
        RECT rcBounds;
        ISalamanderGuiComponentInternal* pFirstBtn = NULL;
        int cButtons;

        pIter = m_pChild;
        while (pIter)
        {
            ISalamanderGuiButton* pBtn;

            if (SUCCEEDED(pIter->QueryInterface(__uuidof(pBtn), (void**)&pBtn)))
            {
                RECT rcBtn;

                hwndBtn = pIter->GetHwnd();
                _ASSERTE(IsWindow(hwndBtn));
                GetWindowRect(hwndBtn, &rcBtn);
                MapWindowRect(NULL, m_hWnd, &rcBtn);
                if (pFirstBtn == NULL)
                {
                    rcBounds = rcBtn;
                    pFirstBtn = pIter;
                    cButtons = 1;
                }
                else
                {
                    if (rcBtn.top == rcBounds.top)
                    {
                        RECT rcTmp = rcBounds;
                        UnionRect(&rcBounds, &rcBtn, &rcTmp);
                        ++cButtons;
                    }
                    else
                    {
                        AlignButtonLine(pFirstBtn, cButtons, rcBounds);
                        pFirstBtn = pIter;
                        cButtons = 1;
                    }
                }

                int nResult;
                pBtn->get_DialogResult(&nResult);
                if (nResult > 0)
                {
                    if (nResult < _countof(apButtons) &&
                        apButtons[nResult] == NULL)
                    {
                        apButtons[nResult] = pIter;
                    }

                    if (pFirstExitBtn == NULL &&
                        nResult != IDCANCEL)
                    {
                        pFirstExitBtn = pIter;
                    }

                    ++cExitButtons;
                }

                pBtn->Release();
            }
            else if (pFirstBtn != NULL)
            {
                AlignButtonLine(pFirstBtn, cButtons, rcBounds);
                pFirstBtn = NULL;
            }

            pIter->GetSibling(&pIter);
        }

        if (pFirstBtn != NULL)
        {
            AlignButtonLine(pFirstBtn, cButtons, rcBounds);
        }
    }

    // Determine the caption bar close button action.
    if (cExitButtons == 0)
    {
        // If there is no button on the form (e.g. some form displaying
        // informative message only), enable the close button and let it
        // return the Cancel code.
        m_nCloseCode = IDCANCEL;
    }
    else if (cExitButtons == 1 && apButtons[IDOK])
    {
        // If OK is the only button, the close button will return
        // OK as well.
        m_nCloseCode = IDOK;
    }
    else if (apButtons[IDCANCEL])
    {
        // If there is a Cancel button on the form, make the close
        // button return the Cancel as well.
        m_nCloseCode = IDCANCEL;
    }
    else
    {
        // There is no appropriate action for the close button,
        // disable it. The user will have to use one of the push
        // buttons to close the dialog.
        m_nCloseCode = -1;
    }

    // Select the default button.
    if (!bHasDefaultButton && cExitButtons > 0)
    {
        ISalamanderGuiComponentInternal* pDefButton = NULL;

        if (apButtons[IDOK])
        {
            // If there is the OK button, make it the default.
            pDefButton = apButtons[IDOK];
        }
        else
        {
            // If there is no OK button, pick the the first
            // button (except the Cancel button, which has the
            // Esc shortcut).
            pDefButton = pFirstExitBtn;
            if (pDefButton == NULL && cExitButtons == 1 && apButtons[IDCANCEL])
            {
                // Cancel will be default if there is no
                // other button.
                pDefButton = apButtons[IDCANCEL];
            }
        }

        if (pDefButton != NULL)
        {
            DWORD dwStyle;
            hwndBtn = pDefButton->GetHwnd();
            _ASSERTE(IsWindow(hwndBtn));
            dwStyle = GetWindowLong(hwndBtn, GWL_STYLE);
            dwStyle |= BS_DEFPUSHBUTTON;
            SetWindowLong(hwndBtn, GWL_STYLE, dwStyle);
        }
    }
}

void CSalamanderGuiForm::AlignButtonLine(
    ISalamanderGuiComponentInternal* pFirstBtn,
    int cButtons,
    const RECT& rcBounds)
{
    RECT rcClient;
    int xshift;
    int cxClient, cxButtons;

    // Center the buttons bounding rect in the form client rect,
    // calculate x-shift offset and the shift all the buttons by
    // that offset.
    GetClientRect(m_hWnd, &rcClient);
    cxClient = rcClient.right;
    cxButtons = rcBounds.right - rcBounds.left;
    _ASSERTE(cxButtons <= cxClient);
    if (cxButtons < cxClient)
    {
        xshift = (cxClient - cxButtons) / 2;

        // subtract left margin that was added by the
        // autolayout algorithm
        if (xshift > 0)
        {
            CBounds margin;
            margin.Empty();
            margin.cy = 8;
            BoundsToPixels(&margin);
            xshift -= margin.cy;
        }

        if (xshift > 0)
        {
            ISalamanderGuiComponentInternal* pIter;

            pIter = pFirstBtn;

            while (cButtons)
            {
                MoveWindowRelative(pIter->GetHwnd(), xshift, 0);
                pIter->GetSibling(&pIter);
                --cButtons;
            }
        }
    }
}

void CSalamanderGuiForm::MoveWindowRelative(
    HWND hWnd,
    int dx,
    int dy)
{
    RECT rc;
    HWND hwndParent;

    if (dx == 0 && dy == 0)
    {
        return;
    }

    _ASSERTE(IsWindow(hWnd));

    GetWindowRect(hWnd, &rc);

    hwndParent = GetParent(hWnd);
    if (hwndParent != NULL)
    {
        MapWindowRect(NULL, hwndParent, &rc);
    }

    SetWindowPos(hWnd, NULL, rc.left + dx, rc.top + dy, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void CSalamanderGuiForm::DismissDialog(int nExitCode)
{
    _ASSERTE(!m_bDismissed);
    _ASSERTE(m_nExitCode == -1);
    _ASSERTE(nExitCode > 0);
    _ASSERTE(IsWindow(m_hWnd));

    m_nExitCode = nExitCode;
    m_bDismissed = true;

    // Wake up the message loop.
    PostMessage(m_hWnd, WM_NULL, 0, 0);
}

void STDMETHODCALLTYPE CSalamanderGuiForm::RecalcBounds(
    HWND hWnd,
    HDC hDC)
{
    if (m_pChild)
    {
        ISalamanderGuiComponentInternal* pIter;
        SALGUI_BOUNDS bounds;

        pIter = m_pChild;
        while (pIter)
        {
            pIter->GetBounds(&bounds);
            UnionBounds(bounds);
            pIter->GetSibling(&pIter);
        }
    }
}

void CSalamanderGuiForm::ResizeFullWidthComponents()
{
    if (m_pChild)
    {
        CBounds margin;
        margin.Empty();
        margin.cx = 8;
        BoundsToPixels(&margin);

        RECT rcForm;
        _ASSERTE(IsWindow(m_hWnd));
        GetClientRect(m_hWnd, &rcForm);

        ISalamanderGuiComponentInternal* pIter = m_pChild;
        while (pIter)
        {
            if (pIter->GetStyle() & SALGUI_STYLE_FULLWIDTH)
            {
                HWND hwndComponent = pIter->GetHwnd();
                _ASSERTE(IsWindow(hwndComponent));

                RECT rcComponent;
                GetWindowRect(hwndComponent, &rcComponent);
                MapWindowRect(NULL, m_hWnd, &rcComponent);

                // Extend component width to the right
                // edge of the form (minus margin).
                int cxForm = rcForm.right - rcForm.left;
                int cxCurrent = rcComponent.right - rcComponent.left;
                int cxNew = cxForm - rcComponent.left - margin.cx;
                _ASSERTE(cxNew > 0 && cxNew >= cxCurrent);

                SetWindowPos(hwndComponent, NULL, 0, 0,
                             cxNew, rcComponent.bottom - rcComponent.top,
                             SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
            }

            pIter->GetSibling(&pIter);
        }
    }
}
