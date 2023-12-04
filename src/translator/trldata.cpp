// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndtree.h"
#include "wndframe.h"
#include "translator.h"
#include "versinfo.h"

#include "wndout.h"
#include "datarh.h"

CData Data;

// pro konverze
#define TEMP_BUFF_LEN 500000
static wchar_t TempBuff[TEMP_BUFF_LEN];

BOOL __GetNextChar(wchar_t& charValue, wchar_t*& start, wchar_t* end)
{
    if (*start == L'\\') // escape-sequence
    {
        if (++start < end)
        {
            switch (*start)
            {
            case L'\'':
                charValue = L'\'';
                start++;
                return TRUE;
            case L'"':
                charValue = L'\"';
                start++;
                return TRUE;
            case L'?':
                charValue = L'\?';
                start++;
                return TRUE;
            case L'\\':
                charValue = L'\\';
                start++;
                return TRUE;
            case L'a':
                charValue = L'\a';
                start++;
                return TRUE;
            case L'b':
                charValue = L'\b';
                start++;
                return TRUE;
            case L'f':
                charValue = L'\f';
                start++;
                return TRUE;
            case L'n':
                charValue = L'\n';
                start++;
                return TRUE;
            case L'r':
                charValue = L'\r';
                start++;
                return TRUE;
            case L't':
                charValue = L'\t';
                start++;
                return TRUE;
            case L'v':
                charValue = L'\v';
                start++;
                return TRUE;

            case L'x':
            case L'X':
            {
                BOOL ok = FALSE;
                start++;
                charValue = 0;
                int c = 4;
                while (start < end)
                {
                    ok = TRUE;
                    if (*start >= L'0' && *start <= L'9')
                    {
                        charValue <<= 4;
                        charValue |= (wchar_t)(*start - L'0');
                    }
                    else
                    {
                        if (*start >= L'a' && *start <= L'f')
                        {
                            charValue <<= 4;
                            charValue |= (wchar_t)(0x0A + (*start - L'a'));
                        }
                        else
                        {
                            if (*start >= L'A' && *start <= L'F')
                            {
                                charValue <<= 4;
                                charValue |= (wchar_t)(0x0A + (*start - L'A'));
                            }
                            else
                            {
                                ok = c != 4; // alespon jedna cifra
                                break;
                            }
                        }
                    }
                    start++;
                    if (--c == 0)
                        break;
                }
                return ok;
            }

            default:
                return FALSE;
            }
        }
        else
            return FALSE;
    }
    else
    {
        charValue = *start++;
        return TRUE;
    }
}

int __GetCharacterString(wchar_t* buf, int bufSize, wchar_t data) // wchar_t buf[7] alespon
{
    switch (data)
    {
        //    case L'\'': wcscpy_s(buf, L"\\'"); return 2;   // pro ucely ALTAP Trlanslator je toto zbytecne
        //    case L'\"': wcscpy_s(buf, L"\\\""); return 2;
    case L'\\':
        wcscpy_s(buf, bufSize, L"\\\\");
        return 2;
    case L'\a':
        wcscpy_s(buf, bufSize, L"\\a");
        return 2;
    case L'\b':
        wcscpy_s(buf, bufSize, L"\\b");
        return 2;
    case L'\f':
        wcscpy_s(buf, bufSize, L"\\f");
        return 2;
    case L'\n':
        wcscpy_s(buf, bufSize, L"\\n");
        return 2;
    case L'\r':
        wcscpy_s(buf, bufSize, L"\\r");
        return 2;
    case L'\t':
        wcscpy_s(buf, bufSize, L"\\t");
        return 2;
    case L'\v':
        wcscpy_s(buf, bufSize, L"\\v");
        return 2;

    default:
    {
        if (data != 0xFFFE && data != 0xFEFF && data != 0xFFFF) // nejsou to specialni znaky
        {
            buf[0] = data;
            return 1;
        }
        swprintf_s(buf, bufSize, L"\\x%04X", (unsigned)data);
        return 6;
    }
    }
}

BOOL DecodeString(const wchar_t* iter, int len, wchar_t** string)
{
    const wchar_t* s = iter;
    wchar_t* tmpBuff = TempBuff;
    int tmpLen = 0;

    while (len > 0)
    {
        int len2 = __GetCharacterString(tmpBuff, _countof(TempBuff) - (tmpBuff - TempBuff), *s++);
        tmpBuff += len2;
        tmpLen += len2;
        len--;
    }

    *string = (wchar_t*)malloc(sizeof(wchar_t) * (tmpLen + 1));
    if (*string == NULL)
    {
        TRACE_E("Low memory");
        return FALSE;
    }
    memcpy(*string, TempBuff, tmpLen * sizeof(wchar_t));
    (*string)[tmpLen] = 0;
    return TRUE;
}

void EncodeString(wchar_t* iter, wchar_t** string)
{
    WORD len = (WORD)wcslen(iter);
    if (len > 0)
    {
        wchar_t c;
        wchar_t *start, *end;
        start = iter;
        end = start + len;
        while (start < end)
        {
            if (__GetNextChar(c, start, end))
            {
                PUT_WORD(*string, c);
                (*string)++;
            }
            else
                break;
        }
    }
    PUT_WORD(*string, 0); // terminator za retezcem
    (*string) += 1;
}

BOOL GetStringFromWindow(HWND hWindow, wchar_t** string)
{
    if (*string == NULL)
    {
        TRACE_E("*string == NULL");
        return FALSE;
    }
    int len = GetWindowTextLength(hWindow);

    wchar_t* tmp = (wchar_t*)malloc(sizeof(wchar_t) * (len + 1));
    if (tmp == NULL)
    {
        TRACE_E("Low memory");
        return FALSE;
    }
    GetWindowTextW(hWindow, tmp, len + 1);
    //  SendMessageW(hWindow, WM_GETTEXT, len + 1, (LPARAM)tmp);

    //  MessageBoxW(hWindow, tmp, tmp, IDOK); // test funkcniho unicode

    free(*string);
    *string = tmp;
    return TRUE;
}

//*****************************************************************************
//
// CData
//

CData::CData()
    : StrData(2, 2),
      MenuData(10, 10),
      DlgData(40, 20),
      DialogsTranslationStates(1000, 1000),
      MenusTranslationStates(1000, 1000),
      StringsTranslationStates(1000, 1000),
      Relayout(100, 100),
      SalMenuSections(50, 50),
      IgnoreLstItems(10, 10),
      CheckLstItems(5, 5),
      Bookmarks(10, 10),
      MenuPreview(100, 100)
{
    Clean();
}

void CData::Clean()
{
    Dirty = FALSE;

    EnumReturn = TRUE;
    HTranModule = NULL;
    Importing = FALSE;
    ShowStyleWarning = TRUE;
    ShowExStyleWarning = TRUE;
    ShowDlgEXWarning = TRUE;
    MUIMode = FALSE;

    ProjectFile[0] = 0;
    SourceFile[0] = 0;
    TargetFile[0] = 0;
    IncludeFile[0] = 0;
    SalMenuFile[0] = 0;
    IgnoreLstFile[0] = 0;
    CheckLstFile[0] = 0;
    SalamanderExeFile[0] = 0;
    ExportFile[0] = 0;
    ExportAsTextArchiveFile[0] = 0;
    FullSourceFile[0] = 0;
    FullTargetFile[0] = 0;
    FullIncludeFile[0] = 0;
    FullSalMenuFile[0] = 0;
    FullIgnoreLstFile[0] = 0;
    FullCheckLstFile[0] = 0;
    FullSalamanderExeFile[0] = 0;
    FullExportFile[0] = 0;

    OpenStringTables = FALSE;
    OpenMenuTables = FALSE;
    OpenDialogTables = FALSE;
    SelectedTreeItem = 0;

    StrData.DestroyMembers();
    MenuData.DestroyMembers();
    DlgData.DestroyMembers();
    CleanTranslationStates();
    SalMenuSections.DestroyMembers();
    IgnoreLstItems.DestroyMembers();
    CheckLstItems.DestroyMembers();
    Bookmarks.DestroyMembers();
    MenuPreview.DestroyMembers();
}

BOOL CData::Save()
{
    char errtext[5000];
    BOOL ret = TRUE;

    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    CleanTranslationStates();

    char buff[MAX_PATH];
    lstrcpy(buff, Data.FullTargetFile);
    lstrcpy(strrchr(buff, '.') + 1, "tmp");
    if (CopyFile(Data.FullTargetFile, buff, FALSE))
    {
        HANDLE hUpdate = BeginUpdateResource(buff, FALSE);
        if (hUpdate != NULL)
        {
            if (ret)
                ret &= SaveStrings(hUpdate);
            if (ret)
                ret &= SaveMenus(hUpdate);
            if (ret)
                ret &= SaveDialogs(hUpdate);
            if (ret)
                ret &= SaveSLGSignature(hUpdate);
            if (ret)
                ret &= SaveModuleName(hUpdate);
            if (ret)
                ret &= VersionInfo.UpdateResource(hUpdate, VS_VERSION_INFO);
            if (ret)
                ret &= EndUpdateResource(hUpdate, FALSE);

            if (ret)
                ret &= StringsAddTranslationStates();
            if (ret)
                ret &= MenusAddTranslationStates();
            if (ret)
                ret &= DialogsAddTranslationStates();
        }
        if (!ret)
        {
            DWORD err = GetLastError();
            sprintf_s(errtext, "Error updating resource file %s.\n%s", Data.FullTargetFile, GetErrorText(err));
            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        }
        if (ret)
        {
            lstrcpy(buff, Data.FullTargetFile);
            lstrcpy(strrchr(buff, '.') + 1, "bak");
            if (!DeleteFile(buff) &&
                GetFileAttributes(buff) != INVALID_FILE_ATTRIBUTES) // neselhalo to kvuli neexistenci .bak souboru
            {                                                       // pokud selze mazani (asi je .bak otevreny v bezicim Salamanderovi z bin adresare)
                // zkusime prejmenovani do "xxxx (2).bak", "xxxx (3).bak", atd.
                char buff2[MAX_PATH + 20];
                lstrcpyn(buff2, buff, MAX_PATH);
                char* num = strrchr(buff2, '.');
                if (num != NULL)
                {
                    for (int i = 2;; i++)
                    {
                        sprintf_s(num, _countof(buff2) - (num - buff2), " (%d).bak", i);
                        if (MoveFile(buff, buff2))
                            break;
                        else
                        {
                            DWORD err = GetLastError();
                            if (err != ERROR_FILE_EXISTS && err != ERROR_ALREADY_EXISTS)
                                break;
                        }
                    }
                }
            }
            if (MoveFile(Data.FullTargetFile, buff))
            {
                lstrcpy(buff, Data.FullTargetFile);
                lstrcpy(strrchr(buff, '.') + 1, "tmp");
                if (!MoveFile(buff, Data.FullTargetFile))
                {
                    ret = FALSE;
                    DWORD err = GetLastError();
                    sprintf_s(errtext, "Error moving file %s to %s.\n%s", buff, Data.FullTargetFile, GetErrorText(err));
                    MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                }
            }
            else
            {
                ret = FALSE;
                DWORD err = GetLastError();
                sprintf_s(errtext, "Error moving file %s to %s.\n%s", Data.FullTargetFile, buff, GetErrorText(err));
                MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            }
        }
    }
    else
    {
        ret = FALSE;
        DWORD err = GetLastError();
        sprintf_s(errtext, "Error copying file %s to %s.\n%s", Data.FullTargetFile, buff, GetErrorText(err));
        MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
    }
    if (ret)
    {
        SetDirty(FALSE);
    }
    SetCursor(hOldCursor);
    return ret;
}

BOOL CALLBACK EnumResLangProc(HANDLE hModule, LPCTSTR lpszType, LPCTSTR lpszName,
                              WORD wIDLanguage, LONG_PTR lParam)
{
    if (*(DWORD*)lParam != 0xFFFFFFFF)
    {
        // resource obsahuje vice jazyku, vyhlasime chybu
        *(DWORD*)lParam = 0xFFFFFFFF;
        return FALSE;
    }
    *(WORD*)lParam = wIDLanguage;
    return TRUE; // chceme dalsi jazyky, abychom upozornili na chybu
}

BOOL CALLBACK EnumResNameProc(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LPARAM lParam)
{
    char errtext[3000];
    CData* data = (CData*)lParam;
    if ((((DWORD)lpszName) & 0xFFFF0000) != 0 && !data->MUIMode) // v MUIMode potrebujeme nacitat i silene MS resourcy pouzivajici stringy misto ID
    {
        sprintf_s(errtext, "Non integer identifier: %s.", lpszName);
        MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        data->EnumReturn = FALSE;
        return FALSE;
    }

    DWORD langID = 0xFFFFFFFF; // pred volanim enumerace musime resetnout
    if (!EnumResourceLanguages(data->HTranModule, lpszType, lpszName, (ENUMRESLANGPROC)EnumResLangProc, (LONG_PTR)&langID))
    {
        DWORD err = GetLastError();
        sprintf_s(errtext, "EnumResourceLanguages() failed for resource type: %d name: %d\n%s",
                  (WORD)(UINT_PTR)lpszType, (WORD)(UINT_PTR)lpszName, GetErrorText(err));
        MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        data->EnumReturn = FALSE;
        return FALSE;
    }
    if (langID == 0xFFFFFFFF && data->MUIMode)
        return FALSE;
    if (langID == 0xFFFFFFFF)
    {
        sprintf_s(errtext, "Multiple language resources for resource type: %d name: %d.",
                  (WORD)(UINT_PTR)lpszType, (WORD)(UINT_PTR)lpszName);
        MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        data->EnumReturn = FALSE;
        return FALSE;
    }

    switch ((DWORD)lpszType)
    {
    case (DWORD)RT_DIALOG:
    {
        CDialogData* dlgData = new CDialogData;
        if (dlgData == NULL)
        {
            TRACE_E("Low memory");
            data->EnumReturn = FALSE;
            return FALSE;
        }
        dlgData->ID = (WORD)(((DWORD)lpszName) & 0x0000FFFF);
        if (data->MUIMode)
            dlgData->ID = (WORD)data->MUIDialogID++;
        dlgData->TLangID = (WORD)langID;
        BOOL added = FALSE;

        HRSRC oHrsrc = FindResource(hModule, lpszName, RT_DIALOG);
        HRSRC tHrsrc = FindResource(data->HTranModule, lpszName, RT_DIALOG);
        if (oHrsrc != NULL && tHrsrc != NULL)
        {
            HGLOBAL oHglb = LoadResource(hModule, oHrsrc);
            HGLOBAL tHglb = LoadResource(data->HTranModule, tHrsrc);
            if (oHglb != NULL && tHglb != NULL)
            {
                WORD* oBuff = (WORD*)LockResource(oHglb);
                WORD* tBuff = (WORD*)LockResource(tHglb);
                if (oBuff != NULL && tBuff != NULL)
                {
                    added = dlgData->LoadDialog(oBuff, tBuff, &data->ShowStyleWarning,
                                                &data->ShowExStyleWarning, &data->ShowDlgEXWarning, data);
                }
                else
                {
                    sprintf_s(errtext, (oBuff == NULL) ? "LockResource failed on RT_DIALOG id:%d in original file." : "LockResource failed on RT_DIALOG id:%d in translated file.",
                              dlgData->ID);
                    MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                }
            }
            else
            {
                sprintf_s(errtext, (oHglb == NULL) ? "LoadResource failed on RT_DIALOG id:%d in original file." : "LoadResource failed on RT_DIALOG id:%d in translated file.",
                          dlgData->ID);
                MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            }
        }
        else
        {
            sprintf_s(errtext, (oHrsrc == NULL) ? "Cannot find RT_DIALOG id:%d in original file." : "Cannot find RT_DIALOG id:%d in translated file.",
                      dlgData->ID);
            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        }

        if (!added)
        {
            delete dlgData;
            data->EnumReturn = FALSE;
            return FALSE;
        }
        data->DlgData.Add(dlgData);
        if (!data->DlgData.IsGood())
        {
            TRACE_E("Low memory");
            delete dlgData;
            data->EnumReturn = FALSE;
            return FALSE;
        }
        break;
    }

    case (DWORD)RT_MENU:
    {
        CMenuData* menuData = new CMenuData;
        if (menuData == NULL)
        {
            TRACE_E("Low memory");
            data->EnumReturn = FALSE;
            return FALSE;
        }
        menuData->ID = (WORD)(((DWORD)lpszName) & 0x0000FFFF);
        if (data->MUIMode)
            menuData->ID = (WORD)data->MUIMenuID++;
        menuData->TLangID = (WORD)langID;
        BOOL added = FALSE;

        HRSRC oHrsrc = FindResource(hModule, lpszName, RT_MENU);
        HRSRC tHrsrc = FindResource(data->HTranModule, lpszName, RT_MENU);
        if (oHrsrc != NULL && tHrsrc != NULL)
        {
            HGLOBAL oHglb = LoadResource(hModule, oHrsrc);
            HGLOBAL tHglb = LoadResource(data->HTranModule, tHrsrc);
            if (oHglb != NULL && tHglb != NULL)
            {
                LPCSTR oBuff = (LPCSTR)LockResource(oHglb);
                LPCSTR tBuff = (LPCSTR)LockResource(tHglb);
                if (oBuff != NULL && tBuff != NULL)
                {
                    WORD oVersion = GET_WORD(oBuff);
                    WORD tVersion = GET_WORD(tBuff);
                    oBuff += sizeof(WORD);
                    tBuff += sizeof(WORD);
                    if (data->MUIMode && oVersion == 1 && tVersion == 1)
                    {
                        WORD oOffset = GET_WORD(oBuff);
                        WORD tOffset = GET_WORD(tBuff);
                        oBuff += sizeof(WORD) + oOffset;
                        tBuff += sizeof(WORD) + tOffset;
                        added = menuData->LoadMenuEx(oBuff, tBuff, data);
                    }
                    else
                    {
                        if (oVersion == 0 && tVersion == 0)
                        {
                            WORD oOffset = GET_WORD(oBuff);
                            WORD tOffset = GET_WORD(tBuff);
                            oBuff += sizeof(WORD) + oOffset;
                            tBuff += sizeof(WORD) + tOffset;
                            if (oOffset != 0 || tOffset != 0)
                            {
                                sprintf_s(errtext, "Non zero offsets are not supported");
                                MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                                data->EnumReturn = FALSE;
                                delete menuData;
                                return FALSE;
                            }
                            added = menuData->LoadMenu(oBuff, tBuff, data);
                        }
                        else
                        {
                            sprintf_s(errtext, "MENUEX templates are not supported");
                            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                            data->EnumReturn = FALSE;
                            delete menuData;
                            return FALSE;
                        }
                    }
                }
            }
        }
        else
        {
            sprintf_s(errtext, (oHrsrc == NULL) ? "Cannot find RT_MENU id:%d in original file." : "Cannot find RT_MENU id:%d in translated file.",
                      menuData->ID);
            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        }

        if (!added)
        {
            delete menuData;
            data->EnumReturn = FALSE;
            return FALSE;
        }
        data->MenuData.Add(menuData);
        if (!data->MenuData.IsGood())
        {
            TRACE_E("Low memory");
            delete menuData;
            data->EnumReturn = FALSE;
            return FALSE;
        }
        break;
    }

    case (DWORD)RT_STRING:
    {
        CStrData* strData = new CStrData;
        if (strData == NULL)
        {
            TRACE_E("Low memory");
            data->EnumReturn = FALSE;
            return FALSE;
        }
        strData->ID = (WORD)(((DWORD)lpszName) & 0x0000FFFF);
        if (data->MUIMode)
            strData->ID = (WORD)data->MUIStringID++;
        strData->TLangID = (WORD)langID;
        BOOL added = FALSE;

        HRSRC oHrsrc = FindResource(hModule, lpszName, RT_STRING);
        HRSRC tHrsrc = FindResource(data->HTranModule, lpszName, RT_STRING);
        if (oHrsrc != NULL && tHrsrc != NULL)
        {
            HGLOBAL oHglb = LoadResource(hModule, oHrsrc);
            HGLOBAL tHglb = LoadResource(data->HTranModule, tHrsrc);
            if (oHglb != NULL && tHglb != NULL)
            {
                LPVOID oBuff = LockResource(oHglb);
                LPVOID tBuff = LockResource(tHglb);
                if (oBuff != NULL && tBuff != NULL)
                {
                    added = strData->LoadStrings(strData->ID, (const wchar_t*)oBuff, (const wchar_t*)tBuff, data);
                }
            }
        }
        else
        {
            sprintf_s(errtext, (oHrsrc == NULL) ? "Cannot find RT_STRING id:%d in original file." : "Cannot find RT_STRING id:%d in translated file.",
                      strData->ID);
            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        }
        if (!added)
        {
            //TRACE_E("Low memory");
            delete strData;
            data->EnumReturn = FALSE;
            return FALSE;
        }
        data->StrData.Add(strData);
        if (!data->StrData.IsGood())
        {
            TRACE_E("Low memory");
            delete strData;
            data->EnumReturn = FALSE;
            return FALSE;
        }

        break;
    }

    default:
    {
        sprintf_s(errtext, "Unknown resource type : %s.", lpszType);
        MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        data->EnumReturn = FALSE;
        return FALSE;
    }
    }
    return TRUE;
}

BOOL CData::CheckIdentifiers(BOOL reportAsErrors)
{
    BOOL ret = TRUE;
    WORD lvIndex; // index v ListView
    int i, j;
    wchar_t buff[10000];
    CMessageTypeEnum msgType = reportAsErrors ? mteError : mteWarning;

    // prohledame dialogy
    for (i = 0; i < DlgData.Count; i++)
    {
        CDialogData* dialog = DlgData[i];
        int idIndex;
        if (!DataRH.GetIDIndex(dialog->ID, idIndex))
        {
            swprintf_s(buff, L"Dialog %hs has unknown identifier", DataRH.GetIdentifier(dialog->ID));
            OutWindow.AddLine(buff, msgType, rteDialog, dialog->ID, 0); // fokusne prvni control v dialogu (neni uplne koser, ale nam to staci)
        }
        lvIndex = 0;
        for (j = 0; j < dialog->Controls.Count; j++)
        {
            CControl* control = dialog->Controls[j];
            if (!control->ShowInLVWithControls(j))
                continue;

            if (j > 0 && !DataRH.GetIDIndex(control->ID, idIndex)) // na j==0 je titulek dialogu + ID dialogu, to se resi pred cyklem
            {
                swprintf_s(buff, L"Dialog %hs: Unknown identifier: %hs (%ls)", DataRH.GetIdentifier(dialog->ID),
                           DataRH.GetIdentifier(control->ID), control->TWindowName);
                OutWindow.AddLine(buff, msgType, rteDialog, dialog->ID, lvIndex);
            }
            lvIndex++;
        }
    }

    // prohledame menu
    for (i = 0; i < MenuData.Count; i++)
    {
        CMenuData* menu = MenuData[i];
        int idIndex;
        if (!DataRH.GetIDIndex(menu->ID, idIndex))
        {
            swprintf_s(buff, L"Menu %hs has unknown identifier", DataRH.GetIdentifier(menu->ID));
            OutWindow.AddLine(buff, msgType, rteMenu, menu->ID, 0); // fokusne prvni item v menu (neni uplne koser, ale nam to staci)
        }
        lvIndex = 0;
        for (j = 0; j < menu->Items.Count; j++)
        {
            CMenuItem* item = &menu->Items[j];
            if (wcslen(item->TString) == 0)
                continue;

            if ((item->Flags & MF_POPUP) == 0 && // popup nema ID
                !DataRH.GetIDIndex(item->ID, idIndex))
            {
                swprintf_s(buff, L"Menu %hs: Unknown identifier %hs (%ls)", DataRH.GetIdentifier(menu->ID),
                           DataRH.GetIdentifier(item->ID), item->TString);
                OutWindow.AddLine(buff, msgType, rteMenu, menu->ID, lvIndex);
            }
            lvIndex++;
        }
    }

    // prohledame strings
    for (i = 0; i < StrData.Count; i++)
    {
        CStrData* strData = StrData[i];
        lvIndex = 0;
        for (j = 0; j < 16; j++)
        {
            wchar_t* str = strData->TStrings[j];

            if (str == NULL || wcslen(str) == 0)
                continue;

            WORD id = (strData->ID - 1) << 4 | j;
            int idIndex;
            if (!DataRH.GetIDIndex(id, idIndex))
            {
                swprintf_s(buff, L"String: Unknown identifier %hs (%ls)", DataRH.GetIdentifier(id), str);
                OutWindow.AddLine(buff, msgType, rteString, strData->ID, lvIndex);
            }
            lvIndex++;
        }
    }
    return ret;
}

BOOL CData::EnumResources(HINSTANCE hSrcModule, HINSTANCE hDstModule, BOOL import)
{
    EnumReturn = TRUE;
    HTranModule = hDstModule;
    Importing = import;
    ShowStyleWarning = TRUE;
    ShowExStyleWarning = TRUE;
    ShowDlgEXWarning = TRUE;
    EnumResourceNames(hSrcModule, RT_DIALOG, EnumResNameProc, (LPARAM)this);
    if (EnumReturn)
        EnumResourceNames(hSrcModule, RT_MENU, EnumResNameProc, (LPARAM)this);
    if (EnumReturn)
        EnumResourceNames(hSrcModule, RT_STRING, EnumResNameProc, (LPARAM)this);
    HTranModule = NULL;

    // projdeme prelozitelne prvky, zda maji prirazeny symbolicke identifikatory
    if (!MUIMode)
        CheckIdentifiers(FALSE);

    return EnumReturn;
}

BOOL CData::Load(const char* original, const char* translated, BOOL import)
{
    char errtext[3000];
    BOOL ret = TRUE;

    {
        wchar_t buff[2 * MAX_PATH];

        if (!MUIMode) // bezne tento vystup zatucu, zbytecne to zdrzuje
        {
            if (MUIMode)
            {
                swprintf_s(buff, L"MUIDialogID: %d", MUIDialogID);
                OutWindow.AddLine(buff, mteInfo);
            }
            swprintf_s(buff, L"Reading ORIGINAL file: %hs", original);
            OutWindow.AddLine(buff, mteInfo);
            swprintf_s(buff, L"Reading TRANSLATED file: %hs", translated);
            OutWindow.AddLine(buff, mteInfo);
        }
    }

    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    HINSTANCE hSrcModule = LoadLibraryEx(original, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (hSrcModule != NULL)
    {
        HINSTANCE hDstModule = LoadLibraryEx(translated, NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (hDstModule != NULL)
        {
            ret = EnumResources(hSrcModule, hDstModule, import);

            if (ret /*&& !import*/ && !MUIMode) // signaturu chceme take importovat; v MUIMode naopak nesmime, MS nic takoveh v DLL nemaji
            {
                ret &= LoadSLGSignature(hDstModule);
                if (!ret)
                {
                    sprintf_s(errtext, "Error opening file %s.\nThe valid SLG signature cannot be found.", translated);
                    MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                }
            }

            FreeLibrary(hDstModule);
        }
        else
        {
            DWORD err = GetLastError();
            sprintf_s(errtext, "Error opening file %s.\n%s", translated, GetErrorText(err));
            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            ret = FALSE;
        }

        FreeLibrary(hSrcModule);
    }
    else
    {
        DWORD err = GetLastError();
        sprintf_s(errtext, "Error opening file %s.\n%s", original, GetErrorText(err));
        MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        ret = FALSE;
    }

    SetCursor(hOldCursor);
    return ret;
}

BOOL CData::LoadSLGSignature(HINSTANCE hModule)
{
    BOOL ret = TRUE;
    if (ret)
        ret &= VersionInfo.ReadResource(hModule, VS_VERSION_INFO);
    BYTE* buffer = NULL;
    DWORD size;

    if (ret)
        ret &= VersionInfo.QueryString("\\StringFileInfo\\040904b0\\SLGAuthor", SLGSignature.Author, 500);
    if (ret)
        ret &= VersionInfo.QueryString("\\StringFileInfo\\040904b0\\SLGWeb", SLGSignature.Web, 500);
    if (ret)
        ret &= VersionInfo.QueryString("\\StringFileInfo\\040904b0\\SLGComment", SLGSignature.Comment, 500);
    if (ret)
    {
        SLGSignature.HelpDirExist = VersionInfo.QueryString("\\StringFileInfo\\040904b0\\SLGHelpDir", SLGSignature.HelpDir, 100);
        SLGSignature.SLGIncompleteExist = VersionInfo.QueryString("\\StringFileInfo\\040904b0\\SLGIncomplete", SLGSignature.SLGIncomplete, 200);
        ret &= VersionInfo.QueryValue("\\VarFileInfo\\Translation", &buffer, &size);
    }
    if (ret)
        SLGSignature.LanguageID = *((WORD*)buffer);
    if (ret && !VersionInfo.QueryString("\\StringFileInfo\\040904b0\\SLGCRCofImpSLT", SLGSignature.CRCofImpSLT, 100))
        lstrcpyW(SLGSignature.CRCofImpSLT, L"not found");

    /*
  ver.WriteResourceToFile(hModule, VS_VERSION_INFO, "D:\\Zumpa\\translator\\prj2\\original.bin");
  ver.WriteResource("D:\\Zumpa\\translator\\prj2\\out.slg", VS_VERSION_INFO);
  HMODULE hTarget = LoadLibrary("D:\\Zumpa\\translator\\prj2\\out.slg");
  ver.WriteResourceToFile(hTarget, VS_VERSION_INFO, "D:\\Zumpa\\translator\\prj2\\new.bin");
  FreeLibrary(hTarget);
  */
    return ret;
}

BOOL CData::SaveSLGSignature(HANDLE hUpdateRes)
{
    BOOL ret = TRUE;
    BYTE* buffer = NULL;
    DWORD size;

    if (ret)
        ret &= VersionInfo.SetString("\\StringFileInfo\\040904b0\\SLGAuthor", SLGSignature.Author);
    if (ret)
        ret &= VersionInfo.SetString("\\StringFileInfo\\040904b0\\SLGWeb", SLGSignature.Web);
    if (ret)
        ret &= VersionInfo.SetString("\\StringFileInfo\\040904b0\\SLGComment", SLGSignature.Comment);
    if (ret && SLGSignature.HelpDirExist)
        ret &= VersionInfo.SetString("\\StringFileInfo\\040904b0\\SLGHelpDir", SLGSignature.HelpDir);
    if (ret && SLGSignature.SLGIncompleteExist)
        ret &= VersionInfo.SetString("\\StringFileInfo\\040904b0\\SLGIncomplete", SLGSignature.SLGIncomplete);
    if (ret && wcscmp(SLGSignature.CRCofImpSLT, L"not found") != 0)
        ret &= VersionInfo.SetString("\\StringFileInfo\\040904b0\\SLGCRCofImpSLT", SLGSignature.CRCofImpSLT);
    if (ret)
        ret &= VersionInfo.QueryValue("\\VarFileInfo\\Translation", &buffer, &size);
    if (ret)
        *((WORD*)buffer) = SLGSignature.LanguageID;

    return ret;
}

BOOL CData::SaveModuleName(HANDLE hUpdateRes)
{
    BOOL ret = FALSE;
    char* name = strrchr(TargetFile, '\\');
    if (name != NULL)
    {
        name++;
        wchar_t buff[MAX_PATH];
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, buff, MAX_PATH);
        _wcsupr_s(buff);
        wchar_t* ext = wcsrchr(buff, '.');
        if (ext != NULL)
        {
            ret = TRUE;
            if (ret)
                ret &= VersionInfo.SetString("\\StringFileInfo\\040904b0\\OriginalFilename", buff);
            *ext = 0;
            if (ret)
                ret &= VersionInfo.SetString("\\StringFileInfo\\040904b0\\InternalName", buff);
        }
        else
            TRACE_EW(L"CData::SaveModuleName(): unexpected module name: " << buff);
    }
    else
        TRACE_E("CData::SaveModuleName(): unexpected TargetFile: " << TargetFile);

    return ret;
}

void CData::SetDirty(BOOL dirty)
{
    BOOL oldDirty = Dirty;
    Dirty = dirty;
    if (dirty != oldDirty)
        FrameWindow.SetTitle();
}

BOOL CData::Import(const char* project, BOOL trlPropOnly, BOOL onlyDlgLayouts)
{
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    OutWindow.EnablePaint(FALSE);
    OutWindow.Clear();
    char buf[MAX_PATH + 1000];
    wchar_t buff[10000];
    swprintf_s(buff, L"Importing old translation...");
    OutWindow.AddLine(buff, mteInfo);

    if (!trlPropOnly && SLGSignature.IsSLTDataChanged())
    {
        sprintf_s(buf, "You have made changes to this module in Translator since last "
                       "import from SLT file, export to SLT file, or creating this language module. "
                       "Now you are trying to import from %s. Import will overwrite all these "
                       "changes.\n\n"
                       "Do you really want to continue with import and so lose your changes?\n\n"
                       "We recommend to backup current translation. Just export "
                       "SLT file for this module, use menu File / Export Translation as Text File "
                       "(Ctrl+D key).",
                  project);
        if (MessageBox(GetMsgParent(), buf, FRAMEWINDOW_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
        {
            swprintf_s(buff, L"Importing old translation was cancelled.");
            OutWindow.AddLine(buff, mteError);
            OutWindow.EnablePaint(TRUE);
            SetCursor(hOldCursor);
            return FALSE;
        }
    }

    CData oldData;
    if (oldData.LoadProject(project) && oldData.Load(oldData.FullSourceFile, oldData.FullTargetFile, TRUE))
    {
        int importedTotal = 0;

        if (!onlyDlgLayouts)
        {
            swprintf_s(buff, L"Importing SLG Signature...");
            OutWindow.AddLine(buff, mteInfo);

            SLGSignature.LanguageID = oldData.SLGSignature.LanguageID;
            lstrcpyW(SLGSignature.Author, oldData.SLGSignature.Author);
            lstrcpyW(SLGSignature.Web, oldData.SLGSignature.Web);
            lstrcpyW(SLGSignature.Comment, oldData.SLGSignature.Comment);
        }
        if (!trlPropOnly)                                      // importem (dela se obvykle jen ze starsi verze) vznikne nova verze, kterou jsme neimportovali z SLT, proto:
            SLGSignature.SetCRCofImpSLTIfFound((wchar_t*)L""); // zmenena verze, ktera nebyla importena/exportena do SLT
        else
            SLGSignature.SLTDataChanged(); // import jen SLG properties bereme stejne jako rucni editaci SLG
        if (!onlyDlgLayouts &&
            SLGSignature.HelpDirExist == oldData.SLGSignature.HelpDirExist) // pri importu translation properties ze salamand.atp do pluginu nesmime prenest HelpDir
        {
            lstrcpyW(SLGSignature.HelpDir, oldData.SLGSignature.HelpDir);
        }
        if (!onlyDlgLayouts &&
            SLGSignature.SLGIncompleteExist == oldData.SLGSignature.SLGIncompleteExist) // pri importu translation properties ze salamand.atp do pluginu nesmime prenest SLGIncomplete
        {
            lstrcpyW(SLGSignature.SLGIncomplete, oldData.SLGSignature.SLGIncomplete);
        }
        if (!trlPropOnly)
        {
            swprintf_s(buff, L"Importing Dialogs...");
            OutWindow.AddLine(buff, mteInfo);

            CleanRelayout();

            // import sekce dialogs
            int dlgTotalCount = 0;
            int dlgTranslatedCount = 0;
            int i;
            for (i = 0; i < DlgData.Count; i++)
            {
                // zkusime nase ID najit v importovanych datech
                int index = oldData.FindDialogData(DlgData[i]->ID);
                if (index != -1)
                {
                    // zjistime, zda se stara anglicka verze zmenila z hlediska layoutu proti nove anglicke verzi
                    BOOL orgLayoutChanged = DlgData[i]->DoesLayoutChanged2(oldData.DlgData[index]);
                    if (orgLayoutChanged)
                    {
                        AddRelayout(DlgData[i]->ID);
                        //          swprintf_s(buff, L"Dialog layout changed for %hs", DataRH.GetIdentifier(DlgData[i]->ID));
                        //          OutWindow.AddLine(buff, mteInfo, rteDialog, DlgData[i]->ID);
                    }
                    if (onlyDlgLayouts)
                    {
                        if (orgLayoutChanged) // layouty anglickych verzi musi byt shodne, importi se v ramci jedne verze
                        {
                            swprintf_s(buff, L"Original dialog layouts must be the same, it's not true for %hs", DataRH.GetIdentifier(DlgData[i]->ID));
                            OutWindow.AddLine(buff, mteError, rteDialog, DlgData[i]->ID);
                            return FALSE;
                        }
                        CDialogData* n = DlgData[i];
                        CDialogData* o = oldData.DlgData[index];
                        n->TX = o->TX;
                        n->TY = o->TY;
                        n->TCX = o->TCX;
                        n->TCY = o->TCY;
                    }
                    else
                    {
                        if (!orgLayoutChanged)
                        {
                            // anglicka verze se nezmenila, muzeme prenest stary layout 1:1;
                            // ignorovane rozdily stylu (napr. u editboxu ES_AUTOHSCROLL) se resi zmenou stylu
                            // na novy primo ve stare verzi dat, viz DoesLayoutChanged2 (jinak by se po teto
                            // kopii pouzival stary styl a to prirozene nechceme)
                            DlgData[i]->LoadFrom(oldData.DlgData[index], TRUE);
                        }
                    }
                    for (int j = 0; j < DlgData[i]->Controls.Count; j++)
                    {
                        // zkusime nase ID najit v importovanych datech
                        int ctrlIndex = oldData.DlgData[index]->FindControlIndex(DlgData[i]->Controls[j]->ID, j == 0);
                        if (ctrlIndex != -1)
                        {
                            if (onlyDlgLayouts) // prenos rozmeru controlu (pouzito pro import layoutu dialogu z ceske do slovenske verze 2.55 po prechodu na W2K look dialogu a kompletnim rozbiti layoutu dialogu)
                            {
                                CControl* n = DlgData[i]->Controls[j];
                                CControl* o = oldData.DlgData[index]->Controls[ctrlIndex];
                                n->TX = o->TX;
                                n->TY = o->TY;
                                n->TCX = o->TCX;
                                n->TCY = o->TCY;
                            }
                            else
                            {
                                wchar_t* newOriginal = DlgData[i]->Controls[j]->OWindowName;
                                wchar_t* oldOriginal = oldData.DlgData[index]->Controls[ctrlIndex]->OWindowName;
                                if (newOriginal != NULL && oldOriginal != NULL)
                                {
                                    if (HIWORD(newOriginal) == 0x0000)
                                    {
                                        // misto retezce je v nazvu ciselna hodnota
                                        if (newOriginal == oldOriginal)
                                        {
                                            DlgData[i]->Controls[j]->TWindowName = oldData.DlgData[index]->Controls[ctrlIndex]->TWindowName;
                                            // stav nastavime jako prelozeny
                                            DlgData[i]->Controls[j]->State = oldData.DlgData[index]->Controls[ctrlIndex]->State;
                                            importedTotal++;
                                        }
                                    }
                                    else
                                    {
                                        // pokud je originalni retezec stejny jako originalni v importovane verzi
                                        if (wcscmp(newOriginal, oldOriginal) == 0)
                                        {
                                            // prekopirujeme z importu prelozen retezec
                                            int len = wcslen(oldData.DlgData[index]->Controls[ctrlIndex]->TWindowName) + 1;
                                            wchar_t* s = (wchar_t*)malloc(len * sizeof(wchar_t));
                                            wcscpy_s(s, len, oldData.DlgData[index]->Controls[ctrlIndex]->TWindowName);
                                            free(DlgData[i]->Controls[j]->TWindowName);
                                            DlgData[i]->Controls[j]->TWindowName = s;
                                            // stav nastavime jako prelozeny
                                            DlgData[i]->Controls[j]->State = oldData.DlgData[index]->Controls[ctrlIndex]->State;
                                            importedTotal++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!onlyDlgLayouts)
            {
                swprintf_s(buff, L"Importing Menus...");
                OutWindow.AddLine(buff, mteInfo);

                // import sekce menus
                for (i = 0; i < MenuData.Count; i++)
                {
                    // zkusime nase ID najit v importovanych datech
                    int index = oldData.FindMenuData(MenuData[i]->ID);
                    if (index != -1)
                    {
                        for (int j = 0; j < MenuData[i]->Items.Count; j++)
                        {
                            // zkusime nase ID najit v importovanych datech
                            int itemIndex = oldData.MenuData[i]->FindItemIndex(MenuData[i]->Items[j].ID);
                            if (MenuData[i]->Items[j].ID == 0) // popup jedeme podle indexu
                            {
                                itemIndex = j < oldData.MenuData[index]->Items.Count ? j : -1;
                                if (itemIndex == -1 ||
                                    *MenuData[i]->Items[j].OString == 0 || *oldData.MenuData[index]->Items[itemIndex].OString == 0 ||
                                    wcscmp(MenuData[i]->Items[j].OString, oldData.MenuData[index]->Items[itemIndex].OString) != 0)
                                { // pokud jsme ho nenasli podle indexu, zkusime ho dohledat hrubou silou
                                    for (int popupIndex = 0; popupIndex < oldData.MenuData[index]->Items.Count; popupIndex++)
                                    {
                                        if (oldData.MenuData[index]->Items[popupIndex].ID == 0) // popup
                                        {
                                            if (*MenuData[i]->Items[j].OString != 0 && *oldData.MenuData[index]->Items[popupIndex].OString != 0 &&
                                                wcscmp(MenuData[i]->Items[j].OString, oldData.MenuData[index]->Items[popupIndex].OString) == 0)
                                            {
                                                itemIndex = popupIndex;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            if (itemIndex != -1)
                            {
                                if (*MenuData[i]->Items[j].OString != 0 && *oldData.MenuData[index]->Items[itemIndex].OString != 0)
                                {
                                    // pokud je originalni retezec stejny jako originalni v importovane verzi
                                    if (wcscmp(MenuData[i]->Items[j].OString, oldData.MenuData[index]->Items[itemIndex].OString) == 0)
                                    {
                                        // prekopirujeme z importu prelozen retezec
                                        int len = wcslen(oldData.MenuData[index]->Items[itemIndex].TString) + 1;
                                        wchar_t* s = (wchar_t*)malloc(len * sizeof(wchar_t));
                                        wcscpy_s(s, len, oldData.MenuData[index]->Items[itemIndex].TString);
                                        free(MenuData[i]->Items[j].TString);
                                        MenuData[i]->Items[j].TString = s;
                                        // stav nastavime jako prelozeny
                                        MenuData[i]->Items[j].State = oldData.MenuData[index]->Items[itemIndex].State;
                                        importedTotal++;
                                    }
                                }
                            }
                        }
                    }
                }

                swprintf_s(buff, L"Importing String Tables...");
                OutWindow.AddLine(buff, mteInfo);

                // import sekce strings
                for (i = 0; i < StrData.Count; i++)
                {
                    // zkusime nase ID najit v importovanych datech
                    int index = oldData.FindStrData(StrData[i]->ID);
                    if (index != -1)
                    {
                        for (int j = 0; j < 16; j++)
                        {
                            if (StrData[i]->TStrings[j] != NULL && oldData.StrData[index]->TStrings[j] != NULL)
                            {
                                // pokud je originalni retezec stejny jako originalni v importovane verzi
                                if (wcscmp(StrData[i]->OStrings[j], oldData.StrData[index]->OStrings[j]) == 0)
                                {
                                    // prekopirujeme z importu prelozen retezec
                                    int len = wcslen(oldData.StrData[index]->TStrings[j]) + 1;
                                    wchar_t* s = (wchar_t*)malloc(len * sizeof(wchar_t));
                                    wcscpy_s(s, len, oldData.StrData[index]->TStrings[j]);
                                    free(StrData[i]->TStrings[j]);
                                    StrData[i]->TStrings[j] = s;
                                    // stav nastavime jako prelozeny
                                    StrData[i]->TState[j] = oldData.StrData[index]->TState[j];
                                    importedTotal++;
                                }
                            }
                        }
                    }
                }

                swprintf_s(buff, L"Totally imported texts: %d", importedTotal);
                if (Relayout.Count > 0)
                {
                    SWPrintFToEnd_s(buff, L", dialogs for relayout: %d", Relayout.Count);
                }
                OutWindow.AddLine(buff, mteSummary);
            }
        }

        Data.SetDirty();
        Data.UpdateAllNodes(); // nastavime translated stavy na treeview
        Data.UpdateTexts();    // update list view v okne Texts

        OutWindow.EnablePaint(TRUE);
        SetCursor(hOldCursor);
        return TRUE;
    }
    else
    {
        swprintf_s(buff, L"Importing old translation FAILED.");
        OutWindow.AddLine(buff, mteError);
    }
    OutWindow.EnablePaint(TRUE);
    SetCursor(hOldCursor);
    return FALSE;
}

void RemoveAmpersands(wchar_t* buff)
{
    wchar_t* p = buff;
    while (*p != 0)
    {
        if (*p == L'&')
        {
            if (*(p + 1) != L'&')
                memmove(p, p + 1, wcslen(p) * sizeof(wchar_t));
            else
                p += 2;
        }
        else
            p++;
    }
}

// postrili ampersandy, \n, \t

void RemoveGarbage(wchar_t* buff)
{
    wchar_t* p = buff;
    while (*p != 0)
    {
        if (*p == L'&')
        {
            if (*(p + 1) == L'&')
                p++;
            memmove(p, p + 1, lstrlenW(p) * sizeof(wchar_t));
        }
        else
        {
            if (*p == L'\\')
            {
                if (*(p + 1) == L'n' || *(p + 1) == L'r' || *(p + 1) == L't')
                {
                    *p = L' ';
                    memmove(p + 1, p + 2, lstrlenW(p + 1) * sizeof(wchar_t));
                }
            }
            p++;
        }
    }
}

char* StripWS(char* str)
{
    char* s = str;
    while (*s != 0 && *s <= ' ')
        s++;
    char* e = s + strlen(s);
    while (e > s && *(e - 1) <= ' ')
        e--;
    if (str < s)
        memmove(str, s, e - s);
    str[e - s] = 0;
    return str;
}

BOOL CData::Export(const char* fileName)
{
    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                        CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    BOOL ret = TRUE;

    if (ret)
        ret &= WriteUnicodeBOM(hFile);

    wchar_t buff[10000];

    int i;
    for (i = 0; i < DlgData.Count; i++)
    {
        for (int j = 0; j < DlgData[i]->Controls.Count; j++)
        {
            CControl* control = DlgData[i]->Controls[j];

            if (HIWORD(control->TWindowName) != 0x0000 && IsTranslatableControl(control->TWindowName))
            {
                lstrcpynW(buff, control->TWindowName, 10000);
                RemoveGarbage(buff);
                if (ret)
                    ret &= WriteUnicodeTextLine(hFile, buff);
            }
        }
    }

    for (i = 0; i < MenuData.Count; i++)
    {
        for (int j = 0; j < MenuData[i]->Items.Count; j++)
        {
            CMenuItem* item = &MenuData[i]->Items[j];

            if (wcslen(item->TString) > 0)
            {
                lstrcpynW(buff, item->TString, 10000);
                RemoveGarbage(buff);
                if (ret)
                    ret &= WriteUnicodeTextLine(hFile, buff);
            }
        }
    }

    for (i = 0; i < StrData.Count; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            if (StrData[i]->TStrings[j] != NULL)
            {
                lstrcpynW(buff, StrData[i]->TStrings[j], 10000);
                RemoveGarbage(buff);
                if (ret)
                    ret &= WriteUnicodeTextLine(hFile, buff);
            }
        }
    }

    HANDLES(CloseHandle(hFile));
    return ret;
}

int GetRootPath(char* root, const char* path);

BOOL CData::ExportAsTextArchive(const char* fileName, BOOL withoutVerInfo)
{
    char buf[MAX_PATH + 1000];

    OutWindow.Clear();
    wchar_t outputBuff[10000];
    swprintf_s(outputBuff, L"Exporting translation to SLT text archive...");
    OutWindow.AddLine(outputBuff, mteInfo);

    /* strucne schema pro nasledujici podminku:
  SLG:
    -EN (nakompilovana anglicka verze)
      -SLT neexistuje                               OK
      -SLT existuje                                 dotaz (prepis neznameho souboru)
    -po importu/exportu do SLT
      -SLT neexistuje                               OK
      -SLT existuje
        -shoda CRC s SLT odkud se imp/exportilo     OK (import a hned export nebo opakovany export = zadna zmena SLT)
        -neshoda CRC s SLT odkud se imp/exportilo   dotaz (jine SLT, muze jit o aktualizovanou podobu, kterou bysme takhle znicili)
    -po zmene
      -SLT neexistuje
        -neimportilo se z SLT                       ok
        -importilo se z SLT                         dotaz (exporti se nova verze SLT, ktera by se mela archivovat, ovsem rucni prepis archivu muze znamenat ztratu dat, je potreba rucni merge s archivem)
      -SLT existuje
        -neimportilo se z SLT                       dotaz (prepis neznameho souboru)
        -shoda CRC s SLT odkud se importilo         OK
        -neshoda CRC s SLT odkud se importilo       dotaz (jine SLT, muze jit o aktualizovanou podobu, kterou bysme takhle znicili, je potreba prejmenovat SLT, provest export do noveho SLT, a pak rucni merge obou verzi SLT)
*/
    if (!withoutVerInfo)
    {
        if (GetFileAttributes(fileName) != INVALID_FILE_ATTRIBUTES) // cilovy SLT soubor jiz existuje
        {
            int res = IDYES;
            if (wcscmp(SLGSignature.CRCofImpSLT, L"none") == 0) // EN verze
            {
                sprintf_s(buf, "Target file already exists. Nothing is known about this file. Target file: %s.\n\n"
                               "Do you really want to overwrite this file?\n\n"
                               "We recommend to export SLT file for this module to filename with some suffix "
                               "(e.g. \"_cur\"), use menu File / Export Translation as Text File (Ctrl+D key). "
                               "Then compare exported SLT file with the existing SLT file. "
                               "If changes in both files are relevant, you can merge them and then import "
                               "resulting SLT file.",
                          fileName);
                res = MessageBox(GetMsgParent(), buf, FRAMEWINDOW_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
            }
            else
            {
                if (SLGSignature.IsSLTDataAfterImportOrExport()) // tesne po exportu do SLT nebo importu z SLT
                {
                    DWORD existingFileCRC;
                    if (!GetFileCRC(fileName, &existingFileCRC))
                        return FALSE;
                    else
                    {
                        wchar_t crcTxt[50];
                        swprintf_s(crcTxt, L"%08X SLT", existingFileCRC);
                        if (wcscmp(SLGSignature.CRCofImpSLT, crcTxt) == 0) // shoda CRC existujiciho SLT souboru (prepisovaneho) a CRC SLT souboru, ze ktereho se naposledy importilo nebo do ktereho se naposledy exportilo z tohoto SLG souboru: budeme predpokladat, ze jde o stejne soubory a tise provedeme prepis existujiciho SLT souboru, nemelo by dojit k jeho zmene, protoze SLG se od importu/exportu nezmenilo
                        {
                        }
                        else // existujici SLT soubor ma jine CRC nez mame ulozeno v SLG => nejspis jde o aktualizovany SLT soubor, ktery nemuzeme jen tak prepsat
                        {
                            sprintf_s(buf, "Target file already exists and differs from the SLT file from which you "
                                           "have imported translation or to which you have exported translation. It is likely to "
                                           "be updated, so we do not recommend to overwrite it. Target file: %s.\n\n"
                                           "Do you really want to overwrite this file?\n\n"
                                           "We recommend to export SLT file for this module to filename with some suffix "
                                           "(e.g. \"_cur\"), use menu File / Export Translation as Text File (Ctrl+D key). "
                                           "Then compare exported SLT file with the existing SLT file. "
                                           "If changes in both files are relevant, you can merge them and then import "
                                           "resulting SLT file.",
                                      fileName);
                            res = MessageBox(GetMsgParent(), buf, FRAMEWINDOW_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                        }
                    }
                }
                else
                {
                    if (SLGSignature.IsSLTDataChanged()) // po zmene
                    {
                        if (wcscmp(SLGSignature.CRCofImpSLT, L"") == 0 ||   // zmenena verze, ktera nebyla importena/exportena do SLT
                            wcscmp(SLGSignature.CRCofImpSLT, L"none") == 0) // EN verze
                        {                                                   // neimportili/neexportili jsme z/do SLT
                            sprintf_s(buf, "Target file already exists. Nothing is known about this file. Target file: %s.\n\n"
                                           "Do you really want to overwrite this file?\n\n"
                                           "We recommend to export SLT file for this module to filename with some suffix "
                                           "(e.g. \"_cur\"), use menu File / Export Translation as Text File (Ctrl+D key). "
                                           "Then compare exported SLT file with the existing SLT file. "
                                           "If changes in both files are relevant, you can merge them and then import "
                                           "resulting SLT file.",
                                      fileName);
                            res = MessageBox(GetMsgParent(), buf, FRAMEWINDOW_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                        }
                        else
                        {
                            if (wcscmp(SLGSignature.CRCofImpSLT, L"not found") != 0)
                            {
                                DWORD existingFileCRC;
                                if (!GetFileCRC(fileName, &existingFileCRC))
                                    return FALSE;
                                else
                                {
                                    wchar_t crcTxt[50];
                                    swprintf_s(crcTxt, L"%08X", existingFileCRC);
                                    if (wcscmp(SLGSignature.CRCofImpSLT, crcTxt) == 0) // shoda CRC existujiciho SLT souboru (prepisovaneho) a CRC SLT souboru, ze ktereho se naposledy importilo nebo do ktereho se naposledy exportilo z tohoto SLG souboru: budeme predpokladat, ze jde o stejne soubory a tise provedeme prepis existujiciho SLT souboru
                                    {
                                    }
                                    else // existujici SLT soubor ma jine CRC nez mame ulozeno v SLG => nejspis jde o aktualizovany SLT soubor, ktery nemuzeme jen tak prepsat
                                    {
                                        sprintf_s(buf, "Target file already exists and differs from the SLT file from which you "
                                                       "have imported translation or to which you have exported translation. It is likely to "
                                                       "be updated, so we do not recommend to overwrite it. Target file: %s.\n\n"
                                                       "Do you really want to overwrite this file?\n\n"
                                                       "We recommend to export SLT file for this module to filename with some suffix "
                                                       "(e.g. \"_cur\"), use menu File / Export Translation as Text File (Ctrl+D key). "
                                                       "Then compare exported SLT file with the existing SLT file. "
                                                       "If changes in both files are relevant, you can merge them and then import "
                                                       "resulting SLT file.",
                                                  fileName);
                                        res = MessageBox(GetMsgParent(), buf, FRAMEWINDOW_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (res == IDNO)
            {
                swprintf_s(outputBuff, L"Exporting translation to SLT text archive was cancelled.");
                OutWindow.AddLine(outputBuff, mteError);
                return FALSE;
            }
        }
        else // cilovy SLT soubor neexistuje
        {
            if (SLGSignature.IsSLTDataChanged()) // po zmene
            {
                if (wcscmp(SLGSignature.CRCofImpSLT, L"") == 0 ||   // zmenena verze, ktera nebyla importena/exportena do SLT
                    wcscmp(SLGSignature.CRCofImpSLT, L"none") == 0) // EN verze
                {                                                   // neimportili/neexportili jsme z/do SLT
                }
                else
                {
                    if (wcscmp(SLGSignature.CRCofImpSLT, L"not found") != 0)
                    { // importili/exportili jsme z/do SLT
                        sprintf_s(buf, "You have imported translation from SLT file or exported translation "
                                       "to SLT file but now target SLT file does not exist, so it is not possible "
                                       "to check if it was not changed since your last import/export. We recommend "
                                       "to place original SLT file to target directory, so it could be checked. "
                                       "Target file: %s.\n\n"
                                       "Do you want to continue with export without checking for changes in original "
                                       "SLT file?",
                                  fileName);
                        if (MessageBox(GetMsgParent(), buf, FRAMEWINDOW_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
                        {
                            swprintf_s(outputBuff, L"Exporting translation to SLT text archive was cancelled.");
                            OutWindow.AddLine(outputBuff, mteError);
                            return FALSE;
                        }
                    }
                }
            }
        }
    }

    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                        CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        swprintf_s(outputBuff, L"Exporting translation to SLT text archive FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }

    BOOL ret = TRUE;
    wchar_t buff[10000];
    char buff2[10000];
    wchar_t buff3[10000];

    DWORD fileCRC32 = 0;
    if (ret)
        ret &= WriteUTF8BOM(hFile, &fileCRC32);

    if (!withoutVerInfo)
    {
        swprintf_s(buff, L"[EXPORTINFO]");
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        char* trlRoot = Data.ProjectFile + strlen(Data.ProjectFile);
        int skipBackslashes = 4;
        while (skipBackslashes >= 0)
        {
            skipBackslashes--;
            if (trlRoot > Data.ProjectFile)
                trlRoot--;
            while (trlRoot > Data.ProjectFile && *trlRoot != '\\')
                trlRoot--;
        }
        strcpy_s(buff2, trlRoot + (*trlRoot == '\\' ? 1 : 0));
        _strlwr_s(buff2);
        swprintf_s(buff, L"PROJECTNAME,\"%hs\"", buff2);
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        if (ret && !VersionInfo.QueryString("\\StringFileInfo\\040904b0\\FileVersion", buff3, 200))
            buff3[0] = 0;
        swprintf_s(buff, L"TEXTVERSION,\"%s\"", buff3);
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        VS_FIXEDFILEINFO* ffi;
        DWORD ffiSize;
        if (ret)
        {
            if (VersionInfo.QueryValue("\\", (BYTE**)&ffi, &ffiSize))
                sprintf_s(buff2, "%d,%d,%d,%d", HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS), HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS));
            else
                buff2[0] = 0;
        }
        swprintf_s(buff, L"VERSION,\"%hs\"", buff2);
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);

        if (ret)
            ret &= WriteUTF8TextLine(hFile, L"", &fileCRC32);
    }

    swprintf_s(buff, L"[TRANSLATION]");
    if (ret)
        ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
    swprintf_s(buff, L"LANGID,%d", Data.SLGSignature.LanguageID);
    if (ret)
        ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
    swprintf_s(buff, L"AUTHOR,\"%s\"", Data.SLGSignature.Author);
    if (ret)
        ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
    swprintf_s(buff, L"WEB,\"%s\"", Data.SLGSignature.Web);
    if (ret)
        ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
    swprintf_s(buff, L"COMMENT,\"%s\"", Data.SLGSignature.Comment);
    if (ret)
        ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
    if (Data.SLGSignature.HelpDirExist)
    {
        swprintf_s(buff, L"HELPDIR,\"%s\"", Data.SLGSignature.HelpDir);
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
    }
    if (Data.SLGSignature.SLGIncompleteExist)
    {
        swprintf_s(buff, L"SLGINCOMPLETE,\"%s\"", Data.SLGSignature.SLGIncomplete);
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
    }

    if (ret)
        ret &= WriteUTF8TextLine(hFile, L"", &fileCRC32);

    int i;
    for (i = 0; i < DlgData.Count; i++)
    {
        CDialogData* dialog = DlgData[i];
        swprintf_s(buff, L"[DIALOG %d]", dialog->ID);
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        for (int j = 0; j < dialog->Controls.Count; j++)
        {
            CControl* control = dialog->Controls[j];
            if (j == 0)
                swprintf_s(buff, L"%d,%d", dialog->TCX, dialog->TCY);
            else
                swprintf_s(buff, L"%d,%d,%d,%d,%d", control->ID, control->TX, control->TY, control->TCX, control->TCY);
            SWPrintFToEnd_s(buff, L",%d,\"%s\"", control->State, control->TWindowName);
            if (ret)
                ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        }
        if (ret)
            ret &= WriteUTF8TextLine(hFile, L"", &fileCRC32);
    }

    for (i = 0; i < MenuData.Count; i++)
    {
        CMenuData* menu = MenuData[i];
        swprintf_s(buff, L"[MENU %d]", menu->ID);
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        for (int j = 0; j < menu->Items.Count; j++)
        {
            CMenuItem* item = &menu->Items[j];

            if (wcslen(item->OString) > 0)
            {
                swprintf_s(buff, L"%d,%d,\"%s\"", item->ID, item->State, item->TString);
                if (ret)
                    ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
            }
        }
        if (ret)
            ret &= WriteUTF8TextLine(hFile, L"", &fileCRC32);
    }

    for (i = 0; i < StrData.Count; i++)
    {
        swprintf_s(buff, L"[STRINGTABLE %d]", i);
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        for (int j = 0; j < 16; j++)
        {
            if (StrData[i]->OStrings[j] != NULL)
            {
                wchar_t* str = (StrData[i]->TStrings[j] != NULL) ? StrData[i]->TStrings[j] : (wchar_t*)L"";
                WORD strID = ((StrData[i]->ID - 1) << 4) | j;
                WORD state = StrData[i]->TState[j];
                swprintf_s(buff, L"%d,%d,\"%s\"", strID, state, str);
                if (ret)
                    ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
            }
        }
        if (ret)
            ret &= WriteUTF8TextLine(hFile, L"", &fileCRC32);
    }

    if (Relayout.Count > 0)
    {
        if (ret)
            ret &= WriteUTF8TextLine(hFile, L"[RELAYOUT]", &fileCRC32);
        for (i = 0; i < Relayout.Count; i++)
        {
            swprintf_s(buff, L"%d", Relayout[i]);
            if (ret)
                ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        }
        if (ret)
            ret &= WriteUTF8TextLine(hFile, L"", &fileCRC32);
    }

    HANDLES(CloseHandle(hFile));

    if (ret)
    {
        if (!withoutVerInfo) // tenhle export neni jen pro diff, tedy update CRC32 ma smysl
        {
            wchar_t bufW[50];
            swprintf_s(bufW, L"%08X SLT", fileCRC32);
            SLGSignature.SetCRCofImpSLTIfFound(bufW);
        }
    }
    else
    {
        swprintf_s(outputBuff, L"Exporting translation to SLT text archive FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
    }
    return ret;
}

BOOL CData::ExportDialogsAndControlsSizes(const char* fileName)
{
    char buf[MAX_PATH + 1000];

    OutWindow.Clear();
    wchar_t outputBuff[10000];
    swprintf_s(outputBuff, L"Exporting dialogs and controls sizes to SDC text file...");
    OutWindow.AddLine(outputBuff, mteInfo);

    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                        CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        swprintf_s(outputBuff, L"Exporting dialogs and controls sizes to SDC text file FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }

    DWORD fileCRC32 = 0; // CRC se zde nepouziva, jen jsem linej prepisovat WriteUTF8TextLine do ANSI varianty
    BOOL ret = TRUE;
    wchar_t buff[10000];

    int i;
    for (i = 0; i < DlgData.Count; i++)
    {
        CDialogData* dialog = DlgData[i];
        swprintf_s(buff, L"[DIALOG %hs]", DataRH.GetIdentifier(dialog->ID, FALSE));
        if (ret)
            ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        for (int j = 0; j < dialog->Controls.Count; j++)
        {
            CControl* control = dialog->Controls[j];
            if (j == 0)
                swprintf_s(buff, L"%d,%d,%d,%d", dialog->TX, dialog->TY, dialog->TCX, dialog->TCY);
            else
                swprintf_s(buff, L"%hs,%d,%d,%d,%d", DataRH.GetIdentifier(control->ID, FALSE), control->TX, control->TY, control->TCX, control->TCY);
            if (ret)
                ret &= WriteUTF8TextLine(hFile, buff, &fileCRC32);
        }
        if (ret)
            ret &= WriteUTF8TextLine(hFile, L"", &fileCRC32);
    }

    HANDLES(CloseHandle(hFile));

    if (!ret)
    {
        swprintf_s(outputBuff, L"Exporting dialogs and controls sizes to SDC text file FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
    }
    return ret;
}

BOOL IsAlphaW(wchar_t ch);
BOOL IsNumberW(wchar_t ch)
{
    return ch >= '0' && ch <= '9';
}

BOOL ITACheckSection(const wchar_t* buff, int lineNumber, const wchar_t* section, DWORD id = -1)
{
    const wchar_t* p = buff;
    if (*p != L'[')
        return FALSE;
    p++;
    const wchar_t* name = p;
    while (IsAlphaW(*p))
        p++;
    if ((p - name) != wcslen(section) || wcsncmp(name, section, p - name) != 0)
        return FALSE;
    if (id != -1)
    {
        if (*p != L' ')
            return FALSE;
        p++;
        const wchar_t* num = p;
        wchar_t numBuff[10];
        while (IsNumberW(*p))
        {
            if (p - num > 9)
                return FALSE;
            numBuff[p - num] = *p;
            p++;
        }
        numBuff[p - num] = 0;

        int i = _wtoi(numBuff);
        if (i < 0 || i > 32767)
            return FALSE;
        if (id != i)
            return FALSE;
    }
    if (*p != L']')
        return FALSE;
    return TRUE;
}

BOOL ITAGetTranslationID(BOOL testOnly, const wchar_t* buff, int lineNumber, const wchar_t* name, WORD* id)
{
    const wchar_t* p = buff;
    while (IsAlphaW(*p))
        p++;
    if ((p - buff) != wcslen(name) || wcsncmp(buff, name, p - buff) != 0)
        return FALSE;
    if (*p != L',')
        return FALSE;
    p++;

    const wchar_t* num = p;
    while (IsNumberW(*p))
        p++;
    if (*p != 0)
        return FALSE;

    WORD i = _wtoi(num);
    if (i < 0 || i > 65535)
        return FALSE;

    if (!testOnly)
        *id = i;
    return TRUE;
}

BOOL ITAGetTranslationText(BOOL testOnly, const wchar_t* buff, int lineNumber, const wchar_t* name,
                           wchar_t* text, int textSize)
{
    const wchar_t* p = buff;
    while (IsAlphaW(*p))
        p++;
    if ((p - buff) != wcslen(name) || wcsncmp(buff, name, p - buff) != 0)
        return FALSE;
    if (*p != L',')
        return FALSE;
    p++;

    if (*p != L'\"')
        return FALSE;
    p++;

    const wchar_t* val = p;

    while (*p != 0 && *(p + 1) != 0)
        p++;
    if (*p != L'\"')
        return FALSE;

    if (p < val || (p - val >= textSize))
        return FALSE;

    if (!testOnly)
        wcsncpy_s(text, textSize, val, p - val);

    return TRUE;
}

BOOL SetNewText(wchar_t** ptr, const wchar_t* newText, int len)
{
    wchar_t* s = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (s == NULL)
    {
        TRACE_E("LOW MEMORY");
        return FALSE;
    }
    wcsncpy_s(s, len + 1, newText, len);
    s[len] = 0;

    if (*ptr != NULL)
        free(*ptr);
    *ptr = s;
    return TRUE;
}

BOOL GetUnicodeItemText(BOOL testOnly, const wchar_t* buff, int lineNumber, wchar_t** itemText)
{
    const wchar_t* p = buff;

    if (*p != L'\"')
        return FALSE;
    p++;

    const wchar_t* text = p;

    while (*p != 0 && *(p + 1) != 0)
        p++;

    if (*p != L'\"')
        return FALSE;

    if (p < text)
        return FALSE;

    if (!testOnly)
        SetNewText(itemText, text, p - text);

    return TRUE;
}

BOOL GetShortFromBuff(const wchar_t** num, short* ret)
{
    wchar_t numBuff[10];
    const wchar_t* p = *num;
    while ((p == *num && *p == '-') || IsNumberW(*p))
    {
        if (p - *num > 9)
            return FALSE;
        numBuff[p - *num] = *p;
        p++;
    }
    numBuff[p - *num] = 0;

    int i = _wtoi(numBuff);
    if (i < -32768 || i > 32767)
        return FALSE;
    *ret = i;
    *num = p;
    return TRUE;
}

BOOL GetIDFromBuff(const wchar_t** num, WORD* ret)
{
    wchar_t numBuff[10];
    const wchar_t* p = *num;
    while (IsNumberW(*p))
    {
        if (p - *num > 9)
            return FALSE;
        numBuff[p - *num] = *p;
        p++;
    }
    numBuff[p - *num] = 0;

    int i = _wtoi(numBuff);
    if (i < 0 || i > 65535)
    {
        TRACE_E("ID out of range: " << i);
        return FALSE;
    }
    *ret = (WORD)i;
    *num = p;
    return TRUE;
}
/*
BOOL GetDWORDFromBuff(const wchar_t **num, DWORD *ret)
{
  wchar_t numBuff[15];
  const wchar_t *p = *num;
  while (IsNumberW(*p))
  {
    if (p - *num > 14)
      return FALSE;
    numBuff[p - *num] = *p;
    p++;
  }
  numBuff[p - *num] = 0;

  DWORD d = _wtoi(numBuff);
  *ret = d;
  *num = p;
  return TRUE;
}

BOOL GetDWORDFromHEXBuff(const wchar_t **num, DWORD *ret)
{
  wchar_t numBuff[9];
  const wchar_t *p = *num;
  while (IsNumberW(*p) || 
         *p == 'a' || *p == 'b' || *p == 'c' || *p == 'd' || *p == 'e' || *p == 'f' ||
         *p == 'A' || *p == 'B' || *p == 'C' || *p == 'D' || *p == 'E' || *p == 'F')
  {
    if (p - *num > 8)
      return FALSE;
    numBuff[p - *num] = *p;
    p++;
  }
  if ((p - *num) != 8)
    return FALSE;
  numBuff[p - *num] = 0;
  DWORD dw;
  swscanf(numBuff, L"%08x", &dw);
  *ret = dw;
  *num = p;
  return TRUE;
}
*/
BOOL GetUnicodeDialogCaption(BOOL testOnly, const wchar_t* buff, int lineNumber, CDialogData* dialog, CControl* control)
{
    short tcx, tcy;
    WORD state;
    const wchar_t* p = buff;

    if (!GetShortFromBuff(&p, &tcx))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetShortFromBuff(&p, &tcy))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetIDFromBuff(&p, &state))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetUnicodeItemText(testOnly, p, lineNumber, &control->TWindowName))
        return FALSE;
    if (state != 0 && state != 1)
        return FALSE;
    if (!testOnly)
    {
        dialog->TCX = tcx;
        dialog->TCY = tcy;
        control->State = state;
    }
    return TRUE;
}

BOOL GetUnicodeDialogItem(BOOL testOnly, const wchar_t* buff, int lineNumber, CControl* control)
{
    WORD id;
    short tx, ty, tcx, tcy;
    WORD state;
    const wchar_t* p = buff;

    if (!GetIDFromBuff(&p, &id))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetShortFromBuff(&p, &tx))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetShortFromBuff(&p, &ty))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetShortFromBuff(&p, &tcx))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetShortFromBuff(&p, &tcy))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetIDFromBuff(&p, &state))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetUnicodeItemText(testOnly, p, lineNumber, &control->TWindowName))
        return FALSE;
    if (control->ID != id)
        return FALSE;
    if (state != 0 && state != 1)
        return FALSE;
    if (!testOnly)
    {
        control->TX = tx;
        control->TY = ty;
        control->TCX = tcx;
        control->TCY = tcy;
        control->State = state;
    }
    return TRUE;
}

BOOL GetUnicodeMenuItem(BOOL testOnly, const wchar_t* buff, int lineNumber, CMenuItem* menuItem)
{
    WORD id, state;
    const wchar_t* p = buff;

    if (!GetIDFromBuff(&p, &id))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetIDFromBuff(&p, &state))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetUnicodeItemText(testOnly, p, lineNumber, &menuItem->TString))
        return FALSE;
    if (menuItem->ID != id)
        return FALSE;
    if (state != 0 && state != 1)
        return FALSE;
    if (!testOnly)
    {
        menuItem->State = state;
    }
    return TRUE;
}

BOOL GetUnicodeStringTableItem(BOOL testOnly, const wchar_t* buff, int lineNumber, wchar_t** itemText, WORD* translationState, int itemID)
{
    WORD id, state;
    const wchar_t* p = buff;

    if (!GetIDFromBuff(&p, &id))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetIDFromBuff(&p, &state))
        return FALSE;
    if (*p++ != L',')
        return FALSE;
    if (!GetUnicodeItemText(testOnly, p, lineNumber, itemText))
        return FALSE;
    if (itemID != id)
        return FALSE;
    if (!testOnly)
    {
        *translationState = state;
    }
    return TRUE;
}

/*
BOOL GetUnicodeTranslatedState(BOOL testOnly, const wchar_t *buff, int lineNumber, int itemID, WORD *itemVal)
{
  DWORD id;
  WORD val;
  const wchar_t *p = buff;

  if (!GetDWORDFromHEXBuff(&p, &id)) return FALSE;
  if (*p++ != L'=') return FALSE;
  if (!GetIDFromBuff(&p, &val)) return FALSE;
  if (id != itemID)
    return FALSE;
  if (val != 0 && val != 1)
    return FALSE;

  if (!testOnly)
  {
    *itemVal = val;
  }

  return TRUE;
}
*/

/*
BOOL GetUnicodeTextLine(const wchar_t **lineStart, const wchar_t *fileEnd, wchar_t *buff, int buffSize, int *lineNumber)
{
  const wchar_t *p = *lineStart;
  while (*p != '\r' && *p != '\n' && p < fileEnd)
  {
    p++;
  }
  if ((p - *lineStart) * 2 < buffSize)
  {
    memmove(buff, *lineStart, (p - *lineStart) * 2);
    buff[p - *lineStart] = 0;

    // ustrihneme whitespacy na konci radku, nemusime byt s importem az tak moc striktni
    wchar_t *term = buff + (p - *lineStart) - 1;
    while (term >= buff && (*term == ' ' || *term == '\t'))
    {
      *term = 0;
      term--;
    }
  }
  else
  {
    // kratky buffer
    return FALSE;
  }
  if (p < fileEnd && *p == '\r')
    p++;
  if (p < fileEnd && *p == '\n')
    p++;

  *lineStart = p;
  (*lineNumber)++;
  
  return TRUE;
}
*/

int ConvertA2U(const char* src, int srcLen, WCHAR* buf, int bufSizeInChars, UINT codepage)
{
    if (buf == NULL || bufSizeInChars <= 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    buf[0] = 0;
    if (src == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (srcLen != -1 && srcLen <= 0)
    {
        if (srcLen == 0)
            return 1;
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    int res = MultiByteToWideChar(codepage, 0, src, srcLen, buf, bufSizeInChars);
    if (srcLen != -1 && res > 0)
        res++;
    if (res > 0 && res <= bufSizeInChars)
        buf[res - 1] = 0; // uspech, zakoncime string nulou
    else
    {
        if (res > bufSizeInChars || res == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            buf[bufSizeInChars - 1] = 0; // maly buffer, vratime chybu, ale castecne prelozeny string nechame v bufferu
        }
        else
            buf[0] = 0; // jina chyba, zajistime prazdny buffer
        res = 0;
    }
    return res;
}

BOOL GetUTF8TextLine(const char** lineStart, const char* fileEnd, wchar_t* buff, int buffSize, int* lineNumber)
{
    const char* p = *lineStart;
    while (*p != '\r' && *p != '\n' && p < fileEnd)
    {
        p++;
    }
    if ((p - *lineStart) < buffSize)
    {
        ConvertA2U(*lineStart, (p - *lineStart), buff, buffSize, CP_UTF8);

        // ustrihneme whitespacy na konci radku, nemusime byt s importem az tak moc striktni
        wchar_t* term = buff + wcslen(buff) - 1;
        while (term >= buff && (*term == ' ' || *term == '\t'))
        {
            *term = 0;
            term--;
        }
    }
    else
    {
        // kratky buffer
        return FALSE;
    }
    if (p < fileEnd && *p == '\r')
        p++;
    if (p < fileEnd && *p == '\n')
        p++;

    *lineStart = p;
    (*lineNumber)++;

    return TRUE;
}

BOOL CData::ImportTextArchive(const char* fileName, BOOL testOnly)
{
    char buf[MAX_PATH + 1000];
    OutWindow.Clear();
    wchar_t outputBuff[10000];
    swprintf_s(outputBuff, L"Importing translation from SLT text archive...");
    OutWindow.AddLine(outputBuff, mteInfo);

    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        sprintf_s(buf, "Error reading file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        swprintf_s(outputBuff, L"Importing translation from SLT text archive FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }

    DWORD size = GetFileSize(hFile, NULL);
    if (size == 0xFFFFFFFF || size == 0)
    {
        sprintf_s(buf, "Error reading file %s.", fileName);
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        HANDLES(CloseHandle(hFile));
        swprintf_s(outputBuff, L"Importing translation from SLT text archive FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }

    char* data = (char*)malloc(size + 2);
    if (data == NULL)
    {
        TRACE_E("Nedostatek pameti");
        HANDLES(CloseHandle(hFile));
        swprintf_s(outputBuff, L"Importing translation from SLT text archive FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }

    DWORD read;
    if (!ReadFile(hFile, data, size, &read, NULL) || read != size)
    {
        DWORD err = GetLastError();
        sprintf_s(buf, "Error reading file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        free(data);
        HANDLES(CloseHandle(hFile));
        swprintf_s(outputBuff, L"Importing translation from SLT text archive FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }
    data[size] = 0; // vlozime terminator

    DWORD fileCRC32 = 0;
    if (!testOnly)
        fileCRC32 = UpdateCrc32(data, size, 0);

    if (testOnly && SLGSignature.IsSLTDataChanged())
    {
        sprintf_s(buf, "You have made changes to this module in Translator since last "
                       "import from SLT file, export to SLT file, or creating this language module. "
                       "Now you are trying to import SLT file %s. Import will overwrite all these "
                       "changes.\n\n"
                       "Do you really want to continue with import and so lose your changes?\n\n"
                       "We recommend to export SLT file for this module to filename with some suffix "
                       "(e.g. \"_cur\"), use menu File / Export Translation as Text File (Ctrl+D key). "
                       "Then compare exported SLT file with SLT file which you want to import. "
                       "If changes in both files are relevant, you can merge them and then import "
                       "resulting SLT file.",
                  fileName);
        if (MessageBox(GetMsgParent(), buf, FRAMEWINDOW_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
        {
            free(data);
            HANDLES(CloseHandle(hFile));
            swprintf_s(outputBuff, L"Importing translation from SLT text archive was cancelled.");
            OutWindow.AddLine(outputBuff, mteError);
            return FALSE;
        }
    }

    const char* lineStart = data;
    const char* fileEnd = data + size;

    BOOL ret = TRUE;
#define LINE_BUFF_SIZE 10000
    wchar_t buff[LINE_BUFF_SIZE];

    int lineNumber = 1;

    // preskocim UTF8 BOM (pokud je na zacatku souboru)
    if (lineStart + 3 <= fileEnd && *lineStart == '\xEF' && *(lineStart + 1) == '\xBB' && *(lineStart + 2) == '\xBF')
        lineStart += 3;

    //  if (ret) ret &= VerifyBOM((WORD)*lineStart);
    //  if (ret) lineStart++;

    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
    if (ret)
        ret &= ITACheckSection(buff, lineNumber, L"EXPORTINFO");
    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber); // skip
    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber); // skip
    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber); // skip
    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber); // skip
    if (ret)
        ret = (buff[0] == 0);

    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
    if (ret)
        ret &= ITACheckSection(buff, lineNumber, L"TRANSLATION");

    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
    if (ret)
        ret &= ITAGetTranslationID(testOnly, buff, lineNumber, L"LANGID", &Data.SLGSignature.LanguageID);
    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
    if (ret)
        ret &= ITAGetTranslationText(testOnly, buff, lineNumber, L"AUTHOR", Data.SLGSignature.Author, _countof(Data.SLGSignature.Author));
    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
    if (ret)
        ret &= ITAGetTranslationText(testOnly, buff, lineNumber, L"WEB", Data.SLGSignature.Web, _countof(Data.SLGSignature.Web));
    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
    if (ret)
        ret &= ITAGetTranslationText(testOnly, buff, lineNumber, L"COMMENT", Data.SLGSignature.Comment, _countof(Data.SLGSignature.Comment));
    if (ret)
        ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
    if (ret)
    {
        if (ITAGetTranslationText(testOnly, buff, lineNumber, L"HELPDIR", Data.SLGSignature.HelpDir, _countof(Data.SLGSignature.HelpDir)))
        {
            if (!testOnly)
                Data.SLGSignature.HelpDirExist = (Data.SLGSignature.HelpDir[0] != 0);
            ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
        }
        else
        {
            if (!testOnly)
            {
                Data.SLGSignature.HelpDirExist = FALSE;
                Data.SLGSignature.HelpDir[0] = 0;
            }
        }
    }
    if (ret)
    {
        if (ITAGetTranslationText(testOnly, buff, lineNumber, L"SLGINCOMPLETE", Data.SLGSignature.SLGIncomplete, _countof(Data.SLGSignature.SLGIncomplete)))
        {
            if (!testOnly)
                Data.SLGSignature.SLGIncompleteExist = TRUE;
            ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
        }
        else
        {
            if (!testOnly)
            {
                Data.SLGSignature.SLGIncompleteExist = FALSE;
                Data.SLGSignature.SLGIncomplete[0] = 0;
            }
        }
    }
    if (ret)
        ret = (buff[0] == 0);

    int i;
    for (i = 0; i < DlgData.Count; i++)
    {
        CDialogData* dialog = DlgData[i];

        if (ret)
            ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
        if (ret)
            ret &= ITACheckSection(buff, lineNumber, L"DIALOG", dialog->ID);

        for (int j = 0; j < dialog->Controls.Count; j++)
        {
            CControl* control = dialog->Controls[j];
            if (ret)
                ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
            if (j == 0)
            {
                if (ret)
                    ret &= GetUnicodeDialogCaption(testOnly, buff, lineNumber, dialog, control);
            }
            else
            {
                if (ret)
                    ret &= GetUnicodeDialogItem(testOnly, buff, lineNumber, control);
            }
        }
        if (ret)
            ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
        if (ret)
            ret = (buff[0] == 0);
    }

    for (i = 0; i < MenuData.Count; i++)
    {
        CMenuData* menu = MenuData[i];

        if (ret)
            ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
        if (ret)
            ret &= ITACheckSection(buff, lineNumber, L"MENU", menu->ID);

        for (int j = 0; j < menu->Items.Count; j++)
        {
            CMenuItem* item = &menu->Items[j];

            if (wcslen(item->OString) > 0)
            {
                if (ret)
                    ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
                if (ret)
                    ret &= GetUnicodeMenuItem(testOnly, buff, lineNumber, item);
            }
        }
        if (ret)
            ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
        if (ret)
            ret = (buff[0] == 0);
    }

    for (i = 0; i < StrData.Count; i++)
    {
        if (ret)
            ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
        if (ret)
            ret &= ITACheckSection(buff, lineNumber, L"STRINGTABLE", i);

        for (int j = 0; j < 16; j++)
        {
            if (StrData[i]->OStrings[j] != NULL)
            {
                WORD strID = ((StrData[i]->ID - 1) << 4) | j;
                if (ret)
                    ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
                if (ret)
                    ret &= GetUnicodeStringTableItem(testOnly, buff, lineNumber, &StrData[i]->TStrings[j], &StrData[i]->TState[j], strID);
            }
        }
        if (ret)
            ret &= GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber);
        if (ret)
            ret = (buff[0] == 0);
    }

    if (!testOnly)
        CleanRelayout(); // jestli jsou nejake dialogy k releayout, budou v sekci [RELAYOUT] .slt souboru

    // okontrolujeme, ze dal uz soubor krome pripadne [RELAYOUT] sekce nic neobsahuje
    BOOL relayoutFound = FALSE;
    while (ret && lineStart < fileEnd &&
           GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber))
    {
        if (buff[0] != 0)
        {
            if (!relayoutFound && ITACheckSection(buff, lineNumber, L"RELAYOUT"))
            {
                relayoutFound = TRUE;
                while (lineStart < fileEnd &&
                       GetUTF8TextLine(&lineStart, fileEnd, buff, LINE_BUFF_SIZE, &lineNumber))
                {
                    if (buff[0] != 0)
                    {
                        wchar_t* endptr;
                        unsigned long id = wcstoul(buff, &endptr, 10);
                        if (*endptr != 0 || id == 0 || id > 0xFFFF)
                        {
                            ret = FALSE;
                            break;
                        }
                        if (!testOnly)
                            AddRelayout(LOWORD(id));
                    }
                    else
                        break;
                }
                continue;
            }
            ret = FALSE;
            break;
        }
    }

    if (!ret /*&& lineNumber > 1*/) // spatnej BOM tu nehlasime
    {
        sprintf_s(buf, "Syntax error in file %s on line %d.", fileName, lineNumber - 1);
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);

        swprintf_s(outputBuff, L"Syntax error in file %hs on line %d.", fileName, lineNumber - 1);
        OutWindow.AddLine(outputBuff, mteError);
    }
    HANDLES(CloseHandle(hFile));
    free(data);
    if (!ret)
    {
        swprintf_s(outputBuff, L"Importing translation from SLT text archive FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
    }
    else
    {
        if (!testOnly)
        {
            wchar_t bufW[50];
            swprintf_s(bufW, L"%08X SLT", fileCRC32);
            SLGSignature.SetCRCofImpSLTIfFound(bufW);
        }
    }
    return ret;
}
