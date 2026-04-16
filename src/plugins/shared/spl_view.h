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
#pragma pack(push, enter_include_spl_view) // to make the structures independent of the current packing alignment
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
private: // guard against incorrect direct method calls (see CPluginInterfaceForViewerEncapsulation)
    friend class CPluginInterfaceForViewerEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // Called when the viewer is requested to open and load file
    // 'name'; 'left'+'top'+'width'+'height'+'showCmd'+'alwaysOnTop' is the recommended window placement;
    // window; if 'returnLock' is FALSE, 'lock'+'lockOwner' have no meaning; if 'returnLock' is
    // TRUE, mel by viewer vratit system-event 'lock' v nonsignaled stavu, do signaled stavu 'lock'
    // becomes signaled when viewing file 'name' ends (the file is removed from the temporary
    // directory at that moment); it should also return TRUE in 'lockOwner' if the 'lock' object is to be closed
    // by the caller (FALSE means the viewer closes 'lock' itself - in that case the viewer must use
    // CSalamanderGeneralAbstract::UnlockFileInCache to make 'lock' signaled);
    // if the viewer does not set 'lock' (it remains NULL), file 'name' is valid only until this
    // ViewFile call ends; if 'viewerData' is not NULL, it passes extended viewer parameters (see
    // CSalamanderGeneralAbstract::ViewFileInPluginViewer); 'enumFilesSourceUID' je UID zdroje (panelu
    // or Find window) from which the viewer is opened; if it is -1, the source is unknown (archives
    // or file systems, or Alt+F11, etc.) - see e.g. CSalamanderGeneralAbstract::GetNextFileNameForViewer;
    // 'enumFilesCurrentIndex' is the index of the opened file in the source (panel or Find window); if it is -1,
    // the source or index is unknown; returns TRUE on success (FALSE means failure, 'lock' and
    // 'lockOwner' have no meaning in that case)
    virtual BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                 BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex) = 0;

    // Called when the viewer is requested to open and load file
    // 'name'; this method should not display any "invalid file format" dialogs; such
    // dialogs are shown only when the ViewFile method of this interface is called; this method determines whether
    // file 'name' can be displayed by the viewer (e.g. the file has a matching signature),
    // and if so returns TRUE; if it returns FALSE, Salamander tries to find another
    // viewer for 'name' (in the viewer priority list; see the Viewers configuration page)
    virtual BOOL WINAPI CanViewFile(const char* name) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_view)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
