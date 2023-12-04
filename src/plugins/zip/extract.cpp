// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <tchar.h>
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
#include <stdio.h>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_file.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "config.h"
#include "common.h"
#include "zip.rh"
#include "zip.rh2"
#include "lang\lang.rh"
#include "extract.h"
#include "inflate.h"
#include "explode.h"
#include "unshrink.h"
#include "unreduce.h"
#include "unbzip2.h"
#include "crypt.h"
#include "add_del.h"
#include "dialogs.h"

// Used in CZipUnpack::ExtractFiles & CZipUnpack::ExtractSingleFile when testing archive. For simplicity reasons we assume this is enough ;-)

#define ZIP_MAX_PATH 1024

CZipUnpack::CZipUnpack(const char* zipName, const char* zipRoot, CSalamanderForOperationsAbstract* salamander,
                       TIndirectArray2<char>* archiveVolumes) : CZipCommon(zipName, zipRoot, salamander, archiveVolumes), Passwords(8)
{
    CALL_STACK_MESSAGE3("CZipUnpack::CZipUnpack(%s, %s, )", zipName, zipRoot);
    Heap = HeapCreate(HEAP_NO_SERIALIZE, INITIAL_HEAP_SIZE, MAXIMUM_HEAP_SIZE);
    if (!Heap)
        ErrorID = IDS_LOWMEM;

    OutputBuffer = NULL;
    Extract = true;
    Unshrinking = false;
    Test = false;
    AllocateWholeFile = true;
    TestAllocateWholeFile = true;
}

int CZipUnpack::UnpackArchive(const char* targetDir, SalEnumSelection next, void* param)
{
    CALL_STACK_MESSAGE2("CZipUnpack::UnpackArchive(%s, , )", targetDir);
    TIndirectArray2<CExtInfo> extrNames(256);  //file names to be extracted
    TIndirectArray2<CFileInfo> extrFiles(256); //file header of files to be extracted
    int dirs;

    int ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                          true, false);
    if (ret)
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    char title[1024];
    sprintf(title, LoadStr(IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(ZipName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);
    ErrorID = CheckZip();
    if (!ErrorID && !ZeroZip)
    {
        MatchedTotalSize = CQuadWord(0, 0);
        ExtrFiles = &extrFiles;
        ErrorID = EnumFiles(extrNames, dirs, next, param);
        if (!ErrorID)
        {
            ErrorID = MatchFiles(extrFiles, extrNames, dirs, NULL);
            if (ErrorID && extrFiles.Count)
            {
                if (ErrorID != IDS_NODISPLAY)
                    SalamanderGeneral->ShowMessageBox(LoadStr(ErrorID), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                ErrorID = 0;
            }
            ProgressTotalSize = MatchedTotalSize;
            if (!ErrorID && extrFiles.Count)
            {
                if (MultiVol)
                {
                    int left = 0, right = 0, i = 0;

                    QuickSortHeaders2(0, extrFiles.Count - 1, extrFiles);
                    while (i < extrFiles.Count)
                    {
                        while (i < extrFiles.Count &&
                               extrFiles[i]->StartDisk == extrFiles[left]->StartDisk)
                        {
                            right = i;
                            i++;
                        }
                        QuickSortHeaders(left, right, extrFiles);
                        left = ++right;
                    }
                }
                else
                {
                    QuickSortHeaders(0, extrFiles.Count - 1, extrFiles);
                }
                if (*OriginalCurrentDir)
                    SetCurrentDirectory(targetDir);
                ErrorID = ExtractFiles(targetDir);
            }
        }
    }
    Salamander->CloseProgressDialog();
    return ErrorID;
}

int CZipUnpack::UnpackOneFile(const char* nameInZip, const CFileData* fileData, const char* targetPath, const char* newFileName)
{
    CALL_STACK_MESSAGE3("CZipUnpack::UnpackOneFile(%s, , %s)", nameInZip, targetPath);
    CFileInfo fileInfo;
    TCHAR targetDir[MAX_PATH + 1];
    int targetDirLen;
    char* sour;
    CZIPFileData* zipFileData = (CZIPFileData*)fileData->PluginData;

    Unix = zipFileData->Unix;

    int ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL, true, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    }
    ErrorID = CheckZip();
    if (!ErrorID && !ZeroZip)
    {
        ErrorID = FindFile(nameInZip, &fileInfo, zipFileData->ItemNumber);
        if (!ErrorID)
        {
            lstrcpy(targetDir, targetPath);
            targetDirLen = lstrlen(targetDir);
            if (targetDirLen && targetDir[targetDirLen - 1] == '\\')
            {
                targetDir[targetDirLen - 1] = 0;
                targetDirLen--;
            }
            fixed_tl64 = NULL;
            fixed_td64 = NULL;
            fixed_tl32 = NULL;
            fixed_td32 = NULL;
            SkipAllIOErrors = 0;
            SkipAllLongNames = 0;
            SkipAllEncrypted = 0;
            SkipAllDataErr = 0;
            SkipAllBadMathods = 0;
            Silent = 0;
            DialogFlags = PE_NOSKIP;
            //fileInfo.FileAttr |= FILE_ATTRIBUTE_TEMPORARY;
            sour = fileInfo.Name + fileInfo.NameLen;
            while (sour > fileInfo.Name)
            {
                if (*sour == '\\')
                    break;
                sour--;
            }
            if (sour > fileInfo.Name)
                RootLen = (int)(sour - fileInfo.Name);
            else
                RootLen = 0;
            InputBuffer = (char*)malloc(DECOMPRESS_INBUFFER_SIZE);
            InBufSize = DECOMPRESS_INBUFFER_SIZE;
            SlideWindow = (char*)malloc(SLIDE_WINDOW_SIZE);
            WinSize = SLIDE_WINDOW_SIZE;
            if (InputBuffer && SlideWindow)
            {
                ErrorID = ExtractSingleFile(targetDir, targetDirLen, &fileInfo, NULL, newFileName);
                InflateFreeFixedHufman();
                free(InputBuffer);
                free(SlideWindow);
            }
            else
            {
                if (InputBuffer)
                    free(InputBuffer);
                if (SlideWindow)
                    free(SlideWindow);
                ErrorID = IDS_LOWMEM;
            }
            //free(fileInfo.Name); uvolni ho destructor
        }
    }
    return ErrorID;
}

int CZipUnpack::UnpackWholeArchive(const char* mask, const char* targetDir)
{
    CALL_STACK_MESSAGE3("CZipUnpack::UnpackWholeArchive(%s, %s)", mask, targetDir);
    TIndirectArray2<char> maskArray(16);
    TIndirectArray2<CFileInfo> extrFiles(256); //file header of files to be extracted

    int ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                          true, false);
    if (ret)
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    char title[1024];
    sprintf(title, LoadStr(Test ? IDS_TESTPROGTITLE : IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(ZipName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);
    ErrorID = CheckZip();
    if (!ErrorID && ArchiveVolumes != NULL)
        ArchiveVolumes->Add(_strdup(ZipName));
    if (!ErrorID && !ZeroZip)
    {
        MatchedTotalSize = CQuadWord(0, 0);
        ExtrFiles = &extrFiles;
        //errorID = EnumFiles(&extrNames, next, param, &extrInfo._CommonInfo);
        ErrorID = PrepareMaskArray(maskArray, mask);
        if (!ErrorID && maskArray.Count)
        {
            ErrorID = MatchFilesToMask(maskArray);
            if (ErrorID && extrFiles.Count)
            {
                if (ErrorID != IDS_NODISPLAY)
                    SalamanderGeneral->ShowMessageBox(LoadStr(ErrorID), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                ErrorID = 0;
            }
            ProgressTotalSize = MatchedTotalSize;
            if (!ErrorID && extrFiles.Count)
            {
                if (MultiVol)
                {
                    int left = 0,
                        right = 0,
                        i = 0;

                    QuickSortHeaders2(0, extrFiles.Count - 1, extrFiles);
                    while (i < extrFiles.Count)
                    {
                        while (i < extrFiles.Count &&
                               extrFiles[i]->StartDisk == extrFiles[left]->StartDisk)
                        {
                            right = i;
                            i++;
                        }
                        QuickSortHeaders(left, right, extrFiles);
                        left = ++right;
                    }
                }
                else
                {
                    QuickSortHeaders(0, extrFiles.Count - 1, extrFiles);
                }
                if (*OriginalCurrentDir && !Test)
                    SetCurrentDirectory(targetDir);
                ErrorID = ExtractFiles(targetDir);
            }
        }
    }
    Salamander->CloseProgressDialog();
    return ErrorID;
}

int CZipUnpack::FindFile(LPCTSTR name, CFileInfo* fileInfo, int nItem)
{
    CALL_STACK_MESSAGE2("CZipUnpack::FindFile(%s, )", name);
    CFileHeader* centralHeader;
    char* tempName;
    int errorID = 0;
    QWORD readOffset;
    QWORD readSize;
    DWORD pathFlag = Unix ? 0 : NORM_IGNORECASE;
    BOOL found = FALSE;

    centralHeader = (CFileHeader*)malloc(MAX_HEADER_SIZE);
    tempName = (char*)malloc(MAX_HEADER_SIZE);
    if (!centralHeader || !tempName)
    {
        if (centralHeader)
            free(centralHeader);
        if (tempName)
            free(tempName);
        return IDS_LOWMEM;
    }
    readOffset = CentrDirOffs + ExtraBytes;
    readSize = 0;
    if (DiskNum != CentrDirStartDisk && MultiVol)
    {
        DiskNum = CentrDirStartDisk;
        errorID = ChangeDisk();
    }
    int i;
    for (i = 0; readSize < CentrDirSize && !errorID; i++)
    {
        unsigned int s;

        errorID = ReadCentralHeader(centralHeader, &readOffset, &s);
        if (errorID)
            break;
        readSize += s;
        if (i == nItem)
        {
            unsigned int tempNameLen = ProcessName(centralHeader, tempName);
            unsigned int nameLen = lstrlen(name);

            if (tempNameLen == nameLen)
            {
                LPCTSTR str = _tcsrchr(name, '\\');
                int pathLen;
                pathLen = str ? (int)(str - name + 1) : 0;
                if ((CompareString(LOCALE_USER_DEFAULT, pathFlag,
                                   name, pathLen,
                                   tempName, pathLen) == CSTR_EQUAL) &&
                    (CompareString(LOCALE_USER_DEFAULT, 0,
                                   name + pathLen, nameLen - pathLen,
                                   tempName + pathLen, nameLen - pathLen) == CSTR_EQUAL))
                {
                    ProcessHeader(centralHeader, fileInfo);
                    if (!fileInfo->IsDir)
                    {
                        fileInfo->NameLen = tempNameLen;
                        fileInfo->Name = _tcsdup(tempName);
                        if (!fileInfo->Name)
                        {
                            errorID = IDS_LOWMEM;
                            break;
                        }
                        found = TRUE;
                        break;
                    }
                }
            }
        }
    }
    if (!found)
        errorID = IDS_FILENOTFOUND;
    free(centralHeader);
    free(tempName);
    return errorID;
}

int CZipUnpack::PrepareMaskArray(TIndirectArray2<char>& maskArray, const char* masks)
{
    CALL_STACK_MESSAGE2("CZipUnpack::PrepareMaskArray(, %s)", masks);
    const char* sour;
    char* dest;
    char* newMask;
    int newMaskLen;
    char buffer[MAX_PATH + 1];

    sour = masks;
    while (*sour)
    {
        dest = buffer;
        while (*sour)
        {
            if (*sour == ';')
            {
                if (*(sour + 1) == ';')
                    sour++;
                else
                    break;
            }
            if (dest == buffer + MAX_PATH)
                return IDS_TOOLONGMASK;
            *dest++ = *sour++;
        }
        while (--dest >= buffer && *dest <= ' ')
            ;
        *(dest + 1) = 0;
        dest = buffer;
        while (*dest != 0 && *dest <= ' ')
            dest++;
        newMaskLen = (int)strlen(dest);
        if (newMaskLen)
        {
            newMask = new char[newMaskLen + 1];
            if (!newMask)
                return IDS_LOWMEM;
            SalamanderGeneral->PrepareMask(newMask, dest);
            if (!maskArray.Add(newMask))
            {
                delete newMask;
                return IDS_LOWMEM;
            }
        }
        if (*sour)
            sour++;
    }
    return 0;
}

int CZipUnpack::MatchFilesToMask(TIndirectArray2<char>& maskArray)
{
    CALL_STACK_MESSAGE1("CZipUnpack::MatchFilesToMask()");
    CFileHeader* centralHeader;
    CFileInfo* fileInfo;
    char* tempName;
    unsigned tempNameLen;
    char* sour;
    int errorID = 0;
    QWORD readOffset;
    QWORD readSize;
    int j; //temporary variable
    bool hasExtension;

    centralHeader = (CFileHeader*)malloc(MAX_HEADER_SIZE);
    tempName = (char*)malloc(MAX_HEADER_SIZE);
    if (!centralHeader || !tempName)
    {
        if (centralHeader)
            free(centralHeader);
        if (tempName)
            free(tempName);
        return IDS_LOWMEM;
    }
    readOffset = CentrDirOffs + ExtraBytes;
    readSize = 0;
    if (DiskNum != CentrDirStartDisk && MultiVol)
    {
        DiskNum = CentrDirStartDisk;
        errorID = ChangeDisk();
    }
    for (; readSize < CentrDirSize && !errorID;)
    {
        unsigned int s;
        errorID = ReadCentralHeader(centralHeader, &readOffset, &s);
        if (errorID)
            break;
        readSize += s;
        tempNameLen = ProcessName(centralHeader, tempName);
        sour = tempName + tempNameLen;
        hasExtension = false;
        while (sour >= tempName && *sour != '\\')
            if (*sour-- == '.')
                hasExtension = true; // ".cvspass" ve Windows je pripona
        sour++;
        for (j = 0; j < maskArray.Count; j++)
        {
            if (SalamanderGeneral->AgreeMask(sour, maskArray[j], hasExtension))
            {
                fileInfo = new CFileInfo;
                if (!fileInfo)
                {
                    errorID = IDS_LOWMEM;
                    break;
                }
                ProcessHeader(centralHeader, fileInfo);
                fileInfo->NameLen = tempNameLen;
                fileInfo->Name = (char*)malloc(tempNameLen + 1);
                if (!fileInfo->Name)
                {
                    delete fileInfo;
                    errorID = IDS_LOWMEM;
                    break;
                }
                lstrcpy(fileInfo->Name, tempName);
                if (!ExtrFiles->Add(fileInfo))
                {
                    delete fileInfo;
                    errorID = IDS_LOWMEM;
                };
                if (!ErrorID)
                    MatchedTotalSize += CQuadWord().SetUI64(fileInfo->Size);
                break;
            }
        }
    }
    free(centralHeader);
    free(tempName);
    return errorID;
}

//inflate hi-level routines
#define CZIPUNPACK(decompress) ((CZipUnpack*)decompress->UserData)

void Refill(CDecompressionObject* decompress)
{
    CALL_STACK_MESSAGE1("Refill()");

    if (CZIPUNPACK(decompress)->BytesLeft == 0)
    {
        TRACE_E("Pozadavek na doplneni vstupniho bufferu za hranici zkomprimovanych dat");
        decompress->Input->Error = IDS_EOF;
        return;
    }

    int s = (int)min(CZIPUNPACK(decompress)->InBufSize, CZIPUNPACK(decompress)->BytesLeft);
    decompress->Input->Error =
        CZIPUNPACK(decompress)->SafeRead(CZIPUNPACK(decompress)->InputBuffer, s, NULL);

    if (decompress->Input->Error)
        return;

    CZIPUNPACK(decompress)->BytesLeft -= s;

    if (CZIPUNPACK(decompress)->Encrypted)
    {
        if (CZIPUNPACK(decompress)->AESContextValid)
        {
            SalamanderCrypt->AESDecrypt(&CZIPUNPACK(decompress)->AESContext, CZIPUNPACK(decompress)->InputBuffer, s);
            if (CZIPUNPACK(decompress)->BytesLeft == 0)
            {
                // posledni porce, overime authentication code
                unsigned char mac[AES_MAXHMAC];
                unsigned char macFile[AES_MAXHMAC];
                int mode = CZIPUNPACK(decompress)->AESContext.mode;
                DWORD macLen = SAL_AES_MAC_LENGTH(mode);

                decompress->Input->Error =
                    CZIPUNPACK(decompress)->SafeRead(macFile, macLen, NULL);
                if (decompress->Input->Error)
                    return;

                SalamanderCrypt->AESEnd(&CZIPUNPACK(decompress)->AESContext, mac, &macLen);
                CZIPUNPACK(decompress)->AESContextValid = FALSE;

                if (memcmp(mac, macFile, macLen) != 0)
                {
                    decompress->Input->Error = IDS_MACERROR;
                    return;
                }
            }
        }
        else
            Decrypt(CZIPUNPACK(decompress)->InputBuffer, s, CZIPUNPACK(decompress)->Keys);
    }

    decompress->Input->NextByte = (__UINT8*)CZIPUNPACK(decompress)->InputBuffer;
    decompress->Input->BytesLeft = s;
}

int ExtractFlush(unsigned bytes, CDecompressionObject* decompress)
{
    CALL_STACK_MESSAGE2("ExtractFlush(0x%X, )", bytes);
    uch* buf = CZIPUNPACK(decompress)->Unshrinking ? (decompress->Output->OutBuf) : (decompress->Output->SlideWin);
    //  TRACE_I("Flush");
    CZIPUNPACK(decompress)->Crc = SalamanderGeneral->UpdateCrc32(buf,
                                                                 bytes,
                                                                 CZIPUNPACK(decompress)->Crc);
    if (!CZIPUNPACK(decompress)->Test)
    {
        CZIPUNPACK(decompress)->OutputError =
            CZIPUNPACK(decompress)->Write(CZIPUNPACK(decompress)->OutputFile, buf, bytes, &CZIPUNPACK(decompress)->SkipAllIOErrors);
    }
    else
    {
        CZIPUNPACK(decompress)->ExtractedBytes += bytes;
        CZIPUNPACK(decompress)->OutputError = 0;
    }
    if (!CZIPUNPACK(decompress)->OutputError)
    {
        //CZIPUNPACK(decompress)->OutputPos += bytes;
        if (CZIPUNPACK(decompress)->Salamander->ProgressAddSize(bytes, TRUE))
            return 0;
        else
        {
            CZIPUNPACK(decompress)->UserBreak = true;
            CZIPUNPACK(decompress)->OutputError = 0;
            return 1;
        }
    }
    else
        return 1; //error
}

int CZipUnpack::InflateFile(CFileInfo* fileInfo, BOOL deflate64, int* errorID)
{
    CALL_STACK_MESSAGE1("CZipUnpack::InflateFile(, )");
    CDecompressionObject decompress;
    COutputManager output;
    CInputManager input;
    int exitCode = DEC_NOERROR;
    //int                  result;

    ZipFile->FilePointer = fileInfo->DataOffset;
    BytesLeft = fileInfo->CompSize;
    if (Encrypted)
        BytesLeft -= AESContextValid ? SAL_AES_SALT_LENGTH(AESContext.mode) + SAL_AES_PWD_VER_LENGTH + AES_MAXHMAC : ENCRYPT_HEADER_SIZE;
    Crc = INIT_CRC;
    input.NextByte = (__UINT8*)InputBuffer;
    input.BytesLeft = 0;
    input.Error = 0;
    input.Refill = Refill;
    output.SlideWin = (__UINT8*)SlideWindow;
    output.WinSize = WinSize;
    output.Flush = ExtractFlush;
    decompress.Input = &input;
    decompress.Output = &output;
    decompress.UserData = this;
    decompress.HeapInfo = (void*)Heap;
    decompress.fixed_tl64 = (huft*)fixed_tl64;
    decompress.fixed_td64 = (huft*)fixed_td64;
    decompress.fixed_bl64 = fixed_bl64;
    decompress.fixed_bd64 = fixed_bd64;
    decompress.fixed_tl32 = (huft*)fixed_tl32;
    decompress.fixed_td32 = (huft*)fixed_td32;
    decompress.fixed_bl32 = fixed_bl32;
    decompress.fixed_bd32 = fixed_bd32;
    switch (Inflate(&decompress, deflate64))
    {
    case 1:;
    case 2:
    {
    BadData:
        switch (ProcessError(IDS_ERRCOMPDATA, 0, FileNameDisp,
                             PE_NORETRY | DialogFlags, &SkipAllDataErr))
        {
        case ERR_SKIP:
            exitCode = DEC_SKIP;
            break;
        case ERR_CANCEL:
            exitCode = DEC_CANCEL;
            *errorID = IDS_NODISPLAY;
        }
        break;
    }

    case 3:
        exitCode = DEC_CANCEL;
        *errorID = IDS_LOWMEM;
        break;
    case 4:
    {
        if (decompress.Input->Error == IDS_EOF)
            goto BadData;
        if (decompress.Input->Error == IDS_MACERROR)
        {
            switch (ProcessError(IDS_MACERROR, 0, FileNameDisp,
                                 PE_NORETRY | DialogFlags, &SkipAllDataErr))
            {
            case ERR_SKIP:
                exitCode = DEC_SKIP;
                break;
            case ERR_CANCEL:
                exitCode = DEC_CANCEL;
                *errorID = IDS_NODISPLAY;
            }
        }
        else
        {
            exitCode = DEC_CANCEL;
            *errorID = decompress.Input->Error;
        }
        break;
    }

    case 5:
    {
        switch (OutputError)
        {
        case ERR_SKIP:
            exitCode = DEC_SKIP;
            break;
        case ERR_CANCEL:
            exitCode = DEC_CANCEL;
            *errorID = IDS_NODISPLAY;
        }
    }
    }
    fixed_tl64 = decompress.fixed_tl64;
    fixed_td64 = decompress.fixed_td64;
    fixed_bl64 = decompress.fixed_bl64;
    fixed_bd64 = decompress.fixed_bd64;
    fixed_tl32 = decompress.fixed_tl32;
    fixed_td32 = decompress.fixed_td32;
    fixed_bl32 = decompress.fixed_bl32;
    fixed_bd32 = decompress.fixed_bd32;
    return exitCode;
}

void CZipUnpack::InflateFreeFixedHufman()
{
    CALL_STACK_MESSAGE1("CZipUnpack::InflateFreeFixedHufman()");
    CDecompressionObject decompress;

    decompress.HeapInfo = (void*)Heap;
    decompress.fixed_tl64 = (huft*)fixed_tl64;
    decompress.fixed_td64 = (huft*)fixed_td64;
    decompress.fixed_tl32 = (huft*)fixed_tl32;
    decompress.fixed_td32 = (huft*)fixed_td32;
    FreeFixedHufman(&decompress);
}

int CZipUnpack::UnStoreFile(CFileInfo* fileInfo, int* errorID)
{
    CALL_STACK_MESSAGE1("CZipUnpack::UnStoreFile(, )");
    int exitCode = DEC_NOERROR;
    unsigned readBytes;
    QWORD bytesLeft;
    int result;

    ZipFile->FilePointer = fileInfo->DataOffset;
    bytesLeft = fileInfo->CompSize;
    if (Encrypted)
        bytesLeft -= AESContextValid ? SAL_AES_SALT_LENGTH(AESContext.mode) + SAL_AES_PWD_VER_LENGTH + AES_MAXHMAC : ENCRYPT_HEADER_SIZE;
    Crc = INIT_CRC;
    while (bytesLeft && !UserBreak)
    {
        readBytes = (unsigned)min(InBufSize, bytesLeft);
        *errorID = SafeRead(InputBuffer, readBytes, NULL);
        if (*errorID)
        {
            if (*errorID == IDS_EOF)
            {
                switch (ProcessError(IDS_ERRCOMPDATA, 0, FileNameDisp,
                                     PE_NORETRY | DialogFlags, &SkipAllDataErr))
                {
                case ERR_SKIP:
                    exitCode = DEC_SKIP;
                    *errorID = 0;
                    break;
                case ERR_CANCEL:
                    exitCode = DEC_CANCEL;
                    *errorID = IDS_NODISPLAY;
                }
            }
            else
            {
                exitCode = DEC_CANCEL;
            }
            break;
        }
        else
        {
            bytesLeft -= readBytes;
            if (Encrypted)
            {
                if (AESContextValid)
                {
                    SalamanderCrypt->AESDecrypt(&AESContext, InputBuffer, readBytes);
                    if (bytesLeft == 0)
                    {
                        // posledni porce, overime authentication code
                        unsigned char mac[AES_MAXHMAC];
                        unsigned char macFile[AES_MAXHMAC];
                        int mode = AESContext.mode;

                        *errorID = SafeRead(macFile, SAL_AES_MAC_LENGTH(mode), NULL);
                        if (*errorID)
                        {
                            if (*errorID == IDS_EOF)
                            {
                                switch (ProcessError(IDS_ERRCOMPDATA, 0, FileNameDisp,
                                                     PE_NORETRY | DialogFlags, &SkipAllDataErr))
                                {
                                case ERR_SKIP:
                                    exitCode = DEC_SKIP;
                                    *errorID = 0;
                                    break;
                                case ERR_CANCEL:
                                    exitCode = DEC_CANCEL;
                                    *errorID = IDS_NODISPLAY;
                                }
                            }
                            else
                            {
                                exitCode = DEC_CANCEL;
                            }
                            break;
                        }

                        DWORD macLen;
                        SalamanderCrypt->AESEnd(&AESContext, mac, &macLen);
                        AESContextValid = FALSE;

                        if (memcmp(mac, macFile, macLen) != 0)
                        {
                            switch (ProcessError(IDS_MACERROR, 0, FileNameDisp,
                                                 PE_NORETRY | DialogFlags, &SkipAllDataErr))
                            {
                            case ERR_SKIP:
                                exitCode = DEC_SKIP;
                                *errorID = 0;
                                break;
                            case ERR_CANCEL:
                                exitCode = DEC_CANCEL;
                                *errorID = IDS_NODISPLAY;
                            }
                            break;
                        }
                    }
                }
                else
                    Decrypt(InputBuffer, readBytes, Keys);
            }
            Crc = SalamanderGeneral->UpdateCrc32(InputBuffer, readBytes, Crc);
            if (!Test)
            {
                result = Write(OutputFile, InputBuffer, readBytes, &SkipAllIOErrors);
                if (result)
                {
                    switch (result)
                    {
                    case ERR_SKIP:
                        exitCode = DEC_SKIP;
                        break;
                    case ERR_CANCEL:
                        exitCode = DEC_CANCEL;
                        *errorID = IDS_NODISPLAY;
                    }
                    break;
                }
            }
            if (!Salamander->ProgressAddSize(readBytes, TRUE))
                UserBreak = true;
        }
    }
    return exitCode;
}

int CZipUnpack::ExplodeFile(CFileInfo* fileInfo, int* errorID)
{
    CALL_STACK_MESSAGE1("CZipUnpack::ExplodeFile(, )");
    CDecompressionObject decompress;
    COutputManager output;
    CInputManager input;
    int exitCode = DEC_NOERROR;

    ZipFile->FilePointer = fileInfo->DataOffset;
    BytesLeft = fileInfo->CompSize;
    if (Encrypted)
        BytesLeft -= AESContextValid ? SAL_AES_SALT_LENGTH(AESContext.mode) + SAL_AES_PWD_VER_LENGTH + AES_MAXHMAC : ENCRYPT_HEADER_SIZE;
    decompress.csize = BytesLeft;
    Crc = INIT_CRC;
    input.NextByte = (__UINT8*)InputBuffer;
    input.BytesLeft = 0;
    input.Error = 0;
    input.Refill = Refill;
    output.SlideWin = (__UINT8*)SlideWindow;
    output.WinSize = WinSize;
    output.Flush = ExtractFlush;
    decompress.Input = &input;
    decompress.Output = &output;
    decompress.UserData = this;
    decompress.HeapInfo = (void*)Heap;
    decompress.ucsize = fileInfo->Size;
    decompress.Flag = fileInfo->Flag;
    switch (Explode(&decompress))
    {
    case 1:;
    case 2:
    {
    BadData:
        switch (ProcessError(IDS_ERRCOMPDATA, 0, FileNameDisp,
                             PE_NORETRY | DialogFlags, &SkipAllDataErr))
        {
        case ERR_SKIP:
            exitCode = DEC_SKIP;
            break;
        case ERR_CANCEL:
            exitCode = DEC_CANCEL;
            *errorID = IDS_NODISPLAY;
        }
        break;
    }

    case 3:
        exitCode = DEC_CANCEL;
        *errorID = IDS_LOWMEM;
        break;

    case 4:
    {
        if (decompress.Input->Error == IDS_EOF)
            goto BadData;
        if (decompress.Input->Error == IDS_MACERROR)
        {
            switch (ProcessError(IDS_MACERROR, 0, FileNameDisp,
                                 PE_NORETRY | DialogFlags, &SkipAllDataErr))
            {
            case ERR_SKIP:
                exitCode = DEC_SKIP;
                break;
            case ERR_CANCEL:
                exitCode = DEC_CANCEL;
                *errorID = IDS_NODISPLAY;
            }
        }
        else
        {
            exitCode = DEC_CANCEL;
            *errorID = decompress.Input->Error;
        }
        break;
    }

    case 5:
    {
        switch (OutputError)
        {
        case ERR_SKIP:
            exitCode = DEC_SKIP;
            break;
        case ERR_CANCEL:
            exitCode = DEC_CANCEL;
            *errorID = IDS_NODISPLAY;
        }
    }
    }
    return exitCode;
}

int CZipUnpack::UnShrinkFile(CFileInfo* fileInfo, int* errorID)
{
    CALL_STACK_MESSAGE1("CZipUnpack::UnShrinkFile(, )");
    CDecompressionObject decompress;
    COutputManager output;
    CInputManager input;
    int exitCode = DEC_NOERROR;
    //int                  result;

    ZipFile->FilePointer = fileInfo->DataOffset;
    BytesLeft = fileInfo->CompSize;
    if (Encrypted)
        BytesLeft -= AESContextValid ? SAL_AES_SALT_LENGTH(AESContext.mode) + SAL_AES_PWD_VER_LENGTH + AES_MAXHMAC : ENCRYPT_HEADER_SIZE;
    decompress.CompBytesLeft = BytesLeft;
    Crc = INIT_CRC;
    input.NextByte = (__UINT8*)InputBuffer;
    input.BytesLeft = 0;
    input.Error = 0;
    input.Refill = Refill;
    output.SlideWin = (__UINT8*)SlideWindow;
    output.WinSize = WinSize;
    output.Flush = ExtractFlush;
    if (!OutputBuffer)
    {
        OutputBuffer = (char*)malloc(OUTPUT_BUFFER_SIZE);
        if (!OutputBuffer)
        {
            *errorID = IDS_LOWMEM;
            return DEC_CANCEL;
        }
        OutBufSize = OUTPUT_BUFFER_SIZE;
    }
    output.OutBuf = (uch*)OutputBuffer;
    output.BufSize = OutBufSize;
    decompress.Input = &input;
    decompress.Output = &output;
    decompress.UserData = this;
    decompress.HeapInfo = (void*)Heap;
    Unshrinking = true;
    switch (Unshrink(&decompress))
    {
    case 1:
    {
        if (decompress.Input->Error == IDS_EOF ||
            decompress.Input->Error == IDS_MACERROR)
        {
            switch (ProcessError(
                decompress.Input->Error == IDS_MACERROR ? IDS_MACERROR : IDS_ERRCOMPDATA,
                0, FileNameDisp,
                PE_NORETRY | DialogFlags, &SkipAllDataErr))
            {
            case ERR_SKIP:
                exitCode = DEC_SKIP;
                break;
            case ERR_CANCEL:
                exitCode = DEC_CANCEL;
                *errorID = IDS_NODISPLAY;
            }
        }
        else
        {
            exitCode = DEC_CANCEL;
            *errorID = decompress.Input->Error;
        }
        break;
    }

    case 2:
    {
        switch (OutputError)
        {
        case ERR_SKIP:
            exitCode = DEC_SKIP;
            break;
        case ERR_CANCEL:
            exitCode = DEC_CANCEL;
            *errorID = IDS_NODISPLAY;
        }
    }
    }
    Unshrinking = false;
    return exitCode;
}

int CZipUnpack::UnReduceFile(CFileInfo* fileInfo, int* errorID)
{
    CALL_STACK_MESSAGE1("CZipUnpack::UnReduceFile(, )");
    CDecompressionObject decompress;
    COutputManager output;
    CInputManager input;
    int exitCode = DEC_NOERROR;

    ZipFile->FilePointer = fileInfo->DataOffset;
    BytesLeft = fileInfo->CompSize;
    if (Encrypted)
        BytesLeft -= AESContextValid ? SAL_AES_SALT_LENGTH(AESContext.mode) + SAL_AES_PWD_VER_LENGTH + AES_MAXHMAC : ENCRYPT_HEADER_SIZE;
    decompress.CompBytesLeft = BytesLeft;
    Crc = INIT_CRC;
    input.NextByte = (__UINT8*)InputBuffer;
    input.BytesLeft = 0;
    input.Error = 0;
    input.Refill = Refill;
    output.SlideWin = (__UINT8*)SlideWindow;
    output.WinSize = WinSize;
    output.Flush = ExtractFlush;
    decompress.Input = &input;
    decompress.Output = &output;
    decompress.UserData = this;
    decompress.HeapInfo = (void*)Heap;
    decompress.ucsize = fileInfo->Size;
    decompress.Method = fileInfo->Method;
    switch (Unreduce(&decompress))
    {
    case 1:
    {
        if (decompress.Input->Error == IDS_EOF ||
            decompress.Input->Error == IDS_MACERROR)
        {
            switch (ProcessError(
                decompress.Input->Error == IDS_MACERROR ? IDS_MACERROR : IDS_ERRCOMPDATA,
                0, FileNameDisp,
                PE_NORETRY | DialogFlags, &SkipAllDataErr))
            {
            case ERR_SKIP:
                exitCode = DEC_SKIP;
                break;
            case ERR_CANCEL:
                exitCode = DEC_CANCEL;
                *errorID = IDS_NODISPLAY;
            }
        }
        else
        {
            exitCode = DEC_CANCEL;
            *errorID = decompress.Input->Error;
        }
        break;
    }

    case 2:
    {
        switch (OutputError)
        {
        case ERR_SKIP:
            exitCode = DEC_SKIP;
            break;
        case ERR_CANCEL:
            exitCode = DEC_CANCEL;
            *errorID = IDS_NODISPLAY;
        }
    }
    }
    return exitCode;
}

int CZipUnpack::UnBZIP2File(CFileInfo* fileInfo, int* errorID)
{
    CALL_STACK_MESSAGE1("CZipUnpack::UnBZIP2File(, )");
    CDecompressionObject decompress;
    COutputManager output;
    CInputManager input;
    int ret, exitCode = DEC_NOERROR;

    memset(&decompress, 0, sizeof(decompress));
    ZipFile->FilePointer = fileInfo->DataOffset;
    BytesLeft = fileInfo->CompSize;
    if (Encrypted)
        BytesLeft -= AESContextValid ? SAL_AES_SALT_LENGTH(AESContext.mode) + SAL_AES_PWD_VER_LENGTH + AES_MAXHMAC : ENCRYPT_HEADER_SIZE;
    decompress.CompBytesLeft = BytesLeft;
    Crc = INIT_CRC;
    input.NextByte = (__UINT8*)InputBuffer;
    input.BytesLeft = 0;
    input.Error = 0;
    input.Refill = Refill;
    output.SlideWin = (__UINT8*)SlideWindow;
    output.WinSize = WinSize;
    output.Flush = ExtractFlush;
    decompress.Input = &input;
    decompress.Output = &output;
    decompress.UserData = this;
    decompress.ucsize = fileInfo->Size;
    switch (ret = UnBZIP2(&decompress))
    {
    case 1:
    {
        if (decompress.Input->Error == IDS_EOF ||
            decompress.Input->Error == IDS_MACERROR)
        {
            switch (ProcessError(
                decompress.Input->Error == IDS_MACERROR ? IDS_MACERROR : IDS_ERRCOMPDATA,
                0, FileNameDisp,
                PE_NORETRY | DialogFlags, &SkipAllDataErr))
            {
            case ERR_SKIP:
                exitCode = DEC_SKIP;
                break;
            case ERR_CANCEL:
                exitCode = DEC_CANCEL;
                *errorID = IDS_NODISPLAY;
            }
        }
        else
        {
            exitCode = DEC_CANCEL;
            *errorID = decompress.Input->Error;
        }
        break;
    }

    case 2:
    {
        switch (OutputError)
        {
        case ERR_SKIP:
            exitCode = DEC_SKIP;
            break;
        case ERR_CANCEL:
            exitCode = DEC_CANCEL;
            *errorID = IDS_NODISPLAY;
            break;
        }
        break;
    }
    case 3: // Out of memory
    case 4: // Error uncompressing BZIP2 stream
        switch (ProcessError(
            ret == 3 ? IDS_LOWMEM : IDS_ERRBZIP2,
            0, FileNameDisp,
            PE_NORETRY | DialogFlags, &SkipAllDataErr))
        {
        case ERR_SKIP:
            exitCode = DEC_SKIP;
            break;
        case ERR_CANCEL:
            exitCode = DEC_CANCEL;
            *errorID = IDS_NODISPLAY;
            break;
        }
        break;
    }
    return exitCode;
}

int CZipUnpack::ExtractSingleFile(char* targetDir, int targetDirLen,
                                  CFileInfo* fileInfo, BOOL* success, const char* newFileName)
{
    CALL_STACK_MESSAGE2("CZipUnpack::ExtractSingleFile(, %d, , )", targetDirLen);
    CLocalFileHeader* localHeader;
    LPTSTR pathBuf;
    LPTSTR path;
    LPTSTR name;
    LPCTSTR sour;
    LPTSTR dest;
    int errorID = 0;
    //bool                retry;
    //bool                reopenZipFile;
    int result;
    bool skip, bCheckCRC = true;
    char errBuf[128];
    CAESExtraField aesExtraField;
    /*
  TRACE_I("Unpacking file: " << fileInfo->Name <<
          ", method: " << fileInfo->Method <<
          ", flag: " << fileInfo->Flag <<
          ", isdir: " << fileInfo->IsDir <<
          ", file attr:" << fileInfo->FileAttr);
*/
    AESContextValid = FALSE; // inicializace
    if (success)
        *success = FALSE;
    localHeader = (CLocalFileHeader*)malloc(MAX_HEADER_SIZE);
    pathBuf = (LPTSTR)malloc(sizeof(TCHAR) * (ZIP_MAX_PATH + 1));
    if (!localHeader || !pathBuf)
    {
        if (localHeader)
            free(localHeader);
        if (pathBuf)
            free(pathBuf);
        return IDS_LOWMEM;
    }
    *(targetDir + targetDirLen++) = '\\';
    if (DiskNum != fileInfo->StartDisk && MultiVol)
    {
        DiskNum = fileInfo->StartDisk;
        errorID = ChangeDisk();
    }
    if (!errorID)
    {
        errorID = ReadLocalHeader(localHeader, fileInfo->LocHeaderOffs);
        if (!errorID)
        {
            ProcessLocalHeader(localHeader, fileInfo, &aesExtraField);
            path = pathBuf;
            SplitPath(&path, &name, fileInfo->Name + RootLen + (RootLen ? 1 : 0));
            if (newFileName)
                name = (LPTSTR)newFileName;
            sour = path;
            dest = targetDir + targetDirLen;
            if (*sour)
            {
                while (*sour)
                    *dest++ = *sour++;
                *dest++ = '\\';
            }
            if (fileInfo->IsDir)
            {
                if (!Test)
                {
                    //*dest++ = '\\';
                    sour = name;
                    while (*sour)
                        *dest++ = *sour++;
                    *dest = 0;
                    bool retry;
                    do
                    {
                        retry = false;
                        if (!SalamanderGeneral->CheckAndCreateDirectory(targetDir, NULL, TRUE, errBuf, 128))
                        {
                            switch (ProcessError(IDS_ERRCREATEDIR, 0, targetDir, DialogFlags,
                                                 &SkipAllIOErrors, errBuf))
                            {
                            case ERR_RETRY:
                                retry = true;
                                break;
                            case ERR_CANCEL:
                                errorID = IDS_NODISPLAY;
                            }
                        }
                    } while (retry);
                    if (!errorID)
                    {
                        SetFileAttributes(targetDir, fileInfo->FileAttr & FILE_ATTTRIBUTE_MASK);
                        if (success)
                        {
                            *success = TRUE;
                        }
                    }
                }
                else if (success)
                {
                    *success = TRUE;
                }
            }
            else
            {
                *dest = 0;
                skip = false;
                bool retry;
                if (!Test)
                {
                    do
                    {
                        retry = false;
                        if (!SalamanderGeneral->CheckAndCreateDirectory(targetDir, NULL, TRUE, errBuf, 128))
                        {
                            switch (ProcessError(IDS_ERRCREATEDIR, 0, targetDir, DialogFlags,
                                                 &SkipAllIOErrors, errBuf))
                            {
                            case ERR_RETRY:
                                retry = true;
                                break;
                            case ERR_CANCEL:
                                errorID = IDS_NODISPLAY;
                            }
                        }
                    } while (retry);
                }
                if (!errorID)
                {
                    sour = name;
                    while (*sour)
                        *dest++ = *sour++;
                    *dest = 0;
                    FileNameDisp = fileInfo->Name + RootLen + (RootLen ? 1 : 0);
                    skip = false;
                    if (fileInfo->Flag & GPF_ENCRYPTED)
                    {
                        if (SkipAllEncrypted)
                            skip = true;
                        else
                        {
                            if (fileInfo->Method == CM_AES)
                            {
                                if (aesExtraField.HeaderID != AES_HEADER_ID ||
                                    aesExtraField.DataSize < sizeof(CAESExtraField) - 4 ||
                                    aesExtraField.Strength < 1 ||
                                    aesExtraField.Strength > 3)
                                {
                                    switch (ProcessError(IDS_BADAES, 0, FileNameDisp,
                                                         PE_NORETRY | DialogFlags, &SkipAllEncrypted))
                                    {
                                    case ERR_SKIP:
                                        skip = true;
                                        break;
                                    default:
                                        errorID = IDS_NODISPLAY;
                                        break;
                                    }
                                }
                                else
                                {
                                    if (aesExtraField.VendorID != AES_NONVENDOR_ID ||
                                        ((AES_VERSION_1 != aesExtraField.Version) && (AES_VERSION_2 != aesExtraField.Version)))
                                        TRACE_E("POZOR: soubor '" << FileNameDisp << "' je zakryptovan neznamou verzi AES, mozne komplikace");

                                    char pwd[MAX_PASSWORD];
                                    unsigned char salt[SAL_AES_MAX_SALT_LENGTH];
                                    WORD pwdVer;
                                    WORD pwdVerFile;
                                    bool repeat;

                                    // AES v2 doesn't store CRC, seems to be created by TC
                                    if (AES_VERSION_2 == aesExtraField.Version)
                                        bCheckCRC = false;
                                    ZipFile->FilePointer = fileInfo->DataOffset;
                                    errorID = SafeRead(salt, SAL_AES_SALT_LENGTH(aesExtraField.Strength), NULL);
                                    if (!errorID)
                                        SafeRead(&pwdVerFile, sizeof(pwdVerFile), NULL);
                                    if (!errorID)
                                    {
                                        // zkusime hesla v cache
                                        int i;
                                        for (i = 0; i < Passwords.Count; i++)
                                        {
                                            if (SalamanderCrypt->AESInit(&AESContext, aesExtraField.Strength,
                                                                         Passwords[i], strlen(Passwords[i]),
                                                                         salt, &pwdVer) == SAL_AES_ERR_GOOD_RETURN)
                                            {
                                                if (memcmp(&pwdVer, &pwdVerFile, sizeof(pwdVerFile)) == 0)
                                                {
                                                    fileInfo->DataOffset +=
                                                        SAL_AES_SALT_LENGTH(aesExtraField.Strength) + sizeof(pwdVer);
                                                    Encrypted = true;
                                                    AESContextValid = TRUE;
                                                    fileInfo->Method = aesExtraField.Method;
                                                    break;
                                                }
                                                else
                                                {
                                                    unsigned char dummy[AES_MAXHMAC];
                                                    SalamanderCrypt->AESEnd(&AESContext, dummy, NULL);
                                                }
                                            }
                                        }
                                        if (!AESContextValid /*i >= Passwords.Count*/) // heslo jsme nenasli v cache
                                            do
                                            {
                                                repeat = false;
                                                switch (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(),
                                                                       FileNameDisp, pwd))
                                                {
                                                case IDOK:
                                                {
                                                    int err = 0;
                                                    switch (SalamanderCrypt->AESInit(&AESContext, aesExtraField.Strength,
                                                                                     pwd, strlen(pwd), salt, &pwdVer))
                                                    {
                                                    case SAL_AES_ERR_GOOD_RETURN:
                                                        if (memcmp(&pwdVer, &pwdVerFile, sizeof(pwdVerFile)) == 0)
                                                        {
                                                            Passwords.Add(_strdup(pwd));
                                                            fileInfo->DataOffset +=
                                                                SAL_AES_SALT_LENGTH(aesExtraField.Strength) + sizeof(pwdVer);
                                                            Encrypted = true;
                                                            AESContextValid = TRUE;
                                                            fileInfo->Method = aesExtraField.Method;
                                                        }
                                                        else
                                                        {
                                                            unsigned char dummy[AES_MAXHMAC];
                                                            SalamanderCrypt->AESEnd(&AESContext, dummy, NULL);
                                                            err = IDS_BADPWD;
                                                        }
                                                        break;
                                                    case SAL_AES_ERR_PASSWORD_TOO_LONG:
                                                        err = IDS_PWDTOOLONG;
                                                        break;
                                                    default:
                                                        err = IDS_AESERROR;
                                                        break;
                                                    }

                                                    if (err)
                                                    {
                                                        SalamanderGeneral->ShowMessageBox(LoadStr(err),
                                                                                          LoadStr(IDS_BADPWDTITLE), MSGBOX_ERROR);
                                                        repeat = true;
                                                    }
                                                    break;
                                                }

                                                case IDC_SKIPALL:
                                                    SkipAllEncrypted = true;
                                                case IDC_SKIP:
                                                    skip = true;
                                                    break;
                                                case IDCANCEL:
                                                default:
                                                    errorID = IDS_NODISPLAY;
                                                    break;
                                                }
                                            } while (repeat);
                                    }
                                }
                            }
                            else
                            {
                                char pwd[MAX_PASSWORD];
                                char check;
                                char header[ENCRYPT_HEADER_SIZE];
                                bool repeat;

                                ZipFile->FilePointer = fileInfo->DataOffset;
                                errorID = SafeRead(header, ENCRYPT_HEADER_SIZE, NULL);
                                if (!errorID)
                                {
                                    check = fileInfo->Flag & GPF_DATADESCR ? localHeader->Time >> 8 : fileInfo->Crc >> 24;
                                    bool bFound = false;
                                    int i;
                                    for (i = 0; i < Passwords.Count; i++)
                                        if (!InitKeys(Passwords[i], header, check, Keys))
                                        {
                                            fileInfo->DataOffset += ENCRYPT_HEADER_SIZE;
                                            Encrypted = bFound = true;
                                            break;
                                        }
                                    //                    if (i >= Passwords.Count)// pwd not found in cache
                                    if (!bFound) // pwd not found in cache
                                        do
                                        {
                                            repeat = false;
                                            switch (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(), FileNameDisp, pwd))
                                            {
                                            case IDOK:
                                                if (InitKeys(pwd, header, check, Keys))
                                                {
                                                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_BADPWD), LoadStr(IDS_BADPWDTITLE), MSGBOX_ERROR);
                                                    repeat = true;
                                                }
                                                else
                                                {
                                                    Passwords.Add(_strdup(pwd));
                                                    fileInfo->DataOffset += ENCRYPT_HEADER_SIZE;
                                                    Encrypted = true;
                                                }
                                                break;
                                            case IDC_SKIPALL:
                                                SkipAllEncrypted = true;
                                            case IDC_SKIP:
                                                skip = true;
                                                break;
                                            case IDCANCEL:
                                            default:
                                                errorID = IDS_NODISPLAY;
                                                break;
                                            }
                                        } while (repeat);
                                }
                            }
                        }
                        if (skip)
                            UserBreak = !ProgressAddSize(fileInfo->Size);
                    }
                    else
                        Encrypted = false;
                    if (!errorID && !skip)
                    {
                        if (!Test)
                        {
                            char attr[101];
                            char buf[MAX_PATH];
                            int len = lstrlen(ZipName);

                            lstrcpy(buf, ZipName);
                            *(buf + len++) = '\\';
                            lstrcpyn(buf + len, fileInfo->Name, MAX_PATH - len);
                            GetInfo(attr, &fileInfo->LastWrite, fileInfo->Size);
                            result = SafeCreateCFile(&OutputFile, targetDir, buf, attr, GENERIC_WRITE,
                                                     FILE_SHARE_READ, fileInfo->FileAttr & ~FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
                                                     DialogFlags, &Silent, &SkipAllIOErrors, fileInfo->Size);
                        }
                        else
                        {
                            result = 0;
                            ExtractedBytes = 0;
                        }
                        if (result)
                        {
                            switch (result)
                            {
                            case ERR_LOWMEM:
                                errorID = IDS_LOWMEM;
                                break;
                            case ERR_SKIP:
                                UserBreak = !ProgressAddSize(fileInfo->Size);
                                break;
                            case ERR_CANCEL:
                                errorID = IDS_NODISPLAY;
                            }
                        }
                        else
                        {
                            switch (fileInfo->Method)
                            {
                            case CM_DEFLATE64:
                                result = InflateFile(fileInfo, TRUE, &errorID);
                                break;
                            case CM_DEFLATED:
                                result = InflateFile(fileInfo, FALSE, &errorID);
                                break;
                            case CM_STORED:
                                result = UnStoreFile(fileInfo, &errorID);
                                break;
                            case CM_IMPLODED:
                                result = ExplodeFile(fileInfo, &errorID);
                                break;
                            case CM_SHRINKED:
                                result = UnShrinkFile(fileInfo, &errorID);
                                break;
                            case CM_REDUCED1:
                            case CM_REDUCED2:
                            case CM_REDUCED3:
                            case CM_REDUCED4:
                                result = UnReduceFile(fileInfo, &errorID);
                                break;
                            case CM_BZIP2:
                                result = UnBZIP2File(fileInfo, &errorID);
                                break;
                            default:
                            {
                                switch (ProcessError(IDS_BADMETHOD, 0, FileNameDisp,
                                                     PE_NORETRY | DialogFlags, &SkipAllBadMathods))
                                {
                                case ERR_SKIP:
                                    result = DEC_SKIP;
                                    break;
                                case ERR_CANCEL:
                                    result = DEC_CANCEL;
                                    errorID = IDS_NODISPLAY;
                                }
                            }
                            } //switch (fileHeader->Method)
                            QWORD remain;
                            if (!Test)
                            {
                                remain = fileInfo->Size - OutputFile->FilePointer;
                                if (result == DEC_NOERROR)
                                {
                                    switch (Flush(OutputFile, OutputFile->OutputBuffer, OutputFile->BufferPosition, &SkipAllIOErrors))
                                    {
                                    case ERR_NOERROR:
                                        result = DEC_NOERROR;
                                        break;
                                    case ERR_SKIP:
                                        result = DEC_SKIP;
                                        break;
                                    case ERR_CANCEL:
                                        result = DEC_CANCEL;
                                        break;
                                    }
                                }
                                SetFileTime(OutputFile->File, &fileInfo->LastWrite,
                                            NULL, &fileInfo->LastWrite);
                                CloseCFile(OutputFile);
                                if (result || UserBreak)
                                    DeleteFile(targetDir);
                                else
                                    SetFileAttributes(targetDir, fileInfo->FileAttr & FILE_ATTTRIBUTE_MASK);
                            }
                            else
                                remain = fileInfo->Size - ExtractedBytes;
                            if (!UserBreak)
                            {
                                switch (result)
                                {
                                case DEC_NOERROR:
                                {
                                    // nekdy jsou v local-headeru ulozena jina data, nez v
                                    // central-headeru porovname tedy crc s obema
                                    if (bCheckCRC && ((Crc != fileInfo->Crc) &&
                                                      ((fileInfo->Flag & GPF_DATADESCR) || Crc != localHeader->Crc)))
                                    {
                                        if (ProcessError(IDS_ERRCRC, 0, FileNameDisp, PE_NORETRY | DialogFlags,
                                                         &SkipAllDataErr) == ERR_CANCEL)
                                        {
                                            result = DEC_CANCEL;
                                            errorID = IDS_NODISPLAY;
                                        }
                                        if (!Test)
                                        {
                                            SalamanderGeneral->ClearReadOnlyAttr(targetDir);
                                            DeleteFile(targetDir);
                                        }
                                    }
                                    else
                                    {
                                        if (success)
                                        {
                                            *success = TRUE;
                                        }
                                    }
                                    break;
                                }
                                case DEC_SKIP:
                                    UserBreak = !ProgressAddSize(remain);
                                    break;
                                case DEC_CANCEL:
                                    break;
                                }
                            }
                        }
                    }
                }
            }
#ifdef TRACE_ENABLE
            if (dest - targetDir >= ZIP_MAX_PATH)
                TRACE_E("Max path length exceeded");
#endif
        }
    }
    *(targetDir + --targetDirLen) = 0;
    free(localHeader);
    free(pathBuf);
    if (AESContextValid)
    {
        unsigned char dummy[AES_MAXHMAC];
        SalamanderCrypt->AESEnd(&AESContext, dummy, NULL);
    }
    return errorID;
} /* CZipUnpack::ExtractSingleFile */

int CZipUnpack::ExtractFiles(const char* targetDir)
{
    CALL_STACK_MESSAGE2("CZipUnpack::ExtractFiles(%s)", targetDir);
    CLocalFileHeader* localHeader;
    CFileInfo* fileInfo;
    char* tempDir;
    int tempDirLen;
    LPTSTR progrTextBuf;
    LPTSTR progrText;
    const char* sour;
    //int                 rootLen = lstrlen(ZipRoot);
    int errorID = 0;
    int i;

    InputBuffer = (char*)malloc(DECOMPRESS_INBUFFER_SIZE);
    InBufSize = DECOMPRESS_INBUFFER_SIZE;
    SlideWindow = (char*)malloc(SLIDE_WINDOW_SIZE);
    WinSize = SLIDE_WINDOW_SIZE;
    localHeader = (CLocalFileHeader*)malloc(MAX_HEADER_SIZE);
    tempDir = (char*)malloc(sizeof(TCHAR) * (ZIP_MAX_PATH + 1));
    progrTextBuf = (LPTSTR)malloc(sizeof(TCHAR) * (ZIP_MAX_PATH + 32));
    if (!localHeader || !tempDir || !progrTextBuf ||
        !InputBuffer || !SlideWindow)
    {
        if (localHeader)
            free(localHeader);
        if (tempDir)
            free(tempDir);
        if (progrTextBuf)
            free(progrTextBuf);
        if (InputBuffer)
            free(InputBuffer);
        if (SlideWindow)
            free(SlideWindow);
        return IDS_LOWMEM;
    }
    /*
  if (!SalGetTempFileName(targetDir, "Sal", tempDir, FALSE, NULL))
    errorID = IDS_ERRTEMPDIR;
  else
  {
  */
    lstrcpy(tempDir, targetDir);
    tempDirLen = lstrlen(tempDir);
    if (tempDirLen && tempDir[tempDirLen - 1] == '\\')
    {
        tempDir[tempDirLen - 1] = 0;
        tempDirLen--;
    }
    if (!Test && !SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                                   targetDir, ProgressTotalSize, LoadStr(IDS_PLUGINNAME)))
        errorID = IDS_NODISPLAY;
    {
        progrText = progrTextBuf;
        sour = LoadStr(Test ? IDS_TESTING : IDS_EXTRACTING);
        while (*sour)
            *progrText++ = *sour++;
        fixed_tl64 = NULL; //for
        fixed_td64 = NULL;
        fixed_tl32 = NULL; //for
        fixed_td32 = NULL;
        DialogFlags = 0;
        SkipAllIOErrors = 0;
        SkipAllLongNames = 0;
        SkipAllEncrypted = 0;
        SkipAllDataErr = 0;
        SkipAllBadMathods = 0;
        Silent = 0;
        ProgressTotalSize += CQuadWord(ExtrFiles->Count, 0);
        Salamander->ProgressDialogAddText(LoadStr(Test ? IDS_TESTFILES : IDS_EXTRACTFILES), FALSE);
        for (i = 0; i < ExtrFiles->Count && !errorID && !UserBreak; i++)
        {
            fileInfo = (*ExtrFiles)[i];
            if (tempDirLen + 1 + fileInfo->NameLen - RootLen - (RootLen ? 1 : 0) >=
                (DWORD)(Test ? ZIP_MAX_PATH : MAX_PATH - (fileInfo->IsDir ? 12 : 0)))
            {
                switch (ProcessError(IDS_TOOLONGNAME3, 0, fileInfo->Name + RootLen + (RootLen ? 1 : 0),
                                     PE_NORETRY | DialogFlags, &SkipAllLongNames))
                {
                case ERR_SKIP:
                    UserBreak = !ProgressAddSize(fileInfo->Size + 1);
                    continue;
                case ERR_CANCEL:
                    errorID = IDS_NODISPLAY;
                }
                break;
            }
            lstrcpy(progrText, fileInfo->Name + RootLen + (RootLen ? 1 : 0));
            Salamander->ProgressDialogAddText(progrTextBuf, TRUE);
            if (Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
            {
                Salamander->ProgressSetTotalSize(CQuadWord().SetUI64(fileInfo->Size), ProgressTotalSize);
                BOOL ok;
                errorID = ExtractSingleFile(tempDir, tempDirLen, fileInfo, &ok);
                if (!ok)
                    AllFilesOK = FALSE; // for archive testing
                UserBreak = !Salamander->ProgressAddSize(1, TRUE);
            }
            else
                UserBreak = true;
        }
        InflateFreeFixedHufman();
    }
    /*
    Salamander->CloseProgressDialog();
    if (!errorID && !UserBreak)
    {
      char  buf[MAX_PATH + 1];
      int   len;

      lstrcpyn(buf, ZipName, MAX_PATH + 1);
      len = lstrlen(buf);
      if (RootLen)
      {
        *(buf + len) = '\\';
        len++;
      }
      lstrcpyn(buf + len, ZipRoot, MAX_PATH + 1 - lstrlen(buf));
      Salamander->MoveFiles(tempDir, targetDir, tempDir, buf);
    }
    SalamanderGeneral->RemoveTemporaryDir(tempDir);
  }
  */
    free(localHeader);
    free(tempDir);
    free(progrTextBuf);
    free(InputBuffer);
    free(SlideWindow);
    return errorID;
}

int CZipUnpack::SafeRead(void* buffer, unsigned bytesToRead, bool* skipAll)
{
    CALL_STACK_MESSAGE2("CZipUnpack::SafeRead(, 0x%X, )", bytesToRead);
    unsigned read, readTotal = 0;
    int err = 0;

    while (!err)
    {
        if (Read(ZipFile, (char*)buffer + readTotal, bytesToRead, &read, skipAll))
            return IDS_NODISPLAY;

        readTotal += read;
        bytesToRead -= read;

        if (bytesToRead == 0)
            break;

        if (!MultiVol)
        {
            Fatal = true;
            return IDS_EOF;
        }

        DiskNum++;
        err = ChangeDisk();
    }

    return err;
}

/*
int CZipUnpack::SafeRead(void * buffer, unsigned bytesToRead,
                         unsigned * bytesRead, bool * skipAll)
{
  CALL_STACK_MESSAGE2("CZipUnpack::SafeRead(, 0x%X, , )", bytesToRead);
  unsigned  read;
  int       err = 0;
  
  if (Read(ZipFile, buffer, bytesToRead, &read, skipAll))
  {
    err = IDS_NODISPLAY;
  }
  else
  {
    if (!read)
    {
      if (MultiVol)
      {
        DiskNum++;
        err = ChangeDisk();
        if (!err)
        {
          if (Read(ZipFile, buffer, bytesToRead, &read, skipAll))
          {
            err = IDS_NODISPLAY;
          }
          else
          {
            if (!read)
            {
              err = IDS_EOF;
            }
          }
        }
      }
      else
      {
        Fatal = true;
        err = IDS_EOF;
      }
    }
  }
  *bytesRead = read;
  return err;
}
*/

int CZipUnpack::SafeCreateCFile(CFile** file, const char* fileName, const char* arcName,
                                const char* fileData, unsigned int access, unsigned int share,
                                unsigned int attributes, int flags, DWORD* silent,
                                bool* skipAll, QWORD size)
{
    CALL_STACK_MESSAGE8("CZipUnpack::SafeCreateCFile(, %s, %s, %s, 0x%X, 0x%X, "
                        "0x%X, %d, , )",
                        fileName, arcName, fileData, access,
                        share, attributes, flags);
    int result; //temp variable
    int errorID = 0;
    int lastError; //value returned by GetLastError()
    int len = lstrlen(fileName);
    BOOL toSkip = FALSE;
    int flagsNoRetry;

    if ((*file = (CFile*)malloc(sizeof(CFile))) == NULL ||
        ((*file)->FileName = (char*)malloc(len + 1)) == NULL)
    {
        if ((*file)->FileName)
        {
            free((*file)->FileName);
        }
        if (*file)
        {
            free(*file);
            *file = NULL;
        }
        return ERR_LOWMEM;
    }
    (*file)->OutputBuffer = NULL;
    (*file)->InputBuffer = NULL;
    if (access & GENERIC_WRITE &&
        ((*file)->OutputBuffer = (char*)malloc(OUTPUT_BUFFER_SIZE)) == NULL)
    {
        free((*file)->FileName);
        free(*file);
        *file = NULL;
        return ERR_LOWMEM;
    }
    else
    {
        flagsNoRetry = 0;
        while (1)
        {
            CQuadWord q = CQuadWord().SetUI64(size);
            bool allocate = AllocateWholeFile &&
                            CQuadWord(2, 0) < q && q < CQuadWord(0, 0x80000000);
            if (TestAllocateWholeFile)
                q += CQuadWord(0, 0x80000000);

            (*file)->File = SalamanderSafeFile->SafeFileCreate(fileName, access, share, attributes,
                                                               FALSE, SalamanderGeneral->GetMsgBoxParent(), arcName, fileData,
                                                               silent, TRUE, &toSkip, NULL, 0, allocate ? &q : NULL, NULL);
            if ((*file)->File != INVALID_HANDLE_VALUE)
            {
                lstrcpy((*file)->FileName, fileName);
                (*file)->FilePointer = 0;
                (*file)->RealFilePointer = 0;
                (*file)->Flags = flags;
                (*file)->BufferPosition = 0;
                (*file)->Size = 0;
                (*file)->BigFile = 1;

                if (allocate)
                {
                    if (q == CQuadWord(0, 0x80000000))
                    {
                        // allokace se nepovedla a uz se o to snazit nebudem
                        AllocateWholeFile = false;
                        TestAllocateWholeFile = false;
                    }
                    else if (q == CQuadWord(0, 0x00000000))
                    {
                        // allokace se nepovedla, ale priste to zkusime znova
                    }
                    else
                    {
                        // allokace se povedla
                        TestAllocateWholeFile = false;
                    }
                }

                // // abychom nefragmentovali disk, nastavime predem souboru cilovou velikost
                // LONG distHi = HIDWORD(size);
                // if (SetFilePointer((*file)->File, LODWORD(size), &distHi, FILE_BEGIN) != 0xFFFFFFFF &&
                //     GetLastError() == NO_ERROR)
                // {
                //   SetEndOfFile((*file)->File);
                //   // vratime ukazatel zpatky na zacatek
                //   if (SetFilePointer((*file)->File, 0, NULL, FILE_BEGIN) == 0xFFFFFFFF &&
                //       GetLastError() != NO_ERROR)
                //   {
                //     errorID = IDS_ERRACCESS;
                //   }
                // }
                if (errorID == 0)
                    return 0; //OK
            }
            else
            {
                if (toSkip)
                    result = ERR_SKIP;
                else
                    result = ERR_CANCEL;
                goto SCF_ABORT;
            }
            lastError = GetLastError();
            if ((*file)->File != INVALID_HANDLE_VALUE)
                CloseHandle((*file)->File);
            result = ProcessError(errorID, lastError, fileName, flags | flagsNoRetry, skipAll);
            if (result != ERR_RETRY)
            {

            SCF_ABORT:
                free((*file)->FileName);
                if (access & GENERIC_WRITE)
                    free((*file)->OutputBuffer);
                free(*file);
                *file = NULL;
                return result;
            }
        }
    }
}

void CZipUnpack::QuickSortHeaders2(int left, int right, TIndirectArray2<CFileInfo>& headers)
{
    CALL_STACK_MESSAGE_NONE

LABEL_QuickSortHeaders2:

    int i = left, j = right;
    int pivotDiskNum = headers[(i + j) / 2]->StartDisk;
    do
    {
        while (headers[i]->StartDisk <= pivotDiskNum && i < right)
            i++;
        while (pivotDiskNum <= headers[j]->StartDisk && j > left)
            j--;
        if (i <= j)
        {
            CFileInfo* tmp = headers[i];
            headers[i] = headers[j];
            headers[j] = tmp;
            i++;
            j--;
        }
    } while (i <= j); //musej bejt shodny?

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) QuickSortHeaders2(left, j, headers);
    //  if (i < right) QuickSortHeaders2(i, right, headers);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                QuickSortHeaders2(left, j, headers);
                left = i;
                goto LABEL_QuickSortHeaders2;
            }
            else
            {
                QuickSortHeaders2(i, right, headers);
                right = j;
                goto LABEL_QuickSortHeaders2;
            }
        }
        else
        {
            right = j;
            goto LABEL_QuickSortHeaders2;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_QuickSortHeaders2;
        }
    }
}

BOOL CZipUnpack::ProgressAddSize(QWORD size)
{
    CALL_STACK_MESSAGE_NONE
    while ((__int64)size > 0)
    {
        int s = (int)min(size, INT_MAX);
        if (!Salamander->ProgressAddSize(s, TRUE))
            return FALSE;
        size -= s;
    }
    return TRUE;
}
