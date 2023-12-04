// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _VQF_SUPPORT_

#include "vqfparser.h"
#include "..\mmviewer.rh"
#include "..\mmviewer.rh2"
#include "..\lang\lang.rh"
#include "..\output.h"

char* LoadStr(int resID);

typedef struct _VQFTAG
{
    char tag[4];
    DWORD len;

    inline BOOL Compare(const char* str) const { return (memcmp(str, &tag, 4) == 0); }
} VQFTAG;

typedef struct
{
    char tag[4];
    int str_id;
    char* str;
} VQF_TAG;

#define TWIN_MAGIC 0x4e495754

#define SWAP_DW(value) \
    (((((DWORD)value) << 24) & 0xFF000000) | \
     ((((DWORD)value) << 8) & 0x00FF0000) | \
     ((((DWORD)value) >> 8) & 0x0000FF00) | \
     ((((DWORD)value) >> 24) & 0x000000FF))

CParserResultEnum
CParserVQF::OpenFile(const char* fileName)
{
    CloseFile();

    f = fopen(fileName, "rb");

    if (!f)
        return preOpenError;

    DWORD magic;
    if (!fread(&magic, sizeof(magic), 1, f))
        return preUnknownFile;

    if (magic != TWIN_MAGIC)
        return preUnknownFile;

    return preOK;
}

CParserResultEnum
CParserVQF::CloseFile()
{
    if (f)
    {
        fclose(f);
        f = NULL;
    }

    return preOK;
}

CParserResultEnum
CParserVQF::GetFileInfo(COutputInterface* output)
{
    if (f)
    {
        VQF_TAG vqftags[VQFTAGS] =
            {
                {'N', 'A', 'M', 'E', IDS_VQF_TITLE, NULL},
                {'A', 'U', 'T', 'H', IDS_VQF_AUTHOR, NULL},
                {'A', 'L', 'B', 'M', IDS_VQF_ALBUM, NULL},
                {'T', 'R', 'A', 'C', IDS_VQF_TRACK, NULL},
                {'Y', 'E', 'A', 'R', IDS_VQF_YEAR, NULL},
                {'C', 'O', 'M', 'T', IDS_VQF_COMMENTS, NULL},
                {'(', 'c', ')', ' ', IDS_VQF_COPYRIGHT, NULL},
                {'F', 'I', 'L', 'E', IDS_VQF_ORIGFILENAME, NULL},

                //TODO nevim presne co tyhle tafy znamenaji, musite si to zjisit
                {'L', 'Y', 'R', 'C', IDS_VQF_LYRC, NULL},
                {'W', 'O', 'R', 'D', IDS_VQF_WORD, NULL},
                {'M', 'U', 'S', 'C', IDS_VQF_MUSC, NULL},
                {'A', 'R', 'N', 'G', IDS_VQF_ARNG, NULL},
                {'P', 'R', 'O', 'D', IDS_VQF_PROD, NULL},
                {'R', 'E', 'M', 'X', IDS_VQF_REMX, NULL},
                {'C', 'D', 'C', 'T', IDS_VQF_CDCT, NULL},
                {'S', 'I', 'N', 'G', IDS_VQF_SING, NULL},
                {'L', 'A', 'B', 'L', IDS_VQF_LABL, NULL},
                {'N', 'O', 'T', 'E', IDS_VQF_NOTE, NULL},
                {'P', 'R', 'S', 'N', IDS_VQF_PRSN, NULL},
                {'B', 'A', 'N', 'D', IDS_VQF_BAND, NULL},
            };

        BOOL first_tag_found = FALSE;
        VQFTAG tag;

        while (fread(&tag, sizeof(tag), 1, f))
        {
            //konec
            if (tag.Compare("DATA"))
                break;

            //kdyz po 64k nenasel zadny tag, vzdej to
            if (!first_tag_found && ftell(f) > 64000)
                return preUnknownFile;

            BOOL ok = FALSE;

            int i;
            for (i = 0; i < VQFTAGS; i++)
            {
                if (tag.Compare(&vqftags[i].tag[0]))
                {
                    tag.len = SWAP_DW(tag.len);

                    if (tag.len > 8192) //zcela urcite chyba
                        break;

                    char* mem = (char*)malloc(tag.len + 1);
                    if (mem)
                    {
                        if (!fread(mem, tag.len, 1, f))
                        {
                            free(mem);
                            return preCorruptedFile;
                        }

                        mem[tag.len] = '\0';
                        vqftags[i].str = mem;

                        ok = TRUE;
                        first_tag_found = TRUE;
                    }
                }
            }

            //tag nenalezen
            if (!ok)
                fseek(f, -signed(sizeof(tag)) + 1, SEEK_CUR);
        }

        // dump
        output->AddHeader(LoadStr(IDS_VQF_INFO));
        int i;
        for (i = 0; i < VQFTAGS; i++)
            if (vqftags[i].str)
            {
                output->AddItem(LoadStr(vqftags[i].str_id), vqftags[i].str);
                free(vqftags[i].str);
            }

        return preOK;
    }

    return preUnknownFile;
}

#endif