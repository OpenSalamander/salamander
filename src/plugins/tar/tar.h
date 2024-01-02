// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "names.h"
#include "dlldefs.h"

// possible return codes
#define TAR_OK 0
//#define TAR_FINISHED       1
#define TAR_ERROR 2
#define TAR_NOTAR 3
#define TAR_PREMATURE_END 4
#define TAR_EOF 5
//#define TAR_HEADERERR    6
//#define TAR_CHKSUMERR    7

//
// TAR constants
//
#define BLOCKSIZE 512

// Values used in typeflag field.
#define REGTYPE '0'   // regular file
#define AREGTYPE '\0' // regular file
#define LNKTYPE '1'   // link
#define SYMTYPE '2'   // reserved or GNU symbolic links
#define CHRTYPE '3'   // character special
#define BLKTYPE '4'   // block special
#define DIRTYPE '5'   // directory
#define FIFOTYPE '6'  // FIFO special
#define CONTTYPE '7'  // TODO // reserved or GNU contiguous files
#define CHRSPEC '8'   // character special (zkontrolovat nazev konstanty)
#define XHDTYPE 'x'   // Extended header referring to the next file in the archive
#define XGLTYPE 'g'   // Global extended header

// GNU extended types
#define GNUTYPE_DUMPDIR 'D'  // TODO // adresar hlavniho archivu v inkrementalnim
#define GNUTYPE_LONGLINK 'K' // TODO // link s dlouhym jmenem
#define GNUTYPE_LONGNAME 'L' // TODO // soubor s dlouhym jmenem
#define GNUTYPE_MULTIVOL 'M' // TODO
#define GNUTYPE_NAMES 'N'    // TODO
#define GNUTYPE_SPARSE 'S'   // TODO // ridky soubor
#define GNUTYPE_VOLHDR 'V'   // TODO

#define EXTRA_H_SPARSES 16
#define SPARSE_H_SPARSES 21
#define OLDGNU_H_SPARSES 4

// identifikace archivu
#define TMAGIC "ustar" // ustar and a null
#define TMAGLEN 6
#define OLDGNU_MAGIC "ustar  " // 7 chars and a null
#define OLDGNU_MAGLEN 8
#define TVERSION "00" // 00 and no null
#define TVERSLEN 2

//
// CPIO constants
//
#define CP_IFMT 0170000 // F000 Mask for all file type bits

#define CP_IFBLK 0060000  // 6000 Block device
#define CP_IFCHR 0020000  // 2000 Char device
#define CP_IFDIR 0040000  // 4000 Directory
#define CP_IFREG 0100000  // 8000 Regular file
#define CP_IFIFO 0010000  // 1000 Fifo
#define CP_IFLNK 0120000  // A000 Link
#define CP_IFSOCK 0140000 // C000 Socket
#define CP_IFNWK 0110000  // 9000 HP/UX network special

// archive file structures - disable data alignment
#pragma pack(push, 1)

//
// TAR structures
//

// popis pro ulozeni jednoho zaznamu z ridkeho souboru
struct TSparse
{                      // byte offset
    char offset[12];   //   0
    char numbytes[12]; //  12
                       //  24
};

// header podle specifikace POSIX 1003.1-1990
struct TPosixHeader
{                       // byte offset
    char name[100];     //   0
    char mode[8];       // 100
    char uid[8];        // 108
    char gid[8];        // 116
    char size[12];      // 124
    char mtime[12];     // 136
    char chksum[8];     // 148
    char typeflag;      // 156
    char linkname[100]; // 157
    char magic[6];      // 257
    char version[2];    // 263
    char uname[32];     // 265
    char gname[32];     // 297
    char devmajor[8];   // 329
    char devminor[8];   // 337
    char prefix[155];   // 345
                        // 500
};

// GNU extra header s daty, ktere nejsou v POSIX headeru. Je ulozen hned
// po POSIX headeru vzdy, kdyz typeflag je pismeno.
struct TExtraHeader
{                                // byte offset
    char atime[12];              //   0
    char ctime[12];              //  12
    char offset[12];             //  24
    char realsize[12];           //  36
    char longnames[4];           //  48
    char unused_pad1[68];        //  52
    TSparse sp[EXTRA_H_SPARSES]; // 120
    char isextended;             // 504
                                 // 505
};

// pokud nestaci zaznamy ridkeho souboru v GNU extra headeru, pripojuje se
// jeden nebo vice techto extra headeru s dalsimi zaznamy
struct TSparseHeader
{                                 // byte offset
    TSparse sp[SPARSE_H_SPARSES]; //   0
    char isextended;              // 504
                                  // 505
};

// old GNU header vyuziva POSIX prefix zaznamu pro extra data
struct TOldGnuHeader
{                                 // byte offset
    char unused_pad1[345];        //   0
    char atime[12];               // 345
    char ctime[12];               // 357
    char offset[12];              // 369
    char longnames[4];            // 381
    char unused_pad2;             // 385
    TSparse sp[OLDGNU_H_SPARSES]; // 386
    char isextended;              // 482
    char realsize[12];            // 483
                                  // 495
};

// blok tar archivu (obsahujici bud data nebo header nektereho z typu)
union TTarBlock
{
    unsigned char RawBlock[BLOCKSIZE];
    TPosixHeader Header;
    TExtraHeader ExtraHeader;
    TOldGnuHeader OldGnuHeader;
    TSparseHeader SparseHeader;
};

//
// CPIO structures
//

struct old_cpio_header
{
    unsigned short c_magic;
    short c_dev;
    unsigned short c_ino;
    unsigned short c_mode;
    unsigned short c_uid;
    unsigned short c_gid;
    unsigned short c_nlink;
    short c_rdev;
    unsigned short c_mtimes[2];
    unsigned short c_namesize;
    unsigned short c_filesizes[2];
    uint32_t c_mtime;    // Long-aligned copy of `c_mtimes'.
    uint32_t c_filesize; // Long-aligned copy of `c_filesizes'.
    char* c_name;
};

struct new_cpio_header
{
    unsigned short c_magic;
    uint32_t c_ino;
    uint32_t c_mode;
    uint32_t c_uid;
    uint32_t c_gid;
    uint32_t c_nlink;
    uint32_t c_mtime;
    uint32_t c_filesize;
    int32_t c_dev_maj;
    int32_t c_dev_min;
    int32_t c_rdev_maj;
    int32_t c_rdev_min;
    uint32_t c_namesize;
    uint32_t c_chksum;
    char* c_name;
    char* c_tar_linkname;
};

#pragma pack(pop)

/* "New" portable format and CRC format:

   Each file has a 110 byte header,
   a variable length, NUL terminated filename,
   and variable length file data.
   A header for a filename "TRAILER!!!" indicates the end of the archive.  */

/* All the fields in the header are ISO 646 (approximately ASCII) strings
   of hexadecimal numbers, left padded, not NUL terminated.

   Field Name Length in Bytes Notes
   c_magic    6               "070701" for "new" portable format
                              "070702" for CRC format
   c_ino      8
   c_mode     8
   c_uid      8
   c_gid      8
   c_nlink    8
   c_mtime    8
   c_filesize 8               must be 0 for FIFOs and directories
   c_maj      8
   c_min      8
   c_rmaj     8               only valid for chr and blk special files
   c_rmin     8               only valid for chr and blk special files
   c_namesize 8               count includes terminating NUL in pathname
   c_chksum   8               0 for "new" portable format; for CRC format
                              the sum of all the bytes in the file  */

enum EArchiveFormat
{
    e_CRCASCII,
    e_NewASCII,
    e_OldASCII,
    e_Binary,
    e_SwapBinary,
    e_TarPosix,
    e_TarOldGnu,
    e_TarV7,
    e_Unknown,
};

struct SCommonHeader
{
    SCommonHeader();
    ~SCommonHeader();
    void Initialize();

    // trvale polozky
    CFileData FileInfo;
    char* Path;
    CQuadWord Checksum;
    BOOL IsDir;
    EArchiveFormat Format;
    BOOL IsExtendedTar;
    BOOL Finished;
    BOOL Ignored;
    // docasne polozky, ktere se budou konvertovat do trvalych
    char* Name;
    CQuadWord Mode;
    CQuadWord MTime;
};

class CArchiveAbstract
{
public:
    virtual ~CArchiveAbstract(){};

    virtual BOOL ListArchive(const char* prefix, CSalamanderDirectoryAbstract* dir) = 0;
    virtual BOOL UnpackOneFile(const char* nameInArchive, const CFileData* fileData,
                               const char* targetPath, const char* newFileName) = 0;
    virtual BOOL UnpackArchive(const char* targetPath, const char* archiveRoot,
                               SalEnumSelection nextName, void* param) = 0;
    virtual BOOL UnpackWholeArchive(const char* mask, const char* targetPath) = 0;

    virtual BOOL IsOk() const = 0;
};

class CArchive : public CArchiveAbstract
{
private:
    CDecompressFile* Stream;
    CSalamanderForOperationsAbstract* SalamanderIf;
    BOOL Ok;
    CQuadWord Offset;
    DWORD Silent;

    BOOL ListStream(CSalamanderDirectoryAbstract* dir);
    BOOL UnpackStream(const char* targetPath, BOOL doProgress,
                      const char* nameInArchive, CNames* names, const char* newName);
    BOOL GetStreamHeader(SCommonHeader& header);

    int ReadArchiveHeader(SCommonHeader& header, BOOL probe);
    int WriteOutData(const SCommonHeader& header, const char* targetPath,
                     const char* nameInArchive, BOOL simulate, BOOL doProgress);

    int SkipBlockPadding(const SCommonHeader& header);
    void GetArchiveType(const unsigned char* buffer, const unsigned short preRead, SCommonHeader& header);
    int ReadNewASCIIHeader(SCommonHeader& header);
    int ReadOldASCIIHeader(SCommonHeader& header);
    int ReadBinaryHeader(SCommonHeader& header);
    int ReadTarHeader(const unsigned char* buffer, SCommonHeader& header);
    BOOL IsTarHeader(const unsigned char* buffer, BOOL& finished,
                     EArchiveFormat& format);
    int ReadCpioName(unsigned long namesize, SCommonHeader& header);
    void MakeFileInfo(const SCommonHeader& header, char* arcfiledata, char* arcfilename);

public:
    CArchive(LPCTSTR fileName, CSalamanderForOperationsAbstract* salamander, DWORD offset, CQuadWord inputSize);
    ~CArchive();

    BOOL IsOk() const override { return Ok; }

    BOOL ListArchive(const char* prefix, CSalamanderDirectoryAbstract* dir) override;
    BOOL UnpackOneFile(const char* nameInArchive, const CFileData* fileData,
                       const char* targetPath, const char* newFileName) override;
    BOOL UnpackArchive(const char* targetPath, const char* archiveRoot,
                       SalEnumSelection nextName, void* param) override;
    BOOL UnpackWholeArchive(const char* mask, const char* targetPath) override;

    BOOL DoUnpackArchive(const char* targetPath, const char* archiveRoot, CNames& names);
};

BOOL FromOctalQ(const unsigned char* ptr, const int length, CQuadWord& result);
void GetFileInfo(const CFileData* newfile, BOOL isDir, char* arcfiledata, char* arcfilename,
                 const char* archiveFileName, const char* fullname);
void namecpy(char* destName, const char* srcName, const unsigned int bufSize);

int CpioDoUnpackArchive(CDecompressFile* stream, CSalamanderForOperationsAbstract* salamander,
                        const char* archiveFileName, const char* targetPath,
                        const char* archiveRoot, CNames& names);
int CpioUnpackOneFile(CDecompressFile* stream, CSalamanderForOperationsAbstract* salamander,
                      const char* archiveFileName, const char* nameInArchive,
                      const char* targetPath);

CArchiveAbstract* CreateArchive(LPCTSTR fileName, CSalamanderForOperationsAbstract* salamander);
