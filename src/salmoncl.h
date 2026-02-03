// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// SalmonClient
// The SALMON.EXE module is used for out-of-process minidump generation, packaging, and upload them to the server.
// SALMON must run from Salamander start-up onward to react to crashes. Crashes that happen before SALMON starts
// will be handled silently and processed by SALMON "next time".
//
// This header is shared between the SALMON and SALAMAND projects because they communicate through shared memory.
//
// out of process minidumps
// http://www.nynaeve.net/?p=128
// http://social.msdn.microsoft.com/Forums/en-US/windbg/thread/2dfd711f-e81e-466f-a566-4605e78075f6
//
// http://www.voyce.com/index.php/2008/06/11/creating-a-featherweight-debugger/
// http://social.msdn.microsoft.com/Forums/en-US/windbg/thread/2dfd711f-e81e-466f-a566-4605e78075f6
// http://social.msdn.microsoft.com/Forums/en-US/vsdebug/thread/b290b7bd-1ec8-4302-8e3a-8ee0dc134683/
// http://www.ms-news.info/f3682/minidumpwritedump-fails-after-writing-partial-dump-access-denied-1843614.html
//
// debugging handles
// http://www.codeproject.com/Articles/6988/Debug-Tutorial-Part-5-Handle-Leaks

#define SALMON_FILEMAPPIN_NAME_SIZE 20

// the x64 and x86 versions of Salamander/Salmon are not compatible
#ifdef _WIN64
#define SALMON_SHARED_MEMORY_VERSION_PLATFORM 0x10000000
#else
#define SALMON_SHARED_MEMORY_VERSION_PLATFORM 0x00000000
#endif
#define SALMON_SHARED_MEMORY_VERSION (SALMON_SHARED_MEMORY_VERSION_PLATFORM | 4)

#pragma pack(push)
#pragma pack(4)
struct CSalmonSharedMemory
{
    DWORD Version;           // SALMON_SHARED_MEMORY_VERSION (if it does not match for SALAM/SALMON, shout and refuse to communicate...)
    HANDLE Process;          // handle of the parent process (lets us wait for it to terminate); intentionally leaked
    DWORD ProcessId;         // ID of the crashed parent process
    DWORD ThreadId;          // ID of the crashed thread
    HANDLE Fire;             // AS signals to SALMON that it should send reports
    HANDLE Done;             // SALMON reports back to AS that it is finished
    HANDLE SetSLG;           // AS signals to SALMON that it should load the SLG stored in the SLGName buffer which is set before the event is signaled
    HANDLE CheckBugs;        // AS signals to SALMON that it should check the directory with bug reports and, if it finds any (from some previous crash), offer to upload them
    char SLGName[MAX_PATH];  // valid at the moment AS signals SetSLG and indicates which SLG should be loaded
    char BugPath[MAX_PATH];  // set by Salamander; specifies the path where bug reports will be stored (the path does not have to exist; it is created only on crash)
    char BugName[MAX_PATH];  // set by Salamander; specifies the internal name of the minidump/bug report file
    char BaseName[MAX_PATH]; // set by Salmon; constructed as "UID-BugName-DATE-TIME"; ".DMP" is appended for a minidumps
    DWORD64 UID;             // unique machine ID created by XORing the GUID; stored in the registry under the Bug Reporter key; set by Salamander, Salmon only reads it and inserts it into the bug report name

    // pass EXCEPTION_POINTERS piece by piece; set before setting the Fire event
    EXCEPTION_RECORD ExceptionRecord;
    CONTEXT ContextRecord;
};
#pragma pack(pop)

//-----------------------------------------------------------------------
#ifdef INSIDE_SALAMANDER

BOOL SalmonInit();
void SalmonSetSLG(const char* slgName); // sets the language in salmon
void SalmonCheckBugs();

// stores the exception info in shared memory and asks Salmon to create a minidump; then waits for it to finish
// returns TRUE if successful, FALSE if Salmon could not be invoked for some reason
BOOL SalmonFireAndWait(const EXCEPTION_POINTERS* e, char* bugReportPath);

#endif //INSIDE_SALAMANDER
