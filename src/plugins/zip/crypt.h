// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define CHECK_OUT_OEM_PASWORD

#define ENCRYPT_HEADER_SIZE 12

//test password and set up keys
int InitKeys(const char* password, const char* header, char check,
             __UINT32* keys);

//decrypt buffer
void Decrypt(char* buffer, unsigned size, __UINT32* keys);

//crypt encryption header
void CryptHeader(const char* password, char* header, unsigned short check,
                 __UINT32* keys);

//encrypt buffer
void Encrypt(char* buffer, unsigned size, __UINT32* keys);

void FillBufferWithRandomData(char* buf, int len);
