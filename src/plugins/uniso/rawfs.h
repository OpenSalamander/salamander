// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "fs.h"

// ****************************************************************************
//
// CRawFS
//
// Tento filesystem pro vybalovani tzv. raw tracku. Primarne je pouzit pro vybalovani
// data tracku z video a supervideo CD.
// Metoda ListDirectory vraci TRUE, protoze na video a supervideo CD jsou adresare
// nahrany v jinem tracku.
// Vykonna je pouze metoda UnpackFile.
//

class CRawFS : public CUnISOFSAbstract
{
protected:
    DWORD ExtentOffset;

public:
    CRawFS(CISOImage* image, DWORD extent);
    virtual ~CRawFS();
    // metody

    virtual BOOL Open(BOOL quiet);
    virtual BOOL DumpInfo(FILE* outStream);
    virtual BOOL ListDirectory(char* path, int session,
                               CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);
    virtual int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                           const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip);

protected:
    BOOL ReadBlockPhys(DWORD lbNum, size_t blocks, unsigned char* data);
};
