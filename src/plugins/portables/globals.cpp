// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	globals.cpp
	Global variables.
*/

#include "precomp.h"
#include "wpdplug.h"
#include "globals.h"
#include "device.h"
#include "wpddeviceicons.h"

CSalamanderGeneralAbstract* SalamanderGeneral;
CSalamanderDebugAbstract* SalamanderDebug;
int SalamanderVersion;
CSalamanderGUIAbstract* SalamanderGUI;

CWpdPluginInterface g_oPlugin;

CWpdDeviceIcons g_oDeviceIcons;
CWpdDeviceList g_oDeviceList;
