// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Utils.Array.h"
#include "System.RWLock.h"
#include "System.WorkerThread.h"
#include "TreeMap.FileData.CZFile.h"

#define ARRAY_BLOCKSIZE_CFILELIST 64
#define MAXREPORTEDFILES 256

class CZFile;
class CZDirectory;
class CZRoot;

class CZDirectory : public CZFile
{
protected:
    TAutoIndirectArray<CZFile>* _files;

    int _filecount;
    int _dircount;

    CZRoot* _root;

    INT64 PopulateDir(CWorkerThread* mythread, TCHAR* path, int pos, size_t pathsize);

    CZDirectory(CZDirectory* parent, TCHAR const* name, FILETIME* createtime, FILETIME* modifytime) : CZFile(parent, name, 0, 0, 0, createtime, modifytime)
    {
        this->_files = new TAutoIndirectArray<CZFile>(ARRAY_BLOCKSIZE_CFILELIST, TRUE);

        this->_filecount = 0;
        this->_dircount = 0;

        if (parent != NULL)
        {
            this->_root = parent->_root;
        }
        else
        {
            this->_root = NULL;
        }
    }

public:
    virtual ~CZDirectory()
    {
        delete this->_files;
    }
    int GetFileCount() { return this->_files->GetCount(); }
    CZFile* GetFile(int i) { return this->_files->At(i); }

    virtual TCHAR* CalcExt() { return this->_ext = NULL; } //slozky nemaji pripony

    int GetSubFileCount() const { return this->_filecount; }
    int GetSubDirsCount() const { return this->_dircount; }

    CZRoot* GetRoot()
    {
        return this->_root;
    }
    virtual BOOL IsDirectory() { return TRUE; }

    void TEST_ENUM(FILE* fileHandle);
    void TEST();
};
