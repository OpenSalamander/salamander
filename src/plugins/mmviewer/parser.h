// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

enum CParserResultEnum
{
    preOK,
    preOutOfMemory,    // insufficient memory to complete the operation
    preUnknownFile,    // unknown file format
    preOpenError,      // error while opening the file
    preReadError,      // error while reading from the file
    preWriteError,     // error while writing to the file
    preSeekError,      // error while setting the position in the file
    preCorruptedFile,  // corruped file
    preExtensionError, // unable to initialize Windows extensions e.g. WMA
    preCount           // another error
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
    // called to open the requested file
    virtual CParserResultEnum OpenFile(const char* fileName) = 0;

    // called to close the currently opened file; pairs with OpenFile
    // after CloseFile is called the interface is considered invalid
    virtual CParserResultEnum CloseFile() = 0;

    //
    // the following methods make sense only when a file is open
    //
    virtual CParserResultEnum GetFileInfo(COutputInterface* output) = 0;
};

//****************************************************************************
//
// CreateAppropriateParser
//
// Attempts to create a parser instance for the file name that will be able
// to provide information about the file.
//

CParserResultEnum CreateAppropriateParser(const char* fileName, CParserInterface** parser);
