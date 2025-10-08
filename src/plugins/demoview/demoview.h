// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

// global data
extern const char* PluginNameEN; // untranslated plugin name, used before loading the language module and for debugging
extern HINSTANCE DLLInstance;    // handle to the SPL - language-independent resources
extern HINSTANCE HLanguage;      // handle to the SLG - language-dependent resources

// general Salamander interface - valid from startup until the plugin is unloaded
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// interface providing customized Windows controls used in Salamander
extern CSalamanderGUIAbstract* SalamanderGUI;

BOOL InitViewer();
void ReleaseViewer();

// global data
extern BOOL CfgSavePosition;               // whether to store the window position / align with the main window
extern WINDOWPLACEMENT CfgWindowPlacement; // invalid if CfgSavePosition != TRUE

extern DWORD LastCfgPage; // start page (sheet) in configuration dialog

// [0, 0] - for open viewer windows: the plugin configuration changed
#define WM_USER_VIEWERCFGCHNG WM_APP + 3346
// [0, 0] - for open viewer windows: the history needs to be trimmed
#define WM_USER_CLEARHISTORY WM_APP + 3347
// [0, 0] - for open viewer windows: Salamander regenerated fonts, call SetFont() on the lists
#define WM_USER_SETTINGCHANGE WM_APP + 3248

char* LoadStr(int resID);

// plugin menu commands
#define MENUCMD_VIEWBMPFROMCLIP 1

//
// ****************************************************************************
// CPluginInterface
//

class CPluginInterfaceForViewer : public CPluginInterfaceForViewerAbstract
{
public:
    virtual BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                 BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex);
    virtual BOOL WINAPI CanViewFile(const char* name) { return TRUE; }
};

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) { return 0; }
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id);
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) {}
};

class CPluginInterfaceForThumbLoader : public CPluginInterfaceForThumbLoaderAbstract
{
public:
    virtual BOOL WINAPI LoadThumbnail(const char* filename, int thumbWidth, int thumbHeight,
                                      CSalamanderThumbnailMakerAbstract* thumbMaker,
                                      BOOL fastThumbnail);
};

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI Configuration(HWND parent);

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) {}

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; }
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer();
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; }
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader();

    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

//
// ****************************************************************************
// CViewerWindow
//

#define BANDID_MENU 1
#define BANDID_TOOLBAR 2

enum CViewerWindowEnablerEnum
{
    vweAlwaysEnabled, // zero index is reserved
    vweCut,
    vwePaste,
    vweCount
};

class CViewerWindow;

class CRendererWindow : public CWindow
{
public:
    CViewerWindow* Viewer;

public:
    CRendererWindow();
    ~CRendererWindow();

    void OnContextMenu(const POINT* p);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CViewerWindow : public CWindow
{
public:
    HANDLE Lock;                      // 'lock' object or NULL (set to the signaled state once we close the file)
    char Name[MAX_PATH];              // file name or ""
    CRendererWindow Renderer;         // viewer inner window
    HIMAGELIST HGrayToolBarImageList; // toolbar and menu in the gray variant (computed from the colored one)
    HIMAGELIST HHotToolBarImageList;  // toolbar and menu in the colored variant

    DWORD Enablers[vweCount];

    HWND HRebar; // holds the MenuBar and ToolBar
    CGUIMenuPopupAbstract* MainMenu;
    CGUIMenuBarAbstract* MenuBar;
    CGUIToolBarAbstract* ToolBar;

    int EnumFilesSourceUID;    // source UID for enumerating files in the viewer
    int EnumFilesCurrentIndex; // index of the current viewer file within the source

public:
    CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex);

    HANDLE GetLock();

    // if 'setLock' is TRUE, set 'Lock' to the signaled state (needed after closing the file)
    void OpenFile(const char* name, BOOL setLock = TRUE);

    BOOL IsMenuBarMessage(CONST MSG* lpMsg);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL FillToolBar();

    BOOL InitializeGraphics();
    BOOL ReleaseGraphics();

    BOOL InsertMenuBand();
    BOOL InsertToolBarBand();

    void LayoutWindows();

    void UpdateEnablers();
};

extern CWindowQueue ViewerWindowQueue; // list of all viewer windows
extern CThreadQueue ThreadQueue;       // list of all window threads

// plugin interface provided to Salamander
extern CPluginInterface PluginInterface;

// open the configuration dialog; if it already exists, display a message and return
void OnConfiguration(HWND hParent);

// open the About window
void OnAbout(HWND hParent);
