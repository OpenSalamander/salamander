// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Pridat do DEBUG verze projektu makro _CRTDBG_MAP_ALLOC, jinak se neukazuje zdroj leaku.

#if defined(_DEBUG) && !defined(HEAP_DISABLE)

#define GCHEAP_MAX_USED_MODULES 100 // kolik nejvic modulu se ma pamatovat pro load pred vypisem leaku

// vola se pro moduly, ve kterych se muzou hlasit memory leaky, pokud se memory leaky detekuji,
// dojde k loadu "as image" (bez initu modulu) vsech takto registrovanych modulu (pri kontrole
// memory leaku uz jsou tyto moduly unloadle), a pak teprve k vypisu memory leaku = jsou videt
// jmena .cpp modulu misto "#File Error#" hlasek, zaroven MSVC neotravuje s hromadou generovanych
// exceptionu (jmena modulu jsou dostupna)
// mozne volat z libovolneho threadu
void AddModuleWithPossibleMemoryLeaks(const TCHAR* fileName);

#endif // defined(_DEBUG) && !defined(HEAP_DISABLE)
