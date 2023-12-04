// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

BOOL EnumProcesses();                   // stores all running precesses and stores names
BOOL FindProcess(const char* fileName); // returns TRUE if process with 'fileName' (fullpath) exists
void FreeProcesses();                   // frees allocated buffers (process names)
