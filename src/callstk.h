// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// makro CALLSTK_DISABLE - odstavi kompletne tenhle modul
// makro CALLSTK_MEASURETIMES - zapne mereni casu straveneho pri priprave call-stack hlaseni (meri se pomer proti
//                              celkovemu casu behu funkci)
//                              POZOR: nutne zapnout tez pro kazdy plugin zvlast
// makro CALLSTK_DISABLEMEASURETIMES - potlaci mereni casu straveneho pri priprave call-stack hlaseni v DEBUG verzi

// prehled typu maker: (vsechna jsou neprazdna jen pokud neni definovano CALLSTK_DISABLE)
// CALL_STACK_MESSAGE - bezne call-stack makro
// SLOW_CALL_STACK_MESSAGE - call-stack makro, u ktereho se ignoruje libovolne zpomaleni kodu (pouziti
//                           na mistech, kde vime, ze vyrazne zpomaluje kod, ale presto ho na tomto
//                           miste potrebujeme)
// DEBUG_SLOW_CALL_STACK_MESSAGE - call-stack makro, ktere je v release verzi prazdne, v DEBUG verzi
//                                 se chova jako SLOW_CALL_STACK_MESSAGE (pouziti na mistech, kde jsme
//                                 ochotni ignorovat zpomaleni jen v debug verzi, release verze je rychla)

//
// ****************************************************************************

// objekt drzi seznam volanych funkci (call-stack) - radky se pridavaji pres CCallStackMessage

typedef void (*FPrintLine)(void* param, const char* txt, BOOL tab);

BOOL StartSalmonProcess(BOOL enableRestartAS);

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
#define CALLSTACK_MONITORINGPERIOD 100    // v milisekundach: jak dlouho se ma sledovat kolikrat se volalo jedno call-stack makro
#define CALLSTACK_MONITOREDITEMS_BASE 30  // na kolik polozek se alokuje fronta sledovanych volani call-stack makra
#define CALLSTACK_MONITOREDITEMS_DELTA 20 // po kolika polozkach se zvetsuje fronta sledovanych volani call-stack makra
struct CCallStackMonitoredItem
{
    __int64 MonitoringStartPerfTime; // odkdy sledujeme toto volani call-stack makra
    DWORD_PTR CallerAddress;         // adresa volani call-stack makra
    DWORD NumberOfCalls;             // pocet volani behem poslednich CALLSTACK_TRACETIME milisekund
    __int64 PushesPerfTime;          // soucet "casu" Pushu tohoto call-stack makra
    BOOL NotAlreadyReported;         // TRUE = problem jeste nebyl ohlasen (prevence proti zbytecnemu plneni Trace Serveru)
};
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

class CCallStack
{
#ifndef CALLSTK_DISABLE
protected:
    DWORD ThreadID;                  // ID aktualniho threadu
    HANDLE ThreadHandle;             // handle aktualniho threadu, pouzivame v CCallStack::PrintBugReport pro GetThreadContext
    char Text[STACK_CALLS_BUF_SIZE]; // double-null-terminated list + za koncovou nulou je vzdy delka predchoziho stringu (na dvou bytech), nasleduje hned dalsi string
    char* End;                       // ukazatel na posledni dve nuly
    int Skipped;                     // pocet messages, ktere nesly ulozit
    char* Enum;                      // ukazatel na posledni vypsany text
    BOOL FirstCallstack;             // jsme prvni instance?

    const char* PluginDLLName; // DLL plug-inu, kde thread prave bezi (NULL jde-li o salamand.exe)
    int PluginDLLNameUses;     // kolikata Pop() operace ma PluginDLLName NULLovat (uroven vnoreni)

    static DWORD TlsIndex;                        // thread-local-storage index, na kterem je vzdy 'this'
    static BOOL SectionOK;                        // je sekce inicializovana?
    static CRITICAL_SECTION Section;              // kriticka sekce pro pristup k CallStacks
    static TIndirectArray<CCallStack> CallStacks; // pole vsech call-stacku
    static BOOL ExceptionExists;                  // zpracovava se uz nejaka exceptiona? (mame se suspendnout?)

    BOOL DontSuspend; // oznaceni threadu, ktery ukazuje bug-report okna

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
#define CALLSTK_MONITOR_SHOWINFORATIO 20     // minimalni pomer realneho casu proti casu stravenemu pri ukladani zpravy na call-stack (vetsi = pise TRACE_I)
#define CALLSTK_MONITOR_SHOWERRORRATIO 10    // minimalni pomer realneho casu proti casu stravenemu pri ukladani zpravy na call-stack (vetsi = pise TRACE_E)
    CCallStackMonitoredItem* MonitoredItems; // kruhova fronta sledovanych volani call-stack maker
    int MonitoredItemsSize;                  // aktualni velikost fronty MonitoredItems
    int NewestItemIndex;                     // index nejnovejsi polozky ve fronte MonitoredItems; -1 = fronta je prazdna
    int OldestItemIndex;                     // index nejstarsi polozky ve fronte MonitoredItems; -1 = fronta je prazdna

public:
    DWORD PushesCounter;                      // pocitadlo Pushu volanych v tomto objektu (aneb v jednom threadu)
    LARGE_INTEGER PushPerfTimeCounter;        // soucet casu straveneho v Push volanych tomto objektu (aneb v jednom threadu)
    LARGE_INTEGER IgnoredPushPerfTimeCounter; // soucet casu straveneho v nemerenych (ignorovanych) Push volanych tomto objektu (aneb v jednom threadu)
    static LARGE_INTEGER SavedPerfFreq;       // nenulovy vysledek QueryPerformanceFrequency
#define CALLSTK_BENCHMARKTIME 100             // cas v milisekundach, po ktery se meri rychlost call-stacku
    static DWORD SpeedBenchmark;              // kolik merenych call-stack maker se stihne pushnout+popnout za CALLSTK_BENCHMARKTIME milisekund
#endif                                        // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

public:
    CCallStack(BOOL dontSuspend = FALSE);
    ~CCallStack();

    static CCallStack* GetThis()
    {
        return (CCallStack*)TlsGetValue(TlsIndex);
    }

    void Push(const char* format, va_list args);

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    void Pop(BOOL printCallStackTop);
    void CheckCallFrequency(DWORD_PTR callerAddress, LARGE_INTEGER* pushTime, LARGE_INTEGER* afterPushTime);
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    void Pop();
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

    // vola se jen pri ukladani call-stack-message z plug-inu
    void PushPluginDLLName(const char* dllName)
    {
        if (PluginDLLName == NULL)
        {
            PluginDLLName = dllName;
            PluginDLLNameUses = 1;
        }
        else
            PluginDLLNameUses++;
    }

    // vola se jen pri vybirani call-stack-message z plug-inu
    void PopPluginDLLName()
    {
        if (PluginDLLNameUses <= 1)
        {
            PluginDLLName = NULL;
            PluginDLLNameUses = 0;
        }
        else
            PluginDLLNameUses--;
    }

    const char* GetPluginDLLName() { return PluginDLLName; }

    void Reset() // zacatek enumerace radek
    {
        Enum = Text;
    }

    const char* GetNextLine(); // vrati bud dalsi radku nebo NULL, pokud jiz zadna neni

    static void ReleaseBeforeExitThread(); // uvolni data call-stack objektu v aktualnim threadu (pouziva se pred vyvolanim exitu threadu uvnitr hlidane oblasti)
    void ReleaseBeforeExitThreadBody();    // vola se z ReleaseBeforeExitThread() po detekci objektu call-stacku z TLS

    static int HandleException(EXCEPTION_POINTERS* e, DWORD shellExtCrashID = -1,
                               const char* iconOvrlsHanName = NULL); // volano z exception handleru
    static DWORD WINAPI ThreadBugReportF(void* exitProcess);         // thread, ve kterem se otevre dialog bug-reportu
    // zavola PrintBugReport do bug reportu
    static BOOL CreateBugReportFile(EXCEPTION_POINTERS* Exception, DWORD ThreadID, DWORD ShellExtCrashID, const char* bugReportFileName);

    // funkce pro tisk informaci o vyjimce (pouziva se jak do souboru, tak do okna)
    // je-li Exception==NULL nejde o vyjimku (user otevrel Bug Report dialog)
    static void PrintBugReport(EXCEPTION_POINTERS* Exception, DWORD ThreadID, DWORD ShellExtCrashID,
                               FPrintLine PrintLine, void* param);
#endif // CALLSTK_DISABLE
};

// uklada a pri odchodu z bloku vybira zpravu z call-stacku, parametry shodne s printf funkcemi

#ifndef CALLSTK_DISABLE

class CCallStackMessage
{
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
public:
#define CALLSTK_MINWARNTIME 200                    // minimalni cas v milisekundach mezi Push a Pop, aby doslo ke hlaseni problemu
#define CALLSTK_MINRATIO 10                        // minimalni povoleny pomer realneho casu proti casu stravenemu pri ukladani zprav na call-stack (10 = z celkoveho casu behu bylo straveno mene nez 1/10 pri generovani call-stack, jinak zarve)
    DWORD PushesCounterStart;                      // start-stav pocitadla Pushu volanych v tomto threadu
    LARGE_INTEGER PushPerfTimeCounterStart;        // start-stav pocitadla casu straveneho v metodach Push volanych v tomto threadu
    LARGE_INTEGER IgnoredPushPerfTimeCounterStart; // start-stav pocitadla casu straveneho v nemerenych (ignorovanych) metodach Push volanych v tomto threadu
    LARGE_INTEGER StartTime;                       // "cas" Pushe tohoto call-stack makra
    DWORD_PTR PushCallerAddress;                   // adresa makra CALL_STACK_MESSAGE (adresa Pushnuti)
#endif                                             // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

public:
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    // 'doNotMeasureTimes'==TRUE = nemerit Push tohoto call-stack makra (zrejme dost zpomaluje, ale
    // nechceme ho vyhodit, je prilis dulezite pro debugovani)
    CCallStackMessage(BOOL doNotMeasureTimes, int dummy, const char* format, ...);
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
    CCallStackMessage(const char* format, ...);
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

    ~CCallStackMessage();
};

#define CALLSTK_TOKEN(x, y) x##y
#define CALLSTK_TOKEN2(x, y) CALLSTK_TOKEN(x, y)
#define CALLSTK_UNIQUE(varname) CALLSTK_TOKEN2(varname, __COUNTER__)

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

extern BOOL __CallStk_T; // always TRUE - just to check format string and type of parameters of call-stack macros

#define CALL_STACK_MESSAGE1(p1) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, 0, p1)
#define CALL_STACK_MESSAGE2(p1, p2) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2)), p1, p2)
#define CALL_STACK_MESSAGE3(p1, p2, p3) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3)), p1, p2, p3)
#define CALL_STACK_MESSAGE4(p1, p2, p3, p4) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4)), p1, p2, p3, p4)
#define CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5)), p1, p2, p3, p4, p5)
#define CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6)), p1, p2, p3, p4, p5, p6)
#define CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7)), p1, p2, p3, p4, p5, p6, p7)
#define CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8)), p1, p2, p3, p4, p5, p6, p7, p8)
#define CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9)), p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) CCallStackMessage CALLSTK_UNIQUE(_m)(FALSE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#define SLOW_CALL_STACK_MESSAGE1(p1) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, 0, p1)
#define SLOW_CALL_STACK_MESSAGE2(p1, p2) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2)), p1, p2)
#define SLOW_CALL_STACK_MESSAGE3(p1, p2, p3) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3)), p1, p2, p3)
#define SLOW_CALL_STACK_MESSAGE4(p1, p2, p3, p4) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4)), p1, p2, p3, p4)
#define SLOW_CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5)), p1, p2, p3, p4, p5)
#define SLOW_CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6)), p1, p2, p3, p4, p5, p6)
#define SLOW_CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7)), p1, p2, p3, p4, p5, p6, p7)
#define SLOW_CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8)), p1, p2, p3, p4, p5, p6, p7, p8)
#define SLOW_CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9)), p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define SLOW_CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define SLOW_CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define SLOW_CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define SLOW_CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define SLOW_CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define SLOW_CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define SLOW_CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define SLOW_CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define SLOW_CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define SLOW_CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define SLOW_CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define SLOW_CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) CCallStackMessage CALLSTK_UNIQUE(_m)(TRUE, (__CallStk_T ? 0 : printf(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)), p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#else // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

#define CALL_STACK_MESSAGE1(p1) CCallStackMessage CALLSTK_UNIQUE(_m)(p1)
#define CALL_STACK_MESSAGE2(p1, p2) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2)
#define CALL_STACK_MESSAGE3(p1, p2, p3) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3)
#define CALL_STACK_MESSAGE4(p1, p2, p3, p4) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4)
#define CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5)
#define CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6)
#define CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7)
#define CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8)
#define CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) CCallStackMessage CALLSTK_UNIQUE(_m)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#define SLOW_CALL_STACK_MESSAGE1 CALL_STACK_MESSAGE1
#define SLOW_CALL_STACK_MESSAGE2 CALL_STACK_MESSAGE2
#define SLOW_CALL_STACK_MESSAGE3 CALL_STACK_MESSAGE3
#define SLOW_CALL_STACK_MESSAGE4 CALL_STACK_MESSAGE4
#define SLOW_CALL_STACK_MESSAGE5 CALL_STACK_MESSAGE5
#define SLOW_CALL_STACK_MESSAGE6 CALL_STACK_MESSAGE6
#define SLOW_CALL_STACK_MESSAGE7 CALL_STACK_MESSAGE7
#define SLOW_CALL_STACK_MESSAGE8 CALL_STACK_MESSAGE8
#define SLOW_CALL_STACK_MESSAGE9 CALL_STACK_MESSAGE9
#define SLOW_CALL_STACK_MESSAGE10 CALL_STACK_MESSAGE10
#define SLOW_CALL_STACK_MESSAGE11 CALL_STACK_MESSAGE11
#define SLOW_CALL_STACK_MESSAGE12 CALL_STACK_MESSAGE12
#define SLOW_CALL_STACK_MESSAGE13 CALL_STACK_MESSAGE13
#define SLOW_CALL_STACK_MESSAGE14 CALL_STACK_MESSAGE14
#define SLOW_CALL_STACK_MESSAGE15 CALL_STACK_MESSAGE15
#define SLOW_CALL_STACK_MESSAGE16 CALL_STACK_MESSAGE16
#define SLOW_CALL_STACK_MESSAGE17 CALL_STACK_MESSAGE17
#define SLOW_CALL_STACK_MESSAGE18 CALL_STACK_MESSAGE18
#define SLOW_CALL_STACK_MESSAGE19 CALL_STACK_MESSAGE19
#define SLOW_CALL_STACK_MESSAGE20 CALL_STACK_MESSAGE20
#define SLOW_CALL_STACK_MESSAGE21 CALL_STACK_MESSAGE21

#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

#else // CALLSTK_DISABLE

#define CALL_STACK_MESSAGE1(p1)
#define CALL_STACK_MESSAGE2(p1, p2)
#define CALL_STACK_MESSAGE3(p1, p2, p3)
#define CALL_STACK_MESSAGE4(p1, p2, p3, p4)
#define CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5)
#define CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6)
#define CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7)
#define CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8)
#define CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#define SLOW_CALL_STACK_MESSAGE1 CALL_STACK_MESSAGE1
#define SLOW_CALL_STACK_MESSAGE2 CALL_STACK_MESSAGE2
#define SLOW_CALL_STACK_MESSAGE3 CALL_STACK_MESSAGE3
#define SLOW_CALL_STACK_MESSAGE4 CALL_STACK_MESSAGE4
#define SLOW_CALL_STACK_MESSAGE5 CALL_STACK_MESSAGE5
#define SLOW_CALL_STACK_MESSAGE6 CALL_STACK_MESSAGE6
#define SLOW_CALL_STACK_MESSAGE7 CALL_STACK_MESSAGE7
#define SLOW_CALL_STACK_MESSAGE8 CALL_STACK_MESSAGE8
#define SLOW_CALL_STACK_MESSAGE9 CALL_STACK_MESSAGE9
#define SLOW_CALL_STACK_MESSAGE10 CALL_STACK_MESSAGE10
#define SLOW_CALL_STACK_MESSAGE11 CALL_STACK_MESSAGE11
#define SLOW_CALL_STACK_MESSAGE12 CALL_STACK_MESSAGE12
#define SLOW_CALL_STACK_MESSAGE13 CALL_STACK_MESSAGE13
#define SLOW_CALL_STACK_MESSAGE14 CALL_STACK_MESSAGE14
#define SLOW_CALL_STACK_MESSAGE15 CALL_STACK_MESSAGE15
#define SLOW_CALL_STACK_MESSAGE16 CALL_STACK_MESSAGE16
#define SLOW_CALL_STACK_MESSAGE17 CALL_STACK_MESSAGE17
#define SLOW_CALL_STACK_MESSAGE18 CALL_STACK_MESSAGE18
#define SLOW_CALL_STACK_MESSAGE19 CALL_STACK_MESSAGE19
#define SLOW_CALL_STACK_MESSAGE20 CALL_STACK_MESSAGE20
#define SLOW_CALL_STACK_MESSAGE21 CALL_STACK_MESSAGE21

#endif // CALLSTK_DISABLE

#ifdef _DEBUG

#define DEBUG_SLOW_CALL_STACK_MESSAGE1 SLOW_CALL_STACK_MESSAGE1
#define DEBUG_SLOW_CALL_STACK_MESSAGE2 SLOW_CALL_STACK_MESSAGE2
#define DEBUG_SLOW_CALL_STACK_MESSAGE3 SLOW_CALL_STACK_MESSAGE3
#define DEBUG_SLOW_CALL_STACK_MESSAGE4 SLOW_CALL_STACK_MESSAGE4
#define DEBUG_SLOW_CALL_STACK_MESSAGE5 SLOW_CALL_STACK_MESSAGE5
#define DEBUG_SLOW_CALL_STACK_MESSAGE6 SLOW_CALL_STACK_MESSAGE6
#define DEBUG_SLOW_CALL_STACK_MESSAGE7 SLOW_CALL_STACK_MESSAGE7
#define DEBUG_SLOW_CALL_STACK_MESSAGE8 SLOW_CALL_STACK_MESSAGE8
#define DEBUG_SLOW_CALL_STACK_MESSAGE9 SLOW_CALL_STACK_MESSAGE9
#define DEBUG_SLOW_CALL_STACK_MESSAGE10 SLOW_CALL_STACK_MESSAGE10
#define DEBUG_SLOW_CALL_STACK_MESSAGE11 SLOW_CALL_STACK_MESSAGE11
#define DEBUG_SLOW_CALL_STACK_MESSAGE12 SLOW_CALL_STACK_MESSAGE12
#define DEBUG_SLOW_CALL_STACK_MESSAGE13 SLOW_CALL_STACK_MESSAGE13
#define DEBUG_SLOW_CALL_STACK_MESSAGE14 SLOW_CALL_STACK_MESSAGE14
#define DEBUG_SLOW_CALL_STACK_MESSAGE15 SLOW_CALL_STACK_MESSAGE15
#define DEBUG_SLOW_CALL_STACK_MESSAGE16 SLOW_CALL_STACK_MESSAGE16
#define DEBUG_SLOW_CALL_STACK_MESSAGE17 SLOW_CALL_STACK_MESSAGE17
#define DEBUG_SLOW_CALL_STACK_MESSAGE18 SLOW_CALL_STACK_MESSAGE18
#define DEBUG_SLOW_CALL_STACK_MESSAGE19 SLOW_CALL_STACK_MESSAGE19
#define DEBUG_SLOW_CALL_STACK_MESSAGE20 SLOW_CALL_STACK_MESSAGE20
#define DEBUG_SLOW_CALL_STACK_MESSAGE21 SLOW_CALL_STACK_MESSAGE21

#else _DEBUG

#define DEBUG_SLOW_CALL_STACK_MESSAGE1(p1)
#define DEBUG_SLOW_CALL_STACK_MESSAGE2(p1, p2)
#define DEBUG_SLOW_CALL_STACK_MESSAGE3(p1, p2, p3)
#define DEBUG_SLOW_CALL_STACK_MESSAGE4(p1, p2, p3, p4)
#define DEBUG_SLOW_CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5)
#define DEBUG_SLOW_CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6)
#define DEBUG_SLOW_CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7)
#define DEBUG_SLOW_CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8)
#define DEBUG_SLOW_CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define DEBUG_SLOW_CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define DEBUG_SLOW_CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define DEBUG_SLOW_CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define DEBUG_SLOW_CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
#define DEBUG_SLOW_CALL_STACK_MESSAGE14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)
#define DEBUG_SLOW_CALL_STACK_MESSAGE15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define DEBUG_SLOW_CALL_STACK_MESSAGE16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
#define DEBUG_SLOW_CALL_STACK_MESSAGE17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)
#define DEBUG_SLOW_CALL_STACK_MESSAGE18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)
#define DEBUG_SLOW_CALL_STACK_MESSAGE19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)
#define DEBUG_SLOW_CALL_STACK_MESSAGE20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)
#define DEBUG_SLOW_CALL_STACK_MESSAGE21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21)

#endif // _DEBUG

// prazdne makro: oznamuje CheckStk, ze u teto funkce si call-stack message neprejeme
#define CALL_STACK_MESSAGE_NONE
