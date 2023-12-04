// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Use the _DEBUG or __ARRAY_DEBUG defines to enable various error state checking. Errors
// are displayed using TRACE_E and TRACE_C macros.
// Use the SAFE_ALLOC define to remove source code with testing if memory allocation
// has not failed (see allochan.*).

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
    etBadDispose,   // index of disposed item is out of array range
    etDestructed,   // array was already destructed using Destroy() method
};

#ifdef TRACE_ENABLE
std::ostream& operator<<(std::ostream& out, const CErrorType& err);
#endif //TRACE_ENABLE

#ifndef __ARRAY_CPP

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

// ****************************************************************************
// TIndirectClassArray:
//  -vhodne pro ulozeni ukazatelu na objekty, neumoznuje zmenu indexu prvku
//  -nad polem se neprovadi zadne operace sesunu prvku - prvek zustava
//   stale na stejnem indexu
//  -pri pridavani prvku se vyplnuji mezery vznikle predchozim uvolnovanim prvku

template <class CLASS_TYPE>
class TIndirectClassArray : public TIndirectArray<CLASS_TYPE>
{
public:
    int FirstFreeIndex;

    TIndirectClassArray(int base, int delta, CDeleteType dt = dtDelete)
        : TIndirectArray<CLASS_TYPE>(base, delta, dt) { FirstFreeIndex = 0; }

    int Add(CLASS_TYPE* c);
    void Dispose(int index);

    void DestroyMembers()
    {
        TIndirectArray<CLASS_TYPE>::DestroyMembers();
        FirstFreeIndex = 0;
    }

    void DetachMembers()
    {
        TIndirectArray<CLASS_TYPE>::DetachMembers();
        FirstFreeIndex = 0;
    }

    void Destroy()
    {
        TIndirectArray<CLASS_TYPE>::Destroy();
        FirstFreeIndex = 0;
    }

protected: // prevence proti volani nefunkcniho kodu (posouva prvky,...)
    void Move(CArrayDirection, int, int) {}
    void Insert(int, void*) {}
    void Insert(int, void**, int) {}
    int Add(void*) { return ULONG_MAX; }
    int Add(void**, int) { return ULONG_MAX; }
    void Delete(int) {}
    void DeleteAt(int) {}
    void Detach(int) {}
};

// ****************************************************************************
// TSmallerDirectArray:
//  -popis viz TDirectArray, ale neni vhodne pro ulozeni objektu (nevola
//   konstruktory ani destruktory), specializace: setri pamet potrebnou
//   na spravu pole tim, ze umoznuje celkovy pocet prvku pole jen 65535 - misto
//   int WORD a nema virtualni metodu CallDestructor
//  -vhodne jako promenna (atribut) tridy, od ktere existuje mhoho objektu
//  -pametove naroky: 6 B (jen +2 B oproti klas. poli), (TDirectArray ma 25 B)
//  -zmenseni za cenu generovani tridy podle sablony pro ruzne 'Base', 'Delta'
//   a zmenseni rozsahu pole

template <class DATA_TYPE, WORD Base, WORD Delta> // jen 65535 prvku
class TSmallerDirectArray
{
public:
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    CErrorType State; // neni-li etNone stala se chyba
#endif
    DATA_TYPE* Data; // ukazatel na pole, public nutne misto etDestructed
    WORD Count;      // soucasny pocet polozek v kolekci

    TSmallerDirectArray<DATA_TYPE, Base, Delta>();
    ~TSmallerDirectArray() { Destroy(); }

    BOOL IsGood() const
    {
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        return State == etNone;
#else
        return TRUE;
#endif
    }

    void ResetState()
    {
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        State = etNone;
        TRACE_I("Array state reset (TSmallerDirectArray).");
#endif
    }

    void Insert(int index, const DATA_TYPE& member);
    inline WORD Add(const DATA_TYPE& member);      // prida prvek na konec Arraye,
                                                   // vraci index prvku
    WORD Add(const DATA_TYPE* members, int count); // pridani count prvku members

    DATA_TYPE& At(int index) // vraci ukazatel na prvek na pozici
    {                        // int pouzity jen kvuli warningum - rozsah do 65535
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
            return Data[0]; // kvuli kompileru vracim mozna neplatny prvek
        }
#endif
    }

    DATA_TYPE& operator[](int index) // vraci ukazatel na prvek na pozici
    {                                // int pouzity jen kvuli warningum - rozsah do 65535
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
            return Data[0]; // kvuli kompileru vracim mozna neplatny prvek
        }
#endif
    }

    void DetachArray()
    {
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        State = etDestructed;
#endif
        Data = NULL;
        Count = 0;
    }

    DATA_TYPE* GetData() { return Data; }

    void DestroyMembers();           // uvolni z pameti jen prvky, pole necha
    void Destroy();                  // kompletni destrukce objektu
    void Delete(int index);          // zrusi prvek na pozici a ostatni posune
    void Reduce(WORD newCount);      // zrusi prvky od indexu newCount az do konce
    void Delete(WORD from, WORD to); // zrusi prvky <from..to) a sesune ostatni

protected:
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    virtual void Error(CErrorType err) // zpracovani chyby v kolekci
    {
        if (State == etNone)
            State = err;
        else
            TRACE_E("Incorrect call to Error method (State = " << State << ").");
    }
#endif
    void EnlargeArray(); // zvetsi nove pole
    void ReduceArray();  // zmensi pole

    void Move(CArrayDirection direction, WORD first, WORD count);
    // posune o 1
private: // nasledujici metody se nebudou volat (prevence)
    TSmallerDirectArray<DATA_TYPE, Base, Delta>(const TSmallerDirectArray<DATA_TYPE, Base, Delta>&) {}
    TSmallerDirectArray<DATA_TYPE, Base, Delta>& operator=(TSmallerDirectArray<DATA_TYPE, Base, Delta>&)
    {
        return *this;
    }
};

// ****************************************************************************
// TClassArray:
//  -vhodne pro ulozeni mnoha malych objektu, neumoznuje zmenu indexu prvku
//  -alokuje objekty CLASS_TYPE primo do pole,
//   provadi se volani konstruktoru i destruktoru techto objektu
//  -nad polem se neprovadi zadne operace sesunu prvku - prvek zustava
//   stale na stejnem indexu
//  -platny prvek pole: (index_prvku < Count && !At(index_prvku).IsEmpty())
//
// naroky na CLASS_TYPE:
//  1) existence metody IsEmpty(), ktera vraci jestli uz se volal
//     destruktor objektu
//  2) nadefinovani operatoru 'new' pomoci makra DEFINE_NEW(CLASS_TYPE)
//
// priklad:
//  class CSimpleObject
//  {
//    public:
//      BOOL Destructed;
//
//      CSimpleObject() {Destructed = FALSE;}
//      ~CSimpleObject() {Destructed = TRUE;}
//
//      BOOL IsEmpty() {return Destructed;}
//
//      DEFINE_NEW(CSimpleObject)
//  };
//
//  TClassArray<CSimpleObject> Simples(10, 5);
//
// pridani prvku do pole:
//  int index_prvku = Simples.FirstFreeIndex;  // prvni volny index v poli
//  new (&Simples)CSimpleObject();   // vraci adresu objektu, pri chybe NULL,
//                                   // prvek byl pridan a ma index index_prvku

template <class CLASS_TYPE>
class TClassArray : public TDirectArray<CLASS_TYPE>
{
public:
    int FirstFreeIndex;

    TClassArray<CLASS_TYPE>(int base, int delta) : TDirectArray<CLASS_TYPE>(base, delta) { FirstFreeIndex = 0; }

    void* GetFreeArraySpace();
    void Dispose(int index);
    virtual void Destructor(int i)
    {
        if (!Data[i].IsEmpty())
            Data[i].~CLASS_TYPE();
    }

    void DestroyMembers()
    {
        TDirectArray<CLASS_TYPE>::DestroyMembers();
        FirstFreeIndex = 0;
    }

    void Destroy()
    {
        TDirectArray<CLASS_TYPE>::Destroy();
        FirstFreeIndex = 0;
    }

    ~TClassArray() { Destroy(); }

private: // prevence proti volani nefunkcniho kodu (posouva prvky ...)
    void Insert(int, const CLASS_TYPE&) {}
    void Insert(int, const CLASS_TYPE*, int) {}
    int Add(const CLASS_TYPE&) { return ULONG_MAX; }
    int Add(const CLASS_TYPE*, int) { return ULONG_MAX; }
    void Delete(int) {}
    void Move(CArrayDirection, int, int) {}
    void DetachMembers() {} // to by nezavolalo destruktory
};

#define DEFINE_NEW(CLASS_TYPE) \
    void* operator new(size_t siz) \
    { \
        TRACE_C("Incorrect 'new' operator was called."); \
        return malloc(siz); \
    } \
    void* operator new(size_t, void* p) \
    { \
        return ((TClassArray<CLASS_TYPE>*)p)->GetFreeArraySpace(); \
    }

#ifndef WITHOUT_INDIRECTARRAY

//
// ****************************************************************************
// TIndirectClassArray
//

template <class CLASS_TYPE>
int TIndirectClassArray<CLASS_TYPE>::Add(CLASS_TYPE* c)
{
    if (State == etNone)
    {
        if (FirstFreeIndex == Count)
        {
            TIndirectArray<CLASS_TYPE>::Add(c);
            if (IsGood())
            {
                FirstFreeIndex = Count;
                return Count - 1;
            }
        }
        else
        {
            int ret = FirstFreeIndex;
            At(FirstFreeIndex) = c;
            int i;
            for (i = FirstFreeIndex + 1; i < Count; i++)
                if (At(i) == NULL)
                    break;
            FirstFreeIndex = i;
            return ret;
        }
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
    return ULONG_MAX;
}

template <class CLASS_TYPE>
void TIndirectClassArray<CLASS_TYPE>::Dispose(int index)
{
    if (State == etNone)
    {
        if (index < 0 || index >= Count)
        {
            TRACE_C("Attempt to dispose item out of array range (index = " << index << ", Count = " << Count << ").");
            Error(etBadDispose);
            return;
        }
        Destructor(At(index));
        At(index) = NULL;
        if (Count == index + 1)
        {
            if (--Count > 0)
                for (int i = Count; 0 < i; i--)
                    if (At(i - 1) == NULL)
                        Count--;
                    else
                        break;

            while (IsGood() && Available > Base && Available - Delta >= Count)
                DestroyArray();

            if (FirstFreeIndex > Count)
                FirstFreeIndex = Count;
        }
        else if (FirstFreeIndex > index)
            FirstFreeIndex = index;
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
}

#endif

//
// ****************************************************************************
// TClassArray
//

template <class CLASS_TYPE>
void* TClassArray<CLASS_TYPE>::GetFreeArraySpace()
{
    if (State == etNone)
    {
        if (FirstFreeIndex == Count)
        {
            if (Count == Available)
                EnlargeArray();
            if (State == etNone)
            {
                FirstFreeIndex = Count + 1;
                return Data + Count++;
            }
        }
        else
        {
            void* ret = Data + FirstFreeIndex;
            int i;
            for (i = FirstFreeIndex + 1; i < Count; i++)
                if (Data[i].IsEmpty())
                    break;
            FirstFreeIndex = i;
            return ret;
        }
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
    return NULL;
}

template <class CLASS_TYPE>
void TClassArray<CLASS_TYPE>::Dispose(int index)
{
    if (State == etNone)
    {
        if (index < 0 || index >= Count)
        {
            TRACE_C("Attempt to dispose item out of array range (index = " << index << ", Count = " << Count << ").");
            Error(etBadDispose);
            return;
        }
        Destructor(index);
        if (Count == index + 1)
        {
            if (--Count > 0)
                for (int i = Count; 0 < i; i--)
                    if (Data[i - 1].IsEmpty())
                        Count--;
                    else
                        break;

            while (IsGood() && Available > Base && Available - Delta >= Count)
                ReduceArray();

            if (FirstFreeIndex > Count)
                FirstFreeIndex = Count;
        }
        else if (FirstFreeIndex > index)
            FirstFreeIndex = index;
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
}

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
#ifndef SAFE_ALLOC
    if (Data == NULL)
    {
        TRACE_E("Out of memory.");
        Error(etLowMemory);
        return;
    }
#endif // SAFE_ALLOC
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
#ifndef SAFE_ALLOC
                if (newData == NULL)
                {
                    TRACE_E("Low memory for array enlargement.");
                    Error(etLowMemory);
                    return;
                }
#endif // SAFE_ALLOC
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
#ifndef SAFE_ALLOC
            if (newData == NULL)
            {
                TRACE_E("Low memory for array enlargement.");
                Error(etLowMemory);
                return ULONG_MAX;
            }
#endif // SAFE_ALLOC
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
#ifndef SAFE_ALLOC
        if (newData == NULL)
        {
            TRACE_E("Low memory for operations related to item destruction.");
            Error(etLowMemory);
            return;
        }
#endif // SAFE_ALLOC
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
#ifndef SAFE_ALLOC
        if (newData == NULL)
        {
            TRACE_E("Low memory for operations related to item destruction.");
            Error(etLowMemory);
            return;
        }
#endif // SAFE_ALLOC
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
#ifndef SAFE_ALLOC
                if (New == NULL)
                {
                    TRACE_E("Low memory for operations related to array reducing.");
                    Error(etLowMemory);
                }
                else
                {
#endif // SAFE_ALLOC
                    Data = New;
                    Available = a;
#ifndef SAFE_ALLOC
                }
#endif // SAFE_ALLOC
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
#ifndef SAFE_ALLOC
                if (New == NULL)
                {
                    TRACE_E("Low memory for operations related to array reducing.");
                    Error(etLowMemory);
                }
                else
                {
#endif // SAFE_ALLOC
                    Data = New;
                    Available = a;
#ifndef SAFE_ALLOC
                }
#endif // SAFE_ALLOC
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
#ifndef SAFE_ALLOC
        if (New == NULL)
        {
            TRACE_E("Low memory for array enlargement.");
            Error(etLowMemory);
            return;
        }
#endif // SAFE_ALLOC
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
#ifndef SAFE_ALLOC
        if (New == NULL)
        {
            TRACE_E("Low memory for operations related to array reducing.");
            Error(etLowMemory);
            return;
        }
#endif // SAFE_ALLOC
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

//
// ****************************************************************************
// TSmallerDirectArray
//

template <class DATA_TYPE, WORD Base, WORD Delta>
TSmallerDirectArray<DATA_TYPE, Base, Delta>::TSmallerDirectArray()
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    State = etNone;
#endif
    Count = 0;
    Data = (DATA_TYPE*)malloc(Base * sizeof(DATA_TYPE));
#ifndef SAFE_ALLOC
    if (Data == NULL)
    {
        TRACE_E("Out of memory.");
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        Error(etLowMemory);
#endif
        return;
    }
#endif // SAFE_ALLOC
}

template <class DATA_TYPE, WORD Base, WORD Delta>
void TSmallerDirectArray<DATA_TYPE, Base, Delta>::Destroy()
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone) // muze prijit etDestructed
    {
#endif
        if (Data != NULL)
        {
            //      for (WORD i = 0; i < Count; i++)
            //        Destructor(i);
            free(Data);
            Data = NULL;
            Count = 0;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
            State = etDestructed;
#endif
        }
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    else if (State != etDestructed)
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE, WORD Base, WORD Delta>
void TSmallerDirectArray<DATA_TYPE, Base, Delta>::Insert(int index, const DATA_TYPE& member)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (Count >= Base && ((Count - Base) % Delta) == 0 &&
            &member >= Data && &member < Data + Count)
        {
            TRACE_C("Inserted item could become invalid during operation.");
            return;
        }
        if ((Count < Base || ((Count - Base) % Delta) != 0) &&
            &member >= Data + index + 1 && &member < Data + Count + 1)
            TRACE_C("Inserted item will change value during operation.");
#endif
        Move(drDown, (short int)index, (short int)(Count - index));
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        if (State == etNone)
        {
#endif
            Count++;
            At(index) = member;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        }
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE, WORD Base, WORD Delta>
WORD TSmallerDirectArray<DATA_TYPE, Base, Delta>::Add(const DATA_TYPE& member)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (Count >= Base && ((Count - Base) % Delta) == 0 &&
            &member >= Data && &member < Data + Count)
        {
            TRACE_C("Added item could become invalid during operation.");
            return USHRT_MAX;
        }
#endif
        if (Count >= Base && ((Count - Base) % Delta) == 0)
            EnlargeArray();
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        if (State == etNone)
        {
#endif
            Count++;
            At(Count - 1) = member;
            return (short int)(Count - 1);
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        }
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
    return USHRT_MAX;
#endif
}

template <class DATA_TYPE, WORD Base, WORD Delta>
WORD TSmallerDirectArray<DATA_TYPE, Base, Delta>::Add(const DATA_TYPE* members, int count)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
#endif
        int needed = Count + count;
        int available;
        if (Count > Base)
        {
            available = Count - Base - 1;
            available = available - (available % Delta) + Delta + Base;
        }
        else
            available = Base;

        if (needed > available)
        {
            needed -= Base + 1;
            needed = needed - (needed % Delta) + Delta + Base;
            DATA_TYPE* newData = (DATA_TYPE*)realloc(Data, needed * sizeof(DATA_TYPE));
#ifndef SAFE_ALLOC
            if (newData == NULL)
            {
                TRACE_E("Low memory for array enlargement.");
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
                Error(etLowMemory);
#endif
                return USHRT_MAX;
            }
#endif // SAFE_ALLOC
            Data = newData;
        }
        memmove(Data + Count, members, count * sizeof(DATA_TYPE));
        Count += (WORD)count;
        return (WORD)(Count - count);
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    return USHRT_MAX;
#endif
}

template <class DATA_TYPE, WORD Base, WORD Delta>
void TSmallerDirectArray<DATA_TYPE, Base, Delta>::DestroyMembers()
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
#endif
        if (Count > 0)
            ; // for (WORD i = 0; i < Count; i++) Destructor(i);
        else
            return;
        if (Count <= Base)
        {
            Count = 0;
            return;
        }
        Count = 0;

        DATA_TYPE* newData = (DATA_TYPE*)realloc(Data, Base * sizeof(DATA_TYPE));
#ifndef SAFE_ALLOC
        if (newData == NULL)
        {
            TRACE_E("Low memory for operations related to item destruction.");
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
            Error(etLowMemory);
#endif
            return;
        }
#endif // SAFE_ALLOC
        Data = newData;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
#endif
}

template <class DATA_TYPE, WORD Base, WORD Delta>
void // int pouzity jen kvuli warningum - rozsah do 65535
TSmallerDirectArray<DATA_TYPE, Base, Delta>::Delete(int index)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (index >= 0 && index < Count)
        {
#endif
            //      Destructor((short int)index);
            Move(drUp, (short int)(index + 1), (short int)(Count - index - 1));
            Count--;
            if (Count >= Base && ((Count - Base) % Delta) == 0)
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

template <class DATA_TYPE, WORD Base, WORD Delta>
void TSmallerDirectArray<DATA_TYPE, Base, Delta>::Reduce(WORD newCount)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
#endif
        if (newCount < Count)
        {
            int needed;
            if (newCount > Base)
                needed = Base + (newCount - Base - 1) - ((newCount - Base - 1) % Delta) + Delta;
            else
                needed = Base;

            DATA_TYPE* New = (DATA_TYPE*)realloc(Data, needed * sizeof(DATA_TYPE));
#ifndef SAFE_ALLOC
            if (New == NULL)
            {
                TRACE_E("Low memory for array enlargement.");
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
                Error(etLowMemory);
#endif
                return;
            }
#endif // SAFE_ALLOC
            Data = New;
            Count = newCount;
        }
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
#endif
}

template <class DATA_TYPE, WORD Base, WORD Delta>
void TSmallerDirectArray<DATA_TYPE, Base, Delta>::Delete(WORD from, WORD to)
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (from < 0 || from > to || to > Count)
        {
            TRACE_C("Incorrect call to Delete method (from=" << from << ", to=" << to << ", count = " << Count << ").");
        }
        else
        {
#endif
            memmove(Data + from, Data + to, (Count - to) * sizeof(DATA_TYPE));
            Reduce((WORD)(Count - (to - from)));
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        }
    }
#endif
}

template <class DATA_TYPE, WORD Base, WORD Delta>
void TSmallerDirectArray<DATA_TYPE, Base, Delta>::EnlargeArray()
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (Count < Base || ((Count - Base) % Delta) != 0)
            TRACE_C("Incorrect call to EnlargeArray: some allocated items are not used.");
#endif
        DATA_TYPE* New = (DATA_TYPE*)realloc(Data, (Count + Delta) * sizeof(DATA_TYPE));
#ifndef SAFE_ALLOC
        if (New == NULL)
        {
            TRACE_E("Low memory for array enlargement.");
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
            Error(etLowMemory);
#endif
            return;
        }
#endif // SAFE_ALLOC
        Data = New;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE, WORD Base, WORD Delta>
void TSmallerDirectArray<DATA_TYPE, Base, Delta>::ReduceArray()
{
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    if (State == etNone)
    {
        if (Count < Base || ((Count - Base) % Delta) != 0)
            TRACE_C("Incorrect call to ReduceArray: number of unused allocated items is not divisible by Delta.");
#endif
        DATA_TYPE* New = (DATA_TYPE*)realloc(Data, Count * sizeof(DATA_TYPE));
#ifndef SAFE_ALLOC
        if (New == NULL)
        {
            TRACE_E("Low memory for operations related to array reducing.");
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
            Error(etLowMemory);
#endif
            return;
        }
#endif // SAFE_ALLOC
        Data = New;
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
    }
    else
        TRACE_E("Incorrect call to array method (State = " << State << ").");
#endif
}

template <class DATA_TYPE, WORD Base, WORD Delta>
void TSmallerDirectArray<DATA_TYPE, Base, Delta>::Move(CArrayDirection direction, WORD first, WORD count)
{
    if (count == 0)
    {
        if (direction == drDown && Count >= Base && ((Count - Base) % Delta) == 0)
            EnlargeArray();
        return;
    }
    if (direction == drDown)
    {
        if (Count >= Base && ((Count - Base) % Delta) == 0)
            EnlargeArray();
#if defined(_DEBUG) || defined(__ARRAY_DEBUG)
        if (State == etNone)
#endif
            memmove(Data + first + 1, Data + first, count * sizeof(DATA_TYPE));
    }
    else // Up
        memmove(Data + first - 1, Data + first, count * sizeof(DATA_TYPE));
}

#endif // __ARRAY_CPP
