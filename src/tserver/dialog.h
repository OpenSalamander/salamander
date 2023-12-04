// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// CAboutDialog
//

class CAboutDialog : public CDialog
{
public:
    CAboutDialog(HINSTANCE modul, int resID, HWND parent)
        : CDialog(modul, resID, parent){};

    virtual void Transfer(CTransferInfo& ti);
};

//****************************************************************************
//
// CCenterDialog
//

class CCenterDialog : public CDialog
{
public:
    CCenterDialog(HINSTANCE modul, int resID, HWND parent)
        : CDialog(modul, resID, parent){};

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CDetailsDialog
//

class CDetailsDialog : public CDialog
{
protected:
    const CGlobalDataMessage* Message;

public:
    CDetailsDialog(HWND parent, const CGlobalDataMessage* message);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CPSPGeneral
//

class CPSPGeneral : public CPropSheetPage
{
public:
    CPSPGeneral(const WCHAR* title, HINSTANCE modul, int resID,
                DWORD flags = PSP_USETITLE, HICON icon = NULL,
                CObjectOrigin origin = ooStatic)
        : CPropSheetPage(title, modul, resID, flags, icon, origin)
    {
    }

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

    void EnableControls();

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CPSPView
//

class CPSPView : public CPropSheetPage
{
public:
    CPSPView(const WCHAR* title, HINSTANCE modul, int resID,
             DWORD flags = PSP_USETITLE, HICON icon = NULL,
             CObjectOrigin origin = ooStatic)
        : CPropSheetPage(title, modul, resID, flags, icon, origin)
    {
    }

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

BOOL DoSetupDialog(HWND hWindow);
