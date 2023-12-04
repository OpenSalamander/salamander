// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ************************************************************************************************
//
// C7zClient
//
// Sandbox knihovny 7za.dll
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
#define S_ISLNK(m) (((m)&0170000) == 0120000) /* symbolic link */
#endif

// tri stavy pro chyby.
// jsou situace, kdy nestaci TRUE/FALSE. mame cinnost, pri ktere muze nastat chyba. nekdy muzeme
// a chceme v cinnosti pokracovat, jindy to nejde. pokud muzeme pokracovat vracime OPER_CONTINUE,
// pokud to dal nejde (dosla pamet, aj.) -> OPER_CANCEL. pokud vse ok -> OPER_OK
#define OPER_OK 0
#define OPER_CONTINUE 1
#define OPER_CANCEL 2

#define E_STOPEXTRACTION (HRESULT)0x8000FEDC

#define MAX_PATH_LEN 1024

typedef UINT32(WINAPI* TCreateObjectFunc)(const GUID* clsID, const GUID* interfaceID, void** outObject);

// slouzi k predani polozek, ktere se budou vybalovat
struct CArchiveItemInfo
{
    CSysString NameInArchive; // v archivu (tedy i s cestou)
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
