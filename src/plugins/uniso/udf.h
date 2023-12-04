// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "fs.h"

// ****************************************************************************
//
// CUDF
//

/* Types */
#define Uint8 BYTE
#define Uint16 WORD
#define Uint32 DWORD
#define Uint64 unsigned __int64

enum
{
    UDF_TAGID_SPARING_TABLE = 0,
    UDF_TAGID_PRI_VOL = 1,
    UDF_TAGID_ANCHOR = 2,
    UDF_TAGID_VOL = 3,
    UDF_TAGID_IMP_VOL = 4,
    UDF_TAGID_PARTITION = 5,
    UDF_TAGID_LOGVOL = 6,
    UDF_TAGID_UNALLOC_SPACE = 7,
    UDF_TAGID_TERM = 8,
    UDF_TAGID_LOGVOL_INTEGRITY = 9,
    UDF_TAGID_FSD = 256,
    UDF_TAGID_FID = 257,
    UDF_TAGID_ALLOCEXTENT = 258,
    UDF_TAGID_INDIRECTENTRY = 259,
    UDF_TAGID_ICB_TERM = 260,
    UDF_TAGID_FENTRY = 261,
    UDF_TAGID_EXTATTR_HDR = 262,
    UDF_TAGID_UNALL_SP_ENTRY = 263,
    UDF_TAGID_SPACE_BITMAP = 264,
    UDF_TAGID_PART_INTEGRETY = 265,
    UDF_TAGID_EXTFENTRY = 266,
    UDF_TAGID_UNKNOWN = 0xFFFF
};

/* short/long/ext extent have flags encoded in length */
#define UDF_EXT_ALLOCATED 0
#define UDF_EXT_FREED 1
#define UDF_EXT_ALLOCATED_BUT_NOT_USED 1
#define UDF_EXT_FREE 2
#define UDF_EXT_REDIRECT 3

typedef Uint16 UDF_TAG_ID;

class CUDF : public CUnISOFSAbstract
{
public:
    struct CTimeStamp
    {
        Uint16 TypeAndTimezone;
        Uint16 Year;
        Uint8 Month;
        Uint8 Day;
        Uint8 Hour;
        Uint8 Minute;
        Uint8 Second;
        Uint8 Centiseconds;
        Uint8 HundredsofMicroseconds;
        Uint8 Microseconds;
    };

protected:
    struct CExtentAd
    {
        DWORD Length;
        DWORD Location;
    };

    ///
    struct CICBTag
    {
        Uint8 FileType;
        Uint16 Flags;
        Uint16 Strategy;
        Uint16 StrategyParametr;
        Uint16 Entries; // Maximum Number of Entries
    };

    ///
    struct CICB
    {
        Uint32 Location;
        Uint16 Partition;
    };

    ///
    struct CAD
    {
        Uint8 Flags;
        Uint32 Location;
        Uint32 Length;
        Uint16 Partition;
        Uint16 Offset; // Offset within sector; Non-zero only for data inlined in tag data
    };

    ///
    struct CPartition
    {
        int Valid;
        char VolumeDesc[128];
        Uint16 Flags;
        Uint16 Number;
        char Contents[32];
        Uint32 AccessType;
        Uint32 Start;
        Uint32 Length;
    };

    struct CCharSpec
    {
        Uint8 CharacterSetType;
        BYTE CharacterSetInfo[63];
    };

    struct CEntityID
    {
        Uint8 Flags;
        char Identifier[23];
        char Suffix[8];
    };

    struct CDescriptorTag
    {
        UDF_TAG_ID ID;
        Uint16 Version;
        Uint8 Checksum;
        BYTE Reserved;
        Uint16 SerialNumber;
        Uint16 CRC;
        Uint16 CRCLength;
        Uint32 TagLocation;
    };

    struct CPrimaryVolumeDescriptor
    {
        CDescriptorTag DescriptorTag;
        Uint32 VolumeDescriptorSequenceNumber;
        Uint32 PrimaryVolumeDescriptorNumber;
        char VolumeIdentifier[32];
        Uint16 VolumeSequenceNumber;
        Uint16 MaximumVolumeSequenceNumber;
        Uint16 InterchangeLevel;
        Uint16 MaximumInterchangeLevel;
        Uint32 CharacterSetList;
        Uint32 MaximumCharacterSetList;
        char VolumeSetIdentifier[128];
        CCharSpec DescriptorCharacterSet;
        CCharSpec ExplanatoryCharacterSet;
        CExtentAd VolumeAbstract;
        CExtentAd VolumeCopyrightNotice;
        CEntityID ApplicationIdentifier;
        CTimeStamp RecordingDateandTime;
        CEntityID ImplementationIdentifier;
        BYTE ImplementationUse[64];
        Uint32 PredecessorVolumeDescriptorSequenceLocation;
        Uint16 Flags;
        BYTE Reserved[22];
    };

    // Anchor Volume Descriptor Pointer
    struct CAVDP
    {
        CExtentAd MVDS;
        CExtentAd RVDS;
    };

    // Partition Descriptor
    struct CPD
    {
        Uint16 Flags;
        Uint16 PartitionNumber;
        Uint32 Start;
        Uint32 Length;
    };

    // Logical Volume Descriptor
    struct CLVD
    {
        Uint32 VDSN; // Volume Descriptor Sequence Number
        Uint32 LogicalBlockSize;
        CAD FSD; // File Set Descriptor
        BYTE LogicalVolumeIdentifier[128];
    };

    enum EPartitionMapping
    {
        mapPhysical,
        mapVirtual,
        mapSparable,
        mapMetadata
    };

    // Partition Map
    struct CPartitionMap
    {
        EPartitionMapping Type;
        WORD VolumeSequenceNumber;
        WORD PartitionNumber;
    };

    struct CPartitionMapping
    {
        EPartitionMapping Type;
        Uint64 Entries;
        Uint32 Length;
        Uint32* Translation;
        BYTE* VAT;
        CAD MetadataICBs[32];
        int nMetadataICBs; // # of used entries in MetadataICBs
    };

    struct CFileEntry
    {
        CICBTag ICBTag;
        CTimeStamp mtime;
        Uint64 InformationLength;
        Uint32 L_EA;
        Uint32 L_AD;
    };

    CPrimaryVolumeDescriptor PVD;

    CAVDP AVDP;
    CPD PD;
    CLVD LVD;

    DWORD ExtentOffset;

    int TrackTryingToOpen;

    TIndirectArray<CPartitionMap> PartitionMaps;
    CPartitionMapping PartitionMapping;

public:
    CUDF(CISOImage* image, DWORD extent, int trackno);
    virtual ~CUDF();
    // metody

    virtual BOOL Open(BOOL quiet);
    virtual BOOL DumpInfo(FILE* outStream);
    virtual BOOL ListDirectory(char* path, int session,
                               CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);
    virtual int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                           const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip);

protected:
    BOOL ReadBlockPhys(Uint32 lbNumber, size_t blocks, unsigned char* data);
    BOOL ReadBlockLog(Uint32 lbNumber, size_t blocks, unsigned char* data);

    BOOL AddFileDir(const char* path, char* fileName, BYTE fileChar, CAD* icb,
                    CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);

    int FindPartition(int partnum, CUDF::CPartition* part);

    int ScanDir(CUDF::CAD dirICB, char* path,
                CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);

    void ReadExtentAd(BYTE sector[], CExtentAd* extent);
    void ReadEntityID(BYTE sector[], CEntityID* entityID);
    void ReadAVDP(BYTE sector[], CAVDP* avdp);
    void ReadPD(BYTE sector[], CPD* pd);
    void ReadLVD(BYTE sector[], CLVD* lvd);
    void ReadDescriptorTag(BYTE sector[], CDescriptorTag* tag);
    int ReadPartitionMap(BYTE data[]);
    int ShortAD(Uint8* data, CUDF::CAD* ad, WORD partNumber);
    int LongAD(Uint8* data, CUDF::CAD* ad);
    int ExtAD(Uint8* data, CUDF::CAD* ad);

    void ReadICBTag(BYTE data[], CICBTag* icb);
    int MapICB(CUDF::CAD icb, CICBTag* icbTag, CUDF::CAD* file);
    int MapICB(CUDF::CAD icb, CFileEntry* fe);
    int ReadFileEntry(Uint8* data, bool bEFE, Uint16 part, CICBTag* icbTag, CUDF::CAD* ad, int maxAds);
    int ReadFileIdentifier(Uint8* sector, Uint8* fileChar, char* fileName, CAD* fileICB);

    void ReadStrategy(BYTE data[], Uint16* entries, Uint16* strategy, Uint16* strategyParam);

    BOOL ReadSupportingTables(BOOL quiet);
    int CheckForVAT(BYTE sector[], bool bEFE, CPartitionMapping* pm, BOOL quiet);

    void ReadFileEntry(BYTE sector[], bool bEFE, CFileEntry* fe);
    int ReadAllocDesc(BYTE data[], CFileEntry* fe, BYTE* vat, DWORD* o);

    int ReadVAT(BYTE fileEntry[], bool bEFE, BYTE** vat, DWORD* length, BOOL quiet);
    Uint32 LogSector(Uint32 lbNum, Uint16 part);

    friend class CUDFISO;
};
