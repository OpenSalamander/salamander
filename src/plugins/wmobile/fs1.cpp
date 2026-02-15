// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// FS-name assigned by Salamander after loading the plugin
char AssignedFSName[MAX_PATH] = "";

// global variables used to store pointers to Salamander's global variables
// shared for both the archive and the FS
const CFileData** TransferFileData = NULL;
int* TransferIsDir = NULL;
char* TransferBuffer = NULL;
int* TransferLen = NULL;
DWORD* TransferRowData = NULL;
CPluginDataInterfaceAbstract** TransferPluginDataIface = NULL;
DWORD* TransferActCustomData = NULL;

// ****************************************************************************
// FILE SYSTEM SECTION
// ****************************************************************************

BOOL InitFS()
{
    return TRUE;
}

void ReleaseFS()
{
}

//
// ****************************************************************************
// CPluginInterfaceForFS
//

CPluginFSInterfaceAbstract* WINAPI
CPluginInterfaceForFS::OpenFS(const char* fsName, int fsNameIndex)
{
    if (!CRAPI::Init())
        return NULL;

    ActiveFSCount++;
    return new CPluginFSInterface;
}

void WINAPI
CPluginInterfaceForFS::CloseFS(CPluginFSInterfaceAbstract* fs)
{
    CPluginFSInterface* dfsFS = (CPluginFSInterface*)fs; // ensure the correct destructor is called

    ActiveFSCount--;

    if (dfsFS != NULL)
        delete dfsFS;

    if (ActiveFSCount == 0)
        CRAPI::UnInit();
}

void WINAPI
CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(int panel)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", panel);

    //JR Start at the root
    SalamanderGeneral->ChangePanelPathToPluginFS(panel, AssignedFSName, "\\"); //JR x:
}

BOOL WINAPI
CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                      CPluginFSInterfaceAbstract* pluginFS,
                                                      const char* pluginFSName, int pluginFSNameIndex,
                                                      BOOL isDetachedFS, BOOL& refreshMenu,
                                                      BOOL& closeMenu, int& postCmd, void*& postCmdParam)
{
    CALL_STACK_MESSAGE7("CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(, %d, %d, %d, , %s, %d, %d, , , ,)",
                        panel, x, y, pluginFSName, pluginFSNameIndex, isDetachedFS);
    // The Windows Mobile plugin has no context Change Drive menu
    return FALSE;
}

void WINAPI
CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(%d, %d,)", panel, postCmd);
}

void WINAPI
CPluginInterfaceForFS::ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                   const char* pluginFSName, int pluginFSNameIndex,
                                   CFileData& file, int isDir)
{
    CPluginFSInterface* fs = (CPluginFSInterface*)pluginFS;
    if (isDir) // subdirectory or up-dir
    {
        char newPath[MAX_PATH];
        strcpy(newPath, fs->Path);

        if (isDir == 2) // up-dir
        {
            char* cutDir = NULL;
            if (SalamanderGeneral->CutDirectory(newPath, &cutDir)) // shorten the path by the last component
            {
                int topIndex; // next top index, -1 -> invalid
                if (!fs->TopIndexMem.FindAndPop(newPath, topIndex))
                    topIndex = -1;
                // change the path in the panel
                SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, newPath, NULL,
                                                             topIndex, cutDir);
            }
        }
        else // subdirectory
        {
            // backup of data for TopIndexMem (backupPath + topIndex)
            char backupPath[MAX_PATH];
            strcpy(backupPath, newPath);
            int topIndex = SalamanderGeneral->GetPanelTopIndex(panel);

            if (CRAPI::PathAppend(newPath, file.Name, MAX_PATH))
            {
                // change the path in the panel
                if (SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, newPath))
                    fs->TopIndexMem.Push(backupPath, topIndex); // remember the top index for the return
            }
        }
    }
    else
    {
        char cmdLine[MAX_PATH], *command = NULL, *params = NULL;
        strcpy(cmdLine, fs->Path);
        if (!CRAPI::PathAppend(cmdLine, file.Name, MAX_PATH))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_NAMETOOLONG),
                                              TitleWMobileError, MSGBOX_ERROR);
            return;
        }

        int l = (int)strlen(cmdLine);
        if (l > 4)
        {
            if (SalamanderGeneral->StrICmp(cmdLine + l - 4, ".lnk") == 0)
            {
                if (CRAPI::SHGetShortcutTarget(cmdLine, cmdLine, MAX_PATH))
                {
                    command = cmdLine;
                    if (*command == '"')
                    {
                        command++;
                        char* end = command + strlen(command) - 1;
                        if (*end == '"')
                            *end = 0;
                    }
                    else
                    {
                        params = strchr(command, ' ');
                        if (params)
                        {
                            *params = 0;
                            params++;
                        }
                    }
                }
            }
            else if (SalamanderGeneral->StrICmp(cmdLine + l - 4, ".exe") == 0)
                command = cmdLine;

            if (command != 0 && command[0] != 0)
            {
                if (!CRAPI::CreateProcess(command, params))
                {
                    DWORD err = CRAPI::GetLastError();
                    SalamanderGeneral->ShowMessageBox(SalamanderGeneral->GetErrorText(err), TitleWMobileError, MSGBOX_ERROR);
                }
            }
        }
    }
}

BOOL WINAPI
CPluginInterfaceForFS::DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                    CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForFS::DisconnectFS(, %d, %d, , %s, %d)",
                        isInPanel, panel, pluginFSName, pluginFSNameIndex);
    BOOL ret = FALSE;
    if (isInPanel)
    {
        SalamanderGeneral->DisconnectFSFromPanel(parent, panel);
        ret = SalamanderGeneral->GetPanelPluginFS(panel) != pluginFS;
    }
    else
    {
        ret = SalamanderGeneral->CloseDetachedFS(parent, pluginFS);
    }
    return ret;
}

//****************************************************************************
//
// CTopIndexMem
//

void CTopIndexMem::Push(const char* path, int topIndex)
{
    // determine whether the path follows Path (path==Path+"\\name")
    const char* s = path + strlen(path);
    if (s > path && *(s - 1) == '\\')
        s--;
    BOOL ok;
    if (s == path)
        ok = FALSE;
    else
    {
        if (s > path && *s == '\\')
            s--;
        while (s > path && *s != '\\')
            s--;

        int l = (int)strlen(Path);
        if (l > 0 && Path[l - 1] == '\\')
            l--;
        ok = s - path == l && SalamanderGeneral->StrNICmp(path, Path, l) == 0;
    }

    if (ok) // matches -> remember the next top index
    {
        if (TopIndexesCount == TOP_INDEX_MEM_SIZE) // need to discard the first top index from memory
        {
            int i;
            for (i = 0; i < TOP_INDEX_MEM_SIZE - 1; i++)
                TopIndexes[i] = TopIndexes[i + 1];
            TopIndexesCount--;
        }
        strcpy(Path, path);
        TopIndexes[TopIndexesCount++] = topIndex;
    }
    else // does not match -> first top index in the sequence
    {
        strcpy(Path, path);
        TopIndexesCount = 1;
        TopIndexes[0] = topIndex;
    }
}

BOOL CTopIndexMem::FindAndPop(const char* path, int& topIndex)
{
    // determine whether the path corresponds to Path (path==Path)
    int l1 = (int)strlen(path);
    if (l1 > 0 && path[l1 - 1] == '\\')
        l1--;
    int l2 = (int)strlen(Path);
    if (l2 > 0 && Path[l2 - 1] == '\\')
        l2--;
    if (l1 == l2 && SalamanderGeneral->StrNICmp(path, Path, l1) == 0)
    {
        if (TopIndexesCount > 0)
        {
            char* s = Path + strlen(Path);
            if (s > Path && *(s - 1) == '\\')
                s--;
            if (s > Path && *s == '\\')
                s--;
            while (s > Path && *s != '\\')
                s--;
            *s = 0;
            topIndex = TopIndexes[--TopIndexesCount];
            return TRUE;
        }
        else // value not stored anymore (never saved or low memory -> was discarded)
        {
            Clear();
            return FALSE;
        }
    }
    else // query for a different path -> clear memory, a long jump occurred
    {
        Clear();
        return FALSE;
    }
}
