// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ************************************************************************************************
//
// C7zClient
//
// 7za.dll library sandbox
//

#include "7za/CPP/Common/StringConvert.h"
#include "7za/CPP/7zip/Common/FileStreams.h"
#include "7za/CPP/7zip/Archive/IArchive.h"
#include "7za/CPP/Windows/PropVariant.h"
#include "7za/CPP/Windows/PropVariantConv.h"
#include "7za/CPP/Windows/DLL.h"
#include "7za/CPP/Windows/Defs.h"

#include "extract.h"
#include "update.h"

#include "structs.h"

#ifndef FILE_ATTRIBUTE_UNIX_EXTENSION
// Transfered from p7zip (portable 7zip)
#define FILE_ATTRIBUTE_UNIX_EXTENSION 0x8000 /* trick for Unix */
#endif

#ifndef S_ISLNK
// Transfered from sys/stat.h on Mac (and other Unix systems)
#define S_ISLNK(m) (((m) & 0170000) == 0120000) /* symbolic link */
#endif

// three states for errors.
// there are situations where TRUE/FALSE is not enough. we have an activity that can run into an error. sometimes we can
// and want to continue the activity, other times we cannot. if we can continue we return OPER_CONTINUE,
// if it cannot go on (out of memory, etc.) -> OPER_CANCEL. if everything is fine -> OPER_OK
#define OPER_OK 0
#define OPER_CONTINUE 1
#define OPER_CANCEL 2

#define E_STOPEXTRACTION (HRESULT)0x8000FEDC

#define MAX_PATH_LEN 1024

typedef UINT32(WINAPI* TCreateObjectFunc)(const GUID* clsID, const GUID* interfaceID, void** outObject);

// used to pass the items that will be extracted
struct CArchiveItemInfo
{
    CSysString NameInArchive; // in the archive (i.e. including the path)
    const CFileData* FileData;
    bool IsDir;

    CArchiveItemInfo(CSysString name, const CFileData* fd, bool isDir)
    {
        NameInArchive = name;
        FileData = fd;
        IsDir = isDir;
    }
};

// ************************************************************************************************
//
// C7zClient
//

class C7zClient : public NWindows::NDLL::CLibrary
{
public:
    struct CItemData
    {
        UINT32 Idx;
        BOOL Encrypted;
        UINT64 PackedSize;
        char* Method;

        CItemData();
        ~CItemData();
        void SetMethod(const char* method);
    };

protected:
    BOOL CreateObject(const GUID* interfaceID, void** object);

public:
    C7zClient();
    ~C7zClient();

    BOOL ListArchive(const char* fileName, CSalamanderDirectoryAbstract* dir, CPluginDataInterface*& pluginData, UString& password);

    int Decompress(CSalamanderForOperationsAbstract* salamander, const char* archiveName, const char* outDir,
                   TIndirectArray<CArchiveItemInfo>* itemList, UString& password, BOOL silentDelete = FALSE);

    int TestArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName);

    int Update(CSalamanderForOperationsAbstract* salamander, const char* archiveName, const char* srcPath, BOOL isNewArchive,
               TIndirectArray<CFileItem>* fileList, CCompressParams* compressParams, bool passwordIsDefined, UString password);

    int Delete(CSalamanderForOperationsAbstract* salamander, const char* archiveName,
               TIndirectArray<CArchiveItemInfo>* archiveList, bool passwordIsDefined, UString& password);

protected:
    BOOL OpenArchive(const char* fileName, IInArchive** archive, UString& password, BOOL quiet = FALSE);

    BOOL FillItemData(IInArchive* archive, UINT32 index, C7zClient::CItemData* itemData);
    BOOL AddFileDir(IInArchive* archive, UINT32 idx,
                    CSalamanderDirectoryAbstract* dir, CPluginDataInterface*& pluginData,
                    BOOL* reportTooLongPathErr, const char* archiveName);

    int GetArchiveItemList(IInArchive* archive, TIndirectArray<CArchiveItem>** archiveItems, UINT32* numItems);

    int UpdateMakeUpdateList(TIndirectArray<CFileItem>* fileList, TIndirectArray<CArchiveItem>* archiveItems,
                             TIndirectArray<CUpdateInfo>* updateList);
    int DeleteMakeUpdateList(TIndirectArray<CArchiveItem>* archiveItems, TIndirectArray<CArchiveItemInfo>* deleteList,
                             TIndirectArray<CUpdateInfo>* updateList);

    HRESULT SetCompressionParams(IOutArchive* outArchive, CCompressParams* compressParams);
};
