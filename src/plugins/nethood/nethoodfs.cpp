// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#include "precomp.h"
#include "nethood.h"
#include "nethoodfs.h"
#include "cache.h"
#include "nethoodfs2.h"
#include "globals.h"

/**
	Cache of network.
*/
extern CNethoodCache g_oNethoodCache;

CNethoodPluginInterfaceForFS::CNethoodPluginInterfaceForFS()
{
    m_cActiveFS = 0;
}

CNethoodPluginInterfaceForFS::~CNethoodPluginInterfaceForFS()
{
    assert(m_cActiveFS == 0);
}

CPluginFSInterfaceAbstract* WINAPI
CNethoodPluginInterfaceForFS::OpenFS(
    __in const char* fsName,
    __in int fsNameIndex)
{
    ++m_cActiveFS;
    return new CNethoodFSInterface();
}

void WINAPI
CNethoodPluginInterfaceForFS::CloseFS(
    __in CPluginFSInterfaceAbstract* pluginFS)
{
    --m_cActiveFS;

    // Typecast to call the correct destructor.
    CNethoodFSInterface* fs = static_cast<CNethoodFSInterface*>(pluginFS);

    if (fs != NULL)
    {
        delete fs;
    }
}

void WINAPI
CNethoodPluginInterfaceForFS::ExecuteChangeDriveMenuItem(
    __in int panel)
{
    CALL_STACK_MESSAGE2("CNethoodPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", panel);

    BOOL changeRes;
    int failReason;

    changeRes = SalamanderGeneral->ChangePanelPathToPluginFS(
        panel,
        g_szAssignedFSName,
        TEXT(""),
        &failReason);
}

BOOL WINAPI
CNethoodPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(
    __in HWND parent,
    __in int panel,
    __in int x,
    __in int y,
    __in CPluginFSInterfaceAbstract* pluginFS,
    __in const char* pluginFSName,
    __in int pluginFSNameIndex,
    __in BOOL isDetachedFS,
    __out BOOL& refreshMenu,
    __out BOOL& closeMenu,
    __out int& postCmd,
    __out void*& postCmdParam)
{
    return FALSE;
}

void WINAPI
CNethoodPluginInterfaceForFS::ExecuteChangeDrivePostCommand(
    __in int panel,
    __in int postCmd,
    __in void* postCmdParam)
{
}

void WINAPI
CNethoodPluginInterfaceForFS::ExecuteOnFS(
    __in int panel,
    __in CPluginFSInterfaceAbstract* pluginFS,
    __in const char* pluginFSName,
    __in int pluginFSNameIndex,
    __in CFileData& file,
    __in int isDir)
{
    TCHAR szNewPath[MAX_PATH];
    CNethoodFSInterface* pFSInterface;
    CNethoodCache::Node node;

    pFSInterface = reinterpret_cast<CNethoodFSInterface*>(pluginFS);
    node = pFSInterface->GetNodeFromFileData(file);
    assert(node != NULL || isDir == 2);

    pFSInterface->GetCurrentPath(szNewPath);

    if (isDir == 2)
    {
        // It's the up-dir.

        TCHAR* pszCutDir = NULL;

        // Trim off the last path component...
        if (szNewPath[0] == TEXT('\\') && szNewPath[1] == TEXT('\\'))
        {
            // Is a UNC path - go back to root.
            //pFSInterface->GetRootPath(szNewPath);
            pFSInterface = NULL; // Pointer may be invalid after ChangePanelPathToXxx
            SalamanderGeneral->ChangePanelPathToPluginFS(
                panel, pluginFSName, TEXT(""), NULL, -1, szNewPath + 2);
        }
        else if (SalamanderGeneral->CutDirectory(szNewPath, &pszCutDir))
        {
            // ...and change the path.
            pFSInterface = NULL; // Pointer may be invalid after ChangePanelPathToXxx
            SalamanderGeneral->ChangePanelPathToPluginFS(
                panel, pluginFSName, szNewPath, NULL,
                -1, pszCutDir);
        }
    }
    else
    {
        // It's a subdirectory (isDir == 1) or a "file"
        // (ie. a server or a share) (isDir == 0).

        const CNethoodCacheNode& nodeData = g_oNethoodCache.GetItemData(node);

        if (nodeData.GetType() == CNethoodCacheNode::TypeShare ||
            nodeData.GetType() == CNethoodCacheNode::TypeTSCVolume)
        {
            // It's a share. Change path to Salamander.
            pFSInterface = NULL; // Pointer may be invalid after ChangePanelPathToXxx

            // SalamanderGeneral->SetUserWorkedOnPanelPath(panel);  // Petr Solin: I think this is not the reason to add current path to List Of Working Directories (Alt+F12)

            if (!SalamanderGeneral->ChangePanelPathToDisk(panel,
                                                          nodeData.GetName()))
            {
                // May be access denied or so...
                // Do nothing - stay on the current nethood path.
            }
        }
        else if (nodeData.GetType() == CNethoodCacheNode::TypeServer)
        {
            // It's a server.

            if (pFSInterface->IsRootPath(szNewPath))
            {
                pFSInterface = NULL; // Pointer may be invalid after ChangePanelPathToXxx
                SalamanderGeneral->ChangePanelPathToPluginFS(
                    panel, pluginFSName, nodeData.GetName());
            }
            else
            {
                // Combine the path...
                if (SalamanderGeneral->SalPathAppend(szNewPath, file.Name, MAX_PATH))
                {
                    pFSInterface = NULL; // Pointer may be invalid after ChangePanelPathToXxx
                    SalamanderGeneral->ChangePanelPathToPluginFS(
                        panel, pluginFSName, szNewPath);
                }
            }
        }
        else
        {
            // Combine the path...
            if (SalamanderGeneral->SalPathAppend(szNewPath, file.Name, MAX_PATH))
            {
                pFSInterface = NULL; // Pointer may be invalid after ChangePanelPathToXxx
                SalamanderGeneral->ChangePanelPathToPluginFS(
                    panel, pluginFSName, szNewPath);
            }
        }
    }
}

BOOL WINAPI
CNethoodPluginInterfaceForFS::DisconnectFS(
    __in HWND parent,
    __in BOOL isInPanel,
    __in int panel,
    __in CPluginFSInterfaceAbstract* pluginFS,
    __in const char* pluginFSName,
    __in int pluginFSNameIndex)
{
    BOOL ret = FALSE;

    CALL_STACK_MESSAGE5("CNethoodPluginInterfaceForFS::DisconnectFS(, %d, %d, , %s, %d)",
                        isInPanel, panel, pluginFSName, pluginFSNameIndex);

    //((CPluginFSInterface *)pluginFS)->CalledFromDisconnectDialog = TRUE; // potlacime zbytecne dotazy (user dal prikaz pro disconnect, jen ho provedeme)

    if (isInPanel)
    {
        SalamanderGeneral->DisconnectFSFromPanel(parent, panel);
        ret = SalamanderGeneral->GetPanelPluginFS(panel) != pluginFS;
    }
    else
    {
        ret = SalamanderGeneral->CloseDetachedFS(parent, pluginFS);
    }

    if (!ret)
    {
        //((CPluginFSInterface *)pluginFS)->CalledFromDisconnectDialog = FALSE; // vypneme potlaceni zbytecnych dotazu
    }

    return ret;
}

void WINAPI
CNethoodPluginInterfaceForFS::ConvertPathToInternal(
    __in const char* fsName,
    __in int fsNameIndex,
    __inout char* fsUserPart)
{
}

void WINAPI
CNethoodPluginInterfaceForFS::ConvertPathToExternal(
    __in const char* fsName,
    __in int fsNameIndex,
    __inout char* fsUserPart)
{
}

void WINAPI
CNethoodPluginInterfaceForFS::EnsureShareExistsOnServer(
    __in int iPanel,
    __in const char* server,
    __in const char* share)
{
    TCHAR szUncPath[MAX_PATH];
    UINT uError;

    assert(server != NULL);

    if (share != NULL)
    {
        StringCchPrintf(szUncPath, COUNTOF(szUncPath),
                        TEXT("\\\\%s\\%s"), server, share);
    }
    else
    {
        StringCchPrintf(szUncPath, COUNTOF(szUncPath),
                        TEXT("\\\\%s"), server);
    }

    uError = g_oNethoodCache.EnsurePathExists(szUncPath);
    assert(uError == NO_ERROR);

    g_iFocusSharePanel = iPanel;
    // Shift iPanel to zero-based value.
    iPanel -= PANEL_LEFT;
    assert(iPanel >= 0 && iPanel < COUNTOF(g_aszFocusShareName));
    StringCchCopy(g_aszFocusShareName[iPanel], COUNTOF(g_aszFocusShareName[iPanel]), share);
}
