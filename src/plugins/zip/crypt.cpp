// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <stdlib.h>
#include <time.h>
#include <process.h>
#include <commctrl.h>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "config.h"
#include "selfextr/comdefs.h"
#include "typecons.h"
#include "crypt.h"

//Return the next byte in the pseudo-random sequence
__forceinline int decrypt_byte(__UINT32* keys)
{
    CALL_STACK_MESSAGE_NONE
    unsigned temp;

    temp = ((unsigned)keys[2] & 0xffff) | 2;
    return (int)(((temp * (temp ^ 1)) >> 8) & 0xff);
}

extern CSalamanderGeneralAbstract* SalamanderGeneral;
#define CRC32(c, b) (SalamanderGeneral->UpdateCrc32(&b, 1, c ^ 0xffffffffL) ^ 0xffffffffL)

//Update the encryption keys with the next byte of plain text
__forceinline int update_keys(int c, __UINT32* keys)
{
    CALL_STACK_MESSAGE_NONE
    keys[0] = CRC32(keys[0], c);
    keys[1] += keys[0] & 0xff;
    keys[1] = keys[1] * 134775813L + 1;
    {
        int keyshift = (int)(keys[1] >> 24);
        keys[2] = CRC32(keys[2], keyshift);
    }
    return c;
}

//initialize the encryption keys and the random header according to
//the given password.
void init_keys(const char* password, __UINT32* keys)
{
    CALL_STACK_MESSAGE2("init_keys(%s, )", password);
    keys[0] = 305419896L;
    keys[1] = 591751049L;
    keys[2] = 878082192L;
    while (*password)
    {
        update_keys((int)*password, keys);
        password++;
    }
}

int testkey(const char* password, const char* header, char check,
            __UINT32* keys)
{
    CALL_STACK_MESSAGE4("testkey(%s, %s, %u, )", password, header, check);
    char buf[ENCRYPT_HEADER_SIZE]; //decrypted header

    //set keys and save the encrypted header
    init_keys(password, keys);
    CopyMemory(buf, header, ENCRYPT_HEADER_SIZE);
    //decrypt header
    Decrypt(buf, ENCRYPT_HEADER_SIZE, keys);
    //check password
    if (buf[ENCRYPT_HEADER_SIZE - 1] == check)
        return 0;
    else
        return -1; // bad
}

//test password and set up keys
int InitKeys(const char* password, const char* header, char check,
             __UINT32* keys)
{
    //CALL_STACK_MESSAGE4("InitKeys(%s, %s, %c, )", password, header, check );
    int ret;

    ret = testkey(password, header, check, keys);

#ifdef CHECK_OUT_OEM_PASWORD
    if (ret)
    {
        char buffer[MAX_PASSWORD];

        CharToOem(password, buffer);
        ret = testkey(buffer, header, check, keys);
    }
#endif

    return ret;
}

//decrypt buffer
void Decrypt(char* buffer, unsigned size, __UINT32* keys)
{
    CALL_STACK_MESSAGE3("Decrypt(%p, %u, , )", buffer, size);
    char* ptr = buffer;

    while (ptr < buffer + size)
        update_keys(*ptr++ ^= decrypt_byte(keys), keys);
}

#define zencode(c, t) (t = decrypt_byte(__G), update_keys(c), t ^ (c))

void FillBufferWithRandomData(char* buf, int len)
{
    static unsigned calls = 0; //ensure different random header each time

    if (++calls == 1)
        srand((unsigned)time(NULL) ^ (unsigned)_getpid());

    while (len--)
        *buf++ = (rand() >> 7) & 0xff;
}

//crypt encryption header
void CryptHeader(const char* password, char* header, unsigned short check,
                 __UINT32* keys)
{
    //CALL_STACK_MESSAGE3("CryptHeader(%s, , 0x%X, )", password, check);

    //First generate ENCRYPT_HEADER_SIZE-1 random bytes. We encrypt the
    //output of rand() to get less predictability, since rand() is
    //often poorly implemented.

    init_keys(password, keys);

    FillBufferWithRandomData(header, ENCRYPT_HEADER_SIZE - 1);

    Encrypt(header, ENCRYPT_HEADER_SIZE - 1, keys);

    // Encrypt random header (last two bytes is high word of crc)
    init_keys(password, keys);
    if (check & 0xFF)
        *(unsigned short*)(header + ENCRYPT_HEADER_SIZE - 2) = check;
    else
        header[ENCRYPT_HEADER_SIZE - 1] = (char)(check >> 8);
    Encrypt(header, ENCRYPT_HEADER_SIZE, keys);
}

//encrypt buffer
void Encrypt(char* buffer, unsigned size, __UINT32* keys)
{
    CALL_STACK_MESSAGE3("Encrypt(%p, %u, , )", buffer, size);
    char* ptr = buffer;
    int temp;

    while (ptr < buffer + size)
    {
        temp = decrypt_byte(keys);
        update_keys(*ptr, keys);
        *ptr = temp ^ *ptr;
        ptr++;
    }
}
