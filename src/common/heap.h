// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// Define _CRTDBG_MAP_ALLOC in the DEBUG build; otherwise the leak source is not shown.

#if defined(_DEBUG) && !defined(HEAP_DISABLE)

#define GCHEAP_MAX_USED_MODULES 100 // maximum number of modules to remember so they can be loaded before reporting memory leaks

// Call this for modules that may report memory leaks. If leaks are detected, all modules registered this way are loaded "as image" (without module initialization; the modules are already unloaded when leaks are checked) before the leak report is printed. This shows .cpp module names instead of "#File Error#" messages, and MSVC does not flood the output with generated exceptions because the module names are available. Can be called from any thread.
void AddModuleWithPossibleMemoryLeaks(const TCHAR* fileName);

#endif // defined(_DEBUG) && !defined(HEAP_DISABLE)
