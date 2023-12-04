// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// #include "../Format/Common/ArchiveInterface.h"
#include "7za/CPP/7zip/IPassword.h"
#include "7za/CPP/Common/MyCom.h"
#include "7za/CPP/Common/MyString.h"
#include "7za/CPP/7zip/Archive/IArchive.h"

//#include "../Common/UpdatePair.h"
//#include "../Common/UpdateProduce.h"
//#include "PercentPrinter.h"

#include "structs.h"

#pragma warning(disable : 4786) // identifier was truncated to '255' characters in the debug information

struct CUpdateItem
{
    UINT32 Attributes;
    FILETIME CreationTime;
    FILETIME LastWriteTime;
    FILETIME LastAccessTime;

    UINT64 Size;
    UString Name;

    bool IsAnti;
    bool IsDirectory;
};

/*
class CUpdateCallbackImp: 
  public IArchiveUpdateCallback,
  public ICryptoGetTextPassword2,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1(ICryptoGetTextPassword2)

  // IProgress
  STDMETHOD(SetTotal)(UINT64 size);
  STDMETHOD(SetCompleted)(const UINT64 *completeValue);

  // IUpdateCallback
  STDMETHOD(EnumProperties)(IEnumSTATPROPSTG **enumerator);  
  STDMETHOD(GetUpdateItemInfo)(UINT32 index, INT32 *newData, INT32 *newProperties, UINT32 *indexInArchive);

  STDMETHOD(GetProperty)(UINT32 index, PROPID propID, PROPVARIANT *value);

  STDMETHOD(GetStream)(UINT32 index, IInStream **inStream);
  
  STDMETHOD(SetOperationResult)(INT32 operationResult);

  STDMETHOD(CryptoGetTextPassword2)(INT32 *passwordIsDefined, BSTR *password);

private:
//  UString SourcePath;
  AString ProcessedFileName;

  // synchronizace pro volani
  CRITICAL_SECTION CSUpdate;

  TIndirectArray<CFileItem> *FileItems;
  TIndirectArray<CArchiveItem> *ArchiveItems;
  TIndirectArray<CUpdateInfo> *UpdateList;

  CQuadWord Total;
  CQuadWord Completed;

  bool PasswordIsDefined;
  UString Password;
  bool AskPassword;

public:
  CUpdateCallbackImp();
  ~CUpdateCallbackImp();

  AString GetProcessedFile() const { return ProcessedFileName; }

  void Init(TIndirectArray<CFileItem> *fileList, TIndirectArray<CArchiveItem> *archiveItems, TIndirectArray<CUpdateInfo> *updateList,
            bool passwordIsDefined, UString password);

  CQuadWord &GetTotalSize() { return Total; }
  CQuadWord &GetCompletedSize() { return Completed; }
};
*/
/*
struct IUpdateCallbackUI
{
  virtual HRESULT SetTotal(UInt64 size) = 0;
  virtual HRESULT SetCompleted(const UInt64 *completeValue) = 0;
  virtual HRESULT CheckBreak() = 0;
  virtual HRESULT Finilize() = 0;
  virtual HRESULT GetStream(const wchar_t *name, bool isAnti) = 0;
  virtual HRESULT OpenFileError(const wchar_t *name, DWORD systemError) = 0;
  virtual HRESULT SetOperationResult(Int32 operationResult) = 0;
  virtual HRESULT CryptoGetTextPassword2(Int32 *passwordIsDefined, BSTR *password) = 0;
  virtual HRESULT CloseProgress() { return S_OK; };
};
*/
class CArchiveUpdateCallback : public IArchiveUpdateCallback2,
                               public ICryptoGetTextPassword2,
                               public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP2(IArchiveUpdateCallback2,
                    ICryptoGetTextPassword2)

    // IProgress
    STDMETHOD(SetTotal)
    (UInt64 size);
    STDMETHOD(SetCompleted)
    (const UInt64* completeValue);

    // IUpdateCallback
    STDMETHOD(EnumProperties)
    (IEnumSTATPROPSTG** enumerator);
    STDMETHOD(GetUpdateItemInfo)
    (UInt32 index,
     Int32* newData, Int32* newProperties, UInt32* indexInArchive);
    STDMETHOD(GetProperty)
    (UInt32 index, PROPID propID, PROPVARIANT* value);
    STDMETHOD(GetStream)
    (UInt32 index, ISequentialInStream** inStream);
    STDMETHOD(SetOperationResult)
    (Int32 operationResult);

    STDMETHOD(GetVolumeSize)
    (UInt32 index, UInt64* size);
    STDMETHOD(GetVolumeStream)
    (UInt32 index, ISequentialOutStream** volumeStream);

    // ICryptoGetTextPassword2
    STDMETHOD(CryptoGetTextPassword2)
    (Int32* passwordIsDefined, BSTR* password);

    AString GetProcessedFile() const { return ProcessedFileName; }
    CQuadWord& GetTotalSize() { return Total; }
    CQuadWord& GetCompletedSize() { return Completed; }

    CArchiveUpdateCallback(HWND _hProgWnd);
    ~CArchiveUpdateCallback();

public:
    AString ProcessedFileName;

    // synchronizace pro volani
    CRITICAL_SECTION CSUpdate;

    TIndirectArray<CFileItem>* FileItems;
    TIndirectArray<CArchiveItem>* ArchiveItems;
    TIndirectArray<CUpdateInfo>* UpdateList;

    CQuadWord Total;
    CQuadWord Completed;

    bool PasswordIsDefined;
    UString Password;
    bool AskPassword;

    CRecordVector<UInt64> VolumesSizes;
    //  UString VolName;
    //  UString VolExt;

    BOOL Silent;

private:
    HWND hProgWnd;

    /*
  CRecordVector<UInt64> VolumesSizes;
  UString VolName;
  UString VolExt;

  IUpdateCallbackUI *Callback;

  UString DirPrefix;
  bool StdInMode;
  const CObjectVector<CDirItem> *DirItems;
  const CObjectVector<CArchiveItem> *ArchiveItems;
  const CObjectVector<CUpdatePair2> *UpdatePairs;
  CMyComPtr<IInArchive> Archive;
*/
};
