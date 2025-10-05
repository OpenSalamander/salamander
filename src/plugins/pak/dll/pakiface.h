// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//flags for HandleError
#define HE_RETRY 0x01

//modes for opening the PAK
#define OP_READ_MODE GENERIC_READ
#define OP_WRITE_MODE (GENERIC_READ | GENERIC_WRITE)

#define PAK_MAXPATH 256

class CPakCallbacksAbstract
{
public:
    //returns TRUE if the processing should continue, or FALSE
    //if the operation should finish
    virtual BOOL HandleError(DWORD flags, int errorID, va_list arglist) = 0;

    //reads data from the output file
    //called while packing files
    //returns TRUE when the operation should continue
    virtual BOOL Read(void* buffer, DWORD size) = 0;

    //writes data to the output file
    //called while extracting files
    //returns TRUE when the operation should continue
    virtual BOOL Write(void* buffer, DWORD size) = 0;

    //reports on the progress of data processing
    //adds the 'size' amount
    //returns TRUE when the operation should continue
    virtual BOOL AddProgress(unsigned size) = 0;

    //reports on ongoing deletion
    virtual BOOL DelNotify(const char* fileName, unsigned fileProgressTotal) = 0;
};

class CPakIfaceAbstract
{
public:
    //initializes the interface, can be called at any time, but no PAK file may be open
    //no PAK file
    //returns TRUE on success
    virtual BOOL Init(CPakCallbacksAbstract* callbacks) = 0;

    //opens a PAK file for further processing
    //'mode' specifies whether it should be opened for reading, writing, or both
    //combination of the PAK_READ_MODE and PAK_WRITE_MODE flags
    //returns TRUE on success
    virtual BOOL OpenPak(const char* fileName, DWORD mode) = 0;

    //closes the PAK file
    virtual BOOL ClosePak() = 0;

    virtual BOOL GetPakTime(FILETIME* lastWrite) = 0;

    //positions to the first file in the PAK directory
    virtual BOOL GetFirstFile(char* fileName, DWORD* size) = 0;

    //moves to the next file in the PAK directory
    virtual BOOL GetNextFile(char* fileName, DWORD* size) = 0;

    //finds a file in the archive and positions to it
    virtual BOOL FindFile(const char* fileName, DWORD* size) = 0;

    //extracts a file from the archive
    virtual BOOL ExtractFile() = 0;

    //marks a file for deletion
    virtual BOOL MarkForDelete() = 0;

    //returns the total progress size for the delete operation
    virtual BOOL GetDelProgressTotal(unsigned* progressSize) = 0;

    //deletes the marked files
    virtual BOOL DeleteFiles(BOOL* needOptim) = 0;

    //initializes adding files
    virtual BOOL StartAdding(unsigned count) = 0;

    //adds a file to the archive
    //'fileName' is the name under which the file should be stored in the archive
    //'size' is its size
    virtual BOOL AddFile(const char* fileName, DWORD size) = 0;

    //writes the new PAK directory to the file
    //must be called after adding new files to the PAK
    virtual BOOL FinalizeAdding() = 0;

    //retrieves information about space usage in the PAK
    virtual void GetOptimizedState(DWORD* pakSize, DWORD* validData) = 0;

    //initializes optimization
    virtual BOOL InitOptimalization(unsigned* progressTotal) = 0;

    //optimizes the PAK
    virtual BOOL OptimizePak() = 0;

    //creates a message from the parameters passed to 'HandleError()'
    virtual char* FormatMessage(char* buffer, int errorID, va_list arglist) = 0;
};

#ifdef PAK_DLL
//obtain pak interface object
typedef CPakIfaceAbstract*(WINAPI* FPAKGetIFace)();

//frees pak interface object
typedef void(WINAPI* FPAKReleaseIFace)(CPakIfaceAbstract* pakIFace);

#else // PAK_DLL

//obtain pak interface object
CPakIfaceAbstract* WINAPI PAKGetIFace();

//frees pak interface object
void WINAPI PAKReleaseIFace(CPakIfaceAbstract* pakIFace);

#endif // PAK_DLL
