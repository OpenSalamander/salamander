// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//flagy pro handle error
#define HE_RETRY 0x01

//mody pro otevreni paku
#define OP_READ_MODE GENERIC_READ
#define OP_WRITE_MODE (GENERIC_READ | GENERIC_WRITE)

#define PAK_MAXPATH 256

class CPakCallbacksAbstract
{
public:
    //vraci TRUE v pripade ze se ma pokracovat dal, nebo FALSE
    //ma-li oprace skoncit
    virtual BOOL HandleError(DWORD flags, int errorID, va_list arglist) = 0;

    //precte data z vystupniho souboru
    //volano behem baleni souboru
    //vraci TRUE kdyz ma operace pokracovat
    virtual BOOL Read(void* buffer, DWORD size) = 0;

    //zapise data do vystupniho souboru
    //volano behem vybalovani souboru
    //vraci TRUE kdyz ma operace pokracovat
    virtual BOOL Write(void* buffer, DWORD size) = 0;

    //informuje o prubehu zpracovani dat
    //prida velikost 'size'
    //vraci kdyz ma oprace pokracovat
    virtual BOOL AddProgress(unsigned size) = 0;

    //informuje o probihajicim mazani
    virtual BOOL DelNotify(const char* fileName, unsigned fileProgressTotal) = 0;
};

class CPakIfaceAbstract
{
public:
    //inicializije interface, muze byt volana kdykoliv, ale nesmi byt otevreny
    //zadny pak soubor
    //vraci TRUE v pripade uspechu
    virtual BOOL Init(CPakCallbacksAbstract* callbacks) = 0;

    //otevre pak soubor pro dalsi zpracovani
    //'mode' specifikuje zda ma byt otevreny pro cteni ci zapis, ci oboji
    //kombinace flagu PAK_READ_MODE a PAK_WRITE_MODE
    //vraci TRUE v pripade uspechu
    virtual BOOL OpenPak(const char* fileName, DWORD mode) = 0;

    //zavre pak soubor
    virtual BOOL ClosePak() = 0;

    virtual BOOL GetPakTime(FILETIME* lastWrite) = 0;

    //nastavi se na prvni soubor v pak adresari
    virtual BOOL GetFirstFile(char* fileName, DWORD* size) = 0;

    //nastavi se nasledujici prvni soubor v pak adresari
    virtual BOOL GetNextFile(char* fileName, DWORD* size) = 0;

    //najde soubor v archivu a nastavi se na nej
    virtual BOOL FindFile(const char* fileName, DWORD* size) = 0;

    //vybali soubor z archivu
    virtual BOOL ExtractFile() = 0;

    //oznaci soubor pro smazani
    virtual BOOL MarkForDelete() = 0;

    //vrati celkovou velikost progresy pro operaci mazani
    virtual BOOL GetDelProgressTotal(unsigned* progressSize) = 0;

    //smaze oznacene soubory
    virtual BOOL DeleteFiles(BOOL* needOptim) = 0;

    //inicializuje pridavani souboru
    virtual BOOL StartAdding(unsigned count) = 0;

    //prida soubor do archivu
    //'fileName' je jmeno pod kterym ma byt soubor ulozen v archivu
    //'size' je jeho velikost
    virtual BOOL AddFile(const char* fileName, DWORD size) = 0;

    //zapise novy pak adresar do souboru
    //!!nutne volat po po pridani novych souboru do paku
    virtual BOOL FinalizeAdding() = 0;

    //zjisti informace o vyuziti mista v paku
    virtual void GetOptimizedState(DWORD* pakSize, DWORD* validData) = 0;

    //inicializuje optimalizaci
    virtual BOOL InitOptimalization(unsigned* progressTotal) = 0;

    //optimalizuje pak
    virtual BOOL OptimizePak() = 0;

    //vytvori zpravu z parametru predanych do 'HandleError()'
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
