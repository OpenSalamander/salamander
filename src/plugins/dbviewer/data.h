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
    char* Name;        // alokovany nazev sloupce
    BOOL LeftAlign;    // TRUE = obsah sloupce zarovnany vlevo; FALSE = vpravo
    BOOL Visible;      // bude sloupec zobrazen v tabulce?
    int Width;         // sirka sloupce v bodech
    int OriginalIndex; // na ktere pozici je sloupec podle databaze
    int Length;        // maximalni zobrazovany pocet znaku ve sloupci
    int FieldLen;      // # of bytes in the file used by this field/column
    char* Type;        // alokovany popis typu sloupce
    int Decimals;      // pocet cislic za desetinnout teckou nebo -1, pokud neni znam
};

//****************************************************************************
//
// CDatabase
//
// Oddeleni mezi skutecnou databazi a viewerem. V pripade, ze uzivatel nezmeni
// poradi razeni a chce videt vsechny polozky, dochazi k suchemu volani DBFLib.
// V opacnem pripade se pouzije pole Rows, ktere se na zaklade pozadavku (potlaceni
// smazanych polozek, sort) naplni a slouzi pro premapovani.
//

class CRendererWindow;
class CParserInterfaceAbstract;

class CDatabase
{
private:
    // alokovany nazev prave otevrene databaze
    char* FileName; // jmeno otevreneho souboru nebo NULL
    CParserInterfaceAbstract* Parser;
    CRendererWindow* Renderer; // ukazatel na vlastnika

    // sloupce vyctene ze struktury databaze
    TDirectArray<CDatabaseColumn> Columns;
    // index viditelnych sloupcu -- plni s v UpdateColumnsInfo
    TDirectArray<DWORD> VisibleColumns;
    // pokud doslo k zmene v poli Columns
    BOOL ColumnsDirty;
    BOOL IsUnicode;
    BOOL IsUTF8;

    int VisibleColumnCount;
    int VisibleColumnsWidth;

public:
    CDatabase();
    ~CDatabase();

    void SetRenderer(CRendererWindow* renderer) { Renderer = renderer; }

    // otevre soubor a vrati TRUE, pokud se to povedlo
    BOOL Open(const char* fileName);
    void Close();

    // vraci TRUE, pokud je databaze otevrena a vsechny promenne inicializovany
    BOOL IsOpened() { return Parser != NULL; }

    // "", "dbf", "csv"
    const char* GetParserName();

    // vraci nazev otevrene databaze nebo NULL
    const char* GetFileName() { return FileName; }

    // naplni predhozeny edit control informacema o databazi
    BOOL GetFileInfo(HWND hEdit);

    BOOL GetIsUnicode() { return IsUnicode; };
    BOOL GetIsUTF8() { return IsUTF8; };

    // vraci pocet sloupcu nactenych z databaze
    int GetColumnCount() { return Columns.Count; }
    // vraci odpovidajici sloupec
    const CDatabaseColumn* GetColumn(int index) { return &Columns[index]; }
    void SetColumn(int index, const CDatabaseColumn* column)
    {
        Columns[index] = *column;
        ColumnsDirty = TRUE;
    }
    // vraci odpovidajici viditelny sloupec
    const CDatabaseColumn* GetVisibleColumn(int index);
    void SetVisibleColumn(int index, const CDatabaseColumn* column)
    {
        Columns[VisibleColumns[index]] = *column;
        ColumnsDirty = TRUE;
    }

    // napocita VisibleColumnCount a VisibleColumnsWidth
    void UpdateColumnsInfo();

    // vraci sirku viditelnych sloupcu
    int GetVisibleColumnsWidth();
    // vraci pocet zobrazenych sloupcu
    int GetVisibleColumnCount();
    // vraci X souradnici sloupce
    int GetVisibleColumnX(int colIndex);

    // vraci skutecny pocet recordu v databazi

    // vraci pocet zobrazovanych radku
    int GetRowCount();

    // vytahne do bufferu odpovidajici zaznam a vrati TRUE
    // v pripade chyby vrati FALSE a zobrazi chybu k oknu hParent
    BOOL FetchRecord(HWND hParent, DWORD rowIndex);

    // operace nad vytazenym radkem
    BOOL IsRecordDeleted();

    // operace nad vytazenym radkem
    // column urcuje sloupce, pro ktery bude vytazen text
    // do promenne len bude nastavena delka radku
    const char* GetCellText(const CDatabaseColumn* column, size_t* textLen);
    const wchar_t* GetCellTextW(const CDatabaseColumn* column, size_t* textLen);
};
