// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//types

typedef unsigned __UINT32;
typedef unsigned short __UINT16;
typedef unsigned char __UINT8;

#pragma pack(push)
#pragma pack(1)

typedef struct
{
    __UINT32 Signature;        //0x06054b50
    __UINT16 DiskNum;          //number of this disk
    __UINT16 StartDisk;        //number of the disk with the
                               //start of the central directory
    __UINT16 DiskTotalEntries; //total number of entries in
                               //the central dir on this disk
    __UINT16 TotalEntries;     //total number of entries in
                               //the central dir
    __UINT32 CentrDirSize;     //size of the central directory
    __UINT32 CentrDirOffs;     //offset of start of central
                               //directory with respect to
                               //the starting disk number
    __UINT16 CommentLen;       //zipfile comment length
} CEOCentrDirRecord;

typedef struct
{
    __UINT32 Signature;     //0x02014b50
    __UINT16 Version;       //version made by
    __UINT16 VersionExtr;   //version needed to extract
    __UINT16 Flag;          //general purpose bit flag
    __UINT16 Method;        //compression method
    __UINT16 Time;          //last mod file time
    __UINT16 Date;          //last mod file date
    __UINT32 Crc;           //crc-32
    __UINT32 CompSize;      //compressed size
    __UINT32 Size;          //uncompressed size
    __UINT16 NameLen;       //filename length
    __UINT16 ExtraLen;      //extra field length
    __UINT16 CommentLen;    //file comment length
    __UINT16 StartDisk;     //disk number start
    __UINT16 InterAttr;     //internal file attributes
    __UINT32 ExternAttr;    //external file attributes
    __UINT32 LocHeaderOffs; //relative offset of local header
} CFileHeader;

typedef struct
{
    __UINT32 Signature;   //0x04034b50
    __UINT16 VersionExtr; //version needed to extract
    __UINT16 Flag;        //general purpose bit flag
    __UINT16 Method;      //compression method
    __UINT16 Time;        //last mod file time
    __UINT16 Date;        //last mod file date
    __UINT32 Crc;         //crc-32
    __UINT32 CompSize;    //compressed size
    __UINT32 Size;        //uncompressed size
    __UINT16 NameLen;     //filename length
    __UINT16 ExtraLen;    //extra field length
} CLocalFileHeader;

#pragma pack(pop)

//constants

//size of crc tab in the memory
#define CRC_TAB_SIZE 256
//initial value of crc shift register
#define INIT_CRC 0L

//signatures
#define SIG_LOCALFH 0x04034b50    //local file header signature
#define SIG_DATADESCR 0x08074b50  //data descriptor signature
#define SIG_CENTRFH 0x02014b50    //central file header signature
#define SIG_EOCENTRDIR 0x06054b50 //end of central dir signature

//general purose bit flag masks
#define GPF_ENCRYPTED 0x01 //indicate that file is encrypted
#define GPF_DATADESCR 0x08 //compressed data are followed by data descriptor

//compression methods
#define CM_STORED 0
#define CM_SHRINKED 1
#define CM_REDUCED1 2
#define CM_REDUCED2 3
#define CM_REDUCED3 4
#define CM_REDUCED4 5
#define CM_IMPLODED 6
#define CM_DEFLATED 8

#define HS_FAT 00
#define HS_VM_CMS 04
#define HS_HPFS 06
#define HS_NTFS 11
#define HS_ACORN 13
#define HS_VFAT 14
#define HS_MVS 15

#define VN_NEED_TO_EXTR(method) (method == CM_STORED ? 10 : 20)
#define VN_MADE_BY 22 //like InfoZip previously used by SS

extern HANDLE ArchiveFile;
extern HANDLE ArchiveMapping;
extern const unsigned char* ArchiveBase;
extern CSelfExtrHeader* ArchiveStart;
extern unsigned long Size;
extern char TargetPath[];
extern bool Stop;
extern unsigned Silent;
extern HANDLE OutFile;
extern __UINT32 CrcTab[];
extern bool Extracting;
extern int ExtractedFiles;
extern int SkippedFiles;
extern __int64 ExtractedMB;

int GetFirstFile(CFileHeader** header, unsigned* totalEntries, char* name);
int GetNextFile(CFileHeader** header, char* name);
DWORD WINAPI ExtractThreadProc(LPVOID lpParameter);
void UpdateCrc(__UINT8* buffer, unsigned length);
int SafeWrite(HANDLE file, const unsigned char* buffer, unsigned size);
void* MemZero(void* dst, unsigned count);
void* MemCpy(void* dst, const void* src, unsigned count);

//***********************************************************************************
//
// Rutiny ze SHLWAPI.DLL
//

BOOL PathAppend(LPTSTR pPath, LPCTSTR pMore);
//BOOL PathAddExtension(LPTSTR pszPath, LPCTSTR pszExtension);
BOOL PathRemoveFileSpec(LPTSTR pszPath);
LPTSTR PathAddBackslash(LPTSTR pszPath);
LPTSTR StrRChr(LPCTSTR lpStart, LPCTSTR lpEnd, char wMatch);
LPTSTR StrChr(LPCTSTR lpStart, char wMatch);
LPTSTR PathRemoveBackslash(LPTSTR pszPath);
BOOL PathStripToRoot(LPTSTR pszPath);

//************************************************************************************

BOOL PathMerge(char* fullPath, const char* relativePath);
char* NumberToStr(char* buffer, const __int64 number);
HANDLE CheckAndCreateDirectory(char* name, bool noSkip = false);
int GetRootLen(const char* path);

// protoze windowsova verze GetFileAttributes neumi pracovat se jmeny koncicimi mezerou/teckou,
// napsali jsme si vlastni (u techto jmen pridava backslash na konec, cimz uz pak
// GetFileAttributes funguje spravne, ovsem jen pro adresare, pro soubory s mezerou/teckou na
// konci reseni nemame, ale aspon se to nezjistuje od jineho souboru - windowsova verze
// orizne mezery/tecky a pracuje tak s jinym souborem/adresarem)
DWORD SalGetFileAttributes(const char* fileName);
