// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "fs.h"

// ****************************************************************************
//
// CAudio
//

class CISOImage;

class CAudio : public CUnISOFSAbstract
{
public:
    CAudio(CISOImage* image, int track);
    virtual ~CAudio();
    // metody

    virtual BOOL Open(BOOL quiet);
    virtual BOOL DumpInfo(FILE* outStream);
    virtual BOOL ListDirectory(char* path, int session,
                               CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);

    virtual int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                           const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip);

protected:
    int Track;

    BOOL AddFileDir(const char* path, char* fileName,
                    CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);
};

struct CISOImage::Track;

struct CAudioTrack : CISOImage::Track
{
public:
    CAudioTrack() { Label = NULL; };
    virtual ~CAudioTrack();
    virtual const char* GetLabel();
    virtual void SetLabel(const char* label);

private:
    char* Label;
};
