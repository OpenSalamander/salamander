// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CDataRHItem
//

struct CDataRHItem
{
    WORD ID;    // ciselne ID vytazene z RH souboru
    char* Name; // alokovany nazev odpovidajici tomuto ID
    int Row;    // cislo odpovidajici radky
};

//*****************************************************************************
//
// CDataRHIncompleteItem
//

struct CDataRHIncompleteItem
{
    char* Name;    // alokovany nazev odpovidajici tomuto ID
    int Row;       // cislo odpovidajici radky
    char* SymbVal; // alokovana symbolicka hodnota v dobe cteni teto polozky nebyla znama jeji hodnota
    int AddConst;  // kolik se ma pridat k hodnote 'SymbVal' (napr. 5 pokud bylo v .RH "IDC_CONST + 5")
};

//*****************************************************************************
//
// CDataRH
//

class CDataRH
{
public:
    TDirectArray<CDataRHItem> Items;                     // konstanty (serazene podle podle ID)
    TDirectArray<CDataRHItem> FileMarks;                 // rozdeleni do souboru (poradi dle souboru, ID se nepouziva)
    TDirectArray<CDataRHIncompleteItem> IncompleteItems; // konstanty, kterych hodnota jeste neni znama (jsou relativni k jine konstante uvedene dale v souboru)
    char* Data;
    DWORD DataSize;

    BOOL ContainsUnknownIdentifier; // TRUE = aspon jeden dotaz na jmeno konstanty, ktera neni v symbols (donutime obsluhu ji tam doplnit)

public:
    CDataRH();
    ~CDataRH();

    // nacte RH soubor
    BOOL Load(const char* fileName);

    // naleje soubor do listboxu
    void FillListBox();

    // pokud existuje, vrati ukazatel na textovy nazev IDcka a jeho numerickou hodnotu
    // jinak vrati jen jeho numerickou hodnotu
    const char* GetIdentifier(WORD id, BOOL inclNum = TRUE);

    // v listboxu ukaze patricny identifikator
    void ShowIdentifier(WORD id);

    // vyhleda polozku lezici na dane radce 'row' (zacina od jedne) v RH souboru
    // vraci index do pole Items nebo -1 pokud ji nenasel
    int RowToIndex(int row);

    // binarnim pulenim hleda dane ID; vrati TRUE a index pokud ho najde
    // jinak vrati FALSE
    BOOL GetIDIndex(WORD id, int& index);

    // vraci TRUE, pokud identifikator jiz existuje (zaroven naplni jeho hodnotu do 'id')
    BOOL GetIDForIdentifier(const char* identifier, WORD* id);

    void Clean();

protected:
    void CleanIncompleteItems();

    // provede analysu radky a pokud jde o define, prida ho do pole
    BOOL ProcessLine(const char* line, const char* lineEnd, int row);

    // prevede hodnotu 'param' na numericke id, pokud je hodnota zatim neznama, protoze
    // je definovana jako symbolicka hodnota s pripadnym offsetem, vraci 'isIncomplete'
    // TRUE a symbolickou hodnotu s offsetem vraci v 'incomplItem'
    BOOL GetID(const char* param, int row, WORD* id, BOOL* isIncomplete, CDataRHIncompleteItem* incomplItem);

    // seradi pole Items podle ID
    void SortItems(int left, int right);

    // najde redundance ID
    BOOL FindEqualItems();

    // na zaklade pole FileMarks dohleda jmeno/radek originalniho include souboru
    BOOL GetOriginalFile(int line, char* originalFile, int buffSize, int* originalFileLine);
};

extern CDataRH DataRH;
