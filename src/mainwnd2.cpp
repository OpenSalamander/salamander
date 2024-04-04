// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <shlwapi.h>
#undef PathIsPrefix // otherwise collision with CSalamanderGeneral::PathIsPrefix

#include "toolbar.h"
#include "stswnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "cfgdlg.h"
#include "usermenu.h"
#include "viewer.h"
#include "zip.h"
#include "pack.h"
#include "find.h"
#include "dialogs.h"
#include "logo.h"
#include "tasklist.h"
#include "pwdmngr.h"

//
// ConfigVersion - version number of the loaded configuration
//
// 0 = no configuration found - default values will be used
// 1 = v1.52 and older
// 2 = 1.6b1
// 3 = 1.6b1.x
// 4 = 1.6b3.x
// 5 = 1.6b3.x          for correct conversion of packetizer configurations between our versions
// 6 = 1.6b4.x
// 7 = 1.6b5.x          for correct conversion of supported plugin functions (see CPlugins::Load)
// 8 = 1.6b5.x          better to check it out ;-)
// 9 = 1.6b5.x          for transitioning from exe name to variable in custom packers
// 10 = 1.6b6           for renaming "XXX (Internal)" to "XXX (Plug-in)" in Pack and Unpack dialogs
//                      and for setting the ANSI version of the "list of files" file for ACE32 and PKZIP25 (un)packers
// 11 = 1.6b7           CheckVer plugin added - we will ensure its automatic installation
// 12 = 2.0 auto-shutdown salopen.exe + PEViewer plugin arrived - we will ensure its automatic installation
// 13 = 2.5b1           added missing configuration conversion for custom packers - reflecting the change in LHA
// 14 = 2.5b1           New Advanced Options in the Find dialog. Transition to CFilterCriteria. Conversion of inverse mask in filters.
// 15 = 2.5b2           newer version, so that plugins can be loaded (upgrade records in the registry)
// 16 = 2.5b2           added coloring of Encrypted files and directories (added during config load + is in the default config)
// 17 = 2.5b2           added mask *.xml to internal viewer settings - "force text mode"
// 18 = 2.5b3           only for transferring plugin configuration from version 2.5b2 for now
// 19 = 2.5b4           only for transferring plugin configuration from version 2.5b3 for now
// 20 = 2.5b5           for now only for transferring plugin configuration from version 2.5b4
// 21 = 2.5b6           only for transferring plugin configuration from version 2.5b5(a) for now
// 22 = 2.5b6           filters in panels -- unified into one history
// 23 = 2.5b6           new view in panel (Tiles)
// 24 = 2.5b7           only for transferring plugin configuration from version 2.5b6 for now
// 25 = 2.5b7           plugins: show in plugin bar -> transfer variable to CPluginData
// 26 = 2.5b8           only for transferring plugin configuration from version 2.5b7 for now
// 27 = 2.5b9           only for transferring plugin configuration from version 2.5b8 for now
// 28 = 2.5b9           new color scheme based on the old DOS Navigator -> conversion 'scheme'
// 29 = 2.5b10          only for transferring plugin configuration from version 2.5b9 for now
// 30 = 2.5b11          only for transferring plugin configuration from version 2.5b10 for now
// 31 = 2.5b11          We have introduced the Floppy section in the Drives configuration and need to forcibly refresh the icon reading for Removable
// 32 = 2.5b11          Find: "Local Settings\\Temporary Internet Files" is implicitly searched
// 33 = 2.5b12          only for transferring plugin configuration from version 2.5b11 for now
// 34 = 2.5b12          modification of external packer/unpacker PKZIP25 (external Win32 version)
// 35 = 2.5RC1          just for transferring the plugin configuration from version 2.5b12 (just internal, we released RC1 instead of it)
// 36 = 2.5RC2          just for transferring the plugin configuration from version 2.5RC1
// 37 = 2.5RC3          just for transferring the plugin configuration from version 2.5RC2
// 38 = 2.5RC3          renaming Servant Salamander to Altap Salamander
// 39 = 2.5             just for transferring the plugin configuration from version 2.5RC3
// 40 = 2.51            just for transferring the plugin configuration from version 2.5
// 41 = 2.51            version of the configuration containing a list of disabled icon overlay handlers (see CONFIG_DISABLEDCUSTICOVRLS_REG)
// 42 = 2.52b1          just for transferring the plugin configuration from version 2.51
// 43 = 2.52b2          just for transferring the plugin configuration from version 2.52 beta 1
// 44 = 2.52b2          change extension of viewers, editors, and archivators to lowercase (uppercase extensions are already in the windows survivor)
// 45 = 2.52b2          introduction of password manager, enforced load of FTP client to log in using Password Manager, see SetPluginUsesPasswordManager
// 46 = 2.52 (DB30) just for transferring the plugin configuration from version 2.52 beta 2
// 47 = 2.52 (IB31) support for Sal/Env variables such as $(SalDir) or $[USERPROFILE] in hot paths; we need to escape old hot paths
// 48 = 2.52            just for transferring the plugin configuration from version 2.52 (DB30)
// 49 = 2.53b1 (DB33) just for transferring plugin configuration from version 2.52
// 50 = 2.53b1 (DB36) just for transferring the plugin configuration from version 2.53b1 (DB33)
// 51 = 2.53b1 (PB38) just for transferring plugin configuration from version 2.53b1 (DB36)
// 52 = 2.53b1 (DB39) just for transferring the plugin configuration from version 2.53b1 (PB38)
// 53 = 2.53b1 (DB41)   just for transferring the plugin configuration from version 2.53b1 (DB39)
// 54 = 2.53b1 (PB44) just for transferring the plugin configuration from version 2.53b1 (DB41)
// 55 = 2.53b1 (DB46) just for transferring the plugin configuration from version 2.53b1 (PB44)
// 56 = 2.53b1          just for transferring the plugin configuration from version 2.53b1 (DB46)
// 57 = 2.53 (DB52) just for transferring the plugin configuration from version 2.53b1
// 58 = 2.53b2 (IB55) just for transferring the plugin configuration from version 2.53 (DB52)
// 59 = 2.53b2          just for transferring the plugin configuration from version 2.53b2 (IB55)
// 60 = 2.53 (DB60) just for transferring the plugin configuration from version 2.53b2
// 61 = 2.53            just for transferring the plugin configuration from version 2.53 (DB60)
// 62 = 2.54b1 (DB66) just for transferring plugin configuration from version 2.53
// 63 = 2.54 just for transferring the plugin configuration from version 2.54b1 (DB66)
// 64 = 2.55b1 (DB72) just for transferring the plugin configuration from version 2.54
// 65 = 2.55b1 (DB72) external archivers: identification by UID instead of Title (translated according to language version, therefore cannot be used for identification) - when switching languages, the settings for external archivers were lost
// 66 = 3.00b1 (PB75) just for transferring the plugin configuration from version 2.55b1 (DB72)
// 67 = 3.00b1 (DB76) just for transferring the plugin configuration from version 3.00b1 (PB75)
// 68 = 3.00b1 (PB79) just for transferring the plugin configuration from version 3.00b1 (DB76)
// 69 = 3.00b1 (DB80) just for transferring the plugin configuration from version 3.00b1 (PB79)
// 70 = 3.00b1 (DB83) just for transferring the plugin configuration from version 3.00b1 (DB80)
// 71 = 3.00b1 (PB87) just for transferring the plugin configuration from version 3.00b1 (DB83)
// 72 = 3.00b1 (DB88) just for transferring the plugin configuration from version 3.00b1 (PB87)
// 73 = 3.00b1          just for transferring the plugin configuration from version 3.00b1 (DB88)
// 74 = 3.00b2 (DB94) just for transferring the plugin configuration from version 3.00b1
// 75 = 3.00b2          just for transferring the plugin configuration from version 3.00b2 (DB94)
// 76 = 3.00b3 (DB100) just for transferring the plugin configuration from version 3.00b2
// 77 = 3.00b3 (PB103) just for transferring the plugin configuration from version 3.00b3 (DB100)
// 78 = 3.00b3 (DB105) just for transferring the plugin configuration from version 3.00b3 (PB103)
// 79 = 3.00b3          just for transferring the plugin configuration from version 3.00b3 (DB105)
// 80 = 3.00b4 (DB111) just for transferring the plugin configuration from version 3.00b3
// 81 = 3.00b4 (DB111) RAR 5.0 needs a new switch on the command line for encoding the file list file
// 82 = 3.00b4          just for transferring the plugin configuration from version 3.00b4 (DB111)
// 83 = 3.00b5 (DB117) just for transferring the plugin configuration from version 3.00b4
// 84 = 3.0             just for transferring the plugin configuration from version 3.00b5 (DB117)
// 85 = 3.10b1 (DB123) just for transferring the plugin configuration from version 3.0
// 86 = 3.01 just for transferring the plugin configuration from version 3.10b1 (DB123)
// 87 = 3.10b1 (DB129) just for transferring plugin configuration from version 3.01
// 88 = 3.02 just for transferring the plugin configuration from version 3.10b1 (DB129)
// 89 = 3.10b1 (DB135) just for transferring the plugin configuration from version 3.02
// 90 = 3.03            just for transferring the plugin configuration from version 3.10b1 (DB135)
// 91 = 3.10b1 (DB141) just for transferring plugin configuration from version 3.03
// 92 = 3.04 just for transferring the plugin configuration from version 3.10b1 (DB141)
// 93 = 3.10b1 (DB147) just for transferring the plugin configuration from version 3.04
// 94 = 3.05            just for transferring the plugin configuration from version 3.10b1 (DB147)
// 95 = 3.10b1 (DB153) just for transferring the plugin configuration from version 3.05
// 96 = 3.06 just for transferring the plugin configuration from version 3.10b1 (DB153)
// 97 = 3.10b1 (DB159) just for transferring the plugin configuration from version 3.06
// 98 = 3.10b1 (DB162) just for transferring the plugin configuration from version 3.10b1 (DB159)
// 99 = 3.07 just for transferring the plugin configuration from version 3.10b1 (DB162)
// 100 = 4.00b1 (DB168) just for transferring the plugin configuration from version 3.07
// 101 = 3.08 just for the transfer of the plugin configuration from version 4.00b1 (DB168) - by mistake they have 3.08 and DB171 the same version number 101, hopefully it doesn't matter, next time I will be more careful
// 101 = 4.00b1 (DB171) just for transferring the plugin configuration from version 4.00b1 (DB168), which is the last one from VC2008, the next versions are from VC2019
// 102 = 4.00b1 (DB177) just for transferring the plugin configuration from version 4.00b1 (DB171)
// 103 = 4.00           just for transferring the plugin configuration from version 4.00b1 (DB177)
// 104 = 5.00           just for transferring the plugin configuration from version 4.00, first Open Salamander
//
// When increasing the configuration version, it is necessary to add one to THIS_CONFIG_VERSION
//
// Pri prechodu na novou verzi programu je treba THIS_CONFIG_VERSION zvysit o 1,
// to perform automatic installation of new plug-ins and reset the last counter
// version plugins.ver.
//

const DWORD THIS_CONFIG_VERSION = 104;

// Roots of the configuration of individual versions of the Open Salamander program.
// The root of the current (youngest) configuration is at index 0.
// Then follow more roots towards older versions of the program.
// At the last index lies NULL, which serves as a terminator when working with an array.
// When creating a new version of the configuration (which is supposed to be separated from the previous one in the registry)
// just insert a line with the path at index 0.

// !!! At the same time, it is necessary to maintain corresponding lines in SalamanderConfigurationVersions
const char* SalamanderConfigurationRoots[SALCFG_ROOTS_COUNT + 1] =
    {
        "Software\\Open Salamander\\5.0",
        "Software\\Altap\\Altap Salamander 4.0",
        "Software\\Altap\\Altap Salamander 4.0 beta 1 (DB177)",
        "Software\\Altap\\Altap Salamander 4.0 beta 1 (DB171)",
        "Software\\Altap\\Altap Salamander 3.08",
        "Software\\Altap\\Altap Salamander 4.0 beta 1 (DB168)",
        "Software\\Altap\\Altap Salamander 3.07",
        "Software\\Altap\\Altap Salamander 3.1 beta 1 (DB162)",
        "Software\\Altap\\Altap Salamander 3.1 beta 1 (DB159)",
        "Software\\Altap\\Altap Salamander 3.06",
        "Software\\Altap\\Altap Salamander 3.1 beta 1 (DB153)",
        "Software\\Altap\\Altap Salamander 3.05",
        "Software\\Altap\\Altap Salamander 3.1 beta 1 (DB147)",
        "Software\\Altap\\Altap Salamander 3.04",
        "Software\\Altap\\Altap Salamander 3.1 beta 1 (DB141)",
        "Software\\Altap\\Altap Salamander 3.03",
        "Software\\Altap\\Altap Salamander 3.1 beta 1 (DB135)",
        "Software\\Altap\\Altap Salamander 3.02",
        "Software\\Altap\\Altap Salamander 3.1 beta 1 (DB129)",
        "Software\\Altap\\Altap Salamander 3.01",
        "Software\\Altap\\Altap Salamander 3.1 beta 1 (DB123)",
        "Software\\Altap\\Altap Salamander 3.0",
        "Software\\Altap\\Altap Salamander 3.0 beta 5 (DB117)",
        "Software\\Altap\\Altap Salamander 3.0 beta 4",
        "Software\\Altap\\Altap Salamander 3.0 beta 4 (DB111)",
        "Software\\Altap\\Altap Salamander 3.0 beta 3",
        "Software\\Altap\\Altap Salamander 3.0 beta 3 (DB105)",
        "Software\\Altap\\Altap Salamander 3.0 beta 3 (PB103)",
        "Software\\Altap\\Altap Salamander 3.0 beta 3 (DB100)",
        "Software\\Altap\\Altap Salamander 3.0 beta 2",
        "Software\\Altap\\Altap Salamander 3.0 beta 2 (DB94)",
        "Software\\Altap\\Altap Salamander 3.0 beta 1",
        "Software\\Altap\\Altap Salamander 3.0 beta 1 (DB88)",
        "Software\\Altap\\Altap Salamander 3.0 beta 1 (PB87)",
        "Software\\Altap\\Altap Salamander 3.0 beta 1 (DB83)",
        "Software\\Altap\\Altap Salamander 3.0 beta 1 (DB80)",
        "Software\\Altap\\Altap Salamander 3.0 beta 1 (PB79)",
        "Software\\Altap\\Altap Salamander 3.0 beta 1 (DB76)",
        "Software\\Altap\\Altap Salamander 3.0 beta 1 (PB75)",
        "Software\\Altap\\Altap Salamander 2.55 beta 1 (DB 72)",
        "Software\\Altap\\Altap Salamander 2.54",
        "Software\\Altap\\Altap Salamander 2.54 beta 1 (DB 66)",
        "Software\\Altap\\Altap Salamander 2.53",
        "Software\\Altap\\Altap Salamander 2.53 (DB 60)",
        "Software\\Altap\\Altap Salamander 2.53 beta 2",
        "Software\\Altap\\Altap Salamander 2.53 beta 2 (IB 55)",
        "Software\\Altap\\Altap Salamander 2.53 (DB 52)",
        "Software\\Altap\\Altap Salamander 2.53 beta 1",
        "Software\\Altap\\Altap Salamander 2.53 beta 1 (DB 46)",
        "Software\\Altap\\Altap Salamander 2.53 beta 1 (PB 44)",
        "Software\\Altap\\Altap Salamander 2.53 beta 1 (DB 41)",
        "Software\\Altap\\Altap Salamander 2.53 beta 1 (DB 39)",
        "Software\\Altap\\Altap Salamander 2.53 beta 1 (PB 38)",
        "Software\\Altap\\Altap Salamander 2.53 beta 1 (DB 36)",
        "Software\\Altap\\Altap Salamander 2.53 beta 1 (DB 33)",
        "Software\\Altap\\Altap Salamander 2.52",
        "Software\\Altap\\Altap Salamander 2.52 (DB 30)",
        "Software\\Altap\\Altap Salamander 2.52 beta 2",
        "Software\\Altap\\Altap Salamander 2.52 beta 1",
        "Software\\Altap\\Altap Salamander 2.51",
        "Software\\Altap\\Altap Salamander 2.5",
        "Software\\Altap\\Altap Salamander 2.5 RC3",
        "Software\\Altap\\Servant Salamander 2.5 RC3",
        "Software\\Altap\\Servant Salamander 2.5 RC2",
        "Software\\Altap\\Servant Salamander 2.5 RC1",
        "Software\\Altap\\Servant Salamander 2.5 beta 12",
        "Software\\Altap\\Servant Salamander 2.5 beta 11",
        "Software\\Altap\\Servant Salamander 2.5 beta 10",
        "Software\\Altap\\Servant Salamander 2.5 beta 9",
        "Software\\Altap\\Servant Salamander 2.5 beta 8",
        "Software\\Altap\\Servant Salamander 2.5 beta 7",
        "Software\\Altap\\Servant Salamander 2.5 beta 6",
        "Software\\Altap\\Servant Salamander 2.5 beta 5",
        "Software\\Altap\\Servant Salamander 2.5 beta 4",
        "Software\\Altap\\Servant Salamander 2.5 beta 3",
        "Software\\Altap\\Servant Salamander 2.5 beta 2",
        "Software\\Altap\\Servant Salamander 2.5 beta 1",
        "Software\\Altap\\Servant Salamander 2.1 beta 1",
        "Software\\Altap\\Servant Salamander 2.0",
        "Software\\Altap\\Servant Salamander 1.6 beta 7",
        "Software\\Altap\\Servant Salamander 1.6 beta 6",
        "Software\\Altap\\Servant Salamander", // 1.6 beta 1 to 1.6 beta 5
        "Software\\Salamander"                 // oldest version (1.52 and older)
};
const char* SalamanderConfigurationVersions[SALCFG_ROOTS_COUNT] =
    {
        "5.0",
        "4.0",
        "4.0 beta 1 (DB177)",
        "4.0 beta 1 (DB171)",
        "3.08",
        "4.0 beta 1 (DB168)",
        "3.07",
        "3.1 beta 1 (DB162)",
        "3.1 beta 1 (DB159)",
        "3.06",
        "3.1 beta 1 (DB153)",
        "3.05",
        "3.1 beta 1 (DB147)",
        "3.04",
        "3.1 beta 1 (DB141)",
        "3.03",
        "3.1 beta 1 (DB135)",
        "3.02",
        "3.1 beta 1 (DB129)",
        "3.01",
        "3.1 beta 1 (DB123)",
        "3.0",
        "3.0 beta 5 (DB117)",
        "3.0 beta 4",
        "3.0 beta 4 (DB111)",
        "3.0 beta 3",
        "3.0 beta 3 (DB105)",
        "3.0 beta 3 (PB103)",
        "3.0 beta 3 (DB100)",
        "3.0 beta 2",
        "3.0 beta 2 (DB94)",
        "3.0 beta 1",
        "3.0 beta 1 (DB88)",
        "3.0 beta 1 (PB87)",
        "3.0 beta 1 (DB83)",
        "3.0 beta 1 (DB80)",
        "3.0 beta 1 (PB79)",
        "3.0 beta 1 (DB76)",
        "3.0 beta 1 (PB75)",
        "2.55 beta 1 (DB72)",
        "2.54",
        "2.54 beta 1 (DB66)",
        "2.53",
        "2.53 (DB60)",
        "2.53 beta 2",
        "2.53 beta 2 (IB55)",
        "2.53 (DB52)",
        "2.53 beta 1",
        "2.53 beta 1 (DB46)",
        "2.53 beta 1 (PB44)",
        "2.53 beta 1 (DB41)",
        "2.53 beta 1 (DB39)",
        "2.53 beta 1 (PB38)",
        "2.53 beta 1 (DB36)",
        "2.53 beta 1 (DB33)",
        "2.52",
        "2.52 (DB30)",
        "2.52 beta 2",
        "2.52 beta 1",
        "2.51",
        "2.5",
        "2.5 RC3",
        "2.5 RC3",
        "2.5 RC2",
        "2.5 RC1",
        "2.5 beta 12",
        "2.5 beta 11",
        "2.5 beta 10",
        "2.5 beta 9",
        "2.5 beta 8",
        "2.5 beta 7",
        "2.5 beta 6",
        "2.5 beta 5",
        "2.5 beta 4",
        "2.5 beta 3",
        "2.5 beta 2",
        "2.5 beta 1",
        "2.1 beta 1",
        "2.0",
        "1.6 beta 7",
        "1.6 beta 6",
        "1.6 beta 1-5",
        "1.52"};

const char* SALAMANDER_ROOT_REG = NULL; // will be set in salamdr1.cpp

const char* SALAMANDER_SAVE_IN_PROGRESS = "Save In Progress"; // the value exists only during the configuration saving (for detecting interruptions during the configuration saving -> damaged configuration)
BOOL IsSetSALAMANDER_SAVE_IN_PROGRESS = FALSE;                // TRUE = a value SALAMANDER_SAVE_IN_PROGRESS is created in the registry (detection of interruption of saving configuration)

const char* SALAMANDER_COPY_IS_OK = "Copy Is OK"; // only backup key: value exists only if the key is successfully copied completely

const char* SALAMANDER_AUTO_IMPORT_CONFIG = "AutoImportConfig"; // the value only exists during UPGRADE (the installation rolls the old version to the new version + in the key of the new version it saves this value directed to the key of the configuration of the old version, from where the configuration should be imported into the new version)

const char* FINDDIALOG_WINDOW_REG = "Find Dialog Window";
const char* SALAMANDER_WINDOW_REG = "Window";
const char* WINDOW_LEFT_REG = "Left";
const char* WINDOW_RIGHT_REG = "Right";
const char* WINDOW_TOP_REG = "Top";
const char* WINDOW_BOTTOM_REG = "Bottom";
const char* WINDOW_SPLIT_REG = "Split Position";
const char* WINDOW_BEFOREZOOMSPLIT_REG = "Before Zoom Split Position";
const char* WINDOW_SHOW_REG = "Show";
const char* FINDDIALOG_NAMEWIDTH_REG = "Name Width";

const char* SALAMANDER_LEFTP_REG = "Left Panel";
const char* SALAMANDER_RIGHTP_REG = "Right Panel";
const char* PANEL_PATH_REG = "Path";
const char* PANEL_VIEW_REG = "View Type";
const char* PANEL_SORT_REG = "Sort Type";
const char* PANEL_REVERSE_REG = "Reverse Sort";
const char* PANEL_DIRLINE_REG = "Directory Line";
const char* PANEL_STATUS_REG = "Status Line";
const char* PANEL_HEADER_REG = "Header Line";
const char* PANEL_FILTER_ENABLE = "Enable Filter";
const char* PANEL_FILTER_INVERSE = "Inverse Filter";
const char* PANEL_FILTERHISTORY_REG = "Filter History";
const char* PANEL_FILTER = "Filter";

const char* SALAMANDER_DEFDIRS_REG = "Default Directories";

const char* SALAMANDER_CONFIG_REG = "Configuration";
const char* CONFIG_SKILLLEVEL_REG = "Skill Level";
const char* CONFIG_FILENAMEFORMAT_REG = "File Name Format";
const char* CONFIG_SIZEFORMAT_REG = "Size Format";
const char* CONFIG_SELECTION_REG = "Select/Deselect Directories";
const char* CONFIG_LONGNAMES_REG = "Use Long File Names";
const char* CONFIG_RECYCLEBIN_REG = "Use Recycle Bin";
const char* CONFIG_RECYCLEMASKS_REG = "Use Recycle Bin For";
const char* CONFIG_SAVEONEXIT_REG = "Save Configuration On Exit";
const char* CONFIG_SHOWGREPERRORS_REG = "Show Errors In Find Files";
const char* CONFIG_FINDFULLROW_REG = "Show Full Row In Find Files";
const char* CONFIG_MINBEEPWHENDONE_REG = "Use Speeker Beep";
const char* CONFIG_INTRN_VIEWER_REG = "Internal Viewer";
const char* CONFIG_VIEWER_REG = "External Viewer";
const char* CONFIG_EDITOR_REG = "External Editor";
const char* CONFIG_CMDLINE_REG = "Command Line";
const char* CONFIG_CMDLFOCUS_REG = "Command Line Focused";
const char* CONFIG_CLOSESHELL_REG = "Close Shell Window";
const char* CONFIG_USECUSTOMPANELFONT_REG = "Use Custom Panel Font";
const char* CONFIG_PANELFONT_REG = "Panel Font";
const char* CONFIG_NAMEDHISTORY_REG = "Named History";
const char* CONFIG_LOOKINHISTORY_REG = "Look In History";
const char* CONFIG_GREPHISTORY_REG = "Grep History";
const char* CONFIG_VIEWERHISTORY_REG = "Viewer History";
const char* CONFIG_COMMANDHISTORY_REG = "Command History";
const char* CONFIG_SELECTHISTORY_REG = "Select History";
const char* CONFIG_COPYHISTORY_REG = "Copy History";
const char* CONFIG_CHANGEDIRHISTORY_REG = "ChangeDir History";
const char* CONFIG_FILELISTHISTORY_REG = "File List History";
const char* CONFIG_CREATEDIRHISTORY_REG = "Create Directory History";
const char* CONFIG_QUICKRENAMEHISTORY_REG = "Quick Rename History";
const char* CONFIG_EDITNEWHISTORY_REG = "Edit New History";
const char* CONFIG_CONVERTHISTORY_REG = "Convert History";
const char* CONFIG_FILTERHISTORY_REG = "Filter History";
const char* CONFIG_WORKDIRSHISTORY_REG = "Working Directories";
const char* CONFIG_FILELISTNAME_REG = "Make File List Name";
const char* CONFIG_FILELISTAPPEND_REG = "Make File List Append";
const char* CONFIG_FILELISTDESTINATION_REG = "Make File List Destination";
const char* CONFIG_COPYFINDTEXT_REG = "Copy Find Text";
const char* CONFIG_CLEARREADONLY_REG = "Clear Readonly Attribute";
const char* CONFIG_PRIMARYCONTEXTMENU_REG = "Primary Context Menu";
const char* CONFIG_NOTHIDDENSYSTEM_REG = "Hide Hidden and System Files and Directories";
const char* CONFIG_RIGHT_FOCUS_REG = "Right Panel Focused";
const char* CONFIG_SHOWCHDBUTTON_REG = "Show Change Drive Button";
const char* CONFIG_ALWAYSONTOP_REG = "Always On Top";
//const char *CONFIG_FASTDIRMOVE_REG = "Fast Directory Move";
const char* CONFIG_SORTUSESLOCALE_REG = "Sort Uses Locale";
const char* CONFIG_SORTDETECTNUMBERS_REG = "Sort Detects Numbers";
const char* CONFIG_SORTNEWERONTOP_REG = "Sort Newer On Top";
const char* CONFIG_SORTDIRSBYNAME_REG = "Sort Dirs By Name";
const char* CONFIG_SORTDIRSBYEXT_REG = "Sort Dirs By Ext";
const char* CONFIG_SAVEHISTORY_REG = "Save History";
const char* CONFIG_SAVEWORKDIRS_REG = "Save Working Dirs";
const char* CONFIG_ENABLECMDLINEHISTORY_REG = "Enable CmdLine History";
const char* CONFIG_SAVECMDLINEHISTORY_REG = "Save CmdLine History";
//const char *CONFIG_LANTASTICCHECK_REG = "Lantastic Check";
const char* CONFIG_USESALOPEN_REG = "Use salopen.exe";
const char* CONFIG_NETWAREFASTDIRMOVE_REG = "Netware Fast Dir Move";
const char* CONFIG_ASYNCCOPYALG_REG = "Async Copy Alg On Network";
const char* CONFIG_RELOAD_ENV_VARS_REG = "Reload Environment Variables";
const char* CONFIG_QUICKRENAME_SELALL_REG = "Quick Rename Select All";
const char* CONFIG_EDITNEW_SELALL_REG = "Edit New File Select All";
const char* CONFIG_SHIFTFORHOTPATHS_REG = "Use Shift For GoTo HotPath";
const char* CONFIG_ONLYONEINSTANCE_REG = "Only One Instance";
const char* CONFIG_STATUSAREA_REG = "Status Area";
const char* CONFIG_SINGLECLICK_REG = "Single Click";
//const char *CONFIG_SHOWTIPOFTHEDAY_REG = "Show tip of the Day";
//const char *CONFIG_LASTTIPOFTHEDAY_REG = "Last tip of the Day";
const char* CONFIG_TOPTOOLBAR_REG = "Top ToolBar";
const char* CONFIG_MIDDLETOOLBAR_REG = "Middle ToolBar";
const char* CONFIG_LEFTTOOLBAR_REG = "Left ToolBar";
const char* CONFIG_RIGHTTOOLBAR_REG = "Right ToolBar";
const char* CONFIG_TOPTOOLBARVISIBLE_REG = "Show Top ToolBar";
const char* CONFIG_PLGTOOLBARVISIBLE_REG = "Show Plugins Bar";
const char* CONFIG_MIDDLETOOLBARVISIBLE_REG = "Show Middle ToolBar";
const char* CONFIG_USERMENUTOOLBARVISIBLE_REG = "Show User Menu ToolBar";
const char* CONFIG_HOTPATHSBARVISIBLE_REG = "Hot Paths Bar";
const char* CONFIG_DRIVEBARVISIBLE_REG = "Show Drive Bar";
const char* CONFIG_DRIVEBAR2VISIBLE_REG = "Show Drive Bar2";
const char* CONFIG_BOTTOMTOOLBARVISIBLE_REG = "Show Bottom ToolBar";
const char* CONFIG_EXPLORERLOOK_REG = "Explorer Look";
const char* CONFIG_FULLROWSELECT_REG = "Full Row Select";
const char* CONFIG_FULLROWHIGHLIGHT_REG = "Full Row Highlight";
const char* CONFIG_USEICONTINCTURE_REG = "Use Icon Tincture";
const char* CONFIG_SHOWPANELCAPTION_REG = "Show Panel Caption";
const char* CONFIG_SHOWPANELZOOM_REG = "Show Panel Zoom";
const char* CONFIG_INFOLINECONTENT_REG = "Information Line Content";
const char* CONFIG_IFPATHISINACCESSIBLEGOTOISMYDOCS_REG = "If Path Is Inaccessible Go To My Docs";
const char* CONFIG_IFPATHISINACCESSIBLEGOTO_REG = "If Path Is Inaccessible Go To";
const char* CONFIG_HOTPATH_AUTOCONFIG = "Auto Configurate Hot Paths";
const char* CONFIG_LASTUSEDSPEEDLIM_REG = "Speed Limit";
const char* CONFIG_QUICKSEARCHENTER_REG = "Quick Search Enter Alt";
const char* CONFIG_CHD_SHOWMYDOC = "Change Drive Show My Documents";
const char* CONFIG_CHD_SHOWANOTHER = "Change Drive Show Another";
const char* CONFIG_CHD_SHOWCLOUDSTOR = "Change Drive Show Cloud Storages";
const char* CONFIG_CHD_SHOWNET = "Change Drive Network";
const char* CONFIG_CURRRENTTIPINDEX = "Current Tip Index";
const char* CONFIG_SEARCHFILECONTENT = "Search File Content";
const char* CONFIG_FINDOPTIONS_REG = "Find Options";
const char* CONFIG_FINDIGNORE_REG = "Find Ignore";
#ifdef _WIN64
const char* CONFIG_LASTPLUGINVER = "Plugins.ver Version (x64)";
const char* CONFIG_LASTPLUGINVER_OP = "Plugins.ver Version (x86)";
#else  // _WIN64
const char* CONFIG_LASTPLUGINVER = "Plugins.ver Version (x86)";
const char* CONFIG_LASTPLUGINVER_OP = "Plugins.ver Version (x64)";
#endif // _WIN64
const char* CONFIG_LANGUAGE_REG = "Language";
const char* CONFIG_SHOWSPLASHSCREEN_REG = "Show Splash Screen";
const char* CONFIG_CONVERSIONTABLE_REG = "Conversion Table";
const char* CONFIG_TITLEBARSHOWPATH_REG = "Title bar show path";
const char* CONFIG_TITLEBARMODE_REG = "Title bar mode";
const char* CONFIG_TITLEBARPREFIX_REG = "Title bar prefix";
const char* CONFIG_TITLEBARPREFIXTEXT_REG = "Title bar prefix text";
const char* CONFIG_MAINWINDOWICONINDEX_REG = "Main window icon index";
const char* CONFIG_CLICKQUICKRENAME_REG = "Click to Quick Rename";
const char* CONFIG_VISIBLEDRIVES_REG = "Visible Drives";
const char* CONFIG_SEPARATEDDRIVES_REG = "Separated Drives";

const char* CONFIG_COMPAREBYTIME_REG = "Compare By Time";
const char* CONFIG_COMPAREBYSIZE_REG = "Compare By Size";
const char* CONFIG_COMPAREBYCONTENT_REG = "Compare By Content";
const char* CONFIG_COMPAREBYATTR_REG = "Compare By Attr";
const char* CONFIG_COMPAREBYSUBDIRS_REG = "Compare By Subdirs";
const char* CONFIG_COMPAREBYSUBDIRSATTR_REG = "Compare By Subdirs Attr";
const char* CONFIG_COMPAREONEPANELDIRS_REG = "Compare One Panel Dirs";
const char* CONFIG_COMPAREMOREOPTIONS_REG = "Compare More Options";
const char* CONFIG_COMPAREIGNOREFILES_REG = "Compare Ignore Files";
const char* CONFIG_COMPAREIGNOREDIRS_REG = "Compare Ignore Dirs";
const char* CONFIG_CONFIGTIGNOREFILESMASKS_REG = "Compare Ignore Files Masks";
const char* CONFIG_CONFIGTIGNOREDIRSMASKS_REG = "Compare Ignore Dirs Masks";
const char* CONFIG_THUMBNAILSIZE_REG = "Thumbnail Size";
const char* CONFIG_ALTLANGFORPLUGINS_REG = "Alternate Language for Plugins";
const char* CONFIG_USEALTLANGFORPLUGINS_REG = "Use Alternate Language for Plugins";
const char* CONFIG_LANGUAGECHANGED_REG = "Language Changed";
const char* CONFIG_ENABLECUSTICOVRLS_REG = "Enable Custom Icon Overlays";
const char* CONFIG_DISABLEDCUSTICOVRLS_REG = "Disabled Custom Icon Overlays";
const char* CONFIG_COPYMOVEOPTIONS_REG = "Copy Move Options";
const char* CONFIG_KEEPPLUGINSSORTED_REG = "Keep Plugins Sorted";
const char* CONFIG_SHOWSLGINCOMPLETE_REG = "Show Translation Is Incomplete";

const char* CONFIG_EDITNEWFILE_USEDEFAULT_REG = "Edit New File Use Default";
const char* CONFIG_EDITNEWFILE_DEFAULT_REG = "Edit New File Default";

//const char *CONFIG_SPACESELCALCSPACE = "Space Selecting";
const char* CONFIG_USETIMERESOLUTION = "Use Time Resolution";
const char* CONFIG_TIMERESOLUTION = "Time Resolution";
const char* CONFIG_IGNOREDSTSHIFTS = "Ignore DST Shifts";

const char* CONFIG_USEDRAGDROPMINTIME = "Use DragDrop Min Time";
const char* CONFIG_DRAGDROPMINTIME = "DragDrop Min Time";

// Configuration dialog pages
const char* CONFIG_LASTFOCUSEDPAGE = "Last Focused Page";
const char* CONFIG_VIEWANDEDITEXPAND = "Viewers And Editors Expanded";
const char* CONFIG_PACKEPAND = "Packers And Unpackers Expanded";
const char* CONFIG_CONFIGURATION_HEIGHT = "Configuration Height";

const char* CONFIG_MENUINDEX_REG = "Menu Index";
const char* CONFIG_MENUBREAK_REG = "Menu Break";
const char* CONFIG_MENUWIDTH_REG = "Menu Width";
const char* CONFIG_TOOLBARINDEX_REG = "ToolBar Index";
const char* CONFIG_TOOLBARBREAK_REG = "ToolBar Break";
const char* CONFIG_TOOLBARWIDTH_REG = "ToolBar Width";
const char* CONFIG_PLUGINSBARINDEX_REG = "PluginsBar Index";
const char* CONFIG_PLUGINSBARBREAK_REG = "PluginsBar Break";
const char* CONFIG_PLUGINSBARWIDTH_REG = "PluginsBar Width";
const char* CONFIG_USERMENUINDEX_REG = "User Menu Index";
const char* CONFIG_USERMENUBREAK_REG = "User Menu Break";
const char* CONFIG_USERMENUWIDTH_REG = "User Menu Width";
const char* CONFIG_USERMENULABELS_REG = "User Menu Labels";
const char* CONFIG_HOTPATHSINDEX_REG = "Hot Paths Index";
const char* CONFIG_HOTPATHSBREAK_REG = "Hot Paths Break";
const char* CONFIG_HOTPATHSWIDTH_REG = "Hot Paths Width";
const char* CONFIG_DRIVEBARINDEX_REG = "Drive Bar Index";
const char* CONFIG_DRIVEBARBREAK_REG = "Drive Bar Break";
const char* CONFIG_DRIVEBARWIDTH_REG = "Drive Bar Width";
const char* CONFIG_GRIPSVISIBLE_REG = "Grips Visible";

const char* SALAMANDER_CONFIRMATION_REG = "Confirmation";
const char* CONFIG_CNFRM_FILEDIRDEL = "Files or Dirs Del";
const char* CONFIG_CNFRM_NEDIRDEL = "Non-empty Dir Del";
const char* CONFIG_CNFRM_FILEOVER = "File Overwrite";
const char* CONFIG_CNFRM_DIROVER = "Directory Overwrite";
const char* CONFIG_CNFRM_SHFILEDEL = "SH File Del";
const char* CONFIG_CNFRM_SHDIRDEL = "SH Dir Del";
const char* CONFIG_CNFRM_SHFILEOVER = "SH File Overwrite";
const char* CONFIG_CNFRM_NTFSPRESS = "NTFS Compress and Uncompress";
const char* CONFIG_CNFRM_NTFSCRYPT = "NTFS Encrypt and Decrypt";
const char* CONFIG_CNFRM_DAD = "Drag and Drop";
const char* CONFIG_CNFRM_CLOSEARCHIVE = "Close Archive";
const char* CONFIG_CNFRM_CLOSEFIND = "Close Find";
const char* CONFIG_CNFRM_STOPFIND = "Stop Find";
const char* CONFIG_CNFRM_CREATETARGETPATH = "Create Target Path";
const char* CONFIG_CNFRM_ALWAYSONTOP = "Always on Top";
const char* CONFIG_CNFRM_ONSALCLOSE = "Close Salamander";
const char* CONFIG_CNFRM_SENDEMAIL = "Send Email";
const char* CONFIG_CNFRM_ADDTOARCHIVE = "Add To Archive";
const char* CONFIG_CNFRM_CREATEDIR = "Create Dir";
const char* CONFIG_CNFRM_CHANGEDIRTC = "Change Dir TC";
const char* CONFIG_CNFRM_SHOWNAMETOCOMP = "Show Names To Compare";
const char* CONFIG_CNFRM_DSTSHIFTSIGNORED = "DST Shifts Ignored";
const char* CONFIG_CNFRM_DSTSHIFTSOCCURED = "DST Shifts Occured";
const char* CONFIG_CNFRM_COPYMOVEOPTIONSNS = "Copy Move Options Not Supported";

const char* SALAMANDER_DRVSPEC_REG = "Drive Special Settings";
const char* CONFIG_DRVSPEC_FLOPPY_MON = "Floppy Automatic Refresh";
const char* CONFIG_DRVSPEC_FLOPPY_SIMPLE = "Floppy Simple Icons";
const char* CONFIG_DRVSPEC_REMOVABLE_MON = "Removable Automatic Refresh";
const char* CONFIG_DRVSPEC_REMOVABLE_SIMPLE = "Removable Simple Icons";
const char* CONFIG_DRVSPEC_FIXED_MON = "Fixed Automatic Refresh";
const char* CONFIG_DRVSPEC_FIXED_SIMPLE = "Fixed Simple Icons";
const char* CONFIG_DRVSPEC_REMOTE_MON = "Remote Automatic Refresh";
const char* CONFIG_DRVSPEC_REMOTE_SIMPLE = "Remote Simple Icons";
const char* CONFIG_DRVSPEC_REMOTE_ACT = "Remote Do Not Refresh on Activation";
const char* CONFIG_DRVSPEC_CDROM_MON = "CDROM Automatic Refresh";
const char* CONFIG_DRVSPEC_CDROM_SIMPLE = "CDROM Simple Icons";

const char* SALAMANDER_HOTPATHS_REG = "Hot Paths";

const char* SALAMANDER_VIEWTEMPLATES_REG = "View Templates";

const char* SALAMANDER_VIEWER_REG = "Viewer";
const char* VIEWER_FINDFORWARD_REG = "Forward Direction";
const char* VIEWER_FINDWHOLEWORDS_REG = "Whole Words";
const char* VIEWER_FINDCASESENSITIVE_REG = "Case Sensitive";
const char* VIEWER_FINDTEXT_REG = "Find Text";
const char* VIEWER_FINDHEXMODE_REG = "HEX-mode";
const char* VIEWER_FINDREGEXP_REG = "Regular Expression";
const char* VIEWER_CONFIGCRLF_REG = "EOL CRLF";
const char* VIEWER_CONFIGCR_REG = "EOL CR";
const char* VIEWER_CONFIGLF_REG = "EOL LF";
const char* VIEWER_CONFIGNULL_REG = "EOL NULL";
const char* VIEWER_CONFIGTABSIZE_REG = "Tabelator Size";
const char* VIEWER_CONFIGDEFMODE_REG = "Default Mode";
const char* VIEWER_CONFIGTEXTMASK_REG = "Text Masks";
const char* VIEWER_CONFIGHEXMASK_REG = "Hex Masks";
const char* VIEWER_CONFIGUSECUSTOMFONT_REG = "Viewer Use Custom Font";
const char* VIEWER_CONFIGFONT_REG = "Viewer Font";
const char* VIEWER_WRAPTEXT_REG = "Wrap Text";
const char* VIEWER_CPAUTOSELECT_REG = "Auto-Select";
const char* VIEWER_DEFAULTCONVERT_REG = "Default Convert";
const char* VIEWER_AUTOCOPYSELECTION_REG = "Auto-Copy Selection";
const char* VIEWER_GOTOOFFSETISHEX_REG = "Go to Offset Is Hex";

const char* VIEWER_CONFIGSAVEWINPOS_REG = "Save Window Position";
const char* VIEWER_CONFIGWNDLEFT_REG = "Left";
const char* VIEWER_CONFIGWNDRIGHT_REG = "Right";
const char* VIEWER_CONFIGWNDTOP_REG = "Top";
const char* VIEWER_CONFIGWNDBOTTOM_REG = "Bottom";
const char* VIEWER_CONFIGWNDSHOW_REG = "Show";

const char* SALAMANDER_USERMENU_REG = "User Menu";
const char* USERMENU_ITEMNAME_REG = "Item Name";
const char* USERMENU_COMMAND_REG = "Command";
const char* USERMENU_ARGUMENTS_REG = "Arguments";
const char* USERMENU_INITDIR_REG = "Initial Directory";
const char* USERMENU_SHELL_REG = "Execute Through Shell";
const char* USERMENU_USEWINDOW_REG = "Open Shell Window";
const char* USERMENU_CLOSE_REG = "Close Shell Window";
const char* USERMENU_SEPARATOR_REG = "Separator";
const char* USERMENU_SHOWINTOOLBAR_REG = "Show In Toolbar";
const char* USERMENU_TYPE_REG = "Type";
const char* USERMENU_ICON_REG = "Icon";

const char* SALAMANDER_VIEWERS_REG = "Viewers";
const char* SALAMANDER_ALTVIEWERS_REG = "Alternative Viewers";
const char* VIEWERS_MASKS_REG = "Masks";
const char* VIEWERS_COMMAND_REG = USERMENU_COMMAND_REG;
const char* VIEWERS_ARGUMENTS_REG = USERMENU_ARGUMENTS_REG;
const char* VIEWERS_INITDIR_REG = USERMENU_INITDIR_REG;
const char* VIEWERS_TYPE_REG = "Type";

const char* SALAMANDER_IZIP_REG = "Internal ZIP Packer";

const char* SALAMANDER_EDITORS_REG = "Editors";
const char* EDITORS_MASKS_REG = VIEWERS_MASKS_REG;
const char* EDITORS_COMMAND_REG = USERMENU_COMMAND_REG;
const char* EDITORS_ARGUMENTS_REG = USERMENU_ARGUMENTS_REG;
const char* EDITORS_INITDIR_REG = USERMENU_INITDIR_REG;

const char* SALAMANDER_VERSION_REG = "Version";
const char* SALAMANDER_VERSIONREG_REG = "Configuration";

const char* SALAMANDER_CUSTOMCOLORS_REG = "Custom Colors";

// colors
const char* SALAMANDER_COLORS_REG = "Colors";
const char* SALAMANDER_CLR_FOCUS_ACTIVE_NORMAL_REG = "Focus Active Normal";
const char* SALAMANDER_CLR_FOCUS_ACTIVE_SELECTED_REG = "Focus Active Selected";
const char* SALAMANDER_CLR_FOCUS_INACTIVE_NORMAL_REG = "Focus Inactive Normal";
const char* SALAMANDER_CLR_FOCUS_INACTIVE_SELECTED_REG = "Focus Inactive Selected";
const char* SALAMANDER_CLR_FOCUS_BK_INACTIVE_NORMAL_REG = "Focus Bk Inactive Normal";
const char* SALAMANDER_CLR_FOCUS_BK_INACTIVE_SELECTED_REG = "Focus Bk Inactive Selected";

const char* SALAMANDER_CLR_ITEM_FG_NORMAL_REG = "Item Fg Normal";
const char* SALAMANDER_CLR_ITEM_FG_SELECTED_REG = "Item Fg Selected";
const char* SALAMANDER_CLR_ITEM_FG_FOCUSED_REG = "Item Fg Focused";
const char* SALAMANDER_CLR_ITEM_FG_FOCSEL_REG = "Item Fg Focused and Selected";
const char* SALAMANDER_CLR_ITEM_FG_HIGHLIGHT_REG = "Item Fg Highlight";

const char* SALAMANDER_CLR_ITEM_BK_NORMAL_REG = "Item Bk Normal";
const char* SALAMANDER_CLR_ITEM_BK_SELECTED_REG = "Item Bk Selected";
const char* SALAMANDER_CLR_ITEM_BK_FOCUSED_REG = "Item Bk Focused";
const char* SALAMANDER_CLR_ITEM_BK_FOCSEL_REG = "Item Bk Focused and Selected";
const char* SALAMANDER_CLR_ITEM_BK_HIGHLIGHT_REG = "Item Bk Highlight";

const char* SALAMANDER_CLR_ICON_BLEND_SELECTED_REG = "Icon Blend Selected";
const char* SALAMANDER_CLR_ICON_BLEND_FOCUSED_REG = "Icon Blend Focused";
const char* SALAMANDER_CLR_ICON_BLEND_FOCSEL_REG = "Icon Blend Focused and Selected";

const char* SALAMANDER_CLR_PROGRESS_FG_NORMAL_REG = "Progress Fg Normal";
const char* SALAMANDER_CLR_PROGRESS_FG_SELECTED_REG = "Progress Fg Selected";
const char* SALAMANDER_CLR_PROGRESS_BK_NORMAL_REG = "Progress Bk Normal";
const char* SALAMANDER_CLR_PROGRESS_BK_SELECTED_REG = "Progress Bk Selected";

const char* SALAMANDER_CLR_VIEWER_FG_NORMAL_REG = "Viewer Fg Normal";
const char* SALAMANDER_CLR_VIEWER_BK_NORMAL_REG = "Viewer Bk Normal";
const char* SALAMANDER_CLR_VIEWER_FG_SELECTED_REG = "Viewer Fg Selected";
const char* SALAMANDER_CLR_VIEWER_BK_SELECTED_REG = "Viewer Bk Selected";

const char* SALAMANDER_CLR_HOT_PANEL_REG = "Hot Panel";
const char* SALAMANDER_CLR_HOT_ACTIVE_REG = "Hot Active";
const char* SALAMANDER_CLR_HOT_INACTIVE_REG = "Hot Inactive";

const char* SALAMANDER_CLR_ACTIVE_CAPTION_FG_REG = "Active Caption Fg";
const char* SALAMANDER_CLR_ACTIVE_CAPTION_BK_REG = "Active Caption Bk";
const char* SALAMANDER_CLR_INACTIVE_CAPTION_FG_REG = "Inactive Caption Fg";
const char* SALAMANDER_CLR_INACTIVE_CAPTION_BK_REG = "Inactive Caption Bk";

const char* SALAMANDER_CLR_THUMBNAIL_FRAME_NORMAL_REG = "Thumbnail Frame Normal";
const char* SALAMANDER_CLR_THUMBNAIL_FRAME_SELECTED_REG = "Thumbnail Frame Selected";
const char* SALAMANDER_CLR_THUMBNAIL_FRAME_FOCUSED_REG = "Thumbnail Frame Focused";
const char* SALAMANDER_CLR_THUMBNAIL_FRAME_FOCSEL_REG = "Thumbnail Frame Focused and Selected";

const char* SALAMANDER_HLT = "Panel Items Hilighting";
const char* SALAMANDER_HLT_ITEM_MASKS = "Masks";
const char* SALAMANDER_HLT_ITEM_ATTR = "Attributes";
const char* SALAMANDER_HLT_ITEM_VALIDATTR = "Valid Attributes";
const char* SALAMANDER_HLT_ITEM_FG_NORMAL_REG = "Item Fg Normal";
const char* SALAMANDER_HLT_ITEM_FG_SELECTED_REG = "Item Fg Selected";
const char* SALAMANDER_HLT_ITEM_FG_FOCUSED_REG = "Item Fg Focused";
const char* SALAMANDER_HLT_ITEM_FG_FOCSEL_REG = "Item Fg Focused and Selected";
const char* SALAMANDER_HLT_ITEM_FG_HIGHLIGHT_REG = "Item Fg Highlight";
const char* SALAMANDER_HLT_ITEM_BK_NORMAL_REG = "Item Bk Normal";
const char* SALAMANDER_HLT_ITEM_BK_SELECTED_REG = "Item Bk Selected";
const char* SALAMANDER_HLT_ITEM_BK_FOCUSED_REG = "Item Bk Focused";
const char* SALAMANDER_HLT_ITEM_BK_FOCSEL_REG = "Item Bk Focused and Selected";
const char* SALAMANDER_HLT_ITEM_BK_HIGHLIGHT_REG = "Item Bk Highlight";

const char* SALAMANDER_CLRSCHEME_REG = "Color Scheme";

// Plugins
const char* SALAMANDER_PLUGINS = "Plugins";
const char* SALAMANDER_PLUGINS_NAME = "Name";
const char* SALAMANDER_PLUGINS_DLLNAME = "DLL";
const char* SALAMANDER_PLUGINS_VERSION = "Version";
const char* SALAMANDER_PLUGINS_COPYRIGHT = "Copyright";
const char* SALAMANDER_PLUGINS_EXTENSIONS = "Extensions";
const char* SALAMANDER_PLUGINS_DESCRIPTION = "Description";
const char* SALAMANDER_PLUGINS_LASTSLGNAME = "LastSLGName";
const char* SALAMANDER_PLUGINS_HOMEPAGE = "HomePage";
//const char *SALAMANDER_PLUGINS_PLGICONS = "PluginIcons";
const char* SALAMANDER_PLUGINS_PLGICONLIST = "PluginIconList";
const char* SALAMANDER_PLUGINS_PLGICONINDEX = "PluginIconIndex";
const char* SALAMANDER_PLUGINS_PLGSUBMENUICONINDEX = "SubmenuIconIndex";
const char* SALAMANDER_PLUGINS_SUBMENUINPLUGINSBAR = "SubmenuInPluginsBar";
const char* SALAMANDER_PLUGINS_THUMBMASKS = "ThumbnailMasks";
const char* SALAMANDER_PLUGINS_REGKEYNAME = "Configuration Key";
const char* SALAMANDER_PLUGINS_FSNAME = "FS Name";
const char* SALAMANDER_PLUGINS_FUNCTIONS = "Functions";
const char* SALAMANDER_PLUGINS_LOADONSTART = "Load On Start";
const char* SALAMANDER_PLUGINS_MENU = "Menu";
const char* SALAMANDER_PLUGINS_MENUITEMNAME = "Name";
const char* SALAMANDER_PLUGINS_MENUITEMSTATE = "State";
const char* SALAMANDER_PLUGINS_MENUITEMID = "ID";
const char* SALAMANDER_PLUGINS_MENUITEMSKILLLEVEL = "Skill";
const char* SALAMANDER_PLUGINS_MENUITEMICONINDEX = "Icon";
const char* SALAMANDER_PLUGINS_MENUITEMTYPE = "Type";
const char* SALAMANDER_PLUGINS_MENUITEMHOTKEY = "HotKey";
const char* SALAMANDER_PLUGINS_FSCMDNAME = "FS Cmd Name";
const char* SALAMANDER_PLUGINS_FSCMDICON = "FS Cmd Icon";
const char* SALAMANDER_PLUGINS_FSCMDVISIBLE = "FS Cmd Visible";
const char* SALAMANDER_PLUGINS_ISNETHOOD = "Is Nethood";
const char* SALAMANDER_PLUGINS_USESPASSWDMAN = "Uses Password Manager";

// Plugins: Note that 8 strings are only for converting from configuration version 6 and lower
const char* SALAMANDER_PLUGINS_PANELVIEW = "Panel List";
const char* SALAMANDER_PLUGINS_PANELEDIT = "Panel Pack";
const char* SALAMANDER_PLUGINS_CUSTPACK = "Custom Pack";
const char* SALAMANDER_PLUGINS_CUSTUNPACK = "Custom Unpack";
const char* SALAMANDER_PLUGINS_CONFIG = "Configuration";
const char* SALAMANDER_PLUGINS_LOADSAVE = "Persistent";
const char* SALAMANDER_PLUGINS_VIEWER = "File Viewer";
const char* SALAMANDER_PLUGINS_FS = "File System";

// Plugins Configuration
const char* SALAMANDER_PLUGINSCONFIG = "Plugins Configuration";

// Plugins Order
const char* SALAMANDER_PLUGINSORDER = "Plugins Order";
const char* SALAMANDER_PLUGINSORDER_SHOW = "ShowInBar";

// Packers & Unpackers
const char* SALAMANDER_PACKANDUNPACK = "Packers & Unpackers";
const char* SALAMANDER_CUSTOMPACKERS = "Custom Packers";
const char* SALAMANDER_CUSTOMUNPACKERS = "Custom Unpackers";
const char* SALAMANDER_PREDPACKERS = "Predefined Packers";
const char* SALAMANDER_ARCHIVEASSOC = "Archive Association";
// for SALAMANDER_CUSTOMPACKERS and SALAMANDER_CUSTOMUNPACKERS
const char* SALAMANDER_ANOTHERPANEL = "Use Another Panel";
const char* SALAMANDER_PREFFERED = "Preffered";
const char* SALAMANDER_NAMEBYARCHIVE = "Use Subdir Name By Archive";
const char* SALAMANDER_SIMPLEICONSINARCHIVES = "Simple Icons In Archives";

const char* SALAMANDER_PWDMNGR_REG = "Password Manager";

//****************************************************************************
//
// GetUpgradeInfo
//
// Try to find "AutoImportConfig" in the configuration key of this version of Salama. If it is not found
// or the key stored in AutoImportConfig does not exist, it points to the key of this version (which is nonsense)
// or contains a corrupted (incomplete saving of configuration) or empty configuration, returns
// in 'autoImportConfig' FALSE. Otherwise, it returns TRUE in 'autoImportConfig'.
// in 'autoImportConfigFromKey' returns the path to the key from which the configuration should be imported.
// Handles situations where AutoImportConfig points to a key that contains AutoImportConfig
// to the next key (we will only go through the "target" key and leave intermediate keys unchanged - if
// if the import is successful, we will still delete the target key). Returns FALSE only if the software is to exit.
//
// If the configuration key of this version of Salama contains not only AutoImportConfig but also
// key "Configuration" (we expect it to be a saved configuration), we will ask the user if they want to:
//   -use current configuration and ignore configuration of the old version (I wouldn't delete it, although
//    it would be more logical, but the poor could lose data and again so much those registries
//    do not clutter) - in this case, immediately delete AutoImportConfig
//    (this is done quietly if AutoImportConfig leads to the configuration key of this version of Salama)
//    (We offer defaults by default, as it does not lead to data loss, people tend to dismiss message boxes without reading)
//   -delete current configuration and import configuration from old version - in that case immediately
//    delete everything except AutoImportConfig
//   -exit software - just return FALSE

BOOL GetUpgradeInfo(BOOL* autoImportConfig, char* autoImportConfigFromKey, int autoImportConfigFromKeySize)
{
    HKEY rootKey;
    DWORD saveInProgress; // dummy
    BOOL doNotExit = TRUE;
    if (autoImportConfigFromKeySize > 0)
        *autoImportConfigFromKey = 0;
    LoadSaveToRegistryMutex.Enter();
    int rounds = 0; // Prevention of cycling
    *autoImportConfig = FALSE;
    if (HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, SalamanderConfigurationRoots[0], 0,
                               KEY_READ, &rootKey)) == ERROR_SUCCESS)
    {
        HKEY oldCfgKey;
        char oldKeyName[200];

        if (GetValue(rootKey, SALAMANDER_AUTO_IMPORT_CONFIG, REG_SZ, oldKeyName, 200))
        { // We found "AutoImportConfig"
        OPEN_AUTO_IMPORT_CONFIG_KEY:
            lstrcpyn(autoImportConfigFromKey, SalamanderConfigurationRoots[0], autoImportConfigFromKeySize);
            if (CutDirectory(autoImportConfigFromKey) &&
                SalPathAppend(autoImportConfigFromKey, oldKeyName, autoImportConfigFromKeySize) &&
                !IsTheSamePath(autoImportConfigFromKey, SalamanderConfigurationRoots[0]) &&     // Key stored in AutoImportConfig does not point to the key of this version
                HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, autoImportConfigFromKey, 0, KEY_READ, // Key stored in AutoImportConfig can be opened (otherwise it does not exist?)
                                       &oldCfgKey)) == ERROR_SUCCESS)
            {
                // if the current "target" key contains AutoImportConfig, we will go through them...
                if (GetValue(oldCfgKey, SALAMANDER_AUTO_IMPORT_CONFIG, REG_SZ, oldKeyName, 200) && ++rounds <= 50)
                {
                    HANDLES(RegCloseKey(oldCfgKey));
                    goto OPEN_AUTO_IMPORT_CONFIG_KEY;
                }
                HKEY cfgKey;
                if (rounds <= 50 &&
                    !GetValue(oldCfgKey, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD)) &&
                    HANDLES_Q(RegOpenKeyEx(oldCfgKey, SALAMANDER_CONFIG_REG, 0, KEY_READ, &cfgKey)) == ERROR_SUCCESS)
                {
                    HANDLES(RegCloseKey(cfgKey));
                    *autoImportConfig = TRUE; // It's not about damaged or empty configuration
                }
                HANDLES(RegCloseKey(oldCfgKey));
            }
        }
        if (*autoImportConfig) // we will check if the key of this version does not contain the configuration as well (except for "AutoImportConfig")
        {
            HKEY cfgKey;
            lstrcpyn(oldKeyName, SalamanderConfigurationRoots[0], 200);
            if (SalPathAppend(oldKeyName, SALAMANDER_CONFIG_REG, 200) &&
                HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, oldKeyName, 0, KEY_READ, &cfgKey)) == ERROR_SUCCESS)
            {
                HANDLES(RegCloseKey(cfgKey));
                BOOL clearCfg = FALSE;
                if (!GetValue(rootKey, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD)))
                { // the key of this version contains an intact configuration, we will ask the user what to do with it
                    HANDLES(RegCloseKey(rootKey));
                    rootKey = NULL;
                    LoadSaveToRegistryMutex.Leave();

                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = NULL;
                    params.Flags = MB_ABORTRETRYIGNORE | MB_ICONQUESTION | MB_SETFOREGROUND;
                    params.Caption = SALAMANDER_TEXT_VERSION;
                    lstrcpyn(oldKeyName, autoImportConfigFromKey, 200);
                    char* keyName;
                    if (!CutDirectory(oldKeyName, &keyName))
                        keyName = oldKeyName; // (theoretically cannot occur)
                    char buf[1000];
                    sprintf(buf, "You have upgraded from %s (old version) to %s (new version). The configuration of the old "
                                 "version should be imported to the new version now, but there is already existing "
                                 "configuration for the new version. You can use this existing configuration (the configuration of "
                                 "the old version remains in registry, so you can import it later). Or you can overwrite "
                                 "this existing configuration (it would be lost) with the configuration of the old version. "
                                 "Or you can exit Open Salamander and solve this problem later.",
                            keyName, SALAMANDER_TEXT_VERSION);
                    params.Text = buf;
                    char aliasBtnNames[200];
                    sprintf(aliasBtnNames, "%d\t%s\t%d\t%s\t%d\t%s",
                            DIALOG_ABORT, "&Use Existing Configuration",
                            DIALOG_RETRY, "&Overwrite Existing Configuration",
                            DIALOG_IGNORE, "&Exit");
                    params.AliasBtnNames = aliasBtnNames;
                    int res = SalMessageBoxEx(&params);
                    switch (res)
                    {
                    case DIALOG_ABORT:
                        *autoImportConfig = FALSE;
                        break;
                    case DIALOG_RETRY:
                        clearCfg = TRUE;
                        break;

                    // case DIALOG_IGNORE:
                    default:
                        doNotExit = FALSE;
                        break;
                    }

                    LoadSaveToRegistryMutex.Enter();
                }
                else
                    clearCfg = TRUE; // Occupied configuration is damaged, we will delete it
                if (clearCfg &&
                    HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, SalamanderConfigurationRoots[0], 0,
                                           KEY_READ | KEY_WRITE, &cfgKey)) == ERROR_SUCCESS)
                { // We will delete the configuration, leaving (or rather recreating) only "AutoImportConfig" there
                    ClearKey(cfgKey);
                    lstrcpyn(oldKeyName, autoImportConfigFromKey, 200);
                    char* keyName;
                    if (!CutDirectory(oldKeyName, &keyName))
                        keyName = oldKeyName; // (theoretically cannot occur)
                    SetValue(cfgKey, SALAMANDER_AUTO_IMPORT_CONFIG, REG_SZ, keyName, -1);
                    HANDLES(RegCloseKey(cfgKey));
                }
            }
        }
        if (rootKey != NULL)
            HANDLES(RegCloseKey(rootKey));
    }
    if (!*autoImportConfig && // the key of this version does not contain "AutoImportConfig" or does not point to a "valid" old configuration
        HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, SalamanderConfigurationRoots[0], 0,
                               KEY_READ | KEY_WRITE, &rootKey)) == ERROR_SUCCESS)
    { // In the key of this version, we will remove the value "AutoImportConfig" (if it is there at all, it doesn't make sense there).
        RegDeleteValue(rootKey, SALAMANDER_AUTO_IMPORT_CONFIG);
        HANDLES(RegCloseKey(rootKey));
    }
    LoadSaveToRegistryMutex.Leave();
    return doNotExit;
}

//****************************************************************************
//
// FindLanguageFromPrevVerOfSal
//
// Extracts the language from an older version of Salamander (using the .slg module). The oldest version,
// from which we obtain this information is 2.53 beta 2 (first version delivered with multiple languages: CZ+DE+EN).
// If there is a configuration of the current version or if such a language is not found, it returns FALSE.
// Otherwise, the function returns the language in 'slgName' (buffer of MAX_PATH characters).

BOOL FindLanguageFromPrevVerOfSal(char* slgName)
{
    HKEY hCfgKey;
    HKEY hRootKey;
    int rootIndex = 0;
    const char* root;
    DWORD saveInProgress; // dummy

    slgName[0] = 0;
    LoadSaveToRegistryMutex.Enter();
    do
    {
        // check if the key exists and if the configuration is stored under it
        root = SalamanderConfigurationRoots[rootIndex];
        BOOL rootFound = HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, root, 0, KEY_READ, &hRootKey)) == ERROR_SUCCESS;
        BOOL cfgFound = rootFound && HANDLES_Q(RegOpenKeyEx(hRootKey, SALAMANDER_CONFIG_REG, 0,
                                                            KEY_READ, &hCfgKey)) == ERROR_SUCCESS;
        if (cfgFound && GetValue(hRootKey, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD)))
        { // This is a damaged configuration
            cfgFound = FALSE;
            HANDLES(RegCloseKey(hCfgKey));
        }
        DWORD configVersion = 1; // This is config from 1.52 and older
        if (cfgFound)
        {
            HKEY actKey;
            if (HANDLES_Q(RegOpenKeyEx(hRootKey, SALAMANDER_VERSION_REG, 0, KEY_READ, &actKey) == ERROR_SUCCESS))
            {
                configVersion = 2; // This is the config from 1.6b1
                GetValue(actKey, SALAMANDER_VERSIONREG_REG, REG_DWORD, &configVersion, sizeof(DWORD));
                HANDLES(RegCloseKey(actKey));
            }
        }
        if (rootFound)
            HANDLES(RegCloseKey(hRootKey));
        if (cfgFound)
        {
            BOOL found = FALSE;
            if (rootIndex != 0 &&                      // only if it is one of the older keys
                configVersion >= 59 /* 2.53 beta 2*/) // Before 2.53 beta 2, there was only English, so reading doesn't make sense, we will offer the user the default system language or manual language selection
            {
                GetValue(hCfgKey, CONFIG_LANGUAGE_REG, REG_SZ, slgName, MAX_PATH);
                found = slgName[0] != 0;
            }
            HANDLES(RegCloseKey(hCfgKey));
            LoadSaveToRegistryMutex.Leave();
            return found;
        }
        rootIndex++;
    } while (rootIndex < SALCFG_ROOTS_COUNT);

    LoadSaveToRegistryMutex.Leave();
    return FALSE;
}

// extracts a number from a string (decimal number format without a sign); returns TRUE if a number was found;
// ignores white-spaces before and after the number
BOOL GetNumFromStr(const char* s, DWORD* retNum)
{
    DWORD n = 0;
    while (*s != 0 && *s <= ' ')
        s++;
    BOOL mayBeOK = *s >= '0' && *s <= '9';
    while (*s >= '0' && *s <= '9')
        n = 10 * n + (*s++ - '0');
    while (*s != 0 && *s <= ' ')
        s++;
    *retNum = n;
    return mayBeOK && *s == 0;
}

void CheckShutdownParams()
{
    // HKEY_CURRENT_USER\Control Panel\Desktop\WaitToKillAppTimeout=20000,REG_SZ  ... less than 20000, break!
    // HKEY_CURRENT_USER\Control Panel\Desktop\AutoEndTasks=0,REG_SZ ... not 0, tear it apart!
    // W2K and XP have it, I didn't find it on Vista, but reportedly it's there too (info from the internet)

    BOOL showWarning = FALSE;
    HKEY key;
    if (OpenKeyAux(NULL, HKEY_CURRENT_USER, "Control Panel\\Desktop", key))
    {
        char num[100];
        DWORD value;
        if (GetValueAux(NULL, key, "WaitToKillAppTimeout", REG_SZ, num, 100) &&
            GetNumFromStr(num, &value) && value < 20000)
        {
            TRACE_E("CheckShutdownParams(): WaitToKillAppTimeout is '" << num << "' (" << value << ")");
            showWarning = TRUE;
        }
        if (GetValueAux(NULL, key, "AutoEndTasks", REG_SZ, num, 100) &&
            GetNumFromStr(num, &value) && value != 0)
        {
            TRACE_E("CheckShutdownParams(): AutoEndTasks is '" << num << "' (" << value << ")");
            showWarning = TRUE;
        }
        CloseKeyAux(key);
    }

    if (showWarning)
        SalMessageBox(NULL, LoadStr(IDS_CHANGEDSHUTDOWNPARS), SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONWARNING);
}

BOOL MyRegRenameKey(HKEY key, const char* name, const char* newName)
{
    BOOL ret = FALSE;
    // There is also NtRenameKey, but I couldn't get it to work (it requires UNICODE_STRING)
    // and probably also the key opened via NtOpenKey, to which the key is passed through OBJECT_ATTRIBUTES
    // initialized via InitializeObjectAttributes), it's unnecessarily complicated, it doesn't work
    // The frequently used code, I will solve it the slow but easy way... I will copy the key to
    // new, and then delete the original
    HKEY newKey;
    if (!OpenKeyAux(NULL, key, newName, newKey)) // test non-existence of the target key
    {
        if (CreateKeyAux(NULL, key, newName, newKey)) // creating the target key
        {
            // zkousel jsem i RegCopyTree (bez KEY_ALL_ACCESS neslapalo) a rychlost byla stejna jako SHCopyKey
            if (SHCopyKey(key, name, newKey, 0) == ERROR_SUCCESS) // copy to the target key
                ret = TRUE;
            CloseKeyAux(newKey);
            if (ret)
                SHDeleteKey(key, name);
        }
        else
            TRACE_E("MyRegRenameKey(): unable to create target key: " << newName);
    }
    else
    {
        CloseKeyAux(newKey);
        TRACE_E("MyRegRenameKey(): target key already exists: " << newName);
    }
    return ret;
}

//****************************************************************************
//
// FindLatestConfiguration
//
// Attempt to find the configuration corresponding to our version of the program.
// If it is found, the variable 'loadConfiguration' will be set and the function will return
// TRUE. If the configuration does not exist yet, the function will gradually search the old ones
// Configuration from the 'SalamanderConfigurationRoots' array (from the youngest to the oldest).
// If it finds any of the configurations, it displays a dialog and offers to convert it to
// configuration current and deletion from the registry. After displaying the last dialog, it returns
// TRUE sets the variables 'deleteConfigurations' and 'loadConfiguration' according to the options
// user. If the user chooses to terminate the application, the function returns FALSE.
//

BOOL FindLatestConfiguration(BOOL* deleteConfigurations, const char*& loadConfiguration)
{
    HKEY hRootKey;
    loadConfiguration = NULL; // We do not want to load any configuration - default values will be used
    int rootIndex = 0;
    const char* root;
    DWORD saveInProgress; // dummy
    HKEY hCfgKey;

    CImportConfigDialog dlg;
    ZeroMemory(dlg.ConfigurationExist, sizeof(dlg.ConfigurationExist)); // no configuration found
    dlg.DeleteConfigurations = deleteConfigurations;
    dlg.IndexOfConfigurationToLoad = -1;

    BOOL offerImportDlg = FALSE; // if there is an old configuration or keys, we offer import

    LoadSaveToRegistryMutex.Enter();

    char backup[200];
    sprintf_s(backup, "%s.backup.63A7CD13", SalamanderConfigurationRoots[0]); // "63A7CD13" is a prevention of key name collision with user
    HKEY backupKey;
    BOOL backupFound = OpenKeyAux(NULL, HKEY_CURRENT_USER, backup, backupKey);
    if (backupFound)
    {
        DWORD copyIsOK;
        if (GetValueAux(NULL, backupKey, SALAMANDER_COPY_IS_OK, REG_DWORD, &copyIsOK, sizeof(DWORD)))
            copyIsOK = 1; // backup is OK
        else
            copyIsOK = 0; // backup is faulty
        HANDLES(RegCloseKey(backupKey));
        if (!copyIsOK) // We will delete the faulty backup, pretending that it never existed (probably it just didn't have time to be fully created).
        {
            TRACE_I("Configuration backup is incomplete, removing... " << backup);
            SHDeleteKey(HKEY_CURRENT_USER, backup);
            backupFound = FALSE;
        }
        else
            TRACE_I("Configuration backup is OK: " << backup);
    }

    do
    {
        root = SalamanderConfigurationRoots[rootIndex];
        // check if the key exists
        BOOL rootFound = OpenKeyAux(NULL, HKEY_CURRENT_USER, root, hRootKey);
        if (rootFound &&
            GetValueAux(NULL, hRootKey, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD)))
        { // This is a damaged configuration
            TRACE_E("Configuration is corrupted!");
            rootFound = FALSE;
            CloseKeyAux(hRootKey);
            if (rootIndex == 0 && backupFound) // we will use the backup if we have it and we won't bother the user with it
            {
                char corrupted[200];
                sprintf_s(corrupted, "%s.corrupted.63A7CD13", root); // "63A7CD13" is a prevention of key name collision with user
                SHDeleteKey(HKEY_CURRENT_USER, corrupted);           // if we already have a corrupted configuration, we remove it, just one is enough
                if (MyRegRenameKey(HKEY_CURRENT_USER, root, corrupted) &&
                    MyRegRenameKey(HKEY_CURRENT_USER, backup, root))
                {
                    backupFound = FALSE;
                    if (CreateKeyAux(NULL, HKEY_CURRENT_USER, root, hRootKey))
                    {
                        DeleteValueAux(hRootKey, SALAMANDER_COPY_IS_OK);
                        CloseKeyAux(hRootKey);
                    }
                    TRACE_I("Corrupted configuration was moved to: " << corrupted);
                    TRACE_I("Using configuration backup instead ...");
                    continue; // In the second round, we will load the configuration from the backup created during the "critical shutdown."
                }
                else
                    TRACE_E("Unable to move corrupted configuration or configuration backup.");
            }

            if (rootIndex == 0) // In the active version of the program, we inform the user about the damaged configuration and let them back up the key, then we try to delete it (in older versions, we simply ignore this damaged configuration).
            {
                char buf[1500];
                _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_CORRUPTEDCONFIGFOUND), root);
                LoadSaveToRegistryMutex.Leave();

                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = NULL;
                params.Flags = MB_OKCANCEL | MB_ICONERROR | MB_DEFBUTTON2;
                params.Caption = SALAMANDER_TEXT_VERSION;
                params.Text = buf;
                char aliasBtnNames[200];
                /* used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
we will let the collision of hotkeys for the message box buttons be solved by simulating that it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_CORRUPTEDCONFIGREMOVEBTN
  {MNTT_IT, IDS_SELLANGEXITBUTTON
  {MNTT_PE, 0
};*/
                sprintf(aliasBtnNames, "%d\t%s\t%d\t%s", DIALOG_OK, LoadStr(IDS_CORRUPTEDCONFIGREMOVEBTN),
                        DIALOG_CANCEL, LoadStr(IDS_SELLANGEXITBUTTON));
                params.AliasBtnNames = aliasBtnNames;
                if (SalMessageBoxEx(&params) == IDCANCEL)
                {
                    CheckShutdownParams(); // Alternatively, we could also display this warning (if I rename the key in the registry, you might not encounter this message at all)
                    return FALSE;          // Exit
                }

                CheckShutdownParams();
                LoadSaveToRegistryMutex.Enter();
                if (HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, root, 0, KEY_READ | KEY_WRITE, &hRootKey)) == ERROR_SUCCESS)
                { // we will delete the damaged configuration (if it is still there - i.e. the user did not rename it for backup)
                    TRACE_I("Deleting corrupted configuration on user demand: " << root);
                    ClearKeyAux(hRootKey);
                    CloseKeyAux(hRootKey);
                    DeleteKeyAux(HKEY_CURRENT_USER, root);
                }
            }
        }
        BOOL cfgFound = rootFound && OpenKeyAux(NULL, hRootKey, SALAMANDER_CONFIG_REG, hCfgKey);
        if (rootFound)
            CloseKeyAux(hRootKey);

        if (rootIndex == 0 && backupFound) // We don't need the backup, we will delete it
        {
            TRACE_I("Removing unnecessary configuration backup: " << backup);
            SHDeleteKey(HKEY_CURRENT_USER, backup);
            backupFound = FALSE;
        }

        if (cfgFound) // We recognize keys with configuration based on the existence of the subkey "Configuration" (the mere existence of the key is not enough, because there may be only "AutoImportConfig" under it)
        {
            CloseKeyAux(hCfgKey);
            if (rootIndex == 0)
            {
                // It is a key to the active version of the program => we confirm loading the key and exit
                loadConfiguration = root;
                LoadSaveToRegistryMutex.Leave();
                return TRUE;
            }
            // It's one of the older keys

            // we will offer configuration for import and deletion
            dlg.ConfigurationExist[rootIndex] = TRUE;
            offerImportDlg = TRUE;
        }
        rootIndex++;
    } while (rootIndex < SALCFG_ROOTS_COUNT);

    LoadSaveToRegistryMutex.Leave();

    if (offerImportDlg)
    {
        HWND hSplash = GetSplashScreenHandle(); // if there is a splash, temporarily turn it off
        if (hSplash != NULL)
            ShowWindow(hSplash, SW_HIDE);

        int dlgRet = (int)dlg.Execute();

        if (hSplash != NULL)
        {
            ShowWindow(hSplash, SW_SHOW);
            UpdateWindow(hSplash);
        }

        if (dlgRet == IDCANCEL)
        {
            return FALSE; // user wants to escape from Salama
        }
        if (dlg.IndexOfConfigurationToLoad != -1)
            loadConfiguration = SalamanderConfigurationRoots[dlg.IndexOfConfigurationToLoad];
    }
    return TRUE;
}

// Iterates keys according to the array returned by the FindLatestConfiguration function

void CMainWindow::DeleteOldConfigurations(BOOL* deleteConfigurations, BOOL autoImportConfig,
                                          const char* autoImportConfigFromKey,
                                          BOOL doNotDeleteImportedCfg)
{
    // Is there anything to lubricate?
    BOOL dirty = FALSE;
    if (autoImportConfig)
        dirty = TRUE;
    else
    {
        int rootIndex;
        for (rootIndex = 0; rootIndex < SALCFG_ROOTS_COUNT; rootIndex++)
        {
            if (deleteConfigurations[rootIndex])
            {
                dirty = TRUE;
                break;
            }
        }
    }
    if (dirty)
    {
        // we will cut old configurations
        HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
        CWaitWindow analysing(HWindow, IDS_DELETINGCONFIGURATION, FALSE, ooStatic);
        analysing.Create();
        EnableWindow(HWindow, FALSE);
        LoadSaveToRegistryMutex.Enter();
        int rootIndex;
        for (rootIndex = 0; rootIndex < SALCFG_ROOTS_COUNT; rootIndex++)
        {
            if (deleteConfigurations[rootIndex])
            {
                HKEY hKey;
                const char* key = SalamanderConfigurationRoots[rootIndex];
                if (CreateKeyAux(NULL, HKEY_CURRENT_USER, key, hKey))
                {
                    ClearKeyAux(hKey);
                    CloseKeyAux(hKey);
                    DeleteKeyAux(HKEY_CURRENT_USER, key);
                }
            }
        }
        if (autoImportConfig) // we will clean up the old configuration (it is already saved in a new key) + in the new key we will remove the value "AutoImportConfig"
        {
            BOOL ok = FALSE;
            HKEY cfgKey;
            if (HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, SalamanderConfigurationRoots[0], 0,
                                       KEY_READ | KEY_WRITE, &cfgKey)) == ERROR_SUCCESS)
            { // In the new key, we will remove the value "AutoImportConfig"
                if (RegDeleteValue(cfgKey, SALAMANDER_AUTO_IMPORT_CONFIG) == ERROR_SUCCESS)
                    ok = TRUE;
                HANDLES(RegCloseKey(cfgKey));
            }
            if (!ok) // if that happens, it's probably not a problem, because we certainly didn't write the Salama configuration (it goes to the same key) and the whole UPGRADE will need to be done again
            {
                TRACE_E("CMainWindow::DeleteOldConfigurations(): unable to delete " << SALAMANDER_AUTO_IMPORT_CONFIG << " value from HKCU\\" << SalamanderConfigurationRoots[0]);
            }
            else // we will clean up the old configuration (it is already saved in a new key)
            {
                if (!doNotDeleteImportedCfg)
                {
                    if (HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, autoImportConfigFromKey, 0,
                                               KEY_READ | KEY_WRITE, &cfgKey)) == ERROR_SUCCESS)
                    {
                        ClearKeyAux(cfgKey);
                        HANDLES(RegCloseKey(cfgKey));
                        DeleteKeyAux(HKEY_CURRENT_USER, autoImportConfigFromKey);
                    }
                }
            }
        }
        LoadSaveToRegistryMutex.Leave();
        EnableWindow(HWindow, TRUE);
        DestroyWindow(analysing.HWindow);
        SetCursor(hOldCursor);
    }
}

//
// ****************************************************************************
// CMainWindow
//

void CMainWindow::SavePanelConfig(CFilesWindow* panel, HKEY hSalamander, const char* reg)
{
    HKEY actKey;
    if (CreateKey(hSalamander, reg, actKey))
    {
        DWORD value;
        value = panel->HeaderLineVisible;
        SetValue(actKey, PANEL_HEADER_REG, REG_DWORD, &value, sizeof(DWORD));
        SetValue(actKey, PANEL_PATH_REG, REG_SZ, panel->GetPath(), -1);
        value = panel->GetViewTemplateIndex();
        SetValue(actKey, PANEL_VIEW_REG, REG_DWORD, &value, sizeof(DWORD));
        value = panel->SortType;
        SetValue(actKey, PANEL_SORT_REG, REG_DWORD, &value, sizeof(DWORD));
        value = panel->ReverseSort;
        SetValue(actKey, PANEL_REVERSE_REG, REG_DWORD, &value, sizeof(DWORD));
        value = (panel->DirectoryLine->HWindow != NULL);
        SetValue(actKey, PANEL_DIRLINE_REG, REG_DWORD, &value, sizeof(DWORD));
        value = (panel->StatusLine->HWindow != NULL);
        SetValue(actKey, PANEL_STATUS_REG, REG_DWORD, &value, sizeof(DWORD));
        SetValue(actKey, PANEL_FILTER_ENABLE, REG_DWORD, &panel->FilterEnabled,
                 sizeof(DWORD));
        SetValue(actKey, PANEL_FILTER, REG_SZ, panel->Filter.GetMasksString(), -1);

        CloseKey(actKey);
    }
}

void CMainWindow::SaveConfig(HWND parent)
{
    CALL_STACK_MESSAGE1("CMainWindow::SaveConfig()");

    if (parent == NULL)
        parent = HWindow;

    if (SALAMANDER_ROOT_REG == NULL)
    {
        TRACE_E("SALAMANDER_ROOT_REG == NULL"); // no error needed: when UPGRADE is performed, we handle the termination of Salama without saving the configuration (if the user does not have all plugins installed and chooses Exit)
        return;
    }

    HCURSOR hOldCursor = NULL;
    if (GlobalSaveWaitWindow == NULL)
        hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    CWaitWindow analysing(parent, IDS_SAVINGCONFIGURATION, FALSE, ooStatic, TRUE);
    int savingProgress = 0;
    HWND oldPluginMsgBoxParent = PluginMsgBoxParent;
    if (GlobalSaveWaitWindow == NULL)
    {
        //TRACE_I("analysing.SetProgressPos() savingProgress="<<savingProgress);
        analysing.SetProgressMax(7 /* NEED TO SYNCHRONIZE with CMainWindow::WindowProc::WM_USER_CLOSE_MAINWND !!!*/); // Wait a minute, let them enjoy the view at 100%
        analysing.Create();
        EnableWindow(parent, FALSE);

        // SaveConfiguration plug-in will also be called -> necessary setting of the parent for their message boxes
        PluginMsgBoxParent = analysing.HWindow;
    }

    LoadSaveToRegistryMutex.Enter();

    HKEY salamander;
    if (CreateKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
    {
        HKEY actKey;

        BOOL cfgIsOK = TRUE;
        BOOL deleteSALAMANDER_SAVE_IN_PROGRESS = !IsSetSALAMANDER_SAVE_IN_PROGRESS;
        if (deleteSALAMANDER_SAVE_IN_PROGRESS)
        {
            DWORD saveInProgress = 1;
            if (GetValueAux(NULL, salamander, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD)))
            {                    // GetValueAux because I don't want a message about Load Configuration
                cfgIsOK = FALSE; // It is about a damaged configuration, it is not fixed by saving (it is not saved completely)
                TRACE_E("CMainWindow::SaveConfig(): unable to save configuration, configuration key in registry is corrupted");
            }
            else
            {
                saveInProgress = 1;
                SetValue(salamander, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD));
                IsSetSALAMANDER_SAVE_IN_PROGRESS = TRUE;
            }
        }

        if (cfgIsOK)
        {
            //--- version
            if (CreateKey(salamander, SALAMANDER_VERSION_REG, actKey))
            {
                DWORD newConfigVersion = THIS_CONFIG_VERSION;
                SetValue(actKey, SALAMANDER_VERSIONREG_REG, REG_DWORD,
                         &newConfigVersion, sizeof(DWORD));
                CloseKey(actKey);
            }

            //--- window

            if (CreateKey(salamander, SALAMANDER_WINDOW_REG, actKey))
            {
                WINDOWPLACEMENT place;
                place.length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement(HWindow, &place);
                SetValue(actKey, WINDOW_LEFT_REG, REG_DWORD,
                         &(place.rcNormalPosition.left), sizeof(DWORD));
                SetValue(actKey, WINDOW_RIGHT_REG, REG_DWORD,
                         &(place.rcNormalPosition.right), sizeof(DWORD));
                SetValue(actKey, WINDOW_TOP_REG, REG_DWORD,
                         &(place.rcNormalPosition.top), sizeof(DWORD));
                SetValue(actKey, WINDOW_BOTTOM_REG, REG_DWORD,
                         &(place.rcNormalPosition.bottom), sizeof(DWORD));
                SetValue(actKey, WINDOW_SHOW_REG, REG_DWORD,
                         &(place.showCmd), sizeof(DWORD));
                char buf[20];
                sprintf(buf, "%.1lf", SplitPosition * 100);
                SetValue(actKey, WINDOW_SPLIT_REG, REG_SZ, buf, -1);
                sprintf(buf, "%.1lf", BeforeZoomSplitPosition * 100);
                SetValue(actKey, WINDOW_BEFOREZOOMSPLIT_REG, REG_SZ, buf, -1);

                CloseKey(actKey);
            }

            if (Configuration.FindDialogWindowPlacement.length != 0)
            {
                if (CreateKey(salamander, FINDDIALOG_WINDOW_REG, actKey))
                {
                    SetValue(actKey, WINDOW_LEFT_REG, REG_DWORD,
                             &(Configuration.FindDialogWindowPlacement.rcNormalPosition.left), sizeof(DWORD));
                    SetValue(actKey, WINDOW_RIGHT_REG, REG_DWORD,
                             &(Configuration.FindDialogWindowPlacement.rcNormalPosition.right), sizeof(DWORD));
                    SetValue(actKey, WINDOW_TOP_REG, REG_DWORD,
                             &(Configuration.FindDialogWindowPlacement.rcNormalPosition.top), sizeof(DWORD));
                    SetValue(actKey, WINDOW_BOTTOM_REG, REG_DWORD,
                             &(Configuration.FindDialogWindowPlacement.rcNormalPosition.bottom), sizeof(DWORD));
                    SetValue(actKey, WINDOW_SHOW_REG, REG_DWORD,
                             &(Configuration.FindDialogWindowPlacement.showCmd), sizeof(DWORD));

                    SetValue(actKey, FINDDIALOG_NAMEWIDTH_REG, REG_DWORD,
                             &(Configuration.FindColNameWidth), sizeof(DWORD));
                    CloseKey(actKey);
                }
            }

            //--- left and right panel

            SavePanelConfig(LeftPanel, salamander, SALAMANDER_LEFTP_REG);
            SavePanelConfig(RightPanel, salamander, SALAMANDER_RIGHTP_REG);

            //--- default directories

            if (CreateKey(salamander, SALAMANDER_DEFDIRS_REG, actKey))
            {
                char name[2];
                name[1] = 0;
                char d;
                for (d = 'A'; d <= 'Z'; d++)
                {
                    name[0] = d;
                    char* path = DefaultDir[d - 'A'];
                    if (path[1] == ':' && path[2] == '\\' && path[3] != 0) // It is not "C:\"
                        SetValue(actKey, name, REG_SZ, path, -1);
                    else
                        DeleteValue(actKey, name);
                }
                CloseKey(actKey);
            }

            //--- password manager

            if (CreateKey(salamander, SALAMANDER_PWDMNGR_REG, actKey))
            {
                PasswordManager.Save(actKey);
                CloseKey(actKey);
            }

            //--- hot paths

            if (CreateKey(salamander, SALAMANDER_HOTPATHS_REG, actKey))
            {
                HotPaths.Save(actKey);
                CloseKey(actKey);
            }

            //--- view templates

            if (CreateKey(salamander, SALAMANDER_VIEWTEMPLATES_REG, actKey))
            {
                ViewTemplates.Save(actKey);
                CloseKey(actKey);
            }

            //--- Plugins
            HKEY configKey;
            HKEY orderKey;
            if (CreateKey(salamander, SALAMANDER_PLUGINS, actKey) &&
                CreateKey(salamander, SALAMANDER_PLUGINSCONFIG, configKey) &&
                CreateKey(salamander, SALAMANDER_PLUGINSORDER, orderKey))
            {
                Plugins.Save(parent, actKey, configKey, orderKey);
                CloseKey(orderKey);
                CloseKey(actKey);
                CloseKey(configKey);
            }

            if (GlobalSaveWaitWindow == NULL)
                analysing.SetProgressPos(++savingProgress); // 1
            else
                GlobalSaveWaitWindow->SetProgressPos(++GlobalSaveWaitWindowProgress); // 1
            //TRACE_I("analysing.SetProgressPos() savingProgress="<<savingProgress);

            //--- Packers & Unpackers
            if (CreateKey(salamander, SALAMANDER_PACKANDUNPACK, actKey))
            {
                SetValue(actKey, SALAMANDER_SIMPLEICONSINARCHIVES, REG_DWORD,
                         &(Configuration.UseSimpleIconsInArchives), sizeof(DWORD));

                //--- Custom Packers
                HKEY actSubKey;
                if (CreateKey(actKey, SALAMANDER_CUSTOMPACKERS, actSubKey))
                {
                    ClearKey(actSubKey);
                    HKEY itemKey;
                    char buf[30];
                    int i;
                    for (i = 0; i < PackerConfig.GetPackersCount(); i++)
                    {
                        itoa(i + 1, buf, 10);
                        if (CreateKey(actSubKey, buf, itemKey))
                        {
                            PackerConfig.Save(i, itemKey);
                            CloseKey(itemKey);
                        }
                        else
                            break;
                    }
                    SetValue(actSubKey, SALAMANDER_ANOTHERPANEL, REG_DWORD,
                             &(Configuration.UseAnotherPanelForPack), sizeof(DWORD));
                    int pp = PackerConfig.GetPreferedPacker();
                    SetValue(actSubKey, SALAMANDER_PREFFERED, REG_DWORD, &pp, sizeof(DWORD));
                    CloseKey(actSubKey);
                }

                if (GlobalSaveWaitWindow == NULL)
                    analysing.SetProgressPos(++savingProgress); // 2
                else
                    GlobalSaveWaitWindow->SetProgressPos(++GlobalSaveWaitWindowProgress); // 2
                //TRACE_I("analysing.SetProgressPos() savingProgress="<<savingProgress);

                //--- Custom Unpackers
                if (CreateKey(actKey, SALAMANDER_CUSTOMUNPACKERS, actSubKey))
                {
                    ClearKey(actSubKey);
                    HKEY itemKey;
                    char buf[30];
                    int i;
                    for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
                    {
                        itoa(i + 1, buf, 10);
                        if (CreateKey(actSubKey, buf, itemKey))
                        {
                            UnpackerConfig.Save(i, itemKey);
                            CloseKey(itemKey);
                        }
                        else
                            break;
                    }
                    SetValue(actSubKey, SALAMANDER_ANOTHERPANEL, REG_DWORD,
                             &(Configuration.UseAnotherPanelForUnpack), sizeof(DWORD));
                    SetValue(actSubKey, SALAMANDER_NAMEBYARCHIVE, REG_DWORD,
                             &(Configuration.UseSubdirNameByArchiveForUnpack), sizeof(DWORD));
                    int pp = UnpackerConfig.GetPreferedUnpacker();
                    SetValue(actSubKey, SALAMANDER_PREFFERED, REG_DWORD, &pp, sizeof(DWORD));
                    CloseKey(actSubKey);
                }

                if (GlobalSaveWaitWindow == NULL)
                    analysing.SetProgressPos(++savingProgress); // 3
                else
                    GlobalSaveWaitWindow->SetProgressPos(++GlobalSaveWaitWindowProgress); // 3
                //TRACE_I("analysing.SetProgressPos() savingProgress="<<savingProgress);

                //--- Predefined Packers
                if (CreateKey(actKey, SALAMANDER_PREDPACKERS, actSubKey))
                {
                    ClearKey(actSubKey);
                    HKEY itemKey;
                    char buf[30];
                    int i;
                    for (i = 0; i < ArchiverConfig.GetArchiversCount(); i++)
                    {
                        itoa(i + 1, buf, 10);
                        if (CreateKey(actSubKey, buf, itemKey))
                        {
                            ArchiverConfig.Save(i, itemKey);
                            CloseKey(itemKey);
                        }
                        else
                            break;
                    }
                    CloseKey(actSubKey);
                }

                //--- Archive Association
                if (CreateKey(actKey, SALAMANDER_ARCHIVEASSOC, actSubKey))
                {
                    ClearKey(actSubKey);
                    HKEY itemKey;
                    char buf[30];
                    int i;
                    for (i = 0; i < PackerFormatConfig.GetFormatsCount(); i++)
                    {
                        itoa(i + 1, buf, 10);
                        if (CreateKey(actSubKey, buf, itemKey))
                        {
                            PackerFormatConfig.Save(i, itemKey);
                            CloseKey(itemKey);
                        }
                        else
                            break;
                    }
                    CloseKey(actSubKey);
                }

                CloseKey(actKey);
            }

            //--- configuration

            if (CreateKey(salamander, SALAMANDER_CONFIG_REG, actKey))
            {
                //--- top rebar begin
                SetValue(actKey, CONFIG_MENUINDEX_REG, REG_DWORD,
                         &Configuration.MenuIndex, sizeof(DWORD));
                SetValue(actKey, CONFIG_MENUBREAK_REG, REG_DWORD,
                         &Configuration.MenuBreak, sizeof(DWORD));
                SetValue(actKey, CONFIG_MENUWIDTH_REG, REG_DWORD,
                         &Configuration.MenuWidth, sizeof(DWORD));
                SetValue(actKey, CONFIG_TOOLBARINDEX_REG, REG_DWORD,
                         &Configuration.TopToolbarIndex, sizeof(DWORD));
                SetValue(actKey, CONFIG_TOOLBARBREAK_REG, REG_DWORD,
                         &Configuration.TopToolbarBreak, sizeof(DWORD));
                SetValue(actKey, CONFIG_TOOLBARWIDTH_REG, REG_DWORD,
                         &Configuration.TopToolbarWidth, sizeof(DWORD));
                SetValue(actKey, CONFIG_PLUGINSBARINDEX_REG, REG_DWORD,
                         &Configuration.PluginsBarIndex, sizeof(DWORD));
                SetValue(actKey, CONFIG_PLUGINSBARBREAK_REG, REG_DWORD,
                         &Configuration.PluginsBarBreak, sizeof(DWORD));
                SetValue(actKey, CONFIG_PLUGINSBARWIDTH_REG, REG_DWORD,
                         &Configuration.PluginsBarWidth, sizeof(DWORD));
                SetValue(actKey, CONFIG_USERMENUINDEX_REG, REG_DWORD,
                         &Configuration.UserMenuToolbarIndex, sizeof(DWORD));
                SetValue(actKey, CONFIG_USERMENUBREAK_REG, REG_DWORD,
                         &Configuration.UserMenuToolbarBreak, sizeof(DWORD));
                SetValue(actKey, CONFIG_USERMENUWIDTH_REG, REG_DWORD,
                         &Configuration.UserMenuToolbarWidth, sizeof(DWORD));
                SetValue(actKey, CONFIG_USERMENULABELS_REG, REG_DWORD,
                         &Configuration.UserMenuToolbarLabels, sizeof(DWORD));
                SetValue(actKey, CONFIG_HOTPATHSINDEX_REG, REG_DWORD,
                         &Configuration.HotPathsBarIndex, sizeof(DWORD));
                SetValue(actKey, CONFIG_HOTPATHSBREAK_REG, REG_DWORD,
                         &Configuration.HotPathsBarBreak, sizeof(DWORD));
                SetValue(actKey, CONFIG_HOTPATHSWIDTH_REG, REG_DWORD,
                         &Configuration.HotPathsBarWidth, sizeof(DWORD));
                SetValue(actKey, CONFIG_DRIVEBARINDEX_REG, REG_DWORD,
                         &Configuration.DriveBarIndex, sizeof(DWORD));
                SetValue(actKey, CONFIG_DRIVEBARBREAK_REG, REG_DWORD,
                         &Configuration.DriveBarBreak, sizeof(DWORD));
                SetValue(actKey, CONFIG_DRIVEBARWIDTH_REG, REG_DWORD,
                         &Configuration.DriveBarWidth, sizeof(DWORD));
                SetValue(actKey, CONFIG_GRIPSVISIBLE_REG, REG_DWORD,
                         &Configuration.GripsVisible, sizeof(DWORD));

                //--- top rebar end
                SetValue(actKey, CONFIG_FILENAMEFORMAT_REG, REG_DWORD,
                         &Configuration.FileNameFormat, sizeof(DWORD));
                SetValue(actKey, CONFIG_SIZEFORMAT_REG, REG_DWORD,
                         &Configuration.SizeFormat, sizeof(DWORD));
                SetValue(actKey, CONFIG_SELECTION_REG, REG_DWORD,
                         &Configuration.IncludeDirs, sizeof(DWORD));
                SetValue(actKey, CONFIG_COPYFINDTEXT_REG, REG_DWORD,
                         &Configuration.CopyFindText, sizeof(DWORD));
                SetValue(actKey, CONFIG_CLEARREADONLY_REG, REG_DWORD,
                         &Configuration.ClearReadOnly, sizeof(DWORD));
                SetValue(actKey, CONFIG_PRIMARYCONTEXTMENU_REG, REG_DWORD,
                         &Configuration.PrimaryContextMenu, sizeof(DWORD));
                SetValue(actKey, CONFIG_NOTHIDDENSYSTEM_REG, REG_DWORD,
                         &Configuration.NotHiddenSystemFiles, sizeof(DWORD));
                SetValue(actKey, CONFIG_RECYCLEBIN_REG, REG_DWORD,
                         &Configuration.UseRecycleBin, sizeof(DWORD));
                SetValue(actKey, CONFIG_RECYCLEMASKS_REG, REG_SZ,
                         Configuration.RecycleMasks.GetMasksString(), -1);
                SetValue(actKey, CONFIG_SAVEONEXIT_REG, REG_DWORD,
                         &Configuration.AutoSave, sizeof(DWORD));
                SetValue(actKey, CONFIG_SHOWGREPERRORS_REG, REG_DWORD,
                         &Configuration.ShowGrepErrors, sizeof(DWORD));
                SetValue(actKey, CONFIG_FINDFULLROW_REG, REG_DWORD,
                         &Configuration.FindFullRowSelect, sizeof(DWORD));
                SetValue(actKey, CONFIG_MINBEEPWHENDONE_REG, REG_DWORD,
                         &Configuration.MinBeepWhenDone, sizeof(DWORD));
                SetValue(actKey, CONFIG_CLOSESHELL_REG, REG_DWORD,
                         &Configuration.CloseShell, sizeof(DWORD));
                DWORD rightPanelFocused = (GetActivePanel() == RightPanel);
                SetValue(actKey, CONFIG_RIGHT_FOCUS_REG, REG_DWORD,
                         &rightPanelFocused, sizeof(DWORD));
                SetValue(actKey, CONFIG_ALWAYSONTOP_REG, REG_DWORD,
                         &Configuration.AlwaysOnTop, sizeof(DWORD));
                //      SetValue(actKey, CONFIG_FASTDIRMOVE_REG, REG_DWORD,
                //               &Configuration.FastDirectoryMove, sizeof(DWORD));
                SetValue(actKey, CONFIG_SORTUSESLOCALE_REG, REG_DWORD,
                         &Configuration.SortUsesLocale, sizeof(DWORD));
                SetValue(actKey, CONFIG_SORTDETECTNUMBERS_REG, REG_DWORD,
                         &Configuration.SortDetectNumbers, sizeof(DWORD));
                SetValue(actKey, CONFIG_SORTNEWERONTOP_REG, REG_DWORD,
                         &Configuration.SortNewerOnTop, sizeof(DWORD));
                SetValue(actKey, CONFIG_SORTDIRSBYNAME_REG, REG_DWORD,
                         &Configuration.SortDirsByName, sizeof(DWORD));
                SetValue(actKey, CONFIG_SORTDIRSBYEXT_REG, REG_DWORD,
                         &Configuration.SortDirsByExt, sizeof(DWORD));
                SetValue(actKey, CONFIG_SAVEHISTORY_REG, REG_DWORD,
                         &Configuration.SaveHistory, sizeof(DWORD));
                SetValue(actKey, CONFIG_SAVEWORKDIRS_REG, REG_DWORD,
                         &Configuration.SaveWorkDirs, sizeof(DWORD));
                SetValue(actKey, CONFIG_ENABLECMDLINEHISTORY_REG, REG_DWORD,
                         &Configuration.EnableCmdLineHistory, sizeof(DWORD));
                SetValue(actKey, CONFIG_SAVECMDLINEHISTORY_REG, REG_DWORD,
                         &Configuration.SaveCmdLineHistory, sizeof(DWORD));
                //      SetValue(actKey, CONFIG_LANTASTICCHECK_REG, REG_DWORD,
                //               &Configuration.LantasticCheck, sizeof(DWORD));
                SetValue(actKey, CONFIG_ONLYONEINSTANCE_REG, REG_DWORD,
                         &Configuration.OnlyOneInstance, sizeof(DWORD));
                SetValue(actKey, CONFIG_STATUSAREA_REG, REG_DWORD,
                         &Configuration.StatusArea, sizeof(DWORD));
                SetValue(actKey, CONFIG_FULLROWSELECT_REG, REG_DWORD,
                         &Configuration.FullRowSelect, sizeof(DWORD));
                SetValue(actKey, CONFIG_FULLROWHIGHLIGHT_REG, REG_DWORD,
                         &Configuration.FullRowHighlight, sizeof(DWORD));
                SetValue(actKey, CONFIG_USEICONTINCTURE_REG, REG_DWORD,
                         &Configuration.UseIconTincture, sizeof(DWORD));
                SetValue(actKey, CONFIG_SHOWPANELCAPTION_REG, REG_DWORD,
                         &Configuration.ShowPanelCaption, sizeof(DWORD));
                SetValue(actKey, CONFIG_SHOWPANELZOOM_REG, REG_DWORD,
                         &Configuration.ShowPanelZoom, sizeof(DWORD));
                SetValue(actKey, CONFIG_SINGLECLICK_REG, REG_DWORD,
                         &Configuration.SingleClick, sizeof(DWORD));
                //      SetValue(actKey, CONFIG_SHOWTIPOFTHEDAY_REG, REG_DWORD,
                //               &Configuration.ShowTipOfTheDay, sizeof(DWORD));
                //      SetValue(actKey, CONFIG_LASTTIPOFTHEDAY_REG, REG_DWORD,
                //               &Configuration.LastTipOfTheDay, sizeof(DWORD));
                SetValue(actKey, CONFIG_INFOLINECONTENT_REG, REG_SZ,
                         Configuration.InfoLineContent, -1);
                SetValue(actKey, CONFIG_IFPATHISINACCESSIBLEGOTOISMYDOCS_REG, REG_DWORD,
                         &Configuration.IfPathIsInaccessibleGoToIsMyDocs, sizeof(DWORD));
                SetValue(actKey, CONFIG_IFPATHISINACCESSIBLEGOTO_REG, REG_SZ,
                         Configuration.IfPathIsInaccessibleGoTo, -1);
                SetValue(actKey, CONFIG_HOTPATH_AUTOCONFIG, REG_DWORD,
                         &Configuration.HotPathAutoConfig, sizeof(DWORD));
                SetValue(actKey, CONFIG_LASTUSEDSPEEDLIM_REG, REG_DWORD,
                         &Configuration.LastUsedSpeedLimit, sizeof(DWORD));
                SetValue(actKey, CONFIG_QUICKSEARCHENTER_REG, REG_DWORD,
                         &Configuration.QuickSearchEnterAlt, sizeof(DWORD));
                SetValue(actKey, CONFIG_CHD_SHOWMYDOC, REG_DWORD,
                         &Configuration.ChangeDriveShowMyDoc, sizeof(DWORD));
                SetValue(actKey, CONFIG_CHD_SHOWCLOUDSTOR, REG_DWORD,
                         &Configuration.ChangeDriveCloudStorage, sizeof(DWORD));
                SetValue(actKey, CONFIG_CHD_SHOWANOTHER, REG_DWORD,
                         &Configuration.ChangeDriveShowAnother, sizeof(DWORD));
                SetValue(actKey, CONFIG_CHD_SHOWNET, REG_DWORD,
                         &Configuration.ChangeDriveShowNet, sizeof(DWORD));
                SetValue(actKey, CONFIG_SEARCHFILECONTENT, REG_DWORD,
                         &Configuration.SearchFileContent, sizeof(DWORD));
                SetValue(actKey, CONFIG_LASTPLUGINVER, REG_DWORD,
                         &Configuration.LastPluginVer, sizeof(DWORD));
                SetValue(actKey, CONFIG_LASTPLUGINVER_OP, REG_DWORD,
                         &Configuration.LastPluginVerOP, sizeof(DWORD));
                SetValue(actKey, CONFIG_USESALOPEN_REG, REG_DWORD,
                         &Configuration.UseSalOpen, sizeof(DWORD));
                SetValue(actKey, CONFIG_NETWAREFASTDIRMOVE_REG, REG_DWORD,
                         &Configuration.NetwareFastDirMove, sizeof(DWORD));
                if (Windows7AndLater)
                    SetValue(actKey, CONFIG_ASYNCCOPYALG_REG, REG_DWORD,
                             &Configuration.UseAsyncCopyAlg, sizeof(DWORD));
                SetValue(actKey, CONFIG_RELOAD_ENV_VARS_REG, REG_DWORD,
                         &Configuration.ReloadEnvVariables, sizeof(DWORD));
                SetValue(actKey, CONFIG_QUICKRENAME_SELALL_REG, REG_DWORD,
                         &Configuration.QuickRenameSelectAll, sizeof(DWORD));
                SetValue(actKey, CONFIG_EDITNEW_SELALL_REG, REG_DWORD,
                         &Configuration.EditNewSelectAll, sizeof(DWORD));
                SetValue(actKey, CONFIG_SHIFTFORHOTPATHS_REG, REG_DWORD,
                         &Configuration.ShiftForHotPaths, sizeof(DWORD));
                SetValue(actKey, CONFIG_LANGUAGE_REG, REG_SZ,
                         Configuration.SLGName, -1);
                SetValue(actKey, CONFIG_USEALTLANGFORPLUGINS_REG, REG_DWORD,
                         &Configuration.UseAsAltSLGInOtherPlugins, sizeof(DWORD));
                SetValue(actKey, CONFIG_ALTLANGFORPLUGINS_REG, REG_SZ,
                         Configuration.AltPluginSLGName, -1);
                DWORD langChanged = (StrICmp(Configuration.SLGName, Configuration.LoadedSLGName) != 0); // TRUE if the user changed the language to Salama
                SetValue(actKey, CONFIG_LANGUAGECHANGED_REG, REG_DWORD, &langChanged, sizeof(DWORD));
                SetValue(actKey, CONFIG_SHOWSPLASHSCREEN_REG, REG_DWORD,
                         &Configuration.ShowSplashScreen, sizeof(DWORD));
                SetValue(actKey, CONFIG_CONVERSIONTABLE_REG, REG_SZ,
                         &Configuration.ConversionTable, -1);
                SetValue(actKey, CONFIG_SKILLLEVEL_REG, REG_DWORD,
                         &Configuration.SkillLevel, sizeof(DWORD));
                SetValue(actKey, CONFIG_TITLEBARSHOWPATH_REG, REG_DWORD,
                         &Configuration.TitleBarShowPath, sizeof(DWORD));
                SetValue(actKey, CONFIG_TITLEBARMODE_REG, REG_DWORD,
                         &Configuration.TitleBarMode, sizeof(DWORD));
                SetValue(actKey, CONFIG_TITLEBARPREFIX_REG, REG_DWORD,
                         &Configuration.UseTitleBarPrefix, sizeof(DWORD));
                SetValue(actKey, CONFIG_TITLEBARPREFIXTEXT_REG, REG_SZ,
                         &Configuration.TitleBarPrefix, -1);
                SetValue(actKey, CONFIG_MAINWINDOWICONINDEX_REG, REG_DWORD,
                         &Configuration.MainWindowIconIndex, sizeof(DWORD));
                SetValue(actKey, CONFIG_CLICKQUICKRENAME_REG, REG_DWORD,
                         &Configuration.ClickQuickRename, sizeof(DWORD));
                SetValue(actKey, CONFIG_VISIBLEDRIVES_REG, REG_DWORD,
                         &Configuration.VisibleDrives, sizeof(DWORD));
                SetValue(actKey, CONFIG_SEPARATEDDRIVES_REG, REG_DWORD,
                         &Configuration.SeparatedDrives, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREBYTIME_REG, REG_DWORD,
                         &Configuration.CompareByTime, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREBYSIZE_REG, REG_DWORD,
                         &Configuration.CompareBySize, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREBYCONTENT_REG, REG_DWORD,
                         &Configuration.CompareByContent, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREBYATTR_REG, REG_DWORD,
                         &Configuration.CompareByAttr, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREBYSUBDIRS_REG, REG_DWORD,
                         &Configuration.CompareSubdirs, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREBYSUBDIRSATTR_REG, REG_DWORD,
                         &Configuration.CompareSubdirsAttr, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREONEPANELDIRS_REG, REG_DWORD,
                         &Configuration.CompareOnePanelDirs, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREMOREOPTIONS_REG, REG_DWORD,
                         &Configuration.CompareMoreOptions, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREIGNOREFILES_REG, REG_DWORD,
                         &Configuration.CompareIgnoreFiles, sizeof(DWORD));
                SetValue(actKey, CONFIG_COMPAREIGNOREDIRS_REG, REG_DWORD,
                         &Configuration.CompareIgnoreDirs, sizeof(DWORD));
                SetValue(actKey, CONFIG_CONFIGTIGNOREFILESMASKS_REG, REG_SZ,
                         Configuration.CompareIgnoreFilesMasks.GetMasksString(), -1);
                SetValue(actKey, CONFIG_CONFIGTIGNOREDIRSMASKS_REG, REG_SZ,
                         Configuration.CompareIgnoreDirsMasks.GetMasksString(), -1);

                SetValue(actKey, CONFIG_THUMBNAILSIZE_REG, REG_DWORD,
                         &Configuration.ThumbnailSize, sizeof(DWORD));
                SetValue(actKey, CONFIG_KEEPPLUGINSSORTED_REG, REG_DWORD,
                         &Configuration.KeepPluginsSorted, sizeof(DWORD));
                SetValue(actKey, CONFIG_SHOWSLGINCOMPLETE_REG, REG_DWORD,
                         &Configuration.ShowSLGIncomplete, sizeof(DWORD));

                // WARNING: in case of a crash in the icon overlay handler, these values are written directly to the registry
                //        (prevention of "non-executability" of Salam), see InformAboutIconOvrlsHanCrash()
                SetValue(actKey, CONFIG_ENABLECUSTICOVRLS_REG, REG_DWORD,
                         &Configuration.EnableCustomIconOverlays, sizeof(DWORD));
                SetValue(actKey, CONFIG_DISABLEDCUSTICOVRLS_REG, REG_SZ,
                         Configuration.DisabledCustomIconOverlays != NULL ? Configuration.DisabledCustomIconOverlays : "", -1);

                SetValue(actKey, CONFIG_EDITNEWFILE_USEDEFAULT_REG, REG_DWORD,
                         &Configuration.UseEditNewFileDefault, sizeof(DWORD));
                SetValue(actKey, CONFIG_EDITNEWFILE_DEFAULT_REG, REG_SZ,
                         Configuration.EditNewFileDefault, -1);

#ifndef _WIN64 // FIXME_X64_WINSCP
                SetValue(actKey, "Add x86-Only Plugins", REG_DWORD,
                         &Configuration.AddX86OnlyPlugins, sizeof(DWORD));
#endif // _WIN64

                HKEY actSubKey;
                if (CreateKey(actKey, SALAMANDER_CONFIRMATION_REG, actSubKey))
                {
                    SetValue(actSubKey, CONFIG_CNFRM_FILEDIRDEL, REG_DWORD,
                             &Configuration.CnfrmFileDirDel, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_NEDIRDEL, REG_DWORD,
                             &Configuration.CnfrmNEDirDel, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_FILEOVER, REG_DWORD,
                             &Configuration.CnfrmFileOver, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_DIROVER, REG_DWORD,
                             &Configuration.CnfrmDirOver, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_SHFILEDEL, REG_DWORD,
                             &Configuration.CnfrmSHFileDel, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_SHDIRDEL, REG_DWORD,
                             &Configuration.CnfrmSHDirDel, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_SHFILEOVER, REG_DWORD,
                             &Configuration.CnfrmSHFileOver, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_NTFSPRESS, REG_DWORD,
                             &Configuration.CnfrmNTFSPress, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_NTFSCRYPT, REG_DWORD,
                             &Configuration.CnfrmNTFSCrypt, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_DAD, REG_DWORD,
                             &Configuration.CnfrmDragDrop, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_CLOSEARCHIVE, REG_DWORD,
                             &Configuration.CnfrmCloseArchive, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_CLOSEFIND, REG_DWORD,
                             &Configuration.CnfrmCloseFind, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_STOPFIND, REG_DWORD,
                             &Configuration.CnfrmStopFind, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_CREATETARGETPATH, REG_DWORD,
                             &Configuration.CnfrmCreatePath, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_ALWAYSONTOP, REG_DWORD,
                             &Configuration.CnfrmAlwaysOnTop, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_ONSALCLOSE, REG_DWORD,
                             &Configuration.CnfrmOnSalClose, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_SENDEMAIL, REG_DWORD,
                             &Configuration.CnfrmSendEmail, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_ADDTOARCHIVE, REG_DWORD,
                             &Configuration.CnfrmAddToArchive, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_CREATEDIR, REG_DWORD,
                             &Configuration.CnfrmCreateDir, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_CHANGEDIRTC, REG_DWORD,
                             &Configuration.CnfrmChangeDirTC, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_SHOWNAMETOCOMP, REG_DWORD,
                             &Configuration.CnfrmShowNamesToCompare, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_DSTSHIFTSIGNORED, REG_DWORD,
                             &Configuration.CnfrmDSTShiftsIgnored, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_DSTSHIFTSOCCURED, REG_DWORD,
                             &Configuration.CnfrmDSTShiftsOccured, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_CNFRM_COPYMOVEOPTIONSNS, REG_DWORD,
                             &Configuration.CnfrmCopyMoveOptionsNS, sizeof(DWORD));

                    CloseKey(actSubKey);
                }

                if (CreateKey(actKey, SALAMANDER_DRVSPEC_REG, actSubKey))
                {
                    SetValue(actSubKey, CONFIG_DRVSPEC_FLOPPY_MON, REG_DWORD,
                             &Configuration.DrvSpecFloppyMon, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_FLOPPY_SIMPLE, REG_DWORD,
                             &Configuration.DrvSpecFloppySimple, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_REMOVABLE_MON, REG_DWORD,
                             &Configuration.DrvSpecRemovableMon, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_REMOVABLE_SIMPLE, REG_DWORD,
                             &Configuration.DrvSpecRemovableSimple, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_FIXED_MON, REG_DWORD,
                             &Configuration.DrvSpecFixedMon, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_FIXED_SIMPLE, REG_DWORD,
                             &Configuration.DrvSpecFixedSimple, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_REMOTE_MON, REG_DWORD,
                             &Configuration.DrvSpecRemoteMon, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_REMOTE_SIMPLE, REG_DWORD,
                             &Configuration.DrvSpecRemoteSimple, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_REMOTE_ACT, REG_DWORD,
                             &Configuration.DrvSpecRemoteDoNotRefreshOnAct, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_CDROM_MON, REG_DWORD,
                             &Configuration.DrvSpecCDROMMon, sizeof(DWORD));
                    SetValue(actSubKey, CONFIG_DRVSPEC_CDROM_SIMPLE, REG_DWORD,
                             &Configuration.DrvSpecCDROMSimple, sizeof(DWORD));
                    CloseKey(actSubKey);
                }

                SetValue(actKey, CONFIG_TOPTOOLBAR_REG, REG_SZ, Configuration.TopToolBar, -1);
                SetValue(actKey, CONFIG_MIDDLETOOLBAR_REG, REG_SZ, Configuration.MiddleToolBar, -1);

                SetValue(actKey, CONFIG_LEFTTOOLBAR_REG, REG_SZ, Configuration.LeftToolBar, -1);
                SetValue(actKey, CONFIG_RIGHTTOOLBAR_REG, REG_SZ, Configuration.RightToolBar, -1);

                SetValue(actKey, CONFIG_TOPTOOLBARVISIBLE_REG, REG_DWORD,
                         &Configuration.TopToolBarVisible, sizeof(DWORD));
                SetValue(actKey, CONFIG_PLGTOOLBARVISIBLE_REG, REG_DWORD,
                         &Configuration.PluginsBarVisible, sizeof(DWORD));
                SetValue(actKey, CONFIG_MIDDLETOOLBARVISIBLE_REG, REG_DWORD,
                         &Configuration.MiddleToolBarVisible, sizeof(DWORD));

                SetValue(actKey, CONFIG_USERMENUTOOLBARVISIBLE_REG, REG_DWORD,
                         &Configuration.UserMenuToolBarVisible, sizeof(DWORD));
                SetValue(actKey, CONFIG_HOTPATHSBARVISIBLE_REG, REG_DWORD,
                         &Configuration.HotPathsBarVisible, sizeof(DWORD));

                SetValue(actKey, CONFIG_DRIVEBARVISIBLE_REG, REG_DWORD,
                         &Configuration.DriveBarVisible, sizeof(DWORD));
                SetValue(actKey, CONFIG_DRIVEBAR2VISIBLE_REG, REG_DWORD,
                         &Configuration.DriveBar2Visible, sizeof(DWORD));

                SetValue(actKey, CONFIG_BOTTOMTOOLBARVISIBLE_REG, REG_DWORD,
                         &Configuration.BottomToolBarVisible, sizeof(DWORD));

                //      SetValue(actKey, CONFIG_SPACESELCALCSPACE, REG_DWORD,
                //               &Configuration.SpaceSelCalcSpace, sizeof(DWORD));
                SetValue(actKey, CONFIG_USETIMERESOLUTION, REG_DWORD,
                         &Configuration.UseTimeResolution, sizeof(DWORD));
                SetValue(actKey, CONFIG_TIMERESOLUTION, REG_DWORD,
                         &Configuration.TimeResolution, sizeof(DWORD));
                SetValue(actKey, CONFIG_IGNOREDSTSHIFTS, REG_DWORD,
                         &Configuration.IgnoreDSTShifts, sizeof(DWORD));
                SetValue(actKey, CONFIG_USEDRAGDROPMINTIME, REG_DWORD,
                         &Configuration.UseDragDropMinTime, sizeof(DWORD));
                SetValue(actKey, CONFIG_DRAGDROPMINTIME, REG_DWORD,
                         &Configuration.DragDropMinTime, sizeof(DWORD));

                SetValue(actKey, CONFIG_LASTFOCUSEDPAGE, REG_DWORD,
                         &Configuration.LastFocusedPage, sizeof(DWORD));
                SetValue(actKey, CONFIG_CONFIGURATION_HEIGHT, REG_DWORD,
                         &Configuration.ConfigurationHeight, sizeof(DWORD));
                SetValue(actKey, CONFIG_VIEWANDEDITEXPAND, REG_DWORD,
                         &Configuration.ViewersAndEditorsExpanded, sizeof(DWORD));
                SetValue(actKey, CONFIG_PACKEPAND, REG_DWORD,
                         &Configuration.PackersAndUnpackersExpanded, sizeof(DWORD));

                SetValue(actKey, CONFIG_CMDLINE_REG, REG_DWORD, &EditPermanentVisible, sizeof(DWORD));
                SetValue(actKey, CONFIG_CMDLFOCUS_REG, REG_DWORD, &EditMode, sizeof(DWORD));

                SetValue(actKey, CONFIG_USECUSTOMPANELFONT_REG, REG_DWORD, &UseCustomPanelFont, sizeof(DWORD));
                SaveLogFont(actKey, CONFIG_PANELFONT_REG, &LogFont);

                if (GlobalSaveWaitWindow == NULL)
                    analysing.SetProgressPos(++savingProgress); // 4
                else
                    GlobalSaveWaitWindow->SetProgressPos(++GlobalSaveWaitWindowProgress); // 4
                //TRACE_I("analysing.SetProgressPos() savingProgress="<<savingProgress);

                SaveHistory(actKey, CONFIG_NAMEDHISTORY_REG, FindNamedHistory,
                            FIND_NAMED_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_LOOKINHISTORY_REG, FindLookInHistory,
                            FIND_LOOKIN_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_GREPHISTORY_REG, FindGrepHistory,
                            FIND_GREP_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_SELECTHISTORY_REG, Configuration.SelectHistory,
                            SELECT_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_COPYHISTORY_REG, Configuration.CopyHistory,
                            COPY_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_CHANGEDIRHISTORY_REG, Configuration.ChangeDirHistory,
                            CHANGEDIR_HISTORY_SIZE, !Configuration.SaveHistory);

                if (GlobalSaveWaitWindow == NULL)
                    analysing.SetProgressPos(++savingProgress); // 5
                else
                    GlobalSaveWaitWindow->SetProgressPos(++GlobalSaveWaitWindowProgress); // 5
                //TRACE_I("analysing.SetProgressPos() savingProgress="<<savingProgress);

                SaveHistory(actKey, CONFIG_VIEWERHISTORY_REG, ViewerHistory,
                            VIEWER_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_COMMANDHISTORY_REG, Configuration.EditHistory,
                            EDIT_HISTORY_SIZE, !(Configuration.SaveHistory && Configuration.EnableCmdLineHistory && Configuration.SaveCmdLineHistory));
                SaveHistory(actKey, CONFIG_FILELISTHISTORY_REG, Configuration.FileListHistory,
                            FILELIST_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_CREATEDIRHISTORY_REG, Configuration.CreateDirHistory,
                            CREATEDIR_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_QUICKRENAMEHISTORY_REG, Configuration.QuickRenameHistory,
                            QUICKRENAME_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_EDITNEWHISTORY_REG, Configuration.EditNewHistory,
                            EDITNEW_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_CONVERTHISTORY_REG, Configuration.ConvertHistory,
                            CONVERT_HISTORY_SIZE, !Configuration.SaveHistory);
                SaveHistory(actKey, CONFIG_FILTERHISTORY_REG, Configuration.FilterHistory,
                            FILTER_HISTORY_SIZE, !Configuration.SaveHistory);

                if (DirHistory != NULL)
                    DirHistory->SaveToRegistry(actKey, CONFIG_WORKDIRSHISTORY_REG, !Configuration.SaveWorkDirs);

                if (GlobalSaveWaitWindow == NULL)
                    analysing.SetProgressPos(++savingProgress); // 6
                else
                    GlobalSaveWaitWindow->SetProgressPos(++GlobalSaveWaitWindowProgress); // 6
                //TRACE_I("analysing.SetProgressPos() savingProgress="<<savingProgress);

                if (CreateKey(actKey, CONFIG_COPYMOVEOPTIONS_REG, actSubKey))
                {
                    CopyMoveOptions.Save(actSubKey);
                    CloseKey(actSubKey);
                }

                if (CreateKey(actKey, CONFIG_FINDOPTIONS_REG, actSubKey))
                {
                    FindOptions.Save(actSubKey);
                    CloseKey(actSubKey);
                }

                if (CreateKey(actKey, CONFIG_FINDIGNORE_REG, actSubKey))
                {
                    FindIgnore.Save(actSubKey);
                    CloseKey(actSubKey);
                }

                SetValue(actKey, CONFIG_FILELISTNAME_REG, REG_SZ, Configuration.FileListName, -1);
                SetValue(actKey, CONFIG_FILELISTAPPEND_REG, REG_DWORD, &Configuration.FileListAppend, sizeof(DWORD));
                SetValue(actKey, CONFIG_FILELISTDESTINATION_REG, REG_DWORD, &Configuration.FileListDestination, sizeof(DWORD));

                CloseKey(actKey);
            }

            //--- viewer

            if (CreateKey(salamander, SALAMANDER_VIEWER_REG, actKey))
            {
                SetValue(actKey, VIEWER_FINDFORWARD_REG, REG_DWORD,
                         &GlobalFindDialog.Forward, sizeof(DWORD));
                SetValue(actKey, VIEWER_FINDWHOLEWORDS_REG, REG_DWORD,
                         &GlobalFindDialog.WholeWords, sizeof(DWORD));
                SetValue(actKey, VIEWER_FINDCASESENSITIVE_REG, REG_DWORD,
                         &GlobalFindDialog.CaseSensitive, sizeof(DWORD));
                SetValue(actKey, VIEWER_FINDREGEXP_REG, REG_DWORD,
                         &GlobalFindDialog.Regular, sizeof(DWORD));
                SetValue(actKey, VIEWER_FINDTEXT_REG, REG_SZ, GlobalFindDialog.Text, -1);
                SetValue(actKey, VIEWER_FINDHEXMODE_REG, REG_DWORD,
                         &GlobalFindDialog.HexMode, sizeof(DWORD));

                SetValue(actKey, VIEWER_CONFIGCRLF_REG, REG_DWORD,
                         &Configuration.EOL_CRLF, sizeof(DWORD));
                SetValue(actKey, VIEWER_CONFIGCR_REG, REG_DWORD,
                         &Configuration.EOL_CR, sizeof(DWORD));
                SetValue(actKey, VIEWER_CONFIGLF_REG, REG_DWORD,
                         &Configuration.EOL_LF, sizeof(DWORD));
                SetValue(actKey, VIEWER_CONFIGNULL_REG, REG_DWORD,
                         &Configuration.EOL_NULL, sizeof(DWORD));
                SetValue(actKey, VIEWER_CONFIGTABSIZE_REG, REG_DWORD,
                         &Configuration.TabSize, sizeof(DWORD));
                SetValue(actKey, VIEWER_CONFIGDEFMODE_REG, REG_DWORD,
                         &Configuration.DefViewMode, sizeof(DWORD));
                SetValue(actKey, VIEWER_CONFIGTEXTMASK_REG, REG_SZ,
                         Configuration.TextModeMasks.GetMasksString(), -1);
                SetValue(actKey, VIEWER_CONFIGHEXMASK_REG, REG_SZ,
                         Configuration.HexModeMasks.GetMasksString(), -1);
                SetValue(actKey, VIEWER_CONFIGUSECUSTOMFONT_REG, REG_DWORD,
                         &UseCustomViewerFont, sizeof(DWORD));
                SaveLogFont(actKey, VIEWER_CONFIGFONT_REG, &ViewerLogFont);
                SetValue(actKey, VIEWER_WRAPTEXT_REG, REG_DWORD,
                         &Configuration.WrapText, sizeof(DWORD));
                SetValue(actKey, VIEWER_CPAUTOSELECT_REG, REG_DWORD,
                         &Configuration.CodePageAutoSelect, sizeof(DWORD));
                SetValue(actKey, VIEWER_DEFAULTCONVERT_REG, REG_SZ, Configuration.DefaultConvert, -1);
                SetValue(actKey, VIEWER_AUTOCOPYSELECTION_REG, REG_DWORD,
                         &Configuration.AutoCopySelection, sizeof(DWORD));
                SetValue(actKey, VIEWER_GOTOOFFSETISHEX_REG, REG_DWORD,
                         &Configuration.GoToOffsetIsHex, sizeof(DWORD));

                SetValue(actKey, VIEWER_CONFIGSAVEWINPOS_REG, REG_DWORD,
                         &Configuration.SavePosition, sizeof(DWORD));
                if (Configuration.WindowPlacement.length != 0)
                {
                    SetValue(actKey, VIEWER_CONFIGWNDLEFT_REG, REG_DWORD,
                             &Configuration.WindowPlacement.rcNormalPosition.left, sizeof(DWORD));
                    SetValue(actKey, VIEWER_CONFIGWNDRIGHT_REG, REG_DWORD,
                             &Configuration.WindowPlacement.rcNormalPosition.right, sizeof(DWORD));
                    SetValue(actKey, VIEWER_CONFIGWNDTOP_REG, REG_DWORD,
                             &Configuration.WindowPlacement.rcNormalPosition.top, sizeof(DWORD));
                    SetValue(actKey, VIEWER_CONFIGWNDBOTTOM_REG, REG_DWORD,
                             &Configuration.WindowPlacement.rcNormalPosition.bottom, sizeof(DWORD));
                    SetValue(actKey, VIEWER_CONFIGWNDSHOW_REG, REG_DWORD,
                             &Configuration.WindowPlacement.showCmd, sizeof(DWORD));
                }

                CloseKey(actKey);
            }

            //--- user menu

            if (CreateKey(salamander, SALAMANDER_USERMENU_REG, actKey))
            {
                ClearKey(actKey);

                HKEY subKey;
                char buf[30];
                int i;
                for (i = 0; i < UserMenuItems->Count; i++)
                {
                    itoa(i + 1, buf, 10);
                    if (CreateKey(actKey, buf, subKey))
                    {
                        SetValue(subKey, USERMENU_ITEMNAME_REG, REG_SZ, UserMenuItems->At(i)->ItemName, -1);
                        SetValue(subKey, USERMENU_COMMAND_REG, REG_SZ, UserMenuItems->At(i)->UMCommand, -1);
                        SetValue(subKey, USERMENU_ARGUMENTS_REG, REG_SZ, UserMenuItems->At(i)->Arguments, -1);
                        SetValue(subKey, USERMENU_INITDIR_REG, REG_SZ, UserMenuItems->At(i)->InitDir, -1);
                        SetValue(subKey, USERMENU_SHELL_REG, REG_DWORD,
                                 &UserMenuItems->At(i)->ThroughShell, sizeof(DWORD));
                        SetValue(subKey, USERMENU_CLOSE_REG, REG_DWORD,
                                 &UserMenuItems->At(i)->CloseShell, sizeof(DWORD));
                        SetValue(subKey, USERMENU_USEWINDOW_REG, REG_DWORD,
                                 &UserMenuItems->At(i)->UseWindow, sizeof(DWORD));

                        SetValue(subKey, USERMENU_ICON_REG, REG_SZ, UserMenuItems->At(i)->Icon, -1);
                        SetValue(subKey, USERMENU_TYPE_REG, REG_DWORD,
                                 &UserMenuItems->At(i)->Type, sizeof(DWORD));
                        SetValue(subKey, USERMENU_SHOWINTOOLBAR_REG, REG_DWORD,
                                 &UserMenuItems->At(i)->ShowInToolbar, sizeof(DWORD));

                        CloseKey(subKey);
                    }
                    else
                        break;
                }
                CloseKey(actKey);
            }

            //--- internal ZIP packer

            if (Configuration.ConfigVersion < 6 && // only old config, otherwise we do not create+clean the key
                CreateKey(salamander, SALAMANDER_IZIP_REG, actKey))
            {
                ClearKey(actKey);

                CloseKey(actKey);

                DeleteKey(salamander, SALAMANDER_IZIP_REG);
            }

            //--- viewers

            SaveViewers(salamander, SALAMANDER_VIEWERS_REG, ViewerMasks);
            SaveViewers(salamander, SALAMANDER_ALTVIEWERS_REG, AltViewerMasks);

            //---  editors

            SaveEditors(salamander, SALAMANDER_EDITORS_REG, EditorMasks);

            if (GlobalSaveWaitWindow == NULL)
                analysing.SetProgressPos(++savingProgress); // 7
            else
                GlobalSaveWaitWindow->SetProgressPos(++GlobalSaveWaitWindowProgress); // 7
            //TRACE_I("analysing.SetProgressPos() savingProgress="<<savingProgress);

            //--- colors
            if (CreateKey(salamander, SALAMANDER_CUSTOMCOLORS_REG, actKey))
            {
                char buff[10];
                int i;
                for (i = 0; i < NUMBER_OF_CUSTOMCOLORS; i++)
                {
                    itoa(i + 1, buff, 10);
                    SaveRGB(actKey, buff, CustomColors[i]);
                }

                CloseKey(actKey);
            }

            if (CreateKey(salamander, SALAMANDER_COLORS_REG, actKey))
            {
                DWORD scheme = 4; // custom
                if (CurrentColors == SalamanderColors)
                    scheme = 0;
                else if (CurrentColors == ExplorerColors)
                    scheme = 1;
                else if (CurrentColors == NortonColors)
                    scheme = 2;
                else if (CurrentColors == NavigatorColors)
                    scheme = 3;
                SetValue(actKey, SALAMANDER_CLRSCHEME_REG, REG_DWORD, &scheme, sizeof(DWORD));

                SaveRGBF(actKey, SALAMANDER_CLR_FOCUS_ACTIVE_NORMAL_REG, UserColors[FOCUS_ACTIVE_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_FOCUS_ACTIVE_SELECTED_REG, UserColors[FOCUS_ACTIVE_SELECTED]);
                SaveRGBF(actKey, SALAMANDER_CLR_FOCUS_INACTIVE_NORMAL_REG, UserColors[FOCUS_FG_INACTIVE_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_FOCUS_INACTIVE_SELECTED_REG, UserColors[FOCUS_FG_INACTIVE_SELECTED]);
                SaveRGBF(actKey, SALAMANDER_CLR_FOCUS_BK_INACTIVE_NORMAL_REG, UserColors[FOCUS_BK_INACTIVE_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_FOCUS_BK_INACTIVE_SELECTED_REG, UserColors[FOCUS_BK_INACTIVE_SELECTED]);

                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_FG_NORMAL_REG, UserColors[ITEM_FG_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_FG_SELECTED_REG, UserColors[ITEM_FG_SELECTED]);
                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_FG_FOCUSED_REG, UserColors[ITEM_FG_FOCUSED]);
                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_FG_FOCSEL_REG, UserColors[ITEM_FG_FOCSEL]);
                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_FG_HIGHLIGHT_REG, UserColors[ITEM_FG_HIGHLIGHT]);

                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_BK_NORMAL_REG, UserColors[ITEM_BK_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_BK_SELECTED_REG, UserColors[ITEM_BK_SELECTED]);
                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_BK_FOCUSED_REG, UserColors[ITEM_BK_FOCUSED]);
                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_BK_FOCSEL_REG, UserColors[ITEM_BK_FOCSEL]);
                SaveRGBF(actKey, SALAMANDER_CLR_ITEM_BK_HIGHLIGHT_REG, UserColors[ITEM_BK_HIGHLIGHT]);

                SaveRGBF(actKey, SALAMANDER_CLR_ICON_BLEND_SELECTED_REG, UserColors[ICON_BLEND_SELECTED]);
                SaveRGBF(actKey, SALAMANDER_CLR_ICON_BLEND_FOCUSED_REG, UserColors[ICON_BLEND_FOCUSED]);
                SaveRGBF(actKey, SALAMANDER_CLR_ICON_BLEND_FOCSEL_REG, UserColors[ICON_BLEND_FOCSEL]);

                SaveRGBF(actKey, SALAMANDER_CLR_PROGRESS_FG_NORMAL_REG, UserColors[PROGRESS_FG_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_PROGRESS_FG_SELECTED_REG, UserColors[PROGRESS_FG_SELECTED]);
                SaveRGBF(actKey, SALAMANDER_CLR_PROGRESS_BK_NORMAL_REG, UserColors[PROGRESS_BK_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_PROGRESS_BK_SELECTED_REG, UserColors[PROGRESS_BK_SELECTED]);

                SaveRGBF(actKey, SALAMANDER_CLR_HOT_PANEL_REG, UserColors[HOT_PANEL]);
                SaveRGBF(actKey, SALAMANDER_CLR_HOT_ACTIVE_REG, UserColors[HOT_ACTIVE]);
                SaveRGBF(actKey, SALAMANDER_CLR_HOT_INACTIVE_REG, UserColors[HOT_INACTIVE]);

                SaveRGBF(actKey, SALAMANDER_CLR_ACTIVE_CAPTION_FG_REG, UserColors[ACTIVE_CAPTION_FG]);
                SaveRGBF(actKey, SALAMANDER_CLR_ACTIVE_CAPTION_BK_REG, UserColors[ACTIVE_CAPTION_BK]);
                SaveRGBF(actKey, SALAMANDER_CLR_INACTIVE_CAPTION_FG_REG, UserColors[INACTIVE_CAPTION_FG]);
                SaveRGBF(actKey, SALAMANDER_CLR_INACTIVE_CAPTION_BK_REG, UserColors[INACTIVE_CAPTION_BK]);

                SaveRGBF(actKey, SALAMANDER_CLR_THUMBNAIL_FRAME_NORMAL_REG, UserColors[THUMBNAIL_FRAME_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_THUMBNAIL_FRAME_SELECTED_REG, UserColors[THUMBNAIL_FRAME_SELECTED]);
                SaveRGBF(actKey, SALAMANDER_CLR_THUMBNAIL_FRAME_FOCUSED_REG, UserColors[THUMBNAIL_FRAME_FOCUSED]);
                SaveRGBF(actKey, SALAMANDER_CLR_THUMBNAIL_FRAME_FOCSEL_REG, UserColors[THUMBNAIL_FRAME_FOCSEL]);

                SaveRGBF(actKey, SALAMANDER_CLR_VIEWER_FG_NORMAL_REG, ViewerColors[VIEWER_FG_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_VIEWER_BK_NORMAL_REG, ViewerColors[VIEWER_BK_NORMAL]);
                SaveRGBF(actKey, SALAMANDER_CLR_VIEWER_FG_SELECTED_REG, ViewerColors[VIEWER_FG_SELECTED]);
                SaveRGBF(actKey, SALAMANDER_CLR_VIEWER_BK_SELECTED_REG, ViewerColors[VIEWER_BK_SELECTED]);

                // save colors for file highlighting
                HKEY hHltKey;
                if (CreateKey(actKey, SALAMANDER_HLT, hHltKey))
                {
                    ClearKey(hHltKey);
                    HKEY hSubKey;
                    char buf[30];
                    int i;
                    for (i = 0; i < HighlightMasks->Count; i++)
                    {
                        itoa(i + 1, buf, 10);
                        if (CreateKey(hHltKey, buf, hSubKey))
                        {
                            CHighlightMasksItem* item = HighlightMasks->At(i);
                            SetValue(hSubKey, SALAMANDER_HLT_ITEM_MASKS, REG_SZ, item->Masks->GetMasksString(), -1);
                            SetValue(hSubKey, SALAMANDER_HLT_ITEM_ATTR, REG_DWORD, &item->Attr, sizeof(DWORD));
                            SetValue(hSubKey, SALAMANDER_HLT_ITEM_VALIDATTR, REG_DWORD, &item->ValidAttr, sizeof(DWORD));

                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_NORMAL_REG, item->NormalFg);
                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_SELECTED_REG, item->SelectedFg);
                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_FOCUSED_REG, item->FocusedFg);
                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_FOCSEL_REG, item->FocSelFg);
                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_HIGHLIGHT_REG, item->HighlightFg);

                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_NORMAL_REG, item->NormalBk);
                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_SELECTED_REG, item->SelectedBk);
                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_FOCUSED_REG, item->FocusedBk);
                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_FOCSEL_REG, item->FocSelBk);
                            SaveRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_HIGHLIGHT_REG, item->HighlightBk);
                            CloseKey(hSubKey);
                        }
                    }
                    CloseKey(hHltKey);
                }
                CloseKey(actKey);
            }

            if (GlobalSaveWaitWindow == NULL)
                analysing.SetProgressPos(++savingProgress); // 8
            else
                GlobalSaveWaitWindow->SetProgressPos(++GlobalSaveWaitWindowProgress); // 8
            //TRACE_I("analysing.SetProgressPos() savingProgress="<<savingProgress);

            if (deleteSALAMANDER_SAVE_IN_PROGRESS)
            {
                DeleteValue(salamander, SALAMANDER_SAVE_IN_PROGRESS);
                IsSetSALAMANDER_SAVE_IN_PROGRESS = FALSE;
            }
        }
        CloseKey(salamander);
    }

    LoadSaveToRegistryMutex.Leave();

    if (GlobalSaveWaitWindow == NULL)
    {
        EnableWindow(parent, TRUE);
        PluginMsgBoxParent = oldPluginMsgBoxParent;
        DestroyWindow(analysing.HWindow);
        SetCursor(hOldCursor);
    }
}

void CMainWindow::LoadPanelConfig(char* panelPath, CFilesWindow* panel, HKEY hSalamander, const char* reg)
{
    HKEY actKey;
    if (OpenKey(hSalamander, reg, actKey))
    {
        DWORD value;
        if (GetValue(actKey, PANEL_PATH_REG, REG_SZ, panelPath, MAX_PATH))
        {
            if (GetValue(actKey, PANEL_HEADER_REG, REG_DWORD, &value, sizeof(DWORD)))
                panel->HeaderLineVisible = value;
            if (GetValue(actKey, PANEL_VIEW_REG, REG_DWORD, &value, sizeof(DWORD)))
            {
                if (Configuration.ConfigVersion < 13 && !value) // conversion: Detailed was saved as FALSE
                    value = 2;
                panel->SelectViewTemplate(value, FALSE, FALSE, VALID_DATA_ALL, FALSE, TRUE);
            }
            if (GetValue(actKey, PANEL_REVERSE_REG, REG_DWORD, &value, sizeof(DWORD)))
                panel->ReverseSort = value;
            if (GetValue(actKey, PANEL_SORT_REG, REG_DWORD, &value, sizeof(DWORD)))
            {
                if (value > stAttr)
                    value = stName;
                panel->SortType = (CSortType)value;
            }
            if (GetValue(actKey, PANEL_DIRLINE_REG, REG_DWORD, &value, sizeof(DWORD)))
                if ((BOOL)value != (panel->DirectoryLine->HWindow != NULL))
                    panel->ToggleDirectoryLine();
            if (GetValue(actKey, PANEL_STATUS_REG, REG_DWORD, &value, sizeof(DWORD)))
                if ((BOOL)value != (panel->StatusLine->HWindow != NULL))
                    panel->ToggleStatusLine();
            GetValue(actKey, PANEL_FILTER_ENABLE, REG_DWORD, &panel->FilterEnabled,
                     sizeof(DWORD));

            char filter[MAX_PATH];
            if (!GetValue(actKey, PANEL_FILTER, REG_SZ, filter, MAX_PATH))
            {
                filter[0] = 0;
                if (Configuration.ConfigVersion < 22)
                {
                    char* filterHistory[1];
                    filterHistory[0] = NULL;
                    LoadHistory(actKey, PANEL_FILTERHISTORY_REG, filterHistory, 1);
                    if (filterHistory[0] != NULL) // Initial state of the filter is also loaded
                    {
                        DWORD filterInverse = FALSE;
                        if (panel->FilterEnabled && Configuration.ConfigVersion < 14) // conversion: the checkbox for inverse filter was removed
                            GetValue(actKey, PANEL_FILTER_INVERSE, REG_DWORD, &filterInverse, sizeof(DWORD));
                        if (filterInverse)
                            strcpy(filter, "|");
                        else
                            filter[0] = 0;
                        strcat(filter, filterHistory[0]);
                        free(filterHistory[0]);
                    }
                }
                else
                    panel->FilterEnabled = FALSE;
            }
            if (filter[0] != 0)
                panel->Filter.SetMasksString(filter);

            panel->UpdateFilterSymbol();
            int errPos;
            if (!panel->Filter.PrepareMasks(errPos))
            {
                panel->Filter.SetMasksString("*.*");
                panel->Filter.PrepareMasks(errPos);
            }
        }

        CloseKey(actKey);
    }
}

void LoadIconOvrlsInfo(const char* root)
{
    HKEY hSalamander;
    if (OpenKey(HKEY_CURRENT_USER, root, hSalamander))
    {
        HKEY actKey;
        DWORD configVersion = 1; // This is config from 1.52 and older
        if (OpenKey(hSalamander, SALAMANDER_VERSION_REG, actKey))
        {
            configVersion = 2; // This is the config from 1.6b1
            GetValue(actKey, SALAMANDER_VERSIONREG_REG, REG_DWORD,
                     &configVersion, sizeof(DWORD));
            CloseKey(actKey);
        }
        if (OpenKey(hSalamander, SALAMANDER_CONFIG_REG, actKey))
        {
            ClearListOfDisabledCustomIconOverlays();
            DWORD disabledCustomIconOverlaysBufSize;
            if (GetValue(actKey, CONFIG_ENABLECUSTICOVRLS_REG, REG_DWORD,
                         &Configuration.EnableCustomIconOverlays, sizeof(DWORD)) &&
                GetSize(actKey, CONFIG_DISABLEDCUSTICOVRLS_REG, REG_SZ, disabledCustomIconOverlaysBufSize))
            {
                if (disabledCustomIconOverlaysBufSize > 1) // <= 1 means an empty string and NULL is sufficient for that
                {
                    Configuration.DisabledCustomIconOverlays = (char*)malloc(disabledCustomIconOverlaysBufSize);
                    if (Configuration.DisabledCustomIconOverlays == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        Configuration.EnableCustomIconOverlays = FALSE; // for security reasons (icon overlay handlers often crash)
                    }
                    else
                    {
                        if (!GetValue(actKey, CONFIG_DISABLEDCUSTICOVRLS_REG, REG_SZ,
                                      Configuration.DisabledCustomIconOverlays, disabledCustomIconOverlaysBufSize))
                        {
                            free(Configuration.DisabledCustomIconOverlays);
                            Configuration.DisabledCustomIconOverlays = NULL;
                            Configuration.EnableCustomIconOverlays = FALSE; // for security reasons (icon overlay handlers often crash)
                        }
                    }
                }
            }
            else
            {
                if (configVersion >= 41) // if this value is missing in the new configurations, we will disable overlays (in older versions these variables were not present, so it is not an error - overlays will remain enabled)
                    Configuration.EnableCustomIconOverlays = FALSE;
            }

            CloseKey(actKey);
        }
        CloseKey(hSalamander);
    }
}

BOOL CMainWindow::LoadConfig(BOOL importingOldConfig, const CCommandLineParams* cmdLineParams)
{
    CALL_STACK_MESSAGE2("CMainWindow::LoadConfig(%d)", importingOldConfig);
    if (SALAMANDER_ROOT_REG == NULL)
        return FALSE;

    LoadSaveToRegistryMutex.Enter();

    HKEY salamander;
    if (OpenKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
    {
        HKEY actKey;
        BOOL ret = TRUE;

        IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_CONFIG));

        Configuration.ConfigVersion = 1; // This is config from 1.52 and older
                                         //--- version
        if (OpenKey(salamander, SALAMANDER_VERSION_REG, actKey))
        {
            Configuration.ConfigVersion = 2; // This is the config from 1.6b1
            GetValue(actKey, SALAMANDER_VERSIONREG_REG, REG_DWORD,
                     &Configuration.ConfigVersion, sizeof(DWORD));
            CloseKey(actKey);
        }

        //--- viewers

        EnterViewerMasksCS();
        LoadViewers(salamander, SALAMANDER_VIEWERS_REG, ViewerMasks);
        LeaveViewerMasksCS();
        LoadViewers(salamander, SALAMANDER_ALTVIEWERS_REG, AltViewerMasks);

        //---  editors

        LoadEditors(salamander, SALAMANDER_EDITORS_REG, EditorMasks);

        //--- colors
        if (OpenKey(salamander, SALAMANDER_CUSTOMCOLORS_REG, actKey))
        {
            char buff[10];
            int i;
            for (i = 0; i < NUMBER_OF_CUSTOMCOLORS; i++)
            {
                itoa(i + 1, buff, 10);
                LoadRGB(actKey, buff, CustomColors[i]);
            }

            CloseKey(actKey);
        }

        if (OpenKey(salamander, SALAMANDER_COLORS_REG, actKey))
        {
            DWORD scheme;
            CurrentColors = UserColors;
            if (GetValue(actKey, SALAMANDER_CLRSCHEME_REG, REG_DWORD, &scheme, sizeof(DWORD)))
            {
                // we added a new schema (DOS Navigator) at position 3
                if (Configuration.ConfigVersion < 28 && scheme == 3)
                    scheme = 4;

                if (scheme == 0)
                    CurrentColors = SalamanderColors;
                else if (scheme == 1)
                    CurrentColors = ExplorerColors;
                else if (scheme == 2)
                    CurrentColors = NortonColors;
                else if (scheme == 3)
                    CurrentColors = NavigatorColors;
            }

            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_FG_NORMAL_REG, UserColors[ITEM_FG_NORMAL]);
            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_FG_SELECTED_REG, UserColors[ITEM_FG_SELECTED]);
            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_FG_FOCUSED_REG, UserColors[ITEM_FG_FOCUSED]);
            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_FG_FOCSEL_REG, UserColors[ITEM_FG_FOCSEL]);
            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_FG_HIGHLIGHT_REG, UserColors[ITEM_FG_HIGHLIGHT]);

            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_BK_NORMAL_REG, UserColors[ITEM_BK_NORMAL]);
            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_BK_SELECTED_REG, UserColors[ITEM_BK_SELECTED]);
            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_BK_FOCUSED_REG, UserColors[ITEM_BK_FOCUSED]);
            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_BK_FOCSEL_REG, UserColors[ITEM_BK_FOCSEL]);
            LoadRGBF(actKey, SALAMANDER_CLR_ITEM_BK_HIGHLIGHT_REG, UserColors[ITEM_BK_HIGHLIGHT]);

            LoadRGBF(actKey, SALAMANDER_CLR_FOCUS_ACTIVE_NORMAL_REG, UserColors[FOCUS_ACTIVE_NORMAL]);
            LoadRGBF(actKey, SALAMANDER_CLR_FOCUS_ACTIVE_SELECTED_REG, UserColors[FOCUS_ACTIVE_SELECTED]);
            LoadRGBF(actKey, SALAMANDER_CLR_FOCUS_INACTIVE_NORMAL_REG, UserColors[FOCUS_FG_INACTIVE_NORMAL]);
            LoadRGBF(actKey, SALAMANDER_CLR_FOCUS_INACTIVE_SELECTED_REG, UserColors[FOCUS_FG_INACTIVE_SELECTED]);
            if (!LoadRGBF(actKey, SALAMANDER_CLR_FOCUS_BK_INACTIVE_NORMAL_REG, UserColors[FOCUS_BK_INACTIVE_NORMAL]))
                UserColors[FOCUS_BK_INACTIVE_NORMAL] = UserColors[ITEM_BK_NORMAL]; // conversion of older configurations
            if (!LoadRGBF(actKey, SALAMANDER_CLR_FOCUS_BK_INACTIVE_SELECTED_REG, UserColors[FOCUS_BK_INACTIVE_SELECTED]))
                UserColors[FOCUS_BK_INACTIVE_SELECTED] = UserColors[ITEM_BK_NORMAL]; // conversion of older configurations

            LoadRGBF(actKey, SALAMANDER_CLR_ICON_BLEND_SELECTED_REG, UserColors[ICON_BLEND_SELECTED]);
            LoadRGBF(actKey, SALAMANDER_CLR_ICON_BLEND_FOCUSED_REG, UserColors[ICON_BLEND_FOCUSED]);
            LoadRGBF(actKey, SALAMANDER_CLR_ICON_BLEND_FOCSEL_REG, UserColors[ICON_BLEND_FOCSEL]);

            LoadRGBF(actKey, SALAMANDER_CLR_PROGRESS_FG_NORMAL_REG, UserColors[PROGRESS_FG_NORMAL]);
            LoadRGBF(actKey, SALAMANDER_CLR_PROGRESS_FG_SELECTED_REG, UserColors[PROGRESS_FG_SELECTED]);
            LoadRGBF(actKey, SALAMANDER_CLR_PROGRESS_BK_NORMAL_REG, UserColors[PROGRESS_BK_NORMAL]);
            LoadRGBF(actKey, SALAMANDER_CLR_PROGRESS_BK_SELECTED_REG, UserColors[PROGRESS_BK_SELECTED]);

            LoadRGBF(actKey, SALAMANDER_CLR_HOT_PANEL_REG, UserColors[HOT_PANEL]);
            LoadRGBF(actKey, SALAMANDER_CLR_HOT_ACTIVE_REG, UserColors[HOT_ACTIVE]);
            LoadRGBF(actKey, SALAMANDER_CLR_HOT_INACTIVE_REG, UserColors[HOT_INACTIVE]);

            LoadRGBF(actKey, SALAMANDER_CLR_ACTIVE_CAPTION_FG_REG, UserColors[ACTIVE_CAPTION_FG]);
            LoadRGBF(actKey, SALAMANDER_CLR_ACTIVE_CAPTION_BK_REG, UserColors[ACTIVE_CAPTION_BK]);
            LoadRGBF(actKey, SALAMANDER_CLR_INACTIVE_CAPTION_FG_REG, UserColors[INACTIVE_CAPTION_FG]);
            LoadRGBF(actKey, SALAMANDER_CLR_INACTIVE_CAPTION_BK_REG, UserColors[INACTIVE_CAPTION_BK]);

            LoadRGBF(actKey, SALAMANDER_CLR_THUMBNAIL_FRAME_NORMAL_REG, UserColors[THUMBNAIL_FRAME_NORMAL]);
            LoadRGBF(actKey, SALAMANDER_CLR_THUMBNAIL_FRAME_SELECTED_REG, UserColors[THUMBNAIL_FRAME_SELECTED]);
            LoadRGBF(actKey, SALAMANDER_CLR_THUMBNAIL_FRAME_FOCUSED_REG, UserColors[THUMBNAIL_FRAME_FOCUSED]);
            LoadRGBF(actKey, SALAMANDER_CLR_THUMBNAIL_FRAME_FOCSEL_REG, UserColors[THUMBNAIL_FRAME_FOCSEL]);

            LoadRGBF(actKey, SALAMANDER_CLR_VIEWER_FG_NORMAL_REG, ViewerColors[VIEWER_FG_NORMAL]);
            LoadRGBF(actKey, SALAMANDER_CLR_VIEWER_BK_NORMAL_REG, ViewerColors[VIEWER_BK_NORMAL]);
            LoadRGBF(actKey, SALAMANDER_CLR_VIEWER_FG_SELECTED_REG, ViewerColors[VIEWER_FG_SELECTED]);
            LoadRGBF(actKey, SALAMANDER_CLR_VIEWER_BK_SELECTED_REG, ViewerColors[VIEWER_BK_SELECTED]);

            // load colors for file highlighting
            HKEY hHltKey;
            if (OpenKey(actKey, SALAMANDER_HLT, hHltKey))
            {
                HKEY hSubKey;
                char buf[30];
                strcpy(buf, "1");
                int i = 1;
                HighlightMasks->DestroyMembers();
                while (OpenKey(hHltKey, buf, hSubKey))
                {
                    char masks[MAX_PATH];
                    if (GetValue(hSubKey, SALAMANDER_HLT_ITEM_MASKS, REG_SZ, masks, MAX_PATH))
                    {
                        CHighlightMasksItem* item = new CHighlightMasksItem();
                        if (item == NULL || !item->Set(masks))
                        {
                            TRACE_E(LOW_MEMORY);
                            if (item != NULL)
                                delete item;
                            continue;
                        }
                        int errPos;
                        item->Masks->PrepareMasks(errPos);

                        GetValue(hSubKey, SALAMANDER_HLT_ITEM_ATTR, REG_DWORD, &item->Attr, sizeof(DWORD));
                        GetValue(hSubKey, SALAMANDER_HLT_ITEM_VALIDATTR, REG_DWORD, &item->ValidAttr, sizeof(DWORD));

                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_NORMAL_REG, item->NormalFg);
                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_SELECTED_REG, item->SelectedFg);
                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_FOCUSED_REG, item->FocusedFg);
                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_FOCSEL_REG, item->FocSelFg);
                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_FG_HIGHLIGHT_REG, item->HighlightFg);

                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_NORMAL_REG, item->NormalBk);
                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_SELECTED_REG, item->SelectedBk);
                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_FOCUSED_REG, item->FocusedBk);
                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_FOCSEL_REG, item->FocSelBk);
                        LoadRGBF(hSubKey, SALAMANDER_HLT_ITEM_BK_HIGHLIGHT_REG, item->HighlightBk);
                        HighlightMasks->Add(item);
                        if (!HighlightMasks->IsGood())
                        {
                            HighlightMasks->ResetState();
                            delete item;
                        }
                        itoa(++i, buf, 10);
                        CloseKey(hSubKey);
                    }
                }
                if (Configuration.ConfigVersion < 16) // add coloring to Encrypted file/directory
                {
                    CHighlightMasksItem* hItem = new CHighlightMasksItem();
                    if (hItem != NULL)
                    {
                        HighlightMasks->Add(hItem);
                        hItem->Set("*.*");
                        int errPos;
                        hItem->Masks->PrepareMasks(errPos);
                        hItem->NormalFg = RGBF(19, 143, 13, 0); // color taken from Windows XP
                        hItem->FocusedFg = RGBF(19, 143, 13, 0);
                        hItem->ValidAttr = FILE_ATTRIBUTE_ENCRYPTED;
                        hItem->Attr = FILE_ATTRIBUTE_ENCRYPTED;
                    }
                }
                CloseKey(hHltKey);
            }

            ColorsChanged(FALSE, TRUE, TRUE); // We save time, we only let color-dependent items change

            CloseKey(actKey);
        }

        //--- window

        WINDOWPLACEMENT place;
        BOOL useWinPlacement = FALSE;
        if (OpenKey(salamander, SALAMANDER_WINDOW_REG, actKey))
        {
            place.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(HWindow, &place);
            if (GetValue(actKey, WINDOW_LEFT_REG, REG_DWORD,
                         &(place.rcNormalPosition.left), sizeof(DWORD)) &&
                GetValue(actKey, WINDOW_RIGHT_REG, REG_DWORD,
                         &(place.rcNormalPosition.right), sizeof(DWORD)) &&
                GetValue(actKey, WINDOW_TOP_REG, REG_DWORD,
                         &(place.rcNormalPosition.top), sizeof(DWORD)) &&
                GetValue(actKey, WINDOW_BOTTOM_REG, REG_DWORD,
                         &(place.rcNormalPosition.bottom), sizeof(DWORD)) &&
                GetValue(actKey, WINDOW_SHOW_REG, REG_DWORD,
                         &(place.showCmd), sizeof(DWORD)))
            {
                char buf[20];
                if (GetValue(actKey, WINDOW_SPLIT_REG, REG_SZ, buf, 20))
                {
                    sscanf(buf, "%lf", &SplitPosition);
                    SplitPosition /= 100;
                    if (SplitPosition < 0)
                        SplitPosition = 0;
                    if (SplitPosition > 1)
                        SplitPosition = 1;
                }
                if (GetValue(actKey, WINDOW_BEFOREZOOMSPLIT_REG, REG_SZ, buf, 20))
                {
                    sscanf(buf, "%lf", &BeforeZoomSplitPosition);
                    BeforeZoomSplitPosition /= 100;
                    if (BeforeZoomSplitPosition < 0)
                        BeforeZoomSplitPosition = 0;
                    if (BeforeZoomSplitPosition > 1)
                        BeforeZoomSplitPosition = 1;
                }
                useWinPlacement = TRUE;
            }
            else
                ret = FALSE;

            CloseKey(actKey);
        }
        else
            ret = FALSE;

        if (OpenKey(salamander, FINDDIALOG_WINDOW_REG, actKey))
        {
            Configuration.FindDialogWindowPlacement.length = sizeof(WINDOWPLACEMENT);

            GetValue(actKey, WINDOW_LEFT_REG, REG_DWORD,
                     &(Configuration.FindDialogWindowPlacement.rcNormalPosition.left), sizeof(DWORD));
            GetValue(actKey, WINDOW_RIGHT_REG, REG_DWORD,
                     &(Configuration.FindDialogWindowPlacement.rcNormalPosition.right), sizeof(DWORD));
            GetValue(actKey, WINDOW_TOP_REG, REG_DWORD,
                     &(Configuration.FindDialogWindowPlacement.rcNormalPosition.top), sizeof(DWORD));
            GetValue(actKey, WINDOW_BOTTOM_REG, REG_DWORD,
                     &(Configuration.FindDialogWindowPlacement.rcNormalPosition.bottom), sizeof(DWORD));
            GetValue(actKey, WINDOW_SHOW_REG, REG_DWORD,
                     &(Configuration.FindDialogWindowPlacement.showCmd), sizeof(DWORD));

            GetValue(actKey, FINDDIALOG_NAMEWIDTH_REG, REG_DWORD,
                     &(Configuration.FindColNameWidth), sizeof(DWORD));
            CloseKey(actKey);
        }

        //--- default directories

        if (OpenKey(salamander, SALAMANDER_DEFDIRS_REG, actKey))
        {
            DWORD values;
            DWORD res = RegQueryInfoKey(actKey, NULL, 0, 0, NULL, NULL, NULL, &values, NULL,
                                        NULL, NULL, NULL);
            if (res == ERROR_SUCCESS)
            {
                char dir[4] = " :\\"; // reset DefaultDir
                char d;
                for (d = 'A'; d <= 'Z'; d++)
                {
                    dir[0] = d;
                    strcpy(DefaultDir[d - 'A'], dir);
                }

                char name[2];
                BYTE path[MAX_PATH];
                DWORD nameLen, dataLen, type;

                int i;
                for (i = 0; i < (int)values; i++)
                {
                    nameLen = 2;
                    dataLen = MAX_PATH;
                    res = RegEnumValue(actKey, i, name, &nameLen, 0, &type, path, &dataLen);
                    if (res == ERROR_SUCCESS)
                        if (type == REG_SZ)
                        {
                            char d2 = LowerCase[name[0]];
                            if (d2 >= 'a' && d2 <= 'z')
                            {
                                if (dataLen > 2 && LowerCase[path[0]] == d2 &&
                                    path[1] == ':' && path[2] == '\\')
                                    memmove(DefaultDir[d2 - 'a'], path, dataLen);
                                else
                                    SalMessageBox(HWindow, LoadStr(IDS_UNEXPECTEDVALUE),
                                                  LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
                            }
                            else
                                SalMessageBox(HWindow, LoadStr(IDS_UNEXPECTEDVALUE),
                                              LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
                        }
                        else
                            SalMessageBox(HWindow, LoadStr(IDS_UNEXPECTEDVALUETYPE),
                                          LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
                    else
                        SalMessageBox(HWindow, GetErrorText(res),
                                      LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
                }
            }
            else if (res != ERROR_FILE_NOT_FOUND)
                SalMessageBox(HWindow, GetErrorText(res),
                              LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
            CloseKey(actKey);
        }

        //--- password manager

        if (OpenKey(salamander, SALAMANDER_PWDMNGR_REG, actKey))
        {
            PasswordManager.Load(actKey);
            CloseKey(actKey);
        }

        //--- hot paths

        if (OpenKey(salamander, SALAMANDER_HOTPATHS_REG, actKey))
        {
            if (Configuration.ConfigVersion == 1) // HotPaths needs to be converted
                HotPaths.Load1_52(actKey);
            else
                HotPaths.Load(actKey);

            CloseKey(actKey);
        }

        //--- view templates

        if (OpenKey(salamander, SALAMANDER_VIEWTEMPLATES_REG, actKey))
        {
            ViewTemplates.Load(actKey);
            CloseKey(actKey);
        }

        //--- Plugins Order
        if (OpenKey(salamander, SALAMANDER_PLUGINSORDER, actKey))
        {
            Plugins.LoadOrder(HWindow, actKey);
            CloseKey(actKey);
        }

        //--- Plugins
        if (OpenKey(salamander, SALAMANDER_PLUGINS, actKey)) // otherwise default values
        {
            Plugins.Load(HWindow, actKey);
            CloseKey(actKey);
        }
        else
        {
            if (Configuration.ConfigVersion >= 6)
                Plugins.Clear(); // It doesn't even want default archivers ...
        }

        //--- Packers & Unpackers
        if (OpenKey(salamander, SALAMANDER_PACKANDUNPACK, actKey))
        {
            GetValue(actKey, SALAMANDER_SIMPLEICONSINARCHIVES, REG_DWORD,
                     &(Configuration.UseSimpleIconsInArchives), sizeof(DWORD));
            //--- Custom Packers
            HKEY actSubKey;
            if (OpenKey(actKey, SALAMANDER_CUSTOMPACKERS, actSubKey))
            {
                PackerConfig.DeleteAllPackers();
                HKEY itemKey;
                char buf[30];
                int i = 1;
                strcpy(buf, "1");
                while (OpenKey(actSubKey, buf, itemKey))
                {
                    PackerConfig.Load(itemKey);
                    CloseKey(itemKey);
                    itoa(++i, buf, 10);
                }
                GetValue(actSubKey, SALAMANDER_ANOTHERPANEL, REG_DWORD,
                         &(Configuration.UseAnotherPanelForPack), sizeof(DWORD));
                int pp;
                if (GetValue(actSubKey, SALAMANDER_PREFFERED, REG_DWORD, &pp, sizeof(DWORD)))
                {
                    PackerConfig.SetPreferedPacker(pp);
                }
                CloseKey(actSubKey);
                // add new features from the last version :-)
                PackerConfig.AddDefault(Configuration.ConfigVersion);
            }
            //--- Custom Unpackers
            if (OpenKey(actKey, SALAMANDER_CUSTOMUNPACKERS, actSubKey))
            {
                UnpackerConfig.DeleteAllUnpackers();
                HKEY itemKey;
                char buf[30];
                int i = 1;
                strcpy(buf, "1");
                while (OpenKey(actSubKey, buf, itemKey))
                {
                    UnpackerConfig.Load(itemKey);
                    CloseKey(itemKey);
                    itoa(++i, buf, 10);
                }
                GetValue(actSubKey, SALAMANDER_ANOTHERPANEL, REG_DWORD,
                         &(Configuration.UseAnotherPanelForUnpack), sizeof(DWORD));
                GetValue(actSubKey, SALAMANDER_NAMEBYARCHIVE, REG_DWORD,
                         &(Configuration.UseSubdirNameByArchiveForUnpack), sizeof(DWORD));
                int pp;
                if (GetValue(actSubKey, SALAMANDER_PREFFERED, REG_DWORD, &pp, sizeof(DWORD)))
                {
                    UnpackerConfig.SetPreferedUnpacker(pp);
                }
                CloseKey(actSubKey);
                // add new features from the last version :-)
                UnpackerConfig.AddDefault(Configuration.ConfigVersion);
            }
            //--- Predefined Packers
            if (OpenKey(actKey, SALAMANDER_PREDPACKERS, actSubKey))
            {
                // Author
                // External Archivers Locations: default values are not deleted during configuration load
                // as before, just updating. If there is an incomplete or unknown record in the registry,
                // Ignore the 'suse'. Only if the Title is set to one of the default values, it will be used
                // her ways.
                // ArchiverConfig.DeleteAllArchivers();
                HKEY itemKey;
                char buf[30];
                int i = 1;
                strcpy(buf, "1");
                while (OpenKey(actSubKey, buf, itemKey))
                {
                    ArchiverConfig.Load(itemKey);
                    CloseKey(itemKey);
                    itoa(++i, buf, 10);
                }
                CloseKey(actSubKey);
                // add new features from the last version :-)
                // ArchiverConfig.AddDefault(Configuration.ConfigVersion); // j.r. no longer needs to be called
            }
            //--- Archive Association
            if (OpenKey(actKey, SALAMANDER_ARCHIVEASSOC, actSubKey))
            {
                PackerFormatConfig.DeleteAllFormats();
                HKEY itemKey;
                char buf[30];
                int i = 1;
                strcpy(buf, "1");
                while (OpenKey(actSubKey, buf, itemKey))
                {
                    PackerFormatConfig.Load(itemKey);
                    CloseKey(itemKey);
                    itoa(++i, buf, 10);
                }
                CloseKey(actSubKey);
                // add new features from the last version :-)
                PackerFormatConfig.AddDefault(Configuration.ConfigVersion);
                PackerFormatConfig.BuildArray();
            }
            CloseKey(actKey);
        }

        Plugins.CheckData(); // processing of loaded data

        //--- user menu

        IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_USERMENU));

        if (OpenKey(salamander, SALAMANDER_USERMENU_REG, actKey))
        {
            HKEY subKey;
            char buf[30];
            strcpy(buf, "1");
            char name[MAX_PATH];
            char command[MAX_PATH];
            char arguments[USRMNUARGS_MAXLEN];
            char initDir[MAX_PATH];
            int throughShell, closeShell, useWindow;
            int showInToolbar, separator;
            CUserMenuItemType type;
            char icon[MAX_PATH];
            int i = 1;
            UserMenuItems->DestroyMembers();

            CUserMenuIconDataArr* bkgndReaderData = new CUserMenuIconDataArr();

            while (OpenKey(actKey, buf, subKey))
            {
                if (GetValue(subKey, USERMENU_ITEMNAME_REG, REG_SZ, name, MAX_PATH) &&
                    GetValue(subKey, USERMENU_COMMAND_REG, REG_SZ, command, MAX_PATH) &&
                    GetValue(subKey, USERMENU_SHELL_REG, REG_DWORD,
                             &throughShell, sizeof(DWORD)) &&
                    GetValue(subKey, USERMENU_CLOSE_REG, REG_DWORD,
                             &closeShell, sizeof(DWORD)))
                {
                    if (Configuration.ConfigVersion == 1 ||
                        !GetValue(subKey, USERMENU_ARGUMENTS_REG, REG_SZ, arguments, USRMNUARGS_MAXLEN))
                    {
                        // Convert from user-menu version 1.52 to the current version
                        char* s = command;
                        while (*s != 0)
                        {
                            if (*s == '%' && *++s != '%')
                                break;
                            s++;
                        }
                        if (*s == 0)
                            *arguments = 0; // no parameters
                        else
                        {
                            s--;
                            while (--s >= command && *s != ' ')
                                ;
                            if (s < command)
                                *arguments = 0; // syntactic error
                            else
                            {
                                *s++ = 0; // parse the command, set it to the first character of the argument
                                char* st = arguments;
                                char* stEnd = arguments + sizeof(arguments) - 1;
                                while (*s != 0 && st < stEnd)
                                {
                                    if (*s == '%')
                                    {
                                        const char* add = "";
                                        switch (LowerCase[*++s])
                                        {
                                        case '%':
                                            add = "%";
                                            break;
                                        case 'd':
                                            add = "$(Drive)";
                                            break;
                                        case 'p':
                                            add = "$(Path)";
                                            break;
                                        case 'h':
                                            add = "$(DOSPath)";
                                            break;
                                        case 'f':
                                            add = "$(Name)";
                                            break;
                                        case 's':
                                            add = "$(DOSName)";
                                            break;
                                        }
                                        if (st + strlen(add) > stEnd)
                                            break;
                                        else
                                        {
                                            strcpy(st, add);
                                            st += strlen(add);
                                        }
                                    }
                                    else
                                        *st++ = *s;
                                    s++;
                                }
                                *st = 0;
                            }
                        }
                    }
                    if (Configuration.ConfigVersion == 1 ||
                        !GetValue(subKey, USERMENU_INITDIR_REG, REG_SZ, initDir, MAX_PATH))
                    {
                        strcpy(initDir, "$(Drive)$(Path)");
                    }
                    if (Configuration.ConfigVersion == 1 ||
                        !GetValue(subKey, USERMENU_USEWINDOW_REG, REG_DWORD, &useWindow, sizeof(DWORD)))
                    {
                        useWindow = TRUE;
                    }

                    if (Configuration.ConfigVersion == 1 ||
                        !GetValue(subKey, USERMENU_ICON_REG, REG_SZ, icon, MAX_PATH))
                    {
                        icon[0] = 0;
                    }

                    if (Configuration.ConfigVersion == 1 ||
                        !GetValue(subKey, USERMENU_SEPARATOR_REG, REG_DWORD, &separator, sizeof(DWORD)))
                    {
                        separator = FALSE;
                    }

                    if (!GetValue(subKey, USERMENU_TYPE_REG, REG_DWORD, &type, sizeof(DWORD)))
                    {
                        type = separator ? umitSeparator : umitItem;
                    }

                    if (Configuration.ConfigVersion == 1 ||
                        !GetValue(subKey, USERMENU_SHOWINTOOLBAR_REG, REG_DWORD, &showInToolbar, sizeof(DWORD)))
                    {
                        showInToolbar = TRUE;
                    }

                    CUserMenuItem* item = new CUserMenuItem(name, command, arguments, initDir, icon,
                                                            throughShell, closeShell, useWindow,
                                                            showInToolbar, type, bkgndReaderData);
                    if (item != NULL && item->IsGood())
                    {
                        UserMenuItems->Add(item);
                        if (!UserMenuItems->IsGood())
                        {
                            delete item;
                            UserMenuItems->ResetState();
                            break;
                        }
                    }
                    else
                    {
                        if (item != NULL)
                            delete item;
                        TRACE_E(LOW_MEMORY);
                        break;
                    }
                }
                else
                    break;
                itoa(++i, buf, 10);
                CloseKey(subKey);
            }

            UserMenuIconBkgndReader.StartBkgndReadingIcons(bkgndReaderData); // WARNING: release 'bkgndReaderData'

            CloseKey(actKey);
        }

        IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_CONFIG));

        //--- configuration

        DWORD cmdLine = 0, cmdLineFocus = 0;
        DWORD rightPanelFocused = FALSE;
        if (OpenKey(salamander, SALAMANDER_CONFIG_REG, actKey))
        {
            if (importingOldConfig)
            {
                GetValue(actKey, CONFIG_ONLYONEINSTANCE_REG, REG_DWORD,
                         &Configuration.OnlyOneInstance, sizeof(DWORD));
            }
            //--- top rebar begin
            GetValue(actKey, CONFIG_MENUINDEX_REG, REG_DWORD,
                     &Configuration.MenuIndex, sizeof(DWORD));
            GetValue(actKey, CONFIG_MENUBREAK_REG, REG_DWORD,
                     &Configuration.MenuBreak, sizeof(DWORD));
            GetValue(actKey, CONFIG_MENUWIDTH_REG, REG_DWORD,
                     &Configuration.MenuWidth, sizeof(DWORD));
            GetValue(actKey, CONFIG_TOOLBARINDEX_REG, REG_DWORD,
                     &Configuration.TopToolbarIndex, sizeof(DWORD));
            GetValue(actKey, CONFIG_TOOLBARBREAK_REG, REG_DWORD,
                     &Configuration.TopToolbarBreak, sizeof(DWORD));
            GetValue(actKey, CONFIG_TOOLBARWIDTH_REG, REG_DWORD,
                     &Configuration.TopToolbarWidth, sizeof(DWORD));
            GetValue(actKey, CONFIG_PLUGINSBARINDEX_REG, REG_DWORD,
                     &Configuration.PluginsBarIndex, sizeof(DWORD));
            GetValue(actKey, CONFIG_PLUGINSBARBREAK_REG, REG_DWORD,
                     &Configuration.PluginsBarBreak, sizeof(DWORD));
            GetValue(actKey, CONFIG_PLUGINSBARWIDTH_REG, REG_DWORD,
                     &Configuration.PluginsBarWidth, sizeof(DWORD));
            GetValue(actKey, CONFIG_USERMENUINDEX_REG, REG_DWORD,
                     &Configuration.UserMenuToolbarIndex, sizeof(DWORD));
            GetValue(actKey, CONFIG_USERMENUBREAK_REG, REG_DWORD,
                     &Configuration.UserMenuToolbarBreak, sizeof(DWORD));
            GetValue(actKey, CONFIG_USERMENUWIDTH_REG, REG_DWORD,
                     &Configuration.UserMenuToolbarWidth, sizeof(DWORD));
            GetValue(actKey, CONFIG_USERMENULABELS_REG, REG_DWORD,
                     &Configuration.UserMenuToolbarLabels, sizeof(DWORD));
            GetValue(actKey, CONFIG_HOTPATHSINDEX_REG, REG_DWORD,
                     &Configuration.HotPathsBarIndex, sizeof(DWORD));
            GetValue(actKey, CONFIG_HOTPATHSBREAK_REG, REG_DWORD,
                     &Configuration.HotPathsBarBreak, sizeof(DWORD));
            GetValue(actKey, CONFIG_HOTPATHSWIDTH_REG, REG_DWORD,
                     &Configuration.HotPathsBarWidth, sizeof(DWORD));
            GetValue(actKey, CONFIG_DRIVEBARINDEX_REG, REG_DWORD,
                     &Configuration.DriveBarIndex, sizeof(DWORD));
            GetValue(actKey, CONFIG_DRIVEBARBREAK_REG, REG_DWORD,
                     &Configuration.DriveBarBreak, sizeof(DWORD));
            GetValue(actKey, CONFIG_DRIVEBARWIDTH_REG, REG_DWORD,
                     &Configuration.DriveBarWidth, sizeof(DWORD));
            GetValue(actKey, CONFIG_GRIPSVISIBLE_REG, REG_DWORD,
                     &Configuration.GripsVisible, sizeof(DWORD));
            //--- top rebar end
            GetValue(actKey, CONFIG_FILENAMEFORMAT_REG, REG_DWORD,
                     &Configuration.FileNameFormat, sizeof(DWORD));
            GetValue(actKey, CONFIG_SIZEFORMAT_REG, REG_DWORD,
                     &Configuration.SizeFormat, sizeof(DWORD));
            // automatic conversion from "mixed-case" to "partially-mixed-case"
            if (Configuration.FileNameFormat == 1)
                Configuration.FileNameFormat = 7;

            GetValue(actKey, CONFIG_SELECTION_REG, REG_DWORD,
                     &Configuration.IncludeDirs, sizeof(DWORD));
            GetValue(actKey, CONFIG_COPYFINDTEXT_REG, REG_DWORD,
                     &Configuration.CopyFindText, sizeof(DWORD));
            GetValue(actKey, CONFIG_CLEARREADONLY_REG, REG_DWORD,
                     &Configuration.ClearReadOnly, sizeof(DWORD));
            GetValue(actKey, CONFIG_PRIMARYCONTEXTMENU_REG, REG_DWORD,
                     &Configuration.PrimaryContextMenu, sizeof(DWORD));
            GetValue(actKey, CONFIG_NOTHIDDENSYSTEM_REG, REG_DWORD,
                     &Configuration.NotHiddenSystemFiles, sizeof(DWORD));
            GetValue(actKey, CONFIG_RECYCLEBIN_REG, REG_DWORD,
                     &Configuration.UseRecycleBin, sizeof(DWORD));
            // piglet, we provide MasksString, there is a range check, nothing to worry about
            GetValue(actKey, CONFIG_RECYCLEMASKS_REG, REG_SZ,
                     Configuration.RecycleMasks.GetWritableMasksString(), MAX_PATH);
            GetValue(actKey, CONFIG_SAVEONEXIT_REG, REG_DWORD,
                     &Configuration.AutoSave, sizeof(DWORD));
            GetValue(actKey, CONFIG_SHOWGREPERRORS_REG, REG_DWORD,
                     &Configuration.ShowGrepErrors, sizeof(DWORD));
            GetValue(actKey, CONFIG_FINDFULLROW_REG, REG_DWORD,
                     &Configuration.FindFullRowSelect, sizeof(DWORD));
            if (Configuration.ConfigVersion <= 6)
                Configuration.ShowGrepErrors = FALSE; // Force FALSE to avoid unnecessary thrashing (others do it too)
            GetValue(actKey, CONFIG_MINBEEPWHENDONE_REG, REG_DWORD,
                     &Configuration.MinBeepWhenDone, sizeof(DWORD));
            GetValue(actKey, CONFIG_CLOSESHELL_REG, REG_DWORD,
                     &Configuration.CloseShell, sizeof(DWORD));
            GetValue(actKey, CONFIG_RIGHT_FOCUS_REG, REG_DWORD,
                     &rightPanelFocused, sizeof(DWORD));
            GetValue(actKey, CONFIG_ALWAYSONTOP_REG, REG_DWORD,
                     &Configuration.AlwaysOnTop, sizeof(DWORD));
            //      GetValue(actKey, CONFIG_FASTDIRMOVE_REG, REG_DWORD,
            //               &Configuration.FastDirectoryMove, sizeof(DWORD));
            GetValue(actKey, CONFIG_SORTUSESLOCALE_REG, REG_DWORD,
                     &Configuration.SortUsesLocale, sizeof(DWORD));
            GetValue(actKey, CONFIG_SORTDETECTNUMBERS_REG, REG_DWORD,
                     &Configuration.SortDetectNumbers, sizeof(DWORD));
            GetValue(actKey, CONFIG_SORTNEWERONTOP_REG, REG_DWORD,
                     &Configuration.SortNewerOnTop, sizeof(DWORD));
            GetValue(actKey, CONFIG_SORTDIRSBYNAME_REG, REG_DWORD,
                     &Configuration.SortDirsByName, sizeof(DWORD));
            GetValue(actKey, CONFIG_SORTDIRSBYEXT_REG, REG_DWORD,
                     &Configuration.SortDirsByExt, sizeof(DWORD));
            GetValue(actKey, CONFIG_SAVEHISTORY_REG, REG_DWORD,
                     &Configuration.SaveHistory, sizeof(DWORD));
            GetValue(actKey, CONFIG_SAVEWORKDIRS_REG, REG_DWORD,
                     &Configuration.SaveWorkDirs, sizeof(DWORD));
            GetValue(actKey, CONFIG_ENABLECMDLINEHISTORY_REG, REG_DWORD,
                     &Configuration.EnableCmdLineHistory, sizeof(DWORD));
            GetValue(actKey, CONFIG_SAVECMDLINEHISTORY_REG, REG_DWORD,
                     &Configuration.SaveCmdLineHistory, sizeof(DWORD));
            //      GetValue(actKey, CONFIG_LANTASTICCHECK_REG, REG_DWORD,
            //               &Configuration.LantasticCheck, sizeof(DWORD));
            GetValue(actKey, CONFIG_STATUSAREA_REG, REG_DWORD,
                     &Configuration.StatusArea, sizeof(DWORD));
            if (!GetValue(actKey, CONFIG_FULLROWSELECT_REG, REG_DWORD,
                          &Configuration.FullRowSelect, sizeof(DWORD)))
            {
                // we do not want conversion - force TRUE
                //        if (GetValue(actKey, CONFIG_EXPLORERLOOK_REG, REG_DWORD,
                //                     &Configuration.FullRowSelect, sizeof(DWORD)))
                //        {
                DeleteValue(actKey, CONFIG_EXPLORERLOOK_REG);
                //          Configuration.FullRowSelect = !Configuration.FullRowSelect;
                //        }
            }
            GetValue(actKey, CONFIG_FULLROWHIGHLIGHT_REG, REG_DWORD,
                     &Configuration.FullRowHighlight, sizeof(DWORD));
            GetValue(actKey, CONFIG_USEICONTINCTURE_REG, REG_DWORD,
                     &Configuration.UseIconTincture, sizeof(DWORD));
            GetValue(actKey, CONFIG_SHOWPANELCAPTION_REG, REG_DWORD,
                     &Configuration.ShowPanelCaption, sizeof(DWORD));
            GetValue(actKey, CONFIG_SHOWPANELZOOM_REG, REG_DWORD,
                     &Configuration.ShowPanelZoom, sizeof(DWORD));
            GetValue(actKey, CONFIG_SINGLECLICK_REG, REG_DWORD,
                     &Configuration.SingleClick, sizeof(DWORD));
            //      GetValue(actKey, CONFIG_SHOWTIPOFTHEDAY_REG, REG_DWORD,
            //               &Configuration.ShowTipOfTheDay, sizeof(DWORD));
            //      GetValue(actKey, CONFIG_LASTTIPOFTHEDAY_REG, REG_DWORD,
            //               &Configuration.LastTipOfTheDay, sizeof(DWORD));
            GetValue(actKey, CONFIG_INFOLINECONTENT_REG, REG_SZ,
                     Configuration.InfoLineContent, 200);
            GetValue(actKey, CONFIG_IFPATHISINACCESSIBLEGOTO_REG, REG_SZ,
                     Configuration.IfPathIsInaccessibleGoTo, MAX_PATH);
            if (!GetValue(actKey, CONFIG_IFPATHISINACCESSIBLEGOTOISMYDOCS_REG, REG_DWORD,
                          &Configuration.IfPathIsInaccessibleGoToIsMyDocs, sizeof(DWORD)))
            {
                char path[MAX_PATH];
                GetIfPathIsInaccessibleGoTo(path, TRUE);
                if (IsTheSamePath(path, Configuration.IfPathIsInaccessibleGoTo)) // user wants to go to my-documents
                {
                    Configuration.IfPathIsInaccessibleGoToIsMyDocs = TRUE;
                    Configuration.IfPathIsInaccessibleGoTo[0] = 0;
                }
                else
                    Configuration.IfPathIsInaccessibleGoToIsMyDocs = FALSE;
            }
            GetValue(actKey, CONFIG_HOTPATH_AUTOCONFIG, REG_DWORD,
                     &Configuration.HotPathAutoConfig, sizeof(DWORD));
            GetValue(actKey, CONFIG_LASTUSEDSPEEDLIM_REG, REG_DWORD,
                     &Configuration.LastUsedSpeedLimit, sizeof(DWORD));
            GetValue(actKey, CONFIG_QUICKSEARCHENTER_REG, REG_DWORD,
                     &Configuration.QuickSearchEnterAlt, sizeof(DWORD));
            GetValue(actKey, CONFIG_CHD_SHOWMYDOC, REG_DWORD,
                     &Configuration.ChangeDriveShowMyDoc, sizeof(DWORD));
            GetValue(actKey, CONFIG_CHD_SHOWCLOUDSTOR, REG_DWORD,
                     &Configuration.ChangeDriveCloudStorage, sizeof(DWORD));
            GetValue(actKey, CONFIG_CHD_SHOWANOTHER, REG_DWORD,
                     &Configuration.ChangeDriveShowAnother, sizeof(DWORD));
            GetValue(actKey, CONFIG_CHD_SHOWNET, REG_DWORD,
                     &Configuration.ChangeDriveShowNet, sizeof(DWORD));
            GetValue(actKey, CONFIG_SEARCHFILECONTENT, REG_DWORD,
                     &Configuration.SearchFileContent, sizeof(DWORD));
            GetValue(actKey, CONFIG_LASTPLUGINVER, REG_DWORD,
                     &Configuration.LastPluginVer, sizeof(DWORD));
            GetValue(actKey, CONFIG_LASTPLUGINVER_OP, REG_DWORD,
                     &Configuration.LastPluginVerOP, sizeof(DWORD));
            GetValue(actKey, CONFIG_QUICKRENAME_SELALL_REG, REG_DWORD,
                     &Configuration.QuickRenameSelectAll, sizeof(DWORD));
            GetValue(actKey, CONFIG_EDITNEW_SELALL_REG, REG_DWORD,
                     &Configuration.EditNewSelectAll, sizeof(DWORD));
            if (!GetValue(actKey, CONFIG_USESALOPEN_REG, REG_DWORD,
                          &Configuration.UseSalOpen, sizeof(DWORD)))
            {
                Configuration.UseSalOpen = FALSE; // default is not to be used
            }
            else
            {
                if (Configuration.ConfigVersion == 11) // in 1.6 beta 7 it was turned on ... we will turn it off
                {
                    Configuration.UseSalOpen = FALSE; // default is not to be used
                }
            }
            GetValue(actKey, CONFIG_NETWAREFASTDIRMOVE_REG, REG_DWORD,
                     &Configuration.NetwareFastDirMove, sizeof(DWORD));
            if (Windows7AndLater)
                GetValue(actKey, CONFIG_ASYNCCOPYALG_REG, REG_DWORD,
                         &Configuration.UseAsyncCopyAlg, sizeof(DWORD));
            GetValue(actKey, CONFIG_RELOAD_ENV_VARS_REG, REG_DWORD,
                     &Configuration.ReloadEnvVariables, sizeof(DWORD));
            GetValue(actKey, CONFIG_SHIFTFORHOTPATHS_REG, REG_DWORD,
                     &Configuration.ShiftForHotPaths, sizeof(DWORD));
            //      GetValue(actKey, CONFIG_LANGUAGE_REG, REG_SZ,
            //               Configuration.SLGName, MAX_PATH);
            //      GetValue(actKey, CONFIG_USEALTLANGFORPLUGINS_REG, REG_DWORD,
            //               &Configuration.UseAsAltSLGInOtherPlugins, sizeof(DWORD));
            //      GetValue(actKey, CONFIG_ALTLANGFORPLUGINS_REG, REG_SZ,
            //               Configuration.AltPluginSLGName, MAX_PATH);
            GetValue(actKey, CONFIG_CONVERSIONTABLE_REG, REG_SZ,
                     &Configuration.ConversionTable, MAX_PATH);
            GetValue(actKey, CONFIG_SKILLLEVEL_REG, REG_DWORD,
                     &Configuration.SkillLevel, sizeof(DWORD));
            GetValue(actKey, CONFIG_TITLEBARSHOWPATH_REG, REG_DWORD,
                     &Configuration.TitleBarShowPath, sizeof(DWORD));
            GetValue(actKey, CONFIG_TITLEBARMODE_REG, REG_DWORD,
                     &Configuration.TitleBarMode, sizeof(DWORD));
            GetValue(actKey, CONFIG_TITLEBARPREFIX_REG, REG_DWORD,
                     &Configuration.UseTitleBarPrefix, sizeof(DWORD));
            GetValue(actKey, CONFIG_TITLEBARPREFIXTEXT_REG, REG_SZ,
                     &Configuration.TitleBarPrefix, TITLE_PREFIX_MAX);
            GetValue(actKey, CONFIG_MAINWINDOWICONINDEX_REG, REG_DWORD,
                     &Configuration.MainWindowIconIndex, sizeof(DWORD));
            if (Configuration.MainWindowIconIndex < 0 || Configuration.MainWindowIconIndex > MAINWINDOWICONS_COUNT)
                Configuration.MainWindowIconIndex = 0;
            GetValue(actKey, CONFIG_CLICKQUICKRENAME_REG, REG_DWORD,
                     &Configuration.ClickQuickRename, sizeof(DWORD));
            GetValue(actKey, CONFIG_VISIBLEDRIVES_REG, REG_DWORD,
                     &Configuration.VisibleDrives, sizeof(DWORD));
            GetValue(actKey, CONFIG_SEPARATEDDRIVES_REG, REG_DWORD,
                     &Configuration.SeparatedDrives, sizeof(DWORD));
            GetValue(actKey, CONFIG_COMPAREBYTIME_REG, REG_DWORD,
                     &Configuration.CompareByTime, sizeof(DWORD));
            if (!GetValue(actKey, CONFIG_COMPAREBYSIZE_REG, REG_DWORD,
                          &Configuration.CompareBySize, sizeof(DWORD)))
            { // Conversion from the older configuration - BySize was part of ByTime, so we will copy the settings accordingly
                Configuration.CompareBySize = Configuration.CompareByTime;
            }
            GetValue(actKey, CONFIG_COMPAREBYCONTENT_REG, REG_DWORD,
                     &Configuration.CompareByContent, sizeof(DWORD));
            GetValue(actKey, CONFIG_COMPAREBYATTR_REG, REG_DWORD,
                     &Configuration.CompareByAttr, sizeof(DWORD));
            GetValue(actKey, CONFIG_COMPAREBYSUBDIRS_REG, REG_DWORD,
                     &Configuration.CompareSubdirs, sizeof(DWORD));
            GetValue(actKey, CONFIG_COMPAREBYSUBDIRSATTR_REG, REG_DWORD,
                     &Configuration.CompareSubdirsAttr, sizeof(DWORD));

            GetValue(actKey, CONFIG_COMPAREONEPANELDIRS_REG, REG_DWORD,
                     &Configuration.CompareOnePanelDirs, sizeof(DWORD));
            GetValue(actKey, CONFIG_COMPAREMOREOPTIONS_REG, REG_DWORD,
                     &Configuration.CompareMoreOptions, sizeof(DWORD));
            GetValue(actKey, CONFIG_COMPAREIGNOREFILES_REG, REG_DWORD,
                     &Configuration.CompareIgnoreFiles, sizeof(DWORD));
            GetValue(actKey, CONFIG_COMPAREIGNOREDIRS_REG, REG_DWORD,
                     &Configuration.CompareIgnoreDirs, sizeof(DWORD));
            // piglet, we provide MasksString, there is a range check, nothing to worry about
            GetValue(actKey, CONFIG_CONFIGTIGNOREFILESMASKS_REG, REG_SZ,
                     Configuration.CompareIgnoreFilesMasks.GetWritableMasksString(), MAX_PATH);
            GetValue(actKey, CONFIG_CONFIGTIGNOREDIRSMASKS_REG, REG_SZ,
                     Configuration.CompareIgnoreDirsMasks.GetWritableMasksString(), MAX_PATH);
            int errPos;
            Configuration.CompareIgnoreFilesMasks.PrepareMasks(errPos);
            Configuration.CompareIgnoreDirsMasks.PrepareMasks(errPos);

            GetValue(actKey, CONFIG_THUMBNAILSIZE_REG, REG_DWORD,
                     &Configuration.ThumbnailSize, sizeof(DWORD));
            LeftPanel->SetThumbnailSize(Configuration.ThumbnailSize);
            RightPanel->SetThumbnailSize(Configuration.ThumbnailSize);

            GetValue(actKey, CONFIG_KEEPPLUGINSSORTED_REG, REG_DWORD,
                     &Configuration.KeepPluginsSorted, sizeof(DWORD));

            Configuration.ShowSLGIncomplete = TRUE;
            if (Configuration.ConfigVersion == THIS_CONFIG_VERSION)
            {
                GetValue(actKey, CONFIG_SHOWSLGINCOMPLETE_REG, REG_DWORD,
                         &Configuration.ShowSLGIncomplete, sizeof(DWORD));
            }

            GetValue(actKey, CONFIG_EDITNEWFILE_USEDEFAULT_REG, REG_DWORD,
                     &Configuration.UseEditNewFileDefault, sizeof(DWORD));
            GetValue(actKey, CONFIG_EDITNEWFILE_DEFAULT_REG, REG_SZ,
                     Configuration.EditNewFileDefault, MAX_PATH);

#ifndef _WIN64 // FIXME_X64_WINSCP
            if (!GetValue(actKey, "Add x86-Only Plugins", REG_DWORD,
                          &Configuration.AddX86OnlyPlugins, sizeof(DWORD)))
            {
                Configuration.AddX86OnlyPlugins = TRUE;
            }
#endif // _WIN64

            HKEY actSubKey;
            if (OpenKey(actKey, SALAMANDER_CONFIRMATION_REG, actSubKey))
            {
                GetValue(actSubKey, CONFIG_CNFRM_FILEDIRDEL, REG_DWORD,
                         &Configuration.CnfrmFileDirDel, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_NEDIRDEL, REG_DWORD,
                         &Configuration.CnfrmNEDirDel, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_FILEOVER, REG_DWORD,
                         &Configuration.CnfrmFileOver, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_DIROVER, REG_DWORD,
                         &Configuration.CnfrmDirOver, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_SHFILEDEL, REG_DWORD,
                         &Configuration.CnfrmSHFileDel, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_SHDIRDEL, REG_DWORD,
                         &Configuration.CnfrmSHDirDel, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_SHFILEOVER, REG_DWORD,
                         &Configuration.CnfrmSHFileOver, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_NTFSPRESS, REG_DWORD,
                         &Configuration.CnfrmNTFSPress, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_NTFSCRYPT, REG_DWORD,
                         &Configuration.CnfrmNTFSCrypt, sizeof(DWORD));
                if (Configuration.ConfigVersion != 1)
                    GetValue(actSubKey, CONFIG_CNFRM_DAD, REG_DWORD,
                             &Configuration.CnfrmDragDrop, sizeof(DWORD));
                else // if it's an old config, we load it one level up
                    GetValue(actKey, "Confirm Drop Operations", REG_DWORD,
                             &Configuration.CnfrmDragDrop, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_CLOSEARCHIVE, REG_DWORD,
                         &Configuration.CnfrmCloseArchive, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_CLOSEFIND, REG_DWORD,
                         &Configuration.CnfrmCloseFind, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_STOPFIND, REG_DWORD,
                         &Configuration.CnfrmStopFind, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_CREATETARGETPATH, REG_DWORD,
                         &Configuration.CnfrmCreatePath, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_ALWAYSONTOP, REG_DWORD,
                         &Configuration.CnfrmAlwaysOnTop, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_ONSALCLOSE, REG_DWORD,
                         &Configuration.CnfrmOnSalClose, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_SENDEMAIL, REG_DWORD,
                         &Configuration.CnfrmSendEmail, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_ADDTOARCHIVE, REG_DWORD,
                         &Configuration.CnfrmAddToArchive, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_CREATEDIR, REG_DWORD,
                         &Configuration.CnfrmCreateDir, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_CHANGEDIRTC, REG_DWORD,
                         &Configuration.CnfrmChangeDirTC, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_SHOWNAMETOCOMP, REG_DWORD,
                         &Configuration.CnfrmShowNamesToCompare, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_DSTSHIFTSIGNORED, REG_DWORD,
                         &Configuration.CnfrmDSTShiftsIgnored, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_DSTSHIFTSOCCURED, REG_DWORD,
                         &Configuration.CnfrmDSTShiftsOccured, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_CNFRM_COPYMOVEOPTIONSNS, REG_DWORD,
                         &Configuration.CnfrmCopyMoveOptionsNS, sizeof(DWORD));

                CloseKey(actSubKey);
            }

            if (OpenKey(actKey, SALAMANDER_DRVSPEC_REG, actSubKey))
            {
                GetValue(actSubKey, CONFIG_DRVSPEC_FLOPPY_MON, REG_DWORD,
                         &Configuration.DrvSpecFloppyMon, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_FLOPPY_SIMPLE, REG_DWORD,
                         &Configuration.DrvSpecFloppySimple, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_REMOVABLE_MON, REG_DWORD,
                         &Configuration.DrvSpecRemovableMon, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_REMOVABLE_SIMPLE, REG_DWORD,
                         &Configuration.DrvSpecRemovableSimple, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_FIXED_MON, REG_DWORD,
                         &Configuration.DrvSpecFixedMon, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_FIXED_SIMPLE, REG_DWORD,
                         &Configuration.DrvSpecFixedSimple, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_REMOTE_MON, REG_DWORD,
                         &Configuration.DrvSpecRemoteMon, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_REMOTE_SIMPLE, REG_DWORD,
                         &Configuration.DrvSpecRemoteSimple, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_REMOTE_ACT, REG_DWORD,
                         &Configuration.DrvSpecRemoteDoNotRefreshOnAct, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_CDROM_MON, REG_DWORD,
                         &Configuration.DrvSpecCDROMMon, sizeof(DWORD));
                GetValue(actSubKey, CONFIG_DRVSPEC_CDROM_SIMPLE, REG_DWORD,
                         &Configuration.DrvSpecCDROMSimple, sizeof(DWORD));

                // For old versions, we force reading icons on removable, because we introduced the floppy category
                if (Configuration.ConfigVersion < 31)
                    Configuration.DrvSpecRemovableSimple = FALSE;

                CloseKey(actSubKey);
            }

            if (Configuration.ConfigVersion >= 8) // for old versions, we will provide a new toolbar
                GetValue(actKey, CONFIG_TOPTOOLBAR_REG, REG_SZ,
                         Configuration.TopToolBar, 400);
            GetValue(actKey, CONFIG_MIDDLETOOLBAR_REG, REG_SZ,
                     Configuration.MiddleToolBar, 400);
            GetValue(actKey, CONFIG_LEFTTOOLBAR_REG, REG_SZ,
                     Configuration.LeftToolBar, 100);
            GetValue(actKey, CONFIG_RIGHTTOOLBAR_REG, REG_SZ,
                     Configuration.RightToolBar, 100);
            // there was only one change drive button - now I am adding two buttons
            // and merge all bitmaps into one

            if (Configuration.ConfigVersion <= 3 && Configuration.RightToolBar[0] != 0)
            {
                char tmp[5000];
                lstrcpyn(tmp, Configuration.RightToolBar, 5000);
                char num[50];

                Configuration.RightToolBar[0] = 0;

                BOOL first = TRUE;
                char* p = strtok(tmp, ",");
                while (p != NULL)
                {
                    int i = atoi(p);

                    // replace old tbbeChangeDrive with tbbeChangeDriveR
                    //#define TBBE_CHANGE_DRIVE_R     51
                    if (i == 36)
                        i = 51;

                    if (!first)
                        strcat(Configuration.RightToolBar, ",");
                    sprintf(num, "%d", i);
                    strcat(Configuration.RightToolBar, num);
                    first = FALSE;
                    p = strtok(NULL, ",");
                }
            }

            if (TopToolBar != NULL)
                TopToolBar->Load(Configuration.TopToolBar);
            if (MiddleToolBar != NULL)
                MiddleToolBar->Load(Configuration.MiddleToolBar);
            if (LeftPanel->DirectoryLine->ToolBar != NULL)
                LeftPanel->DirectoryLine->ToolBar->Load(Configuration.LeftToolBar);
            if (RightPanel->DirectoryLine->ToolBar != NULL)
                RightPanel->DirectoryLine->ToolBar->Load(Configuration.RightToolBar);

            GetValue(actKey, CONFIG_TOPTOOLBARVISIBLE_REG, REG_DWORD,
                     &Configuration.TopToolBarVisible, sizeof(DWORD));
            GetValue(actKey, CONFIG_PLGTOOLBARVISIBLE_REG, REG_DWORD,
                     &Configuration.PluginsBarVisible, sizeof(DWORD));
            GetValue(actKey, CONFIG_MIDDLETOOLBARVISIBLE_REG, REG_DWORD,
                     &Configuration.MiddleToolBarVisible, sizeof(DWORD));

            GetValue(actKey, CONFIG_USERMENUTOOLBARVISIBLE_REG, REG_DWORD,
                     &Configuration.UserMenuToolBarVisible, sizeof(DWORD));
            GetValue(actKey, CONFIG_HOTPATHSBARVISIBLE_REG, REG_DWORD,
                     &Configuration.HotPathsBarVisible, sizeof(DWORD));

            // if it concerns an old version of the configuration and the user has filled user menu,
            // display UserMenuBar
            if (Configuration.ConfigVersion <= 3 && UserMenuItems->Count > 0)
                Configuration.UserMenuToolBarVisible = TRUE;

            GetValue(actKey, CONFIG_DRIVEBARVISIBLE_REG, REG_DWORD,
                     &Configuration.DriveBarVisible, sizeof(DWORD));
            GetValue(actKey, CONFIG_DRIVEBAR2VISIBLE_REG, REG_DWORD,
                     &Configuration.DriveBar2Visible, sizeof(DWORD));

            if (ret) // if we return FALSE, everything will be inserted later
            {
                // We need to insert the bands in the correct order according to their index
                BOOL menuInserted = FALSE; // menu is important, we must insert it at all costs
                // We prohibit saving positions during gang throws, as it would lead to
                // rearranging their order
                int idx;
                for (idx = 0; idx < 10; idx++) // We can safely try more indexes than there are bands
                {
                    if (idx == Configuration.MenuIndex)
                    {
                        InsertMenuBand();
                        menuInserted = TRUE;
                    }
                    if (idx == Configuration.TopToolbarIndex && Configuration.TopToolBarVisible)
                        ToggleTopToolBar(FALSE);
                    if (idx == Configuration.PluginsBarIndex && Configuration.PluginsBarVisible)
                        TogglePluginsBar(FALSE);
                    if (idx == Configuration.UserMenuToolbarIndex && Configuration.UserMenuToolBarVisible)
                        ToggleUserMenuToolBar(FALSE);
                    if (idx == Configuration.HotPathsBarIndex && Configuration.HotPathsBarVisible)
                        ToggleHotPathsBar(FALSE);
                    if (idx == Configuration.DriveBarIndex && Configuration.DriveBarVisible)
                        ToggleDriveBar(Configuration.DriveBar2Visible, FALSE);
                }
                if (!menuInserted)
                {
                    TRACE_E("Inserting MenuBar. Configuration seems to be corrupted.");
                    Configuration.MenuIndex = 0;
                    InsertMenuBand();
                }
                if (Configuration.MiddleToolBarVisible)
                    ToggleMiddleToolBar();
                CreateAndInsertWorkerBand(); // finally, we insert a worker
            }

            GetValue(actKey, CONFIG_BOTTOMTOOLBARVISIBLE_REG, REG_DWORD,
                     &Configuration.BottomToolBarVisible, sizeof(DWORD));
            if (Configuration.BottomToolBarVisible)
                ToggleBottomToolBar();

            //      GetValue(actKey, CONFIG_SPACESELCALCSPACE, REG_DWORD,
            //               &Configuration.SpaceSelCalcSpace, sizeof(DWORD));
            GetValue(actKey, CONFIG_USETIMERESOLUTION, REG_DWORD,
                     &Configuration.UseTimeResolution, sizeof(DWORD));
            GetValue(actKey, CONFIG_TIMERESOLUTION, REG_DWORD,
                     &Configuration.TimeResolution, sizeof(DWORD));
            GetValue(actKey, CONFIG_IGNOREDSTSHIFTS, REG_DWORD,
                     &Configuration.IgnoreDSTShifts, sizeof(DWORD));
            GetValue(actKey, CONFIG_USEDRAGDROPMINTIME, REG_DWORD,
                     &Configuration.UseDragDropMinTime, sizeof(DWORD));
            GetValue(actKey, CONFIG_DRAGDROPMINTIME, REG_DWORD,
                     &Configuration.DragDropMinTime, sizeof(DWORD));

            GetValue(actKey, CONFIG_LASTFOCUSEDPAGE, REG_DWORD,
                     &Configuration.LastFocusedPage, sizeof(DWORD));
            GetValue(actKey, CONFIG_CONFIGURATION_HEIGHT, REG_DWORD,
                     &Configuration.ConfigurationHeight, sizeof(DWORD));
            GetValue(actKey, CONFIG_VIEWANDEDITEXPAND, REG_DWORD,
                     &Configuration.ViewersAndEditorsExpanded, sizeof(DWORD));
            GetValue(actKey, CONFIG_PACKEPAND, REG_DWORD,
                     &Configuration.PackersAndUnpackersExpanded, sizeof(DWORD));

            GetValue(actKey, CONFIG_CMDLINE_REG, REG_DWORD, &cmdLine, sizeof(DWORD));
            GetValue(actKey, CONFIG_CMDLFOCUS_REG, REG_DWORD, &cmdLineFocus, sizeof(DWORD));

            GetValue(actKey, CONFIG_USECUSTOMPANELFONT_REG, REG_DWORD, &UseCustomPanelFont, sizeof(DWORD));
            if (LoadLogFont(actKey, CONFIG_PANELFONT_REG, &LogFont) && UseCustomPanelFont)
            {
                // if the user is using a custom font, we must now propagate it
                SetFont();
            }

            LoadHistory(actKey, CONFIG_NAMEDHISTORY_REG, FindNamedHistory, FIND_NAMED_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_LOOKINHISTORY_REG, FindLookInHistory, FIND_LOOKIN_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_GREPHISTORY_REG, FindGrepHistory, FIND_GREP_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_SELECTHISTORY_REG, Configuration.SelectHistory, SELECT_HISTORY_SIZE);
            //      The boys (Honza Patera, Tomas Jelinek) didn't like this because they put on a new one
            //      instance and I don't remember what happened last time. They give (Un)Select and they are waiting for them there
            //      Mask from the last. FAR, VC, NC when starting a new instance have *.*. We will also behave like this.
            //      if (Configuration.SelectHistory[0] != NULL)  // at se nacita i pocatecni stav num +/- oznacovani
            //        strcpy(SelectionMask, Configuration.SelectHistory[0]);
            LoadHistory(actKey, CONFIG_COPYHISTORY_REG, Configuration.CopyHistory, COPY_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_CHANGEDIRHISTORY_REG, Configuration.ChangeDirHistory, CHANGEDIR_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_VIEWERHISTORY_REG, ViewerHistory, VIEWER_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_COMMANDHISTORY_REG, Configuration.EditHistory, EDIT_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_FILELISTHISTORY_REG, Configuration.FileListHistory, FILELIST_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_CREATEDIRHISTORY_REG, Configuration.CreateDirHistory, CREATEDIR_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_QUICKRENAMEHISTORY_REG, Configuration.QuickRenameHistory, QUICKRENAME_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_EDITNEWHISTORY_REG, Configuration.EditNewHistory, EDITNEW_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_CONVERTHISTORY_REG, Configuration.ConvertHistory, CONVERT_HISTORY_SIZE);
            LoadHistory(actKey, CONFIG_FILTERHISTORY_REG, Configuration.FilterHistory, FILTER_HISTORY_SIZE);
            if (DirHistory != NULL)
            {
                DirHistory->LoadFromRegistry(actKey, CONFIG_WORKDIRSHISTORY_REG);
                if (LeftPanel != NULL)
                    LeftPanel->DirectoryLine->SetHistory(DirHistory->HasPaths());
                if (RightPanel != NULL)
                    RightPanel->DirectoryLine->SetHistory(DirHistory->HasPaths());
            }

            if (OpenKey(actKey, CONFIG_COPYMOVEOPTIONS_REG, actSubKey))
            {
                CopyMoveOptions.Load(actSubKey);
                CloseKey(actSubKey);
            }

            if (OpenKey(actKey, CONFIG_FINDOPTIONS_REG, actSubKey))
            {
                FindOptions.Load(actSubKey, Configuration.ConfigVersion);
                CloseKey(actSubKey);
            }

            if (OpenKey(actKey, CONFIG_FINDIGNORE_REG, actSubKey))
            {
                FindIgnore.Load(actSubKey, Configuration.ConfigVersion);
                CloseKey(actSubKey);
            }

            GetValue(actKey, CONFIG_FILELISTNAME_REG, REG_SZ, Configuration.FileListName, MAX_PATH);
            GetValue(actKey, CONFIG_FILELISTAPPEND_REG, REG_DWORD, &Configuration.FileListAppend, sizeof(DWORD));
            GetValue(actKey, CONFIG_FILELISTDESTINATION_REG, REG_DWORD, &Configuration.FileListDestination, sizeof(DWORD));

            CloseKey(actKey);
        }

        //--- viewer

        if (OpenKey(salamander, SALAMANDER_VIEWER_REG, actKey))
        {
            GetValue(actKey, VIEWER_FINDFORWARD_REG, REG_DWORD,
                     &GlobalFindDialog.Forward, sizeof(DWORD));
            GetValue(actKey, VIEWER_FINDWHOLEWORDS_REG, REG_DWORD,
                     &GlobalFindDialog.WholeWords, sizeof(DWORD));
            GetValue(actKey, VIEWER_FINDCASESENSITIVE_REG, REG_DWORD,
                     &GlobalFindDialog.CaseSensitive, sizeof(DWORD));
            GetValue(actKey, VIEWER_FINDREGEXP_REG, REG_DWORD,
                     &GlobalFindDialog.Regular, sizeof(DWORD));
            GetValue(actKey, VIEWER_FINDTEXT_REG, REG_SZ,
                     GlobalFindDialog.Text, FIND_TEXT_LEN);
            GetValue(actKey, VIEWER_FINDHEXMODE_REG, REG_DWORD,
                     &GlobalFindDialog.HexMode, sizeof(DWORD));

            GetValue(actKey, VIEWER_CONFIGCRLF_REG, REG_DWORD,
                     &Configuration.EOL_CRLF, sizeof(DWORD));
            GetValue(actKey, VIEWER_CONFIGCR_REG, REG_DWORD,
                     &Configuration.EOL_CR, sizeof(DWORD));
            GetValue(actKey, VIEWER_CONFIGLF_REG, REG_DWORD,
                     &Configuration.EOL_LF, sizeof(DWORD));
            GetValue(actKey, VIEWER_CONFIGNULL_REG, REG_DWORD,
                     &Configuration.EOL_NULL, sizeof(DWORD));
            GetValue(actKey, VIEWER_CONFIGTABSIZE_REG, REG_DWORD,
                     &Configuration.TabSize, sizeof(DWORD));
            GetValue(actKey, VIEWER_CONFIGDEFMODE_REG, REG_DWORD,
                     &Configuration.DefViewMode, sizeof(DWORD));
            // piglet, we provide MasksString, there is a range check, nothing to worry about
            GetValue(actKey, VIEWER_CONFIGTEXTMASK_REG, REG_SZ,
                     Configuration.TextModeMasks.GetWritableMasksString(), MAX_PATH);
            if (Configuration.ConfigVersion < 17 &&
                strcmp(Configuration.TextModeMasks.GetWritableMasksString(), "*.txt;*.602") == 0)
            {
                strcpy(Configuration.TextModeMasks.GetWritableMasksString(), "*.txt;*.602;*.xml");
            }
            int errPos;
            Configuration.TextModeMasks.PrepareMasks(errPos);
            // piglet, we provide MasksString, there is a range check, nothing to worry about
            GetValue(actKey, VIEWER_CONFIGHEXMASK_REG, REG_SZ,
                     Configuration.HexModeMasks.GetWritableMasksString(), MAX_PATH);
            Configuration.HexModeMasks.PrepareMasks(errPos);

            GetValue(actKey, VIEWER_CONFIGUSECUSTOMFONT_REG, REG_DWORD,
                     &UseCustomViewerFont, sizeof(DWORD));
            LoadLogFont(actKey, VIEWER_CONFIGFONT_REG, &ViewerLogFont); // No viewer can be open yet, it is not necessary to call SetViewerFont()
            GetValue(actKey, VIEWER_WRAPTEXT_REG, REG_DWORD,
                     &Configuration.WrapText, sizeof(DWORD));
            GetValue(actKey, VIEWER_CPAUTOSELECT_REG, REG_DWORD,
                     &Configuration.CodePageAutoSelect, sizeof(DWORD));
            GetValue(actKey, VIEWER_DEFAULTCONVERT_REG, REG_SZ, Configuration.DefaultConvert, 200);
            GetValue(actKey, VIEWER_AUTOCOPYSELECTION_REG, REG_DWORD,
                     &Configuration.AutoCopySelection, sizeof(DWORD));
            GetValue(actKey, VIEWER_GOTOOFFSETISHEX_REG, REG_DWORD,
                     &Configuration.GoToOffsetIsHex, sizeof(DWORD));

            GetValue(actKey, VIEWER_CONFIGSAVEWINPOS_REG, REG_DWORD,
                     &Configuration.SavePosition, sizeof(DWORD));
            BOOL plcmntExist = TRUE;
            plcmntExist &= GetValue(actKey, VIEWER_CONFIGWNDLEFT_REG, REG_DWORD,
                                    &Configuration.WindowPlacement.rcNormalPosition.left, sizeof(DWORD));
            plcmntExist &= GetValue(actKey, VIEWER_CONFIGWNDRIGHT_REG, REG_DWORD,
                                    &Configuration.WindowPlacement.rcNormalPosition.right, sizeof(DWORD));
            plcmntExist &= GetValue(actKey, VIEWER_CONFIGWNDTOP_REG, REG_DWORD,
                                    &Configuration.WindowPlacement.rcNormalPosition.top, sizeof(DWORD));
            plcmntExist &= GetValue(actKey, VIEWER_CONFIGWNDBOTTOM_REG, REG_DWORD,
                                    &Configuration.WindowPlacement.rcNormalPosition.bottom, sizeof(DWORD));
            plcmntExist &= GetValue(actKey, VIEWER_CONFIGWNDSHOW_REG, REG_DWORD,
                                    &Configuration.WindowPlacement.showCmd, sizeof(DWORD));
            if (plcmntExist)
                Configuration.WindowPlacement.length = sizeof(Configuration.WindowPlacement);

            CloseKey(actKey);
        }

        //--- left and right panel

        char leftPanelPath[MAX_PATH];
        char rightPanelPath[MAX_PATH];
        GetSystemDirectory(leftPanelPath, MAX_PATH);
        strcpy(rightPanelPath, leftPanelPath);
        char sysDefDir[MAX_PATH];
        lstrcpyn(sysDefDir, DefaultDir[LowerCase[leftPanelPath[0]] - 'a'], MAX_PATH);
        LoadPanelConfig(leftPanelPath, LeftPanel, salamander, SALAMANDER_LEFTP_REG);
        LoadPanelConfig(rightPanelPath, RightPanel, salamander, SALAMANDER_RIGHTP_REG);

        CloseKey(salamander);
        salamander = NULL;

        LoadSaveToRegistryMutex.Leave();

        //--- END OF LOADING CONFIGURATION

        if (cmdLine && !SystemPolicies.GetNoRun())
            PostMessage(HWindow, WM_COMMAND, CM_TOGGLEEDITLINE, TRUE);

        MSG msg; // process the sent messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // we will set the active panel according to the parameter from the command line
        if (ret && cmdLineParams != NULL)
        {
            if (cmdLineParams->ActivatePanel == 1 && rightPanelFocused ||
                cmdLineParams->ActivatePanel == 2 && !rightPanelFocused)
            {
                rightPanelFocused = !rightPanelFocused;
            }
        }

        FocusPanel(rightPanelFocused ? RightPanel : LeftPanel);
        (rightPanelFocused ? RightPanel : LeftPanel)->SetCaretIndex(0, FALSE);
        if (cmdLineFocus)
            SendMessage(HWindow, WM_COMMAND, CM_EDITLINE, 0);

        // It didn't do any good here
        // if the panel was directed to a UNC path that was not accessible,
        // It remained here waiting for several seconds
        //    LeftPanel->UpdateDriveIcon(TRUE);
        //    RightPanel->UpdateDriveIcon(TRUE);
        //    RefreshMenuAndTB(TRUE);

        HMENU h = GetSystemMenu(HWindow, FALSE);
        if (h != NULL)
        {
            CheckMenuItem(h, CM_ALWAYSONTOP, MF_BYCOMMAND | (Configuration.AlwaysOnTop ? MF_CHECKED : MF_UNCHECKED));

            char buff[200];
            MENUITEMINFO mii;
            mii.cbSize = sizeof(MENUITEMINFO);
            mii.fMask = MIIM_TYPE;
            mii.dwTypeData = buff;
            mii.cch = 199;

            GetMenuItemInfo(h, SC_MINIMIZE, FALSE, &mii);
            wsprintf(buff + strlen(buff), "\t%s+%s", LoadStr(IDS_SHIFT), LoadStr(IDS_ESCAPE));
            SetMenuItemInfo(h, SC_MINIMIZE, FALSE, &mii);

            mii.cch = 199;
            GetMenuItemInfo(h, SC_MAXIMIZE, FALSE, &mii);
            wsprintf(buff + strlen(buff), "\t%s+%s+%s", LoadStr(IDS_CTRL), LoadStr(IDS_SHIFT), LoadStr(IDS_F11));
            SetMenuItemInfo(h, SC_MAXIMIZE, FALSE, &mii);
        }

        SplashScreenCloseIfExist();
        if (Configuration.StatusArea)
            AddTrayIcon();

        SetWindowIcon();
        SetWindowTitle();

        SetWindowPos(HWindow,
                     Configuration.AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // show the window in full glory
        if (useWinPlacement)
        {
            // from MSDN:
            // ShowCmd: 0 = SW_SHOWNORMAL
            //          3 = SW_SHOWMAXIMIZED
            //          7 = SW_SHOWMINNOACTIVE

            // we do not want the application to start minimized - only if the user defined it
            // at the shortcut level
            if (!Configuration.StatusArea)
            {
                switch (CmdShow)
                {
                case SW_SHOWNORMAL:
                {
                    // if the window is minimized in the configuration, open it restored
                    if (place.showCmd == SW_MINIMIZE)
                        place.showCmd = SW_RESTORE;
                    if (place.showCmd == SW_SHOWMINIMIZED)
                        place.showCmd = SW_SHOWNORMAL;
                    break;
                }

                // Setting in shortcut has priority over configuration
                case SW_SHOWMINNOACTIVE:
                case SW_SHOWMAXIMIZED:
                {
                    place.showCmd = CmdShow;
                    break;
                }
                }
            }
            else
            {
                switch (CmdShow)
                {
                case SW_SHOWNORMAL:
                {
                    // if the window is minimized in the configuration, open it restored
                    if (place.showCmd == SW_MINIMIZE)
                        place.showCmd = SW_RESTORE;
                    if (place.showCmd == SW_SHOWMINIMIZED)
                        place.showCmd = SW_SHOWNORMAL;
                    break;
                }

                // Setting in shortcut has priority over configuration
                case SW_SHOWMINNOACTIVE:
                {
                    place.showCmd = SW_HIDE;
                    PostMessage(HWindow, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                    break;
                }

                // Setting in shortcut has priority over configuration
                case SW_SHOWMAXIMIZED:
                {
                    place.showCmd = CmdShow;
                    PostMessage(HWindow, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                    break;
                }
                }
            }
            SetWindowPlacement(HWindow, &place);
        }
        LeftPanel->SetupListBoxScrollBars();
        RightPanel->SetupListBoxScrollBars();

        UpdateWindow(HWindow);

        // we will set paths in panels according to parameters from the command line (all types of paths, including archives and file systems)
        BOOL leftPanelPathSet = FALSE;
        BOOL rightPanelPathSet = FALSE;
        if (ret && cmdLineParams != NULL)
        {
            if (cmdLineParams->LeftPath[0] == 0 && cmdLineParams->RightPath[0] == 0 && cmdLineParams->ActivePath[0] != 0)
            {
                if (GetActivePanel()->ChangeDirLite(cmdLineParams->ActivePath)) // It doesn't make sense to combine with setting the left/right panel
                {
                    if (rightPanelFocused)
                        rightPanelPathSet = TRUE;
                    else
                    {
                        leftPanelPathSet = TRUE;
                        LeftPanel->RefreshVisibleItemsArray(); // comment below see "RefreshVisibleItemsArray"
                    }
                }
            }
            else
            {
                if (cmdLineParams->LeftPath[0] != 0)
                {
                    if (LeftPanel->ChangeDirLite(cmdLineParams->LeftPath))
                    {
                        leftPanelPathSet = TRUE;
                        LeftPanel->RefreshVisibleItemsArray(); // comment below see "RefreshVisibleItemsArray"
                    }
                }
                if (cmdLineParams->RightPath[0] != 0)
                {
                    if (RightPanel->ChangeDirLite(cmdLineParams->RightPath))
                        rightPanelPathSet = TRUE;
                }
            }
        }

        // we will save an array of visible items, normally done in idle, but if it has
        // be prepared for prioritizing reading icons in the user menu before icons outside the visible area
        // part of the panel, we have to take care of it "manually" (loading icons has been running for a long time,
        // but better now than later, this minimal delay hopefully won't make much of a difference)
        if (rightPanelPathSet)
            RightPanel->RefreshVisibleItemsArray();

        // leftPanelPath and rightPanelPath are just disk paths, we do not store archives or file systems
        DWORD err, lastErr;
        BOOL pathInvalid, cut;
        BOOL tryNet = TRUE;
        if (!leftPanelPathSet)
        {
            if (SalCheckAndRestorePathWithCut(LeftPanel->HWindow, leftPanelPath, tryNet,
                                              err, lastErr, pathInvalid, cut, TRUE))
            {
                LeftPanel->ChangePathToDisk(LeftPanel->HWindow, leftPanelPath);
            }
            else
                LeftPanel->ChangeToRescuePathOrFixedDrive(LeftPanel->HWindow);
            LeftPanel->RefreshVisibleItemsArray(); // comment above see "RefreshVisibleItemsArray"
        }
        UpdateWindow(LeftPanel->HWindow); // Ensure rendering the dir/info line immediately after rendering the content of the panel

        tryNet = TRUE;
        if (!rightPanelPathSet)
        {
            if (SalCheckAndRestorePathWithCut(RightPanel->HWindow, rightPanelPath, tryNet,
                                              err, lastErr, pathInvalid, cut, TRUE))
            {
                RightPanel->ChangePathToDisk(RightPanel->HWindow, rightPanelPath);
            }
            else
                RightPanel->ChangeToRescuePathOrFixedDrive(RightPanel->HWindow);
            RightPanel->RefreshVisibleItemsArray(); // comment above see "RefreshVisibleItemsArray"
        }
        UpdateWindow(RightPanel->HWindow); // Ensure rendering the dir/info line immediately after rendering the content of the panel

        // restore default-dir on system disk (corrupted - sys. root was in both panels)
        lstrcpyn(DefaultDir[LowerCase[sysDefDir[0]] - 'a'], sysDefDir, MAX_PATH);
        // Restore DefaultDir
        MainWindow->UpdateDefaultDir(TRUE);

        return ret;
    }

    LoadSaveToRegistryMutex.Leave();

    return FALSE;
}
