// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <winioctl.h>
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
#include <stdio.h>

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
#include "chicon.h"
#include "common.h"
#include "add_del.h"
#include "deflate.h"
#include "crypt.h"
#include "iosfxset.h"
#include "sfxmake/sfxmake.h"

#ifndef SSZIP
#include "zip.rh"
#include "zip.rh2"
#include "lang\lang.rh"
#include "dialogs.h"
#else //SSZIP
#include "sszip/dialogs.h"
#endif //SSZIP

#define PUT(type, val) \
    { \
        *(type*)extra64 = val; \
        extra64 += sizeof(type); \
    }

void* LoadRCData(int id, DWORD& size)
{
    HRSRC hRsrc = FindResource(DLLInstance, MAKEINTRESOURCE(id), RT_RCDATA);
    if (hRsrc)
    {
        void* data = LoadResource(DLLInstance, hRsrc);
        if (data)
        {
            size = SizeofResource(DLLInstance, hRsrc);
            return data;
        }
    }
    return NULL;
}

int CZipPack::PackNormal(SalEnumSelection2 next, void* param)
{
    CALL_STACK_MESSAGE1("CZipPack::PackNormal( , )");

    int ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                          true, false);
    if (ret)
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    char title[1024];
    sprintf(title, LoadStr(IDS_ADDPROGTITLE), SalamanderGeneral->SalPathFindFileName(ZipName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);
    ErrorID = CheckZip();
    if (ZeroZip)
    {
        EOCentrDir.Signature = SIG_EOCENTRDIR;
        EOCentrDir.DiskNum = 0;
        EOCentrDir.StartDisk = 0;
        EOCentrDir.DiskTotalEntries = 0;
        EOCentrDir.TotalEntries = 0;
        /*EOCentrDir.*/ CentrDirSize = 0;
        /*EOCentrDir.*/ CentrDirOffs = 0;
        EOCentrDir.CommentLen = 0;
        Config.BackupZip = false;
    }
    if (!ErrorID && EOCentrDir.DiskNum == 0xFFFF)
        ErrorID = IDS_MODIFICATION_NOT_SUPPORTED;
    if (!ErrorID)
    {
        if (Config.BackupZip)
            ErrorID = CreateTempFile();
        else
            TempFile = ZipFile;
        if (!ErrorID)
        {
            ErrorID = EnumFiles2(next, param);
            if (!ErrorID)
            {
                ErrorID = LoadCentralDirectory();
                if (!ErrorID)
                {
                    AddTotalSize = CQuadWord(0, 0);
                    MatchedTotalSize = CQuadWord(0, 0); //size of deleted files
                    int addCount;
                    ErrorID = MatchFiles(addCount);
                    if (!ErrorID && addCount > 0xFFFF &&
                        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOTFILES),
                                                          LoadStr(IDS_PLUGINNAME), MSGBOX_EX_QUESTION) != IDYES)
                    {
                        ErrorID = IDS_NODISPLAY;
                        UserBreak = TRUE;
                    }
                    if (!ErrorID && !NothingToDo)
                    {
                        if (DelFiles.Count)
                        {
                            QuickSortHeaders(0, DelFiles.Count - 1, DelFiles);
                            ProgressTotalSize = CQuadWord().SetUI64(ZipFile->Size) -
                                                CQuadWord().SetUI64(DelFiles[0]->LocHeaderOffs) -
                                                MatchedTotalSize -
                                                CQuadWord(sizeof(CLocalFileHeader) * DelFiles.Count, 0) -
                                                CQuadWord().SetUI64(CentrDirSize) -
                                                CQuadWord(sizeof(CEOCentrDirRecord), 0) -
                                                CQuadWord(EOCentrDir.CommentLen, 0);
                            if (Config.BackupZip)
                                ProgressTotalSize += CQuadWord().SetUI64(DelFiles[0]->LocHeaderOffs);
                        }
                        else if (Config.BackupZip)
                            ProgressTotalSize = CQuadWord().SetUI64(CentrDirOffs); //size of backuped file
                        else
                            ProgressTotalSize = CQuadWord(0, 0);
                        ProgressTotalSize += AddTotalSize + CQuadWord(addCount, 0);
                        if (DelFiles.Count)
                        {
                            int i;
                            ErrorID = DeleteFiles(&i);
                            if (ErrorID && !Config.BackupZip && !ZeroZip)
                                Recover();
                        }
                        else //backup zip
                            if (Config.BackupZip)
                                ErrorID = BackupZip();
                        if (!ErrorID && !UserBreak)
                        {
                            ErrorID = PackFiles();
                            if (ErrorID && !Config.BackupZip && !ZeroZip)
                                Recover();
                        }
                        if (!ErrorID && (!UserBreak || UserBreak && !Config.BackupZip && !ZeroZip))
                            ErrorID = FinishPack();
                    }
                }
                if (NewCentrDir)
                    free(NewCentrDir);
            }
            if (Config.BackupZip)
            {
                if (ErrorID || UserBreak || NothingToDo)
                {
                    CloseCFile(TempFile);
                    DeleteFile(TempName);
                }
                else
                {
                    CloseCFile(ZipFile);
                    ZipFile = NULL;
                    if (ZipAttr & FILE_ATTRIBUTE_COMPRESSED)
                    {
                        NTFSCompressFile(TempFile->File);
                        ZipAttr &= ~FILE_ATTRIBUTE_COMPRESSED;
                    }

                    if (Config.TimeToNewestFile &&
                        NewestFileTime.dwLowDateTime != 0 && NewestFileTime.dwHighDateTime != 0)
                        SetFileTime(TempFile->File, NULL, NULL, &NewestFileTime);

                    CloseCFile(TempFile);
                    SalamanderGeneral->ClearReadOnlyAttr(ZipName);
                    if (!DeleteFile(ZipName) ||
                        !MoveFile(TempName, ZipName))
                        ErrorID = IDS_ERRRESTORE;
                    SetFileAttributes(ZipName, ZipAttr | FILE_ATTRIBUTE_ARCHIVE);
                }
            }
            else
            {
                if (ZeroZip && (ErrorID || UserBreak || NothingToDo))
                {
                    CloseCFile(ZipFile);
                    ZipFile = NULL;
                    DeleteFile(ZipName);
                }
                else
                {
                    if (Config.TimeToNewestFile &&
                        NewestFileTime.dwLowDateTime != 0 && NewestFileTime.dwHighDateTime != 0)
                        SetFileTime(ZipFile->File, NULL, NULL, &NewestFileTime);
                }
            }
            if (Move && !ErrorID && !UserBreak)
                ErrorID = CleanUpSource();
        }
    }
    Salamander->CloseProgressDialog();
    return ErrorID;
}

int CZipPack::PackMultiVol(SalEnumSelection2 next, void* param)
{
    CALL_STACK_MESSAGE1("CZipPack::PackMultiVol( , )");

    bool firstSfxDisk = false;
    QWORD ecrecOffs; //pro sfxa

    IgnoreAllFreeSp = false;
    OverwriteAll = false;
    EOCentrDir.Signature = SIG_EOCENTRDIR;
    EOCentrDir.DiskNum = 0;
    EOCentrDir.StartDisk = 0;
    EOCentrDir.DiskTotalEntries = 0;
    EOCentrDir.TotalEntries = 0;
    /*EOCentrDir.*/ NewCentrDirSize = CentrDirSize = 0;
    /*EOCentrDir.*/ NewCentrDirOffs = CentrDirOffs = 4;
    EOCentrDir.CommentLen = 0;
    NewCentrDir = NULL;
    EONewCentrDir = EOCentrDir;

    char title[1024];
    sprintf(title, LoadStr(IDS_ADDPROGTITLE), SalamanderGeneral->SalPathFindFileName(ZipName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);

    if (Options.Action & PA_SELFEXTRACT)
    {
        Salamander->ProgressDialogAddText(LoadStr(IDS_WRITINGEXE), FALSE);

        char name[MAX_PATH];
        lstrcpy(name, ZipName);
        if (!SalamanderGeneral->SalPathRenameExtension(name, ".exe", MAX_PATH))
        {
            Salamander->CloseProgressDialog();
            return IDS_TOOLONGZIPNAME;
        }

        if (TestIfExist(name))
        {
            Salamander->CloseProgressDialog();
            return ErrorID;
        }

        int ret = CreateCFile(&TempFile, name, GENERIC_WRITE, FILE_SHARE_READ,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                              false, false);
        if (ret)
        {
            if (ret == ERR_LOWMEM)
                ErrorID = IDS_LOWMEM;
            else
                ErrorID = IDS_NODISPLAY;
            Salamander->CloseProgressDialog();
            return ErrorID;
        }

        ErrorID = WriteSfxExecutable(name, Options.SfxSettings.SfxFile, FALSE, 0);
        if (!ErrorID)
        {
            char archName[MAX_PATH];
            MakeFileName(1, Options.SeqNames, SalamanderGeneral->SalPathFindFileName(ZipName), archName,
                         false);
            CharToOem(archName, archName);
            if (!WriteSFXHeader(archName, 0, 0) ||
                Write(TempFile, &EONewCentrDir, sizeof(CEOCentrDirRecord), NULL) || //jen tak pro formu, pozdeji ho aktualizujem
                Flush(TempFile, TempFile->OutputBuffer, TempFile->BufferPosition, NULL))
            {
                ErrorID = IDS_NODISPLAY;
            }
        }
        ecrecOffs = TempFile->FilePointer - sizeof(CEOCentrDirRecord);
        ArchiveDataOffs += sizeof(CEOCentrDirRecord);
        if (TempFile)
            CloseCFile(TempFile);
        TempFile = NULL;
        if (ErrorID)
        {
            Salamander->CloseProgressDialog();
            return ErrorID;
        }

        SalamanderGeneral->CutDirectory(name);
        CQuadWord free;
        SalamanderGeneral->GetDiskFreeSpace(&free, name, NULL);
        if (free == CQuadWord(-1, -1))
        {
            ProcessError(IDS_ERRGETDISKFREESP, 0, name, PE_NOSKIP | PE_NORETRY, NULL);
            ErrorID = IDS_NODISPLAY;
            Salamander->CloseProgressDialog();
            return ErrorID;
        }

        if (free < CQuadWord(MIN_VOLSIZE, 0) ||
            Options.VolumeSize != -1 &&
                (ArchiveDataOffs + MIN_VOLSIZE > (QWORD)Options.VolumeSize ||
                 ArchiveDataOffs + free.Value < (QWORD)Options.VolumeSize))
        {
            if (Removable)
            {
                char buf[256];
                sprintf(buf, LoadStr(IDS_CHDISKTEXT), 1, 2);
                if (ChangeDiskDialog(SalamanderGeneral->GetMsgBoxParent(), buf) != IDOK)
                {
                    Salamander->CloseProgressDialog();
                    return ErrorID = IDS_NODISPLAY;
                }
            }
            DiskNum++;
        }
        else
            firstSfxDisk = true;
    }

    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);
    ErrorID = CreateNextFile(firstSfxDisk);
    if (!ErrorID)
    {
        ErrorID = EnumFiles2(next, param);
        if (!ErrorID)
        {
            MatchAll();
            if (AddFiles.Count > 0xFFFF)
            {
                if (Options.Action & PA_SELFEXTRACT)
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOTFILESSFX), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                    ErrorID = IDS_NODISPLAY;
                    UserBreak = TRUE;
                }
                else
                {
                    if (SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOTFILES),
                                                          LoadStr(IDS_PLUGINNAME), MSGBOX_EX_QUESTION) != IDYES)
                    {
                        ErrorID = IDS_NODISPLAY;
                        UserBreak = TRUE;
                    }
                }
            }
            if (!NothingToDo && !ErrorID)
            {
                unsigned i = SIG_DATADESCR;
                if (Write(TempFile, &i, 4, NULL))
                {
                    ErrorID = IDS_NODISPLAY;
                }
                else
                {
                    //rezervujeme si misto pro central directory na zacatku souboru
                    if (Options.Action & PA_SELFEXTRACT)
                    {
                        ErrorID = FinishPack(FPR_SFXRESERVE);
                        /*EONewCentrDir.*/ NewCentrDirOffs = /*(unsigned)*/ TempFile->FilePointer;
                    }
                    if (!ErrorID)
                    {
                        ErrorID = PackFiles();
                        if (!ErrorID && !UserBreak)
                        {
                            if (Options.Action & PA_SELFEXTRACT)
                            {
                                /*EONewCentrDir.*/ NewCentrDirOffs = 4;
                                ErrorID = FinishPack(FPR_SFXEND);
                                if (!ErrorID && /*EONewCentrDir.*/ NewCentrDirSize == 0)
                                    ErrorID = IDS_EMPTYARCHIVE;
                            }
                            else
                            {
                                ErrorID = FinishPack(FPR_NORMAL);
                                // prejmenujeme posledni soubor
                                if (!ErrorID && Options.SeqNames && Config.WinZipNames)
                                {
                                    if (TempFile)
                                    {
                                        char name[MAX_PATH];
                                        strcpy(name, TempFile->FileName);
                                        CloseCFile(TempFile);
                                        TempFile = NULL;
                                        MoveFile(name, ZipName);
                                    }
                                }
                            }
                            if (!ErrorID && !UserBreak)
                            {
                                if (Options.Action & PA_SELFEXTRACT)
                                {
                                    CloseCFile(TempFile);
                                    TempFile = NULL;
                                    ErrorID = WriteSFXECRec(ecrecOffs);
                                    if (!ErrorID)
                                    {
                                        ErrorID = WriteSFXCentralDir();
                                        if (!ErrorID)
                                        {
                                            if (Flush(TempFile, TempFile->OutputBuffer, TempFile->BufferPosition, NULL))
                                                ErrorID = IDS_NODISPLAY;
                                        }
                                    }
                                }
                                if (Move && !ErrorID && !UserBreak)
                                {
                                    ErrorID = CleanUpSource();
                                }
                            }
                        }
                    }
                }
            }
        }
        if (TempFile)
        {
            CloseCFile(TempFile);
        }
        if (ErrorID || UserBreak || NothingToDo)
            DeleteFile(TempName);
    }
    Salamander->CloseProgressDialog();
    return ErrorID;
}

int CZipPack::PackSelfExtract(SalEnumSelection2 next, void* param)
{
    CALL_STACK_MESSAGE1("CZipPack::PackSelfExtract( , )");
    int ret;

    if (!SalamanderGeneral->SalPathRenameExtension(ZipName, ".exe", MAX_PATH))
        return IDS_TOOLONGZIPNAME;

    if (TestIfExist(ZipName))
        return ErrorID;

    ret = CreateCFile(&TempFile, ZipName, GENERIC_WRITE, FILE_SHARE_READ,
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                      false, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    }
    IgnoreAllFreeSp = false;
    OverwriteAll = false;
    EOCentrDir.Signature = SIG_EOCENTRDIR;
    EOCentrDir.DiskNum = 0;
    EOCentrDir.StartDisk = 0;
    EOCentrDir.DiskTotalEntries = 0;
    EOCentrDir.TotalEntries = 0;
    /*EOCentrDir.*/ NewCentrDirSize = CentrDirSize = 0;
    /*EOCentrDir.*/ NewCentrDirOffs = CentrDirOffs = 4;
    EOCentrDir.CommentLen = 0;
    NewCentrDir = NULL;
    EONewCentrDir = EOCentrDir;
    char title[1024];
    sprintf(title, LoadStr(IDS_ADDPROGTITLE), SalamanderGeneral->SalPathFindFileName(ZipName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);
    if (!ErrorID)
    {
        ErrorID = EnumFiles2(next, param);
        if (!ErrorID)
        {
            MatchAll();
            if (AddFiles.Count > 0xFFFF)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOTFILESSFX),
                                                  LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                ErrorID = IDS_NODISPLAY;
                UserBreak = TRUE;
            }
            if (!NothingToDo && !ErrorID)
            {
                ErrorID = WriteSfxExecutable(ZipName, Options.SfxSettings.SfxFile, FALSE, 0);
                if (!ErrorID)
                {

                    ErrorID = PackFiles();
                    if (!ErrorID && !UserBreak)
                    {
                        ErrorID = FinishPack();
                        if (!ErrorID && /*EONewCentrDir.*/ NewCentrDirSize == 0)
                            ErrorID = IDS_EMPTYARCHIVE;
                        if (Move && !ErrorID && !UserBreak)
                        {
                            ErrorID = CleanUpSource();
                        }
                    }
                }
            }
        }
        if (TempFile)
        {
            CloseCFile(TempFile);
        }
        if (ErrorID || UserBreak || NothingToDo)
        {
            DeleteFile(ZipName);
        }
    }
    Salamander->CloseProgressDialog();
    return ErrorID;
}

int CZipPack::WriteCentrDir()
{
    CALL_STACK_MESSAGE1("CZipPack::WriteCentrDir()");
    if (Config.TimeToNewestFile)
    {
        int e = FindNewestFile();
        if (e)
            return e;
    }
    TempFile->FilePointer = /*EONewCentrDir.*/ NewCentrDirOffs;
    if (Write(TempFile, NewCentrDir, /*EONewCentrDir.*/ (unsigned)NewCentrDirSize, NULL))
    {
        return IDS_NODISPLAY;
    }
    return 0;
}
int CZipPack::WriteEOCentrDirRecord()
{
    CALL_STACK_MESSAGE1("CZipPack::WriteEOCentrDirRecord()");
    unsigned comLen = Comment ? EONewCentrDir.CommentLen : 0;
    unsigned len = 0;

    if (NewCentrDirSize < 0xFFFFFFFF)
        EONewCentrDir.CentrDirSize = (__UINT32)NewCentrDirSize;
    else
    {
        EONewCentrDir.CentrDirSize = 0xFFFFFFFF;
        len = sizeof(CZip64EOCentrDirRecord) + sizeof(CZip64EOCentrDirLocator);
    }
    if (NewCentrDirOffs < 0xFFFFFFFF)
        EONewCentrDir.CentrDirOffs = (__UINT32)NewCentrDirOffs;
    else
    {
        EONewCentrDir.CentrDirOffs = 0xFFFFFFFF;
        len = sizeof(CZip64EOCentrDirRecord) + sizeof(CZip64EOCentrDirLocator);
    }

    if (Options.Action & PA_MULTIVOL &&
        sizeof(CEOCentrDirRecord) + comLen + len > DiskSize - TempFile->FilePointer)
    {
        int error = NextDisk();
        if (error)
            return error;
        EONewCentrDir.DiskTotalEntries = 0;
    }
    EONewCentrDir.DiskNum = DiskNum;
    if (len > 0)
    {
        CZip64EOCentrDirRecord cdr64;
        CZip64EOCentrDirLocator cdl64;

        cdr64.Signature = SIG_ZIP64EOCENTRDIR;
        cdr64.Size = sizeof(CZip64EOCentrDirRecord) - sizeof(cdr64.Signature) - sizeof(cdr64.Size);
        cdr64.Version = cdr64.VersionExtr = VN_ZIP64;
        cdr64.DiskNum = DiskNum;
        cdr64.StartDisk = EONewCentrDir.StartDisk;
        cdr64.DiskTotalEntries = EONewCentrDir.DiskTotalEntries;
        cdr64.TotalEntries = EONewCentrDir.TotalEntries;
        cdr64.CentrDirSize = NewCentrDirSize;
        cdr64.CentrDirOffs = NewCentrDirOffs;

        cdl64.Signature = SIG_ZIP64LOCATOR;
        cdl64.Zip64StartDisk = DiskNum; // ???
        cdl64.Zip64Offset = TempFile->FilePointer;
        cdl64.TotalDisks = 1; // ???

        if (Write(TempFile, &cdr64, sizeof(CZip64EOCentrDirRecord), NULL))
        {
            return IDS_NODISPLAY;
        }
        if (Write(TempFile, &cdl64, sizeof(CZip64EOCentrDirLocator), NULL))
        {
            return IDS_NODISPLAY;
        }
    }
    if (Write(TempFile, &EONewCentrDir, sizeof(CEOCentrDirRecord), NULL))
    {
        return IDS_NODISPLAY;
    }
    if (Comment && Write(TempFile, Comment, comLen, NULL))
    {
        return IDS_NODISPLAY;
    }
    return 0;
}

void CZipPack::AddAESExtraField(CFileInfo* fileInfo, CAESExtraField* extraAES, __UINT16* pExtraLen)
{
    if (Options.Encrypt && (Config.EncryptMethod >= EM_AES128))
    {
        extraAES->HeaderID = AES_HEADER_ID;
        extraAES->DataSize = 7;
        extraAES->Version = AES_VERSION_1;
        extraAES->VendorID = AES_NONVENDOR_ID;
        extraAES->Strength = (Config.EncryptMethod == EM_AES128) ? 1 : 3;
        extraAES->Method = (fileInfo->InternalFlags & IF_STORED_AES) ? CM_STORED : CM_DEFLATED;
        *pExtraLen += sizeof(CAESExtraField);
    }
}

int CZipPack::ExportLocalHeader(CFileInfo* fileInfo, char* buffer)
{
    CALL_STACK_MESSAGE1("CZipPack::ExportLocalHeader(, )");
    CLocalFileHeader* localHeader = (CLocalFileHeader*)buffer;
    FILETIME ft;
    SYSTEMTIME st;

    localHeader->Signature = SIG_LOCALFH;
    localHeader->VersionExtr = VN_NEED_TO_EXTR(fileInfo->Method);
    //localHeader->VersionExtr |= HS_FAT << 8;
    localHeader->Method = fileInfo->Method;
    localHeader->Flag = fileInfo->Flag;
    FileTimeToLocalFileTime(&fileInfo->LastWrite, &ft);
    //Y2K
    if (FileTimeToSystemTime(&ft, &st))
        if (st.wYear > MaxZipTime.wYear ||
            st.wYear == MaxZipTime.wYear &&
                st.wMonth == MaxZipTime.wMonth &&
                st.wDay == MaxZipTime.wDay &&
                st.wHour == MaxZipTime.wHour &&
                st.wMinute == MaxZipTime.wMinute &&
                st.wSecond >= MaxZipTime.wSecond)
            st = MaxZipTime;
        else
        {
            if (st.wYear < MinZipTime.wYear)
                st = MinZipTime;
        }
    else
        st = MaxZipTime;
    SystemTimeToFileTime(&st, &ft);
    FileTimeToDosDateTime(&ft, (unsigned short*)&localHeader->Date,
                          (unsigned short*)&localHeader->Time);
    localHeader->Crc = fileInfo->Crc;
    int Zip64Size = 0;
    if (fileInfo->CompSize < 0xFFFFFFFF)
        localHeader->CompSize = (__UINT32)fileInfo->CompSize;
    else
    {
        localHeader->CompSize = 0xFFFFFFFF;
        Zip64Size = 8 + 8; // In Local Header, both Size and CompSize must be present, if any
    }
    if (fileInfo->Size < 0xFFFFFFFF)
        localHeader->Size = (__UINT32)fileInfo->Size;
    else
    {
        localHeader->Size = 0xFFFFFFFF;
        Zip64Size = 8 + 8; // In Local Header, both Size and CompSize must be present, if any
    }
    localHeader->NameLen = ExportName(buffer + sizeof(CLocalFileHeader), fileInfo);
    localHeader->ExtraLen = 0;
    if (Zip64Size)
    {
        char* extra64 = (char*)(buffer + sizeof(CLocalFileHeader) + localHeader->NameLen);
        PUT(WORD, 1); // HeaderID = 1;
        PUT(WORD, Zip64Size);
        PUT(QWORD, fileInfo->Size);
        PUT(QWORD, fileInfo->CompSize);
        localHeader->ExtraLen = 2 + 2 + Zip64Size;
        // AES version is higher than ZIP64 version
        if (localHeader->VersionExtr < VN_ZIP64)
            localHeader->VersionExtr = VN_ZIP64;
    }
    AddAESExtraField(fileInfo, (CAESExtraField*)((char*)buffer + sizeof(CLocalFileHeader) + localHeader->NameLen + localHeader->ExtraLen), &localHeader->ExtraLen);

    return sizeof(CLocalFileHeader) + localHeader->NameLen + localHeader->ExtraLen;
}

int CZipPack::WriteLocalHeader(CFileInfo* fileInfo, char* buffer)
{
    CALL_STACK_MESSAGE1("CZipPack::WriteLocalHeader(, )");
    int size = ExportLocalHeader(fileInfo, buffer);

    if (Write(TempFile, buffer, size, NULL))
        return IDS_NODISPLAY;
    return 0;
}

int CZipPack::WriteDataDecriptor(CFileInfo* fileInfo)
{
    CALL_STACK_MESSAGE1("CZipPack::WriteDataDecriptor()");
    union
    {
        CDataDescriptor descr32;
        CZip64DataDescriptor descr64;
    } descr;
    int error = 0;
    size_t len;

    descr.descr32.Signature = SIG_DATADESCR; // Shared with 64bit
    descr.descr32.Crc = fileInfo->Crc;       // Shared with 64bit
    if ((fileInfo->CompSize >= 0xFFFFFFFF) || (fileInfo->Size >= 0xFFFFFFFF) || (fileInfo->LocHeaderOffs >= 0xFFFFFFFF))
    {
        descr.descr64.CompSize = fileInfo->CompSize;
        descr.descr64.Size = fileInfo->Size;
        len = sizeof(descr.descr64);
    }
    else
    {
        descr.descr32.CompSize = (__UINT32)fileInfo->CompSize;
        descr.descr32.Size = (__UINT32)fileInfo->Size;
        len = sizeof(descr.descr32);
    }
    if (Options.Action & PA_MULTIVOL &&
        len > DiskSize - TempFile->FilePointer)
    {
        error = NextDisk();
    }
    if (!error)
    {
        if (Write(TempFile, &descr, (unsigned int)len, NULL))
            error = IDS_NODISPLAY;
    }
    return error;
}

int CZipPack::WriteCentralHeader(CFileInfo* fileInfo, char* buffer, BOOL first, int reason)
{
    CALL_STACK_MESSAGE3("CZipPack::WriteCentralHeader(, , %d, %d)", first, reason);
    CFileHeader* centralHeader = (CFileHeader*)buffer;
    FILETIME ft;
    SYSTEMTIME st;
    int Zip64Size = 0;

    centralHeader->Signature = SIG_CENTRFH;
    centralHeader->Version = VN_MADE_BY;
    centralHeader->Version |= HS_FAT << 8;
    centralHeader->VersionExtr = VN_NEED_TO_EXTR(fileInfo->Method);
    //centralHeader->VersionExtr |= HS_FAT << 8;
    centralHeader->Flag = fileInfo->Flag;
    centralHeader->Method = fileInfo->Method;
    FileTimeToLocalFileTime(&fileInfo->LastWrite, &ft);
    //Y2K
    if (FileTimeToSystemTime(&ft, &st))
        if (st.wYear > MaxZipTime.wYear ||
            st.wYear == MaxZipTime.wYear &&
                st.wMonth == MaxZipTime.wMonth &&
                st.wDay == MaxZipTime.wDay &&
                st.wHour == MaxZipTime.wHour &&
                st.wMinute == MaxZipTime.wMinute &&
                st.wSecond >= MaxZipTime.wSecond)
            st = MaxZipTime;
        else
        {
            if (st.wYear < MinZipTime.wYear)
                st = MinZipTime;
        }
    else
        st = MaxZipTime;
    SystemTimeToFileTime(&st, &ft);
    FileTimeToDosDateTime(&ft, (unsigned short*)&centralHeader->Date,
                          (unsigned short*)&centralHeader->Time);
    centralHeader->Crc = fileInfo->Crc;
    if (fileInfo->Size < 0xFFFFFFFF)
        centralHeader->Size = (__UINT32)fileInfo->Size;
    else
    {
        centralHeader->Size = 0xFFFFFFFF;
        Zip64Size = 8;
    }
    if (fileInfo->CompSize < 0xFFFFFFFF)
        centralHeader->CompSize = (__UINT32)fileInfo->CompSize;
    else
    {
        centralHeader->Size = 0xFFFFFFFF;
        centralHeader->CompSize = 0xFFFFFFFF;
        Zip64Size = 16;
    }

    centralHeader->NameLen = ExportName(buffer + sizeof(CFileHeader), fileInfo);
    centralHeader->ExtraLen = 0;
    centralHeader->CommentLen = 0;
    if (fileInfo->StartDisk < 0xFFFF)
        centralHeader->StartDisk = fileInfo->StartDisk;
    else
    {
        centralHeader->Size = 0xFFFF;
        Zip64Size += 4;
    }
    centralHeader->InterAttr = fileInfo->InterAttr;
    centralHeader->ExternAttr = fileInfo->FileAttr;
    if (fileInfo->LocHeaderOffs < 0xFFFFFFFF)
        centralHeader->LocHeaderOffs = (__UINT32)fileInfo->LocHeaderOffs;
    else
    {
        centralHeader->LocHeaderOffs = 0xFFFFFFFF;
        Zip64Size += 8;
    }
    if (Zip64Size > 0)
    {
        char* extra64 = (char*)(buffer + sizeof(CFileHeader) + centralHeader->NameLen);
        PUT(WORD, 1);         //HeaderID = 1;
        PUT(WORD, Zip64Size); // DataSize = Zip64Size;
        if (centralHeader->Size == 0xFFFFFFFF)
            PUT(QWORD, fileInfo->Size);
        if (centralHeader->CompSize == 0xFFFFFFFF)
            PUT(QWORD, fileInfo->CompSize);
        if (centralHeader->LocHeaderOffs == 0xFFFFFFFF)
            PUT(QWORD, fileInfo->LocHeaderOffs);
        if (centralHeader->StartDisk == 0xFFFF)
            PUT(DWORD, fileInfo->StartDisk);
        centralHeader->ExtraLen = 2 + 2 + Zip64Size;
        centralHeader->VersionExtr = VN_ZIP64;
    }
    AddAESExtraField(fileInfo, (CAESExtraField*)((char*)buffer + sizeof(CFileHeader) + centralHeader->NameLen + centralHeader->ExtraLen), &centralHeader->ExtraLen);

    int centrDirSizeUpd = sizeof(CFileHeader) + centralHeader->NameLen + centralHeader->ExtraLen;
    if (Options.Action & PA_MULTIVOL &&
        centrDirSizeUpd > DiskSize - TempFile->FilePointer &&
        reason != FPR_SFXEND)
    {
        int error = NextDisk();
        if (error)
            return error;
        EONewCentrDir.DiskTotalEntries = 0;
    }
    if (first && Options.Action & PA_MULTIVOL &&
        reason != FPR_SFXEND && reason != FPR_WRITE)
    {
        EONewCentrDir.StartDisk = DiskNum;
        /*EONewCentrDir.*/ NewCentrDirOffs = /*(unsigned) */ TempFile->FilePointer;
    }
    switch (reason)
    {
    case FPR_NORMAL:
        /*EONewCentrDir.*/ NewCentrDirSize += centrDirSizeUpd;
        EONewCentrDir.TotalEntries++;
        EONewCentrDir.DiskTotalEntries++;
    case FPR_WRITE:
        if (Write(TempFile, buffer, centrDirSizeUpd, NULL))
            return IDS_NODISPLAY;
        break;
    case FPR_SFXEND:
        /*EONewCentrDir.*/ NewCentrDirSize += centrDirSizeUpd;
        EONewCentrDir.TotalEntries++;
        //EONewCentrDir.DiskTotalEntries++; //snad takovyto prohrsek nebude tolik vadit
        break;
    case FPR_SFXRESERVE:
        TempFile->FilePointer += centrDirSizeUpd;
        break;
    }
    return 0;
}

int CZipPack::ExportName(char* name, CFileInfo* fileInfo)
{
    CALL_STACK_MESSAGE1("CZipPack::ExportName(, )");

    char* sour = fileInfo->Name;
    char* dest = name;

    while (*sour)
        if (*sour == '\\')
        {
            *dest++ = '/';
            sour++;
        }
        else
            *dest++ = *sour++;
    if (fileInfo->IsDir)
        *dest++ = '/';
    *dest = 0;

    CharToOem(name, name);

    //*dest = NULL;
    return (int)(dest - name);
}

int CZipPack::CreateTempFile()
{
    CALL_STACK_MESSAGE1("CZipPack::CreateTempFile()");
    char pathBuf[MAX_PATH + 1];
    char* path = pathBuf;
    char* name;
    DWORD lastError; //value returned by GetLastError()
    int ret;

    SplitPath(&path, &name, ZipName);
    TempName[0] = 0;
    while (1)
    {
        if (SalamanderGeneral->SalGetTempFileName(path, "Sal", TempName, TRUE, &lastError))
        {
            ret = CreateCFile(&TempFile, TempName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                              true, false);
            switch (ret)
            {
            case ERR_NOERROR:
                return 0;
            case ERR_LOWMEM:
                *TempName = 0;
                return IDS_LOWMEM;
            default:
                *TempName = 0;
                return IDS_NODISPLAY;
            }
        }
        ret = ProcessError(IDS_ERRTEMP, lastError, TempName, PE_NOSKIP, NULL);
        if (ret != ERR_RETRY)
        {
            *TempName = 0;
            return IDS_NODISPLAY;
        }
    }
}

void CZipPack::NTFSCompressFile(HANDLE file)
{
    CALL_STACK_MESSAGE1("CZipPack::NTFSCompressFile()");
    USHORT state = COMPRESSION_FORMAT_DEFAULT;
    ULONG length;

    DeviceIoControl(file, FSCTL_SET_COMPRESSION, &state,
                    sizeof(USHORT), NULL, 0, &length, FALSE);
}

int CZipPack::EnumFiles2(SalEnumSelection2 next, void* param)
{
    CALL_STACK_MESSAGE1("CZipPack::EnumFiles2( , )");
    CAddInfo* newFile;
    const char* nextName;
    BOOL isDir;
    CQuadWord size;
    int errorID = 0;
    int errorOccured;

    while ((nextName = next(SalamanderGeneral->GetMsgBoxParent(), 3, NULL, &isDir, &size, NULL, NULL, param, &errorOccured)) != NULL)
    {
        newFile = new CAddInfo;
        if (!newFile)
        {
            errorID = IDS_LOWMEM;
            break;
        }
        newFile->NameLen = lstrlen(nextName) + SourceLen + 1;
        newFile->Name = (char*)malloc(newFile->NameLen + 1);
        if (!newFile->Name)
        {
            delete newFile;
            errorID = IDS_LOWMEM;
            break;
        }
        lstrcpy(newFile->Name, SourcePath);
        *(newFile->Name + SourceLen) = '\\';
        lstrcpy(newFile->Name + SourceLen + 1, nextName);
        newFile->IsDir = isDir ? true : false;
        newFile->Size = size;
        newFile->Action = AF_ADD;
        if (Config.NoEmptyDirs && newFile->IsDir)
            if (IsDirectoryEmpty(newFile->Name))
            {
                //newFile->FreeDir = true;
                newFile->Action = AF_NOADD;
            }
            else
                newFile->Action = AF_DEL;
        //    else
        //      newFile->FreeDir = false;
        if (!AddFiles.Add(newFile))
        {
            delete newFile;
            errorID = IDS_LOWMEM;
            break;
        }
    }

    // nastala chyba a uzivatel si preje prerusit operaci
    if (errorOccured == SALENUM_CANCEL)
        errorID = IDS_NODISPLAY;

    return errorID;
}

int CZipPack::LoadCentralDirectory()
{
    CALL_STACK_MESSAGE1("CZipPack::LoadCentralDirectory()");
    int errorID = 0;
    int ret;
    unsigned bytesRead;

    if (ZeroZip)
    {
        NewCentrDir = NULL;
        EONewCentrDir = EOCentrDir;
        NewCentrDirSize = CentrDirSize;
        NewCentrDirOffs = CentrDirOffs;
    }
    else
    {
        NewCentrDir = (char*)malloc(/*EOCentrDir.*/ (size_t)CentrDirSize);
        if (!NewCentrDir)
            errorID = IDS_LOWMEM;
        else
        {
            ZipFile->FilePointer = /*EOCentrDir.*/ CentrDirOffs;
            ret = Read(ZipFile, NewCentrDir, /*EOCentrDir.*/ (unsigned)CentrDirSize, &bytesRead, NULL);
            if (ret || bytesRead != /*EOCentrDir.*/ CentrDirSize)
            {
                if (ret)
                    errorID = IDS_NODISPLAY;
                else
                {
                    errorID = IDS_ERRFORMAT;
                    Fatal = true;
                }
                free(NewCentrDir);
            }
            else
            {
                EONewCentrDir = EOCentrDir;
                NewCentrDirSize = CentrDirSize;
                NewCentrDirOffs = CentrDirOffs;
            }
        }
    }
    return errorID;
}

int CZipPack::MatchFiles(int& count)
{
    CALL_STACK_MESSAGE1("CZipPack::MatchFiles()");
    CFileHeader* centralHeader;
    char* inZip;
    unsigned inZipLen;
    CFileInfo file;
    int errorID = 0;
    char* destNameBuf;
    unsigned destLen;
    char* destName;
    CAddInfo* next;
    int j; //temporary variable
    bool overwriteAll = false;
    bool skipAll = false;
    DWORD pathFlag = NORM_IGNORECASE;

    inZip = (char*)malloc(MAX_HEADER_SIZE);
    destNameBuf = (char*)malloc(MAX_HEADER_SIZE);
    if (!inZip || !destNameBuf)
    {
        if (inZip)
            free(inZip);
        if (destNameBuf)
            free(destNameBuf);
        return IDS_LOWMEM;
    }
    if (RootLen)
    {
        lstrcpy(destNameBuf, ZipRoot);
        *(destNameBuf + RootLen) = '\\';
        destName = destNameBuf + RootLen + 1;
    }
    else
        destName = destNameBuf;

    count = 0;

    // test na unix archiv a spocitani souboru v archivu
    for (centralHeader = (CFileHeader*)NewCentrDir;
         (char*)centralHeader < NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize &&
         !errorID;
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
        if (centralHeader->Version >> 8 == HS_UNIX)
        {
            pathFlag = 0;
            Unix = TRUE;
        }
        count++;
    }

    for (centralHeader = (CFileHeader*)NewCentrDir;
         (char*)centralHeader < NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize &&
         !errorID;
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
        inZipLen = ProcessName(centralHeader, inZip);
        for (j = 0; j < AddFiles.Count; j++)
        {
            next = AddFiles[j];
            if (next->Action != AF_ADD && !Config.NoEmptyDirs ||
                next->Action != AF_ADD && !next->IsDir ||
                next->Action == AF_DEL || next->Action == AF_OVERWRITE)
            {
                // na unixech pokracujem v ptani se na prepis jiz skiplych souboru, muze tam byt
                // jiny soubor se stejnym jmenem, ale jinym casem
                if (!Unix || next->Action != AF_NOADD)
                    continue;
            }
            lstrcpy(destName, next->Name + SourceLen + 1);
            destLen = RootLen + next->NameLen - SourceLen - (RootLen ? 0 : 1);
            if (next->Action == AF_NOADD && next->IsDir) //tohle uz muze platit pro adresare soubory preskocime vyse
                if (Move)
                {
                    if (inZipLen >= destLen &&
                        CompareString(LOCALE_USER_DEFAULT, pathFlag,
                                      destNameBuf, RootLen, inZip, RootLen) == CSTR_EQUAL &&
                        CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                                      destNameBuf + RootLen, destLen - RootLen,
                                      inZip + RootLen, destLen - RootLen) == CSTR_EQUAL &&
                        (*(inZip + destLen) == '\\' || *(inZip + destLen) == 0))
                    {
                        next->Action = AF_DEL;
                        continue;
                    }
                }
                else
                    continue;
            if (inZipLen == destLen &&
                CompareString(LOCALE_USER_DEFAULT, pathFlag,
                              destNameBuf, RootLen, inZip, RootLen) == CSTR_EQUAL &&
                CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                              destNameBuf + RootLen, -1, inZip + RootLen, -1) == CSTR_EQUAL)
            {
                ProcessHeader(centralHeader, &file);
                if (file.IsDir && next->IsDir)
                {
                    next->Action = AF_DEL;
                    break;
                }
                if (!file.IsDir && !next->IsDir)
                {
                    bool overwrite;

                    overwrite = false;
                    if (skipAll)
                    {
                        next->Action = AF_NOADD;
                        break;
                    }
                    if (overwriteAll)
                        overwrite = true;
                    else
                    {
                        CFile* sourFile;
                        int ret;

                        ret = CreateCFile(&sourFile, next->Name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0, 0, &SkipAllIOErrors,
                                          false, false);
                        switch (ret)
                        {
                        case ERR_NOERROR:
                        {
                            char attr1[101], attr2[101];
                            FILETIME ft;
                            char* buffer;
                            int len;

                            len = lstrlen(ZipFile->FileName);
                            buffer = (char*)malloc(len + 1 + inZipLen + 1);
                            if (!buffer)
                                errorID = IDS_LOWMEM;
                            else
                            {
                                lstrcpy(buffer, ZipFile->FileName);
                                *(buffer + len) = '\\';
                                lstrcpy(buffer + len + 1, inZip);
                                GetInfo(attr1, &file.LastWrite, file.Size);
                                GetFileTime(sourFile->File, NULL, NULL, &ft);
                                GetInfo(attr2, &ft, sourFile->Size);
                                switch (SalamanderGeneral->DialogOverwrite(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_YESALLSKIPCANCEL,
                                                                           buffer, attr1, next->Name, attr2))
                                {
                                case DIALOG_ALL:
                                    overwriteAll = true;
                                case DIALOG_YES:
                                    overwrite = true;
                                    break;
                                case DIALOG_SKIPALL:
                                    skipAll = true;
                                case DIALOG_SKIP:
                                    next->Action = AF_NOADD;
                                    break;
                                case DIALOG_CANCEL:
                                default:
                                    errorID = IDS_NODISPLAY;
                                }
                                free(buffer);
                            }
                            CloseCFile(sourFile);
                            break;
                        }

                        case ERR_SKIP:
                            next->Action = AF_NOADD;
                            break;
                        case ERR_CANCEL:
                            errorID = IDS_NODISPLAY;
                            break;
                        case ERR_LOWMEM:
                            errorID = IDS_LOWMEM;
                        }
                    }
                    if (overwrite)
                    {
                        CFileInfo* newFile = new CFileInfo;
                        if (!newFile)
                        {
                            errorID = IDS_LOWMEM;
                            break;
                        }
                        *newFile = file;
                        newFile->NameLen = inZipLen;
                        newFile->Name = (char*)malloc(inZipLen + 1);
                        if (!newFile->Name)
                        {
                            delete (newFile);
                            errorID = IDS_LOWMEM;
                            break;
                        }
                        lstrcpy(newFile->Name, inZip);
                        MatchedTotalSize += CQuadWord().SetUI64(newFile->CompSize);
                        DelFiles.Add(newFile);

                        if (Unix)
                        {
                            // na unixovem archivu mohl byt jednou skipnuty a podruhy vybrany
                            // pro prepis, nastavime, ze soubor ma byt pridany
                            next->Action = AF_OVERWRITE;

                            // jeste nastavime jmeno zdrojoveho souboru na shodne (vcetne case)
                            // se jmenem prepisovaneho souboru
                            strcpy(next->Name + SourceLen + 1, inZip + RootLen + (RootLen ? 1 : 0));
                        }
                    }
                    break;
                }
            }
        }
    }
    NothingToDo = true;
    for (j = 0; j < AddFiles.Count; j++)
    {
        next = AddFiles[j];
        if (next->Action != AF_ADD && next->Action != AF_OVERWRITE)
            continue;
        AddTotalSize += next->Size;
        NothingToDo = false;
        count++;
    }
    count -= DelFiles.Count;
    free(inZip);
    free(destNameBuf);
    return errorID;
}

int CZipPack::BackupZip()
{
    CALL_STACK_MESSAGE1("CZipPack::BackupZip()");
    char* buffer;

    buffer = (char*)malloc(DECOMPRESS_INBUFFER_SIZE);
    if (!buffer)
        return IDS_LOWMEM;
    Salamander->ProgressDialogAddText(LoadStr(IDS_BACKUPING), TRUE);
    Salamander->ProgressSetTotalSize(CQuadWord().SetUI64(CentrDirOffs), ProgressTotalSize);
    int ret = MoveData(0, 0, /*EOCentrDir.*/ CentrDirOffs, buffer);
    free(buffer);
    return ret;
}

int ReadInput(char* buffer, unsigned size, int* error, void* user)
{
    CALL_STACK_MESSAGE2("ReadInput( , %u, , )", size);
    unsigned read;
    int ret;
    CZipPack* pack = (CZipPack*)user;

    if (!pack->Salamander->ProgressAddSize(pack->SizeToAdd, TRUE))
    {
        *error = IDS_USERBREAK;
        return 0;
    }
    ret = pack->Read(pack->SourFile, buffer, size, &read, &pack->SkipAllIOErrors);
    pack->SizeToAdd = read;
    switch (ret)
    {
    case ERR_NOERROR:
        if (read)
        {
            pack->Crc = SalamanderGeneral->UpdateCrc32(buffer, read, pack->Crc);
            return read;
        }
        else
        {
            //TRACE_I("EOF");
            return EOF;
        }
    case ERR_SKIP:
        *error = IDS_SKIP;
        return 0;
    case ERR_CANCEL:
        *error = IDS_NODISPLAY;
        return 0;
    }
    return 0;
}

int WriteOutput(char* buffer, unsigned size, void* user)
{
    CALL_STACK_MESSAGE2("WriteOutput( , %u, )", size);
    CZipPack* pack = (CZipPack*)user;
    int error = 0;

    // Note: The compressed output slightly grows if buffer content is modified here anyhow (e.g. memset)
    // I don't know why, looks like a bug in the compressor?
    // NOTE: The file still gets successfully decompressed even then...
    if (pack->Options.Encrypt)
        if (pack->AESContextValid)
            SalamanderCrypt->AESEncrypt(&pack->AESContext, buffer, size);
        else
            Encrypt(buffer, size, pack->Keys);
    if (pack->Options.Action & PA_MULTIVOL)
    {
        unsigned left = size;
        while (left)
        {
            if (left <= pack->DiskSize - pack->TempFile->FilePointer)
            {
                if (pack->Write(pack->TempFile, buffer, left, NULL))
                    return IDS_NODISPLAY;
                left = 0;
            }
            else
            {
                unsigned i = (unsigned)(pack->DiskSize - pack->TempFile->FilePointer);
                if (pack->Write(pack->TempFile, buffer, i, NULL))
                    return IDS_NODISPLAY;
                error = pack->NextDisk();
                if (error)
                    return error;
                left -= i;
                buffer += i;
            }
        }
    }
    else
    {
        if (pack->Write(pack->TempFile, buffer, size, NULL))
            return IDS_NODISPLAY;
    }
    return 0;
}

int CZipPack::PackFiles()
{
    CALL_STACK_MESSAGE1("CZipPack::PackFiles()");
    char* buffer;
    CFileInfo file;
    CAddInfo* next;
    int i;
    int ret;
    __UINT64 writePos;
    CDeflate* defObj = new CDeflate();
    int errorID = 0;
    char progrTextBuf[MAX_PATH + 32];
    char* progrText;
    int progrPrefixLen; //"adding: "
    char* sour;
    unsigned locHeadSize;
#pragma pack(push)
#pragma pack(1)
    union
    {
        char header[ENCRYPT_HEADER_SIZE];
        struct
        {
            unsigned char salt_pwdVer[SAL_AES_MAX_SALT_LENGTH + SAL_AES_PWD_VER_LENGTH];
        } AES;
    } encHeader;
#pragma pack(pop)
    int encHeaderSize = 0;

    AESContextValid = FALSE; // inicializace

    buffer = (char*)malloc(MAX_HEADER_SIZE);
    if (!buffer || !defObj)
    {
        if (buffer)
            free(buffer);
        if (defObj)
            delete defObj;
        return IDS_LOWMEM;
    }
    sour = LoadStr(IDS_ADDING);
    progrText = progrTextBuf;
    while (*sour)
        *progrText++ = *sour++;
    progrPrefixLen = (int)(progrText - progrTextBuf);
    Salamander->ProgressDialogAddText(LoadStr(IDS_ADDFILES), FALSE);
    if ((Options.Action & (PA_SELFEXTRACT | PA_MULTIVOL)) == PA_SELFEXTRACT)
        writePos = ArchiveDataOffs;
    else
        writePos = /*EONewCentrDir.*/ NewCentrDirOffs;
    for (i = 0; i < AddFiles.Count && !errorID && !UserBreak; i++)
    {
        next = AddFiles[i];
        if (next->Action != AF_ADD && next->Action != AF_OVERWRITE)
            continue;
        //TRACE_I("Packing file: " << next->Name);
        lstrcpyn(progrText, next->Name + SourceLen + 1, MAX_PATH + 32 - progrPrefixLen);
        Salamander->ProgressDialogAddText(progrTextBuf, TRUE);
        if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
        {
            UserBreak = true;
            break;
        }
        Salamander->ProgressSetTotalSize(next->Size, ProgressTotalSize);
        file.NameLen = next->NameLen - SourceLen + RootLen - (RootLen ? 0 : 1);
        file.Name = (char*)malloc(file.NameLen + 1);
        if (!file.Name)
        {
            errorID = IDS_LOWMEM;
            break;
        }
        if (RootLen)
        {
            lstrcpy(file.Name, ZipRoot);
            *(file.Name + RootLen) = '\\';
        }
        lstrcpy(file.Name + RootLen + (RootLen ? 1 : 0), next->Name + SourceLen + 1);
        if (next->IsDir)
        {
            ret = GetDirInfo(next->Name, &next->FileAttr, &next->LastWrite);
            if (ret)
            {
                free(file.Name);
                file.Name = NULL;
                switch (ret)
                {
                case ERR_SKIP:
                    UserBreak = !Salamander->ProgressAddSize((int)next->Size.Value + 1, TRUE);
                    next->Action = AF_NOADD;
                    continue;
                case ERR_CANCEL:
                    errorID = IDS_NODISPLAY;
                }
                break;
            }
            next->FileAttr &= FILE_ATTTRIBUTE_MASK;
            file.FileAttr = next->FileAttr;
            file.LastWrite = next->LastWrite;
            file.IsDir = next->IsDir;
            file.Flag = next->Flag = 0;
            file.InterAttr = next->InterAttr = 0;
            file.Method = next->Method = CM_STORED;
            file.Crc = next->Crc = Crc = INIT_CRC;
            file.CompSize = next->CompSize = 0;
            file.Size = 0;
            file.InternalFlags = next->InternalFlags = 0;
            locHeadSize = ExportLocalHeader(&file, buffer);
            if (Options.Action & PA_MULTIVOL)
            {
                if (locHeadSize >= DiskSize - writePos)
                {
                    errorID = NextDisk();
                    if (!errorID)
                    {
                        file.LocHeaderOffs = next->LocHeaderOffs = writePos = 0;
                    }
                }
                else
                {
                    file.LocHeaderOffs = next->LocHeaderOffs = writePos;
                }
                next->StartDisk = file.StartDisk = DiskNum;
                if (!errorID)
                {
                    TempFile->FilePointer = file.LocHeaderOffs;
                    errorID = WriteLocalHeader(&file, buffer);
                }
            }
            else
            {
                next->StartDisk = file.StartDisk = DiskNum;
                file.LocHeaderOffs = next->LocHeaderOffs = writePos;
                TempFile->FilePointer = writePos + locHeadSize;
            }
        }
        else
        {
            ret = CreateCFile(&SourFile, next->Name, GENERIC_READ, FILE_SHARE_READ,
                              OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0, &SkipAllIOErrors,
                              // Do not allow files over 4GB in SFX files, the SFX module probably doesn't support them
                              (Options.Action & PA_SELFEXTRACT) ? false : true, false);
            if (ret)
            {
                free(file.Name);
                file.Name = NULL;
                switch (ret)
                {
                case ERR_LOWMEM:
                    errorID = IDS_LOWMEM;
                    break;
                case ERR_SKIP:
                    UserBreak = !Salamander->ProgressAddSize((int)next->Size.Value + 1, TRUE);
                    next->Action = AF_NOADD;
                    continue;
                case ERR_CANCEL:
                    errorID = IDS_NODISPLAY;
                }
                break;
            }
            file.FileAttr = next->FileAttr = SalamanderGeneral->SalGetFileAttributes(next->Name) & FILE_ATTTRIBUTE_MASK;
            GetFileTime(SourFile->File, NULL, NULL, &file.LastWrite);
            next->LastWrite = file.LastWrite;
            file.IsDir = next->IsDir;
            file.InterAttr = 0;
            file.Method = next->Method = Config.Level && SourFile->Size
                                             ? ((Options.Encrypt && (Config.EncryptMethod >= EM_AES128)) ? CM_AES : CM_DEFLATED)
                                             : CM_STORED;
            file.Flag = next->Flag = (Options.Action & PA_MULTIVOL || Removable) &&
                                             SourFile->Size
                                         ? GPF_DATADESCR
                                         : 0;
            file.InternalFlags = next->InternalFlags = 0;
            file.Crc = Crc = INIT_CRC;
            file.CompSize = 0;
            file.Size = SourFile->Size;
            next->Size = CQuadWord().SetUI64(file.Size);
            locHeadSize = ExportLocalHeader(&file, buffer);
            next->InterAttr = next->Method == CM_STORED ? 0 : -1;
            if (Options.Encrypt && next->Method != CM_STORED)
            {
                if (Config.EncryptMethod == EM_ZIP20)
                {
                    unsigned short check;
                    FILETIME ft;
                    unsigned short date; //dummy
                    unsigned short time;

                    next->Flag |= GPF_ENCRYPTED | GPF_DATADESCR;
                    // NOTE: Patera 2008.12.03: This FAST/SLOW flag is normally set in CDeflate::lm_init()
                    // in deflate.cpp. But in case of encrypted files LocalHeder is written before lm_init() gets called.
                    // IMHO it would be cleaner to do the same as with non-encrypted files.
                    // WinZIP 12.0 complains about different GPF flags in loc & central headers otherwise.
                    if (Config.Level <= 2)
                    {
                        next->Flag |= FAST;
                    }
                    else if (Config.Level >= 8)
                    {
                        next->Flag |= SLOW;
                    }
                    file.Flag = next->Flag;
                    FileTimeToLocalFileTime(&file.LastWrite, &ft);
                    FileTimeToDosDateTime(&ft, &date, &time);
                    check = time;
                    CryptHeader(Options.Password, encHeader.header, check, Keys);
                    encHeaderSize = ENCRYPT_HEADER_SIZE;
                }
                else
                { // AES
                    int strength = Config.EncryptMethod == EM_AES128 ? 1 : 3;
                    int salLen = SAL_AES_SALT_LENGTH(strength);

                    FillBufferWithRandomData((char*)encHeader.AES.salt_pwdVer, salLen);

                    if (SalamanderCrypt->AESInit(&AESContext, strength, Options.Password, strlen(Options.Password),
                                                 encHeader.AES.salt_pwdVer, (LPWORD)(encHeader.AES.salt_pwdVer + salLen)) == SAL_AES_ERR_GOOD_RETURN)
                    {
                        file.Flag = next->Flag |= GPF_ENCRYPTED;
                        encHeaderSize = salLen + SAL_AES_PWD_VER_LENGTH;
                        AESContextValid = TRUE;
                    }
                    else
                    {
                        errorID = IDS_PWDTOOLONG;
                    }
                }
            }
            else
                *Keys = 0;
            if (Options.Action & PA_MULTIVOL && locHeadSize >= DiskSize - writePos)
            {
                errorID = NextDisk();
                if (!errorID)
                {
                    file.LocHeaderOffs = next->LocHeaderOffs = writePos = 0;
                }
            }
            else
            {
                file.LocHeaderOffs = next->LocHeaderOffs = writePos;
            }
            if (!errorID)
            {
                next->StartDisk = file.StartDisk = DiskNum;
                if (Options.Action & PA_MULTIVOL)
                {
                    TempFile->FilePointer = file.LocHeaderOffs;
                    // Pleasee the comment for encrypted archives 35 lines above, it applies also here
                    if (Config.Level <= 2)
                    {
                        next->Flag |= FAST;
                    }
                    else if (Config.Level >= 8)
                    {
                        next->Flag |= SLOW;
                    }
                    file.Flag = next->Flag;
                    errorID = WriteLocalHeader(&file, buffer);
                    if (file.Flag & GPF_ENCRYPTED && !errorID)
                    {
                        if (encHeaderSize <= DiskSize - TempFile->FilePointer)
                        {
                            if (Write(TempFile, &encHeader, encHeaderSize, NULL))
                            {
                                errorID = IDS_NODISPLAY;
                            }
                        }
                        else
                        {
                            if (Write(TempFile, &encHeader, (unsigned)(DiskSize - TempFile->FilePointer), NULL))
                            {
                                errorID = IDS_NODISPLAY;
                            }
                            else
                            {
                                int left;

                                left = encHeaderSize - (unsigned)(DiskSize - TempFile->FilePointer);
                                errorID = NextDisk();
                                if (!errorID)
                                {
                                    if (Write(TempFile, (char*)&encHeader + (encHeaderSize - left), left, NULL))
                                    {
                                        errorID = IDS_NODISPLAY;
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    if (file.Flag & GPF_DATADESCR)
                    {
                        TempFile->FilePointer = file.LocHeaderOffs;
                        errorID = WriteLocalHeader(&file, buffer);
                    }
                    TempFile->FilePointer = file.LocHeaderOffs + locHeadSize;
                    if (file.Flag & GPF_ENCRYPTED)
                    {
                        if (Write(TempFile, &encHeader, encHeaderSize, NULL))
                        {
                            errorID = IDS_NODISPLAY;
                        }
                    }
                }
            }
            if (errorID)
            {
                CloseCFile(SourFile);
                free(file.Name);
                file.Name = NULL;
                break;
            }
            SizeToAdd = 0;
            if (next->Method == CM_STORED)
            {
                if (file.Size)
                {
                    __UINT64 size = 0;
                    errorID = Store(&size);
                    file.CompSize = size;
                }
            }
            else
            {
                ullg size = 0;
                int method = next->Method;
                errorID = defObj->Deflate(&next->InterAttr, &method, Config.Level, &next->Flag,
                                          &size, WriteOutput, ReadInput, this);
                file.CompSize = size;
                switch (errorID)
                {
                case 1:
                    errorID = IDS_BADPACKLEVEL;
                case 2:
                    errorID = IDS_BLOCKVANISHED;
                }
                if (CM_STORED == method)
                {
                    // Deflate() decided to store the file uncompressed -> propagate the actual method
                    if (CM_AES == next->Method)
                    {
                        file.InternalFlags |= IF_STORED_AES;
                        next->InternalFlags |= IF_STORED_AES;
                    }
                    else
                    {
                        next->Method = CM_STORED;
                    }
                }
            }
            if (errorID == IDS_USERBREAK)
            {
                UserBreak = true;
                errorID = 0;
            }
            file.Flag |= next->Flag;
            file.InterAttr = next->InterAttr;
            file.Method = next->Method;
            file.Crc = next->Crc = Crc;
            if (file.Flag & GPF_ENCRYPTED)
            {
                file.CompSize += encHeaderSize;
                if (AESContextValid)
                {
                    unsigned char mac[AES_MAXHMAC];
                    DWORD macLen;

                    SalamanderCrypt->AESEnd(&AESContext, mac, &macLen);
                    AESContextValid = FALSE;
                    errorID = Write(TempFile, mac, macLen, NULL);
                    if (!errorID)
                        file.CompSize += macLen;
                }
            }
            next->CompSize = file.CompSize;
            CloseCFile(SourFile);
        }
        if (!errorID && !UserBreak)
        {
            if (file.Flag & GPF_DATADESCR)
            {
                errorID = WriteDataDecriptor(&file);
                writePos = TempFile->FilePointer;
            }
            else
            {
                writePos = TempFile->FilePointer;
                TempFile->FilePointer = file.LocHeaderOffs;
                errorID = WriteLocalHeader(&file, buffer);
            }
        }
        else
            switch (errorID)
            {
            case IDS_SKIP:
                next->Action = AF_NOADD;
                UserBreak = !Salamander->ProgressAddSize(int(SourFile->Size - SourFile->FilePointer), TRUE);
                errorID = 0;
                break;
            case IDS_BADPACKLEVEL:
                Fatal = true;
            }
        free(file.Name);
        file.Name = NULL;
        UserBreak = !Salamander->ProgressAddSize(1, TRUE);
    }
    if (!errorID && !UserBreak)
        /*EONewCentrDir.*/ NewCentrDirOffs = writePos;
    free(buffer);
    delete defObj;
    return errorID;
}

int CZipPack::FinishPack(int reason)
{
    CALL_STACK_MESSAGE1("CZipPack::FinishPack()");
    CFileInfo file;
    CAddInfo* next;
    char* buffer;
    int errorID = 0;
    int i;
    BOOL first = TRUE;

    if (!(Options.Action & PA_MULTIVOL))
        errorID = WriteCentrDir();
    if (!errorID)
    {
        if (!UserBreak)
        {
            buffer = (char*)malloc(MAX_HEADER_SIZE);
            if (!buffer)
                return IDS_LOWMEM;
            for (i = 0; i < AddFiles.Count; i++)
            {
                next = AddFiles[i];
                if (next->Action != AF_ADD && next->Action != AF_OVERWRITE)
                    continue;
                file.NameLen = next->NameLen - SourceLen + RootLen - (RootLen ? 0 : 1);
                file.Name = (char*)malloc(file.NameLen + 1);
                if (!file.Name)
                {
                    errorID = IDS_LOWMEM;
                    break;
                }
                if (RootLen)
                {
                    strcpy(file.Name, ZipRoot);
                    *(file.Name + RootLen) = '\\';
                }
                strcpy(file.Name + RootLen + (RootLen ? 1 : 0), next->Name + SourceLen + 1);
                file.FileAttr = next->FileAttr;
                file.LastWrite = next->LastWrite;
                file.IsDir = next->IsDir;
                file.Flag = next->Flag;
                file.InterAttr = next->InterAttr;
                file.Method = next->Method;
                file.Crc = next->Crc;
                file.CompSize = next->CompSize;
                file.Size = next->Size.Value;
                file.StartDisk = next->StartDisk;
                file.InternalFlags = next->InternalFlags;
                file.LocHeaderOffs = next->LocHeaderOffs;
                if ((Options.Action & (PA_SELFEXTRACT | PA_MULTIVOL)) == PA_SELFEXTRACT)
                    file.LocHeaderOffs -= ArchiveDataOffs;
                errorID = WriteCentralHeader(&file, buffer, first, reason);
                first = false;
                free(file.Name);
                file.Name = NULL;
                if (errorID)
                    break;
                if ((reason == FPR_NORMAL || reason == FPR_SFXEND) && Config.TimeToNewestFile && !file.IsDir &&
                    (file.LastWrite.dwHighDateTime > NewestFileTime.dwHighDateTime ||
                     file.LastWrite.dwHighDateTime == NewestFileTime.dwHighDateTime &&
                         file.LastWrite.dwLowDateTime > NewestFileTime.dwLowDateTime))
                {
                    NewestFileTime = file.LastWrite;
                }
            }
            free(buffer);
        }
        if (errorID)
            return errorID;
        if (reason == FPR_SFXRESERVE || reason == FPR_WRITE)
            return 0;
        if ((Options.Action & (PA_SELFEXTRACT | PA_MULTIVOL)) == PA_SELFEXTRACT)
        {
            /*
      CSelfExtrHeader header;
      header.Signature = SELFEXTR_SIG;
      header.HeaderSize = ArchiveDataOffs - ArchiveHeaderOffs;
      header.Flags = Options.SfxSettings.Flags;
      header.EOCentrDirOffs = TempFile->FilePointer - ArchiveDataOffs;
      header.ArchiveSize = header.EOCentrDirOffs + sizeof(CEOCentrDirRecord);
      header.CommandLen = lstrlen(Options.SfxSettings.Command);
      if (header.CommandLen) header.CommandLen++;
      header.TextLen = lstrlen(Options.SfxSettings.Text) + 1;
      //if (header.TextLen) header.QuestionLen++;
      header.TitleLen = lstrlen(Options.SfxSettings.Title) + 1;
      header.SubDirLen = lstrlen(Options.SfxSettings.SubDir) + 1;
      header.AboutLen = lstrlen(Options.About) + 1;
      header.ExtractBtnTextLen = lstrlen(Options.SfxSettings.ExtractBtnText) + 1;
      header.ArchiveNameLen = 1;
      TempFile->FilePointer = ArchiveHeaderOffs;
      if (Write(TempFile, &header, sizeof(CSelfExtrHeader), NULL)) return IDS_NODISPLAY;
      if (header.CommandLen &&
          Write(TempFile, Options.SfxSettings.Command, header.CommandLen, NULL)) return IDS_NODISPLAY;
      if (Write(TempFile, Options.SfxSettings.Text, header.TextLen, NULL)) return IDS_NODISPLAY;
      if (Write(TempFile, Options.SfxSettings.Title, header.TitleLen, NULL)) return IDS_NODISPLAY;
      if (Write(TempFile, Options.SfxSettings.SubDir, header.SubDirLen, NULL)) return IDS_NODISPLAY;
      if (Write(TempFile, Options.About, header.AboutLen, NULL)) return IDS_NODISPLAY;
      if (Write(TempFile, Options.SfxSettings.ExtractBtnText, header.ExtractBtnTextLen, NULL)) return IDS_NODISPLAY;
      if (Write(TempFile, "", header.ArchiveNameLen, NULL)) return IDS_NODISPLAY;
      */
            QWORD offs = TempFile->FilePointer - ArchiveDataOffs;
            if (!WriteSFXHeader("", offs, (unsigned)(offs + sizeof(CEOCentrDirRecord))))
                return IDS_NODISPLAY;
            TempFile->FilePointer = ArchiveDataOffs + offs;
            /*EONewCentrDir.*/ NewCentrDirOffs -= ArchiveDataOffs;
        }
        errorID = WriteEOCentrDirRecord();
        if (!errorID)
        {
            if (!Flush(TempFile, TempFile->OutputBuffer, TempFile->BufferPosition, NULL))
            {
                SetEndOfFile(TempFile->File);
            }
            else
                errorID = IDS_NODISPLAY;
        }
    }
    return errorID;
}

int CZipPack::GetDirInfo(const char* name, DWORD* attr, FILETIME* lastWrite)
{
    CALL_STACK_MESSAGE2("CZipPack::GetDirInfo(%s, , )", name);
    WIN32_FIND_DATA data;
    HANDLE search;
    int ret;

    while (1)
    {
        search = FindFirstFile(name, &data);
        if (search == INVALID_HANDLE_VALUE)
        {
            ret = ProcessError(IDS_ERRACCESDIR, GetLastError(), name, 0, &SkipAllIOErrors);
            if (ret != ERR_RETRY)
                return ret;
        }
        else
        {
            *attr = data.dwFileAttributes;
            *lastWrite = data.ftLastWriteTime;
            FindClose(search);
            return 0;
        }
    }
}

int CZipPack::IsDirectoryEmpty(const char* name)
{
    CALL_STACK_MESSAGE2("CZipPack::IsDirectoryEmpty(%s)", name);
    char buf[MAX_PATH + 1];
    int len;
    HANDLE search;
    WIN32_FIND_DATA data;
    int lastError;
    BOOL ret;

    lstrcpyn(buf, name, MAX_PATH + 1);
    len = lstrlen(buf);
    lstrcpyn(buf + len, "\\*.*", MAX_PATH + 1 - len);
    search = FindFirstFile(buf, &data);
    if (search == INVALID_HANDLE_VALUE)
    {
        ProcessError(IDS_ERRACCESDIR, GetLastError(), name, PE_NORETRY | PE_NOSKIP, NULL);
        return 1; //like an empty directory
    }
    ret = TRUE;
    do
    {
        if (data.cFileName[0] != 0 && strcmp(data.cFileName, ".") && strcmp(data.cFileName, ".."))
        {
            ret = FALSE;
            break;
        }
    } while (FindNextFile(search, &data));
    if (ret) // treat any error as if directory is empty
    {
        lastError = GetLastError();
        if (lastError != ERROR_NO_MORE_FILES)
            ProcessError(IDS_ERRACCESDIR, lastError, name, PE_NORETRY | PE_NOSKIP, NULL);
    }
    FindClose(search);
    return ret;
}

int CZipPack::InsertDir(char* dir, TIndirectArray2<TIndirectArray2_char_>& table)
{
    CALL_STACK_MESSAGE1("CZipPack::InsertDir( , )");
    int level;
    const char* sour;

    level = 0;
    sour = dir + SourceLen + 1;
    while (*sour)
    {
        if (*sour == '\\')
            level++;
        sour++;
    }
    if (level + 1 > table.Count)
    {
        //increase table size
        int i = level + 1 - table.Count;
        TIndirectArray2<char>* temp;

        while (i)
        {
            temp = new TIndirectArray2<char>(16, FALSE);
            if (!temp)
                return IDS_LOWMEM;
            if (!table.Add(temp))
                return IDS_LOWMEM;
            i--;
        }
    }
    if (!table[level]->Add(dir))
        return IDS_LOWMEM;
    return 0;
}

int CZipPack::CleanUpSource()
{
    CALL_STACK_MESSAGE1("CZipPack::CleanUpSource()");
    CAddInfo* next;
    TIndirectArray2<TIndirectArray2_char_> table(8);
    int l; //directory level
    int i;
    int errorID = 0;

    //delete files
    for (i = 0; i < AddFiles.Count; i++)
    {
        next = AddFiles[i];
        if (next->Action != AF_ADD && next->Action != AF_OVERWRITE || next->IsDir)
            continue;
        SalamanderGeneral->ClearReadOnlyAttr(next->Name);
        DeleteFile(next->Name);
    }
    //remove directories
    //sort directories by level
    for (i = 0; i < AddFiles.Count; i++)
    {
        next = AddFiles[i];
        if (!next->IsDir || next->Action == AF_NOADD)
            continue;
        errorID = InsertDir(next->Name, table);
        if (errorID)
            return errorID;
    }
    if (!errorID)
    {
        l = table.Count;
        while (l)
        {
            l--;
            for (i = 0; i < table[l]->Count; i++)
            {
                SalamanderGeneral->ClearReadOnlyAttr((*table[l])[i]);
                if (!RemoveDirectory((*table[l])[i]))
                    TRACE_I("error on RemoveDirectory " << GetLastError());
            }
        }
    }
    return 0;
}
/*
int CZipPack::LoadExPackOptions(unsigned flags)
{
  CALL_STACK_MESSAGE1("CZipPack::LoadExPackOptions(unsigned flags)");

  if (!LoadDefaults()) return IDS_NODISPLAY;
  if (Config.ShowExOptions)
  {
    CPackDialog dlg(SalamanderGeneral->GetMainWindowHWND(), this, &Config, &Options,
                    ZipName, flags);
    if (dlg.Proceed() == IDOK) ::Config = Config;
    else return ErrorID = IDS_NODISPLAY;
  }
  if (Options.SfxSettings.Flags & SE_AUTO) Options.SfxSettings.Flags |= SE_NOTALLOWCHANGE;
  if (Options.Action & PA_SELFEXTRACT)
  {
    LastUsedSfxSet.Settings = Options.SfxSettings;
    //to jmeno je jen pro interni potrebu (nekde dal se to testuje na "")
    lstrcpy(LastUsedSfxSet.Name,  "Last Used");
  }
  return 0;
}
*/
int CZipPack::Store(__UINT64* size)
{
    CALL_STACK_MESSAGE1("CZipPack::Store()");

    char* buffer = (char*)malloc(DECOMPRESS_INBUFFER_SIZE);
    //unsigned  left = SourFile->Size;
    unsigned read = 1;
    int error = 0;
    int ret;

    if (!buffer)
        return IDS_LOWMEM;
    *size = 0;
    while (read)
    {
        ret = Read(SourFile, buffer, DECOMPRESS_INBUFFER_SIZE, &read, &SkipAllIOErrors);
        if (ret)
        {
            if (ret == ERR_SKIP)
                error = IDS_SKIP;
            else
                error = IDS_NODISPLAY;
            break;
        }
        if (read)
        {
            Crc = SalamanderGeneral->UpdateCrc32(buffer, read, Crc);
            if (Write(TempFile, buffer, read, NULL))
            {
                error = IDS_NODISPLAY;
                break;
            }
            *size += read;
            if (!Salamander->ProgressAddSize(read, TRUE))
            {
                error = IDS_USERBREAK;
                break;
            }
        }
    }
    free(buffer);
    return error;
}

int CZipPack::CreateNextFile(bool firstSfxDisk)
{
    CALL_STACK_MESSAGE2("CZipPack::CreateNextFile(%d)", firstSfxDisk);

    char buf[128];
    bool overwrite = false;
    int error = 0;
    CQuadWord freesp;
    int lastErr;
    const char* text;
    int flags;
    bool retry;
    char pathBuf[MAX_PATH + 1];
    char* zipPath;
    char* dummy;
    bool testSpace = true;

    MakeFileName(DiskNum + 1, Options.SeqNames, ZipName, TempName,
                 Config.WinZipNames && !(Options.Action & PA_SELFEXTRACT));
    if (SeccondPass)
    {
        switch (CreateCFile(&TempFile, TempName, GENERIC_WRITE, FILE_SHARE_READ,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                            false, false))
        {
        case 0:
            return 0;
        case ERR_LOWMEM:
            return IDS_LOWMEM;
        default:
            return IDS_NODISPLAY;
        }
    }
    if ((TempFile = (CFile*)malloc(sizeof(CFile))) == NULL ||
        (TempFile->FileName = (char*)malloc(lstrlen(TempName) + 1)) == NULL ||
        (TempFile->OutputBuffer = (char*)malloc(OUTPUT_BUFFER_SIZE)) == NULL)
    {
        if (TempFile)
        {
            if (TempFile->FileName)
                free(TempFile->FileName);
            if (TempFile->OutputBuffer)
                free(TempFile->OutputBuffer);
            free(TempFile);
            TempFile = NULL;
        }
        return IDS_LOWMEM;
    }
    TempFile->InputBuffer = NULL;
    zipPath = pathBuf;
    SplitPath(&zipPath, &dummy, ZipName);
    do
    {
        retry = false;
        if (!SalamanderGeneral->CheckAndCreateDirectory(zipPath, NULL, TRUE, buf, 128))
        {
            switch (ProcessError(IDS_ERRCREATEDIR, 0, zipPath, PE_NOSKIP, NULL, buf))
            {
            case ERR_RETRY:
                retry = true;
                break;
            case ERR_CANCEL:
                error = IDS_NODISPLAY;
            }
        }
        else
        {
            SalamanderGeneral->GetDiskFreeSpace(&freesp, zipPath, NULL);
            if (freesp == CQuadWord(-1, -1))
            {
                switch (ProcessError(IDS_ERRGETDISKFREESP, 0, zipPath, PE_NOSKIP, NULL))
                {
                case ERR_RETRY:
                    retry = true;
                    break;
                case ERR_CANCEL:
                    error = IDS_NODISPLAY;
                }
            }
            else
            {
                flags = 0;
                text = NULL;
                if (freesp < CQuadWord(MIN_VOLSIZE, 0))
                {
                    text = LoadStr(IDS_TOOLOWSPACE);
                    flags |= LSD_NOIGNORE;
                }
                else
                {
                    if (Options.VolumeSize == -1)
                    {
                        if (freesp < CQuadWord(0xFFFFFFFF, 0))
                            DiskSize = freesp.Value;
                        else
                            DiskSize = 0xFFFFFFFF;
                        if (freesp < CQuadWord(SMALL_VOLSIZE, 0) && !firstSfxDisk && testSpace)
                        {
                            text = LoadStr(IDS_LOWSPACE2);
                        }
                    }
                    else
                    {
                        DiskSize = Options.VolumeSize;
                        if (firstSfxDisk)
                        {
                            //pro pripad non-removable disku, pro removable se testuje vyse
                            if (ArchiveDataOffs + MIN_VOLSIZE <= (QWORD)Options.VolumeSize)
                            {
                                DiskSize -= ArchiveDataOffs;
                            }
                            if (Removable)
                                firstSfxDisk = false;
                        }
                        if ((freesp.Value < DiskSize) && testSpace)
                        {
                            text = LoadStr(IDS_LOWSPACE);
                        }
                    }
                }
                testSpace = true;
                if (((flags & LSD_NOIGNORE) || !IgnoreAllFreeSp) && text)
                {
                    switch (LowDiskSpaceDialog(SalamanderGeneral->GetMsgBoxParent(), text, zipPath, freesp.Value,
                                               Options.VolumeSize, flags))
                    {
                    case IDC_ALL:
                        IgnoreAllFreeSp = true;
                    case IDC_IGNORE:
                        testSpace = false;
                        break;
                    case IDC_RETRY:
                        retry = true;
                        break;
                    case IDCANCEL:
                    default:
                        error = IDS_NODISPLAY;
                    }
                }
                if (!error && !retry)
                {
                    if (OverwriteAll)
                        overwrite = true;
                    TempFile->File = CreateFile(TempName, GENERIC_WRITE, /*FILE_SHARE_READ*/ NULL, NULL,
                                                overwrite ? CREATE_ALWAYS : CREATE_NEW,
                                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                    if (TempFile->File != INVALID_HANDLE_VALUE)
                    {
                        lstrcpy(TempFile->FileName, TempName);
                        TempFile->FilePointer = 0;
                        TempFile->Flags = PE_NOSKIP;
                        TempFile->RealFilePointer = 0;
                        TempFile->BufferPosition = 0;
                        return 0; //success
                    }
                    overwrite = false;
                    lastErr = GetLastError();
                    if (lastErr == ERROR_FILE_EXISTS || lastErr == ERROR_ALREADY_EXISTS)
                    {
                        CFile* file;
                        int ret;

                        ret = CreateCFile(&file, TempName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0, PE_NOSKIP, &SkipAllIOErrors,
                                          false, false);
                        switch (ret)
                        {
                        case ERR_NOERROR:
                        {
                            char attr[101];
                            FILETIME ft;

                            GetFileTime(file->File, NULL, NULL, &ft);
                            GetInfo(attr, &ft, file->Size);
                            switch (OverwriteDialog(SalamanderGeneral->GetMsgBoxParent(), TempName, attr))
                            {
                            case IDC_ALL:
                                OverwriteAll = true;
                            case IDC_YES:
                                overwrite = true;
                            case IDC_RETRY:
                                retry = true;
                                break;
                            case IDCANCEL:
                            default:
                                error = IDS_NODISPLAY;
                            }
                            CloseCFile(file);
                            break;
                        }

                        case ERR_CANCEL:
                            retry = true;
                            break;
                        case ERR_LOWMEM:
                            error = IDS_LOWMEM;
                        }
                    }
                    else
                    {
                        switch (ProcessError(IDS_ERRCREATE, lastErr, zipPath, PE_NOSKIP, NULL))
                        {
                        case ERR_RETRY:
                            retry = true;
                            break;
                        case ERR_CANCEL:
                            error = IDS_NODISPLAY;
                        }
                    }
                }
            }
        }
    } while (retry);
    free(TempFile->FileName);
    free(TempFile->OutputBuffer);
    free(TempFile);
    TempFile = NULL;
    return error;
}

int CZipPack::MatchAll()
{
    CALL_STACK_MESSAGE1("CZipPack::MatchAll()");
    CAddInfo* next;
    int j;

    AddTotalSize = CQuadWord(0, 0);
    NothingToDo = true;
    for (j = 0; j < AddFiles.Count; j++)
    {
        next = AddFiles[j];
        AddTotalSize += next->Size;
        NothingToDo = false;
    }
    ProgressTotalSize = AddTotalSize + CQuadWord(AddFiles.Count, 0);
    return 0;
}

int CZipPack::NextDisk()
{
    CALL_STACK_MESSAGE1("CZipPack::NextDisk()");
    char buf[256];
    INT_PTR i;

    if (TempFile)
    {
        //    SetEndOfFile(TempFile->File);
        if (Flush(TempFile, TempFile->OutputBuffer, TempFile->BufferPosition, NULL))
        {
            return IDS_NODISPLAY;
        }
        CloseCFile(TempFile);
        TempFile = NULL;
    }
    DiskNum++;
    if (Removable)
    {
        if (SeccondPass)
            sprintf(buf, LoadStr(IDS_CHDISKTEXT3), DiskNum + 1);
        else
            sprintf(buf, LoadStr(IDS_CHDISKTEXT), DiskNum, DiskNum + 1);
        i = ChangeDiskDialog(SalamanderGeneral->GetMsgBoxParent(), buf);
    }
    else
    {
        i = IDOK;
    }
    if (i == IDOK)
    {
        if (CreateNextFile())
        {
            return IDS_NODISPLAY;
        }
    }
    else
    {
        return IDS_NODISPLAY;
    }
    return 0;
}

int CZipPack::WriteSfxExecutable(const char* sfxFile, const char* sfxPackage, BOOL preview, int progressMode)
{
    CALL_STACK_MESSAGE5("CZipPack::WriteSfxExecutable(%s, %s, %d, %d)", sfxFile,
                        sfxPackage, preview, progressMode);
    CFile* sfx;
    char* buffer;
    int ret;
    unsigned int read;
    unsigned size;
    CSfxFileHeader sfxHead;

    //copy exetutable
    char package[MAX_PATH];
    GetModuleFileName(DLLInstance, package, MAX_PATH);
    SalamanderGeneral->CutDirectory(package);
    SalamanderGeneral->SalPathAppend(package, "sfx", MAX_PATH);
    SalamanderGeneral->SalPathAppend(package, sfxPackage, MAX_PATH);
    ret = CreateCFile(&sfx, package, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, PE_NOSKIP, NULL,
                      false, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    }
    ret = Read(sfx, &sfxHead, sizeof(CSfxFileHeader), &read, NULL);
    if (ret || read != sizeof(CSfxFileHeader) ||
        sfxHead.Signature != SFX_SIGNATURE ||
        sfxHead.CompatibleVersion != SFX_SUPPORTEDVERSION ||
        sfxHead.HeaderCRC != SalamanderGeneral->UpdateCrc32(&sfxHead, sizeof(CSfxFileHeader) - sizeof(DWORD), INIT_CRC))
    {
        CloseCFile(sfx);
        if (ret)
            ErrorID = IDS_NODISPLAY;
        else
        {
            if (sfxHead.CompatibleVersion != SFX_SUPPORTEDVERSION)
                ErrorID = IDS_BADSFXVER2;
            else
                ErrorID = IDS_CORRUPTSFX;
        }
        return ErrorID;
    }
    BOOL bigSfx = !preview && (Options.Encrypt || Options.Action & PA_MULTIVOL ||
                               Options.SfxSettings.Flags & SE_REMOVEAFTER && *Options.SfxSettings.WaitFor);
    if (bigSfx)
    {
        size = sfxHead.BigSfxSize;
        sfx->FilePointer = sfxHead.BigSfxOffset;
    }
    else
    {
        size = sfxHead.SmallSfxSize;
        sfx->FilePointer = sfxHead.SmallSfxOffset;
    }
    if (!preview && !(Options.Action & PA_MULTIVOL))
    {
        ProgressTotalSize += CQuadWord(size, 0);
        Salamander->ProgressDialogAddText(LoadStr(IDS_WRITINGEXE), FALSE);
        if (progressMode == 1)
        {
            Salamander->ProgressSetTotalSize(ProgressTotalSize, CQuadWord(-1, -1));
        }
        else
        {
            Salamander->ProgressSetTotalSize(CQuadWord(size, 0), ProgressTotalSize);
        }
    }
    buffer = (char*)malloc(size);
    char* buffer2 = (char*)malloc(64 * 1024);
    if (!buffer || !buffer2)
    {
        if (buffer)
            free(buffer);
        CloseCFile(sfx);
        return IDS_LOWMEM;
    }
    ret = Read(sfx, buffer, size, &read, NULL);
    if (ret || read != size)
    {
        if (ret)
            ErrorID = IDS_NODISPLAY;
        else
        {
            if (ret)
                ErrorID = IDS_NODISPLAY;
            else
                ErrorID = IDS_CORRUPTSFX;
        }
    }
    else
    {
        int ucsize = 64 * 1024;
        ErrorID = InflateBuffer(buffer, size, buffer2, &ucsize);
        if (ErrorID)
        {
            if (ErrorID == IDS_CORRUPTSFX2)
                ErrorID = IDS_CORRUPTSFX;
        }
        else
        {
            __UINT32 crc = SalamanderGeneral->UpdateCrc32(buffer2, ucsize, INIT_CRC);
            if (crc != (bigSfx ? sfxHead.BigSfxCRC : sfxHead.SmallSfxCRC))
                ErrorID = IDS_CORRUPTSFX;
            else
            {
                if (Write(TempFile, buffer2, ucsize, NULL))
                {
                    ErrorID = IDS_NODISPLAY;
                }
                else
                {
                    if (!preview && !(Options.Action & PA_MULTIVOL))
                    {
                        if (!Salamander->ProgressAddSize(size, TRUE))
                        {
                            UserBreak = true;
                            Salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING), FALSE);
                            Salamander->ProgressEnableCancel(FALSE);
                        }
                    }
                }
            }
        }
    }
    free(buffer);
    free(buffer2);
    CloseCFile(sfx);
    if (ErrorID)
        return ErrorID;
    if (Flush(TempFile, TempFile->OutputBuffer, TempFile->BufferPosition, NULL))
    {
        return IDS_NODISPLAY;
    }

    if (preview)
        return 0;

    CloseCFile(TempFile);
    TempFile = NULL;

    // load appropriate manifest
    DWORD manifestSize;
    void* manifest = LoadRCData((Options.SfxSettings.Flags & SE_REQUIRESADMIN)
                                    ? ID_SFX_MANIFEST_ADMIN
                                    : ID_SFX_MANIFEST_REGULAR,
                                manifestSize);
    if (!manifest)
        return ErrorID = IDS_ERROR; // tohle by se nikdy nemelo stat

    //change icon
    if (!ChangeSfxIconAndAddManifest(sfxFile, Options.Icons, Options.IconsCount,
                                     manifest, manifestSize))
        return ErrorID = IDS_ERRADDICON;

    ret = CreateCFile(&TempFile, sfxFile, GENERIC_WRITE, FILE_SHARE_READ,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                      false, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    }
    TempFile->FilePointer = TempFile->Size;

    //compute data offset
    ArchiveHeaderOffs = (unsigned)TempFile->Size;
    ArchiveDataOffs = ArchiveHeaderOffs + sizeof(CSelfExtrHeader);
    int l = lstrlen(Options.SfxSettings.Command);
    ArchiveDataOffs += l ? ++l : 0;
    l = lstrlen(Options.SfxSettings.Text);
    ArchiveDataOffs += ++l;
    l = lstrlen(Options.SfxSettings.Title);
    ArchiveDataOffs += ++l;

    unsigned td;
    const char* sd;
    const char* sdl;
    const char* sdr;
    //navratovku netestujem otestovano jiz drive
    ParseTargetDir(Options.SfxSettings.TargetDir, &td, &sd, &sdl, &sdr, NULL);
    l = lstrlen(sd);
    ArchiveDataOffs += ++l;
    l = lstrlen(Options.About);
    ArchiveDataOffs += ++l;
    l = lstrlen(Options.SfxSettings.ExtractBtnText);
    ArchiveDataOffs += ++l;
    l = lstrlen(Options.SfxSettings.Vendor);
    ArchiveDataOffs += ++l;
    l = lstrlen(Options.SfxSettings.WWW);
    ArchiveDataOffs += ++l;

    ArchiveDataOffs++; //predpokladame, ze ArchiveName je "", je-li jina, pricte se pozdeji lstrlen(ArchiveName)

    ArchiveDataOffs += (sdr - sdl) + 1;
    if (td == SE_REGVALUE)
    {
        ArchiveDataOffs += sizeof(LONG); //na zacatek ulozime jeste HKEY: i 64-bitova verze uklada jen 32-bitovy klic, jsou to jen zname rooty (napr. HKEY_CURRENT_USER), ktere jsou definovany jako 32-bit id-cka
        const char* bs = StrNChr(sdl, (int)(sdr - sdl), '\\');
        if (!bs)
            ArchiveDataOffs += 1; //pridame znak pro oddeleni subkey a value
    }
    l = lstrlen(Options.SfxSettings.MBoxTitle);
    ArchiveDataOffs += ++l;
    if (Options.SfxSettings.MBoxText)
    {
        l = lstrlen(Options.SfxSettings.MBoxText);
        ArchiveDataOffs += ++l;
    }
    else
        ArchiveDataOffs++;
    l = lstrlen(Options.SfxSettings.WaitFor);
    ArchiveDataOffs += Options.SfxSettings.Flags & SE_REMOVEAFTER ? ++l : 1;
    /*
  if (Options.Action & PA_MULTIVOL)
  {
    CSelfExtrHeader header;
    char archname[MAX_PATH];

    MakeFileName(1, Options.SeqNames, PathFindFileName(ZipName), archname);
    header.ArchiveNameLen = lstrlen(archname) + 1;
    ArchiveDataOffs += header.ArchiveNameLen;
    header.Signature = SELFEXTR_SIG;
    header.HeaderSize = ArchiveDataOffs - ArchiveHeaderOffs;
    header.Flags = Options.SfxSettings.Flags | SE_MULTVOL | (Options.SeqNames ? SE_SEQNAMES : 0);
    header.EOCentrDirOffs = 0;
    header.ArchiveSize = 0;
    header.CommandLen = lstrlen(Options.SfxSettings.Command);
    if (header.CommandLen) header.CommandLen++;
    header.TextLen = lstrlen(Options.SfxSettings.Text) + 1;
    //if (header.TextLen) header.QuestionLen++;
    header.TitleLen = lstrlen(Options.SfxSettings.Title) + 1;
    header.SubDirLen = lstrlen(Options.SfxSettings.SubDir) + 1;
    header.AboutLen = lstrlen(Options.About) + 1;
    header.ExtractBtnTextLen = lstrlen(Options.SfxSettings.ExtractBtnText) + 1;
    TempFile->FilePointer = ArchiveHeaderOffs;
    if (Write(TempFile, &header, sizeof(CSelfExtrHeader), NULL)) return IDS_NODISPLAY;
    if (header.CommandLen &&
        Write(TempFile, Options.SfxSettings.Command, header.CommandLen, NULL)) return IDS_NODISPLAY;
    if (Write(TempFile, Options.SfxSettings.Text, header.TextLen, NULL)) return IDS_NODISPLAY;
    if (Write(TempFile, Options.SfxSettings.Title, header.TitleLen, NULL)) return IDS_NODISPLAY;
    if (Write(TempFile, Options.SfxSettings.SubDir, header.SubDirLen, NULL)) return IDS_NODISPLAY;
    if (Write(TempFile, Options.About, header.AboutLen, NULL)) return IDS_NODISPLAY;
    if (Write(TempFile, Options.SfxSettings.ExtractBtnText, header.ExtractBtnTextLen, NULL)) return IDS_NODISPLAY;
    if (Write(TempFile, archname, header.ArchiveNameLen, NULL)) return IDS_NODISPLAY;
    if (Flush(TempFile, TempFile->OutputBuffer, TempFile->BufferPosition, NULL)) return IDS_NODISPLAY;
  }*/
    return ErrorID;
}

BOOL CZipPack::WriteSFXHeader(const char* archName, QWORD eoCentrDirOffs, DWORD archSize)
{
    CALL_STACK_MESSAGE4("CZipPack::WriteSFXHeader(%s, 0x%I64X, 0x%X)", archName, eoCentrDirOffs,
                        archSize);

    CSelfExtrHeader header;
    int offs = sizeof(CSelfExtrHeader);

    int l = lstrlen(Options.SfxSettings.Command);
    if (l)
    {
        header.CommandOffs = offs;
        offs += ++l;
    }
    else
        header.CommandOffs = 0;
    header.TextOffs = offs;
    l = lstrlen(Options.SfxSettings.Text);
    offs += ++l;
    header.TitleOffs = offs;
    l = lstrlen(Options.SfxSettings.Title);
    offs += ++l;
    header.SubDirOffs = offs;

    unsigned td;
    const char* sd;
    const char* sdl;
    const char* sdr;
    HKEY key;
    //navratovku netestujem otestovano jiz drive
    ParseTargetDir(Options.SfxSettings.TargetDir, &td, &sd, &sdl, &sdr, &key);
    l = lstrlen(sd);
    offs += ++l;
    header.AboutOffs = offs;
    l = lstrlen(Options.About);
    offs += ++l;
    header.ExtractBtnTextOffs = offs;
    l = lstrlen(Options.SfxSettings.ExtractBtnText);
    offs += ++l;
    header.VendorOffs = offs;
    l = lstrlen(Options.SfxSettings.Vendor);
    offs += ++l;
    header.WWWOffs = offs;
    l = lstrlen(Options.SfxSettings.WWW);
    offs += ++l;
    header.ArchiveNameOffs = offs;
    l = lstrlen(archName);
    ArchiveDataOffs += l; //tohle jsme predtim nezapocitaly
    offs += ++l;
    header.TargetDirSpecOffs = offs;
    offs += (int)(sdr - sdl) + 1;
    if (td == SE_REGVALUE)
    {
        offs += sizeof(LONG); // HKEY: je ulozeny na zacatku + i 64-bitova verze uklada jen 32-bitovy klic, jsou to jen zname rooty (napr. HKEY_CURRENT_USER), ktere jsou definovany jako 32-bit id-cka
        const char* bs = StrNChr(sdl, (int)(sdr - sdl), '\\');
        if (!bs)
            offs += 1; //pridame znak pro oddeleni subkey a value
    }
    header.MBoxStyle = (lstrlen(Options.SfxSettings.MBoxTitle) ||
                        Options.SfxSettings.MBoxText && lstrlen(Options.SfxSettings.MBoxText))
                           ? Options.SfxSettings.MBoxStyle
                           : -1;
    header.MBoxTitleOffs = offs;
    l = lstrlen(Options.SfxSettings.MBoxTitle);
    offs += ++l;
    header.MBoxTextOffs = offs;
    if (Options.SfxSettings.MBoxText)
    {
        l = lstrlen(Options.SfxSettings.MBoxText);
        offs += ++l;
    }
    else
        offs++;
    header.WaitForOffs = offs;
    l = lstrlen(Options.SfxSettings.WaitFor);
    offs += Options.SfxSettings.Flags & SE_REMOVEAFTER ? ++l : 1;

    header.Signature = SELFEXTR_SIG;
    header.HeaderSize = (int)(ArchiveDataOffs - ArchiveHeaderOffs);
    header.Flags = Options.SfxSettings.Flags | td;
    if (td == SE_TEMPDIREX)
        header.Flags |= SE_TEMPDIR;
    if (Options.Action & PA_MULTIVOL)
        header.Flags |= SE_MULTVOL | (Options.SeqNames ? SE_SEQNAMES : 0);
    header.EOCentrDirOffs = (__UINT32)eoCentrDirOffs;
    header.ArchiveSize = archSize;

    TempFile->FilePointer = ArchiveHeaderOffs;
    if (Write(TempFile, &header, sizeof(CSelfExtrHeader), NULL))
        return FALSE;
    l = lstrlen(Options.SfxSettings.Command);
    if (l &&
        Write(TempFile, Options.SfxSettings.Command, ++l, NULL))
        return FALSE;
    if (Write(TempFile, Options.SfxSettings.Text, lstrlen(Options.SfxSettings.Text) + 1, NULL))
        return FALSE;
    if (Write(TempFile, Options.SfxSettings.Title, lstrlen(Options.SfxSettings.Title) + 1, NULL))
        return FALSE;
    if (Write(TempFile, (void*)sd, lstrlen(sd) + 1, NULL))
        return FALSE;
    if (Write(TempFile, Options.About, lstrlen(Options.About) + 1, NULL))
        return FALSE;
    if (Write(TempFile, Options.SfxSettings.ExtractBtnText, lstrlen(Options.SfxSettings.ExtractBtnText) + 1, NULL))
        return FALSE;
    if (Write(TempFile, Options.SfxSettings.Vendor, lstrlen(Options.SfxSettings.Vendor) + 1, NULL))
        return FALSE;
    if (Write(TempFile, Options.SfxSettings.WWW, lstrlen(Options.SfxSettings.WWW) + 1, NULL))
        return FALSE;
    if (Write(TempFile, archName, lstrlen(archName) + 1, NULL))
        return FALSE;
    if (td == SE_REGVALUE)
    {
        //key - i 64-bitova verze uklada jen 32-bitovy klic, jsou to jen zname rooty (napr. HKEY_CURRENT_USER), ktere jsou definovany jako 32-bit id-cka
        LONG lkey = (LONG)(DWORD_PTR)key;
        if (Write(TempFile, &lkey, sizeof(lkey), NULL))
            return FALSE;

        //subkey
        const char* bs = StrRChr(sdl, sdr, '\\');
        if (bs)
        {
            if (Write(TempFile, (void*)sdl, (unsigned int)(bs - sdl), NULL))
                return FALSE;
        }
        if (Write(TempFile, "", 1, NULL))
            return FALSE;

        //value
        if (bs)
        {
            bs++;
            if (Write(TempFile, (void*)bs, (unsigned int)(sdr - bs), NULL))
                return FALSE;
        }
        else
        {
            if (Write(TempFile, (void*)sdl, (unsigned int)(sdr - sdl), NULL))
                return FALSE;
        }
        if (Write(TempFile, "", 1, NULL))
            return FALSE;
    }
    else
    {
        if (Write(TempFile, (void*)sdl, (unsigned int)(sdr - sdl), NULL))
            return FALSE;
        if (Write(TempFile, "", 1, NULL))
            return FALSE;
    }
    if (Write(TempFile, Options.SfxSettings.MBoxTitle, lstrlen(Options.SfxSettings.MBoxTitle) + 1, NULL))
        return FALSE;
    const char* str = Options.SfxSettings.MBoxText ? Options.SfxSettings.MBoxText : "";
    if (Write(TempFile, str, lstrlen(str) + 1, NULL))
        return FALSE;
    str = Options.SfxSettings.Flags & SE_REMOVEAFTER ? Options.SfxSettings.WaitFor : "";
    if (Write(TempFile, str, lstrlen(str) + 1, NULL))
        return FALSE;
    return TRUE;
}

/*
BOOL CZipPack::LoadDefaults()
{
  Options = DefOptions;

  // Config->DefSfxFile je "" kdyz nenajdeme zadny *.sfx, takze nelze balit sfx
  if (Config.DefSfxFile)
  {
    char file[MAX_PATH];
    GetModuleFileName(DLLInstance, file, MAX_PATH);
    PathRemoveFileSpec(file);
    PathAppend(file, "sfx");
    PathAppend(file, Config.DefSfxFile);
    if (DefLanguage && lstrcmp(DefLanguage->FileName, file))
    {
      delete DefLanguage;
      DefLanguage = NULL;
    }
    lstrcpy(Options.SfxSettings.SfxFile, file);
    if (!DefLanguage && LoadSfxFileData(file, &DefLanguage))
    {
      char err[512];
      sprintf(err, LoadStr(IDS_UNABLEREADSFX2), file);
      SalamanderGeneral->ShowMessageBox(err, LoadStr(IDS_ERROR), MSGBOX_ERROR);
      lstrcpy(Options.SfxSettings.Text, LoadStr(IDS_DEFAULTTEXT));
      lstrcpy(Options.SfxSettings.Title, LoadStr(IDS_DEFSFXTITLE));
      lstrcpy(Options.About, "Version 1.0 beta, Personal Edition\r\n\r\n"
                                  "Coded by Lukas Cerman\r\n"
                                  "E-mail: lukas.cerman@altap.cz\r\n\r\n"
                                  "ATTENTION: This selfextractor edition is licenced only for "
                                  "personal use and may not be used in business or for programs "
                                  "distribution.");
      lstrcpy(Options.SfxSettings.ExtractBtnText, LoadStr(IDS_DEFEXTRBUTTON));
    }
    else
    {
      lstrcpy(Options.SfxSettings.Text, DefLanguage->DlgText);
      lstrcpy(Options.SfxSettings.Title, DefLanguage->DlgTitle);
      lstrcpy(Options.About, DefLanguage->AboutFree);
      lstrcpy(Options.SfxSettings.ExtractBtnText, DefLanguage->ButtonText);
    }
    lstrcpy(Options.SfxSettings.SubDir, "");

    GetModuleFileName(DLLInstance, Options.SfxSettings.IconFile, MAX_PATH);
    Options.SfxSettings.IconIndex = -IDI_SFXICON;
    int ret = LoadIcons(Options.SfxSettings.IconFile, Options.SfxSettings.IconIndex,
                                    &Options.Icons, &Options.IconsCount);
    if (ret)
    {
      char buffer[1024];
      SalamanderGeneral->ShowMessageBox(FormatMessage(buffer, ret, GetLastError()),
                                        LoadStr(IDS_ERROR), MSGBOX_ERROR);
      return FALSE;
    }
  }

  return TRUE;
}
*/

int CZipPack::CheckArchiveForSFXCompatibility()
{
    CALL_STACK_MESSAGE1("CZipPack::CheckArchiveForSFXCompatibility()");

    ErrorID = CheckZip();
    if (ErrorID)
        return ErrorID;
    if (ZeroZip || /*EOCentrDir.*/ CentrDirSize == 0)
        return IDS_EMPTYARCHIVE;

    CFileHeader* header = (CFileHeader*)malloc(MAX_HEADER_SIZE);
    if (!header)
        return IDS_LOWMEM;
    __UINT64 offset = /*EOCentrDir.*/ CentrDirOffs + /*(unsigned) */ ExtraBytes;
    __UINT64 maxOffset = /*EOCentrDir.*/ CentrDirOffs + /*(unsigned) */ ExtraBytes + /*EOCentrDir.*/ CentrDirSize;
    int i;
    for (i = 0; offset < maxOffset && i <= 0xFFFF; i++)
    {
        QWORD qw = offset;
        ErrorID = ReadCentralHeader(header, &qw, NULL);
        offset = qw;
        if (ErrorID)
            break;
        if (header->Flag & GPF_ENCRYPTED)
            Options.Encrypt = true;
        if (header->Method != CM_STORED && header->Method != CM_DEFLATED)
        {
            ErrorID = IDS_BADMETHODSFX;
            break;
        }
        if (header->VersionExtr >= MIN_ZIP64_VERSION &&
            (header->Size == 0xFFFFFFFF ||
             header->CompSize == 0xFFFFFFFF ||
             header->LocHeaderOffs == 0xFFFFFFFF ||
             header->StartDisk == 0xFFFF))
        {
            // jde o Zip64 odmitneme ho
            ErrorID = IDS_ERRZIP64;
            break;
        }
    }
    if (i > 0xFFFF)
        ErrorID = IDS_LOTFILESSFX;
    free(header);
    return ErrorID;
}

int CZipPack::SaveComment()
{
    CALL_STACK_MESSAGE1("CZipPack::SaveComment()");
    if (ZipAttr & FILE_ATTRIBUTE_READONLY)
        return IDS_READONLY;
    // otevreme si ho pro zapis
    CloseCFile(ZipFile);
    ZipFile = NULL;
    int ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                          true, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
            ErrorID = IDS_LOWMEM;
        else
            ErrorID = IDS_NODISPLAY;
    }

    EONewCentrDir.CommentLen = (__UINT16)strlen(Comment);
    ZipFile->FilePointer = EOCentrDirOffs;
    if (Write(ZipFile, &EONewCentrDir, sizeof(CEOCentrDirRecord), NULL))
    {
        return IDS_NODISPLAY;
    }
    if (Write(ZipFile, Comment, EONewCentrDir.CommentLen, NULL))
    {
        return IDS_NODISPLAY;
    }
    if (Flush(ZipFile, ZipFile->OutputBuffer, ZipFile->BufferPosition, NULL))
    {
        return IDS_NODISPLAY;
    }
    SetEndOfFile(ZipFile->File);
    return 0;
}

int CZipPack::FindNewestFile()
{
    CALL_STACK_MESSAGE1("CZipPack::FindNewestFile()");
    CFileHeader* header;
    header = (CFileHeader*)NewCentrDir;
    for (;
         (char*)header < NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize;
         header = (CFileHeader*)((char*)header + sizeof(CFileHeader) + header->NameLen +
                                 header->ExtraLen + header->CommentLen))
    {
        if ((char*)header + sizeof(CFileHeader) > NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize ||
            (char*)header + sizeof(CFileHeader) + header->NameLen + header->ExtraLen +
                    header->CommentLen >
                NewCentrDir + /*EONewCentrDir.*/ NewCentrDirSize ||
            header->Signature != SIG_CENTRFH)
        {
            Fatal = true;
            return IDS_ERRFORMAT;
        }
        FILETIME lft, ft;
        if (!DosDateTimeToFileTime(header->Date, header->Time, &lft))
        {
            SystemTimeToFileTime(&MinZipTime, &lft);
        }
        LocalFileTimeToFileTime(&lft, &ft);
        if (ft.dwHighDateTime > NewestFileTime.dwHighDateTime ||
            ft.dwHighDateTime == NewestFileTime.dwHighDateTime &&
                ft.dwLowDateTime > NewestFileTime.dwLowDateTime)
        {
            NewestFileTime = ft;
        }
    }
    return 0;
}

int CZipPack::WriteSFXECRec(QWORD offset)
{
    CALL_STACK_MESSAGE2("CZipPack::WriteSFXECRec(0x%I64X)", offset);
    if (Removable && DiskNum != 0)
    {
        char buf[256];
        sprintf(buf, LoadStr(IDS_CHDISKTEXT3), 1);
        if (ChangeDiskDialog(SalamanderGeneral->GetMsgBoxParent(), buf) != IDOK)
        {
            return IDS_NODISPLAY;
        }
    }
    DiskNum = 0;

    char name[MAX_PATH];
    lstrcpy(name, ZipName);
    if (!SalamanderGeneral->SalPathRenameExtension(name, ".exe", MAX_PATH))
    {
        Salamander->CloseProgressDialog();
        return IDS_TOOLONGZIPNAME;
    }

    CFile* file;
    int ret = CreateCFile(&file, name, GENERIC_WRITE, FILE_SHARE_READ,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                          false, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
            ErrorID = IDS_LOWMEM;
        else
            ErrorID = IDS_NODISPLAY;
        return ErrorID;
    }

    file->FilePointer = offset;
    if (Write(file, &EONewCentrDir, sizeof(CEOCentrDirRecord), NULL) ||
        Flush(file, file->OutputBuffer, file->BufferPosition, NULL))
    {
        ret = IDS_NODISPLAY;
    }
    CloseCFile(file);
    return 0;
}

int CZipPack::WriteSFXCentralDir()
{
    CALL_STACK_MESSAGE1("CZipPack::WriteSFXCentralDir()");
    if (Removable && DiskNum != EONewCentrDir.StartDisk)
    {
        char buf[256];
        sprintf(buf, LoadStr(IDS_CHDISKTEXT3), EONewCentrDir.StartDisk + 1);
        if (ChangeDiskDialog(SalamanderGeneral->GetMsgBoxParent(), buf) != IDOK)
        {
            return IDS_NODISPLAY;
        }
    }
    DiskNum = EONewCentrDir.StartDisk;
    SeccondPass = TRUE;
    int ret = CreateNextFile();
    if (ret)
        ret;

    TempFile->FilePointer = 4; //preskocime signaturu
    return FinishPack(FPR_WRITE);
}
