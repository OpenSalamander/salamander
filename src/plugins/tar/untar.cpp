// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dlldefs.h"
#include "fileio.h"
#include "tar.h"
#include "deb/deb.h"

#include "tar.rh"
#include "tar.rh2"
#include "lang\lang.rh"

//
// ****************************************************************************
//
// Helper functions
//
// ****************************************************************************
//

int TxtToShort(const unsigned char* txt, unsigned long& result)
{
    CALL_STACK_MESSAGE2("TxtToShort(%s, )", txt);
    result = 0;
    unsigned long i;
    for (i = 0; i < 4; i++)
    {
        if ((*txt >= '0' && *txt <= '9') || (tolower(*txt) >= 'a' && tolower(*txt) <= 'f'))
        {
            result *= 16;
            if (*txt >= '0' && *txt <= '9')
                result += *txt - '0';
            else
                result += tolower(*txt) - 'a' + 10;
            txt++;
        }
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    return TAR_OK;
}

int TxtToLong(const unsigned char* txt, unsigned long& result)
{
    CALL_STACK_MESSAGE2("TxtToLong(%s, )", txt);
    result = 0;
    unsigned long i;
    for (i = 0; i < 8; i++)
    {
        if ((*txt >= '0' && *txt <= '9') || (tolower(*txt) >= 'a' && tolower(*txt) <= 'f'))
        {
            result *= 16;
            if (*txt >= '0' && *txt <= '9')
                result += *txt - '0';
            else
                result += tolower(*txt) - 'a' + 10;
            txt++;
        }
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    return TAR_OK;
}

int TxtToQuad(const unsigned char* txt, CQuadWord& result)
{
    CALL_STACK_MESSAGE2("TxtToQuad(%s, )", txt);
    result = CQuadWord(0, 0);
    unsigned long i;
    for (i = 0; i < 8; i++)
    {
        if ((*txt >= '0' && *txt <= '9') || (tolower(*txt) >= 'a' && tolower(*txt) <= 'f'))
        {
            result <<= 4;
            if (*txt >= '0' && *txt <= '9')
                result += CQuadWord(*txt - '0', 0);
            else
                result += CQuadWord(tolower(*txt) - 'a' + 10, 0);
            txt++;
        }
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    return TAR_OK;
}

// conversion from the octal numeral system
BOOL FromOctalQ(const unsigned char* ptr, const int length, CQuadWord& result)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("FromOctalQ(, %d,)", length);
    int i = 0;
    while (i < length && isspace(*ptr))
    {
        i++;
        ptr++;
    }
    result.Set(0, 0);
    while (i < length && *ptr >= '0' && *ptr <= '7')
    {
        result.Value = (result.Value << 3) | (*ptr++ - '0');
        i++;
    }
    if (i < length && *ptr && !isspace(*ptr))
        return FALSE;
    return TRUE;
}

//
// ****************************************************************************
//
// SCommonHeader
//
// ****************************************************************************
//
SCommonHeader::SCommonHeader()
{
    Path = NULL;
    ;
    Name = NULL;
    FileInfo.Name = NULL;
    Initialize();
}

SCommonHeader::~SCommonHeader()
{
    Initialize();
}

void SCommonHeader::Initialize()
{
    if (Path != NULL)
        free(Path);
    Path = NULL;
    if (Name != NULL)
        free(Name);
    Name = NULL;
    if (FileInfo.Name != NULL)
        SalamanderGeneral->Free(FileInfo.Name);
    memset(&FileInfo, 0, sizeof(FileInfo));
    IsDir = FALSE;
    Finished = FALSE;
    Ignored = FALSE;
    Checksum.Set(0, 0);
    Mode.Set(0, 0);
    MTime.Set(0, 0);
    Format = e_Unknown;
    IsExtendedTar = FALSE;
}

//
// ****************************************************************************
//
// Public functions
//
// ****************************************************************************
//

CArchive::CArchive(const char* fileName, CSalamanderForOperationsAbstract* salamander, DWORD offset, CQuadWord inputSize)
{
    CALL_STACK_MESSAGE2("CArchive::CArchive(%s, )", fileName);

    // initialization
    Offset.Set(0, 0);
    Silent = 0;
    Ok = TRUE;
    Stream = NULL;
    SalamanderIf = salamander;
    if (fileName == NULL || salamander == NULL)
    {
        Ok = FALSE;
        return;
    }
    // open the input file and determine whether it is compressed
    Stream = CDecompressFile::CreateInstance(fileName, offset, inputSize);
    // verify the stream
    if (Stream == NULL || !Stream->IsOk())
    {
        Ok = FALSE;
        return;
    }
}

CArchive::~CArchive()
{
    CALL_STACK_MESSAGE1("CArchive::~CArchive()");
    // we can now close the archive
    if (Stream != NULL)
        delete Stream;
}

BOOL CArchive::ListArchive(const char* prefix, CSalamanderDirectoryAbstract* dir)
{
    CALL_STACK_MESSAGE1("CArchive::ListArchive( )");

    if (!IsOk())
        return FALSE;

    // first try to detect the archive and read the first header
    Silent = 0;
    Offset.Set(0, 0);
    SCommonHeader header;
    int ret = ReadArchiveHeader(header, TRUE);

    // if this is not a supported format, unpack only the outer compression when present
    if (ret == TAR_NOTAR)
        if (Stream->IsCompressed())
            return ListStream(dir);
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_UNKNOWN), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return FALSE;
        }

    if (ret != TAR_OK)
        return FALSE;

    if (header.Finished)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_NOTFOUND), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }

    // we have an archive, so proceed - decode all files from the archive
    for (;;)
    {
        // ignore entries we cannot interpret
        if (!header.Ignored)
        {
            char path[2 * MAX_PATH];

            if (prefix)
            {
                strcpy(path, prefix);
                if (header.Path)
                    strcat(path, header.Path);
            }
            // add either a new file or a directory
            if (!header.IsDir)
            {
                // TODO: also add user data with the file position inside the archive to separate identically named files
                // this is a file, add the file
                if (!dir->AddFile(prefix ? path : header.Path, header.FileInfo, NULL))
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_FDATA), LoadStr(IDS_TARERR_TITLE),
                                                      MSGBOX_ERROR);
                    return FALSE;
                }
            }
            else
            {
                // TODO: also add user data with the file position inside the archive to separate identically named files
                // this is a directory, add the directory
                if (!dir->AddDir(prefix ? path : header.Path, header.FileInfo, NULL))
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_FDATA), LoadStr(IDS_TARERR_TITLE),
                                                      MSGBOX_ERROR);
                    return FALSE;
                }
            }
            // set to null so we do not accidentally free the passed memory
            header.FileInfo.Name = NULL;
        }

        // read the data so that we advance to the next file
        ret = WriteOutData(header, NULL, NULL, TRUE, FALSE);

        if (ret != TAR_OK)
        {
            // Patera 2004.03.02: Return TRUE if TAR file ended exactly at the end
            // of last stream
            return (ret == TAR_EOF) ? TRUE : FALSE;
        }

        // prepare a new header for the next iteration
        if (ReadArchiveHeader(header, FALSE) != TAR_OK)
            return FALSE;

        // reached the end of the archive; finish appropriately
        if (header.Finished)
            return TRUE;
    }
}

BOOL CArchive::UnpackOneFile(const char* nameInArchive, const CFileData* fileData,
                             const char* targetPath, const char* newFileName)
{
    CALL_STACK_MESSAGE4("CArchive::UnpackOneFile(%s, , %s, %s)", nameInArchive, targetPath, newFileName);

    if (!IsOk())
        return FALSE;

    // first try to detect the archive and read the first header
    Silent = 0;
    Offset.Set(0, 0);
    SCommonHeader header;
    int ret = ReadArchiveHeader(header, TRUE);
    // if this is not a supported format, unpack only the outer compression when present
    if (ret == TAR_NOTAR && Stream->IsCompressed())
        return UnpackStream(targetPath, FALSE, nameInArchive, NULL, newFileName);
    if (ret != TAR_OK)
        return FALSE;
    if (header.Finished)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_NOTFOUND), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    BOOL found = FALSE;
    // we have an archive, so proceed - decode all files from the archive
    for (;;)
    {
        found = !strcmp(header.Name, nameInArchive);
        // if we cannot interpret what we found, skip it; otherwise extract the file
        // What we don't support (everything except files & dirs) has zero size and Ignored set.
        // This new condition unlike the old one also extracts empty files.
        ret = WriteOutData(header, targetPath, newFileName ? newFileName : header.FileInfo.Name,
                           (header.Ignored || !found), FALSE);
        if (ret != TAR_OK)
        {
            if (ret == TAR_EOF)
            {
                if (found)
                    return TRUE;

                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            }
            return FALSE;
        }
        // if we found it, we can finish
        if (found)
            return TRUE;
        // prepare a new header for the next iteration
        if (ReadArchiveHeader(header, FALSE) != TAR_OK)
            return FALSE;
        // reached the end of the archive; finish appropriately
        if (header.Finished)
            return FALSE;
    }
}

// extraction of selected files
BOOL CArchive::UnpackArchive(const char* targetPath, const char* archiveRoot,
                             SalEnumSelection next, void* param)
{
    CALL_STACK_MESSAGE3("CArchive::UnpackArchive(%s, %s, , )", targetPath, archiveRoot);

    if (!IsOk())
        return FALSE;

    // initialize the names to extract
    const char* curName;
    BOOL isDir;
    CQuadWord size;
    CQuadWord totalSize(0, 0);
    CNames names;
    while ((curName = next(NULL, 0, &isDir, &size, NULL, param, NULL)) != NULL)
    {
        if (archiveRoot != NULL && *archiveRoot != '\0')
        {
            char* tmpName = (char*)malloc(strlen(archiveRoot) + 1 + strlen(curName) + 1);
            if (tmpName == NULL)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                                  MSGBOX_ERROR);
                return FALSE;
            }
            strcpy(tmpName, archiveRoot);
            if (tmpName[strlen(tmpName) - 1] != '\\')
                strcat(tmpName, "\\");
            strcat(tmpName, curName);
            names.AddName(tmpName, isDir, NULL, NULL);
            free(tmpName);
        }
        else
            names.AddName(curName, isDir, NULL, NULL);
        totalSize = totalSize + size;
    }
    // check free space; assume TestFreeSpace displays an appropriate message
    if (!SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                          targetPath, totalSize, LoadStr(IDS_TARERR_HEADER)))
        return FALSE;

    // perform the actual extraction by the collected names
    return DoUnpackArchive(targetPath, archiveRoot, names);
}

BOOL CArchive::UnpackWholeArchive(const char* mask, const char* targetPath)
{
    CALL_STACK_MESSAGE3("CArchive::UnpackWholeArchive(%s, %s)", mask, targetPath);

    if (!IsOk())
        return FALSE;

    // initialize the list of names to extract according to the provided mask list
    CNames names;
    char* tmp = (char*)malloc(strlen(mask) + 1);
    if (tmp == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                          MSGBOX_ERROR);
        return TAR_ERROR;
    }
    strcpy(tmp, mask);
    char* ptr = tmp + strlen(tmp) - 1;
    for (;;)
    {
        while (ptr > tmp && *ptr != ';')
            ptr--;
        if (*ptr == ';')
        {
            if (strlen(ptr + 1) > 0)
                names.AddName(ptr + 1, FALSE, NULL, NULL);
            *ptr = '\0';
            ptr--;
        }
        else if (strlen(ptr) > 0)
            names.AddName(ptr, FALSE, NULL, NULL);
        if (ptr <= tmp)
            break;
    }
    free(tmp);

    // and now perform the actual extraction
    return DoUnpackArchive(targetPath, NULL, names);
}

//
// ****************************************************************************
//
// Internal functions
//
// ****************************************************************************
//

BOOL CArchive::DoUnpackArchive(const char* targetPath, const char* archiveRoot, CNames& names)
{
    CALL_STACK_MESSAGE3("CArchive::DoUnpackArchive(%s, %s, )", targetPath, archiveRoot);

    if (!IsOk())
        return FALSE;

    CQuadWord filePos, sizeDelta;
    filePos = Stream->GetStreamPos();
    // first try to detect the archive and read the first header
    Silent = 0;
    Offset.Set(0, 0);
    SCommonHeader header;
    int ret = ReadArchiveHeader(header, TRUE);
    // if this is not a supported format, unpack only the outer compression when present
    if (ret == TAR_NOTAR && Stream->IsCompressed())
        return UnpackStream(targetPath, TRUE, NULL, &names, NULL);
    if (ret != TAR_OK)
        return FALSE;
    if (header.Finished)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_NOTFOUND), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    // open the progress dialog
    SalamanderIf->OpenProgressDialog(LoadStr(IDS_UNPACKPROGRESS_TITLE), FALSE, NULL, FALSE);
    SalamanderIf->ProgressSetTotalSize(Stream->GetStreamSize(), CQuadWord(-1, -1));
    // update the progress after reading the header
    sizeDelta = filePos;
    filePos = Stream->GetStreamPos();
    sizeDelta = filePos - sizeDelta;
    if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
    {
        // handle cancel
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    // we have an archive, so proceed - decode all files from the archive
    for (;;)
    {
        // determine whether this is "our" file
        BOOL found = names.IsNamePresent(header.Name);
        // what should be the name of the created file? (relative path)
        char* ptr = header.Name;
        if (found && !header.Ignored && archiveRoot != NULL)
            ptr += strlen(archiveRoot);

        // if we cannot interpret what we found, skip it; otherwise extract the file
        // What we don't support (everything except files & dirs) has zero size and Ignored set.
        // This new condition unlike the old one also extracts empty files.
        ret = WriteOutData(header, targetPath, ptr, (header.Ignored || !found), TRUE);
        if (ret != TAR_OK)
        {
            SalamanderIf->CloseProgressDialog();
            return (ret == TAR_EOF) ? TRUE : FALSE;
        }

        // progress was advanced during extraction, just acknowledge it here
        filePos = Stream->GetStreamPos();
        // prepare a new header for the next iteration
        if (ReadArchiveHeader(header, FALSE) != TAR_OK)
        {
            SalamanderIf->CloseProgressDialog();
            return FALSE;
        }
        // advance the progress by the size of the read header
        sizeDelta = filePos;
        filePos = Stream->GetStreamPos();
        sizeDelta = filePos - sizeDelta;
        if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
        {
            // handle cancel
            SalamanderIf->CloseProgressDialog();
            return FALSE;
        }
        // reached the end of the archive, stop
        if (header.Finished)
        {
            SalamanderIf->CloseProgressDialog();
            return TRUE;
        }
    }
}

void CArchive::MakeFileInfo(const SCommonHeader& header, char* arcfiledata, char* arcfilename)
{
    CALL_STACK_MESSAGE1("CArchive::MakeFileInfo( , , )");

    if (header.IsDir)
    {
        // this does not matter for directories
        arcfiledata[0] = '\0';
        arcfilename[0] = '\0';
    }
    else
    {
        // filedata
        FILETIME ft;
        FileTimeToLocalFileTime(&header.FileInfo.LastWrite, &ft);
        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);
        char date[50], time[50], number[50];
        if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
            sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
            sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        sprintf(arcfiledata, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, header.FileInfo.Size), date, time);
        // filename
        sprintf(arcfilename, "%s\\%s", Stream->GetArchiveName(), header.Name);
    }
}

int CArchive::WriteOutData(const SCommonHeader& header, const char* targetPath,
                           const char* targetName, BOOL simulate, BOOL doProgress)
{
    SLOW_CALL_STACK_MESSAGE5("CArchive::WriteOutData( , %s, %s, %d, %d)",
                             targetPath, targetName, simulate, doProgress);

    BOOL toSkip = TRUE;
    char* extractedName;
    HANDLE file;
    if (!simulate || doProgress)
    {
        // construct the name of the extracted item
        extractedName = (char*)malloc(strlen(targetPath) + 1 + strlen(targetName) + 1);
        if (extractedName == NULL)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
        strcpy(extractedName, targetPath);
        if (extractedName[strlen(extractedName) - 1] != '\\')
            strcat(extractedName, "\\");
        strcat(extractedName, (targetName[0] == '\\' ? 1 : 0) + targetName);
        // create the new file
        char arcfiledata[500];
        char arcfilename[500];
        MakeFileInfo(header, arcfiledata, arcfilename);
        if (!simulate)
        {
            file = SalamanderSafeFile->SafeFileCreate(extractedName, GENERIC_WRITE, 0, FILE_ATTRIBUTE_NORMAL,
                                                      header.IsDir, SalamanderGeneral->GetMainWindowHWND(),
                                                      arcfilename, arcfiledata, &Silent, TRUE, &toSkip, NULL, 0, NULL, NULL);
            // abort on any problem
            if (file == INVALID_HANDLE_VALUE)
            {
                if (toSkip)
                    simulate = TRUE;
                else
                {
                    free(extractedName);
                    return TAR_ERROR;
                }
            }
        }
        // update the file name in the progress dialog
        if (doProgress)
        {
            char progresstxt[1000];
            if (!toSkip)
                strcpy(progresstxt, LoadStr(IDS_UNPACKPROGRESS_TEXT));
            else
                strcpy(progresstxt, LoadStr(IDS_SKIPPROGRESS_TEXT));
            strcat(progresstxt, header.Name);
            SalamanderIf->ProgressDialogAddText(progresstxt, TRUE);
        }
    }
    unsigned long checksum = 0;
    CQuadWord size(0, 0);
    CQuadWord filePos, sizeDelta;
    filePos = Stream->GetStreamPos();
    // if this is a tar archive with an extended header, process the extensions
    // TODO: if the extended header contains a long name we must handle it
    //   while reading the header; only sparse files should remain here
    // TODO: in general extend tar type detection too
    if (header.IsExtendedTar)
    {
        // TODO: this still needs more thorough handling
        const TTarBlock* tarHeader;
        do
        {
            // read the next block
            tarHeader = (const TTarBlock*)Stream->GetBlock(BLOCKSIZE);
            if (tarHeader == NULL)
            {
                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                if (!simulate)
                {
                    CloseHandle(file);
                    DeleteFile(extractedName);
                    free(extractedName);
                }
                return TAR_ERROR;
            }
            EArchiveFormat format;
            BOOL finished;
            if (!IsTarHeader(tarHeader->RawBlock, finished, format))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                if (!simulate)
                {
                    CloseHandle(file);
                    DeleteFile(extractedName);
                    free(extractedName);
                }
                return TAR_ERROR;
            }
            if (!simulate)
            {
                // TODO: process extended headers according to their specific type here
                /*
        DWORD written;
        if (!WriteFile(file, buffer, toRead, &written, NULL) || written != toRead)
        {
          char message[1000];
          DWORD err = GetLastError();
          strcpy(message, LoadStr(IDS_TARERR_FWRITE));
          if (written != toRead)
            strcat(message, LoadStr(IDS_TARERR_WRSIZE));
          else
            strcat(message, SalamanderGeneral->GetErrorText(err));
          SalamanderGeneral->ShowMessageBox(message, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
          CloseHandle(file);
          DeleteFile(extractedName);
          free(extractedName);
          return TAR_ERROR;
        }
        */
            }
            // update the progress if necessary
            if (doProgress)
            {
                sizeDelta = filePos;
                filePos = Stream->GetStreamPos();
                sizeDelta = filePos - sizeDelta;
                // advance the progress and handle cancellation
                if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
                {
                    // handle cancellation
                    if (!simulate)
                    {
                        CloseHandle(file);
                        DeleteFile(extractedName);
                        free(extractedName);
                    }
                    return TAR_ERROR;
                }
            }
            size += CQuadWord(BLOCKSIZE, 0);
            Offset += CQuadWord(BLOCKSIZE, 0);
        } while (tarHeader->SparseHeader.isextended);
    }
    // TODO: verify once more how the size should be processed here
    size.Set(0, 0);
    while (size < header.FileInfo.Size)
    {
        unsigned short toRead = (unsigned short)(header.FileInfo.Size - size > CQuadWord(BUFSIZE, 0) ? BUFSIZE : (header.FileInfo.Size - size).Value);
        const unsigned char* buffer = Stream->GetBlock(toRead);

        if (buffer == NULL)
        {
            if (!Stream->IsOk())
                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            else
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_EOF), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            if (!simulate)
            {
                CloseHandle(file);
                DeleteFile(extractedName);
                free(extractedName);
            }
            return TAR_ERROR;
        }
        if (!simulate)
        {
            DWORD written;
            if (!WriteFile(file, buffer, toRead, &written, NULL) || written != toRead)
            {
                char message[1000];
                strcpy(message, LoadStr(IDS_TARERR_FWRITE));
                if (written != toRead)
                    strcat(message, LoadStr(IDS_TARERR_WRSIZE));
                else
                    strcat(message, SalamanderGeneral->GetErrorText(GetLastError()));
                SalamanderGeneral->ShowMessageBox(message, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                CloseHandle(file);
                DeleteFile(extractedName);
                free(extractedName);
                return TAR_ERROR;
            }
        }
        // update the progress if necessary
        if (doProgress)
        {
            sizeDelta = filePos;
            filePos = Stream->GetStreamPos();
            sizeDelta = filePos - sizeDelta;
            // advance the progress and handle cancellation
            if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
            {
                // handle cancellation
                if (!simulate)
                {
                    CloseHandle(file);
                    DeleteFile(extractedName);
                    free(extractedName);
                }
                return TAR_ERROR;
            }
        }
        // compute the checksum when needed
        if (header.Format == e_CRCASCII)
        {
            unsigned long k;
            for (k = 0; k < toRead; k++)
                checksum += buffer[k];
        }
        size += CQuadWord(toRead, 0);
        Offset += CQuadWord(toRead, 0);
    }
    // if possible, verify the checksums
    if (header.Format == e_CRCASCII && !header.Ignored &&
        CQuadWord(checksum, 0) != header.Checksum)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_CHECKSUM), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        if (!simulate)
        {
            CloseHandle(file);
            DeleteFile(extractedName);
            free(extractedName);
        }
        return TAR_ERROR;
    }
    // set the properties of the created file
    if (!simulate)
    {
        if (!header.IsDir)
        {
            SetFileTime(file, &header.FileInfo.LastWrite, &header.FileInfo.LastWrite, &header.FileInfo.LastWrite);
            if (!CloseHandle(file))
            {
                char buffer[1000];
                DWORD err = GetLastError();
                strcpy(buffer, LoadStr(IDS_TARERR_FWRITE));
                strcat(buffer, SalamanderGeneral->GetErrorText(err));
                SalamanderGeneral->ShowMessageBox(buffer, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                DeleteFile(extractedName);
                free(extractedName);
                return TAR_ERROR;
            }
        }
        SetFileAttributes(extractedName, header.FileInfo.Attr);
    }
    // the name is no longer needed
    if (!simulate || doProgress)
        free(extractedName);

    // align the input to whole blocks
    int ret = SkipBlockPadding(header);

    // update the progress if necessary
    if (doProgress)
    {
        sizeDelta = filePos;
        filePos = Stream->GetStreamPos();
        sizeDelta = filePos - sizeDelta;
        // advance the progress and handle cancel
        if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
            return TAR_ERROR;
    }

    if (ret != TAR_OK)
    {
        if (ret == TAR_EOF)
        {
            // Patera 2004.03.02:
            // We're at the end of the TAR file just at the end of the last stream.
            // Such files are created by bugreporting system of Kerio Personal Firewall
        }
        else if (ret == TAR_PREMATURE_END)
        {
            // all other errors reported inside SkipBlockPadding()
            SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                              LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    return ret;
} /* CArchive::WriteOutData */

// skip the unused remainder of the block to reach the next valid one
int CArchive::SkipBlockPadding(const SCommonHeader& header)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CArchive::SkipBlockPadding( )");
    unsigned short pad;

    if (header.Format == e_CRCASCII || header.Format == e_NewASCII)
        pad = (unsigned short)((4 - (Offset.Value % 4)) % 4);
    else if (header.Format == e_Binary || header.Format == e_SwapBinary)
        pad = (unsigned short)((2 - (Offset.Value % 2)) % 2);
    else if (header.Format == e_TarPosix ||
             header.Format == e_TarOldGnu ||
             header.Format == e_TarV7)
        pad = (unsigned short)((BLOCKSIZE - (Offset.Value % BLOCKSIZE)) % BLOCKSIZE);
    else
        pad = 0;

    if (pad != 0)
    {
        unsigned short read;
        const unsigned char* buffer = Stream->GetBlock(pad, &read);

        if (buffer == NULL)
        {
            // TAR_EOF indicates there is nowhere to seek at all
            // TAR_PREMATURE_END indicates there were some bytes to skip but less than expected
            return read ? TAR_PREMATURE_END : TAR_EOF;
        }
        Offset += CQuadWord(pad, 0);
    }
    return TAR_OK;
} /* CArchive::SkipBlockPadding */

// reads a block header in the archive (with type auto-detection)
int CArchive::ReadArchiveHeader(SCommonHeader& header, BOOL probe)
{
    SLOW_CALL_STACK_MESSAGE2("CArchive::ReadArchiveHeader(, %d)", probe);

    // clear the structure right away
    header.Initialize();
    // try to read the maximum size - the tar header
    unsigned short preRead = BLOCKSIZE;
    const unsigned char* buffer = Stream->GetBlock(preRead);

    if (buffer == NULL)
    {
        // apparently we do not have enough data; settle for the size of the cpio "magic" number
        if (Stream->IsOk())
        {
            unsigned short available = 0;
            preRead = 6;

            buffer = Stream->GetBlock(preRead, &available);
            if (available < 6)
            {
                preRead = available;
                buffer = Stream->GetBlock(available);
                if (buffer)
                {
                    memset((char*)buffer + available, 0, 6 - available);
                }
            }
            if (!available)
            {
                // Patera 2005.04.28: Fix of bug #183 (phphomework-alpha2.tar.gz)
                // Not able to read anything: try some heuristics
                if ((Stream->GetStreamSize() == Stream->GetStreamPos()) && (Stream->GetStreamSize() > CQuadWord(0, 0)))
                {
                    // TAR: parsed everything; looks like the terminating empty block is missing
                    header.Finished = TRUE;
                    return TAR_OK;
                }
                FILETIME lastWrite;
                CQuadWord fileSize;
                DWORD fileAttr;
                Stream->GetFileInfo(lastWrite, fileSize, fileAttr);
                if (fileSize == Offset)
                {
                    // TAR+GZIP: decompressed as much data as stated at the end
                    header.Finished = TRUE;
                    return TAR_OK;
                }
            }
        }
        // if we cannot read even that, it is time to give up
        if (buffer == NULL)
        {
            if (!Stream->IsOk())
                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            else
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_EOF),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }

    // determine the archive type (after this step the header structure contains a valid Format and, for tar, Finished)
    GetArchiveType(buffer, preRead, header);

    // File starting with 512 zeros is automatically recognized by IsTarHeader as finished e_TarPosix.
    // Unless the TAR contains just one empty sector, it is not a TAR file.
    if (probe && (header.Format == e_TarPosix) && header.Finished)
        header.Format = e_Unknown;

    // unknown archive - abort processing
    if (header.Format == e_Unknown)
    {
        // return the data we should not consume
        Stream->Rewind(preRead);
        if (!Stream->IsOk())
        {
            SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                              LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        // and exit
        if (!probe)
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_UNKNOWN), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_NOTAR;
    }
    // if the tar archive has ended, stop processing
    if (header.Finished)
        return TAR_OK;
    // for non-tar formats we must return the rest of the header
    if (header.Format != e_TarPosix &&
        header.Format != e_TarOldGnu &&
        header.Format != e_TarV7)
        Stream->Rewind(preRead - 6);

    // and read the header
    int ret;
    switch (header.Format)
    {
    case e_NewASCII:
    case e_CRCASCII:
        ret = ReadNewASCIIHeader(header);
        break;
    case e_OldASCII:
        ret = ReadOldASCIIHeader(header);
        break;
    case e_Binary:
    case e_SwapBinary:
        ret = ReadBinaryHeader(header);
        break;
    case e_TarPosix:
    case e_TarOldGnu:
    case e_TarV7:
        ret = ReadTarHeader(buffer, header);
        break;
    default:
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_FORMAT), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        ret = TAR_ERROR;
    }
    // abort when an error occurs
    if (ret != TAR_OK)
        return ret;
    // align the block
    ret = SkipBlockPadding(header);
    if (ret != TAR_OK)
    {
        if ((ret == TAR_PREMATURE_END) || (ret == TAR_EOF))
        {
            SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                              LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        return ret;
    }

    // tar already handled end-of-archive detection; for cpio wait until the name is known
    if (header.Format != e_TarPosix && header.Format != e_TarOldGnu &&
        header.Format != e_TarV7 && header.Name != NULL &&
        !strcmp((char*)header.Name, "TRAILER!!!"))
    {
        header.Finished = TRUE;
        return TAR_OK;
    }
    // normalize the path: remove "." and ".." and convert everything to backslashes
    char* tmpName = (char*)malloc(strlen(header.Name) + 1);
    if (tmpName == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                          MSGBOX_ERROR);
        return TAR_ERROR;
    }
    const char* src = header.Name;
    char* dst = tmpName;
    while (*src != '\0')
    {
        switch (*src)
        {
        case '\\':
        case '/':
            // when we encounter a slash, copy it if it is not the first path character and we do not have one already
            if (dst > tmpName && *(dst - 1) != '\\')
                *dst++ = '\\';
            src++;
            break;
        case '.':
            if (*(src + 1) == '.' && (*(src + 2) == '\\' || *(src + 2) == '/' || *(src + 2) == '\0'))
            {
                char* dstOld = dst;

                // two dots - drop one directory level when possible
                if (dst > tmpName)
                    dst -= 2;
                while ((dst > tmpName) && (*dst != '\\'))
                    dst--;
                // testAIX.tar from martin.pobitzer@gmx.at created on AIX starts with path "../../empty"
                if (dst < dstOld)
                {
                    dst++;
                }
                src += 2;
                break;
            }
            else if (*(src + 1) == '\\' || *(src + 1) == '/' || *(src + 1) == '\0')
            {
                // a single dot - simply ignore it
                src++;
                break;
            }
            // otherwise it is the start of the name; fall through to copying
        default:
            // copy characters until the next slash
            while (*src != '\0' && *src != '\\' && *src != '/')
                *dst++ = *src++;
            break;
        }
    }
    // terminate the destination string
    *dst = '\0';
    // skip invalid names
    if (tmpName[0] == '\0')
    {
        free(tmpName);
        header.Ignored = TRUE;
        return TAR_OK;
    }
    // skip the trailing slash but remember it was there
    if (dst > tmpName && *(dst - 1) == '\\')
    {
        header.IsDir = TRUE;
        dst--;
        *dst = '\0';
    }

    // store the new name instead of the old one
    free(header.Name);
    header.Name = (char*)malloc(strlen(tmpName) + 1);
    if (header.Name == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                          MSGBOX_ERROR);
        free(tmpName);
        return TAR_ERROR;
    }
    strcpy(header.Name, tmpName);
    free(tmpName);
    // now analyze the name
    const char* ptr = header.Name + strlen(header.Name) - 1;
    // find the next separator - split name and path
    while (ptr > header.Name && *ptr != '\\')
        ptr--;
    if (ptr == header.Name)
    {
        // nothing found, we only have the name
        header.Path = NULL;
        header.FileInfo.NameLen = strlen(header.Name);
        header.FileInfo.Name = SalamanderGeneral->DupStr(header.Name);
        if (header.FileInfo.Name == NULL)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    else
    {
        // separator found; copy both the name and the path
        header.Path = (char*)malloc(ptr - header.Name + 1);
        if (header.Path == NULL)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
        strncpy_s(header.Path, ptr - header.Name + 1, header.Name, _TRUNCATE);
        ptr++;
        header.FileInfo.NameLen = strlen(ptr);
        header.FileInfo.Name = SalamanderGeneral->DupStr(ptr);
        if (header.FileInfo.Name == NULL)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    // determine the extension
    int sortByExtDirsAsFiles;
    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                          sizeof(sortByExtDirsAsFiles), NULL);
    if (sortByExtDirsAsFiles || !header.IsDir)
    {
        char* s = header.FileInfo.Name + header.FileInfo.NameLen - 1;
        while (s >= header.FileInfo.Name && *s != '.')
            s--;
        //    if (s > header.FileInfo.Name)   // ".cvspass" in Windows counts as an extension...
        if (s >= header.FileInfo.Name)
            header.FileInfo.Ext = s + 1;
        else
            header.FileInfo.Ext = header.FileInfo.Name + header.FileInfo.NameLen;
    }
    else
        header.FileInfo.Ext = header.FileInfo.Name + header.FileInfo.NameLen; // directories have no extension
    // convert file information from UNIX to Windows format
    // date and time
    header.MTime.Value += 11644473600;      // zero corresponds to 1 Jan 1601
    header.MTime.Value *= 1000 * 1000 * 10; // and convert to 100-nanosecond units
    header.FileInfo.LastWrite.dwLowDateTime = header.MTime.LoDWord;
    header.FileInfo.LastWrite.dwHighDateTime = header.MTime.HiDWord;
    // we already have the type from tar, now handle cpio
    if (header.Format != e_TarPosix && header.Format != e_TarOldGnu &&
        header.Format != e_TarV7)
        if ((header.Mode.LoDWord & CP_IFMT) == CP_IFDIR)
            header.IsDir = TRUE;
        else if ((header.Mode.LoDWord & CP_IFMT) != CP_IFREG)
            header.Ignored = TRUE;
    // attributes
    header.FileInfo.Attr = FILE_ATTRIBUTE_ARCHIVE;
    if ((header.Mode.LoDWord & 0200) == 0)
        header.FileInfo.Attr |= FILE_ATTRIBUTE_READONLY;
    if (header.IsDir)
        header.FileInfo.Attr |= FILE_ATTRIBUTE_DIRECTORY;
    // and the rest
    header.FileInfo.DosName = NULL;
    header.FileInfo.Hidden = header.FileInfo.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
    if (header.IsDir)
        header.FileInfo.IsLink = 0;
    else
        header.FileInfo.IsLink = SalamanderGeneral->IsFileLink(header.FileInfo.Ext);
    header.FileInfo.IsOffline = 0;
    // TODO: store an unambiguous file identifier in PluginData, e.g. the offset...
    header.FileInfo.PluginData = -1; // unnecessary, just for formality
    return TAR_OK;
} /* CArchive::ReadArchiveHeader */

// auto-detect the block type in the archive based on the header contents
void CArchive::GetArchiveType(const unsigned char* buffer, const unsigned short preRead, SCommonHeader& header)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CArchive::GetArchiveType(, %hu, )", preRead);

    header.Finished = FALSE;
    // compare the signature in the buffer with known patterns
    if (!strncmp((const char*)buffer, "070701", 6))
        header.Format = e_NewASCII;
    else if (!strncmp((const char*)buffer, "070702", 6))
        header.Format = e_CRCASCII;
    else if (!strncmp((const char*)buffer, "070707", 6))
        header.Format = e_OldASCII;
    else if ((*(unsigned short*)buffer == 070707) && (preRead >= 26))
        header.Format = e_Binary;
    else if (*(unsigned short*)buffer == 0707070)
        header.Format = e_SwapBinary;
    else if ((preRead >= BLOCKSIZE) &&
             IsTarHeader(buffer, header.Finished, header.Format))
        ;
    else
        header.Format = e_Unknown;
}

// read the file name from the cpio header
int CArchive::ReadCpioName(unsigned long namesize, SCommonHeader& header)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CArchive::ReadCpioName(%lu, )", namesize);
    // sanity-check the name length
    if (namesize > 10000)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    // allocate space for the name
    header.Name = (char*)malloc(namesize);
    if (header.Name == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    // read the name
    const unsigned char* buffer = Stream->GetBlock((unsigned short)namesize);
    if (buffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    // ensure the length matches
    if (buffer[namesize - 1] != '\0')
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    // update the offset for alignment
    Offset += CQuadWord(namesize, 0);
    // copy the name into the structure
    strcpy(header.Name, (const char*)buffer);
    // and finish
    return TAR_OK;
}

int CArchive::ReadNewASCIIHeader(SCommonHeader& header)
{
    CALL_STACK_MESSAGE1("CArchive::ReadNewASCIIHeader( )");

    // read the remainder of the header
    const unsigned char* buffer = Stream->GetBlock(13 * 8);
    if (buffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }

    // update the offset for alignment
    Offset += CQuadWord(6 + 13 * 8, 0);
    // extract the name length
    unsigned long namesize;
    if (TxtToLong(buffer + 11 * 8, namesize) != TAR_OK)
        return TAR_ERROR;
    // and extract the file details we care about
    if (TxtToQuad(buffer + 1 * 8, header.Mode) != TAR_OK)
        return TAR_ERROR;
    if (TxtToQuad(buffer + 5 * 8, header.MTime) != TAR_OK)
        return TAR_ERROR;
    if (TxtToQuad(buffer + 6 * 8, header.FileInfo.Size) != TAR_OK)
        return TAR_ERROR;
    if (TxtToQuad(buffer + 12 * 8, header.Checksum) != TAR_OK)
        return TAR_ERROR;
    // read the name
    return ReadCpioName(namesize, header);
}

int CArchive::ReadOldASCIIHeader(SCommonHeader& header)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CArchive::ReadOldASCIIHeader( )");
    // read the remainder of the header
    const unsigned char* buffer = Stream->GetBlock(70);
    if (buffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }

    // update the offset for alignment
    Offset += CQuadWord(6 + 70, 0);
    // extract the name length
    CQuadWord qnamesize;
    if (!FromOctalQ(buffer + 6 * 7 + 11, 6, qnamesize))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    unsigned long namesize;
    namesize = qnamesize.LoDWord;
    // and extract the file details we care about
    if (!FromOctalQ(buffer + 6 * 2, 6, header.Mode))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    if (!FromOctalQ(buffer + 6 * 7, 11, header.MTime))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    if (!FromOctalQ(buffer + 6 * 8 + 11, 11, header.FileInfo.Size))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    header.Checksum.Set(0, 0);

    // read the name
    return ReadCpioName(namesize, header);
}

int CArchive::ReadBinaryHeader(SCommonHeader& header)
{
    CALL_STACK_MESSAGE1("CArchive::ReadBinaryHeader( )");
    // read the remainder of the header
    const unsigned char* buffer = Stream->GetBlock(20);
    if (buffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }

    // update the offset for alignment
    Offset += CQuadWord(6 + 20, 0);
    unsigned long namesize;
    if (header.Format == e_Binary)
    {
        // extract the name length
        namesize = *(unsigned short*)(buffer + 2 * 7);
        header.Mode.Set(*(unsigned short*)(buffer + 2 * 0), 0);
        header.MTime.Set((*(unsigned short*)(buffer + 2 * 5) << 16) + *(unsigned short*)(buffer + 2 * 6), 0);
        header.FileInfo.Size.Set((*(unsigned short*)(buffer + 2 * 8) << 16) + *(unsigned short*)(buffer + 2 * 9), 0);
    }
    else
    {
        // extract the name length
        if (!TxtToShort(buffer + 2 * 7, namesize))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        // and extract the file details we care about
        unsigned long mode;
        if (!TxtToShort(buffer + 2 * 0, mode))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        header.Mode.Set(mode, 0);
        if (!TxtToQuad(buffer + 2 * 5, header.MTime))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        if (!TxtToQuad(buffer + 2 * 8, header.FileInfo.Size))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    header.Checksum.Set(0, 0);

    // read the name
    return ReadCpioName(namesize, header);
}

// detect a valid TAR header
BOOL CArchive::IsTarHeader(const unsigned char* buffer, BOOL& finished, EArchiveFormat& format)
{
    SLOW_CALL_STACK_MESSAGE1("CArchive::IsTarHeader(, )");
    const TTarBlock* header = (const TTarBlock*)buffer;
    finished = FALSE;

    // check the checksum (both signed and unsigned; the format is messy)
    long int signed_checksum = 0;
    long int unsigned_checksum = 0;
    int i;
    for (i = 0; i < sizeof(TPosixHeader); i++)
    {
        signed_checksum += (signed char)(header->RawBlock[i]);
        unsigned_checksum += (unsigned char)(header->RawBlock[i]);
    }
    // if the block is filled with zeros it marks the end of the archive
    if (unsigned_checksum == 0)
    {
        // assign at least some format so the output stays valid
        format = e_TarPosix;
        finished = TRUE;
        return TRUE;
    }
    // checksum fields must be treated as spaces
    for (i = 0; i < 8; i++)
    {
        signed_checksum -= (signed char)(header->Header.chksum[i]);
        unsigned_checksum -= (unsigned char)(header->Header.chksum[i]);
    }
    int s = sizeof(header->Header.chksum);
    signed_checksum += ' ' * s;
    unsigned_checksum += ' ' * s;

    // verify that it matches
    CQuadWord recorded_checksum;
    if (!FromOctalQ((const unsigned char*)header->Header.chksum, s, recorded_checksum))
        return FALSE;
    if (CQuadWord(signed_checksum, 0) != recorded_checksum &&
        CQuadWord(unsigned_checksum, 0) != recorded_checksum)
        return FALSE;
    // determine the archive format
    if (!strncmp(header->Header.magic, TMAGIC, TMAGLEN))
        format = e_TarPosix;
    else if (!strncmp(header->Header.magic, OLDGNU_MAGIC, OLDGNU_MAGLEN))
        format = e_TarOldGnu;
    else
        format = e_TarV7;
    return TRUE;
}

int CArchive::ReadTarHeader(const unsigned char* buffer, SCommonHeader& header)
{
    CALL_STACK_MESSAGE1("CArchive::ReadTarHeader(, )");

    // the first header block is already read and validated
    const TTarBlock* tarHeader = (const TTarBlock*)buffer;
    Offset += CQuadWord(BLOCKSIZE, 0);

    for (;;)
    {
        // extract the data size (TODO: in the original tar only LNKTYPE uses zero while the rest are read; check the others for correctness
        // verify the remaining cases to ensure they are correct)
        if (tarHeader->Header.typeflag == LNKTYPE ||
            tarHeader->Header.typeflag == SYMTYPE ||
            tarHeader->Header.typeflag == DIRTYPE)
            header.FileInfo.Size.Set(0, 0);
        else if (!FromOctalQ((const unsigned char*)tarHeader->Header.size, 1 + 12, header.FileInfo.Size))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }

        // handle long GNU names
        if (tarHeader->Header.typeflag == GNUTYPE_LONGNAME ||
            tarHeader->Header.typeflag == GNUTYPE_LONGLINK)
        {
            // sanity checking
            if (header.FileInfo.Size > CQuadWord(10000, 0))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                return TAR_ERROR;
            }
            // ignore link entries and keep only actual file names
            if (tarHeader->Header.typeflag == GNUTYPE_LONGNAME)
            {
                if (header.Name != NULL)
                    free(header.Name);
                header.Name = (char*)malloc(header.FileInfo.Size.LoDWord + 1);
                if (header.Name == NULL)
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                                      MSGBOX_ERROR);
                    return TAR_ERROR;
                }
            }
            DWORD toread = header.FileInfo.Size.LoDWord;
            DWORD offs = 0;
            // data referenced by tarHeader is no longer valid once GetBlock is called
            char typeFlag = tarHeader->Header.typeflag;
            while (toread > 0)
            {
                const unsigned char* ptr = Stream->GetBlock(BLOCKSIZE);
                if (ptr == NULL)
                {
                    SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                      LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
                    return TAR_ERROR;
                }
                Offset += CQuadWord(BLOCKSIZE, 0);
                DWORD rest = BLOCKSIZE < toread ? BLOCKSIZE : toread;
                // ignore long-name link entries
                if (typeFlag == GNUTYPE_LONGNAME)
                    memcpy(header.Name + offs, ptr, rest);
                offs += rest;
                toread -= rest;
            }
            // terminate with a null character...
            if (typeFlag == GNUTYPE_LONGNAME)
                *(header.Name + offs) = '\0';
            // and read the next block for further processing
            tarHeader = (const TTarBlock*)Stream->GetBlock(BLOCKSIZE);
            if (tarHeader == NULL)
            {
                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                return TAR_ERROR;
            }
            Offset += CQuadWord(BLOCKSIZE, 0);
            if (!IsTarHeader(tarHeader->RawBlock, header.Finished, header.Format))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                return TAR_ERROR;
            }
            // and loop again
            continue;
        }
        // if no long name was read, use the name from the header
        if (header.Name == NULL)
        {
            // TODO: name matching in the original TAR ignores the prefix. That probably should not hurt us
            // but it should probably be verified :-)
            char tmpName[101], tmpPrefix[156];
            strncpy_s(tmpName, tarHeader->Header.name, _TRUNCATE);
            // the old GNU header used the prefix field differently, so we must not read it
            if (header.Format != e_TarOldGnu)
                strncpy_s(tmpPrefix, tarHeader->Header.prefix, _TRUNCATE);
            else
                tmpPrefix[0] = '\0';
            size_t len = strlen(tmpName);
            if (tmpPrefix[0] != '\0')
                len += 1 + strlen(tmpPrefix);
            header.Name = (char*)malloc(len + 1);
            if (header.Name == NULL)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                                  MSGBOX_ERROR);
                return TAR_ERROR;
            }
            if (tmpPrefix[0] != '\0')
            {
                strcpy(header.Name, tmpPrefix);
                strcat(header.Name, "/");
            }
            else
                header.Name[0] = '\0';
            strcat(header.Name, tmpName);
        }
        // assume a regular file
        header.IsDir = FALSE;
        // inspect the file type
        switch (tarHeader->Header.typeflag)
        {
        case LNKTYPE:
        case SYMTYPE:
            // TODO: tar for DOS extracts symlinks as hard links and hard links
            //       as copies. Or perhaps we could try shortcuts...
        case CHRTYPE:
        case BLKTYPE:
        case FIFOTYPE:
        case CHRSPEC:
        case XHDTYPE:
        case XGLTYPE:
            // types we cannot handle
            header.Ignored = TRUE;
            break;
        case AREGTYPE:
        case REGTYPE:
        case CONTTYPE: // according to the tar sources this counts as a file for some reason...
            // files to extract
            header.IsDir = FALSE;
            header.Ignored = FALSE;
            break;
        case DIRTYPE:
            // directories
            header.IsDir = TRUE;
            header.Ignored = FALSE;
            break;
        case GNUTYPE_VOLHDR:
        case GNUTYPE_MULTIVOL: // TODO: continuation of a file from a previous archive...
        case GNUTYPE_NAMES:    // TODO: the original extracts this through extract_mangle. It is reportedly unused now, but we could review it...
            // information irrelevant for extraction
            header.Ignored = TRUE;
            break;
        case GNUTYPE_DUMPDIR:
            // directory dump in an incremental archive
            //   tar iterates the dump in the archive and removes files that are not present
            //   which probably is not meaningful for us, so we do nothing.
            header.Ignored = TRUE;
            break;
        default:
            // error
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_BADSIG), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            header.Ignored = TRUE;
            break;
        }
        // load the file information
        if (!FromOctalQ((const unsigned char*)tarHeader->Header.mtime, sizeof(tarHeader->Header.mtime) + 1, header.MTime))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
        if (!FromOctalQ((const unsigned char*)tarHeader->Header.mode, sizeof(tarHeader->Header.mode), header.Mode))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
        if (header.Format == e_TarOldGnu &&
            tarHeader->OldGnuHeader.isextended)
            header.IsExtendedTar = TRUE;
        // TODO: if the header contains an extension other than sparse blocks,
        //   such as a long name, it needs to be handled here
        return TAR_OK;
    }
}

BOOL CArchive::GetStreamHeader(SCommonHeader& header)
{
    CALL_STACK_MESSAGE1("CArchive::GetStreamHeader( )");

    // create a "fake" stream header
    header.FileInfo.NameLen = strlen(Stream->GetOldName());
    header.FileInfo.Name = SalamanderGeneral->DupStr(Stream->GetOldName());
    header.Name = (char*)malloc(header.FileInfo.NameLen + 1);
    if (header.FileInfo.Name == NULL || header.Name == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    strcpy(header.FileInfo.Name, Stream->GetOldName());
    strcpy(header.Name, Stream->GetOldName());
    // determine the extension
    char* s = header.FileInfo.Name + header.FileInfo.NameLen - 1;
    while (s >= header.FileInfo.Name && *s != '.')
        s--;
    //  if (s > header.FileInfo.Name)   // ".cvspass" is considered an extension in Windows...
    if (s >= header.FileInfo.Name)
        header.FileInfo.Ext = s + 1;
    else
        header.FileInfo.Ext = header.FileInfo.Name + header.FileInfo.NameLen;
    // copy the parameters from the current file
    Stream->GetFileInfo(header.FileInfo.LastWrite, header.FileInfo.Size,
                        header.FileInfo.Attr);
    if (!Stream->IsOk())
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    // and the remaining attributes
    header.FileInfo.Hidden = header.FileInfo.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
    header.FileInfo.IsLink = SalamanderGeneral->IsFileLink(header.FileInfo.Ext);
    header.FileInfo.IsOffline = 0;
    header.FileInfo.PluginData = -1; // unnecessary, merely for completeness
    return TRUE;
} /* CArchive::GetStreamHeader */

BOOL CArchive::ListStream(CSalamanderDirectoryAbstract* dir)
{
    CALL_STACK_MESSAGE1("CArchive::ListStream( )");

    // prepare a structure with file information
    SCommonHeader header;
    if (!GetStreamHeader(header))
        return FALSE;
    // an unpacked raw stream is always a file, so add it as one
    if (!dir->AddFile(NULL, header.FileInfo, NULL))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_FDATA), LoadStr(IDS_TARERR_TITLE),
                                          MSGBOX_ERROR);
        return FALSE;
    }
    // prevent the destructor from deallocating it
    header.FileInfo.Name = NULL;
    return TRUE;
}

BOOL CArchive::UnpackStream(const char* targetPath, BOOL doProgress,
                            const char* nameInArchive, CNames* names, const char* newName)
{
    CALL_STACK_MESSAGE3("CArchive::UnpackStream(%s, %d)", targetPath, doProgress);

    CQuadWord filePos, sizeDelta;
    if (doProgress)
    {
        filePos = Stream->GetStreamPos();
        // open the progress dialog
        SalamanderIf->OpenProgressDialog(LoadStr(IDS_UNPACKPROGRESS_TITLE), FALSE, NULL, FALSE);
        SalamanderIf->ProgressSetTotalSize(Stream->GetStreamSize(), CQuadWord(-1, -1));
    }
    SCommonHeader header;
    if (!GetStreamHeader(header))
    {
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    if (!(nameInArchive != NULL && !strcmp(header.Name, nameInArchive)) &&
        !(names != NULL && names->IsNamePresent(header.Name)))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_NOTFOUND), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    BOOL toSkip = TRUE;
    char* extractedName;
    HANDLE file;

    // construct the target name
    if (!newName)
        newName = header.Name;
    extractedName = (char*)malloc(strlen(targetPath) + 1 +
                                  strlen(newName) + 1);
    if (extractedName == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                          MSGBOX_ERROR);
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    strcpy(extractedName, targetPath);
    if (extractedName[strlen(extractedName) - 1] != '\\')
        strcat(extractedName, "\\");
    strcat(extractedName, (newName[0] == '\\' ? 1 : 0) + newName);
    // create the new file
    char arcfiledata[500];
    char arcfilename[500];
    MakeFileInfo(header, arcfiledata, arcfilename);
    file = SalamanderSafeFile->SafeFileCreate(extractedName, GENERIC_WRITE, 0, FILE_ATTRIBUTE_NORMAL,
                                              header.IsDir, SalamanderGeneral->GetMainWindowHWND(),
                                              arcfilename, arcfiledata, &Silent, TRUE, &toSkip, NULL, 0, NULL, NULL);
    // abort on any problem
    if (file == INVALID_HANDLE_VALUE)
    {
        SalamanderIf->CloseProgressDialog();
        free(extractedName);
        // if the user wishes to skip, our work here is done
        if (toSkip)
            return TRUE;
        return FALSE;
    }
    // update the file name in the progress dialog
    if (doProgress)
    {
        char progresstxt[1000];
        strcpy(progresstxt, LoadStr(IDS_UNPACKPROGRESS_TEXT));
        strcat(progresstxt, header.Name);
        SalamanderIf->ProgressDialogAddText(progresstxt, TRUE);
    }
    // the size field in the header may not be reliable; keep unpacking while data remains
    for (;;)
    {
        // read an entire block
        unsigned short read;
        const unsigned char* buffer = Stream->GetBlock(BUFSIZE, &read);
        // if a full block is not available, read whatever remains
        if (buffer == NULL && read > 0)
            buffer = Stream->GetBlock(read);
        if (!Stream->IsOk())
        {
            SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                              LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            CloseHandle(file);
            DeleteFile(extractedName);
            free(extractedName);
            SalamanderIf->CloseProgressDialog();
            return FALSE;
        }
        // if the stream is fine yet returns nothing, we are at the end
        if (buffer == NULL)
            break;
        // and write it out
        DWORD written;
        if (!WriteFile(file, buffer, read, &written, NULL) || written != read)
        {
            char message[1000];
            DWORD err = GetLastError();
            strcpy(message, LoadStr(IDS_TARERR_FWRITE));
            if (written != read)
                strcat(message, LoadStr(IDS_TARERR_WRSIZE));
            else
                strcat(message, SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->ShowMessageBox(message, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            CloseHandle(file);
            DeleteFile(extractedName);
            free(extractedName);
            SalamanderIf->CloseProgressDialog();
            return FALSE;
        }
        // update the progress if necessary
        if (doProgress)
        {
            sizeDelta = filePos;
            filePos = Stream->GetStreamPos();
            sizeDelta = filePos - sizeDelta;
            // advance the progress and handle cancellation
            if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
            {
                // handle cancellation
                CloseHandle(file);
                DeleteFile(extractedName);
                free(extractedName);
                SalamanderIf->CloseProgressDialog();
                return FALSE;
            }
        }
    }
    // set the properties of the created file
    SetFileTime(file, &header.FileInfo.LastWrite, &header.FileInfo.LastWrite, &header.FileInfo.LastWrite);
    if (!CloseHandle(file))
    {
        char buffer[1000];
        DWORD err = GetLastError();
        strcpy(buffer, LoadStr(IDS_TARERR_FWRITE));
        strcat(buffer, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(buffer, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        DeleteFile(extractedName);
        free(extractedName);
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    SetFileAttributes(extractedName, header.FileInfo.Attr);
    // the name is no longer needed
    free(extractedName);
    SalamanderIf->CloseProgressDialog();
    return TRUE;
}

CArchiveAbstract* CreateArchive(LPCTSTR fileName, CSalamanderForOperationsAbstract* salamander)
{
    CArchiveAbstract* archive = new CDEBArchive(fileName, salamander);

    if (archive && archive->IsOk())
        return archive;
    delete archive;

    archive = new CArchive(fileName, salamander, 0, CQuadWord(0, 0));

    if (archive && archive->IsOk())
        return archive;
    delete archive;

    return NULL;
}
