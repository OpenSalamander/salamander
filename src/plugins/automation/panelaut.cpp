// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	panelaut.cpp
	Panel automation object.
*/

#include "precomp.h"
#include "salamander_h.h"
#include "panelaut.h"
#include "itemcoll.h"
#include "aututils.h"
#include "lang\lang.rh"

extern CSalamanderGeneralAbstract* SalamanderGeneral;

CSalamanderPanelAutomation::CSalamanderPanelAutomation(int nPanel)
{
    _ASSERTE(nPanel == PANEL_LEFT || nPanel == PANEL_RIGHT);

    m_nPanel = nPanel;
    m_pFocusedItem = NULL;
}

CSalamanderPanelAutomation::~CSalamanderPanelAutomation()
{
    if (m_pFocusedItem)
    {
        m_pFocusedItem->Release();
    }
}

HRESULT CSalamanderPanelAutomation::RaiseChPPErr(int nErr)
{
    int nIdDescription = -1;

    switch (nErr)
    {
    case CHPPFR_INVALIDPATH:
        ::RaiseDosError(ERROR_BAD_PATHNAME, __uuidof(ISalamanderPanel), GetProgId());
        return HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME);

    case CHPPFR_INVALIDARCHIVE:
        nIdDescription = IDS_E_INVALIDARCHIVE;
        break;

    case CHPPFR_CANNOTCLOSEPATH:
        nIdDescription = IDS_E_CANNOTCLOSEPATH;
        break;

    case CHPPFR_SHORTERPATH:
        nIdDescription = IDS_E_SHORTERPATH;
        break;

    default:
        _ASSERTE(0);
        break;
    }

    if (nIdDescription != -1)
    {
        RaiseError(nIdDescription);
    }

    return E_FAIL;
}

/* [propput][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelAutomation::put_Path(
    /* [in] */ BSTR path)
{
    _bstr_t pathT(path);
    int err;

    if (!SalamanderGeneral->ChangePanelPath(m_nPanel, pathT, &err) && err != CHPPFR_FILENAMEFOCUSED) // path contained filename part, the file name was focused
    {
        return RaiseChPPErr(err);
    }

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelAutomation::get_Path(
    /* [retval][out] */ BSTR* path)
{
    TCHAR szPath[MAX_PATH];
    int type;
    TCHAR* pszArchiveOrFs;
    _bstr_t pathT;

    if (!SalamanderGeneral->GetPanelPath(
            m_nPanel,
            szPath,
            _countof(szPath),
            &type,
            &pszArchiveOrFs))
    {
        _ASSERTE(0);
        return E_FAIL;
    }

    pathT = szPath;
    *path = pathT.Detach();

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelAutomation::get_FocusedItem(
    /* [retval][out] */ ISalamanderPanelItem** item)
{
    const CFileData* pFileData;

    if (m_pFocusedItem == NULL)
    {
        m_pFocusedItem = new CSalamanderPanelItemAutomation();
    }

    pFileData = SalamanderGeneral->GetPanelFocusedItem(m_nPanel, NULL);

    if (pFileData)
    {
        m_pFocusedItem->Set(pFileData, m_nPanel);

        *item = m_pFocusedItem;
        (*item)->AddRef();
    }
    else
    {
        // empty panel, no focused item
        *item = NULL;
    }

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelAutomation::get_SelectedItems(
    /* [retval][out] */ ISalamanderPanelItemCollection** coll)
{
    *coll = new CSalamanderPanelItemCollection(m_nPanel, CSalamanderPanelItemCollection::SelectionCollection);
    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelAutomation::get_Items(
    /* [retval][out] */ ISalamanderPanelItemCollection** coll)
{
    *coll = new CSalamanderPanelItemCollection(m_nPanel, CSalamanderPanelItemCollection::ItemCollection);
    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelAutomation::SelectAll(void)
{
    SalamanderGeneral->SelectAllPanelItems(m_nPanel, TRUE, TRUE);
    return S_OK;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelAutomation::DeselectAll(
    /* [optional][in] */ VARIANT* save)
{
    bool bSave = false;

    if (IsArgumentPresent(save))
    {
        bSave = _variant_t(save);
    }

    if (bSave)
    {
        SalamanderGeneral->StoreSelectionOnPanelPath(m_nPanel);
    }

    SalamanderGeneral->SelectAllPanelItems(m_nPanel, FALSE, TRUE);

    return S_OK;
}

/* [propget][id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelAutomation::get_PathType(
    /* [retval][out] */ int* type)
{
    if (SalamanderGeneral->GetPanelPath(m_nPanel, NULL, 0, type, NULL) &&
            *type == PATH_TYPE_WINDOWS ||
        *type == PATH_TYPE_ARCHIVE || *type == PATH_TYPE_FS)
    {
        return S_OK;
    }

    _ASSERTE(0);
    return E_FAIL;
}

/* [id] */ HRESULT STDMETHODCALLTYPE CSalamanderPanelAutomation::StoreSelection(void)
{
    SalamanderGeneral->StoreSelectionOnPanelPath(m_nPanel);
    return S_OK;
}

/* [hidden][restricted][local][id] */ int STDMETHODCALLTYPE CSalamanderPanelAutomation::_GetPanelIndex(void)
{
    return m_nPanel;
}
