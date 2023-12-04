// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "isoimage.h"
#include "iso9660.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#define SECTOR_SIZE 0x800

#define FATTR_HIDDEN 1
#define FATTR_DIRECTORY 2

#define ISO_SEPARATOR1 '.'
#define ISO_SEPARATOR2 ';'

#define SWAPDWORD(d) \
    ((((d)&0x000000FF) << 24) | \
     (((d)&0x0000FF00) << 8) | \
     (((d)&0x00FF0000) >> 8) | \
     (((d)&0xFF000000) >> 24))

// ****************************************************************************
//
// CISO9660
//

CISO9660::CDirectoryRecord::CDirectoryRecord()
{
    ZeroMemory(RecordingDateAndTime, sizeof(RecordingDateAndTime));
}

CISO9660::CISO9660(CISOImage* image, DWORD extent) : CUnISOFSAbstract(image)
{
    Ext = extNone;
    ExtentOffset = extent;

    BootRecordInfo = NULL;
}

CISO9660::~CISO9660()
{
    delete BootRecordInfo;
}

BOOL CISO9660::Open(BOOL quiet)
{
    CALL_STACK_MESSAGE2("CISO9660::Open(%d)", quiet);

    BOOL ret = TRUE;
    DWORD block = 0x10;
    BYTE sector[SECTOR_SIZE];
    BOOL terminate = FALSE;

    // detecting CD-ROM Volume Descriptor Set
    BOOL isoDescriptors = FALSE;
    // Read the VolumeDescriptor (2k) until we find it, but only up to to 1MB
    while (!terminate)
    {
        // Try to read a block
        if (Image->ReadBlock(block, SECTOR_SIZE, sector) != SECTOR_SIZE)
        {
            // Something went wrong, probably EOF?
            Error(IDS_ERROR_READING_SECTOR, quiet, block);
            ret = FALSE;
            break;
        }

        if (strncmp((char*)(sector + 1), "CD001", 5) == 0)
        {
            switch (sector[0])
            {
            case 0x00:
                ReadBootRecord(sector, quiet);
                break;

            case 0x01: // primary volume descriptor
                memcpy(&PVD, sector, SECTOR_SIZE);
                break;

            case 0x02: // suplementary/enhanced volume descriptor
                memcpy(&SVD, sector, SECTOR_SIZE);

                // Joliet ma FileStructureVersion = 1
                if (SVD.FileStructureVersion == 1)
                    Ext = extJoliet;
                // Patera 2009.02.03: The following check seems correct and not the above one...
                // Raptor provided "Pantera Safari.ISO" with corruped FileStructureVersion
                if ((SVD.EscapeSequences[0] == 0x25) && (SVD.EscapeSequences[1] == 0x2F) && ((SVD.EscapeSequences[2] == 0x40) || (SVD.EscapeSequences[2] == 0x43) || (SVD.EscapeSequences[2] == 0x45)))
                    Ext = extJoliet;
                break;

            case 0xff: // terminator
                terminate = TRUE;
                break;
            } // switch
        }     // if

        block++;

        if (block > 128)
        {
            // Just to make sure we don't read in a too big file, stop after 128 sectors.
            ret = FALSE;
            break;
        }
    } // while

    if (ret)
    {
        if (Ext == extJoliet)
            FillDirectoryRecord(Root, SVD.DirectoryRecordForRootDirectory);
        else
            FillDirectoryRecord(Root, PVD.DirectoryRecordForRootDirectory);
    }

    return ret;
}

void CISO9660::FillDirectoryRecord(CDirectoryRecord& dr, BYTE bytes[])
{
#define CpyN(Var, Ofs, N) \
    memcpy(&(dr.##Var), bytes + Ofs, N);

    CpyN(LengthOfDirectoryRecord, 0, 1);
    CpyN(ExtendedAttributeRecordLength, 1, 1);
    CpyN(LocationOfExtent, 2, 4);
    CpyN(LocationOfExtentA, 6, 4);
    CpyN(DataLength, 10, 4);
    CpyN(DataLengthA, 14, 4);
    CpyN(RecordingDateAndTime, 18, 7);
    CpyN(FileFlags, 25, 1);
    CpyN(VolumeSequenceNumber, 28, 2);

    CpyN(LengthOfFileIdentifier, 32, 1);
#undef CpyN
}

void CISO9660::FillPathTableRecord(CPathTableRecord& record, BYTE bytes[])
{
#define CpyN(Var, Ofs, N) \
    memcpy(&(record.##Var), bytes + Ofs, N);

    CpyN(LengthOfDirectoryIdentifier, 0, 1);
    CpyN(ExtendedAttributeRecordLength, 1, 1);
    CpyN(LocationOfExtent, 2, 4);
    CpyN(ParentDirectoryNumber, 6, 2);
#undef CpyN
}

#define ROTATE(a) ((((a)&0xffff) >> 8) | (((a)&0xff) << 8))

//
void CISO9660::ExtractExtFileName(char* fileName, const char* src, CDirectoryRecord& dr)
{
    // zpracovani extensions
    const char* srcSU = src + dr.LengthOfFileIdentifier;
    const char* srcEnd = src + (dr.LengthOfDirectoryRecord - 34);
    int len = dr.LengthOfDirectoryRecord - 34;

    if (dr.LengthOfFileIdentifier % 2 == 0)
    {
        srcSU++;
    }

    // koukneme, jestli v 'system use area' neni nejake rozsireni
    if (srcSU < srcEnd && (Ext != extJoliet))
    {
        EExt ext = extNone;

        CRRExtHeader rrHeader;
        memcpy(&rrHeader, srcSU, sizeof(rrHeader));

        if (strncmp((char*)rrHeader.Signature, "RR", 2) == 0)
            ext = extRockRidge; // je to RockRigde

        char extFileName[2 * MAX_PATH + 1];
        ZeroMemory(extFileName, sizeof(extFileName));

        switch (ext)
        {
        case extRockRidge:
        {
            BOOL bNM = FALSE;

            while (srcSU < srcEnd)
            {
                memcpy(&rrHeader, srcSU, sizeof(rrHeader));

                if (strncmp((char*)rrHeader.Signature, "NM", 2) == 0)
                {
                    bNM = TRUE;
                    strncat(extFileName, srcSU + 5, rrHeader.Length - 5);
                }

                srcSU += rrHeader.Length;
            } // while

            if (bNM)
            {
                strcpy(fileName, extFileName);
                return;
            }
        }
        break;

        default:
            break;
        } // switch
    }

    lstrcpyn(fileName, src, dr.LengthOfFileIdentifier + 1);
}

//
// Vysekne ze 'src' jmeno souboru
// pouziti pro nazvy souboru v iso level 1,2,3
//
void CISO9660::ExtractFileName(char* fileName, const char* src, CISO9660::CDirectoryRecord& dr)
{
    char tmpFileName[2 * MAX_PATH + 1];
    ZeroMemory(tmpFileName, sizeof(tmpFileName));

    ExtractExtFileName(tmpFileName, src, dr);

    // iso filename
    char* sep2 = strchr(tmpFileName, ISO_SEPARATOR2);

    if (sep2 != NULL)
    {
        *sep2 = '\0';

        char* sep1 = strchr(tmpFileName, ISO_SEPARATOR1);
        if (sep1 && sep2 - sep1 == 1)
            *sep1 = '\0';
    }

    strcpy(fileName, tmpFileName);
}

#define READ_SIZE 4096

void CISO9660::ConvJolietName(char* dest, const char* src, int nLen)
{
    char tmp[2 * MAX_PATH];
    ZeroMemory(&tmp, sizeof(tmp));

    memcpy(tmp, src, nLen);

    WCHAR* uname = (WCHAR*)tmp;
    int i;
    for (i = 0; i < (nLen / 2); i++)
        uname[i] = (WORD)ROTATE(uname[i]);

    char final[2 * MAX_PATH];
    ZeroMemory(&final, sizeof(final));
    WideCharToMultiByte(CP_ACP, 0, uname, nLen / 2, final, sizeof(final) - 1, 0, 0);
    final[sizeof(final) - 1] = 0;

    strcpy(dest, final);
}

BOOL CISO9660::AddFileDir(const char* path, char* fileName, CDirectoryRecord& dr,
                          CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    CFileData fd;

    memset(&fd, 0, sizeof(CFileData));
    fd.Name = SalamanderGeneral->DupStr(fileName);
    if (fd.Name == NULL)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return FALSE;
    } // if

    fd.NameLen = strlen(fd.Name);
    char* s = strrchr(fd.Name, '.');
    if (s != NULL)
        fd.Ext = s + 1; // ".cvspass" is extension in Windows
    else
        fd.Ext = fd.Name + fd.NameLen;

    CISOImage::CFilePos* filePos = new CISOImage::CFilePos;
    if (!filePos)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return FALSE;
    }

    filePos->Extent = (DWORD)dr.LocationOfExtent;
    filePos->Type = FS_TYPE_ISO9660;

    char u[1204];
    sprintf(u, "CISO9960::AddFileDir(): name: %s, Extent: 0x%X", fileName, dr.LocationOfExtent);
    TRACE_I(u);

    fd.PluginData = (DWORD_PTR)filePos;

    // set date & time
    if (memcmp(&dr.RecordingDateAndTime, "\x00\x00\x00\x00\x00\x00\x00",
               sizeof(dr.RecordingDateAndTime)) != 0)
    {
        ISODateTimeToFileTime(dr.RecordingDateAndTime, &fd.LastWrite);
    }
    else
        fd.LastWrite = Image->GetLastWrite();

    fd.DosName = NULL;

    fd.Attr = FILE_ATTRIBUTE_READONLY; // Everything is read-only by default
    fd.Hidden = 0;
    fd.IsOffline = 0;
    if (dr.FileFlags & FATTR_HIDDEN)
    {
        fd.Attr |= FILE_ATTRIBUTE_HIDDEN;
        fd.Hidden = 1;
    }

    if (dr.FileFlags & FATTR_DIRECTORY)
    {
        fd.Size = CQuadWord(0, 0);
        fd.Attr |= FILE_ATTRIBUTE_DIRECTORY;
        fd.IsLink = 0;

        if (!SortByExtDirsAsFiles)
            fd.Ext = fd.Name + fd.NameLen; // adresare nemaji priponu

        if (dir && !dir->AddDir(path, fd, pluginData))
        {
            free(fd.Name);
            delete filePos;
            dir->Clear(pluginData);
            return Error(IDS_ERROR);
        }
    }
    else
    {
        fd.Size = CQuadWord(dr.DataLength, 0);

        // soubor
        fd.IsLink = SalamanderGeneral->IsFileLink(fd.Ext);
        if (dir && !dir->AddFile(path, fd, pluginData))
        {
            free(fd.Name);
            dir->Clear(pluginData);
            delete filePos;
            return Error(IDS_ERROR);
        }
    }

    return TRUE;
}

char* CISO9660::GetBootRecordTypeStr(EBootRecordType type)
{
    CALL_STACK_MESSAGE1("CISO9660::GetBootRecordTypeStr()");

    static char buffer[8];

    ZeroMemory(buffer, 8);
    buffer[0] = '-';

    switch (type)
    {
    case biNoEmul:
        strcat(buffer, "noemul");
        break;
    case bi120:
        strcat(buffer, "1.2");
        break;
    case bi144:
        strcat(buffer, "1.44");
        break;
    case bi288:
        strcat(buffer, "2.88");
        break;
    case biHDD:
        strcat(buffer, "hdd");
        break;
    } // switch

    return buffer;
}

BOOL CISO9660::AddBootRecord(char* path, int session,
                             CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    // add boot image file
    if (BootRecordInfo != NULL)
    {

        CDirectoryRecord dr;
        ZeroMemory(&dr, sizeof(dr));

        // nastavit virtualni directory record
        dr.FileFlags = 0;
        dr.LocationOfExtent = BootRecordInfo->LoadRBA;
        //    dr.RecordingDateAndTime = ;
        dr.DataLength = BootRecordInfo->Length;

        char fileName[MAX_PATH];
        if (session == -1)
        {
            session = 1;
            sprintf(fileName, "s%02d-bootdisk%s.ima", session, GetBootRecordTypeStr(BootRecordInfo->Type));
            if (!AddFileDir("\\", fileName, dr, dir, pluginData))
                return FALSE;
            strcat(path, "Session 01");
        }
        else
        {
            sprintf(fileName, "s%02d-bootdisk%s.ima", session, GetBootRecordTypeStr(BootRecordInfo->Type));
            if (!AddFileDir("\\", fileName, dr, dir, pluginData))
                return FALSE;
        }
    }

    return TRUE;
}

BOOL CISO9660::ListDirectory(char* path, int session,
                             CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE3("CISO9660::ListDirectory(%s, %d, , )", path, session);

    AddBootRecord(path, session, dir, pluginData);
    return ListDirectoryRe(path, &Root, dir, pluginData) != ERR_TERMINATE;
}

//
int CISO9660::ListDirectoryRe(char* path, CDirectoryRecord* root,
                              CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    if (root == NULL)
        return ERR_TERMINATE;

    DWORD block = (DWORD)root->LocationOfExtent - ExtentOffset + root->ExtendedAttributeRecordLength;
    int size = root->DataLength;

    if (block == 0 || size == 0)
        return ERR_TERMINATE;

    char* data = new char[size];
    if (data == NULL)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return ERR_TERMINATE;
    }

    if (Image->ReadBlock(block, size, data) != (DWORD)size)
    {
        delete[] data;
        Error(IDS_ERROR_LISTING_IMAGE, FALSE, block);
        // pokud se nepodari nacist sektor s rootem
        return (block == (DWORD)Root.LocationOfExtent - ExtentOffset) ? ERR_CONTINUE : ERR_TERMINATE;
    }

    int ret = ERR_OK;
    int offset = 0;
    while (offset < size - 0x21 && ret != ERR_TERMINATE)
    {
        while (!data[offset] && offset < size - 0x21)
            offset++;

        if (offset >= size - 0x21)
            break;

        //
        CDirectoryRecord dirRecord;

        FillDirectoryRecord(dirRecord, (BYTE*)(data + offset));

        // Safety check
        if ((dirRecord.LocationOfExtent != SWAPDWORD(dirRecord.LocationOfExtentA)) || (dirRecord.DataLength != SWAPDWORD(dirRecord.DataLengthA)))
        {
            Error(IDS_ERROR_LISTING_IMAGE, FALSE, block);
            return ERR_TERMINATE;
        }

        if (dirRecord.FileFlags & FATTR_DIRECTORY)
        {
            if (Ext == extJoliet)
            {
                if (dirRecord.LengthOfFileIdentifier > 1)
                {
                    char dirName[2 * MAX_PATH];
                    ZeroMemory(&dirName, sizeof(dirName));
                    ConvJolietName(dirName, (data + offset + 33), dirRecord.LengthOfFileIdentifier);
                    if (AddFileDir(path, dirName, dirRecord, dir, pluginData))
                    {
                        int pathLen = (int)strlen(path);
                        strcat(path, "\\");
                        strcat(path, dirName);
                        // zanorit jen kdyz je vse OK
                        if (ret == ERR_OK)
                        {
                            ret = ListDirectoryRe(path, &dirRecord, dir, pluginData);
                            // kdyz se vynorime s ukoncovaci chybou, pokracovat ve zpracovani co to pujde
                            if (ret == ERR_TERMINATE)
                                ret = ERR_CONTINUE;
                        }
                        path[pathLen] = '\0';
                    }
                    else
                        ret = ERR_TERMINATE;
                } // if
            }
            else
            {
                char firstChar = data[offset + 33];
                if (firstChar != 0x00 && firstChar != 0x01)
                {
                    char extFileName[2 * MAX_PATH + 1];
                    ZeroMemory(extFileName, sizeof(extFileName));
                    ExtractExtFileName(extFileName, (data + offset + 33), dirRecord);

                    if (AddFileDir(path, extFileName, dirRecord, dir, pluginData))
                    {
                        int pathLen = (int)strlen(path);
                        strcat(path, "\\");
                        strcat(path, extFileName);
                        //          TRACE_I(path);
                        // zanorit jen kdyz je vse OK
                        if (ret == ERR_OK)
                        {
                            ret = ListDirectoryRe(path, &dirRecord, dir, pluginData);
                            // kdyz se vynorime s ukoncovaci chybou, pokracovat ve zpracovani co to pujde
                            if (ret == ERR_TERMINATE)
                                ret = ERR_CONTINUE;
                        }
                        path[pathLen] = '\0';
                    }
                    else
                        ret = ERR_TERMINATE;
                } // if
            }     // if
        }
        else
        {
            char fileName[2 * MAX_PATH + 1];
            ZeroMemory(&fileName, sizeof(fileName));

            char* src = (data + offset + 33);
            if (Ext == extJoliet)
            {
                ConvJolietName(fileName, src, dirRecord.LengthOfFileIdentifier);
                src = fileName;
            }

            ExtractFileName(fileName, src, dirRecord);

            if (!AddFileDir(path, fileName, dirRecord, dir, pluginData))
                ret = ERR_TERMINATE;
        } // if

        offset += dirRecord.LengthOfDirectoryRecord;
    } // while

    delete[] data;

    return ret;
}

int CISO9660::UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path, const char* nameInArc,
                         const CFileData* fileData, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE6("CISO9660::UnpackFile( , %s, %s, %s, , %u, %d)", srcPath, path, nameInArc, silent, toSkip);

    ///
    char name[MAX_PATH];
    strncpy_s(name, path, _TRUNCATE);
    if (!SalamanderGeneral->SalPathAppend(name, fileData->Name, MAX_PATH))
    {
        Error(IDS_ERR_TOO_LONG_NAME);
        return UNPACK_ERROR;
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

    // celkova operace muze pokracovat dal. pouze skip
    if (toSkip)
        return UNPACK_ERROR;

    // celkova operace nemuze pokracovat dal. cancel
    if (hFile == INVALID_HANDLE_VALUE)
        return UNPACK_CANCEL;

    ///

    BOOL bFileComplete = TRUE;

    BOOL ret = UNPACK_OK;
    CISOImage::CFilePos* fp = (CISOImage::CFilePos*)fileData->PluginData;
    DWORD block = fp->Extent - ExtentOffset;
    CQuadWord remain = fileData->Size;
    DWORD sectorUserSize = 0x800;
    DWORD nbytes = sectorUserSize;
    BYTE* buffer = new BYTE[sectorUserSize];
    if (!buffer)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return UNPACK_CANCEL;
    }
    /*
  char u[1024];
  sprintf(u, "CISO9660::UnpackFile(): opening Extent: %x, Size: %d", block, remain);
  TRACE_I(u);
*/
    while (remain.Value > 0)
    {
        if (remain.Value < nbytes)
            nbytes = remain.LoDWord; // !!! velikost bufferu nesmi byt vetsi nez DWORD

        if (!Image->ReadBlock(block, sectorUserSize, buffer))
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

        block++;

        if (!salamander->ProgressAddSize(nbytes, TRUE)) // delayedPaint==TRUE, abychom nebrzdili
        {
            salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING_OPERATION), FALSE);
            salamander->ProgressEnableCancel(FALSE);

            ret = UNPACK_CANCEL;
            bFileComplete = FALSE;
            break; // preruseni akce
        }

        ULONG written;
        if (!file.Write(buffer, nbytes, &written, name, NULL))
        {
            // Error message was already displayed by SafeWriteFile()
            ret = UNPACK_CANCEL;
            bFileComplete = FALSE;
            break;
        }
        remain.Value -= nbytes;
    } // while

    delete[] buffer;

    //  sprintf(u, "CISOImage::UnpackFile(): ret: %d, bFileComplete: %d", ret, bFileComplete);
    //  TRACE_I(u);

    if (!file.Close(name, NULL))
    {
        // Flushing cache may fail
        ret = UNPACK_CANCEL;
        bFileComplete = FALSE;
    }

    if (!bFileComplete)
    {
        // protoze je vytvoren s read-only atributem, musime R attribut
        // shodit, aby sel soubor smazat
        attrs &= ~FILE_ATTRIBUTE_READONLY;
        if (!SetFileAttributes(name, attrs))
            Error(LoadStr(IDS_CANT_SET_ATTRS), GetLastError());

        // user zrusil operaci
        // smazat po sobe neuplny soubor
        if (!DeleteFile(name))
            Error(LoadStr(IDS_CANT_DELETE_TEMP_FILE), GetLastError());
    }
    else
        SetFileAttrs(name, attrs);

    return ret;
}

BOOL CISO9660::DumpInfo(FILE* outStream)
{
    char* s;

    CALL_STACK_MESSAGE1("CISO9660::DumpInfo()");

    // zobrazit info z PVD
    s = ViewerStrNcpy((char*)PVD.SystemIdentifier, 32);
    if (*s)
        fprintf(outStream, "    System Identifier:        %s\n", s);
    s = ViewerStrNcpy((char*)PVD.VolumeIdentifier, 32);
    if (*s)
        fprintf(outStream, "    Volume:                   %s\n", s);
    s = ViewerStrNcpy((char*)PVD.VolumeSetIdentifier, 128);
    if (*s)
        fprintf(outStream, "    Volume Set:               %s\n", s);
    s = ViewerStrNcpy((char*)PVD.PublisherIdentifier, 128);
    if (*s)
        fprintf(outStream, "    Publisher:                %s\n", s);
    s = ViewerStrNcpy((char*)PVD.DataPreparerIdentifier, 128);
    if (*s)
        fprintf(outStream, "    Data Preparer:            %s\n", s);
    s = ViewerStrNcpy((char*)PVD.ApplicationIdentifier, 128);
    if (*s)
        fprintf(outStream, "    Application:              %s\n", s);
    s = ViewerStrNcpy((char*)PVD.CopyrightFileIdentifier, 37);
    if (*s)
        fprintf(outStream, "    Copyright File:           %s\n", s);
    s = ViewerStrNcpy((char*)PVD.AbstractFileIdentifier, 37);
    if (*s)
        fprintf(outStream, "    Abstract File:            %s\n", s);
    s = ViewerStrNcpy((char*)PVD.BibliographicFileIdentifier, 37);
    if (*s)
        fprintf(outStream, "    Bibliographic File:       %s\n", s);

    //  fprintf(outStream, "\n");

    SYSTEMTIME st;
    ISODateTimeStrToSystemTime(PVD.VolumeCreationDateAndTime, &st);
    if (st.wYear != 0)
        fprintf(outStream, "    Volume Creation Date:     %s\n", ViewerPrintSystemTime(&st));

    ISODateTimeStrToSystemTime(PVD.VolumeModificationDateAndTime, &st);
    if (st.wYear != 0)
        fprintf(outStream, "    Volume Modification Date: %s\n", ViewerPrintSystemTime(&st));

    ISODateTimeStrToSystemTime(PVD.VolumeExpirationDateAndTime, &st);
    if (st.wYear != 0)
        fprintf(outStream, "    Volume Expiration Date:   %s\n", ViewerPrintSystemTime(&st));

    ISODateTimeStrToSystemTime(PVD.VolumeEffectiveDateAndTime, &st);
    if (st.wYear != 0)
        fprintf(outStream, "    Volume Effective Date:    %s\n", ViewerPrintSystemTime(&st));

    return TRUE;
}

BOOL CISO9660::ReadBootRecord(BYTE* data, BOOL quiet)
{
    CALL_STACK_MESSAGE2("CISO9660::ReadBootRecord(, %d)", quiet);

    memcpy(&BR, data, sizeof(BR));

    BOOL result = FALSE;

    if (!Options.BootImageAsFile)
        return FALSE;

    // check for El Torito specification
    if (memcmp(BR.BootSystemIdentifier + 0, "EL TORITO SPECIFICATION", 23) == 0)
    {
        DWORD sector = 0;
        memcpy(&sector, data + 0x47, sizeof(DWORD));

        BYTE* catalog = new BYTE[SECTOR_SIZE];
        if (catalog == NULL)
        {
            return Error(IDS_INSUFFICIENT_MEMORY, quiet);
        }

        if (Image->ReadBlock(sector - ExtentOffset, SECTOR_SIZE, catalog) == SECTOR_SIZE)
        {
            result = ReadElTorito(catalog, SECTOR_SIZE, quiet);
        }
        else
        {
            result = Error(IDS_ERROR_READING_SECTOR, FALSE, sector - ExtentOffset);
        }

        delete[] catalog;
    }

    // and finally
    return result;
}
