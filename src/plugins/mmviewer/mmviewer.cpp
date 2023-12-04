// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mmviewer.rh"
#include "mmviewer.rh2"
#include "lang\lang.rh"
#include "output.h"
#include "renderer.h"
#include "mmviewer.h"
#include "auxtools.h"
#include "dialogs.h"
#include "buffer.h"
#include "parser.h"

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro viewer
CPluginInterfaceForViewer InterfaceForViewer;
CPluginInterfaceForMenuExt InterfaceForMenuExt;

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

HFONT HNormalFont = NULL;
HFONT HBoldFont = NULL;
int FontHeight = -1;

CWindowQueue ViewerWindowQueue("MMViewer Viewers"); // seznam vsech oken viewru
CThreadQueue ThreadQueue("MMViewer Viewers");       // seznam vsech threadu oken

const char* CONFIG_VERSION = "Version";
const char* CONFIG_LOGFONT = "LogFont";
const char* CONFIG_SAVEPOS = "SavePosition";
const char* CONFIG_WNDPLACEMENT = "WindowPlacement";
const char* CONFIG_AUTOSELECT = "Auto Select";
const char* CONFIG_DEFAULT_CODING = "Default Coding";
const char* CONFIG_FINDHISTORY = "Find History";

const char* PLUGIN_NAME = "MMVIEWER"; // jmeno pluginu potrebne pro WinLib

// Configuration variables

// CURRENT_CONFIG_VERSION: 0 - default,
//                         1 - po 2.5b6 major upgrade
#define CURRENT_CONFIG_VERSION 1
int ConfigVersion = 0;

LOGFONT CfgLogFont;
BOOL CfgSavePosition = FALSE;
WINDOWPLACEMENT CfgWindowPlacement;

#define IDX_TB_TERMINATOR -2
#define IDX_TB_SEPARATOR -1
#define IDX_TB_PROPERTIES 0
#define IDX_TB_COPY 1
#define IDX_TB_FIND 2
#define IDX_TB_FULLSCREEN 3
#define IDX_TB_OPEN 4
#define IDX_TB_PREV 5
#define IDX_TB_NEXT 6
#define IDX_TB_EXP_HTML 7
//#define IDX_TB_EXP_XML     8
#define IDX_TB_COUNT 8

MENU_TEMPLATE_ITEM MenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        // Files
        {MNTT_PB, IDS_MENU_FILES, MNTS_B | MNTS_I | MNTS_A, CML_FILES, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILES_OPEN, MNTS_B | MNTS_I | MNTS_A, CM_FILES_OPEN, IDX_TB_OPEN, 0, NULL},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILES_EXPORT_HTML, MNTS_B | MNTS_I | MNTS_A, CM_FILES_EXPORT_HTML, IDX_TB_EXP_HTML, 0, (DWORD*)vweFileOpened},
        //{MNTT_IT,    IDS_MENU_FILES_EXPORT_XML,    MNTS_B|MNTS_I|MNTS_A, CM_FILES_EXPORT_XML,        IDX_TB_EXP_XML,             0,              (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_PB, IDS_MENU_FILE_OTHER, MNTS_B | MNTS_I | MNTS_A, CML_OTHERFILES, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_PREV, MNTS_B | MNTS_I | MNTS_A, CM_FILE_PREV, IDX_TB_PREV, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_NEXT, MNTS_B | MNTS_I | MNTS_A, CM_FILE_NEXT, IDX_TB_NEXT, 0, NULL},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_FIRST, MNTS_B | MNTS_I | MNTS_A, CM_FILE_FIRST, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_LAST, MNTS_B | MNTS_I | MNTS_A, CM_FILE_LAST, -1, 0, NULL},
        {MNTT_PE},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        //    {MNTT_IT,    IDS_MENU_FILES_PROPERTIES,    MNTS_B|MNTS_I|MNTS_A, CM_PROPERTIES,              IDX_TB_PROPERTIES,       0,                 (DWORD*)vweDBOpened},
        //    {MNTT_SP,    -1,                           MNTS_B|MNTS_I|MNTS_A, 0,                          -1,                      0,                 NULL},
        {MNTT_IT, IDS_MENU_FILES_EXIT, MNTS_B | MNTS_I | MNTS_A, CM_EXIT, -1, 0, NULL},
        {MNTT_PE},

        // Edit
        {MNTT_PB, IDS_MENU_EDIT, MNTS_B | MNTS_I | MNTS_A, CML_EDIT, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_COPY, MNTS_B | MNTS_I | MNTS_A, CM_COPY, IDX_TB_COPY, 0, (DWORD*)vweFileOpened},
        {MNTT_PE},

        // View
        {MNTT_PB, IDS_MENU_VIEW, MNTS_B | MNTS_I | MNTS_A, CML_VIEW, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_FULLSCREEN, MNTS_B | MNTS_I | MNTS_A, CM_FULLSCREEN, IDX_TB_FULLSCREEN, 0, NULL},
        {MNTT_PE},

        // Options
        //   {MNTT_PB,     IDS_MENU_OPTIONS,             MNTS_B|MNTS_I|MNTS_A, CML_OPTIONS,                -1,                      0,                 NULL},
        //    {MNTT_IT,    IDS_MENU_OPTIONS_CONFIG,      MNTS_B|MNTS_I|MNTS_A, CM_CONFIGURATION,           -1,                      0,                 NULL},
        //   {MNTT_PE},

        // Help
        {MNTT_PB, IDS_MENU_HELP, MNTS_B | MNTS_I | MNTS_A, CML_HELP, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_HELP_ABOUT, MNTS_B | MNTS_I | MNTS_A, CM_ABOUT, -1, 0, NULL},
        {MNTT_PE},

        {MNTT_PE}, // terminator
};

MENU_TEMPLATE_ITEM PopupMenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_COPY, MNTS_B | MNTS_I | MNTS_A, CM_COPY, IDX_TB_COPY, 0, (DWORD*)vweFileOpened},
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
        {IDX_TB_OPEN, IDS_TBTT_OPEN, CM_FILES_OPEN, vweAlwaysEnabled},
        {IDX_TB_COPY, IDS_TBTT_COPY, CM_COPY, vweFileOpened},
        {IDX_TB_EXP_HTML, IDS_TBTT_EXP_HTML, CM_FILES_EXPORT_HTML, vweFileOpened},
        //{IDX_TB_EXP_XML,     IDS_TBTT_EXP_XML,     CM_FILES_EXPORT_XML,         vweFileOpened},
        //  {IDX_TB_PROPERTIES,  IDS_TBTT_PROPERTIES,  CM_PROPERTIES,   vweDBOpened},
        {IDX_TB_FULLSCREEN, IDS_TBTT_FULLSCREEN, CM_FULLSCREEN, vweAlwaysEnabled},
        {IDX_TB_SEPARATOR},
        {IDX_TB_PREV, IDS_TBTT_PREV, CM_FILE_PREV, vweAlwaysEnabled},
        {IDX_TB_NEXT, IDS_TBTT_NEXT, CM_FILE_NEXT, vweAlwaysEnabled},
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

BOOL GDIInit()
{
    HFONT hTmpFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    LOGFONT lf;
    GetObject(hTmpFont, sizeof(lf), &lf);
    HNormalFont = CreateFontIndirect(&lf);
    lf.lfWeight = FW_BOLD;
    HBoldFont = CreateFontIndirect(&lf);

    HDC hdc = GetDC(NULL);
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    FontHeight = tm.tmHeight;
    ReleaseDC(NULL, hdc);

    return TRUE;
}

BOOL GDIRelease()
{
    if (HNormalFont != NULL)
    {
        DeleteObject(HNormalFont);
        HNormalFont = NULL;
    }

    if (HBoldFont != NULL)
    {
        DeleteObject(HBoldFont);
        HBoldFont = NULL;
    }

    return TRUE;
}

BOOL InitViewer()
{
    HAccel = LoadAccelerators(DLLInstance, MAKEINTRESOURCE(IDA_ACCELERATORS));

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    GetObject(hFont, sizeof(CfgLogFont), &CfgLogFont);

    GDIInit();

    CfgSavePosition = FALSE;
    CfgWindowPlacement.length = 0;

    if (!InitializeWinLib(PLUGIN_NAME, DLLInstance))
        return FALSE;
    SetWinLibStrings("Invalid number!", PLUGIN_NAME);
    return TRUE;
}

void ReleaseViewer()
{
    ReleaseWinLib(DLLInstance);
    GDIRelease();
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

    HANDLES_CAN_USE_TRACE();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "Multimedia Viewer" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Multimedia Viewer" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalGeneral = salamander->GetSalamanderGeneral();

    // ziskame rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
    SalamanderGUI = salamander->GetSalamanderGUI();

    // nastavime jmeno souboru s helpem
    SalGeneral->SetHelpFileName("mmviewer.chm");

    if (!InitViewer())
        return NULL; // chyba

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGIN_NAME),
                                   FUNCTION_LOADSAVECONFIGURATION | FUNCTION_VIEWER /*|FUNCTION_CONFIGURATION*/,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   // pozor, dlouhy string, at nam nevytece v okne Plugins Manager
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "MMVIEWER");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

//
// ****************************************************************************
// CPluginInterface
//

void MMViewerAbout(HWND parent)
{
    CAboutDialog dlg(parent);
    dlg.Execute();
}

void CPluginInterface::About(HWND parent)
{
    MMViewerAbout(parent);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    BOOL ret = ViewerWindowQueue.Empty();
    if (!ret && (force || SalGeneral->SalMessageBox(parent, LoadStr(IDS_OPENED_WINDOWS),
                                                    LoadStr(IDS_PLUGIN_NAME),
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
        for (i = 0; i < maxCount; i++)
        {
            _itoa(i + 1, buf, 10);
            DWORD bufferSize;
            if (registry->GetSize(historyKey, buf, REG_SZ, bufferSize))
            {
                history[i] = (char*)malloc(bufferSize);
                if (history[i] == NULL)
                {
                    TRACE_E("Low memory");
                    break;
                }
                if (!registry->GetValue(historyKey, buf, REG_SZ, history[i], bufferSize))
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
        registry->GetValue(regKey, CONFIG_LOGFONT, REG_BINARY, &CfgLogFont, sizeof(LOGFONT));
        registry->GetValue(regKey, CONFIG_SAVEPOS, REG_DWORD, &CfgSavePosition, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_WNDPLACEMENT, REG_BINARY, &CfgWindowPlacement, sizeof(WINDOWPLACEMENT));
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
    registry->SetValue(regKey, CONFIG_LOGFONT, REG_BINARY, &CfgLogFont, sizeof(LOGFONT));
    registry->SetValue(regKey, CONFIG_SAVEPOS, REG_DWORD, &CfgSavePosition, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_WNDPLACEMENT, REG_BINARY, &CfgWindowPlacement, sizeof(WINDOWPLACEMENT));
}

void OnConfiguration(HWND hParent)
{
    static BOOL InConfiguration = FALSE;
    if (InConfiguration)
    {
        SalGeneral->SalMessageBox(hParent,
                                  LoadStr(IDS_CFG_CONFLICT), LoadStr(IDS_PLUGIN_NAME),
                                  MB_ICONINFORMATION | MB_OK);
        return;
    }
    InConfiguration = TRUE;
    /*
  if (CConfigurationDialog(hParent).Execute() == IDOK)
  {
    ViewerWindowQueue.BroadcastMessage(WM_USER_VIEWERCFGCHNG, 0, 0);
  }
  */
    InConfiguration = FALSE;
}

void CPluginInterface::Configuration(HWND parent)
{
    OnConfiguration(parent);
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    return &InterfaceForMenuExt;
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");
    salamander->AddViewer(""

#ifdef _MP4_SUPPORT_
                          "*.aac;*.mp4;*.m4a;*.m4b"
#endif

#ifdef _MPG_SUPPORT_
                          "*.mp3;*.mp2"
#endif

#ifdef _WAV_SUPPORT_
                          ";*.wav;*.wave"
#endif

#ifdef _WMA_SUPPORT_
                          ";*.wma"
#endif

#ifdef _VQF_SUPPORT_
                          ";*.vqf"
#endif

#ifdef _OGG_SUPPORT_
                          ";*.ogg"
#endif

#ifdef _MOD_SUPPORT_
                          ";*.it;*.s3m;*.stm;*.xm;*.mod;*.mtm;*.669"
#endif

                          ,
                          FALSE); // default (install pluginu), jinak Salam ignoruje

    if (ConfigVersion < 1) // pripony pridane po 2.5b6
    {
        salamander->AddViewer("*.wav;*.wave;*.wma;*.ogg", TRUE);
    }

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_EXPORTHTML
  {MNTT_PE, 0
};
*/
    salamander->AddMenuItem(-1, LoadStr(IDS_EXPORTHTML), 0, MENUCMD_HTMLEXPORT, TRUE, 0, 0, MENU_SKILLLEVEL_ALL);

    HBITMAP hBmp = (HBITMAP)HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_TOOLBARMAIN), IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR));
    salamander->SetBitmapWithIcons(hBmp);
    HANDLES(DeleteObject(hBmp));
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);
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
    int EnumFilesSourceUID;    // UID zdroje pro enumeraci souboru ve vieweru
    int EnumFilesCurrentIndex; // index prvniho souboru ve vieweru ve zdroji
};

unsigned WINAPI ViewerThreadBody(void* param)
{
    CALL_STACK_MESSAGE1("ViewerThreadBody()");
    SetThreadNameInVCAndTrace(LoadStr(IDS_PLUGIN_NAME));
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
                                 LoadStr(IDS_PLUGIN_NAME),
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
                                              IMAGE_ICON, 16, 16, SalGeneral->GetIconLRFlags()));
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
        window->Renderer.OpenFile(name);

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
    data.Continue = CreateEvent(NULL, FALSE, FALSE, NULL);
    data.EnumFilesSourceUID = enumFilesSourceUID;
    data.EnumFilesCurrentIndex = enumFilesCurrentIndex;
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

CViewerWindow::CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex) : CWindow(ooStatic), Renderer(enumFilesSourceUID, enumFilesCurrentIndex)
{
    Renderer.Viewer = this;
    Lock = NULL;
    HRebar = NULL;
    MainMenu = NULL;
    MenuBar = NULL;
    ToolBar = NULL;
    HGrayToolBarImageList = NULL;
    HHotToolBarImageList = NULL;
    ZeroMemory(Enablers, sizeof(Enablers));
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
    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_IMAGEINDEX | TLBI_MASK_ID | TLBI_MASK_ENABLER | TLBI_MASK_STYLE;

    ToolBar->SetImageList(HGrayToolBarImageList);
    ToolBar->SetHotImageList(HHotToolBarImageList);

    int i;
    for (i = 0; ToolBarButtons[i].ImageIndex != IDX_TB_TERMINATOR; i++)
    {
        if (ToolBarButtons[i].ImageIndex == IDX_TB_SEPARATOR)
        {
            tii.Style = TLBI_STYLE_SEPARATOR;
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
    Enablers[vweFileOpened] = Renderer.FileName[0] != 0;
    if (ToolBar != NULL)
        ToolBar->UpdateItemsState();
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
                          WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                          0,
                          0,
                          0,
                          0,
                          HWindow,
                          NULL,
                          DLLInstance,
                          &Renderer);

        MenuBar->CreateWnd(HRebar);
        InsertMenuBand();
        ToolBar->CreateWnd(HRebar);
        FillToolBar();
        InsertToolBarBand();

        ViewerWindowQueue.Add(new CWindowQueueItem(HWindow));
        break;
    }

    case WM_ERASEBKGND:
    {
        return 1;
    }

    case WM_COMMAND:
    case WM_SYSKEYDOWN:
        //    case WM_SYSKEYUP:
        //    case WM_KEYDOWN:
        //    case WM_KEYUP:
        {
            // Misto Post je treba volat Send, protoze dojde-li k aktivaci tohoto
            // okna, chodi sem WM_CONTEXTMENU pri Shift+F10, misto aby bylo zachyceno
            // v Renderer okne. Dochazi pak k aktivaci system menu tohoto okna.
            if (Renderer.HWindow != NULL)
                SendMessage(Renderer.HWindow, uMsg, wParam, lParam);
            break;
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
        // Renderer.RebuildGraphics();
        // Renderer.SetupScrollBars();
        InvalidateRect(Renderer.HWindow, NULL, TRUE);
        return 0;
    }

    case WM_USER_SETTINGCHANGE:
    {
        if (MenuBar != NULL)
            MenuBar->SetFont();
        if (ToolBar != NULL)
            ToolBar->SetFont();
        return 0;
    }

    case WM_USER_CLEARHISTORY:
    {
        /*
      if (FindDialog.HWindow != NULL)
        PostMessage(FindDialog.HWindow, uMsg, wParam, lParam);
      */
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

    case WM_USER_TBGETTOOLTIP:
    {
        TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
        lstrcpy(tt->Buffer, LoadStr(ToolBarButtons[tt->Index].ToolTipResID));
        SalamanderGUI->PrepareToolTipText(tt->Buffer, FALSE);
        return TRUE;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

DWORD
CPluginInterfaceForMenuExt::GetMenuItemState(int id, DWORD eventMask)
{
    if (id != MENUCMD_HTMLEXPORT || !vweFileOpened)
        return 0;

    if (eventMask & (MENU_EVENT_DISK |
                     MENU_EVENT_FILES_SELECTED |
                     MENU_EVENT_DIRS_SELECTED |
                     MENU_EVENT_FILE_FOCUSED |
                     MENU_EVENT_DIR_FOCUSED))
        return MENU_ITEM_STATE_ENABLED;

    return 0;
}

struct SMMVOperationFromDiskData
{
    CQuadWord* RetSize;
    HWND Parent;
    BOOL Success;
    COutput* Output;
    int TotalFiles, ProcessedFiles;
};

void WINAPI MMVOperationFromDisk(const char* sourcePath, SalEnumSelection2 next, void* nextParam, void* param)
{
    SMMVOperationFromDiskData* data = (SMMVOperationFromDiskData*)param;

    data->Success = TRUE; // zatim hlasime uspech operace

    BOOL isDir;
    const char* name;
    const char* dosName;
    CQuadWord size;
    DWORD attr;
    FILETIME lastWrite;
    CQuadWord totalSize(0, 0);
    int errorOccured;

    data->ProcessedFiles = 0;
    data->TotalFiles = 0;

    while ((name = next(data->Parent, 1, &dosName, &isDir, &size, &attr, &lastWrite, nextParam, &errorOccured)) != NULL)
    {
        //if (errorOccured == SALENUM_ERROR); // sem SALENUM_CANCEL prijit nemuze
        //  data->Success = FALSE; // Petr: skipnute soubory si myslim nebrani dalsimu processingu (uzivatel o nich vi)

        if (!isDir)
        {
            data->TotalFiles++;

            CParserInterface* parser;
            char pathname[1024];
            strcpy(pathname, sourcePath);
            SalGeneral->SalPathAppend(pathname, name, sizeof(pathname));
            CParserResultEnum result = CreateAppropriateParser(pathname, &parser);
            if (result == preOK)
            {
                data->Output->AddHeader(name, TRUE);
                data->Output->AddSeparator();

                parser->GetFileInfo(data->Output);

                parser->CloseFile();
                delete (parser);

                data->ProcessedFiles++;
            }
        }
    }

    if (errorOccured == SALENUM_CANCEL || // Petr: skipnute soubory si myslim nebrani dalsimu processingu (uzivatel o nich vi), ovsem pokud dal uzivatel Cancel, operaci zrusime
        data->ProcessedFiles == 0)
    {
        data->Success = FALSE;
    }
}

BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                                 int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::ExecuteMenuItem(, , %d, 0x%X)", id, eventMask);

    if (id != MENUCMD_HTMLEXPORT)
        return FALSE;

    int ret = FALSE;

    COutput output;
    char fname[MAX_PATH];
    fname[0] = '\0';
    char* s = LoadStr(IDS_HTMLEXPFILTER);
    if (GetOpenFileName(parent, NULL, s, fname, LoadStr(IDS_HTMLEXT), TRUE))
    {
        CQuadWord size = CQuadWord(-1, -1); // error
        SMMVOperationFromDiskData data;
        data.RetSize = &size;
        data.Parent = parent;
        data.Success = FALSE;
        data.Output = &output;
        data.TotalFiles = 0;
        data.ProcessedFiles = 0;

        SalGeneral->CreateSafeWaitWindow(LoadStr(IDS_PROCESSINGFILES), LoadStr(IDS_PLUGIN_NAME), 500, NULL, NULL);

        SalGeneral->CallPluginOperationFromDisk(PANEL_SOURCE, MMVOperationFromDisk, &data);

        if (data.Success)
        {
            int r = ExportToHTML(fname, output);

            SalGeneral->DestroySafeWaitWindow();

            if (r > 0)
            {
                ret = TRUE;

                SalGeneral->SalMessageBox(parent, FStr(LoadStr(IDS_FILESPROCESSED), data.ProcessedFiles, data.TotalFiles),
                                          LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONINFORMATION);

                if (SalGeneral->SalMessageBox(parent, LoadStr(IDS_EXPORTOPEN), LoadStr(IDS_PLUGIN_NAME), MB_YESNO | MB_ICONQUESTION) == DIALOG_YES)
                    ExecuteFile(fname);
            }
            else
            {
                switch (r)
                {
                case -1:
                case -2:
                default: //prozatim vsechny chyby write error
                    SalGeneral->SalMessageBox(parent, LoadStr(IDS_MMV_WRITE_ERROR), LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONEXCLAMATION);
                    break;
                }
            }
        }
        else
        {
            SalGeneral->DestroySafeWaitWindow();
            SalGeneral->SalMessageBox(parent, FStr(LoadStr(IDS_FILESPROCESSED), data.ProcessedFiles, data.TotalFiles),
                                      LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONSTOP);
        }
    }

    return ret;
}

BOOL WINAPI
CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case MENUCMD_HTMLEXPORT:
        helpID = IDH_EXPORT;
        break;
    }
    if (helpID != 0)
        SalGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}

static char msgbuf[1024];
char* FStr(const char* format, ...)
{
    va_list arglist;

    va_start(arglist, format);
    vsprintf(msgbuf, format, arglist);
    va_end(arglist);
    return (msgbuf);
}

char* my_strrev(char* string)
{
    char* start = string;
    char* left = string;
    char ch;

    while (*string++) /* find end of string */
        ;
    string -= 2;

    while (left < string)
    {
        ch = *left;
        *left++ = *string;
        *string-- = ch;
    }

    return (start);
}

void FormatSize2(__int64 size, char* str_size, BOOL nozero = FALSE)
{
    if (!size && nozero)
    {
        if (str_size)
            str_size[0] = '\0';
        return;
    }

    char s[128];

    _i64toa(size, s, 10);

    int j = 0;
    int len = (int)strlen(s) - 1;
    int i = len;
    int n = 0;
    while (i > -1)
    {
        n++;

        str_size[j++] = s[i];
        if ((n % 3 == 0) && (i != len) && (i != 0))
            str_size[j++] = ' ';

        i--;
    }
    str_size[j] = '\0';
    my_strrev(str_size);
}

bool IsUTF8Text(const char* s)
{
    int nUTF8 = 0, cnt = (int)strlen(s);

    while (cnt-- > 0)
    {
        if (*s & 0x80)
        {
            if ((*s & 0xe0) == 0xc0)
            {
                if (!s[1])
                {
                    if (cnt)
                    { // incomplete 2-byte sequence
                        nUTF8 = 0;
                    }
                    break;
                }
                else if ((s[1] & 0xc0) != 0x80)
                {
                    nUTF8 = 0;
                    break; // not in UCS2
                }
                else
                {
                    nUTF8++;
                    s += 2;
                    cnt--;
                }
            }
            else if ((*s & 0xf0) == 0xe0)
            {
                if (!s[1] || !s[2])
                {
                    if (cnt > 1)
                    { // incomplete 3-byte sequence
                        nUTF8 = 0;
                    }
                    break;
                }
                else if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80)
                {
                    nUTF8 = 0;
                    break; // not in UCS2
                }
                else
                {
                    nUTF8++;
                    s += 3;
                    cnt -= 2;
                }
            }
            else
            {
                nUTF8 = 0;
                break; // not in UCS2
            }
        }
        else
        {
            s++;
        }
    }

    if (nUTF8 > 0)
    {
        // At least one 2-or-more-bytes UTF8 sequence found and no invalid sequences found
        return true;
    }
    return false;
}

//inteligentni dynamicky konverter - zabranuje zbytecne realokaci
char* AnsiToUTF8(const char* chars, int len)
{
    static TBuffer<char> tmpbuf;
    static TBuffer<wchar_t> tmpbufw;

    if (len == 0)
    {
        static char emptyBuff[] = "";
        return emptyBuff;
    }

    tmpbufw.Reserve(len + 1);

    wchar_t* wc = tmpbufw.Get();
    wc[MultiByteToWideChar(CP_ACP, 0, chars, len, wc, len)] = 0;
    int wcl = (int)wcslen(wc);

    tmpbuf.Reserve((wcl + 1) * 4);

    char* c = tmpbuf.Get();

    if (((wcl = WideCharToMultiByte(CP_UTF8, 0, wc, wcl, c, wcl * 3, 0, NULL)) == 0))
    {
        tmpbuf.Reserve(len + 1);
        strcpy(tmpbuf.Get(), chars);
    }
    else
        c[wcl] = 0;

    return tmpbuf.Get();
}

//inteligentni dynamicky konverter - zabranuje zbytecne realokaci
char* UTF8ToAnsi(const char* chars, int len)
{
    static TBuffer<char> tmpbuf;
    static TBuffer<wchar_t> tmpbufw;

    if (len == 0)
    {
        static char emptyBuff[] = "";
        return emptyBuff;
    }

    tmpbufw.Reserve(len + 1);

    wchar_t* wc = tmpbufw.Get();
    wc[MultiByteToWideChar(CP_UTF8, 0, chars, len, wc, len)] = 0;
    int wcl = (int)wcslen(wc);

    tmpbuf.Reserve((wcl + 1) * 4);

    char* c = tmpbuf.Get();

    if (((wcl = WideCharToMultiByte(CP_ACP, 0, wc, wcl, c, wcl * 3, 0, NULL)) == 0))
    {
        tmpbuf.Reserve(len + 1);
        strcpy(tmpbuf.Get(), chars);
    }
    else
        c[wcl] = 0;

    return tmpbuf.Get();
}

//inteligentni dynamicky konverter - zabranuje zbytecne realokaci
// Actually called only from 1 place in wmaparser.cpp
char* WideToAnsi(const wchar_t* chars, int len)
{
    static TBuffer<char> tmpbuf;

    if (len == 0)
    {
        static char emptyBuff[] = "";
        return emptyBuff;
    }

    int outlen = WideCharToMultiByte(CP_UTF8, 0, chars, len, NULL, 0, 0, NULL);
    tmpbuf.Reserve(outlen + 1);

    char* c = tmpbuf.Get();

    c[WideCharToMultiByte(CP_UTF8, 0, chars, len, c, outlen, 0, NULL)] = 0;

    return c;
}

//inteligentni dynamicky konverter - zabranuje zbytecne realokaci
wchar_t* AnsiToWide(const char* chars, int len)
{
    static TBuffer<wchar_t> tmpbufw;

    if (len == 0)
    {
        static wchar_t emptyBuff[] = L"";
        return emptyBuff;
    }

    tmpbufw.Reserve(len + 1);

    wchar_t* wc = tmpbufw.Get();

    // NOTE: Must stay CP_ACP, used for file name conversion
    wc[MultiByteToWideChar(CP_ACP, 0, chars, len, wc, len)] = 0;

    return wc;
}

//inteligentni dynamicky konverter - zabranuje zbytecne realokaci
wchar_t* UTF8ToWide(const char* chars, int len)
{
    static TBuffer<wchar_t> tmpbufw;

    if (len == 0)
    {
        static wchar_t emptyBuff[] = L"";
        return emptyBuff;
    }

    tmpbufw.Reserve(len + 1);

    wchar_t* wc = tmpbufw.Get();

    wc[MultiByteToWideChar(CP_UTF8, 0, chars, len, wc, len)] = 0;

    return wc;
}

void ExecuteFile(LPCTSTR fname)
{
    if (int((INT_PTR)ShellExecute(::GetDesktopWindow(), _T("open"), fname, _T(""), _T("."), SW_SHOW)) <= 32)
    {
        // dej na vyber, s cim to asociovat
        ShellExecute(NULL,
                     _T("open"),
                     _T("rundll32.exe"),
                     FStr("shell32.dll, OpenAs_RunDLL %s", fname),
                     NULL,
                     SW_SHOW);
    }
}

BOOL GetOpenFileName(HWND parent, const char* title, char* filter, char* buffer, const char* ext, BOOL save)
{
    CALL_STACK_MESSAGE4("GetOpenFileName(, %s, %s, , %d)", title, filter, save);
    OPENFILENAME ofn;

    char fileName[MAX_PATH];

    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter;
    while (*filter != 0) // vytvoreni double-null terminated listu
    {
        if (*filter == '|')
            *filter = 0;
        filter++;
    }

    DWORD attr = SalGeneral->SalGetFileAttributes(buffer);
    if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        fileName[0] = 0;
        ofn.lpstrInitialDir = buffer;
    }
    else
        strcpy(fileName, buffer);
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = ext + 1;
    //ofn.lpfnHook = OFNHookProc;
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR /*| OFN_ENABLEHOOK*/;
    ofn.nFilterIndex = 1;

    BOOL ret;
    if (save)
    {
        ofn.Flags |= OFN_OVERWRITEPROMPT;
        ret = SalGeneral->SafeGetSaveFileName(&ofn);
    }
    else
    {
        ofn.Flags |= OFN_FILEMUSTEXIST;
        ret = SalGeneral->SafeGetOpenFileName(&ofn);
    }

    if (ret)
        strcpy(buffer, fileName);

    return ret;
}
