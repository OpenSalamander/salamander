// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	inputbox.h
	The InputBox dialog box.
*/

#pragma once

#include "dialogimpl.h"
#include "abortmodal.h"
#include "lang\lang.rh"

class CInputBoxDlg : public CDialogImpl
{
private:
    BSTR m_strTitle;
    BSTR m_strPrompt;
    BSTR m_strDefault;
    BSTR m_strResponse;
    CGUIStaticTextAbstract* m_pPromptText;
    CScriptInfo* m_pScript;
    bool m_bAborted;
    HWND m_hwndPreviousAbortTarget;

public:
    CInputBoxDlg(
        __in CScriptInfo* pScript,
        __in BSTR strPrompt,
        __in_opt BSTR strTitle,
        __in_opt BSTR strDefault)
    {
        _ASSERTE(pScript != NULL);
        m_pScript = pScript;

        m_strTitle = strTitle;
        m_strPrompt = strPrompt;
        m_strDefault = strDefault;
        m_strResponse = NULL;
        m_pPromptText = NULL;
        m_bAborted = false;
        m_hwndPreviousAbortTarget = NULL;
    }

    BEGIN_MSG_MAP()
    ON_WM_INITDIALOG(OnInitDialog)
    ON_COMMAND_ID_SIMPLE(IDOK, OnOk)
    ON_COMMAND_ID_SIMPLE(IDCANCEL, OnCancel)
    ON_MESSAGE(WM_NCDESTROY, OnNcDestroy)
    ON_MESSAGE(WM_SAL_ABORTMODAL, OnSalAbortModal)
    END_MSG_MAP()

    BOOL OnInitDialog(HWND)
    {
        _ASSERTE(m_bModal);

        m_hwndPreviousAbortTarget = m_pScript->SetAbortTargetHwnd(m_hWnd);

        Center();

        if (m_strPrompt && *m_strPrompt)
        {
            DWORD dwFlags = 0;
            LPCOLESTR s;
            OLECHAR chSep = 0;

            s = m_strPrompt;
            while (*s)
            {
                if (*s == L'\\' || *s == L'/') // contains slash, probably file path
                {
                    chSep = *s;
                    dwFlags = STF_PATH_ELLIPSIS;
                    break;
                }
                ++s;
            }

            m_pPromptText = SalamanderGUI->AttachStaticText(m_hWnd, IDC_PROMPT, dwFlags);
            if (chSep)
            {
                m_pPromptText->SetPathSeparator((char)chSep);
            }

            m_pPromptText->SetText(OLE2A(m_strPrompt));

            //SetDlgItemTextW(m_hWnd, IDC_PROMPT, m_strPrompt);
        }

        if (m_strDefault && *m_strDefault)
        {
            SetDlgItemTextW(m_hWnd, IDC_INPUTBOX, m_strDefault);
        }

        if (m_strTitle && *m_strTitle)
        {
            SetWindowTextW(m_hWnd, m_strTitle);
        }

        return TRUE;
    }

    void OnOk()
    {
        HWND hWndInput = GetDlgItem(IDC_INPUTBOX);
        int len = GetWindowTextLength(hWndInput);
        if (len > 0)
        {
            _ASSERTE(m_strResponse == NULL);
            m_strResponse = SysAllocStringLen(NULL, len);      // nul excluded
            GetWindowTextW(hWndInput, m_strResponse, len + 1); // nul included
        }

        EndDialog(IDOK);
    }

    void OnCancel()
    {
        EndDialog(IDCANCEL);
    }

    INT_PTR OnNcDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
    {
        m_pScript->SetAbortTargetHwnd(m_hwndPreviousAbortTarget);
        return 0;
    }

    INT_PTR OnSalAbortModal(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
    {
        m_bAborted = true;
        EndDialog(0);
        return 0;
    }

    BSTR GetResponse() const
    {
        return m_strResponse;
    }

    bool WasAborted() const
    {
        return m_bAborted;
    }
};
