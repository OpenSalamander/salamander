// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Utils.Array.h"
#include "System.WorkerThread.h"
//#include <stdio.h>

#define ARRAY_BLOCKSIZE_CFILELIST 64
#define MAXREPORTEDFILES 256

#define FILESIZE_DATA 0
#define FILESIZE_REAL 1
#define FILESIZE_DISK 2

#define MYNULL ((TCHAR*)-1)

class CZFile;
class CZDirectory;

class CZFile
{
protected:
    TCHAR* _name;
    TCHAR* _ext; //points to the dot or NULL
    size_t _namelen;
    INT64 _datasize;
    INT64 _realsize;
    INT64 _disksize;
    CZDirectory* _parent;

    FILETIME _createtime;
    FILETIME _modifytime;

public:
    CZFile(CZDirectory* parent, TCHAR const* name, INT64 datasize, INT64 realsize, INT64 disksize, FILETIME* createtime, FILETIME* modifytime)
    {
        this->_parent = parent;
        this->_namelen = _tcslen(name);
        this->_name = (TCHAR*)malloc((this->_namelen + 1) * sizeof TCHAR);
        _tcscpy(this->_name, name);

        this->_datasize = datasize;
        this->_realsize = realsize;
        this->_disksize = disksize;

        if (createtime)
            this->_createtime = *createtime;
        if (modifytime)
            this->_modifytime = *modifytime;

        this->_ext = MYNULL;
    }
    virtual ~CZFile()
    {
        free(this->_name);
    }
    TCHAR const* GetName() { return this->_name; }
    size_t GetNameLen() { return this->_namelen; }
    TCHAR* GetExt()
    {
        if (this->_ext != MYNULL)
            return this->_ext;
        else
            return CalcExt();
    }
    virtual TCHAR* CalcExt()
    {
        //this->_ext = _tcsrchr(this->_name, '.');
        this->_ext = this->_name + this->_namelen; //ukazuje na NULL
        while (--this->_ext != this->_name && *this->_ext != '.')
            ;
        if (*this->_ext != '.')
            this->_ext = NULL;
        return this->_ext;
    }
    FILETIME* GetCreateTime() { return &this->_createtime; }
    FILETIME* GetModifyTime() { return &this->_modifytime; }
    size_t GetFullName(TCHAR* buff, size_t size);
    size_t GetRelativeName(CZDirectory* root, TCHAR* buff, size_t size);
    CZDirectory* GetParent() { return this->_parent; }

    /*CWorkerThread *BeginAsyncIconLoad(HWND owner, UINT msg)
	{
		CWorkerThread *mythread = new CWorkerThread(
			NULL,
			CZIconLoader::IconLoadThreadProc,
			this, 
			owner, 
			msg, 
			this,
			FALSE);
		return mythread;
	}*/

    void LoadFileInfo(TCHAR* displayName, TCHAR* typeName)
    {
        SHFILEINFO shfi;
        TCHAR path[2 * MAX_PATH + 1];
        //SHGetFileInfo(buff, 0, &shfi, sizeof shfi, SHGFI_ICON | SHGFI_SHELLICONSIZE | SHGFI_DISPLAYNAME | SHGFI_TYPENAME);
        UINT flags = 0;
        if (displayName)
        {
            flags |= SHGFI_DISPLAYNAME;
            displayName[0] = TEXT('\0');
        }
        if (typeName)
        {
            flags |= SHGFI_TYPENAME;
            typeName[0] = TEXT('\0');
        }
        if (flags)
        {
            int len = (int)this->GetFullName(path, 2 * MAX_PATH);
            DWORD_PTR result = 0;
            if (len <= MAX_PATH)
            {
                result = SHGetFileInfo(path, 0, &shfi, sizeof shfi, flags);
            }
            else
            {
                result = SHGetFileInfo(this->_name, 0, &shfi, sizeof shfi, flags | SHGFI_USEFILEATTRIBUTES);
            }

            if (result != 0)
            {
                if (displayName)
                    _tcscpy(displayName, shfi.szDisplayName);
                if (typeName)
                    _tcscpy(typeName, shfi.szTypeName);
            }
        }
    }

    //INT64 GetSize() { return this->_datasize; }
    INT64 GetSizeEx(int sizeid = FILESIZE_REAL)
    {
        switch (sizeid)
        {
        case FILESIZE_DATA:
            return this->_datasize;
        case FILESIZE_REAL:
            return this->_realsize;
        case FILESIZE_DISK:
            return this->_disksize;
        default:
            return -1;
        }
    }
    virtual BOOL IsDirectory() { return FALSE; }
};
