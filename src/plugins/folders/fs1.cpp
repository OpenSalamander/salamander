// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dialogs.h"
#include "folders.h"

#include "iltools.h"

#include "folders.rh"
#include "folders.rh2"
#include "lang\lang.rh"

// FS-name pridelene Salamanderem po loadu pluginu
char AssignedFSName[MAX_PATH] = "";

// image-list pro jednoduche ikony FS
HIMAGELIST DFSImageList = NULL;

// globalni promenne, do ktery si ulozim ukazatele na globalni promenne v Salamanderovi
// pro archiv i pro FS - promenne se sdileji
const CFileData** TransferFileData = NULL;
int* TransferIsDir = NULL;
char* TransferBuffer = NULL;
int* TransferLen = NULL;
DWORD* TransferRowData = NULL;
CPluginDataInterfaceAbstract** TransferPluginDataIface = NULL;
DWORD* TransferActCustomData = NULL;

// pomocna promenna pro testy
CPluginFSInterfaceAbstract* LastDetachedFS = NULL;

// ****************************************************************************
// SEKCE FILE SYSTEMU
// ****************************************************************************

BOOL InitFS()
{
    DFSImageList = ImageList_Create(16, 16, ILC_MASK | SalamanderGeneral->GetImageListColorFlags(), 2, 0);
    if (DFSImageList == NULL)
    {
        TRACE_E("Nepodarilo se vytvorit image list.");
        return FALSE;
    }
    ImageList_SetImageCount(DFSImageList, 2); // inicializace
    ImageList_SetBkColor(DFSImageList, SalamanderGeneral->GetCurrentColor(SALCOL_ITEM_BK_NORMAL));

    HICON hIcon = SalamanderGeneral->GetSalamanderIcon(SALICON_DIRECTORY, SALICONSIZE_16);
    if (hIcon != NULL)
    {
        ImageList_ReplaceIcon(DFSImageList, 0, hIcon);
        DestroyIcon(hIcon);
    }
    hIcon = SalamanderGeneral->GetSalamanderIcon(SALICON_NONASSOCIATED, SALICONSIZE_16);
    if (hIcon != NULL)
    {
        ImageList_ReplaceIcon(DFSImageList, 1, hIcon);
        DestroyIcon(hIcon);
    }
    return TRUE;
}

void ReleaseFS()
{
    ImageList_Destroy(DFSImageList);
}

//
// ****************************************************************************
// CPluginInterfaceForFS
//

CPluginFSInterfaceAbstract* WINAPI
CPluginInterfaceForFS::OpenFS(const char* fsName, int fsNameIndex)
{
    ActiveFSCount++;

    CPluginFSInterface* fsIface = new CPluginFSInterface;
    if (!fsIface->IsGood())
    {
        delete fsIface;
        fsIface = NULL;
    }
    return fsIface;
}

void WINAPI
CPluginInterfaceForFS::CloseFS(CPluginFSInterfaceAbstract* fs)
{
    CPluginFSInterface* dfsFS = (CPluginFSInterface*)fs; // aby se volal spravny destruktor

    if (dfsFS == LastDetachedFS)
        LastDetachedFS = NULL;
    ActiveFSCount--;
    if (dfsFS != NULL)
        delete dfsFS;
}

void WINAPI
CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(int panel)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", panel);
    SalamanderGeneral->ChangePanelPathToPluginFS(panel, AssignedFSName, "", NULL);
}

#define CMD_ID_FIRST 1
#define CMD_ID_LAST 0x7fff

IShellFolder* ChangePathNewFolder = NULL; // predavani cesty pres globalky
LPITEMIDLIST ChangePathNewPIDL = NULL;

void WINAPI
CPluginInterfaceForFS::ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                   const char* pluginFSName, int pluginFSNameIndex,
                                   CFileData& file, int isDir)
{
    CPluginFSInterface* fs = (CPluginFSInterface*)pluginFS;
    if (isDir) // podadresar nebo up-dir
    {
        IShellFolder* newFolder;
        LPITEMIDLIST newPIDL;

        if (isDir == 2) // up-dir
        {
            if (CutLastItemFromIL(fs->CurrentPIDL, &newFolder, &newPIDL))
            {
                ChangePathNewFolder = newFolder; // predavani cesty pres globalky
                ChangePathNewPIDL = newPIDL;
                SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, "?");
                if (ChangePathNewFolder != NULL)
                {
                    ChangePathNewFolder->Release();
                    ChangePathNewFolder = NULL;
                }
                if (ChangePathNewPIDL != NULL)
                {
                    ILFree(ChangePathNewPIDL);
                    ChangePathNewPIDL = NULL;
                }
            }
        }
        else // podadresar
        {
            if (AddItemToIL(fs->CurrentPIDL, (LPCITEMIDLIST)file.PluginData, &newFolder, &newPIDL))
            {
                ChangePathNewFolder = newFolder;
                ChangePathNewPIDL = newPIDL;
                SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, "?");
                if (ChangePathNewFolder != NULL)
                {
                    ChangePathNewFolder->Release();
                    ChangePathNewFolder = NULL;
                }
                if (ChangePathNewPIDL != NULL)
                {
                    ILFree(ChangePathNewPIDL);
                    ChangePathNewPIDL = NULL;
                }
            }
        }
    }
    else // soubor
    {
        SalamanderGeneral->SetUserWorkedOnPanelPath(panel); // adresar do Working Directories

        HWND hParent = SalamanderGeneral->GetMsgBoxParent();
        IContextMenu* contextMenu = fs->GetContextMenu(hParent, (LPCITEMIDLIST*)&file.PluginData, 1);
        if (contextMenu != NULL)
        {
            HMENU hMenu = CreatePopupMenu();
            contextMenu->QueryContextMenu(hMenu, 0, CMD_ID_FIRST, CMD_ID_LAST, CMF_NORMAL | CMF_EXPLORE);
            UINT idCmd = GetMenuDefaultItem(hMenu, MF_BYCOMMAND, GMDI_GOINTOPOPUPS);
            if (idCmd != -1)
            {
                CMINVOKECOMMANDINFOEX ici;
                ZeroMemory(&ici, sizeof(ici));
                ici.cbSize = sizeof(ici);
                ici.hwnd = hParent;
                ici.nShow = SW_NORMAL;
                ici.lpVerb = (LPSTR)MAKEINTRESOURCE(idCmd - CMD_ID_FIRST);
                if (FAILED(contextMenu->InvokeCommand((CMINVOKECOMMANDINFO*)&ici)))
                    TRACE_E("InvokeCommand failed");
            }
            else
                TRACE_E("GetMenuDefaultItem failed");

            DestroyMenu(hMenu);
            contextMenu->Release();
        }
    }
}

BOOL WINAPI
CPluginInterfaceForFS::DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                    CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex)
{
    BOOL ret = FALSE;
    if (isInPanel)
    {
        SalamanderGeneral->DisconnectFSFromPanel(parent, panel);
        ret = SalamanderGeneral->GetPanelPluginFS(panel) != pluginFS;
    }
    else
    {
        ret = SalamanderGeneral->CloseDetachedFS(parent, pluginFS);
    }
    return ret;
}

//
// ****************************************************************************
// CPluginDataInterface
//

CPluginDataInterface::CPluginDataInterface(IShellFolder* folder, LPCITEMIDLIST folderPIDL)
    : ShellColumns(10, 20), VisibleColumns(10, 5)
{
    Folder = folder;
    Folder->AddRef();
    FolderPIDL = ILClone(folderPIDL);

    ShellFolder2 = NULL;
    ShellDetails = NULL;

    // do W2K byl podporovan IID_IShellDetails, potom presli na IID_IShellFolder2
    if (FAILED(Folder->QueryInterface(IID_IShellFolder2, (void**)&ShellFolder2)))
    {
        ShellFolder2 = NULL;
        if (FAILED(Folder->CreateViewObject(NULL, IID_IShellDetails, (void**)&ShellDetails)))
            ShellDetails = NULL;
    }
    if (ShellFolder2 != NULL)
        TRACE_I("ShellFolder2 != NULL");
    if (ShellDetails != NULL)
        TRACE_I("ShellDetails != NULL");
}

CPluginDataInterface::~CPluginDataInterface()
{
    if (ShellDetails != NULL)
    {
        ShellDetails->Release();
        ShellDetails = NULL;
    }
    if (ShellFolder2 != NULL)
    {
        ShellFolder2->Release();
        ShellFolder2 = NULL;
    }
    if (Folder != NULL)
    {
        Folder->Release();
        Folder = NULL;
    }
    if (FolderPIDL != NULL)
    {
        ILFree(FolderPIDL);
        FolderPIDL = NULL;
    }
}

BOOL CPluginDataInterface::IsGood()
{
    return FolderPIDL != NULL;
}

//************************************************
// Implementace metod CPluginDataInterfaceAbstract
//************************************************

void WINAPI
CPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
{
    if (file.PluginData != NULL) // UPDIR
        ILFree((ITEMIDLIST*)file.PluginData);
}

HIMAGELIST WINAPI
CPluginDataInterface::GetSimplePluginIcons(int iconSize)
{
    return DFSImageList;
}

HICON WINAPI
CPluginDataInterface::GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon)
{
    destroyIcon = FALSE;
    HICON hIcon = NULL;

    LPITEMIDLIST pidlFull = ILCombine(FolderPIDL, (LPCITEMIDLIST)file->PluginData);
    if (pidlFull != NULL)
    {
        if (SalamanderGeneral->GetFileIcon((const char*)pidlFull, TRUE, &hIcon, iconSize, FALSE, FALSE))
            destroyIcon = TRUE;
        ILFree(pidlFull);
    }

    //  if (hIcon == NULL)
    //    TRACE_E("CPluginDataInterface::GetPluginIcon(): unable to extract icon from: " << file->Name);

    return hIcon;
}

int WINAPI
CPluginDataInterface::CompareFilesFromFS(const CFileData* file1, const CFileData* file2)
{
    LPCITEMIDLIST pidl1 = (LPCITEMIDLIST)file1->PluginData;
    LPCITEMIDLIST pidl2 = (LPCITEMIDLIST)file2->PluginData;
    if (pidl1 == pidl2)
        return 0;
    if (pidl1 == NULL)
        return -1; // chodi sem adresar "..", ten ma pidl==NULL
    if (pidl2 == NULL)
        return 1; // chodi sem adresar "..", ten ma pidl==NULL

    while (1)
    {
        LPCITEMIDLIST end1 = GetNextItemFromIL(pidl1);
        LPCITEMIDLIST end2 = GetNextItemFromIL(pidl2);
        int res = (int)(((char*)end1 - (char*)pidl1) - ((char*)end2 - (char*)pidl2));
        if (res != 0)
            return res; // delsi pidl je "vetsi"
        res = memcmp(pidl1, pidl2, ((char*)end1 - (char*)pidl1));
        if (res != 0)
            return res; // lisi se binarne
        if (end1->mkid.cb == 0)
        {
            if (end2->mkid.cb == 0)
                return 0; // pidly jsou zcela shodne
            else
                return -1; // prvni pidl je kratsi, tedy "mensi"
        }
        else
        {
            if (end2->mkid.cb == 0)
                return 1; // prvni pidl je delsi, tedy "vetsi"
        }
        pidl1 = end1;
        pidl2 = end2;
    }
}

// callback volany ze Salamandera pro ziskani textu
void WINAPI GetRowText()
{
    const CFileData* file = *TransferFileData;
    LPCITEMIDLIST pidl = (LPCITEMIDLIST)file->PluginData;

    if (pidl == NULL) // up-dir
    {
        *TransferLen = 0;
        return;
    }

    int realIndex = *TransferActCustomData;
    CPluginDataInterface* pluginData = (CPluginDataInterface*)*TransferPluginDataIface;

    DETAILSINFO di;
    di.fmt = LVCFMT_LEFT;
    di.cxChar = 20;
    di.str.uType = (UINT)-1;
    di.pidl = (LPCITEMIDLIST)file->PluginData;

    if (SUCCEEDED(pluginData->GetDetailsHelper(realIndex, &di)))
    {
        StrRetToBuf(&di.str, di.pidl, TransferBuffer, TRANSFER_BUFFER_MAX);
        *TransferLen = (int)strlen(TransferBuffer);
    }
    else
    {
        *TransferLen = 0;
    }
}

void WINAPI
CPluginDataInterface::SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                const CFileData* upperDir)
{
    // ziskame globalni promenne pro rychly pristup
    view->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer, TransferLen,
                               TransferRowData, TransferPluginDataIface, TransferActCustomData);

    if (view->GetViewMode() == VIEW_MODE_DETAILED) // upravime sloupce
    {
        GetShellColumns();

        view->SetViewMode(VIEW_MODE_DETAILED, VALID_DATA_NONE); // zrusime ostatni sloupce, nechame jen sloupec Name

        CColumn column;
        int colIndex = 0;
        int i;
        for (i = 0; i < VisibleColumns.Count; i++)
        {
            int realIndex = VisibleColumns[i];
            CShellColumn* shellCol = &ShellColumns[realIndex];
            if (i == 0)
            {
                // 0-ty sloupec je Name
                colIndex++;
            }
            else
            {
                lstrcpyn(column.Name, shellCol->Name, SAL_ARRAYSIZE(column.Name));
                lstrcpyn(column.Description, "", SAL_ARRAYSIZE(column.Description));
                column.GetText = GetRowText;
                column.SupportSorting = 0;
                column.LeftAlignment = (shellCol->Fmt == LVCFMT_LEFT) ? 1 : 0;
                column.ID = COLUMN_ID_CUSTOM;
                column.Width = 0;      // FIXME: sirka sloupce by se mela cist z konfigurace
                column.FixedWidth = 0; // FIXME: elasticita sloupce by se mela cist z konfigurace (ted sloupec nepujde prepnout na pevnou sirku)
                column.CustomData = realIndex;
                view->InsertColumn(colIndex++, &column);
            }
        }
    }
}

BOOL WINAPI
CPluginDataInterface::GetInfoLineContent(int panel, const CFileData* file, BOOL isDir,
                                         int selectedFiles, int selectedDirs, BOOL displaySize,
                                         const CQuadWord& selectedSize, char* buffer,
                                         DWORD* hotTexts, int& hotTextsCount)
{
    return FALSE;
}

//************************************************
// Nase metody
//************************************************

BOOL CPluginDataInterface::GetShellColumns()
{
    int realIndex;
    for (realIndex = 0;; realIndex++)
    {
        DETAILSINFO di;
        di.fmt = LVCFMT_LEFT;
        di.cxChar = 20;
        di.str.uType = (UINT)-1;
        di.pidl = NULL;

        if (FAILED(GetDetailsHelper(realIndex, &di)))
            break;

        CShellColumn col;
        StrRetToBuf(&di.str, NULL, col.Name, SAL_ARRAYSIZE(col.Name));
        col.Fmt = di.fmt;
        col.Char = di.cxChar;
        col.Flags = SHCOLSTATE_ONBYDEFAULT; // implicitne zobrazeny soloupce

        // zkusime vytahnout default stav od shellu
        if (ShellFolder2 != NULL)
        {
            if (FAILED(ShellFolder2->GetDefaultColumnState(realIndex, &col.Flags)))
                col.Flags = SHCOLSTATE_ONBYDEFAULT;
        }

        ShellColumns.Add(col);
        if (!ShellColumns.IsGood())
        {
            ShellColumns.ResetState();
            break;
        }
    }

    // vybereme viditelne sloupce
    for (realIndex = 0; realIndex < ShellColumns.Count; realIndex++)
    {
        if (ShellColumns[realIndex].Flags & SHCOLSTATE_ONBYDEFAULT)
        {
            VisibleColumns.Add(realIndex);
            if (!VisibleColumns.IsGood())
            {
                VisibleColumns.ResetState();
                break;
            }
        }
    }
    return TRUE;
}

HRESULT
CPluginDataInterface::GetDetailsHelper(int i, DETAILSINFO* di)
{
    HRESULT ret = E_NOTIMPL;

    if (ShellFolder2 != NULL)
        ret = ShellFolder2->GetDetailsOf(di->pidl, i, (SHELLDETAILS*)&di->fmt);

    if (FAILED(ret))
    {
        if (ShellDetails != NULL)
            ret = ShellDetails->GetDetailsOf(di->pidl, i, (SHELLDETAILS*)&di->fmt);
    }

    return ret;
}