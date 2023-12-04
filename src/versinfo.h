// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// slouzi pro cteni (a pripadnou modifikaci) resource VERSIONINFO; pro cteni
// by bylo mozne pouzit API GetFileVersionInfo/VerQueryValue, ale nasledna modifikace
// neni podporena, takze problem resime vlastnim modulem navic by pouziti API
// znamenalo linkovani Version.LIB/DLL, kterou na nic jineho nepouzivame
// POZOR: modul se vyskytuje jak v Salanderu, tak v Translatoru

// pokud je definovana nasledujici promenna, bude modul podporovat vedle cteni take zapis
#define VERSINFO_SUPPORT_WRITE

// umi ulozit resource na disk; slouzi pro ladici ucely modulu
#define VERSINFO_SUPPORT_DEBUG

// VERSIONINFO
typedef struct tagVsVersionInfo
{
    WORD wLength;
    WORD wValueLength;
    WORD wType;
    WCHAR szKey[1];
} VERSIONINFO, *LPVERSIONINFO;

enum CVersionBlockType
{
    vbtVersionInfo,
    vbtStringFileInfo,
    vbtStringTable,
    vbtString,
    vbtVarFileInfo,
    vbtVar,
    vbtUNKNOWN
};

//*****************************************************************************
//
// CVersionBlock
//

class CVersionBlock
{
public:
    CVersionBlockType Type;
    WCHAR* Key;
    BOOL Text;      // 1 if the version resource contains text data and 0 if the version resource contains binary data
    VOID* Value;    // zalezi na Type
    WORD ValueSize; // pouzivame pouze pro Var, jinak pocitam
    TIndirectArray<CVersionBlock> Children;

public:
    CVersionBlock();
    ~CVersionBlock();

    BOOL SetKey(const WCHAR* key);
    BOOL SetValue(const BYTE* value, int size);
    BOOL AddChild(CVersionBlock* block);
};

//*****************************************************************************
//
// CVersionInfo
//

class CVersionInfo
{
private:
    CVersionBlock* Root;

public:
    CVersionInfo();
    ~CVersionInfo();

    // nacte VERSIONINFO ze specifikovaneho modulu
    BOOL ReadResource(HINSTANCE hInstance, int resID);

    // QueryValue slouzi k vytazeni dat z resource
    // 'block' viz FindBlock
    BOOL QueryValue(const char* block, BYTE** buffer, DWORD* size);

    // vytahne retezec ze sekce StringFileInfo, ktery rovnou konvertuje retezec z Unicode
    // 'block' viz FindBlock
    BOOL QueryString(const char* block, char* buffer, DWORD maxSize, WCHAR* bufferW = NULL, DWORD maxSizeW = 0);

#ifdef VERSINFO_SUPPORT_WRITE
    // nastavi retezec do blocku 'block'; vraci TRUE v pripade uspechu, jinak FALSE
    // blok musi jiz existovat
    BOOL SetString(const char* block, const char* buffer);

    // alokuje kus pameti, pripravi VERSIONINFO stream a updatne resource
    BOOL UpdateResource(HANDLE hUpdateRes, int resID);
#endif //VERSINFO_SUPPORT_WRITE

#ifdef VERSINFO_SUPPORT_DEBUG
    BOOL WriteResourceToFile(HINSTANCE hInstance, int resID, const char* fileName);
#endif //VERSINFO_SUPPORT_DEBUG

private:
    // ptr: ukazuje do streamu VS_VERSIONINFO na blok, ktery se ma nacist
    // parent: NULL pokud jde o VS_VERSIONINFO, jinak ukazatel na rodice
    CVersionBlock* LoadBlock(const BYTE*& ptr, CVersionBlock* parent);

    // vyhleda blok
    // 'block' je vstupni parametr a udava co se ma ziskat
    //   "\" vrati ukazatel na VS_FIXEDFILEINFO
    //   "\VarFileInfo\Translation" vrati ukazatel na DWORD
    //   "\StringFileInfo\lang-codepage\string-name" vrati ukazatel na hodnotu (UNICODE)
    CVersionBlock* FindBlock(const char* block);

#ifdef VERSINFO_SUPPORT_WRITE
    // rekurzivni funkce pro build VERSIONINFO streamu
    BOOL SaveBlock(CVersionBlock* block, BYTE*& ptr, const BYTE* maxPtr);
#endif //VERSINFO_SUPPORT_WRITE
};

// VS_VERSIONINFO:
// 2 bytes: Length in bytes (this block, and all child blocks. does _not_ include alignment padding between subsequent blocks)
// 2 bytes: Length in bytes of VS_FIXEDFILEINFO struct
// 2 bytes: Type (contains 1 if version resource contains text data and 0 if version resource contains binary data)
// Variable length unicode string (null terminated): Key (currently "VS_VERSION_INFO")
// Variable length padding to align VS_FIXEDFILEINFO on a 32-bit boundary
// VS_FIXEDFILEINFO struct
// Variable length padding to align Child struct on a 32-bit boundary
// Child struct (zero or one StringFileInfo structs, zero or one VarFileInfo structs)

// StringFileInfo:
// 2 bytes: Length in bytes (includes this block, as well as all Child blocks)
// 2 bytes: Value length (always zero)
// 2 bytes: Type (contains 1 if version resource contains text data and 0 if version resource contains binary data)
// Variable length unicode string: Key (currently "StringFileInfo")
// Variable length padding to align Child struct on a 32-bit boundary
// Child structs ( one or more StringTable structs.  Each StringTable struct's Key member indicates the appropriate language and code page for displaying the text in that StringTable struct.)

// StringTable:
// 2 bytes: Length in bytes (includes this block as well as all Child blocks, but excludes any padding between String blocks)
// 2 bytes: Value length (always zero)
// 2 bytes: Type (contains 1 if version resource contains text data and 0 if version resource contains binary data)
// Variable length unicode string: Key. An 8-digit hex number stored as a unicode string.  The four most significant digits represent the language identifier.  The four least significant digits represent the code page for which the data is formatted.
// Variable length padding to align Child struct on a 32-bit boundary
// Child structs (an array of one or more String structs (each aligned on a 32-bit boundary)

// String:
// 2 bytes: Length in bytes (of this block)
// 2 bytes: Value length (the length in words of the Value member)
// 2 bytes: Type (contains 1 if version resource contains text data and 0 if version resource contains binary data)
// Variable length unicode string: Key. arbitrary string, identifies data.
// Variable length padding to align Value on a 32-bit boundary
// Value: Variable length unicode string, holding data.

// VarFileInfo:
// 2 bytes: Length in bytes (includes this block, as well as all Child blocks)
// 2 bytes: Value length (always zero)
// 2 bytes: Type (contains 1 if version resource contains text data and 0 if version resource contains binary data)
// Variable length unicode string: Key (currently "VarFileInfo")
// Variable length padding to align Child struct on a 32-bit boundary
// Child structs (a Var struct)

// Var:
// 2 bytes: Length in bytes of this block
// 2 bytes: Value length in bytes of the Value
// 2 bytes: Type (contains 1 if version resource contains text data and 0 if version resource contains binary data)
// Variable length unicode string: Key ("Translation")
// Variable length padding to align Value on a 32-bit boundary
// Value: an array of one or more 4 byte values that are language and code page identifier pairs, low-order word containing a language identifier, and the high-order word containing a code page number.  Either word can be zero, indicating that the file is language or code page independent.
