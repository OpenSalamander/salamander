// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "isoimage.h"
#include "hfs.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#define BUFFER_SIZE 32768

#pragma runtime_checks("c", off)

// Based on Apple Technical Note TN1150

UInt64 FromM64(UInt64 x)
{
    return (((UInt64)FromM32((UInt32)x)) << 32) | FromM32(x >> 32);
}

static void ConvertHFSDate(FILETIME& ft, UInt32 HFSDate)
{
    SYSTEMTIME st;
    struct tm* gmtm;
    time_t time2;

    time2 = FromM32(HFSDate); // In seconds since midnight January 1, 1904, GMT
    // Convert to 1.1.1970
    time2 -= 365 * 24 * 60 * 60 * (1970 - 1904) + 17 * 24 * 60 * 60 /*leap days*/;
    gmtm = gmtime(&time2);
    if (!gmtm)
    {
        time2 = time(NULL);
        gmtm = gmtime(&time2);
    }
    st.wYear = gmtm->tm_year + 1900;
    st.wMonth = gmtm->tm_mon + 1;
    st.wDay = gmtm->tm_mday;
    st.wHour = gmtm->tm_hour;
    st.wMinute = gmtm->tm_min;
    st.wSecond = gmtm->tm_sec;
    st.wMilliseconds = st.wDayOfWeek = 0;

    SystemTimeToFileTime(&st, &ft);
}

static void FixIllegalFSChars(char* s)
{
    char* s2 = s + strlen(s);

    // Remove trailing spaces
    while ((s2 > s) && (s2[-1] == ' '))
        *--s2 = 0;

    // Just empty path (or consisting of spaces only)?
    if (!*s)
    {
        // Firefox 3.0.7.dmg contains link called " "
        *s = '_';
        s[1] = 0;
        return;
    }
    if ((s2 > s) && (s2[-1] == '/'))
    {
        // HDD_Installer_MacOSX.dmg contains .dmg with "HASP Installation/"
        s2[-1] = 0;
    }

    while (*s)
    {
        if (IsDBCSLeadByte(*s))
        {
            s++; // Skip both the lead and following bytes
            if (!*s)
                break; // Invalid string! Lead byte followed by terminating byte!
        }
        else
        {
            if (((unsigned char)*s < 32) || (*s == '?') || (*s == '\\') || (*s == '*'))
                *s = '_';
        }
        s++;
    }
}

CHFS::CHFS(CISOImage* image) : CUnISOFSAbstract(image)
{
    SectorSize = 0;
    PartStart = 0;
    HFSWrapperOfs = 0;
    pCatalog = NULL;
    nFolders = nAllocedFolders = 0;
    pFolders = NULL;
    bSkipRootParent = false;
}

CHFS::~CHFS()
{
    delete pCatalog;
    if (pFolders)
    {
        for (int i = 0; i < nFolders; i++)
            free(pFolders[i]);
        free(pFolders);
    }
}

BOOL CHFS::Open(BOOL quiet)
{
    Block0 block0; // 1st sector on Block Devices, something like MBR
    DWORD dwBytesRead;
    Partition partition; // Entry in Partition Map

    CALL_STACK_MESSAGE2("CHFS::Open(%d)", quiet);

    HFSWrapperOfs = Image->GetSectorOffset(0);
    SeekSector(0);

    if (!Read(&block0, sizeof(block0), &dwBytesRead))
        return FALSE;

    if (sizeof(block0) != dwBytesRead)
        return Error(IDS_ERR_HFS_READ, quiet);

    if (block0.sbSig == HFS_DEVICE_SIGNATURE)
    {
        SectorSize = FromM16(block0.sbBlkSize);

        DWORD nPartitions = 1;
        for (DWORD i = 0; i < nPartitions; i++)
        {
            SeekSector(i + 1);

            if (!Read(&partition, sizeof(partition), &dwBytesRead))
                return FALSE;

            if ((sizeof(partition) != dwBytesRead) || (partition.pmSig != HFS_PARTITION_SIGNATURE) || (partition.pmSigPad != 0))
                return Error(IDS_ERR_HFS_INVALID_PARTITION, quiet);

            // Each PM entry contains the same # of items in the entire PM
            if ((i > 0) && (nPartitions != FromM32(partition.pmMapBlkCnt)))
                return Error(IDS_ERR_HFS_INVALID_PARTITION, quiet);

            nPartitions = FromM32(partition.pmMapBlkCnt);
            if (!strcmp(partition.pmParType, "Apple_HFS"))
            {
                PartStart = FromM32(partition.pmPyPartStart);
            }
        }
        if (!PartStart)
            return Error(IDS_ERR_HFS_INVALID_PARTITION, quiet);
    }
    else if (block0.sbSig == 0)
    { // It may be just a HFS+ partition
        PartStart = 0;
        SectorSize = 512;
    }
    else
    {
        return Error(IDS_ERR_HFS_UNRECOGNIZED, quiet); // Can this happen???
    }

    // VolumeHeader is stored at sector 2 & second to last sector (starting 1024 bytes before the end)
    // NOTE: SectorSize might actually be 2048
    SeekSector(PartStart);
    SeekRel(2 * 512);
    if (!Read(&VolHeader, sizeof(VolHeader), &dwBytesRead))
        return FALSE;

    if (sizeof(VolHeader) != dwBytesRead)
        return Error(IDS_ERR_HFS_READ, quiet);

    if (VolHeader.signature == HFS_PARTITION_HFS_SIGNATURE)
    {
        MDBRecord mdb;

        // It is a HFS, not HFS partition. Look for wrapped HFS+ partition
        SeekRel(-(int)sizeof(VolHeader));
        if (!Read(&mdb, sizeof(mdb), &dwBytesRead))
            return FALSE;

        if (sizeof(mdb) != dwBytesRead)
            return Error(IDS_ERR_HFS_READ, quiet);

        if ((mdb.drSigWord != HFS_PARTITION_HFS_SIGNATURE) || (mdb.drEmbedSigWord != kHFSPlusSigWord))
            return Error(IDS_ERR_HFS_ORIGINAL_HFS, quiet);

        HFSWrapperOfs += 512 * (UInt32)(FromM16(mdb.drAlBlSt) + FromM16(mdb.drEmbedExtent.xdrStABN) * (FromM32(mdb.drAlBlkSiz) / 512));
        SeekSector(PartStart);
        SeekRel(2 * 512);
        if (!Read(&VolHeader, sizeof(VolHeader), &dwBytesRead))
            return FALSE;

        if (sizeof(VolHeader) != dwBytesRead)
            return Error(IDS_ERR_HFS_READ, quiet);
    }

    if ((VolHeader.signature != kHFSPlusSigWord) || (VolHeader.version != FromM16(kHFSPlusVersion)))
        return Error(IDS_ERR_HFS_UNRECOGNIZED, quiet);

    VolHeader.blockSize = FromM32(VolHeader.blockSize);

    pCatalog = new BTree(this, &VolHeader.catalogFile, quiet);
    if (!pCatalog)
        return Error(IDS_INSUFFICIENT_MEMORY, quiet);

    if (!pCatalog->IsOK())
        return FALSE;

    return TRUE;
}

// Seeks on disk (sector is usually 512 or 2048 bytes)
// Returns TRUE on success
BOOL CHFS::SeekSector(int sector)
{
    __int64 newPos = (__int64)sector * SectorSize + HFSWrapperOfs;
    return (newPos == Image->File->Seek(newPos, FILE_BEGIN)) ? TRUE : FALSE;
}

// Seeks on HFS+ partition (block is usually 4096 bytes)
// Returns TRUE on success
BOOL CHFS::SeekBlock(int block)
{
    __int64 newPos = (__int64)PartStart * SectorSize + (__int64)block * VolHeader.blockSize + HFSWrapperOfs;
    return (newPos == Image->File->Seek(newPos, FILE_BEGIN)) ? TRUE : FALSE;
}

// Seeks relatively
// Returns new position
__int64 CHFS::SeekRel(int offset)
{
    return Image->File->Seek(offset, FILE_CURRENT);
}

BOOL CHFS::Read(LPVOID buf, DWORD dwBytesToRead, DWORD* dwBytesRead)
{
    // Error (when returning FALSE) handled by Read
    // However, dwBytesRead may be less than dwBytesToRead even when returning TRUE
    return Image->File->Read(buf, dwBytesToRead, dwBytesRead, Image->FileName, SalamanderGeneral->GetMainWindowHWND());
}

BOOL CHFS::DumpInfo(FILE* outStream)
{
    return FALSE;
}

BOOL CHFS::GetRootName(char* rootName, int maxlen)
{
    CALL_STACK_MESSAGE3("CHFS::GetRootName(%s, %i)", rootName, maxlen);
    int nRecords = pCatalog->GetNumRecords();

    for (int i = 0; i < min(5, nRecords); i++)
    {
        HFSPlusCatalogKey* pKey = (HFSPlusCatalogKey*)pCatalog->GetRecord(i);
        int keyLen = FromM16(pKey->keyLength);
        int nameLen = FromM16(pKey->nodeName.length);
        wchar_t fileNameW[256];

        if ((keyLen <= 6) || (nameLen == 0) || (pKey->parentID != FromM32(kHFSRootParentID)))
            continue; // skip threads
        for (int j = 0; j < nameLen; j++)
            fileNameW[j] = FromM16(pKey->nodeName.unicode[j]);
        fileNameW[nameLen] = 0;
        if (WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, fileNameW, nameLen + 1, rootName, maxlen, NULL, NULL) > 0)
        {
            rootName[maxlen - 1] = 0;
            bSkipRootParent = TRUE;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CHFS::ListDirectory(char* rootPath, int session,
                         CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE5("CHFS::ListDirectory(%s, %i, %p, %p)", rootPath, session, dir, pluginData);
    int nRecords = pCatalog->GetNumRecords();

    for (int i = 0; i < nRecords; i++)
    {
        HFSPlusCatalogKey* pKey = (HFSPlusCatalogKey*)pCatalog->GetRecord(i);
        int keyLen = FromM16(pKey->keyLength);
        int nameLen = FromM16(pKey->nodeName.length);
        wchar_t fileNameW[256];
        char fileName[256 * 2]; // twice the length for MBCS
        CFileData fd;
        char* path = rootPath;
        union
        {
            SInt16* type;
            HFSPlusCatalogFolder* dir;
            HFSPlusCatalogFile* file;
        } rec;

        if ((keyLen <= 6) || (nameLen == 0) || (bSkipRootParent && pKey->parentID == FromM32(kHFSRootParentID)))
        {
            continue; // Skip threads with empty name and RootParent already used as volume label
        }
        for (int j = 0; j < nameLen; j++)
            fileNameW[j] = FromM16(pKey->nodeName.unicode[j]);
        fileNameW[nameLen] = 0;
        if ((WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, fileNameW, nameLen + 1, fileName, sizeof(fileName), NULL, NULL) <= 0) || !fileName[0])
        {
            // Invalid or empty (thread,...) name -> silently skip
            continue;
        }

        FixIllegalFSChars(fileName);
        memset(&fd, 0, sizeof(CFileData));
        // File Name
        fd.Name = SalamanderGeneral->DupStr(fileName);
        if (!fd.Name)
        {
            return Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        }
        fd.NameLen = strlen(fd.Name);
        // Extension
        char* ext = strrchr(fd.Name, '.');
        if (ext != NULL)
            fd.Ext = ext + 1; // ".cvspass" is extension in Windows
        else
            fd.Ext = fd.Name + fd.NameLen;

        CISOImage::CFilePos* filePos = new CISOImage::CFilePos;
        if (!filePos)
        {
            SalamanderGeneral->Free(fd.Name);
            return Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        }
        filePos->Partition = 0; // Not initialized by ctor
        filePos->Extent = i;    // # of record in catalog file
        filePos->Type = FS_TYPE_HFS;
        fd.PluginData = (DWORD_PTR)filePos;

        if (fileName[0] == '.')
        {
            // Files and folders starting with dot are hidden
            fd.Attr |= FILE_ATTRIBUTE_HIDDEN;
            fd.Hidden = 1;
        }
        // Find the parent. The items are sorted according to parentID which means
        // our parent must have been parsed already.
        for (int j = 0; j < nFolders; j++)
        {
            if (pFolders[j]->id == pKey->parentID)
            {
                path = pFolders[j]->name;
                break;
            }
        }
        if (path == rootPath)
        {
            path = rootPath;
        }
        rec.type = (SInt16*)((char*)pKey + keyLen + sizeof(UInt16));
        if (*rec.type == kHFSFolderRecord)
        {
            ConvertHFSDate(fd.LastWrite, rec.dir->contentModDate ? rec.dir->contentModDate : rec.dir->createDate);

            size_t strElements = strlen(path) + 1 + strlen(fileName) + 1;
            FolderInfo* fi = (FolderInfo*)malloc(sizeof(FolderInfo) + strElements - 1); // 1 znak je jiz soucasti struktury FolderInfo
            if (fi)
            {
                fi->id = rec.dir->folderID;
                strcpy_s(fi->name, strElements, path); // diky strukture FolderInfo selhlava hlidani velikosti ciloveho bufferu pomoci sablon pro strcpy_s
                strcat_s(fi->name, strElements, "\\");
                strcat_s(fi->name, strElements, fileName);

                if (nFolders >= nAllocedFolders)
                {
                    FolderInfo** tmp = (FolderInfo**)realloc(pFolders, (nAllocedFolders + 32) * sizeof(void*));
                    if (!tmp)
                    {
                        // No more memory to remember more paths
                        free(fi);
                        SalamanderGeneral->Free(fd.Name);
                        delete filePos;
                        return Error(IDS_INSUFFICIENT_MEMORY, FALSE);
                    }
                    pFolders = tmp;
                    nAllocedFolders += 32;
                }
                if (!SortByExtDirsAsFiles)
                    fd.Ext = fd.Name + fd.NameLen; // adresare nemaji priponu
                if (dir->AddDir(path, fd, pluginData))
                {
                    pFolders[nFolders++] = fi;
                    continue;
                }
                // AddDir failed (too long path?) -> continue
                free(fi);
                Error(IDS_ERR_TOO_LONG_PATH, FALSE);
            }
            else
            {
                Error(IDS_INSUFFICIENT_MEMORY, FALSE);
            }
        }
        else if (*rec.type == kHFSFileRecord)
        {

            if ((rec.file->userInfo.fdCreator == kSymLinkCreator) && (rec.file->userInfo.fdType == kSymLinkFileType))
            {
                // Symbolic links. The data fork contains UTF8-encoded path to target
                fd.IsLink = true;
                //        SalamanderGeneral->Free(fd.Name);
                //        delete filePos;
                //        continue;
            }
            ConvertHFSDate(fd.LastWrite, rec.file->contentModDate ? rec.file->contentModDate : rec.file->createDate);
            fd.Size.Value = FromM64(rec.file->dataFork.logicalSize);
            if (rec.file->flags & FromM16(kHFSFileLockedMask))
                fd.Attr |= FILE_ATTRIBUTE_READONLY;

            if (!fd.IsLink)
                fd.IsLink = SalamanderGeneral->IsFileLink(fd.Ext);
            if (dir->AddFile(path, fd, pluginData))
                continue;
            // Failed -> fall through
            Error(IDS_ERR_TOO_LONG_PATH, FALSE);
        }
        SalamanderGeneral->Free(fd.Name);
        delete filePos;
        return FALSE;
    }
    return TRUE;
}

int CHFS::UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                     const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE4("CHFS::UnpackFile(, %s, %s, %s,,,)", srcPath, path, nameInArc);
    BOOL ret = UNPACK_OK;
    CISOImage::CFilePos* filePos = (CISOImage::CFilePos*)fileData->PluginData;
    HFSPlusCatalogKey* pKey = (HFSPlusCatalogKey*)pCatalog->GetRecord(filePos->Extent);
    int keyLen = FromM16(pKey->keyLength);
    union
    {
        SInt16* type;
        HFSPlusCatalogFolder* dir;
        HFSPlusCatalogFile* file;
    } rec;

    rec.type = (SInt16*)((char*)pKey + keyLen + sizeof(UInt16));
    if (*rec.type == kHFSFileRecord)
    {
        HFSPlusForkData* fork = &rec.file->dataFork;
        UInt64 size = fileData->Size.Value;
        DWORD attrs = fileData->Attr;
        HANDLE hFile;
        char name[MAX_PATH], fileInfo[100];
        FILETIME ft;
        BOOL bFileComplete = TRUE;
        CQuadWord qwSize;

        strncpy_s(name, path, _TRUNCATE);
        if (!SalamanderGeneral->SalPathAppend(name, fileData->Name, MAX_PATH))
        {
            Error(IDS_ERR_TOO_LONG_NAME, FALSE);
            return UNPACK_ERROR;
        }

        DWORD tmp = 0;
        for (int i = 0; i < 8; i++)
            tmp += FromM32(fork->extents[i].blockCount);

        if (tmp < FromM32(fork->totalBlocks))
        {
            // File fragmented, not supported yet
            Error(IDS_ERR_HFS_FILE_FRAGMENTED, FALSE, fileData->Name);
            return UNPACK_ERROR;
        }
        char* buffer = (char*)malloc((size_t)(min(BUFFER_SIZE, fileData->Size.Value)));
        if (!buffer)
        {
            Error(IDS_INSUFFICIENT_MEMORY, FALSE);
            return UNPACK_ERROR;
        }

        qwSize.Value = size;
        ft = fileData->LastWrite;
        GetInfo(fileInfo, &ft, fileData->Size);
        hFile = SalamanderSafeFile->SafeFileCreate(name, GENERIC_WRITE, FILE_SHARE_READ, attrs, FALSE,
                                                   SalamanderGeneral->GetMainWindowHWND(), nameInArc, fileInfo,
                                                   &silent, TRUE, &toSkip, NULL, 0, &qwSize, NULL);

        // Continue, but skip this file
        if (toSkip)
            return UNPACK_ERROR;

        // Full Cancel
        if (hFile == INVALID_HANDLE_VALUE)
            return UNPACK_CANCEL;

        CBufferedFile file(hFile, GENERIC_WRITE);
        // Set file time
        file.SetFileTime(&ft, &ft, &ft);

        for (int extInd = 0; (extInd < 8) && (size > 0) && bFileComplete; extInd++)
        {
            UInt64 extSize = VolHeader.blockSize * (UInt64)FromM32(fork->extents[extInd].blockCount);

            if (!SeekBlock(FromM32(fork->extents[extInd].startBlock)))
            {
                Error(IDS_ERR_HFS_SEEK_BLOCK_EXTR, FALSE /*quiet*/, FromM32(fork->extents[extInd].startBlock), fileData->Name);
                ret = UNPACK_CANCEL;
                bFileComplete = FALSE;
                break;
            }
            while ((size > 0) && (extSize > 0))
            {
                DWORD nbytes = (DWORD)min(BUFFER_SIZE, extSize);
                DWORD dwBytesRead;

                if (nbytes > size)
                    nbytes = (DWORD)size;

                if (!Read(buffer, nbytes, &dwBytesRead))
                {
                    ret = UNPACK_CANCEL;
                    bFileComplete = FALSE;
                    break;
                }
                if (nbytes != dwBytesRead)
                {
                    ret = UNPACK_CANCEL;
                    bFileComplete = FALSE;
                    break;
                }
                // delayedPaint==TRUE, to avoid slow-downs
                if (!salamander->ProgressAddSize(nbytes, TRUE))
                {
                    salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING_OPERATION), FALSE);
                    salamander->ProgressEnableCancel(FALSE);

                    ret = UNPACK_CANCEL;
                    bFileComplete = FALSE;
                    break;
                }
                if (!file.Write(buffer, nbytes, &dwBytesRead, name, NULL))
                {
                    // Error message was already displayed by SafeWriteFile()
                    ret = UNPACK_CANCEL;
                    bFileComplete = FALSE;
                    break;
                }
                extSize -= nbytes;
                size -= nbytes;
            }
        }
        free(buffer);

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
        {
            SetFileAttrs(name, attrs);
        }
    }
    return ret;
}

CHFS::BTree::BTree(CHFS* hfs, CHFS::HFSPlusForkData* fork, BOOL quiet)
{
    CALL_STACK_MESSAGE1("BTree::BTree(,,)");
    size_t treeSize = (size_t)FromM64(fork->logicalSize);

    pRecords = NULL;
    nRecords = 0;
    bOK = false;
    pBTreeData = NULL;

    char* data = pBTreeData = (char*)malloc(treeSize);
    if (!pBTreeData)
    {
        Error(IDS_INSUFFICIENT_MEMORY, quiet);
        return;
    }

    int blocks = FromM32(fork->totalBlocks);
    int blockSize = hfs->VolHeader.blockSize;

    for (int i = 0; i < 8; i++)
    {
        DWORD dwBytesRead, extSize;
        int cnt = FromM32(fork->extents[i].blockCount);

        if (cnt > blocks)
            cnt = blocks; // safety
        if (!hfs->SeekBlock(FromM32(fork->extents[i].startBlock)))
        {
            Error(IDS_ERR_HFS_SEEK, quiet, FromM32(fork->extents[i].startBlock));
            return;
        }
        extSize = cnt * blockSize;
        if (treeSize < extSize)
            extSize = (DWORD)treeSize;
        if (!hfs->Read(data, extSize, &dwBytesRead))
            return;
        if (extSize != dwBytesRead)
        {
            Error(IDS_READERROR, quiet);
            return;
        }
        data += extSize;
        treeSize -= extSize;
        blocks -= cnt;
        if (!blocks)
            break;
    }
    if (blocks || treeSize)
    {
        Error(IDS_ERR_HFS_BTREE_FRAGMENTED, quiet);
        return;
    }

    BTNodeDescriptor* node = (BTNodeDescriptor*)pBTreeData;

    // We require full tree with header node.
    if (node->kind != kBTHeaderNode)
    {
        Error(IDS_ERR_HFS_BTREE_NODETYPE, quiet);
        return;
    }

    // cannot use this because nodeSize is determined by BTHeaderRec.nodeSize
    //  UInt16 ofs = *(UInt16*)(pBTreeData+blockSize-2);
    BTHeaderRec* header = (BTHeaderRec*)(pBTreeData + sizeof(BTNodeDescriptor));
    int nodeSize = FromM16(header->nodeSize);
    bool keyLenghIsByte = (header->attributes & FromM32(kBTBigKeysMask)) == 0;
    bool keyVariableLen = (header->attributes & FromM32(kBTVariableIndexKeysMask)) != 0;

    node = (BTNodeDescriptor*)(pBTreeData + FromM32(header->firstLeafNode) * nodeSize);

    nRecords = FromM32(header->leafRecords);
    pRecords = (void**)malloc(sizeof(void*) * nRecords);
    if (!pRecords)
    {
        nRecords = 0;
        Error(IDS_INSUFFICIENT_MEMORY, quiet);
        return;
    }
    int nRecordsCheck = 0;
    void** pRecord = pRecords; // Each pointer links to the i-th record

    for (;;)
    {
        int nRecordsInNode = FromM16(node->numRecords);

        nRecordsCheck += nRecordsInNode;
        if (nRecordsCheck > nRecords)
        {
            // In theory, should not be needed.
            // But buggy MagicISO 5.2 stores # of nodes and not records in numRecords :-(
            int currInd = (int)(pRecord - pRecords);
            void** tmp = (void**)realloc(pRecords, sizeof(void*) * nRecordsCheck);
            if (!tmp)
            {
                Error(IDS_INSUFFICIENT_MEMORY, quiet);
                return;
            }
            nRecords = nRecordsCheck;
            pRecords = tmp;
            pRecord = pRecords + currInd;
        }

        for (int i = 0; i < nRecordsInNode; i++)
        {
            UInt16 ofs = *(UInt16*)(((char*)node) + nodeSize - (i + 1) * sizeof(UInt16));
            *pRecord++ = (char*)node + FromM16(ofs);
        }
        // Traverse till the forward link is empty
        if (!node->fLink)
            break;
        node = (BTNodeDescriptor*)(pBTreeData + FromM32(node->fLink) * nodeSize);
    }
    if (nRecords > nRecordsCheck)
    {
        // Should not be needed. Buggy MagicISO? Save some memory...
        nRecords = nRecordsCheck;
        void* tmp = realloc(pRecords, sizeof(void*) * nRecords);
        if (tmp)
        {
            // realloc succeeded
            pRecords = (void**)tmp;
        }
        else if (!nRecords)
        {
            // Either realloc failed or nRecord is 0 and thus pRecords was freed
            pRecords = NULL;
        }
    }

    bOK = true;
}

CHFS::BTree::~BTree()
{
    if (pBTreeData)
        free(pBTreeData);
    if (pRecords)
        free(pRecords);
}

void* CHFS::BTree::GetRecord(int ind)
{
    if (ind < nRecords)
        return pRecords[ind];
    return NULL;
}