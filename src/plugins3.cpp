// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"
#include "cfgdlg.h"
#include "plugins.h"
#include "toolbar.h"
#include "gui.h"
#include <uxtheme.h>

//
// modul plugins3 je vyhrazen pro GUI vystavovane pluginum
//

//****************************************************************************
//
// Helper
//

BOOL CSalamanderGUI::CheckControlAndDeleteOnError(CWindow* control)
{
    if (control != NULL)
    {
        if (control->HWindow == NULL) // nepovedl se attach (duvod uz je v TRACE)
        {
            delete control;
            return FALSE;
        }
    }
    else
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    return TRUE;
}

//****************************************************************************
//
// ProgressBar
//

//
// CGUIProgressBar
//

void CGUIProgressBar::SetProgress(DWORD progress, const char* text)
{
    Control->SetProgress(progress, text);
}

void CGUIProgressBar::SetProgress2(const CQuadWord& progressCurrent, const CQuadWord& progressTotal,
                                   const char* text)
{
    Control->SetProgress2(progressCurrent, progressTotal, text);
}

void CGUIProgressBar::SetSelfMoveTime(DWORD time)
{
    Control->SetSelfMoveTime(time);
}

void CGUIProgressBar::SetSelfMoveSpeed(DWORD moveTime)
{
    Control->SetSelfMoveSpeed(moveTime);
}

void CGUIProgressBar::Stop()
{
    Control->Stop();
}

//
// CProgressBarForPlugin
//

class CProgressBarForPlugin : public CProgressBar
{
protected:
    CGUIProgressBar PluginIface; // rozhrani pro plugin

public:
    CProgressBarForPlugin(HWND hParent, int ctrlID)
        : CProgressBar(hParent, ctrlID) { PluginIface.SetControl(this); }

    CGUIProgressBarAbstract* GetIfaceForPlugin() { return &PluginIface; }
};

//
// CSalamanderGUI
//

CGUIProgressBarAbstract*
CSalamanderGUI::AttachProgressBar(HWND hParent, int ctrlID)
{
    CProgressBarForPlugin* control = new CProgressBarForPlugin(hParent, ctrlID);
    return CheckControlAndDeleteOnError(control) ? control->GetIfaceForPlugin() : NULL;
}

//****************************************************************************
//
// StaticText
//

//
// CGUIStaticText
//

BOOL CGUIStaticText::SetText(const char* text)
{
    return Control->SetText(text);
}

const char*
CGUIStaticText::GetText()
{
    return Control->GetText();
}

void CGUIStaticText::SetPathSeparator(char separator)
{
    Control->SetPathSeparator(separator);
}

BOOL CGUIStaticText::SetToolTipText(const char* text)
{
    return Control->SetToolTipText(text);
}

void CGUIStaticText::SetToolTip(HWND hNotifyWindow, DWORD id)
{
    Control->SetToolTip(hNotifyWindow, id);
}

//
// CStaticTextForPlugin
//

class CStaticTextForPlugin : public CStaticText
{
protected:
    CGUIStaticText PluginIface; // rozhrani pro plugin

public:
    CStaticTextForPlugin(HWND hParent, int ctrlID, DWORD flags)
        : CStaticText(hParent, ctrlID, flags) { PluginIface.SetControl(this); }

    CGUIStaticTextAbstract* GetIfaceForPlugin() { return &PluginIface; }
};

//
// CSalamanderGUI
//

CGUIStaticTextAbstract*
CSalamanderGUI::AttachStaticText(HWND hParent, int ctrlID, DWORD flags)
{
    CStaticTextForPlugin* control = new CStaticTextForPlugin(hParent, ctrlID, flags);
    return CheckControlAndDeleteOnError(control) ? control->GetIfaceForPlugin() : NULL;
}

//****************************************************************************
//
// HyperLink
//

//
// CGUIHyperLink
//

BOOL CGUIHyperLink::SetText(const char* text)
{
    return Control->SetText(text);
}

const char*
CGUIHyperLink::GetText()
{
    return Control->GetText();
}

void CGUIHyperLink::SetActionOpen(const char* file)
{
    Control->SetActionOpen(file);
}

void CGUIHyperLink::SetActionPostCommand(WORD command)
{
    Control->SetActionPostCommand(command);
}

BOOL CGUIHyperLink::SetActionShowHint(const char* text)
{
    return Control->SetActionShowHint(text);
}

BOOL CGUIHyperLink::SetToolTipText(const char* text)
{
    return Control->SetToolTipText(text);
}

void CGUIHyperLink::SetToolTip(HWND hNotifyWindow, DWORD id)
{
    Control->SetToolTip(hNotifyWindow, id);
}

//
// CHyperLinkForPlugin
//

class CHyperLinkForPlugin : public CHyperLink
{
protected:
    CGUIHyperLink PluginIface; // rozhrani pro plugin

public:
    CHyperLinkForPlugin(HWND hParent, int ctrlID, DWORD flags)
        : CHyperLink(hParent, ctrlID, flags) { PluginIface.SetControl(this); }

    CGUIHyperLinkAbstract* GetIfaceForPlugin() { return &PluginIface; }
};

//
// CSalamanderGUI
//

CGUIHyperLinkAbstract*
CSalamanderGUI::AttachHyperLink(HWND hParent, int ctrlID, DWORD flags)
{
    CHyperLinkForPlugin* control = new CHyperLinkForPlugin(hParent, ctrlID, flags);
    return CheckControlAndDeleteOnError(control) ? control->GetIfaceForPlugin() : NULL;
}

//****************************************************************************
//
// Button
//

BOOL CGUIButton::SetToolTipText(const char* text)
{
    return Control->SetToolTipText(text);
}

void CGUIButton::SetToolTip(HWND hNotifyWindow, DWORD id)
{
    Control->SetToolTip(hNotifyWindow, id);
}

//
// CButtonForPlugin
//

class CButtonForPlugin : public CButton
{
protected:
    CGUIButton PluginIface; // rozhrani pro plugin

public:
    CButtonForPlugin(HWND hParent, int ctrlID, DWORD flags)
        : CButton(hParent, ctrlID, flags) { PluginIface.SetControl(this); }

    CGUIButtonAbstract* GetIfaceForPlugin() { return &PluginIface; }
};

//
// CSalamanderGUI
//

CGUIButtonAbstract*
CSalamanderGUI::AttachButton(HWND hParent, int ctrlID, DWORD flags)
{
    CButtonForPlugin* control = new CButtonForPlugin(hParent, ctrlID, flags);
    return CheckControlAndDeleteOnError(control) ? control->GetIfaceForPlugin() : NULL;
}

//****************************************************************************
//
// ColorArrowButton
//

//
// CGUIColorArrowButton
//

void CGUIColorArrowButton::SetColor(COLORREF textColor, COLORREF bkgndColor)
{
    Control->SetColor(textColor, bkgndColor);
}

void CGUIColorArrowButton::SetTextColor(COLORREF textColor)
{
    Control->SetTextColor(textColor);
}

void CGUIColorArrowButton::SetBkgndColor(COLORREF bkgndColor)
{
    Control->SetBkgndColor(bkgndColor);
}

COLORREF
CGUIColorArrowButton::GetTextColor()
{
    return Control->GetTextColor();
}

COLORREF
CGUIColorArrowButton::GetBkgndColor()
{
    return Control->GetBkgndColor();
}

//
// CColorArrowButtonForPlugin
//

class CColorArrowButtonForPlugin : public CColorArrowButton
{
protected:
    CGUIColorArrowButton PluginIface; // rozhrani pro plugin

public:
    CColorArrowButtonForPlugin(HWND hParent, int ctrlID, BOOL showArrow)
        : CColorArrowButton(hParent, ctrlID, showArrow) { PluginIface.SetControl(this); }

    CGUIColorArrowButtonAbstract* GetIfaceForPlugin() { return &PluginIface; }
};

//
// CSalamanderGUI
//

CGUIColorArrowButtonAbstract*
CSalamanderGUI::AttachColorArrowButton(HWND hParent, int ctrlID, BOOL showArrow)
{
    CColorArrowButtonForPlugin* control = new CColorArrowButtonForPlugin(hParent, ctrlID, showArrow);
    return CheckControlAndDeleteOnError(control) ? control->GetIfaceForPlugin() : NULL;
}

//****************************************************************************
//
// ChangeToArrowButton
//

BOOL CSalamanderGUI::ChangeToArrowButton(HWND hParent, int ctrlID)
{
    return ::ChangeToArrowButton(hParent, ctrlID);
}

//****************************************************************************
//
// MenuPopup
//

CGUIMenuPopupAbstract*
CSalamanderGUI::CreateMenuPopup()
{
    return new CMenuPopup();
}

BOOL CSalamanderGUI::DestroyMenuPopup(CGUIMenuPopupAbstract* popup)
{
    if (popup == NULL)
    {
        TRACE_E("CSalamanderGUI::DestroyMenuPopup popup==NULL!");
        return FALSE;
    }
    delete (CMenuPopup*)popup;
    return TRUE;
}

//****************************************************************************
//
// MenuBar
//

CGUIMenuBarAbstract*
CSalamanderGUI::CreateMenuBar(CGUIMenuPopupAbstract* menu, HWND hNotifyWindow)
{
    return new CMenuBar((CMenuPopup*)menu, hNotifyWindow);
}

BOOL CSalamanderGUI::DestroyMenuBar(CGUIMenuBarAbstract* menuBar)
{
    if (menuBar == NULL)
    {
        TRACE_E("CSalamanderGUI::DestroyMenuBar menuBar==NULL!");
        return FALSE;
    }

    CMenuBar* bar = (CMenuBar*)menuBar;
    if (bar->HWindow != NULL)
        DestroyWindow(bar->HWindow);
    delete bar;
    return TRUE;
}

//****************************************************************************
//
// Toolbar support
//

BOOL CSalamanderGUI::CreateGrayscaleAndMaskBitmaps(HBITMAP hSource, COLORREF transparent,
                                                   HBITMAP& hGrayscale, HBITMAP& hMask)
{
    BOOL ret = ::CreateGrayscaleAndMaskBitmaps(hSource, transparent, hGrayscale, hMask);
    if (ret)
    { // handly se podavaji ven do pluginu, za jejich zruseni je odpovedny plugin, vyradime je ze Salamanderovskych HANDLES
        HANDLES_REMOVE(hGrayscale, __htHandle_comp_with_DeleteObject, "DeleteObject");
        HANDLES_REMOVE(hMask, __htHandle_comp_with_DeleteObject, "DeleteObject");
    }
    return ret;
}

//****************************************************************************
//
// Toolbar support
//

CGUIToolBarAbstract*
CSalamanderGUI::CreateToolBar(HWND hNotifyWindow)
{
    return new CToolBar(hNotifyWindow, ooStatic);
}

BOOL CSalamanderGUI::DestroyToolBar(CGUIToolBarAbstract* toolBar)
{
    if (toolBar == NULL)
    {
        TRACE_E("CSalamanderGUI::DestroyToolBar toolBar==NULL!");
        return FALSE;
    }

    CToolBar* bar = (CToolBar*)toolBar;
    if (bar->HWindow != NULL)
        DestroyWindow(bar->HWindow);
    delete bar;
    return TRUE;
}

//****************************************************************************
//
// ToolTip
//

void CSalamanderGUI::SetCurrentToolTip(HWND hNotifyWindow, DWORD id)
{
    ::SetCurrentToolTip(hNotifyWindow, id);
}

void CSalamanderGUI::SuppressToolTipOnCurrentMousePos()
{
    ::SuppressToolTipOnCurrentMousePos();
}

//****************************************************************************
//
// XP Visual Styles
//

BOOL CSalamanderGUI::DisableWindowVisualStyles(HWND hWindow)
{
    SetWindowTheme(hWindow, (L" "), (L" ")); // JRYFIXME - zrusit tohle API, od W7 lze volat primo
    return FALSE;
}

//****************************************************************************
//
// IconList
//

CGUIIconListAbstract*
CSalamanderGUI::CreateIconList()
{
    CIconList* il = new CIconList();
    //il->Dump = TRUE;
    return il;
}

BOOL CSalamanderGUI::DestroyIconList(CGUIIconListAbstract* iconList)
{
    if (iconList == NULL)
    {
        TRACE_E("CSalamanderGUI::DestroyIconList iconList==NULL!");
        return FALSE;
    }
    delete (CIconList*)iconList;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////
//
// ToolTip support
//

void CSalamanderGUI::PrepareToolTipText(char* buf, BOOL stripHotKey)
{
    char* p = buf;
    while (*p != '\t' && *p != 0)
        p++;
    if (*p == '\t')
    {
        if (!stripHotKey && *(p + 1) != 0)
        {
            *p = ' ';
            p++;
            memmove(p + 1, p, lstrlen(p) + 1);
            *p = '(';
            lstrcat(p, ")");
        }
        else
            *p = 0;
    }
}

///////////////////////////////////////////////////////////////////////////
//
// Subject with file/dir name truncated if needed
//

void CSalamanderGUI::SetSubjectTruncatedText(HWND subjectWnd, const char* subjectFormatString, const char* fileName,
                                             BOOL isDir, BOOL duplicateAmpersands)
{
    if (subjectFormatString == NULL || fileName == NULL)
    {
        TRACE_E("CSalamanderGUI::SetSubjectTruncatedText(): subjectFormatString==NULL or fileName==NULL!");
        return;
    }

    char formatedFileName[MAX_PATH];
    char tmpFileName[MAX_PATH];
    lstrcpyn(tmpFileName, fileName, MAX_PATH);
    AlterFileName(formatedFileName, tmpFileName, -1, Configuration.FileNameFormat, 0, isDir);

    CTruncatedString subject;
    subject.Set(subjectFormatString, formatedFileName);

    if (subject.TruncateText(subjectWnd))
    {
        if (duplicateAmpersands)
        {
            char buff[1000];
            lstrcpyn(buff, subject.Get(), 1000);
            DuplicateAmpersands(buff, 1000, TRUE);
            SetWindowText(subjectWnd, buff);
        }
        else
            SetWindowText(subjectWnd, subject.Get());
    }
}

//****************************************************************************
//
// Toolbar header
//

void CGUIToolbarHeader::EnableToolbar(DWORD enableMask)
{
    Control->EnableToolbar(enableMask);
}

void CGUIToolbarHeader::CheckToolbar(DWORD checkMask)
{
    Control->CheckToolbar(checkMask);
}

void CGUIToolbarHeader::SetNotifyWindow(HWND hWnd)
{
    Control->SetNotifyWindow(hWnd);
}

//
// CButtonForPlugin
//

class CToolbarHeaderForPlugin : public CToolbarHeader
{
protected:
    CGUIToolbarHeader PluginIface; // rozhrani pro plugin

public:
    CToolbarHeaderForPlugin(HWND hDlg, int ctrlID, HWND hAlignWindow, DWORD buttonMask)
        : CToolbarHeader(hDlg, ctrlID, hAlignWindow, buttonMask) { PluginIface.SetControl(this); }

    CGUIToolbarHeaderAbstract* GetIfaceForPlugin() { return &PluginIface; }
};

CGUIToolbarHeaderAbstract*
CSalamanderGUI::AttachToolbarHeader(HWND hParent, int ctrlID, HWND hAlignWindow, DWORD buttonMask)
{
    CToolbarHeaderForPlugin* control = new CToolbarHeaderForPlugin(hParent, ctrlID, hAlignWindow, buttonMask);
    return CheckControlAndDeleteOnError(control) ? control->GetIfaceForPlugin() : NULL;
}

void CSalamanderGUI::ArrangeHorizontalLines(HWND hWindow)
{
    ::ArrangeHorizontalLines(hWindow);
}

int CSalamanderGUI::GetWindowFontHeight(HWND hWindow)
{
    return ::GetWindowFontHeight(hWindow);
}
