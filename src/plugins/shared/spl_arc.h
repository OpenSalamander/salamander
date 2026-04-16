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
#pragma pack(push, enter_include_spl_arc) // to make the structures independent of the current alignment setting
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

class CSalamanderDirectoryAbstract;
class CSalamanderForOperationsAbstract;
class CPluginDataInterfaceAbstract;

//
// ****************************************************************************
// CPluginInterfaceForArchiverAbstract
//

class CPluginInterfaceForArchiverAbstract
{
#ifdef INSIDE_SALAMANDER
private: // guards against incorrect direct method calls (see CPluginInterfaceForArchiverEncapsulation)
    friend class CPluginInterfaceForArchiverEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // Function for 'panel archiver view'; called to load the contents of archive 'fileName'.
    // The contents are stored in 'dir'; Salamander obtains the contents of
    // plugin-added columns through the 'pluginData' interface (if the plugin does not add columns,
    // 'pluginData' is returned as NULL). Returns TRUE if the archive contents are loaded successfully;
    // if it returns FALSE, the returned 'pluginData' value is ignored (the data in 'dir' must be
    // released with 'dir.Clear(pluginData)', otherwise only the Salamander-managed part of the data is released).
    // 'salamander' is a set of useful methods exported by Salamander.
    // WARNING: the file 'fileName' may also not exist (if it is open in a panel and deleted elsewhere).
    // ListArchive is not called for zero-length files; they automatically have empty contents.
    // When packing into such files, the file is deleted before calling PackToArchive (for
    // compatibility with external packers)
    virtual BOOL WINAPI ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                    CSalamanderDirectoryAbstract* dir,
                                    CPluginDataInterfaceAbstract*& pluginData) = 0;

    // Function for 'panel archiver view'; called when files/directories are to be unpacked
    // from archive 'fileName' to directory 'targetDir' from archive path 'archiveRoot'; 'pluginData'
    // is an interface for working with file/directory information specific to the plugin
    // (for example data from added columns; it is the same interface returned by ListArchive
    // in the 'pluginData' parameter, so it may be NULL); the files/directories are specified by the enumeration
    // function 'next' with parameter 'nextParam'; returns TRUE if unpacking succeeds (Cancel was not
    // used; Skip may have been used) - the source of the operation is then unselected in the panel;
    // otherwise returns FALSE (the source is not unselected); 'salamander' is a set of useful methods exported by
    // Salamander
    virtual BOOL WINAPI UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                      const char* archiveRoot, SalEnumSelection next,
                                      void* nextParam) = 0;

    // Function for the panel archiver view; called when one file is requested for unpacking for view/edit
    // from archive 'fileName' to directory 'targetDir'; the file name inside the archive is 'nameInArchive';
    // 'pluginData' is an interface for working with file information specific to the plugin
    // (for example, data from added columns; it is the same interface returned by ListArchive
    // in the 'pluginData' parameter, so it may also be NULL); 'fileData' is a pointer to the CFileData
    // structure of the file being unpacked (the structure was built by the plugin when listing the archive);
    // 'newFileName' (if not NULL) is the new name for the unpacked file (used if the original name from the
    // archive cannot be unpacked to disk, for example, 'aux', 'prn', etc.); write TRUE to
    // 'renamingNotSupported' (only if 'newFileName' is not NULL) if the plugin does not support renaming
    // during unpacking (the standard 'renaming not supported' error message is shown by Salamander);
    // returns TRUE if the file is unpacked successfully (the file is at the requested path and neither
    // Cancel nor Skip was used); 'salamander' is a set of useful methods exported by Salamander
    virtual BOOL WINAPI UnpackOneFile(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                                      const CFileData* fileData, const char* targetDir,
                                      const char* newFileName, BOOL* renamingNotSupported) = 0;

    // Function for 'panel archiver edit' and 'custom archiver pack'; called when files/directories are to be packed
    // into archive 'fileName' at path 'archiveRoot'; the files/directories are specified by
    // source path 'sourcePath' and the enumeration function 'next' with parameter 'nextParam'.
    // If 'move' is TRUE, the packed files/directories should be removed from disk. Returns TRUE
    // if all files/directories are packed/removed successfully (Cancel was not used; Skip may have
    // been used) - the source of the operation is then unselected in the panel; otherwise returns FALSE (the source is not unselected);
    // 'salamander' is a set of useful methods exported by Salamander
    virtual BOOL WINAPI PackToArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      const char* archiveRoot, BOOL move, const char* sourcePath,
                                      SalEnumSelection2 next, void* nextParam) = 0;

    // Function for 'panel archiver edit'; called when files/directories are to be deleted from archive
    // 'fileName'; the files/directories are specified by path 'archiveRoot' and the enumeration function 'next'
    // with parameter 'nextParam'; 'pluginData' is an interface for working with file/directory information
    // specific to the plugin (for example data from added columns; it is the same interface returned by
    // ListArchive in the 'pluginData' parameter, so it may be NULL); returns TRUE if all
    // files/directories are deleted successfully (Cancel was not used; Skip may have been used) - the source
    // of the operation is then unselected in the panel; otherwise returns FALSE (the source is not unselected); 'salamander' is a set of
    // useful methods exported by Salamander
    virtual BOOL WINAPI DeleteFromArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                          CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                                          SalEnumSelection next, void* nextParam) = 0;

    // Function for 'custom archiver unpack'; called when files/directories are to be unpacked from archive
    // 'fileName' to directory 'targetDir'; the files/directories are specified by mask 'mask'. Returns TRUE if
    // all files/directories are unpacked successfully (Cancel was not used; Skip may have been used).
    // If 'delArchiveWhenDone' is TRUE, all archive volumes must be added to 'archiveVolumes'
    // (including null terminators; if the archive is not multi-volume, only 'fileName' is stored there). If this function returns
    // TRUE (successful unpacking), all files from 'archiveVolumes' are then deleted;
    // 'salamander' is a set of useful methods exported by Salamander
    virtual BOOL WINAPI UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                           const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                           CDynamicString* archiveVolumes) = 0;

    // Function for 'panel archiver view/edit'; called before the panel with the archive is closed
    // POZOR: pokud se nepodari otevrit novou cestu, archiv muze v panelu zustat (nezavisle na tom,
    //        whatever CanCloseArchive returns); this method therefore cannot be used to destroy the context;
    //        it is intended, for example, to optimize Delete from an archive, when leaving it may
    //        offer to 'shake' the archive
    //        to destroy the context, use CPluginInterfaceAbstract::ReleasePluginDataInterface,
    //        see archivatory.txt
    // 'fileName' is the archive name; 'salamander' is a set of useful methods exported by Salamander;
    // 'panel' identifies the panel in which the archive is open (PANEL_LEFT or PANEL_RIGHT);
    // returns TRUE if closing is possible; if 'force' is TRUE, it always returns TRUE; if
    // critical shutdown (vice viz CSalamanderGeneralAbstract::IsCriticalShutdown), nema
    // there is no point in prompting the user about anything
    virtual BOOL WINAPI CanCloseArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                        BOOL force, int panel) = 0;

    // Gets the required disk-cache settings (the disk cache is used for temporary copies
    // of files opened from an archive in viewers, editors, and through system
    // associations); normally, if a copy of 'tempPath' can be allocated after the call, this method is called
    // only once, before the first use of the disk cache (for example before the first opening of
    // a file from the archive in a viewer/editor); if it returns FALSE, the standard
    // settings are used (files in the TEMP directory, and copies are deleted with the Win32
    // API function DeleteFile() only after the cache size limit is exceeded or when the archive is closed)
    // and all other return values are ignored; if it returns TRUE, the following
    // return values are used: if 'tempPath' (a buffer of size MAX_PATH) is not an
    // empty string, all temporary copies extracted by the plugin from the archive are stored in subdirectories of this path
    // (these subdirectories are deleted by the disk cache when Salamander exits, but the plugin may
    // delete them earlier, for example on unload; when the first instance of the
    // plugin is loaded, not just within one running Salamander, it is also advisable to clean up the 'SAL*.tmp' subdirectories in this
    // path - this handles problems caused by locked files and crashes) + if 'ownDelete' is TRUE,
    // the DeleteTmpCopy and PrematureDeleteTmpCopy methods are called to delete the copies + if
    // 'cacheCopies' is FALSE, the copies are deleted as soon as they are released (for example when the
    // viewer is closed); if 'cacheCopies' is TRUE, the copies are deleted only after the cache size limit is exceeded
    // or when the archive is closed
    virtual BOOL WINAPI GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies) = 0;

    // Used only if GetCacheInfo returns TRUE in parameter 'ownDelete':
    // deletes a temporary copy extracted from this archive (watch out for read-only files,
    // their attributes must be changed before they can be deleted); if possible,
    // it should not display any windows (the user did not invoke this directly and it may disturb them
    // during other work); for longer actions it is useful to use a wait window (see
    // CSalamanderGeneralAbstract::CreateSafeWaitWindow); 'fileName' je jmeno souboru
    // for the copy; if several files are deleted at once (this can happen, for example, after an
    // edited archive is closed), 'firstFile' is TRUE for the first file and FALSE for the remaining
    // soubory (pouziva se ke korektnimu zobrazeni wait-okenka - viz DEMOPLUG)
    //
    // WARNING: it is called in the main thread when a message from the disk cache is delivered to the main window -
    // a message is sent requesting that a temporary copy be released (typically when a viewer or an
    // archive being edited in a panel is closed), so reentry into the plugin may occur
    // (if the message loop inside the plugin dispatches the message); another entry into DeleteTmpCopy
    // is excluded because the disk cache sends no further messages until DeleteTmpCopy
    // returns
    virtual void WINAPI DeleteTmpCopy(const char* fileName, BOOL firstFile) = 0;

    // Used only if GetCacheInfo returns TRUE in parameter 'ownDelete':
    // on plugin unload, determines whether DeleteTmpCopy should be called for copies that are
    // still in use (for example open in a viewer) - it is called only if such copies
    // exist; 'parent' is the parent window of any message box shown to the user (for example
    // to recommend that the user close all files from the archive so the plugin can delete them);
    // 'copiesCount' is the number of copies of archive files still in use; returns TRUE if
    // DeleteTmpCopy should be called; if it returns FALSE, the copies remain on disk. If a
    // critical shutdown is in progress (see CSalamanderGeneralAbstract::IsCriticalShutdown), there is no
    // point in prompting the user or performing lengthy actions (for example shredding files).
    // NOTE: while PrematureDeleteTmpCopy is running, DeleteTmpCopy is guaranteed
    // not to be called
    virtual BOOL WINAPI PrematureDeleteTmpCopy(HWND parent, int copiesCount) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_arc)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
