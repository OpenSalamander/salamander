// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	cfgdlg.h
	Configuration dialog.
*/

#pragma once

class CAutomationConfigDialog : public CDialog
{
protected:
    HWND m_hwndSubclassedEdit;
    WNDPROC m_pfnSubclassedEdit;
    static const _TCHAR SublassPropName[];
    RECT m_rcBrowseBtn;
    int m_nBrowseBtnState;
    CGUIToolbarHeaderAbstract* m_pHeader;
    bool m_bMouseCaptured;
    bool m_bBrowsing;

    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    enum
    {
        WM_USER_RECALCWIDTH = WM_USER + 'r',
        BROWSEBTN_WIDTH = 16,
    };

    BOOL OnDirListNotify(NMHDR* pnmhdr);
    BOOL OnDirListKeyDown(NMLVKEYDOWN* pnmkey);
    BOOL OnDirListBeginLabelEdit(NMLVDISPINFO* nmlv);
    BOOL OnDirListEndLabelEdit(NMLVDISPINFO* nmlv);
    BOOL OnDirListItemChanged(NMLISTVIEW* nmlv);

    void OnInitDialog();
    BOOL OnHeaderCommand(int nId);

    void DirSelChanged();

    BOOL EditDirectory();
    BOOL DeleteDirectory();
    BOOL NewDirectory();
    BOOL MoveDirectory(int iDelta);

    void SubclassLabelEdit(HWND hwndEdit);
    void UnsubclassLabelEdit();
    static LRESULT CALLBACK LabelEditSubclassProc(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);

    void DrawBrowseBtn();
    void OnBrowseBtnClicked();
    void InsertVariable(PCTSTR pszVar, int nCaretPos = -1);

    bool ScreenPointInButtonRect(POINTS pt)
    {
        POINT pt2;

        pt2.x = pt.x;
        pt2.y = pt.y;
        return ScreenPointInButtonRect(pt2);
    }

    bool ScreenPointInButtonRect(POINT pt);

public:
    CAutomationConfigDialog(HWND hwndParent);

    virtual void Transfer(CTransferInfo& ti);
};
