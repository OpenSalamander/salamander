// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <ShObjIdl.h>
#include "versinfo.rh2"
#include "jumplist.h"
#include "mainwnd.h"
//#include "propvarutil.h"

//#pragma comment(lib,"Shlwapi.lib")

/* JUMP LIST DOC
http://blogs.windows.com/windows/archive/b/developers/archive/2009/06/22/developing-for-the-windows-7-taskbar-jump-into-jump-lists-part-1.aspx
http://blogs.windows.com/windows/archive/b/developers/archive/2009/06/25/developing-for-the-windows-7-taskbar-jump-into-jump-lists-part-2.aspx
http://blogs.windows.com/windows/archive/b/developers/archive/2009/07/02/developing-for-the-windows-7-taskbar-jump-into-jump-lists-part-3.aspx
http://msdn.microsoft.com/en-us/library/dd378460%28v=VS.85%29.aspx#custom_jump_lists
*/

DEFINE_PROPERTYKEY(PKEY_Title, 0xF29F85E0, 0x4FF9, 0x1068, 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9, 2);
DEFINE_PROPERTYKEY(PKEY_AppUserModel_IsDestListSeparator, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 6);

// unikatni ID procesu chceme svazat s konfiguraci (dve ruzne verze Salamandera mohou mit napriklad ruzne hot paths)
//const char *SALAMANDER_APP_ID = "OPENSAL.OpenSalamanderAppID." VERSINFO_xstr(VERSINFO_BUILDNUMBER);

//typedef WINSHELLAPI HRESULT (WINAPI *FT_SetCurrentProcessExplicitAppUserModelID)(PCWSTR appID);
//typedef WINSHELLAPI HRESULT (WINAPI *FT_SHCreateItemFromIDList)(PCIDLIST_ABSOLUTE pidl, REFIID riid, void **ppv);
//FT_SetCurrentProcessExplicitAppUserModelID SetCurrentProcessExplicitAppUserModelID = NULL;
//FT_SHCreateItemFromIDList SHCreateItemFromIDList = NULL;

//{
//  BOOL ret = FALSE;
//  if (Windows7AndLater)
//  {
//    HMODULE hShell32 = LoadLibrary("shell32.dll");
//    if (hShell32 != NULL)
//    {
//      FT_SetCurrentProcessExplicitAppUserModelID SetCurrentProcessExplicitAppUserModelID = NULL;
//      SetCurrentProcessExplicitAppUserModelID = (FT_SetCurrentProcessExplicitAppUserModelID)GetProcAddress(hShell32, "SetCurrentProcessExplicitAppUserModelID");
//      SHCreateItemFromIDList = (FT_SHCreateItemFromIDList)GetProcAddress(hShell32, "SHCreateItemFromIDList");
//      if (SetCurrentProcessExplicitAppUserModelID != NULL && SHCreateItemFromIDList != NULL)
//      {
//        wchar_t appID[500];
//        ConvertA2U(SALAMANDER_APP_ID, -1, appID, _countof(appID));
////        HRESULT hres = SetCurrentProcessExplicitAppUserModelID(appID);
//        HRESULT hres = S_OK;
//        if (hres == S_OK)
//        {
//          ret = TRUE;
//        }
//        else
//        {
//          TRACE_E("SetCurrentProcessExplicitAppUserModelID() failed! hres="<<hres);
//        }
//      }
//      //FreeLibrary(hShell32); // FIXME - vyresit nejak cisteji
//    }
//  }
//  return ret;
//}

// Creates a CLSID_ShellLink to insert into the Tasks section of the Jump List.  This type of Jump
// List item allows the specification of an explicit command line to execute the task.
//
//HRESULT CreateShellLink(PCWSTR pszArguments, PCWSTR pszTitle, IShellLink **ppsl)
//{
//    IShellLink *psl;
//    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
//    if (SUCCEEDED(hr))
//    {
//        // Determine our executable's file path so the task will execute this application
//        char szAppPath[MAX_PATH];
//        if (GetModuleFileName(NULL, szAppPath, ARRAYSIZE(szAppPath)))
//        {
//            hr = psl->SetPath(szAppPath);
//            if (SUCCEEDED(hr))
//            {
//
//              char buff[2000];
//              ConvertU2A(pszArguments, -1, buff, sizeof(buff));
//                hr = psl->SetArguments(buff);
//                if (SUCCEEDED(hr))
//                {
//                    // The title property is required on Jump List items provided as an IShellLink
//                    // instance.  This value is used as the display name in the Jump List.
//                    IPropertyStore *pps;
//                    hr = psl->QueryInterface(IID_PPV_ARGS(&pps));
//                    if (SUCCEEDED(hr))
//                    {
//                        PROPVARIANT propvar;
//                        hr = InitPropVariantFromString(pszTitle, &propvar);
//                        if (SUCCEEDED(hr))
//                        {
//                            hr = pps->SetValue(PKEY_Title, propvar);
//                            if (SUCCEEDED(hr))
//                            {
//                                hr = pps->Commit();
//                                if (SUCCEEDED(hr))
//                                {
//                                    hr = psl->QueryInterface(IID_PPV_ARGS(ppsl));
//                                }
//                            }
//                            PropVariantClear(&propvar);
//                        }
//                        pps->Release();
//                    }
//                }
//            }
//        }
//        else
//        {
//            hr = HRESULT_FROM_WIN32(GetLastError());
//        }
//        psl->Release();
//    }
//    return hr;
//}

// The Tasks category of Jump Lists supports separator items.  These are simply IShellLink instances
// that have the PKEY_AppUserModel_IsDestListSeparator property set to TRUE.  All other values are
// ignored when this property is set.
//HRESULT CreateSeparatorLink(IShellLink **ppsl)
//{
//    IPropertyStore *pps;
//    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pps));
//    if (SUCCEEDED(hr))
//    {
//        PROPVARIANT propvar;
//        hr = InitPropVariantFromBoolean(TRUE, &propvar);
//        if (SUCCEEDED(hr))
//        {
//            hr = pps->SetValue(PKEY_AppUserModel_IsDestListSeparator, propvar);
//            if (SUCCEEDED(hr))
//            {
//                hr = pps->Commit();
//                if (SUCCEEDED(hr))
//                {
//                    hr = pps->QueryInterface(IID_PPV_ARGS(ppsl));
//                }
//            }
//            PropVariantClear(&propvar);
//        }
//        pps->Release();
//    }
//    return hr;
//}

HRESULT CreateShellLink(const char* path, const char* name, IShellLink** psl)
{
    char params[HOTPATHITEM_MAXPATH + 100];
    sprintf(params, "-AJ \"%s\"", path);
    if (strlen(params) < INFOTIPSIZE) // omezeni delky W2K+ pro SetArguments
    {
        HRESULT hres;
        IShellLink* ret;
        hres = CoCreateInstance(CLSID_ShellLink, NULL,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&ret));
        if (SUCCEEDED(hres))
        {
            char pathName[MAX_PATH];
            GetModuleFileName(NULL, pathName, sizeof(pathName) - 1);

            // Set path, parameters, icon and description.
            ret->SetPath(pathName);
            ret->SetArguments(params);
            char desc[MAX_PATH];
            lstrcpyn(desc, path, _countof(desc));
            if (strlen(path) >= _countof(desc))
                strcpy(desc + _countof(desc) - 4, "..."); // jako ze je to orizly
            ret->SetDescription(desc);                    // limit je MAX_PATH+1 (aspon na Windows 7, kde to ted testuju), delsi = jump list se vubec neukaze
            ret->SetIconLocation("shell32.dll", -319);    // tato ikona existuje od XP dal

            // To set the link title, we require the property store of the link.
            IPropertyStore* pPS;
            hres = ret->QueryInterface(IID_PPV_ARGS(&pPS));
            if (SUCCEEDED(hres))
            {
                PROPVARIANT pv;
                PropVariantInit(&pv);
                pv.vt = VT_LPSTR;
                pv.pszVal = (LPSTR)name;
                pPS->SetValue(PKEY_Title, pv);
                pPS->Commit();
                pPS->Release();
            }
            else
                TRACE_E("CreateShellLink: QueryInterface(IPropertyStore) failed!");
        }
        else
            TRACE_E("CreateShellLink: CoCreateInstance(CLSID_ShellLink) failed!");

        *psl = ret;
        return hres;
    }
    else
    {
        TRACE_E("CreateShellLink: too long hot-path!");
        *psl = NULL;
        return E_FAIL;
    }
}

// Builds the collection of task items and adds them to the Task section of the Jump List.  All tasks
// should be added to the canonical "Tasks" category by calling ICustomDestinationList::AddUserTasks.
HRESULT AddTasksToList(ICustomDestinationList* pcdl)
{
    IObjectCollection* poc;
    HRESULT hr = CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&poc));
    if (SUCCEEDED(hr))
    {
        IShellLink* psl;

        int count = 0;
        for (int i = 0; i < HOT_PATHS_COUNT; i++)
        {
            if (MainWindow->HotPaths.GetVisible(i))
            {
                char name[MAX_PATH];
                char path[HOTPATHITEM_MAXPATH];
                name[0] = 0;
                path[0] = 0;
                MainWindow->HotPaths.GetName(i, name, MAX_PATH);
                MainWindow->HotPaths.GetPath(i, path, HOTPATHITEM_MAXPATH);
                if (name[0] != 0 && path[0] != 0)
                {
                    hr = CreateShellLink(path, name, &psl);
                    if (SUCCEEDED(hr))
                    {
                        hr = poc->AddObject(psl);
                        if (SUCCEEDED(hr))
                            count++;
                        else
                            TRACE_E("AddTasksToList: AddObject() failed!");
                        psl->Release();
                    }
                }
            }
        }

        //if (SUCCEEDED(hr))
        //{
        //    hr = CreateSeparatorLink(&psl);
        //    if (SUCCEEDED(hr))
        //    {
        //        hr = poc->AddObject(psl);
        //        psl->Release();
        //    }
        //}

        if (SUCCEEDED(hr) && count > 0)
        {
            IObjectArray* poa;
            hr = poc->QueryInterface(IID_PPV_ARGS(&poa));
            if (SUCCEEDED(hr))
            {
                // Add the tasks to the Jump List. Tasks always appear in the canonical "Tasks"
                // category that is displayed at the bottom of the Jump List, after all other
                // categories.
                hr = pcdl->AddUserTasks(poa);
                if (!SUCCEEDED(hr))
                    TRACE_E("AddTasksToList: AddUserTasks failed!");
                poa->Release();
            }
            else
                TRACE_E("AddTasksToList: QueryInterface(IObjectArray) failed!");
        }
        poc->Release();
    }
    else
        TRACE_E("AddTasksToList: CoCreateInstance(CLSID_EnumerableObjectCollection) failed!");
    return hr;
}

void CreateJumpList()
{
    ICustomDestinationList* pcdl;
    HRESULT hr = CoCreateInstance(
        CLSID_DestinationList,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pcdl));
    if (SUCCEEDED(hr))
    {
        //important to setup App Id for the Jump List
        //wchar_t appID[500];
        //ConvertA2U(SALAMANDER_APP_ID, -1, appID, _countof(appID));
        //hr = pcdl->SetAppID(appID);
        //    if (SUCCEEDED(hr))
        {
            UINT uMaxSlots;
            IObjectArray* poaRemoved;
            hr = pcdl->BeginList(
                &uMaxSlots,
                IID_PPV_ARGS(&poaRemoved));
            if (SUCCEEDED(hr))
            {
                hr = AddTasksToList(pcdl);
                if (SUCCEEDED(hr))
                {
                    hr = pcdl->CommitList();
                    if (!SUCCEEDED(hr))
                        TRACE_E("CreateJumpList: CommitList() failed!");
                }
                poaRemoved->Release();
            }
            else
                TRACE_E("CreateJumpList: BeginList() failed!");
        }
    }
    else
        TRACE_E("CreateJumpList: CoCreateInstance(CLSID_DestinationList) failed!");
}
