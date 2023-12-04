// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------
#include "spl_menu.h"
//---------------------------------------------------------------------------
class CPluginFSInterface;
//---------------------------------------------------------------------------
class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract,
                                   protected CSalamanderGeneralLocal
{
public:
    CPluginInterfaceForMenuExt(CPluginInterface* APlugin);
    virtual DWORD WINAPI GetMenuItemState(int Id, DWORD EventMask);
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* Salamander,
                                        HWND Parent, int Id, DWORD EventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND Parent, int Id);
    virtual void WINAPI BuildMenu(HWND Parent, CSalamanderBuildMenuAbstract* Salamander) {}
};
//---------------------------------------------------------------------------
