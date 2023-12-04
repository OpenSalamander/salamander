// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "zip.h"
#include "plugins.h"
#include "pack.h"

//
// ****************************************************************************
// Konstanty a globalni promenne
// ****************************************************************************
//

// Tabulka definic archivu a zachazeni s nimi - modifikujici operace
// !!! POZOR: pri zmenach poradi externich archivatoru je treba zmenit i poradi v poli
// externalArchivers v metode CPlugins::FindViewEdit
const SPackModifyTable PackModifyTable[] =
    {
        // JAR 1.02 Win32
        {
            (TPackErrorTable*)&JARErrors, TRUE,
            "$(SourcePath)", "$(Jar32bitExecutable) a -hl \"$(ArchiveFullName)\" -o\"$(TargetPath)\" !\"$(ListFullName)\"", TRUE,
            "$(ArchivePath)", "$(Jar32bitExecutable) d -r- \"$(ArchiveFileName)\" !\"$(ListFullName)\"", PMT_EMPDIRS_DELETE,
            "$(SourcePath)", "$(Jar32bitExecutable) m -hl \"$(ArchiveFullName)\" -o\"$(TargetPath)\" !\"$(ListFullName)\"", FALSE},
        // RAR 4.20 & 5.0 Win x86/x64
        {
            (TPackErrorTable*)&RARErrors, TRUE,
            "$(SourcePath)", "$(Rar32bitExecutable) a -scol \"$(ArchiveFullName)\" -ap\"$(TargetPath)\" @\"$(ListFullName)\"", TRUE, // od verze 5.0 musime vnutit -scol switch, verzi 4.20 nevadi; vyskytuje se na dalsich mistech a v registry
            "$(ArchivePath)", "$(Rar32bitExecutable) d -scol \"$(ArchiveFileName)\" @\"$(ListFullName)\"", PMT_EMPDIRS_DELETE,
            "$(SourcePath)", "$(Rar32bitExecutable) m -scol \"$(ArchiveFullName)\" -ap\"$(TargetPath)\" @\"$(ListFullName)\"", FALSE},
        // ARJ 2.60 MS-DOS
        {
            (TPackErrorTable*)&ARJErrors, FALSE,
            "$(SourcePath)", "$(Arj16bitExecutable) a -p -va -hl -a $(ArchiveDOSFullName) !$(ListDOSFullName)", FALSE,
            ".", "$(Arj16bitExecutable) d -p -va -hl $(ArchiveDOSFullName) !$(ListDOSFullName)", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Arj16bitExecutable) m -p -va -hl -a $(ArchiveDOSFullName) !$(ListDOSFullName)", FALSE},
        // LHA 2.55 MS-DOS
        {
            (TPackErrorTable*)&LHAErrors, FALSE,
            "$(SourcePath)", "$(Lha16bitExecutable) a -m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE,
            ".", "$(Lha16bitExecutable) d -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)", PMT_EMPDIRS_DELETEWITHASTERISK,
            "$(SourcePath)", "$(Lha16bitExecutable) m -m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE},
        // UC2 2r3 PRO MS-DOS
        {
            (TPackErrorTable*)&UC2Errors, FALSE,
            "$(SourcePath)", "$(UC216bitExecutable) A !SYSHID=ON $(ArchiveDOSFullName) ##$(TargetPath) @$(ListDOSFullName)", TRUE,
            ".", "$(UC216bitExecutable) D $(ArchiveDOSFullName) @$(ListDOSFullName) & $$RED $(ArchiveDOSFullName)", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(UC216bitExecutable) AM !SYSHID=ON $(ArchiveDOSFullName) ##$(TargetPath) @$(ListDOSFullName)", FALSE},
        // JAR 1.02 MS-DOS
        {
            (TPackErrorTable*)&JARErrors, FALSE,
            "$(SourcePath)", "$(Jar16bitExecutable) a -hl $(ArchiveDOSFullName) -o\"$(TargetPath)\" !$(ListDOSFullName)", TRUE,
            "$(ArchivePath)", "$(Jar16bitExecutable) d -r- $(ArchiveDOSFileName) !$(ListDOSFullName)", PMT_EMPDIRS_DELETE,
            "$(SourcePath)", "$(Jar16bitExecutable) m -hl $(ArchiveDOSFullName) -o\"$(TargetPath)\" !$(ListDOSFullName)", FALSE},
        // RAR 2.50 MS-DOS
        {
            (TPackErrorTable*)&RARErrors, FALSE,
            "$(SourcePath)", "$(Rar16bitExecutable) a $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE, //P.S. vyhozena schopnost baleni do podadresaru
            "$(ArchivePath)", "$(Rar16bitExecutable) d $(ArchiveDOSFileName) @$(ListDOSFullName)", PMT_EMPDIRS_DELETE,
            "$(SourcePath)", "$(Rar16bitExecutable) m $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE //P.S. vyhozena schopnost baleni do podadresaru
        },
        // PKZIP 2.50 Win32
        {
            NULL, TRUE,
            "$(SourcePath)", "$(Zip32bitExecutable) -add -nozipextension -attr -path \"$(ArchiveFullName)\" @\"$(ListFullName)\"", FALSE,
            "$(ArchivePath)", "$(Zip32bitExecutable) -del -nozipextension \"$(ArchiveFileName)\" @\"$(ListFullName)\"", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Zip32bitExecutable) -add -nozipextension -attr -path -move \"$(ArchiveFullName)\" @\"$(ListFullName)\"", TRUE},
        // PKZIP 2.04g MS-DOS
        {
            (TPackErrorTable*)&ZIP204Errors, FALSE,
            "$(SourcePath)", "$(Zip16bitExecutable) -a -P -whs $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE,
            ".", "$(Zip16bitExecutable) -d $(ArchiveDOSFullName) @$(ListDOSFullName)", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Zip16bitExecutable) -m -P -whs $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE},
        // ARJ 3.00c Win32
        {
            (TPackErrorTable*)&ARJErrors, TRUE,
            "$(SourcePath)", "$(Arj32bitExecutable) a -p -va -hl -a \"$(ArchiveFullName)\" !\"$(ListFullName)\"", FALSE,
            "$(ArchivePath)", "$(Arj32bitExecutable) d -p -va -hl \"$(ArchiveFileName)\" !\"$(ListFullName)\"", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Arj32bitExecutable) m -p -va -hl -a \"$(ArchiveFullName)\" !\"$(ListFullName)\"", FALSE},
        // ACE 1.2b Win32
        {
            (TPackErrorTable*)&ACEErrors, TRUE,
            "$(SourcePath)", "$(Ace32bitExecutable) a -o -f \"$(ArchiveFullName)\" @\"$(ListFullName)\"", FALSE,
            "$(ArchivePath)", "$(Ace32bitExecutable) d -f \"$(ArchiveFileName)\" @\"$(ListFullName)\"", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Ace32bitExecutable) m -o -f \"$(ArchiveFullName)\" @\"$(ListFullName)\"", TRUE},
        // ACE 1.2b MS-DOS
        {
            (TPackErrorTable*)&ACEErrors, FALSE,
            "$(SourcePath)", "$(Ace16bitExecutable) a -o -f $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE,
            ".", "$(Ace16bitExecutable) d -f $(ArchiveDOSFullName) @$(ListDOSFullName)", PMT_EMPDIRS_DONOTDELETE,
            "$(SourcePath)", "$(Ace16bitExecutable) m -o -f $(ArchiveDOSFullName) @$(ListDOSFullName)", FALSE}};

//
// ****************************************************************************
// Funkce
// ****************************************************************************
//

//
// ****************************************************************************
// Funkce pro kompresi
//

//
// ****************************************************************************
// BOOL PackCompress(HWND parent, CFilesWindow *panel, const char *archiveFileName,
//                   const char *archiveRoot, BOOL move, const char *sourceDir,
//                   SalEnumSelection2 nextName, void *param)
//
//   Funkce pro pridani pozadovanych souboru do archivu.
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  parent je parent message-boxu
//        panel je ukazatel na souborovy panel salamandra
//        archiveFileName je nazev archivu, do ktereho balime
//        archiveRoot je adresar v archivu, do ktereho balime
//        move je TRUE, pokud soubory do archivu presunujeme
//        sourceDir je cesta, ze ktere jsou soubory baleny
//        nextName je callback funkce pro enumeraci nazvu k baleni
//        param jsou parametry pro enumeracni funkci
//   OUT:

BOOL PackCompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                  const char* archiveRoot, BOOL move, const char* sourceDir,
                  SalEnumSelection2 nextName, void* param)
{
    CALL_STACK_MESSAGE5("PackCompress(, , %s, %s, %d, %s, ,)", archiveFileName,
                        archiveRoot, move, sourceDir);
    // najdeme ten pravy podle tabulky
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // Nenasli jsme podporovany archiv - chyba
    if (format == 0)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    if (!PackerFormatConfig.GetUsePacker(format))
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PACKER_UNSUP);
    int index = PackerFormatConfig.GetPackerIndex(format);

    // Nejde o interni zpracovani (DLL) ?
    if (index < 0)
    {
        CPluginData* plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelEdit)
        {
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->PackToArchive(panel, archiveFileName, archiveRoot, move,
                                     sourceDir, nextName, param);
    }

    const SPackModifyTable* modifyTable = ArchiverConfig.GetPackerConfigTable(index);

    // zjistime, jestli provadime copy nebo move
    const char* compressCommand;
    const char* compressInitDir;
    if (!move)
    {
        compressCommand = modifyTable->CompressCommand;
        compressInitDir = modifyTable->CompressInitDir;
    }
    else
    {
        compressCommand = modifyTable->MoveCommand;
        compressInitDir = modifyTable->MoveInitDir;
        if (compressCommand == NULL)
        {
            BOOL ret = (*PackErrorHandlerPtr)(parent, IDS_PACKQRY_NOMOVE);
            if (ret)
            {
                compressCommand = modifyTable->CompressCommand;
                compressInitDir = modifyTable->CompressInitDir;
            }
            else
                return FALSE;
        }
    }

    //
    // V pripade ze archivator nepodporuje baleni do adresare, musime to resit
    //
    char archiveRootPath[MAX_PATH];
    if (archiveRoot != NULL && *archiveRoot != '\0')
    {
        strcpy(archiveRootPath, archiveRoot);
        if (!modifyTable->CanPackToDir) // archivacni program ji nepodporuje
        {
            if ((*PackErrorHandlerPtr)(parent, IDS_PACKQRY_ARCPATH))
                strcpy(archiveRootPath, "\\"); // uzivatel ji chce ignorovat
            else
                return FALSE; // uzivateli to vadi
        }
    }
    else
        strcpy(archiveRootPath, "\\");

    // a provedeme vlastni pakovani
    return PackUniversalCompress(parent, compressCommand, modifyTable->ErrorTable,
                                 compressInitDir, TRUE, modifyTable->SupportLongNames, archiveFileName,
                                 sourceDir, archiveRootPath, nextName, param, modifyTable->NeedANSIListFile);
}

//
// ****************************************************************************
// BOOL PackUniversalCompress(HWND parent, const char *command, TPackErrorTable *const errorTable,
//                            const char *initDir, BOOL expandInitDir, const BOOL supportLongNames,
//                            const char *archiveFileName, const char *sourceDir,
//                            const char *archiveRoot, SalEnumSelection2 nextName,
//                            void *param, BOOL needANSIListFile)
//
//   Funkce pro pridani pozadovanych souboru do archivu. Na rozdil od predchozi
//   je obecnejsi, nepouziva konfiguracni tabulky - muze byt volana samostatne,
//   vse je urceno pouze parametry
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  parent je parent pro message-boxy
//        command je prikazova radka pouzita pro baleni do archivu
//        errorTable ukazatel na tabulku navratovych kodu archivatoru, nebo NULL, pokud neexistuje
//        initDir je adresar, ve kterem bude program spusten
//        supportLongNames znaci, jestli program podporuje pouziti dlouhych nazvu
//        archiveFileName je nazev archivu, do ktereho balime
//        sourceDir je cesta, ze ktere jsou soubory baleny
//        archiveRoot je adresar v archivu, do ktereho balime
//        nextName je callback funkce pro enumeraci nazvu k baleni
//        param jsou parametry pro enumeracni funkci
//        needANSIListFile je TRUE pokud ma byt file list v ANSI (ne OEM)
//   OUT:

BOOL PackUniversalCompress(HWND parent, const char* command, TPackErrorTable* const errorTable,
                           const char* initDir, BOOL expandInitDir, const BOOL supportLongNames,
                           const char* archiveFileName, const char* sourceDir,
                           const char* archiveRoot, SalEnumSelection2 nextName,
                           void* param, BOOL needANSIListFile)
{
    CALL_STACK_MESSAGE9("PackUniversalCompress(, %s, , %s, %d, %d, %s, %s, %s, , , %d)",
                        command, initDir, expandInitDir, supportLongNames, archiveFileName,
                        sourceDir, archiveRoot, needANSIListFile);

    //
    // Musime upravit adresar v archivu do pozadovaneho formatu
    //
    char rootPath[MAX_PATH];
    rootPath[0] = '\0';
    if (archiveRoot != NULL && *archiveRoot != '\0')
    {
        while (*archiveRoot == '\\')
            archiveRoot++;
        if (*archiveRoot != '\0')
        {
            strcpy(rootPath, "\\");
            strcat(rootPath, archiveRoot);
            while (rootPath[0] != '\0' && rootPath[strlen(rootPath) - 1] == '\\')
                rootPath[strlen(rootPath) - 1] = '\0';
        }
    }
    // u 32bit programu budou prazdne uvozovky, u 16bit dame slash
    if (!supportLongNames && rootPath[0] == '\0')
    {
        rootPath[0] = '\\';
        rootPath[1] = '\0';
    }

    // Pro kontrolu delky cesty budeme potrebovat sourceDir v "kratkem" tvaru
    char sourceShortName[MAX_PATH];
    if (!supportLongNames)
    {
        if (!GetShortPathName(sourceDir, sourceShortName, MAX_PATH))
        {
            char buffer[1000];
            strcpy(buffer, "GetShortPathName: ");
            strcat(buffer, GetErrorText(GetLastError()));
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
        }
    }
    else
        strcpy(sourceShortName, sourceDir);

    //
    // v %TEMP% adresari bude pomocny soubor se seznamem souboru k zabaleni
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
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
    }

    // a muzeme ho plnit
    BOOL isDir;

    const char* name;
    unsigned int maxPath;
    char namecnv[MAX_PATH];
    if (!supportLongNames)
        maxPath = DOS_MAX_PATH;
    else
        maxPath = MAX_PATH;
    if (!needANSIListFile)
        CharToOem(sourceShortName, sourceShortName);
    int sourceDirLen = (int)strlen(sourceShortName) + 1;
    int errorOccured;
    // vybereme nazev
    while ((name = nextName(parent, 1, NULL, &isDir, NULL, NULL, NULL, param, &errorOccured)) != NULL)
    {
        if (supportLongNames)
        {
            if (!needANSIListFile)
                CharToOem(name, namecnv);
            else
                strcpy(namecnv, name);
        }
        else
        {
            if (GetShortPathName(name, namecnv, MAX_PATH) == 0)
            {
                char buffer[1000];
                strcpy(buffer, "File: ");
                strcat(buffer, name);
                strcat(buffer, ", GetShortPathName: ");
                strcat(buffer, GetErrorText(GetLastError()));
                fclose(listFile);
                DeleteFile(tmpListNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
            }
            if (!needANSIListFile)
                CharToOem(namecnv, namecnv);
        }

        // osetrime delku
        if (sourceDirLen + strlen(namecnv) >= maxPath)
        {
            char buffer[1000];
            fclose(listFile);
            DeleteFile(tmpListNameBuf);
            sprintf(buffer, "%s\\%s", sourceShortName, namecnv);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PATH, buffer);
        }

        // a soupnem ho do listu
        if (!isDir)
        {
            if (fprintf(listFile, "%s\n", namecnv) <= 0)
            {
                fclose(listFile);
                DeleteFile(tmpListNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
            }
        }
    }
    // a je to
    fclose(listFile);

    // pokud nastala chyba a uzivatel se rozhodl pro cancel operace, ukoncime ji
    if (errorOccured == SALENUM_CANCEL)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE;
    }

    //
    // Ted budeme poustet externi program na vybaleni
    //
    // vykonstruujeme prikazovou radku
    char cmdLine[PACK_CMDLINE_MAXLEN];
    // buffer pro nahradni jmeno (pokud vytvarime archiv s dlouhym jmenem a potrebujeme jeho DOS jmeno,
    // expanduje se misto dlouheho jmena DOSTmpName, po vytvoreni archivu soubor prejmenujeme)
    char DOSTmpName[MAX_PATH];
    if (!PackExpandCmdLine(archiveFileName, rootPath, tmpListNameBuf, NULL,
                           command, cmdLine, PACK_CMDLINE_MAXLEN, DOSTmpName))
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNERR);
    }

    // hack pro RAR 4.x+, kteremu vadi -ap"" pro adresaci rootu archivu; zaroven je toto promazani kompatibilni se starou verzi RAR
    // viz https://forum.altap.cz/viewtopic.php?f=2&t=5487
    if (*rootPath == 0 && strstr(command, "$(Rar32bitExecutable) ") == command)
    {
        char* pAP = strstr(cmdLine, "\" -ap\"\" @\"");
        if (pAP != NULL)
            memmove(pAP + 1, "         ", 7); // sestrelime -ap"", ktery s novym RAR zlobi
    }
    // hack pro kopirovani do adresare v RAR - vadi mu, kdyz cesta zacina zpetnym lomitkem; vytvoril napr adresar \Test ktery ale Salam ukazuje jako Test
    // https://forum.altap.cz/viewtopic.php?p=24586#p24586
    if (*rootPath == '\\' && strstr(command, "$(Rar32bitExecutable) ") == command)
    {
        char* pAP = strstr(cmdLine, "\" -ap\"\\");
        if (pAP != NULL)
            memmove(pAP + 6, pAP + 7, strlen(pAP + 7) + 1); // odmazneme zpetne lomitko ze zacatku
    }

    // zjistime, zda neni komandlajna moc dlouha
    if (!supportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        strcpy(buffer, cmdLine);
        DeleteFile(tmpListNameBuf);
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
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
        }
    }
    else
    {
        if (!PackExpandInitDir(archiveFileName, sourceDir, rootPath, initDir, currentDir,
                               MAX_PATH))
        {
            DeleteFile(tmpListNameBuf);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
        }
    }

    // zazalohujeme si kratky nazev souboru archivu, pozdeji testneme jestli dlouhe jmeno
    // nezaniklo -> pokud kratke zustalo, prejmenujeme kratke na puvodni dlouhe
    char DOSArchiveFileName[MAX_PATH];
    if (!GetShortPathName(archiveFileName, DOSArchiveFileName, MAX_PATH))
        DOSArchiveFileName[0] = 0;

    // a spustime externi program
    BOOL exec = PackExecute(parent, cmdLine, currentDir, errorTable);
    // mezitim testneme jestli dlouhe jmeno nezaniklo -> pokud kratke zustalo, prejmenujeme kratke
    // na puvodni dlouhe
    if (DOSArchiveFileName[0] != 0 &&
        SalGetFileAttributes(archiveFileName) == 0xFFFFFFFF &&
        SalGetFileAttributes(DOSArchiveFileName) != 0xFFFFFFFF)
    {
        SalMoveFile(DOSArchiveFileName, archiveFileName); // pokud se nepovede, kasleme na to...
    }
    if (!exec)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE; // chybove hlaseni bylo jiz vyhozeno
    }

    // list souboru uz neni potreba
    DeleteFile(tmpListNameBuf);

    // pokud jsme pouzili nahradni DOS jmeno, prejmenujeme vsechny soubory tohoto jmena (jmeno.*) na pozadovane long jmeno
    if (DOSTmpName[0] != 0)
    {
        char src[2 * MAX_PATH];
        strcpy(src, DOSTmpName);
        char* tmpOrigName;
        CutDirectory(src, &tmpOrigName);
        tmpOrigName = DOSTmpName + (tmpOrigName - src);
        SalPathAddBackslash(src, 2 * MAX_PATH);
        char* srcName = src + strlen(src);
        char dstNameBuf[2 * MAX_PATH];
        strcpy(dstNameBuf, archiveFileName);
        char* dstExt = dstNameBuf + strlen(dstNameBuf);
        //    while (--dstExt > dstNameBuf && *dstExt != '\\' && *dstExt != '.');
        while (--dstExt >= dstNameBuf && *dstExt != '\\' && *dstExt != '.')
            ;
        //    if (dstExt == dstNameBuf || *dstExt == '\\' || *(dstExt - 1) == '\\') dstExt = dstNameBuf + strlen(dstNameBuf); // u "name", ".cvspass", "path\name" ani "path\.name" neni zadna pripona
        if (dstExt < dstNameBuf || *dstExt == '\\')
            dstExt = dstNameBuf + strlen(dstNameBuf); // u "name" ani "path\name" neni zadna pripona; ".cvspass" ve Windows je pripona
        char path[MAX_PATH];
        strcpy(path, DOSTmpName);
        char* ext = path + strlen(path);
        //    while (--ext > path && *ext != '\\' && *ext != '.');
        while (--ext >= path && *ext != '\\' && *ext != '.')
            ;
        //    if (ext == path || *ext == '\\' || *(ext - 1) == '\\') ext = path + strlen(path); // u "name", ".cvspass", "path\name" ani ""path\.name" neni zadna pripona
        if (ext < path || *ext == '\\')
            ext = path + strlen(path); // u "name" ani "path\name" neni zadna pripona; ".cvspass" ve Windows je pripona
        strcpy(ext, ".*");
        WIN32_FIND_DATA findData;
        int i;
        for (i = 0; i < 2; i++)
        {
            HANDLE find = HANDLES_Q(FindFirstFile(path, &findData));
            if (find != INVALID_HANDLE_VALUE)
            {
                do
                {
                    strcpy(srcName, findData.cFileName);
                    const char* dst;
                    if (StrICmp(tmpOrigName, findData.cFileName) == 0)
                        dst = archiveFileName;
                    else
                    {
                        char* srcExt = findData.cFileName + strlen(findData.cFileName);
                        //            while (--srcExt > findData.cFileName && *srcExt != '.');
                        while (--srcExt >= findData.cFileName && *srcExt != '.')
                            ;
                        //            if (srcExt == findData.cFileName) srcExt = findData.cFileName + strlen(findData.cFileName);  // ".cvspass" ve Windows je pripona ...
                        if (srcExt < findData.cFileName)
                            srcExt = findData.cFileName + strlen(findData.cFileName);
                        strcpy(dstExt, srcExt);
                        dst = dstNameBuf;
                    }
                    if (i == 0)
                    {
                        if (SalGetFileAttributes(dst) != 0xffffffff)
                        {
                            HANDLES(FindClose(find)); // toto jmeno uz s nejakou priponou existuje, hledame dale
                            (*PackErrorHandlerPtr)(parent, IDS_PACKERR_UNABLETOREN, src, dst);
                            return TRUE; // povedlo se, jen jsou trochu jina jmena vysledneho archivu (i multivolume)
                        }
                    }
                    else
                    {
                        if (!SalMoveFile(src, dst))
                        {
                            DWORD err = GetLastError();
                            TRACE_E("Error (" << err << ") in SalMoveFile(" << src << ", " << dst << ").");
                        }
                    }
                } while (FindNextFile(find, &findData));
                HANDLES(FindClose(find)); // toto jmeno uz s nejakou priponou existuje, hledame dale
            }
        }
    }

    return TRUE;
}

//
// ****************************************************************************
// Funkce pro mazani z archivu
//

//
// ****************************************************************************
// BOOL PackDelFromArc(HWND parent, CFilesWindow *panel, const char *archiveFileName,
//                     CPluginDataInterfaceAbstract *pluginData,
//                     const char *archiveRoot, SalEnumSelection nextName,
//                     void *param)
//
//   Funkce pro vymazani pozadovanych souboru z archivu.
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  parent je parent pro message-boxy
//        panel je ukazatel na souborovy panel salamandra
//        archiveFileName je nazev archivu, ze ktereho mazeme
//        archiveRoot je adresar v archivu, ze ktereho mazeme
//        nextName je callback funkce pro enumeraci nazvu k mazani
//        param jsou parametry pro enumeracni funkci
//   OUT:

BOOL PackDelFromArc(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* archiveRoot, SalEnumSelection nextName,
                    void* param)
{
    CALL_STACK_MESSAGE3("PackDelFromArc(, , %s, , %s, , ,)", archiveFileName, archiveRoot);

    // najdeme ten pravy podle tabulky
    int format = PackerFormatConfig.PackIsArchive(archiveFileName);
    // Nenasli jsme podporovany archiv - chyba
    if (format == 0)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);

    format--;
    if (!PackerFormatConfig.GetUsePacker(format))
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PACKER_UNSUP);
    int index = PackerFormatConfig.GetPackerIndex(format);

    // Nejde o interni zpracovani (DLL) ?
    if (index < 0)
    {
        CPluginData* plugin = Plugins.Get(-index - 1);
        if (plugin == NULL || !plugin->SupportPanelEdit)
        {
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_ARCNAME_UNSUP);
        }
        return plugin->DeleteFromArchive(panel, archiveFileName, pluginData, archiveRoot,
                                         nextName, param);
    }

    const SPackModifyTable* modifyTable = ArchiverConfig.GetPackerConfigTable(index);
    BOOL needANSIListFile = modifyTable->NeedANSIListFile;

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
            if (rootPath[strlen(rootPath) - 1] != '\\')
                strcat(rootPath, "\\");
        }
    }
    else
    {
        rootPath[0] = '\0';
    }

    //
    // v %TEMP% adresari bude pomocny soubor se seznamem souboru k vymazani
    //
    // buffer pro fullname pomocneho souboru
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
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
    }

    // a muzeme ho plnit
    BOOL isDir;
    const char* name;
    char namecnv[MAX_PATH];
    int errorOccured;
    if (!needANSIListFile)
        CharToOem(rootPath, rootPath);
    // vybereme nazev
    while ((name = nextName(parent, 1, &isDir, NULL, NULL, param, &errorOccured)) != NULL)
    {
        if (!needANSIListFile)
            CharToOem(name, namecnv);
        else
            strcpy(namecnv, name);
        // a soupnem ho do listu
        if (!isDir)
        {
            if (fprintf(listFile, "%s%s\n", rootPath, namecnv) <= 0)
            {
                fclose(listFile);
                DeleteFile(tmpListNameBuf);
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
            }
        }
        else
        {
            if (modifyTable->DelEmptyDir == PMT_EMPDIRS_DELETE)
            {
                if (fprintf(listFile, "%s%s\n", rootPath, namecnv) <= 0)
                {
                    fclose(listFile);
                    DeleteFile(tmpListNameBuf);
                    return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
                }
            }
            else
            {
                if (modifyTable->DelEmptyDir == PMT_EMPDIRS_DELETEWITHASTERISK)
                {
                    if (fprintf(listFile, "%s%s\\*\n", rootPath, namecnv) <= 0)
                    {
                        fclose(listFile);
                        DeleteFile(tmpListNameBuf);
                        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_FILE);
                    }
                }
            }
        }
    }
    // a je to
    fclose(listFile);

    // pokud nastala chyba a uzivatel se rozhodl pro cancel operace, ukoncime ji
    if (errorOccured == SALENUM_CANCEL)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE;
    }

    //
    // Ted budeme poustet externi program na mazani
    //
    // vykonstruujeme prikazovou radku
    char cmdLine[PACK_CMDLINE_MAXLEN];
    if (!PackExpandCmdLine(archiveFileName, NULL, tmpListNameBuf, NULL,
                           modifyTable->DeleteCommand, cmdLine, PACK_CMDLINE_MAXLEN, NULL))
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNERR);
    }

    // zjistime, zda neni komandlajna moc dlouha
    if (!modifyTable->SupportLongNames && strlen(cmdLine) >= 128)
    {
        char buffer[1000];
        DeleteFile(tmpListNameBuf);
        strcpy(buffer, cmdLine);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_CMDLNLEN, buffer);
    }

    // vykonstruujeme aktualni adresar
    char currentDir[MAX_PATH];
    if (!PackExpandInitDir(archiveFileName, NULL, NULL, modifyTable->DeleteInitDir,
                           currentDir, MAX_PATH))
    {
        DeleteFile(tmpListNameBuf);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_IDIRERR);
    }

    // vezmeme si atributy pro pripad pozdejsiho pouziti
    DWORD fileAttrs = SalGetFileAttributes(archiveFileName);
    if (fileAttrs == 0xFFFFFFFF)
        fileAttrs = FILE_ATTRIBUTE_ARCHIVE;

    // zazalohujeme si kratky nazev souboru archivu, pozdeji testneme jestli dlouhe jmeno
    // nezaniklo -> pokud kratke zustalo, prejmenujeme kratke na puvodni dlouhe
    char DOSArchiveFileName[MAX_PATH];
    if (!GetShortPathName(archiveFileName, DOSArchiveFileName, MAX_PATH))
        DOSArchiveFileName[0] = 0;

    // a spustime externi program
    BOOL exec = PackExecute(NULL, cmdLine, currentDir, modifyTable->ErrorTable);
    // mezitim testneme jestli dlouhe jmeno nezaniklo -> pokud kratke zustalo, prejmenujeme kratke
    // na puvodni dlouhe
    if (DOSArchiveFileName[0] != 0 &&
        SalGetFileAttributes(archiveFileName) == 0xFFFFFFFF &&
        SalGetFileAttributes(DOSArchiveFileName) != 0xFFFFFFFF)
    {
        SalMoveFile(DOSArchiveFileName, archiveFileName); // pokud se nepovede, kasleme na to...
    }
    if (!exec)
    {
        DeleteFile(tmpListNameBuf);
        return FALSE; // chybove hlaseni bylo jiz vyhozeno
    }

    // pokud mazani zrusilo archiv, vytvorime soubor s nulovou delkou
    HANDLE tmpHandle = HANDLES_Q(CreateFile(archiveFileName, GENERIC_READ, 0, NULL,
                                            OPEN_ALWAYS, fileAttrs, NULL));
    if (tmpHandle != INVALID_HANDLE_VALUE)
        HANDLES(CloseHandle(tmpHandle));

    // list souboru uz neni potreba
    DeleteFile(tmpListNameBuf);

    return TRUE;
}
