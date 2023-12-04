// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CRemoteComparator
//

BOOL CRemoteComparator::Created = FALSE;
HANDLE CRemoteComparator::RemoteComparatorThread = NULL;
HANDLE CRemoteComparator::TerminateEvent = NULL;

CRemoteComparator::CRemoteComparator()
    : CThread("Remote Comparator"),
      MessageCenter(MessageCenterName, FALSE){
          CALL_STACK_MESSAGE_NONE}

      CRemoteComparator::~CRemoteComparator()
{
    CALL_STACK_MESSAGE_NONE
}

void CRemoteComparator::CreateRemoteComparator()
{
    CALL_STACK_MESSAGE1("CRemoteComparator::CreateRemoteComparator()");
    if (Created)
        return;

    TerminateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (TerminateEvent == NULL)
        return;

    CRemoteComparator* rc = new CRemoteComparator;
    if (!rc)
    {
        Error((HWND)-1, IDS_LOWMEM);
        CloseHandle(TerminateEvent);
        TerminateEvent = NULL;
        return;
    }
    // TODO nastavit mensi zasobnik
    RemoteComparatorThread = rc->Create(ThreadQueue);
    if (!RemoteComparatorThread)
    {
        delete rc;
        CloseHandle(TerminateEvent);
        TerminateEvent = NULL;
        return;
    }
    Created = TRUE;
}

BOOL CRemoteComparator::Terminate(BOOL force)
{
    CALL_STACK_MESSAGE2("CRemoteComparator::Terminate(%d)", force);
    if (!Created)
        return TRUE;

    SetEvent(TerminateEvent);
    if (!ThreadQueue.WaitForExit(RemoteComparatorThread, force ? 200 : 1000))
    {
        if (!force)
        {
            ResetEvent(TerminateEvent);
            TRACE_E("Remote comparator thread has not finished, we will refuse to unload plugin.");
            return FALSE;
        }
        TRACE_E("Remote comparator thread has not finished, we will terminate it.");
        ThreadQueue.KillThread(RemoteComparatorThread, 666);
    }
    RemoteComparatorThread = NULL;
    CloseHandle(TerminateEvent);
    TerminateEvent = NULL;
    Created = FALSE;
    return TRUE;
}

unsigned
CRemoteComparator::Body()
{
    CALL_STACK_MESSAGE1("CRemoteComparator::Body()");
    if (!MessageCenter.IsGood())
        return -1;
    BOOL ret;
    HANDLE started = CreateEvent(NULL, TRUE, FALSE, StartedEventName);
    SetEvent(started);
    while (1)
    {
        BOOL success;
        BOOL canceled;
        ret = MessageCenter.WaitForMessage(success, INFINITE, TerminateEvent, canceled);
        if (!ret)
            break;
        if (canceled)
            break;
        if (success)
            MessageCenter.RecieveMessages(this);
    }
    ResetEvent(started);
    CloseHandle(started);
    return ret ? 0 : -1;
}

void CRemoteComparator::RecieveMessage(const CMessage* message)
{
    CALL_STACK_MESSAGE1("CRemoteComparator::RecieveMessage()");
    if (message->Size != sizeof(CRCMessage))
        return;
    CRCMessage* msg = (CRCMessage*)message;

    char Path1[MAX_PATH];
    char Path2[MAX_PATH];
    strcpy(Path1, msg->Path1);
    strcpy(Path2, msg->Path2);

    int errID;
    SalGetFullName(Path1, &errID, msg->CurrentDirectory);
    SalGetFullName(Path2, &errID, msg->CurrentDirectory);

    BOOL ok = FALSE;
    CFilecompThread* d = new CFilecompThread(Path1, Path2, TRUE, msg->ReleaseEvent);
    if (!d)
    {
        Error((HWND)NULL, IDS_LOWMEM);
    }
    else
    {
        ok = d->Create(ThreadQueue) != NULL;
        if (!ok)
            delete d;
    }

    if (!ok && *msg->ReleaseEvent)
    {
        // pustime dal filecomp.exe
        HANDLE event = OpenEvent(EVENT_MODIFY_STATE, FALSE, msg->ReleaseEvent);
        SetEvent(event);
        CloseHandle(event);
    }
}
