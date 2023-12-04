// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dlldefs.h"
#include "names.h"

CNameTree::~CNameTree()
{
    CALL_STACK_MESSAGE1("CNameTree::~CNameTree()");
    int i;
    // delete sub-tree
    for (i = 0; i < Branches.Count; i++)
        delete Branches.At(i).next;
    // delete local data
    if (EndingNames != NULL)
    {
        for (i = 0; i < EndingNames->Count; i++)
        {
            SEndingFile* tmp;
            tmp = &EndingNames->At(i);
            if (tmp->path != NULL)
                free(tmp->path);
            if (tmp->mask != NULL)
                free(tmp->mask);
        }
        delete EndingNames;
    }
}

void CNameTree::Add(const char* name, const BOOL isDir, const char* path, const CFileData* fileData)
{
    CALL_STACK_MESSAGE4("CNameTree::Add(%s, %d, %s, )", name, isDir, path);
    if (*name == '\0' || *name == '*' || *name == '?')
    {
        // nazev zde konci (alespon exaktni, bezmaskova cast)
        if (EndingNames == NULL)
            EndingNames = new TDirectArray<SEndingFile>(1, 1);

        SEndingFile tmpEndingFile;
        if (path != NULL && *path != '\0')
        {
            tmpEndingFile.path = (char*)malloc(strlen(path) + 1);
            strcpy(tmpEndingFile.path, path);
        }
        else
            tmpEndingFile.path = NULL;
        tmpEndingFile.isDir = isDir;
        tmpEndingFile.fileData = fileData;
        if (*name != '\0')
        {
            tmpEndingFile.mask = (char*)malloc(strlen(name) + 1);
            SalamanderGeneral->PrepareMask(tmpEndingFile.mask, name);
        }
        else
            tmpEndingFile.mask = NULL;
        EndingNames->Add(tmpEndingFile);
    }
    else
    {
        // jen prochazime - najdeme (binarnim pulenim) v branches objekt odpovidajici *name
        int left = 0;
        int right = Branches.Count - 1;
        while (left < right)
        {
            int index = (left + right) / 2;
            if (Branches.At(index).ch == *name)
                left = right = index;
            else if (Branches.At(index).ch > *name)
                right = index - 1;
            else
                left = index + 1;
        }
        // nasli moji radcove ? (ted se left == right == index)
        if (Branches.Count == 0 || Branches.At(left).ch != *name)
        {
            // jeste neexistuje - musime ho vytvorit a pridat
            SBranch tmpBranch;
            tmpBranch.ch = *name;
            tmpBranch.next = new CNameTree;
            if (Branches.Count == 0)
                left = Branches.Add(tmpBranch);
            else if (Branches.At(left).ch > *name)
                Branches.Insert(left, tmpBranch);
            else if (Branches.Count - 1 > left)
                Branches.Insert(++left, tmpBranch);
            else
                left = Branches.Add(tmpBranch);
        }
        Branches.At(left).next->Add(name + 1, isDir, path, fileData);
    }
}

BOOL CNameTree::IsNamePresent(const char* name, const BOOL hasExtension)
{
    SLOW_CALL_STACK_MESSAGE3("CNameTree::IsNamePresent(%s, %d)", name, hasExtension);
    if (EndingNames != NULL)
    {
        int i;
        for (i = 0; i < EndingNames->Count; i++)
            if ((EndingNames->At(i).mask == NULL &&
                 ((EndingNames->At(i).isDir && (*name == '\\')) || *name == '\0')) ||
                (EndingNames->At(i).mask != NULL &&
                 SalamanderGeneral->AgreeMask(name, EndingNames->At(i).mask, hasExtension)))
                return TRUE;
    }
    // ted najdeme kam dal - opet binarni puleni
    int left = 0;
    int right = Branches.Count - 1;
    while (left < right)
    {
        int index = (left + right) / 2;
        if (Branches.At(index).ch == *name)
            left = right = index;
        else if (Branches.At(index).ch > *name)
            right = index - 1;
        else
            left = index + 1;
    }
    // nasli moji radcove ? (ted se left == right == index)
    // pokud ano, hledame rekurzivne dal
    if (Branches.Count == 0 || Branches.At(left).ch != *name)
        return FALSE;
    else
        return Branches.At(left).next->IsNamePresent(name + 1, hasExtension);
}
