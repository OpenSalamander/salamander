// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "checkver.h"
#include "checkver.rh"
#include "checkver.rh2"
#include "lang\lang.rh"

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// dalsi casti interfacu CPluginInterface
CPluginInterfaceForMenuExt InterfaceForMenuExt;

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

HWND HMainDialog = NULL;

HWND HConfigurationDialog = NULL;
BOOL ConfigurationChanged = FALSE; // TRUE = user dal OK v konfiguracnim dialogu (jestli neco skutecne zmenil uz nepitvame)

BOOL PluginIsReleased = FALSE; // jsme v metode CPluginInterface::Release?

BOOL LoadedOnSalamanderStart = FALSE;
BOOL LoadedOnSalInstall = FALSE;

BOOL SalSaveCfgOnExit = FALSE;

HANDLE HMessageLoopThread = NULL; // slouzi pro kontrolu, ze thread uz dobehnul
HANDLE HDownloadThread = NULL;    // slouzi pro kontrolu, ze thread uz dobehnul

CRITICAL_SECTION MainDialogIDSection;
DWORD MainDialogID = 1;

HANDLE HModulesEnumDone = NULL;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalGeneral = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

// text cisla verze beziciho Salamandera (napr. "2.52 beta 3 (PB 32)")
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
        ZeroMemory(&LastCheckTime, sizeof(LastCheckTime));             // jeste jsme neudelali zadnou kontrolu
        ZeroMemory(&NextOpenOrCheckTime, sizeof(NextOpenOrCheckTime)); // otevreni dialogu prip. s kontrolou se ma udelat pri prvnim load-on-startu (ASAP)
        SalamanderTextVersion[0] = 0;

        if (HModulesEnumDone == NULL)
            return FALSE;
    }
    if (fdwReason == DLL_PROCESS_DETACH)
    {
        // pokud nepustim skrz firewall AS na net a zavru Salama za behu checkver,
        // dojde k destrukci TRACE pred volanim teto funkce - a naslednemu padu v TRACE_I
        // proto zde TRACE jiz nesmime volat
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
                   "Check Version" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Check Version" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalGeneral = salamander->GetSalamanderGeneral();

    // nastavime jmeno souboru s helpem
    SalGeneral->SetHelpFileName("checkver.chm");

    // nastavime zakladni informace o pluginu
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

    // tesne po instaci Salamandera provedeme kontrolu verze s otevrenym oknem, aby
    // bylo videt, co se deje a user pochopil, proc ma otevrit pristup na internet
    // v personalnim firewallu
    if (loadInfo & LOADINFO_NEWSALAMANDERVER)
        LoadedOnSalInstall = TRUE;

    // pokud byl plugin nacten na nasi zadost (LOADINFO_LOADONSTART),
    if (loadInfo & LOADINFO_LOADONSTART)
        LoadedOnSalamanderStart = TRUE;

    // ziskame verzi Salamandera
    int index = 0;
    char salModule[MAX_PATH];
    SalGeneral->EnumInstalledModules(&index, salModule, SalamanderTextVersion);

    // zjistime, jestli nema uzivatel vyple ukladani konfigurace pri exitu, v tom pripade
    // je totiz potreba se chovat na nekolika mistech odlisne
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
    BOOL ret = TRUE; // muzeme se zavrit

    PluginIsReleased = TRUE;

    ShowMinNA_IfNotShownYet(HMainDialog, TRUE, FALSE);

    // potrebujeme zapsat do registry nezavisle na tom, jestli si to user preje nebo ne...
    SalGeneral->CallLoadOrSaveConfiguration(FALSE, LoadOrSaveConfigurationCallback, NULL);

    if (!force)
    {
        if (HDownloadThread == NULL && (HMainDialog != NULL || HConfigurationDialog != NULL))
        {
            // pokud jsou otevrena nejaka okna, zeptame se uzivatele, jestli je mame zavrit
            ret = SalGeneral->SalMessageBox(parent, LoadStr(IDS_OPENED_WINDOWS),
                                            LoadStr(IDS_PLUGINNAME),
                                            MB_YESNO | MB_ICONQUESTION) == IDYES;
        }
    }

    if (HDownloadThread != NULL)
    {
        // prave jede download thread - mame ho nechat vyhnit?
        if (!force && SalGeneral->SalMessageBox(parent, LoadStr(IDS_ABORT_DOWNLOAD),
                                                LoadStr(IDS_PLUGINNAME),
                                                MB_ICONQUESTION | MB_YESNO) == IDNO)
        {
            ret = FALSE; // user to nechce zabalit
        }
        else
        {
            if (HDownloadThread != NULL)
            {
                IncMainDialogID(); // odpojime bezici session - uz nam nic neposle a
                                   // hned jak to bude mozne, skonci
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
        // uzivatel po nas chce, abychom zavreli okna
        if (HConfigurationDialog != NULL)
            SendMessage(HConfigurationDialog, WM_COMMAND, IDCANCEL, 0);
        if (HMainDialog != NULL)
            SendMessage(HMainDialog, WM_COMMAND, IDCANCEL, 0);

        // pockame az se ukonci thread naseho hlavniho okna
        while (HMainDialog != NULL)
            Sleep(100);

        // zajistime dobehnuti threadu
        if (HMessageLoopThread != NULL)
        {
            WaitForSingleObject(HMessageLoopThread, INFINITE); // Petr: nejak nevidim duvod proc nepockat, nemelo by se hlodat
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

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_CHECK_FOR_NEW_VER
	{MNTT_PE, 0
};
*/
    salamander->AddMenuItem(-1, LoadStr(IDS_CHECK_FOR_NEW_VER), 0, CM_CHECK_VERSION, FALSE,
                            MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

    // nastavime ikonku pluginu
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
    RegisterLogClass(); // trida log okna bude pouzita v dialogu;
    HWND foregroundWnd = GetForegroundWindow();
    HMainDialog = CreateDialogParam(HLanguage, MAKEINTRESOURCE(IDD_MAIN), NULL, MainDlgProc, (LPARAM)data);
    if (HMainDialog != NULL)
    {
        if (data->AutoOpen && Data.AutoConnect)
            SetForegroundWindow(foregroundWnd); // puvodni foreground okno deaktivuje zrejme uz jen samotny CreateDialogParam, takze aktivujeme zpet puvodne foregroundove okno zde rucne
        else
        {
            ShowWindow(HMainDialog, SW_SHOW);
            SetForegroundWindow(HMainDialog);
        }
    }

    data->Success = HMainDialog != NULL;
    SetEvent(data->Continue); // pustime dale hl. thread, od tohoto bodu nejsou data platna (=NULL)
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

    IncMainDialogID(); // toto okna prestane existovat pro download thread
    DestroyWindow(HMainDialog);
    HMainDialog = NULL;

    // nechame plugin unloadnout - uz nebude potreba; vyjimka: po zmene konfigurace pri vyplem
    // ukladani konfigurace (dame userovi moznost si konfiguraci ulozit rucne, pokud totiz plugin
    // unloadneme, zmeny konfigurace se okamzite zahodi a uzivatel nepochopi, proc po rucnim
    // "save configuration" nejsou jeho data ulozena)
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
    // pokud uz okno existuje, pouze ho vytahneme na povrch
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
        WaitForSingleObject(HMessageLoopThread, INFINITE); // Petr: nejak nevidim duvod proc nepockat, nemelo by se hlodat
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

        // pockame, az thread zpracuje predana data a vrati vysledky
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
        // vyvolano z menu Salamandera
        OnOpenCheckVersionDialog(parent, FALSE, FALSE);
        break;
    }

    case CM_AUTOCHECK_VERSION:
    {
        // vyvolano pluginem pri automatickem loadu pluginu
        OnOpenCheckVersionDialog(parent, TRUE, FALSE);
        break;
    }

    case CM_FIRSTCHECK_VERSION:
    {
        // vyvolano pluginem pri prvnim loadu pluginu po instalaci Salamandera
        OnOpenCheckVersionDialog(parent, TRUE, TRUE);
        break;
    }

    case CM_ENUMMODULES:
    {
        // enumeraci muzeme provest pouze z hlavniho threadu - jdem na vec
        EnumSalModules();
        // ceka na nas thread dialogu - nechame ho rozjet
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
