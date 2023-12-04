// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// struktura predavana do compress vlakna, slouzi pro presun vstupne/vystupnich parametru
struct CMinidumpParams
{
    BOOL Result;                     // TRUE, pokud operace dobehla uspesne, jinak FALSE
    char ErrorMessage[2 * MAX_PATH]; // pokud je Result FALSE, obsahuje popis chyby
};

BOOL StartMinidumpThread(CMinidumpParams* params);
BOOL IsMinidumpThreadRunning();