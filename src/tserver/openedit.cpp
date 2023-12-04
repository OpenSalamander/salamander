// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// import EnvDTE based on its LIBID, dte80a.tlh will be created in build directory and included into project
#pragma warning(disable : 4278)
#import "libid:80cc9f66-e7d8-4ddd-85b6-d9e6cd0e93e2" raw_interfaces_only named_guids
#pragma warning(default : 4278)

#pragma push_macro("new")
#undef new
#include "wil/com.h"
#pragma pop_macro("new")

#include "trace.h"
#include "openedit.h"

class CCoInitialize
{
public:
    CCoInitialize() noexcept
    {
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr == RPC_E_CHANGED_MODE)
            hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    }

    ~CCoInitialize()
    {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }

    operator HRESULT() const noexcept
    {
        return hr;
    }

private:
    HRESULT hr;
} CoInit;

void OpenFileInMSVC(const WCHAR* filename, int line)
{
    if (FAILED(CoInit))
        return;

    try
    {
        // general or specific VS version: L"VisualStudio.DTE" or L"VisualStudio.DTE.16.0"
        CLSID clsid;
        THROW_IF_FAILED(::CLSIDFromProgID(L"VisualStudio.DTE", &clsid));

        // get active pointer to VS
        wil::com_ptr<IUnknown> unk;
        THROW_IF_FAILED(::GetActiveObject(clsid, NULL, &unk));

        // cast to our smart pointer type for EnvDTE
        auto DTE = unk.query<EnvDTE::_DTE>();

        // get the item operations pointer
        wil::com_ptr<EnvDTE::ItemOperations> itemOperations;
        THROW_IF_FAILED(DTE->get_ItemOperations(&itemOperations));

        // open required file in VS
        _bstr_t bstrFileName(filename);
        _bstr_t bstrKind(EnvDTE::vsViewKindTextView);
        wil::com_ptr<EnvDTE::Window> window;
        THROW_IF_FAILED(itemOperations->OpenFile(bstrFileName, bstrKind, &window));

        // focus required line
        wil::com_ptr<EnvDTE::Document> doc;
        THROW_IF_FAILED(DTE->get_ActiveDocument(&doc));
        wil::com_ptr<IDispatch> selection_dispatch;
        THROW_IF_FAILED(doc->get_Selection(&selection_dispatch));
        auto selection = selection_dispatch.query<EnvDTE::TextSelection>();
        THROW_IF_FAILED(selection->GotoLine(line, FALSE /*Select*/));

        // activate VS window
        wil::com_ptr<EnvDTE::Window> mainWindow;
        THROW_IF_FAILED(DTE->get_MainWindow(&mainWindow));

        long long_hwnd = 0;
        THROW_IF_FAILED(mainWindow->get_HWnd(&long_hwnd));
        HWND hwnd = (HWND)LongToHandle(long_hwnd);
        if (IsWindow(hwnd))
            ::SetForegroundWindow(hwnd);
    }
    catch (std::exception& e)
    {
        const char* msg = e.what();
        TRACE_E("OpenFileInMSVC(): std::exception occurred: " << msg);
    }
    catch (_com_error& e)
    {
        const WCHAR* msg = e.ErrorMessage();
        TRACE_EW(L"OpenFileInMSVC(): _com_error occurred: " << msg);
    }
}
