// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// the download thread sends this message to the plugin main window when it closes
// if the data were loaded correctly, wParam == TRUE; otherwise it is FALSE
#define WM_USER_DOWNLOADTHREAD_EXIT WM_APP + 666

// the user pressed one of the buttons for scrolling through the list
#define WM_USER_KEYDOWN WM_APP + 667

// configuration data
enum CAutoCheckModeEnum // how often the plugin window is activated
{
    achmNever,
    achmDay,
    achmWeek,
    achmMonth,
    achm3Month,
    achm6Month,
    achmCount // terminator
};

enum CInternetConnection // how the user is connected to the internet
{
    inetPhone,
    inetLAN,
    inetNone,
    inetCount // terminator
};

enum CInternetProtocol // which protocol should be used
{
    inetpHTTP,
    inetpFTP,
    inetpFTPPassive,
    inetpCount // terminator
};

struct CDataDefaults
{
    CAutoCheckModeEnum AutoCheckMode;
    BOOL AutoConnect;
    BOOL AutoClose;
    BOOL CheckBetaVersion;
    BOOL CheckPBVersion;
    BOOL CheckReleaseVersion;
};

struct CTVData
{
    BOOL Success;
    HANDLE Continue;
    BOOL AutoOpen;
    BOOL AlwaysOnTop;
    BOOL FirstLoadAfterInstall;
};

extern CDataDefaults Data;
extern CDataDefaults DataDefaults[inetCount];
extern CInternetConnection InternetConnection; // how is the user connected to the internet?
extern CInternetProtocol InternetProtocol;     // how is the user connected to the internet?

extern HINSTANCE DLLInstance; // handle to the SPL - language independent resources
extern HINSTANCE HLanguage;   // handle to the SLG - language dependent resources

extern HWND HMainDialog; // handle of the main dialog (NULL if it is closed)

extern HWND HConfigurationDialog;
extern BOOL ConfigurationChanged; // TRUE = the user clicked OK in the configuration dialog (we do not check whether anything actually changed)

extern BOOL PluginIsReleased; // are we inside CPluginInterface::Release?

extern BOOL LoadedOnSalamanderStart; // was the plugin loaded with the LOADINFO_LOADONSTART flag
extern BOOL LoadedOnSalInstall;      // was the plugin loaded right after Salamander installation?

extern HANDLE HDownloadThread; // used to verify that the thread has already finished

extern SYSTEMTIME LastCheckTime;       // when the check was last performed (zeroed out if no check was executed yet)
extern SYSTEMTIME NextOpenOrCheckTime; // the earliest time the plugin window should open automatically and optionally perform a check (zeroed out if it should happen at the first load-on-start (ASAP))
extern int ErrorsSinceLastCheck;       // how many times we have already failed to perform the automatic check

extern char SalamanderTextVersion[MAX_PATH]; // running Salamander version text (for example, "2.52 beta 3 (PB 32)")

extern DWORD MainDialogID;                   // unique dialog counter
extern CRITICAL_SECTION MainDialogIDSection; // and its lock
DWORD GetMainDialogID();                     // and its handling
void IncMainDialogID();

extern HANDLE HModulesEnumDone; // synchronization of Salamander's main thread and the main dialog thread

#define LOADED_SCRIPT_MAX 100000             // hopefully we will never end up with a script that large :-))
extern BYTE LoadedScript[LOADED_SCRIPT_MAX]; // script is poured in here - either from the internet or from a file in the debug build
extern DWORD LoadedScriptSize;               // number of occupied (valid) bytes

// Salamander's general interface - valid from startup until the plugin is terminated
extern CSalamanderGeneralAbstract* SalGeneral;

char* LoadStr(int resID);

BOOL AddUniqueFilter(const char* itemName);
void FiltersFillListBox(HWND hListBox);
void FiltersLoadFromListBox(HWND hListBox);
void DestroyFilters(); // tear down the filter array
void LoadConfig(HKEY regKey, CSalamanderRegistryAbstract* registry);
void SaveConfig(HKEY regKey, CSalamanderRegistryAbstract* registry);
void OnSaveTimeStamp(HKEY regKey, CSalamanderRegistryAbstract* registry);

// dialog window procs
INT_PTR CALLBACK CfgDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK MainDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OnConfiguration(HWND hParent); // open the configuration window

BOOL RegisterLogClass();
void UnregisterLogClass();
BOOL InitializeLogWindow(HWND hWindow);
void ReleaseLogWindow(HWND hWindow);
void AddLogLine(const char* line, BOOL scrollToEnd);
void ClearLogWindow();

void MainEnableControls(BOOL downloading);

void ShowMinNA_IfNotShownYet(HWND hWindow, BOOL flashIfNotShownYet, BOOL restoreWindow);

BOOL OpenInternetDialog(HWND hParent, CInternetConnection* internetConnection, CInternetProtocol* internetProtocol);

DWORD GetWaitDays();
BOOL IsTimeExpired(const SYSTEMTIME* time);
void GetFutureTime(SYSTEMTIME* tgtTime, const SYSTEMTIME* time, DWORD days);
void GetFutureTime(SYSTEMTIME* tgtTime, DWORD days);

// script loading
BOOL LoadScripDataFromFile(const char* fileName);
HANDLE StartDownloadThread(BOOL firstLoadAfterInstall); // returns the thread handle or NULL

// log output
void ModulesCreateLog(BOOL* moduleWasFound, BOOL rereadModules);

// returns TRUE only if some modules are loaded (from the server or from a file)
BOOL ModulesHasCorrectData();

// cleanup of Modules
void ModulesCleanup();

void ModulesChangeShowDetails(int index);

void EnumSalModules();

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask);
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id);
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) {}
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

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData);

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver();
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer();
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS();
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader();

    virtual void WINAPI Event(int event, DWORD param) {}
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

extern CPluginInterface PluginInterface;
