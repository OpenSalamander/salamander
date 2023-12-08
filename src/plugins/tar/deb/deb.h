// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "tar.h"

#pragma pack(push, 1)

#define DEB_AR_MAGIC 0x0A60

struct SARBlock
{
    char FileName[16];
    char TimeStamp[12]; // Decimal
    char OwnerID[6];    // Decimal
    char GroupID[6];    // Decimal
    char FileMode[8];   // Octal
    char FileSize[10];  // Decimal
    short FileMagic;    // DEB_AR_MAGIC
};

#pragma pack(pop)

class CDEBArchive : public CArchiveAbstract
{
private:
    BOOL bOK;
    CArchive* controlArchive;
    CArchive* dataArchive;

public:
    CDEBArchive(LPCTSTR fileName, CSalamanderForOperationsAbstract* salamander);
    ~CDEBArchive(void);

    BOOL ListArchive(const char* prefix, CSalamanderDirectoryAbstract* dir);
    BOOL UnpackOneFile(const char* nameInArchive, const CFileData* fileData,
                       const char* targetPath, const char* newFileName);
    BOOL UnpackArchive(const char* targetPath, const char* archiveRoot,
                       SalEnumSelection nextName, void* param);
    BOOL UnpackWholeArchive(const char* mask, const char* targetPath);

    BOOL IsOk() { return bOK; };

  private:
    BOOL AssignArchive(const char *archName, CArchive *archive);
};
