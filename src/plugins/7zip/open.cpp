// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "7zip.h"
#include "open.h"
#include "7zip.rh"
#include "7zip.rh2"
#include "lang\lang.rh"
#include "dialogs.h"

#include "Common/StringConvert.h"

CArchiveOpenCallbackImp::CArchiveOpenCallbackImp(UString& password)
    : Password(password)
{
}

CArchiveOpenCallbackImp::~CArchiveOpenCallbackImp()
{
}

STDMETHODIMP CArchiveOpenCallbackImp::SetTotal(const UInt64* /*files*/, const UInt64* /*bytes*/)
{
    return S_OK;
}

STDMETHODIMP CArchiveOpenCallbackImp::SetCompleted(const UInt64* /*files*/, const UInt64* /*bytes*/)
{
    return S_OK;
}

STDMETHODIMP CArchiveOpenCallbackImp::CryptoGetTextPassword(BSTR* password)
{
    if (Password.IsEmpty())
    {
        CEnterPasswordDialog dlg(SalamanderGeneral->GetMsgBoxParent());
        int res = (int)dlg.Execute();

        if (res != IDOK)
            return E_ABORT;

        Password = GetUnicodeString(dlg.GetPassword());
    }
    StringToBstr(Password, password);

    return S_OK;
}

STDMETHODIMP CArchiveOpenCallbackImp::GetStream(const wchar_t* name,
                                                IInStream** inStream)
{
    return S_OK;
}

STDMETHODIMP CArchiveOpenCallbackImp::GetProperty(PROPID propID, PROPVARIANT* value)
{
    return S_OK;
}
