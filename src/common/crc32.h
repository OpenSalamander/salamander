// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//#define STATIC_CRC_TAB

//size of crc tab in the memory
#define CRC_TAB_SIZE 256 * sizeof(DWORD)
//initial value of crc shift register
#define INIT_CRC 0L

#ifdef STATIC_CRC_TAB
//table of CRC-32's of all single-byte values (made by MakeCrcTable)
extern const DWORD StaticCrcTab[256];
#endif

//fill up crc table
void MakeCrcTable(DWORD* crcTab);

//run a set of bytes through the crc shift register, if buffer is a NULL
//pointer, then initialize the crc shift register contents instead
//return the current crc in either case
DWORD UpdateCrc(char* buffer, unsigned length, DWORD crcVal, const DWORD* crcTab);
