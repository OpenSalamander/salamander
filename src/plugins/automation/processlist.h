// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	processlist.h
	Windows process list.
*/

#pragma once

// Returns TRUE when window 'hWnd' belongs to the process with 'dwProcessId'.
// The function walks through parent processes.
BOOL WindowBelongsToProcessID(HWND hWnd, DWORD dwProcessId);
