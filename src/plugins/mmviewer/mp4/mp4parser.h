// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _MP4_SUPPORT_

#include "..\parser.h"

//****************************************************************************
//
// CParserMP4
//

class CParserMP4 : public CParserInterface
{
private:
    FILE* f;

public:
    CParserMP4() : f(NULL){};
    ~CParserMP4() { CloseFile(); }

    virtual CParserResultEnum OpenFile(const char* fileName);
    virtual CParserResultEnum CloseFile();
    virtual CParserResultEnum GetFileInfo(COutputInterface* output);
};

#endif
