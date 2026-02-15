// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define SizeOf(x) (sizeof(x) / sizeof(x[0]))

void GetDefaultLogFont(LOGFONT* lf);

// Configuration variables

struct CCSVConfig
{
    // 0: Auto-Select
    // 1: '\t'
    // 2: ';'
    // 3: ','
    // 4: ' '
    // 5: ValueSeparatorChar
    int ValueSeparator;
    char ValueSeparatorChar;

    // 0: Auto-Select
    // 1: '\"'
    // 2: '\''
    // 3: None
    int TextQualifier;

    // 0: Auto-Select
    // 1: As Column Name
    // 2: As Data
    int FirstRowAsName;
};

extern BOOL CfgAutoSelect;
extern char CfgDefaultCoding[210];
extern CCSVConfig CfgDefaultCSV;

void OnConfiguration(HWND hParent, BOOL bFromSalamander);

extern MENU_TEMPLATE_ITEM PopupMenuTemplate[];

extern CSalamanderGUIAbstract* SalamanderGUI;

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

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI Configuration(HWND parent);

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) { return; }

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; }
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer();
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() { return NULL; }
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; }
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

//
// ****************************************************************************
// CViewerWindow
//

enum CViewerWindowEnablerEnum
{
    vweAlwaysEnabled, // zero index is reserved
    vweDBOpened,
    vweCSVOpened,
    vweMoreBookmarks,
    vweUncertainEncoding, // Encoding can be changed if the file is not Unicode
    vwePrevFile,
    vweNextFile,
    vwePrevSelFile,
    vweNextSelFile,
    vweFirstFile,
    vweLastFile,
    vweCount
};

class CViewerWindow : public CWindow
{
public:
    HANDLE Lock; // 'lock' object or NULL (goes to the signaled state once the file is closed)
    CRendererWindow Renderer;
    int ConversionsCount; // number of conversions retrieved including "Don't convert"; without separators
    CCSVConfig CfgCSV;
    CFindDialog FindDialog;

    HWND HRebar; // holds the MenuBar and ToolBar
    CGUIMenuPopupAbstract* MainMenu;
    CGUIMenuBarAbstract* MenuBar;
    CGUIToolBarAbstract* ToolBar;

    HIMAGELIST HGrayToolBarImageList; // toolbar and menu in a gray variant (derived from the colored one)
    HIMAGELIST HHotToolBarImageList;  // toolbar and menu in a colored variant

    DWORD Enablers[vweCount];
    BOOL IsSrcFileSelected; // valid only if Enablers[vweSelSrcFile]==TRUE: TRUE/FALSE source file is selected/unselected

public:
    CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex);

    HANDLE GetLock();
    BOOL IsMenuBarMessage(CONST MSG* lpMsg);

    void UpdateRowNumberOnToolBar(int cur, int tot);
    void UpdateEnablers();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL InitializeGraphics();
    BOOL ReleaseGraphics();

    BOOL InitCodingSubmenu();
    BOOL GetCodingMenuIndex(const char* coding);
    BOOL GetNextCodingMenuIndex(const char* coding, BOOL next);
    void OnFind(WORD command);

    BOOL InsertMenuBand();
    BOOL InsertToolBarBand();
    BOOL FillToolBar();
    void LayoutWindows();
};
