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
    // if the path ends with a space/period we must append '\\', otherwise GetFileAttributes
    // trims the spaces/periods and works with a different path; with files it still does not
    // work, but it is better than retrieving attributes of another file/directory (for
    // "c:\\file.txt   " it works with the name "c:\\file.txt")
    if (fileNameLen > 0 && (fileName[fileNameLen - 1] <= ' ' || fileName[fileNameLen - 1] == '.') &&
        fileNameLen + 1 < _countof(fileNameCopy))
    {
        memcpy(fileNameCopy, fileName, fileNameLen);
        fileNameCopy[fileNameLen] = '\\';
        fileNameCopy[fileNameLen + 1] = 0;
        return GetFileAttributes(fileNameCopy);
    }
    else // a regular path, nothing to solve, just call the Windows GetFileAttributes
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

        // fetch the list of files and directories that were clicked
        // walk it and find out how many files and how many directories it contains

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

        // then dive into the context menu
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

        // iterate through all items and if they meet the condition, add them to the menu
        while (iterator != NULL)
        {
            // this condition is awful; the selection criteria in Salamander need a redesign
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

        // if there is something in the submenu, insert it into the ContextMenu
        // otherwise remove it
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
                // we must clean up after ourselves and destroy the submenu
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
    int index; // index into the linked list starting at ShellExtConfigFirst
    if (SECGetItemIndex(LOWORD(lpici->lpVerb), &index))
    {
        CShellExtConfigItem* item = SECGetItem(index);
        if (item != NULL)
        {
            char buff[1000];
            wsprintf(buff,
                     "SE_InvokeCommand index = %d\nitem ptr = 0x%p\nThe index has to be passed to Salamander so we can retrieve the pointer to the item through it.",
                     index, item);
            MessageBox(NULL, buff, "shellext.dll", MB_OK | MB_ICONINFORMATION);

            // this is where the communication with Salamander will go

            // the list of clicked files can be obtained by the same method
            // that I use in SE_QueryContextMenu
            // problem: it can arrive either in Unicode or ANSI
            // so I am converting it there and maybe it would be better to leave the conversion
            // to Salamander so that we have it in Unicode

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

    // the string shown in the status bar when the context menu is invoked from Explorer
    // we would have to let it be edited per item - for now I am ignoring it

    return NOERROR;
}

#endif // ENABLE_SH_MENU_EXT
