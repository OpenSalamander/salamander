// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "isoimage.h"
#include "rawfs.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#define SECTOR_SIZE 0x0800

// ****************************************************************************
//
// CRawFS
//

CRawFS::CRawFS(CISOImage* image, DWORD extent) : CUnISOFSAbstract(image)
{
    ExtentOffset = extent;
}

CRawFS::~CRawFS()
{
}

BOOL CRawFS::ReadBlockPhys(DWORD lbNum, size_t blocks, unsigned char* data)
{
    DWORD len = (DWORD)(blocks * SECTOR_SIZE);

    if (Image->ReadBlock(lbNum, len, data) == len)
        return TRUE;
    else
        return FALSE;
}

BOOL CRawFS::Open(BOOL quiet)
{
    return TRUE;
}

BOOL CRawFS::ListDirectory(char* path, int session, CSalamanderDirectoryAbstract* dir,
                           CPluginDataInterfaceAbstract*& pluginData)
{
    return TRUE;
}

int CRawFS::UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                       const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE7("CRawFS::UnpackFile(, %s, %s, %s, %p, %u, %d)", srcPath, path, nameInArc, fileData, silent, toSkip);

    if (fileData == NULL)
        return UNPACK_ERROR;

    BOOL ret = UNPACK_OK;
    BYTE* sector = NULL;
    try
    {
        CISOImage::CFilePos* fp = (CISOImage::CFilePos*)fileData->PluginData;

        // unpack file
        sector = new BYTE[SECTOR_SIZE];
        if (!sector)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw UNPACK_ERROR;
        }

        char name[MAX_PATH];
        strncpy_s(name, path, _TRUNCATE);
        if (!SalamanderGeneral->SalPathAppend(name, fileData->Name, MAX_PATH))
        {
            Error(IDS_ERR_TOO_LONG_NAME);
            throw UNPACK_ERROR;
        }

        char fileInfo[100];
        FILETIME ft = fileData->LastWrite;
        GetInfo(fileInfo, &ft, fileData->Size);

        DWORD attrs = fileData->Attr;

        HANDLE hFile = SalamanderSafeFile->SafeFileCreate(name, GENERIC_WRITE, FILE_SHARE_READ, attrs, FALSE,
                                                          SalamanderGeneral->GetMainWindowHWND(), nameInArc, fileInfo,
                                                          &silent, TRUE, &toSkip, NULL, 0, NULL, NULL);

        CBufferedFile file(hFile, GENERIC_WRITE);
        // set file time
        file.SetFileTime(&ft, &ft, &ft);

        // the overall operation can continue further: skip only
        if (toSkip)
            throw UNPACK_ERROR;

        // the overall operation cannot continue any further: cancel
        if (hFile == INVALID_HANDLE_VALUE)
            throw UNPACK_CANCEL;

        BOOL bFileComplete = TRUE;
        /*
    BYTE track = Image->GetTrackFromExtent(fp->Extent);
    if (!Image->OpenTrack(track)) {
      // it is necessary to open the track we will read from (setting the track parameters)
      Error(IDS_CANT_OPEN_TRACK, silent == 1, track);
      throw UNPACK_ERROR;
    }
*/

        // it is possible that the extent does not need to be subtracted here; there is no data to test with, so I don't know :(
        // but the more probable variant is that it should be subtracted
        DWORD block = fp->Extent - ExtentOffset;
        CQuadWord remain = fileData->Size;

        DWORD nbytes = SECTOR_SIZE;
        while (remain.Value > 0)
        {
            if (remain.Value < nbytes)
                nbytes = remain.LoDWord; // !!! the buffer size must not exceed DWORD

            if (!ReadBlockPhys(block, 1, sector))
            {
                if (silent == 0)
                {
                    char error[1024];
                    sprintf(error, LoadStr(IDS_ERROR_READING_SECTOR), block);
                    int userAction = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL,
                                                                    fileData->Name, error, LoadStr(IDS_READERROR));

                    switch (userAction)
                    {
                    case DIALOG_CANCEL:
                        ret = UNPACK_CANCEL;
                        break;
                    case DIALOG_SKIP:
                        ret = UNPACK_ERROR;
                        break;
                    case DIALOG_SKIPALL:
                        ret = UNPACK_ERROR;
                        silent = 1;
                        break;
                    }
                }

                bFileComplete = FALSE;
                break;
            }

            if (!salamander->ProgressAddSize(nbytes, TRUE)) // delayedPaint==TRUE, so we do not slow things down
            {
                salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING_OPERATION), FALSE);
                salamander->ProgressEnableCancel(FALSE);

                ret = UNPACK_CANCEL;
                bFileComplete = FALSE;
                break; // action interrupted
            }

            ULONG written;
            if (!file.Write(sector, nbytes, &written, name, NULL))
            {
                // Error message was already displayed by SafeWriteFile()
                ret = UNPACK_CANCEL;
                bFileComplete = FALSE;
                break;
            }
            remain.Value -= nbytes;

            block++;
        } // while

        if (!file.Close(name, NULL))
        {
            // Flushing cache may fail
            ret = UNPACK_CANCEL;
            bFileComplete = FALSE;
        }

        if (!bFileComplete)
        {
            // because it was created with the read-only attribute, we must clear
            // the R attribute so the file can be deleted
            attrs &= ~FILE_ATTRIBUTE_READONLY;
            if (!SetFileAttributes(name, attrs))
                Error(LoadStr(IDS_CANT_SET_ATTRS), GetLastError());

            // the user cancelled the operation
            // delete the incomplete file afterwards
            if (!DeleteFile(name))
                Error(LoadStr(IDS_CANT_DELETE_TEMP_FILE), GetLastError());
        }
        else
            SetFileAttrs(name, attrs);
    }
    catch (int e)
    {
        ret = e;
    }

    delete[] sector;

    return ret;
}

BOOL CRawFS::DumpInfo(FILE* outStream)
{
    return TRUE;
}
