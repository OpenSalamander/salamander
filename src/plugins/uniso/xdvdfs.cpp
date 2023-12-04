// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "isoimage.h"
#include "xdvdfs.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang/lang.rh"

#define SECTOR_SIZE 0x0800

#define XBOX_READONLY 0x0001
#define XBOX_HIDDEN 0x0002
#define XBOX_SYSTEM 0x0004
#define XBOX_DIRECTORY 0x0010
#define XBOX_ARCHIVE 0x0020
#define XBOX_NORMAL 0x0080

/* For direct data access, LSB first */
#define GET_BYTE(data, p) ((Uint8)data[p])
#define GET_WORD(data, p) ((Uint16)data[p] | ((Uint16)data[(p) + 1] << 8))
#define GET_DWORD(data, p) ((Uint32)data[p] | ((Uint32)data[(p) + 1] << 8) | ((Uint32)data[(p) + 2] << 16) | ((Uint32)data[(p) + 3] << 24))
#define GET_QWORD(data, p) ((Uint64)data[p] | ((Uint64)data[(p) + 1] << 8) | ((Uint64)data[(p) + 2] << 16) | ((Uint64)data[(p) + 3] << 24) | ((Uint64)data[(p) + 4] << 32) | ((Uint64)data[(p) + 5] << 40) | ((Uint64)data[(p) + 6] << 48) | ((Uint64)data[(p) + 7] << 56))
/* This is wrong with regard to endianess */
#define GETN(data, p, n, target) memcpy(target, &data[p], n)

// ****************************************************************************
//
// CXDVDFS
//

CXDVDFS::CXDVDFS(CISOImage* image, DWORD extent) : CUnISOFSAbstract(image)
{
    ExtentOffset = extent;
}

CXDVDFS::~CXDVDFS()
{
}

BOOL CXDVDFS::ReadBlockPhys(Uint32 lbNum, size_t blocks, unsigned char* data)
{
    DWORD len = (DWORD)(blocks * SECTOR_SIZE);

    if (Image->ReadBlock(lbNum, len, data) == len)
        return TRUE;
    else
        return FALSE;
}

BOOL CXDVDFS::Open(BOOL quiet)
{
    CALL_STACK_MESSAGE2("CXDVDFS::Open(%d)", quiet);

    BOOL ret = TRUE;

    BYTE sector[SECTOR_SIZE]; // (2k)
    int errorID = 0;

    // Try to read a volume descriptor
    if (Image->ReadBlock(0x20, SECTOR_SIZE, sector) == SECTOR_SIZE)
    {
        VD.RootSector = GET_DWORD(sector, 0x14);
        VD.RootDirectorySize = GET_DWORD(sector, 0x18);
        GETN(sector, 0x1C, 8, &VD.CreationTime);
    }
    else
    {
        Error(IDS_ERROR_READING_SECTOR, quiet, 0x20);
        ret = FALSE;
    }

    return ret;
}

BOOL CXDVDFS::AddFileDir(const char* path, char* fileName, CDirectoryEntry* de,
                         CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    CFileData fd;
    CISOImage::CFilePos* filePos = NULL;
    BOOL ret = TRUE;

    try
    {
        fd.Name = SalamanderGeneral->DupStr(fileName);
        if (fd.Name == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw FALSE;
        } // if

        fd.NameLen = strlen(fd.Name);
        char* s = strrchr(fd.Name, '.');
        if (s != NULL)
            fd.Ext = s + 1; // ".cvspass" is extension in Windows
        else
            fd.Ext = fd.Name + fd.NameLen;

        filePos = new CISOImage::CFilePos;
        if (filePos == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw FALSE;
        }

        filePos->Extent = (DWORD)de->StartSector;

        /*
    char u[1024];
    sprintf(u, "CUDF::AddFileDir(): opening Extent: %x", icb->Location);
    TRACE_I(u);
*/

        fd.PluginData = (DWORD_PTR)filePos;

        fd.LastWrite = VD.CreationTime;
        fd.DosName = NULL;

        fd.Attr = FILE_ATTRIBUTE_READONLY; // vse je defaultne read-only
        fd.Hidden = 0;
        fd.IsOffline = 0;

        if (de->Attr & XBOX_HIDDEN)
        {
            fd.Attr |= FILE_ATTRIBUTE_HIDDEN;
            fd.Hidden = 1;
        }

        if (de->Attr & XBOX_DIRECTORY)
        {
            fd.Size = CQuadWord(0, 0);
            fd.Attr |= FILE_ATTRIBUTE_DIRECTORY;
            fd.IsLink = 0;

            if (!SortByExtDirsAsFiles)
                fd.Ext = fd.Name + fd.NameLen; // adresare nemaji priponu

            if (dir && !dir->AddDir(path, fd, pluginData))
            {
                Error(IDS_ERROR);
                throw FALSE;
            }
        }
        else
        {
            fd.Size.Value = de->FileSize;

            // soubor
            fd.IsLink = SalamanderGeneral->IsFileLink(fd.Ext);
            if (dir && !dir->AddFile(path, fd, pluginData))
            {
                Error(IDS_ERROR);
                throw FALSE;
            }
        }
    }
    catch (BOOL e)
    {
        delete filePos;

        ret = e;
    }

    return ret;
}

BOOL CXDVDFS::ListDirectory(char* path, int session, CSalamanderDirectoryAbstract* dir,
                            CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE3("CXDVDFS::ListDirectory(%s, %d, , )", path, session);

    return ScanDir(VD.RootSector, VD.RootDirectorySize, path, dir, pluginData) != ERR_TERMINATE;
}

int CXDVDFS::ScanDir(DWORD sector, DWORD size, char* path, CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    BYTE* data = new BYTE[size];
    if (data == NULL)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return ERR_TERMINATE;
    }

    // read root
    if (Image->ReadBlock(sector, size, data) != size)
    {
        delete[] data;
        Error(IDS_ERROR_LISTING_IMAGE, FALSE, sector);
        // pokud se nepodari nacist sektor s rootem
        return (sector == VD.RootSector) ? ERR_CONTINUE : ERR_TERMINATE;
    }

    DWORD offset = 0;
    int ret = ERR_OK;
    while ((offset + 0x0E) <= size && (ret != ERR_TERMINATE))
    {
        if (*(Uint32*)(data + offset) == 0xFFFFFFFF)
        {
            // Looks like end of sector, the next entry wouldn't fit -> skip padding and start the new sector. See mk7-xbox-iso.iso.
            // NOTE: As offset is multiple of 4, the minimum padding size is 4.
            while ((offset < size) && (data[offset] == 0xFF) && (0 != (offset & (SECTOR_SIZE - 1))))
            {
                offset++;
            }
            if ((offset + 0x0E > size) || (0 != (offset & (SECTOR_SIZE - 1))))
            {
                // Unexpected: either the rest of the data was padded or the padding didn't reach the end of the sector
                // -> bail out to prevent unpredictable behavior
                break;
            }
        }

        // get data
        CDirectoryEntry de;
        de.StartSector = GET_DWORD(data, offset + 0x0004);
        de.FileSize = GET_DWORD(data, offset + 0x0008);
        de.Attr = GET_BYTE(data, offset + 0x000C);

        // get filename
        BYTE len = GET_BYTE(data, offset + 0x000D);
        char fileName[2 * MAX_PATH + 1];
        strncpy_s(fileName, (char*)data + offset + 0x000E, len);

        if (!AddFileDir(path, fileName, &de, dir, pluginData))
        {
            delete[] data;
            return ERR_CONTINUE;
        }

        if ((de.Attr & XBOX_DIRECTORY) && de.FileSize != 0 && de.StartSector != 0)
        {
            int pathLen = (int)strlen(path);
            strcat(path, "\\");
            strcat(path, fileName);

            // zanorit jen kdyz je vse OK
            if (ret == ERR_OK)
            {
                ret = ScanDir(de.StartSector, de.FileSize, path, dir, pluginData);
                // kdyz se vynorime s ukoncovaci chybou, pokracovat ve zpracovani co to pujde
                if (ret == ERR_TERMINATE)
                    ret = ERR_CONTINUE;
            }
            path[pathLen] = '\0';
        }

        offset += 0x000E + len;
        // zarovnat offset na dword
        offset = ((offset + 3) / 4) * 4;
    }

    delete[] data;

    return ret;
}

int CXDVDFS::UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                        const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE7("CXDVDFS::UnpackFile(, %s, %s, %s, %p, %u, %d)", srcPath, path, nameInArc, fileData, silent, toSkip);

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

        // celkova operace muze pokracovat dal. pouze skip
        if (toSkip)
            throw UNPACK_ERROR;

        // celkova operace nemuze pokracovat dal. cancel
        if (hFile == INVALID_HANDLE_VALUE)
            throw UNPACK_CANCEL;

        BOOL bFileComplete = TRUE;

        /*
    BYTE track = Image->GetTrackFromExtent(fp->Extent);
    if (!Image->OpenTrack(track)) {
      // je treba otevrit track, ze ktereho budeme cist (nastaveni paramtru tracku)
      Error(IDS_CANT_OPEN_TRACK, silent == 1, track);
      throw UNPACK_ERROR;
    }
*/
        DWORD block = fp->Extent; // - extentOfs
        CQuadWord remain = fileData->Size;

        DWORD nbytes = SECTOR_SIZE;
        while (remain.Value > 0)
        {
            if (remain.Value < nbytes)
                nbytes = remain.LoDWord; // !!! velikost bufferu nesmi byt vetsi nez DWORD

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

            if (!salamander->ProgressAddSize(nbytes, TRUE)) // delayedPaint==TRUE, abychom nebrzdili
            {
                salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING_OPERATION), FALSE);
                salamander->ProgressEnableCancel(FALSE);

                ret = UNPACK_CANCEL;
                bFileComplete = FALSE;
                break; // preruseni akce
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

        //    sprintf(u, "CISOImage::UnpackFile(): ret: %d, bFileComplete: %d", ret, bFileComplete);
        //    TRACE_I(u);

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
    }
    catch (int e)
    {
        ret = e;
    }

    delete[] sector;

    return ret;
}

BOOL CXDVDFS::DumpInfo(FILE* outStream)
{
    CALL_STACK_MESSAGE1("CXDVDFS::DumpInfo( )");

    SYSTEMTIME st;
    FileTimeToSystemTime(&VD.CreationTime, &st);
    fprintf(outStream, "    Creation Date:                %s\n", ViewerPrintSystemTime(&st));

    return TRUE;
}
