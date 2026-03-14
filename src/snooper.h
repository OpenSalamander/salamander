    // SPDX-FileCopyrightText: 2023 Open Salamander Authors
    // SPDX-License-Identifier: GPL-2.0-or-later
    // CommentsTranslationProject: TRANSLATED

    #pragma once

    class CFilesWindow;

    extern HANDLE RefreshFinishedEvent;
    extern int SnooperSuspended;

    void AddDirectory(CFilesWindow* win, const char* path, BOOL registerDevNotification);                           // add a new directory for the snooper
    void ChangeDirectory(CFilesWindow* win, const char* newPath, BOOL registerDevNotification);                     // change of the specified directory
    void DetachDirectory(CFilesWindow* win, BOOL waitForHandleClosure = FALSE, BOOL closeDevNotifification = TRUE); // no need to search anymore

    BOOL InitializeThread();
    void TerminateThread();

    void BeginSuspendMode(BOOL debugDoNotTestCaller = FALSE);
    void EndSuspendMode(BOOL debugDoNotTestCaller = FALSE);

    typedef TDirectArray<CFilesWindow*> CWindowArray; // (CFilesWindow *)
    typedef TDirectArray<HANDLE> CObjectArray;        // (HANDLE)

    extern CWindowArray WindowArray; // arrays share the same indices
    extern CObjectArray ObjectArray; // the object handle associated with the MainWindow
