// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/*
int MatchFiles(CDynamicArray *namesArray, CExtractInfo *info);
int FindFile(const char *name, CFileInfo * fileInfo, CExtractInfo *info);
int PrepareMaskArray(CDynamicArray * maskArray, const char * masks, 
                     CExtractInfo * info);
int MatchFilesToMask(CDynamicArray * maskArray, CExtractInfo * info);
void InflateFreeFixedHufman(CExtractInfo *info);
int ExtractFiles(const char *targetDir, CExtractInfo *info);
int ExtractSingleFile(char * targetDir, int targetDirLen,
                      CFileInfo * fileInfo, CExtractInfo * info);
*/
class CZipUnpack : public CZipCommon
{
public:
    TIndirectArray2<CFileInfo>* ExtrFiles; //files to be extracted
    int DialogFlags;
    bool SkipAllIOErrors;
    bool SkipAllLongNames;
    bool SkipAllEncrypted;
    bool SkipAllDataErr;
    bool SkipAllBadMathods;
    DWORD Silent;
    char* FileNameDisp; //file name displayed on the screen
    HANDLE Heap;        //on this heap is allocated memory in inflate.cpp
    char* InputBuffer;
    unsigned InBufSize;
    char* SlideWindow;
    unsigned WinSize;
    //unsigned                InputPos;       //read position in zip file
    QWORD BytesLeft;   //number of copressed bytes remainig
                       //to read from zipfile
    CFile* OutputFile; //current output file
    int OutputError;
    __UINT32 Crc;
    __UINT32 Keys[3]; //decryption keys
    TIndirectArray2<char> Passwords;
    bool Encrypted;
    bool AllocateWholeFile;
    bool TestAllocateWholeFile;

    //inflate specifics
    //fixed hufman trees
    void* fixed_tl64; // !! must be NULL initialized
    void* fixed_td64; //before calling inflate
    int fixed_bl64,
        fixed_bd64;
    void* fixed_tl32; // !! must be NULL initialized
    void* fixed_td32; //before calling inflate
    int fixed_bl32,
        fixed_bd32;

    //file testing
    bool Test;
    BOOL AllFilesOK;
    QWORD ExtractedBytes; //number of bytes 'extracted' from file. Used for testing

    //unshrink
    char* OutputBuffer;
    unsigned OutBufSize;
    bool Unshrinking;

    CZipUnpack(const char* zipName, const char* zipRoot, CSalamanderForOperationsAbstract* salamander,
               TIndirectArray2<char>* archiveVolumes);

    ~CZipUnpack()
    {
        if (OutputBuffer)
            free(OutputBuffer);
        if (Heap)
            HeapDestroy(Heap);
    }

    int UnpackArchive(const char* targetDir, SalEnumSelection next, void* param);
    int UnpackOneFile(const char* nameInZip, const CFileData* fileData, const char* targetPath, const char* newFileName);
    int UnpackWholeArchive(const char* mask, const char* targetDir);

    //int EnumFiles(CDynamicArray * namesArray, SalEnumSelection next, void * param);
    //int MatchFiles(CDynamicArray *namesArray);
    int FindFile(LPCTSTR name, CFileInfo* fileInfo, int nItem);
    int PrepareMaskArray(TIndirectArray2<char>& maskArray, const char* masks);
    int MatchFilesToMask(TIndirectArray2<char>& maskArray);
    int InflateFile(CFileInfo* fileInfo, BOOL deflate64, int* errorID);
    void InflateFreeFixedHufman();
    int UnStoreFile(CFileInfo* fileInfo, int* errorID);
    int ExplodeFile(CFileInfo* fileInfo, int* errorID);
    int UnShrinkFile(CFileInfo* fileInfo, int* errorID);
    int UnReduceFile(CFileInfo* fileInfo, int* errorID);
    int UnBZIP2File(CFileInfo* fileInfo, int* errorID);
    int ExtractFiles(const char* targetDir);
    int ExtractSingleFile(char* targetDir, int targetDirLen,
                          CFileInfo* fileInfo, BOOL* success, const char* newFileName = NULL);
    int SafeRead(void* buffer, unsigned bytesToRead, bool* skipAll);
    // int SafeRead(void * buffer, unsigned bytesToRead,
    //              unsigned * bytesRead, bool * skipAll);
    int SafeCreateCFile(CFile** file, const char* fileName, const char* arcName,
                        const char* fileData, unsigned int access, unsigned int share,
                        unsigned int attributes, int flags, DWORD* silent,
                        bool* skipAll, QWORD size);
    void QuickSortHeaders2(int left, int right, TIndirectArray2<CFileInfo>& headers);
    BOOL ProgressAddSize(QWORD size);
};
