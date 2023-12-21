// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED
// 
//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_view) // so that structures are independent of the set alignment
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

struct CSalamanderPluginViewerData;

//
// ****************************************************************************
// CPluginInterfaceForViewerAbstract
//

class CPluginInterfaceForViewerAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect calling of methods (see CPluginInterfaceForViewerEncapsulation)
    friend class CPluginInterfaceForViewerEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // fuction for "file viewer", called during the request for opening viewer and loading file
    // 'name', 'left'+'right'+'width'+'height'+'showCmd'+'alwaysOnTop' is recommended position
    // of window, if 'returnLock' is FALSE, 'lock'+'lockOwner' have no meaning, if 'returnLock'
    // is TRUE, viewer should return system-event 'lock' in nonsignaled state, 'lock' should
    // change to signaled state when viewer finishes viewing of file 'name' (file is deleted
    // from temporary directory at this moment), 'lockOwner' should be TRUE if 'lock' should
    // be closed by caller (FALSE means that viewer closes 'lock' itself - in this case viewer
    // must use method CSalamanderGeneralAbstract::UnlockFileInCache to change 'lock' to
    // signaled state); if viewer does not set 'lock' (remains NULL), file 'name' is valid
    // only until the end of calling of this method ViewFile; if 'viewerData' is not NULL,
    // it is a transfer of extended parameters of viewer (see
    // CSalamanderGeneralAbstract::ViewFileInPluginViewer); 'enumFilesSourceUID' is UID of
    // source (panel or Find window) from which viewer is opened, if -1, source is unknown
    // (archives and file_systems or Alt+F11, etc.) - see e.g.
    // CSalamanderGeneralAbstract::GetNextFileNameForViewer; 'enumFilesCurrentIndex' is index
    // of opened file in source (panel or Find window), if -1, source or index is unknown;
    // returns TRUE on success (FALSE means failure, 'lock' and 'lockOwner' have no meaning
    // in this case)
    virtual BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                 BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex) = 0;

    // function for "file viewer", called during the request for opening viewer and loading
    // file 'name'; this function should not display any windows like "invalid file format",
    // these windows are displayed only when calling method ViewFile of this interface;
    // determines whether file 'name' is viewable (e.g. file has appropriate signature) in
    // viewer and if it is, returns TRUE; if returns FALSE, Salamander tries to find another
    // viewer for 'name' (in the priority list of viewers, see configuration page Viewers)
    virtual BOOL WINAPI CanViewFile(const char* name) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_view)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
