// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// move to the next item in the ID list
#define GetNextItemFromIL(pidl) ((LPITEMIDLIST)((BYTE*)(pidl) + (pidl)->mkid.cb))

// verify the validity of 'pidl'
BOOL CheckPIDL(LPCITEMIDLIST pidl);

// shorten the absolute PIDL 'pidl' by the last item
// on success returns TRUE plus the shortened PIDL 'cutPIDL' and the IShellFolder 'cutFolder' bound to it
//   'cutPIDL' must be released with ILFree after use
//   'cutFolder' must be released by calling its Release method after use
//   'pidl' is not released by the operation
// returns FALSE on failure
BOOL CutLastItemFromIL(LPCITEMIDLIST pidl, IShellFolder** cutFolder, LPITEMIDLIST* cutPIDL);

// append the relative PIDL 'addPIDL' to the absolute PIDL 'pidl'
// on success returns TRUE plus the extended PIDL 'newPIDL' and the IShellFolder 'newFolder' bound to it
//   'newPIDL' must be released with ILFree after use
//   'newFolder' must be released by calling its Release method after use
//   'pidl' and 'addPIDL' are not released by the operation
// returns FALSE on failure
BOOL AddItemToIL(LPCITEMIDLIST pidl, LPCITEMIDLIST addPIDL, IShellFolder** newFolder, LPITEMIDLIST* newPIDL);
