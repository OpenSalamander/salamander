// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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

// Table of archive definitions and how to handle them - modifying operations
// !!! WARNING: when changing the order of external archivers, the order in the
// externalArchivers array in the CPlugins::FindViewEdit method must be changed
// as well
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
            "$(SourcePath)", "$(Rar32bitExecutable) a -scol \"$(ArchiveFullName)\" -ap\"$(TargetPath)\" @\"$(ListFullName)\"", TRUE, // since version 5.0 we must enforce the -scol switch, version 4.20 is fine; it appears elsewhere and in the registry
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
        // UC2 2r3 PRO MS-DOS
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
            "$(SourcePath)", "$(Rar16bitExecutable) a $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE, // P.S. ability to pack into subdirectories removed
            "$(ArchivePath)", "$(Rar16bitExecutable) d $(ArchiveDOSFileName) @$(ListDOSFullName)", PMT_EMPDIRS_DELETE,
            "$(SourcePath)", "$(Rar16bitExecutable) m $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE // P.S. ability to pack into subdirectories removed
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
// Functions
// ****************************************************************************
//

//
// ****************************************************************************
// Functions for compression
//

//
// ****************************************************************************
// BOOL PackCompress(HWND parent, CFilesWindow *panel, const char *archiveFileName,
//                   const char *archiveRoot, BOOL move, const char *sourceDir,
//                   SalEnumSelection2 nextName, void *param)
//
//   Function for adding requested files to an archive.
//
//   RET: returns TRUE on success, FALSE on error
//        on error the callback function *PackErrorHandlerPtr is called
//   IN:  parent is the parent window of message boxes
//        panel is a pointer to the Salamander file panel
//        archiveFileName is the name of the archive to pack into
//        archiveRoot is the directory in the archive to pack into
//        move is TRUE if files are moved into the archive
//        sourceDir is the path from which the files are packed
//        nextName is a callback function that enumerates names to pack
//        param contains parameters for the enumeration function
//   OUT:

BOOL PackCompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                  const char* archiveRoot, BOOL move, const char* sourceDir,
                  SalEnumSelection2 nextName, void* param)
{
    CALL_STACK_MESSAGE5("PackCompress(, , %s, %s, %d, %s, ,)", archiveFileName,
                        archiveRoot, move, sourceDir);
    // find the correct one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // Did not find a supported archive - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    if (!PackerFormatConfig.GetUsePacker(format))
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PACKER_UNSUP);
    int index = PackerFormatConfig.GetPackerIndex(format);

    // Is this not internal processing (DLL)?
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

    // determine whether we perform copy or move
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
    // If the archiver does not support packing into a directory, we must handle it
    //
    char archiveRootPath[MAX_PATH];
    if (archiveRoot != NULL && *archiveRoot != '\0')
    {
        strcpy(archiveRootPath, archiveRoot);
        if (!modifyTable->CanPackToDir) // the archiver program does not support it
        {
            if ((*PackErrorHandlerPtr)(parent, IDS_PACKQRY_ARCPATH))
                strcpy(archiveRootPath, "\\"); // the user wants to ignore it
            else
                return FALSE; // the user will mind
        }
    }
    else
        strcpy(archiveRootPath, "\\");

    // and perform the actual packing
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
//   Function for adding requested files to an archive. Unlike the previous one
//   it is more general and does not use configuration tables - it can be called
//   independently, everything is determined only by parameters
//
//   RET: returns TRUE on success, FALSE on error
//        on error the callback function *PackErrorHandlerPtr is called
//   IN:  parent is the parent window for message boxes
//        command is the command line used for packing into the archive
//        errorTable is a pointer to the table of archiver return codes, or NULL if it does not exist
//        initDir is the directory in which the program will be started
//        supportLongNames indicates whether the program supports the use of long names
//        archiveFileName is the name of the archive to pack into
//        sourceDir is the path from which the files are packed
//        archiveRoot is the directory in the archive to pack into
//        nextName is a callback function that enumerates names to pack
//        param contains parameters for the enumeration function
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
    // We must adjust the directory in the archive to the required format
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
    // for 32-bit programs there will be empty quotes, for 16-bit we add a slash
    if (!supportLongNames && rootPath[0] == '\0')
    {
        rootPath[0] = '\\';
        rootPath[1] = '\0';
    }

    // For path length checks we need sourceDir in the "short" form
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
    // In the %TEMP% directory a helper file will contain the list of files to pack
    //

    // Create the temporary file name
    char tmpListNameBuf[MAX_PATH];
    if (!SalGetTempFileName(NULL, "PACK", tmpListNameBuf, TRUE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    // we have the file, now open it
    FILE* listFile;
    if ((listFile = fopen(tmpListNameBuf, "w")) == NULL)
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
    }

    // and we can fill it
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
    // pick the name
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

        // check the length
        if (sourceDirLen + strlen(namecnv) >= maxPath)
        {
            char buffer[1000];
            fclose(listFile);
            DeleteFile(tmpListNameBuf);
            sprintf(buffer, "%s\\%s", sourceShortName, namecnv);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PATH, buffer);
        }

        // and put it into the list
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
    // that's it
    fclose(listFile);

    // if an error occurred and the user decided to cancel the operation, end it
    if (errorOccured == SALENUM_CANCEL)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE;
    }

    //
    // Now we will launch the external program for compression
    //
    // construct the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    // buffer for a temporary name (when creating an archive with a long name and we need its DOS name,
    // DOSTmpName expands instead of the long name; after creating the archive the file is renamed)
    char DOSTmpName[MAX_PATH];
    if (!PackExpandCmdLine(archiveFileName, rootPath, tmpListNameBuf, NULL,
                           command, cmdLine, PACK_CMDLINE_MAXLEN, DOSTmpName))
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNERR);
    }

    // hack for RAR 4.x+ that dislikes "-ap""" when addressing the archive root; this cleanup works with older RAR too
    // see https://forum.altap.cz/viewtopic.php?f=2&t=5487
    if (*rootPath == 0 && strstr(command, "$(Rar32bitExecutable) ") == command)
    {
        char* pAP = strstr(cmdLine, "\" -ap\"\" @\"");
        if (pAP != NULL)
            memmove(pAP + 1, "         ", 7); // remove "-ap"" that causes issues with newer RAR
    }
    // hack for copying into a directory in RAR - it fails if the path begins with a backslash; it created e.g. \Test directory but Salam shows it as Test
    // https://forum.altap.cz/viewtopic.php?p=24586#p24586
    if (*rootPath == '\\' && strstr(command, "$(Rar32bitExecutable) ") == command)
    {
        char* pAP = strstr(cmdLine, "\" -ap\"\\");
        if (pAP != NULL)
            memmove(pAP + 6, pAP + 7, strlen(pAP + 7) + 1); // remove the leading backslash
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

    // back up the short archive file name, later we check whether the long name
    // survived -> if the short one remained, rename it back to the original long name
    char DOSArchiveFileName[MAX_PATH];
    if (!GetShortPathName(archiveFileName, DOSArchiveFileName, MAX_PATH))
        DOSArchiveFileName[0] = 0;

    // and run the external program
    BOOL exec = PackExecute(parent, cmdLine, currentDir, errorTable);
    // meanwhile check whether the long name did not vanish -> if the short one
    // remained, rename it to the original long one
    if (DOSArchiveFileName[0] != 0 &&
        SalGetFileAttributes(archiveFileName) == 0xFFFFFFFF &&
        SalGetFileAttributes(DOSArchiveFileName) != 0xFFFFFFFF)
    {
        SalMoveFile(DOSArchiveFileName, archiveFileName); // if it fails, we don't care...
    }
    if (!exec)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE; // error message has already been displayed
    }

    // the file list is no longer needed
    DeleteFile(tmpListNameBuf);

    // if we used a temporary DOS name, rename all files of that name (name.*) to the desired long name
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
        //    if (dstExt == dstNameBuf || *dstExt == '\\' || *(dstExt - 1) == '\\') dstExt = dstNameBuf + strlen(dstNameBuf); // for "name", ".cvspass", "path\\name" or "path\\.name" there is no extension
        if (dstExt < dstNameBuf || *dstExt == '\\')
            dstExt = dstNameBuf + strlen(dstNameBuf); // for "name" or "path\\name" there is no extension; in Windows ".cvspass" is an extension
        char path[MAX_PATH];
        strcpy(path, DOSTmpName);
        char* ext = path + strlen(path);
        //    while (--ext > path && *ext != '\\' && *ext != '.');
        while (--ext >= path && *ext != '\\' && *ext != '.')
            ;
        //    if (ext == path || *ext == '\\' || *(ext - 1) == '\\') ext = path + strlen(path); // for "name", ".cvspass", "path\\name" or "path\\.name" there is no extension
        if (ext < path || *ext == '\\')
            ext = path + strlen(path); // for "name" or "path\\name" there is no extension; in Windows ".cvspass" is an extension
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
                        //            if (srcExt == findData.cFileName) srcExt = findData.cFileName + strlen(findData.cFileName);  // ".cvspass" is an extension in Windows ...
                        if (srcExt < findData.cFileName)
                            srcExt = findData.cFileName + strlen(findData.cFileName);
                        strcpy(dstExt, srcExt);
                        dst = dstNameBuf;
                    }
                    if (i == 0)
                    {
                        if (SalGetFileAttributes(dst) != 0xffffffff)
                        {
                            HANDLES(FindClose(find)); // this name already exists with some extension, searching further
                            (*PackErrorHandlerPtr)(parent, IDS_PACKERR_UNABLETOREN, src, dst);
                            return TRUE; // succeeded, only the resulting archive names differ slightly (even multivolume)
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
                HANDLES(FindClose(find)); // this name already exists with some extension, searching further
            }
        }
    }

    return TRUE;
}

//
// ****************************************************************************
// Functions for deleting from an archive
//

//
// ****************************************************************************
// BOOL PackDelFromArc(HWND parent, CFilesWindow *panel, const char *archiveFileName,
//                     CPluginDataInterfaceAbstract *pluginData,
//                     const char *archiveRoot, SalEnumSelection nextName,
//                     void *param)
//
//   Function for removing the requested files from an archive.
//
//   RET: returns TRUE on success, FALSE on error
//        on error the callback function *PackErrorHandlerPtr is called
//   IN:  parent is the parent window for message boxes
//        panel is a pointer to the Salamander file panel
//        archiveFileName is the name of the archive we delete from
//        archiveRoot is the directory in the archive we delete from
//        nextName is a callback function that enumerates names to delete
//        param contains parameters for the enumeration function
//   OUT:

BOOL PackDelFromArc(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* archiveRoot, SalEnumSelection nextName,
                    void* param)
{
    CALL_STACK_MESSAGE3("PackDelFromArc(, , %s, , %s, , ,)", archiveFileName, archiveRoot);

    // find the correct one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // Did not find a supported archive - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    if (!PackerFormatConfig.GetUsePacker(format))
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PACKER_UNSUP);
    int index = PackerFormatConfig.GetPackerIndex(format);

    // Is this not internal processing (DLL)?
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
    // We must adjust the directory in the archive to the required format
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
    // in the %TEMP% directory a helper file will contain the list of files to delete
    //
    // buffer for the full name of the helper file
    char tmpListNameBuf[MAX_PATH];
    if (!SalGetTempFileName(NULL, "PACK", tmpListNameBuf, TRUE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    // we have the file, now open it
    FILE* listFile;
    if ((listFile = fopen(tmpListNameBuf, "w")) == NULL)
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
    }

    // and we can fill it
    BOOL isDir;
    const char* name;
    char namecnv[MAX_PATH];
    int errorOccured;
    if (!needANSIListFile)
        CharToOem(rootPath, rootPath);
    // pick the name
    while ((name = nextName(parent, 1, &isDir, NULL, NULL, param, &errorOccured)) != NULL)
    {
        if (!needANSIListFile)
            CharToOem(name, namecnv);
        else
            strcpy(namecnv, name);
        // and put it into the list
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
    // that's it
    fclose(listFile);

    // if an error occurred and the user decided to cancel the operation, end it
    if (errorOccured == SALENUM_CANCEL)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE;
    }

    //
    // Now we will launch the external program for deletion
    //
    // construct the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    if (!PackExpandCmdLine(archiveFileName, NULL, tmpListNameBuf, NULL,
                           modifyTable->DeleteCommand, cmdLine, PACK_CMDLINE_MAXLEN, NULL))
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNERR);
    }

    // check whether the command line is not too long
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

    // take the attributes in case we need them later
    DWORD fileAttrs = SalGetFileAttributes(archiveFileName);
    if (fileAttrs == 0xFFFFFFFF)
        fileAttrs = FILE_ATTRIBUTE_ARCHIVE;

    // back up the short archive file name, later we check whether the long name
    // survived -> if the short one remained, rename it back to the original long name
    char DOSArchiveFileName[MAX_PATH];
    if (!GetShortPathName(archiveFileName, DOSArchiveFileName, MAX_PATH))
        DOSArchiveFileName[0] = 0;

    // and run the external program
    BOOL exec = PackExecute(NULL, cmdLine, currentDir, modifyTable->ErrorTable);
    // meanwhile, check whether the long name did not vanish -> if the short one
    // remained, rename it to the original long name
    if (DOSArchiveFileName[0] != 0 &&
        SalGetFileAttributes(archiveFileName) == 0xFFFFFFFF &&
        SalGetFileAttributes(DOSArchiveFileName) != 0xFFFFFFFF)
    {
        SalMoveFile(DOSArchiveFileName, archiveFileName); // if it fails, we don't care...
    }
    if (!exec)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE; // error message has already been displayed
    }

    // if deleting removed the archive, create a zero-length file
    HANDLE tmpHandle = HANDLES_Q(CreateFile(archiveFileName, GENERIC_READ, 0, NULL,
                                            OPEN_ALWAYS, fileAttrs, NULL));
    if (tmpHandle != INVALID_HANDLE_VALUE)
        HANDLES(CloseHandle(tmpHandle));

    // the file list is no longer needed
    DeleteFile(tmpListNameBuf);

    return TRUE;
}
