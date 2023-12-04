// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _VQF_SUPPORT_

#include "..\parser.h"

//****************************************************************************
//
// CParserVQF
//
#define VQFTAGS_ADD 12
#define VQFTAGS (8 + VQFTAGS_ADD)

class CParserVQF : public CParserInterface
{
private:
    FILE* f;

public:
    CParserVQF() : f(NULL){};
    ~CParserVQF() { CloseFile(); }

    virtual CParserResultEnum OpenFile(const char* fileName);
    virtual CParserResultEnum CloseFile();
    virtual CParserResultEnum GetFileInfo(COutputInterface* output);
};

#endif
