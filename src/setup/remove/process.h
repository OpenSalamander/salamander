// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

BOOL EnumProcesses();                   // stores all running processes and their names
BOOL FindProcess(const char* fileName); // returns TRUE if a process with 'fileName' (full path) exists
void FreeProcesses();                   // frees allocated buffers containing process names
