// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
// TDirectArray2:
//  -pole, ktere dynamicky roste/zmensuje se po blocich (neni nutne realokovat
//   jiz obsazenou pamnet, pouze se prida dalsi blok)
//  -pri mazani prvku z pole se vola metoda Destructor(index_prvku),
//   ktera v zakladnim objektu nic neprovadi

template <class DATA_TYPE>
class TAutoDirectArray
{
protected:
    DATA_TYPE** Blocks; // ukazatel na pole bloku
    int BlockSize;
    int _count; // pocet prvku v poli

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

            //byl-li Count == BlockSize delalo to problemy
            //proto je zde Count - 1
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
        if (_count % BlockSize == 0) //vypotreboval jsem vsechny pozice posledniho bloku
        {
            DATA_TYPE** newArrayBlocks;

            newArrayBlocks = (DATA_TYPE**)realloc(Blocks, (_count / BlockSize + 1) * sizeof(DATA_TYPE*));
            if (newArrayBlocks)
            {
                Blocks = newArrayBlocks;
                Blocks[_count / BlockSize] = (DATA_TYPE*)malloc(BlockSize * sizeof(DATA_TYPE));
                if (!Blocks[_count / BlockSize]) //nepodarilo se alokovat...
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
                return -1; //nepodarilo se zvetsit pole ukazatelu na bloky
        }
        Blocks[_count / BlockSize][_count % BlockSize] = member;
        return _count++;
    }

    BOOL Remove(int index) // zrusi prvek na dane pozici, na jeho misto
    {
        if (index >= _count)
            return FALSE;
        Delete(index);
        _count--;
        Blocks[index / BlockSize][index % BlockSize] = Blocks[_count / BlockSize][_count % BlockSize];
        if (!(_count % BlockSize)) //vypotreboval jsem posledni z aktualniho bloku
        {
            free(Blocks[_count / BlockSize]);
        }
        if (!_count) //vypotreboval jsem vsechny
        {
            free(Blocks);
            Blocks = NULL;
        }
        return TRUE;
    }
    // soupne prvek z posledniho mista a zmensi pole
    /*
	CDynamicArray * const &operator[](float index); // funkce se nikdy nevola, ale kdyz tu neni
	// tak dela MSVC strasny veci
	*/
    DATA_TYPE& operator[](int index) //vraci prvek na pozici
    {
        return Blocks[index / BlockSize][index % BlockSize];
    }
};

// ****************************************************************************
// CArray2:
//  -predek vsech indirect poli
//  -drzi typ (void *) v DWORD poli (kvuli uspore mista v .exe)

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
//  -vhodne pro ulozeni ukazatelu na objekty
//  -ostatni vlastnosti viz CArray

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
