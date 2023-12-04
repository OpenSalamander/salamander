// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "selfextr/comdefs.h"

//types defintions

//basic types
typedef unsigned __int8 __UINT8;
typedef unsigned __int16 __UINT16;
typedef unsigned __int32 __UINT32;
typedef unsigned __int64 __UINT64;
typedef signed __int64 __INT64;

#ifndef QWORD
typedef unsigned __int64 QWORD;
typedef QWORD* LPQWORD;
#define LODWORD(qw) ((DWORD)qw)
#define HIDWORD(qw) ((DWORD)(qw >> 32))
#define MAKEQWORD(lo, hi) (((QWORD)hi << 32) + lo)
#endif

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
    union
    {
        CEOCentrDirRecord _EOCentrDirRecord;
        struct //CEOCentrDirRecord
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
        };
    };
    __UINT8* Comment; //zipfile comment (variable size)
} CEOCentrDirRecordEx;

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

typedef struct
{
    __UINT32 Signature; //0x08074b50
    __UINT32 Crc;       //crc-32
    __UINT32 CompSize;  //compressed size
    __UINT32 Size;      //uncompressed size
} CDataDescriptor;

typedef struct
{
    __UINT32 Signature; // 0x08074b50
    __UINT32 Crc;       // crc-32
    __UINT64 CompSize;  // compressed size
    __UINT64 Size;      // uncompressed size
} CZip64DataDescriptor;

typedef struct
{
    __UINT32 Signature; // 0x06064b50
    __UINT64 Size;      // size of zip64 end of central
    // directory record, excluding Signature and Size,
    // possibly including CZip64EOCentrDirRecordVer2
    __UINT16 Version;          // version made by
    __UINT16 VersionExtr;      // version needed to extract
    __UINT32 DiskNum;          // number of this disk
    __UINT32 StartDisk;        // number of the disk with the
                               // start of the central directory
    __UINT64 DiskTotalEntries; // total number of entries in the
                               // central directory on this disk
    __UINT64 TotalEntries;     // total number of entries in the
                               // central directory
    __UINT64 CentrDirSize;     // size of the central directory
    __UINT64 CentrDirOffs;     // offset of start of central
    // directory with respect to
    // the starting disk number
    // [variable size] zip64 extensible data sector
} CZip64EOCentrDirRecord;

typedef struct // May immediatelly follow (actually be included in) CZip64EOCentrDirRecord
{
    __UINT16 Method;     // compression method
    __UINT64 CompSize;   // compressed size
    __UINT64 Size;       // uncompressed size
    __UINT16 AlgID;      // Encryption algorithm ID - see ALGID_xxx
    __UINT16 BitLen;     // Encryption key length
    __UINT16 Flags;      // Encryption flags - see SEH_FLAG_xxx
    __UINT16 HashID;     // Hash algorithm identifier
    __UINT16 HashLength; // Length of hash data
    //  __UINT8  HashData[0]; // Hash data
} CZip64EOCentrDirRecordVer2;

typedef struct
{
    __UINT32 Signature;      // 0x07064b50
    __UINT32 Zip64StartDisk; // number of the disk with the
                             // start of the zip64 end of
                             // central directory
    __UINT64 Zip64Offset;    // relative offset of the zip64
                             // end of central directory record
    __UINT32 TotalDisks;     // total number of disks
} CZip64EOCentrDirLocator;

typedef struct
{
    __UINT16 Tag;        // 0x0017  Tag for this "extra" block type
    __UINT16 TSize;      // Size of data that follows
    __UINT16 Format;     // Format definition for this record
    __UINT16 AlgID;      // Encryption algorithm identifier - see ALGID_xxx
    __UINT16 Bitlen;     // Bit length of encryption key
    __UINT16 Flags;      // Processing flags - see SEH_FLAG_xxx
    __UINT8 CertData[1]; // Actually TSize-8 bytes Certificate decryption extra field data
} CStrongEncryptionHeader;

typedef struct
{
    __UINT16 HeaderID; // Extra field header ID (0x9901)
    __UINT16 DataSize; // Data size (currently 7)
    __UINT16 Version;  // Integer version number specific to the zip vendor
    __UINT16 VendorID; // 2-character vendor ID
    __UINT8 Strength;  // Integer mode value indicating AES encryption strength
    __UINT16 Method;   // The actual compression method used to compress the
                       // file
} CAESExtraField;

typedef struct
{
    __UINT16 HeaderID;      // Extra field header ID (0x0001)
    __UINT16 DataSize;      // Data size (16 or 24)
    __UINT64 CompSize;      // Compressed size
    __UINT64 Size;          // Uncompressed size
    __UINT64 LocHeaderOffs; // Relative offset of local header
    __UINT32 StartDisk;     // Disk Start Number
} CZip64ExtraField;

#pragma pack(pop)

#define IF_STORED_AES 1 // File is stored and AES-encrypted

struct CFileInfo
{
    unsigned NameLen;   //filename length
    char* Name;         //filename (variable size)
    DWORD FileAttr;     //external file attributes
    FILETIME LastWrite; //last mod file time
    unsigned IsDir;
    unsigned Flag;      //general purpose bit flag
    unsigned InterAttr; //internal file attributes
    unsigned Method;    //compression method
    unsigned Crc;       //crc-32
    QWORD CompSize;     //compressed size
    QWORD Size;         //uncompressed size
    //unsigned ExtraLen;           //extra field length
    //char *   Extra;              //extra field (variable size)
    //unsigned CommentLen;         //file comment length
    //char *   Comment;            //file comment (variable size)
    QWORD LocHeaderOffs; //relative offset of local header
    QWORD DataOffset;    //relative offset of file data
    int StartDisk;       //disk number start
    int InternalFlags;   // Internal flags used by the plugin, IF_xxx

    CFileInfo() { Name = NULL; }
    ~CFileInfo()
    {
        if (Name)
            free(Name);
    }
};

struct CSfxSettings
{
    unsigned Flags;
    char Command[SE_MAX_COMMANDLINE];
    char SfxFile[MAX_PATH];
    char Text[SE_MAX_TEXT];
    char Title[SE_MAX_TITLE];
    char TargetDir[2 * MAX_PATH];
    char ExtractBtnText[SE_MAX_EXTRBTN];
    char Vendor[SE_MAX_VENDOR];
    char WWW[SE_MAX_WWW];
    char IconFile[MAX_PATH];
    DWORD IconIndex;
    UINT MBoxStyle;
    char* MBoxText;
    char MBoxTitle[SE_MAX_TITLE];
    char WaitFor[MAX_PATH];

    CSfxSettings()
    {
        Flags = SE_SHOWSUMARY;
        *Command = 0;
        *SfxFile = 0;
        *Text = 0;
        *Title = 0;
        *TargetDir = 0;
        *ExtractBtnText = 0;
        *Vendor = 0;
        *WWW = 0;
        *IconFile = 0;
        IconIndex = 0;
        MBoxStyle = MB_OK;
        MBoxText = NULL;
        *MBoxTitle = 0;
        *WaitFor = 0;
    }

    CSfxSettings(const CSfxSettings& origin)
    {
        Flags = origin.Flags;
        strcpy(Command, origin.Command);
        strcpy(SfxFile, origin.SfxFile);
        strcpy(Text, origin.Text);
        strcpy(Title, origin.Title);
        strcpy(TargetDir, origin.TargetDir);
        strcpy(ExtractBtnText, origin.ExtractBtnText);
        strcpy(Vendor, origin.Vendor);
        strcpy(WWW, origin.WWW);
        strcpy(IconFile, origin.IconFile);
        IconIndex = origin.IconIndex;
        MBoxStyle = origin.MBoxStyle;
        MBoxText = DupStr(origin.MBoxText);
        strcpy(MBoxTitle, origin.MBoxTitle);
        strcpy(WaitFor, origin.WaitFor);
    }

    ~CSfxSettings()
    {
        if (MBoxText)
            free(MBoxText);
    }

    CSfxSettings& operator=(const CSfxSettings& origin)
    {
        Flags = origin.Flags;
        strcpy(Command, origin.Command);
        strcpy(SfxFile, origin.SfxFile);
        strcpy(Text, origin.Text);
        strcpy(Title, origin.Title);
        strcpy(TargetDir, origin.TargetDir);
        strcpy(ExtractBtnText, origin.ExtractBtnText);
        strcpy(Vendor, origin.Vendor);
        strcpy(WWW, origin.WWW);
        strcpy(IconFile, origin.IconFile);
        IconIndex = origin.IconIndex;
        MBoxStyle = origin.MBoxStyle;
        if (MBoxText)
            free(MBoxText);
        MBoxText = DupStr(origin.MBoxText);
        strcpy(MBoxTitle, origin.MBoxTitle);
        strcpy(WaitFor, origin.WaitFor);
        return *this;
    }

    char* SetMBoxText(const char* text)
    {
        if (MBoxText)
            free(MBoxText);
        MBoxText = DupStr(text);
        return MBoxText;
    }

private:
    char* DupStr(const char* str)
    {
        if (str)
        {
            char* ret = (char*)malloc(strlen(str) + 1);
            strcpy(ret, str);
            return ret;
        }
        else
            return NULL;
    }
};

//constants

extern const SYSTEMTIME MaxZipTime;
extern const SYSTEMTIME MinZipTime;

#define MIN_VOLSIZE 1024
#define SMALL_VOLSIZE 256 * 1024
#define MAX_VOL_STR 32

//signatures
#define SIG_LOCALFH 0x04034b50         //local file header signature
#define SIG_DATADESCR 0x08074b50       //data descriptor signature
#define SIG_CENTRFH 0x02014b50         //central file header signature
#define SIG_EOCENTRDIR 0x06054b50      //end of central dir signature
#define SIG_ZIP64EOCENTRDIR 0x06064b50 //zip64 end of central dir
#define SIG_ZIP64LOCATOR 0x07064b50    //zip64 end of central dir locator

#define ZIP64_HEADER_ID 0x0001 //zip64 extra header id

//general purose bit flag masks
#define GPF_ENCRYPTED 0x01 //indicate that file is encrypted
#define GPF_DATADESCR 0x08 //compressed data are followed by data descriptor
#define GPF_UTF8 0x800     //the filename and comment fields for this file are encoded in UTF-8 \
                           // extended language encoding data may (must?) be present

//compression methods
#define CM_STORED 0
#define CM_SHRINKED 1
#define CM_REDUCED1 2
#define CM_REDUCED2 3
#define CM_REDUCED3 4
#define CM_REDUCED4 5
#define CM_IMPLODED 6
#define CM_DEFLATED 8
#define CM_DEFLATE64 9
#define CM_BZIP2 12
#define CM_AES 99

#define HS_FAT 00
#define HS_UNIX 03
#define HS_VM_CMS 04
#define HS_HPFS 06
#define HS_NTFS 11
#define HS_ACORN 13
#define HS_VFAT 14
#define HS_MVS 15

// Used in CStrongEncryptionHeader and CZip64EOCentrDirRecordVer2
#define ALGID_DES 0x6601
#define ALGID_RC2_OLD 0x6602 // version needed to extract < 5.2
#define ALGID_3DES_168 0x6603
#define ALGID_3DES_112 0x6609
#define ALGID_AES_128 0x660E
#define ALGID_AES_192 0x660F
#define ALGID_AES_256 0x6610
#define ALGID_RC2 0x6702 // version needed to extract >= 5.2
#define ALGID_BLOWFISH 0x6720
#define ALGID_TWOFISH 0x6721
#define ALGID_RC4 0x6801
#define ALGID_UNKNOWN 0xFFFF

// Used in CStrongEncryptionHeader and CZip64EOCentrDirRecordVer2
#define SEH_FLAG_PWD_REQUIRED 1 // Password is required to decrypt
#define SEH_FLAG_CERTS_ONLY 2   // Certificates only
#define SEH_FLAG_CERT_OR_PWD 3  // Password or certificate required to decrypt

#define VN_NEED_TO_EXTR(method) (method == CM_STORED ? 10 : (method == CM_AES ? 51 : 20))
#define VN_MADE_BY 22 //like InfoZip previously used by SS
#define VN_ZIP64 45

#define MIN_ZIP64_VERSION 45

//AES compression
#define AES_HEADER_ID 0x9901
#define AES_NONVENDOR_ID 0x4541
#define AES_VERSION_1 0x0001
#define AES_VERSION_2 0x0002 // CRC is not filled/used
#define AES_MAXHMAC SAL_AES_MAC_LENGTH(3)

//buffers sizes
#define SLIDE_WINDOW_SIZE (64 * 1024)
// pozor DECOMPRESS_INBUFFER_SIZE musi byt nasobkem 16ti bytu
// kvuli AES knihovne
#define DECOMPRESS_INBUFFER_SIZE (16 * 1024)
#define MAX_HEADER_SIZE (64 * 1024)
#define OUTPUT_BUFFER_SIZE (16 * 1024)
#define INPUT_BUFFER_SIZE (5 * 16 * 1024)

//decompression error codes
#define DEC_NOERROR 0
#define DEC_SKIP 2
#define DEC_CANCEL 3
#define DEC_RETRY 4
#define DEC_REOPEN_ZIP 5

//error codes for file IO functions
#define IOERR_NOERROR 0
#define IOERR_SKIP 1
#define IOERR_SKIPALL 2
#define IOERR_CANCEL 3
//#define IOERR_LOWMEM;  = 4;
#define IOERR_PERNAMENT 5
#define IOERR_DIALOGFAIL 6
#define IOERR_BADDATA 7
//flags
//#define IO_USE_SIMPLE_DLG 0x1
//#define IO_SKIPALL        0x2
//#define IO_NORETRY        0x4

//error codes returned by ProcessError method
#define ERR_NOERROR 0
#define ERR_SKIP 1
#define ERR_RETRY 2
#define ERR_CANCEL 3
#define ERR_LOWMEM 4

//flags for ProcessError
#define PE_NOSKIP 0x1
#define PE_NORETRY 0x2
#define PE_QUIET 0x4

//all files attributes in ZIP are masked before usage
//also when compressing atrrributes are masked before stored
//this preserve displaying some confusing attributes in archive,
//like 'compressed' or 'temporary'

/* winnt.h
#define FILE_ATTRIBUTE_READONLY             0x00000001  
#define FILE_ATTRIBUTE_HIDDEN               0x00000002  
#define FILE_ATTRIBUTE_SYSTEM               0x00000004  
#define FILE_ATTRIBUTE_DIRECTORY            0x00000010  
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020  
*/

#define FILE_ATTTRIBUTE_MASK (0x37)

#define MAX_ZIPCOMMENT 0xFFFF

#define SFX_SUPPORTEDVERSION 5
