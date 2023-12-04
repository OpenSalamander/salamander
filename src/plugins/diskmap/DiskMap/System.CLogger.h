// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "System.Lock.h"
#include "Utils.Array.h"
#include "Utils.CZString.h"

#define LOG_INFORMATION 0
#define LOG_WARNING 1
#define LOG_ERROR 2

#define COUNT_LOGLEVELS 3

#define ARRAY_BLOCKSIZE_CLOG 32

class CBasicLogItem;
class CStaticLogItem;
class CSmartLogItem;

class CLogItemBase
{
protected:
    int _level;

public:
    CLogItemBase(int level)
    {
        this->_level = level;
    }
    virtual ~CLogItemBase() {}
    int GetLevel() const { return this->_level; }
    virtual TCHAR const* GetText() const = 0;
    virtual TCHAR const* GetPath() const = 0;
};

class CBasicLogItem : public CLogItemBase
{
protected:
    TCHAR const* _text;
    TCHAR const* _path;

public:
    CBasicLogItem(int level, TCHAR const* text, TCHAR const* path) : CLogItemBase(level)
    {
        this->_text = text;
        this->_path = path;
    }
    ~CBasicLogItem() {}
    TCHAR const* GetText() const { return this->_text; }
    TCHAR const* GetPath() const { return this->_path; }
};

class CStaticLogItem : public CLogItemBase
{
protected:
    TCHAR const* _text;
    CZString const* _path;

public:
    CStaticLogItem(int level, TCHAR const* text, CZString const* path) : CLogItemBase(level)
    {
        this->_text = text;
        this->_path = path;
    }
    ~CStaticLogItem()
    {
        if (this->_path)
            delete this->_path;
        this->_path = NULL;
    }
    TCHAR const* GetText() const { return this->_text; }
    TCHAR const* GetPath() const { return this->_path->GetString(); }
};

class CSmartLogItem : public CLogItemBase
{
protected:
    CZString const* _text;
    CZString const* _path;

public:
    CSmartLogItem(int level, CZString const* text, CZString const* path) : CLogItemBase(level)
    {
        this->_text = text;
        this->_path = path;
    }
    ~CSmartLogItem()
    {
        if (this->_text)
            delete this->_text;
        this->_text = NULL;
        if (this->_path)
            delete this->_path;
        this->_path = NULL;
    }
    TCHAR const* GetText() const { return this->_text->GetString(); }
    TCHAR const* GetPath() const { return this->_path->GetString(); }
};

class CLogger
{
protected:
    TAutoIndirectArray<CLogItemBase>* _log;
    CLock* _lock;

    HWND _logwnd;
    DWORD _msg;
    volatile BOOL _announced;

    volatile int _count;

public:
    CLogger()
    {
        this->_count = 0;
        this->_log = NULL;
        this->_lock = new CLock();

        this->Clear();
    }
    ~CLogger()
    {
        if (this->_log)
            delete this->_log;
        this->_log = NULL;
        if (this->_lock)
            delete this->_lock;
        this->_lock = NULL;
    }

    static CLogItemBase* CreateLogItem(int level, TCHAR const* text, TCHAR const* path)
    {
        return new CBasicLogItem(level, text, path);
    }
    static CLogItemBase* CreateLogItem(int level, TCHAR const* text, CZString const* path)
    {
        if (path == NULL)
            return new CBasicLogItem(level, text, NULL);
        return new CStaticLogItem(level, text, path);
    }
    static CLogItemBase* CreateLogItem(int level, CZString const* text, CZString const* path)
    {
        return new CSmartLogItem(level, text, path);
    }

    void AttachWindow(HWND hWnd, DWORD msg)
    {
        this->_lock->Enter();

        this->_logwnd = hWnd;
        this->_msg = msg;
        this->_announced = FALSE;

        this->_lock->Leave();
    }
    void ClearAnnounceFlag()
    {
        this->_announced = FALSE;
    }
    void AnnounceChange()
    {
        if (this->_announced == FALSE && this->_logwnd != NULL)
        {
            PostMessage(this->_logwnd, this->_msg, (WPARAM)this->GetLogCount(), (LPARAM)this);
            //this->_announced = TRUE;
        }
    }
    void Log(const CLogItemBase* logitem)
    {
        this->_lock->Enter();

        if (this->_log != NULL)
        {
            this->_log->Add(logitem);
            this->_count++;
            this->AnnounceChange();
        }

        this->_lock->Leave();
    }
    void Clear()
    {
        this->_lock->Enter();

        if (this->_log)
            delete this->_log;
        this->_log = NULL;
        this->_log = new TAutoIndirectArray<CLogItemBase>(ARRAY_BLOCKSIZE_CLOG, TRUE);
        this->_count = 0;

        this->_lock->Leave();
    }
    int GetLogCount() const { return this->_count; }
    CLogItemBase* GetLogItem(int i)
    {
        CLogItemBase* lgi;
        this->_lock->Enter();
        lgi = this->_log->At(i);
        this->_lock->Leave();
        return lgi;
    }
};
