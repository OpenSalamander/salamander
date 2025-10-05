// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#pragma comment(lib, "UxTheme.lib")

// enables dumping blocks that remain allocated on the heap after the plugin finishes
// #define DUMP_MEM

// plugin interface object whose methods are invoked by Salamander
CPluginInterface PluginInterface;
// portion of CPluginInterface dedicated to menus
CPluginInterfaceForMenu InterfaceForMenu;

CWindowQueue MainWindowQueue("FileComp Windows"); // list of all plugin windows
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

    if (!InitLCUtils(salamander, "File Comparator" /* do not translate! */))
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

    // set basic metadata about the plugin
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_LOADSAVECONFIGURATION | FUNCTION_CONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "File Comparator" /* do not translate! */);

    salamander->SetPluginHomePageURL("www.altap.cz");

    // must be after salamander->SetBasicPluginData because worker threads use the plugin
    // version at startup and salamander->SetBasicPluginData updates that value (it used to
    // crash occasionally when the version string was reallocated and the freed buffer was
    // still referenced)
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

    // initialize default values

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

    // colors
    memcpy(Colors, DefaultColors, sizeof(SALCOLOR) * NUMBER_OF_COLORS);
    BandsParams[0].Width = -1;
    memset(CustomColors, 0, 16 * sizeof(COLORREF));

    // default compare options
    DefCompareOptions = DefaultCompareOptions;

    // history
    CBHistoryEntries = 0;

    // last configuration page that was opened
    LastCfgPage = 0;

    if (regKey != NULL) // load from the registry
    {
        DWORD configVersion = 0;
        registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &configVersion, sizeof(DWORD));
        if ((configVersion == CURRENT_CONFIG_VERSION) || (configVersion == CURRENT_CONFIG_VERSION_NORECOMPAREBUTTON) || (configVersion == CURRENT_CONFIG_VERSION_PRESEPARATEOPTIONS))
        {
            CRegBLOBConfiguration blob;
            // load configuration from the registry
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

            // load colors
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
            // history of recently used files
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
            // last visited page in the configuration dialog
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

    // version information
    DWORD dw = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &dw, sizeof(DWORD));

    // configuration block
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

    // colors
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
    // history of recently used files
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
        // trim the history list
        char buf[32];
        int i;
        for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
        {
            sprintf(buf, CONFIG_HISTORY, i);
            registry->DeleteValue(regKey, buf);
        }
    }
    // rebar layout
    registry->SetValue(regKey, CONFIG_REBARBANDSLAYOUT, REG_BINARY, BandsParams, sizeof(CBandParams) * 2);
    // last configuration page that was opened
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

    /* used by the export_mnu.py script that generates salmenu.mnu for the Translator
   keep synchronized with the salamander->AddMenuItem() calls below...
MENU_TEMPLATE_ITEM PluginMenu[] =
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_COMPAREFILES
  {MNTT_PE, 0
};
*/
    salamander->AddMenuItem(-1, LoadStr(IDS_COMPAREFILES), SALHOTKEY('C', HOTKEYF_CONTROL | HOTKEYF_SHIFT), MID_COMPAREFILES, FALSE,
                            MENU_EVENT_DISK, 0, MENU_SKILLLEVEL_ALL);
    // assign the plugin icon
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

void CPluginInterface::Event(int event, DWORD param) // FIXME_X64 - is a 32-bit DWORD sufficient here?
{
    CALL_STACK_MESSAGE2("CPluginInterface::Event(, 0x%X)", param);
    switch (event)
    {
    case PLUGINEVENT_COLORSCHANGED:
    {
        // the text color may have changed
        MainWindowQueue.BroadcastMessage(WM_USER_CFGCHNG, CC_COLORS, 0);
        break;
    }

    case PLUGINEVENT_CONFIGURATIONCHANGED:
    {
        // Cache the value for use in Config dialog
        SG->GetConfigParameter(SALCFG_VIEWERFONT, &::Configuration.InternalViewerFont, sizeof(LOGFONT), NULL);
        // the viewer font may have changed
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
            goto SELECTION_FINISHED; // ignore directories

        if (fd1)
        {
            // we have the first selected file; try to find a second one in the source panel
            fd2 = SG->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);

            if (fd2 && isDir)
                goto SELECTION_FINISHED; // ignore directories

            if (!fd2)
            {
                // one item is selected and we take the other either from focus
                // or from the selection in the target panel
                index = 0;
                fd2 = SG->GetPanelSelectedItem(PANEL_TARGET, &index, &isDir);
                if (!tgtPanelIsDisk || !fd2 || SG->GetPanelSelectedItem(PANEL_TARGET, &index, &isDir))
                {
                    // the target panel contains zero or more than one selected file
                    // so fall back to the focused item in the source panel
                    fd2 = SG->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
                }
                else
                    fd2 = NULL;
            }
            else
            {
                // we have two selected files; ensure a third one is not selected
                // three selected files are ignored
                if (SG->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir))
                    goto SELECTION_FINISHED;
            }
        }
        else
        {
            // no file was selected; use the focused item instead
            fd1 = SG->GetPanelFocusedItem(PANEL_SOURCE, &isDir);

            if (fd1 && isDir)
                goto SELECTION_FINISHED; // ignore directories
        }

        if (fd1 == NULL)
            goto SELECTION_FINISHED; // empty panel

        // store the name of the first file
        if (!SG->GetPanelPath(PANEL_SOURCE, file1, MAX_PATH, NULL, NULL))
            return NULL;
        SG->SalPathAppend(file1, fd1->Name, MAX_PATH);

        if (fd2 &&
            !isDir && fd2 != fd1) // in case we take the file from the focus
        {
            // store the name of the second file
            SG->GetPanelPath(PANEL_SOURCE, file2, MAX_PATH, NULL, NULL);
            SG->SalPathAppend(file2, fd2->Name, MAX_PATH);
            secondFromSource = TRUE;
        }
        else
        {
            if (tgtPanelIsDisk)
            {
                // we still need to pick the second file from the target panel
                index = 0;
                fd2 = SG->GetPanelSelectedItem(PANEL_TARGET, &index, &isDir);

                if (!fd2 || isDir || SG->GetPanelSelectedItem(PANEL_TARGET, &index, &isDir))
                {
                    // find the file with the same name as the first one
                    index = 0;
                    while ((fd2 = SG->GetPanelItem(PANEL_TARGET, &index, &isDir)) != 0)
                    {
                        if (!isDir && SG->StrICmp(fd1->Name, fd2->Name) == 0)
                            break;
                    }
                }

                if (fd2)
                {
                    // store the name of the second file
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
        SG->SetUserWorkedOnPanelPath(PANEL_SOURCE); // treat this command as working with the path (appears in Alt+F12)
        if (!secondFromSource && file2[0])
            SG->SetUserWorkedOnPanelPath(PANEL_TARGET); // also record the target panel

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
            break; // leave the message loop

    LLAUNCHFC:

        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        GetWindowPlacement(SG->GetMainWindowHWND(), &wp);
        // GetWindowPlacement respects the taskbar, so if the taskbar is at the top or left
        // the coordinates are offset by its size. Apply a correction.
        RECT monitorRect;
        RECT workRect;
        SG->MultiMonGetClipRectByRect(&wp.rcNormalPosition, &workRect, &monitorRect);
        OffsetRect(&wp.rcNormalPosition, workRect.left - monitorRect.left,
                   workRect.top - monitorRect.top);

        // if the main window is minimized, keep the File Comparator restored instead
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
        // allow filecomp.exe to continue
        HANDLE event = OpenEvent(EVENT_MODIFY_STATE, FALSE, ReleaseEvent);
        SetEvent(event);
        CloseHandle(event);
    }

    return 0;
}
