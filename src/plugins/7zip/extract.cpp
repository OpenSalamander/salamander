// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "7zip.h"
#include "extract.h"
#include "7zip.rh"
#include "7zip.rh2"
#include "lang\lang.rh"
#include "math.h"
#include "dialogs.h"
#include "7zthreads.h"
#include "7zclient.h"

#include "Common/StringConvert.h"
#include "Windows/Defs.h"
#include "Windows/PropVariant.h"
#include "Windows/PropVariantConv.h"
#include "7zip/IPassword.h"

using namespace NWindows;
using namespace NFile;

CExtractCallbackImp::CExtractCallbackImp(HWND _hProgWnd, UString& password,
                                         ItemsToExtractMap& itemsToExtract) : Password(password), ItemsToExtract(itemsToExtract)
{
    //  Silent = false; // priprava pro skip a skip all

    InitializeCriticalSection(&CSExtract);

    OutFileStreamSpec = NULL;
    TargetDir = NULL;
    hProgWnd = _hProgWnd;
}

CExtractCallbackImp::~CExtractCallbackImp()
{
    DeleteCriticalSection(&CSExtract);
}

// silent - pokud je TRUE a pri vybalovani dojde k chybe DataError (vetsinou spatne zadane heslo), soubor je automaticky
// smazany, na nic se uzivatele neptame (pouziva se pri vybalovani jednoho souboru pro prohlizeni - F3)
BOOL CExtractCallbackImp::Init(IInArchive* archive, const char* outDir,
                               const FILETIME& utcLastWriteTimeDefault, DWORD attributesDefault, BOOL silentDelete /* = FALSE*/)
{
    NumErrors = 0;

    UTCLastWriteTimeDefault = utcLastWriteTimeDefault;
    AttributesDefault = attributesDefault;
    ArchiveHandler = archive;
    TargetDir = outDir;

    //  ItemsToExtract = itemsToExtract;

    PasswordIsDefined = !Password.IsEmpty();

    TargetFileName[0] = '\0';

    // inicializace promennych pro interakci s userem
    SilentDelete = silentDelete;
    if (silentDelete)
    {
        DataErrorMode = Delete;
        DataErrorSilent = TRUE;
        DataErrorDeleteSilent = TRUE;
    }
    else
    {
        DataErrorSilent = FALSE;
        DataErrorMode = Ask;
        DataErrorDeleteSilent = FALSE;
    }

    //
    //  OverwriteMode = Ask;
    //  OverwriteSilent = FALSE;
    OverwriteSilent = 0;
    OverwriteSkip = FALSE;
    OverwriteCancel = FALSE;

    return TRUE;
}

BOOL CExtractCallbackImp::InitTest()
{
    NumErrors = 0;
    TargetFileName[0] = '\0';

    OverwriteCancel = FALSE;
    PasswordIsDefined = !Password.IsEmpty();

    return TRUE;
}

void CExtractCallbackImp::Cleanup()
{
    if (!WholeFile && OutFileStreamSpec != NULL)
    {
        //    TRACE_I("Cleanup File: " << GetAnsiString(ProcessedFileInfo.FileName));

        OutFileStream.Release();

        BOOL silent = FALSE;
        SafeDeleteFile(GetAnsiString(ProcessedFileInfo.FileName), silent);
    }
}

STDMETHODIMP CExtractCallbackImp::SetTotal(UINT64 size)
{
    //  TRACE_I("CExtractCallbackImp::SetTotal: size=" << (DWORD)size);

    Total.Value = size;
    SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_SETTOTAL, (LPARAM)&Total);

    return S_OK;
}

STDMETHODIMP CExtractCallbackImp::SetCompleted(const UINT64* completeValue)
{
    //  TRACE_I("CExtractCallbackImp::SetCompleted: completeValue=" << (DWORD)(*completeValue));

    if (completeValue != NULL)
    {
        Completed.Value = *completeValue;

        return (HRESULT)SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_PROGRESS, (LPARAM)&Completed);
    }

    return S_OK;
}

STDMETHODIMP CExtractCallbackImp::GetStream(UINT32 index, ISequentialOutStream** outStream, INT32 askExtractMode)
{
    HRESULT ret = S_OK;

    // uz jsme dali cancel a zase nam to vlezlo do callbacku (je to zpusobeno implementaci 7za.dll)
    if (OverwriteCancel)
        return E_ABORT;

    EnterCriticalSection(&CSExtract);

    try
    {
        /*    char u[1024];
    sprintf(u, "CExtractCallbackImp::GetStream: index: %d, askExtractMode: %d", index, askExtractMode);
    TRACE_I(u);
*/
        //    TRACE_I("CExtractCallbackImp::GetStream: index=" << index << ", askExtractMode=" << askExtractMode);

        *outStream = NULL;
        OutFileStream.Release();

        // pokud provadime extrakci souboru
        if (askExtractMode == NArchive::NExtract::NAskMode::kExtract)
        {

            const CArchiveItemInfo* aii = ItemsToExtract[index];
            if (!aii)
            {
                // Already extracted??? Not to be extracted?
                throw S_OK;
            }
            ItemsToExtract.erase(index);
            const CFileData* fd = aii->FileData;
            // fd is certainly not NULL

            // protoze jsme si predali seznam CArchiveItem, kde mame vsechny udaje, co potrebujeme, tak muzeme vybalit.
            // potrebujeme k tomu vsak reverzni mapovani (hash funkce), ktere rekne index polozku, kterou pouzit
            // do teto funkce totiz vchazi index v archivu
            //
            // ale mohli bychom setrit pamet a vytahnout vlastnosti, ktery potrebujeme, pomoci ArchiveHandler->GetProperty
            // jenze problem je s nazvy: GetProperty nam vrati path+filename ke koreni archivu a pokud vybalujeme z jineho
            // korene, museli bychom stripovat cestu

            ProcessedFileInfo.Attributes = fd->Attr;
            ProcessedFileInfo.AttributesAreDefined = true;

            ProcessedFileInfo.IsDirectory = aii->IsDir;
            ProcessedFileInfo.LastWrite = fd->LastWrite;
            ProcessedFileInfo.Size = fd->Size.Value;
            ProcessedFileInfo.Name = aii->NameInArchive;

            // TODO: test na volne misto

            _tcscpy(TargetFileName, TargetDir);
            if (SalamanderGeneral->SalPathAppend(TargetFileName, aii->NameInArchive, MAX_PATH))
            {
                ProcessedFileInfo.FileName = GetUnicodeString(TargetFileName);

                // Show file name in the progress dialog
                SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_ADDTEXT, (LPARAM)GetName());

                if (ProcessedFileInfo.IsDirectory)
                {
                    // vytvorit adresar, pokud jeste neexistuje
                    SalamanderGeneral->CheckAndCreateDirectory(TargetFileName);
                    throw S_OK;
                }
                else
                {
                    // POZOR! Tady se provadi test na overwrite
                    CCreateFileParams cfp;
                    char fileInfo[100];

                    FILETIME ftLW(GetLastWrite());
                    GetInfo(fileInfo, &ftLW, GetSize());
                    cfp.FileInfo = fileInfo;
                    cfp.FileName = GetFileName();
                    cfp.Name = GetName();
                    cfp.pSilent = GetOverwriteSilent();
                    cfp.pSkip = GetOverwriteSkip();

                    if (SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_CREATEFILE, (LPARAM)&cfp))
                    {
                        if (!*GetOverwriteSkip())
                            SetOverwriteCancel();
                    }

                    *outStream = NULL;
                    if (OverwriteSkip)
                        throw S_OK;
                    // celkova operace nemuze pokracovat dal (cancel)
                    if (OverwriteCancel)
                        throw E_ABORT;

                    // otevrit stream pro vybalovani
                    // v tomto miste existuje prazdny soubor s FILE_ATTRIBUTES_NORMAL
                    // je vysledkem testu na overwrite - viz vyse

                    // pokud neexistuje cesta kam rozbalujeme -> vytvorit ji
                    LPTSTR lastComp = _tcsrchr(TargetFileName, '\\');
                    if (lastComp != NULL)
                    {
                        *lastComp = '\0';
                        SalamanderGeneral->CheckAndCreateDirectory(TargetFileName);
                    } // if

                    OutFileStreamSpec = new CRetryableOutFileStream(hProgWnd);
                    CMyComPtr<ISequentialOutStream> outStreamLoc(OutFileStreamSpec);
                    if (!OutFileStreamSpec->Open(GetAnsiString(ProcessedFileInfo.FileName), OPEN_ALWAYS))
                    {
                        SysError(IDS_ERROR, ::GetLastError());
                        NumErrors++;
                        throw S_OK;
                    }
                    OutFileStream = outStreamLoc;
                    *outStream = outStreamLoc.Detach();
                }
            }
            else
            {
                *outStream = NULL;

                char errText[1000];
                _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_NAMEISTOOLONG), (const char*)(aii->NameInArchive), TargetFileName);
                SalamanderGeneral->ShowMessageBox(errText, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            }
        }
        else
        {
            *outStream = NULL;
        }
    }
    catch (HRESULT e)
    {
        ret = e;
    }

    LeaveCriticalSection(&CSExtract);

    return ret;
}

STDMETHODIMP CExtractCallbackImp::PrepareOperation(INT32 askExtractMode)
{
    /*  char u[1024];
  sprintf(u, "CExtractCallbackImp::PrepareOperation: askExtractMode: %d", askExtractMode);
  TRACE_I(u);
*/
    WholeFile = FALSE;
    ExtractMode = false;
    switch (askExtractMode)
    {
    case NArchive::NExtract::NAskMode::kExtract:
        ExtractMode = true;
        //      TRACE_I("CExtractCallbackImp::PrepareOperation: Extract!");
        break;

    case NArchive::NExtract::NAskMode::kTest:
        //      TRACE_I("CExtractCallbackImp::PrepareOperation: Test!");
        break;

    case NArchive::NExtract::NAskMode::kSkip:
        //      TRACE_I("CExtractCallbackImp::PrepareOperation: Skip!");
        break;
    }

    return S_OK;
}

//
// Dat uzivateli na vyber keep nebo delete poskozeneho souboru
//
// TRUE pokud pokracujeme
// FALSE pokud Cancel
BOOL CExtractCallbackImp::OnDataError()
{
    EOperationMode mode = DataErrorMode;

    if (DataErrorMode == Ask)
    {
        TCHAR btnBuffer[1024];
        /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_BTN_DELETE
  {MNTT_IT, IDS_BTN_KEEP
  {MNTT_PE, 0
};
*/
        _stprintf(btnBuffer, _T("%d\t%s\t%d\t%s"),
                  DIALOG_YES, LoadStr(IDS_BTN_DELETE),
                  //      DIALOG_ALL, LoadStr(IDS_BTN_DELETE_ALL),
                  DIALOG_NO, LoadStr(IDS_BTN_KEEP)
                  //      DIALOG_SKIPALL, LoadStr(IDS_BTN_KEEP_ALL)
                  //  DIALOG_CANCEL, LoadStr(IDS_BTN_)
        );
        //buffer: "1\t&Start\t2\tE&xit"

        TCHAR msg[1024];
        _stprintf(msg, LoadStr(PasswordIsDefined ? IDS_ERROR_PROCESSING_FILE_PWD : IDS_ERROR_PROCESSING_FILE),
                  (LPCTSTR)GetAnsiString(ProcessedFileInfo.Name));

        MSGBOXEX_PARAMS mbep;
        ZeroMemory(&mbep, sizeof(mbep));
        mbep.HParent = hProgWnd;
        mbep.Caption = LoadStr(IDS_PLUGINNAME);
        mbep.Text = msg;
        mbep.Flags = MSGBOXEX_ICONEXCLAMATION | MSGBOXEX_YESNOCANCEL;
        mbep.AliasBtnNames = btnBuffer;

        int mbRet = (int)SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_SHOWMBOXEX, (LPARAM)&mbep);

        switch (mbRet)
        {
        case DIALOG_YES:
            mode = Delete;
            break;
        case DIALOG_NO:
            mode = Keep;
            break;
            /*
      case DIALOG_YES: mode = Delete; break;
      case DIALOG_ALL: mode = Delete; DataErrorSilent = TRUE; break;
      case DIALOG_SKIP: mode = Keep; break;
      case DIALOG_SKIPALL: mode = Keep; DataErrorSilent = TRUE; break;
*/
        case DIALOG_CANCEL:
            mode = Cancel;
            break;
        }
    }

    // release OutStreamu, aby sel prip. smazat
    if (OutFileStream != NULL)
        OutFileStreamSpec->SetMTime(&ProcessedFileInfo.LastWrite);
    OutFileStream.Release();

    switch (mode)
    {
    case Cancel:
        SafeDeleteFile(GetAnsiString(ProcessedFileInfo.FileName), DataErrorDeleteSilent);
        return FALSE;

    case Delete:
        // pokud Cancel, tak pryc
        if (!SafeDeleteFile(GetAnsiString(ProcessedFileInfo.FileName), DataErrorDeleteSilent))
            return FALSE;
        break;

    case Keep:
        // nechat soubor, takze mu jeste nastavit atributy
        if (ExtractMode && ProcessedFileInfo.AttributesAreDefined)
            SetFileAttributes(GetAnsiString(ProcessedFileInfo.FileName), ProcessedFileInfo.Attributes);
        break;
    }

    // dal uz se nebudeme ptat, takze mod nastavime na operaci, kterou budeme delat
    if (DataErrorSilent)
        DataErrorMode = mode;

    return TRUE;
}

LRESULT CExtractCallbackImp::Error(int resID, ...)
{
    MSGBOXEX_PARAMS mbep;
    char msg[1024];
    va_list arglist;

    va_start(arglist, resID);
    vsprintf(msg, LoadStr(resID), arglist);
    va_end(arglist);

    ZeroMemory(&mbep, sizeof(mbep));
    mbep.HParent = hProgWnd;
    mbep.Caption = LoadStr(IDS_PLUGINNAME);
    mbep.Text = msg;
    mbep.Flags = MSGBOXEX_ICONHAND | MSGBOXEX_OK;

    return SendMessage(hProgWnd, WM_7ZIP, WM_7ZIP_SHOWMBOXEX, (LPARAM)&mbep);
}

STDMETHODIMP CExtractCallbackImp::SetOperationResult(INT32 resultEOperationResult)
{
    //  TRACE_I("CExtractCallbackImp::SetOperationResult: result = " << resultEOperationResult);

    WholeFile = TRUE;

    switch (resultEOperationResult)
    {
    case NArchive::NExtract::NOperationResult::kOK:
        break;

    default:
        NumErrors++;

        switch (resultEOperationResult)
        {
        case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
            Error(IDS_UNSUPPORTED_METHOD);
            break;

        case NArchive::NExtract::NOperationResult::kCRCError:
            Error(IDS_CRC_FAILED);
            break;

        case NArchive::NExtract::NOperationResult::kDataError:
            //          // tento kod pouzit, pokud nepujde provest patch na funkcnost keep/delete
            //          if (ExtractMode)
            //            Error(IDS_DATA_ERROR, GetAnsiString(ProcessedFileInfo.FileName));

            // tento kod funguje pouze s patchem na funkcnost keep/delete
            if (ExtractMode)
            {
                // dat uzivateli moznost pro keep/delete poskozeneho souboru
                if (!OnDataError())
                    return E_ABORT;
                // doslo k chybe pri vybalovani, vymazeme soubor (provede volani funkce vyse) a koncime
                if (SilentDelete)
                {
                    Error(PasswordIsDefined ? IDS_DATA_ERROR_PWD : IDS_DATA_ERROR, (LPCTSTR)ProcessedFileInfo.Name);
                    if (PasswordIsDefined)
                        Password = L""; // Allow entering another password
                    return E_ABORT;
                }
                return S_OK;
            }

            break;

        default:
            Error(IDS_UNKNOWN_ERROR);
            break;
        }

        break;
    }

    if (OutFileStream != NULL)
        OutFileStreamSpec->SetMTime(&ProcessedFileInfo.LastWrite);
    OutFileStream.Release();

    if (ExtractMode && ProcessedFileInfo.AttributesAreDefined)
        SetFileAttributes(GetAnsiString(ProcessedFileInfo.FileName), ProcessedFileInfo.Attributes);

    if (TargetDir && !ItemsToExtract.size())
    {
        // NULL TargetDir means testing the archive
        // Doing partial extract & everything has been extracted. Looks like a solid archive
        return E_STOPEXTRACTION;
    }

    return S_OK;
}

STDMETHODIMP CExtractCallbackImp::CryptoGetTextPassword(BSTR* password)
{
    if (!PasswordIsDefined /*&& !Silent*/) // Silent je pro skip, kt. neni v 7za.dll implementovany
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
        Silent = false;
        return S_OK;

      case DIALOG_SKIPALL:
        PasswordIsDefined = false;
        Silent = true;
        return S_FALSE;
*/
        }
    }
    StringToBstr(Password, password);

    return S_OK;
}
