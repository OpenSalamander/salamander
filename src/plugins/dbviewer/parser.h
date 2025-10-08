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
    char* Name;     // if NULL, the required length is stored into NameMax
    int NameMax;    // if Name != NULL, specifies the buffer size (NameMax equals the number of characters plus the terminator)
    BOOL LeftAlign; // where the text should be aligned when displayed
    int TextMax;    // maximum number of characters shown in this column; -1 if unknown to the parser
    int FieldLen;   // # of bytes in the file used by this field
    char* Type;     // must point to a char[100] buffer where the column type will be written
    int Decimals;   // number of digits after the decimal point; -1 if unknown
};

//****************************************************************************
//
// CParserInterfaceAbstract
//

class CParserInterfaceAbstract
{
public:
    CParserInterfaceAbstract() : bShowingError(false) {};

    // identify the parser ("dbf", "csv", ...)
    virtual const char* GetParserName() = 0;

    // called to open the requested file
    virtual CParserStatusEnum OpenFile(const char* fileName) = 0;

    // called to close the currently opened file; pairs with OpenFile
    // after CloseFile is called, the interface is considered invalid
    virtual void CloseFile() = 0;

    //
    // the following methods are meaningful only when a file is open
    //

    // fill the provided edit control with database information
    virtual BOOL GetFileInfo(HWND hEdit) = 0;

    // return the number of rows
    virtual DWORD GetRecordCount() = 0;

    // return the number of columns
    virtual DWORD GetFieldCount() = 0;

    // retrieve column information
    virtual BOOL GetFieldInfo(DWORD index, CFieldInfo* info) = 0;

    // prepare the relevant row in the buffer; this function is called before GetCellText
    virtual CParserStatusEnum FetchRecord(DWORD index) = 0;

    // called after FetchRecord and returns the text and its length from the corresponding column
    virtual const char* GetCellText(DWORD index, size_t* textLen) = 0;
    virtual const wchar_t* GetCellTextW(DWORD index, size_t* textLen) = 0;

    // called after FetchRecord and returns TRUE if the row is marked as deleted
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
    cDBF* Dbf; // interface to the DBF library

    // the following variables are valid if Dbf != NULL

    _dbf_header* DbfHdr;     // data extracted from the opened database
    _dbf_field* DbfFields;   // pointer to the list of columns
    char* Record;            // buffer used for retrieving records from the database
    char FileName[MAX_PATH]; // path to the opened file

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
    CCSVParserBase* Csv;     // interface to the CSV library
    char FileName[MAX_PATH]; // path to the opened file
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
