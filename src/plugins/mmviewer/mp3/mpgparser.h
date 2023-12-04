// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _MPG_SUPPORT_

#include "..\parser.h"

//****************************************************************************
//
// CParserMPG
//

class CParserMPG : public CParserInterface
{
private:
    FILE* f;

public:
    CParserMPG() : f(NULL){};
    ~CParserMPG() { CloseFile(); }

    virtual CParserResultEnum OpenFile(const char* fileName);
    virtual CParserResultEnum CloseFile();
    virtual CParserResultEnum GetFileInfo(COutputInterface* output);
};

#endif
