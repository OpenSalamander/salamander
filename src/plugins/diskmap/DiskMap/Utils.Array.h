// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
// TDirectArray2:
//  -array that dynamically grows/shrinks by blocks (no need to reallocate
//   already occupied memory, just add another block)
//  -when removing items from the array, the method Destructor(index_of_item)
//   is called, which in the base object does nothing

template <class DATA_TYPE>
class TAutoDirectArray
{
protected:
    DATA_TYPE** Blocks; // pointer to the block array
    int BlockSize;
    int _count; // number of elements in the array

public:
    TAutoDirectArray<DATA_TYPE>(int blocksize)
    {
        BlockSize = blocksize;
        Blocks = NULL;
        _count = 0;
    }
    virtual ~TAutoDirectArray() { Destroy(); };

    virtual void Delete(int) {}

    int GetCount() { return this->_count; }

    void Destroy()
    {
        if (this->_count)
        {
            for (int i = 0; i < _count; i++)
                this->Delete(i);

            //if Count == BlockSize it caused problems
            //therefore Count - 1 is used here
            //DATA_TYPE ** block = Blocks;
            for (DATA_TYPE** block = Blocks; block <= Blocks + (_count - 1) / BlockSize; block++)
            //int i;
            //for (i = 0; i <= (_count - 1)/BlockSize; i++)
            {
                free(*block);
                //block++;
            }
            free(Blocks);
            Blocks = NULL;
            _count = 0;
        }
    }
    int Add(const DATA_TYPE& member)
    {
        if (_count % BlockSize == 0) //all positions of the last block are used up
        {
            DATA_TYPE** newArrayBlocks;

            newArrayBlocks = (DATA_TYPE**)realloc(Blocks, (_count / BlockSize + 1) * sizeof(DATA_TYPE*));
            if (newArrayBlocks)
            {
                Blocks = newArrayBlocks;
                Blocks[_count / BlockSize] = (DATA_TYPE*)malloc(BlockSize * sizeof(DATA_TYPE));
                if (!Blocks[_count / BlockSize]) //allocation failed...
                {
                    if (!_count)
                    {
                        free(Blocks);
                        Blocks = NULL;
                    }
                    return -1;
                }
            }
            else
                return -1; //failed to grow the array of block pointers
        }
        Blocks[_count / BlockSize][_count % BlockSize] = member;
        return _count++;
    }

    BOOL Remove(int index) // removes the element at the given position, in its place
    {
        if (index >= _count)
            return FALSE;
        Delete(index);
        _count--;
        Blocks[index / BlockSize][index % BlockSize] = Blocks[_count / BlockSize][_count % BlockSize];
        if (!(_count % BlockSize)) //used up the last one from the current block
        {
            free(Blocks[_count / BlockSize]);
        }
        if (!_count) //used them all
        {
            free(Blocks);
            Blocks = NULL;
        }
        return TRUE;
    }
    // move the element from the last position and shrink the array
    /*
        CDynamicArray * const &operator[](float index); // function is never called, but if it is missing
        // MSVC does terrible things
	*/
    DATA_TYPE& operator[](int index) //returns the element at the position
    {
        return Blocks[index / BlockSize][index % BlockSize];
    }
};

// ****************************************************************************
// CArray2:
//  -base of all indirect arrays
//  -stores the type (void*) in a DWORD array (to save space in the .exe)

class CAutoIndirectArrayBase : public TAutoDirectArray<ULONG_PTR>
{
protected:
    BOOL DeleteMembers;

public:
    CAutoIndirectArrayBase(int blockSize, BOOL deleteMembers) : TAutoDirectArray<ULONG_PTR>(blockSize)
    {
        DeleteMembers = deleteMembers;
    }

    int Add(const void* member)
    {
        return TAutoDirectArray<ULONG_PTR>::Add((ULONG_PTR)member);
    }

    void Copy(int dst, int src)
    {
        Blocks[dst / BlockSize][dst % BlockSize] = Blocks[src / BlockSize][src % BlockSize];
    }

protected:
    virtual void Delete(void* member) = 0;
    virtual void Delete(int index) { Delete((void*)Blocks[index / BlockSize][index % BlockSize]); }
};

// ****************************************************************************
// TIndirectArray2:
//  -suitable for storing pointers to objects
//  -for other properties see CArray

template <class DATA_TYPE>
class TAutoIndirectArray : public CAutoIndirectArrayBase
{
public:
    TAutoIndirectArray(int blockSize, BOOL deleteMembers = TRUE) : CAutoIndirectArrayBase(blockSize, deleteMembers) {}

    DATA_TYPE*& At(int index)
    {
#ifdef _DEBUG
        if (index < 0 && index > _count)
            Beep(1000, 100);
#endif
        return (DATA_TYPE*&)(CAutoIndirectArrayBase::operator[](index));
    }

    DATA_TYPE*& operator[](int index)
    {
#ifdef _DEBUG
        if (index < 0 && index > _count)
            Beep(1000, 100);
#endif
        return (DATA_TYPE*&)(CAutoIndirectArrayBase::operator[](index));
    }

    virtual ~TAutoIndirectArray() { Destroy(); }

protected:
    virtual void Delete(void* member)
    {
        if (DeleteMembers && member != NULL)
            delete ((DATA_TYPE*)member);
    }
};
