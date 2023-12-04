// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

enum CParserResultEnum
{
    preOK,
    preOutOfMemory,    // nedostatek pameti pro dokonceni operace
    preUnknownFile,    // neznamy format souboru
    preOpenError,      // chyba pri otevirani souboru
    preReadError,      // chyba pri cteni ze souboru
    preWriteError,     // chyba pri zapisu do souboru
    preSeekError,      // chyba pri nastavovani pozice v souboru
    preCorruptedFile,  // corruped file
    preExtensionError, // unable to initialize Windows extensions e.g. WMA
    preCount           // jina chyba
};

void ShowParserError(HWND hParent, CParserResultEnum result);

class COutputInterface;

//****************************************************************************
//
// CParserInterface
//

class CParserInterface
{
public:
    // vola se pro otevreni pozadovaneho souboru
    virtual CParserResultEnum OpenFile(const char* fileName) = 0;

    // vola se pro zavreni prave otevreneho souboru; paruje s OpenFile
    // po zavolani CloseFile je interface povazovany za neplatny
    virtual CParserResultEnum CloseFile() = 0;

    //
    // nasledujici metody maji vyznam pouze je-li otevren soubor
    //
    virtual CParserResultEnum GetFileInfo(COutputInterface* output) = 0;
};

//****************************************************************************
//
// CreateAppropriateParser
//
// Pokusi se k nazvu souboru vytvorit instanci parseru, ktery bude schopny
// podat o souboru informace.
//

CParserResultEnum CreateAppropriateParser(const char* fileName, CParserInterface** parser);
