// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//#include "menu.h"
//#include "drivelst.h"
//#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"

int CPlugins::AddPluginToOrder(const char* dllName, BOOL showInBar)
{
    int index = -1;
    CPluginOrder o;
    o.DLLName = DupStr(dllName);
    o.ShowInBar = showInBar;
    o.Index = -1;
    o.Flags = 0;
    if (o.DLLName != NULL)
    {
        index = Order.Add(o);
        if (!Order.IsGood())
        {
            index = -1;
            free(o.DLLName);
            Order.ResetState();
        }
    }
    return index;
}

void CPlugins::QuickSortPluginsByName(int left, int right)
{
    int i = left, j = right;
    const char* pivot = Data[Order[(i + j) / 2].Index]->Name;

    do
    {
        while (strcmp(Data[Order[i].Index]->Name, pivot) < 0 && i < right)
            i++;
        while (strcmp(pivot, Data[Order[j].Index]->Name) < 0 && j > left)
            j--;

        if (i <= j)
        {
            CPluginOrder swap = Order[i];
            Order[i] = Order[j];
            Order[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    if (left < j)
        QuickSortPluginsByName(left, j);
    if (i < right)
        QuickSortPluginsByName(i, right);
}

BOOL CPlugins::PluginVisibleInBar(const char* dllName)
{
    int i;
    for (i = 0; i < Order.Count; i++)
    {
        CPluginOrder* order = &Order[i];
        if (stricmp(dllName, order->DLLName) == 0)
        {
            return order->ShowInBar;
        }
    }
    return FALSE;
}

void CPlugins::UpdatePluginsOrder(BOOL sortByName)
{
    BOOL firstAdded = TRUE;

    // abychom pozdeji mohli vyradit neexistujici pluginy, nastavime pomocnou promennou
    int i;
    for (i = 0; i < Order.Count; i++)
        Order[i].Flags = 0;

    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* pluginData = Data[i];

        int foundIndex = -1;
        int j;
        for (j = 0; j < Order.Count; j++)
        {
            CPluginOrder* order = &Order[j];
            if (order->Flags == 0 && stricmp(pluginData->DLLName, order->DLLName) == 0)
            {
                foundIndex = j;
                break;
            }
        }
        DWORD flags = 1;
        if (foundIndex == -1) // tento plugin nema zaznam v poli Order => pripojime ho na konec
        {
            foundIndex = AddPluginToOrder(pluginData->DLLName, TRUE);
            if (firstAdded)
            {
                firstAdded = FALSE;
                flags = 2;
            }
        }

        if (foundIndex != -1)
        {
            Order[foundIndex].Index = i;
            Order[foundIndex].Flags = flags;
        }
    }

    // vyradime pluginy, ktere uz neexistuji
    for (i = Order.Count - 1; i >= 0; i--)
    {
        if (Order[i].Flags == 0)
        {
            free(Order[i].DLLName);
            Order.Delete(i);
        }
    }

    // dohledame prvni nove pridany plugin
    int index = -1;
    for (i = 0; i < Order.Count; i++)
    {
        if (Order[i].Flags == 2)
        {
            index = i;
            break;
        }
    }

    // pokud jsme nasli nove pridany plugin a zaroven za nim jsou dalsi, seradime je
    if (Order.Count > 1 && !sortByName && (index != -1 && index < Order.Count - 1))
        QuickSortPluginsByName(index, Order.Count - 1);
    if (Order.Count > 1 && sortByName)
        QuickSortPluginsByName(0, Order.Count - 1);
}

BOOL CPlugins::ChangePluginsOrder(int index, int newIndex)
{
    if (index < 0 || index >= Order.Count)
    {
        return FALSE;
    }
    if (newIndex < 0 || newIndex >= Order.Count)
    {
        return FALSE;
    }
    if (index == newIndex)
    {
        return FALSE;
    }

    // do 'tmp' ulozim polozku ze zdrojoveho indexu
    CPluginOrder tmp;
    tmp = Order[index];

    // posunu polozky mezi
    if (index < newIndex)
    {
        int i;
        for (i = index; i < newIndex; i++)
            Order[i] = Order[i + 1];
    }
    else
    {
        int i;
        for (i = index; i > newIndex; i--)
            Order[i] = Order[i - 1];
    }

    // do cile ulozim 'tmp'
    Order[newIndex] = tmp;

    return TRUE;
}

int CPlugins::GetIndexByOrder(int index)
{
    if (index < 0 || index >= Order.Count)
        return -1;
    return Order[index].Index;
}

int CPlugins::GetPluginOrderIndex(const CPluginData* plugin)
{
    int i;
    for (i = 0; i < Order.Count; i++)
    {
        int orderIndex = Order[i].Index;
        if (plugin != NULL && plugin == Data[orderIndex])
            return i;
    }
    return -1;
}

BOOL CPlugins::GetShowInBar(int index)
{
    CPluginData* plugin = Get(index);
    if (plugin != NULL)
        return plugin->ShowSubmenuInPluginsBar;
    else
        return FALSE;
}

void CPlugins::SetShowInBar(int index, BOOL showInBar)
{
    CPluginData* plugin = Get(index);
    if (plugin != NULL)
        plugin->ShowSubmenuInPluginsBar = showInBar;
}

BOOL CPlugins::GetShowInChDrv(int index)
{
    CPluginData* plugin = Get(index);
    if (plugin != NULL)
        return plugin->ChDrvMenuFSItemVisible;
    else
        return FALSE;
}

void CPlugins::SetShowInChDrv(int index, BOOL showInChDrv)
{
    CPluginData* plugin = Get(index);
    if (plugin != NULL)
        plugin->ChDrvMenuFSItemVisible = showInChDrv;
}

int CPlugins::FindIndexForNewPluginFSTimer(DWORD timeoutAbs)
{
    if (PluginFSTimers.Count == 0)
        return 0;

    // vsechny casy se musi vztahnout k nejblizsimu timeoutu, protoze jen tak se podari
    // seradit timeouty, ktere prekroci 0xFFFFFFFF
    DWORD timeoutAbsBase = PluginFSTimers[0]->AbsTimeout;
    if ((int)(timeoutAbs - timeoutAbsBase) < 0)
        timeoutAbsBase = timeoutAbs;
    timeoutAbs -= timeoutAbsBase;

    int l = 0, r = PluginFSTimers.Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        DWORD actTimeoutAbs = PluginFSTimers[m]->AbsTimeout - timeoutAbsBase;
        if (actTimeoutAbs == timeoutAbs)
        {
            while (++m < PluginFSTimers.Count && PluginFSTimers[m]->AbsTimeout - timeoutAbsBase == timeoutAbs)
                ;     // vratime index za posledni stejny timer
            return m; // nalezeno
        }
        else if (actTimeoutAbs > timeoutAbs)
        {
            if (l == r || l > m - 1)
                return m; // nenalezeno, mel by byt na teto pozici
            r = m - 1;
        }
        else
        {
            if (l == r)
                return m + 1; // nenalezeno, mel by byt az za touto pozici
            l = m + 1;
        }
    }
}

BOOL CPlugins::AddPluginFSTimer(DWORD relTimeout, CPluginFSInterfaceAbstract* timerOwner, DWORD timerParam)
{
    BOOL ret = FALSE;
    CPluginFSTimer* timer = new CPluginFSTimer(GetTickCount() + relTimeout, timerOwner, timerParam, TimerTimeCounter++);
    if (timer != NULL)
    {
        int index = FindIndexForNewPluginFSTimer(timer->AbsTimeout);
        PluginFSTimers.Insert(index, timer);
        if (PluginFSTimers.IsGood())
        {
            ret = TRUE;

            if (index == 0) // zmena nejblizsiho timeoutu, musime zmenit windowsovy timer
            {
                DWORD ti = timer->AbsTimeout - GetTickCount();
                if ((int)ti > 0) // pokud jiz nenastal timeout noveho timeru (muze nastat i zaporny rozdil casu), zmenime nebo nahodime windows timer
                {
                    SetTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS, ti, NULL);
                }
                else
                {
                    if ((int)ti < 0)
                        TRACE_E("CPlugins::AddPluginFSTimer(): expired timer was added (" << (int)ti << " ms)");
                    KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS);                // zrusime pripadny Windowsovy timer, uz neni potreba
                    PostMessage(MainWindow->HWindow, WM_TIMER, IDT_PLUGINFSTIMERS, 0); // co nejdrive zpracujeme dalsi timeout
                }
            }
        }
        else
        {
            PluginFSTimers.ResetState();
            delete timer;
        }
    }
    else
        TRACE_E(LOW_MEMORY);
    return ret;
}

int CPlugins::KillPluginFSTimer(CPluginFSInterfaceAbstract* timerOwner, BOOL allTimers, DWORD timerParam)
{
    int ret = 0;
    BOOL setTimer = FALSE;
    int i;
    for (i = PluginFSTimers.Count - 1; i >= 0; i--)
    {
        CPluginFSTimer* timer = PluginFSTimers[i];
        if (timer->TimerOwner == timerOwner && (allTimers || timer->TimerParam == timerParam))
        {
            if (i == 0)
                setTimer = TRUE;
            PluginFSTimers.Delete(i);
            ret++;
        }
    }
    if (setTimer) // byl zrusen timer s nejblizsim timeoutem, musime prenastavit timeout
    {
        if (PluginFSTimers.Count > 0)
        {
            DWORD ti = PluginFSTimers[0]->AbsTimeout - GetTickCount();
            if ((int)ti > 0) // pokud jiz nenastal timeout noveho timeru (muze nastat i zaporny rozdil casu), zmenime nebo nahodime windows timer
            {
                SetTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS, ti, NULL);
            }
            else
            {
                KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS);                // zrusime pripadny Windowsovy timer, uz neni potreba
                PostMessage(MainWindow->HWindow, WM_TIMER, IDT_PLUGINFSTIMERS, 0); // co nejdrive zpracujeme dalsi timeout
            }
        }
        else
            KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS); // zrusime pripadny Windowsovy timer, uz neni potreba
    }
    return ret;
}

void CPlugins::HandlePluginFSTimers()
{
    CALL_STACK_MESSAGE1("CPlugins::HandlePluginFSTimers()");

    // zrusime pripadny windowsovy timer (aby se zbytecne neopakovalo volani)
    KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS);

    if (!StopTimerHandlerRecursion) // obrana proti rekurzi
    {
        StopTimerHandlerRecursion = TRUE;
        DWORD startTimerTimeCounter = TimerTimeCounter;
        DWORD timeNow = GetTickCount();

        int i;
        for (i = 0; i < PluginFSTimers.Count; i++)
        {
            CPluginFSTimer* timer = PluginFSTimers[i];
            if ((int)(timer->AbsTimeout - timeNow) <= 0 &&     // nastal timeout timeru
                timer->TimerAddedTime < startTimerTimeCounter) // jde o "stary" timer (kvuli nekonecnemu cyklu blokujeme nove pridane timery)
            {
                PluginFSTimers.Detach(i--); // odpojime timer z pole (nastal jeho timeout = je "vyrizeny")

                CPluginFSInterfaceEncapsulation* fs = NULL; // najdeme zapouzdreni FS-objektu timeru (FS v panelu nebo odpojeny FS)
                if (MainWindow->LeftPanel->Is(ptPluginFS) &&
                    MainWindow->LeftPanel->GetPluginFS()->Contains(timer->TimerOwner))
                {
                    fs = MainWindow->LeftPanel->GetPluginFS();
                }
                if (fs == NULL && MainWindow->RightPanel->Is(ptPluginFS) &&
                    MainWindow->RightPanel->GetPluginFS()->Contains(timer->TimerOwner))
                {
                    fs = MainWindow->RightPanel->GetPluginFS();
                }
                if (fs == NULL)
                {
                    CDetachedFSList* list = MainWindow->DetachedFSList;
                    int j;
                    for (j = 0; j < list->Count; j++)
                    {
                        CPluginFSInterfaceEncapsulation* detachedFS = list->At(j);
                        if (detachedFS->Contains(timer->TimerOwner))
                        {
                            fs = detachedFS;
                            break;
                        }
                    }
                }
                if (fs == NULL && WorkingPluginFS != NULL && WorkingPluginFS->Contains(timer->TimerOwner))
                    fs = WorkingPluginFS;
                if (fs != NULL)
                {
                    fs->Event(FSE_TIMER, timer->TimerParam); // oznamime vlastnikovi timeru jeho timeout
                    i = -1;                                  // pro pripad zmeny v poli PluginFSTimers to vezmeme zase pekne od zacatku
                }
                else
                    TRACE_E("CPlugins::HandlePluginFSTimers(): timer owner was not found! (" << timer->TimerOwner << ")");
                delete timer;
            }
        }

        if (PluginFSTimers.Count > 0)
        {
            DWORD ti = PluginFSTimers[0]->AbsTimeout - GetTickCount();
            if ((int)ti > 0) // pokud jiz nenastal timeout noveho timeru (muze nastat i zaporny rozdil casu), zmenime nebo nahodime windows timer
                SetTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS, ti, NULL);
            else
                PostMessage(MainWindow->HWindow, WM_TIMER, IDT_PLUGINFSTIMERS, 0); // co nejdrive zpracujeme dalsi timeout
        }

        StopTimerHandlerRecursion = FALSE;
    }
    else
        TRACE_E("Recursive call to CPlugins::HandlePluginFSTimers()!");
}
