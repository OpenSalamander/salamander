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
    DWORD MaxLength; // maximalni pocet znaku ve sloupci
    char* Name;      // alokovany nazev sloupce nebo NULL, pokud neexistuje
    // pouze docasne promenne plnene pri FetchRecord
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
    // vrati NULL, pokud neni prirazen; jinak vrati ukazatel na nazev terminovany nulou
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
    // vrati NULL, pokud neni prirazen; jinak vrati ukazatel na nazev terminovany nulou
    virtual const char* GetColumnName(DWORD index) { return Columns[index].Name; };
    virtual DWORD GetRecordsCnt(void) { return Rows.Count; };
    virtual DWORD GetColumnsCnt(void) { return Columns.Count; };

    virtual CCSVParserStatus FetchRecord(DWORD index) = 0;

protected:
    // pokud na columnIndex jeste neexistuje sloupec, prida novy s hodnotou MaxLength = columnLen
    // vrati FALSE, pokud se nepodarilo pridat sloupec do pole
    // pokud sloupec uz existuje, pripradi columnLen pouze v pripade, ze je vetsi nez hodnota
    // MaxLength ve sloupci stavajicim
    // vraci TRUE, pokud se pridani/nastaveni slupce zdarilo
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
    // fileName: nazev souboru,ktery bude zobrazen
    // autoSeparator: detekovat separator
    // separator: oddelovac hodnot (ma vyznam pokud je autoSeparator==FALSE nebo se nepodari detekce)
    // autoQualifier: detekovat textQualifier
    // textQualifier: znak oznacujici zacatek a konec retezce (ma vyznam pokud je autoQualifier==FALSE nebo se nepodari detekce)
    // autoFirstRowAsName: detekovat firstRowAsColumnNames
    // firstRowAsColumnNames: pokud je TRUE, obsah prvniho radku bude pouzit pro nazvy sloupcu
    //                        (ma vyznam pokud je autoFirstRowAsName==FALSE nebo se nepodari detekce)
    CCSVParser(const char* filename,
               BOOL autoSeparator, CChar separator,
               BOOL autoQualifier, CCSVParserTextQualifier textQualifier,
               BOOL autoFirstRowAsName, BOOL firstRowAsColumnNames);
    virtual ~CCSVParser();

    virtual CCSVParserStatus FetchRecord(DWORD index);
    virtual void* GetCellText(DWORD index, size_t* textLen);

private:
    // automaticka detekce vybranych hodnot
    // predpoklada otevreny soubor File; nastavi ukazovatko na jeho zacatek
    void AnalyseFile(BOOL autoSeparator, CChar* separator,
                     BOOL autoQualifier, CCSVParserTextQualifier* textQualifier,
                     BOOL autoFirstRowAsName, BOOL* firstRowAsColumnNames);

    // automaticka detekce kvalifikatoru textu
    CCSVParserTextQualifier AnalyseTextQualifier(const CChar* buffer, TDirectArray<WORD>* rows);

    double AnalyseTextQualifierAux(const CChar* buffer, TDirectArray<WORD>* rows, CChar qualifier);

    // automaticka detekce separatoru hodnot
    CChar AnalyseValueSeparator(const CChar* buffer, TDirectArray<WORD>* rows,
                                CChar defaultSeparator, CCSVParserTextQualifier qualifier);

    // automaticka detekce "prvnih radek jako nazvy sloupcu"
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
