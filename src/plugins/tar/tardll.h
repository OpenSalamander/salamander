// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//
// ****************************************************************************
// CPluginInterface
//

class CPluginInterfaceForArchiver : public CPluginInterfaceForArchiverAbstract
{
public:
    BOOL WINAPI ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                            CSalamanderDirectoryAbstract* dir,
                            CPluginDataInterfaceAbstract*& pluginData) override;
    BOOL WINAPI UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                              CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                              const char* archiveRoot, SalEnumSelection next, void* nextParam) override;
    BOOL WINAPI UnpackOneFile(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                              CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                              const CFileData* fileData, const char* targetDir,
                              const char* newFileName, BOOL* renamingNotSupported) override;
    BOOL WINAPI PackToArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                              const char* archiveRoot, BOOL move, const char* sourcePath,
                              SalEnumSelection2 next, void* nextParam) override
    {
        return FALSE;
    }
    BOOL WINAPI DeleteFromArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                  CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                                  SalEnumSelection next, void* nextParam) override
    {
        return FALSE;
    }
    BOOL WINAPI UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                   const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                   CDynamicString* archiveVolumes) override;
    BOOL WINAPI CanCloseArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                BOOL force, int panel) override
    {
        return TRUE;
    }
    BOOL WINAPI GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies) override { return FALSE; }
    void WINAPI DeleteTmpCopy(const char* fileName, BOOL firstFile) override {}
    BOOL WINAPI PrematureDeleteTmpCopy(HWND parent, int copiesCount) override { return FALSE; }
};

class CPluginInterfaceForViewer : public CPluginInterfaceForViewerAbstract
{
public:
    BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                         UINT showCmd, BOOL alwaysOnTop, BOOL returnUnlock, HANDLE* unlock,
                         BOOL* unlockOwner, CSalamanderPluginViewerData* viewerData,
                         int enumFilesSourceUID, int enumFilesCurrentIndex) override;
    BOOL WINAPI CanViewFile(const char* name) override { return TRUE; }
};

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    void WINAPI About(HWND parent) override;

    BOOL WINAPI Release(HWND parent, BOOL force) override { return TRUE; }

    void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) override;
    void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) override;

    void WINAPI Configuration(HWND parent) override
    {
    }

    void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander) override;

    void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) override
    {
        return;
    }

    CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() override;
    CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() override;
    CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() override { return NULL; }
    CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() override { return NULL; }
    CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() override { return NULL; }

    void WINAPI Event(int event, DWORD param) override {}
    void WINAPI ClearHistory(HWND parent) override {}
    void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) override {}

    void WINAPI PasswordManagerEvent(HWND parent, int event) override {}
};
