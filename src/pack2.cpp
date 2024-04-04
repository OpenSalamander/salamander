// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "zip.h"
#include "plugins.h"
#include "pack.h"

//
// ****************************************************************************
// Constants and global variables
// ****************************************************************************
//

// Table of archive definitions and operations on them - modifying operations
// !!! WARNING: when changing the order of external archivers, the order in the array must also be changed
// externalArchivers in the method CPlugins::FindViewEdit
const SPackModifyTable PackModifyTable[] =
    {
        // JAR 1.02 Win32
        {
            (TPackErrorTable*)&JARErrors, TRUE,
            "$(SourcePath)", "$(Jar32bitExecutable) a -hl \"$(ArchiveFullName)\" -o\"$(TargetPath)\" !\"$(ListFullName)\"", TRUE,
            "$(ArchivePath)", "$(Jar32bitExecutable) d -r- \"$(ArchiveFileName)\" !\"$(ListFullName)\"", PMT_EMPDIRS_DELETE,
            "$(SourcePath)", "$(Jar32bitExecutable) m -hl \"$(ArchiveFullName)\" -o\"$(TargetPath)\" !\"$(ListFullName)\"", FALSE},
        // RAR 4.20 & 5.0 Win x86/x64
        {
            (TPackErrorTable*)&RARErrors, TRUE,
            "$(SourcePath)", "$(Rar32bitExecutable) a -scol \"$(ArchiveFullName)\" -ap\"$(TargetPath)\" @\"$(ListFullName)\"", TRUE, // from version 5.0 we need to enforce the -scol switch, version 4.20 doesn't mind; it occurs in other places and in the registry
            "$(ArchivePath)", "$(Rar32bitExecutable) d -scol \"$(ArchiveFileName)\" @\"$(ListFullName)\"", PMT_EMPDIRS_DELETE,
            "$(SourcePath)", "$(Rar32bitExecutable) m -scol \"$(ArchiveFullName)\" -ap\"$(TargetPath)\" @\"$(ListFullName)\"", FALSE},
        // ARJ 2.60 MS-DOS
        {
            (TPackErrorTable*)&ARJErrors, FALSE,
            "$(SourcePath)", "$(Arj16bitExecutable) a -p -va -hl -a $(ArchiveDOSFullName) !$(ListDOSFullName)", FALSE,
            ".", "$(Arj16bitExecutable) d -p -va -hl $(ArchiveDOSFullName) !$(ListDOSFullName)", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Arj16bitExecutable) m -p -va -hl -a $(ArchiveDOSFullName) !$(ListDOSFullName)", FALSE},
        // LHA 2.55 MS-DOS
        {
            (TPackErrorTable*)&LHAErrors, FALSE,
            "$(SourcePath)", "$(Lha16bitExecutable) a -m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE,
            ".", "$(Lha16bitExecutable) d -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)", PMT_EMPDIRS_DELETEWITHASTERISK,
            "$(SourcePath)", "$(Lha16bitExecutable) m -m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE},
        // UC2 2r3 FOR MS-DOS
        {
            (TPackErrorTable*)&UC2Errors, FALSE,
            "$(SourcePath)", "$(UC216bitExecutable) A !SYSHID=ON $(ArchiveDOSFullName) ##$(TargetPath) @$(ListDOSFullName)", TRUE,
            ".", "$(UC216bitExecutable) D $(ArchiveDOSFullName) @$(ListDOSFullName) & $$RED $(ArchiveDOSFullName)", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(UC216bitExecutable) AM !SYSHID=ON $(ArchiveDOSFullName) ##$(TargetPath) @$(ListDOSFullName)", FALSE},
        // JAR 1.02 MS-DOS
        {
            (TPackErrorTable*)&JARErrors, FALSE,
            "$(SourcePath)", "$(Jar16bitExecutable) a -hl $(ArchiveDOSFullName) -o\"$(TargetPath)\" !$(ListDOSFullName)", TRUE,
            "$(ArchivePath)", "$(Jar16bitExecutable) d -r- $(ArchiveDOSFileName) !$(ListDOSFullName)", PMT_EMPDIRS_DELETE,
            "$(SourcePath)", "$(Jar16bitExecutable) m -hl $(ArchiveDOSFullName) -o\"$(TargetPath)\" !$(ListDOSFullName)", FALSE},
        // RAR 2.50 MS-DOS
        {
            (TPackErrorTable*)&RARErrors, FALSE,
            "$(SourcePath)", "$(Rar16bitExecutable) a $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE, //P.S. ability to package into a subdirectory removed
            "$(ArchivePath)", "$(Rar16bitExecutable) d $(ArchiveDOSFileName) @$(ListDOSFullName)", PMT_EMPDIRS_DELETE,
            "$(SourcePath)", "$(Rar16bitExecutable) m $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE //P.S. ability to package into a subdirectory removed
        },
        // PKZIP 2.50 Win32
        {
            NULL, TRUE,
            "$(SourcePath)", "$(Zip32bitExecutable) -add -nozipextension -attr -path \"$(ArchiveFullName)\" @\"$(ListFullName)\"", FALSE,
            "$(ArchivePath)", "$(Zip32bitExecutable) -del -nozipextension \"$(ArchiveFileName)\" @\"$(ListFullName)\"", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Zip32bitExecutable) -add -nozipextension -attr -path -move \"$(ArchiveFullName)\" @\"$(ListFullName)\"", TRUE},
        // PKZIP 2.04g MS-DOS
        {
            (TPackErrorTable*)&ZIP204Errors, FALSE,
            "$(SourcePath)", "$(Zip16bitExecutable) -a -P -whs $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE,
            ".", "$(Zip16bitExecutable) -d $(ArchiveDOSFullName) @$(ListDOSFullName)", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Zip16bitExecutable) -m -P -whs $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE},
        // ARJ 3.00c Win32
        {
            (TPackErrorTable*)&ARJErrors, TRUE,
            "$(SourcePath)", "$(Arj32bitExecutable) a -p -va -hl -a \"$(ArchiveFullName)\" !\"$(ListFullName)\"", FALSE,
            "$(ArchivePath)", "$(Arj32bitExecutable) d -p -va -hl \"$(ArchiveFileName)\" !\"$(ListFullName)\"", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Arj32bitExecutable) m -p -va -hl -a \"$(ArchiveFullName)\" !\"$(ListFullName)\"", FALSE},
        // ACE 1.2b Win32
        {
            (TPackErrorTable*)&ACEErrors, TRUE,
            "$(SourcePath)", "$(Ace32bitExecutable) a -o -f \"$(ArchiveFullName)\" @\"$(ListFullName)\"", FALSE,
            "$(ArchivePath)", "$(Ace32bitExecutable) d -f \"$(ArchiveFileName)\" @\"$(ListFullName)\"", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Ace32bitExecutable) m -o -f \"$(ArchiveFullName)\" @\"$(ListFullName)\"", TRUE},
        // ACE 1.2b MS-DOS
        {
            (TPackErrorTable*)&ACEErrors, FALSE,
            "$(SourcePath)", "$(Ace16bitExecutable) a -o -f $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE,
            ".", "$(Ace16bitExecutable) d -f $(ArchiveDOSFullName) @$(ListDOSFullName)", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Ace16bitExecutable) m -o -f $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE}};

//
// ****************************************************************************
// Function
// ****************************************************************************
//

//
// ****************************************************************************
// Function for compression
//

//
// ****************************************************************************
// BOOL PackCompress(HWND parent, CFilesWindow *panel, const char *archiveFileName,
//                   const char *archiveRoot, BOOL move, const char *sourceDir,
//                   SalEnumSelection2 nextName, void *param)
//
//   Function for adding the desired files to the archive.
//
//   RET: returns TRUE on success, FALSE on error
//        calls the callback function *PackErrorHandlerPtr in case of an error
//   IN:  parent is the parent of the message box
//        panel is a pointer to the file panel of the salamander
//        archiveFileName is the name of the archive we are packing into
//        archiveRoot is the directory in the archive where we are packaging
//        move is TRUE if we are moving files to the archive
//        sourceDir is the path from which files are being packaged
//        nextName is a callback function for enumerating package names
//        param are parameters for the enumeration function
//   OUT:

BOOL PackCompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                  const char* archiveRoot, BOOL move, const char* sourceDir,
                  SalEnumSelection2 nextName, void* param)
{
    CALL_STACK_MESSAGE5("PackCompress(, , %s, %s, %d, %s, ,)", archiveFileName,
                        archiveRoot, move, sourceDir);
    // find the right one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // We did not find a supported archive - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    if (!PackerFormatConfig.GetUsePacker(format))
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PACKER_UNSUP);
    int index = PackerFormatConfig.GetPackerIndex(format);

    // Isn't it about internal processing (DLL)?
    if (index < 0)
    {
        CPluginData* plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelEdit)
        {
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->PackToArchive(panel, archiveFileName, archiveRoot, move,
                                     sourceDir, nextName, param);
    }

    const SPackModifyTable* modifyTable = ArchiverConfig.GetPackerConfigTable(index);

    // Let's determine whether we are performing a copy or a move
    const char* compressCommand;
    const char* compressInitDir;
    if (!move)
    {
        compressCommand = modifyTable->CompressCommand;
        compressInitDir = modifyTable->CompressInitDir;
    }
    else
    {
        compressCommand = modifyTable->MoveCommand;
        compressInitDir = modifyTable->MoveInitDir;
        if (compressCommand == NULL)
        {
            BOOL ret = (*PackErrorHandlerPtr)(parent, IDS_PACKQRY_NOMOVE);
            if (ret)
            {
                compressCommand = modifyTable->CompressCommand;
                compressInitDir = modifyTable->CompressInitDir;
            }
            else
                return FALSE;
        }
    }

    //
    // If the archiver does not support packing into a directory, we need to solve this
    //
    char archiveRootPath[MAX_PATH];
    if (archiveRoot != NULL && *archiveRoot != '\0')
    {
        strcpy(archiveRootPath, archiveRoot);
        if (!modifyTable->CanPackToDir) // archiving program does not support it
        {
            if ((*PackErrorHandlerPtr)(parent, IDS_PACKQRY_ARCPATH))
                strcpy(archiveRootPath, "\\"); // user wants to ignore it
            else
                return FALSE; // users mind it
        }
    }
    else
        strcpy(archiveRootPath, "\\");

    // and we will perform our own packaging
    return PackUniversalCompress(parent, compressCommand, modifyTable->ErrorTable,
                                 compressInitDir, TRUE, modifyTable->SupportLongNames, archiveFileName,
                                 sourceDir, archiveRootPath, nextName, param, modifyTable->NeedANSIListFile);
}

//
// ****************************************************************************
// BOOL PackUniversalCompress(HWND parent, const char *command, TPackErrorTable *const errorTable,
//                            const char *initDir, BOOL expandInitDir, const BOOL supportLongNames,
//                            const char *archiveFileName, const char *sourceDir,
//                            const char *archiveRoot, SalEnumSelection2 nextName,
//                            void *param, BOOL needANSIListFile)
//
//   Function for adding the desired files to the archive. Unlike the previous one
//   is more general, does not use a configuration table - can be called independently,
//   all is determined only by parameters
//
//   RET: returns TRUE on success, FALSE on error
//        calls the callback function *PackErrorHandlerPtr in case of an error
//   IN:  parent is the parent for message boxes
//        command is the command line used for archiving
//        errorTable pointer to the table of return codes of the archiver, or NULL if it does not exist
//        initDir is the directory in which the program will be executed
//        supportLongNames indicates whether the program supports the use of long names
//        archiveFileName is the name of the archive we are packing into
//        sourceDir is the path from which files are being packaged
//        archiveRoot is the directory in the archive where we are packaging
//        nextName is a callback function for enumerating package names
//        param are parameters for the enumeration function
//        needANSIListFile is TRUE if the file list should be in ANSI (not OEM)
//   OUT:

BOOL PackUniversalCompress(HWND parent, const char* command, TPackErrorTable* const errorTable,
                           const char* initDir, BOOL expandInitDir, const BOOL supportLongNames,
                           const char* archiveFileName, const char* sourceDir,
                           const char* archiveRoot, SalEnumSelection2 nextName,
                           void* param, BOOL needANSIListFile)
{
    CALL_STACK_MESSAGE9("PackUniversalCompress(, %s, , %s, %d, %d, %s, %s, %s, , , %d)",
                        command, initDir, expandInitDir, supportLongNames, archiveFileName,
                        sourceDir, archiveRoot, needANSIListFile);

    //
    // We need to adjust the directory in the archive to the desired format
    //
    char rootPath[MAX_PATH];
    rootPath[0] = '\0';
    if (archiveRoot != NULL && *archiveRoot != '\0')
    {
        while (*archiveRoot == '\\')
            archiveRoot++;
        if (*archiveRoot != '\0')
        {
            strcpy(rootPath, "\\");
            strcat(rootPath, archiveRoot);
            while (rootPath[0] != '\0' && rootPath[strlen(rootPath) - 1] == '\\')
                rootPath[strlen(rootPath) - 1] = '\0';
        }
    }
    // For the 32-bit program, empty quotes will be used, for the 16-bit program we will use a slash
    if (!supportLongNames && rootPath[0] == '\0')
    {
        rootPath[0] = '\\';
        rootPath[1] = '\0';
    }

    // For checking the length of the path, we will need the sourceDir in a "short" form
    char sourceShortName[MAX_PATH];
    if (!supportLongNames)
    {
        if (!GetShortPathName(sourceDir, sourceShortName, MAX_PATH))
        {
            char buffer[1000];
            strcpy(buffer, "GetShortPathName: ");
            strcat(buffer, GetErrorText(GetLastError()));
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
        }
    }
    else
        strcpy(sourceShortName, sourceDir);

    //
    // There will be a helper file with a list of files to be packed in the %TEMP% directory
    //

    // Create a name for the temporary file
    char tmpListNameBuf[MAX_PATH];
    if (!SalGetTempFileName(NULL, "PACK", tmpListNameBuf, TRUE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    // we have a file, now we will open it
    FILE* listFile;
    if ((listFile = fopen(tmpListNameBuf, "w")) == NULL)
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
    }

    // and we can fulfill it
    BOOL isDir;

    const char* name;
    unsigned int maxPath;
    char namecnv[MAX_PATH];
    if (!supportLongNames)
        maxPath = DOS_MAX_PATH;
    else
        maxPath = MAX_PATH;
    if (!needANSIListFile)
        CharToOem(sourceShortName, sourceShortName);
    int sourceDirLen = (int)strlen(sourceShortName) + 1;
    int errorOccured;
    // choose a name
    while ((name = nextName(parent, 1, NULL, &isDir, NULL, NULL, NULL, param, &errorOccured)) != NULL)
    {
        if (supportLongNames)
        {
            if (!needANSIListFile)
                CharToOem(name, namecnv);
            else
                strcpy(namecnv, name);
        }
        else
        {
            if (GetShortPathName(name, namecnv, MAX_PATH) == 0)
            {
                char buffer[1000];
                strcpy(buffer, "File: ");
                strcat(buffer, name);
                strcat(buffer, ", GetShortPathName: ");
                strcat(buffer, GetErrorText(GetLastError()));
                fclose(listFile);
                DeleteFile(tmpListNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
            }
            if (!needANSIListFile)
                CharToOem(namecnv, namecnv);
        }

        // handle length
        if (sourceDirLen + strlen(namecnv) >= maxPath)
        {
            char buffer[1000];
            fclose(listFile);
            DeleteFile(tmpListNameBuf);
            sprintf(buffer, "%s\\%s", sourceShortName, namecnv);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PATH, buffer);
        }

        // and push it into the list
        if (!isDir)
        {
            if (fprintf(listFile, "%s\n", namecnv) <= 0)
            {
                fclose(listFile);
                DeleteFile(tmpListNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
            }
        }
    }
    // and that's it
    fclose(listFile);

    // If an error occurred and the user decided to cancel the operation, let's terminate it
    if (errorOccured == SALENUM_CANCEL)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE;
    }

    //
    // Now we will run an external program to unpack
    //
    // we will construct the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    // buffer for the replacement name (if we are creating an archive with a long name and need its DOS name),
    // the space for the long name DOSTmpName is being expanded, after creating the archive we will rename the file)
    char DOSTmpName[MAX_PATH];
    if (!PackExpandCmdLine(archiveFileName, rootPath, tmpListNameBuf, NULL,
                           command, cmdLine, PACK_CMDLINE_MAXLEN, DOSTmpName))
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNERR);
    }

    // hack for RAR 4.x+ that dislikes -ap"" for addressing the root of the archive; at the same time, this deletion is compatible with the old version of RAR
    // see https://forum.altap.cz/viewtopic.php?f=2&t=5487
    if (*rootPath == 0 && strstr(command, "$(Rar32bitExecutable) ") == command)
    {
        char* pAP = strstr(cmdLine, "\" -ap\"\" @\"");
        if (pAP != NULL)
            memmove(pAP + 1, "         ", 7); // shoot down -ap"", which annoys with the new RAR
    }
    // hack for copying to a directory in RAR - it bothers him when the path starts with a backslash; he created for example a directory \Test which Salam shows as Test
    // https://forum.altap.cz/viewtopic.php?p=24586#p24586
    if (*rootPath == '\\' && strstr(command, "$(Rar32bitExecutable) ") == command)
    {
        char* pAP = strstr(cmdLine, "\" -ap\"\\");
        if (pAP != NULL)
            memmove(pAP + 6, pAP + 7, strlen(pAP + 7) + 1); // remove the backslash from the beginning
    }

    // check if the command line is not too long
    if (!supportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        strcpy(buffer, cmdLine);
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // construct the current directory
    char currentDir[MAX_PATH];
    if (!expandInitDir)
    {
        if (strlen(initDir) < MAX_PATH)
            strcpy(currentDir, initDir);
        else
        {
            DeleteFile(tmpListNameBuf);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
        }
    }
    else
    {
        if (!PackExpandInitDir(archiveFileName, sourceDir, rootPath, initDir, currentDir,
                               MAX_PATH))
        {
            DeleteFile(tmpListNameBuf);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
        }
    }

    // We back up a short name of the archive file, later we will test a long name
    // did not disappear -> if it remained short, we will rename short to original long
    char DOSArchiveFileName[MAX_PATH];
    if (!GetShortPathName(archiveFileName, DOSArchiveFileName, MAX_PATH))
        DOSArchiveFileName[0] = 0;

    // and we will launch an external program
    BOOL exec = PackExecute(parent, cmdLine, currentDir, errorTable);
    // Meanwhile, we test if the long name has not disappeared -> if it remained short, we will rename it to short
    // to the original long
    if (DOSArchiveFileName[0] != 0 &&
        SalGetFileAttributes(archiveFileName) == 0xFFFFFFFF &&
        SalGetFileAttributes(DOSArchiveFileName) != 0xFFFFFFFF)
    {
        SalMoveFile(DOSArchiveFileName, archiveFileName); // if it fails, let's forget about it...
    }
    if (!exec)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE; // Error message has already been thrown
    }

    // list of files is no longer needed
    DeleteFile(tmpListNameBuf);

    // if we used an alternative DOS name, we rename all files with this name (name.*) to the desired long name
    if (DOSTmpName[0] != 0)
    {
        char src[2 * MAX_PATH];
        strcpy(src, DOSTmpName);
        char* tmpOrigName;
        CutDirectory(src, &tmpOrigName);
        tmpOrigName = DOSTmpName + (tmpOrigName - src);
        SalPathAddBackslash(src, 2 * MAX_PATH);
        char* srcName = src + strlen(src);
        char dstNameBuf[2 * MAX_PATH];
        strcpy(dstNameBuf, archiveFileName);
        char* dstExt = dstNameBuf + strlen(dstNameBuf);
        //    while (--dstExt > dstNameBuf && *dstExt != '\\' && *dstExt != '.');
        while (--dstExt >= dstNameBuf && *dstExt != '\\' && *dstExt != '.')
            ;
        //    if (dstExt == dstNameBuf || *dstExt == '\\' || *(dstExt - 1) == '\\') dstExt = dstNameBuf + strlen(dstNameBuf); // u "name", ".cvspass", "path\name" ani "path\.name" neni zadna pripona
        if (dstExt < dstNameBuf || *dstExt == '\\')
            dstExt = dstNameBuf + strlen(dstNameBuf); // there is no extension for "name" or "path\name"; ".cvspass" in Windows is an extension
        char path[MAX_PATH];
        strcpy(path, DOSTmpName);
        char* ext = path + strlen(path);
        //    while (--ext > path && *ext != '\\' && *ext != '.');
        while (--ext >= path && *ext != '\\' && *ext != '.')
            ;
        //    if (ext == path || *ext == '\\' || *(ext - 1) == '\\') ext = path + strlen(path); // u "name", ".cvspass", "path\name" ani ""path\.name" neni zadna pripona
        if (ext < path || *ext == '\\')
            ext = path + strlen(path); // there is no extension for "name" or "path\name"; ".cvspass" in Windows is an extension
        strcpy(ext, ".*");
        WIN32_FIND_DATA findData;
        int i;
        for (i = 0; i < 2; i++)
        {
            HANDLE find = HANDLES_Q(FindFirstFile(path, &findData));
            if (find != INVALID_HANDLE_VALUE)
            {
                do
                {
                    strcpy(srcName, findData.cFileName);
                    const char* dst;
                    if (StrICmp(tmpOrigName, findData.cFileName) == 0)
                        dst = archiveFileName;
                    else
                    {
                        char* srcExt = findData.cFileName + strlen(findData.cFileName);
                        //            while (--srcExt > findData.cFileName && *srcExt != '.');
                        while (--srcExt >= findData.cFileName && *srcExt != '.')
                            ;
                        //            if (srcExt == findData.cFileName) srcExt = findData.cFileName + strlen(findData.cFileName);  // ".cvspass" ve Windows je pripona ...
                        if (srcExt < findData.cFileName)
                            srcExt = findData.cFileName + strlen(findData.cFileName);
                        strcpy(dstExt, srcExt);
                        dst = dstNameBuf;
                    }
                    if (i == 0)
                    {
                        if (SalGetFileAttributes(dst) != 0xffffffff)
                        {
                            HANDLES(FindClose(find)); // this name already exists with some suffix, we continue searching
                            (*PackErrorHandlerPtr)(parent, IDS_PACKERR_UNABLETOREN, src, dst);
                            return TRUE; // It worked, only the names of the resulting archive (including multivolume) are a bit different
                        }
                    }
                    else
                    {
                        if (!SalMoveFile(src, dst))
                        {
                            DWORD err = GetLastError();
                            TRACE_E("Error (" << err << ") in SalMoveFile(" << src << ", " << dst << ").");
                        }
                    }
                } while (FindNextFile(find, &findData));
                HANDLES(FindClose(find)); // this name already exists with some suffix, we continue searching
            }
        }
    }

    return TRUE;
}

//
// ****************************************************************************
// Function for deleting from the archive
//

//
// ****************************************************************************
// BOOL PackDelFromArc(HWND parent, CFilesWindow *panel, const char *archiveFileName,
//                     CPluginDataInterfaceAbstract *pluginData,
//                     const char *archiveRoot, SalEnumSelection nextName,
//                     void *param)
//
//   Function for deleting the desired files from the archive.
//
//   RET: returns TRUE on success, FALSE on error
//        calls the callback function *PackErrorHandlerPtr in case of an error
//   IN:  parent is the parent for message boxes
//        panel is a pointer to the file panel of the salamander
//        archiveFileName is the name of the archive from which we are deleting
//        archiveRoot is the directory in the archive from which we are deleting
//        nextName is a callback function for enumerating names to delete
//        param are parameters for the enumeration function
//   OUT:

BOOL PackDelFromArc(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* archiveRoot, SalEnumSelection nextName,
                    void* param)
{
    CALL_STACK_MESSAGE3("PackDelFromArc(, , %s, , %s, , ,)", archiveFileName, archiveRoot);

    // find the right one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // We did not find a supported archive - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    if (!PackerFormatConfig.GetUsePacker(format))
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PACKER_UNSUP);
    int index = PackerFormatConfig.GetPackerIndex(format);

    // Isn't it about internal processing (DLL)?
    if (index < 0)
    {
        CPluginData* plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelEdit)
        {
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->DeleteFromArchive(panel, archiveFileName, pluginData, archiveRoot,
                                         nextName, param);
    }

    const SPackModifyTable* modifyTable = ArchiverConfig.GetPackerConfigTable(index);
    BOOL needANSIListFile = modifyTable->NeedANSIListFile;

    //
    // We need to adjust the directory in the archive to the desired format
    //
    char rootPath[MAX_PATH];
    if (archiveRoot != NULL && *archiveRoot != '\0')
    {
        if (*archiveRoot == '\\')
            archiveRoot++;
        if (*archiveRoot == '\0')
            rootPath[0] = '\0';
        else
        {
            strcpy(rootPath, archiveRoot);
            if (rootPath[strlen(rootPath) - 1] != '\\')
                strcat(rootPath, "\\");
        }
    }
    else
    {
        rootPath[0] = '\0';
    }

    //
    // There will be a temporary file in the %TEMP% directory with a list of files to be deleted
    //
    // buffer for the fullname of the auxiliary file
    char tmpListNameBuf[MAX_PATH];
    if (!SalGetTempFileName(NULL, "PACK", tmpListNameBuf, TRUE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    // we have a file, now we will open it
    FILE* listFile;
    if ((listFile = fopen(tmpListNameBuf, "w")) == NULL)
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
    }

    // and we can fulfill it
    BOOL isDir;
    const char* name;
    char namecnv[MAX_PATH];
    int errorOccured;
    if (!needANSIListFile)
        CharToOem(rootPath, rootPath);
    // choose a name
    while ((name = nextName(parent, 1, &isDir, NULL, NULL, param, &errorOccured)) != NULL)
    {
        if (!needANSIListFile)
            CharToOem(name, namecnv);
        else
            strcpy(namecnv, name);
        // and push it into the list
        if (!isDir)
        {
            if (fprintf(listFile, "%s%s\n", rootPath, namecnv) <= 0)
            {
                fclose(listFile);
                DeleteFile(tmpListNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
            }
        }
        else
        {
            if (modifyTable->DelEmptyDir == PMT_EMPDIRS_DELETE)
            {
                if (fprintf(listFile, "%s%s\n", rootPath, namecnv) <= 0)
                {
                    fclose(listFile);
                    DeleteFile(tmpListNameBuf);
                    return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
                }
            }
            else
            {
                if (modifyTable->DelEmptyDir == PMT_EMPDIRS_DELETEWITHASTERISK)
                {
                    if (fprintf(listFile, "%s%s\\*\n", rootPath, namecnv) <= 0)
                    {
                        fclose(listFile);
                        DeleteFile(tmpListNameBuf);
                        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
                    }
                }
            }
        }
    }
    // and that's it
    fclose(listFile);

    // If an error occurred and the user decided to cancel the operation, let's terminate it
    if (errorOccured == SALENUM_CANCEL)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE;
    }

    //
    // Now we will run an external program for deletion
    //
    // we will construct the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    if (!PackExpandCmdLine(archiveFileName, NULL, tmpListNameBuf, NULL,
                           modifyTable->DeleteCommand, cmdLine, PACK_CMDLINE_MAXLEN, NULL))
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNERR);
    }

    // check if the command line is not too long
    if (!modifyTable->SupportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        DeleteFile(tmpListNameBuf);
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // construct the current directory
    char currentDir[MAX_PATH];
    if (!PackExpandInitDir(archiveFileName, NULL, NULL, modifyTable->DeleteInitDir,
                           currentDir, MAX_PATH))
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
    }

    // let's take attributes for later use
    DWORD fileAttrs = SalGetFileAttributes(archiveFileName);
    if (fileAttrs == 0xFFFFFFFF)
        fileAttrs = FILE_ATTRIBUTE_ARCHIVE;

    // We back up a short name of the archive file, later we will test a long name
    // did not disappear -> if it remained short, we will rename short to original long
    char DOSArchiveFileName[MAX_PATH];
    if (!GetShortPathName(archiveFileName, DOSArchiveFileName, MAX_PATH))
        DOSArchiveFileName[0] = 0;

    // and we will launch an external program
    BOOL exec = PackExecute(NULL, cmdLine, currentDir, modifyTable->ErrorTable);
    // Meanwhile, we test if the long name has not disappeared -> if it remained short, we will rename it to short
    // to the original long
    if (DOSArchiveFileName[0] != 0 &&
        SalGetFileAttributes(archiveFileName) == 0xFFFFFFFF &&
        SalGetFileAttributes(DOSArchiveFileName) != 0xFFFFFFFF)
    {
        SalMoveFile(DOSArchiveFileName, archiveFileName); // if it fails, let's forget about it...
    }
    if (!exec)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE; // Error message has already been thrown
    }

    // if deleting the archive failed, we will create a file with zero length
    HANDLE tmpHandle = HANDLES_Q(CreateFile(archiveFileName, GENERIC_READ, 0, NULL,
                                            OPEN_ALWAYS, fileAttrs, NULL));
    if (tmpHandle != INVALID_HANDLE_VALUE)
        HANDLES(CloseHandle(tmpHandle));

    // list of files is no longer needed
    DeleteFile(tmpListNameBuf);

    return TRUE;
}
