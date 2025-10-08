// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// structure passed to the minidump thread, used to transfer input/output parameters
struct CMinidumpParams
{
    BOOL Result;                     // TRUE if the operation completed successfully, otherwise FALSE
    char ErrorMessage[2 * MAX_PATH]; // if Result is FALSE, contains the error description
};

BOOL StartMinidumpThread(CMinidumpParams* params);
BOOL IsMinidumpThreadRunning();