// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************

struct CCodeTablesData
{
    char* Name;      // jmeno v menu, NULL = separator
    char Table[256]; // kodovaci tabulka
};

enum CCodeTableStateEnum
{
    ctsSuccessfullyLoaded, // convert.cfg byla uspene nacten z adresare dirName
    ctsDefaultValues       // convert.cfg se nepodarilo nacist a byly pouzity implicitni hodnoty
};

// pomocna trida, slouzici k nacteni dat z convert\XXX\convert.cfg
class CCodeTable
{
protected:
    TIndirectArray<CCodeTablesData> Data;
    char WinCodePage[101];            // jmeno windows code page (pro Cechy CP1250 - regionalni kodovani)
    DWORD WinCodePageIdentifier;      // 1250, 1251, 1252, ...
    char WinCodePageDescription[101]; // lidsky citelny popis (Central Europe, West Europe & U.S.)
    char DirectoryName[MAX_PATH];     // nazev adresare XXX: convert\XXX\convert.cfg
    CCodeTableStateEnum State;

public:
    // hWindow je parent pro messageboxy;
    // dirName je adresar, kde se ma nacist convert.cfg (napriklad "centeuro")
    CCodeTable(HWND hWindow, const char* dirName);
    ~CCodeTable();

    CCodeTableStateEnum GetState() { return State; }

    // nebudeme tahat vsechny metory do tohoto objektu, radeji k datum pristoupime zvenku
    friend class CCodeTables;
};

class CCodeTables
{
protected:
    CRITICAL_SECTION LoadCS;    // kriticka sekce pro Load dat z convert.cfg (dale uz se jen cte)
    CRITICAL_SECTION PreloadCS; // kriticka sekce pro Preload dat
    BOOL Loaded;
    CCodeTable* Table; // nactenda tabulka, nad kterou pracujeme

    // slouzi pro enumeraci konverzi
    TIndirectArray<CCodeTable> Preloaded;

public:
    CCodeTables();
    ~CCodeTables();

    // prohleda adresare 'convert' a naplni pole Preloaded
    void PreloadAllConversions();
    // uvolni pole Preloaded
    void FreePreloadedConversions();
    // vraci postupne vsechny polozky z pole Preloaded
    BOOL EnumPreloadedConversions(int* index, const char** winCodePage,
                                  DWORD* winCodePageIdentifier,
                                  const char** winCodePageDescription,
                                  const char** dirName);

    // pokud nalezne polozku s cestou dirName, nastavi index a vrati TRUE
    // jinak vrati FALSE
    BOOL GetPreloadedIndex(const char* dirName, int* index);

    // z pole Preloaded vyhleda nejlepe vyhovujici polozku
    // cfgDirName ukazuje na doporuceny nazev adresare (z konfigurace)
    // dirName musi ukazovat do bufferu o velikosti MAX_PATH, kam bude
    // vracen nazev adresare; mpokud zadny neexistuje, bude vracen prazdny retezec
    // kriteria:
    //  1. cfgDirName
    //  2. Polozka odpovidajici kodove strance operacniho systemu
    //  3. westeuro
    //  4. prvni v seznam
    //  5. pokud neni zadna polozka v seznamu, vratime prazdny retezec
    void GetBestPreloadedConversion(const char* cfgDirName, char* dirName);

    // pripravi objekt na pouziti (nacte skript 'convert.cfg'), hWindow je parent pro messageboxy
    // vrati TRUE, pokud lze volat ostatni metody
    BOOL Init(HWND hWindow);
    // vrati TRUE, pokud jsou inicializovany
    BOOL IsLoaded() { return Loaded; }
    // naplni menu dostupnymi kody a oznaci aktivni
    // kodovani - 'codeType' (pamet typu kodovani okna viewru)
    void InitMenu(HMENU menu, int& codeType);
    // prepne kodovani na dalsi na seznamu
    void Next(int& codeType);
    // prepne kodovani na predchozi na seznamu
    void Previous(int& codeType);
    // naplni kodovaci tabulku podle 'codeType' (pamet typu kodovani okna viewru)
    // vraci TRUE pokud byla 'table' nainicializovana (jinak 'table' nepouzivat a 'codeType' je nastavena na 0)
    BOOL GetCode(char* table, int& codeType);
    // naplni codeType podle 'coding' (jmeno kodovani, ignoruji se '-' a ' ' a '&')
    // vraci TRUE pokud byla tabulka nalezena a 'codeType' je inicializovana
    // jinak 'codeType' obsahuje 0 a je vraceno FALSE
    BOOL GetCodeType(const char* coding, int& codeType);
    // overi platnost 'codeType' (kontrola rozsahu)
    BOOL Valid(int codeType);
    // vrati nazev odpovidajiciho kodovani (muze obsahovat '&' - hotkey v menu)
    BOOL GetCodeName(int codeType, char* buffer, int bufferLen);
    // vraci postupne vsechny kodovani ('name' muze obsahovat '&' - hotkey v menu)
    BOOL EnumCodeTables(HWND parent, int* index, const char** name, const char** table);
    // vraci WinCodePage
    void GetWinCodePage(char* buf);
    // zjisti z bufferu 'pattern' o delce 'patternLen' jestli jde o text (existuje kodova stranka,
    // ve ktere obsahuje jen povolene znaky - zobrazitelne a ridici) a pokud jde o text, zjisti take
    // jeho kodovou stranku (nejpravdepodobnejsi)
    void RecognizeFileType(const char* pattern, int patternLen, BOOL forceText,
                           BOOL* isText, char* codePage);
    // vraci index konverzni tabulky z 'codePage' do WinCodePage; pokud nenajde, vraci -1
    int GetConversionToWinCodePage(const char* codePage);
};

extern CCodeTables CodeTables;
