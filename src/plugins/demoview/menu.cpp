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
// SEKCE MENU
// ****************************************************************************

BOOL WINAPI
CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander,
                                            HWND parent, int id, DWORD eventMask)
{
    switch (id)
    {
    case MENUCMD_VIEWBMPFROMCLIP:
    {
        SalamanderGeneral->ShowMessageBox("TODO: Implement View Bitmap from Clipboard.", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        //      SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_SOURCE);  // tento prikaz povazujeme za praci s cestou (objevi se v Alt+F12)
        break;
    }

    default:
        SalamanderGeneral->ShowMessageBox("Unknown command.", LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        break;
    }
    return FALSE; // neodznacovat polozky v panelu
}

BOOL WINAPI
CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case MENUCMD_VIEWBMPFROMCLIP:
        helpID = IDH_VIEWBMPFROMCLIP;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}
