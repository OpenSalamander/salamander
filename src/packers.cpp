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

// Conversion table for translating exe->variable
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

// order in which custom packers/unpackers were historically added
int CustomOrder[] = {0, 1, 9, 10, 2, 3, 4, 11, 5, 6, 7, 8};

// table of custom packers
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
    {{"a -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"", "a -scol -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\""}, // from version 5.0 we need to enforce the -scol switch, version 4.20 doesn't mind; it occurs in other places and in the registry
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
    // LIE
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

// Custom unpacker table
SPackCustomUnpacker CustomUnpackers[] = {
    // JAR32
    {"x -jyc \"$(ArchiveFullName)\" !\"$(ListFullName)\"", IDS_DU_JAR_E, "*.j", TRUE, FALSE, "jar32"},
    // RAR32
    {"x -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"", IDS_DU_RAR_E, "*.rar", TRUE, FALSE, "rar"}, // from version 5.0 we need to enforce the -scol switch, version 4.20 doesn't mind; it occurs in other places and in the registry
    // ARJ16
    {"x -va -jyc $(ArchiveDOSFullName) !$(ListDOSFullName)", IDS_DU_ARJ16_E, "*.arj", FALSE, FALSE, "arj"},
    // LIE
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
    /*    if (!disableDefaultValues)
    AddDefault(0);*/
}

void CPackerConfig::InitializeDefaultValues()
{
    AddDefault(0);
}

void CPackerConfig::AddDefault(int SalamVersion)
{
    // WARNING: up to version 6 old 'Type' values: 0 ZIP, 1 external, 2 TAR, 3 PAK

    // default values
    int index, i;
    // first internal, so that it is sorted
    switch (SalamVersion)
    {
    case 0: // default config
    case 1: // v1.52 did not have a packer
        if ((index = AddPacker()) == -1)
            return;
        SetPacker(index, 0, "ZIP (Plugin)", "zip", TRUE);
    case 2: // what has been added since beta1
        if ((index = AddPacker()) == -1)
            return;
        SetPacker(index, 3, "PAK (Plugin)", "pak", TRUE);
    case 3:  // what has been added after beta2
    case 4:  // beta 3 but with old configuration (contains $(SpawnName))
    case 5:; // What's new in beta4?
             //      if ((index = AddPacker()) == -1) return;
             //      SetPacker(index, 2, "TAR (Plugin)", "tgz", TRUE);
    }
    // external now
    switch (SalamVersion)
    {
        // format of the parameter
        //BOOL SetPacker(int index, int type, const char *title, const char *ext, BOOL old,
        //               BOOL supportLongNames = FALSE, BOOL supportMove = FALSE,
        //               const char *cmdExecCopy = NULL, const char *cmdArgsCopy = NULL,
        //               const char *cmdExecMove = NULL, const char *cmdArgsMove = NULL,
        //               BOOL needANSIListFile = FALSE);

    case 0: // default config
    case 1: // v1.52 did not have a packer
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
    case 2: // what has been added since beta1
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
    case 3: // what has been added after beta2
    case 4: // beta 3 but with old configuration (contains $(SpawnName))
        // In older versions, there may be a variable $(SpawnName) that no longer exists - we need to remove it
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
    case 5: // beta 3 but without tar
    case 6: // What's new in beta4?
    case 7: // 1.6b6 - for correct conversion of supported plugin functions (see CPlugins::Load)
    case 8:
        // switch to using variables instead of the direct name of the exe file
        for (index = 0; index < GetPackersCount(); index++)
            // we consider only external packers
            if (((GetPackerOldType(index) && GetPackerType(index) == 1) ||
                 (!GetPackerOldType(index) && GetPackerType(index) == CUSTOMPACKER_EXTERNAL)) &&
                GetPackerCmdExecCopy(index) != NULL && GetPackerCmdExecMove(index) != NULL)
            {
                // let's take old commands
                char* cmdC = DupStr(GetPackerCmdExecCopy(index));
                char* cmdM = DupStr(GetPackerCmdExecMove(index));
                i = 0;
                BOOL found = FALSE;
                // and we will search the table with them
                while (PackConversionTable[i].exe != NULL)
                {
                    // compare with elements of the table
                    if (!strcmp(cmdC, PackConversionTable[i].exe) || !strcmp(cmdM, PackConversionTable[i].exe))
                    {
                        // and if we find it, we change it to a variable
                        if (!strcmp(cmdC, PackConversionTable[i].exe))
                        {
                            free(cmdC);
                            // and one ugly hack because of the bug
                            if (i == 2)
                                // if it's a rar, we can't tell if it's 16bit or 32bit directly, only by the support of long names
                                if (GetPackerSupLongNames(index))
                                    cmdC = DupStr(PackConversionTable[i].variable);
                                else
                                    cmdC = DupStr("$(Rar16bitExecutable)");
                            else
                                // for others it is simple
                                cmdC = DupStr(PackConversionTable[i].variable);
                        }
                        if (!strcmp(cmdM, PackConversionTable[i].exe))
                        {
                            free(cmdM);
                            // and one ugly hack because of the bug
                            if (i == 2)
                                // if it's a rar, we can't tell if it's 16bit or 32bit directly, only by the support of long names
                                if (GetPackerSupLongNames(index))
                                    cmdM = DupStr(PackConversionTable[i].variable);
                                else
                                    cmdM = DupStr("$(Rar16bitExecutable)");
                            else
                                // for others it is simple
                                cmdM = DupStr(PackConversionTable[i].variable);
                        }
                        found = TRUE;
                    }
                    i++;
                }
                // Strings must be copied somewhere, otherwise I will delete them before using them
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
    case 9: // 1.6b6 - for transitioning from the name 'exe' to a variable in custom packers
        // loading an ANSI file list in ACE32 and PKZIP25
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
        // change "XXX (Internal)" to "XXX (Plugin)"
        // It will be terribly exhausting because we are changing to a shorter text -> we will just stick with the transcript
        for (index = 0; index < GetPackersCount(); index++)
        {
            if ((GetPackerOldType(index) && GetPackerType(index) != 1) ||
                (!GetPackerOldType(index) && GetPackerType(index) != CUSTOMPACKER_EXTERNAL))
            { // we only take plug-ins (not external packers)
                char* s = Packers[index]->Title;
                char* f;
                if (s != NULL && (f = strstr(s, "(Internal)")) != NULL) // contains (Internal)
                {
                    if (strlen(f) == 10)
                        strcpy(f, "(Plugin)"); // (Internal) is at the end of the string
                }
            }
        }
    case 10: // 1.6b6 - due to renaming "XXX (Internal)" to "XXX (Plugin)" in Pack and Unpack dialogs
             //         and for setting the ANSI version of the "list of files" file for ACE32 and PKZIP25 (un)packers
    case 11: // 1.6b7 - CheckVer plugin added - we will ensure its automatic installation
    case 12: // 2.0 - automatic shutdown of salopen.exe + added PEViewer plugin - we will ensure its automatic installation
    {
        // at LHA added "-m", we need to complete it and if archivers-auto-config added LHA again
        // because it didn't match "-m", we will remove that new record
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

                // test if it is an LHA custom packer
                if (copyEXE != NULL && strcmp(copyEXE, "$(Lha16bitExecutable)") == 0 &&
                    moveEXE != NULL && strcmp(moveEXE, "$(Lha16bitExecutable)") == 0)
                {
                    // test if it is an old record of the LHA custom packer
                    if (copyArgs != NULL &&
                        strcmp(copyArgs, "a -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)") == 0 &&
                        moveArgs != NULL &&
                        strcmp(moveArgs, "m -p -a -l1 -x1 -c $(ArchiveDOSFullName) @$(ListDOSFullName)") == 0)
                    {
                        if (canDelLHA)
                        {
                            // We will delete the record, it is unnecessary (a functional record for LHA already exists)
                            DeletePacker(index);
                            index--;
                        }
                        else
                        {
                            canDelLHA = TRUE;
                            // conversion to new arguments (added "-m")
                            char* s = DupStr(newLHACopyArgs);
                            if (s != NULL)
                            {
                                free(Packers[index]->CmdArgsCopy); // cannot be NULL (old arguments are present)
                                Packers[index]->CmdArgsCopy = s;
                            }
                            s = DupStr(newLHAMoveArgs);
                            if (s != NULL)
                            {
                                free(Packers[index]->CmdArgsMove); // cannot be NULL (old arguments are present)
                                Packers[index]->CmdArgsMove = s;
                            }
                        }
                    }
                    else
                    {
                        // test if it is an archivers-auto-config entry added by the LHA custom packer
                        if (copyArgs != NULL && strcmp(copyArgs, newLHACopyArgs) == 0 &&
                            moveArgs != NULL && strcmp(moveArgs, newLHAMoveArgs) == 0)
                        {
                            if (canDelLHA)
                            {
                                // We will delete the added record, it is unnecessary (a functional record for LHA already exists)
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
    case 13: // 2.5b1 - added missing configuration conversion for custom packers - reflecting changes in LHA
    case 14: // 2.5b1 - New Advanced Options in the Find dialog. Transition to CFilterCriteria. Conversion of inverse mask in the filter.
    case 15: // 2.5b2 - newer version, so that plugins can be loaded (upgrade records in the registry)
    case 16: // 2.5b2 - added coloring of Encrypted files and directories (added during config load + is in the default config)
    case 17: // 2.5b2 - added *.xml mask to the internal viewer settings - "force text mode"
    case 18: // 2.5b3 - currently only for transferring plugin configuration from version 2.5b2
    case 19: // 2.5b4 - currently only for transferring plugin configuration from version 2.5b3
    case 20: // 2.5b5 - currently only for transferring plugin configuration from version 2.5b4
    case 21: // 2.5b6 - currently only for transferring plugin configuration from version 2.5b5(a)
    case 22: // 2.5b6 - filters in panels -- unified into one history
    case 23: // 2.5b6 - new view in the panel (Tiles)
    case 24: // 2.5b7 - currently only for transferring plugin configuration from version 2.5b6
    case 25: // 2.5b7 - plugins: show in plugin bar -> transfer variable to CPluginData
    case 26: // 2.5b8 - currently only for transferring plugin configuration from version 2.5b7
    case 27: // 2.5b9 - currently only for transferring plugin configuration from version 2.5b8
    case 28: // 2.5b9 - new color scheme based on the old DOS Navigator -> conversion 'scheme'
    case 29: // 2.5b10 - currently only for transferring plugin configuration from version 2.5b9
    case 30: // 2.5b11 - currently only for transferring plugin configuration from version 2.5b10
    case 31: // 2.5b11 - we introduced the Floppy section in the Drives configuration and need to forcibly refresh the icon reading for Removable
    case 32: // 2.5b11 - Find: "Local Settings\\Temporary Internet Files" is implicitly searched
    case 33: // 2.5b12 - currently only for transferring plugin configuration from version 2.5b11
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

                // test if it is a PKZIP25 custom packer
                if (copyEXE != NULL && strcmp(copyEXE, "$(Zip32bitExecutable)") == 0 &&
                    moveEXE != NULL && strcmp(moveEXE, "$(Zip32bitExecutable)") == 0)
                {
                    // test if it is an old record of PKZIP25 custom packer
                    if (copyArgs != NULL &&
                        strcmp(copyArgs, "-add -path -attr \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0 &&
                        moveArgs != NULL &&
                        strcmp(moveArgs, "-add -move -path -attr \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0)
                    {
                        // conversion to new arguments (added "-nozipextension")
                        char* s = DupStr(newPKZIP25CopyArgs);
                        if (s != NULL)
                        {
                            free(Packers[index]->CmdArgsCopy); // cannot be NULL (old arguments are present)
                            Packers[index]->CmdArgsCopy = s;
                        }
                        s = DupStr(newPKZIP25MoveArgs);
                        if (s != NULL)
                        {
                            free(Packers[index]->CmdArgsMove); // cannot be NULL (old arguments are present)
                            Packers[index]->CmdArgsMove = s;
                        }
                    }
                }
            }
        }
    }
        // case 34:   // 2.5b12 - modification of external packer/unpacker PKZIP25 (external Win32 version)

    default:
        break;
    }
    if (SalamVersion > 1 && SalamVersion < 81)
    {
        // Starting from RAR 5.0, filelists are by default ANSI instead of OEM, so we need to force OEM using a switch
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

                // test if it is a RAR Win32 custom packer
                if (copyEXE != NULL && strcmp(copyEXE, "$(Rar32bitExecutable)") == 0 &&
                    moveEXE != NULL && strcmp(moveEXE, "$(Rar32bitExecutable)") == 0)
                {
                    // test if it is an old record of RAR Win32 custom packer
                    if (copyArgs != NULL &&
                        strcmp(copyArgs, "a \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0 &&
                        moveArgs != NULL &&
                        strcmp(moveArgs, "m \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0)
                    {
                        // conversion to new arguments (added "-scol")
                        free(Packers[index]->CmdArgsCopy); // cannot be NULL (old arguments are present)
                        Packers[index]->CmdArgsCopy = DupStr(newRAR5CopyArgs);
                        free(Packers[index]->CmdArgsMove); // cannot be NULL (old arguments are present)
                        Packers[index]->CmdArgsMove = DupStr(newRAR5MoveArgs);
                    }
                    if (copyArgs != NULL &&
                        strcmp(copyArgs, "a -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0 &&
                        moveArgs != NULL &&
                        strcmp(moveArgs, "m -v1440 \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0)
                    {
                        // conversion to new arguments (added "-scol")
                        free(Packers[index]->CmdArgsCopy); // cannot be NULL (old arguments are present)
                        Packers[index]->CmdArgsCopy = DupStr(newRAR5CopyVolArgs);
                        free(Packers[index]->CmdArgsMove); // cannot be NULL (old arguments are present)
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

/*  BOOL
CPackerConfig::SwapPackers(int index1, int index2)
{
  BYTE buff[sizeof(CPackerConfigData)];
  memcpy(buff, Packers[index1], sizeof(CPackerConfigData));
  memcpy(Packers[index1], Packers[index2], sizeof(CPackerConfigData));
  memcpy(Packers[index2], buff, sizeof(CPackerConfigData));
  return TRUE;
}*/

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
            PreferedPacker--; // Adjusting the index to represent the same item
        else
        {
            if (index == PreferedPacker)
                PreferedPacker = -1; // we lost the selected item
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
                needANSI = FALSE; // in older versions it was not present, it was assumed FALSE
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
        if (Configuration.ConfigVersion < 44) // convert extension to lowercase
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
    /*    if (!disableDefaultValues)
    AddDefault(0);*/
}

void CUnpackerConfig::InitializeDefaultValues()
{
    AddDefault(0);
}

void CUnpackerConfig::AddDefault(int SalamVersion)
{
    // WARNING: up to version 6 old 'Type' values: 0 ZIP, 1 external, 2 TAR, 3 PAK

    // Convert loaded values from 1.6b1 - extensions were not masks ("EXT" -> "*.EXT")
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

    // default values
    int index, i;
    // first internal, so that it is sorted
    switch (SalamVersion)
    {
    case 0: // default config
    case 1: // v1.52 did not have a packer
        if ((index = AddUnpacker()) == -1)
            return;
        SetUnpacker(index, 0, "ZIP (Plugin)", "*.zip", TRUE);
    case 2: // what has been added since beta1
        // hack to add the pk3 extension to a zip
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
        // and new formats
        if ((index = AddUnpacker()) == -1)
            return;
        SetUnpacker(index, 3, "PAK (Plugin)", "*.pak", TRUE);
    case 3: // what has been added after beta2
    case 4: // beta 3 but without the $(SpawnName) variable
    case 5: // What's new in beta4?
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
    // external now
    switch (SalamVersion)
    {
        // parameters
        //BOOL SetUnpacker(int index, int type, const char *title, const char *ext, BOOL old,
        //                 BOOL supportLongNames = FALSE,
        //                 const char *cmdExecExtract = NULL, const char *cmdArgsExtract = NULL,
        //                 BOOL needANSIListFile = FALSE);

    case 0: // default config
    case 1: // v1.52 did not have a packer
        for (i = 0; i < 7; i++)
        {
            int idx = CustomOrder[i];
            if ((index = AddUnpacker()) == -1)
                return;
            SetUnpacker(index, 1, LoadStr(CustomUnpackers[idx].Title), CustomUnpackers[idx].Ext, TRUE,
                        CustomUnpackers[idx].SupLN, CustomUnpackers[idx].Exe,
                        CustomUnpackers[idx].Args, CustomUnpackers[idx].Ansi);
        }
    case 2: // what has been added since beta1
        for (i = 7; i < 12; i++)
        {
            int idx = CustomOrder[i];
            if ((index = AddUnpacker()) == -1)
                return;
            SetUnpacker(index, 1, LoadStr(CustomUnpackers[idx].Title), CustomUnpackers[idx].Ext, TRUE,
                        CustomUnpackers[idx].SupLN, CustomUnpackers[idx].Exe,
                        CustomUnpackers[idx].Args, CustomUnpackers[idx].Ansi);
        }
    case 3: // what has been added after beta2
    case 4: // beta 3 but without the $(SpawnName) variable
        // In older versions, there may be a variable $(SpawnName) that no longer exists - we need to remove it
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
    case 5: // beta 3 but without tar
    case 6: // What's new in beta4?
    case 7: // 1.6b6 - for correct conversion of supported plugin functions (see CPlugins::Load)
    case 8:
        // switch to using variables instead of the direct name of the exe file
        for (index = 0; index < GetUnpackersCount(); index++)
            // we consider only external packers
            if (((GetUnpackerOldType(index) && GetUnpackerType(index) == 1) ||
                 (!GetUnpackerOldType(index) && GetUnpackerType(index) == CUSTOMUNPACKER_EXTERNAL)) &&
                GetUnpackerCmdExecExtract(index) != NULL)
            {
                // let's take old commands
                char* cmd = DupStr(GetUnpackerCmdExecExtract(index));
                i = 0;
                BOOL found = FALSE;
                // and we will search the table with them
                while (PackConversionTable[i].exe != NULL)
                {
                    // compare with elements of the table
                    if (!strcmp(cmd, PackConversionTable[i].exe))
                    {
                        free(cmd);
                        // and one ugly hack because of the bug
                        if (i == 2)
                            // if it's a rar, we can't tell if it's 16bit or 32bit directly, only by the support of long names
                            if (GetUnpackerSupLongNames(index))
                                cmd = DupStr(PackConversionTable[i].variable);
                            else
                                cmd = DupStr("$(Rar16bitExecutable)");
                        else
                            // for others it is simple
                            cmd = DupStr(PackConversionTable[i].variable);
                        found = TRUE;
                    }
                    i++;
                }
                // Strings must be copied somewhere, otherwise I will delete them before using them
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
    case 9: // 1.6b6 - for transitioning from the name 'exe' to a variable in custom packers
        // loading an ANSI file list in ACE32 and PKZIP25
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
        // change "XXX (Internal)" to "XXX (Plugin)"
        // It will be terribly exhausting because we are changing to a shorter text -> we will just stick with the transcript
        for (index = 0; index < GetUnpackersCount(); index++)
        {
            if ((GetUnpackerOldType(index) && GetUnpackerType(index) != 1) ||
                (!GetUnpackerOldType(index) && GetUnpackerType(index) != CUSTOMUNPACKER_EXTERNAL))
            { // we only take plug-ins (not external unpackers)
                char* s = Unpackers[index]->Title;
                char* f;
                if (s != NULL && (f = strstr(s, "(Internal)")) != NULL) // contains (Internal)
                {
                    if (strlen(f) == 10)
                        strcpy(f, "(Plugin)"); // (Internal) is at the end of the string
                }
            }
        }
    case 10: // 1.6b6 - due to renaming "XXX (Internal)" to "XXX (Plugin)" in Pack and Unpack dialogs
    case 11: // 1.6b7 - CheckVer plugin added - we will ensure its automatic installation
    case 12: // 2.0 - automatic shutdown of salopen.exe + added PEViewer plugin - we will ensure its automatic installation
    case 13: // 2.5b1 - added missing configuration conversion for custom packers - reflecting changes in LHA
    case 14: // 2.5b1 - New Advanced Options in the Find dialog. Transition to CFilterCriteria. Conversion of inverse mask in the filter.
    case 15: // 2.5b2 - newer version, so that plugins can be loaded (upgrade records in the registry)
    case 16: // 2.5b2 - added coloring of Encrypted files and directories (added during config load + is in the default config)
    case 17: // 2.5b2 - added *.xml mask to the internal viewer settings - "force text mode"
    case 18: // 2.5b3 - currently only for transferring plugin configuration from version 2.5b2
    case 19: // 2.5b4 - currently only for transferring plugin configuration from version 2.5b3
    case 20: // 2.5b5 - currently only for transferring plugin configuration from version 2.5b4
    case 21: // 2.5b6 - currently only for transferring plugin configuration from version 2.5b5(a)
    case 22: // 2.5b6 - filters in panels -- unified into one history
    case 23: // 2.5b6 - new view in the panel (Tiles)
    case 24: // 2.5b7 - currently only for transferring plugin configuration from version 2.5b6
    case 25: // 2.5b7 - plugins: show in plugin bar -> transfer variable to CPluginData
    case 26: // 2.5b8 - currently only for transferring plugin configuration from version 2.5b7
    case 27: // 2.5b9 - currently only for transferring plugin configuration from version 2.5b8
    case 28: // 2.5b9 - new color scheme based on the old DOS Navigator -> conversion 'scheme'
    case 29: // 2.5b10 - currently only for transferring plugin configuration from version 2.5b9
    case 30: // 2.5b11 - currently only for transferring plugin configuration from version 2.5b10
    case 31: // 2.5b11 - we introduced the Floppy section in the Drives configuration and need to forcibly refresh the icon reading for Removable
    case 32: // 2.5b11 - Find: "Local Settings\\Temporary Internet Files" is implicitly searched
    case 33: // 2.5b12 - currently only for transferring plugin configuration from version 2.5b11
    {
        // in PKZIP25 added "-nozipextension -directories" and "*.pk3;*.jar", we need to add it
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

                // test if it is a PKZIP25 custom unpacker
                if (extrEXE != NULL && strcmp(extrEXE, "$(Zip32bitExecutable)") == 0)
                {
                    // test if it is an old record of PKZIP25 custom unpacker
                    if (extrArgs != NULL &&
                        strcmp(extrArgs, "-ext -path \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0 &&
                        strcmp(ext, "*.zip") == 0)
                    {
                        // Conversion to new arguments (added "-nozipextension" + "*.pk3;*.jar")
                        char* s = DupStr(newPKZIP25Args);
                        if (s != NULL)
                        {
                            free(Unpackers[index]->CmdArgsExtract); // cannot be NULL (old arguments are present)
                            Unpackers[index]->CmdArgsExtract = s;
                        }
                        s = DupStr(newPKZIP25Ext);
                        if (s != NULL)
                        {
                            free(Unpackers[index]->Ext); // cannot be NULL (old arguments are present)
                            Unpackers[index]->Ext = s;
                        }
                    }
                }
            }
        }
    }
        // case 34:   // 2.5b12 - modification of external packer/unpacker PKZIP25 (external Win32 version)

    default:
        break;
    }
    if (SalamVersion > 1 && SalamVersion < 81)
    {
        // Starting from RAR 5.0, filelists are by default ANSI instead of OEM, so we need to force OEM using a switch
        const char* newRAR5Args = "x -scol \"$(ArchiveFullName)\" @\"$(ListFullName)\"";
        for (index = 0; index < GetUnpackersCount(); index++)
        {
            if ((GetUnpackerOldType(index) && GetUnpackerType(index) == 1) ||
                (!GetUnpackerOldType(index) && GetUnpackerType(index) == CUSTOMPACKER_EXTERNAL))
            {
                const char* extrEXE = GetUnpackerCmdExecExtract(index);
                const char* extrArgs = GetUnpackerCmdArgsExtract(index);
                const char* ext = GetUnpackerExt(index);

                // test if it is a RAR Win32 custom unpacker
                if (extrEXE != NULL && strcmp(extrEXE, "$(Rar32bitExecutable)") == 0)
                {
                    // test if it is an old record of the RAR Win32 custom unpacker
                    if (extrArgs != NULL &&
                        strcmp(extrArgs, "x \"$(ArchiveFullName)\" @\"$(ListFullName)\"") == 0)
                    {
                        // conversion to new arguments (added "-scol")
                        free(Unpackers[index]->CmdArgsExtract); // cannot be NULL (old arguments are present)
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

/*  BOOL
CUnpackerConfig::SwapUnpackers(int index1, int index2)
{
  BYTE buff[sizeof(CUnpackerConfigData)];
  memcpy(buff, Unpackers[index1], sizeof(CUnpackerConfigData));
  memcpy(Unpackers[index1], Unpackers[index2], sizeof(CUnpackerConfigData));
  memcpy(Unpackers[index2], buff, sizeof(CUnpackerConfigData));
  return TRUE;
}*/

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
            PreferedUnpacker--; // Adjusting the index to represent the same item
        else
        {
            if (index == PreferedUnpacker)
                PreferedUnpacker = -1; // we lost the selected item
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

            // we need to save the pointer for deallocation, it will be destroyed
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
                needANSI = FALSE; // in older versions it was not present, it was assumed FALSE
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
        if (Configuration.ConfigVersion < 44) // convert suffixes to lowercase
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
