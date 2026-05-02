// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CPluginInterfaceForMenuExt
//

DWORD
CPluginInterfaceForMenuExt::GetMenuItemState(int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::GetMenuItemState(%d, 0x%X)",
                        id, eventMask);
    return 0;
}

BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                                 int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::ExecuteMenuItem(, , %d, 0x%X)",
                        id, eventMask);
    PARENT(parent);
    BOOL ret = FALSE;
    switch (id)
    {
    case MID_RENAME:
    {
        SG->GetConfigParameter(SALCFG_ALWAYSONTOP, &AlwaysOnTop, sizeof(AlwaysOnTop), NULL);
        if (SG->GetPanelSelection(PANEL_SOURCE, NULL, NULL))
        {
            CRenamerDialogThread* t = new CRenamerDialogThread();
            if (t)
            {
                if (!t->Create(ThreadQueue))
                    delete t;
            }
            else
                Error(IDS_LOWMEM);
        }
        break;
    }

    case MID_UNDO:
    {
        SG->ShowMessageBox("Undo", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        break;
    }
    }
    return ret;
};

BOOL CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    if (id == MID_RENAME)
        SG->OpenHtmlHelp(parent, HHCDisplayContext, IDH_BATCHRENAME, FALSE);
    return id == MID_RENAME;
}
