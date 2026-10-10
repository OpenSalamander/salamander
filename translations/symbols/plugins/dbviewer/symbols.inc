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

///* X:\ALTAP\PETR\salamand\plugins\dbviewer\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//
#define IDC_GOTO_RECORD                 100
#define IDD_PROPERTIES                  12200
#define IDC_PROP_INFO                   12201
#define IDD_CONFIGURATION               12220
#define IDB_CFG_FONT                    12221
#define IDE_CFG_FONT                    12222
#define IDC_CFG_SAVEPOSONCLOSE          12223
#define IDC_CFG_SETBYMAINWINDOW         12224
#define IDB_CFG_CSVOPTIONS              12225
#define IDD_COLUMNS                     12240
#define IDC_COL_LV                      12241
#define IDC_COL_RESTORE                 12242
#define IDD_CSV_CONFIG                  12250
#define IDC_CSV_SETDEF                  12252
#define IDC_CSV_NONE                    12253
#define IDC_CSV_QUOTES                  12254
#define IDC_CSV_DOUBLE                  12255
#define IDE_CSV_OTHER                   12256
#define IDC_CSV_OTHER                   12257
#define IDC_CSV_SPACE                   12258
#define IDC_CSV_COMMA                   12259
#define IDC_CSV_SEMICOLON               12260
#define IDC_CSV_TABULATOR               12261
#define IDC_CSV_AS_TQ                   12262
#define IDC_CSV_AS_FR                   12263
#define IDC_CSV_AS_SEP                  12264
#define IDC_CSV_ASHEADER                12265
#define IDC_CSV_ASDATA                  12266
#define IDD_FIND                        12300
#define IDC_FIND_TEXT                   12301
#define IDC_FIND_SFORWARD               12302
#define IDC_FIND_SBACKWARD              12303
#define IDC_FIND_WHOLEWORDS             12304
#define IDC_FIND_CASESENSITIVE          12305
#define IDC_FIND_REGEXP                 12306
#define IDD_GOTO                        12400

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
///* X:\ALTAP\PETR\salamand\plugins\dbviewer\dbviewer.rh2
#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

#define IDC_STATIC_1 13000
#include "statics.rh2"   // 13000 az 13039 jsou timto zabrane !!!

// Bitmaps
#define IDB_TOOLBAR16               9970
#define IDB_TOOLBAR256              9971

// Cursors
#define IDC_SELECT                  9980
#define IDC_SPLIT                   9981

// Icons
#define IDI_DELETED                 9990
#define IDI_MAIN                    9991
#define IDI_MARKED                  9992

// Accelerators
#define IDA_ACCELERATORS            9998

// Menu
#define CM_FILE_OPEN               10001
#define CM_FIELDS                  10002
#define CM_PROPERTIES              10003
#define CM_EXIT                    10004
#define CM_ABOUT                   10005
#define CM_COPY                    10006
#define CM_FIND                    10007
#define CM_FIND_NEXT               10008
#define CM_FIND_PREV               10009
#define CM_GOTO                    10010
#define CM_TOGGLE_BOOKMARK         10011
#define CM_NEXT_BOOKMARK           10012
#define CM_PREV_BOOKMARK           10013
#define CM_FULLSCREEN              10014
#define CM_VIEW_NORMAL             10015
#define CM_VIEW_RECORD             10016
#define CM_VIEW_WATCH              10017
#define CM_CONFIGURATION           10018
#define CM_SHOW_ALL                10019
#define CM_HIDE_DELETED            10020
#define CM_SORT_BY_DELETED         10021
#define CM_SORT_BY_ORIGINAL        10022
#define CM_SORT_ASCEND             10023
#define CM_SORT_DESCEND            10024
#define CM_CLEAR_ALL_BOOKMARKS     10025
#define CM_SELECT_ALL              10026
#define CM_CODING_RECOGNIZE        10027
#define CM_CODING_DEFAULT          10028
#define CM_CODING_NEXT             10029
#define CM_CODING_PREV             10030
#define CM_CSV_OPTIONS             10031
#define CM_FILE_REFRESH            10032
#define CM_HELP_CONTENTS           10033
#define CM_HELP_INDEX              10034
#define CM_HELP_SEARCH             10035
#define CM_FILE_PREV               10036
#define CM_FILE_NEXT               10037
#define CM_FILE_PREVSELFILE        10038
#define CM_FILE_NEXTSELFILE        10039
#define CM_FILE_FIRST              10040
#define CM_FILE_LAST               10041

// for Coversion submenu
#define CM_CODING_FIRST            10050
#define CM_CODING_LAST             10250

#define CML_FILES                  10251
#define CML_OTHERFILES             10252
#define CML_EDIT                   10253
#define CML_VIEW                   10254
#define CML_CONVERT                10255
#define CML_OPTIONS                10256
#define CML_HELP                   10257

// Strings
#define IDS_PLUGINNAME             11089
#define IDS_PLUGIN_DESCRIPTION     11090
#define IDS_ABOUT                  11092

#define IDS_OPENED_WINDOWS         11101

#define IDS_DBFE_OOM               11110
#define IDS_DBFE_NOT_XBASE         11111
#define IDS_DBFE_FILE_NOT_FOUND    11112
#define IDS_DBFE_READ_ERROR        11113
#define IDS_DBFE_WRITE_ERROR       11114
#define IDS_DBFE_SEEK_ERROR        11115
#define IDS_DBFE_NO_MEMO_FILE      11116
#define IDS_DBFE_INVALID_BLOCK     11117

#define IDS_VIEWERFILTER           11120

 // font description in configuration dialog   "10 pt. MS Shell Dlg"
#define IDS_FONTDESCRIPTION        11121

// The configuration is already opened.
#define IDS_CFG_CONFLICT           11122

// Fields: Name column
#define IDS_COLUMNS_NAME           11123
// Fields:Type column
#define IDS_COLUMNS_TYPE           11124
// Fields: Length column
#define IDS_COLUMNS_LENGTH         11125
// Fields: Decimals column
#define IDS_COLUMNS_DECIMALS       11126

// dBase versions
#define IDS_FILEVER_DBASE2         11130
#define IDS_FILEVER_DBASE3         11131
#define IDS_FILEVER_DBASE4         11132
#define IDS_FILEVER_DBASE5         11133
#define IDS_FILEVER_VISUALDBASE7   11134
#define IDS_FILEVER_FOXPRO         11135
#define IDS_FILEVER_UNKNOWN        11136

// Field Type
#define IDS_FTYPE_CHAR             11140
#define IDS_FTYPE_NUM              11141
#define IDS_FTYPE_LOGICAL          11142
#define IDS_FTYPE_DATE             11143
#define IDS_FTYPE_MEMO             11144
#define IDS_FTYPE_FLOAT            11145
#define IDS_FTYPE_BIN              11146
#define IDS_FTYPE_GEN              11147
#define IDS_FTYPE_PIC              11148
#define IDS_FTYPE_CUR              11149
#define IDS_FTYPE_DTIME            11150
#define IDS_FTYPE_INT              11151
#define IDS_FTYPE_UNKOWN           11152
#define IDS_FTYPE_DOUBLE           11153
#define IDS_FTYPE_TSTAMP           11154
#define IDS_FTYPE_AUTOINC          11155

// At least on of the fields must be visible.
#define IDS_NOVISIBLE_FIELD        11160

// There is no valid field in this database.
#define IDS_NOVALIDCOLUMN          11162

// True/False for logical column
#define IDS_TRUE                   11163
#define IDS_FALSE                  11164

// file information
#define IDS_FINFO_MODIFIED         11165
#define IDS_FINFO_SIZE             11166
#define IDS_FINFO_VERSION          11167
#define IDS_FINFO_RECCOUNT         11168
#define IDS_FINFO_FIELDCOUNT       11169
#define IDS_FINFO_MEMOCOUNT        11170
#define IDS_FINFO_HDRSIZE          11171
#define IDS_FINFO_RECSIZE          11172
#define IDS_FINFO_CODEPAGE         11173

// Coding
#define IDS_CODING_MENU            11190
// None
#define IDS_NONE_CODING            11191
// Next
#define IDS_NEXT_CODING            11192
// Previous
#define IDS_PREV_CODING            11193

// Regular Expression Error
#define IDS_REGEXP_ERROR           11194
// Messagebox title
#define IDS_FIND                   11195
// Unable to match: %s
#define IDS_FIND_NOMATCH           11196
// Unable to match regular expression: %s
#define IDS_FIND_NOREGEXPMATCH     11197

// Main Menu

// File
#define IDS_MENU_FILE              11200
#define IDS_MENU_FILE_OPEN         11201
#define IDS_MENU_FILE_REFRESH      11202
#define IDS_MENU_FILE_PROPERTIES   11203
#define IDS_MENU_FILE_OTHER        11204
#define IDS_MENU_FILE_PREV         11205
#define IDS_MENU_FILE_NEXT         11206
#define IDS_MENU_FILE_PREVSEL      11207
#define IDS_MENU_FILE_NEXTSEL      11208
#define IDS_MENU_FILE_FIRST        11209
#define IDS_MENU_FILE_LAST         11210
#define IDS_MENU_FILE_EXIT         11211

// Edit
#define IDS_MENU_EDIT              11217
#define IDS_MENU_EDIT_COPY         11218
#define IDS_MENU_EDIT_SELECT_ALL   11219
#define IDS_MENU_EDIT_FIND         11220
#define IDS_MENU_EDIT_FIND_NEXT    11221
#define IDS_MENU_EDIT_FIND_PREV    11222
#define IDS_MENU_EDIT_TOGGLE_BM    11223
#define IDS_MENU_EDIT_NEXT_BM      11224
#define IDS_MENU_EDIT_PREV_BM      11225
#define IDS_MENU_EDIT_CLEAR_ALL_BM 11226

// View
#define IDS_MENU_VIEW      11230
#define IDS_MENU_VIEW_GOTO       11231
#define IDS_MENU_VIEW_FIELDS     11232
#define IDS_MENU_VIEW_FULLSCREEN   11233

// Convert
#define IDS_MENU_CONVERT                11250
#define IDS_MENU_CONVERT_RECOGNIZE      11251
#define IDS_MENU_CONVERT_CODING_FIRST   11252
#define IDS_MENU_CONVERT_CODING_DEF     11253
#define IDS_MENU_CONVERT_CODING_NEXT    11254
#define IDS_MENU_CONVERT_CODING_PREV    11255

// Options
#define IDS_MENU_OPTIONS           11270
#define IDS_MENU_OPTIONS_CSV_OPTIONS 11271
#define IDS_MENU_OPTIONS_CONFIG    11272

// Help
#define IDS_MENU_HELP              11280
#define IDS_MENU_HELP_ABOUT        11281
#define IDS_MENU_HELP_CONTENTS     11282
#define IDS_MENU_HELP_INDEX        11283
#define IDS_MENU_HELP_SEARCH       11284

// Toolbar Tooltips
#define IDS_TBTT_COPY              11500
#define IDS_TBTT_OPEN              11501
#define IDS_TBTT_FIND              11502
#define IDS_TBTT_PROPERTIES        11503
#define IDS_TBTT_FULLSCREEN        11504
#define IDS_TBTT_GOTO              11505

// help IDs

#define IDH_INTRODUCTION 9000
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
