// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef EXT_VER
#ifndef __EXTENDED_H

#define ENCRYPT_HEADER_SIZE 12
#define MAX_PASSWORD 256

typedef struct
{
    const char* File;
    char* Password;
} CPasswordDlgInfo;

extern char* RealArchiveFile;
extern char RealArchiveName[];
extern HANDLE RAFile;
extern HANDLE RAMapping;
extern const unsigned char* RAData;
extern unsigned long RASize;
extern int DiskNum;
extern CEOCentrDirRecord* ECRec;
extern CFileHeader* CentrDir;
extern int MVMode;
extern __UINT32 Keys[3];
extern bool Encrypted;
extern bool SearchLastFile;

const char* ReadPath(const char* cmdline, char* path);
int DetachArchive(char* sfxBat, const char* parameters);
//int OpenLastVolume();
int ReadCentralDirectory();
int ChangeDisk(int number, BOOL firstVolume);
BOOL WINAPI PasswordDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
int decrypt_byte();
int update_keys(int c);
int InitKeys(const char* password, const char* header, char check);

extern BOOL WaitingForFirst;
extern BOOL EnumProcOK;

BOOL EnumProcesses(HANDLE* processHandle);

#define __EXTENDED_H
#endif //__EXTENDED_H
#endif //EXT_VER
