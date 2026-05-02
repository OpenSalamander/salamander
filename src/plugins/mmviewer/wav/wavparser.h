// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WAV_SUPPORT_

#include "..\parser.h"
#include <mmsystem.h>

//****************************************************************************
//
// CMMIO
//

class CMMIO
{
public:
    WAVEFORMATEX* m_pwfx; // Pointer to WAVEFORMATEX structure
    HMMIO m_hmmio;        // MM I/O handle for the WAVE
    MMCKINFO m_ck;        // Multimedia RIFF chunk
    MMCKINFO m_ckRiff;    // Use in opening a WAVE file
    DWORD m_dwSize;       // The size of the wave file
    MMIOINFO m_mmioinfoOut;
    DWORD m_dwFlags;

public:
    CMMIO() : m_pwfx(NULL), m_hmmio(NULL), m_dwSize(0), m_dwFlags(0) {}

    ~CMMIO() { Close(); }

    BOOL Open(char* fname);
    void Close(void);

    BOOL IsOpened(void) const { return m_hmmio != NULL; }

    HRESULT ReadMMIO();
    HRESULT ResetFile();

    DWORD GetSize()
    {
        if (FAILED(ResetFile()))
            return 0;

        return m_ck.cksize;
    }
};

//****************************************************************************
//
// CParserWAV
//

class CParserWAV : public CParserInterface
{
private:
    CMMIO mmio;

public:
    CParserWAV() {}

    virtual CParserResultEnum OpenFile(const char* fileName);
    virtual CParserResultEnum CloseFile();
    virtual CParserResultEnum GetFileInfo(COutputInterface* output);
};

#endif
