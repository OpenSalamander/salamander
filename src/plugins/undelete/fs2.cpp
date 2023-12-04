// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "undelete.rh"
#include "undelete.rh2"
#include "lang\lang.rh"

#include "library\undelete.h"
#include "miscstr.h"
#include "os.h"
#include "volume.h"
#include "snapshot.h"
#include "dataruns.h"
#include "stream.h"
#include "dialogs.h"
#include "undelete.h"
#include "bitmap.h"
#include "dataruns.h"
#include "ntfs.h"
#include "fat.h"
#include "exfat.h"

#define COPY_BUFFER (256 * 1024)

DWORD WINAPI
CPluginFSInterface::GetSupportedServices()
{
    return FS_SERVICE_CONTEXTMENU |
           //FS_SERVICE_SHOWPROPERTIES |
           //FS_SERVICE_CHANGEATTRS |
           //FS_SERVICE_COPYFROMDISKTOFS |
           //FS_SERVICE_MOVEFROMDISKTOFS |
           //FS_SERVICE_MOVEFROMFS |
           FS_SERVICE_COPYFROMFS |
           //FS_SERVICE_DELETE |
           FS_SERVICE_VIEWFILE |
           //FS_SERVICE_CREATEDIR |
           //FS_SERVICE_ACCEPTSCHANGENOTIF |
           //FS_SERVICE_QUICKRENAME |
           //FS_SERVICE_COMMANDLINE |
           //FS_SERVICE_SHOWINFO |
           //FS_SERVICE_GETFREESPACE |
           FS_SERVICE_GETFSICON |
           FS_SERVICE_GETNEXTDIRLINEHOTPATH;
    //FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM |
    //FS_SERVICE_GETPATHFORMAINWNDTITLE;
}

// ****************************************************************************
//
//  ChangePath, ListCurrentPath, and other stuff
//

CPluginFSInterface::CPluginFSInterface()
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::CPluginFSInterface()");
    Path[0] = 0;
    Root[0] = 0;
    CurrentDir = NULL;
    FatalError = FALSE;
    IsSnapshotValid = FALSE;
    Snapshot = NULL;
    SnapshotState = NULL;
    Flags = 0;
    if (ConfigScanVacantClusters)
        Flags |= UF_SCANVACANTCLUSTERS;
    if (ConfigShowExistingFiles)
        Flags |= UF_SHOWEXISTING;
    if (ConfigShowZeroFiles)
        Flags |= UF_SHOWZEROFILES;
    if (ConfigShowEmptyDirs)
        Flags |= UF_SHOWEMPTYDIRS;
    if (ConfigShowMetafiles)
        Flags |= UF_SHOWMETAFILES;
    if (ConfigEstimateDamage)
        Flags |= UF_ESTIMATEDAMAGE;
}

void WINAPI
CPluginFSInterface::ReleaseObject(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::ReleaseObject()");
    if (Snapshot != NULL)
    {
        Snapshot->Free();
        delete Snapshot;
        Snapshot = NULL;
        IsSnapshotValid = FALSE;
        SnapshotState->Invalidate();
        SnapshotState->Release();
        SnapshotState = NULL;
    }
    if (Volume.IsOpen())
        Volume.Close();
}

BOOL CPluginFSInterface::RootPathFromFull(const char* pluginFullPath, char* rootPath, size_t bufferSize)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::SplitPluginPath(%s, , )", pluginFullPath);

    strncpy_s(rootPath, bufferSize, pluginFullPath, _TRUNCATE);

    // try looking for image file
    while (1)
    {
        DWORD attribs = SalamanderGeneral->SalGetFileAttributes(rootPath);
        if (INVALID_FILE_ATTRIBUTES != attribs &&
            !(attribs & FILE_ATTRIBUTE_DIRECTORY))
            // it's an existing file, use it as an image
            return TRUE;

        // strip one more part from the path
        char* sep = strrchr(rootPath, '\\');
        if (!sep)
            break;
        *sep = 0;
    }

    // now determine volume root
    BOOL ret = GetVolumePathName(pluginFullPath, rootPath, (DWORD)bufferSize);
    if (ret)
    {
        size_t len = strlen(rootPath);
        if ('\\' != rootPath[len - 1])
        {
            rootPath[len - 1] = '\\';
            rootPath[len] = 0;
        }
    }
    return ret;
}

BOOL WINAPI
CPluginFSInterface::GetRootPath(char* userPart)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::GetRootPath()");
    lstrcpy(userPart, Root);
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::GetCurrentPath(char* userPart)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::GetCurrentPath()");
    strcpy(userPart, Path);
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::GetFullName(CFileData& file, int isDir, char* buf, int bufSize)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::GetFullName(, %d, , %d)", isDir, bufSize);
    lstrcpyn(buf, Path, bufSize); // if path doesn't fit, name doesn't fit too (report error)
    if (isDir == 2)
        return SalamanderGeneral->CutDirectory(buf, NULL); // up-dir
    else
        return SalamanderGeneral->SalPathAppend(buf, file.Name, bufSize);
}

BOOL WINAPI
CPluginFSInterface::GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success)
{
    return FALSE;
}

BOOL WINAPI
CPluginFSInterface::IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::IsCurrentPath(%d, %d, %s)",
                        currentFSNameIndex, fsNameIndex, userPart);
    return currentFSNameIndex == fsNameIndex && SalamanderGeneral->IsTheSamePath(Path, userPart);
}

extern BOOL WantReconnect; // defined in fs1.cpp

BOOL WINAPI
CPluginFSInterface::IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::IsOurPath(%d, %d, %s)",
                        currentFSNameIndex, fsNameIndex, userPart);
    if (WantReconnect)
        return FALSE; // user wants new connection
    char buffer[MAX_PATH];
    if (!RootPathFromFull(userPart, buffer, MAX_PATH))
        return FALSE;
    if (SalamanderGeneral->IsTheSamePath(Root, buffer))
        return TRUE;
    return FALSE;
}

// function for comparing file names (initial '$' equals to 0xE5)
int namecmp(char* name1, char* name2)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("namecmp(, )");
    if ((name1[0] == '$' && name2[0] == 0xE5) || (name1[0] == 0xE5 && name2[0] == '$'))
        return _stricmp(name1 + 1, name2 + 1);
    return _stricmp(name1, name2);
}

void CPluginFSInterface::Replace0xE5(char* filename)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CPluginFSInterface::Replace0xE5()");
    if (filename[0] == 0xE5)
        filename[0] = '$';
    else if (filename[0] == 0x05)
        filename[0] = 0xE5; // there is E5 support in Kanji
}

#define PATHSEP "\\/"

HWND HProgressDlg = NULL;

BOOL WINAPI
CPluginFSInterface::ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                               const char* userPart, char* cutFileName, BOOL* pathWasCut,
                               BOOL forceRefresh, int mode)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::ChangePath(%d, , %d, %s, , , %d, %d)",
                        currentFSNameIndex, fsNameIndex, userPart, forceRefresh,
                        mode);

    if (forceRefresh)
        IsSnapshotValid = FALSE;

    // tests and inits
    if (mode != 3 && (pathWasCut != NULL || cutFileName != NULL))
    {
        TRACE_E("Incorrect value of 'mode' in CPluginFSInterface::ChangePath().");
        mode = 3;
    }
    if (pathWasCut != NULL)
        *pathWasCut = FALSE;
    if (cutFileName != NULL)
        *cutFileName = 0;
    if (FatalError)
    {
        FatalError = FALSE;
        return FALSE;
    }

    // determine the device to open
    if (!RootPathFromFull(userPart, Root, MAX_PATH))
        return String<char>::Error(IDS_UNDELETE, IDS_PATHDISK);
    strcpy(Path, Root);
    CurrentDir = NULL;

    // read snapshot
    if (!IsSnapshotValid)
    {

        BOOL ret = TRUE;
        HWND mainwnd = SalamanderGeneral->GetMainWindowHWND();
        CSnapshotProgressDlg dlg(mainwnd, ooStatic);
        if (dlg.Create() == NULL)
            return String<char>::SysError(IDS_UNDELETE, IDS_ERROROPENINGPROGRESS);
        EnableWindow(mainwnd, FALSE);
        HProgressDlg = dlg.HWindow; // redirect message box parent
        SetForegroundWindow(dlg.HWindow);

        if (!Volume.IsOpen())
        {
            dlg.SetProgressText(IDS_OPENINGVOLUME);
            ret = Volume.Open(Root);
        }

#ifdef TRACE_ENABLE
        DWORD time = GetTickCount();
#endif

        if (ret)
        {
            if (Snapshot == NULL)
            {
                switch (Volume.Type)
                {
                case vtNTFS:
                    Snapshot = new CMFTSnapshot<char>(&Volume);
                    break;
                case vtFAT:
                    Snapshot = new CFATSnapshot<char>(&Volume);
                    break;
                case vtExFAT:
                    Snapshot = new CExFATSnapshot<char>(&Volume);
                    break;
                default:
                    ret = FALSE;
                }
            }
            if (ret)
            {
                if (Snapshot == NULL)
                    ret = String<char>::Error(IDS_UNDELETE, IDS_LOWMEM);
                else
                {
                    // invalidate original state of the snapshot and release it, calling Update() will destroy snapshot data,
                    // plugin must not touch it anymore (from listing through CFileData::PluginData)
                    if (SnapshotState != NULL)
                    {
                        SnapshotState->Invalidate();
                        SnapshotState->Release();
                    }
                    SnapshotState = new CSnapshotState(); // create a new snapshot state (after Update()), new will be valid
                    SnapshotState->AddRef();
                    ret = Snapshot->Update(&dlg, Flags, NULL);
                }
            }
        }
#ifdef TRACE_ENABLE
        time = GetTickCount() - time;
        TRACE_I("Snapshot load time: " << time << " ms");
#endif

        EnableWindow(mainwnd, TRUE);
        HProgressDlg = NULL; // reset message box parent
        DestroyWindow(dlg.HWindow);
        if (!ret)
            return FALSE;
        IsSnapshotValid = TRUE;
    }

    // walk through all path components, also moving in directories structure
    CurrentDir = Snapshot->Root;
    // we will cut path to individual parts
    char temp[MAX_PATH];
    if (strlen(userPart) > strlen(Root))
        lstrcpyn(temp, userPart + strlen(Root), MAX_PATH);
    else
        temp[0] = 0;
    char* component = strtok(temp, PATHSEP);
    while (component != NULL)
    {
        // is on current path object with given name?
        int numitems = CurrentDir->NumDirItems;
        int i;
        for (i = 0; i < numitems; i++)
        {
            DIR_ITEM_I<char>* di = &CurrentDir->DirItems[i];
            if (!namecmp(di->FileName->FNName, component))
            {
                if (di->Record->IsDir) // yes, it is direcotry
                {
                    SalamanderGeneral->SalPathAppend(Path, component, MAX_PATH);
                    CurrentDir = di->Record;
                }
                else // it is file
                {
                    if (cutFileName != NULL)
                        lstrcpyn(cutFileName, di->FileName->FNName, MAX_PATH);
                    if (mode < 3)
                        return !String<char>::Error(IDS_UNDELETE, IDS_PATHFILE);
                }
                break;
            }
        }

        if (i >= numitems) // not found
        {
            if (pathWasCut != NULL)
                *pathWasCut = TRUE;
            // originally here was mode > 1 but I received messages when connecting from directory
            // where are no deleted files (so path doesn't point there)
            if (mode == 3)
                String<char>::Error(IDS_UNDELETE, IDS_PATHNOTFOUND);
            return mode != 3;
        }

        // nect component
        component = strtok(NULL, PATHSEP);
    }

    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                    CPluginDataInterfaceAbstract*& pluginData,
                                    int& iconsType, BOOL forceRefresh)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::ListCurrentPath(, , %d, %d)",
                        iconsType, forceRefresh);

    iconsType = pitFromRegistry;
    if (CurrentDir == NULL || Snapshot == NULL || !IsSnapshotValid)
    {
        FatalError = TRUE;
        return FALSE;
    }

    // set valid 'fd' data members
    dir->SetValidData(VALID_DATA_EXTENSION |
                      VALID_DATA_DOSNAME |
                      VALID_DATA_SIZE |
                      VALID_DATA_TYPE |
                      VALID_DATA_DATE |
                      VALID_DATA_TIME |
                      VALID_DATA_ATTRIBUTES |
                      VALID_DATA_HIDDEN |
                      VALID_DATA_ISLINK |
                      VALID_DATA_ISOFFLINE |
                      VALID_DATA_ICONOVERLAY);

    // optimizations: we will tell Salamander to prepare in advance
    // (before with 90 000 files Files and Dirs reallocations took several seconds)
    int filesCount = 0;
    int dirsCount = 0;
    DWORD i;
    for (i = 0; i < CurrentDir->NumDirItems; i++)
    {
        if (CurrentDir->DirItems[i].Record->IsDir)
            dirsCount++;
        else
            filesCount++;
    }
    dir->SetApproximateCount(filesCount, dirsCount);

    // add up-dir if we are not in root
    CFileData fd;
    memset(&fd, 0, sizeof(fd));
    if (CurrentDir != Snapshot->Root)
    {
        fd.Attr = FILE_ATTRIBUTE_DIRECTORY;
        fd.LastWrite = CurrentDir->TimeLastWrite;
        fd.Name = SalamanderGeneral->DupStr("..");
        fd.NameLen = 2;
        fd.Ext = fd.Name + fd.NameLen;
        fd.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
        dir->AddDir(NULL, fd, NULL);
    }

    int sortByExtDirsAsFiles;
    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                          sizeof(sortByExtDirsAsFiles), NULL);

    // list all files on path where points CurrentDir
    for (i = 0; i < CurrentDir->NumDirItems; i++)
    {
        DIR_ITEM_I<char>* di = &CurrentDir->DirItems[i];
        fd.Attr = di->Record->Attr;
        fd.LastWrite = di->Record->TimeLastWrite;

        fd.Size = CQuadWord(0, 0);
        DATA_STREAM_I<char>* stream = di->Record->Streams;
        while (stream != NULL && stream->DSName != NULL)
            stream = stream->DSNext; // find default stream
        if (stream != NULL)
            fd.Size = CQuadWord().SetUI64(stream->DSSize);

        fd.Name = SalamanderGeneral->DupStr(di->FileName->FNName);
        if (fd.Name == NULL)
        {
            FatalError = TRUE;
            return String<char>::Error(IDS_UNDELETE, IDS_LOWMEM);
        }
        Replace0xE5(fd.Name);
        fd.NameLen = strlen(fd.Name);
        fd.Ext = fd.Name + fd.NameLen;

        if (di->FileName->DOSName)
        {
            fd.DosName = SalamanderGeneral->DupStr(di->FileName->DOSName);
            if (fd.DosName == NULL)
            {
                FatalError = TRUE;
                return String<char>::Error(IDS_UNDELETE, IDS_LOWMEM);
            }
            Replace0xE5(fd.DosName);
        }
        else
            fd.DosName = NULL;

        fd.PluginData = (DWORD_PTR)di;

        if (sortByExtDirsAsFiles || !di->Record->IsDir)
        {
            char* s = strrchr(fd.Name, '.');
            if (s != NULL)
                fd.Ext = s + 1; // ".cvspass" is extension in Windows
        }

        fd.IconOverlayIndex = (di->Record->Flags & FR_FLAGS_DELETED) ? 0 : ICONOVERLAYINDEX_NOTUSED;
        fd.Hidden = (fd.Attr & FILE_ATTRIBUTE_HIDDEN) ? 1 : 0;
        fd.IsOffline = (fd.Attr & FILE_ATTRIBUTE_OFFLINE) ? 1 : 0; // IconOverlayIndex displayed for deleted items has higher priority

        if (di->Record->IsDir)
        {
            fd.IsLink = 0;
            dir->AddDir(NULL, fd, NULL);
        }
        else
        {
            fd.IsLink = SalamanderGeneral->IsFileLink(fd.Ext);
            dir->AddFile(NULL, fd, NULL);
        }
    }

    pluginData = new CPluginFSDataInterface(SnapshotState);

    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::TryCloseOrDetach(%d, %d, %d, %d)",
                        forceClose, canDetach, detach, reason);
    detach = FALSE;
    return TRUE;
}

void WINAPI
CPluginFSInterface::Event(int event, DWORD param)
{
}

HICON WINAPI
CPluginFSInterface::GetFSIcon(BOOL& destroyIcon)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::GetFSIcon(%d)", destroyIcon);
    destroyIcon = TRUE;
    // return LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_FS)); // wrong colors in Alt+F1/2: As Other Panel
    return (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_FS),
                            IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags());
}

BOOL WINAPI
CPluginFSInterface::GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::GetNextDirectoryLineHotPath(%s, %d, "
                        "%d)",
                        text, pathLen, offset);
    if (offset == 0)
    {
        while (offset < pathLen && text[offset] != ':')
            offset++;                                                      // skip FS name
        if (offset < pathLen && offset + 1 + (int)strlen(Root) <= pathLen) // always true
            offset = offset + 1 + (int)strlen(Root);
    }
    else
    {
        while (offset < pathLen && text[offset] != '\\')
            offset++;
        if (offset > 0 && text[offset - 1] == ':')
            offset++;
    }
    return offset < pathLen;
}

// ****************************************************************************
//
//  CFileList - helper class for CopyOrMoveFromFS
//

struct FL_ITEM
{
    char* Name;
    FILE_RECORD_I<char>* Record;

    FL_ITEM()
    {
        Name = NULL;
        Record = NULL;
    }
    ~FL_ITEM() { delete[] Name; }
};

class CFileList : public TIndirectArray<FL_ITEM>
{
public:
    CFileList() : TIndirectArray<FL_ITEM>(256, 256, dtDelete) {}
    BOOL AddFile(DIR_ITEM_I<char>* di);
    BOOL RenameDuplicateFiles();
};

BOOL CFileList::AddFile(DIR_ITEM_I<char>* di)
{
    CALL_STACK_MESSAGE1("CFileList::AddFile( )");

    FL_ITEM* item = new FL_ITEM;
    if (item == NULL)
        return String<char>::Error(IDS_UNDELETE, IDS_LOWMEM);
    item->Name = SalamanderGeneral->DupStr(di->FileName->FNName);
    if (item->Name == NULL)
        return String<char>::Error(IDS_UNDELETE, IDS_LOWMEM);
    item->Record = di->Record;

    Add(item);
    return IsGood();
}

static int compare_items(const void* elem1, const void* elem2)
{
    return _stricmp((*((FL_ITEM**)elem1))->Name, (*((FL_ITEM**)elem2))->Name);
}

BOOL CFileList::RenameDuplicateFiles()
{
    CALL_STACK_MESSAGE1("CFileList::RenameDuplicateFiles()");

    // sort by name
    qsort(Data, Count, sizeof(FL_ITEM*), compare_items);

    // rename duplicated names
    for (int i = 0; i < Count - 1; i++)
    {
        int j = i;
        while (j + 1 < Count && !_stricmp(operator[](i)->Name, operator[](j + 1)->Name))
            j++;

        if (j > i)
        {
            for (int k = i, n = 1; k <= j; k++)
            {
                FL_ITEM* item = operator[](k);
                char* newname = String<char>::AddNumberSuffix(item->Name, n++);
                if (newname == NULL)
                    return String<char>::Error(IDS_UNDELETE, IDS_LOWMEM);
                delete[] item->Name;
                item->Name = newname;
            }
            i = j;
        }
    }

    return TRUE;
}

// ****************************************************************************
//
//  CopyOrMoveFromFS
//

BOOL CPluginFSInterface::AppendPath(char* buffer, char* path, char* name, BOOL* ret)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::AppendPath(, , , )");
    // spojeni cesty a jmena a kontrola delky
    lstrcpyn(buffer, path, MAX_PATH);
    if (!SalamanderGeneral->SalPathAppend(buffer, name, MAX_PATH))
    {
        *ret = TRUE;
        if (!SkipAllLongPaths)
        {
            switch (SalamanderGeneral->DialogError(hErrParent, BUTTONS_SKIPCANCEL, name,
                                                   String<char>::LoadStr(IDS_PATHTOOLONG), NULL))
            {
            case DIALOG_SKIPALL:
                SkipAllLongPaths = TRUE;
            case DIALOG_SKIP:
                break;
            case DIALOG_CANCEL:
                *ret = FALSE;
            }
        }
        return FALSE;
    }
    return TRUE;
}

char* CPluginFSInterface::FixDamagedName(char* name)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::FixDamagedName()");

    if (name[0] != 0xE5)
        return name;

    static char buffer[MAX_PATH];
    lstrcpyn(buffer, name, MAX_PATH);
    if (AllSubstChar)
    {
        buffer[0] = AllSubstChar;
        return buffer;
    }

    Replace0xE5(buffer);
    if (Progress)
        Progress->SetSourceFileName(buffer);
    CFileNameDialog dlg(hErrParent, buffer);
    if (dlg.Execute())
    {
        if (dlg.AllPressed && !strcmp(buffer + 1, name + 1))
            AllSubstChar = buffer[0];
        return buffer;
    }
    else
    {
        return NULL;
    }
}

static BOOL WINAPI EncryptedProgress(int inc, void* ctx)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::EncryptedProgress( , %d)", inc);
    CPluginFSInterface* fs = (CPluginFSInterface*)ctx;
    fs->FileProgress += inc;
    fs->TotalProgress += inc;
    fs->UpdateProgress();
    if (fs->Progress != NULL)
        return fs->Progress->GetWantCancel();
    else
        return SalamanderGeneral->GetSafeWaitWindowClosePressed();
}

char* GetSCFData(FILETIME* lastWrite, CQuadWord& size)
{
    CALL_STACK_MESSAGE1("GetSCFData( , )");
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    static char buffer[200];
    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    sprintf(buffer, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, size), date, time);

    return buffer;
}

BOOL CPluginFSInterface::CopyFile(FILE_RECORD_I<char>* record, char* filename, char* targetPath, BOOL view, int* incompleteFileAnswer)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::CopyFile(, , %d)", view);

    BOOL ret;
    char path[MAX_PATH + MAX_PATH]; // + for stream name
    int oldlen;

    if (!view)
    {
        // print source path to dialog box
        oldlen = (int)strlen(SourcePath);
        SalamanderGeneral->SalPathAddBackslash(SourcePath, MAX_PATH);
        char* namepos = SourcePath + strlen(SourcePath);
        SalamanderGeneral->SalPathAppend(SourcePath, filename, MAX_PATH);
        Replace0xE5(namepos);
        Progress->SetSourceFileName(SourcePath);

        // fix name if needed
        char* name = FixDamagedName(filename);
        if (name == NULL)
            return FALSE;
        lstrcpyn(namepos, name, MAX_PATH - (int)(namepos - SourcePath));

        // make target path
        if (!AppendPath(path, targetPath, name, &ret))
        {
            SourcePath[oldlen] = 0;
            return ret;
        }
    }
    else
    {
        // for view it is simple
        lstrcpyn(path, targetPath, MAX_PATH);
        lstrcpyn(SourcePath, filename, MAX_PATH);
        oldlen = 0;
    }

    // allocate buffer
    BYTE* buffer = new BYTE[COPY_BUFFER];
    if (buffer == NULL)
        return String<char>::Error(IDS_UNDELETE, IDS_LOWMEM);
    int bufclusters = COPY_BUFFER / Volume.BytesPerCluster;

    // init progress
    FileProgress = 1;
    FileTotal = GetFileSize(record, NULL) + 1;
    TotalProgress++;
    UpdateProgress();
    if (Progress)
        Progress->SetDestFileName(path);

    // extract all streams
    char* pathend = path + strlen(path);

    // we want to copy file stream (no ADS streams) first
    // otherwise file overwrite test will be broken beacuse with ADS will be also created base file
    TDirectArray<DATA_STREAM_I<char>*> copyStreams(10, 10);
    DATA_STREAM_I<char>* stream = record->Streams;
    while (stream != NULL)
    {
        if (stream->DSName != NULL)
            copyStreams.Add(stream);
        else
            copyStreams.Insert(0, stream);
        stream = stream->DSNext;
    }

    BOOL encrypted = (record->Attr & FILE_ATTRIBUTE_ENCRYPTED) != 0;
    ret = TRUE;
    BOOL deleteTargetOnError = TRUE;
    for (int streamIndex = 0; streamIndex < copyStreams.Count && ret; streamIndex++)
    {
        stream = copyStreams[streamIndex];
        if (encrypted)
        {
            // append .bak when backuping encrypted files
            if (BackupEncryptedFiles)
            {
                lstrcpyn(pathend, ".bak", MAX_PATH);
                strcat(SourcePath, ".bak");
            }
        }
        else
        {
            // append stream names
            if (stream->DSName != NULL)
            {
                *pathend = ':';
                lstrcpyn(pathend + 1, stream->DSName, MAX_PATH - 1);
            }
            else
                *pathend = 0;
        }

        // create file
        SAFE_FILE file;
        BOOL skipped;
        DWORD attr = record->Attr;
        if (view || encrypted)
            attr &= FILE_ATTRIBUTE_NORMAL;
        HANDLE result = SalamanderSafeFile->SafeFileCreate(
            path, GENERIC_WRITE, FILE_SHARE_READ,
            attr, FALSE, hErrParent, SourcePath,
            GetSCFData(&record->TimeLastWrite, CQuadWord().SetUI64(stream->DSSize)),
            &SilentMask, TRUE, &skipped, NULL, 0, NULL, &file);
        if (skipped)
        {
            // add progress for remaining streams
            for (int i = streamIndex; i < copyStreams.Count; i++)
                TotalProgress += copyStreams[i]->DSSize;
            FileProgress = 0;
            UpdateProgress();
            break;
        }
        if (result == INVALID_HANDLE_VALUE)
        {
            ret = FALSE;
            deleteTargetOnError = FALSE; // overwrite dialog was cancelled, keep target file
            break;
        }

        // extract data
        if (encrypted) // encrypted file
        {
            EFIC_CONTEXT<char> ctx(&Volume, record->Streams, buffer, COPY_BUFFER, EncryptedProgress, (void*)this);

            if (BackupEncryptedFiles)
            {
                BYTE buffer2[5000]; // WriteEncryptedFileRaw is also using buffer with size 5000
                ULONG length;
                ULONG numw;
                do
                {
                    length = 5000;
                    DWORD result2;
                    if ((result2 = EFIC_CONTEXT<char>::EncryptedFileImportCallback(buffer2, (PVOID)&ctx, &length)) != ERROR_SUCCESS ||
                        length && !SalamanderSafeFile->SafeFileWrite(&file, buffer2, length, &numw, hErrParent, BUTTONS_RETRYCANCEL, NULL, NULL))
                    {
                        if (result2 != ERROR_SUCCESS)
                        {
                            SetLastError(result2);
                            String<char>::SysError(IDS_UNDELETE, IDS_ERRORENCRYPTED);
                        }
                        ret = FALSE;
                        break;
                    }
                } while (length != 0);
                SalamanderSafeFile->SafeFileClose(&file);
            }
            else
            {
                // OpenEncryptedFileRaw (and others) unfortunately has own write to output file, we will open file
                // with SafeFileCreate anyway for error handling, but we need to close it
                SalamanderSafeFile->SafeFileClose(&file);

                PVOID context;
                DWORD result2;
                if ((result2 = OpenEncryptedFileRaw(path, CREATE_FOR_IMPORT, &context)) != ERROR_SUCCESS ||
                    (result2 = WriteEncryptedFileRaw((PFE_IMPORT_FUNC)(EFIC_CONTEXT<char>::EncryptedFileImportCallback), (PVOID)&ctx, context)) != ERROR_SUCCESS)
                // WriteEncryptedFileRaw is SLOOOOOW!!!
                {
                    if (result2 != ERROR_INVALID_PARAMETER) // return on user cancel
                    {
                        SetLastError(result2);
                        String<char>::SysError(IDS_UNDELETE, IDS_ERRORENCRYPTED);
                    }
                    ret = FALSE;
                }
                CloseEncryptedFileRaw(context);
            }

            // set time and attributes
            if (ret)
            {
                HANDLE hf = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                if (hf != INVALID_HANDLE_VALUE)
                {
                    SetFileTime(hf, &record->TimeCreation, &record->TimeLastAccess, &record->TimeLastWrite);
                    CloseHandle(hf);
                }
                SetFileAttributes(path, view ? FILE_ATTRIBUTE_ENCRYPTED : record->Attr);
            }
        }
        else
        {
            if (stream->DSSize != 0) // ordinary file
            {
                DWORD numw;
                CStreamReader<char> reader;
                reader.Init(&Volume, stream); // fixme: return value
                QWORD bytesleft = stream->DSSize;
                QWORD clustersleft = (bytesleft - 1) / Volume.BytesPerCluster + 1;
                BOOL exitLoop = FALSE;
                while (!exitLoop && bytesleft > 0)
                {
                    QWORD nc = min(clustersleft, bufclusters);
                    QWORD nb = min(bytesleft, nc * Volume.BytesPerCluster);
                    if (!(record->Attr & FILE_ATTRIBUTE_ENCRYPTED) && bytesleft < nb)
                        nb = bytesleft; // fixme

                    QWORD clustersRead; // number of read clusters
                    if (!reader.GetClusters(buffer, nc, &clustersRead))
                    {
                        if (clustersRead >= 0)
                        {
                            int dlgRet;
                            if (*incompleteFileAnswer == -1)
                            {
                                BOOL sameAnswerNextTime = FALSE;
                                dlgRet = String<char>::PartialRestore(view ? -1 : IDS_PARTIALRECOVER_DONTASK, view ? NULL : &sameAnswerNextTime,
                                                                      IDS_UNDELETE, IDS_PARTIALRECOVER, filename);
                                if (!view && sameAnswerNextTime && (dlgRet == DIALOG_YES || dlgRet == DIALOG_NO))
                                    *incompleteFileAnswer = dlgRet;
                            }
                            else
                            {
                                dlgRet = *incompleteFileAnswer;
                            }
                            if (dlgRet == DIALOG_NO || dlgRet == DIALOG_CANCEL)
                            {
                                ret = FALSE;
                                break;
                            }
                        }
                        else
                        {
                            String<char>::Error(IDS_UNDELETE, IDS_ERRORRECOVER, filename);
                            ret = FALSE;
                            break;
                        }
                    }

                    if (clustersRead < nc)
                    {
                        nc = clustersRead;
                        nb = clustersRead * Volume.BytesPerCluster;
                        exitLoop = TRUE;
                    }

                    if (!SalamanderSafeFile->SafeFileWrite(&file, buffer, (DWORD)nb, &numw, hErrParent, BUTTONS_RETRYCANCEL, NULL, NULL) ||
                        (Progress != NULL && Progress->GetWantCancel()) ||
                        (view && SalamanderGeneral->GetSafeWaitWindowClosePressed()))
                    {
                        ret = FALSE;
                        break;
                    }

                    FileProgress += nb;
                    TotalProgress += nb;
                    UpdateProgress();

                    clustersleft -= nc;
                    bytesleft -= nb;
                }
            }
            if (ret && streamIndex == copyStreams.Count - 1) // set time
                SetFileTime(file.HFile, &record->TimeCreation, &record->TimeLastAccess, &record->TimeLastWrite);
            SalamanderSafeFile->SafeFileClose(&file);
        }

        if (encrypted)
            break; // we don't walk through all streams for encrypted files
    }

    // remove file on error
    if (!ret && deleteTargetOnError)
    {
        *pathend = 0;
        SalamanderGeneral->ClearReadOnlyAttr(path);
        DeleteFile(path);
    }

    SourcePath[oldlen] = 0;
    delete[] buffer;
    return ret;
}

BOOL CPluginFSInterface::CopyDir(FILE_RECORD_I<char>* record, char* filename, char* targetPath)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::CopyDir(, )");

    // print source path to the dialog box
    int oldlen = (int)strlen(SourcePath);
    SalamanderGeneral->SalPathAddBackslash(SourcePath, MAX_PATH);
    char* namepos = SourcePath + strlen(SourcePath);
    SalamanderGeneral->SalPathAppend(SourcePath, filename, MAX_PATH);
    Replace0xE5(namepos);
    Progress->SetSourceFileName(SourcePath);

    // fix name if needed
    char* name = FixDamagedName(filename);
    if (name == NULL)
        return FALSE;

    // make target path
    char path[MAX_PATH];
    BOOL ret;
    if (!AppendPath(path, targetPath, name, &ret))
    {
        SourcePath[oldlen] = 0;
        return ret;
    }

    // create directory
    BOOL skipped;
    FileProgress = 0;
    FileTotal = 1;
    TotalProgress++;
    UpdateProgress();
    Progress->SetDestFileName(path);
    if (SalamanderSafeFile->SafeFileCreate(path, 0, 0, 0, TRUE, hErrParent, NULL, NULL,
                                           &SilentMask, TRUE, &skipped, NULL, 0, NULL, NULL) == INVALID_HANDLE_VALUE)
    {
        SourcePath[oldlen] = 0;
        return skipped;
    }

    // create files list
    CFileList list;
    ret = TRUE;
    for (DWORD i = 0; ret && i < record->NumDirItems; i++)
        ret = list.AddFile(record->DirItems + i);

    // copy list
    ret = ret && CopyFileList(list, path);

    SourcePath[oldlen] = 0;
    return ret;
}

QWORD CPluginFSInterface::GetFileSize(FILE_RECORD_I<char>* record, BOOL* encrypted)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::GetFileSize()");
    QWORD size = 0;
    DATA_STREAM_I<char>* stream = record->Streams;
    while (stream != NULL)
    {
        size += stream->DSSize;
        stream = stream->DSNext;
    }
    if (record->Attr & FILE_ATTRIBUTE_ENCRYPTED && encrypted != NULL)
        *encrypted = TRUE;
    return size;
}

QWORD CPluginFSInterface::GetDirSize(FILE_RECORD_I<char>* record, int plus, BOOL* encrypted)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::GetDirSize(, %d)", plus);
    QWORD size = 0;
    for (DWORD i = 0; i < record->NumDirItems; i++)
    {
        FILE_RECORD_I<char>* r = record->DirItems[i].Record;
        if (r->IsDir)
            size += GetDirSize(r, plus, encrypted);
        else
            size += GetFileSize(r, encrypted) + plus; // +1 for all-zero-size situation
    }
    return size + plus;
}

QWORD CPluginFSInterface::GetTotalProgress(int panel, BOOL focused, int plus, BOOL* encrypted)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::GetTotalProgress(%d, %d, %d)",
                        panel, focused, plus);
    const CFileData* fd;
    QWORD total = 0;
    int index = 0;
    if (encrypted != NULL)
        *encrypted = FALSE;
    while (1)
    {
        if (focused)
            fd = SalamanderGeneral->GetPanelFocusedItem(panel, NULL);
        else
            fd = SalamanderGeneral->GetPanelSelectedItem(panel, &index, NULL);
        if (fd == NULL)
            break;
        DIR_ITEM_I<char>* di = (DIR_ITEM_I<char>*)fd->PluginData; // we don't need to test snapshot state, during CPluginFSInterface::CopyOrMoveFromFS() it is valid

        if (di->Record->IsDir)
            total += GetDirSize(di->Record, plus, encrypted);
        else
            total += GetFileSize(di->Record, encrypted) + plus;

        if (focused)
            break;
    }
    return total;
}

void CPluginFSInterface::UpdateProgress()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CPluginFSInterface::UpdateProgress()");
    if (Progress == NULL)
        return;
    if (FileTotal)
        Progress->SetFileProgress((DWORD)(FileProgress * 1000 / FileTotal));
    if (GrandTotal)
        Progress->SetTotalProgress((DWORD)(TotalProgress * 1000 / GrandTotal));
}

BOOL CPluginFSInterface::CopyFileList(CFileList& list, char* targetPath)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::CopyFileList( , %s)", targetPath);

    if (!list.RenameDuplicateFiles())
        return FALSE;

    int incompleteFileAnswer = -1;
    BOOL ret = TRUE;
    int i;
    for (i = 0; ret && i < list.Count; i++)
    {
        FL_ITEM* item = list[i];
        if (item->Record->IsDir)
            ret = CopyDir(item->Record, item->Name, targetPath);
        else
            ret = CopyFile(item->Record, item->Name, targetPath, FALSE, &incompleteFileAnswer);
    }

    return TRUE;
}

void UndeleteGetResolvedRootPath(const char* path, char* resolvedPath)
{
    strcpy(resolvedPath, path);
    SalamanderGeneral->ResolveSubsts(resolvedPath);
    char rootPath[MAX_PATH];
    SalamanderGeneral->GetRootPath(rootPath, resolvedPath);
    if (!SalamanderGeneral->IsUNCPath(rootPath) && GetDriveType(rootPath) == DRIVE_FIXED) // reparse points exist only on fixed drives
    {
        BOOL cutPathIsPossible = TRUE;
        SalamanderGeneral->ResolveLocalPathWithReparsePoints(resolvedPath, path, &cutPathIsPossible, NULL, NULL, NULL, NULL, NULL);
        // we need root for GetVolumeInformation
        if (cutPathIsPossible)
        {
            SalamanderGeneral->GetRootPath(rootPath, resolvedPath);
            strcpy(resolvedPath, rootPath);
        }
    }
    else
    {
        strcpy(resolvedPath, rootPath);
    }
    SalamanderGeneral->SalPathAddBackslash(resolvedPath, MAX_PATH);
}

BOOL CPluginFSInterface::PrepareRawAPI(char* targetPath, BOOL allowBackup)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::PrepareRawAPI()");

    char resolvedPath[MAX_PATH];
    UndeleteGetResolvedRootPath(targetPath, resolvedPath);

    DWORD flags;
    if (!GetVolumeInformation(resolvedPath, NULL, 0, NULL, NULL, &flags, NULL, 0) ||
        !(flags & FILE_SUPPORTS_ENCRYPTION))
    {
        if (allowBackup)
        {
            // info we are going to copy backups
            if (!ConfigDontShowEncryptedWarning)
            {
                MSGBOXEX_PARAMS mbep;
                memset(&mbep, 0, sizeof(mbep));
                mbep.HParent = SalamanderGeneral->GetMsgBoxParent();
                mbep.Text = String<char>::LoadStr(IDS_ENCRYPTEDWARNING);
                mbep.Caption = String<char>::LoadStr(IDS_UNDELETE);
                mbep.Flags = MSGBOXEX_OKCANCEL | MB_ICONWARNING;
                mbep.HIcon = NULL;
                mbep.HelpCallback = NULL;
                mbep.CheckBoxText = String<char>::LoadStr(IDS_DONTSHOWAGAIN);
                mbep.CheckBoxValue = &ConfigDontShowEncryptedWarning;
                mbep.AliasBtnNames = NULL;
                if (SalamanderGeneral->SalMessageBoxEx(&mbep) == DIALOG_CANCEL)
                {
                    return FALSE;
                }
            }
            BackupEncryptedFiles = TRUE;
        }
        else
        {
            String<char>::Error(IDS_UNDELETE, IDS_CANNOTVIEW);
            return FALSE;
        }
    }
    else
    {
        BackupEncryptedFiles = FALSE;
    }
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                     int panel, int selectedFiles, int selectedDirs,
                                     char* targetPath, BOOL& operationMask,
                                     BOOL& cancelOrHandlePath, HWND dropTarget)
{
    CALL_STACK_MESSAGE9("CPluginFSInterface::CopyOrMoveFromFS(%d, %d, %s, , %d, "
                        "%d, %d, , %d, %d, )",
                        copy, mode, fsName, panel,
                        selectedFiles, selectedDirs, operationMask,
                        cancelOrHandlePath);

    // let Salamander do entering path and handling path errors
    if (mode == 1 || mode == 4)
        return FALSE;

    // let Salamander do test path validity
    if (mode == 2)
    {
        operationMask = FALSE;
        cancelOrHandlePath = TRUE;
        return FALSE;
    }

    // drag & drop
    if (mode == 5)
    {
        if (targetPath[0] != 0 && targetPath[1] == ':' ||   // disk path (C:\path)
            targetPath[0] == '\\' && targetPath[1] == '\\') // UNC path (\\server\share\path)
        {                                                   // append backslash to ensure it is path (in 'mode'==5 it is always path)
            SalamanderGeneral->SalPathAddBackslash(targetPath, MAX_PATH);
        }

        BOOL ok = TRUE;
        int type;
        char* secondPart;
        BOOL isDir;
        if (SalamanderGeneral->SalParsePath(parent, targetPath, type, isDir, secondPart,
                                            String<char>::LoadStr(IDS_ERROR), NULL, FALSE,
                                            NULL, NULL, NULL, 2 * MAX_PATH))
        {
            if (type != PATH_TYPE_WINDOWS)
            {
                SalamanderGeneral->SalMessageBox(parent, String<char>::LoadStr(IDS_BADPATH),
                                                 String<char>::LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
                ok = FALSE;
            }
        }
        if (!ok)
        {
            cancelOrHandlePath = TRUE;
            return TRUE;
        }
    }

    // init
    BOOL ret = TRUE;
    const CFileData* fd;
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL encrypted;
    SkipAllLongPaths = FALSE;
    SilentMask = 0;
    strcpy(SourcePath, Path);
    AllSubstChar = 0;

    // fixme: test if Volume is open and valid?

    // don't undelete to the same volume
    if (!ConfigDontShowSamePartitionWarning && !Volume.IsImage && SalamanderGeneral->PathsAreOnTheSameVolume(targetPath, Root, NULL))
    {
        MSGBOXEX_PARAMS mbep;
        memset(&mbep, 0, sizeof(mbep));
        mbep.HParent = parent;
        mbep.Text = String<char>::LoadStr(IDS_SAMEPARTITION);
        mbep.Caption = String<char>::LoadStr(IDS_UNDELETE);
        mbep.Flags = MSGBOXEX_OKCANCEL | MB_ICONWARNING | MSGBOXEX_DEFBUTTON2;
        mbep.HIcon = NULL;
        mbep.HelpCallback = NULL;
        mbep.CheckBoxText = String<char>::LoadStr(IDS_DONTSHOWAGAIN);
        mbep.CheckBoxValue = &ConfigDontShowSamePartitionWarning;
        mbep.AliasBtnNames = NULL;
        if (SalamanderGeneral->SalMessageBoxEx(&mbep) == DIALOG_CANCEL)
        {
            cancelOrHandlePath = TRUE;
            return TRUE;
        }
    }

    // test free space
    if (!SalamanderGeneral->TestFreeSpace(parent, targetPath,
                                          CQuadWord().SetUI64(GetTotalProgress(panel, focused, 0, &encrypted)),
                                          String<char>::LoadStr(IDS_UNDELETE)))
    {
        cancelOrHandlePath = TRUE;
        return TRUE;
    }

    // prepare for encrypted files recovery
    if (encrypted)
    {
        if (!PrepareRawAPI(targetPath, TRUE))
        {
            cancelOrHandlePath = TRUE;
            return TRUE;
        }
    }

    // open progress
    HWND mainwnd = SalamanderGeneral->GetMainWindowHWND();
    CCopyProgressDlg dlg(mainwnd, ooStatic);
    if (dlg.Create() == NULL)
        return String<char>::SysError(IDS_UNDELETE, IDS_ERROROPENINGPROGRESS);
    EnableWindow(mainwnd, FALSE);
    HProgressDlg = dlg.HWindow; // redirect message box parent
    SetForegroundWindow(dlg.HWindow);
    hErrParent = dlg.HWindow;
    Progress = &dlg;
    TotalProgress = FileProgress = 0;
    GrandTotal = GetTotalProgress(panel, focused, 1, NULL);

    // prepare list of files
    CFileList list;
    while (ret)
    {
        if (focused)
            fd = SalamanderGeneral->GetPanelFocusedItem(panel, NULL);
        else
            fd = SalamanderGeneral->GetPanelSelectedItem(panel, &index, NULL);
        if (fd == NULL)
            break;
        ret = list.AddFile((DIR_ITEM_I<char>*)fd->PluginData); // we don't need to test snapshot state, during CPluginFSInterface::CopyOrMoveFromFS() it is valid
        if (focused)
            break;
    }

    // copy stream
    ret = ret && CopyFileList(list, targetPath);

    // close progress
    EnableWindow(mainwnd, TRUE);
    HProgressDlg = NULL;
    DestroyWindow(dlg.HWindow);

    // report changes on target path
    if (ret)
        SalamanderGeneral->PostChangeOnPathNotification(targetPath, FALSE);

    if (!ret)
        cancelOrHandlePath = TRUE;
    return TRUE;
}

// ****************************************************************************
//
//  ViewFile
//

BOOL CPluginFSInterface::GetTempDirOutsideRoot(HWND parent, char* buffer, char** ret)
{
    char* tempdir = NULL; // return NULL for system TEMP
    char path[MAX_PATH];
    GetTempPath(MAX_PATH, path); // get system TEMP
    if (!Volume.IsImage && SalamanderGeneral->PathsAreOnTheSameVolume(Root, path, NULL))
    {
        while (ConfigTempPath[0] == 0 || SalamanderGeneral->PathsAreOnTheSameVolume(Root, ConfigTempPath, NULL))
        {
            char text[200];
            sprintf(text, String<char>::LoadStr(IDS_TEMPDIR), path[0], path[0]);
            if (!SalamanderGeneral->GetTargetDirectory(parent, parent, String<char>::LoadStr(IDS_VIEW),
                                                       text, ConfigTempPath, FALSE, NULL))
            {
                return FALSE;
            }
        }
        tempdir = ConfigTempPath;
    }
    *ret = tempdir;
    return TRUE;
}

void WINAPI
CPluginFSInterface::ViewFile(const char* fsName, HWND parent,
                             CSalamanderForViewFileOnFSAbstract* salamander,
                             CFileData& file)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::ViewFile(%s, , , )", fsName);

    // chose different TEMP dir when we have opened same volume as volume where disk-cache is
    char buff[MAX_PATH];
    char* tempdir;
    if (!GetTempDirOutsideRoot(parent, buff, &tempdir))
        return;

    // prepare unique file name for disk-cache (standard Salamander path format)
    char uniqueFileName[2 * MAX_PATH];
    sprintf(uniqueFileName, "%IX", file.PluginData); // we don't need to test snapshot state, during CPluginFSInterface::CopyOrMoveFromFS() it is valid
    strcat(uniqueFileName, ":");
    strcat(uniqueFileName, fsName);
    strcat(uniqueFileName, ":");
    strcat(uniqueFileName, Path);
    SalamanderGeneral->SalPathAppend(uniqueFileName + strlen(fsName) + 1, file.Name, MAX_PATH);
    // name on disk are case-insensitive, disk-cache is case-sensitive, we will convert
    // to lowercase so disk-cache will behave as case-insensitive
    SalamanderGeneral->ToLowerCase(uniqueFileName);

    // get name of file copy in disk-cache
    BOOL fileExists;
    const char* tmpFileName = salamander->AllocFileNameInCache(parent, uniqueFileName, file.Name, tempdir, fileExists);
    if (tmpFileName == NULL)
        return; // fatal error

    // find out if we need prepare file copy for disk-cache (download)
    BOOL newFileOK = FALSE;
    CQuadWord newFileSize(0, 0);
    if (!fileExists) // we need it
    {
        Progress = NULL;
        SilentMask = SILENT_OVERWRITE_FILE_EXIST | SILENT_OVERWRITE_FILE_SYSHID;
        hErrParent = parent;

        DIR_ITEM_I<char>* di = (DIR_ITEM_I<char>*)file.PluginData; // we don't need to test snapshot state, during CPluginFSInterface::CopyOrMoveFromFS() it is valid
        BOOL encrypted = (di->Record->Attr & FILE_ATTRIBUTE_ENCRYPTED) ? TRUE : FALSE;

        if (encrypted)
        {
            if (!PrepareRawAPI((char*)tmpFileName, FALSE))
                return;
        }

        SalamanderGeneral->CreateSafeWaitWindow(String<char>::LoadStr(IDS_WAIT), String<char>::LoadStr(IDS_UNDELETE),
                                                1500, TRUE, NULL);
        int incompleteFileAnswer = -1;
        newFileOK = CopyFile(di->Record, file.Name, (char*)tmpFileName, TRUE, &incompleteFileAnswer);
        SalamanderGeneral->DestroySafeWaitWindow();

        if (newFileOK)
        {
            HANDLE hFile = HANDLES_Q(CreateFile(tmpFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                NULL, OPEN_EXISTING, 0, NULL));
            if (hFile != INVALID_HANDLE_VALUE)
            { // ignore error, file size doesn't matter so much
                DWORD err;
                SalamanderGeneral->SalGetFileSize(hFile, newFileSize, err); // ignore errors
                HANDLES(CloseHandle(hFile));
            }
        }
    }

    // open viewer
    HANDLE fileLock;
    BOOL fileLockOwner;
    if (!fileExists && !newFileOK || // open viewer only when file copy is OK
        !salamander->OpenViewer(parent, tmpFileName, &fileLock, &fileLockOwner))
    { // clear "lock" on error
        fileLock = NULL;
        fileLockOwner = FALSE;
    }

    // we must call FreeFileNameInCache in pair to AllocFileNameInCache  (connect viewer and disk-cache)
    salamander->FreeFileNameInCache(uniqueFileName, fileExists, newFileOK,
                                    newFileSize, fileLock, fileLockOwner, FALSE);
}

// ****************************************************************************
//
//  ContextMenu
//

void CPluginFSInterface::ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                     int panel, int selectedFiles, int selectedDirs)
{
    if (type != fscmItemsInPanel)
        return;

    BOOL focusIsDir;
    const CFileData* fd = SalamanderGeneral->GetPanelFocusedItem(panel, &focusIsDir);
    DIR_ITEM_I<char>* di = (DIR_ITEM_I<char>*)fd->PluginData;

    // vytvorime menu
    CGUIMenuPopupAbstract* ctxMenu = SalamanderGUI->CreateMenuPopup();
    if (ctxMenu == NULL)
        return;

    // vlozeni prikazu Salamandera
    char cmdName[300];
    int salIndex = 0;
    int index = 0;
    int salCmd;
    BOOL enabled;
    int type2;
    int lastType = sctyUnknown;
    while (SalamanderGeneral->EnumSalamanderCommands(&salIndex, &salCmd, cmdName, _countof(cmdName), &enabled, &type2))
    {
        if (!enabled || salCmd == SALCMD_OPEN || salCmd == SALCMD_REFRESH || salCmd == SALCMD_DISCONNECT)
            continue;

        MENU_ITEM_INFO mi;
        mi.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING;
        mi.Type = MENU_TYPE_STRING;
        mi.ID = salCmd + CMD_SALCMD_OFFSET;
        mi.String = cmdName;
        ctxMenu->InsertItem(index++, TRUE, &mi);
    }
#if defined(_DEBUG) && _WIN32_WINNT >= 0x0501
    MENU_ITEM_INFO mi;
    MENU_ITEM_INFO mis;
    mis.Mask = MENU_MASK_TYPE;
    mis.Type = MENU_TYPE_SEPARATOR;
    mis.String = NULL;
    if (index > 0)
        ctxMenu->InsertItem(index++, TRUE, &mis);
    mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID | MENU_MASK_STRING;
    mi.Type = MENU_TYPE_STRING;
    mi.State = 0;
    char strbuff[300];
    mi.String = strbuff;
    if (!focusIsDir)
    {
        mi.ID = CMD_VIEWINFO;
        strcpy(strbuff, "DBG View Underlying File System Data");
        ctxMenu->InsertItem(index++, TRUE, &mi);
        mi.ID = CMD_FRAGMENTFILE;
        strcpy(strbuff, "DBG Fragment File");
        ctxMenu->InsertItem(index++, TRUE, &mi);
        mi.ID = CMD_SETVALIDDATAFILE;
        strcpy(strbuff, "DBG Zero End of File with SetFileValidData");
        ctxMenu->InsertItem(index++, TRUE, &mi);
        mi.ID = CMD_SETSPARSEFILE;
        strcpy(strbuff, "DBG Zero Middle of File with FSCTL_SET_ZERO_DATA");
        ctxMenu->InsertItem(index++, TRUE, &mi);
        // ----
        ctxMenu->InsertItem(index++, TRUE, &mis);
    }
    mi.ID = CMD_DUMPFILES;
    strcpy(strbuff, "DBG Dump Specified Files");
    ctxMenu->InsertItem(index++, TRUE, &mi);
    mi.ID = CMD_COMPAREFILES;
    strcpy(strbuff, "DBG Test Undelete on All Existing Files");
    ctxMenu->InsertItem(index++, TRUE, &mi);
#endif //_DEBUG

    DWORD cmd = ctxMenu->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON | MENU_TRACK_NONOTIFY,
                               menuX, menuY, parent, NULL);
    SalamanderGUI->DestroyMenuPopup(ctxMenu);
    if (cmd >= CMD_SALCMD_OFFSET)
        SalamanderGeneral->PostSalamanderCommand(cmd - CMD_SALCMD_OFFSET);

#if defined(_DEBUG) && _WIN32_WINNT >= 0x0501
    if (cmd == CMD_VIEWINFO || cmd == CMD_DUMPFILES || cmd == CMD_COMPAREFILES)
        DumpDebugInformation(parent, di, cmd);
    if (cmd == CMD_FRAGMENTFILE || cmd == CMD_SETVALIDDATAFILE || cmd == CMD_SETSPARSEFILE)
    {
        char fullPath[2 * MAX_PATH];
        char* archiveOrFS = NULL;
        int type;
        if (SalamanderGeneral->GetPanelPath(panel, fullPath, sizeof(fullPath), &type, &archiveOrFS, NULL) && archiveOrFS != NULL)
        {
            archiveOrFS++;
            SalamanderGeneral->SalPathAppend(archiveOrFS, di->FileName->FNName, MAX_PATH);
            if (cmd == CMD_FRAGMENTFILE)
                FragmentFile(parent, di, archiveOrFS);
            if (cmd == CMD_SETVALIDDATAFILE || cmd == CMD_SETSPARSEFILE)
            {
                int confirm = SalamanderGeneral->SalMessageBox(parent, "!!!WARNING!!!\n\nPart of file will be overwritten with zeros. Do you want to continue?",
                                                               String<char>::LoadStr(IDS_QUESTION), MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
                if (confirm == IDYES)
                {
                    if (cmd == CMD_SETVALIDDATAFILE)
                        SetFileValidData(parent, di, archiveOrFS);
                    if (cmd == CMD_SETSPARSEFILE)
                        SetFileSparse(parent, di, archiveOrFS);
                }
            }
        }
    }
#endif _DEBUG
}

#if defined(_DEBUG) && _WIN32_WINNT >= 0x0501
void DumpExistingFileLayout(FILE* file, const char* fileName, CVolume<char>* volume)
{
    HANDLE hFile = HANDLES_Q(CreateFile(fileName, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        return;
    }

    DWORD hiSize = 0;
    DWORD loSize = ::GetFileSize(hFile, &hiSize);
    if (loSize == INVALID_FILE_SIZE)
        TRACE_E("GetFileSize() failed on " << fileName);
    LONGLONG fileSize = MAKEQWORD(loSize, hiSize);

    fprintf(file, "\n");
    fprintf(file, "Query existing file %s\n", fileName);
    fprintf(file, "FSCTL_GET_RETRIEVAL_POINTERS\n");
    if (volume->Type == vtFAT || volume->Type == vtExFAT)
        fprintf(file, "NOTE: LCNs on FAT and exFAT are zero based in this listing\n");
    fprintf(file, "LCN,LENGTH,SEEK\n");

    STARTING_VCN_INPUT_BUFFER inBuffer = {0};
    LARGE_INTEGER rawBuffer[2 * 1024];
    RETRIEVAL_POINTERS_BUFFER* outBuffer = (RETRIEVAL_POINTERS_BUFFER*)rawBuffer;
    DWORD bytesReturned;
    QWORD seek = 0;
    while (DeviceIoControl(hFile, FSCTL_GET_RETRIEVAL_POINTERS,
                           &inBuffer, sizeof(inBuffer),
                           outBuffer, sizeof(rawBuffer),
                           &bytesReturned, NULL) ||
           GetLastError() == ERROR_MORE_DATA)
    {
        ULONG i;
        LARGE_INTEGER prevVcn = inBuffer.StartingVcn;
        for (i = 0; i < outBuffer->ExtentCount; i++)
        {
            LARGE_INTEGER lcn = outBuffer->Extents[i].Lcn;
            LARGE_INTEGER nextVcn = outBuffer->Extents[i].NextVcn;
            LONGLONG vcn = nextVcn.QuadPart - prevVcn.QuadPart;
            prevVcn = nextVcn;
            if (lcn.QuadPart == 0xFFFFFFFFFFFFFFFF)
                fprintf(file, "-1,%I64u,%I64u(0x%I64X)\n", vcn, seek, seek);
            else
                fprintf(file, "%I64u,%I64u,%I64u(0x%I64X)\n", lcn.QuadPart, vcn, seek, seek);
            seek += vcn * volume->BytesPerCluster;
        }
        inBuffer.StartingVcn = prevVcn;
    }
    DWORD err = GetLastError();

    HANDLES(CloseHandle(hFile));
}

void CPluginFSInterface::DumpDirItemInfo(FILE* file, const DIR_ITEM_I<char>* di)
{
    if (di->Record == NULL)
        return;
    DATA_STREAM_I<char>* stream = di->Record->Streams;
    while (stream != NULL)
    {
        if (stream->DSName != NULL)
            fprintf(file, "Stream: %s\n", stream->DSName);
        else
            fprintf(file, "Stream\n");
        if (stream->DSSize != stream->DSValidSize)
        {
            fprintf(file, "Stream size:     %I64u(0x%I64X)\n", stream->DSSize, stream->DSSize);
            fprintf(file, "Valid data size: %I64u(0x%I64X)\n", stream->DSValidSize, stream->DSValidSize);
        }
        if (stream->Ptrs != NULL)
        {
            fprintf(file, "LCN,LENGTH,SEEK,FLAGS\n");
            CRunsWalker<CHAR> runsWalker(stream->Ptrs);
            QWORD lcn = 0;
            QWORD seek = 0;
            CRunsWalkerQuery query;
            query.LastRun = FALSE;
            while (!query.LastRun)
            {
                QWORD lcn;
                QWORD length;
                if (runsWalker.GetNextRun(&lcn, &length, &query))
                {
                    fprintf(file, (lcn == -1) ? "-1" : "%I64u", lcn);
                    fprintf(file, ",%I64u", length);
                    fprintf(file, ",%I64u(0x%I64X)", seek, seek);
                    if (query.NewPointers)
                        fprintf(file, ",(NewPtrs DPFlags=0x%X)", query.DPFlags);
                    fprintf(file, "\n");
                    seek += length * Volume.BytesPerCluster;
                }
                else
                {
                    fprintf(file, "GetNextRun() failed\n");
                    break;
                }
            }
        }
        else
        {
            if (stream->ResidentData != NULL)
                fprintf(file, "Resident Data: %I64u\n", stream->DSSize);
        }
        stream = stream->DSNext;
    }
}

void InspectDataRuns(const DATA_POINTERS* dp, int* depth, int* sparseCnt)
{
    (*depth)++;

    const BYTE* iter = dp->Runs;
    if (iter == NULL)
        return;

    while (*iter != 0 && ((iter - dp->Runs) < (int)dp->RunsSize))
    {
        BYTE lengthBytes = ((*iter) & 0x0F);
        BYTE offsetBytes = ((*iter) & 0xF0) >> 4;
        if (offsetBytes == 0)
            (*sparseCnt)++;

        iter++;
        iter += lengthBytes;
        iter += offsetBytes;
    }

    if (dp->DPNext != NULL)
        InspectDataRuns(dp->DPNext, depth, sparseCnt);
}

void CPluginFSInterface::DumpSpecifiedFiles(FILE* file, FILE_RECORD_I<char>* dir, char* path, int pathSize)
{
    size_t pathLen = strlen(path);
    for (DWORD i = 0; i < dir->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = dir->DirItems[i].Record;
        path[pathLen] = 0;
        SalamanderGeneral->SalPathAppend(path, r->FileNames->FNName, pathSize);
        if ((r->Flags & FR_FLAGS_DELETED) != 0)
            continue;
        if (r->IsDir)
        {
            DumpSpecifiedFiles(file, r, path, pathSize);
        }
        else
        {
            //      if ((r->Attr & FILE_ATTRIBUTE_COMPRESSED) != 0) continue;
            //      if ((r->Attr & FILE_ATTRIBUTE_SPARSE_FILE) != 0) continue;
            if (r->Streams == NULL)
                continue;
            if (r->Streams->Ptrs == NULL)
                continue;
            //      if (r->Streams->Ptrs->Runs == NULL || r->Streams->Ptrs->RunsSize == 0) continue;
            //      if (r->Streams->Ptrs->DPNext == NULL) continue;
            int depth = 0;
            int sparseCnt = 0;
            InspectDataRuns(r->Streams->Ptrs, &depth, &sparseCnt);
            if (depth <= 1)
                continue;
            //      if (sparseCnt == 0) continue;
            fprintf(file, "%s\n", path);
        }
    }
    path[pathLen] = 0;
}

BOOL CPluginFSInterface::TestUndeleteOnExistingFile(FILE* file, FILE_RECORD_I<char>* record, char* path, int pathSize)
{
    BOOL ret;

    // allocate buffer
    BYTE* buffer = new BYTE[COPY_BUFFER];
    BYTE* orgBuffer = new BYTE[COPY_BUFFER];
    int bufclusters = COPY_BUFFER / Volume.BytesPerCluster;

    // extract all streams
    char* pathend = path + strlen(path);
    DATA_STREAM_I<char>* stream = record->Streams;
    BOOL encrypted = (record->Attr & FILE_ATTRIBUTE_ENCRYPTED) != 0;
    ret = TRUE;
    while (stream != NULL && ret)
    {
        if (!encrypted)
        {
            // append stream names
            if (stream->DSName != NULL)
            {
                *pathend = ':';
                lstrcpyn(pathend + 1, stream->DSName, MAX_PATH - 1);
            }
            else
                *pathend = 0;
        }
        Progress->SetSourceFileName(path);

        FILE* orgFile = fopen(path, "rb");
        if (orgFile != NULL)
        {
            // extract data
            if (encrypted) // encrypted file
            {
                TRACE_I("Ignorting encrypted file (test it using copy and compare): " << path);
                fprintf(file, "Ignorting encrypted file (test it using copy and compare): %s\n", path);
            }
            else
            {
                if (stream->DSSize != 0) // ordinary file
                {
                    CStreamReader<char> reader;
                    reader.Init(&Volume, stream); // fixme: return value
                    QWORD bytesleft = stream->DSSize;
                    QWORD clustersleft = (bytesleft - 1) / Volume.BytesPerCluster + 1;
                    BOOL exitLoop = FALSE;
                    while (!exitLoop && bytesleft > 0)
                    {
                        QWORD nc = min(clustersleft, bufclusters);
                        QWORD nb = min(bytesleft, nc * Volume.BytesPerCluster);
                        if (!(record->Attr & FILE_ATTRIBUTE_ENCRYPTED) && bytesleft < nb)
                            nb = bytesleft; // fixme

                        QWORD clustersRead; // number of read clusters
                        if (!reader.GetClusters(buffer, nc, &clustersRead))
                            TRACE_E("reader.GetClusters() failed on " << path);
                        if (fread(orgBuffer, (size_t)nb, 1, orgFile) != 1)
                            TRACE_E("fread() failed on " << path);
                        if (memcmp(buffer, orgBuffer, (size_t)nb) != 0)
                        {
                            TRACE_E("Undeleted file doesn't match original on " << path);
                            fprintf(file, "Undeleted file doesn't match original: %s\n", path);
                        }

                        if (clustersRead < nc)
                        {
                            nc = clustersRead;
                            nb = clustersRead * Volume.BytesPerCluster;
                            exitLoop = TRUE;
                        }
                        clustersleft -= nc;
                        bytesleft -= nb;
                    }
                }
            }
            fclose(orgFile);
        }
        else
        {
            TRACE_I("fopen() failed on " << path);
        }
        if (encrypted)
            break; // we don't walk through all streams for encrypted files
        stream = stream->DSNext;
    }
    delete[] buffer;
    delete[] orgBuffer;

    TotalProgress++;
    UpdateProgress();

    Progress->GetWantCancel();
    return ret;
}

void CPluginFSInterface::TestUndeleteOnExistingFiles(FILE* file, FILE_RECORD_I<char>* dir, char* path, int pathSize, DWORD* count)
{
    DWORD cnt = 0;
    size_t pathLen = strlen(path);
    for (DWORD i = 0; i < dir->NumDirItems; i++)
    {
        FILE_RECORD_I<char>* r = dir->DirItems[i].Record;
        path[pathLen] = 0;
        // ignore deleted items and metafiles
        if ((r->Flags & FR_FLAGS_DELETED) != 0)
            continue;
        if ((r->Flags & FR_FLAGS_METAFILE) != 0)
            continue;
        SalamanderGeneral->SalPathAppend(path, r->FileNames->FNName, pathSize);
        if (r->IsDir)
        {
            TestUndeleteOnExistingFiles(file, r, path, pathSize, count);
        }
        else
        {
            if (count == NULL)
                TestUndeleteOnExistingFile(file, r, path, pathSize);
            cnt++;
        }
    }
    path[pathLen] = 0;
    if (count != NULL)
        (*count) += cnt;
}

void CPluginFSInterface::DumpDebugInformation(HWND parent, const DIR_ITEM_I<char>* di, DWORD mode)
{
    // chose different TEMP dir when we have opened same volume as volume where disk-cache is
    char buff[MAX_PATH];
    char* tempdir;
    if (!GetTempDirOutsideRoot(parent, buff, &tempdir))
        return;

    DWORD error;
    char fileNameBuf[MAX_PATH + MAX_PATH]; // + for stream name
    if (SalamanderGeneral->SalGetTempFileName(tempdir, "view", fileNameBuf, TRUE, &error))
    {
        FILE* file = fopen(fileNameBuf, "w");
        if (file != NULL)
        {
            char path[MAX_PATH];
            if (mode == CMD_VIEWINFO)
            {
                DumpDirItemInfo(file, di);
                strcpy(path, Path);
                SalamanderGeneral->SalPathAppend(path, di->FileName->FNName, sizeof(path));
                DumpExistingFileLayout(file, path, &Volume);
            }

            if (mode == CMD_DUMPFILES)
            {
                fprintf(file, "Existing files, see DumpSpecifiedFiles() for filter\n");
                strcpy(path, Root);
                DumpSpecifiedFiles(file, Snapshot->Root, path, sizeof(path));
            }

            if (mode == CMD_COMPAREFILES)
            {
                fprintf(file, "Test Undelete on Existing files\n");
                strcpy(path, Root);
                DWORD totalCount = 0;
                TestUndeleteOnExistingFiles(file, Snapshot->Root, path, sizeof(path), &totalCount);

                // open progress
                HWND mainwnd = SalamanderGeneral->GetMainWindowHWND();
                CCopyProgressDlg dlg(mainwnd, ooStatic);
                dlg.Create();
                EnableWindow(mainwnd, FALSE);
                HProgressDlg = dlg.HWindow; // redirect message box parent
                SetForegroundWindow(dlg.HWindow);
                hErrParent = dlg.HWindow;
                Progress = &dlg;
                TotalProgress = FileProgress = 0;
                GrandTotal = totalCount;

                TestUndeleteOnExistingFiles(file, Snapshot->Root, path, sizeof(path), NULL);

                // close progress
                EnableWindow(mainwnd, TRUE);
                HProgressDlg = NULL;
                DestroyWindow(dlg.HWindow);
            }

            fprintf(file, "\nDone\n");
            fclose(file);

            CSalamanderPluginInternalViewerData viewerData;
            viewerData.Size = sizeof(viewerData);
            viewerData.FileName = fileNameBuf;
            viewerData.Mode = 0; // text mode
            viewerData.Caption = "Output";
            viewerData.WholeCaption = FALSE;
            int err;
            BOOL ok = SalamanderGeneral->ViewFileInPluginViewer(NULL, &viewerData, TRUE, tempdir, "test.txt", err);
        }
        else
        {
            TRACE_E("ViewUnderlyingFileSystemData() Creating temp file failed.");
        }
    }
}

void CPluginFSInterface::FragmentFile(HWND parent, const DIR_ITEM_I<char>* di, const char* fullPath)
{
    HANDLE hVolume = NULL;
    HANDLE hFile = NULL;
    // get volume GUID
    char sourcePanelGUIDPath[MAX_PATH];
    sourcePanelGUIDPath[0] = 0;
    if (!SalamanderGeneral->GetResolvedPathMountPointAndGUID(Root, NULL, sourcePanelGUIDPath))
    {
        TRACE_E("GetResolvedPathMountPointAndGUID() failed on " << Root);
        return;
    }
    SalamanderGeneral->SalPathRemoveBackslash(sourcePanelGUIDPath);

    // open volume
    hVolume = CreateFile(sourcePanelGUIDPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVolume == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        TRACE_E("Cannot open volume " << Root);
        return;
    }

// get volume bitmap
#define BITMAP_BUFFER_SIZE 1024
    BYTE buffer[BITMAP_BUFFER_SIZE];
    VOLUME_BITMAP_BUFFER* bitmap = (VOLUME_BITMAP_BUFFER*)buffer;
    STARTING_LCN_INPUT_BUFFER startingLcn = {0};
    DWORD bytesReturned;
    BOOL ret;

    LONGLONG freeLCN = -1;
    // get bitmap size
    ret = DeviceIoControl(hVolume, FSCTL_GET_VOLUME_BITMAP,
                          &startingLcn, sizeof(STARTING_LCN_INPUT_BUFFER),
                          bitmap, sizeof(VOLUME_BITMAP_BUFFER),
                          &bytesReturned, NULL);
    DWORD err = GetLastError();
    if (err != ERROR_MORE_DATA)
    {
        TRACE_E("FSCTL_GET_VOLUME_BITMAP failed: " << err);
        goto exit;
    }

    // read end of bitmap
    startingLcn.StartingLcn.QuadPart = bitmap->BitmapSize.QuadPart - (BITMAP_BUFFER_SIZE - sizeof(VOLUME_BITMAP_BUFFER)) * 8;
    ret = DeviceIoControl(hVolume, FSCTL_GET_VOLUME_BITMAP,
                          &startingLcn, sizeof(STARTING_LCN_INPUT_BUFFER),
                          bitmap, BITMAP_BUFFER_SIZE,
                          &bytesReturned, NULL);
    if (!ret)
    {
        err = GetLastError();
        TRACE_E("FSCTL_GET_VOLUME_BITMAP failed: " << err);
        goto exit;
    }

    // find free cluster, use returned re-alligned starting LCN
    for (int i = 0; freeLCN == -1 && i < BITMAP_BUFFER_SIZE - sizeof(VOLUME_BITMAP_BUFFER) - 32; i++)
    {
        if (bitmap->Buffer[i] != 0xFF)
        {
            BYTE b = bitmap->Buffer[i];
            for (int j = 0; freeLCN == -1 && j < 8; j++)
            {
                if ((b & (0x01 << j)) == 0)
                {
                    freeLCN = bitmap->StartingLcn.QuadPart + j;
                }
            }
        }
    }
    if (freeLCN == -1)
    {
        TRACE_E("End of volume is FULL, try to enlarge BITMAP_BUFFER_SIZE const!");
        goto exit;
    }

    // fragment required file: move first cluster of file to freeLCN cluster

    // open volume
    hFile = CreateFile(fullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == NULL)
    {
        TRACE_E("Cannot open file " << hFile);
        goto exit;
    }

    MOVE_FILE_DATA move;
    move.StartingVcn.QuadPart = 0;
    move.StartingLcn.QuadPart = freeLCN;
    move.ClusterCount = 1;
    move.FileHandle = hFile;
    ret = DeviceIoControl(hVolume, FSCTL_MOVE_FILE,
                          &move, sizeof(move),
                          NULL, 0,
                          &bytesReturned, NULL);
    if (!ret)
    {
        err = GetLastError();
        TRACE_E("FSCTL_MOVE_FILE failed: " << err);
        goto exit;
    }

    TRACE_I("SUCCESS, file was fragmented: " << fullPath);

exit:
    if (hFile != NULL)
        CloseHandle(hFile);
    if (hVolume != NULL)
        CloseHandle(hVolume);
}

BOOL EnablePrivileges()
{
    HANDLE token;
    struct
    {
        DWORD count;
        LUID_AND_ATTRIBUTES privilege[1];
    } token_privileges;
    token_privileges.count = 1;
    token_privileges.privilege[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValue(0, SE_MANAGE_VOLUME_NAME, &token_privileges.privilege[0].Luid))
        return FALSE;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
        return FALSE;
    if (!AdjustTokenPrivileges(token, 0, (PTOKEN_PRIVILEGES)&token_privileges, 0, 0, 0))
        return FALSE;
    DWORD err = GetLastError();
    if (err != ERROR_SUCCESS)
        return FALSE;
    return TRUE;
}

void CPluginFSInterface::SetFileValidData(HWND parent, const DIR_ITEM_I<char>* di, const char* fullPath)
{
    // enable SE_MANAGE_VOLUME_NAME privilege for SetFileValidData()
    if (!EnablePrivileges())
        TRACE_E("EnablePrivileges() failed");

    bool ret = false;
    DWORD enlarge = 100000;
    LONGLONG size{};
    DWORD loSize{};

    HANDLE hFile = CreateFile(fullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == NULL)
    {
        TRACE_E("CreateFile() failed on " << fullPath);
        goto exit;
    }
    DWORD hiSize;
    loSize = ::GetFileSize(hFile, &hiSize);
    if (loSize == INVALID_FILE_SIZE)
    {
        TRACE_E("GetFileSize() failed on " << fullPath);
        goto exit;
    }
    size = MAKEQWORD(loSize, hiSize);
    if (size < 2)
    {
        TRACE_E("File is too small for SetFileValidData() test " << fullPath);
        goto exit;
    }

    LARGE_INTEGER offset;
    offset.QuadPart = enlarge;
    if (!SetFilePointerEx(hFile, offset, NULL, FILE_END))
    {
        TRACE_E("SetFilePointerEx() failed " << fullPath);
        goto exit;
    }

    if (!SetEndOfFile(hFile))
    {
        TRACE_E("SetEndOfFile() failed " << fullPath);
        goto exit;
    }

    ret = ::SetFileValidData(hFile, size + enlarge - enlarge / 2);
    if (!ret)
    {
        DWORD err = GetLastError();
        TRACE_E("SetFileValidData() failed on " << fullPath << " err=" << err);
        goto exit;
    }

exit:
    if (hFile != NULL)
        CloseHandle(hFile);
}

void CPluginFSInterface::SetFileSparse(HWND parent, const DIR_ITEM_I<char>* di, const char* fullPath)
{
    HANDLE hFile = CreateFile(fullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    DWORD hiSize{};
    DWORD loSize{};
    LONGLONG size{};
    BOOL ret = false;
    if (hFile == NULL)
    {
        TRACE_E("CreateFile() failed on " << fullPath);
        goto exit;
    }

    DWORD bytes;
    ret = DeviceIoControl(hFile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &bytes, NULL);
    if (!ret)
    {
        DWORD err = GetLastError();
        TRACE_E("FSCTL_SET_SPARSE failed: " << err);
        goto exit;
    }

    loSize = ::GetFileSize(hFile, &hiSize);
    if (loSize == INVALID_FILE_SIZE)
    {
        TRACE_E("GetFileSize() failed on " << fullPath);
        goto exit;
    }
    size = MAKEQWORD(loSize, hiSize);
    if (size < 70000)
    {
        TRACE_E("File is too small for sparse test " << fullPath);
        goto exit;
    }

    FILE_ZERO_DATA_INFORMATION FileZeroData;
    FileZeroData.FileOffset.QuadPart = 20000;
    FileZeroData.BeyondFinalZero.QuadPart = 50000;
    ret = DeviceIoControl(hFile, FSCTL_SET_ZERO_DATA,
                          &FileZeroData, sizeof(FILE_ZERO_DATA_INFORMATION),
                          NULL, 0, &bytes, NULL);
    if (!ret)
    {
        DWORD err = GetLastError();
        TRACE_E("FSCTL_SET_ZERO_DATA failed: " << err);
        goto exit;
    }

exit:
    if (hFile != NULL)
        CloseHandle(hFile);
}

#endif _DEBUG
