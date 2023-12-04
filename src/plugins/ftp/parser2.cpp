// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CFTPParser
//

void CFTPParser::BeforeParsing(const char* listingBeg, const char* listingEnd, int actualYear,
                               int actualMonth, int actualDay, BOOL listingIncomplete)
{
    ActualYear = actualYear;
    ActualMonth = actualMonth;
    ActualDay = actualDay;
    FirstNonEmptyBeg = FirstNonEmptyEnd = LastNonEmptyBeg = LastNonEmptyEnd = NULL;
    ListingBeg = listingBeg;
    ListingIncomplete = listingIncomplete;
    SkipThisLineItIsIncomlete = FALSE;
    AllowedLanguagesMask = PARSER_LANG_ALL;

    // predpocitame prvni neprazdnou radku
    const char* s = listingBeg;
    const char* beg = s;
    BOOL nonEmpty = FALSE;
    while (s < listingEnd)
    {
        if (*s == '\r' || *s == '\n')
        {
            if (*s == '\r' && s + 1 < listingEnd && *(s + 1) == '\n')
                s++;
            if (nonEmpty)
            {
                FirstNonEmptyBeg = beg;
                FirstNonEmptyEnd = s + 1;
                nonEmpty = FALSE;
                break;
            }
            beg = s + 1;
        }
        else
        {
            if (*s > ' ')
                nonEmpty = TRUE;
        }
        s++;
    }
    if (nonEmpty)
    {
        FirstNonEmptyBeg = beg;
        FirstNonEmptyEnd = listingEnd + 1; // aby to fungovalo i po preskoceni posledniho znaku listingu (posledni radka neukoncena EOLem)
    }

    // predpocitame posledni neprazdnou radku
    s = listingEnd - 1;
    const char* end = s + 1;
    nonEmpty = FALSE;
    BOOL lastRowHasEOL = FALSE;
    while (s >= listingBeg)
    {
        if (*s == '\r' || *s == '\n')
        {
            if (nonEmpty)
            {
                LastNonEmptyBeg = s + 1;
                LastNonEmptyEnd = end;
                if (!lastRowHasEOL && LastNonEmptyEnd == listingEnd)
                    LastNonEmptyEnd++; // aby to fungovalo i po preskoceni posledniho znaku listingu (posledni radka neukoncena EOLem)
                nonEmpty = FALSE;
                break;
            }
            end = s + 1;
            lastRowHasEOL = TRUE;
            if (*s == '\n' && s - 1 >= listingBeg && *(s - 1) == '\r')
                s--;
        }
        else
        {
            if (*s > ' ')
                nonEmpty = TRUE;
        }
        s--;
    }
    if (nonEmpty)
    {
        LastNonEmptyBeg = listingBeg;
        LastNonEmptyEnd = end;
        if (!lastRowHasEOL && LastNonEmptyEnd == listingEnd)
            LastNonEmptyEnd++; // aby to fungovalo i po preskoceni posledniho znaku listingu (posledni radka neukoncena EOLem)
    }
}

void FillEmptyValues(BOOL& err, CFileData* file, BOOL isDir,
                     CFTPListingPluginDataInterface* dataIface,
                     TIndirectArray<CSrvTypeColumn>* columns,
                     BOOL* lowMem, DWORD* emptyCol, int actualYear,
                     int actualMonth, int actualDay)
{
    CQuadWord qwVal;
    __int64 int64Val;
    SYSTEMTIME stVal;
    BOOL lastWriteUsed = FALSE;
    SYSTEMTIME lastWriteDate;
    lastWriteDate.wYear = 1602;
    lastWriteDate.wMonth = 1;
    lastWriteDate.wDay = 1;
    lastWriteDate.wHour = 0;
    lastWriteDate.wMinute = 0;
    lastWriteDate.wSecond = 0;
    lastWriteDate.wMilliseconds = 0;
    SYSTEMTIME testDate;
    testDate.wHour = 0;
    testDate.wMinute = 0;
    testDate.wSecond = 0;
    testDate.wMilliseconds = 0;
    FILETIME ft;
    int i;
    for (i = 1; i < columns->Count; i++)
    {
        CSrvTypeColumn* col = columns->At(i);
        if (emptyCol == NULL || emptyCol[i])
        {
            if (GetColumnEmptyValue(col->EmptyValue, col->Type, &qwVal, &int64Val, &stVal, TRUE))
            {
                switch (col->Type)
                {
                // case stctName:   // ten to byt nemuze, muze byt jen v prvnim sloupci
                case stctGeneralText:
                {
                    char* str = SalamanderGeneral->DupStr(col->EmptyValue);
                    if (str == NULL && col->EmptyValue != NULL)
                    {
                        if (lowMem != NULL)
                            *lowMem = TRUE;
                        err = TRUE;
                        break;
                    }
                    dataIface->StoreStringToColumn(*file, i, str);
                    break;
                }

                case stctExt:                           // pripona se nemuze nikdy priradit pri parsovani -> tato cast se provadi
                {                                       //  vzdycky, kdyz existuje sloupec s typem "Extension"
                    if (SortByExtDirsAsFiles || !isDir) // u adresaru se pripona nerozpoznava
                    {
                        char* t = file->Ext; // uz se provedlo: file->Ext = file->Name + file->NameLen
                        while (--t >= file->Name && *t != '.')
                            ;
                        //              if (t > file->Name) file->Ext = t + 1;   // ".cvspass" ve Windows je pripona ...
                        if (t >= file->Name)
                            file->Ext = t + 1;
                    }
                    break;
                }

                    // case stctType: // ignorujeme, neni co nastavovat

                case stctSize:
                {
                    if (isDir)
                        file->Size = CQuadWord(0, 0); // u adresaru velikost pro poradek nulujeme (zbytecne)
                    else
                        file->Size = qwVal;
                    break;
                }

                case stctGeneralNumber:
                    dataIface->StoreNumberToColumn(*file, i, int64Val);
                    break;

                case stctDate:
                {
                    lastWriteUsed = TRUE;
                    if (emptyCol != NULL &&
                        (emptyCol[i] & DATE_MASK_DATE) == DATE_MASK_DATE) // datum byl nacten komplet
                    {
                        CFTPDate* d = (CFTPDate*)&(file->LastWrite.dwLowDateTime);
                        lastWriteDate.wYear = d->Year;
                        lastWriteDate.wMonth = d->Month;
                        lastWriteDate.wDay = d->Day;
                        if (emptyCol[i] & DATE_MASK_YEARCORRECTIONNEEDED)
                        {
                            if (lastWriteDate.wMonth > actualMonth ||
                                lastWriteDate.wMonth == actualMonth && lastWriteDate.wDay > actualDay)
                            { // datum v budoucnosti je nesmysl (UNIX u takoveho pise rovnou rok a ne
                                // cas) -> jde o datum z minuleho roku
                                lastWriteDate.wYear--;
                            }
                        }
                    }
                    else // datum neni kompletni -> bereme prazdnou hodnotu
                    {
                        lastWriteDate.wYear = stVal.wYear;
                        lastWriteDate.wMonth = stVal.wMonth;
                        lastWriteDate.wDay = stVal.wDay;
                    }
                    break;
                }

                case stctTime:
                {
                    lastWriteUsed = TRUE;
                    if (emptyCol != NULL &&
                        (emptyCol[i] & DATE_MASK_TIME) == DATE_MASK_TIME) // cas byl nacten
                    {
                        CFTPTime* t = (CFTPTime*)&(file->LastWrite.dwHighDateTime);
                        lastWriteDate.wHour = t->Hour;
                        lastWriteDate.wMinute = t->Minute;
                        lastWriteDate.wSecond = t->Second;
                        lastWriteDate.wMilliseconds = t->Millisecond;
                    }
                    else // cas nebyl nacten -> bereme prazdnou hodnotu
                    {
                        lastWriteDate.wHour = stVal.wHour;
                        lastWriteDate.wMinute = stVal.wMinute;
                        lastWriteDate.wSecond = stVal.wSecond;
                        lastWriteDate.wMilliseconds = stVal.wMilliseconds;
                    }
                    break;
                }

                case stctGeneralDate:
                {
                    if (emptyCol == NULL ||
                        (emptyCol[i] & DATE_MASK_DATE) != DATE_MASK_DATE) // datum neni kompletni -> bereme prazdnou hodnotu
                    {
                        dataIface->StoreDateToColumn(*file, i, (BYTE)stVal.wDay, (BYTE)stVal.wMonth, stVal.wYear);
                    }
                    else
                    {
                        dataIface->GetDateFromColumn(*file, i, &testDate);
                        if (emptyCol[i] & DATE_MASK_YEARCORRECTIONNEEDED)
                        {
                            if (testDate.wMonth > actualMonth ||
                                testDate.wMonth == actualMonth && testDate.wDay > actualDay)
                            { // datum v budoucnosti je nesmysl (UNIX u takoveho pise rovnou rok a ne
                                // cas) -> jde o datum z minuleho roku
                                testDate.wYear--;
                                dataIface->StoreYearToColumn(*file, i, testDate.wYear);
                            }
                        }
                        if (!SystemTimeToFileTime(&testDate, &ft)) // invalidni datum -> nastavime 1.1.1602
                            dataIface->StoreDateToColumn(*file, i, 1, 1, 1602);
                    }
                    break;
                }

                case stctGeneralTime:
                {
                    if (emptyCol == NULL ||
                        (emptyCol[i] & DATE_MASK_TIME) != DATE_MASK_TIME) // cas nebyl nacten -> bereme prazdnou hodnotu
                    {
                        dataIface->StoreTimeToColumn(*file, i, (BYTE)stVal.wHour, (BYTE)stVal.wMinute,
                                                     (BYTE)stVal.wSecond, stVal.wMilliseconds);
                    }
                    break;
                }
                }
                if (err)
                    break;
            }
        }
    }
    if (!err && lastWriteUsed) // nastaveni file->LastWrite podle hodnot ve sloupcich
    {
        if (!SystemTimeToFileTime(&lastWriteDate, &ft) ||
            !LocalFileTimeToFileTime(&ft, &file->LastWrite))
        { // chyba nastaveni datumu -> invalidni datum (cas nemuze byt spatne) -> nastavime 1.1.1602
            lastWriteDate.wYear = 1602;
            lastWriteDate.wMonth = 1;
            lastWriteDate.wDay = 1;
            SystemTimeToFileTime(&lastWriteDate, &ft);
            LocalFileTimeToFileTime(&ft, &file->LastWrite);
        }
    }
}

BOOL CFTPParser::GetNextItemFromListing(CFileData* file, BOOL* isDir,
                                        CFTPListingPluginDataInterface* dataIface,
                                        TIndirectArray<CSrvTypeColumn>* columns,
                                        const char** listing, const char* listingEnd,
                                        const char** itemStart, BOOL* lowMem, DWORD* emptyCol)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CFTPParser::GetNextItemFromListing()");
    // nastavime defaultni hodnoty (soubor a neni skryty)
    *isDir = FALSE;
    memset(file, 0, sizeof(CFileData));

    if (lowMem != NULL)
        *lowMem = FALSE;
    BOOL err = FALSE;
    if (!dataIface->AllocPluginData(*file))
    {
        if (lowMem != NULL)
            *lowMem = TRUE;
        err = TRUE;
    }

    if (!err)
    {
        // pro kazdy sloupec obsahuje pole 'emptyCol' TRUE dokud se do sloupce nepriradi nejaka hodnota
        int i;
        for (i = 0; i < columns->Count; i++)
            emptyCol[i] = TRUE;

        // provadeni parsingu
        const char* s = *listing;
        while (s < listingEnd)
        {
            const char* restart = s;
            if (itemStart != NULL)
                *itemStart = s;
            int j;
            for (j = 0; j < Rules.Count; j++)
            {
                BOOL brk = FALSE;
                if (Rules[j]->UseRule(file, isDir, dataIface, columns, &s, listingEnd, this, &err, emptyCol))
                {
                    if (SkipThisLineItIsIncomlete || // zjisten nekompletni listing - preskocime zpracovavanou koncovou cast listingu
                        !emptyCol[0])
                        break;  // pravidlo uspelo - nacteni souboru nebo adresare - jdeme zpracovat data
                    brk = TRUE; // pravidlo uspelo - preskok radky - jdeme parsovat dalsi radek
                }
                else
                {
                    if (err) // chyba - nedostatek pameti
                    {
                        if (lowMem != NULL)
                            *lowMem = TRUE;
                        break;
                    }
                    // chyba - pravidlo nelze pouzit - jdeme zkusit dalsi pravidlo na stejnem textu
                    s = restart;
                }

                // provedeme uvolneni a nulovani dat pred jejich dalsim plnenim
                dataIface->ClearPluginData(*file);
                if (file->Name != NULL)
                    SalamanderGeneral->Free(file->Name);
                DWORD_PTR backupPD = file->PluginData;
                memset(file, 0, sizeof(CFileData));
                file->PluginData = backupPD;
                *isDir = FALSE;
                int x;
                for (x = 0; x < columns->Count; x++)
                    emptyCol[x] = TRUE;

                if (brk)
                    break;
            }
            if (j == Rules.Count) // na dany text nelze pouzit zadne pravidlo, vracime chybu
            {
                // s = restart;  // pozice chyby (uz je prirazena)
                break;
            }
            if (err || SkipThisLineItIsIncomlete || !emptyCol[0])
                break; // chyba nebo mame vysledek
        }
        *listing = s;
    }

    BOOL ret = FALSE;
    if (!SkipThisLineItIsIncomlete &&
        !err && !emptyCol[0] && file->Name != NULL /* always true */) // doslo k naplneni Name -> mame soubor/adresar
    {
        // nastavime zatim ignorovane slozky 'file'
        file->NameLen = strlen(file->Name);
        file->Ext = file->Name + file->NameLen;

        // doplnime prazdne hodnoty do prazdnych sloupcu
        FillEmptyValues(err, file, *isDir, dataIface, columns, lowMem, emptyCol, ActualYear,
                        ActualMonth, ActualDay);

        if (!err)
            ret = TRUE; // uspesne jsme ziskali dalsi soubor nebo adresar
    }
    if (!ret) // doslo k chybe nebo jiz nebyl nalezen zadny soubor/adresar, musime uvonit alokovanou pamet
    {
        dataIface->ReleasePluginData(*file, *isDir);
        if (file->Name != NULL)
            SalamanderGeneral->Free(file->Name);
        if (SkipThisLineItIsIncomlete)
            *listing = listingEnd; // uz by melo byt, ale pro jistotu...
    }
    return ret;
}

//
// ****************************************************************************
// CFTPParserRule
//

BOOL CFTPParserRule::UseRule(CFileData* file, BOOL* isDir,
                             CFTPListingPluginDataInterface* dataIface,
                             TIndirectArray<CSrvTypeColumn>* columns,
                             const char** listing, const char* listingEnd,
                             CFTPParser* actualParser, BOOL* lowMemErr, DWORD* emptyCol)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CFTPParserRule::UseRule()");
    const char* s = *listing;
    int i;
    for (i = 0; !(actualParser->SkipThisLineItIsIncomlete) && i < Functions.Count; i++)
    {
        if (!Functions[i]->UseFunction(file, isDir, dataIface, columns, &s, listingEnd,
                                       actualParser, lowMemErr, emptyCol))
            break;
    }
    // jsou-li uspesne pouzite vsechny funkce pravidla a "ukazovatko" je na konci
    // listingu nebo na konci radky, hlasime uspech
    BOOL ret = FALSE;
    if (file->Name == NULL || file->Name[0] != 0) // prirazeni prazdneho retezce do Name je chyba parsovani
    {
        if (actualParser->SkipThisLineItIsIncomlete)
            ret = TRUE;
        else
        {
            if (!(*lowMemErr) && i == Functions.Count && (s == listingEnd || *s == '\r' || *s == '\n'))
            {
                ret = TRUE;
                // preskocime pripadny konec radky
                if (s < listingEnd && *s == '\r')
                    s++;
                if (s < listingEnd && *s == '\n')
                    s++;
            }
        }
    }
    else
        actualParser->SkipThisLineItIsIncomlete = FALSE; // i kdyby byla radka nekompletni, doslo k chybe -> budeme to dale ignorovat
    *listing = s;
    return ret;
}

//
// ****************************************************************************
// CFTPParserFunction
//

BOOL AssignDayToColumn(int col, TIndirectArray<CSrvTypeColumn>* columns, int day,
                       DWORD* emptyCol, CFileData* file, CFTPListingPluginDataInterface* dataIface)
{
    if (col >= 0 && col < columns->Count) // "always true"
    {
        emptyCol[col] |= DATE_MASK_DAY; // priORuju den
        switch (columns->At(col)->Type)
        {
        case stctDate:
            ((CFTPDate*)&(file->LastWrite.dwLowDateTime))->Day = day;
            return TRUE;
        case stctGeneralDate:
            dataIface->StoreDayToColumn(*file, col, (BYTE)day);
            return TRUE;
        default:
            TRACE_E("AssignDayToColumn(): Invalid date column type!");
            return FALSE;
        }
    }
    else
    {
        TRACE_E("AssignDayToColumn(): Invalid column index!");
        return FALSE;
    }
}

BOOL AssignMonthToColumn(int col, TIndirectArray<CSrvTypeColumn>* columns, int month,
                         DWORD* emptyCol, CFileData* file, CFTPListingPluginDataInterface* dataIface)
{
    if (col >= 0 && col < columns->Count) // "always true"
    {
        emptyCol[col] |= DATE_MASK_MONTH; // priORuju mesic
        switch (columns->At(col)->Type)
        {
        case stctDate:
            ((CFTPDate*)&(file->LastWrite.dwLowDateTime))->Month = month;
            return TRUE;
        case stctGeneralDate:
            dataIface->StoreMonthToColumn(*file, col, (BYTE)month);
            return TRUE;
        default:
            TRACE_E("AssignMonthToColumn(): Invalid date column type!");
            return FALSE;
        }
    }
    else
    {
        TRACE_E("AssignMonthToColumn(): Invalid column index!");
        return FALSE;
    }
}

BOOL AssignYearToColumn(int col, TIndirectArray<CSrvTypeColumn>* columns, int year,
                        DWORD* emptyCol, CFileData* file, CFTPListingPluginDataInterface* dataIface,
                        BOOL yearCorrectionNeeded)
{
    if (col >= 0 && col < columns->Count) // "always true"
    {
        // priORuju rok + korekci roku (potreba pro "year_or_time" - cas u souboru mladsich sesti
        // mesicu - nelze jen brat aktualni rok)
        emptyCol[col] |= DATE_MASK_YEAR | (yearCorrectionNeeded ? DATE_MASK_YEARCORRECTIONNEEDED : 0);
        switch (columns->At(col)->Type)
        {
        case stctDate:
            ((CFTPDate*)&(file->LastWrite.dwLowDateTime))->Year = year;
            return TRUE;
        case stctGeneralDate:
            dataIface->StoreYearToColumn(*file, col, (WORD)year);
            return TRUE;
        default:
            TRACE_E("AssignYearToColumn(): Invalid date column type!");
            return FALSE;
        }
    }
    else
    {
        TRACE_E("AssignYearToColumn(): Invalid column index!");
        return FALSE;
    }
}

BOOL AssignDateToColumn(int col, TIndirectArray<CSrvTypeColumn>* columns, CFTPDate* date,
                        DWORD* emptyCol, CFileData* file, CFTPListingPluginDataInterface* dataIface)
{
    if (col >= 0 && col < columns->Count) // "always true"
    {
        emptyCol[col] |= DATE_MASK_DATE; // nastavuje se kompletni datum
        switch (columns->At(col)->Type)
        {
        case stctDate:
            *((CFTPDate*)&(file->LastWrite.dwLowDateTime)) = *date;
            return TRUE;
        case stctGeneralDate:
            dataIface->StoreDateToColumn(*file, col, date->Day, date->Month, date->Year);
            return TRUE;
        default:
            TRACE_E("AssignDateToColumn(): Invalid date column type!");
            return FALSE;
        }
    }
    else
    {
        TRACE_E("AssignDateToColumn(): Invalid column index!");
        return FALSE;
    }
}

BOOL AssignTimeToColumn(int col, TIndirectArray<CSrvTypeColumn>* columns, int hour, int minute,
                        int second, int millisecond, DWORD* emptyCol, CFileData* file,
                        CFTPListingPluginDataInterface* dataIface)
{
    if (col >= 0 && col < columns->Count) // "always true"
    {
        emptyCol[col] |= DATE_MASK_TIME; // priORuju cas
        switch (columns->At(col)->Type)
        {
        case stctTime:
        {
            CFTPTime* t = (CFTPTime*)&(file->LastWrite.dwHighDateTime);
            t->Hour = hour;
            t->Minute = minute;
            t->Second = second;
            t->Millisecond = millisecond;
            return TRUE;
        }

        case stctGeneralTime:
        {
            dataIface->StoreTimeToColumn(*file, col, (BYTE)hour, (BYTE)minute, (BYTE)second, (WORD)millisecond);
            return TRUE;
        }

        default:
            TRACE_E("AssignTimeToColumn(): Invalid time column type!");
            return FALSE;
        }
    }
    else
    {
        TRACE_E("AssignTimeToColumn(): Invalid column index!");
        return FALSE;
    }
}

BOOL AssignNumberToColumn(int col, TIndirectArray<CSrvTypeColumn>* columns, BOOL minus, __int64 number,
                          DWORD* emptyCol, CFileData* file, CFTPListingPluginDataInterface* dataIface,
                          BOOL onlyPositiveNumber)
{
    if (col >= 0 && col < columns->Count) // "always true"
    {
        emptyCol[col] = FALSE;
        switch (columns->At(col)->Type)
        {
        case stctSize:
        {
            if (minus)
            {
                if (onlyPositiveNumber)
                    file->Size.Set(0, 0); // funkce "positive_number" - pro zaporne hodnoty uklada defaultni prazdnou hodnotu
                else
                    return FALSE; // do Size nelze ulozit zaporne cislo (nemuzeme ignorovat -> user by ziral -666 B =~ 18 EB)
            }
            else
                file->Size.SetUI64((unsigned __int64)number);
            break;
        }

        case stctGeneralNumber:
        {
            if (minus && onlyPositiveNumber)
                dataIface->StoreNumberToColumn(*file, col, INT64_EMPTYNUMBER); // funkce "positive_number" - pro zaporne hodnoty uklada defaultni prazdnou hodnotu
            else
                dataIface->StoreNumberToColumn(*file, col, number);
            break;
        }

        default:
        {
            TRACE_E("AssignNumberToColumn(): Invalid number column type!");
            return FALSE;
        }
        }
        return TRUE;
    }
    else
    {
        TRACE_E("AssignNumberToColumn(): Invalid column index!");
        return FALSE;
    }
}

BOOL AssignStringToColumn(int col, TIndirectArray<CSrvTypeColumn>* columns, const char* beg,
                          const char* end, BOOL* lowMemErr, DWORD* emptyCol,
                          CFileData* file, CFTPListingPluginDataInterface* dataIface)
{
    if (col >= 0 && col < columns->Count) // "always true"
    {
        char* str = (char*)SalamanderGeneral->Alloc((int)(end - beg) + 1);
        if (str == NULL)
        {
            TRACE_E(LOW_MEMORY);
            *lowMemErr = TRUE;
            return FALSE;
        }
        memcpy(str, beg, end - beg);
        str[end - beg] = 0;

        emptyCol[col] = FALSE;
        switch (columns->At(col)->Type)
        {
        case stctName:
        {
            if (file->Name != NULL)
                SalamanderGeneral->Free(file->Name);
            file->Name = str;
            if (end - beg > MAX_PATH - 5)
            {
                file->Name[MAX_PATH - 5] = 0; // file->Name muze byt dlouhe max. MAX_PATH - 5 (omezeni Salamandera - pro viewovani snad moc nevadi, operace bude muset user provest v jinem softu)
                TRACE_E("Too long file or directory name, cutting to MAX_PATH-5 characters! Using name: " << file->Name);
            }
            break;
        }

        case stctGeneralText:
            dataIface->StoreStringToColumn(*file, col, str);
            break;

        default:
        {
            SalamanderGeneral->Free(str);
            TRACE_E("AssignStringToColumn(): Invalid string column type!");
            return FALSE;
        }
        }
        return TRUE;
    }
    else
    {
        TRACE_E("AssignStringToColumn(): Invalid column index!");
        *lowMemErr = TRUE; // fatal error - simulace "low memory"
        return FALSE;
    }
}

// vraci cislo mesice jehoz nazev je kodovany tremi pismeny; pokud jde o neznamy
// kod mesice, vraci -1
int GetMonthFromThreeLetters(const char* month, const char* monthStr)
{
    char firstLetter = LowerCase[*month++];
    char secondLetter = LowerCase[*month++];
    char thirdLetter = LowerCase[*month];
    int i;
    for (i = 0; i < 12; i++)
    {
        if (*monthStr == firstLetter &&
            *(monthStr + 1) == secondLetter &&
            *(monthStr + 2) == thirdLetter)
            return i + 1;
        monthStr += 4;
    }
    return -1;
}

struct CMonthNameNumberLanguage
{
    const char* Name;
    int Number;
    DWORD AllowedLanguagesMask;
};

// predpocitana tabulka pro hledani cisla mesice + masky povolenych jazyku
// POZOR: mesice musi byt malymi pismeny!
// PARSER_LANG_ENGLISH:   "jan feb mar apr may jun jul aug sep oct nov dec"
// PARSER_LANG_GERMAN:    "jan feb mär apr mai jun jul aug sep okt nov dez"
// PARSER_LANG_NORWEIGAN: "jan feb mar apr mai jun jul aug sep okt nov des"
// PARSER_LANG_SWEDISH:   "jan feb mar apr maj jun jul aug sep okt nov dec"
CMonthNameNumberLanguage MonthNameNumberLanguageArr[] =
    {
        {"jan", 1, PARSER_LANG_ENGLISH | PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"feb", 2, PARSER_LANG_ENGLISH | PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"mar", 3, PARSER_LANG_ENGLISH | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"apr", 4, PARSER_LANG_ENGLISH | PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"may", 5, PARSER_LANG_ENGLISH},
        {"jun", 6, PARSER_LANG_ENGLISH | PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"jul", 7, PARSER_LANG_ENGLISH | PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"aug", 8, PARSER_LANG_ENGLISH | PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"sep", 9, PARSER_LANG_ENGLISH | PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"oct", 10, PARSER_LANG_ENGLISH},
        {"nov", 11, PARSER_LANG_ENGLISH | PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"dec", 12, PARSER_LANG_ENGLISH | PARSER_LANG_SWEDISH},
        {"mär", 3, PARSER_LANG_GERMAN},
        {"mai", 5, PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN},
        {"okt", 10, PARSER_LANG_GERMAN | PARSER_LANG_NORWEIGAN | PARSER_LANG_SWEDISH},
        {"dez", 12, PARSER_LANG_GERMAN},
        {"des", 12, PARSER_LANG_NORWEIGAN},
        {"maj", 5, PARSER_LANG_SWEDISH},
        {NULL, -1, 0}};

// vraci cislo mesice jehoz nazev je kodovany tremi pismeny, kontroluje jestli jde
// o nazev mesice v povolenem jazyce a pripadne omezi povolene jazyky pro dalsi
// rozpoznavani jazyku; pokud jde o neznamy mesic nebo mesic v nepovolenem jazyce,
// vraci -1
int GetMonthFromThreeLettersAllLangs(const char* month, DWORD* allowedLanguagesMask)
{
    int res = -1;
    DWORD mask = 0; // maska jazyku, ve kterych se tento mesic jmenuje stejne
    char lowerMonth[4];
    lowerMonth[0] = LowerCase[month[0]];
    lowerMonth[1] = LowerCase[month[1]];
    lowerMonth[2] = LowerCase[month[2]];
    lowerMonth[3] = 0;
    CMonthNameNumberLanguage* i = MonthNameNumberLanguageArr;
    while (i->Name != NULL)
    {
        if (strcmp(lowerMonth, i->Name) == 0)
        {
            res = i->Number;
            mask = i->AllowedLanguagesMask;
            break;
        }
        i++;
    }
    if (res == -1 || (*allowedLanguagesMask & mask) == 0)
        return -1; // chyba (nenalezeny mesic nebo mesic z nepovoleneho jazyka)
    else
    {
        *allowedLanguagesMask &= mask;
        return res;
    }
}

// vraci cislo mesice jehoz nazev je kodovany textem; pokud jde o neznamy
// kod mesice, vraci -1
int GetMonthFromText(const char** month, const char* monthEnd, const char* monthStr)
{
    const char* s = *month;
    const char* monthStrEnd = monthStr;
    int m;
    for (m = 1; m <= 12; m++)
    {
        while (*monthStrEnd != 0 && *monthStrEnd != ' ')
            monthStrEnd++;
        if (monthStrEnd - monthStr <= monthEnd - s &&
            SalamanderGeneral->StrNICmp(s, monthStr, (int)(monthStrEnd - monthStr)) == 0)
        {
            *month = s + (monthStrEnd - monthStr);
            return m;
        }
        if (*monthStrEnd != 0)
            monthStrEnd++;
        monthStr = monthStrEnd;
    }
    return -1;
}

struct CMonthTxtNameNumberLanguage
{
    const char* Name;
    int NameLen;
    int Number;
    DWORD AllowedLanguagesMask;
};

// predpocitana tabulka pro hledani cisla mesice + masky povolenych jazyku
// PARSER_LANG_GERMAN:  "Jan. Feb. März Apr. Mai Juni Juli Aug. Sept. Okt. Nov. Dez."
CMonthTxtNameNumberLanguage MonthTxtNameNumberLanguageArr[] =
    {
        {"Jan.", 4, 1, PARSER_LANG_GERMAN},
        {"Feb.", 4, 2, PARSER_LANG_GERMAN},
        {"März", 4, 3, PARSER_LANG_GERMAN},
        {"Apr.", 4, 4, PARSER_LANG_GERMAN},
        {"Mai", 3, 5, PARSER_LANG_GERMAN},
        {"Juni", 4, 6, PARSER_LANG_GERMAN},
        {"Juli", 4, 7, PARSER_LANG_GERMAN},
        {"Aug.", 4, 8, PARSER_LANG_GERMAN},
        {"Sept.", 5, 9, PARSER_LANG_GERMAN},
        {"Okt.", 4, 10, PARSER_LANG_GERMAN},
        {"Nov.", 4, 11, PARSER_LANG_GERMAN},
        {"Dez.", 4, 12, PARSER_LANG_GERMAN},
        {NULL, -1, 0}};

// vraci cislo mesice jehoz nazev je text '*month', kontroluje jestli jde
// o nazev mesice v povolenem jazyce a pripadne omezi povolene jazyky pro dalsi
// rozpoznavani jazyku; pokud jde o neznamy mesic nebo mesic v nepovolenem jazyce,
// vraci -1
int GetMonthFromTextAllLangs(const char** month, const char* monthEnd, DWORD* allowedLanguagesMask)
{
    const char* s = *month;
    int res = -1;
    DWORD mask = 0; // maska jazyku, ve kterych se tento mesic jmenuje stejne
    int monthLen = 0;
    CMonthTxtNameNumberLanguage* i = MonthTxtNameNumberLanguageArr;
    while (i->Name != NULL)
    {
        if (i->NameLen <= monthEnd - s && SalamanderGeneral->StrNICmp(s, i->Name, i->NameLen) == 0)
        {
            res = i->Number;
            mask = i->AllowedLanguagesMask;
            monthLen = i->NameLen;
            break;
        }
        i++;
    }
    if (res == -1 || (*allowedLanguagesMask & mask) == 0)
        return -1; // chyba (nenalezeny mesic nebo mesic z nepovoleneho jazyka)
    else
    {
        *allowedLanguagesMask &= mask;
        *month += monthLen;
        return res;
    }
}

BOOL CFTPParserFunction::UseFunction(CFileData* file, BOOL* isDir,
                                     CFTPListingPluginDataInterface* dataIface,
                                     TIndirectArray<CSrvTypeColumn>* columns, const char** listing,
                                     const char* listingEnd, CFTPParser* actualParser,
                                     BOOL* lowMemErr, DWORD* emptyCol)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE2("CFTPParserFunction::UseFunction(%d)", (int)Function);
    BOOL ret = TRUE;
    const char* s = *listing;
    switch (Function)
    {
    case fpfSkip_white_spaces: // preskocime white-spaces (krome CR a LF)
    {
        while (s < listingEnd && (*s <= ' ' && *s != '\r' && *s != '\n'))
            s++;
        break;
    }

    case fpfSkip_to_number: // preskocime az k nejblizsi dekadicke cislici (krome CR a LF)
    {
        while (s < listingEnd && ((*s < '0' || *s > '9') && *s != '\r' && *s != '\n'))
            s++;
        break;
    }

    case fpfWhite_spaces: // preskocime white-spaces (krome CR a LF)
    {
        if (Parameters.Count > 0)
        {
            int num = (int)Parameters[0]->GetNumber();
            while (num-- && s < listingEnd && (*s <= ' ' && *s != '\r' && *s != '\n'))
                s++;
            if (num != -1)
                ret = FALSE; // uspech jen pri posunu "ukazovatka" o 'num'
        }
        else
        {
            while (s < listingEnd && (*s <= ' ' && *s != '\r' && *s != '\n'))
                s++;
            if (s == *listing)
                ret = FALSE; // uspech jen pri posunu "ukazovatka"
        }
        break;
    }

    case fpfWhite_spaces_and_line_ends: // preskocime white-spaces (vcetne CR a LF)
    {
        while (s < listingEnd && *s <= ' ')
            s++;
        if (s == *listing)
            ret = FALSE; // uspech jen pri posunu "ukazovatka"
        else
        {
            if (s == listingEnd) // uspech jen pokud se ukazovatko zastavilo na nejakem znaku a ne na konci listingu
            {
                if (actualParser->ListingIncomplete)
                { // listing je nekompletni -> predpokladame, ze kdyby byl kompletni, toto se
                    // nestane -> skipneme koncovou cast listingu zpracovanou timto pravidlem
                    actualParser->SkipThisLineItIsIncomlete = TRUE;
                }
                else
                    ret = FALSE;
            }
        }
        break;
    }

    case fpfRest_of_line: // zbytek radky az do CR nebo LF
    {
        while (s < listingEnd && *s != '\r' && *s != '\n')
            s++;
        if (s == *listing)
            ret = FALSE; // uspech jen pri posunu "ukazovatka"
        else
        {
            if (Parameters.Count > 0 && // provedeme jeste prirazeni do sloupce
                !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                      s, lowMemErr, emptyCol, file, dataIface))
            {
                ret = FALSE;
            }
        }
        break;
    }

    case fpfWord: // slovo (do nejblizsiho white-space nebo konce radky)
    {
        while (s<listingEnd&& * s> ' ')
            s++;
        if (s == *listing)
            ret = FALSE; // uspech jen pri posunu "ukazovatka"
        else
        {
            if (Parameters.Count > 0 && // provedeme jeste prirazeni do sloupce
                !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                      s, lowMemErr, emptyCol, file, dataIface))
            {
                ret = FALSE;
            }
        }
        break;
    }

    case fpfNumber:         // cislo (jen '+'/'-' a cislice)
    case fpfPositiveNumber: // jen kladne cislo (jen '+'/'-' a cislice) - najde-li zaporne cislo, priradi defaultni prazdnou hodnotu
    {
        BOOL minus = FALSE;
        if (s < listingEnd)
        {
            if (*s == '+')
                s++;
            else
            {
                if (*s == '-')
                {
                    s++;
                    minus = TRUE;
                }
            }
        }
        const char* beg = s;
        __int64 num = 0;
        while (s < listingEnd && *s >= '0' && *s <= '9')
            num = num * 10 + (*s++ - '0');
        if (minus)
            num = -num;
        if (s == beg || s < listingEnd && IsCharAlpha(*s))
            ret = FALSE; // uspech jen pokud cislo existuje (aspon jedna cislice) a neni ukoncene pismenem
        else
        {
            if (Parameters.Count > 0 && // provedeme jeste prirazeni do sloupce
                !AssignNumberToColumn(Parameters[0]->GetColumnIndex(), columns, minus, num,
                                      emptyCol, file, dataIface, Function == fpfPositiveNumber))
            {
                ret = FALSE;
            }
        }
        break;
    }

    case fpfNumber_with_separators: // cislo se separatory
    {
        BOOL needDealloc = FALSE;
        const char* sep = Parameters.Count > 1 ? Parameters[1]->GetString(s, listingEnd, &needDealloc, lowMemErr) : NULL;
        if (*lowMemErr)
        {
            ret = FALSE;
            break;
        }
        if (sep == NULL)
            sep = "";
        // preskocime separatory krome '+', '-' a cislic
        while (s < listingEnd && *s != '+' && *s != '-' && (*s < '0' || *s > '9') && strchr(sep, *s) != NULL)
            s++;
        BOOL minus = FALSE;
        if (s < listingEnd)
        {
            if (*s == '+')
                s++;
            else
            {
                if (*s == '-')
                {
                    s++;
                    minus = TRUE;
                }
            }
        }
        __int64 num = 0;
        while (s < listingEnd)
        {
            if (*s >= '0' && *s <= '9')
                num = num * 10 + (*s++ - '0');
            else
            {
                if (strchr(sep, *s) != NULL)
                    s++;
                else
                    break;
            }
        }
        if (minus)
            num = -num;
        if (s == *listing || s < listingEnd && IsCharAlpha(*s))
            ret = FALSE; // uspech jen pokud cislo existuje (aspon jeden znak) a neni ukoncene pismenem
        else
        {
            if (Parameters.Count > 0 && // provedeme jeste prirazeni do sloupce
                !AssignNumberToColumn(Parameters[0]->GetColumnIndex(), columns, minus, num,
                                      emptyCol, file, dataIface, FALSE))
            {
                ret = FALSE;
            }
        }
        if (needDealloc && sep != NULL)
            free((void*)sep);
        break;
    }

    case fpfMonth_3: // mesic kodovany do tri pismen ("jan", "feb", "mar", atd.)
    {
        if (s + 2 < listingEnd)
        {
            int month;
            if (Parameters.Count == 2)
                month = GetMonthFromThreeLetters(s, Parameters[1]->String);
            else
                month = GetMonthFromThreeLettersAllLangs(s, &actualParser->AllowedLanguagesMask);
            s += 3;
            if (month == -1 || s < listingEnd && IsCharAlpha(*s))
                ret = FALSE; // uspech jen pokud jsme poznali mesic a neni ukonceny pismenem
            else
            {
                if (Parameters.Count > 0 && // provedeme jeste prirazeni do sloupce
                    !AssignMonthToColumn(Parameters[0]->GetColumnIndex(), columns, month,
                                         emptyCol, file, dataIface))
                {
                    ret = FALSE;
                }
            }
        }
        else
            ret = FALSE; // tri pismena ani nejsou k dispozici
        break;
    }

    case fpfMonth_txt: // mesic kodovany do textu ("Jan.", "Feb.", "März", "Apr.", "Mai", atd.)
    {
        const char* wordEnd = s;
        while (wordEnd<listingEnd&& * wordEnd> ' ')
            wordEnd++;
        if (s < wordEnd)
        {
            int month;
            if (Parameters.Count == 2)
                month = GetMonthFromText(&s, wordEnd, Parameters[1]->String);
            else
                month = GetMonthFromTextAllLangs(&s, wordEnd, &actualParser->AllowedLanguagesMask);
            if (month == -1 || s < listingEnd && IsCharAlpha(*s))
                ret = FALSE; // uspech jen pokud jsme poznali mesic a neni ukonceny pismenem
            else
            {
                if (Parameters.Count > 0 && // provedeme jeste prirazeni do sloupce
                    !AssignMonthToColumn(Parameters[0]->GetColumnIndex(), columns, month,
                                         emptyCol, file, dataIface))
                {
                    ret = FALSE;
                }
            }
        }
        else
            ret = FALSE; // neni k dispozici slovo, ve kterem bysme hledali
        break;
    }

    case fpfMonth:
    case fpfDay:
    case fpfYear: // mesic, den a rok ciselne
    {
        int num = 0;
        while (s < listingEnd && *s >= '0' && *s <= '9')
            num = num * 10 + (*s++ - '0');
        if (s == *listing || s < listingEnd && IsCharAlpha(*s))
            ret = FALSE; // uspech jen pokud cislo existuje (aspon jedna cislice) a neni ukoncene pismenem
        else
        {
            if (Parameters.Count > 0) // provedeme prirazeni do sloupce
            {
                switch (Function)
                {
                case fpfMonth:
                {
                    if (num < 1 || num > 12 ||
                        !AssignMonthToColumn(Parameters[0]->GetColumnIndex(), columns, num,
                                             emptyCol, file, dataIface))
                    {
                        ret = FALSE;
                    }
                    break;
                }

                case fpfDay:
                {
                    if (num < 1 || num > 31 ||
                        !AssignDayToColumn(Parameters[0]->GetColumnIndex(), columns, num,
                                           emptyCol, file, dataIface))
                    {
                        ret = FALSE;
                    }
                    break;
                }

                case fpfYear:
                {
                    if (num < 1000)
                    {
                        if (num < 80)
                            num += 2000;
                        else
                            num += 1900; // napr. "102" = "2002"
                    }
                    if (num == 1601)
                        num = 1602; // nektere wokeni FTP servery posilaji datumy jako 1.1.1601 (nesmyslny datum) - zmenime jim to na 1602 (stejny nesmysl, jen jiz zobrazitelny ve vsech casovych zonach)
                    if (num < 1602 || num > 9999 ||
                        !AssignYearToColumn(Parameters[0]->GetColumnIndex(), columns, num,
                                            emptyCol, file, dataIface, FALSE))
                    {
                        ret = FALSE;
                    }
                    break;
                }
                }
            }
        }
        break;
    }

    case fpfTime: // cas ve formatech 00:00, 00:00:00, 00:00:00.00 + pripona 'a' a 'p' za a.m. a p.m.
    {
        const char* beg = s;
        int hour = 0;
        while (s < listingEnd && *s >= '0' && *s <= '9')
            hour = hour * 10 + (*s++ - '0');
        if (beg != s && hour >= 0 && hour < 24 && s < listingEnd && *s == ':')
        {
            s++;
            beg = s;
            int minute = 0;
            while (s < listingEnd && *s >= '0' && *s <= '9')
                minute = minute * 10 + (*s++ - '0');
            if (beg != s && minute >= 0 && minute < 60) // uz mame cas ve formatu hh:mm
            {
                int second = 0;
                int millisecond = 0;
                if (s < listingEnd && *s == ':') // cas ve formatu hh:mm:ss
                {
                    s++;
                    beg = s;
                    while (s < listingEnd && *s >= '0' && *s <= '9')
                        second = second * 10 + (*s++ - '0');
                    if (beg == s || second < 0 || second >= 60)
                        ret = FALSE;
                    else
                    {
                        if (s < listingEnd && *s == '.') // cas ve formatu hh:mm:ss.ms
                        {
                            s++;
                            beg = s;
                            while (s < listingEnd && *s >= '0' && *s <= '9')
                                millisecond = millisecond * 10 + (*s++ - '0');
                            if (beg == s || millisecond < 0 || millisecond >= 1000)
                                ret = FALSE;
                        }
                    }
                }
                if (ret)
                {
                    if (s < listingEnd && (LowerCase[*s] == 'a' || LowerCase[*s] == 'p')) // pripona 'a'/'am' nebo 'p'/'pm' (a.m., p.m.)
                    {
                        if (LowerCase[*s] == 'p' && hour < 12)
                            hour += 12; // trochu nelogicke, ale "12:03pm" == "12:03"
                        s++;
                        if (s < listingEnd && LowerCase[*s] == 'm')
                            s++; // byla to pripona 'am' nebo 'pm'
                    }
                    if (ret)
                    {
                        if (s < listingEnd && IsCharAlphaNumeric(*s))
                            ret = FALSE; // uspech jen pokud neni ukonceny pismenem
                        else
                        {
                            if (Parameters.Count > 0 && // provedeme jeste prirazeni do sloupce
                                !AssignTimeToColumn(Parameters[0]->GetColumnIndex(), columns, hour, minute,
                                                    second, millisecond, emptyCol, file, dataIface))
                            {
                                ret = FALSE;
                            }
                        }
                    }
                }
            }
            else
                ret = FALSE; // invalid time format
        }
        else
            ret = FALSE; // invalid time format
        break;
    }

    case fpfYear_or_time: // rok (pak je cas neznamy) nebo cas (pak jde o tento rok)
    {
        const char* beg = s;
        int year = actualParser->ActualYear;
        int hour = 0;
        int minute = 0;
        BOOL timeIsEmpty = TRUE;
        while (s < listingEnd && *s >= '0' && *s <= '9')
            hour = hour * 10 + (*s++ - '0');
        if (beg != s && hour >= 0 && hour < 24 && s < listingEnd && *s == ':')
        {
            s++;
            timeIsEmpty = FALSE;
            beg = s;
            while (s < listingEnd && *s >= '0' && *s <= '9')
                minute = minute * 10 + (*s++ - '0');
            if (beg == s || minute < 0 || minute >= 60)
                ret = FALSE; // invalid time format
        }
        else
        {
            if (beg != s && hour >= 1601 && hour < 10000)
            {
                year = hour;
                if (year == 1601)
                    year = 1602; // nektere wokeni FTP servery posilaji datumy jako 1.1.1601 (nesmyslny datum) - zmenime jim to na 1602 (stejny nesmysl, jen jiz zobrazitelny ve vsech casovych zonach)
                hour = 0;
            }
            else
                ret = FALSE; // invalid time or year format
        }
        if (ret &&
            (s < listingEnd && IsCharAlpha(*s) || // uspech jen pokud neni ukonceny pismenem
             Parameters.Count > 1 &&              // provedeme jeste prirazeni do sloupce
                 (!AssignYearToColumn(Parameters[0]->GetColumnIndex(), columns, year,
                                      emptyCol, file, dataIface, !timeIsEmpty /* TRUE = je nutna korekce roku, ActualYear je jen prvni odhad */) ||
                  !timeIsEmpty && !AssignTimeToColumn(Parameters[1]->GetColumnIndex(), columns, hour, minute,
                                                      0, 0, emptyCol, file, dataIface))))
        {
            ret = FALSE;
        }
        break;
    }

    case fpfAll: // N znaku
    {
        if (Parameters.Count > 0)
        {
            int num = (int)Parameters[Parameters.Count == 1 ? 0 : 1]->GetNumber();
            while (num-- && s < listingEnd && *s != '\r' && *s != '\n')
                s++;
            if (num != -1)
                ret = FALSE; // uspech jen pri posunu "ukazovatka" o 'num'
            else
            {
                if (Parameters.Count > 1 && // provedeme jeste prirazeni do sloupce
                    !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                          s, lowMemErr, emptyCol, file, dataIface))
                {
                    ret = FALSE;
                }
            }
        }
        else
            ret = FALSE; // "nemuze nastat"
        break;
    }

    case fpfAll_to:    // vse az do hledaneho retezce (vcetne)
    case fpfAll_up_to: // vse az do hledaneho retezce (bez)
    {
        if (Parameters.Count > 0)
        {
            BOOL needDealloc = FALSE;
            const char* str = Parameters[Parameters.Count == 1 ? 0 : 1]->GetString(s, listingEnd, &needDealloc, lowMemErr);
            if (*lowMemErr)
            {
                ret = FALSE;
                break;
            }
            if (str != NULL)
            {
                const char* found = NULL;
                if (*str != 0)
                {
                    while (s < listingEnd && *s != '\r' && *s != '\n')
                    {
                        if (LowerCase[*str] == LowerCase[*s]) // shoduje se prvni pismeno hledaneho vzorku
                        {                                     // hledame 'str' v 's' (tim nejhloupejsim algoritmem - O(m*n), ovsem v realnych pripadech temer O(1))
                            const char* m = str + 1;
                            const char* t = s + 1;
                            while (*m != 0 && t < listingEnd && *t != '\r' && *t != '\n' &&
                                   LowerCase[*m] == LowerCase[*t])
                            {
                                m++;
                                t++;
                            }
                            if (*m == 0) // nalezeno
                            {
                                found = s;
                                s = t;
                                break;
                            }
                        }
                        s++;
                    }
                }
                else
                    found = s;
                if (found == NULL)
                    ret = FALSE; // uspech jen pri nalezeni 'str'
                else
                {
                    if (Parameters.Count > 1 && // provedeme jeste prirazeni do sloupce
                        !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                              Function == fpfAll_to ? s : found, lowMemErr, emptyCol,
                                              file, dataIface))
                    {
                        ret = FALSE;
                    }
                }
            }
            else
                ret = FALSE; // nemelo by nikdy nastat (NULL jen pri low-memory a to sem nejde)
            if (needDealloc && str != NULL)
                free((void*)str);
        }
        else
            ret = FALSE; // "nemuze nastat"
        break;
    }

    case fpfUnix_link: // unixovy link - format: "link_name -> target_name" nebo "link_name" (bez pristupovych prav k linku)
    {
        if (Parameters.Count > 2)
        {
            int state = 0;
            BOOL isFile = FALSE;
            BOOL isFile2 = FALSE;
            BOOL hasPoint = FALSE;
            BOOL firstChar = FALSE;
            const char* nameEnd = NULL;
            const char* linkBeg = NULL;
            if (s<listingEnd&& * s> ' ') // prvni znak nesmi byt white-space (ani konec radky)
            {
                while (s < listingEnd && *s != '\r' && *s != '\n')
                {
                    switch (state)
                    {
                    case 0:
                        state = 1;
                        break; // prvni znak ignorujeme ('.' neznamena priponu, "->" by byla chyba)

                    case 1:
                    {
                        if (*s == '.')
                            hasPoint = TRUE;
                        else
                        {
                            if (*s == '-' && s + 1 < listingEnd && *(s + 1) == '>') // nalezena "->" - detekce konce jmena a zacatku linku
                            {
                                if (*(s - 1) == ' ')
                                    nameEnd = s - 1;
                                else
                                    nameEnd = s;
                                s++; // preskok '-'
                                if (s + 1 < listingEnd && *(s + 1) == ' ')
                                    s++;         // pokud nasleduje ' ', preskocime ji (format: " -> ")
                                linkBeg = s + 1; // zacatek linku je za "->" nebo " -> "
                                state = 2;
                                hasPoint = FALSE;
                            }
                            else
                            {
                                if (*s > ' ' && hasPoint)
                                    isFile = TRUE;
                            }
                        }
                        break;
                    }

                    case 2:
                        state = 3;
                        break; // prvni znak ignorujeme ('.' neznamena priponu)

                    case 3:
                    {
                        if (*s == '.')
                        {
                            hasPoint = !firstChar;
                            firstChar = FALSE;
                        }
                        else
                        {
                            if (*s == '/')
                            {
                                hasPoint = FALSE;
                                isFile2 = FALSE;
                                firstChar = TRUE;
                            }
                            else
                            {
                                firstChar = FALSE;
                                if (*s > ' ' && hasPoint)
                                    isFile2 = TRUE;
                            }
                        }
                        break;
                    }
                    }
                    s++;
                }
                if (isFile2)
                    isFile = TRUE;
            }
            if (state != 1 && state != 3)
                ret = FALSE; // neocekavany format
            else             // provedeme jeste prirazeni do sloupcu
            {
                if (state == 1)
                    nameEnd = s;
                *isDir = !isFile;
                if (!AssignStringToColumn(Parameters[1]->GetColumnIndex(), columns, *listing, nameEnd,
                                          lowMemErr, emptyCol, file, dataIface) ||
                    state == 3 && !AssignStringToColumn(Parameters[2]->GetColumnIndex(), columns, linkBeg, s,
                                                        lowMemErr, emptyCol, file, dataIface))
                {
                    ret = FALSE;
                }
            }
        }
        else
            ret = FALSE; // "nemuze nastat"
        break;
    }

    case fpfUnix_device: // unixovy device - format: cislo + mezery + "," + mezery + cislo
    {
        if (Parameters.Count > 0)
        {
            int state = 0;
            if (s < listingEnd && *s >= '0' && *s <= '9') // prvni znak musi byt cislice
            {
                while (s < listingEnd && *s != '\r' && *s != '\n')
                {
                    switch (state)
                    {
                    case 0: // prvni cislo
                    {
                        if (*s < '0' || *s > '9')
                        {
                            if (*s <= ' ')
                                state = 1; // prvni mezery
                            else
                            {
                                if (*s == ',')
                                    state = 2; // druhy mezery
                                else
                                    state = 100; // error
                            }
                        }
                        break;
                    }

                    case 1: // prvni mezery
                    {
                        if (*s > ' ')
                        {
                            if (*s == ',')
                                state = 2; // druhy mezery
                            else
                                state = 100; // error
                        }
                        break;
                    }

                    case 2: // druhy mezery
                    {
                        if (*s > ' ')
                        {
                            if (*s >= '0' && *s <= '9')
                                state = 3; // druhy cislo
                            else
                                state = 100; // error
                        }
                        break;
                    }

                    case 3: // druhy cislo
                    {
                        if (*s < '0' || *s > '9')
                        {
                            if (!IsCharAlpha(*s))
                                state = 4; // uspech
                            else
                                state = 100; // error
                        }
                        break;
                    }
                    }
                    if (state > 3)
                        break; // konec
                    s++;
                }
            }
            if (state != 3 && state != 4)
                ret = FALSE; // neocekavany format nebo ukonceni
            else
            {
                if (Parameters.Count > 0 && // provedeme jeste prirazeni do sloupce
                    !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                          s, lowMemErr, emptyCol, file, dataIface))
                {
                    ret = FALSE;
                }
            }
        }
        else
            ret = FALSE; // "nemuze nastat"
        break;
    }

    case fpfIf: // podminka (neni-li vyraz splnen, vyhlas chybu)
    {
        if (Parameters.Count < 1 || // "nemuze nastat"
            !Parameters[0]->GetBoolean(file, isDir, dataIface, columns, s, listingEnd, actualParser))
        {
            ret = FALSE; // FALSE = dale nemuzeme pokracovat
        }
        break;
    }

    case fpfAssign: // prirazeni hodnoty vyrazu do sloupce
    {
        if (Parameters.Count > 1)
        {
            int i = Parameters[0]->GetColumnIndex();
            if (i >= 0 && i < columns->Count)
            {
                switch (columns->At(i)->Type)
                {
                // case stctExt:   // chyba kompilace (nemelo by sem vubec prijit)
                // case stctType:  // chyba kompilace (nemelo by sem vubec prijit)
                case stctName:
                case stctGeneralText: // pfptColumnString
                {
                    const char* beg;
                    const char* end;
                    Parameters[1]->GetStringOperand(&beg, &end, file, dataIface, columns, s, listingEnd);
                    if (!AssignStringToColumn(i, columns, beg, end, lowMemErr, emptyCol, file, dataIface))
                        ret = FALSE;
                    break;
                }

                case stctSize:
                case stctGeneralNumber: // pfptColumnNumber
                {
                    BOOL minus;
                    __int64 num = Parameters[1]->GetNumberOperand(file, dataIface, columns, &minus);
                    if (!AssignNumberToColumn(i, columns, minus, num, emptyCol, file, dataIface, FALSE))
                        ret = FALSE;
                    break;
                }

                case stctDate:
                case stctGeneralDate: // pfptColumnDate
                {
                    CFTPDate date;
                    Parameters[1]->GetDateOperand(&date, file, dataIface, columns);
                    if (!AssignDateToColumn(i, columns, &date, emptyCol, file, dataIface))
                        ret = FALSE;
                    break;
                }

                case stctTime:
                case stctGeneralTime: // pfptColumnTime
                {
                    CFTPTime time;
                    Parameters[1]->GetTimeOperand(&time, file, dataIface, columns);
                    if (!AssignTimeToColumn(i, columns, time.Hour, time.Minute, time.Second,
                                            time.Millisecond, emptyCol, file, dataIface))
                    {
                        ret = FALSE;
                    }
                    break;
                }

                default:
                    ret = FALSE; // "nemuze nastat"
                }
            }
            else
            {
                if (i == COL_IND_ISDIR || i == COL_IND_ISHIDDEN || i == COL_IND_ISLINK) // pfptColumnBoolean
                {
                    BOOL val = Parameters[1]->GetBoolean(file, isDir, dataIface, columns, s,
                                                         listingEnd, actualParser);
                    if (i == COL_IND_ISHIDDEN)
                        file->Hidden = val ? 1 : 0;
                    else
                    {
                        if (i == COL_IND_ISLINK)
                            file->IsLink = val ? 1 : 0;
                        else
                            *isDir = val;
                    }
                }
                else
                    ret = FALSE; // "nemuze nastat"
            }
        }
        else
            ret = FALSE; // "nemuze nastat"
        break;
    }

    case fpfCut_white_spaces_end:   // oriznuti white-space znaku z konce
    case fpfCut_white_spaces_start: // oriznuti white-space znaku ze zacatku
    case fpfCut_white_spaces:       // oriznuti white-space znaku z obou stran
    {
        if (Parameters.Count > 0)
        {
            int i = Parameters[0]->GetColumnIndex();
            if (i >= 0 && i < columns->Count)
            {
                switch (columns->At(i)->Type)
                {
                case stctName:
                case stctGeneralText: // pfptColumnString
                {
                    const char* beg;
                    const char* end;
                    Parameters[0]->GetStringOperand(&beg, &end, file, dataIface, columns, s, listingEnd);
                    const char* newBeg = beg;
                    const char* newEnd = end;
                    if (Function == fpfCut_white_spaces_start || Function == fpfCut_white_spaces)
                        while (newBeg < newEnd && *newBeg <= ' ')
                            newBeg++;
                    if (Function == fpfCut_white_spaces_end || Function == fpfCut_white_spaces)
                        while (newEnd > newBeg && *(newEnd - 1) <= ' ')
                            newEnd--;
                    if (newBeg != beg || newEnd != end)
                    {
                        char* str = (char*)malloc((newEnd - newBeg) + 1); // + 1 at nejsou potize s prazdnymi retezci
                        if (str == NULL)
                        {
                            TRACE_E(LOW_MEMORY);
                            *lowMemErr = TRUE;
                            ret = FALSE;
                            break;
                        }
                        memcpy(str, newBeg, newEnd - newBeg);
                        if (!AssignStringToColumn(i, columns, str, str + (newEnd - newBeg),
                                                  lowMemErr, emptyCol, file, dataIface))
                        {
                            ret = FALSE;
                        }
                        free(str);
                    }
                    break;
                }

                default:
                    ret = FALSE; // "nemuze nastat"
                }
            }
            else
                ret = FALSE; // "nemuze nastat"
        }
        else
            ret = FALSE; // "nemuze nastat"
        break;
    }

    case fpfBack: // posun "ukazovatka" zpet o N znaku
    {
        if (Parameters.Count > 0)
        {
            int num = (int)Parameters[0]->GetNumber();
            while (num-- && s > actualParser->ListingBeg && *(s - 1) != '\r' && *(s - 1) != '\n')
                s--;
            if (num != -1)
                ret = FALSE; // uspech jen pri posunu "ukazovatka" o 'num'
        }
        else
            ret = FALSE; // "nemuze nastat"
        break;
    }

    case fpfAdd_string_to_column: // pridani retezce ziskaneho jako hodnota vyrazu do sloupce
    {
        if (Parameters.Count > 1)
        {
            int i = Parameters[0]->GetColumnIndex();
            if (i >= 0 && i < columns->Count)
            {
                switch (columns->At(i)->Type)
                {
                // case stctExt:   // chyba kompilace (nemelo by sem vubec prijit)
                // case stctType:  // chyba kompilace (nemelo by sem vubec prijit)
                case stctName:
                case stctGeneralText: // pfptColumnString
                {
                    const char* begDst;
                    const char* endDst;
                    Parameters[0]->GetStringOperand(&begDst, &endDst, file, dataIface, columns, s, listingEnd);
                    const char* beg;
                    const char* end;
                    Parameters[1]->GetStringOperand(&beg, &end, file, dataIface, columns, s, listingEnd);
                    if (beg < end) // nejde-li o pridani prazdneho retezce (to ignorujeme)
                    {
                        char* str = (char*)malloc((endDst - begDst) + (end - beg));
                        if (str == NULL)
                        {
                            TRACE_E(LOW_MEMORY);
                            *lowMemErr = TRUE;
                            ret = FALSE;
                            break;
                        }
                        memcpy(str, begDst, endDst - begDst);
                        memcpy(str + (endDst - begDst), beg, end - beg);
                        if (!AssignStringToColumn(i, columns, str, str + (endDst - begDst) + (end - beg),
                                                  lowMemErr, emptyCol, file, dataIface))
                        {
                            ret = FALSE;
                        }
                        free(str);
                    }
                    break;
                }

                default:
                    ret = FALSE; // "nemuze nastat"
                }
            }
            else
                ret = FALSE; // "nemuze nastat"
        }
        else
            ret = FALSE; // "nemuze nastat"
        break;
    }

    case fpfCut_end_of_string: // oriznuti 'number' znaku z konce retezce
    {
        if (Parameters.Count > 1)
        {
            int i = Parameters[0]->GetColumnIndex();
            if (i >= 0 && i < columns->Count)
            {
                int num = (int)Parameters[1]->GetNumber();
                switch (columns->At(i)->Type)
                {
                case stctName:
                case stctGeneralText: // pfptColumnString
                {
                    const char* beg;
                    const char* end;
                    Parameters[0]->GetStringOperand(&beg, &end, file, dataIface, columns, s, listingEnd);
                    if (num <= end - beg)
                    {
                        const char* newEnd = end - num;
                        char* str = (char*)malloc((newEnd - beg) + 1); // + 1 at nejsou potize s prazdnymi retezci
                        if (str == NULL)
                        {
                            TRACE_E(LOW_MEMORY);
                            *lowMemErr = TRUE;
                            ret = FALSE;
                            break;
                        }
                        memcpy(str, beg, newEnd - beg);
                        if (!AssignStringToColumn(i, columns, str, str + (newEnd - beg),
                                                  lowMemErr, emptyCol, file, dataIface))
                        {
                            ret = FALSE;
                        }
                        free(str);
                    }
                    else
                        ret = FALSE;
                    break;
                }

                default:
                    ret = FALSE; // "nemuze nastat"
                }
            }
            else
                ret = FALSE; // "nemuze nastat"
        }
        else
            ret = FALSE; // "nemuze nastat"
        break;
    }

    default:
    {
        TRACE_E("Unexpected situation in CFTPParserFunction::UseFunction(): unknown function!");
        ret = FALSE;
        break;
    }
    }
    *listing = s;
    return ret;
}
