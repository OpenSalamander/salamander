// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _OGG_SUPPORT_

#include "..\parser.h"

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

//****************************************************************************
//
// CParserOGG
//

class CParserOGG : public CParserInterface
{
private:
    FILE* f;
    OggVorbis_File vf;

public:
    CParserOGG() : f(NULL) {}
    ~CParserOGG() { CloseFile(); }

    virtual CParserResultEnum OpenFile(const char* fileName);
    virtual CParserResultEnum CloseFile();
    virtual CParserResultEnum GetFileInfo(COutputInterface* output);
};

#endif
