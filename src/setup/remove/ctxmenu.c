// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

extern int StrICmp(const char* s1, const char* s2);

int SplitPathAndConvertToWideChar(const char* fname, wchar_t* out_path, int out_path_wchars, wchar_t* out_fname, int out_fname_wchars)
{
    char buff[2 * MAX_PATH];
    lstrcpy(buff, fname);
    if (lstrlen(buff) > 0)
    {
        char* p = buff + lstrlen(buff) - 1;
        while (p > buff && *p != '\\')
            p--;
        if (*p == '\\')
            *p = 0;
        MultiByteToWideChar(CP_ACP, 0, buff, -1, out_path, out_path_wchars);
        MultiByteToWideChar(CP_ACP, 0, p + 1, -1, out_fname, out_fname_wchars);
    }
    return 0;
}

BOOL InvokeCommand(IContextMenu* ctxMenu, int id, HWND hParent)
{
    CMINVOKECOMMANDINFO ci;
    memset(&ci, 0, sizeof(ci));
    ci.cbSize = sizeof(ci);
    ci.hwnd = hParent;
    ci.lpVerb = MAKEINTRESOURCE(id);
    ci.nShow = SW_SHOWNORMAL;
    return (ctxMenu->lpVtbl->InvokeCommand(ctxMenu, &ci) == S_OK);
}

BOOL FindAndInvokeCommand(IContextMenu* ctxMenu, HMENU hMenu, const char* cmdName, HWND hParent)
{
    BOOL ret = FALSE;
    MENUITEMINFO mi = {0};
    char itemName[500] = {0};

    int itemsCount = GetMenuItemCount(hMenu);
    int i;
    for (i = 0; i < itemsCount; i++)
    {
        memset(&mi, 0, sizeof(mi)); // nutne zde
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID | MIIM_SUBMENU;
        mi.dwTypeData = itemName;
        mi.cch = 500;
        if (GetMenuItemInfo(hMenu, i, TRUE, &mi))
        {
            if (mi.hSubMenu == NULL && (mi.fType & MFT_SEPARATOR) == 0) // neni submenu ani separator
            {
                char name[2000]; // schvalne mame 2000 misto 200, shell-extensiony obcas zapisuji dvojnasobek (uvaha: unicode = 2 * "pocet znaku"), atp.
                name[0] = 0;
                if (ctxMenu->lpVtbl->GetCommandString(ctxMenu, mi.wID, GCS_VERB, NULL, name, 200) == NOERROR)
                {
                    if (StrICmp(name, cmdName) == 0)
                    {
                        ret = InvokeCommand(ctxMenu, mi.wID, hParent);
                        break;
                    }
                }
            }
        }
    }
    return ret;
}

BOOL InvokeCmdFromContextMenuAux(HWND hWnd, const char* fileName, const char* cmdName)
{
    BOOL ret = FALSE;
    HRESULT res;
    ITEMIDLIST* folder = NULL;
    ITEMIDLIST* file = NULL;
    IShellFolder* shell = NULL;
    IShellFolder* parent = NULL;

    CoCreateInstance(&CLSID_ShellDesktop, NULL, CLSCTX_INPROC, &IID_IShellFolder, &shell);
    if (shell != NULL)
    {
        wchar_t wPath[MAX_PATH] = {0};
        wchar_t wFileName[MAX_PATH] = {0};
        SplitPathAndConvertToWideChar(fileName, wPath, sizeof(wPath) / sizeof(wchar_t), wFileName, sizeof(wFileName) / sizeof(wchar_t));
        res = shell->lpVtbl->ParseDisplayName(shell, NULL, NULL, wPath, NULL, &folder, NULL);
        if (res == S_OK)
        {
            shell->lpVtbl->BindToObject(shell, folder, NULL, &IID_IShellFolder, &parent);
            if (parent != NULL)
            {
                IContextMenu* ctxMenu = NULL;
                res = parent->lpVtbl->ParseDisplayName(parent, NULL, NULL, wFileName, NULL, &file, NULL);
                if (res == S_OK)
                    parent->lpVtbl->GetUIObjectOf(parent, NULL, 1, &file, &IID_IContextMenu, NULL, &ctxMenu);
                parent->lpVtbl->Release(parent);
                if (ctxMenu != NULL)
                {
                    HMENU hMenu = CreatePopupMenu();
                    if (hMenu != NULL)
                    {
                        ctxMenu->lpVtbl->QueryContextMenu(ctxMenu, hMenu, 0, 0, 0x7FFF, CMF_NORMAL);
                        ret = FindAndInvokeCommand(ctxMenu, hMenu, cmdName, hWnd);
                        ctxMenu->lpVtbl->Release(ctxMenu);
                        DestroyMenu(hMenu);
                    }
                }
            }
        }
        shell->lpVtbl->Release(shell);
    }
    if (folder != NULL)
        CoTaskMemFree(folder);
    if (file != NULL)
        CoTaskMemFree(file);
    return ret;
}

BOOL InvokeCmdFromContextMenu(HWND hWnd, const char* fileName, const char* cmdName)
{
    BOOL ret;
    __try
    {
        // nechceme aby nam padal setup/remove kvuli shell extensions
        ret = InvokeCmdFromContextMenuAux(hWnd, fileName, cmdName);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ret = FALSE;
    }
    return ret;
}
