// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>

#include "lstrfix.h"
#include "..\shexreg.h"
#include "shellext.h"

#undef INTERFACE
#define INTERFACE ImpIShellExtInit

STDMETHODIMP SEI_QueryInterface(THIS_ REFIID riid, LPVOID* ppvObj)
{
    ShellExtInit* sei = (ShellExtInit*)This;
    // delegace

    return sei->m_pObj->lpVtbl->QueryInterface((IShellExt*)sei->m_pObj, riid, ppvObj);
}

STDMETHODIMP_(ULONG)
SEI_AddRef(THIS)
{
    ShellExtInit* sei = (ShellExtInit*)This;
    // delegace
    return sei->m_pObj->lpVtbl->AddRef((IShellExt*)sei->m_pObj);
}

STDMETHODIMP_(ULONG)
SEI_Release(THIS)
{
    ShellExtInit* sei = (ShellExtInit*)This;
    // delegace
    return sei->m_pObj->lpVtbl->Release((IShellExt*)sei->m_pObj);
}

//
//  FUNCTION: CShellExt::Initialize(LPCITEMIDLIST, LPDATAOBJECT, HKEY)
//
//  PURPOSE: Called by the shell when initializing a context menu or property
//           sheet extension.
//
//  PARAMETERS:
//    pIDFolder - Specifies the parent folder
//    pDataObj  - Spefifies the set of items selected in that folder.
//    hRegKey   - Specifies the type of the focused item in the selection.
//
//  RETURN VALUE:
//
//    NOERROR in all cases.
//
//  COMMENTS:   Note that at the time this function is called, we don't know
//              (or care) what type of shell extension is being initialized.
//              It could be a context menu or a property sheet.
//

STDMETHODIMP SEI_Initialize(THIS_
                                LPCITEMIDLIST pIDFolder,
                            LPDATAOBJECT pDataObj,
                            HKEY hRegKey)
{
    ShellExtInit* sei = (ShellExtInit*)This;

    WriteToLog("SEI_Initialize");

    //  sei->m_pObj->m_hRegKey = hRegKey;

    // Initialize can be called more than once
    if (sei->m_pObj->m_pDataObj != NULL)
        sei->m_pObj->m_pDataObj->lpVtbl->Release(sei->m_pObj->m_pDataObj);

    //  if (sei->m_pObj->m_pIDFolder != NULL)
    //    ILFree(sei->m_pObj->m_pIDFolder);

    // duplicate the object pointers
    sei->m_pObj->m_pDataObj = pDataObj;
    if (pDataObj != NULL)
        pDataObj->lpVtbl->AddRef(pDataObj);

    //  if (pIDFolder != NULL)
    //  {
    //    sei->m_pObj->m_pIDFolder = ILClone(pIDFolder);
    //    if (sei->m_pObj->m_pIDFolder == NULL)
    //      return(E_OUTOFMEMORY);
    //  }

    return NOERROR;
}
