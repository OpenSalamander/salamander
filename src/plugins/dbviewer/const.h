// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// [0, 0] - for open viewer windows: the plugin configuration changed
#define WM_USER_VIEWERCFGCHNG WM_APP + 3246
// [0, 0] - for open viewer windows: the history needs to be cleared
#define WM_USER_CLEARHISTORY WM_APP + 3247
// [0, 0] - for open viewer windows: Salamander regenerated fonts, lists should call SetFont()
#define WM_USER_SETTINGCHANGE WM_APP + 3248

char* LoadStr(int resID);

// generic Salamander interface - valid from plugin start until shutdown
extern CSalamanderGeneralAbstract* SalGeneral;

extern HINSTANCE DLLInstance; // handle to SPL - language-independent resources
extern HINSTANCE HLanguage;   // handle to SLG - language-dependent resources

// Configuration variables
extern BOOL CfgUseCustomFont;
extern LOGFONT CfgLogFont;                 // description of the font used for the panel
extern BOOL CfgSavePosition;               // store the window position/place according to the main window
extern WINDOWPLACEMENT CfgWindowPlacement; // invalid if CfgSavePosition != TRUE
