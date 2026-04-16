// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_com) // so that the structures are independent of the current packing
#pragma pack(4)
#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression
#endif                    // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

// the plugin must define the SalamanderVersion variable (int) and initialize it in SalamanderPluginEntry:
// SalamanderVersion = salamander->GetVersion();

// global variable with the Salamander version in which this plugin is loaded
extern int SalamanderVersion;

// ****************************************************************************
// CSalamanderDirectoryAbstract
//
// class representing a directory structure - files and directories on the requested paths, the root path is "",
// path separators are backslashes ('\\')

// CQuadWord - 64-bit unsigned integer for file sizes
// tricks:
//  - faster passing of an input parameter of type CQuadWord: const CQuadWord &
//  - assign a 64-bit integer: quadWord.Value = XXX;
//  - compute a size ratio: quadWord1.GetDouble() / quadWord2.GetDouble()  // precision loss before division is small (max. 1e-15)
//  - truncate to DWORD: (DWORD)quadWord.Value
//  - convert (unsigned) __int64 to CQuadWord: CQuadWord().SetUI64(XXX)

struct CQuadWord
{
    union
    {
        struct
        {
            DWORD LoDWord;
            DWORD HiDWord;
        };
        unsigned __int64 Value;
    };

    // WARNING: do not add an assignment operator or a constructor for a single DWORD here,
    //        otherwise the use of 8-byte numbers becomes completely uncontrolled (C++ converts
    //        everything back and forth, which may not always be what you want)

    CQuadWord() {}
    CQuadWord(DWORD lo, DWORD hi)
    {
        LoDWord = lo;
        HiDWord = hi;
    }
    CQuadWord(const CQuadWord& qw)
    {
        LoDWord = qw.LoDWord;
        HiDWord = qw.HiDWord;
    }

    CQuadWord& Set(DWORD lo, DWORD hi)
    {
        LoDWord = lo;
        HiDWord = hi;
        return *this;
    }
    CQuadWord& SetUI64(unsigned __int64 val)
    {
        Value = val;
        return *this;
    }
    CQuadWord& SetDouble(double val)
    {
        Value = (unsigned __int64)val;
        return *this;
    }

    CQuadWord& operator++()
    {
        ++Value;
        return *this;
    } // prefix ++
    CQuadWord& operator--()
    {
        --Value;
        return *this;
    } // prefix --

    CQuadWord operator+(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value + qw.Value;
        return qwr;
    }
    CQuadWord operator-(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value - qw.Value;
        return qwr;
    }
    CQuadWord operator*(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value * qw.Value;
        return qwr;
    }
    CQuadWord operator/(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value / qw.Value;
        return qwr;
    }
    CQuadWord operator%(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value % qw.Value;
        return qwr;
    }
    CQuadWord operator<<(const int num) const
    {
        CQuadWord qwr;
        qwr.Value = Value << num;
        return qwr;
    }
    CQuadWord operator>>(const int num) const
    {
        CQuadWord qwr;
        qwr.Value = Value >> num;
        return qwr;
    }

    CQuadWord& operator+=(const CQuadWord& qw)
    {
        Value += qw.Value;
        return *this;
    }
    CQuadWord& operator-=(const CQuadWord& qw)
    {
        Value -= qw.Value;
        return *this;
    }
    CQuadWord& operator*=(const CQuadWord& qw)
    {
        Value *= qw.Value;
        return *this;
    }
    CQuadWord& operator/=(const CQuadWord& qw)
    {
        Value /= qw.Value;
        return *this;
    }
    CQuadWord& operator%=(const CQuadWord& qw)
    {
        Value %= qw.Value;
        return *this;
    }
    CQuadWord& operator<<=(const int num)
    {
        Value <<= num;
        return *this;
    }
    CQuadWord& operator>>=(const int num)
    {
        Value >>= num;
        return *this;
    }

    BOOL operator==(const CQuadWord& qw) const { return Value == qw.Value; }
    BOOL operator!=(const CQuadWord& qw) const { return Value != qw.Value; }
    BOOL operator<(const CQuadWord& qw) const { return Value < qw.Value; }
    BOOL operator>(const CQuadWord& qw) const { return Value > qw.Value; }
    BOOL operator<=(const CQuadWord& qw) const { return Value <= qw.Value; }
    BOOL operator>=(const CQuadWord& qw) const { return Value >= qw.Value; }

    // conversion to double (beware of precision loss for large numbers - double has only 15 significant digits)
    double GetDouble() const
    { // MSVC cannot convert unsigned __int64 to double, so this must be handled explicitly
        if (Value < CQuadWord(0, 0x80000000).Value)
            return (double)(__int64)Value; // non-negative number
        else
            return 9223372036854775808.0 + (double)(__int64)(Value - CQuadWord(0, 0x80000000).Value);
    }
};

#define QW_MAX CQuadWord(0xFFFFFFFF, 0xFFFFFFFF)

#define ICONOVERLAYINDEX_NOTUSED 15 // value for CFileData::IconOverlayIndex when the icon has no overlay

// record of each file and directory in Salamander (basic file/directory data)
struct CFileData // no destructor may be added here!
{
    char* Name;                    // allocated file name (without the path), must be allocated on
                                   // Salamander's heap (see CSalamanderGeneralAbstract::Alloc/Realloc/Free)
    char* Ext;                     // pointer into Name after the first dot from the right (including a dot at the start of the name,
                                   // on Windows it is understood as an extension, unlike on UNIX) or to the end of
                                   // Name if there is no extension; if FALSE is set in the configuration
                                   // for SALCFG_SORTBYEXTDIRSASFILES, Ext points to the end of Name for directories
                                   // (directories have no extensions)
    CQuadWord Size;                // file size in bytes
    DWORD Attr;                    // file attributes - ORed FILE_ATTRIBUTE_XXX constants
    FILETIME LastWrite;            // last-write time of the file (UTC-based time)
    char* DosName;                 // allocated DOS 8.3 file name; if not needed it is NULL, must be
                                   // allocated on Salamander's heap (see CSalamanderGeneralAbstract::Alloc/Realloc/Free)
    DWORD_PTR PluginData;          // used by the plugin through CPluginDataInterfaceAbstract; Salamander ignores it
    unsigned NameLen : 9;          // length of Name (strlen(Name)) - WARNING: the maximum name length is (MAX_PATH - 5)
    unsigned Hidden : 1;           // je hidden? (je-li 1, ikonka je pruhlednejsi o 50% - ghosted)
    unsigned IsLink : 1;           // is it a link? (if 1, the icon gets the link overlay) - standard filling: see CSalamanderGeneralAbstract::IsFileLink(CFileData::Ext); when displayed, it takes precedence over IsOffline, but IconOverlayIndex takes precedence
    unsigned IsOffline : 1;        // is it offline? (if 1, the icon gets the offline overlay - black clock); when displayed, IsLink and IconOverlayIndex take precedence
    unsigned IconOverlayIndex : 4; // icon overlay index (if the icon has no overlay, this is ICONOVERLAYINDEX_NOTUSED); when displayed, it takes precedence over IsLink and IsOffline

    // flags for Salamander internal use: they are cleared when added to CSalamanderDirectoryAbstract
    unsigned Association : 1;     // meaningful only for displaying 'simple icons' - icon of the associated file, otherwise 0
    unsigned Selected : 1;        // read-only selection flag (0 - item not selected, 1 - item selected)
    unsigned Shared : 1;          // is the directory shared? not used for files
    unsigned Archive : 1;         // is this an archive? used to display the archive icon in the panel
    unsigned SizeValid : 1;       // has the directory size already been computed?
    unsigned Dirty : 1;           // does this item need to be redrawn? (temporary only; the message queue must not be pumped between setting the bit and redrawing the panel, otherwise the icon may be redrawn by the icon reader and the bit reset; as a result the item will not be redrawn)
    unsigned CutToClip : 1;       // je CUT-nutej na clipboardu? (je-li 1, ikonka je pruhlednejsi o 50% - ghosted)
    unsigned IconOverlayDone : 1; // for icon-reader-thread use only: are we retrieving or have we already retrieved the icon overlay? (0 - no, 1 - yes)
};

// constants that specify the validity of data stored directly in CFileData (size, extension, etc.)
// or generated automatically from directly stored data (file type is generated from the extension);
// Name + NameLen are mandatory (must always be valid); PluginData validity is managed by the plugin itself
// (Salamander ignores this field)
#define VALID_DATA_EXTENSION 0x0001   // extension is stored in Ext (without it: all Ext values point to the end of Name)
#define VALID_DATA_DOSNAME 0x0002     // DOS name is stored in DosName (without it: all DosName values = NULL)
#define VALID_DATA_SIZE 0x0004        // the size in bytes is stored in Size (without it: all Size values are 0)
#define VALID_DATA_TYPE 0x0008        // file type can be generated from Ext (without it: it is not generated)
#define VALID_DATA_DATE 0x0010        // modification date (UTC-based) is stored in LastWrite (without it: all dates in LastWrite are 1.1.1602 in local time)
#define VALID_DATA_TIME 0x0020        // modification time (UTC-based) is stored in LastWrite (without it: all times in LastWrite are 0:00:00 in local time)
#define VALID_DATA_ATTRIBUTES 0x0040  // attributes are stored in Attr (ORed Win32 API FILE_ATTRIBUTE_XXX constants) (without it: all Attr values are 0)
#define VALID_DATA_HIDDEN 0x0080      // 'ghosted' icon flag is stored in Hidden (without it: all Hidden values are 0)
#define VALID_DATA_ISLINK 0x0100      // IsLink is 1 for a link; the icon gets the link overlay (without it: all IsLink values are 0)
#define VALID_DATA_ISOFFLINE 0x0200   // IsOffline is 1 for an offline file/directory; the icon gets the offline overlay (without it: all IsOffline values are 0)
#define VALID_DATA_PL_SIZE 0x0400     // meaningful only when VALID_DATA_SIZE is not used: the plugin stores the size in bytes for at least some files/directories (somewhere in PluginData); Salamander calls CPluginDataInterfaceAbstract::GetByteSize() to retrieve it
#define VALID_DATA_PL_DATE 0x0800     // meaningful only when VALID_DATA_DATE is not used: the plugin stores the modification date for at least some files/directories (somewhere in PluginData); Salamander calls CPluginDataInterfaceAbstract::GetLastWriteDate() to retrieve it
#define VALID_DATA_PL_TIME 0x1000     // meaningful only when VALID_DATA_TIME is not used: the plugin stores the modification time for at least some files/directories (somewhere in PluginData); Salamander calls CPluginDataInterfaceAbstract::GetLastWriteTime() to retrieve it
#define VALID_DATA_ICONOVERLAY 0x2000 // IconOverlayIndex is the icon overlay index (no overlay = ICONOVERLAYINDEX_NOTUSED) (without it: all IconOverlayIndex values are ICONOVERLAYINDEX_NOTUSED); for assigning overlays see CSalamanderGeneralAbstract::SetPluginIconOverlays

#define VALID_DATA_NONE 0 // helper constant - only Name and NameLen are valid

#ifdef INSIDE_SALAMANDER
// VALID_DATA_ALL and VALID_DATA_ALL_FS_ARC are for Salamander core internal use only;
// plugins should OR only constants that correspond to data supplied by the plugin
// (to avoid problems when new constants and corresponding data are added)
#define VALID_DATA_ALL 0xFFFF
#define VALID_DATA_ALL_FS_ARC (0xFFFF & ~VALID_DATA_ICONOVERLAY) // for FS and archives: everything except icon overlays
#endif                                                           // INSIDE_SALAMANDER

// If hiding hidden and system files/directories is enabled, items with
// Hidden==1 and Attr containing FILE_ATTRIBUTE_HIDDEN and/or FILE_ATTRIBUTE_SYSTEM are not shown in panels.

// CSalamanderDirectoryAbstract flag constants:
// file and directory names (including path components) are compared case-sensitively with this flag
// (without it, comparison is case-insensitive - the standard Windows behavior)
#define SALDIRFLAG_CASESENSITIVE 0x0001
// subdirectory names within each directory are not checked for duplicates (this
// check is time-consuming and is needed only in archives when items are added not only
// to the root - e.g. so that adding 'file1' to 'dir1' followed by adding
// 'dir1' works correctly: the first operation adds 'dir1' automatically as a missing path,
// and the second operation only refreshes the data for 'dir1' instead of adding it again)
#define SALDIRFLAG_IGNOREDUPDIRS 0x0002

class CPluginDataInterfaceAbstract;

class CSalamanderDirectoryAbstract
{
public:
    // clears the entire object and prepares it for further use; if 'pluginData' is not NULL, it is used
    // to free plugin-specific data (CFileData::PluginData) for files and directories;
    // sets the default value of the valid-data mask (sum of all VALID_DATA_XXX except
    // VALID_DATA_ICONOVERLAY) and object flags (see SetFlags)
    virtual void WINAPI Clear(CPluginDataInterfaceAbstract* pluginData) = 0;

    // sets the valid data mask that determines which CFileData fields are valid
    // and which should only be zeroed (see the VALID_DATA_XXX comment); the 'validData' mask
    // contains ORed VALID_DATA_XXX values; the default mask is the sum of all
    // VALID_DATA_XXX values except VALID_DATA_ICONOVERLAY; the valid data mask must be set
    // before calling AddFile/AddDir
    virtual void WINAPI SetValidData(DWORD validData) = 0;

    // sets flags for this object; 'flags' is a combination of ORed SALDIRFLAG_XXX flags;
    // the default object flag value is zero for archivers (no flags set)
    // and SALDIRFLAG_IGNOREDUPDIRS for file systems (they may add only to the root, so duplicate
    // directory checks are unnecessary)
    virtual void WINAPI SetFlags(DWORD flags) = 0;

    // adds a file to the specified path (relative to this "Salamander directory"), returns success
    // the 'path' string is used only inside the function; the contents of the file structure are used outside the function too
    // (do not free memory allocated for variables inside the structure)
    // in case of failure, the contents of the file structure must be freed;
    // the 'pluginData' parameter is not NULL only for archives (FSs use only an empty 'path' (==NULL));
    // if 'pluginData' is not NULL, 'pluginData' is used when creating new directories (if
    // 'path' does not exist), see CPluginDataInterfaceAbstract::GetFileDataForNewDir;
    // file name uniqueness on path 'path' is not checked
    virtual BOOL WINAPI AddFile(const char* path, CFileData& file, CPluginDataInterfaceAbstract* pluginData) = 0;

    // adds a directory to the specified path (relative to this "Salamander directory"), returns success
    // the 'path' string is used only inside the function; the contents of the file structure are used outside the function too
    // (do not free memory allocated for variables inside the structure)
    // in case of failure, the contents of the file structure must be freed;
    // the 'pluginData' parameter is not NULL only for archives (FSs use only an empty 'path' (==NULL));
    // if 'pluginData' is not NULL, it is used when creating new directories (if 'path' does not exist),
    // see CPluginDataInterfaceAbstract::GetFileDataForNewDir;
    // directory name uniqueness on path 'path' is checked; if an already existing
    // directory is added, the original data is freed (if 'pluginData' is not NULL,
    // CPluginDataInterfaceAbstract::ReleasePluginData is also called to free the data) and the data from 'dir' is stored
    // (this is necessary to restore directory data for directories that were created automatically because 'path' did not exist);
    // special case for FS (or an object allocated through CSalamanderGeneralAbstract::AllocSalamanderDirectory
    // with 'isForFS'==TRUE): if dir.Name is "..", the directory is added as up-dir (there can be only one,
    // it is always shown at the start of the listing and has a special icon)
    virtual BOOL WINAPI AddDir(const char* path, CFileData& dir, CPluginDataInterfaceAbstract* pluginData) = 0;

    // returns the number of files in the object
    virtual int WINAPI GetFilesCount() const = 0;

    // returns the number of directories in the object
    virtual int WINAPI GetDirsCount() const = 0;

    // returns the file at index 'index'; the returned data is read-only
    virtual CFileData const* WINAPI GetFile(int index) const = 0;

    // returns the directory at index 'index'; the returned data is read-only
    virtual CFileData const* WINAPI GetDir(int index) const = 0;

    // returns the CSalamanderDirectory object for the directory at 'index'; the returned object is read-only
    // (objects for empty directories are not allocated, so a single global empty object is returned
    // - modifying that object would affect it globally)
    virtual CSalamanderDirectoryAbstract const* WINAPI GetSalDir(int index) const = 0;

    // Lets the plugin tell Salamander the expected number of files and directories in this directory in advance.
    // Salamander adjusts its reallocation strategy so adding items does not slow down too much.
    // This is worth calling for directories containing thousands of files or directories. For tens of
    // thousands, calling this method is almost necessary, otherwise reallocations can take several seconds.
    // 'files' and 'dirs' therefore specify the approximate total number of files and directories.
    // If either value is -1, Salamander ignores it.
    // The method is useful only while the directory is empty, before AddFile or AddDir has been called.
    virtual void WINAPI SetApproximateCount(int files, int dirs) = 0;
};

//
// ****************************************************************************
// SalEnumSelection a SalEnumSelection2
//

// constants returned by SalEnumSelection and SalEnumSelection2 in the 'errorOccured' parameter
#define SALENUM_SUCCESS 0 // no error occurred
#define SALENUM_ERROR 1   // an error occurred and the user chose to continue the operation (only the faulty files/directories were skipped)
#define SALENUM_CANCEL 2  // an error occurred and the user wants to cancel the operation

// Enumerator: returns file names and ends by returning NULL;
// 'enumFiles' == -1 -> reset enumeration (after this call, enumeration starts again from the beginning); all
//                      other parameters (except 'param') are ignored; it has no return values (everything is
//                      set to zero)
// 'enumFiles' == 0 -> enumerate files and subdirectories from the root only
// 'enumFiles' == 1 -> enumerate all files and subdirectories
// 'enumFiles' == 2 -> enumerate all subdirectories, files from the root only;
// an error can occur only for 'enumFiles' == 1 or 'enumFiles' == 2 ('enumFiles' == 0 does not build
// names and paths); 'parent' is the parent of any error message boxes (NULL means do not show
// errors); 'isDir' (if not NULL) returns TRUE if the item is a directory; 'size' (if not NULL) returns
// the file size (for directories, a size is returned only for 'enumFiles' == 0; otherwise it is zero);
// if 'fileData' is not NULL, it receives a pointer to the CFileData structure of the returned
// file/directory (if the enumerator returns NULL, 'fileData' also receives NULL);
// 'param' is the 'nextParam' parameter passed together with the pointer to a function of this
// type; 'errorOccured' (if not NULL) receives SALENUM_ERROR if a too-long name is encountered while building the returned
// names and the user chooses to skip only the invalid files/directories,
// NOTE: the error does not apply to the name just returned; that one is OK; 'errorOccured' (if not NULL) receives
// SALENUM_CANCEL if, after an error, the user chooses to cancel the operation; at the same time, the
// enumerator returns NULL (ends); 'errorOccured' (if not NULL) receives SALENUM_SUCCESS if
// no error occurred
typedef const char*(WINAPI* SalEnumSelection)(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                              const CFileData** fileData, void* param, int* errorOccured);

// enumerator, returns file names and ends by returning NULL;
// 'enumFiles' == -1 -> reset enumeration (after this call, enumeration starts again from the beginning), all
//                      other parameters (except 'param') are ignored, and no output values are returned (all
//                      outputs are set to zero)
// 'enumFiles' == 0 -> enumerate files and subdirectories from the root only
// 'enumFiles' == 1 -> enumerate all files and subdirectories
// 'enumFiles' == 2 -> enumerate all subdirectories, files from the root only;
// 'enumFiles' == 3 -> enumerate all files and subdirectories + symbolic links to files have the
//                     size of the target file (for 'enumFiles' == 1 they have the size of the link itself, which is
//                     probably always zero); WARNING: 'enumFiles' must remain 3 for all calls to the enumerator;
// an error can occur only for 'enumFiles' == 1, 2, or 3 ('enumFiles' == 0 does not access the disk at all
// and does not build names and paths); 'parent' is the parent of any error message boxes
// (NULL means do not show errors); 'dosName' (if not NULL) returns the DOS name
// (8.3; only if it exists, otherwise NULL); 'isDir' (if not NULL) returns TRUE if it is a directory;
// 'size' (if not NULL) returns the file size (zero for directories); 'attr' (if not NULL)
// returns file/directory attributes; 'lastWrite' (if not NULL) returns the last-write time
// of the file/directory; 'param' is the 'nextParam' parameter passed together with the pointer to a function
// of this type; 'errorOccured' (if not NULL) receives SALENUM_ERROR if an error occurs while reading
// data from disk or if a too-long name is encountered while building the returned names
// and the user chooses to skip only the problematic files/directories; WARNING: the error does not concern the
// name just returned, which is OK; 'errorOccured' (if not NULL) receives SALENUM_CANCEL if
// on error the user chooses to cancel the operation, and the enumerator returns NULL (ends);
// 'errorOccured' (if not NULL) receives SALENUM_SUCCESS if no error occurred
typedef const char*(WINAPI* SalEnumSelection2)(HWND parent, int enumFiles, const char** dosName,
                                               BOOL* isDir, CQuadWord* size, DWORD* attr,
                                               FILETIME* lastWrite, void* param, int* errorOccured);

//
// ****************************************************************************
// CSalamanderViewAbstract
//
// sada metod Salamandera pro praci se sloupci v panelu (vypinani/zapinani/pridavani/nastavovani)

// panel view modes
#define VIEW_MODE_TREE 1
#define VIEW_MODE_BRIEF 2
#define VIEW_MODE_DETAILED 3
#define VIEW_MODE_ICONS 4
#define VIEW_MODE_THUMBNAILS 5
#define VIEW_MODE_TILES 6

#define TRANSFER_BUFFER_MAX 1024 // buffer size for transferring column contents from the plugin to Salamander
#define COLUMN_NAME_MAX 30
#define COLUMN_DESCRIPTION_MAX 100

// Column identifiers. Plugin-inserted columns have ID==COLUMN_ID_CUSTOM.
// Salamander standard columns have the other IDs.
#define COLUMN_ID_CUSTOM 0 // column provided by the plugin - the plugin is responsible for storing its data
#define COLUMN_ID_NAME 1   // left-aligned, supports FixedWidth
// left-aligned, supports FixedWidth; the separate "Ext" column can be only at index==1;
// if the column does not exist and panel data (see CSalamanderDirectoryAbstract::SetValidData())
// contains VALID_DATA_EXTENSION, the "Ext" column is shown in the "Name" column
#define COLUMN_ID_EXTENSION 2
#define COLUMN_ID_DOSNAME 3     // left-aligned
#define COLUMN_ID_SIZE 4        // right-aligned
#define COLUMN_ID_TYPE 5        // left-aligned, supports FixedWidth
#define COLUMN_ID_DATE 6        // right-aligned
#define COLUMN_ID_TIME 7        // right-aligned
#define COLUMN_ID_ATTRIBUTES 8  // right-aligned
#define COLUMN_ID_DESCRIPTION 9 // left-aligned, supports FixedWidth

// Callback that fills the buffer with characters to be shown in the corresponding column.
// For optimization, the function does not receive/return variables through parameters,
// but through global variables (CSalamanderViewAbstract::GetTransferVariables).
typedef void(WINAPI* FColumnGetText)();

// Callback for obtaining simple icon indices for FSs with custom icons (pitFromPlugin).
// For optimization reasons, the function does not receive/return variables through parameters,
// but through global variables (CSalamanderViewAbstract::GetTransferVariables).
// The callback uses only the TransferFileData and TransferIsDir global variables.
typedef int(WINAPI* FGetPluginIconIndex)();

// A column can be created in two ways:
// 1) The column was created by Salamander from the current view template.
//    In this case, 'GetText' (the fill function pointer) points into Salamander
//    and retrieves text from CFileData in the standard way.
//    The 'ID' field is different from COLUMN_ID_CUSTOM.
//
// 2) The column was added by the plugin for its own needs.
//    'GetText' points into the plugin and 'ID' equals COLUMN_ID_CUSTOM.

struct CColumn
{
    char Name[COLUMN_NAME_MAX]; // "Name", "Ext", "Size", ... column name under
                                // which the column appears in the view and menu
                                // It must not contain an empty string.
                                // POZOR: Muze obsahovat (za prvnim null-terminatorem)
                                // also the name of the "Ext" column - this happens if there is no
                                // separate "Ext" column and panel data (see
                                // CSalamanderDirectoryAbstract::SetValidData()) se
                                // contains VALID_DATA_EXTENSION.
                                // retezcu poslouzi CSalamanderGeneralAbstract::AddStrToStr().

    char Description[COLUMN_DESCRIPTION_MAX]; // Tooltip v header line
                                              // It must not contain an empty string.
                                              // POZOR: Muze obsahovat (za prvnim null-terminatorem)
                                              // also the description of the "Ext" column - this happens if there is no
                                              // separate "Ext" column and panel data (see
                                              // CSalamanderDirectoryAbstract::SetValidData()) se
                                              // contains VALID_DATA_EXTENSION.
                                              // retezcu poslouzi CSalamanderGeneralAbstract::AddStrToStr().

    FColumnGetText GetText; // callback used to obtain text (see the FColumnGetText declaration)

    // FIXME_X64 - too small for a pointer, is it ever needed?
    DWORD CustomData; // Not used by Salamander; the plugin can
                      // use it to distinguish the columns added by the plugin.

    unsigned SupportSorting : 1; // can the column be sorted?

    unsigned LeftAlignment : 1; // TRUE means the column is left-aligned; otherwise right-aligned

    unsigned ID : 4; // column identifier
                     // For standard columns provided by Salamander,
                     // it contains values other than COLUMN_ID_CUSTOM.
                     // For columns added by the plugin, it always contains
                     // the value COLUMN_ID_CUSTOM.

    // The Width and FixedWidth variables can be changed by the user while working with the panel.
    // Salamander stores/loads these values for standard columns it provides.
    // Values of these variables for plugin-provided columns must be stored/loaded
    // by the plugin itself.
    // Columns whose width Salamander calculates from their content and the user cannot
    // change are called 'elastic'. Columns whose width the user can set are called
    // 'fixed'.
    unsigned Width : 16;     // column width when in fixed (user-adjustable) width mode.
    unsigned FixedWidth : 1; // is the column in fixed (user-adjustable) width mode?

    // working variables (not stored anywhere and need not be initialized)
    // are reserved for Salamander's internal use and are ignored by plugins,
    // because their contents are not guaranteed when plugin code is called
    unsigned MinWidth : 16; // minimum width to which the column can be shrunk.
                            // It is computed from the column name and its sortability
                            // so that the column header is always visible
};

// Through this interface, the plugin can change the panel display mode when the path changes.
// All column operations apply only to the detailed modes
// (Detailed + Types + the three custom modes Alt+8/9/0). When the path changes, the
// plugin gets a standard set of columns generated from the current view template.
// The plugin can modify this set. The modification is not permanent,
// and on the next path change the plugin again receives the standard set of columns. It can,
// for example, remove one of the standard columns. Before the standard columns are rebuilt,
// the plugin gets a chance to store information about its own columns (COLUMN_ID_CUSTOM).
// It can store their 'Width' and 'FixedWidth', which the user may have changed in the panel
// (see ColumnFixedWidthShouldChange() and ColumnWidthWasChanged() in
// CPluginDataInterfaceAbstract). If the plugin changes the display mode, the change is permanent
// (e.g. switching to Thumbnails remains in effect even after leaving the plugin path).

class CSalamanderViewAbstract
{
public:
    // -------------- panel ----------------

    // returns the panel display mode (tree/brief/detailed/icons/thumbnails/tiles)
    // returns one of the VIEW_MODE_xxxx values (Detailed, Types, and the three custom modes
    // all use VIEW_MODE_DETAILED)
    virtual DWORD WINAPI GetViewMode() = 0;

    // Sets the panel mode to 'viewMode'. If it is one of the detailed modes, it may
    // remove some standard columns (see 'validData'). It is therefore best to call this
    // function first - before the other functions of this interface that modify
    // columns.
    //
    // 'viewMode' is one of the VIEW_MODE_xxxx values
    // The panel mode cannot be changed to Types or to one of the three optional detailed modes
    // (all of them are represented by the VIEW_MODE_DETAILED constant used for the Detailed panel mode).
    // However, if one of these four modes is currently selected in the panel and 'viewMode' is
    // VIEW_MODE_DETAILED, zustane tento rezim zvoleny (aneb neprepne se na Detailed rezim).
    // Changing the panel mode is permanent (it remains even after leaving the plugin path).
    //
    // 'validData' tells which data the plugin wants to display in detailed mode; the value
    // se ANDuje s maskou platnych dat zadanou pomoci CSalamanderDirectoryAbstract::SetValidData
    // (there is no point in showing columns with "zeroed" values).
    virtual void WINAPI SetViewMode(DWORD viewMode, DWORD validData) = 0;

    // Retrieves from Salamander the locations of the variables that replace the parameters of
    // CColumn::GetText. Na strane Salamandera se jedna o globalni promenne. Plugin si
    // stores pointers to them in its own global variables.
    //
    // variables:
    //   transferFileData        [IN]     data on the basis of which the item is drawn
    //   transferIsDir           [IN]     equal to 0 for a file (it is in Files),
    //                                    equal to 1 for a directory (it is in Dirs),
    //                                    equal to 2 for the up-dir symbol
    //   transferBuffer          [OUT]    output is written here, up to TRANSFER_BUFFER_MAX characters
    //                                    it does not need to be null-terminated
    //   transferLen             [OUT]    before returning from the callback, this variable is set to
    //                                    the number of filled characters without a terminator (the terminator does not
    //                                    need to be written to the buffer)
    //   transferRowData         [IN/OUT] points to a DWORD that is always cleared before columns are drawn
    //                                    for each row; it can be used for optimizations
    //                                    Salamander reserves bits 0x00000001 to 0x00000008.
    //                                    The other bits are available to the plugin.
    //   transferPluginDataIface [IN]     plugin-data interface of the panel in which the item is
    //                                    vykresluje (patri k (*transferFileData)->PluginData)
    //   transferActCustomData   [IN]     CustomData of the column for which text is being obtained (for which
    //                                    the callback is called)
    virtual void WINAPI GetTransferVariables(const CFileData**& transferFileData,
                                             int*& transferIsDir,
                                             char*& transferBuffer,
                                             int*& transferLen,
                                             DWORD*& transferRowData,
                                             CPluginDataInterfaceAbstract**& transferPluginDataIface,
                                             DWORD*& transferActCustomData) = 0;

    // only for FSs with custom icons (pitFromPlugin):
    // sets the callback for obtaining simple icon indices (see
    // CPluginDataInterfaceAbstract::GetSimplePluginIcons). If the plugin does not set this callback,
    // only the icon at index 0 is always drawn.
    // The callback uses only the TransferFileData and TransferIsDir global variables.
    virtual void WINAPI SetPluginSimpleIconCallback(FGetPluginIconIndex callback) = 0;

    // ------------- columns ---------------

    // returns the number of columns in the panel (always at least one, because Name is always shown)
    virtual int WINAPI GetColumnsCount() = 0;

    // returns a pointer to the column (read-only)
    // 'index' specifies which column is returned; returns NULL if the column at 'index' does not exist
    virtual const CColumn* WINAPI GetColumn(int index) = 0;

    // Inserts a column at position 'index'. The Name column is always at position 0,
    // and if the Ext column is shown, it is at position 1. Otherwise the column may be inserted
    // at any position. The 'column' structure is copied into Salamander's internal structures.
    // Returns TRUE if the column was inserted.
    virtual BOOL WINAPI InsertColumn(int index, const CColumn* column) = 0;

    // Inserts the standard column with ID 'id' at position 'index'. The Name column is always
    // at position 0; if the Ext column is inserted, it must be at position 1.
    // Otherwise the column may be inserted at any position. 'id' is one of the COLUMN_ID_xxxx values,
    // except COLUMN_ID_CUSTOM and COLUMN_ID_NAME.
    virtual BOOL WINAPI InsertStandardColumn(int index, DWORD id) = 0;

    // Sets the column name and description (they must not be empty strings or NULL). Their lengths
    // are limited to COLUMN_NAME_MAX and COLUMN_DESCRIPTION_MAX. Returns success.
    // POZOR: Jmeno a popis sloupce "Name" muzou obsahovat (vzdy za prvnim
    // null-terminator) also the name and description of the "Ext" column - this happens if there is no
    // separate "Ext" column and panel data (see
    // CSalamanderDirectoryAbstract::SetValidData()) se nastavi VALID_DATA_EXTENSION.
    // In this case, double strings must be set (with two
    // null-terminatory) - viz CSalamanderGeneralAbstract::AddStrToStr().
    virtual BOOL WINAPI SetColumnName(int index, const char* name, const char* description) = 0;

    // Removes the column at position 'index'. Both plugin-added columns
    // and Salamander's standard columns can be removed. The Name column, which is always
    // at index 0, cannot be removed. Be careful when removing the Ext column: if plugin data
    // (via CSalamanderDirectoryAbstract::SetValidData()) includes VALID_DATA_EXTENSION,
    // the Ext column name+description must appear under the Name column.
    virtual BOOL WINAPI DeleteColumn(int index) = 0;
};

// ****************************************************************************
// CPluginDataInterfaceAbstract
//
// set of plugin methods that Salamander needs to obtain plugin-specific data
// for columns added by the plugin (works with CFileData::PluginData)

class CPluginInterfaceAbstract;

class CPluginDataInterfaceAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct method calls (see CPluginDataInterfaceEncapsulation)
    friend class CPluginDataInterfaceEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // returns TRUE if ReleasePluginData should be called for all files associated
    // with this interface; otherwise returns FALSE
    virtual BOOL WINAPI CallReleaseForFiles() = 0;

    // returns TRUE if ReleasePluginData should be called for all directories bound
    // to this interface, otherwise returns FALSE
    virtual BOOL WINAPI CallReleaseForDirs() = 0;

    // releases plugin-specific data (CFileData::PluginData) for 'file' (file or
    // directory - 'isDir' FALSE or TRUE; structure inserted into CSalamanderDirectoryAbstract
    // when listing an archive or FS); it is called for all files if CallReleaseForFiles
    // returns TRUE, and for all directories if CallReleaseForDirs returns TRUE
    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir) = 0;

    // archive data only (FS does not add an up-dir symbol):
    // modifies the proposed contents of the up-dir symbol ('..' at the top of the panel); 'archivePath'
    // is the archive path the symbol is for; 'upDir' receives the proposed symbol
    // data: the name '..' (do not change it), the archive date&time, everything else zeroed;
    // 'upDir' returns plugin changes, mainly 'upDir.PluginData',
    // which is used by the up-dir symbol when obtaining the contents of added columns;
    // ReleasePluginData will not be called for 'upDir'; any necessary cleanup
    // can always be done on the next call to GetFileDataForUpDir or when the
    // entire interface is released (in its destructor - called from
    // CPluginInterfaceAbstract::ReleasePluginDataInterface)
    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) = 0;

    // archive data only (FS uses only the root path in CSalamanderDirectoryAbstract):
    // when a file/directory is added to CSalamanderDirectoryAbstract, the specified path may not exist,
    // in which case it must be created; the individual directories in that
    // path are created automatically, and this method lets the plugin add its specific
    // data (for its columns) to these created directories; 'dirName' is the full path
    // of the added directory in the archive; 'dir' receives the proposed data: the directory name
    // (allocated on Salamander's heap), date&time taken from the added file/directory,
    // everything else zeroed; 'dir' returns the plugin's changes, and in particular it should change
    // 'dir.PluginData'; returns TRUE if adding the plugin data succeeded, otherwise FALSE;
    // if it returns TRUE, 'dir' is freed the normal way (Salamander part +
    // ReleasePluginData), either when the listing is completely released or during
    // its creation if the same directory is later added by
    // CSalamanderDirectoryAbstract::AddDir (overwriting the automatic creation with a later
    // normal add); if it returns FALSE, only the Salamander part of 'dir' is freed
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) = 0;

    // only for FSs with custom icons (pitFromPlugin):
    // returns an image list with simple icons; while items are drawn in the panel,
    // the callback obtains the icon index into this image list; it is called after every
    // new listing is obtained (after CPluginFSInterfaceAbstract::ListCurrentPath),
    // so the image list can be rebuilt for each new listing;
    // 'iconSize' specifies the requested icon size and is one of the SALICONSIZE_xxx values
    // the plugin is responsible for destroying the image list on the next call to GetSimplePluginIcons
    // or when the whole interface is released (in its destructor - called from
    // CPluginInterfaceAbstract::ReleasePluginDataInterface)
    // if the image list cannot be created, returns NULL and the current plugin-icons-type
    // degrades to pitSimple
    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) = 0;

    // only for FSs with custom icons (pitFromPlugin):
    // returns TRUE if a simple icon should be used for the given 'file' file/directory ('isDir' FALSE/TRUE);
    // returns FALSE if GetPluginIcon should be called from the icon-loading thread
    // to obtain the icon (load the icon in the background);
    // in this method, the icon index for the simple icon may also be precomputed
    // (for icons loaded in the background, simple icons are also used until the icon is loaded)
    // and stored in CFileData (most likely in CFileData::PluginData);
    // restriction: only methods of CSalamanderGeneralAbstract that can be called
    // from any thread may be used (methods independent of panel state)
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) = 0;

    // only for FSs with custom icons (pitFromPlugin):
    // returns an icon for the 'file' file or directory, or NULL if the icon cannot be obtained; if
    // 'destroyIcon' is TRUE, the returned icon is freed by the Win32 API function DestroyIcon;
    // 'iconSize' specifies the requested icon size and is one of the SALICONSIZE_xxx values
    // restriction: because this is called from the icon-loading thread (not the main thread), only methods of
    // CSalamanderGeneralAbstract that can be called from any thread may be used
    virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon) = 0;

    // only for FSs with custom icons (pitFromPlugin):
    // compares 'file1' (may be a file or directory) and 'file2' (may be a file or directory),
    // it must not report any two listing items as equal (this ensures unique
    // assignment of a custom icon to a file/directory); if duplicate names in the listing
    // path are not possible (the usual case), it can be implemented simply as:
    // {return strcmp(file1->Name, file2->Name);}
    // returns a value less than zero if 'file1' < 'file2', zero if 'file1' == 'file2', and
    // a value greater than zero if 'file1' > 'file2';
    // restriction: because it is also called from the icon-loading thread (not only the main thread), only methods
    // of CSalamanderGeneralAbstract that can be called from any thread may be used
    virtual int WINAPI CompareFilesFromFS(const CFileData* file1, const CFileData* file2) = 0;

    // used to set view parameters; this method is always called before displaying new
    // panel contents (when the path changes) and when the current view changes (including manual
    // column width changes); 'leftPanel' is TRUE for the left panel (FALSE for the right panel);
    // 'view' is the interface for modifying the view (setting the mode, working with
    // columns); for archive data, 'archivePath' contains the current path in the archive,
    // pro data FS je 'archivePath' NULL; jde-li o data archivu, je 'upperDir' ukazatel na
    // the parent directory (if the current path is the archive root, 'upperDir' is NULL), for
    // FS je vzdy NULL;
    // POZOR: behem volani teto metody nesmi dojit k prekresleni panelu (muze se zde zmenit
    //        icon size, etc.), so no message loops (no dialogs, etc.)!
    // restriction: from CSalamanderGeneralAbstract, only methods that can be
    //          called from any thread may be used (methods independent of panel state)
    virtual void WINAPI SetupView(BOOL leftPanel, CSalamanderViewAbstract* view,
                                  const char* archivePath, const CFileData* upperDir) = 0;

    // setting a new value of "column->FixedWidth" - the user used the context menu
    // on a plugin-added column in the header line > "Automatic Column Width"; the plugin
    // should store the new value of column->FixedWidth stored in 'newFixedWidth'
    // (it is always the negation of column->FixedWidth), so that in subsequent calls to SetupView() it can
    // add the column with FixedWidth already set correctly; at the same time, if fixed
    // column width is being enabled, the plugin should set the current value of "column->Width" (so that
    // enabling fixed width this way does not change the column width) - ideally call
    // "ColumnWidthWasChanged(leftPanel, column, column->Width)"; 'column' identifikuje
    // the column that is to be changed; 'leftPanel' is TRUE for a column in the left
    // panelu (FALSE pokud jde o sloupec z praveho panelu)
    virtual void WINAPI ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column,
                                                     int newFixedWidth) = 0;

    // setting a new value of "column->Width" - the user changed the width of a plugin-added
    // column in the header line with the mouse; the plugin should store the new value of column->Width (it is stored
    // also in 'newWidth') so that in subsequent calls to SetupView() it can add the column with Width already
    // set correctly; 'column' identifies the column that changed; 'leftPanel'
    // is TRUE for a column in the left panel (FALSE for a column in the right panel)
    virtual void WINAPI ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column,
                                              int newWidth) = 0;

    // gets the Information Line contents for the 'file' file/directory ('isDir' TRUE/FALSE)
    // or for the selected files and directories ('file' is NULL and the counts of selected files/directories
    // are in 'selectedFiles'/'selectedDirs') in the panel ('panel' is one of PANEL_XXX);
    // vola se i pri prazdnem listingu (tyka se jen FS, u archivu nemuze nastat, 'file' je NULL,
    // 'selectedFiles' and 'selectedDirs' are 0); if 'displaySize' is TRUE, the size of
    // all selected directories is known (see CFileData::SizeValid; if nothing is selected, it is
    // TRUE); v 'selectedSize' je soucet cisel CFileData::Size oznacenych souboru a adresaru
    // (if nothing is selected, it is zero); 'buffer' is the buffer for the returned text (size
    // 1000 bytes); 'hotTexts' is an array (size 100 DWORDs) that returns information about the position of
    // hot-texts; the low WORD always contains the position of the hot-text in 'buffer', the high WORD contains
    // the length of the hot-text; 'hotTextsCount' contains the size of the 'hotTexts' array (100) and returns the count
    // of written hot-texts in 'hotTexts'; returns TRUE if 'buffer' + 'hotTexts' +
    // 'hotTextsCount' are set, returns FALSE if the Information Line should be filled in the standard
    // zpusobem (jako na disku)
    virtual BOOL WINAPI GetInfoLineContent(int panel, const CFileData* file, BOOL isDir, int selectedFiles,
                                           int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
                                           char* buffer, DWORD* hotTexts, int& hotTextsCount) = 0;

    // archives only: the user copied files/directories from the archive to the clipboard and is now closing
    // the archive in the panel: if the method returns TRUE, this object remains open (optimizing
    // any later Paste from the clipboard - the archive is already listed); if the method returns FALSE,
    // this object is released (any later Paste from the clipboard will cause the archive to be listed first,
    // and only then will the selected files/directories be unpacked); NOTE: if the archive file stays open for the lifetime
    // of the object, the method should return FALSE, otherwise the archive file will remain open for the entire
    // time the data stays on the clipboard (it will not be possible to delete it, etc.)
    virtual BOOL WINAPI CanBeCopiedToClipboard() = 0;

    // only when VALID_DATA_PL_SIZE is passed to CSalamanderDirectoryAbstract::SetValidData():
    // returns TRUE if the size of the 'file' file/directory ('isDir' TRUE/FALSE) is known,
    // otherwise returns FALSE; the size is returned in 'size'
    virtual BOOL WINAPI GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size) = 0;

    // only when VALID_DATA_PL_DATE is passed to CSalamanderDirectoryAbstract::SetValidData():
    // returns TRUE if the date of the 'file' file/directory ('isDir' TRUE/FALSE) is known,
    // otherwise returns FALSE; returns the date in the date part of the 'date' structure (the time part
    // should remain unchanged)
    virtual BOOL WINAPI GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date) = 0;

    // only when VALID_DATA_PL_TIME is passed to CSalamanderDirectoryAbstract::SetValidData():
    // returns TRUE if the time of the 'file' file/directory ('isDir' TRUE/FALSE) is known,
    // otherwise returns FALSE; returns the time in the time part of the 'time' structure (the date part
    // should remain unchanged)
    virtual BOOL WINAPI GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time) = 0;
};

//
// ****************************************************************************
// CSalamanderForOperationsAbstract
//
// sada metod ze Salamandera pro podporu provadeni operaci, platnost interfacu je
// omezena na metodu, ktere je interface predan jako parametr; tedy lze volat pouze
// z tohoto threadu a v teto metode (objekt je na stacku, takze po navratu zanika)

class CSalamanderForOperationsAbstract
{
public:
    // PROGRESS DIALOG: the dialog contains one or two progress bars ('twoProgressBars' FALSE/TRUE)
    // opens a progress dialog titled 'title'; 'parent' is the parent window of the progress dialog (if
    // NULL, the main window is used); if it contains only one progress bar, it can be labeled
    // as "File" ('fileProgress' is TRUE) or "Total" ('fileProgress' is FALSE)
    //
    // the dialog does not run in its own thread; for it to work properly (Cancel button + internal timer),
    // the message queue must be flushed occasionally; this is handled by the ProgressDialogAddText,
    // ProgressAddSize, and ProgressSetSize methods
    //
    // because real-time display of text and progress-bar changes slows things down considerably, the
    // ProgressDialogAddText, ProgressAddSize, and ProgressSetSize methods have a 'delayedPaint'
    // parameter; it should be TRUE for all rapidly changing text and values; the methods then store
    // the texts and display them only after the dialog's internal timer fires; set 'delayedPaint' to
    // FALSE for initial/final texts such as "preparing data..." or "canceling operation...", after
    // displaying which we do not give the dialog a chance to dispatch messages (timer events); if such
    // an operation is likely to take a long time, we should keep the dialog responsive during that time
    // by calling ProgressAddSize(CQuadWord(0, 0), TRUE) and possibly terminate the action early
    // according to its return value
    virtual void WINAPI OpenProgressDialog(const char* title, BOOL twoProgressBars,
                                           HWND parent, BOOL fileProgress) = 0;
    // writes text 'txt' (even multiple lines - it is split into lines) to the progress dialog
    virtual void WINAPI ProgressDialogAddText(const char* txt, BOOL delayedPaint) = 0;
    // if 'totalSize1' is not CQuadWord(-1, -1), sets 'totalSize1' as 100 percent of the first progress bar;
    // if 'totalSize2' is not CQuadWord(-1, -1), sets 'totalSize2' as 100 percent of the second progress bar
    // (for a progress dialog with a single progress bar, 'totalSize2' must be CQuadWord(-1, -1))
    virtual void WINAPI ProgressSetTotalSize(const CQuadWord& totalSize1, const CQuadWord& totalSize2) = 0;
    // if 'size1' is not CQuadWord(-1, -1), sets 'size1' on the first progress bar (size1/total1*100 percent);
    // if 'size2' is not CQuadWord(-1, -1), sets 'size2' on the second progress bar (size2/total2*100 percent)
    // (for a progress dialog with a single progress bar, 'size2' must be CQuadWord(-1, -1)); returns whether the
    // action should continue (FALSE = stop)
    virtual BOOL WINAPI ProgressSetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint) = 0;
    // adds 'size' to one or both progress bars (size/total*100 percent progress),
    // returns whether the action should continue (FALSE = stop)
    virtual BOOL WINAPI ProgressAddSize(int size, BOOL delayedPaint) = 0;
    // enables/disables the Cancel button
    virtual void WINAPI ProgressEnableCancel(BOOL enable) = 0;
    // returns the progress dialog HWND (useful for showing errors and prompts while the progress dialog is open)
    virtual HWND WINAPI ProgressGetHWND() = 0;
    // closes the progress dialog
    virtual void WINAPI CloseProgressDialog() = 0;

    // moves all files from the 'source' directory to the 'target' directory,
    // and also remaps displayed name prefixes ('remapNameFrom' -> 'remapNameTo')
    // returns whether the operation succeeded
    virtual BOOL WINAPI MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                                  const char* remapNameTo) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_com)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
