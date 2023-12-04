// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

struct SEndingFile
{
    const CFileData* fileData;
    char* path;
    BOOL isDir;
    char* mask;
};

class CNameTree;

struct SBranch
{
    unsigned char ch;
    CNameTree* next;
};

class CNameTree
{
private:
    TDirectArray<SBranch> Branches;
    TDirectArray<SEndingFile>* EndingNames;

public:
    CNameTree() : Branches(1, 1) { EndingNames = NULL; }
    ~CNameTree();
    void Add(const char* name, const BOOL isDir, const char* path, const CFileData* fileData);
    BOOL IsNamePresent(const char* name, const BOOL hasExtension);
};

class CNames
{
private:
    CNameTree NameTree;

public:
    CNames(){};
    ~CNames(){};
    void AddName(const char* name, const BOOL isDir, const char* path, const CFileData* fileData)
    {
        if (name == NULL || *name == '\0')
            return;
        NameTree.Add(name, isDir, path, fileData);
    }
    BOOL IsNamePresent(const char* name)
    {
        return NameTree.IsNamePresent(name, strchr(name, '.') != NULL);
    }
};
