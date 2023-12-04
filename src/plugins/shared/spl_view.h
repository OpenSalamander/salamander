// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_view) // aby byly struktury nezavisle na nastavenem zarovnavani
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
private: // ochrana proti nespravnemu primemu volani metod (viz CPluginInterfaceForViewerEncapsulation)
    friend class CPluginInterfaceForViewerEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // funkce pro "file viewer", vola se pri pozadavku na otevreni viewru a nacteni souboru
    // 'name', 'left'+'right'+'width'+'height'+'showCmd'+'alwaysOnTop' je doporucene umisteni
    // okna, je-li 'returnLock' FALSE nemaji 'lock'+'lockOwner' zadny vyznam, je-li 'returnLock'
    // TRUE, mel by viewer vratit system-event 'lock' v nonsignaled stavu, do signaled stavu 'lock'
    // prejde v okamziku ukonceni prohlizeni souboru 'name' (soubor je v tomto okamziku odstranen
    // z docasneho adresare), dale by mel vratit v 'lockOwner' TRUE pokud ma byt objekt 'lock' uzavren
    // volajicim (FALSE znamena, ze si viewer 'lock' rusi sam - v tomto pripade viewer musi pro
    // prechod 'lock' do signaled stavu pouzit metodu CSalamanderGeneralAbstract::UnlockFileInCache);
    // pokud viewer nenastavi 'lock' (zustava NULL) je soubor 'name' platny jen do ukonceni volani teto
    // metody ViewFile; neni-li 'viewerData' NULL, jde o predani rozsirenych parametru viewru (viz
    // CSalamanderGeneralAbstract::ViewFileInPluginViewer); 'enumFilesSourceUID' je UID zdroje (panelu
    // nebo Find okna), ze ktereho je viewer otviran, je-li -1, je zdroj neznamy (archivy a
    // file_systemy nebo Alt+F11, atd.) - viz napr. CSalamanderGeneralAbstract::GetNextFileNameForViewer;
    // 'enumFilesCurrentIndex' je index oteviraneho souboru ve zdroji (panelu nebo Find okne), je-li -1,
    // neni zdroj nebo index znamy; vraci TRUE pri uspechu (FALSE znamena neuspech, 'lock' a
    // 'lockOwner' v tomto pripade nemaji zadny vyznam)
    virtual BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                 BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex) = 0;

    // funkce pro "file viewer", vola se pri pozadavku na otevreni viewru a nacteni souboru
    // 'name'; tato fuknce by nemela zobrazovat zadna okna typu "invalid file format", tato
    // okna se zobrazi az pri volani metody ViewFile tohoto rozhrani; zjisti jestli je
    // soubor 'name' zobrazitelny (napr. soubor ma odpovidajici signaturu) ve vieweru
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
