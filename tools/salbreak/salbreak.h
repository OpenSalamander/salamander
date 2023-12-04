// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// vrati SID (jako retezec) pro nas proces
// vraceny SID je potreba uvolnit pomoci volani LocalFree
//   LPTSTR sid;
//   if (GetStringSid(&sid))
//     LocalFree(sid);
BOOL GetStringSid(LPTSTR* stringSid);

// vrati MD5 hash napocitany ze SID, cimz dostavame z variable-length SIDu pole 16 bajtu
// 'sidMD5' musi ukazovat do pole 16 bajtu
// v pripade uspechu vraci TRUE, jinak FALSE a nuluje cele pole 'sidMD5'
BOOL GetSidMD5(BYTE* sidMD5);

// pripravi SECURITY_ATTRIBUTES tak aby pomoci nich vytvareny objekt (mutex, mapovana pamet) byl zabezpecny
// znamena to, ze skupine Everyone je odebran pristup WRITE_DAC | WRITE_OWNER, jinak je vse povoleno
// jde o tridu lepsi zabezpecni nez "NULL DACL", kde je objekt uplne otevren vsem
// lze volat pod kazdym OS, ukazatel vrati od W2K vejs, jinak vraci NULL
// pokud vrati 'psidEveryone' nebo 'paclNewDacl' ruzne od NULL, je treba je destruovat
SECURITY_ATTRIBUTES* CreateAccessableSecurityAttributes(SECURITY_ATTRIBUTES* sa, SECURITY_DESCRIPTOR* sd,
                                                        DWORD allowedAccessMask, PSID* psidEveryone, PACL* paclNewDacl);

// V pripade uspechu vrati TRUE a naplni DWORD na ktery odkazuje 'integrityLevel'
// jinak (pri selhani nebo pod OS strasima nez Vista) vrati FALSE
BOOL GetProcessIntegrityLevel(DWORD* integrityLevel);
