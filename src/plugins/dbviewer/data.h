// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// CColumn
//

class CDatabaseColumn
{
public:
    char* Name;        // allocated column name
    BOOL LeftAlign;    // TRUE = column content aligned to the left; FALSE = to the right
    BOOL Visible;      // should the column be displayed in the table?
    int Width;         // column width in points
    int OriginalIndex; // column position according to the database
    int Length;        // maximum number of characters shown in the column
    int FieldLen;      // # of bytes in the file used by this field/column
    char* Type;        // allocated description of the column type
    int Decimals;      // number of digits after the decimal point or -1 if unknown
};

//****************************************************************************
//
// CDatabase
//
// Separation between the actual database and the viewer. If the user does not
// change the sort order and wants to see all items, DBFLib is called directly.
// Otherwise the Rows array is used, which is filled on demand (hide deleted
// items, sorting) and serves for remapping.
//

class CRendererWindow;
class CParserInterfaceAbstract;

class CDatabase
{
private:
    // allocated name of the currently opened database
    char* FileName; // name of the opened file or NULL
    CParserInterfaceAbstract* Parser;
    CRendererWindow* Renderer; // pointer to the owner

    // columns read from the database structure
    TDirectArray<CDatabaseColumn> Columns;
    // indices of visible columns -- filled in UpdateColumnsInfo
    TDirectArray<DWORD> VisibleColumns;
    // set if the Columns array changed
    BOOL ColumnsDirty;
    BOOL IsUnicode;
    BOOL IsUTF8;

    int VisibleColumnCount;
    int VisibleColumnsWidth;

public:
    CDatabase();
    ~CDatabase();

    void SetRenderer(CRendererWindow* renderer) { Renderer = renderer; }

    // open a file and return TRUE on success
    BOOL Open(const char* fileName);
    void Close();

    // return TRUE if the database is open and all variables are initialized
    BOOL IsOpened() { return Parser != NULL; }

    // "", "dbf", "csv"
    const char* GetParserName();

    // return the opened database name or NULL
    const char* GetFileName() { return FileName; }

    // fill the supplied edit control with database information
    BOOL GetFileInfo(HWND hEdit);

    BOOL GetIsUnicode() { return IsUnicode; };
    BOOL GetIsUTF8() { return IsUTF8; };

    // return the number of columns read from the database
    int GetColumnCount() { return Columns.Count; }
    // return the corresponding column
    const CDatabaseColumn* GetColumn(int index) { return &Columns[index]; }
    void SetColumn(int index, const CDatabaseColumn* column)
    {
        Columns[index] = *column;
        ColumnsDirty = TRUE;
    }
    // return the corresponding visible column
    const CDatabaseColumn* GetVisibleColumn(int index);
    void SetVisibleColumn(int index, const CDatabaseColumn* column)
    {
        Columns[VisibleColumns[index]] = *column;
        ColumnsDirty = TRUE;
    }

    // compute VisibleColumnCount and VisibleColumnsWidth
    void UpdateColumnsInfo();

    // return the width of visible columns
    int GetVisibleColumnsWidth();
    // return the number of displayed columns
    int GetVisibleColumnCount();
    // return the X coordinate of a column
    int GetVisibleColumnX(int colIndex);

    // return the real number of records in the database

    // return the number of displayed rows
    int GetRowCount();

    // load the corresponding record into the buffer and return TRUE
    // on failure return FALSE and show an error for the hParent window
    BOOL FetchRecord(HWND hParent, DWORD rowIndex);

    // operations on the fetched row
    BOOL IsRecordDeleted();

    // operations on the fetched row
    // column selects the column for which the text will be retrieved
    // len is set to the row length
    const char* GetCellText(const CDatabaseColumn* column, size_t* textLen);
    const wchar_t* GetCellTextW(const CDatabaseColumn* column, size_t* textLen);
};
