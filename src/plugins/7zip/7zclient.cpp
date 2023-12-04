// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <assert.h>
#include "dbg.h"

#include "Common/MyInitGuid.h"

#include "7zip.h"
#include "7zclient.h"
#include "open.h"
#include "7zthreads.h"
#include "FStreams.h"

#include "7zip.rh"
#include "7zip.rh2"
#include "lang\lang.rh"

// ****************************************************************************
//
// C7zClient
//

#ifndef _UNICODE
bool g_IsNT = false;
#endif

HINSTANCE g_hInstance;

// {23170F69-40C1-278A-1000-000110070000}

//DEFINE_GUID(CLSID_CFormat7z,
//  0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x07, 0x00, 0x00);
const CLSID CLSID_CFormat7z = {0x23170F69, 0x40C1, 0x278A, {0x10, 0x00, 0x00, 0x01, 0x10, 0x07, 0x00, 0x00}};
//const CLSID CLSID_CFormatZIP = {0x23170F69, 0x40C1, 0x278A, {0x10, 0x00, 0x00, 0x01, 0x10, 0x01, 0x00, 0x00}};

/*
char *C7zClient::CItemData::GetMethodStr() {
  static char name[64];

  switch (Method) {
    case mUnknown: strcpy(name, "Unknown"); break;
    case mNone:    strcpy(name, "None"); break;
    case mCopy:    strcpy(name, "Copy"); break;
    case mLZMA:    strcpy(name, "LZMA"); break;
    case mBCJ:     strcpy(name, "BJC"); break;
    case mBCJ2:    strcpy(name, "BJC2"); break;
    case mPPMD:    strcpy(name, "PPMD"); break;
    case mDeflate: strcpy(name, "Deflete"); break;
    case mBZip2:   strcpy(name, "BZip2"); break;
    default:       strcpy(name, "Unknown"); break;
  }

  return name;
}
*/

C7zClient::CItemData::CItemData()
{
    Method = NULL;
}

C7zClient::CItemData::~CItemData()
{
    delete[] Method;
}

void C7zClient::CItemData::SetMethod(const char* method)
{
    delete[] Method;
    Method = new char[strlen(method) + 1];
    if (Method == NULL)
        return;
    strcpy(Method, method);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

C7zClient::C7zClient()
{
}

C7zClient::~C7zClient()
{
}

BOOL C7zClient::CreateObject(const GUID* interfaceID, void** object)
{
    TCHAR dllPath[MAX_PATH];
    if (!GetModuleFileName(DLLInstance, dllPath, MAX_PATH))
        return FALSE;
    lstrcpy(_tcsrchr(dllPath, '\\') + 1, _T("7za.dll"));

    if (!Load(dllPath))
        return Error(IDS_CANT_LOAD_LIBRARY);

    TCreateObjectFunc createObjectFunc = (TCreateObjectFunc)GetProc("CreateObject");
    if (createObjectFunc == 0)
        return Error(IDS_CANT_GET_CRATEOBJECT);

    if (createObjectFunc(&CLSID_CFormat7z, interfaceID, object) != S_OK)
    {
        Free();
        return Error(IDS_CANT_GET_CLASS_OBJECT);
    }

    return TRUE;
}

BOOL C7zClient::OpenArchive(const char* fileName, IInArchive** archive, UString& password, BOOL quiet /* = FALSE*/)
{
    CMyComPtr<IInArchive> a;
    if (!CreateObject(&IID_IInArchive, (void**)&a))
        return FALSE;

    CRetryableInFileStream* fileSpec = new CRetryableInFileStream(NULL);
    CMyComPtr<IInStream> file = fileSpec;

    if (!fileSpec->Open(fileName))
        return Error(IDS_CANT_OPEN_ARCHIVE, quiet, fileName);

    CArchiveOpenCallbackImp* openCallbackSpec = new CArchiveOpenCallbackImp(password);
    CMyComPtr<IArchiveOpenCallback> openCallback(openCallbackSpec);

    HRESULT ret = a->Open(file, 0, openCallback);
    if (E_ABORT == ret)
    {
        // E_ABORT returned on Canceled Password dialog
        return FALSE;
    }
    if (S_OK != ret)
        return Error(password.IsEmpty() ? IDS_UNSUPPORTED_ARCHIVE : IDS_CANT_OPEN_ARCHIVE_PWD, quiet, fileName);

    *archive = a.Detach();

    return TRUE;
}

BOOL C7zClient::ListArchive(const char* fileName, CSalamanderDirectoryAbstract* dir, CPluginDataInterface*& pluginData, UString& password)
{
    CMyComPtr<IInArchive> inArchive;

    if (!OpenArchive(fileName, &inArchive, password))
        return FALSE;

    UINT32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    UINT32 i;
    BOOL reportTooLongPathErr = TRUE;
    // Get the path-less archive file name
    LPCTSTR archiveName = _tcsrchr(fileName, '\\');
    if (archiveName)
        archiveName++;
    else
        archiveName = fileName;
    for (i = 0; i < numItems; i++)
        AddFileDir(inArchive, i, dir, pluginData, &reportTooLongPathErr, archiveName);

    return TRUE;
}

BOOL C7zClient::FillItemData(IInArchive* archive, UINT32 index, C7zClient::CItemData* itemData)
{
    NWindows::NCOM::CPropVariant propVariant;

    // index v archivu
    itemData->Idx = index;

    // je-li soubor zaheslovany
    if (archive->GetProperty(index, kpidEncrypted, &propVariant) != S_OK)
        itemData->Encrypted = FALSE;
    else if (propVariant.vt != VT_BOOL)
        itemData->Encrypted = FALSE;
    else
        itemData->Encrypted = VARIANT_BOOLToBool(propVariant.boolVal);

    // velikost zkomprimovaneho souboru
    if (archive->GetProperty(index, kpidPackSize, &propVariant) != S_OK)
        itemData->PackedSize = 0;
    else if (propVariant.vt == VT_EMPTY)
        itemData->PackedSize = 0;
    else
        ConvertPropVariantToUInt64(propVariant, itemData->PackedSize);

    // metoda
    if (archive->GetProperty(index, kpidMethod, &propVariant) != S_OK)
        itemData->SetMethod("Unknown");
    else if (propVariant.vt == VT_EMPTY)
        itemData->SetMethod("None");
    else
        itemData->SetMethod(GetAnsiString(propVariant.bstrVal));

    /*  // 06F10701 je id pro 7zAES -> cili heslo :)
//  if (strstr(itemData->Method, "06F10701") != NULL)
  if (strstr(itemData->Method, "7zAES") != NULL)
    itemData->Encrypted = TRUE;
  else
    itemData->Encrypted = FALSE;*/

    return TRUE;
}

BOOL C7zClient::AddFileDir(IInArchive* archive, UINT32 idx,
                           CSalamanderDirectoryAbstract* dir, CPluginDataInterface*& pluginData,
                           BOOL* reportTooLongPathErr, const char* archiveName)
{
    NWindows::NCOM::CPropVariant propVariant;
    // path
    archive->GetProperty(idx, kpidPath, &propVariant);
    CSysString path = GetAnsiString(propVariant.bstrVal);

    BOOL ret = FALSE;
    LPTSTR p = NULL;
    C7zClient::CItemData* itemData = NULL;

    if (path.IsEmpty())
    {
        // kpidPath is empty -> take the archive name w/o the last extension
        assert(!idx);
        path = archiveName;
        int dot = path.ReverseFind('.');
        if (dot >= 0)
        {
            path.Delete(dot, path.Len() - dot);
        }
    }
    try
    {
        p = new TCHAR[path.Len() + 2]; // lengthof('\\' + '\0') == 2
        // The typecast to LPCTSTR tells the compiler to call operator const T*() on the returned AString object
        _stprintf(p, "\\%s", (LPCTSTR)path);

        // name
        LPTSTR fileName = _tcsrchr(p, '\\');
        if (fileName != NULL)
        {
            *fileName = '\0';
            fileName++;
        }
        LPTSTR filePath = p;

        CFileData fd;
        fd.Name = SalamanderGeneral->DupStr(fileName);
        if (fd.Name == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw FALSE;
        } // if

        fd.NameLen = _tcslen(fd.Name);
        LPTSTR s = _tcsrchr(fd.Name, '.');
        if (s != NULL)
            fd.Ext = s + 1; // ".cvspass" ve Windows je pripona ...
        else
            fd.Ext = fd.Name + fd.NameLen;

        itemData = new C7zClient::CItemData;
        if (!itemData)
        {
            SalamanderGeneral->Free(fd.Name);
            Error(IDS_INSUFFICIENT_MEMORY);
            throw FALSE;
        }

        if (!FillItemData(archive, idx, itemData))
        {
            SalamanderGeneral->Free(fd.Name);
            delete itemData;
            throw FALSE;
        }
        fd.PluginData = (DWORD_PTR)itemData;

        // Creation Time
        archive->GetProperty(idx, kpidMTime, &propVariant);
        fd.LastWrite = propVariant.filetime;

        fd.DosName = NULL;

        // attributy
        archive->GetProperty(idx, kpidAttrib, &propVariant);
        fd.IsLink = 0;
        fd.Attr = propVariant.ulVal;

        if (fd.Attr & FILE_ATTRIBUTE_UNIX_EXTENSION)
        {
            unsigned short mode = fd.Attr >> 16; // stat.st_mode

            if (S_ISLNK(mode))
            {
                fd.IsLink = TRUE;
            }
            fd.Attr &= 0xFFFF; // Get rid of st_mode
        }

        if (itemData->Encrypted)
            fd.Attr |= FILE_ATTRIBUTE_ENCRYPTED;

        fd.Hidden = (fd.Attr & FILE_ATTRIBUTE_HIDDEN) ? 1 : 0;
        fd.IsOffline = 0;

        // is dir
        archive->GetProperty(idx, kpidIsDir, &propVariant);
        if (VARIANT_BOOLToBool(propVariant.boolVal))
        {
            fd.Size = CQuadWord(0, 0);
            fd.Attr |= FILE_ATTRIBUTE_DIRECTORY;
            fd.IsLink = 0;

            if (!SortByExtDirsAsFiles)
                fd.Ext = fd.Name + fd.NameLen; // adresare nemaji priponu

            if (dir && !dir->AddDir(filePath, fd, pluginData))
            {
                SalamanderGeneral->Free(fd.Name);
                delete itemData; // je v fd.PluginData
                // dir->Clear(pluginData);  // Petr: neni duvod vse ostatni zahodit
                if (_tcslen(filePath) > MAX_PATH - 5) // Petr: test na too-long-path prevzaty ze Salamandera
                {
                    if (*reportTooLongPathErr)
                    {
                        Error(IDS_ERRADDDIR_TOOLONG);
                        *reportTooLongPathErr = FALSE; // Petr: predchazim opakovanemu hlaseni too-long-path, muze jich byt hodne
                    }
                }
                else
                    Error(IDS_ERROR);
                throw FALSE;
            }
        }
        else
        {
            // file length
            archive->GetProperty(idx, kpidSize, &propVariant);
            ::ConvertPropVariantToUInt64(propVariant, fd.Size.Value);
            // What is better? Check on *.lnk/pif/url extensions or Unix flags or both? We do both.
            fd.IsLink |= SalamanderGeneral->IsFileLink(fd.Ext);

            // soubor
            if (dir && !dir->AddFile(filePath, fd, pluginData))
            {
                SalamanderGeneral->Free(fd.Name);
                delete itemData; // je v fd.PluginData
                // dir->Clear(pluginData);  // Petr: neni duvod vse ostatni zahodit
                if (_tcslen(filePath) > MAX_PATH - 5) // Petr: test na too-long-path prevzaty ze Salamandera
                {
                    if (*reportTooLongPathErr)
                    {
                        Error(IDS_ERRADDFILE_TOOLONG);
                        *reportTooLongPathErr = FALSE; // Petr: predchazim opakovanemu hlaseni too-long-path, muze jich byt hodne
                    }
                }
                else
                    Error(IDS_ERROR);
                throw FALSE;
            }
        }
    }
    catch (BOOL e)
    {
        ret = e;
    }

    delete[] p;

    return TRUE;
}

static int
compare(const void* arg1, const void* arg2)
{
    return *(UINT32*)arg1 - *(UINT32*)arg2;
}

int C7zClient::Decompress(CSalamanderForOperationsAbstract* salamander, const char* archiveName, const char* outDir,
                          TIndirectArray<CArchiveItemInfo>* itemList, UString& password, BOOL silentDelete /* = FALSE*/)
{
    CMyComPtr<IInArchive> inArchive;

    if (!OpenArchive(archiveName, &inArchive, password))
        return OPER_CANCEL;

    UINT32* fileIndex = NULL;
    int ret = OPER_OK;

    try
    {
        if (inArchive == NULL)
        { // Is this test really needed?
            Error(IDS_ERROR);
            throw OPER_CANCEL;
        }

        fileIndex = new UINT32[itemList->Count];
        if (fileIndex == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw OPER_CANCEL;
        }

        // vytvorit pole indexu pro extrakci
        int count = 0;
        ItemsToExtractMap itemsToExtract;
        int i;
        for (i = 0; i < itemList->Count; i++)
        {
            CArchiveItemInfo* aii = (*itemList)[i];
            if (aii == NULL)
                continue;
            const CFileData* fd = aii->FileData;
            if (fd == NULL)
                continue;
            CItemData* id = (CItemData*)fd->PluginData;
            if (id != NULL)
            {
                try
                {
                    itemsToExtract[id->Idx] = aii;
                    fileIndex[count++] = id->Idx;
                }
                catch (const std::bad_alloc&)
                {
                    Error(IDS_INSUFFICIENT_MEMORY);
                    throw OPER_CANCEL;
                }
            }
            else
                TRACE_I("C7zClient::Decompress(): PluginData == NULL (to by se nemelo stat!)");
        }

        // setup callback
        CExtractCallbackImp* extractCallbackSpec = new CExtractCallbackImp(salamander->ProgressGetHWND(), password, itemsToExtract /*, fileIndex, count*/);
        if (extractCallbackSpec == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            return OPER_CANCEL;
        }
        CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);

        FILETIME ft;
        extractCallbackSpec->Init(inArchive, outDir /*,&archiveItems*/, ft, 0, silentDelete);
        qsort(fileIndex, count, sizeof(fileIndex[0]), compare);

        // spustit vybalovani ve vlakne
        // tahle silenost je tu kvuli tomu, ze 7za.dll je multi-threadovy a nesly zobrazovat nase message boxy
        CDecompressParamObject dpo;
        dpo.Archive = inArchive;
        dpo.FileIndex = fileIndex;
        dpo.Count = count;
        dpo.Test = FALSE;
        dpo.Callback = extractCallback;

        HRESULT result = DoDecompress(salamander, &dpo);

        ret = (result == E_ABORT) ? OPER_CANCEL : ((result == S_OK) ? OPER_OK : OPER_CONTINUE);

        // kontrolujeme navratovy kod vlakna
        if (ret == OPER_CANCEL)
            extractCallbackSpec->Cleanup();
    }
    catch (int e)
    {
        ret = e;
    }

    delete[] fileIndex;

    return ret;
} /* C7zClient::Decompress */

int C7zClient::TestArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName)
{
    CMyComPtr<IInArchive> inArchive;
    UString Password;

    if (!OpenArchive(fileName, &inArchive, Password, FALSE))
        return OPER_CANCEL;

    int ret = OPER_CONTINUE;

    try
    {

        // setup callback
        ItemsToExtractMap itemsToExtract; // Can stay empty because testing decompresses everything
        CExtractCallbackImp* extractCallbackSpec = new CExtractCallbackImp(salamander->ProgressGetHWND(), Password, itemsToExtract);
        if (extractCallbackSpec == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw OPER_CANCEL;
        }
        CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);

        extractCallbackSpec->InitTest();

        // spustit vybalovani ve vlakne
        // tahle silenost je tu kvuli tomu, ze 7za.dll je multi-threadovy a nesly zobrazovat nase message boxy
        CDecompressParamObject dpo;
        dpo.Archive = inArchive;
        dpo.FileIndex = NULL;
        dpo.Count = 0xFFFFFFFF /*this will extract entire archive*/;
        dpo.Test = TRUE;
        dpo.Callback = extractCallback;

        HRESULT result = DoDecompress(salamander, &dpo);

        ret = (extractCallbackSpec->NumErrors > 0) ? OPER_CONTINUE : ((result == E_ABORT) ? OPER_CANCEL : ((result == S_OK) ? OPER_OK : OPER_CONTINUE));
    }
    catch (BOOL e)
    {
        ret = e;
    }

    return ret;
} /* C7zClient::TestArchive */

int C7zClient::GetArchiveItemList(IInArchive* archive, TIndirectArray<CArchiveItem>** archiveItems, UINT32* numItems)
{
    UINT32 itemCount = 0;
    archive->GetNumberOfItems(&itemCount);

    TIndirectArray<CArchiveItem>* items = new TIndirectArray<CArchiveItem>(itemCount, 20, dtDelete);
    if (items == NULL)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return OPER_CANCEL;
    }

    int i;
    for (i = 0; i < (signed)itemCount; i++)
    {
        NWindows::NCOM::CPropVariant propVariant;
        // path
        archive->GetProperty(i, kpidPath, &propVariant);
        UString path;
        path = propVariant.bstrVal;

        // Creation Time
        archive->GetProperty(i, kpidMTime, &propVariant);
        FILETIME lastWrite = propVariant.filetime;

        // attributy
        archive->GetProperty(i, kpidAttrib, &propVariant);
        DWORD attr = propVariant.ulVal;

        // is dir
        archive->GetProperty(i, kpidIsDir, &propVariant);
        bool isDir = VARIANT_BOOLToBool(propVariant.boolVal);

        archive->GetProperty(i, kpidSize, &propVariant);
        UINT64 size;
        ::ConvertPropVariantToUInt64(propVariant, size);

        CArchiveItem* ai = new CArchiveItem(i, path, size, attr, lastWrite, isDir);
        if (ai == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            return OPER_CANCEL;
        }
        items->Add(ai);
    }

    *numItems = itemCount;
    *archiveItems = items;

    return OPER_OK;
} /* C7zClient::GetArchiveItemList */

//
// archiveItems - polozky v archivu
// deleteItems - polozky, ktere se budou z archivu mazat
// updateList - vysledny seznam polozek, ktere v archivu zbydou
//
int C7zClient::DeleteMakeUpdateList(TIndirectArray<CArchiveItem>* archiveItems, TIndirectArray<CArchiveItemInfo>* deleteList,
                                    TIndirectArray<CUpdateInfo>* updateList)
{
    // z archiveItems a deleteList vyrobit updateList, coz je seznam polozek, ktere zbudou v archivu
    int archIdx;
    for (archIdx = 0; archIdx < archiveItems->Count; archIdx++)
    {
        // ziskat index polozky v archivu
        int archItemIdx = (*archiveItems)[archIdx]->Idx;
        bool bToBeDeleted = false;

        int delIdx;
        for (delIdx = 0; delIdx < deleteList->Count; delIdx++)
        {
            CArchiveItemInfo* aii = (*deleteList)[delIdx];
            if (aii != NULL)
            {
                const CFileData* fd = aii->FileData;
                if (fd != NULL)
                {
                    CItemData* id = (CItemData*)fd->PluginData;
                    if (id != NULL)
                    {
                        if ((UINT32)archItemIdx == id->Idx)
                        {
                            bToBeDeleted = true;
                            deleteList->Delete(delIdx);
                            break;
                        }
                    }
                }
            }
        }
        if (!bToBeDeleted)
        {
            CUpdateInfo* ui = new CUpdateInfo;
            if (ui == NULL)
            {
                Error(IDS_INSUFFICIENT_MEMORY);
                return OPER_CANCEL;
            }
            ui->ExistsInArchive = 1;
            ui->ArchiveItemIndex = archItemIdx;
            ui->ExistsOnDisk = 1;
            ui->FileItemIndex = -1;
            ui->IsAnti = false;
            ui->NewData = ui->NewProperties = false;
            updateList->Add(ui);
        }
    }

    _ASSERTE(deleteList->Count == 0);
    return OPER_OK;
} /* C7zClient::DeleteMakeUpdateList */

int C7zClient::Delete(CSalamanderForOperationsAbstract* salamander, const char* archiveName,
                      TIndirectArray<CArchiveItemInfo>* deleteList, bool passwordIsDefined, UString& password)
{
    char tmpName[MAX_PATH];
    DWORD err;

    char srcPath[MAX_PATH];
    strcpy(srcPath, archiveName);
    char* rbackslash = _tcsrchr(srcPath, '\\');
    if (rbackslash != NULL)
        *rbackslash = '\0';

    if (!SalamanderGeneral->SalGetTempFileName(srcPath, "sal", tmpName, TRUE, &err))
    {
        SysError(IDS_CANT_CREATE_TMPFILE, err, FALSE);
        return OPER_CANCEL;
    }
    //  TRACE_I("TempName: " << tmpName);

    CMyComPtr<IInArchive> inArchive;

    if (!OpenArchive(archiveName, &inArchive, password))
        return OPER_CANCEL;

    int ret = OPER_CANCEL;
    TIndirectArray<CArchiveItem>* archiveItems = NULL;

    try
    {
        //    HRESULT result;
        IOutArchive* outArchive;

        CMyComPtr<IInArchive> archive2 = inArchive;
        if (archive2.QueryInterface(IID_IOutArchive, &outArchive) != S_OK)
        {
            Error(IDS_UPDATE_NOT_SUPPORTED);
            throw OPER_CANCEL;
        }

        //
        CRetryableOutFileStream* outStreamSpec = new CRetryableOutFileStream(salamander->ProgressGetHWND());
        if (outStreamSpec == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw OPER_CANCEL;
        }
        CMyComPtr<IOutStream> outStream(outStreamSpec);

        if (!outStreamSpec->Open(tmpName, OPEN_EXISTING))
        {
            Error(IDS_CANT_CREATE_ARCHIVE);
            throw OPER_CANCEL;
        }

        // vylistovat si archiv
        UINT32 numItems = 0;
        if (GetArchiveItemList(inArchive, &archiveItems, &numItems) == OPER_CANCEL)
            throw OPER_CANCEL;

        TIndirectArray<CUpdateInfo> updateList(numItems, 20, dtDelete);
        if (DeleteMakeUpdateList(archiveItems, deleteList, &updateList) == OPER_CANCEL)
            throw OPER_CANCEL;

        CArchiveUpdateCallback* updateCallbackSpec = new CArchiveUpdateCallback(salamander->ProgressGetHWND());
        if (updateCallbackSpec == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw OPER_CANCEL;
        }
        CMyComPtr<IArchiveUpdateCallback> updateCallback(updateCallbackSpec);

        //    updateCallbackSpec->Init(NULL, archiveItems, &updateList, passwordIsDefined, password);
        updateCallbackSpec->FileItems = NULL;
        updateCallbackSpec->ArchiveItems = archiveItems;
        updateCallbackSpec->UpdateList = &updateList;

        updateCallbackSpec->PasswordIsDefined = passwordIsDefined;
        updateCallbackSpec->Password = password;
        updateCallbackSpec->AskPassword = passwordIsDefined;

        // TODO: co s delete? jde nacit kompressni parametry? a musi se vubec nastavovat?
        //    SetCompressionParams(outArchive, compr);

        // spustit update ve vlakne
        // tahle silenost je tu kvuli tomu, ze 7za.dll je multi-threadovy a nesly zobrazovat nase message boxy
        CUpdateParamObject upo;
        upo.Archive = outArchive;
        upo.Stream = outStream;
        upo.Count = updateList.Count;
        upo.Callback = updateCallback;

        HRESULT result = DoUpdate(salamander, &upo);

        ret = (result == E_ABORT) ? OPER_CANCEL : ((result == S_OK) ? OPER_OK : OPER_CONTINUE);

        outArchive->Release();
        outStream.Release();

        //    if (result == E_ABORT) throw OPER_CANCEL;
        if (ret == S_OK)
        {
            // zavrit otevreny archiv
            inArchive->Close();
            // smazat ho
            if (::DeleteFile(archiveName))
            {
                DWORD err2;
                // prejemnovat na tmpfile na archiv
                if (!SalamanderGeneral->SalMoveFile(tmpName, archiveName, &err2))
                {
                    SysError(IDS_CANT_MOVE_TMPARCHIVE, err2, FALSE, tmpName);
                    throw OPER_CANCEL;
                }
            }
            else
            {
                Error(IDS_CANT_UPDATE_ARCHIVE, FALSE, archiveName);
                throw OPER_CANCEL;
            }
        }
        else
        {
            if (ret == OPER_CANCEL)
                throw OPER_CANCEL;

            NWindows::NCOM::CPropVariant propVariant;
            inArchive->GetArchiveProperty(kpidSolid, &propVariant);
            // zahlasit error a ze to neni nase vina
            Error(VARIANT_BOOLToBool(propVariant.boolVal) ? IDS_7Z_SOLID_DELETE_UNSUP : IDS_7Z_UPDATE_ERROR);
            /*
      // smazat temp soubor
      if (!::DeleteFile(tmpName))
      {
        DWORD err = ::GetLastError();
        // zareportit chybu
        SysError(IDS_CANT_DELETE_TMPARCHIVE, err, FALSE, tmpName);
        throw OPER_CANCEL;
      }
*/
        }

        //    ret = (result == S_OK) ? OPER_OK : OPER_CONTINUE;
    }
    catch (int e)
    {
        ret = e;
    }

    delete archiveItems;
    ::DeleteFile(tmpName);

    return ret;
} /* C7zClient::Delete */

struct CSortItem
{
    const UString* String;
    int Index;
};

static int __cdecl CompareStrings(const void* a1, const void* a2)
{
    const UString& s1 = *((CSortItem*)a1)->String;
    const UString& s2 = *((CSortItem*)a2)->String;
    return MyStringCompareNoCase(s1, s2);
}

static void
IndirectSort(UStringVector& strings, CIntVector& indexes)
{
    indexes.Clear();
    if (strings.IsEmpty())
        return;
    int numItems = strings.Size();
    //CPointerVector pointers;
    //pointers.Reserve(numItems);
    indexes.Reserve(numItems);
    TDirectArray<CSortItem> pointers(numItems, 1);
    int i;
    for (i = 0; i < numItems; i++)
    {
        CSortItem si;
        si.String = &strings[i];
        si.Index = i;
        pointers.Add(si);
    }
    qsort(&pointers[0], numItems, sizeof(CSortItem), CompareStrings);
    for (i = 0; i < numItems; i++)
    {
        indexes.Add(pointers[i].Index);
    }
}

static int
AddFileUpdateInfo(TIndirectArray<CUpdateInfo>* updateList, int fileIdx)
{
    CUpdateInfo* ui = new CUpdateInfo;
    if (ui == NULL)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return OPER_CANCEL;
    }

    ui->NewData = true;
    ui->NewProperties = true;
    ui->ExistsInArchive = false;
    ui->FileItemIndex = fileIdx;
    ui->ExistsOnDisk = true;
    ui->IsAnti = false;
    ui->ExistsInArchive = false;
    updateList->Add(ui);

    return OPER_OK;
}

static int
AddArchiveUpdateInfo(TIndirectArray<CUpdateInfo>* updateList, int archiveIdx)
{
    CUpdateInfo* ui = new CUpdateInfo;
    if (ui == NULL)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return OPER_CANCEL;
    }
    ui->ExistsInArchive = true;
    ui->ArchiveItemIndex = archiveIdx;
    ui->ExistsOnDisk = true;
    ui->FileItemIndex = -1;
    ui->IsAnti = false;
    ui->NewData = ui->NewProperties = false;
    updateList->Add(ui);

    return OPER_OK;
}

int C7zClient::UpdateMakeUpdateList(TIndirectArray<CFileItem>* fileList, TIndirectArray<CArchiveItem>* archiveItems,
                                    TIndirectArray<CUpdateInfo>* updateList)
{
    int i;

    // promenne pro overwite dialog
    BOOL overwriteSilent = FALSE;
    enum EOperationMode
    {
        Ask,
        Overwrite,
        Skip
    } overwriteMode = Ask;

    // logicky seradit fileList
    CIntVector fileIndexes;
    UStringVector fileNames;
    int fileItemCount = fileList->Count;
    for (i = 0; i < fileItemCount; i++)
        fileNames.Add((*fileList)[i]->Name);
    IndirectSort(fileNames, fileIndexes);

    // logicky seradit archiveList
    CIntVector archiveIndexes;
    UStringVector archiveNames;
    int archiveItemCount = archiveItems->Count;
    for (i = 0; i < archiveItemCount; i++)
        archiveNames.Add((*archiveItems)[i]->Name);
    IndirectSort(archiveNames, archiveIndexes);

    int fileIterIndex = 0;
    int archiveIterIndex = 0;
    while (fileIterIndex < fileItemCount && archiveIterIndex < archiveItemCount)
    {
        int fileIdx = fileIndexes[fileIterIndex];
        int archiveIdx = archiveIndexes[archiveIterIndex];

        CFileItem* fi = (*fileList)[fileIdx];
        const CArchiveItem* ai = (*archiveItems)[archiveIdx];
        int cmpRes = MyStringCompareNoCase(fi->Name, ai->Name);

        if (cmpRes < 0)
        {
            if (AddFileUpdateInfo(updateList, fileIdx) == OPER_CANCEL)
                return OPER_CANCEL;
            fileIterIndex++;
        }
        else if (cmpRes > 0)
        {
            if (AddArchiveUpdateInfo(updateList, archiveIdx) == OPER_CANCEL)
                return OPER_CANCEL;
            archiveIterIndex++;
        }
        else // cmpRes == 0
        {
            if (!ai->IsDir)
            {
                // prepisovani nas zajima jen u souboru
                EOperationMode mode = overwriteMode;

                if (mode == Ask)
                {
                    char fifd[1024], aifd[1024];
                    GetInfo(fifd, &fi->LastWriteTime, fi->Size);
                    GetInfo(aifd, &ai->LastWrite, ai->Size);

                    int userAct = SalamanderGeneral->DialogOverwrite(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_YESALLSKIPCANCEL,
                                                                     GetAnsiString(ai->Name), aifd, GetAnsiString(fi->FullPath), fifd);

                    switch (userAct)
                    {
                    case DIALOG_YES:
                        mode = Overwrite;
                        break;
                    case DIALOG_ALL:
                        mode = Overwrite;
                        overwriteSilent = TRUE;
                        break;
                    case DIALOG_SKIP:
                        mode = Skip;
                        break;
                    case DIALOG_SKIPALL:
                        mode = Skip;
                        overwriteSilent = TRUE;
                        break;
                    case DIALOG_CANCEL:
                        return OPER_CANCEL;
                    }

                    if (overwriteSilent)
                        overwriteMode = mode;
                }

                switch (mode)
                {
                case Overwrite:
                    if (AddFileUpdateInfo(updateList, fileIdx) == OPER_CANCEL)
                        return OPER_CANCEL;
                    fi->CanDelete = TRUE;
                    break;

                case Skip:
                    if (AddArchiveUpdateInfo(updateList, archiveIdx) == OPER_CANCEL)
                        return OPER_CANCEL;
                    fi->CanDelete = FALSE;
                    break;
                } // switch
            }

            fileIterIndex++;
            archiveIterIndex++;
        }
    }
    // dokoncit vytvoreni seznamu
    for (; fileIterIndex < fileItemCount; fileIterIndex++)
    {
        if (AddFileUpdateInfo(updateList, fileIndexes[fileIterIndex]) == OPER_CANCEL)
            return OPER_CANCEL;
    }

    for (; archiveIterIndex < archiveItemCount; archiveIterIndex++)
    {
        if (AddArchiveUpdateInfo(updateList, archiveIndexes[archiveIterIndex]) == OPER_CANCEL)
            return OPER_CANCEL;
    }

    return OPER_OK;
} /* C7zClient::UpdateMakeUpdateList */

// nastavi parametry komprese podle konfigurace pluginu
HRESULT
C7zClient::SetCompressionParams(IOutArchive* outArchive, CCompressParams* compressParams)
{
    CMyComPtr<ISetProperties> setProperties;
    if (outArchive->QueryInterface(IID_ISetProperties, (void**)&setProperties) == S_OK)
    {
        std::vector<NWindows::NCOM::CPropVariant> values;
        CRecordVector<const wchar_t*> names;

        // compression level
        names.Add(L"x");
        values.push_back(NWindows::NCOM::CPropVariant((UINT32)compressParams->CompressLevel));

        // solid archive
        names.Add(L"s");
        values.push_back(NWindows::NCOM::CPropVariant(compressParams->SolidArchive ? L"2g" : L"off"));

        // TODO: multi volume options

        // kompresni parametry nastavovat pokud opravdu komprimujeme
        // nastavovat jako posledni
        if (compressParams->CompressLevel != COMPRESS_LEVEL_STORE)
        {
            char dictSizeStr[32];
            NWindows::NCOM::CPropVariant prop;
            switch (compressParams->Method)
            {
            case CCompressParams::LZMA:
                // set method
                names.Add(L"0");
                values.push_back(NWindows::NCOM::CPropVariant(L"LZMA"));

                // set dictionary size
                names.Add(L"0d");
                sprintf(dictSizeStr, "%dB", compressParams->DictSize * 1024); // DictSize je v KB
                values.push_back(NWindows::NCOM::CPropVariant(GetUnicodeString(dictSizeStr)));

                // set word size
                names.Add(L"0fb");
                prop = compressParams->WordSize;
                values.push_back(prop);
                break;

            case CCompressParams::LZMA2:
                // set method
                names.Add(L"0");
                values.push_back(NWindows::NCOM::CPropVariant(L"LZMA2"));

                // set dictionary size
                names.Add(L"0d");
                sprintf(dictSizeStr, "%dB", compressParams->DictSize * 1024); // DictSize je v KB
                values.push_back(NWindows::NCOM::CPropVariant(GetUnicodeString(dictSizeStr)));

                // set word size
                names.Add(L"0fb");
                prop = compressParams->WordSize;
                values.push_back(prop);
                break;

            case CCompressParams::PPMd:
                // set method
                names.Add(L"0");
                values.push_back(NWindows::NCOM::CPropVariant(L"PPMd"));

                // set dictionary size
                names.Add(L"0mem");
                sprintf(dictSizeStr, "%dB", compressParams->DictSize * 1024); // DictSize je v KB
                values.push_back(NWindows::NCOM::CPropVariant(GetUnicodeString(dictSizeStr)));

                // set word size
                names.Add(L"0o");
                prop = compressParams->WordSize;
                values.push_back(prop);
                break;
            } // switch
        }

        RINOK(setProperties->SetProperties(&names.Front(), &values.front(), names.Size()));
    }

    return S_OK;
}

int C7zClient::Update(CSalamanderForOperationsAbstract* salamander, const char* archiveName,
                      const char* srcPath, BOOL isNewArchive, TIndirectArray<CFileItem>* fileList,
                      CCompressParams* compressParams, bool passwordIsDefined, UString password)
{
    char tmpName[MAX_PATH];
    // oriznout filename z archiveName, zustane nam cilova cesta, kam budeme vybalovat
    lstrcpy(tmpName, archiveName);
    SalamanderGeneral->CutDirectory(tmpName, NULL);
    DWORD err;
    if (!SalamanderGeneral->SalGetTempFileName(tmpName, "sal", tmpName, TRUE, &err))
    {
        SysError(IDS_CANT_CREATE_TMPFILE, err, FALSE);
        return OPER_CANCEL;
    }

    TIndirectArray<CArchiveItem>* archiveItems = NULL;
    TIndirectArray<CUpdateInfo>* updateList = NULL;
    int ret = OPER_CANCEL;
    CMyComPtr<IInArchive> inArchive;

    try
    {
        CMyComPtr<IOutArchive> outArchive;
        UINT32 numItems = 0;
        if (isNewArchive)
        {
            // new
            if (!CreateObject(&IID_IOutArchive, (void**)&outArchive))
                throw OPER_CANCEL;

            // vytvorit si prazdne pole
            archiveItems = new TIndirectArray<CArchiveItem>(1, 20, dtDelete);
            if (archiveItems == NULL)
            {
                Error(IDS_INSUFFICIENT_MEMORY);
                throw OPER_CANCEL;
            }
        }
        else
        {
            // pridavani polozek do existujiciho archivu
            if (!OpenArchive(archiveName, &inArchive, password))
                throw OPER_CANCEL;

            // update
            CMyComPtr<IInArchive> archive2 = inArchive;
            if (archive2.QueryInterface(IID_IOutArchive, &outArchive) != S_OK)
            {
                Error(IDS_UPDATE_NOT_SUPPORTED);
                throw OPER_CANCEL;
            }

            // vylistovat si archiv
            if (GetArchiveItemList(inArchive, &archiveItems, &numItems) == OPER_CANCEL)
                throw OPER_CANCEL;
        }

        // vystupni stream
        CRetryableOutFileStream* outStreamSpec = new CRetryableOutFileStream(salamander->ProgressGetHWND());
        if (outStreamSpec == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw OPER_CANCEL;
        }
        CMyComPtr<IOutStream> outStream(outStreamSpec);

        if (!outStreamSpec->Open(tmpName, OPEN_EXISTING))
        {
            Error(IDS_CANT_CREATE_ARCHIVE);
            throw OPER_CANCEL;
        }

        // velikost je pocet polozek v archivu + pocet pridavanych polozek
        updateList = new TIndirectArray<CUpdateInfo>(numItems + fileList->Count, 20, dtDelete);
        if (updateList == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw OPER_CANCEL;
        }

        // pripravit 'davku' pro zpravovani
        if (UpdateMakeUpdateList(fileList, archiveItems, updateList) == OPER_CANCEL)
            throw OPER_CANCEL;

        // zpracovat
        CArchiveUpdateCallback* updateCallbackSpec = new CArchiveUpdateCallback(salamander->ProgressGetHWND());
        if (updateCallbackSpec == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw OPER_CANCEL;
        }
        CMyComPtr<IArchiveUpdateCallback> updateCallback(updateCallbackSpec);

        //    updateCallbackSpec->Init(fileList, archiveItems, updateList, passwordIsDefined, password);
        updateCallbackSpec->FileItems = fileList;
        updateCallbackSpec->ArchiveItems = archiveItems;
        updateCallbackSpec->UpdateList = updateList;

        updateCallbackSpec->PasswordIsDefined = passwordIsDefined;
        updateCallbackSpec->Password = password;
        updateCallbackSpec->AskPassword = passwordIsDefined;

        SetCompressionParams(outArchive, compressParams);

        // spustit update ve vlakne
        // tahle silenost je tu kvuli tomu, ze 7za.dll je multi-threadovy a nesly zobrazovat nase message boxy
        CUpdateParamObject upo;
        upo.Archive = outArchive;
        upo.Stream = outStream;
        upo.Count = updateList->Count;
        upo.Callback = updateCallback;

        HRESULT result = DoUpdate(salamander, &upo);

        ret = (result == E_ABORT) ? OPER_CANCEL : ((result == S_OK) ? OPER_OK : OPER_CONTINUE);

        outStream.Release();
        outArchive.Release();

        if (ret == OPER_OK)
        {
            // nakonec jeste prejmenovat tmp
            if (!isNewArchive)
            {
                // zavrit otevreny archiv
                inArchive->Close();
                // smazat ho
                if (!::DeleteFile(archiveName))
                {
                    Error(IDS_CANT_UPDATE_ARCHIVE, FALSE, archiveName);
                    throw OPER_CANCEL;
                }
            }

            DWORD err2;
            // prejmenovat tmpfile na archiv
            if (!SalamanderGeneral->SalMoveFile(tmpName, archiveName, &err2))
            {
                SysError(IDS_CANT_MOVE_TMPARCHIVE, err2, FALSE, tmpName);
                throw OPER_CANCEL;
            }
        }
        else
        {
            if (ret == OPER_CANCEL)
                throw OPER_CANCEL;

            //      if (updateCallback.SystemError != 0) {
            //      }
            //      else {
            if (FAILED(result) && ((FACILITY_WIN32 << 16) == (result & 0x7FFF0000)))
            {
                // LastError error encoded into HRESULT
                // There is something strange: E_OUTOFMEMORY as 0x8007000EL prints as "Not enough storage is available to complete this operation"
                // even when not truncated to 16 bits while 0x80000002L prints as "Ran out of memory"
                SysError(IDS_7Z_FATAL_ERROR, (result == E_OUTOFMEMORY) ? 0x80000002L : (result & 0xFFFF), FALSE);
            }
            else
            {
                int id;

                if (inArchive)
                {
                    // Updating solid archives is supported since some 9.0x version
                    /*          NWindows::NCOM::CPropVariant propVariant;

          inArchive->GetArchiveProperty(kpidSolid, &propVariant);
          id = VARIANT_BOOLToBool(propVariant.boolVal) ? IDS_7Z_SOLID_UPDATE_UNSUP : IDS_7Z_UPDATE_UNKNOWN_ERROR;*/
                    id = IDS_7Z_UPDATE_UNKNOWN_ERROR;
                }
                else
                {
                    id = IDS_7Z_CREATE_UNKNOWN_ERROR;
                }
                Error(id, FALSE, result);
            }
            //      }
            /*
      // smazat temp soubor
      if (!::DeleteFile(tmpName))
      {
        SysError(IDS_CANT_DELETE_TMPARCHIVE, ::GetLastError(), FALSE, tmpName);
        throw OPER_CANCEL;
      }
*/
        }
    }
    catch (int e)
    {
        ret = e;
    }

    delete archiveItems;
    delete updateList;

    ::DeleteFile(tmpName);

    return ret;
} /* C7zClient::Update */
