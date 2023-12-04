// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "fs.h"
#include "iso9660.h"
#include "udf.h"
#include "hfs.h"

// ****************************************************************************
//
// CUDFISO
//

class CUDFISO : public CUnISOFSAbstract
{
public:
    DWORD ExtentOffset;

protected:
    CISO9660* ISO;
    CUDF* UDF;
    CHFS* HFS;

public:
    CUDFISO(CISOImage* image, DWORD extent);
    virtual ~CUDFISO();
    // metody

    virtual BOOL Open(BOOL quiet);
    virtual BOOL DumpInfo(FILE* outStream);
    virtual BOOL ListDirectory(char* path, int session,
                               CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);
    virtual int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                           const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip);
};
