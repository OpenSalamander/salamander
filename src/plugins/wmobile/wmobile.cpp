// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// dalsi casti interfacu CPluginInterface
CPluginInterfaceForFS InterfaceForFS;

// ConfigVersion: 0 - zadna konfigurace se z Registry nenacetla (jde o instalaci pluginu nebo verzi bez konfigurace - do/vcetne 2.5 beta 7),
//                1 - prvni verze konfigurace (s 2.5 beta 8; duvod zavedeni: automaticke vypinani polozky v menu Alt+F1/F2 pokud neni instalovane rapi.dll)

int ConfigVersion = 0;           // verze nactene konfigurace z registry (popis verzi viz vyse)
#define CURRENT_CONFIG_VERSION 1 // aktualni verze konfigurace (uklada se do registry pri unloadu pluginu)
const char* CONFIG_VERSION = "Version";

// globalni data

// ukazatele na tabulky mapovani na mala/velka pismena
unsigned char* LowerCase = NULL;
unsigned char* UpperCase = NULL;

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
CSalamanderGUIAbstract* SalamanderGUI = NULL;

char TitleWMobile[100] = "Windows Mobile Plugin";                  // v entry pointu se nahradi IDS_WMPLUGINTITLE
char TitleWMobileError[100] = "Windows Mobile Plugin Error";       // v entry pointu se nahradi IDS_WMPLUGINTITLE_ERROR
char TitleWMobileQuestion[100] = "Windows Mobile Plugin Question"; // v entry pointu se nahradi IDS_WMPLUGINTITLE_QUESTION

// ****************************************************************************

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DLLInstance = hinstDLL;

        INITCOMMONCONTROLSEX initCtrls;
        initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
        initCtrls.dwICC = ICC_BAR_CLASSES;
        if (!InitCommonControlsEx(&initCtrls))
        {
            MessageBox(NULL, "InitCommonControlsEx failed!", "Error", MB_OK | MB_ICONERROR);
            return FALSE; // DLL won't start
        }
    }

    return TRUE; // DLL can be loaded
}

void OnAbout(HWND hParent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(hParent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

//
// ****************************************************************************
// SalamanderPluginGetReqVer
//

#ifdef __BORLANDC__
extern "C"
{
    int WINAPI SalamanderPluginGetReqVer();
    CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander);
};
#endif // __BORLANDC__

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

//
// ****************************************************************************
// SalamanderPluginEntry
//

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

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
                   "Windows Mobile Plugin" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Windows Mobile Plugin" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderGeneral->GetLowerAndUpperCase(&LowerCase, &UpperCase);

    strncpy_s(TitleWMobile, LoadStr(IDS_WMPLUGINTITLE), _TRUNCATE);
    strncpy_s(TitleWMobileError, LoadStr(IDS_WMPLUGINTITLE_ERROR), _TRUNCATE);
    strncpy_s(TitleWMobileQuestion, LoadStr(IDS_WMPLUGINTITLE_QUESTION), _TRUNCATE);

    // ziskame rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
    SalamanderGUI = salamander->GetSalamanderGUI();

    // nastavime jmeno souboru s helpem
    SalamanderGeneral->SetHelpFileName("wmobile.chm");

    if (!InitializeWinLib("WMOBILE" /* neprekladat! */, DLLInstance))
        return FALSE;
    SetupWinLibHelp(HTMLHelpCallback);

    if (!InitFS())
        return NULL; // chyba

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_FILESYSTEM | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "WMOBILE" /* neprekladat! */, NULL, "CE");

    salamander->SetPluginHomePageURL("www.altap.cz");

    // ziskame nase FS-name (nemusi byt "cefs", Salamander ho muze upravit)
    SalamanderGeneral->GetPluginFSName(AssignedFSName, 0);

    return &PluginInterface;
}

//
// ****************************************************************************
// CPluginInterface
//

void WINAPI
CPluginInterface::About(HWND parent)
{
    OnAbout(parent);
}

BOOL WINAPI
CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    BOOL ret = TRUE;

    if (ret)
    {
        ReleaseFS();

        ReleaseWinLib(DLLInstance);

        // zrusime vsechny kopie souboru z FS v disk-cache (teoreticky zbytecne, kazdy FS po sobe kopie rusi)
        char uniqueFileName[MAX_PATH];
        strcpy(uniqueFileName, AssignedFSName);
        strcat(uniqueFileName, ":");
        // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
        // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
        SalamanderGeneral->ToLowerCase(uniqueFileName);
        SalamanderGeneral->RemoveFilesFromCache(uniqueFileName);
    }
    if (ret && InterfaceForFS.GetActiveFSCount() != 0)
    {
        TRACE_E("Some FS interfaces were not closed (count=" << InterfaceForFS.GetActiveFSCount() << ")");
    }
    return ret;
}

void WINAPI
CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");

    if (regKey != NULL) // load z registry
    {
        registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD));
    }
}

void WINAPI
CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
}

void WINAPI
CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
}

void WINAPI
CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // prechod na ikonky s podporou alfa kanalu
    CGUIIconListAbstract* iconList = SalamanderGUI->CreateIconList();
    iconList->Create(16, 16, 1);
    HICON hIcon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_FS), IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags());
    iconList->ReplaceIcon(0, hIcon);
    DestroyIcon(hIcon);
    salamander->SetIconListForGUI(iconList); // o destrukci iconlistu se postara Salamander
    salamander->SetChangeDriveMenuItem("\tMobile Device", 0);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);

    if (ConfigVersion < 1) // delame jen pri instalaci pluginu nebo prechodu z verze 2.5 beta 7 nebo starsi (abysme nemenili nastaveni usera)
    {
        // pokud neni nainstalovane rapi, tak schovame ikonu, aby neprekazela
        HINSTANCE hLib = LoadLibrary("rapi.dll");
        if (hLib != NULL)
            FreeLibrary(hLib);
        else
            SalamanderGeneral->SetChangeDriveMenuItemVisibility(FALSE);
    }
}

void WINAPI
CPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
    //JR v Windows Mobile pluginu nepouzivam zadany data interface
}

void WINAPI
CPluginInterface::ClearHistory(HWND parent)
{
}

CPluginInterfaceForFSAbstract* WINAPI
CPluginInterface::GetInterfaceForFS()
{
    return &InterfaceForFS;
}
