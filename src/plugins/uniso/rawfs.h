// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "fs.h"

// ****************************************************************************
//
// CRawFS
//
// This file system unpacks so-called raw tracks. It is primarily used for extracting
// data tracks from video and super video CDs.
// The ListDirectory method returns TRUE because directories on video and super video CDs are
// stored in a different track.
// Only the UnpackFile method is functional.
//

class CRawFS : public CUnISOFSAbstract
{
protected:
    DWORD ExtentOffset;

public:
    CRawFS(CISOImage* image, DWORD extent);
    virtual ~CRawFS();
    // methods

    virtual BOOL Open(BOOL quiet);
    virtual BOOL DumpInfo(FILE* outStream);
    virtual BOOL ListDirectory(char* path, int session,
                               CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);
    virtual int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                           const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip);

protected:
    BOOL ReadBlockPhys(DWORD lbNum, size_t blocks, unsigned char* data);
};
