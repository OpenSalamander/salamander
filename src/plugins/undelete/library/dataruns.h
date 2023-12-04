// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/*
Data runs encoding
------------------

Data run has variable size encoding composed from three values:
Header, Length, and Offset.

First byte (Header) contains two 4-bits values, OffsetCnt and LengthCnt: 
(OffsetCnt & 0x0F) << 4 | (LengthCnt & 0x0F)

Next OffsetCnt bytes represents Offset value. OffsetCnt has 0x0 value
for Sparse runs (virtual LengthCnt clusters filled by zero).

Next LengthCnt bytes represents Length value. LengthCnt is always > 0x0,
except of "end of data runs" header.

Header value 0x00 means end of data runs.

Both Offset and Length are encoded as signed values, but only Offset
could contain negative values. Last (most significant) byte stored in
Offset and Length has value from 0x00 to 0x7F for positive values
and (in case of Offset) value from 0x80 to 0xFF for negative values.
Examples: to encode positive value 0x7F one byte is enough. To encode
positive value 0x80 two byte are needed: 0x80 0x00.

1-Run examples:
0x01 0x04
OffsetCnt=0x0, LengthCnt=0x1, Length=0x04 (Sparse run, 4 clusters)

0x21 0x18 0x34 0x56
OffsetCnt=0x2, LengthCnt=0x1, Length=0x34, Offset=0x5634 (+22,068)

0x41 0x7F 0x9F 0x60 0xA2 0x00
OffsetCnt=0x4, LengthCnt=0x1, Length=0x7F, Offset=0x00A2609F (+10,641,567)

0x32 0x80 0x00 0xBA 0x57 0x1B
OffsetCnt=0x3, LengthCnt=0x2, Length=0x0080, Offset=0x1B57BA (+1,791,930)

0x21 0x01 0x99 0xF2
OffsetCnt=0x2, LengthCnt=0x1, Length=0x01, Offset=0xF299 (-3,431)

0x51 0x01 0xB8 0xC8 0xFF 0xF9 0x00
OffsetCnt=0x5, LengthCnt=0x1, Length=0x01, Offset=0x00F9FFC8B8 (+4,194,289,848)

0x51 0x01 0x61 0x38 0x00 0x06 0xFF
OffsetCnt=0x5, LengthCnt=0x1, Length=0x01, Offset=0xFF06003861 (-4,194,289,567)

Multiple-Run example:
0x11 0x08 0x40 0x01 0x08 0x11 0x10 0x08 0x11 0x0C 0x10 0x01 0x04 0x00
0x11 0x08 0x40|0x01 0x08|0x11 0x10 0x08|0x11 0x0C 0x10|0x01 0x04|0x00
RUN0          |RUN1     |RUN2          |RUN3          |RUN4     |END
*/

// **************************************************************************************
//
//   CRunsBuffer
//

template <typename CHAR>
class CRunsBuffer
{
public:
    CRunsBuffer()
    {
        RunsBuffer = NULL;
        RunsMax = 0;
        Clear();
    }

    ~CRunsBuffer()
    {
        if (RunsBuffer != NULL)
        {
            delete[] RunsBuffer;
            RunsBuffer = NULL;
        }
        Clear();
    }

    void Clear()
    {
        RunsLen = 0;
        LastLCN = 0;
    }

    BOOL Enlarge(DWORD newLen)
    {
        if (newLen > RunsMax)
        {
            const int RUN_DELTA = 1024; // size of buffer enlargement for data runs encoding
            DWORD newRunsMax = RunsMax + newLen + RUN_DELTA;
            BYTE* temp = new BYTE[newRunsMax];
            if (temp == NULL)
                return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
            RunsMax = newRunsMax;
            if (RunsBuffer != NULL)
            {
                memcpy(temp, RunsBuffer, RunsLen);
                delete[] RunsBuffer;
            }
            RunsBuffer = temp;
        }
        return TRUE;
    }

    // 'lcn' is unsigned value of first logical cluster number (0 to 2^32) for this data run
    // 'length' is number of cluster in this data run, must be QWORD, it is memcpy source!
    BOOL EncodeRun(QWORD lcn, QWORD length)
    {
        CALL_STACK_MESSAGE_NONE
        //CALL_STACK_MESSAGE3("CRunsBuffer::EncodeDataRun(%d, %d)", lcn, length);

        // offset is signed value, it could contain negative values
        QWORD offset = lcn - LastLCN;
        LastLCN = lcn;

        const QWORD MASK1 = 0xFFFFFFFFFFFFFF80;
        const QWORD MASK2 = 0xFFFFFFFFFFFF8000;
        const QWORD MASK3 = 0xFFFFFFFFFF800000;
        const QWORD MASK4 = 0xFFFFFFFF80000000;
        const QWORD MASK5 = 0xFFFFFF8000000000;
        const QWORD MASK6 = 0xFFFF800000000000;
        const QWORD MASK7 = 0xFF80000000000000;
        const QWORD MASK8 = 0x8000000000000000;
        BYTE lengthBytes;
        BYTE offsetBytes;

        if ((length & MASK1) == 0 || (length & MASK1) == MASK1)
            lengthBytes = 1;
        else if ((length & MASK2) == 0 || (length & MASK2) == MASK2)
            lengthBytes = 2;
        else if ((length & MASK3) == 0 || (length & MASK3) == MASK3)
            lengthBytes = 3;
        else if ((length & MASK4) == 0 || (length & MASK4) == MASK4)
            lengthBytes = 4;
        else if ((length & MASK5) == 0 || (length & MASK5) == MASK5)
            lengthBytes = 5;
        else if ((length & MASK6) == 0 || (length & MASK6) == MASK6)
            lengthBytes = 6;
        else if ((length & MASK7) == 0 || (length & MASK7) == MASK7)
            lengthBytes = 7;
        else
            lengthBytes = 8;

        if ((offset & MASK1) == 0 || (offset & MASK1) == MASK1)
            offsetBytes = 1;
        else if ((offset & MASK2) == 0 || (offset & MASK2) == MASK2)
            offsetBytes = 2;
        else if ((offset & MASK3) == 0 || (offset & MASK3) == MASK3)
            offsetBytes = 3;
        else if ((offset & MASK4) == 0 || (offset & MASK4) == MASK4)
            offsetBytes = 4;
        else if ((offset & MASK5) == 0 || (offset & MASK5) == MASK5)
            offsetBytes = 5;
        else if ((offset & MASK6) == 0 || (offset & MASK6) == MASK6)
            offsetBytes = 6;
        else if ((offset & MASK7) == 0 || (offset & MASK7) == MASK7)
            offsetBytes = 7;
        else
            offsetBytes = 8;

        // does it look like partition with more than 2^32 clusters?
        if (lengthBytes > 5 || offsetBytes > 5)
            TRACE_E("Interesting: lengthBytes > 5 || offsetBytes > 5");

        if (!Enlarge(RunsLen + 1 + lengthBytes + offsetBytes + 1)) // +1 for header, +1 for terminator (only last run will have terminator)
            return FALSE;

        RunsBuffer[RunsLen] = (offsetBytes << 4) | lengthBytes;
        RunsLen += 1;
        memcpy(RunsBuffer + RunsLen, &length, lengthBytes);
        RunsLen += lengthBytes;
        memcpy(RunsBuffer + RunsLen, &offset, offsetBytes);
        RunsLen += offsetBytes;

        RunsBuffer[RunsLen] = 0; // terminator

        return TRUE;
    }

public:
    BYTE* RunsBuffer; // allocated buffer
    DWORD RunsLen;    // number of valid bytes
    DWORD RunsMax;    // number of allocated bytes
    QWORD LastLCN;    // last LCN for subsequent EncodeDataRun() calls
};

// **************************************************************************************
//
//   CRunsWalker
//

inline QWORD GetRunValueUnsigned(const BYTE* run, BYTE length)
{
    QWORD val = 0;
    for (int i = 0; i < length; i++)
    {
        val |= (QWORD)*run << (i * 8);
        run++;
    }
    return val;
}

inline _int64 GetRunValueSigned(const BYTE* run, BYTE length)
{
    QWORD val = 0;
    for (int i = 0; i < length; i++)
    {
        val |= (QWORD)*run << (i * 8);
        run++;
    }
    if (val & ((QWORD)0x80 << (length - 1) * 8))
        val |= (0xFFFFFFFFFFFFFFFF << length * 8);
    return val;
}

struct CRunsWalkerQuery
{
    BOOL LastRun;
    BOOL NewPointers;
    DWORD DPFlags;
};

template <typename CHAR>
class CRunsWalker
{
public:
    CRunsWalker()
    {
        StreamPtrs = NULL;
    }

    CRunsWalker(const DATA_POINTERS* streamPtrs)
    {
        Init(streamPtrs);
    }

    void Init(const DATA_POINTERS* streamPtrs)
    {
        StreamPtrs = streamPtrs;
        CurrentPtrs = StreamPtrs;
        CurrentRun = CurrentPtrs->Runs;
        CurrentLCN = 0;
    }

    // returns TRUE on success, FALSE for errors
    // when returns TRUE, following values are returned:
    //   'lcn' returns -1 for sparse run, LCN otherwise
    //   'length' returns number of clusters
    //   'query' (if it is not NULL) returns detailed information about current run
    BOOL GetNextRun(QWORD* lcn, QWORD* length, CRunsWalkerQuery* query)
    {
#ifdef _DEBUG
        if (StreamPtrs == NULL)
        {
            TRACE_E("Internal error - object not initialized");
            return FALSE;
        }
#endif

        BOOL newPtrs = (StreamPtrs == CurrentPtrs) && (CurrentPtrs != NULL) && (CurrentRun == CurrentPtrs->Runs);

        // read data run header, eventually move to next data pointers ($DATA)
        BYTE runHeader;
        do
        {
            // check array range
            if (CurrentRun >= CurrentPtrs->Runs + CurrentPtrs->RunsSize)
            {
                TRACE_E("Out of data runs!");
                SetLastError(ERROR_BUFFER_OVERFLOW);
                return FALSE;
            }

            runHeader = *CurrentRun;
            CurrentRun++; // move iterator to next byte

            if (runHeader == 0)
            {
                // do we have another data pointers?
                if (CurrentPtrs->DPNext == NULL)
                {
                    TRACE_E("Out of data pointers!");
                    SetLastError(ERROR_CLUSTERLOG_CORRUPT);
                    return FALSE;
                }
                // are they connected?
                if (CurrentPtrs->LastVCN != CurrentPtrs->DPNext->StartVCN - 1)
                {
                    TRACE_E("Next data runs are not connected!");
                    SetLastError(ERROR_CLUSTERLOG_CORRUPT);
                    return FALSE;
                }
                // switch to next data pointers
                CurrentPtrs = CurrentPtrs->DPNext;
                CurrentRun = CurrentPtrs->Runs;
                CurrentLCN = 0; // each data pointers starting first offset from 0
                newPtrs = TRUE;
            }
        } while (runHeader == 0); // skip all 0x00 headers

        // decode data run header
        BYTE lengthBytes = (runHeader & 0x0F);
        BYTE offsetBytes = (runHeader & 0xF0) >> 4;
        // check array range
        if (CurrentRun + lengthBytes + offsetBytes > CurrentPtrs->Runs + CurrentPtrs->RunsSize)
        {
            TRACE_E("Data runs damaged!");
            SetLastError(ERROR_BUFFER_OVERFLOW);
            return FALSE;
        }

        // does it look like partition with more than 2^32 clusters?
        if (lengthBytes > 5 || offsetBytes > 5)
            TRACE_E("Interesting: lengthBytes > 5 || offsetBytes > 5");

        // get section values
        *length = GetRunValueUnsigned(CurrentRun, lengthBytes);
        CurrentRun += lengthBytes;
        _int64 offset = GetRunValueSigned(CurrentRun, offsetBytes);
        CurrentRun += offsetBytes;

        if (offset == 0)
        {
            *lcn = -1; // sparse run
        }
        else
        {
            if (offset < 0 && (QWORD)(-offset) > CurrentLCN)
            {
                TRACE_E("Invalid data run offset!");
                return FALSE;
            }
            CurrentLCN += offset;
            *lcn = CurrentLCN;
        }

        if (query != NULL)
        {
            if (CurrentRun >= CurrentPtrs->Runs + CurrentPtrs->RunsSize)
                TRACE_E("Out of data runs!");
            else
                query->LastRun = (*CurrentRun == 0) && (CurrentPtrs->DPNext == NULL);
            query->NewPointers = newPtrs;
            query->DPFlags = CurrentPtrs->DPFlags;
        }

        return TRUE;
    }

public:
    const DATA_POINTERS* StreamPtrs;  // stream data pointers
    const DATA_POINTERS* CurrentPtrs; // iterator pointing to current data pointers
    const BYTE* CurrentRun;           // iterator pointing to current data run
    QWORD CurrentLCN;                 // logic cluster iterator
};
