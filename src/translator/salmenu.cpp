// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndtree.h"
#include "wndframe.h"
#include "translator.h"
#include "versinfo.h"

#include "wndout.h"
#include "datarh.h"

/* Description of the salmenu.mnu format
MainMenuTemplate=             -- template name
{
IDS_COLUMN_MENU_NAME          -- section 1
IDS_COLUMN_MENU_EXT
IDS_COLUMN_MENU_TIME
IDS_COLUMN_MENU_SIZE
IDS_COLUMN_MENU_ATTR
IDS_MENU_LEFT_SORTOPTIONS

Dialog: IDD_FIND              -- dialog for section 2
IDS_FFMENU_FILES              -- section 2
IDS_FFMENU_FILES_FIND
IDS_FFMENU_EDIT
IDS_FFMENU_VIEW
IDS_FFMENU_OPTIONS
IDS_FFMENU_HELP
}

PluginsDlgRemoveMenu=                  -- template name
{
Control: IDD_PLUGINS IDB_PLUGINREMOVE  -- control linked with section 3 (the button label moves to the menu)
IDS_UNINSTALLUNREGCOMPONENTS           -- section 3
}
*/

enum EnumSalMenuDataParserState
{
    esmpsNoTemplate,
    esmpsTemplateName,
    esmpsTemplateOpenBracket,
    esmpsSectionDialog,
    esmpsSectionControl,
    esmpsSection,
    esmpsSectionSeparator,
    esmpsTemplateCloseBracket,
};

struct CSalMenuDataParserState
{
    EnumSalMenuDataParserState State;
    char TemplateName[500];       // template name, if provided
    WORD SectionDialogID;         // dialog ID for the current section
    WORD SectionControlDialogID;  // control metadata: dialog ID that owns the control
    WORD SectionControlControlID; // control metadata: control ID inside that dialog
    DWORD Errors;

    //  CSalMenuSection *OpenedSection;

    void ResetSectionData()
    {
        SectionDialogID = 0;
        SectionControlDialogID = 0;
        SectionControlControlID = 0;
    }
};

BOOL CData::ProcessSalMenuLine(const char* line, const char* lineEnd, int row, CSalMenuDataParserState* parserState)
{
    wchar_t err[1000];
    char buff[500];
    const char* p = line;

    // Skip leading whitespace.
    while (p < lineEnd && (*p == ' ' || *p == '\t'))
        p++;

    // Trim trailing whitespace.
    while ((lineEnd - 1) > line && (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t'))
        lineEnd--;

    BOOL emptyLine = (p >= lineEnd);

    switch (parserState->State)
    {
    case esmpsNoTemplate:
    {
        if (!emptyLine) // ignore a blank line when no template is open
        {
            // No template is open; expect a line that starts one.
            if (*(lineEnd - 1) != '=')
                return FALSE;
            lstrcpyn(parserState->TemplateName, p, min(500 - 1, (lineEnd - p)));
            parserState->State = esmpsTemplateName;
        }
        break;
    }

    case esmpsTemplateName:
    {
        // Expect the opening brace of the template.
        if (emptyLine || lineEnd > p + 1 || *p != '{')
            return FALSE;
        parserState->State = esmpsTemplateOpenBracket;
        break;
    }

    case esmpsTemplateOpenBracket:
    case esmpsSectionDialog:
    case esmpsSectionControl:
    case esmpsSection:
    case esmpsSectionSeparator:
    {
        if (emptyLine)
        {
            // A blank line after the opening brace or after another blank line inside a section is invalid.
            if (parserState->State == esmpsTemplateOpenBracket || parserState->State == esmpsSectionSeparator)
                return FALSE;

            // Otherwise treat it as a section separator.
            parserState->State = esmpsSectionSeparator;
            parserState->ResetSectionData();
        }
        else
        {
            if (lineEnd == p + 1 && *p == '}')
            {
                // Closing the template.
                parserState->State = esmpsTemplateCloseBracket;
                parserState->ResetSectionData();
            }
            else
            {
                // Does this connect the section with a dialog?
                if (p + 7 < lineEnd && strncmp(p, "Dialog:", 7) == 0)
                {
                    if (parserState->State != esmpsTemplateOpenBracket && parserState->State != esmpsSectionSeparator)
                    {
                        OutWindow.AddLine(L"SalMenu: label Dialog: can be only at the beginning of section", mteError);
                        parserState->Errors++;
                    }
                    else
                    {
                        parserState->State = esmpsSectionDialog;
                        lstrcpyn(buff, p + 7, (lineEnd - (p + 7) + 1));
                        WORD dlgID;
                        if (!DataRH.GetIDForIdentifier(StripWS(buff), &dlgID))
                        {
                            swprintf_s(err, L"SalMenu: cannot find ID for Dialog:%hs", buff);
                            OutWindow.AddLine(err, mteError);
                            parserState->Errors++;
                        }
                        else
                            parserState->SectionDialogID = dlgID;
                    }
                }
                else
                {
                    // Does this attach a control to the section?
                    if (p + 8 < lineEnd && strncmp(p, "Control:", 8) == 0)
                    {
                        if (parserState->State != esmpsTemplateOpenBracket && parserState->State != esmpsSectionSeparator)
                        {
                            OutWindow.AddLine(L"SalMenu: label Control: can be only at the beginning of section", mteError);
                            parserState->Errors++;
                        }
                        else
                        {
                            parserState->State = esmpsSectionControl;
                            lstrcpyn(buff, p + 8, (lineEnd - (p + 8) + 1));
                            StripWS(buff);
                            char* controlID = strchr(buff, ' ');
                            if (controlID == NULL)
                            {
                                swprintf_s(err, L"SalMenu: syntax error on line: Control: %hs", buff);
                                OutWindow.AddLine(err, mteError);
                                parserState->Errors++;
                            }
                            else
                            {
                                *controlID++ = 0;
                                WORD dlgID, ctlID;
                                if (!DataRH.GetIDForIdentifier(StripWS(buff), &dlgID) ||
                                    !DataRH.GetIDForIdentifier(StripWS(controlID), &ctlID))
                                {
                                    swprintf_s(err, L"SalMenu: cannot find ID for dialog: %hs or control: %hs", buff, controlID);
                                    OutWindow.AddLine(err, mteError);
                                    parserState->Errors++;
                                }
                                else
                                {
                                    parserState->SectionControlDialogID = dlgID;
                                    parserState->SectionControlControlID = ctlID;
                                }
                            }
                        }
                    }
                    else
                    {
                        lstrcpyn(buff, p, min(500 - 1, (lineEnd - p + 1)));
                        WORD strID;
                        if (!DataRH.GetIDForIdentifier(buff, &strID))
                        {
                            swprintf_s(err, L"SalMenu: cannot find ID for string %hs", buff);
                            OutWindow.AddLine(err, mteError);
                            parserState->Errors++;
                        }
                        else
                        {
                            if (parserState->State != esmpsSection)
                            {
                                // No section has been created yet; allocate it now.
                                CSalMenuSection* newSection = new CSalMenuSection();
                                if (newSection == NULL)
                                    return FALSE; // lowmem
                                SalMenuSections.Add(newSection);
                                if (!SalMenuSections.IsGood())
                                    return FALSE; // lowmem
                                if (!newSection->SetTemplateName(parserState->TemplateName))
                                    return FALSE; // lowmem
                                newSection->SectionDialogID = parserState->SectionDialogID;
                                newSection->SectionControlDialogID = parserState->SectionControlDialogID;
                                newSection->SectionControlControlID = parserState->SectionControlControlID;
                            }
                            CSalMenuSection* section = SalMenuSections[SalMenuSections.Count - 1];
                            section->StringsID.Add(strID);
                            if (!section->StringsID.IsGood())
                                return FALSE; // lowmem
                        }
                        parserState->State = esmpsSection;
                    }
                }
            }
        }
        break;
    }

    case esmpsTemplateCloseBracket:
    {
        if (!emptyLine)
            return FALSE;
        parserState->State = esmpsNoTemplate;
        break;
    }

    default:
    {
        TRACE_E("uknown state");
        return FALSE;
    }
    }

    return TRUE;
}

BOOL CData::LoadSalMenu(const char* fileName)
{
    if (fileName[0] != 0)
    {
        wchar_t buff[2 * MAX_PATH];
        swprintf_s(buff, L"Reading SALMENU file: %hs", fileName);
        OutWindow.AddLine(buff, mteInfo);
    }
    else
        return TRUE;

    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error opening file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    DWORD salMenuDataSize = GetFileSize(hFile, NULL);
    if (salMenuDataSize == 0xFFFFFFFF)
    {
        char buf[MAX_PATH + 100];
        sprintf_s(buf, "Error reading file %s.", fileName);
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    char* salMenuData = (char*)malloc(salMenuDataSize + 1);
    if (salMenuData == NULL)
    {
        TRACE_E("Nedostatek pameti");
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    DWORD read;
    if (!ReadFile(hFile, salMenuData, salMenuDataSize, &read, NULL) || read != salMenuDataSize)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error reading file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        free(salMenuData);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }
    salMenuData[salMenuDataSize] = 0; // append the terminator

    CSalMenuDataParserState parserState;
    ZeroMemory(&parserState, sizeof(parserState));
    parserState.State = esmpsNoTemplate;

    const char* lineStart = salMenuData;
    const char* lineEnd = salMenuData;
    int line = 1;
    while (lineEnd < salMenuData + salMenuDataSize)
    {
        while (*lineEnd != '\r' && *lineEnd != '\n' && lineEnd < salMenuData + salMenuDataSize)
            lineEnd++;

        if (!ProcessSalMenuLine(lineStart, lineEnd, line, &parserState))
        {
            char errbuf[MAX_PATH + 100];
            sprintf_s(errbuf, "Error reading SalMenu data from file\n"
                              "%s\n"
                              "\n"
                              "Syntax error on line %d",
                      fileName, line);
            MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            free(salMenuData);
            HANDLES(CloseHandle(hFile));
            return FALSE;
        }

        if (lineEnd < salMenuData + salMenuDataSize)
        {
            if (*lineEnd == '\r' && lineEnd + 1 < salMenuData + salMenuDataSize && *(lineEnd + 1) == '\n')
                lineEnd++;
            lineEnd++;
            line++;
            lineStart = lineEnd;
        }
    }
    /*
  for (int i = 0; i < SalMenuSections.Count; i++)
  {
    TRACE_I(SalMenuSections[i]->GetTemplateName());
    if (SalMenuSections[i]->SectionDialogID != 0)
      TRACE_I("###### Dialog:"<<DataRH.GetIdentifier(SalMenuSections[i]->SectionDialogID));
    for (int j = 0; j < SalMenuSections[i]->StringsID.Count; j++)
    {

      TRACE_I(DataRH.GetIdentifier(SalMenuSections[i]->StringsID[j]));
    }
  }
*/
    free(salMenuData);

    if (parserState.Errors > 0)
    {
        wchar_t err[1000];
        swprintf_s(err, L"SalMenu: found %d error(s). Please contact ALTAP for updated .mnu files.", parserState.Errors);
        OutWindow.AddLine(err, mteSummary);
    }

    HANDLES(CloseHandle(hFile));
    return TRUE;
}
