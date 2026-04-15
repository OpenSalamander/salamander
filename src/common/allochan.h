// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// Installs a handler for out-of-memory conditions during calls to operator new or malloc (also used by calloc, realloc, and others; see help). It guarantees that neither operator new nor malloc returns NULL without notifying the user. It displays an "insufficient memory" message box, and after closing other applications the user can retry the allocation. The user can also terminate the process or let the allocation failure propagate to the application (operator new or malloc then returns NULL; allocations of large memory blocks should be prepared for this, otherwise the application may crash, and the user is warned about that).

// Sets the localized text of the out-of-memory message and warning messages
// (use NULL if a string should remain unchanged); expected contents:
// message:
// Insufficient memory to allocate %u bytes. Try to release some memory (e.g.
// close some running application) and click Retry. If it does not help, you can
// click Ignore to pass memory allocation error to this application or click Abort
// to terminate this application.
// title: (used for both "message" and "warning")
// We recommend using the application name so the user knows which application is reporting the problem
// warningIgnore:
// Do you really want to pass memory allocation error to this application?\n\n
// WARNING: Application may crash and then all unsaved data will be lost!\n
// HINT: We recommend to risk this action only if the application is trying to
// allocate extra large block of memory (i.e. more than 500 MB).
// warningAbort:
// Do you really want to terminate this application?\n\nWARNING: All unsaved data will be lost!
void SetAllocHandlerMessage(const TCHAR* message, const TCHAR* title,
                            const TCHAR* warningIgnore, const TCHAR* warningAbort);
