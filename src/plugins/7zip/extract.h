// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>

#include "7za/CPP/Common/MyCom.h"
#include "7za/CPP/Common/MyString.h"

#include "7za/CPP/7zip/IPassword.h"

//#include "7za/Common/String.h"
//#include "7za/Common/StdOutStream.h"

#include "7za/CPP/7zip/Common/FileStreams.h"

//#include "7za/7zip/IPassword.h"

#include "7za/CPP/7zip/Archive/IArchive.h"
//#include "../Common/ZipRegistry.h"
//#include "include/7zip/Archive/IFolder.h"

#include "structs.h"
#include "FStreams.h"

struct CArchiveItemInfo;

typedef std::map<UINT32, CArchiveItemInfo*> ItemsToExtractMap;

class CExtractCallbackImp : public IArchiveExtractCallback,
                            //  public IFolderArchiveExtractCallback,
                            public ICryptoGetTextPassword,
                            public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP1(ICryptoGetTextPassword)

    // IProgress
    STDMETHOD(SetTotal)
    (UINT64 size);
    STDMETHOD(SetCompleted)
    (const UINT64* completeValue);

    // IExtractCallback200
    STDMETHOD(GetStream)
    (UINT32 index, ISequentialOutStream** outStream, INT32 askExtractMode);
    STDMETHOD(PrepareOperation)
    (INT32 askExtractMode);
    STDMETHOD(SetOperationResult)
    (INT32 resultEOperationResult);

    // ICryptoGetTextPassword
    STDMETHOD(CryptoGetTextPassword)
    (BSTR* password);

private:
    CQuadWord Total;
    CQuadWord Completed;

    CMyComPtr<IInArchive> ArchiveHandler;
    ItemsToExtractMap& ItemsToExtract;

    const char* TargetDir;
    char TargetFileName[MAX_PATH];

    bool ExtractMode;
    struct CProcessedFileInfo
    {
        FILETIME LastWrite;
        bool IsDirectory;
        bool AttributesAreDefined;
        UINT32 Attributes;
        UINT64 Size;
        UString FileName;
        CSysString Name;
    } ProcessedFileInfo;

    CRetryableOutFileStream* OutFileStreamSpec;
    CMyComPtr<ISequentialOutStream> OutFileStream;

    FILETIME UTCLastWriteTimeDefault;
    DWORD AttributesDefault;

    bool PasswordIsDefined;
    UString& Password;
    //  bool Silent;  // Silent is for skip, which is not implemented in 7za.dll

    // synchronization for calls
    CRITICAL_SECTION CSExtract;

    enum EOperationMode
    {
        Ask,
        Keep,
        Delete,
        Overwrite,
        Skip,
        Cancel
    };
    // behavior of the callback when a data error occurs
    BOOL DataErrorSilent; // whether to ask about Keep or Delete
    EOperationMode DataErrorMode;
    BOOL DataErrorDeleteSilent; // report an error when deleting
    BOOL SilentDelete;

    //  EOperationMode OverwriteMode;
    //  BOOL OverwriteSilent;   // whether to ask about Overwrite or Skip
    DWORD OverwriteSilent;
    BOOL OverwriteSkip;
    BOOL OverwriteCancel;

    BOOL WholeFile;

    HWND hProgWnd;

public:
    CExtractCallbackImp(HWND _hProgWnd, UString& password, ItemsToExtractMap& itemsToExtract);
    ~CExtractCallbackImp();

    BOOL Init(IInArchive* archive, const char* outDir,
              const FILETIME& utcLastWriteTimeDefault, DWORD attributesDefault,
              BOOL silentDelete = FALSE);
    BOOL InitTest();

    int NumErrors;

    const char* GetFileName() { return TargetFileName; }
    FILETIME GetLastWrite() { return ProcessedFileInfo.LastWrite; }
    DWORD GetAttr() { return ProcessedFileInfo.Attributes; }
    UINT64 GetSize() { return ProcessedFileInfo.Size; }
    const char* GetName() { return ProcessedFileInfo.Name; }

    BOOL* GetOverwriteSkip() { return &OverwriteSkip; }
    DWORD* GetOverwriteSilent() { return &OverwriteSilent; }

    void SetOverwriteCancel(BOOL state = TRUE) { OverwriteCancel = state; }

    void Cleanup();

    CQuadWord& GetTotalSize() { return Total; }
    CQuadWord& GetCompletedSize() { return Completed; }

private:
    BOOL OnDataError();
    LRESULT Error(int resID, ...);
};
