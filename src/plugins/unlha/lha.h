// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//**************************************************************************
//
//  Struktura LHA archivu:
//
//   +--------+------------+--------+------------+ . . . +--------+------------+
//   |hlavicka| zapakovany |hlavicka| zapakovany |       |hlavicka| zapakovany |
//   |souboru1|  soubor1   |souboru2|  soubor2   |       |souboruN|  souborN   |
//   +--------+------------+--------+------------+ . . . +--------+------------+
//
//  LHA archiv nema globalni hlavicku ani seznam souboru (adresaru). Nejsou
//  podporovany solid archivy ani kryptovani. Kazdy soubor je nezavisly.
//  Listovani spociva v precteni hlavicky, preskoceni o hdr.packed_size
//  bajtu dopredu, precteni hlavicky.. atd. Pri rozbalovani celeho archivu
//  staci stridave volat LHAGetHeader a LHAUnpackFile (popr. misto LHAUnpackFile
//  preskocit pomoci fseek)
//

//**************************************************************************
//
//  LHA_HEADER - informace o souboru v archivu
//

struct LHA_TIMESTAMP
{
    unsigned second2 : 5;
    unsigned minute : 6;
    unsigned hour : 5;
    unsigned day : 5;
    unsigned month : 4;
    unsigned year : 7;
};

struct LHA_HEADER
{
    unsigned char header_size;
    int method;         // metoda, pokud je LHA_UNKNOWNMETHOD, soubor se musi preskocit
    long packed_size;   // velikost komprimovanych dat nasledujicich primo za hlavickou
    long original_size; // velikost puvodniho (nekomprimovaneho) souboru
    LHA_TIMESTAMP last_modified_stamp;
    FILETIME last_modified_filetime; // last_modified_stamp prevedeny na FILETIME
    unsigned char attribute;
    unsigned char header_level;
    char name[MAX_PATH];
    unsigned short crc;
    BOOL has_crc;
    unsigned char extend_type;
    unsigned char minor_version;
    unsigned short unix_mode; // muze byt UNIX_FILE_REGULAR, UNIX_FILE_DIRECTORY nebo UNIX_FILE_SYMLINK
};

#define LHA_UNKNOWNMETHOD -1
#define LZHUFF0_METHOD_NUM 0
#define LZHUFF1_METHOD_NUM 1
#define LZHUFF2_METHOD_NUM 2
#define LZHUFF3_METHOD_NUM 3
#define LZHUFF4_METHOD_NUM 4
#define LZHUFF5_METHOD_NUM 5
#define LZHUFF6_METHOD_NUM 6
#define LZHUFF7_METHOD_NUM 7
#define LARC_METHOD_NUM 8
#define LARC5_METHOD_NUM 9
#define LARC4_METHOD_NUM 10
#define LZHDIRS_METHOD_NUM 11

#define UNIX_FILE_REGULAR 0100000
#define UNIX_FILE_DIRECTORY 0040000
#define UNIX_FILE_SYMLINK 0120000
#define UNIX_FILE_TYPEMASK 0170000

//**************************************************************************
//
//  LHAInit - nutno zavolat na zacatku
//

void LHAInit();

//**************************************************************************
//
//  LHAOpenArchive - otevre soubor a eventualne preskoci SFX kod.
//                   Vraci TRUE je-li vse v poradku, jinak FALSE.
//

BOOL LHAOpenArchive(FILE*& f, LPCTSTR lpName);

//**************************************************************************
//
//  LHAGetHeader - z aktualniho mista v souboru nacte hlavicku a ulozi ji do
//                 lpHeader. Vraci GH_ERROR, GH_SUCCESS nebo GH_EOF
//

int LHAGetHeader(FILE* hFile, LHA_HEADER* lpHeader);

#define GH_ERROR 0   // chyba
#define GH_SUCCESS 1 // uspech
#define GH_EOF 2     // konec souboru

//**************************************************************************
//
//  LHAUnpackFile - vypakuje soubor zacinajici na aktualni pozici v otevrenem
//                  archivu 'infile' do otevreneho souboru 'outfile'. Je treba
//                  predat hlavicku ziskanou pomoci LHAGetHeader.
//                  Vraci TRUE pri uspechu, jinak FALSE. Promenna CRC je naplnena
//                  kontrolnim kodem vypocitanym pri rozbalovani, je mozne
//                  jej porovnat s udajem v hlavicce. Behem rozbalovani se
//                  vola funkce ulozena v pointru pfLHAProgress (pokud neni NULL)
//

BOOL LHAUnpackFile(FILE* infile, HANDLE outfile, LHA_HEADER* lpHeader, int* CRC,
                   char* fileName /* jmeno vystup. souboru kvuli SafeWriteFile */);

/*
    Poznamka: Proc 'infile' pouziva IO funkce z CRT a 'outfile' funkce API?
              Funkce z API nejsou bufferovane (a vlastni buffery se mi psat 
              nechtelo, protoze u 'infile' by to bylo celkem slozity), 
              proto jsem se rozhodl nechat v kodu LHA volani funkci z CRT. 
              Pak jsem ale zjistil, ze pro vytvareni souboru se pouziva fce
              Salamandera SafeCreateFile, ktera vraci HANDLE. Aby se soubory
              nevytvarely pres SafeCreateFile a pak se znovu neotviraly
              pres fopen(), prepsal jsem vystupni cast LHA do API a take
              napsal vlastni buffer (ktery je vsak jednoduchy - zadny seek,
              cteni hlavicek...). Takze vzniknul takovy hybrid :(
*/

//**************************************************************************
//
//  exportovane globalni promenne
//

extern int iLHAErrorStrId;              // pri chybe obsahuje ID stringu popisu chyby
extern BOOL (*pfLHAProgress)(int size); // pointer na fci ktere se behem rozbalovani
                                        // predava hodnota v rozsahu 0 .. hdr.original_size
