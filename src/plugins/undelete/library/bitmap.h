// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// **************************************************************************************
//
//   CClusterBitmap - transformed bitmap of clusters
//
//   Each bit from the original cluster bitmap is represented in two bits.
//   Less significant bit is stored in bit array BitmapPartsL and more
//   significant bit in BitmapPartsH.
//

#define BITMAP_PART_SIZE (5 * 1000 * 1024) // keep this const DWORD aligned!

class CClusterBitmap
{
public:
    CClusterBitmap()
        : BitmapPartsL(10, 10), BitmapPartsH(10, 10)
    {
        AllocatedSize = 0;
        CurrentSize = 0;
    }

    ~CClusterBitmap()
    {
        Free();
    }

    BOOL AllocateBitmap(QWORD size)
    {
        if (BitmapPartsL.Count != 0)
            TRACE_E("CClusterBitmap::AllocateBitmap(): BitmapPartsL.Count != 0");
        for (QWORD i = 0; i < (size + BITMAP_PART_SIZE - 1) / BITMAP_PART_SIZE; i++)
        {
            DWORD partSize = (DWORD)min(BITMAP_PART_SIZE, size - i * BITMAP_PART_SIZE);
            BYTE* partL = new BYTE[partSize];
            BYTE* partH = new BYTE[partSize];
            if (partL == NULL || partH == NULL)
            {
                TRACE_E("CClusterBitmap::AllocateBitmap(): Low memory!");
                Free();
                return FALSE;
            }
            BitmapPartsL.Add(partL);
            BitmapPartsH.Add(partH);
            if (!BitmapPartsL.IsGood() || !BitmapPartsH.IsGood())
            {
                TRACE_E("CClusterBitmap::AllocateBitmap(): Low memory!");
                Free();
                return FALSE;
            }
        }
        AllocatedSize = size;
        return TRUE;
    }

    void Free()
    {
        int i;
        for (i = 0; i < BitmapPartsL.Count; i++)
            delete[] BitmapPartsL[i];
        for (i = 0; i < BitmapPartsH.Count; i++)
            delete[] BitmapPartsH[i];
        BitmapPartsL.DestroyMembers();
        BitmapPartsH.DestroyMembers();
        AllocatedSize = 0;
        CurrentSize = 0;
    }

    QWORD GetClusterCount() const { return AllocatedSize * 8; } // 8 clusters per byte

    void AddBitmapBlock(const BYTE* bitmap, QWORD bitmapSize)
    {
        if (CurrentSize + bitmapSize > AllocatedSize)
        {
            TRACE_E("CClusterBitmap::AddBitmapBlock() allocated buffer is too small!"); // this situation should not happen
            return;
        }

        const BYTE* src = bitmap;
        QWORD bytes = bitmapSize;
        while (src < bitmap + bitmapSize)
        {
            QWORD bitmapPart = CurrentSize / BITMAP_PART_SIZE;
            QWORD bitmapTail = CurrentSize - bitmapPart * BITMAP_PART_SIZE;
            BYTE* bitmapL = BitmapPartsL[(int)bitmapPart];
            BYTE* bitmapH = BitmapPartsH[(int)bitmapPart];
            size_t blockSize = (size_t)min(bytes, BITMAP_PART_SIZE - bitmapTail);
            memcpy(bitmapL + bitmapTail, src, blockSize);
            memcpy(bitmapH + bitmapTail, src, blockSize); // expand 0b0->0b00 0b1->0b11
            src += blockSize;
            CurrentSize += blockSize;
            bytes -= blockSize;
        }
    }

    inline BOOL GetValue(QWORD lcn, BYTE* val)
    {
        QWORD a = lcn >> 3;
        if (a < CurrentSize)
        {
            QWORD bitmapPart = a / BITMAP_PART_SIZE;
            QWORD bitmapTail = a - bitmapPart * BITMAP_PART_SIZE;
            const BYTE* bitmapL = BitmapPartsL[(int)bitmapPart];
            const BYTE* bitmapH = BitmapPartsH[(int)bitmapPart];
            BYTE shift = (BYTE)(lcn & 0x07);
            BYTE mask = 1 << shift;
            BYTE valL = bitmapL[bitmapTail] & mask;
            BYTE valH = bitmapH[bitmapTail] & mask;
            *val = (valL >> shift) | ((valH >> shift) << 1);
            return TRUE;
        }
        else
        {
            TRACE_E("CClusterBitmap::GetValue() attempt to access out of Bitmap range!");
            return FALSE;
        }
    }

    inline void IncreaseValue(QWORD lcn)
    {
        QWORD a = lcn >> 3;
        if (a < CurrentSize)
        {
            QWORD bitmapPart = a / BITMAP_PART_SIZE;
            QWORD bitmapTail = a - bitmapPart * BITMAP_PART_SIZE;
            BYTE* bitmapL = BitmapPartsL[(int)bitmapPart];
            BYTE* bitmapH = BitmapPartsH[(int)bitmapPart];
            BYTE shift = (BYTE)(lcn & 0x07);
            BYTE mask = 1 << shift;
            BYTE valL = bitmapL[bitmapTail] & mask;
            BYTE valH = bitmapH[bitmapTail] & mask;
            // 0->1 || 1->2
            if (valH == 0)
            {
                if (valL == 0)
                {
                    bitmapL[bitmapTail] |= mask; // 0->1
                }
                else
                {
                    bitmapL[bitmapTail] &= ~mask; // 1->2
                    bitmapH[bitmapTail] |= mask;
                }
            }
        }
        else
        {
            TRACE_E("CClusterBitmap::IncreaseValue() attempt to access out of Bitmap range!");
        }
    }

protected:
    TDirectArray<BYTE*> BitmapPartsL; // with bitmap divided into multiple parts we have better
    TDirectArray<BYTE*> BitmapPartsH; // chance in x86 process to allocate memory
    QWORD AllocatedSize;              // allocated size of Bitmap in bytes
    QWORD CurrentSize;                // current number of bytes added with AddBitmapBlock()
};
