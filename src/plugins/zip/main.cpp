// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <tchar.h>
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
#include <stdio.h>

#include "versinfo.rh2"

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "spl_vers.h"
#include "dbg.h"

#include "array2.h"

#include "selfextr/comdefs.h"
#include "config.h"
#include "typecons.h"
#include "chicon.h"
#include "common.h"
#include "list.h"
#include "extract.h"
#include "add_del.h"
#include "zipdll.h"
#include "dialogs.h"
#include "main.h"
#include "zip.rh"
#include "zip.rh2"
#include "lang\lang.rh"

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// dalsi casti interfacu CPluginInterface
CPluginInterfaceForArchiver InterfaceForArchiver;
CPluginInterfaceForMenuExt InterfaceForMenuExt;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// interface pro komfortni praci se soubory
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;

// Interface for AES encryption
CSalamanderCryptAbstract* SalamanderCrypt = NULL;

// Interface for BZIP2 de/compression
CSalamanderBZIP2Abstract* SalamanderBZIP2;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
CSalamanderGUIAbstract* SalamanderGUI = NULL;

// definice stringu do configurace
const char* CONFIG_LEVEL = "Level";
const char* CONFIG_ENCRYPTMETHOD = "Encryption Method";
const char* CONFIG_NOEMPTYDIRS = "No Empty Dirs";
const char* CONFIG_BACKUPZIP = "Backup ZIP";
const char* CONFIG_SHOWEXOPT = "Show Extended Options";
const char* CONFIG_TIMETONEWESTFILE = "Time To Newest File";
const char* CONFIG_VOLSIZECACHE = "Volume Size %d";
const char* CONFIG_VOLSIZECACHE1 = "Volume Size 1";
const char* CONFIG_VOLSIZECACHE2 = "Volume Size 2";
const char* CONFIG_VOLSIZECACHE3 = "Volume Size 3";
const char* CONFIG_VOLSIZECACHE4 = "Volume Size 4";
const char* CONFIG_VOLSIZECACHE5 = "Volume Size 5";
const char* CONFIG_VOLSIZEUNITS = "Volume Size Units %d";
const char* CONFIG_VOLSIZEUNITS1 = "Volume Size Units 1";
const char* CONFIG_VOLSIZEUNITS2 = "Volume Size Units 2";
const char* CONFIG_VOLSIZEUNITS3 = "Volume Size Units 3";
const char* CONFIG_VOLSIZEUNITS4 = "Volume Size Units 4";
const char* CONFIG_VOLSIZEUNITS5 = "Volume Size Units 5";
const char* CONFIG_LASTUSEDAUTO = "Last Used Auto";
const char* CONFIG_AUTOEXPANDMV = "Auto Expand MV";
const char* CONFIG_VERSION = "Version";
const char* CONFIG_DEFSFX = "Default Sfx File";
const char* CONFIG_SFXLAST = "Last Used Sfx Settings";
const char* CONFIG_SFXLASTSIZE = "Last Used Sfx Settings Size";
const char* CONFIG_SFXFAV_KEY = "Favorities";
const char* CONFIG_SFXFAVCOUNT = "Number Of Favorities";
const char* CONFIG_SFXFAVNAME = "Favorite %d Name";
const char* CONFIG_SFXFAVSIZE = "Favorite %d Size";
const char* CONFIG_SFXFAVDATA = "Favorite %d Data";
const char* CONFIG_SFXLASTEXPORTPATH = "Last Export Path";
const char* CONFIG_SALVER = "Salamander Version";
const char* CONFIG_CHLANG = "Change Language Reaction";
const char* CONFIG_WINZIPNAMES = "Winzip Names";

const char* CONFIG_LIST_INFO_PACKED_SIZE = "List Info Packed Size";
const char* CONFIG_COL_PACKEDSIZE_FIXEDWIDTH = "Column PackedSize FixedWidth";
const char* CONFIG_COL_PACKEDSIZE_WIDTH = "Column PackedSize Width";

// definice IDs do menu
#define MID_CREATESFX 1
#define MID_REPAIR 2
#define MID_TEST 3
#define MID_COMMENT 4

//
// ****************************************************************************
// SalamanderPluginGetReqVer
//

int WINAPI SalamanderPluginGetReqVer()
{
    CALL_STACK_MESSAGE_NONE
    return LAST_VERSION_OF_SALAMANDER;
}

//
// ****************************************************************************
// SalamanderPluginEntry
//

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    CALL_STACK_MESSAGE_NONE
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (salamander->GetVersion() < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "ZIP" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "ZIP" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();
    SalamanderCrypt = SalamanderGeneral->GetSalamanderCrypt();
    SalamanderBZIP2 = SalamanderGeneral->GetSalamanderBZIP2();

    // ziskame rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
    SalamanderGUI = salamander->GetSalamanderGUI();

    SalamanderGeneral->SetHelpFileName("zip.chm");

    /*
  //beta plati do konce unora 2001
  SYSTEMTIME st;
  GetLocalTime(&st);
  if (st.wYear == 2001 && st.wMonth > 2 || st.wYear > 2001)
  {
    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_EXPIRE), LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
    return NULL;
  }
  */

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_PANELARCHIVEREDIT |
                                       FUNCTION_CUSTOMARCHIVERPACK | FUNCTION_CUSTOMARCHIVERUNPACK |
                                       FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "ZIP" /* neprekladat! */, "zip;pk3;pk4;jar");

    // nastavime URL home-page pluginu
    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

//
// ****************************************************************************
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
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUTPLUGINTITLE), MB_OK | MB_ICONINFORMATION);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    if (SfxLanguages)
        delete SfxLanguages;
    if (DefLanguage)
        delete DefLanguage;
    return TRUE;
}

void ValidateDefSfxFile()
{
    // zjistime jestli soubor existuje, kdyz ne nastavime jako defaultni prvni, ktery najdeme
    char path[MAX_PATH];
    char* file;
    GetModuleFileName(DLLInstance, path, MAX_PATH);
    SalamanderGeneral->CutDirectory(path);
    SalamanderGeneral->SalPathAppend(path, "sfx", MAX_PATH);
    SalamanderGeneral->SalPathAddBackslash(path, MAX_PATH);
    file = path + lstrlen(path);
    lstrcpy(file, Config.DefSfxFile);
    DWORD attr = SalamanderGeneral->SalGetFileAttributes(path);
    if (attr == 0xFFFFFFFF || attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        WIN32_FIND_DATA fd;
        lstrcpy(file, "*.sfx");
        HANDLE find = FindFirstFile(path, &fd);
        while (find != INVALID_HANDLE_VALUE && fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!FindNextFile(find, &fd))
            {
                FindClose(find);
                find = INVALID_HANDLE_VALUE;
            }
        }
        if (find == INVALID_HANDLE_VALUE)
            *Config.DefSfxFile = 0;
        else
        {
            lstrcpy(Config.DefSfxFile, fd.cFileName);
            FindClose(find);
        }
    }
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    DWORD v;
    v = Config.CurSalamanderVersion;
    Config = DefConfig;
    Config.CurSalamanderVersion = v;
    if (regKey != NULL) // load z registry
    {
        registry->GetValue(regKey, CONFIG_LEVEL, REG_DWORD, &Config.Level, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_ENCRYPTMETHOD, REG_DWORD, &Config.EncryptMethod, sizeof(DWORD));
        if (registry->GetValue(regKey, CONFIG_NOEMPTYDIRS, REG_DWORD, &v, sizeof(DWORD)))
        {
            Config.NoEmptyDirs = (v != 0);
        }
        if (registry->GetValue(regKey, CONFIG_BACKUPZIP, REG_DWORD, &v, sizeof(DWORD)))
        {
            Config.BackupZip = (v != 0);
        }
        if (registry->GetValue(regKey, CONFIG_SHOWEXOPT, REG_DWORD, &v, sizeof(DWORD)))
        {
            Config.ShowExOptions = (v != 0);
        }
        if (registry->GetValue(regKey, CONFIG_TIMETONEWESTFILE, REG_DWORD, &v, sizeof(DWORD)))
        {
            Config.TimeToNewestFile = (v != 0);
        }

        char key1[64];
        char key2[64];
        char size[MAX_VOL_STR];
        DWORD units;
        int i;
        for (i = 0; i < 5; i++)
        {
            sprintf(key1, CONFIG_VOLSIZECACHE, i + 1);
            sprintf(key2, CONFIG_VOLSIZEUNITS, i + 1);
            if (registry->GetValue(regKey, key1, REG_SZ, size, MAX_VOL_STR) &&
                registry->GetValue(regKey, key2, REG_DWORD, &units, sizeof(DWORD)))
            {
                lstrcpy(Config.VolSizeCache[i], size);
                Config.VolSizeUnits[i] = units;
            }
            else
                break;
        }
        if (registry->GetValue(regKey, CONFIG_LASTUSEDAUTO, REG_DWORD, &v, sizeof(DWORD)))
        {
            Config.LastUsedAuto = (v != 0);
        }
        if (registry->GetValue(regKey, CONFIG_AUTOEXPANDMV, REG_DWORD, &v, sizeof(DWORD)))
        {
            Config.AutoExpandMV = (v != 0);
        }
        if (!registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &Config.Version, sizeof(DWORD)))
        {
            Config.Version = 1; // beta 3 jeste nema value v konfiguraci
        }
        registry->GetValue(regKey, CONFIG_DEFSFX, REG_SZ, Config.DefSfxFile, MAX_PATH);
        ValidateDefSfxFile();

        char* buffer = NULL;
        DWORD allocated = 0;
        DWORD siz;

        // load last used sfx settings
        *LastUsedSfxSet.Name = 0;
        if (registry->GetValue(regKey, CONFIG_SFXLASTSIZE, REG_DWORD, &siz, sizeof(DWORD)))
        {
            if (siz > allocated)
            {
                allocated = max(siz, allocated * 2);
                buffer = (char*)realloc(buffer, allocated);
            }
            if (registry->GetValue(regKey, CONFIG_SFXLAST, REG_BINARY, buffer, siz))
            {
                if (ExpandSfxSettings(&LastUsedSfxSet.Settings, buffer, siz) == siz)
                {
                    lstrcpy(LastUsedSfxSet.Name, "Last Used");
                }
            }
        }

        // load favorite options
        HKEY favKey;
        if (registry->OpenKey(regKey, CONFIG_SFXFAV_KEY, favKey))
        {
            int count;
            if (registry->GetValue(favKey, CONFIG_SFXFAVCOUNT, REG_DWORD, &count, sizeof(DWORD)))
            {
                char key[64];
                CFavoriteSfx temp;
                Favorities.Destroy();
                for (i = 0; i < count; i++)
                {
                    sprintf(key, CONFIG_SFXFAVSIZE, i + 1);
                    if (registry->GetValue(favKey, key, REG_DWORD, &siz, sizeof(DWORD)))
                    {
                        if (siz > allocated)
                        {
                            allocated = max(siz, allocated * 2);
                            buffer = (char*)realloc(buffer, allocated);
                        }
                        sprintf(key, CONFIG_SFXFAVDATA, i + 1);
                        if (registry->GetValue(favKey, key, REG_BINARY, buffer, siz))
                        {
                            if (ExpandSfxSettings(&temp.Settings, buffer, siz) == siz)
                            {
                                sprintf(key, CONFIG_SFXFAVNAME, i + 1);
                                if (registry->GetValue(favKey, key, REG_SZ, temp.Name, MAX_FAVNAME))
                                {
                                    CFavoriteSfx* newSetting = new CFavoriteSfx;
                                    if (newSetting)
                                        *newSetting = temp;
                                    if (!newSetting || !Favorities.Add(newSetting))
                                    {
                                        if (newSetting)
                                            delete newSetting;
                                        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERRLOADCONFIG), LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONEXCLAMATION);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            registry->CloseKey(favKey);
        }

        if (buffer)
            free(buffer);

        registry->GetValue(regKey, CONFIG_SFXLASTEXPORTPATH, REG_SZ, Config.LastExportPath, MAX_PATH);

        if (!registry->GetValue(regKey, CONFIG_SALVER, REG_DWORD, &v, sizeof(DWORD)))
            v = 0;
        if (!registry->GetValue(regKey, CONFIG_CHLANG, REG_DWORD, &Config.ChangeLangReaction, sizeof(DWORD)))
            Config.ChangeLangReaction = CLR_ASK;
        if (!registry->GetValue(regKey, CONFIG_WINZIPNAMES, REG_DWORD, &Config.WinZipNames, sizeof(DWORD)))
            Config.WinZipNames = TRUE;

        registry->GetValue(regKey, CONFIG_LIST_INFO_PACKED_SIZE, REG_DWORD, &Config.ListInfoPackedSize, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_COL_PACKEDSIZE_FIXEDWIDTH, REG_DWORD, &Config.ColumnPackedSizeFixedWidth, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_COL_PACKEDSIZE_WIDTH, REG_DWORD, &Config.ColumnPackedSizeWidth, sizeof(DWORD));
    }
    else
    {
        ValidateDefSfxFile();
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    registry->SetValue(regKey, CONFIG_LEVEL, REG_DWORD, &Config.Level, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_ENCRYPTMETHOD, REG_DWORD, &Config.EncryptMethod, sizeof(DWORD));
    DWORD v = Config.NoEmptyDirs;
    registry->SetValue(regKey, CONFIG_NOEMPTYDIRS, REG_DWORD, &v, sizeof(DWORD));
    v = Config.BackupZip;
    registry->SetValue(regKey, CONFIG_BACKUPZIP, REG_DWORD, &v, sizeof(DWORD));
    v = Config.ShowExOptions;
    registry->SetValue(regKey, CONFIG_SHOWEXOPT, REG_DWORD, &v, sizeof(DWORD));
    v = Config.TimeToNewestFile;
    registry->SetValue(regKey, CONFIG_TIMETONEWESTFILE, REG_DWORD, &v, sizeof(DWORD));
    /*
  registry->SetValue(regKey, CONFIG_VOLSIZECACHE1, REG_BINARY, Config.VolSizeCache[0], MAX_VOL_STR);
  registry->SetValue(regKey, CONFIG_VOLSIZECACHE2, REG_BINARY, Config.VolSizeCache[1], MAX_VOL_STR);
  registry->SetValue(regKey, CONFIG_VOLSIZECACHE3, REG_BINARY, Config.VolSizeCache[2], MAX_VOL_STR);
  registry->SetValue(regKey, CONFIG_VOLSIZECACHE4, REG_BINARY, Config.VolSizeCache[3], MAX_VOL_STR);
  registry->SetValue(regKey, CONFIG_VOLSIZECACHE5, REG_BINARY, Config.VolSizeCache[4], MAX_VOL_STR);
  */
    registry->SetValue(regKey, CONFIG_VOLSIZECACHE1, REG_SZ, Config.VolSizeCache[0], -1);
    registry->SetValue(regKey, CONFIG_VOLSIZECACHE2, REG_SZ, Config.VolSizeCache[1], -1);
    registry->SetValue(regKey, CONFIG_VOLSIZECACHE3, REG_SZ, Config.VolSizeCache[2], -1);
    registry->SetValue(regKey, CONFIG_VOLSIZECACHE4, REG_SZ, Config.VolSizeCache[3], -1);
    registry->SetValue(regKey, CONFIG_VOLSIZECACHE5, REG_SZ, Config.VolSizeCache[4], -1);
    registry->SetValue(regKey, CONFIG_VOLSIZEUNITS1, REG_DWORD, &Config.VolSizeUnits[0], sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_VOLSIZEUNITS2, REG_DWORD, &Config.VolSizeUnits[1], sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_VOLSIZEUNITS3, REG_DWORD, &Config.VolSizeUnits[2], sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_VOLSIZEUNITS4, REG_DWORD, &Config.VolSizeUnits[3], sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_VOLSIZEUNITS5, REG_DWORD, &Config.VolSizeUnits[4], sizeof(DWORD));
    v = Config.LastUsedAuto;
    registry->SetValue(regKey, CONFIG_LASTUSEDAUTO, REG_DWORD, &v, sizeof(DWORD));
    v = Config.AutoExpandMV;
    registry->SetValue(regKey, CONFIG_AUTOEXPANDMV, REG_DWORD, &v, sizeof(DWORD));
    v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_DEFSFX, REG_SZ, Config.DefSfxFile, -1);

    char* buffer = NULL;
    DWORD allocated = 0;
    DWORD siz;

    //save last used sfx settings
    if (*LastUsedSfxSet.Name)
    {
        siz = PackSfxSettings(&LastUsedSfxSet.Settings, buffer, allocated);
        if (siz == -1)
            TRACE_E("chyba v PackSfxSettings.");
        else
        {
            if (registry->SetValue(regKey, CONFIG_SFXLAST, REG_BINARY, buffer, siz))
                registry->SetValue(regKey, CONFIG_SFXLASTSIZE, REG_DWORD, &siz, sizeof(DWORD));
        }
    }

    //save favorite sfx settings
    HKEY favKey;
    if (registry->CreateKey(regKey, CONFIG_SFXFAV_KEY, favKey))
    {
        registry->ClearKey(favKey);
        char key[64];
        int i;
        for (i = 0; i < Favorities.Count; i++)
        {
            CFavoriteSfx* fav = Favorities[i];

            sprintf(key, CONFIG_SFXFAVNAME, i + 1);
            if (!registry->SetValue(favKey, key, REG_SZ, fav->Name, -1))
                break;
            siz = PackSfxSettings(&fav->Settings, buffer, allocated);
            if (siz == -1)
            {
                TRACE_E("chyba v PackSfxSettings.");
                break;
            }
            sprintf(key, CONFIG_SFXFAVDATA, i + 1);
            if (!registry->SetValue(favKey, key, REG_BINARY, buffer, siz))
                break;
            sprintf(key, CONFIG_SFXFAVSIZE, i + 1);
            if (!registry->SetValue(favKey, key, REG_DWORD, &siz, sizeof(DWORD)))
                break;
        }
        registry->SetValue(favKey, CONFIG_SFXFAVCOUNT, REG_DWORD, &i, sizeof(DWORD));
        registry->CloseKey(favKey);
    }

    if (buffer)
        free(buffer);

    registry->SetValue(regKey, CONFIG_SFXLASTEXPORTPATH, REG_SZ, Config.LastExportPath, -1);
    registry->SetValue(regKey, CONFIG_CHLANG, REG_DWORD, &Config.ChangeLangReaction, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SALVER, REG_DWORD, &Config.CurSalamanderVersion, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_WINZIPNAMES, REG_DWORD, &Config.WinZipNames, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_LIST_INFO_PACKED_SIZE, REG_DWORD, &Config.ListInfoPackedSize, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_COL_PACKEDSIZE_FIXEDWIDTH, REG_DWORD, &Config.ColumnPackedSizeFixedWidth, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_COL_PACKEDSIZE_WIDTH, REG_DWORD, &Config.ColumnPackedSizeWidth, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");

    CConfigDialog dlg(parent, &Config);

    if (dlg.Proceed() == IDOK)
    {
        if (SalamanderGeneral->GetPanelPluginData(PANEL_LEFT) != NULL)
            SalamanderGeneral->PostRefreshPanelPath(PANEL_LEFT);
        if (SalamanderGeneral->GetPanelPluginData(PANEL_RIGHT) != NULL)
            SalamanderGeneral->PostRefreshPanelPath(PANEL_RIGHT);
    }
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    salamander->AddCustomPacker("ZIP (Plugin)", "zip", FALSE);
    salamander->AddCustomUnpacker("ZIP (Plugin)", "*.zip;*.pk3;*.jar",
                                  Config.Version < 2); // pred SS 1.6 beta 4 nebyl *.jar -> obnova
    salamander->AddPanelArchiver("zip;pk3;jar", TRUE, FALSE);

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_MENUCOMMENT
	{MNTT_IT, IDS_MENUCREATESFX
//	{MNTT_IT, IDS_MENUREPAIR
	{MNTT_IT, IDS_MENUTEST
	{MNTT_PE, 0
};
*/

    salamander->AddMenuItem(-1, LoadStr(IDS_MENUCOMMENT), 0, MID_COMMENT, TRUE, 0, 0,
                            MENU_SKILLLEVEL_INTERMEDIATE | MENU_SKILLLEVEL_ADVANCED);
    salamander->AddMenuItem(-1, LoadStr(IDS_MENUCREATESFX), 0, MID_CREATESFX, TRUE, 0, 0,
                            MENU_SKILLLEVEL_ALL);
    //salamander->AddMenuItem(LoadStr(IDS_MENUREPAIR), 0, MID_REPAIR, TRUE, 0, 0);
    salamander->AddMenuItem(-1, LoadStr(IDS_MENUTEST), 0, MID_TEST, TRUE, 0, 0, MENU_SKILLLEVEL_ALL);

    if (Config.Version < 2) // pred SS 1.6 beta 4
    {
        salamander->AddPanelArchiver("jar", TRUE, TRUE); // nebyl jar -> pridame ho
    }

    // nastavime ikonku pluginu
    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_ZIP),
                                      IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR);
    salamander->SetBitmapWithIcons(hBmp);
    DeleteObject(hBmp);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);
}

void CPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
    delete ((CPluginDataInterface*)pluginData);
}

CPluginInterfaceForArchiverAbstract*
CPluginInterface::GetInterfaceForArchiver()
{
    CALL_STACK_MESSAGE_NONE
    return &InterfaceForArchiver;
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    CALL_STACK_MESSAGE_NONE
    return &InterfaceForMenuExt;
}

//
// ****************************************************************************
// CPluginInterfaceForMenuExt
//

DWORD
CPluginInterfaceForMenuExt::GetMenuItemState(int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::GetMenuItemState(%d, 0x%X)",
                        id, eventMask);
    // musi byt na disku nebo v nasem pluginu
    if ((eventMask & (MENU_EVENT_DISK | MENU_EVENT_THIS_PLUGIN_ARCH)) == 0)
        return 0;

    // pokud v archivu jsme, vse je enabled
    if (eventMask & MENU_EVENT_THIS_PLUGIN_ARCH)
        return MENU_ITEM_STATE_ENABLED;

    // vezmeme bud oznacene soubory nebo soubor pod fokusem
    const CFileData* file = NULL;
    BOOL isDir;
    if ((eventMask & MENU_EVENT_FILES_SELECTED) == 0)
    {
        if ((eventMask & MENU_EVENT_FILE_FOCUSED) == 0)
            return 0;
        file = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, &isDir); // selected neni nic -> enumerace failne
    }

    BOOL ret = TRUE;
    int count = 0;

    int index = 0;
    if (file == NULL)
        file = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);
    while (file != NULL && ret)
    {
        if (SalamanderGeneral->IsArchiveHandledByThisPlugin(file->Name))
            count++;
        else
            ret = FALSE;
        file = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);
    }

    return (ret && count > 0) ? MENU_ITEM_STATE_ENABLED : 0; // vsechny oznacene soubory jsou nase
}

BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                                 int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::ExecuteMenuItem(, , %d, 0x%X)", id,
                        eventMask);

    char zipFile[MAX_PATH];
    char* fileName;
    char* arch;
    BOOL selFiles = FALSE;
    int index = 0;
    BOOL ok = TRUE;

    if (!SalamanderGeneral->GetPanelPath(PANEL_SOURCE, zipFile, MAX_PATH, NULL, &arch))
        return FALSE;

    if (!arch)
    {
        SalamanderGeneral->SalPathAddBackslash(zipFile, MAX_PATH);
        fileName = zipFile + lstrlen(zipFile);
        selFiles = eventMask & MENU_EVENT_FILES_SELECTED;
    }

    BOOL changesReported = FALSE; // pomocna promenna - TRUE pokud uz byly hlaseny zmeny na ceste
    do
    {
        if (arch)
        {
            *arch = NULL;
        }
        else
        {
            const CFileData* fileData;
            if (selFiles)
                fileData = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, NULL);
            else
                fileData = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, NULL);
            if (!fileData)
                break; // konec enumerace, nebo chyba (v pripade GetFocusedItem)
            lstrcpy(fileName, fileData->Name);
            DWORD attr = SalamanderGeneral->SalGetFileAttributes(zipFile);
            if (attr != 0xFFFFFFFF && attr & FILE_ATTRIBUTE_DIRECTORY)
                continue;
        }

        if (!ok && SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CONTINUE), LoadStr(IDS_PLUGINNAME),
                                                     MSGBOX_QUESTION) != IDYES)
            return FALSE;
        ok = TRUE;

        switch (id)
        {
        case MID_COMMENT:
        {
            SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_SOURCE); // tento prikaz povazujeme za praci s cestou (objevi se v Alt+F12)

            CZipPack pack(zipFile, "", salamander);

            if (pack.ErrorID || pack.CommentArchive())
            {
                if (pack.ErrorID != IDS_NODISPLAY)
                    SalamanderGeneral->ShowMessageBox(LoadStr(pack.ErrorID),
                                                      LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                ok = FALSE;
            }
            if (pack.UserBreak)
                ok = FALSE;
            break;
        }

        case MID_CREATESFX:
        {
            SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_SOURCE); // tento prikaz povazujeme za praci s cestou (objevi se v Alt+F12)

            CZipPack pack(zipFile, "", salamander);

            if (pack.ErrorID || pack.CreateSFX())
            {
                if (pack.ErrorID != IDS_NODISPLAY)
                    SalamanderGeneral->ShowMessageBox(LoadStr(pack.ErrorID),
                                                      LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                ok = FALSE;
            }
            if (pack.UserBreak)
                ok = FALSE;
            break;
        }

            //case MID_REPAIR:break;

        case MID_TEST:
        {
            SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_SOURCE); // tento prikaz povazujeme za praci s cestou (objevi se v Alt+F12)

            CZipUnpack unpack(zipFile, "", salamander, NULL);

            unpack.Test = true;
            unpack.AllFilesOK = TRUE;
            if (unpack.ErrorID || unpack.UnpackWholeArchive("*.*", ""))
            {
                if (unpack.ErrorID != IDS_NODISPLAY)
                    SalamanderGeneral->ShowMessageBox(LoadStr(unpack.ErrorID),
                                                      LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                ok = FALSE;
            }
            if (unpack.UserBreak)
                ok = FALSE;
            if (ok)
            {
                char buf[1024];
                sprintf(buf, unpack.AllFilesOK ? LoadStr(IDS_TESTOK) : LoadStr(IDS_TESTKO), zipFile);
                SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
            }
            break;
        }
        }

        if (id == MID_COMMENT || id == MID_CREATESFX || id == MID_REPAIR) // pokud mohlo dojit ke zmene
        {
            if (!changesReported) // zmena na ceste + jeste nebyla hlasena -> ohlasime
            {
                changesReported = TRUE;
                // ohlasime zmenu na ceste, kde lezi menene PAK soubory (hlaseni se provede az po opusteni
                // kodu pluginu - po navratu z teto metody)
                char zipFileDir[MAX_PATH];
                strcpy(zipFileDir, zipFile);
                SalamanderGeneral->CutDirectory(zipFileDir); // musi jit, protoze jde o existujici soubor
                SalamanderGeneral->PostChangeOnPathNotification(zipFileDir, FALSE);
            }
        }
    } while (selFiles); // cyklime dokud metoda GetSelectedItem nevrati NULL

    return TRUE;
}

BOOL CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForMenuExt::HelpForMenuItem(, %d)", id);
    int helpID = 0;
    switch (id)
    {
    case MID_COMMENT:
        helpID = IDH_COMMENT;
        break;
    case MID_CREATESFX:
        helpID = IDH_CREATESFX;
        break;
    case MID_TEST:
        helpID = IDH_TEST;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}

//
// ****************************************************************************
// CPluginDataInterface
//

void CPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
{
    // file.PluginData is NULL for folders not having extra items in the archive - see GetFileDataForUpDir & GetFileDataForNewDir
    delete (CZIPFileData*)file.PluginData; // However, delete NULL is perfectly OK
}

// Callback called by Salamander to obtain custom column text - see spl_com.h / FColumnGetText
// Global variables - pointers to global variables used by Salamander
static const CFileData** TransferFileData = NULL;
static int* TransferIsDir = NULL;
static char* TransferBuffer = NULL;
static int* TransferLen = NULL;
static DWORD* TransferRowData = NULL;
static CPluginDataInterfaceAbstract** TransferPluginDataIface = NULL;
static DWORD* TransferActCustomData = NULL;

static void WINAPI GetPackedSizeText()
{
    if (*TransferIsDir)
    {
        *TransferLen = 0;
    }
    else
    {
        CZIPFileData* zipFileData = (CZIPFileData*)(*TransferFileData)->PluginData;
        if (zipFileData->PackedSize != 0)
        {
            SalamanderGeneral->NumberToStr(TransferBuffer, CQuadWord().SetUI64(zipFileData->PackedSize));
            *TransferLen = (int)_tcslen(TransferBuffer);
        }
        else
        {
            *TransferLen = 0;
        }
    }
}

void WINAPI
CPluginDataInterface::SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                const CFileData* upperDir)
{
    view->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer, TransferLen, TransferRowData,
                               TransferPluginDataIface, TransferActCustomData);

    // Special columns added only in Detailed mode
    if (view->GetViewMode() == VIEW_MODE_DETAILED)
    {
        CColumn column;
        if (Config.ListInfoPackedSize)
        {
            // We add Packed Size just after the Size column; or at the end in case of failure
            int sizeIndex = view->GetColumnsCount();
            int i;
            for (i = 0; i < sizeIndex; i++)
                if (view->GetColumn(i)->ID == COLUMN_ID_SIZE)
                {
                    sizeIndex = i + 1;
                    break;
                }

            _tcscpy(column.Name, LoadStr(IDS_LISTINFO_PAKEDSIZE));
            _tcscpy(column.Description, LoadStr(IDS_LISTINFO_PAKEDSIZE_DESC));
            column.GetText = GetPackedSizeText;
            column.SupportSorting = 0;
            column.LeftAlignment = 0;
            column.ID = COLUMN_ID_CUSTOM;
            column.CustomData = 0;
            column.Width = leftPanel ? LOWORD(Config.ColumnPackedSizeWidth) : HIWORD(Config.ColumnPackedSizeWidth);
            column.FixedWidth = leftPanel ? LOWORD(Config.ColumnPackedSizeFixedWidth) : HIWORD(Config.ColumnPackedSizeFixedWidth);
            view->InsertColumn(sizeIndex, &column);
        }
    }
}

void CPluginDataInterface::ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth)
{
    if (column->CustomData == 0)
    {
        if (leftPanel)
            Config.ColumnPackedSizeFixedWidth = MAKELONG(newFixedWidth, HIWORD(Config.ColumnPackedSizeFixedWidth));
        else
            Config.ColumnPackedSizeFixedWidth = MAKELONG(LOWORD(Config.ColumnPackedSizeFixedWidth), newFixedWidth);
    }
    if (newFixedWidth)
        ColumnWidthWasChanged(leftPanel, column, column->Width);
}

void CPluginDataInterface::ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth)
{
    if (column->CustomData == 0)
    {
        if (leftPanel)
            Config.ColumnPackedSizeWidth = MAKELONG(newWidth, HIWORD(Config.ColumnPackedSizeWidth));
        else
            Config.ColumnPackedSizeWidth = MAKELONG(LOWORD(Config.ColumnPackedSizeWidth), newWidth);
    }
}

//
// ****************************************************************************
// CPluginInterfaceForArchiver
//

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander,
                                              const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);

    CZipList list(fileName, salamander);

    BOOL haveFiles;
    if (list.ErrorID || list.ListArchive(dir, haveFiles))
    {
        if (list.ErrorID != IDS_NODISPLAY)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(list.ErrorID),
                                              LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        }
        if (!haveFiles)
            return FALSE;
    }
    pluginData = new CPluginDataInterface();
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                                const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName, targetDir, archiveRoot);

    CRT_MEM_CHECKPOINT

    {
        const char* arcRoot = archiveRoot ? archiveRoot : "";
        if (*arcRoot == '\\')
            arcRoot++;
        CZipUnpack unpack(fileName, arcRoot, salamander, NULL);

        if (unpack.ErrorID || unpack.UnpackArchive(targetDir, next, nextParam))
        {
            if (unpack.ErrorID != IDS_NODISPLAY)
                SalamanderGeneral->ShowMessageBox(LoadStr(unpack.ErrorID), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            if (unpack.Fatal)
                return TRUE;
            else
                return FALSE;
        }
        if (unpack.UserBreak)
            return FALSE;
    }

    CRT_MEM_DUMP_ALL_OBJECTS_SINCE

    return TRUE;
}

BOOL CPluginInterfaceForArchiver::UnpackOneFile(CSalamanderForOperationsAbstract* salamander,
                                                const char* fileName, CPluginDataInterfaceAbstract* pluginData,
                                                const char* nameInArchive, const CFileData* fileData,
                                                const char* targetDir, const char* newFileName,
                                                BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackOneFile(, %s, , %s, , %s, ,)", fileName,
                        nameInArchive, targetDir);

    CRT_MEM_CHECKPOINT

    {
        /*    if (newFileName != NULL)
    {
      *renamingNotSupported = TRUE;
      return FALSE;
    }*/

        CZipUnpack unpack(fileName, "", salamander, NULL);

        if (unpack.ErrorID || unpack.UnpackOneFile(nameInArchive, fileData, targetDir, newFileName))
        {
            if (unpack.ErrorID != IDS_NODISPLAY)
                SalamanderGeneral->ShowMessageBox(LoadStr(unpack.ErrorID),
                                                  LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            return FALSE;
        }
        if (unpack.UserBreak)
            return FALSE;
    }
    /*_CrtMemCheckpoint(&st2);
  _CrtMemDifference(&stdiff, &st1, &st2);
  _CrtMemDumpStatistics(&st1);
  _CrtMemDumpStatistics(&st2);
  _CrtMemDumpStatistics(&stdiff);*/
    CRT_MEM_DUMP_ALL_OBJECTS_SINCE

    return TRUE;
}

BOOL CPluginInterfaceForArchiver::PackToArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                const char* archiveRoot, BOOL move, const char* sourcePath,
                                                SalEnumSelection2 next, void* nextParam)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::PackToArchive(, %s, %s, %d, %s, ,)", fileName,
                        archiveRoot, move, sourcePath);

    CRT_MEM_CHECKPOINT

    {
        const char* arcRoot = archiveRoot ? archiveRoot : "";
        if (*arcRoot == '\\')
            arcRoot++;
        CZipPack pack(fileName, arcRoot, salamander);

        if (pack.ErrorID || pack.PackToArchive(move, sourcePath, next, nextParam))
        {
            if (pack.ErrorID != IDS_NODISPLAY)
                SalamanderGeneral->ShowMessageBox(LoadStr(pack.ErrorID),
                                                  LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            if (!pack.RecoverOK)
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERRRECOVER),
                                                  LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            if (pack.Fatal)
                return TRUE;
            else
                return FALSE;
        }
        if (pack.UserBreak)
            return FALSE;
    }

    CRT_MEM_DUMP_ALL_OBJECTS_SINCE

    return TRUE;
}

BOOL CPluginInterfaceForArchiver::DeleteFromArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                    CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                                                    SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::DeleteFromArchive(, %s, , %s, ,)", fileName, archiveRoot);

    CRT_MEM_CHECKPOINT

    {
        const char* arcRoot = archiveRoot ? archiveRoot : "";
        if (*arcRoot == '\\')
            arcRoot++;
        CZipPack pack(fileName, arcRoot, salamander);

        if (pack.ErrorID || pack.DeleteFromArchive(next, nextParam))
        {
            if (pack.ErrorID != IDS_NODISPLAY)
                SalamanderGeneral->ShowMessageBox(LoadStr(pack.ErrorID),
                                                  LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            if (!pack.RecoverOK)
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERRRECOVER),
                                                  LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);

            if (pack.Fatal)
                return TRUE;
            else
                return FALSE;
        }
        if (pack.UserBreak)
            return FALSE;
    }

    CRT_MEM_DUMP_ALL_OBJECTS_SINCE

    return TRUE;
}

void QuickSortArcVolumes(TIndirectArray2<char>* arcVolumes, int left, int right)
{
    int i = left, j = right;
    const char* pivot = (*arcVolumes)[(i + j) / 2];

    do
    {
        while (strcmp((*arcVolumes)[i], pivot) < 0 && i < right)
            i++;
        while (strcmp(pivot, (*arcVolumes)[j]) < 0 && j > left)
            j--;

        if (i <= j)
        {
            char* swap = (*arcVolumes)[i];
            (*arcVolumes)[i] = (*arcVolumes)[j];
            (*arcVolumes)[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    if (left < j)
        QuickSortArcVolumes(arcVolumes, left, j);
    if (i < right)
        QuickSortArcVolumes(arcVolumes, i, right);
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                     const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);

    CRT_MEM_CHECKPOINT

    {
        TIndirectArray2<char> arcVolumes(100, TRUE);
        CZipUnpack unpack(fileName, "", salamander, delArchiveWhenDone ? &arcVolumes : NULL);

        if (unpack.ErrorID || unpack.UnpackWholeArchive(mask, targetDir))
        {
            if (unpack.ErrorID != IDS_NODISPLAY)
                SalamanderGeneral->ShowMessageBox(LoadStr(unpack.ErrorID),
                                                  LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            /*    // Petr: tohle vazne nevim k cemu melo byt ... kazdopadne ted to musim odstavit, jinak se pri fatalni chybe a 'delArchiveWhenDone'==TRUE smaze archiv, coz je nepripustne
      if (unpack.Fatal)
        return TRUE;
      else
*/
            return FALSE;
        }
        if (unpack.UserBreak)
            return FALSE;
        if (delArchiveWhenDone) // prehazime unikatni svazky z 'arcVolumes' do 'archiveVolumes' (v 'arcVolumes' se muzou svazky opakovat)
        {
            if (arcVolumes.Count > 1)
                QuickSortArcVolumes(&arcVolumes, 0, arcVolumes.Count - 1);
            char* lastVolume = NULL;
            for (int i = 0; i < arcVolumes.Count; i++)
            {
                if (lastVolume == NULL || strcmp(lastVolume, arcVolumes[i]) != 0)
                {
                    lastVolume = arcVolumes[i];
                    archiveVolumes->Add(lastVolume, -2);
                }
            }
        }
    }

    CRT_MEM_DUMP_ALL_OBJECTS_SINCE

    return TRUE;
}
