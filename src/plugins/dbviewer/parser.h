// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "csvlib/csvlib.h"

enum CParserStatusEnum
{
    psOK,
    psOOM, // out of memory
    psUnknownFile,
    psFileNotFound,
    psReadError,
    psWriteError,
    psSeekError,
    psNoMemoFile,
    psCorruptedMemo,
    psCount, // unknown error

    psMask = 0xF0000000,       // special flag bits
    psSystemError = 0x70000000 // lower 28 bits contain OS error
};

bool IsUTF8Encoded(const char* s, int cnt);

struct CFieldInfo
{
    char* Name;     // pokud je rovno NULL, bude nastavena pozadovana delka do NameMax
    int NameMax;    // pokud je Name != NULL, urcuje velikost bufferu (NameMax je rovno poctu znaku plus terminator)
    BOOL LeftAlign; // kam ma byt pri zobrazovani zarovnan text
    int TextMax;    // maximalni pocet znak, ktere v tomto sloupci budou zobrazeny; pokud jej parser nezna, nastavi -1
    int FieldLen;   // # of bytes in the file used by this field
    char* Type;     // musi ukazovat do char[100] pole, kam bude vepsan typ sloupce
    int Decimals;   // urcuje pocet cislic za desetinnou teckou; -1, pokud je neznamy
};

//****************************************************************************
//
// CParserInterfaceAbstract
//

class CParserInterfaceAbstract
{
public:
    CParserInterfaceAbstract() : bShowingError(false){};

    // identifikuje parser ("dbf", "csv", ...)
    virtual const char* GetParserName() = 0;

    // vola se pro otevreni pozadovaneho souboru
    virtual CParserStatusEnum OpenFile(const char* fileName) = 0;

    // vola se pro zavreni prave otevreneho souboru; paruje s OpenFile
    // po zavolani CloseFile je interface povazovany za neplatny
    virtual void CloseFile() = 0;

    //
    // nasledujici metody maji vyznam pouze je-li otevren soubor
    //

    // naplni predhozeny edit control informacema o databazi
    virtual BOOL GetFileInfo(HWND hEdit) = 0;

    // vrati pocet radku
    virtual DWORD GetRecordCount() = 0;

    // vrati pocet sloupce
    virtual DWORD GetFieldCount() = 0;

    // vytahne informaci o sloupci
    virtual BOOL GetFieldInfo(DWORD index, CFieldInfo* info) = 0;

    // pripravi do bufferu patricny radek; tato funkce je volana pred volanim GetCellText
    virtual CParserStatusEnum FetchRecord(DWORD index) = 0;

    // vola se po FetchRecord a vrati text a jeho delku z patricneho sloupce
    virtual const char* GetCellText(DWORD index, size_t* textLen) = 0;
    virtual const wchar_t* GetCellTextW(DWORD index, size_t* textLen) = 0;

    // vola se po FetchRecord a vrati TRUE, pokud je radek oznacen jako Deleted
    virtual BOOL IsRecordDeleted() = 0;

    void ShowParserError(HWND hParent, CParserStatusEnum status);

protected:
    bool bShowingError;
};

//****************************************************************************
//
// CParserInterfaceDBF
//

class cDBF;
struct _dbf_header;
struct _dbf_field;
enum tagDBFStatus;

class CParserInterfaceDBF : public CParserInterfaceAbstract
{
private:
    cDBF* Dbf; // rozhrani k DBF knihovne

    // nasledujici promenne jsou platne, pokud je Dbf != NULL

    _dbf_header* DbfHdr;     // vytazena data z otevrene databaze
    _dbf_field* DbfFields;   // ukazatel na seznam sloupcu
    char* Record;            // buffer pro tahani zaznamu z databaze
    char FileName[MAX_PATH]; // cesta otevreneho souboru

public:
    // constructor
    CParserInterfaceDBF();

    // implementation of virtual-pure methods
    virtual const char* GetParserName() { return "dbf"; }

    virtual CParserStatusEnum OpenFile(const char* fileName);
    virtual void CloseFile();

    virtual BOOL GetFileInfo(HWND hEdit);
    virtual DWORD GetRecordCount();
    virtual DWORD GetFieldCount();
    virtual BOOL GetFieldInfo(DWORD index, CFieldInfo* info);
    virtual CParserStatusEnum FetchRecord(DWORD index);
    virtual const char* GetCellText(DWORD index, size_t* textLen);
    virtual const wchar_t* GetCellTextW(DWORD index, size_t* textLen);
    virtual BOOL IsRecordDeleted();

private:
    // helpers
    CParserStatusEnum TranslateDBFStatus(tagDBFStatus status);
};

//****************************************************************************
//
// CParserInterfaceCSV
//

enum CCSVParserStatus;
struct CCSVConfig;

class CParserInterfaceCSV : public CParserInterfaceAbstract
{
private:
    CCSVParserBase* Csv;     // rozhrani k CSV knihovne
    char FileName[MAX_PATH]; // cesta otevreneho souboru
    const CCSVConfig* Config;
    BOOL IsUnicode;
    BOOL IsUTF8;

public:
    // constructor
    CParserInterfaceCSV(CCSVConfig* config);

    // implementation of virtual-pure methods
    virtual const char* GetParserName() { return "csv"; }

    virtual CParserStatusEnum OpenFile(const char* fileName);
    virtual void CloseFile();

    virtual BOOL GetFileInfo(HWND hEdit);
    BOOL GetIsUnicode() { return IsUnicode; };
    BOOL GetIsUTF8() { return IsUTF8; };
    virtual DWORD GetRecordCount();
    virtual DWORD GetFieldCount();
    virtual BOOL GetFieldInfo(DWORD index, CFieldInfo* info);
    virtual CParserStatusEnum FetchRecord(DWORD index);
    virtual const char* GetCellText(DWORD index, size_t* textLen);
    virtual const wchar_t* GetCellTextW(DWORD index, size_t* textLen);
    virtual BOOL IsRecordDeleted();

private:
    // helpers
    CParserStatusEnum TranslateCSVStatus(CCSVParserStatus status);
};
