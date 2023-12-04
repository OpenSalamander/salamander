// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "worker.h"

#include <Aclapi.h>
#include <Ntsecapi.h>

// tyto funkce nemaji header, musime loadit dynamicky
NTQUERYINFORMATIONFILE DynNtQueryInformationFile = NULL;
NTFSCONTROLFILE DynNtFsControlFile = NULL;

COperationsQueue OperationsQueue; // fronta diskovych operaci

// je-li definovane, pisou se do TRACE ruzne debug hlasky
//#define WORKER_COPY_DEBUG_MSG

// zakomentovat az nebudu chtit sledovat vsechny ty hlasky z prubehu algoritmu asynchronniho kopirovani
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
    { // otestujeme, jestli nejde o nizkou rychlost (vypocet z LastPacketsSize a LastPacketsTime)
        int firstPacket = ((TRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - CountOfLastPackets) % (TRSPMETER_NUMOFSTOREDPACKETS + 1);
        int lastPacket = ((TRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - 1) % (TRSPMETER_NUMOFSTOREDPACKETS + 1);
        DWORD lastPacketTime = LastPacketsTime[lastPacket];
        DWORD totalTime = lastPacketTime - LastPacketsTime[firstPacket]; // cas mezi prijmem prvniho a posledniho paketu
        if (totalTime >= ((DWORD)(CountOfLastPackets - 1) * TRSPMETER_STPCKTSMININTERVAL) / TRSPMETER_NUMOFSTOREDPACKETS)
        {                                     // jde o nizkou rychlost (do TRSPMETER_NUMOFSTOREDPACKETS paketu za TRSPMETER_STPCKTSMININTERVAL ms)
            if (time - lastPacketTime > 2000) // dvouvterinova "ochrana" doba pro posledni napocitanou pomalou rychlost
            {                                 // zkusime, jestli nejde o vice nez dvojnasobne zpomaleni proti rychlosti posledniho paketu, pokud ano, zobrazime
                                              // nulovou rychlost (abysme pri zastaveni pomaleho prenosu neukazovali porad posledni ziskany rychlostni udaj)
                int preLastPacket = ((TRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - 2) % (TRSPMETER_NUMOFSTOREDPACKETS + 1);
                if ((UINT64)2 * MaxPacketSize * (lastPacketTime - LastPacketsTime[preLastPacket]) < (UINT64)LastPacketsSize[lastPacket] * (time - lastPacketTime))
                {
                    speed->SetUI64(0);
                    ResetSpeed = TRUE;
                    return; // rychlost klesla aspon dvakrat, radsi ukazeme nulu
                }
            }
            if (totalTime > TRSPMETER_ACTSPEEDSTEP * TRSPMETER_ACTSPEEDNUMOFSTEPS)
            { // rychlost budeme pocitat jen z dat co nejblizsi dobe TRSPMETER_ACTSPEEDSTEP * TRSPMETER_ACTSPEEDNUMOFSTEPS
                // (pokud budou pakety chodit pomalu, muzou byt ve fronte treba pakety za poslednich pet minut - my tu ovsem
                // pocitame "okamzitou" rychlost, ne prumer za poslednich pet minut)
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
            UINT64 totalSize = 0; // soucet vsech velikosti paketu, krome prvniho, ten vynechavame (z nej se bere jen cas)
            do
            {
                if (++firstPacket >= TRSPMETER_NUMOFSTOREDPACKETS + 1)
                    firstPacket = 0;
                totalSize += LastPacketsSize[firstPacket];
            } while (firstPacket != lastPacket);
            speed->SetUI64((1000 * totalSize) / totalTime);
            return; // mame spocitanou nizkou rychlost, koncime
        }
        else // jde o vysokou rychlost (vic nez TRSPMETER_NUMOFSTOREDPACKETS paketu za TRSPMETER_STPCKTSMININTERVAL ms),
        {    // provedeme test na nahly pokles rychlosti (hlavne kdyz se zacnou kopirovat nulove soubory nebo vytvaret prazdne adresare)
            if (time - lastPacketTime > 800)
            { // pokud uz 800ms neprisel zadny paket, budeme hlasit nulovou rychlost
                speed->SetUI64(0);
                ResetSpeed = TRUE;
                return;
            }
        }
    }
    else // neni z ceho pocitat, zatim hlasime "0 B/s"
    {
        speed->SetUI64(0);
        return;
    }
    // vysoka rychlost (vic nez TRSPMETER_NUMOFSTOREDPACKETS paketu za TRSPMETER_STPCKTSMININTERVAL ms)
    if (CountOfTrBytesItems > 0) // po navazani spojeni je "always true"
    {
        int actIndexAdded = 0;                           // 0 = nebyl zapocitan akt. index, 1 = byl zapocitan akt. index
        int emptyTrBytes = 0;                            // pocet zapocitanych prazdnych kroku
        UINT64 total = 0;                                // celkovy pocet bytu za poslednich max. TRSPMETER_ACTSPEEDNUMOFSTEPS kroku
        int addFromTrBytes = CountOfTrBytesItems - 1;    // pocet uzavrenych kroku k pridani z fronty
        DWORD restTime = 0;                              // cas od posledniho zapocitaneho kroku do tohoto okamziku
        if ((int)(time - ActIndexInTrBytesTimeLim) >= 0) // akt. index uz je uzavreny + mozna budou potreba i prazdne kroky
        {
            emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / TRSPMETER_ACTSPEEDSTEP;
            restTime = (time - ActIndexInTrBytesTimeLim) % TRSPMETER_ACTSPEEDSTEP;
            emptyTrBytes = min(emptyTrBytes, TRSPMETER_ACTSPEEDNUMOFSTEPS);
            if (emptyTrBytes < TRSPMETER_ACTSPEEDNUMOFSTEPS) // nestaci jen prazdne kroky, zapocteme i akt. index
            {
                total = TransferedBytes[ActIndexInTrBytes];
                actIndexAdded = 1;
            }
            addFromTrBytes = TRSPMETER_ACTSPEEDNUMOFSTEPS - actIndexAdded - emptyTrBytes;
            addFromTrBytes = min(addFromTrBytes, CountOfTrBytesItems - 1); // kolik uzavrenych kroku z fronty jeste zapocist
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
                actIndex = TRSPMETER_ACTSPEEDNUMOFSTEPS; // pohyb po kruh. fronte
            total += TransferedBytes[actIndex];
        }
        DWORD t = (addFromTrBytes + actIndexAdded + emptyTrBytes) * TRSPMETER_ACTSPEEDSTEP + restTime;
        if (t > 0)
            speed->SetUI64((total * 1000) / t);
        else
            speed->SetUI64(0); // neni z ceho pocitat, zatim hlasime "0 B/s"
    }
    else
        speed->SetUI64(0); // neni z ceho pocitat, zatim hlasime "0 B/s"
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
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CTransferSpeedMeter::BytesReceived(, ,)"); // parametry ignorujeme z rychlostnich duvodu (call-stack uz tak brzdi)

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
    if ((int)(time - ActIndexInTrBytesTimeLim) < 0) // je v akt. casovem intervalu, jen pridame pocet bytu do intervalu
    {
        TransferedBytes[ActIndexInTrBytes] += count;
    }
    else // neni v akt. casovem intervalu, musime zalozit novy interval
    {
        int emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / TRSPMETER_ACTSPEEDSTEP;
        int i = min(emptyTrBytes, TRSPMETER_ACTSPEEDNUMOFSTEPS); // vic uz nema vliv (nuluje se cela fronta)
        if (i > 0 && CountOfTrBytesItems <= TRSPMETER_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems = min(TRSPMETER_ACTSPEEDNUMOFSTEPS + 1, CountOfTrBytesItems + i);
        while (i--)
        {
            if (++ActIndexInTrBytes > TRSPMETER_ACTSPEEDNUMOFSTEPS)
                ActIndexInTrBytes = 0; // pohyb po kruh. fronte
            TransferedBytes[ActIndexInTrBytes] = 0;
        }
        ActIndexInTrBytesTimeLim += (emptyTrBytes + 1) * TRSPMETER_ACTSPEEDSTEP;
        if (++ActIndexInTrBytes > TRSPMETER_ACTSPEEDNUMOFSTEPS)
            ActIndexInTrBytes = 0; // pohyb po kruh. fronte
        if (CountOfTrBytesItems <= TRSPMETER_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems++;
        TransferedBytes[ActIndexInTrBytes] = count;
    }
}

void CTransferSpeedMeter::AdjustProgressBufferLimit(DWORD* progressBufferLimit, DWORD lastFileBlockCount,
                                                    DWORD lastFileStartTime)
{
    if (CountOfLastPackets > 1 && lastFileBlockCount > 0) // "always true": CountOfLastPackets je na zacatku souboru 1 (2 = mame jeden paket)
    {
        unsigned __int64 size = 0; // celkova velikost ulozenych paketu posledniho souboru
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
                break; // bereme max. 2 vteriny stare pakety (snazime se spocitat "aktualni" rychlost)
            }
        }
        DWORD totalTime = min(ti - LastPacketsTime[i], ti - lastFileStartTime); // LastPacketsTime[i] muze byt starsi nez lastFileStartTime (je to posledni paket minuleho souboru), nas zajima jen cas straveny v tomto souboru
        if (totalTime == 0)
            totalTime = 10; // 0ms bereme jako 10ms (cca krok GetTickCount())
        unsigned __int64 speed = (size * 1000) / totalTime;
        DWORD bufLimit = ASYNC_SLOW_COPY_BUF_SIZE;
        while (bufLimit < ASYNC_COPY_BUF_SIZE)
        {
            // experimentalne zjisteno, ze Windows 7 milujou velikost bufferu 32KB, krivka vyuziti
            // sitove linky je s ni vetsinou krasne hladka, kdezto pri 64KB to skace jak mrcha
            // a celkova dosazena rychlost je tak o 5% mensi ... tedy dirty bloody hack: budeme take
            // preferovat 32KB ... az do 8 * 128 (1024KB/s) ... tim preskocime 64KB a 128KB, dalsi
            // buffer limit je az 256KB
            // +
            // provedeme opatreni proti oscilaci mezi dvema velikostmi buffer limitu u rychlosti na hranici
            // mezi dvema velikostmi buffer limitu, zvyseni o jednu uroven bude tezsi (na vyber stejneho bufferu
            // muze byt rychlost az 9 * bufLimit misto standardnich 8 * bufLimit)
            if (bufLimit == 32 * 1024) // pro 32KB buffer-limit pouzijeme hodnoty pro 128KB buffer-limit (misto 64KB a 128KB vybereme 32KB)
            {
                if (speed <= (bufLimit == *progressBufferLimit ? 9 * 128 * 1024 : 8 * 128 * 1024))
                    break;
                bufLimit = 256 * 1024; // nevyslo 32KB, zkusime az 256KB (64KB a 128KB nehrozi, to by se vybralo 32KB)
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
    { // otestujeme, jestli nejde o nizkou rychlost (vypocet z LastPacketsSize a LastPacketsTime)
        int firstPacket = ((PRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - CountOfLastPackets) % (PRSPMETER_NUMOFSTOREDPACKETS + 1);
        int lastPacket = ((PRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - 1) % (PRSPMETER_NUMOFSTOREDPACKETS + 1);
        DWORD lastPacketTime = LastPacketsTime[lastPacket];
        DWORD totalTime = lastPacketTime - LastPacketsTime[firstPacket]; // cas mezi prijmem prvniho a posledniho paketu
        if (totalTime >= ((DWORD)(CountOfLastPackets - 1) * PRSPMETER_STPCKTSMININTERVAL) / PRSPMETER_NUMOFSTOREDPACKETS)
        {                                     // jde o nizkou rychlost (do PRSPMETER_NUMOFSTOREDPACKETS paketu za PRSPMETER_STPCKTSMININTERVAL ms)
            if (time - lastPacketTime > 5000) // petivterinova "ochrana" doba pro posledni napocitanou pomalou rychlost
            {                                 // zkusime jestli nejde o vice nez ctyrnasobne zpomaleni proti rychlosti posledniho paketu, pokud ano, zobrazime
                                              // nulovou rychlost (abysme pri zastaveni pomaleho prenosu neukazovali porad posledni ziskany time-left udaj)
                int preLastPacket = ((PRSPMETER_NUMOFSTOREDPACKETS + 1) + ActIndexInLastPackets - 2) % (PRSPMETER_NUMOFSTOREDPACKETS + 1);
                if ((UINT64)4 * MaxPacketSize * (lastPacketTime - LastPacketsTime[preLastPacket]) < (UINT64)LastPacketsSize[lastPacket] * (time - lastPacketTime))
                {
                    speed->SetUI64(0);
                    return; // rychlost klesla aspon dvakrat, radsi ukazeme nulu
                }
            }
            if (totalTime > PRSPMETER_ACTSPEEDSTEP * PRSPMETER_ACTSPEEDNUMOFSTEPS)
            { // rychlost budeme pocitat jen z dat co nejblizsi dobe PRSPMETER_ACTSPEEDSTEP * PRSPMETER_ACTSPEEDNUMOFSTEPS
                // (pokud budou pakety chodit pomalu, muzou byt ve fronte treba pakety za poslednich pet minut - my tu ovsem
                // pocitame rychlost za poslednich X vterin, ne prumer za poslednich pet minut)
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
            UINT64 totalSize = 0; // soucet vsech velikosti paketu, krome prvniho, ten vynechavame (z nej se bere jen cas)
            do
            {
                if (++firstPacket >= PRSPMETER_NUMOFSTOREDPACKETS + 1)
                    firstPacket = 0;
                totalSize += LastPacketsSize[firstPacket];
            } while (firstPacket != lastPacket);
            speed->SetUI64((1000 * totalSize) / totalTime);
            return; // mame spocitanou nizkou rychlost, koncime
        }
        else // jde o vysokou rychlost (vic nez PRSPMETER_NUMOFSTOREDPACKETS paketu za PRSPMETER_STPCKTSMININTERVAL ms),
        {    // provedeme test na nahly pokles rychlosti (hlavne kdyz se zacnou kopirovat nulove soubory nebo vytvaret prazdne adresare)
            if (time - lastPacketTime > 5000)
            { // pokud uz 5000ms neprisel zadny paket, budeme hlasit nulovou rychlost
                speed->SetUI64(0);
                return;
            }
        }
    }
    else // neni z ceho pocitat, zatim hlasime "0 B/s"
    {
        speed->SetUI64(0);
        return;
    }
    // vysoka rychlost (vic nez PRSPMETER_NUMOFSTOREDPACKETS paketu za PRSPMETER_STPCKTSMININTERVAL ms)
    if (CountOfTrBytesItems > 0) // po navazani spojeni je "always true"
    {
        int actIndexAdded = 0;                           // 0 = nebyl zapocitan akt. index, 1 = byl zapocitan akt. index
        int emptyTrBytes = 0;                            // pocet zapocitanych prazdnych kroku
        UINT64 total = 0;                                // celkovy pocet bytu za poslednich max. PRSPMETER_ACTSPEEDNUMOFSTEPS kroku
        int addFromTrBytes = CountOfTrBytesItems - 1;    // pocet uzavrenych kroku k pridani z fronty
        DWORD restTime = 0;                              // cas od posledniho zapocitaneho kroku do tohoto okamziku
        if ((int)(time - ActIndexInTrBytesTimeLim) >= 0) // akt. index uz je uzavreny + mozna budou potreba i prazdne kroky
        {
            emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / PRSPMETER_ACTSPEEDSTEP;
            restTime = (time - ActIndexInTrBytesTimeLim) % PRSPMETER_ACTSPEEDSTEP;
            emptyTrBytes = min(emptyTrBytes, PRSPMETER_ACTSPEEDNUMOFSTEPS);
            if (emptyTrBytes < PRSPMETER_ACTSPEEDNUMOFSTEPS) // nestaci jen prazdne kroky, zapocteme i akt. index
            {
                total = TransferedBytes[ActIndexInTrBytes];
                actIndexAdded = 1;
            }
            addFromTrBytes = PRSPMETER_ACTSPEEDNUMOFSTEPS - actIndexAdded - emptyTrBytes;
            addFromTrBytes = min(addFromTrBytes, CountOfTrBytesItems - 1); // kolik uzavrenych kroku z fronty jeste zapocist
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
                actIndex = PRSPMETER_ACTSPEEDNUMOFSTEPS; // pohyb po kruh. fronte
            total += TransferedBytes[actIndex];
        }
        DWORD t = (addFromTrBytes + actIndexAdded + emptyTrBytes) * PRSPMETER_ACTSPEEDSTEP + restTime;
        if (t > 0)
            speed->SetUI64((total * 1000) / t);
        else
            speed->SetUI64(0); // neni z ceho pocitat, zatim hlasime "0 B/s"
    }
    else
        speed->SetUI64(0); // neni z ceho pocitat, zatim hlasime "0 B/s"
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
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CProgressSpeedMeter::BytesReceived(, ,)"); // parametry ignorujeme z rychlostnich duvodu (call-stack uz tak brzdi)

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
    if ((int)(time - ActIndexInTrBytesTimeLim) < 0) // je v akt. casovem intervalu, jen pridame pocet bytu do intervalu
    {
        TransferedBytes[ActIndexInTrBytes] += count;
    }
    else // neni v akt. casovem intervalu, musime zalozit novy interval
    {
        int emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / PRSPMETER_ACTSPEEDSTEP;
        int i = min(emptyTrBytes, PRSPMETER_ACTSPEEDNUMOFSTEPS); // vic uz nema vliv (nuluje se cela fronta)
        if (i > 0 && CountOfTrBytesItems <= PRSPMETER_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems = min(PRSPMETER_ACTSPEEDNUMOFSTEPS + 1, CountOfTrBytesItems + i);
        while (i--)
        {
            if (++ActIndexInTrBytes > PRSPMETER_ACTSPEEDNUMOFSTEPS)
                ActIndexInTrBytes = 0; // pohyb po kruh. fronte
            TransferedBytes[ActIndexInTrBytes] = 0;
        }
        ActIndexInTrBytesTimeLim += (emptyTrBytes + 1) * PRSPMETER_ACTSPEEDSTEP;
        if (++ActIndexInTrBytes > PRSPMETER_ACTSPEEDNUMOFSTEPS)
            ActIndexInTrBytes = 0; // pohyb po kruh. fronte
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
    WaitInQueueSubject = waitInQueueSubject; // uvolnuje se ve FreeScript()
    WaitInQueueFrom = waitInQueueFrom;       // uvolnuje se ve FreeScript()
    WaitInQueueTo = waitInQueueTo;           // uvolnuje se ve FreeScript()
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
                    if (SleepAfterWrite == -1) // tohle je prvni paket, nastavime parametry speed-limitu
                    {
                        CalcLimitBufferSize(limitBufferSize, bufferSize);
                        LastBufferLimit = *limitBufferSize;
                        if (SpeedLimit >= HIGH_SPEED_LIMIT)
                        { // tady nehrozi, ze by prislo vic dat nez povoluje speed-limit (HIGH_SPEED_LIMIT musi byt >= nejvetsimu bufferu)
                            SleepAfterWrite = 0;
                            BytesTrFromLastSetup.SetUI64(bytesCount);
                        }
                        else
                        {
                            SleepAfterWrite = (1000 * *limitBufferSize) / SpeedLimit; // na prvni vterinu prenosu predpokladame, ze samotny prenos je "nekonecne rychly" (urcit realnou rychlost z prvniho paketu je nerealne)
                            if (bytesCount > SpeedLimit)                              // doslo ke zpomaleni behem operace (napr. probehl read&write 32KB bufferu a speed-limit je 1 B/s, tedy teoreticky bysme meli ted 32768 sekund cekat, coz je prirozene nerealne)
                                sleepNow = 1000;                                      // pockame vterinu + do meraku rychlosti "prizname" jen byty do speed-limitu (napr. jen 1 B)
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
                        { // je cas prepocitat parametry speed-limitu + pripadne "dobrzdit"
                            __int64 sleepFromLastSetup64 = (SleepAfterWrite * BytesTrFromLastSetup.Value) / LastBufferLimit;
                            DWORD sleepFromLastSetup = sleepFromLastSetup64 < 1000 ? (DWORD)sleepFromLastSetup64 : 1000;
                            BytesTrFromLastSetup += CQuadWord(bytesCount, 0);
                            __int64 idealTotalTime64 = (1000 * BytesTrFromLastSetup.Value + SpeedLimit - 1) / SpeedLimit; // "+ SpeedLimit - 1" je kvuli zaokrouhlovani
                            int idealTotalTime = idealTotalTime64 < 10000 ? (int)idealTotalTime64 : 10000;
                            if (idealTotalTime > (int)(ti - LastSetupTime))
                            {
                                sleepNow = idealTotalTime - (ti - LastSetupTime); // potrebujeme dobrzdit (jsme rychlejsi nebo jen o malo pomalejsi nez speed-limit)
                                if (sleepNow > 1000)                              // cekat dele nez vterinu nema smysl (do meraku "prizname" max. *limitBufferSize)
                                    sleepNow = 1000;
                            }
                            // else sleepNow = 0;  // jsme pomalejsi nez speed-limit (pri idealni rychlosti bysme cekali pomernou cast ze SleepAfterWrite)

                            CalcLimitBufferSize(limitBufferSize, bufferSize);
                            LastBufferLimit = *limitBufferSize;

                            if (SpeedLimit >= HIGH_SPEED_LIMIT)
                                SleepAfterWrite = 0;
                            else
                            {
                                int idealTotalSleep = (int)(sleepFromLastSetup + (idealTotalTime - (ti - LastSetupTime)));
                                if (idealTotalSleep > 0) // speed-limit je nizsi nez rychlost kopirovani, budeme brzdit za kazdym paketem
                                    SleepAfterWrite = (DWORD)(((unsigned __int64)idealTotalSleep * LastBufferLimit) / BytesTrFromLastSetup.Value);
                                else
                                    SleepAfterWrite = 0; // speed-limit je vyssi nez rychlost kopirovani (neni proc brzdit)
                            }
                            LastSetupTime = ti + sleepNow;
                            BytesTrFromLastSetup.SetUI64(0);
                        }
                        else // pro mezilehle pakety pouzijeme predpocitane parametry
                        {
                            BytesTrFromLastSetup += CQuadWord(bytesCount, 0);
                            *limitBufferSize = LastBufferLimit < bufferSize ? LastBufferLimit : bufferSize;
                            if (SleepAfterWrite > 0)
                            {
                                sleepNow = (SleepAfterWrite * bytesCount) / LastBufferLimit;
                                if (sleepNow > 1000) // cekat dele nez vterinu nema smysl (do meraku "prizname" max. speed-limit)
                                    sleepNow = 1000;
                            }
                        }
                    }
                    if (bytesCount > SpeedLimit)               // doslo ke zpomaleni behem operace (napr. probehl read&write 32KB bufferu a speed-limit je 1 B/s, tedy teoreticky bysme meli ted 32768 sekund cekat, coz je prirozene nerealne)
                        bytesCountForSpeedMeters = SpeedLimit; // do meraku rychlosti "prizname" jen byty do speed-limitu (napr. jen 1 B)
                    if (sleepNow > 0)                          // brzdime kvuli speed-limitu
                    {
                        HANDLES(LeaveCriticalSection(&StatusCS));
                        Sleep(sleepNow);
                        HANDLES(EnterCriticalSection(&StatusCS));
                        ti = GetTickCount();
                    }
                }
                else
                    CalcLimitBufferSize(limitBufferSize, bufferSize); // bez limitu - plna rychlost (az na ProgressBufferLimit)
            }
            TransferSpeedMeter.BytesReceived(bytesCountForSpeedMeters, ti, maxPacketSize);
            TransferredFileSize.Value += bytesCount;

            if (UseProgressBufferLimit &&
                (++LastFileBlockCount >= ASYNC_SLOW_COPY_BUF_MINBLOCKS || // pokud neni malo dat pro test
                 ProgressBufferLimit * LastFileBlockCount >= ASYNC_SLOW_COPY_BUF_MINBLOCKS * ASYNC_SLOW_COPY_BUF_SIZE) &&
                ti - LastProgBufLimTestTime >= 1000) // a je cas na dalsi test
            {                                        // napocitame ProgressBufferLimit pro pristi kolo (dalsi cteni jede jeste se stavajici hodnotou)
                TransferSpeedMeter.AdjustProgressBufferLimit(&ProgressBufferLimit, LastFileBlockCount, LastFileStartTime);
                LastProgBufLimTestTime = GetTickCount();
                if (LastFileBlockCount > 1000000000)
                    LastFileBlockCount = 1000000; // ochrana pred pretecenim (proste hafo bloku, presny pocet neni az tak dulezity)
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
                SleepAfterWrite = -1; // s prichodem prvniho paketu provedeme vypocet
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
            SleepAfterWrite = -1; // s prichodem prvniho paketu provedeme vypocet
            LastSetupTime = GetTickCount();
            BytesTrFromLastSetup.SetUI64(0);
        }
        // po pauze, zmene speed-limitu nebo error-dialogu ustrelime stara data
        if (operInProgress)
        {
            LastFileBlockCount = 0;
            LastFileStartTime = GetTickCount();
            LastProgBufLimTestTime = GetTickCount(); // a dalsi test odlozime o vterinu, abysme meli relevantni data
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
    void* Buffers[8];         // alokovane buffery o velikosti ASYNC_COPY_BUF_SIZE bytu
    OVERLAPPED Overlapped[8]; // struktury pro asynchronni operace

    BOOL UseAsyncAlg; // TRUE = ma se pouzit asynchronni algoritmus (musi se alokovat data), FALSE = synchroni stary algouritmus (nic nealokujeme)

    BOOL HasFailed; // TRUE = nepodarilo se vytvorit nejaky event do pole Overlapped, struktura je nepouzitelna

    CAsyncCopyParams();
    ~CAsyncCopyParams();

    void Init(BOOL useAsyncAlg);

    BOOL Failed() { return HasFailed; }

    DWORD GetOverlappedFlag() { return UseAsyncAlg ? FILE_FLAG_OVERLAPPED : 0; }

    OVERLAPPED* InitOverlapped(int i);                                    // vynuluje a vrati Overlapped[i]
    OVERLAPPED* InitOverlappedWithOffset(int i, const CQuadWord& offset); // vynuluje, nastavi 'offset' a vrati Overlapped[i]
    OVERLAPPED* GetOverlapped(int i) { return &Overlapped[i]; }
    void SetOverlappedToEOF(int i, const CQuadWord& offset); // nastavi Overlapped[i] do stavu po dokonceni asynchronniho cteni, ktere detekovalo EOF
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
    // Overlapped[i].Pointer = 0;  // je to union, Pointer se kryje s Offset a OffsetHigh
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
    // Overlapped[i].Pointer = 0;  // je to union, Pointer se kryje s Offset a OffsetHigh
    return &Overlapped[i];
}

void CAsyncCopyParams::SetOverlappedToEOF(int i, const CQuadWord& offset)
{
    if (!UseAsyncAlg)
        TRACE_C("CAsyncCopyParams::SetOverlappedToEOF(): unexpected call, UseAsyncAlg is FALSE!");
    Overlapped[i].Internal = 0xC0000011 /* STATUS_END_OF_FILE */; // NTSTATUS code equivalent to system error code ERROR_HANDLE_EOF
    Overlapped[i].InternalHigh = 0;
    Overlapped[i].Offset = offset.LoDWord;
    Overlapped[i].OffsetHigh = offset.HiDWord;
    // Overlapped[i].Pointer = 0;  // je to union, Pointer se kryje s Offset a OffsetHigh
    SetEvent(Overlapped[i].hEvent);
}

// **********************************************************************************

BOOL HaveWriteOwnerRight = FALSE; // ma proces pravo WRITE_OWNER?

void InitWorker()
{
    if (NtDLL != NULL) // "always true"
    {
        DynNtQueryInformationFile = (NTQUERYINFORMATIONFILE)GetProcAddress(NtDLL, "NtQueryInformationFile"); // nema header
        DynNtFsControlFile = (NTFSCONTROLFILE)GetProcAddress(NtDLL, "NtFsControlFile");                      // nema header
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

    BOOL OverwriteAll; // drzi stav automatickeho prepisovani targetu sourcem
    BOOL OverwriteHiddenAll;
    BOOL DeleteHiddenAll;
    BOOL EncryptSystemAll;
    BOOL DirOverwriteAll;
    BOOL FileOutLossEncrAll;
    BOOL DirCrLossEncrAll;

    BOOL SkipAllFileWrite; // pouzil se uz Skip All button?
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

    int CnfrmFileOver; // lokalni kopie konfigurace Salamandera
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
{                                                              // pockame si na odpoved, ksicht musi byt zmenen
    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
    if (!*dlgData.CancelWorker)                                // potrebujeme zastavit hl.thread
        SendMessage(hProgressDlg, WM_USER_SETDIALOG, (WPARAM)data, 0);
}

int CaclProg(const CQuadWord& progressCurrent, const CQuadWord& progressTotal)
{
    return progressCurrent >= progressTotal ? (progressTotal.Value == 0 ? 0 : 1000) : (int)((progressCurrent * CQuadWord(1000, 0)) / progressTotal).Value;
}

void SetProgress(HWND hProgressDlg, int operation, int summary, CProgressDlgData& dlgData)
{                                                              // oznamime zmenu a jedeme dal bez cekani na odpoved
    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
    if (!*dlgData.CancelWorker &&
        (*dlgData.OperationProgress != operation || *dlgData.SummaryProgress != summary))
    {
        *dlgData.OperationProgress = operation;
        *dlgData.SummaryProgress = summary;
        SendMessage(hProgressDlg, WM_USER_SETDIALOG, 0, 0);
    }
}

void SetProgressWithoutSuspend(HWND hProgressDlg, int operation, int summary, CProgressDlgData& dlgData)
{ // oznamime zmenu a jedeme dal bez cekani na odpoved
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
        number[0] = 0; // chyba - velikost neznama

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
    { // FindFirstFile selze pro 'dir' zakonceny backslashem (pouziva se pri invalidnich jmenech
        // adresaru), proto to resime v teto situaci pres CreateFile a GetFileTime
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

BOOL IsDirectoryEmpty(const char* name) // adresare/podadresare neobsahuji zadne soubory
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
                if (!IsDirectoryEmpty(dir)) // podadresar neni prazdny
                {
                    HANDLES(FindClose(search));
                    return FALSE;
                }
            }
            else
            {
                HANDLES(FindClose(search)); // existuje zde soubor
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
                        HaveWriteOwnerRight = TRUE; // podarilo se ziskat SE_RESTORE_NAME, WRITE_OWNER je garantovany
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
/*
//  Purpose:    Determines if the user is a member of the administrators group.
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

/* dle http://vcfaq.mvps.org/sdk/21.htm */
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

struct CSrcSecurity // pomocna metoda pro drzeni security-info pro MoveFile (zdroj po operaci zmizi, jeho security-info je treba ulozit predem)
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
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak
    // GetNamedSecurityInfo (a dalsi) mezery/tecky orizne a pracuje
    // tak s jinou cestou
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
    if (srcSecurity != NULL) // MoveFile: jen prevezmeme security-info
    {
        srcOwner = srcSecurity->SrcOwner;
        srcGroup = srcSecurity->SrcGroup;
        srcDACL = srcSecurity->SrcDACL;
        srcSD = srcSecurity->SrcSD;
        *err = srcSecurity->SrcError;
        srcSecurity->Clear();
    }
    else // ziskame security-info ze zdroje
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
            BOOL inheritedDACL = /*(srcSDControl & SE_DACL_AUTO_INHERITED) != 0 &&*/ (srcSDControl & SE_DACL_PROTECTED) == 0; // SE_DACL_AUTO_INHERITED bohuzel neni vzdy nastaveno (napr. Total Cmd ho po presunu souboru nuluje, takze ho ignorujeme)
            DWORD attr = GetFileAttributes(targetNameSec);
            *err = SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT,
                                        DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION |
                                            (inheritedDACL ? UNPROTECTED_DACL_SECURITY_INFORMATION : PROTECTED_DACL_SECURITY_INFORMATION),
                                        srcOwner, srcGroup, srcDACL, NULL);
            ret = *err == ERROR_SUCCESS;

            if (!ret)
            {
                // pokud nejde menit ownera + groupu (nemame na to prava v adresari - mame napr. jen "change" prava),
                // zkontrolujeme jestli jiz nejsou owner + groupa nastavene (to by totiz nebyla chyba)
                PSID tgtOwner = NULL;
                PSID tgtGroup = NULL;
                PACL tgtDACL = NULL;
                PSECURITY_DESCRIPTOR tgtSD = NULL;
                BOOL tgtRead = GetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT,
                                                    DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                                    &tgtOwner, &tgtGroup, &tgtDACL, NULL, &tgtSD) == ERROR_SUCCESS;
                // pokud neni owner ciloveho souboru current-user, zkusime ho nastavit ("take ownership") - jen
                // pokud mame pravo zapisu ownera, abysme pak zase mohli zapsat zpet puvodniho vlastnika
                BOOL ownerOfFile = FALSE;
                if (!tgtRead ||         // pokud nejde nacist security-info z cile, nejspis neni owner current-user (jako owner ma prava ke cteni neblokovatelne)
                    tgtOwner == NULL || // nejspis ptakovina, soubor musi mit nejakeho vlastnika, pokud nastane, zkusime nastavit ownera na current-usera
                    CurrentProcessTokenUserValid && CurrentProcessTokenUser->User.Sid != NULL &&
                        !EqualSid(tgtOwner, CurrentProcessTokenUser->User.Sid))
                {
                    if (HaveWriteOwnerRight &&
                        CurrentProcessTokenUserValid && CurrentProcessTokenUser->User.Sid != NULL &&
                        SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                                             CurrentProcessTokenUser->User.Sid, NULL, NULL, NULL) == ERROR_SUCCESS)
                    { // nastaveni se podarilo, musime znovu ziskat 'tgtSD'
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
                { // jsme vlastnici souboru -> DACL zapisovat jde, zkusime si povolit zapis ownera+groupy+DACLu a zkusime zapsat potrebne hodnoty
                    int allowChPermDACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(ACCESS_ALLOWED_ACE().SidStart) +
                                              GetLengthSid(CurrentProcessTokenUser->User.Sid) + 200 /* +200 bytu je jen paranoia */;
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
                     tgtRead && (srcOwner == NULL && tgtOwner == NULL || // pokud jiz je owner nastaveny, ignorujeme pripadnou chybu nastavovani
                                 srcOwner != NULL && tgtOwner != NULL && EqualSid(srcOwner, tgtOwner))))
                {
                    ownerOK = TRUE;
                }
                if (!groupOK &&
                    (SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT, GROUP_SECURITY_INFORMATION,
                                          NULL, srcGroup, NULL, NULL) == ERROR_SUCCESS ||
                     tgtRead && (srcGroup == NULL && tgtGroup == NULL || // pokud jiz je group nastaveny, ignorujeme pripadnou chybu nastavovani
                                 srcGroup != NULL && tgtGroup != NULL && EqualSid(srcGroup, tgtGroup))))
                {
                    groupOK = TRUE;
                }
                if (!daclOK && // DACL musime nastavovat az posledni, protoze zavisi na ownerovi (CREATOR OWNER se nahrazuje realnym ownerem, atd.)
                    SetNamedSecurityInfo((char*)targetNameSec, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | (inheritedDACL ? UNPROTECTED_DACL_SECURITY_INFORMATION : PROTECTED_DACL_SECURITY_INFORMATION),
                                         NULL, NULL, srcDACL, NULL) == ERROR_SUCCESS)
                {
                    daclOK = TRUE;
                }
                if (ownerOK && groupOK && daclOK)
                {
                    ret = TRUE; // vsechny tri slozky jsou OK -> celek je OK
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
        return ret; // uz je kompresenej

    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak CreateFile
    // mezery/tecky orizne a pracuje tak s jinou cestou
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
        SetFileAttributes(fileNameCrFile, attrs); // navrat k puvodnim atributum (pri chybe se atributy neprenastavi a zustavali nesmyslne zmenene)
    return ret;
}

DWORD UncompressFile(char* fileName, DWORD attrs)
{
    DWORD ret = ERROR_SUCCESS;
    if ((attrs & FILE_ATTRIBUTE_COMPRESSED) == 0)
        return ret; // neni kompresenej

    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak CreateFile
    // mezery/tecky orizne a pracuje tak s jinou cestou
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
        SetFileAttributes(fileNameCrFile, attrs); // navrat k puvodnim atributum (pri chybe se atributy neprenastavi a zustavali nesmyslne zmenene)
    return ret;
}

DWORD MyEncryptFile(HWND hProgressDlg, char* fileName, DWORD attrs, DWORD finalAttrs,
                    CProgressDlgData& dlgData, BOOL& cancelOper, BOOL preserveDate)
{
    DWORD retEnc = ERROR_SUCCESS;
    cancelOper = FALSE;
    if (attrs & FILE_ATTRIBUTE_ENCRYPTED)
        return retEnc; // uz je sifrovany

    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak CreateFile
    // mezery/tecky orizne a pracuje tak s jinou cestou
    const char* fileNameCrFile = fileName;
    char fileNameCrFileCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(fileNameCrFile, fileNameCrFileCopy);

    // ma-li soubor SYSTEM atribut, hlasi API funkce EncryptFile chybu "access denied", resime:
    if ((attrs & FILE_ATTRIBUTE_SYSTEM) && (finalAttrs & FILE_ATTRIBUTE_SYSTEM))
    { // pokud ma a bude mit atribut SYSTEM, musime se optat usera jestli to mysli vazne
        if (!dlgData.EncryptSystemAll)
        {
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
        SetFileAttributes(fileNameCrFile, attrs); // navrat k puvodnim atributum (pri chybe se atributy neprenastavi a zustavali nesmyslne zmenene)
    return retEnc;
}

DWORD MyDecryptFile(char* fileName, DWORD attrs, BOOL preserveDate)
{
    DWORD ret = ERROR_SUCCESS;
    if ((attrs & FILE_ATTRIBUTE_ENCRYPTED) == 0)
        return ret; // neni sifrovany

    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak CreateFile
    // mezery/tecky orizne a pracuje tak s jinou cestou
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
        SetFileAttributes(fileNameCrFile, attrs); // navrat k puvodnim atributum (pri chybe se atributy neprenastavi a zustavali nesmyslne zmenene)
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
        // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak CreateFile
        // mezery/tecky orizne a pracuje tak s jinou cestou
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
        BYTE buffer[65535]; // XPcka vic nez 65535 nesnesou (netusim proc)
        uStatus = DynNtQueryInformationFile(file, &ioStatus, buffer, sizeof(buffer), FileStreamInformation);
        HANDLES(CloseHandle(file));
        if (uStatus != 0 /* cokoliv krome uspechu je chyba (i warningy) */)
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
        if (ioStatus.Information > 0) // zkontrolujeme, jestli jsme vubec ziskali nejaka data
        {
            while (1)
            {
                if (psi->NameLength != 7 * 2 || _memicmp(psi->Name, L"::$DATA", 7 * 2)) // ignore default stream
                {
                    ret = TRUE;
                    if (adsSize != NULL)
                        *adsSize += CQuadWord(psi->Size.LowPart, psi->Size.HighPart); // soucet celkove velikosti vsech alternate-data-streamu
                    if (adsOccupiedSpace != NULL && bytesPerCluster != 0)
                    {
                        CQuadWord fileSize(psi->Size.LowPart, psi->Size.HighPart);
                        *adsOccupiedSpace += fileSize - ((fileSize - CQuadWord(1, 0)) % CQuadWord(bytesPerCluster, 0)) +
                                             CQuadWord(bytesPerCluster - 1, 0);
                    }

                    if (onlyDiscardableStreams != NULL)
                    {                                                                                                                      // pokud se objevi ADS, ktery je neznamy nebo nepostradatelny, prepneme 'onlyDiscardableStreams' na FALSE
                        if ((psi->NameLength < 29 * 2 || _memicmp(psi->Name, L":\x05Q30lsldxJoudresxAaaqpcawXc:", 29 * 2) != 0) &&         // Win2K thumbnail v ADSku: 5952 bytu (zavisi na kompresy jpegu)
                            (psi->NameLength < 40 * 2 || _memicmp(psi->Name, L":{4c8cc155-6c1e-11d1-8e41-00c04fb9386d}:", 40 * 2) != 0) && // Win2K thumbnail v ADSku: 0 bytu
                            (psi->NameLength < 9 * 2 || _memicmp(psi->Name, L":KAVICHS:", 9 * 2) != 0))                                    // Kasperski antivirus: 36/68 bytes
                        {
                            *onlyDiscardableStreams = FALSE;
                        }
                    }

                    if (streamNamesAux != NULL) // sbirani Unicode jmen alternate-data-streamu
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
                            break; // dale uz neni co zjistovat (nesbira jmena, ani velikosti streamu, ani only-discardable-streams)
                    }
                }
                if (psi->NextEntry == 0)
                    break;
                psi = (PFILE_STREAM_INFORMATION)((BYTE*)psi + psi->NextEntry); // move to next item
            }
        }
        if (streamNamesAux != NULL)
        {
            if (lowMem || !ret) // nedostatek pameti nebo zadne ADS, uvolnime vsechna jmena
            {
                int i;
                for (i = 0; i < streamNamesAux->Count; i++)
                    free(streamNamesAux->At(i));
            }
            else // vse OK, predame jmena volajicimu
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
        BYTE buffer[65535]; // XPcka vic nez 65535 nesnesou (netusim proc)
        uStatus = DynNtQueryInformationFile(file, &ioStatus, buffer, sizeof(buffer), FileStreamInformation);
        if (uStatus != 0 /* cokoliv krome uspechu je chyba (i warningy) */)
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
        if (ioStatus.Information > 0) // zkontrolujeme, jestli jsme vubec ziskali nejaka data
        {
            WCHAR adsFullName[2 * MAX_PATH];
            adsFullName[0] = 0;
            WCHAR* adsPart = NULL;
            int adsPartSize = 0;
            while (1)
            {
                if (psi->NameLength != 7 * 2 || _memicmp(psi->Name, L"::$DATA", 7 * 2)) // ignore default stream
                {
                    if (adsFullName[0] == 0) // jmeno souboru prevadima az pri prvni potrebe, setrime strojovym casem
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

// prevod do prodlouzene varianty cest, viz MSDN clanek "File Name Conventions"
void DoLongName(char* buf, const char* name, int bufSize)
{
    if (*name == '\\')
        _snprintf_s(buf, bufSize, _TRUNCATE, "\\\\?\\UNC%s", name + 1); // UNC
    else
        _snprintf_s(buf, bufSize, _TRUNCATE, "\\\\?\\%s", name); // klasicka cesta
}

BOOL SalSetFilePointer(HANDLE file, const CQuadWord& offset)
{
    LONG lo = offset.LoDWord;
    LONG hi = offset.HiDWord;
    lo = SetFilePointer(file, lo, &hi, FILE_BEGIN);
    return (lo != INVALID_SET_FILE_POINTER || GetLastError() == NO_ERROR) &&
           lo == (LONG)offset.LoDWord && hi == (LONG)offset.HiDWord;
}

#define RETRYCOPY_TAIL_MINSIZE (32 * 1024) // minimalne dva bloky teto velikosti se zkontroluji na konci souboru, ktery se otestuje v CheckTailOfOutFile(); pak se velikost bloku zvysuje az k ASYNC_COPY_BUF_SIZE (je-li cteni dost rychle); POZOR: musi byt <= nez ASYNC_COPY_BUF_SIZE
#define RETRYCOPY_TESTINGTIME 3000         // doba testovani v [ms] pro CheckTailOfOutFile()

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
            break; // neni co kontrolovat
        }
#ifdef WORKER_COPY_DEBUG_MSG
        TRACE_I("CheckTailOfOutFile(): check: " << start.Value << " - " << lastOffset.Value << ", size: " << size);
#endif // WORKER_COPY_DEBUG_MSG
        if (asyncPar == NULL)
        {
            if (SalSetFilePointer(in, start))
            { // nastavime 'start' offset v in
                if (SalSetFilePointer(out, start))
                { // nastavime 'start' offset v out
                    DWORD read;
                    if (ReadFile(out, bufOut, size, &read, NULL) && read == size)
                    { // precteme 'size' bajtu do out bufferu (je-li otevren bez "read" accessu, selze)
                        if (ReadFile(in, bufIn, size, &read, NULL) && read == size)
                        {                                         // precteme 'size' bajtu do in bufferu
                            if (memcmp(bufIn, bufOut, size) == 0) // porovname, jestli jsou in/out buffery shodne
                                ok = TRUE;
                            else
                                TRACE_I("CheckTailOfOutFile(): tail of target file is different from source file, tail without differences: " << (offset.Value - lastOffset.Value));
                        }
                        else
                            CheckTailOfOutFileShowErr("Unable to read IN file.");
                    }
                    else
                    {
                        if (ignoreReadErrOnOut) // pokud nastala chyba na in souboru, ignorujeme, ze out nemuzeme cist (in byl znovu otevreny a out je celou dobu otevreny)
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
            // asynchronne precist z in a out blok 'start' o velikosti 'size' bajtu a porovnat
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
                        memcmp(bufIn, bufOut, size) != 0) // porovname, jestli jsou in/out buffery shodne
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
                if (ignoreReadErrOnOut) // pokud nastala chyba na in souboru, ignorujeme, ze out nemuzeme cist (in byl znovu otevreny a out je celou dobu otevreny)
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
                if (roundNum == 2 && t1 > 300 && 10 * t2 < t1) // prvni kolo obsahuje cekani na "pripravenost" disku/site, upravime startovni cas (chceme po zvoleny cas cist a ne jen cekat)
                {
#ifdef WORKER_COPY_DEBUG_MSG
                    TRACE_I("CheckTailOfOutFile(): detected long lasting first block, start time shifted by " << ((roundStartTime - startTime) / 1000.0) << " secs.");
#endif // WORKER_COPY_DEBUG_MSG
                    startTime = roundStartTime;
                }
                else
                {
                    if (t2 > 1000 && ((curBufSize * 10) / lastRoundBufSize) * t1 < t2)
                    { // necekane dlouhe cteni bloku, nejspis obsahuje cekani na "verifikaci" disku ci co, pro jeden blok tento cas vyignorujeme, aby i tak probehla normalni kontrola
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
                break; // uz cteme dost dlouho, koncime (povina dve kola jsou za nami)
            if (ti - roundStartTime < 300 && curBufSize < ASYNC_COPY_BUF_SIZE)
            { // pokud jsme dost rychly, zvetsime buffer, at zbytecne casto neseekujeme (seekujeme neprirozene smerem k zacatku souboru)
                curBufSize *= 2;
                if (curBufSize > ASYNC_COPY_BUF_SIZE)
                    curBufSize = ASYNC_COPY_BUF_SIZE;
            }
        }
        roundNum++;
        lastRoundStartTime = roundStartTime;
        lastRoundBufSize = curBufSizeBackup;
    }

    if (ok && asyncPar == NULL) // nastavime in+out na potrebne offsety
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

// nakopiruje ADS do nove vznikleho souboru/adresare
// FALSE vraci jen pri Cancel; uspech + Skip vraci TRUE; Skip nastavuje 'skip'
// (neni-li NULL) na TRUE
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
    if (operTotal > operDone) // melo by byt vzdycky aspon ==, ale sychrujeme se...
        finalTransferredFileSize += (operTotal - operDone);

COPY_ADS_AGAIN:

    if (CheckFileOrDirADS(sourceName, isDir, NULL, &streamNames, &streamNamesCount,
                          &lowMemory, &adsWinError, 0, NULL, NULL) &&
        !lowMemory && streamNames != NULL)
    {                                  // mame seznam ADS, zkusime je nakopirovat do ciloveho souboru/adresare
        wchar_t srcName[2 * MAX_PATH]; // MAX_PATH pro jmeno souboru i pro jmeno ADS (netusim kolik jsou realne maximalni delky)
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

                            // pokud je to mozne, provedeme alokaci potrebneho mista pro soubor (nedochazi pak k fragmentaci disku + hladsi zapis na diskety)
                            BOOL wholeFileAllocated = FALSE;
                            if (fileSize > CQuadWord(limitBufferSize, 0) && // pod velikost kopirovaciho bufferu nema alokace souboru smysl
                                fileSize < CQuadWord(0, 0x80000000))        // velikost souboru je kladne cislo (jinak nelze seekovat - jde o cisla nad 8EB, takze zrejme nikdy nenastane)
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
                                            ignoreErr = TRUE; // malo mista na disku
                                    }
                                }
                                if (fatal)
                                {
                                    if (!ignoreErr)
                                    {
                                        DWORD err = GetLastError();
                                        TRACE_E("DoCopyADS(): unable to allocate whole file size before copy operation, please report under what conditions this occurs! GetLastError(): " << GetErrorText(err));
                                    }

                                    // zkusime jeste soubor zkratit na nulu, aby nedoslo pri zavreni souboru k nejakemu zbytecnemu zapisu
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
                                    if (!script->ChangeSpeedLimit)                                 // pokud se muze zmenit speed-limit, tady neni "vhodne" misto pro cekani
                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                                    if (*dlgData.CancelWorker)
                                    {
                                    COPY_ERROR_ADS:

                                        if (in != NULL)
                                            HANDLES(CloseHandle(in));
                                        if (out != NULL)
                                        {
                                            if (wholeFileAllocated)
                                                SetEndOfFile(out); // u floppy by se jinak zapisoval zbytek souboru
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

                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                        case IDRETRY: // na siti musime otevrit znovu handle, na lokale to nedovoli sharing
                                        {
                                            if (in == NULL && out == NULL)
                                            {
                                                DeleteFileW(tgtName);
                                                goto COPY_AGAIN_ADS;
                                            }
                                            if (out != NULL)
                                            {
                                                if (wholeFileAllocated)
                                                    SetEndOfFile(out);     // u floppy by se jinak zapisoval zbytek souboru
                                                HANDLES(CloseHandle(out)); // zavreme invalidni handle
                                            }
                                            out = CreateFileW(tgtName, GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_ALWAYS,
                                                              FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                                            HANDLES_ADD_EX(__otQuiet, out != INVALID_HANDLE_VALUE, __htFile,
                                                           __hoCreateFile, out, GetLastError(), TRUE);
                                            if (out != INVALID_HANDLE_VALUE) // otevreno, jeste nastavime offset
                                            {
                                                LONG lo, hi;
                                                lo = GetFileSize(out, (DWORD*)&hi);
                                                if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ||
                                                    CQuadWord(lo, hi) < operationDone ||
                                                    !CheckTailOfOutFile(NULL, in, out, operationDone, operationDone + CQuadWord(read, 0), FALSE))
                                                { // nelze ziskat velikost nebo je soubor prilis maly, jedeme to cely znovu
                                                    HANDLES(CloseHandle(in));
                                                    HANDLES(CloseHandle(out));
                                                    DeleteFileW(tgtName);
                                                    goto COPY_AGAIN_ADS;
                                                }
                                            }
                                            else // nejde otevrit, problem trva ...
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
                                                    SetEndOfFile(out); // u floppy by se jinak zapisoval zbytek souboru
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
                                    if (!script->ChangeSpeedLimit)                                 // pokud se muze zmenit speed-limit, tady neni "vhodne" misto pro cekani
                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                                    if (*dlgData.CancelWorker)
                                        goto COPY_ERROR_ADS;

                                    script->AddBytesToSpeedMetersAndTFSandPS(read, FALSE, bufferSize, &limitBufferSize);

                                    if (!script->ChangeSpeedLimit)                                 // pokud se muze zmenit speed-limit, tady neni "vhodne" misto pro cekani
                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                                    operationDone += CQuadWord(read, 0);
                                    SetProgressWithoutSuspend(hProgressDlg, CaclProg(operDone + operationDone, operTotal),
                                                              CaclProg(totalDone + operDone + operationDone, script->TotalSize),
                                                              dlgData);

                                    if (script->ChangeSpeedLimit)                                  // asi se bude menit speed-limit, zde je "vhodne" misto na cekani, az se
                                    {                                                              // worker zase rozbehne, ziskame znovu velikost bufferu pro kopirovani
                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                                        script->GetNewBufSize(&limitBufferSize, bufferSize);
                                    }
                                }
                                else
                                {
                                READ_ERROR_ADS:

                                    DWORD err;
                                    err = GetLastError();
                                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                            HANDLES(CloseHandle(in)); // zavreme invalidni handle

                                        in = CreateFileW(srcName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                                        HANDLES_ADD_EX(__otQuiet, in != INVALID_HANDLE_VALUE, __htFile,
                                                       __hoCreateFile, in, GetLastError(), TRUE);
                                        if (in != INVALID_HANDLE_VALUE) // otevreno, jeste nastavime offset
                                        {
                                            LONG lo, hi;
                                            lo = GetFileSize(in, (DWORD*)&hi);
                                            if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ||
                                                CQuadWord(lo, hi) < operationDone ||
                                                !CheckTailOfOutFile(NULL, in, out, operationDone, operationDone, TRUE))
                                            { // nelze ziskat velikost nebo je soubor prilis maly, jedeme to cely znovu
                                                HANDLES(CloseHandle(in));
                                                if (wholeFileAllocated)
                                                    SetEndOfFile(out); // u floppy by se jinak zapisoval zbytek souboru
                                                HANDLES(CloseHandle(out));
                                                DeleteFileW(tgtName);
                                                goto COPY_AGAIN_ADS;
                                            }
                                        }
                                        else // nejde otevrit, problem trva ...
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

                            if (wholeFileAllocated &&     // alokovali jsme kompletni podobu souboru
                                operationDone < fileSize) // a zdrojovy soubor se zmensil
                            {
                                if (!SetEndOfFile(out)) // musime ho zde zkratit
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
                            if (!HANDLES(CloseHandle(out))) // i po neuspesnem volani predpokladame, ze je handle zavreny,
                            {                               // viz https://forum.altap.cz/viewtopic.php?f=6&t=8455
                                in = out = NULL;            // (pise, ze cilovy soubor lze smazat, tedy nezustal otevreny jeho handle)
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

                            // podpora pro MACintose: NTFS automaticky generuje ADSka myFile:Afp_Resource a myFile:Afp_AfpInfo,
                            // bez dotazu je prepiseme verzemi ze zdrojoveho souboru
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

                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                dlgData.IgnoreAllADSOpenOutErr = TRUE; // tady break; nechybi
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
                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
        if (adsWinError != NO_ERROR) // musime zobrazit windows chybu (low memory jde jen do TRACE_E)
        {
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                dlgData.IgnoreAllADSReadErr = TRUE; // tady break; nechybi
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
            doCopyADSRet = FALSE; // nedostatek pameti -> cancelujeme operaci
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
        { // pro pripad, ze na cilovem disku nelze vytvorit Encrypted soubor (hrozi na NTFS sitovem disku (ladeno na sharu z XPcek), na ktery jsme zalogovany pod jinym jmenem nez mame v systemu (na soucasny konzoli) - trik je v tom, ze na cilovem systemu je user se stejnym jmenem bez hesla, tedy sitove nepouzitelny)
            out = NOHANDLES(CreateFile(fileName, desiredAccess, shareMode, NULL,
                                       CREATE_NEW, (flagsAndAttributes & ~(FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_READONLY)), NULL));
            if (out != INVALID_HANDLE_VALUE)
            {
                *encryptionNotSupported = TRUE;
                NOHANDLES(CloseHandle(out));
                out = INVALID_HANDLE_VALUE;
                if (!DeleteFile(fileName)) // XP ani Vista si s tim hlavu nelamou, tak na to tez kaslu (ono taky co s tim, ze, leda varovat usera, ze jsme mu pridali nulovy soubor na disk, odkud ho nejde smazat)
                    TRACE_I("Unable to delete testing target file: " << fileName);
            }
        }
        if (err == ERROR_FILE_EXISTS || // zkontrolujeme, jestli nejde jen o prepis dosoveho jmena souboru
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
                    if (StrICmp(tgtName, data.cAlternateFileName) == 0 && // shoda jen pro dos name
                        StrICmp(tgtName, data.cFileName) != 0)            // (plne jmeno je jine)
                    {
                        // prejmenujeme ("uklidime") soubor/adresar s konfliktnim dos name do docasneho nazvu 8.3 (nepotrebuje extra dos name)
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
                            if (tmpName[0] != 0) // pokud se podarilo "uklidit" konfliktni soubor, zkusime vytvoreni
                            {                    // ciloveho souboru znovu, pak vratime "uklizenemu" souboru jeho puvodni jmeno
                                out = NOHANDLES(CreateFile(fileName, desiredAccess, shareMode, NULL,
                                                           CREATE_NEW, flagsAndAttributes, NULL));
                                if (out == INVALID_HANDLE_VALUE && encryptionNotSupported != NULL &&
                                    (flagsAndAttributes & FILE_ATTRIBUTE_ENCRYPTED))
                                { // pro pripad, ze na cilovem disku nelze vytvorit Encrypted soubor (hrozi na NTFS sitovem disku (ladeno na sharu z XPcek), na ktery jsme zalogovany pod jinym jmenem nez mame v systemu (na soucasny konzoli) - trik je v tom, ze na cilovem systemu je user se stejnym jmenem bez hesla, tedy sitove nepouzitelny)
                                    out = NOHANDLES(CreateFile(fileName, desiredAccess, shareMode, NULL,
                                                               CREATE_NEW, (flagsAndAttributes & ~(FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_READONLY)), NULL));
                                    if (out != INVALID_HANDLE_VALUE)
                                    {
                                        *encryptionNotSupported = TRUE;
                                        NOHANDLES(CloseHandle(out));
                                        out = INVALID_HANDLE_VALUE;
                                        if (!DeleteFile(fileName)) // XP ani Vista si s tim hlavu nelamou, tak na to tez kaslu (ono taky co s tim, ze, leda varovat usera, ze jsme mu pridali nulovy soubor na disk, odkud ho nejde smazat)
                                            TRACE_E("Unable to delete testing target file: " << fileName);
                                    }
                                }
                                if (!SalMoveFile(tmpName, origFullName))
                                { // toto se zjevne muze stat, nepochopitelne, ale Windows vytvori misto fileName (dos name) soubor se jmenem origFullName
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
                                        SetFileAttributes(origFullName, origFullNameAttr); // nechame bez osetreni a retry, nedulezite (normalne se nastavuje chaoticky)
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
    if (asyncPar->UseAsyncAlg) // asynchronni varianta
    {
        if (!DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer,
                             nOutBufferSize, NULL, asyncPar->InitOverlapped(0)) &&
                GetLastError() != ERROR_IO_PENDING ||
            !GetOverlappedResult(hDevice, asyncPar->GetOverlapped(0), lpBytesReturned, TRUE))
        { // chyba, vracime FALSE
            *err = GetLastError();
            return FALSE;
        }
    }
    else // synchronni varianta
    {
        if (!DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer,
                             nOutBufferSize, lpBytesReturned, NULL))
        { // chyba, vracime FALSE
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
        { // v SalCreateFileEx (viz vyse) to nejspis nezabralo
            err = NO_ERROR;
            HANDLES(CloseHandle(*out)); // zavreme soubor, jinak mu bohuzel nemuzeme menit Encrypt atribut
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
            // otevreme existujici soubor a provedeme zapis
            *out = HANDLES_Q(CreateFile(name, GENERIC_WRITE | (openAlsoForRead ? GENERIC_READ : 0), 0, NULL, OPEN_ALWAYS,
                                        asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
            if (openAlsoForRead && *out == INVALID_HANDLE_VALUE) // prusvih, soubor nejde znovu otevrit, zkusime ho jeste otevrit jen pro zapis
            {
                *out = HANDLES_Q(CreateFile(name, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                                            asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
            }
            if (*out == INVALID_HANDLE_VALUE) // prusvih, soubor nejde znovu otevrit, zkusime ho smazat + ohlasime chybu
            {
                err = GetLastError();
                DeleteFile(name);
                SetLastError(err);
            }
        }
        if (*out != INVALID_HANDLE_VALUE && // jen pokud nedoslo chybe pri znovu otevirani souboru (a nasledne k jeho smazani)
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
            return; // nepodarilo se nacist data ciloveho souboru, koncime
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
        lastTransferredFileSize += op->FileSize;                      // velikost souboru
        if (op->Size > COPY_MIN_FILE_SIZE)                            // melo by byt vzdycky aspon COPY_MIN_FILE_SIZE, ale sychrujeme se...
            lastTransferredFileSize += op->Size - COPY_MIN_FILE_SIZE; // pricteme velikost ADSek
    }
    else
        lastTransferredFileSize += op->Size; // velikost souboru + ADSek
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
            if (!script->ChangeSpeedLimit)                                 // pokud se muze zmenit speed-limit, tady neni "vhodne" misto pro cekani
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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

                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                case IDRETRY: // na siti musime otevrit znovu handle, na lokale to nedovoli sharing
                {
                    if (out != NULL)
                    {
                        if (wholeFileAllocated)
                            SetEndOfFile(out);     // u floppy by se jinak zapisoval zbytek souboru
                        HANDLES(CloseHandle(out)); // zavreme invalidni handle
                    }
                    out = HANDLES_Q(CreateFile(op->TargetName, GENERIC_WRITE | GENERIC_READ, 0, NULL,
                                               OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                    if (out != INVALID_HANDLE_VALUE) // otevreno, jeste nastavime offset
                    {
                        LONG lo, hi;
                        lo = GetFileSize(out, (DWORD*)&hi);
                        if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR || // nelze ziskat velikost
                            CQuadWord(lo, hi) < operationDone ||                     // soubor je prilis maly
                            wholeFileAllocated && CQuadWord(lo, hi) > fileSize &&
                                CQuadWord(lo, hi) > operationDone + CQuadWord(read, 0) || // predalokovany soubor je prilis velky (vetsi nez predalokovana velikost a zaroven vetsi nez zapsana velikost vcetne posledniho zapisovaneho bloku) = doslo k pridani zapisovanych bytu na konec souboru (allocWholeFileOnStart by melo byt 0 /* need-test */)
                            !CheckTailOfOutFile(NULL, in, out, operationDone, operationDone + CQuadWord(read, 0), FALSE))
                        { // jedeme to cely znovu
                            HANDLES(CloseHandle(in));
                            HANDLES(CloseHandle(out));
                            DeleteFile(op->TargetName);
                            copyAgain = TRUE; // goto COPY_AGAIN;
                            return;
                        }
                    }
                    else // nejde otevrit, problem trva ...
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
            if (!script->ChangeSpeedLimit)                                 // pokud se muze zmenit speed-limit, tady neni "vhodne" misto pro cekani
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
            if (*dlgData.CancelWorker)
            {
                copyError = TRUE; // goto COPY_ERROR
                return;
            }

            script->AddBytesToSpeedMetersAndTFSandPS(read, FALSE, bufferSize, &limitBufferSize);

            if (!script->ChangeSpeedLimit)                                 // pokud se muze zmenit speed-limit, tady neni "vhodne" misto pro cekani
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
            operationDone += CQuadWord(read, 0);
            SetProgressWithoutSuspend(hProgressDlg, CaclProg(operationDone, op->Size),
                                      CaclProg(totalDone + operationDone, script->TotalSize), dlgData);

            if (script->ChangeSpeedLimit)                                  // asi se bude menit speed-limit, zde je "vhodne" misto na cekani, az se
            {                                                              // worker zase rozbehne, ziskame znovu velikost bufferu pro kopirovani
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                script->GetNewBufSize(&limitBufferSize, bufferSize);
            }
        }
        else
        {
        READ_ERROR:

            DWORD err;
            err = GetLastError();
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
            { // na SNAP serveru dochazi pri cteni souboru k nahodnemu vyskytu chyby ERROR_NETNAME_DELETED, tlacitko Retry pry funguje, zkusime ho tedy automaticky
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
                    HANDLES(CloseHandle(in)); // zavreme invalidni handle
                in = HANDLES_Q(CreateFile(op->SourceName, GENERIC_READ,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                          OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                if (in != INVALID_HANDLE_VALUE) // otevreno, jeste nastavime offset
                {
                    LONG lo, hi;
                    lo = GetFileSize(in, (DWORD*)&hi);
                    if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ||
                        CQuadWord(lo, hi) < operationDone ||
                        !CheckTailOfOutFile(NULL, in, out, operationDone, operationDone, TRUE))
                    { // nelze ziskat velikost nebo je soubor prilis maly, jedeme to cely znovu
                        HANDLES(CloseHandle(in));
                        if (wholeFileAllocated)
                            SetEndOfFile(out); // u floppy by se jinak zapisoval zbytek souboru
                        HANDLES(CloseHandle(out));
                        DeleteFile(op->TargetName);
                        copyAgain = TRUE; // goto COPY_AGAIN;
                        return;
                    }
                }
                else // nejde otevrit, problem trva ...
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

    if (wholeFileAllocated) // alokovali jsme kompletni podobu souboru (znamena, ze alokace mela smysl, napr. soubor nemuze byt nulovy)
    {
        if (operationDone < fileSize) // a zdrojovy soubor se zmensil
        {
            if (!SetEndOfFile(out)) // musime ho zde zkratit
            {
                written = read = 0;
                goto WRITE_ERROR;
            }
        }

        if (allocWholeFileOnStart == 0 /* need-test */)
        {
            CQuadWord curFileSize;
            curFileSize.LoDWord = GetFileSize(out, &curFileSize.HiDWord);
            BOOL getFileSizeSuccess = (curFileSize.LoDWord != INVALID_FILE_SIZE || GetLastError() == NO_ERROR);
            if (getFileSizeSuccess && curFileSize == operationDone)
            { // kontrola, jestli nedoslo k pridani zapisovanych bytu na konec souboru + jestli umime soubor zkratit
                allocWholeFileOnStart = 1 /* yes */;
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
                allocWholeFileOnStart = 2 /* no */; // dalsi pokusy na tomhle cilovem disku si odpustime

                HANDLES(CloseHandle(out));
                out = NULL;
                ClearReadOnlyAttr(op->TargetName); // kdyby vznikl jako read-only (nemelo by nikdy nastat), tak abychom si s nim poradili
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
    cbsFree,       // blok se nepouziva
    cbsRead,       // cteni zdrojoveho souboru do bloku - dokoncene (ceka na zapis)
    cbsInProgress, // --- dole jsou stavy bloku "ceka se na dokonceni operace" (nahore jsou dokoncene stavy)
    cbsReading,    // cteni zdrojoveho souboru do bloku - zadane (probiha)
    cbsTestingEOF, // testovani konce zdrojoveho souboru
    cbsWriting,    // zapis bloku do ciloveho souboru
    cbsDiscarded,  // cteni zdrojoveho souboru za koncem souboru (melo by vratit jen error: EOF)
};

enum CCopy_ForceOp
{
    fopNotUsed, // muzeme cist nebo zapisovat, jak se zrovna hodi...
    fopReading, // musime cist
    fopWriting  // musime zapisovat
};

struct CCopy_Context
{
    CAsyncCopyParams* AsyncPar;

    CCopy_ForceOp ForceOp;        // TRUE = ted se musi cist, FALSE = ted se musi zapisovat
    BOOL ReadingDone;             // TRUE = zdrojovy soubor uz je komplet nacteny
    CCopy_BlkState BlockState[8]; // stav bloku
    DWORD BlockDataLen[8];        // pro kazdy blok: ocekavana data (cbsReading + cbsTestingEOF), platna data (cbsWriting)
    CQuadWord BlockOffset[8];     // pro kazdy blok: offset bloku ve zdrojovem/cilovem souboru (je i v OVERLAPPED strukture v 'AsyncPar')
    DWORD BlockTime[8];           // pro kazdy blok: "cas" zahajeni posledni asynchronni operace v tomto bloku
    DWORD CurTime;                // "cas" pro 'BlockTime', pocitame s pretecenim (i kdyz je asi nerealne)
    int FreeBlocks;               // aktualni pocet volnych bloku (cbsFree)
    int FreeBlockIndex;           // tip na index bloku, ktery je volny (cbsFree), nutno overit!
    int ReadingBlocks;            // aktualni pocet bloku, do kterych se zrovna cte (cbsReading a cbsTestingEOF)
    int WritingBlocks;            // aktualni pocet bloku, ze kterych se zrovna zapisuje do souboru (cbsWriting)
    CQuadWord ReadOffset;         // offset pro cteni dalsiho bloku ze zdrojoveho souboru (predchozi uz se cte/cetlo)
    CQuadWord WriteOffset;        // offset pro zapis dalsiho bloku do ciloveho souboru (predchozi uz se zapisuje/zapsal)
    int AutoRetryAttemptsSNAP;    // pocet opakovani automatickeho Retry (nedelame vic jak 3x): na SNAP serveru dochazi pri cteni souboru k nahodnemu vyskytu chyby ERROR_NETNAME_DELETED, tlacitko Retry pry funguje, "mackame" ho tedy automaticky

    // vybrane parametry DoCopyFileLoopAsync, at se to vsude nepredava v paremetrech volani
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

    // prerusi pripadne asynchronni operace
    void CancelOpPhase1();
    // zajisti, ze vsechny asynchronni operace skutecne dobehly + nastavi ukazovatko na konec souvisle
    // zapsane casti ciloveho souboru, aby se soubor spravne zarizl (pred pripadnym uzavrenim a vymazem)
    // POZOR: uvolni nepotrebne bloky, zustavaji jen ty s nactenymi daty IN souboru, ktere navic
    //        navazuji na WriteOffset (jsou pouzitelne pro Retry)
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
                                          0, &ioStatus, 0x00140390 /* IOCTL_LMR_DISABLE_LOCAL_BUFFERING */,
                                          NULL, 0, NULL, 0);
        if (status == STATUS_PENDING) // musime si pockat na dokonceni operace, probiha asynchronne
        {
            CALL_STACK_MESSAGE1("DisableLocalBuffering(): STATUS_PENDING");
            WaitForSingleObject(asyncPar->Overlapped[0].hEvent, INFINITE);
            status = ioStatus.Status;
        }
        if (status == 0 /* STATUS_SUCCESS */)
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
    { // nastala chyba cteni, jdeme ji poresit
        *err = GetLastError();
        if (*err == ERROR_HANDLE_EOF) // synchronne ohlaseny EOF, prevedeme ho na asynchronne hlaseny EOF
            AsyncPar->SetOverlappedToEOF(blkIndex, ReadOffset);
        else
            return FALSE;
    }
    // pokud cteni probehlo synchronne (nebo z cache, coz bohuzel nejsem schopen detekovat), tak ted
    // musime neco zapsat, jinak se muze zapis flakat = celkove zpomaleni operace
    BOOL opCompleted = HasOverlappedIoCompleted(AsyncPar->GetOverlapped(blkIndex));
    ForceOp = opCompleted ? fopWriting : fopNotUsed;

#ifdef ASYNC_COPY_DEBUG_MSG
    TRACE_I("ReadFile result: " << (opCompleted ? "DONE" : "ASYNC"));
#endif // ASYNC_COPY_DEBUG_MSG

    if (opCompleted && !Script->ChangeSpeedLimit)                   // pokud se muze zmenit speed-limit, tady neni "vhodne" misto pro cekani
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
    if (*DlgData->CancelWorker)
    {
        *err = ERROR_CANCELLED;
        return FALSE; // cancel se provede v error-handlingu
    }

    BlockOffset[blkIndex] = ReadOffset;
    BlockDataLen[blkIndex] = readSize;
    if (!testEOF) // blok byl pred volanim teto metody ve stavu cbsFree
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
    { // nastala chyba zapisu, jdeme ji poresit
        *err = GetLastError();
        return FALSE;
    }
    // pokud zapis probehl synchronne (nebo do cache, coz bohuzel nejsem schopen detekovat), tak ted
    // musime neco precist, jinak se muze cteni flakat = celkove zpomaleni operace
    BOOL opCompleted = HasOverlappedIoCompleted(AsyncPar->GetOverlapped(blkIndex));
    ForceOp = !ReadingDone && opCompleted ? fopReading : fopNotUsed;

#ifdef ASYNC_COPY_DEBUG_MSG
    TRACE_I("WriteFile result: " << (opCompleted ? "DONE" : "ASYNC"));
#endif // ASYNC_COPY_DEBUG_MSG

    if (opCompleted && !Script->ChangeSpeedLimit)                   // pokud se muze zmenit speed-limit, tady neni "vhodne" misto pro cekani
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
    if (*DlgData->CancelWorker)
    {
        *err = ERROR_CANCELLED;
        return FALSE; // cancel se provede v error-handlingu
    }

    WriteOffset.Value += BlockDataLen[blkIndex];
    BlockState[blkIndex] = cbsWriting; // blok byl pred volanim teto metody ve stavu cbsRead
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
    return -1; // dead code, jen pro kompilator
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
            if (st == cbsRead) // zahodime nactena data za koncem souboru, nemaji smysl
                FreeBlock(i);
            else
            {
                BlockState[i] = cbsDiscarded; // cteni za koncem souboru nema smysl; BlockTime neni duvod menit
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
        if (*fileSize < minFileSize) // pokud by nahodou GetFileSize vratila kratsi soubor nez uz mame nacteny
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
    // POZOR: errBlkIndex == -1 pro chybu pri zadani asynchronniho cteni (nema prideleny blok)
    //        nebo pro chybu pri zkracovani souboru po dobehnuti hl. kopirovaciho cyklu (nema prideleny blok)
    //        nebo pro Cancel v progress dialogu (nema prideleny blok)

    DWORD bytes;
    for (int i = 0; i < _countof(BlockState); i++)
    {
        if (BlockState[i] > cbsInProgress)
        { // GetOverlappedResult by mel hned vratit vysledek, protoze jsme pro oba soubory volali CancelIo()
            if (GetOverlappedResult(BlockState[i] == cbsWriting ? *Out : *In, AsyncPar->GetOverlapped(i), &bytes, TRUE))
            {
                if (BlockState[i] == cbsReading && BlockDataLen[i] == bytes) // komplet prectene -> zmena na cbsRead blok
                {
                    BlockState[i] = cbsRead;
                    ReadingBlocks--;
                }
                else
                {
                    if (BlockState[i] == cbsWriting && BlockDataLen[i] == bytes) // komplet zapsane -> zmena na cbsRead blok (mozna budeme zapisovat znovu, tak si ted blok nezahodime)
                    {
                        BlockState[i] = cbsRead;
                        WritingBlocks--;
                    }
                }
            }
            else
            {
                DWORD err = GetLastError();
                if (i != errBlkIndex &&             // pro tento blok hlasime chybu, znovu ji hlasit do TRACE nema smysl
                    err != ERROR_OPERATION_ABORTED) // tohle neni chyba, jen hlasi, ze to bylo preruseno (volanim CancelIo())
                {                                   // vypiseme si chyby v dalsich blocich, nejspis nic duleziteho a meli bysme je proste jen ignorovat
                    TRACE_I("CCopy_Context::CancelOpPhase2(): GetOverlappedResult(" << (BlockState[i] == cbsWriting ? "OUT" : "IN") << ", " << i << ") returned error: " << GetErrorText(err));
                }
            }
            switch (BlockState[i])
            {
            case cbsReading:    // neni kompletne prectene
            case cbsTestingEOF: // nedokonceny test EOFu
            case cbsDiscarded:
                FreeBlock(i);
                break;

            case cbsWriting:                      // nezapsany blok
                if (WriteOffset > BlockOffset[i]) // pripadne snizime WriteOffset
                    WriteOffset = BlockOffset[i];
                BlockState[i] = cbsRead; // neni komplet zapsane, ale prectene ano -> zmena na cbsRead blok (mozna budeme zapisovat znovu, tak si ted blok nezahodime)
                WritingBlocks--;
                break;
            }
        }
    }

    ReadOffset = WriteOffset; // zjistime, kam az mame souvisle nactena data od offsetu, kde potrebujeme zacit zapis
    for (int i = 0; i < _countof(BlockState); i++)
    {
        if (BlockState[i] == cbsRead && BlockOffset[i] == ReadOffset) // mame nacteny blok navazujici na ReadOffset
        {
            ReadOffset.Value += BlockDataLen[i];
            i = -1; // a hledame zase pekne od zacatku (pri poctu 8 bloku si to muzeme dovolit, max. 36 pruchodu cyklem)
        }
    }

    // zahodime bloky, ktere uz jsou zapsane nebo naopak jsou moc vpredu (nenavazuji),
    // takze je radsi nechame nacist znovu
    for (int i = 0; i < _countof(BlockState); i++)
        if (BlockState[i] == cbsRead && (BlockOffset[i] < WriteOffset || BlockOffset[i] > ReadOffset))
            FreeBlock(i);

    // pro pripad ruseni ciloveho souboru nastavime file pointer na konec zapsane casti, volajici
    // pak pres SetEndOfFile zarizne soubor pred jeho vymazem (jinak by mohlo dojit k nesmyslnemu
    // zapisu nul od konce zapsane casti az do konce souboru - soubor je predalokovany kvuli
    // prevenci fragmentace)
    if (*Out != NULL) // jen pokud mezitim nebyl cilovy soubor zavreny
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
        HANDLES(CloseHandle(*In)); // zavreme invalidni handle
    *In = HANDLES_Q(CreateFile(Op->SourceName, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, AsyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (*In != INVALID_HANDLE_VALUE) // otevreno, jeste nastavime offset
    {
        CQuadWord size;
        size.LoDWord = GetFileSize(*In, (DWORD*)&size.HiDWord);
        if ((size.LoDWord != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) && size >= ReadOffset)
        { // lze ziskat velikost a soubor je dost velky
            // je-li zdroj na siti: disable local client-side in-memory caching
            // http://msdn.microsoft.com/en-us/library/ee210753%28v=vs.85%29.aspx
            //
            // pouziti Overlapped[0].hEvent z AsyncPar je OK, ted neni nic "in-progress", event se nepouziva
            // (ovsem POZOR: napr. Buffers[0] z AsyncPar se muze pouzivat)
            if ((Op->OpFlags & OPFL_SRCPATH_IS_NET) && !DisableLocalBuffering(AsyncPar, *In, err))
                TRACE_E("CCopy_Context::RetryCopyReadErr(): IOCTL_LMR_DISABLE_LOCAL_BUFFERING failed for network source file: " << Op->SourceName << ", error: " << GetErrorText(*err));
            // pouziti Overlapped[0 a 1].hEvent a Overlapped[0 a 1] z AsyncPar je OK, ted neni nic
            // "in-progress", event ani overlapped struktura se nepouziva (ovsem POZOR: napr. Buffers[0]
            // z AsyncPar se muze pouzivat)
            if (CheckTailOfOutFile(AsyncPar, *In, *Out, WriteOffset, WriteOffset, TRUE))
            {
                ForceOp = ReadOffset > WriteOffset ? fopWriting : fopNotUsed; // pokud mame nacteno napred, zacneme zapisem
                *OperationDone = WriteOffset;
                Script->SetTFSandProgressSize(*LastTransferredFileSize + *OperationDone, *TotalDone + *OperationDone);
                SetProgressWithoutSuspend(HProgressDlg, CaclProg(*OperationDone, Op->Size),
                                          CaclProg(*TotalDone + *OperationDone, Script->TotalSize), *DlgData);
                return TRUE; // uspech: jdeme na retry
            }
        }
        // nelze ziskat velikost nebo je soubor moc maly nebo posledni zapsana cast souboru neodpovida zdroji, jedeme to cely znovu
        HANDLES(CloseHandle(*In));
        if (WholeFileAllocated)
            SetEndOfFile(*Out); // u floppy by se jinak zapisoval zbytek souboru
        HANDLES(CloseHandle(*Out));
        DeleteFile(Op->TargetName);
        *copyAgain = TRUE; // goto COPY_AGAIN;
        return FALSE;
    }
    else // nejde otevrit, problem trva ...
    {
        *err = GetLastError();
        *In = NULL;
        *errAgain = TRUE; // goto READ_ERROR;
        return FALSE;
    }
}

BOOL CCopy_Context::HandleReadingErr(int blkIndex, DWORD err, BOOL* copyError, BOOL* skipCopy, BOOL* copyAgain)
{
    // POZOR: blkIndex == -1 pro chybu pri zadani asynchronniho cteni (nema prideleny blok)

    CancelOpPhase1();

    while (1)
    {
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
        { // na SNAP serveru dochazi pri cteni souboru k nahodnemu vyskytu chyby ERROR_NETNAME_DELETED, tlacitko Retry pry funguje, zkusime ho tedy automaticky
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
                    break;    // stejny problem znovu, opakujeme hlaseni
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
            continue; // IDRETRY: stejny problem znovu, opakujeme hlaseni
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
            SetEndOfFile(*Out);     // u floppy by se jinak zapisoval zbytek souboru
        HANDLES(CloseHandle(*Out)); // zavreme invalidni handle
    }
    *Out = HANDLES_Q(CreateFile(Op->TargetName, GENERIC_WRITE | GENERIC_READ, 0, NULL,
                                OPEN_ALWAYS, AsyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (*Out != INVALID_HANDLE_VALUE) // otevreno, jeste nastavime offset
    {
        BOOL ok = TRUE;
        CQuadWord size;
        size.LoDWord = GetFileSize(*Out, (DWORD*)&size.HiDWord);
        if (size.LoDWord == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ||   // nelze ziskat velikost
            size < WriteOffset ||                                                // soubor je prilis maly
            WholeFileAllocated && size > allocFileSize && size > maxWriteOffset) // predalokovany soubor je prilis velky (vetsi nez predalokovana velikost a zaroven vetsi nez zapsana velikost vcetne posledniho zapisovaneho bloku) = doslo k pridani zapisovanych bytu na konec souboru (allocWholeFileOnStart by melo byt 0 /* need-test */)
        {                                                                        // jedeme to cely znovu
            ok = FALSE;
        }
        // uspech (soubor je tak akorat veliky)
        // je-li cil na siti: disable local client-side in-memory caching
        // http://msdn.microsoft.com/en-us/library/ee210753%28v=vs.85%29.aspx
        //
        // pouziti Overlapped[0].hEvent z AsyncPar je OK, ted neni nic "in-progress", event se nepouziva
        // (ovsem POZOR: napr. Buffers[0] z AsyncPar se muze pouzivat)
        if (ok && (Op->OpFlags & OPFL_TGTPATH_IS_NET) && !DisableLocalBuffering(AsyncPar, *Out, err))
            TRACE_E("CCopy_Context::RetryCopyWriteErr(): IOCTL_LMR_DISABLE_LOCAL_BUFFERING failed for network target file: " << Op->TargetName << ", error: " << GetErrorText(*err));
        // pouziti Overlapped[0 a 1].hEvent a Overlapped[0 a 1] z AsyncPar je OK, ted neni nic
        // "in-progress", event ani overlapped struktura se nepouziva (ovsem POZOR: napr. Buffers[0]
        // z AsyncPar se muze pouzivat)
        if (!ok || !CheckTailOfOutFile(AsyncPar, *In, *Out, WriteOffset, WriteOffset, FALSE))
        {
            HANDLES(CloseHandle(*In));
            HANDLES(CloseHandle(*Out));
            DeleteFile(Op->TargetName);
            *copyAgain = TRUE; // goto COPY_AGAIN;
            return FALSE;
        }
        ForceOp = ReadOffset > WriteOffset ? fopWriting : fopNotUsed; // pokud mame nacteno napred, zacneme zapisem
        *OperationDone = WriteOffset;
        Script->SetTFSandProgressSize(*LastTransferredFileSize + *OperationDone, *TotalDone + *OperationDone);
        SetProgressWithoutSuspend(HProgressDlg, CaclProg(*OperationDone, Op->Size),
                                  CaclProg(*TotalDone + *OperationDone, Script->TotalSize), *DlgData);
        return TRUE; // uspech: jdeme na retry
    }
    else // nejde otevrit, problem trva ...
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
    // POZOR: blkIndex == -1 pro chybu pri zkracovani souboru po dobehnuti hl. kopirovaciho cyklu (nema prideleny blok)

    CancelOpPhase1();

    while (1)
    {
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                    break;    // stejny problem znovu, opakujeme hlaseni
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
            continue; // IDRETRY: stejny problem znovu, opakujeme hlaseni
        TRACE_C("CCopy_Context::HandleWritingErr(): unexpected result of WM_USER_DIALOG(0).");
        return TRUE;
    }
}

BOOL CCopy_Context::HandleSuspModeAndCancel(BOOL* copyError)
{
    if (!Script->ChangeSpeedLimit)                                  // pokud se nemuze zmenit speed-limit (jinak tady neni "vhodne" misto pro cekani)
        WaitForSingleObject(DlgData->WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
    DWORD bytes = 0; // pomocny DWORD - kolik bajtu se precetlo/zapsalo v bloku

    // je-li zdroj/cil na siti: disable local client-side in-memory caching
    // http://msdn.microsoft.com/en-us/library/ee210753%28v=vs.85%29.aspx
    if ((op->OpFlags & OPFL_SRCPATH_IS_NET) && !DisableLocalBuffering(asyncPar, in, &err))
        TRACE_E("DoCopyFileLoopAsync(): IOCTL_LMR_DISABLE_LOCAL_BUFFERING failed for network source file: " << op->SourceName << ", error: " << GetErrorText(err));
    if ((op->OpFlags & OPFL_TGTPATH_IS_NET) && !DisableLocalBuffering(asyncPar, out, &err))
        TRACE_E("DoCopyFileLoopAsync(): IOCTL_LMR_DISABLE_LOCAL_BUFFERING failed for network target file: " << op->TargetName << ", error: " << GetErrorText(err));

    // parametry kopirovaci smycky
    int numOfBlocks = 8;

    // kontext Copy operace (zabranuje predavani hromady parametru do pomocnych funkci, nyni metod kontextu)
    CCopy_Context ctx(asyncPar, numOfBlocks, &dlgData, op, hProgressDlg, &in, &out, wholeFileAllocated, script,
                      &operationDone, &totalDone, &lastTransferredFileSize);
    BOOL doCopy = TRUE;
    while (doCopy)
    {
        if (ctx.ForceOp != fopWriting && ctx.FreeBlocks > 0 && !ctx.ReadingDone && ctx.ReadingBlocks < (numOfBlocks + 1) / 2) // cist soubezne budeme maximalne do poloviny bloku
        {
            DWORD toRead = ctx.ReadOffset + CQuadWord(limitBufferSize, 0) <= fileSize ? limitBufferSize : (fileSize - ctx.ReadOffset).LoDWord;
            BOOL testEOF = toRead == 0;
            if (!testEOF || ctx.ReadingBlocks == 0) // cteni dat nebo test EOFu (test EOFu se dela, jen kdyz jsou vsechna cteni dokoncena)
            {
                if (ctx.BlockState[ctx.FreeBlockIndex] != cbsFree)
                    ctx.FreeBlockIndex = ctx.FindBlock(cbsFree);
                // test EOFu = cteni do celeho bloku, jinak bezne cteni 'toRead'
                if (ctx.StartReading(ctx.FreeBlockIndex, testEOF ? limitBufferSize : toRead, &err, testEOF))
                    continue; // uspech (start asynchronniho cteni), zkusime nastartovat dalsi cteni
                else
                { // chyba (start asynchronniho cteni)
                    if (!ctx.HandleReadingErr(-1, err, &copyError, &skipCopy, &copyAgain))
                        return; // cancel/skip(skip-all)/retry-complete
                    continue;   // retry-resume
                }
            }
        }
        // cteni uz je zadane nebo neni potreba, jdeme zjistit, jestli se neco nedokoncilo
        BOOL shouldWait = TRUE; // TRUE = uz neni co dalsiho asynchronniho zadat, musime pockat na dokonceni nejake zadane operace
        BOOL retryCopy = FALSE; // TRUE = po chybe se ma provest Retry = zacit pekne od zacatku cyklu "doCopy"
        // dve kola jsou potreba jen v pripade synchronniho zapisu (chceme ho oznacit za
        // dokonceny hned a ne az po dalsim cteni, uz kvuli progresu)
        for (int afterWriting = 0; afterWriting < 2; afterWriting++)
        {
            for (int i = 0; i < _countof(ctx.BlockState); i++)
            {
                if (ctx.BlockState[i] > cbsInProgress && HasOverlappedIoCompleted(asyncPar->GetOverlapped(i)))
                {
                    shouldWait = FALSE; // v duchu "keep it simple" (jsou situace, kdy by to mohlo zustat TRUE, ale ignorujeme je)
                    switch (ctx.BlockState[i])
                    {
                    case cbsReading:    // cteni zdrojoveho souboru do bloku - zadane (probiha)
                    case cbsTestingEOF: // testovani konce zdrojoveho souboru
                    {
                        BOOL testingEOF = ctx.BlockState[i] == cbsTestingEOF;

#ifdef ASYNC_COPY_DEBUG_MSG
                        TRACE_I("READ done: " << i);
#endif // ASYNC_COPY_DEBUG_MSG

                        BOOL res = GetOverlappedResult(in, asyncPar->GetOverlapped(i), &bytes, TRUE);
                        if (testingEOF && res && bytes == 0)
                        {
                            res = FALSE; // podle MSDN ma na EOFu vracet FALSE a ERROR_HANDLE_EOF, tak si to posychrujeme (na disku Novell Netware 6.5 vraci TRUE)
                            SetLastError(ERROR_HANDLE_EOF);
                        }
                        if (res || GetLastError() == ERROR_HANDLE_EOF)
                        {
                            ctx.AutoRetryAttemptsSNAP = 0;
                            if (!res) // EOF na zacatku bloku (jen cbsReading: EOF muze byt i pred timto blokem, to se pripadne vyresi nalezenim EOFu pozdeji v bloku s nizsim offsetem)
                            {
                                // kdyz GetOverlappedResult() vraci FALSE, nemusi vratit bytes==0 (byl tu na to TRACE_C
                                // a padacky prisly), takze bytes vynulujeme explicitne
                                bytes = 0;
                                if (testingEOF)
                                    ctx.ReadingDone = TRUE; // potvrzeny konec zdrojoveho souboru, dale uz nic necteme
                                // nesmime forcovat fopWriting (nic jsme neprecetli, nemame co zapsat), pokud nejde o test EOFu,
                                // nechame dokoncit ostatni asynchronni cteni, a pak provedeme test EOFu, a pak uz se bude jen zapisovat
                                ctx.ForceOp = fopNotUsed;
                            }
                            if (bytes < ctx.BlockDataLen[i]) // soubor je kratsi nez jsme cekali -> nastavime novou velikost souboru
                            {
                                if (!testingEOF || bytes != 0)
                                    ctx.ReadOffset = fileSize = ctx.BlockOffset[i] + CQuadWord(bytes, 0);
                                if (!testingEOF)
                                    ctx.DiscardBlocksBehindEOF(fileSize, i);
                                if (bytes == 0) // EOF = zadna data, uvolnime blok
                                {
                                    ctx.FreeBlock(i);
                                    if (testingEOF)
                                        doCopy = !ctx.IsOperationDone(numOfBlocks); // otestujeme, jestli jsme timto nedokoncili kopirovani
                                }
                                else
                                    ctx.BlockDataLen[i] = bytes; // dale si hrajeme na to, ze jsme chteli nacist presne tolik
                            }
                            else
                            {
                                if (testingEOF) // hledali jsme EOF a nacetl se plny blok, asi doslo k razantnimu narustu souboru, zjistime novou velikost
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
                        else // chyba
                        {
                            if (!ctx.HandleReadingErr(i, GetLastError(), &copyError, &skipCopy, &copyAgain))
                                return;       // cancel/skip(skip-all)/retry-complete
                            retryCopy = TRUE; // retry-resume
                        }
                        break;
                    }

                    case cbsWriting: // zapis bloku do ciloveho souboru
                    {
#ifdef ASYNC_COPY_DEBUG_MSG
                        TRACE_I("WRITE done: " << i);
#endif // ASYNC_COPY_DEBUG_MSG

                        BOOL res = GetOverlappedResult(out, asyncPar->GetOverlapped(i), &bytes, TRUE);
                        if (!res || bytes != ctx.BlockDataLen[i]) // chyba
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

                        if (!script->ChangeSpeedLimit)                                 // pokud se muze zmenit speed-limit, tady neni "vhodne" misto pro cekani
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                        operationDone += CQuadWord(bytes, 0);
                        SetProgressWithoutSuspend(hProgressDlg, CaclProg(operationDone, op->Size),
                                                  CaclProg(totalDone + operationDone, script->TotalSize), dlgData);

                        if (script->ChangeSpeedLimit)                                  // asi se bude menit speed-limit, zde je "vhodne" misto na cekani, az se
                        {                                                              // worker zase rozbehne, ziskame znovu velikost bufferu pro kopirovani
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                            script->GetNewBufSize(&limitBufferSize, bufferSize);
                        }

                        // break; // tady break nechybi...
                    }
                    case cbsDiscarded: // cteni zdrojoveho souboru za koncem souboru (melo by vratit jen error: EOF)
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

            // nacetli jsme data do bloku, zjistime, jestli by nesly zapsat do ciloveho souboru;
            // uvolnili jsme zapsane/discardnute bloky (zase do nich pujdeme cist na zacatek smycky)
            CQuadWord nextReadBlkOffset; // nejnizsi offset preskoceneho cbsRead bloku
            do
            {
                nextReadBlkOffset.SetUI64(0);
                // zapisovat soubezne budeme maximalne do poloviny bloku
                for (int i = 0; ctx.ForceOp != fopReading && i < _countof(ctx.BlockState) && ctx.WritingBlocks < (numOfBlocks + 1) / 2; i++)
                {
                    if (ctx.BlockState[i] == cbsRead)
                    {
                        if (ctx.WriteOffset == ctx.BlockOffset[i])
                        {
                            if (!ctx.StartWriting(i, &err))
                            { // chyba (asynchronniho zapisu)
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
                } // mame dalsi cbsRead blok a navazuje na zapsanou cast ciloveho souboru -> zapisujeme dale
            } while (!retryCopy && ctx.ForceOp != fopReading && nextReadBlkOffset.Value != 0 && nextReadBlkOffset == ctx.WriteOffset &&
                     ctx.WritingBlocks < (numOfBlocks + 1) / 2); // zapisovat soubezne budeme maximalne do poloviny bloku
            if (retryCopy || ctx.ForceOp != fopReading)
                break; // jdeme na Retry nebo zapis nebyl synchronni (tedy byl hotov za cca 0 ms) nebo uz jen zapisujeme, kazdopadne dve kola jsou zbytecna
        }
        if (!doCopy || retryCopy)
            continue;

        if (shouldWait) // dalsi pruchod cyklem nema smysl, neni nadeje na zahajeni noveho cteni ani zapisu, pockame
        {               // na dokonceni nejstarsi asynchronni operace
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

            // zde pockame na dokonceni nejstarsi zadane probihajici asynchronni operace
            // zdrojoveho souboru ('in') se tyka: cbsReading, cbsTestingEOF a cbsDiscarded
            // ciloveho souboru ('out') se tyka jen cbsWriting
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

    if (wholeFileAllocated) // alokovali jsme kompletni podobu souboru (znamena, ze alokace mela smysl, napr. soubor nemuze byt nulovy)
    {
        if (operationDone < allocFileSize) // a zdrojovy soubor se zmensil, musime ho zde zkratit
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
                        err2 = ERROR_INVALID_FUNCTION; // uspesny SetFilePointer, ale off != ctx.WriteOffset: nejspis nikdy nenastane, jen pro uplnost
                    if (!ctx.HandleWritingErr(-1, err2, &copyError, &skipCopy, &copyAgain, allocFileSize, CQuadWord(0, 0)))
                        return; // cancel/skip(skip-all)/retry-complete
                                // retry-resume
                }
                else
                    break; // uspech
            }
        }

        if (allocWholeFileOnStart == 0 /* need-test */)
        {
            CQuadWord curFileSize;
            curFileSize.LoDWord = GetFileSize(out, &curFileSize.HiDWord);
            BOOL getFileSizeSuccess = (curFileSize.LoDWord != INVALID_FILE_SIZE || GetLastError() == NO_ERROR);
            if (getFileSizeSuccess && curFileSize == operationDone)
            { // kontrola, jestli nedoslo k pridani zapisovanych bytu na konec souboru + jestli umime soubor zkratit
                allocWholeFileOnStart = 1 /* yes */;
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
                allocWholeFileOnStart = 2 /* no */; // dalsi pokusy na tomhle cilovem disku si odpustime

                while (1)
                {
                    HANDLES(CloseHandle(out));
                    out = NULL;
                    ClearReadOnlyAttr(op->TargetName); // kdyby vznikl jako read-only (nemelo by nikdy nastat), tak abychom si s nim poradili
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

    // pokud cesta konci mezerou/teckou, je invalidni a nesmime provest kopii,
    // CreateFile mezery/tecky orizne a prekopiroval by se tak jiny soubor pripadne do jineho jmena
    BOOL invalidSrcName = FileNameIsInvalid(op->SourceName, TRUE);
    BOOL invalidTgtName = FileNameIsInvalid(op->TargetName, TRUE);

    // optimalizace: zhruba 4x zrychli skipnuti vsech "starsich a stejnych" souboru,
    // zpomaleni pokud je soubor novejsi je 5%, tedy myslim, ze se to silne vyplati
    // (da se jiste predpokladat, ze "Overwrite Older" user zapne pokud dojde k tem skipum)
    BOOL tgtNameCaseCorrected = FALSE; // TRUE = velikost pismen v cilovem jmene uz byla upravena podle existujiciho ciloveho souboru (aby pri prepisu nedoslo ke zmene velikosti pismen)
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
            if (StrICmp(tgtName, dataOut.cFileName) == 0 &&                 // pokud nejde jen o shodu DOS-name (tam dojde ke zmene DOS-name a ne k prepisu)
                (dataOut.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) // pokud nejde o adresar (s tim overwrite-older nic nezmuze)
            {
                find = HANDLES_Q(FindFirstFile(op->SourceName, &dataIn));
                if (find != INVALID_HANDLE_VALUE)
                {
                    HANDLES(FindClose(find));

                    // orizneme casy na sekundy (ruzne FS ukladaji casy s ruznymi prestnostmi, takze dochazelo k "rozdilum" i mezi "shodnymi" casy)
                    *(unsigned __int64*)&dataIn.ftLastWriteTime = *(unsigned __int64*)&dataIn.ftLastWriteTime - (*(unsigned __int64*)&dataIn.ftLastWriteTime % 10000000);
                    *(unsigned __int64*)&dataOut.ftLastWriteTime = *(unsigned __int64*)&dataOut.ftLastWriteTime - (*(unsigned __int64*)&dataOut.ftLastWriteTime % 10000000);

                    if ((dataIn.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&             // zkontrolujeme, ze zdroj je stale jeste soubor
                        CompareFileTime(&dataIn.ftLastWriteTime, &dataOut.ftLastWriteTime) <= 0) // zdrojovy soubor neni novejsi nez cilovy soubor - skipneme copy operaci
                    {
                        CQuadWord fileSize(op->FileSize);
                        if (fileSize < COPY_MIN_FILE_SIZE)
                        {
                            if (op->Size > COPY_MIN_FILE_SIZE)             // melo by byt vzdycky aspon COPY_MIN_FILE_SIZE, ale sychrujeme se...
                                fileSize += op->Size - COPY_MIN_FILE_SIZE; // pricteme velikost ADSek
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

    // rozhodneme jakym algoritmem budeme kopirovat: konvencnim prastarym synchronim nebo
    // asynchronim okoukanym od Windows 7 verze CopyFileEx:
    // -pod Vistou se to zle sralo, kasleme na ni, uz stejne temer vymrela, navic
    //  pri pouziti stareho algoritmu proti Win7 po siti jsem nezjistil zadny rychlostni
    //  rozdil pro upload, u downloadu jsme byli o 15% pomalejsi (coz je unosne)
    // -asynchroni algous ma smysl jen po siti + pokud mame rychly nebo sitovy zdroj/cil
    // -se starym algousem je kopirovani ve Win7 po siti klidne 2x-3x pomalejsi ve smeru
    //  download, skoro 2x pomalejsi upload a tak 30% pomalejsi sit-sit
    BOOL useAsyncAlg = Windows7AndLater && Configuration.UseAsyncCopyAlg &&
                       op->FileSize.Value > 0 && // prazdne soubory jedeme synchrone (zadna data)
                       ((op->OpFlags & OPFL_SRCPATH_IS_NET) && ((op->OpFlags & OPFL_TGTPATH_IS_NET) ||
                                                                (op->OpFlags & OPFL_TGTPATH_IS_FAST)) ||
                        (op->OpFlags & OPFL_TGTPATH_IS_NET) && (op->OpFlags & OPFL_SRCPATH_IS_FAST));

    if (asyncPar == NULL)
        asyncPar = new CAsyncCopyParams;

    asyncPar->Init(useAsyncAlg);
    script->EnableProgressBufferLimit(useAsyncAlg);
    struct CDisableProgressBufferLimit // zajistime volani Script->EnableProgressBufferLimit(FALSE) na vsech exitech z teto funkce
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
                    // GENERIC_READ pro 'out' zpusobi zpomaleni u asynchronniho kopirovani z disku na sit (na Win7 x64 GLAN namereno 95MB/s misto 111MB/s)
                    out = SalCreateFileEx(op->TargetName, GENERIC_WRITE | (script->CopyAttrs ? GENERIC_READ : 0), 0, fileAttrs, &encryptionNotSupported);
                    if (!encryptionNotSupported && script->CopyAttrs && out == INVALID_HANDLE_VALUE) // pro pripad, ze neni povolen read-access do adresare (ten jsme pridali jen kvuli nastavovani Compressed atributu) zkusime jeste vytvorit soubor jen pro zapis
                        out = SalCreateFileEx(op->TargetName, GENERIC_WRITE, 0, fileAttrs, &encryptionNotSupported);

                    if (out == INVALID_HANDLE_VALUE && encryptionNotSupported && dlgData.FileOutLossEncrAll && !lossEncryptionAttr)
                    { // uzivatel souhlasil se ztratou Encrypted atributu pro vsechny problemove soubory, tak tuhle ztratu zaridime
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
                    { // zkontrolujeme jestli je Encrypted skutecne nastaveny (treba na FATce se proste ignoruje, system chyby nevraci (konkretne pro CreateFile))
                        DWORD attrs;
                        attrs = SalGetFileAttributes(op->TargetName);
                        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_ENCRYPTED) == 0)
                        { // nejde nahodit Encrypted atribut, zeptame se usera co s tim...
                            if (dlgData.FileOutLossEncrAll)
                                lossEncryptionAttr = TRUE;
                            else
                            {
                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                    dlgData.FileOutLossEncrAll = TRUE; // tady break; nechybi
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

                    // pokud je to mozne, provedeme alokaci potrebneho mista pro soubor (nedochazi pak k fragmentaci disku + hladsi zapis na diskety)
                    BOOL wholeFileAllocated = FALSE;
                    if (!skipAllocWholeFileOnStart &&               // minule doslo k chybe, ted by nejspis doslo k te same
                        allocWholeFileOnStart != 2 /* no */ &&      // alokovani celeho souboru neni zakazano
                        fileSize > CQuadWord(limitBufferSize, 0) && // pod velikost kopirovaciho bufferu nema alokace souboru smysl
                        fileSize < CQuadWord(0, 0x80000000))        // velikost souboru je kladne cislo (jinak nelze seekovat - jde o cisla nad 8EB, takze zrejme nikdy nenastane)
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
                                    ignoreErr = TRUE; // malo mista na disku
                            }
                        }
                        if (fatal)
                        {
                            if (!ignoreErr)
                            {
                                DWORD err = GetLastError();
                                TRACE_E("DoCopyFile(): unable to allocate whole file size before copy operation, please report under what conditions this occurs! GetLastError(): " << GetErrorText(err));
                                allocWholeFileOnStart = 2 /* no */; // dalsi pokusy na tomhle cilovem disku si odpustime
                            }

                            // zkusime jeste soubor zkratit na nulu, aby nedoslo pri zavreni souboru k nejakemu zbytecnemu zapisu
                            SetFilePointer(out, 0, NULL, FILE_BEGIN);
                            SetEndOfFile(out);

                            HANDLES(CloseHandle(out));
                            out = INVALID_HANDLE_VALUE;
                            ClearReadOnlyAttr(op->TargetName); // kdyby vznikl jako read-only (nemelo by nikdy nastat), tak abychom si s nim poradili
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
                        // POZOR: 'in' ani 'out' nemaji nastaveny file-pointer (SetFilePointer) na konec souboru,
                        //        respektive 'out' ho ma nastaveny jen pri (copyError || skipCopy)
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
                                SetEndOfFile(out); // u floppy by se jinak zapisoval zbytek souboru
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
                                SetEndOfFile(out); // u floppy by se jinak zapisoval zbytek souboru
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
                        {                                                              // Lantastic 7.0, zdanlive vse bez problemu, ale vysledek spatny
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                SetFilePointer(in, 0, NULL, FILE_BEGIN);  // cteme znovu
                                SetFilePointer(out, 0, NULL, FILE_BEGIN); // zapisujeme znovu
                                SetEndOfFile(out);                        // zariznem vystupni soubor
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

                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                            dlgData.IgnoreAllGetFileTimeErr = TRUE; // tady break; nechybi
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

                    if (operationDone < COPY_MIN_FILE_SIZE) // nulove/male soubory trvaji aspon jako soubory s velikosti COPY_MIN_FILE_SIZE
                        script->AddBytesToSpeedMetersAndTFSandPS((DWORD)(COPY_MIN_FILE_SIZE - operationDone).Value, TRUE, 0, NULL, MAX_OP_FILESIZE);

                    DWORD attr = op->Attr & clearReadonlyMask;
                    if (copyADS) // je-li treba kopirovat ADS, udelame to
                    {
                        SetFileAttributes(op->TargetName, FILE_ATTRIBUTE_ARCHIVE); // nejspis zbytecne, proti samotnemu kopirovani moc nebrzdi, duvod: aby se se souborem dalo pracovat, nesmi mit read-only atribut
                        CQuadWord operDone = operationDone;                        // uz je zkopirovany soubor
                        if (operDone < COPY_MIN_FILE_SIZE)
                            operDone = COPY_MIN_FILE_SIZE; // nulove/male soubory trvaji aspon jako soubory s velikosti COPY_MIN_FILE_SIZE
                        BOOL adsSkip = FALSE;
                        if (!DoCopyADS(hProgressDlg, op->SourceName, FALSE, op->TargetName, totalDone,
                                       operDone, op->Size, dlgData, script, &adsSkip, buffer) ||
                            adsSkip) // user dal cancel nebo Skip aspon jedno ADS
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
                                return FALSE; // cancel cele operace
                            if (skip != NULL)
                                *skip = TRUE; // jde o Skip, musime hlasit vyse (Move operace nesmi smazat zdrojovy soubor)
                        }
                    }

                    if (out != NULL)
                    {
                        if (!ignoreGetFileTimeErr) // jen pokud jsme neignorovali chybu cteni casu souboru (neni co nastavovat)
                        {
                            BOOL ignoreSetFileTimeErr = FALSE;
                            while (!ignoreSetFileTimeErr &&
                                   !SetFileTime(out, NULL /*&creation*/, NULL /*&lastAccess*/, &lastWrite))
                            {
                                DWORD err = GetLastError();

                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                    dlgData.IgnoreAllSetFileTimeErr = TRUE; // tady break; nechybi
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
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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

                    if (script->CopyAttrs) // zkontrolujeme jestli se podarilo zachovat atributy zdrojoveho souboru
                    {
                        DWORD curAttrs;
                        curAttrs = SalGetFileAttributes(op->TargetName);
                        if (curAttrs == INVALID_FILE_ATTRIBUTES || (curAttrs & DISPLAYED_ATTRIBUTES) != (attr & DISPLAYED_ATTRIBUTES))
                        {                                                              // atributy se nejspis nepodarilo prenest, varujeme usera
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                dlgData.IgnoreAllSetAttrsErr = TRUE; // tady break; nechybi
                            case IDB_IGNORE:
                                break;

                            case IDCANCEL:
                            {
                            COPY_ERROR_2:

                                ClearReadOnlyAttr(op->TargetName); // aby se dal soubor smazat, nesmi mit read-only atribut
                                DeleteFile(op->TargetName);
                                return FALSE;
                            }
                            }
                        }
                    }

                    if (script->CopySecurity) // mame kopirovat NTFS security permissions?
                    {
                        DWORD err;
                        if (!DoCopySecurity(op->SourceName, op->TargetName, &err, NULL))
                        {
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                dlgData.IgnoreAllCopyPermErr = TRUE; // tady break; nechybi
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
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                            dlgData.FileOutLossEncrAll = TRUE; // tady break; nechybi
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
                        if (err == ERROR_FILE_EXISTS || // prepsat soubor ?
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

                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                                if (*dlgData.CancelWorker)
                                    goto CANCEL_OPEN;

                                if (dlgData.SkipAllOverwrite)
                                    goto SKIP_OPEN;

                                int ret;
                                ret = IDCANCEL;

                                if (!getTimeFailed && script->OverwriteOlder) // option z Copy/Move dialogu
                                {
                                    // orizneme casy na sekundy (ruzne FS ukladaji casy s ruznymi prestnostmi, takze dochazelo k "rozdilum" i mezi "shodnymi" casy)
                                    *(unsigned __int64*)&sFileTime = *(unsigned __int64*)&sFileTime - (*(unsigned __int64*)&sFileTime % 10000000);
                                    *(unsigned __int64*)&tFileTime = *(unsigned __int64*)&tFileTime - (*(unsigned __int64*)&tFileTime % 10000000);

                                    if (CompareFileTime(&sFileTime, &tFileTime) > 0)
                                        ret = IDYES; // starsi mame bez ptani prepsat
                                    else
                                        ret = IDB_SKIP; // ostatni existujici skipnout
                                }
                                else
                                {
                                    // zobrazime dotaz
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
                                default: // pro sychr (aby to odtud nemohlo odejit se zavrenym handlem in)
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
                                if (!dlgData.OverwriteHiddenAll && dlgData.CnfrmSHFileOver) // zde script->OverwriteOlder ignorujeme, user chce videt, ze jde o SYSTEM or HIDDEN soubor i pri zaplem optionu
                                {
                                    HANDLES(CloseHandle(in));
                                    in = NULL;

                                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                    default: // pro sychr (aby to odtud nemohlo odejit se zavrenym handlem in)
                                    {
                                        in = HANDLES_Q(CreateFile(op->SourceName, GENERIC_READ,
                                                                  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                                  OPEN_EXISTING, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                                        if (in == INVALID_HANDLE_VALUE)
                                            goto OPEN_IN_ERROR;
                                        attr = SalGetFileAttributes(op->TargetName); // kdyby user atributy zmenil, tak at mame aktualni hodnoty
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
                                if (targetCannotOpenForWrite || mustDeleteFileBeforeOverwrite == 1 /* yes */)
                                { // je nutne soubor nejdrive smazat
                                    BOOL chAttr = ClearReadOnlyAttr(op->TargetName, attr);

                                    if (!tgtNameCaseCorrected)
                                    {
                                        CorrectCaseOfTgtName(op->TargetName, FALSE, &dataOut);
                                        tgtNameCaseCorrected = TRUE;
                                    }

                                    if (DeleteFile(op->TargetName))
                                        goto OPEN_TGT_FILE; // je-li read-only (shozeni atributu nemuselo projit), pujde smazat jedine na Sambe s povolenym "delete readonly"
                                    else                    // nelze ani smazat, koncime s chybou...
                                    {
                                        err = GetLastError();
                                        if (chAttr)
                                            SetFileAttributes(op->TargetName, attr);
                                        errDeletingFile = TRUE;
                                        goto NORMAL_ERROR;
                                    }
                                }
                                else // jdeme soubor prepsat na miste
                                {
                                    // pokud jsme jeste nedelali test funkcnosti zkraceni souboru na nulu, ziskame soucasnou velikost souboru
                                    CQuadWord origFileSize(0, 0); // velikost souboru pred zkracenim
                                    if (mustDeleteFileBeforeOverwrite == 0 /* need test */)
                                    {
                                        out = HANDLES_Q(CreateFile(op->TargetName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                                   OPEN_EXISTING, 0, NULL));
                                        if (out != INVALID_HANDLE_VALUE)
                                        {
                                            origFileSize.LoDWord = GetFileSize(out, &origFileSize.HiDWord);
                                            if (origFileSize.LoDWord == INVALID_FILE_SIZE && GetLastError() == NO_ERROR)
                                                origFileSize.Set(0, 0); // chyba => dame velikost na nulu, testne se to na dalsim souboru
                                            HANDLES(CloseHandle(out));
                                        }
                                    }

                                    // otevreme soubor s vymazem ADS a zkracenim na nulu
                                    BOOL chAttr = FALSE;
                                    if (attr != INVALID_FILE_ATTRIBUTES &&
                                        (attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
                                    { // CREATE_ALWAYS se nesnasi s readonly, hidden a system atributama, takze je pripadne shodime
                                        chAttr = TRUE;
                                        SetFileAttributes(op->TargetName, 0);
                                    }
                                    // GENERIC_READ pro 'out' zpusobi zpomaleni u asynchronniho kopirovani z disku na sit (na Win7 x64 GLAN namereno 95MB/s misto 111MB/s)
                                    DWORD access = GENERIC_WRITE | (script->CopyAttrs ? GENERIC_READ : 0);
                                    fileAttrs = asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN |
                                                (!lossEncryptionAttr && copyAsEncrypted ? FILE_ATTRIBUTE_ENCRYPTED : 0) | // nastaveni atributu pri CREATE_ALWAYS funguje od XPcek a pokud ma soubor prava read-denied, je to jedina moznost, jak nahodit Encrypted atribut
                                                (script->CopyAttrs ? (op->Attr & (FILE_ATTRIBUTE_COMPRESSED | (lossEncryptionAttr ? 0 : FILE_ATTRIBUTE_ENCRYPTED))) : 0);
                                    out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, CREATE_ALWAYS, fileAttrs, NULL));
                                    if (out == INVALID_HANDLE_VALUE && fileAttrs != (asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN)) // pokud soubor nejde vytvorit s Encrypted, zkusime to jeste bez (hrozi na NTFS sitovem disku (ladeno na sharu z XPcek), na ktery jsme zalogovany pod jinym jmenem nez mame v systemu (na soucasny konzoli) - trik je v tom, ze na cilovem systemu je user se stejnym jmenem bez hesla, tedy sitove nepouzitelny)
                                        out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, CREATE_ALWAYS, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                                    if (script->CopyAttrs && out == INVALID_HANDLE_VALUE)
                                    { // pro pripad, ze neni povolen read-access do adresare (ten jsme pridali jen kvuli nastavovani Compressed atributu) zkusime jeste otevrit soubor jen pro zapis
                                        access = GENERIC_WRITE;
                                        out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, CREATE_ALWAYS, fileAttrs, NULL));
                                        if (out == INVALID_HANDLE_VALUE && fileAttrs != (asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN)) // pokud soubor nejde vytvorit s Encrypted, zkusime to jeste bez (hrozi na NTFS sitovem disku (ladeno na sharu z XPcek), na ktery jsme zalogovany pod jinym jmenem nez mame v systemu (na soucasny konzoli) - trik je v tom, ze na cilovem systemu je user se stejnym jmenem bez hesla, tedy sitove nepouzitelny)
                                            out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, CREATE_ALWAYS, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                                    }
                                    if (out == INVALID_HANDLE_VALUE) // pro zapis nelze cilovy soubor otevrit, zkusime ho tedy smazat a vytvorit znovu
                                    {
                                        // resi situaci, kdy je potreba prepsat soubor na Sambe:
                                        // soubor ma 440+jinyho_vlastnika a je v adresari, kam ma akt. user zapis
                                        // (smazat lze, ale primo prepsat ne (nelze otevrit pro zapis) - obchazime:
                                        //  smazeme+vytvorime soubor znovu)
                                        // (na Sambe lze povolit mazani read-only, coz umozni delete read-only souboru,
                                        //  jinak nelze smazat, protoze Windows neumi smazat read-only soubor a zaroven
                                        //  u toho souboru nelze shodit "read-only" atribut, protoze akt. user neni vlastnik)
                                        if (chAttr)
                                            SetFileAttributes(op->TargetName, attr);
                                        targetCannotOpenForWrite = TRUE;
                                        continue;
                                    }

                                    // na cilovych cestach podporujicich ADS provedeme jeste mazani ADSek na cilovem souboru (CREATE_ALWAYS by je mel smazat, ale na domacich W2K i XP zkratka zustavaji, netusim proc, W2K a XP z VMWare ADSka normalne mazou)
                                    if (script->TargetPathSupADS && !DeleteAllADS(out, op->TargetName))
                                    {
                                        HANDLES(CloseHandle(out));
                                        out = INVALID_HANDLE_VALUE;
                                        if (chAttr)
                                            SetFileAttributes(op->TargetName, attr);
                                        targetCannotOpenForWrite = TRUE;
                                        continue;
                                    }

                                    // pokud jsme jeste nedelali test funkcnosti zkraceni souboru na nulu, ziskame novou velikost souboru
                                    if (mustDeleteFileBeforeOverwrite == 0 /* need test */)
                                    {
                                        HANDLES(CloseHandle(out));
                                        out = HANDLES_Q(CreateFile(op->TargetName, access, 0, NULL, OPEN_ALWAYS, asyncPar->GetOverlappedFlag() | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                                        if (out == INVALID_HANDLE_VALUE) // nelze otevrit pred momentem otevreny cilovy soubor, nepravdepodobne, zkusime ho smazat a vytvorit znovu
                                        {
                                            targetCannotOpenForWrite = TRUE;
                                            continue;
                                        }
                                        CQuadWord newFileSize(0, 0); // velikost souboru po zkraceni
                                        newFileSize.LoDWord = GetFileSize(out, &newFileSize.HiDWord);
                                        if ((newFileSize.LoDWord != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) && // mame novou velikost
                                            newFileSize == CQuadWord(0, 0))                                             // soubor ma skutecne 0 bytu
                                        {
                                            if (origFileSize != CQuadWord(0, 0))            // jestli zkracovani funguje lze testnout jen na nenulovem souboru
                                                mustDeleteFileBeforeOverwrite = 2; /* no */ // zadarilo se (neni SNAP server - NSA drive, tam tohle nefunguje)
                                        }
                                        else
                                        {
                                            HANDLES(CloseHandle(out));
                                            out = INVALID_HANDLE_VALUE;
                                            mustDeleteFileBeforeOverwrite = 1 /* yes */; // pri chybe nebo pokud neni velikost nula, sychrujeme se...
                                            continue;
                                        }
                                    }

                                    if (script->CopyAttrs || !lossEncryptionAttr && copyAsEncrypted)
                                    {
                                        encryptionNotSupported = FALSE;
                                        SetCompressAndEncryptedAttrs(op->TargetName, (!lossEncryptionAttr && copyAsEncrypted ? FILE_ATTRIBUTE_ENCRYPTED : 0) | (script->CopyAttrs ? (op->Attr & (FILE_ATTRIBUTE_COMPRESSED | (lossEncryptionAttr ? 0 : FILE_ATTRIBUTE_ENCRYPTED))) : 0),
                                                                     &out, script->CopyAttrs, &encryptionNotSupported, asyncPar);
                                        if (encryptionNotSupported) // nejde nahodit Encrypted atribut, zeptame se usera co s tim...
                                        {
                                            if (dlgData.FileOutLossEncrAll)
                                                lossEncryptionAttr = TRUE;
                                            else
                                            {
                                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                                    dlgData.FileOutLossEncrAll = TRUE; // tady break; nechybi
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
                        else // obyc. error
                        {
                        NORMAL_ERROR:

                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                err = ERROR_NOT_ENOUGH_MEMORY;                         // nelze vytvorit event pro synchronizaci = nedostatek resourcu (asi nikdy nenastane, tak se s tim nesereme)
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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

    // pokud cesta konci mezerou/teckou, je invalidni a nesmime provest presun,
    // MoveFile mezery/tecky orizne a presune tak jiny soubor pripadne do jineho jmena,
    // adresare jsou na tom lepe, tam zabira pridani backslashe na konec, presunu branime
    // jen pokud vznika nove jmeno adresare, co je invalidni (pri presunu pod starym
    // jmenem je 'ignInvalidName' TRUE)
    BOOL invalidName = FileNameIsInvalid(op->SourceName, TRUE, dir) ||
                       FileNameIsInvalid(op->TargetName, TRUE, dir && ignInvalidName);

    if (!copyAsEncrypted && !script->SameRootButDiffVolume && HasTheSameRootPath(op->SourceName, op->TargetName))
    {
        // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak GetNamedSecurityInfo,
        // GetDirTime, SetFileAttributes i MoveFile mezery/tecky orizne a pracuje tak s jinou cestou
        const char* sourceNameMvDir = op->SourceName;
        char sourceNameMvDirCopy[3 * MAX_PATH];
        MakeCopyWithBackslashIfNeeded(sourceNameMvDir, sourceNameMvDirCopy);
        const char* targetNameMvDir = op->TargetName;
        char targetNameMvDirCopy[3 * MAX_PATH];
        MakeCopyWithBackslashIfNeeded(targetNameMvDir, targetNameMvDirCopy);

        int autoRetryAttempts = 0;
        CSrcSecurity srcSecurity;
        BOOL srcSecurityErr = FALSE;
        if (!invalidName && script->CopySecurity) // mame kopirovat NTFS security permissions?
        {
            srcSecurity.SrcError = GetNamedSecurityInfo(sourceNameMvDir, SE_FILE_OBJECT,
                                                        DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                                        &srcSecurity.SrcOwner, &srcSecurity.SrcGroup, &srcSecurity.SrcDACL,
                                                        NULL, &srcSecurity.SrcSD);
            if (srcSecurity.SrcError != ERROR_SUCCESS) // chyba cteni security-info ze zdrojoveho souboru -> neni co nastavovat na cili
            {
                srcSecurityErr = TRUE;
                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                    dlgData.IgnoreAllCopyPermErr = TRUE; // tady break; nechybi
                case IDB_IGNORE:
                    break;

                case IDCANCEL:
                    return FALSE;
                }
            }
        }
        FILETIME dirTimeModified;
        BOOL dirTimeModifiedIsValid = FALSE;
        if (!invalidName && dir && !*novellRenamePatch && *setDirTimeAfterMove != 2 /* no */) // problem se zrejme netyka Novell Netware, takze to tam vubec nebudeme resit (tyka se napr. Samby)
            dirTimeModifiedIsValid = GetDirTime(sourceNameMvDir, &dirTimeModified);
        while (1)
        {
            if (!invalidName && !*novellRenamePatch && MoveFile(sourceNameMvDir, targetNameMvDir))
            {
                if (script->CopyAttrs && (op->Attr & FILE_ATTRIBUTE_ARCHIVE) == 0) // nebyl nastaveny atribut Archive, MoveFile ho nastavil, zase ho vycistime
                    SetFileAttributes(targetNameMvDir, op->Attr);                  // nechame bez osetreni a retry, nedulezite (normalne se nastavuje chaoticky)

            OPERATION_DONE:

                if (script->CopyAttrs) // zkontrolujeme jestli se podarilo zachovat atributy zdrojoveho souboru
                {
                    DWORD curAttrs;
                    curAttrs = SalGetFileAttributes(targetNameMvDir);
                    if (curAttrs == INVALID_FILE_ATTRIBUTES || (curAttrs & DISPLAYED_ATTRIBUTES) != (op->Attr & DISPLAYED_ATTRIBUTES))
                    {                                                              // atributy se nejspis nepodarilo prenest, varujeme usera
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                            dlgData.IgnoreAllSetAttrsErr = TRUE; // tady break; nechybi
                        case IDB_IGNORE:
                            break;

                        case IDCANCEL:
                        {
                        MOVE_ERROR_2:

                            return FALSE; // soubor se sice presunul do cile + nastal cancel, ale na presun zpet se radsi vykasleme, nejspis to nikomu nebude nijak extra vadit
                        }
                        }
                    }
                }

                if (script->CopySecurity && !srcSecurityErr) // mame kopirovat NTFS security permissions?
                {
                    DWORD err;
                    if (!DoCopySecurity(sourceNameMvDir, targetNameMvDir, &err, &srcSecurity))
                    {
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                            dlgData.IgnoreAllCopyPermErr = TRUE; // tady break; nechybi
                        case IDB_IGNORE:
                            break;

                        case IDCANCEL:
                            goto MOVE_ERROR_2;
                        }
                    }
                }

                if (dir && dirTimeModifiedIsValid && *setDirTimeAfterMove != 2 /* no */)
                {
                    FILETIME movedDirTimeModified;
                    if (GetDirTime(targetNameMvDir, &movedDirTimeModified))
                    {
                        if (CompareFileTime(&dirTimeModified, &movedDirTimeModified) == 0)
                        {
                            if (*setDirTimeAfterMove == 0 /* need test */)
                                *setDirTimeAfterMove = 2 /* no */;
                        }
                        else
                        {
                            if (*setDirTimeAfterMove == 0 /* need test */)
                                *setDirTimeAfterMove = 1 /* yes */;
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
                // patch na Novellu - pred volanim MoveFile je treba shodit read-only atribut
                if (!invalidName && *novellRenamePatch || err == ERROR_ACCESS_DENIED)
                {
                    DWORD attr = SalGetFileAttributes(sourceNameMvDir);
                    BOOL setAttr = ClearReadOnlyAttr(sourceNameMvDir, attr);
                    if (MoveFile(sourceNameMvDir, targetNameMvDir))
                    {
                        if (!*novellRenamePatch)
                            *novellRenamePatch = TRUE; // dalsi operace uz pojedeme rovnou tudy
                        if (setAttr || script->CopyAttrs && (attr & FILE_ATTRIBUTE_ARCHIVE) == 0)
                            SetFileAttributes(targetNameMvDir, attr);

                        goto OPERATION_DONE;
                    }
                    err = GetLastError();
                    if (setAttr)
                        SetFileAttributes(sourceNameMvDir, attr);
                }

                if (StrICmp(op->SourceName, op->TargetName) != 0 && // pokud nejde jen o change-case
                    (err == ERROR_FILE_EXISTS ||                    // zkontrolujeme, jestli nejde jen o prepis dosoveho jmena souboru/adresare
                     err == ERROR_ALREADY_EXISTS) &&
                    targetNameMvDir == op->TargetName) // zadna invalidni jmena sem nepustime
                {
                    WIN32_FIND_DATA findData;
                    HANDLE find = HANDLES_Q(FindFirstFile(op->TargetName, &findData));
                    if (find != INVALID_HANDLE_VALUE)
                    {
                        HANDLES(FindClose(find));
                        const char* tgtName = SalPathFindFileName(op->TargetName);
                        if (StrICmp(tgtName, findData.cAlternateFileName) == 0 && // shoda jen pro dos name
                            StrICmp(tgtName, findData.cFileName) != 0)            // (plne jmeno je jine)
                        {
                            // prejmenujeme ("uklidime") soubor/adresar s konfliktnim dos name do docasneho nazvu 8.3 (nepotrebuje extra dos name)
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
                                if (tmpName[0] != 0) // pokud se podarilo "uklidit" konfliktni soubor/adresar, zkusime presun
                                {                    // souboru/adresare znovu, pak vratime "uklizenemu" souboru/adresari jeho puvodni jmeno
                                    BOOL moveDone = SalMoveFile(sourceNameMvDir, op->TargetName);
                                    if (script->CopyAttrs && (op->Attr & FILE_ATTRIBUTE_ARCHIVE) == 0) // nebyl nastave atribut Archive, MoveFile ho nastavil, zase ho vycistime
                                        SetFileAttributes(op->TargetName, op->Attr);                   // nechame bez osetreni a retry, nedulezite (normalne se nastavuje chaoticky)
                                    if (!SalMoveFile(tmpName, origFullName))
                                    { // toto se zjevne muze stat, nepochopitelne, ale Windows vytvori misto op->TargetName (dos name) soubor se jmenem origFullName
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
                                            SetFileAttributes(origFullName, origFullNameAttr); // nechame bez osetreni a retry, nedulezite (normalne se nastavuje chaoticky)
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

                if ((err == ERROR_ALREADY_EXISTS || // teoreticky muze nastat i pro adresare, tomu zabranime (overwrite prompt je jen pro soubory)
                     err == ERROR_FILE_EXISTS) &&
                    !dir && StrICmp(op->SourceName, op->TargetName) != 0 &&
                    sourceNameMvDir == op->SourceName && targetNameMvDir == op->TargetName) // zadna invalidni jmena sem nepustime (je to jen pro soubory a u nich se jmena kontroluji)
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

                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                        if (*dlgData.CancelWorker)
                            goto CANCEL_OPEN;

                        if (dir)
                            TRACE_E("Error in script.");

                        if (dlgData.SkipAllOverwrite)
                            goto SKIP_OPEN;

                        int ret;
                        ret = IDCANCEL;

                        if (!getTimeFailed && script->OverwriteOlder) // option z Copy/Move dialogu
                        {
                            // orizneme casy na sekundy (ruzne FS ukladaji casy s ruznymi prestnostmi, takze dochazelo k "rozdilum" i mezi "shodnymi" casy)
                            *(unsigned __int64*)&sFileTime = *(unsigned __int64*)&sFileTime - (*(unsigned __int64*)&sFileTime % 10000000);
                            *(unsigned __int64*)&tFileTime = *(unsigned __int64*)&tFileTime - (*(unsigned __int64*)&tFileTime % 10000000);

                            if (CompareFileTime(&sFileTime, &tFileTime) > 0)
                                ret = IDYES; // starsi mame bez ptani prepsat
                            else
                                ret = IDB_SKIP; // ostatni existujici skipnout
                        }
                        else
                        {
                            // zobrazime dotaz
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
                        if (!dlgData.OverwriteHiddenAll && dlgData.CnfrmSHFileOver) // zde script->OverwriteOlder ignorujeme, user chce videt, ze jde o SYSTEM or HIDDEN soubor i pri zaplem optionu
                        {
                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                            attr = SalGetFileAttributes(op->TargetName); // muze i selhat (priradi se INVALID_FILE_ATTRIBUTES)
                        }
                    }

                    ClearReadOnlyAttr(op->TargetName, attr); // aby sel smazat ...
                    while (1)
                    {
                        if (DeleteFile(op->TargetName))
                            break;
                        else
                        {
                            DWORD err2 = GetLastError();
                            if (err2 == ERROR_FILE_NOT_FOUND)
                                break; // pokud uz user stihl soubor smazat sam, je vse OK

                            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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

                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                    if (*dlgData.CancelWorker)
                        return FALSE;

                    if (dlgData.SkipAllMoveErrors)
                        goto SKIP_MOVE_ERROR;

                    if (err == ERROR_SHARING_VIOLATION && ++autoRetryAttempts <= 2)
                    {               // auto-retry zavedeno kvuli chybam presuvu behem nacitani ikon v adresari (soubeh SHGetFileInfo a MoveFile)
                        Sleep(100); // chvilku pockame pred dalsim pokusem
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
        if (notError && !skip) // jeste je potreba uklidit soubor ze sourcu
        {
            ClearReadOnlyAttr(op->SourceName); // aby sel smazat ...
            while (1)
            {
                if (DeleteFile(op->SourceName))
                    break;
                {
                    DWORD err = GetLastError();

                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
    // pokud cesta konci mezerou/teckou, je invalidni a nesmime provest mazani,
    // DeleteFile mezery/tecky orizne a smaze tak jiny soubor
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
                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
            ClearReadOnlyAttr(name, attr); // aby sel smazat ...

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
                        //            if (ext == fileName)   // ".cvspass" ve Windows je pripona ...
                        if (ext < fileName)
                            ext = fileName + tmpLen;
                        else
                            ext++;
                        useRecycleBin = dlgData.AgreeRecycleMasks(fileName, ext);
                    }
                    else
                    {
                        useRecycleBin = TRUE; // pri chybe volime bezpecnou variantu, mazeme do kose
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
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
            DWORD attr2 = SalGetFileAttributes(name); // zjistime aktualni stav atributu
            if (attr2 != INVALID_FILE_ATTRIBUTES)
                attr = attr2;
        }
    }
}

BOOL SalCreateDirectoryEx(const char* name, DWORD* err)
{
    if (err != NULL)
        *err = 0;
    // pokud jmeno obsahuje mezeru/tecku na konci, je nutne pridat na konec '\\', jinak
    // se vytvori jiny adresar (mezery/tecky na konci jmena CreateDirectory tise orizne)
    const char* nameCrDir = name;
    char nameCrDirBuf[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(nameCrDir, nameCrDirBuf);
    if (CreateDirectory(nameCrDir, NULL))
        return TRUE;
    else
    {
        DWORD errLoc = GetLastError();
        if (name == nameCrDir &&            // jmeno s mezerou/teckou na konci jiste nekoliduje s DOS jmenem
            (errLoc == ERROR_FILE_EXISTS || // zkontrolujeme, jestli nejde jen o prepis dosoveho jmena souboru
             errLoc == ERROR_ALREADY_EXISTS))
        {
            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(name, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(find));
                const char* tgtName = SalPathFindFileName(name);
                if (StrICmp(tgtName, data.cAlternateFileName) == 0 && // shoda jen pro dos name
                    StrICmp(tgtName, data.cFileName) != 0)            // (plne jmeno je jine)
                {
                    // prejmenujeme ("uklidime") soubor/adresar s konfliktnim dos name do docasneho nazvu 8.3 (nepotrebuje extra dos name)
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
                        if (tmpName[0] != 0) // pokud se podarilo "uklidit" konfliktni soubor, zkusime presun
                        {                    // souboru znovu, pak vratime "uklizenemu" souboru jeho puvodni jmeno
                            BOOL createDirDone = CreateDirectory(name, NULL);
                            if (!SalMoveFile(tmpName, origFullName))
                            { // toto se zjevne muze stat, nepochopitelne, ale Windows vytvori misto name (dos name) soubor se jmenem origFullName
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
                                    SetFileAttributes(origFullName, origFullNameAttr); // nechame bez osetreni a retry, nedulezite (normalne se nastavuje chaoticky)
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
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak CreateFile
    // mezery/tecky orizne a pracuje tak s jinou cestou
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
            showError = FALSE; // uspech!
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
        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
            dlgData.IgnoreAllCopyDirTimeErr = TRUE; // tady break; nechybi
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

    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak SetFileAttributes
    // a RemoveDirectory mezery/tecky orizne a pracuje tak s jinou cestou
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
            script->AddBytesToSpeedMetersAndTFSandPS((DWORD)CREATE_DIR_SIZE.Value, TRUE, 0, NULL, MAX_OP_FILESIZE); // uz je vytvoreny adresar

            DWORD newAttr = attr & clearReadonlyMask;
            if (sourceDir != NULL && adsCopy) // je-li treba kopirovat ADS, udelame to
            {
                CQuadWord operDone = CREATE_DIR_SIZE; // uz je vytvoreny adresar
                BOOL adsSkip = FALSE;
                if (!DoCopyADS(hProgressDlg, sourceDir, TRUE, name, totalDone,
                               operDone, operTotal, dlgData, script, &adsSkip, buffer) ||
                    adsSkip) // user dal cancel nebo Skipnul aspon jedno ADS
                {
                    if (RemoveDirectory(nameCrDir) == 0)
                    {
                        DWORD err2 = GetLastError();
                        TRACE_E("Unable to remove newly created directory: " << name << ", error: " << GetErrorText(err2));
                    }
                    if (!adsSkip)
                        return FALSE; // cancel cele operace (Skip musi vracet TRUE)
                    skip = TRUE;
                    newAttr = -1; // adresar uz by nemel existovat, nebudeme mu tedy nastavovat atributy
                }
            }
            if (newAttr != -1)
            {
                if (script->CopyAttrs || createAsEncrypted) // nastavime Compressed & Encrypted atributy podle zdrojoveho adresare
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
                                changeAttrErr = MyEncryptFile(hProgressDlg, name, currentAttrs, 0 /* aby umel zasifrovat i adresare se SYSTEM atributem */,
                                                              dlgData, dummyCancelOper, FALSE);

                                if ( //(WindowsVistaAndLater || script->TargetPathSupEFS) &&  // rveme bez ohledu na system a podporu EFS; puvodne: ze adresar na FATce nejde zakryptit rvou az Visty, chovame se stejne (kvuli srovnani s Explorerem, Encrypted atribut neni az tak dulezity)
                                    !dlgData.DirCrLossEncrAll && changeAttrErr != ERROR_SUCCESS)
                                {                                                              // adresari se nepodarilo nastavit Encrypted atribut, poptame se usera co s tim
                                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                        dlgData.DirCrLossEncrAll = TRUE; // tady break; nechybi
                                    case IDYES:
                                        break;

                                    case IDB_SKIPALL:
                                        dlgData.SkipAllDirCrLossEncr = TRUE;
                                    case IDB_SKIP:
                                    {
                                        ClearReadOnlyAttr(nameCrDir); // aby se dal soubor smazat, nesmi mit read-only atribut
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

                if (script->CopyAttrs) // zkontrolujeme jestli se podarilo zachovat atributy zdrojoveho souboru
                {
                    DWORD curAttrs;
                    curAttrs = SalGetFileAttributes(name);
                    if (curAttrs == INVALID_FILE_ATTRIBUTES || (curAttrs & DISPLAYED_ATTRIBUTES) != (newAttr & DISPLAYED_ATTRIBUTES))
                    {                                                              // atributy se nejspis nepodarilo prenest, varujeme usera
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                            dlgData.IgnoreAllSetAttrsErr = TRUE; // tady break; nechybi
                        case IDB_IGNORE:
                            break;

                        case IDCANCEL:
                        {
                        CANCEL_CRDIR:

                            ClearReadOnlyAttr(nameCrDir); // aby se dal soubor smazat, nesmi mit read-only atribut
                            RemoveDirectory(nameCrDir);
                            return FALSE;
                        }
                        }
                    }
                }

                if (sourceDir != NULL && script->CopySecurity) // mame kopirovat NTFS security permissions?
                {
                    DWORD err2;
                    if (!DoCopySecurity(sourceDir, name, &err2, NULL))
                    {
                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                            dlgData.IgnoreAllCopyPermErr = TRUE; // tady break; nechybi
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
                    if (dlgData.CnfrmDirOver && !dlgData.DirOverwriteAll) // mame se ptat usera na prepis adresare?
                    {
                        char sAttr[101], tAttr[101];
                        GetDirInfo(sAttr, sourceDir);
                        GetDirInfo(tAttr, name);

                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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

                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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

            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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

                skip = TRUE; // jde o skip (musi se skipnout vsechny operace v adresari)
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

    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak SetFileAttributes
    // i RemoveDirectory mezery/tecky orizne a pracuje tak s jinou cestou
    const char* nameRmDir = name;
    char nameRmDirCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(nameRmDir, nameRmDirCopy);

    while (1)
    {
        ClearReadOnlyAttr(nameRmDir, attr); // aby sel smazat ...

        err = ERROR_SUCCESS;
        if (script->CanUseRecycleBin && !dontUseRecycleBin &&
            (script->InvertRecycleBin && dlgData.UseRecycleBin == 0 ||
             !script->InvertRecycleBin && dlgData.UseRecycleBin == 1) &&
            IsDirectoryEmpty(name)) // podadresar nesmi obsahovat zadne soubory !!!
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
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
            if (*dlgData.CancelWorker)
                return FALSE;

            if (dlgData.SkipAllDeleteErr)
                goto SKIP_DELETE;

            if (AutoRetryCounter < 4 && GetTickCount() - startTime + (AutoRetryCounter + 1) * 100 <= 2000 &&
                (err == ERROR_DIR_NOT_EMPTY || err == ERROR_SHARING_VIOLATION))
            { // auto-retry sem davame, aby se resila tato situace: mam adresare 1\2\3, mazu 1 vcetne podadresaru, 3 je v panelu (sleduji se v nem zmeny) -> pri mazani 2 se ohlasi chyba "mazani neprazdneho adresare", protoze 3 zustava v "mezistavu" kvuli sledovani zmen (je sice smazany, tedy nejde listovat, atd., ale na disku porad jeste je, dobra pakarna)
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

        DWORD attr2 = SalGetFileAttributes(nameRmDir); // zjistime aktualni stav atributu
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
    // vymaz reparse-pointu z adresare 'nameDelLink'
    if (err != NULL)
        *err = ERROR_SUCCESS;
    BOOL ok = FALSE;
    DWORD attr = GetFileAttributes(nameDelLink);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
    {
        HANDLE dir = HANDLES_Q(CreateFile(nameDelLink, GENERIC_WRITE /* | GENERIC_READ */, 0, 0, OPEN_EXISTING,
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
                { // pokud nejde o volume mount point, junction ani symlink, jdeme ohlasit chybu (asi umime smazat, ale radsi to odmitneme, abysme neco neposrali...)
                    TRACE_E("DoDeleteDirLinkAux(): Unknown type of reparse point (tag is 0x" << std::hex << juncData->ReparseTag << std::dec << "): " << nameDelLink);
                    if (err != NULL)
                        *err = 4394 /* ERROR_REPARSE_TAG_MISMATCH */;
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
        ok = TRUE; // uz je zrejme smazany reparse-point, zbyva jeste smazat prazdny adresar...
    // odstranime prazdny adresar (co zbyl po vymazu reparse-pointu)
    if (ok)
        ClearReadOnlyAttr(nameDelLink, attr); // aby sel smazat i s read-only atributem
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
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak CreateFile
    // i RemoveDirectory mezery/tecky orizne a pracuje tak s jinou cestou
    const char* nameDelLink = name;
    char nameDelLinkCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(nameDelLink, nameDelLinkCopy);

    return DoDeleteDirLinkAux(nameDelLink, err);
}

BOOL DoDeleteDirLink(HWND hProgressDlg, char* name, const CQuadWord& size, COperations* script,
                     CQuadWord& totalDone, CProgressDlgData& dlgData)
{
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak CreateFile
    // i RemoveDirectory mezery/tecky orizne a pracuje tak s jinou cestou
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
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
// b) obsah souboru name prenese do docasneho souboru a zaroven nad daty
//    provadi konverze urcene promennou convertData.CodeType a convertData.EOFType
// c) soubor name prepise docasnym souborem
//
// convertData.EOFType  - urcuje, cim se nahradi konce radku
//            za konec radku se povazuji vsechny CR, LF a CRLF
//            0: neprovadi se nahrada koncu radku
//            1: konce radku se nahradi CRLF (DOS, Windows, OS/2)
//            2: konce radku se nahradi LF (UNIX)
//            3: konce radku se nahradi CR (MAC)
BOOL DoConvert(HWND hProgressDlg, char* name, char* sourceBuffer, char* targetBuffer,
               const CQuadWord& size, COperations* script, CQuadWord& totalDone,
               CConvertData& convertData, CProgressDlgData& dlgData)
{
    // pokud cesta konci mezerou/teckou, je invalidni a nesmime provest konverzi,
    // CreateFile mezery/tecky orizne a zkonverti tak jiny soubor
    BOOL invalidName = FileNameIsInvalid(name, TRUE);

CONVERT_AGAIN:

    CQuadWord operationDone;
    operationDone = CQuadWord(0, 0);
    while (1)
    {
        // pokusim se otevrit zdrojovy soubor
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
            // vytahnu si cestu pro docasny soubor
            char tmpPath[MAX_PATH];
            strcpy(tmpPath, name);
            char* terminator = strrchr(tmpPath, '\\');
            if (terminator == NULL)
            {
                // pracovni testik
                TRACE_E("Parameter 'name' must be full path to file (including path)");
                HANDLES(CloseHandle(hSource));
                return FALSE;
            }
            *(terminator + 1) = 0;

            // najdu nazev pro docasny soubor a necham soubor vytvorit
            char tmpFileName[MAX_PATH];
            BOOL tmpFileExists = FALSE;
            while (1)
            {
                if (SalGetTempFileName(tmpPath, "cnv", tmpFileName, TRUE))
                {
                    tmpFileExists = TRUE;

                    // nastavime atributy tmp-souboru dle zdrojoveho souboru
                    DWORD srcAttrs = SalGetFileAttributes(name);
                    DWORD tgtAttrs = SalGetFileAttributes(tmpFileName);
                    BOOL changeAttrs = FALSE;
                    if (srcAttrs != INVALID_FILE_ATTRIBUTES && tgtAttrs != INVALID_FILE_ATTRIBUTES && srcAttrs != tgtAttrs)
                    {
                        changeAttrs = TRUE; // pozdeji provedeme SetFileAttributes...
                        // lisi se priznak kompresse na NTFS ?
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
                                MyEncryptFile(hProgressDlg, tmpFileName, tgtAttrs, 0 /* aby umel zasifrovat i soubory se SYSTEM atributem */,
                                              dlgData, cancelOper, FALSE);
                            }
                            else
                                MyDecryptFile(tmpFileName, tgtAttrs, FALSE);
                            if (*dlgData.CancelWorker || cancelOper)
                            {
                                HANDLES(CloseHandle(hSource));
                                ClearReadOnlyAttr(tmpFileName); // aby sel smazat
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

                    // otevru prazdny docasny soubor
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
                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                                if (*dlgData.CancelWorker)
                                {
                                CONVERT_ERROR:

                                    if (hSource != NULL)
                                        HANDLES(CloseHandle(hSource));
                                    if (hTarget != NULL)
                                        HANDLES(CloseHandle(hTarget));
                                    ClearReadOnlyAttr(tmpFileName); // aby sel smazat
                                    DeleteFile(tmpFileName);
                                    return FALSE;
                                }

                                // provedu preklad sourceBuffer -> targetBuffer
                                char* sourceIterator;
                                char* targetIterator;
                                sourceIterator = sourceBuffer;
                                targetIterator = targetBuffer;
                                while (sourceIterator - sourceBuffer < (int)read)
                                {
                                    // lastChar je TRUE, pokud sourceIterator ukazuje na posledni znak v bufferu
                                    BOOL lastChar = (sourceIterator - sourceBuffer == (int)read - 1);

                                    if (convertData.EOFType != 0)
                                    {
                                        if (crlfBreak && sourceIterator == sourceBuffer && *sourceIterator == '\n')
                                        {
                                            // toto CRLF mame uz zpracovano, ted necham LF byt
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
                                                // odchytim CRLF, ktere se lame na rozhrani bufferu
                                                if (lastChar && *sourceIterator == '\r')
                                                    crlfBreak = TRUE;
                                                // odchytim CRLF, ktere se nelame - musim preskocit LF
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

                                // provedeme zapis do tmp souboru
                                while (1)
                                {
                                    if (WriteFile(hTarget, targetBuffer, (DWORD)(targetIterator - targetBuffer), &written, NULL) &&
                                        targetIterator - targetBuffer == (int)written)
                                        break;

                                WRITE_ERROR_CONVERT:

                                    DWORD err;
                                    err = GetLastError();

                                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                            ClearReadOnlyAttr(tmpFileName); // aby sel smazat
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
                                        ClearReadOnlyAttr(tmpFileName); // aby sel smazat
                                        DeleteFile(tmpFileName);
                                        SetProgress(hProgressDlg, 0, CaclProg(totalDone, script->TotalSize), dlgData);
                                        return TRUE;
                                    }

                                    case IDCANCEL:
                                        goto CONVERT_ERROR;
                                    }
                                }
                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
                        // zavreme soubory a nastavime globalni progress
                        // nepouzijeme operationDone, aby byl progress korektni i pri zmenach souboru "pod nohama"
                        HANDLES(CloseHandle(hSource));
                        if (!HANDLES(CloseHandle(hTarget))) // i po neuspesnem volani predpokladame, ze je handle zavreny,
                        {                                   // viz https://forum.altap.cz/viewtopic.php?f=6&t=8455
                            hSource = hTarget = NULL;       // (pise, ze cilovy soubor lze smazat, tedy nezustal otevreny jeho handle)
                            goto WRITE_ERROR_CONVERT;
                        }
                        totalDone += size;
                        // donastavime atributy (read-only pri zapisu dela potize)
                        if (changeAttrs)
                            SetFileAttributes(tmpFileName, srcAttrs);
                        // prepiseme puvodni soubor tmp-souborem
                        while (1)
                        {
                            ClearReadOnlyAttr(name); // aby sel smazat ...
                            if (DeleteFile(name))
                            {
                                while (1)
                                {
                                    if (SalMoveFile(tmpFileName, name))
                                        return TRUE; // uspesne koncime
                                    else
                                    {
                                        DWORD err = GetLastError();

                                        WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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

                                WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
                                if (*dlgData.CancelWorker)
                                {
                                CANCEL_CONVERT:

                                    ClearReadOnlyAttr(tmpFileName); // aby sel smazat ...
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

                                    ClearReadOnlyAttr(tmpFileName); // aby sel smazat ...
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

                    char fakeName[MAX_PATH]; // jmeno tmp-souboru, ktery nejde vytvorit/otevrit
                    if (tmpFileExists)
                    {
                        strcpy(fakeName, tmpFileName);
                        ClearReadOnlyAttr(tmpFileName); // aby sel smazat
                        DeleteFile(tmpFileName);        // tmp byl vytvoren, pokusime se ho smazat ...
                        tmpFileExists = FALSE;
                    }
                    else
                    {
                        // dame dohromady smyslene jmeno tmp-filu, ktery se nepodarilo vytvorit
                        char* s = tmpPath + strlen(tmpPath);
                        if (s > tmpPath && *(s - 1) == '\\')
                            s--;
                        memcpy(fakeName, tmpPath, s - tmpPath);
                        strcpy(fakeName + (s - tmpPath), "\\cnv0000.tmp");
                    }

                    WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak
    // SetFileAttributes (a dalsi) mezery/tecky orizne a pracuje
    // tak s jinou cestou
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
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
            // pokud mame nastavit jeden z casu
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

            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
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
    //--- vytvorime lokalni kopii dat
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
        script->TotalSize = CQuadWord(1, 0); // proti deleni nulou
                                             // TRACE_E("ThreadWorkerBody(): script->TotalSize may not be zero!");  // pri stavbe se nenastavuje "synchrovaci jednicka", zlobilo u Calculate Occupied Space
    }

    if (script->CopySecurity)
        GainWriteOwnerAccess();

    HWND hProgressDlg = data->HProgressDlg;
    void* buffer = data->Buffer;
    BOOL bufferIsAllocated = data->BufferIsAllocated;
    CChangeAttrsData* attrsData = (CChangeAttrsData*)data->Buffer;
    DWORD clearReadonlyMask = data->ClearReadonlyMask;
    CConvertData convertData;
    if (data->ConvertData != NULL) // udelame si kopii dat pro Convert
    {
        convertData = *data->ConvertData;
    }
    SetEvent(wContinue); // data mame, pustime dal hl. thread nebo progress-dialog thread
                         //---
    SetProgress(hProgressDlg, 0, 0, dlgData);
    script->InitSpeedMeters(FALSE);

    char lastLantasticCheckRoot[MAX_PATH]; // posledni root cesty kontrolovany na Lantastic ("" = nic nebylo kontrolovano)
    lastLantasticCheckRoot[0] = 0;
    BOOL lastIsLantasticPath = FALSE;                                                                                  // vysledek kontroly na rootu lastLantasticCheckRoot
    int mustDeleteFileBeforeOverwrite = 0; /* need test */                                                             // (zavedeno kvuli SNAP serveru - NSA drive - neslape jim SetEndOfFile - 0/1/2 = need-test/yes/no)
    int allocWholeFileOnStart = 0; /* need test */                                                                     // zavedeno jako jisteni (napr. na SNAP serveru - NSA drivech - asi nebude slapat), nemuzeme si dovolit riskovat nefunkcni Copy - 0/1/2 = need-test/yes/no
    int setDirTimeAfterMove = script->PreserveDirTime && script->SourcePathIsNetwork ? 0 /* need test */ : 2 /* no */; // napr. na Sambe po presunu/prejmenovani adresare dochazi ke zmene jeho datumu a casu - 0/1/2 = need-test/yes/no

    BOOL Error = FALSE;
    CQuadWord totalDone;
    totalDone = CQuadWord(0, 0);
    CProgressData pd;
    BOOL novellRenamePatch = FALSE; // TRUE pokud je nutne odstranovat read-only atribut pred volanim MoveFile (nutne na Novellu)
    char* tgtBuffer = NULL;         // prekladovy buffer pro ocConvert
    CAsyncCopyParams* asyncPar = NULL;
    if (buffer != NULL)
    {
        // nacteme retezce dopredu, aby se to nedelalo pro kazdou operaci zvlast (plni se rychle LoadStr buffer + brzdi)
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
                    if (skip) // skip vytvareni adresare
                    {
                        // preskocime vsechny operace skriptu az do znacky uzavreni tohoto adresare
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
                            op->Attr = 0x10000000 /* dir already existed */;
                        else
                            op->Attr = 0x01000000 /* dir was created */;
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
                // najdeme skip-label, u nej je index create-dir operace a v ni je ulozene, jestli
                // cilovy adresar uz existoval nebo jestli jsme ho vytvareli (datum&cas se kopiruje
                // jen pokud jsme adresar vytvareli)
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
                            if (crDir->Attr == 0x10000000 /* dir already existed */)
                                skipSetDirTime = TRUE;
                            else
                            {
                                if (crDir->Attr != 0x01000000 /* dir was created */)
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
                // vystupni buffer - do nej budu provadet preklad (pro nejnepriznivejsi pripad,
                // kdy vstupni soubor obsahuje same CR nebo LF a prekladame je na CRLF je tento
                // buffer dvojnasobne velikosti nez sourceBuffer) a posleze z nej budu zapisovat
                // do docasneho souboru
                if (tgtBuffer == NULL) // prvni pruchod ?
                {
                    tgtBuffer = (char*)malloc(OPERATION_BUFFER * 2);
                    if (tgtBuffer == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        Error = TRUE;
                        break; // chyba ...
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
                break; // zadna cinnost
            }
            if (Error)
                break;
            WaitForSingleObject(dlgData.WorkerNotSuspended, INFINITE); // pokud mame byt v suspend-modu, cekame ...
        }
        if (!Error && !*dlgData.CancelWorker && i == script->Count && totalDone != script->TotalSize &&
            (totalDone != CQuadWord(0, 0) || script->TotalSize != CQuadWord(1, 0))) // umyslna zmena script->TotalSize na jednicku (opatreni proti deleni nulou)
        {
            TRACE_E("ThreadWorkerBody(): operation done: totalDone != script->TotalSize (" << totalDone.Value << " != " << script->TotalSize.Value << ")");
        }
        CQuadWord transferredFileSize, progressSize;
        if (!Error && !*dlgData.CancelWorker && i == script->Count &&
            script->GetTFSandProgressSize(&transferredFileSize, &progressSize) &&
            (transferredFileSize != script->TotalFileSize ||
             progressSize != script->TotalSize &&
                 (progressSize != CQuadWord(0, 0) || script->TotalSize != CQuadWord(1, 0)))) // umyslna zmena script->TotalSize na jednicku (opatreni proti deleni nulou)
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
    *dlgData.CancelWorker = Error;                  // pokud jde o Cancel, dame to najevo ...
    SendMessage(hProgressDlg, WM_COMMAND, IDOK, 0); // koncime ...
    WaitForSingleObject(wContinue, INFINITE);       // potrebujeme zastavit hl.thread

    FreeScript(script); // vola delete, hl. thread tedy nemuze bezet

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
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
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

    // if (Worker != NULL) HANDLES(CloseHandle(Worker));  // nejspis bylo zbytecne
    HANDLE worker = HANDLES(CreateThread(NULL, 0, ThreadWorker, &data, 0, &threadID));
    if (worker == NULL)
    {
        if (data.BufferIsAllocated)
            free(data.Buffer);
        TRACE_E("Unable to start Worker thread.");
        return NULL;
    }
    //  SetThreadPriority(Worker, THREAD_PRIORITY_HIGHEST);
    WaitForSingleObject(wContinue, INFINITE); // pockame az si prekopiruje data (jsou na stacku)
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
    for (i = 0; i < OperDlgs.Count; i++) // zajistime unikatnost (operace muze byt pridana jen jednou)
        if (OperDlgs[i] == dlg)
            break;

    BOOL ret = FALSE;
    if (i == OperDlgs.Count) // operaci je mozne pridat
    {
        OperDlgs.Add(dlg);
        if (OperDlgs.IsGood())
        {
            if (startOnIdle)
            {
                int j;
                for (j = 0; j < OperPaused.Count && OperPaused[j] == 1 /* auto-paused */; j++)
                    ; // pokud jiz nejaka operace bezi nebo byla rucne pausnuta, startujeme tuto operaci jako "auto-paused"
                *startPaused = j < OperPaused.Count;
            }
            else
                *startPaused = FALSE;
            OperPaused.Add(*startPaused ? 1 /* auto-paused */ : 0 /* running */);
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
            for (j = 0; j < OperPaused.Count && OperPaused[j] == 1 /* auto-paused */; j++)
                ; // pokud nebezi zadna operace ani nebyla zadna operace rucne pausnuta, resumneme prvni operaci ve fronte
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
            OperPaused[j] = 1 /* auto-paused */;
            break;
        }
    }
    if (i == OperDlgs.Count)
        TRACE_E("COperationsQueue::AutoPauseOperation(): operation was not found!");

    // pokud nebezi zadna operace ani nebyla zadna operace rucne pausnuta, resumneme prvni operaci ve fronte
    int j;
    for (j = 0; j < OperPaused.Count && OperPaused[j] == 1 /* auto-paused */; j++)
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
