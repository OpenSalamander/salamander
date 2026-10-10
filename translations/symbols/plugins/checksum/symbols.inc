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

///* X:\ALTAP\PETR\salamand\plugins\checksum\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//
#define IDD_CALCULATE                   1000
#define IDC_LIST_FILES                  1001
#define IDD_CONFIGURATION               1001
#define IDC_BUTTON_CONFIGURE            1002
#define IDC_BUTTON_SAVE                 1003
#define IDC_BUTTON_CLOSE                1004
#define IDC_LABEL_HINT                  1005
#define IDC_PROGRESS                    1006
#define IDC_LABEL                       1007
#define IDD_VERIFY                      2000
#define IDC_BUTTON_FOCUS                2001
#define IDC_LABEL_RESULT                2002

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
///* X:\ALTAP\PETR\salamand\plugins\checksum\checksum.rh2
#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

#define IDC_STATIC_1 300
#include "statics.rh2"   // 300 az 339 jsou timto zabrane !!!

#define IDS_PLUGINNAME                   1
#define IDS_ABOUTTITLE                   2
#define IDS_PLUGIN_DESCRIPTION           3
#define IDS_OUTOFMEM                     4
#define IDS_ERROROPENING                 5
#define IDS_READERROR                    6
#define IDS_WRITEERROR                   7
#define IDS_SKIPPED                     40
#define IDS_CALCULATING                 41
#define IDS_CANCELED                    42
#define IDS_HINT                        43
#define IDS_COPYTOCBOARD_CRC            44
#define IDS_COPYTOCBOARD_MD5            45
#define IDS_COPYTOCBOARD_SHA1           46
#define IDS_COPYTOCBOARD_SHA256         47
#define IDS_COPYTOCBOARD_SHA512         48
// 49-52 Reserved for other IDS_COPYTOCBOARD_xxx
#define IDS_REMOVEITEM                  53
#define IDS_SAVE_OVERWRITE              54
#define IDS_ERRORCREATINGFILE           55
#define IDS_SKIPPEDFILES                56
#define IDS_ERROROPENING2               57
#define IDS_MISSING                     58
#define IDS_BADEXT                      59
#define IDS_BADFILE                     60
#define IDS_VERIFYING                   61
#define IDS_OK                          62
#define IDS_CORRUPT                     63
#define IDS_MENU_VERIFY                 65
#define IDS_MENU_CALCULATE              66
#define IDS_CONFIG_CONFLICT             67
#define IDS_CONFIG_CHANGES_EFFECT       68
#define IDS_ALLOK                       70
#define IDS_RESULT                      71
#define IDS_BUSY                        72
#define IDS_CLOSE                       74
#define IDS_READINGTREE                 75
#define IDS_ERRORREADINGDIR             76
#define IDS_NOFILES                     77
#define IDS_VERIFYTITLE                 78
#define IDS_COLUMN_FILE                 79
#define IDS_COLUMN_SIZE                 80
#define IDS_COLUMN_STATUS               81
#define IDS_COLUMN_CRC                  82
#define IDS_COLUMN_MD5                  83
#define IDS_COLUMN_SHA1                 84
#define IDS_COLUMN_SHA256               85
#define IDS_COLUMN_SHA512               86
// 87-89 Reserved for other IDS_COLUMN_xxx
#define IDS_SAVE_TITLE                  90
#define IDS_SAVE_FILTER_CRC             91
#define IDS_SAVE_FILTER_MD5             92
#define IDS_SAVE_FILTER_SHA1            93
#define IDS_SAVE_FILTER_SHA256          94
#define IDS_SAVE_FILTER_SHA512          95
// 96-99 Reserved for other IDS_SAVE_FILTER_xxx
#define IDS_VERIFY_CRC                  100
#define IDS_VERIFY_MD5                  101
#define IDS_VERIFY_SHA1                 102
#define IDS_VERIFY_SHA256               103
#define IDS_VERIFY_SHA512               104
// 105-110 Reserved for other IDS_VERIFY_xxx
#define IDS_TOOLONGNAME                 120

#define IDI_FILE1                       10001
#define IDI_FILE2                       10002
#define IDI_FILE3                       10003
#define IDI_FILE4                       10004
#define IDI_CHECKMARK                   10005 
#define IDB_CHECKSUM                    10006


#define IDH_CALCCHKSUM                  3000
#define IDH_VERIFYCHKSUM                3001


#define IDC_CFG_SUM_1                   100
//#define IDC_CFG_CRC                     IDC_CFG_SUM_1  // tenhle zapis nezkompiluje HTML Help Compiler
#define IDC_CFG_CRC                     100
//#define IDC_CFG_MD5                     (IDC_CFG_CRC+1) // tenhle zapis nezkompiluje HTML Help Compiler
#define IDC_CFG_MD5                     101
//#define IDC_CFG_SHA1                    (IDC_CFG_MD5+1) // tenhle zapis nezkompiluje HTML Help Compiler
#define IDC_CFG_SHA1                    102
//#define IDC_CFG_SHA256                  (IDC_CFG_SHA1+1) // tenhle zapis nezkompiluje HTML Help Compiler
#define IDC_CFG_SHA256                  103
//#define IDC_CFG_SHA512                  (IDC_CFG_SHA256+1) // tenhle zapis nezkompiluje HTML Help Compiler
#define IDC_CFG_SHA512                  104
//#define IDC_CFG_SUM_COUNT               (IDC_CFG_SHA512+1) // tenhle zapis nezkompiluje HTML Help Compiler
#define IDC_CFG_SUM_COUNT               105
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
