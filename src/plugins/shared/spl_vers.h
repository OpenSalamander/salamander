// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

// WARNING: cannot be replaced by "#pragma once" because it is included from .rc file and it seems resource compiler does not support "#pragma once"
#ifndef __SPL_VERS_H
#define __SPL_VERS_H

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

// conversion macros num->str
#define VERSINFO_xstr(s) VERSINFO_str(s)
#define VERSINFO_str(s) #s

#define VERSINFO_SALAMANDER_MAJOR 5
#define VERSINFO_SALAMANDER_MINORA 0
#define VERSINFO_SALAMANDER_MINORB 0

#if (VERSINFO_SALAMANDER_MINORB == 0) // nulu na setinach nepiseme 2.50 -> 2.5
#define VERSINFO_SALAMANDER_VERSION VERSINFO_xstr(VERSINFO_SALAMANDER_MAJOR) "." VERSINFO_xstr(VERSINFO_SALAMANDER_MINORA) VERSINFO_BETAVERSION_TXT
#define VERSINFO_SAL_SHORT_VERSION VERSINFO_xstr(VERSINFO_SALAMANDER_MAJOR) VERSINFO_xstr(VERSINFO_SALAMANDER_MINORA) VERSINFO_BETAVERSIONSHORT_TXT
#else
#define VERSINFO_SALAMANDER_VERSION VERSINFO_xstr(VERSINFO_SALAMANDER_MAJOR) "." VERSINFO_xstr(VERSINFO_SALAMANDER_MINORA) VERSINFO_xstr(VERSINFO_SALAMANDER_MINORB) VERSINFO_BETAVERSION_TXT
#define VERSINFO_SAL_SHORT_VERSION VERSINFO_xstr(VERSINFO_SALAMANDER_MAJOR) VERSINFO_xstr(VERSINFO_SALAMANDER_MINORA) VERSINFO_xstr(VERSINFO_SALAMANDER_MINORB) VERSINFO_BETAVERSIONSHORT_TXT
#endif

#ifdef VERSINFO_MAJOR      // je definovane jen pokud se pouziva z pluginu
#if (VERSINFO_MINORB == 0) // nulu na setinach nepiseme 2.50 -> 2.5
#define VERSINFO_VERSION VERSINFO_xstr(VERSINFO_MAJOR) "." VERSINFO_xstr(VERSINFO_MINORA) VERSINFO_BETAVERSION_TXT
#define VERSINFO_VERSION_NO_PLATFORM VERSINFO_xstr(VERSINFO_MAJOR) "." VERSINFO_xstr(VERSINFO_MINORA) VERSINFO_BETAVERSION_TXT_NO_PLATFORM
#else
#define VERSINFO_VERSION VERSINFO_xstr(VERSINFO_MAJOR) "." VERSINFO_xstr(VERSINFO_MINORA) VERSINFO_xstr(VERSINFO_MINORB) VERSINFO_BETAVERSION_TXT
#define VERSINFO_VERSION_NO_PLATFORM VERSINFO_xstr(VERSINFO_MAJOR) "." VERSINFO_xstr(VERSINFO_MINORA) VERSINFO_xstr(VERSINFO_MINORB) VERSINFO_BETAVERSION_TXT_NO_PLATFORM
#endif
#endif

#ifdef _WIN64
#define SAL_VER_PLATFORM "x64"
#else // _WIN64
#define SAL_VER_PLATFORM "x86"
#endif // _WIN64

// VERSINFO_BUILDNUMBER:
//
// Slouzi ke snadnemu odliseni verzi vsech modulu mezi jednotlivymi verzemi
// Salamandera (jde o posledni komponentu cisla verze vsech pluginu a
// Salamandera). Zvysovat s kazdou verzi (IB, DB, PB, beta, release nebo i
// jen testovaci verze poslana jednomu uzivateli). Prehled ruznych typu verzi
// je v souboru doc\versions.txt. Vzdy zavest komentar s popisem, ke ktere
// verzi Salamandera patri nove pouzite cislo buildu.
//
// Prehled pouzitych hodnot VERSINFO_BUILDNUMBER:
// 9 - 2.5 beta 9
// 10 - 2.5 beta 10
// 11 - 2.5 beta 11
// 13 - 2.5 RC1
// 14 - 2.5 RC2
// 15 - 2.5 RC3
// 0 - 2.5
// 16 - 2.51
// 18 - 2.52 beta 1
// 29 - 2.52 beta 2
// 32 - 2.52
// 49 - 2.53 beta 1
// 57 - 2.53 beta 2
// 63 - 2.53
// 69 - 2.54
// 91 - 3.0 beta 1
// 97 - 3.0 beta 2
// 108 - 3.0 beta 3
// 114 - 3.0 beta 4
// 120 - 3.0
// 126 - 3.01
// 132 - 3.02
// 138 - 3.03
// 144 - 3.04
// 150 - 3.05
// 156 - 3.06
// 165 - 3.07
// 174 - 3.08
// 175 - 3.08 (SDK)
// 176 - 3.08 (CB176)
// 177 - 4.0 beta 1 (DB177)
// 178 - 4.0 beta 1 (CB178)
// 179 - 4.0 beta 1 (IB179)
// 180 - 4.0
// 181 - 4.0 (SDK)
// 182 - 4.0 (CB182)
// 183 - 5.0

// ! DULEZITE: nova cisla buildu je nutne zapsat do vetve "default", a pak
//             teprve do vedlejsi vetve (kompletni seznam je jen v "default" vetvi)
#define VERSINFO_BUILDNUMBER 183

// VERSINFO_BETAVERSION_TXT:
//
// Meni se s kazdym buildem, v pripade release verze bude VERSINFO_BETAVERSION_TXT="".
// Pokud vydavame specialni opravne beta verze typu 2.5 beta 9a, zvysime
// VERSINFO_BUILDNUMBER o jedna a dame VERSINFO_BETAVERSION_TXT==" beta 9a".
//
// VERSINFO_BETAVERSIONSHORT_TXT slouzi pro pojmenovani bug reportu, jde o co nejkratsi zapis

// priklady ("x86" je pro 32-bit verzi, "x64" pro 64-bit verzi, v nasledujicich prikladech jsou
// x86/x64 zamenne): " (x86)" (pro release verze), " beta 2 (x64)", " beta 2 (SDK x86)",
// " RC1 (x64)", " beta 2 (IB21 x86)", " beta 2 (DB21 x64)", " beta 2 (PB21 x86)"
#define VERSINFO_BETAVERSION_TXT " (" SAL_VER_PLATFORM ")"
#define VERSINFO_BETAVERSION_TXT_NO_PLATFORM "" // kopie radku vyse + smazat SAL_VER_PLATFORM + je-li zavorka prazdna, smazat ji + smazat nadbytecne mezery

// priklady (x86/x64 viz predchozi odstavec): "x86" (pro release verze), "B2x64", "B2SDKx86",
// "RC1x64", "B2IB21x86", "B2DB21x64", "B2PB21x86"
#define VERSINFO_BETAVERSIONSHORT_TXT SAL_VER_PLATFORM

// LAST_VERSION_OF_SALAMANDER:
//
// Podpora pro kontrolu aktualnosti verze Salamandera, kterou interni pluginy
// (distribuovane v jednom balicku se Salamanderem) provadi behem entry-pointu
// (SalamanderPluginEntry) viz metoda CSalamanderPluginEntryAbstract::GetVersion()
// (v spl_base.h). Slouzi hlavne pro jednoduchost: interni plugin muze volat
// jakoukoliv metodu z rozhrani Salamandera, protoze po kontrole na posledni
// verzi Salamandera ma jistotu, ze ji Salamander obsahuje (hrozi mu jen load
// do novejsi verze Salamandera, ktery tyto metody musi tez obsahovat).
//
// Pouziva se i opacne: aby mel interni plugin jistotu, ze mu Salamander bude
// volat vsechny metody (vcetne nejnovejsich), vraci tuto verzi, jako verzi,
// pro kterou byl plugin postaven (viz export pluginu SalamanderPluginGetReqVer).
//
// Pokud nektery plugin vraci z SalamanderPluginGetReqVer nizsi verzi nez
// LAST_VERSION_OF_SALAMANDER (pro zpetnou kompatibilitu se starsimi verzemi
// Salamandera), mel by pridat export SalamanderPluginGetSDKVer a vracet z nej
// LAST_VERSION_OF_SALAMANDER (verze SDK pouzita pro stavbu pluginu), aby mohl
// Salamander (napr. aktualni nebo novejsi) pouzivat i metody pluginu, ktere
// ve verzi vracene z SalamanderPluginGetReqVer jeste nebyly.
//
// Pri zmenach v rozhrani je potreba dodrzet postup uvedeny v doc\how_to_change.txt.
//
// Prehled pouzitych hodnot LAST_VERSION_OF_SALAMANDER:
//   1  - 1.6 beta 4 + 5
//   2  - 1.6 beta 6
//   3  - 1.6 beta 7
//   4  - 2.0
//   5  - 2.5 beta 1
//   6  - 2.5 beta 2
//   7  - 2.5 beta 3
//   8  - 2.5 beta 4
//   9  - 2.5 beta 5
//   10 - 2.5 beta 6
//   11 - 2.5 beta 7
//   12 - 2.5 beta 8
//   13 - 2.5 beta 9
//   14 - 2.5 beta 10
//   15 - 2.5 beta 10a
//   16 - 2.5 beta 11
//   17 - 2.5 beta 12 (jen interni, pustili jsme misto ni RC1)
//   18 - 2.5 RC1
//   19 - 2.5 RC2
//   20 - 2.5 RC3
//   21 - 2.5
//   22 - 2.51
//   23 - 2.52 beta 1 (POZOR: nekompatibilni SDK s predchozimi a dalsimi verzemi)
//   29 - 2.52 beta 2
//   31 - 2.52
//   39 - 2.53 beta 1 + 2.53 beta 1a
//   41 - 2.53 beta 2
//   43 - 2.53
//   45 - 2.54
//   54 - 3.0 beta 1
//   56 - 3.0 beta 2
//   60 - 3.0 beta 3
//   62 - 3.0 beta 4
//   64 - 3.0
//   66 - 3.01
//   68 - 3.02
//   70 - 3.03
//   72 - 3.04
//   74 - 3.05
//   76 - 3.06
//   79 - 3.07
//   81 - 3.08
// ! DULEZITE: vsechny verze z VC2008 musi byt < 100, vsechny verze z VC2019 musi byt >= 100,
//             nova cisla verzi je nutne zapsat do vetve "default", a pak
//             teprve do vedlejsi vetve (kompletni seznam je jen v "default" vetvi)
//   101 - 4.0 beta 1 (DB177)
//   102 - 4.0
//   103 - 5.0

#define LAST_VERSION_OF_SALAMANDER 103
#define REQUIRE_LAST_VERSION_OF_SALAMANDER "This plugin requires Open Salamander 5.0 (" SAL_VER_PLATFORM ") or later."

#endif // __SPL_VERS_H
