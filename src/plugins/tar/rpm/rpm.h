// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define RPMSIG_NONE 0 /* Do not change! */
#define RPMSIG_BAD 2  /* Returned for unknown types */
/* The following types are no longer generated */
#define RPMSIG_PGP262_1024 1 /* No longer generated */
#define RPMSIG_MD5 3
#define RPMSIG_MD5_PGP 4

/* These are the new-style signatures.  They are Header structures.    */
/* Inside them we can put any number of any type of signature we like. */

#define RPMSIG_HEADERSIG 5 /* New Header style signature */

#define RPMLEAD_BINARY 0
#define RPMLEAD_SOURCE 1

#define RPMLEAD_MAGIC0 0xed
#define RPMLEAD_MAGIC1 0xab
#define RPMLEAD_MAGIC2 0xee
#define RPMLEAD_MAGIC3 0xdb

#define RPMLEAD_SIZE 96 /*!< Don't rely on sizeof(struct) */

#define RPMHEADER_MAGIC0 0x8e
#define RPMHEADER_MAGIC1 0xad
#define RPMHEADER_MAGIC2 0xe8

/**
 * Header private tags.
 * @note General use tags should start at 1000 (RPM's tag space starts there).
 */
#define HEADER_IMAGE 61
#define HEADER_SIGNATURES 62
#define HEADER_IMMUTABLE 63
#define HEADER_REGIONS 64
#define HEADER_I18NTABLE 100
#define HEADER_SIGBASE 256
#define HEADER_TAGBASE 1000

// external variables
extern const int rpmTagTableSize;
extern const int rpmSigTagTableSize;
extern const struct headerTagTableEntry rpmTagTable[];
extern const struct headerTagTableEntry rpmSigTagTable[];

/** \ingroup lead
 * The lead data structure.
 * The lead needs to be 8 byte aligned.
 * @deprecated The lead (except for signature_type) is legacy.
 * @todo Don't use any information from lead.
 */
struct rpmlead
{
    unsigned char magic[4];
    unsigned char major, minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type; /*!< Signature header type (RPMSIG_HEADERSIG) */
    char reserved[16];    /*!< Pad to 96 bytes -- 8 byte aligned! */
};

/** \ingroup header
 * Associate tag names with numeric values.
 */
struct headerTagTableEntry
{
    const char* name; /*!< Tag name. */
    int val;          /*!< Tag numeric value. */
};

/** \ingroup header
 * The basic types of data in tags from headers.
 */
typedef enum rpmTagType_e
{
    RPM_NULL_TYPE = 0,
    RPM_CHAR_TYPE = 1,
    RPM_INT8_TYPE = 2,
    RPM_INT16_TYPE = 3,
    RPM_INT32_TYPE = 4,
    RPM_INT64_TYPE = 5,
    RPM_STRING_TYPE = 6,
    RPM_BIN_TYPE = 7,
    RPM_STRING_ARRAY_TYPE = 8,
    RPM_I18NSTRING_TYPE = 9
} rpmTagType;

/** \ingroup signature
 * Tags found in signature header from package.
 */
enum rpmtagSignature
{
    RPMSIGTAG_SIZE = 1000,    /*!< Size in bytes. */
                              /* the md5 sum was broken *twice* on big endian machines */
    RPMSIGTAG_LEMD5_1 = 1001, /*!< Broken MD5, take 1 */
    RPMSIGTAG_PGP = 1002,     /*!< PGP 2.6.3 signature. */
    RPMSIGTAG_LEMD5_2 = 1003, /*!< Broken MD5, take 2 */
    RPMSIGTAG_MD5 = 1004,     /*!< MD5 signature. */
    RPMSIGTAG_GPG = 1005,     /*!< GnuPG signature. */
    RPMSIGTAG_PGP5 = 1006,    /*!< PGP5 signature @deprecated legacy. */

    /* Signature tags by Public Key Algorithm (RFC 2440) */
    /* N.B.: These tags are tenative, the values may change */
    RPMTAG_PK_BASE = 512,                       /*!< @todo Implement. */
    RPMTAG_PK_RSA_ES = RPMTAG_PK_BASE + 1,      /*!< (unused */
    RPMTAG_PK_RSA_E = RPMTAG_PK_BASE + 2,       /*!< (unused) */
    RPMTAG_PK_RSA_S = RPMTAG_PK_BASE + 3,       /*!< (unused) */
    RPMTAG_PK_ELGAMAL_E = RPMTAG_PK_BASE + 16,  /*!< (unused) */
    RPMTAG_PK_DSA = RPMTAG_PK_BASE + 17,        /*!< (unused) */
    RPMTAG_PK_ELLIPTIC = RPMTAG_PK_BASE + 18,   /*!< (unused) */
    RPMTAG_PK_ECDSA = RPMTAG_PK_BASE + 19,      /*!< (unused) */
    RPMTAG_PK_ELGAMAL_ES = RPMTAG_PK_BASE + 20, /*!< (unused) */
    RPMTAG_PK_DH = RPMTAG_PK_BASE + 21,         /*!< (unused) */

    RPMTAG_HASH_BASE = 512 + 64,                   /*!< @todo Implement. */
    RPMTAG_HASH_MD5 = RPMTAG_HASH_BASE + 1,        /*!< (unused) */
    RPMTAG_HASH_SHA1 = RPMTAG_HASH_BASE + 2,       /*!< (unused) */
    RPMTAG_HASH_RIPEMD160 = RPMTAG_HASH_BASE + 3,  /*!< (unused) */
    RPMTAG_HASH_MD2 = RPMTAG_HASH_BASE + 5,        /*!< (unused) */
    RPMTAG_HASH_TIGER192 = RPMTAG_HASH_BASE + 6,   /*!< (unused) */
    RPMTAG_HASH_HAVAL_5_160 = RPMTAG_HASH_BASE + 7 /*!< (unused) */
};

/**
 * Tags identify data in package headers.
 * @note tags should not have value 0!
 */
typedef enum rpmTag_e
{

    RPMTAG_HEADERIMAGE = HEADER_IMAGE,           /*!< Current image. */
    RPMTAG_HEADERSIGNATURES = HEADER_SIGNATURES, /*!< Signatures. */
    RPMTAG_HEADERIMMUTABLE = HEADER_IMMUTABLE,   /*!< Original image. */
    RPMTAG_HEADERREGIONS = HEADER_REGIONS,       /*!< Regions. */

    RPMTAG_HEADERI18NTABLE = HEADER_I18NTABLE, /*!< I18N string locales. */

    /* Retrofit (and uniqify) signature tags for use by tagName() and rpmQuery. */
    /* the md5 sum was broken *twice* on big endian machines */
    /* XXX 2nd underscore prevents tagTable generation */
    RPMTAG_SIG_BASE = HEADER_SIGBASE,
    RPMTAG_SIGSIZE = RPMTAG_SIG_BASE + 1,
    RPMTAG_SIGLEMD5_1 = RPMTAG_SIG_BASE + 2,
    RPMTAG_SIGPGP = RPMTAG_SIG_BASE + 3,
    RPMTAG_SIGLEMD5_2 = RPMTAG_SIG_BASE + 4,
    RPMTAG_SIGMD5 = RPMTAG_SIG_BASE + 5,
    RPMTAG_SIGGPG = RPMTAG_SIG_BASE + 6,
    RPMTAG_SIGPGP5 = RPMTAG_SIG_BASE + 7, /*!< internal */

    RPMTAG_NAME = 1000,
    RPMTAG_VERSION = 1001,
    RPMTAG_RELEASE = 1002,
    RPMTAG_EPOCH = 1003,
#define RPMTAG_SERIAL RPMTAG_EPOCH /* backward comaptibility */
    RPMTAG_SUMMARY = 1004,
    RPMTAG_DESCRIPTION = 1005,
    RPMTAG_BUILDTIME = 1006,
    RPMTAG_BUILDHOST = 1007,
    RPMTAG_INSTALLTIME = 1008,
    RPMTAG_SIZE = 1009,
    RPMTAG_DISTRIBUTION = 1010,
    RPMTAG_VENDOR = 1011,
    RPMTAG_GIF = 1012,
    RPMTAG_XPM = 1013,
    RPMTAG_LICENSE = 1014,
#define RPMTAG_COPYRIGHT RPMTAG_LICENSE /* backward comaptibility */
    RPMTAG_PACKAGER = 1015,
    RPMTAG_GROUP = 1016,
    RPMTAG_CHANGELOG = 1017, /*!< internal */
    RPMTAG_SOURCE = 1018,
    RPMTAG_PATCH = 1019,
    RPMTAG_URL = 1020,
    RPMTAG_OS = 1021,
    RPMTAG_ARCH = 1022,
    RPMTAG_PREIN = 1023,
    RPMTAG_POSTIN = 1024,
    RPMTAG_PREUN = 1025,
    RPMTAG_POSTUN = 1026,
    RPMTAG_OLDFILENAMES = 1027, /* obsolete */
    RPMTAG_FILESIZES = 1028,
    RPMTAG_FILESTATES = 1029,
    RPMTAG_FILEMODES = 1030,
    RPMTAG_FILEUIDS = 1031, /*!< internal */
    RPMTAG_FILEGIDS = 1032, /*!< internal */
    RPMTAG_FILERDEVS = 1033,
    RPMTAG_FILEMTIMES = 1034,
    RPMTAG_FILEMD5S = 1035,
    RPMTAG_FILELINKTOS = 1036,
    RPMTAG_FILEFLAGS = 1037,
    RPMTAG_ROOT = 1038, /*!< obsolete */
    RPMTAG_FILEUSERNAME = 1039,
    RPMTAG_FILEGROUPNAME = 1040,
    RPMTAG_EXCLUDE = 1041,   /*!< internal - deprecated */
    RPMTAG_EXCLUSIVE = 1042, /*!< internal - deprecated */
    RPMTAG_ICON = 1043,
    RPMTAG_SOURCERPM = 1044,
    RPMTAG_FILEVERIFYFLAGS = 1045,
    RPMTAG_ARCHIVESIZE = 1046,
    RPMTAG_PROVIDENAME = 1047,
#define RPMTAG_PROVIDES RPMTAG_PROVIDENAME /* backward comaptibility */
    RPMTAG_REQUIREFLAGS = 1048,
    RPMTAG_REQUIRENAME = 1049,
    RPMTAG_REQUIREVERSION = 1050,
    RPMTAG_NOSOURCE = 1051, /*!< internal */
    RPMTAG_NOPATCH = 1052,  /*!< internal */
    RPMTAG_CONFLICTFLAGS = 1053,
    RPMTAG_CONFLICTNAME = 1054,
    RPMTAG_CONFLICTVERSION = 1055,
    RPMTAG_DEFAULTPREFIX = 1056, /*!< internal - deprecated */
    RPMTAG_BUILDROOT = 1057,
    RPMTAG_INSTALLPREFIX = 1058, /*!< internal - deprecated */
    RPMTAG_EXCLUDEARCH = 1059,
    RPMTAG_EXCLUDEOS = 1060,
    RPMTAG_EXCLUSIVEARCH = 1061,
    RPMTAG_EXCLUSIVEOS = 1062,
    RPMTAG_AUTOREQPROV = 1063, /*!< internal */
    RPMTAG_RPMVERSION = 1064,
    RPMTAG_TRIGGERSCRIPTS = 1065,
    RPMTAG_TRIGGERNAME = 1066,
    RPMTAG_TRIGGERVERSION = 1067,
    RPMTAG_TRIGGERFLAGS = 1068,
    RPMTAG_TRIGGERINDEX = 1069,
    RPMTAG_VERIFYSCRIPT = 1079,
    RPMTAG_CHANGELOGTIME = 1080,
    RPMTAG_CHANGELOGNAME = 1081,
    RPMTAG_CHANGELOGTEXT = 1082,
    RPMTAG_BROKENMD5 = 1083, /*!< internal */
    RPMTAG_PREREQ = 1084,    /*!< internal */
    RPMTAG_PREINPROG = 1085,
    RPMTAG_POSTINPROG = 1086,
    RPMTAG_PREUNPROG = 1087,
    RPMTAG_POSTUNPROG = 1088,
    RPMTAG_BUILDARCHS = 1089,
    RPMTAG_OBSOLETENAME = 1090,
#define RPMTAG_OBSOLETES RPMTAG_OBSOLETENAME /* backward comaptibility */
    RPMTAG_VERIFYSCRIPTPROG = 1091,
    RPMTAG_TRIGGERSCRIPTPROG = 1092,
    RPMTAG_DOCDIR = 1093, /*!< internal */
    RPMTAG_COOKIE = 1094,
    RPMTAG_FILEDEVICES = 1095,
    RPMTAG_FILEINODES = 1096,
    RPMTAG_FILELANGS = 1097,
    RPMTAG_PREFIXES = 1098,
    RPMTAG_INSTPREFIXES = 1099,
    RPMTAG_TRIGGERIN = 1100,        /*!< internal */
    RPMTAG_TRIGGERUN = 1101,        /*!< internal */
    RPMTAG_TRIGGERPOSTUN = 1102,    /*!< internal */
    RPMTAG_AUTOREQ = 1103,          /*!< internal */
    RPMTAG_AUTOPROV = 1104,         /*!< internal */
    RPMTAG_CAPABILITY = 1105,       /*!< internal obsolete */
    RPMTAG_SOURCEPACKAGE = 1106,    /*!< internal */
    RPMTAG_OLDORIGFILENAMES = 1107, /*!< obsolete */
    RPMTAG_BUILDPREREQ = 1108,      /*!< internal */
    RPMTAG_BUILDREQUIRES = 1109,    /*!< internal */
    RPMTAG_BUILDCONFLICTS = 1110,   /*!< internal */
    RPMTAG_BUILDMACROS = 1111,
    RPMTAG_PROVIDEFLAGS = 1112,
    RPMTAG_PROVIDEVERSION = 1113,
    RPMTAG_OBSOLETEFLAGS = 1114,
    RPMTAG_OBSOLETEVERSION = 1115,
    RPMTAG_DIRINDEXES = 1116,
    RPMTAG_BASENAMES = 1117,
    RPMTAG_DIRNAMES = 1118,
    RPMTAG_ORIGDIRINDEXES = 1119, /*!< internal */
    RPMTAG_ORIGBASENAMES = 1120,  /*!< internal */
    RPMTAG_ORIGDIRNAMES = 1121,   /*!< internal */
    RPMTAG_OPTFLAGS = 1122,
    RPMTAG_DISTURL = 1123,
    RPMTAG_PAYLOADFORMAT = 1124,
    RPMTAG_PAYLOADCOMPRESSOR = 1125,
    RPMTAG_PAYLOADFLAGS = 1126,
    RPMTAG_MULTILIBS = 1127,
    RPMTAG_INSTALLTID = 1128,
    RPMTAG_REMOVETID = 1129,
    RPMTAG_FIRSTFREE_TAG /*!< internal */
} rpmTag;

// internal functions
unsigned short RPMToShort(unsigned short value);
unsigned long RPMToLong(unsigned long value);

class CRPM : public CDecompressFile
{
private:
    CZippedFile* Stream;

public:
    CRPM(const char* filename, HANDLE file, unsigned char* buffer, unsigned long read, FILE* fContents = NULL);
    ~CRPM(void) override;

    BOOL RPMDumpLead(FILE* fContents, short& SignatureType);
    BOOL RPMReadSignature(FILE* fContents, short signatureType);
    BOOL RPMReadSection(FILE* fContents);
    BOOL RPMReadHeader(FILE* fContents);

    const unsigned char* GetBlock(unsigned short size, unsigned short* read = NULL) override;
    void Rewind(unsigned short size) override;
    void GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr) override;
    // Should other functions be forwarded to Stream?????
};
