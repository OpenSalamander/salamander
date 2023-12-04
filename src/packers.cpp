// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "plugins.h"
#include "zip.h"
#include "pack.h"

// custom packers / unpackers
const char* SALAMANDER_CPU_TITLE = "Title";
const char* SALAMANDER_CPU_EXT = "Ext";
const char* SALAMANDER_CPU_TYPE = "Type";
const char* SALAMANDER_CPU_SUPLONG = "Support Long Names";
const char* SALAMANDER_CPU_ANSILIST = "Need ANSI List";
// custom packers only
const char* SALAMANDER_CP_EXECCOPY = "Copy Command";
const char* SALAMANDER_CP_ARGSCOPY = "Copy Arguments";
const char* SALAMANDER_CP_SUPMOVE = "Support Move";
const char* SALAMANDER_CP_EXECMOVE = "Move Command";
const char* SALAMANDER_CP_ARGSMOVE = "Move Arguments";
// custom unpackers only
const char* SALAMANDER_CU_EXECEXTRACT = "Extract Command";
const char* SALAMANDER_CU_ARGSEXTRACT = "Extract Arguments";

// konverzni tanulka pro preklad exe->promenna
struct SPackConvTable
{
    const char* exe;
    const char* variable;
};

SPackConvTable PackConversionTable[] = {
    {"jar32", "$(Jar32bitExecutable)"},
    {"jar16", "$(Jar16bitExecutable)"},
    {"rar", "$(Rar32bitExecutable)"},
    {"arj32", "$(Arj32bitExecutable)"},
    {"arj", "$(Arj16bitExecutable)"},
    {"ace32", "$(Ace32bitExecutable)"},
    {"ace", "$(Ace16bitExecutable)"},
    {"lha", "$(Lha16bitExecutable)"},
    {"uc", "$(UC216bitExecutable)"},
    {"pkzip25", "$(Zip32bitExecutable)"},
    {"pkzip", "$(Zip16bitExecutable)"},
    {"pkunzip", "$(Unzip16bitExecutable)"},
    {NULL, NULL}};

// poradi, jak byly custom packery/unpackery historicky pridavany
int CustomOrder[] = {0, 1, 9, 10, 2, 3, 4, 11, 5, 6, 7, 8};

// tabulka custom packeru
SPackCustomPacker CustomPackers[] = {
    // JAR32
    {{"a \"$(ArchiveFullName)\" !\"$(ListFullName)\"", "a -v1440 \"$(ArchiveFullName)\" !\"$(ListFullName)\""},
     {"m \"$(ArchiveFullName)\" !\"$(ListFullName)\"", "m -v1440 \"$(ArchiveFullName)\" !\"$(ListFullName)\""},
     {IDS_DP_JAR_E, IDS_DP_JARV_E},
     "j",
     TRUE,
     FALSE,
     "jar32"},
    // RAR32
    {{"a -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"", "a -scol -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\""}, // od verze 5.0 musime vnutit -scol switch, verzi 4.20 nevadi; vyskytuje se na dalsich mistech a v registry
     {"m -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"", "m -scol -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\""},
     {IDS_DP_RAR_E, IDS_DP_RARV_E},
     "rar",
     TRUE,
     FALSE,
     "rar"},
    // ARJ16
    {{"a -pa $(ArchiveDOSFullName) !$(ListDOSFullName)", "a -pav1440 $(ArchiveDOSFullName) !$(ListDOSFullName)"},
     {"m -pa $(ArchiveDOSFullName) !$(ListDOSFullName)", "m -pav1440 $(ArchiveDOSFullName) !$(ListDOSFullName)"},
     {IDS_DP_ARJ16_E, IDS_DP_ARJ16V_E},
     "arj",
     FALSE,
     FALSE,
     "arj"},
    // LZH
    {{"a -m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)", NULL},
     {"m -m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)", NULL},
     {IDS_DP_LHA_E, -1},
     "lzh",
     FALSE,
     FALSE,
     "lha"},
    // UC2
    {{"A !SYSHID=ON ##\\ $(ArchiveDOSFullName) @$(ListDOSFullName)", NULL},
     {"AM !SYSHID=ON ##\\ $(ArchiveDOSFullName) @$(ListDOSFullName)", NULL},
     {IDS_DP_UC2_E, -1},
     "uc2",
     FALSE,
     FALSE,
     "uc"},
    // JAR16
    {{"a $(ArchiveDOSFullName) !$(ListDOSFullName)", "a -v1440 $(ArchiveDOSFullName) !$(ListDOSFullName)"},
     {"m $(ArchiveDOSFullName) !$(ListDOSFullName)", "m -v1440 $(ArchiveDOSFullName) !$(ListDOSFullName)"},
     {IDS_DP_JAR16_E, IDS_DP_JAR16V_E},
     "j",
     FALSE,
     FALSE,
     "jar16"},
    // RAR16
    {{"a $(ArchiveDOSFullName) @$(ListDOSFullName)", "a -v1440 $(ArchiveDOSFullName) @$(ListDOSFullName)"},
     {"m $(ArchiveDOSFullName) @$(ListDOSFullName)", "m -v1440 $(ArchiveDOSFullName) @$(ListDOSFullName)"},
     {IDS_DP_RAR16_E, IDS_DP_RAR16V_E},
     "rar",
     FALSE,
     FALSE,
     "rar"},
    // ZIP32
    {{"-add -nozipextension -path -attr \"$(ArchiveFullName)\" @\"$(ListFullName)\"", NULL},
     {"-add -nozipextension -move -path -attr \"$(ArchiveFullName)\" @\"$(ListFullName)\"", NULL},
     {IDS_DP_ZIP32_E, -1},
     "zip",
     TRUE,
     TRUE,
     "pkzip25"},
    // ZIP16
    {{"-P -whs $(ArchiveDOSFullName) @$(ListDOSFullName)", NULL},
     {"-m -P -whs $(ArchiveDOSFullName) @$(ListDOSFullName)", NULL},
     {IDS_DP_ZIP16_E, -1},
     "zip",
     FALSE,
     FALSE,
     "pkzip"},
    // ARJ32
    {{"a -pa \"$(ArchiveFullName)\" !\"$(ListFullName)\"", "a -pav1440 \"$(ArchiveFullName)\" !\"$(ListFullName)\""},
     {"m -pa \"$(ArchiveFullName)\" !\"$(ListFullName)\"", "m -pav1440 \"$(ArchiveFullName)\" !\"$(ListFullName)\""},
     {IDS_DP_ARJ32_E, IDS_DP_ARJ32V_E},
     "arj",
     TRUE,
     FALSE,
     "arj32"},
    // ACE32
    {{"a \"$(ArchiveFullName)\" @\"$(ListFullName)\"", "a -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\""},
     {"m \"$(ArchiveFullName)\" @\"$(ListFullName)\"", "m -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\""},
     {IDS_DP_ACE_E, IDS_DP_ACEV_E},
     "ace",
     TRUE,
     TRUE,
     "ace32"},
    // ACE16
    {{"a $(ArchiveDOSFullName) @$(ListDOSFullName)", "a -v1440 $(ArchiveDOSFullName) @$(ListDOSFullName)"},
     {"m $(ArchiveDOSFullName) @$(ListDOSFullName)", "m -v1440 $(ArchiveDOSFullName) @$(ListDOSFullName)"},
     {IDS_DP_ACE16_E, IDS_DP_ACE16V_E},
     "ace",
     FALSE,
     FALSE,
     "ace"},
};

// tabulka custom unpackeru
SPackCustomUnpacker CustomUnpackers[] = {
    // JAR32
    {"x -jyc \"$(ArchiveFullName)\" !\"$(ListFullName)\"", IDS_DU_JAR_E, "*.j", TRUE, FALSE, "jar32"},
    // RAR32
    {"x -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"", IDS_DU_RAR_E, "*.rar", TRUE, FALSE, "rar"}, // od verze 5.0 musime vnutit -scol switch, verzi 4.20 nevadi; vyskytuje se na dalsich mistech a v registry
    // ARJ16
    {"x -va -jyc $(ArchiveDOSFullName) !$(ListDOSFullName)", IDS_DU_ARJ16_E, "*.arj", FALSE, FALSE, "arj"},
    // LZH
    {"x -a -l1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)", IDS_DU_LHA_E, "*.lzh", FALSE, FALSE, "lha"},
    // UC2
    {"ESF $(ArchiveDOSFullName) @$(ListDOSFullName)", IDS_DU_UC2_E, "*.uc2", FALSE, FALSE, "uc"},
    // JAR16
    {"x -jyc $(ArchiveDOSFullName) !$(ListDOSFullName)", IDS_DU_JAR16_E, "*.j", FALSE, FALSE, "jar16"},
    // RAR16
    {"x $(ArchiveDOSFullName) @$(ListDOSFullName)", IDS_DU_RAR16_E, "*.rar", FALSE, FALSE, "rar"},
    // ZIP32
    {"-ext -nozipextension -directories -path \"$(ArchiveFullName)\" @\"$(ListFullName)\"", IDS_DU_ZIP32_E, "*.zip;*.pk3;*.jar", TRUE, TRUE, "pkzip25"},
    // ZIP16
    {"-d -Jhrs $(ArchiveDOSFullName) @$(ListDOSFullName)", IDS_DU_ZIP16_E, "*.zip", FALSE, FALSE, "pkunzip"},
    // ARJ32
    {"x -va -jyc \"$(ArchiveFullName)\" !\"$(ListFullName)\"", IDS_DU_ARJ32_E, "*.arj", TRUE, FALSE, "arj32"},
    // ACE32
    {"x \"$(ArchiveFullName)\" @\"$(ListFullName)\"", IDS_DU_ACE_E, "*.ace", TRUE, TRUE, "ace32"},
    // ACE16
    {"x $(ArchiveDOSFullName) @$(ListDOSFullName)", IDS_DU_ACE16_E, "*.ace", FALSE, FALSE, "ace"},
};

//
// ****************************************************************************
// CPackerConfig
//

CPackerConfig::CPackerConfig(/*BOOL disableDefaultValues*/)
    : Packers(30, 10)
{
    PreferedPacker = -1;
    Move = FALSE;
    /*
  if (!disableDefaultValues)
    AddDefault(0);
*/
}

void CPackerConfig::InitializeDefaultValues()
{
    AddDefault(0);
}

void CPackerConfig::AddDefault(int SalamVersion)
{
    // POZOR: do verze 6 stary 'Type' hodnoty: 0 ZIP, 1 external, 2 TAR, 3 PAK

    // default hodnoty
    int index, i;
    // nejprve interni, at je to serazene
    switch (SalamVersion)
    {
    case 0: // default config
    case 1: // v1.52 nemela pakovace
        if ((index = AddPacker()) == -1)
            return;
        SetPacker(index, 0, "ZIP (Plugin)", "zip", TRUE);
    case 2: // co pribylo po beta1
        if ((index = AddPacker()) == -1)
            return;
        SetPacker(index, 3, "PAK (Plugin)", "pak", TRUE);
    case 3:  // co pribylo po beta2
    case 4:  // beta 3 ale se starou konfiguraci (obsahuje $(SpawnName))
    case 5:; // co je noveho v beta4 ?
             //      if ((index = AddPacker()) == -1) return;
             //      SetPacker(index, 2, "TAR (Plugin)", "tgz", TRUE);
    }
    // ted externi
    switch (SalamVersion)
    {
        // format parametru
        //BOOL SetPacker(int index, int type, const char *title, const char *ext, BOOL old,
        //               BOOL supportLongNames = FALSE, BOOL supportMove = FALSE,
        //               const char *cmdExecCopy = NULL, const char *cmdArgsCopy = NULL,
        //               const char *cmdExecMove = NULL, const char *cmdArgsMove = NULL,
        //               BOOL needANSIListFile = FALSE);

    case 0: // default config
    case 1: // v1.52 nemela pakovace
        for (i = 0; i < 7; i++)
        {
            int idx = CustomOrder[i];
            if ((index = AddPacker()) == -1)
                return;
            SetPacker(index, 1, LoadStr(CustomPackers[idx].Title[0]), CustomPackers[idx].Ext, TRUE,
                      CustomPackers[idx].SupLN, TRUE,
                      CustomPackers[idx].Exe, CustomPackers[idx].CopyArgs[0],
                      CustomPackers[idx].Exe, CustomPackers[idx].MoveArgs[0],
                      CustomPackers[idx].Ansi);
            if (CustomPackers[idx].CopyArgs[1] != NULL)
            {
                if ((index = AddPacker()) == -1)
                    return;
                SetPacker(index, 1, LoadStr(CustomPackers[idx].Title[1]), CustomPackers[idx].Ext, TRUE,
                          CustomPackers[idx].SupLN, TRUE,
                          CustomPackers[idx].Exe, CustomPackers[idx].CopyArgs[1],
                          CustomPackers[idx].Exe, CustomPackers[idx].MoveArgs[1],
                          CustomPackers[idx].Ansi);
            }
        }
    case 2: // co pribylo po beta1
        for (i = 7; i < 12; i++)
        {
            int idx = CustomOrder[i];
            if ((index = AddPacker()) == -1)
                return;
            SetPacker(index, 1, LoadStr(CustomPackers[idx].Title[0]), CustomPackers[idx].Ext, TRUE,
                      CustomPackers[idx].SupLN, TRUE,
                      CustomPackers[idx].Exe, CustomPackers[idx].CopyArgs[0],
                      CustomPackers[idx].Exe, CustomPackers[idx].MoveArgs[0],
                      CustomPackers[idx].Ansi);
            if (CustomPackers[idx].CopyArgs[1] != NULL)
            {
                if ((index = AddPacker()) == -1)
                    return;
                SetPacker(index, 1, LoadStr(CustomPackers[idx].Title[1]), CustomPackers[idx].Ext, TRUE,
                          CustomPackers[idx].SupLN, TRUE,
                          CustomPackers[idx].Exe, CustomPackers[idx].CopyArgs[1],
                          CustomPackers[idx].Exe, CustomPackers[idx].MoveArgs[1],
                          CustomPackers[idx].Ansi);
            }
        }
    case 3: // co pribylo po beta2
    case 4: // beta 3 ale se starou konfiguraci (obsahuje $(SpawnName))
        // u drivejsich verzi muze existovat promenna $(SpawnName), ktera nyni uz neexistuje - musime ji vyhodit
        for (index = 0; index < GetPackersCount(); index++)
            if (GetPackerType(index) == 1)
            {
                const char* cmdC = GetPackerCmdExecCopy(index);
                const char* cmdM = GetPackerCmdExecMove(index);
                if (strncmp(cmdC, "$(SpawnName) ", 13) == 0 ||
                    GetPackerSupMove(index) && strncmp(cmdM, "$(SpawnName) ", 13) == 0)
                {
                    char* copyCmdBuf = (char*)malloc(strlen(cmdC) + 1);
                    if (strncmp(cmdC, "$(SpawnName) ", 13) == 0)
                        strcpy(copyCmdBuf, cmdC + 13);
                    else
                        strcpy(copyCmdBuf, cmdC);
                    char* copyArgBuf = (char*)malloc(strlen(GetPackerCmdArgsCopy(index)) + 1);
                    strcpy(copyArgBuf, GetPackerCmdArgsCopy(index));

                    char *moveCmdBuf, *moveArgBuf;
                    if (GetPackerSupMove(index))
                    {
                        moveCmdBuf = (char*)malloc(strlen(cmdM) + 1);
                        if (strncmp(cmdM, "$(SpawnName) ", 13) == 0)
                            strcpy(moveCmdBuf, cmdM + 13);
                        else
                            strcpy(moveCmdBuf, cmdM);
                        moveArgBuf = (char*)malloc(strlen(GetPackerCmdArgsMove(index)) + 1);
                        strcpy(moveArgBuf, GetPackerCmdArgsMove(index));
                    }
                    else
                        moveCmdBuf = moveArgBuf = NULL;

                    char* TitleBuf = (char*)malloc(strlen(GetPackerTitle(index)) + 1);
                    strcpy(TitleBuf, GetPackerTitle(index));
                    char* ExtBuf = (char*)malloc(strlen(GetPackerExt(index)) + 1);
                    strcpy(ExtBuf, GetPackerExt(index));

                    SetPacker(index, GetPackerType(index), TitleBuf, ExtBuf, TRUE,
                              GetPackerSupLongNames(index), GetPackerSupMove(index),
                              copyCmdBuf, copyArgBuf, moveCmdBuf, moveArgBuf,
                              GetPackerNeedANSIListFile(index));

                    free(copyCmdBuf);
                    free(copyArgBuf);
                    if (moveCmdBuf != NULL)
                        free(moveCmdBuf);
                    if (moveArgBuf != NULL)
                        free(moveArgBuf);
                    free(TitleBuf);
                    free(ExtBuf);
                }
            }
    case 5: // beta 3 ale bez taru
    case 6: // co je noveho v beta4 ?
    case 7: // 1.6b6 - kvuli spravne konverzi podporovanych funkci pluginu (viz CPlugins::Load)
    case 8:
        // prechod na pouzivani promennych misto primeho jmena exe souboru
        for (index = 0; index < GetPackersCount(); index++)
            // uvazujeme pouze externi pakovace
            if (((GetPackerOldType(index) && GetPackerType(index) == 1) ||
                 (!GetPackerOldType(index) && GetPackerType(index) == CUSTOMPACKER_EXTERNAL)) &&
                GetPackerCmdExecCopy(index) != NULL && GetPackerCmdExecMove(index) != NULL)
            {
                // vezmeme stare commandy
                char* cmdC = DupStr(GetPackerCmdExecCopy(index));
                char* cmdM = DupStr(GetPackerCmdExecMove(index));
                i = 0;
                BOOL found = FALSE;
                // a prohledame s nimi tabulku
                while (PackConversionTable[i].exe != NULL)
                {
                    // porovname s prvky tabulky
                    if (!strcmp(cmdC, PackConversionTable[i].exe) || !strcmp(cmdM, PackConversionTable[i].exe))
                    {
                        // a pokud najdeme, zmenime na promennou
                        if (!strcmp(cmdC, PackConversionTable[i].exe))
                        {
                            free(cmdC);
                            // a jeden hnusnej hack kvuli raru
                            if (i == 2)
                                // je-li to rar, neumime poznat, jestli 16bit nebo 32bit primo, ale jen podle podpory dlouhych jmen
                                if (GetPackerSupLongNames(index))
                                    cmdC = DupStr(PackConversionTable[i].variable);
                                else
                                    cmdC = DupStr("$(Rar16bitExecutable)");
                            else
                                // pro ostatni je to jednoduche
                                cmdC = DupStr(PackConversionTable[i].variable);
                        }
                        if (!strcmp(cmdM, PackConversionTable[i].exe))
                        {
                            free(cmdM);
                            // a jeden hnusnej hack kvuli raru
                            if (i == 2)
                                // je-li to rar, neumime poznat, jestli 16bit nebo 32bit primo, ale jen podle podpory dlouhych jmen
                                if (GetPackerSupLongNames(index))
                                    cmdM = DupStr(PackConversionTable[i].variable);
                                else
                                    cmdM = DupStr("$(Rar16bitExecutable)");
                            else
                                // pro ostatni je to jednoduche
                                cmdM = DupStr(PackConversionTable[i].variable);
                        }
                        found = TRUE;
                    }
                    i++;
                }
                // stringy se musi nekam nakopirovat, jinak je smazem driv, nez je pouzijem
                char* title = DupStr(GetPackerTitle(index));
                char* ext = DupStr(GetPackerExt(index));
                char* argsC = DupStr(GetPackerCmdArgsCopy(index));
                char* argsM = DupStr(GetPackerCmdArgsMove(index));

                if (found)
                    SetPacker(index, GetPackerType(index), title, ext, GetPackerOldType(index),
                              GetPackerSupLongNames(index), GetPackerSupMove(index),
                              cmdC, argsC, cmdM, argsM, GetPackerNeedANSIListFile(index));
                free(argsC);
                free(argsM);
                free(title);
                free(ext);
                free(cmdC);
                free(cmdM);
            }
    case 9: // 1.6b6 - kvuli prechodu ze jmena exe na promennou u custom packers
        // nahozeni ANSI file-listu u ACE32 a PKZIP25
        for (index = 0; index < GetPackersCount(); index++)
        {
            if ((GetPackerOldType(index) && GetPackerType(index) == 1) ||
                (!GetPackerOldType(index) && GetPackerType(index) == CUSTOMPACKER_EXTERNAL))
            {
                const char* s = GetPackerCmdExecCopy(index);
                if (s != NULL && (strcmp(s, "$(Zip32bitExecutable)") == 0 ||
                                  strcmp(s, "$(Ace32bitExecutable)") == 0))
                {
                    Packers[index]->NeedANSIListFile = TRUE;
                }
            }
        }
        // zmenime "XXX (Internal)" na "XXX (Plugin)"
        // strasne to vyprasime, protoze menime na kratsi text -> vystacime proste s prepisem
        for (index = 0; index < GetPackersCount(); index++)
        {
            if ((GetPackerOldType(index) && GetPackerType(index) != 1) ||
                (!GetPackerOldType(index) && GetPackerType(index) != CUSTOMPACKER_EXTERNAL))
            { // bereme jen plug-iny (ne externi packery)
                char* s = Packers[index]->Title;
                char* f;
                if (s != NULL && (f = strstr(s, "(Internal)")) != NULL) // obsahuje (Internal)
                {
                    if (strlen(f) == 10)
                        strcpy(f, "(Plugin)"); // (Internal) je na konci stringu
                }
            }
        }
    case 10: // 1.6b6 - kvuli prejmenovani "XXX (Internal)" na "XXX (Plugin)" v Pack a Unpack dialozich
             //         a kvuli nastaveni ANSI verze "list of files" souboru i pro (un)packery ACE32 a PKZIP25
    case 11: // 1.6b7 - pribyl plugin CheckVer - zajistime jeho automatickou do-instalaci
    case 12: // 2.0 - auto-vypnuti salopen.exe + pribyl plugin PEViewer - zajistime jeho automatickou do-instalaci
    {
        // u LHA pribylo "-m", musime ho doplnit a pokud archivers-auto-config pridal podruhe LHA
        // z duvodu, ze nesedelo "-m", tak ten novy zaznam odstranime
        const char* newLHACopyArgs = "a -m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)";
        const char* newLHAMoveArgs = "m -m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)";
        BOOL canDelLHA = FALSE;
        for (index = 0; index < GetPackersCount(); index++)
        {
            if ((GetPackerOldType(index) && GetPackerType(index) == 1) ||
                (!GetPackerOldType(index) && GetPackerType(index) == CUSTOMPACKER_EXTERNAL))
            {
                const char* copyEXE = GetPackerCmdExecCopy(index);
                const char* copyArgs = GetPackerCmdArgsCopy(index);
                const char* moveEXE = GetPackerCmdExecMove(index);
                const char* moveArgs = GetPackerCmdArgsMove(index);

                // test jestli jde o LHA custom packer
                if (copyEXE != NULL && strcmp(copyEXE, "$(Lha16bitExecutable)") == 0 &&
                    moveEXE != NULL && strcmp(moveEXE, "$(Lha16bitExecutable)") == 0)
                {
                    // test jestli jde o stary zaznam LHA custom packeru
                    if (copyArgs != NULL &&
                        strcmp(copyArgs, "a -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)") == 0 &&
                        moveArgs != NULL &&
                        strcmp(moveArgs, "m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)") == 0)
                    {
                        if (canDelLHA)
                        {
                            // zaznam smazeme, je zbytecny (funkcni zaznam pro LHA jiz existuje)
                            DeletePacker(index);
                            index--;
                        }
                        else
                        {
                            canDelLHA = TRUE;
                            // konverze na nove argumenty (pridane "-m")
                            char* s = DupStr(newLHACopyArgs);
                            if (s != NULL)
                            {
                                free(Packers[index]->CmdArgsCopy); // nemuze byt NULL (jsou zde stare argumenty)
                                Packers[index]->CmdArgsCopy = s;
                            }
                            s = DupStr(newLHAMoveArgs);
                            if (s != NULL)
                            {
                                free(Packers[index]->CmdArgsMove); // nemuze byt NULL (jsou zde stare argumenty)
                                Packers[index]->CmdArgsMove = s;
                            }
                        }
                    }
                    else
                    {
                        // test jestli jde o archivers-auto-configem pridany zaznam LHA custom packeru
                        if (copyArgs != NULL && strcmp(copyArgs, newLHACopyArgs) == 0 &&
                            moveArgs != NULL && strcmp(moveArgs, newLHAMoveArgs) == 0)
                        {
                            if (canDelLHA)
                            {
                                // pridany zaznam smazeme, je zbytecny (funkcni zaznam pro LHA jiz existuje)
                                DeletePacker(index);
                                index--;
                            }
                            else
                                canDelLHA = TRUE;
                        }
                    }
                }
            }
        }
    }
    case 13: // 2.5b1 - dopsana chybejici konverze konfigurace u custom-packeru - promitnuti zmeny u LHA
    case 14: // 2.5b1 - Nove Advanced Options ve Find dialogu. Prechod na CFilterCriteria. Konverze inverzni masky u filtru.
    case 15: // 2.5b2 - novejsi verze, at se naloadi pluginy (upgradnou zaznamy v registry)
    case 16: // 2.5b2 - pridano barveni Encrypted souboru a adresaru (pridava se pri loadu konfigu + je v defaultnim konfigu)
    case 17: // 2.5b2 - pridana maska *.xml do nastaveni interniho vieweru - "force text mode"
    case 18: // 2.5b3 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b2
    case 19: // 2.5b4 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b3
    case 20: // 2.5b5 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b4
    case 21: // 2.5b6 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b5(a)
    case 22: // 2.5b6 - filtry v panelech -- sjednoceni na jednu historii
    case 23: // 2.5b6 - novy pohled v panelu (Tiles)
    case 24: // 2.5b7 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b6
    case 25: // 2.5b7 - plugins: show in plugin bar -> prenos promenne do CPluginData
    case 26: // 2.5b8 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b7
    case 27: // 2.5b9 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b8
    case 28: // 2.5b9 - nove barevne schema dle stareho DOS Navigator -> konverze 'scheme'
    case 29: // 2.5b10 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b9
    case 30: // 2.5b11 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b10
    case 31: // 2.5b11 - zavedli jsme Floppy sekci v konfiguraci Drives a potrebujeme pro Removable forcnout cteni ikon
    case 32: // 2.5b11 - Find: "Local Settings\\Temporary Internet Files" je implicitne prohledavane
    case 33: // 2.5b12 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b11
    {
        // u PKZIP25 pribylo "-nozipextension", musime ho doplnit
        const char* newPKZIP25CopyArgs = "-add -nozipextension -path -attr \"$(ArchiveFullName)\" @\"$(ListFullName)\"";
        const char* newPKZIP25MoveArgs = "-add -nozipextension -move -path -attr \"$(ArchiveFullName)\" @\"$(ListFullName)\"";
        for (index = 0; index < GetPackersCount(); index++)
        {
            if ((GetPackerOldType(index) && GetPackerType(index) == 1) ||
                (!GetPackerOldType(index) && GetPackerType(index) == CUSTOMPACKER_EXTERNAL))
            {
                const char* copyEXE = GetPackerCmdExecCopy(index);
                const char* copyArgs = GetPackerCmdArgsCopy(index);
                const char* moveEXE = GetPackerCmdExecMove(index);
                const char* moveArgs = GetPackerCmdArgsMove(index);

                // test jestli jde o PKZIP25 custom packer
                if (copyEXE != NULL && strcmp(copyEXE, "$(Zip32bitExecutable)") == 0 &&
                    moveEXE != NULL && strcmp(moveEXE, "$(Zip32bitExecutable)") == 0)
                {
                    // test jestli jde o stary zaznam PKZIP25 custom packeru
                    if (copyArgs != NULL &&
                        strcmp(copyArgs, "-add -path -attr \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0 &&
                        moveArgs != NULL &&
                        strcmp(moveArgs, "-add -move -path -attr \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0)
                    {
                        // konverze na nove argumenty (pridane "-nozipextension")
                        char* s = DupStr(newPKZIP25CopyArgs);
                        if (s != NULL)
                        {
                            free(Packers[index]->CmdArgsCopy); // nemuze byt NULL (jsou zde stare argumenty)
                            Packers[index]->CmdArgsCopy = s;
                        }
                        s = DupStr(newPKZIP25MoveArgs);
                        if (s != NULL)
                        {
                            free(Packers[index]->CmdArgsMove); // nemuze byt NULL (jsou zde stare argumenty)
                            Packers[index]->CmdArgsMove = s;
                        }
                    }
                }
            }
        }
    }
        // case 34:   // 2.5b12 - uprava externiho packeru/unpackeru PKZIP25 (externi Win32 verze)

    default:
        break;
    }
    if (SalamVersion > 1 && SalamVersion < 81)
    {
        // od RAR 5.0 jsou filelists defaultne ANSI misto OEM, proto musime OEM vnutit pomoci switche
        const char* newRAR5CopyArgs = "a -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"";
        const char* newRAR5MoveArgs = "m -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"";
        const char* newRAR5CopyVolArgs = "a -scol -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\"";
        const char* newRAR5MoveVolArgs = "m -scol -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\"";
        for (index = 0; index < GetPackersCount(); index++)
        {
            if ((GetPackerOldType(index) && GetPackerType(index) == 1) ||
                (!GetPackerOldType(index) && GetPackerType(index) == CUSTOMPACKER_EXTERNAL))
            {
                const char* copyEXE = GetPackerCmdExecCopy(index);
                const char* copyArgs = GetPackerCmdArgsCopy(index);
                const char* moveEXE = GetPackerCmdExecMove(index);
                const char* moveArgs = GetPackerCmdArgsMove(index);

                // test jestli jde o RAR Win32 custom packer
                if (copyEXE != NULL && strcmp(copyEXE, "$(Rar32bitExecutable)") == 0 &&
                    moveEXE != NULL && strcmp(moveEXE, "$(Rar32bitExecutable)") == 0)
                {
                    // test jestli jde o stary zaznam RAR Win32 custom packeru
                    if (copyArgs != NULL &&
                        strcmp(copyArgs, "a \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0 &&
                        moveArgs != NULL &&
                        strcmp(moveArgs, "m \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0)
                    {
                        // konverze na nove argumenty (pridane "-scol")
                        free(Packers[index]->CmdArgsCopy); // nemuze byt NULL (jsou zde stare argumenty)
                        Packers[index]->CmdArgsCopy = DupStr(newRAR5CopyArgs);
                        free(Packers[index]->CmdArgsMove); // nemuze byt NULL (jsou zde stare argumenty)
                        Packers[index]->CmdArgsMove = DupStr(newRAR5MoveArgs);
                    }
                    if (copyArgs != NULL &&
                        strcmp(copyArgs, "a -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0 &&
                        moveArgs != NULL &&
                        strcmp(moveArgs, "m -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0)
                    {
                        // konverze na nove argumenty (pridane "-scol")
                        free(Packers[index]->CmdArgsCopy); // nemuze byt NULL (jsou zde stare argumenty)
                        Packers[index]->CmdArgsCopy = DupStr(newRAR5CopyVolArgs);
                        free(Packers[index]->CmdArgsMove); // nemuze byt NULL (jsou zde stare argumenty)
                        Packers[index]->CmdArgsMove = DupStr(newRAR5MoveVolArgs);
                    }
                }
            }
        }
    }
}

BOOL CPackerConfig::Load(CPackerConfig& src)
{
    CALL_STACK_MESSAGE1("CPackerConfig::Load()");
    Move = src.Move;
    PreferedPacker = src.PreferedPacker;

    DeleteAllPackers();
    int i;
    for (i = 0; i < src.GetPackersCount(); i++)
    {
        int index = AddPacker();
        if (index == -1)
            return FALSE;
        if (!SetPacker(index, src.GetPackerType(i), src.GetPackerTitle(i), src.GetPackerExt(i), FALSE,
                       src.GetPackerSupLongNames(i), src.GetPackerSupMove(i),
                       src.GetPackerCmdExecCopy(i), src.GetPackerCmdArgsCopy(i),
                       src.GetPackerCmdExecMove(i), src.GetPackerCmdArgsMove(i),
                       src.GetPackerNeedANSIListFile(i)))
            return FALSE;
    }
    return TRUE;
}

int CPackerConfig::AddPacker(BOOL toFirstIndex)
{
    CALL_STACK_MESSAGE2("CPackerConfig::AddPacker(%d)", toFirstIndex);
    CPackerConfigData* data = new CPackerConfigData;
    if (data == NULL)
        return -1;
    int index;
    if (toFirstIndex)
    {
        Packers.Insert(0, data);
        index = 0;
        if (PreferedPacker != -1)
            PreferedPacker++;
    }
    else
        index = Packers.Add(data);
    if (!Packers.IsGood())
    {
        delete data;
        Packers.ResetState();
        return -1;
    }
    return index;
}

/*
BOOL
CPackerConfig::SwapPackers(int index1, int index2)
{
  BYTE buff[sizeof(CPackerConfigData)];
  memcpy(buff, Packers[index1], sizeof(CPackerConfigData));
  memcpy(Packers[index1], Packers[index2], sizeof(CPackerConfigData));
  memcpy(Packers[index2], buff, sizeof(CPackerConfigData));
  return TRUE;
}
*/

BOOL CPackerConfig::MovePacker(int srcIndex, int dstIndex)
{
    BYTE buff[sizeof(CPackerConfigData)];
    memcpy(buff, Packers[srcIndex], sizeof(CPackerConfigData));
    if (srcIndex < dstIndex)
    {
        int i;
        for (i = srcIndex; i < dstIndex; i++)
            memcpy(Packers[i], Packers[i + 1], sizeof(CPackerConfigData));
    }
    else
    {
        int i;
        for (i = srcIndex; i > dstIndex; i--)
            memcpy(Packers[i], Packers[i - 1], sizeof(CPackerConfigData));
    }
    memcpy(Packers[dstIndex], buff, sizeof(CPackerConfigData));
    return TRUE;
}

void CPackerConfig::DeletePacker(int index)
{
    if (PreferedPacker >= 0 && PreferedPacker < Packers.Count)
    {
        if (index < PreferedPacker)
            PreferedPacker--; // uprava indexu, aby se reprezentoval stejnou polozku
        else
        {
            if (index == PreferedPacker)
                PreferedPacker = -1; // prisli jsme o vybranou polozku
        }
    }
    Packers.Delete(index);
}

BOOL CPackerConfig::SetPacker(int index, int type, const char* title, const char* ext, BOOL old,
                              BOOL supportLongNames, BOOL supportMove,
                              const char* cmdExecCopy, const char* cmdArgsCopy,
                              const char* cmdExecMove, const char* cmdArgsMove,
                              BOOL needANSIListFile)
{
    CALL_STACK_MESSAGE13("CPackerConfig::SetPacker(%d, %d, %s, %s, %d, %d, %d, %s, %s, %s, %s, %d)",
                         index, type, title, ext, old, supportLongNames, supportMove,
                         cmdExecCopy, cmdArgsCopy, cmdExecMove, cmdArgsMove, needANSIListFile);
    CPackerConfigData* data = Packers[index];
    data->Destroy();
    data->Type = type;
    data->OldType = old;
    data->Title = DupStr(title);
    data->Ext = DupStr(ext);
    if (old && data->Type == 1 ||
        !old && data->Type == CUSTOMPACKER_EXTERNAL)
    {
        data->CmdExecCopy = DupStr(cmdExecCopy);
        data->CmdArgsCopy = DupStr(cmdArgsCopy);
        data->SupportMove = supportMove;

        if (data->SupportMove)
        {
            data->CmdExecMove = DupStr(cmdExecMove);
            data->CmdArgsMove = DupStr(cmdArgsMove);
        }
        data->SupportLongNames = supportLongNames;
        data->NeedANSIListFile = needANSIListFile;
    }

    if (data->IsValid())
    {
        if (PreferedPacker == -1)
            PreferedPacker = Packers.Count - 1;
        return TRUE;
    }
    else
    {
        Packers.Delete(index);
        return FALSE;
    }
}

BOOL CPackerConfig::SetPackerTitle(int index, const char* title)
{
    CPackerConfigData* data = Packers[index];
    if (data->Title != NULL)
        free(data->Title);
    data->Title = DupStr(title);
    return data->Title != NULL;
}

BOOL CPackerConfig::ExecutePacker(CFilesWindow* panel, const char* zipFile, BOOL move,
                                  const char* sourcePath, SalEnumSelection2 next, void* param)
{
    CALL_STACK_MESSAGE4("CPackerConfig::ExecutePacker(, %s, %d, %s, , ,)",
                        zipFile, move, sourcePath);
    if (PreferedPacker >= 0 && PreferedPacker < Packers.Count)
    {
        CPackerConfigData* data = Packers[PreferedPacker];
        if (data->Type == CUSTOMPACKER_EXTERNAL)
        {
            char* command;
            if (move)
            {
                if (!data->SupportMove)
                {
                    TRACE_E("Using \"Move to archive\" with packer, which does not support it !!!");
                    return FALSE;
                }
                command = (char*)malloc(strlen(data->CmdExecMove) +
                                        strlen(data->CmdArgsMove) + 2);
                if (command == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                    return FALSE;
                }
                sprintf(command, "%s %s", data->CmdExecMove, data->CmdArgsMove);
            }
            else
            {
                command = (char*)malloc(strlen(data->CmdExecCopy) +
                                        strlen(data->CmdArgsCopy) + 2);
                if (command == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                    return FALSE;
                }
                sprintf(command, "%s %s", data->CmdExecCopy, data->CmdArgsCopy);
            }
            BOOL ret = PackUniversalCompress(NULL, command, NULL, sourcePath, FALSE,
                                             data->SupportLongNames, zipFile, sourcePath, NULL,
                                             next, param, data->NeedANSIListFile);
            free(command);
            return ret;
        }
        else
        {
            CPluginData* plugin = Plugins.Get(-data->Type - 1);
            if (plugin != NULL && plugin->SupportCustomPack)
            {
                return plugin->PackToArchive(panel, zipFile, "", move, sourcePath, next, param);
            }
            else
                TRACE_E("Unexpected situation in CPackerConfig::ExecutePacker().");
        }
    }
    return FALSE;
}

BOOL CPackerConfig::Save(int index, HKEY hKey)
{
    DWORD d;
    BOOL ret = TRUE;
    int type = GetPackerType(index);
    d = type;
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_CPU_TYPE, REG_DWORD, &d, sizeof(d));
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_CPU_TITLE, REG_SZ, GetPackerTitle(index), -1);
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_CPU_EXT, REG_SZ, GetPackerExt(index), -1);
    if (ret && type == CUSTOMPACKER_EXTERNAL)
    {
        d = GetPackerSupLongNames(index);
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_CPU_SUPLONG, REG_DWORD, &d, sizeof(d));
        d = GetPackerNeedANSIListFile(index);
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_CPU_ANSILIST, REG_DWORD, &d, sizeof(d));
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_CP_EXECCOPY, REG_SZ, GetPackerCmdExecCopy(index), -1);
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_CP_ARGSCOPY, REG_SZ, GetPackerCmdArgsCopy(index), -1);
        d = GetPackerSupMove(index);
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_CP_SUPMOVE, REG_DWORD, &d, sizeof(d));
        if (ret && d == TRUE)
        {
            if (ret)
                ret &= SetValue(hKey, SALAMANDER_CP_EXECMOVE, REG_SZ, GetPackerCmdExecMove(index), -1);
            if (ret)
                ret &= SetValue(hKey, SALAMANDER_CP_ARGSMOVE, REG_SZ, GetPackerCmdArgsMove(index), -1);
        }
    }
    return ret;
}

BOOL CPackerConfig::Load(HKEY hKey)
{
    int max = MAX_PATH + 2;

    char title[MAX_PATH + 2];
    title[0] = 0;
    char ext[MAX_PATH + 2];
    DWORD type;
    DWORD suplong = FALSE;
    DWORD needANSI = FALSE;
    char execcopy[MAX_PATH + 2];
    execcopy[0] = 0;
    char argscopy[MAX_PATH + 2];
    argscopy[0] = 0;
    DWORD supmove = FALSE;
    char execmove[MAX_PATH + 2];
    execmove[0] = 0;
    char argsmove[MAX_PATH + 2];
    argsmove[0] = 0;

    BOOL ret = TRUE;
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_CPU_TYPE, REG_DWORD, &type, sizeof(DWORD));
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_CPU_TITLE, REG_SZ, title, max);
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_CPU_EXT, REG_SZ, ext, max);
    if (ret && (Configuration.ConfigVersion < 6 && type == 1 ||
                Configuration.ConfigVersion >= 6 && type == CUSTOMPACKER_EXTERNAL))
    {
        if (ret)
            ret &= GetValue(hKey, SALAMANDER_CPU_SUPLONG, REG_DWORD, &suplong, sizeof(DWORD));
        if (ret)
        {
            if (!GetValue(hKey, SALAMANDER_CPU_ANSILIST, REG_DWORD, &needANSI, sizeof(DWORD)))
                needANSI = FALSE; // ve starsich verzich nebylo, predpokladalo se FALSE
        }

        if (ret)
            ret &= GetValue(hKey, SALAMANDER_CP_EXECCOPY, REG_SZ, execcopy, max);
        if (ret)
            ret &= GetValue(hKey, SALAMANDER_CP_ARGSCOPY, REG_SZ, argscopy, max);
        if (ret)
            ret &= GetValue(hKey, SALAMANDER_CP_SUPMOVE, REG_DWORD, &supmove, sizeof(DWORD));
        if (ret && supmove == TRUE)
        {
            if (ret)
                ret &= GetValue(hKey, SALAMANDER_CP_EXECMOVE, REG_SZ, execmove, max);
            if (ret)
                ret &= GetValue(hKey, SALAMANDER_CP_ARGSMOVE, REG_SZ, argsmove, max);
        }
    }

    if (ret)
    {
        int index;
        if ((index = AddPacker()) == -1)
            return FALSE;
        if (Configuration.ConfigVersion < 44) // prevod pripony na lowercase
        {
            char extAux[MAX_PATH + 2];
            lstrcpyn(extAux, ext, MAX_PATH + 2);
            StrICpy(ext, extAux);
        }
        ret &= SetPacker(index, (int)type, title, ext, Configuration.ConfigVersion < 6,
                         (BOOL)suplong, BOOL(supmove),
                         execcopy, argscopy,
                         execmove, argsmove, needANSI);
    }

    return ret;
}

//
// ****************************************************************************
// CUnpackerConfig
//

CUnpackerConfig::CUnpackerConfig(/*BOOL disableDefaultValues*/)
    : Unpackers(20, 10)
{
    PreferedUnpacker = -1;
    /*
  if (!disableDefaultValues)
    AddDefault(0);
*/
}

void CUnpackerConfig::InitializeDefaultValues()
{
    AddDefault(0);
}

void CUnpackerConfig::AddDefault(int SalamVersion)
{
    // POZOR: do verze 6 stary 'Type' hodnoty: 0 ZIP, 1 external, 2 TAR, 3 PAK

    // konvert nactenych hodnot z 1.6b1 - pripony nebyly masky ("EXT" -> "*.EXT")
    if (SalamVersion == 2)
    {
        int i;
        for (i = 0; i < Unpackers.Count; i++)
        {
            int count = 1;
            char* ptr = Unpackers[i]->Ext;
            if (ptr == NULL || *ptr == '\0')
                continue;
            while (*ptr != '\0')
            {
                if (*ptr++ == ';')
                    count++;
            }
            char* nptr = (char*)malloc(strlen(Unpackers[i]->Ext) + count * 2 + 1);
            ptr = Unpackers[i]->Ext;
            int pos = 0;
            nptr[pos++] = '*';
            nptr[pos++] = '.';
            while (*ptr != '\0')
            {
                nptr[pos++] = *ptr;
                if (*ptr++ == ';')
                {
                    nptr[pos++] = '*';
                    nptr[pos++] = '.';
                }
            }
            nptr[pos] = '\0';
            free(Unpackers[i]->Ext);
            Unpackers[i]->Ext = nptr;
        }
    }

    // default hodnoty
    int index, i;
    // nejprve interni, at je to serazene
    switch (SalamVersion)
    {
    case 0: // default config
    case 1: // v1.52 nemela pakovace
        if ((index = AddUnpacker()) == -1)
            return;
        SetUnpacker(index, 0, "ZIP (Plugin)", "*.zip", TRUE);
    case 2: // co pribylo po beta1
        // hack na pridani pk3 pripony k zipu
        for (i = 0; i < Unpackers.Count; i++)
            if (!strnicmp(Unpackers[i]->Ext, "*.zip", 5))
            {
                char* ptr = (char*)malloc(strlen(Unpackers[i]->Ext) + 13);
                if (ptr != NULL)
                {
                    strcpy(ptr, Unpackers[i]->Ext);
                    strcat(ptr, ";*.pk3;*.jar");
                    free(Unpackers[i]->Ext);
                    Unpackers[i]->Ext = ptr;
                }
                break;
            }
        // a nove formaty
        if ((index = AddUnpacker()) == -1)
            return;
        SetUnpacker(index, 3, "PAK (Plugin)", "*.pak", TRUE);
    case 3: // co pribylo po beta2
    case 4: // beta 3 ale bez $(SpawnName) promenne
    case 5: // co je noveho v beta4 ?
        if ((index = AddUnpacker()) == -1)
            return;
        SetUnpacker(index, 2, "TAR (Plugin)", "*.TAR;*.TGZ;*.TBZ;*.TAZ;"
                                              "*.TAR.GZ;*.TAR.BZ;*.TAR.BZ2;*.TAR.Z;"
                                              "*_TAR.GZ;*_TAR.BZ;*_TAR.BZ2;*_TAR.Z;"
                                              "*_TAR_GZ;*_TAR_BZ;*_TAR_BZ2;*_TAR_Z;"
                                              "*.TAR_GZ;*.TAR_BZ;*.TAR_BZ2;*.TAR_Z;"
                                              "*.GZ;*.BZ;*.BZ2;*.Z;"
                                              "*.RPM;*.CPIO",
                    TRUE);
    }
    // ted externi
    switch (SalamVersion)
    {
        // parametry
        //BOOL SetUnpacker(int index, int type, const char *title, const char *ext, BOOL old,
        //                 BOOL supportLongNames = FALSE,
        //                 const char *cmdExecExtract = NULL, const char *cmdArgsExtract = NULL,
        //                 BOOL needANSIListFile = FALSE);

    case 0: // default config
    case 1: // v1.52 nemela pakovace
        for (i = 0; i < 7; i++)
        {
            int idx = CustomOrder[i];
            if ((index = AddUnpacker()) == -1)
                return;
            SetUnpacker(index, 1, LoadStr(CustomUnpackers[idx].Title), CustomUnpackers[idx].Ext, TRUE,
                        CustomUnpackers[idx].SupLN, CustomUnpackers[idx].Exe,
                        CustomUnpackers[idx].Args, CustomUnpackers[idx].Ansi);
        }
    case 2: // co pribylo po beta1
        for (i = 7; i < 12; i++)
        {
            int idx = CustomOrder[i];
            if ((index = AddUnpacker()) == -1)
                return;
            SetUnpacker(index, 1, LoadStr(CustomUnpackers[idx].Title), CustomUnpackers[idx].Ext, TRUE,
                        CustomUnpackers[idx].SupLN, CustomUnpackers[idx].Exe,
                        CustomUnpackers[idx].Args, CustomUnpackers[idx].Ansi);
        }
    case 3: // co pribylo po beta2
    case 4: // beta 3 ale bez $(SpawnName) promenne
        // u drivejsich verzi muze existovat promenna $(SpawnName), ktera nyni uz neexistuje - musime ji vyhodit
        for (index = 0; index < GetUnpackersCount(); index++)
            if (GetUnpackerType(index) == 1)
            {
                const char* cmd = GetUnpackerCmdExecExtract(index);
                if (strncmp(cmd, "$(SpawnName) ", 13) == 0)
                {
                    char* extractCmdBuf = (char*)malloc(strlen(cmd) + 1 - 13);
                    strcpy(extractCmdBuf, cmd + 13);
                    char* extractArgBuf = (char*)malloc(strlen(GetUnpackerCmdArgsExtract(index)) + 1);
                    strcpy(extractArgBuf, GetUnpackerCmdArgsExtract(index));
                    char* TitleBuf = (char*)malloc(strlen(GetUnpackerTitle(index)) + 1);
                    strcpy(TitleBuf, GetUnpackerTitle(index));
                    char* ExtBuf = (char*)malloc(strlen(GetUnpackerExt(index)) + 1);
                    strcpy(ExtBuf, GetUnpackerExt(index));

                    SetUnpacker(index, GetUnpackerType(index), TitleBuf, ExtBuf, TRUE,
                                GetUnpackerSupLongNames(index), extractCmdBuf, extractArgBuf,
                                GetUnpackerNeedANSIListFile(index));
                    free(extractCmdBuf);
                    free(extractArgBuf);
                    free(TitleBuf);
                    free(ExtBuf);
                }
            }
    case 5: // beta 3 ale bez taru
    case 6: // co je noveho v beta4 ?
    case 7: // 1.6b6 - kvuli spravne konverzi podporovanych funkci pluginu (viz CPlugins::Load)
    case 8:
        // prechod na pouzivani promennych misto primeho jmena exe souboru
        for (index = 0; index < GetUnpackersCount(); index++)
            // uvazujeme pouze externi pakovace
            if (((GetUnpackerOldType(index) && GetUnpackerType(index) == 1) ||
                 (!GetUnpackerOldType(index) && GetUnpackerType(index) == CUSTOMUNPACKER_EXTERNAL)) &&
                GetUnpackerCmdExecExtract(index) != NULL)
            {
                // vezmeme stare commandy
                char* cmd = DupStr(GetUnpackerCmdExecExtract(index));
                i = 0;
                BOOL found = FALSE;
                // a prohledame s nimi tabulku
                while (PackConversionTable[i].exe != NULL)
                {
                    // porovname s prvky tabulky
                    if (!strcmp(cmd, PackConversionTable[i].exe))
                    {
                        free(cmd);
                        // a jeden hnusnej hack kvuli raru
                        if (i == 2)
                            // je-li to rar, neumime poznat, jestli 16bit nebo 32bit primo, ale jen podle podpory dlouhych jmen
                            if (GetUnpackerSupLongNames(index))
                                cmd = DupStr(PackConversionTable[i].variable);
                            else
                                cmd = DupStr("$(Rar16bitExecutable)");
                        else
                            // pro ostatni je to jednoduche
                            cmd = DupStr(PackConversionTable[i].variable);
                        found = TRUE;
                    }
                    i++;
                }
                // stringy se musi nekam nakopirovat, jinak je smazem driv, nez je pouzijem
                char* title = DupStr(GetUnpackerTitle(index));
                char* ext = DupStr(GetUnpackerExt(index));
                char* args = DupStr(GetUnpackerCmdArgsExtract(index));

                if (found)
                    SetUnpacker(index, GetUnpackerType(index), title, ext, GetUnpackerOldType(index),
                                GetUnpackerSupLongNames(index), cmd, args,
                                GetUnpackerNeedANSIListFile(index));
                free(args);
                free(title);
                free(ext);
                free(cmd);
            }
    case 9: // 1.6b6 - kvuli prechodu ze jmena exe na promennou u custom packers
        // nahozeni ANSI file-listu u ACE32 a PKZIP25
        for (index = 0; index < GetUnpackersCount(); index++)
        {
            if ((GetUnpackerOldType(index) && GetUnpackerType(index) == 1) ||
                (!GetUnpackerOldType(index) && GetUnpackerType(index) == CUSTOMUNPACKER_EXTERNAL))
            {
                const char* s = GetUnpackerCmdExecExtract(index);
                if (s != NULL && (strcmp(s, "$(Zip32bitExecutable)") == 0 ||
                                  strcmp(s, "$(Ace32bitExecutable)") == 0))
                {
                    Unpackers[index]->NeedANSIListFile = TRUE;
                }
            }
        }
        // zmenime "XXX (Internal)" na "XXX (Plugin)"
        // strasne to vyprasime, protoze menime na kratsi text -> vystacime proste s prepisem
        for (index = 0; index < GetUnpackersCount(); index++)
        {
            if ((GetUnpackerOldType(index) && GetUnpackerType(index) != 1) ||
                (!GetUnpackerOldType(index) && GetUnpackerType(index) != CUSTOMUNPACKER_EXTERNAL))
            { // bereme jen plug-iny (ne externi unpackery)
                char* s = Unpackers[index]->Title;
                char* f;
                if (s != NULL && (f = strstr(s, "(Internal)")) != NULL) // obsahuje (Internal)
                {
                    if (strlen(f) == 10)
                        strcpy(f, "(Plugin)"); // (Internal) je na konci stringu
                }
            }
        }
    case 10: // 1.6b6 - kvuli prejmenovani "XXX (Internal)" na "XXX (Plugin)" v Pack a Unpack dialozich
    case 11: // 1.6b7 - pribyl plugin CheckVer - zajistime jeho automatickou do-instalaci
    case 12: // 2.0 - auto-vypnuti salopen.exe + pribyl plugin PEViewer - zajistime jeho automatickou do-instalaci
    case 13: // 2.5b1 - dopsana chybejici konverze konfigurace u custom-packeru - promitnuti zmeny u LHA
    case 14: // 2.5b1 - Nove Advanced Options ve Find dialogu. Prechod na CFilterCriteria. Konverze inverzni masky u filtru.
    case 15: // 2.5b2 - novejsi verze, at se naloadi pluginy (upgradnou zaznamy v registry)
    case 16: // 2.5b2 - pridano barveni Encrypted souboru a adresaru (pridava se pri loadu konfigu + je v defaultnim konfigu)
    case 17: // 2.5b2 - pridana maska *.xml do nastaveni interniho vieweru - "force text mode"
    case 18: // 2.5b3 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b2
    case 19: // 2.5b4 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b3
    case 20: // 2.5b5 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b4
    case 21: // 2.5b6 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b5(a)
    case 22: // 2.5b6 - filtry v panelech -- sjednoceni na jednu historii
    case 23: // 2.5b6 - novy pohled v panelu (Tiles)
    case 24: // 2.5b7 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b6
    case 25: // 2.5b7 - plugins: show in plugin bar -> prenos promenne do CPluginData
    case 26: // 2.5b8 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b7
    case 27: // 2.5b9 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b8
    case 28: // 2.5b9 - nove barevne schema dle stareho DOS Navigator -> konverze 'scheme'
    case 29: // 2.5b10 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b9
    case 30: // 2.5b11 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b10
    case 31: // 2.5b11 - zavedli jsme Floppy sekci v konfiguraci Drives a potrebujeme pro Removable forcnout cteni ikon
    case 32: // 2.5b11 - Find: "Local Settings\\Temporary Internet Files" je implicitne prohledavane
    case 33: // 2.5b12 - zatim jen kvuli prenosu konfigurace pluginu z verze 2.5b11
    {
        // u PKZIP25 pribylo "-nozipextension -directories" a "*.pk3;*.jar", musime to doplnit
        const char* newPKZIP25Args = "-ext -nozipextension -directories -path \"$(ArchiveFullName)\" @\"$(ListFullName)\"";
        const char* newPKZIP25Ext = "*.zip;*.pk3;*.jar";
        for (index = 0; index < GetUnpackersCount(); index++)
        {
            if ((GetUnpackerOldType(index) && GetUnpackerType(index) == 1) ||
                (!GetUnpackerOldType(index) && GetUnpackerType(index) == CUSTOMPACKER_EXTERNAL))
            {
                const char* extrEXE = GetUnpackerCmdExecExtract(index);
                const char* extrArgs = GetUnpackerCmdArgsExtract(index);
                const char* ext = GetUnpackerExt(index);

                // test jestli jde o PKZIP25 custom unpacker
                if (extrEXE != NULL && strcmp(extrEXE, "$(Zip32bitExecutable)") == 0)
                {
                    // test jestli jde o stary zaznam PKZIP25 custom unpackeru
                    if (extrArgs != NULL &&
                        strcmp(extrArgs, "-ext -path \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0 &&
                        strcmp(ext, "*.zip") == 0)
                    {
                        // konverze na nove argumenty (pridane "-nozipextension" + "*.pk3;*.jar")
                        char* s = DupStr(newPKZIP25Args);
                        if (s != NULL)
                        {
                            free(Unpackers[index]->CmdArgsExtract); // nemuze byt NULL (jsou zde stare argumenty)
                            Unpackers[index]->CmdArgsExtract = s;
                        }
                        s = DupStr(newPKZIP25Ext);
                        if (s != NULL)
                        {
                            free(Unpackers[index]->Ext); // nemuze byt NULL (jsou zde stare argumenty)
                            Unpackers[index]->Ext = s;
                        }
                    }
                }
            }
        }
    }
        // case 34:   // 2.5b12 - uprava externiho packeru/unpackeru PKZIP25 (externi Win32 verze)

    default:
        break;
    }
    if (SalamVersion > 1 && SalamVersion < 81)
    {
        // od RAR 5.0 jsou filelists defaultne ANSI misto OEM, proto musime OEM vnutit pomoci switche
        const char* newRAR5Args = "x -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"";
        for (index = 0; index < GetUnpackersCount(); index++)
        {
            if ((GetUnpackerOldType(index) && GetUnpackerType(index) == 1) ||
                (!GetUnpackerOldType(index) && GetUnpackerType(index) == CUSTOMPACKER_EXTERNAL))
            {
                const char* extrEXE = GetUnpackerCmdExecExtract(index);
                const char* extrArgs = GetUnpackerCmdArgsExtract(index);
                const char* ext = GetUnpackerExt(index);

                // test jestli jde o RAR Win32 custom unpacker
                if (extrEXE != NULL && strcmp(extrEXE, "$(Rar32bitExecutable)") == 0)
                {
                    // test jestli jde o stary zaznam RAR Win32 custom unpackeru
                    if (extrArgs != NULL &&
                        strcmp(extrArgs, "x \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0)
                    {
                        // konverze na nove argumenty (pridane "-scol")
                        free(Unpackers[index]->CmdArgsExtract); // nemuze byt NULL (jsou zde stare argumenty)
                        Unpackers[index]->CmdArgsExtract = DupStr(newRAR5Args);
                    }
                }
            }
        }
    }
}

BOOL CUnpackerConfig::Load(CUnpackerConfig& src)
{
    PreferedUnpacker = src.PreferedUnpacker;

    DeleteAllUnpackers();
    int i;
    for (i = 0; i < src.GetUnpackersCount(); i++)
    {
        int index = AddUnpacker();
        if (index == -1)
            return FALSE;
        if (!SetUnpacker(index, src.GetUnpackerType(i), src.GetUnpackerTitle(i), src.GetUnpackerExt(i),
                         FALSE, src.GetUnpackerSupLongNames(i),
                         src.GetUnpackerCmdExecExtract(i), src.GetUnpackerCmdArgsExtract(i),
                         src.GetUnpackerNeedANSIListFile(i)))
            return FALSE;
    }
    return TRUE;
}

int CUnpackerConfig::AddUnpacker(BOOL toFirstIndex)
{
    CALL_STACK_MESSAGE2("CUnpackerConfig::AddUnpacker(%d)", toFirstIndex);
    CUnpackerConfigData* data = new CUnpackerConfigData;
    if (data == NULL)
        return -1;
    int index;
    if (toFirstIndex)
    {
        Unpackers.Insert(0, data);
        index = 0;
        if (PreferedUnpacker != -1)
            PreferedUnpacker++;
    }
    else
        index = Unpackers.Add(data);
    if (!Unpackers.IsGood())
    {
        Unpackers.ResetState();
        return -1;
    }
    return index;
}

/*
BOOL
CUnpackerConfig::SwapUnpackers(int index1, int index2)
{
  BYTE buff[sizeof(CUnpackerConfigData)];
  memcpy(buff, Unpackers[index1], sizeof(CUnpackerConfigData));
  memcpy(Unpackers[index1], Unpackers[index2], sizeof(CUnpackerConfigData));
  memcpy(Unpackers[index2], buff, sizeof(CUnpackerConfigData));
  return TRUE;
}
*/

BOOL CUnpackerConfig::MoveUnpacker(int srcIndex, int dstIndex)
{
    BYTE buff[sizeof(CUnpackerConfigData)];
    memcpy(buff, Unpackers[srcIndex], sizeof(CUnpackerConfigData));
    if (srcIndex < dstIndex)
    {
        int i;
        for (i = srcIndex; i < dstIndex; i++)
            memcpy(Unpackers[i], Unpackers[i + 1], sizeof(CUnpackerConfigData));
    }
    else
    {
        int i;
        for (i = srcIndex; i > dstIndex; i--)
            memcpy(Unpackers[i], Unpackers[i - 1], sizeof(CUnpackerConfigData));
    }
    memcpy(Unpackers[dstIndex], buff, sizeof(CUnpackerConfigData));
    return TRUE;
}

void CUnpackerConfig::DeleteUnpacker(int index)
{
    if (PreferedUnpacker >= 0 && PreferedUnpacker < Unpackers.Count)
    {
        if (index < PreferedUnpacker)
            PreferedUnpacker--; // uprava indexu, aby se reprezentoval stejnou polozku
        else
        {
            if (index == PreferedUnpacker)
                PreferedUnpacker = -1; // prisli jsme o vybranou polozku
        }
    }
    Unpackers.Delete(index);
}

BOOL CUnpackerConfig::SetUnpacker(int index, int type, const char* title, const char* ext, BOOL old,
                                  BOOL supportLongNames,
                                  const char* cmdExecExtract, const char* cmdArgsExtract,
                                  BOOL needANSIListFile)
{
    CALL_STACK_MESSAGE10("CUnpackerConfig::SetUnpacker(%d, %d, %s, %s, %d, %d, %s, %s, %d)",
                         index, type, title, ext, old, supportLongNames, cmdExecExtract, cmdArgsExtract,
                         needANSIListFile);
    CUnpackerConfigData* data = Unpackers[index];
    data->Destroy();
    data->Type = type;
    data->OldType = old;
    data->Title = DupStr(title);
    data->Ext = DupStr(ext);
    if (old && data->Type == 1 ||
        !old && data->Type == CUSTOMUNPACKER_EXTERNAL)
    {
        data->CmdExecExtract = DupStr(cmdExecExtract);
        data->CmdArgsExtract = DupStr(cmdArgsExtract);
        data->SupportLongNames = supportLongNames;
        data->NeedANSIListFile = needANSIListFile;
    }

    if (data->IsValid())
    {
        if (PreferedUnpacker == -1)
            PreferedUnpacker = Unpackers.Count - 1;
        return TRUE;
    }
    else
    {
        Unpackers.Delete(index);
        return FALSE;
    }
}

BOOL CUnpackerConfig::SetUnpackerTitle(int index, const char* title)
{
    CUnpackerConfigData* data = Unpackers[index];
    if (data->Title != NULL)
        free(data->Title);
    data->Title = DupStr(title);
    return data->Title != NULL;
}

BOOL CUnpackerConfig::ExecuteUnpacker(HWND parent, CFilesWindow* panel, const char* zipFile, const char* mask,
                                      const char* targetDir, BOOL delArchiveWhenDone, CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CUnpackerConfig::ExecuteUnpacker(, %s, %s, %s, %d, )",
                        zipFile, mask, targetDir, delArchiveWhenDone);
    if (PreferedUnpacker != -1 && PreferedUnpacker < Unpackers.Count)
    {
        CUnpackerConfigData* data = Unpackers[PreferedUnpacker];
        if (data->Type == CUSTOMUNPACKER_EXTERNAL)
        {
            if (delArchiveWhenDone)
                TRACE_E("CUnpackerConfig::ExecuteUnpacker(): delArchiveWhenDone is TRUE for external archiver (unsupported, ignoring)");

            char* tmpMask = DupStr(mask);
            char* command = (char*)malloc(strlen(data->CmdExecExtract) +
                                          strlen(data->CmdArgsExtract) + 2);
            if (tmpMask == NULL || command == NULL)
            {
                TRACE_E(LOW_MEMORY);
                return FALSE;
            }
            sprintf(command, "%s %s", data->CmdExecExtract, data->CmdArgsExtract);

            // pointer musime ulozit kvuli dealokaci, bude znicen
            char* tmpMask2 = tmpMask;
            BOOL ret = PackUniversalUncompress(parent, command, NULL, targetDir, FALSE, panel,
                                               data->SupportLongNames, zipFile, targetDir,
                                               NULL, PackEnumMask, &tmpMask, data->NeedANSIListFile);
            free(tmpMask2);
            free(command);
            return ret;
        }
        else
        {
            CPluginData* plugin = Plugins.Get(-data->Type - 1);
            if (plugin != NULL && plugin->SupportCustomUnpack)
            {
                return plugin->UnpackWholeArchive(panel, zipFile, mask, targetDir, delArchiveWhenDone, archiveVolumes);
            }
            else
                TRACE_E("Unexpected situation in CUnpackerConfig::ExecuteUnpacker().");
        }
    }
    return FALSE;
}

BOOL CUnpackerConfig::Save(int index, HKEY hKey)
{
    DWORD d;
    BOOL ret = TRUE;
    int type = GetUnpackerType(index);
    d = type;
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_CPU_TYPE, REG_DWORD, &d, sizeof(d));
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_CPU_TITLE, REG_SZ, GetUnpackerTitle(index), -1);
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_CPU_EXT, REG_SZ, GetUnpackerExt(index), -1);
    if (ret && type == CUSTOMUNPACKER_EXTERNAL)
    {
        d = GetUnpackerSupLongNames(index);
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_CPU_SUPLONG, REG_DWORD, &d, sizeof(d));
        d = GetUnpackerNeedANSIListFile(index);
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_CPU_ANSILIST, REG_DWORD, &d, sizeof(d));
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_CU_EXECEXTRACT, REG_SZ, GetUnpackerCmdExecExtract(index), -1);
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_CU_ARGSEXTRACT, REG_SZ, GetUnpackerCmdArgsExtract(index), -1);
    }
    return ret;
}

BOOL CUnpackerConfig::Load(HKEY hKey)
{
    int max = MAX_PATH + 2;

    char title[MAX_PATH + 2];
    char ext[MAX_PATH + 2];
    DWORD type;
    DWORD suplong = FALSE;
    DWORD needANSI = FALSE;
    char execcopy[MAX_PATH + 2];
    execcopy[0] = 0;
    char argscopy[MAX_PATH + 2];
    argscopy[0] = 0;

    BOOL ret = TRUE;
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_CPU_TYPE, REG_DWORD, &type, sizeof(DWORD));
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_CPU_TITLE, REG_SZ, title, max);
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_CPU_EXT, REG_SZ, ext, max);
    if (ret && (Configuration.ConfigVersion < 6 && type == 1 ||
                Configuration.ConfigVersion >= 6 && type == CUSTOMUNPACKER_EXTERNAL))
    {
        if (ret)
        {
            if (!GetValue(hKey, SALAMANDER_CPU_ANSILIST, REG_DWORD, &needANSI, sizeof(DWORD)))
                needANSI = FALSE; // ve starsich verzich nebylo, predpokladalo se FALSE
        }

        if (ret)
            ret &= GetValue(hKey, SALAMANDER_CPU_SUPLONG, REG_DWORD, &suplong, sizeof(DWORD));
        if (ret)
            ret &= GetValue(hKey, SALAMANDER_CU_EXECEXTRACT, REG_SZ, execcopy, max);
        if (ret)
            ret &= GetValue(hKey, SALAMANDER_CU_ARGSEXTRACT, REG_SZ, argscopy, max);
    }

    if (ret)
    {
        int index;
        if ((index = AddUnpacker()) == -1)
            return FALSE;
        if (Configuration.ConfigVersion < 44) // prevod pripon na lowercase
        {
            char extAux[MAX_PATH + 2];
            lstrcpyn(extAux, ext, MAX_PATH + 2);
            StrICpy(ext, extAux);
        }
        ret &= SetUnpacker(index, (int)type, title, ext, Configuration.ConfigVersion < 6,
                           (BOOL)suplong,
                           execcopy, argscopy, needANSI);
    }

    return ret;
}
