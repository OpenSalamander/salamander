// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//********************************************************************************
// texts.h
// messages for Zip2Exe
//

STRING(STR_WELCOME_MESSAGE,
       "\n"
       "Zip2Sfx 2.9  Copyright (c) 2000-2023 Open Salamander Authors   Home page: www.altap.cz\n"
       "                                             E-mail:    support@altap.cz\n\n")
STRING(STR_HELP,
       "Usage:\tzip2sfx [options] zipfile [exefile]\n"
       "\n"
       "zipfile  Archive you want self-extracting archive create from.\n"
       "exefile  New self-extracting archive you wish to create.\n"
       "\n\n"
       "Options are:\n"
       "-o\t\tAutomatically overwrite output file.\n"
       "-p<sfxpackage>\tUse <sfxpackage> file containing executable and language\n"
       "\t\tspecific data.\n"
       "-s<settings>\tUse given settings file containing advanced options\n\n"
       "Note: switches -p and -s cannot be combined, you can use only one of them\n"
       /*"\t\tfor self-extractor.\n"
"-s<settings>\tUse given settings file exported from ZIP plugin\n"
"\t\tfor Open Salamander)\n"*/
       "\n\n"
       "File names contaning spaces must be enclosed in double quotes.\n"
       "Example:\n\tzip2exe -ssample.set \"C:\\My Files\\my archive.zip\"\n\n"
       "For more details on zip2sfx usage see readme.txt.\n")
STRING(STR_SUCCESS, "Operation completed successfully.\nBye\n")
STRING(STR_OVERWRITE, "File '%s' already exists. Overwrite (y/n)? ")
STRING(STR_WRITINGEXE, "Writing self-extractor executable...\n")
STRING(STR_WRITINGARC, "Writing archive data...\n")
STRING(STR_SFXINFO, "Used sfx package: %s\nLanguage: %s\n")
//********************************************************************************
//
// error messages
//

STRING(STR_LOWMEM, "Low memory\n")
STRING(STR_ERRREAD, "Unable to read from file: %s.\n")
STRING(STR_ERRWRITE, "Unable to write to file: %s.\n")
STRING(STR_ERRACCESS, "Error accessing file: %s\n")
STRING(STR_ERROPEN, "Unable to open file: %s\n")
STRING(STR_ERRCREATE, "Unable to create file: %s\n")
STRING(STR_CORRUPTSFX, "SFX package '%s' is corrupted.\n")
STRING(STR_EOF, "Unexpected end file.\n")
STRING(STR_ERRICON, "Error adding icon in executable.\n")
STRING(STR_BADFORMAT, "The archive either damaged or of unknown format.\n")
STRING(STR_MULTVOL, "Sorry. Multi-volume archives are not supported yet.\n")
STRING(STR_EMPTYARCHIVE, "Nothing to do, archive is empty.\n")
STRING(STR_BADMETHOD, "Archive contains files compressed by method not supported by the self-extractor.\n")
STRING(STR_BADSETFORMAT, "Unable to load settings file: invalid format.\n")
STRING(STR_MISSINGVERSION, "Unable to load settings file: missing version number.")
STRING(STR_BADVERSION, "Unable to load settings file: unsupported version.")
STRING(STR_BADMSGBOXTYPE, "\"I Agree + I Disagree\" buttons can be used only with conjuction with \"Long Message\" message box type.")
STRING(STR_BADTEMP, "Unable to load settings file, bad value of 'target_dir': $(Temp) can't be followed by any character.")
STRING(STR_MISBAR, "Unable to load settings file, bad value of 'target_dir': missing closing bar of the $() sequence.")
STRING(STR_BADVAR, "Unable to load settings file, bad value of 'target_dir': unknown $() variable.")
STRING(STR_BADKEY, "Unable to load settings file, bad value of 'target_dir': invalid key name.")
STRING(STR_NODEFPACKAGE,
       "No default sfx package found.\n\n"
       "To specify a default sfx package to be used by zip2exe create an environment\n"
       "variable 'SFX_DEFPACKAGE' and set it's value to a filename which you want\n"
       "to be the default sfx package.\n\n"
       "If you haven't set 'SFX_DEFPACKAGE' environment variable you must specify\n"
       "explicitly which sfx package to use. You can either use switch 'p' to\n"
       "tell dirrectly which sfx package to use or use switch 's' and specify sfx\n"
       "package file in the settings file (it's variable 'sfxpackage')\n")
STRING(STR_ERROPENICO, "Unable to open file containing icon data (%s).\n")
STRING(STR_ERRLOADLIB, "Unable to load module containing icon data (%s). The module is not a valid Win32 application.\n")
STRING(STR_ERRLOADLIB2, "Unable to load module containing icon data (%s).\n")
STRING(STR_ERRLOADICON, "Unable to load icon from file '%s'.\n")
STRING(STR_BADSFXVER, "Unsupported version of SFX package (%s) please download latest Zip2Sfx with an appropriate sfx packages version.\n")
STRING(STR_LARGETEXT, "Unable to load SFX package (%s): texts too large.\n")
STRING(STR_ZIP64, "Sorry. Zip64 archives are not supported.")
STRING(STR_ERRMANIFEST, "Cannot read manifest\n")
