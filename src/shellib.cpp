// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "shellib.h"
#include "cfgdlg.h"
#include "plugins.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"

// puvodni umisteni v fileswnd.h (zde je jen kvuli MakeCopyOfName v CImpDropTarget::ProcessClipboardData)
extern BOOL OurClipDataObject; // TRUE pri "paste" naseho IDataObject
                               // (detekce vlastni copy/move rutiny s cizimi daty)

void* LastSafeDataObject = NULL;

DWORD ExecuteAssociationTlsIndex = TLS_OUT_OF_INDEXES; // dovoli jen jedno volani zaroven (zamezi rekurzi) v kazdem threadu

BOOL DragFromPluginFSEffectIsFromPlugin = FALSE;

//*****************************************************************************
//
// CCopyMoveRecord
//

CCopyMoveRecord::CCopyMoveRecord(const char* fileName, const char* mapName)
{
    FileName = AllocChars(fileName);
    MapName = AllocChars(mapName);
}

CCopyMoveRecord::CCopyMoveRecord(const wchar_t* fileName, const char* mapName)
{
    FileName = AllocChars(fileName);
    MapName = AllocChars(mapName);
}

CCopyMoveRecord::CCopyMoveRecord(const char* fileName, const wchar_t* mapName)
{
    FileName = AllocChars(fileName);
    MapName = AllocChars(mapName);
}

CCopyMoveRecord::CCopyMoveRecord(const wchar_t* fileName, const wchar_t* mapName)
{
    FileName = AllocChars(fileName);
    MapName = AllocChars(mapName);
}

char* CCopyMoveRecord::AllocChars(const char* name)
{
    if (name == NULL)
        return NULL;

    int l = (int)strlen(name);
    char* newName = (char*)malloc(l + 1);
    if (newName != NULL)
        memcpy(newName, name, l + 1);
    else
        TRACE_E(LOW_MEMORY);
    return newName;
}

char* CCopyMoveRecord::AllocChars(const wchar_t* name)
{
    if (name == NULL)
        return NULL;

    int l = lstrlenW(name);
    char* newName = (char*)malloc(l + 1);
    if (newName != NULL)
    {
        WideCharToMultiByte(CP_ACP, 0, name, l + 1, newName, l + 1, NULL, NULL);
        newName[l] = 0;
    }
    else
        TRACE_E(LOW_MEMORY);
    return newName;
}

//*****************************************************************************
//
// DestroyCopyMoveData
//

void DestroyCopyMoveData(CCopyMoveData* data)
{
    if (data != NULL)
    {
        int i;
        for (i = 0; i < data->Count; i++)
        {
            if (data->At(i)->FileName != NULL)
                free(data->At(i)->FileName);
            if (data->At(i)->MapName != NULL)
                free(data->At(i)->MapName);
        }
        delete data;
    }
}

//*****************************************************************************
//
// CImpDropTarget
//

void CImpDropTarget::SetDirectory(const char* path, DWORD grfKeyState, POINTL pt,
                                  DWORD* effect, IDataObject* dataObject, BOOL tgtIsFile,
                                  int tgtType)
{
    CALL_STACK_MESSAGE5("CImpDropTarget::SetDirectory(%s, 0x%X, , , , %d, %d)", path,
                        grfKeyState, tgtIsFile, tgtType);

    if (path == NULL)
    {
        if (CurDirDropTarget != NULL)
        {
            CurDirDropTarget->DragLeave();
            CurDirDropTarget->Release();
        }
        CurDirDropTarget = NULL;
        CurDir[0] = 0;
        TgtType = idtttWindows;
        return;
    }

    TgtType = tgtType;
    if (tgtType == idtttWindows)
    {
        if (strcmp(path, CurDir) != 0 || CurDirDropTarget == NULL)
        {
            if (CurDirDropTarget != NULL)
            {
                CurDirDropTarget->DragLeave();
                CurDirDropTarget->Release();
            }
            if (tgtIsFile && dataObject != NULL && IsFakeDataObject(dataObject, NULL, NULL, 0))
                CurDirDropTarget = NULL;
            else
                CurDirDropTarget = CreateIDropTarget(OwnerWindow, path);
            if (CurDirDropTarget != NULL && dataObject != NULL && effect != NULL)
            {
                if (CurDirDropTarget->DragEnter(dataObject, grfKeyState, pt, effect) != S_OK)
                { // chyba drop-targetu -> uvolnime ho
                    CurDirDropTarget->Release();
                    CurDirDropTarget = NULL;
                    CurDir[0] = 0;
                    return;
                }
            }
            strcpy(CurDir, path);
        }
    }
    else // archivy + FS
    {
        if (CurDirDropTarget != NULL)
        {
            CurDirDropTarget->DragLeave();
            CurDirDropTarget->Release();
        }
        CurDirDropTarget = NULL;
        strcpy(CurDir, path);
    }
}

const char* FindNextString(const char* txt)
{
    while (*txt++ != 0)
        ;
    return txt;
}

const wchar_t* FindNextString(const wchar_t* txt)
{
    while (*txt++ != 0)
        ;
    return txt;
}

BOOL CImpDropTarget::ProcessClipboardData(BOOL copy, const DROPFILES* data,
                                          const char* mapA, const wchar_t* mapW)
{
    CALL_STACK_MESSAGE4("CImpDropTarget::ProcessClipboardData(%d, , %s, %S)", copy, mapA, mapW);
    BOOL ret = FALSE;
    CCopyMoveData* array = new CCopyMoveData(100, 50);
    if (array != NULL)
    {
        // array->MakeCopyOfName bude TRUE pokud jde o nas vlastni copy & paste z clipboardu
        // (kopirovani s tim, ze pokud jiz cil existuje, bude se vytvaret "Copy of ...")
        //    array->MakeCopyOfName = copy && OurClipDataObject && mapA == NULL && mapW == NULL;  // aby chodilo i pres drag&drop
        array->MakeCopyOfName = copy && mapA == NULL && mapW == NULL; // sem se dostane jen nas data-object

        if (data->fWide)
        {
            const wchar_t* fileW = (wchar_t*)(((char*)data) + data->pFiles);
            while (1) // double null terminated, nepocita s prazdnymi stringy (zacatek)
            {
                if (*fileW == 0)
                {
                    ret = DoCopyMove(copy, CurDir, array, DoCopyMoveParam);
                    array = NULL; // uvolnuje DoCopyMove
                    break;
                }
                CCopyMoveRecord* cr;
                if (mapA != NULL)
                    cr = new CCopyMoveRecord(fileW, mapA);
                else
                    cr = new CCopyMoveRecord(fileW, mapW);
                if (cr != NULL)
                    array->Add(cr);
                else
                    break;
                if (!array->IsGood())
                {
                    array->ResetState();
                    break;
                }
                fileW = FindNextString(fileW);
                if (mapA != NULL && *mapA != 0)
                    mapA = FindNextString(mapA);
                else if (mapW != NULL && *mapW != 0)
                    mapW = FindNextString(mapW);
            }
        }
        else
        {
            const char* fileA = ((char*)data) + data->pFiles;
            while (1) // double null terminated, nepocita s prazdnymi stringy (zacatek)
            {
                if (*fileA == 0)
                {
                    ret = DoCopyMove(copy, CurDir, array, DoCopyMoveParam);
                    array = NULL; // uvolnuje DoCopyMove
                    break;
                }
                CCopyMoveRecord* cr;
                if (mapA != NULL)
                    cr = new CCopyMoveRecord(fileA, mapA);
                else
                    cr = new CCopyMoveRecord(fileA, mapW);
                if (cr != NULL)
                    array->Add(cr);
                else
                    break;
                if (!array->IsGood())
                {
                    array->ResetState();
                    break;
                }
                fileA = FindNextString(fileA);
                if (mapA != NULL && *mapA != 0)
                    mapA = FindNextString(mapA);
                else if (mapW != NULL && *mapW != 0)
                    mapW = FindNextString(mapW);
            }
        }
        if (array != NULL)
            DestroyCopyMoveData(array);
    }
    return ret;
}

BOOL CImpDropTarget::TryCopyOrMove(BOOL copy, IDataObject* pDataObject, UINT CF_FileMapA,
                                   UINT CF_FileMapW, BOOL cfFileMapA, BOOL cfFileMapW)
{
    CALL_STACK_MESSAGE2("CImpDropTarget::TryCopyOrMove(%d, , , , ,)", copy);

    FORMATETC formatEtc;
    formatEtc.cfFormat = CF_HDROP;
    formatEtc.ptd = NULL;
    formatEtc.dwAspect = DVASPECT_CONTENT;
    formatEtc.lindex = -1;
    formatEtc.tymed = TYMED_HGLOBAL;

    STGMEDIUM stgMedium;
    stgMedium.tymed = TYMED_HGLOBAL;
    stgMedium.hGlobal = NULL;
    stgMedium.pUnkForRelease = NULL;

    BOOL ret = FALSE;
    if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
    {
        if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
        {
            DROPFILES* data = (DROPFILES*)HANDLES(GlobalLock(stgMedium.hGlobal));
            if (data != NULL)
            {
                if (cfFileMapA || cfFileMapW)
                {
                    formatEtc.cfFormat = (CLIPFORMAT)(cfFileMapA ? CF_FileMapA : CF_FileMapW);
                    formatEtc.ptd = NULL;
                    formatEtc.dwAspect = DVASPECT_CONTENT;
                    formatEtc.lindex = -1;
                    formatEtc.tymed = TYMED_HGLOBAL;

                    STGMEDIUM stgMediumMap;
                    stgMediumMap.tymed = TYMED_HGLOBAL;
                    stgMediumMap.hGlobal = NULL;
                    stgMediumMap.pUnkForRelease = NULL;

                    if (pDataObject->GetData(&formatEtc, &stgMediumMap) == S_OK)
                    {
                        if (stgMediumMap.tymed == TYMED_HGLOBAL && stgMediumMap.hGlobal != NULL)
                        {
                            void* map = HANDLES(GlobalLock(stgMediumMap.hGlobal));

                            if (map != NULL)
                            {
                                ret = ProcessClipboardData(copy, data,
                                                           (char*)(cfFileMapA ? map : NULL),
                                                           (wchar_t*)(cfFileMapA ? NULL : map));
                                HANDLES(GlobalUnlock(stgMediumMap.hGlobal));
                            }
                        }
                        ReleaseStgMedium(&stgMediumMap);
                    }
                }
                else
                    ret = ProcessClipboardData(copy, data, NULL, NULL);
                HANDLES(GlobalUnlock(stgMedium.hGlobal));
            }
        }
        ReleaseStgMedium(&stgMedium);
    }
    return ret;
}

BOOL IsSimpleSelection(IDataObject* pDataObject, CDragDropOperData* namesList)
{
    CALL_STACK_MESSAGE1("IsSimpleSelection()");
    BOOL ret = FALSE;
    if (pDataObject != NULL && !IsFakeDataObject(pDataObject, NULL, NULL, 0)) // z archivu/FS se to tudy neprijima
    {
        IEnumFORMATETC* enumFormat;
        if (pDataObject->EnumFormatEtc(DATADIR_GET, &enumFormat) == S_OK)
        {
            BOOL cfHDrop = FALSE;
            BOOL cfFileMapA = FALSE;
            BOOL cfFileMapW = FALSE;
            UINT CF_FileMapA = RegisterClipboardFormat(CFSTR_FILENAMEMAPA);
            UINT CF_FileMapW = RegisterClipboardFormat(CFSTR_FILENAMEMAPW);

            // Windows XP Remote Desktop problem, viz https://forum.altap.cz/viewtopic.php?p=13176#13176
            // Pokud detekujeme zkracene verze nazvu formatu, jde s nejvyssi pravdepodobnosti o Remote Desktop
            // a nesmime zavolat pDataObject->GetData(), protoze bychom na vzdalenem stroji spustili kopirovani
            // soubor k nam do tempu a po tu doby bychom byli zamrzly
            // Od Windows Vista uz je problem opraveny a zaroven uz nejsou oreazen nazvy, takze tento patch
            // se dotkane pouze XP.
            BOOL cfRemoteDesktop1 = FALSE;
            BOOL cfRemoteDesktop2 = FALSE;
            BOOL cfRemoteDesktop3 = FALSE;
            UINT CF_RemoteDesktop1 = RegisterClipboardFormat("Preferred DropEf");
            UINT CF_RemoteDesktop2 = RegisterClipboardFormat("Shell Object Off");
            UINT CF_RemoteDesktop3 = RegisterClipboardFormat("Shell IDList Arr");

            FORMATETC formatEtc;
            enumFormat->Reset();
            while (enumFormat->Next(1, &formatEtc, NULL) == S_OK)
            {
                // debug only
                // char formatName[1000];
                // if (GetClipboardFormatName(formatEtc.cfFormat, formatName, 1000) == 0)
                //   formatName[0] = 0;
                // TRACE_I("formatEtc.cfFormat="<<formatEtc.cfFormat<<" tymed="<<formatEtc.tymed<<" name:"<<formatName);

                if (formatEtc.cfFormat == CF_FileMapA)
                    cfFileMapA = TRUE;
                if (formatEtc.cfFormat == CF_FileMapW)
                    cfFileMapW = TRUE;
                if (formatEtc.cfFormat == CF_HDROP)
                    cfHDrop = TRUE;
                if (formatEtc.cfFormat == CF_RemoteDesktop1)
                    cfRemoteDesktop1 = TRUE;
                if (formatEtc.cfFormat == CF_RemoteDesktop2)
                    cfRemoteDesktop2 = TRUE;
                if (formatEtc.cfFormat == CF_RemoteDesktop3)
                    cfRemoteDesktop3 = TRUE;
            }
            enumFormat->Release();

            BOOL remoteDesktop = cfRemoteDesktop1 && cfRemoteDesktop2 && cfRemoteDesktop3; // pochazeji data z remote desktop

            if (cfHDrop && !cfFileMapA && !cfFileMapW && !remoteDesktop) // zadny mapovani (blokujeme Recycle Bin)
            {
                FORMATETC formatEtc2;
                formatEtc2.cfFormat = CF_HDROP;
                formatEtc2.ptd = NULL;
                formatEtc2.dwAspect = DVASPECT_CONTENT;
                formatEtc2.lindex = -1;
                formatEtc2.tymed = TYMED_HGLOBAL;

                STGMEDIUM stgMedium;
                stgMedium.tymed = TYMED_HGLOBAL;
                stgMedium.hGlobal = NULL;
                stgMedium.pUnkForRelease = NULL;

                if (pDataObject->GetData(&formatEtc2, &stgMedium) == S_OK)
                {
                    if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
                    {
                        DROPFILES* data = (DROPFILES*)HANDLES(GlobalLock(stgMedium.hGlobal));
                        if (data != NULL)
                        {
                            int prefixLen = -1;
                            wchar_t prefixBuf[MAX_PATH];
                            prefixBuf[0] = 0;
                            if (data->fWide)
                            {
                                char mulbyteName[MAX_PATH];
                                wchar_t* prefix = prefixBuf;
                                const wchar_t* fileW = (wchar_t*)(((char*)data) + data->pFiles);
                                while (1) // double null terminated, nepocita s prazdnymi stringy (zacatek)
                                {
                                    if (*fileW == 0) // uz nejsou zadna dalsi jmena, uspech!
                                    {
                                        if (namesList != NULL) // pridame do namesList spolecnou cestu vsech jmen
                                        {
                                            if (WideCharToMultiByte(CP_ACP, 0, prefix, prefixLen + 1, mulbyteName, MAX_PATH, NULL, NULL) == 0)
                                            {
                                                DWORD err = GetLastError();
                                                TRACE_E("IsSimpleSelection(): WideCharToMultiByte: " << GetErrorText(err));
                                                mulbyteName[0] = 0;
                                            }
                                            strcpy(namesList->SrcPath, mulbyteName);
                                            if (prefixLen < 3)
                                                SalPathAddBackslash(namesList->SrcPath, MAX_PATH);
                                        }
                                        ret = TRUE;
                                        break;
                                    }

                                    // test na spolecnou cestu vsech obsazenych jmen
                                    const wchar_t* s = fileW;
                                    const wchar_t* lastBackslash = NULL; // posledni backslash (krome toho na konci retezce)
                                    while (*s != 0)
                                    {
                                        if (*s == L'\\' && *(s + 1) != 0)
                                            lastBackslash = s;
                                        s++;
                                    }
                                    if (lastBackslash != NULL)
                                    {
                                        if (lastBackslash - fileW == prefixLen)
                                        {
                                            if (CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, fileW,
                                                               prefixLen, prefix, prefixLen) != CSTR_EQUAL)
                                            {
                                                ret = FALSE; // zmena cesty, chyba
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            if (prefixLen == -1)
                                            {
                                                prefixLen = (int)(lastBackslash - fileW);
                                                if (prefixLen >= MAX_PATH)
                                                    prefixLen = MAX_PATH - 1;
                                                memmove(prefix, fileW, prefixLen * sizeof(wchar_t));
                                                prefix[prefixLen] = 0;
                                            }
                                            else
                                            {
                                                ret = FALSE; // zmena cesty, chyba
                                                break;
                                            }
                                        }

                                        if (namesList != NULL) // pridame do namesList aktualni jmeno souboru nebo adresare
                                        {
                                            if (s > fileW && *(s - 1) == L'\\')
                                                s--; // pripadny orez koncoveho backslashe
                                            int len;
                                            if ((len = WideCharToMultiByte(CP_ACP, 0, lastBackslash + 1,
                                                                           (int)(s - (lastBackslash + 1)), mulbyteName,
                                                                           MAX_PATH, NULL, NULL)) == 0)
                                            {
                                                DWORD err = GetLastError();
                                                TRACE_E("IsSimpleSelection(): WideCharToMultiByte: " << GetErrorText(err));
                                                mulbyteName[0] = 0;
                                            }
                                            else
                                                mulbyteName[min(MAX_PATH - 1, len)] = 0;
                                            char* add = DupStr(mulbyteName);
                                            if (add != NULL)
                                            {
                                                namesList->Names.Add(add);
                                                if (!namesList->Names.IsGood())
                                                {
                                                    namesList->Names.ResetState();
                                                    free(add);
                                                    ret = FALSE; // nedostatek pameti pro jmena souboru/adresaru, chyba
                                                    break;
                                                }
                                            }
                                            else
                                            {
                                                ret = FALSE; // nedostatek pameti pro jmena souboru/adresaru, chyba
                                                break;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        ret = FALSE; // neni plna cesta, chyba
                                        break;
                                    }

                                    fileW = FindNextString(fileW);
                                }
                            }
                            else
                            {
                                char* prefix = (char*)prefixBuf;
                                const char* fileA = ((char*)data) + data->pFiles;
                                while (1) // double null terminated, nepocita s prazdnymi stringy (zacatek)
                                {
                                    if (*fileA == 0) // uz nejsou zadna dalsi jmena, uspech!
                                    {
                                        if (namesList != NULL) // pridame do namesList spolecnou cestu vsech jmen
                                        {
                                            strcpy(namesList->SrcPath, prefix);
                                            if (prefixLen < 3)
                                                SalPathAddBackslash(namesList->SrcPath, MAX_PATH);
                                        }
                                        ret = TRUE;
                                        break;
                                    }

                                    // test na spolecnou cestu vsech obsazenych jmen
                                    const char* s = fileA;
                                    const char* lastBackslash = NULL; // posledni backslash (krome toho na konci retezce)
                                    while (*s != 0)
                                    {
                                        if (*s == '\\' && *(s + 1) != 0)
                                            lastBackslash = s;
                                        s++;
                                    }
                                    if (lastBackslash != NULL)
                                    {
                                        if (lastBackslash - fileA == prefixLen)
                                        {
                                            if (StrICmpEx(fileA, prefixLen, prefix, prefixLen) != 0)
                                            {
                                                ret = FALSE; // zmena cesty, chyba
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            if (prefixLen == -1)
                                            {
                                                prefixLen = (int)(lastBackslash - fileA);
                                                if (prefixLen >= MAX_PATH)
                                                    prefixLen = MAX_PATH - 1;
                                                memmove(prefix, fileA, prefixLen);
                                                prefix[prefixLen] = 0;
                                            }
                                            else
                                            {
                                                ret = FALSE; // zmena cesty, chyba
                                                break;
                                            }
                                        }

                                        if (namesList != NULL) // pridame do namesList aktualni jmeno souboru nebo adresare
                                        {
                                            if (s > fileA && *(s - 1) == '\\')
                                                s--; // pripadny orez koncoveho backslashe
                                            char* add = (char*)malloc(s - (lastBackslash + 1) + 1);
                                            if (add != NULL)
                                            {
                                                memcpy(add, lastBackslash + 1, s - (lastBackslash + 1));
                                                add[s - (lastBackslash + 1)] = 0;
                                                namesList->Names.Add(add);
                                                if (!namesList->Names.IsGood())
                                                {
                                                    namesList->Names.ResetState();
                                                    free(add);
                                                    ret = FALSE; // nedostatek pameti pro jmena souboru/adresaru, chyba
                                                    break;
                                                }
                                            }
                                            else
                                            {
                                                ret = FALSE; // nedostatek pameti pro jmena souboru/adresaru, chyba
                                                break;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        ret = FALSE; // neni plna cesta, chyba
                                        break;
                                    }

                                    fileA = FindNextString(fileA);
                                }
                            }
                            HANDLES(GlobalUnlock(stgMedium.hGlobal));
                        }
                    }
                    ReleaseStgMedium(&stgMedium);
                }
            }
        }
    }
    return ret;
}

STDMETHODIMP CImpDropTarget::QueryInterface(REFIID refiid, void FAR* FAR* ppv)
{
    if (refiid == IID_IUnknown || refiid == IID_IDropTarget)
    {
        *ppv = this;
        AddRef();
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP CImpDropTarget::DragEnter(IDataObject* pDataObject,
                                       DWORD grfKeyState,
                                       POINTL pt, DWORD* pdwEffect)
{
    CALL_STACK_MESSAGE2("CImpDropTarget::DragEnter(, 0x%X, ,)", grfKeyState);

    DWORD origEffect = *pdwEffect;
    DWORD origKeyState = grfKeyState;
    if (EnterLeaveDrop != NULL)
        EnterLeaveDrop(TRUE, EnterLeaveDropParam);
    RButton = (grfKeyState & MK_RBUTTON) && !(grfKeyState & MK_LBUTTON);

    if (OldDataObject != NULL)
        OldDataObject->Release();
    OldDataObject = pDataObject;
    OldDataObjectIsFake = IsFakeDataObject(OldDataObject, &OldDataObjectSrcType,
                                           OldDataObjectSrcFSPath, 2 * MAX_PATH);

    OldDataObjectIsSimple = -1; // neznama hodnota
    OldDataObject->AddRef();

    if (ImageDragging)
        ImageDragEnter(pt.x, pt.y);
    if (GetCurDir != NULL)
    {
        BOOL tgtFile;
        int tgtType;
        const char* tgtPath = GetCurDir(pt, GetCurDirParam, pdwEffect, RButton, tgtFile,
                                        grfKeyState, tgtType, OldDataObjectSrcType);
        SetDirectory(tgtPath, 0, pt, NULL, OldDataObject, tgtFile, tgtType);
        if (TgtType != idtttWindows && TgtType != idtttFullPluginFSPath)
        { // neni-li oznaceni z jedne cesty (hrozi asi jen u Findu), neumime copy/move do archivu ani FS
            OldDataObjectIsSimple = IsSimpleSelection(OldDataObject, NULL);
            if (!OldDataObjectIsSimple)
                SetDirectory(NULL, 0, pt, NULL, OldDataObject, FALSE, idtttWindows);
        }
    }

    if (CurDirDropTarget != NULL) // jen idtttWindows
    {
        HRESULT res = CurDirDropTarget->DragEnter(pDataObject, grfKeyState, pt, pdwEffect);
        if (res != S_OK) // chyba drop-targetu - ohlasime ji jako "none" drop-effect, protoze
        {                // ostatni drop-targety v panelu muzou stale fungovat
            LastEffect = -1;
            *pdwEffect = DROPEFFECT_NONE;
            CurDirDropTarget->Release(); // zrusime drop-target, aby se mu nezavolal drag-over
            CurDirDropTarget = NULL;
        }
        else
        {
            if (OldDataObjectIsFake)
            { // nas data-object (nemusi byt z tohoto procesu): defaultni je Copy (fake je v TEMPU, na stejnem disku to delalo defaultne Move, tak to takhle obejdeme)
                if ((origKeyState & MK_SHIFT) != 0 && (origKeyState & MK_CONTROL) == 0 &&
                    (origEffect & DROPEFFECT_MOVE) != 0)
                {
                    *pdwEffect = DROPEFFECT_MOVE;
                }
                else
                {
                    if ((origEffect & DROPEFFECT_COPY) != 0)
                        *pdwEffect = DROPEFFECT_COPY;
                    else
                    {
                        if ((origEffect & DROPEFFECT_MOVE) != 0)
                            *pdwEffect = DROPEFFECT_MOVE;
                        else // chyba drop-targetu
                        {
                            *pdwEffect = DROPEFFECT_NONE;
                            pdwEffect = NULL;
                            CurDirDropTarget->DragLeave();
                            CurDirDropTarget->Release(); // zrusime drop-target, aby se mu nezavolal drag-over
                            CurDirDropTarget = NULL;
                        }
                    }
                }
            }
            LastEffect = (pdwEffect != NULL) ? *pdwEffect : -1;
        }
    }
    else
    {
        if (TgtType == idtttArchive || TgtType == idtttPluginFS ||
            TgtType == idtttArchiveOnWinPath || TgtType == idtttFullPluginFSPath)
        {
            DWORD allowedEffects = *pdwEffect;
            if ((origKeyState & MK_SHIFT) != 0 && (origKeyState & MK_CONTROL) == 0 &&
                (*pdwEffect & DROPEFFECT_MOVE) != 0) // user chce Move
            {
                *pdwEffect = DROPEFFECT_MOVE;
            }
            else
            {
                if ((origKeyState & MK_SHIFT) == 0 && (origKeyState & MK_CONTROL) != 0 &&
                    (*pdwEffect & DROPEFFECT_COPY) != 0) // user chce Copy
                {
                    *pdwEffect = DROPEFFECT_COPY;
                }
            }
            // zjistime defaultni drop effect
            if (TgtType == idtttFullPluginFSPath && OldDataObjectSrcType == 2 /* FS */ &&
                OldDataObjectSrcFSPath[0] != 0 && GetFSToFSDropEffect != NULL)
            { // FS na FS: zjistime od pluginu jaky effect preferuje
                GetFSToFSDropEffect(OldDataObjectSrcFSPath, CurDir, allowedEffects, origKeyState,
                                    pdwEffect, GetFSToFSDropEffectParam);
                DragFromPluginFSEffectIsFromPlugin = TRUE;
            }
            else // z disku do archivu + z disku na FS: Copy ma prioritu
            {
                if ((*pdwEffect & DROPEFFECT_COPY) != 0)
                    *pdwEffect = DROPEFFECT_COPY;
                else
                {
                    if ((*pdwEffect & DROPEFFECT_MOVE) != 0)
                        *pdwEffect = DROPEFFECT_MOVE;
                    else
                        *pdwEffect = DROPEFFECT_NONE; // chyba drop-targetu
                }
            }
            if (*pdwEffect == DROPEFFECT_NONE)
                pdwEffect = NULL; // chyba drop-targetu
            LastEffect = (pdwEffect != NULL) ? *pdwEffect : -1;
        }
        else
        {
            *pdwEffect = DROPEFFECT_NONE;
            LastEffect = -1;
        }
    }
    return S_OK;
}

STDMETHODIMP CImpDropTarget::DragOver(DWORD grfKeyState, POINTL pt,
                                      DWORD* pdwEffect)
{
    CALL_STACK_MESSAGE2("CImpDropTarget::DragOver(0x%X, ,)", grfKeyState);

    DWORD origEffect = *pdwEffect;
    DWORD origKeyState = grfKeyState;
    RButton = (grfKeyState & MK_RBUTTON) && !(grfKeyState & MK_LBUTTON);

    if (ImageDragging)
        ImageDragMove(pt.x, pt.y);

    if (GetCurDir != NULL)
    {
        BOOL tgtFile;
        int tgtType;
        const char* tgtPath = GetCurDir(pt, GetCurDirParam, pdwEffect, RButton, tgtFile,
                                        grfKeyState, tgtType, OldDataObjectSrcType);
        SetDirectory(tgtPath, grfKeyState, pt, pdwEffect, OldDataObject, tgtFile, tgtType);
        if (TgtType != idtttWindows && TgtType != idtttFullPluginFSPath)
        { // neni-li oznaceni z jedne cesty (hrozi asi jen u Findu), neumime copy/move do archivu ani FS
            if (OldDataObjectIsSimple == -1)
                OldDataObjectIsSimple = IsSimpleSelection(OldDataObject, NULL);
            if (!OldDataObjectIsSimple)
                SetDirectory(NULL, grfKeyState, pt, pdwEffect, OldDataObject, FALSE, idtttWindows);
        }
    }
    if (CurDirDropTarget != NULL) // jen idtttWindows
    {
        HRESULT res = CurDirDropTarget->DragOver(grfKeyState, pt, pdwEffect);
        if (res == S_OK && OldDataObjectIsFake)
        { // nas data-object (nemusi byt z tohoto procesu): defaultni je Copy (fake je v TEMPU, na stejnem disku to delalo defaultne Move, tak to takhle obejdeme)
            if ((origKeyState & MK_SHIFT) != 0 && (origKeyState & MK_CONTROL) == 0 &&
                (origEffect & DROPEFFECT_MOVE) != 0)
            {
                *pdwEffect = DROPEFFECT_MOVE;
            }
            else
            {
                if ((origEffect & DROPEFFECT_COPY) != 0)
                    *pdwEffect = DROPEFFECT_COPY;
                else
                {
                    if ((origEffect & DROPEFFECT_MOVE) != 0)
                        *pdwEffect = DROPEFFECT_MOVE;
                    else // chyba drop-targetu
                    {
                        *pdwEffect = DROPEFFECT_NONE;
                        pdwEffect = NULL;
                        CurDirDropTarget->DragLeave();
                        CurDirDropTarget->Release(); // zrusime drop-target, aby se mu nezavolal drag-over
                        CurDirDropTarget = NULL;
                    }
                }
            }
        }
        LastEffect = (pdwEffect != NULL) ? *pdwEffect : -1;
        return res;
    }
    else
    {
        if (TgtType == idtttArchive || TgtType == idtttPluginFS ||
            TgtType == idtttArchiveOnWinPath || TgtType == idtttFullPluginFSPath)
        {
            DWORD allowedEffects = *pdwEffect;
            if ((origKeyState & MK_SHIFT) != 0 && (origKeyState & MK_CONTROL) == 0 &&
                (*pdwEffect & DROPEFFECT_MOVE) != 0) // user chce Move
            {
                *pdwEffect = DROPEFFECT_MOVE;
            }
            else
            {
                if ((origKeyState & MK_SHIFT) == 0 && (origKeyState & MK_CONTROL) != 0 &&
                    (*pdwEffect & DROPEFFECT_COPY) != 0) // user chce Copy
                {
                    *pdwEffect = DROPEFFECT_COPY;
                }
            }
            // zjistime defaultni drop effect
            if (TgtType == idtttFullPluginFSPath && OldDataObjectSrcType == 2 /* FS */ &&
                OldDataObjectSrcFSPath[0] != 0 && GetFSToFSDropEffect != NULL)
            { // FS na FS: zjistime od pluginu jaky effect preferuje
                GetFSToFSDropEffect(OldDataObjectSrcFSPath, CurDir, allowedEffects,
                                    origKeyState, pdwEffect, GetFSToFSDropEffectParam);
                DragFromPluginFSEffectIsFromPlugin = TRUE;
            }
            else // z disku do archivu + z disku na FS: Copy ma prioritu
            {
                if ((*pdwEffect & DROPEFFECT_COPY) != 0)
                    *pdwEffect = DROPEFFECT_COPY;
                else
                {
                    if ((*pdwEffect & DROPEFFECT_MOVE) != 0)
                        *pdwEffect = DROPEFFECT_MOVE;
                    else
                        *pdwEffect = DROPEFFECT_NONE; // chyba drop-targetu
                }
            }
            if (*pdwEffect == DROPEFFECT_NONE)
                pdwEffect = NULL; // chyba drop-targetu
            LastEffect = (pdwEffect != NULL) ? *pdwEffect : -1;
        }
        else
        {
            *pdwEffect = DROPEFFECT_NONE;
            LastEffect = -1;
        }
        return S_OK;
    }
}

STDMETHODIMP CImpDropTarget::DragLeave()
{
    CALL_STACK_MESSAGE1("CImpDropTarget::DragLeave()");

    if (ImageDragging)
        ImageDragLeave();
    if (EnterLeaveDrop != NULL)
        EnterLeaveDrop(FALSE, EnterLeaveDropParam);

    RButton = FALSE;
    if (OldDataObject != NULL)
    {
        OldDataObject->Release();
        OldDataObject = NULL;
        OldDataObjectIsFake = FALSE;
        OldDataObjectIsSimple = -1; // neznama hodnota
        OldDataObjectSrcType = 0;
        OldDataObjectSrcFSPath[0] = 0;
    }

    HRESULT ret = S_OK;
    if (CurDirDropTarget != NULL)
    {
        ret = CurDirDropTarget->DragLeave();
        CurDirDropTarget->Release();
        CurDirDropTarget = NULL;
    }
    if (DropEnd != NULL)
        DropEnd(FALSE, FALSE, DropEndParam, FALSE, FALSE, TgtType);
    TgtType = idtttWindows;
    LastEffect = -1;
    return ret;
}

STDMETHODIMP CImpDropTarget::Drop(IDataObject* pDataObject, DWORD grfKeyState,
                                  POINTL pt, DWORD* pdwEffect)
{
    CALL_STACK_MESSAGE2("CImpDropTarget::Drop(, 0x%X, ,)", grfKeyState);

    DWORD lastEffect = LastEffect;
    LastEffect = -1; // zjednodusene zneplatneni (nemusi byt pred kazdym returnem)

    if (pdwEffect == NULL)
    {
        DragLeave();
        return E_INVALIDARG;
    }

    DWORD origEffect = *pdwEffect;
    DWORD origKeyState = grfKeyState;

    if (ImageDragging)
        ImageDragLeave();
    if (EnterLeaveDrop != NULL)
        EnterLeaveDrop(FALSE, EnterLeaveDropParam);

    DWORD defEffect = -1;
    if (RButton || ConfirmDropEnable != NULL && *ConfirmDropEnable)
    {
        if (GetCurDir != NULL) // musime nechat omezit pdwEffect pri tazeni v ramci panelu
        {
            BOOL tgtFile;
            int tgtType;
            const char* tgtPath = GetCurDir(pt, GetCurDirParam, pdwEffect, RButton, tgtFile,
                                            grfKeyState, tgtType, OldDataObjectSrcType);
            SetDirectory(tgtPath, grfKeyState, pt, pdwEffect, OldDataObject, tgtFile, tgtType);
            if (TgtType != idtttWindows && TgtType != idtttFullPluginFSPath)
            { // neni-li oznaceni z jedne cesty (hrozi asi jen u Findu), neumime copy/move do archivu ani FS
                if (OldDataObjectIsSimple == -1)
                    OldDataObjectIsSimple = IsSimpleSelection(OldDataObject, NULL);
                if (!OldDataObjectIsSimple)
                {
                    SetDirectory(NULL, grfKeyState, pt, pdwEffect, OldDataObject, FALSE, idtttWindows);
                    *pdwEffect = DROPEFFECT_NONE;
                    return DragLeave();
                }
            }
        }
        defEffect = *pdwEffect;

        if (TgtType == idtttWindows)
        {
            if (OldDataObjectIsFake)
            {
                if ((origKeyState & MK_SHIFT) != 0 && (origKeyState & MK_CONTROL) == 0 &&
                    (origEffect & DROPEFFECT_MOVE) != 0)
                {
                    defEffect = DROPEFFECT_MOVE;
                }
                else
                {
                    if ((origEffect & DROPEFFECT_COPY) != 0)
                        defEffect = DROPEFFECT_COPY;
                    else
                    {
                        if ((origEffect & DROPEFFECT_MOVE) != 0)
                            defEffect = DROPEFFECT_MOVE;
                        else
                            defEffect = 0; // chyba drop-targetu
                    }
                }
            }
            else
            {
                if (CurDirDropTarget != NULL) // zjisteni default drop effectu
                {
                    CurDirDropTarget->DragOver(grfKeyState, pt, &defEffect);
                }
                else
                    defEffect = 0;
            }
        }
        else
        {
            if (TgtType == idtttArchive || TgtType == idtttPluginFS ||
                TgtType == idtttArchiveOnWinPath || TgtType == idtttFullPluginFSPath)
            {
                defEffect = *pdwEffect;
                if ((origKeyState & MK_SHIFT) != 0 && (origKeyState & MK_CONTROL) == 0 &&
                    (defEffect & DROPEFFECT_MOVE) != 0) // user chce Move
                {
                    defEffect = DROPEFFECT_MOVE;
                }
                else
                {
                    if ((origKeyState & MK_SHIFT) == 0 && (origKeyState & MK_CONTROL) != 0 &&
                        (defEffect & DROPEFFECT_COPY) != 0) // user chce Copy
                    {
                        defEffect = DROPEFFECT_COPY;
                    }
                }
                // zjistime defaultni drop effect
                if (TgtType == idtttFullPluginFSPath && OldDataObjectSrcType == 2 /* FS */ &&
                    OldDataObjectSrcFSPath[0] != 0 && GetFSToFSDropEffect != NULL)
                { // FS na FS: zjistime od pluginu jaky effect preferuje
                    GetFSToFSDropEffect(OldDataObjectSrcFSPath, CurDir, *pdwEffect,
                                        origKeyState, &defEffect, GetFSToFSDropEffectParam);
                    if (defEffect == DROPEFFECT_NONE)
                        defEffect = 0; // chyba drop-targetu
                    DragFromPluginFSEffectIsFromPlugin = TRUE;
                }
                else // z disku do archivu + z disku na FS: Copy ma prioritu
                {
                    if ((defEffect & DROPEFFECT_COPY) != 0)
                        defEffect = DROPEFFECT_COPY;
                    else
                    {
                        if ((defEffect & DROPEFFECT_MOVE) != 0)
                            defEffect = DROPEFFECT_MOVE;
                        else
                            defEffect = DROPEFFECT_NONE; // nemelo by nastat (resi se pres: TgtType==idtttWindows + CurDirDropTarget==NULL)
                    }
                }
            }
            else
                defEffect = 0;
        }

        if (ConfirmDrop != NULL && !ConfirmDrop(*pdwEffect, defEffect, grfKeyState))
        {
            *pdwEffect = DROPEFFECT_NONE;
            return DragLeave();
        }
        *pdwEffect = defEffect;
        origEffect = *pdwEffect;

        if (CurDirDropTarget != NULL) // info o zmene klaves (shift+control u other...), od W2K zrejme zbytecne
        {
            CurDirDropTarget->DragOver(grfKeyState, pt, &defEffect);
            defEffect = *pdwEffect;
        }
    }

    if (OldDataObject != NULL)
    {
        OldDataObject->Release();
        OldDataObject = NULL;
        OldDataObjectIsFake = FALSE;
        OldDataObjectIsSimple = -1; // neznama hodnota
        OldDataObjectSrcType = 0;
        OldDataObjectSrcFSPath[0] = 0;
    }

    int dataObjectSrcType;
    char dataObjectSrcFSPath[2 * MAX_PATH];
    BOOL isFake = IsFakeDataObject(pDataObject, &dataObjectSrcType, dataObjectSrcFSPath, 2 * MAX_PATH);
    BOOL tgtFile = TRUE; // je cilem operace soubor?
    CDragDropOperData* namesList = new CDragDropOperData;
    if (GetCurDir != NULL)
    {
        int tgtType;
        const char* tgtPath = GetCurDir(pt, GetCurDirParam, pdwEffect, RButton, tgtFile,
                                        grfKeyState, tgtType, dataObjectSrcType);
        SetDirectory(tgtPath, grfKeyState, pt, pdwEffect, pDataObject, tgtFile, tgtType);
        if (TgtType != idtttWindows && TgtType != idtttFullPluginFSPath &&
            !IsSimpleSelection(pDataObject, namesList))
        { // neni-li oznaceni z jedne cesty (hrozi asi jen u Findu), neumime copy/move do archivu ani FS
            SetDirectory(NULL, grfKeyState, pt, pdwEffect, pDataObject, FALSE, idtttWindows);
            if (DropEnd != NULL)
                DropEnd(FALSE, FALSE, DropEndParam, FALSE, FALSE, TgtType);
            if (namesList != NULL)
                delete namesList;
            return S_OK;
        }
    }

    BOOL operationDone = FALSE;
    HRESULT ret = E_UNEXPECTED;
    if (TgtType == idtttWindows)
    {
        // zjistime defEffect
        BOOL ownRutine = !tgtFile && !isFake && (UseOwnRutine == NULL || UseOwnRutine(pDataObject));
        if (ownRutine && defEffect == -1)
        {
            if (lastEffect != -1)
                defEffect = lastEffect;
            else
            {
                defEffect = *pdwEffect;
                if (CurDirDropTarget != NULL) // zjisteni default drop effectu
                {
                    CurDirDropTarget->DragOver(grfKeyState, pt, &defEffect);
                }
                else
                    defEffect = 0;
            }
        }

        // pokusime si s tim poradit sami
        defEffect &= DROPEFFECT_COPY | DROPEFFECT_MOVE;
        if (ownRutine &&
            (defEffect == DROPEFFECT_COPY || defEffect == DROPEFFECT_MOVE) &&
            pDataObject != NULL && DoCopyMove != NULL)
        { // nebudem schopny provest operaci sami ?
            IEnumFORMATETC* enumFormat;
            if (pDataObject->EnumFormatEtc(DATADIR_GET, &enumFormat) == S_OK)
            {
                BOOL cfHDrop = FALSE;
                BOOL cfFileMapA = FALSE;
                BOOL cfFileMapW = FALSE;
                UINT CF_FileMapA = RegisterClipboardFormat(CFSTR_FILENAMEMAPA);
                UINT CF_FileMapW = RegisterClipboardFormat(CFSTR_FILENAMEMAPW);

                FORMATETC formatEtc;
                enumFormat->Reset();
                while (enumFormat->Next(1, &formatEtc, NULL) == S_OK)
                {
                    if (formatEtc.cfFormat == CF_FileMapA)
                        cfFileMapA = TRUE;
                    if (formatEtc.cfFormat == CF_FileMapW)
                        cfFileMapW = TRUE;
                    if (formatEtc.cfFormat == CF_HDROP)
                        cfHDrop = TRUE;
                }
                enumFormat->Release();

                if (cfHDrop &&
                    TryCopyOrMove(defEffect == DROPEFFECT_COPY, pDataObject, CF_FileMapA,
                                  CF_FileMapW, cfFileMapA, cfFileMapW))
                {
                    if (CurDirDropTarget != NULL)
                    {
                        CurDirDropTarget->DragLeave();
                        CurDirDropTarget->Release();
                        CurDirDropTarget = NULL;
                        operationDone = TRUE;
                        ret = S_OK;
                    }
                }
            }
        }

        // pokud jde o "fake" adresar (unpack z archivu, copy/move z FS), zpracujeme ho zde
        if (!operationDone && isFake && CurDir[0] != 0)
        {
            // zjisteni default drop effectu - nas data-object (nemusi byt z tohoto procesu): defaultni je
            // Copy (fake je v TEMPU, na stejnem disku to delalo defaultne Move, tak to takhle obejdeme)
            if ((origKeyState & MK_SHIFT) != 0 && (origKeyState & MK_CONTROL) == 0 &&
                (origEffect & DROPEFFECT_MOVE) != 0)
            {
                *pdwEffect = DROPEFFECT_MOVE;
            }
            else
            {
                if ((origEffect & DROPEFFECT_COPY) != 0)
                    *pdwEffect = DROPEFFECT_COPY;
                else
                {
                    if ((origEffect & DROPEFFECT_MOVE) != 0)
                        *pdwEffect = DROPEFFECT_MOVE;
                    else
                        *pdwEffect = DROPEFFECT_NONE; // chyba drop-targetu
                }
            }

            if (*pdwEffect == DROPEFFECT_COPY || *pdwEffect == DROPEFFECT_MOVE)
            {
                BOOL success = FALSE;
                if (SalShExtSharedMemView != NULL)
                {
                    WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                    if (SalShExtSharedMemView->DoDragDropFromSalamander)
                    {
                        SalShExtSharedMemView->DropDone = TRUE;
                        SalShExtSharedMemView->PasteDone = FALSE;
                        lstrcpyn(SalShExtSharedMemView->TargetPath, CurDir, MAX_PATH); // jen diskova cesta, staci MAX_PATH
                        SalShExtSharedMemView->Operation = *pdwEffect == DROPEFFECT_COPY ? SALSHEXT_COPY : SALSHEXT_MOVE;
                        success = TRUE;
                    }
                    ReleaseMutex(SalShExtSharedMemMutex);
                }
                if (success && CurDirDropTarget != NULL)
                {
                    CurDirDropTarget->DragLeave();
                    CurDirDropTarget->Release();
                    CurDirDropTarget = NULL;
                    operationDone = TRUE;
                    ret = S_OK;
                }
            }
        }

        // sami si s tim neporadime, nechame to udelat system
        if (!operationDone && CurDirDropTarget != NULL)
        {
            ret = CurDirDropTarget->Drop(pDataObject, grfKeyState, pt, pdwEffect);
            CurDirDropTarget->Release();
            CurDirDropTarget = NULL;
        }
    }
    else // archivy a FS
    {
        if (TgtType == idtttArchive || TgtType == idtttPluginFS ||
            TgtType == idtttArchiveOnWinPath || TgtType == idtttFullPluginFSPath)
        {
            DWORD allowedEffects = *pdwEffect;
            if ((origKeyState & MK_SHIFT) != 0 && (origKeyState & MK_CONTROL) == 0 &&
                (*pdwEffect & DROPEFFECT_MOVE) != 0) // user chce Move
            {
                *pdwEffect = DROPEFFECT_MOVE;
            }
            else
            {
                if ((origKeyState & MK_SHIFT) == 0 && (origKeyState & MK_CONTROL) != 0 &&
                    (*pdwEffect & DROPEFFECT_COPY) != 0) // user chce Copy
                {
                    *pdwEffect = DROPEFFECT_COPY;
                }
            }
            // zjistime defaultni drop effect
            if (TgtType == idtttFullPluginFSPath && dataObjectSrcType == 2 /* FS */ &&
                dataObjectSrcFSPath[0] != 0 && GetFSToFSDropEffect != NULL)
            { // FS na FS: zjistime od pluginu jaky effect preferuje
                GetFSToFSDropEffect(dataObjectSrcFSPath, CurDir, allowedEffects,
                                    origKeyState, pdwEffect, GetFSToFSDropEffectParam);
                DragFromPluginFSEffectIsFromPlugin = TRUE;
            }
            else // z disku do archivu + z disku na FS: Copy ma prioritu
            {
                if ((*pdwEffect & DROPEFFECT_COPY) != 0)
                    *pdwEffect = DROPEFFECT_COPY;
                else
                {
                    if ((*pdwEffect & DROPEFFECT_MOVE) != 0)
                        *pdwEffect = DROPEFFECT_MOVE;
                    else
                        *pdwEffect = DROPEFFECT_NONE; // nemelo by nastat (resi se pres: TgtType==idtttWindows + CurDirDropTarget==NULL)
                }
            }

            if (*pdwEffect != DROPEFFECT_NONE)
            {
                if (TgtType == idtttFullPluginFSPath) // drag&drop z FS na FS
                {
                    if (isFake && dataObjectSrcType == 2 /* FS */ && CurDir[0] != 0 &&      // "always true"
                        (*pdwEffect == DROPEFFECT_COPY || *pdwEffect == DROPEFFECT_MOVE) && // "always true"
                        SalShExtSharedMemView != NULL)
                    {
                        WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                        if (SalShExtSharedMemView->DoDragDropFromSalamander)
                        {
                            SalShExtSharedMemView->DropDone = TRUE;
                            SalShExtSharedMemView->PasteDone = FALSE;
                            lstrcpyn(SalShExtSharedMemView->TargetPath, CurDir, 2 * MAX_PATH); // full FS cesta, potreba 2 * MAX_PATH
                            SalShExtSharedMemView->Operation = *pdwEffect == DROPEFFECT_COPY ? SALSHEXT_COPY : SALSHEXT_MOVE;
                        }
                        ReleaseMutex(SalShExtSharedMemMutex);
                    }
                }
                else // TgtType: idtttArchive, idtttArchiveOnWinPath, idtttPluginFS
                {
                    if (DoDragDropOper != NULL && (*pdwEffect == DROPEFFECT_COPY || *pdwEffect == DROPEFFECT_MOVE) &&
                        namesList != NULL)
                    {
                        DoDragDropOper(*pdwEffect == DROPEFFECT_COPY, TgtType == idtttArchive || TgtType == idtttArchiveOnWinPath,
                                       TgtType == idtttArchiveOnWinPath ? CurDir : NULL,
                                       TgtType == idtttArchiveOnWinPath ? "" : CurDir, namesList, DoDragDropOperParam);
                        namesList = NULL; // DoDragDropOper ho necha dealokovat, tady uz to delat nebudeme
                    }
                }
                ret = S_OK;
            }
        }
    }

    if (DropEnd != NULL) // parametry 'operationDone' a 'isFake' se v DropEnd ignoruji pro TgtType != idtttWindows
        DropEnd(TRUE, (*pdwEffect == DROPEFFECT_LINK), DropEndParam, operationDone, isFake, TgtType);
    TgtType = idtttWindows;
    if (namesList != NULL)
        delete namesList;
    return ret;
}

//*****************************************************************************
//
// CImpIDropSource
//

STDMETHODIMP CImpIDropSource::QueryInterface(REFIID refiid, void FAR* FAR* ppv)
{
    if (refiid == IID_IUnknown || refiid == IID_IDropSource)
    {
        *ppv = this;
        AddRef();
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

//*****************************************************************************
//
// InitializeShellib
//

BOOL InitializeShellib()
{
    CALL_STACK_MESSAGE1("InitializeShellib()");

    // OLE nyni inicializujeme primo ve WinMainBody, protoze na nej pripojujeme SPY
    //  if (OleInitialize(NULL) != S_OK) // CoInitialize uz nestaci, nefunguji pak napriklad registrace oken pro drag&drop
    //  {
    //    TRACE_E("Error in OleInitialize.");
    //    return FALSE;
    //  }
    if (ExecuteAssociationTlsIndex == TLS_OUT_OF_INDEXES)
        ExecuteAssociationTlsIndex = HANDLES(TlsAlloc());
    return TRUE;
}

//*****************************************************************************
//
// ReleaseShellib
//

void ReleaseShellib()
{
    __try
    {
        if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES)
        {
            HANDLES(TlsFree(ExecuteAssociationTlsIndex));
            ExecuteAssociationTlsIndex = TLS_OUT_OF_INDEXES;
        }
        OleFlushClipboard(); // predame systemu data z IDataObjectu, ktery jsme na clipboardu nechali lezet (tento IDataObject se tim uvolni)
        // OLE nyni deinicializujeme primo ve WinMainBody (SPY)
        //    OleUninitialize();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        OCUExceptionHasOccured++;
    }
}

//*****************************************************************************
//
// GetItemIdListForFileName
//

LPITEMIDLIST GetItemIdListForFileName(LPSHELLFOLDER folder, const char* fileName,
                                      BOOL addUNCPrefix = FALSE, BOOL useEnumForPIDLs = FALSE,
                                      const char* enumNamePrefix = NULL)
{
    CALL_STACK_MESSAGE4("GetItemIdListForFileName(, %s, %d, %d,)", fileName, addUNCPrefix, useEnumForPIDLs);

    // pokud hledame jmeno, ktere konci na mezeru/tecku, nezbyva nez hledat pomalu
    // s pouzitim enumerace celeho folderu
    if (!useEnumForPIDLs && enumNamePrefix != NULL && !addUNCPrefix)
    {
        int len = (int)strlen(fileName);
        if (len > 0 && (fileName[len - 1] <= ' ' || fileName[len - 1] == '.'))
            useEnumForPIDLs = TRUE;
    }
    if (useEnumForPIDLs) // pomalejsi varianta, bohuzel pro ziskani PIDLu sharu na serveru je nutna
    {
        LPITEMIDLIST foundPidl = NULL;
        LPENUMIDLIST enumIDList;
        if (SUCCEEDED(folder->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN,
                                          &enumIDList)))
        {
            ULONG celt;
            LPITEMIDLIST idList;
            STRRET str;
            enumIDList->Reset();
            IMalloc* alloc;
            if (SUCCEEDED(CoGetMalloc(1, &alloc)))
            {
                int enumNamePrefixLen = enumNamePrefix == NULL ? 0 : (int)strlen(enumNamePrefix);
                if (enumNamePrefixLen > 0 && enumNamePrefix[enumNamePrefixLen - 1] == '\\')
                    enumNamePrefixLen--;
                while (1)
                {
                    if (enumIDList->Next(1, &idList, &celt) == NOERROR)
                    {
                        if (folder->GetDisplayNameOf(idList, SHGDN_FORPARSING, &str) == NOERROR)
                        {
                            char buf[MAX_PATH];
                            char* name;
                            switch (str.uType)
                            {
                            case STRRET_CSTR:
                                name = str.cStr;
                                break;
                            case STRRET_OFFSET:
                                name = (char*)idList + str.uOffset;
                                break;
                            case STRRET_WSTR:
                            {
                                WideCharToMultiByte(CP_ACP, 0, str.pOleStr, -1, buf, MAX_PATH, NULL, NULL);
                                buf[MAX_PATH - 1] = 0;
                                name = buf;
                                if (alloc->DidAlloc(str.pOleStr) == 1)
                                    alloc->Free(str.pOleStr);
                                break;
                            }
                            default:
                                name = NULL;
                            }

                            if (name != NULL)
                            {
                                if (*(name + strlen(name) - 1) == '\\')
                                    *(name + strlen(name) - 1) = 0;
                                if (enumNamePrefix != NULL && StrNICmp(name, enumNamePrefix, enumNamePrefixLen) == 0 &&
                                        name[enumNamePrefixLen] == '\\' && StrICmp(name + enumNamePrefixLen + 1, fileName) == 0 ||
                                    enumNamePrefix == NULL && StrICmp(name, fileName) == 0) // mame share, ktery hledame
                                {
                                    foundPidl = idList;
                                    break; // pidl nalezen (ziskan)
                                }
                            }
                        }
                        if (alloc->DidAlloc(idList) == 1)
                            alloc->Free(idList);
                    }
                    else
                        break;
                }
                alloc->Release();
            }
            enumIDList->Release();
        }
        if (foundPidl != NULL)
            return foundPidl;
        else
            TRACE_E("GetItemIdListForFileName(): unable to find PIDL usign enumeration, trying to get it using ParseDisplayName...");
    }

    OLECHAR olePath[MAX_PATH];
    if (addUNCPrefix)
        olePath[0] = olePath[1] = L'\\';
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fileName, -1, olePath + (addUNCPrefix ? 2 : 0),
                        MAX_PATH - (addUNCPrefix ? 2 : 0));
    olePath[MAX_PATH - 1] = 0;

    LPITEMIDLIST pidl;
    ULONG chEaten;
    HRESULT ret;
    if (SUCCEEDED((ret = folder->ParseDisplayName(NULL, NULL, olePath, &chEaten,
                                                  &pidl, NULL))))
    {
        return pidl;
    }
    else
    {
        TRACE_E("ParseDisplayName error: 0x" << std::hex << ret << std::dec);
        return NULL;
    }
}

//*****************************************************************************
//
// DestroyItemIdList
//

void DestroyItemIdList(ITEMIDLIST** list, int itemsInList)
{
    CALL_STACK_MESSAGE2("DestroyItemIdList(, %d)", itemsInList);
    IMalloc* alloc;
    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
    {
        int i;
        for (i = 0; i < itemsInList; i++)
        {
            if (list[i] != NULL && alloc->DidAlloc(list[i]) == 1)
            {
                alloc->Free(list[i]);
            }
        }
        alloc->Free(list);
        alloc->Release();
    }
}

//*****************************************************************************
//
// CreateItemIdList
//

ITEMIDLIST** CreateItemIdList(LPSHELLFOLDER folder, int files,
                              CEnumFileNamesFunction nextFile, void* param,
                              UINT& itemsInList, BOOL addUNCPrefix = FALSE,
                              BOOL useEnumForPIDLs = FALSE, const char* enumNamePrefix = NULL,
                              BOOL namesMustBeValid = FALSE)
{
    CALL_STACK_MESSAGE5("CreateItemIdList(, %d, , , , %d, %d, , %d)",
                        files, addUNCPrefix, useEnumForPIDLs, namesMustBeValid);
    if (files <= 0)
        return NULL;

    ITEMIDLIST** list = NULL;
    IMalloc* alloc;
    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
    {
        list = (ITEMIDLIST**)alloc->Alloc(sizeof(ITEMIDLIST*) * files);
        alloc->Release();
    }
    if (list == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return NULL;
    }
    memset(list, 0, sizeof(ITEMIDLIST*) * files);

    ITEMIDLIST* pidl = NULL;
    int i;
    for (i = 0; i < files; i++)
    {
        const char* fileName = nextFile(i, param);
        // napr. pro ziskani funkcniho data-objectu je nutne, aby obsazena jmena byla validni,
        // drag&drop invalidniho jmena znamena operaci nad jmenem s tise orezanymi mezerami/teckami
        // na konci (misto "a   " vezme "a"), to rozhodne nechceme
        if (namesMustBeValid && FileNameIsInvalid(fileName, FALSE))
        {
            TRACE_I("CreateItemIdList: unable to create IdList becuase of invalid name: \"" << fileName << "\"");
            pidl = NULL;
            break;
        }
        pidl = (ITEMIDLIST*)GetItemIdListForFileName(folder, fileName, addUNCPrefix, useEnumForPIDLs, enumNamePrefix);
        if (pidl != NULL)
            list[i] = pidl;
        else
            break; // nejaka chyba
    }

    if (pidl == NULL)
    {
        DestroyItemIdList(list, files);
        itemsInList = 0;
        return NULL;
    }
    else
    {
        itemsInList = files;
        return list;
    }
}

//*****************************************************************************
//
// GetShellFolder
//

BOOL GetShellFolder(const char* dir, IShellFolder*& shellFolderObj, LPITEMIDLIST& pidlFolder)
{
    CALL_STACK_MESSAGE2("GetShellFolder(%s, ,)", dir);
    shellFolderObj = NULL;
    pidlFolder = NULL;
    HRESULT ret;
    LPSHELLFOLDER desktop;
    // pokud cesta obsahuje komponenty koncici na mezery/tecky, shell nam nevrati
    // folder pro pozadovanou cestu, ale pro cestu vzniklou oriznutim techto
    // mezer/tecek, takze radsi se na to vcas vykasleme...
    if (PathContainsValidComponents((char*)dir, FALSE))
    {
        if (SUCCEEDED((ret = SHGetDesktopFolder(&desktop))))
        {
            int rootFolder;
            if (dir[0] != '\\')
                rootFolder = CSIDL_DRIVES; // normalni cesta
            else
                rootFolder = CSIDL_NETWORK; // UNC - sitove zdroje
            LPITEMIDLIST rootFolderID;
            if (SUCCEEDED((ret = SHGetSpecialFolderLocation(NULL, rootFolder, &rootFolderID))))
            {
                if (SUCCEEDED((ret = desktop->BindToObject(rootFolderID, NULL,
                                                           IID_IShellFolder,
                                                           (LPVOID*)&shellFolderObj))))
                {
                    char root[MAX_PATH];
                    GetRootPath(root, dir);
                    if (strlen(root) < strlen(dir)) // neni to root cesta
                    {
                        strcpy(root, dir);
                        char* name = root + strlen(root);
                        if (*--name == '\\')
                            *name = 0;
                        else
                            name++;
                        while (*--name != '\\')
                            ;
                        char c = *++name;
                        *name = 0;
                        LPITEMIDLIST pidlUpperDir = GetItemIdListForFileName(shellFolderObj, root);
                        LPSHELLFOLDER folder2;
                        if (pidlUpperDir != NULL &&
                            SUCCEEDED((ret = shellFolderObj->BindToObject(pidlUpperDir, NULL,
                                                                          IID_IShellFolder, (LPVOID*)&folder2))))
                        {
                            shellFolderObj->Release();
                            shellFolderObj = folder2;
                            *name = c;
                            dir = name;
                        }
                        else
                            TRACE_E("BindToObject error: 0x" << std::hex << ret << std::dec); // dir zustava
                        IMalloc* alloc;
                        if (pidlUpperDir != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
                        {
                            if (alloc->DidAlloc(pidlUpperDir) == 1)
                                alloc->Free(pidlUpperDir);
                            alloc->Release();
                        }
                    }
                    else
                    {
                        if (rootFolder == CSIDL_DRIVES)
                        {
                            LPENUMIDLIST enumIDList;
                            if (SUCCEEDED((ret = shellFolderObj->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_INCLUDEHIDDEN,
                                                                             &enumIDList))))
                            {
                                ULONG celt;
                                LPITEMIDLIST idList;
                                STRRET str;
                                enumIDList->Reset();
                                IMalloc* alloc;
                                if (SUCCEEDED(CoGetMalloc(1, &alloc)))
                                {
                                    while (1)
                                    {
                                        ret = enumIDList->Next(1, &idList, &celt);
                                        if (ret == NOERROR)
                                        {
                                            ret = shellFolderObj->GetDisplayNameOf(idList, SHGDN_FORPARSING, &str);
                                            if (ret == NOERROR)
                                            {
                                                char buf[MAX_PATH];
                                                char* name;
                                                switch (str.uType)
                                                {
                                                case STRRET_CSTR:
                                                    name = str.cStr;
                                                    break;
                                                case STRRET_OFFSET:
                                                    name = (char*)idList + str.uOffset;
                                                    break;
                                                case STRRET_WSTR:
                                                {
                                                    WideCharToMultiByte(CP_ACP, 0, str.pOleStr, -1, buf, MAX_PATH, NULL, NULL);
                                                    buf[MAX_PATH - 1] = 0;
                                                    name = buf;
                                                    if (alloc->DidAlloc(str.pOleStr) == 1)
                                                        alloc->Free(str.pOleStr);
                                                    break;
                                                }
                                                default:
                                                    name = NULL;
                                                }

                                                if (name != NULL)
                                                {
                                                    if (strlen(name) <= 3 && StrNICmp(name, root, 2) == 0) // name = "c:" nebo "c:\"
                                                    {
                                                        pidlFolder = idList;
                                                        break; // pidl nalezen (ziskan)
                                                    }
                                                }
                                            }
                                            if (alloc->DidAlloc(idList) == 1)
                                                alloc->Free(idList);
                                        }
                                        else
                                            break;
                                    }
                                    alloc->Release();
                                }
                                enumIDList->Release();
                            }
                        }
                        else
                        {
                            if (rootFolder == CSIDL_NETWORK) // musime ziskat slozite pidl, jinak nechodi mapovani
                            {
                                *(root + strlen(root) - 1) = 0;
                                dir = root;
                                char* s = root + 2;
                                if (*s == 0) // sitova cesta "\\\\" (root site)
                                {
                                    shellFolderObj->Release();
                                    shellFolderObj = desktop;
                                    desktop = NULL;
                                    pidlFolder = rootFolderID;
                                    rootFolderID = NULL;
                                }
                                else
                                {
                                    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // ceka uz ?
                                    HCURSOR oldCur;
                                    if (setWait)
                                        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                                    while (*s != 0 && *s != '\\')
                                        s++;
                                    BOOL dirIsOnlyServer = *s == 0;
                                    *s = 0;
                                    LPITEMIDLIST pidl = GetItemIdListForFileName(shellFolderObj, root);
                                    if (dirIsOnlyServer) // sitova cesta "\\\\server" (server na siti)
                                    {
                                        pidlFolder = pidl;
                                        pidl = NULL;
                                    }
                                    else
                                    {
                                        *s = '\\';
                                        LPSHELLFOLDER folder2;
                                        if (pidl != NULL &&
                                            SUCCEEDED((ret = shellFolderObj->BindToObject(pidl, NULL,
                                                                                          IID_IShellFolder, (LPVOID*)&folder2))))
                                        {
                                            LPENUMIDLIST enumIDList;
                                            if (SUCCEEDED((ret = folder2->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN,
                                                                                      &enumIDList))))
                                            {
                                                ULONG celt;
                                                LPITEMIDLIST idList;
                                                STRRET str;
                                                enumIDList->Reset();
                                                IMalloc* alloc;
                                                if (SUCCEEDED(CoGetMalloc(1, &alloc)))
                                                {
                                                    while (1)
                                                    {
                                                        ret = enumIDList->Next(1, &idList, &celt);
                                                        if (ret == NOERROR)
                                                        {
                                                            ret = folder2->GetDisplayNameOf(idList, SHGDN_FORPARSING, &str);
                                                            if (ret == NOERROR)
                                                            {
                                                                char buf[MAX_PATH];
                                                                char* name;
                                                                switch (str.uType)
                                                                {
                                                                case STRRET_CSTR:
                                                                    name = str.cStr;
                                                                    break;
                                                                case STRRET_OFFSET:
                                                                    name = (char*)idList + str.uOffset;
                                                                    break;
                                                                case STRRET_WSTR:
                                                                {
                                                                    WideCharToMultiByte(CP_ACP, 0, str.pOleStr, -1, buf, MAX_PATH, NULL, NULL);
                                                                    buf[MAX_PATH - 1] = 0;
                                                                    name = buf;
                                                                    if (alloc->DidAlloc(str.pOleStr) == 1)
                                                                        alloc->Free(str.pOleStr);
                                                                    break;
                                                                }
                                                                default:
                                                                    name = NULL;
                                                                }

                                                                if (name != NULL)
                                                                {
                                                                    if (*(name + strlen(name) - 1) == '\\')
                                                                        *(name + strlen(name) - 1) = 0;
                                                                    if (StrICmp(name, root) == 0)
                                                                    {
                                                                        pidlFolder = idList;
                                                                        LPSHELLFOLDER swap = shellFolderObj;
                                                                        shellFolderObj = folder2;
                                                                        folder2 = swap;
                                                                        break; // pidl nalezen (ziskan)
                                                                    }
                                                                }
                                                            }
                                                            if (alloc->DidAlloc(idList) == 1)
                                                                alloc->Free(idList);
                                                        }
                                                        else
                                                            break;
                                                    }
                                                    alloc->Release();
                                                }
                                                enumIDList->Release();
                                            }
                                            folder2->Release();
                                        }
                                    }
                                    IMalloc* alloc;
                                    if (pidl != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
                                    {
                                        if (alloc->DidAlloc(pidl) == 1)
                                            alloc->Free(pidl);
                                        alloc->Release();
                                    }
                                    if (setWait)
                                        SetCursor(oldCur);
                                }
                            }
                        }
                    }
                    if (pidlFolder == NULL)
                        pidlFolder = GetItemIdListForFileName(shellFolderObj, dir);

                    // shellFolderObj + pidlFolder  -> dohromady "dir" folder
                }
                else
                    TRACE_E("BindToObject error: 0x" << std::hex << ret << std::dec);
                IMalloc* alloc;
                if (rootFolderID != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
                {
                    if (alloc->DidAlloc(rootFolderID) == 1)
                        alloc->Free(rootFolderID);
                    alloc->Release();
                }
            }
            else
                TRACE_E("SHGetSpecialFolderLocation error: 0x" << std::hex << ret << std::dec);
            if (desktop != NULL)
                desktop->Release();
        }
        else
            TRACE_E("SHGetDesktopFolder error: 0x" << std::hex << ret << std::dec);
    }
    else
        TRACE_I("GetShellFolder: unable to get folder for path containing invalid components: \"" << dir << "\"");
    if (shellFolderObj != NULL && pidlFolder != NULL)
        return TRUE;
    else
    {
        if (shellFolderObj != NULL)
            shellFolderObj->Release();
        if (pidlFolder != NULL)
        {
            IMalloc* alloc;
            if (SUCCEEDED(CoGetMalloc(1, &alloc)))
            {
                if (alloc->DidAlloc(pidlFolder) == 1)
                    alloc->Free(pidlFolder);
                alloc->Release();
            }
        }
        return FALSE;
    }
}

//*****************************************************************************
//
// CreateIDataObject
//

IDataObject* CreateIDataObjectAux(HWND hOwnerWindow, const char* rootDir, int files,
                                  CEnumFileNamesFunction nextFile, void* param)
{
    CALL_STACK_MESSAGE3("CreateIDataObjectAux(, %s, %d, ,)", rootDir, files);

    IDataObject* dataObj = NULL;
    IShellFolder* shellFolderObj;
    LPITEMIDLIST pidlFolder;
    if (GetShellFolder(rootDir, shellFolderObj, pidlFolder))
    {
        HRESULT ret;
        LPSHELLFOLDER folder;
        if (SUCCEEDED((ret = shellFolderObj->BindToObject(pidlFolder, NULL,
                                                          IID_IShellFolder, (LPVOID*)&folder))))
        {
            UINT itemsInList;
            ITEMIDLIST** list;

            list = CreateItemIdList(folder, files, nextFile, param, itemsInList, FALSE, FALSE, NULL, TRUE);
            if (list != NULL)
            {
                if (!SUCCEEDED((ret = folder->GetUIObjectOf(hOwnerWindow, itemsInList, (LPCITEMIDLIST*)list,
                                                            IID_IDataObject, NULL,
                                                            (LPVOID*)&dataObj))))
                {
                    TRACE_E("GetUIObjectOf error: 0x" << std::hex << ret << std::dec);
                }
                DestroyItemIdList(list, itemsInList);
            }
            folder->Release();
        }
        else
            TRACE_E("BindToObject error: 0x" << std::hex << ret << std::dec);

        IMalloc* alloc;
        if (pidlFolder != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (alloc->DidAlloc(pidlFolder) == 1)
                alloc->Free(pidlFolder);
            alloc->Release();
        }
        shellFolderObj->Release();
    }
    return dataObj;
}

IDataObject* CreateIDataObject(HWND hOwnerWindow, const char* rootDir, int files,
                               CEnumFileNamesFunction nextFile, void* param)
{
    __try
    {
        return CreateIDataObjectAux(hOwnerWindow, rootDir, files, nextFile, param);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SHLExceptionHasOccured++;
    }
    return NULL; // chyba
}

//*****************************************************************************
//
// CreateIContextMenu2
//

IContextMenu2* CreateIContextMenu2Aux(HWND hOwnerWindow, const char* rootDir, int files,
                                      CEnumFileNamesFunction nextFile, void* param)
{
    CALL_STACK_MESSAGE3("CreateIContextMenu2Aux(, %s, %d, ,)", rootDir, files);
    IContextMenu2* contextMenu2Obj = NULL;
    IShellFolder* shellFolderObj;
    LPITEMIDLIST pidlFolder;
    if (GetShellFolder(rootDir, shellFolderObj, pidlFolder))
    {
        HRESULT ret;
        LPSHELLFOLDER folder;
        if (SUCCEEDED((ret = shellFolderObj->BindToObject(pidlFolder, NULL,
                                                          IID_IShellFolder, (LPVOID*)&folder))))
        {
            UINT itemsInList;
            ITEMIDLIST** list;

            list = CreateItemIdList(folder, files, nextFile, param, itemsInList,
                                    strcmp(rootDir, "\\\\") == 0,                                                                     // jde o "\\\\"?
                                    rootDir[0] == '\\' && rootDir[1] == '\\' && rootDir[2] != 0 && strchr(rootDir + 2, '\\') == NULL, // jde o "\\\\server"?
                                    rootDir);
            if (list != NULL)
            {
                IContextMenu* contextMenuObj;
                if (SUCCEEDED((ret = folder->GetUIObjectOf(hOwnerWindow, itemsInList, (LPCITEMIDLIST*)list,
                                                           IID_IContextMenu, NULL,
                                                           (LPVOID*)&contextMenuObj))))
                {
                    if (!SUCCEEDED((ret = contextMenuObj->QueryInterface(IID_IContextMenu2,
                                                                         (void**)&contextMenu2Obj))))
                    {
                        TRACE_E("QueryInterface error: 0x" << std::hex << ret << std::dec);
                    }
                    contextMenuObj->Release();
                }
                else
                    TRACE_E("GetUIObjectOf error: 0x" << std::hex << ret << std::dec);
                DestroyItemIdList(list, itemsInList);
            }
            folder->Release();
        }
        else
            TRACE_E("BindToObject error: 0x" << std::hex << ret << std::dec);

        IMalloc* alloc;
        if (pidlFolder != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (alloc->DidAlloc(pidlFolder) == 1)
                alloc->Free(pidlFolder);
            alloc->Release();
        }
        shellFolderObj->Release();
    }
    return contextMenu2Obj;
}

IContextMenu2* CreateIContextMenu2(HWND hOwnerWindow, const char* rootDir, int files,
                                   CEnumFileNamesFunction nextFile, void* param)
{
    __try
    {
        return CreateIContextMenu2Aux(hOwnerWindow, rootDir, files, nextFile, param);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SHLExceptionHasOccured++;
    }
    return NULL; // chyba
}

//*****************************************************************************
//
// CreateIContextMenu2
//

IContextMenu2* CreateIContextMenu2Aux(HWND hOwnerWindow, const char* dir)
{
    CALL_STACK_MESSAGE2("CreateIContextMenu2Aux(, %s)", dir);
    IContextMenu2* contextMenu2Obj = NULL;
    IShellFolder* shellFolderObj;
    LPITEMIDLIST pidlFolder;
    if (GetShellFolder(dir, shellFolderObj, pidlFolder))
    {
        HRESULT ret;
        IContextMenu* contextMenuObj;
        if (SUCCEEDED((ret = shellFolderObj->GetUIObjectOf(hOwnerWindow, 1, (LPCITEMIDLIST*)&pidlFolder,
                                                           IID_IContextMenu, NULL,
                                                           (LPVOID*)&contextMenuObj))))
        {
            if (!SUCCEEDED((ret = contextMenuObj->QueryInterface(IID_IContextMenu2,
                                                                 (void**)&contextMenu2Obj))))
            {
                TRACE_E("QueryInterface error: 0x" << std::hex << ret << std::dec);
            }
            contextMenuObj->Release();
        }
        else
            TRACE_E("GetUIObjectOf error: 0x" << std::hex << ret << std::dec);

        IMalloc* alloc;
        if (pidlFolder != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (alloc->DidAlloc(pidlFolder) == 1)
                alloc->Free(pidlFolder);
            alloc->Release();
        }
        shellFolderObj->Release();
    }
    return contextMenu2Obj;
}

IContextMenu2* CreateIContextMenu2(HWND hOwnerWindow, const char* dir)
{
    __try
    {
        return CreateIContextMenu2Aux(hOwnerWindow, dir);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SHLExceptionHasOccured++;
    }
    return NULL; // chyba
}

//*****************************************************************************
//
// HasDropTarget
//

BOOL HasDropTarget(const char* dir)
{
    CALL_STACK_MESSAGE2("HasDropTarget(%s)", dir);
    /*
  IShellFolder *shellFolderObj;
  LPITEMIDLIST pidlFolder;
  ULONG attrs = 0;
  if (GetShellFolder(dir, shellFolderObj, pidlFolder))
  {
    HRESULT ret;
    attrs = SFGAO_DROPTARGET;  // ptame se jen na tento atribut
    if (!SUCCEEDED((ret = shellFolderObj->GetAttributesOf(1, (LPCITEMIDLIST *)&pidlFolder, &attrs))))
    {
      TRACE_E("GetAttributesOf error: " << hex << ret);
      attrs = 0;
    }

    IMalloc *alloc;
    if (pidlFolder != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
    {
      if (alloc->DidAlloc(pidlFolder) == 1) alloc->Free(pidlFolder);
      alloc->Release();
    }
    shellFolderObj->Release();
  }
  return (attrs & SFGAO_DROPTARGET) != 0;
*/
    IDropTarget* drop = CreateIDropTarget(NULL, dir); // bohuzel to jinak nejde ...
    if (drop != NULL)
    {
        drop->Release();
        return TRUE;
    }
    return FALSE;
}

//*****************************************************************************
//
// CreateIDropTarget
//

IDropTarget* CreateIDropTargetAux(HWND hOwnerWindow, const char* dir)
{
    CALL_STACK_MESSAGE2("CreateIDropTargetAux(, %s)", dir);
    IDropTarget* dropTargetObj = NULL;
    IShellFolder* shellFolderObj;
    LPITEMIDLIST pidlFolder;
    if (GetShellFolder(dir, shellFolderObj, pidlFolder))
    {
        HRESULT ret;
        if (!SUCCEEDED((ret = shellFolderObj->GetUIObjectOf(hOwnerWindow, 1,
                                                            (LPCITEMIDLIST*)&pidlFolder,
                                                            IID_IDropTarget, NULL,
                                                            (LPVOID*)&dropTargetObj))))
        {
            TRACE_I("GetUIObjectOf error: 0x" << std::hex << ret << std::dec);
        }

        IMalloc* alloc;
        if (pidlFolder != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (alloc->DidAlloc(pidlFolder) == 1)
                alloc->Free(pidlFolder);
            alloc->Release();
        }
        shellFolderObj->Release();
    }
    return dropTargetObj;
}

IDropTarget* CreateIDropTarget(HWND hOwnerWindow, const char* dir)
{
    __try
    {
        return CreateIDropTargetAux(hOwnerWindow, dir);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SHLExceptionHasOccured++;
    }
    return NULL; // chyba
}

//*****************************************************************************
//
// OpenSpecFolder
//

void OpenSpecFolder(HWND hOwnerWindow, int specFolder)
{
    CALL_STACK_MESSAGE2("OpenSpecFolder(, %d)", specFolder);
    ITEMIDLIST* pidl;
    if (SHGetSpecialFolderLocation(NULL, specFolder, &pidl) == NOERROR && pidl != NULL)
    {
        CShellExecuteWnd shellExecuteWnd;
        SHELLEXECUTEINFO se;
        memset(&se, 0, sizeof(SHELLEXECUTEINFO));
        se.cbSize = sizeof(SHELLEXECUTEINFO);
        se.fMask = SEE_MASK_IDLIST;
        se.lpVerb = "open";
        se.hwnd = shellExecuteWnd.Create(hOwnerWindow, "SEW: OpenSpecFolder specFolder=%d verb=%s", specFolder, se.lpVerb);
        se.nShow = SW_SHOWNORMAL;
        se.lpIDList = pidl;
        ShellExecuteEx(&se);

        IMalloc* alloc;
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (pidl != NULL && alloc->DidAlloc(pidl) == 1)
                alloc->Free(pidl);
            alloc->Release();
        }
    }
}

//*****************************************************************************
//
// OpenFolder
//

void OpenFolderAndFocusItem(HWND hOwnerWindow, const char* dir, const char* item)
{
    CALL_STACK_MESSAGE2("OpenFolder(, %s)", dir);
    // pokud cesta obsahuje komponenty koncici na mezery/tecky, shell nam nevrati
    // pidl pro pozadovanou cestu, ale pro cestu vzniklou oriznutim techto
    // mezer/tecek, takze radsi se na to vcas vykasleme...
    char mydir[2 * MAX_PATH];
    strcpy(mydir, dir);
    if (item[0] != 0)
        SalPathAppend(mydir, item, 2 * MAX_PATH);
    if (PathContainsValidComponents((char*)mydir, FALSE))
    {
        BOOL useOldMethod = TRUE; // SHOpenFolderAndSelectItems je podporovano od XP dal a my zatim behame i na W2K a XP bez SPx
        if (item[0] != 0)         // pokud nemame vybrat polozku, nepouzijeme SHOpenFolderAndSelectItems, protoze by zobrazilo nadrazeny adresar, viz MSDN
        {
            HMODULE hShell32 = LoadLibrary("shell32.dll");
            if (hShell32 != NULL)
            {
                typedef HRESULT(WINAPI * F_SHOpenFolderAndSelectItems)(PCIDLIST_ABSOLUTE pidlFolder, UINT cidl, PCUITEMID_CHILD_ARRAY apidl, DWORD dwFlags);
                F_SHOpenFolderAndSelectItems mySHOpenFolderAndSelectItems = NULL;
                mySHOpenFolderAndSelectItems = (F_SHOpenFolderAndSelectItems)GetProcAddress(hShell32, "SHOpenFolderAndSelectItems"); // Min: XP
                if (mySHOpenFolderAndSelectItems != NULL)
                {
                    LPITEMIDLIST pidl = NULL;
                    LPSHELLFOLDER desktop;
                    if (SUCCEEDED(SHGetDesktopFolder(&desktop)))
                    {
                        pidl = GetItemIdListForFileName(desktop, mydir);
                        desktop->Release();
                    }
                    else
                        TRACE_E("SHGetDesktopFolder error");

                    mySHOpenFolderAndSelectItems(pidl, 0, NULL, 0);
                    useOldMethod = FALSE;

                    IMalloc* alloc;
                    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
                    {
                        if (pidl != NULL && alloc->DidAlloc(pidl) == 1)
                            alloc->Free(pidl);
                        alloc->Release();
                    }
                }
                FreeLibrary(hShell32);
            }
        }

        if (useOldMethod)
        {
            LPITEMIDLIST pidl = NULL;
            LPSHELLFOLDER desktop;
            if (SUCCEEDED(SHGetDesktopFolder(&desktop)))
            {
                pidl = GetItemIdListForFileName(desktop, dir);
                desktop->Release();
            }
            else
                TRACE_E("SHGetDesktopFolder error");

            if (pidl != NULL)
            {
                CShellExecuteWnd shellExecuteWnd;
                SHELLEXECUTEINFO se;
                memset(&se, 0, sizeof(SHELLEXECUTEINFO));
                se.cbSize = sizeof(SHELLEXECUTEINFO);
                se.fMask = SEE_MASK_IDLIST;
                se.lpVerb = "open";
                se.hwnd = shellExecuteWnd.Create(hOwnerWindow, "SEW: OpenFolderAndFocusItem verb=%s", se.lpVerb);
                se.nShow = SW_SHOWNORMAL;
                se.lpIDList = pidl;
                ShellExecuteEx(&se);

                IMalloc* alloc;
                if (SUCCEEDED(CoGetMalloc(1, &alloc)))
                {
                    if (pidl != NULL && alloc->DidAlloc(pidl) == 1)
                        alloc->Free(pidl);
                    alloc->Release();
                }
            }
        }
    }
    else
        TRACE_I("OpenFolderAndFocusItem: unable to open folder for path containing invalid components: \"" << mydir << "\"");
}

//*****************************************************************************
//
// GetTargetDirectory
//
//  parent  - okno vlastnika dialogu
//  title   - titul dialogu
//  comment - text zobrazeny nad tree-view
//  path    - buffer pro vybranou cestu (delka minimalne MAX_PATH)
//
//  vraci TRUE pokud je path platna nova cesta

struct CBrowseData
{
    const char* Title;
    const char* InitDir;
    HWND HCenterWindow;
};

int CALLBACK DirectoryBrowse(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    CALL_STACK_MESSAGE4("DirectoryBrowse(, 0x%X, 0x%IX, 0x%IX)", uMsg, lParam, lpData);
    if (uMsg == BFFM_INITIALIZED)
    {
        MultiMonCenterWindow(hwnd, ((CBrowseData*)lpData)->HCenterWindow, FALSE);

        // nastavim header
        SetWindowText(hwnd, ((CBrowseData*)lpData)->Title);
        if (((CBrowseData*)lpData)->InitDir != NULL)
        {
            char path[MAX_PATH];
            GetRootPath(path, ((CBrowseData*)lpData)->InitDir);
            if (strlen(path) < strlen(((CBrowseData*)lpData)->InitDir)) // neni to root-dir
            {
                strcpy(path, ((CBrowseData*)lpData)->InitDir);
                char& ch = path[strlen(path) - 1];
                if (ch == '\\')
                    ch = 0;
            }
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)path);
        }
    }
    if (uMsg == BFFM_SELCHANGED)
    {
        if ((ITEMIDLIST*)lParam != NULL)
        {
            char path[MAX_PATH];
            BOOL ret = SHGetPathFromIDList((ITEMIDLIST*)lParam, path);
            SendMessage(hwnd, BFFM_ENABLEOK, 0, ret);
        }
    }
    return 0;
}

BOOL GetTargetDirectoryAux(HWND parent, HWND hCenterWindow,
                           const char* title, const char* comment,
                           char* path, BOOL onlyNet, const char* initDir)
{
    __try
    {
        ITEMIDLIST* pidl; // vyber root-folderu
        if (onlyNet)
            SHGetSpecialFolderLocation(parent, CSIDL_NETWORK, &pidl);
        else
            pidl = NULL;

        // otevreni dialogu
        char display[MAX_PATH];
        BROWSEINFO bi;
        ZeroMemory(&bi, sizeof(bi));
        bi.hwndOwner = parent;
        bi.pidlRoot = pidl;
        bi.pszDisplayName = display;
        bi.lpszTitle = comment;
        bi.ulFlags = BIF_RETURNONLYFSDIRS;
        /* j.r.: pod W2K se po otevreni focus stavi na OK misto do treeview (jak to bylo drive); navic nefunguje ensure_visible; proste HNUS, vracime se ke stare verzi dialogu; pripadne ho muzem casem prepsat
    if (!onlyNet)  // Petr: dialog Network funguje jen ve stare verzi - nova neumi pozadat usera o login na server (situace, kdy mu nestaci aktualni login)
      bi.ulFlags |= BIF_NEWDIALOGSTYLE; // vetsi a natahovaci dialog
    */
        bi.lpfn = DirectoryBrowse;
        CBrowseData bd;
        bd.Title = title;
        bd.InitDir = initDir;
        bd.HCenterWindow = hCenterWindow;
        bi.lParam = (LPARAM)&bd;
        LPITEMIDLIST res = SHBrowseForFolder(&bi);
        BOOL ret = FALSE; // navratova hodnota
        if (res != NULL)
        {
            SHGetPathFromIDList(res, path);
            ret = TRUE;
        }
        // uvolneni item-id-listu
        IMalloc* alloc;
        if ((pidl != NULL || res != NULL) && SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (pidl != NULL && alloc->DidAlloc(pidl) == 1)
                alloc->Free(pidl);
            if (res != NULL && alloc->DidAlloc(res) == 1)
                alloc->Free(res);
            alloc->Release();
        }
        return ret;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        GTDExceptionHasOccured++;
        return FALSE; // chyba
    }
}

void ResolveNetHoodPath(char* path)
{
    if (path[0] == '\\')
        return; // UNC cesta -> nemuze byt NetHood

    char name[MAX_PATH];
    GetRootPath(name, path);
    if (GetDriveType(name) != DRIVE_FIXED)
        return; // neni lokalni fixed cesta -> nemuze byt NetHood

    BOOL tryTarget = FALSE; // je-li TRUE, ma cenu zkouset najit soubor "target.lnk"
    lstrcpyn(name, path, MAX_PATH);
    if (SalPathAppend(name, "desktop.ini", MAX_PATH))
    {
        HANDLE hFile = HANDLES_Q(CreateFile(name, GENERIC_READ,
                                            FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
                                            OPEN_EXISTING,
                                            FILE_FLAG_SEQUENTIAL_SCAN,
                                            NULL));
        if (hFile != INVALID_HANDLE_VALUE)
        {
            if (GetFileSize(hFile, NULL) <= 1000) // zatim vsechny meli 92 bytu, takze 1000 bytu by melo bohate stacit
            {
                char buf[1000];
                DWORD read;
                if (ReadFile(hFile, buf, 1000, &read, NULL) && read != 0) // nacteme soubor do pameti
                {
                    char* s = buf;
                    char* end = buf + read;
                    while (s < end) // hledame v souboru CLSID "folder shortcut"
                    {
                        if (*s == '{')
                        {
                            s++;
                            char* beg = s;
                            while (s < end && *s != '}')
                                s++;
                            if (s < end)
                            {
                                const char* folderShortcutCLSID = "0AFACED1-E828-11D1-9187-B532F1E9575D";
                                if (StrNICmp(beg, folderShortcutCLSID, (int)(s - beg)) == 0)
                                {
                                    tryTarget = TRUE;
                                    break;
                                }
                            }
                        }
                        else
                            s++;
                    }
                }
            }
            HANDLES(CloseHandle(hFile));
        }
    }

    if (tryTarget)
    {
        lstrcpyn(name, path, MAX_PATH);
        if (SalPathAppend(name, "target.lnk", MAX_PATH))
        {
            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(name, &data));
            if (find != INVALID_HANDLE_VALUE) // soubor existuje a mame jeho 'data'
            {
                HANDLES(FindClose(find));

                HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                IShellLink* link;
                if (CoCreateInstance(CLSID_ShellLink, NULL,
                                     CLSCTX_INPROC_SERVER, IID_IShellLink,
                                     (LPVOID*)&link) == S_OK)
                {
                    IPersistFile* fileInt;
                    if (link->QueryInterface(IID_IPersistFile, (LPVOID*)&fileInt) == S_OK)
                    {
                        OLECHAR oleName[MAX_PATH];
                        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, oleName, MAX_PATH);
                        oleName[MAX_PATH - 1] = 0;
                        if (fileInt->Load(oleName, STGM_READ) == S_OK)
                        {
                            if (link->GetPath(name, MAX_PATH, &data, SLGP_UNCPRIORITY) == NOERROR)
                            {                       // nepouzijeme Resolve, protoze zde to neni az tak kriticke a dost by to zpomalovalo
                                strcpy(path, name); // heureka, konecne vime kam ten link vede
                            }
                        }
                        fileInt->Release();
                    }
                    link->Release();
                }
                SetCursor(oldCur);
            }
        }
    }
}

BOOL GetTargetDirectory(HWND parent, HWND hCenterWindow,
                        const char* title, const char* comment,
                        char* path, BOOL onlyNet, const char* initDir)
{
    CALL_STACK_MESSAGE5("GetTargetDirectory(, , %s, %s, , %d, %s)", title, comment, onlyNet, initDir);
    BOOL ret = GetTargetDirectoryAux(parent, hCenterWindow, title, comment, path, onlyNet, initDir);
    if (ret)
        ResolveNetHoodPath(path);
    return ret;
}

//*****************************************************************************
//
// GetNewOrBackgroundMenu
//
// hOwnerWindow - predek oteviranych oken (jak chyb, tak prikazu kontextoveho menu)
// dir - adresar, od ktereho se ziskava menu New
// menu - navratova hodnota - submenu New + jeho interfacy
// minCmd, maxCmd - interval moznych hodnot prikazu v 'menu'
// backgoundMenu - TRUE = chceme kompletni view-background menu (right-click za polozkami v Explorerovi; nejen menu New, ale take napr. Tortoise CVS, atd.)

void GetMenuNewAux(IContextMenu2* contextMenu2, HMENU m, int minCmd, int maxCmd)
{
    CALL_STACK_MESSAGE_NONE

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        UINT flags = CMF_NORMAL | CMF_EXPLORE;
        // osetrime stisknuty shift - rozsirene kontextove menu, pod W2K je tam napriklad Run as...
#define CMF_EXTENDEDVERBS 0x00000100 // rarely used verbs
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shiftPressed)
            flags |= CMF_EXTENDEDVERBS;

        contextMenu2->QueryContextMenu(m, 0, minCmd, maxCmd, flags);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 16))
    {
        QCMExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void GetNewOrBackgroundMenu(HWND hOwnerWindow, const char* dir, CMenuNew* menu,
                            int minCmd, int maxCmd, BOOL backgoundMenu)
{
    CALL_STACK_MESSAGE4("GetNewOrBackgroundMenu(, %s, , %d, %d)", dir, minCmd, maxCmd);
    menu->Init();
    IShellFolder* shellFolderObj;
    LPITEMIDLIST pidlFolder;
    if (GetShellFolder(dir, shellFolderObj, pidlFolder))
    {
        HRESULT ret;
        LPSHELLFOLDER folder;
        if (SUCCEEDED((ret = shellFolderObj->BindToObject(pidlFolder, NULL,
                                                          IID_IShellFolder, (LPVOID*)&folder))))
        {
            IContextMenu* contextMenu;
            if (SUCCEEDED((ret = folder->CreateViewObject(hOwnerWindow, IID_IContextMenu,
                                                          (void**)&contextMenu))))
            {
                IContextMenu2* contextMenu2 = NULL;
                if (SUCCEEDED((ret = contextMenu->QueryInterface(IID_IContextMenu2,
                                                                 (void**)&contextMenu2))))
                {
                    HMENU m = CreatePopupMenu();
                    if (m != NULL)
                    {
                        GetMenuNewAux(contextMenu2, m, minCmd, maxCmd);
                        RemoveUselessSeparatorsFromMenu(m);

                        if (backgoundMenu) // bereme cele background menu
                        {
                            menu->Set(contextMenu2, m);
                        }
                        else // vyrizneme jen menu New
                        {
                            MENUITEMINFO mi;
                            int index = 0;
                            int foundIndex = -1;
                            HMENU foundSubMenu = NULL;
                            while (1)
                            {
                                mi.cbSize = sizeof(mi);
                                mi.fMask = MIIM_SUBMENU;
                                if (GetMenuItemInfo(m, index, TRUE, &mi))
                                {
                                    if (mi.hSubMenu != NULL)
                                    { // hledame posledni submenu (uzivatelske polozky snad pribyvaji jen pred Windows polozky, uvidime casem)
                                        foundIndex = index;
                                        foundSubMenu = mi.hSubMenu;
                                    }
                                }
                                else
                                    break;
                                index++;
                            }
                            if (foundIndex != -1)
                            {
                                menu->Set(contextMenu2, foundSubMenu);
                                RemoveMenu(m, foundIndex, MF_BYPOSITION);
                            }
                            DestroyMenu(m);
                        }
                    }
                    if (!menu->MenuIsAssigned())
                        contextMenu2->Release();
                }
                contextMenu->Release();
            }
            folder->Release();
        }

        IMalloc* alloc;
        if (pidlFolder != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (alloc->DidAlloc(pidlFolder) == 1)
                alloc->Free(pidlFolder);
            alloc->Release();
        }
        shellFolderObj->Release();
    }
}

//*****************************************************************************
//
// CMenuNew
//

void CMenuNew::ReleaseBody()
{
    __try
    {
        // HMENU Menu je zdestruovano primo z menu, do ktereho bylo pripojeno
        if (Menu2 != NULL)
            Menu2->Release(); // toto volani sem tam spadne
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        MenuNewExceptionHasOccured++;
    }
    Init();
}

void CMenuNew::Release()
{
    CALL_STACK_MESSAGE1("CMenuNew::Release()");
    ReleaseBody();
}

//
//*****************************************************************************
// CTextDataObject
//

STDMETHODIMP CTextDataObject::QueryInterface(REFIID iid, void** ppv)
{
    if (iid == IID_IUnknown || iid == IID_IDataObject)
    {
        *ppv = this;
        AddRef();
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP CTextDataObject::GetData(FORMATETC* formatEtc, STGMEDIUM* medium)
{
    if (formatEtc == NULL || medium == NULL)
        return E_INVALIDARG;
    if ((formatEtc->cfFormat == CF_TEXT || formatEtc->cfFormat == CF_UNICODETEXT) && (formatEtc->tymed & TYMED_HGLOBAL))
    {
        HGLOBAL dataDup = NULL; // vyrobime kopii Data
        if (Data != NULL)
        {
            BOOL ok = FALSE;
            if (formatEtc->cfFormat == CF_TEXT)
            {
                SIZE_T size = GlobalSize(Data);
                dataDup = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size));
                if (dataDup != NULL)
                {
                    void* ptr1 = HANDLES(GlobalLock(dataDup));
                    void* ptr2 = HANDLES(GlobalLock(Data));
                    if (ptr1 != NULL && ptr2 != NULL)
                    {
                        memcpy(ptr1, ptr2, size);
                        ok = TRUE;
                    }
                    if (ptr2 != NULL)
                        HANDLES(GlobalUnlock(Data));
                    if (ptr1 != NULL)
                        HANDLES(GlobalUnlock(dataDup));
                }
            }
            else // formatEtc->cfFormat == CF_UNICODETEXT
            {
                const char* ptr2 = (const char*)HANDLES(GlobalLock(Data));
                if (ptr2 != NULL)
                {
                    int len = MultiByteToWideChar(CP_ACP, 0, ptr2, -1, NULL, 0);
                    if (len > 0)
                    {
                        dataDup = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, len * sizeof(WCHAR)));
                        if (dataDup != NULL)
                        {
                            WCHAR* ptr1 = (WCHAR*)HANDLES(GlobalLock(dataDup));
                            if (ptr1 != NULL)
                            {
                                if (ConvertA2U(ptr2, -1, ptr1, len))
                                    ok = TRUE;
                                else
                                    TRACE_E("ConvertA2U() failed to make unicode translation for our ANSI text.");
                                HANDLES(GlobalUnlock(dataDup));
                            }
                        }
                    }
                    else
                        TRACE_E("MultiByteToWideChar() failed to return size of unicode translation for our ANSI text.");
                    HANDLES(GlobalUnlock(Data));
                }
            }
            if (!ok && dataDup != NULL)
            {
                NOHANDLES(GlobalFree(dataDup));
                dataDup = NULL;
            }
        }
        if (dataDup != NULL) // mame data, ulozime je na medium a vratime
        {
            medium->tymed = TYMED_HGLOBAL;
            medium->hGlobal = dataDup;
            medium->pUnkForRelease = NULL;
            return S_OK;
        }
        else
            return E_UNEXPECTED;
    }
    return (formatEtc->tymed & TYMED_HGLOBAL) ? DV_E_FORMATETC : DV_E_TYMED;
}

//
//*****************************************************************************
// GetMyDocumentsOrDesktopPath
//

BOOL GetMyDocumentsOrDesktopPath(char* path, int pathLen)
{
    char buff[2 * MAX_PATH];

    BOOL ret = FALSE;
    ITEMIDLIST* pidl = NULL;
    if (SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl) == NOERROR)
    {
        if (SHGetPathFromIDList(pidl, buff))
            ret = TRUE;
        IMalloc* alloc;
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            alloc->Free(pidl);
            alloc->Release();
        }
    }
    if (!ret && SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOP, &pidl) == NOERROR)
    {
        if (SHGetPathFromIDList(pidl, buff))
            ret = TRUE;
        IMalloc* alloc;
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            alloc->Free(pidl);
            alloc->Release();
        }
    }

    if (ret)
    {
        if ((int)strlen(buff) >= pathLen)
            TRACE_E("GetMyDocumentsOrDesktopPath() Buffer too small!");

        lstrcpyn(path, buff, pathLen);
    }

    return ret;
}

//
//*****************************************************************************
// GetSHObjectName
//

BOOL GetSHObjectName(ITEMIDLIST* pidl, DWORD flags, char* name, int nameSize, IMalloc* alloc)
{
    BOOL ret = FALSE;
    if (nameSize > 0)
        name[0] = 0;
    if (pidl != NULL && pidl->mkid.cb != 0) // v seznamu musi byt alespon jedno IDcko, jinak neni co zjistovat
    {
        // najdu posledni IDcko v seznamu
        ITEMIDLIST* lastID = pidl;
        while (1)
        {
            ITEMIDLIST* nextID = (ITEMIDLIST*)((BYTE*)lastID + lastID->mkid.cb);
            if (nextID->mkid.cb != 0)
                lastID = nextID;
            else
                break;
        }

        // provizorne zkratim seznam IDcek a ziskam IShellFolder, ve kterem lezi puvodni 'pidl'
        USHORT lastCB = lastID->mkid.cb;
        lastID->mkid.cb = 0;

        // vytahneme Desktop folder
        IShellFolder* desktopFolder;
        if (SHGetDesktopFolder(&desktopFolder) == NOERROR && desktopFolder != NULL)
        {
            IShellFolder* folder;
            if (pidl->mkid.cb != 0) // neprazdny seznam IDcek, pozadame desktop o prislusny folder
            {
                if (desktopFolder->BindToObject(pidl, NULL, IID_IShellFolder, (void**)&folder) != S_OK)
                {
                    folder = NULL;
                    TRACE_E("GetSHObjectName(): unable to get folder for 'pidl' without last ID");
                }
                desktopFolder->Release();
            }
            else // prazdny seznam IDcek = folder je primo desktop
                folder = desktopFolder;

            if (folder != NULL)
            {
                // oprava seznamu ('pidl') do puvodni velikosti
                lastID->mkid.cb = lastCB;

                STRRET str;
                if (folder->GetDisplayNameOf(lastID, flags, &str) == S_OK)
                {
                    ret = TRUE;
                    switch (str.uType)
                    {
                    case STRRET_CSTR:
                        lstrcpyn(name, str.cStr, nameSize);
                        break;
                    case STRRET_OFFSET:
                        lstrcpyn(name, (char*)lastID + str.uOffset, nameSize);
                        break;

                    case STRRET_WSTR:
                    {
                        if (WideCharToMultiByte(CP_ACP, 0, str.pOleStr, -1, name, nameSize, NULL, NULL) == 0)
                        {
                            ret = FALSE;
                            if (nameSize > 0)
                                name[0] = 0;
                        }
                        else
                        {
                            if (nameSize > 0)
                                name[nameSize - 1] = 0;
                        }
                        if (alloc->DidAlloc(str.pOleStr) == 1)
                            alloc->Free(str.pOleStr);
                        break;
                    }

                    default:
                    {
                        ret = FALSE;
                        TRACE_E("GetSHObjectName(): unexpected str.uType");
                        break;
                    }
                    }
                }
                else
                    TRACE_E("GetSHObjectName(): GetDisplayNameOf has failed");

                folder->Release();
            }
        }
        else
            TRACE_E("GetSHObjectName(): unable to get Desktop folder");

        // oprava seznamu ('pidl') do puvodni velikosti
        lastID->mkid.cb = lastCB;
    }
    else
        TRACE_E("GetSHObjectName(): unable to get name for empty 'pidl'");
    return ret;
}
