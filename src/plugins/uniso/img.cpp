// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "isoimage.h"

#include "uniso.h"
#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#define CCD_ROW_LEN 1024

struct TextRow
{
    char* Row;
    char *Id, *Value;

    TextRow(char* row)
    {
        Row = new char[strlen(row) + 1];
        strcpy(Row, row);

        Id = strtok(Row, "=");
        Value = strtok(NULL, "\n");
    }

    ~TextRow()
    {
        delete[] Row;
    }
};

enum ECCDType
{
    ccdNone,
    ccdHeader,
    ccdDisc,
    ccdSession,
    ccdEntry,
    ccdTrack
};

struct TextSection
{
    TIndirectArray<TextRow> Rows;
    ECCDType Type;

    TextSection(ECCDType type) : Rows(10, 5, dtDelete)
    {
        Type = type;
    }

    void Add(char* row)
    {
        Rows.Add(new TextRow(row));
    }
};

struct CCDSession
{
    int PreGapMode;
    int PreGapSubC;

    CCDSession(int preGapMode, int preGapSubC)
    {
        PreGapMode = preGapMode;
        PreGapSubC = preGapSubC;
    }
};

struct CCDTrack
{
    int Mode;
    int Index;

    CCDTrack(int mode, int index)
    {
        Mode = mode;
        Index = index;
    }
};

struct CCDEntry
{
    BYTE Session;
    DWORD Point;
    DWORD PLBA;
    BYTE ADR;

    CCDEntry(BYTE session, DWORD point, DWORD plba, BYTE adr)
    {
        Session = session;
        Point = point;
        PLBA = plba;
        ADR = adr;
    }
};

struct CCD
{
    TIndirectArray<CCDTrack> TrackArray;
    TIndirectArray<CCDEntry> EntryArray;
    TIndirectArray<CCDSession> SessionArray;
    BOOL DisplayMissingCCDWarning;

    CCD() : TrackArray(10, 5, dtDelete), EntryArray(100, 10, dtDelete), SessionArray(5, 2, dtDelete)
    {
        DisplayMissingCCDWarning = TRUE;
    }
};

//

static BOOL
processHeaderSection(TextSection* section)
{
    CALL_STACK_MESSAGE1("processHeaderSection()");

    int i;
    for (i = 0; i < section->Rows.Count; i++)
    {
        //    TextRow *row = section->Rows[i];
        if (strcmp(section->Rows[i]->Id, "Version") == 0)
        {
        }
    }

    return TRUE;
}

static BOOL
processDiscSection(TextSection* section, CCD* ccd)
{
    CALL_STACK_MESSAGE1("processDiscSection(, )");

    int tocEntries, sessions;

    int i;
    for (i = 0; i < section->Rows.Count; i++)
    {
        if (strcmp(section->Rows[i]->Id, "TocEntries") == 0)
        {
            sscanf(section->Rows[i]->Value, "%d", &tocEntries);
        }
        else if (strcmp(section->Rows[i]->Id, "Sessions") == 0)
        {
            sscanf(section->Rows[i]->Value, "%d", &sessions);
        }
    }

    return TRUE;
}

static BOOL
processTrackSection(TextSection* section, CCD* ccd)
{
    CALL_STACK_MESSAGE1("processTrackSection(, )");

    int mode, index;
    int items = 0;

    int i;
    for (i = 0; i < section->Rows.Count; i++)
    {
        if (strcmp(section->Rows[i]->Id, "MODE") == 0)
        {
            sscanf(section->Rows[i]->Value, "%d", &mode);
            items++;
        }
        else if (strncmp(section->Rows[i]->Id, "INDEX 1", 7) == 0)
        {
            sscanf(section->Rows[i]->Value, "%d", &index);
            items++;
        }
    }

    if (items == 2)
    {
        ccd->TrackArray.Add(new CCDTrack(mode, index));
        return TRUE;
    }
    else
        return FALSE;
}

static BOOL
processEntrySection(TextSection* section, CCD* ccd)
{
    CALL_STACK_MESSAGE1("processEntrySection(, )");

    int session, point, plba, adr;
    int items = 0;

    int i;
    for (i = 0; i < section->Rows.Count; i++)
    {
        if (strcmp(section->Rows[i]->Id, "Session") == 0)
        {
            sscanf(section->Rows[i]->Value, "%d", &session);
            items++;
        }
        else if (strcmp(section->Rows[i]->Id, "Point") == 0)
        {
            sscanf(section->Rows[i]->Value, "%x", &point);
            items++;
        }
        else if (strcmp(section->Rows[i]->Id, "PLBA") == 0)
        {
            sscanf(section->Rows[i]->Value, "%d", &plba);
            items++;
        }
        else if (strcmp(section->Rows[i]->Id, "ADR") == 0)
        {
            sscanf(section->Rows[i]->Value, "%x", &adr);
            items++;
        }
    }

    if (items == 4)
    {
        ccd->EntryArray.Add(new CCDEntry(session, point, plba, adr));
        return TRUE;
    }
    else
        return FALSE;
}

static BOOL
processSessionSection(TextSection* section, CCD* ccd)
{
    CALL_STACK_MESSAGE1("processSessionSection(, )");

    int preGapMode, preGapSubC;
    int items = 0;

    int i;
    for (i = 0; i < section->Rows.Count; i++)
    {
        if (strcmp(section->Rows[i]->Id, "PreGapMode") == 0)
        {
            sscanf(section->Rows[i]->Value, "%d", &preGapMode);
            items++;
        }
        else if (strcmp(section->Rows[i]->Id, "PreGapSubC") == 0)
        {
            sscanf(section->Rows[i]->Value, "%d", &preGapSubC);
            items++;
        }
    }

    if (items == 2)
    {
        ccd->SessionArray.Add(new CCDSession(preGapMode, preGapSubC));
        return TRUE;
    }
    else
        return FALSE;
}

static BOOL
processTextSection(TextSection* section, CCD* ccd)
{
    CALL_STACK_MESSAGE1("processTextSection(, )");

    if (section == NULL)
        return FALSE;

    switch (section->Type)
    {
    case ccdHeader:
        return processHeaderSection(section);
    case ccdDisc:
        return processDiscSection(section, ccd);
    case ccdSession:
        return processSessionSection(section, ccd);
    case ccdEntry:
        return processEntrySection(section, ccd);
    case ccdTrack:
        return processTrackSection(section, ccd);
    } // switch

    return TRUE;
}

BOOL CISOImage::CheckForBootSectorOrMBR()
{
    BYTE buf[512];

    if (512 == ReadDataByPos(0, 512, buf))
    {
        if (((buf[0] == 0xEB) || (buf[0] == 0xE9) || (buf[0] == 0xFA)) && (buf[0x1FE] == 0x55) && (buf[0x1FF] == 0xAA))
        {
            // Looks like a Boot record or Master Boot Record
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL
processCCDFile(CISOImage* pISOImage, CFile& File, const char* fileName, CCD* ccd, BOOL quiet /* = FALSE*/)
{
    CALL_STACK_MESSAGE3("processCCDFile(%s, , %d)", fileName, quiet);

    if (fileName == NULL)
        return FALSE;

    FILE* f = NULL;
    TextSection* textSection = NULL;
    TIndirectArray<TextSection> sections(100, 50, dtDelete);

    BOOL ret = TRUE;
    if ((f = fopen(fileName, "r")) == NULL)
    {
        if (ccd->DisplayMissingCCDWarning)
        {
            if (!pISOImage->CheckForBootSectorOrMBR())
            {
                Warning(IDS_CCD_MISSING, quiet);
            }
            ccd->DisplayMissingCCDWarning = FALSE; // do not display the warning any more
        }
        return FALSE;
    }

    char row[CCD_ROW_LEN];
    while (fgets(row, CCD_ROW_LEN, f) != NULL)
    {
        // remove trailing \n
        char* n = strchr(row, '\n');
        if (n != NULL)
            *n = '\0';

        // skip empty rows
        if (strlen(row) == 0)
            continue;

        if (strncmp(row, "[", 1) == 0)
        {
            ECCDType type = ccdNone;
            if (strncmp(row, "[CloneCD]", 9) == 0)
                type = ccdHeader;
            else if (strncmp(row, "[Disc]", 9) == 0)
                type = ccdDisc;
            else if (strncmp(row, "[Session ", 9) == 0)
                type = ccdSession;
            else if (strncmp(row, "[Entry ", 7) == 0)
                type = ccdEntry;
            else if (strncmp(row, "[TRACK ", 7) == 0)
                type = ccdTrack;

            if (type != ccdNone)
            {
                textSection = new TextSection(type);
                if (textSection == FALSE)
                {
                    fclose(f);
                    return Error(IDS_INSUFFICIENT_MEMORY, quiet);
                }
                sections.Add(textSection);
            }
            else
                textSection = NULL;
        }
        else
        {
            if (textSection != NULL)
                textSection->Add(row);
        }
    } // while

    fclose(f);

    if (sections.Count > 0)
    {
        BOOL validCCD = TRUE;
        int i;
        for (i = 0; i < sections.Count; i++)
        {
            textSection = sections[i];
            if (!processTextSection(textSection, ccd))
            {
                validCCD = FALSE;
                break;
            }
        }

        if (validCCD)
            ret = TRUE;
        else
            ret = Error(IDS_CCD_BAD_FORMAT, quiet);
    }
    else
        ret = Error(IDS_CCD_BAD_FORMAT, quiet);

    return ret;
}

BOOL CISOImage::ReadSessionCCD(char* fileName, BOOL quiet /* = FALSE*/)
{
    CALL_STACK_MESSAGE3("CISOImage::ReadSessionCCD(%s, %d)", fileName, quiet);

    BOOL ret = TRUE;

    // process read info
    CCD* ccd = new CCD;
    ccd->DisplayMissingCCDWarning = DisplayMissingCCDWarning;
    if (ccd == NULL)
        return Error(IDS_INSUFFICIENT_MEMORY, quiet);

    if (processCCDFile(this, *File, fileName, ccd, quiet))
    {
        if (ccd->SessionArray.Count > 0 && ccd->TrackArray.Count > 0 && ccd->EntryArray.Count > 0)
        {
            // process read ccd info
            int i;

            DWORD* pointMin = NULL;
            DWORD* pointMax = NULL;
            DWORD* extents = NULL;
            DWORD* extentsEnd = NULL;

            try
            {
                pointMin = new DWORD[ccd->SessionArray.Count];
                if (pointMin == NULL)
                    throw Error(IDS_INSUFFICIENT_MEMORY, quiet);
                pointMax = new DWORD[ccd->SessionArray.Count];
                if (pointMax == NULL)
                    throw Error(IDS_INSUFFICIENT_MEMORY, quiet);
                extents = new DWORD[ccd->SessionArray.Count];
                if (extents == NULL)
                    throw Error(IDS_INSUFFICIENT_MEMORY, quiet);
                extentsEnd = new DWORD[ccd->SessionArray.Count];
                if (extentsEnd == NULL)
                    throw Error(IDS_INSUFFICIENT_MEMORY, quiet);

                // extent se v ramci session pocita od nuly
                // je-li v session vice tracku, pak je extent tracku ulozen v ccdtrk->Index

                // build track info
                for (i = 0; i < ccd->TrackArray.Count; i++)
                {
                    CCDTrack* ccdtrk = ccd->TrackArray[i];

                    CISOImage::Track* track = new CISOImage::Track;
                    if (track == NULL)
                        throw Error(IDS_INSUFFICIENT_MEMORY, quiet);

                    track->SectorSize = 2048;
                    // CCD track modes
                    switch (ccdtrk->Mode)
                    {
                    case 0:
                        track->Mode = mNone;
                        track->FSType = fsAudio;
                        break;
                    case 1:
                        track->Mode = mMode1;
                        break;
                    case 2:
                        track->Mode = mMode2;
                        break;
                    }
                    track->Start = 0x00;
                    track->End = 0x00;
                    track->ExtentOffset = ccdtrk->Index; // extent tracku v ramci session
                    AddTrack(track);
                } // for

                // count tracks for each session
                int firstTrack = 0;
                for (i = 1; i < ccd->TrackArray.Count; i++)
                {
                    CCDTrack* track = ccd->TrackArray[i];
                    if (track->Index == 0)
                    {
                        AddSessionTracks(i - firstTrack);
                        firstTrack = i;
                    }
                } // for
                AddSessionTracks(ccd->TrackArray.Count - firstTrack);

                // set sector format for tracks in session
                firstTrack = 0;
                for (i = 0; i < ccd->SessionArray.Count; i++)
                {
                    ETrackFSType fileSystemType = fsUnknown;
                    WORD sectorSize = 0x930;
                    CCDSession* ccdSession = ccd->SessionArray[i];

                    int j;
                    for (j = 0; j < Session[i]; j++)
                    {
                        CISOImage::Track* track = Tracks[firstTrack + j];
                        track->SectorSize = sectorSize;
                        track->SectorHeaderSize = CISOImage::GetHeaderSizeFromMode(track->Mode, track->SectorSize);
                    }

                    firstTrack += Session[i];
                }

                // set ExtentOffset in sessions
                for (i = 0; i < ccd->SessionArray.Count; i++)
                {
                    pointMin[i] = 0xFFFFFFFF;
                    pointMax[i] = 0x00000000;
                    extents[i] = 0;
                    extentsEnd[i] = 0;
                }

                for (i = 0; i < ccd->EntryArray.Count; i++)
                {
                    CCDEntry* entry = ccd->EntryArray[i];
                    int session = entry->Session - 1;
                    if (entry->ADR == 1 && entry->Point < pointMin[session])
                    {
                        pointMin[session] = entry->Point;
                        extents[session] = entry->PLBA;
                    }

                    if (entry->ADR == 1 && entry->Point > pointMax[session])
                    {
                        pointMax[session] = entry->Point;
                        extentsEnd[session] = entry->PLBA;
                    }
                }

                LONGLONG start = 0x0;
                firstTrack = 0;
                for (i = 0; i < Session.Count; i++)
                {
                    int j;
                    for (j = 0; j < Session[i]; j++)
                    {
                        CISOImage::Track* track = Tracks[firstTrack + j];
                        track->ExtentOffset += extents[i]; // extent je v ramci session pocitan od nuly -> my chceme absolutni polohu v image

                        DWORD endSector;
                        if (j < Session[i] - 1)
                            endSector = ccd->TrackArray[firstTrack + j + 1]->Index;
                        else
                            endSector = extentsEnd[i] - extents[i];

                        CCDTrack* ccdTrack = ccd->TrackArray[firstTrack + j];
                        DWORD sectorCount = endSector - ccdTrack->Index;
                        track->End = start + (track->SectorSize * sectorCount);
                        start = track->End;
                    }

                    firstTrack += Session[i];
                }

                for (i = 0; i < Tracks.Count - 1; i++)
                {
                    Tracks[i + 1]->Start = Tracks[i]->End;
                }
            }
            catch (BOOL e)
            {
                ret = e;
            }

            delete[] extents;
            delete[] extentsEnd;
            delete[] pointMin;
            delete[] pointMax;
        }
        else
            ret = Error(IDS_CCD_BAD_FORMAT, quiet);
    }
    else
        ret = FALSE;

    DisplayMissingCCDWarning = ccd->DisplayMissingCCDWarning;
    delete ccd;

    return ret;
}
