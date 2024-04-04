// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dialogs.h"
#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "zip.h"
#include "pack.h"

//
// ****************************************************************************
// Constants and global variables
// ****************************************************************************
//

// Pointer to the error handling function
BOOL(*PackErrorHandlerPtr)
(HWND parent, const WORD errNum, ...) = EmptyErrorHandler;

const char* SPAWN_EXE_NAME = "salspawn.exe";
const char* SPAWN_EXE_PARAMS = "-c10000";

// Path to the salspawn program
char SpawnExe[MAX_PATH * 2] = {0};
BOOL SpawnExeInitialised = FALSE;

// to report a date error only once
BOOL FirstError;

// Table of archive definitions and operations on them - non-modifying operations
// !!! WARNING: when changing the order of external archivers, the order in the array must also be changed
// externalArchivers in the method CPlugins::FindViewEdit
const SPackBrowseTable PackBrowseTable[] =
    {
        // JAR 1.02 Win32
        {
            (TPackErrorTable*)&JARErrors, TRUE,
            "$(ArchivePath)", "$(Jar32bitExecutable) v -ju- \"$(ArchiveFileName)\"",
            NULL, "Analyzing", 4, 0, 3, "Total files listed:", ' ', 2, 3, 9, 8, 4, 1, 2,
            "$(TargetPath)", "$(Jar32bitExecutable) x -r- -jyc \"$(ArchiveFullName)\" !\"$(ListFullName)\"",
            "$(TargetPath)", "$(Jar32bitExecutable) e -r- \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", FALSE},
        // RAR 4.20 & 5.0 Win x86/x64
        {
            (TPackErrorTable*)&RARErrors, TRUE,
            "$(ArchivePath)", "$(Rar32bitExecutable) v -c- \"$(ArchiveFileName)\"",
            NULL, "--------", 0, 0, 2, "--------", ' ', 1, 2, 6, 5, 7, 3, 2,                              // After RAR 5.0, we are patching the indexes at runtime, see variable 'RAR5AndLater'
            "$(TargetPath)", "$(Rar32bitExecutable) x -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"", // from version 5.0 we need to enforce the -scol switch, version 4.20 doesn't mind; it occurs in other places and in the registry
            "$(TargetPath)", "$(Rar32bitExecutable) e \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", FALSE},
        // ARJ 2.60 MS-DOS
        {
            (TPackErrorTable*)&ARJErrors, FALSE,
            ".", "$(Arj16bitExecutable) v -ja1 $(ArchiveDOSFullName)",
            NULL, "--------", 0, 0, 2, "--------", ' ', 2, 5, 9, -8, 11, 1, 2,
            ".", "$(Arj16bitExecutable) x -p -va -hl -jyc $(ArchiveDOSFullName) $(TargetDOSPath)\\ !$(ListDOSFullName)",
            "$(TargetPath)", "$(Arj16bitExecutable) e -p -va -hl $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // LHA 2.55 MS-DOS
        {
            (TPackErrorTable*)&LHAErrors, FALSE,
            ".", "$(Lha16bitExecutable) v $(ArchiveDOSFullName)",
            NULL, "--------------", 0, 0, 2, "--------------", ' ', 1, 2, 6, 5, 7, 1, 2,
            ".", "$(Lha16bitExecutable) x -p -a -l1 -x1 -c $(ArchiveDOSFullName) $(TargetDOSPath)\\ @$(ListDOSFullName)",
            "$(TargetPath)", "$(Lha16bitExecutable) e -p -a -l1 -c $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // UC2 2r3 FOR MS-DOS
        {
            (TPackErrorTable*)&UC2Errors, FALSE,
            ".", "$(UC216bitExecutable) ~D $(ArchiveDOSFullName)",
            PackUC2List, "", 0, 0, 0, "", ' ', 0, 0, 0, 0, 0, 0, 0,
            ".", "$(UC216bitExecutable) EF $(ArchiveDOSFullName) ##$(TargetDOSPath) @$(ListDOSFullName)",
            "$(TargetPath)", "$(UC216bitExecutable) E $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // JAR 1.02 MS-DOS
        {
            (TPackErrorTable*)&JARErrors, FALSE,
            ".", "$(Jar16bitExecutable) v -ju- $(ArchiveDOSFullName)",
            NULL, "Analyzing", 4, 0, 3, "Total files listed:", ' ', 2, 3, 9, 8, 4, 1, 2,
            ".", "$(Jar16bitExecutable) x -r- -jyc $(ArchiveDOSFullName) -o$(TargetDOSPath) !$(ListDOSFullName)",
            "$(TargetPath)", "$(Jar16bitExecutable) e -r- $(ArchiveDOSFullName) \"$(ExtractFullName)\"", FALSE},
        // RAR 2.05 MS-DOS
        {
            (TPackErrorTable*)&RARErrors, FALSE,
            ".", "$(Rar16bitExecutable) v -c- $(ArchiveDOSFullName)",
            NULL, "--------", 0, 0, 2, "--------", ' ', 1, 2, 6, 5, 7, 3, 1,
            ".", "$(Rar16bitExecutable) x $(ArchiveDOSFullName) $(TargetDOSPath)\\ @$(ListDOSFullName)",
            "$(TargetPath)", "$(Rar16bitExecutable) e $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // PKZIP 2.50 Win32
        {
            NULL, TRUE,
            "$(ArchivePath)", "$(Zip32bitExecutable) -com=none -nozipextension \"$(ArchiveFileName)\"",
            NULL, "  ------  ------    -----", 0, 0, 1, "  ------           ------", ' ', 9, 1, 6, 5, 8, 3, 1,
            "$(TargetPath)", "$(Zip32bitExecutable) -ext -nozipextension -directories -path \"$(ArchiveFullName)\" @\"$(ListFullName)\"",
            "$(TargetPath)", "$(Zip32bitExecutable) -ext -nozipextension \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", TRUE},
        // PKUNZIP 2.04g MS-DOS
        {
            (TPackErrorTable*)&UNZIP204Errors, FALSE,
            ".", "$(Unzip16bitExecutable) -v $(ArchiveDOSFullName)",
            NULL, " ------  ------   -----", 0, 0, 1, " ------          ------", ' ', 9, 1, 6, 5, 8, 3, 1,
            ".", "$(Unzip16bitExecutable) -d $(ArchiveDOSFullName) $(TargetDOSPath)\\ @$(ListDOSFullName)",
            "$(TargetPath)", "$(Unzip16bitExecutable) $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // ARJ 3.00c Win32
        {
            (TPackErrorTable*)&ARJErrors, TRUE,
            "$(ArchivePath)", "$(Arj32bitExecutable) v -ja1 \"$(ArchiveFileName)\"",
            NULL, "--------", 0, 0, 0, "--------", ' ', 2, 5, 9, 8, 11, 1, 2,
            "$(TargetPath)", "$(Arj32bitExecutable) x -p -va -hl -jyc \"$(ArchiveFullName)\" !\"$(ListFullName)\"",
            "$(TargetPath)", "$(Arj32bitExecutable) e -p -va -hl \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", FALSE},
        // ACE 1.2b Win32
        {
            (TPackErrorTable*)&ACEErrors, TRUE,
            "$(ArchivePath)", "$(Ace32bitExecutable) v \"$(ArchiveFileName)\"",
            NULL, "Date    ", 0, 1, 1, "        ", 0xB3, 6, 4, 2, 1, 0, 3, 2,
            "$(TargetPath)", "$(Ace32bitExecutable) x -f \"$(ArchiveFullName)\" @\"$(ListFullName)\"",
            "$(TargetPath)", "$(Ace32bitExecutable) e -f \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", TRUE},
        // ACE 1.2b MS-DOS
        {
            (TPackErrorTable*)&ACEErrors, FALSE,
            ".", "$(Ace16bitExecutable) v $(ArchiveDOSFullName)",
            NULL, "Date    ", 0, 1, 1, "        ", 0xB3, 6, 4, 2, 1, 0, 3, 2,
            ".", "$(Ace16bitExecutable) x -f $(ArchiveDOSFullName) $(TargetDOSPath)\\ @$(ListDOSFullName)",
            "$(TargetPath)", "$(Ace16bitExecutable) e -f $(ArchiveDOSFullName) $(ExtractFullName)", FALSE}};

//
// ****************************************************************************
// Function
// ****************************************************************************
//

//
// ****************************************************************************
// Function for listing archives
//

//
// ****************************************************************************
// char *PackGetField(char *buffer, const int index, const int nameidx)
//
//   In the buffer string, it finds the item at the index-th position. Items can be
//   separated by any number of spaces, tabs, or line breaks.
//   (added vertical bar character (ASCII 0xB3) for ACE)
//   If we are iterating over an item with the index nameidx (usually a file name),
//   is treated as a separator item only when transitioning to a new line (the name can
//   containing spaces or tabs). It is called from PackScanLine().
//
//   RET: returns a pointer to the specified item in the buffer string or NULL if
//        cannot be found
//   IN: buffer is a line of text intended for analysis
//        index is the ordinal number of the item to be found
//        nameidx is the index of the "file name" item

char* PackGetField(char* buffer, const int index, const int nameidx, const char separator)
{
    CALL_STACK_MESSAGE5("PackGetField(%s, %d, %d, %u)", buffer, index, nameidx, separator);
    // The searched item for the given archiving program does not exist
    if (index == 0)
        return NULL;

    // indicates the currently selected item
    int i = 1;

    // skip initial spaces and tabs (if any)
    while (*buffer != '\0' && (*buffer == ' ' || *buffer == '\t' ||
                               *buffer == 0x10 || *buffer == 0x11 || // arrows for ACE
                               *buffer == separator))
        buffer++;

    // find the specified item
    while (index != i)
    {
        // we will iterate over an item
        if (i == nameidx)
            // if we are at the name, the separator is just a newline
            while (*buffer != '\0' && *buffer != '\n')
                buffer++;
        else
            // otherwise it is a space, tab, dash, or line break
            while (*buffer != '\0' && *buffer != ' ' && *buffer != '\n' &&
                   *buffer != '\t' && *buffer != 0x10 && *buffer != 0x11 &&
                   *buffer != separator)
                buffer++;

        // skip spaces after her
        while (*buffer != '\0' && (*buffer == ' ' || *buffer == '\t' ||
                                   *buffer == '\n' || *buffer == 0x10 ||
                                   *buffer == 0x11 || *buffer == separator))
            buffer++;

        // we are on the next item
        i++;
    }
    return buffer;
}

//
// ****************************************************************************
// BOOL PackScanLine(char *buffer, CSalamanderDirectory &dir, const int index)
//
//   Analyzes one item of the file list from the archive output
//   program. It is usually one line, but it can be more
//   connected - must contain all the information for a connection
//   file stored in the archive
//   Called from the function PackList()
//
//   RET: returns TRUE on success, FALSE on error
//        calls the callback function *PackErrorHandlerPtr in case of an error
//   IN:  buffer is a line of text intended for analysis - it is changed during analysis!
//        index is the index in the PackTable table to the corresponding row
//   OUT: CSalamanderDirectory is created and filled with archive data

BOOL PackScanLine(char* buffer, CSalamanderDirectory& dir, const int index,
                  const SPackBrowseTable* configTable, BOOL ARJHack)
{
    CALL_STACK_MESSAGE3("PackScanLine(%s, , %d,)", buffer, index);
    // file or directory to be added
    CFileData newfile;
    int idx;

    // buffer for the file name
    char filename[MAX_PATH];
    char* tmpfname = filename;

    // find the name in the line
    char* tmpbuf = PackGetField(buffer, configTable->NameIdx,
                                configTable->NameIdx,
                                configTable->Separator);
    // it is nonsense for the name not to exist, but if we leave it in the configuration
    // Deleting a user, it should save it
    if (tmpbuf == NULL)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCCFG);

    // we will eliminate any potential backslash or slash at the beginning
    if (*tmpbuf == '\\' || *tmpbuf == '/')
        tmpbuf++;
    // we only copy the name, replacing slashes with backslashes
    while (*tmpbuf != '\0' && *tmpbuf != '\n')
    {
        if (*tmpbuf == '/')
        {
            tmpbuf++;
            *tmpfname++ = '\\';
        }
        else
            *tmpfname++ = *tmpbuf++;
    }

    // remove any separators at the end of the name
    while (*(tmpfname - 1) == ' ' || *(tmpfname - 1) == '\t' ||
           *(tmpfname - 1) == configTable->Separator)
        tmpfname--;

    // if the processed object is not a directory according to the slash at the end of the name,
    // find attributes in the row, and if any of them is D or d,
    // It's also about a directory, so we add a backslash at the end
    if (*(tmpfname - 1) != '\\')
    {
        idx = configTable->AttrIdx;
        if (ARJHack)
            idx--;
        tmpbuf = PackGetField(buffer, idx,
                              configTable->NameIdx,
                              configTable->Separator);
        if (tmpbuf != NULL && *tmpbuf != '\0')
        {
            while (*tmpbuf != '\0' && *tmpbuf != '\n' && *tmpbuf != '\t' &&
                   *tmpbuf != ' ' && *tmpbuf != 'D' && *tmpbuf != 'd')
                tmpbuf++;
            if (*tmpbuf == 'D' || *tmpbuf == 'd')
                *tmpfname++ = '\\';
        }
    }
    // Let's finish and prepare a pointer to the last character (non-backslash)
    *tmpfname-- = '\0';
    if (*tmpfname == '\\')
        tmpfname--;

    char* pomptr = tmpfname; // points to the end of the name
    // separate it from the road
    while (pomptr > filename && *pomptr != '\\')
        pomptr--;

    char* pomptr2;
    if (*pomptr == '\\')
    {
        // both the name and the path are here
        *pomptr++ = '\0';
        pomptr2 = filename;
    }
    else
    {
        // only the name is here
        pomptr2 = NULL;
    }

    // now we have in the computer the name of the added directory or file
    // and in the pomptr2 case, the path to it
    newfile.NameLen = tmpfname - pomptr + 1;

    // set the name of a new file or directory
    newfile.Name = (char*)malloc(newfile.NameLen + 1);
    if (!newfile.Name)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_NOMEM);
    OemToCharBuff(pomptr, newfile.Name, newfile.NameLen); // Copy with conversion from OEM to ANSI
    newfile.Name[newfile.NameLen] = '\0';

    // Convert the path from OEM to ANSI
    if (pomptr2 != NULL)
        OemToChar(pomptr2, pomptr2);

    // set the extension
    char* s = tmpfname - 1;
    while (s >= pomptr && *s != '.')
        s--;
    if (s >= pomptr)
        //  if (s > pomptr)  // ".cvspass" in Windows is an extension ...
        newfile.Ext = newfile.Name + (s - pomptr) + 1;
    else
        newfile.Ext = newfile.Name + newfile.NameLen;

    // now we will read the date and time
    SYSTEMTIME t;
    idx = abs(configTable->DateIdx);
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // We did not find the item in the listing, we will set default
    if (tmpbuf == NULL)
    {
        t.wYear = 1980;
        t.wMonth = 1;
        t.wDay = 1;
    }
    else
    {
        // otherwise we will read all three parts of the data
        int i;
        for (i = 1; i < 4; i++)
        {
            WORD tmpnum = 0;
            // read a number
            while (*tmpbuf >= '0' && *tmpbuf <= '9')
            {
                tmpnum = tmpnum * 10 + (*tmpbuf - '0');
                tmpbuf++;
            }
            // and assign it to the correct variable
            if (configTable->DateYIdx == i)
                t.wYear = tmpnum;
            else if (configTable->DateMIdx == i)
                t.wMonth = tmpnum;
            else
                t.wDay = tmpnum;
            tmpbuf++;
        }
    }

    t.wDayOfWeek = 0; // ignored
    if (t.wYear < 100)
    {
        if (t.wYear >= 80)
            t.wYear += 1900;
        else
            t.wYear += 2000;
    }

    // now time
    idx = configTable->TimeIdx;
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // Reset in case we did not read the item (default time)
    t.wHour = 0;
    t.wMinute = 0;
    t.wSecond = 0;
    t.wMilliseconds = 0;
    if (tmpbuf != NULL)
    {
        // item exists, so let's load the time
        while (*tmpbuf >= '0' && *tmpbuf <= '9')
        {
            t.wHour = t.wHour * 10 + (*tmpbuf - '0');
            tmpbuf++;
        }
        // skip one delimiter character
        tmpbuf++;
        // the next digit must be minutes
        while (*tmpbuf >= '0' && *tmpbuf <= '9')
        {
            t.wMinute = t.wMinute * 10 + (*tmpbuf - '0');
            tmpbuf++;
        }
        // does not follow am/pm?
        if (*tmpbuf == 'a' || *tmpbuf == 'p' || *tmpbuf == 'A' || *tmpbuf == 'P')
        {
            if (*tmpbuf == 'p' || *tmpbuf == 'P')
            {
                t.wHour += 12;
            }
            tmpbuf++;
            if (*tmpbuf == 'm' || *tmpbuf == 'M')
                tmpbuf++;
        }
        // and if there is no item separator following, we can still load seconds
        if (*tmpbuf != '\0' && *tmpbuf != '\n' && *tmpbuf != '\t' &&
            *tmpbuf != ' ')
        {
            // skip separator
            tmpbuf++;
            // and let's read the seconds
            while (*tmpbuf >= '0' && *tmpbuf <= '9')
            {
                t.wSecond = t.wSecond * 10 + (*tmpbuf - '0');
                tmpbuf++;
            }
        }
        // does not follow am/pm?
        if (*tmpbuf == 'a' || *tmpbuf == 'p' || *tmpbuf == 'A' || *tmpbuf == 'P')
        {
            if (*tmpbuf == 'p' || *tmpbuf == 'P')
            {
                t.wHour += 12;
            }
            tmpbuf++;
            if (*tmpbuf == 'm' || *tmpbuf == 'M')
                tmpbuf++;
        }
    }

    // and save it into a structure
    FILETIME lt;
    if (!SystemTimeToFileTime(&t, &lt))
    {
        DWORD ret = GetLastError();
        if (ret != ERROR_INVALID_PARAMETER && ret != ERROR_SUCCESS)
        {
            char buff[1000];
            strcpy(buff, "SystemTimeToFileTime: ");
            strcat(buff, GetErrorText(ret));
            free(newfile.Name);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buff);
        }
        if (FirstError)
        {
            FirstError = FALSE;
            (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_DATETIME);
        }
        t.wYear = 1980;
        t.wMonth = 1;
        t.wDay = 1;
        t.wHour = 0;
        t.wMinute = 0;
        t.wSecond = 0;
        t.wMilliseconds = 0;
        if (!SystemTimeToFileTime(&t, &lt))
        {
            free(newfile.Name);
            return FALSE;
        }
    }
    if (!LocalFileTimeToFileTime(&lt, &newfile.LastWrite))
    {
        char buff[1000];
        strcpy(buff, "LocalFileTimeToFileTime: ");
        strcat(buff, GetErrorText(GetLastError()));
        free(newfile.Name);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buff);
    }

    // now let's read the size of the file
    idx = configTable->SizeIdx;
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // default is zero
    unsigned __int64 tmpvalue = 0;
    // if the item exists
    if (tmpbuf != NULL)
        // read it
        while (*tmpbuf >= '0' && *tmpbuf <= '9')
        {
            tmpvalue = tmpvalue * 10 + (*tmpbuf - '0');
            tmpbuf++;
        }
    // and we will set in the structure
    newfile.Size.Set((DWORD)(tmpvalue & 0xFFFFFFFF), (DWORD)(tmpvalue >> 32));

    // following attributes
    idx = configTable->AttrIdx;
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // default is not set
    newfile.Attr = 0;
    newfile.Hidden = 0;
    newfile.IsOffline = 0;
    if (tmpbuf != NULL)
        while (*tmpbuf != '\0' && *tmpbuf != '\n' && *tmpbuf != '\t' &&
               *tmpbuf != ' ')
        {
            switch (*tmpbuf)
            {
            // read-only attribute
            case 'R':
            case 'r':
                newfile.Attr |= FILE_ATTRIBUTE_READONLY;
                break;
            // archive attribute
            case 'A':
            case 'a':
                newfile.Attr |= FILE_ATTRIBUTE_ARCHIVE;
                break;
            // system attribute
            case 'S':
            case 's':
                newfile.Attr |= FILE_ATTRIBUTE_SYSTEM;
                break;
            // hidden attribute
            case 'H':
            case 'h':
                newfile.Attr |= FILE_ATTRIBUTE_HIDDEN;
                newfile.Hidden = 1;
                break;
            }
            tmpbuf++;
        }

    // set other items of the structure (those that are not zeroed in AddFile/Dir)
    newfile.DosName = NULL;
    newfile.PluginData = -1; // -1 is just ignored

    // and we will either add a new file or directory
    if (*(tmpfname + 1) != '\\')
    {
        newfile.IsLink = IsFileLink(newfile.Ext);

        // it's a file, add a file
        if (!dir.AddFile(pomptr2, newfile, NULL))
        {
            free(newfile.Name);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_FDATA);
        }
    }
    else
    {
        // it is a directory, add a directory
        newfile.Attr |= FILE_ATTRIBUTE_DIRECTORY;
        newfile.IsLink = 0;
        if (!Configuration.SortDirsByExt)
            newfile.Ext = newfile.Name + newfile.NameLen; // directories do not have extensions
        if (!dir.AddDir(pomptr2, newfile, NULL))
        {
            free(newfile.Name);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_FDATA);
        }
    }
    return TRUE;
}

//
// ****************************************************************************
// BOOL PackList(CFilesWindow *panel, const char *archiveFileName, CSalamanderDirectory &dir,
//               CPluginDataInterfaceAbstract *&pluginData, CPluginData *&plugin)
//
//   Function for determining the contents of the archive.
//
//   RET: returns TRUE on success, FALSE on error
//        calls the callback function *PackErrorHandlerPtr in case of an error
//   IN:  panel is the file panel of Salamander
//        archiveFileName is the name of the archive file to be listed
//   OUT: dir is filled with archive data
//        pluginData is an interface to data about columns defined by the archiver plugin
//        plugin is a record of the plug-in that performed ListArchive

BOOL PackList(CFilesWindow* panel, const char* archiveFileName, CSalamanderDirectory& dir,
              CPluginDataInterfaceAbstract*& pluginData, CPluginData*& plugin)
{
    CALL_STACK_MESSAGE2("PackList(, %s, , ,)", archiveFileName);
    // Let's clean up just in case
    dir.Clear(NULL);
    pluginData = NULL;
    plugin = NULL;

    FirstError = TRUE;

    // find the right one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // We did not find a supported archive - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    int index = PackerFormatConfig.GetUnpackerIndex(format);

    // Isn't it about internal processing (DLL)?
    if (index < 0)
    {
        plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelView)
        {
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->ListArchive(panel, archiveFileName, dir, pluginData);
    }

    // if we haven't found the way to the spawn yet, let's do it now
    if (!InitSpawnName(NULL))
        return FALSE;

    //
    // We will launch an external program with output redirection
    //
    const SPackBrowseTable* browseTable = ArchiverConfig.GetUnpackerConfigTable(index);

    // construct the current directory
    char currentDir[MAX_PATH];
    if (!PackExpandInitDir(archiveFileName, NULL, NULL, browseTable->ListInitDir,
                           currentDir, MAX_PATH))
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_IDIRERR);

    // we will construct the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    sprintf(cmdLine, "\"%s\" %s ", SpawnExe, SPAWN_EXE_PARAMS);
    int cmdIndex = (int)strlen(cmdLine);
    if (!PackExpandCmdLine(archiveFileName, NULL, NULL, NULL, browseTable->ListCommand,
                           cmdLine + cmdIndex, PACK_CMDLINE_MAXLEN - cmdIndex, NULL))
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNERR);

    char cmdForErrors[MAX_PATH]; // Path + name of the launched exe in case of an error
    if (PackExpandCmdLine(archiveFileName, NULL, NULL, NULL, browseTable->ListCommand,
                          cmdForErrors, MAX_PATH, NULL))
    {
        char* begin;
        char* p = cmdForErrors;
        while (*p == ' ')
            p++;
        begin = p;
        if (*p == '\"')
        {
            p++;
            begin = p;
            while (*p != '\"' && *p != 0)
                p++;
        }
        else
        {
            while (*p != ' ' && *p != 0)
                p++;
        }
        *p = 0;
        if (begin > cmdForErrors)
            memmove(cmdForErrors, begin, strlen(begin) + 1);
    }
    else
        cmdForErrors[0] = 0;

    // check if the command line is not too long
    if (!browseTable->SupportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // we need to inherit handles
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    // create a pipe for communication with the process
    HANDLE StdOutRd, StdOutWr, StdErrWr;
    if (!HANDLES(CreatePipe(&StdOutRd, &StdOutWr, &sa, 0)))
    {
        char buffer[1000];
        strcpy(buffer, "CreatePipe: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }
    // so that we can use it as stderr
    if (!HANDLES(DuplicateHandle(GetCurrentProcess(), StdOutWr, GetCurrentProcess(), &StdErrWr,
                                 0, TRUE, DUPLICATE_SAME_ACCESS)))
    {
        char buffer[1000];
        strcpy(buffer, "DuplicateHandle: ");
        strcat(buffer, GetErrorText(GetLastError()));
        HANDLES(CloseHandle(StdOutRd));
        HANDLES(CloseHandle(StdOutWr));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    // create structures for a new process
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = NULL;
    si.hStdOutput = StdOutWr;
    si.hStdError = StdErrWr;
    // and we are starting...
    if (!HANDLES(CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NEW_CONSOLE | CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                               NULL, currentDir, &si, &pi)))
    {
        // if it failed, we have the wrong path to salspawn
        DWORD err = GetLastError();
        HANDLES(CloseHandle(StdOutRd));
        HANDLES(CloseHandle(StdOutWr));
        HANDLES(CloseHandle(StdErrWr));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PROCESS, SpawnExe, GetErrorText(err));
    }

    // My uz tyhle handly nepotrebujeme, duplikaty si zavre child, nechame si jen StdOutRd
    HANDLES(CloseHandle(StdOutWr));
    HANDLES(CloseHandle(StdErrWr));

    // We will extract all data from the pipe into an array of lines
    char tmpbuff[1000];
    DWORD read;
    CPackLineArray lineArray(1000, 500);
    int buffOffset = 0;
    while (1)
    {
        // Read the full buffer from the end of unprocessed data
        if (!ReadFile(StdOutRd, tmpbuff + buffOffset, 1000 - buffOffset, &read, NULL))
            break;
        // we are starting from the beginning
        char* start = tmpbuff;
        // and we are looking for line endings throughout the entire buffer
        unsigned int i;
        for (i = 0; i < read + buffOffset; i++)
        {
            if (tmpbuff[i] == '\n')
            {
                // line length
                int lineLen = (int)(tmpbuff + i - start);
                // remove \r if it is there
                if (lineLen > 0 && tmpbuff[i - 1] == '\r')
                    lineLen--;
                // allocate a new line
                char* newLine = new char[lineLen + 2];
                // fill in the data and finish
                strncpy(newLine, start, lineLen);
                newLine[lineLen] = '\n';
                newLine[lineLen + 1] = '\0';
                // add it to the array
                lineArray.Add(newLine);
                // and move towards it
                start = tmpbuff + i + 1;
            }
        }
        // We have processed the buffer, now we will determine how much is left and shift it to the beginning of the buffer
        buffOffset = (int)(tmpbuff + read + buffOffset - start);
        if (buffOffset > 0)
            memmove(tmpbuff, start, buffOffset);
        // maximum line length is 990, hopefully it will be enough :-)
        if (buffOffset >= 990)
        {
            HANDLES(CloseHandle(StdOutRd));
            HANDLES(CloseHandle(pi.hProcess));
            HANDLES(CloseHandle(pi.hThread));
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
        }
    }

    // We have read it, we no longer need it
    HANDLES(CloseHandle(StdOutRd));

    // We will wait for the external program to finish (it should have been finished long ago, but better safe than sorry)
    if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
    {
        char buffer[1000];
        strcpy(buffer, "WaitForSingleObject: ");
        strcat(buffer, GetErrorText(GetLastError()));
        HANDLES(CloseHandle(pi.hProcess));
        HANDLES(CloseHandle(pi.hThread));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    // Let's focus on ourselves again
    SetForegroundWindow(MainWindow->HWindow);

    // and we will find out how it turned out - hopefully all return 0 as success
    DWORD exitCode;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode))
    {
        char buffer[1000];
        strcpy(buffer, "GetExitCodeProcess: ");
        strcat(buffer, GetErrorText(GetLastError()));
        HANDLES(CloseHandle(pi.hProcess));
        HANDLES(CloseHandle(pi.hThread));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    // Release process handles
    HANDLES(CloseHandle(pi.hProcess));
    HANDLES(CloseHandle(pi.hThread));

    if (exitCode != 0)
    {
        //
        // First errors in salspawn.exe
        //
        if (exitCode >= SPAWN_ERR_BASE)
        {
            // error salspawn.exe - wrong parameters or something...
            if (exitCode >= SPAWN_ERR_BASE && exitCode < SPAWN_ERR_BASE * 2)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, SPAWN_EXE_NAME, LoadStr(IDS_PACKRET_SPAWN));
            // Error CreateProcess
            if (exitCode >= SPAWN_ERR_BASE * 2 && exitCode < SPAWN_ERR_BASE * 3)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PROCESS, cmdForErrors, GetErrorText(exitCode - SPAWN_ERR_BASE * 2));
            // error WaitForSingleObject
            if (exitCode >= SPAWN_ERR_BASE * 3 && exitCode < SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "WaitForSingleObject: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 3));
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
            // error GetExitCodeProcess
            if (exitCode >= SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "GetExitCodeProcess: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 4));
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
        }
        //
        // now there will be errors from an external program
        //
        // if errorTable == NULL, then we do not perform translation (table does not exist)
        if (!browseTable->ErrorTable)
        {
            char buffer[1000];
            sprintf(buffer, LoadStr(IDS_PACKRET_GENERAL), exitCode);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, cmdForErrors, buffer);
        }
        // we will find the corresponding text in the table
        int i;
        for (i = 0; (*browseTable->ErrorTable)[i][0] != -1 &&
                    (*browseTable->ErrorTable)[i][0] != (int)exitCode;
             i++)
            ;
        // Did we find him?
        if ((*browseTable->ErrorTable)[i][0] == -1)
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, cmdForErrors, LoadStr(IDS_PACKRET_UNKNOWN));
        else
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, cmdForErrors,
                                          LoadStr((*browseTable->ErrorTable)[i][1]));
    }

    //
    // now the main - parsing the output of the packer into our structures
    //
    if (lineArray.Count == 0)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_NOOUTPUT);

    // if we need to use a special parsing function, we are going to do it right now
    if (browseTable->SpecialList)
        return (*(browseTable->SpecialList))(archiveFileName, lineArray, dir);

    // now we are parsing with a universal parser

    // some local variables
    char* line = NULL; // buffer for building "multi-line rows"
    int lines = 0;     // how many lines of a multi-line item we have read
    int validData = 0; // whether we are reading the header/footer or valid data
    int toSkip = browseTable->LinesToSkip;
    int alwaysSkip = browseTable->AlwaysSkip;
    int linesPerFile = browseTable->LinesPerFile;
    BOOL ARJHack;
    BOOL RAR5AndLater = FALSE; // Starting from RAR 5.0, there is a new format of the listing (commands 'v' and 'l'), the name is in the last column

    int i;
    for (i = 0; i < lineArray.Count; i++)
    {
        // let's find out what to actually do with this data
        switch (validData)
        {
        case 0: // we are in the header
            // let's find out if we're staying in it
            if (!strncmp(lineArray[i], browseTable->StartString,
                         strlen(browseTable->StartString)))
                validData++;
            if (i == 1 && strncmp(lineArray[i], "RAR ", 4) == 0 && lineArray[i][4] >= '5' && lineArray[i][4] <= '9')
            {
                RAR5AndLater = TRUE; // test fails from RAR 10, which will come in handy ;-)
                linesPerFile = 1;
            }
            // Anyway, we are not interested in this line
            continue;
        case 2: // we are at the bottom and the dog is shitting on it
            continue;
        case 1: // we are in the data - just need to find out if it ends and then get to work
            // if we still have something to skip, we will do it now
            if (alwaysSkip > 0)
            {
                alwaysSkip--;
                continue;
            }
            if (!strncmp(lineArray[i], browseTable->StopString,
                         strlen(browseTable->StopString)))
            {
                validData++;
                // there may be some remnants of the previous line left
                if (line)
                    free(line);
                continue;
            }
        }

        // if we still have something to skip, we will do it now
        if (toSkip > 0)
        {
            toSkip--;
            continue;
        }

        // we have another row
        lines++;
        // if it's the first one, we need to allocate a buffer
        if (lines == 1)
        {
            // Let's determine if we are two or four bytes (ARJ32 hack)
            if (browseTable->LinesPerFile == 0)
                if (i + 3 < lineArray.Count && lineArray[i + 2][3] == ' ')
                    linesPerFile = 4;
                else
                    linesPerFile = 2;
            // check if the OS type is missing (ARJ16 hack)
            ARJHack = FALSE;
            if (browseTable->DateIdx < 0)
                if (lineArray[i + 1][5] == ' ')
                    ARJHack = TRUE;
            // Do we really have so many lines?
            if (i + linesPerFile - 1 >= lineArray.Count)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // calculate the resulting length
            int len = 0;
            int j;
            for (j = 0; j < linesPerFile; j++)
                len = len + (int)strlen(lineArray[i + j]);
            // allocate a buffer for her
            line = (char*)malloc(len + 1);
            if (!line)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_NOMEM);
            // and initialize it
            line[0] = '\0';
        }
        // if it's not the last line, we just add it to the buffer
        if (lines < linesPerFile)
        {
            strcat(line, lineArray[i]);
            continue;
        }
        // if this is the last line, we add it to the buffer
        strcat(line, lineArray[i]);

        // and we have everything to one item - process it
        SPackBrowseTable browseTableRAR5;
        if (RAR5AndLater)
        {
            memmove(&browseTableRAR5, browseTable, sizeof(SPackBrowseTable));
            browseTableRAR5.NameIdx = 8;
            browseTableRAR5.AttrIdx = 1;
        }
        BOOL ret = PackScanLine(line, dir, index, RAR5AndLater ? &browseTableRAR5 : browseTable, ARJHack);
        // We no longer need the buffer
        free(line);
        line = NULL;
        if (!ret)
            return FALSE; // We don't need to call the error function anymore, it was already called from PackScanLine.

        // initialize variables
        lines = 0;
    }

    // if we ended up somewhere other than at the bottom, we have a problem
    if (validData < 2)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);

    return TRUE;
}

//
// ****************************************************************************
// BOOL PackUC2List(const char *archiveFileName, CPackLineArray &lineArray,
//                  CSalamanderDirectory &dir)
//
//   Function for retrieving the contents of a UC2 format archive (parser only)
//
//   RET: returns TRUE on success, FALSE on error
//        calls the callback function *PackErrorHandlerPtr in case of an error
//   IN:  archiveFileName is the name of the archive we are working with
//        lineArray is an array of lines of the archiver's output
//   OUT: directory is created and filled with archive data

BOOL PackUC2List(const char* archiveFileName, CPackLineArray& lineArray,
                 CSalamanderDirectory& dir)
{
    CALL_STACK_MESSAGE2("PackUC2List(%s, ,)", archiveFileName);
    // First, we will delete the temporary file that UC2 creates when using the ~D flag
    char arcPath[MAX_PATH];
    const char* arcName = strrchr(archiveFileName, '\\') + 1;
    strncpy(arcPath, archiveFileName, arcName - archiveFileName);
    arcPath[arcName - archiveFileName] = '\0';
    strcat(arcPath, "U$~RESLT.OK");
    DeleteFile(arcPath);

    char* txtPtr;         // pointer to the current position in the read line
    char currentDir[256]; // current directory we are exploring
    int line = 0;         // index to the array of rows
    // a bit unnecessary check, but better safe than sorry
    if (lineArray.Count < 1)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);

    // main parsing loop
    while (1)
    {
        // file or directory to be added
        CFileData newfile;

        // skip initial spaces
        for (txtPtr = lineArray[line]; *txtPtr == ' '; txtPtr++)
            ;

        // if the item is END, we are done
        if (!strncmp(txtPtr, "END", 3))
            break;

        // if the item is a LIST, then we determine in which directory we are
        if (!strncmp(txtPtr, "LIST", 4))
        {
            // we will reach the beginning of the name
            while (*txtPtr != '\0' && *txtPtr != '[')
                txtPtr++;
            // error handling - should not occur
            if (*txtPtr == '\0')
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // and the first letter of the name
            txtPtr++;
            int i = 0;
            // skip initial backslashes
            while (*txtPtr == '\\')
                txtPtr++;
            // copy the name into a variable
            while (*txtPtr != '\0' && *txtPtr != ']')
                currentDir[i++] = *txtPtr++;
            // end the string
            currentDir[i] = '\0';
            OemToChar(currentDir, currentDir);
            // and check again
            if (*txtPtr == '\0')
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // prepare the next line
            if (++line > lineArray.Count - 1)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // and we're off for another round
            continue;
        }

        // if the item is FILE/DIR, then we create a file/directory
        if (!strncmp(txtPtr, "DIR", 3) || !strncmp(txtPtr, "FILE", 4))
        {
            // What is this about? About a file or a directory
            BOOL isDir = TRUE;
            if (!strncmp(txtPtr, "FILE", 4))
                isDir = FALSE;

            // we will prepare some default values
            SYSTEMTIME t;
            t.wYear = 1980;
            t.wMonth = 1;
            t.wDay = 1;
            t.wDayOfWeek = 0; // ignored
            t.wHour = 0;
            t.wMinute = 0;
            t.wSecond = 0;
            t.wMilliseconds = 0;

            newfile.Size = CQuadWord(0, 0);
            newfile.DosName = NULL;
            newfile.PluginData = -1; // -1 is just ignored

            // main loop for parsing files/directories
            // Ends as soon as we encounter a keyword that we do not know
            while (1)
            {
                // prepare the next line
                if (++line > lineArray.Count - 1)
                    return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);

                // skip initial spaces
                for (txtPtr = lineArray[line]; *txtPtr == ' '; txtPtr++)
                    ;

                // Is it about the name?
                if (!strncmp(txtPtr, "NAME=", 5))
                {
                    // go to the beginning of the name
                    while (*txtPtr != '\0' && *txtPtr != '[')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    int i = 0;
                    // we copy the name into the variable newName
                    char newName[15];
                    while (*txtPtr != '\0' && *txtPtr != ']')
                        newName[i++] = *txtPtr++;
                    newName[i] = '\0';
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    // and put it into the structure
                    newfile.NameLen = strlen(newName);
                    newfile.Name = (char*)malloc(newfile.NameLen + 1);
                    if (!newfile.Name)
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    OemToChar(newName, newfile.Name);
                    newfile.Ext = strrchr(newfile.Name, '.');
                    if (newfile.Ext != NULL) // ".cvspass" in Windows is an extension ...
                                             //          if (newfile.Ext != NULL && newfile.Name != newfile.Ext)
                        newfile.Ext++;
                    else
                        newfile.Ext = newfile.Name + newfile.NameLen;
                    // and another round ...
                    continue;
                }
                // or is it perhaps a date?
                if (!strncmp(txtPtr, "DATE(MDY)=", 10))
                {
                    // we reset
                    t.wYear = 0;
                    t.wMonth = 0;
                    t.wDay = 0;
                    // Let's move to the beginning of the data
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // read the month
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wMonth = t.wMonth * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // read the day
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wDay = t.wDay * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // read the year
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wYear = t.wYear * 10 + *txtPtr++ - '0';

                    // conversion, just in case (should not be necessary)
                    if (t.wYear < 100)
                    {
                        if (t.wYear >= 80)
                            t.wYear += 1900;
                        else
                            t.wYear += 2000;
                    }
                    if (t.wMonth == 0)
                        t.wMonth = 1;
                    if (t.wDay == 0)
                        t.wDay = 1;

                    // and again in a loop...
                    continue;
                }
                // it could also be the time of last modification
                if (!strncmp(txtPtr, "TIME(HMS)=", 10))
                {
                    // reset again
                    t.wHour = 0;
                    t.wMinute = 0;
                    t.wSecond = 0;
                    // start of data
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // read the time
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wHour = t.wHour * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // read a minute
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wMinute = t.wMinute * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // read a second
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wSecond = t.wSecond * 10 + *txtPtr++ - '0';

                    // and again ...
                    continue;
                }
                // there are still attributes...
                if (!strncmp(txtPtr, "ATTRIB=", 7))
                {
                    // start of data
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // initialize in advance
                    newfile.Attr = 0;
                    newfile.Hidden = 0;
                    // and arm ourselves with what is needed
                    while (*txtPtr != '\0')
                    {
                        switch (*txtPtr++)
                        {
                        // readonly attribute
                        case 'R':
                            newfile.Attr |= FILE_ATTRIBUTE_READONLY;
                            break;
                        // archive attribute
                        case 'A':
                            newfile.Attr |= FILE_ATTRIBUTE_ARCHIVE;
                            break;
                        // system attribute
                        case 'S':
                            newfile.Attr |= FILE_ATTRIBUTE_SYSTEM;
                            break;
                        // hidden attribute
                        case 'H':
                            newfile.Attr |= FILE_ATTRIBUTE_HIDDEN;
                            newfile.Hidden = 1;
                        }
                    }
                    // and back to the point ...
                    continue;
                }
                // and finally the size
                if (!strncmp(txtPtr, "SIZE=", 5))
                {
                    // we will go over unimportant things again
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // we need an auxiliary variable
                    unsigned __int64 tmpvalue = 0;
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        tmpvalue = tmpvalue * 10 + *txtPtr++ - '0';
                    // and we will set in the structure
                    newfile.Size.Set((DWORD)(tmpvalue & 0xFFFFFFFF), (DWORD)(tmpvalue >> 32));
                    // and a line break on the next line
                    continue;
                }
                // dummy values - we need to know them, but we can ignore them
                if (!strncmp(txtPtr, "VERSION=", 8))
                    continue;
                if (!strncmp(txtPtr, "CHECK=", 6))
                    continue;

                // it is not any known item, so this will be the end of the section
                break;
            }
            //
            // we have everything, let's create an object
            //

            // we will store in the structure what is not there yet
            FILETIME lt;
            if (!SystemTimeToFileTime(&t, &lt))
            {
                char buffer[1000];
                strcpy(buffer, "SystemTimeToFileTime: ");
                strcat(buffer, GetErrorText(GetLastError()));
                free(newfile.Name);
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
            if (!LocalFileTimeToFileTime(&lt, &newfile.LastWrite))
            {
                char buffer[1000];
                strcpy(buffer, "LocalFileTimeToFileTime: ");
                strcat(buffer, GetErrorText(GetLastError()));
                free(newfile.Name);
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
            // and finally, we will simply create a new object
            newfile.IsOffline = 0;
            if (isDir)
            {
                // as for the directory, we do it here
                newfile.Attr |= FILE_ATTRIBUTE_DIRECTORY;
                if (!Configuration.SortDirsByExt)
                    newfile.Ext = newfile.Name + newfile.NameLen; // directories do not have extensions
                newfile.IsLink = 0;
                if (!dir.AddDir(currentDir, newfile, NULL))
                {
                    free(newfile.Name);
                    return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_FDATA);
                }
            }
            else
            {
                newfile.IsLink = IsFileLink(newfile.Ext);

                // as for the file, we go this way
                if (!dir.AddFile(currentDir, newfile, NULL))
                {
                    free(newfile.Name);
                    return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_FDATA);
                }
            }
            // and let's go for another round
            continue;
        }
    }
    return TRUE;
}

//
// ****************************************************************************
// Function for decompression
//

//
// ****************************************************************************
// BOOL PackUncompress(HWND parent, CFilesWindow *panel, const char *archiveFileName,
//                     CPluginDataInterfaceAbstract *pluginData,
//                     const char *targetDir, const char *archiveRoot,
//                     SalEnumSelection nextName, void *param)
//
//   Function for extracting the requested files from the archive.
//
//   RET: returns TRUE on success, FALSE on error
//        calls the callback function *PackErrorHandlerPtr in case of an error
//   IN:  parent is the parent for message boxes
//        panel is a pointer to the file panel of the salamander
//        archiveFileName is the name of the archive from which we are unpacking
//        targetDir is the path where the files are extracted
//        archiveRoot is the directory in the archive from which we are unpacking
//        nextName is a callback function for enumerating names to unpack
//        param are parameters for the enumeration function
//        pluginData is an interface for working with data about files/directories that are specific to the plugin
//   OUT:

BOOL PackUncompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* targetDir, const char* archiveRoot,
                    SalEnumSelection nextName, void* param)
{
    CALL_STACK_MESSAGE4("PackUncompress(, , %s, , %s, %s, ,)", archiveFileName, targetDir, archiveRoot);
    // find the right one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // We did not find a supported archive - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    int index = PackerFormatConfig.GetUnpackerIndex(format);

    // Isn't it about internal processing (DLL)?
    if (index < 0)
    {
        CPluginData* plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelView)
        {
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->UnpackArchive(panel, archiveFileName, pluginData, targetDir, archiveRoot, nextName, param);
    }
    const SPackBrowseTable* browseTable = ArchiverConfig.GetUnpackerConfigTable(index);

    return PackUniversalUncompress(parent, browseTable->UncompressCommand,
                                   browseTable->ErrorTable, browseTable->UncompressInitDir, TRUE, panel,
                                   browseTable->SupportLongNames, archiveFileName, targetDir,
                                   archiveRoot, nextName, param, browseTable->NeedANSIListFile);
}

//
// ****************************************************************************
// BOOL PackUniversalUncompress(HWND parent, const char *command, TPackErrorTable *const errorTable,
//                              const char *initDir, BOOL expandInitDir, CFilesWindow *panel,
//                              const BOOL supportLongNames, const char *archiveFileName,
//                              const char *targetDir, const char *archiveRoot,
//                              SalEnumSelection nextName, void *param, BOOL needANSIListFile)
//
//   Function for extracting the required files from the archive. Unlike the previous one
//   is more general, does not use a configuration table - can be called independently,
//   all is determined only by parameters
//
//   RET: returns TRUE on success, FALSE on error
//        calls the callback function *PackErrorHandlerPtr in case of an error
//   IN:  parent is the parent for message boxes
//        command is the command line used for extracting from the archive
//        errorTable is a pointer to a table of return codes, or NULL if it does not exist
//        initDir is the directory in which the archiver should be launched
//        panel is a pointer to the file panel of the salamander
//        supportLongNames indicates whether the archiver supports long names
//        archiveFileName is the name of the archive from which we are unpacking
//        targetDir is the path where the files are extracted
//        archiveRoot is the path in the archive from which we are unpacking, or NULL
//        nextName is a callback function for file enumeration during extraction
//        param are the parameters passed to the callback function
//        needANSIListFile is TRUE if the file list should be in ANSI (not OEM)
//   OUT:

BOOL PackUniversalUncompress(HWND parent, const char* command, TPackErrorTable* const errorTable,
                             const char* initDir, BOOL expandInitDir, CFilesWindow* panel,
                             const BOOL supportLongNames, const char* archiveFileName,
                             const char* targetDir, const char* archiveRoot,
                             SalEnumSelection nextName, void* param, BOOL needANSIListFile)
{
    CALL_STACK_MESSAGE9("PackUniversalUncompress(, %s, , %s, %d, , %d, %s, %s, %s, , , %d)",
                        command, initDir, expandInitDir, supportLongNames, archiveFileName,
                        targetDir, archiveRoot, needANSIListFile);

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
            strcat(rootPath, "\\");
        }
    }
    else
    {
        rootPath[0] = '\0';
    }

    //
    // Now it's time for the helper directory to be unpacked

    // Creating a name for the temporary directory
    char tmpDirNameBuf[MAX_PATH];
    if (!SalGetTempFileName(targetDir, "PACK", tmpDirNameBuf, FALSE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    //
    // Now we will prepare a helper file with a list of files to be extracted in the %TEMP% directory
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
        RemoveDirectory(tmpDirNameBuf);
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
    }

    // and we can fulfill it
    BOOL isDir;
    CQuadWord size;
    const char* name;
    char namecnv[MAX_PATH];
    CQuadWord totalSize(0, 0);
    int errorOccured;

    if (!needANSIListFile)
        CharToOem(rootPath, rootPath);
    // choose a name
    while ((name = nextName(parent, 1, &isDir, &size, NULL, param, &errorOccured)) != NULL)
    {
        if (!needANSIListFile)
            CharToOem(name, namecnv);
        else
            strcpy(namecnv, name);
        // calculate the total size
        totalSize += size;
        // insert the name into the list
        if (!isDir)
        {
            if (fprintf(listFile, "%s%s\n", rootPath, namecnv) <= 0)
            {
                fclose(listFile);
                DeleteFile(tmpListNameBuf);
                RemoveDirectory(tmpDirNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
            }
        }
    }
    // and that's it
    fclose(listFile);

    // if an error occurred and the user decided to cancel the operation, let's terminate it +
    // test for sufficient free disk space
    if (errorOccured == SALENUM_CANCEL ||
        !TestFreeSpace(parent, tmpDirNameBuf, totalSize, LoadStr(IDS_PACKERR_TITLE)))
    {
        DeleteFile(tmpListNameBuf);
        RemoveDirectory(tmpDirNameBuf);
        return FALSE;
    }

    //
    // Now we will run an external program to unpack
    //
    // we will construct the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    if (!PackExpandCmdLine(archiveFileName, tmpDirNameBuf, tmpListNameBuf, NULL,
                           command, cmdLine, PACK_CMDLINE_MAXLEN, NULL))
    {
        DeleteFile(tmpListNameBuf);
        RemoveDirectory(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNERR);
    }

    // check if the command line is not too long
    if (!supportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        DeleteFile(tmpListNameBuf);
        RemoveDirectory(tmpDirNameBuf);
        strcpy(buffer, cmdLine);
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
            RemoveDirectory(tmpDirNameBuf);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
        }
    }
    else
    {
        if (!PackExpandInitDir(archiveFileName, NULL, tmpDirNameBuf, initDir, currentDir, MAX_PATH))
        {
            DeleteFile(tmpListNameBuf);
            RemoveDirectory(tmpDirNameBuf);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
        }
    }

    // and we will launch an external program
    if (!PackExecute(NULL, cmdLine, currentDir, errorTable))
    {
        DeleteFile(tmpListNameBuf);
        RemoveTemporaryDir(tmpDirNameBuf);
        return FALSE; // Error message has already been thrown
    }
    // list of files is no longer needed
    DeleteFile(tmpListNameBuf);

    // and now we finally move the files where they belong
    char srcDir[MAX_PATH];
    strcpy(srcDir, tmpDirNameBuf);
    if (*rootPath != '\0')
    {
        // we will find the unpacked subdirectory path - the subdirectory names may not match due to
        // in Czech and long names :-(
        char* r = rootPath;
        WIN32_FIND_DATA foundFile;
        char buffer[1000];
        while (1)
        {
            if (*r == 0)
                break;
            while (*r != 0 && *r != '\\')
                r++; // jump one floor up in the original rootPath
            while (*r == '\\')
                r++; // Escape backslashes in the original rootPath
            strcat(srcDir, "\\*");

            HANDLE found = HANDLES_Q(FindFirstFile(srcDir, &foundFile));
            if (found == INVALID_HANDLE_VALUE)
            {
                strcpy(buffer, "FindFirstFile: ");

            _ERR:

                strcat(buffer, GetErrorText(GetLastError()));
                RemoveTemporaryDir(tmpDirNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
            }
            while (foundFile.cFileName[0] == 0 ||
                   strcmp(foundFile.cFileName, ".") == 0 || // Ignore "." and ".."
                   strcmp(foundFile.cFileName, "..") == 0)
            {
                if (!FindNextFile(found, &foundFile))
                {
                    HANDLES(FindClose(found));
                    strcpy(buffer, "FindNextFile: ");
                    goto _ERR;
                }
            }
            HANDLES(FindClose(found));

            if (foundFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            { // we will attach another subdirectory to the path
                srcDir[strlen(srcDir) - 1] = 0;
                strcat(srcDir, foundFile.cFileName);
            }
            else
            {
                TRACE_E("Unexpected error in PackUniversalUncompress().");
                RemoveTemporaryDir(tmpDirNameBuf);
                return FALSE;
            }
        }
    }
    if (!panel->MoveFiles(srcDir, targetDir, tmpDirNameBuf, archiveFileName))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_MOVE);
    }

    RemoveTemporaryDir(tmpDirNameBuf);
    return TRUE;
}

//
// ****************************************************************************
// const char * WINAPI PackEnumMask(HWND parent, int enumFiles, BOOL *isDir, CQuadWord *size,
//                                  const CFileData **fileData, void *param, int *errorOccured)
//
//   Callback function for enumerating specified masks
//
//   RET: Returns the mask that is currently in line, or NULL if done
//   IN:  enumFiles is ignored
//        fileData is ignored (only initialized to NULL)
//        param is a pointer to a string of file masks separated by semicolons
//   OUT: isDir is always FALSE
//        size is always 0
//        dochazi ke zmene retezce masek souboru oddelenych strednikem (";"->"\0" pri oddelovani masek, ";;"->";" + eliminace ";" na konci)

const char* WINAPI PackEnumMask(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                const CFileData** fileData, void* param, int* errorOccured)
{
    CALL_STACK_MESSAGE2("PackEnumMask(%d, , ,)", enumFiles);
    if (errorOccured != NULL)
        *errorOccured = SALENUM_SUCCESS;
    // set unused variables
    if (isDir != NULL)
        *isDir = FALSE;
    if (size != NULL)
        *size = CQuadWord(0, 0);
    if (fileData != NULL)
        *fileData = NULL;

    // if there are no more masks, we return NULL - end
    if (param == NULL || *(char**)param == NULL)
        return NULL;

    // remove any possible semicolon at the end of the string (shortening the string)
    char* ptr = *(char**)param + strlen(*(char**)param) - 1;
    char* endPtr = ptr;
    while (1)
    {
        while (ptr >= *(char**)param && *ptr == ';')
            ptr--;
        if (((endPtr - ptr) & 1) == 1)
            *endPtr-- = 0; // ';' is ignored only if it is odd (even number will be converted later ";;"->";")
        if (endPtr >= *(char**)param && *endPtr <= ' ')
        {
            endPtr--;
            while (endPtr >= *(char**)param && *endPtr <= ' ')
                endPtr--; // ignore white-spaces at the end
            *(endPtr + 1) = 0;
            ptr = endPtr; // We need to try again to see if it is necessary to skip the odd ';'
        }
        else
            break;
    }
    // we don't have any mask anymore, there are only semicolons and white-spaces (or nothing :-) )
    if (endPtr < *(char**)param)
        return NULL;

    // otherwise find the last semicolon (only if there is an odd number of them, otherwise replace ";;" with ";") - before the last mask
    while (ptr >= *(char**)param)
    {
        if (*ptr == ';')
        {
            char* p = ptr - 1;
            while (p >= *(char**)param && *p == ';')
                p--;
            if (((ptr - p) & 1) == 1)
                break;
            else
                ptr = p;
        }
        else
            ptr--;
    }

    if (ptr < *(char**)param)
    {
        // if there is no semicolon anymore, we have only one
        *(char**)param = NULL;
    }
    else
    {
        // Remove the last mask from the others by zero instead of the found semicolon
        *ptr = '\0';
    }

    while (*(ptr + 1) != 0 && *(ptr + 1) <= ' ')
        ptr++; // skip white-spaces at the beginning of the mask
    char* s = ptr + 1;
    while (*s != 0)
    {
        if (*s == ';' && *(s + 1) == ';')
            memmove(s, s + 1, strlen(s + 1) + 1);
        s++;
    }
    // and return it
    return ptr + 1;
}

//
// ****************************************************************************
// BOOL PackUnpackOneFile(CFilesWindow *panel, const char *archiveFileName,
//                        CPluginDataInterfaceAbstract *pluginData, const char *nameInArchive,
//                        CFileData *fileData, const char *targetDir, const char *newFileName,
//                        BOOL *renamingNotSupported)
//
//   Function for extracting one file from the archive (for viewer).
//
//   RET: returns TRUE on success, FALSE on error
//        calls the callback function *PackErrorHandlerPtr in case of an error
//   IN:  panel is the file panel of Salamander
//        archiveFileName is the name of the archive from which we are unpacking
//        nameInArchive is the name of the file we are unpacking
//        fileData is a pointer to the CFileData structure of the unpacked file
//        targetDir is the path where the file should be extracted
//        newFileName (if not NULL) is the new name of the extracted file (when extracting
//          must be renamed from the original name to this new one)
//        renamingNotSupported (only if newFileName is not NULL) - set TRUE if the plugin
//          It does not support renaming during unpacking, Salamander will display an error message
//   OUT:

BOOL PackUnpackOneFile(CFilesWindow* panel, const char* archiveFileName,
                       CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                       CFileData* fileData, const char* targetDir, const char* newFileName,
                       BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE5("PackUnpackOneFile(, %s, , %s, , %s, %s, )",
                        archiveFileName, nameInArchive, targetDir, newFileName);

    // find the right one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // We did not find a supported archive - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    int index = PackerFormatConfig.GetUnpackerIndex(format);

    // Isn't it about internal processing (DLL)?
    if (index < 0)
    {
        CPluginData* plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelView)
        {
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->UnpackOneFile(panel, archiveFileName, pluginData, nameInArchive,
                                     fileData, targetDir, newFileName, renamingNotSupported);
    }

    if (newFileName != NULL) // For external archivers, renaming is not supported yet (I didn't even bother to find out if it's possible).
    {
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_INVALIDNAME);
    }

    //
    // We will create a temporary directory where we will extract the file
    //

    // buffer for the fullname of the auxiliary directory
    char tmpDirNameBuf[MAX_PATH];
    if (!SalGetTempFileName(targetDir, "PACK", tmpDirNameBuf, FALSE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    //
    // Now we will run an external program to unpack
    //
    const SPackBrowseTable* browseTable = ArchiverConfig.GetUnpackerConfigTable(index);

    // we will construct the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    if (!PackExpandCmdLine(archiveFileName, tmpDirNameBuf, NULL, nameInArchive,
                           browseTable->ExtractCommand, cmdLine, PACK_CMDLINE_MAXLEN, NULL))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNERR);
    }

    // check if the command line is not too long
    if (!browseTable->SupportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        RemoveTemporaryDir(tmpDirNameBuf);
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // construct the current directory
    char currentDir[MAX_PATH];
    if (!PackExpandInitDir(archiveFileName, NULL, tmpDirNameBuf, browseTable->ExtractInitDir,
                           currentDir, MAX_PATH))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_IDIRERR);
    }

    // and we will launch an external program
    if (!PackExecute(NULL, cmdLine, currentDir, browseTable->ErrorTable))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return FALSE; // Error message has already been thrown
    }

    // We find the unpacked file - the name does not have to match due to Czech and long names :-(
    char* extractedFile = (char*)malloc(strlen(tmpDirNameBuf) + 2 + 1);
    WIN32_FIND_DATA foundFile;
    strcpy(extractedFile, tmpDirNameBuf);
    strcat(extractedFile, "\\*");
    HANDLE found = HANDLES_Q(FindFirstFile(extractedFile, &foundFile));
    if (found == INVALID_HANDLE_VALUE)
    {
        char buffer[1000];
        strcpy(buffer, "FindFirstFile: ");
        strcat(buffer, GetErrorText(GetLastError()));
        RemoveTemporaryDir(tmpDirNameBuf);
        free(extractedFile);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }
    free(extractedFile);
    while (foundFile.cFileName[0] == 0 ||
           strcmp(foundFile.cFileName, ".") == 0 || strcmp(foundFile.cFileName, "..") == 0)
    {
        if (!FindNextFile(found, &foundFile))
        {
            char buffer[1000];
            strcpy(buffer, "FindNextFile: ");
            strcat(buffer, GetErrorText(GetLastError()));
            HANDLES(FindClose(found));
            RemoveTemporaryDir(tmpDirNameBuf);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
        }
    }
    HANDLES(FindClose(found));

    // and finally we will move it where it belongs
    char* srcName = (char*)malloc(strlen(tmpDirNameBuf) + 1 + strlen(foundFile.cFileName) + 1);
    strcpy(srcName, tmpDirNameBuf);
    strcat(srcName, "\\");
    strcat(srcName, foundFile.cFileName);
    const char* onlyName = strrchr(nameInArchive, '\\');
    if (onlyName == NULL)
        onlyName = nameInArchive;
    char* destName = (char*)malloc(strlen(targetDir) + 1 + strlen(onlyName) + 1);
    strcpy(destName, targetDir);
    strcat(destName, "\\");
    strcat(destName, onlyName);
    if (!SalMoveFile(srcName, destName))
    {
        char buffer[1000];
        strcpy(buffer, "MoveFile: ");
        strcat(buffer, GetErrorText(GetLastError()));
        RemoveTemporaryDir(tmpDirNameBuf);
        free(srcName);
        free(destName);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    // and we will clean up
    free(srcName);
    free(destName);
    RemoveTemporaryDir(tmpDirNameBuf);
    return TRUE;
}
