// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// definice IDs do menu
#define MID_COMPAREFILES 1

#define CURRENT_CONFIG_VERSION_PRESEPARATEOPTIONS 6
#define CURRENT_CONFIG_VERSION_NORECOMPAREBUTTON 7
#define CURRENT_CONFIG_VERSION 8

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
class CPluginInterface;
extern CPluginInterface PluginInterface;
extern BOOL AlwaysOnTop;

extern BOOL LoadOnStart;

// ****************************************************************************
//
// Interface pluginu
//

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

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; };
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; }
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }
    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}
    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

class CPluginInterfaceForMenu : public CPluginInterfaceForMenuExtAbstract
{
public:
    // vraci stav polozky menu s identifikacnim cislem 'id'; navratova hodnota je kombinaci
    // flagu (viz MENU_ITEM_STATE_XXX); 'eventMask' viz CSalamanderConnectAbstract::AddMenuItem
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) { return 0; }

    // spousti prikaz menu s identifikacnim cislem 'id', 'eventMask' viz
    // CSalamanderConnectAbstract::AddMenuItem, 'salamander' je sada pouzitelnych metod
    // Salamandera pro provadeni operaci, 'parent' je parent messageboxu, vraci TRUE pokud
    // ma byt v panelu zruseno oznaceni (nebyl pouzit Cancel, mohl byt pouzit Skip), jinak
    // vraci FALSE (neprovede se odznaceni);
    // POZOR: Pokud prikaz zpusobi zmeny na nejake ceste (diskove/FS), mel by pouzit
    //        CSalamanderGeneralAbstract::PostChangeOnPathNotification pro informovani
    //        panelu bez automatickeho refreshe a otevrene FS (aktivni i odpojene)
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id);
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) {}
};

// ****************************************************************************
//
// CFileCompThread
//

class CFilecompThread : public CThread
{
public:
    char Path1[MAX_PATH];
    char Path2[MAX_PATH];
    BOOL DontConfirmSelection;
    char ReleaseEvent[20];

    CFilecompThread(const char* file1, const char* file2, BOOL dontConfirmSelection,
                    const char* releaseEvent) : CThread("Filecomp Thread")
    {
        strcpy(Path1, file1);
        strcpy(Path2, file2);
        DontConfirmSelection = dontConfirmSelection;
        strcpy(ReleaseEvent, releaseEvent);
    }

    virtual unsigned Body();
};

extern CWindowQueue MainWindowQueue; // seznam vsech oken filecompu
extern CThreadQueue ThreadQueue;     // seznam vsech oken+workeru filecompu + remote control
