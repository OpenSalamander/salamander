// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// CRemoteComparator
//

class CRemoteComparator : public CThread, public CMessageListener
{
public:
    CRemoteComparator();
    ~CRemoteComparator();
    static void CreateRemoteComparator();
    static BOOL Terminate(BOOL force);

protected:
    CMessageCenter MessageCenter;
    static BOOL Created;
    static HANDLE RemoteComparatorThread; // POZOR: handle threadu jen pro pouziti v ThreadQueue
    static HANDLE TerminateEvent;
    virtual unsigned Body();
    virtual void RecieveMessage(const CMessage* message);
};
