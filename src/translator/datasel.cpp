// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

COLORREF GetSelectionColor(BOOL clipped)
{
    if (clipped)
        return RGB(255, 0, 0);
    else
        return RGB(0, 162, 232);
    // Windows 10 cannot visually distinguish the selected button from the default one, so
    // we stopped using GetSysColor(COLOR_HIGHLIGHT)
}

void CDialogData::SetHighlightControl(int index)
{
    HighlightIndex = index;
}

BOOL CDialogData::IsDialogSelected()
{
    return (Controls.Count > 0 && Controls[0]->Selected);
}

void CDialogData::SetDialogSelected(BOOL value)
{
    if (Controls.Count > 0)
        Controls[0]->Selected = value;
}

int CDialogData::GetSelectedControlsCount()
{
    int count = 0;
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (control->Selected)
            count++;
    }
    return count;
}

CControl*
CDialogData::SelCtrlsGetCurrentControl()
{
    int currentIndex = SelCtrlsGetCurrentControlIndex();
    if (currentIndex == -1)
        return NULL;
    return Controls[currentIndex];
}

int CDialogData::SelCtrlsGetCurrentControlIndex()
{
    if (GetSelectedControlsCount() != 1)
        return -1;
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (control->Selected)
            return i;
    }
    return -1;
}

HWND CDialogData::SelCtrlsGetCurrentControlHandle(HWND hDialog)
{
    int index = SelCtrlsGetCurrentControlIndex();
    if (index == -1)
        return NULL;
    return GetControlHandle(hDialog, index);
}

int CDialogData::SelCtrlsGetFirstControlIndex()
{
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (control->Selected)
            return i;
    }
    return -1;
}

HWND CDialogData::GetControlHandle(HWND hDialog, int index)
{
    HWND hChild = GetWindow(hDialog, GW_CHILD);
    int i = 1; // skip index 0; it stores the dialog title
    do
    {
        if (i == index)
            break;
        hChild = GetWindow(hChild, GW_HWNDNEXT);
        i++;
    } while (i < index && hChild != NULL);

    if (i != index)
        hChild = NULL;
    return hChild;
}

int CDialogData::GetControlIndex(HWND hDialog, HWND hControl)
{
    HWND hChild = GetWindow(hDialog, GW_CHILD);
    int i = 1; // skip index 0; it stores the dialog title
    do
    {
        if (hChild == hControl)
            return i;
        hChild = GetWindow(hChild, GW_HWNDNEXT);
        i++;
    } while (hChild != NULL);
    return -1;
}

void CDialogData::CtrlsMove(BOOL selectedOnly, int deltaX, int deltaY)
{
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (selectedOnly && !control->Selected)
            continue;

        control->TX += deltaX;
        control->TY += deltaY;
    }
}

void CDialogData::SelCtrlsResize(CEdgeEnum edge, int delta, const RECT* originalR)
{
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        if (control->IsIcon()) // do not allow resizing icons
            continue;

        if (control->IsComboBox() && (edge == edgeTop || edge == edgeBottom)) // suppress height changes for combo boxes
            continue;

        if (originalR != NULL)
        {
            control->TX = (short)originalR->left;
            control->TY = (short)originalR->top;
            control->TCX = (short)(originalR->right - originalR->left);
            control->TCY = (short)(originalR->bottom - originalR->top);
        }

        switch (edge)
        {
        case edgeLeft:
        {
            delta = min(delta, control->TCX);
            control->TX += delta;
            control->TCX -= delta;
            break;
        }

        case edgeTop:
        {
            delta = min(delta, control->TCY);
            control->TY += delta;
            control->TCY -= delta;
            break;
        }

        case edgeRight:
        {
            control->TCX += delta;
            if (control->TCX < 0)
                control->TCX = 0;
            break;
        }

        case edgeBottom:
        {
            control->TCY += delta;
            if (control->TCY < 0)
                control->TCY = 0;
            break;
        }
        }
    }
}

void CDialogData::DialogResize(BOOL horizontal, int delta, const RECT* originalR)
{
    if (originalR != NULL)
    {
        TX = (short)originalR->left;
        TY = (short)originalR->top;
        TCX = (short)(originalR->right - originalR->left);
        TCY = (short)(originalR->bottom - originalR->top);
    }

    if (horizontal)
    {
        TCX += delta;
        if (TCX < 1)
            TCX = 1;
    }
    else
    {
        TCY += delta;
        if (TCY < 1)
            TCY = 1;
    }
}

void CDialogData::SelCtrlsSetSize(int width, int height)
{
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        if (control->IsIcon()) // do not allow resizing icons
            continue;

        if (width != -1)
            control->TCX = width;

        if (control->IsComboBox()) // suppress height changes for combo boxes
            continue;

        if (height != -1)
            control->TCY = height;
    }
}

void CDialogData::DialogSetSize(int width, int height)
{
    TCX = width;
    TCY = height;
}

void CDialogData::SelCtrlsAlign(CEdgeEnum edge)
{
    if (GetSelectedControlsCount() < 2)
        return;

    // locate the bounding rectangle
    RECT outline = {0};
    SelCtrlsGetOuterRect(&outline);

    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        switch (edge)
        {
        case edgeLeft:
        {
            control->TX = (WORD)outline.left;
            break;
        }
        case edgeTop:
        {
            control->TY = (WORD)outline.top;
            break;
        }
        case edgeRight:
        {
            control->TX = (WORD)outline.right - control->TCX;
            break;
        }
        case edgeBottom:
        {
            int controlH = control->TCY;
            if (control->IsComboBox())
                controlH = COMBOBOX_BASE_HEIGHT;
            control->TY = (WORD)outline.bottom - controlH;
            break;
        }
        }
    }
}

void CDialogData::SelCtrlsCenter(BOOL horizontal)
{
    if (GetSelectedControlsCount() < 2)
        return;

    // locate the bounding rectangle
    RECT outline = {0};
    SelCtrlsGetOuterRect(&outline);

    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        if (horizontal)
            control->TX = (short)(outline.left + (outline.right - outline.left - control->TCX) / 2);
        else
        {
            short ctrlH = control->TCY;
            if (control->IsComboBox()) // for combo boxes use their collapsed size
                ctrlH = COMBOBOX_BASE_HEIGHT;
            control->TY = (short)(outline.top + (outline.bottom - outline.top - ctrlH) / 2);
        }
    }
}

void CDialogData::SelCtrlsCenterToDialog(BOOL horizontal)
{
    if (GetSelectedControlsCount() < 1)
        return;

    // locate the bounding rectangle
    RECT outline = {0};
    SelCtrlsGetOuterRect(&outline);

    // dialog dimensions
    short dlgW = TCX;
    short dlgH = TCY;

    short deltaX = (short)((dlgW - (outline.right - outline.left)) / 2 - outline.left);
    short deltaY = (short)((dlgH - (outline.bottom - outline.top)) / 2 - outline.top);

    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        if (horizontal)
            control->TX += deltaX;
        else
            control->TY += deltaY;
    }
}

void CDialogData::SelCtrlsSizeToContent(HWND hDialog)
{
    if (GetSelectedControlsCount() < 1)
        return;

    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        CCheckLstItem *multiTextOrComboItem, *checkLstItem;
        ::Data.GetItemFromCheckLst(ID, control->ID, &multiTextOrComboItem, &checkLstItem);
        int idealSizeX, idealSizeY;
        IsControlClipped(this, control, hDialog, &idealSizeX, &idealSizeY,
                         multiTextOrComboItem, checkLstItem);
        if (idealSizeX != -1)
        {
            if (control->IsStaticText(FALSE, TRUE)) // Expand right-aligned texts to the left
                control->TX = control->TX + control->TCX - (idealSizeX + 2) /* artificially widen the control to reduce clipping risk when the font changes */;
            control->TCX = idealSizeX + 2 /* artificially widen the control to reduce clipping risk when the font changes */;
        }
        if (idealSizeY != -1)
            control->TCY = idealSizeY;

        // adjust horizontal separators that follow static text and radio/check boxes
        if (control->IsStaticText(TRUE) ||
            control->IsRadioOrCheckBox())
        {
            CControl* line = NULL;
            BOOL moreLines = FALSE;
            for (int j = 1; j < Controls.Count; j++) // skip index 0; it stores the dialog title
            {
                CControl* c = Controls[j];
                if (c->IsHorizLine() && c->IsTVertContainedIn(control))
                {
                    if (line == NULL)
                        line = c;
                    else
                        moreLines = TRUE;
                }
            }
            if (line != NULL && !moreLines &&
                line->TX + line->TCX > control->TX + control->TCX + 2)
            {
                int lineR = line->TX + line->TCX;
                line->TX = control->TX + control->TCX + 2;
                line->TCX = lineR - line->TX;
            }
        }
    }
}

void CDialogData::SelCtrlsAlignTo(const CAlignToParams* params)
{
    if (GetSelectedControlsCount() < 1)
        return;

    RECT refR;
    if (params->HighlightIndex > 0)
    {
        const CControl* refCtrl = Controls[params->HighlightIndex];
        refR.left = refCtrl->TX;
        refR.top = refCtrl->TY;
        refR.right = refCtrl->TX + refCtrl->TCX;
        int ctrlH = refCtrl->TCY;
        if (refCtrl->IsComboBox()) // for combo boxes use their collapsed size
            ctrlH = COMBOBOX_BASE_HEIGHT;
        refR.bottom = refCtrl->TY + ctrlH;
    }
    else
    {
        refR.left = 0;
        refR.top = 0;
        refR.right = TCX;
        refR.bottom = TCY;
    }

    BOOL move = (params->Operation == atoeMove);
    if (!move && (params->SelPart == atpeHCenter || params->SelPart == atpeVCenter))
    {
        TRACE_E("Wrong parameters!");
        return;
    }

    int ref;
    switch (params->RefPart)
    {
    case atpeTop:
        ref = refR.top;
        break;
    case atpeRight:
        ref = refR.right;
        break;
    case atpeBottom:
        ref = refR.bottom;
        break;
    case atpeLeft:
        ref = refR.left;
        break;
    case atpeHCenter:
        ref = refR.left + (refR.right - refR.left) / 2;
        break;
    case atpeVCenter:
        ref = refR.top + (refR.bottom - refR.top) / 2;
        break;
    }

    RECT selR;
    if (params->MoveAsGroup)
        SelCtrlsGetOuterRect(&selR);

    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        // when moving controls individually, keep their position
        if (!params->MoveAsGroup)
            control->GetTRect(&selR, FALSE);

        if (move)
        {
            switch (params->SelPart)
            {
            case atpeLeft:
                control->TX = (short)(ref + (control->TX - selR.left));
                break;
            case atpeTop:
                control->TY = (short)(ref + (control->TY - selR.top));
                break;
            case atpeRight:
                control->TX = (short)(ref - (selR.right - selR.left) + (control->TX - selR.left));
                break;
            case atpeBottom:
                control->TY = (short)(ref - (selR.bottom - selR.top) + (control->TY - selR.top));
                break;
            case atpeHCenter:
                control->TX = (short)(ref - (selR.right - selR.left) / 2 + (control->TX - selR.left));
                break;
            case atpeVCenter:
                control->TY = (short)(ref - (selR.bottom - selR.top) / 2 + (control->TY - selR.top));
                break;
            }
        }
        else
        {
            // resize
            switch (params->SelPart)
            {
            case atpeTop:
            {
                if (control->IsComboBox() || control->IsHorizLine()) // do not change the height for these controls
                    break;
                if (control->TY > ref)
                {
                    control->TCY += control->TY - ref;
                    control->TY = ref;
                }
                break;
            }

            case atpeRight:
            {
                if (control->TX < ref)
                    control->TCX = ref - control->TX;
                break;
            }

            case atpeBottom:
            {
                if (control->IsComboBox() || control->IsHorizLine()) // do not change the height for these controls
                    break;
                if (control->TY < ref)
                    control->TCY = ref - control->TY;
                break;
            }

            case atpeLeft:
            {
                if (control->TX > ref)
                {
                    control->TCX += control->TX - ref;
                    control->TX = ref;
                }
                break;
            }
            }
        }
    }
}

void CDialogData::SortControlsByEdge(TDirectArray<DWORD>* indexes, BOOL sortByLeftEdge)
{
    BOOL swap;
    do
    {
        swap = FALSE;
        for (int i = 0; i < indexes->Count - 1; i++)
        {
            CControl* control1 = Controls[indexes->At(i)];
            CControl* control2 = Controls[indexes->At(i + 1)];
            if (sortByLeftEdge && control1->TX > control2->TX ||
                !sortByLeftEdge && control1->TY > control2->TY)
            {
                DWORD tmp = indexes->At(i);
                indexes->At(i) = indexes->At(i + 1);
                indexes->At(i + 1) = tmp;
                swap = TRUE;
            }
        }
    } while (swap);
}

void CDialogData::SelCtrlsEqualSpacing(BOOL horizontal, int delta)
{
    int count = GetSelectedControlsCount();
    if (count < 2)
        return;

    // locate the bounding rectangle
    RECT outline = {0};
    SelCtrlsGetOuterRect(&outline);
    short outlineW = (short)(outline.right - outline.left);
    short outlineH = (short)(outline.bottom - outline.top);

    // total width and height of the controls
    int controlsW = 0;
    int controlsH = 0;
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        controlsW += control->TCX;
        short ctrlH = control->TCY;
        if (control->IsComboBox()) // for combo boxes use their collapsed size
            ctrlH = COMBOBOX_BASE_HEIGHT;
        controlsH += ctrlH;
    }

    // spacing between controls (the last one may shift it); spacing can be negative
    short spaceH = (outlineW - controlsW) / (count - 1);
    short spaceV = (outlineH - controlsH) / (count - 1);

    // sort controls by their left/top edge
    TDirectArray<DWORD> indexes(count, 1);
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
        if (Controls[i]->Selected)
            indexes.Add(i);

    SortControlsByEdge(&indexes, horizontal);

    if (delta != -1)
        spaceH = spaceV = delta;

    short pos = 0;
    for (int i = 0; i < indexes.Count; i++)
    {
        CControl* control = Controls[indexes[i]];
        if (horizontal)
        {
            control->TX = (short)outline.left + pos;
            pos += control->TCX + spaceH;
        }
        else
        {
            control->TY = (short)outline.top + pos;
            short ctrlH = control->TCY;
            if (control->IsComboBox()) // for combo boxes use their collapsed size
                ctrlH = COMBOBOX_BASE_HEIGHT;
            pos += ctrlH + spaceV;
        }
    }
}

void CDialogData::SelCtrlsResize(CResizeControlsEnum resize)
{
    if (GetSelectedControlsCount() < 2)
        return;

    WORD maxWidth = 0;
    WORD minWidth = 0;
    WORD maxHeight = 0;
    WORD minHeight = 0;

    // determine boundary values
    BOOL first = TRUE;
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        if (first || control->TCX < minWidth)
            minWidth = control->TCX;
        if (first || control->TCX > maxWidth)
            maxWidth = control->TCX;
        if (first || control->TCY < maxHeight)
            minHeight = control->TCY;
        if (first || control->TCY > maxHeight)
            maxHeight = control->TCY;
        if (first)
            first = FALSE;
    }

    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        if (control->IsIcon()) // suppress resizing for icons
            continue;
        switch (resize)
        {
        case rceWidthLarge:
        {
            control->TCX = maxWidth;
            break;
        }
        case rceWidthSmall:
        {
            control->TCX = minWidth;
            break;
        }
        case rceHeightLarge:
        {
            if (!control->IsComboBox()) // suppress height changes for combo boxes
                control->TCY = maxHeight;
            break;
        }
        case rceHeightSmall:
        {
            if (!control->IsComboBox()) // suppress height changes for combo boxes
                control->TCY = minHeight;
            break;
        }
        }
    }
}

BOOL CDialogData::SelCtrlsGetCurrentControlRect(RECT* r)
{
    if (IsDialogSelected())
    {
        r->left = TX;
        r->top = TY;
        r->right = TX + TCX;
        r->bottom = TY + TCY;
    }
    else
    {
        CControl* control = SelCtrlsGetCurrentControl();
        if (control == NULL)
        {
            TRACE_E("just one control must be selected!");
            return FALSE;
        }
        r->left = control->TX;
        r->top = control->TY;
        r->right = control->TX + control->TCX;
        int ctrlH = control->TCY;
        if (control->IsComboBox()) // for combo boxes use their collapsed size
            ctrlH = COMBOBOX_BASE_HEIGHT;
        r->bottom = control->TY + ctrlH;
    }
    return TRUE;
}

void CDialogData::GetCurrentControlResizeDelta(const RECT* originalR, CEdgeEnum edge, int* deltaX, int* deltaY)
{
    CControl* control = SelCtrlsGetCurrentControl();
    if (control == NULL)
    {
        TRACE_E("just one control must be selected!");
        return;
    }
    *deltaX = 0;
    *deltaY = 0;
    switch (edge)
    {
    case edgeLeft:
    {
        *deltaX = control->TX - originalR->left;
        break;
    }

    case edgeRight:
    {
        *deltaX = control->TX + control->TCX - originalR->right;
        break;
    }

    case edgeTop:
    {
        *deltaY = control->TY - originalR->top;
        break;
    }

    case edgeBottom:
    {
        *deltaY = control->TY + control->TCY - originalR->bottom;
        break;
    }
    }
}

void CDialogData::GetDialogResizeDelta(const RECT* originalR, CEdgeEnum edge, int* deltaX, int* deltaY)
{
    *deltaX = 0;
    *deltaY = 0;
    switch (edge)
    {
    case edgeRight:
    {
        *deltaX = TX + TCX - originalR->right;
        break;
    }

    case edgeBottom:
    {
        *deltaY = TY + TCY - originalR->bottom;
        break;
    }

    default:
    {
        TRACE_E("Wrong edge!");
        break;
    }
    }
}

BOOL CDialogData::SelCtrlsContains(HWND hDialog, HWND hControl)
{
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;
        HWND hChild = GetControlHandle(hDialog, i);
        if (hChild == hControl)
            return TRUE;
    }
    return FALSE;
}

BOOL CDialogData::SelCtrlsContainsControlWithIndex(int index)
{
    if (index < 1 || index >= Controls.Count)
    {
        TRACE_E("index < 1 || index >= Controls.Count!");
        return FALSE;
    }
    CControl* control = Controls[index];
    return control->Selected;
}

void CDialogData::SelectControlByID(int id)
{
    ClearSelection();
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (control->ID == id)
        {
            control->Selected = TRUE;
            return;
        }
    }
}

void CDialogData::SelectNextPrevControl(BOOL next)
{
    int selCount = GetSelectedControlsCount();
    if (selCount > 1)
    {
        for (int i = 1; i < Controls.Count && selCount > 1; i++) // skip index 0; it stores the dialog title
        {
            CControl* control = Controls[i];
            if (control->Selected)
            {
                control->Selected = FALSE;
                selCount--;
            }
        }
    }
    else
    {
        // fetch the currently selected item (-1:none 0..Count-1:control Count:dialog)
        int selectedIndex = -1;
        if (selCount == 1)
        {
            selectedIndex = SelCtrlsGetCurrentControlIndex();
            Controls[selectedIndex]->Selected = FALSE;
        }
        else
        {
            if (IsDialogSelected())
                selectedIndex = Controls.Count;
        }

        if (next)
        {
            if (selectedIndex == -1)
                selectedIndex = 0;
            selectedIndex++;
            if (selectedIndex > Controls.Count)
                selectedIndex = 1; // skip the title
        }
        else
        {
            if (selectedIndex == -1)
                selectedIndex = Controls.Count + 1;
            selectedIndex--;
            if (selectedIndex < 1)
                selectedIndex = Controls.Count; // skip the title
        }
        if (selectedIndex != -1 && selectedIndex != Controls.Count)
        {
            SetDialogSelected(FALSE);
            Controls[selectedIndex]->Selected = TRUE;
        }
        else
        {
            if (selectedIndex == Controls.Count)
                SetDialogSelected(TRUE);
        }
    }
}

void CDialogData::ModifySelection(HWND hDialog, CModifySelectionMode mode, HWND hControl)
{
    SetDialogSelected(mode == emsmSelectDialog);

    if (mode == emsmSelectControl || mode == emsmSelectAllControls || mode == emsmSelectDialog)
        ClearSelection();

    if (mode == emsmSelectDialog)
        return;

    HWND hChild = GetWindow(hDialog, GW_CHILD);
    int i = 1; // 0 - dialog title
    do
    {
        if (hChild == hControl)
        {
            if (mode == emsmToggleControl)
                Controls[i]->Selected = !Controls[i]->Selected;
            else
                Controls[i]->Selected = TRUE;
            break;
        }
        if (mode == emsmSelectAllControls)
            Controls[i]->Selected = TRUE;
        i++;
    } while (hChild = GetWindow(hChild, GW_HWNDNEXT));
}

void CDialogData::ClearSelection()
{
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
        Controls[i]->Selected = FALSE;
}

void CDialogData::SelectControlsGroup(HWND hDialog, HWND hControl, BOOL keepSelection, BOOL forceVertical)
{
    SetDialogSelected(FALSE);
    if (!keepSelection)
        ClearSelection();
    int fromIndex = GetControlIndex(hDialog, hControl);
    if (fromIndex != -1)
    {
        // always include the reference control
        Controls[fromIndex]->Selected = TRUE;

        // try to add controls from the group to the right/left, otherwise use below/above
        BOOL found = FALSE;
        for (int dir = forceVertical ? esdDown : esdRight; dir <= esdUp; dir++)
        {
            int iterator = fromIndex;
            int index;
            while ((index = GetNearestControlInDirection(iterator, (CSearchDirection)dir, FALSE)) != -1)
            {
                if (!BelongsToSameSelectionGroup(fromIndex, index))
                    break;
                if (!ControlsHasSameGroupEdge(fromIndex, index, (CSearchDirection)dir))
                    break;
                Controls[index]->Selected = TRUE;
                found = TRUE;
                iterator = index;
            }
            // stop once controls are found to the left/right
            if (dir == esdLeft && found)
                break;
        }
    }
}

void CDialogData::SelectControlsByCage(BOOL add, const RECT* cageR)
{
    if (!add)
        ClearSelection();

    int selected = 0;

    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        RECT controlR;
        control->GetTRect(&controlR, TRUE);

        RECT myCageR = *cageR;
        if (myCageR.top == myCageR.bottom) // zero-height cages must also work
            myCageR.bottom = myCageR.top + 1;
        if (myCageR.left == myCageR.right) // zero-width cages must work as well
            myCageR.right = myCageR.left + 1;
        if (controlR.left == controlR.right) // zero-width controls must work as well
            controlR.right = controlR.left + 1;
        if (controlR.top == controlR.bottom) // zero-height controls must work as well
            controlR.top = controlR.bottom + 1;

        RECT dummy;
        if (IntersectRect(&dummy, &controlR, &myCageR))
        {
            Controls[i]->Selected = TRUE;
            selected++;
        }
    }

    if (selected)
        SetDialogSelected(FALSE);
}

void OutlineRect(HDC hDC, const RECT* r, BOOL isClipped)
{
    COLORREF clr = GetSelectionColor(isClipped);
    LOGBRUSH lbrush = {BS_SOLID, clr, NULL};
    DWORD us1[] = {2, 1};
    DWORD us2[] = {1, 2};
    HPEN hPen = ExtCreatePen(PS_COSMETIC | PS_USERSTYLE, 1, &lbrush, 2, us1);
    int oldBKMode = SetBkMode(hDC, TRANSPARENT);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hDC, (HBRUSH)GetStockObject(NULL_BRUSH));
    HPEN hOldPen = (HPEN)SelectObject(hDC, hPen);
    Rectangle(hDC, r->left, r->top, r->right, r->bottom);
    SelectObject(hDC, hOldPen);
    DeleteObject(hPen);
    lbrush.lbColor = RGB(255, 255, 255);
    hPen = ExtCreatePen(PS_COSMETIC | PS_USERSTYLE, 1, &lbrush, 2, us2);
    SelectObject(hDC, hPen);
    Rectangle(hDC, r->left, r->top, r->right, r->bottom);
    SetBkMode(hDC, oldBKMode);
    SelectObject(hDC, hOldBrush);
    SelectObject(hDC, hOldPen);
    DeleteObject(hPen);
}

void CDialogData::OutlineControls(HWND hDialog, HDC hDC)
{
    HWND hParent = GetParent(hDialog);
    HWND hChild = GetWindow(hDialog, GW_CHILD);
    int childIndex = 1;
    do
    {
        // draw into the parent; drawing into hwndDlg produces a dashed frame (its DC is likely clipped)
        if (hChild != NULL)
        {
            RECT r;
            GetWindowRect(hChild, &r);
            POINT p1;
            p1.x = r.left;
            p1.y = r.top;
            ScreenToClient(hParent, &p1);
            POINT p2;
            p2.x = r.right;
            p2.y = r.bottom;
            ScreenToClient(hParent, &p2);
            r.left = p1.x;
            r.top = p1.y;
            r.right = p2.x;
            r.bottom = p2.y;

            BOOL isClipped = FALSE;
            // skip clipping detection for controls containing dummy texts
            BOOL isEmpty = (Controls[childIndex]->TWindowName[0] == 0);
            if (!isEmpty)
            {
                CCheckLstItem *multiTextOrComboItem, *checkLstItem;
                ::Data.GetItemFromCheckLst(ID, Controls[childIndex]->ID, &multiTextOrComboItem, &checkLstItem);
                isClipped = IsControlClipped(this, Controls[childIndex], hDialog, NULL, NULL,
                                             multiTextOrComboItem, checkLstItem);
            }
            OutlineRect(hDC, &r, isClipped);
        }
        childIndex++;
    } while (hChild = GetWindow(hChild, GW_HWNDNEXT));
}

void PaintSelectionRect(HDC hDC, const RECT* r, COLORREF clr)
{
    HPEN hPen = CreatePen(PS_SOLID, 2, clr);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hDC, (HBRUSH)GetStockObject(NULL_BRUSH));
    HPEN hOldPen = (HPEN)SelectObject(hDC, hPen);
    Rectangle(hDC, r->left + 1, r->top + 1, r->right, r->bottom);
    SelectObject(hDC, hOldBrush);
    SelectObject(hDC, hOldPen);
    DeleteObject(hPen);
}

void CDialogData::PaintSelection(HWND hDialog, HDC hDC)
{
    HWND hParent = GetParent(hDialog);
    if (IsDialogSelected())
    {
        RECT r;
        GetChildRectPx(hParent, hDialog, &r);
        PaintSelectionRect(hDC, &r, GetSelectionColor(FALSE));
    }
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        // draw into the parent; drawing into hwndDlg produces a dashed frame (its DC is likely clipped)
        HWND hChild = GetControlHandle(hDialog, i);
        if (hChild != NULL)
        {
            RECT r;
            GetChildRectPx(hParent, hChild, &r);

            BOOL isClipped = FALSE;
            CCheckLstItem *multiTextOrComboItem, *checkLstItem;
            ::Data.GetItemFromCheckLst(ID, Controls[i]->ID, &multiTextOrComboItem, &checkLstItem);
            isClipped = IsControlClipped(this, Controls[i], hDialog, NULL, NULL,
                                         multiTextOrComboItem, checkLstItem);
            PaintSelectionRect(hDC, &r, GetSelectionColor(isClipped));
        }
    }
}

void CDialogData::PaintHighlight(HWND hDialog, HDC hDC)
{
    if (HighlightIndex == -1)
        return;

    HWND hParent = GetParent(hDialog);
    COLORREF clr = RGB(255, 127, 39);
    if (HighlightIndex == 0)
    {
        RECT r;
        GetChildRectPx(hParent, hDialog, &r);
        PaintSelectionRect(hDC, &r, clr);
    }
    else
    {
        // draw into the parent; drawing into hwndDlg produces a dashed frame (its DC is likely clipped)
        HWND hChild = GetControlHandle(hDialog, HighlightIndex);
        if (hChild != NULL)
        {
            RECT r;
            GetChildRectPx(hParent, hChild, &r);
            PaintSelectionRect(hDC, &r, clr);
        }
    }
}

void CDialogData::GetChildRectPx(HWND hParent, HWND hChild, RECT* r)
{
    GetWindowRect(hChild, r);
    POINT p1;
    p1.x = r->left;
    p1.y = r->top;
    ScreenToClient(hParent, &p1);
    POINT p2;
    p2.x = r->right;
    p2.y = r->bottom;
    ScreenToClient(hParent, &p2);
    r->left = p1.x;
    r->top = p1.y;
    r->right = p2.x;
    r->bottom = p2.y;
}

BOOL CDialogData::SelCtrlsGetOuterRectPx(HWND hDialog, RECT* r)
{
    BOOL first = TRUE;
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        HWND hChild = GetControlHandle(hDialog, i);
        RECT cR;
        GetChildRectPx(hDialog, hChild, &cR);
        if (first)
        {
            *r = cR;
            first = FALSE;
        }
        else
        {
            if (cR.left < r->left)
                r->left = cR.left;
            if (cR.top < r->top)
                r->top = cR.top;
            if (cR.right > r->right)
                r->right = cR.right;
            if (cR.bottom > r->bottom)
                r->bottom = cR.bottom;
        }
    }
    return (first == FALSE);
}

BOOL CDialogData::SelCtrlsGetOuterRect(RECT* r)
{
    // locate the bounding rectangle
    BOOL first = TRUE;
    for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
    {
        CControl* control = Controls[i];
        if (!control->Selected)
            continue;

        RECT cR;
        control->GetTRect(&cR, FALSE);
        if (first)
        {
            *r = cR;
            first = FALSE;
        }
        else
        {
            if (cR.left < r->left)
                r->left = cR.left;
            if (cR.top < r->top)
                r->top = cR.top;
            if (cR.right > r->right)
                r->right = cR.right;
            if (cR.bottom > r->bottom)
                r->bottom = cR.bottom;
        }
    }
    return (first == FALSE);
}

/*
void
CDialogData::SelCtrlsEnlargeSelectionBox(RECT *box)
{
  for (int i = 1; i < Controls.Count; i++) // skip index 0; it stores the dialog title
  {
    CControl *control = Controls[i];
    if (!control->Selected)
      continue;

    HWND hChild = GetControlHandle(i);
    RECT wr2;
    GetWindowRect(GetParent(HDialog), &wr2);
    RECT r;
    GetWindowRect(hChild, &r);
    r.left -= wr2.left;
    r.top -= wr2.top;
    r.right -= wr2.left;
    r.bottom -= wr2.top;
    if (box->left == 0 && box->top == 0 && box->right == 0 && box->bottom == 0)
      *box = r;
    else
    {
      if (r.left < box->left)
        box->left = r.left;
      if (r.right > box->right)
        box->right = r.right;
      if (r.top < box->top)
        box->top = r.top;
      if (r.bottom > box->bottom)
        box->bottom = r.bottom;
    }
  }
}
*/
