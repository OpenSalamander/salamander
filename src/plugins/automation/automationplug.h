// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	automationplug.h
	Automation plugin main object and menu extension.
*/

#pragma once

/// Defines indices of the icons used throughout the plugin.
enum PluginIcons
{
    PluginIconMain,   ///< Plugin main icon.
    PluginIconScript, ///< Icon of the script.
    PluginIconRun,    ///< Play icon.
    PluginIconStop,   ///< Stop icon.
};

/// Automation menu extension.
class CAutomationMenuExtInterface : public CPluginInterfaceForMenuExtAbstract
{
protected:
    /// True if the script popup menu should be opened from next BuildMenu.
    bool m_bDeferredPopup;

    int ExecuteScriptMenu();

    void AddScriptContainerToPopup(
        const class CScriptContainer* pContainer,
        CGUIMenuPopupAbstract* pMenu,
        int nLevel);

    void AddScriptContainerToMenu(
        const CScriptContainer* pContainer,
        CSalamanderBuildMenuAbstract* pMenuBuilder,
        int nLevel);

    bool CanExecuteFocusedItem();

public:
    /// IDs assigned to the automation menu items.
    enum MenuCommandId
    {
        /// Run focused script command.
        CmdRunFocusedScript = 1,

        /// Popup script menu.
        CmdScriptPopupMenu = 2,

        /// Actually open script menu.
        CmdOpenPopupMenu = 3,

        /// First script from the repository.
        CmdRunLoadedScriptBase = 1000,
    };

    CAutomationMenuExtInterface();

    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask);

    virtual BOOL WINAPI ExecuteMenuItem(
        CSalamanderForOperationsAbstract* salamander,
        HWND parent,
        int id, DWORD eventMask);

    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id)
    {
        return FALSE;
    }

    virtual void WINAPI BuildMenu(
        HWND parent,
        CSalamanderBuildMenuAbstract* salamander);
};

/// Automation plugin.
class CAutomationPluginInterface : public CPluginInterfaceAbstract
{
protected:
    CGUIIconListAbstract* m_pIcons;

    /// Handle to the image list containing the hot icons.
    HIMAGELIST m_himlHot;

    /// Handle to the image list containing the cold (gray) icons.
    HIMAGELIST m_himlCold;

    /// Determines whether JIT debugging of scripts is enabled.
    bool m_bEnableDebugger;

    struct DIRECTORY_INFO
    {
        TCHAR szDirectory[MAX_PATH];

        void Set(PCTSTR pszDirectory)
        {
            StringCchCopy(szDirectory, _countof(szDirectory), pszDirectory);
        }
    };

    TDirectArray<DIRECTORY_INFO> m_aDirectories;

    /// Salamander directory.
    TCHAR m_szSalDir[MAX_PATH];

    void AddScriptContainerToMenu(
        const class CScriptContainer* pContainer,
        CSalamanderConnectAbstract* pConnect,
        int nLevel);

    /// Callback for $(SalDir) expansion.
    static PCTSTR CALLBACK ExpandSalDir(HWND hwndParent, void* pContext);

public:
    /// Constructor.
    CAutomationPluginInterface();

    /// Destructor.
    ~CAutomationPluginInterface();

    /// Returns plugin image list.
    HIMAGELIST GetImageList(bool bHot)
    {
        return bHot ? m_himlHot : m_himlCold;
    }

    /// Returns plugins icons as icon list object.
    CGUIIconListAbstract* GetIconList()
    {
        return m_pIcons;
    }

    /// Determines whether JIT debugging of scripts is enabled.
    bool IsDebuggerEnabled() const
    {
        return m_bEnableDebugger;
    }

    /// Enables JIT debugging of scripts.
    void EnableDebugger(bool bEnable)
    {
        m_bEnableDebugger = bEnable;
    }

    /// Returns number of script directories.
    int GetScriptDirectoryCount() const
    {
        return m_aDirectories.Count;
    }

    /// Returns script directory, does not expand environment variables.
    PCTSTR GetScriptDirectoryRaw(__in int iDir)
    {
        _ASSERTE(iDir < m_aDirectories.Count);
        return m_aDirectories.At(iDir).szDirectory;
    }

    /// Deletes all script directories from the list.
    void RemoveAllScriptDirectories()
    {
        m_aDirectories.DestroyMembers();
    }

    /// Adds directory to the script directory list.
    int AddScriptDirectory(PCTSTR pszDirectory)
    {
        DIRECTORY_INFO dir;

        dir.Set(pszDirectory);
        m_aDirectories.Add(dir);

        return m_aDirectories.Count - 1;
    }

    /// Expands variables contained in the provided file path.
    bool ExpandPath(
        __in PCTSTR pszPath,
        __out_ecount(cchMax) PTSTR pszExpanded,
        __in int cchMax);

    // CPluginInterfaceAbstract

    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);

    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);

    virtual void WINAPI Configuration(HWND parent);

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) {}

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver()
    {
        return NULL;
    }

    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer()
    {
        return NULL;
    }

    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt()
    {
        extern CAutomationMenuExtInterface g_oMenuExtInterface;
        return &g_oMenuExtInterface;
    }

    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS()
    {
        return NULL;
    }

    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader()
    {
        return NULL;
    }

    virtual void WINAPI Event(int event, DWORD param);

    virtual void WINAPI ClearHistory(HWND parent) {}

    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};
