// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#pragma once

class CNethoodPluginInterfaceForFS : public CPluginInterfaceForFSAbstract
{
private:
    /// Count of opened file systems.
    int m_cActiveFS;

public:
    /// Contructor.
    CNethoodPluginInterfaceForFS();

    /// Destructor.
    ~CNethoodPluginInterfaceForFS();

    virtual CPluginFSInterfaceAbstract* WINAPI OpenFS(
        __in const char* fsName,
        __in int fsNameIndex);

    virtual void WINAPI CloseFS(
        __in CPluginFSInterfaceAbstract* pluginFS);

    virtual void WINAPI ExecuteOnFS(
        __in int panel,
        __in CPluginFSInterfaceAbstract* pluginFS,
        __in const char* pluginFSName,
        __in int pluginFSNameIndex,
        __in CFileData& file,
        __in int isDir);

    virtual BOOL WINAPI DisconnectFS(
        __in HWND parent,
        __in BOOL isInPanel,
        __in int panel,
        __in CPluginFSInterfaceAbstract* pluginFS,
        __in const char* pluginFSName,
        __in int pluginFSNameIndex);

    virtual void WINAPI ConvertPathToInternal(
        __in const char* fsName,
        __in int fsNameIndex,
        __inout char* fsUserPart);

    virtual void WINAPI ConvertPathToExternal(
        __in const char* fsName,
        __in int fsNameIndex,
        __inout char* fsUserPart);

    virtual void WINAPI EnsureShareExistsOnServer(
        __in int panel,
        __in const char* server,
        __in const char* share);

    virtual void WINAPI ExecuteChangeDriveMenuItem(
        __in int panel);

    virtual BOOL WINAPI ChangeDriveMenuItemContextMenu(
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
        __out void*& postCmdParam);

    virtual void WINAPI ExecuteChangeDrivePostCommand(
        __in int panel,
        __in int postCmd,
        __in void* postCmdParam);
};
