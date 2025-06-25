// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED
#pragma once

// OLESPY is used for detecting COM/OLE leaks.
// The bodies of the followed functions are empty in the release version
// (if _DEBUG is not defined).
//
// Details on finding OLE leaks are described in OLESPY.CPP.

// Attaches our IMallocSpy to OLE; COM must be initialized first.
// If it returns TRUE, the followed functions can be called.
BOOL OleSpyRegister();

// Detaches the Spy from OLE; OleSpyDump can still be called after this function.
void OleSpyRevoke();

// Used to break the application upon reaching the 'alloc' allocation.
// Call anytime between OleSpyRegister and OleSpyRevoke.
void OleSpySetBreak(int alloc);

// Prints statistics and leaks to the debugger's Debug window and to TRACE_I.
// For leaks, it displays the allocation order in [n], which can be used for OleSpySetBreak.
void OleSpyDump();

// Stress test of the IMallocSpy implementation.
// Intended for debugging purposes.
// void OleSpyStressTest();
