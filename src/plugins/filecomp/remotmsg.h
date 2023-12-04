// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MessageCenterName "RemoteComparator"
#define StartedEventName "RemoteComparatorStarted"

struct CRCMessage : public CMessage
{
    char CurrentDirectory[MAX_PATH];
    char Path1[MAX_PATH];
    char Path2[MAX_PATH];
    char ReleaseEvent[20];
};
