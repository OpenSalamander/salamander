// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "iltools.h"

BOOL CheckPIDL(LPCITEMIDLIST pidl)
{
    if (IsBadReadPtr(pidl, sizeof(USHORT /* pidl->mkid.cb */)) ||
        IsBadReadPtr(pidl, pidl->mkid.cb + sizeof(USHORT /* terminating pidl->mkid.cb, which equals 0 */)))
    {
        TRACE_E("Unable to read PIDL 0x" << pidl);
        return FALSE;
    }
    LPCITEMIDLIST nextItem = GetNextItemFromIL(pidl);
    if (nextItem->mkid.cb == 0)
        return TRUE; // already at the terminator, success
    else
        return CheckPIDL(nextItem);
}

BOOL CutLastItemFromIL(LPCITEMIDLIST pidl, IShellFolder** cutFolder, LPITEMIDLIST* cutPIDL)
{
    if (!ILIsEmpty(pidl)) // Desktop -- cannot be shortened
    {
        LPITEMIDLIST cutPIDLAux = ILClone(pidl);
        if (cutPIDLAux != NULL)
        {
            if (!ILIsEmpty(cutPIDLAux) && ILRemoveLastID(cutPIDLAux))
            {
                // obtain the Desktop folder
                HRESULT hr;
                IShellFolder* desktopFolder;
                if (SUCCEEDED(hr = SHGetDesktopFolder(&desktopFolder)))
                {
                    IShellFolder* cutFolderAux;
                    if (ILIsEmpty(cutPIDLAux))
                        cutFolderAux = desktopFolder;
                    else
                    {
                        hr = desktopFolder->BindToObject(cutPIDLAux, NULL, IID_IShellFolder, (void**)&cutFolderAux);
                        desktopFolder->Release();
                    }
                    if (SUCCEEDED(hr))
                    {
                        *cutPIDL = cutPIDLAux;
                        *cutFolder = cutFolderAux;
                        return TRUE;
                    }
                    else
                        TRACE_E("CutLastItemFromIL(): BindToObject failed");
                }
                else
                    TRACE_E("CutLastItemFromIL(): SHGetDesktopFolder failed");
            }
            else
                TRACE_E("CutLastItemFromIL(): ILRemoveLastID failed");
            ILFree(cutPIDLAux);
        }
        else
            TRACE_E("CutLastItemFromIL(): ILClone failed");
    }
    return FALSE;
}

BOOL AddItemToIL(LPCITEMIDLIST pidl, LPCITEMIDLIST addPIDL, IShellFolder** newFolder, LPITEMIDLIST* newPIDL)
{
    LPITEMIDLIST newPIDLAux = ILCombine(pidl, addPIDL);
    if (newPIDLAux != NULL)
    {
        // obtain the Desktop folder
        IShellFolder* desktopFolder;
        if (SUCCEEDED(SHGetDesktopFolder(&desktopFolder)))
        {
            // obtain IShellFolder for the new PIDL
            IShellFolder* newFolderAux;
            HRESULT hr = desktopFolder->BindToObject(newPIDLAux, NULL, IID_IShellFolder, (void**)&newFolderAux);
            desktopFolder->Release();
            if (SUCCEEDED(hr))
            {
                *newPIDL = newPIDLAux;
                *newFolder = newFolderAux;
                return TRUE;
            }
            else
                TRACE_E("AddItemToIL(): BindToObject failed");
        }
        else
            TRACE_E("AddItemToIL(): SHGetDesktopFolder failed");
        ILFree(newPIDLAux);
    }
    else
        TRACE_E("AddItemToIL(): ILCombine failed");
    return FALSE;
}