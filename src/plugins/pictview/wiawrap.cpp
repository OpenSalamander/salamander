// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef ENABLE_WIA

#pragma comment(lib, "wiaguid")

// I have used source code from GetImage sample from MSVC 2008
// (C:\Program Files (x86)\Microsoft Visual Studio 9.0\Samples\1033\
//  AllVCLanguageSamples.zip\C++\OS\WindowsXP\GetImage)
// GetImage sample copy: pictview\doc\GetImage-sample-WIA.7z

#include <comdef.h>

// we use compiler option /J: Default char Type Is unsigned
// MSDN warning: If you use this compiler option with ATL/MFC, an error might be generated.
// Although you could disable this error by defining _ATL_ALLOW_CHAR_UNSIGNED, this
// workaround is not supported and may not always work.
// http://stackoverflow.com/questions/27246149/when-will-atl-allow-unsigned-char-work comment:
// The principal trouble-maker here is the AtlGetHexValue() function. This function returns -1.
// We don't use AtlGetHexValue(), so it looks like I can compile with /J with reasonable confidence.
#define _ATL_ALLOW_CHAR_UNSIGNED

#ifdef new
//#pragma push_macro("new")  // push_macro and pop_macro are not working here (try memory leak test)
#define __WIAWRAP_REDEF_NEW
#undef new
#endif

#include <atlbase.h>

#ifdef __WIAWRAP_REDEF_NEW
//#pragma pop_macro("new")  // push_macro and pop_macro are not working here (try memory leak test)
#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define __WIAWRAP_STR2(x) #x
#define __WIAWRAP_STR(x) __WIAWRAP_STR2(x)
#pragma message(__FILE__ "(" __WIAWRAP_STR(__LINE__) "): error: operator new with unknown reason for redefinition")
#endif
#undef __WIAWRAP_REDEF_NEW
#endif

#if (_WIN32_WINNT >= 0x0501) // Windows XP and later
#include <wia.h>
#else // (_WIN32_WINNT >= 0x0501) // Windows XP and later
// WIA is Windows XP+, we use it only in x64 build, so it is no problem (Windows XP x64 is the first 64-bit Windows)
#if _WIN32_WINNT != 0x0500
void* just_to_show_error = &change_default_value_of_WIN32_WINNT;
#endif
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501 // change W2K to XP
#include <wia.h>
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 // change back to W2K
#endif                      // (_WIN32_WINNT >= 0x0501) // Windows XP and later

#include <sti.h>

#include "lib/pvw32dll.h"
#include "renderer.h"
#include "dialogs.h"
#include "pictview.h"
#include "wiawrap.h"

//****************************************************************************
//
// CInitCommonControlsEx
//

struct CInitCommonControlsEx
{
    CInitCommonControlsEx()
    {
        INITCOMMONCONTROLSEX iccex;

        iccex.dwSize = sizeof(iccex);
        iccex.dwICC = ICC_BAR_CLASSES;

        InitCommonControlsEx(&iccex);
    }
} __InitCommonControlsEx;

//****************************************************************************
//
// CComPtrArray
//

/*++

    CComPtrArray stores an array of COM interface pointers and performs
    reference counting through AddRef and Release methods. 
    
    CComPtrArray can be used with WiaGetImage and DeviceDlg functions 
    to provide automatic deallocation of the output arrays.

Methods

    CComPtrArray()
        Initializes the array pointer and the count to zero.

    CComPtrArray(int nCount)
        Allocates the array for with CoTaskMemAlloc for nCount items and 
        initializes the interface pointers to NULL

    Copy(const CComPtrArray& rhs)
        Allocates a new array with CoTaskMemAlloc, copies the interface 
        pointers and call AddRef() on the copied pointers.

      Clear()
        Calls Release on the interface pointers in the array and
        deallocates the array with CoTaskMemFree

    CComPtrArray(const CComPtrArray& rhs)
        Calls the Copy method to copy the new contents.

    CComPtrArray &operator =(const CComPtrArray& rhs)
        Calls the Clear method to delete the current contents and calls 
        the Copy method to copy the new contents.

    ~CComPtrArray()
        Destructor, calls the Clear method

    operator T **()
        Returns the dereferenced value of the member pointer.

    bool operator!()
        Returns TRUE or FALSE, depending on whether the member pointer is 
        NULL or not.

    T ***operator&()
        Returns the address of the member pointer.

    LONG &Count()
        Returns a reference to the count.

--*/

template <class T>
class CComPtrArray
{
public:
    CComPtrArray()
    {
        m_pArray = NULL;
        m_nCount = 0;
    }

    explicit CComPtrArray(int nCount)
    {
        m_pArray = (T**)CoTaskMemAlloc(nCount * sizeof(T*));

        m_nCount = m_pArray == NULL ? 0 : nCount;

        for (int i = 0; i < m_nCount; ++i)
        {
            m_pArray[i] = NULL;
        }
    }

    CComPtrArray(const CComPtrArray& rhs)
    {
        Copy(rhs);
    }

    ~CComPtrArray()
    {
        Clear();
    }

    CComPtrArray& operator=(const CComPtrArray& rhs)
    {
        if (this != &rhs)
        {
            Clear();
            Copy(rhs);
        }

        return *this;
    }

    operator T**()
    {
        return m_pArray;
    }

    bool operator!()
    {
        return m_pArray == NULL;
    }

    T*** operator&()
    {
        return &m_pArray;
    }

    LONG& Count()
    {
        return m_nCount;
    }

    void Clear()
    {
        if (m_pArray != NULL)
        {
            for (int i = 0; i < m_nCount; ++i)
            {
                if (m_pArray[i] != NULL)
                {
                    m_pArray[i]->Release();
                }
            }

            CoTaskMemFree(m_pArray);

            m_pArray = NULL;
            m_nCount = 0;
        }
    }

    void Copy(const CComPtrArray& rhs)
    {
        m_pArray = NULL;
        m_nCount = 0;

        if (rhs.m_pArray != NULL)
        {
            m_pArray = (T**)CoTaskMemAlloc(rhs.m_nCount * sizeof(T*));

            if (m_pArray != NULL)
            {
                m_nCount = rhs.m_nCount;

                for (int i = 0; i < m_nCount; ++i)
                {
                    m_pArray[i] = rhs.m_pArray[i];

                    if (m_pArray[i] != NULL)
                    {
                        m_pArray[i]->AddRef();
                    }
                }
            }
        }
    }

private:
    T** m_pArray;
    LONG m_nCount;
};

//****************************************************************************
//
// ReadPropertyLong
//

HRESULT
ReadPropertyLong(
    IWiaPropertyStorage* pWiaPropertyStorage,
    const PROPSPEC* pPropSpec,
    LONG* plResult)
{
    PROPVARIANT PropVariant;

    HRESULT hr = pWiaPropertyStorage->ReadMultiple(
        1,
        pPropSpec,
        &PropVariant);

    // Generally, the return value should be checked against S_FALSE.
    // If ReadMultiple returns S_FALSE, it means the property name or ID
    // had valid syntax, but it didn't exist in this property set, so
    // no properties were retrieved, and each PROPVARIANT structure is set
    // to VT_EMPTY. But the following switch statement will handle this case
    // and return E_FAIL. So the caller of ReadPropertyLong does not need
    // to check for S_FALSE explicitly.

    if (SUCCEEDED(hr))
    {
        switch (PropVariant.vt)
        {
        case VT_I1:
        {
            *plResult = (LONG)PropVariant.cVal;

            hr = S_OK;

            break;
        }

        case VT_UI1:
        {
            *plResult = (LONG)PropVariant.bVal;

            hr = S_OK;

            break;
        }

        case VT_I2:
        {
            *plResult = (LONG)PropVariant.iVal;

            hr = S_OK;

            break;
        }

        case VT_UI2:
        {
            *plResult = (LONG)PropVariant.uiVal;

            hr = S_OK;

            break;
        }

        case VT_I4:
        {
            *plResult = (LONG)PropVariant.lVal;

            hr = S_OK;

            break;
        }

        case VT_UI4:
        {
            *plResult = (LONG)PropVariant.ulVal;

            hr = S_OK;

            break;
        }

        case VT_INT:
        {
            *plResult = (LONG)PropVariant.intVal;

            hr = S_OK;

            break;
        }

        case VT_UINT:
        {
            *plResult = (LONG)PropVariant.uintVal;

            hr = S_OK;

            break;
        }

        case VT_R4:
        {
            *plResult = (LONG)(PropVariant.fltVal + 0.5);

            hr = S_OK;

            break;
        }

        case VT_R8:
        {
            *plResult = (LONG)(PropVariant.dblVal + 0.5);

            hr = S_OK;

            break;
        }

        default:
        {
            hr = E_FAIL;

            break;
        }
        }
    }

    PropVariantClear(&PropVariant);

    return hr;
}

//****************************************************************************
//
// ReadPropertyGuid
//

#ifndef MAX_GUID_STRING_LEN
#define MAX_GUID_STRING_LEN 39
#endif //MAX_GUID_STRING_LEN

HRESULT
ReadPropertyGuid(
    IWiaPropertyStorage* pWiaPropertyStorage,
    const PROPSPEC* pPropSpec,
    GUID* pguidResult)
{
    PROPVARIANT PropVariant;

    HRESULT hr = pWiaPropertyStorage->ReadMultiple(
        1,
        pPropSpec,
        &PropVariant);

    // Generally, the return value should be checked against S_FALSE.
    // If ReadMultiple returns S_FALSE, it means the property name or ID
    // had valid syntax, but it didn't exist in this property set, so
    // no properties were retrieved, and each PROPVARIANT structure is set
    // to VT_EMPTY. But the following switch statement will handle this case
    // and return E_FAIL. So the caller of ReadPropertyGuid does not need
    // to check for S_FALSE explicitly.

    if (SUCCEEDED(hr))
    {
        switch (PropVariant.vt)
        {
        case VT_CLSID:
        {
            *pguidResult = *PropVariant.puuid;

            hr = S_OK;

            break;
        }

        case VT_BSTR:
        {
            hr = CLSIDFromString(PropVariant.bstrVal, pguidResult);

            break;
        }

        case VT_LPWSTR:
        {
            hr = CLSIDFromString(PropVariant.pwszVal, pguidResult);

            break;
        }

        case VT_LPSTR:
        {
            WCHAR wszGuid[MAX_GUID_STRING_LEN];
            mbstowcs_s(NULL, wszGuid, MAX_GUID_STRING_LEN, PropVariant.pszVal, MAX_GUID_STRING_LEN);

            wszGuid[MAX_GUID_STRING_LEN - 1] = L'\0';

            hr = CLSIDFromString(wszGuid, pguidResult);

            break;
        }

        default:
        {
            hr = E_FAIL;

            break;
        }
        }
    }

    PropVariantClear(&PropVariant);

    return hr;
}

//****************************************************************************
//
// CProgressDlg
//

/*++

    CProgressDlg implements a simple progress dialog with a status message,
    progress bar and a cancel button. The dialog is created in a separate 
    thread so that it remains responsive to the user while the main thread 
    is busy with a long operation. It is descended from IUnknown for 
    reference counting.

Methods

    CProgressDlg
        Creates a new thread that will display the progress dialog

    ~CProgressDlg
        Closes the progress dialog

    Cancelled
        Returns TRUE if the user has pressed the cancel button.

    SetTitle
        Sets the title of the progress dialog.

    SetMessage
        Sets the status message.

    SetPercent
        Sets the progress bar position. 

    ThreadProc
        Creates the dialog box

    DialogProc
        Processes window messages.

--*/

class CProgressDlg : public IUnknown
{
public:
    CProgressDlg(HWND hWndParent);
    ~CProgressDlg();

    // IUnknown interface

    STDMETHOD(QueryInterface)
    (REFIID iid, LPVOID* ppvObj);
    STDMETHOD_(ULONG, AddRef)
    ();
    STDMETHOD_(ULONG, Release)
    ();

    // CProgressDlg methods

    BOOL Cancelled() const;

    VOID SetTitle(PCTSTR pszTitle);
    VOID SetMessage(PCTSTR pszMessage);
    VOID SetPercent(UINT nPercent);

private:
    static DWORD WINAPI ThreadProc(PVOID pParameter);
    static INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM);

private:
    LONG m_cRef;
    HWND m_hDlg;
    HWND m_hWndParent;
    LONG m_bCancelled;
    HANDLE m_hInitDlg;
};

CProgressDlg::CProgressDlg(HWND hWndParent)
{
    m_cRef = 0;

    m_hDlg = NULL;
    m_hWndParent = hWndParent;
    m_bCancelled = FALSE;

    m_hInitDlg = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (m_hInitDlg != NULL)
    {
        // Create the progress dialog in a separate thread so that it
        // remains responsive

        DWORD dwThreadId;

        HANDLE hThread = CreateThread(
            NULL,
            0,
            ThreadProc,
            this,
            0,
            &dwThreadId);

        // Wait until the progress dialog is created and ready to process
        // messages. During creation, the new thread will send window messages
        // to this thread, and this will cause a deadlock if we use
        // WaitForSingleObject instead of MsgWaitForMultipleObjects

        if (hThread != NULL)
        {
            while (MsgWaitForMultipleObjects(1, &m_hInitDlg, FALSE, INFINITE, QS_ALLINPUT | QS_ALLPOSTMESSAGE) == WAIT_OBJECT_0 + 1)
            {
                MSG msg;

                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            CloseHandle(hThread);
        }

        CloseHandle(m_hInitDlg);
    }
}

CProgressDlg::~CProgressDlg()
{
    EndDialog(m_hDlg, IDCLOSE);
}

STDMETHODIMP CProgressDlg::QueryInterface(REFIID iid, LPVOID* ppvObj)
{
    if (ppvObj == NULL)
    {
        return E_POINTER;
    }

    if (iid == IID_IUnknown)
    {
        *ppvObj = (IUnknown*)this;
    }
    else
    {
        *ppvObj = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG)
CProgressDlg::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG)
CProgressDlg::Release()
{
    LONG cRef = InterlockedDecrement(&m_cRef);

    if (cRef == 0)
    {
        delete this;
    }

    return cRef;
}

BOOL CProgressDlg::Cancelled() const
{
    return m_bCancelled;
}

VOID CProgressDlg::SetTitle(PCTSTR pszTitle)
{
    SetWindowText(m_hDlg, pszTitle);
}

VOID CProgressDlg::SetMessage(PCTSTR pszMessage)
{
    SetDlgItemText(m_hDlg, IDC_WIAPROGR_MESSAGE, pszMessage);
}

VOID CProgressDlg::SetPercent(UINT nPercent)
{
    SendDlgItemMessage(m_hDlg, IDC_WIAPROGR_BAR, PBM_SETPOS, (WPARAM)nPercent, 0);
}

DWORD WINAPI CProgressDlg::ThreadProc(PVOID pParameter)
{
    CProgressDlg* that = (CProgressDlg*)pParameter;

    INT_PTR nResult = DialogBoxParam(
        HLanguage,
        MAKEINTRESOURCE(DLG_WIAPROGRESS),
        that->m_hWndParent,
        DialogProc,
        (LPARAM)that);

    return (DWORD)nResult;
}

INT_PTR
CALLBACK
CProgressDlg::DialogProc(
    HWND hDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // Retrieve and store the "this" pointer

        CProgressDlg* that = (CProgressDlg*)lParam;

        SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)that);

        that->m_hDlg = hDlg;

        // Initialize progress bar to the range 0 to 100

        SendDlgItemMessage(hDlg, IDC_WIAPROGR_BAR, PBM_SETRANGE32, (WPARAM)0, (LPARAM)100);

        // Signal the main thread that we are ready

        SetEvent(that->m_hInitDlg);

        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
        {
            // If the user presses the cancel button, set the m_bCancelled flag.

            CProgressDlg* that = (CProgressDlg*)GetWindowLongPtr(hDlg, DWLP_USER);

            InterlockedIncrement(&that->m_bCancelled);

            // The cancel operation will probably take some time, so
            // change the cancel button text to "wait...", so that
            // the user will see the cancel command is received and
            // is being processed

            SetDlgItemText(hDlg, IDCANCEL, LoadStr(IDS_WIA_WAIT));

            return TRUE;
        }
        }

        break;
    }
    }

    return FALSE;
}

//****************************************************************************
//
// GetBitmapHeaderSize
//

ULONG GetBitmapHeaderSize(LPCVOID pDib)
{
    ULONG nHeaderSize = *(PDWORD)pDib;

    switch (nHeaderSize)
    {
    case sizeof(BITMAPCOREHEADER):
    case sizeof(BITMAPINFOHEADER):
    case sizeof(BITMAPV4HEADER):
    case sizeof(BITMAPV5HEADER):
    {
        return nHeaderSize;
    }
    }

    return 0;
}

//****************************************************************************
//
// GetBitmapLineWidthInBytes
//

ULONG GetBitmapLineWidthInBytes(ULONG nWidthInPixels, ULONG nBitCount)
{
    return (((nWidthInPixels * nBitCount) + 31) & ~31) >> 3;
}

//****************************************************************************
//
// GetBitmapSize
//

ULONG GetBitmapSize(LPCVOID pDib)
{
    ULONG nHeaderSize = GetBitmapHeaderSize(pDib);

    if (nHeaderSize == 0)
    {
        return 0;
    }

    // Start the calculation with the header size

    ULONG nDibSize = nHeaderSize;

    // is this an old style BITMAPCOREHEADER?

    if (nHeaderSize == sizeof(BITMAPCOREHEADER))
    {
        PBITMAPCOREHEADER pbmch = (PBITMAPCOREHEADER)pDib;

        // Add the color table size

        if (pbmch->bcBitCount <= 8)
        {
            nDibSize += (ULONG)sizeof(RGBTRIPLE) * (1 << pbmch->bcBitCount);
        }

        // Add the bitmap size

        ULONG nWidth = GetBitmapLineWidthInBytes(pbmch->bcWidth, pbmch->bcBitCount);

        nDibSize += nWidth * pbmch->bcHeight;
    }
    else
    {
        // this is at least a BITMAPINFOHEADER

        PBITMAPINFOHEADER pbmih = (PBITMAPINFOHEADER)pDib;

        // Add the color table size

        if (pbmih->biClrUsed != 0)
        {
            nDibSize += sizeof(RGBQUAD) * pbmih->biClrUsed;
        }
        else if (pbmih->biBitCount <= 8)
        {
            nDibSize += (ULONG)sizeof(RGBQUAD) * (1 << pbmih->biBitCount);
        }

        // Add the bitmap size

        if (pbmih->biSizeImage != 0)
        {
            nDibSize += pbmih->biSizeImage;
        }
        else
        {
            // biSizeImage must be specified for compressed bitmaps

            if (pbmih->biCompression != BI_RGB &&
                pbmih->biCompression != BI_BITFIELDS)
            {
                return 0;
            }

            ULONG nWidth = GetBitmapLineWidthInBytes(pbmih->biWidth, pbmih->biBitCount);

            nDibSize += nWidth * abs(pbmih->biHeight);
        }

        // Consider special cases

        if (nHeaderSize == sizeof(BITMAPINFOHEADER))
        {
            // If this is a 16 or 32 bit bitmap and BI_BITFIELDS is used,
            // bmiColors member contains three DWORD color masks.
            // For V4 or V5 headers, this info is included the header

            if (pbmih->biCompression == BI_BITFIELDS)
            {
                nDibSize += 3 * sizeof(DWORD);
            }
        }
        else if (nHeaderSize >= sizeof(BITMAPV5HEADER))
        {
            // If this is a V5 header and an ICM profile is specified,
            // we need to consider the profile data size

            PBITMAPV5HEADER pbV5h = (PBITMAPV5HEADER)pDib;

            // if there is some padding before the profile data, add it

            if (pbV5h->bV5ProfileData > nDibSize)
            {
                nDibSize = pbV5h->bV5ProfileData;
            }

            // add the profile data size

            nDibSize += pbV5h->bV5ProfileSize;
        }
    }

    return nDibSize;
}

//****************************************************************************
//
// FixBitmapHeight
//

BOOL FixBitmapHeight(PVOID pDib, ULONG nSize, BOOL bTopDown)
{
    ULONG nHeaderSize = GetBitmapHeaderSize(pDib);

    if (nHeaderSize == 0)
    {
        return FALSE;
    }

    // is this an old style BITMAPCOREHEADER?

    if (nHeaderSize == sizeof(BITMAPCOREHEADER))
    {
        PBITMAPCOREHEADER pbmch = (PBITMAPCOREHEADER)pDib;

        // fix the height value if necessary

        if (pbmch->bcHeight == 0)
        {
            // start the calculation with the header size

            ULONG nSizeImage = nSize - nHeaderSize;

            // subtract the color table size

            if (pbmch->bcBitCount <= 8)
            {
                nSizeImage -= (ULONG)sizeof(RGBTRIPLE) * (1 << pbmch->bcBitCount);
            }

            // calculate the height

            ULONG nWidth = GetBitmapLineWidthInBytes(pbmch->bcWidth, pbmch->bcBitCount);

            if (nWidth == 0)
            {
                return FALSE;
            }

            LONG nHeight = nSizeImage / nWidth;

            pbmch->bcHeight = (WORD)nHeight;
        }
    }
    else
    {
        // this is at least a BITMAPINFOHEADER

        PBITMAPINFOHEADER pbmih = (PBITMAPINFOHEADER)pDib;

        // fix the height value if necessary

        if (pbmih->biHeight == 0)
        {
            // find the size of the image data

            ULONG nSizeImage;

            if (pbmih->biSizeImage != 0)
            {
                // if the size is specified in the header, take it

                nSizeImage = pbmih->biSizeImage;
            }
            else
            {
                // start the calculation with the header size

                nSizeImage = nSize - nHeaderSize;

                // subtract the color table size

                if (pbmih->biClrUsed != 0)
                {
                    nSizeImage -= sizeof(RGBQUAD) * pbmih->biClrUsed;
                }
                else if (pbmih->biBitCount <= 8)
                {
                    nSizeImage -= (ULONG)sizeof(RGBQUAD) * (1 << pbmih->biBitCount);
                }

                // Consider special cases

                if (nHeaderSize == sizeof(BITMAPINFOHEADER))
                {
                    // If this is a 16 or 32 bit bitmap and BI_BITFIELDS is used,
                    // bmiColors member contains three DWORD color masks.
                    // For V4 or V5 headers, this info is included the header

                    if (pbmih->biCompression == BI_BITFIELDS)
                    {
                        nSizeImage -= 3 * sizeof(DWORD);
                    }
                }
                else if (nHeaderSize >= sizeof(BITMAPV5HEADER))
                {
                    // If this is a V5 header and an ICM profile is specified,
                    // we need to consider the profile data size

                    PBITMAPV5HEADER pbV5h = (PBITMAPV5HEADER)pDib;

                    // add the profile data size

                    nSizeImage -= pbV5h->bV5ProfileSize;
                }

                // store the image size

                pbmih->biSizeImage = nSizeImage;
            }

            // finally, calculate the height

            ULONG nWidth = GetBitmapLineWidthInBytes(pbmih->biWidth, pbmih->biBitCount);

            if (nWidth == 0)
            {
                return FALSE;
            }

            LONG nHeight = nSizeImage / nWidth;

            pbmih->biHeight = bTopDown ? -nHeight : nHeight;
        }
    }

    return TRUE;
}

//****************************************************************************
//
// GetBitmapOffsetBits
//

ULONG GetBitmapOffsetBits(LPCVOID pDib)
{
    ULONG nHeaderSize = GetBitmapHeaderSize(pDib);

    if (nHeaderSize == 0)
    {
        return 0;
    }

    // Start the calculation with the header size

    ULONG nOffsetBits = nHeaderSize;

    // is this an old style BITMAPCOREHEADER?

    if (nHeaderSize == sizeof(BITMAPCOREHEADER))
    {
        PBITMAPCOREHEADER pbmch = (PBITMAPCOREHEADER)pDib;

        // Add the color table size

        if (pbmch->bcBitCount <= 8)
        {
            nOffsetBits += (ULONG)sizeof(RGBTRIPLE) * (1 << pbmch->bcBitCount);
        }
    }
    else
    {
        // this is at least a BITMAPINFOHEADER

        PBITMAPINFOHEADER pbmih = (PBITMAPINFOHEADER)pDib;

        // Add the color table size

        if (pbmih->biClrUsed != 0)
        {
            nOffsetBits += sizeof(RGBQUAD) * pbmih->biClrUsed;
        }
        else if (pbmih->biBitCount <= 8)
        {
            nOffsetBits += (ULONG)sizeof(RGBQUAD) * (1 << pbmih->biBitCount);
        }

        // Consider special cases

        if (nHeaderSize == sizeof(BITMAPINFOHEADER))
        {
            // If this is a 16 or 32 bit bitmap and BI_BITFIELDS is used,
            // bmiColors member contains three DWORD color masks.
            // For V4 or V5 headers, this info is included in the header

            if (pbmih->biCompression == BI_BITFIELDS)
            {
                nOffsetBits += 3 * sizeof(DWORD);
            }
        }
        else if (nHeaderSize >= sizeof(BITMAPV5HEADER))
        {
            // If this is a V5 header and an ICM profile is specified,
            // we need to consider the profile data size

            PBITMAPV5HEADER pbV5h = (PBITMAPV5HEADER)pDib;

            // if the profile data comes before the pixel data, add it

            if (pbV5h->bV5ProfileData <= nOffsetBits)
            {
                nOffsetBits += pbV5h->bV5ProfileSize;
            }
        }
    }

    return nOffsetBits;
}

//****************************************************************************
//
// FillBitmapFileHeader
//

BOOL FillBitmapFileHeader(LPCVOID pDib, PBITMAPFILEHEADER pbmfh)
{
    ULONG nSize = GetBitmapSize(pDib);

    if (nSize == 0)
    {
        return FALSE;
    }

    ULONG nOffset = GetBitmapOffsetBits(pDib);

    if (nOffset == 0)
    {
        return FALSE;
    }

    pbmfh->bfType = MAKEWORD('B', 'M');
    pbmfh->bfSize = sizeof(BITMAPFILEHEADER) + nSize;
    pbmfh->bfReserved1 = 0;
    pbmfh->bfReserved2 = 0;
    pbmfh->bfOffBits = sizeof(BITMAPFILEHEADER) + nOffset;

    return TRUE;
}

//****************************************************************************
//
// ProgressCallback
//

/*++

    The ProgressCallback function is an application-defined callback function
    used with the WiaGetImage function.


    HRESULT 
    CALLBACK 
    ProgressCallback(
        LONG   lStatus,
        LONG   lPercentComplete,
        PVOID  pParam
    );


Parameters

    lStatus
        [in] Specifies a constant that indicates the status of the WIA device.
        Can be set to one of the following values;

            IT_STATUS_TRANSFER_FROM_DEVICE: Data is currently being 
            transferred from the WIA device. 

            IT_STATUS_PROCESSING_DATA: Data is currently being processed. 

            IT_STATUS_TRANSFER_TO_CLIENT: Data is currently being transferred 
            to the client's data buffer. 

    lPercentComplete
        [in] Specifies the percentage of the total data that has been 
        transferred so far.

    pParam
        [in] Specifies the application-defined value given in WiaGetImage


Return Values
    
    To continue the data transfer, return S_OK.

    To cancel the data transfer, return S_FALSE. 

    If the method fails, return a standard COM error code.

--*/

typedef HRESULT(CALLBACK* PFNPROGRESSCALLBACK)(
    LONG lStatus,
    LONG lPercentComplete,
    PVOID pParam);

//****************************************************************************
//
// CDataCallback
//

/*++

    CDataCallback implements a WIA data callback object that stores the
    transferred data in an array of stream objects.

Methods

    CDataCallback
        Initializes the object

    BandedDataCallback
        This method is called periodically during data transfers. It can be
        called for one of these reasons;

        IT_MSG_DATA_HEADER: The method tries to allocate memory for the 
        image if the size is given in the header

        IT_MSG_DATA: The method adjusts the stream size and copies the new 
        data to the stream.

        case IT_MSG_STATUS: The method invoke the progress callback function.

        IT_MSG_TERMINATION or IT_MSG_NEW_PAGE: For BMP images, calculates the 
        image height if it is not given in the image header, fills in the 
        BITMAPFILEHEADER and stores this stream in the successfully 
        transferred images array

    ReAllocBuffer
        Increases the size of the current stream object, or creates a 
        new stream object if this is the first call.

    CopyToBuffer
        Copies new data to the current stream.

    StoreBuffer
        Stores the current stream in the array of successfully transferred
        images.

--*/

class CDataCallback : public IWiaDataCallback
{
public:
    CDataCallback(
        PFNPROGRESSCALLBACK pfnProgressCallback,
        PVOID pProgressCallbackParam,
        LONG* plCount,
        IStream*** pppStream);

    // IUnknown interface

    STDMETHOD(QueryInterface)
    (REFIID iid, LPVOID* ppvObj);
    STDMETHOD_(ULONG, AddRef)
    ();
    STDMETHOD_(ULONG, Release)
    ();

    // IWiaDataCallback interface

    STDMETHOD(BandedDataCallback)
    (
        LONG lReason,
        LONG lStatus,
        LONG lPercentComplete,
        LONG lOffset,
        LONG lLength,
        LONG lReserved,
        LONG lResLength,
        PBYTE pbBuffer);

    // CDataCallback methods

private:
    HRESULT ReAllocBuffer(ULONG nBufferSize);
    HRESULT CopyToBuffer(ULONG nOffset, LPCVOID pBuffer, ULONG nSize);
    HRESULT StoreBuffer();

private:
    LONG m_cRef;

    BOOL m_bBMP;
    LONG m_nHeaderSize;
    LONG m_nDataSize;
    CComPtr<IStream> m_pStream;

    PFNPROGRESSCALLBACK m_pfnProgressCallback;
    PVOID m_pProgressCallbackParam;

    LONG* m_plCount;
    IStream*** m_pppStream;
};

CDataCallback::CDataCallback(
    PFNPROGRESSCALLBACK pfnProgressCallback,
    PVOID pProgressCallbackParam,
    LONG* plCount,
    IStream*** pppStream)
{
    m_cRef = 0;

    m_bBMP = FALSE;
    m_nHeaderSize = 0;
    m_nDataSize = 0;

    m_pfnProgressCallback = pfnProgressCallback;
    m_pProgressCallbackParam = pProgressCallbackParam;

    m_plCount = plCount;
    m_pppStream = pppStream;
}

STDMETHODIMP CDataCallback::QueryInterface(REFIID iid, LPVOID* ppvObj)
{
    if (ppvObj == NULL)
    {
        return E_POINTER;
    }

    if (iid == IID_IUnknown)
    {
        *ppvObj = (IUnknown*)this;
    }
    else if (iid == IID_IWiaDataCallback)
    {
        *ppvObj = (IWiaDataCallback*)this;
    }
    else
    {
        *ppvObj = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG)
CDataCallback::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG)
CDataCallback::Release()
{
    LONG cRef = InterlockedDecrement(&m_cRef);

    if (cRef == 0)
    {
        delete this;
    }

    return cRef;
}

STDMETHODIMP CDataCallback::BandedDataCallback(
    LONG lReason,
    LONG lStatus,
    LONG lPercentComplete,
    LONG lOffset,
    LONG lLength,
    LONG, LONG,
    PBYTE pbBuffer)
{
    HRESULT hr;

    // Parse the message

    switch (lReason)
    {
    case IT_MSG_DATA_HEADER:
    {
        PWIA_DATA_CALLBACK_HEADER pHeader = (PWIA_DATA_CALLBACK_HEADER)pbBuffer;

        // Determine if this is a BMP transfer

        m_bBMP = pHeader->guidFormatID == WiaImgFmt_MEMORYBMP || pHeader->guidFormatID == WiaImgFmt_BMP;

        // For WiaImgFmt_MEMORYBMP transfers, WIA does not send a BITMAPFILEHEADER before the data.
        // In this program, we desire all BMP files to contain a BITMAPFILEHEADER, so add it manually

        m_nHeaderSize = pHeader->guidFormatID == WiaImgFmt_MEMORYBMP ? sizeof(BITMAPFILEHEADER) : 0;

        // Allocate memory for the image if the size is given in the header

        if (pHeader != NULL && pHeader->lBufferSize != 0)
        {
            hr = ReAllocBuffer(m_nHeaderSize + pHeader->lBufferSize);

            if (FAILED(hr))
            {
                return hr;
            }
        }

        break;
    }

    case IT_MSG_DATA:
    {
        // Invoke the callback function

        hr = m_pfnProgressCallback(lStatus, lPercentComplete, m_pProgressCallbackParam);

        if (FAILED(hr) || hr == S_FALSE)
        {
            return hr;
        }

        // If the buffer is not allocated yet and this is the first block,
        // and the transferred image is in BMP format, allocate the buffer
        // according to the size information in the bitmap header

        if (m_pStream == NULL && lOffset == 0 && m_bBMP)
        {
            LONG nBufferSize = GetBitmapSize(pbBuffer);

            if (nBufferSize != 0)
            {
                hr = ReAllocBuffer(m_nHeaderSize + nBufferSize);

                if (FAILED(hr))
                {
                    return hr;
                }
            }
        }

        if (m_nHeaderSize + lOffset + lLength < 0)
        {
            return E_OUTOFMEMORY;
        }

        // If the transfer goes past the buffer, try to expand it
        if (m_nHeaderSize + lOffset + lLength > m_nDataSize)
        {
            hr = ReAllocBuffer(m_nHeaderSize + lOffset + lLength);

            if (FAILED(hr))
            {
                return hr;
            }
        }

        // copy the transfer buffer

        hr = CopyToBuffer(m_nHeaderSize + lOffset, pbBuffer, lLength);

        if (FAILED(hr))
        {
            return hr;
        }

        break;
    }

    case IT_MSG_STATUS:
    {
        // Invoke the callback function

        hr = m_pfnProgressCallback(lStatus, lPercentComplete, m_pProgressCallbackParam);

        if (FAILED(hr) || hr == S_FALSE)
        {
            return hr;
        }

        break;
    }

    case IT_MSG_TERMINATION:
    case IT_MSG_NEW_PAGE:
    {
        if (m_pStream != NULL)
        {
            // For BMP files, we should validate the the image header
            // So, obtain the memory buffer from the stream

            if (m_bBMP)
            {
                // Since the stream is created using CreateStreamOnHGlobal,
                // we can get the memory buffer with GetHGlobalFromStream.

                HGLOBAL hBuffer;

                hr = GetHGlobalFromStream(m_pStream, &hBuffer);

                if (FAILED(hr))
                {
                    return hr;
                }

                PBITMAPFILEHEADER pBuffer = (PBITMAPFILEHEADER)GlobalLock(hBuffer);

                if (pBuffer == NULL)
                {
                    return HRESULT_FROM_WIN32(GetLastError());
                }

                // Some scroll-fed scanners may return 0 as the bitmap height
                // In this case, calculate the image height and modify the header

                FixBitmapHeight(pBuffer + 1, m_nDataSize, TRUE);

                // For WiaImgFmt_MEMORYBMP transfers, the WIA service does not
                // include a BITMAPFILEHEADER preceeding the bitmap data.
                // In this case, fill in the BITMAPFILEHEADER structure.

                if (m_nHeaderSize != 0)
                {
                    FillBitmapFileHeader(pBuffer + 1, pBuffer);
                }

                GlobalUnlock(hBuffer);
            }

            // Store this buffer in the successfully transferred images array

            hr = StoreBuffer();

            if (FAILED(hr))
            {
                return hr;
            }
        }

        break;
    }
    }

    return S_OK;
}

HRESULT CDataCallback::ReAllocBuffer(ULONG nSize)
{
    HRESULT hr;

    // If m_pStream is not initialized yet, create a new stream object

    if (m_pStream == 0)
    {
        hr = CreateStreamOnHGlobal(0, TRUE, &m_pStream);

        if (FAILED(hr))
        {
            return hr;
        }
    }

    // Next, set the size of the stream object

    ULARGE_INTEGER liSize = {nSize};

    hr = m_pStream->SetSize(liSize);

    if (FAILED(hr))
    {
        return hr;
    }

    m_nDataSize = nSize;

    return S_OK;
}

HRESULT CDataCallback::CopyToBuffer(ULONG nOffset, LPCVOID pBuffer, ULONG nSize)
{
    HRESULT hr;

    // First move the stream pointer to the data offset

    LARGE_INTEGER liOffset = {nOffset};

    hr = m_pStream->Seek(liOffset, STREAM_SEEK_SET, 0);

    if (FAILED(hr))
    {
        return hr;
    }

    // Next, write the new data to the stream

    hr = m_pStream->Write(pBuffer, nSize, 0);

    if (FAILED(hr))
    {
        return hr;
    }

    return S_OK;
}

HRESULT CDataCallback::StoreBuffer()
{
    // Increase the successfully transferred buffers array size

    int nAllocSize = (*m_plCount + 1) * sizeof(IStream*);
    IStream** ppStream = (IStream**)CoTaskMemRealloc(
        *m_pppStream,
        nAllocSize);

    if (ppStream == NULL)
    {
        return E_OUTOFMEMORY;
    }

    *m_pppStream = ppStream;

    // Rewind the current buffer

    LARGE_INTEGER liZero = {0};

    m_pStream->Seek(liZero, STREAM_SEEK_SET, 0);

    // Store the current buffer as the last successfully transferred buffer

    if (*m_plCount < 0 || *m_plCount > nAllocSize)
    {
        return E_OUTOFMEMORY;
    }

    (*m_pppStream)[*m_plCount] = m_pStream;
    (*m_pppStream)[*m_plCount]->AddRef();

    *m_plCount += 1;

    // Reset the current buffer

    m_pStream.Release();

    m_nDataSize = 0;

    return S_OK;
}

//****************************************************************************
//
// DefaultProgressCallback
//

HRESULT
CALLBACK
DefaultProgressCallback(
    LONG lStatus,
    LONG lPercentComplete,
    PVOID pParam)
{
    CProgressDlg* pProgressDlg = (CProgressDlg*)pParam;

    if (pProgressDlg == NULL)
    {
        return E_POINTER;
    }

    // If the user has pressed the cancel button, abort transfer

    if (pProgressDlg->Cancelled())
    {
        return S_FALSE;
    }

    // Form the message text

    UINT uID;

    switch (lStatus)
    {
    case IT_STATUS_TRANSFER_FROM_DEVICE:
    {
        uID = IDS_WIA_STATUS_TRANSFER_FROM_DEVICE;
        break;
    }

    case IT_STATUS_PROCESSING_DATA:
    {
        uID = IDS_WIA_STATUS_PROCESSING_DATA;
        break;
    }

    case IT_STATUS_TRANSFER_TO_CLIENT:
    {
        uID = IDS_WIA_STATUS_TRANSFER_TO_CLIENT;
        break;
    }

    default:
    {
        return E_INVALIDARG;
    }
    }

    TCHAR szStatusText[300];

    _sntprintf_s(szStatusText, _TRUNCATE, LoadStr(uID), lPercentComplete);

    // Update the progress bar values

    pProgressDlg->SetMessage(szStatusText);

    pProgressDlg->SetPercent(lPercentComplete);

    return S_OK;
}

//****************************************************************************
//
// WiaGetImage
//

/*++

    The WiaGetImage function displays one or more dialog boxes that enable a 
    user to acquire multiple images from a WIA device and return them in an
    array of IStream interfaces. This method combines the functionality of 
    SelectDeviceDlg, DeviceDlg and idtGetBandedData API's to completely 
    encapsulate image acquisition within a single function call.


    HRESULT 
    WiaGetImage(
        HWND                 hWndParent,
        LONG                 lDeviceType,
        LONG                 lFlags,
        LONG                 lIntent,
        IWiaDevMgr          *pSuppliedWiaDevMgr,
        IWiaItem            *pSuppliedItemRoot,
        PFNPROGRESSCALLBACK  pfnProgressCallback,
        PVOID                pProgressCallbackParam,
        GUID                *pguidFormat,
        LONG                *plCount,
        IStream             ***pppStream
    );


Parameters

    hwndParent
        [in] Handle of the window that will own the dialog boxes.

    lDeviceType 
        [in] Specifies which type of WIA device to use. Can be set to one of 
        the following values;

            StiDeviceTypeDefault
            StiDeviceTypeScanner
            StiDeviceTypeDigitalCamera
            StiDeviceTypeStreamingVideo 

    lFlags 
        [in] Specifies dialog box behavior. Can be set to any combination of 
        the following values;

            0: Default behavior. 

            WIA_SELECT_DEVICE_NODEFAULT: Force this method to display the 
            Select Device dialog box. If this flag is not present, and if
            WiaGetImage finds only one matching device, it will not display 
            the Select Device dialog box. 

            WIA_DEVICE_DIALOG_SINGLE_IMAGE: Restrict image selection to a 
            single image in the device image acquisition dialog box.

            WIA_DEVICE_DIALOG_USE_COMMON_UI: Use the system UI, if available,
            rather than the vendor-supplied UI. If the system UI is not 
            available, the vendor UI is used. If neither UI is available, 
            the function returns E_NOTIMPL.

    lIntent 
        [in] Specifies what type of data the image is intended to represent. 

    pSuppliedWiaDevMgr
        [in] Pointer to the interface of the WIA device manager. This 
        interface is used when WiaGetImage displays the Select Device dialog 
        box. If the application passes NULL for this parameter, WiaGetImage 
        connects to the local WIA device manager.

    pSuppliedItemRoot 
        [in] Pointer to the interface of the hierarchical tree of IWiaItem 
        objects. If the application passes NULL for this parameter, 
        WiaGetImage displays the Select Device dialog box that lets the user 
        select the WIA input device. If the application specifies a WIA input 
        device by passing a valid pointer to the device's item tree for this 
        parameter, WiaGetImage does not display the Select Device dialog box, 
        instead, it will use the specified input device to acquire the image.

    pfnProgressCallback
        [in] Specifies the address of a callback function of type 
        PFNPROGRESSCALLBACK that is called periodically to provide data 
        transfer status notifications. If the application passes NULL
        for this parameter, WiaGetImage displays a simple progress dialog 
        with a status message, a progress bar and a cancel button. If the 
        application passes a valid function, WiaGetImage does not display
        this progress dialog, instead, it calls the specified function with
        the status message and the completion percentage values.

    pProgressCallbackParam
        [in] Specifies an argument to be passed to the callback function. 
        The value of the argument is specified by the application. This 
        parameter can be NULL. 

    pguidFormat (changed: in GetImage sample it is IN/OUT)
        [out] On output, holds the format used.

    plCount
        [out] Receives the number of items in the array indicated by the 
        pppStream parameter. 

    pppStream
        [out] Receives the address of an array of pointers to IStream 
        interfaces. Applications must call the IStream::Release method 
        for each element in the array of interface pointers they receive. 
        Applications must also free the pppStream array itself using 
        CoTaskMemFree.


Return Values
    
    WiaGetImage returns one of the following values or a standard COM error 
    if it fails for any other reason.

    S_OK: if the data is transferred successfully.

    S_FALSE: if the user cancels one of the device selection, image selection
    or image transfer dialog boxes.

    WIA_S_NO_DEVICE_AVAILABLE: if no WIA device is currently available.
    
    E_NOTIMPL: if no UI is available.

    
Remarks

    WiaGetImage returns the transferred images as stream objects in the 
    pppStream array parameter. The array is created with CoTaskMemAlloc 
    and the stream objects are created with CreateStreamOnHGlobal. The array 
    will contain a single entry if the WIA_DEVICE_DIALOG_SINGLE_IMAGE flag 
    is specified. Otherwise, it may contain one or more entries and the 
    count will be returned in the plCount parameter.

    The stream object will contain the image data. To create a GDI+ image 
    object from the stream object, use the Gdiplus::Image(IStream* stream)
    function. To obtain a pointer to the memory address of the data, use 
    the GetHGlobalFromStream API to obtain an HGLOBAL and use GlobalLock
    to obtain a direct pointer to the data.

--*/

HRESULT
WiaGetImage(
    HWND hWndParent,
    LONG lDeviceType,
    LONG lFlags,
    LONG lIntent,
    IWiaDevMgr* pSuppliedWiaDevMgr,
    IWiaItem* pSuppliedItemRoot,
    PFNPROGRESSCALLBACK pfnProgressCallback,
    PVOID pProgressCallbackParam,
    GUID* pguidFormat,
    LONG* plCount,
    IStream*** pppStream)
{
    HRESULT hr;

    // Validate and initialize output parameters

    GUID guidFormat = GUID_NULL;

    if (pguidFormat == NULL)
    {
        pguidFormat = &guidFormat;
    }

    *pguidFormat = WiaImgFmt_MEMORYBMP; // we start with WiaImgFmt_MEMORYBMP, if it fails, we try WiaImgFmt_BMP

    if (plCount == NULL)
    {
        return E_POINTER;
    }

    if (pppStream == NULL)
    {
        return E_POINTER;
    }

    *plCount = 0;
    *pppStream = NULL;

    // Initialize the local root item variable with the supplied value.
    // If no value is supplied, display the device selection common dialog.

    CComPtr<IWiaItem> pItemRoot = pSuppliedItemRoot;

    if (pItemRoot == NULL)
    {
        // Initialize the device manager pointer with the supplied value
        // If no value is supplied, connect to the local device manager

        CComPtr<IWiaDevMgr> pWiaDevMgr = pSuppliedWiaDevMgr;

        if (pWiaDevMgr == NULL)
        {
            hr = pWiaDevMgr.CoCreateInstance(CLSID_WiaDevMgr);

            if (FAILED(hr))
            {
                return hr;
            }
        }

        // Display the device selection common dialog

        hr = pWiaDevMgr->SelectDeviceDlg(
            hWndParent,
            lDeviceType,
            (lFlags & WIA_SELECT_DEVICE_NODEFAULT),
            NULL,
            &pItemRoot);

        if (FAILED(hr) || hr == S_FALSE)
        {
            return hr;
        }
    }

    // Display the image selection common dialog

    CComPtrArray<IWiaItem> ppIWiaItem;

    hr = pItemRoot->DeviceDlg(
        hWndParent,
        (lFlags & (WIA_DEVICE_DIALOG_SINGLE_IMAGE | WIA_DEVICE_DIALOG_USE_COMMON_UI)),
        lIntent,
        &ppIWiaItem.Count(),
        &ppIWiaItem);

    if (FAILED(hr) || hr == S_FALSE)
    {
        return hr;
    }

    // For ADF scanners, the common dialog explicitly sets the page count to one.
    // So in order to transfer multiple images, set the page count to ALL_PAGES
    // if the WIA_DEVICE_DIALOG_SINGLE_IMAGE flag is not specified,

    if (!(lFlags & WIA_DEVICE_DIALOG_SINGLE_IMAGE))
    {
        // Get the property storage interface pointer for the root item

        CComQIPtr<IWiaPropertyStorage> pWiaRootPropertyStorage(pItemRoot);

        if (pWiaRootPropertyStorage == NULL)
        {
            return E_NOINTERFACE;
        }

        // Determine if the selected device is a scanner or not

        PROPSPEC specDevType;

        specDevType.ulKind = PRSPEC_PROPID;
        specDevType.propid = WIA_DIP_DEV_TYPE;

        LONG nDevType;

        hr = ReadPropertyLong(
            pWiaRootPropertyStorage,
            &specDevType,
            &nDevType);

        if (SUCCEEDED(hr) && (GET_STIDEVICE_TYPE(nDevType) == StiDeviceTypeScanner))
        {
            // Determine if the document feeder is selected or not

            PROPSPEC specDocumentHandlingSelect;

            specDocumentHandlingSelect.ulKind = PRSPEC_PROPID;
            specDocumentHandlingSelect.propid = WIA_DPS_DOCUMENT_HANDLING_SELECT;

            LONG nDocumentHandlingSelect;

            hr = ReadPropertyLong(
                pWiaRootPropertyStorage,
                &specDocumentHandlingSelect,
                &nDocumentHandlingSelect);

            if (SUCCEEDED(hr) && (nDocumentHandlingSelect & FEEDER))
            {
                PROPSPEC specPages;

                specPages.ulKind = PRSPEC_PROPID;
                specPages.propid = WIA_DPS_PAGES;

                PROPVARIANT varPages;

                varPages.vt = VT_I4;
                varPages.lVal = ALL_PAGES;

                pWiaRootPropertyStorage->WriteMultiple(
                    1,
                    &specPages,
                    &varPages,
                    WIA_DPS_FIRST);

                PropVariantClear(&varPages);
            }
        }
    }

    // If a status callback function is not supplied, use the default.
    // The default displays a simple dialog with a progress bar and cancel button.

    CComPtr<CProgressDlg> pProgressDlg;

    if (pfnProgressCallback == NULL)
    {
        pfnProgressCallback = DefaultProgressCallback;

        pProgressDlg = new CProgressDlg(hWndParent);

        pProgressCallbackParam = (CProgressDlg*)pProgressDlg;
    }

    // Create the data callback interface

    CComPtr<CDataCallback> pDataCallback = new CDataCallback(
        pfnProgressCallback,
        pProgressCallbackParam,
        plCount,
        pppStream);

    if (pDataCallback == NULL)
    {
        return E_OUTOFMEMORY;
    }

    // Start the transfer of the selected items

    for (int i = 0; i < ppIWiaItem.Count(); ++i)
    {
        // Get the interface pointers

        CComQIPtr<IWiaPropertyStorage> pWiaPropertyStorage(ppIWiaItem[i]);

        if (pWiaPropertyStorage == NULL)
        {
            return E_NOINTERFACE;
        }

        CComQIPtr<IWiaDataTransfer> pIWiaDataTransfer(ppIWiaItem[i]);

        if (pIWiaDataTransfer == NULL)
        {
            return E_NOINTERFACE;
        }

        // Set the transfer type

        PROPSPEC specTymed;

        specTymed.ulKind = PRSPEC_PROPID;
        specTymed.propid = WIA_IPA_TYMED;

        PROPVARIANT varTymed;

        varTymed.vt = VT_I4;
        varTymed.lVal = TYMED_CALLBACK;

        hr = pWiaPropertyStorage->WriteMultiple(
            1,
            &specTymed,
            &varTymed,
            WIA_IPA_FIRST);

        PropVariantClear(&varTymed);

        if (FAILED(hr))
        {
            return hr;
        }
        /*
        // part of GetImage sample, *pguidFormat cannot be GUID_NULL, so it's commented here:
        // If there is no transfer format specified, use the device default

        if (*pguidFormat == GUID_NULL)
        {
            PROPSPEC specPreferredFormat;

            specPreferredFormat.ulKind = PRSPEC_PROPID;
            specPreferredFormat.propid = WIA_IPA_PREFERRED_FORMAT;

            hr = ReadPropertyGuid(
                pWiaPropertyStorage,
                &specPreferredFormat,
                pguidFormat
            );

            if (FAILED(hr))
            {
                return hr;
            }
        }
*/
        // Set the transfer format

        PROPSPEC specFormat[2];

        specFormat[0].ulKind = PRSPEC_PROPID;
        specFormat[0].propid = WIA_IPA_FORMAT;

        PROPVARIANT varFormat[2];

        varFormat[0].vt = VT_CLSID;
        varFormat[0].puuid = (CLSID*)CoTaskMemAlloc(sizeof(CLSID));

        if (varFormat[0].puuid == NULL)
        {
            return E_OUTOFMEMORY;
        }

        *varFormat[0].puuid = *pguidFormat;

        hr = pWiaPropertyStorage->WriteMultiple(
            1,
            specFormat,
            varFormat,
            WIA_IPA_FIRST);

        if (FAILED(hr))
        {
            TRACE_E("WriteMultiple() failed (0x" << std::hex << hr << std::dec << "), trying to set format to WiaImgFmt_BMP");

            *varFormat[0].puuid = WiaImgFmt_BMP;

            // repeating of transfer type specification is required for Canon LiDE 600F on x64 AS/W7
            // when WiaImgFmt_BMP format is required, otherwise we receive hr==0x80070057
            specFormat[1].ulKind = PRSPEC_PROPID;
            specFormat[1].propid = WIA_IPA_TYMED;
            varFormat[1].vt = VT_I4;
            varFormat[1].lVal = TYMED_CALLBACK;

            HRESULT hr2 = pWiaPropertyStorage->WriteMultiple(
                2,
                specFormat,
                varFormat,
                WIA_IPA_FIRST);

            if (FAILED(hr2))
            {
                TRACE_E("WriteMultiple() failed again (0x" << std::hex << hr2 << std::dec << "), giving up...");
            }
            else
            {
                hr = hr2;                     // success
                *pguidFormat = WiaImgFmt_BMP; // format changed to WiaImgFmt_BMP
            }

            PropVariantClear(&varFormat[1]);
        }

        PropVariantClear(&varFormat[0]); // varFormat[1] is not used in this scope

        if (FAILED(hr))
        {
            return hr;
        }

        // Read the transfer buffer size from the device, default to 64K

        PROPSPEC specBufferSize;

        specBufferSize.ulKind = PRSPEC_PROPID;
        specBufferSize.propid = WIA_IPA_BUFFER_SIZE;

        LONG nBufferSize;

        hr = ReadPropertyLong(
            pWiaPropertyStorage,
            &specBufferSize,
            &nBufferSize);

        if (FAILED(hr))
        {
            nBufferSize = 64 * 1024;
        }

        // Choose double buffered transfer for better performance

        WIA_DATA_TRANSFER_INFO WiaDataTransferInfo = {0};

        WiaDataTransferInfo.ulSize = sizeof(WIA_DATA_TRANSFER_INFO);
        WiaDataTransferInfo.ulBufferSize = 2 * nBufferSize;
        WiaDataTransferInfo.bDoubleBuffer = TRUE;

        // Start the transfer

        hr = pIWiaDataTransfer->idtGetBandedData(
            &WiaDataTransferInfo,
            pDataCallback);

        if (FAILED(hr) || hr == S_FALSE)
        {
            return hr;
        }
    }

    return S_OK;
}

//****************************************************************************
//
// CWiaWrap
//

CWiaWrap::CWiaWrap()
{
    WiaWrapUsed = FALSE;
    WiaWrapOK = TRUE;
    MyThreadID = GetCurrentThreadId();
    AcquiringImage = FALSE;
    ShouldCloseParentAfterAcquiring = FALSE;
}

CWiaWrap::~CWiaWrap()
{
    if (MyThreadID != GetCurrentThreadId())
    {
        TRACE_E("Invalid call to CWiaWrap::~CWiaWrap(). Wrong thread!");
        return;
    }
    if (WiaWrapUsed)
        CoUninitialize(); // we must call CoUninitialize() even if CoInitialize() has failed
}

HBITMAP
CWiaWrap::AcquireImage(HWND parent, TDirectArray<HBITMAP>** extraScanImages, BOOL* closeParent)
{
    if (MyThreadID != GetCurrentThreadId())
    {
        TRACE_E("Invalid call to CWiaWrap::AcquireImage. Wrong thread!");
        return NULL;
    }
    if (!WiaWrapUsed)
    {
        WiaWrapUsed = TRUE;

        // Initialize the COM library
        if (FAILED(CoInitialize(NULL)))
            WiaWrapOK = FALSE;
    }

    if (!WiaWrapOK)
        return NULL;

    AcquiringImage = TRUE;
    ShouldCloseParentAfterAcquiring = FALSE; // prevence proti necekanemu zavreni parenta (zatim o to neslo pozadat)

    // Launch the get image dialog

    CComPtrArray<IStream> ppStream;

    // Default formats returned by scanners we have:
    // WiaImgFmt_BMP from CANON LiDE 600F
    // WiaImgFmt_MEMORY BMP from CANON LiDE 200, CANON 9000F, CANON MD8030Cn, and CANON MF8500C
    //
    // So we have decided to use WiaImgFmt_MEMORYBMP and if it fails, we try to use WiaImgFmt_BMP
    // (with repeating of transfer type specification which is required for Canon LiDE 600F on x64 AS/W7
    //  for WiaImgFmt_BMP format, otherwise we receive hr==0x80070057)

    GUID guidFormat; // used format will be stored here

    HRESULT hr = WiaGetImage(parent,
                             StiDeviceTypeDefault,
                             0,
                             WIA_INTENT_NONE,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             &guidFormat,
                             &ppStream.Count(),
                             &ppStream);

    if (hr == S_FALSE)
    {
        AcquiringImage = FALSE;
        if (ShouldCloseParentAfterAcquiring)
        {
            if (closeParent != NULL)
                *closeParent = TRUE; // napr. nepocitat enablery, nezdrzovat
            ShouldCloseParentAfterAcquiring = FALSE;
        }
        return NULL; // Cancel
    }

    if (SUCCEEDED(hr) && guidFormat != WiaImgFmt_BMP && guidFormat != WiaImgFmt_MEMORYBMP)
    {
        TRACE_E("CWiaWrap::AcquireImage(): unexpected format of acquired image (not BMP).");
        hr = HRESULT_FROM_WIN32(ERROR_INVALID_PIXEL_FORMAT);
    }

    HBITMAP hFirstBitmap = NULL;
    if (SUCCEEDED(hr) && ppStream.Count() > 0)
    {
        HDC hDC = GetDC(NULL);
        if (hDC != NULL)
        {
            for (int i = 0; SUCCEEDED(hr) && i < ppStream.Count(); i++)
            {
                HGLOBAL hBuffer;
                hr = GetHGlobalFromStream(ppStream[i], &hBuffer);
                if (SUCCEEDED(hr))
                {
                    PBITMAPFILEHEADER pBmfh = (PBITMAPFILEHEADER)GlobalLock(hBuffer);
                    if (pBmfh != NULL)
                    {
                        PBITMAPINFOHEADER pBmih = (PBITMAPINFOHEADER)(pBmfh + 1); // pointer behind BITMAPFILEHEADER
                        void* bits;
                        HBITMAP hBitmap = CreateDIBSection(hDC, (PBITMAPINFO)pBmih, DIB_RGB_COLORS, &bits, NULL, 0);
                        if (hBitmap != NULL)
                        {
                            int paletteSize = 0;
                            switch (pBmih->biBitCount)
                            {
                            case 1:
                                paletteSize = 2;
                                break;
                            case 4:
                                paletteSize = 16;
                                break;
                            case 8:
                                paletteSize = 256;
                                break;
                            }
                            // let's copy bitmap data
                            DWORD bytes = ((pBmih->biBitCount * pBmih->biWidth + 31) >> 3) & ~3;
                            memcpy(bits, (BYTE*)pBmih + sizeof(BITMAPINFOHEADER) + paletteSize * sizeof(RGBQUAD), abs(pBmih->biHeight) * bytes);

                            if (i == 0)
                                hFirstBitmap = hBitmap;
                            else
                            {
                                // SCAN MULTIPAGE - all images except the first one is stored in 'extraScanImages' for later handling
                                if (extraScanImages != NULL)
                                {
                                    if (*extraScanImages == NULL)
                                        *extraScanImages = new TDirectArray<HBITMAP>(10, 30);
                                    (*extraScanImages)->Add(hBitmap);
                                }
                                else
                                    DeleteObject(hBitmap); // we should delete extra images
                            }
                            hBitmap = NULL;
                        }
                        else
                            hr = HRESULT_FROM_WIN32(GetLastError());

                        GlobalUnlock(hBuffer);
                    }
                    else
                        hr = HRESULT_FROM_WIN32(GetLastError());
                    // release source bitmap, we have already copied it
                    ppStream[i]->Release();
                    ppStream[i] = NULL; // prevent second Release()
                }
            }
            ReleaseDC(NULL, hDC);
        }
        else
            hr = HRESULT_FROM_WIN32(GetLastError());
    }

    // If there was an error, display an error message box
    if (FAILED(hr))
    {
        _com_error err(hr);
        char buf[1000];
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_WIA_ERROR_ACQUIREIMG), hr, err.ErrorMessage());
        SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_PLUGINNAME), MB_ICONEXCLAMATION | MB_OK);
    }
    AcquiringImage = FALSE;
    if (ShouldCloseParentAfterAcquiring)
    {
        if (closeParent != NULL)
            *closeParent = TRUE; // napr. nepocitat enablery, nezdrzovat
        ShouldCloseParentAfterAcquiring = FALSE;
    }
    return hFirstBitmap;
}

#endif // ENABLE_WIA
