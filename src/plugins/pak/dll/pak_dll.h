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

    //inicializije interface, muze byt volana kdykoliv, ale nesmi byt otevreny
    //zadny pak soubor
    //vraci TRUE v pripade uspechu
    virtual BOOL Init(CPakCallbacksAbstract* callbacks);

    //otevre pak soubor pro dalsi zpracovani
    //vraci TRUE v pripade uspechu
    virtual BOOL OpenPak(const char* fileName, DWORD mode);

    //zavre pak soubor
    virtual BOOL ClosePak();

    virtual BOOL GetPakTime(FILETIME* lastWrite);

    //nastavi se na prvni soubor v pak adresari
    //je-li archive prazdny vrati NULL
    //vraci jmeno souboru
    virtual BOOL GetFirstFile(char* fileName, DWORD* size);

    //nastavi se nasledujici prvni soubor v pak adresari
    //na konci archivu vrati NULL
    //vraci jmeno souboru
    virtual BOOL GetNextFile(char* fileName, DWORD* size);

    //najde soubor v archivu a nastavi se na nej
    virtual BOOL FindFile(const char* fileName, DWORD* size);

    //vybali soubor z archivu
    virtual BOOL ExtractFile();

    //oznaci soubor pro smazani
    virtual BOOL MarkForDelete();

    //vrati celkovou velikost progresy pro operaci mazani
    virtual BOOL GetDelProgressTotal(unsigned* progressSize);

    //smaze oznacene soubory
    virtual BOOL DeleteFiles(BOOL* needOptim);

    //inicializuje pridavani souboru
    virtual BOOL StartAdding(unsigned count);

    //prida soubor do archivu
    //'fileName' je jmeno pod kterym ma byt soubor ulozen v archivu
    //'size' je jeho velikost
    virtual BOOL AddFile(const char* fileName, DWORD size);

    //zapise novy pak adresar do souboru
    //!!nutne volat po po pridani novych souboru do paku
    virtual BOOL FinalizeAdding();

    //zjisti informace o vyuziti mista v paku
    virtual void GetOptimizedState(DWORD* pakSize, DWORD* validData);

    //inicializuje optimalizaci
    virtual BOOL InitOptimalization(unsigned* progressTotal);

    //optimalizuje pak
    virtual BOOL OptimizePak();

    //vytvori zpravu z parametru predanych do 'HandleError()'
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
