// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// structure passed to the upload thread, used to transfer input/output parameters
struct CUploadParams
{
    BOOL Result;                     // TRUE if the operation completed successfully, otherwise FALSE
    char ErrorMessage[2 * MAX_PATH]; // if Result is FALSE, contains the error description
    char FileName[MAX_PATH];         // full path to the file that should be uploaded
};

BOOL StartUploadThread(CUploadParams* params);
BOOL IsUploadThreadRunning();