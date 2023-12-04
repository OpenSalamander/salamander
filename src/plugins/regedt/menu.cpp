// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CPluginInterfaceForMenuExt
//

BOOL CPluginInterfaceForMenuExt::PostFocusCommand(const char* path,
                                                  const char* name)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::PostFocusCommand(%s, %s)",
                        path, name);
    if (!SG->SalamanderIsNotBusy(NULL))
        return FALSE;

    strcpy(Path, path);
    strcpy(Name, name);

    SG->PostMenuExtCommand(MID_FOCUS, TRUE);

    // dojde k prepnuti do jineho okna, takze teoreticky tenhle Sleep
    // nicemu nebude vadit
    Sleep(500);

    // po 0.5 sekunde uz o fokus nestojime (resi pripad, kdy jsme trefili
    // zacatek BUSY rezimu Salamandera)
    Path[0] = 0;
    Name[0] = 0;

    return TRUE;
}

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
    case MID_FIND:
    {
        SG->GetConfigParameter(SALCFG_ALWAYSONTOP, &AlwaysOnTop, sizeof(AlwaysOnTop), NULL);

        CPluginFSInterface* fs = (CPluginFSInterface*)SG->GetPanelPluginFS(PANEL_SOURCE);
        WCHAR path[MAX_FULL_KEYNAME];
        int len = StrToWStr(path, MAX_FULL_KEYNAME, AssignedFSName);
        path[len - 1] = L':';
        if (!fs ||
            !fs->GetCurrentPathW(path + len, MAX_FULL_KEYNAME))
        {
            wcscpy(path + len, L"\\");
        }
        CFindDialogThread* t = new CFindDialogThread(path);
        if (t)
        {
            if (!t->Create(ThreadQueue))
                delete t;
        }
        else
            Error(IDS_LOWMEM);
        break;
    }

    case MID_NEWKEY:
    {
        SG->PostSalamanderCommand(SALCMD_CREATEDIRECTORY);
        break;
    }

    case MID_NEWVAL:
    {
        CPluginFSInterface* fs = (CPluginFSInterface*)SG->GetPanelPluginFS(PANEL_SOURCE);
        if (fs)
            fs->EditNewFile();
        break;
    }

    case MID_EXPORT:
    {
        CPluginFSInterface* fs = (CPluginFSInterface*)SG->GetPanelPluginFS(PANEL_SOURCE);
        WCHAR path[MAX_FULL_KEYNAME];
        int len = StrToWStr(path, MAX_FULL_KEYNAME, AssignedFSName);
        path[len - 1] = L':';
        if (fs && fs->GetCurrentPathW(path + len, MAX_FULL_KEYNAME))
        {
            BOOL isDir;
            const CFileData* fd = SG->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
            CPluginData* pd = (CPluginData*)fd->PluginData;
            if (isDir && pd && pd->Name && wcscmp(pd->Name, L"..") != 0)
            {
                PathAppend(path, pd->Name, MAX_FULL_KEYNAME);
            }
        }
        else
            wcscpy(path + len, L"\\");
        ExportKey(path);
        break;
    }

    case MID_FOCUS:
    {
        // jen pokud jsme nemeli smulu (netrefili jsme zacatek BUSY rezimu Salamandera)
        if (Path[0] != 0)
        {
            SetForegroundWindow(SG->GetMainWindowHWND());
            SG->ChangePanelPathToPluginFS(PANEL_SOURCE, AssignedFSName, Path, NULL,
                                          -1, Name);
            Path[0] = 0;
            Name[0] = 0;
        }
        break;
    }
    }
    return ret;
}

BOOL WINAPI
CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case MID_FIND:
        helpID = IDH_SEARCHREG;
        break;
    case MID_NEWKEY:
        helpID = IDH_NEWKEY;
        break;
    case MID_NEWVAL:
        helpID = IDH_NEWVALUE;
        break;
    case MID_EXPORT:
        helpID = IDH_EXPORT;
        break;
    }
    if (helpID != 0)
        SG->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}
