// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crtdbg.h>
#include <wtypes.h>
#include <time.h>
#include <commctrl.h>
#include <limits.h>
#include <new.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "spl_com.h"
#include "arraylt.h"

#include "csvlib.h"

// pouze optimalizacni zalezitost
extern BOOL IsAlphaNumeric[256]; // pole TRUE/FALSE pro znaky (FALSE = neni pismeno ani cislice)
extern BOOL IsAlpha[256];

#define SizeOf(x) (sizeof(x) / sizeof(x[0]))

//****************************************************************************
//
// CCSVParserCore
//

/*BOOL IsTextQualifier(char p, CCSVParserTextQualifier tq)
{
  if (tq == CSVTQ_NONE)
    return FALSE;
  if (tq == CSVTQ_QUOTE && p == '\"')
    return TRUE;
  if (tq == CSVTQ_SINGLEQUOTE && p == '\'')
    return TRUE;
  return FALSE;
}*/

// bloky, po kterych nacitame soubor
#define READ_BUFFER_SIZE 60 * 1024

enum CReadingStateEnum
{
    rsNewLine,
    rsNewLineR,
    rsNewLineN,
    rsData,
    rsQualifiedData,
    rsQualifiedDataFirst,
    rsPostQualifiedData,
};

CCSVParserCore::CCSVParserCore() : Rows(5000, 10000), Columns(500, 500)
{
    Status = CSVE_OK;
}

CCSVParserCore::~CCSVParserCore()
{
    int i;
    for (i = 0; i < Columns.Count; i++)
    {
        if (Columns[i].Name != NULL)
            free(Columns[i].Name);
    }
    if (File != NULL)
        fclose(File);
}

BOOL CCSVParserCore::SetLongerColumn(int columnIndex, DWORD columnLen)
{
    if (columnIndex >= Columns.Count)
    {
        CCSVColumn column;
        column.First = 0;
        column.Length = 0;
        column.MaxLength = columnLen;
        column.Name = NULL;
        Columns.Add(column);
        if (!Columns.IsGood())
        {
            Columns.ResetState();
            return FALSE;
        }
    }
    else
    {
        if (Columns[columnIndex].MaxLength < columnLen)
            Columns[columnIndex].MaxLength = columnLen;
    }
    return TRUE;
}

int CCSVParserCore::AnalyseSeparatorRatings(int rowsCount, bool charUsed[], CLineRating* lines)
{
    // ohodnotime pouzite znaky a hledame znak s maximalni hodnocenim
    int maxRating = 100;
    int maxIndex = -1;
    int i;
    for (i = 0; i < 256; i++)
    {
        if (charUsed[i])
        {
            int rating = 100;
            int row;
            for (row = 0; row < rowsCount; row++)
            {
                if (row < rowsCount - 1)
                {
                    // pokud je pocet znaku v nasi a nasledujici radce stejny a vyssi nez nula,
                    // hodnotime to kladne
                    if (lines[row].CharCount[i] > 0 &&
                        (lines[row].CharCount[i] == lines[row + 1].CharCount[i]))
                        rating++;
                    else
                        rating--;
                }
                rating += lines[row].CharRating[i];
            }

            if (rating > maxRating)
            {
                maxRating = rating;
                maxIndex = i;
            }
            else if (rating == maxRating)
            { // Same rating - take the more appropriate one
                static const char priorities[] = ".:;,\t";
                const char* prio1 = strchr(priorities, maxIndex);
                const char* prio2 = strchr(priorities, i);
                if (prio2 > prio1)
                {
                    maxRating = rating;
                    maxIndex = i;
                }
            }
        }
    }

    return maxIndex;
} /* CCSVParserCore::AnalyseSeparatorRatings */

//****************************************************************************
//
// CCSVParser<CChar>
//

static void SwapWords(char* s, size_t len)
{
    while (len-- > 0)
    {
        char tmp = *s;
        *s = s[1];
        s[1] = tmp;
        s += 2;
    }
}

template <class CChar>
CCSVParser<CChar>::CCSVParser(const char* filename,
                              BOOL autoSeparator, CChar separator,
                              BOOL autoQualifier, CCSVParserTextQualifier textQualifier,
                              BOOL autoFirstRowAsName, BOOL firstRowAsColumnNames)
{
    __int64 rowSeek = 0; // pozice radku v souboru

    Buffer = NULL;

    File = fopen(filename, "rb");
    if (File == NULL)
    {
        Status = (CCSVParserStatus)(GetLastError() | CSVE_SYSTEM_ERROR);
        //Status = CSVE_FILE_NOT_FOUND;
        return;
    }

    // vytahnu velikost souboru
    fseek(File, 0, SEEK_END);
    fgetpos(File, &FileSize);
    fseek(File, 0, SEEK_SET);

    if (sizeof(CChar) == 2)
    {
        WORD BOM;

        fread(&BOM, 1, 2, File); // Read BOM
        rowSeek = 1;             // in # of characters
        bIsBigEndian = BOM == 0xFFFE;
    }
    else
    { // Detect UTF8
        BYTE b[3];
        fread(b, 1, 3, File);
        if (!memcmp(b, "\xEF\xBB\xBF", 3))
        {                // Skip UTF8 BOM
            rowSeek = 3; // in # of characters
        }
        else
        {
            fseek(File, 0, SEEK_SET);
        }
    }

    if (autoSeparator || autoQualifier || autoFirstRowAsName)
    {
        // user chece detekovat nektere z parametru
        AnalyseFile(autoSeparator, &separator,
                    autoQualifier, &textQualifier,
                    autoFirstRowAsName, &firstRowAsColumnNames);
    }

    Separator = separator;
    TextQualifier = textQualifier;

    CChar buffer[READ_BUFFER_SIZE];

    size_t bytesRead; // pocet skutecne nactenych znaku do bufferu

    // kde na prave stoji iterator
    CReadingStateEnum rs = rsData;

    int rowLen = 0;      // delka prave cteneho radku
    int maxRowLen = 0;   // delka nejdelsiho radku
    DWORD columnLen = 0; // pocet znaku v aktualnim sloupci
    int columnIndex = 0; // aktualni slupec

    fseek(File, (long)rowSeek * sizeof(CChar), SEEK_SET);

    do
    {
        // cteme oknem o velikosti READ_BUFFER_SIZE
        bytesRead = fread(buffer, sizeof(CChar), SizeOf(buffer), File);
        if (bytesRead > 0)
        {
            if (bIsBigEndian && (sizeof(CChar) == 2))
                SwapWords((char*)buffer, bytesRead);
            size_t index;
            for (index = 0; index < bytesRead; index++)
            {
                CChar c = buffer[index];
                // Enable CR/LF inside quoted text
                if (c == 0 || ((c == '\r' || c == '\n') && (rs != rsQualifiedData)))
                {
                    // pokud jde o konec radku
                    if ((rs == rsNewLineR && c == '\n') ||
                        (rs == rsNewLineN && c == '\r'))
                    {
                        // a jde uz o druhy znak, pouze se pres nej prekleneme
                        rowSeek++;
                        rs = rsNewLine;
                    }
                    else
                    {
                        // a jde o prvni znak, zalozime radek
                        Rows.Add(rowSeek);
                        if (!Rows.IsGood())
                        {
                            Rows.ResetState();
                            Status = CSVE_OOM;
                            return;
                        }
                        // pokud je tu novy sloupec, ulozime ho
                        if (!SetLongerColumn(columnIndex, columnLen))
                        {
                            Status = CSVE_OOM;
                            return;
                        }

                        rowSeek += rowLen + 1;
                        rowLen = 0;
                        columnLen = 0;
                        columnIndex = 0;
                        // pripravime se na prijmuti druheho znaku z konce radku
                        switch (c)
                        {
                        case 0:
                            rs = rsNewLine;
                            break;
                        case '\r':
                            rs = rsNewLineR;
                            break;
                        case '\n':
                            rs = rsNewLineN;
                            break;
                        }
                    }
                    continue;
                }

                // druhy znak z konce radku nemusel prijit -> shodime stav
                if (rs == rsNewLine || rs == rsNewLineR || rs == rsNewLineN)
                    rs = rsData;

                rowLen++;
                if (rowLen > maxRowLen)
                    maxRowLen = rowLen;

                if ((TextQualifier == CSVTQ_QUOTE && c == '\"') ||
                    (TextQualifier == CSVTQ_SINGLEQUOTE && c == '\''))
                {
                    if (rs == rsData && columnLen == 0)
                    {
                        rs = rsQualifiedData;
                        continue;
                    }

                    if (rs == rsQualifiedData)
                    {
                        rs = rsQualifiedDataFirst;
                        continue;
                    }

                    if (rs == rsQualifiedDataFirst)
                    {
                        rs = rsQualifiedData;
                        columnLen++;
                        continue;
                    }

                    columnLen++;

                    continue;
                }

                if (rs != rsQualifiedData && c == separator)
                {
                    // pokud je tu novy sloupec, ulozime ho
                    if (!SetLongerColumn(columnIndex, columnLen))
                    {
                        Status = CSVE_OOM;
                        return;
                    }

                    columnIndex++;
                    columnLen = 0;
                    rs = rsData;
                    continue;
                }

                columnLen++;
            }
        }
        else
        {
            if (ferror(File))
            {
                Status = CSVE_READ_ERROR;
                return;
            }
        }
    } while (bytesRead > 0);

    if (rs != rsNewLine && rs != rsNewLineR && rs != rsNewLineN && FileSize > 0)
    {
        Rows.Add(rowSeek);
        if (!Rows.IsGood())
        {
            Rows.ResetState();
            Status = CSVE_OOM;
            return;
        }
        // pokud je tu novy sloupec, ulozime ho
        if (!SetLongerColumn(columnIndex, columnLen))
        {
            Status = CSVE_OOM;
            return;
        }
    }

    BufferSize = maxRowLen;
    if (firstRowAsColumnNames && Rows.Count > 0)
    {
        FetchRecord(0);
        int i;
        for (i = 0; i < Columns.Count; i++)
        {
            size_t textLen;
            const CChar* text = (const CChar*)GetCellText(i, &textLen);
            Columns[i].Name = (char*)malloc((textLen + 1) * sizeof(CChar));
            if (Columns[i].Name == NULL)
                goto SKIP_ROW_CONVERT;
            memcpy(Columns[i].Name, text, textLen * sizeof(CChar));
            ((CChar*)Columns[i].Name)[textLen] = 0;
        }
        Rows.Delete(0);
    }
SKIP_ROW_CONVERT:
    return;
}

template <class CChar>
CCSVParser<CChar>::~CCSVParser()
{
    if (Buffer != NULL)
        free(Buffer);
}

// Snazi se detekovat kvalifikator textu.
//   buffer: kompletni buffer
//   rows:   offsetu od pocatku 'buffer' pro jednotlive radky
//
// Postup hledani:
//   kvalifikator muze nabyvat tri hodnot: CSVTQ_QUOTE, CSVTQ_SINGLEQUOTE, CSVTQ_NONE
//   radky se zkusi prohledat pro znaky ' a " a obe varianty se ohodnoti
//     + kvalifikatoru je v kazdem radku sudy pocet
//     - kvalifikatoru je v kazdem radku lichy pocet
//     pokud stoji kvalifikator osamocen (nejedna se o dvojici)
//     + pred oteviracim a za zaviracim kvalifikatorem neni alfanumericky znak
//     - pred oteviracim a za zaviracim kvalifikatorem je alfanumericky znak

template <class CChar>
CCSVParserTextQualifier
CCSVParser<CChar>::AnalyseTextQualifier(const CChar* buffer, TDirectArray<WORD>* rows)
{
    if (rows->Count == 0)
        return CSVTQ_NONE;
    // zkusime varinatu "
    double quoteRating = AnalyseTextQualifierAux(buffer, rows, '\"');
    // zkusime varinatu '
    double singleQuoteRating = AnalyseTextQualifierAux(buffer, rows, '\'');
    // pokud jsou obe pod prahem, vratime prazdny kvalifikator
    if (quoteRating <= 100 && singleQuoteRating <= 100)
        return CSVTQ_NONE;
    // vybereme kvalifikator s vetsi vahou
    if (quoteRating >= singleQuoteRating)
        return CSVTQ_QUOTE;
    return CSVTQ_SINGLEQUOTE;
}

template <class CChar>
double
CCSVParser<CChar>::AnalyseTextQualifierAux(const CChar* buffer, TDirectArray<WORD>* rows, CChar qualifier)
{
    double rating = 100.0 * rows->Count;
    int row;
    for (row = 0; row < rows->Count; row++)
    {
        const CChar* p = buffer + rows->At(row);
        WORD qualifierCount = 0;
        BOOL qualifiledValue = FALSE; // jsme mezi kvalifikatory?
        while (*p != 0)
        {
            if (*p == qualifier)
            {
                qualifierCount++;

                // pokud stoji kvalifikator osamocen (nejedna se o dvojici)
                // + pred oteviracim a za zaviracim kvalifikatorem neni alfanumericky znak
                // - pred oteviracim a za zaviracim kvalifikatorem je alfanumericky znak
                if (!qualifiledValue)
                {
                    if (p > buffer + rows->At(row))
                    {
                        if ((p[-1] < 0x100) && IsAlphaNumeric[p[-1]])
                            rating--;
                        else
                            rating++;
                    }
                    else
                        rating++;
                    qualifiledValue = TRUE;
                }
                else
                {
                    if (qualifiledValue)
                    {
                        if (*(p + 1) == qualifier)
                        {
                            // dvojice kvalifikatoru
                            qualifierCount++;
                            p++;
                        }
                        else
                        {
                            if ((p[1] < 0x100) && IsAlphaNumeric[p[1]])
                                rating--;
                            else
                                rating++;
                            qualifiledValue = FALSE;
                        }
                    }
                }
            }
            p++;
        }
        if (qualifierCount > 0)
        {
            // + sudy pocet kvalifikatoru v radku
            // - lichy pocet kvalifikatoru v radku
            if (qualifierCount & 0x00000001)
                rating--;
            else
                rating++;
        }
    }
    return rating / rows->Count;
}

// hledame nealfanumericky znak, ktery neni roven kvalifikatoru a zaroven
// se vyskytuje na radcich v stejnem poctu
// pokud je kvalifikator nenulovy, je mel by zaroven separator lezet pred
// oteviracim a za zaviracim kvalifikatorem

template <class CChar>
CChar CCSVParser<CChar>::AnalyseValueSeparator(const CChar* buffer, TDirectArray<WORD>* rows,
                                               CChar defaultSeparator, CCSVParserTextQualifier qualifier)
{
    if (rows->Count < 1)
        return defaultSeparator;

    int row;
    CChar qualifierChar = 0;
    if (qualifier == CSVTQ_QUOTE)
        qualifierChar = '\"';
    else if (qualifier == CSVTQ_SINGLEQUOTE)
        qualifierChar = '\'';

    // omezime pocet radku, abychom nealokovali prilis pameti
    int rowsCount = min(100, rows->Count);

    CLineRating* lines = (CLineRating*)calloc(rowsCount, sizeof(CLineRating));
    if (lines == NULL)
        return defaultSeparator;

    // je tento znak pouziva nekde v poli ratings?
    bool charUsed[256];
    ZeroMemory(charUsed, sizeof(charUsed));

    // naplnime pole ratings a charUsed
    // zajimaji nas pouze nealfanumericke znaky a jeste jen ty mimo kvalifikovany text
    // a mimo kvalifikatoru
    // pole charUsed bude obsahovat TRUE na pozici znaku, ktere jsou v nektere radce zastoupeny
    // pole lines pak obsahuje jejich pocet pro jednotlive radky
    for (row = 0; row < rowsCount; row++)
    {
        CLineRating* line = &lines[row];
        const CChar* p = buffer + rows->At(row);
        while (*p != 0)
        {
            // preskocim kvalifikovany text
            if (*p == qualifierChar)
            {
                do
                {
                    p++;
                    if (*p == qualifierChar && *(p + 1) == qualifierChar)
                    {
                        // dvojice kvalifikatoru
                        p += 2;
                        continue;
                    }
                } while (*p != 0 && *p != qualifierChar);
                if (*p != 0)
                    p++;
            }
            if (*p != 0)
            {
                if ((*p < 0x100) && !IsAlphaNumeric[*p])
                {
                    charUsed[*p] |= TRUE;
                    line->CharCount[*p]++;
                    if (p > buffer + rows->At(row) && *(p + 1) != 0)
                    {
                        // pokud pred a za znakem je kvalifikator textu, ohodnotime to kladne
                        if (*(p - 1) == qualifierChar && *(p + 1) == qualifierChar)
                            line->CharRating[*p]++;
                        // pokud pred znakem je kvalifikator textu, ohodnotime to kladne
                        if (*(p - 1) == qualifierChar)
                            line->CharRating[*p]++;
                        // pokud za znakem je kvalifikator textu, ohodnotime to kladne
                        if (*(p + 1) == qualifierChar)
                            line->CharRating[*p]++;
                    }
                }
                p++;
            }
        }
    }

    CChar separator = AnalyseSeparatorRatings(rowsCount, charUsed, lines);
    free(lines);
    if (separator != -1)
        return separator;
    else
        return defaultSeparator;
}

template <class CChar>
BOOL CCSVParser<CChar>::AnalyseFirstRowAsColumnName(const CChar* buffer, TDirectArray<WORD>* rows,
                                                    CChar defaultFirstRowAsColumnNames,
                                                    CCSVParserTextQualifier qualifier)
{
    if (rows->Count < 2)
        return defaultFirstRowAsColumnNames;

    // pokud ma prvni radek mene cislic nez radek druhy, povazujeme ho za popisovy

    double num0 = 0;
    double num1 = 0;
    const CChar* p = buffer;
    while (*p != 0)
    {
        if ((*p < 0x100) && IsAlphaNumeric[*p] && !IsAlpha[*p])
            num0++;
        p++;
    }
    p = buffer + rows->At(1);
    while (*p != 0)
    {
        if ((*p < 0x100) && IsAlphaNumeric[*p] && !IsAlpha[*p])
            num1++;
        p++;
    }

    if (num0 < num1 && (num1 - num0) > num1 / 20.0)
        return TRUE;
    else
        return FALSE;
}

template <class CChar>
void CCSVParser<CChar>::AnalyseFile(BOOL autoSeparator, CChar* separator,
                                    BOOL autoQualifier, CCSVParserTextQualifier* textQualifier,
                                    BOOL autoFirstRowAsName, BOOL* firstRowAsColumnNames)
{
    if (!autoSeparator && !autoQualifier && !autoFirstRowAsName)
        return; // neni co analysovat

    const WORD SAMPLE_BUFFER_SIZE = 60000; // velikost bufferu, ve kterem budeme analyzovat
    // !!! nesmi prekrocit 2^32, protoze na to neni staveny zbytek algoritmu

    CChar buffer[SAMPLE_BUFFER_SIZE + 1];

    size_t bytesRead; // pocet skutecne nactenych znaku do bufferu

    bytesRead = fread(buffer, sizeof(CChar), SAMPLE_BUFFER_SIZE, File);
    fseek(File, 0, SEEK_SET);
    int row = 0;
    if (bytesRead > 0)
    {
        if (bIsBigEndian && (sizeof(CChar) == 2))
            SwapWords((char*)buffer, bytesRead);
        buffer[bytesRead] = 0; // dummy

        TDirectArray<WORD> rows(100, 100);
        // vyhledam zacatky uplnych radku a jejich offsety naleju do pole rows
        size_t index = 0;
        size_t rowSeek = 0;
        CChar c;
        do
        {
            c = buffer[index];
            if (c == 0 || c == '\r' || c == '\n')
            {
                if (row == 0 || index - rowSeek > 0) // prazdne radky neberem, pokud to neni nulty radek
                {
                    rows.Add((WORD)rowSeek);
                    if (!rows.IsGood())
                    {
                        rows.ResetState();
                        break;
                    }
                }
                if ((c == '\r' && buffer[index + 1] == '\n') ||
                    (c == '\n' && buffer[index + 1] == '\r'))
                {
                    buffer[index] = 0;
                    index++;
                }
                buffer[index] = 0;
                index++;
                // zacatek pristiho radku
                rowSeek = index;
                row++;
            }
            else
                index++;
        } while (index < bytesRead);
        if (bytesRead < SAMPLE_BUFFER_SIZE && (row == 0 || index - rowSeek > 0))
            rows.Add((WORD)rowSeek);

        if (rows.IsGood() && rows.Count > 0)
        {
            // detekce kvalifikatoru hodnot
            if (autoQualifier)
                *textQualifier = AnalyseTextQualifier(buffer, &rows);

            // detekce separatoru hodnot
            if (autoSeparator)
                *separator = AnalyseValueSeparator(buffer, &rows, *separator, *textQualifier);

            // detekce prvniho radku: nazev sloupcu/hodnoty
            if (autoFirstRowAsName)
                *firstRowAsColumnNames = AnalyseFirstRowAsColumnName(buffer, &rows,
                                                                     *firstRowAsColumnNames,
                                                                     *textQualifier);
        }
        if (!rows.IsGood())
            rows.ResetState();
    }
}

template <class CChar>
CCSVParserStatus
CCSVParser<CChar>::FetchRecord(DWORD index)
{
    if (Status != CSVE_OK)
        return Status;
    if ((int)index >= Rows.Count)
    {
        Status = CSVE_SEEK_ERROR;
        return Status;
    }
    if (Buffer == NULL)
    {
        Buffer = (CChar*)malloc((BufferSize + 1) * sizeof(CChar)); // prostor pro dva terminatory
        if (Buffer == NULL)
        {
            Status = CSVE_OOM;
            return Status;
        }
    }
    __int64 pos = Rows[index] * sizeof(CChar);
    fsetpos(File, &pos);

    __int64 end = (int)index < Rows.Count - 1 ? Rows[index + 1] : FileSize;
    size_t lineLen = (size_t)min(BufferSize, (end - Rows[index]));
    size_t bytesRead = fread(Buffer, sizeof(CChar), lineLen, File);
    if (bIsBigEndian && (sizeof(CChar) == 2))
        SwapWords((char*)Buffer, bytesRead);
    lineLen = min(lineLen, bytesRead); // mohlo dojit ke zmene souboru a radka uz nemusi existovat
    // vzadu vlozim null terminatory
    CChar* p = Buffer + lineLen;
    *p = 0;
    p--;
    while (p >= Buffer && (*p == 0 || *p == '\r' || *p == '\n'))
    {
        *p = 0;
        p--;
    }

    p = Buffer;
    CChar* p2 = Buffer;
    int colIndex = 0;
    int colLen = 0;
    CChar* begin = p;
    BOOL exit = FALSE;
    CReadingStateEnum rs = rsData;
    while (!exit)
    {
        if ((TextQualifier == CSVTQ_QUOTE && *p == '\"') ||
            (TextQualifier == CSVTQ_SINGLEQUOTE && *p == '\''))
        {
            if (rs == rsData && colLen == 0)
            {
                rs = rsQualifiedData;
                p++;
                continue;
            }

            if (rs == rsQualifiedData)
            {
                rs = rsQualifiedDataFirst;
                p++;
                continue;
            }

            if (rs == rsQualifiedDataFirst)
            {
                rs = rsQualifiedData;
                colLen++;
                *p2 = *p;
                p++;
                p2++;
                continue;
            }

            colLen++;
            *p2 = *p;
            p++;
            p2++;
            continue;
        }
        if ((*p == Separator && rs != rsQualifiedData) || *p == 0)
        {
            if (colIndex < Columns.Count) // Petr: hrozi pokud se zmeni soubor behem cteni (napr. Samba na linuxu a do souboru zapisuje root)
            {
                Columns[colIndex].First = (DWORD)(begin - Buffer);
                Columns[colIndex].Length = colLen;
            }
            colLen = 0;
            colIndex++;
            begin = p2 + 1;
            rs = rsData;
            if (*p == 0)
                exit = TRUE;
        }
        else
            colLen++;
        *p2 = *p;
        p2++;
        p++;
    }
    // neexistujici sloupce nastavime jako prazdne
    int i;
    for (i = colIndex; i < Columns.Count; i++)
    {
        Columns[i].First = 0;
        Columns[i].Length = 0;
    }

    return CSVE_OK;
}

template <class CChar>
void* CCSVParser<CChar>::GetCellText(DWORD index, size_t* textLen)
{
    *textLen = Columns[index].Length;
    return Buffer + Columns[index].First;
}

//****************************************************************************
//
// CCSVParserBase
//
CCSVParserBase::~CCSVParserBase()
{
}

//****************************************************************************
//
// CCSVParserUTF8 - internally uses CCSVParser<char>
//

CCSVParserUTF8::CCSVParserUTF8(const char* filename,
                               BOOL autoSeparator, char separator,
                               BOOL autoQualifier, CCSVParserTextQualifier textQualifier,
                               BOOL autoFirstRowAsName, BOOL firstRowAsColumnNames) : parser(filename, autoSeparator, separator, autoQualifier, textQualifier,
                                                                                             autoFirstRowAsName, firstRowAsColumnNames)
{
    Buffer = (wchar_t*)malloc(10);
    BufferSize = 5;
}

CCSVParserUTF8::~CCSVParserUTF8()
{
    if (Buffer)
        free(Buffer);
}

const char* CCSVParserUTF8::GetColumnName(DWORD index)
{
    const char* name = parser.GetColumnName(index);

    if (!name)
        return NULL;

    int nameLen = (int)strlen(name) + 1;
    int len = MultiByteToWideChar(CP_UTF8, 0, name, nameLen, Buffer, BufferSize);
    if (len <= 0)
    {
        len = MultiByteToWideChar(CP_UTF8, 0, name, nameLen, NULL, 0);
        if (len > 0)
        {
            free(Buffer);
            Buffer = (wchar_t*)malloc(len * sizeof(wchar_t));
            if (Buffer)
            {
                BufferSize = len;
                len = MultiByteToWideChar(CP_UTF8, 0, name, nameLen, Buffer, BufferSize);
            }
        }
    }
    return (const char*)Buffer;
}

void* CCSVParserUTF8::GetCellText(DWORD index, size_t* textLen)
{
    LPCSTR text = (LPCSTR)parser.GetCellText(index, textLen);
    if (!text)
        return NULL;

    int len = MultiByteToWideChar(CP_UTF8, 0, text, (int)*textLen, Buffer, BufferSize);
    if (len <= 0)
    {
        len = MultiByteToWideChar(CP_UTF8, 0, text, (int)*textLen, NULL, 0);
        if (len > 0)
        {
            free(Buffer);
            Buffer = (wchar_t*)malloc(len * sizeof(wchar_t));
            if (Buffer)
            {
                BufferSize = len;
                len = MultiByteToWideChar(CP_UTF8, 0, text, (int)*textLen, Buffer, BufferSize);
            }
        }
    }
    *textLen = len;
    return (char*)Buffer;
}

//template class CCSVParser<char>;
template class CCSVParser<wchar_t>;
