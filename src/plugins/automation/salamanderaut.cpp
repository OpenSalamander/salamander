// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	salamanderaut.cpp
	Salamander root automation object.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "salamanderaut.h"
#include "aututils.h"
#include "inputbox.h"
#include "versinfo.rh2"
#include "scriptlist.h"
#include "fileinfo.h"
#include "panelaut.h"
#include "progressaut.h"
#include "waitwndaut.h"
#include "guinamespace.h"
#include "scriptinfoaut.h"
#include "persistence.h"
#include "abortmodal.h"

extern CSalamanderGeneralAbstract* SalamanderGeneral;
extern int SalamanderVersion;
extern HINSTANCE g_hInstance;
extern HINSTANCE g_hLangInst;
extern CPersistentValueStorage g_oPersistentStorage;

CSalamanderAutomation::CSalamanderAutomation(__in CScriptInfo* pScriptInfo)
{
    m_apPanels[0] = NULL;
    m_apPanels[1] = NULL;
    m_pScriptInfo = pScriptInfo;
    m_pExecInfo = NULL;
    m_pProgress = NULL;
    m_pWaitWindow = NULL;
    m_pGui = NULL;
    m_pScriptAut = NULL;
}

CSalamanderAutomation::~CSalamanderAutomation()
{
    if (m_apPanels[0])
    {
        m_apPanels[0]->Release();
    }

    if (m_apPanels[1])
    {
        m_apPanels[1]->Release();
    }
}

struct ABORTABLE_MSGBOXEX_PARAMS
{
    MSGBOXEX_PARAMS Params;
    int nDlgResult;
};

static void WINAPI ShowMsgBoxProc(void* pContext)
{
    ABORTABLE_MSGBOXEX_PARAMS* msgbox = reinterpret_cast<ABORTABLE_MSGBOXEX_PARAMS*>(pContext);
    msgbox->nDlgResult = SalamanderGeneral->SalMessageBoxEx(&msgbox->Params);
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::MsgBox(
    /* [in] */ BSTR prompt,
    /* [optional][in] */ VARIANT* buttons,
    /* [optional][in] */ VARIANT* title,
    /* [retval][out] */ int* result)
{
    ABORTABLE_MSGBOXEX_PARAMS msgbox = {
        0,
    };
    _bstr_t promptT(prompt);
    _bstr_t titleT;
    int buttonsN = 0;
    HRESULT hr;

    msgbox.Params.HParent = SalamanderGeneral->GetMsgBoxParent();
    msgbox.Params.Text = promptT;

    if (IsArgumentPresent(buttons))
    {
        try
        {
            buttonsN = _variant_t(buttons);
        }
        catch (_com_error& e)
        {
            return e.Error();
        }
    }

    if (IsArgumentPresent(title))
    {
        titleT = title;
        msgbox.Params.Caption = titleT;
    }

    msgbox.Params.Flags = buttonsN;

    hr = AbortableModalDialogWrapper(m_pScriptInfo, msgbox.Params.HParent, ShowMsgBoxProc, &msgbox);
    if (FAILED(hr))
    {
        if (hr == SALAUT_E_ABORT)
        {
            RaiseError(IDS_E_SCRIPTABORTED);
        }
        return hr;
    }

    if (msgbox.nDlgResult == 0)
    {
        // the most probable reason of failure is invalid arguments
        // to SalMessageBoxEx
        return E_INVALIDARG;
    }

    *result = msgbox.nDlgResult;

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_LeftPanel(
    /* [retval][out] */ ISalamanderPanel** result)
{
    return GetPanel(PANEL_LEFT, result);
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_RightPanel(
    /* [retval][out] */ ISalamanderPanel** result)
{
    return GetPanel(PANEL_RIGHT, result);
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_SourcePanel(
    /* [retval][out] */ ISalamanderPanel** result)
{
    return GetPanel(PANEL_SOURCE, result);
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_TargetPanel(
    /* [retval][out] */ ISalamanderPanel** result)
{
    return GetPanel(PANEL_TARGET, result);
}

HRESULT CSalamanderAutomation::GetPanel(int nPanel, ISalamanderPanel** ppPanel)
{
    int iPanel;

    if (nPanel == PANEL_SOURCE || nPanel == PANEL_TARGET)
    {
        int nPanelCanonic;

        nPanelCanonic = SalamanderGeneral->GetSourcePanel();
        _ASSERTE(nPanelCanonic == PANEL_LEFT || nPanelCanonic == PANEL_RIGHT);

        // target panel is the second panel
        if (nPanel == PANEL_TARGET)
        {
            if (nPanelCanonic == PANEL_LEFT)
            {
                nPanelCanonic = PANEL_RIGHT;
            }
            else
            {
                nPanelCanonic = PANEL_LEFT;
            }
        }

        nPanel = nPanelCanonic;
    }

    iPanel = nPanel - PANEL_LEFT;
    _ASSERTE(iPanel >= 0 && iPanel < _countof(m_apPanels));

    if (m_apPanels[iPanel] == NULL)
    {
        m_apPanels[iPanel] = new CSalamanderPanelAutomation(nPanel);
    }

    *ppPanel = m_apPanels[iPanel];
    (*ppPanel)->AddRef();

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_Version(
    /* [retval][out] */ int* version)
{
    *version = SalamanderVersion;
    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::InputBox(
    /* [in] */ BSTR prompt,
    /* [optional][in] */ VARIANT* title,
    /* [optional][in] */ VARIANT* defresponse,
    /* [retval][out] */ BSTR* result)
{
    _bstr_t titleT;
    _bstr_t defT;

    if (IsArgumentPresent(title))
    {
        titleT = title;
    }

    if (IsArgumentPresent(defresponse))
    {
        defT = defresponse;
    }

    CInputBoxDlg dlg(m_pScriptInfo, prompt, titleT, defT);
    dlg.DoModal(IDD_INPUTBOX, SalamanderGeneral->GetMsgBoxParent());

    if (dlg.WasAborted())
    {
        *result = NULL;
        RaiseError(IDS_E_SCRIPTABORTED);
        return SALAUT_E_ABORT;
    }

    *result = dlg.GetResponse();
    if (*result == NULL)
    {
        *result = SysAllocString(L"");
    }

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_WindowsVersion(
    /* [retval][out] */ int* version)
{
    static OSVERSIONINFOEX osver = {0};
    static BOOL salGetVersionExCached = FALSE;

    if (!salGetVersionExCached)
    {
        SalGetVersionEx(&osver, TRUE); // !!! SLOW, original GetVersionEx() is deprecated !!!
        salGetVersionExCached = TRUE;
    }

    *version = osver.dwMajorVersion * 10 + osver.dwMinorVersion;

    return S_OK;
}

_STATIC_ASSERT(VERSINFO_MAJOR >= 1);
_STATIC_ASSERT(VERSINFO_MINORA < 10);
_STATIC_ASSERT(VERSINFO_MINORB == 0);

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_AutomationVersion(
    /* [retval][out] */ int* version)
{
    *version = VERSINFO_MAJOR * 10 + VERSINFO_MINORA;
    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::Sleep(
    /* [in] */ int ms)
{
    HRESULT hr = S_OK;

    if (ms >= 0)
    {
        _ASSERTE(m_pScriptInfo != NULL);

        HANDLE hAbortEvent = m_pScriptInfo->GetAbortEvent();
        _ASSERTE(hAbortEvent != NULL);

        DWORD dwWait = WaitForSingleObject(hAbortEvent, ms);
        if (dwWait == WAIT_OBJECT_0)
        {
            RaiseError(IDS_E_SCRIPTABORTED);
            hr = SALAUT_E_ABORT;
        }
    }

    return hr;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::AbortScript(void)
{
    return m_pScriptInfo->AbortScript();
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::TraceI(
    /* [in] */ BSTR message)
{
    return TraceWorker(TraceInfo, message);
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::TraceE(
    /* [in] */ BSTR message)
{
    return TraceWorker(TraceError, message);
}

HRESULT CSalamanderAutomation::TraceWorker(
    __in TraceLevel level,
    __in BSTR message)
{
    _bstr_t messageT = message;
    A2OLE sFileW(m_pScriptInfo->GetFileName());
    int line = 0;

    // TraceConnectToServer should not be called here!
    //SalamanderDebug->TraceConnectToServer();

    if (level == TraceInfo)
    {
        SalamanderDebug->TraceIW(sFileW, line, messageT);
    }
    else
    {
        SalamanderDebug->TraceEW(sFileW, line, messageT);
    }

    return S_OK;
}

void CSalamanderAutomation::SetExecutionInfo(CScriptInfo::EXECUTION_INFO* info)
{
    if (info != NULL)
    {
        m_pExecInfo = info;
        OnBeginExecution();
    }
    else
    {
        OnEndExecution();

        if (m_pProgress != NULL)
        {
            m_pProgress->Release();
            m_pProgress = NULL;
        }

        if (m_pWaitWindow != NULL)
        {
            m_pWaitWindow->Release();
            m_pWaitWindow = NULL;
        }

        if (m_pGui != NULL)
        {
            m_pGui->Release();
            m_pGui = NULL;
        }

        if (m_pScriptAut != NULL)
        {
            m_pScriptAut->Release();
            m_pScriptAut = NULL;
        }

        m_pExecInfo = NULL;
    }
}

void CSalamanderAutomation::OnBeginExecution()
{
}

void CSalamanderAutomation::OnEndExecution()
{
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_ProgressDialog(
    /* [retval][out] */ ISalamanderProgressDialog** dialog)
{
    if (m_pProgress == NULL)
    {
        _ASSERTE(m_pExecInfo);
        m_pProgress = new CSalamanderProgressAutomation(m_pExecInfo->pOperation);
    }

    *dialog = m_pProgress;
    m_pProgress->AddRef();

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_WaitWindow(
    /* [retval][out] */ ISalamanderWaitWindow** window)
{
    if (m_pWaitWindow == NULL)
    {
        m_pWaitWindow = new CSalamanderWaitWndAutomation();
    }

    *window = m_pWaitWindow;
    m_pWaitWindow->AddRef();

    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::ViewFile(
    /* [in] */ BSTR file,
    /* [optional][in] */ VARIANT* temp)
{
    HRESULT hr = S_OK;
    int err;
    _bstr_t fileT(file);
    CSalamanderPluginInternalViewerData viewerData;
    TCHAR caption[512];
    int cacheMode;
    TCHAR szTempFile[MAX_PATH];
    HCURSOR hOldCursor;

    if (IsArgumentPresent(temp))
    {
        if (V_VT(temp) == VT_BOOL)
        {
            cacheMode = (V_BOOL(temp) == VARIANT_FALSE) ? 0 : 1;
        }
        else
        {
            try
            {
                cacheMode = _variant_t(temp);
            }
            catch (_com_error& e)
            {
                return e.Error();
            }
        }
    }
    else
    {
        cacheMode = 0;
    }

    if (cacheMode < 0 || cacheMode > 2)
    {
        return E_INVALIDARG;
    }

    viewerData.Size = sizeof(viewerData);
    viewerData.FileName = fileT;
    viewerData.Mode = 0; // text mode
    StringCchPrintf(caption, _countof(caption), _T("%s - %s"), (PCTSTR)fileT,
                    SalamanderGeneral->LoadStr(g_hLangInst, IDS_PLUGINNAME));
    viewerData.Caption = caption;
    viewerData.WholeCaption = TRUE;

    hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    if (cacheMode == 1)
    {
        DWORD dwDosErr = NO_ERROR;

        if (SalamanderGeneral->SalGetTempFileName(NULL, _T("AUT"), szTempFile, TRUE, &dwDosErr))
        {
            if (CopyFile(fileT, szTempFile, FALSE))
            {
                viewerData.FileName = szTempFile;
            }
            else
            {
                dwDosErr = GetLastError();
            }
        }

        if (dwDosErr != NO_ERROR)
        {
            SetCursor(hOldCursor);
            RaiseDosError(dwDosErr, __uuidof(ISalamander), GetProgId());
            return HRESULT_FROM_WIN32(dwDosErr);
        }
    }

    if (!SalamanderGeneral->ViewFileInPluginViewer(
            NULL,
            &viewerData,
            cacheMode > 0,
            NULL,
            PathFindFileName(fileT),
            err))
    {
        hr = E_FAIL;

        switch (err)
        {

        case 1:
            // Failed to load the plugin.
            RaiseError(IDS_E_PLUGLOADFAIL);
            break;

        case 2:
            // Plugin returned an error.
            RaiseError(IDS_E_PLUGERROR);
            break;

        case 3:
            // Failed to move the file to the cache.
            RaiseError(IDS_E_CACHEERROR);
            break;

        default:
            _ASSERTE(0);
        }
    }

    SetCursor(hOldCursor);

    return hr;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::MatchesMask(
    /* [in] */ BSTR file,
    /* [in] */ BSTR mask,
    /* [retval][out] */ VARIANT_BOOL* match)
{
    HRESULT hr = S_OK;
    CSalamanderMaskGroup* pMaskGroup;
    _bstr_t fileT(file);
    _bstr_t maskT(mask);
    int err;

    pMaskGroup = SalamanderGeneral->AllocSalamanderMaskGroup();
    if (pMaskGroup == NULL)
    {
        return E_OUTOFMEMORY;
    }

    pMaskGroup->SetMasksString(maskT, TRUE);
    if (pMaskGroup->PrepareMasks(err))
    {
        *match = pMaskGroup->AgreeMasks(fileT, NULL) ? VARIANT_TRUE : VARIANT_FALSE;
    }
    else
    {
        if (err == 0)
        {
            hr = E_OUTOFMEMORY;
        }
        else
        {
            RaiseError(IDS_E_FILEMASKSYNTAX);
            hr = SALAUT_E_FILEMASKSYNTAX;
        }
    }

    SalamanderGeneral->FreeSalamanderMaskGroup(pMaskGroup);

    return hr;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::DebugBreak(void)
{
    HRESULT hr = S_OK;

    _ASSERTE(m_pExecInfo != NULL);
    if (m_pExecInfo->dbgInfo.pDbgApp)
    {
        hr = m_pExecInfo->dbgInfo.pDbgApp->CauseBreak();
    }

    return hr;
}

struct ABORTABLE_DLG_ERROR_OR_QUESTION_PARAMS
{
    HWND hwndOwner;
    DWORD dwFlags;
    LPCTSTR pszFileName;
    LPCTSTR pszErrorOrQuestion;
    LPCTSTR pszTitle;
    int nDlgResult;
};

static void CALLBACK ShowErrorDialogProc(void* pContext)
{
    ABORTABLE_DLG_ERROR_OR_QUESTION_PARAMS* msgbox;
    msgbox = reinterpret_cast<ABORTABLE_DLG_ERROR_OR_QUESTION_PARAMS*>(pContext);
    msgbox->nDlgResult = SalamanderGeneral->DialogError(
        msgbox->hwndOwner,
        msgbox->dwFlags,
        msgbox->pszFileName,
        msgbox->pszErrorOrQuestion,
        msgbox->pszTitle);
}

static void CALLBACK ShowQuestionDialogProc(void* pContext)
{
    ABORTABLE_DLG_ERROR_OR_QUESTION_PARAMS* msgbox;
    msgbox = reinterpret_cast<ABORTABLE_DLG_ERROR_OR_QUESTION_PARAMS*>(pContext);
    msgbox->nDlgResult = SalamanderGeneral->DialogQuestion(
        msgbox->hwndOwner,
        msgbox->dwFlags,
        msgbox->pszFileName,
        msgbox->pszErrorOrQuestion,
        msgbox->pszTitle);
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::ErrorDialog(
    /* [in] */ VARIANT* file,
    /* [in] */ VARIANT* error,
    /* [in] */ int buttons,
    /* [optional][in] */ VARIANT* title,
    /* [retval][out] */ int* result)
{
    HRESULT hr;
    ABORTABLE_DLG_ERROR_OR_QUESTION_PARAMS msgbox = {
        0,
    };
    PCTSTR errorT;
    _bstr_t errorHolder;
    _bstr_t titleT;
    TCHAR errorBuffer[256];

    if (buttons != BUTTONS_OK &&
        buttons != BUTTONS_RETRYCANCEL &&
        buttons != BUTTONS_SKIPCANCEL &&
        buttons != BUTTONS_RETRYSKIPCANCEL)
    {
        return E_INVALIDARG;
    }

    CFileInfo oInfo;
    hr = oInfo.FromVariant(file);
    if (FAILED(hr))
    {
        return hr;
    }

    switch (V_VT(error) & VT_TYPEMASK)
    {
    case VT_ERROR:
    case VT_I2:
    case VT_I4:
    case VT_R4:
    case VT_R8:
    case VT_I1:
    case VT_UI1:
    case VT_UI2:
    case VT_UI4:
    case VT_INT:
    case VT_UINT:
    {
        try
        {
            FormatErrorText(_variant_t(error), errorBuffer, _countof(errorBuffer));
            errorT = errorBuffer;
        }
        catch (_com_error& e)
        {
            return e.Error();
        }

        break;
    }

    default:
        try
        {
            errorHolder = error;
            errorT = errorHolder;
        }
        catch (_com_error& e)
        {
            return e.Error();
        }
    }

    msgbox.hwndOwner = SalamanderGeneral->GetMsgBoxParent();
    msgbox.dwFlags = buttons;
    msgbox.pszFileName = oInfo.Path();
    msgbox.pszErrorOrQuestion = errorT;
    if (IsArgumentPresent(title))
    {
        titleT = title;
        msgbox.pszTitle = (PCTSTR)titleT;
    }

    hr = AbortableModalDialogWrapper(m_pScriptInfo, msgbox.hwndOwner, ShowErrorDialogProc, &msgbox);
    if (FAILED(hr))
    {
        if (hr == SALAUT_E_ABORT)
        {
            RaiseError(IDS_E_SCRIPTABORTED);
        }
        return hr;
    }

    if (msgbox.nDlgResult)
    {
        return E_INVALIDARG;
    }

    *result = ButtonToFriendlyNumber(msgbox.nDlgResult);

    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::QuestionDialog(
    /* [in] */ VARIANT* file,
    /* [in] */ BSTR question,
    /* [in] */ int buttons,
    /* [optional][in] */ VARIANT* title,
    /* [retval][out] */ int* result)
{
    HRESULT hr;
    ABORTABLE_DLG_ERROR_OR_QUESTION_PARAMS msgbox = {
        0,
    };
    _bstr_t titleT;
    OLE2A questionT(question);

    if (buttons != BUTTONS_YESALLSKIPCANCEL &&
        buttons != BUTTONS_YESNOCANCEL &&
        buttons != BUTTONS_YESALLCANCEL)
    {
        return E_INVALIDARG;
    }

    CFileInfo oInfo;
    hr = oInfo.FromVariant(file);
    if (FAILED(hr))
    {
        return hr;
    }

    msgbox.hwndOwner = SalamanderGeneral->GetMsgBoxParent();
    msgbox.dwFlags = buttons;
    msgbox.pszFileName = oInfo.Path();
    msgbox.pszErrorOrQuestion = questionT;
    if (IsArgumentPresent(title))
    {
        titleT = title;
        msgbox.pszTitle = (PCTSTR)titleT;
    }

    hr = AbortableModalDialogWrapper(m_pScriptInfo, msgbox.hwndOwner, ShowQuestionDialogProc, &msgbox);
    if (FAILED(hr))
    {
        if (hr == SALAUT_E_ABORT)
        {
            RaiseError(IDS_E_SCRIPTABORTED);
        }
        return hr;
    }

    if (msgbox.nDlgResult == 0)
    {
        return E_INVALIDARG;
    }

    *result = ButtonToFriendlyNumber(msgbox.nDlgResult);

    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::OverwriteDialog(
    /* [in] */ VARIANT* file1,
    /* [in] */ VARIANT* file2,
    /* [in] */ int buttons,
    /* [retval][out] */ int* result)
{
    HRESULT hr;
    int dlgres;

    if (buttons != BUTTONS_YESALLSKIPCANCEL &&
        buttons != BUTTONS_YESNOCANCEL)
    {
        return E_INVALIDARG;
    }

    CFileInfo oInfo1;
    hr = oInfo1.FromVariant(file1);
    if (FAILED(hr))
    {
        return hr;
    }

    CFileInfo oInfo2;
    hr = oInfo2.FromVariant(file2);
    if (FAILED(hr))
    {
        return hr;
    }

    dlgres = SalamanderGeneral->DialogOverwrite(
        SalamanderGeneral->GetMsgBoxParent(),
        buttons,
        oInfo1.Path(),
        oInfo1.Info(),
        oInfo2.Path(),
        oInfo2.Info());

    if (dlgres == 0)
    {
        return E_INVALIDARG;
    }

    *result = ButtonToFriendlyNumber(dlgres);

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_Forms(
    /* [retval][out] */ ISalamanderGui** gui)
{
    if (m_pGui == NULL)
    {
        m_pGui = new CSalamanderGuiNamespace(m_pScriptInfo);
    }

    *gui = m_pGui;
    m_pGui->AddRef();

    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::SetPersistentVal(
    /* [in] */ BSTR key,
    /* [in] */ VARIANT* val)
{
    HRESULT hr;

    hr = g_oPersistentStorage.SetValue(key, val);

    return hr;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::GetPersistentVal(
    /* [in] */ BSTR key,
    /* [retval][out] */ VARIANT* val)
{
    HRESULT hr;

    hr = g_oPersistentStorage.GetValue(key, val);

    return hr;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::get_Script(
    /* [retval][out] */ ISalamanderScriptInfo** info)
{
    if (m_pScriptAut == NULL)
    {
        m_pScriptAut = new CSalamanderScriptInfoAutomation(m_pScriptInfo);
    }

    *info = m_pScriptAut;
    m_pScriptAut->AddRef();

    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderAutomation::GetFullPath(
    /* [in] */ BSTR path,
    /* [optional][in] */ VARIANT* panel,
    /* [retval][out] */ BSTR* result)
{
    HRESULT hr = S_OK;
    ISalamanderPanel* pPanel = NULL;
    const char* currentT = NULL;
    _bstr_t pathT(path);
    TCHAR szPath[MAX_PATH];

    // validate input
    if (lstrlen(pathT) >= MAX_PATH)
    {
        return E_INVALIDARG;
    }

    if (IsArgumentPresent(panel))
    {
        if (V_VT(panel) == VT_DISPATCH || V_VT(panel) == VT_UNKNOWN)
        {
            hr = V_UNKNOWN(panel)->QueryInterface(__uuidof(pPanel), (void**)&pPanel);
        }
        else
        {
            hr = E_INVALIDARG;
        }

        if (FAILED(hr))
        {
            // TODO: report better error, st. like "panel object expected"
            return hr;
        }
    }

    // when srcpanel is present, we need to call SalUpdateDefaultDir() and get corresponding panel path
    if (pPanel != NULL)
    {
        int iPanel = pPanel->_GetPanelIndex();
        pPanel->Release();
        pPanel = NULL;

        BOOL useSourcePanel = (iPanel == PANEL_SOURCE) ||
                              (iPanel == SalamanderGeneral->GetSourcePanel());
        SalamanderGeneral->SalUpdateDefaultDir(useSourcePanel);

        if (!SalamanderGeneral->GetPanelPath(
                useSourcePanel ? PANEL_SOURCE : PANEL_TARGET,
                szPath,
                _countof(szPath),
                NULL,
                NULL))
        {
            _ASSERTE(0);
            return E_FAIL;
        }
        currentT = szPath;
    }

    TCHAR szName[MAX_PATH];
    StringCchCopy(szName, _countof(szName), pathT);
    int errTextID;
    BOOL ret = SalamanderGeneral->SalGetFullName(szName, &errTextID, currentT, NULL);
    if (!ret)
    {
        char errText[200];
        SalamanderGeneral->GetGFNErrorText(errTextID, errText, 200);
        RaiseError(A2OLE(errText));
        hr = SALAUT_E_INVALIDPATH;
    }
    else
    {
        *result = SysAllocString(A2OLE(szName));
    }

    return hr;
}

HRESULT CSalamanderAutomation::InvokeFilter(__out EXCEPINFO* pExcepInfo)
{
    return CheckAbort(m_pScriptInfo, pExcepInfo);
}
