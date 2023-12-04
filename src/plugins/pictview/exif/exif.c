// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <crtdbg.h>

#include "exif.h"
#include <libexif/exif-data.h>
#include <libexif/exif-loader.h>
#include <libexif/exif-utils.h>
#include <libjpeg/jpeg-data.h>

#pragma warning(push)
#pragma warning(disable : 4267) // FIXME_X64 - docasne potlacen warning, vyresit
#pragma warning(disable : 4133) // FIXME_X64 - docasne potlacen warning, vyresit

//
// Global variables
//

HINSTANCE HInstance = NULL; // Handle to this DLL itself.

struct CEnumData
{
    EXIFENUMPROC EnumFunc;
    LPARAM LParam;
};

BOOL WINAPI
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        HInstance = hInstance;
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        FreeTranslations();
    }
    return TRUE;
}

static void
show_entry(ExifEntry* entry, void* data)
{
    struct CEnumData* enumData = (struct CEnumData*)data;
    char value[256];
    ExifIfd ifd = exif_entry_get_ifd(entry);

    enumData->EnumFunc(entry->tag,
                       exif_tag_get_title_in_ifd(entry->tag, ifd),
                       exif_tag_get_description_in_ifd(entry->tag, ifd),
                       exif_entry_get_value(entry, value, sizeof(value)),
                       enumData->LParam);
}

static void
show_ifd(ExifContent* content, void* data)
{
    exif_content_foreach_entry(content, show_entry, data);
}

DWORD WINAPI
EXIFGetVersion()
{
    return EXIF_DLL_VERSION;
}

BOOL WINAPI
EXIFGetInfo(const char* fileName, int dataLen, EXIFENUMPROC enumFunc, LPARAM lParam)
{
    ExifData* ed;
    ExifMnoteData* md;

    struct CEnumData data;
    data.EnumFunc = enumFunc;
    data.LParam = lParam;

    if (dataLen)
    {
        ed = exif_data_new_from_data(fileName, dataLen);
    }
    else
    {
        ed = exif_data_new_from_file(fileName);
    }
    if (ed == NULL)
        return FALSE;
    md = exif_data_get_mnote_data(ed);

    exif_data_foreach_content(ed, show_ifd, &data);

    if (md)
    {
        int c, i;
        unsigned prevId = -1, cnt = 0x1000;

        c = exif_mnote_data_count(md);
        for (i = 0; i < c; i++)
        {

            char val[256];
            const char* title;

            exif_mnote_data_get_value(md, i, val, sizeof(val));
            title = exif_mnote_data_get_title(md, i);
            // ignore unknown parts of Canon makernote
            if (title)
            {
                // We need the tag number to let the user highlight tags
                // he/she is interested in.
                // 0x5678 is a proprierary shift that helps us to distinguish mnote from ordinary tags
                unsigned int id = exif_mnote_data_get_id(md, i) + 0x5678;
                // There are multiple 'Settings (first part)' & 'Settings (second part)' values coming from Canon
                // We attempt here to give a unique ID to each of them.
                if (id == prevId)
                {
                    id += cnt++;
                }
                else
                {
                    prevId = id;
                }

                enumFunc(id, title, exif_mnote_data_get_description(md, i), val, lParam);
            }
        }
    }
    exif_data_unref(ed);

    return TRUE;
}

// Loads exif data without forced fixup of entries
ExifData* get_exif_data_no_fixups(const char* fileName)
{
    ExifLoader* el;
    ExifData* ed;
    const unsigned char* buf;
    size_t buf_size;

    el = exif_loader_new();
    if (!el)
        return NULL;
    exif_loader_write_file(el, fileName);

    exif_loader_get_buf(el, &buf, &buf_size);
    if (!buf_size)
    {
        exif_loader_unref(el);
        return NULL;
    }
    ed = exif_data_new();
    if (!ed)
    {
        exif_loader_unref(el);
        return NULL;
    }
    exif_data_unset_option(ed, ~0);
    exif_data_set_data_type(ed, EXIF_DATA_TYPE_UNKNOWN);
    exif_data_load_data(ed, buf, buf_size);
    exif_loader_unref(el);
    return ed;
}

BOOL WINAPI EXIFReplaceThumbnail(char* fileName, char* newFile, unsigned char* pData, int size)
{
    JPEGData* pJpeg;
    ExifData* pExif;
    BOOL ret = FALSE;

    pJpeg = jpeg_data_new_from_file(fileName);
    if (pJpeg)
    {
        // NOTE: since sometime in 2009, exif_loader_get_data automatically calls
        // exif_data_fix that fixes or removes bogus tags.
        // I think (Patera 2009.10.06) that we better don't do it.
        //      pExif = jpeg_data_get_exif_data(pJpeg);
        pExif = get_exif_data_no_fixups(fileName);
        if (pExif)
        {
            // It is possible there is uncompressed one
            // (e.g. Kodak-210 or Sony-D700) -> smash out all traces of it
            // We also remove all tags that may longer have correct values
            static int tags[] = {
                EXIF_TAG_IMAGE_WIDTH,
                EXIF_TAG_IMAGE_LENGTH,
                EXIF_TAG_BITS_PER_SAMPLE,
                EXIF_TAG_COMPRESSION,
                EXIF_TAG_PHOTOMETRIC_INTERPRETATION,
                EXIF_TAG_STRIP_OFFSETS,
                EXIF_TAG_SAMPLES_PER_PIXEL,
                EXIF_TAG_ROWS_PER_STRIP,
                EXIF_TAG_STRIP_BYTE_COUNTS,
                EXIF_TAG_PLANAR_CONFIGURATION,
                EXIF_TAG_JPEG_PROC,
                EXIF_TAG_YCBCR_COEFFICIENTS,
                EXIF_TAG_YCBCR_SUB_SAMPLING,
                EXIF_TAG_YCBCR_POSITIONING};
            ExifEntry* e;
            ExifContent* ifd = pExif->ifd[EXIF_IFD_1];
            int i;

            for (i = 0; i < sizeof(tags) / sizeof(tags[0]); i++)
            {
                e = exif_content_get_entry(ifd, tags[i]);
                exif_content_remove_entry(ifd, e);
            }
            free(pExif->data);
            if (pData)
            {
                pExif->size = size;
                pExif->data = malloc(size);
                memcpy(pExif->data, pData, size);
                e = exif_entry_new();
                exif_content_add_entry(ifd, e);
                exif_entry_initialize(e, EXIF_TAG_COMPRESSION);
                // JPEG compression
                exif_set_short(e->data, exif_data_get_byte_order(pExif), 6);
                exif_entry_unref(e);
            }
            else
            {
                pExif->size = 0;
                pExif->data = NULL;
            }
            jpeg_data_set_exif_data(pJpeg, pExif);
            exif_data_unref(pExif);
            ret = jpeg_data_save_file(pJpeg, newFile);
        }
        else
        {
            // No APP1 EXIF marker -> make APP0 JFXX
            unsigned int i = 0;
            JPEGSection* pSect = pJpeg->sections;

            while (i < pJpeg->count)
            {
                switch (pSect->marker)
                {
                case JPEG_MARKER_APP0:
                    if ((pSect->content.generic.size < 4) || (*(DWORD*)pSect->content.generic.data == 'XXFJ'))
                    {
                        // invalid or existing JFXX marker -> remove it
                        free(pSect->content.generic.data);
                        memmove(pSect, &pSect[1], (--pJpeg->count - i) * sizeof(JPEGSection));
                        break;
                    }
                    // fall through
                case JPEG_MARKER_SOI:
                    pSect++;
                    i++;
                    break;
                default:
                    // any non-APP0 marker -> insert JFXX before it
                    jpeg_data_append_section(pJpeg);
                    pSect = &pJpeg->sections[i];
                    memmove(&pSect[1], pSect, (pJpeg->count - i - 1) * sizeof(JPEGSection));
                    pSect->marker = JPEG_MARKER_APP0;
                    pSect->content.generic.size = (6 + size);
                    pSect->content.generic.data = malloc(6 + size);
                    *(DWORD*)pSect->content.generic.data = 'XXFJ';
                    // NULL temrination of JFXX, version number
                    ((WORD*)pSect->content.generic.data)[2] = 0x1000;
                    memcpy((char*)pSect->content.generic.data + 6, pData, size);
                    ret = jpeg_data_save_file(pJpeg, newFile);
                    i = pJpeg->count;
                }
            }
        }
        jpeg_data_unref(pJpeg);
    }
    return ret;
}

static void orient_enum_entry(ExifEntry* entry, void* data)
{
    ExifIfd ifd = exif_entry_get_ifd(entry);
    char value[256];

    if (entry->tag == 274 /* orientation */)
    {
        static const char* TR = NULL;
        static const char* BR = NULL;
        static const char* BL = NULL;
        static const char* LT = NULL;
        static const char* RT = NULL;
        static const char* RB = NULL;
        static const char* LB = NULL;
        static int bInited = FALSE;
        PThumbExifInfo pInfo = (PThumbExifInfo)data;

        if (!bInited)
        {
            bInited = TRUE;
            TR = TranslateText("Top-right");
            BR = TranslateText("Bottom-right");
            BL = TranslateText("Bottom-left");
            LT = TranslateText("Left-top");
            RT = TranslateText("Right-top");
            RB = TranslateText("Right-bottom");
            LB = TranslateText("Left-bottom");
        }
        exif_entry_get_value(entry, value, sizeof(value));

        //     if (!stricmp(value, "top - left")) pInfo->Orient = 1; /* default */
        if (!_stricmp(value, TR))
            pInfo->Orient = 2;
        if (!_stricmp(value, BR))
            pInfo->Orient = 3;
        if (!_stricmp(value, BL))
            pInfo->Orient = 4;
        if (!_stricmp(value, LT))
            pInfo->Orient = 5;
        if (!_stricmp(value, RT))
            pInfo->Orient = 6;
        if (!_stricmp(value, RB))
            pInfo->Orient = 7;
        if (!_stricmp(value, LB))
            pInfo->Orient = 8;
        pInfo->flags |= TEI_ORIENT;
    }
    if (entry->tag == 0xa002)
    {
        PThumbExifInfo pInfo = (PThumbExifInfo)data;

        exif_entry_get_value(entry, value, sizeof(value));
        pInfo->Width = atoi(value);
        pInfo->flags |= TEI_WIDTH;
    }
    if (entry->tag == 0xa003)
    {
        PThumbExifInfo pInfo = (PThumbExifInfo)data;

        exif_entry_get_value(entry, value, sizeof(value));
        pInfo->Height = atoi(value);
        pInfo->flags |= TEI_HEIGHT;
    }
} /* orient_enum_entry */

static void orient_enum_ifd(ExifContent* content, void* data)
{
    exif_content_foreach_entry(content, orient_enum_entry, data);
}

BOOL WINAPI EXIFGetOrientationInfo(const char* fileName, PThumbExifInfo pInfo)
{
    ExifData* ed;

    pInfo->Orient = pInfo->flags = 0;

    ed = exif_data_new_from_file(fileName);

    if (ed == NULL)
        return FALSE;

    exif_data_foreach_content(ed, orient_enum_ifd, pInfo);

    exif_data_unref(ed);

    return TRUE;
}

#pragma warning(pop) // FIXME_X64 - docasne potlacen warning, vyresit
