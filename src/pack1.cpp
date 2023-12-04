// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dialogs.h"
#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "zip.h"
#include "pack.h"

//
// ****************************************************************************
// Konstanty a globalni promenne
// ****************************************************************************
//

// Ukazatel na funkci zpracovavajici chybu
BOOL(*PackErrorHandlerPtr)
(HWND parent, const WORD errNum, ...) = EmptyErrorHandler;

const char* SPAWN_EXE_NAME = "salspawn.exe";
const char* SPAWN_EXE_PARAMS = "-c10000";

// Cesta k programu salspawn
char SpawnExe[MAX_PATH * 2] = {0};
BOOL SpawnExeInitialised = FALSE;

// aby to hlasilo chybu datumu jen jednou
BOOL FirstError;

// Tabulka definic archivu a zachazeni s nimi - nemodifikujici operace
// !!! POZOR: pri zmenach poradi externich archivatoru je treba zmenit i poradi v poli
// externalArchivers v metode CPlugins::FindViewEdit
const SPackBrowseTable PackBrowseTable[] =
    {
        // JAR 1.02 Win32
        {
            (TPackErrorTable*)&JARErrors, TRUE,
            "$(ArchivePath)", "$(Jar32bitExecutable) v -ju- \"$(ArchiveFileName)\"",
            NULL, "Analyzing", 4, 0, 3, "Total files listed:", ' ', 2, 3, 9, 8, 4, 1, 2,
            "$(TargetPath)", "$(Jar32bitExecutable) x -r- -jyc \"$(ArchiveFullName)\" !\"$(ListFullName)\"",
            "$(TargetPath)", "$(Jar32bitExecutable) e -r- \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", FALSE},
        // RAR 4.20 & 5.0 Win x86/x64
        {
            (TPackErrorTable*)&RARErrors, TRUE,
            "$(ArchivePath)", "$(Rar32bitExecutable) v -c- \"$(ArchiveFileName)\"",
            NULL, "--------", 0, 0, 2, "--------", ' ', 1, 2, 6, 5, 7, 3, 2,                              // po RAR 5.0 patchujeme indexy runtime, viz promenna 'RAR5AndLater'
            "$(TargetPath)", "$(Rar32bitExecutable) x -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"", // od verze 5.0 musime vnutit -scol switch, verzi 4.20 nevadi; vyskytuje se na dalsich mistech a v registry
            "$(TargetPath)", "$(Rar32bitExecutable) e \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", FALSE},
        // ARJ 2.60 MS-DOS
        {
            (TPackErrorTable*)&ARJErrors, FALSE,
            ".", "$(Arj16bitExecutable) v -ja1 $(ArchiveDOSFullName)",
            NULL, "--------", 0, 0, 2, "--------", ' ', 2, 5, 9, -8, 11, 1, 2,
            ".", "$(Arj16bitExecutable) x -p -va -hl -jyc $(ArchiveDOSFullName) $(TargetDOSPath)\\ !$(ListDOSFullName)",
            "$(TargetPath)", "$(Arj16bitExecutable) e -p -va -hl $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // LHA 2.55 MS-DOS
        {
            (TPackErrorTable*)&LHAErrors, FALSE,
            ".", "$(Lha16bitExecutable) v $(ArchiveDOSFullName)",
            NULL, "--------------", 0, 0, 2, "--------------", ' ', 1, 2, 6, 5, 7, 1, 2,
            ".", "$(Lha16bitExecutable) x -p -a -l1 -x1 -c $(ArchiveDOSFullName) $(TargetDOSPath)\\ @$(ListDOSFullName)",
            "$(TargetPath)", "$(Lha16bitExecutable) e -p -a -l1 -c $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // UC2 2r3 PRO MS-DOS
        {
            (TPackErrorTable*)&UC2Errors, FALSE,
            ".", "$(UC216bitExecutable) ~D $(ArchiveDOSFullName)",
            PackUC2List, "", 0, 0, 0, "", ' ', 0, 0, 0, 0, 0, 0, 0,
            ".", "$(UC216bitExecutable) EF $(ArchiveDOSFullName) ##$(TargetDOSPath) @$(ListDOSFullName)",
            "$(TargetPath)", "$(UC216bitExecutable) E $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // JAR 1.02 MS-DOS
        {
            (TPackErrorTable*)&JARErrors, FALSE,
            ".", "$(Jar16bitExecutable) v -ju- $(ArchiveDOSFullName)",
            NULL, "Analyzing", 4, 0, 3, "Total files listed:", ' ', 2, 3, 9, 8, 4, 1, 2,
            ".", "$(Jar16bitExecutable) x -r- -jyc $(ArchiveDOSFullName) -o$(TargetDOSPath) !$(ListDOSFullName)",
            "$(TargetPath)", "$(Jar16bitExecutable) e -r- $(ArchiveDOSFullName) \"$(ExtractFullName)\"", FALSE},
        // RAR 2.05 MS-DOS
        {
            (TPackErrorTable*)&RARErrors, FALSE,
            ".", "$(Rar16bitExecutable) v -c- $(ArchiveDOSFullName)",
            NULL, "--------", 0, 0, 2, "--------", ' ', 1, 2, 6, 5, 7, 3, 1,
            ".", "$(Rar16bitExecutable) x $(ArchiveDOSFullName) $(TargetDOSPath)\\ @$(ListDOSFullName)",
            "$(TargetPath)", "$(Rar16bitExecutable) e $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // PKZIP 2.50 Win32
        {
            NULL, TRUE,
            "$(ArchivePath)", "$(Zip32bitExecutable) -com=none -nozipextension \"$(ArchiveFileName)\"",
            NULL, "  ------  ------    -----", 0, 0, 1, "  ------           ------", ' ', 9, 1, 6, 5, 8, 3, 1,
            "$(TargetPath)", "$(Zip32bitExecutable) -ext -nozipextension -directories -path \"$(ArchiveFullName)\" @\"$(ListFullName)\"",
            "$(TargetPath)", "$(Zip32bitExecutable) -ext -nozipextension \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", TRUE},
        // PKUNZIP 2.04g MS-DOS
        {
            (TPackErrorTable*)&UNZIP204Errors, FALSE,
            ".", "$(Unzip16bitExecutable) -v $(ArchiveDOSFullName)",
            NULL, " ------  ------   -----", 0, 0, 1, " ------          ------", ' ', 9, 1, 6, 5, 8, 3, 1,
            ".", "$(Unzip16bitExecutable) -d $(ArchiveDOSFullName) $(TargetDOSPath)\\ @$(ListDOSFullName)",
            "$(TargetPath)", "$(Unzip16bitExecutable) $(ArchiveDOSFullName) $(ExtractFullName)", FALSE},
        // ARJ 3.00c Win32
        {
            (TPackErrorTable*)&ARJErrors, TRUE,
            "$(ArchivePath)", "$(Arj32bitExecutable) v -ja1 \"$(ArchiveFileName)\"",
            NULL, "--------", 0, 0, 0, "--------", ' ', 2, 5, 9, 8, 11, 1, 2,
            "$(TargetPath)", "$(Arj32bitExecutable) x -p -va -hl -jyc \"$(ArchiveFullName)\" !\"$(ListFullName)\"",
            "$(TargetPath)", "$(Arj32bitExecutable) e -p -va -hl \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", FALSE},
        // ACE 1.2b Win32
        {
            (TPackErrorTable*)&ACEErrors, TRUE,
            "$(ArchivePath)", "$(Ace32bitExecutable) v \"$(ArchiveFileName)\"",
            NULL, "Date    ", 0, 1, 1, "        ", 0xB3, 6, 4, 2, 1, 0, 3, 2,
            "$(TargetPath)", "$(Ace32bitExecutable) x -f \"$(ArchiveFullName)\" @\"$(ListFullName)\"",
            "$(TargetPath)", "$(Ace32bitExecutable) e -f \"$(ArchiveFullName)\" \"$(ExtractFullName)\"", TRUE},
        // ACE 1.2b MS-DOS
        {
            (TPackErrorTable*)&ACEErrors, FALSE,
            ".", "$(Ace16bitExecutable) v $(ArchiveDOSFullName)",
            NULL, "Date    ", 0, 1, 1, "        ", 0xB3, 6, 4, 2, 1, 0, 3, 2,
            ".", "$(Ace16bitExecutable) x -f $(ArchiveDOSFullName) $(TargetDOSPath)\\ @$(ListDOSFullName)",
            "$(TargetPath)", "$(Ace16bitExecutable) e -f $(ArchiveDOSFullName) $(ExtractFullName)", FALSE}};

//
// ****************************************************************************
// Funkce
// ****************************************************************************
//

//
// ****************************************************************************
// Funkce pro listing archivu
//

//
// ****************************************************************************
// char *PackGetField(char *buffer, const int index, const int nameidx)
//
//   V retezci buffer najde polozku index-tou v poradi. Polozky mohou byt
//   oddeleny libovolnym poctem mezer, tabelatoru nebo prechodu na novy radek.
//   (pridan znak svisle cary (ASCII 0xB3) kvuli ACE)
//   Pokud prechazime pres polozku s indexem nameidx (vetsinou nazev souboru),
//   bere se jako oddelovac polozek pouze prechod na novy radek (nazev muze
//   obsahovat mezery nebo tabelatory). Je volana z PackScanLine().
//
//   RET: vraci ukazatel na zadanou polozku v retezci buffer nebo NULL, pokud
//        nemuze byt nalezena
//   IN:  buffer je radka textu urcena k analyze
//        index je poradove cislo polozky, ktera ma byt nalezena
//        nameidx je index polozky "jmeno souboru"

char* PackGetField(char* buffer, const int index, const int nameidx, const char separator)
{
    CALL_STACK_MESSAGE5("PackGetField(%s, %d, %d, %u)", buffer, index, nameidx, separator);
    // hledana polozka pro dany archivacni program neexistuje
    if (index == 0)
        return NULL;

    // indikuje prave aktualni polozku, na ktere jsme
    int i = 1;

    // preskocime uvodni mezery a tabelatory (jsou-li)
    while (*buffer != '\0' && (*buffer == ' ' || *buffer == '\t' ||
                               *buffer == 0x10 || *buffer == 0x11 || // sipky pro ACE
                               *buffer == separator))
        buffer++;

    // najdeme zadanou polozku
    while (index != i)
    {
        // prejedeme polozku
        if (i == nameidx)
            // pokud jsme na jmene, oddelovacem je pouze prechod na novy radek
            while (*buffer != '\0' && *buffer != '\n')
                buffer++;
        else
            // jinak je to mezera, tabelator, pomlcka, nebo prechod na novy radek
            while (*buffer != '\0' && *buffer != ' ' && *buffer != '\n' &&
                   *buffer != '\t' && *buffer != 0x10 && *buffer != 0x11 &&
                   *buffer != separator)
                buffer++;

        // prejedeme mezery za ni
        while (*buffer != '\0' && (*buffer == ' ' || *buffer == '\t' ||
                                   *buffer == '\n' || *buffer == 0x10 ||
                                   *buffer == 0x11 || *buffer == separator))
            buffer++;

        // jsme na dalsi polozce
        i++;
    }
    return buffer;
}

//
// ****************************************************************************
// BOOL PackScanLine(char *buffer, CSalamanderDirectory &dir, const int index)
//
//   Analyzuje jednu polozku seznamu souboru z vypisu archivacniho
//   programu. Vetsinou jde o jednu radku, muze vsak jit i o vice
//   spojenych - musi obsahovat vsechny informace k jedinenu
//   souboru ulozenemu v archivu
//   Je volana z funkce PackList()
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  buffer je radka textu urcena k analyze - pri analyze je zmenen !
//        index je index v tabulce PackTable k prislusnemu radku
//   OUT: CSalamanderDirectory je vytoreno a naplneno udaji o archivu

BOOL PackScanLine(char* buffer, CSalamanderDirectory& dir, const int index,
                  const SPackBrowseTable* configTable, BOOL ARJHack)
{
    CALL_STACK_MESSAGE3("PackScanLine(%s, , %d,)", buffer, index);
    // pridavany soubor nebo adresar
    CFileData newfile;
    int idx;

    // buffer pro nazev souboru
    char filename[MAX_PATH];
    char* tmpfname = filename;

    // najdeme nazev v radku
    char* tmpbuf = PackGetField(buffer, configTable->NameIdx,
                                configTable->NameIdx,
                                configTable->Separator);
    // je blbost, aby neexistoval nazev, ale pokud do konfigurace nechame
    // stourat uzivatele, melo by ho to servat
    if (tmpbuf == NULL)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCCFG);

    // zlikvidujeme pripadny backslash nebo lomitko na zacatku
    if (*tmpbuf == '\\' || *tmpbuf == '/')
        tmpbuf++;
    // prekopirujeme jen nazev, lomitka nahrazujeme backslashema
    while (*tmpbuf != '\0' && *tmpbuf != '\n')
    {
        if (*tmpbuf == '/')
        {
            tmpbuf++;
            *tmpfname++ = '\\';
        }
        else
            *tmpfname++ = *tmpbuf++;
    }

    // zrusime pripadne separatory na konci nazvu
    while (*(tmpfname - 1) == ' ' || *(tmpfname - 1) == '\t' ||
           *(tmpfname - 1) == configTable->Separator)
        tmpfname--;

    // pokud zpracovavany objekt neni adresar podle lomitka na konci nazvu,
    // najdeme atributy v radku, a pokud je nektery z nich D nebo d,
    // jde take o adresar, takze pridame backslash na konec
    if (*(tmpfname - 1) != '\\')
    {
        idx = configTable->AttrIdx;
        if (ARJHack)
            idx--;
        tmpbuf = PackGetField(buffer, idx,
                              configTable->NameIdx,
                              configTable->Separator);
        if (tmpbuf != NULL && *tmpbuf != '\0')
        {
            while (*tmpbuf != '\0' && *tmpbuf != '\n' && *tmpbuf != '\t' &&
                   *tmpbuf != ' ' && *tmpbuf != 'D' && *tmpbuf != 'd')
                tmpbuf++;
            if (*tmpbuf == 'D' || *tmpbuf == 'd')
                *tmpfname++ = '\\';
        }
    }
    // zakoncime, a pripravime ukazatel na posledni znak (ne-backslashovy)
    *tmpfname-- = '\0';
    if (*tmpfname == '\\')
        tmpfname--;

    char* pomptr = tmpfname; // ukazuje na konec nazvu
    // oddelime ho od cesty
    while (pomptr > filename && *pomptr != '\\')
        pomptr--;

    char* pomptr2;
    if (*pomptr == '\\')
    {
        // je tu i nazev i cesta
        *pomptr++ = '\0';
        pomptr2 = filename;
    }
    else
    {
        // je tu jen nazev
        pomptr2 = NULL;
    }

    // ted mame v pomptr nazev pridavaneho adresare nebo souboru
    // a v pomptr2 pripadnou cestu k nemu
    newfile.NameLen = tmpfname - pomptr + 1;

    // nastavime jmeno noveho souboru nebo adresare
    newfile.Name = (char*)malloc(newfile.NameLen + 1);
    if (!newfile.Name)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_NOMEM);
    OemToCharBuff(pomptr, newfile.Name, newfile.NameLen); // kopie s prevodem z OEM na ANSI
    newfile.Name[newfile.NameLen] = '\0';

    // prevedeme jeste cestu z OEM na ANSI
    if (pomptr2 != NULL)
        OemToChar(pomptr2, pomptr2);

    // nastavime priponu
    char* s = tmpfname - 1;
    while (s >= pomptr && *s != '.')
        s--;
    if (s >= pomptr)
        //  if (s > pomptr)  // ".cvspass" ve Windows je pripona ...
        newfile.Ext = newfile.Name + (s - pomptr) + 1;
    else
        newfile.Ext = newfile.Name + newfile.NameLen;

    // nyni nacteme datum a cas
    SYSTEMTIME t;
    idx = abs(configTable->DateIdx);
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // nenasli jsme polozku ve vypisu, dame default
    if (tmpbuf == NULL)
    {
        t.wYear = 1980;
        t.wMonth = 1;
        t.wDay = 1;
    }
    else
    {
        // jinak nacteme vsechny tri casti data
        int i;
        for (i = 1; i < 4; i++)
        {
            WORD tmpnum = 0;
            // nacteme cislo
            while (*tmpbuf >= '0' && *tmpbuf <= '9')
            {
                tmpnum = tmpnum * 10 + (*tmpbuf - '0');
                tmpbuf++;
            }
            // a priradime do spravne promenne
            if (configTable->DateYIdx == i)
                t.wYear = tmpnum;
            else if (configTable->DateMIdx == i)
                t.wMonth = tmpnum;
            else
                t.wDay = tmpnum;
            tmpbuf++;
        }
    }

    t.wDayOfWeek = 0; // ignored
    if (t.wYear < 100)
    {
        if (t.wYear >= 80)
            t.wYear += 1900;
        else
            t.wYear += 2000;
    }

    // ted cas
    idx = configTable->TimeIdx;
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // vynulujem pro pripad, ze bychom polozku neprecetli (default cas)
    t.wHour = 0;
    t.wMinute = 0;
    t.wSecond = 0;
    t.wMilliseconds = 0;
    if (tmpbuf != NULL)
    {
        // polozka existuje, tak nacteme hodiny
        while (*tmpbuf >= '0' && *tmpbuf <= '9')
        {
            t.wHour = t.wHour * 10 + (*tmpbuf - '0');
            tmpbuf++;
        }
        // preskocime jeden znak oddelovace
        tmpbuf++;
        // dalsi cislice musi byt minuty
        while (*tmpbuf >= '0' && *tmpbuf <= '9')
        {
            t.wMinute = t.wMinute * 10 + (*tmpbuf - '0');
            tmpbuf++;
        }
        // nenasleduje am/pm ?
        if (*tmpbuf == 'a' || *tmpbuf == 'p' || *tmpbuf == 'A' || *tmpbuf == 'P')
        {
            if (*tmpbuf == 'p' || *tmpbuf == 'P')
            {
                t.wHour += 12;
            }
            tmpbuf++;
            if (*tmpbuf == 'm' || *tmpbuf == 'M')
                tmpbuf++;
        }
        // a jestli nenasleduje oddelovac polozek, muzeme nacist jeste sekundy
        if (*tmpbuf != '\0' && *tmpbuf != '\n' && *tmpbuf != '\t' &&
            *tmpbuf != ' ')
        {
            // preskocime oddelovac
            tmpbuf++;
            // a nacteme sekundy
            while (*tmpbuf >= '0' && *tmpbuf <= '9')
            {
                t.wSecond = t.wSecond * 10 + (*tmpbuf - '0');
                tmpbuf++;
            }
        }
        // nenasleduje am/pm ?
        if (*tmpbuf == 'a' || *tmpbuf == 'p' || *tmpbuf == 'A' || *tmpbuf == 'P')
        {
            if (*tmpbuf == 'p' || *tmpbuf == 'P')
            {
                t.wHour += 12;
            }
            tmpbuf++;
            if (*tmpbuf == 'm' || *tmpbuf == 'M')
                tmpbuf++;
        }
    }

    // a ulozime do struktury
    FILETIME lt;
    if (!SystemTimeToFileTime(&t, &lt))
    {
        DWORD ret = GetLastError();
        if (ret != ERROR_INVALID_PARAMETER && ret != ERROR_SUCCESS)
        {
            char buff[1000];
            strcpy(buff, "SystemTimeToFileTime: ");
            strcat(buff, GetErrorText(ret));
            free(newfile.Name);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buff);
        }
        if (FirstError)
        {
            FirstError = FALSE;
            (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_DATETIME);
        }
        t.wYear = 1980;
        t.wMonth = 1;
        t.wDay = 1;
        t.wHour = 0;
        t.wMinute = 0;
        t.wSecond = 0;
        t.wMilliseconds = 0;
        if (!SystemTimeToFileTime(&t, &lt))
        {
            free(newfile.Name);
            return FALSE;
        }
    }
    if (!LocalFileTimeToFileTime(&lt, &newfile.LastWrite))
    {
        char buff[1000];
        strcpy(buff, "LocalFileTimeToFileTime: ");
        strcat(buff, GetErrorText(GetLastError()));
        free(newfile.Name);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buff);
    }

    // ted nacteme velikost souboru
    idx = configTable->SizeIdx;
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // default je nula
    unsigned __int64 tmpvalue = 0;
    // pokud polozka existuje
    if (tmpbuf != NULL)
        // nacteme ji
        while (*tmpbuf >= '0' && *tmpbuf <= '9')
        {
            tmpvalue = tmpvalue * 10 + (*tmpbuf - '0');
            tmpbuf++;
        }
    // a nastavime ve strukture
    newfile.Size.Set((DWORD)(tmpvalue & 0xFFFFFFFF), (DWORD)(tmpvalue >> 32));

    // nasleduji atributy
    idx = configTable->AttrIdx;
    if (ARJHack)
        idx--;
    tmpbuf = PackGetField(buffer, idx,
                          configTable->NameIdx,
                          configTable->Separator);
    // default je zadne nenastavene
    newfile.Attr = 0;
    newfile.Hidden = 0;
    newfile.IsOffline = 0;
    if (tmpbuf != NULL)
        while (*tmpbuf != '\0' && *tmpbuf != '\n' && *tmpbuf != '\t' &&
               *tmpbuf != ' ')
        {
            switch (*tmpbuf)
            {
            // read-only atribut
            case 'R':
            case 'r':
                newfile.Attr |= FILE_ATTRIBUTE_READONLY;
                break;
            // archive atribut
            case 'A':
            case 'a':
                newfile.Attr |= FILE_ATTRIBUTE_ARCHIVE;
                break;
            // system atribut
            case 'S':
            case 's':
                newfile.Attr |= FILE_ATTRIBUTE_SYSTEM;
                break;
            // hidden atribut
            case 'H':
            case 'h':
                newfile.Attr |= FILE_ATTRIBUTE_HIDDEN;
                newfile.Hidden = 1;
                break;
            }
            tmpbuf++;
        }

    // nastavime ostatni polozky struktury (ty co nejsou nulovane v AddFile/Dir)
    newfile.DosName = NULL;
    newfile.PluginData = -1; // -1 jen tak, ignoruje se

    // a pridame bud novy soubor, nebo adresar
    if (*(tmpfname + 1) != '\\')
    {
        newfile.IsLink = IsFileLink(newfile.Ext);

        // je to soubor, pridame soubor
        if (!dir.AddFile(pomptr2, newfile, NULL))
        {
            free(newfile.Name);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_FDATA);
        }
    }
    else
    {
        // je to adresar, pridame adresar
        newfile.Attr |= FILE_ATTRIBUTE_DIRECTORY;
        newfile.IsLink = 0;
        if (!Configuration.SortDirsByExt)
            newfile.Ext = newfile.Name + newfile.NameLen; // adresare nemaji pripony
        if (!dir.AddDir(pomptr2, newfile, NULL))
        {
            free(newfile.Name);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_FDATA);
        }
    }
    return TRUE;
}

//
// ****************************************************************************
// BOOL PackList(CFilesWindow *panel, const char *archiveFileName, CSalamanderDirectory &dir,
//               CPluginDataInterfaceAbstract *&pluginData, CPluginData *&plugin)
//
//   Funkce pro zjisteni obsahu archivu.
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  panel je souborovy panel Salamandera
//        archiveFileName je nazev souboru archivu k vylistovani
//   OUT: dir je naplneno udaji o archivu
//        pluginData je interface k datum o sloupcich definovanych plug-inem archivatoru
//        plugin je zaznam plug-inu, ktery provedl ListArchive

BOOL PackList(CFilesWindow* panel, const char* archiveFileName, CSalamanderDirectory& dir,
              CPluginDataInterfaceAbstract*& pluginData, CPluginData*& plugin)
{
    CALL_STACK_MESSAGE2("PackList(, %s, , ,)", archiveFileName);
    // pro jistotu uklidime
    dir.Clear(NULL);
    pluginData = NULL;
    plugin = NULL;

    FirstError = TRUE;

    // najdeme ten pravy podle tabulky
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // Nenasli jsme podporovany archiv - chyba
    if (format == 0)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    int index = PackerFormatConfig.GetUnpackerIndex(format);

    // Nejde o interni zpracovani (DLL) ?
    if (index < 0)
    {
        plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelView)
        {
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->ListArchive(panel, archiveFileName, dir, pluginData);
    }

    // pokud jsme jeste nezjistili cestu ke spawnu, udelame to ted
    if (!InitSpawnName(NULL))
        return FALSE;

    //
    // Budeme spoustet externi program s presmerovanim vystupu
    //
    const SPackBrowseTable* browseTable = ArchiverConfig.GetUnpackerConfigTable(index);

    // vykonstruujeme aktualni adresar
    char currentDir[MAX_PATH];
    if (!PackExpandInitDir(archiveFileName, NULL, NULL, browseTable->ListInitDir,
                           currentDir, MAX_PATH))
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_IDIRERR);

    // vykonstruujeme prikazovou radku
    char cmdLine[PACK_CMDLINE_MAXLEN];
    sprintf(cmdLine, "\"%s\" %s ", SpawnExe, SPAWN_EXE_PARAMS);
    int cmdIndex = (int)strlen(cmdLine);
    if (!PackExpandCmdLine(archiveFileName, NULL, NULL, NULL, browseTable->ListCommand,
                           cmdLine + cmdIndex, PACK_CMDLINE_MAXLEN - cmdIndex, NULL))
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNERR);

    char cmdForErrors[MAX_PATH]; // cesta+nazev spusteneho exe pro pripad chyby
    if (PackExpandCmdLine(archiveFileName, NULL, NULL, NULL, browseTable->ListCommand,
                          cmdForErrors, MAX_PATH, NULL))
    {
        char* begin;
        char* p = cmdForErrors;
        while (*p == ' ')
            p++;
        begin = p;
        if (*p == '\"')
        {
            p++;
            begin = p;
            while (*p != '\"' && *p != 0)
                p++;
        }
        else
        {
            while (*p != ' ' && *p != 0)
                p++;
        }
        *p = 0;
        if (begin > cmdForErrors)
            memmove(cmdForErrors, begin, strlen(begin) + 1);
    }
    else
        cmdForErrors[0] = 0;

    // zjistime, zda neni komandlajna moc dlouha
    if (!browseTable->SupportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // musime dedit handly
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    // vytvorime pipu na komunikaci s procesem
    HANDLE StdOutRd, StdOutWr, StdErrWr;
    if (!HANDLES(CreatePipe(&StdOutRd, &StdOutWr, &sa, 0)))
    {
        char buffer[1000];
        strcpy(buffer, "CreatePipe: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }
    // abychom ji mohli pouzit i jako stderr
    if (!HANDLES(DuplicateHandle(GetCurrentProcess(), StdOutWr, GetCurrentProcess(), &StdErrWr,
                                 0, TRUE, DUPLICATE_SAME_ACCESS)))
    {
        char buffer[1000];
        strcpy(buffer, "DuplicateHandle: ");
        strcat(buffer, GetErrorText(GetLastError()));
        HANDLES(CloseHandle(StdOutRd));
        HANDLES(CloseHandle(StdOutWr));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    // vytvorime struktury pro novy proces
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = NULL;
    si.hStdOutput = StdOutWr;
    si.hStdError = StdErrWr;
    // a spoustime ...
    if (!HANDLES(CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NEW_CONSOLE | CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                               NULL, currentDir, &si, &pi)))
    {
        // pokud se to nepovedlo, mame spatne cestu k salspawn
        DWORD err = GetLastError();
        HANDLES(CloseHandle(StdOutRd));
        HANDLES(CloseHandle(StdOutWr));
        HANDLES(CloseHandle(StdErrWr));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PROCESS, SpawnExe, GetErrorText(err));
    }

    // My uz tyhle handly nepotrebujeme, duplikaty si zavre child, nechame si jen StdOutRd
    HANDLES(CloseHandle(StdOutWr));
    HANDLES(CloseHandle(StdErrWr));

    // Vytahneme z pajpy vsechna data do pole radek
    char tmpbuff[1000];
    DWORD read;
    CPackLineArray lineArray(1000, 500);
    int buffOffset = 0;
    while (1)
    {
        // nacteme plny buffer od konce nezpracovanych dat
        if (!ReadFile(StdOutRd, tmpbuff + buffOffset, 1000 - buffOffset, &read, NULL))
            break;
        // zaciname od zacatku
        char* start = tmpbuff;
        // a po celem bufferu hledame konce radek
        unsigned int i;
        for (i = 0; i < read + buffOffset; i++)
        {
            if (tmpbuff[i] == '\n')
            {
                // delka radku
                int lineLen = (int)(tmpbuff + i - start);
                // zrusime \r, pokud tam je
                if (lineLen > 0 && tmpbuff[i - 1] == '\r')
                    lineLen--;
                // naalokujeme novy radek
                char* newLine = new char[lineLen + 2];
                // naplnime daty a ukoncime
                strncpy(newLine, start, lineLen);
                newLine[lineLen] = '\n';
                newLine[lineLen + 1] = '\0';
                // pridame ho do pole
                lineArray.Add(newLine);
                // a posunem se za nej
                start = tmpbuff + i + 1;
            }
        }
        // mame buffer zpracovany, ted zjistime, kolik nam tam toho zbylo a setreseme to na zacatek bufferu
        buffOffset = (int)(tmpbuff + read + buffOffset - start);
        if (buffOffset > 0)
            memmove(tmpbuff, start, buffOffset);
        // maximalni delka radku je 990, snad to bude stacit :-)
        if (buffOffset >= 990)
        {
            HANDLES(CloseHandle(StdOutRd));
            HANDLES(CloseHandle(pi.hProcess));
            HANDLES(CloseHandle(pi.hThread));
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
        }
    }

    // mame docteno, uz ho nepotrebujeme
    HANDLES(CloseHandle(StdOutRd));

    // Pockame na dokonceni externiho programu (uz by mel byt davno ukoncenej, ale jistota je jistota)
    if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
    {
        char buffer[1000];
        strcpy(buffer, "WaitForSingleObject: ");
        strcat(buffer, GetErrorText(GetLastError()));
        HANDLES(CloseHandle(pi.hProcess));
        HANDLES(CloseHandle(pi.hThread));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    // Nastavime fokus zase na nas
    SetForegroundWindow(MainWindow->HWindow);

    // a zjistime jak to dopadlo - snad vsechny vraci 0 jako success
    DWORD exitCode;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode))
    {
        char buffer[1000];
        strcpy(buffer, "GetExitCodeProcess: ");
        strcat(buffer, GetErrorText(GetLastError()));
        HANDLES(CloseHandle(pi.hProcess));
        HANDLES(CloseHandle(pi.hThread));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    // uvolnime handly procesu
    HANDLES(CloseHandle(pi.hProcess));
    HANDLES(CloseHandle(pi.hThread));

    if (exitCode != 0)
    {
        //
        // Nejprve chyby salspawn.exe
        //
        if (exitCode >= SPAWN_ERR_BASE)
        {
            // chyba salspawn.exe - spatne parametry nebo tak...
            if (exitCode >= SPAWN_ERR_BASE && exitCode < SPAWN_ERR_BASE * 2)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, SPAWN_EXE_NAME, LoadStr(IDS_PACKRET_SPAWN));
            // chyba CreateProcess
            if (exitCode >= SPAWN_ERR_BASE * 2 && exitCode < SPAWN_ERR_BASE * 3)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PROCESS, cmdForErrors, GetErrorText(exitCode - SPAWN_ERR_BASE * 2));
            // chyba WaitForSingleObject
            if (exitCode >= SPAWN_ERR_BASE * 3 && exitCode < SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "WaitForSingleObject: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 3));
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
            // chyba GetExitCodeProcess
            if (exitCode >= SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "GetExitCodeProcess: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 4));
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
        }
        //
        // ted budou chyby externiho programu
        //
        // pokud je errorTable == NULL, pak nedelame preklad (neexistuje tabulka)
        if (!browseTable->ErrorTable)
        {
            char buffer[1000];
            sprintf(buffer, LoadStr(IDS_PACKRET_GENERAL), exitCode);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, cmdForErrors, buffer);
        }
        // najdeme v tabulce patricny text
        int i;
        for (i = 0; (*browseTable->ErrorTable)[i][0] != -1 &&
                    (*browseTable->ErrorTable)[i][0] != (int)exitCode;
             i++)
            ;
        // nasli jsme ho ?
        if ((*browseTable->ErrorTable)[i][0] == -1)
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, cmdForErrors, LoadStr(IDS_PACKRET_UNKNOWN));
        else
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_RETURN, cmdForErrors,
                                          LoadStr((*browseTable->ErrorTable)[i][1]));
    }

    //
    // ted to hlavni - parsovani vystupu pakovace do nasich struktur
    //
    if (lineArray.Count == 0)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_NOOUTPUT);

    // pokud musime pouzit specialni parsovaci funkci, jdeme na to prave ted
    if (browseTable->SpecialList)
        return (*(browseTable->SpecialList))(archiveFileName, lineArray, dir);

    // ted parsujeme univerzalnim parserem

    // nejake ty lokalni promenne
    char* line = NULL; // buffer pro sestaveni "viceradkove radky"
    int lines = 0;     // kolik radku z viceradkove polozky jsme nacetli
    int validData = 0; // jestli cteme header/footer nebo platna data
    int toSkip = browseTable->LinesToSkip;
    int alwaysSkip = browseTable->AlwaysSkip;
    int linesPerFile = browseTable->LinesPerFile;
    BOOL ARJHack;
    BOOL RAR5AndLater = FALSE; // od RAR 5.0 je novy format listingu (prikazy 'v' a 'l'), nazev je v poslednim sloupci

    int i;
    for (i = 0; i < lineArray.Count; i++)
    {
        // zjistime, co s temi daty vlastne delat
        switch (validData)
        {
        case 0: // jsme v hlavicce
            // zjistime, jestli v ni zustavame
            if (!strncmp(lineArray[i], browseTable->StartString,
                         strlen(browseTable->StartString)))
                validData++;
            if (i == 1 && strncmp(lineArray[i], "RAR ", 4) == 0 && lineArray[i][4] >= '5' && lineArray[i][4] <= '9')
            {
                RAR5AndLater = TRUE; // test selze od RAR 10, coz se bude hodit ;-)
                linesPerFile = 1;
            }
            // kazdopadne nas tenhle radek nezajima
            continue;
        case 2: // jsme v paticce a na tu sere pes
            continue;
        case 1: // jsme v datech - jen zjistime jestli nekonci a pak do prace
            // jestli mame jeste neco preskocit, udelame to ted
            if (alwaysSkip > 0)
            {
                alwaysSkip--;
                continue;
            }
            if (!strncmp(lineArray[i], browseTable->StopString,
                         strlen(browseTable->StopString)))
            {
                validData++;
                // mozna nam zustaly nejake zbytky predchozi radky
                if (line)
                    free(line);
                continue;
            }
        }

        // jestli mame jeste neco preskocit, udelame to ted
        if (toSkip > 0)
        {
            toSkip--;
            continue;
        }

        // mame dalsi radku
        lines++;
        // pokud je to prvni, musime alokovat buffer
        if (lines == 1)
        {
            // zjistime, jestli jsme dvou ci ctyrradkovi (ARJ32 hack)
            if (browseTable->LinesPerFile == 0)
                if (i + 3 < lineArray.Count && lineArray[i + 2][3] == ' ')
                    linesPerFile = 4;
                else
                    linesPerFile = 2;
            // zjistime, jestli nechybi typ OS (ARJ16 hack)
            ARJHack = FALSE;
            if (browseTable->DateIdx < 0)
                if (lineArray[i + 1][5] == ' ')
                    ARJHack = TRUE;
            // mame vubec tolik radku ?
            if (i + linesPerFile - 1 >= lineArray.Count)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // zjistime vyslednou delku
            int len = 0;
            int j;
            for (j = 0; j < linesPerFile; j++)
                len = len + (int)strlen(lineArray[i + j]);
            // naalokujeme pro ni buffer
            line = (char*)malloc(len + 1);
            if (!line)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_NOMEM);
            // a nainicializujeme ho
            line[0] = '\0';
        }
        // pokud to neni posledni radek, jen ho pridame do bufferu
        if (lines < linesPerFile)
        {
            strcat(line, lineArray[i]);
            continue;
        }
        // je-li to posledni radek, pridame ho do bufferu
        strcat(line, lineArray[i]);

        // a mame vse k jedne polozce - zpracujem ji
        SPackBrowseTable browseTableRAR5;
        if (RAR5AndLater)
        {
            memmove(&browseTableRAR5, browseTable, sizeof(SPackBrowseTable));
            browseTableRAR5.NameIdx = 8;
            browseTableRAR5.AttrIdx = 1;
        }
        BOOL ret = PackScanLine(line, dir, index, RAR5AndLater ? &browseTableRAR5 : browseTable, ARJHack);
        // buffer uz nepotrebujeme
        free(line);
        line = NULL;
        if (!ret)
            return FALSE; // nemusime uz volat error funkci, uz byla zavolana z PackScanLine

        // nainicializujeme promenne
        lines = 0;
    }

    // pokud jsme skoncili jinde nez v paticce, mame problem
    if (validData < 2)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);

    return TRUE;
}

//
// ****************************************************************************
// BOOL PackUC2List(const char *archiveFileName, CPackLineArray &lineArray,
//                  CSalamanderDirectory &dir)
//
//   Funkce pro zjisteni obsahu archivu pro format UC2 (pouze parser)
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  archiveFileName je nazev archivu, se kterym pracujeme
//        lineArray je pole radek vystupu archivatoru
//   OUT: dir je vytoreno a naplneno udaji o archivu

BOOL PackUC2List(const char* archiveFileName, CPackLineArray& lineArray,
                 CSalamanderDirectory& dir)
{
    CALL_STACK_MESSAGE2("PackUC2List(%s, ,)", archiveFileName);
    // Nejprve smazeme pomocny fajl, ktery UC2 vytvari pri pouziti flagu ~D
    char arcPath[MAX_PATH];
    const char* arcName = strrchr(archiveFileName, '\\') + 1;
    strncpy(arcPath, archiveFileName, arcName - archiveFileName);
    arcPath[arcName - archiveFileName] = '\0';
    strcat(arcPath, "U$~RESLT.OK");
    DeleteFile(arcPath);

    char* txtPtr;         // pointer na aktualni pozici ve ctenem radku
    char currentDir[256]; // aktualni adresar, ktery prozkoumavame
    int line = 0;         // index do pole radku
    // trochu zbytecna kontrola, ale jistota je jistota
    if (lineArray.Count < 1)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);

    // hlavni parsovaci smycka
    while (1)
    {
        // pridavany soubor nebo adresar
        CFileData newfile;

        // prebehneme pocatecni mezery
        for (txtPtr = lineArray[line]; *txtPtr == ' '; txtPtr++)
            ;

        // pokud je polozka END, jsme hotovi
        if (!strncmp(txtPtr, "END", 3))
            break;

        // pokud je polozka LIST, pak zjistime, v jakem jsme adresari
        if (!strncmp(txtPtr, "LIST", 4))
        {
            // dobehneme na zacatek nazvu
            while (*txtPtr != '\0' && *txtPtr != '[')
                txtPtr++;
            // osetreni chyb - nemelo by nastat
            if (*txtPtr == '\0')
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // a na prvni pismeno nazvu
            txtPtr++;
            int i = 0;
            // preskocime uvodni backslashe
            while (*txtPtr == '\\')
                txtPtr++;
            // zkopirujeme nazev do promenne
            while (*txtPtr != '\0' && *txtPtr != ']')
                currentDir[i++] = *txtPtr++;
            // zakoncime retezec
            currentDir[i] = '\0';
            OemToChar(currentDir, currentDir);
            // a zase kontrola
            if (*txtPtr == '\0')
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // pripravime dalsi radek
            if (++line > lineArray.Count - 1)
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
            // a jedeme dalsi rundu
            continue;
        }

        // pokud je polozka FILE/DIR, pak vytvarime subor/adresar
        if (!strncmp(txtPtr, "DIR", 3) || !strncmp(txtPtr, "FILE", 4))
        {
            // o co tu jde ? o soubor, nebo o adresar
            BOOL isDir = TRUE;
            if (!strncmp(txtPtr, "FILE", 4))
                isDir = FALSE;

            // pripravime nejake ty defaultni hodnoty
            SYSTEMTIME t;
            t.wYear = 1980;
            t.wMonth = 1;
            t.wDay = 1;
            t.wDayOfWeek = 0; // ignored
            t.wHour = 0;
            t.wMinute = 0;
            t.wSecond = 0;
            t.wMilliseconds = 0;

            newfile.Size = CQuadWord(0, 0);
            newfile.DosName = NULL;
            newfile.PluginData = -1; // -1 jen tak, ignoruje se

            // hlavni smycka parsovani souboru/adresare
            // konci, jakmile narazime na klicove slovo, ktere nezname
            while (1)
            {
                // pripravime dalsi radek
                if (++line > lineArray.Count - 1)
                    return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);

                // prebehneme pocatecni mezery
                for (txtPtr = lineArray[line]; *txtPtr == ' '; txtPtr++)
                    ;

                // jde o nazev ?
                if (!strncmp(txtPtr, "NAME=", 5))
                {
                    // jdeme na zacatek nazvu
                    while (*txtPtr != '\0' && *txtPtr != '[')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    int i = 0;
                    // okopirujeme nazev do promenne newName
                    char newName[15];
                    while (*txtPtr != '\0' && *txtPtr != ']')
                        newName[i++] = *txtPtr++;
                    newName[i] = '\0';
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    // a sup s nim do struktury
                    newfile.NameLen = strlen(newName);
                    newfile.Name = (char*)malloc(newfile.NameLen + 1);
                    if (!newfile.Name)
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    OemToChar(newName, newfile.Name);
                    newfile.Ext = strrchr(newfile.Name, '.');
                    if (newfile.Ext != NULL) // ".cvspass" ve Windows je pripona ...
                                             //          if (newfile.Ext != NULL && newfile.Name != newfile.Ext)
                        newfile.Ext++;
                    else
                        newfile.Ext = newfile.Name + newfile.NameLen;
                    // a dalsi runda ...
                    continue;
                }
                // nebo se snad jedna o datum ?
                if (!strncmp(txtPtr, "DATE(MDY)=", 10))
                {
                    // vynulujeme
                    t.wYear = 0;
                    t.wMonth = 0;
                    t.wDay = 0;
                    // prejdeme na zacatek udaju
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // nacteme mesic
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wMonth = t.wMonth * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // nacteme den
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wDay = t.wDay * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // nacteme rok
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wYear = t.wYear * 10 + *txtPtr++ - '0';

                    // konverze, kdyby nahodou (nemelo by byt potreba)
                    if (t.wYear < 100)
                    {
                        if (t.wYear >= 80)
                            t.wYear += 1900;
                        else
                            t.wYear += 2000;
                    }
                    if (t.wMonth == 0)
                        t.wMonth = 1;
                    if (t.wDay == 0)
                        t.wDay = 1;

                    // a znovu dokola ...
                    continue;
                }
                // taky by to mohl byt cas posledni modifikace
                if (!strncmp(txtPtr, "TIME(HMS)=", 10))
                {
                    // opet vynulujeme
                    t.wHour = 0;
                    t.wMinute = 0;
                    t.wSecond = 0;
                    // zacatek dat
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // nacteme hodinu
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wHour = t.wHour * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // nacteme minutu
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wMinute = t.wMinute * 10 + *txtPtr++ - '0';
                    while (*txtPtr == ' ')
                        txtPtr++;
                    // nacteme vterinu
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        t.wSecond = t.wSecond * 10 + *txtPtr++ - '0';

                    // a znova ...
                    continue;
                }
                // zbyvaji jeste atributy...
                if (!strncmp(txtPtr, "ATTRIB=", 7))
                {
                    // zacatek dat
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // predem vynulujeme
                    newfile.Attr = 0;
                    newfile.Hidden = 0;
                    // a naorujem, co je potreba
                    while (*txtPtr != '\0')
                    {
                        switch (*txtPtr++)
                        {
                        // readonly atribut
                        case 'R':
                            newfile.Attr |= FILE_ATTRIBUTE_READONLY;
                            break;
                        // archive atribut
                        case 'A':
                            newfile.Attr |= FILE_ATTRIBUTE_ARCHIVE;
                            break;
                        // system atribut
                        case 'S':
                            newfile.Attr |= FILE_ATTRIBUTE_SYSTEM;
                            break;
                        // hidden atribut
                        case 'H':
                            newfile.Attr |= FILE_ATTRIBUTE_HIDDEN;
                            newfile.Hidden = 1;
                        }
                    }
                    // a znova na vec ...
                    continue;
                }
                // a nakonec jeste velikost
                if (!strncmp(txtPtr, "SIZE=", 5))
                {
                    // zase prejdeme nedulezite veci
                    while (*txtPtr != '\0' && *txtPtr != '=')
                        txtPtr++;
                    if (*txtPtr == '\0')
                        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_PARSE);
                    txtPtr++;
                    // potrebujeme pomocnou promennou
                    unsigned __int64 tmpvalue = 0;
                    while (*txtPtr >= '0' && *txtPtr <= '9')
                        tmpvalue = tmpvalue * 10 + *txtPtr++ - '0';
                    // a nastavime ve strukture
                    newfile.Size.Set((DWORD)(tmpvalue & 0xFFFFFFFF), (DWORD)(tmpvalue >> 32));
                    // a trada na dalsi radek
                    continue;
                }
                // dummy hodnoty - musime je znat, ale muzeme je ignorovat
                if (!strncmp(txtPtr, "VERSION=", 8))
                    continue;
                if (!strncmp(txtPtr, "CHECK=", 6))
                    continue;

                // neni to zadna znama polozka, tak to bude konec sekce
                break;
            }
            //
            // mame vsechno, vytvorime objekt
            //

            // ulozime do struktury, co tam jeste neni
            FILETIME lt;
            if (!SystemTimeToFileTime(&t, &lt))
            {
                char buffer[1000];
                strcpy(buffer, "SystemTimeToFileTime: ");
                strcat(buffer, GetErrorText(GetLastError()));
                free(newfile.Name);
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
            if (!LocalFileTimeToFileTime(&lt, &newfile.LastWrite))
            {
                char buffer[1000];
                strcpy(buffer, "LocalFileTimeToFileTime: ");
                strcat(buffer, GetErrorText(GetLastError()));
                free(newfile.Name);
                return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
            }
            // a nakonec uz jen vytvorime novy objekt
            newfile.IsOffline = 0;
            if (isDir)
            {
                // pokud jde o adresar, delame to tady
                newfile.Attr |= FILE_ATTRIBUTE_DIRECTORY;
                if (!Configuration.SortDirsByExt)
                    newfile.Ext = newfile.Name + newfile.NameLen; // adresare nemaji pripony
                newfile.IsLink = 0;
                if (!dir.AddDir(currentDir, newfile, NULL))
                {
                    free(newfile.Name);
                    return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_FDATA);
                }
            }
            else
            {
                newfile.IsLink = IsFileLink(newfile.Ext);

                // pokud jde o soubor, jdeme tudy
                if (!dir.AddFile(currentDir, newfile, NULL))
                {
                    free(newfile.Name);
                    return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_FDATA);
                }
            }
            // a sup na dalsi rundu
            continue;
        }
    }
    return TRUE;
}

//
// ****************************************************************************
// Funkce pro dekompresi
//

//
// ****************************************************************************
// BOOL PackUncompress(HWND parent, CFilesWindow *panel, const char *archiveFileName,
//                     CPluginDataInterfaceAbstract *pluginData,
//                     const char *targetDir, const char *archiveRoot,
//                     SalEnumSelection nextName, void *param)
//
//   Funkce pro vybaleni pozadovanych souboru z archivu.
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  parent je parent pro message-boxy
//        panel je ukazatel na souborovy panel salamandra
//        archiveFileName je nazev archivu, ze ktereho vybalujeme
//        targetDir je cesta, kam jsou soubory vybaleny
//        archiveRoot je adresar v archivu, ze ktereho vybalujeme
//        nextName je callback funkce pro enumeraci nazvu k vybaleni
//        param jsou parametry pro enumeracni funkci
//        pluginData je interface pro praci s daty o souborech/adresarich, ktere jsou specificke pro plugin
//   OUT:

BOOL PackUncompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* targetDir, const char* archiveRoot,
                    SalEnumSelection nextName, void* param)
{
    CALL_STACK_MESSAGE4("PackUncompress(, , %s, , %s, %s, ,)", archiveFileName, targetDir, archiveRoot);
    // najdeme ten pravy podle tabulky
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // Nenasli jsme podporovany archiv - chyba
    if (format == 0)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    int index = PackerFormatConfig.GetUnpackerIndex(format);

    // Nejde o interni zpracovani (DLL) ?
    if (index < 0)
    {
        CPluginData* plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelView)
        {
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->UnpackArchive(panel, archiveFileName, pluginData, targetDir, archiveRoot, nextName, param);
    }
    const SPackBrowseTable* browseTable = ArchiverConfig.GetUnpackerConfigTable(index);

    return PackUniversalUncompress(parent, browseTable->UncompressCommand,
                                   browseTable->ErrorTable, browseTable->UncompressInitDir, TRUE, panel,
                                   browseTable->SupportLongNames, archiveFileName, targetDir,
                                   archiveRoot, nextName, param, browseTable->NeedANSIListFile);
}

//
// ****************************************************************************
// BOOL PackUniversalUncompress(HWND parent, const char *command, TPackErrorTable *const errorTable,
//                              const char *initDir, BOOL expandInitDir, CFilesWindow *panel,
//                              const BOOL supportLongNames, const char *archiveFileName,
//                              const char *targetDir, const char *archiveRoot,
//                              SalEnumSelection nextName, void *param, BOOL needANSIListFile)
//
//   Funkce pro vybaleni pozadovanych souboru z archivu. Na rozdil od predchozi
//   je obecnejsi, nepouziva konfiguracni tabulky - muze byt volana samostatne,
//   vse je urceno pouze parametry
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  parent je parent pro message-boxy
//        command je prikazova radka pouzita pro vybaleni z archivu
//        errorTable je ukazatel na tabulku navratovych kodu, nebo NULL, pokud neexistuje
//        initDir je adresar, ve kterem se ma archiver spustit
//        panel je ukazatel na souborovy panel salamandra
//        supportLongNames znaci, jestli archivator umi s dlouhymi nazvy
//        archiveFileName je nazev archivu, ze ktereho vybalujeme
//        targetDir je cesta, na kterou jsou soubory vybaleny
//        archiveRoot je cesta v archivu, ze ktere vybalujeme, nebo NULL
//        nextName je callback funkce pro enumeraci souboru k vybaleni
//        param jsou parametry predavane callback funkci
//        needANSIListFile je TRUE pokud ma byt file list v ANSI (ne OEM)
//   OUT:

BOOL PackUniversalUncompress(HWND parent, const char* command, TPackErrorTable* const errorTable,
                             const char* initDir, BOOL expandInitDir, CFilesWindow* panel,
                             const BOOL supportLongNames, const char* archiveFileName,
                             const char* targetDir, const char* archiveRoot,
                             SalEnumSelection nextName, void* param, BOOL needANSIListFile)
{
    CALL_STACK_MESSAGE9("PackUniversalUncompress(, %s, , %s, %d, , %d, %s, %s, %s, , , %d)",
                        command, initDir, expandInitDir, supportLongNames, archiveFileName,
                        targetDir, archiveRoot, needANSIListFile);

    //
    // Musime upravit adresar v archivu do pozadovaneho formatu
    //
    char rootPath[MAX_PATH];
    if (archiveRoot != NULL && *archiveRoot != '\0')
    {
        if (*archiveRoot == '\\')
            archiveRoot++;
        if (*archiveRoot == '\0')
            rootPath[0] = '\0';
        else
        {
            strcpy(rootPath, archiveRoot);
            strcat(rootPath, "\\");
        }
    }
    else
    {
        rootPath[0] = '\0';
    }

    //
    // Ted prijde na radu pomocny adresar na vybaleni

    // Vytvorime nazev temporary adresare
    char tmpDirNameBuf[MAX_PATH];
    if (!SalGetTempFileName(targetDir, "PACK", tmpDirNameBuf, FALSE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    //
    // Nyni pripravime pomocny soubor s vypisem souboru k rozbaleni v %TEMP% adresari
    //

    // Vytvorime nazev temporary souboru
    char tmpListNameBuf[MAX_PATH];
    if (!SalGetTempFileName(NULL, "PACK", tmpListNameBuf, TRUE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    // mame soubor, ted ho otevreme
    FILE* listFile;
    if ((listFile = fopen(tmpListNameBuf, "w")) == NULL)
    {
        RemoveDirectory(tmpDirNameBuf);
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
    }

    // a muzeme ho plnit
    BOOL isDir;
    CQuadWord size;
    const char* name;
    char namecnv[MAX_PATH];
    CQuadWord totalSize(0, 0);
    int errorOccured;

    if (!needANSIListFile)
        CharToOem(rootPath, rootPath);
    // vybereme nazev
    while ((name = nextName(parent, 1, &isDir, &size, NULL, param, &errorOccured)) != NULL)
    {
        if (!needANSIListFile)
            CharToOem(name, namecnv);
        else
            strcpy(namecnv, name);
        // scitame celkovou velikost
        totalSize += size;
        // soupnem nazev do listu
        if (!isDir)
        {
            if (fprintf(listFile, "%s%s\n", rootPath, namecnv) <= 0)
            {
                fclose(listFile);
                DeleteFile(tmpListNameBuf);
                RemoveDirectory(tmpDirNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
            }
        }
    }
    // a je to
    fclose(listFile);

    // pokud nastala chyba a uzivatel se rozhodl pro cancel operace, ukoncime ji +
    // test na dostatek volneho mista na disku
    if (errorOccured == SALENUM_CANCEL ||
        !TestFreeSpace(parent, tmpDirNameBuf, totalSize, LoadStr(IDS_PACKERR_TITLE)))
    {
        DeleteFile(tmpListNameBuf);
        RemoveDirectory(tmpDirNameBuf);
        return FALSE;
    }

    //
    // Ted budeme poustet externi program na vybaleni
    //
    // vykonstruujeme prikazovou radku
    char cmdLine[PACK_CMDLINE_MAXLEN];
    if (!PackExpandCmdLine(archiveFileName, tmpDirNameBuf, tmpListNameBuf, NULL,
                           command, cmdLine, PACK_CMDLINE_MAXLEN, NULL))
    {
        DeleteFile(tmpListNameBuf);
        RemoveDirectory(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNERR);
    }

    // zjistime, zda neni komandlajna moc dlouha
    if (!supportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        DeleteFile(tmpListNameBuf);
        RemoveDirectory(tmpDirNameBuf);
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // vykonstruujeme aktualni adresar
    char currentDir[MAX_PATH];
    if (!expandInitDir)
    {
        if (strlen(initDir) < MAX_PATH)
            strcpy(currentDir, initDir);
        else
        {
            DeleteFile(tmpListNameBuf);
            RemoveDirectory(tmpDirNameBuf);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
        }
    }
    else
    {
        if (!PackExpandInitDir(archiveFileName, NULL, tmpDirNameBuf, initDir, currentDir, MAX_PATH))
        {
            DeleteFile(tmpListNameBuf);
            RemoveDirectory(tmpDirNameBuf);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
        }
    }

    // a spustime externi program
    if (!PackExecute(NULL, cmdLine, currentDir, errorTable))
    {
        DeleteFile(tmpListNameBuf);
        RemoveTemporaryDir(tmpDirNameBuf);
        return FALSE; // chybove hlaseni bylo jiz vyhozeno
    }
    // list souboru uz neni potreba
    DeleteFile(tmpListNameBuf);

    // a ted konecne soupneme soubory kam patri
    char srcDir[MAX_PATH];
    strcpy(srcDir, tmpDirNameBuf);
    if (*rootPath != '\0')
    {
        // vybalenou podadresarovou cestu najdeme - jmena podadresaru nemusi odpovidat kvuli
        // cestine a dlouhym nazvum :-(
        char* r = rootPath;
        WIN32_FIND_DATA foundFile;
        char buffer[1000];
        while (1)
        {
            if (*r == 0)
                break;
            while (*r != 0 && *r != '\\')
                r++; // preskok jednoho patra v puvodni rootPath
            while (*r == '\\')
                r++; // preskok backslashe v puvodni rootPath
            strcat(srcDir, "\\*");

            HANDLE found = HANDLES_Q(FindFirstFile(srcDir, &foundFile));
            if (found == INVALID_HANDLE_VALUE)
            {
                strcpy(buffer, "FindFirstFile: ");

            _ERR:

                strcat(buffer, GetErrorText(GetLastError()));
                RemoveTemporaryDir(tmpDirNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
            }
            while (foundFile.cFileName[0] == 0 ||
                   strcmp(foundFile.cFileName, ".") == 0 || // "." a ".." ignorujeme
                   strcmp(foundFile.cFileName, "..") == 0)
            {
                if (!FindNextFile(found, &foundFile))
                {
                    HANDLES(FindClose(found));
                    strcpy(buffer, "FindNextFile: ");
                    goto _ERR;
                }
            }
            HANDLES(FindClose(found));

            if (foundFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            { // pripojime dalsi podadresar na ceste
                srcDir[strlen(srcDir) - 1] = 0;
                strcat(srcDir, foundFile.cFileName);
            }
            else
            {
                TRACE_E("Unexpected error in PackUniversalUncompress().");
                RemoveTemporaryDir(tmpDirNameBuf);
                return FALSE;
            }
        }
    }
    if (!panel->MoveFiles(srcDir, targetDir, tmpDirNameBuf, archiveFileName))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_MOVE);
    }

    RemoveTemporaryDir(tmpDirNameBuf);
    return TRUE;
}

//
// ****************************************************************************
// const char * WINAPI PackEnumMask(HWND parent, int enumFiles, BOOL *isDir, CQuadWord *size,
//                                  const CFileData **fileData, void *param, int *errorOccured)
//
//   Callback funkce pro enumeraci zadanych masek
//
//   RET: Vraci masku, ktera je prave na rade, nebo NULL, pokud je hotovo
//   IN:  enumFiles je ignorovano
//        fileData je ignorovano (jen init na NULL)
//        param je ukazatel na retezec masek souboru oddelenych strednikem
//   OUT: isDir je vzdy FALSE
//        size je vzdy 0
//        dochazi ke zmene retezce masek souboru oddelenych strednikem (";"->"\0" pri oddelovani masek, ";;"->";" + eliminace ";" na konci)

const char* WINAPI PackEnumMask(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                const CFileData** fileData, void* param, int* errorOccured)
{
    CALL_STACK_MESSAGE2("PackEnumMask(%d, , ,)", enumFiles);
    if (errorOccured != NULL)
        *errorOccured = SALENUM_SUCCESS;
    // nastavime nepouzite promenne
    if (isDir != NULL)
        *isDir = FALSE;
    if (size != NULL)
        *size = CQuadWord(0, 0);
    if (fileData != NULL)
        *fileData = NULL;

    // pokud uz nejsou zadne masky, vracime NULL - konec
    if (param == NULL || *(char**)param == NULL)
        return NULL;

    // vypustime pripadny strednik na konci retezce (retezec zkracujeme)
    char* ptr = *(char**)param + strlen(*(char**)param) - 1;
    char* endPtr = ptr;
    while (1)
    {
        while (ptr >= *(char**)param && *ptr == ';')
            ptr--;
        if (((endPtr - ptr) & 1) == 1)
            *endPtr-- = 0; // ';' ignorujeme jen je-li lichy (sudy pocet pozdeji prevedeme ";;"->";")
        if (endPtr >= *(char**)param && *endPtr <= ' ')
        {
            endPtr--;
            while (endPtr >= *(char**)param && *endPtr <= ' ')
                endPtr--; // ignorujeme white-spaces na konci
            *(endPtr + 1) = 0;
            ptr = endPtr; // musime znovu zkusit, jestli neni treba preskocit lichy ';'
        }
        else
            break;
    }
    // uz nemame zadnou masku, jsou jen stredniky a white-spaces (nebo nic :-) )
    if (endPtr < *(char**)param)
        return NULL;

    // jinak najdeme posledni strednik (jen je-li jich lichy pocet, jinak se nahradi ";;"->";") - pred posledni maskou
    while (ptr >= *(char**)param)
    {
        if (*ptr == ';')
        {
            char* p = ptr - 1;
            while (p >= *(char**)param && *p == ';')
                p--;
            if (((ptr - p) & 1) == 1)
                break;
            else
                ptr = p;
        }
        else
            ptr--;
    }

    if (ptr < *(char**)param)
    {
        // pokud uz zadny strednik neni, mame jen jednu
        *(char**)param = NULL;
    }
    else
    {
        // odrizneme posledni masku od ostatnich nulou misto najiteho stredniku
        *ptr = '\0';
    }

    while (*(ptr + 1) != 0 && *(ptr + 1) <= ' ')
        ptr++; // preskocime white-spaces na zacatku masky
    char* s = ptr + 1;
    while (*s != 0)
    {
        if (*s == ';' && *(s + 1) == ';')
            memmove(s, s + 1, strlen(s + 1) + 1);
        s++;
    }
    // a vratime ji
    return ptr + 1;
}

//
// ****************************************************************************
// BOOL PackUnpackOneFile(CFilesWindow *panel, const char *archiveFileName,
//                        CPluginDataInterfaceAbstract *pluginData, const char *nameInArchive,
//                        CFileData *fileData, const char *targetDir, const char *newFileName,
//                        BOOL *renamingNotSupported)
//
//   Funkce pro vybaleni jednoho souboru z archivu (pro viewer).
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  panel je souborovy panel Salamandera
//        archiveFileName je nazev archivu, ze ktereho vybalujeme
//        nameInArchive je nazev souboru, ktery vybalujeme
//        fileData je ukazatel na strukturu CFileData vypakovavaneho souboru
//        targetDir je cesta, kam soubor vybalit
//        newFileName (neni-li NULL) je nove jmeno vybalovaneho souboru (pri vybalovani
//          musi dojit k prejmenovani z puvodniho jmena na toto nove)
//        renamingNotSupported (jen neni-li newFileName NULL) - zapsat TRUE pokud plugin
//          nepodporuje prejmenovani pri vybalovani, Salamander zobrazi chybovou hlasku
//   OUT:

BOOL PackUnpackOneFile(CFilesWindow* panel, const char* archiveFileName,
                       CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                       CFileData* fileData, const char* targetDir, const char* newFileName,
                       BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE5("PackUnpackOneFile(, %s, , %s, , %s, %s, )",
                        archiveFileName, nameInArchive, targetDir, newFileName);

    // najdeme ten pravy podle tabulky
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // Nenasli jsme podporovany archiv - chyba
    if (format == 0)
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    int index = PackerFormatConfig.GetUnpackerIndex(format);

    // Nejde o interni zpracovani (DLL) ?
    if (index < 0)
    {
        CPluginData* plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelView)
        {
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->UnpackOneFile(panel, archiveFileName, pluginData, nameInArchive,
                                     fileData, targetDir, newFileName, renamingNotSupported);
    }

    if (newFileName != NULL) // u externich archivatoru prejmenovani zatim nepodporujeme (zatim se mi nechtelo ani zjistovat, jestli to vubec umi)
    {
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_INVALIDNAME);
    }

    //
    // Vytvorime pomocny adresar, do ktereho soubor vybalime
    //

    // buffer pro fullname pomocneho adresare
    char tmpDirNameBuf[MAX_PATH];
    if (!SalGetTempFileName(targetDir, "PACK", tmpDirNameBuf, FALSE))
    {
        char buffer[1000];
        strcpy(buffer, "SalGetTempFileName: ");
        strcat(buffer, GetErrorText(GetLastError()));
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    //
    // Ted budeme poustet externi program na vybaleni
    //
    const SPackBrowseTable* browseTable = ArchiverConfig.GetUnpackerConfigTable(index);

    // vykonstruujeme prikazovou radku
    char cmdLine[PACK_CMDLINE_MAXLEN];
    if (!PackExpandCmdLine(archiveFileName, tmpDirNameBuf, NULL, nameInArchive,
                           browseTable->ExtractCommand, cmdLine, PACK_CMDLINE_MAXLEN, NULL))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNERR);
    }

    // zjistime, zda neni komandlajna moc dlouha
    if (!browseTable->SupportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        RemoveTemporaryDir(tmpDirNameBuf);
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // vykonstruujeme aktualni adresar
    char currentDir[MAX_PATH];
    if (!PackExpandInitDir(archiveFileName, NULL, tmpDirNameBuf, browseTable->ExtractInitDir,
                           currentDir, MAX_PATH))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_IDIRERR);
    }

    // a spustime externi program
    if (!PackExecute(NULL, cmdLine, currentDir, browseTable->ErrorTable))
    {
        RemoveTemporaryDir(tmpDirNameBuf);
        return FALSE; // chybove hlaseni bylo jiz vyhozeno
    }

    // vybaleny soubor najdeme - jmeno nemusi odpovidat kvuli cestine a dlouhym nazvum :-(
    char* extractedFile = (char*)malloc(strlen(tmpDirNameBuf) + 2 + 1);
    WIN32_FIND_DATA foundFile;
    strcpy(extractedFile, tmpDirNameBuf);
    strcat(extractedFile, "\\*");
    HANDLE found = HANDLES_Q(FindFirstFile(extractedFile, &foundFile));
    if (found == INVALID_HANDLE_VALUE)
    {
        char buffer[1000];
        strcpy(buffer, "FindFirstFile: ");
        strcat(buffer, GetErrorText(GetLastError()));
        RemoveTemporaryDir(tmpDirNameBuf);
        free(extractedFile);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }
    free(extractedFile);
    while (foundFile.cFileName[0] == 0 ||
           strcmp(foundFile.cFileName, ".") == 0 || strcmp(foundFile.cFileName, "..") == 0)
    {
        if (!FindNextFile(found, &foundFile))
        {
            char buffer[1000];
            strcpy(buffer, "FindNextFile: ");
            strcat(buffer, GetErrorText(GetLastError()));
            HANDLES(FindClose(found));
            RemoveTemporaryDir(tmpDirNameBuf);
            return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
        }
    }
    HANDLES(FindClose(found));

    // a nakonec ho presuneme tam kam patri
    char* srcName = (char*)malloc(strlen(tmpDirNameBuf) + 1 + strlen(foundFile.cFileName) + 1);
    strcpy(srcName, tmpDirNameBuf);
    strcat(srcName, "\\");
    strcat(srcName, foundFile.cFileName);
    const char* onlyName = strrchr(nameInArchive, '\\');
    if (onlyName == NULL)
        onlyName = nameInArchive;
    char* destName = (char*)malloc(strlen(targetDir) + 1 + strlen(onlyName) + 1);
    strcpy(destName, targetDir);
    strcat(destName, "\\");
    strcat(destName, onlyName);
    if (!SalMoveFile(srcName, destName))
    {
        char buffer[1000];
        strcpy(buffer, "MoveFile: ");
        strcat(buffer, GetErrorText(GetLastError()));
        RemoveTemporaryDir(tmpDirNameBuf);
        free(srcName);
        free(destName);
        return (*PackErrorHandlerPtr)(NULL, IDS_PACKERR_GENERAL, buffer);
    }

    // a uklidime
    free(srcName);
    free(destName);
    RemoveTemporaryDir(tmpDirNameBuf);
    return TRUE;
}
