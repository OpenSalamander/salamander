// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern int InitExtraction(const char* name, struct SCabinet* cabinet);
extern DWORD WINAPI ExtractArchive(LPVOID cabinet);
extern int HandleError(int titleID, int messageID, unsigned long err, const char* fileName);
extern int HandleErrorW(int titleID, int messageID, unsigned long err, const wchar_t* fileName);

extern void RefreshProgress(unsigned long inend, unsigned long inptr, int diskSavePart); // if diskSavePart==0 this is the first half of progress (in-memory extraction); when diskSavePart==1 it represents writing to disk
extern void RefreshName(unsigned char* current_file_name);

extern unsigned long ProgressPos;
extern unsigned char CurrentName[101];
extern HWND DlgWin;
