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

///* X:\ALTAP\PETR\salamand\plugins\undelete\lang\lang.rh
//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by lang.rc
//
#define IDD_SNAPSHOTPROGRESSDLG         1000
#define IDC_LABEL_FILENAME              1001
#define IDC_PROGRESSBAR                 1002
#define IDD_CONNECT                     1100
#define IDC_CHECK_SCANVACANT            1101
#define IDC_CHECK_SHOWEXISTING          1102
#define IDC_LIST_VOLUMES                1103
#define IDC_CHECK_IMAGE                 1104
#define IDC_EDIT_IMAGE                  1105
#define IDC_CHECK_SHOWZEROFILES         1106
#define IDC_CHECK_SHOWEMPTYDIRS         1107
#define IDC_CHECK_SHOWMETAFILES         1108
#define IDC_CHECK_ESTIMATEDAMAGE        1109
#define IDD_COPYPROGRESSDLG             1200
#define IDC_LABEL_SOURCE                1201
#define IDC_LABEL_DEST                  1202
#define IDC_PROGRESS_FILE               1203
#define IDC_PROGRESS_TOTAL              1204
#define IDC_LABEL_UNDELETING            1205
#define IDD_FILENAME                    1300
#define IDC_EDIT_FILENAME               1301
#define IDC_BUTTON_ALL                  1302
#define IDD_CONFIG                      1400
#define IDC_EDIT_TEMPPATH               1401
#define IDC_BUTTON_BROWSE               1402
#define IDC_CHECK_ALWAYSREUSE           1403
#define IDC_CHECK_SAMEPARTITION         1404
#define IDC_CHECK_EFS                   1405
#define IDD_RESTORE                     1500
#define IDC_EDIT_TARGET                 1501

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        8501
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         8700
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
///* X:\ALTAP\PETR\salamand\plugins\undelete\undelete.rh2
#pragma once

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

#define IDC_STATIC_1 3000
#include "statics.rh2"   // 3000 to 3039 are occupied!

#include "library\UndeleteDLL\texts.rh2"

#define IDS_UNDELETE                1
#define IDS_DESCRIPTION             3
#define IDS_ABOUTTITLE              4
//#define IDS_ERRORMETADAMAGED        8
#define IDS_PATHDISK                18
#define IDS_PATHNOTFOUND            19
#define IDS_PATHFILE                20
#define IDS_ERROROPENINGPROGRESS    22
#define IDS_OPENINGVOLUME           23
#define IDS_SCANNINGFOUND           32
#define IDS_ALWAYSCHOOSEYES         34
#define IDS_PATHTOOLONG             42
#define IDS_QUESTION                43
#define IDS_WANTCANCEL              44
#define IDS_BADPATH                 45
#define IDS_ERROR                   46
#define IDS_VIEW                    47
#define IDS_TEMPDIR                 48
#define IDS_VOLENUMERATE            49
#define IDS_IMAGENOTFOUND           50
#define IDS_WAIT                    59
#define IDS_ENCRYPTEDWARNING        60
#define IDS_DONTSHOWAGAIN           61
#define IDS_CHOOSETEMPDIR           62
#define IDS_SAMEPARTITION           63
#define IDS_UNDELETECMD             65
#define IDS_ERRORENCRYPTED          66
#define IDS_RESTORECMD              67
#define IDS_CANNOTVIEW              68
#define IDS_CHOOSETARGET            69
#define IDS_READINGTREE             70
#define IDS_RESTORE                 71
#define IDS_RESTORING               72
#define IDS_NOEFS                   73
#define IDS_VOLUME_MOUNT            80
#define IDS_VOLUME_NAME             81
#define IDS_VOLUME_FORMAT           82
#define IDS_VOLUME_SIZE             83
#define IDS_VOLUME_FREE             84
#define IDS_VOLUME_FREEPROC         85
#define IDS_VOLUME_VOLID            86
//#define IDS_VOLUME_          87, 88, 89, 90
#define IDS_CONDITION               95
#define IDS_CONDITIONDESC           96
#define IDS_CONDITION_GOOD          97
#define IDS_CONDITION_FAIR          98
#define IDS_CONDITION_POOR          99
#define IDS_CONDITION_LOST          100
#define IDS_CONDITION_UNKNOWN       101
#define IDS_UNDELETEINCHDRVMENU     102
#define IDS_PARTIALRECOVER          103
#define IDS_PARTIALRECOVER_DONTASK  104
#define IDS_ERRORRECOVER            105

#define IDH_UNDELETE             2000
#define IDH_RESTOREENCRFILES     2001

#define IDI_FS                   30000
#define IDB_FS                   30001
#define IDI_DELETED              30002
///* X:\ALTAP\PETR\salamand\plugins\undelete\library\undeletedll\texts.rh2
// ****************************************************************************
//
//   TEXTS.RH2
//
//   Copyright (c) 2004-2016 ALTAP, spol. s r.o.
//
//   This file and all others associated with it contains trade secrets and
//   information that is proprietary to ALTAP, spol. s r.o. It may not be copied or
//   distributed to any person or firm without the express written permission
//   of ALTAP Ltd. This permission is available only in the form of a Software
//   License Agreement.
//
// ****************************************************************************


#ifndef __TEXTS_RH2
#define __TEXTS_RH2

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

// here are texts shared between Undelete plugin and DLL
#define IDS_LOWMEM                  2
#define IDS_ERRORREADINGMFT         6
#define IDS_MFTRECORDDAMAGED        9
#define IDS_ROOTNOTFOUND            10
#define IDS_VDIRECTORY              11
#define IDS_ERRORNOTNTFS            12
#define IDS_ERRORNOTOPEN            13
#define IDS_ERROROPENINGVOLUME      14
#define IDS_ERRORREADINGBOOT        15
#define IDS_ALLDELETEDFILES         17
#define IDS_READINGMFT              21
#define IDS_ERRORVOLUMETYPE         24
#define IDS_ERRORREADINGFAT         25
#define IDS_ERRORREADINGDIRCLUSTERS 26
#define IDS_ERRORNOTFAT             27
#define IDS_READINGEXISTINGDIRS     28
#define IDS_LOADINGFAT              29
#define IDS_READINGDELETEDDIRS      30
#define IDS_SCANNINGVACANTCLUSTERS  31
#define IDS_REUSESCANNEDINFO        33
#define IDS_RELOADINGCLUSTERS       35
#define IDS_FLOPPY350               36
#define IDS_FLOPPY525               37
#define IDS_FLOPPY800               38
#define IDS_COMPACT_DISK            39
#define IDS_LOCAL_DISK              40
#define IDS_REMOVABLE_DISK          41
#define IDS_ANALYZINGDIRS           64
#define IDS_READINGDIRS             74
#define IDS_METAFILES               75
#define IDS_INCOMPLETEMFT           77
#define IDS_BITMAPISEMPTY           90
#define IDS_BITMAPNOTFOUND          91
#define IDS_BITMAPNOSTREAM          92
#define IDS_LOWMEMBITMAP            93
#define IDS_ERRORREADINGBMP         94

#endif
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
