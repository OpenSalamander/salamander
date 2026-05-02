// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "PVMessage.h"

PVMessage::PVMessage() : pData(NULL), hFileMap(NULL), pWImg(NULL)
{
}

PVMessage::~PVMessage()
{
    // NOTE: ownership of pData not taken when hFileMap is NULL
    if (hFileMap)
    {
        if (pData)
            UnmapViewOfFile(pData);
        CloseHandle(hFileMap);
    }
}

LPBYTE PVMessage::GetData()
{
    return (LPBYTE)pData;
}

PVCODE PVMessage::GetResultCode()
{
    if (pData)
    {
        return ((LPPVMessageHeader)pData)->ResultCode;
    }
    return PVC_ENVELOPE_NOT_LOADED;
}
