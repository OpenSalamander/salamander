// Defines for Altap Translator
#define IDOK                1
#define IDCANCEL            2
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define IDYES               6
#define IDNO                7
#define IDCLOSE             8
#define IDHELP              9

///* X:\ALTAP\PETR\salamand\plugins\checkver\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//
#define IDD_MAIN                        100
#define IDC_MAIN_LOG                    101
#define IDC_MAIN_CFG                    102
#define IDC_MAIN_CHECK                  103
#define IDD_CONFIGURATION               110
#define IDC_CFG_AUTOCLOSE               111
#define IDC_CFG_BETA                    112
#define IDC_CFG_AUTOCONNECT             113
#define IDC_CFG_NEVER                   114
#define IDC_CFG_DAY                     115
#define IDC_CFG_WEEK                    116
#define IDC_CFG_MONTH                   117
#define IDC_CFG_3MONTHS                 118
#define IDC_CFG_6MONTHS                 119
#define IDC_CFG_FILTER                  120
#define IDC_CFG_REMOVE                  121
#define IDC_CFG_DEFAULTS                122
#define IDC_CFG_CHANGE                  123
#define IDC_CFG_INTERNET                124
#define IDC_CFG_PB                      125
#define IDC_CFG_RELEASE                 126
#define IDD_INTERNET                    130
#define IDC_INET_LAN                    131
#define IDC_INET_NONE                   132
#define IDC_INET_PHONE                  133
#define IDC_INET_FTP                    134
#define IDC_INET_FTP_PASSIVE            135
#define IDC_INET_HTTP                   136
#define IDC_INTERNET_ICON               137

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        8500
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         8700
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
///* X:\ALTAP\PETR\salamand\plugins\checkver\checkver.rh2
#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

#define IDC_STATIC_1 400
#include "statics.rh2"   // 400 az 439 jsou timto zabrane !!!

// icons, cursors, bitmaps
#define IDI_CHECKVER            80
#define IDI_INTERNET            81
#define IDB_CHECKVER            84

// commands
#define CM_CHECK_VERSION        90
#define CM_AUTOCHECK_VERSION    91
#define CM_ENUMMODULES          92
#define CM_FIRSTCHECK_VERSION   93
#define CM_OPENFILE             94  // pouze v debug verzi - v systemovem menu
#define CM_ABOUT                95  // vyvola about dialog
#define CM_CHECK_FIRSTLOAD      96  // pousti se pokud jde o prvni check z prave nainstalovaneho Salamandera (okno checkveru je zobrazene kvuli firewallu, aby user pochopil proc jdeme na internet)

// texts
#define IDS_PLUGIN_DESCRIPTION  190
#define IDS_ABOUT               191

#define IDS_CTXMENU_OPEN        210
#define IDS_CTXMENU_COPY        211
#define IDS_CTXMENU_FILTER      212
#define IDS_PLUGINNAME          213
#define IDS_FILTER_CONFIRM      214
#define IDS_REMOVE_CNFRM        215
#define IDS_SETDEF_CNFRM        216
#define IDS_OPENED_WINDOWS      217
#define IDS_ABORT_DOWNLOAD      218

#define IDS_INET_INIT           220
#define IDS_INET_INIT_FAILED    221
#define IDS_INET_CONNECT        222
#define IDS_INET_CONNECT_FAILED 223
#define IDS_INET_READ           224
#define IDS_INET_READ_FAILED    225
#define IDS_INET_SUCCESS        226
#define IDS_INET_ABORTED        227
#define IDS_INET_PROTOCOL       228

#define IDS_CFG_CONFLICT1       230
#define IDS_CFG_CONFLICT2       231
#define IDS_CFG_CONFLICT3       232

#define IDS_CLOSE_CONFLICT      233
#define IDS_PLUGIN_BUSY         234
#define IDS_INFO_CORRUPTED      235

#define IDS_FILTERED_MODULES    240
#define IDS_INSTALLED_MODULES   241
#define IDS_NEWREL_MODULES      242
#define IDS_NEWBETA_MODULES     243
#define IDS_NONEW_MODULES       244

#define IDS_BTN_CHECK           247
#define IDS_BTN_STOP            248

#define IDS_COPYRIGHT1          250
#define IDS_COPYRIGHT2          251

#define IDS_MENU_ABOUT          253
#define IDS_LAST_CHECK          254

#define IDS_INTERNET_PHONE      255
#define IDS_INTERNET_LAN        256
#define IDS_INTERNET_NONE       257

#define IDS_SKIP_CHECK          258
#define IDS_CHECK_FOR_NEW_VER   260
#define IDS_UPDATE_SCRIPT_LANG  266
#define IDS_DONOTUSEFTP         267
#define IDS_LOGWNDHELP1         268
#define IDS_LOGWNDHELP2         269
#define IDS_SHOWDETAILS         270
#define IDS_HIDEDETAILS         271
#define IDS_NEWPB_MODULES       272
#define IDS_CTXMENU_DOWNLOAD    273
#define IDS_FILTERVER_CONFIRM   274
#define IDS_FILTERTHISVER       275

#define IDS_MENU_OPENFILE       276
#define IDS_FILE_OPENERROR      277
#define IDS_FILE_READERROR      278
#define IDS_FILE_OPENED         279

// help IDs

#define IDH_CHECKVER            500
///* X:\ALTAP\PETR\salamand\plugins\shared\statics.rh2
//#define IDC_STATIC_1        (IDC_STATIC_1+0) // definovano v nadrazenem headru
#define IDC_STATIC_2        (IDC_STATIC_1+1)
#define IDC_STATIC_3        (IDC_STATIC_1+2)
#define IDC_STATIC_4        (IDC_STATIC_1+3)
#define IDC_STATIC_5        (IDC_STATIC_1+4)
#define IDC_STATIC_6        (IDC_STATIC_1+5)
#define IDC_STATIC_7        (IDC_STATIC_1+6)
#define IDC_STATIC_8        (IDC_STATIC_1+7)
#define IDC_STATIC_9        (IDC_STATIC_1+8)
#define IDC_STATIC_10       (IDC_STATIC_1+9)
#define IDC_STATIC_11       (IDC_STATIC_1+10)
#define IDC_STATIC_12       (IDC_STATIC_1+11)
#define IDC_STATIC_13       (IDC_STATIC_1+12)
#define IDC_STATIC_14       (IDC_STATIC_1+13)
#define IDC_STATIC_15       (IDC_STATIC_1+14)
#define IDC_STATIC_16       (IDC_STATIC_1+15)
#define IDC_STATIC_17       (IDC_STATIC_1+16)
#define IDC_STATIC_18       (IDC_STATIC_1+17)
#define IDC_STATIC_19       (IDC_STATIC_1+18)
#define IDC_STATIC_20       (IDC_STATIC_1+19)
#define IDC_STATIC_21       (IDC_STATIC_1+20)
#define IDC_STATIC_22       (IDC_STATIC_1+21)
#define IDC_STATIC_23       (IDC_STATIC_1+22)
#define IDC_STATIC_24       (IDC_STATIC_1+23)
#define IDC_STATIC_25       (IDC_STATIC_1+24)
#define IDC_STATIC_26       (IDC_STATIC_1+25)
#define IDC_STATIC_27       (IDC_STATIC_1+26)
#define IDC_STATIC_28       (IDC_STATIC_1+27)
#define IDC_STATIC_29       (IDC_STATIC_1+28)
#define IDC_STATIC_30       (IDC_STATIC_1+29)
#define IDC_STATIC_31       (IDC_STATIC_1+30)
#define IDC_STATIC_32       (IDC_STATIC_1+31)
#define IDC_STATIC_33       (IDC_STATIC_1+32)
#define IDC_STATIC_34       (IDC_STATIC_1+33)
#define IDC_STATIC_35       (IDC_STATIC_1+34)
#define IDC_STATIC_36       (IDC_STATIC_1+35)
#define IDC_STATIC_37       (IDC_STATIC_1+36)
#define IDC_STATIC_38       (IDC_STATIC_1+37)
#define IDC_STATIC_39       (IDC_STATIC_1+38)
#define IDC_STATIC_40       (IDC_STATIC_1+39)
