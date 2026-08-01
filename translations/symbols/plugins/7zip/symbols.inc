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

///* X:\ALTAP\PETR\salamand\plugins\7zip\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//
#define IDD_CONFIGURATION               1200
#define IDD_NEWARCHIVE                  1201
#define IDC_CFG_SHOWEXTENDEDOPTIONS     1201
#define IDD_ENTERPASSWORD               1202
#define IDC_CFG_EXTENDEDLISTINFO        1202
#define IDC_CFG_LISTINFOPACKEDSIZE      1203
#define IDC_CFG_LISTINFOMETHOD          1204
#define IDC_CFG_COMPRESS_LEVEL          1205
#define IDC_CFG_COMPRESS_METHOD         1206
#define IDC_CFG_DICT_SIZE               1207
#define IDC_CFG_WORD_SIZE               1208
#define IDC_CFG_SOLID_ARCHIVE           1209
#define IDC_NA_ARCHIVE                  1210
#define IDC_NA_ENCRYPTFILES             1211
#define IDC_NA_PASSWORD                 1212
#define IDC_NA_CONFIRMPASSWORD          1213
#define IDC_NA_NOTAGAIN                 1214
#define IDC_NA_PASSWORD_TXT             1215
#define IDC_NA_CONFIRMPASSWORD_TXT      1216
#define IDC_SHOW_CONFIG                 1217
#define IDC_PASSWORD                    1220
#define IDC_SKIP                        1221
#define IDC_SKIPALL                     1222
#define IDC_FILE                        1223
#define IDC_LOCK_ICON                   1224

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
///* X:\ALTAP\PETR\salamand\plugins\7zip\7zip.rh2
#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

#define IDC_STATIC_1 1300
#include "statics.rh2"   // 1300 az 1339 jsou timto zabrane !!!

//***********************************************************************************
//
// ikony, bitmapy, kurzory
//

#define IDB_7ZIP                    900
#define IDI_LOCK                    901

//***********************************************************************************
//
// texty
//

#define IDS_PLUGINNAME              1000
#define IDS_ABOUT                   1001
#define IDS_PLUGIN_DESCRIPTION      1002
#define IDS_INSUFFICIENT_MEMORY     1003
#define IDS_CANT_LOAD_LIBRARY       1004
#define IDS_CANT_GET_CRATEOBJECT    1005
#define IDS_CANT_GET_CLASS_OBJECT   1006
#define IDS_CANT_OPEN_ARCHIVE       1007
#define IDS_CANT_OPEN_ARCHIVE_PWD   1008
#define IDS_ERROR                   1009
#define IDS_PACKING_ARCHIVE         1010
#define IDS_UNPACKING_ARCHIVE       1011
#define IDS_READING_DIRTREE         1012
#define IDS_CANT_CREATE_ARCHIVE     1013
#define IDS_REMOVING                1014
#define IDS_TESTARCHIVE             1015
#define IDS_TESTARCHIVECORRUPTED    1017
#define IDS_TESTARCHIVEOK           1018
#define IDS_TESTING_ARCHIVE         1019
#define IDS_INCORRECTPASSWORD       1020
#define IDS_INCORRECTPASSWORDBODY   1021
#define IDS_PASSWORDSNOTMATCH       1022
#define IDS_EMPTYPASSWORD           1023
#define IDS_UPDATE_NOT_SUPPORTED    1024
#define IDS_CANT_MOVE_TMPARCHIVE    1025
#define IDS_CANT_DELETE_TMPARCHIVE  1026
#define IDS_7Z_UPDATE_ERROR         1027
#define IDS_7Z_SOLID_DELETE_UNSUP   1028
#define IDS_CANT_UPDATE_ARCHIVE     1029
#define IDS_CANT_CREATE_TMPFILE     1030
//#define IDS_CANT_CREATEMUTEX        1030
#define IDS_ARCHIVE_CHANGED         1031
#define IDS_DELETE_ERROR            1032
#define IDS_ADD_FILES_TO_ARCHIVE    1033
#define IDS_CREATE_NEW_ARCHIVE      1034
#define IDS_REMOVING_FILES          1035
#define IDS_READING_ARCHIVEITEMS    1036
#define IDS_UNPACKING               1037
#define IDS_PACKING                 1038
#define IDS_UPDATING_ARCHIVE        1039
#define IDS_CANCELING_OPERATION     1040
#define IDS_UNSUPPORTED_METHOD      1041
#define IDS_CRC_FAILED              1042
#define IDS_UNKNOWN_ERROR           1043
#define IDS_DATA_ERROR              1044
#define IDS_DATA_ERROR_PWD          1045
#define IDS_BTN_DELETE              1046
#define IDS_BTN_DELETE_ALL          1047
#define IDS_BTN_KEEP                1048
#define IDS_BTN_KEEP_ALL            1049
#define IDS_ERROR_PROCESSING_FILE   1050
#define IDS_ERROR_PROCESSING_FILE_PWD 1051
#define IDS_OVERWRITE_FILE          1052
#define IDS_UPDATING                1053
#define IDS_READONLY_ARCHIVE        1054
#define IDS_CANT_REMOVE_DIRECTORY   1055
#define IDS_CANT_READ               1056
#define IDS_CANT_READ_ARCHIVE       1057
#define IDS_CANT_WRITE              1058

#define IDS_STORE                   1060
#define IDS_FASTEST                 1061
#define IDS_FAST                    1062
#define IDS_NORMAL                  1063
#define IDS_MAXIMUM                 1064
#define IDS_ULTRA                   1065

#define IDS_METHOD_LZMA2            1069
#define IDS_METHOD_LZMA             1070
#define IDS_METHOD_PPMD             1071

#define IDS_PACK_UPDATE_ERROR       1072
#define IDS_UNPACK_ERROR            1073
#define IDS_TEST_ERROR              1074
#define IDS_UNSUPPORTED_ARCHIVE     1075

#define IDS_7Z_FATAL_ERROR          1076
#define IDS_7Z_UPDATE_UNKNOWN_ERROR 1077
//#define IDS_7Z_SOLID_UPDATE_UNSUP   1078
#define IDS_7Z_CREATE_UNKNOWN_ERROR 1079

#define IDS_LISTINFO_PAKEDSIZE      1080
#define IDS_LISTINFO_PAKEDSIZE_DESC 1081
#define IDS_LISTINFO_METHOD         1082
#define IDS_LISTINFO_METHOD_DESC    1083

#define IDS_TESTING_ARCHIVE_NAME    1084

#define IDS_BTN_RETRY               1090
#define IDS_BTN_ABORT               1091

#define IDS_ARCLOCK_ERROR           1095

#define IDS_ERRADDFILE_TOOLONG      1096
#define IDS_ERRADDDIR_TOOLONG       1097
#define IDS_NAMEISTOOLONG           1098

//***********************************************************************************
//
// help IDs
//

#define IDH_TESTARCHIVE             1500
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
