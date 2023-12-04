// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>

#include "lstrfix.h"
#include "..\shexreg.h"
#include "shellext.h"

#ifdef ENABLE_SH_MENU_EXT

DWORD SalGetFileAttributes(const char* fileName)
{
    int fileNameLen = (int)strlen(fileName);
    char fileNameCopy[3 * MAX_PATH];
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak GetFileAttributes
    // mezery/tecky orizne a pracuje tak s jinou cestou + u souboru to sice nefunguje,
    // ale porad lepsi nez ziskat atributy jineho souboru/adresare (pro "c:\\file.txt   "
    // pracuje se jmenem "c:\\file.txt")
    if (fileNameLen > 0 && (fileName[fileNameLen - 1] <= ' ' || fileName[fileNameLen - 1] == '.') &&
        fileNameLen + 1 < _countof(fileNameCopy))
    {
        memcpy(fileNameCopy, fileName, fileNameLen);
        fileNameCopy[fileNameLen] = '\\';
        fileNameCopy[fileNameLen + 1] = 0;
        return GetFileAttributes(fileNameCopy);
    }
    else // obycejna cesta, neni co resit, jen zavolame windowsovou GetFileAttributes
    {
        return GetFileAttributes(fileName);
    }
}

BOOL IncFilesDirs(const char* path, int* files, int* dirs)
{
    DWORD attrs = SalGetFileAttributes(path);
    if (attrs == 0xFFFFFFFF)
        return FALSE;

    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        (*dirs)++;
    else
        (*files)++;

    return TRUE;
}

STDMETHODIMP SE_QueryContextMenu(THIS_
                                     HMENU hMenu,
                                 UINT indexMenu,
                                 UINT idCmdFirst,
                                 UINT idCmdLast,
                                 UINT uFlags)
{
    UINT idCmd = idCmdFirst;

    char buff[MAX_PATH];

    /*
  char buff1[1000];
  wsprintf(buff1, "SE_QueryContextMenu");
  MessageBox(NULL, buff1, "shellext.dll", MB_OK | MB_ICONINFORMATION);
*/

    if ((uFlags & 0x000F) == CMF_NORMAL || (uFlags & CMF_VERBSONLY) || (uFlags & CMF_EXPLORE))
    {
        HMENU hTmpMenu = hMenu;
        BOOL subMenu = FALSE;
        BOOL of;
        BOOL mf;
        BOOL od;
        BOOL md;

        LPDATAOBJECT pDataObj = ((ShellExt*)This)->m_pDataObj;

        int index = 0;
        int itemsCount = 0;
        CShellExtConfigItem* iterator = ShellExtConfigFirst;

        int filesCount = 0;
        int dirsCount = 0;

        FORMATETC formatEtc;
        STGMEDIUM stgMedium;
        char path[MAX_PATH];

        formatEtc.cfFormat = CF_HDROP;
        formatEtc.ptd = NULL;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;

        stgMedium.tymed = TYMED_HGLOBAL;
        stgMedium.hGlobal = NULL;
        stgMedium.pUnkForRelease = NULL;

        // vytahnu si seznam souboru a adresaru, na ktere je kliknuto
        // projdu ho a zjistim kolik je v nem souboru a kolik adresaru

        if (pDataObj->lpVtbl->GetData(pDataObj, &formatEtc, &stgMedium) == S_OK)
        {
            if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
            {
                DROPFILES* data = (DROPFILES*)GlobalLock(stgMedium.hGlobal);
                if (data != NULL)
                {
                    int l;
                    if (data->fWide)
                    {
                        const wchar_t* fileW = (wchar_t*)(((char*)data) + data->pFiles);
                        do
                        {
                            l = lstrlenW(fileW);
                            WideCharToMultiByte(CP_ACP, 0, fileW, l + 1, path, l + 1, NULL, NULL);
                            path[l] = 0;
                            IncFilesDirs(path, &filesCount, &dirsCount);
                            fileW += l + 1;
                        } while (*fileW != 0);
                    }
                    else
                    {
                        const char* fileA = ((char*)data) + data->pFiles;
                        do
                        {
                            l = lstrlen(fileA);
                            IncFilesDirs(fileA, &filesCount, &dirsCount);
                            fileA += l + 1;
                            filesCount++;
                        } while (*fileA != 0);
                    }
                    GlobalUnlock(stgMedium.hGlobal);
                }
            }
            ReleaseStgMedium(&stgMedium);
        }

        // potom se vetru do kontextoveho menu
        of = filesCount == 1;
        mf = filesCount > 1;
        od = dirsCount == 1;
        md = dirsCount > 1;

        if (ShellExtConfigSubmenu)
        {
            hTmpMenu = CreatePopupMenu();
            subMenu = TRUE;
            indexMenu = 0;
        }

        // projedu vsechny itemy a pokud vyhovuji podmince, zaradim je do menu
        while (iterator != NULL)
        {
            // tahle podminka je na hovno - je treba predelat kriteria v Salamu
            if ((iterator->LogicalAnd && (iterator->OneFile == of) && (iterator->MoreFiles == mf) &&
                 (iterator->OneDirectory == od) && (iterator->MoreDirectories == md)) ||
                (!iterator->LogicalAnd && ((iterator->OneFile == of) || (iterator->MoreFiles == mf) ||
                                           (iterator->OneDirectory == od) || (iterator->MoreDirectories == md))))
            {
                lstrcpy(buff, iterator->Name);
                InsertMenu(hTmpMenu,
                           indexMenu++,
                           MF_STRING | MF_BYPOSITION,
                           idCmd++,
                           buff);
                iterator->Cmd = index;
                index++;
                itemsCount++;
            }
            iterator = iterator->Next;
        }

        // pokud je neco v submenu, tak ho vlozime do ContextMenu
        // jinak ho sestrelim
        if (ShellExtConfigSubmenu)
        {
            if (itemsCount > 0)
            {
                InsertMenu(hMenu,
                           indexMenu,
                           MF_POPUP | MF_BYPOSITION,
                           (UINT_PTR)hTmpMenu,
                           ShellExtConfigSubmenuName);
            }
            else
            {
                // musim po sobe uklidit submenu
                DestroyMenu(hMenu);
            }
        }

        //Must return number of menu items we added.
        return itemsCount;
    }
    return NOERROR;
}

STDMETHODIMP SE_InvokeCommand(THIS_
                                  LPCMINVOKECOMMANDINFO lpici)
{
    int index; // jde o index do spojaku zacinajiciho na ShellExtConfigFirst
    if (SECGetItemIndex(LOWORD(lpici->lpVerb), &index))
    {
        CShellExtConfigItem* item = SECGetItem(index);
        if (item != NULL)
        {
            char buff[1000];
            wsprintf(buff, "SE_InvokeCommand index = %d\nitem ptr = 0x%p\nIndex je treba predat do Salamandera, kde pres nej vytahnem ukazatel na itemu.", index, item);
            MessageBox(NULL, buff, "shellext.dll", MB_OK | MB_ICONINFORMATION);

            // sem prijde komunikace se Salamanderem

            // seznam souboru, ktere jsou kliknute lze obdrzet stejnou metodou,
            // jakou pouzivam v metode SE_QueryContextMenu
            // problem: muze to tam prijit jak v unicodu tak normalne
            // takze to tam prevadim a mozna by bylo lepsi nechat tenhle prevod
            // udelat az Salama - abychom to v nem meli v unicodu

            return NOERROR;
        }
    }
    return E_INVALIDARG;
}

STDMETHODIMP SE_GetCommandString(THIS_
                                     UINT idCmd,
                                 UINT uType,
                                 UINT* pwReserved,
                                 LPSTR pszName,
                                 UINT cchMax)
{
    if (pszName != NULL)
        *pszName = 0;

    // retezec, ktery se ukazuje v status bare pri vyvolani context menu z exploderu
    // museli bychom ho nechat naeditovat k jednotlivym itemam - zatim na to pecu

    return NOERROR;
}

#endif // ENABLE_SH_MENU_EXT