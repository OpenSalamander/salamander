// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#pragma comment(lib, "UxTheme.lib")

// zapina vypis bloku ktere zustaly alokovane na heapu po skonceni pluginu
// #define DUMP_MEM

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro menu
CPluginInterfaceForMenu InterfaceForMenu;

CWindowQueue MainWindowQueue("FileComp Windows"); // seznam vsech oken pluginu
CThreadQueue ThreadQueue("FileComp Windows, Workers, and Remote Control");
CMappedFontFactory MappedFontFactory;
HINSTANCE hNormalizDll = NULL;
TNormalizeString PNormalizeString = NULL;
BOOL AlwaysOnTop = FALSE;

const char* CONFIG_VERSION = "Version";
const char* CONFIG_CONFIGURATION = "Configuration";
const char* CONFIG_COLORS = "Colors";
const char* CONFIG_CUSTOMCOLORS = "Custom Colors";
const char* CONFIG_DEFOPTIONS = "Default Diff Options";
const char* CONFIG_FORCETEXT = "Force Text";
const char* CONFIG_FORCEBINARY = "Force Binary";
const char* CONFIG_IGNORESPACECHANGE = "Ignore Space Change";
const char* CONFIG_IGNOREALLSPACE = "Ignore All Space";
const char* CONFIG_IGNORELINEBREAKSCHG = "Ignore Line Breaks Changes";
const char* CONFIG_IGNORECASE = "Ignore Case";
const char* CONFIG_EOLCONVERSION0 = "EOL Conversion 0";
const char* CONFIG_EOLCONVERSION1 = "EOL Conversion 1";
const char* CONFIG_REBARBANDSLAYOUT = "Rebar Bands Layout";
const char* CONFIG_HISTORY = "History %d";
const char* CONFIG_LASTCFGPAGE = "Last Configuration Page";
const char* CONFIG_LOADONSTART = "Load On Start";
const char* CONFIG_VIEW_HORIZONTAL = "Horizontal View";
const char* CONFIG_AUTO_COPY = "Auto-Copy Selection";
const char* CONFIG_NORMALIZATION_FORM = "Normalization Form";
const char* CONFIG_ENCODING0 = "Encoding 0";
const char* CONFIG_ENCODING1 = "Encoding 1";
const char* CONFIG_ENDIANS0 = "Endians 0";
const char* CONFIG_ENDIANS1 = "Endians 1";
const char* CONFIG_INPUTENC0 = "InputEnc 0";
const char* CONFIG_INPUTENC1 = "InputEnc 1";
const char* CONFIG_INPUTENCTABLE0 = "InputEnc Table 0";
const char* CONFIG_INPUTENCTABLE1 = "InputEnc Table 1";

BOOL LoadOnStart;

#ifdef DUMP_MEM
_CrtMemState ___CrtMemState;
#endif //DUMP_MEM

int WINAPI SalamanderPluginGetReqVer()
{
    CALL_STACK_MESSAGE_NONE
    return LAST_VERSION_OF_SALAMANDER;
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    CALL_STACK_MESSAGE_NONE

#ifdef DUMP_MEM
    _CrtMemCheckpoint(&___CrtMemState);
#endif //DUMP_MEM

    if (!InitLCUtils(salamander, "File Comparator" /* neprekladat! */))
        return NULL;

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    SG->SetHelpFileName("filecomp.chm");

    if (!InitDialogs())
    {
        ReleaseLCUtils();
        return NULL;
    }

    InitXUnicode();
    MappedFontFactory.Init();

    hNormalizDll = LoadLibrary("normaliz.dll");
    if (hNormalizDll)
    {
        PNormalizeString = (TNormalizeString)GetProcAddress(hNormalizDll, "NormalizeString"); // Min: Vista
    }

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_LOADSAVECONFIGURATION | FUNCTION_CONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "File Comparator" /* neprekladat! */);

    salamander->SetPluginHomePageURL("www.altap.cz");

    // musi byt az za salamander->SetBasicPluginData, protoze pri startu threadu se pouziva
    // verze pluginu a tu salamander->SetBasicPluginData meni (padalo to obcas prave diky
    // tomu, ze se verze realokovala, a pak se pouzival dealokovany retezec)
    CRemoteComparator::CreateRemoteComparator();

    return &PluginInterface;
}

// ****************************************************************************
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
    SG->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

void WINAPI
LoadOrSaveConfiguration(BOOL load, HKEY regKey, CSalamanderRegistryAbstract* registry, void* param)
{
    CALL_STACK_MESSAGE2("LoadOrSaveConfiguration(%d, , , )", load);
    if (!load)
        PluginInterface.SaveConfiguration((HWND)param, regKey, registry);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);

    BOOL ret = CRemoteComparator::Terminate(force) || force;
    if (ret)
    {
        ret = MainWindowQueue.Empty();
        if (!ret)
        {
            ret = MainWindowQueue.CloseAllWindows(force) || force;
        }
        if (ret)
        {
            if (!ThreadQueue.KillAll(force) && !force)
                ret = FALSE;
            else
            {
                //SG->CallLoadOrSaveConfiguration(FALSE, LoadOrSaveConfiguration, parent);

                ReleaseDialogs();
                ReleaseLCUtils();
                MappedFontFactory.Free();
                if (hNormalizDll)
                    FreeLibrary(hNormalizDll);

#ifdef DUMP_MEM
                _CrtMemDumpAllObjectsSince(&___CrtMemState);
#endif //DUMP_MEM
            }
        }
    }
    _CrtCheckMemory();
    return ret;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, , )");

    // nastavime defaultni hodnoty

    // configuration
    ::Configuration.ConfirmSelection = TRUE;
    ::Configuration.Context = 2;
    ::Configuration.TabSize = 8;
    ::Configuration.UseViewerFont = TRUE;
    ::Configuration.WhiteSpace = (char)0xB7;
    LoadOnStart = FALSE;

    // switches
    ::Configuration.ViewMode = fvmStandard;
    ::Configuration.ShowWhiteSpace = FALSE;
    ::Configuration.DetailedDifferences = TRUE;
    ::Configuration.HorizontalView = FALSE;
    SG->GetConfigParameter(SALCFG_AUTOCOPYSELTOCLIPBOARD, &::Configuration.AutoCopy,
                           sizeof(::Configuration.AutoCopy), NULL);

    // barvy
    memcpy(Colors, DefaultColors, sizeof(SALCOLOR) * NUMBER_OF_COLORS);
    BandsParams[0].Width = -1;
    memset(CustomColors, 0, 16 * sizeof(COLORREF));

    // default compare options
    DefCompareOptions = DefaultCompareOptions;

    // historie
    CBHistoryEntries = 0;

    // posledni navstivena stranka v konfiguraci
    LastCfgPage = 0;

    if (regKey != NULL) // load z registry
    {
        DWORD configVersion = 0;
        registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &configVersion, sizeof(DWORD));
        if ((configVersion == CURRENT_CONFIG_VERSION) || (configVersion == CURRENT_CONFIG_VERSION_NORECOMPAREBUTTON) || (configVersion == CURRENT_CONFIG_VERSION_PRESEPARATEOPTIONS))
        {
            CRegBLOBConfiguration blob;
            // nacteme konfiguraci z registru
            if (registry->GetValue(regKey, CONFIG_CONFIGURATION, REG_BINARY, &blob, sizeof(blob)))
            { // Configuration as stored in binary form in registry
                ::Configuration.ConfirmSelection = blob.ConfirmSelection;
                ::Configuration.Context = blob.Context;
                ::Configuration.TabSize = blob.TabSize;
                ::Configuration.FileViewLogFont = blob.FileViewLogFont;
                ::Configuration.UseViewerFont = blob.UseViewerFont;
                ::Configuration.WhiteSpace = blob.WhiteSpace;
                ::Configuration.ViewMode = blob.ViewMode;
                ::Configuration.ShowWhiteSpace = blob.ShowWhiteSpace;
                ::Configuration.DetailedDifferences = blob.DetailedDifferences;
            }

            // nacteme barvy
            registry->GetValue(regKey, CONFIG_COLORS, REG_BINARY, Colors, sizeof(SALCOLOR) * NUMBER_OF_COLORS);
            registry->GetValue(regKey, CONFIG_CUSTOMCOLORS, REG_BINARY, CustomColors, 16 * sizeof(COLORREF));
            // default compare options
            if (configVersion == CURRENT_CONFIG_VERSION_PRESEPARATEOPTIONS)
            {
                // import old config
                struct COldOptions
                {
                    int ForceText;
                    int ForceBinary;
                    int IgnoreSpaceChange;
                    int IgnoreAllSpace;
                    int Unused1;
                    int Unused2;
                    int Unused3;
                    int IgnoreCase;
                    char* Unused4[4];
                    char* Unused5[3];
                    unsigned int EolConversion[2];
                } old;
                if (registry->GetValue(regKey, CONFIG_DEFOPTIONS, REG_BINARY, &old, sizeof(COldOptions)))
                {
                    DefCompareOptions.ForceText = old.ForceText;
                    DefCompareOptions.ForceBinary = old.ForceBinary;
                    DefCompareOptions.IgnoreSpaceChange = old.IgnoreSpaceChange;
                    DefCompareOptions.IgnoreAllSpace = old.IgnoreAllSpace;
                    DefCompareOptions.IgnoreCase = old.IgnoreCase;
                    DefCompareOptions.EolConversion[0] = old.EolConversion[0] >> 1;
                    DefCompareOptions.EolConversion[1] = old.EolConversion[1] >> 1;
                }
            }
            else
            {
                _ASSERT(sizeof(int) == 4);
                registry->GetValue(regKey, CONFIG_FORCETEXT, REG_DWORD, &DefCompareOptions.ForceText, 4);
                registry->GetValue(regKey, CONFIG_FORCEBINARY, REG_DWORD, &DefCompareOptions.ForceBinary, 4);
                registry->GetValue(regKey, CONFIG_IGNORESPACECHANGE, REG_DWORD, &DefCompareOptions.IgnoreSpaceChange, 4);
                registry->GetValue(regKey, CONFIG_IGNOREALLSPACE, REG_DWORD, &DefCompareOptions.IgnoreAllSpace, 4);
                registry->GetValue(regKey, CONFIG_IGNORELINEBREAKSCHG, REG_DWORD, &DefCompareOptions.IgnoreLineBreakChanges, 4);
                registry->GetValue(regKey, CONFIG_IGNORECASE, REG_DWORD, &DefCompareOptions.IgnoreCase, 4);
                registry->GetValue(regKey, CONFIG_EOLCONVERSION0, REG_DWORD, &DefCompareOptions.EolConversion[0], 4);
                registry->GetValue(regKey, CONFIG_EOLCONVERSION1, REG_DWORD, &DefCompareOptions.EolConversion[1], 4);
                registry->GetValue(regKey, CONFIG_ENCODING0, REG_DWORD, &DefCompareOptions.Encoding[0], 4);
                registry->GetValue(regKey, CONFIG_ENCODING1, REG_DWORD, &DefCompareOptions.Encoding[1], 4);
                registry->GetValue(regKey, CONFIG_ENDIANS0, REG_DWORD, &DefCompareOptions.Endians[0], 4);
                registry->GetValue(regKey, CONFIG_ENDIANS1, REG_DWORD, &DefCompareOptions.Endians[1], 4);
                registry->GetValue(regKey, CONFIG_INPUTENC0, REG_DWORD, &DefCompareOptions.PerformASCII8InputEnc[0], 4);
                registry->GetValue(regKey, CONFIG_INPUTENC1, REG_DWORD, &DefCompareOptions.PerformASCII8InputEnc[1], 4);
                registry->GetValue(regKey, CONFIG_INPUTENCTABLE0, REG_SZ, DefCompareOptions.ASCII8InputEncTableName[0], 101);
                registry->GetValue(regKey, CONFIG_INPUTENCTABLE1, REG_SZ, DefCompareOptions.ASCII8InputEncTableName[1], 101);
                DWORD dw;
                if (registry->GetValue(regKey, CONFIG_NORMALIZATION_FORM, REG_DWORD, &dw, sizeof(DWORD)))
                    DefCompareOptions.NormalizationForm = dw ? TRUE : FALSE;
            }
            // historie pouzitych souboru
            TCHAR buf[32];
            for (; CBHistoryEntries < MAX_HISTORY_ENTRIES; CBHistoryEntries++)
            {
                _stprintf(buf, CONFIG_HISTORY, CBHistoryEntries);
                if (!registry->GetValue(regKey, buf, REG_SZ, CBHistory[CBHistoryEntries], SizeOf(CBHistory[CBHistoryEntries])))
                    break;
            }
            if (configVersion > CURRENT_CONFIG_VERSION_NORECOMPAREBUTTON)
            {
                // rebar layout: Do not read from config versions prior to 8 because Recompare btn was added in 8
                // and thus the toolbar could be partially covered by Differences
                registry->GetValue(regKey, CONFIG_REBARBANDSLAYOUT, REG_BINARY, BandsParams, sizeof(CBandParams) * 2);
            }
            // posledni navstivena stranka v konfiguraci
            registry->GetValue(regKey, CONFIG_LASTCFGPAGE, REG_DWORD, &LastCfgPage, sizeof(DWORD));
            // load on start flag
            DWORD dw;
            if (registry->GetValue(regKey, CONFIG_LOADONSTART, REG_DWORD, &dw, sizeof(DWORD)))
                LoadOnStart = dw != 0;
            if (registry->GetValue(regKey, CONFIG_VIEW_HORIZONTAL, REG_DWORD, &dw, sizeof(DWORD)))
                ::Configuration.HorizontalView = dw ? TRUE : FALSE;
            if (registry->GetValue(regKey, CONFIG_AUTO_COPY, REG_DWORD, &dw, sizeof(DWORD)))
                ::Configuration.AutoCopy = dw ? TRUE : FALSE;
        }
    }

    SG->GetConfigParameter(SALCFG_VIEWERFONT, &::Configuration.InternalViewerFont, sizeof(LOGFONT), NULL);
    if (::Configuration.UseViewerFont)
    {
        // Used the font of Internal Viewer;
        ::Configuration.FileViewLogFont = ::Configuration.InternalViewerFont;
    }

    UpdateDefaultColors(Colors, Palette);
    // Do not allow normalization if Normaliz.dll not present
    if (!PNormalizeString)
        DefCompareOptions.NormalizationForm = FALSE;
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, , )");

    // verze
    DWORD dw = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &dw, sizeof(DWORD));

    // konfigurace
    CRegBLOBConfiguration blob;
    // Configuration is stored in binary form in registry
    blob.ConfirmSelection = ::Configuration.ConfirmSelection;
    blob.Context = ::Configuration.Context;
    blob.TabSize = ::Configuration.TabSize;
    blob.FileViewLogFont = ::Configuration.FileViewLogFont;
    blob.UseViewerFont = ::Configuration.UseViewerFont;
    blob.WhiteSpace = ::Configuration.WhiteSpace;
    blob.ViewMode = ::Configuration.ViewMode;
    blob.ShowWhiteSpace = ::Configuration.ShowWhiteSpace;
    blob.DetailedDifferences = ::Configuration.DetailedDifferences;
    registry->SetValue(regKey, CONFIG_CONFIGURATION, REG_BINARY, &blob, sizeof(blob));

    // barvy
    registry->SetValue(regKey, CONFIG_COLORS, REG_BINARY, Colors, sizeof(SALCOLOR) * NUMBER_OF_COLORS);
    registry->SetValue(regKey, CONFIG_CUSTOMCOLORS, REG_BINARY, CustomColors, 16 * sizeof(COLORREF));
    // default compare options
    registry->SetValue(regKey, CONFIG_FORCETEXT, REG_DWORD, &DefCompareOptions.ForceText, 4);
    registry->SetValue(regKey, CONFIG_FORCEBINARY, REG_DWORD, &DefCompareOptions.ForceBinary, 4);
    registry->SetValue(regKey, CONFIG_IGNORESPACECHANGE, REG_DWORD, &DefCompareOptions.IgnoreSpaceChange, 4);
    registry->SetValue(regKey, CONFIG_IGNOREALLSPACE, REG_DWORD, &DefCompareOptions.IgnoreAllSpace, 4);
    registry->SetValue(regKey, CONFIG_IGNORELINEBREAKSCHG, REG_DWORD, &DefCompareOptions.IgnoreLineBreakChanges, 4);
    registry->SetValue(regKey, CONFIG_IGNORECASE, REG_DWORD, &DefCompareOptions.IgnoreCase, 4);
    registry->SetValue(regKey, CONFIG_EOLCONVERSION0, REG_DWORD, &DefCompareOptions.EolConversion[0], 4);
    registry->SetValue(regKey, CONFIG_EOLCONVERSION1, REG_DWORD, &DefCompareOptions.EolConversion[1], 4);
    registry->SetValue(regKey, CONFIG_ENCODING0, REG_DWORD, &DefCompareOptions.Encoding[0], 4);
    registry->SetValue(regKey, CONFIG_ENCODING1, REG_DWORD, &DefCompareOptions.Encoding[1], 4);
    registry->SetValue(regKey, CONFIG_ENDIANS0, REG_DWORD, &DefCompareOptions.Endians[0], 4);
    registry->SetValue(regKey, CONFIG_ENDIANS1, REG_DWORD, &DefCompareOptions.Endians[1], 4);
    registry->SetValue(regKey, CONFIG_INPUTENC0, REG_DWORD, &DefCompareOptions.PerformASCII8InputEnc[0], 4);
    registry->SetValue(regKey, CONFIG_INPUTENC1, REG_DWORD, &DefCompareOptions.PerformASCII8InputEnc[1], 4);
    registry->SetValue(regKey, CONFIG_INPUTENCTABLE0, REG_SZ, DefCompareOptions.ASCII8InputEncTableName[0], int(strlen(DefCompareOptions.ASCII8InputEncTableName[0])));
    registry->SetValue(regKey, CONFIG_INPUTENCTABLE1, REG_SZ, DefCompareOptions.ASCII8InputEncTableName[1], int(strlen(DefCompareOptions.ASCII8InputEncTableName[1])));
    dw = DefCompareOptions.NormalizationForm;
    registry->SetValue(regKey, CONFIG_NORMALIZATION_FORM, REG_DWORD, &dw, sizeof(DWORD));
    // historie pouzitych souboru
    BOOL b;
    if (SG->GetConfigParameter(SALCFG_SAVEHISTORY, &b, sizeof(BOOL), NULL) && b)
    {
        char buf[32];
        int i;
        for (i = 0; i < CBHistoryEntries; i++)
        {
            sprintf(buf, CONFIG_HISTORY, i);
            registry->SetValue(regKey, buf, REG_SZ, CBHistory[i], int(strlen(CBHistory[i])));
        }
    }
    else
    {
        //podrizneme historii
        char buf[32];
        int i;
        for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
        {
            sprintf(buf, CONFIG_HISTORY, i);
            registry->DeleteValue(regKey, buf);
        }
    }
    // layout rebary
    registry->SetValue(regKey, CONFIG_REBARBANDSLAYOUT, REG_BINARY, BandsParams, sizeof(CBandParams) * 2);
    // posledni navstivena stranka v konfiguraci
    registry->SetValue(regKey, CONFIG_LASTCFGPAGE, REG_DWORD, &LastCfgPage, sizeof(DWORD));
    dw = LoadOnStart;
    registry->SetValue(regKey, CONFIG_LOADONSTART, REG_DWORD, &dw, sizeof(DWORD));
    SG->SetFlagLoadOnSalamanderStart(LoadOnStart);
    dw = ::Configuration.HorizontalView;
    registry->SetValue(regKey, CONFIG_VIEW_HORIZONTAL, REG_DWORD, &dw, sizeof(DWORD));
    dw = ::Configuration.AutoCopy;
    registry->SetValue(regKey, CONFIG_AUTO_COPY, REG_DWORD, &dw, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");

    DWORD flag;
    CConfigurationDialog dlg(parent, &::Configuration, Colors, &DefCompareOptions, &flag);
    dlg.Execute();
    if (flag)
        MainWindowQueue.BroadcastMessage(WM_USER_CFGCHNG, flag, 0);
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_COMPAREFILES
  {MNTT_PE, 0
};
*/
    salamander->AddMenuItem(-1, LoadStr(IDS_COMPAREFILES), SALHOTKEY('C', HOTKEYF_CONTROL | HOTKEYF_SHIFT), MID_COMPAREFILES, FALSE,
                            MENU_EVENT_DISK, 0, MENU_SKILLLEVEL_ALL);
    // nastavime ikonku pluginu
    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_FILECOMP),
                                      IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR);
    salamander->SetBitmapWithIcons(hBmp);
    DeleteObject(hBmp);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    CALL_STACK_MESSAGE_NONE
    return &InterfaceForMenu;
}

void CPluginInterface::Event(int event, DWORD param) // FIXME_X64 - staci zde jen 32b DWORD?
{
    CALL_STACK_MESSAGE2("CPluginInterface::Event(, 0x%X)", param);
    switch (event)
    {
    case PLUGINEVENT_COLORSCHANGED:
    {
        // mohla se zmenit barva textu
        MainWindowQueue.BroadcastMessage(WM_USER_CFGCHNG, CC_COLORS, 0);
        break;
    }

    case PLUGINEVENT_CONFIGURATIONCHANGED:
    {
        // Cache the value for use in Config dialog
        SG->GetConfigParameter(SALCFG_VIEWERFONT, &::Configuration.InternalViewerFont, sizeof(LOGFONT), NULL);
        // mohl se zmenit font vieweru
        if (::Configuration.UseViewerFont)
        {
            ::Configuration.FileViewLogFont = ::Configuration.InternalViewerFont;
            MainWindowQueue.BroadcastMessage(WM_USER_CFGCHNG, CC_FONT, 0);
        }
        break;
    }
    }
}

void CPluginInterface::ClearHistory(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::ClearHistory()");
    MainWindowQueue.BroadcastMessage(WM_USER_CLEARHISTORY, 0, 0);
    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
        CBHistory[i][0] = 0;
}

// ****************************************************************************
//
// CPluginInterfaceForMenu
//

BOOL CPluginInterfaceForMenu::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                              int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenu::ExecuteMenuItem(, , %d, 0x%X)",
                        id, eventMask);
    switch (id)
    {
    case MID_COMPAREFILES:
    {
        char file1[MAX_PATH];
        char file2[MAX_PATH];
        const CFileData *fd1, *fd2 = NULL;
        int index = 0;
        BOOL isDir;
        BOOL secondFromSource = FALSE;
        int tgtPathType;
        SG->GetPanelPath(PANEL_TARGET, NULL, 0, &tgtPathType, NULL);
        BOOL tgtPanelIsDisk = (tgtPathType == PATH_TYPE_WINDOWS);

        *file1 = 0;
        *file2 = 0;

        fd1 = SG->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);

        if (fd1 && isDir)
            goto SELECTION_FINISHED; // adresare neberem

        if (fd1)
        {
            // mame prvni soubor ze selektu zkusime najit druhy ze source panelu
            fd2 = SG->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);

            if (fd2 && isDir)
                goto SELECTION_FINISHED; // adresare neberem

            if (!fd2)
            {
                // jeden je selektly a druhy berem z fokusu nebo ze
                // selection v target panelu
                index = 0;
                fd2 = SG->GetPanelSelectedItem(PANEL_TARGET, &index, &isDir);
                if (!tgtPanelIsDisk || !fd2 || SG->GetPanelSelectedItem(PANEL_TARGET, &index, &isDir))
                {
                    // v target panelu je 0 nebo vice nez 1 selektlych souboru
                    // berem fokus v source panelu
                    fd2 = SG->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
                }
                else
                    fd2 = NULL;
            }
            else
            {
                // mame dva selektle soubory jeste se podivame, zda neni vybrany treti
                // tri vybrane soubory neberem
                if (SG->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir))
                    goto SELECTION_FINISHED;
            }
        }
        else
        {
            // zadny vybrany soubor, berem tedy fokus
            fd1 = SG->GetPanelFocusedItem(PANEL_SOURCE, &isDir);

            if (fd1 && isDir)
                goto SELECTION_FINISHED; // adresare neberem
        }

        if (fd1 == NULL)
            goto SELECTION_FINISHED; // prazdny panel

        // ulozime si jmeno prvniho souboru
        if (!SG->GetPanelPath(PANEL_SOURCE, file1, MAX_PATH, NULL, NULL))
            return NULL;
        SG->SalPathAppend(file1, fd1->Name, MAX_PATH);

        if (fd2 &&
            !isDir && fd2 != fd1) // pro pripad, ze berem soubor z fokusu
        {
            // ulozime jmeno druheho souboru
            SG->GetPanelPath(PANEL_SOURCE, file2, MAX_PATH, NULL, NULL);
            SG->SalPathAppend(file2, fd2->Name, MAX_PATH);
            secondFromSource = TRUE;
        }
        else
        {
            if (tgtPanelIsDisk)
            {
                // jeste musime vybrat druhy soubor s target panelu
                index = 0;
                fd2 = SG->GetPanelSelectedItem(PANEL_TARGET, &index, &isDir);

                if (!fd2 || isDir || SG->GetPanelSelectedItem(PANEL_TARGET, &index, &isDir))
                {
                    // najdeme soubor se stejnym jmenem jako prvni
                    index = 0;
                    while ((fd2 = SG->GetPanelItem(PANEL_TARGET, &index, &isDir)) != 0)
                    {
                        if (!isDir && SG->StrICmp(fd1->Name, fd2->Name) == 0)
                            break;
                    }
                }

                if (fd2)
                {
                    // ulozime jmeno druheho souboru
                    if (!SG->GetPanelPath(PANEL_TARGET, file2, MAX_PATH, NULL, NULL))
                        return NULL;
                    SG->SalPathAppend(file2, fd2->Name, MAX_PATH);
                }
            }
        }

    SELECTION_FINISHED:

        BOOL doNotSwapNames = secondFromSource || SG->GetSourcePanel() == PANEL_LEFT;
        SG->GetConfigParameter(SALCFG_ALWAYSONTOP, &AlwaysOnTop, sizeof(AlwaysOnTop), NULL);
        CFilecompThread* d = new CFilecompThread(doNotSwapNames ? file1 : file2, doNotSwapNames ? file2 : file1, FALSE, "");
        if (!d)
            return Error((HWND)-1, IDS_LOWMEM);
        if (!d->Create(ThreadQueue))
            delete d;
        SG->SetUserWorkedOnPanelPath(PANEL_SOURCE); // tento prikaz povazujeme za praci s cestou (objevi se v Alt+F12)
        if (!secondFromSource && file2[0])
            SG->SetUserWorkedOnPanelPath(PANEL_TARGET); // pridame jeste target

        return FALSE;
    }
    }
    return FALSE;
}

BOOL WINAPI
CPluginInterfaceForMenu::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case MID_COMPAREFILES:
        helpID = IDH_COMPAREFILES;
        break;
    }
    if (helpID != 0)
        SG->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}

// ****************************************************************************
//
// CFilecompThread
//

unsigned
CFilecompThread::Body()
{
    CALL_STACK_MESSAGE1("CFilecompThread::CThreadBody()");
    BOOL succes = FALSE;
    BOOL dialogBox = TRUE;
    HWND wnd;
    CCompareOptions options = DefCompareOptions;

    if (!*Path1 || !*Path2 || !DontConfirmSelection && Configuration.ConfirmSelection)
    {
        CCompareFilesDialog* dlg = new CCompareFilesDialog(0, Path1, Path2, succes, &options);
        if (!dlg)
        {
            Error(HWND(NULL), IDS_LOWMEM);
            goto LBODYFINAL;
        }
        wnd = dlg->Create();
        if (!wnd)
        {
            TRACE_E("Nepodarilo se vytvorit CompareFilesDialog");
            goto LBODYFINAL;
        }
        SetForegroundWindow(wnd);
    }
    else
    {
        AddToHistory(Path2);
        AddToHistory(Path1);
        goto LLAUNCHFC;
    }

    while (1)
    {
        if (!MainWindowQueue.Add(new CWindowQueueItem(wnd)))
        {
            TRACE_E("Low memory");
            DestroyWindow(wnd);
            break;
        }

        MSG msg;
        while (IsWindow(wnd) && GetMessage(&msg, NULL, 0, 0))
        {
            if (!dialogBox && !TranslateAccelerator(wnd, HAccels, &msg) ||
                dialogBox && !IsDialogMessage(wnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (!dialogBox || !succes)
            break; // ukoncime message loopu

    LLAUNCHFC:

        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        GetWindowPlacement(SG->GetMainWindowHWND(), &wp);
        // GetWindowPlacement cti Taskbar, takze pokud je Taskbar nahore nebo vlevo,
        // jsou hodnoty posunute o jeho rozmery. Provedeme korekci.
        RECT monitorRect;
        RECT workRect;
        SG->MultiMonGetClipRectByRect(&wp.rcNormalPosition, &workRect, &monitorRect);
        OffsetRect(&wp.rcNormalPosition, workRect.left - monitorRect.left,
                   workRect.top - monitorRect.top);

        // pokud je hlavni okno je minimalizovane, nechceme minimalizovany File Comparator
        CMainWindow* win = new CMainWindow(Path1, Path2, &options,
                                           wp.showCmd == SW_SHOWMAXIMIZED ? SW_SHOWMAXIMIZED : SW_SHOW);
        if (!win)
        {
            Error(HWND(NULL), IDS_LOWMEM);
            break;
        }
        wnd = win->CreateEx(AlwaysOnTop ? WS_EX_TOPMOST : 0,
                            MAINWINDOW_CLASSNAME,
                            LoadStr(IDS_PLUGINNAME),
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE | (wp.showCmd == SW_SHOWMAXIMIZED ? WS_MAXIMIZE : 0),
                            wp.rcNormalPosition.left,
                            wp.rcNormalPosition.top,
                            wp.rcNormalPosition.right - wp.rcNormalPosition.left,
                            wp.rcNormalPosition.bottom - wp.rcNormalPosition.top,
                            NULL, // parent
                            LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_MENU)),
                            DLLInstance,
                            win);
        if (!wnd)
        {
            TRACE_E("Nepodarilo se vytvorit MainWindow, GetLastError() = " << GetLastError());
            break;
        }
        dialogBox = FALSE;
    }
LBODYFINAL:
    if (*ReleaseEvent)
    {
        // pustime dal filecomp.exe
        HANDLE event = OpenEvent(EVENT_MODIFY_STATE, FALSE, ReleaseEvent);
        SetEvent(event);
        CloseHandle(event);
    }

    return 0;
}
