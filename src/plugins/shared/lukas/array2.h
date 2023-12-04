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
class TDirectArray2
{
protected:
    DATA_TYPE** Blocks; // ukazatel na pole bloku
    int BlockSize;      // velikost jednoho bloku

public:
    int Count; // pocet prvku v poli

    TDirectArray2<DATA_TYPE>(int blockSize)
    {
        Blocks = NULL;
        BlockSize = blockSize;
        Count = 0;
    }

    virtual ~TDirectArray2() { Destroy(); };

    virtual void Destructor(int) {}

    void Destroy();                    // vycisti pole
    BOOL Add(const DATA_TYPE& member); // prida prvek na posledni pozici
    BOOL Delete(int index);            // zrusi prvek na dane pozici, na jeho misto
                                       // soupne prvek z posledniho mista a zmensi pole
                                       /*
    CDynamicArray * const &operator[](float index); // funkce se nikdy nevola, ale kdyz tu neni
                                                    // tak dela MSVC strasny veci
*/
    DATA_TYPE& operator[](int index)   //vraci prvek na pozici
    {
        return Blocks[index / BlockSize][index % BlockSize];
    }
};

// ****************************************************************************
// CArray2:
//  -predek vsech indirect poli
//  -drzi typ (void *) v DWORD poli (kvuli uspore mista v .exe)

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
//  -vhodne pro ulozeni ukazatelu na objekty
//  -ostatni vlastnosti viz CArray

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

        //byl-li Count == BlockSize delalo to problemy
        //proto je zde Count - 1
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
