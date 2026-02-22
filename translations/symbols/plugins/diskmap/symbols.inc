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

///* X:\ALTAP\PETR\salamand\plugins\diskmap\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//

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
///* X:\ALTAP\PETR\salamand\plugins\diskmap\zarevak.altap.diskmap\resources\zarevak.altap.diskmap.rh2
// Used by Zarevak.Altap.DiskMapPlugin.rc2 and lang.rc2
//

#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)


// statics
#define IDC_STATIC_1 3000
#include "statics.rh2"   // 3000 az 3039 jsou timto zabrane !!!


//Dialog:
#define IDD_ABOUTBOX                    103
//Dialog Control:
#define IDC_MYICON                      2  //ID ikony na About Dialogu


//Icon:
#define IDI_ZAREVAKALTAPDISKMAP         107


#define IDC_ZAREVAKALTAPDISKMAP         109 //ID hlavniho menu


//Menu:
#define IDM_FILE_OPEN                   111
#define IDM_FILE_FOCUSINSALAMANDER      112
#define IDM_FILE_ABORT                  113
#define IDM_FILE_REFRESH                114
#define IDM_FILE_EXIT                   115

#define IDM_VIEW_ZOOMIN                 121
#define IDM_VIEW_ZOOMOUT                122
#define IDM_VIEW_GOTOROOT               123
#define IDM_VIEW_LOG                    124

#define IDM_OPTIONS_CONSFIRMCLOSE       141
#define IDM_OPTIONS_SHOWFOLDERS         142
#define IDM_OPTIONS_SHOWTOOLTIP         143

#define IDM_HELP_ABOUT                  131

#define IDM_DEBUG_EXPORTDIR             191

#define IDM_DESIGN_FIRST                200
#define IDM_DESIGN_BASIC                200
#define IDM_DESIGN_PLASTIC              201
#define IDM_DESIGN_GLASS                202
#define IDM_DESIGN_SLEEK                203
#define IDM_DESIGN_TEST                 204
#define IDM_DESIGN_LAST                 204

#define IDM_PATHFORMAT_FIRST            300
#define IDM_PATHFORMAT_ABSOLUTE         300
#define IDM_PATHFORMAT_RELATIVEROOT     301
#define IDM_PATHFORMAT_RELATIVEVIEW     302
#define IDM_PATHFORMAT_FILENAME         303
#define IDM_PATHFORMAT_LAST             303

//Retezce pluginu - nacitane pomoci LoadStr() v Zarevak.Altap.DiskMapPlugin.cpp
#define IDS_PLUGIN_NAME                 32
#define IDS_PLUGIN_DESCRIPTION          33
#define IDS_PLUGIN_ABOUT                34
#define IDS_PLUGIN_MENU                 35
#define IDS_CLOSE_CONFIRMATION          40
#define IDS_CLOSE_CONFIRMATION_CHECKBOX 41

//Retezce DiskMapy - nacitane pomoci CZLocalizer a CZResourceString
#define IDS_DISKMAP_FIRST               48

//Main Window
#define IDS_DISKMAP_TITLE               48
#define IDS_DISKMAP_TITLEPREFIX         49
//Load animation
#define IDS_DISKMAP_LOAD_TITLE          50
#define IDS_DISKMAP_LOAD_FOOTER         51
#define IDS_DISKMAP_LOAD_HSIZE          52
#define IDS_DISKMAP_LOAD_HFILE          53
#define IDS_DISKMAP_LOAD_HDIRS          54
//Abort info
#define IDS_DISKMAP_ABORT_TITLE         55
#define IDS_DISKMAP_ABORT_FOOTER        56
//Log Window
#define IDS_DISKMAP_LOG_TITLE           57
#define IDS_DISKMAP_LOG_HTYPE           58
#define IDS_DISKMAP_LOG_HTEXT           59
#define IDS_DISKMAP_LOG_HFILE           60
#define IDS_DISKMAP_LOG_LINFO           61
#define IDS_DISKMAP_LOG_LWARN           62
#define IDS_DISKMAP_LOG_LERROR          63

//Shell menu
#define IDS_DISKMAP_SHELL_FOCUS         64
#define IDS_DISKMAP_SHELL_ZOOMIN        65
#define IDS_DISKMAP_SHELL_ZOOMOUT       66
#define IDS_DISKMAP_SHELL_DIROPEN       67
#define IDS_DISKMAP_SHELL_DIRCHANGE     68

//Tooltip
#define IDS_DISKMAP_TTIP_TIMECREATE     69
#define IDS_DISKMAP_TTIP_TIMEMODIFY     70
#define IDS_DISKMAP_TTIP_TIMEACCESS     71
#define IDS_DISKMAP_TTIP_SIZEDATA       72
#define IDS_DISKMAP_TTIP_SIZEREAL       73
#define IDS_DISKMAP_TTIP_SIZEDISK       74
#define IDS_DISKMAP_TTIP_COUNTFILE      75
#define IDS_DISKMAP_TTIP_COUNTDIRS      76

//General
#define IDS_DISKMAP_FORMAT_BYTE0        77 //pro "0 bajtu"
#define IDS_DISKMAP_FORMAT_BYTE1        78 //pro "1 bajt"
#define IDS_DISKMAP_FORMAT_BYTEN        79 //pro "n bajtu

#define IDS_DISKMAP_LAST                79


#define IDR_CUSHIONDATA_LEGACY          1
#define IDR_CUSHIONDATA_GLASS           2
#define IDR_CUSHIONDATA_SLEEK           3
#define IDR_CUSHIONDATA_TEST            4

// help IDs
#define IDH_SHOWDISKMAP                 500
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
