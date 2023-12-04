// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <ShObjIdl.h>

#include "dialogs.h"
#include "folders.h"
#include "iltools.h"

//
// ****************************************************************************
// CPluginFSInterface
//

CPluginFSInterface::CPluginFSInterface()
{
    // Desktop -> CurrentPIDL
    if (FAILED(SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOP, &CurrentPIDL)))
    {
        // PIDL Desktopu je prazdny PIDL, ale ziskame ho ciste
        CurrentPIDL = NULL;
        TRACE_E("SHGetSpecialFolderLocation failed on CSIDL_DESKTOP");
    }

    // Desktop -> CurrentFolder
    if (FAILED(SHGetDesktopFolder(&CurrentFolder)))
    {
        CurrentFolder = NULL;
        TRACE_E("SHGetDesktopFolder failed");
    }

    // Desktop -> DesktopFolder
    if (FAILED(SHGetDesktopFolder(&DesktopFolder)))
    {
        DesktopFolder = NULL;
        TRACE_E("SHGetDesktopFolder failed");
    }

    ContextMenu2 = NULL;

    ErrorState = fesOK;
}

CPluginFSInterface::~CPluginFSInterface()
{
    if (CurrentPIDL != NULL)
    {
        ILFree(CurrentPIDL);
        CurrentPIDL = NULL;
    }
    if (CurrentFolder != NULL)
    {
        CurrentFolder->Release();
        CurrentFolder = NULL;
    }
    if (DesktopFolder != NULL)
    {
        DesktopFolder->Release();
        DesktopFolder = NULL;
    }
}

BOOL CPluginFSInterface::IsGood()
{
    return CurrentPIDL != NULL && CurrentFolder != NULL && DesktopFolder != NULL;
}

void WINAPI
CPluginFSInterface::ReleaseObject(HWND parent)
{
}

BOOL WINAPI
CPluginFSInterface::GetRootPath(char* userPart)
{
    userPart[0] = 0;
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::GetCurrentPath(char* userPart)
{
    userPart[0] = 0;
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::GetFullName(CFileData& file, int isDir, char* buf, int bufSize)
{
    buf[0] = 0;
    //  lstrcpyn(buf, Path, bufSize);  // pokud se nevejde cesta, jmeno se urcite take nevejde (ohlasi chybu)
    if (isDir == 2)
        return SalamanderGeneral->CutDirectory(buf, NULL); // up-dir
    else
        return SalamanderGeneral->SalPathAppend(buf, file.Name, bufSize);
}

BOOL WINAPI
CPluginFSInterface::GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success)
{
    return FALSE; // preklad neni mozny, at si chybu ohlasi sam Salamander
}

BOOL WINAPI
CPluginFSInterface::IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    return FALSE;
}

BOOL WINAPI
CPluginFSInterface::IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                               const char* userPart, char* cutFileName, BOOL* pathWasCut,
                               BOOL forceRefresh, int mode)
{
    if (mode != 3 && (pathWasCut != NULL || cutFileName != NULL))
    {
        TRACE_E("Incorrect value of 'mode' in CPluginFSInterface::ChangePath().");
        mode = 3;
    }
    if (pathWasCut != NULL)
        *pathWasCut = FALSE;
    if (cutFileName != NULL)
        *cutFileName = 0;
    if (ErrorState == fesFatal)
    {
        ErrorState = fesOK;
        return FALSE; // ListCurrentPath selhala kvuli pameti, fatal error
    }

    return TRUE;
}

extern IShellFolder* ChangePathNewFolder;
extern LPITEMIDLIST ChangePathNewPIDL;

BOOL WINAPI
CPluginFSInterface::ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                    CPluginDataInterfaceAbstract*& pluginData,
                                    int& iconsType, BOOL forceRefresh)
{
    TRACE_I("CPluginFSInterface::ListCurrentPath");

    CFileData file;
    DWORD flags;

    LPCITEMIDLIST currentPIDL = CurrentPIDL;
    IShellFolder* currentFolder = CurrentFolder;
    if (ChangePathNewFolder != NULL && ChangePathNewPIDL != NULL)
    {
        currentFolder = ChangePathNewFolder;
        currentPIDL = ChangePathNewPIDL;
    }

    // pokud nejde o Desktop (prazdny PIDL), vlozime ".."
    if (!ILIsEmpty(currentPIDL))
    {
        file.Name = SalamanderGeneral->DupStr("..");
        if (file.Name == NULL)
            goto ON_FATAL_ERROR;
        file.NameLen = 2;
        file.Ext = file.Name + file.NameLen;
        file.Size = CQuadWord(0, 0);
        file.Attr = FILE_ATTRIBUTE_DIRECTORY;
        file.LastWrite.dwLowDateTime = 0;
        file.LastWrite.dwHighDateTime = 0;
        file.Hidden = 0;
        file.DosName = NULL;
        file.PluginData = NULL;
        file.IsLink = 0;
        file.IsOffline = 0;
        if (!dir->AddDir(NULL, file, NULL))
            goto ON_FATAL_ERROR;
    }

    LPENUMIDLIST enumIDList;
    flags = SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN;
    if (SUCCEEDED(currentFolder->EnumObjects(NULL, flags, &enumIDList)))
    {
        int sortByExtDirsAsFiles;
        SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                              sizeof(sortByExtDirsAsFiles), NULL);

        ULONG celt;
        LPITEMIDLIST idList; // single-level PIDL, relative to the folder
        STRRET str;
        while (enumIDList->Next(1, &idList, &celt) == NOERROR)
        {
            if (SUCCEEDED(currentFolder->GetDisplayNameOf(idList, SHGDN_INFOLDER, &str)))
            {
                char name[MAX_PATH];
                if (SUCCEEDED(StrRetToBuf(&str, idList, name, MAX_PATH)))
                {
                    file.Name = SalamanderGeneral->DupStr(name);
                    if (file.Name == NULL)
                    {
                        enumIDList->Release();
                        goto ON_FATAL_ERROR;
                    }
                    file.NameLen = strlen(file.Name);

                    BOOL isDir = FALSE;
                    file.Hidden = 0;

                    SFGAOF uAttr = SFGAO_FOLDER | SFGAO_GHOSTED;
                    if (SUCCEEDED(currentFolder->GetAttributesOf(1, (LPCITEMIDLIST*)&idList, &uAttr)))
                    {
                        isDir = (uAttr & SFGAO_FOLDER) != 0;
                        file.Hidden = (uAttr & SFGAO_GHOSTED) ? 1 : 0;
                    }

                    if (!sortByExtDirsAsFiles && isDir)
                        file.Ext = file.Name + file.NameLen; // foldery nemaji pripony
                    else
                    {
                        char* s = strrchr(file.Name, '.');
                        if (s != NULL)
                            file.Ext = s + 1; // ".cvspass" ve Windows je pripona
                        else
                            file.Ext = file.Name + file.NameLen;
                    }
                    file.Size = CQuadWord(0, 0);
                    file.Attr = 0;
                    file.LastWrite.dwLowDateTime = 0;
                    file.LastWrite.dwHighDateTime = 0;
                    file.PluginData = (DWORD_PTR)idList; // destrukce se provede v CPluginDataInterface::ReleasePluginData
                    file.DosName = NULL;

                    // FIXME: tady by to casem asi chtelo nejakou sofistikovanejsi metodu (folder o svych polozkach bude vedet jestli maji mit overlay nebo ne)
                    if (isDir)
                        file.IsLink = 0;
                    else
                        file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);
                    file.IsOffline = 0;

                    BOOL ret = isDir ? dir->AddDir(NULL, file, NULL) : dir->AddFile(NULL, file, NULL);
                    if (!ret)
                    {
                        free(file.Name);
                        enumIDList->Release();
                        goto ON_FATAL_ERROR;
                    }
                }
            }
        }
        enumIDList->Release();

        // predavani cesty pres globalky
        if (ChangePathNewFolder != NULL && ChangePathNewPIDL != NULL)
        {
            if (CurrentPIDL != NULL)
                ILFree(CurrentPIDL);
            if (CurrentFolder != NULL)
                CurrentFolder->Release();

            // nastavime nove hodnoty
            CurrentFolder = ChangePathNewFolder;
            CurrentPIDL = ChangePathNewPIDL;
            ChangePathNewFolder = NULL;
            ChangePathNewPIDL = NULL;
        }

        if (!CheckPIDL(CurrentPIDL) || CurrentFolder == NULL)
            goto ON_FATAL_ERROR;

        iconsType = pitFromPlugin;
        pluginData = new CPluginDataInterface(CurrentFolder, CurrentPIDL);
        if (pluginData == NULL)
            goto ON_FATAL_ERROR;

        return TRUE;
    }
    else
        TRACE_E("EnumObjects failed");

ON_FATAL_ERROR:
    ErrorState = fesFatal;
    return FALSE;
}

BOOL WINAPI
CPluginFSInterface::TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
{
    detach = FALSE;
    return TRUE;
}

void WINAPI
CPluginFSInterface::Event(int event, DWORD param)
{
}

DWORD WINAPI
CPluginFSInterface::GetSupportedServices()
{
    return FS_SERVICE_CONTEXTMENU |
           FS_SERVICE_SHOWPROPERTIES |
           FS_SERVICE_GETFSICON |
           FS_SERVICE_OPENACTIVEFOLDER;
}

BOOL WINAPI
CPluginFSInterface::GetChangeDriveOrDisconnectItem(const char* fsName, char*& title, HICON& icon, BOOL& destroyIcon)
{
    return FALSE;
}

HICON WINAPI
CPluginFSInterface::GetFSIcon(BOOL& destroyIcon)
{
    HICON icon;
    if (!SalamanderGeneral->GetFileIcon((LPCTSTR)CurrentPIDL, TRUE, &icon, SALICONSIZE_16, TRUE, TRUE))
        icon = NULL;
    destroyIcon = TRUE;
    return icon;
}

void WINAPI
CPluginFSInterface::GetFSFreeSpace(CQuadWord* retValue)
{
    *retValue = CQuadWord(-1, -1);
}

BOOL WINAPI
CPluginFSInterface::GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
{
    return TRUE;
}

void WINAPI
CPluginFSInterface::ShowInfoDialog(const char* fsName, HWND parent)
{
}

BOOL WINAPI
CPluginFSInterface::ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo)
{
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                char* newName, BOOL& cancel)
{
    return TRUE;
}

void WINAPI
CPluginFSInterface::AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs)
{
}

BOOL WINAPI
CPluginFSInterface::CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel)
{
    return TRUE;
}

void WINAPI
CPluginFSInterface::ViewFile(const char* fsName, HWND parent,
                             CSalamanderForViewFileOnFSAbstract* salamander,
                             CFileData& file)
{
}

BOOL WINAPI
CPluginFSInterface::Delete(const char* fsName, int mode, HWND parent, int panel,
                           int selectedFiles, int selectedDirs, BOOL& cancelOrError)
{
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                     int panel, int selectedFiles, int selectedDirs,
                                     char* targetPath, BOOL& operationMask,
                                     BOOL& cancelOrHandlePath, HWND dropTarget)
{
    return TRUE; // uspech nebo error/cancel
}

BOOL WINAPI
CPluginFSInterface::CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                           const char* sourcePath, SalEnumSelection2 next,
                                           void* nextParam, int sourceFiles, int sourceDirs,
                                           char* targetPath, BOOL* invalidPathOrCancel)
{
    return FALSE; // neznamy 'mode'
}

BOOL WINAPI
CPluginFSInterface::ChangeAttributes(const char* fsName, HWND parent, int panel,
                                     int selectedFiles, int selectedDirs)
{
    return FALSE; // cancel
}

void WINAPI
CPluginFSInterface::OpenActiveFolder(const char* fsName, HWND parent)
{
    SHELLEXECUTEINFO se;
    memset(&se, 0, sizeof(SHELLEXECUTEINFO));
    se.cbSize = sizeof(SHELLEXECUTEINFO);
    se.fMask = SEE_MASK_IDLIST;
    se.lpVerb = "open";
    se.hwnd = parent;
    se.nShow = SW_SHOWNORMAL;
    se.lpIDList = CurrentPIDL;
    ShellExecuteEx(&se);
}

//----------------------------------------------------------------------------
#define CMD_ID_FIRST 1
#define CMD_ID_LAST 0x7fff

void WINAPI
CPluginFSInterface::ShowProperties(const char* fsName, HWND parent, int panel,
                                   int selectedFiles, int selectedDirs)
{
    LPCITEMIDLIST* pidlArray;
    int itemsInArray;
    if (CreateIDListFromSelection(panel, selectedFiles, selectedDirs, &pidlArray, &itemsInArray))
    {
        IContextMenu* contextMenu = GetContextMenu(parent, pidlArray, itemsInArray);
        if (contextMenu != NULL)
        {
            HMENU hMenu = CreatePopupMenu();
            contextMenu->QueryContextMenu(hMenu, 0, CMD_ID_FIRST, CMD_ID_LAST, CMF_NORMAL | CMF_EXPLORE);

            CMINVOKECOMMANDINFOEX ici;
            ZeroMemory(&ici, sizeof(ici));
            ici.cbSize = sizeof(ici);
            ici.hwnd = parent;
            ici.nShow = SW_NORMAL;
            ici.lpVerb = "properties";
            if (FAILED(contextMenu->InvokeCommand((CMINVOKECOMMANDINFO*)&ici)))
                TRACE_E("InvokeCommand failed");

            DestroyMenu(hMenu);

            contextMenu->Release();
        }
        LocalFree(pidlArray);
    }
}

void RemoveUselessSeparatorsFromMenu(HMENU h)
{
    int miCount = GetMenuItemCount(h);
    MENUITEMINFO mi;
    int lastSep = -1;
    int i;
    for (i = miCount - 1; i >= 0; i--)
    {
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE;
        if (GetMenuItemInfo(h, i, TRUE, &mi) && (mi.fType & MFT_SEPARATOR))
        {
            if (lastSep != -1 && lastSep == i + 1) // dva separatory po sobe, jeden smazeme, je nadbytecny
                DeleteMenu(h, i, MF_BYPOSITION);
            lastSep = i;
        }
    }
}

void WINAPI
CPluginFSInterface::ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                int panel, int selectedFiles, int selectedDirs)
{
    if (type == fscmItemsInPanel)
    {
        LPCITEMIDLIST* pidlArray;
        int itemsInArray;
        if (CreateIDListFromSelection(panel, selectedFiles, selectedDirs, &pidlArray, &itemsInArray))
        {
            IContextMenu2* contextMenu2 = GetContextMenu2(parent, pidlArray, itemsInArray);
            if (contextMenu2 != NULL)
            {
                HMENU hMenu = CreatePopupMenu();
                contextMenu2->QueryContextMenu(hMenu, 0, CMD_ID_FIRST, CMD_ID_LAST, CMF_NORMAL | CMF_EXPLORE);
                RemoveUselessSeparatorsFromMenu(hMenu);

                ContextMenu2 = contextMenu2; // aby nam fungoval forward zprav z HandleMenuMsg
                DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, menuX, menuY,
                                             parent, NULL);
                ContextMenu2 = NULL;
                if (cmd != 0)
                {
                    CMINVOKECOMMANDINFOEX ici;
                    ZeroMemory(&ici, sizeof(ici));
                    ici.cbSize = sizeof(ici);
                    ici.hwnd = parent;
                    ici.nShow = SW_NORMAL;
                    ici.lpVerb = (LPSTR)MAKEINTRESOURCE(cmd - CMD_ID_FIRST);
                    if (FAILED(contextMenu2->InvokeCommand((CMINVOKECOMMANDINFO*)&ici)))
                        TRACE_E("InvokeCommand failed");
                }

                DestroyMenu(hMenu);
                contextMenu2->Release();
            }
            LocalFree(pidlArray);
        }
    }
}

BOOL WINAPI
CPluginFSInterface::HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult)
{
    if (*plResult != NULL)
        *plResult = 0; // TODO: move from ContextMenu2 to ContextMenu3 and HandleMenuMsg2()
    if (ContextMenu2 != NULL)
    {
        TRACE_I("HandleMenuMsg uMsg=" << uMsg);
        ContextMenu2->HandleMenuMsg(uMsg, wParam, lParam);
        return TRUE;
    }
    return FALSE;
}

BOOL CPluginFSInterface::CreateIDListFromSelection(int panel, int selectedFiles, int selectedDirs,
                                                   LPCITEMIDLIST** pidlArray, int* itemsInArray)
{
    const CFileData* f = NULL; // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
    BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;

    int itemsCount = focused ? 1 : (selectedFiles + selectedDirs);
    *pidlArray = (LPCITEMIDLIST*)LocalAlloc(LPTR, sizeof(LPCITEMIDLIST) * itemsCount);
    if (*pidlArray == NULL)
        return FALSE;

    *itemsInArray = 0;
    do
    {
        // vyzvedneme data o zpracovavanem souboru
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // provedeme operaci na souboru/adresari
        if (f != NULL)
        {
            (*pidlArray)[*itemsInArray] = (LPCITEMIDLIST)f->PluginData;
            (*itemsInArray)++;
        }

        // zjistime jestli ma cenu pokracovat (pokud neni cancel a existuje jeste nejaka dalsi oznacena polozka)
    } while (!focused && f != NULL);
    return TRUE;
}

IContextMenu*
CPluginFSInterface::GetContextMenu(HWND hParent, LPCITEMIDLIST* pidlArray, int itemsInArray)
{
    HRESULT hr;
    IContextMenu* contextMenuObj;
    hr = CurrentFolder->GetUIObjectOf(hParent, itemsInArray, pidlArray,
                                      IID_IContextMenu, NULL, (LPVOID*)&contextMenuObj);
    if (SUCCEEDED(hr))
    {
        return contextMenuObj;
    }
    else
        TRACE_E("CurrentFolder->GetUIObjectOf failed");
    return NULL;
}

IContextMenu2*
CPluginFSInterface::GetContextMenu2(HWND hParent, LPCITEMIDLIST* pidlArray, int itemsInArray)
{
    IContextMenu* contextMenu = GetContextMenu(hParent, pidlArray, itemsInArray);
    if (contextMenu != NULL)
    {
        IContextMenu2* contextMenu2;
        if (SUCCEEDED(contextMenu->QueryInterface(IID_IContextMenu2, (void**)&contextMenu2)))
        {
            contextMenu->Release();
            return contextMenu2;
        }
        contextMenu->Release();
    }
    return NULL;
}
