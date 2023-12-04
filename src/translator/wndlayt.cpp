// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "translator.h"
#include "wndlayt.h"
#include "wndframe.h"
#include "wndtext.h"
#include "wndprev.h"
#include "config.h"
#include "datarh.h"
#include "bitmap.h"
#include "trlipc.h"
#include "dialogs.h"

const char* LAYOUTWINDOW_NAME = "Layout";

//*****************************************************************************
//
// CLayoutEditor
//

void GetMaxTranslatedRect(CDialogData* data, RECT* rOut)
{
    RECT r;
    r.left = 0;
    r.top = 0;
    r.right = data->TX + data->TCX;
    r.bottom = data->TX + data->TCX;
    for (int i = 0; i < data->Controls.Count; i++)
    {
        CControl* control = data->Controls[i];
        if (control->TX + control->TCX > r.right)
            r.right = control->TX + control->TCX;
        if (control->TY + control->TCY > r.bottom)
            r.bottom = control->TY + control->TCY;
    }
    *rOut = r;
}

CLayoutEditor::CLayoutEditor()
    : CWindow(ooStatic), TransformStack(100, 100)
{
    CacheBitmap = NULL;
    TempBitmap = NULL;
    CageBitmap = NULL;
    HDialogHolder = NULL;
    HDialog = NULL;
    CurrentDialog = -1;
    PostponedControlSelect = -1;
    DraggingMode = edmNone;
    OriginalDialogData = NULL;
    ShiftPressedOnMouseDown = FALSE;
    RepeatArrow = -1;
    LastRepeatArrow = -1;
    AlignToMode = FALSE;
    KeepAlignToMode = FALSE;
}

CLayoutEditor::~CLayoutEditor()
{
    if (CacheBitmap != NULL)
        delete CacheBitmap;
    if (TempBitmap != NULL)
        delete TempBitmap;
    if (CageBitmap != NULL)
        delete CageBitmap;
}

void CLayoutEditor::Open(CDialogData* dialogData, int selectedControlID)
{
    OriginalDialogData = dialogData;
    CDialogData* copy = new CDialogData();
    copy->LoadFrom(dialogData);
    TransformStack.Add(copy);
    CurrentDialog = 0;
    PreviewMode = FALSE;
    DialogOffsetX = 0;
    DialogOffsetY = 0;
    CTransformation transformation;
    transformation.SetTransformationWithNoParam(eloSelect);
    dialogData = NewTransformation(&transformation);
    dialogData->SelectControlByID(selectedControlID);
    dialogData->StoreSelectionToTransformation();
    if (SharedMemoryCopy != NULL)
    {
        //DebugBreak();
        RebuildDialog(); // inicializuje HDialog, ktery potrebujeme pro LoadTransformStackStream
        LoadTransformStackStream(((BYTE*)SharedMemoryCopy) + sizeof(CSharedMemory));
    }
}

BOOL CLayoutEditor::Close()
{
    // zjistime, zda se template zmenil
    if (TransformStack.Count > 0)
    {
        CDialogData* data = TransformStack[CurrentDialog];
        CDialogData* dataOrg = OriginalDialogData;
        if (data->DoesLayoutChanged(dataOrg))
        {
            DWORD ret = MessageBox(HWindow, "The dialog box layout has changed.\nDo you want to use new layout?", "Question", MB_YESNOCANCEL | MB_ICONQUESTION);
            if (ret == IDCANCEL)
                return FALSE;
            if (ret == IDYES)
            {
                OriginalDialogData->LoadFrom(data);
                Data.RemoveRelayout(data->ID);
                Data.SetDirty();
                Data.SLGSignature.SLTDataChanged();
            }
        }
    }
    return TRUE;
}

void CLayoutEditor::UpdateBuffer()
{
    RECT r;
    GetClientRect(HWindow, &r);
    RECT updateRect = r;

    HDC hDC = HANDLES(GetDC(HWindow));

    int width = max(1, r.right);
    int height = max(1, r.bottom);
    if (CacheBitmap == NULL)
    {
        CacheBitmap = new CBitmap();
        CacheBitmap->CreateBmp(hDC, width, height);
        TempBitmap = new CBitmap();
        TempBitmap->CreateBmp(hDC, width, height);
    }
    else
    {
        CacheBitmap->Enlarge(width, height);
        TempBitmap->Enlarge(width, height);
    }

    SendMessage(LayoutWindow->DialogHolder.HWindow, WM_PRINT, (WPARAM)CacheBitmap->HMemDC, PRF_CLIENT | PRF_CHILDREN | PRF_OWNED | PRF_ERASEBKGND);
    CDialogData* dialogData = CurrentDialog != -1 ? TransformStack[CurrentDialog] : NULL;
    if (Config.EditorOutlineControls && dialogData != NULL)
        dialogData->OutlineControls(HDialog, CacheBitmap->HMemDC);
    if (Config.EditorOutlineControls)
        PaintSelectionGuidelines(CacheBitmap->HMemDC);
    if (dialogData != NULL)
    {
        dialogData->PaintSelection(HDialog, CacheBitmap->HMemDC);
        if (AlignToMode)
            dialogData->PaintHighlight(HDialog, CacheBitmap->HMemDC);
    }

    //  SetWindowPos(HWindow, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOREDRAW);
    //  ValidateRect(hEditor, NULL);

    BitBlt(hDC, updateRect.left, updateRect.top, updateRect.right - updateRect.left, updateRect.bottom - updateRect.top, CacheBitmap->HMemDC, updateRect.left, updateRect.top, SRCCOPY);

    if ((PreviewMode && PreviewModeShowExitHint) || AlignToMode)
    {
        RECT tr;
        GetWindowRect(LayoutWindow->LayoutEditor.HDialog, &tr);
        POINT p;
        p.x = tr.left;
        p.y = tr.bottom + 10;
        ScreenToClient(HWindow, &p);
        const char* text;
        if (PreviewMode)
            text = "Preview mode, click to return to edit mode.";
        else
            text = "Align To: Choose reference item.";
        SetBkMode(hDC, TRANSPARENT);
        TextOut(hDC, p.x, p.y, text, strlen(text));
    }

    // zobrazeni changedRect -- pro ladici ucely
    //  if (changedRect != NULL)
    //  {
    //      HDC hDC = HANDLES(GetDC(HWindow));
    //      FillRect(hDC, &updateRect, (HBRUSH)(COLOR_HIGHLIGHTTEXT + 1));
    //      HANDLES(ReleaseDC(HWindow, hDC));
    //  }

    HANDLES(ReleaseDC(HWindow, hDC));
}

BOOL CLayoutEditor::HasUndo()
{
    return CurrentDialog > 0;
}

void CLayoutEditor::OnUndo()
{
    if (HasUndo())
    {
        // pokud se mezi Undo/Redo stavy menil pouze vyber (rozmery zustaly stejne),
        // tento krok preskocime, viz Undo/Redo v Adobe Illustrator
        while (CurrentDialog > 1)
        {
            if (TransformStack[CurrentDialog]->DoesLayoutChanged(TransformStack[CurrentDialog - 1]))
                break;
            CurrentDialog--;
        }
        CurrentDialog--;
        RebuildDialog();
    }
}

BOOL CLayoutEditor::HasRedo()
{
    return (CurrentDialog >= 0 && CurrentDialog < TransformStack.Count - 1);
}

void CLayoutEditor::OnRedo()
{
    if (HasRedo())
    {
        // pokud se mezi Undo/Redo stavy menil pouze vyber (rozmery zustaly stejne),
        // tento krok preskocime, viz Undo/Redo v Adobe Illustrator
        while (CurrentDialog < TransformStack.Count - 2)
        {
            if (TransformStack[CurrentDialog]->DoesLayoutChanged(TransformStack[CurrentDialog + 1]))
                break;
            CurrentDialog++;
        }
        CurrentDialog++;
        RebuildDialog();
    }
}

CDialogData*
CLayoutEditor::NewTransformation(CTransformation* transformation)
{
    CDialogData* ret = NULL;
    if (CurrentDialog >= 0 && TransformStack[CurrentDialog]->Transformation.TryMergeWith(transformation))
        return TransformStack[CurrentDialog];

    if (CurrentDialog >= 0)
    {
        if (CurrentDialog < TransformStack.Count - 1)
        {
            // pokud nejsme na konci Undo stacku, zahodime vsechny polozky za aktualnim indexem
            for (int i = TransformStack.Count - 1; i > CurrentDialog; i--)
                TransformStack.Delete(i);
        }

        CDialogData* dlgData = new CDialogData;
        dlgData->LoadFrom(TransformStack[CurrentDialog]);
        dlgData->Transformation.CopyFrom(transformation);

        TransformStack.Add(dlgData);
        CurrentDialog = TransformStack.Count - 1;
        ret = TransformStack[CurrentDialog];
    }
    else
        TRACE_E("Wrong CurrentDialog");
    return ret;
}

void CLayoutEditor::NewTransformationExecuteRebuildDlg(CTransformation* transformation, HWND hDlg, CStringList* errors)
{
    CDialogData* dialogData = NewTransformation(transformation);
    dialogData->ExecuteTransformation(hDlg, errors);
    RebuildDialog();
}

DWORD
CLayoutEditor::GetTransformStackStreamSize()
{
    DWORD ret = 4; // DWORD s poctem polozkek v nasledujicim poli
    for (int i = 0; i < TransformStack.Count; i++)
    {
        CDialogData* dialogData = TransformStack[i];
        ret += dialogData->Transformation.GetStreamSize();
    }
    return ret;
}

void CLayoutEditor::WriteTransformStackStream(BYTE* stream)
{
    *((DWORD*)stream) = TransformStack.Count;
    stream += 4;
    for (int i = 0; i < TransformStack.Count; i++)
    {
        CDialogData* dialogData = TransformStack[i];
        stream += dialogData->Transformation.WriteStream(stream);
    }
}

void CLayoutEditor::LoadTransformStackStream(const BYTE* stream)
{
    DWORD count = *((DWORD*)stream);
    stream += 4;
    for (DWORD i = 0; i < count; i++)
    {
        CDialogData* dlgData = new CDialogData;
        dlgData->LoadFrom(TransformStack[CurrentDialog]);
        stream += dlgData->Transformation.ReadStream(stream);

        CStringList errors;
        dlgData->ExecuteTransformation(HDialog, &errors);
        if (errors.GetCount() > 0)
        {
            char buff[10000];
            errors.GetStrings(buff, sizeof(buff));
            MessageBox(HWindow, buff, "Error", MB_OK | MB_ICONASTERISK);
        }

        TransformStack.Add(dlgData);
        CurrentDialog = TransformStack.Count - 1;
    }
}

void DescribeEmptyControls(HWND hDialog)
{
    HWND hChild = GetWindow(hDialog, GW_CHILD);
    do
    {
        if (hChild != NULL)
        {
            wchar_t text[2000];
            wchar_t className[2000];
            GetWindowTextW(hChild, text, 2000);
            GetClassNameW(hChild, className, 2000);
            if (text[0] == 0 && _wcsicmp(className, L"static") == 0)
                SetWindowTextW(hChild, L"[Static control]");
        }
    } while (hChild = GetWindow(hChild, GW_HWNDNEXT));
}

void RemoveDialogAmpersands(HWND hDialog)
{
    wchar_t buff[10000];
    HWND hChild = GetWindow(hDialog, GW_CHILD);
    do
    {
        if (hChild != NULL)
        {
            buff[0] = 0;
            if (GetWindowTextW(hChild, buff, 10000) != 0 && buff[0] != 0)
            {
                char className[300];
                if (GetClassName(hChild, className, _countof(className)) && _stricmp(className, "Static") == 0)
                {
                    LONG style = GetWindowLong(hChild, GWL_STYLE);
                    if (style & SS_NOPREFIX)
                        continue; // statiky s SS_NOPREFIX nebudeme zbavovat ampersandu, to by menilo jejich text
                }
                RemoveAmpersands(buff);
                SetWindowTextW(hChild, buff);
            }
        }
    } while (hChild = GetWindow(hChild, GW_HWNDNEXT));
}

BOOL CALLBACK
CLayoutEditor::EditDialogProcW(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
        {
            DestroyWindow(hwndDlg);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE;
}

BOOL CLayoutEditor::RebuildDialog()
{
    CDialogData* dialogData = TransformStack[CurrentDialog];

    WORD buff[200000];
    dialogData->PrepareTemplate(buff, FALSE, TRUE, FALSE);

    // dialog umistime do pocatku okna a shodime mu napriklad DS_CENTER, aby se nepokousel centrovat
    // styly DS_CONTROL | WS_CHILD zakazujeme, protoze jinak dochazelo k orezavani presahujicich prvku v layout editoru
    // styly WS_BORDER | WS_DLGFRAME pridavame kvuli dialogu v PictView (2300), ktery jinak clipoval controly
    DWORD addStyles = WS_CHILDWINDOW | WS_DISABLED | WS_OVERLAPPED | DS_MODALFRAME | WS_BORDER | WS_DLGFRAME;
    DWORD removeStyles = WS_VISIBLE | WS_POPUP | DS_CENTER | DS_SETFOREGROUND | DS_CONTROL;
    dialogData->TemplateAddRemoveStyles(buff, addStyles, removeStyles);
    dialogData->TemplateSetPos(buff, 10, 10);

    HWND hDlg = CreateDialogIndirectW(HInstance, (LPDLGTEMPLATE)buff, HDialogHolder, EditDialogProcW);
    if (hDlg == NULL)
    {
        DWORD err = GetLastError();
        TRACE_E("CreateDialogIndirect() failed, error=" << err);
        return FALSE;
    }

    GetWindowRect(hDlg, &DialogRect);
    int dlgW = DialogRect.right - DialogRect.left;
    int dlgH = DialogRect.bottom - DialogRect.top;
    ScreenToClient(HWindow, (LPPOINT)&DialogRect);
    DialogRect.right = DialogRect.left + dlgW;
    DialogRect.bottom = DialogRect.top + dlgH;

    // urcime o kolik je v bodech dialog posunut
    POINT ptOur;
    ptOur.x = 0;
    ptOur.y = 0;
    ClientToScreen(HWindow, &ptOur);
    POINT ptDlg;
    ptDlg.x = 0;
    ptDlg.y = 0;
    ClientToScreen(hDlg, &ptDlg);
    DialogOffsetX = ptDlg.x - ptOur.x;
    DialogOffsetY = ptDlg.y - ptOur.y;

    // nedokazal jsem nas zbavit problikavani horkych klaves, takze je proste vyhodime
    RemoveDialogAmpersands(hDlg);

    // static text staticum s prazdnym textem
    // DescribeEmptyControls(hDlg);

    if (HDialog != NULL)
        DestroyWindow(HDialog);

    HDialog = hDlg;
    SetProp(HDialog, "ATRLLayoutEditor", (HANDLE)this);
    LayoutWindow->LayoutInfo.SetDialogInfo(dialogData);

    // get base dlg units
    HDC hDC = HANDLES(GetDC(HDialog));
    HFONT hFont = (HFONT)SendMessage(HDialog, WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
    TEXTMETRIC tm;
    GetTextMetrics(hDC, &tm);
    BaseDlgUnitY = tm.tmHeight;
    SIZE sz;
    GetTextExtentPoint32(hDC, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &sz);
    BaseDlgUnitX = (sz.cx / 26 + 1) / 2;
    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(HDialog, hDC));

    ShowWindow(hDlg, SW_SHOWNA);
    UpdateBuffer();

    return TRUE;
}

void NormalizeRect(RECT* r)
{
    if (r->left > r->right)
    {
        int swap = r->left;
        r->left = r->right;
        r->right = swap;
    }
    if (r->top > r->bottom)
    {
        int swap = r->top;
        r->top = r->bottom;
        r->bottom = swap;
    }

    if (r->left < 0)
        r->left = 0;
    if (r->top < 0)
        r->top = 0;
}

void CLayoutEditor::DrawCage(POINT pt)
{
    RECT r;
    r.left = MouseDownPoint.x;
    r.top = MouseDownPoint.y;
    r.right = pt.x;
    r.bottom = pt.y;
    NormalizeRect(&r);
    HDC hDC = HANDLES(GetDC(HWindow));

    RECT cageRect;
    cageRect.left = 0;
    cageRect.top = 0;
    cageRect.right = max(1, r.right - r.left);
    ;
    cageRect.bottom = max(1, r.bottom - r.top);
    if (CageBitmap == NULL)
    {
        CageBitmap = new CBitmap();
        CageBitmap->CreateBmp(hDC, cageRect.right, cageRect.bottom);
    }
    else
    {
        CageBitmap->Enlarge(cageRect.right, cageRect.bottom);
    }

    int cageColor = COLOR_HIGHLIGHT;
    FillRect(CageBitmap->HMemDC, &cageRect, (HBRUSH)(cageColor + 1));

    BLENDFUNCTION bf;
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 50;
    bf.AlphaFormat = 0; // AC_SRC_ALPHA nefunguje

    BitBlt(TempBitmap->HMemDC, 0, 0, TempBitmap->GetWidth(), TempBitmap->GetHeight(), CacheBitmap->HMemDC, 0, 0, SRCCOPY);
    AlphaBlend(TempBitmap->HMemDC, r.left, r.top, r.right - r.left, r.bottom - r.top, CageBitmap->HMemDC, 0, 0, r.right - r.left, r.bottom - r.top, bf);

    HBRUSH hOldBrush = (HBRUSH)SelectObject(TempBitmap->HMemDC, GetStockObject(NULL_BRUSH));
    HPEN hFramePen = HANDLES(CreatePen(PS_SOLID, 0, GetSysColor(cageColor)));
    HPEN hOldPen = (HPEN)SelectObject(TempBitmap->HMemDC, hFramePen);
    Rectangle(TempBitmap->HMemDC, r.left, r.top, r.right, r.bottom);
    SelectObject(TempBitmap->HMemDC, hOldBrush);
    SelectObject(TempBitmap->HMemDC, hOldPen);
    HANDLES(DeleteObject(hFramePen));

    BitBlt(hDC, 0, 0, TempBitmap->GetWidth(), TempBitmap->GetHeight(), TempBitmap->HMemDC, 0, 0, SRCCOPY);

    HANDLES(ReleaseDC(HWindow, hDC));
}

void CLayoutEditor::PaintCachedBitmap()
{
    HDC hDC = HANDLES(GetDC(HWindow));
    BitBlt(hDC, 0, 0, TempBitmap->GetWidth(), TempBitmap->GetHeight(), CacheBitmap->HMemDC, 0, 0, SRCCOPY);
    HANDLES(ReleaseDC(HWindow, hDC));
}

void CLayoutEditor::ResetTemplateToOriginal()
{
    CTransformation transformation;
    transformation.SetTransformationWithNoParam(eloTemplateReset);
    NewTransformationExecuteRebuildDlg(&transformation);
}

void CLayoutEditor::NormalizeLayout(BOOL wide)
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (!dialogData->CanNormalizeLayout())
    {
        MessageBox(HWindow, "Layout normalization is supported only for dialog boxes with outer controls on the right side.", "Error", MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    CTransformation transformation;
    transformation.SetTransformationWithIntParam(eloNormalizeLayout, wide);
    CStringList errors;
    NewTransformationExecuteRebuildDlg(&transformation, HDialog, &errors);
    if (errors.GetCount() > 0)
    {
        char buff[10000];
        errors.GetStrings(buff, sizeof(buff));
        MessageBox(HWindow, buff, "Error", MB_OK | MB_ICONASTERISK);
    }
}

void CLayoutEditor::BeginPreviewLayout(BOOL original, BOOL showExitHint)
{
    if (PreviewMode)
    {
        TRACE_E("CLayoutEditor::BeginPreviewLayout() Already in PreviewMode!");
        return;
    }
    if (!original && TransformStack.Count <= 1)
        return;

    CTransformation transformation;
    transformation.SetTransformationWithNoParam(eloPreviewLayout);
    CDialogData* dialogData = NewTransformation(&transformation);
    PreviewMode = TRUE;
    PreviewModeShowExitHint = showExitHint;
    SetCapture(HWindow);
    // behem preview chceme zachovat aktualni selection, aby se clovek mohl soustredit na zmeny layoutu
    if (original)
    {
        dialogData->LoadOriginalLayout();
    }
    else
    {
        dialogData->LoadFrom(TransformStack[0], TRUE);
        dialogData->LoadSelectionFrom(TransformStack[CurrentDialog - 1]);
    }
    RebuildDialog();
}

void CLayoutEditor::EndPreviewLayout()
{
    TransformStack.Delete(CurrentDialog);
    CurrentDialog--;
    PreviewMode = FALSE;
    RebuildDialog();
}

void CLayoutEditor::AlignControls(CEdgeEnum edge)
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->GetSelectedControlsCount() > 1)
    {
        CTransformation transformation;
        transformation.SetTransformationWithIntParam(eloAlignControls, edge);
        NewTransformationExecuteRebuildDlg(&transformation);
    }
}

void CLayoutEditor::CenterControls(BOOL horizontal)
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->GetSelectedControlsCount() > 1)
    {
        CTransformation transformation;
        transformation.SetTransformationWithIntParam(eloCenterControls, horizontal);
        NewTransformationExecuteRebuildDlg(&transformation);
    }
}

void CLayoutEditor::CenterControlsToDialog(BOOL horizontal)
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->GetSelectedControlsCount() > 0)
    {
        CTransformation transformation;
        transformation.SetTransformationWithIntParam(eloCenterControlsToDlg, horizontal);
        NewTransformationExecuteRebuildDlg(&transformation);
    }
}

void CLayoutEditor::EqualSpacing(BOOL horizontal, int delta)
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->GetSelectedControlsCount() > 1)
    {
        CTransformation transformation;
        transformation.SetTransformationWith2IntParams(eloEqualSpacing, horizontal, delta);
        NewTransformationExecuteRebuildDlg(&transformation);
    }
}

void CLayoutEditor::ResizeControls(CResizeControlsEnum resize)
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->GetSelectedControlsCount() > 1)
    {
        CTransformation transformation;
        transformation.SetTransformationWithIntParam(eloResizeControls, resize);
        NewTransformationExecuteRebuildDlg(&transformation);
    }
}

void CLayoutEditor::SetSize()
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->GetSelectedControlsCount() > 0)
    {
        int width = -1;
        int height = -1;
        CSetSizeDialog dlg(HDialogHolder, HDialog, this, &width, &height);
        if (dlg.Execute() == IDOK)
        {
            CTransformation transformation;
            transformation.SetTransformationWith2IntParams(eloSetSize, width, height);
            NewTransformationExecuteRebuildDlg(&transformation);
        }
    }
}

void CLayoutEditor::ControlsSizeToContent()
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->GetSelectedControlsCount() > 0)
    {
        CTransformation transformation;
        transformation.SetTransformationWithNoParam(eloSizeToContent);
        NewTransformationExecuteRebuildDlg(&transformation, HDialog);
    }
}

void CLayoutEditor::SelectAllControls(BOOL select)
{
    CTransformation transformation;
    transformation.SetTransformationWithNoParam(eloSelect);
    CDialogData* dialogData = NewTransformation(&transformation);
    dialogData->ModifySelection(HDialog, select ? emsmSelectAllControls : emsmSelectControl, NULL);
    dialogData->StoreSelectionToTransformation();
    RebuildDialog();
}

void CLayoutEditor::NextPrevControl(BOOL next)
{
    CTransformation transformation;
    transformation.SetTransformationWithNoParam(eloSelect);
    CDialogData* dialogData = NewTransformation(&transformation);
    dialogData->SelectNextPrevControl(next);
    dialogData->StoreSelectionToTransformation();
    RebuildDialog();
}

void CLayoutEditor::PaintSelectionGuidelines(HDC hDC)
{
    if (CurrentDialog == -1)
        return;
    RECT r;
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->SelCtrlsGetOuterRectPx(HDialog, &r))
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, GetSelectionColor(FALSE) ^ RGB(255, 255, 255));
        HPEN hOldPen = (HPEN)SelectObject(hDC, hPen);

        RECT wr;
        GetClientRect(HDialogHolder, &wr);

        r.left += DialogOffsetX;
        r.top += DialogOffsetY;
        r.right += DialogOffsetX;
        r.bottom += DialogOffsetY;
        int oldROP = SetROP2(hDC, R2_XORPEN);

        MoveToEx(hDC, r.left, 0, NULL);
        LineTo(hDC, r.left, wr.bottom);

        MoveToEx(hDC, r.right - 1, 0, NULL);
        LineTo(hDC, r.right - 1, wr.bottom);

        MoveToEx(hDC, 0, r.top, NULL);
        LineTo(hDC, wr.right, r.top);

        MoveToEx(hDC, 0, r.bottom - 1, NULL);
        LineTo(hDC, wr.right, r.bottom - 1);

        SetROP2(hDC, oldROP);
        SelectObject(hDC, hOldPen);
        DeleteObject(hPen);
    }
}

int CLayoutEditor::GetOurChildWindowIndex(POINT pt)
{
    ScreenToClient(HDialog, &pt);
    int x = ((pt.x /*- DialogOffsetX*/) * 4) / BaseDlgUnitX;
    int y = ((pt.y /*- DialogOffsetY*/) * 8) / BaseDlgUnitY;

    CDialogData* dialogData = TransformStack[CurrentDialog];
    for (int i = 1; i < dialogData->Controls.Count; i++) // 0 - title dialogu
    {
        const CControl* control = dialogData->Controls[i];
        if (control->TCX <= 3 || control->TCY <= 3)
        {
            int ctrlX = control->TX - 2;
            int ctrlW = control->TCX + 4;
            int ctrlY = control->TY - 2;
            int ctrlH = control->TCY + 4;
            if (control->IsComboBox()) // u comboboxu chceme jejich "zabaleny" rozmer
                ctrlH = COMBOBOX_BASE_HEIGHT;

            if (x >= ctrlX && x < ctrlX + ctrlW && y >= ctrlY && y < ctrlY + ctrlH)
                return i;
        }
    }
    return -1;
}

HWND CLayoutEditor::GetOurChildWindow(POINT pt)
{
    int index = GetOurChildWindowIndex(pt);
    if (index != -1)
    {
        CDialogData* dialogData = TransformStack[CurrentDialog];
        return dialogData->GetControlHandle(HDialog, index);
    }
    return NULL;
}

BOOL CLayoutEditor::HitTest(const POINT* p, CEdgeEnum* edge)
{
#define CAGE_WIDTH 4
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->GetSelectedControlsCount() != 1 && !dialogData->IsDialogSelected())
        return FALSE;

    HWND hControl;
    if (dialogData->IsDialogSelected())
    {
        hControl = HDialog;
    }
    else
    {
        hControl = dialogData->SelCtrlsGetCurrentControlHandle(HDialog);
        if (hControl == NULL)
            return FALSE;
    }

    RECT r;
    GetWindowRect(hControl, &r);
    if (r.left == r.right)
        r.right = r.left + 1;
    if (r.top == r.bottom)
        r.bottom = r.top + 1;

    int controlW = r.right - r.left;
    int controlH = r.bottom - r.top;

    // usnadnime u uzkych prvku jejich uchyt v rohu
    int cageVertical = 0;
    int cageHorizontal = 0;
    if (controlW <= CAGE_WIDTH)
        cageHorizontal = CAGE_WIDTH;
    if (controlH <= CAGE_WIDTH)
        cageVertical = CAGE_WIDTH;

    if (p->y > (r.top - cageVertical) && p->y < (r.bottom + cageVertical))
    {
        if (!dialogData->IsDialogSelected() && (p->x - r.left < CAGE_WIDTH / 2) && (p->x - r.left >= -CAGE_WIDTH / 2))
        {
            *edge = edgeLeft;
            return TRUE;
        }
        if ((p->x - r.left >= controlW - CAGE_WIDTH / 2) && (p->x - r.left < controlW + CAGE_WIDTH / 2))
        {
            *edge = edgeRight;
            return TRUE;
        }
    }

    // pro horizontalni oddelovac zakazeme up/down resize, uzivatel chce spis posouvat
    int itemIndex = dialogData->SelCtrlsGetFirstControlIndex();
    if (!dialogData->IsDialogSelected() && (itemIndex == -1 || dialogData->Controls[itemIndex]->IsHorizLine()))
        return FALSE;

    if (p->x > (r.left - cageHorizontal) && p->x < (r.right + cageHorizontal))
    {
        if (!dialogData->IsDialogSelected() && (p->y - r.top < CAGE_WIDTH / 2) && (p->y - r.top >= -CAGE_WIDTH / 2))
        {
            *edge = edgeTop;
            return TRUE;
        }
        if ((p->y - r.top >= controlH - CAGE_WIDTH / 2) && (p->y - r.top < controlH + CAGE_WIDTH / 2))
        {
            *edge = edgeBottom;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CALLBACK
CLayoutEditor::TestDialogProcW(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CenterWindowToWindow(hwndDlg, GetParent(hwndDlg));
        FillControlsWithDummyValues(hwndDlg);
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
        {
            EndDialog(hwndDlg, wParam);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE;
}

void CLayoutEditor::PostMouseMove()
{
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(HWindow, &p);
    SendMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
}

void CLayoutEditor::SetAlignToMode(BOOL active)
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->GetSelectedControlsCount() < 1)
        return;

    AlignToMode = active;
    if (AlignToMode)
    {
        SetCapture(HWindow);
        SetCursor(LoadCursor(NULL, IDC_HAND));
        PostMouseMove();
    }
    else
    {
        ReleaseCapture();
        PostMouseMove();
    }
}

void CLayoutEditor::TestDialogBox()
{
    CDialogData* dialogData = TransformStack[CurrentDialog];
    WORD buff[200000];
    dialogData->PrepareTemplate(buff, FALSE, FALSE, FALSE);

    // pridame flagy, diky kterym budou videt i property pages
    DWORD addStyles = WS_CAPTION | WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS | DS_MODALFRAME | DS_SETFOREGROUND | DS_3DLOOK;
    DWORD removeStyles = 0;
    dialogData->TemplateAddRemoveStyles(buff, addStyles, removeStyles);
    DialogBoxIndirectParamW(HInstance, (LPCDLGTEMPLATEW)buff, HWindow, TestDialogProcW, 0);
}

void CLayoutEditor::ApplyAlignTo(const CAlignToParams* params)
{
    if (!KeepAlignToMode)
    {
        TRACE_E("Align To dialog box is not opened!");
        return;
    }
    CDialogData* dialogData = TransformStack[CurrentDialog];
    if (dialogData->HighlightIndex == -1)
    {
        TRACE_E("Reference item is not selected!");
        return;
    }

    TransformStack.Delete(CurrentDialog);
    CurrentDialog--;
    CTransformation transformation;
    transformation.SetTransformationWithNoParam(eloAlignTo);
    dialogData = NewTransformation(&transformation);
    dialogData->SelCtrlsAlignTo(params);
    RebuildDialog();
}

LRESULT
CLayoutEditor::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (PreviewMode && uMsg == WM_KEYUP && (wParam == 'U' || wParam == 'P'))
    {
        ReleaseCapture();
        return 0;
    }
    if ((uMsg == WM_KEYDOWN || uMsg == WM_KEYUP) && wParam == VK_SHIFT)
    {
        BOOL firstPress;
        if (uMsg == WM_KEYDOWN)
            firstPress = (lParam & 0x40000000) == 0;
        else
            firstPress = (lParam & 0x40000000) != 0;
        // VK_SHIFT zamkne tazeni v jedne ose; vnutime mousemove, aby se hned zmena vizualiozvala
        if (firstPress && DraggingMode == edmMove)
            PostMouseMove();
    }

    switch (uMsg)
    {
    case WM_SIZE:
    {
        UpdateBuffer();
        break;
    }

    case WM_ERASEBKGND:
    {
        return TRUE; // pozadi smazano
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
        RECT r;
        GetClientRect(HWindow, &r);
        BitBlt(hDC, 0, 0, r.right, r.bottom, CacheBitmap->HMemDC, 0, 0, SRCCOPY);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_CAPTURECHANGED:
    {
        if (PreviewMode)
            EndPreviewLayout();
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    {
        if (PreviewMode)
        {
            ReleaseCapture();
            return 0;
        }
        if (AlignToMode)
        {
            CTransformation transformation;
            transformation.SetTransformationWithNoParam(eloAlignTo);
            CDialogData* dialogData = NewTransformation(&transformation);

            KeepAlignToMode = TRUE;
            CAlignToParams params;
            params.Operation = atoeMove;
            params.MoveAsGroup = TRUE;
            params.SelPart = (CAlignToPartEnum)-1;
            params.RefPart = (CAlignToPartEnum)-1;
            params.HighlightIndex = dialogData->HighlightIndex;
            CAlignToDialog dlg(HDialogHolder, HDialog, &params, this);
            if (dlg.Execute() == IDOK)
            {
                dialogData = TransformStack[CurrentDialog];
                dialogData->Transformation.SetStructParam(&params, sizeof(params));
            }
            else
            {
                TransformStack.Delete(CurrentDialog);
                CurrentDialog--;
            }
            KeepAlignToMode = FALSE;
            SetAlignToMode(FALSE);

            RebuildDialog();
            return 0;
        }

        DWORD pos = GetMessagePos();
        POINT pt;
        pt.x = GET_X_LPARAM(pos);
        pt.y = GET_Y_LPARAM(pos);

        POINT ptClient;
        ptClient.x = GET_X_LPARAM(lParam);
        ptClient.y = GET_Y_LPARAM(lParam);

        //RECT changedRect; // optimalizace paintu, budeme kreslit jen zmenenou oblast
        //ZeroMemory(&changedRect, sizeof(changedRect));
        //SelectedControls.EnlargeSelectionBox(&changedRect);

        PostponedControlSelect = -1;
        DraggingMode = edmNone;

        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        CDialogData* dialogData = TransformStack[CurrentDialog];
        POINT p;
        GetCursorPos(&p);
        CEdgeEnum hitEdge;

        // TRACE_I("WM_LBUTTONDOWN x="<<pt.x<<" y="<<pt.y<<" hitTest="<<hitTest);
        if (uMsg == WM_LBUTTONDOWN && HitTest(&p, &hitEdge))
        {
            // double click nechceme, mohlo by jit o oznaceni groupy
            if (dialogData->SelCtrlsGetCurrentControlRect(&OriginalResizeRect))
            {
                DraggingMode = edmResize;
                ResizeEdge = hitEdge;
                OldResizeDelta = 0;
            }
        }
        else
        {
            HWND hChild = GetChildWindow(pt, HDialog);
            if (hChild == NULL)
                hChild = GetOurChildWindow(pt); // dame sanci uzkym controlum
            if (hChild == NULL || altPressed)
            {
                // Alt umoznuje nastartovat klec nad prvkem (misto jeho tazeni)
                if (!shiftPressed)
                {
                    CTransformation transformation;
                    transformation.SetTransformationWithNoParam(eloSelect);
                    dialogData = NewTransformation(&transformation);
                    dialogData->ModifySelection(HDialog, PtInRect(&DialogRect, ptClient) ? emsmSelectDialog : emsmSelectControl, NULL);
                    dialogData->StoreSelectionToTransformation();
                }
                DraggingMode = edmCage;
            }
            else
            {
                if (uMsg == WM_LBUTTONDBLCLK)
                {
                    CTransformation transformation;
                    transformation.SetTransformationWithNoParam(eloSelect);
                    dialogData = NewTransformation(&transformation);
                    dialogData->SelectControlsGroup(HDialog, hChild, shiftPressed, controlPressed);
                    dialogData->StoreSelectionToTransformation();
                }
                else
                {
                    if (dialogData->SelCtrlsContains(HDialog, hChild))
                    {
                        // je vybrano vice prvku, jeden z nich je ten, na ktery uzivatel kliknul bez SHIFT
                        // nemuzeme v tuto chvili ostatni odvybrat, protoze muze jit o zacatek tazeni -- proto pripadne odvybrani
                        // musime odlozit na LBUTTONUP
                        PostponedControlSelect = dialogData->GetControlIndex(HDialog, hChild);
                        DraggingMode = edmPreMove;
                    }
                    else
                    {
                        CModifySelectionMode modifyMode = emsmSelectControl;
                        if (shiftPressed)
                            modifyMode = emsmToggleControl;
                        CTransformation transformation;
                        transformation.SetTransformationWithNoParam(eloSelect);
                        dialogData = NewTransformation(&transformation);
                        dialogData->ModifySelection(HDialog, modifyMode, hChild);
                        dialogData->StoreSelectionToTransformation();
                        DraggingMode = edmPreMove;
                    }
                }
            }
        }

        POINT mouseDownPoint;
        mouseDownPoint.x = GET_X_LPARAM(lParam);
        mouseDownPoint.y = GET_Y_LPARAM(lParam);
        MouseDownPoint = mouseDownPoint;
        MouseMovedDeltaX = 0;
        MouseMovedDeltaY = 0;

        ShiftPressedOnMouseDown = shiftPressed;

        //LastOperation = eloNone; FIXME - smazat?
        RebuildDialog();
        SetFocus(HWindow);
        SetCapture(HWindow);

        /*
      HDC hDC = HANDLES(GetDC(HWindow));
      FillRect(hDC, &changedRect, (HBRUSH)(COLOR_WINDOW + 1));
      HANDLES(ReleaseDC(HWindow, hDC));
*/
        break;
    }

    case WM_LBUTTONUP:
    {
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (GetCapture() == HWindow)
        {
            if (PostponedControlSelect != -1)
            {
                CTransformation transformation;
                transformation.SetTransformationWithNoParam(eloSelect);
                CDialogData* dialogData = NewTransformation(&transformation);
                dialogData->ModifySelection(HDialog, shiftPressed ? emsmToggleControl : emsmSelectControl,
                                            dialogData->GetControlHandle(HDialog, PostponedControlSelect));
                dialogData->StoreSelectionToTransformation();
                RebuildDialog();
            }
            if (DraggingMode == edmCage)
            {
                POINT pt;
                pt.x = GET_X_LPARAM(lParam);
                pt.y = GET_Y_LPARAM(lParam);
                RECT r;
                r.left = MouseDownPoint.x;
                r.top = MouseDownPoint.y;
                r.right = pt.x;
                r.bottom = pt.y;
                NormalizeRect(&r);
                r.left = ((r.left - DialogOffsetX) * 4) / BaseDlgUnitX;
                r.right = ((r.right - DialogOffsetX) * 4) / BaseDlgUnitX;
                r.top = ((r.top - DialogOffsetY) * 8) / BaseDlgUnitY;
                r.bottom = ((r.bottom - DialogOffsetY) * 8) / BaseDlgUnitY;

                CTransformation transformation;
                transformation.SetTransformationWithNoParam(eloSelect);
                CDialogData* dialogData = NewTransformation(&transformation);
                dialogData->SelectControlsByCage(ShiftPressedOnMouseDown, &r);
                dialogData->StoreSelectionToTransformation();
                RebuildDialog();
            }
        }
        // propadneme do WM_CANCELMODE
    }
    case WM_CANCELMODE:
    {
        BOOL rebuildDlg = FALSE;
        if (AlignToMode && !KeepAlignToMode)
        {
            SetAlignToMode(FALSE);
            rebuildDlg = TRUE;
        }
        if (DraggingMode != edmNone)
        {
            CDraggingModeEnum oldDraggingMode = DraggingMode;
            DraggingMode = edmNone;
            LayoutWindow->LayoutInfo.SetMouseDelta(FALSE);

            if (uMsg == WM_CANCELMODE && (oldDraggingMode == edmMove || oldDraggingMode == edmResize))
                OnUndo();
        }
        ReleaseCapture();
        if (rebuildDlg)
            RebuildDialog();
        else
            PaintCachedBitmap();
    };

    case WM_MOUSEMOVE:
    {
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);

        if (shiftPressed && DraggingMode == edmMove)
        {
            if (abs(MouseDownPoint.x - pt.x) < abs(MouseDownPoint.y - pt.y))
                pt.x = MouseDownPoint.x;
            else
                pt.y = MouseDownPoint.y;
        }

        // info o mysi
        POINT infoCurrent;
        infoCurrent.x = ((pt.x - DialogOffsetX) * 4) / BaseDlgUnitX; // pixelX = (dialogunitX * baseunitX) / 4
        infoCurrent.y = ((pt.y - DialogOffsetY) * 8) / BaseDlgUnitY; // pixelY = (dialogunitY * baseunitY) / 8
        POINT infoDown;
        infoDown.x = ((MouseDownPoint.x - DialogOffsetX) * 4) / BaseDlgUnitX; // pixelX = (dialogunitX * baseunitX) / 4
        infoDown.y = ((MouseDownPoint.y - DialogOffsetY) * 8) / BaseDlgUnitY; // pixelY = (dialogunitY * baseunitY) / 8
        LayoutWindow->LayoutInfo.SetMouseCurrent(&infoCurrent);
        BOOL showDelta = (GetCapture() == HWindow) && (DraggingMode == edmMove || DraggingMode == edmResize);
        int deltaX = infoCurrent.x - infoDown.x;
        int deltaY = infoCurrent.y - infoDown.y;
        if (DraggingMode == edmResize)
        {
            CDialogData* dialogData = TransformStack[CurrentDialog];
            if (dialogData->IsDialogSelected())
                dialogData->GetDialogResizeDelta(&OriginalResizeRect, ResizeEdge, &deltaX, &deltaY);
            else
                dialogData->GetCurrentControlResizeDelta(&OriginalResizeRect, ResizeEdge, &deltaX, &deltaY);
        }
        LayoutWindow->LayoutInfo.SetMouseDelta(showDelta, deltaX, deltaY);

        if (GetCapture() == HWindow)
        {
            if (AlignToMode)
            {
                DWORD pos = GetMessagePos();
                POINT pt;
                pt.x = GET_X_LPARAM(pos);
                pt.y = GET_Y_LPARAM(pos);

                int index = 0;
                HWND hChild = GetChildWindow(pt, HDialog, &index);
                if (hChild == NULL)
                {
                    index = GetOurChildWindowIndex(pt); // dame sanci uzkym controlum
                    if (index == -1)
                        index = 0;
                }
                CDialogData* dialogData = TransformStack[CurrentDialog];
                dialogData->SetHighlightControl(index);
                RebuildDialog();
            }

            if (DraggingMode == edmPreMove)
            {
                if (abs(MouseDownPoint.x - GET_X_LPARAM(lParam)) > GetSystemMetrics(SM_CXDRAG) ||
                    abs(MouseDownPoint.y - GET_Y_LPARAM(lParam)) > GetSystemMetrics(SM_CYDRAG) ||
                    controlPressed)
                {
                    DraggingMode = edmMove;
                }
            }

            if (DraggingMode == edmMove)
            {
                deltaX = ((pt.x - MouseDownPoint.x) * 4) / BaseDlgUnitX - MouseMovedDeltaX; // pixelX = (dialogunitX * baseunitX) / 4
                deltaY = ((pt.y - MouseDownPoint.y) * 8) / BaseDlgUnitY - MouseMovedDeltaY; // pixelY = (dialogunitY * baseunitY) / 8
                if (deltaX != 0 || deltaY != 0)
                {
                    PostponedControlSelect = -1;

                    CTransformation transformation;
                    transformation.SetTransformationWith2IntParams(eloMove, deltaX, deltaY);
                    CDialogData* dialogData = NewTransformation(&transformation);
                    dialogData->CtrlsMove(TRUE, deltaX, deltaY);
                    RebuildDialog();
                    MouseMovedDeltaX += deltaX;
                    MouseMovedDeltaY += deltaY;
                }
            }

            if (DraggingMode == edmCage)
            {
                DrawCage(pt);
            }

            if (DraggingMode == edmResize)
            {
                deltaX = ((pt.x - MouseDownPoint.x) * 4) / BaseDlgUnitX - MouseMovedDeltaX; // pixelX = (dialogunitX * baseunitX) / 4
                deltaY = ((pt.y - MouseDownPoint.y) * 8) / BaseDlgUnitY - MouseMovedDeltaY; // pixelY = (dialogunitY * baseunitY) / 8
                if (deltaX != 0 || deltaY != 0)
                {
                    int delta;
                    switch (ResizeEdge)
                    {
                    case edgeLeft:
                        delta = deltaX;
                        break;
                    case edgeTop:
                        delta = deltaY;
                        break;
                    case edgeRight:
                        delta = deltaX;
                        break;
                    case edgeBottom:
                        delta = deltaY;
                        break;
                    default:
                        TRACE_E("Unknown resize mode!");
                    }
                    CTransformation transformation;
                    CDialogData* dialogData = TransformStack[CurrentDialog];
                    if (dialogData->IsDialogSelected())
                    {
                        BOOL horizontal = ResizeEdge == edgeRight;
                        transformation.SetTransformationWith2IntParams(eloResizeDlg, horizontal, delta - OldResizeDelta);
                        dialogData = NewTransformation(&transformation);
                        dialogData->DialogResize(horizontal, delta, &OriginalResizeRect);
                    }
                    else
                    {
                        transformation.SetTransformationWith2IntParams(eloResize, ResizeEdge, delta - OldResizeDelta);
                        dialogData = NewTransformation(&transformation);
                        dialogData->SelCtrlsResize(ResizeEdge, delta, &OriginalResizeRect);
                    }
                    OldResizeDelta = delta;
                    RebuildDialog();
                }
            }
        }
        break;
    }

    case WM_SETCURSOR:
    {
        POINT p;
        GetCursorPos(&p);
        CEdgeEnum hitEdge;
        //      TRACE_I("WM_SETCURSOR x="<<p.x<<" y="<<p.y<<" hitTest="<<hitTest);
        if (HitTest(&p, &hitEdge))
        {
            LPSTR cursorID = 0;
            switch (hitEdge)
            {
            case edgeLeft:
                cursorID = IDC_SIZEWE;
                break;
            case edgeTop:
                cursorID = IDC_SIZENS;
                break;
            case edgeRight:
                cursorID = IDC_SIZEWE;
                break;
            case edgeBottom:
                cursorID = IDC_SIZENS;
                break;
            }
            SetCursor(LoadCursor(NULL, cursorID));
            return TRUE;
        }
        break;
    }

    case WM_KEYDOWN:
    {
        // TRACE_I("WM_KEYDOWN");
        int repeatArrow = RepeatArrow;
        if (wParam == 'A')
            repeatArrow = RepeatArrow = LastRepeatArrow;
        else
        {
            if (wParam >= '0' && wParam <= '9')
            {
                if (RepeatArrow > 0)
                    repeatArrow = RepeatArrow = RepeatArrow * 10;
                else
                    repeatArrow = RepeatArrow = 0;
                RepeatArrow += wParam - '0';
            }
            else
            {
                if (wParam != VK_SHIFT && wParam != VK_CONTROL)
                    RepeatArrow = -1;
            }
        }
        switch (wParam)
        {
        case VK_ESCAPE:
        {
            if (GetCapture() == HWindow)
                PostMessage(HWindow, WM_CANCELMODE, 0, 0);
            else
                PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_EXIT, 0); // bezpecny close-window
            return 0;
        }

        case VK_RIGHT:
        case VK_LEFT:
        case VK_UP:
        case VK_DOWN:
        {
            CDialogData* dialogData = TransformStack[CurrentDialog];
            BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if ((!shiftPressed || shiftPressed && controlPressed) && dialogData->GetSelectedControlsCount() > 0 ||
                shiftPressed && !controlPressed && (dialogData->GetSelectedControlsCount() > 0 || dialogData->IsDialogSelected()))
            {
                if (repeatArrow < 1)
                    repeatArrow = 1;
                LastRepeatArrow = repeatArrow;
                int delta = 0;
                if (wParam == VK_LEFT)
                    delta = -repeatArrow;
                if (wParam == VK_RIGHT)
                    delta = repeatArrow;
                if (wParam == VK_UP)
                    delta = -repeatArrow;
                if (wParam == VK_DOWN)
                    delta = repeatArrow;
                BOOL horizontal = (wParam == VK_LEFT || wParam == VK_RIGHT);
                CTransformation transformation;
                if (shiftPressed && controlPressed)
                {
                    CEdgeEnum edge = horizontal ? edgeLeft : edgeTop;
                    transformation.SetTransformationWith2IntParams(eloResize, edge, delta);
                    CDialogData* dialogData = NewTransformation(&transformation);
                    dialogData->SelCtrlsResize(edge, delta);
                }
                else
                {
                    if (!shiftPressed)
                    {
                        int deltaX = horizontal ? delta : 0;
                        int deltaY = horizontal ? 0 : delta;
                        transformation.SetTransformationWith2IntParams(eloMove, deltaX, deltaY);
                        CDialogData* dialogData = NewTransformation(&transformation);
                        dialogData->CtrlsMove(TRUE, deltaX, deltaY);
                    }
                    else
                    {
                        if (dialogData->IsDialogSelected())
                        {
                            transformation.SetTransformationWith2IntParams(eloResizeDlg, horizontal, delta);
                            CDialogData* dialogData = NewTransformation(&transformation);
                            dialogData->DialogResize(horizontal, delta);
                        }
                        else
                        {
                            CEdgeEnum edge = horizontal ? edgeRight : edgeBottom;
                            transformation.SetTransformationWith2IntParams(eloResize, edge, delta);
                            CDialogData* dialogData = NewTransformation(&transformation);
                            dialogData->SelCtrlsResize(edge, delta);
                        }
                    }
                }
                RebuildDialog();
            }
            break;
        }
        }
        break;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
};

//*****************************************************************************
//
// CLayoutEditorHolder
//

CLayoutEditorHolder::CLayoutEditorHolder()
    : CWindow(ooStatic)
{
}

//*****************************************************************************
//
// CLayoutInfo
//

CLayoutInfo::CLayoutInfo()
    : CWindow(ooStatic)
{
    HFont = NULL;
    FontHeight = 0;
    DialogData = NULL;
    DataChanged = TRUE;
    InfoBitmap = NULL;
    ShowMouseDelta = FALSE;
}

CLayoutInfo::~CLayoutInfo()
{
    if (DialogData != NULL)
        delete DialogData;
    if (InfoBitmap != NULL)
        delete InfoBitmap;
}

void CLayoutInfo::SetDialogInfo(const CDialogData* dialogData)
{
    if (DialogData != NULL)
    {
        delete DialogData;
        DialogData = NULL;
    }
    if (dialogData != NULL)
    {
        DialogData = new CDialogData();
        DialogData->LoadFrom((CDialogData*)dialogData);
    }
    DataChanged = TRUE;
    InvalidateRect(HWindow, NULL, FALSE);
}

void CLayoutInfo::SetMouseCurrent(POINT* pt)
{
    MouseCurrent = *pt;
    DataChanged = TRUE;
    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
}

void CLayoutInfo::SetMouseDelta(BOOL showDelta, int deltaX, int deltaY)
{
    ShowMouseDelta = showDelta;
    MouseDeltaX = deltaX;
    MouseDeltaY = deltaY;
    DataChanged = TRUE;
    InvalidateRect(HWindow, NULL, FALSE);
}

int CLayoutInfo::GetNeededHeight()
{
    return 6 * FontHeight;
}

BOOL CLayoutInfo::GetSameProperties(BOOL* sameTX, BOOL* sameTY, BOOL* sameTCX, BOOL* sameTCY, BOOL* sameRight, BOOL* sameBottom)
{
    if (DialogData == NULL || DialogData->GetSelectedControlsCount() < 1)
        return FALSE;

    *sameTX = TRUE;
    *sameTY = TRUE;
    *sameTCX = TRUE;
    *sameTCY = TRUE;
    *sameRight = TRUE;  // stejna prava hrana
    *sameBottom = TRUE; // stejna spodni hrana
    const CControl* controlFirst = NULL;
    for (int i = 1; i < DialogData->Controls.Count; i++) // 0-ty prvek preskocime, obsahuje nazev dialogu
    {
        const CControl* control = DialogData->Controls[i];
        if (!control->Selected)
            continue;
        if (controlFirst == NULL)
        {
            controlFirst = control;
            continue;
        }
        if (control->TX != controlFirst->TX)
            *sameTX = FALSE;
        if (control->TY != controlFirst->TY)
            *sameTY = FALSE;
        if (control->TCX != controlFirst->TCX)
            *sameTCX = FALSE;
        if (control->TCY != controlFirst->TCY)
            *sameTCY = FALSE;
        if (control->TX + control->TCX != controlFirst->TX + controlFirst->TCX)
            *sameRight = FALSE;
        if (control->TY + control->TCY != controlFirst->TY + controlFirst->TCY)
            *sameBottom = FALSE;
    }
    return TRUE;
}

BOOL CLayoutInfo::GetOuterSpace(RECT* outerSpace)
{
    if (DialogData == NULL || DialogData->GetSelectedControlsCount() < 1)
        return FALSE;

    RECT outline = {0};
    DialogData->SelCtrlsGetOuterRect(&outline);

    // budeme hledat vzdalenost k prvkum (ve 4 smerech), ktere do vybranych controlu nepatri, pripadne k hranam dialogu

    // napred vzdalenost od hran dialogu
    outerSpace->left = outline.left;
    outerSpace->top = outline.top;
    outerSpace->right = DialogData->TCX - outline.right;
    outerSpace->bottom = DialogData->TCY - outline.bottom;

    // v druhe fazi od controlu v kazdem ze smeru
    for (int i = 1; i < DialogData->Controls.Count; i++) // 0 - title dialogu
    {
        const CControl* control = DialogData->Controls[i];
        if (!DialogData->SelCtrlsContainsControlWithIndex(i)) // -1 abychom preskocili title dialogu
        {
            int ctrlX = control->TX;
            int ctrlW = control->TCX;
            int ctrlY = control->TY;
            int ctrlH = control->TCY;
            if (control->IsComboBox()) // u comboboxu chceme jejich "zabaleny" rozmer
                ctrlH = COMBOBOX_BASE_HEIGHT;

            // POZOR, obdobny kod je jeste v HasDifferentSpacing()

            // control lezi vejskove v pasmu nasi skupiny
            if ((ctrlY >= outline.top && ctrlY < outline.bottom) ||                  // horni hrana controly lezi uprostred skupiny
                (ctrlY + ctrlH >= outline.top && ctrlY + ctrlH <= outline.bottom) || // dolni hrana controlu lezi uprostred skupiny
                (ctrlY < outline.top && ctrlY + ctrlH > outline.bottom))             // horni hrana lezi nad skupinou, dolni pod skupinou (control je vyssi nez cela skupina)
            {
                if ((ctrlX + ctrlW <= outline.left) && (outline.left - (ctrlX + ctrlW) < outerSpace->left))
                {
                    outerSpace->left = outline.left - (ctrlX + ctrlW);
                }
                if ((ctrlX >= outline.right) && (ctrlX - outline.right < outerSpace->right))
                {
                    outerSpace->right = ctrlX - outline.right;
                }
            }
            // control lezi sirkove v pasmu nasi skupiny
            if ((ctrlX >= outline.left && ctrlX < outline.right) ||
                (ctrlX + ctrlW >= outline.left && ctrlX + ctrlW <= outline.right) ||
                (ctrlX < outline.left && ctrlX + ctrlW > outline.right))
            {
                if ((ctrlY + ctrlH <= outline.top) && (outline.top - (ctrlY + ctrlH) < outerSpace->top))
                {
                    outerSpace->top = outline.top - (ctrlY + ctrlH);
                    //          TRACE_I("TOP:"<<control->TWindowName<<" ID:"<<control->ID);
                }
                if ((ctrlY >= outline.bottom) && (ctrlY - outline.bottom < outerSpace->bottom))
                {
                    outerSpace->bottom = ctrlY - outline.bottom;
                    //          TRACE_I("B: "<<control->TWindowName<<" ID:"<<control->ID);
                }
            }
        }
    }

    return TRUE;
}

void CLayoutInfo::Paint(HDC hDC)
{
    RECT r;
    GetClientRect(HWindow, &r);

    if (r.right > 0 && r.bottom > 0)
    {
        if (InfoBitmap == NULL)
        {
            InfoBitmap = new CBitmap();
            InfoBitmap->CreateBmp(hDC, r.right, r.bottom);
            DataChanged = TRUE;
        }
        else
        {
            if ((DWORD)r.right > InfoBitmap->GetWidth() || (DWORD)r.bottom > InfoBitmap->GetHeight())
            {
                InfoBitmap->Enlarge(r.right, r.bottom);
                DataChanged = TRUE;
            }
        }

        HDC hMemDC = InfoBitmap->HMemDC;
        if (DataChanged)
        {
            wchar_t buff[10000];
            buff[0] = 0;
            DataChanged = FALSE;
            FillRect(hMemDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
            SelectObject(hMemDC, HFont);
            if (DialogData != NULL)
            {
                wchar_t* p = buff;
                int selectedCount = DialogData->GetSelectedControlsCount();
                if (selectedCount > 0)
                {
                    int firstSelected = DialogData->SelCtrlsGetFirstControlIndex();
                    const CControl* control = DialogData->Controls[firstSelected];
                    BOOL sameTX, sameTY, sameTCX, sameTCY, sameRight, sameBottom;
                    if (GetSameProperties(&sameTX, &sameTY, &sameTCX, &sameTCY, &sameRight, &sameBottom))
                    {
                        p += swprintf_s(p, _countof(buff) - (p - buff), L"Items:%d", selectedCount);
                        if (sameTX)
                            p += swprintf_s(p, _countof(buff) - (p - buff), L" L:%d", control->TX);
                        if (sameTY)
                            p += swprintf_s(p, _countof(buff) - (p - buff), L" T:%d", control->TY);
                        if (sameRight)
                            p += swprintf_s(p, _countof(buff) - (p - buff), L" R:%d", control->TX + control->TCX);
                        if (sameBottom)
                            p += swprintf_s(p, _countof(buff) - (p - buff), L" B:%d", control->TY + control->TCY);
                        if (sameTCX)
                            p += swprintf_s(p, _countof(buff) - (p - buff), L" W:%d", control->TCX);
                        if (sameTCY)
                            p += swprintf_s(p, _countof(buff) - (p - buff), L" H:%d", control->TCY);
                        p += swprintf_s(p, _countof(buff) - (p - buff), L"\n");
                    }
                }
                if (DialogData->IsDialogSelected())
                {
                    p += swprintf_s(p, _countof(buff) - (p - buff), L"Item:1");
                    p += swprintf_s(p, _countof(buff) - (p - buff), L" W:%d", DialogData->TCX);
                    p += swprintf_s(p, _countof(buff) - (p - buff), L" H:%d", DialogData->TCY);
                    p += swprintf_s(p, _countof(buff) - (p - buff), L"\n");
                    RECT ctrlRect;
                    DialogData->GetControlsRect(FALSE, TRUE, &ctrlRect);
                    p += swprintf_s(p, _countof(buff) - (p - buff), L"Margins: L:%d T:%d R:%d B:%d\n",
                                    ctrlRect.left, ctrlRect.top, DialogData->TCX - ctrlRect.right, DialogData->TCY - ctrlRect.bottom);
                }
                RECT outerSpace;
                if (GetOuterSpace(&outerSpace))
                {
                    p += swprintf_s(p, _countof(buff) - (p - buff), L"Margins: L:%d T:%d R:%d B:%d\n",
                                    outerSpace.left, outerSpace.top, outerSpace.right, outerSpace.bottom);
                }

                if (selectedCount > 1)
                {
                    RECT outline = {0};
                    DialogData->SelCtrlsGetOuterRect(&outline);
                    p += swprintf_s(p, _countof(buff) - (p - buff), L"Group: L:%d T:%d R:%d B:%d W:%d H:%d\n", outline.left, outline.top, outline.right, outline.bottom, outline.right - outline.left, outline.bottom - outline.top);
                }

                p += swprintf_s(p, _countof(buff) - (p - buff), L"Dialog:%hs", DataRH.GetIdentifier(DialogData->ID));
                if (selectedCount == 1)
                {
                    int firstSelected = DialogData->SelCtrlsGetFirstControlIndex();
                    const CControl* control = DialogData->Controls[firstSelected];
                    p += swprintf_s(p, _countof(buff) - (p - buff), L" Control:%hs", DataRH.GetIdentifier(control->ID));
                }
                p += swprintf_s(p, _countof(buff) - (p - buff), L"\n");

                p += swprintf_s(p, _countof(buff) - (p - buff), L"Mouse X:%d Y:%d", MouseCurrent.x, MouseCurrent.y);
                if (ShowMouseDelta)
                {
                    p += swprintf_s(p, _countof(buff) - (p - buff), L" DX:%d DY:%d", MouseDeltaX, MouseDeltaY);
                }
                p += swprintf_s(p, _countof(buff) - (p - buff), L"\n");

                /*
        RECT outerSpace;
        BOOL sameTX, sameTY, sameTCX, sameTCY, sameRight, sameBottom;
        if (GetOuterSpace(&outerSpace) &&
            GetSameProperties(&sameTX, &sameTY, &sameTCX, &sameTCY, &sameRight, &sameBottom) &&
            (sameTX || sameTY || sameRight || sameBottom))
        {
          p += swprintf_s(p, _countof(buff) - (p - buff), L"Margins:%d", SelectedControls->Count);
          if (sameTX) p += swprintf_s(p, _countof(buff) - (p - buff), L" L:%d", outerSpace.left);
          if (sameTY) p += swprintf_s(p, _countof(buff) - (p - buff), L" T:%d", outerSpace.top);
          if (sameRight) p += swprintf_s(p, _countof(buff) - (p - buff), L" R:%d", outerSpace.right);
          if (sameBottom) p += swprintf_s(p, _countof(buff) - (p - buff), L" B:%d", outerSpace.bottom);
          p += swprintf_s(p, _countof(buff) - (p - buff), L"\n");
        }
*/
            }
            r.left += 8;
            r.top += 8;
            DrawTextW(hMemDC, buff, -1, &r, DT_LEFT);
        }

        BitBlt(hDC, 0, 0, r.right, r.bottom, hMemDC, 0, 0, SRCCOPY);
    }
}

LRESULT
CLayoutInfo::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        LOGFONT lf;
        GetFixedLogFont(&lf);
        HFont = HANDLES(CreateFontIndirect(&lf));

        HDC hDC = HANDLES(GetDC(NULL));
        HFONT oldFont = (HFONT)SelectObject(hDC, HFont);
        TEXTMETRIC tm;
        GetTextMetrics(hDC, &tm);
        FontHeight = tm.tmHeight;
        SelectObject(hDC, oldFont);
        HANDLES(ReleaseDC(NULL, hDC));
        break;
    }

    case WM_DESTROY:
    {
        if (HFont != NULL)
        {
            HANDLES(DeleteObject(HFont));
            HFont = NULL;
        }
        break;
    }

    case WM_ERASEBKGND:
    {
        return TRUE; // pozadi smazano
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
        Paint(hDC);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//*****************************************************************************

int GetProject0ItemIndex(HMENU hSubmenu)
{
    int count = GetMenuItemCount(hSubmenu);
    for (int i = 0; i < count; i++)
    {
        UINT id = GetMenuItemID(hSubmenu, i);
        if (id == ID_LAYOUT_PROJECT_0)
            return i;
    }
    return -1;
}

int GetProjectsSubmenuIndex(HMENU hMenu)
{
    int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; i++)
    {
        HMENU hSubmenu = GetSubMenu(hMenu, i);
        int project0Index = GetProject0ItemIndex(hSubmenu);
        if (project0Index != -1)
            return i;
    }
    return -1;
}

void DeleteProjectSubmenu(HMENU hMenu)
{
    int submenuIndex = GetProjectsSubmenuIndex(hMenu);
    if (submenuIndex == -1)
    {
        TRACE_E("Project submenu not found!");
        return;
    }
    DeleteMenu(hMenu, submenuIndex, MF_BYPOSITION);
}

void InsertProjectNamesToMenu(HMENU hMenu)
{
    int submenuIndex = GetProjectsSubmenuIndex(hMenu);
    if (submenuIndex == -1)
    {
        TRACE_E("Project submenu not found!");
        return;
    }
    HMENU hSubmenu = GetSubMenu(hMenu, submenuIndex);
    int project0Index = GetProject0ItemIndex(hSubmenu);
    if (project0Index == -1)
    {
        TRACE_E("Project0 index not found!");
        return;
    }
    DeleteMenu(hSubmenu, project0Index, MF_BYPOSITION);
    MENUITEMINFO mii;
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING;
    mii.fType = MIIM_STRING;
    const char* projects[] = {"Czech", "Dutch", "French", "German", "Hungarian", "Romanian", "Russian", "Slovak", "Spanish", "Chinese Simplified", NULL};
    for (int i = 0; projects[i] != NULL; i++)
    {
        mii.dwTypeData = (char*)projects[i];
        mii.wID = ID_LAYOUT_PROJECT_0 + i;
        InsertMenuItem(hSubmenu, project0Index + i, MF_BYPOSITION, &mii);
    }
}

//*****************************************************************************
//
// CLayoutWindow
//

CLayoutWindow::CLayoutWindow()
    : CWindow(ooStatic)
{
    DefWndProc = DefMDIChildProc;
}

void CLayoutWindow::OnInitMenu(HMENU hMenu)
{
    if (LayoutEditor.HWindow != NULL)
    {
        EnableMenuItem(hMenu, ID_LAYOUT_EDIT_UNDO, MF_BYCOMMAND | (LayoutEditor.HasUndo() ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_EDIT_REDO, MF_BYCOMMAND | (LayoutEditor.HasRedo() ? MF_ENABLED : MF_GRAYED));

        CDialogData* dialogData = LayoutEditor.TransformStack[LayoutEditor.CurrentDialog];
        BOOL selectedCount = dialogData->GetSelectedControlsCount();
        EnableMenuItem(hMenu, ID_LAYOUT_EDIT_DESELECTALL, MF_BYCOMMAND | (selectedCount > 0 ? MF_ENABLED : MF_GRAYED));

        BOOL selectedMore = selectedCount > 1;
        EnableMenuItem(hMenu, ID_LAYOUT_ALIGN_LEFT, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_ALIGN_RIGHT, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_ALIGN_TOP, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_ALIGN_BOTTOM, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_RESIZE_WIDTHLARGE, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_RESIZE_WIDTHSMALL, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_RESIZE_HEIGHTLARGE, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_RESIZE_HEIGHTSMALL, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_RESIZE_SETSIZE, MF_BYCOMMAND | (selectedCount > 0 ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_RESIZE_SIZETOCONTENT, MF_BYCOMMAND | (selectedCount > 0 ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_EQUAL_H_SPACING, MF_BYCOMMAND | (selectedCount > 1 ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_EQUAL_V_SPACING, MF_BYCOMMAND | (selectedCount > 1 ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_CENTER_H, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_CENTER_V, MF_BYCOMMAND | (selectedMore ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_CENTER_H2D, MF_BYCOMMAND | (selectedCount > 0 ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_CENTER_V2D, MF_BYCOMMAND | (selectedCount > 0 ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, ID_LAYOUT_ALIGN_EDGE_TO, MF_BYCOMMAND | (selectedCount > 0 ? MF_ENABLED : MF_GRAYED));

        int transformStackCount = LayoutEditor.TransformStack.Count;
        EnableMenuItem(hMenu, ID_LAYOUT_TOOLS_PREVIEWUNCHANGED, MF_BYCOMMAND | (transformStackCount > 1 ? MF_ENABLED : MF_GRAYED));

        CheckMenuItem(hMenu, ID_LAYOUT_TOOLS_OUTLINE, MF_BYCOMMAND | (Config.EditorOutlineControls ? MF_CHECKED : MF_UNCHECKED));
    }
}

void CLayoutWindow::SetupMenu(HMENU hMenu)
{
    if (Data.IsEnglishProject())
        InsertProjectNamesToMenu(hMenu);
    else
        DeleteProjectSubmenu(hMenu);
}

BOOL CLayoutWindow::OpenLayoutEditorForProject()
{
    if (!Data.IsEnglishProject())
        return FALSE;
    return LayoutEditor.StartNewTranslatorWithLayoutEditor();
}

LRESULT
CLayoutWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        LayoutInfo.Create(CWINDOW_CLASSNAME,
                          "",
                          WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
                          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                          HWindow,
                          NULL,
                          HInstance,
                          &LayoutInfo);

        LayoutEditor.Create(CWINDOW_CLASSNAME,
                            "",
                            WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE, // okno si zobrazime a zhasneme az bude potreba
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                            HWindow,
                            NULL,
                            HInstance,
                            &LayoutEditor);

        DialogHolder.Create(CWINDOW_CLASSNAME,
                            "",
                            WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                            HWindow,
                            NULL,
                            HInstance,
                            &DialogHolder);

        LayoutEditor.HDialogHolder = DialogHolder.HWindow;

        SetWindowPos(LayoutEditor.HWindow, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        LayoutEditor.Open(PreviewWindow.CurrentDialog, PreviewWindow.HighlightedControlID);
        LayoutEditor.RebuildDialog();
        SetFocus(LayoutEditor.HWindow);

        break;
    }

    case WM_CLOSE:
    {
        PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_EXIT, 0); // bezpecny close-window
        return 0;
    }

    case WM_COMMAND:
    {
        int repeatArrow = LayoutEditor.RepeatArrow;
        LayoutEditor.RepeatArrow = -1;

        switch (LOWORD(wParam))
        {
        case ID_LAYOUT_EDIT_UNDO:
        {
            LayoutEditor.OnUndo();
            return 0;
        }

        case ID_LAYOUT_EDIT_SELECTALL:
        {
            LayoutEditor.SelectAllControls(TRUE);
            return 0;
        }

        case ID_LAYOUT_EDIT_DESELECTALL:
        {
            LayoutEditor.SelectAllControls(FALSE);
            return 0;
        }

        case ID_LAYOUT_EDIT_REDO:
        {
            LayoutEditor.OnRedo();
            return 0;
        }

        case ID_LAYOUT_TOOLS_OUTLINE:
        {
            Config.EditorOutlineControls = !Config.EditorOutlineControls;
            LayoutEditor.UpdateBuffer();
            return 0;
        }

        case ID_LAYOUT_TOOLS_NORMALIZE_STD:
        {
            LayoutEditor.NormalizeLayout(FALSE);
            return 0;
        }

        case ID_LAYOUT_TOOLS_NORMALIZE_WIDE:
        {
            LayoutEditor.NormalizeLayout(TRUE);
            return 0;
        }

        case ID_LAYOUT_ALIGN_EDGE_TO:
        {
            LayoutEditor.SetAlignToMode(TRUE);
            return 0;
        }

        case ID_LAYOUT_TOOLS_PREVIEWORIGINAL:
        {
            LayoutEditor.BeginPreviewLayout(TRUE, (GetKeyState('P') & 0x8000) == 0); // musi odpovidat akceleratorum
            return 0;
        }

        case ID_LAYOUT_TOOLS_TESTDLGBOX:
        {
            LayoutEditor.TestDialogBox();
            return 0;
        }

        case ID_LAYOUT_TOOLS_PREVIEWUNCHANGED:
        {
            LayoutEditor.BeginPreviewLayout(FALSE, (GetKeyState('U') & 0x8000) == 0); // musi odpovidat akceleratorum
            return 0;
        }

        case ID_LAYOUT_TOOLS_RESET:
        {
            LayoutEditor.ResetTemplateToOriginal();
            return 0;
        }

        case ID_LAYOUT_ALIGN_LEFT:
        {
            LayoutEditor.AlignControls(edgeLeft);
            return 0;
        }

        case ID_LAYOUT_ALIGN_RIGHT:
        {
            LayoutEditor.AlignControls(edgeRight);
            return 0;
        }

        case ID_LAYOUT_ALIGN_TOP:
        {
            LayoutEditor.AlignControls(edgeTop);
            return 0;
        }

        case ID_LAYOUT_ALIGN_BOTTOM:
        {
            LayoutEditor.AlignControls(edgeBottom);
            return 0;
        }

        case ID_LAYOUT_RESIZE_WIDTHLARGE:
        {
            LayoutEditor.ResizeControls(rceWidthLarge);
            return 0;
        }

        case ID_LAYOUT_RESIZE_WIDTHSMALL:
        {
            LayoutEditor.ResizeControls(rceWidthSmall);
            return 0;
        }

        case ID_LAYOUT_RESIZE_HEIGHTLARGE:
        {
            LayoutEditor.ResizeControls(rceHeightLarge);
            return 0;
        }

        case ID_LAYOUT_RESIZE_SETSIZE:
        {
            LayoutEditor.SetSize();
            return 0;
        }

        case ID_LAYOUT_RESIZE_SIZETOCONTENT:
        {
            LayoutEditor.ControlsSizeToContent();
            return 0;
        }

        case ID_LAYOUT_RESIZE_HEIGHTSMALL:
        {
            LayoutEditor.ResizeControls(rceHeightSmall);
            return 0;
        }

        case ID_LAYOUT_NEXTCTRL:
        {
            LayoutEditor.NextPrevControl(TRUE);
            return 0;
        }

        case ID_LAYOUT_PREVCTRL:
        {
            LayoutEditor.NextPrevControl(FALSE);
            return 0;
        }

        case ID_LAYOUT_CENTER_H:
        {
            LayoutEditor.CenterControls(TRUE);
            return 0;
        }

        case ID_LAYOUT_CENTER_V:
        {
            LayoutEditor.CenterControls(FALSE);
            return 0;
        }

        case ID_LAYOUT_CENTER_H2D:
        {
            LayoutEditor.CenterControlsToDialog(TRUE);
            return 0;
        }

        case ID_LAYOUT_CENTER_V2D:
        {
            LayoutEditor.CenterControlsToDialog(FALSE);
            return 0;
        }

        case ID_EQUAL_H_SPACING:
        {
            LayoutEditor.LastRepeatArrow = repeatArrow;
            LayoutEditor.EqualSpacing(TRUE, repeatArrow);
            return 0;
        }

        case ID_EQUAL_V_SPACING:
        {
            LayoutEditor.LastRepeatArrow = repeatArrow;
            LayoutEditor.EqualSpacing(FALSE, repeatArrow);
            return 0;
        }

        case ID_LAYOUT_PROJECT_NEXT:
        {
            OpenLayoutEditorForProject();
            return 0;
        }
        }
        break;
    }

    case WM_SIZE:
    {
        if (DialogHolder.HWindow != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);

            HDWP hdwp = HANDLES(BeginDeferWindowPos(3));
            if (hdwp != NULL)
            {
                int infoHeight = LayoutInfo.GetNeededHeight();
                int divHeight = 4;
                hdwp = HANDLES(DeferWindowPos(hdwp, LayoutInfo.HWindow, NULL,
                                              0, r.bottom - infoHeight, r.right, r.bottom,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
                hdwp = HANDLES(DeferWindowPos(hdwp, DialogHolder.HWindow, NULL,
                                              0, 0, r.right, r.bottom - infoHeight - divHeight,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
                hdwp = HANDLES(DeferWindowPos(hdwp, LayoutEditor.HWindow, NULL,
                                              0, 0, r.right, r.bottom - infoHeight - divHeight,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
                HANDLES(EndDeferWindowPos(hdwp));
            }
        }
        break;
    }

    case WM_SETFOCUS:
    case WM_MDIACTIVATE:
    {
        if (LayoutEditor.HWindow != NULL)
            SetFocus(LayoutEditor.HWindow);
        break;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

CLayoutWindow* LayoutWindow = NULL;
