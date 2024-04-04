// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "worker.h"

#include <Aclapi.h>
#include <Ntsecapi.h>

// these functions do not have a header, we need to load them dynamically
NTQUERYINFORMATIONFILE DynNtQueryInformationFile = NULL;
NTFSCONTROLFILE DynNtFsControlFile = NULL;

COperationsQueue OperationsQueue; // queue of disk operations

// if defined, various debug messages are written to TRACE
//#define WORKER_COPY_DEBUG_MSG

// comment out if I don't want to track all those messages from the progress of the asynchronous copy algorithm
//#define ASYNC_COPY_DEBUG_MSG

//
// ****************************************************************************
// CTransferSpeedMeter
//

CTransferSpeedMeter::CTransferSpeedMeter()
{
    Clear();
}

void CTransferSpeedMeter::Clear()
{
    ActIndexInTrBytes = 0;
    ActIndexInTrBytesTimeLim = 0;
    CountOfTrBytesItems = 0;
    ActIndexInLastPackets = 0;
    CountOfLastPackets = 0;
    ResetSpeed = TRUE;
    MaxPacketSize = 0;
}

void CTransferSpeedMeter::GetSpeed(CQuadWord* speed)
{
    CALL_STACK_MESSAGE1("CTransferSpeedMeter::GetSpeed()");

    DWORD time = GetTickCount();

    if (CountOfLastPackets >= 2)
    { // we will test if it is not a low speed (calculation from LastPacketsSize and LastPacketsTime)
        int firstPacket = ((TRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - CountOfLastPackets) % (TRSPMETER_NUMOFSTOREDPACKETS + 1);
        int lastPacket = ((TRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - 1) % (TRSPMETER_NUMOFSTOREDPACKETS + 1);
        DWORD lastPacketTime = LastPacketsTime[lastPacket];
        DWORD totalTime = lastPacketTime - LastPacketsTime[firstPacket]; // time between receiving the first and last packet
        if (totalTime >= ((DWORD)(CountOfLastPackets - 1) * TRSPMETER_STPCKTSMININTERVAL) / TRSPMETER_NUMOFSTOREDPACKETS)
        {                                     // it is about low speed (up to TRSPMETER_NUMOFSTOREDPACKETS packets in TRSPMETER_STPCKTSMININTERVAL ms)
            if (time - lastPacketTime > 2000) // two-minute "guard" time for the last calculated slow speed
            {                                 // we will try if it is not more than double the slowdown compared to the speed of the last packet, if so, we will display
                                              // zero speed (so that we do not always show the last received speed value when stopping a slow transfer)
                int preLastPacket = ((TRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - 2) % (TRSPMETER_NUMOFSTOREDPACKETS + 1);
                if ((UINT64)2 * MaxPacketSize * (lastPacketTime - LastPacketsTime[preLastPacket]) < (UINT64)LastPacketsSize[lastPacket] * (time - lastPacketTime))
                {
                    speed->SetUI64(0);
                    ResetSpeed = TRUE;
                    return; // speed decreased at least twice, better show zero
                }
            }
            if (totalTime > TRSPMETER_ACTSPEEDSTEP * TRSPMETER_ACTSPEEDNUMOFSTEPS)
            { // we will calculate the speed only from the data in the nearest time TRSPMETER_ACTSPEEDSTEP * TRSPMETER_ACTSPEEDNUMOFSTEPS
                // (if packets start arriving slowly, there may be packets in the queue from the last five minutes - however, we
                // we calculate "instantaneous" speed, not the average over the last five minutes)
                int i = firstPacket;
                while (1)
                {
                    if (++i >= TRSPMETER_NUMOFSTOREDPACKETS + 1)
                        i = 0;
                    if (i == lastPacket || lastPacketTime - LastPacketsTime[i] < TRSPMETER_ACTSPEEDSTEP * TRSPMETER_ACTSPEEDNUMOFSTEPS)
                        break;
                    firstPacket = i;
                }
                totalTime = lastPacketTime - LastPacketsTime[firstPacket];
            }
            UINT64 totalSize = 0; // sum of all packet sizes except the first one, which is omitted (only its time is taken)
            do
            {
                if (++firstPacket >= TRSPMETER_NUMOFSTOREDPACKETS + 1)
                    firstPacket = 0;
                totalSize += LastPacketsSize[firstPacket];
            } while (firstPacket != lastPacket);
            speed->SetUI64((1000 * totalSize) / totalTime);
            return; // We have calculated a low speed, we are finishing
        }
        else // it is about high speed (more than TRSPMETER_NUMOFSTOREDPACKETS packets in TRSPMETER_STPCKTSMININTERVAL ms),
        {    // we will perform a test for sudden speed drop (especially when zero files start to be copied or empty directories are created)
            if (time - lastPacketTime > 800)
            { // if no packet has arrived within 800ms, we will report zero speed
                speed->SetUI64(0);
                ResetSpeed = TRUE;
                return;
            }
        }
    }
    else // There is nothing to calculate, currently reporting "0 B/s"
    {
        speed->SetUI64(0);
        return;
    }
    // high speed (more than TRSPMETER_NUMOFSTOREDPACKETS packets in TRSPMETER_STPCKTSMININTERVAL ms)
    if (CountOfTrBytesItems > 0) // after establishing the connection, it is "always true"
    {
        int actIndexAdded = 0;                           // 0 = current index was not counted, 1 = current index was counted
        int emptyTrBytes = 0;                            // number of counted empty steps
        UINT64 total = 0;                                // total number of apartments for the last max. TRSPMETER_ACTSPEEDNUMOFSTEPS steps
        int addFromTrBytes = CountOfTrBytesItems - 1;    // number of steps closed to add from the queue
        DWORD restTime = 0;                              // time elapsed from the last counted step to this moment
        if ((int)(time - ActIndexInTrBytesTimeLim) >= 0) // current index is already closed + empty steps may be needed
        {
            emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / TRSPMETER_ACTSPEEDSTEP;
            restTime = (time - ActIndexInTrBytesTimeLim) % TRSPMETER_ACTSPEEDSTEP;
            emptyTrBytes = min(emptyTrBytes, TRSPMETER_ACTSPEEDNUMOFSTEPS);
            if (emptyTrBytes < TRSPMETER_ACTSPEEDNUMOFSTEPS) // Not only empty steps are enough, we will also count the activity index
            {
                total = TransferedBytes[ActIndexInTrBytes];
                actIndexAdded = 1;
            }
            addFromTrBytes = TRSPMETER_ACTSPEEDNUMOFSTEPS - actIndexAdded - emptyTrBytes;
            addFromTrBytes = min(addFromTrBytes, CountOfTrBytesItems - 1); // how many closed steps are still pending from the queue
        }
        else
        {
            restTime = time + TRSPMETER_ACTSPEEDSTEP - ActIndexInTrBytesTimeLim;
            total = TransferedBytes[ActIndexInTrBytes];
        }

        int actIndex = ActIndexInTrBytes;
        int i;
        for (i = 0; i < addFromTrBytes; i++)
        {
            if (--actIndex < 0)
                actIndex = TRSPMETER_ACTSPEEDNUMOFSTEPS; // movement along a circular queue
            total += TransferedBytes[actIndex];
        }
        DWORD t = (addFromTrBytes + actIndexAdded + emptyTrBytes) * TRSPMETER_ACTSPEEDSTEP + restTime;
        if (t > 0)
            speed->SetUI64((total * 1000) / t);
        else
            speed->SetUI64(0); // There is nothing to calculate, currently reporting "0 B/s"
    }
    else
        speed->SetUI64(0); // There is nothing to calculate, currently reporting "0 B/s"
}

void CTransferSpeedMeter::JustConnected()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CTransferSpeedMeter::JustConnected()");

    TransferedBytes[0] = 0;
    ActIndexInTrBytes = 0;
    ActIndexInTrBytesTimeLim = (LastPacketsTime[0] = GetTickCount()) + TRSPMETER_ACTSPEEDSTEP;
    CountOfTrBytesItems = 1;
    LastPacketsSize[0] = 0;
    ActIndexInLastPackets = 1;
    CountOfLastPackets = 1;
    ResetSpeed = TRUE;
    MaxPacketSize = 0;
}

void CTransferSpeedMeter::BytesReceived(DWORD count, DWORD time, DWORD maxPacketSize)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CTransferSpeedMeter::BytesReceived(, ,)"); // we ignore parameters for performance reasons (call-stack is already slowing down)

    MaxPacketSize = maxPacketSize;

    if (count > 0)
    {
        //    if (count > MaxPacketSize)  // deje se pri zmenach rychlosti (kvuli SpeedLimit nebo ProgressBufferLimit), prichazi pakety nactene jeste se starou velikosti bufferu
        //      TRACE_E("CTransferSpeedMeter::BytesReceived(): count > MaxPacketSize (" << count << " > " << MaxPacketSize << ")");

        if (ResetSpeed)
            ResetSpeed = FALSE;

        LastPacketsSize[ActIndexInLastPackets] = count;
        LastPacketsTime[ActIndexInLastPackets] = time;
        if (++ActIndexInLastPackets >= TRSPMETER_NUMOFSTOREDPACKETS + 1)
            ActIndexInLastPackets = 0;
        if (CountOfLastPackets < TRSPMETER_NUMOFSTOREDPACKETS + 1)
            CountOfLastPackets++;
    }
    if ((int)(time - ActIndexInTrBytesTimeLim) < 0) // is in the current time interval, we just add the number of bytes to the interval
    {
        TransferedBytes[ActIndexInTrBytes] += count;
    }
    else // is not in the current time interval, we need to create a new interval
    {
        int emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / TRSPMETER_ACTSPEEDSTEP;
        int i = min(emptyTrBytes, TRSPMETER_ACTSPEEDNUMOFSTEPS); // no longer has an effect (the entire queue is reset)
        if (i > 0 && CountOfTrBytesItems <= TRSPMETER_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems = min(TRSPMETER_ACTSPEEDNUMOFSTEPS + 1, CountOfTrBytesItems + i);
        while (i--)
        {
            if (++ActIndexInTrBytes > TRSPMETER_ACTSPEEDNUMOFSTEPS)
                ActIndexInTrBytes = 0; // movement along a circular queue
            TransferedBytes[ActIndexInTrBytes] = 0;
        }
        ActIndexInTrBytesTimeLim += (emptyTrBytes + 1) * TRSPMETER_ACTSPEEDSTEP;
        if (++ActIndexInTrBytes > TRSPMETER_ACTSPEEDNUMOFSTEPS)
            ActIndexInTrBytes = 0; // movement along a circular queue
        if (CountOfTrBytesItems <= TRSPMETER_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems++;
        TransferedBytes[ActIndexInTrBytes] = count;
    }
}

void CTransferSpeedMeter::AdjustProgressBufferLimit(DWORD* progressBufferLimit, DWORD lastFileBlockCount,
                                                    DWORD lastFileStartTime)
{
    if (CountOfLastPackets > 1 && lastFileBlockCount > 0) // "always true": CountOfLastPackets is 1 at the beginning of the file (2 = we have one packet)
    {
        unsigned __int64 size = 0; // total size of stored packets of the last file
        int i = ((TRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - 1) % (TRSPMETER_NUMOFSTOREDPACKETS + 1);
        int c = min((DWORD)(CountOfLastPackets - 1), lastFileBlockCount);
        int packets = c;
        DWORD ti = GetTickCount();
        while (c--)
        {
            size += LastPacketsSize[i];
            if (i-- == 0)
                i = TRSPMETER_NUMOFSTOREDPACKETS;
            if (ti - LastPacketsTime[i] > 2000)
            {
                packets -= c;
                break; // we take max. 2 seconds old packets (trying to calculate the "current" speed)
            }
        }
        DWORD totalTime = min(ti - LastPacketsTime[i], ti - lastFileStartTime); // LastPacketsTime[i] can be older than lastFileStartTime (it is the last packet of the previous file), we are only interested in the time spent in this file
        if (totalTime == 0)
            totalTime = 10; // We consider 0ms as 10ms (approximately one step of GetTickCount())
        unsigned __int64 speed = (size * 1000) / totalTime;
        DWORD bufLimit = ASYNC_SLOW_COPY_BUF_SIZE;
        while (bufLimit < ASYNC_COPY_BUF_SIZE)
        {
            // experimentally found that Windows 7 loves buffer size 32KB, utilization curve
            // network lines are usually beautifully smooth with it, whereas at 64KB it jumps like a bitch
            // and the overall achieved speed is about 5% lower ... so dirty bloody hack: we will also
            // prefer 32KB ... up to 8 * 128 (1024KB/s) ... we will skip 64KB and 128KB, next
            // buffer limit is up to 256KB
            // +
            // we will take measures against oscillation between two buffer limit sizes at the boundary speed
            // between two buffer size limits, increasing by one level will be more difficult (choose the same buffer)
            // speed can be up to 9 * bufLimit instead of the standard 8 * bufLimit
            if (bufLimit == 32 * 1024) // For a 32KB buffer limit, we will use values for a 128KB buffer limit (instead of 64KB and 128KB we will choose 32KB)
            {
                if (speed <= (bufLimit == *progressBufferLimit ? 9 * 128 * 1024 : 8 * 128 * 1024))
                    break;
                bufLimit = 256 * 1024; // 32KB didn't work out, let's try 256KB (64KB and 128KB are not an option, that would select 32KB)
            }
            else
            {
                if (speed <= (bufLimit == *progressBufferLimit ? 9 * bufLimit : 8 * bufLimit))
                    break;
                bufLimit *= 2;
            }
        }
        if (bufLimit > ASYNC_COPY_BUF_SIZE)
            bufLimit = ASYNC_COPY_BUF_SIZE;
        *progressBufferLimit = bufLimit;
#ifdef WORKER_COPY_DEBUG_MSG
        TRACE_I("AdjustProgressBufferLimit(): speed=" << speed / 1024.0 << " KB/s, size=" << size << " B, packets=" << packets << ", new buffer limit=" << bufLimit);
#endif // WORKER_COPY_DEBUG_MSG
    }
    else
        TRACE_E("Unexpected situation in CTransferSpeedMeter::AdjustProgressBufferLimit()!");
}

//
// ****************************************************************************
// CProgressSpeedMeter
//

CProgressSpeedMeter::CProgressSpeedMeter()
{
    Clear();
}

void CProgressSpeedMeter::Clear()
{
    ActIndexInTrBytes = 0;
    ActIndexInTrBytesTimeLim = 0;
    CountOfTrBytesItems = 0;
    ActIndexInLastPackets = 0;
    CountOfLastPackets = 0;
    MaxPacketSize = 0;
}

void CProgressSpeedMeter::GetSpeed(CQuadWord* speed)
{
    CALL_STACK_MESSAGE1("CProgressSpeedMeter::GetSpeed()");

    DWORD time = GetTickCount();

    if (CountOfLastPackets >= 2)
    { // we will test if it is not a low speed (calculation from LastPacketsSize and LastPacketsTime)
        int firstPacket = ((PRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - CountOfLastPackets) % (PRSPMETER_NUMOFSTOREDPACKETS + 1);
        int lastPacket = ((PRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - 1) % (PRSPMETER_NUMOFSTOREDPACKETS + 1);
        DWORD lastPacketTime = LastPacketsTime[lastPacket];
        DWORD totalTime = lastPacketTime - LastPacketsTime[firstPacket]; // time between receiving the first and last packet
        if (totalTime >= ((DWORD)(CountOfLastPackets - 1) * PRSPMETER_STPCKTSMININTERVAL) / PRSPMETER_NUMOFSTOREDPACKETS)
        {                                     // it is about low speed (up to PRSPMETER_NUMOFSTOREDPACKETS packets in PRSPMETER_STPCKTSMININTERVAL ms)
            if (time - lastPacketTime > 5000) // Five-minute "guard" period for the last counted slow speed
            {                                 // Let's try if it's not more than four times slower than the speed of the last packet, if so, we will display
                                              // zero speed (so that we don't show the last received time-left value when stopping a slow transfer)
                int preLastPacket = ((PRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - 2) % (PRSPMETER_NUMOFSTOREDPACKETS + 1);
                if ((UINT64)4 * MaxPacketSize * (lastPacketTime - LastPacketsTime[preLastPacket]) < (UINT64)LastPacketsSize[lastPacket] * (time - lastPacketTime))
                {
                    speed->SetUI64(0);
                    return; // speed decreased at least twice, better show zero
                }
            }
            if (totalTime > PRSPMETER_ACTSPEEDSTEP * PRSPMETER_ACTSPEEDNUMOFSTEPS)
            { // we will calculate the speed only from the data closest to the PRSPMETER_ACTSPEEDSTEP * PRSPMETER_ACTSPEEDNUMOFSTEPS
                // (if packets start arriving slowly, there may be packets in the queue from the last five minutes - however, we
                // we calculate the speed for the last X seconds, not the average for the last five minutes)
                int i = firstPacket;
                while (1)
                {
                    if (++i >= PRSPMETER_NUMOFSTOREDPACKETS + 1)
                        i = 0;
                    if (i == lastPacket || lastPacketTime - LastPacketsTime[i] < PRSPMETER_ACTSPEEDSTEP * PRSPMETER_ACTSPEEDNUMOFSTEPS)
                        break;
                    firstPacket = i;
                }
                totalTime = lastPacketTime - LastPacketsTime[firstPacket];
            }
            UINT64 totalSize = 0; // sum of all packet sizes except the first one, which is omitted (only its time is taken)
            do
            {
                if (++firstPacket >= PRSPMETER_NUMOFSTOREDPACKETS + 1)
                    firstPacket = 0;
                totalSize += LastPacketsSize[firstPacket];
            } while (firstPacket != lastPacket);
            speed->SetUI64((1000 * totalSize) / totalTime);
            return; // We have calculated a low speed, we are finishing
        }
        else // it is about high speed (more than PRSPMETER_NUMOFSTOREDPACKETS packets in PRSPMETER_STPCKTSMININTERVAL ms),
        {    // we will perform a test for sudden speed drop (especially when zero files start to be copied or empty directories are created)
            if (time - lastPacketTime > 5000)
            { // if no packet has arrived within 5000ms, we will report zero speed
                speed->SetUI64(0);
                return;
            }
        }
    }
    else // There is nothing to calculate, currently reporting "0 B/s"
    {
        speed->SetUI64(0);
        return;
    }
    // high speed (more than PRSPMETER_NUMOFSTOREDPACKETS packets in PRSPMETER_STPCKTSMININTERVAL ms)
    if (CountOfTrBytesItems > 0) // after establishing the connection, it is "always true"
    {
        int actIndexAdded = 0;                           // 0 = current index was not counted, 1 = current index was counted
        int emptyTrBytes = 0;                            // number of counted empty steps
        UINT64 total = 0;                                // total number of apartments for the last max. PRSPMETER_ACTSPEEDNUMOFSTEPS steps
        int addFromTrBytes = CountOfTrBytesItems - 1;    // number of steps closed to add from the queue
        DWORD restTime = 0;                              // time elapsed from the last counted step to this moment
        if ((int)(time - ActIndexInTrBytesTimeLim) >= 0) // current index is already closed + empty steps may be needed
        {
            emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / PRSPMETER_ACTSPEEDSTEP;
            restTime = (time - ActIndexInTrBytesTimeLim) % PRSPMETER_ACTSPEEDSTEP;
            emptyTrBytes = min(emptyTrBytes, PRSPMETER_ACTSPEEDNUMOFSTEPS);
            if (emptyTrBytes < PRSPMETER_ACTSPEEDNUMOFSTEPS) // Not only empty steps are enough, we will also count the activity index
            {
                total = TransferedBytes[ActIndexInTrBytes];
                actIndexAdded = 1;
            }
            addFromTrBytes = PRSPMETER_ACTSPEEDNUMOFSTEPS - actIndexAdded - emptyTrBytes;
            addFromTrBytes = min(addFromTrBytes, CountOfTrBytesItems - 1); // how many closed steps are still pending from the queue
        }
        else
        {
            restTime = time + PRSPMETER_ACTSPEEDSTEP - ActIndexInTrBytesTimeLim;
            total = TransferedBytes[ActIndexInTrBytes];
        }

        int actIndex = ActIndexInTrBytes;
        int i;
        for (i = 0; i < addFromTrBytes; i++)
        {
            if (--actIndex < 0)
                actIndex = PRSPMETER_ACTSPEEDNUMOFSTEPS; // movement along a circular queue
            total += TransferedBytes[actIndex];
        }
        DWORD t = (addFromTrBytes + actIndexAdded + emptyTrBytes) * PRSPMETER_ACTSPEEDSTEP + restTime;
        if (t > 0)
            speed->SetUI64((total * 1000) / t);
        else
            speed->SetUI64(0); // There is nothing to calculate, currently reporting "0 B/s"
    }
    else
        speed->SetUI64(0); // There is nothing to calculate, currently reporting "0 B/s"
}

void CProgressSpeedMeter::JustConnected()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CProgressSpeedMeter::JustConnected()");

    TransferedBytes[0] = 0;
    ActIndexInTrBytes = 0;
    ActIndexInTrBytesTimeLim = (LastPacketsTime[0] = GetTickCount()) + PRSPMETER_ACTSPEEDSTEP;
    CountOfTrBytesItems = 1;
    LastPacketsSize[0] = 0;
    ActIndexInLastPackets = 1;
    CountOfLastPackets = 1;
    MaxPacketSize = 0;
}

void CProgressSpeedMeter::BytesReceived(DWORD count, DWORD time, DWORD maxPacketSize)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CProgressSpeedMeter::BytesReceived(, ,)"); // we ignore parameters for performance reasons (call-stack is already slowing down)

    MaxPacketSize = maxPacketSize;

    if (count > 0)
    {
        //    if (count > MaxPacketSize)  // deje se pri zmenach rychlosti (kvuli SpeedLimit nebo ProgressBufferLimit), prichazi pakety nactene jeste se starou velikosti bufferu
        //      TRACE_E("CProgressSpeedMeter::BytesReceived(): count > MaxPacketSize (" << count << " > " << MaxPacketSize << ")");

        LastPacketsSize[ActIndexInLastPackets] = count;
        LastPacketsTime[ActIndexInLastPackets] = time;
        if (++ActIndexInLastPackets >= PRSPMETER_NUMOFSTOREDPACKETS + 1)
            ActIndexInLastPackets = 0;
        if (CountOfLastPackets < PRSPMETER_NUMOFSTOREDPACKETS + 1)
            CountOfLastPackets++;
    }
    if ((int)(time - ActIndexInTrBytesTimeLim) < 0) // is in the current time interval, we just add the number of bytes to the interval
    {
        TransferedBytes[ActIndexInTrBytes] += count;
    }
    else // is not in the current time interval, we need to create a new interval
    {
        int emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / PRSPMETER_ACTSPEEDSTEP;
        int i = min(emptyTrBytes, PRSPMETER_ACTSPEEDNUMOFSTEPS); // no longer has an effect (the entire queue is reset)
        if (i > 0 && CountOfTrBytesItems <= PRSPMETER_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems = min(PRSPMETER_ACTSPEEDNUMOFSTEPS + 1, CountOfTrBytesItems + i);
        while (i--)
        {
            if (++ActIndexInTrBytes > PRSPMETER_ACTSPEEDNUMOFSTEPS)
                ActIndexInTrBytes = 0; // movement along a circular queue
            TransferedBytes[ActIndexInTrBytes] = 0;
        }
        ActIndexInTrBytesTimeLim += (emptyTrBytes + 1) * PRSPMETER_ACTSPEEDSTEP;
        if (++ActIndexInTrBytes > PRSPMETER_ACTSPEEDNUMOFSTEPS)
            ActIndexInTrBytes = 0; // movement along a circular queue
        if (CountOfTrBytesItems <= PRSPMETER_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems++;
        TransferedBytes[ActIndexInTrBytes] = count;
    }
}

//
// ****************************************************************************
// COperations
//

COperations::COperations(int base, int delta, char* waitInQueueSubject, char* waitInQueueFrom,
                         char* waitInQueueTo) : TDirectArray<COperation>(base, delta), Sizes(1, 400)
{
    TotalSize = CQuadWord(0, 0);
    CompressedSize = CQuadWord(0, 0);
    OccupiedSpace = CQuadWord(0, 0);
    TotalFileSize = CQuadWord(0, 0);
    FreeSpace = CQuadWord(0, 0);
    BytesPerCluster = 0;
    ClearReadonlyMask = 0xFFFFFFFF;
    InvertRecycleBin = FALSE;
    CanUseRecycleBin = TRUE;
    SameRootButDiffVolume = FALSE;
    TargetPathSupADS = FALSE;
    //  TargetPathSupEFS = FALSE;
    IsCopyOrMoveOperation = FALSE;
    OverwriteOlder = FALSE;
    CopySecurity = FALSE;
    PreserveDirTime = FALSE;
    SourcePathIsNetwork = FALSE;
    CopyAttrs = FALSE;
    StartOnIdle = FALSE;
    ShowStatus = FALSE;
    IsCopyOperation = FALSE;
    FastMoveUsed = FALSE;
    ChangeSpeedLimit = FALSE;
    FilesCount = 0;
    DirsCount = 0;
    RemapNameFrom = NULL;
    RemapNameFromLen = 0;
    RemapNameTo = NULL;
    RemapNameToLen = 0;
    RemovableTgtDisk = FALSE;
    RemovableSrcDisk = FALSE;
    SkipAllCountSizeErrors = FALSE;
    WorkPath1[0] = 0;
    WorkPath1InclSubDirs = FALSE;
    WorkPath2[0] = 0;
    WorkPath2InclSubDirs = FALSE;
    WaitInQueueSubject = waitInQueueSubject; // Released in FreeScript()
    WaitInQueueFrom = waitInQueueFrom;       // Released in FreeScript()
    WaitInQueueTo = waitInQueueTo;           // Released in FreeScript()
    HANDLES(InitializeCriticalSection(&StatusCS));
    TransferredFileSize = CQuadWord(0, 0);
    ProgressSize = CQuadWord(0, 0);
    UseSpeedLimit = FALSE;
    SpeedLimit = 1;
    SleepAfterWrite = -1;
    LastBufferLimit = 1;
    LastSetupTime = GetTickCount();
    BytesTrFromLastSetup = CQuadWord(0, 0);
    UseProgressBufferLimit = FALSE;
    ProgressBufferLimit = ASYNC_SLOW_COPY_BUF_SIZE;
    LastProgBufLimTestTime = GetTickCount() - 1000;
    LastFileBlockCount = 0;
    LastFileStartTime = GetTickCount();
}

void COperations::SetTFS(const CQuadWord& TFS)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        TransferredFileSize = TFS;
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::CalcLimitBufferSize(int* limitBufferSize, int bufferSize)
{
    if (limitBufferSize != NULL)
    {
        *limitBufferSize = UseSpeedLimit && SpeedLimit < (DWORD)bufferSize ? (UseProgressBufferLimit && ProgressBufferLimit < SpeedLimit ? ProgressBufferLimit : SpeedLimit) : (UseProgressBufferLimit && ProgressBufferLimit < (DWORD)bufferSize ? ProgressBufferLimit : bufferSize);
    }
}

void COperations::EnableProgressBufferLimit(BOOL useProgressBufferLimit)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        UseProgressBufferLimit = useProgressBufferLimit;
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::SetFileStartParams()
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        LastFileBlockCount = 0;
        LastFileStartTime = GetTickCount();
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::SetTFSandProgressSize(const CQuadWord& TFS, const CQuadWord& pSize,
                                        int* limitBufferSize, int bufferSize)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        TransferredFileSize = TFS;
        ProgressSize = pSize;
        CalcLimitBufferSize(limitBufferSize, bufferSize);
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::GetNewBufSize(int* limitBufferSize, int bufferSize)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        CalcLimitBufferSize(limitBufferSize, bufferSize);
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::AddBytesToSpeedMetersAndTFSandPS(DWORD bytesCount, BOOL onlyToProgressSpeedMeter,
                                                   int bufferSize, int* limitBufferSize, DWORD maxPacketSize)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        DWORD ti = GetTickCount();

        if (maxPacketSize == 0)
            CalcLimitBufferSize((int*)&maxPacketSize, bufferSize);

        DWORD bytesCountForSpeedMeters = bytesCount;
        if (!onlyToProgressSpeedMeter)
        {
            if (limitBufferSize != NULL && bytesCount > 0)
            {
                if (UseSpeedLimit)
                {
                    DWORD sleepNow = 0;
                    if (SleepAfterWrite == -1) // this is the first packet, we will set the speed limit parameters
                    {
                        CalcLimitBufferSize(limitBufferSize, bufferSize);
                        LastBufferLimit = *limitBufferSize;
                        if (SpeedLimit >= HIGH_SPEED_LIMIT)
                        { // There is no risk here that more data will arrive than allowed by the speed limit (HIGH_SPEED_LIMIT must be >= the largest buffer)
                            SleepAfterWrite = 0;
                            BytesTrFromLastSetup.SetUI64(bytesCount);
                        }
                        else
                        {
                            SleepAfterWrite = (1000 * *limitBufferSize) / SpeedLimit; // in the first second of transmission, we assume that the transmission itself is "infinitely fast" (determining the actual speed from the first packet is unrealistic)
                            if (bytesCount > SpeedLimit)                              // There has been a slowdown during the operation (e.g. a read&write of a 32KB buffer has taken place and the speed limit is 1 B/s, so theoretically we should now wait 32768 seconds, which is naturally unrealistic)
                                sleepNow = 1000;                                      // wait a second + measure the speed "admit" only bytes up to the speed limit (e.g. only 1 B)
                            else
                                sleepNow = (SleepAfterWrite * bytesCount) / *limitBufferSize;
                            BytesTrFromLastSetup.SetUI64(0);
                            LastSetupTime = ti + sleepNow;
                        }
                    }
                    else
                    {
                        if ((int)(ti - LastSetupTime) >= 1000 || BytesTrFromLastSetup.Value + bytesCount >= SpeedLimit ||
                            SpeedLimit >= HIGH_SPEED_LIMIT && BytesTrFromLastSetup.Value + bytesCount >= SpeedLimit / HIGH_SPEED_LIMIT_BRAKE_DIV)
                        { // It's time to recalculate the speed limit parameters + possibly "brake"
                            __int64 sleepFromLastSetup64 = (SleepAfterWrite * BytesTrFromLastSetup.Value) / LastBufferLimit;
                            DWORD sleepFromLastSetup = sleepFromLastSetup64 < 1000 ? (DWORD)sleepFromLastSetup64 : 1000;
                            BytesTrFromLastSetup += CQuadWord(bytesCount, 0);
                            __int64 idealTotalTime64 = (1000 * BytesTrFromLastSetup.Value + SpeedLimit - 1) / SpeedLimit; // "+ SpeedLimit - 1" is due to rounding
                            int idealTotalTime = idealTotalTime64 < 10000 ? (int)idealTotalTime64 : 10000;
                            if (idealTotalTime > (int)(ti - LastSetupTime))
                            {
                                sleepNow = idealTotalTime - (ti - LastSetupTime); // we need to slow down (we are faster or just slightly slower than the speed limit)
                                if (sleepNow > 1000)                              // It doesn't make sense to wait longer than a second (up to the "admitted" maximum in the *limitBufferSize gauge)
                                    sleepNow = 1000;
                            }
                            // else sleepNow = 0;  // we are slower than the speed limit (at ideal speed, we would wait a proportional part of SleepAfterWrite)

                            CalcLimitBufferSize(limitBufferSize, bufferSize);
                            LastBufferLimit = *limitBufferSize;

                            if (SpeedLimit >= HIGH_SPEED_LIMIT)
                                SleepAfterWrite = 0;
                            else
                            {
                                int idealTotalSleep = (int)(sleepFromLastSetup + (idealTotalTime - (ti - LastSetupTime)));
                                if (idealTotalSleep > 0) // speed-limit is lower than copying speed, we will slow down for each packet
                                    SleepAfterWrite = (DWORD)(((unsigned __int64)idealTotalSleep * LastBufferLimit) / BytesTrFromLastSetup.Value);
                                else
                                    SleepAfterWrite = 0; // speed-limit is higher than copying speed (no need to slow down)
                            }
                            LastSetupTime = ti + sleepNow;
                            BytesTrFromLastSetup.SetUI64(0);
                        }
                        else // precomputed parameters will be used for intermediate packets
                        {
                            BytesTrFromLastSetup += CQuadWord(bytesCount, 0);
                            *limitBufferSize = LastBufferLimit < bufferSize ? LastBufferLimit : bufferSize;
                            if (SleepAfterWrite > 0)
                            {
                                sleepNow = (SleepAfterWrite * bytesCount) / LastBufferLimit;
                                if (sleepNow > 1000) // Waiting for more than a second doesn't make sense (up to the "admitted" speed limit)
                                    sleepNow = 1000;
                            }
                        }
                    }
                    if (bytesCount > SpeedLimit)               // There has been a slowdown during the operation (e.g. a read&write of a 32KB buffer has taken place and the speed limit is 1 B/s, so theoretically we should now wait 32768 seconds, which is naturally unrealistic)
                        bytesCountForSpeedMeters = SpeedLimit; // for the speed measurement "admit" only entities up to the speed limit (e.g. only 1 B)
                    if (sleepNow > 0)                          // we are braking because of the speed limit
                    {
                        HANDLES(LeaveCriticalSection(&StatusCS));
                        Sleep(sleepNow);
                        HANDLES(EnterCriticalSection(&StatusCS));
                        ti = GetTickCount();
                    }
                }
                else
                    CalcLimitBufferSize(limitBufferSize, bufferSize); // unlimited - full speed (except for ProgressBufferLimit)
            }
            TransferSpeedMeter.BytesReceived(bytesCountForSpeedMeters, ti, maxPacketSize);
            TransferredFileSize.Value += bytesCount;

            if (UseProgressBufferLimit &&
                (++LastFileBlockCount >= ASYNC_SLOW_COPY_BUF_MINBLOCKS || // if there is not enough data for testing
                 ProgressBufferLimit * LastFileBlockCount >= ASYNC_SLOW_COPY_BUF_MINBLOCKS * ASYNC_SLOW_COPY_BUF_SIZE) &&
                ti - LastProgBufLimTestTime >= 1000) // and it's time for another test
            {                                        // we calculate the ProgressBufferLimit for the next round (further reading still uses the current value)
                TransferSpeedMeter.AdjustProgressBufferLimit(&ProgressBufferLimit, LastFileBlockCount, LastFileStartTime);
                LastProgBufLimTestTime = GetTickCount();
                if (LastFileBlockCount > 1000000000)
                    LastFileBlockCount = 1000000; // Protection against overflow (just a bunch of blocks, the exact number is not that important)
            }
        }
        ProgressSpeedMeter.BytesReceived(bytesCountForSpeedMeters, ti, maxPacketSize);
        ProgressSize.Value += bytesCount;
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::AddBytesToTFSandSetProgressSize(const CQuadWord& bytesCount, const CQuadWord& pSize)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        TransferredFileSize += bytesCount;
        ProgressSize = pSize;
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::AddBytesToTFS(const CQuadWord& bytesCount)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        TransferredFileSize += bytesCount;
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::GetTFS(CQuadWord* TFS)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        *TFS = TransferredFileSize;
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::GetTFSandResetTrSpeedIfNeeded(CQuadWord* TFS)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        *TFS = TransferredFileSize;
        if (TransferSpeedMeter.ResetSpeed)
        {
            TransferSpeedMeter.JustConnected();
            if (UseSpeedLimit)
            {
                SleepAfterWrite = -1; // Upon arrival of the first packet, we will perform the calculation
                LastSetupTime = GetTickCount();
                BytesTrFromLastSetup.SetUI64(0);
            }
        }
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::SetProgressSize(const CQuadWord& pSize)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        ProgressSize = pSize;
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

void COperations::GetStatus(CQuadWord* transferredFileSize, CQuadWord* transferSpeed,
                            CQuadWord* progressSize, CQuadWord* progressSpeed,
                            BOOL* useSpeedLimit, DWORD* speedLimit)
{
    HANDLES(EnterCriticalSection(&StatusCS));
    *transferredFileSize = TransferredFileSize;
    *progressSize = ProgressSize;
    TransferSpeedMeter.GetSpeed(transferSpeed);
    ProgressSpeedMeter.GetSpeed(progressSpeed);
    *useSpeedLimit = UseSpeedLimit;
    *speedLimit = SpeedLimit;
    HANDLES(LeaveCriticalSection(&StatusCS));
}

void COperations::InitSpeedMeters(BOOL operInProgress)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        TransferSpeedMeter.JustConnected();
        ProgressSpeedMeter.JustConnected();
        if (UseSpeedLimit)
        {
            SleepAfterWrite = -1; // Upon arrival of the first packet, we will perform the calculation
            LastSetupTime = GetTickCount();
            BytesTrFromLastSetup.SetUI64(0);
        }
        // after a pause, change of speed limit or error dialog, we will discard old data
        if (operInProgress)
        {
            LastFileBlockCount = 0;
            LastFileStartTime = GetTickCount();
            LastProgBufLimTestTime = GetTickCount(); // Let's delay the next test by a second to have relevant data
        }
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
}

BOOL COperations::GetTFSandProgressSize(CQuadWord* transferredFileSize, CQuadWord* progressSize)
{
    if (ShowStatus)
    {
        HANDLES(EnterCriticalSection(&StatusCS));
        *transferredFileSize = TransferredFileSize;
        *progressSize = ProgressSize;
        HANDLES(LeaveCriticalSection(&StatusCS));
    }
    return ShowStatus;
}

void COperations::SetSpeedLimit(BOOL useSpeedLimit, DWORD speedLimit)
{
    HANDLES(EnterCriticalSection(&StatusCS));
    UseSpeedLimit = useSpeedLimit;
    SpeedLimit = speedLimit;
    HANDLES(LeaveCriticalSection(&StatusCS));
}

void COperations::GetSpeedLimit(BOOL* useSpeedLimit, DWORD* speedLimit)
{
    HANDLES(EnterCriticalSection(&StatusCS));
    *useSpeedLimit = UseSpeedLimit;
    *speedLimit = SpeedLimit;
    HANDLES(LeaveCriticalSection(&StatusCS));
}

//
// ****************************************************************************
// CAsyncCopyParams
//

struct CAsyncCopyParams
{
    void* Buffers[8];         // allocated buffers of size ASYNC_COPY_BUF_SIZE bytes
    OVERLAPPED Overlapped[8]; // structures for asynchronous operations

    BOOL UseAsyncAlg; // TRUE = asynchronous algorithm should be used (data needs to be allocated), FALSE = synchronous old algorithm (no allocation needed)

    BOOL HasFailed; // TRUE = failed to create any event in the Overlapped array, the structure is unusable

    CAsyncCopyParams();
    ~CAsyncCopyParams();

    void Init(BOOL useAsyncAlg);

    BOOL Failed() { return HasFailed; }

    DWORD GetOverlappedFlag() { return UseAsyncAlg ? FILE_FLAG_OVERLAPPED : 0; }

    OVERLAPPED* InitOverlapped(int i);                                    // resets and returns Overlapped[i]
    OVERLAPPED* InitOverlappedWithOffset(int i, const CQuadWord& offset); // resets, sets 'offset', and returns Overlapped[i]
    OVERLAPPED* GetOverlapped(int i) { return &Overlapped[i]; }
    void SetOverlappedToEOF(int i, const CQuadWord& offset); // Set Overlapped[i] to the state after completing the asynchronous read that detected EOF
};

CAsyncCopyParams::CAsyncCopyParams()
{
    memset(Buffers, 0, sizeof(Buffers));
    memset(Overlapped, 0, sizeof(Overlapped));
    UseAsyncAlg = FALSE;
    HasFailed = FALSE;
}

void CAsyncCopyParams::Init(BOOL useAsyncAlg)
{
    UseAsyncAlg = useAsyncAlg;
    if (UseAsyncAlg && Buffers[0] == NULL)
    {
        for (int i = 0; i < 8; i++)
        {
            Buffers[i] = malloc(ASYNC_COPY_BUF_SIZE);
            Overlapped[i].hEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
            if (Overlapped[i].hEvent == NULL)
            {
                DWORD err = GetLastError();
                TRACE_E("Unable to create synchronization object for Copy rutine: " << GetErrorText(err));
                HasFailed = TRUE;
            }
        }
    }
}

CAsyncCopyParams::~CAsyncCopyParams()
{
    for (int i = 0; i < 8; i++)
    {
        if (Buffers[i] != NULL)
            free(Buffers[i]);
        if (Overlapped[i].hEvent != NULL)
            HANDLES(CloseHandle(Overlapped[i].hEvent));
    }
}

OVERLAPPED*
CAsyncCopyParams::InitOverlapped(int i)
{
    if (!UseAsyncAlg)
        TRACE_C("CAsyncCopyParams::InitOverlapped(): unexpected call, UseAsyncAlg is FALSE!");
    Overlapped[i].Internal = 0;
    Overlapped[i].InternalHigh = 0;
    Overlapped[i].Offset = 0;
    Overlapped[i].OffsetHigh = 0;
    // Overlapped[i].Pointer = 0;  // it's a union, Pointer overlaps with Offset and OffsetHigh
    return &Overlapped[i];
}

OVERLAPPED*
CAsyncCopyParams::InitOverlappedWithOffset(int i, const CQuadWord& offset)
{
    if (!UseAsyncAlg)
        TRACE_C("CAsyncCopyParams::InitOverlappedWithOffset(): unexpected call, UseAsyncAlg is FALSE!");
    Overlapped[i].Internal = 0;
    Overlapped[i].InternalHigh = 0;
    Overlapped[i].Offset = offset.LoDWord;
    Overlapped[i].OffsetHigh = offset.HiDWord;
    // Overlapped[i].Pointer = 0;  // it's a union, Pointer overlaps with Offset and OffsetHigh
    return &Overlapped[i];
}

void CAsyncCopyParams::SetOverlappedToEOF(int i, const CQuadWord& offset)
{
    if (!UseAsyncAlg)
        TRACE_C("CAsyncCopyParams::SetOverlappedToEOF(): unexpected call, UseAsyncAlg is FALSE!");
    Overlapped[i].Internal = 0xC0000011 /* STATUS_END_OF_FILE*/; // NTSTATUS code equivalent to system error code ERROR_HANDLE_EOF
    Overlapped[i].InternalHigh = 0;
    Overlapped[i].Offset = offset.LoDWord;
    Overlapped[i].OffsetHigh = offset.HiDWord;
    // Overlapped[i].Pointer = 0;  // it's a union, Pointer overlaps with Offset and OffsetHigh
    SetEvent(Overlapped[i].hEvent);
}

// **********************************************************************************

BOOL HaveWriteOwnerRight = FALSE; // Does the process have the WRITE_OWNER right?

void InitWorker()
{
    if (NtDLL != NULL) // "always true"
    {
        DynNtQueryInformationFile = (NTQUERYINFORMATIONFILE)GetProcAddress(NtDLL, "NtQueryInformationFile"); // no header
        DynNtFsControlFile = (NTFSCONTROLFILE)GetProcAddress(NtDLL, "NtFsControlFile");                      // no header
    }
}

void ReleaseWorker()
{
    DynNtQueryInformationFile = NULL;
    DynNtFsControlFile = NULL;
}

struct CWorkerData
{
    COperations* Script;
    HWND HProgressDlg;
    void* Buffer;
    BOOL BufferIsAllocated;
    DWORD ClearReadonlyMask;
    CConvertData* ConvertData;
    HANDLE WContinue;

    HANDLE WorkerNotSuspended;
    BOOL* CancelWorker;
    int* OperationProgress;
    int* SummaryProgress;
};

struct CProgressDlgData
{
    HANDLE WorkerNotSuspended;
    BOOL* CancelWorker;
    int* OperationProgress;
    int* SummaryProgress;

    BOOL OverwriteAll; // Keeps the state of automatic target-to-source rewriting
    BOOL OverwriteHiddenAll;
    BOOL DeleteHiddenAll;
    BOOL EncryptSystemAll;
    BOOL DirOverwriteAll;
    BOOL FileOutLossEncrAll;
    BOOL DirCrLossEncrAll;

    BOOL SkipAllFileWrite; // Has the Skip All button been used yet?
    BOOL SkipAllFileRead;
    BOOL SkipAllOverwrite;
    BOOL SkipAllSystemOrHidden;
    BOOL SkipAllFileOpenIn;
    BOOL SkipAllFileOpenOut;
    BOOL SkipAllOverwriteErr;
    BOOL SkipAllMoveErrors;
    BOOL SkipAllDeleteErr;
    BOOL SkipAllDirCreate;
    BOOL SkipAllDirCreateErr;
    BOOL SkipAllChangeAttrs;
    BOOL SkipAllEncryptSystem;
    BOOL SkipAllFileADSOpenIn;
    BOOL SkipAllFileADSOpenOut;
    BOOL SkipAllGetFileTime;
    BOOL SkipAllSetFileTime;
    BOOL SkipAllFileADSRead;
    BOOL SkipAllFileADSWrite;
    BOOL SkipAllDirOver;
    BOOL SkipAllFileOutLossEncr;
    BOOL SkipAllDirCrLossEncr;

    BOOL IgnoreAllADSReadErr;
    BOOL IgnoreAllADSOpenOutErr;
    BOOL IgnoreAllGetFileTimeErr;
    BOOL IgnoreAllSetFileTimeErr;
    BOOL IgnoreAllSetAttrsErr;
    BOOL IgnoreAllCopyPermErr;
    BOOL IgnoreAllCopyDirTimeErr;

    int CnfrmFileOver; // local copy of Salamander configuration
    int CnfrmDirOver;
    int CnfrmSHFileOver;
    int CnfrmSHFileDel;
    int UseRecycleBin;
    CMaskGroup RecycleMasks;

    BOOL PrepareRecycleMasks(int& errorPos)
    {
        return RecycleMasks.PrepareMasks(errorPos);
    }

    BOOL AgreeRecycleMasks(const char* fileName, const char* fileExt)
    {
        return RecycleMasks.AgreeMasks(fileName, fileExt);
    }
};

void SetProgressDialog(HWND hProgressDlg, CProgressData* data, CProgressDlgData& dlgData)
{                                                              // we will wait for the response, the face must be changed
    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
    if (!*dlgData.CancelWorker)                                // we need to stop the hl.thread
        SendMessage(hProgressDlg, WM_USER_SETDIALOG, (WPARAM)data, 0);
}

int CaclProg(const CQuadWord& progressCurrent, const CQuadWord& progressTotal)
{
    return progressCurrent >= progressTotal ? (progressTotal.Value == 0 ? 0 : 1000) : (int)((progressCurrent * CQuadWord(1000, 0)) / progressTotal).Value;
}

void SetProgress(HWND hProgressDlg, int operation, int summary, CProgressDlgData& dlgData)
{                                                              // we announce the change and continue without waiting for a response
    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
    if (!*dlgData.CancelWorker &&
        (*dlgData.OperationProgress != operation || *dlgData.SummaryProgress != summary))
    {
        *dlgData.OperationProgress = operation;
        *dlgData.SummaryProgress = summary;
        SendMessage(hProgressDlg, WM_USER_SETDIALOG, 0, 0);
    }
}

void SetProgressWithoutSuspend(HWND hProgressDlg, int operation, int summary, CProgressDlgData& dlgData)
{ // we announce the change and continue without waiting for a response
    if (!*dlgData.CancelWorker &&
        (*dlgData.OperationProgress != operation || *dlgData.SummaryProgress != summary))
    {
        *dlgData.OperationProgress = operation;
        *dlgData.SummaryProgress = summary;
        SendMessage(hProgressDlg, WM_USER_SETDIALOG, 0, 0);
    }
}

BOOL GetDirTime(const char* dirName, FILETIME* ftModified);
BOOL DoCopyDirTime(HWND hProgressDlg, const char* targetName, FILETIME* modified, CProgressDlgData& dlgData, BOOL quiet);

void GetFileOverwriteInfo(char* buff, int buffLen, HANDLE file, const char* fileName, FILETIME* fileTime, BOOL* getTimeFailed)
{
    FILETIME lastWrite;
    SYSTEMTIME st;
    FILETIME ft;
    char date[50], time[50];
    if (!GetFileTime(file, NULL, NULL, &lastWrite) ||
        !FileTimeToLocalFileTime(&lastWrite, &ft) ||
        !FileTimeToSystemTime(&ft, &st))
    {
        if (getTimeFailed != NULL)
            *getTimeFailed = TRUE;
        date[0] = 0;
        time[0] = 0;
    }
    else
    {
        if (fileTime != NULL)
            *fileTime = ft;
        if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
            sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
            sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    }

    char attr[30];
    lstrcpy(attr, ", ");
    DWORD attrs = SalGetFileAttributes(fileName);
    if (attrs != 0xFFFFFFFF)
        GetAttrsString(attr + 2, attrs);
    if (strlen(attr) == 2)
        attr[0] = 0;

    char number[50];
    CQuadWord size;
    DWORD err;
    if (SalGetFileSize(file, size, err))
        NumberToStr(number, size);
    else
        number[0] = 0; // error - size unknown

    _snprintf_s(buff, buffLen, _TRUNCATE, "%s, %s, %s%s", number, date, time, attr);
}

void GetDirInfo(char* buffer, const char* dir)
{
    const char* dirFindFirst = dir;
    char dirFindFirstCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(dirFindFirst, dirFindFirstCopy);

    BOOL ok = FALSE;
    FILETIME lastWrite;
    if (NameEndsWithBackslash(dirFindFirst))
    { // FindFirstFile fails for 'dir' ending with a backslash (used for invalid names)
        // directory), so we are solving it in this situation through CreateFile and GetFileTime
        HANDLE file = HANDLES_Q(CreateFile(dirFindFirst, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL));
        if (file != INVALID_HANDLE_VALUE)
        {
            if (GetFileTime(file, NULL, NULL, &lastWrite))
                ok = TRUE;
            HANDLES(CloseHandle(file));
        }
    }
    else
    {
        WIN32_FIND_DATA data;
        HANDLE find = HANDLES_Q(FindFirstFile(dirFindFirst, &data));
        if (find != INVALID_HANDLE_VALUE)
        {
            HANDLES(FindClose(find));
            lastWrite = data.ftLastWriteTime;
            ok = TRUE;
        }
    }
    if (ok)
    {
        SYSTEMTIME st;
        FILETIME ft;
        if (FileTimeToLocalFileTime(&lastWrite, &ft) &&
            FileTimeToSystemTime(&ft, &st))
        {
            char date[50], time[50];
            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
                sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
            if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
                sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);

            sprintf(buffer, "%s, %s", date, time);
        }
        else
            sprintf(buffer, "%s, %s", LoadStr(IDS_INVALID_DATEORTIME), LoadStr(IDS_INVALID_DATEORTIME));
    }
    else
        buffer[0] = 0;
}

BOOL IsDirectoryEmpty(const char* name) // directories/subdirectories do not contain any files
{
    char dir[MAX_PATH + 5];
    int len = (int)strlen(name);
    memcpy(dir, name, len);
    if (dir[len - 1] != '\\')
        dir[len++] = '\\';
    char* end = dir + len;
    strcpy(end, "*");

    WIN32_FIND_DATA fileData;
    HANDLE search;
    search = HANDLES_Q(FindFirstFile(dir, &fileData));
    if (search != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (fileData.cFileName[0] == 0 ||
                fileData.cFileName[0] == '.' && (fileData.cFileName[1] == 0 ||
                                                 fileData.cFileName[1] == '.' && fileData.cFileName[2] == 0))
                continue;

            if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                strcpy(end, fileData.cFileName);
                if (!IsDirectoryEmpty(dir)) // subdirectory is not empty
                {
                    HANDLES(FindClose(search));
                    return FALSE;
                }
            }
            else
            {
                HANDLES(FindClose(search)); // there is a file
                return FALSE;
            }
        } while (FindNextFile(search, &fileData));
        HANDLES(FindClose(search));
    }
    return TRUE;
}

BOOL CurrentProcessTokenUserValid = FALSE;
char CurrentProcessTokenUserBuf[200];
TOKEN_USER* CurrentProcessTokenUser = (TOKEN_USER*)CurrentProcessTokenUserBuf;

void GainWriteOwnerAccess()
{
    static BOOL firstCall = TRUE;
    if (firstCall)
    {
        firstCall = FALSE;

        HANDLE tokenHandle;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tokenHandle))
        {
            TRACE_E("GainWriteOwnerAccess(): OpenProcessToken failed!");
            return;
        }

        DWORD reqSize;
        if (GetTokenInformation(tokenHandle, TokenUser, CurrentProcessTokenUser, 200, &reqSize))
            CurrentProcessTokenUserValid = TRUE;

        int i;
        for (i = 0; i < 3; i++)
        {
            const char* privName = NULL;
            switch (i)
            {
            case 0:
                privName = SE_RESTORE_NAME;
                break;
            case 1:
                privName = SE_TAKE_OWNERSHIP_NAME;
                break;
            case 2:
                privName = SE_SECURITY_NAME;
                break;
            }

            LUID value;
            if (privName != NULL && LookupPrivilegeValue(NULL, privName, &value))
            {
                TOKEN_PRIVILEGES tokenPrivileges;
                tokenPrivileges.PrivilegeCount = 1;
                tokenPrivileges.Privileges[0].Luid = value;
                tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

                AdjustTokenPrivileges(tokenHandle, FALSE, &tokenPrivileges, sizeof(tokenPrivileges), NULL, NULL);
                if (GetLastError() != NO_ERROR)
                {
                    DWORD err = GetLastError();
                    TRACE_E("GainWriteOwnerAccess(): AdjustTokenPrivileges(" << privName << ") failed! error: " << GetErrorText(err));
                }
                else
                {
                    if (i == 0)
                        HaveWriteOwnerRight = TRUE; // Managed to obtain SE_RESTORE_NAME, WRITE_OWNER is guaranteed
                }
            }
            else
            {
                DWORD err = GetLastError();
                TRACE_E("GainWriteOwnerAccess(): LookupPrivilegeValue(" << (privName != NULL ? privName : "null") << ") failed! error: " << GetErrorText(err));
            }
        }
        CloseHandle(tokenHandle);
    }
}
/*  //  Purpose:    Determines if the user is a member of the administrators group.
//  Return:     TRUE if user is a admin
//              FALSE if not
#define STATUS_SUCCESS          ((NTSTATUS)0x00000000L) // ntsubauth
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
typedef NTSTATUS (WINAPI *FNtQueryInformationToken)(
    HANDLE TokenHandle,                             // IN
    TOKEN_INFORMATION_CLASS TokenInformationClass,  // IN
    PVOID TokenInformation,                         // OUT
    ULONG TokenInformationLength,                   // IN
    PULONG ReturnLength                             // OUT
    );


BOOL IsUserAdmin()
{
  if (NtDLL == NULL)
    return TRUE;

  GainWriteOwnerAccess();

  FNtQueryInformationToken DynNTNtQueryInformationToken = (FNtQueryInformationToken)GetProcAddress(NtDLL, "NtQueryInformationToken"); // nema header
  if (DynNTNtQueryInformationToken == NULL)
  {
    TRACE_E("Getting NtQueryInformationToken export failed!");
    return FALSE;
  }

  static int fIsUserAnAdmin = -1;  // cache

  if (-1 == fIsUserAnAdmin)
  {
    SID_IDENTIFIER_AUTHORITY authNT = SECURITY_NT_AUTHORITY;
    NTSTATUS                 Status;
    ULONG                    InfoLength;
    PTOKEN_GROUPS            TokenGroupList;
    ULONG                    GroupIndex;
    BOOL                     FoundAdmins;
    PSID                     AdminsDomainSid;
    HANDLE                   hUserToken;

    // Open the user's token
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hUserToken))
        return FALSE;

    // Create Admins domain sid.
    Status = AllocateAndInitializeSid(
               &authNT,
               2,
               SECURITY_BUILTIN_DOMAIN_RID,
               DOMAIN_ALIAS_RID_ADMINS,
               0, 0, 0, 0, 0, 0,
               &AdminsDomainSid
               );

    // Test if user is in the Admins domain

    // Get a list of groups in the token
    Status = DynNTNtQueryInformationToken(
                 hUserToken,               // Handle
                 TokenGroups,              // TokenInformationClass
                 NULL,                     // TokenInformation
                 0,                        // TokenInformationLength
                 &InfoLength               // ReturnLength
                 );

    if ((Status != STATUS_SUCCESS) && (Status != STATUS_BUFFER_TOO_SMALL))
    {
      FreeSid(AdminsDomainSid);
      CloseHandle(hUserToken);
      return FALSE;
    }

    TokenGroupList = (PTOKEN_GROUPS)GlobalAlloc(GPTR, InfoLength);

    if (TokenGroupList == NULL)
    {
      FreeSid(AdminsDomainSid);
      CloseHandle(hUserToken);
      return FALSE;
    }

    Status = DynNTNtQueryInformationToken(
                 hUserToken,        // Handle
                 TokenGroups,              // TokenInformationClass
                 TokenGroupList,           // TokenInformation
                 InfoLength,               // TokenInformationLength
                 &InfoLength               // ReturnLength
                 );

    if (!NT_SUCCESS(Status))
    {
      GlobalFree(TokenGroupList);
      FreeSid(AdminsDomainSid);
      CloseHandle(hUserToken);
      return FALSE;
    }


    // Search group list for Admins alias
    FoundAdmins = FALSE;

    for (GroupIndex=0; GroupIndex < TokenGroupList->GroupCount; GroupIndex++ )
    {
      if (EqualSid(TokenGroupList->Groups[GroupIndex].Sid, AdminsDomainSid))
      {
        FoundAdmins = TRUE;
        break;
      }
    }

    // Tidy up
    GlobalFree(TokenGroupList);
    FreeSid(AdminsDomainSid);
    CloseHandle(hUserToken);

    fIsUserAnAdmin = FoundAdmins ? 1 : 0;
  }

  return (BOOL)fIsUserAnAdmin;
}

*/

/* according to http://vcfaq.mvps.org/sdk/21.htm*/
#define BUFF_SIZE 1024
BOOL IsUserAdmin()
{
    HANDLE hToken = NULL;
    PSID pAdminSid = NULL;
    BYTE buffer[BUFF_SIZE];
    PTOKEN_GROUPS pGroups = (PTOKEN_GROUPS)buffer;
    DWORD dwSize; // buffer size
    DWORD i;
    BOOL bSuccess;
    SID_IDENTIFIER_AUTHORITY siaNtAuth = SECURITY_NT_AUTHORITY;

    // get token handle
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return FALSE;

    bSuccess = GetTokenInformation(hToken, TokenGroups, (LPVOID)pGroups, BUFF_SIZE, &dwSize);
    CloseHandle(hToken);
    if (!bSuccess)
        return FALSE;

    if (!AllocateAndInitializeSid(&siaNtAuth, 2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &pAdminSid))
        return FALSE;

    bSuccess = FALSE;
    for (i = 0; (i < pGroups->GroupCount) && !bSuccess; i++)
    {
        if (EqualSid(pAdminSid, pGroups->Groups[i].Sid))
            bSuccess = TRUE;
    }
    FreeSid(pAdminSid);

    return bSuccess;
}

struct CSrcSecurity // Helper method for holding security info for MoveFile (source disappears after operation, its security info needs to be saved in advance)
{
    PSID SrcOwner;
    PSID SrcGroup;
    PACL SrcDACL;
    PSECURITY_DESCRIPTOR SrcSD;
    DWORD SrcError;

    CSrcSecurity() { Clear(); }
    ~CSrcSecurity()
    {
        if (SrcSD != NULL)
            LocalFree(SrcSD);
    }
    void Clear()
    {
        SrcOwner = NULL;
        SrcGroup = NULL;
        SrcDACL = NULL;
        SrcSD = NULL;
        SrcError = NO_ERROR;
    }
};

BOOL DoCopySecurity(const char* sourceName, const char* targetName, DWORD* err, CSrcSecurity* srcSecurity)
{
    // if the path ends with a space/dot, we need to append '\\', otherwise
    // GetNamedSecurityInfo (and others) trims spaces/dots and works
    // so with another way
    const char* sourceNameSec = sourceName;
    char sourceNameSecCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(sourceNameSec, sourceNameSecCopy);
    const char* targetNameSec = targetName;
    char targetNameSecCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(targetNameSec, targetNameSecCopy);

    PSID srcOwner = NULL;
    PSID srcGroup = NULL;
    PACL srcDACL = NULL;
    PSECURITY_DESCRIPTOR srcSD = NULL;
    if (srcSecurity != NULL) // MoveFile: just take over security-info
    {
        srcOwner = srcSecurity->SrcOwner;
        srcGroup = srcSecurity->SrcGroup;
        srcDACL = srcSecurity->SrcDACL;
        srcSD = srcSecurity->SrcSD;
        *err = srcSecurity->SrcError;
        srcSecurity->Clear();
    }
    else // obtain security info from source
    {
        *err = GetNamedSecurityInfo((char*)sourceNameSec, SE_FILE_OBJECT,
                                    DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                    &srcOwner, &srcGroup, &srcDACL, NULL, &srcSD);
    }
    BOOL ret = *err == ERROR_SUCCESS;

    if (ret)
    {
        SECURITY_DESCRIPTOR_CONTROL srcSDControl;
        DWORD srcSDRevision;
        if (!GetSecurityDescriptorControl(srcSD, &srcSDControl, &srcSDRevision))
        {
            *err = GetLastError();
            ret = FALSE;
        }
        else
        {
            BOOL inheritedDACL = /*(srcSDControl & SE_DACL_AUTO_INHERITED) != 0 &&*/ (srcSDControl & SE_DACL_PROTECTED) == 0; // SE_DACL_AUTO_INHERITED is unfortunately not always set (for example, Total Commander resets it after moving a file, so we ignore it)
            DWORD attr = GetFileAttributes(targetNameSec);
            *err = SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT,
                                        DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION |
                                            (inheritedDACL ? UNPROTECTED_DACL_SECURITY_INFORMATION : PROTECTED_DACL_SECURITY_INFORMATION),
                                        srcOwner, srcGroup, srcDACL, NULL);
            ret = *err == ERROR_SUCCESS;

            if (!ret)
            {
                // if it is not possible to change the owner and group (we do not have permissions in the directory - for example, we only have "change" permissions),
                // we will check if owner + group are already set (as this would not be an error)
                PSID tgtOwner = NULL;
                PSID tgtGroup = NULL;
                PACL tgtDACL = NULL;
                PSECURITY_DESCRIPTOR tgtSD = NULL;
                BOOL tgtRead = GetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT,
                                                    DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                                    &tgtOwner, &tgtGroup, &tgtDACL, NULL, &tgtSD) == ERROR_SUCCESS;
                // if the owner of the target file is not the current user, we will try to set it ("take ownership") - only
                // if we have the right to write the owner, so that we can write back the original owner
                BOOL ownerOfFile = FALSE;
                if (!tgtRead ||         // if it is not possible to load security-info from the target, probably the owner is not the current user (as the owner has non-blocking read permissions)
                    tgtOwner == NULL || // probably nonsense, a file must have an owner, if it happens, we will try to set the owner to the current user
                    CurrentProcessTokenUserValid && CurrentProcessTokenUser->User.Sid != NULL &&
                        !EqualSid(tgtOwner, CurrentProcessTokenUser->User.Sid))
                {
                    if (HaveWriteOwnerRight &&
                        CurrentProcessTokenUserValid && CurrentProcessTokenUser->User.Sid != NULL &&
                        SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                                             CurrentProcessTokenUser->User.Sid, NULL, NULL, NULL) == ERROR_SUCCESS)
                    { // settings were successful, we need to retrieve 'tgtSD' again
                        ownerOfFile = TRUE;
                        if (tgtSD != NULL)
                            LocalFree(tgtSD);
                        tgtOwner = NULL;
                        tgtGroup = NULL;
                        tgtDACL = NULL;
                        tgtSD = NULL;
                        tgtRead = GetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT,
                                                       DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                                       &tgtOwner, &tgtGroup, &tgtDACL, NULL, &tgtSD) == ERROR_SUCCESS;
                    }
                }
                else
                {
                    ownerOfFile = tgtRead && tgtOwner != NULL && CurrentProcessTokenUserValid &&
                                  CurrentProcessTokenUser->User.Sid != NULL;
                }
                BOOL daclOK = FALSE;
                BOOL ownerOK = FALSE;
                BOOL groupOK = FALSE;
                if (ownerOfFile && CurrentProcessTokenUserValid && CurrentProcessTokenUser->User.Sid != NULL)
                { // we are the owners of the file -> writing to the DACL is possible, let's try to allow writing for owner+group+DACL and try to write the necessary values
                    int allowChPermDACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(ACCESS_ALLOWED_ACE().SidStart) +
                                              GetLengthSid(CurrentProcessTokenUser->User.Sid) + 200 /* +200 apartments is just paranoia*/;
                    char buff3[500];
                    PACL allowChPermDACL;
                    if (allowChPermDACLSize > 500)
                        allowChPermDACL = (PACL)malloc(allowChPermDACLSize);
                    else
                        allowChPermDACL = (PACL)buff3;
                    if (allowChPermDACL != NULL && InitializeAcl(allowChPermDACL, allowChPermDACLSize, ACL_REVISION) &&
                        AddAccessAllowedAce(allowChPermDACL, ACL_REVISION, READ_CONTROL | WRITE_DAC | WRITE_OWNER,
                                            CurrentProcessTokenUser->User.Sid) &&
                        SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT,
                                             DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                             NULL, NULL, allowChPermDACL, NULL) == ERROR_SUCCESS)
                    {
                        ownerOK = SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT,
                                                       OWNER_SECURITY_INFORMATION,
                                                       srcOwner, NULL, NULL, NULL) == ERROR_SUCCESS;
                        groupOK = SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT,
                                                       GROUP_SECURITY_INFORMATION,
                                                       NULL, srcGroup, NULL, NULL) == ERROR_SUCCESS;
                        daclOK = SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | (inheritedDACL ? UNPROTECTED_DACL_SECURITY_INFORMATION : PROTECTED_DACL_SECURITY_INFORMATION),
                                                      NULL, NULL, srcDACL, NULL) == ERROR_SUCCESS;
                    }
                    if (allowChPermDACL != (PACL)buff3 && allowChPermDACL != NULL)
                        free(allowChPermDACL);
                }
                if (!ownerOK &&
                    (SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                                          srcOwner, NULL, NULL, NULL) == ERROR_SUCCESS ||
                     tgtRead && (srcOwner == NULL && tgtOwner == NULL || // If the owner is already set, we ignore any potential setting error
                                 srcOwner != NULL && tgtOwner != NULL && EqualSid(srcOwner, tgtOwner))))
                {
                    ownerOK = TRUE;
                }
                if (!groupOK &&
                    (SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT, GROUP_SECURITY_INFORMATION,
                                          NULL, srcGroup, NULL, NULL) == ERROR_SUCCESS ||
                     tgtRead && (srcGroup == NULL && tgtGroup == NULL || // If the group is already set, we ignore any potential setting error
                                 srcGroup != NULL && tgtGroup != NULL && EqualSid(srcGroup, tgtGroup))))
                {
                    groupOK = TRUE;
                }
                if (!daclOK && // We need to set the DACL last because it depends on the owner (CREATOR OWNER is replaced by the real owner, etc.)
                    SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | (inheritedDACL ? UNPROTECTED_DACL_SECURITY_INFORMATION : PROTECTED_DACL_SECURITY_INFORMATION),
                                         NULL, NULL, srcDACL, NULL) == ERROR_SUCCESS)
                {
                    daclOK = TRUE;
                }
                if (ownerOK && groupOK && daclOK)
                {
                    ret = TRUE; // all three components are OK -> the whole is OK
                    *err = NO_ERROR;
                }
                if (tgtSD != NULL)
                    LocalFree(tgtSD);
            }
            if (attr != INVALID_FILE_ATTRIBUTES)
                SetFileAttributes(targetNameSec, attr);
        }
    }
    if (srcSD != NULL)
        LocalFree(srcSD);
    return ret;
}

DWORD CompressFile(char* fileName, DWORD attrs)
{
    DWORD ret = ERROR_SUCCESS;
    if (attrs & FILE_ATTRIBUTE_COMPRESSED)
        return ret; // It is already compressed

    // if the path ends with a space/dot, we need to append '\\', otherwise CreateFile
    // Trims spaces/dots and operates with a different path
    const char* fileNameCrFile = fileName;
    char fileNameCrFileCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(fileNameCrFile, fileNameCrFileCopy);

    BOOL attrsChange = FALSE;
    if (attrs & FILE_ATTRIBUTE_READONLY)
    {
        attrsChange = TRUE;
        SetFileAttributes(fileNameCrFile, attrs & ~FILE_ATTRIBUTE_READONLY);
    }
    HANDLE file = HANDLES_Q(CreateFile(fileNameCrFile, FILE_READ_DATA | FILE_WRITE_DATA,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                       OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS,
                                       NULL));
    if (file == INVALID_HANDLE_VALUE)
        ret = GetLastError();
    else
    {
        USHORT state = COMPRESSION_FORMAT_DEFAULT;
        ULONG length;
        if (!DeviceIoControl(file, FSCTL_SET_COMPRESSION, &state,
                             sizeof(USHORT), NULL, 0, &length, FALSE))
            ret = GetLastError();
        HANDLES(CloseHandle(file));
    }
    if (attrsChange)
        SetFileAttributes(fileNameCrFile, attrs); // return to the original attributes (if an error occurs, the attributes will not be reset and will remain nonsensically changed)
    return ret;
}

DWORD UncompressFile(char* fileName, DWORD attrs)
{
    DWORD ret = ERROR_SUCCESS;
    if ((attrs & FILE_ATTRIBUTE_COMPRESSED) == 0)
        return ret; // not compressed

    // if the path ends with a space/dot, we need to append '\\', otherwise CreateFile
    // Trims spaces/dots and operates with a different path
    const char* fileNameCrFile = fileName;
    char fileNameCrFileCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(fileNameCrFile, fileNameCrFileCopy);

    BOOL attrsChange = FALSE;
    if (attrs & FILE_ATTRIBUTE_READONLY)
    {
        attrsChange = TRUE;
        SetFileAttributes(fileNameCrFile, attrs & ~FILE_ATTRIBUTE_READONLY);
    }

    HANDLE file = HANDLES_Q(CreateFile(fileNameCrFile, FILE_READ_DATA | FILE_WRITE_DATA,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS,
                                       NULL));
    if (file == INVALID_HANDLE_VALUE)
        ret = GetLastError();
    else
    {
        USHORT state = COMPRESSION_FORMAT_NONE;
        ULONG length;
        if (!DeviceIoControl(file, FSCTL_SET_COMPRESSION, &state,
                             sizeof(USHORT), NULL, 0, &length, FALSE))
            ret = GetLastError();
        HANDLES(CloseHandle(file));
    }
    if (attrsChange)
        SetFileAttributes(fileNameCrFile, attrs); // return to the original attributes (if an error occurs, the attributes will not be reset and will remain nonsensically changed)
    return ret;
}

DWORD MyEncryptFile(HWND hProgressDlg, char* fileName, DWORD attrs, DWORD finalAttrs,
                    CProgressDlgData& dlgData, BOOL& cancelOper, BOOL preserveDate)
{
    DWORD retEnc = ERROR_SUCCESS;
    cancelOper = FALSE;
    if (attrs & FILE_ATTRIBUTE_ENCRYPTED)
        return retEnc; // it is now encrypted

    // if the path ends with a space/dot, we need to append '\\', otherwise CreateFile
    // Trims spaces/dots and operates with a different path
    const char* fileNameCrFile = fileName;
    char fileNameCrFileCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(fileNameCrFile, fileNameCrFileCopy);

    // if the SYSTEM file has an attribute, the EncryptFile API function reports an "access denied" error, we are solving:
    if ((attrs & FILE_ATTRIBUTE_SYSTEM) && (finalAttrs & FILE_ATTRIBUTE_SYSTEM))
    { // if it has an attribute SYSTEM, we need to ask the user if they are serious
        if (!dlgData.EncryptSystemAll)
        {
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return retEnc;

            if (dlgData.SkipAllEncryptSystem)
                return retEnc;

            int ret = IDCANCEL;
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = LoadStr(IDS_CONFIRMSFILEENCRYPT);
            data[2] = fileName;
            data[3] = LoadStr(IDS_ENCRYPTSFILE);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 2, (LPARAM)data);
            switch (ret)
            {
            case IDB_ALL:
                dlgData.EncryptSystemAll = TRUE;
            case IDYES:
                break;

            case IDB_SKIPALL:
                dlgData.SkipAllEncryptSystem = TRUE;
            case IDB_SKIP:
                return retEnc;

            case IDCANCEL:
            {
                cancelOper = TRUE;
                return retEnc;
            }
            }
        }
    }

    BOOL attrsChange = FALSE;
    if (attrs & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY))
    {
        attrsChange = TRUE;
        SetFileAttributes(fileNameCrFile, attrs & ~(FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY));
    }
    if (preserveDate)
    {
        HANDLE file;
        file = HANDLES_Q(CreateFile(fileNameCrFile, GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING,
                                    (attrs & FILE_ATTRIBUTE_DIRECTORY) ? FILE_FLAG_BACKUP_SEMANTICS : 0,
                                    NULL));
        if (file != INVALID_HANDLE_VALUE)
        {
            FILETIME ftCreated, /*ftAccessed,*/ ftModified;
            GetFileTime(file, &ftCreated, NULL /*&ftAccessed*/, &ftModified);
            HANDLES(CloseHandle(file));

            if (!EncryptFile(fileNameCrFile))
                retEnc = GetLastError();

            file = HANDLES_Q(CreateFile(fileNameCrFile, GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL, OPEN_EXISTING,
                                        (attrs & FILE_ATTRIBUTE_DIRECTORY) ? FILE_FLAG_BACKUP_SEMANTICS : 0,
                                        NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                SetFileTime(file, &ftCreated, NULL /*&ftAccessed*/, &ftModified);
                HANDLES(CloseHandle(file));
            }
        }
        else
            retEnc = GetLastError();
    }
    else
    {
        if (!EncryptFile(fileNameCrFile))
            retEnc = GetLastError();
    }
    if (attrsChange)
        SetFileAttributes(fileNameCrFile, attrs); // return to the original attributes (if an error occurs, the attributes will not be reset and will remain nonsensically changed)
    return retEnc;
}

DWORD MyDecryptFile(char* fileName, DWORD attrs, BOOL preserveDate)
{
    DWORD ret = ERROR_SUCCESS;
    if ((attrs & FILE_ATTRIBUTE_ENCRYPTED) == 0)
        return ret; // not encrypted

    // if the path ends with a space/dot, we need to append '\\', otherwise CreateFile
    // Trims spaces/dots and operates with a different path
    const char* fileNameCrFile = fileName;
    char fileNameCrFileCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(fileNameCrFile, fileNameCrFileCopy);

    BOOL attrsChange = FALSE;
    if (attrs & FILE_ATTRIBUTE_READONLY)
    {
        attrsChange = TRUE;
        SetFileAttributes(fileNameCrFile, attrs & ~FILE_ATTRIBUTE_READONLY);
    }
    if (preserveDate)
    {
        HANDLE file;
        file = HANDLES_Q(CreateFile(fileNameCrFile, GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING,
                                    (attrs & FILE_ATTRIBUTE_DIRECTORY) ? FILE_FLAG_BACKUP_SEMANTICS : 0,
                                    NULL));
        if (file != INVALID_HANDLE_VALUE)
        {
            FILETIME ftCreated, /*ftAccessed,*/ ftModified;
            GetFileTime(file, &ftCreated, NULL /*&ftAccessed*/, &ftModified);
            HANDLES(CloseHandle(file));

            if (!DecryptFile(fileNameCrFile, 0))
                ret = GetLastError();

            file = HANDLES_Q(CreateFile(fileNameCrFile, GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL, OPEN_EXISTING,
                                        (attrs & FILE_ATTRIBUTE_DIRECTORY) ? FILE_FLAG_BACKUP_SEMANTICS : 0,
                                        NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                SetFileTime(file, &ftCreated, NULL /*&ftAccessed*/, &ftModified);
                HANDLES(CloseHandle(file));
            }
        }
        else
            ret = GetLastError();
    }
    else
    {
        if (!DecryptFile(fileNameCrFile, 0))
            ret = GetLastError();
    }
    if (attrsChange)
        SetFileAttributes(fileNameCrFile, attrs); // return to the original attributes (if an error occurs, the attributes will not be reset and will remain nonsensically changed)
    return ret;
}

BOOL CheckFileOrDirADS(const char* fileName, BOOL isDir, CQuadWord* adsSize, wchar_t*** streamNames,
                       int* streamNamesCount, BOOL* lowMemory, DWORD* winError,
                       DWORD bytesPerCluster, CQuadWord* adsOccupiedSpace,
                       BOOL* onlyDiscardableStreams)
{
    if (adsSize != NULL)
        adsSize->SetUI64(0);
    if (adsOccupiedSpace != NULL)
        adsOccupiedSpace->SetUI64(0);
    if (streamNames != NULL)
        *streamNames = NULL;
    if (streamNamesCount != NULL)
        *streamNamesCount = 0;
    if (lowMemory != NULL)
        *lowMemory = FALSE;
    if (winError != NULL)
        *winError = NO_ERROR;
    if (onlyDiscardableStreams != NULL)
        *onlyDiscardableStreams = TRUE;

    if (DynNtQueryInformationFile != NULL) // "always true"
    {
        // if the path ends with a space/dot, we need to append '\\', otherwise CreateFile
        // Trims spaces/dots and operates with a different path
        const char* fileNameCrFile = fileName;
        char fileNameCrFileCopy[3 * MAX_PATH];
        MakeCopyWithBackslashIfNeeded(fileNameCrFile, fileNameCrFileCopy);

        HANDLE file = HANDLES_Q(CreateFile(fileNameCrFile, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           NULL, OPEN_EXISTING,
                                           isDir ? FILE_FLAG_BACKUP_SEMANTICS : 0, NULL));
        if (file == INVALID_HANDLE_VALUE)
        {
            if (winError != NULL)
                *winError = GetLastError();
            return FALSE;
        }

        // get stream info
        NTSTATUS uStatus;
        IO_STATUS_BLOCK ioStatus;
        BYTE buffer[65535]; // XP systems cannot handle more than 65535 (I don't know why)
        uStatus = DynNtQueryInformationFile(file, &ioStatus, buffer, sizeof(buffer), FileStreamInformation);
        HANDLES(CloseHandle(file));
        if (uStatus != 0 /* anything but success is a mistake (including warnings)*/)
        {
            if (winError != NULL)
            {
                if (uStatus == STATUS_BUFFER_OVERFLOW)
                    *winError = ERROR_INSUFFICIENT_BUFFER;
                else
                    *winError = LsaNtStatusToWinError(uStatus);
            }
            return FALSE;
        }

        TDirectArray<wchar_t*>* streamNamesAux = NULL;
        if (streamNames != NULL)
        {
            streamNamesAux = new TDirectArray<wchar_t*>(10, 100);
            if (streamNamesAux == NULL)
            {
                if (lowMemory != NULL)
                    *lowMemory = TRUE;
                TRACE_E(LOW_MEMORY);
                return FALSE;
            }
        }

        // iterate through the streams
        PFILE_STREAM_INFORMATION psi = (PFILE_STREAM_INFORMATION)buffer;
        BOOL ret = FALSE;
        BOOL lowMem = FALSE;
        if (ioStatus.Information > 0) // check if we have received any data at all
        {
            while (1)
            {
                if (psi->NameLength != 7 * 2 || _memicmp(psi->Name, L"::$DATA", 7 * 2)) // ignore default stream
                {
                    ret = TRUE;
                    if (adsSize != NULL)
                        *adsSize += CQuadWord(psi->Size.LowPart, psi->Size.HighPart); // sum of the total size of all alternate data streams
                    if (adsOccupiedSpace != NULL && bytesPerCluster != 0)
                    {
                        CQuadWord fileSize(psi->Size.LowPart, psi->Size.HighPart);
                        *adsOccupiedSpace += fileSize - ((fileSize - CQuadWord(1, 0)) % CQuadWord(bytesPerCluster, 0)) +
                                             CQuadWord(bytesPerCluster - 1, 0);
                    }

                    if (onlyDiscardableStreams != NULL)
                    {                                                                                                                      // if an ADS appears that is unknown or non-essential, we will switch 'onlyDiscardableStreams' to FALSE
                        if ((psi->NameLength < 29 * 2 || _memicmp(psi->Name, L":\x05Q30lsldxJoudresxAaaqpcawXc:", 29 * 2) != 0) &&         // Win2K thumbnail in ADS: 5952 bytes (depends on JPEG compression)
                            (psi->NameLength < 40 * 2 || _memicmp(psi->Name, L":{4c8cc155-6c1e-11d1-8e41-00c04fb9386d}:", 40 * 2) != 0) && // Win2K thumbnail in ADS: 0 bytes
                            (psi->NameLength < 9 * 2 || _memicmp(psi->Name, L":KAVICHS:", 9 * 2) != 0))                                    // Kaspersky antivirus: 36/68 bytes
                        {
                            *onlyDiscardableStreams = FALSE;
                        }
                    }

                    if (streamNamesAux != NULL) // Collecting Unicode names of alternate data streams
                    {
                        wchar_t* str = (wchar_t*)malloc(psi->NameLength + 2);
                        if (str != NULL)
                        {
                            memcpy(str, psi->Name, psi->NameLength);
                            str[psi->NameLength / 2] = 0;
                            streamNamesAux->Add(str);
                            if (!streamNamesAux->IsGood())
                            {
                                free(str);
                                streamNamesAux->ResetState();
                                if (lowMemory != NULL)
                                    *lowMemory = TRUE;
                                lowMem = TRUE;
                                break;
                            }
                        }
                        else
                        {
                            if (lowMemory != NULL)
                                *lowMemory = TRUE;
                            lowMem = TRUE;
                            TRACE_E(LOW_MEMORY);
                            break;
                        }
                    }
                    else
                    {
                        if (adsSize == NULL && adsOccupiedSpace == NULL && onlyDiscardableStreams == NULL)
                            break; // There is nothing more to discover (does not collect names, stream sizes, or only-discardable-streams)
                    }
                }
                if (psi->NextEntry == 0)
                    break;
                psi = (PFILE_STREAM_INFORMATION)((BYTE*)psi + psi->NextEntry); // move to next item
            }
        }
        if (streamNamesAux != NULL)
        {
            if (lowMem || !ret) // lack of memory or no ADS, release all names
            {
                int i;
                for (i = 0; i < streamNamesAux->Count; i++)
                    free(streamNamesAux->At(i));
            }
            else // all OK, we will pass the names to the caller
            {
                if (streamNamesCount != NULL)
                    *streamNamesCount = streamNamesAux->Count;
                *streamNames = streamNamesAux->GetData();
                streamNamesAux->DetachArray();
            }
            delete streamNamesAux;
        }
        return ret;
    }
    return FALSE;
}

BOOL DeleteAllADS(HANDLE file, const char* fileName)
{
    if (DynNtQueryInformationFile != NULL) // "always true"
    {
        // get stream info
        NTSTATUS uStatus;
        IO_STATUS_BLOCK ioStatus;
        BYTE buffer[65535]; // XP systems cannot handle more than 65535 (I don't know why)
        uStatus = DynNtQueryInformationFile(file, &ioStatus, buffer, sizeof(buffer), FileStreamInformation);
        if (uStatus != 0 /* anything but success is a mistake (including warnings)*/)
        {
            DWORD err;
            if (uStatus == STATUS_BUFFER_OVERFLOW)
                err = ERROR_INSUFFICIENT_BUFFER;
            else
                err = LsaNtStatusToWinError(uStatus);
            TRACE_I("DeleteAllADS(" << fileName << "): NtQueryInformationFile failed: " << GetErrorText(err));
            return FALSE;
        }

        // iterate through the streams
        PFILE_STREAM_INFORMATION psi = (PFILE_STREAM_INFORMATION)buffer;
        if (ioStatus.Information > 0) // check if we have received any data at all
        {
            WCHAR adsFullName[2 * MAX_PATH];
            adsFullName[0] = 0;
            WCHAR* adsPart = NULL;
            int adsPartSize = 0;
            while (1)
            {
                if (psi->NameLength != 7 * 2 || _memicmp(psi->Name, L"::$DATA", 7 * 2)) // ignore default stream
                {
                    if (adsFullName[0] == 0) // File name conversion is done only when needed, saving machine time
                    {
                        if (ConvertA2U(fileName, -1, adsFullName, 2 * MAX_PATH) == 0)
                            return FALSE; // "always false"
                        adsPart = adsFullName + wcslen(adsFullName);
                        adsPartSize = (int)((adsFullName + 2 * MAX_PATH) - adsPart);
                        if (adsPartSize > 0)
                        {
                            *adsPart++ = L':';
                            adsPartSize--;
                        }
                        else
                            return FALSE; // "always false"
                    }
                    WCHAR* start = (WCHAR*)psi->Name;
                    WCHAR* nameEnd = (WCHAR*)((char*)psi->Name + psi->NameLength);
                    if (start < nameEnd && *start == L':')
                        start++;
                    WCHAR* end = start;
                    while (end < nameEnd && *end != L':')
                        end++;
                    if (end - start >= adsPartSize)
                    {
                        TRACE_I("DeleteAllADS(" << fileName << "): too long ADS name!");
                        return FALSE;
                    }
                    if (end > start)
                    {
                        memcpy(adsPart, start, (end - start) * sizeof(WCHAR));
                        adsPart[end - start] = 0;
                        if (!DeleteFileW(adsFullName))
                        {
                            DWORD err = GetLastError();
                            TRACE_IW(L"DeleteAllADS(" << adsFullName << L"): DeleteFile has failed: " << GetErrorTextW(err));
                            return FALSE;
                        }
                    }
                }
                if (psi->NextEntry == 0)
                    break;
                psi = (PFILE_STREAM_INFORMATION)((BYTE*)psi + psi->NextEntry); // move to next item
            }
        }
    }
    return TRUE;
}

void MyStrCpyNW(wchar_t* s1, wchar_t* s2, int maxChars)
{
    if (maxChars == 0)
        return;
    while (--maxChars && *s2 != 0)
        *s1++ = *s2++;
    *s1 = 0;
}

void CutADSNameSuffix(char* s)
{
    char* end = strrchr(s, ':');
    if (end != NULL && stricmp(end, ":$DATA") == 0)
        *end = 0;
}

// Conversion to extended-length path, see MSDN article "File Name Conventions"
void DoLongName(char* buf, const char* name, int bufSize)
{
    if (*name == '\\')
        _snprintf_s(buf, bufSize, _TRUNCATE, "\\\\?\\UNC%s", name + 1); // UNC
    else
        _snprintf_s(buf, bufSize, _TRUNCATE, "\\\\?\\%s", name); // classic way
}

BOOL SalSetFilePointer(HANDLE file, const CQuadWord& offset)
{
    LONG lo = offset.LoDWord;
    LONG hi = offset.HiDWord;
    lo = SetFilePointer(file, lo, &hi, FILE_BEGIN);
    return (lo != INVALID_SET_FILE_POINTER || GetLastError() == NO_ERROR) &&
           lo == (LONG)offset.LoDWord && hi == (LONG)offset.HiDWord;
}

#define RETRYCOPY_TAIL_MINSIZE (32 * 1024) // At least two blocks of this size are checked at the end of the file, which is tested in CheckTailOfOutFile(); then the block size is increased up to ASYNC_COPY_BUF_SIZE (if reading is fast enough); WARNING: it must be <= than ASYNC_COPY_BUF_SIZE
#define RETRYCOPY_TESTINGTIME 3000         // time in [ms] for testing in CheckTailOfOutFile()

void CheckTailOfOutFileShowErr(const char* txt)
{
    DWORD err = GetLastError();
    TRACE_I("CheckTailOfOutFile(): " << txt << " Error: " << GetErrorText(err));
}

BOOL CheckTailOfOutFile(CAsyncCopyParams* asyncPar, HANDLE in, HANDLE out, const CQuadWord& offset,
                        const CQuadWord& curInOffset, BOOL ignoreReadErrOnOut)
{
    char* bufIn = (char*)malloc(ASYNC_COPY_BUF_SIZE);
    char* bufOut = (char*)malloc(ASYNC_COPY_BUF_SIZE);

    DWORD startTime = GetTickCount();
    DWORD rutineStartTime = startTime;
    CQuadWord lastOffset = offset;
    int roundNum = 1;
    DWORD curBufSize = RETRYCOPY_TAIL_MINSIZE;
    DWORD lastRoundStartTime = 0;
    DWORD lastRoundBufSize = 0;
    BOOL searchLongLastingBlock = TRUE;
    BOOL ok;
    while (1)
    {
        DWORD roundStartTime = GetTickCount();
        ok = FALSE;
        CQuadWord start;
        start.Value = lastOffset.Value > curBufSize ? lastOffset.Value - curBufSize : 0;
        DWORD size = (DWORD)(lastOffset.Value - start.Value);
        if (size == 0)
        {
            ok = TRUE;
            break; // nothing to check
        }
#ifdef WORKER_COPY_DEBUG_MSG
        TRACE_I("CheckTailOfOutFile(): check: " << start.Value << " - " << lastOffset.Value << ", size: " << size);
#endif // WORKER_COPY_DEBUG_MSG
        if (asyncPar == NULL)
        {
            if (SalSetFilePointer(in, start))
            { // set the 'start' offset in
                if (SalSetFilePointer(out, start))
                { // set the 'start' offset in out
                    DWORD read;
                    if (ReadFile(out, bufOut, size, &read, NULL) && read == size)
                    { // read 'size' bytes into the out buffer (fails if opened without "read" access)
                        if (ReadFile(in, bufIn, size, &read, NULL) && read == size)
                        {                                         // Read 'size' bytes into the buffer
                            if (memcmp(bufIn, bufOut, size) == 0) // compare if the in/out buffers are identical
                                ok = TRUE;
                            else
                                TRACE_I("CheckTailOfOutFile(): tail of target file is different from source file, tail without differences: " << (offset.Value - lastOffset.Value));
                        }
                        else
                            CheckTailOfOutFileShowErr("Unable to read IN file.");
                    }
                    else
                    {
                        if (ignoreReadErrOnOut) // if an error occurred on the input file, we ignore that we cannot read from the output (the input was reopened and the output has been open all the time)
                        {
                            CheckTailOfOutFileShowErr("Unable to read OUT file, but it was not broken, so it's no problem.");
                            ok = TRUE;
                            break;
                        }
                        else
                            CheckTailOfOutFileShowErr("Unable to read OUT file.");
                    }
                }
                else
                    CheckTailOfOutFileShowErr("Unable to set file pointer to start offset in OUT file.");
            }
            else
                CheckTailOfOutFileShowErr("Unable to set file pointer to start offset in IN file.");
        }
        else
        {
            // Asynchronously read from the 'start' in and out block of 'size' bytes and compare
            DWORD readOut;
            if ((ReadFile(out, bufOut, size, NULL,
                          asyncPar->InitOverlappedWithOffset(0, start)) ||
                 GetLastError() == ERROR_IO_PENDING) &&
                GetOverlappedResult(out, asyncPar->GetOverlapped(0), &readOut, TRUE))
            {
                DWORD readIn;
                if ((ReadFile(in, bufIn, size, NULL,
                              asyncPar->InitOverlappedWithOffset(1, start)) ||
                     GetLastError() == ERROR_IO_PENDING) &&
                    GetOverlappedResult(in, asyncPar->GetOverlapped(1), &readIn, TRUE))
                {
                    if (readOut != size || readIn != size ||
                        memcmp(bufIn, bufOut, size) != 0) // compare if the in/out buffers are identical
                    {
                        TRACE_I("CheckTailOfOutFile(): tail of target file is different from source file (async), tail without differences: " << (offset.Value - lastOffset.Value));
                    }
                    else
                        ok = TRUE;
                }
                else
                    CheckTailOfOutFileShowErr("Unable to read IN file (async).");
            }
            else
            {
                if (ignoreReadErrOnOut) // if an error occurred on the input file, we ignore that we cannot read from the output (the input was reopened and the output has been open all the time)
                {
                    CheckTailOfOutFileShowErr("Unable to read OUT file (async), but it was not broken, so it's no problem.");
                    ok = TRUE;
                    break;
                }
                else
                    CheckTailOfOutFileShowErr("Unable to read OUT file (async).");
            }
        }
        if (!ok)
            break;
        lastOffset = start;
        DWORD curBufSizeBackup = curBufSize;
        if (roundNum > 1)
        {
            DWORD ti = GetTickCount();
            if (searchLongLastingBlock)
            {
                DWORD t1 = roundStartTime - lastRoundStartTime;
                DWORD t2 = ti - roundStartTime;
                if (roundNum == 2 && t1 > 300 && 10 * t2 < t1) // the first round involves waiting for the "readiness" of the disk/site, we will adjust the start time (we want to read after the selected time and not just wait)
                {
#ifdef WORKER_COPY_DEBUG_MSG
                    TRACE_I("CheckTailOfOutFile(): detected long lasting first block, start time shifted by " << ((roundStartTime - startTime) / 1000.0) << " secs.");
#endif // WORKER_COPY_DEBUG_MSG
                    startTime = roundStartTime;
                }
                else
                {
                    if (t2 > 1000 && ((curBufSize * 10) / lastRoundBufSize) * t1 < t2)
                    { // Unexpectedly long reading of a block, probably involving waiting for "disk verification" or something, for one block we will ignore this time so that normal control can still take place
                        searchLongLastingBlock = FALSE;
                        DWORD sh = t2 - ((unsigned __int64)curBufSize * t1) / lastRoundBufSize;
#ifdef WORKER_COPY_DEBUG_MSG
                        TRACE_I("CheckTailOfOutFile(): detected long lasting block, start time shifted by " << (sh / 1000.0) << " secs.");
#endif // WORKER_COPY_DEBUG_MSG
                        startTime += sh;
                    }
                }
            }
            if (ti - startTime > RETRYCOPY_TESTINGTIME)
                break; // we have been reading for quite a while now, we are finishing (two mandatory rounds are behind us)
            if (ti - roundStartTime < 300 && curBufSize < ASYNC_COPY_BUF_SIZE)
            { // if we are fast enough, we will increase the buffer size so that we do not seek unnecessarily often (unnatural seeking towards the beginning of the file)
                curBufSize *= 2;
                if (curBufSize > ASYNC_COPY_BUF_SIZE)
                    curBufSize = ASYNC_COPY_BUF_SIZE;
            }
        }
        roundNum++;
        lastRoundStartTime = roundStartTime;
        lastRoundBufSize = curBufSizeBackup;
    }

    if (ok && asyncPar == NULL) // set in+out to the necessary offsets
    {
        if (!SalSetFilePointer(in, curInOffset))
        {
            CheckTailOfOutFileShowErr("Unable to set file pointer back to current offset in IN file.");
            ok = FALSE;
        }
        if (ok && !SalSetFilePointer(out, offset))
        {
            CheckTailOfOutFileShowErr("Unable to set file pointer back to current offset in OUT file.");
            ok = FALSE;
        }
    }
#ifdef WORKER_COPY_DEBUG_MSG
    if (!ok)
        TRACE_I("CheckTailOfOutFile(): aborting Retry...");
    else
    {
        TRACE_I("CheckTailOfOutFile(): " << (offset.Value - lastOffset.Value) / 1024.0 << " KB tested in " << (GetTickCount() - rutineStartTime) / 1000.0 << " secs (clear read time: " << (GetTickCount() - startTime) / 1000.0 << " secs).");
    }
#endif // WORKER_COPY_DEBUG_MSG
    free(bufIn);
    free(bufOut);
    return ok;
}

// copies the ADS to the newly created file/directory
// FALSE returns only on Cancel; success + Skip returns TRUE; Skip sets 'skip'
// (if not NULL) to TRUE
BOOL DoCopyADS(HWND hProgressDlg, const char* sourceName, BOOL isDir, const char* targetName,
               CQuadWord const& totalDone, CQuadWord& operDone, CQuadWord const& operTotal,
               CProgressDlgData& dlgData, COperations* script, BOOL* skip, void* buffer)
{
    BOOL doCopyADSRet = TRUE;
    BOOL lowMemory;
    DWORD adsWinError;
    wchar_t** streamNames;
    int streamNamesCount;
    BOOL skipped = FALSE;
    CQuadWord lastTransferredFileSize, finalTransferredFileSize;
    script->GetTFSandResetTrSpeedIfNeeded(&lastTransferredFileSize);
    finalTransferredFileSize = lastTransferredFileSize;
    if (operTotal > operDone) // It should always be at least ==, but we compromise...
        finalTransferredFileSize += (operTotal - operDone);

COPY_ADS_AGAIN:

    if (CheckFileOrDirADS(sourceName, isDir, NULL, &streamNames, &streamNamesCount,
                          &lowMemory, &adsWinError, 0, NULL, NULL) &&
        !lowMemory && streamNames != NULL)
    {                                  // We have a list of ADS, let's try to copy them to the target file/directory
        wchar_t srcName[2 * MAX_PATH]; // MAX_PATH for file name and for ADS name (I don't know the actual maximum lengths)
        wchar_t tgtName[2 * MAX_PATH];
        char longSourceName[MAX_PATH + 100];
        char longTargetName[MAX_PATH + 100];
        DoLongName(longSourceName, sourceName, MAX_PATH + 100);
        DoLongName(longTargetName, targetName, MAX_PATH + 100);
        if (!MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, longSourceName, -1, srcName, 2 * MAX_PATH))
            srcName[0] = 0;
        if (!MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, longTargetName, -1, tgtName, 2 * MAX_PATH))
            tgtName[0] = 0;
        wchar_t* srcEnd = srcName + lstrlenW(srcName);
        if (srcEnd > srcName && *(srcEnd - 1) == L'\\')
            *--srcEnd = 0;
        wchar_t* tgtEnd = tgtName + lstrlenW(tgtName);
        if (tgtEnd > tgtName && *(tgtEnd - 1) == L'\\')
            *--tgtEnd = 0;

        int bufferSize = script->RemovableSrcDisk || script->RemovableTgtDisk ? REMOVABLE_DISK_COPY_BUFFER : OPERATION_BUFFER;

        char nameBuf[2 * MAX_PATH];
        BOOL endProcessing = FALSE;
        CQuadWord operationDone;
        int i;
        for (i = 0; i < streamNamesCount; i++)
        {
            MyStrCpyNW(srcEnd, streamNames[i], (int)(2 * MAX_PATH - (srcEnd - srcName)));
            MyStrCpyNW(tgtEnd, streamNames[i], (int)(2 * MAX_PATH - (tgtEnd - tgtName)));

        COPY_AGAIN_ADS:

            operationDone = CQuadWord(0, 0);
            int limitBufferSize = bufferSize;
            script->SetTFSandProgressSize(lastTransferredFileSize, totalDone + operDone, &limitBufferSize, bufferSize);

            BOOL doNextFile = FALSE;
            while (1)
            {
                HANDLE in = CreateFileW(srcName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                HANDLES_ADD_EX(__otQuiet, in != INVALID_HANDLE_VALUE, __htFile,
                               __hoCreateFile, in, GetLastError(), TRUE);
                if (in != INVALID_HANDLE_VALUE)
                {
                    CQuadWord fileSize;
                    fileSize.LoDWord = GetFileSize(in, &fileSize.HiDWord);
                    if (fileSize.LoDWord == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
                    {
                        DWORD err = GetLastError();
                        TRACE_E("GetFileSize(some ADS of " << sourceName << "): unexpected error: " << GetErrorText(err));
                        fileSize.SetUI64(0);
                    }

                    while (1)
                    {
                        HANDLE out = CreateFileW(tgtName, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                        HANDLES_ADD_EX(__otQuiet, out != INVALID_HANDLE_VALUE, __htFile,
                                       __hoCreateFile, out, GetLastError(), TRUE);

                        BOOL canOverwriteMACADSs = TRUE;

                    COPY_OVERWRITE:

                        if (out != INVALID_HANDLE_VALUE)
                        {
                            canOverwriteMACADSs = FALSE;

                            // if possible, we will allocate the necessary space for the file (to prevent disk fragmentation + smoother writing to floppy disks)
                            BOOL wholeFileAllocated = FALSE;
                            if (fileSize > CQuadWord(limitBufferSize, 0) && // it doesn't make sense to allocate a file under the size of the copy buffer
                                fileSize < CQuadWord(0, 0x80000000))        // file size is a positive number (otherwise seeking is not possible - these are numbers above 8EB, so it will probably never happen)
                            {
                                BOOL fatal = TRUE;
                                BOOL ignoreErr = FALSE;
                                if (SalSetFilePointer(out, fileSize))
                                {
                                    if (SetEndOfFile(out))
                                    {
                                        if (SetFilePointer(out, 0, NULL, FILE_BEGIN) == 0)
                                        {
                                            fatal = FALSE;
                                            wholeFileAllocated = TRUE;
                                        }
                                    }
                                    else
                                    {
                                        if (GetLastError() == ERROR_DISK_FULL)
                                            ignoreErr = TRUE; // low disk space
                                    }
                                }
                                if (fatal)
                                {
                                    if (!ignoreErr)
                                    {
                                        DWORD err = GetLastError();
                                        TRACE_E("DoCopyADS(): unable to allocate whole file size before copy operation, please report under what conditions this occurs! GetLastError(): " << GetErrorText(err));
                                    }

                                    // Let's try to truncate the file to zero to avoid any unnecessary writes when closing the file
                                    SetFilePointer(out, 0, NULL, FILE_BEGIN);
                                    SetEndOfFile(out);

                                    HANDLES(CloseHandle(out));
                                    out = INVALID_HANDLE_VALUE;
                                    if (DeleteFileW(tgtName))
                                    {
                                        out = CreateFileW(tgtName, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                                        HANDLES_ADD_EX(__otQuiet, out != INVALID_HANDLE_VALUE, __htFile,
                                                       __hoCreateFile, out, GetLastError(), TRUE);
                                        if (out == INVALID_HANDLE_VALUE)
                                            goto CREATE_ERROR_ADS;
                                    }
                                    else
                                        goto CREATE_ERROR_ADS;
                                }
                            }

                            DWORD read;
                            DWORD written;
                            while (1)
                            {
                                if (ReadFile(in, buffer, limitBufferSize, &read, NULL))
                                {
                                    if (read == 0)
                                        break;                                                     // EOF
                                    if (!script->ChangeSpeedLimit)                                 // if the speed limit can change, this is not a "suitable" place to wait
                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                    if (*dlgData.CancelWorker)
                                    {
                                    COPY_ERROR_ADS:

                                        if (in != NULL)
                                            HANDLES(CloseHandle(in));
                                        if (out != NULL)
                                        {
                                            if (wholeFileAllocated)
                                                SetEndOfFile(out); // Otherwise, the remainder of the file would be written to the floppy disk differently
                                            HANDLES(CloseHandle(out));
                                        }
                                        DeleteFileW(tgtName);
                                        doCopyADSRet = FALSE;
                                        endProcessing = TRUE;
                                        break;
                                    }

                                    while (1)
                                    {
                                        if (WriteFile(out, buffer, read, &written, NULL) && read == written)
                                            break;

                                    WRITE_ERROR_ADS:

                                        DWORD err;
                                        err = GetLastError();

                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                        if (*dlgData.CancelWorker)
                                            goto COPY_ERROR_ADS;

                                        if (dlgData.SkipAllFileADSWrite)
                                            goto SKIP_COPY_ADS;

                                        int ret;
                                        ret = IDCANCEL;
                                        char* data[4];
                                        data[0] = (char*)&ret;
                                        data[1] = LoadStr(IDS_ERRORWRITINGADS);
                                        WideCharToMultiByte(CP_ACP, 0, tgtName, -1, nameBuf, 2 * MAX_PATH, NULL, NULL);
                                        nameBuf[2 * MAX_PATH - 1] = 0;
                                        CutADSNameSuffix(nameBuf);
                                        data[2] = nameBuf;
                                        if (err == NO_ERROR && read != written)
                                            err = ERROR_DISK_FULL;
                                        data[3] = GetErrorText(err);
                                        SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                                        switch (ret)
                                        {
                                        case IDRETRY: // on the network we need to reopen the handle, locally it does not allow sharing
                                        {
                                            if (in == NULL && out == NULL)
                                            {
                                                DeleteFileW(tgtName);
                                                goto COPY_AGAIN_ADS;
                                            }
                                            if (out != NULL)
                                            {
                                                if (wholeFileAllocated)
                                                    SetEndOfFile(out);     // Otherwise, the remainder of the file would be written to the floppy disk differently
                                                HANDLES(CloseHandle(out)); // close invalid handle
                                            }
                                            out = CreateFileW(tgtName, GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_ALWAYS,
                                                              FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                                            HANDLES_ADD_EX(__otQuiet, out != INVALID_HANDLE_VALUE, __htFile,
                                                           __hoCreateFile, out, GetLastError(), TRUE);
                                            if (out != INVALID_HANDLE_VALUE) // open, let's set the offset
                                            {
                                                LONG lo, hi;
                                                lo = GetFileSize(out, (DWORD*)&hi);
                                                if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ||
                                                    CQuadWord(lo, hi) < operationDone ||
                                                    !CheckTailOfOutFile(NULL, in, out, operationDone, operationDone + CQuadWord(read, 0), FALSE))
                                                { // Unable to obtain size or file is too small, let's do it all over again
                                                    HANDLES(CloseHandle(in));
                                                    HANDLES(CloseHandle(out));
                                                    DeleteFileW(tgtName);
                                                    goto COPY_AGAIN_ADS;
                                                }
                                            }
                                            else // cannot open, problem persists ...
                                            {
                                                out = NULL;
                                                goto WRITE_ERROR_ADS;
                                            }
                                            break;
                                        }

                                        case IDB_SKIPALL:
                                            dlgData.SkipAllFileADSWrite = TRUE;
                                        case IDB_SKIP:
                                        {
                                        SKIP_COPY_ADS:

                                            if (in != NULL)
                                                HANDLES(CloseHandle(in));
                                            if (out != NULL)
                                            {
                                                if (wholeFileAllocated)
                                                    SetEndOfFile(out); // Otherwise, the remainder of the file would be written to the floppy disk differently
                                                HANDLES(CloseHandle(out));
                                            }
                                            DeleteFileW(tgtName);
                                            if (skip != NULL)
                                                *skip = TRUE;
                                            skipped = TRUE;
                                            endProcessing = TRUE;
                                            break;
                                        }

                                        case IDCANCEL:
                                            goto COPY_ERROR_ADS;
                                        }
                                        if (endProcessing)
                                            break;
                                    }
                                    if (endProcessing)
                                        break;
                                    if (!script->ChangeSpeedLimit)                                 // if the speed limit can change, this is not a "suitable" place to wait
                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                    if (*dlgData.CancelWorker)
                                        goto COPY_ERROR_ADS;

                                    script->AddBytesToSpeedMetersAndTFSandPS(read, FALSE, bufferSize, &limitBufferSize);

                                    if (!script->ChangeSpeedLimit)                                 // if the speed limit can change, this is not a "suitable" place to wait
                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                    operationDone += CQuadWord(read, 0);
                                    SetProgressWithoutSuspend(hProgressDlg, CaclProg(operDone + operationDone, operTotal),
                                                              CaclProg(totalDone + operDone + operationDone, script->TotalSize),
                                                              dlgData);

                                    if (script->ChangeSpeedLimit)                                  // the speed limit may change, here is a "suitable" place to wait until
                                    {                                                              // worker starts again, we will obtain the buffer size for copying again
                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                        script->GetNewBufSize(&limitBufferSize, bufferSize);
                                    }
                                }
                                else
                                {
                                READ_ERROR_ADS:

                                    DWORD err;
                                    err = GetLastError();
                                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                    if (*dlgData.CancelWorker)
                                        goto COPY_ERROR_ADS;

                                    if (dlgData.SkipAllFileADSRead)
                                        goto SKIP_COPY_ADS;

                                    int ret = IDCANCEL;
                                    char* data[4];
                                    data[0] = (char*)&ret;
                                    data[1] = LoadStr(IDS_ERRORREADINGADS);
                                    WideCharToMultiByte(CP_ACP, 0, srcName, -1, nameBuf, 2 * MAX_PATH, NULL, NULL);
                                    nameBuf[2 * MAX_PATH - 1] = 0;
                                    CutADSNameSuffix(nameBuf);
                                    data[2] = nameBuf;
                                    data[3] = GetErrorText(err);
                                    SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                                    switch (ret)
                                    {
                                    case IDRETRY:
                                    {
                                        if (in != NULL)
                                            HANDLES(CloseHandle(in)); // close invalid handle

                                        in = CreateFileW(srcName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                                        HANDLES_ADD_EX(__otQuiet, in != INVALID_HANDLE_VALUE, __htFile,
                                                       __hoCreateFile, in, GetLastError(), TRUE);
                                        if (in != INVALID_HANDLE_VALUE) // open, let's set the offset
                                        {
                                            LONG lo, hi;
                                            lo = GetFileSize(in, (DWORD*)&hi);
                                            if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ||
                                                CQuadWord(lo, hi) < operationDone ||
                                                !CheckTailOfOutFile(NULL, in, out, operationDone, operationDone, TRUE))
                                            { // Unable to obtain size or file is too small, let's do it all over again
                                                HANDLES(CloseHandle(in));
                                                if (wholeFileAllocated)
                                                    SetEndOfFile(out); // Otherwise, the remainder of the file would be written to the floppy disk differently
                                                HANDLES(CloseHandle(out));
                                                DeleteFileW(tgtName);
                                                goto COPY_AGAIN_ADS;
                                            }
                                        }
                                        else // cannot open, problem persists ...
                                        {
                                            in = NULL;
                                            goto READ_ERROR_ADS;
                                        }
                                        break;
                                    }
                                    case IDB_SKIPALL:
                                        dlgData.SkipAllFileADSRead = TRUE;
                                    case IDB_SKIP:
                                        goto SKIP_COPY_ADS;
                                    case IDCANCEL:
                                        goto COPY_ERROR_ADS;
                                    }
                                }
                            }
                            if (endProcessing)
                                break;

                            if (wholeFileAllocated &&     // We allocated the complete structure of the file
                                operationDone < fileSize) // and the source file has been reduced
                            {
                                if (!SetEndOfFile(out)) // we need to shorten it here
                                {
                                    written = read = 0;
                                    goto WRITE_ERROR_ADS;
                                }
                            }

                            // zakomentovano, protoze misto casu ADSek to nastavuje cas souboru/adresare, ke kteremu ADS patri
                            //              FILETIME creation, lastAccess, lastWrite;
                            //              GetFileTime(in, NULL /*&creation*/, NULL /*&lastAccess*/, &lastWrite);
                            //              SetFileTime(out, NULL /*&creation*/, NULL /*&lastAccess*/, &lastWrite);

                            HANDLES(CloseHandle(in));
                            if (!HANDLES(CloseHandle(out))) // Even after an unsuccessful call, we assume that the handle is closed,
                            {                               // see https://forum.altap.cz/viewtopic.php?f=6&t=8455
                                in = out = NULL;            // (it says that the target file can be deleted, so its handle was not left open)
                                written = read = 0;
                                goto WRITE_ERROR_ADS;
                            }

                            // zakomentovano, protoze misto atributy ADSek to nastavuje atributy souboru/adresare, ke kteremu ADS patri
                            //              DWORD attr = DynGetFileAttributesW(srcName);
                            //              if (attr != INVALID_FILE_ATTRIBUTES) DynSetFileAttributesW(tgtName, attr);

                            operDone += operationDone;
                            lastTransferredFileSize += operationDone;
                            doNextFile = TRUE;
                        }
                        else
                        {
                        CREATE_ERROR_ADS:

                            DWORD err = GetLastError();

                            // Support for MACintosh: NTFS automatically generates ADSka myFile:Afp_Resource and myFile:Afp_AfpInfo,
                            // Without a query, we will overwrite it with versions from the source file
                            if (canOverwriteMACADSs &&
                                (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS) &&
                                (_wcsnicmp(streamNames[i], L":Afp_Resource", 13) == 0 &&
                                     (streamNames[i][13] == 0 || streamNames[i][13] == L':') ||
                                 _wcsnicmp(streamNames[i], L":Afp_AfpInfo", 12) == 0 &&
                                     (streamNames[i][12] == 0 || streamNames[i][12] == L':')))
                            {
                                out = CreateFileW(tgtName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                                  FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                                HANDLES_ADD_EX(__otQuiet, out != INVALID_HANDLE_VALUE, __htFile,
                                               __hoCreateFile, out, GetLastError(), TRUE);

                                canOverwriteMACADSs = FALSE;
                                goto COPY_OVERWRITE;
                            }

                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                            if (*dlgData.CancelWorker)
                                goto CANCEL_OPEN2_ADS;

                            if (dlgData.SkipAllFileADSOpenOut)
                                goto SKIP_OPEN_OUT_ADS;

                            if (dlgData.IgnoreAllADSOpenOutErr)
                                goto IGNORE_OPENOUTADS;

                            int ret;
                            ret = IDCANCEL;
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = LoadStr(IDS_ERROROPENINGADS);
                            WideCharToMultiByte(CP_ACP, 0, tgtName, -1, nameBuf, 2 * MAX_PATH, NULL, NULL);
                            nameBuf[2 * MAX_PATH - 1] = 0;
                            CutADSNameSuffix(nameBuf);
                            data[2] = nameBuf;
                            data[3] = GetErrorText(err);
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 8, (LPARAM)data);
                            switch (ret)
                            {
                            case IDRETRY:
                                break;

                            case IDB_IGNOREALL:
                                dlgData.IgnoreAllADSOpenOutErr = TRUE; // here break; is not missing
                            case IDB_IGNORE:
                            {
                            IGNORE_OPENOUTADS:

                                HANDLES(CloseHandle(in));
                                operDone += fileSize;
                                lastTransferredFileSize += fileSize;
                                script->SetTFSandProgressSize(lastTransferredFileSize, totalDone + operDone);
                                doNextFile = TRUE;
                                break;
                            }

                            case IDB_SKIPALL:
                                dlgData.SkipAllFileADSOpenOut = TRUE;
                            case IDB_SKIP:
                            {
                            SKIP_OPEN_OUT_ADS:

                                HANDLES(CloseHandle(in));
                                if (skip != NULL)
                                    *skip = TRUE;
                                skipped = TRUE;
                                endProcessing = TRUE;
                                break;
                            }

                            case IDCANCEL:
                            {
                            CANCEL_OPEN2_ADS:

                                HANDLES(CloseHandle(in));
                                doCopyADSRet = FALSE;
                                endProcessing = TRUE;
                                break;
                            }
                            }
                        }
                        if (doNextFile || endProcessing)
                            break;
                    }
                }
                else
                {
                    DWORD err = GetLastError();
                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                    if (*dlgData.CancelWorker)
                    {
                        doCopyADSRet = FALSE;
                        endProcessing = TRUE;
                        break;
                    }

                    if (dlgData.SkipAllFileADSOpenIn)
                        goto SKIP_OPEN_IN_ADS;

                    int ret;
                    ret = IDCANCEL;
                    char* data[4];
                    data[0] = (char*)&ret;
                    data[1] = LoadStr(IDS_ERROROPENINGADS);
                    WideCharToMultiByte(CP_ACP, 0, srcName, -1, nameBuf, 2 * MAX_PATH, NULL, NULL);
                    nameBuf[2 * MAX_PATH - 1] = 0;
                    CutADSNameSuffix(nameBuf);
                    data[2] = nameBuf;
                    data[3] = GetErrorText(err);
                    SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                    switch (ret)
                    {
                    case IDRETRY:
                        break;

                    case IDB_SKIPALL:
                        dlgData.SkipAllFileADSOpenIn = TRUE;
                    case IDB_SKIP:
                    {
                    SKIP_OPEN_IN_ADS:

                        if (skip != NULL)
                            *skip = TRUE;
                        skipped = TRUE;
                        endProcessing = TRUE;
                        break;
                    }

                    case IDCANCEL:
                    {
                        doCopyADSRet = FALSE;
                        endProcessing = TRUE;
                        break;
                    }
                    }
                }
                if (doNextFile || endProcessing)
                    break;
            }
            if (endProcessing)
                break;
        }

        for (i = 0; i < streamNamesCount; i++)
            free(streamNames[i]);
        free(streamNames);
    }
    else
    {
        if (adsWinError != NO_ERROR) // we need to display a Windows error (low memory only goes to TRACE_E)
        {
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (dlgData.IgnoreAllADSReadErr)
                goto IGNORE_ADS;

            int ret;
            ret = IDCANCEL;
            char* data[3];
            data[0] = (char*)&ret;
            data[1] = (char*)sourceName;
            data[2] = GetErrorText(adsWinError);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 6, (LPARAM)data);
            switch (ret)
            {
            case IDRETRY:
                goto COPY_ADS_AGAIN;

            case IDB_IGNOREALL:
                dlgData.IgnoreAllADSReadErr = TRUE; // here break; is not missing
            case IDB_IGNORE:
            {
            IGNORE_ADS:

                script->SetTFSandProgressSize(finalTransferredFileSize, totalDone + operTotal);

                SetProgress(hProgressDlg, 0, CaclProg(totalDone + operTotal, script->TotalSize), dlgData);
                return TRUE;
            }

            case IDCANCEL:
                return FALSE;
            }
        }
        if (lowMemory)
            doCopyADSRet = FALSE; // out of memory -> cancelling operation
    }
    if (doCopyADSRet && skipped)
    {
        script->SetTFSandProgressSize(finalTransferredFileSize, totalDone + operTotal);

        SetProgress(hProgressDlg, 0, CaclProg(totalDone + operTotal, script->TotalSize), dlgData);
    }
    return doCopyADSRet;
}

HANDLE SalCreateFileEx(const char* fileName, DWORD desiredAccess,
                       DWORD shareMode, DWORD flagsAndAttributes, BOOL* encryptionNotSupported)
{
    HANDLE out = NOHANDLES(CreateFile(fileName, desiredAccess, shareMode, NULL,
                                      CREATE_NEW, flagsAndAttributes, NULL));
    if (out == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (encryptionNotSupported != NULL && (flagsAndAttributes & FILE_ATTRIBUTE_ENCRYPTED))
        { // in case it is not possible to create an Encrypted file on the target disk (there is a risk on an NTFS network disk (shared from XP machines) where we are logged in under a different name than we have in the system (on the current console) - the trick is that on the target system there is a user with the same name without a password, thus network unusable)
            out = NOHANDLES(CreateFile(fileName, desiredAccess, shareMode, NULL,
                                       CREATE_NEW, (flagsAndAttributes & ~(FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_READONLY)), NULL));
            if (out != INVALID_HANDLE_VALUE)
            {
                *encryptionNotSupported = TRUE;
                NOHANDLES(CloseHandle(out));
                out = INVALID_HANDLE_VALUE;
                if (!DeleteFile(fileName)) // XP or Vista don't bother with it, so I don't care either (also, what to do with it, except warn the user that we added a zero file to the disk that cannot be deleted)
                    TRACE_I("Unable to delete testing target file: " << fileName);
            }
        }
        if (err == ERROR_FILE_EXISTS || // we will check if it's just a rewrite of the DOS file name
            err == ERROR_ALREADY_EXISTS ||
            err == ERROR_ACCESS_DENIED)
        {
            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(fileName, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(find));
                if (err != ERROR_ACCESS_DENIED || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    const char* tgtName = SalPathFindFileName(fileName);
                    if (StrICmp(tgtName, data.cAlternateFileName) == 0 && // match only for two names
                        StrICmp(tgtName, data.cFileName) != 0)            // (full name is different)
                    {
                        // rename ("clean up") the file/directory with a conflicting long name to a temporary 8.3 name (does not require an extra long name)
                        char tmpName[MAX_PATH + 20];
                        lstrcpyn(tmpName, fileName, MAX_PATH);
                        CutDirectory(tmpName);
                        SalPathAddBackslash(tmpName, MAX_PATH + 20);
                        char* tmpNamePart = tmpName + strlen(tmpName);
                        char origFullName[MAX_PATH];
                        if (SalPathAppend(tmpName, data.cFileName, MAX_PATH))
                        {
                            strcpy(origFullName, tmpName);
                            DWORD num = (GetTickCount() / 10) % 0xFFF;
                            DWORD origFullNameAttr = SalGetFileAttributes(origFullName);
                            while (1)
                            {
                                sprintf(tmpNamePart, "sal%03X", num++);
                                if (SalMoveFile(origFullName, tmpName))
                                    break;
                                DWORD e = GetLastError();
                                if (e != ERROR_FILE_EXISTS && e != ERROR_ALREADY_EXISTS)
                                {
                                    tmpName[0] = 0;
                                    break;
                                }
                            }
                            if (tmpName[0] != 0) // if the conflict file has been successfully "cleaned up", we will try to create
                            {                    // target file again, then we return the "cleaned up" file to its original name
                                out = NOHANDLES(CreateFile(fileName, desiredAccess, shareMode, NULL,
                                                           CREATE_NEW, flagsAndAttributes, NULL));
                                if (out == INVALID_HANDLE_VALUE && encryptionNotSupported != NULL &&
                                    (flagsAndAttributes & FILE_ATTRIBUTE_ENCRYPTED))
                                { // in case it is not possible to create an Encrypted file on the target disk (there is a risk on an NTFS network disk (shared from XP machines) where we are logged in under a different name than we have in the system (on the current console) - the trick is that on the target system there is a user with the same name without a password, thus network unusable)
                                    out = NOHANDLES(CreateFile(fileName, desiredAccess, shareMode, NULL,
                                                               CREATE_NEW, (flagsAndAttributes & ~(FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_READONLY)), NULL));
                                    if (out != INVALID_HANDLE_VALUE)
                                    {
                                        *encryptionNotSupported = TRUE;
                                        NOHANDLES(CloseHandle(out));
                                        out = INVALID_HANDLE_VALUE;
                                        if (!DeleteFile(fileName)) // XP or Vista don't bother with it, so I don't care either (also, what to do with it, except warn the user that we added a zero file to the disk that cannot be deleted)
                                            TRACE_E("Unable to delete testing target file: " << fileName);
                                    }
                                }
                                if (!SalMoveFile(tmpName, origFullName))
                                { // this can obviously happen, incomprehensible, but Windows will create a fileName (DOS name) file with the name origFullName
                                    TRACE_I("Unexpected situation in SalCreateFileEx(): unable to rename file from tmp-name to original long file name! " << origFullName);

                                    if (out != INVALID_HANDLE_VALUE)
                                    {
                                        NOHANDLES(CloseHandle(out));
                                        out = INVALID_HANDLE_VALUE;
                                        DeleteFile(fileName);
                                        if (!SalMoveFile(tmpName, origFullName))
                                            TRACE_E("Fatal unexpected situation in SalCreateFileEx(): unable to rename file from tmp-name to original long file name! " << origFullName);
                                    }
                                }
                                else
                                {
                                    if ((origFullNameAttr & FILE_ATTRIBUTE_ARCHIVE) == 0)
                                        SetFileAttributes(origFullName, origFullNameAttr); // Leave it without handling and retry, unimportant (usually set chaotically)
                                }
                            }
                        }
                        else
                            TRACE_E("SalCreateFileEx(): Original full file name is too long, unable to bypass only-dos-name-overwrite problem!");
                    }
                }
            }
        }
        if (out == INVALID_HANDLE_VALUE)
            SetLastError(err);
    }
    return out;
}

BOOL SyncOrAsyncDeviceIoControl(CAsyncCopyParams* asyncPar, HANDLE hDevice, DWORD dwIoControlCode,
                                LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer,
                                DWORD nOutBufferSize, LPDWORD lpBytesReturned, DWORD* err)
{
    if (asyncPar->UseAsyncAlg) // asynchronous version
    {
        if (!DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer,
                             nOutBufferSize, NULL, asyncPar->InitOverlapped(0)) &&
                GetLastError() != ERROR_IO_PENDING ||
            !GetOverlappedResult(hDevice, asyncPar->GetOverlapped(0), lpBytesReturned, TRUE))
        { // error, returning FALSE
            *err = GetLastError();
            return FALSE;
        }
    }
    else // synchronous version
    {
        if (!DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer,
                             nOutBufferSize, lpBytesReturned, NULL))
        { // error, returning FALSE
            *err = GetLastError();
            return FALSE;
        }
    }
    *err = NO_ERROR;
    return TRUE;
}

void SetCompressAndEncryptedAttrs(const char* name, DWORD attr, HANDLE* out, BOOL openAlsoForRead,
                                  BOOL* encryptionNotSupported, CAsyncCopyParams* asyncPar)
{
    if (*out != INVALID_HANDLE_VALUE)
    {
        DWORD err = NO_ERROR;
        DWORD curAttr = SalGetFileAttributes(name);
        if ((curAttr == INVALID_FILE_ATTRIBUTES ||
             (attr & FILE_ATTRIBUTE_COMPRESSED) != (curAttr & FILE_ATTRIBUTE_COMPRESSED)) &&
            (attr & FILE_ATTRIBUTE_COMPRESSED) == 0)
        {
            USHORT state = COMPRESSION_FORMAT_NONE;
            ULONG length;
            if (!SyncOrAsyncDeviceIoControl(asyncPar, *out, FSCTL_SET_COMPRESSION, &state,
                                            sizeof(USHORT), NULL, 0, &length, &err))
            {
                TRACE_I("SetCompressAndEncryptedAttrs(): Unable to set Compressed attribute for " << name << "! error=" << GetErrorText(err));
            }
        }
        if (curAttr == INVALID_FILE_ATTRIBUTES ||
            (attr & FILE_ATTRIBUTE_ENCRYPTED) != (curAttr & FILE_ATTRIBUTE_ENCRYPTED))
        { // in SalCreateFileEx (see above) it probably didn't work
            err = NO_ERROR;
            HANDLES(CloseHandle(*out)); // close the file, otherwise we unfortunately cannot change the Encrypt attribute
            if (attr & FILE_ATTRIBUTE_ENCRYPTED)
            {
                if (!EncryptFile(name))
                {
                    err = GetLastError();
                    if (encryptionNotSupported != NULL)
                        *encryptionNotSupported = TRUE;
                }
            }
            else
            {
                if (!DecryptFile(name, 0))
                    err = GetLastError();
            }
            if (err != NO_ERROR)
                TRACE_I("SetCompressAndEncryptedAttrs(): Unable to set Encrypted attribute for " << name << "! error=" << GetErrorText(err));
            // open the existing file and perform writing
            *out = HANDLES_Q(CreateFile(name, GENERIC_WRITE | (openAlsoForRead ? GENERIC_READ : 0), 0, NULL, OPEN_ALWAYS,
                                        asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
            if (openAlsoForRead && *out == INVALID_HANDLE_VALUE) // Oops, the file cannot be reopened, let's try reopening it only for writing
            {
                *out = HANDLES_Q(CreateFile(name, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                                            asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
            }
            if (*out == INVALID_HANDLE_VALUE) // Oops, the file cannot be reopened, let's try deleting it + report an error
            {
                err = GetLastError();
                DeleteFile(name);
                SetLastError(err);
            }
        }
        if (*out != INVALID_HANDLE_VALUE && // only if there was no error when reopening the file (and subsequently deleting it)
            (curAttr == INVALID_FILE_ATTRIBUTES ||
             (attr & FILE_ATTRIBUTE_COMPRESSED) != (curAttr & FILE_ATTRIBUTE_COMPRESSED)) &&
            (attr & FILE_ATTRIBUTE_COMPRESSED) != 0)
        {
            USHORT state = COMPRESSION_FORMAT_DEFAULT;
            ULONG length;
            if (!SyncOrAsyncDeviceIoControl(asyncPar, *out, FSCTL_SET_COMPRESSION, &state,
                                            sizeof(USHORT), NULL, 0, &length, &err))
            {
                TRACE_I("SetCompressAndEncryptedAttrs(): Unable to set Compressed attribute for " << name << "! error=" << GetErrorText(err));
            }
        }
    }
}

void CorrectCaseOfTgtName(char* tgtName, BOOL dataRead, WIN32_FIND_DATA* data)
{
    if (!dataRead)
    {
        HANDLE find = HANDLES_Q(FindFirstFile(tgtName, data));
        if (find != INVALID_HANDLE_VALUE)
            HANDLES(FindClose(find));
        else
            return; // Failed to load the data of the target file, exiting
    }
    int len = (int)strlen(data->cFileName);
    int tgtNameLen = (int)strlen(tgtName);
    if (tgtNameLen >= len && StrICmp(tgtName + tgtNameLen - len, data->cFileName) == 0)
        memcpy(tgtName + tgtNameLen - len, data->cFileName, len);
}

void SetTFSandPSforSkippedFile(COperation* op, CQuadWord& lastTransferredFileSize,
                               COperations* script, const CQuadWord& pSize)
{
    if (op->FileSize < COPY_MIN_FILE_SIZE)
    {
        lastTransferredFileSize += op->FileSize;                      // file size
        if (op->Size > COPY_MIN_FILE_SIZE)                            // should always be at least COPY_MIN_FILE_SIZE, but we compromise...
            lastTransferredFileSize += op->Size - COPY_MIN_FILE_SIZE; // add the size of ADSek
    }
    else
        lastTransferredFileSize += op->Size; // file size + ADSek
    script->SetTFSandProgressSize(lastTransferredFileSize, pSize);
}

void DoCopyFileLoopOrig(HANDLE& in, HANDLE& out, void* buffer, int& limitBufferSize,
                        COperations* script, CProgressDlgData& dlgData, BOOL wholeFileAllocated,
                        COperation* op, const CQuadWord& totalDone, BOOL& copyError, BOOL& skipCopy,
                        HWND hProgressDlg, CQuadWord& operationDone, CQuadWord& fileSize,
                        int bufferSize, int& allocWholeFileOnStart, BOOL& copyAgain)
{
    int autoRetryAttemptsSNAP = 0;
    DWORD read;
    DWORD written;
    while (1)
    {
        if (ReadFile(in, buffer, limitBufferSize, &read, NULL))
        {
            autoRetryAttemptsSNAP = 0;
            if (read == 0)
                break;                                                     // EOF
            if (!script->ChangeSpeedLimit)                                 // if the speed limit can change, this is not a "suitable" place to wait
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
            {
                copyError = TRUE; // goto COPY_ERROR
                return;
            }

            while (1)
            {
                if (WriteFile(out, buffer, read, &written, NULL) &&
                    read == written)
                {
                    break;
                }

            WRITE_ERROR:

                DWORD err;
                err = GetLastError();

                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                if (*dlgData.CancelWorker)
                {
                    copyError = TRUE; // goto COPY_ERROR
                    return;
                }

                if (dlgData.SkipAllFileWrite)
                {
                    skipCopy = TRUE; // goto SKIP_COPY
                    return;
                }

                int ret;
                ret = IDCANCEL;
                char* data[4];
                data[0] = (char*)&ret;
                data[1] = LoadStr(IDS_ERRORWRITINGFILE);
                data[2] = op->TargetName;
                if (err == NO_ERROR && read != written)
                    err = ERROR_DISK_FULL;
                data[3] = GetErrorText(err);
                SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                switch (ret)
                {
                case IDRETRY: // on the network we need to reopen the handle, locally it does not allow sharing
                {
                    if (out != NULL)
                    {
                        if (wholeFileAllocated)
                            SetEndOfFile(out);     // Otherwise, the remainder of the file would be written to the floppy disk differently
                        HANDLES(CloseHandle(out)); // close invalid handle
                    }
                    out = HANDLES_Q(CreateFile(op->TargetName, GENERIC_WRITE | GENERIC_READ, 0, NULL,
                                               OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                    if (out != INVALID_HANDLE_VALUE) // open, let's set the offset
                    {
                        LONG lo, hi;
                        lo = GetFileSize(out, (DWORD*)&hi);
                        if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR || // unable to retrieve size
                            CQuadWord(lo, hi) < operationDone ||                     // file is too small
                            wholeFileAllocated && CQuadWord(lo, hi) > fileSize &&
                                CQuadWord(lo, hi) > operationDone + CQuadWord(read, 0) || // Preallocated file is too large (larger than preallocated size and also larger than the written size including the last written block) = written bytes have been added to the end of the file (allocWholeFileOnStart should be 0 /* need-test */)
                            !CheckTailOfOutFile(NULL, in, out, operationDone, operationDone + CQuadWord(read, 0), FALSE))
                        { // Let's do it all over again
                            HANDLES(CloseHandle(in));
                            HANDLES(CloseHandle(out));
                            DeleteFile(op->TargetName);
                            copyAgain = TRUE; // goto COPY_AGAIN;
                            return;
                        }
                    }
                    else // cannot open, problem persists ...
                    {
                        out = NULL;
                        goto WRITE_ERROR;
                    }
                    break;
                }

                case IDB_SKIPALL:
                    dlgData.SkipAllFileWrite = TRUE;
                case IDB_SKIP:
                {
                    skipCopy = TRUE; // goto SKIP_COPY
                    return;
                }

                case IDCANCEL:
                {
                    copyError = TRUE; // goto COPY_ERROR
                    return;
                }
                }
            }
            if (!script->ChangeSpeedLimit)                                 // if the speed limit can change, this is not a "suitable" place to wait
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
            {
                copyError = TRUE; // goto COPY_ERROR
                return;
            }

            script->AddBytesToSpeedMetersAndTFSandPS(read, FALSE, bufferSize, &limitBufferSize);

            if (!script->ChangeSpeedLimit)                                 // if the speed limit can change, this is not a "suitable" place to wait
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            operationDone += CQuadWord(read, 0);
            SetProgressWithoutSuspend(hProgressDlg, CaclProg(operationDone, op->Size),
                                      CaclProg(totalDone + operationDone, script->TotalSize), dlgData);

            if (script->ChangeSpeedLimit)                                  // the speed limit may change, here is a "suitable" place to wait until
            {                                                              // worker starts again, we will obtain the buffer size for copying again
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                script->GetNewBufSize(&limitBufferSize, bufferSize);
            }
        }
        else
        {
        READ_ERROR:

            DWORD err;
            err = GetLastError();
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
            {
                copyError = TRUE; // goto COPY_ERROR
                return;
            }

            if (dlgData.SkipAllFileRead)
            {
                skipCopy = TRUE; // goto SKIP_COPY
                return;
            }

            if (err == ERROR_NETNAME_DELETED && ++autoRetryAttemptsSNAP <= 3)
            { // on the SNAP server, there is a random occurrence of the ERROR_NETNAME_DELETED error when reading a file, the Retry button supposedly works, so let's try it automatically
                Sleep(100);
                goto RETRY_COPY;
            }

            int ret;
            ret = IDCANCEL;
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = LoadStr(IDS_ERRORREADINGFILE);
            data[2] = op->SourceName;
            data[3] = GetErrorText(err);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
            switch (ret)
            {
            case IDRETRY:
            {
            RETRY_COPY:

                if (in != NULL)
                    HANDLES(CloseHandle(in)); // close invalid handle
                in = HANDLES_Q(CreateFile(op->SourceName, GENERIC_READ,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                          OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                if (in != INVALID_HANDLE_VALUE) // open, let's set the offset
                {
                    LONG lo, hi;
                    lo = GetFileSize(in, (DWORD*)&hi);
                    if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ||
                        CQuadWord(lo, hi) < operationDone ||
                        !CheckTailOfOutFile(NULL, in, out, operationDone, operationDone, TRUE))
                    { // Unable to obtain size or file is too small, let's do it all over again
                        HANDLES(CloseHandle(in));
                        if (wholeFileAllocated)
                            SetEndOfFile(out); // Otherwise, the remainder of the file would be written to the floppy disk differently
                        HANDLES(CloseHandle(out));
                        DeleteFile(op->TargetName);
                        copyAgain = TRUE; // goto COPY_AGAIN;
                        return;
                    }
                }
                else // cannot open, problem persists ...
                {
                    in = NULL;
                    goto READ_ERROR;
                }
                break;
            }

            case IDB_SKIPALL:
                dlgData.SkipAllFileRead = TRUE;
            case IDB_SKIP:
            {
                skipCopy = TRUE; // goto SKIP_COPY
                return;
            }

            case IDCANCEL:
            {
                copyError = TRUE; // goto COPY_ERROR
                return;
            }
            }
        }
    }

    if (wholeFileAllocated) // We allocated the complete structure of the file (meaning that the allocation made sense, for example, the file cannot be null)
    {
        if (operationDone < fileSize) // and the source file has been reduced
        {
            if (!SetEndOfFile(out)) // we need to shorten it here
            {
                written = read = 0;
                goto WRITE_ERROR;
            }
        }

        if (allocWholeFileOnStart == 0 /* need-test*/)
        {
            CQuadWord curFileSize;
            curFileSize.LoDWord = GetFileSize(out, &curFileSize.HiDWord);
            BOOL getFileSizeSuccess = (curFileSize.LoDWord != INVALID_FILE_SIZE || GetLastError() == NO_ERROR);
            if (getFileSizeSuccess && curFileSize == operationDone)
            { // check if no writable bytes have been added to the end of the file + if we can truncate the file
                allocWholeFileOnStart = 1 /* yes*/;
            }
            else
            {
#ifdef _DEBUG
                if (getFileSizeSuccess)
                {
                    char num1[50];
                    char num2[50];
                    TRACE_E("DoCopyFileLoopOrig(): unable to allocate whole file size before copy operation, please report "
                            "under what conditions this occurs! Error: different file sizes: target="
                            << NumberToStr(num1, curFileSize) << " bytes, source=" << NumberToStr(num2, operationDone) << " bytes");
                }
                else
                {
                    DWORD err = GetLastError();
                    TRACE_E("DoCopyFileLoopOrig(): unable to test result of allocation of whole file size before copy operation, please report "
                            "under what conditions this occurs! GetFileSize("
                            << op->TargetName << ") error: " << GetErrorText(err));
                }
#endif
                allocWholeFileOnStart = 2 /* no*/; // We will refrain from further attempts on this target disk

                HANDLES(CloseHandle(out));
                out = NULL;
                ClearReadOnlyAttr(op->TargetName); // If it were to occur as read-only (it should never happen), we would handle it.
                if (DeleteFile(op->TargetName))
                {
                    HANDLES(CloseHandle(in));
                    copyAgain = TRUE; // goto COPY_AGAIN;
                    return;
                }
                else
                {
                    written = read = 0;
                    goto WRITE_ERROR;
                }
            }
        }
    }
}

enum CCopy_BlkState
{
    cbsFree,       // block is not used
    cbsRead,       // reading source file into block - completed (waiting for write)
    cbsInProgress, // --- below are the states of the "waiting for operation completion" blocks (completed states are above)
    cbsReading,    // reading source file into block - in progress
    cbsTestingEOF, // testing end of source file
    cbsWriting,    // write block to target file
    cbsDiscarded,  // Reading from the source file past the end of the file (should return only an error: EOF)
};

enum CCopy_ForceOp
{
    fopNotUsed, // we can read or write as needed...
    fopReading, // we need to read
    fopWriting  // we need to write
};

struct CCopy_Context
{
    CAsyncCopyParams* AsyncPar;

    CCopy_ForceOp ForceOp;        // TRUE = now it must be read, FALSE = now it must be written
    BOOL ReadingDone;             // TRUE = source file is already fully loaded
    CCopy_BlkState BlockState[8]; // block status
    DWORD BlockDataLen[8];        // for each block: expected data (cbsReading + cbsTestingEOF), valid data (cbsWriting)
    CQuadWord BlockOffset[8];     // for each block: block offset in the source/target file (also in the OVERLAPPED structure in 'AsyncPar')
    DWORD BlockTime[8];           // for each block: "time" start of the last asynchronous operation in this block
    DWORD CurTime;                // "cas" pro 'BlockTime', pocitame s pretecenim (i kdyz je asi nerealne)
    int FreeBlocks;               // current number of free blocks (cbsFree)
    int FreeBlockIndex;           // Hint for the index of a block that is free (cbsFree), must be verified!
    int ReadingBlocks;            // current number of blocks being read (cbsReading and cbsTestingEOF)
    int WritingBlocks;            // current number of blocks being written to the file (cbsWriting)
    CQuadWord ReadOffset;         // offset for reading the next block from the source file (the previous one has been read)
    CQuadWord WriteOffset;        // offset for writing the next block to the target file (the previous one is being/was written)
    int AutoRetryAttemptsSNAP;    // Number of automatic Retry repetitions (we do not do more than 3 times): on the SNAP server, when reading a file, there is a random occurrence of the ERROR_NETNAME_DELETED error, the Retry button supposedly works, so we press it automatically

    // selected parameters for DoCopyFileLoopAsync, so it doesn't have to be passed everywhere in the function call parameters
    CProgressDlgData* DlgData;
    COperation* Op;
    HWND HProgressDlg;
    HANDLE* In;
    HANDLE* Out;
    BOOL WholeFileAllocated;
    COperations* Script;
    CQuadWord* OperationDone;
    const CQuadWord* TotalDone;
    const CQuadWord* LastTransferredFileSize;

    CCopy_Context(CAsyncCopyParams* asyncPar, int numOfBlocks, CProgressDlgData* dlgData, COperation* op,
                  HWND hProgressDlg, HANDLE* in, HANDLE* out, BOOL wholeFileAllocated, COperations* script,
                  CQuadWord* operationDone, const CQuadWord* totalDone, const CQuadWord* lastTransferredFileSize)
    {
        AsyncPar = asyncPar;
        ForceOp = fopNotUsed;
        ReadingDone = FALSE;
        CurTime = 0;
        for (int i = 0; i < _countof(BlockState); i++)
            BlockState[i] = cbsFree;
        memset(BlockDataLen, 0, sizeof(BlockDataLen));
        memset(BlockOffset, 0, sizeof(BlockOffset));
        memset(BlockTime, 0, sizeof(BlockTime));
        FreeBlocks = numOfBlocks;
        FreeBlockIndex = 0;
        ReadingBlocks = 0;
        WritingBlocks = 0;
        ReadOffset.SetUI64(0);
        WriteOffset.SetUI64(0);
        AutoRetryAttemptsSNAP = 0;

        DlgData = dlgData;
        Op = op;
        HProgressDlg = hProgressDlg;
        In = in;
        Out = out;
        WholeFileAllocated = wholeFileAllocated;
        Script = script;
        OperationDone = operationDone;
        TotalDone = totalDone;
        LastTransferredFileSize = lastTransferredFileSize;
    }

    BOOL IsOperationDone(int numOfBlocks)
    {
        return ReadingDone && FreeBlocks == numOfBlocks;
    }

    BOOL StartReading(int blkIndex, DWORD readSize, DWORD* err, BOOL testEOF);
    BOOL StartWriting(int blkIndex, DWORD* err);
    int FindBlock(CCopy_BlkState state);
    void FreeBlock(int blkIndex);
    void DiscardBlocksBehindEOF(const CQuadWord& fileSize, int excludeIndex);
    void GetNewFileSize(const char* fileName, HANDLE file, CQuadWord* fileSize, const CQuadWord& minFileSize);

    BOOL HandleReadingErr(int blkIndex, DWORD err, BOOL* copyError, BOOL* skipCopy, BOOL* copyAgain);
    BOOL HandleWritingErr(int blkIndex, DWORD err, BOOL* copyError, BOOL* skipCopy, BOOL* copyAgain,
                          const CQuadWord& allocFileSize, const CQuadWord& maxWriteOffset);

    // Interrupts any pending asynchronous operations
    void CancelOpPhase1();
    // Ensure that all asynchronous operations have actually completed + set the pointer to the end consistently
    // written parts of the target file so that the file is properly handled (before potentially closing and deleting it)
    // WARNING: release unnecessary blocks, only those with loaded data from the IN file remain, in addition
    //        Continuing from WriteOffset (usable for Retry)
    void CancelOpPhase2(int errBlkIndex);
    BOOL RetryCopyReadErr(DWORD* err, BOOL* copyAgain, BOOL* errAgain);
    BOOL RetryCopyWriteErr(DWORD* err, BOOL* copyAgain, BOOL* errAgain, const CQuadWord& allocFileSize,
                           const CQuadWord& maxWriteOffset);
    BOOL HandleSuspModeAndCancel(BOOL* copyError);
};

BOOL DisableLocalBuffering(CAsyncCopyParams* asyncPar, HANDLE file, DWORD* err)
{
    CALL_STACK_MESSAGE1("DisableLocalBuffering()");
    if (DynNtFsControlFile != NULL) // "always true"
    {
        IO_STATUS_BLOCK ioStatus;
        ResetEvent(asyncPar->Overlapped[0].hEvent);
        ULONG status = DynNtFsControlFile(file, asyncPar->Overlapped[0].hEvent, NULL,
                                          0, &ioStatus, 0x00140390 /* IOCTL_LMR_DISABLE_LOCAL_BUFFERING*/,
                                          NULL, 0, NULL, 0);
        if (status == STATUS_PENDING) // we need to wait for the operation to finish, it is running asynchronously
        {
            CALL_STACK_MESSAGE1("DisableLocalBuffering(): STATUS_PENDING");
            WaitForSingleObject(asyncPar->Overlapped[0].hEvent, INFINITE);
            status = ioStatus.Status;
        }
        if (status == 0 /* STATUS_SUCCESS*/)
            return TRUE;
        *err = LsaNtStatusToWinError(status);
    }
    else
        *err = ERROR_INVALID_FUNCTION;
    return FALSE;
}

BOOL CCopy_Context::StartReading(int blkIndex, DWORD readSize, DWORD* err, BOOL testEOF)
{
#ifdef ASYNC_COPY_DEBUG_MSG
    char sss[1000];
    sprintf(sss, "ReadFile: %d 0x%08X 0x%08X", blkIndex, ReadOffset.LoDWord, readSize);
    TRACE_I(sss);
#endif // ASYNC_COPY_DEBUG_MSG

    if (!ReadFile(*In, AsyncPar->Buffers[blkIndex], readSize, NULL,
                  AsyncPar->InitOverlappedWithOffset(blkIndex, ReadOffset)) &&
        GetLastError() != ERROR_IO_PENDING)
    { // an error occurred while reading, let's solve it
        *err = GetLastError();
        if (*err == ERROR_HANDLE_EOF) // synchronously reported EOF, we will convert it to asynchronously reported EOF
            AsyncPar->SetOverlappedToEOF(blkIndex, ReadOffset);
        else
            return FALSE;
    }
    // if the reading was done synchronously (or from cache, which unfortunately I am not able to detect), then
    // we need to write something, otherwise the write operation may fail = overall operation slowdown
    BOOL opCompleted = HasOverlappedIoCompleted(AsyncPar->GetOverlapped(blkIndex));
    ForceOp = opCompleted ? fopWriting : fopNotUsed;

#ifdef ASYNC_COPY_DEBUG_MSG
    TRACE_I("ReadFile result: " << (opCompleted ? "DONE" : "ASYNC"));
#endif // ASYNC_COPY_DEBUG_MSG

    if (opCompleted && !Script->ChangeSpeedLimit)                   // if the speed limit can change, this is not a "suitable" place to wait
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
    if (*DlgData->CancelWorker)
    {
        *err = ERROR_CANCELLED;
        return FALSE; // cancel is performed in error handling
    }

    BlockOffset[blkIndex] = ReadOffset;
    BlockDataLen[blkIndex] = readSize;
    if (!testEOF) // the block was in the cbsFree state before calling this method
    {
        ReadOffset.Value += readSize;
        BlockState[blkIndex] = cbsReading;
    }
    else
        BlockState[blkIndex] = cbsTestingEOF;
    BlockTime[blkIndex] = CurTime++;
    FreeBlocks--;
    ReadingBlocks++;
    return TRUE;
}

BOOL CCopy_Context::StartWriting(int blkIndex, DWORD* err)
{
#ifdef ASYNC_COPY_DEBUG_MSG
    char sss[1000];
    sprintf(sss, "WriteFile: %d 0x%08X 0x%08X", blkIndex, WriteOffset.LoDWord, BlockDataLen[blkIndex]);
    TRACE_I(sss);
#endif // ASYNC_COPY_DEBUG_MSG

    if (!WriteFile(*Out, AsyncPar->Buffers[blkIndex], BlockDataLen[blkIndex], NULL,
                   AsyncPar->InitOverlappedWithOffset(blkIndex, WriteOffset)) &&
        GetLastError() != ERROR_IO_PENDING)
    { // an error in writing occurred, let's solve it
        *err = GetLastError();
        return FALSE;
    }
    // if the write operation was synchronous (or to cache, which unfortunately I am not able to detect), then
    // we need to read something, otherwise reading can fail = overall operation slowdown
    BOOL opCompleted = HasOverlappedIoCompleted(AsyncPar->GetOverlapped(blkIndex));
    ForceOp = !ReadingDone && opCompleted ? fopReading : fopNotUsed;

#ifdef ASYNC_COPY_DEBUG_MSG
    TRACE_I("WriteFile result: " << (opCompleted ? "DONE" : "ASYNC"));
#endif // ASYNC_COPY_DEBUG_MSG

    if (opCompleted && !Script->ChangeSpeedLimit)                   // if the speed limit can change, this is not a "suitable" place to wait
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
    if (*DlgData->CancelWorker)
    {
        *err = ERROR_CANCELLED;
        return FALSE; // cancel is performed in error handling
    }

    WriteOffset.Value += BlockDataLen[blkIndex];
    BlockState[blkIndex] = cbsWriting; // the block was in the cbsRead state before calling this method
    BlockTime[blkIndex] = CurTime++;
    WritingBlocks++;
    return TRUE;
}

int CCopy_Context::FindBlock(CCopy_BlkState state)
{
    for (int i = 0; i < _countof(BlockState); i++)
        if (BlockState[i] == state)
            return i;
    TRACE_C("CCopy_Context::FindBlock(): unable to find block with required state (" << (int)state << ").");
    return -1; // dead code, just for the compiler
}

void CCopy_Context::FreeBlock(int blkIndex)
{
    if (BlockState[blkIndex] == cbsReading || BlockState[blkIndex] == cbsTestingEOF)
        ReadingBlocks--;
    if (BlockState[blkIndex] == cbsWriting)
        WritingBlocks--;
    BlockState[blkIndex] = cbsFree;
    FreeBlockIndex = blkIndex;
    FreeBlocks++;
}

void CCopy_Context::DiscardBlocksBehindEOF(const CQuadWord& fileSize, int excludeIndex)
{
    for (int i = 0; i < _countof(BlockState); i++)
    {
        if (i == excludeIndex)
            continue;
        CCopy_BlkState st = BlockState[i];
        if ((st == cbsRead || st == cbsReading) && BlockOffset[i] >= fileSize)
        {
            if (st == cbsRead) // discard the read data beyond the end of the file, they are meaningless
                FreeBlock(i);
            else
            {
                BlockState[i] = cbsDiscarded; // Reading beyond the end of the file doesn't make sense; BlockTime is not a reason to change
                ReadingBlocks--;
            }
        }
    }
}

void CCopy_Context::GetNewFileSize(const char* fileName, HANDLE file, CQuadWord* fileSize, const CQuadWord& minFileSize)
{
    fileSize->LoDWord = GetFileSize(file, &fileSize->HiDWord);
    if (fileSize->LoDWord == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
    {
        DWORD err = GetLastError();
        TRACE_E("CCopy_Context::GetNewFileSize(): GetFileSize(" << fileName << "): unexpected error: " << GetErrorText(err));
        *fileSize = minFileSize;
    }
    else
    {
        if (*fileSize < minFileSize) // if by any chance GetFileSize returned a file shorter than we have already loaded
            *fileSize = minFileSize;
    }
}

void CCopy_Context::CancelOpPhase1()
{
    if (!CancelIo(*In))
    {
        DWORD err = GetLastError();
        TRACE_E("CCopy_Context::CancelOpPhase1(): CancelIo(IN) failed, error: " << GetErrorText(err));
    }
    if (*Out != NULL && !CancelIo(*Out))
    {
        DWORD err = GetLastError();
        TRACE_E("CCopy_Context::CancelOpPhase1(): CancelIo(OUT) failed, error: " << GetErrorText(err));
    }
}

void CCopy_Context::CancelOpPhase2(int errBlkIndex)
{
    // WARNING: errBlkIndex == -1 for error in asynchronous read request (no block allocated)
    //        or for an error when truncating the file after the main copying cycle has finished (no block allocated)
    //        or for Cancel in the progress dialog (does not have a dedicated block)

    DWORD bytes;
    for (int i = 0; i < _countof(BlockState); i++)
    {
        if (BlockState[i] > cbsInProgress)
        { // GetOverlappedResult should return the result immediately because we called CancelIo() for both files
            if (GetOverlappedResult(BlockState[i] == cbsWriting ? *Out : *In, AsyncPar->GetOverlapped(i), &bytes, TRUE))
            {
                if (BlockState[i] == cbsReading && BlockDataLen[i] == bytes) // completely read -> change to cbsRead block
                {
                    BlockState[i] = cbsRead;
                    ReadingBlocks--;
                }
                else
                {
                    if (BlockState[i] == cbsWriting && BlockDataLen[i] == bytes) // complete write -> change to cbsRead block (we may write again, so let's not discard the block now)
                    {
                        BlockState[i] = cbsRead;
                        WritingBlocks--;
                    }
                }
            }
            else
            {
                DWORD err = GetLastError();
                if (i != errBlkIndex &&             // For this block, we report an error, reporting it again to TRACE doesn't make sense
                    err != ERROR_OPERATION_ABORTED) // this is not an error, just reporting that it was interrupted (by calling CancelIo())
                {                                   // we will print out errors in the following blocks, most likely nothing important and we should just ignore them
                    TRACE_I("CCopy_Context::CancelOpPhase2(): GetOverlappedResult(" << (BlockState[i] == cbsWriting ? "OUT" : "IN") << ", " << i << ") returned error: " << GetErrorText(err));
                }
            }
            switch (BlockState[i])
            {
            case cbsReading:    // not completely read
            case cbsTestingEOF: // unfinished test of EOF
            case cbsDiscarded:
                FreeBlock(i);
                break;

            case cbsWriting:                      // unregistered block
                if (WriteOffset > BlockOffset[i]) // Alternatively, we can decrease WriteOffset
                    WriteOffset = BlockOffset[i];
                BlockState[i] = cbsRead; // not completely written, but read yes -> change to cbsRead block (we may rewrite, so let's not discard the block now)
                WritingBlocks--;
                break;
            }
        }
    }

    ReadOffset = WriteOffset; // we will find out where we have continuously read data from the offset where we need to start writing
    for (int i = 0; i < _countof(BlockState); i++)
    {
        if (BlockState[i] == cbsRead && BlockOffset[i] == ReadOffset) // We have read a block following ReadOffset
        {
            ReadOffset.Value += BlockDataLen[i];
            i = -1; // and we are looking again from the beginning (with 8 blocks we can afford it, max. 36 passes through the loop)
        }
    }

    // discard blocks that are already written or, conversely, are too far ahead (not linked),
    // so let's rather reload it
    for (int i = 0; i < _countof(BlockState); i++)
        if (BlockState[i] == cbsRead && (BlockOffset[i] < WriteOffset || BlockOffset[i] > ReadOffset))
            FreeBlock(i);

    // in case of target file corruption, we will set the file pointer to the end of the written part, caller
    // then SetEndOfFile truncates the file before deleting it (otherwise it could lead to nonsense
    // write zeros from the end of the written part to the end of the file - the file is preallocated because of
    // prevention of fragmentation)
    if (*Out != NULL) // only if the target file has not been closed in the meantime
    {
        if (!SalSetFilePointer(*Out, WriteOffset))
        {
            DWORD err = GetLastError();
            TRACE_E("CCopy_Context::CancelOpPhase2(): unable to set file pointer in OUT file, error: " << GetErrorText(err));
        }
    }
}

BOOL CCopy_Context::RetryCopyReadErr(DWORD* err, BOOL* copyAgain, BOOL* errAgain)
{
    if (*In != NULL)
        HANDLES(CloseHandle(*In)); // close invalid handle
    *In = HANDLES_Q(CreateFile(Op->SourceName, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, AsyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (*In != INVALID_HANDLE_VALUE) // open, let's set the offset
    {
        CQuadWord size;
        size.LoDWord = GetFileSize(*In, (DWORD*)&size.HiDWord);
        if ((size.LoDWord != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) && size >= ReadOffset)
        { // It is possible to obtain the size and the file is quite large
            // if the source is on the network: disable local client-side in-memory caching
            // http://msdn.microsoft.com/en-us/library/ee210753%28v=vs.85%29.aspx
            //
            // Using Overlapped[0].hEvent from AsyncPar is OK, nothing is "in-progress" now, the event is not being used
            // (however BEWARE: for example, Buffers[0] from AsyncPar can be used)
            if ((Op->OpFlags & OPFL_SRCPATH_IS_NET) && !DisableLocalBuffering(AsyncPar, *In, err))
                TRACE_E("CCopy_Context::RetryCopyReadErr(): IOCTL_LMR_DISABLE_LOCAL_BUFFERING failed for network source file: " << Op->SourceName << ", error: " << GetErrorText(*err));
            // Using Overlapped[0 and 1].hEvent and Overlapped[0 and 1] from AsyncPar is OK, there is nothing now
            // "in-progress", event or overlapped structure is not used (however BEWARE: for example Buffers[0]
            // can be used from AsyncPar)
            if (CheckTailOfOutFile(AsyncPar, *In, *Out, WriteOffset, WriteOffset, TRUE))
            {
                ForceOp = ReadOffset > WriteOffset ? fopWriting : fopNotUsed; // if we have read first, we will start writing
                *OperationDone = WriteOffset;
                Script->SetTFSandProgressSize(*LastTransferredFileSize + *OperationDone, *TotalDone + *OperationDone);
                SetProgressWithoutSuspend(HProgressDlg, CaclProg(*OperationDone, Op->Size),
                                          CaclProg(*TotalDone + *OperationDone, Script->TotalSize), *DlgData);
                return TRUE; // Success: let's go for a retry
            }
        }
        // unable to obtain size or the file is too small or the last written part of the file does not match the source, let's do it all over again
        HANDLES(CloseHandle(*In));
        if (WholeFileAllocated)
            SetEndOfFile(*Out); // Otherwise, the remainder of the file would be written to the floppy disk differently
        HANDLES(CloseHandle(*Out));
        DeleteFile(Op->TargetName);
        *copyAgain = TRUE; // goto COPY_AGAIN;
        return FALSE;
    }
    else // cannot open, problem persists ...
    {
        *err = GetLastError();
        *In = NULL;
        *errAgain = TRUE; // goto READ_ERROR;
        return FALSE;
    }
}

BOOL CCopy_Context::HandleReadingErr(int blkIndex, DWORD err, BOOL* copyError, BOOL* skipCopy, BOOL* copyAgain)
{
    // WARNING: blkIndex == -1 for an error in asynchronous read input (no block assigned)

    CancelOpPhase1();

    while (1)
    {
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
        if (*DlgData->CancelWorker)
        {
            CancelOpPhase2(blkIndex);
            *copyError = TRUE; // goto COPY_ERROR
            return FALSE;
        }

        if (DlgData->SkipAllFileRead)
        {
            CancelOpPhase2(blkIndex);
            *skipCopy = TRUE; // goto SKIP_COPY
            return FALSE;
        }

        int ret = IDCANCEL;
        if (err == ERROR_NETNAME_DELETED && ++AutoRetryAttemptsSNAP <= 3)
        { // on the SNAP server, there is a random occurrence of the ERROR_NETNAME_DELETED error when reading a file, the Retry button supposedly works, so let's try it automatically
            Sleep(100);
            ret = IDRETRY;
        }
        else
        {
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = LoadStr(IDS_ERRORREADINGFILE);
            data[2] = Op->SourceName;
            data[3] = GetErrorText(err);
            SendMessage(HProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
        }
        CancelOpPhase2(blkIndex);
        BOOL errAgain = FALSE;
        switch (ret)
        {
        case IDRETRY:
        {
            if (RetryCopyReadErr(&err, copyAgain, &errAgain))
                return TRUE; // retry
            else
            {
                if (errAgain)
                    break;    // same problem again, repeating the message
                return FALSE; // copyAgain==TRUE, goto COPY_AGAIN;
            }
        }

        case IDB_SKIPALL:
            DlgData->SkipAllFileRead = TRUE;
        case IDB_SKIP:
        {
            *skipCopy = TRUE; // goto SKIP_COPY
            return FALSE;
        }

        case IDCANCEL:
        {
            *copyError = TRUE; // goto COPY_ERROR
            return FALSE;
        }
        }
        if (errAgain)
            continue; // IDRETRY: same problem again, repeating the message
        TRACE_C("CCopy_Context::HandleReadingErr(): unexpected result of WM_USER_DIALOG(0).");
        return TRUE;
    }
}

BOOL CCopy_Context::RetryCopyWriteErr(DWORD* err, BOOL* copyAgain, BOOL* errAgain,
                                      const CQuadWord& allocFileSize, const CQuadWord& maxWriteOffset)
{
    if (*Out != NULL)
    {
        if (WholeFileAllocated)
            SetEndOfFile(*Out);     // Otherwise, the remainder of the file would be written to the floppy disk differently
        HANDLES(CloseHandle(*Out)); // close invalid handle
    }
    *Out = HANDLES_Q(CreateFile(Op->TargetName, GENERIC_WRITE | GENERIC_READ, 0, NULL,
                                OPEN_ALWAYS, AsyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (*Out != INVALID_HANDLE_VALUE) // open, let's set the offset
    {
        BOOL ok = TRUE;
        CQuadWord size;
        size.LoDWord = GetFileSize(*Out, (DWORD*)&size.HiDWord);
        if (size.LoDWord == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ||   // unable to retrieve size
            size < WriteOffset ||                                                // file is too small
            WholeFileAllocated && size > allocFileSize && size > maxWriteOffset) // Preallocated file is too large (larger than preallocated size and also larger than the written size including the last written block) = written bytes have been added to the end of the file (allocWholeFileOnStart should be 0 /* need-test */)
        {                                                                        // Let's do it all over again
            ok = FALSE;
        }
        // success (the file is just the right size)
        // if the target on the network: disable local client-side in-memory caching
        // http://msdn.microsoft.com/en-us/library/ee210753%28v=vs.85%29.aspx
        //
        // Using Overlapped[0].hEvent from AsyncPar is OK, nothing is "in-progress" now, the event is not being used
        // (however BEWARE: for example, Buffers[0] from AsyncPar can be used)
        if (ok && (Op->OpFlags & OPFL_TGTPATH_IS_NET) && !DisableLocalBuffering(AsyncPar, *Out, err))
            TRACE_E("CCopy_Context::RetryCopyWriteErr(): IOCTL_LMR_DISABLE_LOCAL_BUFFERING failed for network target file: " << Op->TargetName << ", error: " << GetErrorText(*err));
        // Using Overlapped[0 and 1].hEvent and Overlapped[0 and 1] from AsyncPar is OK, there is nothing now
        // "in-progress", event or overlapped structure is not used (however BEWARE: for example Buffers[0]
        // can be used from AsyncPar)
        if (!ok || !CheckTailOfOutFile(AsyncPar, *In, *Out, WriteOffset, WriteOffset, FALSE))
        {
            HANDLES(CloseHandle(*In));
            HANDLES(CloseHandle(*Out));
            DeleteFile(Op->TargetName);
            *copyAgain = TRUE; // goto COPY_AGAIN;
            return FALSE;
        }
        ForceOp = ReadOffset > WriteOffset ? fopWriting : fopNotUsed; // if we have read first, we will start writing
        *OperationDone = WriteOffset;
        Script->SetTFSandProgressSize(*LastTransferredFileSize + *OperationDone, *TotalDone + *OperationDone);
        SetProgressWithoutSuspend(HProgressDlg, CaclProg(*OperationDone, Op->Size),
                                  CaclProg(*TotalDone + *OperationDone, Script->TotalSize), *DlgData);
        return TRUE; // Success: let's go for a retry
    }
    else // cannot open, problem persists ...
    {
        *err = GetLastError();
        *Out = NULL;
        *errAgain = TRUE; // goto WRITE_ERROR;
        return FALSE;
    }
}

BOOL CCopy_Context::HandleWritingErr(int blkIndex, DWORD err, BOOL* copyError, BOOL* skipCopy, BOOL* copyAgain,
                                     const CQuadWord& allocFileSize, const CQuadWord& maxWriteOffset)
{
    // WARNING: blkIndex == -1 for an error when truncating a file after the main copying cycle has finished (no block allocated)

    CancelOpPhase1();

    while (1)
    {
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
        if (*DlgData->CancelWorker)
        {
            CancelOpPhase2(blkIndex);
            *copyError = TRUE; // goto COPY_ERROR
            return FALSE;
        }

        if (DlgData->SkipAllFileWrite)
        {
            CancelOpPhase2(blkIndex);
            *skipCopy = TRUE; // goto SKIP_COPY
            return FALSE;
        }

        int ret = IDCANCEL;
        char* data[4];
        data[0] = (char*)&ret;
        data[1] = LoadStr(IDS_ERRORWRITINGFILE);
        data[2] = Op->TargetName;
        data[3] = GetErrorText(err);
        SendMessage(HProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
        CancelOpPhase2(blkIndex);
        BOOL errAgain = FALSE;
        switch (ret)
        {
        case IDRETRY:
        {
            if (RetryCopyWriteErr(&err, copyAgain, &errAgain, allocFileSize, maxWriteOffset))
                return TRUE; // retry
            else
            {
                if (errAgain)
                    break;    // same problem again, repeating the message
                return FALSE; // copyAgain==TRUE, goto COPY_AGAIN;
            }
        }

        case IDB_SKIPALL:
            DlgData->SkipAllFileWrite = TRUE;
        case IDB_SKIP:
        {
            *skipCopy = TRUE; // goto SKIP_COPY
            return FALSE;
        }

        case IDCANCEL:
        {
            *copyError = TRUE; // goto COPY_ERROR
            return FALSE;
        }
        }
        if (errAgain)
            continue; // IDRETRY: same problem again, repeating the message
        TRACE_C("CCopy_Context::HandleWritingErr(): unexpected result of WM_USER_DIALOG(0).");
        return TRUE;
    }
}

BOOL CCopy_Context::HandleSuspModeAndCancel(BOOL* copyError)
{
    if (!Script->ChangeSpeedLimit)                                  // if the speed limit cannot be changed (otherwise this is not a "suitable" place to wait)
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
    if (*DlgData->CancelWorker)
    {
        CancelOpPhase1();
        CancelOpPhase2(-1);
        *copyError = TRUE; // goto COPY_ERROR
        return TRUE;
    }
    return FALSE;
}

void DoCopyFileLoopAsync(CAsyncCopyParams* asyncPar, HANDLE& in, HANDLE& out, void* buffer, int& limitBufferSize,
                         COperations* script, CProgressDlgData& dlgData, BOOL wholeFileAllocated, COperation* op,
                         const CQuadWord& totalDone, BOOL& copyError, BOOL& skipCopy, HWND hProgressDlg,
                         CQuadWord& operationDone, CQuadWord& fileSize, int bufferSize,
                         int& allocWholeFileOnStart, BOOL& copyAgain, const CQuadWord& lastTransferredFileSize)
{
    CQuadWord allocFileSize = fileSize;
    DWORD err = NO_ERROR;
    DWORD bytes = 0; // helper DWORD - how many bytes were read/written in the block

    // if the source/target is on the network: disable local client-side in-memory caching
    // http://msdn.microsoft.com/en-us/library/ee210753%28v=vs.85%29.aspx
    if ((op->OpFlags & OPFL_SRCPATH_IS_NET) && !DisableLocalBuffering(asyncPar, in, &err))
        TRACE_E("DoCopyFileLoopAsync(): IOCTL_LMR_DISABLE_LOCAL_BUFFERING failed for network source file: " << op->SourceName << ", error: " << GetErrorText(err));
    if ((op->OpFlags & OPFL_TGTPATH_IS_NET) && !DisableLocalBuffering(asyncPar, out, &err))
        TRACE_E("DoCopyFileLoopAsync(): IOCTL_LMR_DISABLE_LOCAL_BUFFERING failed for network target file: " << op->TargetName << ", error: " << GetErrorText(err));

    // parameters of the copying loop
    int numOfBlocks = 8;

    // Context of the Copy operation (prevents passing a lot of parameters to helper functions, now methods of the context)
    CCopy_Context ctx(asyncPar, numOfBlocks, &dlgData, op, hProgressDlg, &in, &out, wholeFileAllocated, script,
                      &operationDone, &totalDone, &lastTransferredFileSize);
    BOOL doCopy = TRUE;
    while (doCopy)
    {
        if (ctx.ForceOp != fopWriting && ctx.FreeBlocks > 0 && !ctx.ReadingDone && ctx.ReadingBlocks < (numOfBlocks + 1) / 2) // We will read concurrently up to half of the block at most
        {
            DWORD toRead = ctx.ReadOffset + CQuadWord(limitBufferSize, 0) <= fileSize ? limitBufferSize : (fileSize - ctx.ReadOffset).LoDWord;
            BOOL testEOF = toRead == 0;
            if (!testEOF || ctx.ReadingBlocks == 0) // reading data or testing EOF (testing EOF is done only when all readings are completed)
            {
                if (ctx.BlockState[ctx.FreeBlockIndex] != cbsFree)
                    ctx.FreeBlockIndex = ctx.FindBlock(cbsFree);
                // test if EOF is read, reading into a whole block, otherwise normal reading 'toRead'
                if (ctx.StartReading(ctx.FreeBlockIndex, testEOF ? limitBufferSize : toRead, &err, testEOF))
                    continue; // success (start of asynchronous reading), let's try to start another reading
                else
                { // error (start of asynchronous reading)
                    if (!ctx.HandleReadingErr(-1, err, &copyError, &skipCopy, &copyAgain))
                        return; // cancel/skip(skip-all)/retry-complete
                    continue;   // retry-resume
                }
            }
        }
        // Reading is already requested or not needed, let's find out if something is unfinished
        BOOL shouldWait = TRUE; // TRUE = there is no more asynchronous task to submit, we have to wait for the completion of some requested operation
        BOOL retryCopy = FALSE; // TRUE = retry should be performed after an error = start fresh from the beginning of the "doCopy" cycle
        // Two wheels are needed only in case of synchronous writing (we want to mark it as
        // completed immediately and not after the next reading, just for the sake of progress)
        for (int afterWriting = 0; afterWriting < 2; afterWriting++)
        {
            for (int i = 0; i < _countof(ctx.BlockState); i++)
            {
                if (ctx.BlockState[i] > cbsInProgress && HasOverlappedIoCompleted(asyncPar->GetOverlapped(i)))
                {
                    shouldWait = FALSE; // in the spirit of "keep it simple" (there are situations where it could remain TRUE, but we ignore them)
                    switch (ctx.BlockState[i])
                    {
                    case cbsReading:    // reading source file into block - in progress
                    case cbsTestingEOF: // testing end of source file
                    {
                        BOOL testingEOF = ctx.BlockState[i] == cbsTestingEOF;

#ifdef ASYNC_COPY_DEBUG_MSG
                        TRACE_I("READ done: " << i);
#endif // ASYNC_COPY_DEBUG_MSG

                        BOOL res = GetOverlappedResult(in, asyncPar->GetOverlapped(i), &bytes, TRUE);
                        if (testingEOF && res && bytes == 0)
                        {
                            res = FALSE; // According to MSDN, it should return FALSE and ERROR_HANDLE_EOF at EOF, so we will tweak it (on Novell Netware 6.5 disk it returns TRUE)
                            SetLastError(ERROR_HANDLE_EOF);
                        }
                        if (res || GetLastError() == ERROR_HANDLE_EOF)
                        {
                            ctx.AutoRetryAttemptsSNAP = 0;
                            if (!res) // EOF at the beginning of the block (only cbsReading: EOF can also be before this block, which will be resolved by finding EOF later in the block with a lower offset if necessary)
                            {
                                // When GetOverlappedResult() returns FALSE, it may not return bytes==0 (TRACE_C was here for that)
                                // and failures have occurred), so we explicitly reset the bytes
                                bytes = 0;
                                if (testingEOF)
                                    ctx.ReadingDone = TRUE; // Confirmed end of source file, we do not read anything further
                                // We must not force fopWriting (we haven't read anything, we have nothing to write) unless it's an EOF test,
                                // Let the remaining asynchronous reads finish, then perform the EOF test, and then only writing will be done
                                ctx.ForceOp = fopNotUsed;
                            }
                            if (bytes < ctx.BlockDataLen[i]) // the file is shorter than we expected -> we will set a new file size
                            {
                                if (!testingEOF || bytes != 0)
                                    ctx.ReadOffset = fileSize = ctx.BlockOffset[i] + CQuadWord(bytes, 0);
                                if (!testingEOF)
                                    ctx.DiscardBlocksBehindEOF(fileSize, i);
                                if (bytes == 0) // EOF = no data, release block
                                {
                                    ctx.FreeBlock(i);
                                    if (testingEOF)
                                        doCopy = !ctx.IsOperationDone(numOfBlocks); // Let's test if we have finished copying this way
                                }
                                else
                                    ctx.BlockDataLen[i] = bytes; // we are still pretending that we wanted to load exactly that much
                            }
                            else
                            {
                                if (testingEOF) // we were looking for EOF and a full block was read, there must have been a significant increase in the file, we will determine the new size
                                {
                                    ctx.ReadOffset = ctx.BlockOffset[i] + CQuadWord(bytes, 0);
                                    ctx.GetNewFileSize(op->SourceName, in, &fileSize, ctx.ReadOffset);
                                }
                            }
                            if (ctx.BlockState[i] == cbsReading || ctx.BlockState[i] == cbsTestingEOF)
                            {
                                ctx.ReadingBlocks--;
                                ctx.BlockState[i] = cbsRead;
                            }
                        }
                        else // error
                        {
                            if (!ctx.HandleReadingErr(i, GetLastError(), &copyError, &skipCopy, &copyAgain))
                                return;       // cancel/skip(skip-all)/retry-complete
                            retryCopy = TRUE; // retry-resume
                        }
                        break;
                    }

                    case cbsWriting: // write block to target file
                    {
#ifdef ASYNC_COPY_DEBUG_MSG
                        TRACE_I("WRITE done: " << i);
#endif // ASYNC_COPY_DEBUG_MSG

                        BOOL res = GetOverlappedResult(out, asyncPar->GetOverlapped(i), &bytes, TRUE);
                        if (!res || bytes != ctx.BlockDataLen[i]) // error
                        {
                            err = GetLastError();
                            if (err == NO_ERROR && bytes != ctx.BlockDataLen[i])
                                err = ERROR_DISK_FULL;
                            CQuadWord maxWriteOffset = ctx.WriteOffset;
                            if (!ctx.HandleWritingErr(i, err, &copyError, &skipCopy, &copyAgain, allocFileSize, maxWriteOffset))
                                return;       // cancel/skip(skip-all)/retry-complete
                            retryCopy = TRUE; // retry-resume
                            break;
                        }

                        if (ctx.HandleSuspModeAndCancel(&copyError))
                            return; // cancel

                        script->AddBytesToSpeedMetersAndTFSandPS(bytes, FALSE, bufferSize, &limitBufferSize);

                        if (!script->ChangeSpeedLimit)                                 // if the speed limit can change, this is not a "suitable" place to wait
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                        operationDone += CQuadWord(bytes, 0);
                        SetProgressWithoutSuspend(hProgressDlg, CaclProg(operationDone, op->Size),
                                                  CaclProg(totalDone + operationDone, script->TotalSize), dlgData);

                        if (script->ChangeSpeedLimit)                                  // the speed limit may change, here is a "suitable" place to wait until
                        {                                                              // worker starts again, we will obtain the buffer size for copying again
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                            script->GetNewBufSize(&limitBufferSize, bufferSize);
                        }

                        // break; // break is not missing here...
                    }
                    case cbsDiscarded: // Reading from the source file past the end of the file (should return only an error: EOF)
                    {
                        ctx.FreeBlock(i);
                        doCopy = !ctx.IsOperationDone(numOfBlocks);
                        break;
                    }
                    }
                }
                if (!doCopy || retryCopy)
                    break;
            }
            if (!doCopy || retryCopy)
                break;

            // We have loaded the data into a block, let's see if they can be written to the target file;
            // We have released the written/discarded blocks (we will go back to read from them at the beginning of the loop)
            CQuadWord nextReadBlkOffset; // lowest offset of the skipped cbsRead block
            do
            {
                nextReadBlkOffset.SetUI64(0);
                // we will write concurrently up to half of the block
                for (int i = 0; ctx.ForceOp != fopReading && i < _countof(ctx.BlockState) && ctx.WritingBlocks < (numOfBlocks + 1) / 2; i++)
                {
                    if (ctx.BlockState[i] == cbsRead)
                    {
                        if (ctx.WriteOffset == ctx.BlockOffset[i])
                        {
                            if (!ctx.StartWriting(i, &err))
                            { // error (asynchronous write)
                                CQuadWord maxWriteOffset = ctx.WriteOffset + CQuadWord(ctx.BlockDataLen[i], 0);
                                if (!ctx.HandleWritingErr(i, err, &copyError, &skipCopy, &copyAgain, allocFileSize, maxWriteOffset))
                                    return;       // cancel/skip(skip-all)/retry-complete
                                retryCopy = TRUE; // retry-resume
                                break;
                            }
                        }
                        else
                        {
                            if (nextReadBlkOffset.Value == 0 || ctx.BlockOffset[i] < nextReadBlkOffset)
                                nextReadBlkOffset = ctx.BlockOffset[i];
                        }
                    }
                } // we have another cbsRead block and it continues from the written part of the target file -> we continue writing
            } while (!retryCopy && ctx.ForceOp != fopReading && nextReadBlkOffset.Value != 0 && nextReadBlkOffset == ctx.WriteOffset &&
                     ctx.WritingBlocks < (numOfBlocks + 1) / 2); // we will write concurrently up to half of the block
            if (retryCopy || ctx.ForceOp != fopReading)
                break; // we go to Retry or the write was not synchronous (thus it was done in about 0 ms) or we are just writing, in any case, two rounds are unnecessary
        }
        if (!doCopy || retryCopy)
            continue;

        if (shouldWait) // Another cycle pass makes no sense, there is no hope for starting a new read or write, let's wait
        {               // to complete the oldest asynchronous operation
            DWORD oldestBlockTime = 0;
            int oldestBlockIndex = -1;
            for (int i = 0; i < _countof(ctx.BlockState); i++)
            {
                if (ctx.BlockState[i] > cbsInProgress)
                {
                    DWORD ti = ctx.CurTime - ctx.BlockTime[i];
                    if (oldestBlockTime < ti)
                    {
                        oldestBlockTime = ti;
                        oldestBlockIndex = i;
                    }
                }
            }
            if (oldestBlockIndex == -1)
                TRACE_C("DoCopyFileLoopAsync(): unexpected situation: unable to find any block with operation in progress!");

#ifdef ASYNC_COPY_DEBUG_MSG
            TRACE_I("wait: GetOverlappedResult: " << oldestBlockIndex << (ctx.BlockState[oldestBlockIndex] == cbsWriting ? " WRITE" : " READ"));
#endif // ASYNC_COPY_DEBUG_MSG

            // here we wait for the oldest requested ongoing asynchronous operation to finish
            // source file ('in') concerns: cbsReading, cbsTestingEOF, and cbsDiscarded
            // the target file ('out') is only relevant to cbsWriting
            GetOverlappedResult(ctx.BlockState[oldestBlockIndex] == cbsWriting ? out : in,
                                asyncPar->GetOverlapped(oldestBlockIndex), &bytes, TRUE);

#ifdef ASYNC_COPY_DEBUG_MSG
            char sss[1000];
            sprintf(sss, "wait done: 0x%08X 0x%08X", ctx.BlockOffset[oldestBlockIndex].LoDWord, bytes);
            TRACE_I(sss);
#endif // ASYNC_COPY_DEBUG_MSG

            if (ctx.HandleSuspModeAndCancel(&copyError))
                return; // cancel
        }
    }
    if (ctx.ReadOffset != ctx.WriteOffset || operationDone != ctx.WriteOffset)
        TRACE_C("DoCopyFileLoopAsync(): unexpected situation after copy: ReadOffset != WriteOffset || operationDone != ctx.WriteOffset");

    if (wholeFileAllocated) // We allocated the complete structure of the file (meaning that the allocation made sense, for example, the file cannot be null)
    {
        if (operationDone < allocFileSize) // and the source file has been reduced, we need to shorten it here
        {
            while (1)
            {
                CQuadWord off = ctx.WriteOffset;
                off.LoDWord = SetFilePointer(out, off.LoDWord, (LONG*)&(off.HiDWord), FILE_BEGIN);
                if (off.LoDWord == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ||
                    off != ctx.WriteOffset ||
                    !SetEndOfFile(out))
                {
                    DWORD err2 = GetLastError();
                    if ((off.LoDWord != INVALID_SET_FILE_POINTER || err2 == NO_ERROR) && off != ctx.WriteOffset)
                        err2 = ERROR_INVALID_FUNCTION; // Successful SetFilePointer, but off != ctx.WriteOffset: probably never happens, just for completeness
                    if (!ctx.HandleWritingErr(-1, err2, &copyError, &skipCopy, &copyAgain, allocFileSize, CQuadWord(0, 0)))
                        return; // cancel/skip(skip-all)/retry-complete
                                // retry-resume
                }
                else
                    break; // success
            }
        }

        if (allocWholeFileOnStart == 0 /* need-test*/)
        {
            CQuadWord curFileSize;
            curFileSize.LoDWord = GetFileSize(out, &curFileSize.HiDWord);
            BOOL getFileSizeSuccess = (curFileSize.LoDWord != INVALID_FILE_SIZE || GetLastError() == NO_ERROR);
            if (getFileSizeSuccess && curFileSize == operationDone)
            { // check if no writable bytes have been added to the end of the file + if we can truncate the file
                allocWholeFileOnStart = 1 /* yes*/;
            }
            else
            {
#ifdef _DEBUG
                if (getFileSizeSuccess)
                {
                    char num1[50];
                    char num2[50];
                    TRACE_E("DoCopyFileLoopAsync(): unable to allocate whole file size before copy operation, please report "
                            "under what conditions this occurs! Error: different file sizes: target="
                            << NumberToStr(num1, curFileSize) << " bytes, source=" << NumberToStr(num2, operationDone) << " bytes");
                }
                else
                {
                    DWORD err2 = GetLastError();
                    TRACE_E("DoCopyFileLoopAsync(): unable to test result of allocation of whole file size before copy operation, please report "
                            "under what conditions this occurs! GetFileSize("
                            << op->TargetName << ") error: " << GetErrorText(err2));
                }
#endif
                allocWholeFileOnStart = 2 /* no*/; // We will refrain from further attempts on this target disk

                while (1)
                {
                    HANDLES(CloseHandle(out));
                    out = NULL;
                    ClearReadOnlyAttr(op->TargetName); // If it were to occur as read-only (it should never happen), we would handle it.
                    if (DeleteFile(op->TargetName))
                    {
                        HANDLES(CloseHandle(in));
                        copyAgain = TRUE; // goto COPY_AGAIN;
                        return;
                    }
                    else
                    {
                        if (!ctx.HandleWritingErr(-1, GetLastError(), &copyError, &skipCopy, &copyAgain, allocFileSize, CQuadWord(0, 0)))
                            return; // cancel/skip(skip-all)/retry-complete
                                    // retry-resume
                    }
                }
            }
        }
    }
}

BOOL DoCopyFile(COperation* op, HWND hProgressDlg, void* buffer,
                COperations* script, CQuadWord& totalDone,
                DWORD clearReadonlyMask, BOOL* skip, BOOL lantasticCheck,
                int& mustDeleteFileBeforeOverwrite, int& allocWholeFileOnStart,
                CProgressDlgData& dlgData, BOOL copyADS, BOOL copyAsEncrypted,
                BOOL isMove, CAsyncCopyParams*& asyncPar)
{
    if (script->CopyAttrs && copyAsEncrypted)
        TRACE_E("DoCopyFile(): unexpected parameter value: copyAsEncrypted is TRUE when script->CopyAttrs is TRUE!");

    // if the path ends with a space/dot, it is invalid and we must not make a copy,
    // CreateFile trims spaces/dots and would thus copy another file possibly to a different name
    BOOL invalidSrcName = FileNameIsInvalid(op->SourceName, TRUE);
    BOOL invalidTgtName = FileNameIsInvalid(op->TargetName, TRUE);

    // optimization: roughly 4x speedup skipping all "older and same" files,
    // Slowdown if the file is newer by 5%, so I think it's highly worth it
    // (it can be assumed that the "Overwrite Older" user will enable it if there are any skips)
    BOOL tgtNameCaseCorrected = FALSE; // TRUE = the letter case in the target name has already been adjusted according to the existing target file (to prevent a change in letter case during overwriting)
    WIN32_FIND_DATA dataIn, dataOut;
    if ((op->OpFlags & OPFL_OVERWROLDERALRTESTED) == 0 &&
        !invalidSrcName && !invalidTgtName && script->OverwriteOlder)
    {
        HANDLE find;
        find = HANDLES_Q(FindFirstFile(op->TargetName, &dataOut));
        if (find != INVALID_HANDLE_VALUE)
        {
            HANDLES(FindClose(find));

            CorrectCaseOfTgtName(op->TargetName, TRUE, &dataOut);
            tgtNameCaseCorrected = TRUE;

            const char* tgtName = SalPathFindFileName(op->TargetName);
            if (StrICmp(tgtName, dataOut.cFileName) == 0 &&                 // if it's not just about matching the DOS name (there will be a change in the DOS name, not just an overwrite)
                (dataOut.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) // if it's not a directory (the overwrite-older won't change anything)
            {
                find = HANDLES_Q(FindFirstFile(op->SourceName, &dataIn));
                if (find != INVALID_HANDLE_VALUE)
                {
                    HANDLES(FindClose(find));

                    // We will truncate times to seconds (different file systems store times with different precisions, so there were "differences" even between "identical" times)
                    *(unsigned __int64*)&dataIn.ftLastWriteTime = *(unsigned __int64*)&dataIn.ftLastWriteTime - (*(unsigned __int64*)&dataIn.ftLastWriteTime % 10000000);
                    *(unsigned __int64*)&dataOut.ftLastWriteTime = *(unsigned __int64*)&dataOut.ftLastWriteTime - (*(unsigned __int64*)&dataOut.ftLastWriteTime % 10000000);

                    if ((dataIn.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&             // check if the source is still a file
                        CompareFileTime(&dataIn.ftLastWriteTime, &dataOut.ftLastWriteTime) <= 0) // source file is not newer than the target file - skipping the copy operation
                    {
                        CQuadWord fileSize(op->FileSize);
                        if (fileSize < COPY_MIN_FILE_SIZE)
                        {
                            if (op->Size > COPY_MIN_FILE_SIZE)             // should always be at least COPY_MIN_FILE_SIZE, but we compromise...
                                fileSize += op->Size - COPY_MIN_FILE_SIZE; // add the size of ADSek
                        }
                        else
                            fileSize = op->Size;
                        totalDone += op->Size;
                        script->AddBytesToTFSandSetProgressSize(fileSize, totalDone);

                        SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                        if (skip != NULL)
                            *skip = TRUE;
                        return TRUE;
                    }
                }
            }
        }
    }

    // we will decide which algorithm we will use for copying: conventional old synchronous or
    // asynchronously watched from the Windows 7 version of CopyFileEx:
    // -under the Vistula it was bad, we don't care about it, she almost died anyway, besides
    //  When using the old algorithm against Win7 over the network, I did not notice any speed
    //  difference for upload, we were 15% slower for download (which is acceptable)
    // Asynchronous algorithms only make sense over a network + if we have a fast or network source/target
    // -with the old algous, copying in Win7 over the network is easily 2x-3x slower in one direction
    //  download, almost 2 times slower upload and about 30% slower network-network
    BOOL useAsyncAlg = Windows7AndLater && Configuration.UseAsyncCopyAlg &&
                       op->FileSize.Value > 0 && // empty files are processed synchronously (no data)
                       ((op->OpFlags & OPFL_SRCPATH_IS_NET) && ((op->OpFlags & OPFL_TGTPATH_IS_NET) ||
                                                                (op->OpFlags & OPFL_TGTPATH_IS_FAST)) ||
                        (op->OpFlags & OPFL_TGTPATH_IS_NET) && (op->OpFlags & OPFL_SRCPATH_IS_FAST));

    if (asyncPar == NULL)
        asyncPar = new CAsyncCopyParams;

    asyncPar->Init(useAsyncAlg);
    script->EnableProgressBufferLimit(useAsyncAlg);
    struct CDisableProgressBufferLimit // we will ensure calling Script->EnableProgressBufferLimit(FALSE) on all exits from this function
    {
        COperations* Script;
        CDisableProgressBufferLimit(COperations* script) { Script = script; }
        ~CDisableProgressBufferLimit() { Script->EnableProgressBufferLimit(FALSE); }
    } DisableProgressBufferLimit(script);

    CQuadWord operationDone;
    CQuadWord lastTransferredFileSize;
    script->GetTFSandResetTrSpeedIfNeeded(&lastTransferredFileSize);

COPY_AGAIN:

    operationDone = CQuadWord(0, 0);
    HANDLE in;

    if (skip != NULL)
        *skip = FALSE;

    int bufferSize;
    if (useAsyncAlg)
    {
        if (op->FileSize.Value <= 512 * 1024)
            bufferSize = ASYNC_COPY_BUF_SIZE_512KB;
        else if (op->FileSize.Value <= 2 * 1024 * 1024)
            bufferSize = ASYNC_COPY_BUF_SIZE_2MB;
        else if (op->FileSize.Value <= 8 * 1024 * 1024)
            bufferSize = ASYNC_COPY_BUF_SIZE_8MB;
        else
            bufferSize = ASYNC_COPY_BUF_SIZE;
    }
    else
        bufferSize = script->RemovableSrcDisk || script->RemovableTgtDisk ? REMOVABLE_DISK_COPY_BUFFER : OPERATION_BUFFER;

    int limitBufferSize = bufferSize;
    script->SetTFSandProgressSize(lastTransferredFileSize, totalDone, &limitBufferSize, bufferSize);

    while (1)
    {
        if (!invalidSrcName && !asyncPar->Failed())
        {
            in = HANDLES_Q(CreateFile(op->SourceName, GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                      OPEN_EXISTING, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
        }
        else
        {
            in = INVALID_HANDLE_VALUE;
        }
        if (in != INVALID_HANDLE_VALUE)
        {
            CQuadWord fileSize = op->FileSize;

            HANDLE out;
            BOOL lossEncryptionAttr = FALSE;
            BOOL skipAllocWholeFileOnStart = FALSE;
            while (1)
            {
            OPEN_TGT_FILE:

                BOOL encryptionNotSupported = FALSE;
                DWORD fileAttrs = asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN |
                                  (!lossEncryptionAttr && copyAsEncrypted ? FILE_ATTRIBUTE_ENCRYPTED : 0) |
                                  (script->CopyAttrs ? (op->Attr & (FILE_ATTRIBUTE_COMPRESSED | (lossEncryptionAttr ? 0 : FILE_ATTRIBUTE_ENCRYPTED))) : 0);
                if (!invalidTgtName)
                {
                    // GENERIC_READ for 'out' causes slowdown in asynchronous copying from disk to network (on Win7 x64 GLAN measured 95MB/s instead of 111MB/s)
                    out = SalCreateFileEx(op->TargetName, GENERIC_WRITE | (script->CopyAttrs ? GENERIC_READ : 0), 0, fileAttrs, &encryptionNotSupported);
                    if (!encryptionNotSupported && script->CopyAttrs && out == INVALID_HANDLE_VALUE) // in case read-access to the directory is not allowed (which we added only for setting the Compressed attribute), we will try to create a file just for writing
                        out = SalCreateFileEx(op->TargetName, GENERIC_WRITE, 0, fileAttrs, &encryptionNotSupported);

                    if (out == INVALID_HANDLE_VALUE && encryptionNotSupported && dlgData.FileOutLossEncrAll && !lossEncryptionAttr)
                    { // The user has agreed to the loss of the Encrypted attribute for all problematic files, so we will handle this loss.
                        lossEncryptionAttr = TRUE;
                        continue;
                    }
                    HANDLES_ADD_EX(__otQuiet, out != INVALID_HANDLE_VALUE, __htFile,
                                   __hoCreateFile, out, GetLastError(), TRUE);
                    if (script->CopyAttrs)
                    {
                        fileAttrs = lossEncryptionAttr ? (op->Attr & ~FILE_ATTRIBUTE_ENCRYPTED) : op->Attr;
                        SetCompressAndEncryptedAttrs(op->TargetName, fileAttrs, &out, TRUE, NULL, asyncPar);
                    }

                    if (out != INVALID_HANDLE_VALUE && (fileAttrs & FILE_ATTRIBUTE_ENCRYPTED))
                    { // we will check if Encrypted is actually set (for example, on FAT it is simply ignored, the system does not return an error (specifically for CreateFile))
                        DWORD attrs;
                        attrs = SalGetFileAttributes(op->TargetName);
                        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_ENCRYPTED) == 0)
                        { // Unable to set the Encrypted attribute, let's ask the user what to do...
                            if (dlgData.FileOutLossEncrAll)
                                lossEncryptionAttr = TRUE;
                            else
                            {
                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                if (*dlgData.CancelWorker)
                                    goto CANCEL_ENCNOTSUP;

                                if (dlgData.SkipAllFileOutLossEncr)
                                    goto SKIP_ENCNOTSUP;

                                int ret;
                                ret = IDCANCEL;
                                char* data[4];
                                data[0] = (char*)&ret;
                                data[1] = (char*)TRUE;
                                data[2] = op->TargetName;
                                data[3] = (char*)(INT_PTR)isMove;
                                SendMessage(hProgressDlg, WM_USER_DIALOG, 12, (LPARAM)data);
                                switch (ret)
                                {
                                case IDB_ALL:
                                    dlgData.FileOutLossEncrAll = TRUE; // here break; is not missing
                                case IDYES:
                                    lossEncryptionAttr = TRUE;
                                    break;

                                case IDB_SKIPALL:
                                    dlgData.SkipAllFileOutLossEncr = TRUE;
                                case IDB_SKIP:
                                {
                                SKIP_ENCNOTSUP:

                                    HANDLES(CloseHandle(out));
                                    DeleteFile(op->TargetName);
                                    goto SKIP_OPEN_OUT;
                                }

                                case IDCANCEL:
                                {
                                CANCEL_ENCNOTSUP:

                                    HANDLES(CloseHandle(out));
                                    DeleteFile(op->TargetName);
                                    goto CANCEL_OPEN2;
                                }
                                }
                            }
                        }
                    }
                }
                else
                {
                    out = INVALID_HANDLE_VALUE;
                }

                if (out != INVALID_HANDLE_VALUE)
                {

                COPY:

                    // if possible, we will allocate the necessary space for the file (to prevent disk fragmentation + smoother writing to floppy disks)
                    BOOL wholeFileAllocated = FALSE;
                    if (!skipAllocWholeFileOnStart &&               // last time there was an error, now it would probably be the same
                        allocWholeFileOnStart != 2 /* no*/ &&      // allocating the entire file is not prohibited
                        fileSize > CQuadWord(limitBufferSize, 0) && // it doesn't make sense to allocate a file under the size of the copy buffer
                        fileSize < CQuadWord(0, 0x80000000))        // file size is a positive number (otherwise seeking is not possible - these are numbers above 8EB, so it will probably never happen)
                    {
                        BOOL fatal = TRUE;
                        BOOL ignoreErr = FALSE;
                        if (SalSetFilePointer(out, fileSize))
                        {
                            if (SetEndOfFile(out))
                            {
                                if (SetFilePointer(out, 0, NULL, FILE_BEGIN) == 0)
                                {
                                    fatal = FALSE;
                                    wholeFileAllocated = TRUE;
                                }
                            }
                            else
                            {
                                if (GetLastError() == ERROR_DISK_FULL)
                                    ignoreErr = TRUE; // low disk space
                            }
                        }
                        if (fatal)
                        {
                            if (!ignoreErr)
                            {
                                DWORD err = GetLastError();
                                TRACE_E("DoCopyFile(): unable to allocate whole file size before copy operation, please report under what conditions this occurs! GetLastError(): " << GetErrorText(err));
                                allocWholeFileOnStart = 2 /* no*/; // We will refrain from further attempts on this target disk
                            }

                            // Let's try to truncate the file to zero to avoid any unnecessary writes when closing the file
                            SetFilePointer(out, 0, NULL, FILE_BEGIN);
                            SetEndOfFile(out);

                            HANDLES(CloseHandle(out));
                            out = INVALID_HANDLE_VALUE;
                            ClearReadOnlyAttr(op->TargetName); // If it were to occur as read-only (it should never happen), we would handle it.
                            if (DeleteFile(op->TargetName))
                            {
                                skipAllocWholeFileOnStart = TRUE;
                                goto OPEN_TGT_FILE;
                            }
                            else
                                goto CREATE_ERROR;
                        }
                    }

                    script->SetFileStartParams();

                    BOOL copyError = FALSE;
                    BOOL skipCopy = FALSE;
                    BOOL copyAgain = FALSE;
                    if (useAsyncAlg)
                    {
                        DoCopyFileLoopAsync(asyncPar, in, out, buffer, limitBufferSize, script, dlgData, wholeFileAllocated, op,
                                            totalDone, copyError, skipCopy, hProgressDlg, operationDone, fileSize,
                                            bufferSize, allocWholeFileOnStart, copyAgain, lastTransferredFileSize);
                        // WARNING: 'in' and 'out' do not have the file-pointer set to the end of the file,
                        //        respectively 'out' is set only when (copyError || skipCopy)
                    }
                    else
                    {
                        DoCopyFileLoopOrig(in, out, buffer, limitBufferSize, script, dlgData, wholeFileAllocated, op,
                                           totalDone, copyError, skipCopy, hProgressDlg, operationDone, fileSize,
                                           bufferSize, allocWholeFileOnStart, copyAgain);
                    }

                    if (copyError)
                    {
                    COPY_ERROR:

                        if (in != NULL)
                            HANDLES(CloseHandle(in));
                        if (out != NULL)
                        {
                            if (wholeFileAllocated)
                                SetEndOfFile(out); // Otherwise, the remainder of the file would be written to the floppy disk differently
                            HANDLES(CloseHandle(out));
                        }
                        DeleteFile(op->TargetName);
                        return FALSE;
                    }
                    if (skipCopy)
                    {
                    SKIP_COPY:

                        totalDone += op->Size;
                        SetTFSandPSforSkippedFile(op, lastTransferredFileSize, script, totalDone);

                        if (in != NULL)
                            HANDLES(CloseHandle(in));
                        if (out != NULL)
                        {
                            if (wholeFileAllocated)
                                SetEndOfFile(out); // Otherwise, the remainder of the file would be written to the floppy disk differently
                            HANDLES(CloseHandle(out));
                        }
                        DeleteFile(op->TargetName);
                        SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                        if (skip != NULL)
                            *skip = TRUE;
                        return TRUE;
                    }
                    if (copyAgain)
                        goto COPY_AGAIN;

                    if (lantasticCheck)
                    {
                        CQuadWord inSize, outSize;
                        inSize.LoDWord = GetFileSize(in, &inSize.HiDWord);
                        outSize.LoDWord = GetFileSize(out, &outSize.HiDWord);
                        if (inSize != outSize)
                        {                                                              // Lantastic 7.0, seemingly everything without problems, but the result is wrong
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                            if (*dlgData.CancelWorker)
                                goto COPY_ERROR;

                            if (dlgData.SkipAllFileWrite)
                                goto SKIP_COPY;

                            int ret = IDCANCEL;
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = LoadStr(IDS_ERRORWRITINGFILE);
                            data[2] = op->TargetName;
                            data[3] = GetErrorText(ERROR_DISK_FULL);
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                            switch (ret)
                            {
                            case IDRETRY:
                            {
                                operationDone = CQuadWord(0, 0);
                                script->SetTFSandProgressSize(lastTransferredFileSize, totalDone);
                                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                                SetFilePointer(in, 0, NULL, FILE_BEGIN);  // reading again
                                SetFilePointer(out, 0, NULL, FILE_BEGIN); // writing again
                                SetEndOfFile(out);                        // create output file
                                goto COPY;
                            }

                            case IDB_SKIPALL:
                                dlgData.SkipAllFileWrite = TRUE;
                            case IDB_SKIP:
                                goto SKIP_COPY;

                            case IDCANCEL:
                                goto COPY_ERROR;
                            }
                        }
                    }

                    FILETIME /*creation, lastAccess,*/ lastWrite;
                    BOOL ignoreGetFileTimeErr = FALSE;
                    while (!ignoreGetFileTimeErr &&
                           !GetFileTime(in, NULL /*&creation*/, NULL /*&lastAccess*/, &lastWrite))
                    {
                        DWORD err = GetLastError();

                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                        if (*dlgData.CancelWorker)
                            goto COPY_ERROR;

                        if (dlgData.SkipAllGetFileTime)
                            goto SKIP_COPY;

                        if (dlgData.IgnoreAllGetFileTimeErr)
                            goto IGNORE_GETFILETIME;

                        int ret;
                        ret = IDCANCEL;
                        char* data[4];
                        data[0] = (char*)&ret;
                        data[1] = LoadStr(IDS_ERRORGETTINGFILETIME);
                        data[2] = op->SourceName;
                        data[3] = GetErrorText(err);
                        SendMessage(hProgressDlg, WM_USER_DIALOG, 8, (LPARAM)data);
                        switch (ret)
                        {
                        case IDRETRY:
                            break;

                        case IDB_IGNOREALL:
                            dlgData.IgnoreAllGetFileTimeErr = TRUE; // here break; is not missing
                        case IDB_IGNORE:
                        {
                        IGNORE_GETFILETIME:

                            ignoreGetFileTimeErr = TRUE;
                            break;
                        }

                        case IDB_SKIPALL:
                            dlgData.SkipAllGetFileTime = TRUE;
                        case IDB_SKIP:
                            goto SKIP_COPY;

                        case IDCANCEL:
                            goto COPY_ERROR;
                        }
                    }

                    HANDLES(CloseHandle(in));
                    in = NULL;

                    if (operationDone < COPY_MIN_FILE_SIZE) // Zero/Small files take at least as long as files with size COPY_MIN_FILE_SIZE
                        script->AddBytesToSpeedMetersAndTFSandPS((DWORD)(COPY_MIN_FILE_SIZE - operationDone).Value, TRUE, 0, NULL, MAX_OP_FILESIZE);

                    DWORD attr = op->Attr & clearReadonlyMask;
                    if (copyADS) // if it is necessary to copy ADS, we will do it
                    {
                        SetFileAttributes(op->TargetName, FILE_ATTRIBUTE_ARCHIVE); // Probably unnecessary, doesn't really slow down copying itself, reason: in order to work with the file, it must not have the read-only attribute
                        CQuadWord operDone = operationDone;                        // the file has been copied
                        if (operDone < COPY_MIN_FILE_SIZE)
                            operDone = COPY_MIN_FILE_SIZE; // Zero/Small files take at least as long as files with size COPY_MIN_FILE_SIZE
                        BOOL adsSkip = FALSE;
                        if (!DoCopyADS(hProgressDlg, op->SourceName, FALSE, op->TargetName, totalDone,
                                       operDone, op->Size, dlgData, script, &adsSkip, buffer) ||
                            adsSkip) // user either clicked cancel or skipped at least one ad
                        {
                            if (out != NULL)
                                HANDLES(CloseHandle(out));
                            out = NULL;
                            if (DeleteFile(op->TargetName) == 0)
                            {
                                DWORD err = GetLastError();
                                TRACE_E("DoCopyFile(): Unable to remove newly created file: " << op->TargetName << ", error: " << GetErrorText(err));
                            }
                            if (!adsSkip)
                                return FALSE; // cancel the whole operation
                            if (skip != NULL)
                                *skip = TRUE; // jde o Skip, musime hlasit vyse (Move operace nesmi smazat zdrojovy soubor)
                        }
                    }

                    if (out != NULL)
                    {
                        if (!ignoreGetFileTimeErr) // only if we did not ignore the error of reading the file time (there is nothing to set)
                        {
                            BOOL ignoreSetFileTimeErr = FALSE;
                            while (!ignoreSetFileTimeErr &&
                                   !SetFileTime(out, NULL /*&creation*/, NULL /*&lastAccess*/, &lastWrite))
                            {
                                DWORD err = GetLastError();

                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                if (*dlgData.CancelWorker)
                                    goto COPY_ERROR;

                                if (dlgData.SkipAllSetFileTime)
                                    goto SKIP_COPY;

                                if (dlgData.IgnoreAllSetFileTimeErr)
                                    goto IGNORE_SETFILETIME;

                                int ret;
                                ret = IDCANCEL;
                                char* data[4];
                                data[0] = (char*)&ret;
                                data[1] = LoadStr(IDS_ERRORSETTINGFILETIME);
                                data[2] = op->TargetName;
                                data[3] = GetErrorText(err);
                                SendMessage(hProgressDlg, WM_USER_DIALOG, 8, (LPARAM)data);
                                switch (ret)
                                {
                                case IDRETRY:
                                    break;

                                case IDB_IGNOREALL:
                                    dlgData.IgnoreAllSetFileTimeErr = TRUE; // here break; is not missing
                                case IDB_IGNORE:
                                {
                                IGNORE_SETFILETIME:

                                    ignoreSetFileTimeErr = TRUE;
                                    break;
                                }

                                case IDB_SKIPALL:
                                    dlgData.SkipAllSetFileTime = TRUE;
                                case IDB_SKIP:
                                    goto SKIP_COPY;

                                case IDCANCEL:
                                    goto COPY_ERROR;
                                }
                            }
                        }
                        if (!HANDLES(CloseHandle(out)))
                        {
                            out = NULL;
                            DWORD err = GetLastError();
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                            if (*dlgData.CancelWorker)
                                goto COPY_ERROR;

                            if (dlgData.SkipAllFileWrite)
                                goto SKIP_COPY;

                            int ret = IDCANCEL;
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = LoadStr(IDS_ERRORWRITINGFILE);
                            data[2] = op->TargetName;
                            data[3] = GetErrorText(err);
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                            switch (ret)
                            {
                            case IDRETRY:
                            {
                                if (DeleteFile(op->TargetName) == 0)
                                {
                                    DWORD err2 = GetLastError();
                                    TRACE_E("DoCopyFile(): Unable to remove newly created file: " << op->TargetName << ", error: " << GetErrorText(err2));
                                }
                                goto COPY_AGAIN;
                            }

                            case IDB_SKIPALL:
                                dlgData.SkipAllFileWrite = TRUE;
                            case IDB_SKIP:
                                goto SKIP_COPY;

                            case IDCANCEL:
                                goto COPY_ERROR;
                            }
                        }

                        SetFileAttributes(op->TargetName, script->CopyAttrs ? attr : (attr | FILE_ATTRIBUTE_ARCHIVE));
                    }

                    if (script->CopyAttrs) // we will check if the attributes of the source file were preserved
                    {
                        DWORD curAttrs;
                        curAttrs = SalGetFileAttributes(op->TargetName);
                        if (curAttrs == INVALID_FILE_ATTRIBUTES || (curAttrs & DISPLAYED_ATTRIBUTES) != (attr & DISPLAYED_ATTRIBUTES))
                        {                                                              // Attributes probably failed to transfer, warning the user
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                            if (*dlgData.CancelWorker)
                                goto COPY_ERROR_2;

                            int ret;
                            ret = IDCANCEL;
                            if (dlgData.IgnoreAllSetAttrsErr)
                                ret = IDB_IGNORE;
                            else
                            {
                                char* data[4];
                                data[0] = (char*)&ret;
                                data[1] = op->TargetName;
                                data[2] = (char*)(DWORD_PTR)(attr & DISPLAYED_ATTRIBUTES);
                                data[3] = (char*)(DWORD_PTR)(curAttrs == INVALID_FILE_ATTRIBUTES ? 0 : (curAttrs & DISPLAYED_ATTRIBUTES));
                                SendMessage(hProgressDlg, WM_USER_DIALOG, 9, (LPARAM)data);
                            }
                            switch (ret)
                            {
                            case IDB_IGNOREALL:
                                dlgData.IgnoreAllSetAttrsErr = TRUE; // here break; is not missing
                            case IDB_IGNORE:
                                break;

                            case IDCANCEL:
                            {
                            COPY_ERROR_2:

                                ClearReadOnlyAttr(op->TargetName); // to be able to delete the file, it must not have the read-only attribute
                                DeleteFile(op->TargetName);
                                return FALSE;
                            }
                            }
                        }
                    }

                    if (script->CopySecurity) // Should we copy NTFS security permissions?
                    {
                        DWORD err;
                        if (!DoCopySecurity(op->SourceName, op->TargetName, &err, NULL))
                        {
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                            if (*dlgData.CancelWorker)
                                goto COPY_ERROR_2;

                            int ret;
                            ret = IDCANCEL;
                            if (dlgData.IgnoreAllCopyPermErr)
                                ret = IDB_IGNORE;
                            else
                            {
                                char* data[4];
                                data[0] = (char*)&ret;
                                data[1] = op->SourceName;
                                data[2] = op->TargetName;
                                data[3] = (char*)(DWORD_PTR)err;
                                SendMessage(hProgressDlg, WM_USER_DIALOG, 10, (LPARAM)data);
                            }
                            switch (ret)
                            {
                            case IDB_IGNOREALL:
                                dlgData.IgnoreAllCopyPermErr = TRUE; // here break; is not missing
                            case IDB_IGNORE:
                                break;

                            case IDCANCEL:
                                goto COPY_ERROR_2;
                            }
                        }
                    }

                    totalDone += op->Size;
                    script->SetProgressSize(totalDone);
                    return TRUE;
                }
                else
                {
                    if (!invalidTgtName && encryptionNotSupported)
                    {
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                        if (*dlgData.CancelWorker)
                            goto CANCEL_OPEN2;

                        if (dlgData.SkipAllFileOutLossEncr)
                            goto SKIP_OPEN_OUT;

                        int ret;
                        ret = IDCANCEL;
                        char* data[4];
                        data[0] = (char*)&ret;
                        data[1] = (char*)TRUE;
                        data[2] = op->TargetName;
                        data[3] = (char*)(INT_PTR)isMove;
                        SendMessage(hProgressDlg, WM_USER_DIALOG, 12, (LPARAM)data);
                        switch (ret)
                        {
                        case IDB_ALL:
                            dlgData.FileOutLossEncrAll = TRUE; // here break; is not missing
                        case IDYES:
                            lossEncryptionAttr = TRUE;
                            break;

                        case IDB_SKIPALL:
                            dlgData.SkipAllFileOutLossEncr = TRUE;
                        case IDB_SKIP:
                        {
                        SKIP_OPEN_OUT:

                            totalDone += op->Size;
                            SetTFSandPSforSkippedFile(op, lastTransferredFileSize, script, totalDone);

                            HANDLES(CloseHandle(in));
                            SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                            if (skip != NULL)
                                *skip = TRUE;
                            return TRUE;
                        }

                        case IDCANCEL:
                        {
                        CANCEL_OPEN2:

                            HANDLES(CloseHandle(in));
                            return FALSE;
                        }
                        }
                    }
                    else
                    {
                    CREATE_ERROR:

                        DWORD err = GetLastError();
                        if (invalidTgtName)
                            err = ERROR_INVALID_NAME;
                        BOOL errDeletingFile = FALSE;
                        if (err == ERROR_FILE_EXISTS || // overwrite file?
                            err == ERROR_ALREADY_EXISTS)
                        {
                            if (!dlgData.OverwriteAll && (dlgData.CnfrmFileOver || script->OverwriteOlder))
                            {
                                char sAttr[101], tAttr[101];
                                BOOL getTimeFailed;
                                getTimeFailed = FALSE;
                                FILETIME sFileTime, tFileTime;
                                GetFileOverwriteInfo(sAttr, _countof(sAttr), in, op->SourceName, &sFileTime, &getTimeFailed);
                                HANDLES(CloseHandle(in));
                                in = NULL;
                                out = HANDLES_Q(CreateFile(op->TargetName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                           OPEN_EXISTING, 0, NULL));
                                if (out != INVALID_HANDLE_VALUE)
                                {
                                    GetFileOverwriteInfo(tAttr, _countof(tAttr), out, op->TargetName, &tFileTime, &getTimeFailed);
                                    HANDLES(CloseHandle(out));
                                }
                                else
                                {
                                    getTimeFailed = TRUE;
                                    strcpy(tAttr, LoadStr(IDS_ERR_FILEOPEN));
                                }
                                out = NULL;

                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                if (*dlgData.CancelWorker)
                                    goto CANCEL_OPEN;

                                if (dlgData.SkipAllOverwrite)
                                    goto SKIP_OPEN;

                                int ret;
                                ret = IDCANCEL;

                                if (!getTimeFailed && script->OverwriteOlder) // option from the Copy/Move dialog
                                {
                                    // We will truncate times to seconds (different file systems store times with different precisions, so there were "differences" even between "identical" times)
                                    *(unsigned __int64*)&sFileTime = *(unsigned __int64*)&sFileTime - (*(unsigned __int64*)&sFileTime % 10000000);
                                    *(unsigned __int64*)&tFileTime = *(unsigned __int64*)&tFileTime - (*(unsigned __int64*)&tFileTime % 10000000);

                                    if (CompareFileTime(&sFileTime, &tFileTime) > 0)
                                        ret = IDYES; // older mother without asking to rewrite
                                    else
                                        ret = IDB_SKIP; // skip the remaining existing
                                }
                                else
                                {
                                    // display the query
                                    char* data[5];
                                    data[0] = (char*)&ret;
                                    data[1] = op->TargetName;
                                    data[2] = tAttr;
                                    data[3] = op->SourceName;
                                    data[4] = sAttr;
                                    SendMessage(hProgressDlg, WM_USER_DIALOG, 1, (LPARAM)data);
                                }
                                switch (ret)
                                {
                                case IDB_ALL:
                                    dlgData.OverwriteAll = TRUE;
                                case IDYES:
                                default: // for safety (so that it cannot escape from here with a closed handle in)
                                {
                                    in = HANDLES_Q(CreateFile(op->SourceName, GENERIC_READ,
                                                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                              OPEN_EXISTING, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                                    if (in == INVALID_HANDLE_VALUE)
                                        goto OPEN_IN_ERROR;
                                    break;
                                }

                                case IDB_SKIPALL:
                                    dlgData.SkipAllOverwrite = TRUE;
                                case IDB_SKIP:
                                {
                                SKIP_OPEN:

                                    totalDone += op->Size;
                                    SetTFSandPSforSkippedFile(op, lastTransferredFileSize, script, totalDone);

                                    SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                                    if (skip != NULL)
                                        *skip = TRUE;
                                    return TRUE;
                                }

                                case IDCANCEL:
                                {
                                CANCEL_OPEN:

                                    return FALSE;
                                }
                                }
                            }

                            DWORD attr = SalGetFileAttributes(op->TargetName);
                            if (attr != INVALID_FILE_ATTRIBUTES && (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
                            {
                                if (!dlgData.OverwriteHiddenAll && dlgData.CnfrmSHFileOver) // Here we ignore script->OverwriteOlder, the user wants to see that it is a SYSTEM or HIDDEN file even with the option turned on
                                {
                                    HANDLES(CloseHandle(in));
                                    in = NULL;

                                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                    if (*dlgData.CancelWorker)
                                        goto CANCEL_OPEN;

                                    if (dlgData.SkipAllSystemOrHidden)
                                        goto SKIP_OPEN;

                                    int ret = IDCANCEL;
                                    char* data[4];
                                    data[0] = (char*)&ret;
                                    data[1] = LoadStr(IDS_CONFIRMFILEOVERWRITING);
                                    data[2] = op->TargetName;
                                    data[3] = LoadStr(IDS_WANTOVERWRITESHFILE);
                                    SendMessage(hProgressDlg, WM_USER_DIALOG, 2, (LPARAM)data);
                                    switch (ret)
                                    {
                                    case IDB_ALL:
                                        dlgData.OverwriteHiddenAll = TRUE;
                                    case IDYES:
                                    default: // for safety (so that it cannot escape from here with a closed handle in)
                                    {
                                        in = HANDLES_Q(CreateFile(op->SourceName, GENERIC_READ,
                                                                  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                                  OPEN_EXISTING, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                                        if (in == INVALID_HANDLE_VALUE)
                                            goto OPEN_IN_ERROR;
                                        attr = SalGetFileAttributes(op->TargetName); // if the user changes the attributes, let's have the current values
                                        break;
                                    }

                                    case IDB_SKIPALL:
                                        dlgData.SkipAllSystemOrHidden = TRUE;
                                    case IDB_SKIP:
                                        goto SKIP_OPEN;

                                    case IDCANCEL:
                                        goto CANCEL_OPEN;
                                    }
                                }
                            }

                            BOOL targetCannotOpenForWrite = FALSE;
                            while (1)
                            {
                                if (targetCannotOpenForWrite || mustDeleteFileBeforeOverwrite == 1 /* yes*/)
                                { // the file must be deleted first
                                    BOOL chAttr = ClearReadOnlyAttr(op->TargetName, attr);

                                    if (!tgtNameCaseCorrected)
                                    {
                                        CorrectCaseOfTgtName(op->TargetName, FALSE, &dataOut);
                                        tgtNameCaseCorrected = TRUE;
                                    }

                                    if (DeleteFile(op->TargetName))
                                        goto OPEN_TGT_FILE; // if it is read-only (clearing the attribute may not have passed), it can only be deleted on Samba with the "delete readonly" permission enabled
                                    else                    // cannot delete, ending with an error...
                                    {
                                        err = GetLastError();
                                        if (chAttr)
                                            SetFileAttributes(op->TargetName, attr);
                                        errDeletingFile = TRUE;
                                        goto NORMAL_ERROR;
                                    }
                                }
                                else // we are going to overwrite the file in place
                                {
                                    // if we haven't tested the file truncation functionality yet, we will obtain the current file size
                                    CQuadWord origFileSize(0, 0); // file size before truncation
                                    if (mustDeleteFileBeforeOverwrite == 0 /* need test*/)
                                    {
                                        out = HANDLES_Q(CreateFile(op->TargetName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                                   OPEN_EXISTING, 0, NULL));
                                        if (out != INVALID_HANDLE_VALUE)
                                        {
                                            origFileSize.LoDWord = GetFileSize(out, &origFileSize.HiDWord);
                                            if (origFileSize.LoDWord == INVALID_FILE_SIZE && GetLastError() == NO_ERROR)
                                                origFileSize.Set(0, 0); // error => set size to zero, test it on another file
                                            HANDLES(CloseHandle(out));
                                        }
                                    }

                                    // open the file with deletion of ADS and truncation to zero
                                    BOOL chAttr = FALSE;
                                    if (attr != INVALID_FILE_ATTRIBUTES &&
                                        (attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
                                    { // CREATE_ALWAYS does not tolerate readonly, hidden, and system attributes, so we may drop them if necessary
                                        chAttr = TRUE;
                                        SetFileAttributes(op->TargetName, 0);
                                    }
                                    // GENERIC_READ for 'out' causes slowdown in asynchronous copying from disk to network (on Win7 x64 GLAN measured 95MB/s instead of 111MB/s)
                                    DWORD access = GENERIC_WRITE | (script->CopyAttrs ? GENERIC_READ : 0);
                                    fileAttrs = asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN |
                                                (!lossEncryptionAttr && copyAsEncrypted ? FILE_ATTRIBUTE_ENCRYPTED : 0) | // Setting the attribute during CREATE_ALWAYS works from XP onwards, and if the file has read-denied permissions, it is the only way to set the Encrypted attribute.
                                                (script->CopyAttrs ? (op->Attr & (FILE_ATTRIBUTE_COMPRESSED | (lossEncryptionAttr ? 0 : FILE_ATTRIBUTE_ENCRYPTED))) : 0);
                                    out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, CREATE_ALWAYS, fileAttrs, NULL));
                                    if (out == INVALID_HANDLE_VALUE && fileAttrs != (asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN)) // if the file cannot be created with Encrypted, we will try it without (there is a risk on an NTFS network drive (tuned on a share from XP machines), on which we are logged in under a different name than we have in the system (on the current console) - the trick is that on the target system there is a user with the same name without a password, so it is network unusable)
                                        out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, CREATE_ALWAYS, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                                    if (script->CopyAttrs && out == INVALID_HANDLE_VALUE)
                                    { // in case read-access to the directory is not allowed (which we added just for setting the Compressed attribute), we will try to open the file only for writing
                                        access = GENERIC_WRITE;
                                        out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, CREATE_ALWAYS, fileAttrs, NULL));
                                        if (out == INVALID_HANDLE_VALUE && fileAttrs != (asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN)) // if the file cannot be created with Encrypted, we will try it without (there is a risk on an NTFS network drive (tuned on a share from XP machines), on which we are logged in under a different name than we have in the system (on the current console) - the trick is that on the target system there is a user with the same name without a password, so it is network unusable)
                                            out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, CREATE_ALWAYS, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                                    }
                                    if (out == INVALID_HANDLE_VALUE) // Cannot open the target file for writing, so we will try to delete it and create it again
                                    {
                                        // handles the situation when it is necessary to rewrite a file on Samba:
                                        // file has 440+another_owner and is in a directory where the current user has write access
                                        // (deletion is possible, but direct overwrite is not (cannot be opened for writing) - we bypass:
                                        //  delete and create the file again
                                        // (In Samba, it is possible to enable deleting read-only, which allows deleting read-only files,
                                        //  otherwise it cannot be deleted, because Windows cannot delete a read-only file at the same time
                                        //  It is not possible to remove the "read-only" attribute from that file because the current user is not the owner.
                                        if (chAttr)
                                            SetFileAttributes(op->TargetName, attr);
                                        targetCannotOpenForWrite = TRUE;
                                        continue;
                                    }

                                    // On target paths supporting ADS, we will also delete ADS on the target file (CREATE_ALWAYS should delete them, but on home W2K and XP they remain, I don't know why, W2K and XP in VMWare normally delete ADS).
                                    if (script->TargetPathSupADS && !DeleteAllADS(out, op->TargetName))
                                    {
                                        HANDLES(CloseHandle(out));
                                        out = INVALID_HANDLE_VALUE;
                                        if (chAttr)
                                            SetFileAttributes(op->TargetName, attr);
                                        targetCannotOpenForWrite = TRUE;
                                        continue;
                                    }

                                    // if we haven't tested the file truncation functionality yet, we will obtain the new file size
                                    if (mustDeleteFileBeforeOverwrite == 0 /* need test*/)
                                    {
                                        HANDLES(CloseHandle(out));
                                        out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, OPEN_ALWAYS, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                                        if (out == INVALID_HANDLE_VALUE) // Cannot open the target file that was opened a moment ago, unlikely, let's try to delete it and create it again
                                        {
                                            targetCannotOpenForWrite = TRUE;
                                            continue;
                                        }
                                        CQuadWord newFileSize(0, 0); // file size after truncation
                                        newFileSize.LoDWord = GetFileSize(out, &newFileSize.HiDWord);
                                        if ((newFileSize.LoDWord != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) && // we have a new size
                                            newFileSize == CQuadWord(0, 0))                                             // soubor ma skutecne 0 bytu
                                        {
                                            if (origFileSize != CQuadWord(0, 0))            // if shortening works, it can only be tested on a non-zero file
                                                mustDeleteFileBeforeOverwrite = 2; /* no*/ // It succeeded (there is no SNAP server - NSA drive, this does not work there)
                                        }
                                        else
                                        {
                                            HANDLES(CloseHandle(out));
                                            out = INVALID_HANDLE_VALUE;
                                            mustDeleteFileBeforeOverwrite = 1 /* yes*/; // in case of an error or if the size is not zero, we throw an exception...
                                            continue;
                                        }
                                    }

                                    if (script->CopyAttrs || !lossEncryptionAttr && copyAsEncrypted)
                                    {
                                        encryptionNotSupported = FALSE;
                                        SetCompressAndEncryptedAttrs(op->TargetName, (!lossEncryptionAttr && copyAsEncrypted ? FILE_ATTRIBUTE_ENCRYPTED : 0) | (script->CopyAttrs ? (op->Attr & (FILE_ATTRIBUTE_COMPRESSED | (lossEncryptionAttr ? 0 : FILE_ATTRIBUTE_ENCRYPTED))) : 0),
                                                                     &out, script->CopyAttrs, &encryptionNotSupported, asyncPar);
                                        if (encryptionNotSupported) // Unable to set the Encrypted attribute, let's ask the user what to do...
                                        {
                                            if (dlgData.FileOutLossEncrAll)
                                                lossEncryptionAttr = TRUE;
                                            else
                                            {
                                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                                if (*dlgData.CancelWorker)
                                                    goto CANCEL_ENCNOTSUP;

                                                if (dlgData.SkipAllFileOutLossEncr)
                                                    goto SKIP_ENCNOTSUP;

                                                int ret;
                                                ret = IDCANCEL;
                                                char* data[4];
                                                data[0] = (char*)&ret;
                                                data[1] = (char*)TRUE;
                                                data[2] = op->TargetName;
                                                data[3] = (char*)(INT_PTR)isMove;
                                                SendMessage(hProgressDlg, WM_USER_DIALOG, 12, (LPARAM)data);
                                                switch (ret)
                                                {
                                                case IDB_ALL:
                                                    dlgData.FileOutLossEncrAll = TRUE; // here break; is not missing
                                                case IDYES:
                                                    lossEncryptionAttr = TRUE;
                                                    break;

                                                case IDB_SKIPALL:
                                                    dlgData.SkipAllFileOutLossEncr = TRUE;
                                                case IDB_SKIP:
                                                    goto SKIP_ENCNOTSUP;

                                                case IDCANCEL:
                                                    goto CANCEL_ENCNOTSUP;
                                                }
                                            }
                                        }
                                    }
                                }
                                break;
                            }

                            goto COPY;
                        }
                        else // common error
                        {
                        NORMAL_ERROR:

                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                            if (*dlgData.CancelWorker)
                                goto CANCEL_OPEN2;

                            if (dlgData.SkipAllFileOpenOut)
                                goto SKIP_OPEN_OUT;

                            int ret;
                            ret = IDCANCEL;
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = LoadStr(errDeletingFile ? IDS_ERRORDELETINGFILE : IDS_ERROROPENINGFILE);
                            data[2] = op->TargetName;
                            data[3] = GetErrorText(err);
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                            switch (ret)
                            {
                            case IDRETRY:
                                break;

                            case IDB_SKIPALL:
                                dlgData.SkipAllFileOpenOut = TRUE;
                            case IDB_SKIP:
                                goto SKIP_OPEN_OUT;

                            case IDCANCEL:
                                goto CANCEL_OPEN2;
                            }
                        }
                    }
                }
            }
        }
        else
        {
        OPEN_IN_ERROR:

            DWORD err = GetLastError();
            if (invalidSrcName)
                err = ERROR_INVALID_NAME;
            if (asyncPar->Failed())
                err = ERROR_NOT_ENOUGH_MEMORY;                         // cannot create event for synchronization = lack of resources (probably will never happen, so don't worry about it)
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (dlgData.SkipAllFileOpenIn)
                goto SKIP_OPEN_IN;

            int ret;
            ret = IDCANCEL;
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = LoadStr(IDS_ERROROPENINGFILE);
            data[2] = op->SourceName;
            data[3] = GetErrorText(err);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
            switch (ret)
            {
            case IDRETRY:
                break;

            case IDB_SKIPALL:
                dlgData.SkipAllFileOpenIn = TRUE;
            case IDB_SKIP:
            {
            SKIP_OPEN_IN:

                totalDone += op->Size;
                SetTFSandPSforSkippedFile(op, lastTransferredFileSize, script, totalDone);

                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                if (skip != NULL)
                    *skip = TRUE;
                return TRUE;
            }

            case IDCANCEL:
                return FALSE;
            }
        }
    }
}

BOOL DoMoveFile(COperation* op, HWND hProgressDlg, void* buffer,
                COperations* script, CQuadWord& totalDone, BOOL dir,
                DWORD clearReadonlyMask, BOOL* novellRenamePatch, BOOL lantasticCheck,
                int& mustDeleteFileBeforeOverwrite, int& allocWholeFileOnStart,
                CProgressDlgData& dlgData, BOOL copyADS, BOOL copyAsEncrypted,
                BOOL* setDirTimeAfterMove, CAsyncCopyParams*& asyncPar,
                BOOL ignInvalidName)
{
    if (script->CopyAttrs && copyAsEncrypted)
        TRACE_E("DoMoveFile(): unexpected parameter value: copyAsEncrypted is TRUE when script->CopyAttrs is TRUE!");

    // if the path ends with a space/dot, it is invalid and we must not perform the move,
    // MoveFile trims spaces/dots and moves another file possibly to a different name,
    // Directories are better off, adding a backslash to the end there, we move the defense
    // only if a new directory name that is invalid is being created (when moving under the old
    // named 'ignInvalidName' TRUE)
    BOOL invalidName = FileNameIsInvalid(op->SourceName, TRUE, dir) ||
                       FileNameIsInvalid(op->TargetName, TRUE, dir && ignInvalidName);

    if (!copyAsEncrypted && !script->SameRootButDiffVolume && HasTheSameRootPath(op->SourceName, op->TargetName))
    {
        // if the path ends with a space/dot, we need to append '\\', otherwise GetNamedSecurityInfo,
        // GetDirTime, SetFileAttributes and MoveFile will trim spaces/dots and work with a different path
        const char* sourceNameMvDir = op->SourceName;
        char sourceNameMvDirCopy[3 * MAX_PATH];
        MakeCopyWithBackslashIfNeeded(sourceNameMvDir, sourceNameMvDirCopy);
        const char* targetNameMvDir = op->TargetName;
        char targetNameMvDirCopy[3 * MAX_PATH];
        MakeCopyWithBackslashIfNeeded(targetNameMvDir, targetNameMvDirCopy);

        int autoRetryAttempts = 0;
        CSrcSecurity srcSecurity;
        BOOL srcSecurityErr = FALSE;
        if (!invalidName && script->CopySecurity) // Should we copy NTFS security permissions?
        {
            srcSecurity.SrcError = GetNamedSecurityInfo(sourceNameMvDir, SE_FILE_OBJECT,
                                                        DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                                        &srcSecurity.SrcOwner, &srcSecurity.SrcGroup, &srcSecurity.SrcDACL,
                                                        NULL, &srcSecurity.SrcSD);
            if (srcSecurity.SrcError != ERROR_SUCCESS) // error reading security-info from source file -> nothing to set on the target
            {
                srcSecurityErr = TRUE;
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                if (*dlgData.CancelWorker)
                    return FALSE;

                int ret;
                ret = IDCANCEL;
                if (dlgData.IgnoreAllCopyPermErr)
                    ret = IDB_IGNORE;
                else
                {
                    char* data[4];
                    data[0] = (char*)&ret;
                    data[1] = op->SourceName;
                    data[2] = op->TargetName;
                    data[3] = (char*)(DWORD_PTR)srcSecurity.SrcError;
                    SendMessage(hProgressDlg, WM_USER_DIALOG, 10, (LPARAM)data);
                }
                switch (ret)
                {
                case IDB_IGNOREALL:
                    dlgData.IgnoreAllCopyPermErr = TRUE; // here break; is not missing
                case IDB_IGNORE:
                    break;

                case IDCANCEL:
                    return FALSE;
                }
            }
        }
        FILETIME dirTimeModified;
        BOOL dirTimeModifiedIsValid = FALSE;
        if (!invalidName && dir && !*novellRenamePatch && *setDirTimeAfterMove != 2 /* no*/) // The problem apparently does not concern Novell Netware, so we will not address it there at all (it concerns, for example, Samba)
            dirTimeModifiedIsValid = GetDirTime(sourceNameMvDir, &dirTimeModified);
        while (1)
        {
            if (!invalidName && !*novellRenamePatch && MoveFile(sourceNameMvDir, targetNameMvDir))
            {
                if (script->CopyAttrs && (op->Attr & FILE_ATTRIBUTE_ARCHIVE) == 0) // Attribute Archive was not set, MoveFile set it, we will clean it up again
                    SetFileAttributes(targetNameMvDir, op->Attr);                  // Leave it without handling and retry, unimportant (usually set chaotically)

            OPERATION_DONE:

                if (script->CopyAttrs) // we will check if the attributes of the source file were preserved
                {
                    DWORD curAttrs;
                    curAttrs = SalGetFileAttributes(targetNameMvDir);
                    if (curAttrs == INVALID_FILE_ATTRIBUTES || (curAttrs & DISPLAYED_ATTRIBUTES) != (op->Attr & DISPLAYED_ATTRIBUTES))
                    {                                                              // Attributes probably failed to transfer, warning the user
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                        if (*dlgData.CancelWorker)
                            goto MOVE_ERROR_2;

                        int ret;
                        ret = IDCANCEL;
                        if (dlgData.IgnoreAllSetAttrsErr)
                            ret = IDB_IGNORE;
                        else
                        {
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = op->TargetName;
                            data[2] = (char*)(DWORD_PTR)(op->Attr & DISPLAYED_ATTRIBUTES);
                            data[3] = (char*)(DWORD_PTR)(curAttrs == INVALID_FILE_ATTRIBUTES ? 0 : (curAttrs & DISPLAYED_ATTRIBUTES));
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 9, (LPARAM)data);
                        }
                        switch (ret)
                        {
                        case IDB_IGNOREALL:
                            dlgData.IgnoreAllSetAttrsErr = TRUE; // here break; is not missing
                        case IDB_IGNORE:
                            break;

                        case IDCANCEL:
                        {
                        MOVE_ERROR_2:

                            return FALSE; // the file did move to the destination + cancel occurred, but let's skip moving it back, probably no one will mind
                        }
                        }
                    }
                }

                if (script->CopySecurity && !srcSecurityErr) // Should we copy NTFS security permissions?
                {
                    DWORD err;
                    if (!DoCopySecurity(sourceNameMvDir, targetNameMvDir, &err, &srcSecurity))
                    {
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                        if (*dlgData.CancelWorker)
                            goto MOVE_ERROR_2;

                        int ret;
                        ret = IDCANCEL;
                        if (dlgData.IgnoreAllCopyPermErr)
                            ret = IDB_IGNORE;
                        else
                        {
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = op->SourceName;
                            data[2] = op->TargetName;
                            data[3] = (char*)(DWORD_PTR)err;
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 10, (LPARAM)data);
                        }
                        switch (ret)
                        {
                        case IDB_IGNOREALL:
                            dlgData.IgnoreAllCopyPermErr = TRUE; // here break; is not missing
                        case IDB_IGNORE:
                            break;

                        case IDCANCEL:
                            goto MOVE_ERROR_2;
                        }
                    }
                }

                if (dir && dirTimeModifiedIsValid && *setDirTimeAfterMove != 2 /* no*/)
                {
                    FILETIME movedDirTimeModified;
                    if (GetDirTime(targetNameMvDir, &movedDirTimeModified))
                    {
                        if (CompareFileTime(&dirTimeModified, &movedDirTimeModified) == 0)
                        {
                            if (*setDirTimeAfterMove == 0 /* need test*/)
                                *setDirTimeAfterMove = 2 /* no*/;
                        }
                        else
                        {
                            if (*setDirTimeAfterMove == 0 /* need test*/)
                                *setDirTimeAfterMove = 1 /* yes*/;
                            DoCopyDirTime(hProgressDlg, targetNameMvDir, &dirTimeModified, dlgData, TRUE); // pripadny neuspech ignorujeme, je to proste jen hack (ignorujeme uz i chybu cteni casu a datumu z adresare), MoveFile by casy menit nemel
                        }
                    }
                }

                script->AddBytesToSpeedMetersAndTFSandPS((DWORD)op->Size.Value, TRUE, 0, NULL, MAX_OP_FILESIZE);

                totalDone += op->Size;
                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                return TRUE;
            }
            else
            {
                DWORD err = GetLastError();
                if (invalidName)
                    err = ERROR_INVALID_NAME;
                // patch for Novell - before calling MoveFile, the read-only attribute needs to be removed
                if (!invalidName && *novellRenamePatch || err == ERROR_ACCESS_DENIED)
                {
                    DWORD attr = SalGetFileAttributes(sourceNameMvDir);
                    BOOL setAttr = ClearReadOnlyAttr(sourceNameMvDir, attr);
                    if (MoveFile(sourceNameMvDir, targetNameMvDir))
                    {
                        if (!*novellRenamePatch)
                            *novellRenamePatch = TRUE; // We will proceed with the next operation straight ahead
                        if (setAttr || script->CopyAttrs && (attr & FILE_ATTRIBUTE_ARCHIVE) == 0)
                            SetFileAttributes(targetNameMvDir, attr);

                        goto OPERATION_DONE;
                    }
                    err = GetLastError();
                    if (setAttr)
                        SetFileAttributes(sourceNameMvDir, attr);
                }

                if (StrICmp(op->SourceName, op->TargetName) != 0 && // if it's not just about change-case
                    (err == ERROR_FILE_EXISTS ||                    // we will check if it's just a rewrite of the DOS file/directory name
                     err == ERROR_ALREADY_EXISTS) &&
                    targetNameMvDir == op->TargetName) // We will not allow any invalid names here
                {
                    WIN32_FIND_DATA findData;
                    HANDLE find = HANDLES_Q(FindFirstFile(op->TargetName, &findData));
                    if (find != INVALID_HANDLE_VALUE)
                    {
                        HANDLES(FindClose(find));
                        const char* tgtName = SalPathFindFileName(op->TargetName);
                        if (StrICmp(tgtName, findData.cAlternateFileName) == 0 && // match only for two names
                            StrICmp(tgtName, findData.cFileName) != 0)            // (full name is different)
                        {
                            // rename ("clean up") the file/directory with a conflicting long name to a temporary 8.3 name (does not require an extra long name)
                            char tmpName[MAX_PATH + 20];
                            lstrcpyn(tmpName, op->TargetName, MAX_PATH);
                            CutDirectory(tmpName);
                            SalPathAddBackslash(tmpName, MAX_PATH + 20);
                            char* tmpNamePart = tmpName + strlen(tmpName);
                            char origFullName[MAX_PATH];
                            if (SalPathAppend(tmpName, findData.cFileName, MAX_PATH))
                            {
                                strcpy(origFullName, tmpName);
                                DWORD num = (GetTickCount() / 10) % 0xFFF;
                                DWORD origFullNameAttr = SalGetFileAttributes(origFullName);
                                while (1)
                                {
                                    sprintf(tmpNamePart, "sal%03X", num++);
                                    if (SalMoveFile(origFullName, tmpName))
                                        break;
                                    DWORD e = GetLastError();
                                    if (e != ERROR_FILE_EXISTS && e != ERROR_ALREADY_EXISTS)
                                    {
                                        tmpName[0] = 0;
                                        break;
                                    }
                                }
                                if (tmpName[0] != 0) // if the conflict file/directory has been successfully "cleaned up", we will try to move
                                {                    // file/directory again, then we will return the "cleaned up" file/directory its original name
                                    BOOL moveDone = SalMoveFile(sourceNameMvDir, op->TargetName);
                                    if (script->CopyAttrs && (op->Attr & FILE_ATTRIBUTE_ARCHIVE) == 0) // Attribute Archive was not set, MoveFile set it, we will clean it up again
                                        SetFileAttributes(op->TargetName, op->Attr);                   // Leave it without handling and retry, unimportant (usually set chaotically)
                                    if (!SalMoveFile(tmpName, origFullName))
                                    { // this can obviously happen, incomprehensible, but Windows will create a space at op->TargetName (dos name) file named origFullName
                                        TRACE_I("DoMoveFile(): Unexpected situation: unable to rename file/dir from tmp-name to original long file name! " << origFullName);
                                        if (moveDone)
                                        {
                                            if (SalMoveFile(op->TargetName, sourceNameMvDir))
                                                moveDone = FALSE;
                                            if (!SalMoveFile(tmpName, origFullName))
                                                TRACE_E("DoMoveFile(): Fatal unexpected situation: unable to rename file/dir from tmp-name to original long file name! " << origFullName);
                                        }
                                    }
                                    else
                                    {
                                        if ((origFullNameAttr & FILE_ATTRIBUTE_ARCHIVE) == 0)
                                            SetFileAttributes(origFullName, origFullNameAttr); // Leave it without handling and retry, unimportant (usually set chaotically)
                                    }

                                    if (moveDone)
                                        goto OPERATION_DONE;
                                }
                            }
                            else
                                TRACE_E("DoMoveFile(): Original full file/dir name is too long, unable to bypass only-dos-name-overwrite problem!");
                        }
                    }
                }

                if ((err == ERROR_ALREADY_EXISTS || // theoretically it can also occur for directories, we prevent that (the overwrite prompt is only for files)
                     err == ERROR_FILE_EXISTS) &&
                    !dir && StrICmp(op->SourceName, op->TargetName) != 0 &&
                    sourceNameMvDir == op->SourceName && targetNameMvDir == op->TargetName) // We will not allow any invalid names here (it's only for files and their names are checked).
                {
                    HANDLE in, out;
                    in = HANDLES_Q(CreateFile(op->SourceName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
                    if (in == INVALID_HANDLE_VALUE)
                    {
                        err = GetLastError();
                        goto NORMAL_ERROR;
                    }
                    out = HANDLES_Q(CreateFile(op->TargetName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
                    if (out == INVALID_HANDLE_VALUE)
                    {
                        err = GetLastError();
                        HANDLES(CloseHandle(in));
                        goto NORMAL_ERROR;
                    }

                    if (!dlgData.OverwriteAll && (dlgData.CnfrmFileOver || script->OverwriteOlder))
                    {
                        char sAttr[101], tAttr[101];
                        BOOL getTimeFailed;
                        getTimeFailed = FALSE;
                        FILETIME sFileTime, tFileTime;
                        GetFileOverwriteInfo(sAttr, _countof(sAttr), in, op->SourceName, &sFileTime, &getTimeFailed);
                        GetFileOverwriteInfo(tAttr, _countof(tAttr), out, op->TargetName, &tFileTime, &getTimeFailed);
                        HANDLES(CloseHandle(in));
                        HANDLES(CloseHandle(out));

                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                        if (*dlgData.CancelWorker)
                            goto CANCEL_OPEN;

                        if (dir)
                            TRACE_E("Error in script.");

                        if (dlgData.SkipAllOverwrite)
                            goto SKIP_OPEN;

                        int ret;
                        ret = IDCANCEL;

                        if (!getTimeFailed && script->OverwriteOlder) // option from the Copy/Move dialog
                        {
                            // We will truncate times to seconds (different file systems store times with different precisions, so there were "differences" even between "identical" times)
                            *(unsigned __int64*)&sFileTime = *(unsigned __int64*)&sFileTime - (*(unsigned __int64*)&sFileTime % 10000000);
                            *(unsigned __int64*)&tFileTime = *(unsigned __int64*)&tFileTime - (*(unsigned __int64*)&tFileTime % 10000000);

                            if (CompareFileTime(&sFileTime, &tFileTime) > 0)
                                ret = IDYES; // older mother without asking to rewrite
                            else
                                ret = IDB_SKIP; // skip the remaining existing
                        }
                        else
                        {
                            // display the query
                            char* data[5];
                            data[0] = (char*)&ret;
                            data[1] = op->TargetName;
                            data[2] = tAttr;
                            data[3] = op->SourceName;
                            data[4] = sAttr;
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 1, (LPARAM)data);
                        }
                        switch (ret)
                        {
                        case IDB_ALL:
                            dlgData.OverwriteAll = TRUE;
                        case IDYES:
                            break;

                        case IDB_SKIPALL:
                            dlgData.SkipAllOverwrite = TRUE;
                        case IDB_SKIP:
                        {
                        SKIP_OPEN:

                            totalDone += op->Size;
                            script->SetProgressSize(totalDone);
                            SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                            return TRUE;
                        }

                        case IDCANCEL:
                        {
                        CANCEL_OPEN:

                            return FALSE;
                        }
                        }
                    }
                    else
                    {
                        HANDLES(CloseHandle(in));
                        HANDLES(CloseHandle(out));
                    }

                    DWORD attr = SalGetFileAttributes(op->TargetName);
                    if (attr != INVALID_FILE_ATTRIBUTES && (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
                    {
                        if (!dlgData.OverwriteHiddenAll && dlgData.CnfrmSHFileOver) // Here we ignore script->OverwriteOlder, the user wants to see that it is a SYSTEM or HIDDEN file even with the option turned on
                        {
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                            if (*dlgData.CancelWorker)
                                goto CANCEL_OPEN;

                            if (dir)
                                TRACE_E("Error in script.");

                            if (dlgData.SkipAllSystemOrHidden)
                                goto SKIP_OPEN;

                            int ret = IDCANCEL;
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = LoadStr(IDS_CONFIRMFILEOVERWRITING);
                            data[2] = op->TargetName;
                            data[3] = LoadStr(IDS_WANTOVERWRITESHFILE);
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 2, (LPARAM)data);
                            switch (ret)
                            {
                            case IDB_ALL:
                                dlgData.OverwriteHiddenAll = TRUE;
                            case IDYES:
                                break;

                            case IDB_SKIPALL:
                                dlgData.SkipAllSystemOrHidden = TRUE;
                            case IDB_SKIP:
                                goto SKIP_OPEN;

                            case IDCANCEL:
                                goto CANCEL_OPEN;
                            }
                            attr = SalGetFileAttributes(op->TargetName); // can also fail (assigns INVALID_FILE_ATTRIBUTES)
                        }
                    }

                    ClearReadOnlyAttr(op->TargetName, attr); // to be deleted ...
                    while (1)
                    {
                        if (DeleteFile(op->TargetName))
                            break;
                        else
                        {
                            DWORD err2 = GetLastError();
                            if (err2 == ERROR_FILE_NOT_FOUND)
                                break; // if the user managed to delete the file themselves, everything is OK

                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                            if (*dlgData.CancelWorker)
                                return FALSE;

                            if (dir)
                                TRACE_E("Error in script.");

                            if (dlgData.SkipAllOverwriteErr)
                                goto SKIP_OVERWRITE_ERROR;

                            int ret;
                            ret = IDCANCEL;
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = LoadStr(IDS_ERROROVERWRITINGFILE);
                            data[2] = op->TargetName;
                            data[3] = GetErrorText(err2);
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                            switch (ret)
                            {
                            case IDRETRY:
                                break;

                            case IDB_SKIPALL:
                                dlgData.SkipAllOverwriteErr = TRUE;
                            case IDB_SKIP:
                            {
                            SKIP_OVERWRITE_ERROR:

                                totalDone += op->Size;
                                script->SetProgressSize(totalDone);
                                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                                return TRUE;
                            }

                            case IDCANCEL:
                                return FALSE;
                            }
                        }
                    }
                }
                else
                {
                NORMAL_ERROR:

                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                    if (*dlgData.CancelWorker)
                        return FALSE;

                    if (dlgData.SkipAllMoveErrors)
                        goto SKIP_MOVE_ERROR;

                    if (err == ERROR_SHARING_VIOLATION && ++autoRetryAttempts <= 2)
                    {               // auto-retry introduced due to errors during moving icons in a directory (conflict between SHGetFileInfo and MoveFile)
                        Sleep(100); // Let's wait a moment before the next attempt
                    }
                    else
                    {
                        int ret;
                        ret = IDCANCEL;
                        char* data[4];
                        data[0] = (char*)&ret;
                        data[1] = op->SourceName;
                        data[2] = op->TargetName;
                        data[3] = GetErrorText(err);
                        SendMessage(hProgressDlg, WM_USER_DIALOG, dir ? 4 : 3, (LPARAM)data);
                        switch (ret)
                        {
                        case IDRETRY:
                            break;

                        case IDB_SKIPALL:
                            dlgData.SkipAllMoveErrors = TRUE;
                        case IDB_SKIP:
                        {
                        SKIP_MOVE_ERROR:

                            totalDone += op->Size;
                            script->SetProgressSize(totalDone);
                            SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                            return TRUE;
                        }

                        case IDCANCEL:
                            return FALSE;
                        }
                    }
                }
            }
        }
    }
    else
    {
        if (dir)
        {
            TRACE_E("Error in script.");
            return FALSE;
        }

        BOOL skip;
        BOOL notError = DoCopyFile(op, hProgressDlg, buffer, script, totalDone,
                                   clearReadonlyMask, &skip, lantasticCheck,
                                   mustDeleteFileBeforeOverwrite, allocWholeFileOnStart,
                                   dlgData, copyADS, copyAsEncrypted, TRUE, asyncPar);
        if (notError && !skip) // the source file still needs to be cleaned up
        {
            ClearReadOnlyAttr(op->SourceName); // to be deleted ...
            while (1)
            {
                if (DeleteFile(op->SourceName))
                    break;
                {
                    DWORD err = GetLastError();

                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                    if (*dlgData.CancelWorker)
                        return FALSE;

                    if (dlgData.SkipAllDeleteErr)
                        return TRUE;

                    int ret = IDCANCEL;
                    char* data[4];
                    data[0] = (char*)&ret;
                    data[1] = LoadStr(IDS_ERRORDELETINGFILE);
                    data[2] = op->SourceName;
                    data[3] = GetErrorText(err);
                    SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                    switch (ret)
                    {
                    case IDRETRY:
                        break;
                    case IDB_SKIPALL:
                        dlgData.SkipAllDeleteErr = TRUE;
                    case IDB_SKIP:
                        return TRUE;
                    case IDCANCEL:
                        return FALSE;
                    }
                }
            }
        }
        return notError;
    }
}

BOOL DoDeleteFile(HWND hProgressDlg, char* name, const CQuadWord& size, COperations* script,
                  CQuadWord& totalDone, DWORD attr, CProgressDlgData& dlgData)
{
    // if the path ends with a space/dot, it is invalid and we must not perform deletion,
    // DeleteFile trims spaces/dots and thus deletes another file
    BOOL invalidName = FileNameIsInvalid(name, TRUE);

    DWORD err;
    while (1)
    {
        if (!invalidName)
        {
            if (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
            {
                if (!dlgData.DeleteHiddenAll && dlgData.CnfrmSHFileDel)
                {
                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                    if (*dlgData.CancelWorker)
                        return FALSE;

                    if (dlgData.SkipAllSystemOrHidden)
                        goto SKIP_DELETE;

                    int ret = IDCANCEL;
                    char* data[4];
                    data[0] = (char*)&ret;
                    data[1] = LoadStr(IDS_CONFIRMSHFILEDELETE);
                    data[2] = name;
                    data[3] = LoadStr(IDS_DELETESHFILE);
                    SendMessage(hProgressDlg, WM_USER_DIALOG, 2, (LPARAM)data);
                    switch (ret)
                    {
                    case IDB_ALL:
                        dlgData.DeleteHiddenAll = TRUE;
                    case IDYES:
                        break;

                    case IDB_SKIPALL:
                        dlgData.SkipAllSystemOrHidden = TRUE;
                    case IDB_SKIP:
                        goto SKIP_DELETE;

                    case IDCANCEL:
                        return FALSE;
                    }
                }
            }
            ClearReadOnlyAttr(name, attr); // to be deleted ...

            err = ERROR_SUCCESS;
            BOOL useRecycleBin;
            switch (dlgData.UseRecycleBin)
            {
            case 0:
                useRecycleBin = script->CanUseRecycleBin && script->InvertRecycleBin;
                break;
            case 1:
                useRecycleBin = script->CanUseRecycleBin && !script->InvertRecycleBin;
                break;
            case 2:
            {
                if (!script->CanUseRecycleBin || script->InvertRecycleBin)
                    useRecycleBin = FALSE;
                else
                {
                    const char* fileName = strrchr(name, '\\');
                    if (fileName != NULL) // "always true"
                    {
                        fileName++;
                        int tmpLen = lstrlen(fileName);
                        const char* ext = fileName + tmpLen;
                        //            while (ext > fileName && *ext != '.') ext--;
                        while (--ext >= fileName && *ext != '.')
                            ;
                        //            if (ext == fileName)   // ".cvspass" in Windows is the extension ...
                        if (ext < fileName)
                            ext = fileName + tmpLen;
                        else
                            ext++;
                        useRecycleBin = dlgData.AgreeRecycleMasks(fileName, ext);
                    }
                    else
                    {
                        useRecycleBin = TRUE; // in case of an error, we choose the safe option, delete to the trash
                        TRACE_E("DoDeleteFile(): unexpected situation: filename does not contain backslash: " << name);
                    }
                }
                break;
            }
            }
            if (useRecycleBin)
            {
                char nameList[MAX_PATH + 1];
                int l = (int)strlen(name) + 1;
                memmove(nameList, name, l);
                nameList[l] = 0;
                if (!PathContainsValidComponents(nameList, FALSE))
                {
                    err = ERROR_INVALID_NAME;
                }
                else
                {
                    CShellExecuteWnd shellExecuteWnd;
                    SHFILEOPSTRUCT opCode;
                    memset(&opCode, 0, sizeof(opCode));

                    opCode.hwnd = shellExecuteWnd.Create(hProgressDlg, "SEW: DoDeleteFile");

                    opCode.wFunc = FO_DELETE;
                    opCode.pFrom = nameList;
                    opCode.fFlags = FOF_ALLOWUNDO | FOF_SILENT | FOF_NOCONFIRMATION;
                    opCode.lpszProgressTitle = "";
                    err = SHFileOperation(&opCode);
                }
            }
            else
            {
                if (DeleteFile(name) == 0)
                    err = GetLastError();
            }
        }
        else
        {
            err = ERROR_INVALID_NAME;
        }
        if (err == ERROR_SUCCESS)
        {
            totalDone += size;
            SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
            return TRUE;
        }
        else
        {
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (dlgData.SkipAllDeleteErr)
                goto SKIP_DELETE;

            int ret;
            ret = IDCANCEL;
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = LoadStr(IDS_ERRORDELETINGFILE);
            data[2] = name;
            data[3] = GetErrorText(err);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
            switch (ret)
            {
            case IDRETRY:
                break;

            case IDB_SKIPALL:
                dlgData.SkipAllDeleteErr = TRUE;
            case IDB_SKIP:
            {
            SKIP_DELETE:

                totalDone += size;
                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                return TRUE;
            }

            case IDCANCEL:
                return FALSE;
            }
        }
        if (!invalidName)
        {
            DWORD attr2 = SalGetFileAttributes(name); // get the current state of the attribute
            if (attr2 != INVALID_FILE_ATTRIBUTES)
                attr = attr2;
        }
    }
}

BOOL SalCreateDirectoryEx(const char* name, DWORD* err)
{
    if (err != NULL)
        *err = 0;
    // if the name contains a space/dot at the end, it is necessary to add '\\' to the end, otherwise
    // another directory will be created (spaces/dots at the end of the CreateDirectory name will be silently trimmed)
    const char* nameCrDir = name;
    char nameCrDirBuf[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(nameCrDir, nameCrDirBuf);
    if (CreateDirectory(nameCrDir, NULL))
        return TRUE;
    else
    {
        DWORD errLoc = GetLastError();
        if (name == nameCrDir &&            // name with space/dot at the end surely does not collide with DOS name
            (errLoc == ERROR_FILE_EXISTS || // we will check if it's just a rewrite of the DOS file name
             errLoc == ERROR_ALREADY_EXISTS))
        {
            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(name, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(find));
                const char* tgtName = SalPathFindFileName(name);
                if (StrICmp(tgtName, data.cAlternateFileName) == 0 && // match only for two names
                    StrICmp(tgtName, data.cFileName) != 0)            // (full name is different)
                {
                    // rename ("clean up") the file/directory with a conflicting long name to a temporary 8.3 name (does not require an extra long name)
                    char tmpName[MAX_PATH + 20];
                    lstrcpyn(tmpName, name, MAX_PATH);
                    CutDirectory(tmpName);
                    SalPathAddBackslash(tmpName, MAX_PATH + 20);
                    char* tmpNamePart = tmpName + strlen(tmpName);
                    char origFullName[MAX_PATH];
                    if (SalPathAppend(tmpName, data.cFileName, MAX_PATH))
                    {
                        strcpy(origFullName, tmpName);
                        DWORD num = (GetTickCount() / 10) % 0xFFF;
                        DWORD origFullNameAttr = SalGetFileAttributes(origFullName);
                        while (1)
                        {
                            sprintf(tmpNamePart, "sal%03X", num++);
                            if (SalMoveFile(origFullName, tmpName))
                                break;
                            DWORD e = GetLastError();
                            if (e != ERROR_FILE_EXISTS && e != ERROR_ALREADY_EXISTS)
                            {
                                tmpName[0] = 0;
                                break;
                            }
                        }
                        if (tmpName[0] != 0) // if the conflict file has been "cleaned up", we will try to move
                        {                    // file again, then we return the "cleaned up" file its original name
                            BOOL createDirDone = CreateDirectory(name, NULL);
                            if (!SalMoveFile(tmpName, origFullName))
                            { // this can obviously happen, incomprehensible, but Windows will create a file named origFullName in the directory (dos name)
                                TRACE_I("Unexpected situation: unable to rename file from tmp-name to original long file name! " << origFullName);
                                if (createDirDone)
                                {
                                    if (RemoveDirectory(name))
                                        createDirDone = FALSE;
                                    if (!SalMoveFile(tmpName, origFullName))
                                        TRACE_E("Fatal unexpected situation: unable to rename file from tmp-name to original long file name! " << origFullName);
                                }
                            }
                            else
                            {
                                if ((origFullNameAttr & FILE_ATTRIBUTE_ARCHIVE) == 0)
                                    SetFileAttributes(origFullName, origFullNameAttr); // Leave it without handling and retry, unimportant (usually set chaotically)
                            }

                            if (createDirDone)
                                return TRUE;
                        }
                    }
                    else
                        TRACE_E("Original full file name is too long, unable to bypass only-dos-name-overwrite problem!");
                }
            }
        }
        if (err != NULL)
            *err = errLoc;
    }
    return FALSE;
}

BOOL GetDirTime(const char* dirName, FILETIME* ftModified)
{
    HANDLE dir;
    dir = HANDLES_Q(CreateFile(dirName, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS,
                               NULL));
    if (dir != INVALID_HANDLE_VALUE)
    {
        BOOL ret = GetFileTime(dir, NULL /*ftCreated*/, NULL /*ftAccessed*/, ftModified);
        HANDLES(CloseHandle(dir));
        return ret;
    }
    return FALSE;
}

BOOL DoCopyDirTime(HWND hProgressDlg, const char* targetName, FILETIME* modified, CProgressDlgData& dlgData, BOOL quiet)
{
    // if the path ends with a space/dot, we need to append '\\', otherwise CreateFile
    // Trims spaces/dots and operates with a different path
    const char* targetNameCrFile = targetName;
    char targetNameCrFileCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(targetNameCrFile, targetNameCrFileCopy);

    BOOL showError = !quiet;
    DWORD error = NO_ERROR;
    DWORD attr = GetFileAttributes(targetNameCrFile);
    BOOL setAttr = FALSE;
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY))
    {
        SetFileAttributes(targetNameCrFile, attr & ~FILE_ATTRIBUTE_READONLY);
        setAttr = TRUE;
    }
    HANDLE file;
    file = HANDLES_Q(CreateFile(targetNameCrFile, GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS,
                                NULL));
    if (file != INVALID_HANDLE_VALUE)
    {
        if (SetFileTime(file, NULL /*&ftCreated*/, NULL /*&ftAccessed*/, modified))
            showError = FALSE; // Success!
        else
            error = GetLastError();
        HANDLES(CloseHandle(file));
    }
    else
        error = GetLastError();
    if (setAttr)
        SetFileAttributes(targetNameCrFile, attr);

    if (showError)
    {
        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
        if (*dlgData.CancelWorker)
            return FALSE;

        int ret;
        ret = IDCANCEL;
        if (dlgData.IgnoreAllCopyDirTimeErr)
            ret = IDB_IGNORE;
        else
        {
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = (char*)targetNameCrFile;
            data[2] = (char*)(DWORD_PTR)error;
            SendMessage(hProgressDlg, WM_USER_DIALOG, 11, (LPARAM)data);
        }
        switch (ret)
        {
        case IDB_IGNOREALL:
            dlgData.IgnoreAllCopyDirTimeErr = TRUE; // here break; is not missing
        case IDB_IGNORE:
            break;

        case IDCANCEL:
            return FALSE;
        }
    }
    return TRUE;
}

BOOL DoCreateDir(HWND hProgressDlg, char* name, DWORD attr,
                 DWORD clearReadonlyMask, CProgressDlgData& dlgData,
                 CQuadWord& totalDone, CQuadWord& operTotal,
                 const char* sourceDir, BOOL adsCopy, COperations* script,
                 void* buffer, BOOL& skip, BOOL& alreadyExisted,
                 BOOL createAsEncrypted, BOOL ignInvalidName)
{
    if (script->CopyAttrs && createAsEncrypted)
        TRACE_E("DoCreateDir(): unexpected parameter value: createAsEncrypted is TRUE when script->CopyAttrs is TRUE!");

    skip = FALSE;
    alreadyExisted = FALSE;
    CQuadWord lastTransferredFileSize;
    script->GetTFS(&lastTransferredFileSize);

    BOOL invalidName = FileNameIsInvalid(name, TRUE, ignInvalidName);

    // if the path ends with a space/dot, we need to append '\\', otherwise SetFileAttributes
    // and RemoveDirectory trims spaces/dots and works with a different path
    const char* nameCrDir = name;
    char nameCrDirCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(nameCrDir, nameCrDirCopy);
    const char* sourceDirCrDir = sourceDir;
    char sourceDirCrDirCopy[3 * MAX_PATH];
    if (sourceDirCrDir != NULL)
        MakeCopyWithBackslashIfNeeded(sourceDirCrDir, sourceDirCrDirCopy);

    while (1)
    {
        DWORD err;
        if (!invalidName && SalCreateDirectoryEx(name, &err))
        {
            script->AddBytesToSpeedMetersAndTFSandPS((DWORD)CREATE_DIR_SIZE.Value, TRUE, 0, NULL, MAX_OP_FILESIZE); // directory has been created

            DWORD newAttr = attr & clearReadonlyMask;
            if (sourceDir != NULL && adsCopy) // if it is necessary to copy ADS, we will do it
            {
                CQuadWord operDone = CREATE_DIR_SIZE; // directory has been created
                BOOL adsSkip = FALSE;
                if (!DoCopyADS(hProgressDlg, sourceDir, TRUE, name, totalDone,
                               operDone, operTotal, dlgData, script, &adsSkip, buffer) ||
                    adsSkip) // user either canceled or skipped at least one ADS
                {
                    if (RemoveDirectory(nameCrDir) == 0)
                    {
                        DWORD err2 = GetLastError();
                        TRACE_E("Unable to remove newly created directory: " << name << ", error: " << GetErrorText(err2));
                    }
                    if (!adsSkip)
                        return FALSE; // cancel the whole operation (Skip must return TRUE)
                    skip = TRUE;
                    newAttr = -1; // the directory should no longer exist, so we will not set its attributes
                }
            }
            if (newAttr != -1)
            {
                if (script->CopyAttrs || createAsEncrypted) // Set Compressed & Encrypted attributes according to the source directory
                {
                    if (createAsEncrypted)
                    {
                        newAttr &= ~FILE_ATTRIBUTE_COMPRESSED;
                        newAttr |= FILE_ATTRIBUTE_ENCRYPTED;
                    }
                    DWORD changeAttrErr = NO_ERROR;
                    DWORD currentAttrs = SalGetFileAttributes(name);
                    if (currentAttrs != INVALID_FILE_ATTRIBUTES)
                    {
                        if ((newAttr & FILE_ATTRIBUTE_COMPRESSED) != (currentAttrs & FILE_ATTRIBUTE_COMPRESSED) &&
                            (newAttr & FILE_ATTRIBUTE_COMPRESSED) == 0)
                        {
                            changeAttrErr = UncompressFile(name, currentAttrs);
                        }
                        if (changeAttrErr == NO_ERROR &&
                            (newAttr & FILE_ATTRIBUTE_ENCRYPTED) != (currentAttrs & FILE_ATTRIBUTE_ENCRYPTED))
                        {
                            BOOL dummyCancelOper = FALSE;
                            if (newAttr & FILE_ATTRIBUTE_ENCRYPTED)
                            {
                                changeAttrErr = MyEncryptFile(hProgressDlg, name, currentAttrs, 0 /* to be able to encrypt directories with the SYSTEM attribute*/,
                                                              dlgData, dummyCancelOper, FALSE);

                                if ( //(WindowsVistaAndLater || script->TargetPathSupEFS) &&  // we break regardless of the system and EFS support; originally: you can't encrypt a directory on FAT until Vista, we behave the same (for comparison with Explorer, the Encrypted attribute is not that important)
                                    !dlgData.DirCrLossEncrAll && changeAttrErr != ERROR_SUCCESS)
                                {                                                              // Failed to set the Encrypted attribute for the directory, we will ask the user what to do about it
                                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                    if (*dlgData.CancelWorker)
                                        goto CANCEL_CRDIR;

                                    int ret;
                                    if (dlgData.SkipAllDirCrLossEncr)
                                        ret = IDB_SKIP;
                                    else
                                    {
                                        ret = IDCANCEL;
                                        char* data[4];
                                        data[0] = (char*)&ret;
                                        data[1] = (char*)FALSE;
                                        data[2] = name;
                                        data[3] = (char*)(!script->IsCopyOperation);
                                        SendMessage(hProgressDlg, WM_USER_DIALOG, 12, (LPARAM)data);
                                    }
                                    switch (ret)
                                    {
                                    case IDB_ALL:
                                        dlgData.DirCrLossEncrAll = TRUE; // here break; is not missing
                                    case IDYES:
                                        break;

                                    case IDB_SKIPALL:
                                        dlgData.SkipAllDirCrLossEncr = TRUE;
                                    case IDB_SKIP:
                                    {
                                        ClearReadOnlyAttr(nameCrDir); // to be able to delete the file, it must not have the read-only attribute
                                        RemoveDirectory(nameCrDir);
                                        script->SetTFS(lastTransferredFileSize); // TFS se pricte az za kompletni adresar venku; ProgressSize se srovna az venku (tady nema smysl ho rovnat)
                                        skip = TRUE;
                                        return TRUE;
                                    }

                                    case IDCANCEL:
                                        goto CANCEL_CRDIR;
                                    }
                                }
                            }
                            else
                                changeAttrErr = MyDecryptFile(name, currentAttrs, FALSE);
                        }
                        if (changeAttrErr == NO_ERROR &&
                            (newAttr & FILE_ATTRIBUTE_COMPRESSED) != (currentAttrs & FILE_ATTRIBUTE_COMPRESSED) &&
                            (newAttr & FILE_ATTRIBUTE_COMPRESSED) != 0)
                        {
                            changeAttrErr = CompressFile(name, currentAttrs);
                        }
                    }
                    else
                        changeAttrErr = GetLastError();
                    if (changeAttrErr != NO_ERROR)
                    {
                        TRACE_I("DoCreateDir(): Unable to set Encrypted or Compressed attributes for " << name << "! error=" << GetErrorText(changeAttrErr));
                    }
                }
                SetFileAttributes(nameCrDir, newAttr);

                if (script->CopyAttrs) // we will check if the attributes of the source file were preserved
                {
                    DWORD curAttrs;
                    curAttrs = SalGetFileAttributes(name);
                    if (curAttrs == INVALID_FILE_ATTRIBUTES || (curAttrs & DISPLAYED_ATTRIBUTES) != (newAttr & DISPLAYED_ATTRIBUTES))
                    {                                                              // Attributes probably failed to transfer, warning the user
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                        if (*dlgData.CancelWorker)
                            goto CANCEL_CRDIR;

                        int ret;
                        ret = IDCANCEL;
                        if (dlgData.IgnoreAllSetAttrsErr)
                            ret = IDB_IGNORE;
                        else
                        {
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = name;
                            data[2] = (char*)(DWORD_PTR)(newAttr & DISPLAYED_ATTRIBUTES);
                            data[3] = (char*)(DWORD_PTR)(curAttrs == INVALID_FILE_ATTRIBUTES ? 0 : (curAttrs & DISPLAYED_ATTRIBUTES));
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 9, (LPARAM)data);
                        }
                        switch (ret)
                        {
                        case IDB_IGNOREALL:
                            dlgData.IgnoreAllSetAttrsErr = TRUE; // here break; is not missing
                        case IDB_IGNORE:
                            break;

                        case IDCANCEL:
                        {
                        CANCEL_CRDIR:

                            ClearReadOnlyAttr(nameCrDir); // to be able to delete the file, it must not have the read-only attribute
                            RemoveDirectory(nameCrDir);
                            return FALSE;
                        }
                        }
                    }
                }

                if (sourceDir != NULL && script->CopySecurity) // Should we copy NTFS security permissions?
                {
                    DWORD err2;
                    if (!DoCopySecurity(sourceDir, name, &err2, NULL))
                    {
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                        if (*dlgData.CancelWorker)
                            goto CANCEL_CRDIR;

                        int ret;
                        ret = IDCANCEL;
                        if (dlgData.IgnoreAllCopyPermErr)
                            ret = IDB_IGNORE;
                        else
                        {
                            char* data[4];
                            data[0] = (char*)&ret;
                            data[1] = (char*)sourceDir;
                            data[2] = name;
                            data[3] = (char*)(DWORD_PTR)err2;
                            SendMessage(hProgressDlg, WM_USER_DIALOG, 10, (LPARAM)data);
                        }
                        switch (ret)
                        {
                        case IDB_IGNOREALL:
                            dlgData.IgnoreAllCopyPermErr = TRUE; // here break; is not missing
                        case IDB_IGNORE:
                            break;

                        case IDCANCEL:
                            goto CANCEL_CRDIR;
                        }
                    }
                }
            }
            return TRUE;
        }
        else
        {
            if (invalidName)
                err = ERROR_INVALID_NAME;
            if (err == ERROR_ALREADY_EXISTS ||
                err == ERROR_FILE_EXISTS)
            {
                DWORD attr2 = SalGetFileAttributes(name);
                if (attr2 & FILE_ATTRIBUTE_DIRECTORY) // "directory overwrite"
                {
                    if (dlgData.CnfrmDirOver && !dlgData.DirOverwriteAll) // Should we ask the user to overwrite the directory?
                    {
                        char sAttr[101], tAttr[101];
                        GetDirInfo(sAttr, sourceDir);
                        GetDirInfo(tAttr, name);

                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                        if (*dlgData.CancelWorker)
                            return FALSE;

                        if (dlgData.SkipAllDirOver)
                            goto SKIP_CREATE_ERROR;

                        int ret = IDCANCEL;
                        char* data[5];
                        data[0] = (char*)&ret;
                        data[1] = name;
                        data[2] = tAttr;
                        data[3] = (char*)sourceDir;
                        data[4] = sAttr;
                        SendMessage(hProgressDlg, WM_USER_DIALOG, 7, (LPARAM)data);
                        switch (ret)
                        {
                        case IDB_ALL:
                            dlgData.DirOverwriteAll = TRUE;
                        case IDYES:
                            break;

                        case IDB_SKIPALL:
                            dlgData.SkipAllDirOver = TRUE;
                        case IDB_SKIP:
                            goto SKIP_CREATE_ERROR;

                        case IDCANCEL:
                            return FALSE;
                        }
                    }
                    alreadyExisted = TRUE;
                    return TRUE; // o.k.
                }

                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                if (*dlgData.CancelWorker)
                    return FALSE;

                if (dlgData.SkipAllDirCreate)
                    goto SKIP_CREATE_ERROR;

                int ret = IDCANCEL;
                char* data[4];
                data[0] = (char*)&ret;
                data[1] = LoadStr(IDS_ERRORCREATINGDIR);
                data[2] = name;
                data[3] = LoadStr(IDS_NAMEALREADYUSED);
                SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                switch (ret)
                {
                case IDRETRY:
                    break;

                case IDB_SKIPALL:
                    dlgData.SkipAllDirCreate = TRUE;
                case IDB_SKIP:
                    goto SKIP_CREATE_ERROR;

                case IDCANCEL:
                    return FALSE;
                }
                continue;
            }

            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (dlgData.SkipAllDirCreateErr)
                goto SKIP_CREATE_ERROR;

            int ret;
            ret = IDCANCEL;
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = LoadStr(IDS_ERRORCREATINGDIR);
            data[2] = name;
            data[3] = GetErrorText(err);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
            switch (ret)
            {
            case IDRETRY:
                break;

            case IDB_SKIPALL:
                dlgData.SkipAllDirCreateErr = TRUE;
            case IDB_SKIP:
            {
            SKIP_CREATE_ERROR:

                skip = TRUE; // It's about skipping (all operations in the directory must be skipped)
                return TRUE;
            }
            case IDCANCEL:
                return FALSE;
            }
        }
    }
}

BOOL DoDeleteDir(HWND hProgressDlg, char* name, const CQuadWord& size, COperations* script,
                 CQuadWord& totalDone, DWORD attr, BOOL dontUseRecycleBin, CProgressDlgData& dlgData)
{
    DWORD err;
    int AutoRetryCounter = 0;
    DWORD startTime = GetTickCount();

    // if the path ends with a space/dot, we need to append '\\', otherwise SetFileAttributes
    // i RemoveDirectory trims spaces/dots and thus works with a different path
    const char* nameRmDir = name;
    char nameRmDirCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(nameRmDir, nameRmDirCopy);

    while (1)
    {
        ClearReadOnlyAttr(nameRmDir, attr); // to be deleted ...

        err = ERROR_SUCCESS;
        if (script->CanUseRecycleBin && !dontUseRecycleBin &&
            (script->InvertRecycleBin && dlgData.UseRecycleBin == 0 ||
             !script->InvertRecycleBin && dlgData.UseRecycleBin == 1) &&
            IsDirectoryEmpty(name)) // subdirectory must not contain any files !!!
        {
            char nameList[MAX_PATH + 1];
            int l = (int)strlen(name) + 1;
            memmove(nameList, name, l);
            nameList[l] = 0;
            if (!PathContainsValidComponents(nameList, FALSE))
            {
                err = ERROR_INVALID_NAME;
            }
            else
            {
                CShellExecuteWnd shellExecuteWnd;
                SHFILEOPSTRUCT opCode;
                memset(&opCode, 0, sizeof(opCode));

                opCode.hwnd = shellExecuteWnd.Create(hProgressDlg, "SEW: DoDeleteDir");

                opCode.wFunc = FO_DELETE;
                opCode.pFrom = nameList;
                opCode.fFlags = FOF_ALLOWUNDO | FOF_SILENT | FOF_NOCONFIRMATION;
                opCode.lpszProgressTitle = "";
                err = SHFileOperation(&opCode);
            }
        }
        else
        {
            if (RemoveDirectory(nameRmDir) == 0)
                err = GetLastError();
        }

        if (err == ERROR_SUCCESS)
        {
            script->AddBytesToSpeedMetersAndTFSandPS((DWORD)size.Value, TRUE, 0, NULL, MAX_OP_FILESIZE);

            totalDone += size;
            SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
            return TRUE;
        }
        else
        {
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (dlgData.SkipAllDeleteErr)
                goto SKIP_DELETE;

            if (AutoRetryCounter < 4 && GetTickCount() - startTime + (AutoRetryCounter + 1) * 100 <= 2000 &&
                (err == ERROR_DIR_NOT_EMPTY || err == ERROR_SHARING_VIOLATION))
            { // auto-retry is used here to handle this situation: I have directories 1\2\3, I delete 1 including its subdirectories, 3 is in the panel (changes are being monitored in it) -> when deleting 2, an error "deleting a non-empty directory" is reported because 3 remains in an "intermediate state" due to change monitoring (even though it is deleted, so it cannot be listed, etc., but it still exists on the disk, a tricky situation)
                //        TRACE_I("DoDeleteDir(): err: " << GetErrorText(err));
                AutoRetryCounter++;
                Sleep(AutoRetryCounter * 100);
                //        TRACE_I("DoDeleteDir(): " << AutoRetryCounter << ". retry, delay is " << AutoRetryCounter * 100 << "ms");
            }
            else
            {
                int ret;
                ret = IDCANCEL;
                char* data[4];
                data[0] = (char*)&ret;
                data[1] = LoadStr(IDS_ERRORDELETINGDIR);
                data[2] = (char*)nameRmDir;
                data[3] = GetErrorText(err);
                SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                switch (ret)
                {
                case IDRETRY:
                    break;

                case IDB_SKIPALL:
                    dlgData.SkipAllDeleteErr = TRUE;
                case IDB_SKIP:
                {
                SKIP_DELETE:

                    totalDone += size;
                    script->SetProgressSize(totalDone);
                    SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                    return TRUE;
                }

                case IDCANCEL:
                    return FALSE;
                }
            }
        }

        DWORD attr2 = SalGetFileAttributes(nameRmDir); // get the current state of the attribute
        if (attr2 != INVALID_FILE_ATTRIBUTES)
            attr = attr2;
    }
}

#define FSCTL_GET_REPARSE_POINT CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 42, METHOD_BUFFERED, FILE_ANY_ACCESS)        // REPARSE_DATA_BUFFER
#define FSCTL_DELETE_REPARSE_POINT CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 43, METHOD_BUFFERED, FILE_SPECIAL_ACCESS) // REPARSE_DATA_BUFFER,

#define IO_REPARSE_TAG_SYMLINK (0xA000000CL)

/*  tenhle kod zkopiruje junction point do prazdneho adresare (je potreba predem vytvorit - zde z uspornych
    duvodu vzdycky "D:\\ZUMPA\\link")

  lidi chteji obcas kopirovat obsah junction pointu, obcas chteji kopirovat junction jen jako link,
  obcas to chteji skipnout (nevim jestli s vytvorenim pradneho adresare junctionu nebo bez) ... takze
  kdyby se melo jednou psat, tak bude potreba udelat na to pri buildu skriptu velky dotazovaci dialog

#define FSCTL_SET_REPARSE_POINT     CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 41, METHOD_BUFFERED, FILE_SPECIAL_ACCESS) // REPARSE_DATA_BUFFER,
#define FSCTL_GET_REPARSE_POINT     CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 42, METHOD_BUFFERED, FILE_ANY_ACCESS) // REPARSE_DATA_BUFFER

// Structure for FSCTL_SET_REPARSE_POINT, FSCTL_GET_REPARSE_POINT, and
// FSCTL_DELETE_REPARSE_POINT.
// This version of the reparse data buffer is only for Microsoft tags.

struct TMN_REPARSE_DATA_BUFFER
{
  DWORD ReparseTag;
  WORD  ReparseDataLength;
  WORD  Reserved;
  WORD  SubstituteNameOffset;
  WORD  SubstituteNameLength;
  WORD  PrintNameOffset;
  WORD  PrintNameLength;
  WCHAR PathBuffer[1];
};

#define IO_REPARSE_TAG_VALID_VALUES 0xE000FFFF
#define IsReparseTagValid(x) (!((x)&~IO_REPARSE_TAG_VALID_VALUES)&&((x)>IO_REPARSE_TAG_RESERVED_RANGE))
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE      ( 16 * 1024 )


  HANDLE srcDir = HANDLES_Q(CreateFile(name, GENERIC_READ, 0, 0, OPEN_EXISTING,
                                       FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL));
  if (srcDir != INVALID_HANDLE_VALUE)
  {
    HANDLE tgtDir = HANDLES_Q(CreateFile("D:\\ZUMPA\\link", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL));
    if (tgtDir != INVALID_HANDLE_VALUE)
    {
      char szBuff[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
      TMN_REPARSE_DATA_BUFFER& rdb = *(TMN_REPARSE_DATA_BUFFER*)szBuff;

      DWORD dwBytesReturned;
      if (DeviceIoControl(srcDir, FSCTL_GET_REPARSE_POINT, NULL, 0, (LPVOID)&rdb,
                          MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwBytesReturned, 0) &&
          IsReparseTagValid(rdb.ReparseTag))
      {
        DWORD dwBytesReturnedDummy;
        if (DeviceIoControl(tgtDir, FSCTL_SET_REPARSE_POINT, (LPVOID)&rdb, dwBytesReturned,
                            NULL, 0, &dwBytesReturnedDummy, 0))
        {
          TRACE_I("eureka?");
        }
      }
      HANDLES(CloseHandle(tgtDir));
    }
    HANDLES(CloseHandle(srcDir));
  }
  return FALSE;
*/

BOOL DoDeleteDirLinkAux(const char* nameDelLink, DWORD* err)
{
    // delete reparse point from directory 'nameDelLink'
    if (err != NULL)
        *err = ERROR_SUCCESS;
    BOOL ok = FALSE;
    DWORD attr = GetFileAttributes(nameDelLink);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
    {
        HANDLE dir = HANDLES_Q(CreateFile(nameDelLink, GENERIC_WRITE /* | GENERIC_READ*/, 0, 0, OPEN_EXISTING,
                                          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL));
        if (dir != INVALID_HANDLE_VALUE)
        {
            DWORD dummy;
            char buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
            REPARSE_GUID_DATA_BUFFER* juncData = (REPARSE_GUID_DATA_BUFFER*)buf;
            if (DeviceIoControl(dir, FSCTL_GET_REPARSE_POINT, NULL, 0, juncData,
                                MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dummy, NULL) == 0)
            {
                if (err != NULL)
                    *err = GetLastError();
            }
            else
            {
                if (juncData->ReparseTag != IO_REPARSE_TAG_MOUNT_POINT &&
                    juncData->ReparseTag != IO_REPARSE_TAG_SYMLINK)
                { // if it's not a volume mount point, junction, or symlink, we report an error (we probably know how to delete, but we'd rather refuse to avoid messing something up...)
                    TRACE_E("DoDeleteDirLinkAux(): Unknown type of reparse point (tag is 0x" << std::hex << juncData->ReparseTag << std::dec << "): " << nameDelLink);
                    if (err != NULL)
                        *err = 4394 /* ERROR_REPARSE_TAG_MISMATCH*/;
                }
                else
                {
                    REPARSE_GUID_DATA_BUFFER rgdb = {0};
                    rgdb.ReparseTag = juncData->ReparseTag;

                    DWORD dwBytes;
                    if (DeviceIoControl(dir, FSCTL_DELETE_REPARSE_POINT, &rgdb, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE,
                                        NULL, 0, &dwBytes, 0) != 0)
                    {
                        ok = TRUE;
                    }
                    else
                    {
                        if (err != NULL)
                            *err = GetLastError();
                    }
                }
            }
            HANDLES(CloseHandle(dir));
        }
        else
        {
            if (err != NULL)
                *err = GetLastError();
        }
    }
    else
        ok = TRUE; // the reparse point is now apparently deleted, only the empty directory remains to be deleted...
    // remove empty directory (left after deleting the reparse point)
    if (ok)
        ClearReadOnlyAttr(nameDelLink, attr); // to be able to delete with the read-only attribute
    if (ok && !RemoveDirectory(nameDelLink))
    {
        ok = FALSE;
        if (err != NULL)
            *err = GetLastError();
    }
    return ok;
}

BOOL DeleteDirLink(const char* name, DWORD* err)
{
    // if the path ends with a space/dot, we need to append '\\', otherwise CreateFile
    // i RemoveDirectory trims spaces/dots and thus works with a different path
    const char* nameDelLink = name;
    char nameDelLinkCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(nameDelLink, nameDelLinkCopy);

    return DoDeleteDirLinkAux(nameDelLink, err);
}

BOOL DoDeleteDirLink(HWND hProgressDlg, char* name, const CQuadWord& size, COperations* script,
                     CQuadWord& totalDone, CProgressDlgData& dlgData)
{
    // if the path ends with a space/dot, we need to append '\\', otherwise CreateFile
    // i RemoveDirectory trims spaces/dots and thus works with a different path
    const char* nameDelLink = name;
    char nameDelLinkCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(nameDelLink, nameDelLinkCopy);

    while (1)
    {
        DWORD err;
        BOOL ok = DoDeleteDirLinkAux(nameDelLink, &err);

        if (ok)
        {
            script->AddBytesToSpeedMetersAndTFSandPS((DWORD)size.Value, TRUE, 0, NULL, MAX_OP_FILESIZE);

            totalDone += size;
            SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
            return TRUE;
        }
        else
        {
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (dlgData.SkipAllDeleteErr)
                goto SKIP_DELETE_LINK;

            int ret;
            ret = IDCANCEL;
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = LoadStr(IDS_ERRORDELETINGDIRLINK);
            data[2] = (char*)nameDelLink;
            data[3] = GetErrorText(err);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
            switch (ret)
            {
            case IDRETRY:
                break;

            case IDB_SKIPALL:
                dlgData.SkipAllDeleteErr = TRUE;
            case IDB_SKIP:
            {
            SKIP_DELETE_LINK:

                totalDone += size;
                script->SetProgressSize(totalDone);
                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                return TRUE;
            }

            case IDCANCEL:
                return FALSE;
            }
        }
    }
}

// a) ve stejnem adresari, jako je soubor name vytvori docasny soubor
// b) transfers the content of the file name to a temporary file and simultaneously processes the data
//    performs conversion specified by variables convertData.CodeType and convertData.EOFType
// c) file name will be overwritten by a temporary file
//
// convertData.EOFType - specifies what will replace end of line characters
//            All CR, LF, and CRLF are considered end of line
//            0: no line ending replacement is performed
//            1: end of line will be replaced by CRLF (DOS, Windows, OS/2)
//            2: end of line is replaced by LF (UNIX)
//            3: end of line will be replaced by CR (MAC)
BOOL DoConvert(HWND hProgressDlg, char* name, char* sourceBuffer, char* targetBuffer,
               const CQuadWord& size, COperations* script, CQuadWord& totalDone,
               CConvertData& convertData, CProgressDlgData& dlgData)
{
    // if the path ends with a space/dot, it is invalid and we must not perform the conversion,
    // CreateFile trims spaces/dots and converts another file accordingly
    BOOL invalidName = FileNameIsInvalid(name, TRUE);

CONVERT_AGAIN:

    CQuadWord operationDone;
    operationDone = CQuadWord(0, 0);
    while (1)
    {
        // trying to open the source file
        HANDLE hSource;
        if (!invalidName)
        {
            hSource = HANDLES_Q(CreateFile(name, GENERIC_READ,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                           OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
        }
        else
        {
            hSource = INVALID_HANDLE_VALUE;
        }
        if (hSource != INVALID_HANDLE_VALUE)
        {
            // Retrieve a path for a temporary file
            char tmpPath[MAX_PATH];
            strcpy(tmpPath, name);
            char* terminator = strrchr(tmpPath, '\\');
            if (terminator == NULL)
            {
                // working test
                TRACE_E("Parameter 'name' must be full path to file (including path)");
                HANDLES(CloseHandle(hSource));
                return FALSE;
            }
            *(terminator + 1) = 0;

            // find a name for a temporary file and create the file
            char tmpFileName[MAX_PATH];
            BOOL tmpFileExists = FALSE;
            while (1)
            {
                if (SalGetTempFileName(tmpPath, "cnv", tmpFileName, TRUE))
                {
                    tmpFileExists = TRUE;

                    // set the attributes of the temporary file according to the source file
                    DWORD srcAttrs = SalGetFileAttributes(name);
                    DWORD tgtAttrs = SalGetFileAttributes(tmpFileName);
                    BOOL changeAttrs = FALSE;
                    if (srcAttrs != INVALID_FILE_ATTRIBUTES && tgtAttrs != INVALID_FILE_ATTRIBUTES && srcAttrs != tgtAttrs)
                    {
                        changeAttrs = TRUE; // later we will perform SetFileAttributes...
                        // Is the compression attribute different on NTFS?
                        if ((srcAttrs & FILE_ATTRIBUTE_COMPRESSED) != (tgtAttrs & FILE_ATTRIBUTE_COMPRESSED) &&
                            (srcAttrs & FILE_ATTRIBUTE_COMPRESSED) == 0)
                        {
                            UncompressFile(tmpFileName, tgtAttrs);
                        }
                        if ((srcAttrs & FILE_ATTRIBUTE_ENCRYPTED) != (tgtAttrs & FILE_ATTRIBUTE_ENCRYPTED))
                        {
                            BOOL cancelOper = FALSE;
                            if (srcAttrs & FILE_ATTRIBUTE_ENCRYPTED)
                            {
                                MyEncryptFile(hProgressDlg, tmpFileName, tgtAttrs, 0 /* to be able to encrypt files with the SYSTEM attribute*/,
                                              dlgData, cancelOper, FALSE);
                            }
                            else
                                MyDecryptFile(tmpFileName, tgtAttrs, FALSE);
                            if (*dlgData.CancelWorker || cancelOper)
                            {
                                HANDLES(CloseHandle(hSource));
                                ClearReadOnlyAttr(tmpFileName); // to be deleted
                                DeleteFile(tmpFileName);
                                return FALSE;
                            }
                        }
                        if ((srcAttrs & FILE_ATTRIBUTE_COMPRESSED) != (tgtAttrs & FILE_ATTRIBUTE_COMPRESSED) &&
                            (srcAttrs & FILE_ATTRIBUTE_COMPRESSED) != 0)
                        {
                            CompressFile(tmpFileName, tgtAttrs);
                        }
                    }

                    // open an empty temporary file
                    HANDLE hTarget = HANDLES_Q(CreateFile(tmpFileName, GENERIC_WRITE, 0, NULL,
                                                          OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                    if (hTarget != INVALID_HANDLE_VALUE)
                    {
                        DWORD read;
                        BOOL crlfBreak = FALSE;
                        while (1)
                        {
                            if (ReadFile(hSource, sourceBuffer, OPERATION_BUFFER, &read, NULL))
                            {
                                DWORD written;
                                if (read == 0)
                                    break;                                                 // EOF
                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                if (*dlgData.CancelWorker)
                                {
                                CONVERT_ERROR:

                                    if (hSource != NULL)
                                        HANDLES(CloseHandle(hSource));
                                    if (hTarget != NULL)
                                        HANDLES(CloseHandle(hTarget));
                                    ClearReadOnlyAttr(tmpFileName); // to be deleted
                                    DeleteFile(tmpFileName);
                                    return FALSE;
                                }

                                // perform translation from sourceBuffer to targetBuffer
                                char* sourceIterator;
                                char* targetIterator;
                                sourceIterator = sourceBuffer;
                                targetIterator = targetBuffer;
                                while (sourceIterator - sourceBuffer < (int)read)
                                {
                                    // lastChar is TRUE if sourceIterator points to the last character in the buffer
                                    BOOL lastChar = (sourceIterator - sourceBuffer == (int)read - 1);

                                    if (convertData.EOFType != 0)
                                    {
                                        if (crlfBreak && sourceIterator == sourceBuffer && *sourceIterator == '\n')
                                        {
                                            // we have already processed this CRLF, now let LF be
                                            crlfBreak = FALSE;
                                        }
                                        else
                                        {
                                            if (*sourceIterator == '\r' || *sourceIterator == '\n')
                                            {
                                                switch (convertData.EOFType)
                                                {
                                                case 2:
                                                    *targetIterator++ = convertData.CodeTable['\n'];
                                                    break;
                                                case 3:
                                                    *targetIterator++ = convertData.CodeTable['\r'];
                                                    break;
                                                default:
                                                {
                                                    *targetIterator++ = convertData.CodeTable['\r'];
                                                    *targetIterator++ = convertData.CodeTable['\n'];
                                                    break;
                                                }
                                                }
                                                // catch CRLF that breaks at the buffer interface
                                                if (lastChar && *sourceIterator == '\r')
                                                    crlfBreak = TRUE;
                                                // catch CRLF that doesn't break - I have to skip LF
                                                if (!lastChar &&
                                                    *sourceIterator == '\r' && *(sourceIterator + 1) == '\n')
                                                    sourceIterator++;
                                            }
                                            else
                                            {
                                                *targetIterator = convertData.CodeTable[*sourceIterator];
                                                targetIterator++;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        *targetIterator = convertData.CodeTable[*sourceIterator];
                                        targetIterator++;
                                    }
                                    sourceIterator++;
                                }

                                // we will write to the tmp file
                                while (1)
                                {
                                    if (WriteFile(hTarget, targetBuffer, (DWORD)(targetIterator - targetBuffer), &written, NULL) &&
                                        targetIterator - targetBuffer == (int)written)
                                        break;

                                WRITE_ERROR_CONVERT:

                                    DWORD err;
                                    err = GetLastError();

                                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                    if (*dlgData.CancelWorker)
                                        goto CONVERT_ERROR;

                                    if (dlgData.SkipAllFileWrite)
                                        goto SKIP_CONVERT;

                                    int ret;
                                    ret = IDCANCEL;
                                    char* data[4];
                                    data[0] = (char*)&ret;
                                    data[1] = LoadStr(IDS_ERRORWRITINGFILE);
                                    data[2] = tmpFileName;
                                    if (hTarget != NULL && err == NO_ERROR && targetIterator - targetBuffer != (int)written)
                                        err = ERROR_DISK_FULL;
                                    data[3] = GetErrorText(err);
                                    SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                                    switch (ret)
                                    {
                                    case IDRETRY:
                                    {
                                        if (hSource == NULL && hTarget == NULL)
                                        {
                                            ClearReadOnlyAttr(tmpFileName); // to be deleted
                                            DeleteFile(tmpFileName);
                                            SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                                            goto CONVERT_AGAIN;
                                        }
                                        break;
                                    }

                                    case IDB_SKIPALL:
                                        dlgData.SkipAllFileWrite = TRUE;
                                    case IDB_SKIP:
                                    {
                                    SKIP_CONVERT:

                                        totalDone += size;
                                        if (hSource != NULL)
                                            HANDLES(CloseHandle(hSource));
                                        if (hTarget != NULL)
                                            HANDLES(CloseHandle(hTarget));
                                        ClearReadOnlyAttr(tmpFileName); // to be deleted
                                        DeleteFile(tmpFileName);
                                        SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                                        return TRUE;
                                    }

                                    case IDCANCEL:
                                        goto CONVERT_ERROR;
                                    }
                                }
                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                if (*dlgData.CancelWorker)
                                    goto CONVERT_ERROR;

                                operationDone += CQuadWord(read, 0);
                                SetProgress(hProgressDlg,
                                            CaclProg(operationDone, size),
                                            CaclProg(totalDone + operationDone, script->TotalSize), dlgData);
                            }
                            else
                            {
                                DWORD err = GetLastError();
                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                if (*dlgData.CancelWorker)
                                    goto CONVERT_ERROR;

                                if (dlgData.SkipAllFileRead)
                                    goto SKIP_CONVERT;

                                int ret = IDCANCEL;
                                char* data[4];
                                data[0] = (char*)&ret;
                                data[1] = LoadStr(IDS_ERRORREADINGFILE);
                                data[2] = name;
                                data[3] = GetErrorText(err);
                                SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                                switch (ret)
                                {
                                case IDRETRY:
                                    break;
                                case IDB_SKIPALL:
                                    dlgData.SkipAllFileRead = TRUE;
                                case IDB_SKIP:
                                    goto SKIP_CONVERT;
                                case IDCANCEL:
                                    goto CONVERT_ERROR;
                                }
                            }
                        }
                        // close files and set global progress
                        // we will not use operationDone to ensure correct progress even when files are "moving under our feet"
                        HANDLES(CloseHandle(hSource));
                        if (!HANDLES(CloseHandle(hTarget))) // Even after an unsuccessful call, we assume that the handle is closed,
                        {                                   // see https://forum.altap.cz/viewtopic.php?f=6&t=8455
                            hSource = hTarget = NULL;       // (it says that the target file can be deleted, so its handle was not left open)
                            goto WRITE_ERROR_CONVERT;
                        }
                        totalDone += size;
                        // we will not set attributes (read-only causes problems when writing)
                        if (changeAttrs)
                            SetFileAttributes(tmpFileName, srcAttrs);
                        // we will overwrite the original file with a temporary file
                        while (1)
                        {
                            ClearReadOnlyAttr(name); // to be deleted ...
                            if (DeleteFile(name))
                            {
                                while (1)
                                {
                                    if (SalMoveFile(tmpFileName, name))
                                        return TRUE; // successfully finished
                                    else
                                    {
                                        DWORD err = GetLastError();

                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                        if (*dlgData.CancelWorker)
                                            return FALSE;

                                        if (dlgData.SkipAllMoveErrors)
                                            return TRUE;

                                        int ret = IDCANCEL;
                                        char* data[4];
                                        data[0] = (char*)&ret;
                                        data[1] = tmpFileName;
                                        data[2] = name;
                                        data[3] = GetErrorText(err);
                                        SendMessage(hProgressDlg, WM_USER_DIALOG, 3, (LPARAM)data);
                                        switch (ret)
                                        {
                                        case IDRETRY:
                                            break;

                                        case IDB_SKIPALL:
                                            dlgData.SkipAllMoveErrors = TRUE;
                                        case IDB_SKIP:
                                            return TRUE;

                                        case IDCANCEL:
                                            return FALSE;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                DWORD err = GetLastError();

                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                                if (*dlgData.CancelWorker)
                                {
                                CANCEL_CONVERT:

                                    ClearReadOnlyAttr(tmpFileName); // to be deleted ...
                                    DeleteFile(tmpFileName);
                                    return FALSE;
                                }

                                if (dlgData.SkipAllOverwriteErr)
                                    goto SKIP_OVERWRITE_ERROR;

                                int ret;
                                ret = IDCANCEL;
                                char* data[4];
                                data[0] = (char*)&ret;
                                data[1] = LoadStr(IDS_ERROROVERWRITINGFILE);
                                data[2] = name;
                                data[3] = GetErrorText(err);
                                SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                                switch (ret)
                                {
                                case IDRETRY:
                                    break;

                                case IDB_SKIPALL:
                                    dlgData.SkipAllOverwriteErr = TRUE;
                                case IDB_SKIP:
                                {
                                SKIP_OVERWRITE_ERROR:

                                    ClearReadOnlyAttr(tmpFileName); // to be deleted ...
                                    DeleteFile(tmpFileName);
                                    return TRUE;
                                }

                                case IDCANCEL:
                                    goto CANCEL_CONVERT;
                                }
                            }
                        }
                    }
                    else
                        goto TMP_OPEN_ERROR;
                }
                else
                {
                TMP_OPEN_ERROR:

                    DWORD err = GetLastError();

                    char fakeName[MAX_PATH]; // name of temporary file that cannot be created/opened
                    if (tmpFileExists)
                    {
                        strcpy(fakeName, tmpFileName);
                        ClearReadOnlyAttr(tmpFileName); // to be deleted
                        DeleteFile(tmpFileName);        // tmp was created, we will try to delete it ...
                        tmpFileExists = FALSE;
                    }
                    else
                    {
                        // put together a meaningful name for the tmp file that failed to be created
                        char* s = tmpPath + strlen(tmpPath);
                        if (s > tmpPath && *(s - 1) == '\\')
                            s--;
                        memcpy(fakeName, tmpPath, s - tmpPath);
                        strcpy(fakeName + (s - tmpPath), "\\cnv0000.tmp");
                    }

                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
                    if (*dlgData.CancelWorker)
                        goto CANCEL_OPEN2;

                    if (dlgData.SkipAllFileOpenOut)
                        goto SKIP_OPEN_OUT;

                    int ret;
                    ret = IDCANCEL;
                    char* data[4];
                    data[0] = (char*)&ret;
                    data[1] = LoadStr(IDS_ERRORCREATINGTMPFILE);
                    data[2] = fakeName;
                    data[3] = GetErrorText(err);
                    SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
                    switch (ret)
                    {
                    case IDRETRY:
                        break;

                    case IDB_SKIPALL:
                        dlgData.SkipAllFileOpenOut = TRUE;
                    case IDB_SKIP:
                    {
                    SKIP_OPEN_OUT:

                        HANDLES(CloseHandle(hSource));
                        totalDone += size;
                        SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                        return TRUE;
                    }

                    case IDCANCEL:
                    {
                    CANCEL_OPEN2:

                        HANDLES(CloseHandle(hSource));
                        return FALSE;
                    }
                    }
                }
            }
        }
        else
        {
            DWORD err = GetLastError();
            if (invalidName)
                err = ERROR_INVALID_NAME;
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (dlgData.SkipAllFileOpenIn)
                goto SKIP_OPEN_IN;

            int ret;
            ret = IDCANCEL;
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = LoadStr(IDS_ERROROPENINGFILE);
            data[2] = name;
            data[3] = GetErrorText(err);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
            switch (ret)
            {
            case IDRETRY:
                break;

            case IDB_SKIPALL:
                dlgData.SkipAllFileOpenIn = TRUE;
            case IDB_SKIP:
            {
            SKIP_OPEN_IN:

                totalDone += size;
                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                return TRUE;
            }

            case IDCANCEL:
                return FALSE;
            }
        }
    }
}

BOOL DoChangeAttrs(HWND hProgressDlg, char* name, const CQuadWord& size, DWORD attrs,
                   COperations* script, CQuadWord& totalDone,
                   FILETIME* timeModified, FILETIME* timeCreated, FILETIME* timeAccessed,
                   BOOL& changeCompression, BOOL& changeEncryption, DWORD fileAttr,
                   CProgressDlgData& dlgData)
{
    // if the path ends with a space/dot, we need to append '\\', otherwise
    // SetFileAttributes (and others) trims spaces/dots and works
    // so with another way
    const char* nameSetAttrs = name;
    char nameSetAttrsCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(nameSetAttrs, nameSetAttrsCopy);

    while (1)
    {
        DWORD error = ERROR_SUCCESS;
        BOOL showCompressErr = FALSE;
        BOOL showEncryptErr = FALSE;
        char* errTitle = NULL;
        if (changeCompression && (attrs & FILE_ATTRIBUTE_COMPRESSED) == 0)
        {
            error = UncompressFile(name, fileAttr);
            if (error != ERROR_SUCCESS)
            {
                errTitle = LoadStr(IDS_ERRORCOMPRESSING);
                if (error == ERROR_INVALID_FUNCTION)
                    showCompressErr = TRUE; // not supported
            }
        }
        if (error == ERROR_SUCCESS && changeEncryption && (attrs & FILE_ATTRIBUTE_ENCRYPTED) == 0)
        {
            error = MyDecryptFile(name, fileAttr, TRUE);
            if (error != ERROR_SUCCESS)
            {
                errTitle = LoadStr(IDS_ERRORENCRYPTING);
                if (error == ERROR_INVALID_FUNCTION)
                    showEncryptErr = TRUE; // not supported
            }
        }
        if (error == ERROR_SUCCESS && changeCompression && (attrs & FILE_ATTRIBUTE_COMPRESSED))
        {
            error = CompressFile(name, fileAttr);
            if (error != ERROR_SUCCESS)
            {
                errTitle = LoadStr(IDS_ERRORCOMPRESSING);
                if (error == ERROR_INVALID_FUNCTION)
                    showCompressErr = TRUE; // not supported
            }
        }
        if (error == ERROR_SUCCESS && changeEncryption && (attrs & FILE_ATTRIBUTE_ENCRYPTED))
        {
            BOOL cancelOper = FALSE;
            error = MyEncryptFile(hProgressDlg, name, fileAttr, attrs, dlgData, cancelOper, TRUE);
            if (*dlgData.CancelWorker || cancelOper)
                return FALSE;
            if (error != ERROR_SUCCESS)
            {
                errTitle = LoadStr(IDS_ERRORENCRYPTING);
                if (error == ERROR_INVALID_FUNCTION)
                    showEncryptErr = TRUE; // not supported
            }
        }
        if (showCompressErr || showEncryptErr)
        {
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (showCompressErr)
                changeCompression = FALSE;
            if (showEncryptErr)
                changeEncryption = FALSE;
            char* data[3];
            data[0] = LoadStr((showCompressErr && (attrs & FILE_ATTRIBUTE_COMPRESSED) || !showEncryptErr) ? IDS_ERRORCOMPRESSING : IDS_ERRORENCRYPTING);
            data[1] = name;
            data[2] = LoadStr((showCompressErr && (attrs & FILE_ATTRIBUTE_COMPRESSED) || !showEncryptErr) ? IDS_COMPRNOTSUPPORTED : IDS_ENCRYPNOTSUPPORTED);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 5, (LPARAM)data);
            error = ERROR_SUCCESS;
        }
        if (error == ERROR_SUCCESS && SetFileAttributes(nameSetAttrs, attrs))
        {
            BOOL isDir = ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0);
            // if we need to set one of the times
            if (timeModified != NULL || timeCreated != NULL || timeAccessed != NULL)
            {
                HANDLE file;
                if (attrs & FILE_ATTRIBUTE_READONLY)
                    SetFileAttributes(nameSetAttrs, attrs & (~FILE_ATTRIBUTE_READONLY));
                file = HANDLES_Q(CreateFile(nameSetAttrs, GENERIC_READ | GENERIC_WRITE,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            NULL, OPEN_EXISTING, isDir ? FILE_FLAG_BACKUP_SEMANTICS : 0, NULL));
                if (file != INVALID_HANDLE_VALUE)
                {
                    FILETIME ftCreated, ftAccessed, ftModified;
                    GetFileTime(file, &ftCreated, &ftAccessed, &ftModified);
                    if (timeCreated != NULL)
                        ftCreated = *timeCreated;
                    if (timeAccessed != NULL)
                        ftAccessed = *timeAccessed;
                    if (timeModified != NULL)
                        ftModified = *timeModified;
                    SetFileTime(file, &ftCreated, &ftAccessed, &ftModified);
                    HANDLES(CloseHandle(file));
                    if (attrs & FILE_ATTRIBUTE_READONLY)
                        SetFileAttributes(nameSetAttrs, attrs);
                }
                else
                {
                    if (attrs & FILE_ATTRIBUTE_READONLY)
                        SetFileAttributes(nameSetAttrs, attrs);
                    goto SHOW_ERROR;
                }
            }
            totalDone += size;
            SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
            return TRUE;
        }
        else
        {
        SHOW_ERROR:

            if (error == ERROR_SUCCESS)
                error = GetLastError();
            if (errTitle == NULL)
                errTitle = LoadStr(IDS_ERRORCHANGINGATTRS);

            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (dlgData.SkipAllChangeAttrs)
                goto SKIP_ATTRS_ERROR;

            int ret;
            ret = IDCANCEL;
            char* data[4];
            data[0] = (char*)&ret;
            data[1] = errTitle;
            data[2] = name;
            data[3] = GetErrorText(error);
            SendMessage(hProgressDlg, WM_USER_DIALOG, 0, (LPARAM)data);
            switch (ret)
            {
            case IDRETRY:
                break;

            case IDB_SKIPALL:
                dlgData.SkipAllChangeAttrs = TRUE;
            case IDB_SKIP:
            {
            SKIP_ATTRS_ERROR:

                totalDone += size;
                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                return TRUE;
            }

            case IDCANCEL:
                return FALSE;
            }
        }
    }
}

unsigned ThreadWorkerBody(void* parameter)
{
    CALL_STACK_MESSAGE1("ThreadWorkerBody()");
    SetThreadNameInVCAndTrace("Worker");
    TRACE_I("Begin");

    CWorkerData* data = (CWorkerData*)parameter;
    //--- create a local copy of the data
    HANDLE wContinue = data->WContinue;
    CProgressDlgData dlgData;
    dlgData.WorkerNotSuspended = data->WorkerNotSuspended;
    dlgData.CancelWorker = data->CancelWorker;
    dlgData.OperationProgress = data->OperationProgress;
    dlgData.SummaryProgress = data->SummaryProgress;
    dlgData.OverwriteAll = dlgData.OverwriteHiddenAll = dlgData.DeleteHiddenAll =
        dlgData.SkipAllFileWrite = dlgData.SkipAllFileRead =
            dlgData.SkipAllOverwrite = dlgData.SkipAllSystemOrHidden =
                dlgData.SkipAllFileOpenIn = dlgData.SkipAllFileOpenOut =
                    dlgData.SkipAllOverwriteErr = dlgData.SkipAllMoveErrors =
                        dlgData.SkipAllDeleteErr = dlgData.SkipAllDirCreate =
                            dlgData.SkipAllDirCreateErr = dlgData.SkipAllChangeAttrs =
                                dlgData.EncryptSystemAll = dlgData.SkipAllEncryptSystem =
                                    dlgData.IgnoreAllADSReadErr = dlgData.SkipAllFileADSOpenIn =
                                        dlgData.SkipAllFileADSOpenOut = dlgData.SkipAllFileADSRead =
                                            dlgData.SkipAllFileADSWrite = dlgData.DirOverwriteAll =
                                                dlgData.SkipAllDirOver = dlgData.IgnoreAllADSOpenOutErr =
                                                    dlgData.IgnoreAllSetAttrsErr = dlgData.IgnoreAllCopyPermErr =
                                                        dlgData.IgnoreAllCopyDirTimeErr = dlgData.SkipAllFileOutLossEncr =
                                                            dlgData.FileOutLossEncrAll = dlgData.SkipAllDirCrLossEncr =
                                                                dlgData.DirCrLossEncrAll = dlgData.IgnoreAllGetFileTimeErr =
                                                                    dlgData.IgnoreAllSetFileTimeErr = dlgData.SkipAllGetFileTime =
                                                                        dlgData.SkipAllSetFileTime = FALSE;
    dlgData.CnfrmFileOver = Configuration.CnfrmFileOver;
    dlgData.CnfrmDirOver = Configuration.CnfrmDirOver;
    dlgData.CnfrmSHFileOver = Configuration.CnfrmSHFileOver;
    dlgData.CnfrmSHFileDel = Configuration.CnfrmSHFileDel;
    dlgData.UseRecycleBin = Configuration.UseRecycleBin;
    dlgData.RecycleMasks.SetMasksString(Configuration.RecycleMasks.GetMasksString(),
                                        Configuration.RecycleMasks.GetExtendedMode());
    int errorPos;
    if (dlgData.UseRecycleBin == 2 &&
        !dlgData.PrepareRecycleMasks(errorPos))
        TRACE_E("Error in recycle-bin group mask.");
    COperations* script = data->Script;
    if (script->TotalSize == CQuadWord(0, 0))
    {
        script->TotalSize = CQuadWord(1, 0); // against division by zero
                                             // TRACE_E("ThreadWorkerBody(): script->TotalSize may not be zero!");  // "synchronization one" is not set during construction, caused trouble in Calculate Occupied Space
    }

    if (script->CopySecurity)
        GainWriteOwnerAccess();

    HWND hProgressDlg = data->HProgressDlg;
    void* buffer = data->Buffer;
    BOOL bufferIsAllocated = data->BufferIsAllocated;
    CChangeAttrsData* attrsData = (CChangeAttrsData*)data->Buffer;
    DWORD clearReadonlyMask = data->ClearReadonlyMask;
    CConvertData convertData;
    if (data->ConvertData != NULL) // Let's make a copy of the data for Convert
    {
        convertData = *data->ConvertData;
    }
    SetEvent(wContinue); // we have the data, let's start either the main thread or the progress-dialog thread
                         //---
    SetProgress(hProgressDlg, 0, 0, dlgData);
    script->InitSpeedMeters(FALSE);

    char lastLantasticCheckRoot[MAX_PATH]; // last root of the path checked on Lantastic ("" = nothing was checked)
    lastLantasticCheckRoot[0] = 0;
    BOOL lastIsLantasticPath = FALSE;                                                                                  // Result of the check on the root lastLantasticCheckRoot
    int mustDeleteFileBeforeOverwrite = 0; /* need test*/                                                             // (zavedeno kvuli SNAP serveru - NSA drive - neslape jim SetEndOfFile - 0/1/2 = need-test/yes/no)
    int allocWholeFileOnStart = 0; /* need test*/                                                                     // Implemented as a backup (e.g. on a SNAP server - NSA drives - probably won't work), we cannot afford to risk a non-functional Copy - 0/1/2 = need-test/yes/no
    int setDirTimeAfterMove = script->PreserveDirTime && script->SourcePathIsNetwork ? 0 /* need test*/ : 2 /* no*/; // e.g. when moving/renaming a directory on Samba, its date and time are changed - 0/1/2 = need-test/yes/no

    BOOL Error = FALSE;
    CQuadWord totalDone;
    totalDone = CQuadWord(0, 0);
    CProgressData pd;
    BOOL novellRenamePatch = FALSE; // TRUE if it is necessary to remove the read-only attribute before calling MoveFile (necessary on Novell)
    char* tgtBuffer = NULL;         // Translation buffer for ocConvert
    CAsyncCopyParams* asyncPar = NULL;
    if (buffer != NULL)
    {
        // Read strings forward to avoid doing it separately for each operation (the LoadStr buffer fills quickly + slows down)
        char opStrCopying[50];
        lstrcpyn(opStrCopying, LoadStr(IDS_COPYING), 50);
        char opStrCopyingPrep[50];
        lstrcpyn(opStrCopyingPrep, LoadStr(IDS_COPYINGPREP), 50);
        char opStrMoving[50];
        lstrcpyn(opStrMoving, LoadStr(IDS_MOVING), 50);
        char opStrMovingPrep[50];
        lstrcpyn(opStrMovingPrep, LoadStr(IDS_MOVINGPREP), 50);
        char opStrCreatingDir[50];
        lstrcpyn(opStrCreatingDir, LoadStr(IDS_CREATINGDIR), 50);
        char opStrDeleting[50];
        lstrcpyn(opStrDeleting, LoadStr(IDS_DELETING), 50);
        char opStrConverting[50];
        lstrcpyn(opStrConverting, LoadStr(IDS_CONVERTING), 50);
        char opChangAttrs[50];
        lstrcpyn(opChangAttrs, LoadStr(IDS_CHANGINGATTRS), 50);

        int i;
        for (i = 0; !*dlgData.CancelWorker && i < script->Count; i++)
        {
            COperation* op = &script->At(i);

            switch (op->Opcode)
            {
            case ocCopyFile:
            {
                pd.Operation = opStrCopying;
                pd.Source = op->SourceName;
                pd.Preposition = opStrCopyingPrep;
                pd.Target = op->TargetName;
                SetProgressDialog(hProgressDlg, &pd, dlgData);

                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);

                BOOL lantasticCheck = IsLantasticDrive(op->TargetName, lastLantasticCheckRoot, lastIsLantasticPath);

                Error = !DoCopyFile(op, hProgressDlg, buffer, script, totalDone,
                                    clearReadonlyMask, NULL, lantasticCheck, mustDeleteFileBeforeOverwrite,
                                    allocWholeFileOnStart, dlgData,
                                    (op->OpFlags & OPFL_COPY_ADS) != 0,
                                    (op->OpFlags & OPFL_AS_ENCRYPTED) != 0,
                                    FALSE, asyncPar);
                break;
            }

            case ocMoveDir:
            case ocMoveFile:
            {
                pd.Operation = opStrMoving;
                pd.Source = op->SourceName;
                pd.Preposition = opStrMovingPrep;
                pd.Target = op->TargetName;
                SetProgressDialog(hProgressDlg, &pd, dlgData);

                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);

                BOOL lantasticCheck = IsLantasticDrive(op->TargetName, lastLantasticCheckRoot, lastIsLantasticPath);
                BOOL ignInvalidName = op->Opcode == ocMoveDir && (op->OpFlags & OPFL_IGNORE_INVALID_NAME) != 0;

                Error = !DoMoveFile(op, hProgressDlg, buffer, script, totalDone,
                                    op->Opcode == ocMoveDir, clearReadonlyMask, &novellRenamePatch,
                                    lantasticCheck, mustDeleteFileBeforeOverwrite,
                                    allocWholeFileOnStart, dlgData,
                                    (op->OpFlags & OPFL_COPY_ADS) != 0,
                                    (op->OpFlags & OPFL_AS_ENCRYPTED) != 0,
                                    &setDirTimeAfterMove, asyncPar, ignInvalidName);
                break;
            }

            case ocCreateDir:
            {
                BOOL copyADS = (op->OpFlags & OPFL_COPY_ADS) != 0;
                BOOL crAsEncrypted = (op->OpFlags & OPFL_AS_ENCRYPTED) != 0;
                BOOL ignInvalidName = (op->OpFlags & OPFL_IGNORE_INVALID_NAME) != 0;
                pd.Operation = opStrCreatingDir;
                pd.Source = op->TargetName;
                pd.Preposition = "";
                pd.Target = "";
                SetProgressDialog(hProgressDlg, &pd, dlgData);

                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);

                BOOL skip, alreadyExisted;
                Error = !DoCreateDir(hProgressDlg, op->TargetName, op->Attr, clearReadonlyMask, dlgData,
                                     totalDone, op->Size, op->SourceName, copyADS, script, buffer, skip,
                                     alreadyExisted, crAsEncrypted, ignInvalidName);
                if (!Error)
                {
                    if (skip) // skip creating directory
                    {
                        // skip all script operations until the closing tag of this directory
                        CQuadWord skipTotal(0, 0);
                        int createDirIndex = i;
                        while (++i < script->Count)
                        {
                            COperation* oper = &script->At(i);
                            if (oper->Opcode == ocLabelForSkipOfCreateDir && (int)oper->Attr == createDirIndex)
                            {
                                script->AddBytesToTFS(CQuadWord((DWORD)(DWORD_PTR)oper->SourceName, (DWORD)(DWORD_PTR)oper->TargetName));
                                break;
                            }
                            skipTotal += oper->Size;
                        }
                        if (i == script->Count)
                        {
                            i = createDirIndex;
                            TRACE_E("ThreadWorkerBody(): unable to find end-label for dir-create operation: opcode=" << op->Opcode << ", index=" << i);
                        }
                        else
                            totalDone += skipTotal;
                    }
                    else
                    {
                        if (alreadyExisted)
                            op->Attr = 0x10000000 /* dir already existed*/;
                        else
                            op->Attr = 0x01000000 /* dir was created*/;
                    }
                    totalDone += op->Size;
                    script->SetProgressSize(totalDone);
                    SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                }
                break;
            }

            case ocCopyDirTime:
            {
                BOOL skipSetDirTime = FALSE;
                // find the skip-label, its index is in the create-dir operation and it stores whether
                // Check if the target directory already existed or if we were creating it (date&time is being copied)
                // only if we created the directory)
                COperation* skipLabel = NULL;
                if (i + 1 < script->Count && script->At(i + 1).Opcode == ocLabelForSkipOfCreateDir)
                    skipLabel = &script->At(i + 1);
                else
                {
                    if (i + 2 < script->Count && script->At(i + 2).Opcode == ocLabelForSkipOfCreateDir)
                        skipLabel = &script->At(i + 2);
                }
                if (skipLabel != NULL)
                {
                    if (skipLabel->Attr < (DWORD)script->Count)
                    {
                        COperation* crDir = &script->At(skipLabel->Attr);
                        if (crDir->Opcode == ocCreateDir && (crDir->OpFlags & OPFL_AS_ENCRYPTED) == 0)
                        {
                            if (crDir->Attr == 0x10000000 /* dir already existed*/)
                                skipSetDirTime = TRUE;
                            else
                            {
                                if (crDir->Attr != 0x01000000 /* dir was created*/)
                                    TRACE_E("ThreadWorkerBody(): unexpected value of Attr in create-dir operation (not 'existed' nor 'created')!");
                            }
                        }
                        else
                            TRACE_E("ThreadWorkerBody(): unexpected opcode or flags of create-dir operation! Opcode=" << crDir->Opcode << ", OpFlags=" << crDir->OpFlags);
                    }
                    else
                        TRACE_E("ThreadWorkerBody(): unexpected index of create-dir operation! index=" << skipLabel->Attr);
                }
                else
                    TRACE_E("ThreadWorkerBody(): unable to find end-label for dir-create operation (not in first following item nor in second following item)!");

                if (!skipSetDirTime)
                {
                    pd.Operation = opChangAttrs;
                    pd.Source = op->TargetName;
                    pd.Preposition = "";
                    pd.Target = "";
                    SetProgressDialog(hProgressDlg, &pd, dlgData);

                    SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);

                    FILETIME modified;
                    modified.dwLowDateTime = (DWORD)(DWORD_PTR)op->SourceName;
                    modified.dwHighDateTime = op->Attr;
                    Error = !DoCopyDirTime(hProgressDlg, op->TargetName, &modified, dlgData, FALSE);
                }
                if (!Error)
                {
                    script->AddBytesToSpeedMetersAndTFSandPS((DWORD)op->Size.Value, TRUE, 0, NULL, MAX_OP_FILESIZE);

                    totalDone += op->Size;
                    SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                }
                break;
            }

            case ocDeleteFile:
            case ocDeleteDir:
            case ocDeleteDirLink:
            {
                pd.Operation = opStrDeleting;
                pd.Source = op->SourceName;
                pd.Preposition = "";
                pd.Target = "";
                SetProgressDialog(hProgressDlg, &pd, dlgData);

                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);

                if (op->Opcode == ocDeleteFile)
                {
                    Error = !DoDeleteFile(hProgressDlg, op->SourceName, op->Size,
                                          script, totalDone, op->Attr, dlgData);
                }
                else
                {
                    if (op->Opcode == ocDeleteDir)
                    {
                        Error = !DoDeleteDir(hProgressDlg, op->SourceName, op->Size,
                                             script, totalDone, op->Attr, (DWORD)(DWORD_PTR)op->TargetName != -1,
                                             dlgData);
                    }
                    else
                    {
                        Error = !DoDeleteDirLink(hProgressDlg, op->SourceName, op->Size,
                                                 script, totalDone, dlgData);
                    }
                }
                break;
            }

            case ocConvert:
            {
                // output buffer - I will perform translation into it (for the worst case,
                // when the input file contains only CR or LF and we translate them to CRLF, this
                // buffer twice the size of sourceBuffer) and then I will be writing to it
                // to a temporary file
                if (tgtBuffer == NULL) // first pass?
                {
                    tgtBuffer = (char*)malloc(OPERATION_BUFFER * 2);
                    if (tgtBuffer == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        Error = TRUE;
                        break; // error ...
                    }
                }
                pd.Operation = opStrConverting;
                pd.Source = op->SourceName;
                pd.Preposition = "";
                pd.Target = "";
                SetProgressDialog(hProgressDlg, &pd, dlgData);

                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);

                Error = !DoConvert(hProgressDlg, op->SourceName, (char*)buffer, tgtBuffer, op->Size, script,
                                   totalDone, convertData, dlgData);
                break;
            }

            case ocChangeAttrs:
            {
                pd.Operation = opChangAttrs;
                pd.Source = op->SourceName;
                pd.Preposition = "";
                pd.Target = "";
                SetProgressDialog(hProgressDlg, &pd, dlgData);

                SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);

                Error = !DoChangeAttrs(hProgressDlg, op->SourceName, op->Size, (DWORD)(DWORD_PTR)op->TargetName,
                                       script, totalDone,
                                       attrsData->ChangeTimeModified ? &attrsData->TimeModified : NULL,
                                       attrsData->ChangeTimeCreated ? &attrsData->TimeCreated : NULL,
                                       attrsData->ChangeTimeAccessed ? &attrsData->TimeAccessed : NULL,
                                       attrsData->ChangeCompression, attrsData->ChangeEncryption,
                                       op->Attr, dlgData);
                break;
            }

            case ocLabelForSkipOfCreateDir:
                break; // no activity
            }
            if (Error)
                break;
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // if we are to be in suspend mode, we wait ...
        }
        if (!Error && !*dlgData.CancelWorker && i == script->Count && totalDone != script->TotalSize &&
            (totalDone != CQuadWord(0, 0) || script->TotalSize != CQuadWord(1, 0))) // Intentional change of script->TotalSize to one (precaution against division by zero)
        {
            TRACE_E("ThreadWorkerBody(): operation done: totalDone != script->TotalSize (" << totalDone.Value << " != " << script->TotalSize.Value << ")");
        }
        CQuadWord transferredFileSize, progressSize;
        if (!Error && !*dlgData.CancelWorker && i == script->Count &&
            script->GetTFSandProgressSize(&transferredFileSize, &progressSize) &&
            (transferredFileSize != script->TotalFileSize ||
             progressSize != script->TotalSize &&
                 (progressSize != CQuadWord(0, 0) || script->TotalSize != CQuadWord(1, 0)))) // Intentional change of script->TotalSize to one (precaution against division by zero)
        {
            if (transferredFileSize != script->TotalFileSize)
            {
                TRACE_E("ThreadWorkerBody(): operation done: transferredFileSize != script->TotalFileSize (" << transferredFileSize.Value << " != " << script->TotalFileSize.Value << ")");
            }
            if (progressSize != script->TotalSize &&
                (progressSize != CQuadWord(0, 0) || script->TotalSize != CQuadWord(1, 0)))
            {
                TRACE_E("ThreadWorkerBody(): operation done: progressSize != script->TotalSize (" << progressSize.Value << " != " << script->TotalSize.Value << ")");
            }
        }
    }
    if (asyncPar != NULL)
        delete asyncPar;
    if (tgtBuffer != NULL)
        free(tgtBuffer);
    if (bufferIsAllocated)
        free(buffer);
    *dlgData.CancelWorker = Error;                  // when it comes to Cancel, we make it clear...
    SendMessage(hProgressDlg, WM_COMMAND, IDOK, 0); // we are finishing ...
    WaitForSingleObject(wContinue, INFINITE);       // we need to stop the hl.thread

    FreeScript(script); // calls delete, main thread cannot run without it

    TRACE_I("End");
    return 0;
}

unsigned ThreadWorkerEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ThreadWorkerBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread Worker: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ThreadWorker(void* param)
{
    CCallStack stack;
    return ThreadWorkerEH(param);
}

HANDLE StartWorker(COperations* script, HWND hDlg, CChangeAttrsData* attrsData,
                   CConvertData* convertData, HANDLE wContinue, HANDLE workerNotSuspended,
                   BOOL* cancelWorker, int* operationProgress, int* summaryProgress)
{
    CWorkerData data;
    data.WorkerNotSuspended = workerNotSuspended;
    data.CancelWorker = cancelWorker;
    data.OperationProgress = operationProgress;
    data.SummaryProgress = summaryProgress;
    data.WContinue = wContinue;
    data.ConvertData = convertData;
    data.Script = script;
    data.HProgressDlg = hDlg;
    data.ClearReadonlyMask = script->ClearReadonlyMask;
    if (attrsData != NULL)
    {
        data.Buffer = attrsData;
        data.BufferIsAllocated = FALSE;
    }
    else
    {
        data.BufferIsAllocated = TRUE;
        data.Buffer = malloc(max(REMOVABLE_DISK_COPY_BUFFER, OPERATION_BUFFER));
        if (data.Buffer == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return NULL;
        }
    }
    DWORD threadID;
    ResetEvent(wContinue);
    *cancelWorker = FALSE;

    // if (Worker != NULL) HANDLES(CloseHandle(Worker));  // probably unnecessary
    HANDLE worker = HANDLES(CreateThread(NULL, 0, ThreadWorker, &data, 0, &threadID));
    if (worker == NULL)
    {
        if (data.BufferIsAllocated)
            free(data.Buffer);
        TRACE_E("Unable to start Worker thread.");
        return NULL;
    }
    //  SetThreadPriority(Worker, THREAD_PRIORITY_HIGHEST);
    WaitForSingleObject(wContinue, INFINITE); // Wait until the data is copied (it's on the stack)
    return worker;
}

void FreeScript(COperations* script)
{
    if (script == NULL)
        return;
    int i;
    for (i = 0; i < script->Count; i++)
    {
        COperation* op = &script->At(i);
        if (op->SourceName != NULL && op->Opcode != ocCopyDirTime && op->Opcode != ocLabelForSkipOfCreateDir)
            free(op->SourceName);
        if (op->TargetName != NULL && op->Opcode != ocChangeAttrs && op->Opcode != ocLabelForSkipOfCreateDir)
            free(op->TargetName);
    }
    if (script->WaitInQueueSubject != NULL)
        free(script->WaitInQueueSubject);
    if (script->WaitInQueueFrom != NULL)
        free(script->WaitInQueueFrom);
    if (script->WaitInQueueTo != NULL)
        free(script->WaitInQueueTo);
    delete script;
}

BOOL COperationsQueue::AddOperation(HWND dlg, BOOL startOnIdle, BOOL* startPaused)
{
    CALL_STACK_MESSAGE1("COperationsQueue::AddOperation()");

    HANDLES(EnterCriticalSection(&QueueCritSect));

    int i;
    for (i = 0; i < OperDlgs.Count; i++) // ensure uniqueness (operation can only be added once)
        if (OperDlgs[i] == dlg)
            break;

    BOOL ret = FALSE;
    if (i == OperDlgs.Count) // operation can be added
    {
        OperDlgs.Add(dlg);
        if (OperDlgs.IsGood())
        {
            if (startOnIdle)
            {
                int j;
                for (j = 0; j < OperPaused.Count && OperPaused[j] == 1 /* auto-paused*/; j++)
                    ; // if an operation is already running or has been manually paused, we start this operation as "auto-paused"
                *startPaused = j < OperPaused.Count;
            }
            else
                *startPaused = FALSE;
            OperPaused.Add(*startPaused ? 1 /* auto-paused*/ : 0 /* running*/);
            if (!OperPaused.IsGood())
            {
                OperPaused.ResetState();
                OperDlgs.Delete(OperDlgs.Count - 1);
                if (!OperDlgs.IsGood())
                    OperDlgs.ResetState();
            }
            else
                ret = TRUE;
        }
        else
            OperDlgs.ResetState();
    }
    else
        TRACE_E("COperationsQueue::AddOperation(): this operation has already been added!");

    HANDLES(LeaveCriticalSection(&QueueCritSect));

    return ret;
}

void COperationsQueue::OperationEnded(HWND dlg, BOOL doNotResume, HWND* foregroundWnd)
{
    CALL_STACK_MESSAGE1("COperationsQueue::OperationEnded()");

    HANDLES(EnterCriticalSection(&QueueCritSect));

    BOOL found = FALSE;
    int i;
    for (i = 0; i < OperDlgs.Count; i++)
    {
        if (OperDlgs[i] == dlg)
        {
            found = TRUE;
            OperDlgs.Delete(i);
            if (!OperDlgs.IsGood())
                OperDlgs.ResetState();
            OperPaused.Delete(i);
            if (!OperPaused.IsGood())
                OperPaused.ResetState();
            break;
        }
    }
    if (!found)
        TRACE_E("COperationsQueue::OperationEnded(): unexpected situation: operation was not found!");
    else
    {
        if (!doNotResume)
        {
            int j;
            for (j = 0; j < OperPaused.Count && OperPaused[j] == 1 /* auto-paused*/; j++)
                ; // if no operation is running and no operation has been manually paused, we will resume the first operation in the queue
            if (j == OperPaused.Count && OperDlgs.Count > 0)
            {
                PostMessage(OperDlgs[0], WM_COMMAND, CM_RESUMEOPER, 0);
                if (foregroundWnd != NULL && GetForegroundWindow() == dlg)
                    *foregroundWnd = OperDlgs[0];
            }
        }
    }

    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

void COperationsQueue::SetPaused(HWND dlg, int paused)
{
    CALL_STACK_MESSAGE1("COperationsQueue::SetPaused()");

    HANDLES(EnterCriticalSection(&QueueCritSect));

    int i;
    for (i = 0; i < OperDlgs.Count; i++)
    {
        if (OperDlgs[i] == dlg)
        {
            OperPaused[i] = paused;
            break;
        }
    }
    if (i == OperDlgs.Count)
        TRACE_E("COperationsQueue::SetPaused(): operation was not found!");

    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

BOOL COperationsQueue::IsEmpty()
{
    CALL_STACK_MESSAGE1("COperationsQueue::IsEmpty()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    BOOL ret = OperDlgs.Count == 0;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return ret;
}

void COperationsQueue::AutoPauseOperation(HWND dlg, HWND* foregroundWnd)
{
    CALL_STACK_MESSAGE1("COperationsQueue::AutoPauseOperation()");

    HANDLES(EnterCriticalSection(&QueueCritSect));

    int i;
    for (i = 0; i < OperDlgs.Count; i++)
    {
        if (OperDlgs[i] == dlg)
        {
            int j;
            for (j = i; j + 1 < OperDlgs.Count; j++)
                OperDlgs[j] = OperDlgs[j + 1];
            for (j = i; j + 1 < OperPaused.Count; j++)
                OperPaused[j] = OperPaused[j + 1];
            OperDlgs[j] = dlg;
            OperPaused[j] = 1 /* auto-paused*/;
            break;
        }
    }
    if (i == OperDlgs.Count)
        TRACE_E("COperationsQueue::AutoPauseOperation(): operation was not found!");

    // if no operation is running and no operation has been manually paused, we will resume the first operation in the queue
    int j;
    for (j = 0; j < OperPaused.Count && OperPaused[j] == 1 /* auto-paused*/; j++)
        ;
    if (j == OperPaused.Count && OperDlgs.Count > 0)
    {
        PostMessage(OperDlgs[0], WM_COMMAND, CM_RESUMEOPER, 0);
        if (foregroundWnd != NULL && GetForegroundWindow() == dlg)
            *foregroundWnd = OperDlgs[0];
    }

    HANDLES(LeaveCriticalSection(&QueueCritSect));
}

int COperationsQueue::GetNumOfOperations()
{
    CALL_STACK_MESSAGE1("COperationsQueue::GetNumOfOperations()");

    HANDLES(EnterCriticalSection(&QueueCritSect));
    int c = OperDlgs.Count;
    HANDLES(LeaveCriticalSection(&QueueCritSect));
    return c;
}
