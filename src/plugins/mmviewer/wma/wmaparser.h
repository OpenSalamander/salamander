// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WMA_SUPPORT_

#include "..\parser.h"

#include "wmsdk\\wmsdk.h"

//****************************************************************************
//
// CParserWMA
//

class CParserWMA : public CParserInterface
{
private:
    IWMMetadataEditor* pEditor;
    IWMHeaderInfo* pHeaderInfo;

    HMODULE module;

protected:
    HRESULT GetHeaderAttribute(IWMHeaderInfo* pHdrInfo, LPCWSTR pwszName, BOOL* pbIsPresent, char** output);

public:
    CParserWMA();
    ~CParserWMA()
    {
        CloseFile();
        if (module)
            FreeLibrary(module);
    }

    virtual CParserResultEnum OpenFile(LPCTSTR fileName);
    virtual CParserResultEnum CloseFile();
    virtual CParserResultEnum GetFileInfo(COutputInterface* output);
};

#endif
