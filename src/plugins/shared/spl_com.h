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
#pragma pack(push, enter_include_spl_com) // so that the structures are independent of the alignment setting
#pragma pack(4)
#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression
#endif                    // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

// in the plugin, it is neccessary to define the variable SalamanderVersion (int) and initialize
// this variable in SalamanderPluginEntry:
// SalamanderVersion = salamander->GetVersion();

// global variable with the version of Salamander in which this plugin is loaded
extern int SalamanderVersion;

//
// ****************************************************************************
// CSalamanderDirectoryAbstract
//
// Class representing a directory structure - files and directories in the desired paths, root path is "",
// separators in the path are backslashes ('\\')
//

// CQuadWord - 64-bit unsigned integer for file sizes
// tricks:
//  -faster passing of an input parameter of type CQuadWord: const CQuadWord &
//  -assign a 64-bit integer: quadWord.Value = XXX;
//  -calculate the size ratio: quadWord1.GetDouble() / quadWord2.GetDouble() // the loss of precision before division will be small (max. 1e-15)
//  -trim to DWORD: (DWORD)quadWord.Value
//  -convert (unsigned) __int64 to CQuadWord: CQuadWord().SetUI64(XXX)

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

    // CAUTION: The assignment operator or constructor for a single DWORD must not be used here,
    //          otherwise the use of 8-bit numbers will be completely uncontrollable (C++ converts everything
    //          mutually, which may not always be correct)

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

    // Conversion to double (note the loss of precision for large numbers - double has only 15 valid digits)
    double GetDouble() const
    { // MSVC does not know how to convert unsigned __int64 to double, so we have to help ourselves
        if (Value < CQuadWord(0, 0x80000000).Value)
            return (double)(__int64)Value; // positive number
        else
            return 9223372036854775808.0 + (double)(__int64)(Value - CQuadWord(0, 0x80000000).Value);
    }
};

#define QW_MAX CQuadWord(0xFFFFFFFF, 0xFFFFFFFF)

#define ICONOVERLAYINDEX_NOTUSED 15 // value for CFileData::IconOverlayIndex in case the icon has no overlay

// record of each file and directory in Salamander (basic data about the file/directory)
struct CFileData // destructor must not be here!
{
    char* Name;                    // allocated file name (without path), must be allocated on the heap
                                   // of the Salamander (see CSalamanderGeneralAbstract::Alloc/Realloc/Free)
    char* Ext;                     // pointer to the Name after the first dot from the right (including the dot at the beginning of the name,
                                   // on Windows it is understood as an extension, unlike UNIX) or at the end
                                   // of Name if the extension does not exist; if FALSE is set in the configuration
                                   // for SALCFG_SORTBYEXTDIRSASFILES, Ext for directories is a pointer to the end
                                   // of Name (directories have no extensions)
    CQuadWord Size;                // file size in bytes
    DWORD Attr;                    // file attributes - ORed constants FILE_ATTRIBUTE_XXX
    FILETIME LastWrite;            // time of the last write to the file (UTC-based time)
    char* DosName;                 // allocated DOS 8.3 file name, if not needed, it is NULL, must be allocated on the heap
                                   // of the Salamander (see CSalamanderGeneralAbstract::Alloc/Realloc/Free)
    DWORD_PTR PluginData;          // used by a plugin through CPluginDataInterfaceAbstract, Salamander ignores
    unsigned NameLen : 9;          // length of the Name string (strlen(Name)) - WARNING: the maximum length of the name is (MAX_PATH - 5)
    unsigned Hidden : 1;           // is hidden? (if 1, the icon is 50% transparent - ghosted)
    unsigned IsLink : 1;           // is links? (if 1, the icon has a link overlay) - standard filling see CSalamanderGeneralAbstract::IsFileLink(CFileData::Ext), has priority over IsOffline when displayed, but IconOverlayIndex has priority
    unsigned IsOffline : 1;        // is offline? (if 1, the icon has an offline overlay - black clock), when displayed, both IsLink and IconOverlayIndex have priority
    unsigned IconOverlayIndex : 4; // index of the icon overlay (if the icon has no overlay, the value is ICONOVERLAYINDEX_NOTUSED), when displayed, it has priority over IsLink and IsOffline

    // flags for internal use in Salamander: reset when added to CSalamanderDirectoryAbstract
    unsigned Association : 1;     // makes sense only for displaying 'simple icons' - icon associated with the file, otherwise 0
    unsigned Selected : 1;        // read-only flag of selection (0 - item not selected, 1 - item selected)
    unsigned Shared : 1;          // is the directory shared? not used for files
    unsigned Archive : 1;         // is it an archive? used to display the archive icon in the panel
    unsigned SizeValid : 1;       // is the size of the directory counted for the directory?
    unsigned Dirty : 1;           // is it necessary to redraw this item? (temporary validity; between setting the bit and redrawing the panel, the message queue must not be pumped, otherwise the icon (icon reader) may be redrawn and thus the bit reset! as a result, the item is not redrawn)
    unsigned CutToClip : 1;       // is it CUT to the clipboard? (if 1, the icon is 50% more transparent - ghosted)
    unsigned IconOverlayDone : 1; // only for the needs of the icon reader thread: have we already obtained the icon overlay? (0 - no, 1 - yes)
};

// constants determining the validity of data that is directly stored in CFileData (size, extension, etc.)
// or are generated from directly stored data automatically (file-type is generated from the extension);
// Name + NameLen are mandatory (must be valid at all times); the validity of PluginData is controlled by the plugin itself
// (Salamander ignores this attribute)
#define VALID_DATA_EXTENSION 0x0001   // extension is stored in Ext (if not set: all Ext = end of Name)
#define VALID_DATA_DOSNAME 0x0002     // DOS name is stored in DosName (if not set: all DosName = NULL)
#define VALID_DATA_SIZE 0x0004        // size in bytes is stored in Size (if not set: all Size = 0)
#define VALID_DATA_TYPE 0x0008        // file-type can be generated from Ext (if not set: not generated)
#define VALID_DATA_DATE 0x0010        // modification date (UTC-based) is stored in LastWrite (if not set: all dates in LastWrite are 1.1.1602 in local time)
#define VALID_DATA_TIME 0x0020        // modification time (UTC-based) is stored in LastWrite (if not set: all times in LastWrite are 0:00:00 in local time)
#define VALID_DATA_ATTRIBUTES 0x0040  // attributes are stored in Attr (ORed Win32 API constants FILE_ATTRIBUTE_XXX) (if not set: all Attr = 0)
#define VALID_DATA_HIDDEN 0x0080      // "ghosted" icon flag is stored in Hidden (if not set: all Hidden = 0)
#define VALID_DATA_ISLINK 0x0100      // IsLink contains 1 if it is a link, the icon has a link overlay (without: all IsLink = 0)
#define VALID_DATA_ISOFFLINE 0x0200   // IsOffline contains 1 if it is an offline file/directory, the icon has an offline overlay (without: all IsOffline = 0)
#define VALID_DATA_PL_SIZE 0x0400     // makes sense only without using VALID_DATA_SIZE: the plugin has the size of the file/directory stored in bytes (somewhere in PluginData), to get this size, Salamander calls CPluginDataInterfaceAbstract::GetByteSize()
#define VALID_DATA_PL_DATE 0x0800     // makes sense only without using VALID_DATA_DATE: the plugin has the modification date of the file/directory stored (somewhere in PluginData), to get this size, Salamander calls CPluginDataInterfaceAbstract::GetLastWriteDate()
#define VALID_DATA_PL_TIME 0x1000     // makes sense only without using VALID_DATA_TIME: the plugin has the modification time of the file/directory stored (somewhere in PluginData), to get this size, Salamander calls CPluginDataInterfaceAbstract::GetLastWriteTime()
#define VALID_DATA_ICONOVERLAY 0x2000 // IconOverlayIndex is the index of the icon overlay (no overlay = ICONOVERLAYINDEX_NOTUSED value) (if not used: all IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED), icon assignment see CSalamanderGeneralAbstract::SetPluginIconOverlays

#define VALID_DATA_NONE 0 // helper constant - valid for Name and NameLen only

#ifdef INSIDE_SALAMANDER
// VALID_DATA_ALL and VALID_DATA_ALL_FS_ARC are only for internal use in Salamander (core),
// plugins only OR constants corresponding to the dates supplied by the plugins (this prevents
// problems when introducing additional constants and their corresponding dates)

#define VALID_DATA_ALL 0xFFFF
#define VALID_DATA_ALL_FS_ARC (0xFFFF & ~VALID_DATA_ICONOVERLAY) // for FS and archives: everything except icon-overlays
#endif                                                           // INSIDE_SALAMANDER

// if hiding of hidden and system files and directories is turned on, items with Hidden==1 and Attr
// containing FILE_ATTRIBUTE_HIDDEN and/or FILE_ATTRIBUTE_SYSTEM are not displayed in the panels

// constants of flags for CSalamangerDirectoryAbstract:
// names of files and directories (even in paths) are to be compared case-sensitive (without this flag,
// comparison is case-insensitive - standard behavior in Windows)

#define SALDIRFLAG_CASESENSITIVE 0x0001
// names of subdir in each directory are not tested for duplicity (this test is time-consuming and is
// necessary only in archives, if items are added not only to the root - to make it work, for example,
// adding "file1" to "dir1" followed by adding "dir1" - "dir1" is added by the first operation
// (the non-existent path is automatically added), the second operation only restores the data about "dir1"
// (it must not be added again)
#define SALDIRFLAG_IGNOREDUPDIRS 0x0002

class CPluginDataInterfaceAbstract;

class CSalamanderDirectoryAbstract
{
public:
    // clear the entire object, prepare it for further use; if 'pluginData' is not NULL, it is used
    // for files and directories to release data specific to plugins (CFileData::PluginData);
    // sets the standard value of the mask of valid data (the sum of all VALID_DATA_XXX except
    // VALID_DATA_ICONOVERLAY) and the flag of the object (see the SetFlags method)
    virtual void WINAPI Clear(CPluginDataInterfaceAbstract* pluginData) = 0;

    // set the mask of valid data, according to which it is determined which data from CFileData are valid
    // and which should only be "zeroed" (see the comment for VALID_DATA_XXX); the mask 'validData'
    // contains ORed values VALID_DATA_XXX; the standard value of the mask is the sum of all
    // VALID_DATA_XXX except VALID_DATA_ICONOVERLAY; the mask of valid data must be set before calling
    // AddFile/AddDir
    virtual void WINAPI SetValidData(DWORD validData) = 0;

    // set the flag of the object; 'flags' is a combination of ORed flags SALDIRFLAG_XXX, the standard
    // value of the object flag is zero for archivers (no flag is set) and SALDIRFLAG_IGNOREDUPDIRS
    // for file systems (can only be added to the root, the test for duplicate directories is unnecessary)
    virtual void WINAPI SetFlags(DWORD flags) = 0;

    // add a file to the specified path (relative to this "salamander directory"), returns success
    // the string 'path' is used only inside the function, the contents of the file structure are
    // used outside the function as well (do not release the memory allocated for variables inside the structure)
    // (if case of failure, the contents of the file structure must be released;
    // the 'pluginData' parameter is not NULL only for archives (FS only use empty 'path' (==NULL));
    // if 'pluginData' is not NULL, it is used to create new directories (if 'path' does not exist),
    // see CPluginDataInterfaceAbstract::GetFileDataForNewDir;
    // the uniqueness of the file name on the path 'path' is not checked
    virtual BOOL WINAPI AddFile(const char* path, CFileData& file, CPluginDataInterfaceAbstract* pluginData) = 0;

    // add a directory to the specified path (relative to this "salamander directory"), returns success
    // the string 'path' is used only inside the function, the contents of the file structure are
    // used outside the function as well (do not release the memory allocated for variables inside the structure)
    // (if case of failure, the contents of the file structure must be released;
    // the 'pluginData' parameter is not NULL only for archives (FS only use empty 'path' (==NULL));
    // if 'pluginData' is not NULL, it is used to create new directories (if 'path' does not exist),
    // see CPluginDataInterfaceAbstract::GetFileDataForNewDir;
    // the uniqueness of the directory name on the path 'path' is checked, if a directory with the same
    // name already exists, the original data are released (if 'pluginData' is not NULL, CPluginDataInterfaceAbstract::ReleasePluginData
    // is called to release the data) and the data from 'dir' are saved (it is necessary to restore the data
    // of the directory, which is automatically created when 'path' does not exist);
    // speciality for FS (or an object allocated via CSalamanderGeneralAbstract::AllocSalamanderDirectory
    // with 'isForFS'==TRUE): if dir.Name == "..", the directory is added as an up-dir (there can be only
    // one, it is always displayed at the beginning of the listing and has a special icon)
    virtual BOOL WINAPI AddDir(const char* path, CFileData& dir, CPluginDataInterfaceAbstract* pluginData) = 0;

    // returns the number of files in the object
    virtual int WINAPI GetFilesCount() const = 0;

    // returns the number of directories in the object
    virtual int WINAPI GetDirsCount() const = 0;

    // returns the files from the 'index' index, the returned data can only be used for reading
    virtual CFileData const* WINAPI GetFile(int index) const = 0;

    // returns the directory from the 'index' index, the returned data can only be used for reading
    virtual CFileData const* WINAPI GetDir(int index) const = 0;

    // returns the object CSalamanderDirectory for the directory from the 'index' index, the returned object
    // can only be used for reading (objects for empty directories are not allocated, one global empty object is returned
    // - changing this object would be reflected globally)
    virtual CSalamanderDirectoryAbstract const* WINAPI GetSalDir(int index) const = 0;

    // allows to inform the Plugin about expected number of files and directories in this directory beforehand.
    // Salamander will adjust its reallocation strategy to avoid slowdowns when adding items. Makes sense only
    // for directories containing thousands of files or directories. For tens of thousands of items it is
    // almost a necessity, otherwise reallocations take several seconds. 'files' and 'dirs' express approximate
    // total number of files and directories. If any of the values is -1, Salamander will ignore it.
    // This method makes sense only if the directory is empty, i.e. AddFile or AddDir was not called.
    virtual void WINAPI SetApproximateCount(int files, int dirs) = 0;
};

//
// ****************************************************************************
// SalEnumSelection a SalEnumSelection2
//

// constants returned from SalEnumSelection a SalEnumSelection2 in the 'errorOccured' parameter
#define SALENUM_SUCCESS 0 // error did not occur
#define SALENUM_ERROR 1   // error occurred and the user wants to continue the operation (only incorrectly named files/directories were skipped)
#define SALENUM_CANCEL 2  // error occurred and the user wants to cancel the operation

// enumerator, returns file names, ends with NULL;
// 'enumFiles' == -1 -> enumeration reset (after this call, the enumeration starts again from the beginning),
//                      all other parameters (except 'param') are ignored, no return values (everything is set to zero)
// 'enumFiles' == 0 -> enumeration of files and subdirectories only from the root
// 'enumFiles' == 1 -> enumeration of all files and subdirectories
// 'enumFiles' == 2 -> enumeration of all subdirectories, files only from the root;
// an error can occur only when 'enumFiles' == 1 or 'enumFiles' == 2 ('enumFiles' == 0 does not make
// names and paths complete); 'parent' is the parent of any error message boxes (NULL means not to show errors);
// 'isDir' (if not NULL) returns TRUE if it is a directory; 'size' (if not NULL) returns the size of the file
// (for directories it only returns the size when 'enumFiles' == 0 - otherwise it is zero);
// if 'fileData' is not NULL, it returns a pointer to the CFileData structure of the returned file/directory
// (if the enumerator returns NULL, NULL is also returned in 'fileData');
// 'param' is the parameter 'nextParam' passed together with the pointer to the function of this type;
// 'errorOccured' (if not NULL) returns SALENUM_ERROR, if a very long name was encountered during the
// compilation of the returned names and the user decided to skip only the incorrect files/directories,
// CAUTION: the error does not apply to the name returned, it is OK; 'errorOccured' (if not NULL) returns
// SALENUM_CANCEL if the user decided to cancel the operation (cancel), at the same time the enumerator
// returns NULL (ends); 'errorOccured' (if not NULL) returns SALENUM_SUCCESS if no error occurred
typedef const char*(WINAPI* SalEnumSelection)(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                              const CFileData** fileData, void* param, int* errorOccured);

// enumerator, returns file names, ends with NULL;
// 'enumFiles' == -1 -> enumeration reset (after this call, the enumeration starts again from the beginning),
//                     all other parameters (except 'param') are ignored, no return values (everything is set to zero)
// 'enumFiles' == 0 -> enumeration of files and subdirectories only from the root
// 'enumFiles' == 1 -> enumeration of all files and subdirectories
// 'enumFiles' == 2 -> enumeration of all subdirectories, files only from the root;
// 'enumFiles' == 3 -> enumeration of all files and subdirectories, symbolic links to files have
//                     the size of the target file (for 'enumFiles' == 1 they have the size of the link,
//                     which is probably always zero); CAUTION: 'enumFiles' must remain 3 for all calls
//                     to the enumerator;
// an error can occur only when 'enumFiles' == 1, 2 or 3 ('enumFiles' == 0 does not work with the disk
// or complete names and paths); 'parent' is the parent of any error message boxes (NULL means not to show errors);
// 'dosName' (if not NULL) returns the DOS name (8.3; only if it exists, otherwise NULL); 'isDir' (if not NULL)
// returns TRUE if it is a directory; 'size' (if not NULL) returns the size of the file (for directories
// returns zero); 'attr' (if not NULL) returns the attributes of the file/directory; 'lastWrite' (if not NULL)
// returns the time of the last write to the file/directory; 'param' is the parameter 'nextParam' passed
// together with the pointer to the function of this type; 'errorOccured' (if not NULL) returns SALENUM_ERROR,
// if an error occurred while reading data from the disk or if a very long name was encountered during the
// compilation of the returned names and the user decided to skip only the incorrect files/directories,
// CAUTION: the error does not apply to the name returned, it is OK; 'errorOccured' (if not NULL) returns
// SALENUM_CANCEL if the user decided to cancel the operation (cancel), at the same time the enumerator
// returns NULL (ends); 'errorOccured' (if not NULL) returns SALENUM_SUCCESS if no error occurred
typedef const char*(WINAPI* SalEnumSelection2)(HWND parent, int enumFiles, const char** dosName,
                                               BOOL* isDir, CQuadWord* size, DWORD* attr,
                                               FILETIME* lastWrite, void* param, int* errorOccured);

//
// ****************************************************************************
// CSalamanderViewAbstract
//
// set of Salamander methods for working with columns in the panel (turning off/on/adding/setting)

// panel view modes
// rezimy pohledu panelu
#define VIEW_MODE_TREE 1
#define VIEW_MODE_BRIEF 2
#define VIEW_MODE_DETAILED 3
#define VIEW_MODE_ICONS 4
#define VIEW_MODE_THUMBNAILS 5
#define VIEW_MODE_TILES 6

#define TRANSFER_BUFFER_MAX 1024 // buffer size for transferring column contents from the plugin to Salamander
#define COLUMN_NAME_MAX 30
#define COLUMN_DESCRIPTION_MAX 100

// Columns identifiers. Columns added by the plugin have ID==COLUMN_ID_CUSTOM.
// Salamander default columns have other IDs.
#define COLUMN_ID_CUSTOM 0 // the column is provided by the plugin - the plugin will take care of saving its data
#define COLUMN_ID_NAME 1   // aligned to the left, supports FixedWidth
// aligned to the left, supports FixedWidth; separate column "Ext" can only be at index==1;
// if the column does not exist and VALID_DATA_EXTENSION is set in the panel data (see CSalamanderDirectoryAbstract::SetValidData()),
// the column "Ext" is displayed in the column "Name"
#define COLUMN_ID_EXTENSION 2
#define COLUMN_ID_DOSNAME 3     // aligned to the left
#define COLUMN_ID_SIZE 4        // aligned to the right
#define COLUMN_ID_TYPE 5        // aligned to the left, supports FixedWidth
#define COLUMN_ID_DATE 6        // aligned to the right
#define COLUMN_ID_TIME 7        // aligned to the right
#define COLUMN_ID_ATTRIBUTES 8  // aligned to the right
#define COLUMN_ID_DESCRIPTION 9 // aligned to the left, supports FixedWidth

// Callback for filling the buffer with characters to be displayed in the corresponding column.
// For optimization reasons, the function does not receive/return variables via parameters,
// but via global variables (CSalamanderViewAbstract::GetTransferVariables).
typedef void(WINAPI* FColumnGetText)();

// Callback to get index of simple icons for FS with custom icons (pitFromPlugin).
// For optimization reasons, the function does not receive/return variables via parameters,
// but via global variables (CSalamanderViewAbstract::GetTransferVariables).
// Only TransferFileData and TransferIsDir are used from the global variables.
typedef int(WINAPI* FGetPluginIconIndex)();

// column can only be created by these two ways:
// 1) Salamander creates a column based on the template of the current view.
//    In this case, the 'GetText' pointer (to the filler function) points to Salamander
//    and gets the texts from CFileData.
//    The value of the 'ID' variable is different from COLUMN_ID_CUSTOM.
//
// 2) The plugin added the column based on its needs.
//   'GetText' points to the plugin and 'ID' is equal to COLUMN_ID_CUSTOM.

struct CColumn
{
    char Name[COLUMN_NAME_MAX]; // "Name", "Ext", "Size", ... name of column, under
                                // which the column appears in the view and in the menu
                                // Must not contain an empty string.
                                // CAUTION: It may contain (after the first null-terminator)
                                // also the name of the column "Ext" - this happens if there is no
                                // separate column "Ext" and in the data of the panel (see
                                // CSalamanderDirectoryAbstract::SetValidData()) VALID_DATA_EXTENSION is set.
                                // To join two strings, use CSalamanderGeneralAbstract::AddStrToStr().

    char Description[COLUMN_DESCRIPTION_MAX]; // Tooltip in header line
                                              // Must not contain an empty string.
                                              // CAUTION: It may contain (after the first null-terminator)
                                              // also the description of the column "Ext" - this happens if there is no
                                              // separate column "Ext" and in the data of the panel (see
                                              // CSalamanderDirectoryAbstract::SetValidData()) VALID_DATA_EXTENSION is set.
                                              // To join two strings, use CSalamanderGeneralAbstract::AddStrToStr().

    FColumnGetText GetText; // callback to get the text (description at the FColumnGetText type declaration)

    // FIXME_X64 - small for pointer, isn't it sometimes needed?
    DWORD CustomData; // not used by Salamander; the plugin can use it to distinguish its added columns.

    unsigned SupportSorting : 1; // can the column be sorted?

    unsigned LeftAlignment : 1; // for TRUE, the column is aligned to the left; otherwise to the right

    unsigned ID : 4; // column identifier
                     // For standard columns provided by Salamander
                     // it contains values other than COLUMN_ID_CUSTOM.
                     // For columns added by the plugin, it always contains
                     // the value COLUMN_ID_CUSTOM.
    // The variables Width and FixedWidth can be changed by the user during work with the panel.
    // Standard columns provided by Salamander have the saving/loading of these values
    // ensured. The values of these variables for columns provided by the plugin must be saved/loaded
    // within the plugin.
    // Columns whose width is calculated by Salamander based on the content and the user cannot
    // change it are called 'elastic'. Columns for which the user can set the width are called
    // 'fixed'.
    unsigned Width : 16;     // Column width in case of fixed (adjustable) width.
    unsigned FixedWidth : 1; // Is the column in fixed (adjustable) width mode?

    // working variables (not saved anywhere and it is not necessary to initialize them)
    // they are intended for internal needs of Salamander and plugins ignore them,
    // because their content is not guaranteed when calling plugins
    unsigned MinWidth : 16; // Minimal width to which the column can be narrowed.
                            // It is calculated based on the column name and its sortability
                            // so that the column header is always visible
};

// Plugin can change the mode of displaying the panel via this interface when the path changes.
// All work with columns concerns only all detailed modes (Detailed + Types + three optional modes Alt+8/9/0).
// When the path changes, the plugin gets the standard set of columns generated based on the template
// of the current view. The plugin can modify this set. The modification is not permanent and when the
// path changes again, the plugin gets the standard set of columns again. It can, for example, remove
// some of the std. columns. Before filling the std. columns again, the plugin gets the opportunity
// to save information about its columns (COLUMN_ID_CUSTOM). It can save their 'Width' and 'FixedWidth',
// which the user could set in the panel (see ColumnFixedWidthShouldChange() and ColumnWidthWasChanged()
// in the interface CPluginDataInterfaceAbstract). If the plugin changes the view mode, the change is permanent
// (e.g. switching to the Thumbnails mode remains even after leaving the plugin path).

class CSalamanderViewAbstract
{
public:
    // -------------- panel ----------------

    // returns mode in which the panel is displayed (tree/brief/detailed/icons/thumbnails/tiles)
    // returns one of the values VIEW_MODE_xxxx (mode Detailed, Types and three optional modes are
    // all VIEW_MODE_DETAILED)
    virtual DWORD WINAPI GetViewMode() = 0;

    // set the panel mode to 'viewMode'. If it is one of the detailed modes, it can remove some of
    // the standard columns (see 'validData'). Therefore, it is appropriate to call this function
    // first - before other functions from this interface that modify columns.
    //
    // 'viewMode' is one of the values VIEW_MODE_xxxx
    // The panel mode cannot be changed to Types or one of the three optional detailed modes
    // (all are represented by the constant VIEW_MODE_DETAILED used in the Detailed mode of the panel).
    // However, if one of these four modes is currently selected in the panel and 'viewMode' is
    // VIEW_MODE_DETAILED, this mode remains selected (or in other words, it does not switch to
    // the Detailed mode).
    // Changing the panel mode is permanent (it persists even after leaving the plugin path).
    //
    // 'validData' informs about what data the plugin wants to display in the detailed mode, the value
    // is ANDed with the mask of valid data specified by CSalamanderDirectoryAbstract::SetValidData
    // (it does not make sense to display columns with "zeroed" values).
    virtual void WINAPI SetViewMode(DWORD viewMode, DWORD validData) = 0;

    // Get locations of variables from Salamander that replace callback parameters
    // CColumn::GetText. On the Salamander side, these are global variables. The plugin
    // saves pointers to them in its own global variables.
    //
    // variables:
    //   transferFileData        [IN]     data based on which the item is to be drawn
    //   transferIsDir           [IN]     equal to 0 if it is a file (it is in the array Files),
    //                                    equal to 1 if it is a directory (it is in the array Dirs),
    //                                    equal to 2 if it is an up-dir symbol
    //   transferBuffer          [OUT]    data are poured into this buffer, maximum TRANSFER_BUFFER_MAX characters
    //                                    it is not necessary to terminate them with a zero
    //   transferLen             [OUT]    before returning from the callback, the number of filled characters
    //                                    without a terminator is set in this variable (the terminator does not
    //                                    need to be written to the buffer)
    //   transferRowData         [IN/OUT] points to DWORD, which is always zeroed before drawing columns
    //                                    for each row; can be used for optimizations
    //                                    Salamander has reserved bits 0x00000001 to 0x00000008.
    //                                    Other bits are available for the plugin.
    //   transferPluginDataIface [IN]     plugin-data-interface of the panel to which the item is drawn
    //                                    (belongs to (*transferFileData)->PluginData)
    //   transferActCustomData   [IN]     CustomData of the column for which the text is obtained (for which
    //                                    the callback is called)
    virtual void WINAPI GetTransferVariables(const CFileData**& transferFileData,
                                             int*& transferIsDir,
                                             char*& transferBuffer,
                                             int*& transferLen,
                                             DWORD*& transferRowData,
                                             CPluginDataInterfaceAbstract**& transferPluginDataIface,
                                             DWORD*& transferActCustomData) = 0;

    // for FS with custom icons only (pitFromPlugin):
    // Sets callback for getting index of simple icons (see CPluginDataInterfaceAbstract::GetSimplePluginIcons).
    // If this callback is not set by the plugin, only icon from index 0 will be drawn.
    // Only TransferFileData and TransferIsDir are used from the global variables.
    virtual void WINAPI SetPluginSimpleIconCallback(FGetPluginIconIndex callback) = 0;

    // ------------- columns ---------------

    // returns the number of columns in the panel (always at least one, because the column "Name" is always displayed)
    virtual int WINAPI GetColumnsCount() = 0;

    // returns a pointer to the column (read-only)
    // 'index' specifies which of the columns will be returned; if the column 'index' does not exist, NULL is returned
    virtual const CColumn* WINAPI GetColumn(int index) = 0;

    // returns column to the position 'index'. The column Name is always placed at position 0,
    // if the column Ext is displayed, it will be at position 1. Otherwise, the column can be placed
    // anywhere. The structure 'column' will be copied to the Salamander internal structures.
    // Returns TRUE if the column was inserted.
    virtual BOOL WINAPI InsertColumn(int index, const CColumn* column) = 0;

    // inserts the standard column with ID 'id' at position 'index'. The column Name is always placed
    // at position 0, if the column Ext is inserted, it must be at position 1. Otherwise, the column
    // can be placed anywhere. 'id' is one of the values COLUMN_ID_xxxx, except COLUMN_ID_CUSTOM and
    // COLUMN_ID_NAME.
    virtual BOOL WINAPI InsertStandardColumn(int index, DWORD id) = 0;

    // sets the name and description of the column (cannot be empty strings or NULL). The length of
    // the string is limited to COLUMN_NAME_MAX and COLUMN_DESCRIPTION_MAX. Returns success.
    // CAUTION: The name and description of the column "Name" may contain (always after the first
    // null-terminator) also the name and description of the column "Ext" - this happens if there is
    // no separate column "Ext" and in the data of the panel (see CSalamanderDirectoryAbstract::SetValidData())
    // VALID_DATA_EXTENSION is set. In this case, it is necessary to set double strings (with two
    // null-terminators) - see CSalamanderGeneralAbstract::AddStrToStr().
    virtual BOOL WINAPI SetColumnName(int index, const char* name, const char* description) = 0;

    // removes column at position 'index'. It is possible to remove both columns added by the plugin
    // and standard columns provided by Salamander. The column 'Name' cannot be removed, it is always
    // at position 0. Be careful when removing the column 'Ext', if the plugin data (see
    // CSalamanderDirectoryAbstract::SetValidData()) VALID_DATA_EXTENSION is set, the column 'Ext'
    // must be displayed in the column 'Name'.
    virtual BOOL WINAPI DeleteColumn(int index) = 0;
};

//
// ****************************************************************************
// CPluginDataInterfaceAbstract
//
// set of plugin methods which are needed by Salamander to get specific data of the plugin
// into the columns added by the plugin (works with CFileData::PluginData)

class CPluginInterfaceAbstract;

class CPluginDataInterfaceAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect calling of methods (see CPluginDataInterfaceEncapsulation)
    friend class CPluginDataInterfaceEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // returns TRUE if the method ReleasePluginData is about to be called for all files related
    // to this interface, otherwise return FALSE
    virtual BOOL WINAPI CallReleaseForFiles() = 0;

    // returns TRUE if the method ReleasePluginData is about to be called for all directories related
    // to this interface, otherwise return FALSE
    virtual BOOL WINAPI CallReleaseForDirs() = 0;

    // releeases data specific to the plugin (CFileData::PluginData) for 'file' (file or directory -
    // 'isDir' FALSE or TRUE; structure inserted into CSalamanderDirectoryAbstract when listing archives
    // or FS); called for all files if CallReleaseForFiles returns TRUE, and for all directories if
    // CallReleaseForDirs returns TRUE
    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir) = 0;

    // for archive data only (for FS, the up-dir symbol is not added):
    // modified the suggested content of the up-dir symbol (".." at the top of the panel); 'archivePath'
    // is the path in the archive for which the symbol is intended; the proposed data of the symbol
    // are entered in 'upDir': the name ".." (do not change), date&time of the archive, the rest is
    // zeroed; the changes of the plugin are entered in 'upDir', especially 'upDir.PluginData', which
    // will be used for the up-dir symbol when getting the content of the added columns;
    // for 'upDir', ReleasePluginData will not be called, any necessary release can be performed
    // either when calling GetFileDataForUpDir again or when releasing the entire interface (in its
    // destructor - called from CPluginInterfaceAbstract::ReleasePluginDataInterface)
    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) = 0;

    // for archive data only (FS uses only root path in CSalamanderDirectoryAbstract):
    // when adding a file or directory to CsalamanderDirectoryAbstract, it may happen that the specified
    // path does not exist and it is therefore necessary to create it, the individual directories of this
    // path are created automatically and this method allows the plugin to add its specific data (for its
    // columns) to these created directories; 'dirName' is the full path of the added directory in the
    // archive; the proposed data of the directory are entered in 'dir': the name of the directory
    // (allocated on the heap of Salamander), date&time taken from the added file/directory, the rest
    // is zeroed; the changes of the plugin are entered in 'dir', especially 'dir.PluginData'; returns
    // TRUE if adding the plugin data was successful, otherwise FALSE; if it returns TRUE, 'dir' will
    // be released in the standard way (Salamander part + ReleasePluginData) and either when releasing
    // the entire listing or still during its creation in case the same directory is added using
    // CSalamanderDirectoryAbstract::AddDir (overwriting the automatic creation by later normal addition);
    // if it returns FALSE, only the Salamander part of 'dir' will be released
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) = 0;

    // for FS with custom icons only (pitFromPlugin):
    // returns image-list with simple icons, during drawing items in the panel, the icon-index
    // into this image-list is obtained using a call-back; it is called only after getting a new
    // listing (after calling CPluginFSInterfaceAbstract::ListCurrentPath), so it is possible to
    // modify the image-list for each new listing;
    // 'iconSize' specifies the required size of the icons and is one of the values SALICONSIZE_xxx
    // the plugin will take care of destroying the image-list when calling GetSimplePluginIcons again
    // or when releasing the entire interface (in its destructor - called from
    // CPluginInterfaceAbstract::ReleasePluginDataInterface)
    // if the image-list cannot be created, it returns NULL and the current plugin-icons-type
    // degrades to pitSimple
    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) = 0;

    // for FS with custom icons only (pitFromPlugin):
    // returns TRUE, if a simple icon for the file/directory ('isDir' FALSE/TRUE) 'file' should be
    // used; returns FALSE, if the icon should be obtained by calling the method GetPluginIcon
    // from the thread for loading icons (loading icons "in the background");
    // at the same time, the icon-index for a simple icon (for icons loaded "in the background",
    // simple icons are used until the icon is loaded) can be pre-calculated in this method
    // and saved in CFileData (most likely in CFileData::PluginData);
    // limitation: only methods that can be called from any thread (methods independent of the
    // panel state) can be used from CSalamanderGeneralAbstract
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) = 0;

    // for FS with custom icons only (pitFromPlugin):
    // returns icon for the file/directory or NULL if the icon cannot be obtained; if
    // 'destroyIcon' is TRUE, the Win32 API function DestroyIcon is called to release the icon;
    // 'iconSize' specifies the required size of the icon and is one of the values SALICONSIZE_xxx
    // limitation: since it is called from the thread for loading icons (it is not the main thread),
    //  you can use only methods from CSalamanderGeneralAbstract that can be called from any thread
    virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon) = 0;

    // for FS with custom icons only (pitFromPlugin):
    // compares 'file1' (can be a file or directory) and 'file2' (can be a file or directory),
    // must not return that two listing items are the same (ensures unambiguous assignment of
    // a custom icon to a file/directory); if there is no risk of duplicate names in the listing
    // paths (usual case), it can be easily implemented as:
    // {return strcmp(file1->Name, file2->Name);}
    // returns a number less than zero if 'file1' < 'file2', zero if 'file1' == 'file2' and
    // a number greater than zero if 'file1' > 'file2';
    // limitation: since it is called from the thread for loading icons (it is not the main thread),
    //  you can use only methods from CSalamanderGeneralAbstract that can be called from any thread
    virtual int WINAPI CompareFilesFromFS(const CFileData* file1, const CFileData* file2) = 0;

    // used to set the view parameter, this method is called before displaying new content
    // of the panel (when changing the path) and when changing the current view (also when
    // manually changing the width of the column); 'leftPanel' is TRUE if it is the left panel
    // (FALSE if it is the right panel); 'view' is an interface for modifying the view
    // (setting the mode, working with columns); if it is archive data, 'archivePath' contains
    // the current path in the archive, for FS data it is NULL; if it is archive data, 'upperDir'
    // is a pointer to the parent directory (if the current path is the root of the archive,
    // 'upperDir' is NULL), for FS data it is always NULL;
    // CAUTION: during the call to this method, the panel must not be repainted (the size of
    //          the icons, etc. may change here), so no message loops (no dialogs, etc.)!
    // limitation: only methods that can be called from any thread (methods independent of the
    //             panel state) can be used from CSalamanderGeneralAbstract
    virtual void WINAPI SetupView(BOOL leftPanel, CSalamanderViewAbstract* view,
                                  const char* archivePath, const CFileData* upperDir) = 0;

    // setting new value "column->FixedWidth" - the user used the context menu on the column
    // added by the plugin in the header-line > "Automatic Column Width"; the plugin should
    // save the new value of column->FixedWidth stored in 'newFixedWidth' (it is always the
    // negation of column->FixedWidth), so that when calling SetupView() again, the column
    // can be added with the correct FixedWidth; at the same time, if the fixed width of the
    // column is turned on, the plugin should set the current value of "column->Width" (so
    // that the width of the column does not change when turning on this fixed width) - the
    // ideal is to call "ColumnWidthWasChanged(leftPanel, column, column->Width)"; 'column'
    // identifies the column to be changed; 'leftPanel' is TRUE if it is a column from the
    // left panel (FALSE if it is a column from the right panel)
    virtual void WINAPI ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column,
                                                     int newFixedWidth) = 0;

    // setting new value "column->Width" - the user changed the width of the column added
    // by the plugin in the header-line by the mouse; the plugin should save the new value
    // of column->Width stored in 'newWidth', so that when calling SetupView() again, the
    // column can be added with the correct Width; 'column' identifies the column that has
    // changed; 'leftPanel' is TRUE if it is a column from the left panel (FALSE if it is
    // a column from the right panel)
    virtual void WINAPI ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column,
                                              int newWidth) = 0;

    // gets the content of Information Line for the file/directory ('isDir' TRUE/FALSE) 'file'
    // or the selected files and directories ('file' is NULL and the number of selected files
    // and directories are in 'selectedFiles'/'selectedDirs') in the panel ('panel' is one of
    // PANEL_XXX); it is also called when the listing is empty (it only applies to FS, it cannot
    // occur for archives, 'file' is NULL, 'selectedFiles' and 'selectedDirs' are 0); if 'displaySize'
    // is TRUE, the size of all selected directories is known (see CFileData::SizeValid; if nothing
    // is selected, it is TRUE); the sum of the numbers CFileData::Size of the selected files and
    // directories is in 'selectedSize' (if nothing is selected, it is zero); 'buffer' is a buffer
    // for the returned text (size 1000 bytes); 'hotTexts' is an array (size 100 DWORDs), in which
    // information about the position of the hot-text is returned, the lower WORD always contains
    // the position of the hot-text in 'buffer', the upper WORD contains the length of the hot-text;
    // 'hotTextsCount' is the size of the 'hotTexts' array (100) and returns the number of written
    // hot-texts in the 'hotTexts' array; returns TRUE if 'buffer' + 'hotTexts' + 'hotTextsCount'
    // is set, returns FALSE if the Information Line is to be filled in the standard way (as on disk)
    virtual BOOL WINAPI GetInfoLineContent(int panel, const CFileData* file, BOOL isDir, int selectedFiles,
                                           int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
                                           char* buffer, DWORD* hotTexts, int& hotTextsCount) = 0;

    // for archives only: user stored files/directories from the archive to the clipboard, now
    // the archive in the panel is being closed: if the method returns TRUE, the archive will
    // remain open (optimization of possible Paste from the clipboard - the archive is already
    // listed), if the method returns FALSE, the object will be released (possible Paste from
    // the clipboard will cause listing of the archive, then the selected files/directories
    // will be unpacked); NOTE: if the file of the archive is open during the lifetime of the
    // object, the method should return FALSE, otherwise the file of the archive will remain
    // open for the entire "stay" of the data on the clipboard (cannot be deleted, etc.)
    virtual BOOL WINAPI CanBeCopiedToClipboard() = 0;

    // only when entering VALID_DATA_PL_SIZE into CSalamanderDirectoryAbstract::SetValidData():
    // returns TRUE if the size of the file/directory ('isDir' TRUE/FALSE) 'file' is known,
    // otherwise returns FALSE; the size is returned in 'size'
    virtual BOOL WINAPI GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size) = 0;

    // only when entering VALID_DATA_PL_DATE into CSalamanderDirectoryAbstract::SetValidData():
    // returns TRUE if the date of the file/directory ('isDir' TRUE/FALSE) 'file' is known,
    // otherwise returns FALSE; the date is returned in 'date' part of the 'date' structure
    // ("time" part of the structure should remain untouched)
    virtual BOOL WINAPI GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date) = 0;

    // only when entering VALID_DATA_PL_TIME into CSalamanderDirectoryAbstract::SetValidData():
    // returns TRUE if the time of the file/directory ('isDir' TRUE/FALSE) 'file' is known,
    // otherwise returns FALSE; the time is returned in 'time' part of the 'time' structure
    // ("date" part of the structure should remain untouched)
    virtual BOOL WINAPI GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time) = 0;
};

//
// ****************************************************************************
// CSalamanderForOperationsAbstract
//
// set of Salamander methods for supporting the execution of operations, the validity of the interface is
// limited to the method that is passed as a parameter; therefore, it can only be called from this thread
// and in this method (the object is on the stack, so it disappears after returning)

class CSalamanderForOperationsAbstract
{
public:
    // PROGRESS DIALOG: the dialog contains one/two ('twoProgressBars' FALSE/TRUE) progress meters
    // opens a progress dialog with the title 'title'; 'parent' is the parent window of the progress
    // dialog (if NULL, the main window is used); if it contains only one progress meter, it can be
    // described as "File" ('fileProgress' is TRUE) or "Total" ('fileProgress' is FALSE)
    //
    // the dialog does not run in its own thread; for its functioning (Cancel button + internal timer)
    // it is necessary to occasionally empty the message queue; this is ensured by the methods
    // ProgressDialogAddText, ProgressAddSize and ProgressSetSize
    //
    // because real-time display of text and changes in progress bars is very slow, the methods
    // ProgressDialogAddText, ProgressAddSize and ProgressSetSize have the parameter 'delayedPaint';
    // it should be TRUE for all quickly changing texts and values; the methods then save the texts
    // and display them only after the internal timer of the dialog is delivered;
    // 'delayedPaint' should be set to FALSE for initialization/ending texts such as "preparing data..."
    // or "canceling operation..."; after displaying them, we do not give the dialog a chance to
    // distribute messages (timers); if such an operation is likely to take a long time, we should
    // "refresh" the dialog during this time by calling ProgressAddSize(CQuadWord(0, 0), TRUE) and
    // depending on its return value, possibly terminate the action prematurely
    virtual void WINAPI OpenProgressDialog(const char* title, BOOL twoProgressBars,
                                           HWND parent, BOOL fileProgress) = 0;
    // prints the text 'txt' (even several lines - the text is broken down into lines) to the progress dialog
    virtual void WINAPI ProgressDialogAddText(const char* txt, BOOL delayedPaint) = 0;
    // if 'totalSize1' is not CQuadWord(-1, -1), sets 'totalSize1' as 100 percent of the first progress meter,
    // if 'totalSize2' is not CQuadWord(-1, -1), sets 'totalSize2' as 100 percent of the second progress meter
    // (for a progress dialog with one progress meter, 'totalSize2' CQuadWord(-1, -1) is mandatory)
    virtual void WINAPI ProgressSetTotalSize(const CQuadWord& totalSize1, const CQuadWord& totalSize2) = 0;
    // if 'size1' is not CQuadWord(-1, -1), sets the size 'size1' (size1/total1*100 percent) on the first progress meter,
    // if 'size2' is not CQuadWord(-1, -1), sets the size 'size2' (size2/total2*100 percent) on the second progress meter
    // (for a progress dialog with one progress meter, 'size2' CQuadWord(-1, -1) is mandatory), returns whether the action
    // should continue (FALSE = end)
    virtual BOOL WINAPI ProgressSetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint) = 0;
    // adds (possibly to both progress meters) the size 'size' (size/total*100 percent of the progress)
    // returns an information whether the action should continue (FALSE = end)
    virtual BOOL WINAPI ProgressAddSize(int size, BOOL delayedPaint) = 0;
    // enables/disables the Cancel button
    virtual void WINAPI ProgressEnableCancel(BOOL enable) = 0;
    // returns HWND of the progress dialog (useful when displaying errors and queries with an open progress dialog)
    virtual HWND WINAPI ProgressGetHWND() = 0;
    // closes the progress dialog
    virtual void WINAPI CloseProgressDialog() = 0;

    // moves all files from 'source' to 'target' directory,
    // moreover, it remaps the prefixes of the displayed names ('remapNameFrom' -> 'remapNameTo')
    // returns success of the operation
    virtual BOOL WINAPI MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                                  const char* remapNameTo) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_com)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
