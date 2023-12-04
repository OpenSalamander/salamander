// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

BOOL CombineFiles(TIndirectArray<char>& files, LPTSTR targetName,
                  BOOL bOnlyCrc, BOOL bTestCrc, UINT32& Crc,
                  BOOL bTime, FILETIME* origTime, HWND parent,
                  CSalamanderForOperationsAbstract* salamander);

BOOL CalculateFileCRC(UINT32& Crc, HWND parent, CSalamanderForOperationsAbstract* salamander);

BOOL CombineCommand(DWORD eventMask, HWND parent, CSalamanderForOperationsAbstract* salamander);
