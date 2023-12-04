// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TreeMap.FileData.CZFile.h"

#define SHGFI_ADDOVERLAYS 0x000000020 // apply the appropriate overlays

#define SHGFI_ICONMASK (SHGFI_ICON | SHGFI_SELECTED | SHGFI_LARGEICON | SHGFI_SMALLICON | SHGFI_OPENICON | SHGFI_SHELLICONSIZE | SHGFI_ADDOVERLAYS | SHGFI_USEFILEATTRIBUTES)

class CZIconLoader
{
protected:
    DWORD _flags;
    TCHAR _filename[2 * MAX_PATH + 1];

    static DWORD_PTR WINAPI LoadIconThreadProc(CWorkerThread* mythread, LPVOID lpParam)
    {
        CZIconLoader* self = (CZIconLoader*)lpParam;
        HICON hicon = CZIconLoader::LoadIconSync(self->_filename, self->_flags);
        delete self;
        return (DWORD_PTR)hicon;
    }
    CZIconLoader(CZFile* file, DWORD flags)
    {
        int len = (int)file->GetFullName(this->_filename, ARRAYSIZE(CZIconLoader::_filename));
        //pokud moc dlouha cesta, kterou SHGetFileInfo() nezvladne...
        if (len > MAX_PATH)
        {
            _tcscpy(this->_filename, file->GetName());
            flags |= SHGFI_USEFILEATTRIBUTES;
            //flags &= ~SHGFI_ADDOVERLAYS; //vypada, ze neni nutne
        }
        this->_flags = flags;
    }
    CZIconLoader(TCHAR const* path, DWORD flags)
    {
        _tcscpy(this->_filename, path);
        this->_flags = flags;
    }
    ~CZIconLoader()
    {
    }
    static CWorkerThread* BeginAsyncLoadIcon(CZIconLoader* self, HWND owner, UINT msg, LPVOID lParam)
    {
        CWorkerThread* mythread = new CWorkerThread(
            NULL,
            CZIconLoader::LoadIconThreadProc,
            self,
            owner,
            msg,
            lParam,
            FALSE);
        return mythread;
    }

public:
    static CWorkerThread* BeginAsyncLoadIcon(CZFile* file, DWORD flags, HWND owner, UINT msg)
    {
        CZIconLoader* self = new CZIconLoader(file, flags);
        return CZIconLoader::BeginAsyncLoadIcon(self, owner, msg, file);
    }
    static CWorkerThread* BeginAsyncLoadIcon(CZString const* path, DWORD flags, HWND owner, UINT msg, LPVOID lParam)
    {
        CZIconLoader* self = new CZIconLoader(path->GetString(), flags);
        return CZIconLoader::BeginAsyncLoadIcon(self, owner, msg, lParam);
    }
    static HICON LoadIconSync(CZFile* file, DWORD flags)
    {
        TCHAR path[2 * MAX_PATH + 1];
        file->GetFullName(path, ARRAYSIZE(path));
        return CZIconLoader::LoadIconSync(path, flags);
    }
    static HICON LoadIconSync(TCHAR* path, DWORD flags)
    {
        flags |= SHGFI_ICON;     //ziskej ikonu
        flags &= SHGFI_ICONMASK; //vyskrtni nezadouci flagy
        SHFILEINFO shfi;
        if (SHGetFileInfo(path, 0, &shfi, sizeof shfi, flags) == 0)
            return NULL;
        return shfi.hIcon;
    }
};
