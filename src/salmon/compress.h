// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// structure passed to the compression thread, used to transfer input/output parameters
struct CCompressParams
{
    BOOL Result;                     // TRUE if the operation completed successfully, otherwise FALSE
    char ErrorMessage[2 * MAX_PATH]; // if Result is FALSE, contains the error description
};

BOOL StartCompressThread(CCompressParams* params);
BOOL IsCompressThreadRunning();