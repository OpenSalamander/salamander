// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MAILSLOT_NAME "\\\\.\\mailslot\\openedit_mailslot"
#define CHECK_MAILSLOT_EVENT_NAME "openedit_check_mailslot"

struct COpenEditPacket
{
    char File[MAX_PATH];
    int Line;
    int Column;
};
