// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <crtdbg.h>
#include <ostream>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#if defined(_DEBUG) && !defined(HEAP_DISABLE)

#include "heap.h"
#include "lstrfix.h"

// The order here is important.
// Section names must be 8 characters or less.
// The sections with the same name before the $
// are merged into one section. The order that
// they are merged is determined by sorting
// the characters after the $.
// i_heap and i_heap_end are used to set
// boundaries so we can find the real functions
// that we need to call for initialization.

#pragma warning(disable : 4075) // chceme definovat poradi inicializace modulu

typedef void(__cdecl* _PVFV)(void);

#pragma section(".i_hea$a", read)
__declspec(allocate(".i_hea$a")) const _PVFV i_heap = (_PVFV)1; // na zacatek sekce .i_hea si dame promennou i_heap

#pragma section(".i_hea$z", read)
__declspec(allocate(".i_hea$z")) const _PVFV i_heap_end = (_PVFV)1; // a na konec sekce .i_hea si dame promennou i_heap_end

void Initialize__Heap()
{
    const _PVFV* x = &i_heap;
    for (++x; x < &i_heap_end; ++x)
        if (*x != NULL)
            (*x)();
}

#pragma init_seg(".i_hea$m")

#include "trace.h"

int OurReportingFunction(int reportType, char* userMessage, int* retVal)
{
    const char* rType = "Unknown";

    if (reportType == _CRT_WARN)
        rType = "_CRT_WARN";
    else if (reportType == _CRT_ERROR)
        rType = "_CRT_ERROR";
    else if (reportType == _CRT_ASSERT)
        rType = "_CRT_ASSERT";

    TRACE_E(rType << ": " << userMessage);

    // By setting retVal to zero, we are instructing _CrtDbgReport
    // to continue with normal execution after generating the report.
    // If we wanted _CrtDbgReport to start the debugger, we would set
    // retVal to one.
    *retVal = 0;

    // we'll report some information, but we also
    // want _CrtDbgReport to get called - so we'll return FALSE
    return FALSE;
}

class C__GCHeapInit
{
public:
    C__GCHeapInit()
    {
        // uloz stav pameti na zacatku
        _CrtMemCheckpoint(&start_state);
        prev_reporting_hook = _CrtSetReportHook(OurReportingFunction);
        InitializeCriticalSection(&CriticalSection);
        UsedModulesCount = 0;
    }
    ~C__GCHeapInit()
    {
        // zjisti aktualni stav pameti
        _CrtMemState end_state;
        _CrtMemCheckpoint(&end_state);

        // zkontroluj, jestli jsou nejake leaky
        _CrtMemState diff;
        if (_CrtMemDifference(&diff, &start_state, &end_state))
        {
            HMODULE hUsedModules[GCHEAP_MAX_USED_MODULES];
            // namapuju do pameti vsechny moduly, ve kterych se muzou hlasit memory leaky,
            // tim se v reportu zobrazi jmena .cpp souboru (jinak by tam bylo jen "#File Error#")
            for (int i = 0; i < UsedModulesCount; i++)
                hUsedModules[i] = LoadLibraryEx(UsedModules[i], NULL, DONT_RESOLVE_DLL_REFERENCES);

            // vypise vsechny neuvolnene bloky
            _CrtMemDumpAllObjectsSince(&start_state);

            // kdyz uz mame diff, tak ho taky vypisem
            _CrtMemDumpStatistics(&diff);

            // zase uvolnime namapovane moduly
            for (int i = 0; i < UsedModulesCount; i++)
                FreeLibrary(hUsedModules[i]);

            // vyhod warning messagebox
            MSG msg; // remove possibly buffered ESC key (not to close msgbox immediately)
            while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                ;
            MessageBoxA(NULL, "Detected memory leaks!", "Heap Message",
                        MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL);
        }
        _CrtSetReportHook(prev_reporting_hook);
        DeleteCriticalSection(&CriticalSection);
    }
    void AddUsedModule(const TCHAR* fileName)
    {
        EnterCriticalSection(&CriticalSection);
        if (UsedModulesCount < GCHEAP_MAX_USED_MODULES)
        {
            for (int i = 0; i < UsedModulesCount; i++)
            {
                if (lstrcmpi(UsedModules[i], fileName) == 0)
                {
                    LeaveCriticalSection(&CriticalSection);
                    return;
                }
            }
            lstrcpyn(UsedModules[UsedModulesCount++], fileName, MAX_PATH);
        }
        else
            TRACE_E("C__GCHeapInit::AddUsedModule(): insufficient reserve for module names, please enlarge GCHEAP_MAX_USED_MODULES!");
        LeaveCriticalSection(&CriticalSection);
    }

private:
    _CrtMemState start_state;
    _CRT_REPORT_HOOK prev_reporting_hook;
    CRITICAL_SECTION CriticalSection;
    TCHAR UsedModules[GCHEAP_MAX_USED_MODULES][MAX_PATH];
    int UsedModulesCount;
} __GCHeapCheckLeaks;

void AddModuleWithPossibleMemoryLeaks(const TCHAR* fileName)
{
    __GCHeapCheckLeaks.AddUsedModule(fileName);
}

#endif // defined(_DEBUG) && !defined(HEAP_DISABLE)
