// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

CChangeMonitor ChangeMonitor;

// ****************************************************************************
//
// CChangeMonitorThread
//
//

CChangeMonitorThread::CChangeMonitorThread(CChangeMonitor& monitor)
    : ConnectedFS(4, 4, dtNoDelete), CThread("Registry Change Monitor"),
      Monitor(monitor)
{
    CALL_STACK_MESSAGE1("CChangeMonitorThread::CChangeMonitorThread()");
    State = cmtIdle;
    Action = aeNoAction;
    ActionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    RegistryEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    IgnoreChanges = 0;
}

CChangeMonitorThread::~CChangeMonitorThread()
{
    CALL_STACK_MESSAGE1("CChangeMonitorThread::~CChangeMonitorThread()");
    CloseHandle(ActionEvent);
    CloseHandle(RegistryEvent);
}

unsigned
CChangeMonitorThread::Body()
{
    CALL_STACK_MESSAGE1("CChangeMonitorThread::Body()");
    DWORD timeout = INFINITE;
    HKEY hKey = NULL;
    BOOL loop = TRUE;
    while (loop)
    {
        HANDLE handles[2] = {ActionEvent, RegistryEvent};
        DWORD res = WaitForMultipleObjects(2, handles, FALSE, timeout);
        // let threads that haven't received any task for a longer time finish
        timeout = 1000;
        State = cmtIdle;
        Monitor.CS.Enter();
        ResetEvent(ActionEvent);
        switch (Action)
        {
        LSETPATH:
        case aeSetPath:
        {
            TRACE_I("chmon: aeSetPath, hKey = " << hKey);
            if (hKey)
            {
                RegCloseKey(hKey);
                hKey = NULL;
            }
            ResetEvent(RegistryEvent);
            LONG ret = RegOpenKeyExW(PredefinedHKeys[Root].HKey, Key, 0, KEY_NOTIFY, &hKey);
            if (ret == ERROR_SUCCESS)
            {
                ret = RegNotifyChangeKeyValue(hKey, FALSE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY,
                                              RegistryEvent, TRUE);
                if (ret != ERROR_SUCCESS)
                    TRACE_E("RegNotifyChangeKeyValue failed.");
                else
                {
                    State = cmtWaitingForChange;
                    timeout = INFINITE;
                }
            }
            else
                TRACE_E("Unable to open registry key for change notification");
            if (State != cmtWaitingForChange)
                ConnectedFS.DetachMembers();
            break;
        }

        case aeCancel:
            TRACE_I("chmon: aeCancel, hKey = " << hKey);
            if (hKey)
            {
                RegCloseKey(hKey);
                hKey = NULL;
            }
            ResetEvent(RegistryEvent);
            ConnectedFS.DetachMembers();
            break;

        LFINISH:
        case aeFinish:
        {
            TRACE_I("chmon: aeFinish, hKey = " << hKey);
            if (hKey)
                RegCloseKey(hKey);
            // disconnect the thread from the monitor
            int i;
            for (i = 0; i < Monitor.Threads.Count; i++)
                if (Monitor.Threads[i] == this)
                {
                    Monitor.Threads.Detach(i);
                    break;
                }
            loop = FALSE; // we're done
            break;
        }

        case aeNoAction:
        {
            TRACE_I("chmod: aeNoAction, hKey = " << hKey);
            if (res == WAIT_TIMEOUT)
                goto LFINISH; // we're done
            if (res - WAIT_OBJECT_0 == 1)
            {
                if (IgnoreChanges)
                {
                    TRACE_I("chmod: ignoring change " << IgnoreChanges);
                    IgnoreChanges--;
                    goto LSETPATH;
                }
                TRACE_I("chmod: posting refresh");
                int i;
                for (i = 0; i < ConnectedFS.Count; i++)
                    SG->PostRefreshPanelFS(ConnectedFS[i], ConnectedFS[i]->FocusFirstNewItem);
                ConnectedFS.DetachMembers();
            }
            break;
        }
        }
        Action = aeNoAction;
        Monitor.CS.Leave();
    }
    return 0;
}

// ****************************************************************************
//
// CChangeMonitor
//
//

CChangeMonitor::CChangeMonitor() : Threads(4, 4, dtNoDelete){
                                       CALL_STACK_MESSAGE_NONE}

                                   CChangeMonitor::~CChangeMonitor()
{
    CALL_STACK_MESSAGE_NONE
}

void CChangeMonitor::AddPath(int root, LPWSTR key, CPluginFSInterface* fs)
{
    CALL_STACK_MESSAGE2("CChangeMonitor::AddPath(%d, , )", root);
    CS.Enter();
    int idle = -1;
    // check whether this path is already monitored
    int i;
    for (i = 0; i < Threads.Count; i++)
    {
        if (Threads[i]->State == cmtIdle)
        {
            idle = i; // remember the last idle thread
            continue;
        }
        if (Threads[i]->Root == root &&
            CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                           Threads[i]->Key, -1,
                           key, -1) == CSTR_EQUAL)
            break;
    }
    // if it is not monitored, create a new monitor or use an idle one
    if (i == Threads.Count)
    {
        if (idle != -1)
            i = idle;
        else
        {
            CChangeMonitorThread* thread = new CChangeMonitorThread(*this);
            if (Threads.Add(thread) == ULONG_MAX)
            {
                delete thread;
                CS.Leave();
                return;
            }
            if (!thread->Create(ThreadQueue))
            {
                Threads.Detach(i);
                delete thread;
                TRACE_E("Failed to start a new change monitor thread");
                CS.Leave();
                return;
            }
        }
    }
    // add the FS to the list of monitored file systems
    Threads[i]->ConnectedFS.Add(fs);
    // if this is the first item, activate the monitoring thread
    if (Threads[i]->ConnectedFS.Count == 1)
    {
        Threads[i]->IgnoreChanges = 0;
        Threads[i]->Root = root;
        wcscpy(Threads[i]->Key, key);
        Threads[i]->Action = aeSetPath;
        SetEvent(Threads[i]->ActionEvent);
    }
    CS.Leave();
}

void CChangeMonitor::Cancel(CPluginFSInterface* fs)
{
    CALL_STACK_MESSAGE1("CChangeMonitor::Cancel()");
    CS.Enter();
    int idle = -1;
    // find all threads monitoring this FS
    int i;
    for (i = 0; i < Threads.Count; i++)
    {
        if (Threads[i]->State == cmtIdle)
            continue;
        int j;
        for (j = 0; j < Threads[i]->ConnectedFS.Count;)
        {
            if (Threads[i]->ConnectedFS[j] == fs)
            {
                Threads[i]->ConnectedFS.Detach(j);
                continue;
            }
            j++;
        }
        if (Threads[i]->ConnectedFS.Count == 0)
        {
            Threads[i]->State = cmtIdle;
            Threads[i]->Action = aeCancel;
            SetEvent(Threads[i]->ActionEvent);
        }
    }
    CS.Leave();
}

void CChangeMonitor::Stop()
{
    CALL_STACK_MESSAGE1("CChangeMonitor::Stop()");
    // gradually shut down all running threads
    while (1)
    {
        CS.Enter();

        // we have no threads left, so we're done
        if (Threads.Count == 0)
        {
            CS.Leave();
            return;
        }

        HANDLE thread = Threads[Threads.Count - 1]->Thread;
        Threads[Threads.Count - 1]->Action = aeFinish;
        SetEvent(Threads[Threads.Count - 1]->ActionEvent);

        CS.Leave();

        ThreadQueue.WaitForExit(thread, INFINITE);
    }
}

void CChangeMonitor::IgnoreNextRootChange(int root)
{
    CALL_STACK_MESSAGE2("CChangeMonitor::IgnoreNextRootChange(%d)", root);
    CS.Enter();
    // check whether this path is already monitored
    int i;
    for (i = 0; i < Threads.Count; i++)
    {
        if (Threads[i]->State == cmtIdle)
            continue;
        if (Threads[i]->Root == root && Threads[i]->Key[0] == L'\0')
            Threads[i]->IgnoreChanges = 1;
    }
    CS.Leave();
}
