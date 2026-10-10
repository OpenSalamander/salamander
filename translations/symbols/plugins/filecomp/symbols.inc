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

///* X:\ALTAP\PETR\salamand\plugins\filecomp\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//
#define IDD_COMPAREFILES                100
#define IDE_PATH1                       101
#define IDE_PATH2                       102
#define IDADVANCED                      103
#define IDB_BROWSE1                     104
#define IDB_BROWSE2                     105
#define IDD_CFGCOLORS                   105
#define IDC_ITEMS                       106
#define IDS_DESCRIPTION1                107
#define IDS_DESCRIPTION2                108
#define IDS_DESCRIPTION3                109
#define IDS_DESCRIPTION4                110
#define IDB_LINENUM1                    111
#define IDB_LINENUM2                    112
#define IDB_LINENUM3                    113
#define IDB_LINENUM4                    114
#define IDB_TEXT1                       115
#define IDB_TEXT2                       116
#define IDB_TEXT3                       117
#define IDB_TEXT4                       118
#define IDB_DEFAULT                     120
#define IDS_PREVIEW2                    121
#define IDS_PREVIEW1                    122
#define IDD_ADVANCEDOPTIONS             130
#define IDC_IGNOREALLSPACE              131
#define IDC_IGNORESPACECHANGE           132
#define IDC_IGNOREBLANKS                133
#define IDC_IGNORECASE                  134
#define IDR_ALWAYSTEXT                  135
#define IDC_USEINFUTURE                 136
#define IDE_FONT                        136
#define IDR_ALWAYSBINARY                137
#define IDB_FONT                        137
#define IDR_AUTO                        138
#define IDC_WHITESPACE                  138
#define IDC_CRLF1                       139
#define IDC_DETAILDIFFS                 139
#define IDC_CRLF2                       140
#define IDD_CFGGENERAL                  140
#define IDC_VIEW_HORIZONTAL             140
#define IDC_CR1                         141
#define IDE_CONTEXT                     141
#define IDC_CR2                         142
#define IDC_CONFIRM                     142
#define IDC_LF1                         143
#define IDE_TABSIZE                     143
#define IDC_LF2                         144
#define IDC_LOADONSTART                 144
#define IDC_NULL1                       145
#define IDC_NULL2                       146
#define IDC_NORMALIZATION_FORM          147
#define IDD_CFGDEFOPTIONS               150
#define IDE_LEFTENC                     151
#define IDE_RIGHTENC                    152
#define IDB_LEFTENC                     153
#define IDB_RIGHTENC                    154
#define IDM_MENU                        600
#define IDM_CTX_MENU                    601
#define IDM_CTX_1COLOR_MENU             602
#define IDM_CTX_2COLOR_MENU             603
#define IDM_CTX_FONT_MENU               604

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
///* X:\ALTAP\PETR\salamand\plugins\filecomp\filecomp.rh2
#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

#define IDC_STATIC_1 300
#include "statics.rh2"   // 300 az 339 jsou timto zabrane !!!

#define IDI_FCICO       500
#define IDC_SPLIT_VERT  501
#define IDC_SPLIT_HOR   502
#define IDB_TOOLBAR     502
#define IDI_BROWSE      503
#define IDB_FILECOMP    504

//***********************************************************************************
//
// texty
//

#define IDS_PLUGINNAME  1000
#define IDS_LOWMEM    1001
#define IDS_COMPAREFILES  1002
//#define IDS_SOMEWINDOWSOPEN 1003
#define IDS_READFILE  1004
#define IDS_ACCESFILE 1005
#define IDS_INTERNALERROR 1006
#define IDS_LARGEFILE 1007
#define IDS_OPEN      1008
#define IDS_NODIFFERRENCE 1009
#define IDS_BINARYDIFFER  1010
#define IDS_MAXLINES  1011
#define IDS_MAINWNDHEADER  1035
#define IDS_MAINWNDHEADER_NODIF 1036
#define IDS_DIFFERENCES    1012
#define IDS_DIFFERENCES2   1013
#define IDS_MISSINGPATH    1014
#define IDS_ERROR     1015
#define IDS_SELECTALL 1016
#define IDS_RECOMPARE 1017
#define IDS_COPY      1018
#define IDS_COPYCLIP  1019
#define IDS_FIRSTDIFF 1020
#define IDS_PREVDIFF  1021
#define IDS_NEXTDIFF  1022
#define IDS_LASTDIFF  1023
#define IDS_CANCELED 1025
#define IDS_CLOSEDIFF 1026
#define IDS_COMMONTEXT 1027
#define IDS_SELECTEDTEXT 1028
#define IDS_CHANGEDTEXT 1029
#define IDS_FOCUSEDCHANGE 1030
#define IDS_FOCUSEDFRAME 1031
#define IDS_COMMONCOLORS 1032
#define IDS_LEFTCOLORS 1033
#define IDS_RIGHTCOLORS 1034
#define IDS_POSITIVEVALUE 1041
#define IDS_SELECTFIRST 1042
#define IDS_SELECTSECOND 1043
#define IDS_FILEDOESNOTEXIST 1044
#define IDS_ALLFILES 1045
#define IDS_CONFIGURATION 1046
#define IDS_FONTDESCRIPTION 1047
#define IDS_ACCESFILE2  1050
#define IDS_BINREPORT1  1051
#define IDS_BINREPORT2  1052
#define IDS_ADD1        1053
#define IDS_ADD2        1054
#define IDS_DELETE1     1055
#define IDS_DELETE2     1056
#define IDS_CHANGEFROM1 1057
#define IDS_CHANGEFROM2 1058
#define IDS_CHANGETO1   1059
#define IDS_CHANGETO2   1060
#define IDS_MAINWNDHEADERCOMPUTING 1061
#define IDS_MAINWNDHEADERCOMPUTING2 1062
#define IDS_MAINWNDHEADERCOMPUTING_PROGRESS 1063
#define IDS_MAINWNDHEADERCOMPUTING_PROGRESS_FOUND 1064
#define IDS_MAINWNDHEADERTOOMANY    1065
#define IDS_OK          1066
#define IDS_CANCEL      1067
#define IDS_OPTIONSCHANGED  1068
#define IDS_FOCUSEDCHANGE2  1069
#define IDS_COMMONFOCUSED  1070
#define IDS_COMMONFOCUSED2  1071
#define IDS_DETAILCANCEL 1072
#define IDS_INVALIDNUMBER 1073
#define IDS_NOTVALIDFILE 1074
#define IDS_SPLERROR	1075
#define IDS_INVALIDARGS	1076
#define IDS_MSGERR	1077
#define IDS_LAUNCHSAL	1078
#define IDS_MSGERR2	1079
#define IDS_PLUGIN_DESCRIPTION 1080
#define IDS_CONFIGURATIONTITLE 1081
#define IDS_ABOUT 1082
#define IDS_ERRORUNICODE 1083
#define IDS_ENCMENU_AUTO 1084
#define IDS_ENCMENU_ASCII8 1085
#define IDS_ENCMENU_ASCII8NOINPUTENC 1086
#define IDS_ENCMENU_ASCII8AUTOINPUTENC 1087
#define IDS_ENCMENU_UNICODE 1088
#define IDS_ENCMENU_UTF8 1089
#define IDS_ENCMENU_UTF16LE 1090
#define IDS_ENCMENU_UTF16BE 1091
#define IDS_ENCMENU_UTF32LE 1092
#define IDS_ENCMENU_UTF32BE 1093
#define IDS_AUTOINPUTENC 1094
#define IDS_NOINPUTENC 1095
#define IDS_MAXLEFTVIEW   1096
#define IDS_MAXTOPVIEW    1097
#define IDS_MAXRIGHTVIEW  1098
#define IDS_MAXBOTTOMVIEW 1099
#define IDS_ERROR_COPY_LARGE_SELECTION 1100
#define IDS_ERROR_COPY_OOM  1101
#define IDS_ERROR_COPY_FAILED 1102
#define IDS_LOWMEM_TRY_BINARY 1103
#define IDS_ALLDIFFSIGNORED 1104

//***********************************************************************************
//
// virtualni IDcka - pouze pro vazbu s helpem
//

#define IDH_KEYBOARD        400
#define IDH_COMPAREFILES    401
#define IDH_INTRODUCTION    402

//***********************************************************************************
//
// commands
//

#define CM_EXIT 601
#define CM_MAXIMIZE 602
#define CM_VIEW_HORIZONTAL 603
#define CM_COPY 604
#define CM_FIRSTDIFF 605
#define CM_PREVDIFF 606
#define CM_NEXTDIFF 607
#define CM_LASTDIFF 608
#define CM_SELECTALL 609
#define CM_COMPARE 610
#define CM_RECOMPARE 611
#define CM_CANCEL 612
//#define CM_COLORS 613
#define CM_CUSTOMTEXT 614
#define CM_AUTOTEXT 615
#define CM_CUSTOMBACKGROUND 616
#define CM_AUTOBACKGROUND 617
#define CM_ONLYDIFFERENCES 618
#define CM_OPTIONS 619
#define CM_AUTOCOPY 638
#define CM_SHOWWHITESPACE 620
#define CM_SHOWCARET 621
#define CM_CBACTIVATE 622
#define CM_SWITCHWINDOW 623
#define CM_CUSTOMFONT 624
#define CM_AUTOFONT 625
#define CM_CLOSEFILES 626
#define CM_ALTDOWN 627
#define CM_ALTUP 628
#define CM_HELP 629
#define CM_ABOUT 630
#define CM_DETAILDIFF 631
#define CM_MAXLEFTVIEW  632
#define CM_MAXRIGHTVIEW 633
#define CM_SAMESIZEVIEW 634
#define CM_CYCLEVIEW    635
#define CM_ANTICYCLEVIEW 636
#define CM_HELPKEYBOARD 637
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
