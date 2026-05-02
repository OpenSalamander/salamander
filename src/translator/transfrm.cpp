// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndtree.h"
#include "wndframe.h"
#include "translator.h"
#include "versinfo.h"

//#include "wndout.h"
//#include "datarh.h"

CTransformation::CTransformation()
{
    Transformation = eloInit;
    Params = NULL;
    ParamsSize = 0;
}

CTransformation::~CTransformation()
{
    Clean();
}

void CTransformation::Clean()
{
    Transformation = eloInit;
    if (Params != NULL)
    {
        free(Params);
        Params = NULL;
    }
    ParamsSize = 0;
}

void CTransformation::CopyFrom(const CTransformation* src)
{
    Clean();
    Transformation = src->Transformation;
    ParamsSize = src->ParamsSize;
    if (src->ParamsSize > 0)
    {
        Params = malloc(src->ParamsSize);
        memcpy(Params, src->Params, src->ParamsSize);
    }
    else
    {
        Params = NULL;
    }
}

BOOL CTransformation::TryMergeWith(CTransformation* newTrans)
{
    if (Transformation != newTrans->Transformation)
        return FALSE;

    switch (Transformation)
    {
    case eloAlignTo:
    case eloSetSize:
    {
        return FALSE;
    }

    case eloInit:
    case eloTemplateReset:
    case eloSizeToContent:
    case eloSelect:
    case eloPreviewLayout:
    {
        return TRUE;
    }

    case eloAlignControls:
    case eloResizeControls:
    case eloCenterControls:
    case eloCenterControlsToDlg:
    {
        int param, paramNew;
        GetIntParam(&param);
        newTrans->GetIntParam(&paramNew);
        return param == paramNew;
    }

    case eloEqualSpacing:
    {
        int param, paramNew;
        int delta, deltaNew;
        Get2IntParams(&param, &delta);
        newTrans->Get2IntParams(&paramNew, &deltaNew);
        if (param == paramNew)
        {
            Set2IntParams(paramNew, deltaNew);
            return TRUE;
        }
        return FALSE;
    }

    case eloResize:
    case eloResizeDlg:
    {
        int horzOrEdge, horzOrEdgeNew;
        int delta, deltaNew;
        Get2IntParams(&horzOrEdge, &delta);
        newTrans->Get2IntParams(&horzOrEdgeNew, &deltaNew);
        if (horzOrEdge == horzOrEdgeNew)
        {
            Set2IntParams(horzOrEdge, delta + deltaNew);
            return TRUE;
        }
        return FALSE;
    }

    case eloMove:
    {
        int deltaX, deltaY;
        int deltaXNew, deltaYNew;
        Get2IntParams(&deltaX, &deltaY);
        newTrans->Get2IntParams(&deltaXNew, &deltaYNew);
        Set2IntParams(deltaX + deltaXNew, deltaY + deltaYNew);
        return TRUE;
    }
    }
    return FALSE;
}

void CTransformation::SetTransformationWithNoParam(CTransformationEnum transformation)
{
    switch (transformation)
    {
    case eloTemplateReset:
    case eloSizeToContent:
    case eloSelect:
    case eloPreviewLayout:
    case eloAlignTo:
        break;
    default:
        TRACE_E("Wrong parameters!");
    }
    Clean();
    Transformation = transformation;
}

void CTransformation::SetTransformationWithIntParam(CTransformationEnum transformation, int param)
{
    switch (transformation)
    {
    case eloNormalizeLayout:
    case eloAlignControls:
    case eloCenterControls:
    case eloCenterControlsToDlg:
    case eloResizeControls:
        break;
    default:
        TRACE_E("Wrong parameters!");
    }
    Clean();
    Transformation = transformation;
    SetIntParam(param);
}

void CTransformation::SetTransformationWith2IntParams(CTransformationEnum transformation, int param1, int param2)
{
    switch (transformation)
    {
    case eloMove:
    case eloResize:
    case eloResizeDlg:
    case eloEqualSpacing:
    case eloSetSize:
        break;
    default:
        TRACE_E("Wrong parameters!");
    }
    Clean();
    Transformation = transformation;
    Set2IntParams(param1, param2);
}

void CTransformation::SetIntParam(int param)
{
    if (Params != NULL)
        free(Params);
    ParamsSize = sizeof(int);
    Params = malloc(ParamsSize);
    *((int*)Params) = param;
}

void CTransformation::GetIntParam(int* param)
{
    if (Params == NULL || ParamsSize != sizeof(int))
    {
        TRACE_E("Wrong parameters!");
        return;
    }
    *param = *((int*)Params);
}

void CTransformation::Set2IntParams(int param1, int param2)
{
    if (Params != NULL)
        free(Params);
    ParamsSize = 2 * sizeof(int);
    Params = malloc(ParamsSize);
    *((int*)Params) = param1;
    *(((int*)Params) + 1) = param2;
}

void CTransformation::Get2IntParams(int* param1, int* param2)
{
    if (Params == NULL || ParamsSize != 2 * sizeof(int))
    {
        TRACE_E("Wrong parameters!");
        return;
    }
    *param1 = *((int*)Params);
    *param2 = *(((int*)Params) + 1);
}

void CTransformation::SetStructParam(void* param, int size)
{
    if ((size & 0x3) != 0) // ensure structure size is DWORD aligned
    {
        TRACE_E("Wrong structure size!");
        return;
    }
    if (Params != NULL)
        free(Params);
    Params = malloc(size);
    memcpy(Params, param, size);
    ParamsSize = size;
}

void CTransformation::GetStructParam(void* param, int size)
{
    if (Params == NULL || ParamsSize != size)
    {
        TRACE_E("Wrong parameters!");
        return;
    }
    memcpy(param, Params, size);
}

DWORD
CTransformation::GetStreamSize()
{
    return 4 + 4 + ParamsSize;
}

DWORD
CTransformation::WriteStream(BYTE* stream)
{
    *((DWORD*)stream) = Transformation;
    stream += 4;
    *((DWORD*)stream) = ParamsSize;
    stream += 4;
    if (ParamsSize > 0)
        memcpy(stream, Params, ParamsSize);
    return GetStreamSize();
}

DWORD
CTransformation::ReadStream(const BYTE* stream)
{
    Clean();
    Transformation = *((CTransformationEnum*)stream);
    stream += 4;
    ParamsSize = *((DWORD*)stream);
    stream += 4;
    if (ParamsSize > 0)
    {
        Params = malloc(ParamsSize);
        memcpy(Params, stream, ParamsSize);
    }
    else
    {
        Params = NULL;
    }
    return GetStreamSize();
}

void CDialogData::StoreSelectionToTransformation()
{
    if (Transformation.Transformation != eloSelect)
    {
        TRACE_E("Wrong transformation type!");
        return;
    }
    if (Transformation.Params != NULL)
        free(Transformation.Params);
    Transformation.ParamsSize = 4 * ((Controls.Count + 31) / 32);
    Transformation.Params = malloc(Transformation.ParamsSize);
    ZeroMemory(Transformation.Params, Transformation.ParamsSize);
    DWORD* bits = (DWORD*)Transformation.Params;
    for (int i = 0; i < Controls.Count; i++)
    {
        if (Controls[i]->Selected)
            bits[i / 32] |= 1 << (i % 32);
    }
}

void CDialogData::LoadSelectionFromTransformation()
{
    if (Transformation.Transformation != eloSelect)
    {
        TRACE_E("Wrong transformation type!");
        return;
    }
    if (Transformation.ParamsSize < 4 * ((Controls.Count + 31) / 32))
    {
        TRACE_E("Invalid parameters size!");
        return;
    }
    DWORD* bits = (DWORD*)Transformation.Params;
    for (int i = 0; i < Controls.Count; i++)
    {
        if ((bits[i / 32] & (1 << (i % 32))) != 0)
            Controls[i]->Selected = TRUE;
        else
            Controls[i]->Selected = FALSE;
    }
}

void CDialogData::ExecuteTransformation(HWND hDlg, CStringList* errors)
{
    switch (Transformation.Transformation)
    {
    case eloInit:
    {
        break;
    }

    case eloMove:
    {
        int deltaX;
        int deltaY;
        Transformation.Get2IntParams(&deltaX, &deltaY);
        CtrlsMove(TRUE, deltaX, deltaY);
        break;
    }

    case eloResize:
    {
        CEdgeEnum edge;
        int delta;
        Transformation.Get2IntParams((int*)&edge, &delta);
        SelCtrlsResize(edge, delta);
        break;
    }

    case eloResizeDlg:
    {
        BOOL horizontal;
        int delta;
        Transformation.Get2IntParams(&horizontal, &delta);
        DialogResize(horizontal, delta);
        break;
    }

    case eloSetSize:
    {
        int width;
        int height;
        Transformation.Get2IntParams(&width, &height);
        SelCtrlsSetSize(width, height);
        break;
    }

    case eloTemplateReset:
    {
        LoadOriginalLayout();
        break;
    }

    case eloNormalizeLayout:
    {
        BOOL wide;
        Transformation.GetIntParam(&wide);
        NormalizeLayout(wide, errors);
        break;
    }

    case eloAlignControls:
    {
        CEdgeEnum edge;
        Transformation.GetIntParam((int*)&edge);
        SelCtrlsAlign(edge);
        break;
    }

    case eloCenterControls:
    {
        BOOL horizontal;
        Transformation.GetIntParam(&horizontal);
        SelCtrlsCenter(horizontal);
        break;
    }

    case eloCenterControlsToDlg:
    {
        BOOL horizontal;
        Transformation.GetIntParam(&horizontal);
        SelCtrlsCenterToDialog(horizontal);
        break;
    }

    case eloEqualSpacing:
    {
        BOOL horizontal;
        int delta;
        Transformation.Get2IntParams(&horizontal, &delta);
        SelCtrlsEqualSpacing(horizontal, delta);
        break;
    }

    case eloResizeControls:
    {
        CResizeControlsEnum resize;
        Transformation.GetIntParam((int*)&resize);
        SelCtrlsResize(resize);
        break;
    }

    case eloSizeToContent:
    {
        SelCtrlsSizeToContent(hDlg);
        break;
    }

    case eloSelect:
    {
        LoadSelectionFromTransformation();
        break;
    }

    case eloAlignTo:
    {
        CAlignToParams params;
        Transformation.GetStructParam(&params, sizeof(params));
        HighlightIndex = params.HighlightIndex;
        SelCtrlsAlignTo(&params);
        break;
    }

    default:
    {
        TRACE_E("Unknown transformation!");
        break;
    }
    }
}
