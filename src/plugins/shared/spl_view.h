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
    // okna, je-li 'returnLock' FALSE nemaji 'lock'+'lockOwner' zadny vyznam, je-li 'returnLock'
    // TRUE, mel by viewer vratit system-event 'lock' v nonsignaled stavu, do signaled stavu 'lock'
    // becomes signaled when viewing file 'name' ends (the file is removed from the temporary
    // z docasneho adresare), dale by mel vratit v 'lockOwner' TRUE pokud ma byt objekt 'lock' uzavren
    // volajicim (FALSE znamena, ze si viewer 'lock' rusi sam - v tomto pripade viewer musi pro
    // prechod 'lock' do signaled stavu pouzit metodu CSalamanderGeneralAbstract::UnlockFileInCache);
    // pokud viewer nenastavi 'lock' (zustava NULL) je soubor 'name' platny jen do ukonceni volani teto
    // metody ViewFile; neni-li 'viewerData' NULL, jde o predani rozsirenych parametru viewru (viz
    // CSalamanderGeneralAbstract::ViewFileInPluginViewer); 'enumFilesSourceUID' je UID zdroje (panelu
    // or Find window) from which the viewer is opened; if it is -1, the source is unknown (archives
    // file_systemy nebo Alt+F11, atd.) - viz napr. CSalamanderGeneralAbstract::GetNextFileNameForViewer;
    // 'enumFilesCurrentIndex' is the index of the opened file in the source (panel or Find window); if it is -1,
    // neni zdroj nebo index znamy; vraci TRUE pri uspechu (FALSE znamena neuspech, 'lock' a
    // 'lockOwner' have no meaning in that case)
    virtual BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                 BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex) = 0;

    // Called when the viewer is requested to open and load file
    // 'name'; this method should not display any "invalid file format" dialogs; those
    // dialogs are displayed only when the ViewFile method of this interface is called; checks whether
    // file 'name' can be displayed in the viewer (e.g. the file has a matching signature)
    // a pokud je, vraci TRUE; pokud vrati FALSE, zkusi Salamander pro 'name' najit jiny
    // viewer (v prioritnim seznamu vieweru, viz konfiguracni stranka Viewers)
    virtual BOOL WINAPI CanViewFile(const char* name) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_view)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
