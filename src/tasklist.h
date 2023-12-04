// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//
// ****************************************************************************

// TRUE = prvni bezici instance verze 3.0 nebo novejsi
// urcuje se na zaklade mutexu v globalnim namespace, takze se vidi s mutexty
// z ostatnich sessions (remote desktop, fast user switching)
extern BOOL FirstInstance_3_or_later;

// sdilena pamet obsahuje:
//  DWORD                  - PID procesu, ktery se ma breaknout
//  DWORD                  - pocet polozek v listu
//  MAX_TL_ITEMS * CTLItem - list polozek

#define MAX_TL_ITEMS 500 // maximalni pocet polozek ve sdilene pameti, nelze menit!

#define TASKLIST_TODO_HIGHLIGHT 1 // okno procesu daneho v 'PID' se ma vysvitit
#define TASKLIST_TODO_BREAK 2     // proces dany v 'PID' se ma breaknout
#define TASKLIST_TODO_TERMINATE 3 // proces dany v 'PID' se ma terminovat
#define TASKLIST_TODO_ACTIVATE 4  // proces dany v 'PID' se ma aktivovat

#define TASKLIST_TODO_TIMEOUT 5000 // 5 vterin, ktere maji procesy pro zpracovani todo

#define PROCESS_STATE_STARTING 1 // nas proces startuje, jeste neexistuje hlavni okno
#define PROCESS_STATE_RUNNING 2  // nas proces bezi, mame hlavni okno
#define PROCESS_STATE_ENDING 3   // nas proces konci, nemame uz hlavni okno

#pragma pack(push, enter_include_tasklist) // aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(4)

extern HANDLE HSalmonProcess;

// POZOR, pomoci struktury komunikuji x64 a x86 procesy, pozor na typy (napr. HANDLE), ktere maji ruzne sirky
struct CProcessListItem
{
    DWORD PID;            // ProcessID, unikatni po dobu behu procesu, pak muze byt znovu pouzito
    SYSTEMTIME StartTime; // Kdy byl proces nastartovan
    DWORD IntegrityLevel; // Integrity Level procesu, slouzi k rozliseni procesu spustenych na ruzne urovni opravneni
    BYTE SID_MD5[16];     // MD5 napocitana ze SID procesu, slouzi nam k rozliseni procesu bezicich pod ruznymi uzivateli; SID ma neznamou delku, proto tato obezlicka
    DWORD ProcessState;   // Stav v jakem se Salamander nachazi, viz PROCESS_STATE_xxx
    UINT64 HMainWindow;   // (x64 friendly) Handle hlavniho okna, pokud jiz/jeste existuje (nastavuje se pri jeho vytvareni/destrukci)
    DWORD SalmonPID;      // ProcessID salmonu, aby mu brakujici proces mohl garantovat pravo pro SetForegroundWindow

    CProcessListItem()
    {
        PID = GetCurrentProcessId();
        GetLocalTime(&StartTime);
        GetProcessIntegrityLevel(&IntegrityLevel);
        GetSidMD5(SID_MD5);
        ProcessState = PROCESS_STATE_STARTING;
        HMainWindow = NULL;
        SalmonPID = 0;
        if (HSalmonProcess != NULL)
            SalmonPID = SalGetProcessId(HSalmonProcess); // v tuto dobu jiz Salmon bezi
    }
};

// POZOR, ke strukture lze pouze pridavat polozky, protoze na ni chodi i starsi verze Salamandera
// POZOR, pomoci struktury komunikuji x64 a x86 procesy, pozor na typy (napr. HANDLE), ktere maji ruzne sirky
// POZOR, nejspis nema smysl zvedat verzi a rozsirovat strukturu, protoze pridana data v tom pripade
//        nebudou vzdy k dispozici (stara verze Salama nahozena jako prvni = ve sdilene pameti nove
//        polozky vubec nebudou) => spravne reseni je nejspis zmena AS_PROCESSLIST_NAME a spol. +
//        uprava dat dle libosti (klidne zvetsovani, promazavani, zmena poradi, atd.)
struct CCommandLineParams
{
    DWORD Version;               // novejsi verze Salamandera mohou zvysovat 'Version' a zacit vyuzivat promenne ReservedX
    DWORD RequestUID;            // unikatni (zvysujici se) ID pozadavku o aktivaci
    DWORD RequestTimestamp;      // GetTickCount() hodnota ze chvile, kdy byl zakladan pozadavek pro aktivaci
    char LeftPath[2 * MAX_PATH]; // cesty do panelu (levy, pravy, pripadne aktivni); pokud jsou prazdne, nemaji se nastavit
    char RightPath[2 * MAX_PATH];
    char ActivePath[2 * MAX_PATH];
    DWORD ActivatePanel;         // ktery panel se ma aktivovat 0-zadny, 1-levy, 2-pravy
    BOOL SetTitlePrefix;         // pokud je TRUE, nastavi se prefix titulku podle TitlePrefix
    char TitlePrefix[MAX_PATH];  // prefix titulku, pokud je prazdny, nemenit; delku radeji deklaruji na MAX_PATH, misto TITLE_PREFIX_MAX, ktery by se mohl pod rukama zmenit
    BOOL SetMainWindowIconIndex; // pokud je TRUE, nastavi se ikona hlavniho okna podle MainWindowIconIndex
    DWORD MainWindowIconIndex;   // 0: prvni ikona, 1: druha ikona, ...
    // POZOR, strukturu lze rozsirovat pouze v pripade, ze je stale zadeklarovana jako posledni v CProcessList,
    // jinak uz je pozde a nesmi se ji dotknout

    CCommandLineParams()
    {
        ZeroMemory(this, sizeof(CCommandLineParams));
    }
};

// Open Salamander Process List
// !!! POZOR, ke strukture lze pouze pridavat polozky, protoze na ni chodi i starsi verze Salamandera
struct CProcessList
{
    DWORD Version; // novejsi verze Salamandera mohou zvysovat 'Version' a zacit vyuzivat promenne ReservedX

    DWORD ItemsCount;    // pocet validnich polozek v poli Items
    DWORD ItemsStateUID; // "verze" Items seznamu; zvysuje se s kazdou zmenou; slouzi pro Tasks dialog jako signal, ze se ma refreshnout
    CProcessListItem Items[MAX_TL_ITEMS];

    DWORD Todo;                           // urcuje co se ma delat po odpaleni eventu pomoci FireEvent, obsahuje jednu z hodnot TASKLIST_TODO_*
    DWORD TodoUID;                        // poradi zaslaneho pozadavku, pro kazdy dalsi pozadavek ze zvysuje
    DWORD TodoTimestamp;                  // GetTickCount() hodnota ze chvile, kdy byl zakladan Todo pozadavek
    DWORD PID;                            // PID, pro ktery se ma provest cinnost z Todo
    CCommandLineParams CommandLineParams; // cesty pro panely a dalsi parametry pro aktivaci
                                          // POZOR, pokud bude potreba rozsirovat tuto strukturu, bylo by rozumne napred rozsirit CCommandLineParams, napriklad
                                          // reservovat nejake MAX_PATH buffery a par DWORDu, pokud bychom chteli predavat nove command line parametry
};

#pragma pack(pop, enter_include_tasklist)

class CTaskList
{
protected:
    HANDLE FMO;                // file-mapping-object, sdilena pamet
    CProcessList* ProcessList; // ukazatel do sdilene pameti
    HANDLE FMOMutex;           // mutex pro reseni pristupu k FMO
    HANDLE Event;              // event, je-li signaled, mely by se ostatni procesy podivat,
                               // jestli se nemaji provest cinnost danou v Todo
    HANDLE EventProcessed;     // pokud jeden z procesu provede cinnost v Todo, nastavi tento
                               // event na signaled na znameni ridicimu procesu, ze je hotovo
    HANDLE TerminateEvent;     // event pro ukonceni break-threadu
    HANDLE ControlThread;      // control-thread (ceka na eventy, ktere obratem odbavi)
    BOOL OK;                   // probehla konstrukce o.k.?

public:
    CTaskList();
    ~CTaskList();

    BOOL Init();

    // naplni polozky task-listu, items - pole alespon MAX_TL_ITEMS struktur CTLItem, vraci pocet polozek
    // 'items' muze byt NULL, pokud nas zajima pouze 'itemsStateUID'
    // vrati "verzi" sestavy procesu; verze se zvysuje s kazdou zmenou v seznamu (pokud je pridana nebo odebrana polozka)
    // slouzi pro dialog jako informace, ze ma refreshnout seznam; 'itemsStateUID' muze byt NULL
    // pokud je 'timeouted' ruzny od NULL, nastavi zda k neuspechu vedl timeout pri cekani na sdilenou pamet
    int GetItems(CProcessListItem* items, DWORD* itemsStateUID, BOOL* timeouted = NULL);

    // pozada proces 'pid' o provedeni akce dle 'todo' (mimo TASKLIST_TODO_ACTIVATE)
    // pokud je 'timeouted' ruzny od NULL, nastavi zda k neuspechu vedl timeout pri cekani na sdilenou pamet
    BOOL FireEvent(DWORD todo, DWORD pid, BOOL* timeouted = NULL);

    // pokud je 'timeouted' ruzny od NULL, nastavi zda k neuspechu vedl timeout pri cekani na sdilenou pamet
    BOOL ActivateRunningInstance(const CCommandLineParams* cmdLineParams, BOOL* timeouted = NULL);

    // v seznamu procesu dohleda nas a nastavi 'ProcessState' a 'HMainWindow'; vraci TRUE v pripade uspechu, jinak FALSE
    // pokud je 'timeouted' ruzny od NULL, nastavi zda k neuspechu vedl timeout pri cekani na sdilenou pamet
    BOOL SetProcessState(DWORD processState, HWND hMainWindow, BOOL* timeouted = NULL);

protected:
    // projde seznam procesu a vytridi neexistujici polozky
    // nutne volat po uspesnem vstupu kriticke sekce 'FMOMutex'!
    // nastavi 'changed' na TRUE, pokud byla nejaka polozka zahozena, jinak na FALSE
    BOOL RemoveKilledItems(BOOL* changed);

    friend DWORD WINAPI FControlThread(void* param);
};

extern CTaskList TaskList;

// ochrana pristupu do CommandLineParams
extern CRITICAL_SECTION CommandLineParamsCS;
// slouzi k predani parametru pro aktivaci Salamander z Control threadu do hlavniho thradu
extern CCommandLineParams CommandLineParams;
// event je "signaled" jakmile hlavni thread prevezme paramtery
extern HANDLE CommandLineParamsProcessed;
