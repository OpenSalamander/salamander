// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _MOD_SUPPORT_

#include "..\parser.h"

//****************************************************************************
//
// CParserMOD
//

class CParserMOD : public CParserInterface
{
private:
    char modulename[128];
    WORD patterns, samples, instruments;
    FILE* f;

protected:
    BOOL ReadImpulseTrackerModule();
    BOOL ReadScreamTrackerModule();
    BOOL ReadExtendedModule();
    BOOL ReadMultiTrackerModule();
    BOOL ReadProTrackerModule();
    BOOL Read669Module();

public:
    CParserMOD() : f(NULL) {}
    ~CParserMOD() { CloseFile(); }

    virtual CParserResultEnum OpenFile(const char* fileName);
    virtual CParserResultEnum CloseFile();
    virtual CParserResultEnum GetFileInfo(COutputInterface* output);
};

#endif
