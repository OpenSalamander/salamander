// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndframe.h"
#include "config.h"
#include "translator.h"

const char* DB_ROOT_KEY = "Software\\Open Salamander\\Translator";
const char* WINDOW_LEFT_REG = "Left";
const char* WINDOW_RIGHT_REG = "Right";
const char* WINDOW_TOP_REG = "Top";
const char* WINDOW_BOTTOM_REG = "Bottom";
const char* WINDOW_SHOW_REG = "Show";

const char* COLUMN_TEXTNAME_REG = "Name";
const char* COLUMN_TEXTTRANSLATED_REG = "Translated";
const char* COLUMN_TEXTORIGINAL_REG = "Original";

const char* CONFIG_REG = "Configuration";
const char* CONFIG_FINDTEXTS = "FindTexts";
const char* CONFIG_FINDSYMBOLS = "FindSymbols";
const char* CONFIG_FINDORIGINAL = "FindOriginal";
const char* CONFIG_FINDWORDS = "FindWords";
const char* CONFIG_FINDCASE = "FindCase";
const char* CONFIG_IGNOREAMPERSAND = "IgnoreAmpersand";
const char* CONFIG_IGNOREDASH = "IgnoreDash";

const char* CONFIG_FINDHISTORY_REG = "Find History";

const char* CONFIG_RELOADPROJECT = "ReloadProjectAtStartup";
const char* CONFIG_PREVIEW_EXTENDDIALOGS = "PreviewExtendDialogs";
const char* CONFIG_PREVIEW_OUTLINECTRLS = "PreviewOutlineControls";
const char* CONFIG_PREVIEW_FILLCTRLS = "PreviewFillControls";

const char* CONFIG_EDITOR_OUTLINECTRLS = "EditorOutlineControls";
const char* CONFIG_EDITOR_FILLCTRLS = "EditorFillControls";

const char* CONFIG_LASTPROJECT = "Last Project";

const char* CONFIG_MUI_ORIGINAL = "MUI Original";
const char* CONFIG_MUI_TRANSLATED = "MUI Translated";

const char* RECENT_REG = "Recent Projects";

const char* WINDOWS_REG = "Windows";
const char* FRAMEWINDOW_REG = "FrameWindow";
const char* TREEWINDOW_REG = "TreeWindow";
const char* RHWINDOW_REG = "RHWindow";
const char* TEXTWINDOW_REG = "TextWindow";
const char* OUTWINDOW_REG = "OutWindow";
const char* PREVIEWWINDOW_REG = "PreviewWindow";

const char* CONFIG_AGREEMENT_CONFIRMED_REG = "AgreementConfirmed";

const char* CONFIG_VLD_DLGHOTKEY = "ValidateDlgHotKeyConflicts";
const char* CONFIG_VLD_MENUHOTKEY = "ValidateMenuHotKeyConflicts";
const char* CONFIG_VLD_PRINTF = "ValidatePrintfSpecifier";
const char* CONFIG_VLD_CONTROLCHARS = "ValidateControlChars";
const char* CONFIG_VLD_STRINGSWITHCSV = "ValidateStringsWithCSV";
const char* CONFIG_VLD_PLURALSTR = "ValidatePluralStr";
const char* CONFIG_VLD_TEXTENDING = "ValidateTextEnding";
const char* CONFIG_VLD_HOTKEYS = "ValidateHotKeys";
const char* CONFIG_VLD_LAYOUT = "ValidateLayout";
const char* CONFIG_VLD_CLIPPING = "ValidateClipping";
const char* CONFIG_VLD_CONTROLSTODLG = "ValidateControlsToDlg";
const char* CONFIG_VLD_MISALIGNEDCONTROLS = "ValidateMisalignedControls";
const char* CONFIG_VLD_DIFFERENTLYSIZEDCONTROLS = "ValidateDifferentSizedControls";
const char* CONFIG_VLD_DIFFERENTLYSPACEDCONTROLS = "ValidateDifferentSpacedControls";
const char* CONFIG_VLD_STANDARDCONTROLSSIZE = "ValidateStandardControlsSize";
const char* CONFIG_VLD_LABELSTOCONTROLSSPACING = "ValidateLabelsToControlsSpacing";
const char* CONFIG_VLD_SIZESOFPROPPAGES = "ValidateSizesOfPropPages";
const char* CONFIG_VLD_DIALOGCONTROLSID = "ValidateDialogControlsID";
const char* CONFIG_VLD_HBUTTONSPACING = "ValidateHButtonSpacing";

CConfiguration Config;

// ****************************************************************************

BOOL CreateKey(HKEY hKey, const char* name, HKEY& createdKey)
{
    DWORD createType; // info jestli byl klic vytvoren nebo jen otevren
    LONG res = HANDLES(RegCreateKeyEx(hKey, name, 0, NULL, REG_OPTION_NON_VOLATILE,
                                      KEY_READ | KEY_WRITE, NULL, &createdKey,
                                      &createType));
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        MessageBox(FrameWindow.HWindow, GetErrorText(res),
                   "Chyba pri ukladani klice", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

// ****************************************************************************

BOOL SetValue(HKEY hKey, const char* name, DWORD type,
              const void* data, DWORD dataSize)
{
    if (dataSize == -1)
        dataSize = strlen((char*)data) + 1;
    LONG res = RegSetValueEx(hKey, name, 0, type, (CONST BYTE*)data, dataSize);
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        MessageBox(FrameWindow.HWindow, GetErrorText(res),
                   "Chyba pri ukladani klice", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

BOOL SetValueW(HKEY hKey, const char* name, DWORD type,
               const void* data, DWORD dataSize)
{
    if (dataSize == -1)
        dataSize = strlen((char*)data) + 1;

    wchar_t tmp[1000];
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, tmp, 1000);

    LONG res = RegSetValueExW(hKey, tmp, 0, type, (CONST BYTE*)data, dataSize);
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        MessageBox(FrameWindow.HWindow, GetErrorText(res),
                   "Chyba pri ukladani klice", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

// ****************************************************************************

BOOL OpenKey(HKEY hKey, const char* name, HKEY& openedKey)
{
    LONG res = HANDLES_Q(RegOpenKeyEx(hKey, name, 0, KEY_READ, &openedKey));
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        if (res != ERROR_FILE_NOT_FOUND)
            MessageBox(FrameWindow.HWindow, GetErrorText(res),
                       "Chyba pri nacitani konfigurace", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

// ****************************************************************************

BOOL GetValue(HKEY hKey, const char* name, DWORD type, void* buffer, DWORD bufferSize)
{
    DWORD gettedType;
    LONG res = RegQueryValueEx(hKey, name, 0, &gettedType, (BYTE*)buffer, &bufferSize);
    if (res == ERROR_SUCCESS)
        if (gettedType == type)
            return TRUE;
        else
        {
            MessageBox(FrameWindow.HWindow, "Neocekavany typ hodnoty",
                       "Chyba pri nacitani konfigurace", MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    else
    {
        if (res != ERROR_FILE_NOT_FOUND)
            MessageBox(FrameWindow.HWindow, GetErrorText(res),
                       "Chyba pri nacitani konfigurace", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

BOOL GetValueW(HKEY hKey, const char* name, DWORD type, void* buffer, DWORD bufferSize)
{
    DWORD gettedType;

    wchar_t tmp[1000];
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, tmp, 1000);

    LONG res = RegQueryValueExW(hKey, tmp, 0, &gettedType, (BYTE*)buffer, &bufferSize);
    if (res == ERROR_SUCCESS)
        if (gettedType == type)
            return TRUE;
        else
        {
            MessageBox(FrameWindow.HWindow, "Neocekavany typ hodnoty",
                       "Chyba pri nacitani konfigurace", MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    else
    {
        if (res != ERROR_FILE_NOT_FOUND)
            MessageBox(FrameWindow.HWindow, GetErrorText(res),
                       "Chyba pri nacitani konfigurace", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

// ****************************************************************************

BOOL GetSize(HKEY hKey, const char* name, DWORD type, DWORD& bufferSize)
{
    DWORD gettedType;
    LONG res = RegQueryValueEx(hKey, name, 0, &gettedType, NULL, &bufferSize);
    if (res == ERROR_SUCCESS)
        if (gettedType == type)
            return TRUE;
        else
        {
            MessageBox(FrameWindow.HWindow, "Neocekavany typ hodnoty",
                       "Chyba pri nacitani konfigurace", MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    else
    {
        if (res != ERROR_FILE_NOT_FOUND)
            MessageBox(FrameWindow.HWindow, GetErrorText(res),
                       "Chyba pri nacitani konfigurace", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

BOOL GetSizeW(HKEY hKey, const char* name, DWORD type, DWORD& bufferSize)
{
    DWORD gettedType;

    wchar_t tmp[1000];
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, tmp, 1000);

    LONG res = RegQueryValueExW(hKey, tmp, 0, &gettedType, NULL, &bufferSize);
    if (res == ERROR_SUCCESS)
        if (gettedType == type)
            return TRUE;
        else
        {
            MessageBox(FrameWindow.HWindow, "Neocekavany typ hodnoty",
                       "Chyba pri nacitani konfigurace", MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    else
    {
        if (res != ERROR_FILE_NOT_FOUND)
            MessageBox(FrameWindow.HWindow, GetErrorText(res),
                       "Chyba pri nacitani konfigurace", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

// ****************************************************************************

BOOL ClearKey(HKEY key)
{
    char name[MAX_PATH];
    HKEY subKey;
    while (RegEnumKey(key, 0, name, MAX_PATH) == ERROR_SUCCESS)
    {
        if (HANDLES_Q(RegOpenKeyEx(key, name, 0, KEY_READ | KEY_WRITE, &subKey)) == ERROR_SUCCESS)
        {
            BOOL ret = ClearKey(subKey);
            HANDLES(RegCloseKey(subKey));
            if (!ret || RegDeleteKey(key, name) != ERROR_SUCCESS)
                return FALSE;
        }
        else
            return FALSE;
    }

    DWORD size = MAX_PATH;
    while (RegEnumValue(key, 0, name, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        if (RegDeleteValue(key, name) != ERROR_SUCCESS)
        {
            TRACE_E("Nepodarilo se smazat hodnoty klice z registry.");
            break;
        }
        else
            size = MAX_PATH;

    return TRUE;
}

// ****************************************************************************

void SaveWindowPlacement(HKEY hKey, const char* name, WINDOWPLACEMENT* wp)
{
    HKEY actKey;

    if (CreateKey(hKey, name, actKey))
    {
        SetValue(actKey, WINDOW_LEFT_REG, REG_DWORD,
                 &(wp->rcNormalPosition.left), sizeof(DWORD));
        SetValue(actKey, WINDOW_RIGHT_REG, REG_DWORD,
                 &(wp->rcNormalPosition.right), sizeof(DWORD));
        SetValue(actKey, WINDOW_TOP_REG, REG_DWORD,
                 &(wp->rcNormalPosition.top), sizeof(DWORD));
        SetValue(actKey, WINDOW_BOTTOM_REG, REG_DWORD,
                 &(wp->rcNormalPosition.bottom), sizeof(DWORD));
        SetValue(actKey, WINDOW_SHOW_REG, REG_DWORD,
                 &(wp->showCmd), sizeof(DWORD));

        HANDLES(RegCloseKey(actKey));
    }
}

BOOL LoadWindowPlacement(HKEY hKey, const char* name, WINDOWPLACEMENT* wp)
{
    BOOL ret = TRUE;
    HKEY actKey;
    if (OpenKey(hKey, name, actKey))
    {
        ret &= GetValue(actKey, WINDOW_LEFT_REG, REG_DWORD,
                        &(wp->rcNormalPosition.left), sizeof(DWORD));
        ret &= GetValue(actKey, WINDOW_RIGHT_REG, REG_DWORD,
                        &(wp->rcNormalPosition.right), sizeof(DWORD));
        ret &= GetValue(actKey, WINDOW_TOP_REG, REG_DWORD,
                        &(wp->rcNormalPosition.top), sizeof(DWORD));
        ret &= GetValue(actKey, WINDOW_BOTTOM_REG, REG_DWORD,
                        &(wp->rcNormalPosition.bottom), sizeof(DWORD));
        ret &= GetValue(actKey, WINDOW_SHOW_REG, REG_DWORD,
                        &(wp->showCmd), sizeof(DWORD));
        if (ret)
            wp->length = sizeof(WINDOWPLACEMENT);

        HANDLES(RegCloseKey(actKey));
    }
    else
        ret = FALSE;

    return ret;
}

/*
// ****************************************************************************
BOOL LoadHistory(HKEY hKey, const char *name, wchar_t *history[], int maxCount)
{
  HKEY historyKey;
  for (int i = 0; i < maxCount; i++)
    if (history[i] != NULL)
    {
      free(history[i]);
      history[i] = NULL;
    }
  if (OpenKey(hKey, name, historyKey))
  {
    char buf[10];
    for (i = 0; i < maxCount; i++)
    {
      _itoa_s(i + 1, buf, 10);
      DWORD bufferSize;
      if (GetSize(historyKey, buf, REG_SZ, bufferSize))
      {
        history[i] = (wchar_t *)malloc(bufferSize);
        if (history[i] == NULL)
        {
          break;
        }
        if (!GetValue(historyKey, buf, REG_SZ, history[i], bufferSize))
          break;
      }
    }
    HANDLES(RegCloseKey(historyKey));
  }
  return TRUE;
}

// ****************************************************************************

BOOL SaveHistory(HKEY hKey, const char *name, wchar_t *history[], int maxCount)
{
  HKEY historyKey;
  if (CreateKey(hKey, name, historyKey))
  {
    ClearKey(historyKey);
    char buf[10];
    for (int i = 0; i < maxCount; i++)
    {
      if (history[i] != NULL)
      {
        _itoa_s(i + 1, buf, 10);
        SetValue(historyKey, buf, REG_SZ, history[i], strlen(history[i]) + 1);
      }
      else break;
    }
    HANDLES(RegCloseKey(historyKey));
  }
  return TRUE;
}
*/

// ****************************************************************************
BOOL LoadHistoryW(HKEY hKey, const char* name, wchar_t* history[], int maxCount)
{
    HKEY historyKey;
    int i;
    for (i = 0; i < maxCount; i++)
        if (history[i] != NULL)
        {
            free(history[i]);
            history[i] = NULL;
        }
    if (OpenKey(hKey, name, historyKey))
    {
        char buf[10];
        for (i = 0; i < maxCount; i++)
        {
            _itoa_s(i + 1, buf, 10);
            DWORD bufferSize;
            if (GetSizeW(historyKey, buf, REG_SZ, bufferSize))
            {
                history[i] = (wchar_t*)malloc(bufferSize);
                if (history[i] == NULL)
                {
                    break;
                }
                if (!GetValueW(historyKey, buf, REG_SZ, history[i], bufferSize))
                    break;
            }
        }
        HANDLES(RegCloseKey(historyKey));
    }
    return TRUE;
}

// ****************************************************************************

BOOL SaveHistoryW(HKEY hKey, const char* name, wchar_t* history[], int maxCount)
{
    HKEY historyKey;
    if (CreateKey(hKey, name, historyKey))
    {
        ClearKey(historyKey);
        char buf[10];
        for (int i = 0; i < maxCount; i++)
        {
            if (history[i] != NULL)
            {
                _itoa_s(i + 1, buf, 10);
                SetValueW(historyKey, buf, REG_SZ, history[i], (wcslen(history[i]) + 1) * sizeof(wchar_t));
            }
            else
                break;
        }
        HANDLES(RegCloseKey(historyKey));
    }
    return TRUE;
}

//*****************************************************************************
//
// CConfiguration
//

CConfiguration::CConfiguration()
{
    FrameWindowPlacement.length = 0;
    TreeWindowPlacement.length = 0;
    RHWindowPlacement.length = 0;
    TextWindowPlacement.length = 0;
    PreviewWindowPlacement.length = 0;

    ColumnNameWidth = -1;
    ColumnTranslatedWidth = -1;
    ColumnOriginalWidth = -1;

    int i;
    for (i = 0; i < RECENT_PROJECTS_COUNT; i++)
        RecentProjects[i][0] = 0;

    LastProject[0] = 0;
    LastMUIOriginal[0] = 0;
    LastMUITranslated[0] = 0;

    FindTexts = TRUE;
    FindSymbols = TRUE;
    FindOriginal = FALSE;
    FindWords = FALSE;
    FindCase = FALSE;
    FindIgnoreAmpersand = TRUE;
    FindIgnoreDash = TRUE;

    AgreementConfirmed = FALSE;

    for (i = 0; i < FIND_HISTORY_SIZE; i++)
        FindHistory[i] = NULL;

    ReloadProjectAtStartup = FALSE;
    PreviewExtendDialogs = FALSE;
    PreviewOutlineControls = TRUE;
    PreviewFillControls = TRUE;

    EditorOutlineControls = TRUE;
    EditorFillControls = TRUE;

    // Validate Translation
    SetValidateAll();
}

CConfiguration::~CConfiguration()
{
    for (int i = 0; i < FIND_HISTORY_SIZE; i++)
        if (FindHistory[i] != NULL)
            free(FindHistory[i]);
}

void CConfiguration::SetValidateAll(int mode)
{
    ValidateDlgHotKeyConflicts = mode == 1;
    ValidateMenuHotKeyConflicts = mode == 1;
    ValidatePrintfSpecifier = TRUE;
    ValidateControlChars = TRUE;
    ValidateStringsWithCSV = TRUE;
    ValidatePluralStr = TRUE;
    ValidateTextEnding = TRUE;
    ValidateHotKeys = TRUE;
    ValidateLayout = TRUE;
    ValidateClipping = TRUE;
    ValidateControlsToDialog = TRUE;
    ValidateMisalignedControls = TRUE;
    ValidateDifferentSizedControls = TRUE;
    ValidateDifferentSpacedControls = TRUE;
    ValidateStandardControlsSize = TRUE;
    ValidateLabelsToControlsSpacing = TRUE;
    ValidateSizesOfPropPages = TRUE;
    ValidateDialogControlsID = TRUE;
    ValidateHButtonSpacing = TRUE;
}

BOOL CConfiguration::Save()
{
    HKEY config;
    if (CreateKey(HKEY_CURRENT_USER, DB_ROOT_KEY, config))
    {
        HKEY actKey;
        if (CreateKey(config, CONFIG_REG, actKey))
        {
            //      SetValue(actKey, CONFIG_SOURCEPATH_REG, REG_SZ, SourceFile, -1);
            //      SetValue(actKey, CONFIG_TARGETPATH_REG, REG_SZ, TargetFile, -1);
            //      SetValue(actKey, CONFIG_INCLUDEPATH_REG, REG_SZ, IncludeFile, -1);

            SetValue(actKey, CONFIG_FINDTEXTS, REG_DWORD, &FindTexts, sizeof(DWORD));
            SetValue(actKey, CONFIG_FINDSYMBOLS, REG_DWORD, &FindSymbols, sizeof(DWORD));
            SetValue(actKey, CONFIG_FINDORIGINAL, REG_DWORD, &FindOriginal, sizeof(DWORD));
            SetValue(actKey, CONFIG_FINDWORDS, REG_DWORD, &FindWords, sizeof(DWORD));
            SetValue(actKey, CONFIG_FINDCASE, REG_DWORD, &FindCase, sizeof(DWORD));
            SetValue(actKey, CONFIG_IGNOREAMPERSAND, REG_DWORD, &FindIgnoreAmpersand, sizeof(DWORD));
            SetValue(actKey, CONFIG_IGNOREDASH, REG_DWORD, &FindIgnoreDash, sizeof(DWORD));

            SaveHistoryW(actKey, CONFIG_FINDHISTORY_REG, FindHistory, FIND_HISTORY_SIZE);

            SetValue(actKey, COLUMN_TEXTNAME_REG, REG_DWORD, &ColumnNameWidth, sizeof(DWORD));
            SetValue(actKey, COLUMN_TEXTTRANSLATED_REG, REG_DWORD, &ColumnTranslatedWidth, sizeof(DWORD));
            SetValue(actKey, COLUMN_TEXTORIGINAL_REG, REG_DWORD, &ColumnOriginalWidth, sizeof(DWORD));

            SetValue(actKey, CONFIG_RELOADPROJECT, REG_DWORD, &ReloadProjectAtStartup, sizeof(DWORD));
            SetValue(actKey, CONFIG_PREVIEW_EXTENDDIALOGS, REG_DWORD, &PreviewExtendDialogs, sizeof(DWORD));
            SetValue(actKey, CONFIG_PREVIEW_OUTLINECTRLS, REG_DWORD, &PreviewOutlineControls, sizeof(DWORD));
            SetValue(actKey, CONFIG_PREVIEW_FILLCTRLS, REG_DWORD, &PreviewFillControls, sizeof(DWORD));
            SetValue(actKey, CONFIG_EDITOR_OUTLINECTRLS, REG_DWORD, &EditorOutlineControls, sizeof(DWORD));
            SetValue(actKey, CONFIG_EDITOR_FILLCTRLS, REG_DWORD, &EditorFillControls, sizeof(DWORD));

            SetValue(actKey, CONFIG_VLD_DLGHOTKEY, REG_DWORD, &ValidateDlgHotKeyConflicts, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_MENUHOTKEY, REG_DWORD, &ValidateMenuHotKeyConflicts, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_PRINTF, REG_DWORD, &ValidatePrintfSpecifier, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_CONTROLCHARS, REG_DWORD, &ValidateControlChars, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_STRINGSWITHCSV, REG_DWORD, &ValidateStringsWithCSV, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_PLURALSTR, REG_DWORD, &ValidatePluralStr, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_TEXTENDING, REG_DWORD, &ValidateTextEnding, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_HOTKEYS, REG_DWORD, &ValidateHotKeys, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_LAYOUT, REG_DWORD, &ValidateLayout, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_CLIPPING, REG_DWORD, &ValidateClipping, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_CONTROLSTODLG, REG_DWORD, &ValidateControlsToDialog, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_MISALIGNEDCONTROLS, REG_DWORD, &ValidateMisalignedControls, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_DIFFERENTLYSIZEDCONTROLS, REG_DWORD, &ValidateDifferentSizedControls, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_DIFFERENTLYSPACEDCONTROLS, REG_DWORD, &ValidateDifferentSpacedControls, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_STANDARDCONTROLSSIZE, REG_DWORD, &ValidateStandardControlsSize, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_LABELSTOCONTROLSSPACING, REG_DWORD, &ValidateLabelsToControlsSpacing, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_SIZESOFPROPPAGES, REG_DWORD, &ValidateSizesOfPropPages, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_DIALOGCONTROLSID, REG_DWORD, &ValidateDialogControlsID, sizeof(DWORD));
            SetValue(actKey, CONFIG_VLD_HBUTTONSPACING, REG_DWORD, &ValidateHButtonSpacing, sizeof(DWORD));

            SetValue(actKey, CONFIG_AGREEMENT_CONFIRMED_REG, REG_DWORD, &AgreementConfirmed, sizeof(DWORD));

            SetValue(actKey, CONFIG_LASTPROJECT, REG_SZ, LastProject, -1);
            SetValue(actKey, CONFIG_MUI_ORIGINAL, REG_SZ, LastMUIOriginal, -1);
            SetValue(actKey, CONFIG_MUI_TRANSLATED, REG_SZ, LastMUITranslated, -1);

            HANDLES(RegCloseKey(actKey));
        }

        if (CreateKey(config, WINDOWS_REG, actKey))
        {
            WINDOWPLACEMENT place;
            place.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(FrameWindow.HWindow, &place);
            SaveWindowPlacement(actKey, FRAMEWINDOW_REG, &place);

            SaveWindowPlacement(actKey, TREEWINDOW_REG, &TreeWindowPlacement);
            SaveWindowPlacement(actKey, RHWINDOW_REG, &RHWindowPlacement);
            SaveWindowPlacement(actKey, TEXTWINDOW_REG, &TextWindowPlacement);
            SaveWindowPlacement(actKey, OUTWINDOW_REG, &OutWindowPlacement);
            SaveWindowPlacement(actKey, PREVIEWWINDOW_REG, &PreviewWindowPlacement);

            HANDLES(RegCloseKey(actKey));
        }

        if (CreateKey(config, RECENT_REG, actKey))
        {
            ClearKey(actKey);
            char buf[10];
            for (int i = 0; i < RECENT_PROJECTS_COUNT; i++)
            {
                if (RecentProjects[i][0] != 0)
                {
                    _itoa_s(i + 1, buf, 10);
                    SetValue(actKey, buf, REG_SZ, RecentProjects[i], strlen(RecentProjects[i]) + 1);
                }
                else
                    break;
            }
            HANDLES(RegCloseKey(actKey));
        }

        HANDLES(RegCloseKey(config));
    }

    return TRUE;
}

BOOL CConfiguration::Load()
{
    HKEY config;
    if (OpenKey(HKEY_CURRENT_USER, DB_ROOT_KEY, config))
    {
        HKEY actKey;

        if (OpenKey(config, CONFIG_REG, actKey))
        {
            GetValue(actKey, CONFIG_FINDTEXTS, REG_DWORD, &FindTexts, sizeof(DWORD));
            GetValue(actKey, CONFIG_FINDSYMBOLS, REG_DWORD, &FindSymbols, sizeof(DWORD));
            GetValue(actKey, CONFIG_FINDORIGINAL, REG_DWORD, &FindOriginal, sizeof(DWORD));
            GetValue(actKey, CONFIG_FINDWORDS, REG_DWORD, &FindWords, sizeof(DWORD));
            GetValue(actKey, CONFIG_FINDCASE, REG_DWORD, &FindCase, sizeof(DWORD));
            GetValue(actKey, CONFIG_IGNOREAMPERSAND, REG_DWORD, &FindIgnoreAmpersand, sizeof(DWORD));
            GetValue(actKey, CONFIG_IGNOREDASH, REG_DWORD, &FindIgnoreDash, sizeof(DWORD));

            LoadHistoryW(actKey, CONFIG_FINDHISTORY_REG, FindHistory, FIND_HISTORY_SIZE);

            GetValue(actKey, COLUMN_TEXTNAME_REG, REG_DWORD, &ColumnNameWidth, sizeof(DWORD));
            GetValue(actKey, COLUMN_TEXTTRANSLATED_REG, REG_DWORD, &ColumnTranslatedWidth, sizeof(DWORD));
            GetValue(actKey, COLUMN_TEXTORIGINAL_REG, REG_DWORD, &ColumnOriginalWidth, sizeof(DWORD));

            GetValue(actKey, CONFIG_RELOADPROJECT, REG_DWORD, &ReloadProjectAtStartup, sizeof(DWORD));

            GetValue(actKey, CONFIG_PREVIEW_EXTENDDIALOGS, REG_DWORD, &PreviewExtendDialogs, sizeof(DWORD));
            GetValue(actKey, CONFIG_PREVIEW_OUTLINECTRLS, REG_DWORD, &PreviewOutlineControls, sizeof(DWORD));
            GetValue(actKey, CONFIG_PREVIEW_FILLCTRLS, REG_DWORD, &PreviewFillControls, sizeof(DWORD));
            GetValue(actKey, CONFIG_EDITOR_OUTLINECTRLS, REG_DWORD, &EditorOutlineControls, sizeof(DWORD));
            GetValue(actKey, CONFIG_EDITOR_FILLCTRLS, REG_DWORD, &EditorFillControls, sizeof(DWORD));

            GetValue(actKey, CONFIG_VLD_DLGHOTKEY, REG_DWORD, &ValidateDlgHotKeyConflicts, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_MENUHOTKEY, REG_DWORD, &ValidateMenuHotKeyConflicts, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_PRINTF, REG_DWORD, &ValidatePrintfSpecifier, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_CONTROLCHARS, REG_DWORD, &ValidateControlChars, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_STRINGSWITHCSV, REG_DWORD, &ValidateStringsWithCSV, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_PLURALSTR, REG_DWORD, &ValidatePluralStr, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_TEXTENDING, REG_DWORD, &ValidateTextEnding, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_HOTKEYS, REG_DWORD, &ValidateHotKeys, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_LAYOUT, REG_DWORD, &ValidateLayout, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_CLIPPING, REG_DWORD, &ValidateClipping, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_CONTROLSTODLG, REG_DWORD, &ValidateControlsToDialog, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_MISALIGNEDCONTROLS, REG_DWORD, &ValidateMisalignedControls, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_DIFFERENTLYSIZEDCONTROLS, REG_DWORD, &ValidateDifferentSizedControls, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_DIFFERENTLYSPACEDCONTROLS, REG_DWORD, &ValidateDifferentSpacedControls, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_STANDARDCONTROLSSIZE, REG_DWORD, &ValidateStandardControlsSize, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_LABELSTOCONTROLSSPACING, REG_DWORD, &ValidateLabelsToControlsSpacing, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_SIZESOFPROPPAGES, REG_DWORD, &ValidateSizesOfPropPages, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_DIALOGCONTROLSID, REG_DWORD, &ValidateDialogControlsID, sizeof(DWORD));
            GetValue(actKey, CONFIG_VLD_HBUTTONSPACING, REG_DWORD, &ValidateHButtonSpacing, sizeof(DWORD));

            GetValue(actKey, CONFIG_AGREEMENT_CONFIRMED_REG, REG_DWORD, &AgreementConfirmed, sizeof(DWORD));

            GetValue(actKey, CONFIG_LASTPROJECT, REG_SZ, LastProject, MAX_PATH);
            GetValue(actKey, CONFIG_MUI_ORIGINAL, REG_SZ, LastMUIOriginal, MAX_PATH);
            GetValue(actKey, CONFIG_MUI_TRANSLATED, REG_SZ, LastMUITranslated, MAX_PATH);

            HANDLES(RegCloseKey(actKey));
        }

        if (OpenKey(config, WINDOWS_REG, actKey))
        {
            if (!LoadWindowPlacement(actKey, FRAMEWINDOW_REG, &FrameWindowPlacement))
            {
                FrameWindowPlacement.length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement(FrameWindow.HWindow, &FrameWindowPlacement);
            }
            if (!LoadWindowPlacement(actKey, TREEWINDOW_REG, &TreeWindowPlacement))
            {
                TreeWindowPlacement.length = 0;
            }
            if (!LoadWindowPlacement(actKey, RHWINDOW_REG, &RHWindowPlacement))
            {
                RHWindowPlacement.length = 0;
            }
            if (!LoadWindowPlacement(actKey, TEXTWINDOW_REG, &TextWindowPlacement))
            {
                TextWindowPlacement.length = 0;
            }
            if (!LoadWindowPlacement(actKey, OUTWINDOW_REG, &OutWindowPlacement))
            {
                OutWindowPlacement.length = 0;
            }
            if (!LoadWindowPlacement(actKey, PREVIEWWINDOW_REG, &PreviewWindowPlacement))
            {
                PreviewWindowPlacement.length = 0;
            }
            HANDLES(RegCloseKey(actKey));
        }

        int i;
        for (i = 0; i < RECENT_PROJECTS_COUNT; i++)
            RecentProjects[i][0] = 0;
        if (OpenKey(config, RECENT_REG, actKey))
        {
            char buf[10];
            for (i = 0; i < RECENT_PROJECTS_COUNT; i++)
            {
                _itoa_s(i + 1, buf, 10);
                if (!GetValue(actKey, buf, REG_SZ, RecentProjects[i], MAX_PATH))
                    break;
            }
            HANDLES(RegCloseKey(actKey));
        }

        HANDLES(RegCloseKey(config));
    }
    return TRUE;
}

void CConfiguration::AddRecentProject(const char* fileName)
{
    int index = FindRecentProject(fileName);
    if (index != -1)
    {
        if (index == 0)
            return;
        for (int i = index; i > 0; i--)
            lstrcpy(RecentProjects[i], RecentProjects[i - 1]);
    }
    else
    {
        for (int i = RECENT_PROJECTS_COUNT - 1; i > 0; i--)
            lstrcpy(RecentProjects[i], RecentProjects[i - 1]);
    }
    lstrcpy(RecentProjects[0], fileName);
}

void CConfiguration::RemoveRecentProject(int index)
{
    if (index >= 0 && index < RECENT_PROJECTS_COUNT)
    {
        for (int i = index; i < RECENT_PROJECTS_COUNT; i++)
            lstrcpy(RecentProjects[i], RecentProjects[i + 1]);
        RecentProjects[RECENT_PROJECTS_COUNT - 1][0] = 0;
    }
}

int CConfiguration::FindRecentProject(const char* fileName)
{
    for (int i = 0; i < RECENT_PROJECTS_COUNT; i++)
    {
        if (_stricmp(fileName, RecentProjects[i]) == 0)
            return i;
    }
    return -1;
}
