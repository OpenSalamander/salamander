// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#pragma once

/// Network neighborhood plugin data interface.
class CNethoodPluginDataInterface : public CPluginDataInterfaceAbstract
{
protected:
    enum
    {
        ColumnDataComment = 1,
    };

    TCHAR m_szRedirectPath[MAX_PATH];

    void SetupColumns(__in CSalamanderViewAbstract* pView);
    void AddDescriptionColumn(__in CSalamanderViewAbstract* pView);

    static void WINAPI GetCommentColumnText();

    static int WINAPI GetSimpleIconIndex();

    static int NodeTypeToIconIndex(__in CNethoodCacheNode::Type nodeType);

    static void RedirectUncPathToSalamander(
        __in int iPanel,
        __in PCTSTR pszUncPath);

public:
    /// Constructor.
    CNethoodPluginDataInterface();

    /// Destructor.
    ~CNethoodPluginDataInterface();

    void SetRedirectPath(__in PCTSTR pszUncPath)
    {
        StringCchCopy(m_szRedirectPath, COUNTOF(m_szRedirectPath), pszUncPath);
    }

    //----------------------------------------------------------------------
    // CPluginDataInterfaceAbstract

    /// \return The return value should be TRUE if ReleasePluginData method
    ///         should be called for every single file bound to this interface.
    ///         Otherwise the return value should be FALSE.
    virtual BOOL WINAPI CallReleaseForFiles();

    /// \return The return value should be TRUE if ReleasePluginData method
    ///         should be called for every single directory bound to this
    ///         interface. Otherwise the return value should be FALSE.
    virtual BOOL WINAPI CallReleaseForDirs();

    // releases plugin-specific data (CFileData::PluginData) for 'file' (file or
    // directory - 'isDir' FALSE or TRUE; structure inserted into CSalamanderDirectoryAbstract
    // when listing archives or the FS); called for all files if CallReleaseForFiles
    // returns TRUE, and for all directories if CallReleaseForDirs returns TRUE
    virtual void WINAPI ReleasePluginData(
        __in CFileData& file,
        __in BOOL isDir);

    // archives only (the up-dir symbol is not filled for FS data):
    // adjusts the proposed content of the up-dir symbol (".." at the top of the panel); 'archivePath'
    // is the path in the archive for which the symbol is intended; 'upDir' receives the proposed
    // symbol data: name ".." (do not change), archive date&time, the rest zeroed;
    // 'upDir' outputs plugin modifications, primarily it should change 'upDir.PluginData',
    // which will be used on the up-dir symbol when obtaining the contents of additional columns;
    // ReleasePluginData will not be called for 'upDir'; any necessary cleanup can always be
    // performed during the next call to GetFileDataForUpDir or when releasing
    // the entire interface (in its destructor - called from
    // CPluginInterfaceAbstract::ReleasePluginDataInterface)
    virtual void WINAPI GetFileDataForUpDir(
        __in const char* archivePath,
        __inout CFileData& upDir);

    // archives only (the FS uses only the root path in CSalamanderDirectoryAbstract):
    // when adding a file/directory to CSalamanderDirectoryAbstract it may happen that
    // the requested path does not exist and therefore needs to be created; the individual directories
    // of this path are created automatically and this method allows the plugin to add its specific
    // data (for its own columns) to these directories being created; 'dirName' is the full path
    // of the directory being added in the archive; 'dir' receives the proposed data: directory name
    // (allocated on Salamander's heap), date&time taken from the added file/directory,
    // the rest zeroed; 'dir' outputs plugin changes, primarily it should change
    // 'dir.PluginData'; returns TRUE if adding the plugin data succeeded, otherwise FALSE;
    // if it returns TRUE, 'dir' will be released the standard way (Salamander part +
    // ReleasePluginData) either during the complete release of the listing or even during
    // its creation if the same directory is added again using
    // CSalamanderDirectoryAbstract::AddDir (overwriting the automatic creation with a later
    // normal addition); if it returns FALSE, only the Salamander part will be released from 'dir'
    virtual BOOL WINAPI GetFileDataForNewDir(
        __in const char* dirName,
        __inout CFileData& dir);

    // FS with custom icons only (pitFromPlugin):
    // returns an image list with simple icons; during panel item rendering
    // the icon index is obtained for this image list via callbacks; called every time a new listing
    // is obtained (after calling CPluginFSInterfaceAbstract::ListCurrentPath),
    // so the image list can be rebuilt for each new listing;
    // 'iconSize' determines the required icon size and is one of the SALICONSIZE_xxx values
    // the plugin must ensure destruction of the image list on the next GetSimplePluginIcons call
    // or when releasing the entire interface (in its destructor - called from
    // CPluginInterfaceAbstract::ReleasePluginDataInterface)
    // if the image list cannot be created, returns NULL and downgrades the current plugin-icons-type
    // to pitSimple
    virtual HIMAGELIST WINAPI GetSimplePluginIcons(
        __in int iconSize);

    // FS with custom icons only (pitFromPlugin):
    // returns TRUE if the simple icon should be used for the given file/directory ('isDir' FALSE/TRUE) 'file';
    // returns FALSE if the icon should be obtained by calling GetPluginIcon from the icon loading thread (background loading);
    // this method can also precompute the icon index for the simple icon
    // (for icons loaded in the background the simple icons are used until the icon is loaded)
    // and store it in CFileData (most likely in CFileData::PluginData);
    // limitation: from CSalamanderGeneralAbstract only methods that can be called from any thread
    // (methods independent of the panel state) may be used
    virtual BOOL WINAPI HasSimplePluginIcon(
        CFileData& file,
        BOOL isDir);

    // FS with custom icons only (pitFromPlugin):
    // returns an icon for the file or directory 'file' or NULL if the icon cannot be obtained; if
    // 'destroyIcon' is TRUE, the Win32 API function DestroyIcon is called to release the returned icon;
    // 'iconSize' determines the desired icon size and is one of the SALICONSIZE_xxx values
    // limitation: because it is called from the icon loading thread (not the main thread), only methods
    // from CSalamanderGeneralAbstract that can be called from any thread may be used
    virtual HICON WINAPI GetPluginIcon(
        __in const CFileData* file,
        __in int iconSize,
        __out BOOL& destroyIcon);

    // FS with custom icons only (pitFromPlugin):
    // compares 'file1' (can be a file or directory) and 'file2' (can be a file or directory),
    // it must not return that any two listing items are identical (ensures a unique
    // assignment of a custom icon to a file/directory); if duplicate names in the listing path
    // are not a concern (the usual case), it can simply be implemented as:
    // {return strcmp(file1->Name, file2->Name);}
    // returns a number less than zero if 'file1' < 'file2', zero if 'file1' == 'file2' and
    // a number greater than zero if 'file1' > 'file2';
    // limitation: because it is also called from the icon loading thread (not only from the main thread),
    // only methods from CSalamanderGeneralAbstract that can be called from any thread may be used
    virtual int WINAPI CompareFilesFromFS(
        __in const CFileData* file1,
        __in const CFileData* file2);

    // configures the view parameters; this method is called before displaying new
    // panel content (when the path changes) and when the current view changes (even a manual column width change);
    // 'leftPanel' is TRUE for the left panel (FALSE for the right panel);
    // 'view' is the interface for modifying the view (mode settings, working with columns);
    // for archive data, 'archivePath' contains the current path in the archive,
    // for FS data 'archivePath' is NULL; for archive data, 'upperDir' is a pointer to
    // the parent directory (if the current path is the archive root, 'upperDir' is NULL); for FS data
    // it is always NULL;
    // WARNING: while this method is being called the panel must not be redrawn (icon size, etc. may change),
    //          so no message loops (no dialogs, etc.)!
    // limitation: from CSalamanderGeneralAbstract only methods that can be called from any thread
    //             (methods independent of the panel state) may be used
    virtual void WINAPI SetupView(
        __in BOOL leftPanel,
        __in CSalamanderViewAbstract* view,
        __in const char* archivePath,
        __in const CFileData* upperDir);

    // sets a new value for "column->FixedWidth" - the user used the context menu
    // on the plugin-added column in the header line > "Automatic Column Width"; the plugin
    // should store the new column->FixedWidth value provided in 'newFixedWidth'
    // (it is always the negation of column->FixedWidth) so that during subsequent SetupView() calls it can
    // add the column with the correctly configured FixedWidth; additionally, if the fixed width
    // of the column is being enabled, the plugin should store the current value of "column->Width" (so that
    // enabling the fixed width does not change the column width) - ideally call
    // "ColumnWidthWasChanged(leftPanel, column, column->Width)"; 'column' identifies
    // the column to be changed; 'leftPanel' is TRUE for a column from the left panel
    // (FALSE for a column from the right panel)
    virtual void WINAPI ColumnFixedWidthShouldChange(
        __in BOOL leftPanel,
        __in const CColumn* column,
        __in int newFixedWidth);

    // sets a new value for "column->Width" - the user changed the width of the plugin-added
    // column in the header line with the mouse; the plugin should store the new column->Width value (also stored
    // in 'newWidth') so that during subsequent SetupView() calls it can add the column with
    // the correct Width; 'column' identifies the column that changed; 'leftPanel'
    // is TRUE for a column from the left panel (FALSE for a column from the right panel)
    virtual void WINAPI ColumnWidthWasChanged(
        __in BOOL leftPanel,
        __in const CColumn* column,
        __in int newWidth);

    // obtains the Information Line contents for the file/directory ('isDir' TRUE/FALSE) 'file'
    // or for the selected files and directories ('file' is NULL and the counts of selected files/directories
    // are in 'selectedFiles'/'selectedDirs') in the panel ('panel' is one of PANEL_XXX);
    // if 'displaySize' is TRUE, the size of all selected directories is known (see
    // CFileData::SizeValid; if nothing is selected, this is TRUE); 'selectedSize' holds
    // the sum of CFileData::Size values of selected files and directories (if nothing is selected,
    // it is zero); 'buffer' is the buffer for the returned text (size 1000 bytes); 'hotTexts'
    // is an array (size 100 DWORDs) that returns information about hot-text positions; the lower WORD
    // always contains the position of the hot-text in 'buffer', the upper WORD contains the hot-text length;
    // 'hotTextsCount' returns the number of hot-text entries written to 'hotTexts'; returns TRUE
    // if 'buffer' + 'hotTexts' + 'hotTextsCount' are set, returns FALSE if the Information Line should be
    // filled in the standard way (as on disks)
    virtual BOOL WINAPI GetInfoLineContent(
        __in int panel,
        __in const CFileData* file,
        __in BOOL isDir,
        __in int selectedFiles,
        __in int selectedDirs,
        __in BOOL displaySize,
        __in const CQuadWord& selectedSize,
        __out_bcount(1000) char* buffer,
        __out_ecount(100) DWORD* hotTexts,
        __out int& hotTextsCount);

    // archives only: the user saved files/directories from the archive to the clipboard and is now closing
    // the archive in the panel; if the method returns TRUE, this object remains open (optimization
    // for a possible Paste from the clipboard - the archive is already listed); if the method returns FALSE,
    // this object is released (a potential Paste from the clipboard will list the archive and only then
    // extract the selected files/directories); NOTE: if a file from the archive remains open for the lifetime
    // of the object, the method should return FALSE, otherwise the archive file will remain open
    // for the entire "stay" of the data on the clipboard (it will not be possible to delete it, etc.)
    virtual BOOL WINAPI CanBeCopiedToClipboard();

    // only when VALID_DATA_PL_SIZE is provided to CSalamanderDirectoryAbstract::SetValidData():
    // returns TRUE if the size of the file/directory ('isDir' TRUE/FALSE) 'file' is known,
    // otherwise returns FALSE; the size is returned in 'size'
    virtual BOOL WINAPI GetByteSize(
        __in const CFileData* file,
        __in BOOL isDir,
        __out CQuadWord* size);

    // only when VALID_DATA_PL_DATE is provided to CSalamanderDirectoryAbstract::SetValidData():
    // returns TRUE if the date of the file/directory ('isDir' TRUE/FALSE) 'file' is known,
    // otherwise returns FALSE; the date is returned in the "date" part of the 'date' structure (the "time" part
    // should remain untouched)
    virtual BOOL WINAPI GetLastWriteDate(
        __in const CFileData* file,
        __in BOOL isDir,
        __out SYSTEMTIME* date);

    // only when VALID_DATA_PL_TIME is provided to CSalamanderDirectoryAbstract::SetValidData():
    // returns TRUE if the time of the file/directory ('isDir' TRUE/FALSE) 'file' is known,
    // otherwise returns FALSE; the time is returned in the "time" part of the 'time' structure (the "date" part
    // should remain untouched)
    virtual BOOL WINAPI GetLastWriteTime(
        __in const CFileData* file,
        __in BOOL isDir,
        __out SYSTEMTIME* time);
};
