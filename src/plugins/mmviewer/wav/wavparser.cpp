// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _WAV_SUPPORT_

#include "wavparser.h"
#include <mmreg.h>
#include <msacm.h>
#include "..\mmviewer.rh"
#include "..\mmviewer.rh2"
#include "..\lang\lang.rh"
#include "..\output.h"

char* LoadStr(int resID);
char* FStr(const char* format, ...);
void FormatSize2(__int64 size, char* str_size, BOOL nozero = FALSE);

#define SAFE_DELETE(x) \
    { \
        if (x) \
        { \
            delete (x); \
            (x) = NULL; \
        } \
    }
#define SAFE_DELETE_ARRAY(x) \
    { \
        if (x) \
        { \
            delete[] (x); \
            (x) = NULL; \
        } \
    }
#define SAFE_FREE(x) \
    { \
        if (x) \
        { \
            free(x); \
            (x) = NULL; \
        } \
    }
#define SAFE_CALL(obj, method) \
    if (obj) \
    obj->method

#ifndef SIZEOF_WAVEFORMATEX
#define SIZEOF_WAVEFORMATEX(pwfx) ((WAVE_FORMAT_PCM == (pwfx)->wFormatTag) ? sizeof(PCMWAVEFORMAT) : (sizeof(WAVEFORMATEX) + (pwfx)->cbSize))
#endif

typedef struct
{
    char tag[4];
    int str_id;
    char* str;
} WAV_TAG;

void CMMIO::Close(void)
{
    if (m_hmmio)
        mmioClose(m_hmmio, 0);

    if (m_pwfx)
    {
        free(m_pwfx);
        m_pwfx = NULL;
    }
}

BOOL CMMIO::Open(char* fname)
{
    Close();

    m_hmmio = mmioOpen(fname, NULL, MMIO_ALLOCBUF | MMIO_READ);

    return IsOpened();
}
/*
HRESULT CMMIO::ReadInfoChunk()
{
	MMCKINFO        ckIn;           // chunk info. for general use.

    // Search the input file for for the 'fmt ' chunk.
    ckIn.ckid = mmioFOURCC('I', 'N', 'F', 'O');
    if (0 != mmioDescend(m_hmmio, &ckIn, &m_ckRiff, MMIO_FINDCHUNK))
        return E_FAIL;

    WAV_TAG wavtags[] = 
    {
		{'I','N','A','M',   IDS_WAV_TITLE,
		{'I','S','B','J',   IDS_WAV_SUBJECT,
		{'I','C','R','D',	IDS_WAV_CREATION_DATE,
		{'I','E','N','G',	IDS_WAV_ENGINEER,
		{'I','C','O','P',	IDS_WAV_COPYRIGHT,
		{'I','A','R','L',	IDS_WAV_ARCHIVAL_LOCATION,
		{'I','A','R','T',	IDS_WAV_ARTIST,
		{'I','C','M','S',	IDS_WAV_COMMISSIONED,
		{'I','C','R','P',	IDS_WAV_CROPPED,
		{'I','D','I','M',	IDS_WAV_DIMENSIONS,
		{'I','D','P','I',	IDS_WAV_DOTS_PER_INCH,
		{'I','G','N','R',	IDS_WAV_GENRE,
		{'I','K','E','Y',	IDS_WAV_KEYWORDS,
		{'I','L','G','T',	IDS_WAV_LIGHTNESS,
		{'I','M','E','D',	IDS_WAV_MEDIUM,
		{'I','P','L','T',	IDS_WAV_PALETTE_SETTING,
		{'I','P','R','D',	IDS_WAV_PRODUCT,
		{'I','S','H','P',	IDS_WAV_SHARPNESS,
		{'I','S','R','C',	IDS_WAV_SOURCE_ALBUM,
		{'I','S','R','F',	IDS_WAV_SOURCE_FORM,
		{'I','T','C','H',	IDS_WAV_TECHNICIAN,
		{'D','I','S','P',	IDS_WAV_SOUND_SCHEME_TITLE,
		{'T','L','E','N',	IDS_WAV_TEXT_LENGTH_MS,
		{'T','R','C','K',	IDS_WAV_TRACK_NUMBER,
		{'T','U','R','L',	IDS_WAV_URL,
		{'T','V','E','R',	IDS_WAV_VERSION,
		{'L','O','C','A',	IDS_WAV_LOCATION,
		{'T','O','R','G',	IDS_WAV_ORGANISATION,
		{'I','C','M','T',	IDS_WAV_COMMENTS
    };
}*/

HRESULT CMMIO::ReadMMIO()
{
    if (m_hmmio)
    {
        MMCKINFO ckIn;               // chunk info. for general use.
        PCMWAVEFORMAT pcmWaveFormat; // Temp PCM structure to load in.

        m_pwfx = NULL;

        if ((0 != mmioDescend(m_hmmio, &m_ckRiff, NULL, 0)))
            return E_FAIL;

        // Check to make sure this is a valid wave file
        if ((m_ckRiff.ckid != FOURCC_RIFF) ||
            (m_ckRiff.fccType != mmioFOURCC('W', 'A', 'V', 'E')))
            return E_FAIL;

        // Search the input file for for the 'fmt ' chunk.
        ckIn.ckid = mmioFOURCC('f', 'm', 't', ' ');
        if (0 != mmioDescend(m_hmmio, &ckIn, &m_ckRiff, MMIO_FINDCHUNK))
            return E_FAIL;

        // Expect the 'fmt' chunk to be at least as large as <PCMWAVEFORMAT>;
        // if there are extra parameters at the end, we'll ignore them
        if (ckIn.cksize < (LONG)sizeof(PCMWAVEFORMAT))
            return E_FAIL;

        // Read the 'fmt ' chunk into <pcmWaveFormat>.
        if (mmioRead(m_hmmio, (HPSTR)&pcmWaveFormat,
                     sizeof(pcmWaveFormat)) != sizeof(pcmWaveFormat))
            return E_FAIL;

        // Allocate the waveformatex, but if its not pcm format, read the next
        // word, and thats how many extra bytes to allocate.
        if (pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_PCM)
        {
            m_pwfx = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX));
            if (NULL == m_pwfx)
                return E_FAIL;

            // Copy the bytes from the pcm structure to the waveformatex structure
            memcpy(m_pwfx, &pcmWaveFormat, sizeof(pcmWaveFormat));
            m_pwfx->cbSize = 0;
        }
        else
        {
            // Read in length of extra bytes.
            WORD cbExtraBytes = 0L;
            if (mmioRead(m_hmmio, (CHAR*)&cbExtraBytes, sizeof(WORD)) != sizeof(WORD))
                return E_FAIL;

            m_pwfx = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX) + cbExtraBytes);
            if (NULL == m_pwfx)
                return E_FAIL;

            // Copy the bytes from the pcm structure to the waveformatex structure
            memcpy(m_pwfx, &pcmWaveFormat, sizeof(pcmWaveFormat));
            m_pwfx->cbSize = cbExtraBytes;

            // Now, read those extra bytes into the structure, if cbExtraAlloc != 0.
            if (mmioRead(m_hmmio, (CHAR*)(((BYTE*)&(m_pwfx->cbSize)) + sizeof(WORD)),
                         cbExtraBytes) != cbExtraBytes)
            {
                free(m_pwfx);
                return E_FAIL;
            }
        }

        // Ascend the input file out of the 'fmt ' chunk.
        if (0 != mmioAscend(m_hmmio, &ckIn, 0))
        {
            free(m_pwfx);
            return E_FAIL;
        }

        //ReadInfoChunk();

        return S_OK;
    }

    return E_FAIL;
}

HRESULT CMMIO::ResetFile()
{
    if (m_hmmio == NULL)
        return E_FAIL;

    if (-1 == mmioSeek(m_hmmio, m_ckRiff.dwDataOffset + sizeof(FOURCC), SEEK_SET))
        return E_FAIL;

    m_ck.ckid = mmioFOURCC('d', 'a', 't', 'a');
    if (0 != mmioDescend(m_hmmio, &m_ck, &m_ckRiff, MMIO_FINDCHUNK))
        return E_FAIL;

    return S_OK;
}

CParserResultEnum
CParserWAV::OpenFile(const char* fileName)
{
    //mmioOpen chce nekonstantni char ;-0
    char fnamecpy[MAX_PATH];
    strcpy(fnamecpy, fileName);

    if (!mmio.Open(fnamecpy))
        return preOpenError;

    return preOK;
}

CParserResultEnum
CParserWAV::CloseFile()
{
    mmio.Close();
    return preOK;
}

CParserResultEnum
CParserWAV::GetFileInfo(COutputInterface* output)
{
    if (mmio.IsOpened())
    {
        if (FAILED(mmio.ReadMMIO()))
            return preUnknownFile;

        DWORD filesize = mmio.GetSize();

        //if (filesize == 0)
        //  return preCorruptedFile;

        int s = 0;
        DWORD bps;
        DWORD hz;
        DWORD chn;
        char size[64], time[64];
        char mode[256];
        char compression[256];
        WAVEFORMATEX* m_pwfx = mmio.m_pwfx;

        if (m_pwfx->wFormatTag == WAVE_FORMAT_PCM)
        {
            bps = m_pwfx->wBitsPerSample;
            hz = m_pwfx->nSamplesPerSec;
            chn = m_pwfx->nChannels;
            lstrcpy(compression, "PCM");
        }
        else //for non-pcm formats
        {
            bps = m_pwfx->wBitsPerSample;
            hz = m_pwfx->nSamplesPerSec;
            chn = m_pwfx->nChannels;

            ACMFORMATTAGDETAILS aftd;
            memset(&aftd, 0, sizeof(ACMFORMATTAGDETAILS));
            aftd.cbStruct = sizeof(ACMFORMATTAGDETAILS);
            aftd.dwFormatTag = m_pwfx->wFormatTag;
            MMRESULT mmr = acmFormatTagDetails(NULL, &aftd, ACM_FORMATTAGDETAILSF_FORMATTAG);
            if (mmr == 0)
                lstrcpy(compression, aftd.szFormatTag);
            else
                lstrcpy(compression, "?");
        }

        double div = double(bps / 8 * hz * chn);
        if (div > 0.0f)
            s = (int)(double(filesize) / div);

        if (s / 3600)
            lstrcpy(time, FStr("%02lu:%02lu:%02lu", s / 3600, s / 60 % 60, s % 60));
        else
            lstrcpy(time, FStr("%02lu:%02lu", s / 60 % 60, s % 60));

        if (chn >= 2)
            lstrcpy(mode, LoadStr(IDS_WAV_STEREO));
        else
            lstrcpy(mode, LoadStr(IDS_WAV_MONO));

        output->AddHeader(LoadStr(IDS_WAV_INFO));
        output->AddItem(LoadStr(IDS_OGG_LENGTH), time);
        output->AddItem(LoadStr(IDS_WAV_COMPRESSION), compression);
        FormatSize2(bps, size);
        output->AddItem(LoadStr(IDS_WAV_BITRATE), size);
        FormatSize2(hz, size);
        output->AddItem(LoadStr(IDS_WAV_FREQUENCY), size);
        output->AddItem(LoadStr(IDS_WAV_MODE), mode);

        return preOK;
    }

    return preUnknownFile;
}

#endif
