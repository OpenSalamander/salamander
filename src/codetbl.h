// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ****************************************************************************

struct CCodeTablesData
{
    char* Name;      // name shown in menu, NULL means a separator
    char Table[256]; // encoding table
};

enum CCodeTableStateEnum
{
    ctsSuccessfullyLoaded, // convert.cfg was successfully loaded from directory dirName
    ctsDefaultValues       // convert.cfg could not be loaded and default values were used
};

// helper class used to load data from convert\XXX\convert.cfg
class CCodeTable
{
protected:
    TIndirectArray<CCodeTablesData> Data;
    char WinCodePage[101];            // Windows code page name (for Czech CP1250 - regional encoding)
    DWORD WinCodePageIdentifier;      // 1250, 1251, 1252, ...
    char WinCodePageDescription[101]; // human - readable description (Central Europe, West Europe & U.S.)
    char DirectoryName[MAX_PATH];     // directory name XXX: convert\XXX\convert.cfg
    CCodeTableStateEnum State;

public:
    // hWindow is the parent for message boxes;
    // dirName is the directory where convert.cfg is loaded (for example "centeuro")
    CCodeTable(HWND hWindow, const char* dirName);
    ~CCodeTable();

    CCodeTableStateEnum GetState() { return State; }

    // we will not pull all methods into this object; data will be accessed from the outside
    friend class CCodeTables;
};

class CCodeTables
{
protected:
    CRITICAL_SECTION LoadCS;    // critical section for loading data from convert.cfg (read only afterwards)
    CRITICAL_SECTION PreloadCS; // critical section for preloading data
    BOOL Loaded;
    CCodeTable* Table; // loaded table currently in use

    // used for conversion enumeration
    TIndirectArray<CCodeTable> Preloaded;

public:
    CCodeTables();
    ~CCodeTables();

    // scans the "convert" directories and fills the Preloaded array
    void PreloadAllConversions();
    // frees the Preloaded array
    void FreePreloadedConversions();
    // returns all items from the Preloaded array one by one
    BOOL EnumPreloadedConversions(int* index, const char** winCodePage,
                                  DWORD* winCodePageIdentifier,
                                  const char** winCodePageDescription,
                                  const char** dirName);

    // if an item with path dirName is found, it sets index and returns TRUE
    // otherwise it returns FALSE
    BOOL GetPreloadedIndex(const char* dirName, int* index);

    // selects the best matching item from the Preloaded array
    // cfgDirName points to the recommended directory name (from the configuration)
    // dirName must point to a buffer of MAX_PATH characters where the directory name is returned;
    // if none exists, an empty string is returned
    // criteria:
    //  1. cfgDirName
    //  2. item matching the OS code page
    //  3. westeuro
    //  4. first in the list
    //  5. if there is no item in the list, we will return an empty string
    void GetBestPreloadedConversion(const char* cfgDirName, char* dirName);

    // prepares the object for use (loads 'convert.cfg' script), hWindow is the parent window for message boxes
    // returns TRUE, only if other methods can be called
    BOOL Init(HWND hWindow);
    // returns TRUE if they are initialized
    BOOL IsLoaded() { return Loaded; }
    // fills the menu with available encodings and marks the active ones
    // encoding - 'codeType' (memory of the viewer window's encoding type)
    void InitMenu(HMENU menu, int& codeType);
    // switch to the next encoding in the list
    void Next(int& codeType);
    // switch to the previous encoding in the list
    void Previous(int& codeType);
    // fills the encoding table according to 'codeType' (memory of the viewer window's encoding type)
    // returns TRUE if 'table' was initialized (otherwise do not use 'table' and 'codeType' is set to 0)
    BOOL GetCode(char* table, int& codeType);
    // sets codeType using the 'coding' (the coding name, '-' and ' ' and '&' are ignored)
    // returns TRUE if the table was found and codeType initialized
    // otherwise 'codeType' is set to 0 and FALSE is returned
    BOOL GetCodeType(const char* coding, int& codeType);
    // checks validity of 'codeType' (range check)
    BOOL Valid(int codeType);
    // returns the name of the corresponding encoding (may contain '&' for a menu hotkey)
    BOOL GetCodeName(int codeType, char* buffer, int bufferLen);
    // returns all encodings one by one ('name' may contain '&' – menu hotkey)
    BOOL EnumCodeTables(HWND parent, int* index, const char** name, const char** table);
    // returns WinCodePage
    void GetWinCodePage(char* buf);
    // checks from the 'pattern' buffer of length 'patternLen' whether it is text (a code page exists,
    // containing only allowed characters – displayable and control) and if it is text, also determines
    // its code page (most probable)
    void RecognizeFileType(const char* pattern, int patternLen, BOOL forceText,
                           BOOL* isText, char* codePage);
    // returns the index of the conversion table from 'codePage' into WinCodePage; if not found, returns -1
    int GetConversionToWinCodePage(const char* codePage);
};

extern CCodeTables CodeTables;
