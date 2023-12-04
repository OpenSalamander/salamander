// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#pragma once

/**
	General Salamander services.
*/
extern CSalamanderGeneralAbstract* SalamanderGeneral;

/**
	Debug services, needed by "dbg.h".
*/
extern CSalamanderDebugAbstract* SalamanderDebug;

/**
	Salamander version.
*/
extern int SalamanderVersion;

/**
	GUI services.
*/
extern CSalamanderGUIAbstract* SalamanderGUI;

/**
	Basic plugin interface for Salamander.
*/
extern CNethoodPluginInterface g_oNethoodPlugin;

/**
	File-system interface for Salamander.
*/
extern CNethoodPluginInterfaceForFS g_oNethoodFS;

/**
	Name of the filesystem assigned to us by Salamander
	(need not to be the same as suggested name).
*/
extern TCHAR g_szAssignedFSName[MAX_PATH];
extern size_t g_cchAssignedFSName;

/**
	Module handle.
*/
extern HINSTANCE g_hInstance;

/**
	 Language resources module handle.
*/
extern HINSTANCE g_hLangInstance;

// handle to the image list containing simple file icons
extern HIMAGELIST g_himlSimpleIcons;

// global Salamander variables shared between FS and archiver
extern const CFileData** g_transferFileData;
extern int* g_transferIsDir;
extern char* g_transferBuffer;
extern int* g_transferLen;
extern DWORD* g_transferRowData;
extern CPluginDataInterfaceAbstract** g_transferPluginDataIface;
extern DWORD* g_transferActCustomData;

extern TCHAR g_aszRedirectPath[2][MAX_PATH];

#define MAX_SHARE_NAME 16
extern TCHAR g_aszFocusShareName[2][MAX_SHARE_NAME];
extern int g_iFocusSharePanel;

#define POSTED_THROBBER_QUEUE_LEN 8
extern DWORD_PTR g_adwPostedThrobberQueue[POSTED_THROBBER_QUEUE_LEN];
extern int g_iPostedThrobberQueue;

//CNethoodIcons		g_oIcons;

/**
	Sets module handle of the language resources.
	
	\param hInst Language resources module handle.
*/
static inline void SetLangInstance(HINSTANCE hInst)
{
    extern HINSTANCE g_hLangInstance;
    g_hLangInstance = hInst;
}

/**
	Retrieves module handle of the language resources.
	
	\return The return value is handle to the language resources module.
*/
static inline HINSTANCE GetLangInstance()
{
    extern HINSTANCE g_hLangInstance;
    return g_hLangInstance;
}

#define MENUCMD_REDIRECT_BASE 1

#define MENUCMD_REDIRECT_LEFT (MENUCMD_REDIRECT_BASE + 0)
#define MENUCMD_REDIRECT_RIGHT (MENUCMD_REDIRECT_BASE + 1)
#define MENUCMD_REDIRECT_LAST MENUCMD_REDIRECT_RIGHT

#define MENUCMD_START_THROBBER (MENUCMD_REDIRECT_LAST + 1)
#define MENUCMD_STOP_THROBBER (MENUCMD_REDIRECT_LAST + 2) // not used right now, reserved for the future

#define MENUCMD_FOCUS_SHARE_BASE (MENUCMD_REDIRECT_LAST + 3)
#define MENUCMD_FOCUS_SHARE_LEFT (MENUCMD_FOCUS_SHARE_BASE + 0)
#define MENUCMD_FOCUS_SHARE_RIGHT (MENUCMD_FOCUS_SHARE_BASE + 1)
#define MENUCMD_FOCUS_SHARE_LAST MENUCMD_FOCUS_SHARE_RIGHT

/**
	Period before the throbber starts to spin, in milliseconds.
*/
#define THROBBER_GRACE_PERIOD 1000

/**
	Period before the throbber starts to spin after force
	refresh (Ctrl+R), in milliseconds. This is set to small non-zero
	value to prevent flashing of the throbber when the enumeration
	finishes quickly (the throbber won't be ever shown in such case).
*/
#define THROBBER_GRACE_PERIOD_IMMEDIATE 100
