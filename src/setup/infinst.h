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
    char ApplicationName[MAX_PATH];             // extracted from the INF file
    char ApplicationNameVer[MAX_PATH];          // including the version
    char DefaultDirectory[MAX_PATH];            // extracted from the INF file
                                                //  char UseLastDirectory[MAX_PATH];   // extracted from the INF file
    char LastDirectories[3000];                 // extracted from the INF file
    char License[100000];                       // loaded from LICENSE.TXT
    char ViewReadmePath[MAX_PATH];              // extracted from the INF file
    char CheckRunningApps[MAX_PATH];            // extracted from the INF file
    char RunProgramPath[MAX_PATH];              // extracted from the INF file
    char RunProgramQuietPath[MAX_PATH];         // extracted from the INF file
    char IncrementFileContentSrc[5000];         // extracted from the INF file
    char IncrementFileContentDst[5000];         // extracted from the INF file
    char EnsureSalamander25Dir[5000];           // extracted from the INF file
    char UnistallRunProgramQuietPath[MAX_PATH]; // extracted from the INF file
    char SaveRemoveLog[MAX_PATH];               // extracted from the INF file
    char LicenseFilePath[MAX_PATH];             // extracted from the INF file
    char FirstReadme[100000];                   // loaded using FirstReadmeFilePath
    char FirstReadmeFilePath[MAX_PATH];         // extracted from the INF file
    BOOL UseFirstReadme;
    char AutoImportConfig[MAX_PATH]; // extracted from the INF file
    char SLGLanguages[5000];         // extracted from the INF file
    char WERLocalDumps[5000];        // extracted from the INF file

    char RegPathVal['Z' - 'A' + 1][MAX_PATH];

    BOOL UseLicenseFile; // does a license file exist - should the installer go through this window?

    char CreateDirectory[MAX_PATH]; // user confirmed creating this directory

    BOOL UninstallExistingVersion; // should we uninstall the existing version of the application?
    BOOL TheExistingVersionIsSame; // is the version being overwritten the same as the one we install?

    char CopySection[400000];
    char CopyFrom[200000];
    char CopyTo[200000];
    BYTE CopyFlags[10000];
    int CopyCount;

    // it occurred to me that we can calculate the total size on the user's machine from the unpacked files
    // and thus simplify preparing setup.inf
    //  DWORD SpaceRequired;

    BOOL RebootNeeded; // after the installation finishes a reboot will be recommended

    char ShortcutSection[100000];
    BOOL DesktopPresent;
    BOOL StartMenuPresent;
    BOOL Silent; // TRUE = silent installation; FALSE = the classic UI with windows

    char CreateDirsSection[100000];
    int CreateDirsCount;

    char TmpSection[500000]; // buffer used for registry data

    char InfFile[100000]; // because the installer cannot read the INF file directly

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

    int DisplayWelcomeWarning; // 0 - no warning

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

extern HWND SfxDlg;   // SFX7ZIP window that launched this setup.exe (before setup.exe exits we show and activate it so the installed readme and Salam can be launched from it)
extern BOOL RunBySfx; // TRUE if setup.exe was started with the /runbysfx parameter

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

// options from the command line
extern char CmdLineDestination[MAX_PATH]; // if the string is not empty, it specifies the target directory (otherwise the default is used)

extern char PreviousVerOfFileToIncrementContent[10000]; // contents of the just-uninstalled plugins.ver

//
//functions
//
BOOL CreateWizard();
BOOL StopInstalling();
void InstallDone();
void CenterWindow(HWND hWindow); // centers to the main window

char* LoadStr(int resID);                        // fetches a string from resources
void InsertProgramName(char* str, BOOL version); // inserts the installed program name into str
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

#define WM_USER_STARTINSTALL (WM_APP + 100)   // start the installation
#define WM_USER_STARTUNINSTALL (WM_APP + 101) // start the uninstallation
#define WM_USER_SHOWACTSFX7ZIP (WM_APP + 666) // show and activate the SFX7ZIP dialog (the number is shared with SFX7ZIP)
#define WM_USER_CLOSEWIZARDDLG (WM_APP + 667) // delayed closing of the wizard (wait until Readme and Salamander start; if the dialog closes immediately, the launched apps are not activated and remain in the background while other windows stay on top)
