// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// definice pro uziti v SFXMake
#define __UINT32 unsigned long
#define __UINT8 unsigned char
#define STATIC_CRC_TAB

//size of crc tab in the memory
#define CRC_TAB_SIZE 256 * sizeof(__UINT32)
//initial value of crc shift register
#define INIT_CRC 0L

#ifdef STATIC_CRC_TAB
//table of CRC-32's of all single-byte values (made by MakeCrcTable)
extern const __UINT32 StaticCrcTab[256];
#endif

//fill up crc table
void MakeCrcTable(__UINT32* crcTab);

//run a set of bytes through the crc shift register, if buffer is a NULL
//pointer, then initialize the crc shift register contents instead
//return the current crc in either case
__UINT32 UpdateCrc(__UINT8* buffer, unsigned length, __UINT32 crcVal, const __UINT32* crcTab);
