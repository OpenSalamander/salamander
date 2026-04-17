// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ****************************************************************************
// TDirectArray2:
//  -array that grows/shrinks dynamically in blocks (occupied memory does not need
//   to be reallocated; only another block is added)
//  -when an element is deleted from the array, Destructor(element_index) is called,
//   which does nothing in the base class

template <class DATA_TYPE>
class TDirectArray2
{
protected:
    DATA_TYPE** Blocks; // pointer to the block array
    int BlockSize;      // block size

public:
    int Count; // number of elements in the array

    TDirectArray2<DATA_TYPE>(int blockSize)
    {
        Blocks = NULL;
        BlockSize = blockSize;
        Count = 0;
    }

    virtual ~TDirectArray2() { Destroy(); };

    virtual void Destructor(int) {}

    void Destroy();                    // clears the array
    BOOL Add(const DATA_TYPE& member); // adds an element at the end
    BOOL Delete(int index);            // deletes the element at the given position;
                                       // moves the last element into its place and shrinks the array
                                       /*
                                        * this function is never called, but without it
                                        * MSVC does terrible things
                                        */
    DATA_TYPE& operator[](int index)   // returns the element at the given index
    {
        return Blocks[index / BlockSize][index % BlockSize];
    }
};

// ****************************************************************************
// CArray2:
//  -base class for all indirect arrays
//  -stores (void *) values in a DWORD array (to save space in the .exe)

class CArray2 : public TDirectArray2<DWORD_PTR>
{
protected:
    BOOL DeleteMembers;

public:
    CArray2(int blockSize, BOOL deleteMembers) : TDirectArray2<DWORD_PTR>(blockSize)
    {
        DeleteMembers = deleteMembers;
    }

    BOOL Add(const void* member)
    {
        return TDirectArray2<DWORD_PTR>::Add((DWORD_PTR)member);
    }

protected:
    virtual void Destructor(void* member) = 0;
    virtual void Destructor(int i) { Destructor((void*)Blocks[i / BlockSize][i % BlockSize]); }
};

// ****************************************************************************
// TIndirectArray2:
//  -suitable for storing pointers to objects
//  -for other properties, see CArray

template <class DATA_TYPE>
class TIndirectArray2 : public CArray2
{
public:
    TIndirectArray2(int blockSize, BOOL deleteMembers = TRUE) : CArray2(blockSize, deleteMembers) {}

    DATA_TYPE*& At(int index)
    {
        return (DATA_TYPE*&)(this->CArray::operator[](index));
    }

    DATA_TYPE*& operator[](int index)
    {
        return (DATA_TYPE*&)(CArray2::operator[](index));
    }

    virtual ~TIndirectArray2() { Destroy(); }

protected:
    virtual void Destructor(void* member)
    {
        if (DeleteMembers && member != NULL)
            delete ((DATA_TYPE*)member);
    }
};

// ****************************************************************************
//
// TDirectArray2
//

template <class DATA_TYPE>
void TDirectArray2<DATA_TYPE>::Destroy()
{
    if (Count)
    {
        for (int i = 0; i < Count; i++)
            Destructor(i);

        //Count == BlockSize caused problems,
        //so Count - 1 is used here
        for (DATA_TYPE** block = Blocks; block <= Blocks + (Count - 1) / BlockSize; block++)
        {
            free(*block);
        }
        free(Blocks);
        Blocks = NULL;
        Count = 0;
    }
}

template <class DATA_TYPE>
int TDirectArray2<DATA_TYPE>::Add(const DATA_TYPE& member)
{
    //  CALL_STACK_MESSAGE1("CDynamicArray::Add( )");
    if (!(Count % BlockSize))
    {
        DATA_TYPE** newArrayBlocks;

        newArrayBlocks = (DATA_TYPE**)realloc(Blocks, (Count / BlockSize + 1) * sizeof(DATA_TYPE*));
        if (newArrayBlocks)
        {
            Blocks = newArrayBlocks;
            Blocks[Count / BlockSize] = (DATA_TYPE*)malloc(BlockSize * sizeof(DATA_TYPE));
            if (!Blocks[Count / BlockSize])
            {
                if (!Count)
                {
                    free(Blocks);
                    Blocks = NULL;
                }
                return FALSE;
            }
        }
        else
            return FALSE;
    }
    Blocks[Count / BlockSize][Count % BlockSize] = member;
    Count++;
    return TRUE;
}

template <class DATA_TYPE>
BOOL TDirectArray2<DATA_TYPE>::Delete(int index)
{
    if (index >= Count)
        return FALSE;
    Destructor(index);
    Count--;
    Blocks[index / BlockSize][index % BlockSize] = Blocks[Count / BlockSize][Count % BlockSize];
    if (!(Count % BlockSize))
    {
        free(Blocks[Count / BlockSize]);
    }
    if (!Count)
    {
        free(Blocks);
        Blocks = NULL;
    }
    return TRUE;
}
