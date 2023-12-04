// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// vim: noexpandtab:sw=8:ts=8

/*
	PE File Viewer Plugin for Open Salamander

	Copyright (c) 2015-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2015-2023 Open Salamander Authors

	cfgdlg.h
	Configuration dialog box.
*/

#ifdef _MSC_VER
#pragma once
#endif

class CPeViewerConfigDialog : public CDialog
{
protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    void OnCommand(int nCmd);

    void EnableControls();

    void FillListBoxes();
    void BuildConfigurationChain();

    int InsertItem(HWND hwndLB, const struct _CFG_DUMPER* pDumperCfg, int index, bool bSelect);

    int AddItem(HWND hwndLB, const struct _CFG_DUMPER* pDumperCfg, bool bSelect = false)
    {
        return InsertItem(hwndLB, pDumperCfg, -1, bSelect);
    }

    void MoveItem(HWND hwndSourceLB, HWND hwndDestinationLB);
    void MoveItem(HWND hwndLB, int nRelativeIndex);

public:
    CPeViewerConfigDialog(HWND hwndParent);

    virtual void Transfer(CTransferInfo& ti);
};
