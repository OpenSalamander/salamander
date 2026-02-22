// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CDataRHItem
//

struct CDataRHItem
{
    WORD ID;    // numeric ID extracted from the RH file
    char* Name; // allocated name corresponding to this ID
    int Row;    // number of the matching line
};

//*****************************************************************************
//
// CDataRHIncompleteItem
//

struct CDataRHIncompleteItem
{
    char* Name;    // allocated name corresponding to this ID
    int Row;       // number of the matching line
    char* SymbVal; // allocated symbolic value; when this item was read the numeric value was unknown
    int AddConst;  // value added to 'SymbVal' (e.g. 5 for "IDC_CONST + 5" in the .RH file)
};

//*****************************************************************************
//
// CDataRH
//

class CDataRH
{
public:
    TDirectArray<CDataRHItem> Items;                     // constants sorted by ID
    TDirectArray<CDataRHItem> FileMarks;                 // file boundaries (ordered by file; the ID field is unused)
    TDirectArray<CDataRHIncompleteItem> IncompleteItems; // constants whose value is not known yet (relative to another constant later in the file)
    char* Data;
    DWORD DataSize;

    BOOL ContainsUnknownIdentifier; // TRUE = at least one request for a constant name missing in Symbols (forces the user to add it)

public:
    CDataRH();
    ~CDataRH();

    // load an RH file
    BOOL Load(const char* fileName);

    // populate the list box with the file contents
    void FillListBox();

    // if it exists, returns a pointer to the textual name and its numeric value
    // otherwise returns only the numeric value
    const char* GetIdentifier(WORD id, BOOL inclNum = TRUE);

    // highlight the identifier in the list box
    void ShowIdentifier(WORD id);

    // find the item located on line 'row' (1-based) in the RH file
    // returns the index in Items or -1 when missing
    int RowToIndex(int row);

    // search for the given ID using binary search; returns TRUE and the index if it succeeds
    // otherwise returns FALSE
    BOOL GetIDIndex(WORD id, int& index);

    // returns TRUE if the identifier already exists (and stores the value in 'id')
    BOOL GetIDForIdentifier(const char* identifier, WORD* id);

    void Clean();

protected:
    void CleanIncompleteItems();

    // analyse the line and add it to the array if it is a define
    BOOL ProcessLine(const char* line, const char* lineEnd, int row);

    // convert 'param' to a numeric ID; when the value is still unknown because
    // it is defined as a symbolic value with an optional offset, set 'isIncomplete' to TRUE
    // and return the symbolic value plus offset via 'incomplItem'
    BOOL GetID(const char* param, int row, WORD* id, BOOL* isIncomplete, CDataRHIncompleteItem* incomplItem);

    // sort Items by ID
    void SortItems(int left, int right);

    // find duplicate IDs
    BOOL FindEqualItems();

    // look up the file name and line of the original include file using FileMarks
    BOOL GetOriginalFile(int line, char* originalFile, int buffSize, int* originalFileLine);
};

extern CDataRH DataRH;
