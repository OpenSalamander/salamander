// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// OLESPY slouzi pro detekci COM/OLE leaku
// tela nasledujicich funkci jsou prazdna, pokud se jedna o release verzi
// (neni definovano _DEFINE)
//
// Podrobnosti k hledani OLE leaku jsou popsany v OLESPY.CPP

// Pripojeni naseho IMallocSpy k OLE; napred je treba inicializova COM
// Pokud vrati TRUE, je mozne volat nasledujici funkce
BOOL OleSpyRegister();

// Odpojeni SPYe od OLE; po teto funkci lze jeste zavolat OleSpyDump
void OleSpyRevoke();

// Slouzi k breknuti aplikace pri dosazeni 'alloc' alokace
// Zavolat nekdy od OleSpyRegister do OleSpyRevoke
void OleSpySetBreak(int alloc);

// Vypise do Debug okna debuggeru a do TRACE_I statistiky a leaky
// U leaku zobrazuje v [n] poradi alokaci, ktere lze vyuzit pro OleSpySetBreak
void OleSpyDump();

// stress test implementace IMallocSpy
// urceno pro ladici ucely
// void OleSpyStressTest();
