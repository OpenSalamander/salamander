// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// returns the SID (as a string) for the current process
// the returned SID must be freed by calling LocalFree
//   LPTSTR sid;
//   if (GetStringSid(&sid))
//     LocalFree(sid);
BOOL GetStringSid(LPTSTR* stringSid);

// returns the MD5 hash computed from the SID, which gives us a 16-byte array
// from the variable-length SID
// 'sidMD5' must point to a 16-byte array
// returns TRUE on success; otherwise returns FALSE and zeroes the entire 'sidMD5' array
BOOL GetSidMD5(BYTE* sidMD5);

// prepares SECURITY_ATTRIBUTES so that an object created with them (mutex,
// mapped memory) is secured
// this means the Everyone group is denied WRITE_DAC | WRITE_OWNER access;
// everything else is allowed
// this is a class of security better than a "NULL DACL", where the object is
// completely open to everyone
// can be called on any OS; returns the pointer on W2K and later, otherwise NULL
// if it returns 'psidEveryone' or 'paclNewDacl' different from NULL, they must be destroyed
SECURITY_ATTRIBUTES* CreateAccessableSecurityAttributes(SECURITY_ATTRIBUTES* sa, SECURITY_DESCRIPTOR* sd,
                                                        DWORD allowedAccessMask, PSID* psidEveryone, PACL* paclNewDacl);

// returns TRUE on success and fills the DWORD pointed to by 'integrityLevel'
// otherwise (on failure or on OSes older than Vista) returns FALSE
BOOL GetProcessIntegrityLevel(DWORD* integrityLevel);
