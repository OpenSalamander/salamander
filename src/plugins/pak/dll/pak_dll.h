// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define IOBUFSIZE (16 * 1024)
#define PAK_UPDIR "__$UPDIR" //alias to ".."

#pragma pack(push)
#pragma pack(1)

struct CPackHeader
{
    DWORD Pack;      // Identify PACK file format
    DWORD DirOffset; // Position of PACK directory from start of file
    DWORD DirSize;   // Number of entries * 0x40
};

struct CPackEntry
{
    char FileName[0x38]; // Name of the file, Unix style, with extension,
                         // 50 chars, padded with '\0'.
    DWORD Offset;        // Position of the entry in PACK file
    DWORD Size;          // Size of the entry in PACK file
};

#pragma pack(pop)

struct CDelRegion
{
    char* Description;
    DWORD Offset;
    DWORD Size;
    char* FileName;

    CDelRegion(const char* descript, DWORD offset, DWORD size, const char* fileName);
    ~CDelRegion();
};

char* LoadStr(int resID);
void FreeRegion(void* region);

class CPakIface : public CPakIfaceAbstract
{
    CPakCallbacksAbstract* Callbacks;
    HANDLE PakFile;
    DWORD PakSize;
    BOOL EmptyPak;
    CPackHeader Header;
    CPackEntry* PakDir;
    unsigned DirSize;
    unsigned DirPos;

    //add_del
    TIndirectArray2<CDelRegion> DelRegions; //(256, FreeRegion);
    TIndirectArray2<CDelRegion> ZeroSizedFiles;
    DWORD AddPos;

public:
    CPakIface();

    //~CPakObject();

    //initializes the interface, can be called at any time, but no PAK file may be open
    //no PAK file
    //returns TRUE on success
    virtual BOOL Init(CPakCallbacksAbstract* callbacks);

    //opens a PAK file for further processing
    //returns TRUE on success
    virtual BOOL OpenPak(const char* fileName, DWORD mode);

    //closes the PAK file
    virtual BOOL ClosePak();

    virtual BOOL GetPakTime(FILETIME* lastWrite);

    //positions to the first file in the PAK directory
    //returns NULL if the archive is empty
    //returns the file name
    virtual BOOL GetFirstFile(char* fileName, DWORD* size);

    //moves to the next file in the PAK directory
    //returns NULL at the end of the archive
    //returns the file name
    virtual BOOL GetNextFile(char* fileName, DWORD* size);

    //finds a file in the archive and positions to it
    virtual BOOL FindFile(const char* fileName, DWORD* size);

    //extracts a file from the archive
    virtual BOOL ExtractFile();

    //marks a file for deletion
    virtual BOOL MarkForDelete();

    //returns the total progress size for the delete operation
    virtual BOOL GetDelProgressTotal(unsigned* progressSize);

    //deletes the marked files
    virtual BOOL DeleteFiles(BOOL* needOptim);

    //initializes adding files
    virtual BOOL StartAdding(unsigned count);

    //adds a file to the archive
    //'fileName' is the name under which the file should be stored in the archive
    //'size' is its size
    virtual BOOL AddFile(const char* fileName, DWORD size);

    //writes the new PAK directory to the file
    //must be called after adding new files to the PAK
    virtual BOOL FinalizeAdding();

    //retrieves information about space usage in the PAK
    virtual void GetOptimizedState(DWORD* pakSize, DWORD* validData);

    //initializes optimization
    virtual BOOL InitOptimalization(unsigned* progressTotal);

    //optimizes the PAK
    virtual BOOL OptimizePak();

    //creates a message from the parameters passed to 'HandleError()'
    virtual char* FormatMessage(char* buffer, int errorID, va_list arglist);

protected:
    BOOL HandleError(DWORD flags, int errorID, ...);

    char* LastErrorString(int lastError, char* buffer);

    BOOL SafeSeek(HANDLE file, DWORD position);

    BOOL SafeRead(HANDLE file, void* buffer, DWORD size);

    BOOL SafeWrite(HANDLE file, void* buffer, DWORD size);

    BOOL GetName(const char* nameInPak, char* outName);

    void UpdateDir(CDelRegion* region, DWORD topOffset, DWORD delta);

    BOOL DeleteZeroSized();

    BOOL MoveData(DWORD writePos, DWORD readPos, DWORD size, BOOL* userBreak);
};
