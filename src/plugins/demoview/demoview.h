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

// globalni data
extern const char* PluginNameEN; // neprekladane jmeno pluginu, pouziti pred loadem jazykoveho modulu + pro debug veci
extern HINSTANCE DLLInstance;    // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;      // handle k SLG-cku - jazykove zavisle resourcy

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
extern CSalamanderGUIAbstract* SalamanderGUI;

BOOL InitViewer();
void ReleaseViewer();

// globalni data
extern BOOL CfgSavePosition;               // ukladat pozici okna/umistit dle hlavniho okna
extern WINDOWPLACEMENT CfgWindowPlacement; // neplatne, pokud CfgSavePosition != TRUE

extern DWORD LastCfgPage; // start page (sheet) in configuration dialog

// [0, 0] - pro otevrena okna viewru: konfigurace pluginu se zmenila
#define WM_USER_VIEWERCFGCHNG WM_APP + 3346
// [0, 0] - pro otevrena okna viewru: je treba podriznou historie
#define WM_USER_CLEARHISTORY WM_APP + 3347
// [0, 0] - pro otevrena okna vieweru: Salamander pregeneroval fonty, mame zavolat SetFont() listam
#define WM_USER_SETTINGCHANGE WM_APP + 3248

char* LoadStr(int resID);

// prikazy pluginoveho menu
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
    HANDLE Lock;                      // 'lock' objekt nebo NULL (do signaled stavu az zavreme soubor)
    char Name[MAX_PATH];              // jmeno souboru nebo ""
    CRendererWindow Renderer;         // vnitrni okno vieweru
    HIMAGELIST HGrayToolBarImageList; // toolbar a menu v sedivem provedeni (pocitano z barevneho)
    HIMAGELIST HHotToolBarImageList;  // toolbar a menu v barevnem provedeni

    DWORD Enablers[vweCount];

    HWND HRebar; // drzi MenuBar a ToolBar
    CGUIMenuPopupAbstract* MainMenu;
    CGUIMenuBarAbstract* MenuBar;
    CGUIToolBarAbstract* ToolBar;

    int EnumFilesSourceUID;    // UID zdroje pro enumeraci souboru ve vieweru
    int EnumFilesCurrentIndex; // index aktualniho souboru ve vieweru ve zdroji

public:
    CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex);

    HANDLE GetLock();

    // je-li 'setLock' TRUE, dojde k nastaveni 'Lock' do signaled stavu (je
    // potreba po zavreni souboru)
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

extern CWindowQueue ViewerWindowQueue; // seznam vsech oken viewru
extern CThreadQueue ThreadQueue;       // seznam vsech threadu oken

// rozhrani pluginu poskytnute Salamanderovi
extern CPluginInterface PluginInterface;

// otevre konfiguracni dialog; pokud jiz existuje, zobrazi hlasku a vrati se
void OnConfiguration(HWND hParent);

// otevre About okno
void OnAbout(HWND hParent);
