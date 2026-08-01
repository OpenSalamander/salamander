// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_crypt) // Ensure structure packing regardless of compiler settings
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

/**************************************************************************
 * Interface to the encryption libraries provided by Salamander.
 * Supported methods: AES, SHA1
 * Notes: for MD5 use CSalamanderGeneralAbstract::AllocSalamanderMD5,
 *        for CRC32 use CSalamanderGeneralAbstract::UpdateCrc32.
 **************************************************************************/

/* ================================= AES ================================ */

#define SAL_AES_MAX_KEY_LENGTH 32
#define SAL_AES_MAX_PWD_LENGTH 128
#define SAL_AES_MAX_SALT_LENGTH 16
#define SAL_AES_PWD_VER_LENGTH 2

/*  AES modes and parameter sizes
    Field lengths (in bytes) versus File Encryption Mode (0 < mode < 4)

    Mode KeyLen SaltLen  MACLen Overhead
       1     16       8      10       18
       2     24      12      10       22
       3     32      16      10       26

   The following macros assume that the mode value is correct.
*/

#define SAL_AES_KEY_LENGTH(mode) (8 * (mode & 3) + 8)
#define SAL_AES_SALT_LENGTH(mode) (4 * (mode & 3) + 4)
#define SAL_AES_MAC_LENGTH(mode) (10)

/* Return codes of AESInit */
#define SAL_AES_ERR_GOOD_RETURN 0
#define SAL_AES_ERR_PASSWORD_TOO_LONG -100
#define SAL_AES_ERR_BAD_MODE -101

#define SAL_AES_BLOCK_SIZE 16
#define SAL_AES_ENCR_CTX_SIZE 264
#define SAL_AES_AUTH_CTX_SIZE 160

/* Internal control structure associated with each AES de/compression task.
   Not to be modified by the plugin.
 */
struct CSalAES
{
    BYTE nonce[SAL_AES_BLOCK_SIZE];       /* the CTR nonce          */
    BYTE encr_bfr[SAL_AES_BLOCK_SIZE];    /* encrypt buffer         */
    BYTE encr_ctx[SAL_AES_ENCR_CTX_SIZE]; /* encryption context     */
    BYTE auth_ctx[SAL_AES_AUTH_CTX_SIZE]; /* authentication context */
    DWORD encr_pos;                       /* block position (enc)   */
    DWORD pwd_len;                        /* password length        */
    DWORD mode;                           /* File encryption mode   */
};

/* =============================== SHA1 =============================== */

/* Internal control structure associated with each SHA1 task.
   Not to be modified by the plugin.
 */
struct CSalSHA1
{
    DWORD state[5];
    DWORD count[2];
    BYTE buffer[64];
};

/* Any function of CSalamanderCryptAbstract can be called from any thread.
   Single instance can be used many times, using different control structs.
 */

class CSalamanderCryptAbstract
{
public:
    /* AES functions */

    /* To decrypt encrypted data, provide the same salt.
      The salt size is SAL_AES_SALT_LENGTH(mode) bytes.
      Return value is one of SAL_AES_ERR_XXX. */
    virtual int WINAPI AESInit(CSalAES* aes,
                               int mode,            /* Mode (key size) to be used (input) */
                               LPCSTR pwd,          /* User specified password (input)    */
                               size_t pwd_len,      /* Password length (input)            */
                               LPBYTE salt,         /* Salt (input)                       */
                               LPWORD pwd_ver) = 0; /* 2 byte password verifier (output)  */

    /* Encryption and decryption is performed in place */
    virtual void WINAPI AESEncrypt(CSalAES* aes, LPVOID data, size_t dataLen) = 0;
    virtual void WINAPI AESDecrypt(CSalAES* aes, LPVOID data, size_t dataLen) = 0;

    /* AESEnd finishes encryption/decryption and calculates the MAC value,
      the actual size (SAL_AES_MAC_LENGTH(aes->mode)) is returned in *pMacLen (optional).
      To verify password correctness, compare the MAC's of encrypted and decrypted data. */
    virtual void WINAPI AESEnd(CSalAES* aes, LPBYTE mac, LPDWORD pMacLen) = 0;

    /* SHA1 functions */

    virtual void WINAPI SHA1Init(CSalSHA1* sha1) = 0;

    virtual void WINAPI SHA1Update(CSalSHA1* sha1, const LPBYTE data, size_t dataLen) = 0;

    /* Finishes hashing and retrieves 20-byte (160-bit) hash digest */
    virtual void WINAPI SHA1Final(CSalSHA1* sha1, BYTE digest[20]) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_crypt)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
