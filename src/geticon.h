// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//***************************************************************************
//
//  ExtractIcons - extracts 1 or more icons from a file.
//
//  input:
//      szFileName          - EXE/DLL/ICO/CUR/ANI file to extract from
//      nIconIndex          - what icon to extract
//                              0 = first icon, 1=second icon, etc.
//                             -N = icon with id==N
//      cxIcon              - icon size wanted (if HIWORD != 0 two sizes...)
//      cyIcon              - icon size wanted (if HIWORD != 0 two sizes...)
//                            0,0 means extract at natural size.
//      phicon              - place to return extracted icon(s)
//      nIcons              - number of icons to extract.
//      flags               - LoadImage LR_* flags
//
//  returns:
//      if picon is NULL, number of icons in the file is returned.
//
//  notes:
//      handles extraction from PE (Win32), NE (Win16), ICO (Icon),
//      CUR (Cursor), ANI (Animated Cursor), and BMP (Bitmap) files.
//      only Win16 3.x files are supported (not 2.x)
//
//      cx/cyIcon are the size of the icon to extract, two sizes
//      can be extracted by putting size 1 in the loword and size 2 in the
//      hiword, ie MAKELONG(24, 48) would extract 24 and 48 size icons.
//      yea this is a stupid hack it is done so IExtractIcon::Extract
//      can be called by outside people with custom large/small icon
//      sizes that are not what the shell uses internaly.
//
UINT WINAPI ExtractIcons(LPCTSTR szFileName, int nIconIndex, int cxIcon, int cyIcon,
                         HICON* phicon, UINT* piconid, UINT nIcons, UINT flags);

// komentar viz spl_gen.h/GetFileIcon
BOOL GetFileIcon(const char* path, BOOL pathIsPIDL, HICON* hIcon, CIconSizeEnum iconSize,
                 BOOL fallbackToDefIcon, BOOL defIconIsDir);
