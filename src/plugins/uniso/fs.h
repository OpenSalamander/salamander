// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CISOImage;

class CUnISOFSAbstract
{
protected:
    CISOImage* Image;

public:
    CUnISOFSAbstract(CISOImage* image) { Image = image; }
    virtual ~CUnISOFSAbstract() {}

    virtual BOOL Open(BOOL quiet) = 0;
    virtual BOOL DumpInfo(FILE* outStream) = 0;
    virtual BOOL ListDirectory(char* path, int session,
                               CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData) = 0;
    virtual int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                           const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip) = 0;
};
