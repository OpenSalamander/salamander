// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	globals.h
	Global variables.
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
extern class CWpdPluginInterface g_oPlugin;

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
