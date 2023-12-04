// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// SalmonClient
// Modul SALMON.EXE slouzi k out-of-process generovani minidumpu, jeho zabaleni a upload na server
// SALMON musi bezet od startu Salamandera, aby mohl na pady reagovat. Pady pred spusteni SALMON
// probehnou tise a SALMON je zpracuje "priste"
//
// tento header je sdileny mezi projekty SALMON a SALAMAND kvuli pameti, pres kterou komunikuji
//
// out of process minidumps
// http://www.nynaeve.net/?p=128
// http://social.msdn.microsoft.com/Forums/en-US/windbg/thread/2dfd711f-e81e-466f-a566-4605e78075f6
//
// http://www.voyce.com/index.php/2008/06/11/creating-a-featherweight-debugger/
// http://social.msdn.microsoft.com/Forums/en-US/windbg/thread/2dfd711f-e81e-466f-a566-4605e78075f6
// http://social.msdn.microsoft.com/Forums/en-US/vsdebug/thread/b290b7bd-1ec8-4302-8e3a-8ee0dc134683/
// http://www.ms-news.info/f3682/minidumpwritedump-fails-after-writing-partial-dump-access-denied-1843614.html
//
// debugging handles
// http://www.codeproject.com/Articles/6988/Debug-Tutorial-Part-5-Handle-Leaks

#define SALMON_FILEMAPPIN_NAME_SIZE 20

// x64 a x86 verze Salamander/Salmon nejsou kompatibilni
#ifdef _WIN64
#define SALMON_SHARED_MEMORY_VERSION_PLATFORM 0x10000000
#else
#define SALMON_SHARED_MEMORY_VERSION_PLATFORM 0x00000000
#endif
#define SALMON_SHARED_MEMORY_VERSION (SALMON_SHARED_MEMORY_VERSION_PLATFORM | 4)

#pragma pack(push)
#pragma pack(4)
struct CSalmonSharedMemory
{
    DWORD Version;           // SALMON_SHARED_MEMORY_VERSION (pokud nesouhlasi pro SALAM/SALMON, je rvat a nekomunikovat...)
    HANDLE Process;          // Handle parent procesu (abychom mohli cekat na jeho terminovani); tutu proto hodnotu nechame leakovat
    DWORD ProcessId;         // ID padleho parent procesu
    DWORD ThreadId;          // ID padleho threadu
    HANDLE Fire;             // AS signalizuje SALMONu, ze ma odeslat reporty
    HANDLE Done;             // SALMON vraci do AS, ze je hotovo
    HANDLE SetSLG;           // AS signalizuje SALMONu, ze ma nacist SLG podle bufferu SLGName, ktery pred nasetovani eventu nastavi
    HANDLE CheckBugs;        // AS signalizuje SALMONu, ze ma zkontrolvat adresar s bug reporty a pokud nejake najde (z nejakeho predesleho padu), nabidnout jejich upload
    char SLGName[MAX_PATH];  // ma vyznam ve chvili, kdy AS nasetuje SetSLG a rika, ktere SLG se ma nacist
    char BugPath[MAX_PATH];  // nastavuje Salamander, udava cestu kam budou padat bug reporty (cesta nemusi existovat, vytvari se az pri padu);
    char BugName[MAX_PATH];  // nastavuje Salamander, udava vnitrni nazev souboru minidumpu/bug reportu
    char BaseName[MAX_PATH]; // nastavuje salmon, jde o sestavu "UID-BugName-DATUM-CAS"; pro minidump ze za to pripoji ".DMP"
    DWORD64 UID;             // unikatni ID stroje, vytvari se xorem z GUID; uklada se v registry v Bug Reporter klici; nastavuje Salamander, salmon jen cte a vklada do nazvu bug reportu

    // predani EXCEPTION_POINTERS po jeho slozkach; nastavime pred nasetovanim eventu Fire
    EXCEPTION_RECORD ExceptionRecord;
    CONTEXT ContextRecord;
};
#pragma pack(pop)

//-----------------------------------------------------------------------
#ifdef INSIDE_SALAMANDER

BOOL SalmonInit();
void SalmonSetSLG(const char* slgName); // nastavi do salmon jazyk
void SalmonCheckBugs();

// ulozi do sdilene pameti info o exception a pozada salmon o vytvoreni minidumpu; potom ceka, az dobehne
// vraci TRUE v pripade uspechu, FALSE pokud se nepodarilo z nejakeho duvodu Salmon zavolat
BOOL SalmonFireAndWait(const EXCEPTION_POINTERS* e, char* bugReportPath);

#endif //INSIDE_SALAMANDER
