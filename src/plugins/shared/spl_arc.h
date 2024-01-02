// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED
//
//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_arc) // so that the structures are independent of the alignment
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
private: // protection against incorrect method call (see CPluginInterfaceForArchiverEncapsulation)
    friend class CPluginInterfaceForArchiverEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // function for "panel archiver view"; called to load content of archive 'fileName';
    // content is filled into 'dir' object; Salamander gets content of plugin added columns
    // using interface 'pluginData' (if plugin does not add any columns, 'pluginData'==NULL
    // is returned); returns TRUE if archive content was loaded successfully, if FALSE is
    // returned, return value of 'pluginData' is ignored (data in 'dir' must be freed using
    // 'dir.Clear(pluginData)', otherwise only Salamander part of data is freed);
    // 'salamander' is set of useful methods exported from Salamander,
    // CAUTION: file 'fileName' does not have to exist (if it is opened in panel and deleted
    // from outside), ListArchive is not called for zero-length files, they have
    // automatically empty content, when packing to such files, file is deleted before
    // calling PackToArchive (for compatibility with external packers)
    virtual BOOL WINAPI ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                    CSalamanderDirectoryAbstract* dir,
                                    CPluginDataInterfaceAbstract*& pluginData) = 0;

    // function for "panel archiver view"; called during a request to unpack file/directory
    // from archive 'fileName' to directory 'targetDir' from path in archive 'archiveRoot';
    // 'pluginData' is interface for working with information about files/directories which
    // are specific to plugin (e.g. data from added columns; it is the same interface which
    // is returned by ListArchive in parameter 'pluginData' - so it can be NULL); files/directories
    // are specified by enumeration function 'next' with parameter 'nextParam'; returns TRUE
    // if unpacking was successful (Cancel was not used, Skip could be used) - source of
    // operation in panel is unselected, otherwise returns FALSE (unselection is not performed);
    // 'salamander' is set of useful methods exported from Salamander
    virtual BOOL WINAPI UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                      const char* archiveRoot, SalEnumSelection next,
                                      void* nextParam) = 0;

    // function for "panel archiver view"; called during a request for unpacking one file
    // for view/edit from archive 'fileName' to directory 'targetDir'; filename in archive
    // is 'nameInArchive'; 'pluginData' is interface for working with information about files
    // which are specific to plugin (e.g. data from added columns; it is the same interface
    // which is returned by ListArchive in parameter 'pluginData' - so it can be NULL);
    // 'fileData' is pointer to structure CFileData of unpacked file (structure was built
    // by plugin during listing of archive); 'newFileName' (if not NULL) is new name for
    // unpacked file (it is used if original name from archive cannot be unpacked to disk
    // (e.g. "aux", "prn", etc.)); write TRUE to 'renamingNotSupported' (only if 'newFileName'
    // is not NULL) if plugin does not support renaming during unpacking (standard error
    // message "renaming not supported" is displayed by Salamander); returns TRUE if file
    // was unpacked successfully (file is on specified path, either Cancel or Skip was not
    // used), 'salamander' is set of useful methods exported from Salamander
    virtual BOOL WINAPI UnpackOneFile(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                                      const CFileData* fileData, const char* targetDir,
                                      const char* newFileName, BOOL* renamingNotSupported) = 0;

    // function for "panel archiver view" and "custom archiver pack"; called during a request
    // for packing file/directory archive 'fileName' to path 'archiveRoot'; files/directories
    // are specified by the source path 'sourcePath' and enumeration function 'next' with
    // parameter 'nextParam'; if 'move' is TRUE, packed files/directories should be deleted
    // from disk; returns TRUE if all files/directories were packed/deleted successfully
    // (Cancel was not used, Skip could be used) - source of operation in panel is unselected,
    // otherwise returns FALSE (unselection is not performed); 'salamander' is set of useful
    // methods exported from Salamander
    virtual BOOL WINAPI PackToArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      const char* archiveRoot, BOOL move, const char* sourcePath,
                                      SalEnumSelection2 next, void* nextParam) = 0;

    // function for "panel archiver view", called during a request for deleting file/directory
    // from archive 'fileName'; files/directories are specified by path 'archiveRoot' and
    // enumeration function 'next' with parameter 'nextParam'; 'pluginData' is interface for
    // working with information about files/directories which are specific to plugin (e.g.
    // data from added columns; it is the same interface which is returned by ListArchive
    // in parameter 'pluginData' - so it can be NULL); returns TRUE if all files/directories
    // were deleted successfully (Cancel was not used, Skip could be used) - source of operation
    // in panel is unselected, otherwise returns FALSE (unselection is not performed);
    // 'salamander' is set of useful methods exported from Salamander
    virtual BOOL WINAPI DeleteFromArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                          CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                                          SalEnumSelection next, void* nextParam) = 0;

    // function for "custom archiver unpack"; called during a request for unpacking file/directory
    // from archive 'fileName' to directory 'targetDir'; files/directories are specified by
    // the mask 'mask'; returns TRUE if all files/directories were unpacked successfully
    // (Cancel was not used, Skip could be used); if 'delArchiveWhenDone' is TRUE, all archive
    // volumes (including null-terminator) must be added to 'archiveVolumes' (if archive is
    // not multi-volume, only 'fileName' is added), if TRUE is returned (unpacking was successful),
    // all files from 'archiveVolumes' are then deleted; 'salamander' is set of useful methods
    // exported from Salamander
    virtual BOOL WINAPI UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                           const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                           CDynamicString* archiveVolumes) = 0;

    // function for "panel archiver view/edit", called before closing panel with archive;
    // CAUTION: if opening new path fails, archive can remain in panel (independent on
    //          return value of CanCloseArchive); therefore this method cannot be used
    //          for destruction of context; it is intended e.g. for optimization of
    //          Delete operation from archive, when it can offer "shredding" of archive
    //          for destruction of context, method CPluginInterfaceAbstract::ReleasePluginDataInterface,
    //          can be used, see document archivators.txt
    // 'fileName' is name of archive; 'salamander' is set of useful methods exported from Salamander;
    // 'panel' specifies panel in which archive is opened (PANEL_LEFT or PANEL_RIGHT);
    // returns TRUE if closing is possible, if 'force' is TRUE, returns TRUE always;
    // if critical shutdown is in progress (see CSalamanderGeneralAbstract::IsCriticalShutdown),
    // it does not make sense to ask user anything
    virtual BOOL WINAPI CanCloseArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                        BOOL force, int panel) = 0;

    // finds out required disk-cache settings (disk-cache is used for temporary copies
    // of files opened from archive in viewers, editors and via system associations);
    // normally (if it is possible to allocate 'tempPath' buffer) it is called only once
    // before first use of disk-cache (e.g. before first opening of file from archive
    // in viewer/editor); if it returns FALSE, standard settings are used (files in TEMP
    // directory, copies are deleted using Win32 API function DeleteFile() when cache
    // limit is exceeded or when archive is closed) and all other return values are ignored;
    // if it returns TRUE, following return values are used: if 'tempPath' (buffer of size
    // MAX_PATH) is not empty string, all temporary copies unpacked from this archive
    // will be stored in subdirectories of this path (disk-cache deletes these subdirectories
    // when Salamander is closed, but plugin can delete them earlier, e.g. during its unload;
    // it is also good to delete subdirectories "SAL*.tmp" in this path during load of first
    // instance of plugin (not only in one Salamander instance) - it solves problems caused
    // by locked files and software crashes) + if 'ownDelete' is TRUE, method DeleteTmpCopy
    // and PrematureDeleteTmpCopy will be called for deleting copies + if 'cacheCopies'
    // is FALSE, copies will be deleted as soon as they are released (e.g. when viewer is
    // closed), if 'cacheCopies' is TRUE, copies will be deleted when cache limit is exceeded
    // or when archive is closed
    virtual BOOL WINAPI GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies) = 0;

    // used only if method GetCacheInfo returns TRUE in parameter 'ownDelete':
    // deletes temporary copy unpacked from this archive (be careful with read-only files,
    // you have to change attributes first and then delete them), if possible, no windows
    // should be displayed (user did not initiate this action, he can be disturbed by it
    // in other activity), it is good to use wait-window (see CSalamanderGeneralAbstract::CreateSafeWaitWindow)
    // for longer actions; 'fileName' is name of file with copy; if more files are deleted
    // at once (e.g. after closing edited archive), 'firstFile' is TRUE for first file
    // and FALSE for other files (it is used for correct display of wait-window - see DEMOPLUG)
    //
    // CAUTION: it is called in main thread on the basis of message from disk-cache main window
    // - message about need to delete temporary copy (typically when viewer is closed
    // or when archive is "edited" in panel), therefore it can lead to repeated entry to plugin
    // (if message is distributed by message-loop inside plugin), another entry to DeleteTmpCopy
    // is excluded until end of call of DeleteTmpCopy, disk-cache does not send any other
    // messages until then
    virtual void WINAPI DeleteTmpCopy(const char* fileName, BOOL firstFile) = 0;

    // only used if method GetCacheInfo returns TRUE in parameter 'ownDelete':
    // during plugin unload, finds out if DeleteTmpCopy should be called for copies
    // which are still used (e.g. opened in viewer) - it is called only if there are such
    // copies; 'parent' is parent of possible messagebox with question to user
    // (or recommendation to close all files from archive so that plugin can delete them);
    // 'copiesCount' is number of used copies of files from archive; returns TRUE if
    // DeleteTmpCopy should be called, if FALSE is returned, copies will remain on disk;
    // if critical shutdown is in progress (see CSalamanderGeneralAbstract::IsCriticalShutdown),
    // it does not make sense to ask user anything and to perform time-consuming actions
    // (e.g. shredding of files)
    // NOTE: during execution of PrematureDeleteTmpCopy, it is ensured that DeleteTmpCopy
    //       will not be called
    virtual BOOL WINAPI PrematureDeleteTmpCopy(HWND parent, int copiesCount) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_arc)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
