// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "7za/CPP/7zip/Common/FileStreams.h"

class CRetryableOutFileStream : public COutFileStream
{
public:
    CRetryableOutFileStream(HWND hParentWnd);

    virtual ~CRetryableOutFileStream() {}

    STDMETHOD(Write)
    (const void* data, UInt32 size, UInt32* processedSize);

private:
    HWND hParentWnd;
};

class CRetryableInFileStream : public CInFileStream
{
public:
    CRetryableInFileStream(HWND hParentWnd);

    virtual ~CRetryableInFileStream() {}

    STDMETHOD(Read)
    (void* data, UInt32 size, UInt32* processedSize);

private:
    HWND hParentWnd;
};
