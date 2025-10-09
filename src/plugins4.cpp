// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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

    // initialize a helper flag so we can later remove plugins that no longer exist
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
        if (foundIndex == -1) // this plugin has no record in the Order array => append it to the end
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

    // if we found a newly added plugin and there are more after it, sort them
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

    // store the item from the source index in 'tmp'
    CPluginOrder tmp;
    tmp = Order[index];

    // shift the items in between
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

    // store 'tmp' into the destination
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

    // all times must refer to the nearest timeout, because only then
    // can we sort timeouts that exceed 0xFFFFFFFF
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
                ;     // return the index after the last identical timer
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
                return m + 1; // not found, should be right after this position
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

            if (index == 0) // the nearest timeout changed, adjust the Windows timer
            {
                DWORD ti = timer->AbsTimeout - GetTickCount();
                if ((int)ti > 0) // if the new timer hasn't already timed out (the difference can even be negative), adjust or start the Windows timer
                {
                    SetTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS, ti, NULL);
                }
                else
                {
                    if ((int)ti < 0)
                        TRACE_E("CPlugins::AddPluginFSTimer(): expired timer was added (" << (int)ti << " ms)");
                    KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS);                // cancel any Windows timer, it's not needed
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
    if (setTimer) // the timer with the nearest timeout was cancelled, the timeout must be reset
    {
        if (PluginFSTimers.Count > 0)
        {
            DWORD ti = PluginFSTimers[0]->AbsTimeout - GetTickCount();
            if ((int)ti > 0) // if the new timer hasn't already timed out (the difference can even be negative), adjust or start the Windows timer
            {
                SetTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS, ti, NULL);
            }
            else
            {
                KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS);                // cancel any Windows timer, it's not needed
                PostMessage(MainWindow->HWindow, WM_TIMER, IDT_PLUGINFSTIMERS, 0); // process the next timeout as soon as possible
            }
        }
        else
            KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS); // cancel any Windows timer, it's no longer needed
    }
    return ret;
}

void CPlugins::HandlePluginFSTimers()
{
    CALL_STACK_MESSAGE1("CPlugins::HandlePluginFSTimers()");

    // cancel any Windows timer (to avoid repeated calls)
    KillTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS);

    if (!StopTimerHandlerRecursion) // protection against recursion
    {
        StopTimerHandlerRecursion = TRUE;
        DWORD startTimerTimeCounter = TimerTimeCounter;
        DWORD timeNow = GetTickCount();

        int i;
        for (i = 0; i < PluginFSTimers.Count; i++)
        {
            CPluginFSTimer* timer = PluginFSTimers[i];
            if ((int)(timer->AbsTimeout - timeNow) <= 0 &&     // the timer timed out
                timer->TimerAddedTime < startTimerTimeCounter) // it's an "old" timer (to prevent an infinite loop, we block newly added timers)
            {
                PluginFSTimers.Detach(i--); // detach the timer from the array (its timeout occurred = it's "handled")

                CPluginFSInterfaceEncapsulation* fs = NULL; // find the encapsulation of the timer's FS object (FS in a panel or a detached FS)
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
                    fs->Event(FSE_TIMER, timer->TimerParam); // notify the timer owner about its timeout
                    i = -1;                                  // in the case of changes in the PluginFSTimers array, start again from the beginning
                }
                else
                    TRACE_E("CPlugins::HandlePluginFSTimers(): timer owner was not found! (" << timer->TimerOwner << ")");
                delete timer;
            }
        }

        if (PluginFSTimers.Count > 0)
        {
            DWORD ti = PluginFSTimers[0]->AbsTimeout - GetTickCount();
            if ((int)ti > 0) // if the new timer hasn't already timed out (the difference can even be negative), adjust or start the Windows timer
                SetTimer(MainWindow->HWindow, IDT_PLUGINFSTIMERS, ti, NULL);
            else
                PostMessage(MainWindow->HWindow, WM_TIMER, IDT_PLUGINFSTIMERS, 0); // process the next timeout as soon as possible
        }

        StopTimerHandlerRecursion = FALSE;
    }
    else
        TRACE_E("Recursive call to CPlugins::HandlePluginFSTimers()!");
}
