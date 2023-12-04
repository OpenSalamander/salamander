// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _MOD_SUPPORT_

#include "modparser.h"
#include "..\mmviewer.rh"
#include "..\mmviewer.rh2"
#include "..\lang\lang.rh"
#include "..\output.h"

extern char* LoadStr(int resID);
char* FStr(const char* format, ...);

BOOL CParserMOD::ReadImpulseTrackerModule()
{
    DWORD magic;

    if (fseek(f, 0, SEEK_SET) != 0)
        return FALSE;
    if (fread(&magic, 4, 1, f) != 1)
        return FALSE;

    if (magic == 0x4D504D49)
    {
        memset(modulename, '\0', sizeof(modulename));
        if (fread(modulename, 26, 1, f) != 1)
            return FALSE;

        if (fseek(f, 0x22, SEEK_SET) != 0)
            return FALSE;
        if (fread(&instruments, 2, 1, f) != 1)
            return FALSE;
        if (fread(&samples, 2, 1, f) != 1)
            return FALSE;
        if (fread(&patterns, 2, 1, f) != 1)
            return FALSE;
    }
    else
        return FALSE;

    return TRUE;
}

BOOL CParserMOD::ReadScreamTrackerModule()
{
    BYTE magic;
    BYTE type;
    DWORD magic2;

    if (fseek(f, 0x1c, SEEK_SET) != 0)
        return FALSE;
    if (fread(&magic, 1, 1, f) != 1)
        return FALSE;

    if (magic == 0x1a)
    {
        memset(modulename, '\0', sizeof(modulename));

        if (fread(&type, 1, 1, f) != 1)
            return FALSE;

        if (type == 16) //jde o S3M
        {
            if (fseek(f, 0x2C, SEEK_SET) != 0)
                return FALSE;
            if (fread(&magic2, 4, 1, f) != 1)
                return FALSE;

            if (magic2 != 0x4D524353)
                return FALSE;

            if (fseek(f, 0x22, SEEK_SET) != 0)
                return FALSE;
            if (fread(&samples, 2, 1, f) != 1)
                return FALSE;
            if (fread(&patterns, 2, 1, f) != 1)
                return FALSE;

            if (fseek(f, 0, SEEK_SET) != 0)
                return FALSE;
            if (fread(modulename, 28, 1, f) != 1)
                return FALSE;
            modulename[27] = '\0';
        }
        else
        {
            samples = 0;

            BYTE p;
            if (fseek(f, 0x21, SEEK_SET) != 0)
                return FALSE;
            if (fread(&p, 1, 1, f) != 1)
                return FALSE;
            patterns = p;

            if (fseek(f, 0, SEEK_SET) != 0)
                return FALSE;
            if (fread(modulename, 20, 1, f) != 1)
                return FALSE;
            modulename[19] = '\0';
        }

        instruments = 0;
    }
    else
        return FALSE;

    return TRUE;
}

BOOL CParserMOD::ReadExtendedModule()
{
    const char* magic_str = "Extended Module: ";
    char magic[18];

    samples = 0;

    if (fseek(f, 0, SEEK_SET) != 0)
        return FALSE;

    memset(magic, '\0', sizeof(magic));

    if (fread(magic, 17, 1, f) != 1)
        return FALSE;

    if (strncmp(magic, magic_str, 17) == 0)
    {
        memset(modulename, '\0', sizeof(modulename));
        if (fread(modulename, 20, 1, f) != 1)
            return FALSE;

        int i;
        for (i = 19; i >= 0; i--)
        {
            if (modulename[i] == 0x20)
                modulename[i] = '\0';
            else
                break;
        }

        BYTE submagic;
        if (fseek(f, 37, SEEK_SET) != 0)
            return FALSE;
        if (fread(&submagic, 1, 1, f) != 1)
            return FALSE;
        if (submagic != 0x1a)
            return FALSE;

        DWORD headsize;
        if (fseek(f, 60, SEEK_SET) != 0)
            return FALSE;
        if (fread(&headsize, 4, 1, f) != 1)
            return FALSE;

        if (fseek(f, 60 + 10, SEEK_SET) != 0)
            return FALSE;
        if (fread(&patterns, 2, 1, f) != 1)
            return FALSE;

        if (fseek(f, 60 + 12, SEEK_SET) != 0)
            return FALSE;
        if (fread(&instruments, 2, 1, f) != 1)
            return FALSE;
    }
    else
        return FALSE;

    return TRUE;
}

BOOL CParserMOD::ReadMultiTrackerModule()
{
    const char* magic_str = "MTM";
    char magic[4];

    if (fseek(f, 0, SEEK_SET) != 0)
        return FALSE;

    memset(magic, '\0', sizeof(magic));

    if (fread(magic, 3, 1, f) != 1)
        return FALSE;

    if (strncmp(magic, magic_str, 3) == 0)
    {
        if (fseek(f, 4, SEEK_SET) != 0)
            return FALSE;

        memset(modulename, '\0', sizeof(modulename));
        if (fread(modulename, 20, 1, f) != 1)
            return FALSE;
        modulename[19] = '\0';

        instruments = 0;

        BYTE b;
        if (fseek(f, 26, SEEK_SET) != 0)
            return FALSE;
        if (fread(&b, 1, 1, f) != 1)
            return FALSE;
        patterns = b;

        if (fseek(f, 30, SEEK_SET) != 0)
            return FALSE;
        if (fread(&b, 1, 1, f) != 1)
            return FALSE;
        samples = b;
    }
    else
        return FALSE;

    return TRUE;
}

BOOL CParserMOD::ReadProTrackerModule()
{
    //u tohodle stupidniho formatu nejde kloudne zjistit, zda se opravdu
    //jedna o mod soubor, tak se zkusim chytit aspon nuloveho terminatoru
    //za nazvem songu. Pokud tam je, beru to jako ze je to MOD.
    //Take zkontroluji, zda tech 19 znaku jsou norm. znaky

    memset(modulename, '\0', sizeof(modulename));

    if (fseek(f, 0, SEEK_SET) != 0)
        return FALSE;
    if (fread(modulename, 20, 1, f) != 1)
        return FALSE;

    int i;
    for (i = 0; i < 19; i++)
        if ((modulename[i] < 32) || (modulename[i] > 126))
            return FALSE;

    if (modulename[19] != '\0')
        return FALSE;

    patterns = samples = instruments = 0;

    return TRUE;
}

BOOL CParserMOD::Read669Module()
{
    const char* magic_str = "if";
    const char* magic_str_extended = "JN";
    char magic[3];

    if (fseek(f, 0, SEEK_SET) != 0)
        return FALSE;

    memset(magic, '\0', sizeof(magic));

    if (fread(magic, 2, 1, f) != 1)
        return FALSE;

    if ((strncmp(magic, magic_str, 2) == 0) || (strncmp(magic, magic_str_extended, 2) == 0))
    {
        if (fseek(f, 2, SEEK_SET) != 0)
            return FALSE;

        memset(modulename, '\0', sizeof(modulename));
        if (fread(modulename, 108, 1, f) != 1)
            return FALSE;
        modulename[108] = '\0';

        //zjistim kde text zacina
        int i;
        for (i = 0; i < 108; i++)
        {
            if ((modulename[i] != 0x20) && (modulename[i] != '\0'))
                break;
        }

        int j = i;
        while (i < 107)
        {
            modulename[i - j] = modulename[i];
            modulename[i] = 0x20;
            i++;
        }

        //smazu koncove mezery
        for (i = 107; i >= 0; i--)
        {
            if (modulename[i] == 0x20)
                modulename[i] = '\0';
            else
                break;
        }

        instruments = 0;

        BYTE b;

        //if (fseek(f,30,SEEK_SET)!=0) return FALSE;
        if (fread(&b, 1, 1, f) != 1)
            return FALSE;
        samples = b;

        //if (fseek(f,26,SEEK_SET)!=0) return FALSE;
        if (fread(&b, 1, 1, f) != 1)
            return FALSE;
        patterns = b;
    }
    else
        return FALSE;

    return TRUE;
}

CParserResultEnum
CParserMOD::OpenFile(const char* fileName)
{
    CloseFile();

    f = fopen(fileName, "rb");

    if (!f)
        return preOpenError;

    return preOK;
}

CParserResultEnum
CParserMOD::CloseFile()
{
    if (f)
    {
        fclose(f);
        f = NULL;
    }

    return preOK;
}

CParserResultEnum
CParserMOD::GetFileInfo(COutputInterface* output)
{
    if (f)
    {
        if (!ReadImpulseTrackerModule())
            if (!ReadExtendedModule())
                if (!ReadScreamTrackerModule())
                    if (!Read669Module())
                        if (!ReadMultiTrackerModule())
                            if (!ReadProTrackerModule())
                                return preUnknownFile;

        output->AddHeader(LoadStr(IDS_MOD_INFO));
        output->AddItem(LoadStr(IDS_MOD_TITLE), modulename);
        output->AddItem(LoadStr(IDS_MOD_PATTERNS), FStr("%lu", patterns));
        output->AddItem(LoadStr(IDS_MOD_SAMPLES), FStr("%lu", samples));
        output->AddItem(LoadStr(IDS_MOD_INSTRUMENTS), FStr("%lu", instruments));

        return preOK;
    }

    return preUnknownFile;
}

#endif
