// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// pri zmene hledat "BuiltForVersion" - testy na starsi verze pluginu uz nebudou mit smysl, zahodit je
#define PLUGIN_REQVER 103 // ("5.0") loadit jen pluginy, ktere vraci alespon tuto pozadovanou verzi Salamandera

//
// ****************************************************************************
// CPluginInterfaceEncapsulation + zapouzdreni jednotlivych casti pluginoveho interfacu
//
// trida pres kterou Salamander pristupuje k metodam interfacu CPluginInterfaceAbstract,
// zajistuje volani EnterPlugin a LeavePlugin, prime volani metod interfacu
// CPluginInterfaceAbstract muze vest k chybam (napr. refresh panelu behem prace s jeho
// daty - neplatne ukazatele, atd.)
//
// call-stack-message jsou obsazene jiz v CPluginData, odkud se volaji vsechny metody
// tohoto ifacu, vyjimkou jsou CloseFS a ReleasePluginDataInterface

// funkce volane pred a po volani jakekoliv metody plug-inu
void EnterPlugin();
void LeavePlugin();

class CPluginInterfaceForArchiverEncapsulation
{
protected:
    CPluginInterfaceForArchiverAbstract* Interface; // zapouzdreny interface

public:
    CPluginInterfaceForArchiverEncapsulation(CPluginInterfaceForArchiverAbstract* iface = NULL) { Interface = iface; }
    // je zapouzdreni inicializovano?
    BOOL NotEmpty() { return Interface != NULL; }
    // inicializace zapouzdreni
    void Init(CPluginInterfaceForArchiverAbstract* iface) { Interface = iface; }
    // zapouzdrujeme tento iface?
    BOOL Contains(CPluginInterfaceForArchiverAbstract const* iface) { return iface != NULL && Interface == iface; }
    // vraci ukazatel na zapouzdreny interface
    CPluginInterfaceForArchiverAbstract* GetInterface() { return Interface; }

    // metody interfacu CPluginInterfaceForArchiverAbstract bez "virtual" (je zbytecne, jen by zpomalovalo)

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
    // POZOR: pred spustenim operace v plug-inu je potreba snizit prioritu threadu !!!
    // ********************************************************************************
};

class CPluginInterfaceForViewerEncapsulation
{
protected:
    CPluginInterfaceForViewerAbstract* Interface; // zapouzdreny interface

public:
    CPluginInterfaceForViewerEncapsulation(CPluginInterfaceForViewerAbstract* iface = NULL) { Interface = iface; }
    // je zapouzdreni inicializovano?
    BOOL NotEmpty() { return Interface != NULL; }
    // inicializace zapouzdreni
    void Init(CPluginInterfaceForViewerAbstract* iface) { Interface = iface; }
    // zapouzdrujeme tento iface?
    BOOL Contains(CPluginInterfaceForViewerAbstract const* iface) { return iface != NULL && Interface == iface; }
    // vraci ukazatel na zapouzdreny interface
    CPluginInterfaceForViewerAbstract* GetInterface() { return Interface; }

    // metody interfacu CPluginInterfaceForViewerAbstract bez "virtual" (je zbytecne, jen by zpomalovalo)

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
    CPluginInterfaceForMenuExtAbstract* Interface; // zapouzdreny interface
    int BuiltForVersion;                           // platne jen kdyz je plugin naloadeny: verze Salamandera, pro kterou byl tento plugin nakompilovan (viz prehled verzi u LAST_VERSION_OF_SALAMANDER v spl_vers.h)

public:
    CPluginInterfaceForMenuExtEncapsulation(CPluginInterfaceForMenuExtAbstract* iface, int builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }

    // je zapouzdreni inicializovano?
    BOOL NotEmpty() { return Interface != NULL; }

    // inicializace zapouzdreni
    void Init(CPluginInterfaceForMenuExtAbstract* iface, BOOL builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }

    // zapouzdrujeme tento iface?
    BOOL Contains(CPluginInterfaceForMenuExtAbstract const* iface) { return iface != NULL && Interface == iface; }
    // vraci ukazatel na zapouzdreny interface
    CPluginInterfaceForMenuExtAbstract* GetInterface() { return Interface; }

    // metody interfacu CPluginInterfaceForMenuExtAbstract bez "virtual" (je zbytecne, jen by zpomalovalo)

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
    CPluginInterfaceForFSAbstract* Interface; // zapouzdreny interface
    int BuiltForVersion;                      // platne jen kdyz je plugin naloadeny: verze Salamandera, pro kterou byl tento plugin nakompilovan (viz prehled verzi u LAST_VERSION_OF_SALAMANDER v spl_vers.h)

public:
    CPluginInterfaceForFSEncapsulation(CPluginInterfaceForFSAbstract* iface, int builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }

    // je zapouzdreni inicializovano?
    BOOL NotEmpty() { return Interface != NULL; }

    // inicializace zapouzdreni
    void Init(CPluginInterfaceForFSAbstract* iface, int builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }

    // zapouzdrujeme tento iface?
    BOOL Contains(CPluginInterfaceForFSAbstract const* iface) { return iface != NULL && Interface == iface; }
    // vraci ukazatel na zapouzdreny interface
    CPluginInterfaceForFSAbstract* GetInterface() { return Interface; }

    // vraci verzi Salamandera, pro kterou byl tento plugin nakompilovan (viz prehled verzi u LAST_VERSION_OF_SALAMANDER v spl_vers.h)
    int GetBuiltForVersion() { return BuiltForVersion; }

    // metody interfacu CPluginInterfaceForFSAbstract bez "virtual" (je zbytecne, jen by zpomalovalo)

    CPluginFSInterfaceAbstract* OpenFS(const char* fsName, int fsNameIndex)
    {
        EnterPlugin();
        CPluginFSInterfaceAbstract* r = Interface->OpenFS(fsName, fsNameIndex);
        LeavePlugin();
        return r;
    }

    // prime volani interfacu (nejde pres CPluginData + InitDLL), zakladame call-stack-message
    void CloseFS(CPluginFSInterfaceAbstract* fs);

    void ExecuteChangeDriveMenuItem(int panel);

    BOOL ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                        CPluginFSInterfaceAbstract* pluginFS,
                                        const char* pluginFSName, int pluginFSNameIndex,
                                        BOOL isDetachedFS, BOOL& refreshMenu,
                                        BOOL& closeMenu, int& postCmd, void*& postCmdParam);

    // prime volani interfacu (nejde pres CPluginData + InitDLL), zakladame call-stack-message;
    // vola se tesne po ChangeDriveMenuItemContextMenu - plug-in je urcite naloaden
    void ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam);

    // prime volani interfacu (nejde pres CPluginData + InitDLL), zakladame call-stack-message;
    // vola se pouze kdyz je v panelu 'pluginFS' - plug-in je urcite naloaden
    void ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                     const char* pluginFSName, int pluginFSNameIndex,
                     CFileData& file, int isDir);

    // prime volani interfacu (nejde pres CPluginData + InitDLL), zakladame call-stack-message;
    // vola se pouze kdyz 'pluginFS' existuje (v panelu nebo odpojeny) - plug-in je urcite naloaden
    BOOL DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                      CPluginFSInterfaceAbstract* pluginFS,
                      const char* pluginFSName, int pluginFSNameIndex);

    // prime volani interfacu (nejde pres CPluginData + InitDLL), zakladame call-stack-message;
    // vola se pouze kdyz je plug-in naloaden (je otevreny FS)
    void ConvertPathToInternal(const char* fsName, int fsNameIndex, char* fsUserPart);

    // prime volani interfacu (nejde pres CPluginData + InitDLL), zakladame call-stack-message;
    // vola se pouze kdyz je plug-in naloaden (je otevreny FS)
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
    CPluginInterfaceForThumbLoaderAbstract* Interface; // zapouzdreny interface

    const char* DLLName; // odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril
    const char* Version; // odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril

public:
    CPluginInterfaceForThumbLoaderEncapsulation(CPluginInterfaceForThumbLoaderAbstract* iface = NULL,
                                                const char* dllName = NULL, const char* version = NULL)
    {
        Interface = iface;
        DLLName = dllName;
        Version = version;
    }

    // je zapouzdreni inicializovano?
    BOOL NotEmpty() { return Interface != NULL; }

    // inicializace zapouzdreni
    void Init(CPluginInterfaceForThumbLoaderAbstract* iface, const char* dllName, const char* version)
    {
        Interface = iface;
        DLLName = dllName;
        Version = version;
    }

    // zapouzdrujeme tento iface?
    BOOL Contains(CPluginInterfaceForThumbLoaderAbstract const* iface) { return iface != NULL && Interface == iface; }
    // vraci ukazatel na zapouzdreny interface
    CPluginInterfaceForThumbLoaderAbstract* GetInterface() { return Interface; }

    // metody interfacu CPluginInterfaceForThumbLoaderAbstract bez "virtual" (je zbytecne, jen by zpomalovalo)

    // nepotrebuje Enter/LeavePlugin, protoze smi pouzivat ze sal-general jen metody,
    // ktere jsou volatelne z libovolneho threadu (coz operace s panelem nejsou)
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
    CPluginInterfaceAbstract* Interface; // zapouzdreny interface; behem entry-pointu je nastaveno na -1
    int BuiltForVersion;                 // platne jen po dobu platnosti 'Interface': verze Salamandera, pro kterou byl tento plugin nakompilovan (viz prehled verzi u LAST_VERSION_OF_SALAMANDER v spl_vers.h)

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
    // je zapouzdreni inicializovano?
    BOOL NotEmpty() { return Interface != NULL && (INT_PTR)Interface != -1; } // -1 je nastaveno behem entry-pointu
    // inicializace zapouzdreni
    void Init(CPluginInterfaceAbstract* iface, int builtForVersion)
    {
        Interface = iface;
        BuiltForVersion = builtForVersion;
    }
    // zapouzdrujeme tento iface?
    BOOL Contains(CPluginInterfaceAbstract const* iface) { return iface != NULL && Interface == iface; }
    // vraci ukazatel na zapouzdreny interface
    CPluginInterfaceAbstract* GetInterface() { return Interface; }

    // metody interfacu CPluginInterfaceAbstract bez "virtual" (je zbytecne, jen by zpomalovalo)

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

    // prime volani interfacu (nejde pres CPluginData + InitDLL), zakladame call-stack-message
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
// trida pres kterou Salamander pristupuje k metodam interfacu CPluginDataInterfaceAbstract,
// zajistuje volani EnterPlugin a LeavePlugin, prime volani metod interfacu
// CPluginDataInterfaceAbstract muze vest k chybam (napr. refresh panelu behem prace s jeho
// daty - neplatne ukazatele, atd.)

class CFilesArray;

class CPluginDataInterfaceEncapsulation
{
protected:
    CPluginDataInterfaceAbstract* Interface; // zapouzdreny interface
    int BuiltForVersion;                     // platne jen po dobu platnosti 'Interface': verze Salamandera, pro kterou byl tento plugin nakompilovan (viz prehled verzi u LAST_VERSION_OF_SALAMANDER v spl_vers.h)

    const char* DLLName;              // odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril
    const char* Version;              // odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril
    CPluginInterfaceAbstract* Plugin; // plug-in, ktery iface vytvoril

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

    // je zapouzdreni inicializovano?
    BOOL NotEmpty() { return Interface != NULL; }

    // inicializace zapouzdreni
    void Init(CPluginDataInterfaceAbstract* iface, const char* dllName, const char* version,
              CPluginInterfaceAbstract* plugin, int builtForVersion)
    {
        Interface = iface;
        DLLName = dllName;
        Version = version;
        Plugin = plugin;
        BuiltForVersion = builtForVersion;
    }

    // zapouzdrujeme tento iface?
    BOOL Contains(CPluginDataInterfaceAbstract const* iface) { return iface != NULL && Interface == iface; }

    // vraci ukazatel na zapouzdreny interface
    CPluginDataInterfaceAbstract* GetInterface() { return Interface; }

    // vraci odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril
    const char* GetDLLName() { return DLLName; }

    // vraci odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril
    const char* GetVersion() { return Version; }

    // vraci iface plug-inu, ktery PluginData-iface vytvoril
    CPluginInterfaceAbstract* GetPluginInterface() { return Plugin; }

    // vraci verzi Salamandera, pro kterou byl tento plugin nakompilovan (viz prehled verzi u LAST_VERSION_OF_SALAMANDER v spl_vers.h)
    int GetBuiltForVersion() { return BuiltForVersion; }

    // nahrazka (kvuli rychlosti) za ReleasePluginData
    void ReleaseFilesOrDirs(CFilesArray* filesOrDirs, BOOL areDirs);

    // ReleasePluginData, jen jinak pojmenovane, aby se upozornilo na ReleaseFilesOrDirs
    void ReleasePluginData2(CFileData& file, BOOL isDir)
    {
        CALL_STACK_MESSAGE4("CPluginDataInterfaceEncapsulation::ReleasePluginData2(, %d) (%s v. %s)",
                            isDir, DLLName, Version);
        EnterPlugin();
        Interface->ReleasePluginData(file, isDir);
        LeavePlugin();
    }

    // metody interfacu CPluginDataInterfaceAbstract bez "virtual" (je zbytecne, jen by zpomalovalo)

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

        // prevedeme image list pluginu do naseho CIconList
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

    // nepotrebuje Enter/LeavePlugin, protoze smi pouzivat ze sal-general jen metody,
    // ktere jsou volatelne z libovolneho threadu (coz operace s panelem nejsou)
    BOOL HasSimplePluginIcon(CFileData& file, BOOL isDir)
    {
        // vola se pro vsechny soubory a adresare, CALL_STACK_MESSAGE zde zdrzuje (40ms na 3000 volanich)
        //      CALL_STACK_MESSAGE5("CPluginDataInterfaceEncapsulation::HasSimplePluginIcon(%s, %d) (%s v. %s)",
        //                          file.Name, isDir, DLLName, Version);
        return Interface->HasSimplePluginIcon(file, isDir);
    }

    // nepotrebuje Enter/LeavePlugin, protoze smi pouzivat ze sal-general jen metody,
    // ktere jsou volatelne z libovolneho threadu (coz operace s panelem nejsou)
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

    // nepotrebuje Enter/LeavePlugin, protoze smi pouzivat ze sal-general jen metody,
    // ktere jsou volatelne z libovolneho threadu (coz operace s panelem nejsou)
    int CompareFilesFromFS(const CFileData* file1, const CFileData* file2)
    {
        CALL_STACK_MESSAGE_NONE
        // nema call-stack z rychlostnich duvodu
        //      CALL_STACK_MESSAGE5("CPluginDataInterfaceEncapsulation::CompareFilesFromFS(%s, %s) (%s v. %s)",
        //                          file1->Name, file2->Name, DLLName, Version);
        return Interface->CompareFilesFromFS(file1, file2);
    }

    // nepotrebuje Enter/LeavePlugin, protoze smi pouzivat ze sal-general jen metody,
    // ktere jsou volatelne z libovolneho threadu (coz operace s panelem nejsou)
    void SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                   const CFileData* upperDir)
    {
        CALL_STACK_MESSAGE5("CPluginDataInterfaceEncapsulation::SetupView(%d, , %s,) (%s v. %s)",
                            leftPanel, archivePath, DLLName, Version);
        Interface->SetupView(leftPanel, view, archivePath, upperDir);
    }

private:
    void ReleasePluginData(CFileData& file, BOOL isDir) {} // bylo by prilis pomale, nahrazuje ReleaseFilesOrDirs
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
// trida pres kterou Salamander pristupuje k metodam interfacu CPluginFSInterfaceAbstract,
// zajistuje volani EnterPlugin a LeavePlugin, prime volani metod interfacu
// CPluginFSInterfaceAbstract muze vest k chybam (napr. refresh panelu behem prace s jeho
// daty - neplatne ukazatele, atd.)

class CPluginFSInterfaceEncapsulation
{
protected:
    CPluginFSInterfaceAbstract* Interface; // zapouzdreny interface
    int BuiltForVersion;                   // platne jen kdyz je plugin naloadeny: verze Salamandera, pro kterou byl tento plugin nakompilovan (viz prehled verzi u LAST_VERSION_OF_SALAMANDER v spl_vers.h)

    const char* DLLName;                           // odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril
    const char* Version;                           // odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril
    CPluginInterfaceForFSEncapsulation IfaceForFS; // plug-in, ktery iface vytvoril (cast pro FS)
    CPluginInterfaceAbstract* Iface;               // plug-in, ktery iface vytvoril (zaklad)
    DWORD SupportedServices;                       // posledni hodnota vracena metodou Interface->GetSupportedServices()
    char PluginFSName[MAX_PATH];                   // jmeno otevreneho FS
    int PluginFSNameIndex;                         // index jmena otevreneho FS

    static DWORD PluginFSTime; // globalni "cas" (pocitadlo) pro ziskani "casu" vytvoreni FS
    DWORD PluginFSCreateTime;  // "cas" vytvoreni tohoto FS (0 == neinicializovana hodnota)

    int ChngDrvDuplicateItemIndex; // poradove cislo duplicitni polozky v Change Drive menu (nepouziva se pokud menu neobsahuje duplicitni polozku k polozce pro tento FS) (0 = neinicializovana hodnota)

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

    // je zapouzdreni inicializovano?
    BOOL NotEmpty() { return Interface != NULL; }

    // inicializace zapouzdreni
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

    // zapouzdrujeme tento iface?
    BOOL Contains(CPluginFSInterfaceAbstract const* iface) { return iface != NULL && Interface == iface; }

    // vraci ukazatel na zapouzdreny interface
    CPluginFSInterfaceAbstract* GetInterface() { return Interface; }

    // vraci verzi Salamandera, pro kterou byl tento plugin nakompilovan (viz prehled verzi u LAST_VERSION_OF_SALAMANDER v spl_vers.h)
    int GetBuiltForVersion() { return BuiltForVersion; }

    // vraci odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril
    const char* GetDLLName() { return DLLName; }

    // vraci odkaz na retezec z CPluginData plug-inu, ktery iface vytvoril
    const char* GetVersion() { return Version; }

    // vraci iface plug-inu, ktery FS-iface vytvoril (cast pro FS)
    CPluginInterfaceForFSEncapsulation* GetPluginInterfaceForFS() { return &IfaceForFS; }

    // vraci iface plug-inu, ktery FS-iface vytvoril (zaklad)
    CPluginInterfaceAbstract* GetPluginInterface() { return Iface; }

    // vraci jmeno otevreneho FS
    const char* GetPluginFSName() { return PluginFSName; }

    // vraci index jmena otevreneho FS
    int GetPluginFSNameIndex() { return PluginFSNameIndex; }

    // zmena jmena FS
    void SetPluginFS(const char* fsName, int fsNameIndex)
    {
        lstrcpyn(PluginFSName, fsName, MAX_PATH);
        PluginFSNameIndex = fsNameIndex;
    }

    DWORD GetPluginFSCreateTime() { return PluginFSCreateTime; }

    int GetChngDrvDuplicateItemIndex() { return ChngDrvDuplicateItemIndex; }
    void SetChngDrvDuplicateItemIndex(int i) { ChngDrvDuplicateItemIndex = i; }

    // vraci TRUE pokud je 'fsName' ze stejneho pluginu jako tento FS; pokud vraci TRUE,
    // vraci i index 'fsNameIndex' jmena FS 'fsName' pluginu
    BOOL IsFSNameFromSamePluginAsThisFS(const char* fsName, int& fsNameIndex);

    // vraci TRUE pokud je cesta 'fsName':'fsUserPart' z tohoto FS ('fsName' je ze stejneho pluginu
    // jako tento FS a IsOurPath('fsName', 'fsUserPart') vraci TRUE)
    BOOL IsPathFromThisFS(const char* fsName, const char* fsUserPart);

    // vraci TRUE pokud je sluzba na FS podporovana
    BOOL IsServiceSupported(DWORD s) { return (SupportedServices & s) != 0; }

    // metody interfacu CPluginFSInterfaceAbstract bez "virtual" (je zbytecne, jen by zpomalovalo)

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

    // v debug verzi pocitame OpenedPDCounter, proto musi byt v .cpp modulu
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

    void Event(int event, DWORD param) // FIXME_X64 - nemel by 'param' umet podrzet x64 hodnotu?
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

    // POZOR: pouziva se vyhradne uvnitr sekce EnterPlugin+LeavePlugin, nevolat mimo tuto sekci!
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
                return FALSE; // at uzivatel opravi cilovou cestu (kopirovani/presun na tuto cestu neni podporovan)
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
    // POZOR: pred spustenim operace v plug-inu je potreba snizit prioritu threadu !!!
    // ********************************************************************************

private:
    // pouzivat spise IsServiceSupported() - SupportedServices je cachovano
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

// interface by mel byt pripraveny na opakovane volani (listing se nemusi
// povest napoprve (u FS) nebo se nemusi povest vubec), nebo zajistit aby
// se zmeny provedly az pokud se listing povede, nebo poznamenat v spl_xxxx.h,
// ze na tenhle iface je mozne sahat jen pokud bude vraceno TRUE z listingu (uspech)

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
    const char* DLLName; // odkaz na retezec z CPluginData plug-inu, ktery iface pouziva
    const char* Version; // odkaz na retezec z CPluginData plug-inu, ktery iface pouziva

public:
    CSalamanderDebug()
    {
        DLLName = NULL;
        Version = NULL;
    }

    // nutne volat behem SetBasicPluginData pri zmene retezcu
    void Init(const char* dllName, const char* version)
    {
        DLLName = dllName;
        Version = version;
    }

    // Implementace metod CSalamanderDebugAbstract:

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
    CProgressBar* Control; // zapouzdreny WinLib objekt

public:
    CGUIProgressBar() {}

    // pomocna: nastaveni zapouzdreneho WinLib objektu
    void SetControl(CProgressBar* control) { Control = control; }

    // Implementace metod CGUIProgressBarAbstract:
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
    CStaticText* Control; // zapouzdreny WinLib objekt

public:
    CGUIStaticText() {}

    // pomocna: nastaveni zapouzdreneho WinLib objektu
    void SetControl(CStaticText* control) { Control = control; }

    // Implementace metod CGUIStaticTextAbstract:
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
    CHyperLink* Control; // zapouzdreny WinLib objekt

public:
    CGUIHyperLink() {}

    // pomocna: nastaveni zapouzdreneho WinLib objektu
    void SetControl(CHyperLink* control) { Control = control; }

    // Implementace metod CGUIHyperLinkAbstract:
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
    CButton* Control; // zapouzdreny WinLib objekt

public:
    CGUIButton() {}

    // pomocna: nastaveni zapouzdreneho WinLib objektu
    void SetControl(CButton* control) { Control = control; }

    // Implementace metod CGUIButtonAbstract:
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
    CColorArrowButton* Control; // zapouzdreny WinLib objekt

public:
    CGUIColorArrowButton() {}

    // pomocna: nastaveni zapouzdreneho WinLib objektu
    void SetControl(CColorArrowButton* control) { Control = control; }

    // Implementace metod CGUIColorArrowButtonAbstract:
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
    CToolbarHeader* Control; // zapouzdreny WinLib objekt

public:
    CGUIToolbarHeader() {}

    // pomocna: nastaveni zapouzdreneho WinLib objektu
    void SetControl(CToolbarHeader* control) { Control = control; }

    // Implementace metod CGUIColorArrowButtonAbstract:
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

    // Implementace metod CSalamanderGUIAbstract:
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
    // pomocna funkce: otestuje, zda se 'control' povedlo alokovat a attachnout;
    // pokud ne, vola na ukazatel detete a vraci FALSE
    BOOL CheckControlAndDeleteOnError(CWindow* control);
};

class CSalamanderPasswordManager : public CSalamanderPasswordManagerAbstract
{
private:
    const char* DLLName; // odkaz na retezec z CPluginData plug-inu, ktery iface pouziva

public:
    CSalamanderPasswordManager()
    {
        DLLName = NULL;
    }

    // nutne volat behem SetBasicPluginData pri zmene retezcu
    void Init(const char* dllName)
    {
        DLLName = dllName;
    }

    // Implementace metod CSalamanderPasswordManagerAbstract:
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
    CPluginInterfaceAbstract* Plugin; // plug-in, ktery iface pouziva, !!! POZOR: muze byt i NULL
                                      // (plug-in neni naloaden) nebo -1 (behem entry-pointu plug-inu)

    char HelpFileName[MAX_PATH]; // pokud neni prazdne, jde o jmeno (bez cesty) .chm souboru s napovedou pouzivaneho timto pluginem (jen optimalizace, nikam se neuklada)

public:
    HINSTANCE LanguageModule; // pokud neni NULL, jde o handle .SLG jazykoveho modulu pluginu

public:
    CSalamanderGeneral();
    ~CSalamanderGeneral();

    // nutne volat hned po entry-pointu plug-inu
    void Init(CPluginInterfaceAbstract* plugin) { Plugin = plugin; }

    // vola se po unloadu pluginu - priprava dat pro dalsi load pluginu
    void Clear();

    // pomocna nevirtualni funkce pro ziskani ukazatele na panel podle PATH_TYPE_XXX
    CFilesWindow* WINAPI GetPanel(int panel);

    // Implementace metod CSalamanderGeneralAbstract:

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
    pmitItemOrSeparator, // polozka nebo separator (Name neni nebo je NULL)
    pmitStartSubmenu,    // zacatek submenu (polozka umoznujici otevrit submenu, ktere obsahuje vsechny polozky z pole polozek menu az do symbolu konce submenu)
    pmitEndSubmenu,      // symbol konce submenu (pouziva se jen pri stavbe menu, ostatni data v CPluginMenuItem jsou prazdna)
};

// pokud ma HotKey nastaven tento bit, zmenil tuto klavesu uzivatel v konfiguraci a behem
// Plugin::Connect()/AddMenuItems ji musime zachovat
#define HOTKEY_DIRTY 0x00010000
#define HOTKEY_HINT 0x00020000 // 'Name' obsahuje za znakem '\t' horkou klavesu, ktera bude zobrazena v menu, pokud uzivatel nepriradi commandu vlastni klavesu
#define HOTKEY_MASK 0x0000FFFF
#define HOTKEY_GET(x) (WORD)(x & HOTKEY_MASK)
#define HOTKEY_GETDIRTY(x) ((x & HOTKEY_DIRTY) != 0)

struct CPluginMenuItem
{
public:
    CPluginMenuItemType Type; // typ polozky, viz popis u CPluginMenuItemType
    int IconIndex;            // index ikony v bitmape s ikonami pluginu; -1=zadna ikona; POZOR: index neni overovan (muze byt neplatny)
    char* Name;               // jmeno polozky v menu, je-li NULL jde o separator
    DWORD StateMask;          // hiword je or-maska, loword je and-maska, pokud je -1, pouziva se
                              // CPluginInterfaceAbstract::GetMenuItemState
    DWORD SkillLevel;         // ktere urovni uzivatelu mame ukazovat tuto polozku MENU_SKILLLEVEL_XXX
    int ID;                   // plug-in-UID - unikatni cislo itemy v ramci plug-inu
    DWORD HotKey;             // horka klavesa: LOWORD=hotkey(LOBYTE:vk, HIBYTE:mods), HIWORD=(0:uzivatel ji nemenil, 1:uzivatel ji zmenil, je dirty)

    // pomocna data:
    int SUID; // salamander-UID - unikatni cislo v ramci Salamandera, pocita se pri plneni menu

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
    int BuiltForVersion;       // platne jen kdyz je plugin naloadeny: verze Salamandera, pro kterou byl tento plugin nakompilovan (viz prehled verzi u LAST_VERSION_OF_SALAMANDER v spl_vers.h)
    char* Name;                // jmeno plug-inu - zobrazeno v dialozich Extensions/C.Packers/C.Unpackers
                               // max. delka jmena MAX_PATH - 1
    char* DLLName;             // jmeno DLL souboru, relativni k "plugins" nebo absolutni
                               // max. delka jmena MAX_PATH - 1
    BOOL SupportPanelView;     // TRUE => umi ListArchive, UnpackArchive, UnpackOneFile (panel archivator/view)
    BOOL SupportPanelEdit;     // TRUE => umi PackToArchive, DeleteFromArchive (panel archivator/edit)
    BOOL SupportCustomPack;    // TRUE => umi PackToArchive (custom archivator/pack)
    BOOL SupportCustomUnpack;  // TRUE => umi UnpackWholeArchive (custom archivator/unpack)
    BOOL SupportConfiguration; // TRUE => umi Configuration (lze konfigurovat)
    BOOL SupportLoadSave;      // TRUE => umi load/save do registry (perzistence)
    BOOL SupportViewer;        // TRUE => umi ViewFile a CanViewFile (file viewer)
    BOOL SupportFS;            // TRUE => umi FS (file system)
    BOOL SupportDynMenuExt;    // TRUE => menu se pridava v PluginIfaceForMenuExt::BuildMenu misto v PluginIface::Connect (menu je dynamicke, stavi se znovu pred kazdym otevrenim menu pluginu)

    BOOL LoadOnStart; // ma se plug-in loadit pri kazdem startu Salamandera?

    char* Version;                // verze plug-inu (max. delka MAX_PATH - 1)
    char* Copyright;              // copyright vyrobce (max. delka MAX_PATH - 1)
    char* Description;            // strucny popis plug-inu (max. delka MAX_PATH - 1)
    char* RegKeyName;             // jmeno klice v registry - konfigurace (max. delka MAX_PATH - 1)
    char* Extensions;             // pripony archivu oddelene ';' (max. delka MAX_PATH - 1)
    TIndirectArray<char> FSNames; // pole jmen file systemu pluginu (kazde ma max. delku MAX_PATH - 1)

    char* LastSLGName; // jmeno posledniho pouziteho .SLG souboru (NULL = zatim zadny nebo shodny jazyk se Salamanderem)

    char* PluginHomePageURL; // URL na home-page pluginu (NULL == zadna home-page neexistuje)

    char* ChDrvMenuFSItemName;    // prikaz FS v change-drive menu: jmeno (max. delka MAX_PATH - 1)
    int ChDrvMenuFSItemIconIndex; // prikaz FS v change-drive menu: index ikony (v PluginIcons; -1=zadna ikona; index ikony neni kontrolovan - muze byt neplatny)
    BOOL ChDrvMenuFSItemVisible;  // prikaz FS v change-drive menu: je viditelny? (user muze potlacit jeho zobrazovani z Plugins Manageru)

    TIndirectArray<CPluginMenuItem> MenuItems; // pole polozek v menu
    BOOL DynMenuWasAlreadyBuild;               // TRUE = BuildMenu() uz se volala, proto se jeho dalsi volani maji ignorovat

    char* BugReportMessage; // zprava, kterou ma zobrazit Bug Report dialog, je-li exceptiona v plug-inu
    char* BugReportEMail;   // e-mail, ktery ma zobrazit Bug Report dialog, je-li exceptiona v plug-inu

    CMaskGroup ThumbnailMasks;   // masky urcujici soubory, kterym plugin umi vytvorit thumbnaily (max. delka MAX_GROUPMASK - 1) - NULL == plugin zadne thumbnaily negeneruje
    BOOL ThumbnailMasksDisabled; // TRUE jen pokud probiha unload/remove pluginu

    BOOL ArcCacheHaveInfo;    // TRUE = uz mame informace o pouzivani disk-cache v archivu pluginu (bylo volano CPluginInterfaceForArchiverAbstract::GetCacheInfo)
    char* ArcCacheTmpPath;    // umisteni kopii souboru z archivu: NULL = TEMP, jinak cesta (root-dir cache)
    BOOL ArcCacheOwnDelete;   // TRUE = pro mazani kopii z cache volat CPluginInterfaceForArchiverAbstract::DeleteTmpCopy()
    BOOL ArcCacheCacheCopies; // TRUE = crtCache (mazat pri zavreni archivu + na limitu cache), FALSE = crtDirect (mazat hned jak neni potreba)

    CIconList* PluginIcons;     // ikony pluginu pro GUI (menu, toolbars, dialogy) (vlastnikem je Salamander + prezije unload pluginu)
    CIconList* PluginIconsGray; // cernobila verze

    CIconList* PluginDynMenuIcons; // ikony pro dynamicke menu, viz SupportDynMenuExt (vlastnikem je Salamander)

    int PluginIconIndex;          // index ikony pluginu (okno Plugins/Plugins + menu Help/About Plugin + pripadne submenu pluginu v menu Plugins (detaily viz PluginSubmenuIconIndex)) (index v PluginIcons; -1=zadna ikona; index ikony neni kontrolovan - muze byt neplatny)
    int PluginSubmenuIconIndex;   // index ikony submenu pluginu v menu Plugins a pripadne drop-down buttonu pro submenu na top-toolbare (index v PluginIcons; -1=pouzit PluginIconIndex; index ikony neni kontrolovan - muze byt neplatny)
    BOOL ShowSubmenuInPluginsBar; // TRUE = v top-toolare se ma ukazat drop-down button se submenu pluginu s ikonou PluginSubmenuIconIndex

    BOOL PluginIsNethood;           // TRUE = plugin je nahrada za Network polozku z Change Drive menu (pridano pro plugin Nethood)
    BOOL PluginUsesPasswordManager; // TRUE = plugin pouziva Password Manager

    int IconOverlaysCount; // pocet trojic icon-overlays v poli IconOverlays
    HICON* IconOverlays;   // alokovane pole icon-overlays, pocet prvku = 3 * IconOverlaysCount (velikosti: 16 + 32 + 48)

    // pomocna data:
    CMenuPopup* SubMenu; // ukazatel na submenu nalezici tomuto plug-inu, NULL - neexistuje

    CSalamanderDebug SalamanderDebug;                     // objekt s TRACE + CALL_STACK pro tento plug-in
    CSalamanderGeneral SalamanderGeneral;                 // objekt s obecne pouzitelnymi metodami pro tento plug-in
    CSalamanderGUI SalamanderGUI;                         // objekt poskytujici upravene Windows controly pouzivane v Salamanderovi
    CSalamanderPasswordManager SalamanderPasswordManager; // objekt s pristupem do password manageru pro tento plug-in

    BOOL ShouldUnload;      // TRUE pokud se ma plug-in unloadnout pri nejblizsi mozne prilezitosti
    BOOL ShouldRebuildMenu; // TRUE pokud se ma rebuildnout menu plug-inu pri nejblizsi mozne prilezitosti

    // prikazy, o ktere si pozadal tento plug-in (cekaji na spusteni v "sal-idle"):
    // <0, 499> - prikazy Salamandera
    // <500, 1000499> - prikazy z menu pluginu (menu-extensiona)
    TDirectArray<DWORD> Commands;

    BOOL OpenPackDlg;                // TRUE = ma se otevrit Pack dialog pro tento plugin
    int PackDlgDelFilesAfterPacking; // Pack dialog: checkbox "Delete files after packing": 0=default, 1=zapnuty, 2=vypnuty
    BOOL OpenUnpackDlg;              // TRUE = ma se otevrit Unpack dialog pro tento plugin
    char* UnpackDlgUnpackMask;       // Unpack dialog: maska "Unpack files": NULL=default, jinak text masky (alokovany)

#ifdef _DEBUG
    int OpenedFSCounter; // pocty otevrenych FS ifacu
    int OpenedPDCounter; // pocty otevrenych PluginData ifacu
#endif

protected:
    HINSTANCE DLL;                                                         // handle DLL souboru plug-inu
    CPluginInterfaceEncapsulation PluginIface;                             // interface plug-inu (behem volani entry-pointu je -1)
    CPluginInterfaceForArchiverEncapsulation PluginIfaceForArchiver;       // interface plug-inu: archivator
    CPluginInterfaceForViewerEncapsulation PluginIfaceForViewer;           // interface plug-inu: viewer
    CPluginInterfaceForMenuExtEncapsulation PluginIfaceForMenuExt;         // interface plug-inu: rozsireni menu
    CPluginInterfaceForFSEncapsulation PluginIfaceForFS;                   // interface plug-inu: file-system
    CPluginInterfaceForThumbLoaderEncapsulation PluginIfaceForThumbLoader; // interface plug-inu: thumbnail loader

public:
    CPluginData(const char* name, const char* dllName, BOOL supportPanelView,
                BOOL supportPanelEdit, BOOL supportCustomPack, BOOL supportCustomUnpack,
                BOOL supportConfiguration, BOOL supportLoadSave, BOOL supportViewer,
                BOOL supportFS, BOOL supportDynMenuExt, const char* version,
                const char* copyright, const char* description, const char* regKeyName,
                const char* extensions, TIndirectArray<char>* fsNames, BOOL loadOnStart,
                char* lastSLGName, const char* pluginHomePageURL);
    ~CPluginData();

    BOOL IsGood() { return Name != NULL; } // probehl konstruktor o.k.?

    // vraci interface plug-inu
    CPluginInterfaceEncapsulation* GetPluginInterface() { return &PluginIface; }
    CPluginInterfaceForFSEncapsulation* GetPluginInterfaceForFS() { return &PluginIfaceForFS; }
    CPluginInterfaceForMenuExtEncapsulation* GetPluginInterfaceForMenuExt() { return &PluginIfaceForMenuExt; }
    CPluginInterfaceForArchiverEncapsulation* GetPluginIfaceForArchiver() { return &PluginIfaceForArchiver; }
    CPluginInterfaceForViewerEncapsulation* GetPluginInterfaceForViewer() { return &PluginIfaceForViewer; }
    CPluginInterfaceForThumbLoaderEncapsulation* GetPluginInterfaceForThumbLoader() { return &PluginIfaceForThumbLoader; }

    // nacte DLL do pameti, pripoji se a overi platnost zde ulozenych informaci (SupportXXX atd.),
    // nacte jen DLL, ktere presne odpovida, jinak nutna reinstalace plug-inu,
    // parent je "parent" okno pro messageboxy, je-li "quiet"==TRUE nevypisuji se chybove hlasky
    // (ale hlasky uvnitr plug-inu se vypisuji)
    // 'waitCursor' slouzi zobrazeni Wait kurzoru behem nacitani DLL knihovny
    // je-li 'showUnsupOnX64' TRUE, ukazuje se u pluginu nepodporovanych na x64 messagebox
    // je-li 'releaseDynMenuIcons' TRUE, uvolni se ikony dynamickeho menu pluginu (pred otevrenim menu se totiz ziskavaji znovu)
    BOOL InitDLL(HWND parent, BOOL quiet = FALSE, BOOL waitCursor = TRUE, BOOL showUnsupOnX64 = TRUE,
                 BOOL releaseDynMenuIcons = TRUE);

    BOOL GetLoaded() { return DLL != NULL; }

    HINSTANCE GetPluginDLL() { return DLL; }

    // vraci jmeno plug-inu rozsirene o "(Plugin)", pouziva se tam, kde neni jasne, ze jde o plug-in
    // (napr. v combo-boxech v Archivers/Extensions, atd.)
    void GetDisplayName(char* buf, int bufSize);

    // volani plug-inu: uloz vlastni konfiguraci, parent je okno pro messageboxy
    void Save(HWND parent, HKEY regKeyConfig);

    // otevira konfiguracni dialog plug-inu, parent je okno pro messageboxy
    void Configuration(HWND parent);

    // otevira about dialog plug-inu, parent je okno pro messageboxy
    void About(HWND parent);

    // zavola 'loadOrSaveFunc' pro load/save konfigurace plug-inu (popis viz
    // CSalamanderGeneral::CallLoadOrSaveConfiguration
    void CallLoadOrSaveConfiguration(BOOL load, FSalLoadOrSaveConfiguration loadOrSaveFunc,
                                     void* param);

    // provede unload DLL, je-li 'ask' TRUE, zepta se na Save pokud je "save on exit" "on",
    // parent je okno pro messageboxy, je-li 'ask' FALSE provede pripadny Save bez ptani,
    // vraci TRUE pokud doslo k unloadu
    BOOL Unload(HWND parent, BOOL ask);

    // zjisti jestli je mozne plug-in odstranit, parent je okno pro messageboxy,
    // index je index tohoto plug-inu v Plugins, vraci TRUE pokud je mozne plug-in odstranit
    BOOL Remove(HWND parent, int index, BOOL canDelPluginRegKey);

    // prida polozku menu do MenuItems
    void AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState, DWORD state_or,
                     DWORD state_and, DWORD skillLevel, CPluginMenuItemType type);

    // prohleda polozky menu pluginu a hleda command s 'id'; pokud ho nalezne, nastavi podle nej
    // 'hotKey' (muze byt NULL) a 'hotKeyText' (muze byt NULL) a vrati TRUE; jinak vrati FALSE
    BOOL GetMenuItemHotKey(int id, WORD* hotKey, char* hotKeyText, int hotKeyTextSize);

    // inicializuje polozky menu tohoto plug-inu, 'parent' je okno pro messageboxy,
    // 'index' je index tohoto plug-inu v Plugins, 'menu' je submenu pro tento plug-in
    void InitMenuItems(HWND parent, int index, CMenuPopup* menu);
    // pomocna metoda: vytvari z pole MenuItems jeden level submenu (pro vnorene levely
    // submenu se vola rekurzivne); 'menu' je submenu, do ktereho se pridavaji polozky;
    // 'i' je aktualni index do pole MenuItems - pri navratu z metody je bud za koncem
    // pole nebo na polozce symbolu end-submenu; 'count' je pocet polozek v submenu
    // 'menu'; 'mask' je predpocitana state-maska
    void AddMenuItemsToSubmenuAux(CMenuPopup* menu, int& i, int count, DWORD mask);

    // vycisti SUID vsech polozek menu
    void ClearSUID();

    // volani plug-inu: ExecuteMenuItem
    // spousti prikaz menu s identifikacnim cislem 'suid' (reakce na WM_COMMAND),
    // 'panel' je panel, pro ktery se ma provest prikaz z menu (pouziva se pro MoveFiles),
    // 'parent' je "parent" okno messageboxu, 'index' je index tohoto plug-inu v Plugins,
    // vraci TRUE pokud 'suid' byl nalezen mezi polozkami menu tohoto plug-inu,
    // vraci 'unselect'=navratova hodnota volani plug-inu ExecuteMenuItem
    BOOL ExecuteMenuItem(CFilesWindow* panel, HWND parent, int index, int suid, BOOL& unselect);
    // 'id' je interni ID commandu; ExecuteMenuItem2 se pouziva pro Last Command
    BOOL ExecuteMenuItem2(CFilesWindow* panel, HWND parent, int index, int id, BOOL& unselect);

    // volani plug-inu: HelpForMenuItem
    // zobrazi help k prikazu menu s identifikacnim cislem 'suid' (reakce na WM_COMMAND),
    // 'parent' je "parent" okno messageboxu, 'index' je index tohoto plug-inu v Plugins,
    // vraci TRUE pokud 'suid' byl nalezen mezi polozkami menu tohoto plug-inu,
    // vraci 'helpDisplayed'=navratova hodnota volani plug-inu HelpForMenuItem
    BOOL HelpForMenuItem(HWND parent, int index, int suid, BOOL& helpDisplayed);

    // volani plug-inu: BuildMenu
    // nechame plugin sestavit nove menu, 'parent' je "parent" okno messageboxu;
    // je-li 'force' TRUE, ignoruje se 'DynMenuWasAlreadyBuild' a menu se sestavi vzdy;
    // vraci TRUE pokud je plugin nacteny a ma staticke menu nebo ma dynamicke menu a
    // zaroven vraci rozhrani menu-ext
    BOOL BuildMenu(HWND parent, BOOL force);

    // volani plug-inu: ListArchive
    BOOL ListArchive(CFilesWindow* panel, const char* archiveFileName, CSalamanderDirectory& dir,
                     CPluginDataInterfaceAbstract*& pluginData);

    // volani plug-inu: UnpackArchive
    BOOL UnpackArchive(CFilesWindow* panel, const char* archiveFileName,
                       CPluginDataInterfaceAbstract* pluginData,
                       const char* targetDir, const char* archiveRoot,
                       SalEnumSelection nextName, void* param);

    // volani plug-inu: UnpackOneFile
    BOOL UnpackOneFile(CFilesWindow* panel, const char* archiveFileName,
                       CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                       const CFileData* fileData, const char* targetDir,
                       const char* newFileName, BOOL* renamingNotSupported);

    // volani plug-inu: PackToArchive
    BOOL PackToArchive(CFilesWindow* panel, const char* archiveFileName,
                       const char* archiveRoot, BOOL move, const char* sourceDir,
                       SalEnumSelection2 nextName, void* param);

    // volani plug-inu: DeleteFromArchive
    BOOL DeleteFromArchive(CFilesWindow* panel, const char* archiveFileName,
                           CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                           SalEnumSelection nextName, void* param);

    // volani plug-inu: UnpackWholeArchive
    BOOL UnpackWholeArchive(CFilesWindow* panel, const char* archiveFileName, const char* mask,
                            const char* targetDir, BOOL delArchiveWhenDone,
                            CDynamicString* archiveVolumes);

    // volani plug-inu: CanCloseArchive
    BOOL CanCloseArchive(CFilesWindow* panel, const char* archiveFileName, BOOL force);

    // volani plug-inu: CanViewFile
    BOOL CanViewFile(const char* name);

    // volani plug-inu: ViewFile
    BOOL ViewFile(const char* name, int left, int top, int width, int height,
                  UINT showCmd, BOOL alwaysOnTop, BOOL returnLock,
                  HANDLE* lock, BOOL* lockOwner,
                  int enumFilesSourceUID, int enumFilesCurrentIndex);

    // volani plug-inu: OpenFS
    CPluginFSInterfaceAbstract* OpenFS(const char* fsName, int fsNameIndex);

    // volani plug-inu: ExecuteChangeDriveMenuItem
    void ExecuteChangeDriveMenuItem(int panel);

    // volani plug-inu: ChangeDriveMenuItemContextMenu
    BOOL ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                        CPluginFSInterfaceAbstract* pluginFS,
                                        const char* pluginFSName, int pluginFSNameIndex,
                                        BOOL isDetachedFS, BOOL& refreshMenu,
                                        BOOL& closeMenu, int& postCmd, void*& postCmdParam);

    // volani plug-inu: EnsureShareExistsOnServer
    void EnsureShareExistsOnServer(HWND parent, int panel, const char* server, const char* share);

    // pokud je plugin loaded, volani plug-inu: Event
    void Event(int event, DWORD param);

    // volani plug-inu: ClearHistory
    void ClearHistory(HWND parent);

    // pokud je plugin loaded, volani plug-inu: AcceptChangeOnPathNotification
    void AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    // volani plug-inu: PasswordManagerEvent
    void PasswordManagerEvent(HWND parent, int event);

    // ziskani nastaveni disk-cache pro archiver - pouziva volani pluginu
    void GetCacheInfo(char* arcCacheTmpPath, BOOL* arcCacheOwnDelete, BOOL* arcCacheCacheCopies);

    // volani plug-inu: DeleteTmpCopy
    void DeleteTmpCopy(const char* fileName, BOOL firstFile);

    // volani plug-inu: PrematureDeleteTmpCopy
    BOOL PrematureDeleteTmpCopy(HWND parent, int copiesCount);

    // vraci TRUE pokud je plugin archivator a ma vlastni mazani kopii souboru vybalenych z archivu
    // musi fungovat i po unloadu pluginu (az do pristiho loadu pluginu)
    BOOL IsArchiverAndHaveOwnDelete() { return ArcCacheOwnDelete; }

    HIMAGELIST CreateImageList(BOOL gray);

    // naplni strukturu 'mii::State' a 'mii::Type' podle commandu urcneho 'pluginIndex' a 'menuItemIndex'
    // vraci pokud TRUE, pokud je polozka enabled a FALSE pokud je GRAYED
    BOOL GetMenuItemStateType(int pluginIndex, int menuItemIndex, MENU_ITEM_INFO* mii);

    // synchronizuje stare (z konfigurace) a nove (z Connect()) hot keys:
    // z pole 'oldMenuItems' vytaha hot keys, ktere maji nastaven dirty bit a prenese je do pole 'MenuItems'
    // synchronizace se ridi ID hodnotou commandu
    void HotKeysMerge(TIndirectArray<CPluginMenuItem>* oldMenuItems);

    // zajisteni integrity horkych klaves (nezadouci nastavi na 0):
    // - hot key nesmi patrit Salamu
    // - hot key nesmi patrit jinemu pluginu
    // - hot key se v ramci jednoho submenu nesmi opakovat
    void HotKeysEnsureIntegrity();

    // uvolni a vynuluje 'PluginDynMenuIcons'
    void ReleasePluginDynMenuIcons();

    // uvolni a vynuluje 'IconOverlays' a 'IconOverlaysCount'
    void ReleaseIconOverlays();

protected:
    // vrati mask (0 nebo kombinace MENU_EVENT_xxx) pro tento plugin a soucasny stav
    // ActualStateMask, ActiveUnpackerIndex, ActivePackerIndex...
    // 'index' je index tohoto plug-inu v Plugins
    DWORD GetMaskForMenuItems(int index);

    friend class CCallStack;
};

// Prehled objektu spojenych s archivatory/viewry:
//   CPackerFormatConfig - seznam pripon archivu, k nim asociovane panelove archivatory view/edit,
//                         'PackerIndex' (edit) a 'UnpackerIndex' (view), viz nize
//   CArchiverConfig - externi panelove archivatory (data pro moduly packX.cpp)
//   CPackerConfig - custom pack - Alt+F5 - externi i interni pakovace
//   CUnpackerConfig - custom unpack - Alt+F6 - externi i interni rozpakovavace
//   CPlugins - seznam plug-inu archivatoru/viewru
//
// 'PackerIndex' a 'UnpackerIndex' v CPackerFormatConfig:
//   i < 0  => (-i - 1) je index v CPlugins plug-inu, ktery je pouzit pro "panel-arch. view/edit"
//   i >= 0 => i je index v CArchiverConfig pro externi "panel-arch. view/edit"
//
// 'Type' v CPackerConfig a CUnpackerConfig:
//   i == 0 => jde o externi archivator, pouzivaji se data pro externi "custom pack/unpack"
//   i < 0 => (-i - 1) je index v CPlugins plug-inu, ktery je pouzit pro "custom pack/unpack"
//
// 'ViewerType' v CViewerMasks:
//   i == 0 => jde o externi viewer, pouzivaji se data pro externi "file viewer"
//   i == 1 => jde o interni text/hex viewer
//   i < 0 => (-i - 1) je index v CPlugins plug-inu, ktery je pouzit pro "file viewer"

class CMenuPopup;
class CDrivesList;

struct CPluginsStateCache
{
    DWORD ActualStateMask;      // predpocitana maska pro CPluginMenuItem::StateMask
    int ActiveUnpackerIndex;    // predpocitany index "unpacker" pro aktivni panel, -1 => neplatne
    int ActivePackerIndex;      // predpocitany index "packer" pro aktivni panel, -1 => neplatne
    int NonactiveUnpackerIndex; // predpocitany index "unpacker" pro neaktivni panel, -1 => neplatne
    int NonactivePackerIndex;   // predpocitany index "packer" pro neaktivni panel, -1 => neplatne
    int FileUnpackerIndex;      // predpocitany index "unpacker" pro fokuseny soubor, -1 => neplatne
    int FilePackerIndex;        // predpocitany index "packer" pro fokuseny soubor, -1 => neplatne
    int ActiveFSIndex;          // predpocitany index "FS" pro aktivni panel, -1 => neplatne
    int NonactiveFSIndex;       // predpocitany index "FS" pro neaktivni panel, -1 => neplatne

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

// CPluginOrder slouzi pro urceni poradi, v jakem budou pluginy zobrazeny (menu, plugin bar, ...)
struct CPluginOrder
{
    char* DLLName; // jmeno DLL souboru, relativni k "plugins" nebo absolutni

    // docasne promenne, neukladaji se do registry
    BOOL ShowInBar; // pouze pro konverzi stare konfigurace
    int Index;      // index v CPlugins::Data
    DWORD Flags;    // docasna pomocna promenna pro UpdatePluginsOrder
};

struct CPluginFSTimer
{
    DWORD AbsTimeout;                       // absolutni cas timeoutu timeru
    CPluginFSInterfaceAbstract* TimerOwner; // FS-objekt, kteremu se ma hlasit timeout timeru
    DWORD TimerParam;                       // parametr timeru (data pluginu)

    DWORD TimerAddedTime; // "cas" pridani timeru (zamezeni nekonecneho cyklu uvnitr CPlugins::HandlePluginFSTimers())

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
    CRITICAL_SECTION DataCS; // sekce pouzita jen pro synchronizaci dat pouzivanych metodou GetIndex()

    TDirectArray<CPluginOrder> Order; // poradi pro zobrazovani pluginu

    BOOL DefaultConfiguration; // TRUE => ZIP+TAR+PAK, je mozne prekodovat stare archivator-data

    TIndirectArray<CPluginFSTimer> PluginFSTimers;    // timery jednotlivych pluginovych FS
    DWORD TimerTimeCounter;                           // "cas" pro pridavani timeru (slouzi k zamezeni nekonecneho cyklu uvnitr CPlugins::HandlePluginFSTimers())
    BOOL StopTimerHandlerRecursion;                   // brani rekurzivnimu volani HandlePluginFSTimers()
    CPluginFSInterfaceEncapsulation* WorkingPluginFS; // pracovni objekt pluginoveho FS (zatim neni ani v panelu, ani mezi odpojenymi FS)

    // LastPlgCmdXXX udrzuji informaci o poslednim spustenem commandu z menu Plugins
    // pokud je LastPlgCmdPlugin == NULL, nema LastPlgCmdID vyznam a polozka v menu bude disabled s default textem
    char* LastPlgCmdPlugin; // cesta k pluginu, jemuz command patril (CPluginData::DLLName)
    int LastPlgCmdID;       // interni ID commandu (CPluginMenuItem::ID)

public:                     // pomocne promenne pro osetreni polozek menu z plug-inu:
    int LastSUID;           // pomocna promenna pro generovani SUID pro menuitemy
    int RootMenuItemsCount; // puvodni pocet polozek v root menu pro menu plug-inu, -1 je "zatim nezjisten"

    CPluginsStateCache StateCache;

    DWORD LoadInfoBase; // zaklad pro tvorbu flagu vraceneho pres CSalamanderPluginEntry::GetLoadInformation()

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
        // LastPlgCmdID incializace nema vyznam
        TimerTimeCounter = 0;
        StopTimerHandlerRecursion = FALSE;
        WorkingPluginFS = NULL;
    }
    ~CPlugins();

    void EnterDataCS() { HANDLES(EnterCriticalSection(&DataCS)); }
    void LeaveDataCS() { HANDLES(LeaveCriticalSection(&DataCS)); }

    // nacte objekt z registry klic regKey nebo default hodnoty (regKey==NULL) (ZIP + TAR + PAK)
    // parent je okno pro messageboxy (muze byt i NULL)
    void Load(HWND parent, HKEY regKey);
    void LoadOrder(HWND parent, HKEY regKey); // nacte poradi pluginu do pole Order

    // ulozi objekt do registry (info o plug-inech + vlastni konfigurace plug-inu),
    // parent je okno pro messageboxy
    void Save(HWND parent, HKEY regKey, HKEY regKeyConfig, HKEY regKeyOrder);

    // vycisti zaznamy o plug-inech (pak je treba volat CheckData)
    void Clear()
    {
        HANDLES(EnterCriticalSection(&DataCS));
        Data.DestroyMembers();
        HANDLES(LeaveCriticalSection(&DataCS));
        DefaultConfiguration = FALSE;
    }

    // upravi vsechny datove struktury tykajici se archivu -> zajisti konzistenci dat
    void CheckData();

    // vyhazi z Data pluginy, jejichz .spl prestalo existovat; je-li 'canDelPluginRegKey'
    // TRUE, podmazne tez jejich konfiguraci v registry; v 'notLoadedPluginNames' (buffer
    // o velikosti 'notLoadedPluginNamesSize') vraci seznam jmen (maximalne 'maxNotLoadedPluginNames'
    // jmen) nenactenych pluginu s konfiguraci v registry (bud vyhozenych nebo jim selhalo
    // InitDLL()) oddeleny ", " + v 'numOfSkippedNotLoadedPluginNames' (neni-li NULL) vraci pocet
    // jmen, ktere nejsou uvedena v 'notLoadedPluginNames'; v 'loadAllPlugins' je TRUE jen
    // pokud jde o upgrade na novou verzi Salama a mame nacist vsechny pluginy a ty co
    // nejdou a zaroven maji konfiguraci v registry mame ulozit do 'notLoadedPluginNames'
    void RemoveNoLongerExistingPlugins(BOOL canDelPluginRegKey, BOOL loadAllPlugins = FALSE,
                                       char* notLoadedPluginNames = NULL,
                                       int notLoadedPluginNamesSize = 0,
                                       int maxNotLoadedPluginNames = 0,
                                       int* numOfSkippedNotLoadedPluginNames = NULL,
                                       HWND parent = NULL);

    // obstara automatickou instalaci pluginu ze std. adresare "plugins" (prida jen
    // dosud nepridane) + automaticky odinstaluje pluginy, kterym prestaly existovat
    // .spl soubory
    void AutoInstallStdPluginsDir(HWND parent);

    // obstara pridani nove doinstalovanych plug-inu (nacte plugins.ver); vraci TRUE pokud byla
    // nalezena nova verze souboru plugins.ver (je treba ulozit konfiguraci, aby se pri pristim
    // spusteni Salama vse neopakovalo)
    BOOL ReadPluginsVer(HWND parent, BOOL importFromOldConfig);

    // obstara load vsech pluginu a zavolani jejich metody ClearHistory
    void ClearHistory(HWND parent);

    // zkusi load vsech pluginu (tlacitko Test All v dialogu Plugins), vraci TRUE pokud se
    // aspon jeden plugin nove naloadil
    BOOL TestAll(HWND parent);

    // zkusi load vsech pluginu (pri prvnim spusteni Salama po zmene jazyka)
    void LoadAll(HWND parent);

    // provede load vsech plug-inu s flagem load-on-start
    void HandleLoadOnStartFlag(HWND parent);

    // provede rebuild menu a unload vsech plug-inu, ktere si pozadaly; vraci cislo prvniho prikazu Salamandera/menu,
    // o ktery plug-iny pozadaly (WM_COMMAND se okamzite doruci hlavnimu oknu) + prislusny plugin (v 'data')
    void GetCmdAndUnloadMarkedPlugins(HWND parent, int* cmd, CPluginData** data);

    // vraci plugin, ktery zada o otevreni Pack/Unpack dialogu; pokud zadny neni, vraci v 'data'
    // NULL a v 'pluginIndex' -1
    void OpenPackOrUnpackDlgForMarkedPlugins(CPluginData** data, int* pluginIndex);

    // vraci postupne salamand.exe a vsechny plug-iny vcetne verzi, index jde od nuly (in/out)
    // vraci TRUE pokud vysledek plati
    BOOL EnumInstalledModules(int* index, char* module, char* version);

    // prida plug-in, vraci uspech
    BOOL AddPlugin(const char* name, const char* dllName, BOOL supportPanelView,
                   BOOL supportPanelEdit, BOOL supportCustomPack, BOOL supportCustomUnpack,
                   BOOL supportConfiguration, BOOL supportLoadSave, BOOL supportViewer,
                   BOOL supportFS, BOOL supportDynMenuExt, const char* version,
                   const char* copyright, const char* description, const char* regKeyName,
                   const char* extensions, TIndirectArray<char>* fsNames, BOOL loadOnStart,
                   char* lastSLGName, const char* pluginHomePageURL);

    // prida plug-in, 'parent' je "parent" okno messageboxu, 'fileName' je jmeno DLL souboru
    // plug-inu, vraci TRUE pokud je plug-in pridan
    BOOL AddPlugin(HWND parent, const char* fileName);

    // odstrani plug-in, 'parent' je "parent" okno messageboxu, udrzuje konzistenci dat,
    // vraci uspech
    BOOL Remove(HWND parent, int index, BOOL canDelPluginRegKey);

    // Salamander konci, plug-iny by mely taky koncit, vraci usech (TRUE = plug-iny jsou unloadeny)
    BOOL UnloadAll(HWND parent);

    // do 'uniqueKeyName' ulozi unikatni jmeno klice v registry pro "soukrome" data
    // plug-inu, jmeno je zalozene na 'regKeyName'
    void GetUniqueRegKeyName(char* uniqueKeyName, const char* regKeyName);

    // do 'uniqueFSName' ulozi unikatni jmeno FS, jmeno je zalozene na 'fsName';
    // 'uniqueFSNames' (neni-li NULL) je pole jmen, ke kterym ma byt vysledne
    // 'uniqueFSName' take unikatni; 'oldFSNames' (neni-li NULL) je pole starych
    // (z predchoziho loadu pluginu) fs-names, unikatni jmeno FS se prednostne
    // vybira a odstranuje z tohoto pole (aby se fs-name uzivateli nemenil pri
    // kazdem loadu pluginu)
    void GetUniqueFSName(char* uniqueFSName, const char* fsName,
                         TIndirectArray<char>* uniqueFSNames,
                         TIndirectArray<char>* oldFSNames);

    // vraci pocet pluginu
    int GetCount() { return Data.Count; }

    // vraci data o plug-inu z indexu 'index'
    // POZOR: ukazatel je platny jen do pristi zmeny poctu plug-inu (pole se roztahuje/smrstuje)
    CPluginData* Get(int index)
    {
        if (index >= 0 && index < Data.Count)
            return Data[index];
        else
            return NULL;
    }

    // vraci index pluginu v Data, -1 pokud neni nalezen; pouziti jen pro CSalamanderConnect
    // a CSalamanderBuildMenu, jinak pokud mozno CPluginData jako ukazatel neukladat - tudiz
    // nedohledavat podle nej)
    // POZOR: index je platny jen do pristi zmeny poctu plug-inu (pole se roztahuje/smrstuje)
    int GetIndexJustForConnect(const CPluginData* plugin);

    // vraci index pluginu v Data, -1 pokud neni nalezen
    // POZOR: index je platny jen do pristi zmeny poctu plug-inu (pole se roztahuje/smrstuje)
    // mozne volat z libovolneho threadu
    int GetIndex(const CPluginInterfaceAbstract* plugin);

    // vraci CPluginData obsahujici iface 'plugin', pokud neexistuje vraci NULL
    // POZOR: ukazatel je platny jen do pristi zmeny poctu plug-inu (pole se roztahuje/smrstuje)
    CPluginData* GetPluginData(const CPluginInterfaceForFSAbstract* plugin);

    // vraci CPluginData obsahujici iface 'plugin', pokud neexistuje vraci NULL
    // POZOR: ukazatel je platny jen do pristi zmeny poctu plug-inu (pole se roztahuje/smrstuje)
    CPluginData* GetPluginData(const CPluginInterfaceAbstract* plugin, int* lastIndex = NULL);

    // vraci CPluginData obsahujici DLLName==dllName (DLLName se alokuje jen jednou -> jednoznacne
    // identifikuje plug-in), pokud neexistuje vraci NULL
    // POZOR: ukazatel je platny jen do pristi zmeny poctu plug-inu (pole se roztahuje/smrstuje)
    CPluginData* GetPluginData(const char* dllName);

    // vraci CPluginData obsahujici DLLName koncici na 'dllSuffix', pokud neexistuje vraci NULL
    // POZOR: ukazatel je platny jen do pristi zmeny poctu plug-inu (pole se roztahuje/smrstuje)
    CPluginData* GetPluginDataFromSuffix(const char* dllSuffix);

    // vytvori imagelist (barevny je-li gray == FALSE, jinak v odstinech sede)
    // kazdy plugin je v imagelistu reprezentovan jednou ikonkou
    // pokud plugin nema svou ikonu, bude vlozena defaultni ikona (zastrcka)
    HIMAGELIST CreateIconsList(BOOL gray);

    // prida vsechna jmena plug-inu do listview; pokud je setOnly, pouze nastavi hodnoty;
    // v 'numOfLoaded' vraci pocet naloadenych pluginu
    void AddNamesToListView(HWND hListView, BOOL setOnly, int* numOfLoaded);

    // prida ukazatele na CPluginData pluginu, ktere poskytuji thumbnaily
    // POZOR: ukazatele jsou platne jen do pristi zmeny poctu plug-inu (pole se roztahuje/smrstuje)
    void AddThumbLoaderPlugins(TIndirectArray<CPluginData>& thumbLoaderPlugins);

    // prida jmena plug-inu do menu
    // pocet omezi promennou maxCount
    // commandy nastavi na firstID + itemIndex
    // 'configurableOnly' pokud je TRUE, budou zarazeny pouze pluginy podporujici konfiguraci
    //                    jinak budou pridana vsechna jmena
    // vraci TRUE, pokud byl pridan alespon jeden plugin a maji se priradit Alt+? klavesy
    BOOL AddNamesToMenu(CMenuPopup* menu, DWORD firstID, int maxCount, BOOL configurableOnly);

    // prida prikazy FS do change-drive menu, vraci uspech
    BOOL AddItemsToChangeDrvMenu(CDrivesList* drvList, int& currentFSIndex,
                                 CPluginInterfaceForFSAbstract* ifaceForFS,
                                 BOOL getGrayIcons);

    // otevre about dialog pluginu
    void OnPluginAbout(HWND hParent, int index);

    // otevre konfiguracni dialog pluginu (pokud je podporovan)
    void OnPluginConfiguration(HWND hParent, int index);

    // reaguje na WM_USER_INITMENUPOPUP pro Plugins menu
    // 'parent' je "parent" okno messageboxu, 'root' je menu Plugins
    void InitMenuItems(HWND parent, CMenuPopup* root);

    // reaguje na WM_USER_INITMENUPOPUP pro submenu jednotlivych pluginu z menu Plugins
    // 'parent' je "parent" okno messageboxu, 'submenu' je menu konkretniho pluginu
    void InitSubMenuItems(HWND parent, CMenuPopup* submenu);

    // inicializuje polozky menu pluginu, 'parent' je okno pro messageboxy,
    // 'index' je index pluginu v Plugins, 'menu' je submenu pro tento plugin
    // vraci TRUE v pripade uspechu, jinak vraci FALSE
    BOOL InitPluginMenuItemsForBar(HWND parent, int index, CMenuPopup* menu);

    // do toolbary napridava tlacitka pluginu, ktere maji menu a maji byt videt
    BOOL InitPluginsBar(CToolBar* bar);

    // spousti prikaz menu s identifikacnim cislem 'suid' (reakce na WM_COMMAND),
    // 'parent' je "parent" okno messageboxu, vraci TRUE pokud se ma zrusit oznaceni v panelu
    BOOL ExecuteMenuItem(CFilesWindow* panel, HWND parent, int suid);

    // zobrazi help k prikazu menu s identifikacnim cislem 'suid' (reakce na WM_COMMAND v HelpMode),
    // 'parent' je "parent" okno messageboxu, vraci TRUE pokud se help zobrazil
    BOOL HelpForMenuItem(HWND parent, int suid);

    // hleda plug-iny a externi archivatory podporujici "panel archiver view/edit" pro
    // alespon jednu z 'extensions', 'exclude' je index plug-inu, o ktery nestojice (bude zrusen),
    // 'view' a 'edit' jsou indexy nalezenych archivatoru (konvence 'PackerIndex' a 'UnpackerIndex'
    // z CPackerFormatConfig, viz vyse), 'viewFound' a 'editFound' rikaji jestli jsou 'view'
    // a 'edit' platne
    void FindViewEdit(const char* extensions, int exclude, BOOL& viewFound, int& view,
                      BOOL& editFound, int& edit);

    // hleda DLL soubor mezi DLL soubory plug-inu, pokud existuje, vraci TRUE a index plug-inu
    BOOL FindDLL(const char* dllName, int& index);

    // vraci TRUE pokud je fsName znamy plug-in file system; pokud vraci TRUE, vraci i 'index'
    // pluginu FS a index 'fsNameIndex' jmena file systemu pluginu
    BOOL IsPluginFS(const char* fsName, int& index, int& fsNameIndex);

    // vraci TRUE pokud jsou 'fsName1' a 'fsName2' ze stejneho pluginu; pokud vraci TRUE,
    // vraci i index 'fsName2Index' jmena file systemu 'fsName2' pluginu
    BOOL AreFSNamesFromSamePlugin(const char* fsName1, const char* fsName2, int& fsName2Index);

    // vraci index plug-inu pro "custom pack", ktery je 'count'-ty v poradi (od nulteho),
    // vraci -1, pokud takovy neexistuje
    int GetCustomPackerIndex(int count);

    // vraci pocet plug-inu pro "custom pack" pred zadanym indexem,
    // inverzni funkce k GetCustomPackerIndex,
    // vraci -1, pokud je 'index' neplatny
    int GetCustomPackerCount(int index);

    // vraci index plug-inu pro "custom unpack", ktery je 'count'-ty v poradi (od nulteho),
    // vraci -1, pokud takovy neexistuje
    int GetCustomUnpackerIndex(int count);

    // vraci pocet plug-inu pro "custom unpack" pred zadanym indexem,
    // inverzni funkce k GetCustomUnpackerIndex,
    // vraci -1, pokud je 'index' neplatny
    int GetCustomUnpackerCount(int index);

    // vraci index plug-inu pro "panel view", ktery je 'count'-ty v poradi (od nulteho),
    // vraci -1, pokud takovy neexistuje
    int GetPanelViewIndex(int count);

    // vraci pocet plug-inu pro "panel view" pred zadanym indexem,
    // inverzni funkce k GetPanelViewIndex,
    // vraci -1, pokud je 'index' neplatny
    int GetPanelViewCount(int index);

    // vraci index plug-inu pro "panel edit", ktery je 'count'-ty v poradi (od nulteho),
    // vraci -1, pokud takovy neexistuje
    int GetPanelEditIndex(int count);

    // vraci pocet plug-inu pro "panel edit" pred zadanym indexem,
    // inverzni funkce k GetPanelEditIndex,
    // vraci -1, pokud je 'index' neplatny
    int GetPanelEditCount(int index);

    // vraci index plug-inu pro "file viewer", ktery je 'count'-ty v poradi (od nulteho),
    // vraci -1, pokud takovy neexistuje
    int GetViewerIndex(int count);

    // vraci pocet plug-inu pro "file viewer" pred zadanym indexem,
    // inverzni funkce k GetViewerIndex,
    // vraci -1, pokud je 'index' neplatny
    int GetViewerCount(int index);

    // zavola metodu Event vsech plug-inu (doruceni zpravy nactenym plug-inum)
    void Event(int event, DWORD param);

    // zavola metodu AcceptChangeOnPathNotification vsech plug-inu (doruceni zpravy nactenym plug-inum)
    void AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    // reakce na prikaz Plugins/Last Commad -- pokud existuje nejaky Last Command, spusti ho
    // 'parent' je "parent" okno messageboxu, vraci TRUE pokud se ma zrusit oznaceni v panelu
    BOOL OnLastCommand(CFilesWindow* panel, HWND parent);

    // execute commandu vyvolany horkou klavesou; 'pluginIndex' a 'menuItemIndex' urcuji command
    // 'parent' je "parent" okno messageboxu, vraci TRUE pokud se ma zrusit oznaceni v panelu
    BOOL ExecuteCommand(int pluginIndex, int menuItemIndex, CFilesWindow* panel, HWND parent);

    // projde pole Order, vyradi z nej jiz neexistujici pluginy a na konec pripoji
    // nove pluginy, ktere v poli jeste nemaji zaznam
    // pokud je 'sortByName' TRUE, pluginy abecedne seradi
    void UpdatePluginsOrder(BOOL sortByName);

    // presune v ramci pole Order polozku z 'index' na 'newIndex'
    BOOL ChangePluginsOrder(int index, int newIndex);

    // vrati Order[index].Index
    int GetIndexByOrder(int index);

    // vrati index daneho pluginu (podle pole Order) nebo -1, pokud ho nenalezne
    int GetPluginOrderIndex(const CPluginData* plugin);

    // ziska/nastavuje promennou v poli CPluginData::ShowSubmenuInPluginsBar
    // 'index' je index pluginu v poli Data (ne Orders)
    BOOL GetShowInBar(int index);
    void SetShowInBar(int index, BOOL showInBar);

    // ziska/nastavuje promennou pluginu CPluginData::ChDrvMenuFSItemVisible
    // 'index' je index pluginu v poli Data (ne Orders)
    BOOL GetShowInChDrv(int index);
    void SetShowInChDrv(int index, BOOL showInChDrv);

    // prida do pole PluginFSTimers novy timer, absolutni timeout je GetTickCount() + 'relTimeout';
    // az GetTickCount() vrati hodnotu vetsi nebo rovnu timeoutu, zavola se metoda Event() FS-objektu
    // 'timerOwner' s parametry FSE_TIMER a 'timerParam'; vraci uspech (TRUE = timer uspesne pridan)
    BOOL AddPluginFSTimer(DWORD relTimeout, CPluginFSInterfaceAbstract* timerOwner, DWORD timerParam);

    // zrusi z pole PluginFSTimers bud vsechny timery FS-objektu 'timerOwner' (je-li 'allTimers' TRUE)
    // nebo jen vsechny timery s parametrem rovnym 'timerParam' (je-li 'allTimers' FALSE); vraci pocet
    // zrusenych timeru
    int KillPluginFSTimer(CPluginFSInterfaceAbstract* timerOwner, BOOL allTimers, DWORD timerParam);

    // vola se pri prichodu WM_TIMER s parametrem IDT_PLUGINFSTIMERS; slouzi k vyvolavani timeru
    // z pole PluginFSTimers
    void HandlePluginFSTimers();

    void SetWorkingPluginFS(CPluginFSInterfaceEncapsulation* workingPluginFS) { WorkingPluginFS = workingPluginFS; }

    // hleda 'hotKey' v menu pluginu, vraci TRUE pokud hotKey nalezl a v 'pluginIndex' a 'menuItemIndex'
    // vrati umisteni
    BOOL FindHotKey(WORD hotKey, BOOL ignoreSkillLevels, const CPluginData* ignorePlugin, int* pluginIndex, int* menuItemIndex);

    // obehne vsechny pluginy mimo 'ignorePlugin' a pokud ma nektera polozka menu uvedenou horkou klavesu, odstrani ji
    void RemoveHotKey(WORD hotKey, const CPluginData* ignorePlugin);

    // vrati TRUE, pokud nekteremu z commandu patri horka klavesa, zaroven pak nastavi 'pluginIndex' a 'menuItemIndex'
    BOOL QueryHotKey(WPARAM wParam, LPARAM lParam, int* pluginIndex, int* menuItemIndex);

    // rakce na WM_(SYS)KEYDOWN v panelu nebo edit line -- pluginy prohledaji menu,
    // zda nemaji tuto hot key zabranou; pokud zpravu zpracuji, vraci TRUE, jinak FALSE
    BOOL HandleKeyDown(WPARAM wParam, LPARAM lParam, CFilesWindow* activePanel, HWND hParent);

    // nastavi promenne LastPlgCmdPlugin a LastPlgCmdID
    void SetLastPlgCmd(const char* dllName, int id);

    // vraci pocet loadlych pluginu, ktere budou ukladat konfiguraci
    int GetPluginSaveCount();

    // po zmene jazyka v Salamanderovi promaze LastSLGName vsem pluginum, aby doslo k nove volbe nahradniho
    // jazyka u pluginu (pokud pro plugin nebude existovat jazyk zvoleny pro Salamandera)
    void ClearLastSLGNames();

    // vraci pocet pluginu, ktere lze naloadit (vraci GetLoaded() TRUE)
    int GetNumOfPluginsToLoad();

    // vraci FS name prvniho pluginu, ktery pridava FS a zaroven ma nastavene PluginIsNethood
    // na TRUE; neni-li 'fsName' NULL, vraci v nem nalezene FS name; neni-li 'nethoodPlugin' NULL,
    // vraci v nem nalezeny plugin; vraci uspech (FALSE = zadny takovy plugin neexistuje)
    BOOL GetFirstNethoodPluginFSName(char* fsName = NULL, CPluginData** nethoodPlugin = NULL);

    // zavola metodu PasswordManagerEvent vsech pluginu, ktere pouzivaji Password Manager,
    // viz CSalamanderGeneralAbstract::SetPluginUsesPasswordManager (nenactene pluginy nacte)
    void PasswordManagerEvent(HWND parent, int event);

    // uvolni a vynuluje 'PluginDynMenuIcons' vsech pluginu
    void ReleasePluginDynMenuIcons();

protected:
    // dle promennych LastPlgCmdXXX dohleda plugin a polozku v jeho menu odpovidajici poslednimu
    // spustenemu prikazu z menu Plugins; 'rebuildDynMenu' je TRUE pokud se ma v pripade dynamickeho
    // menu provest pred hledanim prikazu BuildMenu(); 'parent' je parent messageboxu (jen v pripade,
    // ze 'rebuildDynMenu' je TRUE);
    // pokud prikaz nalezne, vraci TRUE a 'pluginIndex' obsahuje index pluginu v poli CPlugins::Data
    // a 'menuItemIndex' index do pole MenuItems
    // jinak vraci FALSE
    BOOL FindLastCommand(int* pluginIndex, int* menuItemIndex, BOOL rebuildDynMenu, HWND parent);

    // napocteme novy CPluginsStateCache::ActualStateMask a ostatni promenne (pokud to ma cenu)
    // pozdeji z nich bude CPluginData::GetMaskForMenuItems urcovat masku
    // count je pocek polozek menu Plugins
    void CalculateStateCache();

    // prida zaznam do pole Order; vraci index v poli v pripade uspechu, jinak vraci -1
    int AddPluginToOrder(const char* dllName, BOOL showInBar);

    // seradi pole Orders podle nazvu pluginu (slouzi pro nove pridane pluginy, aby byly abecedne serazeny)
    void QuickSortPluginsByName(int left, int right);

    // pouze pro konverzi ze stare konfigurace (promennou pro viditelnost jsem presunul do CPluginData)
    BOOL PluginVisibleInBar(const char* dllName);

    // pomocna funkce: vyhledani indexu pro umisteni timeru s 'timeoutAbs' v poli PluginFSTimers
    int FindIndexForNewPluginFSTimer(DWORD timeoutAbs);
};

//
// ****************************************************************************
// CSalamanderPluginEntry
//

class CSalamanderPluginEntry : public CSalamanderPluginEntryAbstract
{
protected:
    HWND Parent;                     // parent pripadnych messageboxu
    CPluginData* Plugin;             // zaznam o plug-inu
    BOOL Valid;                      // TRUE pokud byla uspesne volana SetBasicPluginData
    BOOL Error;                      // uz byl vypsan na obrazovku error?
    DWORD LoadInfo;                  // DWORD, ktery vraci GetLoadInformation()
    TIndirectArray<char> OldFSNames; // pole starych fs-names (jmena z registry, nahrazuji se pri nacitani pluginu)

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

    // pridava do LoadInfo dalsi LOADINFO_XXX flag
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
    // vycisti klic 'key' od vsech podklicu a hodnot, vraci uspech
    virtual BOOL WINAPI ClearKey(HKEY key);

    // vytvori nebo otevre existujici podklic 'name' klice 'key', vraci 'createdKey' a uspech
    virtual BOOL WINAPI CreateKey(HKEY key, const char* name, HKEY& createdKey);

    // otevre existujici podklic 'name' klice 'key', vraci 'openedKey' a uspech
    virtual BOOL WINAPI OpenKey(HKEY key, const char* name, HKEY& openedKey);

    // zavre klic otevreny pres OpenKey nebo CreateKey
    virtual void WINAPI CloseKey(HKEY key);

    // smaze podklic 'name' klice 'key', vraci uspech
    virtual BOOL WINAPI DeleteKey(HKEY key, const char* name);

    // nacte hodnotu 'name'+'type'+'buffer'+'bufferSize' z klice 'key', vraci uspech
    virtual BOOL WINAPI GetValue(HKEY key, const char* name, DWORD type, void* buffer, DWORD bufferSize);

    // ulozi hodnotu 'name'+'type'+'data'+'dataSize' do klice 'key', pro retezce je mozne
    // zadat 'dataSize' == -1 -> vypocet delky retezce pomoci funkce strlen,
    // vraci uspech
    virtual BOOL WINAPI SetValue(HKEY key, const char* name, DWORD type, const void* data, DWORD dataSize);

    // smaze hodnotu 'name' klice 'key', vraci uspech
    virtual BOOL WINAPI DeleteValue(HKEY key, const char* name);

    // vytahne do 'bufferSize' protrebnou velikost pro hodnotu 'name'+'type' z klice 'key', vraci uspech
    virtual BOOL WINAPI GetSize(HKEY key, const char* name, DWORD type, DWORD& bufferSize);
};

//
// ****************************************************************************
// CSalamanderConnect
//

class CSalamanderConnect : public CSalamanderConnectAbstract
{
protected:
    int Index; // index plug-inu v Plugins

    // peti-promenny filtr pro upgrade plug-inu, menu se upgraduje vzdy
    BOOL CustomPack;   // TRUE -> modifikace "custom pack" povoleny
    BOOL CustomUnpack; // TRUE -> modifikace "custom unpack" povoleny
    BOOL PanelView;    // TRUE -> modifikace "panel view" povoleny
    BOOL PanelEdit;    // TRUE -> modifikace "panel edit" povoleny
    BOOL Viewer;       // TRUE -> modifikace "file viewer" povoleny
    int SubmenuLevel;  // aktualni level vkladanych submenu (0 = submenu pluginu v menu Plugins)

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
    int Index;        // index plug-inu v Plugins
    int SubmenuLevel; // aktualni level vkladanych submenu (0 = submenu pluginu v menu Plugins)

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

// interni funkce pro praci s ikonkama pluginu
BOOL CreateGrayscaleDIB(HBITMAP hSource, COLORREF transparent, HBITMAP& hGrayscale);
//HICON GetIconFromDIB(HBITMAP hBitmap, int index);

// provede konverzi cesty na externi format (zavola prislusnemu pluginu metodu
// CPluginInterfaceForFSAbstract::ConvertPathToExternal())
void PluginFSConvertPathToExternal(char* path);

#ifdef _WIN64 // FIXME_X64_WINSCP
// test, jestli jde o plugin chybejici v x64 verzi Salama: uz jen WinSCP
BOOL IsPluginUnsupportedOnX64(const char* dllName, const char** pluginNameEN = NULL);
#endif // _WIN64
