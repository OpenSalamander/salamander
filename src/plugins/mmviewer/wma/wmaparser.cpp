// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _WMA_SUPPORT_

#include "wmaparser.h"
#include "..\output.h"
#include "..\renderer.h"
#include "..\mmviewer.h"
#include "..\mmviewer.rh"
#include "..\mmviewer.rh2"
#include "..\lang\lang.rh"
#include "..\output.h"

extern char* LoadStr(int resID);

char* FStr(const char* format, ...);

typedef HRESULT(STDMETHODCALLTYPE* WMCREATEEDITOR)(IWMMetadataEditor** ppEditor);

WMCREATEEDITOR wiWMCreateEditor;

#ifndef SAFE_RELEASE

#define SAFE_RELEASE(x) \
    if (NULL != x) \
    { \
        x->Release(); \
        x = NULL; \
    }

#endif // SAFE_RELEASE

typedef struct
{
    LPCWSTR tag;
    int str_id;
    char* str;
} WMA_TAG;

#define GUIDSTR_MAX 38
#define GUID_STRING_SIZE 64
static const BYTE GuidMap[] = {3, 2, 1, 0, '-', 5, 4, '-', 7, 6, '-', 8, 9, '-', 10, 11, 12, 13, 14, 15};
static const CHAR szDigits[] = "0123456789ABCDEF";

int StringFromGUID(LPGUID lpguid, LPSTR lpsz)
{
    int i;

    const BYTE* pBytes = (const BYTE*)lpguid;

    *lpsz++ = '{';

    for (i = 0; i < sizeof(GuidMap); i++)
    {
        if (GuidMap[i] == '-')
            *lpsz++ = '-';
        else
        {
            *lpsz++ = szDigits[(pBytes[GuidMap[i]] & 0xF0) >> 4];
            *lpsz++ = szDigits[(pBytes[GuidMap[i]] & 0x0F)];
        }
    }
    *lpsz++ = '}';
    *lpsz = '\0';

    return GUIDSTR_MAX;
}

HRESULT CParserWMA::GetHeaderAttribute(IWMHeaderInfo* pHdrInfo, LPCWSTR pwszName, BOOL* pbIsPresent, char** output)
{
    HRESULT hr = S_OK;

    *pbIsPresent = FALSE;

    WORD nstreamNum = 0;
    WORD cbLength = 0;
    BYTE* pValue = NULL;

    static char out[512];
    *output = &out[0];

    WMT_ATTR_DATATYPE type;

    hr = pHdrInfo->GetAttributeByName(&nstreamNum, pwszName, &type, NULL, &cbLength);

    if (FAILED(hr) && hr != ASF_E_NOTFOUND)
    {
        TRACE_E(FStr("GetAttributeByName failed for Attribute name %ws (hr=0x%08x).\n", pwszName, hr));
        return hr;
    }

    if (cbLength == 0 && hr == ASF_E_NOTFOUND)
    {
        hr = S_OK;
        return hr;
    }

    pValue = new BYTE[cbLength];

    if (NULL == pValue)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        TRACE_E(FStr("Internal Error (hr=0x%08x).\n", hr));
        return hr;
    }

    hr = pHdrInfo->GetAttributeByName(&nstreamNum, pwszName, &type, pValue, &cbLength);
    if (FAILED(hr))
    {
        delete[] pValue;
        TRACE_E(FStr("GetAttributeByName failed for Attribute name %ws (hr=0x%08x).\n", pwszName, hr));
        return hr;
    }

    switch (type)
    {
    case WMT_TYPE_DWORD:
        wsprintf(out, "%lu", *((DWORD*)pValue));
        break;

    case WMT_TYPE_STRING:
        *output = WideToAnsi((LPCWSTR)pValue, cbLength / 2);
        break;

    case WMT_TYPE_BINARY:
        wsprintf(out, LoadStr(IDS_WMA_BINARY), cbLength);
        break;

    case WMT_TYPE_BOOL:
        (*(BOOL*)pValue) ? lstrcpy(out, LoadStr(IDS_YES)) : lstrcpy(out, LoadStr(IDS_NO));
        break;

    case WMT_TYPE_WORD:
        wsprintf(out, "%u", *((WORD*)pValue));
        break;

    case WMT_TYPE_QWORD:
        _i64toa(*((QWORD*)pValue), &out[0], 10);
        break;

    case WMT_TYPE_GUID:
        StringFromGUID((GUID*)pValue, &out[0]);
        break;

    default:
        hr = E_INVALIDARG;
        break;
    }

    delete[] pValue;
    pValue = NULL;

    *pbIsPresent = TRUE;

    return hr;
}

CParserWMA::CParserWMA() : pEditor(NULL), pHeaderInfo(NULL), module(NULL)
{
    module = LoadLibrary("wmvcore.dll");

    if (module)
    {
        if ((wiWMCreateEditor = (WMCREATEEDITOR)GetProcAddress(module, "WMCreateEditor")) != NULL) // Windows Media
        {
            //OK
        }
        else
        {
            //chyba
            FreeLibrary(module);
        }
    }
}

CParserResultEnum
CParserWMA::OpenFile(LPCSTR fileName)
{
    if (!module)
        return preExtensionError;

    CloseFile();

    HRESULT hr = S_OK;
    LPCWSTR pwszInFile = NULL;

    hr = wiWMCreateEditor(&pEditor);
    if (FAILED(hr))
    {
        TRACE_E(FStr("Could not create Metadata Editor (hr=0x%08x).\n", hr));
        return preExtensionError;
    }

#ifdef _UNICODE
    pwszInFile = fileName;
#else
    pwszInFile = AnsiToWide(fileName, (int)strlen(fileName));
#endif

    hr = pEditor->Open(pwszInFile);
    if (FAILED(hr))
    {
        TRACE_E(FStr("Could not open outfile %ws (hr=0x%08x).\n", pwszInFile, hr));
        return preOpenError;
    }

    hr = pEditor->QueryInterface(IID_IWMHeaderInfo, (void**)&pHeaderInfo);
    if (FAILED(hr))
    {
        TRACE_E(FStr("Could not QI for IWMHeaderInfo (hr=0x%08x).\n", hr));
        return preExtensionError;
    }

    return preOK;
}

CParserResultEnum
CParserWMA::CloseFile()
{
    SAFE_RELEASE(pHeaderInfo);

    if (pEditor)
        pEditor->Close();

    SAFE_RELEASE(pEditor);

    return preOK;
}

CParserResultEnum
CParserWMA::GetFileInfo(COutputInterface* output)
{
    if (pEditor) //aspon nejaka kontrola
    {
        // v tomto poradi budou tagy vypisovany. prehazujte poradi dle libosti
        WMA_TAG wmatags[] = {
            {g_wszWMBitrate, IDS_WMA_BITRATE, NULL},
            {g_wszWMDuration, IDS_WMA_DURATION, NULL},
            {g_wszWMTrack, IDS_WMA_TRACK, NULL},
            {g_wszWMTitle, IDS_WMA_TITLE, NULL},
            {g_wszWMAuthor, IDS_WMA_AUTHOR, NULL},
            {g_wszWMAlbumTitle, IDS_WMA_ALBUMTITLE, NULL},
            {g_wszWMYear, IDS_WMA_YEAR, NULL},
            {g_wszWMGenre, IDS_WMA_GENRE, NULL},
            {g_wszWMSignature_Name, IDS_WMA_SIGNATURENAME, NULL},
            {g_wszWMDescription, IDS_WMA_DESCRIPTION, NULL},
            {g_wszWMCopyright, IDS_WMA_COPYRIGHT, NULL},
            {g_wszWMRating, IDS_WMA_RATING, NULL},
            {g_wszWMPromotionURL, IDS_WMA_PROMOTIONURL, NULL},
            {g_wszWMAlbumCoverURL, IDS_WMA_ALBUMCOVERURL, NULL},
            {g_wszWMMCDI, IDS_WMA_MCDI, NULL},
            {g_wszWMBannerImageType, IDS_WMA_BANNERIMAGETYPE, NULL},
            {g_wszWMBannerImageData, IDS_WMA_BANNERIMAGEDATA, NULL},
            {g_wszWMBannerImageURL, IDS_WMA_BANNERIMAGEURL, NULL},
            {g_wszWMCopyrightURL, IDS_WMA_COPYRIGHTURL, NULL},
            {g_wszWMNSCName, IDS_WMA_NSCNAME, NULL},
            {g_wszWMNSCAddress, IDS_WMA_NSCADDRESS, NULL},
            {g_wszWMNSCPhone, IDS_WMA_NSCPHONE, NULL},
            {g_wszWMNSCEmail, IDS_WMA_NSCEMAIL, NULL},
            {g_wszWMNSCDescription, IDS_WMA_NSCDESCRIPTION, NULL},
            {g_wszWMSeekable, IDS_WMA_SEEKABLE, NULL},
            {g_wszWMStridable, IDS_WMA_STRIDABLE, NULL},
            {g_wszWMBroadcast, IDS_WMA_BROADCAST, NULL},
            {g_wszWMProtected, IDS_WMA_PROTECTED, NULL},
            {g_wszWMTrusted, IDS_WMA_TRUSTED, NULL},
        };

        int countTags = sizeof(wmatags) / sizeof(WMA_TAG);

        char* out;
        BOOL isPresent = FALSE;
        HRESULT hr;
        int i;

        for (i = 0; i < countTags; i++)
        {
            hr = GetHeaderAttribute(pHeaderInfo, wmatags[i].tag, &isPresent, &out);

            if (FAILED(hr))
                break;

            if (isPresent && out[0])
            {
                if (wmatags[i].tag == g_wszWMDuration)
                {
                    int dur = (int)(_atoi64(out) / 10000000);
                    if (dur / 3600)
                        wmatags[i].str = SalGeneral->DupStr(FStr("%02lu:%02lu:%02lu", dur / 3600, dur / 60 % 60, dur % 60));
                    else
                        wmatags[i].str = SalGeneral->DupStr(FStr("%02lu:%02lu", dur / 60 % 60, dur % 60));
                }
                else if (wmatags[i].tag == g_wszWMBitrate)
                {
                    wmatags[i].str = SalGeneral->DupStr(FStr("%lu", atol(out) / 1000));
                }
                else
                {
                    wmatags[i].str = SalGeneral->DupStr(out);
                }
            }
        }

        // dump
        output->AddHeader(LoadStr(IDS_WMA_INFO));
        for (i = 0; i < countTags; i++)
            if (wmatags[i].str)
            {
                output->AddItem(LoadStr(wmatags[i].str_id), wmatags[i].str);
                SalGeneral->Free(wmatags[i].str);
            }

        return preOK;
    }

    return preUnknownFile;
}

#endif
