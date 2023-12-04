// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "fs.h"

// ****************************************************************************
//
// CISO9660
//

#define FATTR_HIDDEN 1
#define FATTR_DIRECTORY 2

#define ISO_SEPARATOR1 '.'
#define ISO_SEPARATOR2 ';'

#define ISO_MAX_PATH_LEN 1024

class CISO9660 : public CUnISOFSAbstract
{
protected:
    // struktury
    struct CDirectoryRecord
    {
        BYTE LengthOfDirectoryRecord;
        BYTE ExtendedAttributeRecordLength; // Bytes - this field refers to the
                                            // Extended Attribute Record, which provides
                                            // additional information about a file to
                                            // systems that know how to use it. Since
                                            // few systems use it, we will not discuss
                                            // it here. Refer to ISO 9660:1988 for
                                            // more information.
        DWORD LocationOfExtent;             // This is the Logical Block Number of the
        DWORD LocationOfExtentA;            // first Logical Block allocated to the file
        DWORD DataLength;                   // Length of the file section in bytes
        DWORD DataLengthA;
        BYTE RecordingDateAndTime[7]; //
        BYTE FileFlags;               // One Byte, each bit of which is a Flag:
                                      // bit
                                      // 0 File is Hidden if this bit is 1
                                      // 1 Entry is a Directory if this bit is 1
                                      // 2 Entry is an Associated file is this bit is 1
                                      // 3 Information is structured according to
                                      //   the extended attribute record if this
                                      //   bit is 1
                                      // 4 Owner, group and permissions are
                                      //   specified in the extended attribute
                                      //   record if this bit is 1
                                      // 5 Reserved (0)
                                      // 6 Reserved (0)
                                      // 7 File has more than one directory record
                                      //   if this bit is 1
        BYTE FileUnitSize;            // This field is only valid if the file is
                                      // recorded in interleave mode
                                      // Otherwise this field is (00)
        BYTE InterleaveGapSize;       // This field is only valid if the file is
                                      // recorded in interleave mode
                                      // Otherwise this field is (00)
        WORD VolumeSequenceNumber;    // The ordinal number of the volume in the Volume
        WORD VolumeSequenceNumberA;   // Set on which the file described by the
                                      // directory record is recorded.
        BYTE LengthOfFileIdentifier;  // Length of File Identifier (LEN_FI)

        CDirectoryRecord();
    };

    struct CPathTableRecord
    {
        BYTE LengthOfDirectoryIdentifier;   // Length of Directory Identifier (LEN_DI)
        BYTE ExtendedAttributeRecordLength; // If an Extended Attribute Record is
                                            // recorded, this is the length in Bytes.
                                            // Otherwise, this is (00)
        DWORD LocationOfExtent;             // Logical Block Number of the first Logical
                                            // Block allocated to the Directory
        WORD ParentDirectoryNumber;         // The record number in the Path Table for
                                            // the parent directory of this directory
    };

    struct CPrimaryVolumeDescriptor
    {
        BYTE VolumeDescriptorType;
        BYTE StandardIdentifier[5]; // CD001
        BYTE VolumeDescriptorVersion;
        BYTE Ununsed;
        BYTE SystemIdentifier[32];
        BYTE VolumeIdentifier[32];
        BYTE Unused2[8];
        DWORD VolumeSpaceSize; // Number of logical blocks in the Volume
        DWORD VolumeSpaceSizeA;
        BYTE Unused3[32];
        WORD VolumeSetSize; // The assigned Volume Set size of the Volume
        WORD VolumeSetSizeA;
        WORD VolumeSequenceNumber;  // The ordinal number of the volume in
        WORD VolumeSequenceNumberA; // the Volume Set

        WORD LogicalBlockSize; // The size in bytes of a Logical Block
        WORD LogicalBlockSizeA;
        DWORD PathTableSize; // Length in bytes of the path table
        DWORD PathTableSizeA;
        DWORD LocationOfTypeLPathTable;              // Logical Block Number of first Block allocated
                                                     // to the Type L Path Table
        DWORD LocationOfOptionalTypeLPathTable;      // 0 if Optional Path Table was not recorded,
                                                     // otherwise, Logical Block Number of first
                                                     // Block allocated to the Optional Type L
                                                     // Path Table
        DWORD LocationOfTypeMPathTable;              // Logical Block Number of first Block
                                                     // allocated to the Type M
        DWORD LocationOfOptionalTypeMPathTable;      // 0 if Optional Path Table was not
                                                     // recorded, otherwise, Logical Path Table,
                                                     // Block Number of first Block allocated to the
                                                     // Type M Path Table.
        BYTE DirectoryRecordForRootDirectory[34];    // This is the actual directory record for
                                                     // the top of the directory structure
        BYTE VolumeSetIdentifier[128];               // Name of the multiple volume set of which
                                                     // this volume is a member.
        BYTE PublisherIdentifier[128];               // Identifies who provided the actual data
                                                     // contained in the files. acharacters allowed.
        BYTE DataPreparerIdentifier[128];            // Identifies who performed the actual
                                                     // creation of the current volume.
        BYTE ApplicationIdentifier[128];             // Identifies the specification of how the
                                                     // data in the files are recorded.
        BYTE CopyrightFileIdentifier[37];            // Identifies the file in the root directory
                                                     // that contains the copyright notice for
                                                     // this volume
        BYTE AbstractFileIdentifier[37];             // Identifies the file in the root directory
                                                     // that contains the abstract statement for
                                                     // this volume
        BYTE BibliographicFileIdentifier[37];        // Identifies the file in the root directory
                                                     // that contains bibliographic records.
        BYTE VolumeCreationDateAndTime[17];          // Date and time at which the volume was created
        BYTE VolumeModificationDateAndTime[17];      // Date and time at which the volume was
                                                     // last modified
        BYTE VolumeExpirationDateAndTime[17];        // Date and Time at which the information in
                                                     // the volume may be considered obsolete.
        BYTE VolumeEffectiveDateAndTime[17];         // Date and Time at which the information
                                                     // in the volume may be used
        BYTE FileStructureVersion;                   // 1
        BYTE ReservedForFutureStandardization;       // 0
        BYTE ApplicationUse[512];                    // This field is reserved for application use.
                                                     // Its content is not specified by ISO-9660.
        BYTE ReservedForFutureStandardization2[653]; // 0
    };                                               // sizeof(PrimaryVolumeDescriptor) must be 2048

    struct CSuplementaryVolumeDescriptor
    {
        BYTE VolumeDescriptorType;  // 2
        BYTE StandardIdentifier[5]; // CD001
        BYTE VolumeDescriptorVersion;
        BYTE VolumeFlags;
        BYTE SystemIdentifier[32];
        BYTE VolumeIdentifier[32];
        BYTE Unused2[8];
        DWORD VolumeSpaceSize; // Number of logical blocks in the Volume
        DWORD VolumeSpaceSizeA;
        BYTE EscapeSequences[32];
        WORD VolumeSetSize; // The assigned Volume Set size of the Volume
        WORD VolumeSetSizeA;
        WORD VolumeSequenceNumber;  // The ordinal number of the volume in
        WORD VolumeSequenceNumberA; // the Volume Set

        WORD LogicalBlockSize; // The size in bytes of a Logical Block
        WORD LogicalBlockSizeA;
        DWORD PathTableSize; // Length in bytes of the path table
        DWORD PathTableSizeA;
        DWORD LocationOfTypeLPathTable;              // Logical Block Number of first Block allocated
                                                     // to the Type L Path Table
        DWORD LocationOfOptionalTypeLPathTable;      // 0 if Optional Path Table was not recorded,
                                                     // otherwise, Logical Block Number of first
                                                     // Block allocated to the Optional Type L
                                                     // Path Table
        DWORD LocationOfTypeMPathTable;              // Logical Block Number of first Block
                                                     // allocated to the Type M
        DWORD LocationOfOptionalTypeMPathTable;      // 0 if Optional Path Table was not
                                                     // recorded, otherwise, Logical Path Table,
                                                     // Block Number of first Block allocated to the
                                                     // Type M Path Table.
        BYTE DirectoryRecordForRootDirectory[34];    // This is the actual directory record for
                                                     // the top of the directory structure
        BYTE VolumeSetIdentifier[128];               // Name of the multiple volume set of which
                                                     // this volume is a member.
        BYTE PublisherIdentifier[128];               // Identifies who provided the actual data
                                                     // contained in the files. acharacters allowed.
        BYTE DataPreparerIdentifier[128];            // Identifies who performed the actual
                                                     // creation of the current volume.
        BYTE ApplicationIdentifier[128];             // Identifies the specification of how the
                                                     // data in the files are recorded.
        BYTE CopyrightFileIdentifier[37];            // Identifies the file in the root directory
                                                     // that contains the copyright notice for
                                                     // this volume
        BYTE AbstractFileIdentifier[37];             // Identifies the file in the root directory
                                                     // that contains the abstract statement for
                                                     // this volume
        BYTE BibliographicFileIdentifier[37];        // Identifies the file in the root directory
                                                     // that contains bibliographic records.
        BYTE VolumeCreationDateAndTime[17];          // Date and time at which the volume was created
        BYTE VolumeModificationDateAndTime[17];      // Date and time at which the volume was
                                                     // last modified
        BYTE VolumeExpirationDateAndTime[17];        // Date and Time at which the information in
                                                     // the volume may be considered obsolete.
        BYTE VolumeEffectiveDateAndTime[17];         // Date and Time at which the information
                                                     // in the volume may be used
        BYTE FileStructureVersion;                   // 1
        BYTE ReservedForFutureStandardization;       // 0
        BYTE ApplicationUse[512];                    // This field is reserved for application use.
                                                     // Its content is not specified by ISO-9660.
        BYTE ReservedForFutureStandardization2[653]; // 0
    };                                               // sizeof(PrimaryVolumeDescriptor) must be 2048

    struct CBootRecordVolumeDescriptor
    {
        BYTE VolumeDescriptorType;  // 0
        BYTE StandardIdentifier[5]; // CD001
        BYTE VolumeDescriptorVersion;
        BYTE BootSystemIdentifier[32];
        BYTE BootIdentifier[32];
    };

    struct CPathRecord
    {
        CPathTableRecord Record;
        char FilePath[MAX_PATH];
    };

    enum EExt
    {
        extNone,
        extRockRidge,
        extJoliet
    };

    struct CRRExtHeader
    {
        BYTE Signature[2];
        BYTE Length;
        BYTE Version;
    };

    struct CRRExtNM
    {
        CRRExtHeader Header;
        char Name[MAX_PATH];
    };

    enum EBootRecordType
    {
        biNoEmul,
        bi120,
        bi144,
        bi288,
        biHDD
    };

    struct CBootRecordInfo
    {
        //      WORD SectorCount;
        DWORD LoadRBA;
        EBootRecordType Type;
        DWORD Length;
    };

    EExt Ext;
    CDirectoryRecord Root;
    WORD LogicalBlockSize;
    DWORD ExtentOffset;
    CBootRecordInfo* BootRecordInfo;

public:
    //    CISO9660();
    //    CISO9660(CISOImage *image, EExt ext, BYTE *root, WORD LogicalBlockSize);
    CISO9660(CISOImage* image, DWORD extent);
    virtual ~CISO9660();
    // metody

    virtual BOOL Open(BOOL quiet);
    virtual BOOL DumpInfo(FILE* outStream);

    //    virtual WORD GetLogicalBlockSize();
    //    virtual DWORD GetRootLocationOfExtent();
    //    virtual DWORD GetRootDataLength();
    //    virtual BOOL ListDirectory(int track, char *data, DWORD size, char *path);
    virtual BOOL ListDirectory(char* path, int session,
                               CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);
    //    virtual int UnpackFile(CFilePos *fp, CSalamanderForOperationsAbstract *salamander);
    ///    virtual int UnpackFile(CSalamanderForOperationsAbstract *salamander, HANDLE outFile, CQuadWord fileSize, DWORD block, BOOL &whole);
    virtual int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                           const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip);

protected:
    void FillDirectoryRecord(CDirectoryRecord& root, BYTE bytes[]);
    void FillPathTableRecord(CPathTableRecord& record, BYTE bytes[]);

    int ListDirectoryRe(char* path, CDirectoryRecord* root,
                        CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);

    void ConvJolietName(char* dest, const char* src, int nLen);

    void ExtractExtFileName(char* fileName, const char* src, CDirectoryRecord& dr);
    void ExtractFileName(char* fileName, const char* src, CDirectoryRecord& dr);

    BOOL AddFileDir(const char* path, char* fileName, CDirectoryRecord& dr,
                    CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);

    BOOL ReadBootRecord(BYTE* data, BOOL quiet);

    CPrimaryVolumeDescriptor PVD;
    CSuplementaryVolumeDescriptor SVD;
    CBootRecordVolumeDescriptor BR;

    // boot extensions
    char* GetBootRecordTypeStr(EBootRecordType type);
    BOOL ReadElTorito(BYTE* catalog, DWORD size, BOOL quiet);
    BOOL FillBootRecordInfoElTorito(CBootRecordInfo* bri, BYTE bootMedia, WORD sectorCount, DWORD loadRBA);
    BOOL AddBootRecord(char* path, int session, CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);

    friend class CUDFISO;
};
