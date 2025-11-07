// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// when changing this header search for "BuiltForVersion" - tests for older plugin versions will no longer make sense and should be removed
#define PLUGIN_REQVER 103 // ("5.0") load only plugins that return at least this required Salamander version

//
// ****************************************************************************
// CPluginInterfaceEncapsulation + encapsulation of individual parts of the plugin interface
//
// class through which Salamander accesses the methods of the CPluginInterfaceAbstract interface,
// ensures EnterPlugin and LeavePlugin are called; direct calls to to CPluginInterfaceAbstract interface methods may lead to errors
// (e.g. refreshing a panel while operating on its data - invalid pointers, etc.)
//
// call-stack messages are handled in CPluginData where all interface methods are invoked,
// with the exception of CloseFS and ReleasePluginDataInterface

// functions called before and after invoking any plugin method
void EnterPlugin();
void LeavePlugin();

class CPluginInterfaceForArchiverEncapsulation
{
protected:
    CPluginInterfaceForArchiverAbstract* Interface; // encapsulated interface

public:
    CPluginInterfaceForArchiverEncapsulation(CPluginInterfaceForArchiverAbstract* iface = NULL) { Interface = iface; }
    // is the encapsulation initialized?
    BOOL NotEmpty() { return Interface != NULL; }
    // initialize the encapsulation
    void Init(CPluginInterfaceForArchiverAbstract* iface) { Interface = iface; }
    // are we encapsulating this iface?
    BOOL Contains(CPluginInterfaceForArchiverAbstract const* iface) { return iface != NULL && Interface == iface; }
    // returns a pointer to the encapsulated interface
    CPluginInterfaceForArchiverAbstract* GetInterface() { return Interface; }

    // methods of the CPluginInterfaceForArchiverAbstract interface without "virtual" (would be unnecessary and slower)

    BOOL ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                     CSalamanderDirectoryAbstract* dir,
                     CPluginDataInterfaceAbstract*& pluginData)
    {
        EnterPlugin();
        BOOL r = Interface->ListArchive(salamander, fileName, dir, pluginData);
        LeavePlugin();
        return r;
    }

    BOOL UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                       CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                       const char* archiveRoot, SalEnumSelection next, void* nextParam)
    {
        EnterPlugin();
        BOOL r = Interface->UnpackArchive(salamander, fileName, pluginData, targetDir, archiveRoot, next, nextParam);
        LeavePlugin();
        return r;
    }

    BOOL UnpackOneFile(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                       CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                       const CFileData* fileData, const char* targetDir,
                       const char* newFileName, BOOL* renamingNotSupported)
    {
        EnterPlugin();
        BOOL r = Interface->UnpackOneFile(salamander, fileName, pluginData, nameInArchive,
                                          fileData, targetDir, newFileName, renamingNotSupported);
        LeavePlugin();
        return r;
    }

    BOOL PackToArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                       const char* archiveRoot, BOOL move, const char* sourcePath,
                       SalEnumSelection2 next, void* nextParam)
    {
        EnterPlugin();
        BOOL r = Interface->PackToArchive(salamander, fileName, archiveRoot, move, sourcePath,
                                          next, nextParam);
        LeavePlugin();
        return r;
    }

    BOOL DeleteFromArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                           CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                           SalEnumSelection next, void* nextParam)
    {
        EnterPlugin();
        BOOL r = Interface->DeleteFromArchive(salamander, fileName, pluginData, archiveRoot,
                                              next, nextParam);
        LeavePlugin();
        return r;
    }

    BOOL UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                            const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                            CDynamicString* archiveVolumes)
    {
        EnterPlugin();
        BOOL r = Interface->UnpackWholeArchive(salamander, fileName, mask, targetDir,
                                               delArchiveWhenDone, archiveVolumes);
        LeavePlugin();
        return r;
    }

    BOOL CanCloseArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                         BOOL force, int panel)
    {
        EnterPlugin();
        BOOL r = Interface->CanCloseArchive(salamander, fileName, force, panel);
        LeavePlugin();
        return r;
    }

    BOOL GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies)
    {
        EnterPlugin();
        BOOL r = Interface->GetCacheInfo(tempPath, ownDelete, cacheCopies);
        LeavePlugin();
        return r;
    }

    void DeleteTmpCopy(const char* fileName, BOOL firstFile)
    {
        EnterPlugin();
        Interface->DeleteTmpCopy(fileName, firstFile);
        LeavePlugin();
    }

    BOOL PrematureDeleteTmpCopy(HWND parent, int copiesCount)
    {
        EnterPlugin();
        BOOL r = Interface->PrematureDeleteTmpCopy(parent, copiesCount);
        LeavePlugin();
        return r;
    }

    // ********************************************************************************
    // WARNING: lower thread priority before executing plug-in operations!
    // ********************************************************************************
};

class CPluginInterfaceForViewerEncapsulation
{
protected:
    CPluginInterfaceForViewerAbstract* Interface; // encapsulated interface

public:
    CPluginInterfaceForViewerEncapsulation(CPluginInterfaceForViewerAbstract* iface = NULL) { Interface = iface; }
    // is the encapsulation initialized?
    BOOL NotEmpty() { return Interface != NULL; }
    // encapsulation initialization
    void Init(CPluginInterfaceForViewerAbstract* iface) { Interface = iface; }
    // are we encapsulating this iface?
    BOOL Contains(CPluginInterfaceForViewerAbstract const* iface) { return iface != NULL && Interface == iface; }
    // returns a pointer to the encapsulated interface
    CPluginInterfaceForViewerAbstract* GetInterface() { return Interface; }

    // methods of the CPluginInterfaceForViewerAbstract interface without 'virtual' (unnecessary — it would only slow things down)

    BOOL ViewFile(const char* name, int left, int top, int width, int height,
                  UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                  BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                  int enumFilesSourceUID, int enumFilesCurrentIndex)
    {
        EnterPlugin();
        BOOL r = Interface->ViewFile(name, left, top, width, height, showCmd, alwaysOnTop,
                                     returnLock, lock, lockOwner, viewerData,
                                     enumFilesSourceUID, enumFilesCurrentIndex);
        LeavePlugin();
        return r;
    }

    BOOL CanViewFile(const char* name)
    {
        EnterPlugin();
        BOOL r = Interface->CanViewFile(name);
        LeavePlugin();
        return r;
    }
};

class CPluginInterfaceForMenuExtEncapsulation
{
protected:
    CPluginInterfaceForMenuExtAbstract* Interface; // encapsulated interface
    int BuiltForVersion;                           // valid only when the plugin is loaded: Salamander version the plugin was compiled for (the list of versions under LAST_VERSION_OF_SALAMANDER in spl_vers.h)

public:
    CPluginInterfaceForMenuExtEncapsulation(CPluginInterfaceForMenuExtAbstract* iface, int builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }

    // is the encapsulation initialized?
    BOOL NotEmpty() { return Interface != NULL; }

    // encapsulation initialization
    void Init(CPluginInterfaceForMenuExtAbstract* iface, BOOL builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }

    // are we encapsulating this iface?
    BOOL Contains(CPluginInterfaceForMenuExtAbstract const* iface) { return iface != NULL && Interface == iface; }
    // returns a pointer to the encapsulated interface
    CPluginInterfaceForMenuExtAbstract* GetInterface() { return Interface; }

    // methods of the CPluginInterfaceForMenuExtAbstract interface without 'virtual' (unnecessary — it would only slow things down)

    DWORD GetMenuItemState(int id, DWORD eventMask)
    {
        EnterPlugin();
        DWORD r = Interface->GetMenuItemState(id, eventMask);
        LeavePlugin();
        return r;
    }

    BOOL ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                         int id, DWORD eventMask)
    {
        EnterPlugin();
        BOOL r = Interface->ExecuteMenuItem(salamander, parent, id, eventMask);
        LeavePlugin();
        return r;
    }

    BOOL HelpForMenuItem(HWND parent, int id)
    {
        EnterPlugin();
        BOOL r = Interface->HelpForMenuItem(parent, id);
        LeavePlugin();
        return r;
    }

    void BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander)
    {
        EnterPlugin();
        Interface->BuildMenu(parent, salamander);
        LeavePlugin();
    }
};

class CPluginInterfaceForFSEncapsulation
{
protected:
    CPluginInterfaceForFSAbstract* Interface; // encapsulated interface
    int BuiltForVersion;                      // valid only when the plugin is loaded: Salamander version the plugin was compiled for (see the list of versions under LAST_VERSION_OF_SALAMANDER in spl_vers.h)

public:
    CPluginInterfaceForFSEncapsulation(CPluginInterfaceForFSAbstract* iface, int builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }

    // is the encapsulation initialized?
    BOOL NotEmpty() { return Interface != NULL; }

    // encapsulation initialization
    void Init(CPluginInterfaceForFSAbstract* iface, int builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }

    // are we encapsulating this iface?
    BOOL Contains(CPluginInterfaceForFSAbstract const* iface) { return iface != NULL && Interface == iface; }
    // returns a pointer to the encapsulated interface
    CPluginInterfaceForFSAbstract* GetInterface() { return Interface; }

    // returns the Salamander version this plugin was built for (see the list of versions under LAST_VERSION_OF_SALAMANDER in spl_vers.h)
    int GetBuiltForVersion() { return BuiltForVersion; }

    // methods of the CPluginInterfaceForFSAbstract interface without 'virtual' (unnecessary — it would only slow things down)

    CPluginFSInterfaceAbstract* OpenFS(const char* fsName, int fsNameIndex)
    {
        EnterPlugin();
        CPluginFSInterfaceAbstract* r = Interface->OpenFS(fsName, fsNameIndex);
        LeavePlugin();
        return r;
    }

    // direct interface call (does not go through CPluginData + InitDLL), a call-stack message is created
    void CloseFS(CPluginFSInterfaceAbstract* fs);

    void ExecuteChangeDriveMenuItem(int panel);

    BOOL ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                        CPluginFSInterfaceAbstract* pluginFS,
                                        const char* pluginFSName, int pluginFSNameIndex,
                                        BOOL isDetachedFS, BOOL& refreshMenu,
                                        BOOL& closeMenu, int& postCmd, void*& postCmdParam);

    // direct interface call (does not go through CPluginData + InitDLL), a call-stack message is created;
    // called right after ChangeDriveMenuItemContextMenu -- the plugin is definitely loaded
    void ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam);

    // direct interface call (does not go through CPluginData + InitDLL), call-stack-message is created;
    // called only when 'pluginFS' is in a panel - the plugin is definitely loaded
    void ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                     const char* pluginFSName, int pluginFSNameIndex,
                     CFileData& file, int isDir);

    // direct interface call (does not go through CPluginData + InitDLL), call-stack-message is created;
    // called only when 'pluginFS' exists (either in a panel or detached) - the plugin is definitely loaded
    BOOL DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                      CPluginFSInterfaceAbstract* pluginFS,
                      const char* pluginFSName, int pluginFSNameIndex);

    // direct interface call (does not go through CPluginData + InitDLL), call-stack-message is created;
    // called only when the plugin is loaded (its FS is open)
    void ConvertPathToInternal(const char* fsName, int fsNameIndex, char* fsUserPart);

    // direct interface call (does not go through CPluginData + InitDLL), call-stack-message is created;
    // called only when the plugin is loaded (its FS is open)
    void ConvertPathToExternal(const char* fsName, int fsNameIndex, char* fsUserPart);

    void EnsureShareExistsOnServer(int panel, const char* server, const char* share)
    {
        EnterPlugin();
        Interface->EnsureShareExistsOnServer(panel, server, share);
        LeavePlugin();
    }
};

class CPluginInterfaceForThumbLoaderEncapsulation
{
protected:
    CPluginInterfaceForThumbLoaderAbstract* Interface; // encapsulated interface

    const char* DLLName; // reference to the string from CPluginData of the plugin that created this interface
    const char* Version; // reference to the string from CPluginData of the plugin that created this interface

public:
    CPluginInterfaceForThumbLoaderEncapsulation(CPluginInterfaceForThumbLoaderAbstract* iface = NULL,
                                                const char* dllName = NULL, const char* version = NULL)
    {
        Interface = iface;
        DLLName = dllName;
        Version = version;
    }

    // is the encapsulation initialized?
    BOOL NotEmpty() { return Interface != NULL; }

    // initialization of the encapsulation
    void Init(CPluginInterfaceForThumbLoaderAbstract* iface, const char* dllName, const char* version)
    {
        Interface = iface;
        DLLName = dllName;
        Version = version;
    }

    // are we encapsulating this iface?
    BOOL Contains(CPluginInterfaceForThumbLoaderAbstract const* iface) { return iface != NULL && Interface == iface; }
    // returns a pointer to the encapsulated interface
    CPluginInterfaceForThumbLoaderAbstract* GetInterface() { return Interface; }

    // methods of CPluginInterfaceForThumbLoaderAbstract interface without "virtual" (it's unnecessary and would just slow things down)

    // does not need Enter/LeavePlugin because it may use only sal-general methods
    // that can be called from any thread (unlike panel operations)
    BOOL LoadThumbnail(const char* filename, int thumbWidth, int thumbHeight,
                       CSalamanderThumbnailMakerAbstract* thumbMaker, BOOL fastThumbnail)
    {
        CALL_STACK_MESSAGE7("CPluginInterfaceForThumbLoaderEncapsulation::LoadThumbnail(%s, %d, %d, , %d) (%s v. %s)",
                            filename, thumbWidth, thumbHeight, fastThumbnail, DLLName, Version);
        return Interface->LoadThumbnail(filename, thumbWidth, thumbHeight, thumbMaker, fastThumbnail);
    }
};

class CPluginInterfaceEncapsulation
{
protected:
    CPluginInterfaceAbstract* Interface; // encapsulated interface; during the entry point, it is set to -1
    int BuiltForVersion;                 // valid only while 'Interface' is active: Salamander version the plugin was compiled for (see the list of versions under LAST_VERSION_OF_SALAMANDER in spl_vers.h)

public:
    CPluginInterfaceEncapsulation()
    {
        Interface = NULL;
        BuiltForVersion = 0;
    }
    CPluginInterfaceEncapsulation(CPluginInterfaceAbstract* iface, int builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }
    // is the encapsulation initialized?
    BOOL NotEmpty() { return Interface != NULL && (INT_PTR)Interface != -1; } // -1 is set during the entry point
    // initialization of the encapsulation
    void Init(CPluginInterfaceAbstract* iface, int builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }
    // are we encapsulating this iface?
    BOOL Contains(CPluginInterfaceAbstract const* iface) { return iface != NULL && Interface == iface; }
    // returns a pointer to the encapsulated interface
    CPluginInterfaceAbstract* GetInterface() { return Interface; }

    // methods of the CPluginInterfaceAbstract interface without 'virtual' (unnecessary — it would only slow things down)

    void About(HWND parent)
    {
        EnterPlugin();
        Interface->About(parent);
        LeavePlugin();
    }

    BOOL Release(HWND parent, BOOL force)
    {
        EnterPlugin();
        BOOL r = Interface->Release(parent, force);
        LeavePlugin();
        return r;
    }

    void LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
    {
        EnterPlugin();
        Interface->LoadConfiguration(parent, regKey, registry);
        LeavePlugin();
    }

    void SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
    {
        EnterPlugin();
        Interface->SaveConfiguration(parent, regKey, registry);
        LeavePlugin();
    }

    void Configuration(HWND parent)
    {
        EnterPlugin();
        Interface->Configuration(parent);
        LeavePlugin();
    }

    void Connect(HWND parent, CSalamanderConnectAbstract* salamander)
    {
        EnterPlugin();
        Interface->Connect(parent, salamander);
        LeavePlugin();
    }

    // direct interface call (does not go through CPluginData + InitDLL), a call-stack message is created
    void ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData);

    CPluginInterfaceForArchiverAbstract* GetInterfaceForArchiver()
    {
        EnterPlugin();
        CPluginInterfaceForArchiverAbstract* r = Interface->GetInterfaceForArchiver();
        LeavePlugin();
        return r;
    }

    CPluginInterfaceForViewerAbstract* GetInterfaceForViewer()
    {
        EnterPlugin();
        CPluginInterfaceForViewerAbstract* r = Interface->GetInterfaceForViewer();
        LeavePlugin();
        return r;
    }

    CPluginInterfaceForMenuExtAbstract* GetInterfaceForMenuExt()
    {
        EnterPlugin();
        CPluginInterfaceForMenuExtAbstract* r = Interface->GetInterfaceForMenuExt();
        LeavePlugin();
        return r;
    }

    CPluginInterfaceForFSAbstract* GetInterfaceForFS()
    {
        EnterPlugin();
        CPluginInterfaceForFSAbstract* r = Interface->GetInterfaceForFS();
        LeavePlugin();
        return r;
    }

    CPluginInterfaceForThumbLoaderAbstract* GetInterfaceForThumbLoader()
    {
        EnterPlugin();
        CPluginInterfaceForThumbLoaderAbstract* r = Interface->GetInterfaceForThumbLoader();
        LeavePlugin();
        return r;
    }

    void Event(int event, DWORD param)
    {
        EnterPlugin();
        Interface->Event(event, param);
        LeavePlugin();
    }

    void ClearHistory(HWND parent)
    {
        EnterPlugin();
        Interface->ClearHistory(parent);
        LeavePlugin();
    }

    void AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs)
    {
        EnterPlugin();
        Interface->AcceptChangeOnPathNotification(path, includingSubdirs);
        LeavePlugin();
    }

    void PasswordManagerEvent(HWND parent, int event)
    {
        EnterPlugin();
        Interface->PasswordManagerEvent(parent, event);
        LeavePlugin();
    }
};

//
// ****************************************************************************
// CPluginDataInterfaceEncapsulation
//
// Class through which Salamander accesses the methods of the CPluginDataInterfaceAbstract interface.
// It ensures EnterPlugin and LeavePlugin are invoked. Direct calls to the
// CPluginDataInterfaceAbstract interface methods may lead to errors (e.g., refreshing a panel while its data
// are in use can leave invalid pointers, etc.)

class CFilesArray;

class CPluginDataInterfaceEncapsulation
{
protected:
    CPluginDataInterfaceAbstract* Interface; // encapsulated interface
    int BuiltForVersion;                     // valid only while 'Interface' is active: Salamander version this plugin was compiled for (see the list of versions under LAST_VERSION_OF_SALAMANDER in spl_vers.h)

    const char* DLLName;              // string reference from the CPluginData of the plugin that created this interface
    const char* Version;              // string reference from the CPluginData of the plugin that created this interface
    CPluginInterfaceAbstract* Plugin; // plugin that created this interface

public:
    CPluginDataInterfaceEncapsulation()
    {
        Interface = NULL;
        DLLName = NULL;
        Version = NULL;
        Plugin = NULL;
        BuiltForVersion = 0;
    }

    CPluginDataInterfaceEncapsulation(CPluginDataInterfaceAbstract* iface, const char* dllName,
                                      const char* version, CPluginInterfaceAbstract* plugin,
                                      int builtForVersion)
    {
        Interface = iface;
        DLLName = dllName;
        Version = version;
        Plugin = plugin;
        BuiltForVersion = builtForVersion;
    }

    // is the encapsulation initialized?
    BOOL NotEmpty() { return Interface != NULL; }

    // initialization of the encapsulation
    void Init(CPluginDataInterfaceAbstract* iface, const char* dllName, const char* version,
              CPluginInterfaceAbstract* plugin, int builtForVersion)
    {
        Interface = iface;
        DLLName = dllName;
        Version = version;
        Plugin = plugin;
        BuiltForVersion = builtForVersion;
    }

    // are we encapsulating this iface?
    BOOL Contains(CPluginDataInterfaceAbstract const* iface) { return iface != NULL && Interface == iface; }

    // returns a pointer to the encapsulated interface
    CPluginDataInterfaceAbstract* GetInterface() { return Interface; }

    // returns a reference to the string from the CPluginData of the plugin that created this interface
    const char* GetDLLName() { return DLLName; }

    // returns a reference to the string from the CPluginData of the plugin that created this interface
    const char* GetVersion() { return Version; }

    // returns the plugin interface that created the PluginData interface
    CPluginInterfaceAbstract* GetPluginInterface() { return Plugin; }

    // returns the Salamander version this plugin was compiled for (see the list of versions under LAST_VERSION_OF_SALAMANDER in spl_vers.h)
    int GetBuiltForVersion() { return BuiltForVersion; }

    // replacement (for performance reasons) for ReleasePluginData
    void ReleaseFilesOrDirs(CFilesArray* filesOrDirs, BOOL areDirs);

    // Same as ReleasePluginData but renamed to highlight the ReleaseFilesOrDirs call
    void ReleasePluginData2(CFileData& file, BOOL isDir)
    {
        CALL_STACK_MESSAGE4("CPluginDataInterfaceEncapsulation::ReleasePluginData2(, %d) (%s v. %s)",
                            isDir, DLLName, Version);
        EnterPlugin();
        Interface->ReleasePluginData(file, isDir);
        LeavePlugin();
    }

    // methods of the CPluginDataInterfaceAbstract interface without 'virtual' (unnecessary — it would only slow things down)

    BOOL CallReleaseForFiles()
    {
        CALL_STACK_MESSAGE3("CPluginDataInterfaceEncapsulation::CallReleaseForFiles() (%s v. %s)",
                            DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->CallReleaseForFiles();
        LeavePlugin();
        return r;
    }

    BOOL CallReleaseForDirs()
    {
        CALL_STACK_MESSAGE3("CPluginDataInterfaceEncapsulation::CallReleaseForDirs() (%s v. %s)",
                            DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->CallReleaseForDirs();
        LeavePlugin();
        return r;
    }

    void GetFileDataForUpDir(const char* archivePath, CFileData& upDir)
    {
        CALL_STACK_MESSAGE4("CPluginDataInterfaceEncapsulation::GetFileDataForUpDir(%s,) (%s v. %s)",
                            archivePath, DLLName, Version);
        EnterPlugin();
        Interface->GetFileDataForUpDir(archivePath, upDir);
        LeavePlugin();
    }

    BOOL GetFileDataForNewDir(const char* dirName, CFileData& dir)
    {
        SLOW_CALL_STACK_MESSAGE4("CPluginDataInterfaceEncapsulation::GetFileDataForNewDir(%s,) (%s v. %s)",
                                 dirName, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->GetFileDataForNewDir(dirName, dir);
        LeavePlugin();
        return r;
    }

    CIconList* GetSimplePluginIcons(CIconSizeEnum iconSize)
    {
        CALL_STACK_MESSAGE3("CPluginDataInterfaceEncapsulation::GetSimplePluginIcons() (%s v. %s)",
                            DLLName, Version);

        int size;
        switch (iconSize)
        {
        case ICONSIZE_16:
            size = SALICONSIZE_16;
            break;
        case ICONSIZE_32:
            size = SALICONSIZE_32;
            break;
        case ICONSIZE_48:
            size = SALICONSIZE_48;
            break;
        default:
        {
            TRACE_E("GetSimplePluginIcons() unexpected iconSize=" << iconSize);
            size = SALICONSIZE_16;
            break;
        }
        }

        EnterPlugin();
        HIMAGELIST il = Interface->GetSimplePluginIcons(size);
        LeavePlugin();

        // convert the plugin's image list into our CIconList
        CIconList* iconList = new CIconList();
        if (iconList != NULL)
        {
            iconList->SetBkColor(GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]));
            if (!iconList->CreateFromImageList(il, IconSizes[iconSize]))
            {
                delete iconList;
                iconList = NULL;
            }
        }
        else
            TRACE_E(LOW_MEMORY);

        return iconList;
    }

    void ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth)
    {
        CALL_STACK_MESSAGE5("CPluginDataInterfaceEncapsulation::ColumnFixedWidthShouldChange(%d, , %d) (%s v. %s)",
                            leftPanel, newFixedWidth, DLLName, Version);
        EnterPlugin();
        Interface->ColumnFixedWidthShouldChange(leftPanel, column, newFixedWidth);
        LeavePlugin();
    }

    void ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth)
    {
        CALL_STACK_MESSAGE5("CPluginDataInterfaceEncapsulation::ColumnWidthWasChanged(%d, , %d) (%s v. %s)",
                            leftPanel, newWidth, DLLName, Version);
        EnterPlugin();
        Interface->ColumnWidthWasChanged(leftPanel, column, newWidth);
        LeavePlugin();
    }

    BOOL GetInfoLineContent(int panel, const CFileData* file, BOOL isDir, int selectedFiles,
                            int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
                            char* buffer, DWORD* hotTexts, int& hotTextsCount)
    {
        CALL_STACK_MESSAGE9("CPluginDataInterfaceEncapsulation::GetInfoLineContent(%d, , %d, %d, %d, %d, %I64u, , ,) (%s v. %s)",
                            panel, isDir, selectedFiles, selectedDirs, displaySize,
                            selectedSize.Value, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->GetInfoLineContent(panel, file, isDir, selectedFiles, selectedDirs,
                                               displaySize, selectedSize, buffer,
                                               hotTexts, hotTextsCount);
        LeavePlugin();
        return r;
    }

    BOOL CanBeCopiedToClipboard()
    {
        CALL_STACK_MESSAGE3("CPluginDataInterfaceEncapsulation::CanBeCopiedToClipboard() (%s v. %s)",
                            DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->CanBeCopiedToClipboard();
        LeavePlugin();
        return r;
    }

    BOOL GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size)
    {
        CALL_STACK_MESSAGE4("CPluginDataInterfaceEncapsulation::GetByteSize(, %d,) (%s v. %s)",
                            isDir, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->GetByteSize(file, isDir, size);
        LeavePlugin();
        return r;
    }

    BOOL GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date)
    {
        CALL_STACK_MESSAGE4("CPluginDataInterfaceEncapsulation::GetLastWriteDate(, %d,) (%s v. %s)",
                            isDir, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->GetLastWriteDate(file, isDir, date);
        LeavePlugin();
        return r;
    }

    BOOL GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time)
    {
        CALL_STACK_MESSAGE4("CPluginDataInterfaceEncapsulation::GetLastWriteTime(, %d,) (%s v. %s)",
                            isDir, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->GetLastWriteTime(file, isDir, time);
        LeavePlugin();
        return r;
    }

    // does not need Enter/LeavePlugin because it may use only sal-general methods
    // that can be called from any thread (unlike panel operations)
    BOOL HasSimplePluginIcon(CFileData& file, BOOL isDir)
    {
        // called for all files and directories; CALL_STACK_MESSAGE slows it down here (40ms for 3000 calls)
        //      CALL_STACK_MESSAGE5("CPluginDataInterfaceEncapsulation::HasSimplePluginIcon(%s, %d) (%s v. %s)",
        //                          file.Name, isDir, DLLName, Version);
        return Interface->HasSimplePluginIcon(file, isDir);
    }

    // does not need Enter/LeavePlugin because it may use only sal-general methods
    // that can be called from any thread (unlike panel operations)
    HICON GetPluginIcon(const CFileData* file, CIconSizeEnum iconSize, BOOL& destroyIcon)
    {
        CALL_STACK_MESSAGE4("CPluginDataInterfaceEncapsulation::GetPluginIcon(%s,) (%s v. %s)",
                            file->Name, DLLName, Version);
        int size;
        switch (iconSize)
        {
        case ICONSIZE_16:
            size = SALICONSIZE_16;
            break;
        case ICONSIZE_32:
            size = SALICONSIZE_32;
            break;
        case ICONSIZE_48:
            size = SALICONSIZE_48;
            break;
        default:
        {
            TRACE_E("GetPluginIcon() unexpected iconSize=" << iconSize);
            size = SALICONSIZE_16;
            break;
        }
        }
        return Interface->GetPluginIcon(file, size, destroyIcon);
    }

    // does not need Enter/LeavePlugin because it may use only sal-general methods
    // that can be called from any thread (unlike panel operations)
    int CompareFilesFromFS(const CFileData* file1, const CFileData* file2)
    {
        CALL_STACK_MESSAGE_NONE
        // no call-stack here for speed reasons
        //      CALL_STACK_MESSAGE5("CPluginDataInterfaceEncapsulation::CompareFilesFromFS(%s, %s) (%s v. %s)",
        //                          file1->Name, file2->Name, DLLName, Version);
        return Interface->CompareFilesFromFS(file1, file2);
    }

    // does not need Enter/LeavePlugin because it may use only sal-general methods
    // that can be called from any thread (unlike panel operations)
    void SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                   const CFileData* upperDir)
    {
        CALL_STACK_MESSAGE5("CPluginDataInterfaceEncapsulation::SetupView(%d, , %s,) (%s v. %s)",
                            leftPanel, archivePath, DLLName, Version);
        Interface->SetupView(leftPanel, view, archivePath, upperDir);
    }

private:
    void ReleasePluginData(CFileData& file, BOOL isDir) {} // would be too slow, replaced by ReleaseFilesOrDirs
};

//
// ****************************************************************************
// CSalamanderForViewFileOnFS
//

class CSalamanderForViewFileOnFS : public CSalamanderForViewFileOnFSAbstract
{
protected:
    BOOL AltView;
    DWORD HandlerID;

    int CallsCounter;

public:
    CSalamanderForViewFileOnFS(BOOL altView, DWORD handlerID)
    {
        AltView = altView;
        HandlerID = handlerID;
        CallsCounter = 0;
    }

    ~CSalamanderForViewFileOnFS()
    {
        if (CallsCounter != 0)
        {
            TRACE_E("You have probably forgot to call CSalamanderForViewFileOnFS::FreeFileNameInCache! "
                    "(calls: "
                    << CallsCounter << ")");
        }
    }

    virtual const char* WINAPI AllocFileNameInCache(HWND parent, const char* uniqueFileName, const char* nameInCache,
                                                    const char* rootTmpPath, BOOL& fileExists);
    virtual BOOL WINAPI OpenViewer(HWND parent, const char* fileName, HANDLE* fileLock,
                                   BOOL* fileLockOwner);
    virtual void WINAPI FreeFileNameInCache(const char* uniqueFileName, BOOL fileExists, BOOL newFileOK,
                                            const CQuadWord& newFileSize, HANDLE fileLock,
                                            BOOL fileLockOwner, BOOL removeAsSoonAsPossible);
};

//
// ****************************************************************************
// CPluginFSInterfaceEncapsulation
//
// class through which Salamander accesses methods of the CPluginFSInterfaceAbstract interface;
// it ensures EnterPlugin and LeavePlugin are called. Direct calls to
// methods of the CPluginFSInterfaceAbstract interface may cause issues (e.g., refreshing
// a panel while its data are being processed could leave invalid pointers, etc.)

class CPluginFSInterfaceEncapsulation
{
protected:
    CPluginFSInterfaceAbstract* Interface; // encapsulated interface
    int BuiltForVersion;                   // valid only when the plugin is loaded: Salamander version the plugin was compiled for (see the list of versions under LAST_VERSION_OF_SALAMANDER in spl_vers.h)

    const char* DLLName;                           // reference to the string from CPluginData of the plugin that created this interface
    const char* Version;                           // reference to the string from CPluginData of the plugin that created this interface
    CPluginInterfaceForFSEncapsulation IfaceForFS; // plugin that created this interface (FS part)
    CPluginInterfaceAbstract* Iface;               // plugin that created this interface (base)
    DWORD SupportedServices;                       // last value returned by the Interface->GetSupportedServices() method
    char PluginFSName[MAX_PATH];                   // name of the opened FS
    int PluginFSNameIndex;                         // index of the opened FS name

    static DWORD PluginFSTime; // global "time" (counter) used to obtain the creation time of a FS
    DWORD PluginFSCreateTime;  // "time" of creation of this FS (0 == uninitialized)

    int ChngDrvDuplicateItemIndex; // index number of the duplicate item in the Change Drive menu (not used if the menu contains no duplicate item for this FS) (0 = uninitialized value)

public:
    CPluginFSInterfaceEncapsulation() : IfaceForFS(NULL, 0)
    {
        Interface = NULL;
        BuiltForVersion = 0;
        DLLName = NULL;
        Version = NULL;
        Iface = NULL;
        SupportedServices = 0;
        PluginFSName[0] = 0;
        PluginFSNameIndex = -1;
        PluginFSCreateTime = 0;
        ChngDrvDuplicateItemIndex = 0;
    }

    CPluginFSInterfaceEncapsulation(CPluginFSInterfaceAbstract* fsIface, const char* dllName,
                                    const char* version, CPluginInterfaceForFSAbstract* ifaceForFS,
                                    CPluginInterfaceAbstract* iface, const char* pluginFSName,
                                    int pluginFSNameIndex, DWORD pluginFSCreateTime,
                                    int chngDrvDuplicateItemIndex, int builtForVersion)
        : IfaceForFS(ifaceForFS, builtForVersion)
    {
        Interface = fsIface;
        BuiltForVersion = builtForVersion;
        DLLName = dllName;
        Version = version;
        Iface = iface;
        if (Interface != NULL)
            GetSupportedServices();
        else
            SupportedServices = 0;
        if (pluginFSName != NULL)
            lstrcpyn(PluginFSName, pluginFSName, MAX_PATH);
        else
            PluginFSName[0] = 0;
        PluginFSNameIndex = pluginFSNameIndex;
        if (pluginFSCreateTime == -1)
            PluginFSCreateTime = PluginFSTime++;
        else
            PluginFSCreateTime = pluginFSCreateTime;
        ChngDrvDuplicateItemIndex = chngDrvDuplicateItemIndex;
    }

    // is the encapsulation initialized?
    BOOL NotEmpty() { return Interface != NULL; }

    // initialization of the encapsulation
    void Init(CPluginFSInterfaceAbstract* fsIface, const char* dllName, const char* version,
              CPluginInterfaceForFSAbstract* ifaceForFS, CPluginInterfaceAbstract* iface,
              const char* pluginFSName, int pluginFSNameIndex, DWORD pluginFSCreateTime,
              int chngDrvDuplicateItemIndex, int builtForVersion)
    {
        Interface = fsIface;
        BuiltForVersion = builtForVersion;
        DLLName = dllName;
        Version = version;
        IfaceForFS.Init(ifaceForFS, builtForVersion);
        Iface = iface;
        if (Interface != NULL)
            GetSupportedServices();
        else
            SupportedServices = 0;
        if (pluginFSName != NULL)
            lstrcpyn(PluginFSName, pluginFSName, MAX_PATH);
        else
            PluginFSName[0] = 0;
        PluginFSNameIndex = pluginFSNameIndex;
        if (pluginFSCreateTime == -1)
            PluginFSCreateTime = PluginFSTime++;
        else
            PluginFSCreateTime = pluginFSCreateTime;
        ChngDrvDuplicateItemIndex = chngDrvDuplicateItemIndex;
    }

    // are we encapsulating this iface?
    BOOL Contains(CPluginFSInterfaceAbstract const* iface) { return iface != NULL && Interface == iface; }

    // returns a pointer to the encapsulated interface
    CPluginFSInterfaceAbstract* GetInterface() { return Interface; }

    // returns the Salamander version this plugin was compiled for (see the list of versions under
    // LAST_VERSION_OF_SALAMANDER in spl_vers.h)
    int GetBuiltForVersion() { return BuiltForVersion; }

    // returns a reference to the string from the CPluginData of the plugin that created this interface
    const char* GetDLLName() { return DLLName; }

    // returns a reference to the string from the CPluginData of the plugin that created this interface
    const char* GetVersion() { return Version; }

    // returns the plug-in interface that created the FS interface (FS part)
    CPluginInterfaceForFSEncapsulation* GetPluginInterfaceForFS() { return &IfaceForFS; }

    // returns the plug-in interface that created the FS interface (base class)
    CPluginInterfaceAbstract* GetPluginInterface() { return Iface; }

    // returns the name of the opened FS
    const char* GetPluginFSName() { return PluginFSName; }

    // returns the index name of the opened FS
    int GetPluginFSNameIndex() { return PluginFSNameIndex; }

    // change the FS name
    void SetPluginFS(const char* fsName, int fsNameIndex)
    {
        lstrcpyn(PluginFSName, fsName, MAX_PATH);
        PluginFSNameIndex = fsNameIndex;
    }

    DWORD GetPluginFSCreateTime() { return PluginFSCreateTime; }

    int GetChngDrvDuplicateItemIndex() { return ChngDrvDuplicateItemIndex; }
    void SetChngDrvDuplicateItemIndex(int i) { ChngDrvDuplicateItemIndex = i; }

    // returns TRUE if 'fsName' belongs to the same plugin as this FS; if it returns TRUE,
    // it also returns the index 'fsNameIndex' of the plugin FS name 'fsName'
    BOOL IsFSNameFromSamePluginAsThisFS(const char* fsName, int& fsNameIndex);

    // returns TRUE if the path 'fsName':'fsUserPart' is from this FS ('fsName' is from the same plugin
    // as this FS and IsOurPath('fsName', 'fsUserPart') returns TRUE)
    BOOL IsPathFromThisFS(const char* fsName, const char* fsUserPart);

    // returns TRUE if the service on the FS is supported
    BOOL IsServiceSupported(DWORD s) { return (SupportedServices & s) != 0; }

    // methods of the CPluginFSInterfaceAbstract interface without 'virtual' (unnecessary — it would only slow things down)

    BOOL GetCurrentPath(char* userPart)
    {
        CALL_STACK_MESSAGE3("CPluginFSInterfaceEncapsulation::GetCurrentPath() (%s v. %s)",
                            DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->GetCurrentPath(userPart);
        LeavePlugin();
        return r;
    }

    BOOL GetFullName(CFileData& file, int isDir, char* buf, int bufSize)
    {
        CALL_STACK_MESSAGE5("CPluginFSInterfaceEncapsulation::GetFullName(, %d, , %d) (%s v. %s)",
                            isDir, bufSize, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->GetFullName(file, isDir, buf, bufSize);
        LeavePlugin();
        return r;
    }

    BOOL GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::GetFullFSPath(, %s, %s, %d,) (%s v. %s)",
                            fsName, path, pathSize, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->GetFullFSPath(parent, fsName, path, pathSize, success);
        LeavePlugin();
        return r;
    }

    BOOL GetRootPath(char* userPart)
    {
        CALL_STACK_MESSAGE3("CPluginFSInterfaceEncapsulation::GetRootPath() (%s v. %s)",
                            DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->GetRootPath(userPart);
        LeavePlugin();
        return r;
    }

    BOOL IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::IsCurrentPath(%d, %d, %s) (%s v. %s)",
                            currentFSNameIndex, fsNameIndex, userPart, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->IsCurrentPath(currentFSNameIndex, fsNameIndex, userPart);
        LeavePlugin();
        return r;
    }

    BOOL IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::IsOurPath(%d, %d, %s) (%s v. %s)",
                            currentFSNameIndex, fsNameIndex, userPart, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->IsOurPath(currentFSNameIndex, fsNameIndex, userPart);
        LeavePlugin();
        return r;
    }

    BOOL ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                    const char* userPart, char* cutFileName,
                    BOOL* pathWasCut, BOOL forceRefresh, int mode)
    {
        CALL_STACK_MESSAGE9("CPluginFSInterfaceEncapsulation::ChangePath(%d, %s, %d, %s, , , %d, %d) (%s v. %s)",
                            currentFSNameIndex, fsName, fsNameIndex, userPart, forceRefresh, mode, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->ChangePath(currentFSNameIndex, fsName, fsNameIndex, userPart,
                                       cutFileName, pathWasCut, forceRefresh, mode);
        CALL_STACK_MESSAGE1("CPluginFSInterface::GetSupportedServices()");
        SupportedServices = Interface->GetSupportedServices();
        LeavePlugin();
        return r;
    }

    // in the debug version we count OpenedPDCounter, therefore it must be in the .cpp module
    BOOL ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                         CPluginDataInterfaceAbstract*& pluginData,
                         int& iconsType, BOOL forceRefresh);

    BOOL TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::TryCloseOrDetach(%d, %d, , %d) (%s v. %s)",
                            forceClose, canDetach, reason, DLLName, Version);
        EnterPlugin();
        BOOL r = Interface->TryCloseOrDetach(forceClose, canDetach, detach, reason);
        LeavePlugin();
        return r;
    }

    void ReleaseObject(HWND parent)
    {
        CALL_STACK_MESSAGE3("CPluginFSInterfaceEncapsulation::ReleaseObject() (%s v. %s)",
                            DLLName, Version);
        EnterPlugin();
        Interface->ReleaseObject(parent);
        LeavePlugin();
    }

    void Event(int event, DWORD param) // FIXME_X64 - shouldn't 'param' be able to hold an x64 value?
    {
        CALL_STACK_MESSAGE5("CPluginFSInterfaceEncapsulation::Event(%d, 0x%X) (%s v. %s)",
                            (int)event, param, DLLName, Version);
        EnterPlugin();
        Interface->Event(event, param);
        LeavePlugin();
    }

    BOOL GetChangeDriveOrDisconnectItem(const char* fsName, char*& title, HICON& icon, BOOL& destroyIcon);

    HICON GetFSIcon(BOOL& destroyIcon);

    void GetDropEffect(const char* srcFSPath, const char* tgtFSPath, DWORD allowedEffects,
                       DWORD keyState, DWORD* dropEffect)
    {
        CALL_STACK_MESSAGE3("CPluginFSInterfaceEncapsulation::GetDropEffect(, , , ,) (%s v. %s)",
                            DLLName, Version);
        EnterPlugin();
        Interface->GetDropEffect(srcFSPath, tgtFSPath, allowedEffects, keyState, dropEffect);
        LeavePlugin();
    }

    void GetFSFreeSpace(CQuadWord* retValue)
    {
        CALL_STACK_MESSAGE3("CPluginFSInterfaceEncapsulation::GetFreeSpace() (%s v. %s)",
                            DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_GETFREESPACE))
        {
            EnterPlugin();
            Interface->GetFSFreeSpace(retValue);
            LeavePlugin();
        }
        else
            *retValue = CQuadWord(-1, -1);
    }

    BOOL GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::GetNextDirectoryLineHotPath(%s, %d, %d) (%s v. %s)",
                            text, pathLen, offset, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_GETNEXTDIRLINEHOTPATH))
        {
            EnterPlugin();
            BOOL r = Interface->GetNextDirectoryLineHotPath(text, pathLen, offset);
            LeavePlugin();
            return r;
        }
        else
            return FALSE;
    }

    void CompleteDirectoryLineHotPath(char* path, int pathBufSize)
    {
        CALL_STACK_MESSAGE5("CPluginFSInterfaceEncapsulation::CompleteDirectoryLineHotPath(%s, %d) (%s v. %s)",
                            path, pathBufSize, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_GETNEXTDIRLINEHOTPATH))
        {
            EnterPlugin();
            Interface->CompleteDirectoryLineHotPath(path, pathBufSize);
            LeavePlugin();
        }
    }

    BOOL GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::GetPathForMainWindowTitle(%s, %d, , %d) (%s v. %s)",
                            fsName, mode, bufSize, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_GETPATHFORMAINWNDTITLE))
        {
            EnterPlugin();
            BOOL r = Interface->GetPathForMainWindowTitle(fsName, mode, buf, bufSize);
            LeavePlugin();
            return r;
        }
        else
            return FALSE;
    }

    void ShowInfoDialog(const char* fsName, HWND parent)
    {
        CALL_STACK_MESSAGE4("CPluginFSInterfaceEncapsulation::ShowInfoDialog(%s,) (%s v. %s)",
                            fsName, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_SHOWINFO))
        {
            EnterPlugin();
            Interface->ShowInfoDialog(fsName, parent);
            LeavePlugin();
        }
    }

    BOOL ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo)
    {
        CALL_STACK_MESSAGE4("CPluginFSInterfaceEncapsulation::ExecuteCommandLine(, %s, ,) (%s v. %s)",
                            command, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_COMMANDLINE))
        {
            EnterPlugin();
            BOOL r = Interface->ExecuteCommandLine(parent, command, selFrom, selTo);
            LeavePlugin();
            return r;
        }
        else
            return FALSE;
    }

    BOOL QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                     char* newName, BOOL& cancel)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::QuickRename(%s, %d, , , %d, ,) (%s v. %s)",
                            fsName, mode, isDir, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_QUICKRENAME))
        {
            EnterPlugin();
            BOOL r = Interface->QuickRename(fsName, mode, parent, file, isDir, newName, cancel);
            LeavePlugin();
            return r;
        }
        else
        {
            cancel = TRUE;
            return FALSE;
        }
    }

    // WARNING: used exclusively within the EnterPlugin+LeavePlugin section, do not call outside this section!
    void AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::AcceptChangeOnPathNotification(%s, %s, %d) (%s v. %s)",
                            fsName, path, includingSubdirs, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_ACCEPTSCHANGENOTIF))
        {
            Interface->AcceptChangeOnPathNotification(fsName, path, includingSubdirs);
        }
    }

    BOOL CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel)
    {
        CALL_STACK_MESSAGE5("CPluginFSInterfaceEncapsulation::CreateDir(%s, %d, , ,) (%s v. %s)",
                            fsName, mode, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_CREATEDIR))
        {
            EnterPlugin();
            BOOL r = Interface->CreateDir(fsName, mode, parent, newName, cancel);
            LeavePlugin();
            return r;
        }
        else
        {
            cancel = TRUE;
            return FALSE;
        }
    }

    void ViewFile(const char* fsName, HWND parent,
                  CSalamanderForViewFileOnFSAbstract* salamander, CFileData& file)
    {
        CALL_STACK_MESSAGE4("CPluginFSInterfaceEncapsulation::ViewFile(%s, , ,) (%s v. %s)",
                            fsName, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_VIEWFILE))
        {
            EnterPlugin();
            Interface->ViewFile(fsName, parent, salamander, file);
            LeavePlugin();
        }
    }

    BOOL Delete(const char* fsName, int mode, HWND parent, int panel,
                int selectedFiles, int selectedDirs, BOOL& cancelOrError)
    {
        CALL_STACK_MESSAGE8("CPluginFSInterfaceEncapsulation::Delete(%s, %d, , %d, %d, %d,) (%s v. %s)",
                            fsName, mode, panel, selectedFiles, selectedDirs, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_DELETE))
        {
            EnterPlugin();
            BOOL r = Interface->Delete(fsName, mode, parent, panel, selectedFiles, selectedDirs, cancelOrError);
            LeavePlugin();
            return r;
        }
        else
        {
            cancelOrError = TRUE;
            return FALSE;
        }
    }

    BOOL CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                          int panel, int selectedFiles, int selectedDirs,
                          char* targetPath, BOOL& operationMask,
                          BOOL& cancelOrHandlePath, HWND dropTarget)
    {
        CALL_STACK_MESSAGE10("CPluginFSInterfaceEncapsulation::CopyOrMoveFromFS(%d, %d, %s, , %d, %d, %d, %s, , ,) (%s v. %s)",
                             copy, mode, fsName, panel, selectedFiles, selectedDirs, targetPath, DLLName, Version);
        if (copy && IsServiceSupported(FS_SERVICE_COPYFROMFS) ||
            !copy && IsServiceSupported(FS_SERVICE_MOVEFROMFS))
        {
            EnterPlugin();
            BOOL r = Interface->CopyOrMoveFromFS(copy, mode, fsName, parent, panel, selectedFiles,
                                                 selectedDirs, targetPath, operationMask,
                                                 cancelOrHandlePath, dropTarget);
            LeavePlugin();
            return r;
        }
        else
        {
            cancelOrHandlePath = TRUE;
            return TRUE; // cancel
        }
    }

    BOOL CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                const char* sourcePath, SalEnumSelection2 next,
                                void* nextParam, int sourceFiles, int sourceDirs,
                                char* targetPath, BOOL* invalidPathOrCancel)
    {
        CALL_STACK_MESSAGE9("CPluginFSInterfaceEncapsulation::CopyOrMoveFromDiskToFS(%d, %d, %s, , %s, , , %d, %d, ,) (%s v. %s)",
                            copy, mode, fsName, sourcePath, sourceFiles, sourceDirs, DLLName, Version);
        if (copy && IsServiceSupported(FS_SERVICE_COPYFROMDISKTOFS) ||
            !copy && IsServiceSupported(FS_SERVICE_MOVEFROMDISKTOFS))
        {
            EnterPlugin();
            BOOL r = Interface->CopyOrMoveFromDiskToFS(copy, mode, fsName, parent, sourcePath, next, nextParam,
                                                       sourceFiles, sourceDirs, targetPath, invalidPathOrCancel);
            LeavePlugin();
            return r;
        }
        else
        { // cancel
            if (mode == 1)
                return FALSE;
            else
            {
                SalMessageBox(parent, LoadStr(IDS_FSCOPYMOVE_TOFS_NOTSUP),
                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                if (invalidPathOrCancel != NULL)
                    *invalidPathOrCancel = TRUE;
                return FALSE; // let the user fix the target path (copy/move to this path is not supported)
            }
        }
    }

    BOOL ChangeAttributes(const char* fsName, HWND parent, int panel,
                          int selectedFiles, int selectedDirs)
    {
        CALL_STACK_MESSAGE7("CPluginFSInterfaceEncapsulation::ChangeAttributes(%s, , %d, %d, %d) (%s v. %s)",
                            fsName, panel, selectedFiles, selectedDirs, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_CHANGEATTRS))
        {
            EnterPlugin();
            BOOL r = Interface->ChangeAttributes(fsName, parent, panel, selectedFiles, selectedDirs);
            LeavePlugin();
            return r;
        }
        else
            return FALSE; // cancel
    }

    void ShowProperties(const char* fsName, HWND parent, int panel, int selectedFiles, int selectedDirs)
    {
        CALL_STACK_MESSAGE7("CPluginFSInterfaceEncapsulation::ShowProperties(%s, , %d, %d, %d) (%s v. %s)",
                            fsName, panel, selectedFiles, selectedDirs, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_SHOWPROPERTIES))
        {
            EnterPlugin();
            Interface->ShowProperties(fsName, parent, panel, selectedFiles, selectedDirs);
            LeavePlugin();
        }
    }

    void ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                     int panel, int selectedFiles, int selectedDirs)
    {
        CALL_STACK_MESSAGE10("CPluginFSInterfaceEncapsulation::ContextMenu(%s, , %d, %d, %d, %d, %d, %d) (%s v. %s)",
                             fsName, menuX, menuY, (int)type, panel, selectedFiles, selectedDirs, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_CONTEXTMENU))
        {
            EnterPlugin();
            Interface->ContextMenu(fsName, parent, menuX, menuY, type, panel, selectedFiles, selectedDirs);
            LeavePlugin();
        }
    }

    BOOL HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::HandleMenuMsg(%u, 0x%IX, 0x%IX,) (%s v. %s)",
                            uMsg, wParam, lParam, DLLName, Version);
        BOOL ret = FALSE;
        if (IsServiceSupported(FS_SERVICE_CONTEXTMENU))
        {
            EnterPlugin();
            ret = Interface->HandleMenuMsg(uMsg, wParam, lParam, plResult);
            LeavePlugin();
        }
        return ret;
    }

    BOOL OpenFindDialog(const char* fsName, int panel)
    {
        CALL_STACK_MESSAGE5("CPluginFSInterfaceEncapsulation::OpenFindDialog(%s, %d) (%s v. %s)",
                            fsName, panel, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_OPENFINDDLG))
        {
            EnterPlugin();
            BOOL ret = Interface->OpenFindDialog(fsName, panel);
            LeavePlugin();
            return ret;
        }
        else
            return FALSE;
    }

    void OpenActiveFolder(const char* fsName, HWND parent)
    {
        CALL_STACK_MESSAGE4("CPluginFSInterfaceEncapsulation::OpenActiveFolder(%s, ,) (%s v. %s)",
                            fsName, DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_OPENACTIVEFOLDER))
        {
            EnterPlugin();
            Interface->OpenActiveFolder(fsName, parent);
            LeavePlugin();
        }
    }

    void GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects)
    {
        CALL_STACK_MESSAGE6("CPluginFSInterfaceEncapsulation::GetAllowedDropEffects(%d, %s, %u) (%s v. %s)",
                            mode, tgtFSPath, (allowedEffects == NULL ? 0 : *allowedEffects), DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_MOVEFROMFS) || IsServiceSupported(FS_SERVICE_COPYFROMFS))
        {
            EnterPlugin();
            Interface->GetAllowedDropEffects(mode, tgtFSPath, allowedEffects);
            LeavePlugin();
        }
    }

    BOOL GetNoItemsInPanelText(char* textBuf, int textBufSize)
    {
        CALL_STACK_MESSAGE4("CPluginFSInterfaceEncapsulation::GetNoItemsInPanelText(, %d) (%s v. %s)",
                            textBufSize, DLLName, Version);
        EnterPlugin();
        BOOL ret = Interface->GetNoItemsInPanelText(textBuf, textBufSize);
        LeavePlugin();
        return ret;
    }

    void ShowSecurityInfo(HWND parent)
    {
        CALL_STACK_MESSAGE3("CPluginFSInterfaceEncapsulation::ShowSecurityInfo() (%s v. %s)",
                            DLLName, Version);
        if (IsServiceSupported(FS_SERVICE_SHOWSECURITYINFO))
        {
            EnterPlugin();
            Interface->ShowSecurityInfo(parent);
            LeavePlugin();
        }
    }

    // ********************************************************************************
    // WARNING: lower the thread priority before launching an operation in the plugin !!!
    // ********************************************************************************

private:
    // prefer calling IsServiceSupported() - SupportedServices is cached
    DWORD GetSupportedServices()
    {
        CALL_STACK_MESSAGE3("CPluginFSInterfaceEncapsulation::GetSupportedServices() (%s v. %s)",
                            DLLName, Version);
        EnterPlugin();
        SupportedServices = Interface->GetSupportedServices();
        LeavePlugin();
        return SupportedServices;
    }
};

//
// ****************************************************************************
// CDetachedFSList
//

class CDetachedFSList : public TIndirectArray<CPluginFSInterfaceEncapsulation>
{
public:
    CDetachedFSList() : TIndirectArray<CPluginFSInterfaceEncapsulation>(10, 10) {}
    ~CDetachedFSList()
    {
        if (Count > 0)
        {
            TRACE_E("Deleting DetachedFSList which still contains " << Count << " opened FS.");
        }
    }
};

//
// ****************************************************************************
// CSalamanderColumns
//

// the interface should be ready for repeated calls (listing may not succeed on the
// first attempt (for FS) or may fail completely). alternatively, ensure changes are made
// only if listing succeeds, or note in spl_xxxx.h that this interface may be
// accessed only when the listing returns TRUE (success)

class CSalamanderView : public CSalamanderViewAbstract
{
protected:
    CFilesWindow* Panel;

public:
    CSalamanderView(CFilesWindow* panel);

    // -------------- panel ----------------
    virtual DWORD WINAPI GetViewMode();
    virtual void WINAPI SetViewMode(DWORD viewMode, DWORD validFileData);
    virtual void WINAPI GetTransferVariables(const CFileData**& transferFileData,
                                             int*& transferIsDir,
                                             char*& transferBuffer,
                                             int*& transferLen,
                                             DWORD*& transferRowData,
                                             CPluginDataInterfaceAbstract**& transferPluginDataIface,
                                             DWORD*& transferActCustomData)
    {
        transferFileData = &TransferFileData;
        transferIsDir = &TransferIsDir;
        transferBuffer = TransferBuffer;
        transferLen = &TransferLen;
        transferRowData = &TransferRowData;
        transferPluginDataIface = &TransferPluginDataIface;
        transferActCustomData = &TransferActCustomData;
    }

    virtual void WINAPI SetPluginSimpleIconCallback(FGetPluginIconIndex callback);

    // ------------- columns ---------------
    virtual int WINAPI GetColumnsCount();
    virtual const CColumn* WINAPI GetColumn(int index);
    virtual BOOL WINAPI InsertColumn(int index, const CColumn* column);
    virtual BOOL WINAPI InsertStandardColumn(int index, DWORD id);
    virtual BOOL WINAPI SetColumnName(int index, const char* name, const char* description);
    virtual BOOL WINAPI DeleteColumn(int index);
};

//
// ****************************************************************************
// CSalamanderDebug
//

struct CPluginData;

class CSalamanderDebug : public CSalamanderDebugAbstract
{
protected:
    const char* DLLName; // reference to the string from CPluginData used by this interface
    const char* Version; // reference to the string from CPluginData used by this interface

public:
    CSalamanderDebug()
    {
        DLLName = NULL;
        Version = NULL;
    }

    // must be called during SetBasicPluginData when strings change
    void Init(const char* dllName, const char* version)
    {
        DLLName = dllName;
        Version = version;
    }

    // Implementation of CSalamanderDebugAbstract methods:

    virtual void WINAPI TraceI(const char* file, int line, const char* str);
    virtual void WINAPI TraceIW(const WCHAR* file, int line, const WCHAR* str);
    virtual void WINAPI TraceE(const char* file, int line, const char* str);
    virtual void WINAPI TraceEW(const WCHAR* file, int line, const WCHAR* str);

    virtual void WINAPI TraceAttachThread(HANDLE thread, unsigned tid);

    virtual void WINAPI TraceSetThreadName(const char* name);
    virtual void WINAPI TraceSetThreadNameW(const WCHAR* name);

    virtual unsigned WINAPI CallWithCallStack(unsigned(WINAPI* threadBody)(void*), void* param);
    unsigned CallWithCallStackEH(unsigned(WINAPI* threadBody)(void*), void* param);

    virtual void WINAPI Push(const char* format, va_list args, CCallStackMsgContext* callStackMsgContext,
                             BOOL doNotMeasureTimes);
    virtual void WINAPI Pop(CCallStackMsgContext* callStackMsgContext);

    virtual void WINAPI SetThreadNameInVC(const char* name);
    virtual void WINAPI SetThreadNameInVCAndTrace(const char* name);

    virtual void WINAPI TraceConnectToServer();

    virtual void WINAPI AddModuleWithPossibleMemoryLeaks(const char* fileName);
};

//
// ****************************************************************************
// CGUIProgressBar
//

class CProgressBar;

class CGUIProgressBar : public CGUIProgressBarAbstract
{
protected:
    CProgressBar* Control; // encapsulated WinLib object

public:
    CGUIProgressBar() {}

    // helper: setting of the encapsulated WinLib object
    void SetControl(CProgressBar* control) { Control = control; }

    // Implementation of CGUIProgressBarAbstract methods:
    virtual void WINAPI SetProgress(DWORD progress, const char* text);
    virtual void WINAPI SetSelfMoveTime(DWORD time);
    virtual void WINAPI SetSelfMoveSpeed(DWORD moveTime);
    virtual void WINAPI Stop();
    virtual void WINAPI SetProgress2(const CQuadWord& progressCurrent, const CQuadWord& progressTotal,
                                     const char* text);
};

//
// ****************************************************************************
// CGUIStaticText
//

class CStaticText;

class CGUIStaticText : public CGUIStaticTextAbstract
{
protected:
    CStaticText* Control; // encapsulated WinLib object

public:
    CGUIStaticText() {}

    // helper: setting of the encapsulated WinLib object
    void SetControl(CStaticText* control) { Control = control; }

    // Implementation of CGUIStaticTextAbstract methods:
    virtual BOOL WINAPI SetText(const char* text);
    virtual const char* WINAPI GetText();
    virtual void WINAPI SetPathSeparator(char separator);
    virtual BOOL WINAPI SetToolTipText(const char* text);
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id);
};

//
// ****************************************************************************
// CGUIHyperLink
//

class CHyperLink;

class CGUIHyperLink : public CGUIHyperLinkAbstract
{
protected:
    CHyperLink* Control; // encapsulated WinLib object

public:
    CGUIHyperLink() {}

    // helper: setting of the encapsulated WinLib object
    void SetControl(CHyperLink* control) { Control = control; }

    // Implementation of CGUIHyperLinkAbstract methods:
    virtual BOOL WINAPI SetText(const char* text);
    virtual const char* WINAPI GetText();
    virtual void WINAPI SetActionOpen(const char* file);
    virtual void WINAPI SetActionPostCommand(WORD command);
    virtual BOOL WINAPI SetActionShowHint(const char* text);
    virtual BOOL WINAPI SetToolTipText(const char* text);
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id);
};

//
// ****************************************************************************
// CGUIButton
//

class CButton;

class CGUIButton : public CGUIButtonAbstract
{
protected:
    CButton* Control; // encapsulated WinLib object

public:
    CGUIButton() {}

    // helper: setting of the encapsulated WinLib object
    void SetControl(CButton* control) { Control = control; }

    // Implementation of CGUIButtonAbstract methods:
    virtual BOOL WINAPI SetToolTipText(const char* text);
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id);
};

//
// ****************************************************************************
// CGUIColorArrowButton
//

class CColorArrowButton;

class CGUIColorArrowButton : public CGUIColorArrowButtonAbstract
{
protected:
    CColorArrowButton* Control; // encapsulated WinLib object

public:
    CGUIColorArrowButton() {}

    // helper: setting of the encapsulated WinLib object
    void SetControl(CColorArrowButton* control) { Control = control; }

    // Implementation of CGUIColorArrowButtonAbstract methods:
    virtual void WINAPI SetColor(COLORREF textColor, COLORREF bkgndColor);
    virtual void WINAPI SetTextColor(COLORREF textColor);
    virtual void WINAPI SetBkgndColor(COLORREF bkgndColor);
    virtual COLORREF WINAPI GetTextColor();
    virtual COLORREF WINAPI GetBkgndColor();
};

//
// ****************************************************************************
// CGUIToolbarHeader
//

class CToolbarHeader;

class CGUIToolbarHeader : public CGUIToolbarHeaderAbstract
{
protected:
    CToolbarHeader* Control; // encapsulated WinLib object

public:
    CGUIToolbarHeader() {}

    // helper: setting of the encapsulated WinLib object
    void SetControl(CToolbarHeader* control) { Control = control; }

    // Implementation of CGUIColorArrowButtonAbstract methods:
    virtual void WINAPI EnableToolbar(DWORD enableMask);
    virtual void WINAPI CheckToolbar(DWORD checkMask);
    virtual void WINAPI SetNotifyWindow(HWND hWnd);
};

//
// ****************************************************************************
// CSalamanderGUI
//

class CSalamanderGUI : public CSalamanderGUIAbstract
{
public:
    CSalamanderGUI() {}

    // Implementation of CSalamanderGUIAbstract methods:
    virtual CGUIProgressBarAbstract* WINAPI AttachProgressBar(HWND hParent, int ctrlID);
    virtual CGUIStaticTextAbstract* WINAPI AttachStaticText(HWND hParent, int ctrlID, DWORD flags);
    virtual CGUIHyperLinkAbstract* WINAPI AttachHyperLink(HWND hParent, int ctrlID, DWORD flags);
    virtual CGUIButtonAbstract* WINAPI AttachButton(HWND hParent, int ctrlID, DWORD flags);
    virtual CGUIColorArrowButtonAbstract* WINAPI AttachColorArrowButton(HWND hParent, int ctrlID, BOOL showArrow);
    virtual BOOL WINAPI ChangeToArrowButton(HWND hParent, int ctrlID);
    virtual CGUIMenuPopupAbstract* WINAPI CreateMenuPopup();
    virtual BOOL WINAPI DestroyMenuPopup(CGUIMenuPopupAbstract* popup);
    virtual CGUIMenuBarAbstract* WINAPI CreateMenuBar(CGUIMenuPopupAbstract* menu, HWND hNotifyWindow);
    virtual BOOL WINAPI DestroyMenuBar(CGUIMenuBarAbstract* menuBar);
    virtual BOOL WINAPI CreateGrayscaleAndMaskBitmaps(HBITMAP hSource, COLORREF transparent,
                                                      HBITMAP& hGrayscale, HBITMAP& hMask);
    virtual CGUIToolBarAbstract* WINAPI CreateToolBar(HWND hNotifyWindow);
    virtual BOOL WINAPI DestroyToolBar(CGUIToolBarAbstract* toolBar);
    virtual void WINAPI SetCurrentToolTip(HWND hNotifyWindow, DWORD id);
    virtual void WINAPI SuppressToolTipOnCurrentMousePos();
    virtual BOOL WINAPI DisableWindowVisualStyles(HWND hWindow);
    virtual CGUIIconListAbstract* WINAPI CreateIconList();
    virtual BOOL WINAPI DestroyIconList(CGUIIconListAbstract* iconList);
    virtual void WINAPI PrepareToolTipText(char* buf, BOOL stripHotKey);
    virtual void WINAPI SetSubjectTruncatedText(HWND subjectWnd, const char* subjectFormatString,
                                                const char* fileName, BOOL isDir, BOOL duplicateAmpersands);
    virtual CGUIToolbarHeaderAbstract* WINAPI AttachToolbarHeader(HWND hParent, int ctrlID, HWND hAlignWindow, DWORD buttonMask);
    virtual void WINAPI ArrangeHorizontalLines(HWND hWindow);
    virtual int WINAPI GetWindowFontHeight(HWND hWindow);

protected:
    // helper function: checks whether 'control' was successfully allocated and attached;
    // if not, it calls the pointer's delete function and returns FALSE
    BOOL CheckControlAndDeleteOnError(CWindow* control);
};

class CSalamanderPasswordManager : public CSalamanderPasswordManagerAbstract
{
private:
    const char* DLLName; // reference to the string from the CPluginData plugin used by this interface

public:
    CSalamanderPasswordManager()
    {
        DLLName = NULL;
    }

    // must be called during SetBasicPluginData when strings change
    void Init(const char* dllName)
    {
        DLLName = dllName;
    }

    // Implementation of CSalamanderPasswordManagerAbstract methods:
    virtual BOOL WINAPI IsUsingMasterPassword();
    virtual BOOL WINAPI IsMasterPasswordSet();
    virtual BOOL WINAPI AskForMasterPassword(HWND hParent);

    virtual BOOL WINAPI EncryptPassword(const char* plainPassword, BYTE** encryptedPassword, int* encryptedPasswordSize, BOOL encrypt);
    virtual BOOL WINAPI DecryptPassword(const BYTE* encryptedPassword, int encryptedPasswordSize, char** plainPassword);
    virtual BOOL WINAPI IsPasswordEncrypted(const BYTE* encyptedPassword, int encyptedPasswordSize);
};

//
// ****************************************************************************
// CSalamanderGeneral
//

class CSalamanderGeneral : public CSalamanderGeneralAbstract
{
protected:
    CPluginInterfaceAbstract* Plugin; // plug-in used by this iface; !!! note: may be NULL
                                      // (plugin not loaded) or -1 (during the plugin's entry point)

    char HelpFileName[MAX_PATH]; // if not empty, this is the name (without path) of the .chm help file used by this plugin (optimization only, not stored anywhere)

public:
    HINSTANCE LanguageModule; // if not NULL, it is the handle to the plugin's .SLG language module

public:
    CSalamanderGeneral();
    ~CSalamanderGeneral();

    // must be called immediately after the plugin's entry point
    void Init(CPluginInterfaceAbstract* plugin) { Plugin = plugin; }

    // called after the plugin unloads - prepares data for the next plugin load
    void Clear();

    // helper non-virtual function to obtain a pointer to the panel according to PATH_TYPE_XXX
    CFilesWindow* WINAPI GetPanel(int panel);

    // Implementation of CSalamanderGeneralAbstract methods:

    virtual int WINAPI ShowMessageBox(const char* text, const char* title, int type);
    virtual int WINAPI SalMessageBox(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);
    virtual int WINAPI SalMessageBoxEx(const MSGBOXEX_PARAMS* params);

    virtual HWND WINAPI GetMsgBoxParent();
    virtual HWND WINAPI GetMainWindowHWND();
    virtual void WINAPI RestoreFocusInSourcePanel();

    virtual int WINAPI DialogError(HWND parent, DWORD flags, const char* fileName, const char* error, const char* title);
    virtual int WINAPI DialogOverwrite(HWND parent, DWORD flags, const char* fileName1, const char* fileData1,
                                       const char* fileName2, const char* fileData2);
    virtual int WINAPI DialogQuestion(HWND parent, DWORD flags, const char* fileName, const char* question, const char* title);

    virtual BOOL WINAPI CheckAndCreateDirectory(const char* dir, HWND parent = NULL, BOOL quiet = TRUE,
                                                char* errBuf = NULL, int errBufSize = 0,
                                                char* firstCreatedDir = NULL, BOOL manualCrDir = FALSE);
    virtual BOOL WINAPI TestFreeSpace(HWND parent, const char* path, const CQuadWord& totalSize,
                                      const char* messageTitle);

    virtual void WINAPI GetDiskFreeSpace(CQuadWord* retValue, const char* path, CQuadWord* total);
    virtual BOOL WINAPI SalGetDiskFreeSpace(const char* path, LPDWORD lpSectorsPerCluster,
                                            LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                                            LPDWORD lpTotalNumberOfClusters);
    virtual BOOL WINAPI SalGetVolumeInformation(const char* path, char* rootOrCurReparsePoint, LPTSTR lpVolumeNameBuffer,
                                                DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                                                LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                                                LPTSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);
    virtual UINT WINAPI SalGetDriveType(const char* path);

    virtual BOOL WINAPI SalGetTempFileName(const char* path, const char* prefix, char* tmpName, BOOL file, DWORD* err);
    virtual void WINAPI RemoveTemporaryDir(const char* dir);
    virtual BOOL WINAPI SalMoveFile(const char* srcName, const char* destName, DWORD* err);
    virtual BOOL WINAPI SalGetFileSize(HANDLE file, CQuadWord& size, DWORD& err);
    virtual void WINAPI ExecuteAssociation(HWND parent, const char* path, const char* name);
    virtual BOOL WINAPI GetTargetDirectory(HWND parent, HWND hCenterWindow, const char* title,
                                           const char* comment, char* path, BOOL onlyNet,
                                           const char* initDir);

    virtual void WINAPI PrepareMask(char* mask, const char* src);
    virtual BOOL WINAPI AgreeMask(const char* filename, const char* mask, BOOL hasExtension);
    virtual char* WINAPI MaskName(char* buffer, int bufSize, const char* name, const char* mask);
    virtual void WINAPI PrepareExtMask(char* mask, const char* src);
    virtual BOOL WINAPI AgreeExtMask(const char* filename, const char* mask, BOOL hasExtension);
    virtual CSalamanderMaskGroup* WINAPI AllocSalamanderMaskGroup();
    virtual void WINAPI FreeSalamanderMaskGroup(CSalamanderMaskGroup* maskGroup);

    virtual void* WINAPI Alloc(int size);
    virtual void WINAPI Free(void* ptr);
    virtual char* WINAPI DupStr(const char* str);

    virtual void WINAPI GetLowerAndUpperCase(unsigned char** lowerCase, unsigned char** upperCase);
    virtual void WINAPI ToLowerCase(char* str);
    virtual void WINAPI ToUpperCase(char* str);

    virtual int WINAPI StrCmpEx(const char* s1, int l1, const char* s2, int l2);
    virtual int WINAPI StrICpy(char* dest, const char* src);
    virtual int WINAPI StrICmp(const char* s1, const char* s2);
    virtual int WINAPI StrICmpEx(const char* s1, int l1, const char* s2, int l2);
    virtual int WINAPI StrNICmp(const char* s1, const char* s2, int n);
    virtual int WINAPI MemICmp(const void* buf1, const void* buf2, int n);

    virtual int WINAPI RegSetStrICmp(const char* s1, const char* s2);
    virtual int WINAPI RegSetStrICmpEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual);
    virtual int WINAPI RegSetStrCmp(const char* s1, const char* s2);
    virtual int WINAPI RegSetStrCmpEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual);

    virtual BOOL WINAPI GetPanelPath(int panel, char* buffer, int bufferSize, int* type,
                                     char** archiveOrFS, BOOL convertFSPathToExternal);
    virtual BOOL WINAPI GetLastWindowsPanelPath(int panel, char* buffer, int bufferSize);
    virtual void WINAPI GetPluginFSName(char* buf, int fsNameIndex);
    virtual CPluginFSInterfaceAbstract* WINAPI GetPanelPluginFS(int panel);
    virtual CPluginDataInterfaceAbstract* WINAPI GetPanelPluginData(int panel);
    virtual const CFileData* WINAPI GetPanelFocusedItem(int panel, BOOL* isDir);
    virtual const CFileData* WINAPI GetPanelItem(int panel, int* index, BOOL* isDir);
    virtual const CFileData* WINAPI GetPanelSelectedItem(int panel, int* index, BOOL* isDir);
    virtual BOOL WINAPI GetPanelSelection(int panel, int* selectedFiles, int* selectedDirs);
    virtual int WINAPI GetPanelTopIndex(int panel);

    virtual void WINAPI SkipOneActivateRefresh();

    virtual void WINAPI SelectPanelItem(int panel, const CFileData* file, BOOL select);
    virtual void WINAPI RepaintChangedItems(int panel);
    virtual void WINAPI SelectAllPanelItems(int panel, BOOL select, BOOL repaint);
    virtual void WINAPI SetPanelFocusedItem(int panel, const CFileData* file, BOOL partVis);

    virtual BOOL WINAPI GetFilterFromPanel(int panel, char* masks, int masksBufSize);

    virtual int WINAPI GetSourcePanel();
    virtual BOOL WINAPI GetPanelWithPluginFS(CPluginFSInterfaceAbstract* pluginFS, int& panel);
    virtual void WINAPI ChangePanel();

    virtual char* WINAPI NumberToStr(char* buffer, const CQuadWord& number);
    virtual char* WINAPI PrintDiskSize(char* buf, const CQuadWord& size, int mode);
    virtual char* WINAPI PrintTimeLeft(char* buf, const CQuadWord& secs);

    virtual BOOL WINAPI HasTheSameRootPath(const char* path1, const char* path2);
    virtual int WINAPI CommonPrefixLength(const char* path1, const char* path2);
    virtual BOOL WINAPI PathIsPrefix(const char* prefix, const char* path);
    virtual BOOL WINAPI IsTheSamePath(const char* path1, const char* path2);
    virtual int WINAPI GetRootPath(char* root, const char* path);
    virtual BOOL WINAPI CutDirectory(char* path, char** cutDir = NULL);
    virtual BOOL WINAPI SalPathAppend(char* path, const char* name, int pathSize);
    virtual BOOL WINAPI SalPathAddBackslash(char* path, int pathSize);
    virtual void WINAPI SalPathRemoveBackslash(char* path);
    virtual void WINAPI SalPathStripPath(char* path);
    virtual void WINAPI SalPathRemoveExtension(char* path);
    virtual BOOL WINAPI SalPathAddExtension(char* path, const char* extension, int pathSize);
    virtual BOOL WINAPI SalPathRenameExtension(char* path, const char* extension, int pathSize);
    virtual const char* WINAPI SalPathFindFileName(const char* path);

    virtual BOOL WINAPI SalGetFullName(char* name, int* errTextID = NULL, const char* curDir = NULL,
                                       char* nextFocus = NULL, int nameBufSize = MAX_PATH);
    virtual void WINAPI SalUpdateDefaultDir(BOOL activePrefered);
    virtual char* WINAPI GetGFNErrorText(int GFN, char* buf, int bufSize);

    virtual char* WINAPI GetErrorText(int err, char* buf, int bufSize);

    virtual COLORREF WINAPI GetCurrentColor(int color);

    virtual void WINAPI FocusNameInPanel(int panel, const char* path, const char* name);
    virtual BOOL WINAPI ChangePanelPath(int panel, const char* path, int* failReason = NULL,
                                        int suggestedTopIndex = -1,
                                        const char* suggestedFocusName = NULL,
                                        BOOL convertFSPathToInternal = TRUE);
    virtual BOOL WINAPI ChangePanelPathToDisk(int panel, const char* path, int* failReason = NULL,
                                              int suggestedTopIndex = -1,
                                              const char* suggestedFocusName = NULL);
    virtual BOOL WINAPI ChangePanelPathToArchive(int panel, const char* archive, const char* archivePath,
                                                 int* failReason = NULL, int suggestedTopIndex = -1,
                                                 const char* suggestedFocusName = NULL,
                                                 BOOL forceUpdate = FALSE);
    virtual BOOL WINAPI ChangePanelPathToPluginFS(int panel, const char* fsName, const char* fsUserPart,
                                                  int* failReason = NULL, int suggestedTopIndex = -1,
                                                  const char* suggestedFocusName = NULL,
                                                  BOOL forceUpdate = FALSE, BOOL convertPathToInternal = FALSE);
    virtual BOOL WINAPI ChangePanelPathToDetachedFS(int panel, CPluginFSInterfaceAbstract* detachedFS,
                                                    int* failReason = NULL, int suggestedTopIndex = -1,
                                                    const char* suggestedFocusName = NULL);
    virtual BOOL WINAPI ChangePanelPathToFixedDrive(int panel, int* failReason = NULL);

    virtual void WINAPI RefreshPanelPath(int panel, BOOL forceRefresh = FALSE,
                                         BOOL focusFirstNewItem = FALSE);
    virtual void WINAPI PostRefreshPanelPath(int panel, BOOL focusFirstNewItem = FALSE);
    virtual void WINAPI PostRefreshPanelFS(CPluginFSInterfaceAbstract* modifiedFS,
                                           BOOL focusFirstNewItem = FALSE);

    virtual BOOL WINAPI CloseDetachedFS(HWND parent, CPluginFSInterfaceAbstract* detachedFS);

    virtual BOOL WINAPI DuplicateAmpersands(char* buffer, int bufferSize);
    virtual void WINAPI RemoveAmpersands(char* text);

    virtual BOOL WINAPI ValidateVarString(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2,
                                          const CSalamanderVarStrEntry* variables);
    virtual BOOL WINAPI ExpandVarString(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                                        const CSalamanderVarStrEntry* variables, void* param,
                                        BOOL ignoreEnvVarNotFoundOrTooLong = FALSE,
                                        DWORD* varPlacements = NULL, int* varPlacementsCount = NULL,
                                        BOOL detectMaxVarWidths = FALSE, int* maxVarWidths = NULL,
                                        int maxVarWidthsCount = 0);

    virtual BOOL WINAPI SetFlagLoadOnSalamanderStart(BOOL start);

    virtual void WINAPI PostUnloadThisPlugin();

    virtual BOOL WINAPI EnumInstalledModules(int* index, char* module, char* version);

    virtual void WINAPI CallLoadOrSaveConfiguration(BOOL load, FSalLoadOrSaveConfiguration loadOrSaveFunc,
                                                    void* param);

    virtual BOOL WINAPI CopyTextToClipboard(const char* text, int textLen, BOOL showEcho, HWND echoParent);
    virtual BOOL WINAPI CopyTextToClipboardW(const wchar_t* text, int textLen, BOOL showEcho, HWND echoParent);

    virtual void WINAPI PostMenuExtCommand(int id, BOOL waitForSalIdle);

    virtual BOOL WINAPI SalamanderIsNotBusy(DWORD* lastIdleTime);

    virtual void WINAPI SetPluginBugReportInfo(const char* message, const char* email);

    virtual BOOL WINAPI IsPluginInstalled(const char* pluginSPL);

    virtual BOOL WINAPI ViewFileInPluginViewer(const char* pluginSPL,
                                               CSalamanderPluginViewerData* pluginData,
                                               BOOL useCache, const char* rootTmpPath,
                                               const char* fileNameInCache, int& error);

    virtual void WINAPI PostChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    virtual DWORD WINAPI SalCheckPath(BOOL echo, const char* path, DWORD err, HWND parent);
    virtual BOOL WINAPI SalCheckAndRestorePath(HWND parent, const char* path, BOOL tryNet);
    virtual BOOL WINAPI SalCheckAndRestorePathWithCut(HWND parent, char* path, BOOL& tryNet, DWORD& err,
                                                      DWORD& lastErr, BOOL& pathInvalid, BOOL& cut,
                                                      BOOL donotReconnect);
    virtual BOOL WINAPI SalParsePath(HWND parent, char* path, int& type, BOOL& isDir, char*& secondPart,
                                     const char* errorTitle, char* nextFocus, BOOL curPathIsDiskOrArchive,
                                     const char* curPath, const char* curArchivePath, int* error,
                                     int pathBufSize);
    virtual BOOL WINAPI SalSplitWindowsPath(HWND parent, const char* title, const char* errorTitle, int selCount,
                                            char* path, char* secondPart, BOOL pathIsDir, BOOL backslashAtEnd,
                                            const char* dirName, const char* curDiskPath, char*& mask);
    virtual BOOL WINAPI SalSplitGeneralPath(HWND parent, const char* title, const char* errorTitle,
                                            int selCount, char* path, char* afterRoot, char* secondPart,
                                            BOOL pathIsDir, BOOL backslashAtEnd, const char* dirName,
                                            const char* curPath, char*& mask, char* newDirs,
                                            SGP_IsTheSamePathF isTheSamePathF);
    virtual BOOL WINAPI SalRemovePointsFromPath(char* afterRoot);

    virtual BOOL WINAPI GetConfigParameter(int paramID, void* buffer, int bufferSize, int* type);

    virtual void WINAPI AlterFileName(char* tgtName, char* srcName, int format, int changedParts,
                                      BOOL isDir);

    virtual void WINAPI CreateSafeWaitWindow(const char* message, const char* caption,
                                             int delay, BOOL showCloseButton, HWND hForegroundWnd);
    virtual void WINAPI DestroySafeWaitWindow();
    virtual void WINAPI ShowSafeWaitWindow(BOOL show);
    virtual BOOL WINAPI GetSafeWaitWindowClosePressed();
    virtual void WINAPI SetSafeWaitWindowText(const char* message);

    virtual BOOL WINAPI GetFileFromCache(const char* uniqueFileName, const char*& tmpName,
                                         HANDLE fileLock);
    virtual void WINAPI UnlockFileInCache(HANDLE fileLock);
    virtual BOOL WINAPI MoveFileToCache(const char* uniqueFileName, const char* nameInCache,
                                        const char* rootTmpPath, const char* newFileName,
                                        const CQuadWord& newFileSize, BOOL* alreadyExists);
    virtual void WINAPI RemoveOneFileFromCache(const char* uniqueFileName);
    virtual void WINAPI RemoveFilesFromCache(const char* fileNamesRoot);

    virtual BOOL WINAPI EnumConversionTables(HWND parent, int* index, const char** name, const char** table);
    virtual BOOL WINAPI GetConversionTable(HWND parent, char* table, const char* conversion);
    virtual void WINAPI GetWindowsCodePage(HWND parent, char* codePage);
    virtual void WINAPI RecognizeFileType(HWND parent, const char* pattern, int patternLen, BOOL forceText,
                                          BOOL* isText, char* codePage);
    virtual BOOL WINAPI IsANSIText(const char* text, int textLen);

    virtual void WINAPI CallPluginOperationFromDisk(int panel, SalPluginOperationFromDisk callback,
                                                    void* param);

    virtual BYTE WINAPI GetUserDefaultCharset();

    virtual CSalamanderBMSearchData* WINAPI AllocSalamanderBMSearchData();
    virtual void WINAPI FreeSalamanderBMSearchData(CSalamanderBMSearchData* data);
    virtual CSalamanderREGEXPSearchData* WINAPI AllocSalamanderREGEXPSearchData();
    virtual void WINAPI FreeSalamanderREGEXPSearchData(CSalamanderREGEXPSearchData* data);

    virtual BOOL WINAPI EnumSalamanderCommands(int* index, int* salCmd, char* nameBuf, int nameBufSize,
                                               BOOL* enabled, int* type);
    virtual BOOL WINAPI GetSalamanderCommand(int salCmd, char* nameBuf, int nameBufSize, BOOL* enabled,
                                             int* type);
    virtual void WINAPI PostSalamanderCommand(int salCmd);

    virtual void WINAPI SetUserWorkedOnPanelPath(int panel);
    virtual void WINAPI StoreSelectionOnPanelPath(int panel);
    virtual DWORD WINAPI UpdateCrc32(const void* buffer, DWORD count, DWORD crcVal);
    virtual CSalamanderMD5* WINAPI AllocSalamanderMD5();
    virtual void WINAPI FreeSalamanderMD5(CSalamanderMD5* md5);
    virtual BOOL WINAPI LookForSubTexts(char* text, DWORD* varPlacements, int* varPlacementsCount);
    virtual void WINAPI WaitForESCRelease();

    virtual DWORD WINAPI GetMouseWheelScrollLines();
    virtual DWORD WINAPI GetMouseWheelScrollChars();

    virtual HWND WINAPI GetTopVisibleParent(HWND hParent);
    virtual BOOL WINAPI MultiMonGetDefaultWindowPos(HWND hByWnd, POINT* p);
    virtual void WINAPI MultiMonGetClipRectByRect(const RECT* rect, RECT* workClipRect, RECT* monitorClipRect);
    virtual void WINAPI MultiMonGetClipRectByWindow(HWND hByWnd, RECT* workClipRect, RECT* monitorClipRect);
    virtual void WINAPI MultiMonCenterWindow(HWND hWindow, HWND hByWnd, BOOL findTopWindow);
    virtual BOOL WINAPI MultiMonEnsureRectVisible(RECT* rect, BOOL partialOK);

    virtual BOOL WINAPI InstallWordBreakProc(HWND hWindow);

    virtual BOOL WINAPI IsFirstInstance3OrLater();

    virtual int WINAPI ExpandPluralString(char* buffer, int bufferSize, const char* format,
                                          int parametersCount, const CQuadWord* parametersArray);
    virtual int WINAPI ExpandPluralFilesDirs(char* buffer, int bufferSize, int files, int dirs,
                                             int mode, BOOL forDlgCaption);
    virtual int WINAPI ExpandPluralBytesFilesDirs(char* buffer, int bufferSize,
                                                  const CQuadWord& selectedBytes, int files, int dirs,
                                                  BOOL useSubTexts);

    virtual void WINAPI GetCommonFSOperSourceDescr(char* sourceDescr, int sourceDescrSize,
                                                   int panel, int selectedFiles, int selectedDirs,
                                                   const char* fileOrDirName, BOOL isDir,
                                                   BOOL forDlgCaption);

    virtual void WINAPI AddStrToStr(char* dstStr, int dstBufSize, const char* srcStr);

    virtual BOOL WINAPI SalIsValidFileNameComponent(const char* fileNameComponent);
    virtual void WINAPI SalMakeValidFileNameComponent(char* fileNameComponent);

    virtual BOOL WINAPI IsFileEnumSourcePanel(int srcUID, int* panel);
    virtual BOOL WINAPI GetNextFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                                 BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                                 char* fileName, BOOL* noMoreFiles, BOOL* srcBusy);
    virtual BOOL WINAPI GetPreviousFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                                     BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                                     char* fileName, BOOL* noMoreFiles, BOOL* srcBusy);
    virtual BOOL WINAPI IsFileNameForViewerSelected(int srcUID, int lastFileIndex, const char* lastFileName,
                                                    BOOL* isFileSelected, BOOL* srcBusy);
    virtual BOOL WINAPI SetSelectionOnFileNameForViewer(int srcUID, int lastFileIndex,
                                                        const char* lastFileName, BOOL select,
                                                        BOOL* srcBusy);

    virtual BOOL WINAPI GetStdHistoryValues(int historyID, char*** historyArr, int* historyItemsCount);
    virtual void WINAPI AddValueToStdHistoryValues(char** historyArr, int historyItemsCount,
                                                   const char* value, BOOL caseSensitiveValue);
    virtual void WINAPI LoadComboFromStdHistoryValues(HWND combo, char** historyArr, int historyItemsCount);

    virtual BOOL WINAPI CanUse256ColorsBitmap();

    virtual HWND WINAPI GetWndToFlash(HWND parent);

    virtual void WINAPI ActivateDropTarget(HWND dropTarget, HWND progressWnd);

    virtual void WINAPI PostOpenPackDlgForThisPlugin(int delFilesAfterPacking);
    virtual void WINAPI PostOpenUnpackDlgForThisPlugin(const char* unpackMask);

    virtual HANDLE WINAPI SalCreateFileEx(const char* fileName, DWORD desiredAccess, DWORD shareMode,
                                          DWORD flagsAndAttributes, DWORD* err);
    virtual BOOL WINAPI SalCreateDirectoryEx(const char* name, DWORD* err);

    virtual void WINAPI PanelStopMonitoring(int panel, BOOL stopMonitoring);

    virtual CSalamanderDirectoryAbstract* WINAPI AllocSalamanderDirectory(BOOL isForFS);
    virtual void WINAPI FreeSalamanderDirectory(CSalamanderDirectoryAbstract* salDir);

    virtual BOOL WINAPI AddPluginFSTimer(int timeout, CPluginFSInterfaceAbstract* timerOwner,
                                         DWORD timerParam);
    virtual int WINAPI KillPluginFSTimer(CPluginFSInterfaceAbstract* timerOwner, BOOL allTimers,
                                         DWORD timerParam);

    virtual BOOL WINAPI GetChangeDriveMenuItemVisibility();
    virtual void WINAPI SetChangeDriveMenuItemVisibility(BOOL visible);

    virtual void WINAPI OleSpySetBreak(int alloc);

    virtual HICON WINAPI GetSalamanderIcon(int icon, int iconSize);

    virtual BOOL WINAPI GetFileIcon(const char* path, BOOL pathIsPIDL, HICON* hIcon, int iconSize,
                                    BOOL fallbackToDefIcon, BOOL defIconIsDir);

    virtual BOOL WINAPI FileExists(const char* fileName);

    virtual void WINAPI DisconnectFSFromPanel(HWND parent, int panel);

    virtual BOOL WINAPI IsArchiveHandledByThisPlugin(const char* name);

    virtual DWORD WINAPI GetIconLRFlags();

    virtual int WINAPI IsFileLink(const char* fileExtension);

    virtual DWORD WINAPI GetImageListColorFlags();

    virtual BOOL WINAPI SafeGetOpenFileName(LPOPENFILENAME lpofn);
    virtual BOOL WINAPI SafeGetSaveFileName(LPOPENFILENAME lpofn);

    virtual void WINAPI SetHelpFileName(const char* chmName);
    virtual BOOL WINAPI OpenHtmlHelp(HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData, BOOL quiet);

    virtual BOOL WINAPI PathsAreOnTheSameVolume(const char* path1, const char* path2,
                                                BOOL* resIsOnlyEstimation);

    virtual void* WINAPI Realloc(void* ptr, int size);

    virtual void WINAPI GetPanelEnumFilesParams(int panel, int* enumFilesSourceUID,
                                                int* enumFilesCurrentIndex);

    virtual BOOL WINAPI PostRefreshPanelFS2(CPluginFSInterfaceAbstract* modifiedFS,
                                            BOOL focusFirstNewItem = FALSE);

    virtual char* WINAPI LoadStr(HINSTANCE module, int resID);
    virtual WCHAR* WINAPI LoadStrW(HINSTANCE module, int resID);

    virtual BOOL WINAPI ChangePanelPathToRescuePathOrFixedDrive(int panel, int* failReason = NULL);

    virtual void WINAPI SetPluginIsNethood();

    virtual void WINAPI OpenNetworkContextMenu(HWND parent, int panel, BOOL forItems, int menuX,
                                               int menuY, const char* netPath, char* newlyMappedDrive);

    virtual BOOL WINAPI DuplicateBackslashes(char* buffer, int bufferSize);

    virtual int WINAPI StartThrobber(int panel, const char* tooltip, int delay);
    virtual BOOL WINAPI StopThrobber(int id);

    virtual void WINAPI ShowSecurityIcon(int panel, BOOL showIcon, BOOL isLocked,
                                         const char* tooltip);

    virtual void WINAPI RemoveCurrentPathFromHistory(int panel);

    virtual BOOL WINAPI IsUserAdmin();

    virtual BOOL WINAPI IsRemoteSession();

    virtual DWORD WINAPI SalWNetAddConnection2Interactive(LPNETRESOURCE lpNetResource);

    virtual CSalamanderZLIBAbstract* WINAPI GetSalamanderZLIB();

    virtual CSalamanderPNGAbstract* WINAPI GetSalamanderPNG();

    virtual CSalamanderCryptAbstract* WINAPI GetSalamanderCrypt();

    virtual void WINAPI SetPluginUsesPasswordManager();

    virtual CSalamanderPasswordManagerAbstract* WINAPI GetSalamanderPasswordManager();

    virtual BOOL WINAPI OpenHtmlHelpForSalamander(HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData, BOOL quiet);

    virtual CSalamanderBZIP2Abstract* WINAPI GetSalamanderBZIP2();

    virtual void WINAPI GetFocusedItemMenuPos(POINT* pos);

    virtual void WINAPI LockMainWindow(BOOL lock, HWND hToolWnd, const char* lockReason);

    virtual void WINAPI PostPluginMenuChanged();

    virtual BOOL WINAPI GetMenuItemHotKey(int id, WORD* hotKey, char* hotKeyText, int hotKeyTextSize);

    virtual LONG WINAPI SalRegQueryValue(HKEY hKey, LPCSTR lpSubKey, LPSTR lpData, PLONG lpcbData);
    virtual LONG WINAPI SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                                           LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);

    virtual DWORD WINAPI SalGetFileAttributes(const char* fileName);

    virtual BOOL WINAPI IsPathOnSSD(const char* path);

    virtual BOOL WINAPI IsUNCPath(const char* path);

    virtual BOOL WINAPI ResolveSubsts(char* resPath);

    virtual void WINAPI ResolveLocalPathWithReparsePoints(char* resPath, const char* path,
                                                          BOOL* cutResPathIsPossible,
                                                          BOOL* rootOrCurReparsePointSet,
                                                          char* rootOrCurReparsePoint,
                                                          char* junctionOrSymlinkTgt, int* linkType,
                                                          char* netPath);

    virtual BOOL WINAPI GetResolvedPathMountPointAndGUID(const char* path, char* mountPoint, char* guidPath);

    virtual BOOL WINAPI PointToLocalDecimalSeparator(char* buffer, int bufferSize);

    virtual void WINAPI SetPluginIconOverlays(int iconOverlaysCount, HICON* iconOverlays);

    virtual BOOL WINAPI SalGetFileSize2(const char* fileName, CQuadWord& size, DWORD* err);

    virtual BOOL WINAPI GetLinkTgtFileSize(HWND parent, const char* fileName, CQuadWord* size,
                                           BOOL* cancel, BOOL* ignoreAll);

    virtual BOOL WINAPI DeleteDirLink(const char* name, DWORD* err);

    virtual BOOL WINAPI ClearReadOnlyAttr(const char* name, DWORD attr = -1);

    virtual BOOL WINAPI IsCriticalShutdown();

    virtual void WINAPI CloseAllOwnedEnabledDialogs(HWND parent, DWORD tid = 0);
};

//
// ****************************************************************************
// CPlugins
//

enum CPluginMenuItemType
{
    pmitItemOrSeparator, // item or separator (Name is missing or it is NULL)
    pmitStartSubmenu,    // start of a submenu (an item that allows opening a submenu containing all menu items from the array up to the submenu end marker)
    pmitEndSubmenu,      // end-of-submenu marker (used only while building the menu, other data in CPluginMenuItem are blank)
};

// If the HotKey has this bit set, the user has changed this key in the configuration and
// we must preserve it during Plugin::Connect()/AddMenuItems
#define HOTKEY_DIRTY 0x00010000
#define HOTKEY_HINT 0x00020000 // 'Name' contains, after the '\t' character, a hotkey that will be displayed in the menu if the user does not assign a custom key to the command
#define HOTKEY_MASK 0x0000FFFF
#define HOTKEY_GET(x) (WORD)(x & HOTKEY_MASK)
#define HOTKEY_GETDIRTY(x) ((x & HOTKEY_DIRTY) != 0)

struct CPluginMenuItem
{
public:
    CPluginMenuItemType Type; // item type, see description in CPluginMenuItemType
    int IconIndex;            // icon index in the plugin icon bitmap; -1 = no icon; WARNING: index is unchecked (may be invalid)
    char* Name;               // name of the menu item; if NULL, it represents a separator
    DWORD StateMask;          // hiword is an OR mask, loword is an AND mask; if it is -1,
                              // CPluginInterfaceAbstract::GetMenuItemState is used
    DWORD SkillLevel;         // which user levels should see this item MENU_SKILLLEVEL_XXX
    int ID;                   // plug-in UID - unique item number within the plugin
    DWORD HotKey;             // hot key: LOWORD=hotkey(LOBYTE:vk, HIBYTE:mods), HIWORD=(0:user kept it, 1:user changed it,it is dirty)

    // helper data:
    int SUID; // Salamander-UID - unique number within Salamander, computed when filling the menu

public:
    CPluginMenuItem(int iconIndex, const char* name, DWORD hotKey, DWORD stateMask, int id, DWORD skillLevel,
                    CPluginMenuItemType type);
    ~CPluginMenuItem()
    {
        if (Name != NULL)
            free(Name);
    }
};

class CSalamanderDirectory;
class CMenuPopup;
class CToolBar;

struct CPluginData
{
public:
    int BuiltForVersion;       // valid only when the plugin is loaded: Salamander version the plugin was compiled for (see the list of versions under LAST_VERSION_OF_SALAMANDER in spl_vers.h)
    char* Name;                // plugin name shown in Extensions/C.Packers/C.Unpackers dialogs
                               // max. length of the name MAX_PATH - 1
    char* DLLName;             // DLL file name, relative to "plugins" or absolute
                               // max. length of the name MAX_PATH - 1
    BOOL SupportPanelView;     // TRUE => supports ListArchive, UnpackArchive, UnpackOneFile (panel archiver/view)
    BOOL SupportPanelEdit;     // TRUE => supports PackToArchive, DeleteFromArchive (panel archiver/edit)
    BOOL SupportCustomPack;    // TRUE => supports PackToArchive (custom archiver/pack)
    BOOL SupportCustomUnpack;  // TRUE => supports UnpackWholeArchive (custom archiver/unpack)
    BOOL SupportConfiguration; // TRUE => supports Configuration (can be configured)
    BOOL SupportLoadSave;      // TRUE => supports registry load/save (persistence)
    BOOL SupportViewer;        // TRUE => supports ViewFile and CanViewFile (file viewer)
    BOOL SupportFS;            // TRUE => supports a file system
    BOOL SupportDynMenuExt;    // TRUE => menu is added in PluginIfaceForMenuExt::BuildMenu instead of PluginIface::Connect (menu is dynamic and rebuilt before each plugin menu open)

    BOOL LoadOnStart; // should the plugin load at every Salamander start?

    char* Version;                // plugin version (max length MAX_PATH - 1)
    char* Copyright;              // manufacturer's copyright (max length MAX_PATH - 1)
    char* Description;            // short plugin description (max length MAX_PATH - 1)
    char* RegKeyName;             // registry key name for configuration (max length MAX_PATH - 1)
    char* Extensions;             // archive extensions separated by ';' (max length MAX_PATH - 1)
    TIndirectArray<char> FSNames; // array of plugin filesystem names (each max length MAX_PATH - 1)

    char* LastSLGName; // name of the last used .SLG file (NULL = none yet or same language as Salamander)

    char* PluginHomePageURL; // URL of the plugin home page (NULL == no home page exists)

    char* ChDrvMenuFSItemName;    // filesystem command in the change-drive menu: name (max length MAX_PATH - 1)
    int ChDrvMenuFSItemIconIndex; // filesystem command in the change-drive menu: icon index (in PluginIcons; -1=no icon; the index is not checked – it may be invalid)
    BOOL ChDrvMenuFSItemVisible;  // filesystem command in the change-drive menu: is it visible? (users can hide it from Plugins Manager)

    TIndirectArray<CPluginMenuItem> MenuItems; // array of items in the menu
    BOOL DynMenuWasAlreadyBuild;               // TRUE if BuildMenu() was already called; further calls are ignored

    char* BugReportMessage; // message to be displayed by the Bug Report dialog when a plugin exception occurs
    char* BugReportEMail;   // e-mail to be displayed by the Bug Report dialog when a plugin exception occurs

    CMaskGroup ThumbnailMasks;   // masks determining files for which the plugin can create thumbnails (max length MAX_GROUPMASK - 1); NULL == the plugin does not generate any thumbnails
    BOOL ThumbnailMasksDisabled; // TRUE only when the plugin is unloading/removing

    BOOL ArcCacheHaveInfo;    // TRUE == information about the use of disk cache in the archive plugin is already available (CPluginInterfaceForArchiverAbstract::GetCacheInfo has been called)
    char* ArcCacheTmpPath;    // location of copies of files extracted from the archive: NULL = TEMP, otherwise a path (root directory of the cache)
    BOOL ArcCacheOwnDelete;   // TRUE = call CPluginInterfaceForArchiverAbstract::DeleteTmpCopy() to delete cached copies
    BOOL ArcCacheCacheCopies; // TRUE = crtCache (delete when closing the archive or when cache limit is reached), FALSE = crtDirect (delete immediately when not needed)

    CIconList* PluginIcons;     // plugin icons for the GUI (menus, toolbars, dialogs) owned by Salamander and surviving plugin unload
    CIconList* PluginIconsGray; // grayscale version

    CIconList* PluginDynMenuIcons; // icons for the dynamic menu, see SupportDynMenuExt (owned by Salamander)

    int PluginIconIndex;          // index of the plugin icon (Plugins window/Plugins menu + Help/About Plugin menu and optional submenu icon in the Plugins menu — see details in PluginSubmenuIconIndex) (index in PluginIcons; -1=no icon; the icon index is not checked – it may be invalid)
    int PluginSubmenuIconIndex;   // icon index for the plugin submenu in the Plugins menu and optionally in the drop-down button for the submenu on the top toolbar (index in PluginIcons; -1=PluginIconIndex was used; the icon index is not checked – it may be invalid)
    BOOL ShowSubmenuInPluginsBar; // TRUE = show a drop-down button with the plugin submenu on the top toolbar using PluginSubmenuIconIndex icon

    BOOL PluginIsNethood;           // TRUE = the plugin replaces the Network item from the Change Drive menu (added for Nethood plugin)
    BOOL PluginUsesPasswordManager; // TRUE = the plugin uses the Password Manager

    int IconOverlaysCount; // number of icon-overlay triples in the IconOverlays array
    HICON* IconOverlays;   // allocated array of icon overlays, number of elements = 3 * IconOverlaysCount (sizes: 16 + 32 + 48)

    // helper data:
    CMenuPopup* SubMenu; // pointer to the submenu belonging to this plugin, NULL if none exists

    CSalamanderDebug SalamanderDebug;                     // object with TRACE and CALL_STACK for this plugin
    CSalamanderGeneral SalamanderGeneral;                 // object with general-purpose methods for this plugin
    CSalamanderGUI SalamanderGUI;                         // object providing customized Windows controls used in Salamander
    CSalamanderPasswordManager SalamanderPasswordManager; // object providing access to the password manager for this plugin

    BOOL ShouldUnload;      // TRUE if the plugin should unload at the next possible opportunity
    BOOL ShouldRebuildMenu; // TRUE if the plugin menu should be rebuilt at the next possible opportunity

    // commands requested by this plugin (waiting to run in "sal-idle"):
    // <0, 499>  - Salamander commands
    // <500, 1000499> - commands from the plugin menu (menu extensions)
    TDirectArray<DWORD> Commands;

    BOOL OpenPackDlg;                // TRUE = open the Pack dialog for this plugin
    int PackDlgDelFilesAfterPacking; // Pack dialog: "Delete files after packing" checkbox: 0=default, 1=on, 2=off
    BOOL OpenUnpackDlg;              // TRUE = open the Unpack dialog for this plugin
    char* UnpackDlgUnpackMask;       // Unpack dialog: "Unpack files" mask; NULL=default, otherwise allocated mask text

#ifdef _DEBUG
    int OpenedFSCounter; // count of open FS interfaces
    int OpenedPDCounter; // count of open PluginData interfaces
#endif

protected:
    HINSTANCE DLL;                                                         // handle of the plug-in’s DLL file
    CPluginInterfaceEncapsulation PluginIface;                             // plugin interface (set to -1 during the entry point call)
    CPluginInterfaceForArchiverEncapsulation PluginIfaceForArchiver;       // plugin interface: archiver
    CPluginInterfaceForViewerEncapsulation PluginIfaceForViewer;           // plugin interface: viewer
    CPluginInterfaceForMenuExtEncapsulation PluginIfaceForMenuExt;         // plugin interface: menu extension
    CPluginInterfaceForFSEncapsulation PluginIfaceForFS;                   // plugin interface: file system
    CPluginInterfaceForThumbLoaderEncapsulation PluginIfaceForThumbLoader; // plugin interface: thumbnail loader

public:
    CPluginData(const char* name, const char* dllName, BOOL supportPanelView,
                BOOL supportPanelEdit, BOOL supportCustomPack, BOOL supportCustomUnpack,
                BOOL supportConfiguration, BOOL supportLoadSave, BOOL supportViewer,
                BOOL supportFS, BOOL supportDynMenuExt, const char* version,
                const char* copyright, const char* description, const char* regKeyName,
                const char* extensions, TIndirectArray<char>* fsNames, BOOL loadOnStart,
                char* lastSLGName, const char* pluginHomePageURL);
    ~CPluginData();

    BOOL IsGood() { return Name != NULL; } // was the constructor successful?

    // returns the plugin interface
    CPluginInterfaceEncapsulation* GetPluginInterface() { return &PluginIface; }
    CPluginInterfaceForFSEncapsulation* GetPluginInterfaceForFS() { return &PluginIfaceForFS; }
    CPluginInterfaceForMenuExtEncapsulation* GetPluginInterfaceForMenuExt() { return &PluginIfaceForMenuExt; }
    CPluginInterfaceForArchiverEncapsulation* GetPluginIfaceForArchiver() { return &PluginIfaceForArchiver; }
    CPluginInterfaceForViewerEncapsulation* GetPluginInterfaceForViewer() { return &PluginIfaceForViewer; }
    CPluginInterfaceForThumbLoaderEncapsulation* GetPluginInterfaceForThumbLoader() { return &PluginIfaceForThumbLoader; }

    // loads the DLL into memory, attaches to it and verifies the validity of the stored information (SupportXXX, etc.)
    // loads only a DLL that matches exactly, otherwise the plugin reinstallation is required
    // 'parent' is the parent window for message boxes; if it is 'quiet'==TRUE no error messages are shown
    // (however, messages from inside the plugin are still displayed)
    // 'waitCursor' shows the Wait cursor while loading the DLL library
    // if 'showUnsupOnX64' is TRUE, a message box warns about plugins unsupported on x64
    // if 'releaseDynMenuIcons' is TRUE, plugins`s dynamic menu icons are released (they are reloaded before opening the menu)
    BOOL InitDLL(HWND parent, BOOL quiet = FALSE, BOOL waitCursor = TRUE, BOOL showUnsupOnX64 = TRUE,
                 BOOL releaseDynMenuIcons = TRUE);

    BOOL GetLoaded() { return DLL != NULL; }

    HINSTANCE GetPluginDLL() { return DLL; }

    // returns the plugin name extended with "(Plugin)"; used where it's not clear that it is a plugin
    // (e.g. in combo boxes within Archivers/Extensions, etc.)
    void GetDisplayName(char* buf, int bufSize);

    // plugin call: save its configuration; 'parent' is the parent window for message boxes
    void Save(HWND parent, HKEY regKeyConfig);

    // opens the plugin configuration dialog; 'parent' is the parent window for message boxes
    void Configuration(HWND parent);

    // opens the plugin about dialog; 'parent' is the parent window for message boxes
    void About(HWND parent);

    // calls 'loadOrSaveFunc' to load or save the plugin configuration (see
    // CSalamanderGeneral::CallLoadOrSaveConfiguration for details)
    void CallLoadOrSaveConfiguration(BOOL load, FSalLoadOrSaveConfiguration loadOrSaveFunc,
                                     void* param);

    // unloads the DLL; if 'ask' is TRUE, it prompts to Save when "save on exit" is enabled,
    // 'parent' is the parent window for message boxes; if 'ask' is FALSE it saves without asking,
    // returns TRUE if the DLL was unloaded
    BOOL Unload(HWND parent, BOOL ask);

    // checks if the plugin can be removed; 'parent' is the parent message box window,
    // 'index' is this plugin index in Plugins; returns TRUE if the plugin can be removed
    BOOL Remove(HWND parent, int index, BOOL canDelPluginRegKey);

    // adds a menu item to MenuItems
    void AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState, DWORD state_or,
                     DWORD state_and, DWORD skillLevel, CPluginMenuItemType type);

    // searches the plugin menu items for a command with 'id'; if it is found, sets
    // 'hotKey' (may be NULL) and 'hotKeyText' (may be NULL) and returns TRUE; otherwise FALSE
    BOOL GetMenuItemHotKey(int id, WORD* hotKey, char* hotKeyText, int hotKeyTextSize);

    // initializes menu items for this plugin; 'parent' is the parent message box window,
    // 'index' is this plugin index in Plugins, 'menu' is the submenu for this plugin
    void InitMenuItems(HWND parent, int index, CMenuPopup* menu);
    // helper method: builds one submenu level from MenuItems array (for nested submenu levels,
    // it is called recursively); 'menu' is the submenu receiving the items;
    // 'i' is the current index in MenuItems array - upon return, it is either past the end of the array
    // or at the item marking the end of the submenu; 'count' is the number of items in the submenu
    // 'menu'; 'mask' is the precomputed state mask
    void AddMenuItemsToSubmenuAux(CMenuPopup* menu, int& i, int count, DWORD mask);

    // clears the SUID of all menu items
    void ClearSUID();

    // plugin call: ExecuteMenuItem
    // runs a menu command with the identifier 'suid' (response to WM_COMMAND),
    // 'panel' is the panel for which the command should execute (used for MoveFiles),
    // 'parent' is the parent message box window, 'index' is this plugin index in Plugins,
    // returns TRUE if 'suid' was found among the plugin's menu items,
    // returns 'unselect' = the return value of the plugin ExecuteMenuItem call
    BOOL ExecuteMenuItem(CFilesWindow* panel, HWND parent, int index, int suid, BOOL& unselect);
    // 'id' is the internal command ID; ExecuteMenuItem2 is used for Last Command
    BOOL ExecuteMenuItem2(CFilesWindow* panel, HWND parent, int index, int id, BOOL& unselect);

    // plugin call: HelpForMenuItem
    // shows help for the menu command with the identifier 'suid' (response to WM_COMMAND),
    // 'parent' is the parent message box window, 'index' is this plugin index in Plugins,
    // returns TRUE if 'suid' was found among this plugin's menu items,
    // returns 'helpDisplayed' = the return value of the plugin HelpForMenuItem call
    BOOL HelpForMenuItem(HWND parent, int index, int suid, BOOL& helpDisplayed);

    // plugin call: BuildMenu
    // let the plugin build a new menu; 'parent' is the parent message box window;
    // if 'force' is TRUE, 'DynMenuWasAlreadyBuild' is ignored and the menu is always built;
    // returns TRUE if the plugin is loaded and has a static menu or has a dynamic menu and
    // also returns the menu-extension interface
    BOOL BuildMenu(HWND parent, BOOL force);

    // plugin call: ListArchive
    BOOL ListArchive(CFilesWindow* panel, const char* archiveFileName, CSalamanderDirectory& dir,
                     CPluginDataInterfaceAbstract*& pluginData);

    // plugin call: UnpackArchive
    BOOL UnpackArchive(CFilesWindow* panel, const char* archiveFileName,
                       CPluginDataInterfaceAbstract* pluginData,
                       const char* targetDir, const char* archiveRoot,
                       SalEnumSelection nextName, void* param);

    // plugin call: UnpackOneFile
    BOOL UnpackOneFile(CFilesWindow* panel, const char* archiveFileName,
                       CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                       const CFileData* fileData, const char* targetDir,
                       const char* newFileName, BOOL* renamingNotSupported);

    // plugin call: PackToArchive
    BOOL PackToArchive(CFilesWindow* panel, const char* archiveFileName,
                       const char* archiveRoot, BOOL move, const char* sourceDir,
                       SalEnumSelection2 nextName, void* param);

    // plugin call: DeleteFromArchive
    BOOL DeleteFromArchive(CFilesWindow* panel, const char* archiveFileName,
                           CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                           SalEnumSelection nextName, void* param);

    // plugin call: UnpackWholeArchive
    BOOL UnpackWholeArchive(CFilesWindow* panel, const char* archiveFileName, const char* mask,
                            const char* targetDir, BOOL delArchiveWhenDone,
                            CDynamicString* archiveVolumes);

    // plugin call: CanCloseArchive
    BOOL CanCloseArchive(CFilesWindow* panel, const char* archiveFileName, BOOL force);

    // plugin call: CanViewFile
    BOOL CanViewFile(const char* name);

    // plugin call: ViewFile
    BOOL ViewFile(const char* name, int left, int top, int width, int height,
                  UINT showCmd, BOOL alwaysOnTop, BOOL returnLock,
                  HANDLE* lock, BOOL* lockOwner,
                  int enumFilesSourceUID, int enumFilesCurrentIndex);

    // plugin call: OpenFS
    CPluginFSInterfaceAbstract* OpenFS(const char* fsName, int fsNameIndex);

    // plugin call: ExecuteChangeDriveMenuItem
    void ExecuteChangeDriveMenuItem(int panel);

    // plugin call: ChangeDriveMenuItemContextMenu
    BOOL ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                        CPluginFSInterfaceAbstract* pluginFS,
                                        const char* pluginFSName, int pluginFSNameIndex,
                                        BOOL isDetachedFS, BOOL& refreshMenu,
                                        BOOL& closeMenu, int& postCmd, void*& postCmdParam);

    // plugin call: EnsureShareExistsOnServer
    void EnsureShareExistsOnServer(HWND parent, int panel, const char* server, const char* share);

    // if the plugin is loaded, plugin call: Event
    void Event(int event, DWORD param);

    // plugin call: ClearHistory
    void ClearHistory(HWND parent);

    // if the plugin is loaded, plugin call: AcceptChangeOnPathNotification
    void AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    // plugin call: PasswordManagerEvent
    void PasswordManagerEvent(HWND parent, int event);

    // obtain the disk-cache settings from the archiver - uses a plugin call
    void GetCacheInfo(char* arcCacheTmpPath, BOOL* arcCacheOwnDelete, BOOL* arcCacheCacheCopies);

    // plugin call: DeleteTmpCopy
    void DeleteTmpCopy(const char* fileName, BOOL firstFile);

    // plugin call: PrematureDeleteTmpCopy
    BOOL PrematureDeleteTmpCopy(HWND parent, int copiesCount);

    // returns TRUE if the plugin is an archiver and has its own mechanism for deleting copies of files extracted from the archive 
    // must work even after unloading the plugin (until it is loaded again)
    BOOL IsArchiverAndHaveOwnDelete() { return ArcCacheOwnDelete; }

    HIMAGELIST CreateImageList(BOOL gray);

    // fills 'mii::State' and 'mii::Type' structuresaccording to the command for 'pluginIndex' and 'menuItemIndex'
    // returns TRUE if the item is enabled and FALSE if it is grayed
    BOOL GetMenuItemStateType(int pluginIndex, int menuItemIndex, MENU_ITEM_INFO* mii);

    // synchronizes old hot keys (from configuration) with new ones (from Connect()):
    // extracts hot keys with the dirty bit set from the 'oldMenuItems' array and moves them into the 'MenuItems' array
    // synchronization is driven by the command ID value
    void HotKeysMerge(TIndirectArray<CPluginMenuItem>* oldMenuItems);

    // ensures hot-key integrity (invalid ones set to 0):
    // - hot key must not belong to Salamander
    // - hot key must not belong to another plugin
    // - hot key cannot be repeated within one submenu
    void HotKeysEnsureIntegrity();

    // releases and zeros 'PluginDynMenuIcons'
    void ReleasePluginDynMenuIcons();

    // releases and zeros 'IconOverlays' and 'IconOverlaysCount'
    void ReleaseIconOverlays();

protected:
    // returns a mask (0 or a combination of MENU_EVENT_xxx) for this plugin and the current state
    // ActualStateMask, ActiveUnpackerIndex, ActivePackerIndex...
    // 'index' is this plugin index in Plugins
    DWORD GetMaskForMenuItems(int index);

    friend class CCallStack;
};

// Overview of objects related to archivers/viewers:
//   CPackerFormatConfig - list of archive extensions with associated panel archivers for view/edit,
//                         'PackerIndex' (edit) and 'UnpackerIndex' (view), see below
//   CArchiverConfig - external panel archivers (data for packX.cpp modules)
//   CPackerConfig - custom pack - Alt+F5 - external and internal packers
//   CUnpackerConfig - custom unpack - Alt+F6 - external and internal unpackers
//   CPlugins - list of archiver/viewer plugins
//
// 'PackerIndex' and 'UnpackerIndex' in CPackerFormatConfig:
//   i < 0  => (-i - 1) is the index in CPlugins of the plugin used for "panel-arch. view/edit"
//   i >= 0 => i is the index in CArchiverConfig for an external "panel-arch. view/edit"
//
// 'Type' in CPackerConfig and CUnpackerConfig:
//   i == 0 => it is an external archiver; data for external "custom pack/unpack" are used
//   i < 0 => (-i - 1) is the index in CPlugins of the plugin used for "custom pack/unpack"
//
// 'ViewerType' in CViewerMasks:
//   i == 0 => it is an external viewer; data for the external "file viewer" are used
//   i == 1 => it is an internal text/hex viewer
//   i < 0 => (-i - 1) is the index in CPlugins of the plugin that is used for "file viewer"

class CMenuPopup;
class CDrivesList;

struct CPluginsStateCache
{
    DWORD ActualStateMask;      // precomputed mask for CPluginMenuItem::StateMask
    int ActiveUnpackerIndex;    // precomputed "unpacker" index for the active panel, -1 => invalid
    int ActivePackerIndex;      // precomputed "packer" index for the active panel, -1 => invalid
    int NonactiveUnpackerIndex; // precomputed "unpacker" index for the inactive panel, -1 => invalid
    int NonactivePackerIndex;   // precomputed "packer" index for the inactive panel, -1 => invalid
    int FileUnpackerIndex;      // precomputed "unpacker" index for the focused file, -1 => invalid
    int FilePackerIndex;        // precomputed "packer" index for the focused file, -1 => invalid
    int ActiveFSIndex;          // precomputed "FS" index for the active panel, -1 => invalid
    int NonactiveFSIndex;       // precomputed "FS" index for the inactive panel, -1 => invalid

    void Clean()
    {
        ActualStateMask = 0;
        ActiveUnpackerIndex = -1;
        ActivePackerIndex = -1;
        NonactiveUnpackerIndex = -1;
        NonactivePackerIndex = -1;
        FileUnpackerIndex = -1;
        FilePackerIndex = -1;
        ActiveFSIndex = -1;
        NonactiveFSIndex = -1;
    }
};

// CPluginOrder specifies the order in which plugins are displayed (menu, plugin bar, ...)
struct CPluginOrder
{
    char* DLLName; // DLL file name, relative to "plugins" or absolute

    // temporary variables, not stored in the registry
    BOOL ShowInBar; // used only when converting the old configuration
    int Index;      // index in CPlugins::Data
    DWORD Flags;    // temporary helper variable for UpdatePluginsOrder
};

struct CPluginFSTimer
{
    DWORD AbsTimeout;                       // absolute timeout value for the timer
    CPluginFSInterfaceAbstract* TimerOwner; // FS object that should receive the timer timeout
    DWORD TimerParam;                       // timer parameter (plugin data)

    DWORD TimerAddedTime; // "time" the timer was added (prevents endless loops inside CPlugins::HandlePluginFSTimers())

    CPluginFSTimer(DWORD absTimeout, CPluginFSInterfaceAbstract* timerOwner, DWORD timerParam, DWORD timerAddedTime)
    {
        AbsTimeout = absTimeout;
        TimerOwner = timerOwner;
        TimerParam = timerParam;
        TimerAddedTime = timerAddedTime;
    }
};

class CPlugins
{
protected:
    TIndirectArray<CPluginData> Data;
    CRITICAL_SECTION DataCS; // critical section used only to synchronize data accessed by GetIndex() method

    TDirectArray<CPluginOrder> Order; // order in which plugins are displayed

    BOOL DefaultConfiguration; // TRUE => ZIP+TAR+PAK; allows recoding of old archiver data

    TIndirectArray<CPluginFSTimer> PluginFSTimers;    // timers of individual plugin FS
    DWORD TimerTimeCounter;                           // "time" for adding timer (prevents endless loops inside CPlugins::HandlePluginFSTimers())
    BOOL StopTimerHandlerRecursion;                   // prevents recursive calls to HandlePluginFSTimers()
    CPluginFSInterfaceEncapsulation* WorkingPluginFS; // working plugin FS object (neither in a panel nor among detached FS yet)

    // LastPlgCmdXXX keep information about the last executed command from the Plugins menu
    // if LastPlgCmdPlugin == NULL, LastPlgCmdID is meaningless and the menu item will be disabled with the default text
    char* LastPlgCmdPlugin; // path to the plugin whose command was executed (CPluginData::DLLName)
    int LastPlgCmdID;       // internal ID of the command (CPluginMenuItem::ID)

public:                     // helper variables for handling menu items coming from plugins:
    int LastSUID;           // helper variable for generating SUID for menu items
    int RootMenuItemsCount; // original number of items in the plugin root menu, -1 means "not yet determined"

    CPluginsStateCache StateCache;

    DWORD LoadInfoBase; // base for creating flag returned via CSalamanderPluginEntry::GetLoadInformation()

public:
    CPlugins() : Data(30, 10), Order(30, 10), PluginFSTimers(10, 50)
    {
        LastSUID = -1;
        RootMenuItemsCount = -1;
        DefaultConfiguration = FALSE;
        HANDLES(InitializeCriticalSection(&DataCS));
        Load(NULL, NULL);
        StateCache.Clean();
        LoadInfoBase = 0;
        LastPlgCmdPlugin = NULL;
        // initializing LastPlgCmdID has no meaning
        TimerTimeCounter = 0;
        StopTimerHandlerRecursion = FALSE;
        WorkingPluginFS = NULL;
    }
    ~CPlugins();

    void EnterDataCS() { HANDLES(EnterCriticalSection(&DataCS)); }
    void LeaveDataCS() { HANDLES(LeaveCriticalSection(&DataCS)); }

    // loads the object from registry key 'regKey' or default values (if regKey==NULL) (ZIP + TAR + PAK)
    // 'parent' is the parent window for message boxes (may be NULL)
    void Load(HWND parent, HKEY regKey);
    void LoadOrder(HWND parent, HKEY regKey); // loads plugin order into the Order array

    // saves the object to the registry (plugin info + plugin`s own configuration),
    // 'parent' is the parent window for message boxes
    void Save(HWND parent, HKEY regKey, HKEY regKeyConfig, HKEY regKeyOrder);

    // clears plugin records (CheckData must be called afterwards)
    void Clear()
    {
        HANDLES(EnterCriticalSection(&DataCS));
        Data.DestroyMembers();
        HANDLES(LeaveCriticalSection(&DataCS));
        DefaultConfiguration = FALSE;
    }

    // adjusts all archive-related data structures -> ensures data consistency
    void CheckData();

    // removes from Data all plugins whose .spl file no longer exists; if 'canDelPluginRegKey' is TRUE,
    // their configuration in the registry is also deleted. in the 'notLoadedPluginNames' (buffer of size
    // 'notLoadedPluginNamesSize') returns a list of names (up to 'maxNotLoadedPluginNames' names) of
    // plugins that were not loaded but with the configuration in the registry(either removed or failed InitDLL()),
    // separated by ", " + in 'numOfSkippedNotLoadedPluginNames' (if not NULL) returns the number of
    // names that are not stored in 'notLoadedPluginNames'. in 'loadAllPlugins' is TRUE only when upgrading
    // to a new Salamander version and all plugins should be loaded; those that fail and still
    // have the configuration in the registryare should be stored in 'notLoadedPluginNames'
    void RemoveNoLongerExistingPlugins(BOOL canDelPluginRegKey, BOOL loadAllPlugins = FALSE,
                                       char* notLoadedPluginNames = NULL,
                                       int notLoadedPluginNamesSize = 0,
                                       int maxNotLoadedPluginNames = 0,
                                       int* numOfSkippedNotLoadedPluginNames = NULL,
                                       HWND parent = NULL);

    // automatically installs plugins from the standard "plugins" directory (adds only
    // those not yet added) and automatically uninstalls plugins whose .spl files disappeared
    void AutoInstallStdPluginsDir(HWND parent);

    // handles addition of newly installed plugins (reads plugins.ver); returns TRUE if a new
    // version of plugins.ver file was found (configuration must be saved so the process doesn't repeat
    // on the next start of Salamander)
    BOOL ReadPluginsVer(HWND parent, BOOL importFromOldConfig);

    // loads all plugins and calls their ClearHistory methods
    void ClearHistory(HWND parent);

    // tries to load all plugins (Test All button in the Plugins dialog), returns TRUE if at
    // least one plugin was newly loaded
    BOOL TestAll(HWND parent);

    // tries to load all plugins (on first Salamander start after a language change)
    void LoadAll(HWND parent);

    // loads all plugins with the load-on-start flag
    void HandleLoadOnStartFlag(HWND parent);

    // performs a rebuild of the menu and unloads all plugins that requested it; returns the ID of the first Salamander/menu
    // command requested by the plugins (WM_COMMAND is sent immediately to the main window) along with the plugin in 'data'
    void GetCmdAndUnloadMarkedPlugins(HWND parent, int* cmd, CPluginData** data);

    // returns the plugin that requested to open the Pack/Unpack dialog; if there is none, returns NULL in 'data'
    // and -1 in 'pluginIndex'
    void OpenPackOrUnpackDlgForMarkedPlugins(CPluginData** data, int* pluginIndex);

    // returns, one by one, salamand.exe and all plugins including their versions; index counts from zero (in/out)
    // returns TRUE if the result is valid
    BOOL EnumInstalledModules(int* index, char* module, char* version);

    // adds a plugin, returns on success
    BOOL AddPlugin(const char* name, const char* dllName, BOOL supportPanelView,
                   BOOL supportPanelEdit, BOOL supportCustomPack, BOOL supportCustomUnpack,
                   BOOL supportConfiguration, BOOL supportLoadSave, BOOL supportViewer,
                   BOOL supportFS, BOOL supportDynMenuExt, const char* version,
                   const char* copyright, const char* description, const char* regKeyName,
                   const char* extensions, TIndirectArray<char>* fsNames, BOOL loadOnStart,
                   char* lastSLGName, const char* pluginHomePageURL);

    // adds a plugin; 'parent' is the parent message box window, 'fileName' is the DLL file name
    // of the plugin, returns TRUE if the plugin is added
    BOOL AddPlugin(HWND parent, const char* fileName);

    // removes a plugin; 'parent' is the parent message box window, maintains data consistency,
    // returns success
    BOOL Remove(HWND parent, int index, BOOL canDelPluginRegKey);

    // Salamander is exiting; plugins should exit as well, returns success (TRUE = plugins unloaded)
    BOOL UnloadAll(HWND parent);

    // it stores a unique registry key name for plugin private data in 'uniqueKeyName',
    // the name is based on 'regKeyName'
    void GetUniqueRegKeyName(char* uniqueKeyName, const char* regKeyName);

    // it stores a unique FS name based on 'fsName' in 'uniqueFSName';
    // 'uniqueFSNames' (if not NULL) is an array of names to which the resulting
    // 'uniqueFSName' must also be unique; 'oldFSNames' (if not NULL) is an array of old
    // fs names from previous plugin loads from which a unique name is preferably selected
    // and removed (so the user's FS name doesn't change with each plugin load)
    void GetUniqueFSName(char* uniqueFSName, const char* fsName,
                         TIndirectArray<char>* uniqueFSNames,
                         TIndirectArray<char>* oldFSNames);

    // returns the number of plugins
    int GetCount() { return Data.Count; }

    // returns plugin data from the given index
    // NOTE: the pointer is valid only until the number of plugins changes (the array expands or shrinks)
    CPluginData* Get(int index)
    {
        if (index >= 0 && index < Data.Count)
            return Data[index];
        else
            return NULL;
    }

    // returns the index of a plugin in Data, -1 if not found; used only by CSalamanderConnect
    // and CSalamanderBuildMenu. Avoid storing CPluginData pointers and thus avoid searching by them.
    // NOTE: the index is valid only until the number of plugins changes (the array expands or shrinks)
    int GetIndexJustForConnect(const CPluginData* plugin);

    // returns the index of a plugin in Data, -1 if not found
    // NOTE: the index is valid only until the number of plugins changes (the array expands or shrinks)
    // may be called from any thread
    int GetIndex(const CPluginInterfaceAbstract* plugin);

    // returns the CPluginData containing iface 'plugin', or NULL if it does not exist
    // NOTE: the pointer is valid only until the number of plugins changes (the array expands or shrinks)
    CPluginData* GetPluginData(const CPluginInterfaceForFSAbstract* plugin);

    // returns the CPluginData with iface 'plugin', or NULL if none exists
    // NOTE: the pointer is valid only until the number of plugins changes (the array expands or shrinks)
    CPluginData* GetPluginData(const CPluginInterfaceAbstract* plugin, int* lastIndex = NULL);

    // returns the CPluginData whose DLLName equals dllName (DLLName is allocated only once -> unique
    // plugin identifier). Returns NULL if not found.
    // NOTE: the pointer is valid only until the number of plugins changes (the array expands or shrinks)
    CPluginData* GetPluginData(const char* dllName);

    // returns the CPluginData containing DLLName that ends with 'dllSuffix'; returns NULL if it does not exist
    // NOTE: the pointer is valid only until the number of plugins changes (the array expands or shrinks)
    CPluginData* GetPluginDataFromSuffix(const char* dllSuffix);

    // creates an image list (colorful, if gray == FALSE, otherwise grayscale)
    // each plugin is represented by a single icon in the imagelist
    // a default plug icon (plug symbol) is used when the plugin lacks its own icon
    HIMAGELIST CreateIconsList(BOOL gray);

    // adds all plugin names to the list view; if setOnly is TRUE, only thevalues are set;
    // 'numOfLoaded' returns the number of loaded plugins
    void AddNamesToListView(HWND hListView, BOOL setOnly, int* numOfLoaded);

    // adds pointers to CPluginData of plugins that provide thumbnails
    // NOTE: the pointers are valid only until the number of plugins changes (the array expands or shrinks)
    void AddThumbLoaderPlugins(TIndirectArray<CPluginData>& thumbLoaderPlugins);

    // adds plugin names to a menu
    // the number of entries is limited by maxCount variable
    // commands are set to firstID + itemIndex
    // if 'configurableOnly' is TRUE only plugins supporting configuration are listed,
    // otherwise all names are added
    // returns TRUE if at least one plugin was added and Alt+? keys should be assigned
    BOOL AddNamesToMenu(CMenuPopup* menu, DWORD firstID, int maxCount, BOOL configurableOnly);

    // adds FS commands to the change-drive menu, returns success
    BOOL AddItemsToChangeDrvMenu(CDrivesList* drvList, int& currentFSIndex,
                                 CPluginInterfaceForFSAbstract* ifaceForFS,
                                 BOOL getGrayIcons);

    // opens the plugin about dialog
    void OnPluginAbout(HWND hParent, int index);

    // opens the plugin configuration dialog (if supported)
    void OnPluginConfiguration(HWND hParent, int index);

    // reacts to WM_USER_INITMENUPOPUP for the Plugins menu
    // 'parent' is the parent message box window, 'root' is the Plugins menu
    void InitMenuItems(HWND parent, CMenuPopup* root);

    // handles WM_USER_INITMENUPOPUP for individual plugin submenus from the Plugins menu
    // 'parent' is the parent window for message boxes, 'submenu' is the specific plugin menu
    void InitSubMenuItems(HWND parent, CMenuPopup* submenu);

    // initializes menu items of a plugin; 'parent' is the parent window for message boxes,
    // 'index' is the plugin index in Plugins, 'menu' is the submenu for that plugin
    // returns TRUE on success, otherwise FALSE
    BOOL InitPluginMenuItemsForBar(HWND parent, int index, CMenuPopup* menu);

    // adds to the toolbar the buttons of plugins that have menus and are set to be visible
    BOOL InitPluginsBar(CToolBar* bar);

    // runs the menu command with the identifier 'suid' (response to WM_COMMAND)
    // 'parent' is the parent window for message boxes; returns TRUE if panel selection should be cleared
    BOOL ExecuteMenuItem(CFilesWindow* panel, HWND parent, int suid);

    // shows help for the menu command with the identifier 'suid' (WM_COMMAND in HelpMode)
    // 'parent' is the parent window for message boxes; returns TRUE if the help was shown
    BOOL HelpForMenuItem(HWND parent, int suid);

    // searches plugins and external archivers that support "panel archiver view/edit" for at least
    // one of the given extensions; 'exclude' is the plugin index to ignore (it will be cleared),
    // 'view' and 'edit' are the indexes of the found archivers (following the PackerIndex and UnpackerIndex
    // conventions from CPackerFormatConfig, see above), 'viewFound' and 'editFound' say whether 'view'
    // and 'edit' are valid
    void FindViewEdit(const char* extensions, int exclude, BOOL& viewFound, int& view,
                      BOOL& editFound, int& edit);

    // searches for a DLL file among plugin DLL files; if it exists, returns TRUE and the plugin index
    BOOL FindDLL(const char* dllName, int& index);

    // returns TRUE if fsName is a known plugin file system; if TRUE, it also returns 'index'
    // of the FS plugin and the 'fsNameIndex' index of the plugin's file system name
    BOOL IsPluginFS(const char* fsName, int& index, int& fsNameIndex);

    // returns TRUE if 'fsName1' and 'fsName2' are from the same plugin; if TRUE,
    // it also returns the 'fsName2Index' index of the file system name 'fsName2' within that plugin
    BOOL AreFSNamesFromSamePlugin(const char* fsName1, const char* fsName2, int& fsName2Index);

    // returns the index of the plugin for "custom pack" that is the 'count'-th in order (from zero),
    // returns -1 if such plugin does not exist
    int GetCustomPackerIndex(int count);

    // returns the number of "custom pack" plugins before the given index,
    // inverse function to GetCustomPackerIndex,
    // returns -1 if 'index' is invalid
    int GetCustomPackerCount(int index);

    // returns the index of the plugin for "custom unpack" that is the 'count'-th in order (from zero),
    // returns -1 if such plugin does not exist
    int GetCustomUnpackerIndex(int count);

    // returns the number of "custom unpack" plugins before the given index,
    // inverse function to GetCustomUnpackerIndex,
    // returns -1 if 'index' is invalid
    int GetCustomUnpackerCount(int index);

    // returns the index of the plugin for "panel view" that is the 'count'-th in order (from zero),
    // returns -1 if such plugin does not exist
    int GetPanelViewIndex(int count);

    // returns the number of plugins for "panel view" before the given index,
    // inverse function to GetPanelViewIndex,
    // returns -1 if 'index' is invalid
    int GetPanelViewCount(int index);

    // returns the index of the plugin for "panel edit" that is the 'count'-th in order (from zero),
    // returns -1 if such plugin does not exist
    int GetPanelEditIndex(int count);

    // returns the number of plugins for "panel edit" before the given index,
    // inverse function to GetPanelEditIndex,
    // returns -1 if 'index' is invalid
    int GetPanelEditCount(int index);

    // returns the index of the plugin for "file viewer" that is the 'count'-th in order (from zero),
    // returns -1 if such plugin does not exist
    int GetViewerIndex(int count);

    // returns the number of plugins for "file viewer" before the given index,
    // inverse function to GetViewerIndex,
    // returns -1 if 'index' is invalid
    int GetViewerCount(int index);

    // calls the Event method of all plugins (delivering the message to loaded plugins)
    void Event(int event, DWORD param);

    // calls AcceptChangeOnPathNotification method for all plugins (delivering the message to loaded plugins)
    void AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    // reaction to the Plugins/Last Command -- if a Last Command exists, it is executed
    // 'parent' is the parent window for message boxes; returns TRUE if the panel selection should be cleared
    BOOL OnLastCommand(CFilesWindow* panel, HWND parent);

    // executes a command triggered by a hot key; 'pluginIndex' and 'menuItemIndex' specify the command
    // 'parent' is the parent window for message boxes; returns TRUE if the panel selection should be cleared
    BOOL ExecuteCommand(int pluginIndex, int menuItemIndex, CFilesWindow* panel, HWND parent);

    // goes through the Order array, removes plugins that no longer exist and finally appends
    // new plugins that do not yet have a record in the array
    // if 'sortByName' is TRUE, plugins are sorted alphabetically
    void UpdatePluginsOrder(BOOL sortByName);

    // moves an item within the Order array from 'index' to 'newIndex'
    BOOL ChangePluginsOrder(int index, int newIndex);

    // returns Order[index].Index
    int GetIndexByOrder(int index);

    // returns the index of the given plugin (according to the Order array) or -1 if not found
    int GetPluginOrderIndex(const CPluginData* plugin);

    // gets/sets the variable in the CPluginData::ShowSubmenuInPluginsBar array
    // 'index' is the plugin index in the Data array (not Orders)
    BOOL GetShowInBar(int index);
    void SetShowInBar(int index, BOOL showInBar);

    // gets/sets the plugin variable CPluginData::ChDrvMenuFSItemVisible
    // 'index' is the plugin index in the Data array (not Orders)
    BOOL GetShowInChDrv(int index);
    void SetShowInChDrv(int index, BOOL showInChDrv);

    // adds a new timer to the PluginFSTimers array; the absolute timeout is GetTickCount() + 'relTimeout'.
    // Once GetTickCount() returns a value greater than or equal to that timeout,
    // the FS object's Event() method is called with FSE_TIMER and 'timerParam'.
    // Returns TRUE on success (timer successfully added)
    BOOL AddPluginFSTimer(DWORD relTimeout, CPluginFSInterfaceAbstract* timerOwner, DWORD timerParam);

    // removes from the PluginFSTimers array either all timers belonging to 'timerOwner' (when 'allTimers' is TRUE),
    // or only those timers whose parameter equals 'timerParam' (when 'allTimers' is FALSE); returns the
    // number of removed timers
    int KillPluginFSTimer(CPluginFSInterfaceAbstract* timerOwner, BOOL allTimers, DWORD timerParam);

    // called when WM_TIMER with IDT_PLUGINFSTIMERS arrives; used to trigger timers
    // from the PluginFSTimers array
    void HandlePluginFSTimers();

    void SetWorkingPluginFS(CPluginFSInterfaceEncapsulation* workingPluginFS) { WorkingPluginFS = workingPluginFS; }

    // searches plugin menus for 'hotKey'; returns TRUE when found and provides
    // its location in 'pluginIndex' and 'menuItemIndex'
    BOOL FindHotKey(WORD hotKey, BOOL ignoreSkillLevels, const CPluginData* ignorePlugin, int* pluginIndex, int* menuItemIndex);

    // traverses all plugins except 'ignorePlugin' and removes the hot key if any
    // menu item has it assigned
    void RemoveHotKey(WORD hotKey, const CPluginData* ignorePlugin);

    // returns TRUE if any command has an assigned hot key and also sets 'pluginIndex' and 'menuItemIndex' accordingly
    BOOL QueryHotKey(WPARAM wParam, LPARAM lParam, int* pluginIndex, int* menuItemIndex);

    // reaction to WM_(SYS)KEYDOWN in the panel or the edit line -- plugins search
    // their menus to check whether this hotkey is taken; if they process the message, TRUE is returned, otherwise FALSE
    BOOL HandleKeyDown(WPARAM wParam, LPARAM lParam, CFilesWindow* activePanel, HWND hParent);

    // sets the LastPlgCmdPlugin and LastPlgCmdID variables
    void SetLastPlgCmd(const char* dllName, int id);

    // returns the number of loaded plugins that will save their configuration
    int GetPluginSaveCount();

    // after changing Salamander's language clears LastSLGName for all plugins so a new fallback
    // language is chosen for a plugin (used if the plug-in does not support the language currently selected in Salamander)
    void ClearLastSLGNames();

    // returns the number of plugins that can be loaded (GetLoaded() returns TRUE)
    int GetNumOfPluginsToLoad();

    // returns the FS name of the first plugin that adds a file system and at the same time, has PluginIsNethood
    // set to TRUE; if 'fsName' is not NULL, the found FS name is copied there; if 'nethoodPlugin'
    // is not NULL, it receives the found plugin. Returns success (FALSE if no such plugin exists)
    BOOL GetFirstNethoodPluginFSName(char* fsName = NULL, CPluginData** nethoodPlugin = NULL);

    // invokes PasswordManagerEvent method for all plugins using the Password Manager.
    // See CSalamanderGeneralAbstract::SetPluginUsesPasswordManager (loads unloaded plug-ins if necessary)
    void PasswordManagerEvent(HWND parent, int event);

    // releases and clears each plugin's 'PluginDynMenuIcons'
    void ReleasePluginDynMenuIcons();

protected:
    // based on the LastPlgCmdXXX variables finds the plugin and item in its menu corresponding to
    // the the last executed command from the Plugins menu; 'rebuildDynMenu' is TRUE if, in the case of a
    // dynamic menu, BuildMenu() should be called before searching; 'parent' is the parent message box
    // window (only when 'rebuildDynMenu' is TRUE);
    // if the command is found, TRUE is returned and 'pluginIndex' holds the plugin index in CPlugins::Data
    // and 'menuItemIndex' holds the index in the MenuItems array
    // otherwise FALSE is returned
    BOOL FindLastCommand(int* pluginIndex, int* menuItemIndex, BOOL rebuildDynMenu, HWND parent);

    // compute a new CPluginsStateCache::ActualStateMask and other variables (if it makes sense)
    // later CPluginData::GetMaskForMenuItems will determine the mask from them
    // count is the number of items in the Plugins menu
    void CalculateStateCache();

    // adds a record to the Order array; returns the index in the array on success, otherwise returns -1
    int AddPluginToOrder(const char* dllName, BOOL showInBar);

    // Sorts the Orders array by plugin name (used for newly added plug-ins to ensure alphabetical order)
    void QuickSortPluginsByName(int left, int right);

    // used only for the conversion from the old configuration (the visibility variable has been moved to CPluginData)
    BOOL PluginVisibleInBar(const char* dllName);

    // helper function: finds the index position for inserting a timer with 'timeoutAbs' into the PluginFSTimers array
    int FindIndexForNewPluginFSTimer(DWORD timeoutAbs);
};

//
// ****************************************************************************
// CSalamanderPluginEntry
//

class CSalamanderPluginEntry : public CSalamanderPluginEntryAbstract
{
protected:
    HWND Parent;                     // parent window of possible message boxes
    CPluginData* Plugin;             // plugin record
    BOOL Valid;                      // TRUE if SetBasicPluginData was called successfully
    BOOL Error;                      // has an error already been displayed?
    DWORD LoadInfo;                  // DWORD value returned by GetLoadInformation()
    TIndirectArray<char> OldFSNames; // array of old fs-names (names from the registry, replaced during plug-in loading)

public:
    CSalamanderPluginEntry(HWND parent, CPluginData* plugin) : OldFSNames(1, 10)
    {
        Parent = parent;
        Plugin = plugin;
        Valid = FALSE;
        Error = FALSE;
        LoadInfo = 0;
    }

    BOOL DataValid() { return Valid; }
    BOOL ShowError() { return !Error; }

    // adds another LOADINFO_XXX flag to LoadInfo
    void AddLoadInfo(DWORD flag) { LoadInfo |= flag; }

    virtual int WINAPI GetVersion() { return LAST_VERSION_OF_SALAMANDER; }

    virtual HWND WINAPI GetParentWindow() { return Parent; }

    virtual CSalamanderDebugAbstract* WINAPI GetSalamanderDebug() { return &Plugin->SalamanderDebug; }

    virtual BOOL WINAPI SetBasicPluginData(const char* pluginName, DWORD functions,
                                           const char* version, const char* copyright,
                                           const char* description, const char* regKeyName = NULL,
                                           const char* extensions = NULL, const char* fsName = NULL);

    virtual CSalamanderGeneralAbstract* WINAPI GetSalamanderGeneral() { return &Plugin->SalamanderGeneral; }

    virtual DWORD WINAPI GetLoadInformation() { return LoadInfo; }

    virtual HINSTANCE WINAPI LoadLanguageModule(HWND parent, const char* pluginName);
    virtual WORD WINAPI GetCurrentSalamanderLanguageID() { return (WORD)LanguageID; }

    virtual CSalamanderGUIAbstract* WINAPI GetSalamanderGUI() { return &Plugin->SalamanderGUI; }
    virtual CSalamanderSafeFileAbstract* WINAPI GetSalamanderSafeFile() { return &SalSafeFile; }

    virtual void WINAPI SetPluginHomePageURL(const char* url);

    virtual BOOL WINAPI AddFSName(const char* fsName, int* newFSNameIndex);
};

//
// ****************************************************************************
// CSalamanderRegistry
//

class CSalamanderRegistry : public CSalamanderRegistryAbstract
{
public:
    // clears the key 'key' of all subkeys and values; returns success
    virtual BOOL WINAPI ClearKey(HKEY key);

    // creates or opens an existing subkey 'name' of key 'key'; returns 'createdKey' and success
    virtual BOOL WINAPI CreateKey(HKEY key, const char* name, HKEY& createdKey);

    // opens an existing subkey 'name' of key 'key'; returns 'openedKey' and success
    virtual BOOL WINAPI OpenKey(HKEY key, const char* name, HKEY& openedKey);

    // closes a key opened via OpenKey or CreateKey
    virtual void WINAPI CloseKey(HKEY key);

    // deletes the subkey 'name' of the key 'key'; returns success
    virtual BOOL WINAPI DeleteKey(HKEY key, const char* name);

    // loads the value 'name'+'type'+'buffer'+'bufferSize' from key 'key'; returns success
    virtual BOOL WINAPI GetValue(HKEY key, const char* name, DWORD type, void* buffer, DWORD bufferSize);

    // stores the value 'name'+'type'+'data'+'dataSize' to key 'key'; for strings you may
    // specify 'dataSize' == -1 -> the string length is calculated using the strlen function;
    // returns success
    virtual BOOL WINAPI SetValue(HKEY key, const char* name, DWORD type, const void* data, DWORD dataSize);

    // deletes the value 'name' of the key 'key'; returns success
    virtual BOOL WINAPI DeleteValue(HKEY key, const char* name);

    // retrieves into 'bufferSize' the required size for the value of 'name' + 'type' from the key 'key'; returns success
    virtual BOOL WINAPI GetSize(HKEY key, const char* name, DWORD type, DWORD& bufferSize);
};

//
// ****************************************************************************
// CSalamanderConnect
//

class CSalamanderConnect : public CSalamanderConnectAbstract
{
protected:
    int Index; // plugin index in Plugins

    // five-variable filter for plugin upgrade, the menu is always upgraded
    BOOL CustomPack;   // TRUE -> "custom pack" modifications allowed
    BOOL CustomUnpack; // TRUE -> "custom unpack" modifications allowed
    BOOL PanelView;    // TRUE -> "panel view" modifications allowed
    BOOL PanelEdit;    // TRUE -> "panel edit" modifications allowed
    BOOL Viewer;       // TRUE -> "file viewer" modifications allowed
    int SubmenuLevel;  // current level of inserted submenus (0 = plugin submenu in the Plugins menu)

public:
    CSalamanderConnect(int index, BOOL customPack, BOOL customUnpack, BOOL panelView, BOOL panelEdit,
                       BOOL viewer)
    {
        Index = index;
        CustomPack = customPack;
        CustomUnpack = customUnpack;
        PanelView = panelView;
        PanelEdit = panelEdit;
        Viewer = viewer;
        SubmenuLevel = 0;
    }

    ~CSalamanderConnect()
    {
        if (SubmenuLevel > 0)
            TRACE_E("CSalamanderConnect::~CSalamanderConnect(): missing end of submenu (see CSalamanderConnect::AddSubmenuEnd())!");
    }

    virtual void WINAPI AddCustomPacker(const char* title, const char* defaultExtension, BOOL update);
    virtual void WINAPI AddCustomUnpacker(const char* title, const char* masks, BOOL update);

    virtual void WINAPI AddPanelArchiver(const char* extensions, BOOL edit, BOOL updateExts);
    virtual void WINAPI ForceRemovePanelArchiver(const char* extension);

    virtual void WINAPI AddViewer(const char* masks, BOOL force);
    virtual void WINAPI ForceRemoveViewer(const char* mask);

    virtual void WINAPI AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                    DWORD state_or, DWORD state_and, DWORD skillLevel);
    virtual void WINAPI AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                        DWORD state_or, DWORD state_and, DWORD skillLevel);
    virtual void WINAPI AddSubmenuEnd();
    virtual void WINAPI SetChangeDriveMenuItem(const char* title, int iconIndex);
    virtual void WINAPI SetThumbnailLoader(const char* masks);

    virtual void WINAPI SetBitmapWithIcons(HBITMAP bitmap);
    virtual void WINAPI SetPluginIcon(int iconIndex);
    virtual void WINAPI SetPluginMenuAndToolbarIcon(int iconIndex);
    virtual void WINAPI SetIconListForGUI(CGUIIconListAbstract* iconList);
};

//
// ****************************************************************************
// CSalamanderBuildMenu
//

class CSalamanderBuildMenu : public CSalamanderBuildMenuAbstract
{
protected:
    int Index;        // plugin index in Plugins
    int SubmenuLevel; // current level of inserted submenus (0 = plugin submenu in the Plugins menu)

public:
    CSalamanderBuildMenu(int index)
    {
        Index = index;
        SubmenuLevel = 0;
    }

    ~CSalamanderBuildMenu()
    {
        if (SubmenuLevel > 0)
            TRACE_E("CSalamanderBuildMenu::~CSalamanderBuildMenu(): missing end of submenu (see CSalamanderBuildMenu::AddSubmenuEnd())!");
    }

    virtual void WINAPI AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                    DWORD state_or, DWORD state_and, DWORD skillLevel);
    virtual void WINAPI AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                        DWORD state_or, DWORD state_and, DWORD skillLevel);
    virtual void WINAPI AddSubmenuEnd();

    virtual void WINAPI SetIconListForMenu(CGUIIconListAbstract* iconList);
};

extern CPlugins Plugins;
extern int AlreadyInPlugin;

// internal function for handling plugin icons
BOOL CreateGrayscaleDIB(HBITMAP hSource, COLORREF transparent, HBITMAP& hGrayscale);
//HICON GetIconFromDIB(HBITMAP hBitmap, int index);

// executes the conversion of a path to the external format (calls the corresponding plugin method)
// CPluginInterfaceForFSAbstract::ConvertPathToExternal())
void PluginFSConvertPathToExternal(char* path);

#ifdef _WIN64 // FIXME_X64_WINSCP
// test whether this is a plugin missing in the x64 version of Salamander: currently only WinSCP
BOOL IsPluginUnsupportedOnX64(const char* dllName, const char** pluginNameEN = NULL);
#endif // _WIN64
