// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern char LowerCase[256];
extern char UpperCase[256];
extern unsigned short CType[256];

inline int IsSpace(int c) { return CType[c] & C1_SPACE; }
inline int IsAlpha(int c) { return CType[c] & C1_ALPHA; }
inline int IsAlnum(int c) { return CType[c] & (C1_ALPHA | C1_DIGIT); }
inline int IsCntrl(int c) { return CType[c] & C1_CNTRL; }
inline int IsDigit(int c) { return CType[c] & C1_DIGIT; }
inline int IsGraph(int c) { return CType[c] & (C1_PUNCT | C1_ALPHA | C1_DIGIT); }
inline int IsLower(int c) { return CType[c] & C1_LOWER; }
inline int IsPrint(int c) { return CType[c] & (C1_PUNCT | C1_ALPHA | C1_DIGIT | C1_BLANK); }
inline int IsUpper(int c) { return CType[c] & C1_UPPER; }
inline int IsXdigit(int c) { return CType[c] & C1_XDIGIT; }
inline int IsWord(int c) { return (CType[c] & (C1_ALPHA | C1_DIGIT)) || c == '_'; }
inline int IsCType(int c, unsigned short ctype) { return CType[c] & ctype; }

inline int IsSpace(char c) { return CType[(unsigned char)c] & C1_SPACE; }
inline int IsAlpha(char c) { return CType[(unsigned char)c] & C1_ALPHA; }
inline int IsAlnum(char c) { return CType[(unsigned char)c] & (C1_ALPHA | C1_DIGIT); }
inline int IsCntrl(char c) { return CType[(unsigned char)c] & C1_CNTRL; }
inline int IsDigit(char c) { return CType[(unsigned char)c] & C1_DIGIT; }
inline int IsGraph(char c) { return CType[(unsigned char)c] & (C1_PUNCT | C1_ALPHA | C1_DIGIT); }
inline int IsLower(char c) { return CType[(unsigned char)c] & C1_LOWER; }
inline int IsPrint(char c) { return CType[(unsigned char)c] & (C1_PUNCT | C1_ALPHA | C1_DIGIT | C1_BLANK); }
inline int IsUpper(char c) { return CType[(unsigned char)c] & C1_UPPER; }
inline int IsXdigit(char c) { return CType[(unsigned char)c] & C1_XDIGIT; }
inline int IsWord(char c) { return (CType[(unsigned char)c] & (C1_ALPHA | C1_DIGIT)) || c == '_'; }
inline int IsCType(char c, unsigned short ctype) { return CType[(unsigned char)c] & ctype; }

inline char ToLower(unsigned char c) { return LowerCase[c]; }
inline char ToUpper(unsigned char c) { return UpperCase[c]; }

inline int InvertCase(unsigned char c) { return IsUpper(c) ? LowerCase[c] : UpperCase[c]; }

int MemCmpI(const char* s1, const char* s2, int n);
char* MemChrI(char* s, char c, int n);
char* StrNCat(char* dest, const char* sour, int destSize);
