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

///* X:\ALTAP\PETR\salamand\plugins\uniso\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//
#define IDD_CONFIGURATION               1200
#define IDC_CFG_READONLY                1201
#define IDC_CFG_SESSIONASDIR            1202
#define IDC_CFG_BOOTIMAGEASFILE         1203

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
///* X:\ALTAP\PETR\salamand\plugins\uniso\uniso.rh2
#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

#define IDC_STATIC_1 3000
#include "statics.rh2"   // 3000 az 3039 jsou timto zabrane !!!

//***********************************************************************************
//
// ikony, bitmapy, kurzory
//

#define IDB_UNISO                    900

//***********************************************************************************
//
// texty
//

#define IDS_PLUGINNAME              1000
#define IDS_CANT_OPEN               1001

#define IDS_UNKNOW_ERROR            1002
#define IDS_UNPACKING_ARCHIVE       1003
#define IDS_ERROR_UNPACKING         1004
#define IDS_CANT_HANDLE_FILE        1005
#define IDS_INSUFFICIENT_MEMORY     1006

#define IDS_ABOUT                   1007
#define IDS_PLUGIN_DESCRIPTION      1009
#define IDS_CANT_CREATE_TEMP        1010
#define IDS_CANT_DELETE_TEMP_FILE   1011
#define IDS_UNKNOWN_FILE_FORMAT     1012
#define IDS_CANT_OPEN_FILE          1013
#define IDS_ERR_TMP                 1014
#define IDS_CANT_SET_ATTRS          1015
#define IDS_CANT_CREATE_DIR         1016
#define IDS_YES                     1020
#define IDS_NO                      1021
#define IDS_ERROR                   1022
#define IDS_WARNING                 1023
#define IDS_ERROR_OPENING_MULTISESSION 1024
#define IDS_UNKNOWN_TRACK_TYPE      1025
#define IDS_UDF_UNKNOWN_ALLOCATION_STRATEGY 1026
#define IDS_CCD_BAD_FORMAT          1027
#define IDS_CCD_MISSING             1028
#define IDS_ERROR_READING_SECTOR    1029
#define IDS_UDF_INVALID_TAG_ID      1030
#define IDS_ERROR_LISTING_IMAGE     1031
#define IDS_INTERNAL_PLUGIN_ERROR   1032
#define IDS_UDF_ERROR_READING_VAT   1033
#define IDS_UDF_MISSING_VAT         1034
#define IDS_UDF_BAD_ROOT_ICB        1035
#define IDS_UDF_ICB_AND_FILESIZE_MISMATCH 1036
#define IDS_CANT_OPEN_TRACK         1037
#define IDS_BOOT_OR_MBR             1038

#define IDS_ERR_HFS_READ            1050
#define IDS_ERR_HFS_INVALID_PARTITION 1051
#define IDS_ERR_HFS_UNRECOGNIZED    1052
#define IDS_ERR_HFS_ORIGINAL_HFS    1053
#define IDS_ERR_HFS_SEEK_BLOCK_EXTR 1054
#define IDS_ERR_HFS_SEEK            1055
#define IDS_ERR_HFS_BTREE_NODETYPE  1056
#define IDS_ERR_HFS_BTREE_FRAGMENTED 1057
#define IDS_ERR_HFS_FILE_FRAGMENTED 1058

#define IDS_ERR_TOO_LONG_PATH       1059
#define IDS_ERR_TOO_LONG_NAME       1060

#define IDS_ERR_DMG_READ_HDR        1070
#define IDS_ERR_DMG_ENCRYPTED       1071
#define IDS_ERR_DMG_NO_FOOTER       1072
#define IDS_ERR_DMG_PLIST           1073
#define IDS_ERR_DMG_BLOCK_UNDEFINED 1074

#define IDS_ERR_BF_READ             1075
#define IDS_ERR_BF_SEEK             1076
#define IDS_ERR_BF_UNSUPPORTED_SEEK_METHOD 1077
#define IDS_ERR_BF_BLOCK_UNK_TYPE   1078
#define IDS_ERR_BF_ZLIB             1079
#define IDS_ERR_BF_BZIP2            1080

#define IDS_ERR_ISZ_NO_HEADER       1085
#define IDS_ERR_ISZ_READ_HDR        1086
#define IDS_ERR_ISZ_SEGMENTED       1087
#define IDS_ERR_ISZ_ENCRYPTED       1088

#define IDS_READERROR               1100
#define IDS_WRITEERROR              1101
#define IDS_AUDIO_NOT_EXTRACTABLE   1102
#define IDS_CANCELING_OPERATION     1103

#define IDS_INFO_LABEL              1150
#define IDS_INFO_LABEL_LABEL        1151
#define IDS_INFO_CNT_SESSIONS       1152
#define IDS_INFO_SESSION_NUM        1153
#define IDS_INFO_CNT_TRACKS         1154
#define IDS_INFO_TRACK_TYPE_SIZE    1155
#define IDS_INFO_TRACK_TYPE_SIZE_LABEL 1156
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
