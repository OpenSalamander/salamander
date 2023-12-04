// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _MP4_SUPPORT_

#include "mp4head.h"

#define ADIF_MAX_SIZE 30 /* Should be enough */
#define ADTS_MAX_SIZE 10 /* Should be enough */

static int sample_rates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};

int StringComp(const char* str1, const char* str2, unsigned long len)
{
    signed int c1 = 0, c2 = 0;

    while (len--)
    {
        c1 = *str1++;
        c2 = *str2++;

        if (c1 == 0 || c1 != c2)
            break;
    }

    return c1 - c2;
}

static bool ReadADIFHeader(FILE* file, AACHEAD_DECODED* info)
{
    int bitstream;
    unsigned char buffer[ADIF_MAX_SIZE];
    int skip_size = 0;
    int sf_idx;

    /* Get ADIF header data */
    info->header_type = 1;

    if (fread(buffer, ADIF_MAX_SIZE, 1, file) != 1)
        return false;

    /* copyright string */
    if (buffer[0] & 0x80)
        skip_size += 9; /* skip 9 bytes */

    bitstream = buffer[0 + skip_size] & 0x10;
    info->bitrate = ((unsigned int)(buffer[0 + skip_size] & 0x0F) << 19) |
                    ((unsigned int)buffer[1 + skip_size] << 11) |
                    ((unsigned int)buffer[2 + skip_size] << 3) |
                    ((unsigned int)buffer[3 + skip_size] & 0xE0);

    if (bitstream == 0)
    {
        info->object_type = ((buffer[6 + skip_size] & 0x01) << 1) | ((buffer[7 + skip_size] & 0x80) >> 7);
        sf_idx = (buffer[7 + skip_size] & 0x78) >> 3;
    }
    else
    {
        info->object_type = (buffer[4 + skip_size] & 0x18) >> 3;
        sf_idx = ((buffer[4 + skip_size] & 0x07) << 1) | ((buffer[5 + skip_size] & 0x80) >> 7);
    }
    info->sampling_rate = sample_rates[sf_idx];

    return true;
}

static int ReadADTSHeader(FILE* file, AACHEAD_DECODED* info)
{
    /* Get ADTS header data */
    unsigned char buffer[ADTS_MAX_SIZE];
    int frames, framesinsec = 0, t_framelength = 0, frame_length = 0, sr_idx = 0, ID = 0;
    int second = 0, pos = 0;
    float frames_per_sec = 0;
    unsigned long bytes;

    info->header_type = 2;

    /* Read all frames to ensure correct time and bitrate */
    for (frames = 0; /* */; frames++, framesinsec++)
    {
        pos = ftell(file);

        /* 12 bit SYNCWORD */
        bytes = fread(buffer, 1, ADTS_MAX_SIZE, file);

        if (bytes != ADTS_MAX_SIZE)
        {
            /* Bail out if no syncword found */
            break;
        }

        /* check syncword */
        if (!((buffer[0] == 0xFF) && ((buffer[1] & 0xF6) == 0xF0)))
            break;

        if (!frames)
        {
            /* fixed ADTS header is the same for every frame, so we read it only once */
            /* Syncword found, proceed to read in the fixed ADTS header */
            ID = buffer[1] & 0x08;
            info->object_type = (buffer[2] & 0xC0) >> 6;
            sr_idx = (buffer[2] & 0x3C) >> 2;
            info->channels = ((buffer[2] & 0x01) << 2) | ((buffer[3] & 0xC0) >> 6);

            frames_per_sec = sample_rates[sr_idx] / 1024.f;
        }

        /* ...and the variable ADTS header */
        if (ID == 0)
        {
            info->version = 4;
            frame_length = (((unsigned int)buffer[4]) << 5) | ((unsigned int)buffer[5] >> 3);
        }
        else
        { /* MPEG-2 */
            info->version = 2;
            frame_length = ((((unsigned int)buffer[3] & 0x3)) << 11) | (((unsigned int)buffer[4]) << 3) | (buffer[5] >> 5);
        }

        t_framelength += frame_length;

        if (framesinsec == 43)
            framesinsec = 0;

        if (fseek(file, frame_length - ADTS_MAX_SIZE, SEEK_CUR) != 0)
            break;
    }

    info->sampling_rate = sample_rates[sr_idx];
    info->bitrate = (int)(((t_framelength / frames) * (info->sampling_rate / 1024.0)) + 0.5) * 8;
    info->length = (int)((float)(frames / frames_per_sec)) * 1000;

    return true;
}

bool DecodeAACHeader(FILE* file, AACHEAD_DECODED* info)
{
    unsigned long tagsize = 0;
    char buffer[10];
    unsigned long file_len = 0;
    unsigned char adxx_id[5];
    unsigned long tmp;

    memset(info, 0, sizeof(AACHEAD_DECODED));

    fseek(file, 0, SEEK_END);
    file_len = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Skip the tag, if it's there */
    tmp = fread(buffer, 10, 1, file);

    if (StringComp(buffer, "ID3", 3) == 0)
    {
        /* high bit is not used */
        tagsize = (buffer[6] << 21) | (buffer[7] << 14) | (buffer[8] << 7) | (buffer[9] << 0);

        fseek(file, tagsize, SEEK_CUR);

        tagsize += 10;
    }
    else
    {
        tagsize = 0;

        fseek(file, 0, SEEK_SET);
    }

    if (file_len)
        file_len -= tagsize;

    tmp = fread(adxx_id, 2, 1, file);
    if (tmp != 1)
        return false;

    adxx_id[5 - 1] = 0;
    info->length = 0;

    /* Determine the header type of the file, check the first two bytes */
    if (StringComp((const char*)adxx_id, "AD", 2) == 0)
    {
        /* We think its an ADIF header, but check the rest just to make sure */
        tmp = fread(adxx_id + 2, 2, 1, file);
        if (tmp != 1)
            return false;

        if (StringComp((const char*)adxx_id, "ADIF", 4) == 0)
        {
            if (!ReadADIFHeader(file, info))
                return false;
        }
    }
    else
    {
        /* No ADIF, check for ADTS header */
        if ((adxx_id[0] == 0xFF) && ((adxx_id[1] & 0xF6) == 0xF0))
        {
            /* ADTS  header located */
            /* Since this routine must work for streams, we can't use the seek function to go backwards, thus 
			   we have to use a quick hack as seen below to go back where we need to. */

            fseek(file, -2, SEEK_CUR);

            if (!ReadADTSHeader(file, info))
                return false;
        }
        else
        {
            /* Unknown/headerless AAC file, assume format: */
            return false;
        }
    }

    return true;
}

/*
#ifdef TEST

#include <stdio.h>

void main(int argc, char *argv[])
{
    faadAACInfo info;
    unsigned long *seek_table = NULL;
    int seek_table_len = 0;
    char *header, *object;

    if (argc < 2)
    {
        fprintf(stderr, "USAGE: aacinfo aacfile.aac\n");
        return;
    }

    get_AAC_format(argv[1], &info, &seek_table, &seek_table_len);

    fprintf(stdout, "MPEG version: %d\n", info.version);
    fprintf(stdout, "channels: %d\n", info.channels);
    fprintf(stdout, "sampling_rate: %d\n", info.sampling_rate);
    fprintf(stdout, "bitrate: %d\n", info.bitrate);
    fprintf(stdout, "length: %.3f\n", (float)info.length/1000.0);

    switch (info.object_type)
    {
    case 0:
        object = "MAIN";
        break;
    case 1:
        object = "LC";
        break;
    case 2:
        object = "SSR";
        break;
    case 3:
        object = "LTP";
        break;
    }
    fprintf(stdout, "object_type: %s\n", object);

    switch (info.header_type)
    {
    case 0:
        header = "RAW";
        break;
    case 1:
        header = "ADIF";
        break;
    case 2:
        header = "ADTS";
        break;
    }
    fprintf(stdout, "header_type: %s\n", header);

}

#endif*/

#endif