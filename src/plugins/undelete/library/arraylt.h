// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

// "light" version of array.h

#pragma once

// Use the _DEBUG or __ARRAY_DEBUG defines to enable various error state checking. Errors
// are displayed using TRACE_E and TRACE_C macros.

// We need to make this module independent on TRACE macros, so if they are not defined,
// we define their fakes. Certainly error reporting will not work in such situation.
#if !defined(TRACE_I) && !defined(TRACE_E) && !defined(TRACE_C)
inline void __TraceEmptyFunction() {}
#define TRACE_I(str) __TraceEmptyFunction()
#define TRACE_E(str) __TraceEmptyFunction()
#define TRACE_C(str) (*((int*)NULL) = 0x666)
#endif // TRACE

enum CArrayDirection
{
    drUp,
    drDown
};

enum CDeleteType
{
    dtNoDelete, // delete is not called for pointers stored in indirect array
    dtDelete    // delete is called for pointers stored in indirect array
};

enum CErrorType
{
    etNone,         // OK
    etLowMemory,    // new - NULL
    etUnknownIndex, // index is out of array range
    etBadInsert,    // index of inserted item is out of array range
    etDestructed,   // array was already destructed using Destroy() method
};

// ****************************************************************************
// TDirectArray:
//  -behaves like classic array, in addition it can pre-allocate to bigger or
//   smaller (look at constructor 'base' and 'delta' values).
//  -when adding item to array, copy-constructor is called
//  -when deleting item, item destructor is called, you can change this behaviour,
//   see CallDestructor method
//  -you can use this array for simple types and also for objects which does not
//   contain pointers to its own data, reason:
//     objects are moved in array (e.g. when reallocating array or when inserting
//     item to the beginning of array) simply by using memmove, so during these
//     moves contructors/destructors are not called, example:
//       char Path[MAX_PATH];  // full file name
//       char *Name;           // points to 'Path' to file name (without path)
//     SOLUTION: store only offsets instead of complete pointers

template <class DATA_TYPE>
class TDirectArray
{
public:
    CErrorType State; // etNone if array is OK, otherwise some error occured
    int Count;        // current count of items in array

    TDirectArray<DATA_TYPE>(int base, int delta);
    virtual ~TDirectArray() { Destroy(); }

    BOOL IsGood() const { return State == etNone; }
    void ResetState()
    {
        State = etNone;
        TRACE_I("Array state reset (TDirectArray).");
    }

    void Insert(int index, const DATA_TYPE& member);
    int Add(const DATA_TYPE& member); // adds item to the end of array, returns item index

    void Insert(int index, const DATA_TYPE* members, int count); // insert 'count' of 'members' items
    int Add(const DATA_TYPE* members, int count);                // add 'count' of 'members' items

    DATA_TYPE& At(int index) // returns pointer to item at 'index' possition
    {
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        if (index >= 0 && index < Count)
#endif
            return Data[index];
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        else
        {
            TRACE_C("Index is out of range (index = " << index
                                                      << ", Count = " << Count << ").");
            Error(etUnknownIndex);
            return Data[0]; // because of compiler we must return (invalid) item
        }
#endif
    }

    DATA_TYPE& operator[](int index) // returns pointer to item at 'index' possition
    {
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        if (index >= 0 && index < Count)
#endif
            return Data[index];
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        else
        {
            TRACE_C("Index is out of range (index = " << index
                                                      << ", Count = " << Count << ").");
            Error(etUnknownIndex);
            return Data[0]; // because of compiler we must return (invalid) item
        }
#endif
    }

    void DetachArray()
    {
        Data = NULL;
        State = etDestructed;
        Count = 0;
        Available = 0;
    }
    DATA_TYPE* GetData() { return Data; }

    void DestroyMembers();             // release items from memory (calling destructors), keep array
    void DetachMembers();              // detach all items (destructors are NOT called), keep array
    void Destroy();                    // complete array destruction (calling destructors)
    void Delete(int index);            // delete item at 'index' possition (calling destructor), move remaining items
    void Delete(int index, int count); // delete 'count' of items at 'index' possition (calling destructors), move remaining items
    void Detach(int index);            // detach item at 'index' possition (destructor is NOT called), move remaining items
    void Detach(int index, int count); // detach 'count' of items at 'index' possition (destructors are NOT called), move remaining items

    int SetDelta(int delta); // change 'Delta', return real used value; NOTE: can be used only for empty array

protected:
    DATA_TYPE* Data; // pointer to array
    int Available;   // allocated size of array
    int Base;        // smallest allocated size of array
    int Delta;       // allocated array size is enlarged/reduced by this value

    virtual void Error(CErrorType err) // array error handling
    {
        if (State == etNone)
            State = err;
        else
            TRACE_E("Incorrect call to Error method (State = " << State << ").");
    }
    void EnlargeArray(); // enlarges array
    void ReduceArray();  // reduces array

    void Move(CArrayDirection direction, int first, int count); // move selected items to next/previous index

    void CallCopyConstructor(DATA_TYPE* placement, const DATA_TYPE& member)
    {
#ifdef new
//#pragma push_macro("new")  // push_macro and pop_macro are not working here (memory leak test placed further in this module is not reported with correct module and line information) -- the reason is that we are not using simple macro to define 'new' as in MFC (MFC uses "#define new DEBUG_NEW")
#define __ARRAY_REDEF_NEW
#undef new
#endif

        ::new (placement) DATA_TYPE(member); // call copy constructor

#ifdef __ARRAY_REDEF_NEW
//#pragma pop_macro("new")  // push_macro and pop_macro are not working here (memory leak test placed further in this module is not reported with correct module and line information) -- the reason is that we are not using simple macro to define 'new' as in MFC (MFC uses "#define new DEBUG_NEW")
#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define __ARRAY_STR2(x) #x
#define __ARRAY_STR(x) __ARRAY_STR2(x)
#pragma message(__FILE__ "(" __ARRAY_STR(__LINE__) "): error: operator new with unknown reason for redefinition")
#endif
#undef __ARRAY_REDEF_NEW
#endif

        //      new int(5);  // memory leak test
    }

    virtual void CallDestructor(DATA_TYPE& member) { member.~DATA_TYPE(); }

private: // following methods will not be called (prevention)
    TDirectArray<DATA_TYPE>() {}
    TDirectArray<DATA_TYPE>(const TDirectArray<DATA_TYPE>&) {}
    TDirectArray<DATA_TYPE>& operator=(TDirectArray<DATA_TYPE>&) { return *this; }

    // compiler reports error on this line: we have just wanted to catch source code designed for
    // older version of TDirectArray template: use CallDestructor instead of Destructor and please
    // notice that in new version of TDirectArray copy-constructors and destructors are called
    virtual int Destructor(int) { return 0; }
};

// ****************************************************************************
// CArray:
//  -ancestor for all indirect arrays

class CArray : public TDirectArray<void*>
{
protected:
    CDeleteType DeleteType;

public:
    CArray(int base, int delta, CDeleteType dt) : TDirectArray<void*>(base, delta)
    {
        DeleteType = dt;
    }
};

// ****************************************************************************
// TIndirectArray:
//  -indirect array is designed for storing pointers to objects (allocated or not)
//  -look at CArray and TDirectArray<void *> for other features

template <class DATA_TYPE>
class TIndirectArray : public CArray
{
public:
    TIndirectArray(int base, int delta, CDeleteType dt = dtDelete)
        : CArray(base, delta, dt) {}
    DATA_TYPE*& At(int index)
    {
        return (DATA_TYPE*&)(CArray::operator[](index));
    }

    DATA_TYPE*& operator[](int index)
    {
        return (DATA_TYPE*&)(CArray::operator[](index));
    }

    void Insert(int index, DATA_TYPE* member)
    {
        CArray::Insert(index, (void*)member);
    }

    int Add(DATA_TYPE* member)
    {
        return CArray::Add((void*)member);
    }

    void Insert(int index, DATA_TYPE* const* members, int count)
    {
        CArray::Insert(index, (void**)members, count);
    }

    int Add(DATA_TYPE* const* members, int count)
    {
        return CArray::Add((void**)members, count);
    }

    DATA_TYPE** GetData()
    {
        return (DATA_TYPE**)Data;
    }

    virtual ~TIndirectArray() { Destroy(); }

protected:
    virtual void CallDestructor(void*& member)
    {
        if (DeleteType == dtDelete && (DATA_TYPE*)member != NULL)
            delete ((DATA_TYPE*)member);
    }
};

//
// ****************************************************************************
// TDirectArray
//

template <class DATA_TYPE>
TDirectArray<DATA_TYPE>::TDirectArray(int base, int delta)
{
    if (base <= 0)
        TRACE_E("Base is less or equal to zero, correcting to 1.");
    Base = (base > 0) ? base : 1;
    if (delta <= 0)
        TRACE_E("Delta is less or equal to zero, correcting to 1.");
    Delta = (delta > 0) ? delta : 1;
    State = etNone;
    Available = Count = 0;
    Data = (DATA_TYPE*)malloc(Base * sizeof(DATA_TYPE));
    if (Data == NULL)
    {
        TRACE_E("Out of memory.");
        Error(etLowMemory);
        return;
    }
    Available = Base;
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::Destroy()
{
    if (State == etNone) // it can be also etDestructed
    {
        if (Data != NULL)
        {
            for (int i = 0; i < Count; i++)
                CallDestructor(Data[i]);
            free(Data);
            Data = NULL;
            State = etDestructed;
        }
    }
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    else if (State != etDestructed)
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::Insert(int index, const DATA_TYPE& member)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (Count == Available && &member >= Data && &member < Data + Count)
        {
            TRACE_C("Inserted item could become invalid during operation.");
            return;
        }
        if (Count < Available &&
            &member >= Data + index + 1 && &member < Data + Count + 1)
            TRACE_C("Inserted item will change value during operation.");
#endif
        if (index >= 0 && index <= Count)
        {
            Move(drDown, index, Count - index);
            if (State == etNone)
            {
                Count++;
                CallCopyConstructor(&At(index), member);
            }
        }
        else
        {
            TRACE_C("Attempt to insert item out of array range.");
            State = etBadInsert;
        }
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE>
int TDirectArray<DATA_TYPE>::Add(const DATA_TYPE& member)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (Count == Available && &member >= Data && &member < Data + Count)
        {
            TRACE_C("Added item could become invalid during operation.");
            return ULONG_MAX;
        }
#endif
        if (Count == Available)
            EnlargeArray();
        if (State == etNone)
        {
            Count++;
            CallCopyConstructor(&At(Count - 1), member);
            return Count - 1;
        }
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
    return ULONG_MAX;
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::Insert(int index, const DATA_TYPE* members, int count)
{
    if (count <= 0)
        return; // nothing to do
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (Count + count > Available && members + count - 1 >= Data && // array will be reallocated and inserted items are from this array (items will be released after array reallocation)
            members < Data + Count)
        {
            TRACE_C("Inserted items could become invalid during operation.");
            return;
        }
        if (Count + count <= Available &&                  // if array will not be reallocated
            members + count - 1 >= Data + index + count && // and if inserted part is from memory where array will be enlarged
            members < Data + Count + count)
        {
            TRACE_C("Inserted items will change value during operation.");
        }
#endif
        if (index >= 0 && index <= Count)
        {
            int needed = Count + count;
            if (needed > Available)
            {
                needed -= Base + 1;
                needed = needed - (needed % Delta) + Delta + Base;
                DATA_TYPE* newData = (DATA_TYPE*)realloc(Data, needed * sizeof(DATA_TYPE));
                if (newData == NULL)
                {
                    TRACE_E("Low memory for array enlargement.");
                    Error(etLowMemory);
                    return;
                }
                Available = needed;
                Data = newData;
            }
            memmove(Data + index + count, Data + index, (Count - index) * sizeof(DATA_TYPE));

            DATA_TYPE* placement = Data + index;
            for (int i = 0; i < count; i++)
                CallCopyConstructor(placement + i, members[i]);

            Count += count;
        }
        else
        {
            TRACE_C("Attempt to insert item out of array range.");
            State = etBadInsert;
        }
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE>
int TDirectArray<DATA_TYPE>::Add(const DATA_TYPE* members, int count)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (Count + count > Available && members + count - 1 >= Data &&
            members < Data + Count)
        {
            TRACE_C("Added items could become invalid during operation.");
            return ULONG_MAX;
        }
#endif
        int needed = Count + count;
        if (needed > Available)
        {
            needed -= Base + 1;
            needed = needed - (needed % Delta) + Delta + Base;
            DATA_TYPE* newData = (DATA_TYPE*)realloc(Data, needed * sizeof(DATA_TYPE));
            if (newData == NULL)
            {
                TRACE_E("Low memory for array enlargement.");
                Error(etLowMemory);
                return ULONG_MAX;
            }
            Available = needed;
            Data = newData;
        }

        DATA_TYPE* placement = Data + Count;
        for (int i = 0; i < count; i++)
            CallCopyConstructor(placement + i, members[i]);

        Count += count;
        return Count - count;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    return ULONG_MAX;
#endif
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::DestroyMembers()
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
#endif
        if (Count > 0)
            for (int i = 0; i < Count; i++)
                CallDestructor(Data[i]);
        else
            return;
        Count = 0;
        if (Available == Base)
            return;

        DATA_TYPE* newData = (DATA_TYPE*)realloc(Data, Base * sizeof(DATA_TYPE));
        if (newData == NULL)
        {
            TRACE_E("Low memory for operations related to item destruction.");
            Error(etLowMemory);
            return;
        }
        Available = Base;
        Data = newData;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
#endif
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::DetachMembers()
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
#endif
        if (Count == 0)
            return;
        Count = 0;
        if (Available == Base)
            return;

        DATA_TYPE* newData = (DATA_TYPE*)realloc(Data, Base * sizeof(DATA_TYPE));
        if (newData == NULL)
        {
            TRACE_E("Low memory for operations related to item destruction.");
            Error(etLowMemory);
            return;
        }
        Available = Base;
        Data = newData;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
#endif
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::Delete(int index)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (index >= 0 && index < Count)
        {
#endif
            CallDestructor(Data[index]);
            Move(drUp, index + 1, Count - index - 1);
            Count--;
            if (Available > Base && Available - Delta == Count)
                ReduceArray();
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        }
        else
            TRACE_C("Attempt to delete item out of array range. index = " << index << ", count = " << Count);
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::Delete(int index, int count)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (index >= 0 && index + count - 1 < Count)
        {
#endif
            for (int i = index; i < index + count; i++)
                CallDestructor(Data[i]);
            memmove(Data + index, Data + index + count, (Count - count - index) * sizeof(DATA_TYPE));
            Count -= count;
            if (Available > Base && Available - Delta >= Count)
            {
                int a = (Count <= Base) ? Base : Base + Delta * ((Count - Base - 1) / Delta + 1);
                DATA_TYPE* New = (DATA_TYPE*)realloc(Data, a * sizeof(DATA_TYPE));
                if (New == NULL)
                {
                    TRACE_E("Low memory for operations related to array reducing.");
                    Error(etLowMemory);
                }
                else
                {
                    Data = New;
                    Available = a;
                }
            }
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        }
        else
            TRACE_C("Attempt to delete item out of array range. index = " << index << ", count = " << count << ", Count = " << Count);
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::Detach(int index)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (index >= 0 && index < Count)
        {
#endif
            Move(drUp, index + 1, Count - index - 1);
            Count--;
            if (Available > Base && Available - Delta == Count)
                ReduceArray();
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        }
        else
            TRACE_C("Attempt to detach item out of array range. index = " << index << ", count = " << Count);
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::Detach(int index, int count)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (index >= 0 && index + count - 1 < Count)
        {
#endif
            memmove(Data + index, Data + index + count, (Count - count - index) * sizeof(DATA_TYPE));
            Count -= count;
            if (Available > Base && Available - Delta >= Count)
            {
                int a = (Count <= Base) ? Base : Base + Delta * ((Count - Base - 1) / Delta + 1);
                DATA_TYPE* New = (DATA_TYPE*)realloc(Data, a * sizeof(DATA_TYPE));
                if (New == NULL)
                {
                    TRACE_E("Low memory for operations related to array reducing.");
                    Error(etLowMemory);
                }
                else
                {
                    Data = New;
                    Available = a;
                }
            }
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        }
        else
            TRACE_C("Attempt to detach item out of array range. index = " << index << ", count = " << count << ", Count = " << Count);
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE>
int TDirectArray<DATA_TYPE>::SetDelta(int delta)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
#endif // defined(_DEBUG) || defined(__ARRAY_DEBUG)
        if (Count == 0)
        {
            if (delta > 0 && delta > Delta)
                Delta = delta;
        }
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        else
            TRACE_E("SetDelta() may be used only for empty array. (Count = " << Count << ").");
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif // defined(_DEBUG) || defined(__ARRAY_DEBUG)
    return Delta;
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::EnlargeArray()
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
#endif
        DATA_TYPE* New = (DATA_TYPE*)realloc(Data, (Available + Delta) * sizeof(DATA_TYPE));
        if (New == NULL)
        {
            TRACE_E("Low memory for array enlargement.");
            Error(etLowMemory);
            return;
        }
        Data = New;
        Available += Delta;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::ReduceArray()
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
#endif
        DATA_TYPE* New = (DATA_TYPE*)realloc(Data, (Available - Delta) * sizeof(DATA_TYPE));
        if (New == NULL)
        {
            TRACE_E("Low memory for operations related to array reducing.");
            Error(etLowMemory);
            return;
        }
        Data = New;
        Available -= Delta;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE>
void TDirectArray<DATA_TYPE>::Move(CArrayDirection direction, int first, int count)
{
    if (count == 0)
    {
        if (direction == drDown && Available == Count)
            EnlargeArray();
        return;
    }
    if (direction == drDown)
    {
        if (Available == Count)
            EnlargeArray();
        if (State == etNone)
            memmove(Data + first + 1, Data + first, count * sizeof(DATA_TYPE));
    }
    else // Up
        memmove(Data + first - 1, Data + first, count * sizeof(DATA_TYPE));
}
