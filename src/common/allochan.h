// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Installs a handler for out-of-memory failures in operator new and malloc
// (and in calloc, realloc, and related functions). It ensures those calls never
// return NULL silently: the user sees an "Insufficient memory" message box and
// can retry after closing other applications. The user may also terminate the
// process or let the allocation failure propagate back to the application.

// Sets localized text for the out-of-memory and warning messages
// (pass NULL to leave a string unchanged); expected contents:
// message:
// Insufficient memory to allocate %u bytes. Try to release some memory (e.g.
// close some running application) and click Retry. If it does not help, you can
// click Ignore to pass memory allocation error to this application or click Abort
// to terminate this application.
// title: (used for both "message" and "warning")
// We recommend using the application name so the user knows which program reported the problem.
// warningIgnore:
// Do you really want to pass memory allocation error to this application?\n\n
// WARNING: Application may crash and then all unsaved data will be lost!\n
// HINT: We recommend to risk this action only if the application is trying to
// allocate extra large block of memory (i.e. more than 500 MB).
// warningAbort:
// Do you really want to terminate this application?\n\nWARNING: All unsaved data will be lost!
void SetAllocHandlerMessage(const TCHAR* message, const TCHAR* title,
                            const TCHAR* warningIgnore, const TCHAR* warningAbort);
