// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// posun na dalsi polozku v ID listu
#define GetNextItemFromIL(pidl) ((LPITEMIDLIST)((BYTE*)(pidl) + (pidl)->mkid.cb))

// overi platnost 'pidl'
BOOL CheckPIDL(LPCITEMIDLIST pidl);

// zkrati absolutni pidl 'pidl' o posledni polozku
// v pripade uspechu vrati TRUE + zkraceny pidl 'cutPIDL' a na nej navazany IShellFolder 'cutFolder'
//   'cutPIDL' je po pouziti treba uvolnit pomoci ILFree
//   'cutFolder' je po pouziti treba uvolnit pomoci zavolani jeho metody Release
//   'pidl' neni operaci uvolnen
// v pripade neuspechu vraci FALSE
BOOL CutLastItemFromIL(LPCITEMIDLIST pidl, IShellFolder** cutFolder, LPITEMIDLIST* cutPIDL);

// pripoji k absolutnimu pidlu 'pidl' relativni pidl 'addPIDL'
// v pripade uspechu vrati TRUE + prodlouzeny pidl 'newPIDL' a na nej navazeny IShellFolder 'newFolder'
//   'newPIDL' je po pouziti treba uvolnit pomoci ILFree
//   'newFolder' je po pouziti treba uvolnit pomoci zavolani jeho metody Release
//   'pidl' a 'addPIDL' nejsou operaci uvolneny
// v pripade neuspechu vraci FALSE
BOOL AddItemToIL(LPCITEMIDLIST pidl, LPCITEMIDLIST addPIDL, IShellFolder** newFolder, LPITEMIDLIST* newPIDL);
