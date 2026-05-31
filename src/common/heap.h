// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Define _CRTDBG_MAP_ALLOC in debug builds, otherwise leak reports will not show the source location.

#if defined(_DEBUG) && !defined(HEAP_DISABLE)

#define GCHEAP_MAX_USED_MODULES 100 // maximum number of modules remembered so they can be reloaded before printing leak reports

// Register modules that may report memory leaks. If leaks are detected, all
// registered modules are reloaded "as image" (without running initialization)
// before the leak report is printed, because they are already unloaded then. That preserves .cpp module names instead
// of "#File Error#" messages and avoids noisy MSVC-generated exceptions.
// Safe to call from any thread.
void AddModuleWithPossibleMemoryLeaks(const TCHAR* fileName);

#endif // defined(_DEBUG) && !defined(HEAP_DISABLE)
