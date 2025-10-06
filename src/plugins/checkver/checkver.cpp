// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "checkver.h"
#include "checkver.rh"
#include "checkver.rh2"
#include "lang\lang.rh"

// plugin interface object, its methods are called from Salamander
CPluginInterface PluginInterface;
// other parts of the CPluginInterface interface
CPluginInterfaceForMenuExt InterfaceForMenuExt;

HINSTANCE DLLInstance = NULL; // handle to the SPL - language independent resources
HINSTANCE HLanguage = NULL;   // handle to the SLG - language dependent resources

HWND HMainDialog = NULL;

HWND HConfigurationDialog = NULL;
BOOL ConfigurationChanged = FALSE; // TRUE = the user clicked OK in the configuration dialog (we do not check whether anything actually changed)

BOOL PluginIsReleased = FALSE; // are we inside CPluginInterface::Release?

BOOL LoadedOnSalamanderStart = FALSE;
BOOL LoadedOnSalInstall = FALSE;

BOOL SalSaveCfgOnExit = FALSE;

HANDLE HMessageLoopThread = NULL; // used to verify that the thread has already finished
HANDLE HDownloadThread = NULL;    // used to verify that the thread has already finished

CRITICAL_SECTION MainDialogIDSection;
DWORD MainDialogID = 1;

HANDLE HModulesEnumDone = NULL;

// Salamander's general interface - valid from startup until the plugin is terminated
CSalamanderGeneralAbstract* SalGeneral = NULL;

// variable definition for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// variable definition for "spl_com.h"
int SalamanderVersion = 0;

// running Salamander version text (for example, "2.52 beta 3 (PB 32)")
char SalamanderTextVersion[MAX_PATH];

// ****************************************************************************

char* LoadStr(int resID)
{
    return SalGeneral->LoadStr(HLanguage, resID);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DLLInstance = hinstDLL;
        InitializeCriticalSection(&MainDialogIDSection);
        HModulesEnumDone = CreateEvent(NULL, TRUE, FALSE, NULL); // "non-signaled" state, manual
        LoadedScriptSize = 0;
        ZeroMemory(&LastCheckTime, sizeof(LastCheckTime));             // we have not run any checks yet
        ZeroMemory(&NextOpenOrCheckTime, sizeof(NextOpenOrCheckTime)); // opening the dialog (optionally with a check) should happen at the first load-on-start (ASAP)
        SalamanderTextVersion[0] = 0;

        if (HModulesEnumDone == NULL)
            return FALSE;
    }
    if (fdwReason == DLL_PROCESS_DETACH)
    {
        // if Altap Salamander is blocked from accessing the internet by the firewall and Salamander is closed while checkver is running,
        // TRACE is destroyed before this function is called, which leads to a crash in TRACE_I,
        // therefore TRACE must not be called here
        //TRACE_I("CheckVer DLL_PROCESS_DETACH");
        DeleteCriticalSection(&MainDialogIDSection);
        if (HModulesEnumDone != NULL)
            CloseHandle(HModulesEnumDone);
        SalGeneral = NULL;
        SalamanderDebug = NULL;
    }

    return TRUE; // DLL can be loaded
}

//****************************************************************************
//
// SalamanderPluginGetReqVer
//

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

//****************************************************************************
//
// SalamanderPluginEntry
//

void WINAPI
LoadOrSaveConfigurationCallback(BOOL load, HKEY regKey, CSalamanderRegistryAbstract* registry, void* param)
{
    OnSaveTimeStamp(regKey, registry);
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // set SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // this plugin is intended for the current version of Salamander and newer - perform a check
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // reject older versions
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "Check Version" /* do not translate! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // load the language module (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Check Version" /* do not translate! */);
    if (HLanguage == NULL)
        return NULL;

    // get Salamander's general interface
    SalGeneral = salamander->GetSalamanderGeneral();

    // set the help file name
    SalGeneral->SetHelpFileName("checkver.chm");

    // set the basic plugin information
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "CHECKVER");

    salamander->SetPluginHomePageURL("www.altap.cz");

    // load-on-start
    SalGeneral->SetFlagLoadOnSalamanderStart(TRUE);

    DWORD loadInfo = salamander->GetLoadInformation();

    // immediately after Salamander is installed, perform the version check with an open window so that
    // it is visible what is happening and the user understands why internet access must be allowed
    // in the personal firewall
    if (loadInfo & LOADINFO_NEWSALAMANDERVER)
        LoadedOnSalInstall = TRUE;

    // if the plugin was loaded on our request (LOADINFO_LOADONSTART),
    if (loadInfo & LOADINFO_LOADONSTART)
        LoadedOnSalamanderStart = TRUE;

    // obtain the Salamander version
    int index = 0;
    char salModule[MAX_PATH];
    SalGeneral->EnumInstalledModules(&index, salModule, SalamanderTextVersion);

    // find out whether the user disabled saving the configuration on exit, in that case
    // the behaviour must differ in several places
    SalGeneral->GetConfigParameter(SALCFG_SAVEONEXIT, &SalSaveCfgOnExit, sizeof(BOOL), NULL);

    return &PluginInterface;
}

//****************************************************************************
//
// CPluginInterface
//

void CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    BOOL ret = TRUE; // we can close

    PluginIsReleased = TRUE;

    ShowMinNA_IfNotShownYet(HMainDialog, TRUE, FALSE);

    // we need to write to the registry regardless of whether the user wants it or not...
    SalGeneral->CallLoadOrSaveConfiguration(FALSE, LoadOrSaveConfigurationCallback, NULL);

    if (!force)
    {
        if (HDownloadThread == NULL && (HMainDialog != NULL || HConfigurationDialog != NULL))
        {
            // if any windows are open, ask the user whether we should close them
            ret = SalGeneral->SalMessageBox(parent, LoadStr(IDS_OPENED_WINDOWS),
                                            LoadStr(IDS_PLUGINNAME),
                                            MB_YESNO | MB_ICONQUESTION) == IDYES;
        }
    }

    if (HDownloadThread != NULL)
    {
        // the download thread is running right now - should we let it finish on its own?
        if (!force && SalGeneral->SalMessageBox(parent, LoadStr(IDS_ABORT_DOWNLOAD),
                                                LoadStr(IDS_PLUGINNAME),
                                                MB_ICONQUESTION | MB_YESNO) == IDNO)
        {
            ret = FALSE; // the user does not want to shut it down
        }
        else
        {
            if (HDownloadThread != NULL)
            {
                IncMainDialogID(); // detach the running session - it will not send anything else and
                                   // will finish as soon as possible
                CloseHandle(HDownloadThread);
                HDownloadThread = NULL;
                ModulesCleanup();
                MainEnableControls(FALSE);
                ClearLogWindow();
                AddLogLine(LoadStr(IDS_INET_ABORTED), TRUE);
            }
        }
    }

    if (ret)
    {
        // the user wants us to close the windows
        if (HConfigurationDialog != NULL)
            SendMessage(HConfigurationDialog, WM_COMMAND, IDCANCEL, 0);
        if (HMainDialog != NULL)
            SendMessage(HMainDialog, WM_COMMAND, IDCANCEL, 0);

        // wait until the thread of our main window terminates
        while (HMainDialog != NULL)
            Sleep(100);

        // ensure the thread finishes
        if (HMessageLoopThread != NULL)
        {
            WaitForSingleObject(HMessageLoopThread, INFINITE); // Petr: I do not see any reason not to wait; it should not cause issues
            CloseHandle(HMessageLoopThread);
            HMessageLoopThread = NULL;
        }

        DestroyFilters();
        UnregisterLogClass();
    }
    PluginIsReleased = FALSE;
    return ret;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    LoadConfig(regKey, registry);
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");
    SaveConfig(regKey, registry);
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
    OnConfiguration(parent);
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    /* used by the export_mnu.py script, which generates salmenu.mnu for the Translator
   keep synchronized with the salamander->AddMenuItem() call below...
MENU_TEMPLATE_ITEM PluginMenu[] =
{
        {MNTT_PB, 0
        {MNTT_IT, IDS_CHECK_FOR_NEW_VER
        {MNTT_PE, 0
};
*/
    salamander->AddMenuItem(-1, LoadStr(IDS_CHECK_FOR_NEW_VER), 0, CM_CHECK_VERSION, FALSE,
                            MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

    // set the plugin icon
    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_CHECKVER),
                                      IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR);
    salamander->SetBitmapWithIcons(hBmp);
    DeleteObject(hBmp);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);
}

void CPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
}

CPluginInterfaceForArchiverAbstract*
CPluginInterface::GetInterfaceForArchiver()
{
    return NULL;
}

CPluginInterfaceForViewerAbstract*
CPluginInterface::GetInterfaceForViewer()
{
    return NULL;
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    return &InterfaceForMenuExt;
}

CPluginInterfaceForFSAbstract*
CPluginInterface::GetInterfaceForFS()
{
    return NULL;
}

CPluginInterfaceForThumbLoaderAbstract*
CPluginInterface::GetInterfaceForThumbLoader()
{
    return NULL;
}

// ****************************************************************************
// SEKCE MessageLoop
// ****************************************************************************

unsigned WINAPI ThreadMessageLoopBody(void* param)
{
    CALL_STACK_MESSAGE1("ThreadMessageLoopBody");
    SetThreadNameInVCAndTrace("CheckVerLoop");
    TRACE_I("Begin");

    CTVData* data = (CTVData*)param;

    BOOL dataAutoOpen = data->AutoOpen;
    RegisterLogClass(); // the log window class will be used in the dialog
    HWND foregroundWnd = GetForegroundWindow();
    HMainDialog = CreateDialogParam(HLanguage, MAKEINTRESOURCE(IDD_MAIN), NULL, MainDlgProc, (LPARAM)data);
    if (HMainDialog != NULL)
    {
        if (data->AutoOpen && Data.AutoConnect)
            SetForegroundWindow(foregroundWnd); // the original foreground window is apparently deactivated by CreateDialogParam itself, so reactivate the original foreground window here manually
        else
        {
            ShowWindow(HMainDialog, SW_SHOW);
            SetForegroundWindow(HMainDialog);
        }
    }

    data->Success = HMainDialog != NULL;
    SetEvent(data->Continue); // let the main thread continue; from this point on the data are invalid (=NULL)
    data = NULL;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_KEYDOWN)
        {
            if (HMainDialog != NULL && GetActiveWindow() == HMainDialog)
            {
                if (msg.wParam == VK_UP || msg.wParam == VK_DOWN ||
                    msg.wParam == VK_HOME || msg.wParam == VK_END ||
                    msg.wParam == VK_NEXT || msg.wParam == VK_PRIOR)
                {
                    SendMessage(HMainDialog, WM_USER_KEYDOWN, msg.wParam, msg.lParam);
                    continue;
                }
            }
        }
        CALL_STACK_MESSAGE5("MSG(0x%p, 0x%X, 0x%IX, 0x%IX)", msg.hwnd, msg.message, msg.wParam, msg.lParam);
        if (HMainDialog == NULL || !IsWindow(HMainDialog) || !IsDialogMessage(HMainDialog, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    IncMainDialogID(); // this window will cease to exist for the download thread
    DestroyWindow(HMainDialog);
    HMainDialog = NULL;

    // request the plugin to unload - it is no longer needed; exception: after a configuration change when saving the configuration is disabled
    // we allow the user to save the configuration manually, because if we unloaded the plugin,
    // the configuration changes would be discarded immediately and the user would not understand why their data are not saved after manual
    // "save configuration"
    if (dataAutoOpen && (!ConfigurationChanged || SalSaveCfgOnExit))
        SalGeneral->PostUnloadThisPlugin();

    TRACE_I("End");
    return 0;
}

DWORD WINAPI ThreadMessageLoop(void* param)
{
    return SalamanderDebug->CallWithCallStack(ThreadMessageLoopBody, param);
}

BOOL OnOpenCheckVersionDialog(HWND hParent, BOOL autoOpen, BOOL firstLoadAfterInstall)
{
    // if the window already exists, simply bring it to the front
    if (HMainDialog != NULL)
    {
        ShowMinNA_IfNotShownYet(HMainDialog, FALSE, FALSE);
        if (autoOpen)
            TRACE_E("This should not happen");
        if (IsIconic(HMainDialog))
            ShowWindow(HMainDialog, SW_RESTORE);
        SetForegroundWindow(HMainDialog);
        return TRUE;
    }

    CTVData data;

    data.Success = FALSE;
    data.AutoOpen = autoOpen;
    data.FirstLoadAfterInstall = firstLoadAfterInstall;
    data.Continue = CreateEvent(NULL, FALSE, FALSE, NULL);
    data.AlwaysOnTop = FALSE;
    SalGeneral->GetConfigParameter(SALCFG_ALWAYSONTOP, &data.AlwaysOnTop, sizeof(data.AlwaysOnTop), NULL);

    if (data.Continue == NULL)
    {
        TRACE_E("Unable to create Continue event.");
        return FALSE;
    }

    if (HMessageLoopThread != NULL)
    {
        WaitForSingleObject(HMessageLoopThread, INFINITE); // Petr: I do not see any reason not to wait; it should not cause issues
        CloseHandle(HMessageLoopThread);
        HMessageLoopThread = NULL;
    }

    DWORD threadID;
    HMessageLoopThread = CreateThread(NULL, 0, ThreadMessageLoop, &data, CREATE_SUSPENDED, &threadID);
    if (HMessageLoopThread == NULL)
    {
        TRACE_E("Unable to create Check Version message loop thread.");
        CloseHandle(data.Continue);
        return FALSE;
    }
    else
    {
        SalamanderDebug->TraceAttachThread(HMessageLoopThread, threadID);
        ResumeThread(HMessageLoopThread);

        // wait until the thread processes the provided data and returns the results
        WaitForSingleObject(data.Continue, INFINITE);
        CloseHandle(data.Continue);

        return data.Success;
    }
}

// ****************************************************************************
// SEKCE MENU
// ****************************************************************************

DWORD
CPluginInterfaceForMenuExt::GetMenuItemState(int id, DWORD eventMask)
{
    TRACE_E("Unexpected call to CPluginInterfaceForMenuExt::GetMenuItemState()");
    return 0;
}

BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander,
                                                 HWND parent, int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
    switch (id)
    {
    case CM_CHECK_VERSION:
    {
        // triggered from Salamander's menu
        OnOpenCheckVersionDialog(parent, FALSE, FALSE);
        break;
    }

    case CM_AUTOCHECK_VERSION:
    {
        // triggered by the plugin during its automatic load
        OnOpenCheckVersionDialog(parent, TRUE, FALSE);
        break;
    }

    case CM_FIRSTCHECK_VERSION:
    {
        // triggered by the plugin during the first load after Salamander installation
        OnOpenCheckVersionDialog(parent, TRUE, TRUE);
        break;
    }

    case CM_ENUMMODULES:
    {
        // enumeration can only be done from the main thread - get to it
        EnumSalModules();
        // the dialog thread is waiting for us - let it continue
        SetEvent(HModulesEnumDone);
        break;
    }
    }
    return FALSE;
}

BOOL WINAPI
CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case CM_CHECK_VERSION:
        helpID = IDH_CHECKVER;
        break;
    }
    if (helpID != 0)
        SalGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}
