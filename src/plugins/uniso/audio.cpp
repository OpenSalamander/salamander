// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "isoimage.h"
#include "audio.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

// ****************************************************************************
//
// CAudio
//

CAudio::CAudio(CISOImage* image, int track) : CUnISOFSAbstract(image)
{
    Track = track;
}

CAudio::~CAudio()
{
}

BOOL CAudio::DumpInfo(FILE* outStream)
{
    return TRUE;
}

BOOL CAudio::AddFileDir(const char* path, char* fileName,
                        CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    CFileData fd;

    fd.Name = SalamanderGeneral->DupStr(fileName);
    if (fd.Name == NULL)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        dir->Clear(pluginData);
        return FALSE;
    } // if

    fd.NameLen = strlen(fd.Name);
    char* s = strrchr(fd.Name, '.');
    if (s != NULL)
        fd.Ext = s + 1; // ".cvspass" is extension in Windows
    else
        fd.Ext = fd.Name + fd.NameLen;

    fd.PluginData = 0;

    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &fd.LastWrite);

    fd.DosName = NULL;

    fd.Attr = FILE_ATTRIBUTE_READONLY; // vse je defaultne read-only
    fd.Hidden = 0;

    fd.Size = CQuadWord(0, 0);

    // soubor
    fd.IsLink = SalamanderGeneral->IsFileLink(fd.Ext);
    fd.IsOffline = 0;
    if (dir && !dir->AddFile(path, fd, pluginData))
    {
        free(fd.Name);
        dir->Clear(pluginData);
        return Error(IDS_ERROR);
    }

    return TRUE;
}

BOOL CAudio::ListDirectory(char* path, int session, CSalamanderDirectoryAbstract* dir,
                           CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE3("CAudio::ListDirectory(%s, %d, ,)", path, session);

    char audioTrack[MAX_PATH];
    const char* label = Image->GetTrack(Track)->GetLabel();

    if (*label)
    {
        sprintf(audioTrack, "%02d %s", Track + 1, label);
    }
    else
    {
        sprintf(audioTrack, "Audio Track %02d", Track + 1);
    }

    return AddFileDir(path, audioTrack, dir, pluginData);
}

int CAudio::UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                       const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip)
{
    return UNPACK_AUDIO_UNSUP;
}

BOOL CAudio::Open(BOOL quiet)
{
    return TRUE;
}

// ****************************************************************************
//
// CAudioTrack
//

CAudioTrack::~CAudioTrack()
{
    if (Label)
    {
        free(Label);
    }
}

const char* CAudioTrack::GetLabel()
{
    return Label ? Label : "";
}

void CAudioTrack::SetLabel(const char* label)
{
    if (Label)
    {
        free(Label);
    }
    Label = _strdup(label);
}
