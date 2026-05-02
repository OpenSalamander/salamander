// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	unkimpl.h
	Helper template for IUnknown implementations.
*/

#pragma once

/// Provides the basic infrastructure to implement IUnknown.
class CUnknownImpl : public IUnknown
{
protected:
    LONG m_cRef;

    // http://blogs.msdn.com/oldnewthing/archive/2005/09/28/474855.aspx
    enum
    {
        DESTRUCTOR_REFCOUNT = 42
    };

    /// Called when the ref-count drops to zero but before the
    /// destructor is called.
    virtual void FinalRelease()
    {
    }

public:
    CUnknownImpl()
    {
        m_cRef = 1;
    }

    virtual ~CUnknownImpl()
    {
        _ASSERTE(m_cRef == DESTRUCTOR_REFCOUNT);
    }

    // IUnknown

    STDMETHOD(QueryInterface)
    (REFIID iid, void** ppvObject)
    {
        HRESULT hr = S_OK;

        if (ppvObject == NULL)
        {
            return E_POINTER;
        }

        *ppvObject = NULL;

        if (IsEqualIID(iid, __uuidof(IUnknown)))
        {
            *ppvObject = (IUnknown*)this;
        }

        if (*ppvObject)
        {
            AddRef();
        }
        else
        {
            hr = E_NOINTERFACE;
        }

        return hr;
    }

    STDMETHOD_(ULONG, AddRef)
    ()
    {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHOD_(ULONG, Release)
    ()
    {
        LONG cRef;

        _ASSERTE(m_cRef > 0);

        cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
        {
            FinalRelease();
            m_cRef = DESTRUCTOR_REFCOUNT; // avoid double-destruction
            delete this;
        }

        return cRef;
    }
};

// The CUnknownImpl template checks whether the destructor is called from the
// Release() to avoid double destruction (see few lines above). However when
// the exception is raised from the constructor, the destructor is immediately
// called by C++ (as dictated by the standard). If you expect exceptions in
// the constructor you should protect it against the mentioned assertion check.
#ifdef _DEBUG
#define UNK_PROTECT_CONSTRUCTOR_BEGIN \
    try \
    {
#define UNK_PROTECT_CONSTRUCTOR_END \
    } \
    catch (_com_error) \
    { \
        m_cRef = DESTRUCTOR_REFCOUNT; \
        throw; \
    }
#else
#define UNK_PROTECT_CONSTRUCTOR_BEGIN
#define UNK_PROTECT_CONSTRUCTOR_END
#endif
