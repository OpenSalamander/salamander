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

    // In order to later eliminate non-existing plugins, we will set a helper variable
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
        if (foundIndex == -1) // this plugin does not have a record in the Order array => we will append it to the end
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

    // remove plugins that no longer exist
    for (i = Order.Count - 1; i >= 0; i--)
    {
        if (Order[i].Flags == 0)
        {
            free(Order[i].DLLName);
            Order.Delete(i);
        }
    }

    // find the first newly added plugin
    int index = -1;
    for (i = 0; i < Order.Count; i++)
    {
        if (Order[i].Flags == 2)
        {
            index = i;
            break;
        }
    }

    // if we have found a newly added plugin and there are others behind it, we will sort them
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

    // store the item from the source index into 'tmp'
    CPluginOrder tmp;
    tmp = Order[index];

    // move item between
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

    // store 'tmp' in the destination
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

    // All times must be related to the nearest timeout, because that's the only way it will succeed
    // sort timeouts that exceed 0xFFFFFFFF
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
                ;     // return the index for the last same timer
            return m; // found
        }
        else if (actTimeoutAbs > timeoutAbs)
        {
            if (l == r || l > m - 1)
                return m; // not found, should be at this position
            r = m - 1;
        }
        else
        {
            if (l == r)
                return m + 1; // not found, should be after this position
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

            if (index == 0) // change the nearest timeout, we need to change the Windows timer
            {
                DWORD ti = timer->AbsTimeout - GetTickCount();
                if ((int)ti > 0) // if the timeout of the new timer has not yet occurred (negative time difference is also possible), we change or set a Windows timer
                {
                    SetTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS, ti, NULL);
                }
                else
                {
                    if ((int)ti < 0)
                        TRACE_E("CPlugins::AddPluginFSTimer(): expired timer was added (" << (int)ti << " ms)");
                    KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS);                // we will cancel the possible Windows timer, it is no longer needed
                    PostMessage(MainWindow->HWindow, WM_TIMER, IDT_PLUGINFSTIMERS, 0); // process the next timeout as soon as possible
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
    if (setTimer) // the timer with the closest timeout has been canceled, we need to reset the timeout
    {
        if (PluginFSTimers.Count > 0)
        {
            DWORD ti = PluginFSTimers[0]->AbsTimeout - GetTickCount();
            if ((int)ti > 0) // if the timeout of the new timer has not yet occurred (negative time difference is also possible), we change or set a Windows timer
            {
                SetTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS, ti, NULL);
            }
            else
            {
                KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS);                // we will cancel the possible Windows timer, it is no longer needed
                PostMessage(MainWindow->HWindow, WM_TIMER, IDT_PLUGINFSTIMERS, 0); // process the next timeout as soon as possible
            }
        }
        else
            KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS); // we will cancel the possible Windows timer, it is no longer needed
    }
    return ret;
}

void CPlugins::HandlePluginFSTimers()
{
    CALL_STACK_MESSAGE1("CPlugins::HandlePluginFSTimers()");

    // we will cancel any Windows timer (to avoid unnecessary repeated calls)
    KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS);

    if (!StopTimerHandlerRecursion) // Defense against recursion
    {
        StopTimerHandlerRecursion = TRUE;
        DWORD startTimerTimeCounter = TimerTimeCounter;
        DWORD timeNow = GetTickCount();

        int i;
        for (i = 0; i < PluginFSTimers.Count; i++)
        {
            CPluginFSTimer* timer = PluginFSTimers[i];
            if ((int)(timer->AbsTimeout - timeNow) <= 0 &&     // timeout timer has occurred
                timer->TimerAddedTime < startTimerTimeCounter) // It's about an "old" timer (we are blocking newly added timers due to an infinite loop)
            {
                PluginFSTimers.Detach(i--); // disconnect the timer from the array (its timeout has occurred = it is "handled")

                CPluginFSInterfaceEncapsulation* fs = NULL; // we find encapsulation of the FS-timer object (FS in the panel or disconnected FS)
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
                    fs->Event(FSE_TIMER, timer->TimerParam); // notify the owner of the timer about its timeout
                    i = -1;                                  // in case of a change in the PluginFSTimers array, we will start over again
                }
                else
                    TRACE_E("CPlugins::HandlePluginFSTimers(): timer owner was not found! (" << timer->TimerOwner << ")");
                delete timer;
            }
        }

        if (PluginFSTimers.Count > 0)
        {
            DWORD ti = PluginFSTimers[0]->AbsTimeout - GetTickCount();
            if ((int)ti > 0) // if the timeout of the new timer has not yet occurred (negative time difference is also possible), we change or set a Windows timer
                SetTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS, ti, NULL);
            else
                PostMessage(MainWindow->HWindow, WM_TIMER, IDT_PLUGINFSTIMERS, 0); // process the next timeout as soon as possible
        }

        StopTimerHandlerRecursion = FALSE;
    }
    else
        TRACE_E("Recursive call to CPlugins::HandlePluginFSTimers()!");
}
