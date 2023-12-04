// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// popis viz shared/spl_gen.h
// proti funkci v Salamanderu ma tato vykuchan parametr nextFocus
// a podporu pro DefaultDir (cesty typu "c:path...")
BOOL SalGetFullName(LPTSTR name, int* errTextID, LPCTSTR curDir);
