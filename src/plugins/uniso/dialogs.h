// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// CCommonDialog
//
// Dialog centered relative to the parent
//

class CCommonDialog : public CDialog
{
public:
    CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin = ooStandard);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();
};

//****************************************************************************
//
// CConfigurationDialog
//

class CConfigurationDialog : public CCommonDialog
{
private:
public:
    CConfigurationDialog(HWND hParent);

    virtual void Transfer(CTransferInfo& ti);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
