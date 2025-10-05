// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

enum CCSVParserStatus
{
    CSVE_OK,
    CSVE_OOM,
    CSVE_NOT_CSV,
    CSVE_FILE_NOT_FOUND,
    CSVE_READ_ERROR,
    CSVE_SEEK_ERROR,
    CSVE_MASK = 0xF0000000,        // special flag bits
    CSVE_SYSTEM_ERROR = 0x70000000 // lower 28 bits contain OS error
};

enum CCSVParserTextQualifier
{
    CSVTQ_QUOTE,       // "
    CSVTQ_SINGLEQUOTE, // '
    CSVTQ_NONE,        // (none)
};

struct CCSVColumn
{
    DWORD MaxLength; // maximum number of characters in the column
    char* Name;      // allocated column name or NULL if it does not exist
    // temporary variables filled during FetchRecord
    DWORD First;
    DWORD Length;
};
//****************************************************************************
//
// CCSVParser
//

class CCSVParserBase
{
public:
    virtual ~CCSVParserBase();
    virtual CCSVParserStatus GetStatus() = 0;
    virtual DWORD GetColumnMaxLen(int index) = 0;
    // returns NULL if not assigned; otherwise returns a pointer to a null-terminated name
    virtual const char* GetColumnName(DWORD index) = 0;
    virtual DWORD GetRecordsCnt(void) = 0;
    virtual DWORD GetColumnsCnt(void) = 0;
    virtual CCSVParserStatus FetchRecord(DWORD index) = 0;
    virtual void* GetCellText(DWORD index, size_t* textLen) = 0;
};

class CCSVParserCore : public CCSVParserBase
{
protected:
    CCSVParserStatus Status;
    FILE* File;
    __int64 FileSize;
    int BufferSize;
    TDirectArray<__int64> Rows;
    TDirectArray<CCSVColumn> Columns;
    CCSVParserTextQualifier TextQualifier;

public:
    CCSVParserCore();
    virtual ~CCSVParserCore();

    // GetStatus should be called after constructing the object to verify success
    virtual CCSVParserStatus GetStatus() { return Status; };
    virtual DWORD GetColumnMaxLen(int index) { return Columns[index].MaxLength; };
    // returns NULL if not assigned; otherwise returns a pointer to a null-terminated name
    virtual const char* GetColumnName(DWORD index) { return Columns[index].Name; };
    virtual DWORD GetRecordsCnt(void) { return Rows.Count; };
    virtual DWORD GetColumnsCnt(void) { return Columns.Count; };

    virtual CCSVParserStatus FetchRecord(DWORD index) = 0;

protected:
    // if the column does not exist at columnIndex, add a new one with MaxLength = columnLen
    // returns FALSE if the column could not be added to the array
    // if the column already exists, extend columnLen only when it is larger than
    // the current MaxLength value in that column
    // returns TRUE if the addition/update of the column succeeded
    BOOL SetLongerColumn(int columnIndex, DWORD columnLen);

    struct CLineRating
    {
        WORD CharCount[256];
        int CharRating[256];
    };

    int AnalyseSeparatorRatings(int rowCount, bool charUsed[], CLineRating* lines);
};

template <class CChar>
class CCSVParser : public CCSVParserCore
{
private:
    CChar* Buffer;
    CChar Separator;
    bool bIsBigEndian; // Actually used only when CChar is wchar_t

public:
    // fileName: name of the file to display
    // autoSeparator: detect the separator
    // separator: value separator (used when autoSeparator == FALSE or detection fails)
    // autoQualifier: detect the text qualifier
    // textQualifier: character marking the start and end of the string (used when autoQualifier == FALSE or detection fails)
    // autoFirstRowAsName: detect firstRowAsColumnNames
    // firstRowAsColumnNames: when TRUE, contents of the first row are used as column names
    //                        (used when autoFirstRowAsName == FALSE or detection fails)
    CCSVParser(const char* filename,
               BOOL autoSeparator, CChar separator,
               BOOL autoQualifier, CCSVParserTextQualifier textQualifier,
               BOOL autoFirstRowAsName, BOOL firstRowAsColumnNames);
    virtual ~CCSVParser();

    virtual CCSVParserStatus FetchRecord(DWORD index);
    virtual void* GetCellText(DWORD index, size_t* textLen);

private:
    // automatic detection of selected values
    // assumes that File is open; rewinds its pointer to the beginning
    void AnalyseFile(BOOL autoSeparator, CChar* separator,
                     BOOL autoQualifier, CCSVParserTextQualifier* textQualifier,
                     BOOL autoFirstRowAsName, BOOL* firstRowAsColumnNames);

    // automatic detection of the text qualifier
    CCSVParserTextQualifier AnalyseTextQualifier(const CChar* buffer, TDirectArray<WORD>* rows);

    double AnalyseTextQualifierAux(const CChar* buffer, TDirectArray<WORD>* rows, CChar qualifier);

    // automatic detection of the value separator
    CChar AnalyseValueSeparator(const CChar* buffer, TDirectArray<WORD>* rows,
                                CChar defaultSeparator, CCSVParserTextQualifier qualifier);

    // automatic detection of "first row contains column names"
    BOOL AnalyseFirstRowAsColumnName(const CChar* buffer, TDirectArray<WORD>* rows,
                                     CChar defaultFirstRowAsColumnNames, CCSVParserTextQualifier qualifier);
};

class CCSVParserUTF8 : public CCSVParserBase
{
public:
    CCSVParserUTF8(const char* filename,
                   BOOL autoSeparator, char separator,
                   BOOL autoQualifier, CCSVParserTextQualifier textQualifier,
                   BOOL autoFirstRowAsName, BOOL firstRowAsColumnNames);

    virtual ~CCSVParserUTF8();

    virtual CCSVParserStatus GetStatus() { return parser.GetStatus(); };
    virtual DWORD GetColumnMaxLen(int index) { return parser.GetColumnMaxLen(index); };
    virtual const char* GetColumnName(DWORD index);
    virtual DWORD GetRecordsCnt(void) { return parser.GetRecordsCnt(); };
    virtual DWORD GetColumnsCnt(void) { return parser.GetColumnsCnt(); };

    virtual CCSVParserStatus FetchRecord(DWORD index) { return parser.FetchRecord(index); };
    virtual void* GetCellText(DWORD index, size_t* textLen);

private:
    CCSVParser<char> parser;
    wchar_t* Buffer;
    int BufferSize;
};
