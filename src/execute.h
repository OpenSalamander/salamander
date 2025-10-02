// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//******************************************************************************
//
// CComboboxEdit
//
// The combobox loses focus and control is buggy so we can't determine the selection
// the usual way. This helper control works around that problem.
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

extern const char* EXECUTE_SEPARATOR;    // to insert a separator into the menu
extern const char* EXECUTE_BROWSE;       // to insert a Browse command into the menu
extern const char* EXECUTE_HELP;         // to call help
extern const char* EXECUTE_TERMINATOR;   // to end menu
extern const char* EXECUTE_SUBMENUSTART; // start of a submenu (only one level supported)
extern const char* EXECUTE_SUBMENUEND;   // end of a submenu (only one level supported)

//******************************************************************************
//
// CExecuteItem
//

// by default after selecting the item the current text in the edit line is replaced,
// the cursor is positioned after this text,
// and the edit line receives focus.
// The following flags modify this default behavior:
#define EIF_REPLACE_ALL 0x01 // replace the entire contents of the edit line
#define EIF_CURSOR_1 0x02    // place the cursor one character before the end of the inserted text
#define EIF_CURSOR_2 0x04    // place the cursor two characters before the end of the inserted text
#define EIF_VARIABLE 0x08    // wrap the inserted text in $(text)
#define EIF_DONT_FOCUS 0x10  // do not move focus back to the edit line after the action

struct CExecuteItem
{
    const char* Keyword; // string inserted into the edit line
    int NameResID;       // resource string displayed in the menu
    BYTE Flags;          // EIF_xxxx
};

//******************************************************************************
//
// Predefined arrays
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
// Helper data structures
//

struct CUserMenuValidationData // additional data used to validate User Menu: array Arguments
{
    BOOL UsesListOfSelNames;     // TRUE = this parameter is used; (check that it is not too long)
    BOOL UsesListOfSelFullNames; // TRUE = this parameter is used; (check that it is not too long)
    BOOL UsesFullPathLeft;       // TRUE = this parameter is used; (verify if it is defined)
    BOOL UsesFullPathRight;      // TRUE = this parameter is used; (verify if it is defined)
    BOOL UsesFullPathInactive;   // TRUE = this parameter is used; (verify if it is defined)

    BOOL MustHandleItemsAsGroup;     // TRUE = items must be processed as a group: ListOfSelectedNames, ListOfSelectedFullNames, FileToCompareXXX, DirToCompareXXX
    BOOL MustHandleItemsOneByOne;    // TRUE = items must be processed individually: FullName, Name, NamePart, ExtPart, DOSFullName, DOSName, DOSNamePart, DOSExtPart
    int UsedCompareType;             // 0 = none yet, 1 = file-left-right, 2 = file-active-inactive, 3 = dir-left-right, 4 = dir-active-inactive, 5 = multiple types (invalid), 6 = file-or-dir-left-right, 7 = file-or-dir-active-inactive
    BOOL UsedCompareLeftOrActive;    // TRUE = at least one variable compare-left or compare-active is used (we're testing if both parameters are used; otherwise it's nonsense)
    BOOL UsedCompareRightOrInactive; // TRUE = at least one variable compare-right or compare-inactive is used (we're testing if both parameters are used; otherwise it's nonsense)
};

struct CUserMenuAdvancedData // additional data used only for the User Menu: array Arguments
{
    // precompute values of some parameters:
    char ListOfSelNames[USRMNUARGS_MAXLEN];     // empty string = empty or too long list (longer than USRMNUARGS_MAXLEN); see ListOfSelNamesIsEmpty
    BOOL ListOfSelNamesIsEmpty;                 // TRUE = ListOfSelNames is empty
    char ListOfSelFullNames[USRMNUARGS_MAXLEN]; // empty string = empty or too long list (longer than USRMNUARGS_MAXLEN); see ListOfSelFullNamesIsEmpty
    BOOL ListOfSelFullNamesIsEmpty;             // TRUE = ListOfSelFullNames is empty
    char FullPathLeft[MAX_PATH];                // empty string = not defined (we are in Find or the panel shows archive/FS)
    char FullPathRight[MAX_PATH];               // empty string = not defined (we are in Find or the panel shows archive/FS)
    const char* FullPathInactive;               // points to FullPathLeft or FullPathRight: empty string = not defined (we are in Find or the panel shows archive/FS)
    char CompareName1[MAX_PATH];                // first full name for compare (file or directory)
    char CompareName2[MAX_PATH];                // second full name for compare (file or directory)
    BOOL CompareNamesAreDirs;                   // TRUE = CompareName1 and CompareName2 are directories (otherwise they're files)
    BOOL CompareNamesReversed;                  // TRUE = names for compare come from different panels + CompareName1 is from the right panel
};

//******************************************************************************
//
// Custom functions
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
