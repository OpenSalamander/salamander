// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "array2.h"

#include "selfextr/comdefs.h"
#include "config.h"
#include "typecons.h"
#include "zip.rh2"
#include "chicon.h"
#include "common.h"
#include "add_del.h"

int CZipPack::CountFilesInRoot(int* filesInRoot, bool* rootExist)
{
    CALL_STACK_MESSAGE1("CZipPack::CountFilesInRoot(, )");
    CFileHeader* centralHeader;
    char* tempName;
    unsigned tempNameLen;
    int errorID = 0;
    const char* zipRoot = ZipRoot;
    int rootLen = RootLen;

    //centralHeader = (CFileHeader *) malloc( MAX_HEADER_SIZE);
    tempName = (char*)malloc(MAX_HEADER_SIZE);
    if (!tempName)
    {
        return IDS_LOWMEM;
    }
    *filesInRoot = 0;
    *rootExist = false;
    centralHeader = (CFileHeader*)NewCentrDir;
    for (; (char*)centralHeader < NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize && !errorID;
         centralHeader = (CFileHeader*)((char*)centralHeader +
                                        sizeof(CFileHeader) +
                                        centralHeader->NameLen +
                                        centralHeader->ExtraLen +
                                        centralHeader->CommentLen))
    {
        if ((char*)centralHeader + sizeof(CFileHeader) > NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize ||
            (char*)centralHeader + sizeof(CFileHeader) + centralHeader->NameLen +
                    centralHeader->ExtraLen + centralHeader->CommentLen >
                NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize)
        {
            errorID = IDS_ERRFORMAT;
            Fatal = true;
            break;
        }
        tempNameLen = ProcessName(centralHeader, tempName);
        if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                          zipRoot, rootLen, tempName, rootLen) == CSTR_EQUAL)
        {
            if (*(tempName + rootLen) == '\\')
                (*filesInRoot)++;
            else
            {
                if (*(tempName + rootLen) == 0)
                    *rootExist = true;
            }
        }
    }
    //TRACE_I("FilesInRoot " << *filesInRoot);
    //free(centralHeader);
    free(tempName);
    return errorID;
}

int CZipPack::DeleteFiles(int* deletedFiles)
{
    CALL_STACK_MESSAGE1("CZipPack::DeleteFiles()");
    char* buffer;
    int i = 0;
    QWORD delta = 0;
    QWORD writePos;
    QWORD readPos;
    QWORD moveSize;
    QWORD delSize;
    CFileInfo* curFile;
    CFileInfo* nextFile;
    CLocalFileHeader localHeader;
    unsigned bytesRead;
    int errorID = 0;
    int ret;
    //bool              cancel =  false;
    char progrTextBuf[MAX_PATH + 32];
    char* progrText;
    char* sour;
    int progrPrefixLen; //"deleting: "

    buffer = (char*)malloc(DECOMPRESS_INBUFFER_SIZE);
    if (!buffer)
        return IDS_LOWMEM;
    if (Pack)
        sour = LoadStr(IDS_REMOVING);
    else
        sour = LoadStr(IDS_DELETING);
    progrText = progrTextBuf;
    while (*sour)
        *progrText++ = *sour++;
    progrPrefixLen = (int)(progrText - progrTextBuf);
    if (Config.BackupZip)
    {
        Salamander->ProgressDialogAddText(LoadStr(IDS_BACKUPING), TRUE);
        Salamander->ProgressSetTotalSize(CQuadWord().SetUI64(DelFiles[0]->LocHeaderOffs),
                                         ProgressTotalSize);
        errorID = MoveData(0, 0, DelFiles[0]->LocHeaderOffs, buffer);
    }
    if (Pack)
        Salamander->ProgressDialogAddText(LoadStr(IDS_REMOVEFILES), TRUE);
    else
        Salamander->ProgressDialogAddText(LoadStr(IDS_DELETEFILES), TRUE);
    if (!errorID && !UserBreak)
    {
        writePos = DelFiles[0]->LocHeaderOffs;
        for (i; i < DelFiles.Count; i++)
        {
            curFile = DelFiles[i];
            /*
      TRACE_I("Deleting file: " << curFile->Name <<
              ", method: " << curFile->Method <<
              ", flag: " << curFile->Flag <<
              ", isdir: " << curFile->IsDir <<
              ", file attr:" << curFile->FileAttr);
*/
            lstrcpyn(progrText, curFile->Name + RootLen + (RootLen ? 1 : 0), MAX_PATH + 32 - progrPrefixLen);
            Salamander->ProgressDialogAddText(progrTextBuf, TRUE);
            if (i + 1 < DelFiles.Count)
                nextFile = DelFiles[i + 1];
            else
                nextFile = NULL;
            ZipFile->FilePointer = curFile->LocHeaderOffs;
            ret = Read(ZipFile, &localHeader, sizeof(CLocalFileHeader), &bytesRead, NULL);
            if (ret || bytesRead != sizeof(CLocalFileHeader))
            {
                if (ret)
                    errorID = IDS_NODISPLAY;
                else
                {
                    Fatal = true;
                    errorID = IDS_ERRFORMAT;
                }
                break;
            }
            else
            {
                int descSize = 0;

                if (curFile->Flag & GPF_DATADESCR)
                {
                    if ((curFile->Size >= 0xFFFFFFFF) || (curFile->CompSize >= 0xFFFFFFFF) || (curFile->LocHeaderOffs >= 0xFFFFFFFF))
                        descSize = sizeof(CZip64DataDescriptor);
                    else
                        descSize = sizeof(CDataDescriptor);
                }
                delSize = sizeof(CLocalFileHeader) +
                          localHeader.NameLen +
                          localHeader.ExtraLen +
                          curFile->CompSize + descSize;
                readPos = curFile->LocHeaderOffs + delSize;
                delta = readPos - writePos;
                if (nextFile)
                    moveSize = nextFile->LocHeaderOffs - readPos;
                else
                    moveSize = /*EOCentrDir.*/ CentrDirOffs - readPos;
                if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
                {
                    UserBreak = true;
                    break;
                }
                Salamander->ProgressSetTotalSize(CQuadWord().SetUI64(moveSize), ProgressTotalSize);
                errorID = MoveData(writePos, readPos, moveSize, buffer);
                UpdateCentrDir(curFile, nextFile, delta);
                if (errorID)
                    break;
                writePos += moveSize;
                if (!Salamander->ProgressAddSize(localHeader.NameLen + localHeader.ExtraLen + descSize, TRUE))
                {
                    if (!UserBreak)
                        Salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING), FALSE);
                    UserBreak = true;
                }
                if (UserBreak)
                {
                    if (!Config.BackupZip)
                        i++;
                    break;
                }
            }
        }
    }
    *deletedFiles = i;
    if (*deletedFiles == DelFiles.Count)
        /*EONewCentrDir.*/ NewCentrDirOffs = writePos;
    free(buffer);
    return errorID;
}

int CZipPack::MoveData(QWORD writePos, QWORD readPos, QWORD moveSize, char* buffer)
{
    CALL_STACK_MESSAGE4("CZipPack::MoveData(%I64u, %I64u, %I64u, , )", writePos,
                        readPos, moveSize);
    unsigned readSize;
    unsigned bytesRead;
    int ret;
    int errorID = 0;

    while (moveSize)
    {
        if (moveSize > DECOMPRESS_INBUFFER_SIZE)
            readSize = DECOMPRESS_INBUFFER_SIZE;
        else
            readSize = (unsigned)moveSize;
        ZipFile->FilePointer = readPos;
        ret = Read(ZipFile, buffer, readSize, &bytesRead, NULL);
        if (ret || bytesRead != readSize)
        {
            if (ret)
                errorID = IDS_NODISPLAY;
            else
            {
                Fatal = true;
                errorID = IDS_ERRFORMAT;
            }
            break;
        }
        TempFile->FilePointer = writePos;
        if (Write(TempFile, buffer, readSize, NULL))
        {
            errorID = IDS_NODISPLAY;
            break;
        }
        moveSize -= readSize;
        writePos += readSize;
        readPos += readSize;
        if (!Salamander->ProgressAddSize(readSize, TRUE))
        {
            if (!UserBreak)
                Salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING), FALSE);
            Salamander->ProgressEnableCancel(FALSE);
            UserBreak = true;
            if (Config.BackupZip)
                break;
        }
    }
    return errorID;
}

void CZipPack::UpdateCentrDir(CFileInfo* curFile, CFileInfo* nextFile, QWORD delta)
{
    CALL_STACK_MESSAGE2("CZipPack::UpdateCentrDir(, , 0x%I64X)", delta);
    CFileHeader* centrHeader;
    char* sour;
    int i;
    unsigned size;

    centrHeader = (CFileHeader*)NewCentrDir;
    for (i = 0; (char*)centrHeader < NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize;)
    {
        QWORD locHeaderOffs = centrHeader->LocHeaderOffs;
        char* locHeaderOffsOffs = NULL;

        if ((0xFFFFFFFF == locHeaderOffs) && (centrHeader->ExtraLen >= 2 + 2 + 8))
        {
            locHeaderOffsOffs = (char*)(centrHeader) + sizeof(CFileHeader) + centrHeader->NameLen;

            locHeaderOffsOffs += 2 + 2; // HeaderID & DataSize
            if (0xFFFFFFFF == centrHeader->CompSize)
                locHeaderOffsOffs += 8;
            if (0xFFFFFFFF == centrHeader->Size)
                locHeaderOffsOffs += 8;
            locHeaderOffs = *(QWORD*)locHeaderOffsOffs;
        }

        if (locHeaderOffs == curFile->LocHeaderOffs)
        {
            sour = (char*)centrHeader +
                   sizeof(CFileHeader) +
                   centrHeader->NameLen +
                   centrHeader->ExtraLen +
                   centrHeader->CommentLen;
            size = (int)((NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize) - sour);
            /*EONewCentrDir.*/ NewCentrDirSize -= sizeof(CFileHeader) +
                                                  centrHeader->NameLen +
                                                  centrHeader->ExtraLen +
                                                  centrHeader->CommentLen;
            memmove(centrHeader, sour, size);
            EONewCentrDir.TotalEntries--;
            EONewCentrDir.DiskTotalEntries--;
            continue;
        }
        if (locHeaderOffs > curFile->LocHeaderOffs &&
            (!nextFile || locHeaderOffs < nextFile->LocHeaderOffs))
        {
            locHeaderOffs -= delta;
            if (!locHeaderOffsOffs)
            {
                // Was 32bit, still is 32bit
                centrHeader->LocHeaderOffs = (__UINT32)locHeaderOffs;
            }
            else
            {
                // We leave Zip64 record here even when no longer needed, it is not a violation
                *(QWORD*)locHeaderOffsOffs = locHeaderOffs;
            }
        }
        centrHeader = (CFileHeader*)((char*)centrHeader +
                                     sizeof(CFileHeader) +
                                     centrHeader->NameLen +
                                     centrHeader->ExtraLen +
                                     centrHeader->CommentLen);
        i++;
    }
}

void CZipPack::Recover()
{
    CALL_STACK_MESSAGE1("CZipPack::Recover()");
    RecoverOK = false;
    TempFile->Flags |= PE_QUIET;
    /*
  if (!WriteCentrDir())
    if (!WriteEOCentrDirRecord())
      if(SetEndOfFile(TempFile->File))
        RecoverOK = true;
        */
    if (!WriteCentrDir() && !WriteEOCentrDirRecord() &&
        !Flush(TempFile, TempFile->OutputBuffer, TempFile->BufferPosition, NULL) &&
        SetEndOfFile(TempFile->File))
    {
        RecoverOK = true;
    }
}
