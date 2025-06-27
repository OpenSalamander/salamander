// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// interface pro komfortni praci se soubory
extern CSalamanderSafeFileAbstract* SalamanderSafeFile;

char* LoadStr(int resID);

char* LoadErr(int resID, DWORD LastError);
