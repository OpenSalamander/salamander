// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "7zip.h"
#include "update.h"
#include "7zip.rh"
#include "7zip.rh2"
#include "lang\lang.rh"
#include "dialogs.h"
#include "7zthreads.h"
#include "FStreams.h"

#include "7za/CPP/Common/StringConvert.h"
#include "7za/CPP/Common/IntToString.h"
#include "7za/CPP/Common/Defs.h"
#include "7za/CPP/Windows/PropVariant.h"

#include "7za/CPP/7zip/Common/FileStreams.h"

CArchiveUpdateCallback::CArchiveUpdateCallback(HWND _hProgWnd)
{
    InitializeCriticalSection(&CSUpdate);
    FileItems = NULL;
    ArchiveItems = NULL;
    UpdateList = NULL;

    Silent = FALSE;
    hProgWnd = _hProgWnd;
}

CArchiveUpdateCallback::~CArchiveUpdateCallback()
{
    DeleteCriticalSection(&CSUpdate);
}

STDMETHODIMP CArchiveUpdateCallback::SetTotal(UInt64 size)
{
    Total.Value = size;
    SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_SETTOTAL, (LPARAM)&Total);
    return S_OK;
}

STDMETHODIMP CArchiveUpdateCallback::SetCompleted(const UInt64* completeValue)
{
    if (completeValue != NULL)
    {
        Completed.Value = *completeValue;

        return (HRESULT)SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_PROGRESS, (LPARAM)&Completed);
    }

    return S_OK;
}

STDMETHODIMP CArchiveUpdateCallback::EnumProperties(IEnumSTATPROPSTG** enumerator)
{
    return E_NOTIMPL;
}

STDMETHODIMP CArchiveUpdateCallback::GetUpdateItemInfo(UInt32 index,
                                                       Int32* newData, Int32* newProperties, UInt32* indexInArchive)
{
    const CUpdateInfo* ui = (*UpdateList)[index];
    if (ui == NULL)
        return E_FAIL;

    if (newData != NULL)
        *newData = BoolToInt(ui->NewData);
    if (newProperties != NULL)
        *newProperties = BoolToInt(ui->NewProperties);
    if (indexInArchive != NULL)
    {
        if (ui->ExistsInArchive)
            *indexInArchive = (*ArchiveItems)[ui->ArchiveItemIndex]->Idx;
        else
            *indexInArchive = UINT32(-1);
    }
    /*
  if (ui->ExistsOnDisk)
  {
    const CFileItem *fi = (*FileItems)[ui->FileItemIndex];
    ProcessedFileName = GetAnsiString(fi->Name);

    if (::SetEvent(HProgressAddFileName))
      ::WaitForSingleObject(HDone, INFINITE);
  }
*/
    return S_OK;
}

STDMETHODIMP CArchiveUpdateCallback::GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value)
{
    NWindows::NCOM::CPropVariant propVariant;
    const CUpdateInfo* ui = (*UpdateList)[index];
    if (ui == NULL)
        return E_FAIL;

    if (propID == kpidIsAnti)
    {
        propVariant = ui->IsAnti;
        propVariant.Detach(value);
        return S_OK;
    }

    if (ui->IsAnti)
    {
        switch (propID)
        {
        case kpidIsDir:
        case kpidPath:
            break;
        case kpidSize:
            propVariant = (UINT64)0;
            propVariant.Detach(value);
            return S_OK;
        default:
            propVariant.Detach(value);
            return S_OK;
        }
    }

    if (ui->ExistsOnDisk)
    {
        const CFileItem* fi = (*FileItems)[ui->FileItemIndex];
        switch (propID)
        {
        case kpidPath:
            propVariant = fi->Name;
            break;

        case kpidIsDir:
            propVariant = fi->IsDir;
            break;

        case kpidSize:
            propVariant = fi->Size;
            break;

        case kpidAttrib:
            propVariant = fi->Attributes;
            break;

        case kpidATime:
            propVariant = fi->LastAccessTime;
            break;

        case kpidCTime:
            propVariant = fi->CreationTime;
            break;

        case kpidMTime:
            propVariant = fi->LastWriteTime;
            break;
        }
    }
    propVariant.Detach(value);
    return S_OK;
}

STDMETHODIMP CArchiveUpdateCallback::GetStream(UInt32 index,
                                               ISequentialInStream** inStream)
{
    /*
  char u[1024];
  sprintf(u, "UpdateCallback::GetStream: %d", index);
  TRACE_I(u);
*/

    HRESULT res = S_OK;

    EnterCriticalSection(&CSUpdate);

    try
    {
        const CUpdateInfo* ui = (*UpdateList)[index];
        if (!ui->NewData)
            throw E_FAIL;

        if (ui->IsAnti)
            throw S_OK;

        const CFileItem* fi = (*FileItems)[ui->FileItemIndex];
        ProcessedFileName = GetAnsiString(fi->Name);

        // Show file name in the progress dialog
        SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_ADDTEXT, (LPARAM)(const char*)GetProcessedFile());

        if (fi->IsDir)
            throw S_OK;

        CRetryableInFileStream* inStreamSpec = new CRetryableInFileStream(hProgWnd);
        if (inStreamSpec == NULL)
        {
            MSGBOXEX_PARAMS mbep;
            ZeroMemory(&mbep, sizeof(mbep));
            mbep.HParent = hProgWnd;
            mbep.Caption = LoadStr(IDS_PLUGINNAME);
            mbep.Text = LoadStr(IDS_INSUFFICIENT_MEMORY);
            mbep.Flags = MSGBOXEX_ICONEXCLAMATION;
            SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_SHOWMBOXEX, (LPARAM)&mbep);
            throw E_FAIL;
        }

        // Retry / Skip / Skip All / Cancel dialog
        int mbRet;
        CMyComPtr<IInStream> inStreamLoc(inStreamSpec);
        do
        {
            mbRet = DIALOG_OK;
            if (!inStreamSpec->Open(GetAnsiString(fi->FullPath)))
            {
                mbRet = DIALOG_SKIP;
                if (!Silent)
                {
                    DWORD err = ::GetLastError();
                    AString fn = GetAnsiString(fi->FullPath);
                    // Warning: GetStream can get called from a parallel thread launched by 7za.dll!
                    CDialogErrorParams dep;

                    dep.Flags = BUTTONS_RETRYSKIPCANCEL;
                    dep.FileName = fn;
                    dep.Error = SalamanderGeneral->GetErrorText(err);
                    mbRet = (int)SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_DIALOGERROR, (LPARAM)&dep);
                }
            }
        } while (mbRet == DIALOG_RETRY);

        BOOL ret = FALSE;
        switch (mbRet)
        {
        case DIALOG_OK: /* do nothing */
            break;
        case DIALOG_CANCEL:
            throw E_ABORT;
            break;
        case DIALOG_SKIP:
            throw S_FALSE;
            break;
        case DIALOG_SKIPALL:
            Silent = TRUE;
            throw S_FALSE;
            break;
        }

        *inStream = inStreamLoc.Detach();
    }
    catch (HRESULT e)
    {
        res = e;
    }

    LeaveCriticalSection(&CSUpdate);

    return res;
}

STDMETHODIMP CArchiveUpdateCallback::SetOperationResult(Int32 operationResult)
{
    /*  char u[1024];
  sprintf(u, "UpdateCallback::SetOperationResult: %d", operationResult);
  TRACE_I(u);
*/
    return S_OK;
}

STDMETHODIMP CArchiveUpdateCallback::GetVolumeSize(UInt32 index, UInt64* size)
{
    if (VolumesSizes.Size() == 0)
        return S_FALSE;
    if (index >= (UInt32)VolumesSizes.Size())
        index = VolumesSizes.Size() - 1;
    *size = VolumesSizes[index];
    return S_OK;
}

STDMETHODIMP CArchiveUpdateCallback::GetVolumeStream(UInt32 index, ISequentialOutStream** volumeStream)
{
    wchar_t temp[32];
    ConvertUInt64ToString(index + 1, temp);
    UString res = temp;
    while (res.Len() < 2)
        res = UString(L'0') + res;
    // FIXME: This has never worked: VolName & VolExt have always been empty...
    /*  UString fileName = VolName;
  fileName += L'.';
  fileName += res;
  fileName += VolExt;
  CRetryableOutFileStream *streamSpec = new CRetryableOutFileStream(hProgWnd);
  CMyComPtr<ISequentialOutStream> streamLoc(streamSpec);
  if(!streamSpec->Create(fileName, false))
    return ::GetLastError();
  *volumeStream = streamLoc.Detach();
  return S_OK;*/
    return S_FALSE;
}

STDMETHODIMP CArchiveUpdateCallback::CryptoGetTextPassword2(Int32* passwordIsDefined, BSTR* password)
{
    TRACE_I("CArchiveUpdateCallback::CryptoGetTextPassword2");

    if (!PasswordIsDefined && AskPassword)
    {
        char pwd[PASSWORD_LEN];

        pwd[0] = 0;
        switch (SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_PASSWORD, (LPARAM)pwd))
        {
        case IDOK:
            PasswordIsDefined = true;
            Password = GetUnicodeString(pwd);
            break;

        case IDCANCEL:
            return E_ABORT;
            /*
      // pripraveno pri skip a skip all (snad to rusak v pristi verzi dodela)
      case DIALOG_SKIP:
        PasswordIsDefined = false;
        AskPassword = false;
        return S_OK;

      case DIALOG_SKIPALL:
        PasswordIsDefined = false;
        AskPassword = true;
        return S_FALSE;
*/
        }
    }
    StringToBstr(Password, password);
    *passwordIsDefined = BoolToInt(PasswordIsDefined);

    return S_OK;
}
