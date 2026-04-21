// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_view) // Make structures independent of the current alignment settings.
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
private: // Guard against incorrect direct method calls (see CPluginInterfaceForViewerEncapsulation).
    friend class CPluginInterfaceForViewerEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // Function for the file viewer; called to open the viewer and load a file
    // Parameters 'name', 'left', 'right', 'width', 'height', 'showCmd', and 'alwaysOnTop' are the recommended window placement parameters
    // If 'returnLock' is FALSE, 'lock' and 'lockOwner' have no meaning; if 'returnLock'
    // TRUE, mel by viewer vratit system-event 'lock' v nonsignaled stavu, do signaled stavu 'lock'
    // the 'lock' transitions to the signaled state when viewing of the file 'name' finishes (the file is removed from the temporary directory),
    // the viewer should set 'lockOwner' to TRUE if the 'lock' object is to remain held by the caller
    // FALSE means the viewer releases the 'lock' itself — in that case the viewer must use
    // CSalamanderGeneralAbstract::UnlockFileInCache to transition the 'lock' to the signaled state);
    // If the viewer does not set 'lock' (it remains NULL), the file 'name' is valid only until this ViewFile call returns;
    // if 'viewerData' is not NULL, it passes extended parameters to the viewer (see
    // CSalamanderGeneralAbstract::ViewFileInPluginViewer); 'enumFilesSourceUID' je UID zdroje (panelu
    // or Find window) from which the viewer is opened; if -1, the source is unknown (archives and
    // file systems or Alt+F11, etc.) — see e.g. CSalamanderGeneralAbstract::GetNextFileNameForViewer;
    // 'enumFilesCurrentIndex' is the index of the opened file in the source (panel or Find window); if -1,
    // the source or index is unknown; returns TRUE on success (FALSE indicates failure; 'lock' and
    // 'lockOwner' have no meaning in that case)
    virtual BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                 BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex) = 0;

    // Function for the file viewer; called to check whether the viewer can open and display 'name'. This function must not show dialogs such as "invalid file format" — those dialogs are shown by ViewFile. Return TRUE if the viewer can display 'name' (for example, if the file has a matching signature); return FALSE otherwise. If FALSE is returned, Salamander will try to find another viewer for 'name' using the viewer priority list (see Viewers configuration).
    virtual BOOL WINAPI CanViewFile(const char* name) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_view)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
