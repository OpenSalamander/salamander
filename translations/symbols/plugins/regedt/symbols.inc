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

///* X:\ALTAP\PETR\salamand\plugins\regedt\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//
#define IDD_CREATEKEY                   100
#define IDE_NAME                        101
#define IDS_TEXT                        102
#define IDE_FILE                        102
#define IDC_DIRECT                      103
#define IDD_NEWVALUE                    110
#define IDC_TYPE                        111
#define IDD_EDITVAL                     120
#define IDE_DATA                        121
#define IDR_HEX                         122
#define IDR_DEC                         123
#define IDC_BASE                        124
#define IDS_BASE                        124
#define IDD_COPY                        130
#define IDD_SEARCH                      135
#define IDC_PATTERN                     136
#define IDC_LOOKIN                      137
#define IDC_OPTIONS                     139
#define IDC_SUBDIRS                     140
#define IDS_LOOKAT                      141
#define IDC_KEYS                        142
#define IDC_CASE                        143
#define IDC_WHOLEWORDS                  144
#define IDC_VALUES                      145
#define IDC_DATA                        146
#define IDC_HEX                         147
#define IDC_FOUND_FILES                 148
#define IDC_RESULTS                     149
#define IDC_REGEXP                      150
#define IDC_MINTIME                     152
#define IDC_MAXTIME                     153
#define IDS_OPTIONS                     154
#define IDS_NEWER                       155
#define IDS_OLDER                       156
#define IDS_FORMAT1                     157
#define IDS_FORMAT2                     158
#define IDC_STATUS                      159
#define IDD_RAWEDITVAL                  165
#define IDS_MESSAGE                     166
#define IDD_CONFIG                      166
#define IDB_EDIT                        167
#define IDE_INITDIR                     167
#define IDB_INITDIRHELP                 168
#define IDB_ARGHELP                     169
#define IDE_ARGUMENTS                   170
#define IDB_BROWSE                      171
#define IDE_COMMAND                     172
#define IDD_RENAME                      175
#define IDS_NOTDWORD                    176
#define IDD_EXPORT                      177

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
///* X:\ALTAP\PETR\salamand\plugins\regedt\regedt.rh2
#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

#define IDC_STATIC_1 700
#include "statics.rh2"   // 700 az 739 jsou timto zabrane !!!

#define IDI_REGEDT 500
#define IDI_SZ 501
#define IDI_BIN 502
#define IDI_UNKNOWN 503
#define IDI_FIND  504
#define IDB_REGEDT 505

// menu commandy
#define CMD_EXPORT  800
#define CMD_SEARCH  801
#define CMD_NEWVALUE  802
#define CMD_RAWEDIT 803
#define CMD_COPYNAME  804
#define CMD_COPYFULLNAME  805
#define CMD_COPYPATH  806

#define CMD_FINDFOCUS 850

#define IDS_PLUGINNAME   1000
#define IDS_LOWMEM    1001
#define IDS_DRIVEMENUTEXT    1002
#define IDS_DEFAULTVALUE 1003
#define IDS_TYPE 1004
#define IDS_DATA 1005
#define IDS_SIZE 1006
#define IDS_KEY 1007
#define IDS_EMPTY 1008
#define IDS_ERROR 1009
#define IDS_NEWKEYINROOT 1010
#define IDS_LONGNAME 1011
#define IDS_NEWKEY 1012
#define IDS_KEYEXISTS 1013
#define IDS_DELVAL 1014
#define IDS_DELKEY 1015
#define IDS_CONFIRNNONEMPTYDELETE 1016
#define IDS_QUESTION 1017
#define IDS_DELKEYINROOT 1018
#define IDS_MENUNEWKEY 1019
#define IDS_MENUNEWVAL 1020
#define IDS_EDIT  1022
#define IDS_NEWVALINROOT  1028
#define IDS_NEWVAL  1029
#define IDS_REPLACEVAL  1030
#define IDS_INVALIDHEXVALUE 1031
#define IDS_INVALIDDECVALUE 1032
#define IDS_REGEDTERR 1033
#define IDS_QUERYVAL  1034
#define IDS_SETVAL  1035
#define IDS_BADPATH 1036
#define IDS_OPENKEY 1037
#define IDS_ENUMSUBKEY  1038
#define IDS_ENUMVALUE 1039
#define IDS_ENUMKEY 1040
#define IDS_EXPORT  1041
#define IDS_SEARCH  1042
#define IDS_QUICKRENAMEKEY  1043
#define IDS_QUICKRENAMEVAL  1044
#define IDS_QUICKRENAME 1045
#define IDS_COPY  1046
#define IDS_COPY1 1047
#define IDS_MOVE  1048
#define IDS_MOVE1 1049
#define IDS_SAMETARGET  1050
#define IDS_OPENKEY2  1051
#define IDS_ACCESS  1052
#define IDS_CANTCOPYTOITSELF  1053
#define IDS_CREATE  1054
#define IDS_OPEN  1055
#define IDS_ACCESS2 1056
#define IDS_DELETE  1057
#define IDS_COPYKEY 1058
#define IDS_MOVEKEY 1059
#define IDS_SETVAL2 1060
#define IDS_REMOVESOURCE  1061
#define IDS_COPYVALUE 1062
#define IDS_MOVEVALUE 1063
#define IDS_REMOVESOURCE2 1064
#define IDS_CANTCOPYTOITSELF2 1065
#define IDS_CANTMOVETOITSELF  1066
#define IDS_CANTMOVETOITSELF2 1067
#define IDS_COPYCLASSNAME 1068
#define IDS_WARNING 1069
#define IDS_CANNOTRENAME  1070
#define IDS_RENAMEKEY 1071
#define IDS_SAMECASE  1072
#define IDS_RENAMEVAL 1073
#define IDS_SAMECASEKEY 1074
#define IDS_SAMECASEVAL 1075
#define IDS_SYNTAX  1076
#define IDS_CREATETARGET  1077
#define IDS_COPYORMOVE1 1078
#define IDS_COPYORMOVE2 1079
#define IDS_COPYORMOVE3 1080
#define IDS_COPYORMOVE4 1081
#define IDS_COPYORMOVE5 1082
#define IDS_MOVERENAME  1083
#define IDS_COPYPROGRESS  1084
#define IDS_MOVEPROGRESS  1085
#define IDS_DELETEPROGRESS  1086
#define IDS_CANCEL  1087
#define IDS_COPYTOTROOT 1088
#define IDS_STOP  1089
#define IDS_START 1090
#define IDS_FOUNDITEMS1 1091
#define IDS_FOUNDITEMS2 1092
#define IDS_NOFILESFOUND  1093
#define IDS_STOPPED 1094
#define IDS_NAME_COLUMN 1095
#define IDS_TYPE_COLUMN 1096
#define IDS_DATA_COLUMN 1097
#define IDS_PATH_COLUMN 1098
#define IDS_SIZE_COLUMN 1099
#define IDS_DATE_COLUMN 1100
#define IDS_TIME_COLUMN 1101
#define IDS_FIND  1102
#define IDS_DESCRIPTION 1103
#define IDS_ABOUT 1105
#define IDS_SEARCHERROR 1107
#define IDS_BADHEXSTRING  1108
#define IDS_REGEXPERR 1109
#define IDS_SEARCHING 1110
#define IDS_SUBSTRINGHELP 1111
#define IDS_HEXHELP 1112
#define IDS_REGEXPHELP  1113
#define IDS_LONGDATA  1114
#define IDS_BADTIMEFORMAT 1115
#define IDS_CREATETEMP  1116
#define IDS_WRITETEMP   1117
#define IDS_RAWEDIT 1118
#define IDS_EXP_FULLNAME  1119
#define IDS_EXP_DRIVE 1120
#define IDS_EXP_PATH 1121
#define IDS_EXP_NAME  1122
#define IDS_EXP_NAMEPART  1123
#define IDS_EXP_EXTPART 1124
#define IDS_EXP_FULLPATH  1125
#define IDS_EXP_WINDIR  1126
#define IDS_EXP_SYSDIR  1127
#define IDS_EXP_DOSFULLNAME 1128
#define IDS_EXP_DOSDRIVE  1129
#define IDS_EXP_DOSPATH 1130
#define IDS_EXP_DOSNAME 1131
#define IDS_EXP_DOSNAMEPART 1132
#define IDS_EXP_DOSEXTPART  1133
#define IDS_EXP_DOSFULLPATH 1134
#define IDS_EXP_DOSWINDIR 1135
#define IDS_EXP_DOSSYSDIR 1136
#define IDS_EXP_ENVVAR 1137
#define IDS_EXEFILES  1138
#define IDS_OPENTEMP  1139
#define IDS_READTEMP  1140
#define IDS_SIZEOFTEMP 1141
#define IDS_PROCESS 1142
#define IDS_REGFILES  1143
#define IDS_FILENAMEISDIR 1144
#define IDS_OVERWRITE 1145
#define IDS_OVERWRITETITLE  1146
#define IDS_REPLACEERROR  1147
#define IDS_PROCESS2  1148
#define IDS_BADEXPORT 1149
#define IDS_EXPORTING 1150
#define IDS_SELECTED1 1151
#define IDS_SELECTED2 1152
#define IDS_SELECTED3 1153
#define IDS_SELECTEDSIZE  1154
#define IDS_CANNOTRENAMEDEFVAL 1158
#define IDS_COPYNAME  1159
#define IDS_COPYFULLNAME  1160
#define IDS_COPYPATH  1161
#define IDS_BUSY  1162
#define IDS_NOTREGEDTPATH 1163
#define IDS_EXP_BROWSE 1164
#define IDS_EXP_SALDIR 1165

// IDcka pro propojeni s helpem
#define IDH_SEARCHREG  1500
#define IDH_NEWKEY     1501
#define IDH_NEWVALUE   1502
#define IDH_EXPORT     1503
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
