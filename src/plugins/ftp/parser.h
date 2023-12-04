// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// preskoci nazev identifikatoru (jmeno funkce, stavove promenne nebo sloupce),
// vraci TRUE pokud byl preskocen aspon jeden znak (identifikator ma aspon jeden
// znak); jinak vraci v 'errorResID' chybu 'emptyErrID'
BOOL SkipIdentifier(const char*& s, const char* end, int* errorResID, int emptyErrID);

//
// ****************************************************************************
// CFTPParserParameter
//

// kody stavovych promennych
enum CFTPParserStateVariables
{
    psvNone, // neinicializovano
    psvFirstNonEmptyLine,
    psvLastNonEmptyLine,
    psvNextChar,
    psvNextWord,
    psvRestOfLine,
};

enum CFTPParserBinaryOperators
{
    pboNone,                // neinicializovano
    pboEqual,               // rovnost
    pboNotEqual,            // nerovnost
    pboStrEqual,            // rovnost retezcu (case insensitive)
    pboStrNotEqual,         // nerovnost retezcu (case insensitive)
    pboSubStrIsInString,    // podretezec je v retezci (case insensitive)
    pboSubStrIsNotInString, // podretezec neni v retezci (case insensitive)
    pboStrEndWithString,    // retezec konci na retezec (case insensitive)
    pboStrNotEndWithString, // retezec nekonci na retezec (case insensitive)

    // POZOR: typova kontrola je v metode CFTPParserFunction::MoveRightOperandToExpression()
};

enum CFTPParserParameterType
{
    pptNone,       // neinicializovano
    pptColumnID,   // identifikator sloupce (-3==is_link, -2==is_hidden, -1==is_dir, od 0 jde o index v poli 'columns')
    pptBoolean,    // boolean (true==1, false==0)
    pptString,     // retezec
    pptNumber,     // signed int64 (unsigned int64 je zbytecny)
    pptStateVar,   // stavova promenna (viz typ CStateVariables)
    pptExpression, // vyraz (jen format: prvni_operand + bin_operace + druhy_operand)
};

// konstanty pro hodnoty parametru typu pptColumnID
#define COL_IND_ISLINK -3   // standardni sloupec "is_link"
#define COL_IND_ISHIDDEN -2 // standardni sloupec "is_hidden"
#define COL_IND_ISDIR -1    // standardni sloupec "is_dir"

// konstanty pro typy operandu pri testovani spravneho pouziti operace ve vyrazu
enum CFTPParserOperandType
{
    potNone, // neinicializovano
    potBoolean,
    potString,
    potNumber,
    potDate,
    potTime,
};

// konstanty pro typy parametru pri testovani spravneho pouziti funkci (ocekavany
// pocet a typ parametru)
enum CFTPParserFuncParType
{
    pfptNone, // neinicializovano
    pfptBoolean,
    pfptString,
    pfptNumber,
    pfptColumnBoolean,
    pfptColumnString,
    pfptColumnNumber,
    pfptColumnDate,
    pfptColumnTime,
};

struct CFTPParserParameter
{
    CFTPParserParameterType Type; // typ parametru - podle tohoto atributu jsou platne/neplatne dalsi atributy objektu

    union
    {
        int ColumnIndex; // hodnota parametru typu pptColumnID

        BOOL Boolean; // hodnota parametru typu pptBoolean

        char* String; // hodnota parametru typu pptString

        __int64 Number; // hodnota parametru typu pptNumber

        CFTPParserStateVariables StateVar; // hodnota parametru typu pptStateVar

        struct // data pro vypocet parametru typu pptExpression
        {
            CFTPParserBinaryOperators BinOperator; // typ binarni operace
            CFTPParserParameter** Parameters;      // pole dvou ukazatelu na operandy
        };
    };

    CFTPParserParameter() { Type = pptNone; }

    ~CFTPParserParameter()
    {
        if (Type == pptString && String != NULL)
            free(String);
        else
        {
            if (Type == pptExpression && Parameters != NULL)
            {
                if (Parameters[0] != NULL)
                    delete Parameters[0];
                if (Parameters[1] != NULL)
                    delete Parameters[1];
                delete[] Parameters;
            }
        }
    }

    // vraci typ operandu vyrazu
    CFTPParserOperandType GetOperandType(TIndirectArray<CSrvTypeColumn>* columns);

    // vraci typ parametru funkce
    CFTPParserFuncParType GetFuncParType(TIndirectArray<CSrvTypeColumn>* columns);

    // vraci hodnotu parametru typu pfptNumber; pouziva se behem parsovani listingu
    // (ziskani hodnoty parametru funkce)
    __int64 GetNumber()
    {
#ifdef _DEBUG
        if (Type != pptNumber)
        {
            TRACE_E("Unexpected situation in CFTPParserParameter::GetNumber(): not a number!");
            return 0;
        }
#endif
        return Number;
    }

    // vraci index sloupce (parametr typu pfptColumnXXX); pouziva se behem parsovani listingu
    // (ziskani hodnoty parametru funkce - zapis do sloupce)
    int GetColumnIndex()
    {
#ifdef _DEBUG
        if (Type != pptColumnID)
        {
            TRACE_E("Unexpected situation in CFTPParserParameter::GetColumnIndex(): not a column!");
            return -1;
        }
#endif
        return ColumnIndex;
    }

    // vraci hodnotu parametru nebo stavove promenne typu pptString; pouziva se
    // behem parsovani listingu; (ziskani hodnoty parametru funkce); muze vracet
    // i NULL (nedostatek pameti (do 'lowMemErr' se priradi TRUE) nebo 'String' je NULL)
    const char* GetString(const char* listing, const char* listingEnd, BOOL* needDealloc,
                          BOOL* lowMemErr);

    // vraci hodnotu parametru, vyrazu, ze sloupce nebo stavove promenne typu
    // pptBoolean a potBoolean; pouziva se behem parsovani listingu;
    // (ziskani hodnoty parametru funkce nebo operandu)
    BOOL GetBoolean(CFileData* file, BOOL* isDir, CFTPListingPluginDataInterface* dataIface,
                    TIndirectArray<CSrvTypeColumn>* columns, const char* listing,
                    const char* listingEnd, CFTPParser* actualParser);

    // vraci hodnotu parametru nebo hodnotu ze sloupce typu potNumber; pouziva se behem
    // parsovani listingu (ziskani hodnoty operandu); v 'minus' (nesmi byt NULL) vraci TRUE
    // pokud jde o zaporne cislo (jen sloupec typu stctGeneralNumber nebo parametr pptNumber)
    __int64 GetNumberOperand(CFileData* file, CFTPListingPluginDataInterface* dataIface,
                             TIndirectArray<CSrvTypeColumn>* columns, BOOL* minus);

    // vraci hodnotu ze sloupce typu potDate; pouziva se behem parsovani listingu
    // (ziskani hodnoty operandu)
    void GetDateOperand(CFTPDate* date, CFileData* file,
                        CFTPListingPluginDataInterface* dataIface,
                        TIndirectArray<CSrvTypeColumn>* columns);

    // vraci hodnotu ze sloupce typu potTime; pouziva se behem parsovani listingu
    // (ziskani hodnoty operandu)
    void GetTimeOperand(CFTPTime* time, CFileData* file,
                        CFTPListingPluginDataInterface* dataIface,
                        TIndirectArray<CSrvTypeColumn>* columns);

    // vraci hodnotu parametru, stavove promenne nebo hodnotu ze sloupce typu potTime;
    // pouziva se behem parsovani listingu (ziskani hodnoty operandu)
    void GetStringOperand(const char** beg, const char** end, CFileData* file,
                          CFTPListingPluginDataInterface* dataIface,
                          TIndirectArray<CSrvTypeColumn>* columns,
                          const char* listing, const char* listingEnd);
};

//
// ****************************************************************************
// CFTPParserFunction
//

// kody funkci
enum CFTPParserFunctionCode
{
    fpfNone, // neinicializovano
    fpfSkip_white_spaces,
    fpfWhite_spaces,
    fpfWhite_spaces_and_line_ends,
    fpfRest_of_line,
    fpfWord,
    fpfNumber,
    fpfPositiveNumber,
    fpfNumber_with_separators,
    fpfMonth_3,
    fpfMonth_txt,
    fpfMonth,
    fpfDay,
    fpfYear,
    fpfTime,
    fpfYear_or_time,
    fpfAll,
    fpfAll_to,
    fpfAll_up_to,
    fpfUnix_link,
    fpfUnix_device,
    fpfIf,
    fpfAssign,
    fpfCut_white_spaces_end,
    fpfCut_white_spaces_start,
    fpfCut_white_spaces,
    fpfBack,
    fpfAdd_string_to_column,
    fpfCut_end_of_string,
    fpfSkip_to_number,
};

class CFTPParserFunction
{
protected:
    CFTPParserFunctionCode Function;

    TIndirectArray<CFTPParserParameter> Parameters; // seznam vsech funkci pravidla

public:
    CFTPParserFunction(CFTPParserFunctionCode func) : Parameters(1, 1) { Function = func; }

    BOOL IsGood() { return Parameters.IsGood(); }

    // prida parametr typu pptColumnID; pri chybe alokace vraci FALSE a 'lowMem' (neni-li
    // NULL) nastavi na TRUE
    BOOL AddColumnIDParameter(int columnIndex, BOOL* lowMem);

    // prida parametr typu pptString; 'str' je alokovany string, metoda zajisti jeho uvolneni;
    // pri chybe alokace vraci FALSE a 'lowMem' (neni-li NULL) nastavi na TRUE
    BOOL AddStringParameter(char* str, BOOL* lowMem);

    // prida parametr typu pptNumber; pri chybe alokace vraci FALSE a 'lowMem' (neni-li
    // NULL) nastavi na TRUE
    BOOL AddNumberParameter(__int64 number, BOOL* lowMem);

    // prida parametr typu pptStateVar; pri chybe alokace vraci FALSE a 'lowMem' (neni-li
    // NULL) nastavi na TRUE
    BOOL AddStateVarParameter(CFTPParserStateVariables var, BOOL* lowMem);

    // prida parametr typu pptBoolean; pri chybe alokace vraci FALSE a 'lowMem' (neni-li
    // NULL) nastavi na TRUE
    BOOL AddBooleanParameter(BOOL boolVal, BOOL* lowMem);

    // nahradi posledni parametr nove alokovanym parametrem typu pptExpression; nahrazeny
    // parametr (pred volanim metody posledni v Parameters) je pouzity jako levy operand
    // vznikleho vyrazu; pri chybe alokace vraci FALSE a 'lowMem' (neni-li NULL) nastavi
    // na TRUE
    BOOL AddExpressionParameter(CFTPParserBinaryOperators oper, BOOL* lowMem);

    // presune posledni parametr do parametru typu pptExpression (ten je predposledni);
    // vraci FALSE pri spatnem volani ('lowMem' (neni-li NULL) se pak da na TRUE) nebo
    // pokud jsou typy operandu nevhodne pro operator (text chyby da do 'errorResID'
    // (neni-li NULL))
    BOOL MoveRightOperandToExpression(TIndirectArray<CSrvTypeColumn>* columns,
                                      int* errorResID, BOOL* lowMem, BOOL* colAssigned);

    // zkontroluje typ a pocet parametru pouzivanych touto funkci; vraci FALSE
    // pokud je pocet nebo typ parametru nevhodny pro funkci (text chyby da do
    // 'errorResID' (neni-li NULL))
    BOOL CheckParameters(TIndirectArray<CSrvTypeColumn>* columns, int* errorResID,
                         BOOL* colAssigned);

    // zkusi pouzit tuto funkci na text listingu od pozice "ukazovatka"; do 'file'+'isDir'
    // (nesmi byt NULL) uklada vyparsovane hodnoty (pokud funkce provadi prirazeni do
    // sloupce); 'dataIface' je rozhrani pro praci s hodnotami ulozenymi mimo CFileData
    // (pro praci s CFileData::PluginData); 'columns' jsou uzivatelem definovane sloupce;
    // 'listing' je "ukazovatko" v listingu (pozice odkud zacit parsovani), po pouziti
    // funkce se vraci nova pozice "ukazovatka"; 'listingEnd' je konec listingu;
    // pokud dojde k prirazeni hodnoty do sloupce, musi se do pole 'emptyCol' na index
    // sloupce zapsat FALSE; vraci TRUE pri uspesnem pouziti funkce, vraci FALSE pri
    // chybach: nedostatek pameti (vraci v 'lowMemErr' (nesmi byt NULL) TRUE) nebo
    // nelze pouzit tuto funkci
    BOOL UseFunction(CFileData* file, BOOL* isDir, CFTPListingPluginDataInterface* dataIface,
                     TIndirectArray<CSrvTypeColumn>* columns, const char** listing,
                     const char* listingEnd, CFTPParser* actualParser, BOOL* lowMemErr,
                     DWORD* emptyCol);

protected:
    // prida nove alokovany parametr (bez inicializace) a vrati ho v 'newPar';
    // pri chybe alokace vraci FALSE a 'lowMem' (neni-li NULL) nastavi na TRUE
    BOOL AddParameter(CFTPParserParameter*& newPar, BOOL* lowMem);
};

//
// ****************************************************************************
// CFTPParserRule
//

class CFTPParserRule
{
protected:
    TIndirectArray<CFTPParserFunction> Functions; // seznam vsech funkci pravidla

public:
    CFTPParserRule() : Functions(5, 5) {}

    BOOL IsGood() { return Functions.IsGood(); }

    // vraci TRUE pokud se uspesne nakompilovala funkce v pravidle (az do symbolu ')');
    // pri chybe vraci FALSE a nastavi 'errorResID' (cislo retezce popisujiciho
    // chybu - ulozen v resourcich) nebo 'lowMem' (TRUE = malo pameti)
    BOOL CompileNewFunction(CFTPParserFunctionCode func, const char*& rules, const char* rulesEnd,
                            TIndirectArray<CSrvTypeColumn>* columns, int* errorResID,
                            BOOL* lowMem, BOOL* colAssigned);

    // zkusi pouzit toto pravidlo na text listingu od pozice "ukazovatka"; ve 'file'
    // (nesmi byt NULL) vraci vyparsovana data (soubor nebo adresar je to jen pokud
    // emptyCol[0]==FALSE - viz popis 'emptyCol'); v 'isDir' (nesmi byt NULL) vraci
    // TRUE pokud se podarilo vyparsovat adresar a FALSE pokud se podarilo vyparsovat
    // soubor; 'dataIface' je rozhrani pro praci s hodnotami ulozenymi mimo CFileData
    // (pro praci s CFileData::PluginData); 'columns' jsou uzivatelem definovane sloupce;
    // 'listing' je "ukazovatko" v listingu (pozice odkud zacit parsovani), pri
    // uspesnem pouziti pravidla ukazuje pri navratu z metody na zacatek dalsi
    // (prvni nezpracovane) radky; 'listingEnd' je konec listingu;
    // pole 'emptyCol' na zacatku obsahuje pro kazdy sloupec TRUE, pokud dojde
    // k prirazeni hodnoty do sloupce, musi se do pole na index sloupce zapsat FALSE;
    // vraci TRUE pri uspesnem pouziti pravidla, vraci FALSE pri chybach: nedostatek
    // pameti (vraci v 'lowMemErr' (nesmi byt NULL) TRUE) nebo nelze pouzit toto pravidlo
    BOOL UseRule(CFileData* file, BOOL* isDir, CFTPListingPluginDataInterface* dataIface,
                 TIndirectArray<CSrvTypeColumn>* columns, const char** listing,
                 const char* listingEnd, CFTPParser* actualParser, BOOL* lowMemErr,
                 DWORD* emptyCol);
};

//
// ****************************************************************************
// CFTPParser
//

// bity na indexu sloupce s datumem v 'emptyCol' (viz CFTPParser::GetNextItemFromListing)
#define DATE_MASK_DAY 0x02                  // v datumu je nastaveny den (zaciname od 0x02, protoze TRUE = 0x01)
#define DATE_MASK_MONTH 0x04                // v datumu je nastaveny mesic
#define DATE_MASK_YEAR 0x08                 // v datumu je nastaveny rok
#define DATE_MASK_DATE 0x0E                 // datum je komplet nastaveny
#define DATE_MASK_YEARCORRECTIONNEEDED 0x10 // rok v datumu jeste potrebuje provest korekci (pouziva se u "year_or_time")
#define DATE_MASK_TIME 0x100                // cas je komplet nastaveny (jde o jiny sloupec, mohlo by byt 0x02, ale pro snazsi detekci chyb dame 0x10)

// konstanty pro CFTPParser::AllowedLanguagesMask
#define PARSER_LANG_ALL 0xFFFF
#define PARSER_LANG_ENGLISH 0x0001
#define PARSER_LANG_GERMAN 0x0002
#define PARSER_LANG_NORWEIGAN 0x0004
#define PARSER_LANG_SWEDISH 0x0008

class CFTPParser
{
protected:
    TIndirectArray<CFTPParserRule> Rules; // seznam vsech pravidel parseru

public:                             // pomocne promenne pouzivane behem parsovani listingu:
    int ActualYear;                 // rok z dnesniho datumu (pouziva se u funkce "year_or_time")
    int ActualMonth;                // mesic z dnesniho datumu (pouziva se u funkce "year_or_time")
    int ActualDay;                  // den z dnesniho datumu (pouziva se u funkce "year_or_time")
    const char* FirstNonEmptyBeg;   // zacatek prvniho neprazdneho radku
    const char* FirstNonEmptyEnd;   // konec prvniho neprazdneho radku
    const char* LastNonEmptyBeg;    // zacatek posledniho neprazdneho radku
    const char* LastNonEmptyEnd;    // konec posledniho neprazdneho radku
    const char* ListingBeg;         // zacatek listingu
    BOOL ListingIncomplete;         // TRUE pri nekompletnim listingu
    BOOL SkipThisLineItIsIncomlete; // TRUE jen pokud pri zpracovani pravidla bylo zjisteno, ze listing je nekompletni - skipneme koncovou cast listingu zpracovanou timto pravidlem
    DWORD AllowedLanguagesMask;     // povolene jazyky pro funkce month_3 a month_txt (bitova kombinace konstant PARSER_LANG_XXX) - ucel: aby se nemixovaly ruzne jazyky pri detekci mesicu

public:
    CFTPParser() : Rules(5, 5)
    {
        ActualYear = 0;
        ActualMonth = 0;
        ActualDay = 0;
        ListingBeg = FirstNonEmptyBeg = FirstNonEmptyEnd = LastNonEmptyBeg = LastNonEmptyEnd = NULL;
        ListingIncomplete = FALSE;
        AllowedLanguagesMask = PARSER_LANG_ALL;
    }

    BOOL IsGood() { return Rules.IsGood(); }

    // vraci TRUE pokud se uspesne nakompilovalo pravidlo (az do symbolu ';');
    // pri chybe vraci FALSE a nastavi 'errorResID' (cislo retezce popisujiciho
    // chybu - ulozen v resourcich) nebo 'lowMem' (TRUE = malo pameti)
    BOOL CompileNewRule(const char*& rules, const char* rulesEnd,
                        TIndirectArray<CSrvTypeColumn>* columns,
                        int* errorResID, BOOL* lowMem, BOOL* colAssigned);

    // inicializace parseru - nutne volat pred parsovanim (pred volanim GetNextItemFromListing);
    // 'listingBeg' je zacatek listingu (ukazatel na prvni znak kompletniho listingu);
    // 'listingEnd' je konec listingu; 'actualYear'+'actualMonth'+'actualDay' je rok+mesic+den
    // z dnesniho datumu; 'listingIncomplete' je TRUE pokud je text listingu nekompletni
    // (jeho download byl prerusen)
    void BeforeParsing(const char* listingBeg, const char* listingEnd, int actualYear,
                       int actualMonth, int actualDay, BOOL listingIncomplete);

    // vyparsuje z listingu jeden soubor nebo adresar; ve 'file' (nesmi byt NULL)
    // vraci vysledek; v 'isDir' (nesmi byt NULL) vraci TRUE pokud jde o adresar,
    // FALSE pokud jde o soubor; 'dataIface' je rozhrani pro praci s hodnotami
    // ulozenymi mimo CFileData (pro praci s CFileData::PluginData); 'columns'
    // jsou uzivatelem definovane sloupce; 'listing' je "ukazovatko" v listingu
    // (pozice odkud zacit parsovani), pri navratu z metody ukazuje na zacatek
    // dalsi (prvni nezpracovane) radky; 'listingEnd' je konec listingu;
    // v 'itemStart' (neni-li NULL) vraci ukazatel do bufferu '*listing' na
    // zacatek radky, ze ktere byl vyparsovan vraceny soubor/adresar;
    // 'emptyCol' je predalokovane pomocne pole (minimalne columns->Count prvku);
    // vraci TRUE pri uspechu, vraci FALSE pokud byl dosazen konec listingu (pak se
    // '*listing' rovna 'listingEnd') nebo pokud tento parser neumi zpracovat listing
    // (pak se '*listing' nerovna 'listingEnd') nebo pri chybe alokace pameti
    // (pak je v 'lowMem' (neni-li NULL) TRUE)
    BOOL GetNextItemFromListing(CFileData* file, BOOL* isDir,
                                CFTPListingPluginDataInterface* dataIface,
                                TIndirectArray<CSrvTypeColumn>* columns,
                                const char** listing, const char* listingEnd,
                                const char** itemStart, BOOL* lowMem, DWORD* emptyCol);
};

//
// ****************************************************************************
// CFTPAutodetCondNode
//

// typy funkci v autodetekcni podmince
enum CFTPAutodetCondFunction
{
    acfNone,
    acfAlwaysTrue, // hledani prazdneho retezce = always true
    acfSyst_contains,
    acfWelcome_contains,
    acfReg_exp_in_syst,
    acfReg_exp_in_welcome,
};

// typy pro uzly autodetekcni podminky
enum CFTPAutodetCondNodeType
{
    acntNone,       // neinicializovany
    acntAlwaysTrue, // prazdna podminka = always true
    acntOr,         // OR
    acntAnd,        // AND
    acntNot,        // NOT
    acntFunc,       // funkce s algoritmem hledani retezce (viz CFTPAutodetCondFunction)
};

class CFTPAutodetCondNode
{
public:
    CFTPAutodetCondNodeType Type; // typ uzlu - podle tohoto atributu jsou platne/neplatne dalsi atributy objektu

    union
    {
        struct // levy a pravy operand pro acntOr a acntAnd
        {
            CFTPAutodetCondNode* Left;
            CFTPAutodetCondNode* Right;
        };

        CFTPAutodetCondNode* NegNode; // operand pro acntNot

        struct // typ funkce a algoritmus hledani (Moore/RegExp) pro acntFunc
        {
            CFTPAutodetCondFunction Function;
            void* Algorithm; // bud (CSalamanderBMSearchData *) a nebo (CSalamanderREGEXPSearchData *)
        };
    };

public:
    CFTPAutodetCondNode() { Type = acntNone; }
    ~CFTPAutodetCondNode();

    // vyhodnoceni autodetekcni podminky na zaklade retezcu 'welcomeReply' a 'systReply';
    // vraci vysledek vyrazu podminky (TRUE/FALSE)
    BOOL Evaluate(const char* welcomeReply, int welcomeReplyLen, const char* systReply,
                  int systReplyLen);
};

// naalokuje parser a vytvori jeho strukturu podle retezce 'rules' (pravidla pro parsovani);
// 'columns' jsou definovane sloupce; pri chybe vraci NULL; v 'lowMem' (neni-li NULL) vraci
// TRUE pokud jde o chybu zpusobenou nedostatkem pameti; v 'errorPos' (neni-li NULL) vraci
// offset syntakticke nebo semanticke chyby uvnitr 'rules' (-1=neznama pozice chyby), zaroven
// v 'errorResID' (neni-li NULL) vraci jeste cislo retezce popisujiciho chybu (ulozen
// v resourcich; -1=zadny popis chyby)
CFTPParser* CompileParsingRules(const char* rules, TIndirectArray<CSrvTypeColumn>* columns,
                                int* errorPos, int* errorResID, BOOL* lowMem);

// nacte podminku pro autodetekci z retezce 'cond' a ulozi ji do alokovaneho stromu,
// jehoz koren vraci; pri chybe vraci NULL; v 'lowMem' (neni-li NULL) vraci TRUE pokud jde
// o chybu zpusobenou nedostatkem pameti; v 'errorPos' (neni-li NULL) vraci offset
// syntakticke chyby uvnitr 'cond' (-1=neznama pozice chyby), zaroven v 'errorResID'
// (neni-li NULL) vraci jeste cislo retezce popisujiciho chybu (ulozen v resourcich;
// -1=zadny popis chyby) a v bufferu 'errBuf'+'errBufSize' vraci textovy popis chyby
// (ma vetsi prioritu nez 'errorResID')
CFTPAutodetCondNode* CompileAutodetectCond(const char* cond, int* errorPos, int* errorResID,
                                           BOOL* lowMem, char* errBuf, int errBufSize);

// doplni prazdne hodnoty do prazdnych sloupcu; pri chybe vraci v 'err' TRUE;
// 'file'+'isDir'+'dataIface' jsou data zpracovavane polozky (souboru/adresare);
// 'columns' jsou sloupce v panelu; v 'lowMem' (neni-li NULL) se vraci TRUE
// pri chybe zpusobene nedostatkem pameti; 'emptyCol' je pole DWORDu pro vsechny
// sloupce, TRUE na indexu znamena, ze je sloupec prazdny; je-li 'emptyCol' NULL,
// je naplnene jen jmeno (Name+NameLen+Hidden), zbytek je prazdny a doplni se
// z empty-values; 'actualYear'+'actualMonth'+'actualDay' je rok+mesic+den
// z dnesniho datumu (pouziva se pro korekci roku u datumu vznikleho pouzitim
// "year_or_time")
void FillEmptyValues(BOOL& err, CFileData* file, BOOL isDir,
                     CFTPListingPluginDataInterface* dataIface,
                     TIndirectArray<CSrvTypeColumn>* columns,
                     BOOL* lowMem, DWORD* emptyCol, int actualYear,
                     int actualMonth, int actualDay);
