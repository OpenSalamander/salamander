// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
#include <tchar.h>

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
#include "list.h"

int CZipList::ListArchive(CSalamanderDirectoryAbstract* dir, BOOL& haveFiles)
{
    CALL_STACK_MESSAGE1("CZipList::ListArchive( )");
    int ret;

    haveFiles = FALSE;
    ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                      true, true); // 'useReadCache' dame TRUE -> optimalizovane cteni central directory
    if (ret)
        if (ret == ERR_LOWMEM)
            return ErrorID = IDS_LOWMEM;
        else
            return ErrorID = IDS_NODISPLAY;
    ErrorID = CheckZip();
    if (!ErrorID)
        return ErrorID = List(dir, haveFiles);
    return ErrorID;
}

int CZipList::List(CSalamanderDirectoryAbstract* dir, BOOL& haveFiles)
{
    CALL_STACK_MESSAGE1("CZipList::List()");
    CFileHeader* centralHeader;
    CFileInfo fileInfo;
    CFileData file;
    int errorID = 0;
    QWORD readOffset;
    //  char *              pathBuf;
    LPCTSTR path;
    LPTSTR name;

    if (ZeroZip)
        return 0;
    centralHeader = (CFileHeader*)malloc(MAX_HEADER_SIZE);
    fileInfo.Name = (LPTSTR)malloc(sizeof(TCHAR) * MAX_HEADER_SIZE);
    //  pathBuf = (char *) malloc( MAX_HEADER_SIZE);
    if (!centralHeader || !fileInfo.Name /* || !pathBuf*/)
    {
        if (centralHeader)
            free(centralHeader);
        //if (fileInfo.Name ) free(fileInfo.Name); freed in destructor
        //    if (pathBuf)
        //      free(pathBuf);
        return IDS_LOWMEM;
    }

START_LIST:
    // TRACE_I("zip listing started");

    haveFiles = FALSE;
    readOffset = CentrDirOffs + ExtraBytes;
    if (DiskNum != CentrDirStartDisk && MultiVol)
    {
        DiskNum = CentrDirStartDisk;
        errorID = ChangeDisk();
    }
    if (!errorID)
    {
        int sortByExtDirsAsFiles;
        int cnt = 0;

        SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                              sizeof(sortByExtDirsAsFiles), NULL);

        QWORD readSize;
        for (readSize = 0; readSize < CentrDirSize; cnt++)
        {
            unsigned int s;
            int err = ReadCentralHeader(centralHeader, &readOffset, &s);
            if (err)
            {
                errorID = err;
                break;
            }
            readSize += s;
            if (centralHeader->Version >> 8 == HS_UNIX && !Unix)
            {
                Unix = TRUE;
                dir->Clear(NULL);
                dir->SetFlags(SALDIRFLAG_CASESENSITIVE);
                goto START_LIST;
            }
            int fileInfoNameLen = ProcessName(centralHeader, fileInfo.Name);
            ProcessHeader(centralHeader, &fileInfo);
            //      path = pathBuf;
            //      SplitPath(&path, &name, fileInfo.Name);
            // j.r. optimalizace misto SplitPath
            path = fileInfo.Name;
            // _tcsrchr works correctly on MBCS file names
            name = _tcsrchr(fileInfo.Name, '\\');
            /*      name = fileInfo.Name + fileInfoNameLen;
      while (name > fileInfo.Name && *name != '\\')
        name--;*/
            if (name && (*name == '\\'))
            {
                *name = 0;
                name++;
            }
            else
            {
                name = fileInfo.Name;
                path = _T("");
            }
            int nameLen = (int)(fileInfo.Name + fileInfoNameLen - name);

            file.NameLen = nameLen;
            file.Name = (LPTSTR)SalamanderGeneral->Alloc(sizeof(TCHAR) * (file.NameLen + 1));
            if (!file.Name)
            {
                errorID = IDS_LOWMEM;
                break;
            }
            memcpy(file.Name, name, sizeof(TCHAR) * (file.NameLen + 1));
            //initialize remaining members of CFileData
            file.Size = CQuadWord().SetUI64(fileInfo.Size);
            file.Attr = fileInfo.FileAttr & FILE_ATTTRIBUTE_MASK;
            if (fileInfo.Flag & GPF_ENCRYPTED)
                file.Attr |= FILE_ATTRIBUTE_ENCRYPTED;
            file.LastWrite = fileInfo.LastWrite;
            file.DosName = NULL;
            file.Hidden = file.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
            file.PluginData = (DWORD_PTR) new CZIPFileData(fileInfo.CompSize, cnt, Unix);
            if (!file.PluginData)
            {
                SalamanderGeneral->Free(file.Name);
                errorID = IDS_LOWMEM;
                break;
            }

            if (!sortByExtDirsAsFiles && fileInfo.IsDir)
            {
                file.Ext = file.Name + file.NameLen; // adresare nemaji pripony
            }
            else
            {
                char* dot = file.Name + file.NameLen - 1;
                //search backward for last dot
                for (; dot >= file.Name && *dot != '.'; dot--)
                    ; // ".cvspass" is extension in Windows
                //dot found?
                if (dot >= file.Name)
                    file.Ext = dot + 1;
                else
                    file.Ext = file.Name + file.NameLen;
            }
            file.IsOffline = 0;
            if (fileInfo.IsDir)
            {
                file.IsLink = 0;
                if (!dir->AddDir(path, file, NULL))
                {
                    delete (CZIPFileData*)file.PluginData;
                    TRACE_E("Error adding directory " << path << "\\" << file.Name << " in the list");
                    SalamanderGeneral->Free(file.Name);
                    if (_tcslen(path) >= _MAX_PATH)
                    {
                        errorID = IDS_ERRADDDIR_TOOLONG;
                        // NOTE: no break! We continue parsing the archive
                    }
                    else
                    {
                        errorID = IDS_ERRADDDIR;
                        break;
                    }
                }
            }
            else
            {
                file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);
                if (!dir->AddFile(path, file, NULL))
                {
                    delete (CZIPFileData*)file.PluginData;
                    TRACE_E("Error adding file " << path << "\\" << file.Name << " to the list");
                    SalamanderGeneral->Free(file.Name);
                    if (_tcslen(path) >= _MAX_PATH)
                    {
                        errorID = IDS_ERRADDFILE_TOOLONG;
                        // NOTE: no break! We continue parsing the archive
                    }
                    else
                    {
                        errorID = IDS_ERRADDFILE;
                        break;
                    }
                }
            }

            /*
      TRACE_I("Listing file: " << fileInfo.Name <<
              ", method: " << fileInfo.Method <<
              ", flag: " << fileInfo.Flag <<
              ", isdir: " << fileInfo.IsDir <<
              ", file attr: " << fileInfo.FileAttr);
*/
        }
        haveFiles = cnt > 0;
    }
    free(centralHeader);
    //free(fileInfo.Name); mame destructor
    //  free(pathBuf);

    // TRACE_I("zip listing finished");

    return errorID;
}
