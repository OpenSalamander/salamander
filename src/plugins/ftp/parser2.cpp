// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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

    // precompute the first non-empty line
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
        FirstNonEmptyEnd = listingEnd + 1; // make it work even after skipping the last listing character (the last line is not terminated by an EOL)
    }

    // precompute the last non-empty line
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
                    LastNonEmptyEnd++; // make it work even after skipping the last listing character (the last line is not terminated by an EOL)
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
            LastNonEmptyEnd++; // make it work even after skipping the last listing character (the last line is not terminated by an EOL)
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
                // case stctName:   // this cannot occur here; it can only be in the first column
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

                case stctExt:                           // the extension can never be assigned during parsing -> this part is executed
                {                                       //  whenever a column of type "Extension" exists
                    if (SortByExtDirsAsFiles || !isDir) // extensions are not detected for directories
                    {
                        char* t = file->Ext; // already done: file->Ext = file->Name + file->NameLen
                        while (--t >= file->Name && *t != '.')
                            ;
                        //              if (t > file->Name) file->Ext = t + 1;   // ".cvspass" in Windows is treated as an extension ...
                        if (t >= file->Name)
                            file->Ext = t + 1;
                    }
                    break;
                }

                    // case stctType: // we ignore it; there is nothing to set

                case stctSize:
                {
                    if (isDir)
                        file->Size = CQuadWord(0, 0); // for directories we reset the size to zero just in case (unnecessary)
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
                        (emptyCol[i] & DATE_MASK_DATE) == DATE_MASK_DATE) // the date was read completely
                    {
                        CFTPDate* d = (CFTPDate*)&(file->LastWrite.dwLowDateTime);
                        lastWriteDate.wYear = d->Year;
                        lastWriteDate.wMonth = d->Month;
                        lastWriteDate.wDay = d->Day;
                        if (emptyCol[i] & DATE_MASK_YEARCORRECTIONNEEDED)
                        {
                            if (lastWriteDate.wMonth > actualMonth ||
                                lastWriteDate.wMonth == actualMonth && lastWriteDate.wDay > actualDay)
                            { // a date in the future is nonsense (UNIX servers write the year directly instead of the time)
                                // -> this is a date from the previous year
                                lastWriteDate.wYear--;
                            }
                        }
                    }
                    else // the date is incomplete -> take the empty value
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
                        (emptyCol[i] & DATE_MASK_TIME) == DATE_MASK_TIME) // the time was read
                    {
                        CFTPTime* t = (CFTPTime*)&(file->LastWrite.dwHighDateTime);
                        lastWriteDate.wHour = t->Hour;
                        lastWriteDate.wMinute = t->Minute;
                        lastWriteDate.wSecond = t->Second;
                        lastWriteDate.wMilliseconds = t->Millisecond;
                    }
                    else // the time was not read -> take the empty value
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
                        (emptyCol[i] & DATE_MASK_DATE) != DATE_MASK_DATE) // the date is incomplete -> take the empty value
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
                            { // a date in the future is nonsense (UNIX servers write the year directly instead of the time)
                                // -> this is a date from the previous year
                                testDate.wYear--;
                                dataIface->StoreYearToColumn(*file, i, testDate.wYear);
                            }
                        }
                        if (!SystemTimeToFileTime(&testDate, &ft)) // invalid date -> set it to 1/1/1602
                            dataIface->StoreDateToColumn(*file, i, 1, 1, 1602);
                    }
                    break;
                }

                case stctGeneralTime:
                {
                    if (emptyCol == NULL ||
                        (emptyCol[i] & DATE_MASK_TIME) != DATE_MASK_TIME) // the time was not read -> take the empty value
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
    if (!err && lastWriteUsed) // set file->LastWrite according to the values in the columns
    {
        if (!SystemTimeToFileTime(&lastWriteDate, &ft) ||
            !LocalFileTimeToFileTime(&ft, &file->LastWrite))
        { // error setting the date -> invalid date (the time cannot be wrong) -> set it to 1/1/1602
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
    // set default values (it is a file and not hidden)
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
        // for each column the 'emptyCol' array contains TRUE until some value is assigned
        int i;
        for (i = 0; i < columns->Count; i++)
            emptyCol[i] = TRUE;

        // perform the parsing
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
                    if (SkipThisLineItIsIncomlete || // an incomplete listing was detected - skip the processed trailing part of the listing
                        !emptyCol[0])
                        break;  // the rule succeeded - a file or directory was read - proceed to process the data
                    brk = TRUE; // the rule succeeded - skip the line - continue parsing the next line
                }
                else
                {
                    if (err) // error - out of memory
                    {
                        if (lowMem != NULL)
                            *lowMem = TRUE;
                        break;
                    }
                    // error - the rule cannot be used - try the next rule on the same text
                    s = restart;
                }

                // release and reset the data before filling them again
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
            if (j == Rules.Count) // no rule can be applied to this text, return an error
            {
                // s = restart;  // error position (already assigned)
                break;
            }
            if (err || SkipThisLineItIsIncomlete || !emptyCol[0])
                break; // an error occurred or we have a result
        }
        *listing = s;
    }

    BOOL ret = FALSE;
    if (!SkipThisLineItIsIncomlete &&
        !err && !emptyCol[0] && file->Name != NULL /* always true */) // Name was populated -> we have a file/directory
    {
        // set the previously ignored members of 'file'
        file->NameLen = strlen(file->Name);
        file->Ext = file->Name + file->NameLen;

        // fill the empty values into empty columns
        FillEmptyValues(err, file, *isDir, dataIface, columns, lowMem, emptyCol, ActualYear,
                        ActualMonth, ActualDay);

        if (!err)
            ret = TRUE; // we successfully obtained another file or directory
    }
    if (!ret) // an error occurred or no file/directory was found, we must free the allocated memory
    {
        dataIface->ReleasePluginData(*file, *isDir);
        if (file->Name != NULL)
            SalamanderGeneral->Free(file->Name);
        if (SkipThisLineItIsIncomlete)
            *listing = listingEnd; // it should already be set, but just in case...
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
    // if all functions of the rule are successfully used and the "pointer" is at the end
    // of the listing or at the end of the line, report success
    BOOL ret = FALSE;
    if (file->Name == NULL || file->Name[0] != 0) // assigning an empty string to Name is a parsing error
    {
        if (actualParser->SkipThisLineItIsIncomlete)
            ret = TRUE;
        else
        {
            if (!(*lowMemErr) && i == Functions.Count && (s == listingEnd || *s == '\r' || *s == '\n'))
            {
                ret = TRUE;
                // skip a possible end of line
                if (s < listingEnd && *s == '\r')
                    s++;
                if (s < listingEnd && *s == '\n')
                    s++;
            }
        }
    }
    else
        actualParser->SkipThisLineItIsIncomlete = FALSE; // even if the line was incomplete, an error occurred -> ignore it from now on
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
        emptyCol[col] |= DATE_MASK_DAY; // OR the day into the mask
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
        emptyCol[col] |= DATE_MASK_MONTH; // OR the month into the mask
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
        // OR the year plus the year correction (needed for "year_or_time" - the time for files younger than six
        // months - we cannot just take the current year)
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
        emptyCol[col] |= DATE_MASK_DATE; // store the complete date
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
        emptyCol[col] |= DATE_MASK_TIME; // OR the time into the mask
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
                    file->Size.Set(0, 0); // the "positive_number" function - stores the default empty value for negative numbers
                else
                    return FALSE; // Size cannot store a negative number (we cannot ignore it -> the user would stare at -666 B =~ 18 EB)
            }
            else
                file->Size.SetUI64((unsigned __int64)number);
            break;
        }

        case stctGeneralNumber:
        {
            if (minus && onlyPositiveNumber)
                dataIface->StoreNumberToColumn(*file, col, INT64_EMPTYNUMBER); // the "positive_number" function - stores the default empty value for negative numbers
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
                file->Name[MAX_PATH - 5] = 0; // file->Name can be at most MAX_PATH - 5 characters long (Salamander limitation - hopefully not an issue for viewing, the user must perform the operation in other software)
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
        *lowMemErr = TRUE; // fatal error - simulating "low memory"
        return FALSE;
    }
}

// returns the number of the month whose name is encoded by three letters; if it is an unknown
// month code, returns -1
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

// precomputed table for looking up the month number + the mask of allowed languages
// NOTE: months must be in lowercase!
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

// returns the number of the month whose name is encoded by three letters, checks whether it is
// a month name in an allowed language and possibly restricts the allowed languages for further
// language detection; if it is an unknown month or a month in a disallowed language,
// returns -1
int GetMonthFromThreeLettersAllLangs(const char* month, DWORD* allowedLanguagesMask)
{
    int res = -1;
    DWORD mask = 0; // mask of languages in which this month has the same name
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
        return -1; // error (month not found or month from a disallowed language)
    else
    {
        *allowedLanguagesMask &= mask;
        return res;
    }
}

// returns the number of the month whose name is encoded by text; if it is an unknown
// month code, returns -1
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

// precomputed table for looking up the month number + the mask of allowed languages
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

// returns the number of the month whose name is the text '*month', checks whether it is
// a month name in an allowed language and possibly restricts the allowed languages for further
// language detection; if it is an unknown month or a month in a disallowed language,
// returns -1
int GetMonthFromTextAllLangs(const char** month, const char* monthEnd, DWORD* allowedLanguagesMask)
{
    const char* s = *month;
    int res = -1;
    DWORD mask = 0; // mask of languages in which this month has the same name
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
        return -1; // error (month not found or month from a disallowed language)
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
    case fpfSkip_white_spaces: // skip white-space characters (except CR and LF)
    {
        while (s < listingEnd && (*s <= ' ' && *s != '\r' && *s != '\n'))
            s++;
        break;
    }

    case fpfSkip_to_number: // skip ahead to the nearest decimal digit (except CR and LF)
    {
        while (s < listingEnd && ((*s < '0' || *s > '9') && *s != '\r' && *s != '\n'))
            s++;
        break;
    }

    case fpfWhite_spaces: // skip white-space characters (except CR and LF)
    {
        if (Parameters.Count > 0)
        {
            int num = (int)Parameters[0]->GetNumber();
            while (num-- && s < listingEnd && (*s <= ' ' && *s != '\r' && *s != '\n'))
                s++;
            if (num != -1)
                ret = FALSE; // success only if the "pointer" advances by 'num'
        }
        else
        {
            while (s < listingEnd && (*s <= ' ' && *s != '\r' && *s != '\n'))
                s++;
            if (s == *listing)
                ret = FALSE; // success only if the "pointer" advances
        }
        break;
    }

    case fpfWhite_spaces_and_line_ends: // skip white-space characters (including CR and LF)
    {
        while (s < listingEnd && *s <= ' ')
            s++;
        if (s == *listing)
            ret = FALSE; // success only if the "pointer" advances
        else
        {
            if (s == listingEnd) // success only if the pointer stopped on a character and not at the end of the listing
            {
                if (actualParser->ListingIncomplete)
                { // the listing is incomplete -> assume that if it were complete, this would not happen
                    // so skip the trailing part of the listing processed by this rule
                    actualParser->SkipThisLineItIsIncomlete = TRUE;
                }
                else
                    ret = FALSE;
            }
        }
        break;
    }

    case fpfRest_of_line: // the rest of the line up to CR or LF
    {
        while (s < listingEnd && *s != '\r' && *s != '\n')
            s++;
        if (s == *listing)
            ret = FALSE; // success only if the "pointer" advances
        else
        {
            if (Parameters.Count > 0 && // also assign it to the column
                !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                      s, lowMemErr, emptyCol, file, dataIface))
            {
                ret = FALSE;
            }
        }
        break;
    }

    case fpfWord: // word (up to the nearest white-space or end of line)
    {
        while (s < listingEnd && *s > ' ')
            s++;
        if (s == *listing)
            ret = FALSE; // success only if the "pointer" advances
        else
        {
            if (Parameters.Count > 0 && // also assign it to the column
                !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                      s, lowMemErr, emptyCol, file, dataIface))
            {
                ret = FALSE;
            }
        }
        break;
    }

    case fpfNumber:         // number (only '+'/'-' and digits)
    case fpfPositiveNumber: // only a positive number (only '+'/'-' and digits) - if it finds a negative number, assigns the default empty value
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
            ret = FALSE; // success only if the number exists (at least one digit) and does not end with a letter
        else
        {
            if (Parameters.Count > 0 && // also assign it to the column
                !AssignNumberToColumn(Parameters[0]->GetColumnIndex(), columns, minus, num,
                                      emptyCol, file, dataIface, Function == fpfPositiveNumber))
            {
                ret = FALSE;
            }
        }
        break;
    }

    case fpfNumber_with_separators: // number with separators
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
        // skip separators except '+', '-' and digits
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
            ret = FALSE; // success only if the number exists (at least one character) and does not end with a letter
        else
        {
            if (Parameters.Count > 0 && // also assign it to the column
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

    case fpfMonth_3: // month encoded into three letters ("jan", "feb", "mar", etc.)
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
                ret = FALSE; // success only if the month was recognized and it does not end with a letter
            else
            {
                if (Parameters.Count > 0 && // also assign it to the column
                    !AssignMonthToColumn(Parameters[0]->GetColumnIndex(), columns, month,
                                         emptyCol, file, dataIface))
                {
                    ret = FALSE;
                }
            }
        }
        else
            ret = FALSE; // even three letters are not available
        break;
    }

    case fpfMonth_txt: // month encoded as text ("Jan.", "Feb.", "März", "Apr.", "Mai", etc.)
    {
        const char* wordEnd = s;
        while (wordEnd < listingEnd && *wordEnd > ' ')
            wordEnd++;
        if (s < wordEnd)
        {
            int month;
            if (Parameters.Count == 2)
                month = GetMonthFromText(&s, wordEnd, Parameters[1]->String);
            else
                month = GetMonthFromTextAllLangs(&s, wordEnd, &actualParser->AllowedLanguagesMask);
            if (month == -1 || s < listingEnd && IsCharAlpha(*s))
                ret = FALSE; // success only if the month was recognized and it does not end with a letter
            else
            {
                if (Parameters.Count > 0 && // also assign it to the column
                    !AssignMonthToColumn(Parameters[0]->GetColumnIndex(), columns, month,
                                         emptyCol, file, dataIface))
                {
                    ret = FALSE;
                }
            }
        }
        else
            ret = FALSE; // there is no word available to search in
        break;
    }

    case fpfMonth:
    case fpfDay:
    case fpfYear: // year in numeric form
    {
        int num = 0;
        while (s < listingEnd && *s >= '0' && *s <= '9')
            num = num * 10 + (*s++ - '0');
        if (s == *listing || s < listingEnd && IsCharAlpha(*s))
            ret = FALSE; // success only if the number exists (at least one digit) and does not end with a letter
        else
        {
            if (Parameters.Count > 0) // assign it to the column
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
                            num += 1900; // e.g. "102" = "2002"
                    }
                    if (num == 1601)
                        num = 1602; // some Windows FTP servers send dates like 1.1.1601 (a nonsensical date) - change it to 1602 (the same nonsense, but displayable in all time zones)
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

    case fpfTime: // time in formats 00:00, 00:00:00, 00:00:00.00 + suffix 'a' and 'p' for a.m. and p.m.
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
            if (beg != s && minute >= 0 && minute < 60) // we already have the time in the hh:mm format
            {
                int second = 0;
                int millisecond = 0;
                if (s < listingEnd && *s == ':') // time in the hh:mm:ss format
                {
                    s++;
                    beg = s;
                    while (s < listingEnd && *s >= '0' && *s <= '9')
                        second = second * 10 + (*s++ - '0');
                    if (beg == s || second < 0 || second >= 60)
                        ret = FALSE;
                    else
                    {
                        if (s < listingEnd && *s == '.') // time in the hh:mm:ss.ms format
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
                    if (s < listingEnd && (LowerCase[*s] == 'a' || LowerCase[*s] == 'p')) // suffix 'a'/'am' or 'p'/'pm' (a.m., p.m.)
                    {
                        if (LowerCase[*s] == 'p' && hour < 12)
                            hour += 12; // slightly illogical, but "12:03pm" == "12:03"
                        s++;
                        if (s < listingEnd && LowerCase[*s] == 'm')
                            s++; // it was the suffix 'am' or 'pm'
                    }
                    if (ret)
                    {
                        if (s < listingEnd && IsCharAlphaNumeric(*s))
                            ret = FALSE; // success only if it does not end with a letter
                        else
                        {
                            if (Parameters.Count > 0 && // also assign it to the column
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

    case fpfYear_or_time: // year (then the time is unknown) or time (then it is this year)
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
                    year = 1602; // some Windows FTP servers send dates like 1.1.1601 (a nonsensical date) - change it to 1602 (the same nonsense, but displayable in all time zones)
                hour = 0;
            }
            else
                ret = FALSE; // invalid time or year format
        }
        if (ret &&
            (s < listingEnd && IsCharAlpha(*s) || // success only if it does not end with a letter
             Parameters.Count > 1 &&              // also assign it to the column
                 (!AssignYearToColumn(Parameters[0]->GetColumnIndex(), columns, year,
                                      emptyCol, file, dataIface, !timeIsEmpty /* TRUE = a year correction is needed, ActualYear is only the first estimate */) ||
                  !timeIsEmpty && !AssignTimeToColumn(Parameters[1]->GetColumnIndex(), columns, hour, minute,
                                                      0, 0, emptyCol, file, dataIface))))
        {
            ret = FALSE;
        }
        break;
    }

    case fpfAll: // N characters
    {
        if (Parameters.Count > 0)
        {
            int num = (int)Parameters[Parameters.Count == 1 ? 0 : 1]->GetNumber();
            while (num-- && s < listingEnd && *s != '\r' && *s != '\n')
                s++;
            if (num != -1)
                ret = FALSE; // success only if the "pointer" advances by 'num'
            else
            {
                if (Parameters.Count > 1 && // also assign it to the column
                    !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                          s, lowMemErr, emptyCol, file, dataIface))
                {
                    ret = FALSE;
                }
            }
        }
        else
            ret = FALSE; // "cannot happen"
        break;
    }

    case fpfAll_to:    // everything up to the searched string (inclusive)
    case fpfAll_up_to: // everything up to the searched string (exclusive)
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
                        if (LowerCase[*str] == LowerCase[*s]) // the first letter of the searched pattern matches
                        {                                     // search for 'str' in 's' (using the simplest algorithm - O(m*n), but almost O(1) in real cases)
                            const char* m = str + 1;
                            const char* t = s + 1;
                            while (*m != 0 && t < listingEnd && *t != '\r' && *t != '\n' &&
                                   LowerCase[*m] == LowerCase[*t])
                            {
                                m++;
                                t++;
                            }
                            if (*m == 0) // found
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
                    ret = FALSE; // success only when 'str' is found
                else
                {
                    if (Parameters.Count > 1 && // also assign it to the column
                        !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                              Function == fpfAll_to ? s : found, lowMemErr, emptyCol,
                                              file, dataIface))
                    {
                        ret = FALSE;
                    }
                }
            }
            else
                ret = FALSE; // should never happen (NULL only on low-memory, which does not reach here)
            if (needDealloc && str != NULL)
                free((void*)str);
        }
        else
            ret = FALSE; // "cannot happen"
        break;
    }

    case fpfUnix_link: // Unix link - format: "link_name -> target_name" or "link_name" (without access rights to the link)
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
            if (s < listingEnd && *s > ' ') // the first character must not be white-space (or the end of line)
            {
                while (s < listingEnd && *s != '\r' && *s != '\n')
                {
                    switch (state)
                    {
                    case 0:
                        state = 1;
                        break; // ignore the first character ('.' does not mean an extension, "->" would be an error)

                    case 1:
                    {
                        if (*s == '.')
                            hasPoint = TRUE;
                        else
                        {
                            if (*s == '-' && s + 1 < listingEnd && *(s + 1) == '>') // "->" found - detect the end of the name and the start of the link
                            {
                                if (*(s - 1) == ' ')
                                    nameEnd = s - 1;
                                else
                                    nameEnd = s;
                                s++; // skip '-'
                                if (s + 1 < listingEnd && *(s + 1) == ' ')
                                    s++;         // if a space follows, skip it (format: " -> ")
                                linkBeg = s + 1; // the link begins after "->" or " -> "
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
                        break; // ignore the first character ('.' does not mean an extension)

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
                ret = FALSE; // unexpected format
            else             // also assign them to the columns
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
            ret = FALSE; // "cannot happen"
        break;
    }

    case fpfUnix_device: // Unix device - format: number + spaces + "," + spaces + number
    {
        if (Parameters.Count > 0)
        {
            int state = 0;
            if (s < listingEnd && *s >= '0' && *s <= '9') // the first character must be a digit
            {
                while (s < listingEnd && *s != '\r' && *s != '\n')
                {
                    switch (state)
                    {
                    case 0: // first number
                    {
                        if (*s < '0' || *s > '9')
                        {
                            if (*s <= ' ')
                                state = 1; // first spaces
                            else
                            {
                                if (*s == ',')
                                    state = 2; // second spaces
                                else
                                    state = 100; // error
                            }
                        }
                        break;
                    }

                    case 1: // first spaces
                    {
                        if (*s > ' ')
                        {
                            if (*s == ',')
                                state = 2; // second spaces
                            else
                                state = 100; // error
                        }
                        break;
                    }

                    case 2: // second spaces
                    {
                        if (*s > ' ')
                        {
                            if (*s >= '0' && *s <= '9')
                                state = 3; // second number
                            else
                                state = 100; // error
                        }
                        break;
                    }

                    case 3: // second number
                    {
                        if (*s < '0' || *s > '9')
                        {
                            if (!IsCharAlpha(*s))
                                state = 4; // success
                            else
                                state = 100; // error
                        }
                        break;
                    }
                    }
                    if (state > 3)
                        break; // end
                    s++;
                }
            }
            if (state != 3 && state != 4)
                ret = FALSE; // unexpected format or termination
            else
            {
                if (Parameters.Count > 0 && // also assign it to the column
                    !AssignStringToColumn(Parameters[0]->GetColumnIndex(), columns, *listing,
                                          s, lowMemErr, emptyCol, file, dataIface))
                {
                    ret = FALSE;
                }
            }
        }
        else
            ret = FALSE; // "cannot happen"
        break;
    }

    case fpfIf: // condition (if the expression is not met, report an error)
    {
        if (Parameters.Count < 1 || // "cannot happen"
            !Parameters[0]->GetBoolean(file, isDir, dataIface, columns, s, listingEnd, actualParser))
        {
            ret = FALSE; // FALSE = we cannot continue further
        }
        break;
    }

    case fpfAssign: // assign the value of the expression to the column
    {
        if (Parameters.Count > 1)
        {
            int i = Parameters[0]->GetColumnIndex();
            if (i >= 0 && i < columns->Count)
            {
                switch (columns->At(i)->Type)
                {
                // case stctExt:   // compilation error (should never get here)
                // case stctType:  // compilation error (should never get here)
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
                    ret = FALSE; // "cannot happen"
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
                    ret = FALSE; // "cannot happen"
            }
        }
        else
            ret = FALSE; // "cannot happen"
        break;
    }

    case fpfCut_white_spaces_end:   // trim white-space characters from the end
    case fpfCut_white_spaces_start: // trim white-space characters from the beginning
    case fpfCut_white_spaces:       // trim white-space characters from both sides
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
                        char* str = (char*)malloc((newEnd - newBeg) + 1); // +1 to avoid issues with empty strings
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
                    ret = FALSE; // "cannot happen"
                }
            }
            else
                ret = FALSE; // "cannot happen"
        }
        else
            ret = FALSE; // "cannot happen"
        break;
    }

    case fpfBack: // move the "pointer" back by N characters
    {
        if (Parameters.Count > 0)
        {
            int num = (int)Parameters[0]->GetNumber();
            while (num-- && s > actualParser->ListingBeg && *(s - 1) != '\r' && *(s - 1) != '\n')
                s--;
            if (num != -1)
                ret = FALSE; // success only if the "pointer" advances by 'num'
        }
        else
            ret = FALSE; // "cannot happen"
        break;
    }

    case fpfAdd_string_to_column: // add the string obtained as the value of the expression to the column
    {
        if (Parameters.Count > 1)
        {
            int i = Parameters[0]->GetColumnIndex();
            if (i >= 0 && i < columns->Count)
            {
                switch (columns->At(i)->Type)
                {
                // case stctExt:   // compilation error (should never get here)
                // case stctType:  // compilation error (should never get here)
                case stctName:
                case stctGeneralText: // pfptColumnString
                {
                    const char* begDst;
                    const char* endDst;
                    Parameters[0]->GetStringOperand(&begDst, &endDst, file, dataIface, columns, s, listingEnd);
                    const char* beg;
                    const char* end;
                    Parameters[1]->GetStringOperand(&beg, &end, file, dataIface, columns, s, listingEnd);
                    if (beg < end) // if we are not adding an empty string (that is ignored)
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
                    ret = FALSE; // "cannot happen"
                }
            }
            else
                ret = FALSE; // "cannot happen"
        }
        else
            ret = FALSE; // "cannot happen"
        break;
    }

    case fpfCut_end_of_string: // trim 'number' characters from the end of the string
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
                        char* str = (char*)malloc((newEnd - beg) + 1); // +1 to avoid issues with empty strings
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
                    ret = FALSE; // "cannot happen"
                }
            }
            else
                ret = FALSE; // "cannot happen"
        }
        else
            ret = FALSE; // "cannot happen"
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
