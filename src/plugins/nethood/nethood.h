// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#pragma once

class CNethoodPluginInterface : public CPluginInterfaceAbstract
{
protected:
    bool m_bPreload;

public:
    CNethoodPluginInterface()
    {
        m_bPreload = false;
    }

    void SetPreloadFlag(bool bPreload);

    bool GetPreloadFlag() const
    {
        return m_bPreload;
    }

    virtual void WINAPI About(
        __in HWND parent);

    virtual BOOL WINAPI Release(
        __in HWND parent,
        __in BOOL force);

    virtual void WINAPI LoadConfiguration(
        __in HWND parent,
        __in HKEY regKey,
        __in CSalamanderRegistryAbstract* registry);

    virtual void WINAPI SaveConfiguration(
        __in HWND parent,
        __in HKEY regKey,
        __in CSalamanderRegistryAbstract* registry);

    virtual void WINAPI Configuration(
        __in HWND parent);

    virtual void WINAPI Connect(
        __in HWND parent,
        __in CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(
        __in CPluginDataInterfaceAbstract* pluginData);

    virtual CPluginInterfaceForArchiverAbstract* WINAPI
    GetInterfaceForArchiver();

    virtual CPluginInterfaceForViewerAbstract* WINAPI
    GetInterfaceForViewer();

    virtual CPluginInterfaceForMenuExtAbstract* WINAPI
    GetInterfaceForMenuExt();

    virtual CPluginInterfaceForFSAbstract* WINAPI
    GetInterfaceForFS();

    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI
    GetInterfaceForThumbLoader();

    virtual void WINAPI Event(
        __in int event,
        __in DWORD param);

    virtual void WINAPI ClearHistory(
        __in HWND parent);

    virtual void WINAPI AcceptChangeOnPathNotification(
        __in const char* path,
        __in BOOL includingSubdirs);

    virtual void WINAPI PasswordManagerEvent(
        __in HWND parent,
        __in int event);
};
