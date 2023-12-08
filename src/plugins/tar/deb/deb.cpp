// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "tar.rh"
#include "tar.rh2"
#include "lang\lang.rh"

#include "../dlldefs.h"
#include "../fileio.h"
#include "deb.h"

#define DEB_STREAM_NAME_CONTROL "control"
#define DEB_STREAM_NAME_DATA "data"

void ShowError(int errorID)
{
    char txtbuf[1000];
    int err = GetLastError();
    strcpy(txtbuf, LoadStr(errorID));
    strcat(txtbuf, SalamanderGeneral->GetErrorText(err));
    SalamanderGeneral->ShowMessageBox(txtbuf, LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
}

CDEBArchive::CDEBArchive(const char* fileName, CSalamanderForOperationsAbstract* salamander)
{
    CALL_STACK_MESSAGE2("CDEBArchive::CDEBArchive(%s)", fileName);

    controlArchive = dataArchive = NULL;
    bOK = FALSE;

    // Open input file
    HANDLE file = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        ShowError(IDS_GZERR_FOPEN);
        return;
    }
    // Read file header
    char buffer[8];
    DWORD read;

    if (!ReadFile(file, buffer, sizeof(buffer), &read, NULL) || (read != sizeof(buffer)))
    {
        ShowError(IDS_ERR_FREAD);
        CloseHandle(file);
        return;
    }
    if (memcmp(buffer, "!<arch>\x0a", sizeof(buffer)) != 0)
    { // Not an "ar" archive
        CloseHandle(file);
        return;
    }

    // There should be 3 items: debian-binary, control.tar.gz, data.tar.gz
    SARBlock ARBlock;
    if (!ReadFile(file, &ARBlock, sizeof(ARBlock), &read, NULL) || (read != sizeof(ARBlock)))
    {
        ShowError(IDS_ERR_FREAD);
        CloseHandle(file);
        return;
    }

    // Check that the first item is valid, is named "debian-binary", and has 4 bytes
    DWORD SubArchiveSize;
    sscanf(ARBlock.FileSize, "%u", &SubArchiveSize);
    if ((ARBlock.FileMagic != DEB_AR_MAGIC) || _strnicmp(ARBlock.FileName, "debian-binary", sizeof("debian-binary") - 1) || (SubArchiveSize != 4))
    {
        // No -> reject the file
        CloseHandle(file);
        return;
    }
    // Skip the 4 data bytes that should contain version number "2.0\x0A"
    DWORD pos = SetFilePointer(file, SubArchiveSize, NULL, FILE_CURRENT);
    if (pos == INVALID_SET_FILE_POINTER)
    {
        ShowError(IDS_GZERR_SEEK);
        CloseHandle(file);
        return;
    }
    // Read the next item and check it is valid
    if (!ReadFile(file, &ARBlock, sizeof(ARBlock), &read, NULL) || (read != sizeof(ARBlock)))
    {
        ShowError(IDS_ERR_FREAD);
        CloseHandle(file);
        return;
    }
    if (ARBlock.FileMagic != DEB_AR_MAGIC)
    {
        CloseHandle(file);
        return;
    }
    sscanf(ARBlock.FileSize, "%u", &SubArchiveSize);
    pos += sizeof(ARBlock);

    // Open the fist subarchive
    CArchive *archive = new CArchive(fileName, salamander, pos, CQuadWord(SubArchiveSize, 0));
    if (!archive->IsOk())
    {
        delete archive;
        archive = NULL;
        return;
    }
    // We are happy if at least one subarchive was recognized
    if (AssignArchive(ARBlock.FileName, archive))
        bOK = TRUE;

    // Skip the subarchive and read the 3rd and last subarchive
    if (SubArchiveSize & 1)
        SubArchiveSize++; // ARBlock starts on even positions
    pos = SetFilePointer(file, SubArchiveSize, NULL, FILE_CURRENT);
    if (pos == INVALID_SET_FILE_POINTER)
    {
        CloseHandle(file);
        return;
    }
    if (!ReadFile(file, &ARBlock, sizeof(ARBlock), &read, NULL) || (read != sizeof(ARBlock)))
    {
        ShowError(IDS_ERR_FREAD);
        CloseHandle(file);
        return;
    }
    if (ARBlock.FileMagic != DEB_AR_MAGIC)
    {
        CloseHandle(file);
        return;
    }
    sscanf(ARBlock.FileSize, "%u", &SubArchiveSize);
    pos += sizeof(ARBlock);
    archive = new CArchive(fileName, salamander, pos, CQuadWord(SubArchiveSize, 0));
    if (!archive->IsOk())
    {
        delete archive;
        archive = NULL;
    }
    if (AssignArchive(ARBlock.FileName, archive))
        bOK = TRUE;
    CloseHandle(file);
}

CDEBArchive::~CDEBArchive(void)
{
    delete controlArchive;
    delete dataArchive;
}

BOOL CDEBArchive::ListArchive(const char* prefix, CSalamanderDirectoryAbstract* dir)
{
    BOOL ret = controlArchive->ListArchive(DEB_STREAM_NAME_CONTROL "\\", dir);
    if (!dataArchive)
        return ret;
    return ret | dataArchive->ListArchive(DEB_STREAM_NAME_DATA "\\", dir);
}

BOOL CDEBArchive::UnpackOneFile(const char* nameInArchive, const CFileData* fileData,
                                const char* targetPath, const char* newFileName)
{
    CALL_STACK_MESSAGE4("CDEBArchive::UnpackOneFile(%s, , %s, , %s)", nameInArchive, targetPath, newFileName);
    if (!strncmp(nameInArchive, DEB_STREAM_NAME_CONTROL "\\", sizeof(DEB_STREAM_NAME_CONTROL "\\") - 1))
    {
        nameInArchive += sizeof(DEB_STREAM_NAME_CONTROL "\\") - 1;
        return controlArchive->UnpackOneFile(nameInArchive, fileData, targetPath, newFileName);
    }
    if (!strncmp(nameInArchive, DEB_STREAM_NAME_DATA "\\", sizeof(DEB_STREAM_NAME_DATA "\\") - 1))
    {
        nameInArchive += sizeof(DEB_STREAM_NAME_DATA "\\") - 1;
        return dataArchive->UnpackOneFile(nameInArchive, fileData, targetPath, newFileName);
    }
    _ASSERT(0);
    return FALSE;
}

BOOL CDEBArchive::UnpackArchive(const char* targetPath, const char* archiveRoot,
                                SalEnumSelection next, void* param)
{
    CALL_STACK_MESSAGE3("CDEBArchive::UnpackArchive(%s, %s, , )", targetPath, archiveRoot);
    if (!strncmp(archiveRoot, DEB_STREAM_NAME_CONTROL, sizeof(DEB_STREAM_NAME_CONTROL) - 1))
    {
        archiveRoot += sizeof(DEB_STREAM_NAME_CONTROL) - 1;
        if (archiveRoot[0] == '\\')
            archiveRoot++;
        return controlArchive->UnpackArchive(targetPath, archiveRoot, next, param);
    }
    if (!strncmp(archiveRoot, DEB_STREAM_NAME_DATA, sizeof(DEB_STREAM_NAME_DATA) - 1))
    {
        archiveRoot += sizeof(DEB_STREAM_NAME_DATA) - 1;
        if (archiveRoot[0] == '\\')
            archiveRoot++;
        return dataArchive->UnpackArchive(targetPath, archiveRoot, next, param);
    }
    _ASSERT(!*archiveRoot);

    // Looks like either entire control or entire data or both folders are to be extracted
    // Split names according the 2 subarchives
    const char* curName;
    BOOL isDir, isData, isControl;
    CQuadWord size;
    CQuadWord totalSize(0, 0);
    CNames dataNames, controlNames;
    isData = isControl = FALSE;
    while ((curName = next(NULL, 0, &isDir, &size, NULL, param, NULL)) != NULL)
    {
        if (!strncmp(curName, DEB_STREAM_NAME_CONTROL, sizeof(DEB_STREAM_NAME_CONTROL) - 1))
        {
            curName += sizeof(DEB_STREAM_NAME_CONTROL) - 1;
            if (!*curName)
                curName = "*";
            controlNames.AddName(curName, isDir, NULL, NULL);
            isControl = TRUE;
        }
        else if (!strncmp(curName, DEB_STREAM_NAME_DATA, sizeof(DEB_STREAM_NAME_DATA) - 1))
        {
            curName += sizeof(DEB_STREAM_NAME_DATA) - 1;
            if (!*curName)
                curName = "*";
            dataNames.AddName(curName, isDir, NULL, NULL);
            isData = TRUE;
        }
        else
            _ASSERT(0); // Unexpected folder

        totalSize = totalSize + size;
    }
    // kontrola volneho mista, predpokladam, ze TestFreeSpace vyhodi patricnou hlasku
    if (!SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                          targetPath, totalSize, LoadStr(IDS_TARERR_HEADER)))
        return FALSE;

    // a vlastni vybaleni podle jmen
    BOOL ret = FALSE;
    char* path = (char*)malloc(strlen(targetPath) + max(sizeof(DEB_STREAM_NAME_CONTROL), sizeof(DEB_STREAM_NAME_DATA)) - 1 + 1 + 1);
    if (path)
    {
        if (isControl)
        {
            strcpy(path, targetPath);
            if (path[0] && (path[strlen(path) - 1] != '\\'))
                strcat(path, "\\");
            strcat(path, DEB_STREAM_NAME_CONTROL);
            ret = controlArchive->DoUnpackArchive(path, archiveRoot, controlNames);
        }

        if (isData)
        {
            strcpy(path, targetPath);
            if (path[0] && (path[strlen(path) - 1] != '\\'))
                strcat(path, "\\");
            strcat(path, DEB_STREAM_NAME_DATA);
            ret = dataArchive->DoUnpackArchive(path, archiveRoot, dataNames) && (isControl ? ret : TRUE);
        }
        free(path);
    }
    else
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
    }
    return ret;
}

BOOL CDEBArchive::UnpackWholeArchive(const char* mask, const char* targetPath)
{
    BOOL isData = FALSE, isControl = FALSE;
    if (mask)
    {
        if (!_strnicmp(mask, DEB_STREAM_NAME_CONTROL "\\", sizeof(DEB_STREAM_NAME_CONTROL "\\") - 1))
        {
            mask += sizeof(DEB_STREAM_NAME_CONTROL "\\") - 1;
            isControl = TRUE;
        }
        else if (!_strnicmp(mask, DEB_STREAM_NAME_DATA "\\", sizeof(DEB_STREAM_NAME_DATA "\\") - 1))
        {
            mask += sizeof(DEB_STREAM_NAME_DATA "\\") - 1;
            isData = TRUE;
        }
        else if (mask[0] == '*')
        {
            isControl = isData = TRUE;
        }
        else
        {
            return FALSE; // invalid mask
        }
    }
    else
    {
        isControl = isData = TRUE;
    }

    BOOL ret = FALSE;
    char* path = (char*)malloc(strlen(targetPath) + max(sizeof(DEB_STREAM_NAME_CONTROL), sizeof(DEB_STREAM_NAME_DATA)) - 1 + 1 + 1);

    if (path)
    {
        // controlArchive is always present
        if (isControl)
        {
            strcpy(path, targetPath);
            if (path[0] && (path[strlen(path) - 1] != '\\'))
                strcat(path, "\\");
            strcat(path, DEB_STREAM_NAME_CONTROL);
            ret = controlArchive->UnpackWholeArchive(mask, path);
        }

        if (isData && dataArchive)
        {
            strcpy(path, targetPath);
            if (path[0] && (path[strlen(path) - 1] != '\\'))
                strcat(path, "\\");
            strcat(path, DEB_STREAM_NAME_DATA);
            ret = dataArchive->UnpackWholeArchive(mask, path) && (isControl ? ret : TRUE);
        }
        free(path);
    }
    else
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
    }
    return ret;
}

BOOL CDEBArchive::AssignArchive(const char* archName, CArchive* archive)
{
    if (!controlArchive && !strncmp(archName, DEB_STREAM_NAME_CONTROL".", sizeof(DEB_STREAM_NAME_CONTROL".") - 1))
    {
        controlArchive = archive;
        return TRUE;
    }
    else if (!dataArchive && !strncmp(archName, DEB_STREAM_NAME_DATA".", sizeof(DEB_STREAM_NAME_DATA".") - 1))
    {
        dataArchive = archive;
        return TRUE;
    }

    // archiv se uz nebude pouzivat
    delete archive;
    return FALSE;
}
