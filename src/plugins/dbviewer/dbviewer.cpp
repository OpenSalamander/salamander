// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dbviewer.rh"
#include "dbviewer.rh2"
#include "lang\lang.rh"
#include "data.h"
#include "renderer.h"
#include "dialogs.h"
#include "dbviewer.h"
#include "auxtools.h"

// neprelozeny nazev pluginu (pouziva se nez naloadime language modul + pro debug veci, kde by preklad byl naskodu)
const char* READABLE_EN_PLUGIN_NAME = "Database Viewer";

/*
TODO: rozepsany enablery; je treba implementovat offsetovou metodu (enablersOffset) na strane Salamander
      take doplnit komentar k teto promenne v SPL_GUI.H
      zavest v DBVIEWER.CPP a DEMOPLUG.CPP, kde jsou enablery globalni pro okna z vice threadu -> konflikt
*/

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro viewer
CPluginInterfaceForViewer InterfaceForViewer;

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy
HACCEL HAccel = NULL;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalGeneral = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
CSalamanderGUIAbstract* SalamanderGUI = NULL;

CWindowQueue ViewerWindowQueue("DBViewer Viewers"); // seznam vsech oken viewru
CThreadQueue ThreadQueue("DBViewer Viewers");       // seznam vsech threadu oken

#define CURRENT_CONFIG_VERSION 0
const char* CONFIG_VERSION = "Version";
const char* CONFIG_USECUSTOMFONT = "Use Custom Font";
const char* CONFIG_LOGFONT = "LogFont";
const char* CONFIG_SAVEPOS = "SavePosition";
const char* CONFIG_WNDPLACEMENT = "WindowPlacement";
const char* CONFIG_AUTOSELECT = "Auto Select";
const char* CONFIG_DEFAULT_CODING = "Default Coding";
const char* CONFIG_CSV_TEXTQUALIFIER = "CSV Text Qualifier";
const char* CONFIG_CSV_VALUESEPARATOR = "CSV Value Separator";
const char* CONFIG_CSV_VALUESEPARATORCHAR = "CSV Value Separator Char";
const char* CONFIG_CSV_FIRSTROW = "CSV First Row As Name";
const char* CONFIG_FINDHISTORY = "Find History";

const char* PLUGIN_NAME = "DBVIEWER"; // jmeno pluginu potrebne pro WinLib

// Configuration variables

int ConfigVersion = 0;         // ConfigVersion: 0 - default,
BOOL CfgUseCustomFont = FALSE; // TRUE - pouziva se font na zaklade CfgLogFont, FALSE - default font
LOGFONT CfgLogFont;
BOOL CfgSavePosition = FALSE;
WINDOWPLACEMENT CfgWindowPlacement;
BOOL CfgAutoSelect = TRUE;
char CfgDefaultCoding[210];
CCSVConfig CfgDefaultCSV;

#define IDX_TB_ROWNUMBER -3
#define IDX_TB_TERMINATOR -2
#define IDX_TB_SEPARATOR -1
#define IDX_TB_PROPERTIES 0
#define IDX_TB_COPY 1
#define IDX_TB_FIND 2
#define IDX_TB_FULLSCREEN 3
#define IDX_TB_OPEN 4
#define IDX_TB_HELP 5
#define IDX_TB_PREV 6
#define IDX_TB_NEXT 7
#define IDX_TB_PREVSELFILE 8
#define IDX_TB_NEXTSELFILE 9
#define IDX_TB_FIRST 10
#define IDX_TB_LAST 11
#define IDX_TB_COUNT 12

MENU_TEMPLATE_ITEM MenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        // Files
        {MNTT_PB, IDS_MENU_FILE, MNTS_B | MNTS_I | MNTS_A, CML_FILES, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_OPEN, MNTS_B | MNTS_I | MNTS_A, CM_FILE_OPEN, IDX_TB_OPEN, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_REFRESH, MNTS_B | MNTS_I | MNTS_A, CM_FILE_REFRESH, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_PROPERTIES, MNTS_B | MNTS_I | MNTS_A, CM_PROPERTIES, IDX_TB_PROPERTIES, 0, (DWORD*)vweDBOpened},
        //    {MNTT_SP,    -1,                           MNTS_B|MNTS_I|MNTS_A, 0,                          -1,                      0,                 NULL},
        //    {MNTT_IT,    IDS_MENU_FILES_CSV_OPTIONS,   MNTS_B|MNTS_I|MNTS_A, CM_CSV_OPTIONS,             -1,                      0,                 (DWORD*)vweCSVOpened},
        {MNTT_PB, IDS_MENU_FILE_OTHER, MNTS_B | MNTS_I | MNTS_A, CML_OTHERFILES, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_PREV, MNTS_B | MNTS_I | MNTS_A, CM_FILE_PREV, IDX_TB_PREV, 0, (DWORD*)vwePrevFile},
        {MNTT_IT, IDS_MENU_FILE_NEXT, MNTS_B | MNTS_I | MNTS_A, CM_FILE_NEXT, IDX_TB_NEXT, 0, (DWORD*)vweNextFile},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_PREVSEL, MNTS_B | MNTS_I | MNTS_A, CM_FILE_PREVSELFILE, IDX_TB_PREVSELFILE, 0, (DWORD*)vwePrevSelFile},
        {MNTT_IT, IDS_MENU_FILE_NEXTSEL, MNTS_B | MNTS_I | MNTS_A, CM_FILE_NEXTSELFILE, IDX_TB_NEXTSELFILE, 0, (DWORD*)vweNextSelFile},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_FIRST, MNTS_B | MNTS_I | MNTS_A, CM_FILE_FIRST, IDX_TB_FIRST, 0, (DWORD*)vweFirstFile},
        {MNTT_IT, IDS_MENU_FILE_LAST, MNTS_B | MNTS_I | MNTS_A, CM_FILE_LAST, IDX_TB_LAST, 0, (DWORD*)vweFirstFile},
        {MNTT_PE},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_EXIT, MNTS_B | MNTS_I | MNTS_A, CM_EXIT, -1, 0, NULL},
        {MNTT_PE},

        // Edit
        {MNTT_PB, IDS_MENU_EDIT, MNTS_B | MNTS_I | MNTS_A, CML_EDIT, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_COPY, MNTS_B | MNTS_I | MNTS_A, CM_COPY, IDX_TB_COPY, 0, (DWORD*)vweDBOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_SELECT_ALL, MNTS_B | MNTS_I | MNTS_A, CM_SELECT_ALL, -1, 0, (DWORD*)vweDBOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_FIND, MNTS_B | MNTS_I | MNTS_A, CM_FIND, IDX_TB_FIND, 0, (DWORD*)vweDBOpened},
        {MNTT_IT, IDS_MENU_EDIT_FIND_NEXT, MNTS_B | MNTS_I | MNTS_A, CM_FIND_NEXT, -1, 0, (DWORD*)vweDBOpened},
        {MNTT_IT, IDS_MENU_EDIT_FIND_PREV, MNTS_B | MNTS_I | MNTS_A, CM_FIND_PREV, -1, 0, (DWORD*)vweDBOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_TOGGLE_BM, MNTS_B | MNTS_I | MNTS_A, CM_TOGGLE_BOOKMARK, -1, 0, (DWORD*)vweDBOpened},
        {MNTT_IT, IDS_MENU_EDIT_NEXT_BM, MNTS_B | MNTS_I | MNTS_A, CM_NEXT_BOOKMARK, -1, 0, (DWORD*)vweMoreBookmarks},
        {MNTT_IT, IDS_MENU_EDIT_PREV_BM, MNTS_B | MNTS_I | MNTS_A, CM_PREV_BOOKMARK, -1, 0, (DWORD*)vweMoreBookmarks},
        {MNTT_IT, IDS_MENU_EDIT_CLEAR_ALL_BM, MNTS_B | MNTS_I | MNTS_A, CM_CLEAR_ALL_BOOKMARKS, -1, 0, (DWORD*)vweMoreBookmarks},
        {MNTT_PE},

        // View
        {MNTT_PB, IDS_MENU_VIEW, MNTS_B | MNTS_I | MNTS_A, CML_VIEW, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_GOTO, MNTS_B | MNTS_I | MNTS_A, CM_GOTO, -1, 0, (DWORD*)vweDBOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_FIELDS, MNTS_B | MNTS_I | MNTS_A, CM_FIELDS, -1, 0, (DWORD*)vweDBOpened},
        //    MENUITEM "&Normal\tCtrl+N", CM_VIEW_NORMAL
        //    MENUITEM "&Record\tCtrl+R", CM_VIEW_RECORD
        //    MENUITEM SEPARATOR
        //    MENUITEM "Show &All Records", CM_SHOW_ALL
        //    MENUITEM "Hide &Deleted Records", CM_HIDE_DELETED
        //    MENUITEM SEPARATOR
        //    POPUP "&Sort By"
        //    {
        //      MENUITEM "&Original Order", CM_SORT_BY_ORIGINAL
        //      MENUITEM "&Deleted State", CM_SORT_BY_DELETED
        //      MENUITEM SEPARATOR
        //      MENUITEM "Sort &Ascending", CM_SORT_ASCEND
        //      MENUITEM "Sort &Descending", CM_SORT_DESCEND
        //    }
        //    {MNTT_IT,    IDS_MENU_VIEW_FIELDS,         MNTS_B|MNTS_I|MNTS_A, CM_FIELDS,                  -1,                      0,                 (DWORD*)vweDBOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_FULLSCREEN, MNTS_B | MNTS_I | MNTS_A, CM_FULLSCREEN, IDX_TB_FULLSCREEN, 0, NULL},
        {MNTT_PE},

        // Convert
        {MNTT_PB, IDS_MENU_CONVERT, MNTS_B | MNTS_I | MNTS_A, CML_CONVERT, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_CONVERT_RECOGNIZE, MNTS_B | MNTS_I | MNTS_A, CM_CODING_RECOGNIZE, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_CONVERT_CODING_FIRST, MNTS_B | MNTS_I | MNTS_A, CM_CODING_FIRST, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_CONVERT_CODING_DEF, MNTS_B | MNTS_I | MNTS_A, CM_CODING_DEFAULT, -1, 0, (DWORD*)vweUncertainEncoding},
        {MNTT_IT, IDS_MENU_CONVERT_CODING_NEXT, MNTS_B | MNTS_I | MNTS_A, CM_CODING_NEXT, -1, 0, (DWORD*)vweUncertainEncoding},
        {MNTT_IT, IDS_MENU_CONVERT_CODING_PREV, MNTS_B | MNTS_I | MNTS_A, CM_CODING_PREV, -1, 0, (DWORD*)vweUncertainEncoding},
        {MNTT_PE},

        // Options
        {MNTT_PB, IDS_MENU_OPTIONS, MNTS_B | MNTS_I | MNTS_A, CML_OPTIONS, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_OPTIONS_CSV_OPTIONS, MNTS_B | MNTS_I | MNTS_A, CM_CSV_OPTIONS, -1, 0, (DWORD*)vweCSVOpened},
        {MNTT_IT, IDS_MENU_OPTIONS_CONFIG, MNTS_B | MNTS_I | MNTS_A, CM_CONFIGURATION, -1, 0, NULL},
        {MNTT_PE},

        // Help
        {MNTT_PB, IDS_MENU_HELP, MNTS_B | MNTS_I | MNTS_A, CML_HELP, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_HELP_CONTENTS, MNTS_B | MNTS_I | MNTS_A, CM_HELP_CONTENTS, IDX_TB_HELP, 0, NULL},
        {MNTT_IT, IDS_MENU_HELP_INDEX, MNTS_B | MNTS_I | MNTS_A, CM_HELP_INDEX, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_HELP_SEARCH, MNTS_B | MNTS_I | MNTS_A, CM_HELP_SEARCH, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_HELP_ABOUT, MNTS_B | MNTS_I | MNTS_A, CM_ABOUT, -1, 0, NULL},
        {MNTT_PE},

        {MNTT_PE}, // terminator
};

MENU_TEMPLATE_ITEM PopupMenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_COPY, MNTS_B | MNTS_I | MNTS_A, CM_COPY, IDX_TB_COPY, 0, (DWORD*)vweDBOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_SELECT_ALL, MNTS_B | MNTS_I | MNTS_A, CM_SELECT_ALL, -1, 0, (DWORD*)vweDBOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_FIND, MNTS_B | MNTS_I | MNTS_A, CM_FIND, -1, 0, (DWORD*)vweDBOpened},
        {MNTT_IT, IDS_MENU_EDIT_FIND_NEXT, MNTS_B | MNTS_I | MNTS_A, CM_FIND_NEXT, -1, 0, (DWORD*)vweDBOpened},
        {MNTT_IT, IDS_MENU_EDIT_FIND_PREV, MNTS_B | MNTS_I | MNTS_A, CM_FIND_PREV, -1, 0, (DWORD*)vweDBOpened},
        {MNTT_PE}, // terminator
};

struct CButtonData
{
    int ImageIndex;                   // zero base index
    WORD ToolTipResID;                // resID se stringem pro tooltip
    WORD ID;                          // univerzalni Command
    CViewerWindowEnablerEnum Enabler; // ridici promenna pro enablovani tlacitka
};

CButtonData ToolBarButtons[] =
    {
        // ImageIndex          ToolTipResID          ID               Enabler
        {IDX_TB_OPEN, IDS_TBTT_OPEN, CM_FILE_OPEN, vweAlwaysEnabled},
        {IDX_TB_COPY, IDS_TBTT_COPY, CM_COPY, vweDBOpened},
        {IDX_TB_FIND, IDS_TBTT_FIND, CM_FIND, vweDBOpened},
        {IDX_TB_PROPERTIES, IDS_TBTT_PROPERTIES, CM_PROPERTIES, vweDBOpened},
        {IDX_TB_FULLSCREEN, IDS_TBTT_FULLSCREEN, CM_FULLSCREEN, vweAlwaysEnabled},
        {IDX_TB_SEPARATOR},
        {IDX_TB_ROWNUMBER, IDS_TBTT_GOTO, CM_GOTO, vweDBOpened},
        {IDX_TB_SEPARATOR},
        {IDX_TB_TERMINATOR}};

//
// ****************************************************************************
// DllMain
//

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        DLLInstance = hinstDLL;

    if (fdwReason == DLL_PROCESS_DETACH)
    {
        if (HAccel != NULL)
        {
            DestroyAcceleratorTable(HAccel);
            HAccel = NULL;
        }
    }

    return TRUE; // DLL can be loaded
}

//
// ****************************************************************************
// LoadStr
//

char* LoadStr(int resID)
{
    return SalGeneral->LoadStr(HLanguage, resID);
}

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

void GetDefaultLogFont(LOGFONT* lf)
{
    if (!SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), lf, 0))
    {
        // kdyby nahodou selhalo SystemParametersInfo, pouzijeme nahradni reseni
        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
        *lf = ncm.lfMessageFont;
        lf->lfWeight = FW_NORMAL;
    }
}

BOOL InitViewer()
{
    HAccel = LoadAccelerators(DLLInstance, MAKEINTRESOURCE(IDA_ACCELERATORS));

    GetDefaultLogFont(&CfgLogFont);

    CfgSavePosition = FALSE;
    CfgWindowPlacement.length = 0;
    CfgDefaultCoding[0] = 0;

    CfgDefaultCSV.TextQualifier = 0;  // Auto-Select
    CfgDefaultCSV.ValueSeparator = 0; // Auto-Select
    CfgDefaultCSV.ValueSeparatorChar = 0;
    CfgDefaultCSV.FirstRowAsName = 0; // Auto-Select

    int i;
    for (i = 0; i < FIND_HISTORY_SIZE; i++)
        FindHistory[i] = NULL;

    int j;
    for (j = 0; j < 256; j++)
    {
        IsAlphaNumeric[j] = IsCharAlphaNumeric((char)j);
        IsAlpha[j] = IsCharAlpha((char)j);
    }

    if (!InitializeWinLib(PLUGIN_NAME, DLLInstance))
        return FALSE;
    SetWinLibStrings("Invalid number!", PLUGIN_NAME);
    SetupWinLibHelp(HTMLHelpCallback);
    return TRUE;
}

void ReleaseViewer()
{
    int i;
    for (i = 0; i < FIND_HISTORY_SIZE; i++)
        if (FindHistory[i] != NULL)
            free(FindHistory[i]);

    ReleaseWinLib(DLLInstance);
}

//
// ****************************************************************************
// SalamanderPluginGetReqVer
//

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

//
// ****************************************************************************
// SalamanderPluginEntry
//

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // nastavime SalamanderVersion pro "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   READABLE_EN_PLUGIN_NAME, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), READABLE_EN_PLUGIN_NAME);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalGeneral = salamander->GetSalamanderGeneral();

    // nastavime jmeno souboru s helpem
    SalGeneral->SetHelpFileName("dbviewer.chm");

    // ziskame rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
    SalamanderGUI = salamander->GetSalamanderGUI();

    if (!InitViewer())
        return NULL; // chyba

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_LOADSAVECONFIGURATION | FUNCTION_VIEWER |
                                       FUNCTION_CONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "DBVIEWER");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

//
// ****************************************************************************
// CPluginInterface
//

void DBFViewAbout(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

void CPluginInterface::About(HWND parent)
{
    DBFViewAbout(parent);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    BOOL ret = ViewerWindowQueue.Empty();
    if (!ret && (force || MessageBox(parent, LoadStr(IDS_OPENED_WINDOWS), LoadStr(IDS_PLUGINNAME),
                                     MB_YESNO | MB_ICONQUESTION) == IDYES))
    {
        ret = ViewerWindowQueue.CloseAllWindows(force) || force;
    }
    if (ret)
    {
        if (!ThreadQueue.KillAll(force) && !force)
            ret = FALSE;
        else
            ReleaseViewer();
    }
    return ret;
}

void CPluginInterface::ClearHistory(HWND parent)
{
    ViewerWindowQueue.BroadcastMessage(WM_USER_CLEARHISTORY, 0, 0);
    int i;
    for (i = 0; i < FIND_HISTORY_SIZE; i++)
    {
        if (FindHistory[i] != NULL)
        {
            free(FindHistory[i]);
            FindHistory[i] = NULL;
        }
    }
}

void CPluginInterface::Event(int event, DWORD param)
{
    switch (event)
    {
    case PLUGINEVENT_SETTINGCHANGE:
        ViewerWindowQueue.BroadcastMessage(WM_USER_SETTINGCHANGE, 0, 0);
        break;
    }
}

// ****************************************************************************

BOOL LoadHistory(CSalamanderRegistryAbstract* registry, HKEY hKey, const char* name, char* history[], int maxCount)
{
    HKEY historyKey;
    int i;
    for (i = 0; i < maxCount; i++)
        if (history[i] != NULL)
        {
            free(history[i]);
            history[i] = NULL;
        }
    if (registry->OpenKey(hKey, name, historyKey))
    {
        char buf[10];
        int j;
        for (j = 0; j < maxCount; j++)
        {
            _itoa(j + 1, buf, 10);
            DWORD bufferSize;
            if (registry->GetSize(historyKey, buf, REG_SZ, bufferSize))
            {
                history[j] = (char*)malloc(bufferSize);
                if (history[j] == NULL)
                {
                    TRACE_E("Low memory");
                    break;
                }
                if (!registry->GetValue(historyKey, buf, REG_SZ, history[j], bufferSize))
                    break;
            }
        }
        registry->CloseKey(historyKey);
    }
    return TRUE;
}

// ****************************************************************************

BOOL SaveHistory(CSalamanderRegistryAbstract* registry, HKEY hKey, const char* name, char* history[], int maxCount)
{
    HKEY historyKey;
    if (registry->CreateKey(hKey, name, historyKey))
    {
        registry->ClearKey(historyKey);

        BOOL saveHistory = FALSE;
        SalGeneral->GetConfigParameter(SALCFG_SAVEHISTORY, &saveHistory, sizeof(BOOL), NULL);
        if (saveHistory)
        {
            char buf[10];
            int i;
            for (i = 0; i < maxCount; i++)
            {
                if (history[i] != NULL)
                {
                    _itoa(i + 1, buf, 10);
                    registry->SetValue(historyKey, buf, REG_SZ, history[i], (DWORD)strlen(history[i]) + 1);
                }
                else
                    break;
            }
        }
        registry->CloseKey(historyKey);
    }
    return TRUE;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    if (regKey != NULL) // load z registry
    {
        if (!registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD)))
        {
            ConfigVersion = CURRENT_CONFIG_VERSION; // asi nejakej nenechavec... ;-)
        }
        registry->GetValue(regKey, CONFIG_USECUSTOMFONT, REG_DWORD, &CfgUseCustomFont, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_LOGFONT, REG_BINARY, &CfgLogFont, sizeof(LOGFONT));
        registry->GetValue(regKey, CONFIG_SAVEPOS, REG_DWORD, &CfgSavePosition, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_WNDPLACEMENT, REG_BINARY, &CfgWindowPlacement, sizeof(WINDOWPLACEMENT));
        registry->GetValue(regKey, CONFIG_AUTOSELECT, REG_DWORD, &CfgAutoSelect, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_DEFAULT_CODING, REG_SZ, &CfgDefaultCoding, 210);

        registry->GetValue(regKey, CONFIG_CSV_TEXTQUALIFIER, REG_DWORD, &CfgDefaultCSV.TextQualifier, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_CSV_VALUESEPARATOR, REG_DWORD, &CfgDefaultCSV.ValueSeparator, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_CSV_VALUESEPARATORCHAR, REG_DWORD, &CfgDefaultCSV.ValueSeparatorChar, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_CSV_FIRSTROW, REG_DWORD, &CfgDefaultCSV.FirstRowAsName, sizeof(DWORD));

        LoadHistory(registry, regKey, CONFIG_FINDHISTORY, FindHistory, FIND_HISTORY_SIZE);
    }
    else // default configuration
    {
        ConfigVersion = 0;
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_USECUSTOMFONT, REG_DWORD, &CfgUseCustomFont, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_LOGFONT, REG_BINARY, &CfgLogFont, sizeof(LOGFONT));
    registry->SetValue(regKey, CONFIG_SAVEPOS, REG_DWORD, &CfgSavePosition, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_WNDPLACEMENT, REG_BINARY, &CfgWindowPlacement, sizeof(WINDOWPLACEMENT));
    registry->SetValue(regKey, CONFIG_AUTOSELECT, REG_DWORD, &CfgAutoSelect, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_DEFAULT_CODING, REG_SZ, &CfgDefaultCoding, -1);

    registry->SetValue(regKey, CONFIG_CSV_TEXTQUALIFIER, REG_DWORD, &CfgDefaultCSV.TextQualifier, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_CSV_VALUESEPARATOR, REG_DWORD, &CfgDefaultCSV.ValueSeparator, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_CSV_VALUESEPARATORCHAR, REG_DWORD, &CfgDefaultCSV.ValueSeparatorChar, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_CSV_FIRSTROW, REG_DWORD, &CfgDefaultCSV.FirstRowAsName, sizeof(DWORD));

    SaveHistory(registry, regKey, CONFIG_FINDHISTORY, FindHistory, FIND_HISTORY_SIZE);
}

void OnConfiguration(HWND hParent, BOOL bFromSalamander)
{
    static BOOL InConfiguration = FALSE;
    if (InConfiguration)
    {
        SalGeneral->SalMessageBox(hParent,
                                  LoadStr(IDS_CFG_CONFLICT), LoadStr(IDS_PLUGINNAME),
                                  MB_ICONINFORMATION | MB_OK);
        return;
    }
    InConfiguration = TRUE;
    if (CConfigurationDialog(hParent, bFromSalamander).Execute() == IDOK)
    {
        ViewerWindowQueue.BroadcastMessage(WM_USER_VIEWERCFGCHNG, 0, 0);
    }
    InConfiguration = FALSE;
}

void CPluginInterface::Configuration(HWND parent)
{
    OnConfiguration(parent, TRUE);
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");
    salamander->AddViewer("*.csv;*.dbf", FALSE); // default (install pluginu), jinak Salam ignoruje
}

CPluginInterfaceForViewerAbstract*
CPluginInterface::GetInterfaceForViewer()
{
    return &InterfaceForViewer;
}

//
// ****************************************************************************
// CPluginInterfaceForViewer
//

struct CTVData
{
    BOOL AlwaysOnTop;
    const char* Name;
    int Left, Top, Width, Height;
    UINT ShowCmd;
    BOOL ReturnLock;
    HANDLE* Lock;
    BOOL* LockOwner;
    BOOL Success;
    HANDLE Continue;
    int EnumFilesSourceUID;
    int EnumFilesCurrentIndex;
};

unsigned WINAPI ViewerThreadBody(void* param)
{
    CALL_STACK_MESSAGE1("ViewerThreadBody()");
    SetThreadNameInVCAndTrace(READABLE_EN_PLUGIN_NAME);
    TRACE_I("Begin");

    CTVData* data = (CTVData*)param;

    CViewerWindow* window = new CViewerWindow(data->EnumFilesSourceUID, data->EnumFilesCurrentIndex);
    if (window != NULL)
    {
        if (data->ReturnLock)
        {
            *data->Lock = window->GetLock();
            *data->LockOwner = TRUE;
        }
        CALL_STACK_MESSAGE1("ViewerThreadBody::CreateWindowEx");
        if (!data->ReturnLock || *data->Lock != NULL)
        {
            if (CfgSavePosition && CfgWindowPlacement.length != 0)
            {
                WINDOWPLACEMENT place = CfgWindowPlacement;
                // GetWindowPlacement cti Taskbar, takze pokud je Taskbar nahore nebo vlevo,
                // jsou hodnoty posunute o jeho rozmery. Provedeme korekci.
                RECT monitorRect;
                RECT workRect;
                SalGeneral->MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
                OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
                           workRect.top - monitorRect.top);
                SalGeneral->MultiMonEnsureRectVisible(&place.rcNormalPosition, TRUE);
                data->Left = place.rcNormalPosition.left;
                data->Top = place.rcNormalPosition.top;
                data->Width = place.rcNormalPosition.right - place.rcNormalPosition.left;
                data->Height = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
                data->ShowCmd = place.showCmd;
            }
            if (window->CreateEx(data->AlwaysOnTop ? WS_EX_TOPMOST : 0,
                                 CWINDOW_CLASSNAME2,
                                 LoadStr(IDS_PLUGINNAME),
                                 WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                 data->Left,
                                 data->Top,
                                 data->Width,
                                 data->Height,
                                 NULL,
                                 NULL,
                                 DLLInstance,
                                 window) != NULL)
            {
                CALL_STACK_MESSAGE1("ViewerThreadBody::ShowWindow");
                // !POZOR! ziskane ikony je treba ve WM_DESTROY destruovat
                SendMessage(window->HWindow, WM_SETICON, ICON_BIG,
                            (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_MAIN)));
                SendMessage(window->HWindow, WM_SETICON, ICON_SMALL,
                            (LPARAM)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_MAIN),
                                              IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
                ShowWindow(window->HWindow, data->ShowCmd);
                SetForegroundWindow(window->HWindow);
                data->Success = TRUE;
                window->Renderer.Creating = FALSE;
            }
            else
            {
                if (data->ReturnLock && *data->Lock != NULL)
                    CloseHandle(*data->Lock);
            }
        }
    }

    CALL_STACK_MESSAGE1("ViewerThreadBody::SetEvent");
    char name[MAX_PATH];
    strcpy(name, data->Name);
    BOOL openFile = data->Success;
    SetEvent(data->Continue); // pustime dale hl. thread, od tohoto bodu nejsou data platna (=NULL)
    data = NULL;

    // pokud probehlo vse bez potizi, otevreme v okne pozadovany soubor
    if (openFile)
    {
        CALL_STACK_MESSAGE1("ViewerThreadBody::OpenFile");
        window->Renderer.OpenFile(name, TRUE);

        CALL_STACK_MESSAGE1("ViewerThreadBody::message-loop");
        // message loopa
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if ((!window->IsMenuBarMessage(&msg)) &&
                (!TranslateAccelerator(window->HWindow, HAccel, &msg)))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    if (window != NULL)
        delete window;

    TRACE_I("End");
    return 0;
}

BOOL CPluginInterfaceForViewer::ViewFile(const char* name, int left, int top, int width, int height,
                                         UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                         BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                         int enumFilesSourceUID, int enumFilesCurrentIndex)
{
    CTVData data;
    data.AlwaysOnTop = alwaysOnTop;
    data.Name = name;
    data.Left = left;
    data.Top = top;
    data.Width = width;
    data.Height = height;
    data.ShowCmd = showCmd;
    data.ReturnLock = returnLock;
    data.Lock = lock;
    data.LockOwner = lockOwner;
    data.Success = FALSE;
    data.EnumFilesSourceUID = enumFilesSourceUID;
    data.EnumFilesCurrentIndex = enumFilesCurrentIndex;
    data.Continue = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (data.Continue == NULL)
    {
        TRACE_E("Nepodarilo se vytvorit Continue event.");
        return FALSE;
    }

    if (ThreadQueue.StartThread(ViewerThreadBody, &data))
    {
        // pockame, az thread zpracuje predana data a vrati vysledky
        WaitForSingleObject(data.Continue, INFINITE);
    }
    else
        data.Success = FALSE;
    CloseHandle(data.Continue);
    return data.Success;
}

//
// ****************************************************************************
// CViewerWindow
//

#define BANDID_MENU 1
#define BANDID_TOOLBAR 2

#define CODING_MENU_INDEX 3

CViewerWindow::CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex) : CWindow(ooStatic), FindDialog(HLanguage, IDD_FIND, IDD_FIND), Renderer(enumFilesSourceUID, enumFilesCurrentIndex)
{
    Renderer.Viewer = this;
    Lock = NULL;
    CfgCSV = CfgDefaultCSV;
    HRebar = NULL;
    MainMenu = NULL;
    MenuBar = NULL;
    ToolBar = NULL;
    HGrayToolBarImageList = NULL;
    HHotToolBarImageList = NULL;
    ZeroMemory(Enablers, sizeof(Enablers));
    IsSrcFileSelected = FALSE;
}

BOOL CViewerWindow::IsMenuBarMessage(CONST MSG* lpMsg)
{
    if (MenuBar == NULL)
        return FALSE;
    return MenuBar->IsMenuBarMessage(lpMsg);
}

BOOL CViewerWindow::InitializeGraphics()
{
    HBITMAP hTmpMaskBitmap;
    HBITMAP hTmpGrayBitmap;
    HBITMAP hTmpColorBitmap;

    hTmpColorBitmap = LoadBitmap(DLLInstance, MAKEINTRESOURCE(SalGeneral->CanUse256ColorsBitmap() ? IDB_TOOLBAR256 : IDB_TOOLBAR16));
    SalamanderGUI->CreateGrayscaleAndMaskBitmaps(hTmpColorBitmap, RGB(255, 0, 255),
                                                 hTmpGrayBitmap, hTmpMaskBitmap);
    HHotToolBarImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLORDDB, IDX_TB_COUNT, 1);
    HGrayToolBarImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLORDDB, IDX_TB_COUNT, 1);
    ImageList_Add(HHotToolBarImageList, hTmpColorBitmap, hTmpMaskBitmap);
    ImageList_Add(HGrayToolBarImageList, hTmpGrayBitmap, hTmpMaskBitmap);
    DeleteObject(hTmpMaskBitmap);
    DeleteObject(hTmpGrayBitmap);
    DeleteObject(hTmpColorBitmap);
    return TRUE;
}

BOOL CViewerWindow::ReleaseGraphics()
{
    if (HHotToolBarImageList != NULL)
        ImageList_Destroy(HHotToolBarImageList);
    if (HGrayToolBarImageList != NULL)
        ImageList_Destroy(HGrayToolBarImageList);
    return TRUE;
}

BOOL CViewerWindow::FillToolBar()
{
    char emptyBuff[] = "";
    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_IMAGEINDEX | TLBI_MASK_ID | TLBI_MASK_ENABLER | TLBI_MASK_STYLE;

    ToolBar->SetImageList(HGrayToolBarImageList);
    ToolBar->SetHotImageList(HHotToolBarImageList);

    int i;
    for (i = 0; ToolBarButtons[i].ImageIndex != IDX_TB_TERMINATOR; i++)
    {
        tii.Mask = TLBI_MASK_IMAGEINDEX | TLBI_MASK_ID | TLBI_MASK_ENABLER | TLBI_MASK_STYLE;
        if (ToolBarButtons[i].ImageIndex == IDX_TB_SEPARATOR)
        {
            tii.Style = TLBI_STYLE_SEPARATOR;
        }
        else if (ToolBarButtons[i].ImageIndex == IDX_TB_ROWNUMBER)
        {
            tii.Mask = TLBI_MASK_ID | TLBI_MASK_TEXT | TLBI_MASK_STYLE | TLBI_MASK_ENABLER;
            tii.Style = TLBI_STYLE_SHOWTEXT;
            tii.Text = emptyBuff;
            tii.ID = ToolBarButtons[i].ID;
            tii.Enabler = Enablers + ToolBarButtons[i].Enabler;
        }
        else
        {
            tii.Style = 0;
            tii.ImageIndex = ToolBarButtons[i].ImageIndex;
            tii.ID = ToolBarButtons[i].ID;
            if (ToolBarButtons[i].Enabler == vweAlwaysEnabled)
                tii.Enabler = NULL;
            else
                tii.Enabler = Enablers + ToolBarButtons[i].Enabler;
        }
        ToolBar->InsertItem2(0xFFFFFFFF, TRUE, &tii);
    }

    // obehne enablery
    ToolBar->UpdateItemsState();
    return TRUE;
}

BOOL CViewerWindow::InsertMenuBand()
{
    REBARBANDINFO rbbi;
    ZeroMemory(&rbbi, sizeof(rbbi));

    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_ID;
    rbbi.cxMinChild = MenuBar->GetNeededWidth();
    rbbi.cyMinChild = MenuBar->GetNeededHeight();
    rbbi.fStyle = RBBS_NOGRIPPER;
    rbbi.hwndChild = MenuBar->GetHWND();
    rbbi.wID = BANDID_MENU;
    SendMessage(HRebar, RB_INSERTBAND, (WPARAM)0, (LPARAM)&rbbi);
    return TRUE;
}

BOOL CViewerWindow::InsertToolBarBand()
{
    REBARBANDINFO rbbi;
    ZeroMemory(&rbbi, sizeof(rbbi));

    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_SIZE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_ID;
    rbbi.cxMinChild = ToolBar->GetNeededWidth();
    rbbi.cyMinChild = ToolBar->GetNeededHeight();
    rbbi.cx = 10000;
    rbbi.fStyle = RBBS_NOGRIPPER;
    rbbi.hwndChild = ToolBar->GetHWND();
    rbbi.wID = BANDID_TOOLBAR;
    SendMessage(HRebar, RB_INSERTBAND, (WPARAM)1, (LPARAM)&rbbi);
    return TRUE;
}

BOOL CViewerWindow::InitCodingSubmenu()
{
    CGUIMenuPopupAbstract* popup = MainMenu->GetSubMenu(CML_CONVERT, FALSE);
    if (popup == NULL)
        return FALSE;

    MENU_ITEM_INFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING | MENU_MASK_ENABLER;

    int index = 0;
    int menuIndex = 1;
    const char *name, *table;
    char buff[1000];
    mi.String = buff;
    mi.Enabler = &Enablers[vweUncertainEncoding];
    while (SalGeneral->EnumConversionTables(HWindow, &index, &name, &table))
    {
        if (name == NULL)
        {
            mi.ID = 0;
            mi.Type = MENU_TYPE_SEPARATOR;
            buff[0] = 0;
        }
        else
        {
            mi.ID = CM_CODING_FIRST + menuIndex;
            mi.Type = MENU_TYPE_STRING;
            lstrcpyn(buff, name, 1000);
            menuIndex++;
        }
        popup->InsertItem(2 + index, TRUE, &mi);
    }
    ConversionsCount = menuIndex;
    return TRUE;
}

int CViewerWindow::GetCodingMenuIndex(const char* coding)
{
    int index = 0;
    const char* name;
    int menuIndex = 1;
    while (SalGeneral->EnumConversionTables(HWindow, &index, &name, NULL))
    {
        if (name != NULL)
        {
            if (lstrcmpi(name, coding) == 0)
                return menuIndex;
            menuIndex++;
        }
    }
    return 0; // none
}

int CViewerWindow::GetNextCodingMenuIndex(const char* coding, BOOL next)
{
    int menuIndex = GetCodingMenuIndex(coding);
    if (next)
        menuIndex++;
    else
        menuIndex--;

    if (menuIndex >= ConversionsCount)
        menuIndex = 0;
    if (menuIndex < 0)
        menuIndex = ConversionsCount - 1;

    return menuIndex;
}

void CViewerWindow::OnFind(WORD command)
{
    if (command == CM_FIND || FindDialog.Text[0] == 0)
        if (FindDialog.Execute() != IDOK || FindDialog.Text[0] == 0)
            return;

    BOOL forward = (command != CM_FIND_PREV) ^ (!FindDialog.Forward);
    WORD flags = SASF_FORWARD; // hledame vzdy dopredu, protoze hledame prez jendnotlive bunky
    if (FindDialog.CaseSensitive)
        flags |= SASF_CASESENSITIVE;

    if (FindDialog.Regular)
    {
        CSalamanderREGEXPSearchData* regexp = SalGeneral->AllocSalamanderREGEXPSearchData();
        if (regexp != NULL)
        {
            if (regexp->Set(FindDialog.Text, flags))
            {
                Renderer.Find(forward, FindDialog.WholeWords, NULL, regexp);
            }
            else // chyba - regularniho vyrazu (nesedi zavorky, atd.) nebo nedostatek pameti
            {
                SalGeneral->SalMessageBox(HWindow, regexp->GetLastErrorText(),
                                          LoadStr(IDS_REGEXP_ERROR), MB_ICONEXCLAMATION);
            }
            SalGeneral->FreeSalamanderREGEXPSearchData(regexp);
        }
    }
    else
    {
        CSalamanderBMSearchData* bm = SalGeneral->AllocSalamanderBMSearchData();
        if (bm != NULL)
        {
            bm->Set(FindDialog.Text, flags);
            if (bm->IsGood())
                Renderer.Find(forward, FindDialog.WholeWords, bm, NULL);
            SalGeneral->FreeSalamanderBMSearchData(bm);
        }
    }
}

void CViewerWindow::LayoutWindows()
{
    RECT r;
    GetClientRect(HWindow, &r);
    SendMessage(HWindow, WM_SIZE, SIZE_RESTORED,
                MAKELONG(r.right - r.left, r.bottom - r.top));
}

HANDLE
CViewerWindow::GetLock()
{
    if (Lock == NULL)
        Lock = CreateEvent(NULL, FALSE, FALSE, NULL);
    return Lock;
}

void CViewerWindow::UpdateEnablers()
{
    Enablers[vweDBOpened] = Renderer.Database.IsOpened();
    Enablers[vweCSVOpened] = _stricmp(Renderer.Database.GetParserName(), "csv") == 0;
    Enablers[vweMoreBookmarks] = Enablers[vweDBOpened] && Renderer.GetBookmarkCount() > 0;
    Enablers[vweUncertainEncoding] = !Renderer.Database.GetIsUnicode();

    LPCTSTR FileName = Renderer.Database.GetFileName();

    if (IsWindowVisible(HWindow))
    {
        BOOL srcBusy, noMoreFiles;
        TCHAR fileName[MAX_PATH] = _T("");
        LPCTSTR openedFileName = FileName;
        int enumFilesCurrentIndex = Renderer.EnumFilesCurrentIndex;

        BOOL ok = SalGeneral->GetPreviousFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                           &enumFilesCurrentIndex,
                                                           openedFileName, FALSE,
                                                           TRUE, fileName, &noMoreFiles,
                                                           &srcBusy);
        Enablers[vwePrevFile] = ok || srcBusy;                 // jen pokud existuje nejaky predchazejici soubor (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)
        Enablers[vweFirstFile] = ok || srcBusy || noMoreFiles; // skok na prvni nebo posledni soubor jde jen pokud neni prerusena vazba se zdrojem (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)

        if (Enablers[vweFirstFile])
        { // pokud neni spojeni se zdrojem, nema smysl zjistovat dalsi informace
            // zjistime jestli existuje nejaky predchozi selected soubor
            enumFilesCurrentIndex = Renderer.EnumFilesCurrentIndex;
            ok = SalGeneral->GetPreviousFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                          &enumFilesCurrentIndex,
                                                          openedFileName,
                                                          TRUE /* prefer selected */, TRUE,
                                                          fileName, &noMoreFiles,
                                                          &srcBusy);
            BOOL isSrcFileSel = FALSE;
            if (ok)
            {
                ok = SalGeneral->IsFileNameForViewerSelected(Renderer.EnumFilesSourceUID,
                                                             enumFilesCurrentIndex,
                                                             fileName, &isSrcFileSel,
                                                             &srcBusy);
            }
            Enablers[vwePrevSelFile] = ok && isSrcFileSel || srcBusy; // jen pokud je predchazejici soubor opravdu selected (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)

            if (FileName && (*FileName != '<'))
            {
                //           ok = SalGeneral->IsFileNameForViewerSelected(Renderer.EnumFilesSourceUID,
                //                                                            Renderer.EnumFilesCurrentIndex,
                //                                                            Renderer.FileName, &IsSrcFileSelected,
                //                                                            &srcBusy);
                //           Enablers[vweSelSrcFile] = ok || srcBusy;  // jen pokud zdrojovy soubor existuje (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)
            }
            else
            {
                //           Enablers[vweSelSrcFile] = FALSE;
                IsSrcFileSelected = FALSE;
            }

            fileName[0] = 0;
            enumFilesCurrentIndex = Renderer.EnumFilesCurrentIndex;
            ok = SalGeneral->GetNextFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                      &enumFilesCurrentIndex,
                                                      openedFileName, FALSE,
                                                      TRUE, fileName, &noMoreFiles,
                                                      &srcBusy);
            Enablers[vweNextFile] = ok || srcBusy; // jen pokud existuje nejaky dalsi soubor (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)

            // zjistime jestli je dalsi soubor selected nebo jestli uz zadny selected neni na sklade
            enumFilesCurrentIndex = Renderer.EnumFilesCurrentIndex;
            ok = SalGeneral->GetNextFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                      &enumFilesCurrentIndex,
                                                      openedFileName,
                                                      TRUE /* prefer selected */, TRUE,
                                                      fileName, &noMoreFiles,
                                                      &srcBusy);
            isSrcFileSel = FALSE;
            if (ok)
            {
                ok = SalGeneral->IsFileNameForViewerSelected(Renderer.EnumFilesSourceUID,
                                                             enumFilesCurrentIndex,
                                                             fileName, &isSrcFileSel,
                                                             &srcBusy);
            }
            Enablers[vweNextSelFile] = ok && isSrcFileSel || srcBusy; // jen pokud je dalsi soubor opravdu selected (nebo je Salam busy, to nezbyva nez to usera nechat zkusit pozdeji)
        }
        else
        {
            //        Enablers[vweSelSrcFile] = FALSE;
            Enablers[vweNextFile] = FALSE;
            Enablers[vwePrevSelFile] = FALSE;
            Enablers[vweNextSelFile] = FALSE;
            IsSrcFileSelected = FALSE;
        }
    }
    else
    {
        //     Enablers[vweSelSrcFile] = FALSE;
        Enablers[vwePrevFile] = FALSE;
        Enablers[vweNextFile] = FALSE;
        Enablers[vwePrevSelFile] = FALSE;
        Enablers[vweNextSelFile] = FALSE;
        Enablers[vweFirstFile] = FALSE;
        IsSrcFileSelected = FALSE;
    }

    if (ToolBar != NULL)
        ToolBar->UpdateItemsState();
}

void CViewerWindow::UpdateRowNumberOnToolBar(int cur /*zero-based*/, int tot)
{
    TLBI_ITEM_INFO2 tii;
    TCHAR buf[50];

    _stprintf(buf, _T("%d/%d"), cur + 1, tot);

    tii.Mask = TLBI_MASK_TEXT;
    tii.Text = buf;
    ToolBar->SetItemInfo2(CM_GOTO, FALSE, &tii);
}

LRESULT
CViewerWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        InitializeGraphics();
        MainMenu = SalamanderGUI->CreateMenuPopup();
        if (MainMenu == NULL)
            return -1;
        ToolBar = SalamanderGUI->CreateToolBar(HWindow);
        if (ToolBar == NULL)
            return -1;
        MainMenu->LoadFromTemplate(HLanguage, MenuTemplate, Enablers, HGrayToolBarImageList, HHotToolBarImageList);
        MenuBar = SalamanderGUI->CreateMenuBar(MainMenu, HWindow);
        if (MenuBar == NULL)
            return -1;

        RECT r;
        GetClientRect(HWindow, &r);
        HRebar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, "",
                                WS_VISIBLE | /*WS_BORDER |  */ WS_CHILD |
                                    WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                                    RBS_VARHEIGHT | CCS_NODIVIDER |
                                    RBS_BANDBORDERS | CCS_NOPARENTALIGN |
                                    RBS_AUTOSIZE,
                                0, 0, r.right, r.bottom, // dummy
                                HWindow, (HMENU)0, DLLInstance, NULL);

        // nechceme vizualni styly pro rebar
        SalamanderGUI->DisableWindowVisualStyles(HRebar);

        Renderer.CreateEx(WS_EX_CLIENTEDGE,
                          CWINDOW_CLASSNAME2,
                          "",
                          WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPSIBLINGS,
                          0,
                          0,
                          0,
                          0,
                          HWindow,
                          NULL,
                          DLLInstance,
                          &Renderer);

        FindDialog.SetParent(HWindow);

        MenuBar->CreateWnd(HRebar);
        InsertMenuBand();
        ToolBar->CreateWnd(HRebar);
        ToolBar->SetStyle(TLB_STYLE_TEXT | TLB_STYLE_IMAGE);
        FillToolBar();
        InsertToolBarBand();

        ViewerWindowQueue.Add(new CWindowQueueItem(HWindow));
        InitCodingSubmenu();
        break;
    }

    case WM_ERASEBKGND:
    {
        return 1;
    }

    case WM_NOTIFY:
    {
        LPNMHDR lphdr = (LPNMHDR)lParam;
        if (lphdr->code == RBN_AUTOSIZE)
        {
            LayoutWindows();
            return 0;
        }
        break;
    }

    case WM_SIZE:
    {
        if (Renderer.HWindow != NULL && HRebar != NULL && MenuBar != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);

            RECT rebarRect;
            GetWindowRect(HRebar, &rebarRect);
            int rebarHeight = rebarRect.bottom - rebarRect.top;

            HDWP hdwp = BeginDeferWindowPos(2);
            if (hdwp != NULL)
            {
                // + 4: pri zvetsovani sirky okna mi nechodilo prekreslovani poslednich 4 bodu
                // v rebaru; ani po nekolika hodinach jsem nenasel pricinu; v Salamu to slape;
                // zatim to resim takto; treba si casem vzpomenu, kde je problem
                hdwp = DeferWindowPos(hdwp, HRebar, NULL,
                                      0, 0, r.right + 4, rebarHeight,
                                      SWP_NOACTIVATE | SWP_NOZORDER);

                hdwp = DeferWindowPos(hdwp, Renderer.HWindow, NULL,
                                      0, rebarHeight, r.right, r.bottom - rebarHeight,
                                      SWP_NOACTIVATE | SWP_NOZORDER);
                EndDeferWindowPos(hdwp);
            }
        }
        break;
    }

    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
            PostMessage(HWindow, WM_CLOSE, 0, 0);
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        // tady by se mely premapovat barvy
        TRACE_I("CViewerWindow::WindowProc - WM_SYSCOLORCHANGE");
        break;
    }

    case WM_USER_VIEWERCFGCHNG:
    {
        // tady by se mely projevit zmeny v konfiguraci pluginu
        TRACE_I("CViewerWindow::WindowProc - config has changed");
        Renderer.RebuildGraphics();
        Renderer.SetupScrollBars();
        InvalidateRect(Renderer.HWindow, NULL, TRUE);
        return 0;
    }

    case WM_USER_CLEARHISTORY:
    {
        if (FindDialog.HWindow != NULL)
            PostMessage(FindDialog.HWindow, uMsg, wParam, lParam);
        return 0;
    }

    case WM_ACTIVATE:
    {
        if (!LOWORD(wParam))
        {
            // hlavni okno pri prepnuti z viewru nebude delat refresh
            SalGeneral->SkipOneActivateRefresh();
        }
        break;
    }

    case WM_SETFOCUS:
    {
        if (Renderer.HWindow != NULL)
            SetFocus(Renderer.HWindow);
        break;
    }

    case WM_DESTROY:
    {
        if (CfgSavePosition)
        {
            CfgWindowPlacement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(HWindow, &CfgWindowPlacement);
        }
        CfgAutoSelect = Renderer.AutoSelect;
        strcpy(CfgDefaultCoding, Renderer.DefaultCoding);

        if (MenuBar != NULL)
        {
            SalamanderGUI->DestroyMenuBar(MenuBar);
            MenuBar = NULL;
        }
        if (MainMenu != NULL)
        {
            SalamanderGUI->DestroyMenuPopup(MainMenu);
            MainMenu = NULL;
        }
        if (ToolBar != NULL)
        {
            SalamanderGUI->DestroyToolBar(ToolBar);
            ToolBar = NULL;
        }

        if (Renderer.HWindow != NULL)
            DestroyWindow(Renderer.HWindow);
        ViewerWindowQueue.Remove(HWindow);

        if (Lock != NULL)
        {
            SetEvent(Lock);
            Lock = NULL;
        }
        ReleaseGraphics();

        // ikony ziskane bez flagu LR_SHARED je treba destruovat (ICON_SMALL)
        // a sdilenym (ICON_BIG) destrukce neublizi (nic se nestane)
        HICON hIcon = (HICON)SendMessage(HWindow, WM_SETICON, ICON_BIG, NULL);
        if (hIcon != NULL)
            DestroyIcon(hIcon);
        hIcon = (HICON)SendMessage(HWindow, WM_SETICON, ICON_SMALL, NULL);
        if (hIcon != NULL)
            DestroyIcon(hIcon);

        PostQuitMessage(0);
        break;
    }
        /*
    case WM_INITMENU:
    {
      HMENU hMenu = GetMenu(HWindow);
      if (hMenu != NULL)
      {
        BOOL dbOpened = Renderer.Database.IsOpened();
        EnableMenuItem(hMenu, CM_FIELDS, MF_BYCOMMAND | (dbOpened ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, CM_PROPERTIES, MF_BYCOMMAND | (dbOpened ? MF_ENABLED : MF_GRAYED));
        BOOL csvOpened = lstrcmpi(Renderer.Database.GetParserName(), "csv") == 0;
        EnableMenuItem(hMenu, CM_CSV_OPTIONS, MF_BYCOMMAND | (csvOpened ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, CM_COPY, MF_BYCOMMAND | (dbOpened ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, CM_SELECT_ALL, MF_BYCOMMAND | (dbOpened ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, CM_FIND, MF_BYCOMMAND | (dbOpened ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, CM_FIND_NEXT, MF_BYCOMMAND | (dbOpened ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, CM_FIND_PREV, MF_BYCOMMAND | (dbOpened ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, CM_TOGGLE_BOOKMARK, MF_BYCOMMAND | (dbOpened ? MF_ENABLED : MF_GRAYED));
        BOOL moreBookmarks = dbOpened  && Renderer.GetBookmarkCount() > 0;
        EnableMenuItem(hMenu, CM_NEXT_BOOKMARK, MF_BYCOMMAND | (moreBookmarks ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, CM_PREV_BOOKMARK, MF_BYCOMMAND | (moreBookmarks ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, CM_CLEAR_ALL_BOOKMARKS, MF_BYCOMMAND | (moreBookmarks ? MF_ENABLED : MF_GRAYED));
      }
      break;
    }
*/
    case WM_USER_INITMENUPOPUP:
    {
        CGUIMenuPopupAbstract* popup = (CGUIMenuPopupAbstract*)wParam;
        WORD popupID = HIWORD(lParam);

        if (popupID == CML_CONVERT)
        {
            BOOL autoSelect = Renderer.AutoSelect;
            popup->CheckItem(CM_CODING_RECOGNIZE, FALSE, autoSelect);

            int menuIndex = GetCodingMenuIndex(Renderer.Coding);

            popup->CheckRadioItem(CM_CODING_FIRST, CM_CODING_LAST,
                                  CM_CODING_FIRST + menuIndex, FALSE);

            if (autoSelect)
            {
                popup->SetDefaultItem(-1, FALSE);
            }
            else
            {
                int defMenuIndex = GetCodingMenuIndex(Renderer.DefaultCoding);
                popup->SetDefaultItem(CM_CODING_FIRST + defMenuIndex, FALSE);
            }
        }
        UpdateEnablers();
        break;
    }

    case WM_USER_TBGETTOOLTIP:
    {
        TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
        lstrcpy(tt->Buffer, LoadStr(ToolBarButtons[tt->Index].ToolTipResID));
        SalamanderGUI->PrepareToolTipText(tt->Buffer, FALSE);
        return TRUE;
    }

    case WM_USER_SETTINGCHANGE:
    {
        if (MenuBar != NULL)
            MenuBar->SetFont();
        if (ToolBar != NULL)
            ToolBar->SetFont();
        return 0;
    }

    case WM_COMMAND:
    {
        WORD command = LOWORD(wParam);
        if (command >= CM_CODING_FIRST && command <= CM_CODING_LAST)
        {
            CGUIMenuPopupAbstract* popup = MainMenu->GetSubMenu(CML_CONVERT, FALSE);
            if (popup != NULL)
            {
                char conversion[220];
                MENU_ITEM_INFO mii;
                mii.Mask = MENU_MASK_STRING;
                mii.String = conversion;
                mii.StringLen = 220;
                if (popup->GetItemInfo(command, FALSE, &mii))
                    Renderer.SelectConversion(command == CM_CODING_FIRST ? NULL : conversion);
            }
            return 0;
        }

        switch (command)
        {
        case CM_FILE_OPEN:
        {
            Renderer.OnFileOpen();
            return 0;
        }

        case CM_FILE_REFRESH:
        {
            Renderer.OnFileReOpen();
            return 0;
        }

        case CM_FILE_FIRST:
        case CM_FILE_PREV:
        case CM_FILE_NEXT:
        case CM_FILE_PREVSELFILE:
        case CM_FILE_NEXTSELFILE:
        case CM_FILE_LAST:
        {
            LPCTSTR FileName = Renderer.Database.GetFileName();

            if (!FileName || !*FileName || (*FileName != '<') /* || !strcmp(FileName, LoadStr(IDS_DELETED_TITLE))*/)
            {
                BOOL ok = FALSE;
                BOOL srcBusy = FALSE;
                BOOL noMoreFiles = FALSE;
                TCHAR fileName[MAX_PATH] = _T("");
                LPCTSTR openedFileName = FileName;
                int enumFilesCurrentIndex = Renderer.EnumFilesCurrentIndex;

                if ((command == CM_FILE_PREV) || (command == CM_FILE_LAST) || (command == CM_FILE_PREVSELFILE))
                {
                    if (command == CM_FILE_LAST)
                    {
                        enumFilesCurrentIndex = -1;
                    }
                    ok = SalGeneral->GetPreviousFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                                  &enumFilesCurrentIndex,
                                                                  openedFileName,
                                                                  command == CM_FILE_PREVSELFILE,
                                                                  TRUE, fileName,
                                                                  &noMoreFiles, &srcBusy);
                    if (ok && (command == CM_FILE_PREVSELFILE))
                    { // bereme jen selected soubory
                        BOOL isSrcFileSel = FALSE;

                        ok = SalGeneral->IsFileNameForViewerSelected(Renderer.EnumFilesSourceUID,
                                                                     enumFilesCurrentIndex,
                                                                     fileName, &isSrcFileSel,
                                                                     &srcBusy);
                        if (ok && !isSrcFileSel)
                            ok = FALSE;
                    }
                }
                else
                {
                    if (command == CM_FILE_FIRST)
                    {
                        enumFilesCurrentIndex = -1;
                    }
                    ok = SalGeneral->GetNextFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                              &enumFilesCurrentIndex,
                                                              openedFileName,
                                                              command == CM_FILE_NEXTSELFILE,
                                                              TRUE, fileName,
                                                              &noMoreFiles, &srcBusy);
                    if (ok && (command == CM_FILE_NEXTSELFILE))
                    { // bereme jen selected soubory
                        BOOL isSrcFileSel = FALSE;

                        ok = SalGeneral->IsFileNameForViewerSelected(Renderer.EnumFilesSourceUID,
                                                                     enumFilesCurrentIndex,
                                                                     fileName, &isSrcFileSel,
                                                                     &srcBusy);
                        if (ok && !isSrcFileSel)
                            ok = FALSE;
                    }
                }

                if (ok)
                { // we've got a new name
                    if (!openedFileName || !*openedFileName || _stricmp(fileName, openedFileName))
                    {

                        if (Lock != NULL)
                        {
                            SetEvent(Lock);
                            Lock = NULL; // ted uz je to jen na disk-cache
                        }
                        Renderer.OpenFile(fileName, TRUE);
                    }
                    // index nastavime i v pripade neuspechu, aby user mohl prejit na dalsi/predchozi obrazek
                    Renderer.EnumFilesCurrentIndex = enumFilesCurrentIndex;
                }
            }
            return 0;
        }

        case CM_GOTO:
        {
            Renderer.OnGoto();
            return 0;
        }

        case CM_FIELDS:
        {
            if (!Renderer.Database.IsOpened())
                return 0;
            CColumnsDialog(Renderer.HWindow, &Renderer).Execute();
            return 0;
        }

        case CM_PROPERTIES:
        {
            if (!Renderer.Database.IsOpened())
                return 0;
            CPropertiesDialog(Renderer.HWindow, &Renderer).Execute();
            return 0;
        }

        case CM_CSV_OPTIONS:
        {
            if (lstrcmpi(Renderer.Database.GetParserName(), "csv") != 0)
                return 0;
            CCSVConfig oldCfg = CfgCSV;
            if (CCSVOptionsDialog(Renderer.HWindow, &CfgCSV, &CfgDefaultCSV).Execute() == IDOK)
            {
                if (memcmp(&oldCfg, &CfgCSV, sizeof(CCSVConfig)) != 0)
                {
                    // pokud uzivatel neco zmenil, otevreme soubor znovu
                    Renderer.OnFileReOpen();
                }
            }
            return 0;
        }

        case CM_EXIT:
        {
            DestroyWindow(HWindow);
            return 0;
        }

        case CM_COPY:
        {
            if (!Renderer.Database.IsOpened())
                return 0;
            Renderer.CopySelectionToClipboard();
            UpdateEnablers();
            return 0;
        }

        case CM_SELECT_ALL:
        {
            if (!Renderer.Database.IsOpened())
                return 0;
            Renderer.SelectAll();
            return 0;
        }

        case CM_FIND:
        case CM_FIND_NEXT:
        case CM_FIND_PREV:
        {
            if (!Renderer.Database.IsOpened())
                return 0;
            OnFind(command);
            return 0;
        }

        case CM_TOGGLE_BOOKMARK:
        {
            if (!Renderer.Database.IsOpened())
                return 0;
            Renderer.OnToggleBookmark();
            UpdateEnablers();
            return 0;
        }

        case CM_NEXT_BOOKMARK:
        {
            if (!Renderer.Database.IsOpened())
                return 0;
            Renderer.OnNextBookmark(TRUE);
            return 0;
        }

        case CM_PREV_BOOKMARK:
        {
            if (!Renderer.Database.IsOpened())
                return 0;
            Renderer.OnNextBookmark(FALSE);
            return 0;
        }

        case CM_CLEAR_ALL_BOOKMARKS:
        {
            if (!Renderer.Database.IsOpened())
                return 0;
            Renderer.OnClearBookmarks();
            UpdateEnablers();
            return 0;
        }

        case CM_FULLSCREEN:
        {
            WPARAM wParam2;
            if (IsZoomed(HWindow))
                wParam2 = SC_RESTORE;
            else
                wParam2 = SC_MAXIMIZE;
            SendMessage(HWindow, WM_SYSCOMMAND, wParam2, 0);
            return 0;
        }

        case CM_CODING_RECOGNIZE:
        {
            Renderer.AutoSelect = !Renderer.AutoSelect;
            if (Renderer.AutoSelect)
                Renderer.DefaultCoding[0] = 0;
            return 0;
        }

        case CM_CODING_NEXT:
        case CM_CODING_PREV:
        {
            CGUIMenuPopupAbstract* popup = MainMenu->GetSubMenu(CML_CONVERT, FALSE);
            if ((popup != NULL) && !Renderer.Database.GetIsUnicode())
            {
                int nextIndex = GetNextCodingMenuIndex(Renderer.Coding, command == CM_CODING_NEXT);
                char conversion[220];
                MENU_ITEM_INFO mii;
                mii.Mask = MENU_MASK_STRING;
                mii.String = conversion;
                mii.StringLen = 220;
                if (popup->GetItemInfo(CM_CODING_FIRST + nextIndex, FALSE, &mii))
                    Renderer.SelectConversion(nextIndex == NULL ? NULL : conversion);
            }
            return 0;
        }

        case CM_CODING_DEFAULT:
        {
            CGUIMenuPopupAbstract* popup = MainMenu->GetSubMenu(CML_CONVERT, FALSE);
            if (popup != NULL)
            {
                int index = GetCodingMenuIndex(Renderer.Coding);
                char conversion[220];
                MENU_ITEM_INFO mii;
                mii.Mask = MENU_MASK_STRING;
                mii.String = conversion;
                mii.StringLen = 220;
                if (popup->GetItemInfo(CM_CODING_FIRST + index, FALSE, &mii))
                {
                    strcpy(Renderer.DefaultCoding, index == 0 ? "" : conversion);
                    Renderer.AutoSelect = FALSE;
                }
            }
            return 0;
        }

        case CM_CONFIGURATION:
        {
            OnConfiguration(Renderer.HWindow, FALSE);
            return 0;
        }

        case CM_HELP_CONTENTS:
        case CM_HELP_SEARCH:
        case CM_HELP_INDEX:
        {
            CHtmlHelpCommand command2;
            DWORD_PTR dwData = 0;
            switch (LOWORD(wParam))
            {
            case CM_HELP_CONTENTS:
            {
                SalGeneral->OpenHtmlHelp(HWindow, HHCDisplayContext, IDH_INTRODUCTION, TRUE); // nechceme dva messageboxy za sebou
                command2 = HHCDisplayTOC;
                break;
            }

            case CM_HELP_INDEX:
            {
                command2 = HHCDisplayIndex;
                break;
            }

            case CM_HELP_SEARCH:
            {
                command2 = HHCDisplaySearch;
                break;
            }
            }
            SalGeneral->OpenHtmlHelp(HWindow, command2, dwData, FALSE);
            return 0;
        }

        case CM_ABOUT:
        {
            DBFViewAbout(Renderer.HWindow);
            return 0;
        }
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
