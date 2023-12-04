// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "../dlldefs.h"
#include "../fileio.h"
#include "compress.h"

#include "..\tar.rh"
#include "..\tar.rh2"
#include "..\lang\lang.rh"

#define MAXCODE(n) (1L << (n))

// class constructor
CCompress::CCompress(const char* filename, HANDLE file, unsigned char* buffer, unsigned long read, CQuadWord inputSize) : CZippedFile(filename, file, buffer, 0, read, inputSize), PrefixTab(NULL), SuffixTab(NULL), DecoStack(NULL)
{
    CALL_STACK_MESSAGE2("CCompress::CCompress(%s, , , )", filename);

    // pokud neprosel konstruktor parenta, balime to rovnou
    if (!Ok)
        return;

    // nemuze to byt compress archiv, pokud mame min nez identifikaci...
    if (DataEnd - DataStart < 3)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // pokud neni spravne "magicke cislo" na zacatku, nejde o compress
    if (((unsigned short*)DataStart)[0] != LZW_MAGIC)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // prvni byte za magic je info o archivu, precteme ho
    unsigned char info = ((unsigned char*)DataStart)[2];
    // ostatni bity jsou reserved
    if ((info & RESERVED_MASK) != 0)
    {
        ErrorCode = IDS_GZERR_BADFLAGS;
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // nacti informace o archivu
    BlockMode = (info & BLCKMODE_MASK) != 0;
    MaxBitsNumber = info & BITNUM_MASK;
    if (MaxBitsNumber > BITS)
    {
        ErrorCode = IDS_GZERR_BADFLAGS;
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // mame archiv, potvrdime precteny zacatek
    FReadBlock(3);
    if (!Ok)
    {
        FreeBufAndFile = FALSE;
        return;
    }

    // a nainicializujeme promenne pro dekompresi
    BitsNumber = INIT_BITS;
    MaxMaxCode = MAXCODE(MaxBitsNumber);
    MaxCode = MAXCODE(BitsNumber) - 1;
    BitMask = MAXCODE(BitsNumber) - 1;
    FreeEnt = BlockMode ? FIRST : 256;
    Finished = FALSE;
    OldCode = -1;
    FInChar = 0;
    ReadyBits = 0;
    InputData = 0;
    UsedBytes = 0;
    // naalokuj tabulky
    PrefixTab = (unsigned short*)malloc(MAXCODE(BITS) * sizeof(unsigned short));
    SuffixTab = (unsigned char*)malloc(MAXCODE(BITS) * sizeof(unsigned char));
    DecoStack = (unsigned char*)malloc(MAXCODE(BITS) * sizeof(char));
    if (PrefixTab == NULL || SuffixTab == NULL || DecoStack == NULL)
    {
        Ok = FALSE;
        ErrorCode = IDS_ERR_MEMORY;
        FreeBufAndFile = FALSE;
        return;
    }
    StackTop = DecoStack + MAXCODE(BITS) - 1;
    StackPtr = StackTop;
    // a nainicializuj je
    memset(PrefixTab, 0, 256);
    int i;
    for (i = 0; i < 256; i++)
        SuffixTab[i] = (unsigned char)i;

    // hotovo
}

// class cleanup
CCompress::~CCompress()
{
    CALL_STACK_MESSAGE1("CCompress::~CCompress()");
    if (PrefixTab != NULL)
        free(PrefixTab);
    if (SuffixTab != NULL)
        free(SuffixTab);
    if (DecoStack != NULL)
        free(DecoStack);
}

BOOL CCompress::DecompressBlock(unsigned short needed)
{
    // nejprve zjistime, jestli nam na stacku nezbylo neco od minula
    unsigned long cnt = (unsigned long)(StackTop - StackPtr);
    if (cnt > 0)
    {
        // do vystupniho bufferu vykopirujeme stack
        if (ExtrEnd + cnt >= Window + BUFSIZE)
        {
            // je toho vic, nez mame mista, pustime ven jen co se vejde
            if (cnt + ExtrEnd > Window + BUFSIZE)
                cnt = (unsigned long)(Window + BUFSIZE - ExtrEnd);
            memcpy(ExtrEnd, StackPtr, cnt);
            ExtrEnd += cnt;
            StackPtr += cnt;
            // mame plny buffer, muzem si dat padla
            return TRUE;
        }
        else
        {
            // vykopirujeme stack na vystup
            memcpy(ExtrEnd, StackPtr, cnt);
            // a posuneme ukazatel vystupu zase na volne misto
            ExtrEnd += cnt;
        }
    }
    // v bufferu je porad misto, pokracujem normalnim postupem
    unsigned long code;
    for (;;)
    {
        // pokud nam uz nestaci maximalni velikost kodu, musime pridat bity
        if (FreeEnt > MaxCode)
        {
            if (UsedBytes)
            {
                if (FReadBlock(BitsNumber - UsedBytes) == NULL)
                    if (IsOk())
                        Finished = TRUE;
                    else
                        return FALSE;
                UsedBytes = 0;
            }
            BitsNumber++;
            if (BitsNumber >= MaxBitsNumber)
            {
                // MaxBitsNumber=9 needs special handling because there (unlike the other cases)
                // (BitsNumber >= MaxBitsNumber) gets triggered already on first codeword size increase
                // and thus MaxCode may still be equal MaxMaxCode-1
                if ((BitsNumber > MaxBitsNumber) && ((MaxMaxCode > 512) || (MaxCode > 511)))
                {
                    Ok = FALSE;
                    ErrorCode = IDS_ERR_CORRUPT;
                    return FALSE;
                }
                MaxCode = MaxMaxCode;
            }
            else
                MaxCode = MAXCODE(BitsNumber) - 1;
            BitMask = MAXCODE(BitsNumber) - 1;
        }
        // zasobnik nainicializujeme na zacatek (vyprazdnime ho)
        StackPtr = StackTop;
        // dopln ze vstupniho bufferu kod na delku BitsNumber bitu
        while (ReadyBits < BitsNumber && !Finished)
        {
            InputData |= FReadByte() << ReadyBits;
            if (!IsOk())
                if (ErrorCode == IDS_ERR_EOF)
                {
                    Finished = TRUE;
                    Ok = TRUE;
                    ErrorCode = 0;
                }
                else
                    return FALSE;
            else
            {
                ReadyBits += 8;
                UsedBytes++;
                if (UsedBytes == BitsNumber)
                    UsedBytes = 0;
            }
        }
        // pokud nemame z ceho rozbalovat, skoncili jsme...
        if (ReadyBits < BitsNumber)
            return (ExtrEnd - ExtrStart >= needed);
        code = InputData & BitMask;
        InputData >>= BitsNumber;
        ReadyBits -= BitsNumber;

        // prvni znak zpracujeme mimo tabulky
        if (OldCode == -1)
        {
            // prvni kod musi byt cisty znak
            if (code >= 256)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_CORRUPT;
                return FALSE;
            }
            // posli precteny kod na vystup
            OldCode = code;
            FInChar = (unsigned char)code;
            *ExtrEnd++ = (unsigned char)code;
            // pokud jsme na konci bufferu, dame si padla, dokud nebudou treba dalsi data
            if (ExtrEnd >= Window + BUFSIZE)
                return (ExtrEnd - ExtrStart >= needed);
            // a jdi pro dalsi kod ze vstupu
            continue;
        }
        // pokud jedeme v blokovem modu a prisel znak CLEAR, zrusime
        //   vytvorene tabulky a zacnem znova
        if (code == CLEAR && BlockMode)
        {
            memset(PrefixTab, 0, 256);
            FreeEnt = FIRST;
            if (UsedBytes)
            {
                if (FReadBlock(BitsNumber - UsedBytes) == NULL)
                    if (IsOk())
                        Finished = TRUE;
                    else
                        return FALSE;
                UsedBytes = 0;
            }
            ReadyBits = 0;
            OldCode = -1;
            BitsNumber = INIT_BITS;
            MaxCode = MAXCODE(BitsNumber) - 1;
            BitMask = MAXCODE(BitsNumber) - 1;
            continue;
        }
        // zapamatujeme si nacteny kod
        unsigned long incode = code;

        // Special case for KwKwK string
        if (code >= FreeEnt)
        {
            // pokud jsme dostali kod, ktery jeste neni v tabulce, nastal problem
            if (code > FreeEnt)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_CORRUPT;
                return FALSE;
            }
            // ulozime na stack znak vybaleny jako prvni (skutecne nacteny, jen prekodovany)
            //   v minulem kole
            StackPtr--;
            if (StackPtr < DecoStack)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_INTERNAL;
                return FALSE;
            }
            *StackPtr = FInChar;
            // obnovime kod nacteny v minulem kole
            code = OldCode;
        }
        // pokud slo o retezec, ktery uz mame v tabulce, vyexpandujeme ho
        while (code >= 256)
        {
            // nahazej znaky do zasobniku v opacnem poradi
            // v SuffixTab jsou dekompresene znaky
            StackPtr--;
            if (StackPtr < DecoStack)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_INTERNAL;
                return FALSE;
            }
            *StackPtr = SuffixTab[code];
            // v PrefixTab jsou kody pro zretezeni
            code = PrefixTab[code];
        }
        // hodime na stack i posledni znak
        FInChar = SuffixTab[code];
        StackPtr--;
        if (StackPtr < DecoStack)
        {
            Ok = FALSE;
            ErrorCode = IDS_ERR_INTERNAL;
            return FALSE;
        }
        *StackPtr = FInChar;
        // prihodime kod do dekompresnich tabulek
        if (FreeEnt < MaxMaxCode)
        {
            PrefixTab[FreeEnt] = (unsigned short)OldCode;
            SuffixTab[FreeEnt] = FInChar;
            FreeEnt++;
        }
        // uloz predchozi kod pro pristi kolo
        OldCode = incode;
        // pocet znaku na stacku k vyhozeni na vystup
        unsigned long cnt2 = (unsigned long)(StackTop - StackPtr);
        // a do vystupniho bufferu je ulozime ve spravnem poradi
        if (ExtrEnd + cnt2 >= Window + BUFSIZE)
        {
            // je toho vic, nez mame mista, pustime ven jen co se vejde
            if (cnt2 + ExtrEnd > Window + BUFSIZE)
                cnt2 = (unsigned long)(Window + BUFSIZE - ExtrEnd);
            memcpy(ExtrEnd, StackPtr, cnt2);
            ExtrEnd += cnt2;
            StackPtr += cnt2;
            // mame plny buffer, muzem si dat padla
            return TRUE;
        }
        else
        {
            // vykopirujeme stack na vystup
            memcpy(ExtrEnd, StackPtr, cnt2);
            // a posuneme ukazatel vystupu zase na volne misto
            ExtrEnd += cnt2;
        }
    }
}
