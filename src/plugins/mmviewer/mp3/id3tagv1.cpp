// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _MPG_SUPPORT_

#include "id3tagv1.h"

void chkstrcpy(char* out, const char* in, size_t maxoutsize);

//max ID3TAGV1::genre size allowed
const char* ID3TAGV1_GetGenreStr(BYTE genre)
{
    switch (genre)
    {
    case 0:
        return "Blues";
    case 1:
        return "Classic Rock";
    case 2:
        return "Country";
    case 3:
        return "Dance";
    case 4:
        return "Disco";
    case 5:
        return "Funk";
    case 6:
        return "Grunge";
    case 7:
        return "Hip-Hop";
    case 8:
        return "Jazz";
    case 9:
        return "Metal";
    case 10:
        return "New Age";
    case 11:
        return "Oldies";
    case 12:
        return "Other";
    case 13:
        return "Pop";
    case 14:
        return "R&B";
    case 15:
        return "Rap";
    case 16:
        return "Reggae";
    case 17:
        return "Rock";
    case 18:
        return "Techno";
    case 19:
        return "Industrial";
    case 20:
        return "Alternative";
    case 21:
        return "Ska";
    case 22:
        return "Death Metal";
    case 23:
        return "Pranks";
    case 24:
        return "Soundtrack";
    case 25:
        return "Euro-Techno";
    case 26:
        return "Ambient";
    case 27:
        return "Trip-Hop";
    case 28:
        return "Vocal";
    case 29:
        return "Jazz+Funk";
    case 30:
        return "Fusion";
    case 31:
        return "Trance";
    case 32:
        return "Classical";
    case 33:
        return "Instrumental";
    case 34:
        return "Acid";
    case 35:
        return "House";
    case 36:
        return "Game";
    case 37:
        return "Sound Clip";
    case 38:
        return "Gospel";
    case 39:
        return "Noise";
    case 40:
        return "AlternRock";
    case 41:
        return "Bass";
    case 42:
        return "Soul";
    case 43:
        return "Punk";
    case 44:
        return "Space";
    case 45:
        return "Meditative";
    case 46:
        return "Instrumental Pop";
    case 47:
        return "Instrumental Rock";
    case 48:
        return "Ethnic";
    case 49:
        return "Gothic";
    case 50:
        return "Darkwave";
    case 51:
        return "Techno-Industrial";
    case 52:
        return "Electronic";
    case 53:
        return "Pop-Folk";
    case 54:
        return "Eurodance";
    case 55:
        return "Dream";
    case 56:
        return "Southern Rock";
    case 57:
        return "Comedy";
    case 58:
        return "Cult";
    case 59:
        return "Gangsta";
    case 60:
        return "Top 40";
    case 61:
        return "Christian Rap";
    case 62:
        return "Pop/Funk";
    case 63:
        return "Jungle";
    case 64:
        return "Native American";
    case 65:
        return "Cabaret";
    case 66:
        return "New Wave";
    case 67:
        return "Psychadelic";
    case 68:
        return "Rave";
    case 69:
        return "Showtunes";
    case 70:
        return "Trailer";
    case 71:
        return "Lo-Fi";
    case 72:
        return "Tribal";
    case 73:
        return "Acid Punk";
    case 74:
        return "Acid Jazz";
    case 75:
        return "Polka";
    case 76:
        return "Retro";
    case 77:
        return "Musical";
    case 78:
        return "Rock & Roll";
    case 79:
        return "Hard Rock";

    //winamp extension
    case 80:
        return "Folk";
    case 81:
        return "Folk-Rock";
    case 82:
        return "National Folk";
    case 83:
        return "Swing";
    case 84:
        return "Fast Fusion";
    case 85:
        return "Bebob";
    case 86:
        return "Latin";
    case 87:
        return "Revival";
    case 88:
        return "Celtic";
    case 89:
        return "Bluegrass";
    case 90:
        return "Avantgarde";
    case 91:
        return "Gothic Rock";
    case 92:
        return "Progressive Rock";
    case 93:
        return "Psychedelic Rock";
    case 94:
        return "Symphonic Rock";
    case 95:
        return "Slow Rock";
    case 96:
        return "Big Band";
    case 97:
        return "Chorus";
    case 98:
        return "Easy Listening";
    case 99:
        return "Acoustic";
    case 100:
        return "Humour";
    case 101:
        return "Speech";
    case 102:
        return "Chanson";
    case 103:
        return "Opera";
    case 104:
        return "Chamber Music";
    case 105:
        return "Sonata";
    case 106:
        return "Symphony";
    case 107:
        return "Booty Bass";
    case 108:
        return "Primus";
    case 109:
        return "Porn Groove";
    case 110:
        return "Satire";
    case 111:
        return "Slow Jam";
    case 112:
        return "Club";
    case 113:
        return "Tango";
    case 114:
        return "Samba";
    case 115:
        return "Folklore";
    case 116:
        return "Ballad";
    case 117:
        return "Power Ballad";
    case 118:
        return "Rhythmic Soul";
    case 119:
        return "Freestyle";
    case 120:
        return "Duet";
    case 121:
        return "Punk Rock";
    case 122:
        return "Drum Solo";
    case 123:
        return "A capella";
    case 124:
        return "Euro-House";
    case 125:
        return "Dance Hall";
    case 126:
        return "Goa";
    case 127:
        return "Drum & Bass";
    case 128:
        return "Club-House";
    case 129:
        return "Hardcore";
    case 130:
        return "Terror";
    case 131:
        return "Indie";
    case 132:
        return "BritPop";
    case 133:
        return "Negerpunk";
    case 134:
        return "Polsk Punk";
    case 135:
        return "Beat";
    case 136:
        return "Christian Gangsta Rap";
    case 137:
        return "Heavy Metal";
    case 138:
        return "Black Metal";
    case 139:
        return "Crossover";
    case 140:
        return "Contemporary Christian";
    case 141:
        return "Christian Rock";
    case 142:
        return "Merengue";
    case 143:
        return "Salsa";
    case 144:
        return "Thrash Metal";
    case 145:
        return "Anime";
    case 146:
        return "JPop";
    case 147:
        return "Synthpop";
    }

    return "";
}

static void WipeOut(int init_pos, char* str)
{
    int p = init_pos;

    while (((str[p] == 32) || (str[p] == '\0')))
    {
        str[p] = '\0';
        p--;
    }
}

BOOL ID3TAGV1_Read(FILE* f, ID3TAGV1* ph)
{
    if (f && ph)
    {
        memset(ph, 0, sizeof(ID3TAGV1));

        int fpos = ftell(f);

        if ((fread(ph, sizeof(ID3TAGV1), 1, f) == 1) &&
            (ph->tag[0] == 'T') && (ph->tag[1] == 'A') && (ph->tag[2] == 'G'))
        {
            return TRUE;
        }
        else
        {
            //vrat se zpet na puv. pozici
            fseek(f, fpos, SEEK_SET);
            return FALSE;
        }
    }

    return FALSE;
}

BOOL ID3TAGV1_Decode(ID3TAGV1* ph, ID3TAGV1_DECODED* phd)
{
    if (ph && phd)
    {
        memset(phd, 0, sizeof(ID3TAGV1_DECODED));

        if (ph->comments[29] != '\0' && ph->comments[28] == '\0') //v1.1 - track number
        {
            phd->track = ph->comments[29];
            ph->comments[29] = '\0';
            phd->version = 0x0101;
        }
        else
            phd->version = 0x0100;

        //smaz prebytecne mezery (maji nektere stare tagy)
        WipeOut(sizeof(ph->artist) - 1, ph->artist);
        WipeOut(sizeof(ph->album) - 1, ph->album);
        WipeOut(sizeof(ph->title) - 1, ph->title);
        WipeOut(sizeof(ph->year) - 1, ph->year);
        WipeOut(sizeof(ph->comments) - 1, ph->comments);

        //zkopiruj polozky do dekodovane struktury
        strncpy_s(phd->artist, ph->artist, sizeof(ph->artist));
        strncpy_s(phd->album, ph->album, sizeof(ph->album));
        strncpy_s(phd->title, ph->title, sizeof(ph->title));
        strncpy_s(phd->year, ph->year, sizeof(ph->year));
        strncpy_s(phd->comments, ph->comments, sizeof(ph->comments));

        chkstrcpy(phd->genre, ID3TAGV1_GetGenreStr(ph->genre), sizeof(phd->genre));

        return TRUE;
    }

    return FALSE;
}

#endif