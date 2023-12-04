// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "7za/CPP/Common/MyCom.h"
#include "7za/CPP/Common/MyString.h"

#include "7za/CPP/7zip/IPassword.h"
#include "7za/CPP/7zip/Archive/IArchive.h"

class CArchiveOpenCallbackImp : public IArchiveOpenCallback,
                                public IArchiveOpenVolumeCallback,
                                public ICryptoGetTextPassword,
                                public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP1(ICryptoGetTextPassword)

    // IArchiveOpenCallback
    STDMETHOD(SetTotal)
    (const UInt64* files, const UInt64* bytes);
    STDMETHOD(SetCompleted)
    (const UInt64* files, const UInt64* bytes);

    // ICryptoGetTextPassword
    STDMETHOD(CryptoGetTextPassword)
    (BSTR* password);

    // IArchiveOpenVolumeCallback
    STDMETHOD(GetProperty)
    (PROPID propID, PROPVARIANT* value);
    STDMETHOD(GetStream)
    (const wchar_t* name, IInStream** inStream);

private:
    UString& Password;

public:
    // If entered, the password gets propagated towards password
    CArchiveOpenCallbackImp(UString& password);
    ~CArchiveOpenCallbackImp();
};
