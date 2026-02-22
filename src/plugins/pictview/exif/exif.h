// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// WARNING: cannot be replaced by "#pragma once" because it is included from .rc file and it seems resource compiler does not support "#pragma once"
#ifndef __EXIF_H
#define __EXIF_H

// EXIF_DLL_VERSION: increase it for each newly released EXIF.DLL
// Version 6: 2007.05.15: ConvertUTF8ToUCS2 is exported

// NOTE: the version also needs to be increased in the exif.def file
#define EXIF_DLL_VERSION 8

#ifndef RC_INVOKED

// returns the EXIF.DLL version (1, 2, ...)
typedef DWORD(WINAPI* EXIFGETVERSION)();

// EXIFENUMPROC is a callback called from EXIFGETINFO for individual [tag, value] pairs from the EXIF record
// 'tagNum' is a numeric representation of the tag
// 'tagTitle' is a short tag name
// 'tagDescription' is a long description of the tag (for help)
// 'value' is the tag value
// 'lParam' is the user value from the EXIFGETINFO call
typedef BOOL(CALLBACK* EXIFENUMPROC)(DWORD tagNum,
                                     const char* tagTitle,
                                     const char* tagDescription,
                                     const char* value,
                                     LPARAM lParam);

// EXIFGETINFO is an exif.dll export that extracts EXIF information from the image 'fileName'
// and calls 'enumFunc' for individual tags; each call passes along the user value 'lParam'
// fileName is a data buffer of length dataLen if dataLen is non-zero.
// if unsuccessful (EXIF record not found) it returns FALSE; otherwise it calls 'enumFunc'
// and finally returns TRUE
typedef BOOL(WINAPI* EXIFGETINFO)(const char* fileName,
                                  int dataLen,
                                  EXIFENUMPROC enumFunc,
                                  LPARAM lParam);

typedef BOOL(WINAPI* EXIFREPLACETHUMBNAIL)(char* fileName, char* newFile,
                                           unsigned char* pData, int size);

typedef void(WINAPI* EXIFINITTRANSLATIONS)(LPCTSTR fname);

typedef void(WINAPI* CONVERTUTF8TOUCS2)(const char* in, LPWSTR out);

// Extracting orientation from EXIF section

#define TEI_ORIENT 1
#define TEI_WIDTH 2
#define TEI_HEIGHT 4
#define TEI_ALL (TEI_ORIENT | TEI_WIDTH | TEI_HEIGHT)

typedef struct _thumbExifInfo
{
    int Width;
    int Height;
    int Orient;
    int flags; // TEI_xxx
} SThumbExifInfo, *PThumbExifInfo;

typedef BOOL(WINAPI* EXIFGETORIENTATIONINFO)(const char* filename, PThumbExifInfo pInfo);

#ifdef __cplusplus
extern "C"
{
#endif
    void FreeTranslations(void);
    const char* TranslateText(const char* txt);
#ifdef __cplusplus
}
#endif

#endif // RC_INVOKED

#endif // __EXIF_H
