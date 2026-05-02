// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Provides reading (and optional modification) of the VERSIONINFO resource.
// The information could be read with GetFileVersionInfo/VerQueryValue, but
// updating is not supported by that API, so we use a custom helper instead.
// Using the API would also require linking Version.LIB/DLL, which we do not
// otherwise depend on. Note: the module is shared between Salamander and the
// Translator.

// Define the following flag to allow writing the resource in addition to reading it.
#define VERSINFO_SUPPORT_WRITE

// Enable dumping the resource to disk for debugging purposes.
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
    VOID* Value;    // Interpretation depends on Type.
    WORD ValueSize; // Used only for Var blocks; other blocks compute it dynamically.
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
    WORD TLangID;

public:
    CVersionInfo();
    ~CVersionInfo();

    // Load VERSIONINFO from the specified module.
    BOOL ReadResource(HINSTANCE hInstance, int resID);

    // QueryValue extracts raw data from the resource.
    // See FindBlock for the syntax of 'block'.
    BOOL QueryValue(const char* block, BYTE** buffer, DWORD* size);

    // Fetch a string from StringFileInfo and convert it from Unicode immediately.
    // See FindBlock for the syntax of 'block'.
    BOOL QueryString(const char* block, wchar_t* buffer, DWORD maxSize);

#ifdef VERSINFO_SUPPORT_WRITE
    // Store a string into the given block; returns TRUE on success.
    // The block must already exist.
    BOOL SetString(const char* block, const wchar_t* buffer);

    // Allocate a buffer, build the VERSIONINFO stream, and update the resource.
    BOOL UpdateResource(HANDLE hUpdateRes, int resID);
#endif //VERSINFO_SUPPORT_WRITE

#ifdef VERSINFO_SUPPORT_DEBUG
    BOOL WriteResourceToFile(HINSTANCE hInstance, int resID, const char* fileName);
#endif //VERSINFO_SUPPORT_DEBUG

private:
    // ptr: points into the VS_VERSIONINFO stream at the block to load.
    // parent: NULL for the root VS_VERSIONINFO, otherwise the parent block.
    CVersionBlock* LoadBlock(BYTE*& ptr, CVersionBlock* parent);

    // Locate a block.
    // 'block' specifies the item to retrieve:
    //   "\" returns a pointer to VS_FIXEDFILEINFO
    //   "\VarFileInfo\Translation" returns a pointer to a DWORD
    //   "\StringFileInfo\lang-codepage\string-name" returns a pointer to a Unicode value
    CVersionBlock* FindBlock(const char* block);

#ifdef VERSINFO_SUPPORT_WRITE
    // Recursive helper that builds the VERSIONINFO stream.
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
