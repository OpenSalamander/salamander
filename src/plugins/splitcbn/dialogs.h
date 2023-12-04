// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

BOOL SplitDialog(LPTSTR fileName,        // [in]     jmeno splitovaneho souboru
                 CQuadWord& fileSize,    // [in]     jeho velikost
                 LPTSTR targetDir,       // [in/out] cilovy adresar
                 CQuadWord* partialSize, // [out]    velikost jedne casti
                 HWND hParent);

BOOL CombineDialog(TIndirectArray<char>& files, // [in/out] pole jmen castecnych souboru
                   LPTSTR targetName,           // [in/out] jmeno ciloveho souboru
                   BOOL bOrigCrcFound,          // [in]     flag jestli byl nalezen puvodni CRC
                   UINT32 origCrc,              // [in]     puvodni CRC
                   HWND hParent, CSalamanderForOperationsAbstract* salamander);

void ConfigDialog(HWND hParent);

void CRCDialog(TIndirectArray<char>& files, BOOL bCrcFound, UINT32 origCrc,
               HWND parent, CSalamanderForOperationsAbstract* salamander);
