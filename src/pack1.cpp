// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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

// so that the date error is reported only once
BOOL FirstError;

// Table of archive definitions and handling - non-modifying operations
// !!! WARNING: when changing the order of external archivers you must also change
// the order in the externalArchivers array inside CPlugins::FindViewEdit method
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
            NULL, "--------", 0, 0, 2, "--------", ' ', 1, 2, 6, 5, 7, 3, 2,                              // after RAR 5.0 we patch the indices at runtime; see variable 'RAR5AndLater'
            "$(TargetPath)", "$(Rar32bitExecutable) x -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"", // since version 5.0 we must enforce the -scol switch; version 4.20 is fine; appears elsewhere and in the registry
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
        // UC2 2r3 PRO MS-DOS
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
// Functions
// ****************************************************************************
//

//
// ****************************************************************************
// Functions for listing archives
//

//
// ****************************************************************************
// char *PackGetField(char *buffer, const int index, const int nameidx)
//
//   In the string, buffer finds the item at the given index. Items can be
//   separated by any number of spaces, tabs or newlines.
//   (the vertical bar character (ASCII 0xB3) was added because of ACE)
//   When passing over the item at index nameidx (usually the file name),
//   the only item separator is a newline (as the name can contain spaces or tabs).
//   This function is called from PackScanLine().
//
//   RET: returns a pointer to the given item in buffer string or NULL if it cannot be found
//   IN:  buffer is a line of text for analysis
//        index is the ordinal number of the item to be found
//        nameidx is the index of the "file name" item

char* PackGetField(char* buffer, const int index, const int nameidx, const char separator)
{
    CALL_STACK_MESSAGE5("PackGetField(%s, %d, %d, %u)", buffer, index, nameidx, separator);
    // the requested item does not exist for the given archiver program
    if (index == 0)
        return NULL;

    // indicates the current item we are at
    int i = 1;

    // skip leading spaces and tabs (if there are any)
    while (*buffer != '\0' && (*buffer == ' ' || *buffer == '\t' ||
                               *buffer == 0x10 || *buffer == 0x11 || // arrow characters for ACE
                               *buffer == separator))
        buffer++;

    // find the specified item
    while (index != i)
    {
        // skip the item
        if (i == nameidx)
            // if we are on the name, only a newline is a separator
            while (*buffer != '\0' && *buffer != '\n')
                buffer++;
        else
            // otherwise it is a space, tab, dash or newline
            while (*buffer != '\0' && *buffer != ' ' && *buffer != '\n' &&
                   *buffer != '\t' && *buffer != 0x10 && *buffer != 0x11 &&
                   *buffer != separator)
                buffer++;

        // skip the spaces behind it
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
//   Analyzes one item from the file list output of the archiver program.
//   Usually, it is a single line but it can span multiple connected lines -
//   it must contain all information about a single file stored
//   in the archive. Called from the PackList() function.
//
//   RET: returns TRUE on success, FALSE on error
//        on error the callback function *PackErrorHandlerPtr is called
//   IN:  buffer is the line of text to be analyzed - it is modified during analysis !
//        index is the index in PackTable table corresponding to the given line
//   OUT: CSalamanderDirectory is created and filled with archive data

BOOL PackScanLine(char* buffer, CSalamanderDirectory& dir, const int index,
                  const SPackBrowseTable* configTable, BOOL ARJHack)
{
    CALL_STACK_MESSAGE3("PackScanLine(%s, , %d,)", buffer, index);
    // the file or directory being added
    CFileData newfile;
    int idx;

    // buffer for the file name
    char filename[MAX_PATH];
    char* tmpfname = filename;

    // locate the name in the line
    char* tmpbuf = PackGetField(buffer, configTable->NameIdx,
                                configTable->NameIdx,
                                configTable->Separator);
    // it makes no sense for the name to be missing, but if we let the user
    // tamper with the configuration, it should shout at him
    if (tmpbuf == NULL)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCCFG);

    // remove a leading backslash or slash if present
    if (*tmpbuf == '\\' || *tmpbuf == '/')
        tmpbuf++;
    // copy only the name, replacing slashes with backslashes
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

    // remove any trailing separators from the name
    while (*(tmpfname - 1) == ' ' || *(tmpfname - 1) == '\t' ||
           *(tmpfname - 1) == configTable->Separator)
        tmpfname--;

    // if the processed object is not a directory according to the trailing slash,
    // find the attributes in the line and if any of them is D or d,
    // it is also a directory so append a backslash at the end
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
    // terminate and prepare a pointer to the last non-backslash character
    *tmpfname-- = '\0';
    if (*tmpfname == '\\')
        tmpfname--;

    char* pomptr = tmpfname; // points to the end of the name
    // separate it from the path
    while (pomptr > filename && *pomptr != '\\')
        pomptr--;

    char* pomptr2;
    if (*pomptr == '\\')
    {
        // there is both a name and a path
        *pomptr++ = '\0';
        pomptr2 = filename;
    }
    else
    {
        // only the name is present
        pomptr2 = NULL;
    }

    // pomptr now holds the name of the added directory or file
    // and pomptr2 possibly holds the path to it
    newfile.NameLen = tmpfname - pomptr + 1;

    // set the name of the new file or directory
    newfile.Name = (char*)malloc(newfile.NameLen + 1);
    if (!newfile.Name)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_NOMEM);
    OemToCharBuff(pomptr, newfile.Name, newfile.NameLen); // copy with conversion from OEM to ANSI
    newfile.Name[newfile.NameLen] = '\0';

    // convert the path from OEM to ANSI as well
    if (pomptr2 != NULL)
        OemToChar(pomptr2, pomptr2);

    // set the extension
    char* s = tmpfname - 1;
    while (s >= pomptr && *s != '.')
        s--;
    if (s >= pomptr)
        //  if (s > pomptr)  // ".cvspass" is an extension in Windows...
        newfile.Ext = newfile.Name + (s - pomptr) + 1;
    else
        newfile.Ext = newfile.Name + newfile.NameLen;

    // now load the date and time
    SYSTEMTIME t;
    idx = abs(configTable->DateIdx);
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // the item was not found in the listing, use defaults
    if (tmpbuf == NULL)
    {
        t.wYear = 1980;
        t.wMonth = 1;
        t.wDay = 1;
    }
    else
    {
        // otherwise read all three parts of the date
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

    // ted cas
    idx = configTable->TimeIdx;
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // set to zero, in case we did not read the item (default time)
    t.wHour = 0;
    t.wMinute = 0;
    t.wSecond = 0;
    t.wMilliseconds = 0;
    if (tmpbuf != NULL)
    {
        // item exists, read hours
        while (*tmpbuf >= '0' && *tmpbuf <= '9')
        {
            t.wHour = t.wHour * 10 + (*tmpbuf - '0');
            tmpbuf++;
        }
        // skip one separator character
        tmpbuf++;
        // next digits must be minutes
        while (*tmpbuf >= '0' && *tmpbuf <= '9')
        {
            t.wMinute = t.wMinute * 10 + (*tmpbuf - '0');
            tmpbuf++;
        }
        // is am/pm following?
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
        // if no item separator follows, we can read only seconds
        if (*tmpbuf != '\0' && *tmpbuf != '\n' && *tmpbuf != '\t' &&
            *tmpbuf != ' ')
        {
            // skip the separator
            tmpbuf++;
            // and read seconds
            while (*tmpbuf >= '0' && *tmpbuf <= '9')
            {
                t.wSecond = t.wSecond * 10 + (*tmpbuf - '0');
                tmpbuf++;
            }
        }
        // is am/pm following?
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

    // and store it in the structure
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

    // now read the file size
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
    // and set it in the structure
    newfile.Size.Set((DWORD)(tmpvalue & 0xFFFFFFFF), (DWORD)(tmpvalue >> 32));

    // attributes follow
    idx = configTable->AttrIdx;
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // default is none set
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

    // set the remaining structure items (those not zeroed in AddFile/Dir)
    newfile.DosName = NULL;
    newfile.PluginData = -1; // -1 just for now, ignored

    // and add either a new file or a directory
    if (*(tmpfname + 1) != '\\')
    {
        newfile.IsLink = IsFileLink(newfile.Ext);

        // it is a file, add a file
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
            newfile.Ext = newfile.Name + newfile.NameLen; // directories have no extension
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
//   Function to obtain the contents of an archive.
//
//   RET: returns TRUE on success, FALSE on failure
//        on failure the callback function *PackErrorHandlerPtr is called
//   IN:  panel is Salamander's file panel
//        archiveFileName is the name of the archive file to be listed
//   OUT: dir is filled with archive data
//        pluginData is the interface to column data defined by the archiver plug-in
//        plugin is the plug-in record that performed ListArchive

BOOL PackList(CFilesWindow* panel, const char* archiveFileName, CSalamanderDirectory& dir,
              CPluginDataInterfaceAbstract*& pluginData, CPluginData*& plugin)
{
    CALL_STACK_MESSAGE2("PackList(, %s, , ,)", archiveFileName);
    // clean up just in case
    dir.Clear(NULL);
    pluginData = NULL;
    plugin = NULL;

    FirstError = TRUE;

    // find the correct one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // Supported archive not found - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    int index = PackerFormatConfig.GetUnpackerIndex(format);

    // Is this not internal processing (DLL)?
    if (index < 0)
    {
        plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelView)
        {
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->ListArchive(panel, archiveFileName, dir, pluginData);
    }

    // if we have not determined the spawn path yet, do it now
    if (!InitSpawnName(NULL))
        return FALSE;

    //
    // We will run an external program with redirected output
    //
    const SPackBrowseTable* browseTable = ArchiverConfig.GetUnpackerConfigTable(index);

    // build the current directory
    char currentDir[MAX_PATH];
    if (!PackExpandInitDir(archiveFileName, NULL, NULL, browseTable->ListInitDir,
                           currentDir, MAX_PATH))
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_IDIRERR);

    // build the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    sprintf(cmdLine, "\"%s\" %s ", SpawnExe, SPAWN_EXE_PARAMS);
    int cmdIndex = (int)strlen(cmdLine);
    if (!PackExpandCmdLine(archiveFileName, NULL, NULL, NULL, browseTable->ListCommand,
                           cmdLine + cmdIndex, PACK_CMDLINE_MAXLEN - cmdIndex, NULL))
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNERR);

    char cmdForErrors[MAX_PATH]; // path and name of executed exe if an error occurs
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

    // check whether the command line is too long
    if (!browseTable->SupportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // we must inherit handles
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
    // so that we can use it as stderr as well
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

    // create structures for the new process
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = NULL;
    si.hStdOutput = StdOutWr;
    si.hStdError = StdErrWr;
    // and start it ...
    if (!HANDLES(CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NEW_CONSOLE | CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                               NULL, currentDir, &si, &pi)))
    {
        // if this failed, we have a bad path to salspawn
        DWORD err = GetLastError();
        HANDLES(CloseHandle(StdOutRd));
        HANDLES(CloseHandle(StdOutWr));
        HANDLES(CloseHandle(StdErrWr));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PROCESS, SpawnExe, GetErrorText(err));
    }

    // We no longer need these handles; the child will close the duplicates, we keep only StdOutRd
    HANDLES(CloseHandle(StdOutWr));
    HANDLES(CloseHandle(StdErrWr));

    // Pull all data from the pipe into an array of lines
    char tmpbuff[1000];
    DWORD read;
    CPackLineArray lineArray(1000, 500);
    int buffOffset = 0;
    while (1)
    {
        // read a full buffer from the end of the unprocessed data
        if (!ReadFile(StdOutRd, tmpbuff + buffOffset, 1000 - buffOffset, &read, NULL))
            break;
        // start at the beginning
        char* start = tmpbuff;
        // search the entire buffer for line ends
        unsigned int i;
        for (i = 0; i < read + buffOffset; i++)
        {
            if (tmpbuff[i] == '\n')
            {
                // length of the line
                int lineLen = (int)(tmpbuff + i - start);
                // remove \r, if it is present
                if (lineLen > 0 && tmpbuff[i - 1] == '\r')
                    lineLen--;
                // allocate a new line
                char* newLine = new char[lineLen + 2];
                // fill it with data and terminate it
                strncpy(newLine, start, lineLen);
                newLine[lineLen] = '\n';
                newLine[lineLen + 1] = '\0';
                // add it to the array
                lineArray.Add(newLine);
                // and move past it
                start = tmpbuff + i + 1;
            }
        }
        // buffer processed; now find out how much is left and move it to the start of the buffer
        buffOffset = (int)(tmpbuff + read + buffOffset - start);
        if (buffOffset > 0)
            memmove(tmpbuff, start, buffOffset);
        // maximum line length is 990, hopefully that is enough :-)
        if (buffOffset >= 990)
        {
            HANDLES(CloseHandle(StdOutRd));
            HANDLES(CloseHandle(pi.hProcess));
            HANDLES(CloseHandle(pi.hThread));
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
        }
    }

    // done reading, we no longer need it
    HANDLES(CloseHandle(StdOutRd));

    // Wait for the external program to finish (it should be done already but better be sure)
    if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
    {
        char buffer[1000];
        strcpy(buffer, "WaitForSingleObject: ");
        strcat(buffer, GetErrorText(GetLastError()));
        HANDLES(CloseHandle(pi.hProcess));
        HANDLES(CloseHandle(pi.hThread));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    // Restore focus back to us
    SetForegroundWindow(MainWindow->HWindow);

    // find out how it ended - hopefully all return 0 as success
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

    // release the process handles
    HANDLES(CloseHandle(pi.hProcess));
    HANDLES(CloseHandle(pi.hThread));

    if (exitCode != 0)
    {
        //
        // First handle salspawn.exe errors
        //
        if (exitCode >= SPAWN_ERR_BASE)
        {
            // salspawn.exe error - wrong parameters or such
            if (exitCode >= SPAWN_ERR_BASE && exitCode < SPAWN_ERR_BASE * 2)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, SPAWN_EXE_NAME, LoadStr(IDS_PACKRET_SPAWN));
            // CreateProcess error
            if (exitCode >= SPAWN_ERR_BASE * 2 && exitCode < SPAWN_ERR_BASE * 3)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PROCESS, cmdForErrors, GetErrorText(exitCode - SPAWN_ERR_BASE * 2));
            // WaitForSingleObject error
            if (exitCode >= SPAWN_ERR_BASE * 3 && exitCode < SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "WaitForSingleObject: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 3));
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
            // GetExitCodeProcess error
            if (exitCode >= SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "GetExitCodeProcess: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 4));
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
        }
        //
        // now handle errors of the external program
        //
        // if errorTable == NULL, do not translate (no table exists)
        if (!browseTable->ErrorTable)
        {
            char buffer[1000];
            sprintf(buffer, LoadStr(IDS_PACKRET_GENERAL), exitCode);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, cmdForErrors, buffer);
        }
        // find the appropriate text in the table
        int i;
        for (i = 0; (*browseTable->ErrorTable)[i][0] != -1 &&
                    (*browseTable->ErrorTable)[i][0] != (int)exitCode;
             i++)
            ;
        // did we find it?
        if ((*browseTable->ErrorTable)[i][0] == -1)
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, cmdForErrors, LoadStr(IDS_PACKRET_UNKNOWN));
        else
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, cmdForErrors,
                                          LoadStr((*browseTable->ErrorTable)[i][1]));
    }

    //
    // now the main part - parsing the packer`s output into our structures
    //
    if (lineArray.Count == 0)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_NOOUTPUT);

    // if we must use a special parsing function, do it right now
    if (browseTable->SpecialList)
        return (*(browseTable->SpecialList))(archiveFileName, lineArray, dir);

    // now we parse with the universal parser

    // a few local variables
    char* line = NULL; // buffer for building a "multi-line line"
    int lines = 0;     // how many lines of the multi-line item we have read
    int validData = 0; // whether we read header/footer or valid data
    int toSkip = browseTable->LinesToSkip;
    int alwaysSkip = browseTable->AlwaysSkip;
    int linesPerFile = browseTable->LinesPerFile;
    BOOL ARJHack;
    BOOL RAR5AndLater = FALSE; // starting with RAR 5.0 the listing format is new (commands 'v' and 'l'), the name is in the last column

    int i;
    for (i = 0; i < lineArray.Count; i++)
    {
        // determine what to do with this data
        switch (validData)
        {
        case 0: // we are in the header
            // determine whether we stay in it
            if (!strncmp(lineArray[i], browseTable->StartString,
                         strlen(browseTable->StartString)))
                validData++;
            if (i == 1 && strncmp(lineArray[i], "RAR ", 4) == 0 && lineArray[i][4] >= '5' && lineArray[i][4] <= '9')
            {
                RAR5AndLater = TRUE; // the test fails starting with RAR 10, which will be useful ;-)
                linesPerFile = 1;
            }
            // in any case this line does not interest us
            continue;
        case 2: // we are in the footer and we do not care about it
            continue;
        case 1: // we are in the data - just check if it ends and then work
            // if we still need to skip something, do it now
            if (alwaysSkip > 0)
            {
                alwaysSkip--;
                continue;
            }
            if (!strncmp(lineArray[i], browseTable->StopString,
                         strlen(browseTable->StopString)))
            {
                validData++;
                // maybe some leftovers from the previous line remain
                if (line)
                    free(line);
                continue;
            }
        }

        // if we still have something to skip, do it now
        if (toSkip > 0)
        {
            toSkip--;
            continue;
        }

        // we have another line
        lines++;
        // if this is the first line, we must allocate a buffer
        if (lines == 1)
        {
            // determine whether we are dealing with two or four lines (ARJ32 hack)
            if (browseTable->LinesPerFile == 0)
                if (i + 3 < lineArray.Count && lineArray[i + 2][3] == ' ')
                    linesPerFile = 4;
                else
                    linesPerFile = 2;
            // determine whether the OS type is missing (ARJ16 hack)
            ARJHack = FALSE;
            if (browseTable->DateIdx < 0)
                if (lineArray[i + 1][5] == ' ')
                    ARJHack = TRUE;
            // do we even have that many lines?
            if (i + linesPerFile - 1 >= lineArray.Count)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // determine the resulting length
            int len = 0;
            int j;
            for (j = 0; j < linesPerFile; j++)
                len = len + (int)strlen(lineArray[i + j]);
            // allocate a buffer for it
            line = (char*)malloc(len + 1);
            if (!line)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_NOMEM);
            // and initialize it
            line[0] = '\0';
        }
        // if this is not the last line just append it to the buffer
        if (lines < linesPerFile)
        {
            strcat(line, lineArray[i]);
            continue;
        }
        // if it is the last line, append it to the buffer
        strcat(line, lineArray[i]);

        // and we have everything for one item - process it
        SPackBrowseTable browseTableRAR5;
        if (RAR5AndLater)
        {
            memmove(&browseTableRAR5, browseTable, sizeof(SPackBrowseTable));
            browseTableRAR5.NameIdx = 8;
            browseTableRAR5.AttrIdx = 1;
        }
        BOOL ret = PackScanLine(line, dir, index, RAR5AndLater ? &browseTableRAR5 : browseTable, ARJHack);
        // we no longer need the buffer
        free(line);
        line = NULL;
        if (!ret)
            return FALSE; // no need to call the error function, PackScanLine already did the call

        // initialize variables
        lines = 0;
    }

    // if we ended somewhere else than in the footer, we have a problem
    if (validData < 2)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);

    return TRUE;
}

//
// ****************************************************************************
// BOOL PackUC2List(const char *archiveFileName, CPackLineArray &lineArray,
//                  CSalamanderDirectory &dir)
//
//   Function for retrieving archive contents for the UC2 format (parser only)
//
//   RET: returns TRUE on success, FALSE on error
//        on error the callback function *PackErrorHandlerPtr is called
//   IN:  archiveFileName is the archive name we work with
//        lineArray is the array of lines from the archiver output
//   OUT: dir is created and filled with archive data

BOOL PackUC2List(const char* archiveFileName, CPackLineArray& lineArray,
                 CSalamanderDirectory& dir)
{
    CALL_STACK_MESSAGE2("PackUC2List(%s, ,)", archiveFileName);
    // First delete the helper file that UC2 creates when using the ~D flag
    char arcPath[MAX_PATH];
    const char* arcName = strrchr(archiveFileName, '\\') + 1;
    strncpy(arcPath, archiveFileName, arcName - archiveFileName);
    arcPath[arcName - archiveFileName] = '\0';
    strcat(arcPath, "U$~RESLT.OK");
    DeleteFile(arcPath);

    char* txtPtr;         // pointer to the current position in the read line
    char currentDir[256]; // current directory we are exploring
    int line = 0;         // index into the line array
    // a bit redundant check but better be safe
    if (lineArray.Count < 1)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);

    // main parsing loop
    while (1)
    {
        // added file or directory
        CFileData newfile;

        // skip leading spaces
        for (txtPtr = lineArray[line]; *txtPtr == ' '; txtPtr++)
            ;

        // if the item is END, we are done
        if (!strncmp(txtPtr, "END", 3))
            break;

        // if the item is LIST, we determine which directory we are in
        if (!strncmp(txtPtr, "LIST", 4))
        {
            // run to the start of the name
            while (*txtPtr != '\0' && *txtPtr != '[')
                txtPtr++;
            // error check - should not happen
            if (*txtPtr == '\0')
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // and to the first letter of the name
            txtPtr++;
            int i = 0;
            // skip leading backslashes
            while (*txtPtr == '\\')
                txtPtr++;
            // copy the name into the variable
            while (*txtPtr != '\0' && *txtPtr != ']')
                currentDir[i++] = *txtPtr++;
            // terminate the string
            currentDir[i] = '\0';
            OemToChar(currentDir, currentDir);
            // one more check
            if (*txtPtr == '\0')
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // prepare the next line
            if (++line > lineArray.Count - 1)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // and go for another round
            continue;
        }

        // if the item is FILE/DIR, we create a file/directory
        if (!strncmp(txtPtr, "DIR", 3) || !strncmp(txtPtr, "FILE", 4))
        {
            // what is it, a file or a directory?
            BOOL isDir = TRUE;
            if (!strncmp(txtPtr, "FILE", 4))
                isDir = FALSE;

            // prepare some default values
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
            newfile.PluginData = -1; // just -1, ignored

            // main parsing loop of a file/directory
            // ends once we hit an unknown keyword
            while (1)
            {
                // prepare the next line
                if (++line > lineArray.Count - 1)
                    return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);

                // skip leading spaces
                for (txtPtr = lineArray[line]; *txtPtr == ' '; txtPtr++)
                    ;

                // is it a name?
                if (!strncmp(txtPtr, "NAME=", 5))
                {
                    // move to the start of the name
                    while (*txtPtr != '\0' && *txtPtr != '[')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    int i = 0;
                    // copy the name to the newName variable
                    char newName[15];
                    while (*txtPtr != '\0' && *txtPtr != ']')
                        newName[i++] = *txtPtr++;
                    newName[i] = '\0';
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    // and store it in the structure
                    newfile.NameLen = strlen(newName);
                    newfile.Name = (char*)malloc(newfile.NameLen + 1);
                    if (!newfile.Name)
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    OemToChar(newName, newfile.Name);
                    newfile.Ext = strrchr(newfile.Name, '.');
                    if (newfile.Ext != NULL) // ".cvspass" is an extension in Windows ...
                                             //          if (newfile.Ext != NULL && newfile.Name != newfile.Ext)
                        newfile.Ext++;
                    else
                        newfile.Ext = newfile.Name + newfile.NameLen;
                    // and go another round
                    continue;
                }
                // or is it a date?
                if (!strncmp(txtPtr, "DATE(MDY)=", 10))
                {
                    // reset it
                    t.wYear = 0;
                    t.wMonth = 0;
                    t.wDay = 0;
                    // go to the start of the data
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

                    // conversion just in case (should not be needed)
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

                    // and again ...
                    continue;
                }
                // it could also be the last modification time
                if (!strncmp(txtPtr, "TIME(HMS)=", 10))
                {
                    // reset again
                    t.wHour = 0;
                    t.wMinute = 0;
                    t.wSecond = 0;
                    // start of the data
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // read the hour
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wHour = t.wHour * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // read the minute
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wMinute = t.wMinute * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // read the second
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wSecond = t.wSecond * 10 + *txtPtr++ - '0';

                    // and again ...
                    continue;
                }
                // attributes remain...
                if (!strncmp(txtPtr, "ATTRIB=", 7))
                {
                    // start of the data
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // clear first
                    newfile.Attr = 0;
                    newfile.Hidden = 0;
                    // and set what is needed
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
                    // and again at it ...
                    continue;
                }
                // and finally the size
                if (!strncmp(txtPtr, "SIZE=", 5))
                {
                    // again skip the unimportant stuff
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // we need a helper variable
                    unsigned __int64 tmpvalue = 0;
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        tmpvalue = tmpvalue * 10 + *txtPtr++ - '0';
                    // and store it in the structure
                    newfile.Size.Set((DWORD)(tmpvalue & 0xFFFFFFFF), (DWORD)(tmpvalue >> 32));
                    // and off to the next line
                    continue;
                }
                // dummy values - we must know them but can ignore them
                if (!strncmp(txtPtr, "VERSION=", 8))
                    continue;
                if (!strncmp(txtPtr, "CHECK=", 6))
                    continue;

                // unknown item - end of the section
                break;
            }
            //
            // we have everything, create the object
            //

            // store in the structure what is not there yet
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
            // and finally just create a new object
            newfile.IsOffline = 0;
            if (isDir)
            {
                // if it is a directory, handle it here
                newfile.Attr |= FILE_ATTRIBUTE_DIRECTORY;
                if (!Configuration.SortDirsByExt)
                    newfile.Ext = newfile.Name + newfile.NameLen; // directories have no extensions
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

                // if it is a file, go this way
                if (!dir.AddFile(currentDir, newfile, NULL))
                {
                    free(newfile.Name);
                    return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_FDATA);
                }
            }
            // and off to the next round
            continue;
        }
    }
    return TRUE;
}

//
// ****************************************************************************
// Decompression functions
//

//
// ****************************************************************************
// BOOL PackUncompress(HWND parent, CFilesWindow *panel, const char *archiveFileName,
//                     CPluginDataInterfaceAbstract *pluginData,
//                     const char *targetDir, const char *archiveRoot,
//                     SalEnumSelection nextName, void *param)
//
//   Function for extracting requested files from an archive.
//
//   RET: returns TRUE on success, FALSE on error
//        on error the callback function *PackErrorHandlerPtr is called
//   IN:  parent is the parent window for message boxes
//        panel is a pointer to the Salamander file panel
//        archiveFileName is name of the archive we extract from
//        targetDir is the path where files are extracted
//        archiveRoot is the directory in the archive we extract from
//        nextName is the callback function for enumerating names to extract
//        param are the parameters for the enumeration function
//        pluginData is the interface for working with file/directory data specific to the plugin
//   OUT:

BOOL PackUncompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* targetDir, const char* archiveRoot,
                    SalEnumSelection nextName, void* param)
{
    CALL_STACK_MESSAGE4("PackUncompress(, , %s, , %s, %s, ,)", archiveFileName, targetDir, archiveRoot);
    // find the correct one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // Supported archive not found - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    int index = PackerFormatConfig.GetUnpackerIndex(format);

    // Is this not internal processing (DLL)?
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
//   Function for extracting requested files from an archive. Unlike the previous
//   one it is more general, does not use configuration tables and can be called
//   standalone; everything is determined only by parameters
//
//   RET: returns TRUE on success, FALSE on error
//        on error the callback function *PackErrorHandlerPtr is called
//   IN:  parent is the parent for message boxes
//        command is the command line used for extraction
//        errorTable is a pointer to the return codes table or NULL if none exists
//        initDir is the directory in which the archiver should run
//        panel is a pointer to the Salamander file panel
//        supportLongNames indicates whether the archiver supports long names
//        archiveFileName is the name of the archive we unpack
//        targetDir is the path where files are extracted
//        archiveRoot is the path in the archive from which we are extracting, or NULL
//        nextName is the callback function enumerating files to extract
//        param are the parameters passed to the callback function
//        needANSIListFile is TRUE if the file list must be in ANSI (not OEM)
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
            strcat(rootPath, "\\");
        }
    }
    else
    {
        rootPath[0] = '\0';
    }

    //
    // Now it's time for the temporary extraction directory

    // Create the name of the temporary directory
    char tmpDirNameBuf[MAX_PATH];
    if (!SalGetTempFileName(targetDir, "PACK", tmpDirNameBuf, FALSE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    //
    // Now prepare a helper file with a list of files to extract in the %TEMP% directory
    //

    // Create the name of the temporary file
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
        RemoveDirectory(tmpDirNameBuf);
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
    }

    // and we can fill it
    BOOL isDir;
    CQuadWord size;
    const char* name;
    char namecnv[MAX_PATH];
    CQuadWord totalSize(0, 0);
    int errorOccured;

    if (!needANSIListFile)
        CharToOem(rootPath, rootPath);
    // pick the name
    while ((name = nextName(parent, 1, &isDir, &size, NULL, param, &errorOccured)) != NULL)
    {
        if (!needANSIListFile)
            CharToOem(name, namecnv);
        else
            strcpy(namecnv, name);
        // sum the total size
        totalSize += size;
        // put the name into the list
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
    // and that is it
    fclose(listFile);

    // if an error occurred and the user chose to cancel the operation, end the it +
    // also test for enough free disk space
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
    // build the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    if (!PackExpandCmdLine(archiveFileName, tmpDirNameBuf, tmpListNameBuf, NULL,
                           command, cmdLine, PACK_CMDLINE_MAXLEN, NULL))
    {
        DeleteFile(tmpListNameBuf);
        RemoveDirectory(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNERR);
    }

    // check if the command line is too long
    if (!supportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        DeleteFile(tmpListNameBuf);
        RemoveDirectory(tmpDirNameBuf);
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // build the current directory
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

    // and run the external program
    if (!PackExecute(NULL, cmdLine, currentDir, errorTable))
    {
        DeleteFile(tmpListNameBuf);
        RemoveTemporaryDir(tmpDirNameBuf);
        return FALSE; // the error message has already been shown
    }
    // the file list is no longer needed
    DeleteFile(tmpListNameBuf);

    // and now finally move the files where they belong
    char srcDir[MAX_PATH];
    strcpy(srcDir, tmpDirNameBuf);
    if (*rootPath != '\0')
    {
        // locate the extracted subdirectory path - names of subdirectories may not match
        // because of the Czech characters and long names :-(
        char* r = rootPath;
        WIN32_FIND_DATA foundFile;
        char buffer[1000];
        while (1)
        {
            if (*r == 0)
                break;
            while (*r != 0 && *r != '\\')
                r++; // skip one level in the original rootPath
            while (*r == '\\')
                r++; // skip the backslash in the original rootPath
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
                   strcmp(foundFile.cFileName, ".") == 0 || // we ignore "." and ".."
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
            { // attach another subdirectory on the path
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
//   Callback function for enumerating given masks
//
//   RET: Returns the mask currently processed or NULL when it is done
//   IN:  enumFiles is ignored
//        fileData is ignored (only initialized to NULL)
//        param is a pointer to a string of file masks separated by semicolon
//   OUT: isDir is always FALSE
//        size is always 0
//        the string of masks separated by semicolon is modified (";"->"\0" when separating masks, ";;"->";" with the trailing ";" removed)

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

    // if there are no more masks return NULL - finished
    if (param == NULL || *(char**)param == NULL)
        return NULL;

    // drop a possible semicolon at the end of the string (shorten the string)
    char* ptr = *(char**)param + strlen(*(char**)param) - 1;
    char* endPtr = ptr;
    while (1)
    {
        while (ptr >= *(char**)param && *ptr == ';')
            ptr--;
        if (((endPtr - ptr) & 1) == 1)
            *endPtr-- = 0; // ignore ';' only if odd (even count will later be converted ";;"->";")
        if (endPtr >= *(char**)param && *endPtr <= ' ')
        {
            endPtr--;
            while (endPtr >= *(char**)param && *endPtr <= ' ')
                endPtr--; // ignore trailing white - spaces
            *(endPtr + 1) = 0;
            ptr = endPtr; // must try again to see if an odd ';' has to be skipped
        }
        else
            break;
    }
    // no mask left, there are only semicolons and white spaces (or nothing :-))
    if (endPtr < *(char**)param)
        return NULL;

    // otherwise find the last semicolon (only if their count is odd, otherwise ";;" will be replaced with ";") - before the last mask
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
        // if there is no semicolon left we have only one
        *(char**)param = NULL;
    }
    else
    {
        // cut off the last mask from the rest by placing zero instead of the found semicolon
        *ptr = '\0';
    }

    while (*(ptr + 1) != 0 && *(ptr + 1) <= ' ')
        ptr++; // skip white - spaces at the beginning of the mask
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
//   Function for extracting a single file from an archive (for the viewer).
//
//   RET: returns TRUE on success, FALSE on error
//        on error the callback function *PackErrorHandlerPtr is called
//   IN:  panel is the Salamander file panel
//        archiveFileName is the name of the archive from which we extract
//        nameInArchive is the name of the file we extract
//        fileData is a pointer to the CFileData structure of the extracted file
//        targetDir is the path where the file should be extracted
//        newFileName (if not NULL) is the new name of the extracted file (during extraction, the file
//          must be renamed from its original name to this new one)
//        renamingNotSupported (only if newFileName is not NULL) - set TRUE if the plugin
//          does not support renaming during extraction, Salamander will show an error
//   OUT:

BOOL PackUnpackOneFile(CFilesWindow* panel, const char* archiveFileName,
                       CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                       CFileData* fileData, const char* targetDir, const char* newFileName,
                       BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE5("PackUnpackOneFile(, %s, , %s, , %s, %s, )",
                        archiveFileName, nameInArchive, targetDir, newFileName);

    // find the correct one according to the table
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // No supported archive found - error
    if (format == 0)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    int index = PackerFormatConfig.GetUnpackerIndex(format);

    // Is this not internal processing (DLL)?
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

    if (newFileName != NULL) // external archivers do not support renaming yet (we did not even check if they can)
    {
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_INVALIDNAME);
    }

    //
    // Create a temporary directory into which we unpack the file
    //

    // buffer for the full name of the temporary directory
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

    // build the command line
    char cmdLine[PACK_CMDLINE_MAXLEN];
    if (!PackExpandCmdLine(archiveFileName, tmpDirNameBuf, NULL, nameInArchive,
                           browseTable->ExtractCommand, cmdLine, PACK_CMDLINE_MAXLEN, NULL))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNERR);
    }

    // check whether the command line is too long
    if (!browseTable->SupportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        RemoveTemporaryDir(tmpDirNameBuf);
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // build the current directory
    char currentDir[MAX_PATH];
    if (!PackExpandInitDir(archiveFileName, NULL, tmpDirNameBuf, browseTable->ExtractInitDir,
                           currentDir, MAX_PATH))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_IDIRERR);
    }

    // and run the external program
    if (!PackExecute(NULL, cmdLine, currentDir, browseTable->ErrorTable))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return FALSE; // the error message has already been shown
    }

    // find the extracted file - the name may not match due to the Czech characters and long names :-(
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

    // and finally move it where it belongs
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

    // and clean up
    free(srcName);
    free(destName);
    RemoveTemporaryDir(tmpDirNameBuf);
    return TRUE;
}
