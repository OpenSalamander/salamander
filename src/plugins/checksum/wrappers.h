// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CHashAlgo
{
public:
    CHashAlgo(){};
    virtual ~CHashAlgo(){};

    virtual bool IsOK() = 0; // Was constructed successfully?
    virtual bool Init() = 0; // Init for a new file. true on success
    virtual bool Update(const char* buf, DWORD size) = 0;
    virtual bool Finalize() = 0;
    virtual int GetDigest(char* buf, DWORD bufsize) = 0; // Returns # of copied binary bytes
    virtual bool ParseDigest(char* buf, char* fileName, int fileNameLen, char* digest) = 0;
};

typedef CHashAlgo* (*THashFactory)();

CHashAlgo* CRCFactory();
CHashAlgo* MD5Factory();
CHashAlgo* SHA1Factory();
CHashAlgo* SHA256Factory();
CHashAlgo* SHA512Factory();
