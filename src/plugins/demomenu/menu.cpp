// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"

// ****************************************************************************
// MENU SECTION
// ****************************************************************************

BOOL WINAPI
CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander,
                                            HWND parent, int id, DWORD eventMask)
{
    switch (id)
    {
    case MENUCMD_TESTCMD:
    {
        SalamanderGeneral->ShowMessageBox("You are trying Test Command.", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        //      SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_SOURCE);  // Treat this command as operating on the path (it appears in Alt+F12)
        break;
    }

    default:
        SalamanderGeneral->ShowMessageBox("Unknown command.", LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        break;
    }
    return FALSE; // do not unselect items in the panel
}

BOOL WINAPI
CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case MENUCMD_TESTCMD:
        helpID = IDH_TESTCMD;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}
