// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef ASSERT
#define ASSERT assert
#endif

#define BEGIN_MSG_MAP() \
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) \
    { \
        if (0) \
        { \
            __assume(0); \
        }

#define END_MSG_MAP() \
    else return FALSE; \
    }

#define ON_WM_INITDIALOG(fn) \
    if (uMsg == WM_INITDIALOG) \
        return (INT_PTR)(BOOL)fn((HWND)wParam);

#define ON_COMMAND_ID(id, fn) \
    if (uMsg == WM_COMMAND && GET_WM_COMMAND_ID(wParam, lParam) == id) \
    { \
        fn((UINT)GET_WM_COMMAND_CMD(wParam, lParam), (int)GET_WM_COMMAND_ID(wParam, lParam), GET_WM_COMMAND_HWND(wParam, lParam)); \
        return 0; \
    }
// void OnCommand(UINT uCode, int nId, HWND hwndCtl);

#define ON_COMMAND_ID_SIMPLE(id, fn) \
    if (uMsg == WM_COMMAND && GET_WM_COMMAND_ID(wParam, lParam) == id) \
    { \
        fn(); \
        return 0; \
    }

#define ON_BN_CLICKED_SIMPLE(id, fn) \
    if (uMsg == WM_COMMAND && GET_WM_COMMAND_ID(wParam, lParam) == id && GET_WM_COMMAND_CMD(wParam, lParam) == BN_CLICKED) \
    { \
        fn(); \
        return 0; \
    }

#define ON_NOTIFICATION(id, cd, fn) \
    if (uMsg == WM_NOTIFY && cd == ((LPNMHDR)lParam)->code && id == ((LPNMHDR)lParam)->idFrom) \
    { \
        return (LRESULT)fn((LPNMHDR)lParam); \
    }

#define ON_MESSAGE(msg, fn) \
    if (uMsg == msg) \
    { \
        return fn(msg, wParam, lParam); \
    }

class CDialogImpl
{
public:
    HWND m_hWnd;
    bool m_bModal;

    CDialogImpl() : m_hWnd(NULL),
                    m_bModal(false)
    {
    }

    BEGIN_MSG_MAP()
    END_MSG_MAP()
    /*virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg) {
		default:
			return FALSE;
		}
	}*/

    bool IsNull()
    {
        return (m_hWnd == NULL);
    }

    INT_PTR DoModal(int nIdResourceTemplate, HWND hwndParent = NULL);
    HWND Create(int nIdResourceTemplate, HWND hwndParent = NULL);

    bool EndDialog(int nRetCode)
    {
        ASSERT(m_hWnd);
        ASSERT(m_bModal);
        bool res = ::EndDialog(m_hWnd, nRetCode) != FALSE;
        if (res)
            m_hWnd = NULL;
        return res;
    }

    bool DestroyWindow()
    {
        ASSERT(m_hWnd);
        ASSERT(!m_bModal);
        bool res = ::DestroyWindow(m_hWnd) != FALSE;
        if (res)
            m_hWnd = NULL;
        return res;
    }

    bool IsDlgButtonChecked(int nIdButton)
    {
        ASSERT(m_hWnd);
        return ::IsDlgButtonChecked(m_hWnd, nIdButton) == BST_CHECKED;
    }

    bool CheckDlgButton(int nIdButton, bool fCheck)
    {
        ASSERT(m_hWnd);
        return ::CheckDlgButton(m_hWnd, nIdButton,
                                fCheck ? BST_CHECKED : BST_UNCHECKED) != FALSE;
    }

    HWND GetDlgItem(int nId)
    {
        ASSERT(m_hWnd);
        return ::GetDlgItem(m_hWnd, nId);
    }

    bool EnableDlgItem(int nId, bool fEnable)
    {
        HWND hCtl;
        ASSERT(m_hWnd);
        hCtl = ::GetDlgItem(m_hWnd, nId);
        ASSERT(hCtl);
        return ::EnableWindow(hCtl, fEnable) != FALSE;
    }

    bool SetDlgItemText(int nId, LPCTSTR pszText)
    {
        ASSERT(m_hWnd);
        return ::SetDlgItemText(m_hWnd, nId, pszText) != FALSE;
    }

    UINT GetDlgItemText(int nId, LPTSTR pszText, int nMaxCount)
    {
        ASSERT(m_hWnd);
        return ::GetDlgItemText(m_hWnd, nId, pszText, nMaxCount);
    }

    LRESULT SendDlgItemMessage(int nId, UINT uMsg, WPARAM wParam = 0,
                               LPARAM lParam = 0)
    {
        ASSERT(m_hWnd);
        return ::SendDlgItemMessage(m_hWnd, nId, uMsg, wParam, lParam);
    }

    HWND GetParent()
    {
        ASSERT(m_hWnd);
        return ::GetParent(m_hWnd);
    }

    void Center()
    {
        extern CSalamanderGeneralAbstract* SalamanderGeneral;
        SalamanderGeneral->MultiMonCenterWindow(m_hWnd, GetParent(), FALSE);
    }
};
