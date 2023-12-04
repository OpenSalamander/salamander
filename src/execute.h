// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//******************************************************************************
//
// CComboboxEdit
//
// Protoze je combobox vyprasenej, nelze klasickou cestou (CB_GETEDITSEL) zjistit
// po opusteni focusu, jaka byla selection. To resi tento control.
//

class CComboboxEdit : public CWindow
{
protected:
    DWORD SelStart;
    DWORD SelEnd;

public:
    CComboboxEdit();

    void GetSel(DWORD* start, DWORD* end);

    void ReplaceText(const char* text);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//******************************************************************************
//
// Keywords
//

extern const char* EXECUTE_DRIVE;
extern const char* EXECUTE_PATH;
extern const char* EXECUTE_DOSPATH;
extern const char* EXECUTE_NAME;
extern const char* EXECUTE_DOSNAME;

extern const char* EXECUTE_ENV; // Environment Variable

extern const char* EXECUTE_SEPARATOR;    // Pro vlozeni separatoru do menu
extern const char* EXECUTE_BROWSE;       // Pro vlozeni Browse commandu do menu
extern const char* EXECUTE_HELP;         // Pro privolani helpu
extern const char* EXECUTE_TERMINATOR;   // Pro ukonceni menu
extern const char* EXECUTE_SUBMENUSTART; // zacatek submenu (umime jen jednu uroven)
extern const char* EXECUTE_SUBMENUEND;   // konec submenu (umime jen jednu uroven)

//******************************************************************************
//
// CExecuteItem
//

// implicitne se po vyberu polozky nahradi vybrany text v editlajne
// kurzor se postavi za tento text
// a do editlajny se presune focus
// nasledujici flagy modifikuji implicitni chovani:
#define EIF_REPLACE_ALL 0x01 // obsah cele editlajny bude nahrazen
#define EIF_CURSOR_1 0x02    // kurzor bude postaven jeden znak pred konec vlozeneho textu
#define EIF_CURSOR_2 0x04    // kurzor bude postaven dva znaky pred konec vlozeneho textu
#define EIF_VARIABLE 0x08    // vlozeny text bude ohranicen: $(text)
#define EIF_DONT_FOCUS 0x10  // po akci nebude focus postaven do editlajny

struct CExecuteItem
{
    const char* Keyword; // string vlozeny do editlajny
    int NameResID;       // string z resourcu, ktery bude vybalen v menu
    BYTE Flags;          // EIF_xxxx
};

//******************************************************************************
//
// Preddefinovana pole
//

extern CExecuteItem UserMenuArgsExecutes[];
extern CExecuteItem HotPathItems[];
extern CExecuteItem CommandExecutes[];
extern CExecuteItem ArgumentsExecutes[];
extern CExecuteItem InitDirExecutes[];
extern CExecuteItem InfoLineContentItems[];
extern CExecuteItem MakeFileListItems[];
extern CExecuteItem RegularExpressionItems[];

//******************************************************************************
//
// Pomocne datove struktury
//

struct CUserMenuValidationData // extra data pro validaci User Menu: pole Arguments
{
    BOOL UsesListOfSelNames;     // TRUE = tento parametr se pouziva (musime overit jestli neni prilis dlouhy)
    BOOL UsesListOfSelFullNames; // TRUE = tento parametr se pouziva (musime overit jestli neni prilis dlouhy)
    BOOL UsesFullPathLeft;       // TRUE = tento parametr se pouziva (musime overit jestli je definovan)
    BOOL UsesFullPathRight;      // TRUE = tento parametr se pouziva (musime overit jestli je definovan)
    BOOL UsesFullPathInactive;   // TRUE = tento parametr se pouziva (musime overit jestli je definovan)

    BOOL MustHandleItemsAsGroup;     // TRUE = s polozkami se musi pracovat jako se skupinou: ListOfSelectedNames, ListOfSelectedFullNames, FileToCompareXXX, DirToCompareXXX
    BOOL MustHandleItemsOneByOne;    // TRUE = s polozkami se musi pracovat samostatne: FullName, Name, NamePart, ExtPart, DOSFullName, DOSName, DOSNamePart, DOSExtPart
    int UsedCompareType;             // 0 = zatim zadny, 1 = file-left-right, 2 = file-active-inactive, 3 = dir-left-right, 4 = dir-active-inactive, 5 = kolize vice typu (nepripustne), 6 = file-or-dir-left-right, 7 = file-or-dir-active-inactive
    BOOL UsedCompareLeftOrActive;    // TRUE = pouzita aspon jedna promenna compare-left nebo compare-active (testujeme jestli se pouzivaji oba parametry, jinak je to nesmysl)
    BOOL UsedCompareRightOrInactive; // TRUE = pouzita aspon jedna promenna compare-right nebo compare-inactive (testujeme jestli se pouzivaji oba parametry, jinak je to nesmysl)
};

struct CUserMenuAdvancedData // extra data jen pro User Menu: pole Arguments
{
    // predpocitane hodnoty nekterych parametru:
    char ListOfSelNames[USRMNUARGS_MAXLEN];     // prazdny string = prazdny nebo prilis dlouhy seznam (delsi nez USRMNUARGS_MAXLEN), rozhoduje ListOfSelNamesIsEmpty
    BOOL ListOfSelNamesIsEmpty;                 // TRUE = prazdny ListOfSelNames
    char ListOfSelFullNames[USRMNUARGS_MAXLEN]; // prazdny string = prazdny nebo prilis dlouhy seznam (delsi nez USRMNUARGS_MAXLEN), rozhoduje ListOfSelFullNamesIsEmpty
    BOOL ListOfSelFullNamesIsEmpty;             // TRUE = prazdny ListOfSelFullNames
    char FullPathLeft[MAX_PATH];                // prazdny string = neni definovana (jsme ve Findu nebo je v panelu archiv/FS)
    char FullPathRight[MAX_PATH];               // prazdny string = neni definovana (jsme ve Findu nebo je v panelu archiv/FS)
    const char* FullPathInactive;               // ukazuje do FullPathLeft nebo FullPathRight: prazdny string = neni definovana (jsme ve Findu nebo je v panelu archiv/FS)
    char CompareName1[MAX_PATH];                // prvni full-name pro compare (soubor nebo adresar)
    char CompareName2[MAX_PATH];                // druhy full-name pro compare (soubor nebo adresar)
    BOOL CompareNamesAreDirs;                   // TRUE = CompareName1 a CompareName2 jsou adresare (jinak jde o soubory)
    BOOL CompareNamesReversed;                  // TRUE = jmena pro compare nejsou z jednoho panelu + CompareName1 je z praveho panelu
};

//******************************************************************************
//
// Vlastni funkce
//

// vybali Popupmenu s prislusnym seznamem a po zvoleni polozy vlozi text do editlajny
// hParent:          dialog, ve kterem je editlajna/combobx a browse tlacitko
// buttonResID:      id browse tlacitka
// editlineResID:    id editlajny/comboboxu
// combobox:         pokud je TRUE, editlineResID identifikuje editlajnu; jinak combobox
//                   pokud je TRUE, k editlineResID by mel byt pripojeny control CComboboxEdit
// executeItems:     pole, ze ktereho je naplneno menu
// filterResID:      text do browse okna otevreneho ve specialnim pripade z menu
// replaceWholeText: pokud je TRUE, cely obsah editlineResID bude zmenen; jinak
//                   se nahradi pouze selection
const CExecuteItem* TrackExecuteMenu(HWND hParent, int buttonResID, int editlineResID,
                                     BOOL combobox, CExecuteItem* executeItems, int filterResID = 0);

// vybali FileOpen dialog pro *.exe
// vybrany soubor vlozi do editliny
// vraci TRUE, pokud user vybral soubor; jinak vraci FALSE
BOOL BrowseCommand(HWND hParent, int editlineResID, int filterResID);

// kontroluje varText obsahujici promenne z pole UserMenuArgsExecutes
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ValidateUserMenuArguments(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2,
                               CUserMenuValidationData* userMenuValidationData);

// expanduje varText obsahujici promenne z pole UserMenuArgsExecutes, vysledek ulozi do bufferu
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji; neni-li
// 'fileNameUsed' NULL, prirazuje se do nej TRUE pokud se pouzije cesta nebo jmeno souboru
BOOL ExpandUserMenuArguments(HWND msgParent, const char* name, const char* dosName, const char* varText,
                             char* buffer, int bufferLen, BOOL* fileNameUsed,
                             CUserMenuAdvancedData* userMenuAdvancedData,
                             BOOL ignoreEnvVarNotFoundOrTooLong);

// kontroluje varText obsahujici promenne z pole Command
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ValidateCommandFile(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2);

// kontroluje varText obsahujici promenne z pole HotPath
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ValidateHotPath(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2);

// kontroluje varText obsahujici promenne z pole ArgumentsExecutes
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ValidateArguments(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2);

// expanduje varText obsahujici promenne z pole ArgumentsExecutes, vysledek ulozi do bufferu
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji; neni-li
// 'fileNameUsed' NULL, prirazuje se do nej TRUE pokud se pouzije cesta nebo jmeno souboru
BOOL ExpandArguments(HWND msgParent, const char* name, const char* dosName, const char* varText,
                     char* buffer, int bufferLen, BOOL* fileNameUsed);

// kontroluje varText obsahujici promenne z pole InfoLineContentItems
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ValidateInfoLineItems(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2);

// expanduje varText obsahujici promenne z pole InfoLineContentItems, vysledek ulozi do bufferu
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
// varPlacements: pole o [varPlacementsCount] polozkach, bude naplneno pozicema promennych
//                ve vystupnim bufferu (LOWORD) a jejim poctu znaku (HIWORD)

BOOL ExpandInfoLineItems(HWND msgParent, const char* varText, CPluginDataInterfaceEncapsulation* pluginData,
                         CFileData* fData, BOOL isDir, char* buffer, int bufferLen, DWORD* varPlacements,
                         int* varPlacementsCount, DWORD validFileData, BOOL isDisk);

// kontroluje varText obsahujici promenne z pole MakeFileListItems
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ValidateMakeFileList(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2);

// expanduje varText obsahujici promenne z pole MakeFileListItems, vysledek ulozi do bufferu
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
// maxVarSizes: pole o [maxVarSizesCount] polozkach, pokud odpovidajici pormenna bude mit
//              modifikator ":max" a zaroven jeji delka bude vetsi nez polozka v poli,
//              bude do pole prirazena delka;
//              pokud bude detectMaxVarSizes==TRUE, pouzije se maximalni delka pro format sloupce
BOOL ExpandMakeFileList(HWND msgParent, const char* varText, CPluginDataInterfaceEncapsulation* pluginData,
                        CFileData* fData, BOOL isDir, char* buffer, int bufferLen, BOOL detectMaxVarSizes,
                        int* maxVarSizes, int maxVarSizesCount, DWORD validFileData, const char* path,
                        BOOL ignoreEnvVarNotFoundOrTooLong);

// kontroluje varText obsahujici promenne z pole InitDirExecutes
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ValidateInitDir(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2);

// expanduje varText obsahujici promenne z pole InitDirExecutes, vysledek ulozi do bufferu
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ExpandInitDir(HWND msgParent, const char* name, const char* dosName, const char* varText,
                   char* buffer, int bufferLen, BOOL ignoreEnvVarNotFoundOrTooLong);

// expanduje varText obsahujici environment promenne , vysledek ulozi do bufferu
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ExpandCommand(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                   BOOL ignoreEnvVarNotFoundOrTooLong);

// expanduje varText obsahujici environment promenne, vysledek ulozi do bufferu
// msgParent - parent message-boxu s chybou, je-li NULL, chyby se nevypisuji
BOOL ExpandHotPath(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                   BOOL ignoreEnvVarNotFoundOrTooLong);
