// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "parser.h"
#include "output.h"
#include "renderer.h"
#include "mmviewer.h"
#include "mmviewer.rh"
#include "mmviewer.rh2"
#include "lang\lang.rh"

#ifdef _MP4_SUPPORT_
#include "mp4\\mp4parser.h"
#endif

#ifdef _MPG_SUPPORT_
#include "mp3\\mpgparser.h"
#endif

#ifdef _WAV_SUPPORT_
#include "wav\\wavparser.h"
#endif

#ifdef _WMA_SUPPORT_
#include "wma\\wmaparser.h"
#endif

#ifdef _VQF_SUPPORT_
#include "vqf\\vqfparser.h"
#endif

#ifdef _OGG_SUPPORT_
#include "ogg\\oggparser.h"
#endif

#ifdef _MOD_SUPPORT_
#include "mod\\modparser.h"
#endif

void ShowParserError(HWND hParent, CParserResultEnum result)
{
    int strID = -1;
    switch (result)
    {
    case preOK:
        return;
    case preOutOfMemory:
        strID = IDS_MMV_OOM;
        break;
    case preUnknownFile:
        strID = IDS_MMV_UNKNOWN_FILE;
        break;
    case preOpenError:
        strID = IDS_MMV_OPEN_ERROR;
        break;
    case preReadError:
        strID = IDS_MMV_READ_ERROR;
        break;
    case preWriteError:
        strID = IDS_MMV_WRITE_ERROR;
        break;
    case preSeekError:
        strID = IDS_MMV_SEEK_ERROR;
        break;
    case preCorruptedFile:
        strID = IDS_MMV_CORRUPTED_FILE;
        break;
    case preExtensionError:
        strID = IDS_MMV_EXTENSION_ERROR;
        break;
    }
    const char* text;
    if (strID == -1)
        text = "Unknown error";
    else
        text = LoadStr(strID);
    SalGeneral->SalMessageBox(hParent, text, LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONEXCLAMATION);
}

CParserResultEnum
CreateAppropriateParser(const char* fileName, CParserInterface** parser)
{
    CParserInterface* iface = NULL;

    const char* ext = strrchr(fileName, '.'); // ".cvspass" ve Windows je pripona

    if (!ext)
        return preUnknownFile;

#ifdef _MP4_SUPPORT_
    if ((SalGeneral->StrICmp(ext, ".mp4") == 0) || (SalGeneral->StrICmp(ext, ".m4a") == 0) || (SalGeneral->StrICmp(ext, ".aac") == 0))
        iface = new CParserMP4();
#endif

#ifdef _MPG_SUPPORT_
    if ((SalGeneral->StrICmp(ext, ".mp3") == 0) || (SalGeneral->StrICmp(ext, ".mp2") == 0))
        iface = new CParserMPG();
#endif

#ifdef _WAV_SUPPORT_
    if ((SalGeneral->StrICmp(ext, ".wav") == 0) || (SalGeneral->StrICmp(ext, ".wave") == 0))
        iface = new CParserWAV();
#endif

#ifdef _WMA_SUPPORT_
    if (SalGeneral->StrICmp(ext, ".wma") == 0)
        iface = new CParserWMA();
#endif

#ifdef _VQF_SUPPORT_
    if (SalGeneral->StrICmp(ext, ".vqf") == 0)
        iface = new CParserVQF();
#endif

#ifdef _OGG_SUPPORT_
    if (SalGeneral->StrICmp(ext, ".ogg") == 0)
        iface = new CParserOGG();
#endif

#ifdef _MOD_SUPPORT_
    if ((SalGeneral->StrICmp(ext, ".it") == 0) || (SalGeneral->StrICmp(ext, ".s3m") == 0) || (SalGeneral->StrICmp(ext, ".stm") == 0) ||
        (SalGeneral->StrICmp(ext, ".xm") == 0) || (SalGeneral->StrICmp(ext, ".mod") == 0) || (SalGeneral->StrICmp(ext, ".mtm") == 0) ||
        (SalGeneral->StrICmp(ext, ".669") == 0))
        iface = new CParserMOD();
#endif

    if (iface == NULL)
        return preOutOfMemory;

    CParserResultEnum result;
    result = iface->OpenFile(fileName);
    if (result != preOK)
        delete iface;

    if (result == preOK)
    {
        *parser = iface;
        return preOK;
    }
    else
        return preUnknownFile; // nenasli jsme zadny parser schopny otevrit dany soubor
}
