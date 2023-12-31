﻿// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// text.h - english version

STRING(STR_TITLE, "Self-Extracting ZIP Archive")
STRING(STR_ERROR, "Error")
STRING(STR_QUESTION, "Question")
STRING(STR_ERROR_FOPEN, "Unable to open archive file: ")
STRING(STR_ERROR_FSIZE, "Unable to get archive file size: ")
STRING(STR_ERROR_MAXSIZE, "Maximal archive size reached.")
STRING(STR_ERROR_FMAPPING, "Unable to map archive file to memory: ")
STRING(STR_ERROR_FVIEWING, "Unable to create view of memory mapped archive file: ")
STRING(STR_ERROR_NOEXESIG, "No EXE signature found.")
STRING(STR_ERROR_NOPESIG, "No PE executable signature found.")
STRING(STR_ERROR_INCOMPLETE, "Error in data format - incomplete file.")
STRING(STR_ERROR_SFXSIG, "No self-extractor signature found.")
STRING(STR_ERROR_TEMPPATH, "Unable to get path to temporary files from system: ")
STRING(STR_ERROR_TEMPNAME, "Unable to get temporary file name: ")
STRING(STR_ERROR_DELFILE, "Unable to delete temporary file: ")
STRING(STR_ERROR_MKDIR, "Unable to create temporary directory: ")
STRING(STR_ERROR_GETATTR, "Unable to get file attributes: ")
STRING(STR_ERROR_GETCWD, "Unable to get current working directory: ")
STRING(STR_ERROR_BADDATA, "Error in compressed data.")
STRING(STR_ERROR_TOOLONGNAME, "File name is too long.")
STRING(STR_LOWMEM, "Error, low memory: ")
STRING(STR_NOTENOUGHTSPACE, "There doesn't seem to be enough space on target drive.\nDo you want to continue ?\n\nRequired space: %s bytes.\nAvailable space: %s bytes.\n%s")
STRING(STR_ERROR_TCREATE, "Unable to create new thread: ")
STRING(STR_ERROR_METHOD, "Unable to extract file. File is compressed by unsupported method.")
STRING(STR_ERROR_CRC, "CRC doesn't match.")
STRING(STR_ERROR_ACCES, "Unable to access the file: ")
STRING(STR_ERROR_WRITE, "Unable to write the file: ")
STRING(STR_ERROR_BREAK, "Do you wish to terminate files extraction?")
STRING(STR_ERROR_DLG, "Unable to create dialog window: ")
STRING(STR_ERROR_FCREATE, "Unable to create or open file: ")
STRING(STR_ERROR_WAIT, "Unable to wait for process: ")
STRING(STR_ERROR_WAIT2, "\nPress OK when you assume the temporary files can be safely deleted.")
STRING(STR_ERROR_WAITTHREAD, "Unable to wait for thread: ")
STRING(STR_ERROR_DCREATE, "Unable to create new directory: ")
STRING(STR_ERROR_DCREATE2, "Unable to create new directory: This name has already been used for a file.")
STRING(STR_ERROR_FCREATE2, "Unable to create file: This name has already been used for a directory.")
STRING(STR_TARGET_NOEXIST, "Target directory doesn't exist, do you want to create it?")
STRING(STR_OVERWRITE_RO, "Do you want to overwrite the file with READONLY, SYSTEM or HIDDEN attribute?")
STRING(STR_STOP, "Stop")
STRING(STR_STATNOTCOMPLETE, "No files were extracted.")
STRING(STR_SKIPPED, "Number of skipped files:\t%d\n")
STRING(STR_STATOK, "Extracting successfully completed.\n\nNumber of extracted files:\t%d\n%sTotal size of extracted files:\t%s B  ")
STRING(STR_BROWSEDIRTITLE, "Select folder.")
STRING(STR_BROWSEDIRCOMMENT, "Select the folder to extract files to.")
STRING(STR_OVERWRITEDLGTITLE, "Confirm File Overwrite")
STRING(STR_TEMPDIR, "&Folder to store temporary files:")
STRING(STR_EXTRDIR, "&Folder to extract files to:")
STRING(STR_TOOLONGCOMMAND, "Unable to execute command: filename is too long")
STRING(STR_ERROR_EXECSHELL, "Unable to execute command: ")
STRING(STR_ADVICE, "\nNote: You can run selfextractor with the parameter \'-s\' and select the folder to extract files to by yourself.")
STRING(STR_BADDRIVE, "Specified drive or network share name is invalid.")
STRING(STR_ABOUT1, "&About >>")
STRING(STR_ABOUT2, "<< &About")
STRING(STR_OK, "OK")
STRING(STR_CANCEL, "Cancel")
STRING(STR_YES, "Yes")
STRING(STR_NO, "No")
STRING(STR_AGREE, "I Agree")
STRING(STR_DISAGREE, "I Disagree")
#ifndef EXT_VER
STRING(STR_ERROR_ENCRYPT, "Unable to extract file. File is encrypted.")
STRING(STR_PARAMTERS, "Command line parameters:\n\n-s\tSafe mode. Displays main selfextractor dialog and\n\tenables a change of the extract to folder.")
#else  //EXT_VER
STRING(STR_ERROR_TEMPCREATE, "Unable to create temporary file: ")
STRING(STR_ERROR_GETMODULENAME, "Unable to get module name: ")
STRING(STR_ERROR_SPAWNPROCESS, "Unable to spawn new process: ")
STRING(STR_INSERT_NEXT_DISK, "Please insert disk number %d.\nPress OK when ready.")
STRING(STR_ERROR_GETWD, "Unable to get Windows directory: ")
STRING(STR_ADVICE2, "\nNote: You can run selfextractor with the parameter \'-t\' and specify the folder for temporary files by yourself.")
STRING(STR_PARAMTERS, "Command line parameters:\n\n"
                      "-s\t\tSafe mode. Displays main selfextractor dialog and\n"
                      "\t\tenables a change of the extract to folder.\n"
                      "-t [path]\t\tStore temporary files in the specified folder.\n"
                      "-a [archive]\tExtract archive.\n"
                      "\t\tis the last archive volume.\n\n"
                      "Note: Long path names should be enclosed in double quotes.")
STRING(STR_ERROR_READMAPPED, "Unable to read from source file.")
STRING(STR_PASSWORDDLGTITLE, "Password needed")
#endif //EXT_VER

//STRING(STR_NOTSTARTED, "")
