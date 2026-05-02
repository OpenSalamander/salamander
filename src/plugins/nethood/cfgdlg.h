// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...

	Network cache.
*/

#ifdef _MSC_VER
#pragma once
#endif

class CNethoodConfigDialog : public CDialog
{
protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void EnableDisableCheckBox();

    /// Refreshes panels after the configuration options are modified.
    void RefreshPanel(int iPanel);

    /// Refreshes panels after the dialog is closed.
    void RefreshPanels()
    {
        RefreshPanel(PANEL_LEFT);
        RefreshPanel(PANEL_RIGHT);
    }

public:
    CNethoodConfigDialog(__in HWND hwndParent);

    virtual void Transfer(__in CTransferInfo& ti);
};
