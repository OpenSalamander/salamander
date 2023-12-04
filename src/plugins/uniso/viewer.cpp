// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "isoimage.h"

#include "uniso.h"
#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#include "fs.h"

/*
void
CISOImage::CopyDateTimeRecord(CISOImage::CVolumeDateTime &date, BYTE bytes[])
{
#define CpyN(Var, Ofs, N) \
  memcpy(&(date.##Var), bytes + Ofs, N);

  CpyN(Year, 0, 4);
  CpyN(Month, 4, 2);
  CpyN(Day, 6, 2);
  CpyN(Hour, 8, 2);
  CpyN(Minute, 10, 2);
  CpyN(Second, 12, 2);
  CpyN(Second100, 14, 2);
  CpyN(Zone, 16, 1);
#undef CpyN
}
*/

char* ViewerPrintSystemTime(SYSTEMTIME* st)
{
    static char buffer[128];

    char date[50], time[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st->wHour, st->wMinute, st->wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st->wDay, st->wMonth, st->wYear);

    sprintf(buffer, "%s %s", date, time);
    return buffer;
}

char* ViewerStrNcpy(char data[], int count)
{
    static char buffer[129]; // 128 chars + ending '\0'

    if (count > sizeof(buffer) - 1)
        count = sizeof(buffer) - 1; // nechceme si prepsat pamet za 'buffer'em
    strncpy_s(buffer, count + 1, data, _TRUNCATE);

    // kill trailing spaces
    int idx = (int)strlen(buffer);
    if (idx && !buffer[idx - 1])
        idx--;
    while (idx && (buffer[idx - 1] == ' '))
        idx--;

    buffer[idx] = '\0';

    return buffer;
}

static char*
getTrackTypeStr(CISOImage::Track* track)
{
    CALL_STACK_MESSAGE1("getTrackTypeStr( )");

    static char buffer[64];

    ZeroMemory(buffer, 64);

    switch (track->FSType)
    {
    case CISOImage::fsUnknown:
        strcpy(buffer, "Unknown");
        break;
    case CISOImage::fsAudio:
        strcpy(buffer, "Audio");
        break;
    case CISOImage::fsISO9660:
        strcpy(buffer, "ISO 9660");
        break;
    case CISOImage::fsUDF_ISO9660:
        strcpy(buffer, "UDF/ISO 9660");
        break;
    case CISOImage::fsUDF_ISO9660_HFS:
        strcpy(buffer, "UDF/ISO 9660/HFS+");
        break;
    case CISOImage::fsUDF_HFS:
        strcpy(buffer, "UDF/HFS+");
        break;
    case CISOImage::fsISO9660_HFS:
        strcpy(buffer, "ISO 9660/HFS+");
        break;
    case CISOImage::fsHFS:
        strcpy(buffer, "HFS+");
        break;
    case CISOImage::fsUDF_ISO9660_APFS:
        strcpy(buffer, "UDF/ISO 9660/APFS");
        break;
    case CISOImage::fsUDF_APFS:
        strcpy(buffer, "UDF/APFS");
        break;
    case CISOImage::fsISO9660_APFS:
        strcpy(buffer, "ISO 9660/APFS");
        break;
    case CISOImage::fsAPFS:
        strcpy(buffer, "APFS");
        break;
    case CISOImage::fsUDF:
        strcpy(buffer, "UDF");
        break;
    case CISOImage::fsData:
        strcpy(buffer, "Data");
        break;
    case CISOImage::fsXbox:
        strcpy(buffer, "Xbox");
        break;
    }

    if (track->Bootable)
    {
        strcat(buffer, "/bootable");
    }

    if (track->FSType != CISOImage::fsUnknown && track->FSType != CISOImage::fsAudio && track->FSType != CISOImage::fsHFS)
    {
        if (track->Mode == CISOImage::mMode1)
            strcat(buffer, " (Mode 1)");
        else if (track->Mode == CISOImage::mMode2)
            strcat(buffer, " (Mode 2)");
    }

    return buffer;
}

BOOL CISOImage::DumpInfo(FILE* outStream)
{
    CALL_STACK_MESSAGE1("CISOImage::DumpInfo( )");

    // zobrazit info o sessions
    fprintf(outStream, LoadStr(*GetLabel() ? IDS_INFO_LABEL_LABEL : IDS_INFO_LABEL), GetLabel());
    fprintf(outStream, LoadStr(IDS_INFO_CNT_SESSIONS), Session.Count);

    int session = 0;
    int limit = 0;
    int i;
    for (i = 0; i < Tracks.Count; i++)
    {
        if (i == limit)
        {
            int trks = Session[session];
            limit += Session[session];
            session++;
            fprintf(outStream, LoadStr(IDS_INFO_SESSION_NUM), session);
            fprintf(outStream, LoadStr(IDS_INFO_CNT_TRACKS), trks);
        }

        DWORD size = (DWORD)((Tracks[i]->End - Tracks[i]->Start) / 1024);
        fprintf(outStream, LoadStr(*Tracks[i]->GetLabel() ? IDS_INFO_TRACK_TYPE_SIZE_LABEL : IDS_INFO_TRACK_TYPE_SIZE), i + 1, getTrackTypeStr(Tracks[i]), size, Tracks[i]->GetLabel());
        if (OpenTrack(i, TRUE))
        {
            Tracks[i]->FileSystem->DumpInfo(outStream);
        }
    } // for

    /*
  fprintf(outStream, "Bootable:                      %s\n", Bootable ? LoadStr(IDS_YES): LoadStr(IDS_NO));
  if (Bootable)
  {
    fprintf(outStream, "Boot System Identifier:        %s\n", MyStrNcpy((char *)BootRecord.BootSystemIdentifier, 32));
    fprintf(outStream, "Boot Identifier:               %s\n", MyStrNcpy((char *)BootRecord.BootIdentifier, 32));
  }

*/
    return TRUE;
}
