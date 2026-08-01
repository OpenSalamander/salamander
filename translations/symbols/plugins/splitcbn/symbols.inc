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

///* X:\ALTAP\PETR\salamand\plugins\splitcbn\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//
#define IDD_CONFIG                      101
#define IDD_SPLIT                       102
#define IDD_COMBINE                     103
#define IDD_CRC                         104
#define IDC_EDIT_DIR                    1000
#define IDC_EDIT_TARGET                 1000
#define IDC_BUTTON_BROWSE               1001
#define IDC_BUTTON_UP                   1001
#define IDC_RADIO_SIZE                  1002
#define IDC_BUTTON_DOWN                 1002
#define IDC_COMBO_SIZE                  1003
#define IDC_BUTTON_ADD                  1003
#define IDC_RADIO_NUMBER                1004
#define IDC_BUTTON_REMOVE               1004
#define IDC_EDIT_LASTPART               1005
#define IDC_BUTTON_CRC                  1005
#define IDC_EDIT_NUMBER                 1006
#define IDC_BUTTON_BROWSE2              1006
#define IDC_BUTTON_CONFIG               1007
#define IDC_STATIC_TITLE                1009
#define IDC_CHECK_INCLUDE               1010
#define IDC_CHECK_CREATE                1011
#define IDC_LIST_FILES                  1011
#define IDC_CHECK_SPLITOTHER            1012
#define IDC_CHECK_COMBINEOTHER          1013
#define IDC_EDIT_CRC1                   1013
#define IDC_CHECK_SPLITSUB              1014
#define IDC_ICON_WARN                   1014
#define IDC_EDIT_CRC2                   1015
#define IDC_EDIT_CRC3                   1016
#define IDC_STATIC_CRCTITLE             1016
#define IDC_EDIT_CRC4                   1017
#define IDC_ICON_OK                     1018
#define IDC_SPLIT_ICON                  1019

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
///* X:\ALTAP\PETR\salamand\plugins\splitcbn\splitcbn.rh2
#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

#define IDC_STATIC_1 3000
#include "statics.rh2"   // 3000 az 3039 jsou timto zabrane !!!

// IDcka pro propojeni s helpem
#define IDH_COMBINE                     2500
#define IDH_SPLIT                       2501
#define IDH_SPLIT2                      2502  // obchazime chybu Resource Workshopu (jinak nejde .rc otevrit, tragedie)
#define IDH_SPLIT3                      2503  // obchazime chybu Resource Workshopu (jinak nejde .rc otevrit, tragedie)
#define IDH_SPLIT4                      2504  // obchazime chybu Resource Workshopu (jinak nejde .rc otevrit, tragedie)

// ********************************************************************************
//
//  TEXTY
//

#define IDS_PLUGINNAME                  1
#define IDS_OPENERROR                   2
#define IDS_SELECTDIR                   3
#define IDS_ZEROSIZE                    4
#define IDS_INSERTDISK                  5
#define IDS_FIXEDNOSENSE                6
#define IDS_OUTOFSPACE                  7
#define IDS_OUTOFMEM                    8
#define IDS_READERROR                   9
#define IDS_WRITEERROR                  10
#define IDS_INSERTNEXT                  11
#define IDS_SPLIT                       12
#define IDS_COMBINE                     13
#define IDS_TOOMANYPARTS                14
#define IDS_WRITING                     15
#define IDS_TOOLONGNAME                 16
#define IDS_ADDFILTER                   17
#define IDS_ADDTITLE                    18
#define IDS_BROWSETITLE                 19
#define IDS_CRCERROR                    21
#define IDS_PROCESSING                  22
#define IDS_ZEROFILES                   23
#define IDS_CANCEL                      24
#define IDS_CRCTITLE                    25
#define IDS_CRCDEC                      26
#define IDS_CRCHEX                      27
#define IDS_CRCNOTFOUND                 28
#define IDS_CALCCRC                     29
#define IDS_SPLITTITLE                  30
#define IDS_144FLOPPY                   31
#define IDS_720FLOPPY                   32
#define IDS_12FLOPPY                    33
#define IDS_360FLOPPY                   34
#define IDS_AUTODETECT                  35
#define IDS_FILECRCTITLE                36
#define IDS_BATTOOLONG                  37
#define IDS_BYTES                       38  // used when checking IDC_COMBO_SIZE syntax
#define IDS_BYTE                        39  // used when checking IDC_COMBO_SIZE syntax
#define IDS_PATHERROR                   40
#define IDS_TARGETPATHEXIST             41
#define IDS_WINPATH                     42
#define IDS_SPECIFYFILE                 43
#define IDS_MENU1                       63
#define IDS_MENU2                       64
#define IDS_ABOUTTITLE                  67
#define IDS_PLUGIN_DESCRIPTION          68
#define IDS_BATFILE_GENBYSAL            69
#define IDS_BATFILE_CTRL_C_TO_QUIT      70
#define IDS_BATFILE_DESCR               71
#define IDS_SIZE_B                      72
#define IDS_SIZE_KB                     73
#define IDS_SIZE_K                      74
#define IDS_SIZE_MB                     75
#define IDS_SIZE_GB                     76
#define IDS_100MB_ZIP                   77
#define IDS_250MB_ZIP                   78
#define IDS_120MB_LS120                 79
#define IDS_650MB_CDR                   80
#define IDS_700MB_CDR                   81
#define IDS_TOOLONGNAME2                82

// ********************************************************************************
//
//  IKONY
//

#define IDI_SPLIT                       2000
#define IDI_FILE                        2001
#define IDI_WARN                        2002
#define IDI_OK                          2003
#define IDC_DRAG                        2005
#define IDB_SPLIT                       2010
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
