// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// Used to read (and optionally modify) the VERSIONINFO resource. Reading alone 
// could be handled by the GetFileVersionInfo/VerQueryValue API, but that API does not support later modifications,
// so we solve the problem with our own module. Using the API would also mean 
// linking Version.LIB/DLL, which we do not use for anything else.
// WARNING: the module exists both in Salamander and in Translator.

// If the following variable is defined, the module supports writing in addition to reading.
#define VERSINFO_SUPPORT_WRITE

// Can save the resource to disk; used for module debugging purposes.
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
    VOID* Value;    // depends on Type
    WORD ValueSize; // used only for Var blocks, otherwise we compute it
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

    // Loads VERSIONINFO from the specified module.
    BOOL ReadResource(HINSTANCE hInstance, int resID);

    // QueryValue extracts data from the resource.
    // 'block' - see FindBlock
    BOOL QueryValue(const char* block, BYTE** buffer, DWORD* size);

    // Extracts a string from the StringFileInfo section and converts the Unicode string on the fly.
    // 'block' - see FindBlock.
    BOOL QueryString(const char* block, char* buffer, DWORD maxSize, WCHAR* bufferW = NULL, DWORD maxSizeW = 0);

#ifdef VERSINFO_SUPPORT_WRITE
    // Sets the string into the 'block' block; returns TRUE on success, otherwise FALSE.
    // The block must already exist.
    BOOL SetString(const char* block, const char* buffer);

    // Allocates a block of memory, prepares the VERSIONINFO stream, and updates the resource.
    BOOL UpdateResource(HANDLE hUpdateRes, int resID);
#endif //VERSINFO_SUPPORT_WRITE

#ifdef VERSINFO_SUPPORT_DEBUG
    BOOL WriteResourceToFile(HINSTANCE hInstance, int resID, const char* fileName);
#endif //VERSINFO_SUPPORT_DEBUG

private:
    // ptr: points into the VS_VERSIONINFO stream at the block to be loaded.
    // parent: NULL for VS_VERSIONINFO, otherwise pointer to the parent block.
    CVersionBlock* LoadBlock(const BYTE*& ptr, CVersionBlock* parent);

    // Locates a block.
    // 'block' is the input parameter and specifies what to retrieve:
    //   "\" returns a pointer to VS_FIXEDFILEINFO
    //   "\VarFileInfo\Translation" returns a pointer to a DWORD
    //   "\StringFileInfo\lang-codepage\string-name" returns a pointer to the value (UNICODE)
    CVersionBlock* FindBlock(const char* block);

#ifdef VERSINFO_SUPPORT_WRITE
    // Recursive helper for building the VERSIONINFO stream.
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
