// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

BOOL OpenInfFile();
BOOL DoInstallation();
BOOL DoUninstall(HWND hParent, BOOL* needRestart);
BOOL DeleteAutoImportConfigMarker(const char* autoImportConfig);
BOOL FindConflictWithAnotherVersion(BOOL* sameOrOlderVersion, BOOL* sameVersion, /*BOOL *sameOrOlderVersionIgnoringBuild, BOOL *sameVersionIgnoringBuild, */ BOOL* foundRemoveLog);
BOOL RefreshDesktop(BOOL sleep);
BOOL GetRegistryDWORDValue(const char* line, DWORD* retVal);
BOOL FindOutIfThisIsUpgrade(HWND hDlg);
void ReadPreviousVerOfFileToIncrementContent();
WORD GetCurDispLangID();

//
// commands
//
#define ID_INSTALL 100
#define ID_STOPINSTALL 101
#define ID_WIZARDDONE 102

//
// typedefs
//
typedef struct tagINSTALLINFO
{
    char ApplicationName[MAX_PATH];             // vytahneme z INF souboru
    char ApplicationNameVer[MAX_PATH];          // vcetne verze
    char DefaultDirectory[MAX_PATH];            // vytahneme z INF souboru
                                                //  char UseLastDirectory[MAX_PATH];   // vytahneme z INF souboru
    char LastDirectories[3000];                 // vytahneme z INF souboru
    char License[100000];                       // vytahnem z LICENSE.TXT
    char ViewReadmePath[MAX_PATH];              // vytahneme z INF souboru
    char CheckRunningApps[MAX_PATH];            // vytahneme z INF souboru
    char RunProgramPath[MAX_PATH];              // vytahneme z INF souboru
    char RunProgramQuietPath[MAX_PATH];         // vytahneme z INF souboru
    char IncrementFileContentSrc[5000];         // vytahneme z INF souboru
    char IncrementFileContentDst[5000];         // vytahneme z INF souboru
    char EnsureSalamander25Dir[5000];           // vytahneme z INF souboru
    char UnistallRunProgramQuietPath[MAX_PATH]; // vytahneme z INF souboru
    char SaveRemoveLog[MAX_PATH];               // vytahneme z INF souboru
    char LicenseFilePath[MAX_PATH];             // vytahneme z INF souboru
    char FirstReadme[100000];                   // vytahnem z FirstReadmeFilePath
    char FirstReadmeFilePath[MAX_PATH];         // vytahneme z INF souboru
    BOOL UseFirstReadme;
    char AutoImportConfig[MAX_PATH]; // vytahneme z INF souboru
    char SLGLanguages[5000];         // vytahneme z INF souboru
    char WERLocalDumps[5000];        // vytahneme z INF souboru

    char RegPathVal['Z' - 'A' + 1][MAX_PATH];

    BOOL UseLicenseFile; // existuje license soubor - mame instalak hnat pres tohle okno?

    char CreateDirectory[MAX_PATH]; // user potvrdil vytvoreni tohoto adresare

    BOOL UninstallExistingVersion; // mame odinstaloval existuji verzi aplikace?
    BOOL TheExistingVersionIsSame; // je prepisovana verze stejna jako ta co instalujeme?

    char CopySection[400000];
    char CopyFrom[200000];
    char CopyTo[200000];
    BYTE CopyFlags[10000];
    int CopyCount;

    // doslo mi, ze celkovou velikost muzeme napocitat az u uzivatele z vybalenych souboreu
    // a zjednodusit tak pripravu setup.inf
    //  DWORD SpaceRequired;

    BOOL RebootNeeded; // po ukonceni instalace bude doporucen reboot

    char ShortcutSection[100000];
    BOOL DesktopPresent;
    BOOL StartMenuPresent;
    BOOL Silent; // TRUE=ticha instalace; FALSE=klasika s okenkama

    char CreateDirsSection[100000];
    int CreateDirsCount;

    char TmpSection[500000]; // pro registry

    char InfFile[100000]; // protoze salati neumi cist inf soubor

    // uninstall
    char UnpinFromTaskbar[100000];
    char RemoveFiles[400000];
    char RemoveDirs[100000];
    char RemoveRegValues[100000];
    char RemoveRegKeys[100000];
    char RemoveShellExts[100000];

    BOOL ShortcutInDesktop;
    BOOL ShortcutInStartMenu;
    //BOOL PinToTaskbar;
    BOOL CommonFolders;

    BOOL ViewReadme;
    BOOL RunProgram;
    BOOL RunProgramQuiet;

    BOOL SkipChooseDir;

    BOOL LoadOldRemoveLog;
    char OldRemoveOptions[100000];

    int DisplayWelcomeWarning; // 0 - zadny warning

} INSTALLINFO;

//
// globals
//
extern HINSTANCE HInstance;   // current instance
extern INSTALLINFO SetupInfo; // a structure containing the review information
extern HWND HWizardDialog;
extern HWND HProgressDialog;
extern char InfFileName[MAX_PATH];
extern char ModulePath[MAX_PATH];
extern char WindowsDirectory[MAX_PATH];
extern char SystemDirectory[MAX_PATH];
extern char ProgramFilesDirectory[MAX_PATH];

extern char DesktopDirectory[MAX_PATH];
extern char StartMenuDirectory[MAX_PATH];
extern char StartMenuProgramDirectory[MAX_PATH];
extern char QuickLaunchDirectory[MAX_PATH];

extern BOOL SfxDirectoriesValid;
extern char SfxDirectories[7][MAX_PATH];

extern HWND SfxDlg;   // okno SFX7ZIP, ktere spustilo tento setup.exe (pred ukoncenim setup.exe toto okno nechame ukazat a aktivovat, aby se z nej mohly spoustet instalovane readme a Salam)
extern BOOL RunBySfx; // TRUE = setup.exe byl spusten s parametrem /runbysfx

extern char MAINWINDOW_TITLE[100];

extern const char* INF_PRIVATE_SECTION;
extern const char* INF_VIEWREADME;
extern const char* INF_RUNPROGRAM;
extern const char* INF_RUNPROGRAMQUIET;
extern const char* INF_UNINSTALLRUNPROGRAMQUIET;
extern const char* INF_SAVEREMOVELOG;
extern const char* INF_SKIPCHOOSEDIR;

extern DWORD CCMajorVer;
extern DWORD CCMinorVer;
extern DWORD CCMajorVerNeed;
extern DWORD CCMinorVerNeed;

// options z command line
extern char CmdLineDestination[MAX_PATH]; // pokud je retezec nenulovy, urcuje cilovy adresar (jinak se pouzije dafault)

extern char PreviousVerOfFileToIncrementContent[10000]; // obsah prave odinstalovaneho plugins.ver

//
//functions
//
BOOL CreateWizard();
BOOL StopInstalling();
void InstallDone();
void CenterWindow(HWND hWindow); // centruje k hlavnimu oknu

char* LoadStr(int resID);                        // taha string z resourcu
void InsertProgramName(char* str, BOOL version); // do str nacpe jmeno instalovaneho programu
void InsertAppName(HWND hDlg, int resID, BOOL version);
BOOL QueryFreeSpace(char* driveSpec, LONGLONG* spaceRequired);
BOOL GetSpecialFolderPath(int folder, char* path);
void GetRootPath(char* root, const char* path);
char* strrchr(const char* string, int c);
int StrICmp(const char* s1, const char* s2);
void ExpandPath(char* path);

void ExtractCopySection();
void ExtractShortcutSection();
void ExtractCreateDirsSection();
BOOL DoGetRegistryVarSection();

BOOL GetRegString(HKEY hRootKey, LPSTR pszKey, LPSTR pszValue, LPSTR pszData);

void SetProgressMax(int max);
void SetProgressPos(int pos);
void SetFromTo(const char* from, const char* to);

BOOL LoadLastDirectory();

INT_PTR CALLBACK CommonControlsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL RemoveAllSubKeys(HKEY hKey);
HKEY GetRootHandle(const char* root);

BOOL FileExist(const char* fileName);

BOOL StoreExecuteInfo(const char* cmdLineViewer, const char* cmdLineProgram, BOOL execViewer, BOOL execProgram);

LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);

DWORD MyStrToDWORD(const char* num);
int HandleErrorM(int titleID, const char* msg, unsigned long err);
BOOL LoadRemoveLog();
BOOL LoadTextFile(const char* fileName, char* buff, int buffMax);
void GetFoldersPaths();

#define WM_USER_STARTINSTALL (WM_APP + 100)   // spusti instalaci
#define WM_USER_STARTUNINSTALL (WM_APP + 101) // spusti instalaci
#define WM_USER_SHOWACTSFX7ZIP (WM_APP + 666) // nechame ukazat a aktivovat dialog SFX7ZIP (cislo je sdilene se SFX7ZIP)
#define WM_USER_CLOSEWIZARDDLG (WM_APP + 667) // zpozdene zavreni wizardu (cekame, az se spusti Readme a Salamander, pokud se dialog zavre hned, neaktivuji se vzdy spoustene softy, zustavaji na pozadi a aktivovane jsou okna pod nimi, proste des bes)
