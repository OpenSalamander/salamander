// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// WARNING: cannot be replaced by "#pragma once" because it is included from .rc file and it seems resource compiler does not support "#pragma once"
#ifndef __EXIF_H
#define __EXIF_H

// EXIF_DLL_VERSION: zvysovat pro kazde nove releasnute EXIF.DLL
// Version 6: 2007.05.15: ConvertUTF8ToUCS2 is exported

// POZOR: verzi je potreba zvysit jeste v souboru exif.def
#define EXIF_DLL_VERSION 8

#ifndef RC_INVOKED

// vraci verzi EXIF.DLL (1, 2, ...)
typedef DWORD(WINAPI* EXIFGETVERSION)();

// EXIFENUMPROC je callback volany z EXIFGETINFO pro jednotlive pary [tag, value] z EXIF zaznamu
// 'tagNum' je numericka reprezentace tagu
// 'tagTitle' je kratky nazev tagu
// 'tagDescription' je dlouhy popis tagu (pro napovedu)
// 'value' je hodnota tagu
// 'lParam' je uzivatelska hodnota z volani EXIFGETINFO
typedef BOOL(CALLBACK* EXIFENUMPROC)(DWORD tagNum,
                                     const char* tagTitle,
                                     const char* tagDescription,
                                     const char* value,
                                     LPARAM lParam);

// EXIFGETINFO je export exif.dll, ktery vytahne EXIF informace z obrazku
// 'fileName' a bude volat 'enumFunc' pro jednotlive tagy; pri volani bude
// predavat uzivatelskou hodnotu 'lParam'
// fileName je datovy buffer delky dataLen, pokud je dataLen ruzne od nuly.
// v pripade neuspechu (nenalezeni EXIF zaznamu) vrati FALSE, jinak vola 'enumFunc'
// a na zaver vrati TRUE
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
