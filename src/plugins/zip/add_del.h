// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//action flags

#define AF_ADD 0       //add file and delete it if moving files to zip
#define AF_DEL 1       //don't add file, but delete it if moving files to zip \
                       //does matter only on directories
#define AF_NOADD 2     //don't add file and don't delete it if moving files to zip
#define AF_OVERWRITE 3 //for unix files when ovewriting, same as AF_ADD

struct CAddInfo
{
    char* Name;
    int NameLen;
    bool IsDir;
    CQuadWord Size;
    __UINT16 InterAttr,
        Flag;
    int Method;
    DWORD FileAttr;
    FILETIME LastWrite;
    __UINT64 CompSize;
    unsigned Crc;
    __UINT64 LocHeaderOffs;
    unsigned StartDisk;
    int Action;        //flag, see above
    int InternalFlags; // IF_xxx

    CAddInfo() { Name = NULL; }
    ~CAddInfo()
    {
        if (Name)
            free(Name);
    }
};

#define FPR_NORMAL 0
#define FPR_SFXRESERVE 1
#define FPR_SFXEND 2
#define FPR_WRITE 3

typedef TIndirectArray2<char> TIndirectArray2_char_;

enum CSfxSettingsComments
{
    SFX_COMMENT_HEAD,
    SFX_COMMENT_VERSION,
    SFX_COMMENT_TARGDIR,
    SFX_COMMENT_ALLOWCHANGE,
    SFX_COMMENT_REMOVE,
    SFX_COMMENT_AUTO,
    SFX_COMMENT_SUMMARY,
    SFX_COMMENT_HIDE,
    SFX_COMMENT_OVERWRITE,
    SFX_COMMENT_AUTODIR,
    SFX_COMMENT_COMMAND,
    SFX_COMMENT_PACKAGE,
    SFX_COMMENT_MBUT,
    SFX_COMMENT_MICO,
    SFX_COMMENT_MBOX,
    SFX_COMMENT_TEXT,
    SFX_COMMENT_TITLE,
    SFX_COMMENT_BUTTON,
    SFX_COMMENT_VENDOR,
    SFX_COMMENT_WWW,
    SFX_COMMENT_ICOFILE,
    SFX_COMMENT_ICOINDEX,
    SFX_COMMENT_WAITFOR,
    SFX_COMMENT_REQUIRESADMIN
};

class CZipPack : public CZipCommon
{
public:
    //int                 ErrorID;
    TIndirectArray2<CFileInfo> DelFiles; //file header of files to be extracted
    //unsigned            AssumedDelProgress;
    CEOCentrDirRecordEx EONewCentrDir; //end of central directory record
    QWORD NewCentrDirSize;
    QWORD NewCentrDirOffs;
    char* NewCentrDir;
    DWORD ZipAttr;
    CFile* TempFile;
    char TempName[MAX_PATH + 1];
    //bool                Backup;
    CQuadWord AddTotalSize;
    int SizeToAdd;
    const char* SourcePath;
    int SourceLen;
    TIndirectArray2<CAddInfo> AddFiles;
    bool SkipAllIOErrors;
    CFile* SourFile;
    __UINT32 Crc;
    bool Move;
    bool Pack; //flag indicating pack operation
    bool NothingToDo;
    __UINT32 Keys[3]; //decryption keys
    //bool                NoFreeDirs;
    CExtendedOptions Options;
    bool RecoverOK;
    FILETIME NewestFileTime;

    //multi-volume archives
    bool OverwriteAll;
    bool IgnoreAllFreeSp;
    QWORD DiskSize;

    //self-extracting archives
    unsigned ArchiveHeaderOffs;
    QWORD ArchiveDataOffs;
    BOOL SeccondPass;

    //delete from archive

    CZipPack(const char* zipName, const char* zipRoot,
             CSalamanderForOperationsAbstract* salamander) : CZipCommon(zipName, zipRoot, salamander, NULL), DelFiles(256),
                                                             AddFiles(256)
    {
        RecoverOK = true;
        DiskNum = 0;
        Extract = false;
        Options.Icons = NULL;
        NewestFileTime.dwLowDateTime = 0;
        NewestFileTime.dwHighDateTime = 0;
        SeccondPass = FALSE;
    }

    ~CZipPack()
    {
        if (Options.Icons)
            DestroyIcons(Options.Icons, Options.IconsCount);
    }

    int DeleteFromArchive(SalEnumSelection next, void* param);
    int PackToArchive(BOOL move, const char* sourcePath,
                      SalEnumSelection2 next, void* param);
    int PackNormal(SalEnumSelection2 next, void* param);
    int PackMultiVol(SalEnumSelection2 next, void* param);
    int PackSelfExtract(SalEnumSelection2 next, void* param);
    int CountFilesInRoot(int* filesInRoot, bool* rootExist);
    int DeleteFiles(int* deletedFiles);
    int MoveData(QWORD writePos, QWORD readPos, QWORD moveSize, char* buffer);
    void UpdateCentrDir(CFileInfo* curFile, CFileInfo* nextFile, QWORD delta);
    int WriteCentrDir();
    void AddAESExtraField(CFileInfo* fileInfo, CAESExtraField* extraAES, __UINT16* pExtraLen);
    int ExportLocalHeader(CFileInfo* fileInfo, char* buffer);
    int WriteLocalHeader(CFileInfo* fileInfo, char* buffer);
    int WriteDataDecriptor(CFileInfo* fileInfo);
    int WriteCentralHeader(CFileInfo* fileInfo, char* buffer, BOOL first, int reason);
    int WriteEOCentrDirRecord();
    int ExportName(char* destName, CFileInfo* fileInfo);
    int CreateTempFile();
    void NTFSCompressFile(HANDLE file);
    int EnumFiles2(SalEnumSelection2 next, void* param);
    int LoadCentralDirectory();
    int MatchFiles(int& count); // count je predpokladany pocet souboru po operaci
    int BackupZip();
    int PackFiles();
    int FinishPack(int reason = FPR_NORMAL);
    int GetDirInfo(const char* name, DWORD* attr, FILETIME* lastWrite);
    int IsDirectoryEmpty(const char* name);
    int InsertDir(char* dir, TIndirectArray2<TIndirectArray2_char_>& table);
    int CleanUpSource();
    int LoadExPackOptions(unsigned flags);
    void Recover();
    int Store(__UINT64* size);
    int CreateNextFile(bool firstSfxDisk = false);
    int NextDisk();
    int MatchAll();
    int WriteSfxExecutable(const char* sfxFile, const char* sfxPackage, BOOL preview, int progressMode);
    BOOL WriteSFXHeader(const char* archName, QWORD eoCentrDirOffs, DWORD archSize);
    /*
    bool ChangeSfxIcon(const char * sfxFile);
    int LoadIcons(const char * iconFile, DWORD index, CIcon **icons, int * count);
    void DestroyIcons(CIcon * icons, int count);
    CIcon * LoadIconsFromDirectory(HINSTANCE module, LPICONDIR directory, bool isIco);
    LPICONDIR LoadIconDirectoryByResName(HINSTANCE module, LPTSTR lpszName);
    LPICONDIR LoadIconDirectoryFromEXE(HINSTANCE module, DWORD index);
    LPVOID LoadIconDataFromEXE(HINSTANCE module, DWORD id);
    LPICONDIR LoadIconDirectoryFromICO(CFile * icoFile);
    LPVOID LoadIconDataFromICO(CFile * icoFile, DWORD offset, DWORD size);
    //void LoadIDsList(HINSTANCE module, CDynamicArray * IDsArray);
    */
    BOOL LoadDefaults();
    int CreateSFX();
    int CheckArchiveForSFXCompatibility();
    int CommentArchive();
    int SaveComment();
    BOOL ExportLongString(CFile* outFile, const char* string);
    BOOL WriteSFXComment(CFile* outFile, CSfxSettingsComments comment);
    BOOL ExportSFXSettings(CFile* outFile, CSfxSettings* settings);
    int FindNewestFile();
    int WriteSFXECRec(QWORD offset);
    int WriteSFXCentralDir();
};
