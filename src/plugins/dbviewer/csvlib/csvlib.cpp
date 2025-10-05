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

// only an optimization detail
extern BOOL IsAlphaNumeric[256]; // TRUE/FALSE array for characters (FALSE = not a letter nor a digit)
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

// block size used when reading the file
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
    // score the characters used and search for the one with the highest rating
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
                    // if the number of characters in the current and next row is the same and greater than zero,
                    // reward it
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
    __int64 rowSeek = 0; // row position within the file

    Buffer = NULL;

    File = fopen(filename, "rb");
    if (File == NULL)
    {
        Status = (CCSVParserStatus)(GetLastError() | CSVE_SYSTEM_ERROR);
        //Status = CSVE_FILE_NOT_FOUND;
        return;
    }

    // retrieve the file size
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
        // the user wants to detect some of the parameters
        AnalyseFile(autoSeparator, &separator,
                    autoQualifier, &textQualifier,
                    autoFirstRowAsName, &firstRowAsColumnNames);
    }

    Separator = separator;
    TextQualifier = textQualifier;

    CChar buffer[READ_BUFFER_SIZE];

    size_t bytesRead; // number of characters actually read into the buffer

    // where the iterator is currently positioned
    CReadingStateEnum rs = rsData;

    int rowLen = 0;      // length of the row currently being read
    int maxRowLen = 0;   // length of the longest row
    DWORD columnLen = 0; // number of characters in the current column
    int columnIndex = 0; // current column index

    fseek(File, (long)rowSeek * sizeof(CChar), SEEK_SET);

    do
    {
        // read in windows of READ_BUFFER_SIZE
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
                    // if this is the end of a row
                    if ((rs == rsNewLineR && c == '\n') ||
                        (rs == rsNewLineN && c == '\r'))
                    {
                        // if it's the second character, just skip over it
                        rowSeek++;
                        rs = rsNewLine;
                    }
                    else
                    {
                        // if it's the first character, add the row
                        Rows.Add(rowSeek);
                        if (!Rows.IsGood())
                        {
                            Rows.ResetState();
                            Status = CSVE_OOM;
                            return;
                        }
                        // if there is a new column, store it
                        if (!SetLongerColumn(columnIndex, columnLen))
                        {
                            Status = CSVE_OOM;
                            return;
                        }

                        rowSeek += rowLen + 1;
                        rowLen = 0;
                        columnLen = 0;
                        columnIndex = 0;
                        // prepare to receive the second character of the line ending
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

                // the second character of the line ending might not arrive -> reset the state
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
                    // if there is a new column, store it
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
        // if there is a new column, store it
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

// Attempts to detect the text qualifier.
//   buffer: complete buffer
//   rows:   offsets from the beginning of 'buffer' for individual rows
//
// Search procedure:
//   the qualifier can take three values: CSVTQ_QUOTE, CSVTQ_SINGLEQUOTE, CSVTQ_NONE
//   the rows are scanned for the characters ' and " and both variants are scored
//     + even number of qualifiers in every row
//     - odd number of qualifiers in every row
//     if a qualifier stands alone (it is not a pair)
//     + no alphanumeric character before the opening and after the closing qualifier
//     - an alphanumeric character before the opening or after the closing qualifier

template <class CChar>
CCSVParserTextQualifier
CCSVParser<CChar>::AnalyseTextQualifier(const CChar* buffer, TDirectArray<WORD>* rows)
{
    if (rows->Count == 0)
        return CSVTQ_NONE;
    // try the double-quote variant
    double quoteRating = AnalyseTextQualifierAux(buffer, rows, '\"');
    // try the single-quote variant
    double singleQuoteRating = AnalyseTextQualifierAux(buffer, rows, '\'');
    // if both are below the threshold, return an empty qualifier
    if (quoteRating <= 100 && singleQuoteRating <= 100)
        return CSVTQ_NONE;
    // choose the qualifier with the higher weight
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
        BOOL qualifiledValue = FALSE; // are we inside a qualified value?
        while (*p != 0)
        {
            if (*p == qualifier)
            {
                qualifierCount++;

                // if a qualifier stands alone (not forming a pair)
                // + no alphanumeric character before the opening and after the closing qualifier
                // - an alphanumeric character before the opening or after the closing qualifier
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
                            // a pair of qualifiers
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
            // + even number of qualifiers in the row
            // - odd number of qualifiers in the row
            if (qualifierCount & 0x00000001)
                rating--;
            else
                rating++;
        }
    }
    return rating / rows->Count;
}

// Look for a non-alphanumeric character that is not equal to the qualifier and
// that appears on the rows in the same count.
// If the qualifier is non-zero, the separator should also appear before the
// opening qualifier and after the closing qualifier.

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

    // limit the number of rows to avoid allocating too much memory
    int rowsCount = min(100, rows->Count);

    CLineRating* lines = (CLineRating*)calloc(rowsCount, sizeof(CLineRating));
    if (lines == NULL)
        return defaultSeparator;

    // is this character used anywhere in the ratings array?
    bool charUsed[256];
    ZeroMemory(charUsed, sizeof(charUsed));

    // fill the ratings and charUsed arrays
    // we care only about non-alphanumeric characters and only those outside qualified text
    // and outside the qualifiers themselves
    // the charUsed array will contain TRUE at positions of characters present in any row
    // the lines array then contains their counts for individual rows
    for (row = 0; row < rowsCount; row++)
    {
        CLineRating* line = &lines[row];
        const CChar* p = buffer + rows->At(row);
        while (*p != 0)
        {
            // skip the qualified text
            if (*p == qualifierChar)
            {
                do
                {
                    p++;
                    if (*p == qualifierChar && *(p + 1) == qualifierChar)
                    {
                        // pair of qualifiers
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
                        // if a text qualifier is before and after the character, reward it
                        if (*(p - 1) == qualifierChar && *(p + 1) == qualifierChar)
                            line->CharRating[*p]++;
                        // if a text qualifier is before the character, reward it
                        if (*(p - 1) == qualifierChar)
                            line->CharRating[*p]++;
                        // if a text qualifier is after the character, reward it
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

    // if the first row has fewer digits than the second row, treat it as descriptive

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
        return; // nothing to analyze

    const WORD SAMPLE_BUFFER_SIZE = 60000; // buffer size used for the analysis
    // must not exceed 2^32, because the rest of the algorithm is not built for it

    CChar buffer[SAMPLE_BUFFER_SIZE + 1];

    size_t bytesRead; // number of characters actually read into the buffer

    bytesRead = fread(buffer, sizeof(CChar), SAMPLE_BUFFER_SIZE, File);
    fseek(File, 0, SEEK_SET);
    int row = 0;
    if (bytesRead > 0)
    {
        if (bIsBigEndian && (sizeof(CChar) == 2))
            SwapWords((char*)buffer, bytesRead);
        buffer[bytesRead] = 0; // dummy

        TDirectArray<WORD> rows(100, 100);
        // find the beginnings of complete rows and store their offsets in the rows array
        size_t index = 0;
        size_t rowSeek = 0;
        CChar c;
        do
        {
            c = buffer[index];
            if (c == 0 || c == '\r' || c == '\n')
            {
                if (row == 0 || index - rowSeek > 0) // skip empty rows unless it is the first row
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
                // start of the next row
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
            // detect the text qualifier
            if (autoQualifier)
                *textQualifier = AnalyseTextQualifier(buffer, &rows);

            // detect the value separator
            if (autoSeparator)
                *separator = AnalyseValueSeparator(buffer, &rows, *separator, *textQualifier);

            // detect whether the first row contains column names or values
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
        Buffer = (CChar*)malloc((BufferSize + 1) * sizeof(CChar)); // space for two terminators
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
    lineLen = min(lineLen, bytesRead); // the file might have changed and the row may no longer exist
    // append null terminators at the end
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
            if (colIndex < Columns.Count) // Petr: may happen if the file changes while reading (e.g. Samba on Linux with root writing to the file)
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
    // mark non-existent columns as empty
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
