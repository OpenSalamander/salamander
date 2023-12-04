// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	persistence.h
	Implements storage for the persistent values.
*/

#pragma once

/// Persistent value storage.
class CPersistentValueStorage
{
private:
    /// True if the storage was modified (value changed, added or removed)
    /// since last load.
    bool m_bModified;

    enum ENTRY_STATE
    {
        ENTRY_STALE,
        ENTRY_NEW,
        ENTRY_MODIFIED,
        ENTRY_DELETED,
        ENTRY_DETACHED
    };

    struct PERSISTENT_ENTRY
    {
        mutable BSTR name;
        mutable VARIANT value;
        mutable ENTRY_STATE state;

        PERSISTENT_ENTRY()
        {
            name = NULL;
            VariantInit(&value);
            state = ENTRY_STALE;
        }

        PERSISTENT_ENTRY(const OLECHAR* name, const VARIANT* value, ENTRY_STATE state)
        {
            if (name)
            {
                this->name = SysAllocString(name);
            }
            else
            {
                this->name = NULL;
            }

            VariantInit(&this->value);
            VariantCopy(&this->value, value);

            this->state = state;
        }

        ~PERSISTENT_ENTRY()
        {
            if (name)
            {
                SysFreeString(name);
                name = NULL;
            }

            VariantClear(&value);
        }

        /// Smart (automatic) copy constructor.
        PERSISTENT_ENTRY(const PERSISTENT_ENTRY& e)
        {
            // Transfer ownership of the values to this entry.
            // Reduces overhead of excesive allocating/copying
            // between temporary instances.

            this->name = e.name;
            this->value = e.value; // shallow copy
            this->state = e.state;

            e.name = NULL;
            V_VT(&e.value) = VT_EMPTY; // only mark as empty, do not VariantClear
            e.state = ENTRY_DETACHED;
        }

        /// Not implemented to prevent use.
        PERSISTENT_ENTRY& operator=(const PERSISTENT_ENTRY&);
    };

    TDirectArray<PERSISTENT_ENTRY> m_values;

    PERSISTENT_ENTRY* LookupValue(BSTR strName);

    static bool ValidateName(BSTR strName);
    static bool SupportedType(VARTYPE vt);

    static bool SaveEntry(
        PCWSTR pwzName,
        const VARIANT* value,
        HKEY hKey,
        CSalamanderRegistryAbstract* registry);

public:
    /// Constructor.
    CPersistentValueStorage();

    /// Destructor.
    ~CPersistentValueStorage();

    bool Load(HKEY hKey, CSalamanderRegistryAbstract* registry);
    bool Save(HKEY hKey, CSalamanderRegistryAbstract* registry);

    bool IsModified() const
    {
        return m_bModified;
    }

    HRESULT GetValue(BSTR strName, VARIANT* value);
    HRESULT SetValue(BSTR strName, const VARIANT* value);
};
