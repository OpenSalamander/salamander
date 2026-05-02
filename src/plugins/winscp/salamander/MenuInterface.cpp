// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#pragma hdrstop

#include "Salamand.h"
#include "MenuInterface.h"
#include "FileSystem.h"
#include "Salamander.rh"

#include <Common.h>
//---------------------------------------------------------------------------
CPluginInterfaceForMenuExt::CPluginInterfaceForMenuExt(CPluginInterface* APlugin) : CPluginInterfaceForMenuExtAbstract(), CSalamanderGeneralLocal(APlugin)
{
}
//---------------------------------------------------------------------------
DWORD WINAPI CPluginInterfaceForMenuExt::GetMenuItemState(int /*Id*/,
                                                          DWORD /*EventMask*/)
{
    assert(false);
    return 0;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginInterfaceForMenuExt::ExecuteMenuItem(
    CSalamanderForOperationsAbstract* /*Salamander*/, HWND Parent,
    int Id, DWORD /*EventMask*/)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::ExecuteMenuItem(%x, %d)",
                        Parent, Id);

    bool Result = FALSE;

    try
    {
        SetParentWindow(Parent);

        if (Id == pmConnect)
        {
            FPlugin->ConnectFileSystem(PANEL_SOURCE);
        }
        else if (Id == pmDisconnectF12)
        {
            SalamanderGeneral()->PostSalamanderCommand(SALCMD_DISCONNECT);
        }
        else if (Id == pmDisconnectLeft || Id == pmDisconnectRight)
        {
            int Panel = ((Id == pmDisconnectLeft) ? PANEL_LEFT : PANEL_RIGHT);
            CPluginFSInterface* FSInterface = GetPanelFS(Panel);
            if (FSInterface != NULL)
            {
                FSInterface->ForceClose();
            }
            ChangePanelPathOutOfPlugin(Panel);
        }
        else if (Id == pmFullSynchronize)
        {
            bool Source = true;
            CPluginFSInterface* FSInterface = GetPanelFS(PANEL_SOURCE);
            if (FSInterface == NULL)
            {
                FSInterface = GetPanelFS(PANEL_TARGET);
                Source = false;
            }
            FSInterface->FullSynchronize(Source);
        }
        else if (Id == pmSynchronize)
        {
            CPluginFSInterface* FSInterface = GetPanelFS(PANEL_SOURCE);
            if (FSInterface == NULL)
            {
                FSInterface = GetPanelFS(PANEL_TARGET);
            }
            FSInterface->Synchronize();
        }
        else
        {
            assert(false);
        }
    }
    catch (Exception& E)
    {
        ShowExtendedException(&E);
    }
    return Result;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginInterfaceForMenuExt::HelpForMenuItem(HWND /*Parent*/, int Id)
{
    int HelpID = 0;
    switch (Id)
    {
    case pmConnect:
        HelpID = IDH_UI_LOGIN;
        break;
    case pmDisconnectF12:
        HelpID = IDH_DISCONNECT_F12;
        break;
    case pmFullSynchronize:
        HelpID = IDH_DIR_SYNCHRONIZE;
        break;
    case pmSynchronize:
        HelpID = IDH_KEEPUPTODATE_SYNCHRONIZE;
        break;
    }
    if (HelpID != 0)
        PluginInterface.HelpContext(HelpID);
    return HelpID != 0;
}
//---------------------------------------------------------------------------
