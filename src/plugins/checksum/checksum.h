// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "wrappers.h"

#define SizeOf(x) (sizeof(x) / sizeof(x[0]))

typedef enum eHASH_TYPE
{
    HT_CRC,
    HT_MD5,
    HT_SHA1,
    HT_SHA256,
    HT_SHA512,
    HT_COUNT // Not a hash type but # of known hash types
} eHASH_TYPE;

typedef struct SHashInfo
{
    eHASH_TYPE Type; // A little redundant - same as the index in SConfig.HashInfo
    int bCalculate;
    int idColumnHeader; // Column header Calculate window
    int idContextMenu;  // Line in context menu in Calculate window
    int idSaveAsFilter; // Filter to SaveAs dialog in Calculate window
    int idVerifyTitle;  // Title of Verify window
    LPCTSTR sSaveAsExt;
    const char* sRegID; // Key in registry
    THashFactory Factory;
} SHashInfo;

typedef struct SConfig
{
    eHASH_TYPE HashType; // The default format for SaveAs
    int CalcDlgWidths[2 + 2 + HT_COUNT];
    int VerDlgWidths[5];
    SHashInfo HashInfo[HT_COUNT];
} SConfig;

extern CSalamanderGeneralAbstract* SalamanderGeneral;
extern CSalamanderGUIAbstract* SalamanderGUI;
extern CSalamanderCryptAbstract* SalamanderCrypt;

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI Configuration(HWND parent);

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) { return; };

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; };
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; };
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; };
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param) {}
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) { return 0; };
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id);
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) {}
};

extern SConfig Config;
extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy

char* LoadStr(int resID);
INT_PTR OnConfiguration(HWND hParent);

#define CMD_CALCULATE 1
#define CMD_VERIFY 2

// reseni focusnuti z dialogu Verify
#define CMD_FOCUSFILE 99
extern char Focus_Path[MAX_PATH];

#define DUMP_MEM_OBJECTS

#if defined(DUMP_MEM_OBJECTS) && defined(_DEBUG)
#define CRT_MEM_CHECKPOINT \
    _CrtMemState ___CrtMemState; \
    _CrtMemCheckpoint(&___CrtMemState);
#define CRT_MEM_DUMP_ALL_OBJECTS_SINCE _CrtMemDumpAllObjectsSince(&___CrtMemState);

#else //DUMP_MEM_OBJECTS
#define CRT_MEM_CHECKPOINT ;
#define CRT_MEM_DUMP_ALL_OBJECTS_SINCE ;

#endif //DUMP_MEM_OBJECTS
