// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndout.h"
#include "datarh.h"
#include "config.h"

//                               [dlg units]
#define DIALOG_MARGIN_WIDTH 7
#define DIALOG_STD_MARGIN_HEIGHT 7
#define DIALOG_WIDE_MARGIN_HEIGHT 14
#define BUTTONS_SPACING 4

//*****************************************************************************
//
// CStringList
//

CStringList::CStringList()
    : Strings(10, 10)
{
}

CStringList::~CStringList()
{
    for (int i = 0; i < Strings.Count; i++)
        free(Strings[i]);
}

BOOL CStringList::AddString(const char* string)
{
    int len = strlen(string);
    char* buff = (char*)malloc(len + 1);
    if (buff == NULL)
    {
        TRACE_E("Low memory!");
        return FALSE;
    }
    strcpy_s(buff, len + 1, string);
    Strings.Add(buff);
    if (!Strings.IsGood())
    {
        TRACE_E("Low memory!");
        free(buff);
        return FALSE;
    }
    return TRUE;
}

BOOL CStringList::GetStrings(char* buff, int size)
{
    char* iter = buff;
    *iter = 0;
    for (int i = 0; i < Strings.Count; i++)
    {
        int strLen = strlen(Strings[i]);
        if (size - (iter - buff) <= strLen + 2)
        {
            TRACE_E("Buffer too small!");
            *iter = 0;
            return FALSE;
        }
        strcpy_s(iter, strLen + 1, Strings[i]);
        iter += strLen;
        *iter = '\n';
        iter++;
    }
    *iter = 0;
    return TRUE;
}

//*****************************************************************************
//
// CDialogData
//

CControl::CControl()
{
    ID = 0;
    Style = 0;
    ExStyle = 0;
    OX = 0;
    OY = 0;
    OCX = 0;
    OCY = 0;
    TX = 0;
    TY = 0;
    TCX = 0;
    TCY = 0;
    ClassName = NULL;
    OWindowName = NULL;
    TWindowName = NULL;
    ExHelpID = 0;
    State = PROGRESS_STATE_UNTRANSLATED;
    Selected = FALSE;
}

CControl::~CControl()
{
    Clean();
}

void CControl::Clean()
{
    if (ClassName != NULL && LOWORD(ClassName) != 0xFFFF)
    {
        free(ClassName);
        ClassName = NULL;
    }
    if (OWindowName != NULL && HIWORD(OWindowName) != 0x0000)
    {
        free(OWindowName);
        OWindowName = NULL;
    }
    if (TWindowName != NULL && HIWORD(TWindowName) != 0x0000)
    {
        free(TWindowName);
        TWindowName = NULL;
    }
}

void CControl::LoadFrom(CControl* src)
{
    Clean();

    ID = src->ID;

    Style = src->Style;
    ExStyle = src->ExStyle;

    OX = src->OX;
    OY = src->OY;
    OCX = src->OCX;
    OCY = src->OCY;

    TX = src->TX;
    TY = src->TY;
    TCX = src->TCX;
    TCY = src->TCY;

    if (LOWORD(src->ClassName) != 0xFFFF)
        ClassName = dupstr(src->ClassName);
    else
        ClassName = src->ClassName;

    if (HIWORD(src->OWindowName) != 0x0000)
        OWindowName = dupstr(src->OWindowName);
    else
        OWindowName = src->OWindowName;

    if (HIWORD(src->TWindowName) != 0x0000)
        TWindowName = dupstr(src->TWindowName);
    else
        TWindowName = src->TWindowName;

    ExHelpID = src->ExHelpID;
    State = src->State;
    Selected = src->Selected;
}

BOOL IsStyleStaticText(DWORD style, BOOL onlyLeft, BOOL onlyRight)
{
    return (!onlyRight && (style & SS_TYPEMASK) == SS_LEFT || // neni ikona, frame, atd.
            !onlyLeft && !onlyRight && (style & SS_TYPEMASK) == SS_CENTER ||
            !onlyLeft && (style & SS_TYPEMASK) == SS_RIGHT ||
            !onlyRight && (style & SS_TYPEMASK) == SS_SIMPLE ||
            !onlyRight && (style & SS_TYPEMASK) == SS_LEFTNOWORDWRAP);
}

BOOL CControl::IsStaticText(BOOL onlyLeft, BOOL onlyRight) const
{
    return ClassName == (wchar_t*)0x0082FFFF &&             // je static
           IsStyleStaticText(Style, onlyLeft, onlyRight) && // neni ikona, frame, atd.
           OCY > 1;                                         // eliminace horizontalnich car (jsou vedene jako textove statiky), do nich se texty nepisou
}

BOOL CControl::IsIcon() const
{
    return ClassName == (wchar_t*)0x0082FFFF && // je static
           (Style & SS_TYPEMASK) == SS_ICON;    // je ikona
}

BOOL CControl::IsWhiteFrame() const
{
    return ClassName == (wchar_t*)0x0082FFFF &&    // je static
           (Style & SS_TYPEMASK) == SS_WHITEFRAME; // je white-frame
}

BOOL CControl::IsComboBox() const
{
    return ClassName == (wchar_t*)0x0085ffff;
}

BOOL CControl::IsEditBox() const
{
    return ClassName == (wchar_t*)0x0081ffff;
}

BOOL CControl::IsGroupBox() const
{
    return ClassName == (wchar_t*)0x0080ffff &&
           (Style & BS_TYPEMASK) == BS_GROUPBOX;
}

BOOL CControl::IsHorizLine() const
{
    return ClassName == (wchar_t*)0x0082FFFF &&    // je static
           (Style & SS_TYPEMASK) == SS_ETCHEDHORZ; // etched-horizontal
}

BOOL IsRadioOrCheckBox(int bt);
BOOL IsRadioBox(int bt);
BOOL IsCheckBox(int bt);

BOOL CControl::IsRadioOrCheckBox() const
{
    return ClassName == (wchar_t*)0x0080FFFF && // je button
           ::IsRadioOrCheckBox(Style);          // je check nebo radio
}

BOOL CControl::IsRadioBox() const
{
    return ClassName == (wchar_t*)0x0080FFFF && // je button
           ::IsRadioBox(Style);
}

BOOL CControl::IsCheckBox() const
{
    return ClassName == (wchar_t*)0x0080FFFF && // je button
           ::IsCheckBox(Style);
}

BOOL IsPushButton(int bt);

BOOL CControl::IsPushButton() const
{
    return ClassName == (wchar_t*)0x0080FFFF && // je button
           ::IsPushButton(Style);               // je push button
}

BOOL CControl::IsTContainedIn(CControl const* c) const
{
    return TX >= c->TX && TX + TCX <= c->TX + c->TCX &&
           TY >= c->TY && TY + TCY <= c->TY + c->TCY;
}

BOOL CControl::IsTVertContainedIn(CControl const* c) const
{
    return TY >= c->TY && TY + TCY <= c->TY + c->TCY;
}

BOOL CControl::ShowInLVWithControls(int i)
{
    return OWindowName != NULL && HIWORD(OWindowName) != 0x0000 &&
           (IsTranslatableControl(OWindowName) || // vse krome ikon a dalsich neprelozitelnych prvku
            i > 0 && IsStaticText());             // u staticu s textem delame vyjimku, je mozne prelozit i prazdny text (napr. "" -> "Prelozil (c) 2010 Ferda" v About dialogu PictView)
}

void CControl::GetTRect(RECT* r, BOOL adjustZeroSize)
{
    r->left = TX;
    r->top = TY;
    // 0 jednotek siroke nebo vysoke controly rozsirime na 1 jednotku
    int w = TCX;
    if (adjustZeroSize && w == 0)
        w = 1;
    int h = TCY;
    if (adjustZeroSize && h == 0)
        h = 1;
    if (IsComboBox()) // u comboboxu chceme jejich "zabaleny" rozmer
        h = COMBOBOX_BASE_HEIGHT;
    r->right = TX + w;
    r->bottom = TY + h;
}

int CControl::GetDistanceFromRect(const RECT* r, CSearchDirection direction)
{
    int distance = -1;
    RECT tR;
    GetTRect(&tR, FALSE);
    switch (direction)
    {
    case esdUp:
    {
        if (r->top >= tR.bottom)
            distance = r->top - tR.bottom;
        break;
    }

    case esdRight:
    {
        if (r->right <= tR.left)
            distance = tR.left - r->right;
        break;
    }

    case esdDown:
    {
        if (r->bottom <= tR.top)
            distance = tR.top - r->bottom;
        break;
    }

    case esdLeft:
    {
        if (r->left >= tR.right)
            distance = r->left - tR.right;
        break;
    }
    }
    return distance;
}

CDialogData::CDialogData()
    : Controls(20, 20)
{
    ID = 0;
    TLangID = 0;
    Style = 0;
    ExStyle = 0;

    OX = 0;
    OY = 0;
    OCX = 0;
    OCY = 0;
    TX = 0;
    TY = 0;
    TCX = 0;
    TCY = 0;

    MenuName = NULL;
    ClassName = NULL;

    FontSize = 0;
    FontName = NULL;

    IsEX = FALSE;
    ExDlgVer = 0;
    ExSignature = 0;
    ExHelpID = 0;
    ExWeight = 0;
    ExItalic = 0;
    ExCharset = 0;

    HighlightIndex = -1;
}

CDialogData::~CDialogData()
{
    Clean();
}

void CDialogData::Clean()
{
    if (MenuName != NULL && HIWORD(MenuName) != 0)
    {
        free(MenuName);
        MenuName = NULL;
    }
    if (ClassName != NULL && HIWORD(ClassName) != 0)
    {
        free(ClassName);
        ClassName = NULL;
    }
    if (FontName != NULL)
    {
        free(FontName);
        FontName = NULL;
    }
    Controls.DestroyMembers();
    Transformation.Clean();
}

void CDialogData::LoadFrom(CDialogData* src, BOOL keepLangID)
{
    WORD oldTLangID = TLangID;

    Clean();

    ID = src->ID;
    if (keepLangID)
        TLangID = oldTLangID;
    else
        TLangID = src->TLangID;

    Style = src->Style;
    ExStyle = src->ExStyle;

    OX = src->OX;
    OY = src->OY;
    OCX = src->OCX;
    OCY = src->OCY;

    TX = src->TX;
    TY = src->TY;
    TCX = src->TCX;
    TCY = src->TCY;

    if (HIWORD(src->MenuName) != 0)
        MenuName = dupstr(src->MenuName); // alokovany retezec
    else
        MenuName = src->MenuName;

    if (HIWORD(src->ClassName) != 0)
        ClassName = dupstr(src->ClassName); // alokovany retezec
    else
        ClassName = src->ClassName;

    FontSize = src->FontSize;
    FontName = dupstr(src->FontName); // alokovany retezec

    IsEX = src->IsEX;
    ExDlgVer = src->ExDlgVer;
    ExSignature = src->ExSignature;
    ExHelpID = src->ExHelpID;
    ExWeight = src->ExWeight;
    ExItalic = src->ExItalic;
    ExCharset = src->ExCharset;

    HighlightIndex = src->HighlightIndex;
    Transformation.CopyFrom(&src->Transformation);

    for (int i = 0; i < src->Controls.Count; i++)
    {
        CControl* control = new CControl;
        control->LoadFrom(src->Controls[i]);
        Controls.Add(control);
    }
}

void CDialogData::LoadSelectionFrom(CDialogData* src)
{
    for (int i = 0; i < src->Controls.Count; i++)
        Controls[i]->Selected = src->Controls.At(i)->Selected;
}

BOOL CDialogData::DoesLayoutChanged(CDialogData* orgDialogData)
{
    BOOL changed = FALSE;
    CDialogData* dataOrg = orgDialogData;
    if (TX != dataOrg->TX)
        changed = TRUE;
    if (TY != dataOrg->TY)
        changed = TRUE;
    if (TCX != dataOrg->TCX)
        changed = TRUE;
    if (TCY != dataOrg->TCY)
        changed = TRUE;
    for (int i = 0; i < Controls.Count; i++)
    {
        CControl* control = Controls[i];
        CControl* controlOrg = dataOrg->Controls[i];
        if (control->TX != controlOrg->TX)
            changed = TRUE;
        if (control->TY != controlOrg->TY)
            changed = TRUE;
        if (control->TCX != controlOrg->TCX)
            changed = TRUE;
        if (control->TCY != controlOrg->TCY)
            changed = TRUE;
    }
    return changed;
}

BOOL IsFilteredDialogStyleSame(DWORD style1, DWORD style2)
{
    DWORD mask = DS_3DLOOK | DS_FIXEDSYS; // do masky pridat styly, u kterych budeme ignorovat jejich rozdilnost
    return (style1 & ~mask) == (style2 & ~mask);
}

BOOL IsEmptyWindowName(const wchar_t* windowName)
{
    return windowName != NULL && HIWORD(windowName) != 0x0000 && windowName[0] == 0;
}

BOOL IsFilteredControlStyleSame(CControl* control, CControl* controlOrg)
{
    if (control->IsStaticText(TRUE) && controlOrg->IsWhiteFrame() &&
        IsEmptyWindowName(control->OWindowName) &&
        IsEmptyWindowName(controlOrg->OWindowName) &&
        IsEmptyWindowName(controlOrg->TWindowName) &&
        control->Style == (controlOrg->Style & ~(WS_BORDER | SS_WHITEFRAME)))
    { // Petr: u staticu pro progress-bar ignorujeme ztratu WS_BORDER | SS_WHITEFRAME (prechod na flat variantu progresu)
        return TRUE;
    }

    DWORD editMask = ES_AUTOHSCROLL;
    if (control->IsEditBox() && controlOrg->IsEditBox() && // Petr: u editu ignorujeme zmenu ES_AUTOHSCROLL, nema zadny vliv na layout
        (control->Style & ~editMask) == (controlOrg->Style & ~editMask))
    {
        return TRUE;
    }
    return control->Style == controlOrg->Style;
}

BOOL CDialogData::DoesLayoutChanged2(CDialogData* orgDialogData)
{
    wchar_t buff[500];
    CDialogData* dataOrg = orgDialogData;
    BOOL changed = FALSE;

    if (ID != dataOrg->ID) // Petr: tohle snad nemuze nastat, ne? Vzdyt se podle ID ty dialogy sparovaly, tak musi byt shodna.
    {
        swprintf_s(buff, L"Dialog %hs: ID changed, original ID:%d, new ID:%d",
                   DataRH.GetIdentifier(ID),
                   dataOrg->ID, ID);
        OutWindow.AddLine(buff, mteError, rteDialog, ID);
        changed = TRUE;
    }

    if (!IsFilteredDialogStyleSame(Style, dataOrg->Style) || ExStyle != dataOrg->ExStyle)
    {
        swprintf_s(buff, L"Dialog %hs: Style or ExStyle changed, original Style:0x%08x ExStyle:0x%08x, new Style:0x%08x ExStyle:0x%08x",
                   DataRH.GetIdentifier(ID), dataOrg->Style, dataOrg->ExStyle, Style, ExStyle);
        OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
        changed = TRUE;
    }
    else
    {
        if (dataOrg->Style != Style)
            dataOrg->Style = Style; // Petr: pri importu se kopiruji komplet data z puvodniho dialogu, tedy nakopiroval by se "spatny" styl, tohle je reseni, i kdyz trochu prasarna
    }

    if (OX != dataOrg->OX || OY != dataOrg->OY || OCX != dataOrg->OCX || OCY != dataOrg->OCY)
    {
        swprintf_s(buff, L"Dialog %hs: layout changed, original X:%d Y:%d W:%d H:%d, new X:%d Y:%d W:%d H:%d",
                   DataRH.GetIdentifier(ID),
                   dataOrg->OX, dataOrg->OY, dataOrg->OCX, dataOrg->OCY,
                   OX, OY, OCX, OCY);
        OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
        changed = TRUE;
    }

    if (HIWORD(MenuName) == 0 && HIWORD(dataOrg->MenuName) != 0 ||
        HIWORD(MenuName) != 0 && HIWORD(dataOrg->MenuName) == 0 ||
        HIWORD(MenuName) == 0 && (DWORD)MenuName != (DWORD)dataOrg->MenuName ||
        HIWORD(MenuName) != 0 && wcscmp(MenuName, dataOrg->MenuName) != 0)
    {
        swprintf_s(buff, L"Dialog %hs: MenuName changed",
                   DataRH.GetIdentifier(ID));
        OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
        changed = TRUE;
    }

    if (HIWORD(ClassName) == 0 && HIWORD(dataOrg->ClassName) != 0 ||
        HIWORD(ClassName) != 0 && HIWORD(dataOrg->ClassName) == 0 ||
        HIWORD(ClassName) == 0 && (DWORD)ClassName != (DWORD)dataOrg->ClassName ||
        HIWORD(ClassName) != 0 && wcscmp(ClassName, dataOrg->ClassName) != 0)
    {
        swprintf_s(buff, L"Dialog %hs: ClassName changed",
                   DataRH.GetIdentifier(ID));
        OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
        changed = TRUE;
    }

    if (FontSize != dataOrg->FontSize)
    {
        swprintf_s(buff, L"Dialog %hs: FontSize changed, original:%d, new %d",
                   DataRH.GetIdentifier(ID), FontSize, dataOrg->FontSize);
        OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
        changed = TRUE;
    }

    if (wcscmp(FontName, dataOrg->FontName) != 0)
    {
        swprintf_s(buff, L"Dialog %hs: FontName changed",
                   DataRH.GetIdentifier(ID));
        OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
        changed = TRUE;
    }

    if (IsEX != dataOrg->IsEX || IsEX && (ExDlgVer != dataOrg->ExDlgVer ||
                                          ExSignature != dataOrg->ExSignature ||
                                          ExHelpID != dataOrg->ExHelpID ||
                                          ExWeight != dataOrg->ExWeight ||
                                          ExItalic != dataOrg->ExItalic ||
                                          ExCharset != dataOrg->ExCharset))
    {
        swprintf_s(buff, L"Dialog %hs: ExDlgVer, ExSignature, ExHelpID, ExWeight, ExItalic, or ExCharset changed", DataRH.GetIdentifier(ID));
        OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
        changed = TRUE;
    }

    if (Controls.Count == dataOrg->Controls.Count)
    {
        for (int i = 0; i < Controls.Count; i++)
        {
            CControl* control = Controls[i];
            CControl* controlOrg = dataOrg->Controls[i];
            BOOL controlChanged = FALSE;

            if (control->ID != controlOrg->ID)
            {
                swprintf_s(buff, L"Dialog %hs, control index %d: ID changed, original ID:%d, new ID:%d",
                           DataRH.GetIdentifier(ID), i, controlOrg->ID, control->ID);
                OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
                changed = TRUE;
            }

            if (control->Style != controlOrg->Style || control->ExStyle != controlOrg->ExStyle)
            {
                if (!IsFilteredControlStyleSame(control, controlOrg) || control->ExStyle != controlOrg->ExStyle)
                {
                    swprintf_s(buff, L"Dialog %hs, control %hs: Style or ExStyle changed, original Style:0x%08x ExStyle:0x%08x, new Style:0x%08x ExStyle:0x%08x",
                               DataRH.GetIdentifier(ID), DataRH.GetIdentifier(control->ID), controlOrg->Style, controlOrg->ExStyle, control->Style, control->ExStyle);
                    OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
                    changed = TRUE;
                }
                else // lisi se jen Style, ale neni to podstatny pro layout, takze to ignorujeme
                {
                    controlOrg->Style = control->Style; // Petr: trochu prasarna: pri importu se kopiruje kompletni puvodni dialog, ale styl potrebujeme z nove verze dialogu, tedy ho prevezmeme do puvodni verze a je to vyresene
                }
            }

            if (control->OX != controlOrg->OX || control->OY != controlOrg->OY ||
                control->OCX != controlOrg->OCX || control->OCY != controlOrg->OCY)
            {
                swprintf_s(buff, L"Dialog %hs, control %hs, layout changed, original X:%d Y:%d W:%d H:%d, new X:%d Y:%d W:%d H:%d",
                           DataRH.GetIdentifier(ID), DataRH.GetIdentifier(control->ID),
                           controlOrg->OX, controlOrg->OY, controlOrg->OCX, controlOrg->OCY,
                           control->OX, control->OY, control->OCX, control->OCY);
                OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
                changed = TRUE;
            }

            if (LOWORD(control->ClassName) == 0xFFFF && LOWORD(controlOrg->ClassName) != 0xFFFF ||
                LOWORD(control->ClassName) != 0xFFFF && LOWORD(controlOrg->ClassName) == 0xFFFF ||
                LOWORD(control->ClassName) == 0xFFFF && (DWORD)control->ClassName != (DWORD)controlOrg->ClassName ||
                LOWORD(control->ClassName) != 0xFFFF && control->ClassName != NULL && controlOrg->ClassName != NULL && wcscmp(control->ClassName, controlOrg->ClassName) != 0)
            {
                swprintf_s(buff, L"Dialog %hs, control %hs: ClassName changed",
                           DataRH.GetIdentifier(ID), DataRH.GetIdentifier(control->ID));
                OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
                changed = TRUE;
            }

            if (control->ExHelpID != dataOrg->ExHelpID)
            {
                swprintf_s(buff, L"Dialog %hs, control %hs: ExHelpID changed",
                           DataRH.GetIdentifier(ID), DataRH.GetIdentifier(control->ID));
                OutWindow.AddLine(buff, mteInfo, rteDialog, ID);
                changed = TRUE;
            }
        }
    }
    else
        changed = TRUE;
    return changed;
}

int CDialogData::FindControlIndex(WORD id, BOOL isDlgTitle)
{
    if (isDlgTitle) // ID nazvu dialogu srovnavame jen ID nazvu dialogu (ID se muze mezi controly opakovat)
    {
        if (Controls.Count > 0 && Controls[0]->ID == id)
            return 0;
    }
    else
    {
        for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
        {
            if (Controls[i]->ID == id)
                return i;
        }
    }
    return -1;
}

BOOL CDialogData::GetControlIndexInLV(int controlIndex, int* lvIndex)
{
    if (Controls.Count > 0 && (Controls[0]->OWindowName == NULL || Controls[0]->OWindowName[0] == 0)) // prazdny nazev dialogu
        *lvIndex = 0;
    else
        *lvIndex = 1;
    for (int x = 1; x < Controls.Count; x++) // 0 je nazev dialogu, zacneme tedy od 1
    {
        CControl* control = Controls[x];
        if (!control->ShowInLVWithControls(x))
            continue;
        if (x == controlIndex)
            return TRUE;
        (*lvIndex)++;
    }
    return FALSE;
}

BOOL CDialogData::LoadDialog(WORD* oBuff, WORD* tBuff, BOOL* showStyleWarning,
                             BOOL* showExStyleWarning, BOOL* showDlgEXWarning,
                             CData* data)
{
    char errtext[3000];
    wchar_t buff[500];
    DWORD oStyle = GET_DWORD(oBuff);
    oBuff += 2;
    DWORD tStyle = GET_DWORD(tBuff);
    tBuff += 2;
    DWORD oStyleBackup = oStyle;

    // DLGTEMPLATE http://msdn.microsoft.com/en-us/library/ms645394%28VS.85%29.aspx
    // DLGTEMPLATEEX http://msdn.microsoft.com/en-us/library/ms645398%28VS.85%29.aspx
    BOOL oIsEX = oStyle == 0xffff0001;
    BOOL tIsEX = tStyle == 0xffff0001;

    if (oIsEX != tIsEX && !data->MUIMode)
    {
        if (*showDlgEXWarning)
        {
            sprintf_s(errtext, "Original dialog is stored as DLGTEMPLATE and translated dialog is "
                               "stored as DLGTEMPLATEEX or vice versa. It may cause problems, translated dialog "
                               "should use the same dialog resource format as original dialog. To correct this, "
                               "use menu File / Save All. Other similar errors will be only added to Output "
                               "Window (no other messagebox).\n\n"
                               "Dialog ID: %s\n"
                               "Original Dialog Format: %s\n"
                               "Translated Dialog Format: %s",
                      DataRH.GetIdentifier(ID),
                      oIsEX ? "DLGTEMPLATEEX" : "DLGTEMPLATE",
                      tIsEX ? "DLGTEMPLATEEX" : "DLGTEMPLATE");
            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            *showDlgEXWarning = FALSE;
        }
        swprintf_s(buff, L"Dialog %hs: original is %s, translated is %s",
                   DataRH.GetIdentifier(ID),
                   oIsEX ? L"DLGTEMPLATEEX" : L"DLGTEMPLATE",
                   tIsEX ? L"DLGTEMPLATEEX" : L"DLGTEMPLATE");
        OutWindow.AddLine(buff, mteError, rteDialog, ID, 0);
    }

    DWORD oHelpID = 0;
    if (oIsEX)
    {
        oHelpID = GET_DWORD(oBuff);
        oBuff += 2;
    }
    if (tIsEX)
        tBuff += 2;

    DWORD oExStyle = GET_DWORD(oBuff);
    oBuff += 2;
    DWORD tExStyle = GET_DWORD(tBuff);
    tBuff += 2;

    if (oIsEX)
    {
        oStyle = GET_DWORD(oBuff);
        oBuff += 2;
    }
    if (tIsEX)
    {
        tStyle = GET_DWORD(tBuff);
        tBuff += 2;
    }

    WORD oCount = GET_WORD(oBuff);
    oBuff++;
    WORD tCount = GET_WORD(tBuff);
    tBuff++;

    if ((oStyle & ~(DS_3DLOOK | (oIsEX != tIsEX ? DS_FIXEDSYS : 0))) !=
            (tStyle & ~(DS_3DLOOK | (oIsEX != tIsEX ? DS_FIXEDSYS : 0))) || // rozdily v DS_3DLOOK ignorujeme, pokud je jeden dialog DLG a druhy DLG-EX, ignorujeme jeste taky DS_FIXEDSYS
        oExStyle != tExStyle ||
        oCount != tCount)
    {
        if (!data->MUIMode)
        {
            sprintf_s(errtext, "Styles or numbers of controls of original and translated dialogs are different.\n\n"
                               "dialog ID: %d\n"
                               "original dialog styles: 0x%08X\n"
                               "translated dialog styles: 0x%08X\n"
                               "original dialog ex-styles: 0x%08X\n"
                               "translated dialog ex-styles: 0x%08X\n"
                               "number of controls in original dialog: %d\n"
                               "number of controls in translated dialog: %d",
                      ID, oStyle, tStyle, oExStyle, tExStyle, oCount, tCount);
            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    }

    IsEX = oIsEX;
    ExDlgVer = LOWORD(oStyleBackup);
    ExSignature = HIWORD(oStyleBackup);
    ExHelpID = oHelpID;

    Style = oStyle;
    ExStyle = oExStyle;

    OX = GET_WORD(oBuff);
    oBuff++;
    TX = GET_WORD(tBuff);
    tBuff++;

    OY = GET_WORD(oBuff);
    oBuff++;
    TY = GET_WORD(tBuff);
    tBuff++;

    OCX = GET_WORD(oBuff);
    oBuff++;
    TCX = GET_WORD(tBuff);
    tBuff++;

    OCY = GET_WORD(oBuff);
    oBuff++;
    TCY = GET_WORD(tBuff);
    tBuff++;

    // menu name
    switch (GET_WORD(tBuff))
    {
    case 0x0000:
    {
        tBuff++;
        break;
    }

    case 0xffff:
    {
        tBuff += 2;
        break;
    }

    default:
    {
        tBuff += wcslen((wchar_t*)tBuff) + 1;
        break;
    }
    }
    switch (GET_WORD(oBuff))
    {
    case 0x0000:
    {
        MenuName = NULL;
        oBuff++;
        break;
    }

    case 0xffff:
    {
        MenuName = (wchar_t*)(UINT)GET_WORD(oBuff + 1);
        oBuff += 2;
        break;
    }

    default:
    {
        WORD oLen = (WORD)wcslen((wchar_t*)oBuff);
        if (!DecodeString((wchar_t*)oBuff, oLen, &MenuName))
            return FALSE;
        oBuff += oLen + 1;
        break;
    }
    }

    // class name
    switch (GET_WORD(tBuff))
    {
    case 0x0000:
    {
        tBuff++;
        break;
    }

    case 0xffff:
    {
        tBuff += 2;
        break;
    }

    default:
    {
        tBuff += wcslen((wchar_t*)tBuff) + 1;
        break;
    }
    }
    switch (GET_WORD(oBuff))
    {
    case 0x0000:
    {
        ClassName = NULL;
        oBuff++;
        break;
    }

    case 0xffff:
    {
        ClassName = (wchar_t*)(UINT)GET_WORD(oBuff + 1);
        oBuff += 2;
        break;
    }

    default:
    {
        WORD oLen = (WORD)wcslen((wchar_t*)oBuff);
        if (!DecodeString((wchar_t*)oBuff, oLen, &ClassName))
            return FALSE;
        oBuff += oLen + 1;
        break;
    }
    }

    // window name
    // nulty control bude nazev dialogu
    CControl* control = new CControl();
    if (control == NULL)
    {
        TRACE_E("LOW MEMORY");
        return FALSE;
    }

    WORD oLen = (WORD)wcslen((wchar_t*)oBuff);
    WORD tLen = (WORD)wcslen((wchar_t*)tBuff);

    control->ID = ID;
    if (tLen > 0 && !data->MUIMode)
        control->State = data->QueryTranslationState(tteDialogs, 0, ID, (wchar_t*)oBuff, (wchar_t*)tBuff);
    else
        control->State = PROGRESS_STATE_TRANSLATED; // prazdny retezec je "prelozeny"

    if (!DecodeString((wchar_t*)oBuff, oLen, &control->OWindowName))
        return FALSE;
    if (!DecodeString((wchar_t*)tBuff, tLen, &control->TWindowName))
        return FALSE;

    if (control->OWindowName == NULL || control->TWindowName == NULL)
    {
        TRACE_E("LOW MEMORY");
        delete control;
        return FALSE;
    }
    Controls.Add(control);
    if (!Controls.IsGood())
    {
        delete control;
        TRACE_E("LOW MEMORY");
        return FALSE;
    }

    oBuff += oLen + 1;
    tBuff += tLen + 1;

    if (tStyle & DS_SETFONT)
    {
        FontSize = GET_WORD(oBuff);
        oBuff++;
        tBuff++;

        if (oIsEX)
        {
            ExWeight = GET_WORD(oBuff);
            oBuff++;
            WORD w = GET_WORD(oBuff);
            oBuff++;
            ExItalic = LOBYTE(w);
            ExCharset = HIBYTE(w);
        }
        if (tIsEX)
            tBuff += 2;

        oLen = (WORD)wcslen((wchar_t*)oBuff);
        tLen = (WORD)wcslen((wchar_t*)tBuff);
        if (!DecodeString((wchar_t*)oBuff, oLen, &FontName))
            return FALSE;
        oBuff += oLen + 1;
        tBuff += tLen + 1;
    }

    // nacteme controls
    for (int i = 0; i < oCount; i++)
    {
        // control je zarovnany na DWORDy
        oBuff = (WORD*)((((int)oBuff) + 3) & ~3);
        tBuff = (WORD*)((((int)tBuff) + 3) & ~3);

        control = new CControl();
        if (control == NULL)
        {
            TRACE_E("LOW MEMORY");
            return FALSE;
        }

        DWORD tControlStyle;

        if (oIsEX)
        {
            control->ExHelpID = GET_DWORD(oBuff);
            oBuff += 2;
        }
        else
        {
            control->Style = GET_DWORD(oBuff);
            oBuff += 2;
        }
        if (tIsEX)
            tBuff += 2;
        else
        {
            tControlStyle = GET_DWORD(tBuff);
            tBuff += 2;
        }

        BOOL sameExStyles = GET_DWORD(oBuff) == GET_DWORD(tBuff);
        control->ExStyle = GET_DWORD(oBuff);
        oBuff += 2;
        DWORD tControlExStyle = GET_DWORD(tBuff);
        tBuff += 2;

        if (oIsEX)
        {
            control->Style = GET_DWORD(oBuff);
            oBuff += 2;
        }
        if (tIsEX)
        {
            tControlStyle = GET_DWORD(tBuff);
            tBuff += 2;
        }
        BOOL sameStyles = control->Style == tControlStyle;

        control->OX = GET_WORD(oBuff);
        oBuff++;
        control->TX = GET_WORD(tBuff);
        tBuff++;

        control->OY = GET_WORD(oBuff);
        oBuff++;
        control->TY = GET_WORD(tBuff);
        tBuff++;

        control->OCX = GET_WORD(oBuff);
        oBuff++;
        control->TCX = GET_WORD(tBuff);
        tBuff++;

        control->OCY = GET_WORD(oBuff);
        oBuff++;
        control->TCY = GET_WORD(tBuff);
        tBuff++;

        WORD oControlID;

        if (oIsEX)
        {
            if (HIWORD(GET_DWORD(oBuff)) != 0)
            {
                if (!data->MUIMode)
                {
                    sprintf_s(errtext, "32-bit IDs are not supported");
                    MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                    delete control;
                    return FALSE;
                }
            }
            oControlID = LOWORD(GET_DWORD(oBuff));
            oBuff += 2;
        }
        else
        {
            oControlID = GET_WORD(oBuff);
            oBuff++;
        }

        if (data->MUIMode)
            oControlID = i + 1 + 1000; // pokud jsem ID sazel od 1, osklive se na nekterych resourcech podelala okna, failnulo vytvoreni dialogu a dochazelo k prepisum pameti; zrejme se jedna o ID vyhrazena tlacitkum OK a spol??

        if (tIsEX)
        {
            if (HIWORD(GET_DWORD(tBuff)) != 0)
            {
                if (!data->MUIMode)
                {
                    sprintf_s(errtext, "32-bit IDs are not supported");
                    MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                    delete control;
                    return FALSE;
                }
            }
            control->ID = LOWORD(GET_DWORD(tBuff));
            tBuff += 2;
        }
        else
        {
            control->ID = GET_WORD(tBuff);
            tBuff++;
        }

        if (data->MUIMode)
            control->ID = i + 1 + 1000; // pokud jsem ID sazel od 1, osklive se na nekterych resourcech podelala okna, failnulo vytvoreni dialogu a dochazelo k prepisum pameti; zrejme se jedna o ID vyhrazena tlacitkum OK a spol??

        BOOL sameIDs = oControlID == control->ID;

        // class name
        wchar_t* oClassName;
        if (GET_WORD(oBuff) == 0xffff)
        {
            oClassName = (wchar_t*)(UINT)GET_DWORD(oBuff);
            oBuff += 2;
        }
        else
        {
            WORD len = (WORD)wcslen((wchar_t*)oBuff);
            if (!DecodeString((wchar_t*)oBuff, len, &oClassName))
            {
                delete control;
                return FALSE;
            }
            oBuff += len + 1;
        }

        if (GET_WORD(tBuff) == 0xffff)
        {
            control->ClassName = (wchar_t*)(UINT)GET_DWORD(tBuff);
            tBuff += 2;
        }
        else
        {
            WORD len = (WORD)wcslen((wchar_t*)tBuff);
            if (!DecodeString((wchar_t*)tBuff, len, &control->ClassName))
            {
                delete control;
                if (LOWORD(oClassName) != 0xFFFF)
                    free(oClassName);
                return FALSE;
            }
            tBuff += len + 1;
        }

        BOOL sameClasses = oClassName == control->ClassName ||
                           LOWORD(oClassName) != 0xFFFF && LOWORD(control->ClassName) != 0xFFFF &&
                               wcscmp(oClassName, control->ClassName) == 0;

        // window name
        wchar_t* oWinName = (wchar_t*)L"";
        if (GET_WORD(oBuff) == 0xffff)
        {
            control->OWindowName = (wchar_t*)(UINT)GET_WORD(oBuff + 1);
            oBuff += 2;
        }
        else
        {
            WORD len = (WORD)wcslen((wchar_t*)oBuff);
            if (!DecodeString((wchar_t*)oBuff, len, &control->OWindowName))
            {
                delete control;
                if (LOWORD(oClassName) != 0xFFFF)
                    free(oClassName);
                return FALSE;
            }
            oWinName = control->OWindowName;
            oBuff += len + 1;
        }

        wchar_t* tWinName = (wchar_t*)L"";
        if (GET_WORD(tBuff) == 0xffff)
        {
            control->TWindowName = (wchar_t*)(UINT)GET_WORD(tBuff + 1);
            tBuff += 2;
        }
        else
        {
            WORD len = (WORD)wcslen((wchar_t*)tBuff);
            if (!DecodeString((wchar_t*)tBuff, len, &control->TWindowName))
            {
                delete control;
                if (LOWORD(oClassName) != 0xFFFF)
                    free(oClassName);
                return FALSE;
            }
            tWinName = control->TWindowName;
            tBuff += len + 1;
        }

        if (!data->MUIMode && (GET_WORD(tBuff) == 0xffff || HIWORD(control->TWindowName) != 0x0000 && wcslen(control->TWindowName) > 0))
            control->State = data->QueryTranslationState(tteDialogs, Controls.Count, ID, oWinName, tWinName);
        else
            control->State = PROGRESS_STATE_TRANSLATED; // prazdny retezec je "prelozeny"

        // control data
        WORD oExtraCount = GET_WORD(oBuff);
        if (oExtraCount && !data->MUIMode)
        {
            sprintf_s(errtext, "Control creation data are not supported");
            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            delete control;
            if (LOWORD(oClassName) != 0xFFFF)
                free(oClassName);
            return FALSE;
        }
        oBuff += 1 + ((oExtraCount + 1) / 2);

        WORD tExtraCount = GET_WORD(tBuff);
        if (tExtraCount && !data->MUIMode)
        {
            sprintf_s(errtext, "Control creation data are not supported");
            MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            delete control;
            if (LOWORD(oClassName) != 0xFFFF)
                free(oClassName);
            return FALSE;
        }
        tBuff += 1 + ((tExtraCount + 1) / 2);

        if (!sameIDs || !sameClasses)
        {
            wchar_t oClass[50];
            wchar_t tClass[50];
            wchar_t* oStrClass;
            wchar_t* tStrClass;
            if (LOWORD(oClassName) == 0xFFFF)
            {
                swprintf_s(oClass, L"0x%04X", (UINT)HIWORD(oClassName));
                oStrClass = oClass;
            }
            else
                oStrClass = oClassName;
            if (LOWORD(control->ClassName) == 0xFFFF)
            {
                swprintf_s(tClass, L"0x%04X", (UINT)HIWORD(control->ClassName));
                tStrClass = tClass;
            }
            else
                tStrClass = control->ClassName;

            if (!data->MUIMode)
            {
                sprintf_s(errtext, "Changed dialog structure (order of controls).\n\n"
                                   "Dialog ID: %s\n"
                                   "Original control ID: %s\n"
                                   "Translated control ID: %s\n"
                                   "Same IDs: %s\n"
                                   "Same Styles: %s\n"
                                   "Same Ex-Styles: %s\n"
                                   "Original control class: %ls\n"
                                   "Translated control class: %ls\n"
                                   "Same Classes: %s\n",
                          DataRH.GetIdentifier(ID), DataRH.GetIdentifier(oControlID),
                          DataRH.GetIdentifier(control->ID),
                          (sameIDs ? "yes" : "no"),
                          (sameStyles ? "yes" : "no"),
                          (sameExStyles ? "yes" : "no"),
                          oStrClass, tStrClass,
                          (sameClasses ? "yes" : "no"));
                MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            }
            delete control;
            if (LOWORD(oClassName) != 0xFFFF)
                free(oClassName);
            return FALSE;
        }

        if (LOWORD(oClassName) != 0xFFFF)
            free(oClassName);

        Controls.Add(control);
        if (!Controls.IsGood())
        {
            delete control;
            TRACE_E("LOW MEMORY");
            return FALSE;
        }

        if (!sameStyles && !Data.MUIMode)
        {
            BOOL isError = (control->Style & ~(WS_TABSTOP | WS_GROUP)) != (tControlStyle & ~(WS_TABSTOP | WS_GROUP));
            if (isError && *showStyleWarning && !data->MUIMode)
            {
                sprintf_s(errtext, "Changed control style in dialog (not only WS_TABSTOP and WS_GROUP). "
                                   "Other similar errors will be only added to Output Window (no other messagebox).\n\n"
                                   "Dialog ID: %s\n"
                                   "Control ID: %s\n"
                                   "Original Style=0x%08X\n"
                                   "Translated Style=0x%08X",
                          DataRH.GetIdentifier(ID), DataRH.GetIdentifier(oControlID),
                          control->Style, tControlStyle);
                MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                *showStyleWarning = FALSE;
            }
            swprintf_s(buff, L"Dialog %hs: %s %hs: org=0x%08X, tr=0x%08X",
                       DataRH.GetIdentifier(ID),
                       isError ? L"diff. style of" : L"TAB-stop or Group changed for",
                       DataRH.GetIdentifier(oControlID),
                       control->Style, tControlStyle);
            int lvIndex2;
            BOOL useLVIndex = GetControlIndexInLV(Controls.Count - 1, &lvIndex2);
            OutWindow.AddLine(buff, mteError, rteDialog, ID, useLVIndex ? lvIndex2 : 0, useLVIndex ? 0 : oControlID);
        }
        if (!sameExStyles)
        {
            if (*showExStyleWarning)
            {
                sprintf_s(errtext, "Changed control ex-style in dialog. "
                                   "Other similar errors will be only added to Output Window (no other messagebox).\n\n"
                                   "Dialog ID: %s\n"
                                   "Control ID: %s\n"
                                   "Original Ex-Style=0x%08X\n"
                                   "Translated Ex-Style=0x%08X",
                          DataRH.GetIdentifier(ID), DataRH.GetIdentifier(oControlID),
                          control->ExStyle, tControlExStyle);
                MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                *showExStyleWarning = FALSE;
            }
            swprintf_s(buff, L"Dialog %hs: diff. ex-style of %hs: org=0x%08X, tr=0x%08X",
                       DataRH.GetIdentifier(ID),
                       DataRH.GetIdentifier(oControlID),
                       control->ExStyle, tControlExStyle);
            int lvIndex2;
            BOOL useLVIndex = GetControlIndexInLV(Controls.Count - 1, &lvIndex2);
            OutWindow.AddLine(buff, mteError, rteDialog, ID,
                              useLVIndex ? lvIndex2 : 0, useLVIndex ? 0 : oControlID);
        }
    }
    /*
//    sprintf_s(errtext, "DLGTEMPLATEEX templates are not supported");
//    MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
//    return FALSE;

    DWORD oHelpID = GET_DWORD(oBuff); oBuff += 2;
    DWORD tHelpID = GET_DWORD(tBuff); tBuff += 2;

    DWORD oExStyle = GET_DWORD(oBuff); oBuff += 2;
    DWORD tExStyle = GET_DWORD(tBuff); tBuff += 2;

    // prepiseme style
    oStyle = GET_DWORD(oBuff); oBuff += 2;
    tStyle = GET_DWORD(tBuff); tBuff += 2;

    WORD oCount = GET_WORD(oBuff); oBuff++;
    WORD tCount = GET_WORD(tBuff); tBuff++;

    if (!DialogPropertiesCheck(ID, oCount, tCount, oStyle, tStyle, oExStyle, tExStyle))
      return FALSE;

    Style = oStyle;
    ExStyle = oExStyle;

    OX = GET_WORD(oBuff); oBuff++;
    TX = GET_WORD(tBuff); tBuff++;

    OY = GET_WORD(oBuff); oBuff++;
    TY = GET_WORD(tBuff); tBuff++;

    OCX = GET_WORD(oBuff); oBuff++;
    TCX = GET_WORD(tBuff); tBuff++;

    OCY = GET_WORD(oBuff); oBuff++;
    TCY = GET_WORD(tBuff); tBuff++;

    WORD oCount = GET_WORD(oBuff); oBuff++;
    WORD tCount = GET_WORD(tBuff); tBuff++;


  }
  */
    return TRUE;
}

DWORD
CDialogData::PrepareTemplate(WORD* buff, BOOL addProgress, BOOL forPreview, BOOL extendDailog)
{
    if (Controls.Count < 1)
    {
        TRACE_E("Uknown window name");
        return 0;
    }

    // prvni control je virtualni (nazev dialogu)

    WORD* p = buff;
    if (IsEX)
    {
        PUT_WORD(p, ExDlgVer);
        p++;
        PUT_WORD(p, ExSignature);
        p++;
        PUT_DWORD(p, ExHelpID);
        p += 2;
        PUT_DWORD(p, ExStyle);
        p += 2;
        PUT_DWORD(p, Style);
        p += 2;
    }
    else
    {
        PUT_DWORD(p, Style);
        p += 2;
        PUT_DWORD(p, ExStyle);
        p += 2;
    }
    PUT_WORD(p, Controls.Count - 1);
    p++;
    PUT_WORD(p, TX);
    p++;
    PUT_WORD(p, TY);
    p++;
    WORD* dialogWidth = p;
    PUT_WORD(p, TCX);
    p++;
    WORD* dialogHeight = p;
    PUT_WORD(p, TCY);
    p++;

    // menu name
    if (MenuName == NULL || forPreview) // pro preview nepropojime s menu, protoze by se nepodarilo dialog vytvorit
    {
        PUT_WORD(p, 0);
        p++;
    }
    else if (HIWORD(MenuName) == 0)
    {
        PUT_WORD(p, 0xffff);
        p++;
        PUT_WORD(p, LOWORD(MenuName));
        p++;
    }
    else
    {
        EncodeString(MenuName, (wchar_t**)&p);
    }

    // class name
    if (ClassName == NULL || forPreview) // ve win jsem potkal dialogy, ktere mely nejaky custom class a nebylo mozne je nasledne vytvorit pro preview
    {
        PUT_WORD(p, 0);
        p++;
    }
    else if (HIWORD(ClassName) == 0)
    {
        PUT_WORD(p, 0xffff);
        p++;
        PUT_WORD(p, LOWORD(ClassName));
        p++;
    }
    else
    {
        EncodeString(ClassName, (wchar_t**)&p);
    }

    // window name
    EncodeString(Controls[0]->TWindowName, (wchar_t**)&p);

    if (Style & DS_SETFONT)
    {
        PUT_WORD(p, FontSize);
        p++;
        if (IsEX)
        {
            PUT_WORD(p, ExWeight);
            p++;
            WORD w = MAKEWORD(ExItalic, ExCharset);
            PUT_WORD(p, w);
            p++;
        }
        EncodeString(FontName, (wchar_t**)&p);
    }

    // controls

    RECT maxControlsRect; // pri preview "nafoukneme" dialog tak, aby byly vydet i prvky mimo (pouzivame to jak my, tak hojne Honza Patera)
    ZeroMemory(&maxControlsRect, sizeof(maxControlsRect));
    for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        CControl* control = Controls[i];
        *p = 0;
        *(p + 1) = 0;
        // zarovnat na DWORDy
        p = (WORD*)((((int)p) + 3) & ~3);

        if (IsEX)
        {
            PUT_DWORD(p, control->ExHelpID);
            p += 2;
            PUT_DWORD(p, control->ExStyle);
            p += 2;
            PUT_DWORD(p, control->Style);
            p += 2;
        }
        else
        {
            PUT_DWORD(p, control->Style);
            p += 2;
            PUT_DWORD(p, control->ExStyle);
            p += 2;
        }
        PUT_WORD(p, control->TX);
        p++;
        PUT_WORD(p, control->TY);
        p++;
        if (forPreview && control->IsIcon() && Data.IgnoreIconSizeIconIsSmall(ID, control->ID))
        {
            PUT_WORD(p, 10);
            p++;
            PUT_WORD(p, 10);
            p++;
        }
        else
        {
            PUT_WORD(p, control->TCX);
            p++;
            PUT_WORD(p, control->TCY);
            p++;
        }
        PUT_WORD(p, control->ID);
        p++;
        if (IsEX)
        {
            PUT_WORD(p, 0);
            p++;
        }

        if (forPreview)
        {
            int ctrlX = control->TX;
            int ctrlW = control->TCX;
            int ctrlY = control->TY;
            int ctrlH = control->TCY;
            if (control->IsComboBox()) // u comboboxu chceme jejich "zabaleny" rozmer
                ctrlH = COMBOBOX_BASE_HEIGHT;
            if (ctrlX + ctrlW > maxControlsRect.right)
                maxControlsRect.right = ctrlX + ctrlW;
            if (ctrlY + ctrlH > maxControlsRect.bottom)
                maxControlsRect.bottom = ctrlY + ctrlH;
        }

        // class name
        if (control->ClassName == NULL)
        {
            PUT_DWORD(p, 0);
            p += 2;
        }
        else if (LOWORD(control->ClassName) == 0xFFFF)
        {
            PUT_WORD(p, 0xFFFF);
            p++;
            PUT_WORD(p, HIWORD(control->ClassName));
            p++;
        }
        else
        {
            wchar_t* className = control->ClassName;
            if (forPreview)
            {
                // pro ucely preview je potreba, aby vsechny class childu byly registrovany
                // snadnejsi pro nas je zmenit class v takovem pripade na static nez ho registrovat
                // pokud se ho casem rozhodneme registrovat, muzeme si do nej posadit treba napis s nazvem tridy
                WNDCLASSW wc;
                if (!GetClassInfoW(HInstance, className, &wc))
                {
                    TRACE_IW(L"Window Class substitution for dialog preview. Original: " << className << L" substituion:" << L"static");
                    className = (wchar_t*)L"static";
                }
            }
            EncodeString(className, (wchar_t**)&p);
        }

        // window name
        if (HIWORD(control->TWindowName) == 0x0000)
        {
            PUT_WORD(p, 0xFFFF);
            p++;
            PUT_WORD(p, LOWORD(control->TWindowName));
            p++;
        }
        else
        {
            EncodeString(control->TWindowName, (wchar_t**)&p);
        }

        // control data
        PUT_WORD(p, 0x0000);
        p++;
    }

    if (forPreview && extendDailog)
    {
        if (maxControlsRect.right > *dialogWidth)
            *dialogWidth = (WORD)maxControlsRect.right;
        if (maxControlsRect.bottom > *dialogHeight)
            *dialogHeight = (WORD)maxControlsRect.bottom;
    }

    return (p - buff) * 2;
}

void CDialogData::TemplateAddRemoveStyles(WORD* buff, DWORD addStyles, DWORD removeStyles)
{
    if (IsEX)
    {
        DLGTEMPLATEEX* templ = (DLGTEMPLATEEX*)buff;
        templ->style |= addStyles;
        templ->style &= ~removeStyles;
    }
    else
    {
        LPDLGTEMPLATE templ = (LPDLGTEMPLATE)buff;
        templ->style |= addStyles;
        templ->style &= ~removeStyles;
    }
}

void CDialogData::TemplateSetPos(WORD* buff, short x, short y)
{
    if (IsEX)
    {
        DLGTEMPLATEEX* templ = (DLGTEMPLATEEX*)buff;
        templ->x = x;
        templ->y = y;
    }
    else
    {
        LPDLGTEMPLATE templ = (LPDLGTEMPLATE)buff;
        templ->x = x;
        templ->y = y;
    }
}

int CDialogData::GetNearestControlInDirection(int fromIndex, CSearchDirection direction, BOOL wideScan)
{
    if (fromIndex == 0)
        TRACE_E("fromIndex == 0");

    RECT fR;
    RECT fRProjected; // obdelnik prodlouzeny do nekonecna ve smeru hledani
    Controls[fromIndex]->GetTRect(&fR, TRUE);
    fRProjected = fR;

    switch (direction)
    {
    case esdUp:
        fRProjected.top = LONG_MIN;
        break;
    case esdRight:
        fRProjected.right = LONG_MAX;
        break;
    case esdDown:
        fRProjected.bottom = LONG_MAX;
        break;
    case esdLeft:
        fRProjected.left = LONG_MIN;
        break;
    }

    if (wideScan)
    {
        // obdelnik rozsirime az k hranicim dialogu
        if (direction == esdRight || direction == esdLeft)
        {
            fRProjected.top = 0;
            fRProjected.bottom = TCY;
            if (direction == esdRight)
                fRProjected.left = fR.right + 1;
            else
                fRProjected.right = fR.left - 1;
        }
        else
        {
            fRProjected.left = 0;
            fRProjected.right = TCX;
            if (direction == esdDown)
                fRProjected.top = fR.bottom + 1;
            else
                fRProjected.bottom = fR.top - 1;
        }
    }
    else
    {
        // prodlouzeny obdelnik zuzime na jednu jednotku kvuli dialogum jako IDD_CFGPAGE_GENERAL,
        // kde mezi checkboxy mame napriklad edit line, ktera nam prerusovala vyber skupiny
        if (direction == esdRight || direction == esdLeft)
        {
            fRProjected.bottom = fRProjected.top + 1;
        }
        else
        {
            if (Controls[fromIndex]->IsStaticText(FALSE, TRUE))
                fRProjected.left = fRProjected.right - 1; // right-aligned static
            else
                fRProjected.right = fRProjected.left + 1;
        }
    }

    int distance = -1;
    int distanceIndex = -1;
    for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        if (i == fromIndex)
            continue;
        CControl* ctrl = Controls[i];
        RECT cR;
        ctrl->GetTRect(&cR, TRUE);
        RECT dummy;
        if (IntersectRect(&dummy, &cR, &fRProjected))
        {
            int d = ctrl->GetDistanceFromRect(&fR, direction);
            if (d >= 0 && (d < distance || distance == -1))
            {
                distance = d;
                distanceIndex = i;
            }
        }
    }
    return distanceIndex;
}

BOOL CDialogData::GetFooterSeparatorAndButtons(TDirectArray<DWORD>* footer)
{
    footer->DetachMembers();
    // v prvni fazi prohledame vsechny prvky, ktere nejsou push button a urcime jejich nejspodnejsi hranu
    // pro push buttons dohledame nejspodnejsi Y pozici
    int ctrlBottom = -1;
    int buttonsY = -1;
    for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        CControl* ctrl = Controls[i];
        if (!ctrl->IsPushButton())
        {
            int ctrlY = ctrl->TY;
            int ctrlH = ctrl->TCY;
            if (ctrl->IsComboBox()) // u comboboxu chceme jejich "zabaleny" rozmer
                ctrlH = COMBOBOX_BASE_HEIGHT;
            if (ctrlY + ctrlH >= ctrlBottom)
                ctrlBottom = ctrlY + ctrlH;
        }
        else
        {
            if (ctrl->TY > buttonsY)
                buttonsY = ctrl->TY;
        }
    }

    // ve druhe fazi hledame vsechny push button prvky, ktere lezi pod spodni hranou,
    // jsou umistene na buttonsY souradnici a maji shodne rozestupy
    TDirectArray<DWORD> indexes(10, 10);
    for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        CControl* ctrl = Controls[i];
        if (ctrl->IsPushButton() && ctrl->TY > ctrlBottom && ctrl->TY == buttonsY)
            indexes.Add(i);
    }

    // omerime vzdalenosti mezi tlacitky, v nekterych dialozich je spodni rada tlacitek
    // roztrzena na dve casti (jedna zarovnana vlevo, jedna vpravo) - takovou verzi automaticky
    // neucesavame
    BOOL spacingIsOK = TRUE;
    if (indexes.Count > 2)
    {
        // prvky seradime zleva doprava
        SortControlsByEdge(&indexes, TRUE);
        int firstSpace = -1;
        // zkontrolujeme, ze jsou mezi nimy konstantni mezery
        for (int i = 1; i < indexes.Count; i++)
        {
            int space = Controls[indexes[i]]->TX - (Controls[indexes[i - 1]]->TX + Controls[indexes[i - 1]]->TCX);
            if (firstSpace == -1)
            {
                firstSpace = space;
            }
            else
            {
                // tolerujeme malou odchylku mezi tlacitky +- 1 dlg unit
                if (space < firstSpace - 1 || space > firstSpace + 1)
                    spacingIsOK = FALSE;
            }
        }
    }

    // pokud jsme nasli tlacitka, ale nemaji shodne mezery, nechame zobrazit varovani
    if (indexes.Count > 0 && !spacingIsOK)
        return FALSE;

    if (indexes.Count > 0)
    {
        // podivame se, zda nad tlacitky lezi vodorovny oddelovac a pokud ano, pridame ho na prvni pozici
        int topIndex = GetNearestControlInDirection(indexes[0], esdUp, TRUE);
        if (topIndex != -1 && Controls[topIndex]->IsHorizLine())
        {
            // nesmi mit vlevo text
            if (GetNearestControlInDirection(topIndex, esdLeft, FALSE) == -1)
                footer->Add(topIndex);
        }

        for (int i = 0; i < indexes.Count; i++)
            footer->Add(indexes[i]);
    }

    return TRUE;
}

void CDialogData::AdjustFooter(BOOL wide, TDirectArray<DWORD>* footer)
{
    if (footer->Count == 0)
        return;

    int marginHeight = wide ? DIALOG_WIDE_MARGIN_HEIGHT : DIALOG_STD_MARGIN_HEIGHT;

    // dohledame posledni prvek nad patickou
    int bottomCtrl = GetNearestControlInDirection(footer->At(0), esdUp, TRUE);
    if (bottomCtrl == -1)
        return;

    RECT cr;
    Controls[bottomCtrl]->GetTRect(&cr, FALSE);
    int y = cr.bottom + marginHeight;
    int firstBtnIndex = 0;
    if (Controls[footer->At(0)]->IsHorizLine())
    {
        CControl* sep = Controls[footer->At(0)];
        sep->TY = y;
        sep->TX = DIALOG_MARGIN_WIDTH;
        sep->TCX = TCX - DIALOG_MARGIN_WIDTH - 2;
        firstBtnIndex++;
        y += 1 + DIALOG_STD_MARGIN_HEIGHT;
    }

    int x = TCX - DIALOG_MARGIN_WIDTH;
    for (int i = footer->Count - 1; i >= firstBtnIndex; i--)
    {
        CControl* ctrl = Controls[footer->At(i)];
        ctrl->TX = x - ctrl->TCX;
        ctrl->TY = y;
        x -= ctrl->TCX + BUTTONS_SPACING;
    }

    int btnHeight = Controls[footer->At(footer->Count - 1)]->TCY;
    y += btnHeight + DIALOG_STD_MARGIN_HEIGHT;
    TCY = y;
}

BOOL CDialogData::BelongsToSameSelectionGroup(int fromIndex, int testIndex)
{
    if (fromIndex == 0 || testIndex == 0)
    {
        TRACE_E("fromIndex == 0 || testIndex == 0");
        return FALSE;
    }

    CControl* fromCtrl = Controls[fromIndex];
    CControl* testCtrl = Controls[testIndex];

    // pokud se ClassName lisi (retezce nebo IDcka), nepatri prvky do stejne skupiny
    if (LOWORD(fromCtrl->ClassName) != 0xFFFF && LOWORD(testCtrl->ClassName) != 0xFFFF)
    {
        if (wcscmp(fromCtrl->ClassName, testCtrl->ClassName) != 0)
            return FALSE;
    }
    else
    {
        if (fromCtrl->ClassName != testCtrl->ClassName)
            return FALSE;
    }

    // zde jsou ClassName obou prvku shodne, budeme testovat styly
    if (fromCtrl->IsStaticText(TRUE, FALSE) != testCtrl->IsStaticText(TRUE, FALSE))
        return FALSE;
    if (fromCtrl->IsStaticText(FALSE, TRUE) != testCtrl->IsStaticText(FALSE, TRUE))
        return FALSE;
    if (fromCtrl->IsIcon() != testCtrl->IsIcon())
        return FALSE;
    if (fromCtrl->IsWhiteFrame() != testCtrl->IsWhiteFrame())
        return FALSE;
    if (fromCtrl->IsHorizLine() != testCtrl->IsHorizLine())
        return FALSE;
    if (fromCtrl->IsGroupBox() != testCtrl->IsGroupBox())
        return FALSE;
    if (fromCtrl->IsPushButton() != testCtrl->IsPushButton())
        return FALSE;
    if (fromCtrl->IsCheckBox() != testCtrl->IsCheckBox())
        return FALSE;
    if (fromCtrl->IsRadioBox() != testCtrl->IsRadioBox())
        return FALSE;
    if (fromCtrl->IsEditBox() != testCtrl->IsEditBox())
        return FALSE;
    return TRUE;
}

BOOL CDialogData::ControlsHasSameGroupEdge(int fromIndex, int testIndex, CSearchDirection direction)
{
    if (fromIndex == 0 || testIndex == 0)
    {
        TRACE_E("fromIndex == 0 || testIndex == 0");
        return FALSE;
    }

    CControl* fromCtrl = Controls[fromIndex];
    CControl* testCtrl = Controls[testIndex];

    if (direction == esdRight || direction == esdLeft)
    {
        return fromCtrl->TY == testCtrl->TY;
    }
    else
    {
        if (fromCtrl->IsStaticText(FALSE, TRUE))
            return fromCtrl->TX + fromCtrl->TCX == testCtrl->TX + testCtrl->TCX; // right-aligned static
        else
            return fromCtrl->TX == testCtrl->TX;
    }
}

BOOL CDialogData::HasOuterControls()
{
    for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        CControl* ctrl = Controls[i];
        RECT cr;
        ctrl->GetTRect(&cr, FALSE);
        if (cr.left >= TCX || cr.top >= TCY)
            return TRUE;
    }
    return FALSE;
}

BOOL CDialogData::GetControlsRect(BOOL ignoreSeparators, BOOL ignoreOuterControls, RECT* r)
{
    BOOL first = TRUE;
    for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        CControl* ctrl = Controls[i];

        if (ctrl->IsHorizLine() && ignoreSeparators)
            continue;

        RECT cr;
        ctrl->GetTRect(&cr, FALSE);
        if (cr.left >= TCX || cr.top >= TCY && ignoreOuterControls)
            continue;

        if (first)
        {
            *r = cr;
            first = FALSE;
        }
        else
        {
            if (r->left > cr.left)
                r->left = cr.left;
            if (r->top > cr.top)
                r->top = cr.top;
            if (r->right < cr.right)
                r->right = cr.right;
            if (r->bottom < cr.bottom)
                r->bottom = cr.bottom;
        }
    }
    return (first != TRUE);
}

BOOL CDialogData::CanNormalizeLayout()
{
    for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        CControl* ctrl = Controls[i];
        RECT cr;
        ctrl->GetTRect(&cr, FALSE);
        // pokud control spodni hranou presahuje pres TCY, normalizace neni nakodovana
        if (cr.bottom > TCY)
            return FALSE;
    }
    return TRUE;
}

void CDialogData::AdjustSeparatorsWidth(CStringList* errors)
{
    char buff[1000];
    for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        CControl* ctrl = Controls[i];
        if (!ctrl->IsHorizLine())
            continue;
        // pokud vpravo od separatoru nalezneme element, informujeme
        int rightIndex = GetNearestControlInDirection(i, esdRight, FALSE);
        if (rightIndex != -1)
        {
            sprintf_s(buff, "Cannot adjust separator <%d>. Other element found on the right side.", ctrl->ID);
            errors->AddString(buff);
            continue;
        }
        // pokud je pravy konec separatory prilis daleko od prave hrany dialogu, informujeme
        if (ctrl->TX + ctrl->TCX < TCX - DIALOG_MARGIN_WIDTH * 2)
        {
            sprintf_s(buff, "Cannot adjust separator <%d>. It is too far from the right dialog box side.", ctrl->ID);
            errors->AddString(buff);
            continue;
        }
        // pokud vlevo od separatoru nic neni a separator zacina priblizne na hrane dialogu,
        // usadime jeho levou pozici presne na margin
        int leftIndex = GetNearestControlInDirection(i, esdLeft, FALSE);
        if (leftIndex == -1)
        {
            // pokud je leva strana separatoru prilis vzdalena od leve hrany dialogu, informujeme
            if (ctrl->TX > DIALOG_MARGIN_WIDTH * 2)
            {
                sprintf_s(buff, "Cannot adjust separator <%d>. It is too far from the left dialog box side.", ctrl->ID);
                errors->AddString(buff);
                continue;
            }
            int delta = ctrl->TX - DIALOG_MARGIN_WIDTH;
            ctrl->TX = DIALOG_MARGIN_WIDTH;
        }
        ctrl->TCX = TCX - ctrl->TX - DIALOG_MARGIN_WIDTH + 1;
    }
}

void CDialogData::NormalizeLayout(BOOL wide, CStringList* errors)
{
    int marginHeight = wide ? DIALOG_WIDE_MARGIN_HEIGHT : DIALOG_STD_MARGIN_HEIGHT;
    // v prvni fazi najdeme rozmery obdelniku opsaneho kolem prvku dialogu
    // ignorujeme vodorovne separatory a prvky lezici za hranici dialogu (pouzivame pro ruzne real-time zmeny layoutu)
    RECT r;
    GetControlsRect(TRUE, TRUE, &r);
    CtrlsMove(FALSE, DIALOG_MARGIN_WIDTH - r.left, marginHeight - r.top);
    SIZE dlgSize;
    dlgSize.cx = r.right - r.left + 2 * DIALOG_MARGIN_WIDTH;
    dlgSize.cy = r.bottom - r.top + (marginHeight + DIALOG_STD_MARGIN_HEIGHT);
    // property pages nikdy nezmensujeme, protoze je udrzujeme stejne velike kvuli pohodlnesimu
    // layoutovani (clovek vidi, jaky prostor ma diky nejvetsi strance k dispozici)
    if (Style & DS_CONTROL)
    {
        dlgSize.cx = max(TCX, dlgSize.cx);
        dlgSize.cy = max(TCY, dlgSize.cy);
    }
    // tato funkce je volana pouze pro dialogy bez prvku mimo hranici dialogu, pripadne s prvky za jeho pravou hranici
    // prvkum lezicim vpravo vnutime jejich puvodni offset od prave hrany dialogu
    for (int i = 1; i < Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        CControl* ctrl = Controls[i];
        RECT cr;
        ctrl->GetTRect(&cr, FALSE);
        if (cr.left >= TCX)
            ctrl->TX = (short)dlgSize.cx + (ctrl->TX - TCX) - (DIALOG_MARGIN_WIDTH - (short)r.left);
    }
    // nova velikost dialogu
    DialogSetSize(dlgSize.cx, dlgSize.cy);
    // uceseme paticku dialogu (spodni rada tlacitek, pripadne separator nad nima)

    TDirectArray<DWORD> footerIndexes(10, 10);
    if (!GetFooterSeparatorAndButtons(&footerIndexes))
        errors->AddString("Cannot normalize footer because of uneven spaced buttons.");
    if (footerIndexes.Count > 0)
        AdjustFooter(wide, &footerIndexes);

    // protahneme vodorovne separatory az k prave hrane dialogu (minus margin)
    AdjustSeparatorsWidth(errors);
}

void CDialogData::LoadOriginalLayout()
{
    TX = OX;
    TY = OY;
    TCX = OCX;
    TCY = OCY;
    for (int i = 0; i < Controls.Count; i++)
    {
        CControl* control = Controls[i];
        control->TX = control->OX;
        control->TY = control->OY;
        control->TCX = control->OCX;
        control->TCY = control->OCY;
    }
}

//*****************************************************************************
//
// CData
//

BOOL CData::SaveDialogs(HANDLE hUpdateRes)
{
    BYTE buff[200000];

    for (int i = 0; i < DlgData.Count; i++)
    {
        CDialogData* dlgData = DlgData[i];
        DWORD size = dlgData->PrepareTemplate((WORD*)buff, TRUE, FALSE, FALSE);
        BOOL result = TRUE;
        if (dlgData->TLangID != MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)) // resource neni "neutral", musime ho smaznout, aby ve vyslednem .SLG nebyly dialogy dva
        {
            result = UpdateResource(hUpdateRes, RT_DIALOG, MAKEINTRESOURCE(dlgData->ID),
                                    dlgData->TLangID,
                                    NULL, 0);
        }
        if (result)
        {
            result = UpdateResource(hUpdateRes, RT_DIALOG, MAKEINTRESOURCE(dlgData->ID),
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                                    buff, size);
        }
        if (!result)
            return FALSE;
    }
    return TRUE;
}

BOOL CData::DialogsAddTranslationStates()
{
    for (int i = 0; i < DlgData.Count; i++)
    {
        CDialogData* dlgData = DlgData[i];
        if (wcslen(dlgData->Controls[0]->TWindowName) > 0 &&
            !Data.AddTranslationState(tteDialogs, 0, dlgData->ID, dlgData->Controls[0]->State))
        {
            return FALSE;
        }
        for (int j = 1; j < dlgData->Controls.Count; j++)
        {
            CControl* control = dlgData->Controls[j];
            if ((HIWORD(control->TWindowName) == 0x0000 || wcslen(control->TWindowName) > 0) &&
                !Data.AddTranslationState(tteDialogs, j, dlgData->ID, control->State))
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}

int CData::FindDialogData(WORD id)
{
    for (int i = 0; i < DlgData.Count; i++)
        if (id == DlgData[i]->ID)
            return i;
    return -1;
}
