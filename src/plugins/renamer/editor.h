// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern char Command[MAX_PATH];
extern char Arguments[MAX_PATH];
extern char InitDir[MAX_PATH];

extern CSalamanderVarStrEntry ExpCommandVariables[];
extern CSalamanderVarStrEntry ExpArgumentsVariables[];
extern CSalamanderVarStrEntry ExpInitDirVariables[];

BOOL ExecuteEditor(const char* tempFile);
