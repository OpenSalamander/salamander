// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

void InitUtils();

int StrICmp(const char* s1, const char* s2);
void* mini_memcpy(void* out, const void* in, int len);
char* mystrstr(char* a, char* b);
BOOL GetFolderPath(int nFolder, LPTSTR pszPath); //pszPath musi byt minimalne MAX_PATH znaku!
//BOOL Is64BitWindows();
