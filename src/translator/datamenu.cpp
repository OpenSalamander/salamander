// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndout.h"
#include "datarh.h"
#include "config.h"

//*****************************************************************************
//
// CMenuData
//

CMenuData::CMenuData()
    : Items(10, 10)
{
    ID = 0;
    TLangID = 0;
    ConflictGroupsCount = 0;
    IsEX = FALSE;
}

CMenuData::~CMenuData()
{
    for (int i = 0; i < Items.Count; i++)
    {
        if (Items[i].OString != NULL)
        {
            free(Items[i].OString);
            Items[i].OString = NULL;
        }
        if (Items[i].TString != NULL)
        {
            free(Items[i].TString);
            Items[i].TString = NULL;
        }
    }
}

int CMenuData::FindItemIndex(WORD id)
{
    for (int i = 0; i < Items.Count; i++)
    {
        if (Items[i].ID == id)
            return i;
    }
    return -1;
}

#define MENU_ITEM_TYPE(flags) ((flags) & (MF_STRING | MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR))
#define IS_STRING_ITEM(flags) (MENU_ITEM_TYPE((flags)) == MF_STRING)

BOOL CMenuData::LoadMenu(LPCSTR original, LPCSTR translated, CData* data)
{
    int level = 0;
    int conflictGroupMax = 0; // nejvyssi cislo group
    TDirectArray<int> conflictGroupStack(100, 100);
    TDirectArray<DWORD> popupFlagsStack(100, 100); // slouzi pro detekci konciciho popupu
    WORD oID;
    WORD tID;
    char errBuf[3000];

    // ukazka obsahu ConflictGroup pro rozvetvene menu
    // MENU  [ConflictGroup]
    // ---------------------
    // AAAA  [0]
    //  A1   [1]
    //   AA1 [2]
    //   AA2 [2]
    //   AA3 [2]
    //  A2   [1]
    // BBBB  [0]
    //  B1   [3]
    //   BB1 [4]
    //  B2   [3]
    //   BB2 [5]
    //  B3   [3]
    //   BB1 [6]
    // CCCC  [0]
    conflictGroupStack.Add(conflictGroupMax); // top-level group

    do
    {
        oID = 0;
        tID = 0;
        WORD oFlags = GET_WORD(original);
        WORD tFlags = GET_WORD(translated);
        original += sizeof(WORD);
        translated += sizeof(WORD);
        if (!(oFlags & MF_POPUP))
        {
            oID = GET_WORD(original);
            tID = GET_WORD(translated);
        }
        if (oFlags != tFlags && // neshoda flagu vyjma ruzne zapsanych separatoru (prazdny string nebo pres MF_SEPARATOR, jako je to v german menu automation)
            (oFlags != 0 || MENU_ITEM_TYPE(tFlags) != MF_SEPARATOR ||
             wcslen((LPCWSTR)(original + sizeof(WORD))) != 0) &&
            (tFlags != 0 || MENU_ITEM_TYPE(oFlags) != MF_SEPARATOR ||
             wcslen((LPCWSTR)(translated + sizeof(WORD))) != 0))
        {
            sprintf_s(errBuf, "Original and translated menu item has different flags.\n\n"
                              "Original menu item ID: %s\n"
                              "Translated menu item ID: %s",
                      oID == 0 ? "POPUP" : DataRH.GetIdentifier(oID),
                      tID == 0 ? "POPUP" : DataRH.GetIdentifier(tID));
            MessageBox(GetMsgParent(), errBuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }

        LPCWSTR oStr = NULL;
        LPCWSTR tStr = NULL;

        if (!(oFlags & MF_POPUP))
        {
            original += sizeof(WORD);
            translated += sizeof(WORD);
        }

        if (oID != tID)
        {
            sprintf_s(errBuf, "Original and translated menu item has different command ID.\n\n"
                              "Original menu item ID: %s\n"
                              "Translated menu item ID: %s",
                      oID == 0 ? "POPUP" : DataRH.GetIdentifier(oID),
                      tID == 0 ? "POPUP" : DataRH.GetIdentifier(tID));
            MessageBox(GetMsgParent(), errBuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        // umime jen texty a separatory (zapsane jako prazdne stringy i pres MF_SEPARATOR, jako je to v german menu automation)
        if (!IS_STRING_ITEM(oFlags) && MENU_ITEM_TYPE(oFlags) != MF_SEPARATOR)
        {
            sprintf_s(errBuf, "Original menu item has not string nor is separator.\n\n"
                              "Original menu item ID: %s",
                      oID == 0 ? "POPUP" : DataRH.GetIdentifier(oID));
            MessageBox(GetMsgParent(), errBuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        if (!IS_STRING_ITEM(tFlags) && MENU_ITEM_TYPE(tFlags) != MF_SEPARATOR)
        {
            sprintf_s(errBuf, "Translated menu item has not string nor is separator.\n\n"
                              "Translated menu item ID: %s",
                      tID == 0 ? "POPUP" : DataRH.GetIdentifier(tID));
            MessageBox(GetMsgParent(), errBuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }

        oStr = (LPCWSTR)original;
        tStr = (LPCWSTR)translated;

        int oLen = wcslen((LPCWSTR)original) + 1;
        int tLen = wcslen((LPCWSTR)translated) + 1;

        original += oLen * sizeof(WCHAR);
        translated += tLen * sizeof(WCHAR);

        CMenuItem item;
        ZeroMemory(&item, sizeof(item));

        if (!DecodeString(oStr, oLen, &item.OString))
            return FALSE;
        if (!DecodeString(tStr, tLen, &item.TString))
            return FALSE;

        item.ID = oID;
        item.Flags = oFlags;
        if (oLen == 1)
            item.State = PROGRESS_STATE_TRANSLATED; // separator, nebude zobrazen, prazdny retezec je "prelozeny"
        else
            item.State = data->QueryTranslationState(tteMenus, Items.Count, ID, item.OString, item.TString);
        item.Level = level;
        item.ConflictGroup = conflictGroupStack[conflictGroupStack.Count - 1];

        Items.Add(item);
        if (!Items.IsGood())
        {
            return FALSE;
        }

        // POZOR -- polozka muze mit zaroven MF_POPUP i MF_END v pripade popupu, ktere jsou posledni ve svem levelu
        // prazdny popup nemuze existovat (kompiler rve chybu)
        // popup, ktery na svem levelu za sebou nema dalsi polozku ma flag (MF_POPUP | MF_END)
        // popup, za kterym jeste neco nasleduje ma flagy MF_POPUP
        // polozka, ktera neni posledni v popupu nema flag MF_END
        // polozka, ktera je posledni v popupu ma flag MF_END
        if (oFlags & MF_POPUP)
        {
            level++;
            conflictGroupMax++;
            conflictGroupStack.Add(conflictGroupMax);

            popupFlagsStack.Add(oFlags);
        }
        if (!(oFlags & MF_POPUP) && (oFlags & MF_END))
        {
            // jde o polozku, ktera neni popup a zaroven je v popupu posledni
            DWORD popupFlags;
            do
            {
                level--;
                if (level < 0)
                    break;
                conflictGroupStack.Delete(conflictGroupStack.Count - 1);
                popupFlags = popupFlagsStack[popupFlagsStack.Count - 1];
                popupFlagsStack.Delete(popupFlagsStack.Count - 1);
            } while ((popupFlags & MF_END) && level >= 0);
        }
    } while (level >= 0);

    ConflictGroupsCount = conflictGroupMax;

    //  if (ID == 600)
    //  {
    //    TRACE_I("#################### MENU ####################");
    //    for (int ii = 0; ii < Items.Count; ii++)
    //    {
    //      CMenuItem *item = &Items[ii];
    //      TRACE_I(item->TString << " #" << item->ConflictGroup << " Flags:"<<std::hex<< item->Flags);
    //    }
    //  }

    return TRUE;
}

// maximalne zjednodusene nacitani ciste kvuli hledani v MUIMode
// pokud bychom meli MENUEX zacit pouzivat i pro ukladani, je ho treba prepsat
BOOL CMenuData::LoadMenuEx(LPCSTR original, LPCSTR translated, CData* data)
{
    char errBuf[3000];
    DWORD muiID = 1;
    DWORD oResinfo;
    int level = 0;
    TDirectArray<DWORD> popupFlagsStack(100, 100); // slouzi pro detekci konciciho popupu

    do
    {
        DWORD oType = GET_DWORD(original);
        DWORD tType = GET_DWORD(translated);
        original += sizeof(DWORD);
        translated += sizeof(DWORD);

        DWORD oState = GET_DWORD(original);
        DWORD tState = GET_DWORD(translated);
        original += sizeof(DWORD);
        translated += sizeof(DWORD);

        DWORD oID = GET_DWORD(original);
        DWORD tID = GET_DWORD(translated);
        original += sizeof(DWORD);
        translated += sizeof(DWORD);

        oResinfo = GET_WORD(original);
        DWORD tResinfo = GET_WORD(translated);
        original += sizeof(WORD);
        translated += sizeof(WORD);

        // align
        original += (~((UINT_PTR)original - 1)) & 1;
        translated += (~((UINT_PTR)translated - 1)) & 1;
        LPWSTR oText = (LPWSTR)original;
        LPWSTR tText = (LPWSTR)translated;
        original += (1 + lstrlenW((wchar_t*)oText)) * sizeof(WCHAR);
        translated += (1 + lstrlenW((wchar_t*)tText)) * sizeof(WCHAR);
        // align
        original += (~((UINT_PTR)original - 1)) & 3;
        translated += (~((UINT_PTR)translated - 1)) & 3;

        if (oResinfo != tResinfo)
        {
            sprintf_s(errBuf, "Original and translated menu item has different resinfo.");
            MessageBox(GetMsgParent(), errBuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }

        int oLen = wcslen((LPCWSTR)oText) + 1;
        int tLen = wcslen((LPCWSTR)tText) + 1;

        CMenuItem item;
        ZeroMemory(&item, sizeof(item));
        item.State = PROGRESS_STATE_TRANSLATED;

        if (!DecodeString(oText, oLen, &item.OString))
            return FALSE;
        if (!DecodeString(tText, tLen, &item.TString))
            return FALSE;

        item.ID = (WORD)muiID;
        muiID++;

        if (oResinfo & 1) // popup?
        {
            // skip helpid
            original += sizeof(DWORD);
            translated += sizeof(DWORD);
        }

        Items.Add(item);
        if (!Items.IsGood())
        {
            return FALSE;
        }

        if (oResinfo & 1) // popup
        {
            level++;
            popupFlagsStack.Add(oResinfo);
        }
        if (!(oResinfo & 1) && (oResinfo & MF_END))
        {
            // jde o polozku, ktera neni popup a zaroven je v popupu posledni
            DWORD popupFlags;
            do
            {
                level--;
                if (level < 0)
                    break;
                popupFlags = popupFlagsStack[popupFlagsStack.Count - 1];
                popupFlagsStack.Delete(popupFlagsStack.Count - 1);
            } while ((popupFlags & MF_END) && level >= 0);
        }
    } while (level >= 0);
    return TRUE;
}

//*****************************************************************************
//
// CData
//

BOOL CData::SaveMenus(HANDLE hUpdateRes)
{
    BYTE buff[200000];

    for (int i = 0; i < MenuData.Count; i++)
    {
        CMenuData* menuData = MenuData[i];
        BYTE* iter = buff;
        PUT_DWORD(iter, 0);
        iter += sizeof(DWORD);
        for (int j = 0; j < menuData->Items.Count; j++)
        {
            CMenuItem* menuItem = &menuData->Items[j];
            PUT_WORD(iter, menuItem->Flags);
            iter += sizeof(WORD);
            if (!(menuItem->Flags & MF_POPUP))
            {
                PUT_WORD(iter, menuItem->ID);
                iter += sizeof(WORD);
            }

            EncodeString(menuItem->TString, (wchar_t**)&iter);
        }
        BOOL result = TRUE;
        if (menuData->TLangID != MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)) // resource neni "neutral", musime ho smaznout, aby ve vyslednem .SLG nebyly menu dve
        {
            result = UpdateResource(hUpdateRes, RT_MENU, MAKEINTRESOURCE(menuData->ID),
                                    menuData->TLangID,
                                    NULL, 0);
        }
        if (result)
        {
            result = UpdateResource(hUpdateRes, RT_MENU, MAKEINTRESOURCE(menuData->ID),
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                                    buff, iter - buff);
        }
        if (!result)
            return FALSE;
    }
    return TRUE;
}

BOOL CData::MenusAddTranslationStates()
{
    for (int i = 0; i < MenuData.Count; i++)
    {
        CMenuData* menuData = MenuData[i];
        for (int j = 0; j < menuData->Items.Count; j++)
        {
            CMenuItem* menuItem = &menuData->Items[j];
            if (wcslen(menuItem->TString) > 0 && !AddTranslationState(tteMenus, j, menuData->ID, menuItem->State))
                return FALSE;
        }
    }
    return TRUE;
}

int CData::FindMenuData(WORD id)
{
    for (int i = 0; i < MenuData.Count; i++)
        if (id == MenuData[i]->ID)
            return i;
    return -1;
}
