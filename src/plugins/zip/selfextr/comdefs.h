// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// WARNING: cannot be replaced by "#pragma once" because it is included from .rc file and it seems resource compiler does not support "#pragma once"
#ifndef __COMDEFS_H
#define __COMDEFS_H

//common definitions for both - zip and self-extractor

#pragma pack(push)
#pragma pack(1)

typedef struct
{
    unsigned Signature;
    int HeaderSize; //size of header
    unsigned Flags;
    unsigned EOCentrDirOffs;
    unsigned ArchiveSize; //size of zip archive following this header
    short CommandOffs;    //leght of command line string following this structure, include terminating '\0'
    short TextOffs;
    short TitleOffs;
    short SubDirOffs;
    short AboutOffs;
    short ExtractBtnTextOffs;
    short VendorOffs;        //jmeno firmy
    short WWWOffs;           //www link
    short ArchiveNameOffs;   //name of first archive volume
    short TargetDirSpecOffs; //0 or environment variable or regitry valaue
    UINT MBoxStyle;
    short MBoxTitleOffs;
    short MBoxTextOffs;
    int WaitForOffs; // tohle nemuze byt short, MBoxText muze byt hodne dlouhy
    //char          Command[];
    //char          Text[];
    //char          Title[];
    //char          SubDir[];
    //char          About[];
    //char          ExtractBtnText[];
    //char          Vendor[];
    //char          WWWLink[];
    //char          ArchiveName[];
    //char          TargetDirSpec[];
    //char          MBoxTitle[];
    //char          MBoxText[];
} CSelfExtrHeader;

#pragma pack(pop)

#define SELFEXTR_SIG 0x45537353

//#define SE_CURRENTDIR         0x01 //extract files to current directory
#define SE_TEMPDIR 0x0001        //extract files to temporarry directory, set iff SE_TEMPDIREX is set
#define SE_NOTALLOWCHANGE 0x0002 //preserve user from changing target diretory
#define SE_REMOVEAFTER 0x0004    //delete files after extraction
#define SE_AUTO 0x0008           //start extraction automatically
#define SE_SHOWSUMARY 0x0010     //displays report of successfully extracted files
#define SE_HIDEMAINDLG 0x0020
#define SE_OVEWRITEALL 0x0040
#define SE_MULTVOL 0x0080
#define SE_SEQNAMES 0x0100
#define SE_AUTODIR 0x0200       //automatically create target directory
#define SE_REQUIRESADMIN 0x0400 //requires administrative privileges
#define SE_DIRMASK 0xF000
#define SE_CURDIR 0x0000    //extract files to current directory
#define SE_TEMPDIREX 0x1000 //extract files to temporarry directory
#define SE_WINDIR 0x2000    //extract files to windows directory
#define SE_SYSDIR 0x3000    //extract files to system directory
#define SE_REGVALUE 0x4000  //extract files to directory specified by registry value
#define SE_ENVVAR 0x5000    //extract files to directory specified by system variable

// typy long message dialogu
#define SE_LONGMESSAGE 0x80000000
#define SE_MBOK 0x80000000
#define SE_MBOKCANCEL 0x80000001
#define SE_MBYESNO 0x80000002
#define SE_MBAGREEDISAGREE 0x80000004

#define SFX_BIGEXE_FILENAME "sfxbig.exe"
#define SFX_SMALLEXE_FILENAME "sfxsmall.exe"

#define SE_MAX_TEXT 256
#define SE_MAX_COMMANDLINE 1024
#define SE_MAX_TITLE 256
#define SE_MAX_ABOUT 1024
#define SE_MAX_EXTRBTN 32
#define SE_MAX_MBOXTEXT (-1)
#define SE_MAX_VENDOR 128
#define SE_MAX_WWW 128

#define SE_IDI_ICON 500
#define SE_IDD_SFXDIALOG 100

#endif // __COMDEFS_H
