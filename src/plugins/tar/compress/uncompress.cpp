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

    // if the parent constructor failed, bail out immediately
    if (!Ok)
        return;

    // this cannot be a compress archive if we have less than the identifier...
    if (DataEnd - DataStart < 3)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // if the "magic number" at the start is wrong, it is not a compress archive
    if (((unsigned short*)DataStart)[0] != LZW_MAGIC)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // the first byte after the magic value stores archive info; read it
    unsigned char info = ((unsigned char*)DataStart)[2];
    // the remaining bits are reserved
    if ((info & RESERVED_MASK) != 0)
    {
        ErrorCode = IDS_GZERR_BADFLAGS;
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // load archive parameters
    BlockMode = (info & BLCKMODE_MASK) != 0;
    MaxBitsNumber = info & BITNUM_MASK;
    if (MaxBitsNumber > BITS)
    {
        ErrorCode = IDS_GZERR_BADFLAGS;
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // a valid archive is present; confirm the consumed header
    FReadBlock(3);
    if (!Ok)
    {
        FreeBufAndFile = FALSE;
        return;
    }

    // initialize the variables used for decompression
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
    // allocate the tables
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
    // and initialize them
    memset(PrefixTab, 0, 256);
    int i;
    for (i = 0; i < 256; i++)
        SuffixTab[i] = (unsigned char)i;

    // done
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
    // first determine whether the stack still contains leftover data
    unsigned long cnt = (unsigned long)(StackTop - StackPtr);
    if (cnt > 0)
    {
        // copy the stack contents to the output buffer
        if (ExtrEnd + cnt >= Window + BUFSIZE)
        {
            // if there is more than fits, write out only what we can store
            if (cnt + ExtrEnd > Window + BUFSIZE)
                cnt = (unsigned long)(Window + BUFSIZE - ExtrEnd);
            memcpy(ExtrEnd, StackPtr, cnt);
            ExtrEnd += cnt;
            StackPtr += cnt;
            // the buffer is full; pause until it is consumed
            return TRUE;
        }
        else
        {
            // copy the stack to the output
            memcpy(ExtrEnd, StackPtr, cnt);
            // and advance the output pointer to the free space again
            ExtrEnd += cnt;
        }
    }
    // there is still space in the buffer; continue with the normal workflow
    unsigned long code;
    for (;;)
    {
        // if the current maximum code size is insufficient, increase the bit width
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
        // reset the stack pointer (effectively emptying it)
        StackPtr = StackTop;
        // extend the input code to BitsNumber bits from the input buffer
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
        // if there is nothing left to decompress, we are finished...
        if (ReadyBits < BitsNumber)
            return (ExtrEnd - ExtrStart >= needed);
        code = InputData & BitMask;
        InputData >>= BitsNumber;
        ReadyBits -= BitsNumber;

        // process the first symbol outside the tables
        if (OldCode == -1)
        {
            // the first code must be a literal character
            if (code >= 256)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_CORRUPT;
                return FALSE;
            }
            // emit the decoded code to the output
            OldCode = code;
            FInChar = (unsigned char)code;
            *ExtrEnd++ = (unsigned char)code;
            // if we reached the end of the buffer, pause until more data is needed
            if (ExtrEnd >= Window + BUFSIZE)
                return (ExtrEnd - ExtrStart >= needed);
            // and fetch the next code from the input
            continue;
        }
        // if we are in block mode and receive the CLEAR code, reset the tables
        //   and start over
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
        // remember the code we just read
        unsigned long incode = code;

        // Special case for KwKwK string
        if (code >= FreeEnt)
        {
            // if we received a code that is not yet in the table, something went wrong
            if (code > FreeEnt)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_CORRUPT;
                return FALSE;
            }
            // push onto the stack the first character decoded in the previous iteration
            StackPtr--;
            if (StackPtr < DecoStack)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_INTERNAL;
                return FALSE;
            }
            *StackPtr = FInChar;
            // restore the code read in the previous loop iteration
            code = OldCode;
        }
        // if the code references a string that is already in the table, expand it
        while (code >= 256)
        {
            // push the characters onto the stack in reverse order
            // SuffixTab stores the decompressed characters
            StackPtr--;
            if (StackPtr < DecoStack)
            {
                Ok = FALSE;
                ErrorCode = IDS_ERR_INTERNAL;
                return FALSE;
            }
            *StackPtr = SuffixTab[code];
            // PrefixTab stores the codes used for concatenation
            code = PrefixTab[code];
        }
        // push the final character onto the stack as well
        FInChar = SuffixTab[code];
        StackPtr--;
        if (StackPtr < DecoStack)
        {
            Ok = FALSE;
            ErrorCode = IDS_ERR_INTERNAL;
            return FALSE;
        }
        *StackPtr = FInChar;
        // add the new code to the decompression tables
        if (FreeEnt < MaxMaxCode)
        {
            PrefixTab[FreeEnt] = (unsigned short)OldCode;
            SuffixTab[FreeEnt] = FInChar;
            FreeEnt++;
        }
        // store the previous code for the next iteration
        OldCode = incode;
        // determine how many characters are on the stack to write to the output
        unsigned long cnt2 = (unsigned long)(StackTop - StackPtr);
        // and store them in the output buffer in the proper order
        if (ExtrEnd + cnt2 >= Window + BUFSIZE)
        {
            // if more data exists than space allows, write only what fits
            if (cnt2 + ExtrEnd > Window + BUFSIZE)
                cnt2 = (unsigned long)(Window + BUFSIZE - ExtrEnd);
            memcpy(ExtrEnd, StackPtr, cnt2);
            ExtrEnd += cnt2;
            StackPtr += cnt2;
            // the buffer is full; pause until it is consumed
            return TRUE;
        }
        else
        {
            // copy the stack to the output
            memcpy(ExtrEnd, StackPtr, cnt2);
            // and advance the output pointer to the next free position
            ExtrEnd += cnt2;
        }
    }
}
