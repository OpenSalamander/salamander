// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// general Salamander interface - valid from plugin start until its termination
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// interface for convenient work with files
extern CSalamanderSafeFileAbstract* SalamanderSafeFile;

char* LoadStr(int resID);

char* LoadErr(int resID, DWORD LastError);
