// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "../dlldefs.h"
#include "../fileio.h"

#include "../gzip/gzip.h"
//#include "bzip/bzlib.h"
//#include "bzip/bzip.h"
#include "rpm.h"
//#include "orig/rpmorig.h"

#include "..\tar.rh"
#include "..\tar.rh2"
#include "..\lang\lang.rh"

const struct headerTagTableEntry rpmSigTagTable[] = {
    {"RPMSIGTAG_SIZE", RPMSIGTAG_SIZE},
    {"RPMSIGTAG_LEMD5_1", RPMSIGTAG_LEMD5_1},
    {"RPMSIGTAG_PGP", RPMSIGTAG_PGP},
    {"RPMSIGTAG_LEMD5_2", RPMSIGTAG_LEMD5_2},
    {"RPMSIGTAG_MD5", RPMSIGTAG_MD5},
    {"RPMSIGTAG_GPG", RPMSIGTAG_GPG},
    {"RPMSIGTAG_PGP5", RPMSIGTAG_PGP5},
};

const int rpmSigTagTableSize = sizeof(rpmSigTagTable) / sizeof(struct headerTagTableEntry);

const struct headerTagTableEntry rpmTagTable[] = {
    {
        "RPMTAG_HEADERIMAGE",
        HEADER_IMAGE,
    },
    {
        "RPMTAG_HEADERSIGNATURES",
        HEADER_SIGNATURES,
    },
    {
        "RPMTAG_HEADERIMMUTABLE",
        HEADER_IMMUTABLE,
    },
    {
        "RPMTAG_HEADERREGIONS",
        HEADER_REGIONS,
    },
    {
        "RPMTAG_HEADERI18NTABLE",
        HEADER_I18NTABLE,
    },
    {
        "RPMTAG_SIGSIZE",
        RPMTAG_SIG_BASE + 1,
    },
    {
        "RPMTAG_SIGPGP",
        RPMTAG_SIG_BASE + 3,
    },
    {
        "RPMTAG_SIGMD5",
        RPMTAG_SIG_BASE + 5,
    },
    {
        "RPMTAG_SIGGPG",
        RPMTAG_SIG_BASE + 6,
    },
    {
        "RPMTAG_NAME",
        1000,
    },
    {
        "RPMTAG_VERSION",
        1001,
    },
    {
        "RPMTAG_RELEASE",
        1002,
    },
    {
        "RPMTAG_EPOCH",
        1003,
    },
    {"RPMTAG_SERIAL", RPMTAG_EPOCH},
    {
        "RPMTAG_SUMMARY",
        1004,
    },
    {
        "RPMTAG_DESCRIPTION",
        1005,
    },
    {
        "RPMTAG_BUILDTIME",
        1006,
    },
    {
        "RPMTAG_BUILDHOST",
        1007,
    },
    {
        "RPMTAG_INSTALLTIME",
        1008,
    },
    {
        "RPMTAG_SIZE",
        1009,
    },
    {
        "RPMTAG_DISTRIBUTION",
        1010,
    },
    {
        "RPMTAG_VENDOR",
        1011,
    },
    {
        "RPMTAG_GIF",
        1012,
    },
    {
        "RPMTAG_XPM",
        1013,
    },
    {
        "RPMTAG_LICENSE",
        1014,
    },
    {"RPMTAG_COPYRIGHT", RPMTAG_LICENSE},
    {
        "RPMTAG_PACKAGER",
        1015,
    },
    {
        "RPMTAG_GROUP",
        1016,
    },
    {
        "RPMTAG_SOURCE",
        1018,
    },
    {
        "RPMTAG_PATCH",
        1019,
    },
    {
        "RPMTAG_URL",
        1020,
    },
    {
        "RPMTAG_OS",
        1021,
    },
    {
        "RPMTAG_ARCH",
        1022,
    },
    {
        "RPMTAG_PREIN",
        1023,
    },
    {
        "RPMTAG_POSTIN",
        1024,
    },
    {
        "RPMTAG_PREUN",
        1025,
    },
    {
        "RPMTAG_POSTUN",
        1026,
    },
    {
        "RPMTAG_OLDFILENAMES",
        1027,
    },
    {
        "RPMTAG_FILESIZES",
        1028,
    },
    {
        "RPMTAG_FILESTATES",
        1029,
    },
    {
        "RPMTAG_FILEMODES",
        1030,
    },
    {
        "RPMTAG_FILERDEVS",
        1033,
    },
    {
        "RPMTAG_FILEMTIMES",
        1034,
    },
    {
        "RPMTAG_FILEMD5S",
        1035,
    },
    {
        "RPMTAG_FILELINKTOS",
        1036,
    },
    {
        "RPMTAG_FILEFLAGS",
        1037,
    },
    {
        "RPMTAG_ROOT",
        1038,
    },
    {
        "RPMTAG_FILEUSERNAME",
        1039,
    },
    {
        "RPMTAG_FILEGROUPNAME",
        1040,
    },
    {
        "RPMTAG_ICON",
        1043,
    },
    {
        "RPMTAG_SOURCERPM",
        1044,
    },
    {
        "RPMTAG_FILEVERIFYFLAGS",
        1045,
    },
    {
        "RPMTAG_ARCHIVESIZE",
        1046,
    },
    {
        "RPMTAG_PROVIDENAME",
        1047,
    },
    {"RPMTAG_PROVIDES", RPMTAG_PROVIDENAME},
    {
        "RPMTAG_REQUIREFLAGS",
        1048,
    },
    {
        "RPMTAG_REQUIRENAME",
        1049,
    },
    {
        "RPMTAG_REQUIREVERSION",
        1050,
    },
    {
        "RPMTAG_CONFLICTFLAGS",
        1053,
    },
    {
        "RPMTAG_CONFLICTNAME",
        1054,
    },
    {
        "RPMTAG_CONFLICTVERSION",
        1055,
    },
    {
        "RPMTAG_BUILDROOT",
        1057,
    },
    {
        "RPMTAG_EXCLUDEARCH",
        1059,
    },
    {
        "RPMTAG_EXCLUDEOS",
        1060,
    },
    {
        "RPMTAG_EXCLUSIVEARCH",
        1061,
    },
    {
        "RPMTAG_EXCLUSIVEOS",
        1062,
    },
    {
        "RPMTAG_RPMVERSION",
        1064,
    },
    {
        "RPMTAG_TRIGGERSCRIPTS",
        1065,
    },
    {
        "RPMTAG_TRIGGERNAME",
        1066,
    },
    {
        "RPMTAG_TRIGGERVERSION",
        1067,
    },
    {
        "RPMTAG_TRIGGERFLAGS",
        1068,
    },
    {
        "RPMTAG_TRIGGERINDEX",
        1069,
    },
    {
        "RPMTAG_VERIFYSCRIPT",
        1079,
    },
    {
        "RPMTAG_CHANGELOGTIME",
        1080,
    },
    {
        "RPMTAG_CHANGELOGNAME",
        1081,
    },
    {
        "RPMTAG_CHANGELOGTEXT",
        1082,
    },
    {
        "RPMTAG_PREINPROG",
        1085,
    },
    {
        "RPMTAG_POSTINPROG",
        1086,
    },
    {
        "RPMTAG_PREUNPROG",
        1087,
    },
    {
        "RPMTAG_POSTUNPROG",
        1088,
    },
    {
        "RPMTAG_BUILDARCHS",
        1089,
    },
    {
        "RPMTAG_OBSOLETENAME",
        1090,
    },
    {"RPMTAG_OBSOLETES", RPMTAG_OBSOLETENAME},
    {
        "RPMTAG_VERIFYSCRIPTPROG",
        1091,
    },
    {
        "RPMTAG_TRIGGERSCRIPTPROG",
        1092,
    },
    {
        "RPMTAG_COOKIE",
        1094,
    },
    {
        "RPMTAG_FILEDEVICES",
        1095,
    },
    {
        "RPMTAG_FILEINODES",
        1096,
    },
    {
        "RPMTAG_FILELANGS",
        1097,
    },
    {
        "RPMTAG_PREFIXES",
        1098,
    },
    {
        "RPMTAG_INSTPREFIXES",
        1099,
    },
    {
        "RPMTAG_OLDORIGFILENAMES",
        1107,
    },
    {
        "RPMTAG_BUILDMACROS",
        1111,
    },
    {
        "RPMTAG_PROVIDEFLAGS",
        1112,
    },
    {
        "RPMTAG_PROVIDEVERSION",
        1113,
    },
    {
        "RPMTAG_OBSOLETEFLAGS",
        1114,
    },
    {
        "RPMTAG_OBSOLETEVERSION",
        1115,
    },
    {
        "RPMTAG_DIRINDEXES",
        1116,
    },
    {
        "RPMTAG_BASENAMES",
        1117,
    },
    {
        "RPMTAG_DIRNAMES",
        1118,
    },
    {
        "RPMTAG_OPTFLAGS",
        1122,
    },
    {
        "RPMTAG_DISTURL",
        1123,
    },
    {
        "RPMTAG_PAYLOADFORMAT",
        1124,
    },
    {
        "RPMTAG_PAYLOADCOMPRESSOR",
        1125,
    },
    {
        "RPMTAG_PAYLOADFLAGS",
        1126,
    },
    {
        "RPMTAG_MULTILIBS",
        1127,
    },
    {
        "RPMTAG_INSTALLTID",
        1128,
    },
    {
        "RPMTAG_REMOVETID",
        1129,
    }};

const int rpmTagTableSize = sizeof(rpmTagTable) / sizeof(struct headerTagTableEntry);

struct TIndex
{
    uint32_t Tag;
    uint32_t Type;
    uint32_t Offset;
    uint32_t Count;
};

unsigned long SectionSize;
unsigned char* SectionData;
unsigned long IndexCount;
TIndex* Index;

unsigned short RPMToShort(unsigned short value)
{
    return (((unsigned char*)&value)[0] << 8) + ((unsigned char*)&value)[1];
}

uint32_t RPMToLong(uint32_t value)
{
    return (((unsigned char*)&value)[0] << 24) + (((unsigned char*)&value)[1] << 16) +
           (((unsigned char*)&value)[2] << 8) + ((unsigned char*)&value)[3];
}

__int64 RPMToI64(__int64 value)
{
    __int64 tmp = 0;

    int i;
    for (i = 0; i < 8; i++)
    {
        tmp <<= 8;
        tmp += ((unsigned char*)&value)[i];
    }
    return tmp;
}

const char* rpmTypeTable[] = {
    "NULL_TYPE",
    "CHAR_TYPE",
    "INT8_TYPE",
    "INT16_TYPE",
    "INT32_TYPE",
    "INT64_TYPE",
    "STRING_TYPE",
    "BIN_TYPE",
    "STRING_ARRAY_TYPE",
    "I18N_STRING_TYPE"};

#define LEFTCOLUMN1WIDTH 15
#define LEFTCOLUMN2WIDTH 25

// tato funkce nehlasi chyby, muze byt pouzita i k detekci archivu
// hlaseni chyb je treba zpracovat ve volajicim kodu
BOOL CRPM::RPMDumpLead(FILE* fContents, short& SignatureType)
{
    // nemuze to byt RPM, pokud mame min nez signaturu...
    if (DataEnd - DataStart < RPMLEAD_SIZE)
        return FALSE;

    // pokud neni "magicke cislo" na zacatku, nejde o RPM
    rpmlead* RPMLead = (rpmlead*)DataStart;
    if (RPMLead->magic[0] != RPMLEAD_MAGIC0 ||
        RPMLead->magic[1] != RPMLEAD_MAGIC1 ||
        RPMLead->magic[2] != RPMLEAD_MAGIC2 ||
        RPMLead->magic[3] != RPMLEAD_MAGIC3)
        return FALSE;

    // mame RPM, potvrdime precteny header
    FReadBlock(RPMLEAD_SIZE);

    if (fContents != NULL)
    {
        // analyzujeme archive lead
        fprintf(fContents, "Archive lead (unreliable information):\n");
        fprintf(fContents, "%*s: %u.%u\n", LEFTCOLUMN2WIDTH, "Archive version", RPMLead->major, RPMLead->minor);
        fprintf(fContents, "%*s: ", LEFTCOLUMN2WIDTH, "Archive type");
        switch (RPMToShort(RPMLead->type))
        {
        case RPMLEAD_BINARY:
            fprintf(fContents, "binary\n");
            fprintf(fContents, "%*s: ", LEFTCOLUMN2WIDTH, "Target architecture: ");
            switch (RPMToShort(RPMLead->archnum)) // TODO: zkontrolovat nazvy
            {
            case 1:
                fprintf(fContents, "i386\n");
                break;
            case 2:
                fprintf(fContents, "Alpha\n");
                break;
            case 3:
                fprintf(fContents, "Sparc\n");
                break;
            case 4:
                fprintf(fContents, "MIPS big endian\n");
                break;
            case 5:
                fprintf(fContents, "PowerPC\n");
                break;
            case 6:
                fprintf(fContents, "68000\n");
                break;
            case 7:
                fprintf(fContents, "SGI\n");
                break;
            case 8:
                fprintf(fContents, "RS6000\n");
                break;
            case 9:
                fprintf(fContents, "ia64\n"); // TODO
                break;
            case 10:
                fprintf(fContents, "Sparc 64bit\n");
                break;
            case 11:
                fprintf(fContents, "MIPS little endian\n");
                break;
            case 12:
                fprintf(fContents, "ARM\n");
                break;
            case 13:
                fprintf(fContents, "Atari\n"); // TODO
                break;
            case 14:
                fprintf(fContents, "s390/i370\n"); // TODO
                break;
            default:
                fprintf(fContents, "unknown\n");
                break;
            }
            break;
        case RPMLEAD_SOURCE:
            fprintf(fContents, "source\n");
            break;
        default:
            fprintf(fContents, "unknown\n");
            break;
        }
        fprintf(fContents, "%*s: %s\n", LEFTCOLUMN2WIDTH, "Name", RPMLead->name);
        fprintf(fContents, "%*s: ", LEFTCOLUMN2WIDTH, "Target operating system");
        switch (RPMToShort(RPMLead->osnum)) // TODO: zkontrolovat nazvy
        {
        case 1:
            fprintf(fContents, "Linux\n");
            break;
        case 2:
            fprintf(fContents, "IRIX\n");
            break;
        case 3:
            fprintf(fContents, "SunOS5\n");
            break;
        case 4:
            fprintf(fContents, "SunOS4\n");
            break;
        case 5:
            fprintf(fContents, "AmigaOS,AIX\n");
            break;
        case 6:
            fprintf(fContents, "HP-UX\n");
            break;
        case 7:
            fprintf(fContents, "OSF1,OSF4.0,OSF3.2\n");
            break;
        case 8:
            fprintf(fContents, "FreeBSD\n");
            break;
        case 9:
            fprintf(fContents, "SCO_SV3.2v5.0.2\n");
            break;
        case 10:
            fprintf(fContents, "Irix64\n");
            break;
        case 11:
            fprintf(fContents, "NextStep\n");
            break;
        case 12:
            fprintf(fContents, "bsdi\n");
            break;
        case 13:
            fprintf(fContents, "machten\n");
            break;
        case 14:
            fprintf(fContents, "CYGWIN32_NT\n");
            break;
        case 15:
            fprintf(fContents, "CYGWIN32_95\n");
            break;
        case 16:
            fprintf(fContents, "UNIX_SV\n");
            break;
        case 17:
            fprintf(fContents, "FreeMiNT\n");
            break;
        case 18:
            fprintf(fContents, "OS/390\n");
            break;
        case 19:
            fprintf(fContents, "VM/ESA\n");
            break;
        case 20:
            fprintf(fContents, "OS/390,VM/ESA\n");
            break;
        default:
            fprintf(fContents, "unknown\n");
            break;
        }
        fprintf(fContents, "%*s: ", LEFTCOLUMN2WIDTH, "Type of signature");
        switch (RPMToShort(RPMLead->signature_type))
        {
        case RPMSIG_NONE:
            fprintf(fContents, "None\n");
            break;
        case RPMSIG_PGP262_1024:
            fprintf(fContents, "Old PGP type\n");
            break;
        case RPMSIG_BAD:
            fprintf(fContents, "Bad or unknown\n");
            break;
        case RPMSIG_MD5:
            fprintf(fContents, "Old (internal-only) MD5 type\n");
            break;
        case RPMSIG_MD5_PGP:
            fprintf(fContents, "Old (internal-only) MD5/PGP type\n");
            break;
        case RPMSIG_HEADERSIG:
            fprintf(fContents, "Header style\n");
            break;
        default:
            fprintf(fContents, "Unknown\n");
            break;
        }
    }
    SignatureType = RPMToShort(RPMLead->signature_type);
    return TRUE;
}

BOOL CRPM::RPMReadHeader(FILE* fContents)
{
    const unsigned char* header;

    // nemuze to byt RPM header, pokud mame min nez signaturu...
    if (DataEnd - DataStart < 8)
    {
        SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                         LoadStr(IDS_ERR_NORPM), LoadStr(IDS_ERR_RPMTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    // pokud neni "magicke cislo" na zacatku, nejde o RPM
    header = DataStart;
    if (header[0] != RPMHEADER_MAGIC0 || header[1] != RPMHEADER_MAGIC1 ||
        header[2] != RPMHEADER_MAGIC2)
    {
        SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                         LoadStr(IDS_ERR_NORPM), LoadStr(IDS_ERR_RPMTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    // pokud jsme ho nasli, potvrdime ho
    header = FReadBlock(8);

    // check version number
    if (header[3] != 0x01)
    {
        SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                         LoadStr(IDS_ERR_BADRPMVERSION), LoadStr(IDS_ERR_RPMTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    header = FReadBlock(8);
    if (header == NULL)
    {
        SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                         LoadStr(IDS_ERR_FREAD), LoadStr(IDS_ERR_RPMTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    IndexCount = RPMToLong(*((uint32_t*)header));
    SectionSize = RPMToLong(*((uint32_t*)(header + sizeof(uint32_t))));
    if (fContents != NULL)
    {
        Index = new TIndex[IndexCount];
        if (Index == NULL)
        {
            SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                             LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_ERR_RPMTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        SectionData = new unsigned char[SectionSize];
        if (SectionData == NULL)
        {
            SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                             LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_ERR_RPMTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            delete[] Index;
            return FALSE;
        }
    }
    else
    {
        SectionData = NULL;
        Index = NULL;
    }
    unsigned long i;
    for (i = 0; i < IndexCount; i++)
    {
        header = FReadBlock(16);
        if (fContents != NULL)
        {
            if (header == NULL)
            {
                delete[] Index;
                delete[] SectionData;
                SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                                 LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_ERR_RPMTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                return FALSE;
            }
            Index[i].Tag = RPMToLong(*((uint32_t*)header));
            Index[i].Type = RPMToLong(*((uint32_t*)(header + 4)));
            Index[i].Offset = RPMToLong(*((uint32_t*)(header + 8)));
            Index[i].Count = RPMToLong(*((uint32_t*)(header + 12)));
        }
        else if (header == NULL)
        {
            SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                             LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_ERR_RPMTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    }
    unsigned char* ptr = SectionData;
    long j;
    for (j = SectionSize; j > 0; j -= BUFSIZE)
    {
        unsigned long count = j > BUFSIZE ? BUFSIZE : j;
        header = FReadBlock(count);
        if (fContents != NULL)
        {
            if (header == NULL)
            {
                delete[] Index;
                delete[] SectionData;
                SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                                 LoadStr(IDS_ERR_FREAD), LoadStr(IDS_ERR_RPMTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                return FALSE;
            }
            memcpy(ptr, header, count);
            ptr += count;
        }
        else if (header == NULL)
        {
            SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                             LoadStr(IDS_ERR_FREAD), LoadStr(IDS_ERR_RPMTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    }
    return TRUE;
}

const char* const SigTagName(int tagNum)
{
    int i;
    for (i = 0; i < rpmSigTagTableSize; i++)
    {
        if (tagNum == rpmSigTagTable[i].val)
            return rpmSigTagTable[i].name;
    }
    return "Unknown";
}

const char* const TagName(int tagNum)
{
    int i;
    for (i = 0; i < rpmTagTableSize; i++)
    {
        if (tagNum == rpmTagTable[i].val)
            return rpmTagTable[i].name;
    }
    return "Unknown";
}

const char* const TypeName(int typeNum)
{
    if (typeNum >= 0 && typeNum < sizeof(rpmTypeTable) / sizeof(const char*))
        return rpmTypeTable[typeNum];
    else
        return "Unknown";
}

void PrintIndexEntry(FILE* fContents, const char* const (*TagNameFn)(int tagNum), TIndex& index, unsigned char* sectionData)
{
    unsigned long i, j;

    fprintf(fContents, "%*s: %s\n", LEFTCOLUMN1WIDTH, "Section", (*TagNameFn)(index.Tag));
    fprintf(fContents, "%*s: %s\n", LEFTCOLUMN2WIDTH, "Type", TypeName(index.Type));
    fprintf(fContents, "%*s: %u\n", LEFTCOLUMN2WIDTH, "Offset", index.Offset);
    fprintf(fContents, "%*s: %u\n", LEFTCOLUMN2WIDTH, "Count", index.Count);
    fprintf(fContents, "%*s: ", LEFTCOLUMN2WIDTH, "Data");
    // TODO: spesl zpracovani pro nektere zmname sekce (datum, permissions apod.)
    switch (index.Type)
    {
    case RPM_CHAR_TYPE:
        for (i = 0; i < index.Count;)
        {
            fprintf(fContents, "%c", sectionData[index.Offset + i]);
            i++;
            if (i < index.Count && i % 16 == 0)
                fprintf(fContents, "\n%*s  ", LEFTCOLUMN2WIDTH, " ");
        }
        fprintf(fContents, "\n");
        break;
    case RPM_INT16_TYPE:
        for (i = 0; i < index.Count * sizeof(unsigned short);)
        {
            fprintf(fContents, "%u ", RPMToShort(*(unsigned short*)(sectionData + index.Offset + i)));
            i += sizeof(unsigned short);
            if (i < index.Count && i % 8 == 0)
                fprintf(fContents, "\n%*s  ", LEFTCOLUMN2WIDTH, " ");
        }
        fprintf(fContents, "\n");
        break;
    case RPM_INT32_TYPE:
        for (i = 0; i < index.Count * sizeof(uint32_t);)
        {
            fprintf(fContents, "%u ", RPMToLong(*(uint32_t*)(sectionData + index.Offset + i)));
            i += sizeof(uint32_t);
            if (i < index.Count && i % 4 == 0)
                fprintf(fContents, "\n%*s  ", LEFTCOLUMN2WIDTH, " ");
        }
        fprintf(fContents, "\n");
        break;
    case RPM_INT64_TYPE:
        for (i = 0; i < index.Count * sizeof(__int64);)
        {
            fprintf(fContents, "%I64d ", RPMToI64(*(__int64*)(sectionData + index.Offset + i)));
            i += sizeof(__int64);
            if (i < index.Count && i % 4 == 0)
                fprintf(fContents, "\n%*s  ", LEFTCOLUMN2WIDTH, " ");
        }
        fprintf(fContents, "\n");
        break;
    case RPM_STRING_TYPE:
        i = 0;
        while (sectionData[index.Offset + i] != '\0')
        {
            fprintf(fContents, "%c", sectionData[index.Offset + i]);
            i++;
            if (sectionData[index.Offset + i] != '\0' && i % 50 == 0)
                fprintf(fContents, "\n%*s  ", LEFTCOLUMN2WIDTH, " ");
        }
        fprintf(fContents, "\n");
        break;
    case RPM_STRING_ARRAY_TYPE:
        i = 0;
        for (j = 0; j < index.Count; j++)
        {
            int column = 0;
            while (sectionData[index.Offset + i] != '\0')
            {
                fprintf(fContents, "%c", sectionData[index.Offset + i]);
                i++;
                column++;
                if (sectionData[index.Offset + i] != '\0' && column % 50 == 0)
                    fprintf(fContents, "\n%*s  ", LEFTCOLUMN2WIDTH, " ");
            }
            i++;
            if (j < index.Count - 1)
                fprintf(fContents, "\n%*s  ", LEFTCOLUMN2WIDTH, " ");
        }
        fprintf(fContents, "\n");
        break;
    case RPM_I18NSTRING_TYPE:
    case RPM_NULL_TYPE:
    case RPM_BIN_TYPE:
    case RPM_INT8_TYPE:
    default:
        for (i = 0; i < index.Count;)
        {
            fprintf(fContents, "%02X ", sectionData[index.Offset + i]);
            i++;
            if (i < index.Count && i % 16 == 0)
                fprintf(fContents, "\n%*s  ", LEFTCOLUMN2WIDTH, " ");
        }
        fprintf(fContents, "\n");
        break;
    }
}

BOOL CRPM::RPMReadSignature(FILE* fContents, short signatureType)
{
    const unsigned char* buffer;
    unsigned long i;

    if (fContents != NULL)
        fprintf(fContents, "\nArchive signature:\n");

    switch (signatureType)
    {
    case RPMSIG_NONE:
        if (fContents != NULL)
            fprintf(fContents, "%*s\n", LEFTCOLUMN2WIDTH, "None");
        break;
    case RPMSIG_PGP262_1024:
        // These are always 256 bytes
        buffer = FReadBlock(256);
        if (buffer == NULL)
        {
            SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                             LoadStr(IDS_ERR_FREAD), LoadStr(IDS_ERR_RPMTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        if (fContents != NULL)
            fprintf(fContents, "%*s\n", LEFTCOLUMN2WIDTH, "Old PGP signature 256 bytes long");
        break;
    case RPMSIG_MD5:
    case RPMSIG_MD5_PGP:
        if (fContents != NULL)
            fprintf(fContents, "%*s\n", LEFTCOLUMN2WIDTH, "Unsupported, skipping the rest of the file");
        return FALSE;
    case RPMSIG_HEADERSIG:
        // This is a new style signature
        if (!RPMReadHeader(fContents))
        {
            SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                             LoadStr(IDS_ERR_FREAD), LoadStr(IDS_ERR_RPMTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        // read in the padding
        if (SectionSize % 8 > 0)
            if (FReadBlock(8 - SectionSize % 8) == NULL)
            {
                SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                                 LoadStr(IDS_ERR_FREAD), LoadStr(IDS_ERR_RPMTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                return FALSE;
            }
        if (fContents != NULL)
        {
            for (i = 0; i < IndexCount; i++)
                PrintIndexEntry(fContents, SigTagName, Index[i], SectionData);
            delete[] Index;
            delete[] SectionData;
        }
        break;
    default:
        SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                         LoadStr(IDS_ERR_BADSIGNATURE), LoadStr(IDS_ERR_RPMTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    return TRUE;
}

BOOL CRPM::RPMReadSection(FILE* fContents)
{
    if (!RPMReadHeader(fContents))
        return FALSE;

    if (fContents != NULL)
    {
        fprintf(fContents, "\nArchive header:\n");
        unsigned long i;
        for (i = 0; i < IndexCount; i++)
            PrintIndexEntry(fContents, TagName, Index[i], SectionData);
        delete[] Index;
        delete[] SectionData;
    }
    return TRUE;
}
