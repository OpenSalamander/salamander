// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern int InitExtraction(const char* name, struct SCabinet* cabinet);
extern DWORD WINAPI ExtractArchive(LPVOID cabinet);
extern int HandleError(int titleID, int messageID, unsigned long err, const char* fileName);
extern int HandleErrorW(int titleID, int messageID, unsigned long err, const wchar_t* fileName);

extern void RefreshProgress(unsigned long inend, unsigned long inptr, int diskSavePart); // pokud je diskSavePart==0, jde o prvni pulku progressu (rozbaleni do pameti); pri diskSavePart==1 jde o vybaleni na disk
extern void RefreshName(unsigned char* current_file_name);

extern unsigned long ProgressPos;
extern unsigned char CurrentName[101];
extern HWND DlgWin;
