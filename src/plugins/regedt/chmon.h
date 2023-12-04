// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

enum CChangeMonitorThreadState
{
    cmtIdle,
    cmtWaitingForChange
};
enum CActionEvent
{
    aeNoAction,
    aeSetPath,
    aeFinish,
    aeCancel
};

class CChangeMonitor;

class CChangeMonitorThread : public CThread
{
    CChangeMonitorThreadState State;
    TIndirectArray<CPluginFSInterface> ConnectedFS;
    int Root;
    WCHAR Key[MAX_KEYNAME];
    HANDLE ActionEvent;
    CActionEvent Action;
    HANDLE RegistryEvent;
    CChangeMonitor& Monitor;
    int IgnoreChanges;

public:
    CChangeMonitorThread(CChangeMonitor& monitor);
    ~CChangeMonitorThread();
    virtual unsigned Body();

    friend class CChangeMonitor;
};

class CChangeMonitor
{
    TIndirectArray<CChangeMonitorThread> Threads;
    CCS CS;

public:
    CChangeMonitor();
    ~CChangeMonitor();
    void AddPath(int root, LPWSTR key, CPluginFSInterface* fs);
    void Cancel(CPluginFSInterface* fs);
    void Stop();
    void IgnoreNextRootChange(int root);

    friend CChangeMonitorThread;
};

extern CChangeMonitor ChangeMonitor;

/*
class CChangeMonitor : public CThread
{
    CPluginFSInterface *FS;
    int Root;
    WCHAR Key[MAX_KEYNAME];
    HANDLE ActionEvent;
    enum {aeNoAction, aeSetPath, aeFinish, aeCancel} Action;
    HANDLE RegistryEvent;
    CCS CS;
  
  public:
    CChangeMonitor(CPluginFSInterface * fs);
    ~CChangeMonitor();
    void SetPath(int root, WCHAR * key);
    void Cancel();
    void Finish();
    virtual unsigned Body();
};
*/
