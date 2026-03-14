// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

class CFilesArray;
struct CFileData;

enum CSortType
{
    stName,
    stExtension,
    stTime,
    stSize,
    stAttr
};

void SortFilesAndDirectories(CFilesArray* files, CFilesArray* dirs,
                             CSortType sortType, BOOL reverseSort, BOOL sortDirsByName);

void SortNameExt(CFilesArray& files, int left, int right, BOOL reverse);
void SortExtName(CFilesArray& files, int left, int right, BOOL reverse);
void SortTimeNameExt(CFilesArray& files, int left, int right, BOOL reverse);
void SortSizeNameExt(CFilesArray& files, int left, int right, BOOL reverse);
void SortAttrNameExt(CFilesArray& files, int left, int right, BOOL reverse);

typedef BOOL (*CLessFunction)(const CFileData&, const CFileData&, BOOL);

// comparison for two files, 1st key name, 2nd key extension, returns -1, 0, 1 like strcmp
int CmpNameExt(const CFileData& f1, const CFileData& f2);
int CmpNameExtIgnCase(const CFileData& f1, const CFileData& f2); // ignore-case variant

// NOTE: sort codes in RefreshDirectory, ChangeSortType and CompareDirectories must match!!!

BOOL LessNameExt(const CFileData& f1, const CFileData& f2, BOOL reverse);
BOOL LessNameExtIgnCase(const CFileData& f1, const CFileData& f2, BOOL reverse);
BOOL LessExtName(const CFileData& f1, const CFileData& f2, BOOL reverse);
BOOL LessTimeNameExt(const CFileData& f1, const CFileData& f2, BOOL reverse);
BOOL LessSizeNameExt(const CFileData& f1, const CFileData& f2, BOOL reverse);
BOOL LessAttrNameExt(const CFileData& f1, const CFileData& f2, BOOL reverse);

void IntSort(int array[], int left, int right);

// StrICmp for regional-settings sort and detect-numbers sort; if 'numericalyEqual' is not
// NULL it returns TRUE when the strings are numerically equal (for example "a01" and "a1")
int RegSetStrICmp(const char* s1, const char* s2);
int RegSetStrICmpEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual);

// strcmp for regional-settings sort and detect-numbers sort; if 'numericalyEqual' is not
// NULL it returns TRUE when the strings are numerically equal (for example "a01" and "a1")
int RegSetStrCmp(const char* s1, const char* s2);
int RegSetStrCmpEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual);

int StrCmpLogicalEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual, BOOL ignoreCase);
