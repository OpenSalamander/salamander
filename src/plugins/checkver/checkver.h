// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// tuto zpravu posti download thread hlavnimu oknu pluginu pro svem zavreni
// pokud se podarilo data nacist korektne, bude wParam == TRUE; jinak bude FALSE
#define WM_USER_DOWNLOADTHREAD_EXIT WM_APP + 666

// user stisknul jedno z tlacitek pro rolovani seznamem
#define WM_USER_KEYDOWN WM_APP + 667

// data konfigurace
enum CAutoCheckModeEnum // jak casto se aktivuje okno pluginu
{
    achmNever,
    achmDay,
    achmWeek,
    achmMonth,
    achm3Month,
    achm6Month,
    achmCount // terminator
};

enum CInternetConnection // jak je user pripojenej na net
{
    inetPhone,
    inetLAN,
    inetNone,
    inetCount // terminator
};

enum CInternetProtocol // jaky protokol se ma pouzit
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
extern CInternetConnection InternetConnection; // jak je user pripojenej na net?
extern CInternetProtocol InternetProtocol;     // jak je user pripojenej na net?

extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy

extern HWND HMainDialog; // handle hlavniho dialog (NULL, pokud je zavreny)

extern HWND HConfigurationDialog;
extern BOOL ConfigurationChanged; // TRUE = user dal OK v konfiguracnim dialogu (jestli neco skutecne zmenil uz nepitvame)

extern BOOL PluginIsReleased; // jsme v metode CPluginInterface::Release?

extern BOOL LoadedOnSalamanderStart; // byl plugin nacten s flagem LOADINFO_LOADONSTART
extern BOOL LoadedOnSalInstall;      // byl plugin nalouden tesne po instalaci Salamandera?

extern HANDLE HDownloadThread; // slouzi pro kontrolu, ze thread uz dobehnul

extern SYSTEMTIME LastCheckTime;       // kdy byla naposledy provedena kontrola (nulovane pokud jsme kontrolu jeste nedelali)
extern SYSTEMTIME NextOpenOrCheckTime; // kdy nejdrive se ma automaticky otevrit okno pluginu a prip. provest kontrola (nulovane pokud se ma udelat pri prvnim load-on-startu (ASAP))
extern int ErrorsSinceLastCheck;       // kolikrat jsme se uz neuspesne pokouseli automaticky provest kontrolu

extern char SalamanderTextVersion[MAX_PATH]; // text cisla verze beziciho Salamandera (napr. "2.52 beta 3 (PB 32)")

extern DWORD MainDialogID;                   // unikatni counter dialogu
extern CRITICAL_SECTION MainDialogIDSection; // a jeho zamek
DWORD GetMainDialogID();                     // a jeho obsluha
void IncMainDialogID();

extern HANDLE HModulesEnumDone; // synchronizace hlavniho threadu salamandera a threadu hlavniho dialogu

#define LOADED_SCRIPT_MAX 100000             // doufam, ze k takovehlemu scriptu se nikdy nedopracujeme :-))
extern BYTE LoadedScript[LOADED_SCRIPT_MAX]; // sem nalejeme script - at pres inet nebo ze souboru v debug verzi
extern DWORD LoadedScriptSize;               // pocet obsazenych (validnich) bajtu

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalGeneral;

char* LoadStr(int resID);

BOOL AddUniqueFilter(const char* itemName);
void FiltersFillListBox(HWND hListBox);
void FiltersLoadFromListBox(HWND hListBox);
void DestroyFilters(); // sestreli pole fitru
void LoadConfig(HKEY regKey, CSalamanderRegistryAbstract* registry);
void SaveConfig(HKEY regKey, CSalamanderRegistryAbstract* registry);
void OnSaveTimeStamp(HKEY regKey, CSalamanderRegistryAbstract* registry);

// dialog window procs
INT_PTR CALLBACK CfgDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK MainDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OnConfiguration(HWND hParent); // oteve konfiguracni okno

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

// nacteni scriptu
BOOL LoadScripDataFromFile(const char* fileName);
HANDLE StartDownloadThread(BOOL firstLoadAfterInstall); // vraci handle threadu nebo NULL

// vypsani logu
void ModulesCreateLog(BOOL* moduleWasFound, BOOL rereadModules);

// vraci TRUE jen pokud mame nactene nejake moduly (ze serveru nebo ze souboru)
BOOL ModulesHasCorrectData();

// vycisteni Modules
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
