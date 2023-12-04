// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <tchar.h>
#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
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
#include "zip.rh"
#include "zip.rh2"
#include "lang\lang.rh"
#include "chicon.h"
#include "common.h"
#include "chicon.h"
#include "add_del.h"
#include "deflate.h"
#include "crypt.h"
#include "dialogs.h"
//#include "sfxmake/sfxmake.h"

int CZipPack::PackToArchive(BOOL move, const char* sourcePath,
                            SalEnumSelection2 next, void* param)
{
    CALL_STACK_MESSAGE3("CZipPack::PackToArchive(%d, %s, , )", move, sourcePath);
    bool noexist = false;
    unsigned flags = 0;

    /*HWND dlg = CreateDialog(HLanguage, MAKEINTRESOURCE(IDD_POKUS),
                          GetParent(), PokusDlg);
  if (dlg)
  {
    MSG msg;
    BOOL ret;
    while (ret = GetMessage(&msg, NULL, 0, 0))// && IsWindow(Dlg))
    {
      if (//!TranslateAccelerator(dlg, Accel, &msg) &&
          !IsDialogMessage(dlg, &msg))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }*/

    ZipAttr = SalamanderGeneral->SalGetFileAttributes(ZipName);
    if (ZipAttr == 0xFFFFFFFF)
    {
        int err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
        {
            ZipAttr = FILE_ATTRIBUTE_NORMAL;
            noexist = true;
        }
        else
        {
            ProcessError(IDS_ERRACCESS, err, ZipName, PE_NORETRY | PE_NOSKIP, NULL);
            return ErrorID = IDS_NODISPLAY;
        }
    }
    if (ZipAttr & FILE_ATTRIBUTE_READONLY)
    {
        return ErrorID = IDS_READONLY;
    }
    DetectRemovable();

    flags |= (noexist ? PD_NEWARCHIVE : 0) | (Removable ? PD_REMOVALBE : 0);

    if ((ErrorID = LoadExPackOptions(flags)) != 0)
        return ErrorID;
    if (Options.Encrypt && Config.Level < 1)
        return ErrorID = IDS_ECRYPTSTORED;
    if ((Options.Action & PA_SELFEXTRACT) && Options.Encrypt && (Config.EncryptMethod > EM_ZIP20))
        return ErrorID = IDS_ECRYPTSFX;
    if (Options.Action & PA_MULTIVOL && Config.Level < 1)
        return ErrorID = IDS_MULTISTORED;

    SourcePath = sourcePath;
    SourceLen = lstrlen(SourcePath);
    if (*(SourcePath + SourceLen - 1) == '\\')
        SourceLen--;
    SkipAllIOErrors = false;
    Move = move ? true : false;
    Pack = true;

    if (*OriginalCurrentDir)
        SetCurrentDirectory(sourcePath);
    switch (Options.Action)
    {
    case PA_NORMAL:
        ErrorID = PackNormal(next, param);
        break;
    case PA_SELFEXTRACT:
        ErrorID = PackSelfExtract(next, param);
        break;
    case PA_MULTIVOL | PA_SELFEXTRACT:
    case PA_MULTIVOL:
        //if (noexist)
        ErrorID = PackMultiVol(next, param);
        //else
        // ErrorID = IDS_CANTMULTIVOL;
        break;
    }
    return ErrorID;
}

int CZipPack::DeleteFromArchive(SalEnumSelection next, void* param)
{
    CALL_STACK_MESSAGE1("CZipPack::DeleteFromArchive(, )");
    TIndirectArray2<CExtInfo> delNames(256);
    int dirs;
    int filesDeleted;
    int filesInRoot;
    CFileInfo rootDir;
    char* buffer = NULL;

    rootDir.Name = NULL;

    //  if (ErrorID = LoadConfig())
    //    return ErrorID;
    Pack = false;
    ZipAttr = SalamanderGeneral->SalGetFileAttributes(ZipName);
    if (ZipAttr == 0xFFFFFFFF)
    {
        ProcessError(IDS_ERRACCESS, GetLastError(), ZipName, PE_NORETRY | PE_NOSKIP, NULL);
        return ErrorID = IDS_NODISPLAY;
    }
    if (ZipAttr & FILE_ATTRIBUTE_READONLY)
    {
        return ErrorID = IDS_READONLY;
    }
    int ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL, true, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    }
    Options.Action = PA_NORMAL;
    Options.Encrypt = false;
    TCHAR title[1024];
    _stprintf(title, LoadStr(IDS_DELPROGTITLE), SalamanderGeneral->SalPathFindFileName(ZipName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);
    ErrorID = CheckZip();
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
            //      RootLen = lstrlen(ZipRoot);
            if (EOCentrDir.DiskNum)
            {
                ErrorID = IDS_MULDISK;
                Fatal = true;
            }
            else
            {
                ErrorID = EnumFiles(delNames, dirs, next, param);
                if (!ErrorID)
                {
                    ErrorID = LoadCentralDirectory();
                    if (!ErrorID)
                    {
                        MatchedTotalSize = CQuadWord(0, 0);
                        ErrorID = CZipCommon::MatchFiles(DelFiles, delNames, dirs, NewCentrDir);
                        if (!ErrorID && DelFiles.Count)
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
                            //Salamander->ProgressSetTotalSize(0, AssumedDelProgress);
                            bool exist;
                            if (RootLen && !Config.NoEmptyDirs)
                                ErrorID = CountFilesInRoot(&filesInRoot, &exist);
                            if (!ErrorID)
                            {
                                ErrorID = DeleteFiles(&filesDeleted);
                                if (ErrorID && !Config.BackupZip)
                                    Recover();
                                if (!ErrorID && (!UserBreak || UserBreak && !Config.BackupZip))
                                {
                                    if (RootLen && !Config.NoEmptyDirs && filesDeleted == filesInRoot && !exist)
                                    {
                                        rootDir.CompSize = 0;
                                        rootDir.Crc = 0;
                                        rootDir.FileAttr = FILE_ATTRIBUTE_DIRECTORY;
                                        rootDir.Flag = 0;
                                        rootDir.InterAttr = 0;
                                        rootDir.IsDir = true;
                                        GetFileTime(ZipFile->File, NULL, NULL, &rootDir.LastWrite);
                                        rootDir.LocHeaderOffs = /*EONewCentrDir.*/ NewCentrDirOffs;
                                        rootDir.Method = CM_STORED;
                                        rootDir.Size = 0;
                                        rootDir.StartDisk = 0;
                                        TempFile->FilePointer = /*EONewCentrDir.*/ NewCentrDirOffs;
                                        rootDir.Name = (char*)malloc(lstrlen(ZipRoot) + 1);
                                        buffer = (char*)malloc(MAX_HEADER_SIZE);
                                        if (buffer && rootDir.Name)
                                        {
                                            lstrcpy(rootDir.Name, ZipRoot);
                                            rootDir.NameLen = RootLen;
                                            ErrorID = WriteLocalHeader(&rootDir, buffer);
                                            /*EONewCentrDir.*/ NewCentrDirOffs = /*(unsigned) */ TempFile->FilePointer;
                                        }
                                        else
                                            ErrorID = IDS_LOWMEM;
                                    }
                                    if (!ErrorID)
                                    {
                                        ErrorID = WriteCentrDir();
                                        if (!ErrorID)
                                        {
                                            if (RootLen && !Config.NoEmptyDirs && filesDeleted == filesInRoot && !exist)
                                            {
                                                lstrcpy(rootDir.Name, ZipRoot);
                                                ErrorID = WriteCentralHeader(&rootDir, buffer, TRUE, FPR_NORMAL);
                                            }
                                            if (!ErrorID)
                                            {
                                                ErrorID = WriteEOCentrDirRecord();
                                                if (!ErrorID)
                                                {
                                                    if (!Flush(TempFile, TempFile->OutputBuffer, TempFile->BufferPosition, NULL))
                                                    {
                                                        SetEndOfFile(TempFile->File);
                                                    }
                                                    else
                                                        ErrorID = IDS_NODISPLAY;
                                                }
                                            }
                                        }
                                    }
                                    if (buffer)
                                        free(buffer);
                                    //if (rootDir.Name) free(rootDir.Name); smaze se to samo, rootDir ma jiz destructor
                                }
                            }
                        }
                        free(NewCentrDir);
                    }
                }
            }
            if (Config.BackupZip)
            {
                if (ErrorID || UserBreak)
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
                if (!ErrorID && Config.TimeToNewestFile &&
                    NewestFileTime.dwLowDateTime != 0 && NewestFileTime.dwHighDateTime != 0)
                    SetFileTime(ZipFile->File, NULL, NULL, &NewestFileTime);
            }
        }
    }
    Salamander->CloseProgressDialog();
    return ErrorID;
}

BOOL CZipPack::LoadDefaults()
{
    CALL_STACK_MESSAGE1("CZipPack::LoadDefaults()");
    Options = DefOptions;
    //Options.SfxSettings.IconIndex = -IDI_SFXICON;

    // Config->DefSfxFile je "" kdyz nenajdeme zadny *.sfx, takze nelze balit sfx
    if (*Config.DefSfxFile)
    {
        if (DefLanguage && lstrcmp(DefLanguage->FileName, Config.DefSfxFile))
        {
            delete DefLanguage;
            DefLanguage = NULL;
        }
        lstrcpy(Options.SfxSettings.SfxFile, Config.DefSfxFile);
        char file[MAX_PATH];
        GetModuleFileName(DLLInstance, file, MAX_PATH);
        SalamanderGeneral->CutDirectory(file);
        SalamanderGeneral->SalPathAppend(file, "sfx", MAX_PATH);
        SalamanderGeneral->SalPathAppend(file, Config.DefSfxFile, MAX_PATH);
        if (!DefLanguage && LoadSfxFileData(file, &DefLanguage))
        {
            char err[512];
            sprintf(err, LoadStr(IDS_UNABLEREADSFX2), file);
            SalamanderGeneral->ShowMessageBox(err, LoadStr(IDS_ERROR), MSGBOX_ERROR);
            lstrcpy(Options.SfxSettings.Text, LoadStr(IDS_DEFAULTTEXT));
            lstrcpy(Options.SfxSettings.Title, LoadStr(IDS_DEFSFXTITLE));
            lstrcpy(Options.About, "Version 1.40");
            lstrcpy(Options.SfxSettings.ExtractBtnText, LoadStr(IDS_DEFEXTRBUTTON));
            lstrcpy(Options.SfxSettings.Vendor, "Self-Extractor © 2000-2023 Open Salamander Authors");
            lstrcpy(Options.SfxSettings.WWW, "https://www.altap.cz");
        }
        else
        {
            lstrcpy(Options.SfxSettings.Text, DefLanguage->DlgText);
            lstrcpy(Options.SfxSettings.Title, DefLanguage->DlgTitle);
            lstrcpyn(Options.About, DefLanguage->AboutLicenced, SE_MAX_ABOUT);
            lstrcpy(Options.SfxSettings.ExtractBtnText, DefLanguage->ButtonText);
            lstrcpy(Options.SfxSettings.Vendor, DefLanguage->Vendor);
            lstrcpy(Options.SfxSettings.WWW, DefLanguage->WWW);
        }
        lstrcpy(Options.SfxSettings.TargetDir, "");

        GetModuleFileName(DLLInstance, Options.SfxSettings.IconFile, MAX_PATH);
        Options.SfxSettings.IconIndex = -IDI_SFXICON;
        int ret = LoadIcons(Options.SfxSettings.IconFile, Options.SfxSettings.IconIndex,
                            &Options.Icons, &Options.IconsCount);
        switch (ret)
        {
        case 1:
            ret = IDS_ERROPENICO;
            break;
        case 2:
            ret = IDS_ERRLOADLIB;
            break;
        case 3:
            ret = IDS_ERRLOADLIB2;
            break;
        case 4:
            ret = IDS_ERRLOADICON;
            break;
        }
        if (ret)
        {
            char buffer[1024];
            int e = GetLastError();
            SalamanderGeneral->ShowMessageBox(FormatMessage(buffer, ret, e), LoadStr(IDS_ERROR), MSGBOX_ERROR);
            return FALSE;
        }
    }

    return TRUE;
}

int CZipPack::LoadExPackOptions(unsigned flags)
{
    CALL_STACK_MESSAGE2("CZipPack::LoadExPackOptions(0x%X)", flags);

    if (!LoadDefaults())
        return IDS_NODISPLAY;
    if (Config.ShowExOptions)
    {
        if (PackDialog(SalamanderGeneral->GetMainWindowHWND(), this, &Config, &Options, ZipName, flags) != IDOK)
        {
            //nebudeme ukladat zachovame starou VolSizeCache
            int i;
            for (i = 0; i < 5; i++)
            {
                memcpy(Config.VolSizeCache[i], ::Config.VolSizeCache[i], MAX_VOL_STR);
                Config.VolSizeUnits[i] = ::Config.VolSizeUnits[i];
            }
            Config.LastUsedAuto = ::Config.LastUsedAuto;
            ::Config = Config;
            return ErrorID = IDS_NODISPLAY;
        }
        else
            ::Config = Config;
    }
    if (Options.SfxSettings.Flags & SE_AUTO)
        Options.SfxSettings.Flags |= SE_NOTALLOWCHANGE;
    if (Options.Action & PA_SELFEXTRACT)
    {
        LastUsedSfxSet.Settings = Options.SfxSettings;
        //to jmeno je jen pro interni potrebu (nekde dal se to testuje na "")
        lstrcpy(LastUsedSfxSet.Name, "Last Used");
    }
    return 0;
}

int CZipPack::CreateSFX()
{
    CALL_STACK_MESSAGE1("CZipPack::CreateSFX()");
    char exeName[MAX_PATH];

    //MenuSfx = true;
    if (!*Config.DefSfxFile)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_NOSFXINSTALLED), LoadStr(IDS_ERROR), MSGBOX_ERROR);
        return ErrorID = IDS_NODISPLAY;
    }

    if (!LoadDefaults())
        return ErrorID = IDS_NODISPLAY;

    lstrcpy(exeName, ZipName);
    SalamanderGeneral->SalPathRenameExtension(exeName, ".exe", MAX_PATH);
    CCreateSFXDialog dlg(SalamanderGeneral->GetMainWindowHWND(), ZipName, exeName, &Options, this);
    if (dlg.Proceed() != IDOK)
        return ErrorID = IDS_NODISPLAY;

    LastUsedSfxSet.Settings = Options.SfxSettings;
    //to jmeno je jen pro interni potrebu (nekde dal se to testuje na "")
    lstrcpy(LastUsedSfxSet.Name, "Last Used");

    if (Options.SfxSettings.Flags & SE_AUTO)
        Options.SfxSettings.Flags |= SE_NOTALLOWCHANGE;

    int ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
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
    ErrorID = CheckArchiveForSFXCompatibility();
    if (ErrorID)
        return ErrorID;

    if (TestIfExist(exeName))
        return ErrorID;

    ret = CreateCFile(&TempFile, exeName, GENERIC_WRITE, FILE_SHARE_READ,
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                      false, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    }

    char title[1024];
    sprintf(title, LoadStr(IDS_SFXPROGTITLE), SalamanderGeneral->SalPathFindFileName(exeName));
    Salamander->OpenProgressDialog(title, FALSE, NULL, FALSE);
    ProgressTotalSize = CQuadWord().SetUI64(ZipFile->Size);
    ErrorID = WriteSfxExecutable(exeName, Options.SfxSettings.SfxFile, FALSE, 1);
    if (!ErrorID)
    {
        if (!WriteSFXHeader("", unsigned(EOCentrDirOffs + ExtraBytes), (unsigned)ZipFile->Size))
            ErrorID = IDS_NODISPLAY;
        else
        {
            Salamander->ProgressDialogAddText(LoadStr(IDS_COPYDATA), FALSE);
            char* buffer = (char*)malloc(DECOMPRESS_INBUFFER_SIZE);
            if (!buffer)
                ErrorID = IDS_LOWMEM;
            {
                ErrorID = MoveData(ArchiveDataOffs, 0, ZipFile->Size, buffer);
                if (!ErrorID && Flush(TempFile, TempFile->OutputBuffer, TempFile->BufferPosition, NULL))
                    ErrorID = IDS_NODISPLAY;
            }
            free(buffer);
        }
    }

    Salamander->CloseProgressDialog();
    CloseCFile(TempFile);
    if (ErrorID)
        DeleteFile(exeName);

    return ErrorID;
}

int CZipPack::CommentArchive()
{
    CALL_STACK_MESSAGE1("CZipPack::CommentArchive()");

    int ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                          true, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
            ErrorID = IDS_LOWMEM;
        else
            ErrorID = IDS_NODISPLAY;
        return ErrorID;
    }

    ZipAttr = SalamanderGeneral->SalGetFileAttributes(ZipName);
    if (ZipAttr == 0xFFFFFFFF)
    {
        ProcessError(IDS_ERRACCESS, GetLastError(), ZipName, PE_NORETRY | PE_NOSKIP, NULL);
        return ErrorID = IDS_NODISPLAY;
    }
    Extract = true;
    ErrorID = CheckZip();
    if (ErrorID)
        return ErrorID;

    if (MultiVol)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_COMMENTMV), LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
    }

    EONewCentrDir = EOCentrDir;
    NewCentrDirSize = CentrDirSize;
    NewCentrDirOffs = CentrDirOffs;

    void* temp = realloc(Comment, MAX_ZIPCOMMENT);
    if (!temp)
        return ErrorID = IDS_LOWMEM;
    Comment = (char*)temp;
    Comment[EONewCentrDir.CommentLen] = 0; // pro jistotu

    CCommentDialog dlg(SalamanderGeneral->GetMainWindowHWND(), this);
    if (dlg.Proceed() != IDOK)
        return ErrorID = IDS_NODISPLAY;

    return ErrorID;
}
