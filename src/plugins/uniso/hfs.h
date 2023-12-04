// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "fs.h"

// ****************************************************************************
//
// CHFS
//

#define FromM16(x) (UInt16)(((x) << 8) | ((x) >> 8))
#define FromM32(x) (UInt32)(((x) >> 24) | (((x) >> 8) & 0xFF00) | (((x) << 8) & 0xFF0000) | ((x) << 24))

#define HFS_DEVICE_SIGNATURE 0x5245 // 'ER'

#define HFS_PARTITION_HFS_SIGNATURE 0x4442 // 'BD'
#define HFS_PARTITION_SIGNATURE 0x4d50     // 'PM'

// Volume Header - using the same constant names as in Technical Note 1150
#define kHFSPlusSigWord 0x2b48 // 'H+'
#define kHFSPlusVersion 4

typedef signed char SInt8;
typedef BYTE UInt8;
typedef signed short SInt16;
typedef WORD UInt16;
typedef unsigned int SInt32;
typedef DWORD UInt32;
typedef __int64 SInt64;
typedef unsigned __int64 UInt64;
typedef wchar_t UniChar;

UInt64 FromM64(UInt64 x);

class CHFS : public CUnISOFSAbstract
{
public:
#pragma pack(push, 1)
    struct Block0
    {
        UInt16 sbSig;       // device signature
        UInt16 sbBlkSize;   // block size of the device
        UInt32 sbBlkCount;  // number of blocks on the device
        UInt16 sbDevType;   // reserved
        UInt16 sbDevId;     // reserved
        UInt32 sbData;      // reserved
        UInt16 sbDrvrCount; // number of driver descriptor entries
        UInt32 ddBlock;     // first driver's starting block
        UInt16 ddSize;      // size of the driver, in 512-byte blocks
        UInt16 ddType;      // operating system type (MacOS = 1)
                            //    UInt16  AdditionalDrivers[242]; // additional drivers, if any
    };

    struct Partition
    {
        UInt16 pmSig;         // partition signature
        UInt16 pmSigPad;      // reserved
        UInt32 pmMapBlkCnt;   // number of blocks in partition map
        UInt32 pmPyPartStart; // first physical block of partition
        UInt32 pmPartBlkCnt;  // number of blocks in partition
        char pmPartName[32];  // partition name
        char pmParType[32];   // partition type
        UInt32 pmLgDataStart; // first logical block of data area
        UInt32 pmDataCnt;     // number of blocks in data area
        UInt32 pmPartStatus;  // partition status information
        UInt32 pmLgBootStart; // first logical block of boot code
        UInt32 pmBootSize;    // size of boot code, in bytes
        UInt32 pmBootAddr;    // boot code load address
        UInt32 pmBootAddr2;   // reserved
        UInt32 pmBootEntry;   // boot code entry point
        UInt32 pmBootEntry2;  // reserved
        UInt32 pmBootCksum;   // boot code checksum
        char pmProcessor[16]; // processor type
                              //    UInt16  pmPad[0..188]; // reserved
    };

    struct HFSExtentDescriptor
    {                       // extent descriptor
        UInt16 xdrStABN;    // first allocation block
        UInt16 xdrNumABlks; // number of allocation blocks
    };

    struct MDBExtDataRec
    {
        HFSExtentDescriptor ed[3]; // extent data record
    };

    struct MDBRecord
    {                         // master directory block
        UInt16 drSigWord;     // volume signature - HFS_PARTITION_HFS_SIGNATURE
        UInt32 drCrDate;      // date and time of volume creation
        UInt32 drLsMod;       // date and time of last modification
        UInt16 drAtrb;        // volume attributes
        UInt16 drNmFls;       // number of files in root directory
        UInt16 drVBMSt;       // first block of volume bitmap
        UInt16 drAllocPtr;    // start of next allocation search
        UInt16 drNmAlBlks;    // number of allocation blocks in volume
        UInt32 drAlBlkSiz;    // size (in bytes) of allocation blocks
        UInt32 drClpSiz;      // default clump size
        UInt16 drAlBlSt;      // first allocation block in volume
        UInt32 drNxtCNID;     // next unused catalog node ID
        UInt16 drFreeBks;     // number of unused allocation blocks
        char drVN[28];        // volume name
        UInt32 drVolBkUp;     // date and time of last backup
        UInt16 drVSeqNum;     // volume backup sequence number
        UInt32 drWrCnt;       // volume write count
        UInt32 drXTClpSiz;    // clump size for extents overflow file
        UInt32 drCTClpSiz;    // clump size for catalog file
        UInt16 drNmRtDirs;    // number of directories in root directory
        UInt32 drFilCnt;      // number of files in volume
        UInt32 drDirCnt;      // number of directories in volume
        UInt32 drFndrInfo[8]; // information used by the Finder
#if 0
    UInt16	drVCSize;	// size (in blocks) of volume cache
    UInt16	drVBMCSize;	// size (in blocks) of volume bitmap cache
    UInt16	drCtlCSize;	// size (in blocks) of common volume cache
#else
        UInt16 drEmbedSigWord;             // type of embedded volume
        HFSExtentDescriptor drEmbedExtent; // embedded volume extent
#endif
        UInt32 drXTFlSize;        // size of extents overflow file
        MDBExtDataRec drXTExtRec; // extent record for extents overflow file
        UInt32 drCTFlSize;        // size of catalog file
        MDBExtDataRec drCTExtRec; // extent record for catalog file
    };

    typedef UInt32 HFSCatalogNodeID;

    struct HFSPlusExtentDescriptor
    {
        UInt32 startBlock;
        UInt32 blockCount;
    };

    typedef HFSPlusExtentDescriptor HFSPlusExtentRecord[8];

    struct HFSPlusForkData
    {
        UInt64 logicalSize;
        UInt32 clumpSize;
        UInt32 totalBlocks;
        HFSPlusExtentRecord extents;
    };

    struct HFSPlusVolumeHeader
    {
        UInt16 signature; // kHFSPlusSigWord
        UInt16 version;   // kHFSPlusVersion
        UInt32 attributes;
        UInt32 lastMountedVersion;
        UInt32 reserved;
        UInt32 createDate;
        UInt32 modifyDate;
        UInt32 backupDate;
        UInt32 checkedDate;
        UInt32 fileCount;
        UInt32 folderCount;
        UInt32 blockSize;
        UInt32 totalBlocks;
        UInt32 freeBlocks;
        UInt32 nextAllocation;
        UInt32 rsrcClumpSize;
        UInt32 dataClumpSize;
        HFSCatalogNodeID nextCatalogID;
        UInt32 writeCount;
        UInt64 encodingsBitmap;
        UInt8 finderInfo[32];
        HFSPlusForkData allocationFile;
        HFSPlusForkData extentsFile;
        HFSPlusForkData catalogFile;
        HFSPlusForkData attributesFile;
        HFSPlusForkData startupFile;
    };

    struct HFSUniStr255
    {
        UInt16 length;
        UniChar unicode[255]; // BigEndian, fully decomposed and in canonical order
    };

    // B-Trees used for extentsFile, catalogFile, attributesFile:
    enum eBTNodeType
    {
        kBTLeafNode = -1,
        kBTIndexNode = 0,
        kBTHeaderNode = 1, // BTHeaderRec + unused 128 bytes + Map Record
        kBTMapNode = 2
    };

    struct BTNodeDescriptor
    {
        UInt32 fLink; // ID of the next node of this type, or 0 if this is the last node
        UInt32 bLink; // of the previous node of this type, or 0 if this is the first node
        SInt8 kind;   // see eBTNodeType
        UInt8 height;
        UInt16 numRecords; // The number of records contained in this node
        UInt16 reserved;
    };

    enum BTHeaderAttrs
    {
        kBTBadCloseMask = 0x00000001,
        kBTBigKeysMask = 0x00000002,
        kBTVariableIndexKeysMask = 0x00000004
    };

    struct BTHeaderRec
    {
        UInt16 treeDepth;
        UInt32 rootNode;
        UInt32 leafRecords;
        UInt32 firstLeafNode;
        UInt32 lastLeafNode;
        UInt16 nodeSize;
        UInt16 maxKeyLength;
        UInt32 totalNodes;
        UInt32 freeNodes;
        UInt16 reserved1;
        UInt32 clumpSize;
        UInt8 btreeType;
        UInt8 reserved2;
        UInt32 attributes; // flags, determined by BTHeaderAttrs
        UInt32 reserved3[16];
    };

    // Catalog
    typedef UInt32 HFSCatalogNodeID;

    enum
    {
        kHFSRootParentID = 1,           // Parent ID of the root folder
        kHFSRootFolderID = 2,           // Folder ID of the root folder
        kHFSExtentsFileID = 3,          // File ID of the extents overflow file
        kHFSCatalogFileID = 4,          // File ID of the catalog file
        kHFSBadBlockFileID = 5,         // File ID of the bad block file. The bad block file is not a file in the same sense as a special file and a user file
        kHFSAllocationFileID = 6,       // File ID of the allocation file (introduced with HFS Plus)
        kHFSStartupFileID = 7,          // File ID of the startup file (introduced with HFS Plus)
        kHFSAttributesFileID = 8,       // File ID of the attributes file (introduced with HFS Plus)
        kHFSBogusExtentFileID = 15,     // Used temporarily during ExchangeFiles operations
        kHFSFirstUserCatalogNodeID = 16 // First CNID available for use by user files and folders
                                        // In addition, the CNID of zero is never used and serves as a nil value
    };

    // Symbolic links. The data fork contains UTF8-encoded path to target
    enum
    {
        kSymLinkFileType = 0x6B6E6C73, /* 'slnk' */
        kSymLinkCreator = 0x70616872   /* 'rhap' */
    };

    struct HFSPlusCatalogKey
    {
        UInt16 keyLength; // actually sizeof(HFSPlusCatalogKey)-sizeof(keyLength)
        HFSCatalogNodeID parentID;
        HFSUniStr255 nodeName;
    };

    enum HFSCatalogRecordType
    {
        kHFSFolderRecord = 0x0100,
        kHFSFileRecord = 0x0200,
        kHFSFolderThreadRecord = 0x0300,
        kHFSFileThreadRecord = 0x0400
    };

    struct HFSPlusPermissions
    {
        UInt32 ownerID;
        UInt32 groupID;
        UInt32 permissions;
        UInt32 specialDevice;
    };

    struct Rect
    {
        SInt16 a, b, c, d;
    };

    struct Point
    {
        SInt16 x, y;
    };

    struct DInfo
    {
        Rect frRect;
        UInt16 frFlags;
        Point frLocation;
        SInt16 frView;
    };

    struct DXInfo
    {
        Point frScroll;
        SInt32 frOpenChain;
        SInt8 frScript;
        SInt8 frXFlags;
        SInt16 frComment;
        SInt32 frPutAway;
    };

    typedef UInt32 OSType;

    struct FInfo
    {
        OSType fdType;
        OSType fdCreator;
        UInt16 fdFlags;
        Point fdLocation;
        SInt16 fdFldr;
    };

    struct FXInfo
    {
        SInt16 fdIconID;
        SInt16 fdReserved[3];
        SInt8 fdScript;
        SInt8 fdXFlags;
        SInt16 fdComment;
        SInt32 fdPutAway;
    };

    struct HFSPlusCatalogFolder
    {
        SInt16 recordType; // HFSCatalogRecordType (swapped)
        UInt16 flags;      // Unused
        UInt32 valence;
        HFSCatalogNodeID folderID;
        UInt32 createDate;
        UInt32 contentModDate;
        UInt32 attributeModDate;
        UInt32 accessDate;
        UInt32 backupDate;
        HFSPlusPermissions permissions;
        DInfo userInfo;
        DXInfo finderInfo;
        UInt32 textEncoding;
        UInt32 reserved;
    };

    enum HFSFileFlags
    {
        kHFSFileLockedBit = 0x0000,
        kHFSFileLockedMask = 0x0001,
        kHFSThreadExistsBit = 0x0001,
        kHFSThreadExistsMask = 0x0002
    };

    struct HFSPlusCatalogFile
    {
        SInt16 recordType; // HFSCatalogRecordType (swapped)
        UInt16 flags;      // HFSFileFlags
        UInt32 reserved1;
        HFSCatalogNodeID fileID;
        UInt32 createDate;
        UInt32 contentModDate;
        UInt32 attributeModDate;
        UInt32 accessDate;
        UInt32 backupDate;
        HFSPlusPermissions permissions;
        FInfo userInfo;
        FXInfo finderInfo;
        UInt32 textEncoding;
        UInt32 reserved2;
        HFSPlusForkData dataFork;
        HFSPlusForkData resourceFork;
    };

#pragma pack(pop)

    class BTree
    {
    private:
        char* pBTreeData;
        int nRecords;
        void** pRecords;
        bool bOK;

    public:
        BTree(CHFS* hfs, HFSPlusForkData* fork, BOOL quiet);
        ~BTree();
        void* GetRecord(int ind);
        int GetNumRecords() { return nRecords; };
        bool IsOK() { return bOK; };
    };

    // Used internally when traversing catalog and reconstructing file tree
    struct FolderInfo
    {
        UInt32 id;
        char name[1];
    };

private:
    DWORD SectorSize;
    __int64 HFSWrapperOfs;
    DWORD PartStart; // Partition Start Block

    HFSPlusVolumeHeader VolHeader;

    BTree* pCatalog;
    int nFolders, nAllocedFolders;
    FolderInfo** pFolders;
    bool bSkipRootParent;

    BOOL SeekSector(int sector); // Seeks on disk (sector is usually 512 or 2048 bytes); TRUE on success
    BOOL SeekBlock(int block);   // Seeks on HFS+ partition (block is usually 4096 bytes); TRUE on success
    __int64 SeekRel(int offset); // Seeks relatively; Returns new position
    BOOL Read(LPVOID buf, DWORD dwBytesToRead, DWORD* dwBytesRead);

public:
    CHFS(CISOImage* image);
    virtual ~CHFS();

    virtual BOOL Open(BOOL quiet);
    virtual BOOL DumpInfo(FILE* outStream);
    virtual BOOL ListDirectory(char* path, int session,
                               CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);
    virtual int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                           const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip);

    BOOL GetRootName(char* rootName, int maxlen);

    friend class CUDFISO;
    friend class BTree;
};
