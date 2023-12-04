// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// struktura predavana do upload vlakna, slouzi pro presun vstupne/vystupnich parametru
struct CUploadParams
{
    BOOL Result;                     // TRUE, pokud operace dobehla uspesne, jinak FALSE
    char ErrorMessage[2 * MAX_PATH]; // pokud je Result FALSE, obsahuje popis chyby
    char FileName[MAX_PATH];         // plna cesta k souboru, ktery se ma uploadnout
};

BOOL StartUploadThread(CUploadParams* params);
BOOL IsUploadThreadRunning();