// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#pragma once

class CNethoodFSInterface;

class CNethoodFSCacheConsumer : public CNethoodCacheEventConsumer
{
private:
    CNethoodFSInterface* m_pChief;

public:
    CNethoodFSCacheConsumer(__in CNethoodFSInterface* pChief)
    {
        m_pChief = pChief;
    }

    virtual void OnCacheNodeUpdated(__in CNethoodCache::Node node);
};

/// Implements CPluginFSInterfaceAbstract interface for the Nethood plugin.
class CNethoodFSInterface : public CPluginFSInterfaceAbstract
{
private:
    /// Represents state of the internal state machine.
    enum FSState
    {
        /// Normal operation.
        FSStateNormal,

        /// Do immediate refresh.
        FSStateImmediateRefresh,

        /// Redirect an UNC path to the Salamander.
        FSStateRedirect,

        /// Find an accessible path.
        FSStateFindAccessible,

        /// Display error and go to accessible path.
        FSStateDisplayError,

        /// Request to redirect from an enumeration thread.
        FSStateAsyncRedirect,

        /// The path was a symbolink link and now is being
        /// redirected to the target path.
        FSStateSymLink,
    };

    /// Current path.
    TCHAR m_szCurrentPath[MAX_PATH];

    /// Cache node for this path.
    CNethoodCache::Node m_pathNode;

    /// Consumer of events on the current path.
    CNethoodFSCacheConsumer* m_pPathNodeEventConsumer;

    /// State of the internal state machine.
    FSState m_state;

    /// Contains the UNC path to redirect to.
    TCHAR m_szRedirectPath[MAX_PATH];

    /// Ignore the force refresh flag in the next ListCurrentPath?
    bool m_bIgnoreForceRefresh;

    /// Contains accessible path found asynchronously from the enumeration
    /// thread. This member variable should be accessed while holding the
    /// cache lock.
    TCHAR m_szAccessiblePath[MAX_PATH];

    /// This contains the result of asynchronous enumeration.
    DWORD m_dwEnumerationResult;

    /// Flag whether the path was changed manually.
    bool m_bManualEntry;

    /// Show throbber (animation in Directory line showing that enumeration is still in progress)?
    bool m_bShowThrobber;

    /// Contains throbber identification number.
    int m_iThrobberID;

    /// True if the workgroup/domain servers should not be displayed
    /// in root.
    static bool s_bHideServersInRoot;

    /// Convert's nethood cache node to Salamander's file structure.
    /// \param nodeData Nethood cache node data.
    /// \param file When the method returns this parameter will receive
    ///        converted data.
    /// \return If the conversion succeeds the return value is true.
    ///         Otherwise the return value is false.
    bool NethoodNodeToFileData(
        __in CNethoodCache::Node node,
        __out CFileData& file);

    void DisplayError(DWORD dwError);

    bool PostRedirectPathToSalamander(__in PCTSTR pszPath);

    void StartThrobber();

    void PostStartThrobber(__in CNethoodCache::Node node);

    void StopThrobber();

    bool IsRootForContextMenu();

    bool AreItemsSuitableForContextMenu(__in bool bSelected);

public:
    /// Constructor.
    CNethoodFSInterface();

    /// Destructor.
    ~CNethoodFSInterface();

    //----------------------------------------------------------------------
    // CPluginFSInterfaceAbstract

    /// Retrieves user portion of the current plugin's path.
    /// \param userPart Caller supplied buffer. The method should copy user
    ///        portion of the current path to it. The buffer is MAX_PATH
    ///        characters in length.
    /// \return If the method succeeds it should return nonzero value,
    ///         otherwise it should return zero.
    virtual BOOL WINAPI GetCurrentPath(
        __out_ecount(MAX_PATH) char* userPart);

    /// Retrieves user portion of the file, directory or up-dir on the
    /// current path.
    /// \param file File data.
    /// \param isDir This parameter can be one of the following values:
    ///        0 The file data represents a file.
    ///        1 The file data represents a directory.
    ///        2 The file data represents the up-dir.
    /// \param buf Caller supplied buffer. The method should copy the
    ///        path to it.
    /// \param bufSize Size of the buffer, in characters.
    /// \return If the method succeeds it should return nonzero value,
    ///         otherwise it should return zero.
    /// Implement this method to get the Alt+Insert behavior. Is is also
    /// used by the drag'n'drop manipulation and Ctrl+Shift+arrow handling.
    virtual BOOL WINAPI GetFullName(
        __in CFileData& file,
        __in int isDir,
        __out_ecount(bufSize) char* buf,
        __in int bufSize);

    virtual BOOL WINAPI GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize,
                                      BOOL& success);
    virtual BOOL WINAPI GetRootPath(char* userPart);
    virtual BOOL WINAPI IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart);
    virtual BOOL WINAPI IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart);
    virtual BOOL WINAPI ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                                   const char* userPart, char* cutFileName, BOOL* pathWasCut,
                                   BOOL forceRefresh, int mode);
    virtual BOOL WINAPI ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                        CPluginDataInterfaceAbstract*& pluginData,
                                        int& iconsType, BOOL forceRefresh);
    virtual BOOL WINAPI TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason);
    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ReleaseObject(HWND parent);
    virtual DWORD WINAPI GetSupportedServices();
    virtual BOOL WINAPI GetChangeDriveOrDisconnectItem(const char* fsName, char*& title,
                                                       HICON& icon, BOOL& destroyIcon);
    virtual HICON WINAPI GetFSIcon(BOOL& destroyIcon);
    virtual void WINAPI GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                      DWORD allowedEffects, DWORD keyState,
                                      DWORD* dropEffect);
    virtual void WINAPI GetFSFreeSpace(CQuadWord* retValue);

    /// Retrieves next component of the path for the directory line
    /// hot-tracking feature.
    /// \param text String that the method should search for the points
    ///        of division. The string contains path from the directory
    ///        line and can also contain a filter specifier.
    /// \param pathLen Length of the path, in characters. The remainder
    ///        of the text is a filter specifier.
    /// \param offset On entry it contains character offset where the
    ///        method should start the search. On exit it should contain
    ///        offset of the next point of division if one is found.
    /// \return If the method finds next point of division it must return
    ///         a nonzero value and set the offset of the point in the
    ///         offset parameter. If the next point is not found, the return
    ///         value must be zero. The method should not treat the end of
    ///         the string as a point of division.
    /// This method is only called if the plugin reports its support by
    /// the flag FS_SERVICE_GETNEXTDIRLINEHOTPATH returned from the
    /// GetSupportedServices method.
    virtual BOOL WINAPI GetNextDirectoryLineHotPath(
        __in const char* text,
        __in int pathLen,
        __inout int& offset);

    virtual void WINAPI CompleteDirectoryLineHotPath(char* path, int pathBufSize);

    /// Retrieves title for the Salamander's main window.
    /// \param fsName Name of the current file system.
    /// \param mode This parameter specifies the format of the title.
    ///        This can be one of the following values:
    ///        1 Directory name only. The method should return the name
    ///          of the current directory (the last component of the
    ///          current path.
    ///        2 Shortened path. The method should return current path
    ///          shortened format 'root' (including the path separator) +
    ///          ellipsis ('...') + path separator + last component of the
    ///          current path.
    /// \param buf Caller supplied buffer. The method should copy the
    ///        title to it.
    /// \param bufSize Size of the buffer, in characters.
    /// \return If the method provides the title, it shoulld return
    ///         a nonzero value. If the return value is zero, Salamander
    ///         will use the GetNextDirectoryLineHotPath method
    ///         to create the title automatically.
    /// This method is only called if the plugin reports its support by
    /// the flag FS_SERVICE_GETPATHFORMAINWNDTITLE returned from the
    /// GetSupportedServices method.
    /// Also the user has to enable the option 'Display current path in
    /// main window title' in the Salamander's configuration.
    virtual BOOL WINAPI GetPathForMainWindowTitle(
        __in const char* fsName,
        __in int mode,
        __out_ecount(bufSize) char* buf,
        __in int bufSize);

    virtual void WINAPI ShowInfoDialog(const char* fsName, HWND parent);
    virtual BOOL WINAPI ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo);
    virtual BOOL WINAPI QuickRename(const char* fsName, int mode, HWND parent, CFileData& file,
                                    BOOL isDir, char* newName, BOOL& cancel);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* fsName, const char* path,
                                                       BOOL includingSubdirs);
    virtual BOOL WINAPI CreateDir(const char* fsName, int mode, HWND parent,
                                  char* newName, BOOL& cancel);
    virtual void WINAPI ViewFile(const char* fsName, HWND parent,
                                 CSalamanderForViewFileOnFSAbstract* salamander,
                                 CFileData& file);
    virtual BOOL WINAPI Delete(const char* fsName, int mode, HWND parent, int panel,
                               int selectedFiles, int selectedDirs, BOOL& cancelOrError);
    virtual BOOL WINAPI CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                         int panel, int selectedFiles, int selectedDirs,
                                         char* targetPath, BOOL& operationMask,
                                         BOOL& cancelOrHandlePath, HWND dropTarget);
    virtual BOOL WINAPI CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                               const char* sourcePath, SalEnumSelection2 next,
                                               void* nextParam, int sourceFiles, int sourceDirs,
                                               char* targetPath, BOOL* invalidPathOrCancel);
    virtual BOOL WINAPI ChangeAttributes(const char* fsName, HWND parent, int panel,
                                         int selectedFiles, int selectedDirs);
    virtual void WINAPI ShowProperties(const char* fsName, HWND parent, int panel,
                                       int selectedFiles, int selectedDirs);
    virtual void WINAPI ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                    int panel, int selectedFiles, int selectedDirs);
    virtual BOOL WINAPI OpenFindDialog(const char* fsName, int panel);
    virtual void WINAPI OpenActiveFolder(const char* fsName, HWND parent);
    virtual void WINAPI GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects);

    virtual BOOL WINAPI HandleMenuMsg(
        __in UINT uMsg,
        __in WPARAM wParam,
        __in LPARAM lParam,
        __out LRESULT* plResult);

    virtual BOOL WINAPI GetNoItemsInPanelText(
        __out_ecount(textBufSize) char* textBuf,
        __in int textBufSize);

    virtual void WINAPI ShowSecurityInfo(HWND parent) {}

public:
    void NotifyPathUpdated(__in CNethoodCache::Node node);

    CNethoodCache::Node GetCurrentNode() const
    {
        return m_pathNode;
    }

    /// Informs the FS that the plugin's configuration changed.
    void ConfigurationChanged();

    static CNethoodCache::Node GetNodeFromFileData(__in const CFileData& file);

    static CNethoodCacheNode::Type GetNodeTypeFromFileData(__in const CFileData& file);

    static bool IsRootPath(__in PCTSTR pszPath);

    static void SetHideServersInRoot(bool bHide)
    {
        s_bHideServersInRoot = bHide;
    }

    static bool GetHideServersInRoot()
    {
        return s_bHideServersInRoot;
    }
};
