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
#pragma pack(push, enter_include_spl_menu) // aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

class CSalamanderForOperationsAbstract;

//
// ****************************************************************************
// CSalamanderBuildMenuAbstract
//
// sada metod Salamandera pro stavbu menu pluginu
//
// jde o podmnozinu metod CSalamanderConnectAbstract, metody se stejne chovaji,
// pouzivaji se stejne konstanty, popis viz CSalamanderConnectAbstract

class CSalamanderBuildMenuAbstract
{
public:
    // ikony se zadavaji metodou CSalamanderBuildMenuAbstract::SetIconListForMenu, zbytek
    // popisu viz CSalamanderConnectAbstract::AddMenuItem
    virtual void WINAPI AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                    DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // ikony se zadavaji metodou CSalamanderBuildMenuAbstract::SetIconListForMenu, zbytek
    // popisu viz CSalamanderConnectAbstract::AddSubmenuStart
    virtual void WINAPI AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                        DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // popis viz CSalamanderConnectAbstract::AddSubmenuEnd
    virtual void WINAPI AddSubmenuEnd() = 0;

    // nastavi bitmapu s ikonami pluginu pro menu; bitmapu je treba alokovat pomoci volani
    // CSalamanderGUIAbstract::CreateIconList() a nasledne vytvorit a naplnit pomoci
    // metod CGUIIconListAbstract interfacu; rozmery ikonek musi byt 16x16 bodu;
    // Salamander si objekt bitmapy prebira do sve spravy, plugin ji po zavolani
    // teto funkce nesmi destruovat; Salamander ji drzi jen v pameti, nikam se neuklada
    virtual void WINAPI SetIconListForMenu(CGUIIconListAbstract* iconList) = 0;
};

//
// ****************************************************************************
// CPluginInterfaceForMenuExtAbstract
//

// flagy stavu polozek v menu (pro pluginy rozsireni menu)
#define MENU_ITEM_STATE_ENABLED 0x01 // enablovana, bez tohoto flagu je polozka disablovana
#define MENU_ITEM_STATE_CHECKED 0x02 // pred polozkou je "check" nebo "radio" znacka
#define MENU_ITEM_STATE_RADIO 0x04   // bez MENU_ITEM_STATE_CHECKED se ignoruje, \
                                     // "radio" znacka, bez tohoto flagu "check" znacka
#define MENU_ITEM_STATE_HIDDEN 0x08  // polozka se v menu vubec nema objevit

class CPluginInterfaceForMenuExtAbstract
{
#ifdef INSIDE_SALAMANDER
private: // ochrana proti nespravnemu primemu volani metod (viz CPluginInterfaceForMenuExtEncapsulation)
    friend class CPluginInterfaceForMenuExtEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // vraci stav polozky menu s identifikacnim cislem 'id'; navratova hodnota je kombinaci
    // flagu (viz MENU_ITEM_STATE_XXX); 'eventMask' viz CSalamanderConnectAbstract::AddMenuItem
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) = 0;

    // spousti prikaz menu s identifikacnim cislem 'id', 'eventMask' viz
    // CSalamanderConnectAbstract::AddMenuItem, 'salamander' je sada pouzitelnych metod
    // Salamandera pro provadeni operaci (POZOR: muze byt NULL, viz popis metody
    // CSalamanderGeneralAbstract::PostMenuExtCommand), 'parent' je parent messageboxu,
    // vraci TRUE pokud ma byt v panelu zruseno oznaceni (nebyl pouzit Cancel, mohl byt
    // pouzit Skip), jinak vraci FALSE (neprovede se odznaceni);
    // POZOR: Pokud prikaz zpusobi zmeny na nejake ceste (diskove/FS), mel by pouzit
    //        CSalamanderGeneralAbstract::PostChangeOnPathNotification pro informovani
    //        panelu bez automatickeho refreshe a otevrene FS (aktivni i odpojene)
    // POZNAMKA: pokud prikaz pracuje se soubory/adresari z cesty v aktualnim panelu nebo
    //           i primo s touto cestou, je treba volat
    //           CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath pro aktualni panel,
    //           jinak nebude cesta v tomto panelu vlozena do seznamu pracovnich
    //           adresaru - List of Working Directories (Alt+F12)
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask) = 0;

    // zobrazi napovedu pro prikaz menu s identifikacnim cislem 'id' (user stiskne Shift+F1,
    // najde v menu Plugins menu tohoto pluginu a vybere z nej prikaz), 'parent' je parent
    // messageboxu, vraci TRUE pokud byla zobrazena nejaka napoveda, jinak se zobrazi z helpu
    // Salamandera kapitola "Using Plugins"
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id) = 0;

    // funkce pro "dynamic menu extension", vola se jen pokud zadate FUNCTION_DYNAMICMENUEXT do
    // SetBasicPluginData; sestavi menu pluginu pri jeho loadu, a pak vzdy znovu tesne pred
    // jeho otevrenim v menu Plugins nebo na Plugin bare (navic i pred otevrenim okna Keyboard
    // Shortcuts z Plugins Manageru); prikazy v novem menu by mely mit stejne ID jako ve starem,
    // aby jim zustavaly uzivatelem pridelene hotkeys a aby pripadne fungovaly jako posledni
    // pouzity prikaz (viz Plugins / Last Command); 'parent' je parent messageboxu, 'salamander'
    // je sada metod pro stavbu menu
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_menu)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
