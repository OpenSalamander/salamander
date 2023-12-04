// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// [0, 0] - pro otevrena okna viewru: konfigurace pluginu se zmenila
#define WM_USER_VIEWERCFGCHNG WM_APP + 3246
// [0, 0] - pro otevrena okna viewru: je treba podriznou historie
#define WM_USER_CLEARHISTORY WM_APP + 3247
// [0, 0] - pro otevrena okna vieweru: Salamander pregeneroval fonty, mame zavolat SetFont() listam
#define WM_USER_SETTINGCHANGE WM_APP + 3248

char* LoadStr(int resID);

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalGeneral;

extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy

// Configuration variables
extern BOOL CfgUseCustomFont;
extern LOGFONT CfgLogFont;                 // popis fontu pouzivaneho pro panel
extern BOOL CfgSavePosition;               // ukladat pozici okna/umistit dle hlavniho okna
extern WINDOWPLACEMENT CfgWindowPlacement; // neplatne, pokud CfgSavePosition != TRUE
