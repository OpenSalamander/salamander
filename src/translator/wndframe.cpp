// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "translator.h"
#include "wndrh.h"
#include "wndtext.h"
#include "wndframe.h"
#include "wndtree.h"
#include "wndout.h"
#include "wndprev.h"
#include "wndlayt.h"
#include "datarh.h"
#include "config.h"
#include "dialogs.h"
#include "trlipc.h"

CFrameWindow FrameWindow;

const char* FRAMEWINDOW_NAME = "Translator";

BOOL RestartSalamander();
BOOL KillSalamanders();

BOOL PathAppend(char* path, const char* name, int pathSize)
{
    if (*name == '\\')
        name++;
    int l = strlen(path);
    if (l > 0 && path[l - 1] == '\\')
        l--;
    if (*name != 0)
    {
        int n = strlen(name);
        if (l + 1 + n < pathSize) // vejdeme se i s nulou na konci?
        {
            if (l != 0)
                path[l] = '\\';
            else
                l = -1;
            memcpy(path + l + 1, name, n + 1);
        }
        else
            return FALSE;
    }
    else
        path[l] = 0;
    return TRUE;
}

int GetRootPath(char* root, const char* path);

void GetFullPath(char* tgt, const char* name)
{
    if (*name == '\\' && *(name + 1) == '\\' || *name != 0 && *(name + 1) == ':')
        lstrcpyn(tgt, name, MAX_PATH);
    else
    {
        char curDir[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, curDir);
        if (*name == '\\')
            GetRootPath(tgt, curDir);
        else
            lstrcpyn(tgt, curDir, MAX_PATH);
        PathAppend(tgt, name, MAX_PATH);
    }
}

//*****************************************************************************
//
// CMainWindow
//

CFrameWindow::CFrameWindow()
    : CWindow(ooStatic)
{
    HMDIClient = NULL;
    HPredLayoutActiveWnd = NULL;
}

BOOL CFrameWindow::OpenChildWindows()
{
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    MDICREATESTRUCT mcs;

    mcs.szClass = MDICHILD_CLASSNAME;
    mcs.hOwner = HInstance;
    mcs.x = mcs.cx = CW_USEDEFAULT;
    mcs.y = mcs.cy = CW_USEDEFAULT;
    mcs.lParam = CW_USEDEFAULT;
    mcs.style = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_OVERLAPPED;

    mcs.szTitle = "Navigator";
    mcs.lParam = (LPARAM)&TreeWindow;
    if (SendMessage(HMDIClient, WM_MDICREATE, 0, (LONG)(LPMDICREATESTRUCT)&mcs) == NULL)
    {
        SetCursor(hOldCursor);
        return FALSE;
    }
    if (Config.TreeWindowPlacement.length != 0)
        SetWindowPlacement(TreeWindow.HWindow, &Config.TreeWindowPlacement);
    Data.FillTree();
    ShowWindow(TreeWindow.HWindow, SW_SHOW);

    mcs.szTitle = "Texts";
    mcs.lParam = (LPARAM)&TextWindow;
    if (SendMessage(HMDIClient, WM_MDICREATE, 0, (LONG)(LPMDICREATESTRUCT)&mcs) == NULL)
    {
        SetCursor(hOldCursor);
        return FALSE;
    }
    if (Config.TextWindowPlacement.length != 0)
        SetWindowPlacement(TextWindow.HWindow, &Config.TextWindowPlacement);
    ShowWindow(TextWindow.HWindow, SW_SHOW);

    mcs.szTitle = "Resource Symbols";
    mcs.lParam = (LPARAM)&RHWindow;
    if (SendMessage(HMDIClient, WM_MDICREATE, 0, (LONG)(LPMDICREATESTRUCT)&mcs) == NULL)
    {
        SetCursor(hOldCursor);
        return FALSE;
    }
    if (Config.RHWindowPlacement.length != 0)
        SetWindowPlacement(RHWindow.HWindow, &Config.RHWindowPlacement);
    DataRH.FillListBox();
    ShowWindow(RHWindow.HWindow, SW_SHOW);

    mcs.szTitle = "Output";
    mcs.lParam = (LPARAM)&OutWindow;
    if (SendMessage(HMDIClient, WM_MDICREATE, 0, (LONG)(LPMDICREATESTRUCT)&mcs) == NULL)
    {
        SetCursor(hOldCursor);
        return FALSE;
    }
    if (Config.OutWindowPlacement.length != 0)
        SetWindowPlacement(OutWindow.HWindow, &Config.OutWindowPlacement);
    ShowWindow(OutWindow.HWindow, SW_SHOW);

    mcs.szTitle = "Preview";
    mcs.lParam = (LPARAM)&PreviewWindow;
    if (SendMessage(HMDIClient, WM_MDICREATE, 0, (LONG)(LPMDICREATESTRUCT)&mcs) == NULL)
    {
        SetCursor(hOldCursor);
        return FALSE;
    }
    if (Config.PreviewWindowPlacement.length != 0)
        SetWindowPlacement(PreviewWindow.HWindow, &Config.PreviewWindowPlacement);
    ShowWindow(PreviewWindow.HWindow, SW_SHOW);

    SendMessage(HMDIClient, WM_MDIACTIVATE, (WPARAM)TreeWindow.HWindow, NULL);

    TreeWindow.EnableTreeNotifications = TRUE;

    // vybereme posledne vybranou polozku
    if (Data.SelectedTreeItem != 0)
    {
        HTREEITEM hItem = TreeWindow.GetItem(Data.SelectedTreeItem);
        if (hItem != NULL)
            TreeWindow.SelectItem(hItem);
    }

    SetCursor(hOldCursor);
    return TRUE;
}

void CFrameWindow::CloseChildWindows()
{
    if (TreeWindow.HWindow == NULL)
        return;

    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    Config.TreeWindowPlacement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(TreeWindow.HWindow, &Config.TreeWindowPlacement);
    Config.RHWindowPlacement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(RHWindow.HWindow, &Config.RHWindowPlacement);

    TextWindow.SaveColumnsWidth();
    Config.TextWindowPlacement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(TextWindow.HWindow, &Config.TextWindowPlacement);
    Config.OutWindowPlacement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(OutWindow.HWindow, &Config.OutWindowPlacement);
    Config.PreviewWindowPlacement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(PreviewWindow.HWindow, &Config.PreviewWindowPlacement);

    SendMessage(HMDIClient, WM_MDIDESTROY, (WPARAM)TreeWindow.HWindow, NULL);
    SendMessage(HMDIClient, WM_MDIDESTROY, (WPARAM)TextWindow.HWindow, NULL);
    SendMessage(HMDIClient, WM_MDIDESTROY, (WPARAM)RHWindow.HWindow, NULL);
    SendMessage(HMDIClient, WM_MDIDESTROY, (WPARAM)OutWindow.HWindow, NULL);
    SendMessage(HMDIClient, WM_MDIDESTROY, (WPARAM)PreviewWindow.HWindow, NULL);
    SetCursor(hOldCursor);
}

BOOL CFrameWindow::IsProjectOpened()
{
    return TreeWindow.HWindow != NULL;
}

void CFrameWindow::EnableNonLayoutWindows(BOOL enable)
{
    if (TreeWindow.HWindow == NULL)
        return;
    EnableWindow(TreeWindow.HWindow, enable);
    EnableWindow(TextWindow.HWindow, enable);
    EnableWindow(RHWindow.HWindow, enable);
    EnableWindow(OutWindow.HWindow, enable);
    EnableWindow(PreviewWindow.HWindow, enable);
}

BOOL CFrameWindow::OpenLayoutEditor()
{
    if (TreeWindow.HWindow == NULL || PreviewWindow.HDialog == NULL)
        return FALSE;

    HPredLayoutActiveWnd = (HWND)SendMessage(HMDIClient, WM_MDIGETACTIVE, 0, 0);
    LayoutWindow = new CLayoutWindow();

    MDICREATESTRUCT mcs;

    mcs.szClass = MDICHILD_CLASSNAME;
    mcs.hOwner = HInstance;
    mcs.x = mcs.cx = CW_USEDEFAULT;
    mcs.y = mcs.cy = CW_USEDEFAULT;
    mcs.lParam = CW_USEDEFAULT;
    mcs.style = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_OVERLAPPED;

    mcs.szTitle = "Layout Editor";
    mcs.lParam = (LPARAM)LayoutWindow;
    if (SendMessage(HMDIClient, WM_MDICREATE, 0, (LONG)(LPMDICREATESTRUCT)&mcs) == NULL)
        return FALSE;
    //  if (Config.TreeWindowPlacement.length != 0)
    //    SetWindowPlacement(TreeWindow.HWindow, &Config.TreeWindowPlacement);
    //  Data.FillTree();
    ShowWindow(LayoutWindow->HWindow, SW_MAXIMIZE);

    EnableNonLayoutWindows(FALSE);

    HMENU hBaseMenu = GetMenu(HWindow);
    HMENU hMenu = LoadMenu(HInstance, MAKEINTRESOURCE(IDM_LAYOUTMENU));
    LayoutWindow->SetupMenu(hMenu);
    SetMenu(HWindow, hMenu);
    DestroyMenu(hBaseMenu);
    return TRUE;
}

BOOL CFrameWindow::CloseLayoutEditor()
{
    if (LayoutWindow == NULL)
        return FALSE;

    if (!LayoutWindow->LayoutEditor.Close())
        return FALSE;

    EnableNonLayoutWindows(TRUE);

    FreeSharedMemory();

    DestroyWindow(LayoutWindow->HWindow);
    delete LayoutWindow;
    LayoutWindow = NULL;

    HMENU hMenu = GetMenu(HWindow);
    HMENU hBaseMenu = LoadMenu(HInstance, MAKEINTRESOURCE(IDM_MAINMENU));
    SetMenu(HWindow, hBaseMenu);
    DestroyMenu(hMenu);

    SendMessage(HMDIClient, WM_MDIACTIVATE, (WPARAM)HPredLayoutActiveWnd, 0);
    HPredLayoutActiveWnd = NULL;

    // vnutime refresh do preview okna
    WORD id = PreviewWindow.GetHighlightControl();
    PreviewWindow.PreviewDialog(PreviewWindow.CurrentDialogIndex);
    PreviewWindow.HighlightControl(id);

    return TRUE;
}

void CFrameWindow::SetTitle()
{
    char buff[MAX_PATH + 100];
    if (IsProjectOpened())
    {
        if (Data.MUIMode)
        {
            sprintf_s(buff, "MUI %s - %s", Config.LastMUITranslated, FRAMEWINDOW_NAME);
        }
        else
        {
            if (Data.IsDirty())
                sprintf_s(buff, "%s * - %s", Data.ProjectFile, FRAMEWINDOW_NAME);
            else
                sprintf_s(buff, "%s - %s", Data.ProjectFile, FRAMEWINDOW_NAME);
        }
    }
    else
    {
        sprintf_s(buff, "%s", FRAMEWINDOW_NAME);
    }
    SetWindowText(HWindow, buff);
}

BOOL CFrameWindow::QueryClose()
{
    if (IsProjectOpened())
    {
        if (Data.MUIMode)
        {
            int ret = MessageBox(GetMsgParent(), "Do you want to close MUI package?", FRAMEWINDOW_NAME, MB_OKCANCEL | MB_ICONQUESTION);
            if (ret == IDCANCEL)
                return FALSE;
        }
        else
        {
            if (Data.IsDirty())
            {
                char buff[MAX_PATH + 100];
                sprintf_s(buff, "Save changes to %s?", Data.ProjectFile);
                int ret = MessageBox(GetMsgParent(), buff, FRAMEWINDOW_NAME, MB_YESNOCANCEL | MB_ICONQUESTION);
                if (ret == IDCANCEL)
                    return FALSE;
                if (ret == IDYES)
                {
                    if (!TextWindow.CanLeaveText())
                        return FALSE;
                    if (!Data.Save())
                        return FALSE;
                    if (!Data.SaveProject())
                        return FALSE;
                }
            }
        }
    }
    return TRUE;
}

BOOL CFrameWindow::OpenProject(const char* importSubPath)
{
    // info do Output okna
    OutWindow.Clear();

    BOOL ret = FALSE;
    BOOL showOutro = TRUE;
    if (DataRH.Load(Data.FullIncludeFile) &&
        Data.LoadCheckLst(Data.FullCheckLstFile) &&
        Data.Load(Data.FullSourceFile, Data.FullTargetFile, FALSE) &&
        Data.LoadSalMenu(Data.FullSalMenuFile) &&
        Data.LoadIgnoreLst(Data.FullIgnoreLstFile))
    {
        Data.LookForIdConflicts();

        if ((QuietTranslate || QuietValidate && !DataRH.ContainsUnknownIdentifier) &&
            OutWindow.GetErrorLines() == 0)
        { // load probehl bez nalezeni chyb, pokracujeme validaci nebo prekladem
            showOutro = FALSE;
            BOOL completelyUntranslated = FALSE;
            if (QuietValidate)
                Data.ValidateTranslation(HWindow);
            else
                Data.FindUntranslated(&completelyUntranslated);
            if (QuietValidate && OutWindow.GetErrorLines() == 0 ||                           // validace probehla komplet OK, zavirame soft...
                QuietTranslate && (OutWindow.GetInfoLines() == 1 || completelyUntranslated)) // neni co prekladat (1. info radek je hlavicka hledani) nebo neni prelozene nic (to hlasime jen exit-codem)
            {
                DestroyWindow(HWindow);
                ExitProcess(QuietTranslate && completelyUntranslated ? 0 : 1); // ukoncime soft, jinak se jeste ukaze hlavni okno a dalsi nechtene veci + exit code = 1 znamena uspesnou validaci
            }
        }

        if (QuietMarkChAsTrl && OutWindow.GetErrorLines() == 0)
        { // load probehl bez nalezeni chyb, muzeme jit oznacit vsechny zmenene texty jako Translated
            showOutro = FALSE;
            Data.MarkChangedTextsAsTranslated();
            BOOL dirty = Data.IsDirty();
            if (!dirty || Data.Save() && Data.SaveProject())
            {
                DestroyWindow(HWindow);
                ExitProcess(dirty ? 0 : 1); // ukoncime soft, jinak se jeste ukaze hlavni okno a dalsi nechtene veci + exit code = 0 znamena ze doslo ke zmenam, 1 = nedoslo ke zmenam
            }
        }

        if (QuietImportTrlProp[0] != 0 && OutWindow.GetErrorLines() == 0)
        { // load probehl bez nalezeni chyb, muzeme importovat translation properties
            showOutro = FALSE;
            char importPath[MAX_PATH];
            lstrcpy(importPath, Data.ProjectFile);
            int backSlashCount = 0;
            char* p = importPath + strlen(importPath) - 1;
            while (p >= importPath && backSlashCount < 1)
            {
                if (*p == '\\')
                {
                    *p = 0;
                    backSlashCount++;
                }
                p--;
            }

            char project[MAX_PATH];
            strcpy_s(project, importPath);
            PathAppend(project, importSubPath, MAX_PATH);
            Data.Import(project, TRUE, FALSE);

            if (OutWindow.GetErrorLines() == 0) // import probehl komplet OK, ulozime data a zavirame soft...
            {
                if (Data.Save() && Data.SaveProject())
                {
                    DestroyWindow(HWindow);
                    ExitProcess(1); // ukoncime soft, jinak se jeste ukaze hlavni okno a dalsi nechtene veci + exit code = 1 znamena uspesnou validaci
                }
            }
        }

        if (QuietImport[0] != 0 && OutWindow.GetErrorLines() == 0)
        { // load probehl bez nalezeni chyb, pokracujeme importem stareho prekladu
            showOutro = FALSE;
            char importPath[MAX_PATH];
            lstrcpy(importPath, Data.ProjectFile);
            int backSlashCount = 0;
            char* p = importPath + strlen(importPath) - 1;
            while (p >= importPath && backSlashCount <= 3)
            {
                if (*p == '\\')
                {
                    *p = 0;
                    backSlashCount++;
                }
                p--;
            }

            backSlashCount = 0;
            const char* b = importSubPath;
            while (*b != 0)
            {
                if (*b == '\\')
                    backSlashCount++;
                b++;
            }
            if (backSlashCount < 3)
            {
                p = Data.ProjectFile + strlen(Data.ProjectFile) - 1;
                while (p >= Data.ProjectFile && backSlashCount < 3)
                {
                    if (*p == '\\')
                    {
                        backSlashCount++;
                    }
                    if (backSlashCount < 3)
                        p--;
                }

                char project[MAX_PATH];
                strcpy_s(project, importPath);
                PathAppend(project, importSubPath, MAX_PATH);
                PathAppend(project, p, MAX_PATH);
                Data.Import(project, FALSE, QuietImportOnlyDlgLayout != 0);

                if (OutWindow.GetErrorLines() == 0) // import probehl komplet OK, ulozime data a zavirame soft...
                {
                    if (Data.Save() && Data.SaveProject())
                    {
                        DestroyWindow(HWindow);
                        ExitProcess(1); // ukoncime soft, jinak se jeste ukaze hlavni okno a dalsi nechtene veci + exit code = 1 znamena uspesnou validaci
                    }
                }
            }
        }

        if ((QuietExportSpellChecker[0] != 0 ||
             QuietImportSLT[0] != 0 ||
             QuietExportSLT[0] != 0 ||
             QuietExportSDC[0] != 0) &&
            OutWindow.GetErrorLines() == 0)
        { // load probehl bez nalezeni chyb, muzeme exportovat/importovat SLT nebo exportovat texty pro spell checker
            showOutro = FALSE;

            const char* sltPath = QuietExportSpellChecker[0] != 0 ? QuietExportSpellChecker : QuietImportSLT[0] != 0 ? QuietImportSLT
                                                                                          : QuietExportSDC[0] != 0   ? QuietExportSDC
                                                                                                                     : QuietExportSLT;

            char fullSLTPath[MAX_PATH];
            BOOL isAbsolutePath = strlen(sltPath) > 2 && ((sltPath[0] == '\\' && sltPath[1] == '\\') || (sltPath[1] == ':'));
            if (isAbsolutePath)
                lstrcpy(fullSLTPath, sltPath);
            else
            {
                lstrcpy(fullSLTPath, Data.ProjectFile);
                *strrchr(fullSLTPath, '\\') = 0;
                PathAppend(fullSLTPath, sltPath, MAX_PATH);
            }
            PathAppend(fullSLTPath, strrchr(Data.ProjectFile, '\\'), MAX_PATH);
            char* ext = strrchr(fullSLTPath, '.') + 1;
            strcpy_s(ext, _countof(fullSLTPath) - (ext - fullSLTPath), QuietExportSpellChecker[0] != 0 ? "txt" : QuietExportSDC[0] != 0 ? "sdc"
                                                                                                                                        : "slt");

            BOOL doNotSaveData = TRUE;
            if (QuietExportSpellChecker[0] != 0)
            {
                Data.Export(fullSLTPath);
            }
            else
            {
                if (QuietExportSDC[0] != 0)
                {
                    Data.ExportDialogsAndControlsSizes(fullSLTPath);
                }
                else
                {
                    if (QuietImportSLT[0] == 0)
                    {
                        Data.ExportAsTextArchive(fullSLTPath, QuietExportSLTForDiff);
                        doNotSaveData = FALSE; // zmena SLGCRCofImpSLT na CRC exportleho SLT
                        Data.SetDirty();
                    }
                    else
                    {
                        if (Data.ImportTextArchive(fullSLTPath, TRUE))
                        {
                            Data.ImportTextArchive(fullSLTPath, FALSE);
                            doNotSaveData = FALSE;
                            Data.SetDirty();
                            Data.UpdateAllNodes(); // nastavime translated stavy na treeview
                            Data.UpdateTexts();    // update list view v okne Texts
                        }
                    }
                }
            }

            if (OutWindow.GetErrorLines() == 0) // quiet-operace probehla komplet OK, zavirame soft...
            {
                if (doNotSaveData || Data.Save() && Data.SaveProject())
                {
                    DestroyWindow(HWindow);
                    ExitProcess(1); // ukoncime soft, jinak se jeste ukaze hlavni okno a dalsi nechtene veci + exit code = 1 znamena uspesnou validaci
                }
            }
        }

        ret = TRUE;
        OpenChildWindows();
        SetTitle();
        Config.AddRecentProject(Data.ProjectFile);
    }

    // display outro
    if (showOutro)
    {
        wchar_t buff[1000];
        int errors = OutWindow.GetErrorLines();
        if (errors > 0)
            swprintf_s(buff, L"%d error(s) have been found.", errors);
        else
            swprintf_s(buff, L"No errors have been found.");
        OutWindow.AddLine(buff, mteSummary);
        if (errors == 0)
            PostMessage(HWindow, WM_FOCLASTINOUTPUTWND, 0, 0); // Output okno jeste nemusi existovat
        else
            PostMessage(HWindow, WM_COMMAND, CM_WND_OUTPUT, 0); // aktivace Output okna
    }
    return ret;
}

void CFrameWindow::ProcessCmdLineParams(char* argv[], int p)
{
    QuietImport[0] = 0;
    QuietImportOnlyDlgLayout = 0;
    QuietImportTrlProp[0] = 0;
    QuietExportSLT[0] = 0;
    QuietExportSDC[0] = 0;
    QuietExportSLTForDiff = FALSE;
    QuietImportSLT[0] = 0;
    QuietExportSpellChecker[0] = 0;
    if (p > 3)
        return;
    if (p == 2)
    {
        if (_stricmp(argv[0], "-quiet-validate-all") != 0 &&
            _stricmp(argv[0], "-quiet-validate-layout") != 0 &&
            _stricmp(argv[0], "-quiet-translate") != 0 &&
            _stricmp(argv[0], "-quiet-mark-changed-as-translated") != 0)
        {
            return;
        }
        else
        {
            if (_stricmp(argv[0], "-quiet-mark-changed-as-translated") == 0)
            {
                QuietMarkChAsTrl = 1;
            }
            else
            {
                if (_stricmp(argv[0], "-quiet-translate") == 0)
                {
                    QuietTranslate = 1;
                }
                else
                {
                    QuietValidate = _stricmp(argv[0], "-quiet-validate-all") == 0 ? 1 : 2;
                    Config.SetValidateAll(QuietValidate);
                }
            }
        }
    }
    if (p == 3)
    {
        if (_stricmp(argv[0], "-quiet-import") != 0 &&
            _stricmp(argv[0], "-quiet-import-only-dialog-layout") != 0 &&
            _stricmp(argv[0], "-quiet-import-trlprop") != 0 &&
            _stricmp(argv[0], "-quiet-export-slt") != 0 &&
            _stricmp(argv[0], "-quiet-export-sizes") != 0 &&
            _stricmp(argv[0], "-quiet-import-slt") != 0 &&
            _stricmp(argv[0], "-quiet-export-slt-for-diff") != 0 &&
            _stricmp(argv[0], "-quiet-export-spellcheck") != 0 &&
            _stricmp(argv[0], "-open-layout-editor") != 0)
        {
            return;
        }
        else
        {
            if (_stricmp(argv[0], "-quiet-import") == 0 ||
                _stricmp(argv[0], "-quiet-import-only-dialog-layout") == 0)
            {
                lstrcpy(QuietImport, argv[1]);
                QuietImportOnlyDlgLayout = _stricmp(argv[0], "-quiet-import-only-dialog-layout") == 0;
            }
            if (_stricmp(argv[0], "-quiet-import-trlprop") == 0)
                lstrcpy(QuietImportTrlProp, argv[1]);
            QuietExportSLTForDiff = _stricmp(argv[0], "-quiet-export-slt-for-diff") == 0;
            if (_stricmp(argv[0], "-quiet-export-slt") == 0 || QuietExportSLTForDiff)
                lstrcpy(QuietExportSLT, argv[1]);
            if (_stricmp(argv[0], "-quiet-export-sizes") == 0)
                lstrcpy(QuietExportSDC, argv[1]);
            if (_stricmp(argv[0], "-quiet-import-slt") == 0)
                lstrcpy(QuietImportSLT, argv[1]);
            if (_stricmp(argv[0], "-quiet-export-spellcheck") == 0)
                lstrcpy(QuietExportSpellChecker, argv[1]);
            if (_stricmp(argv[0], "-open-layout-editor") == 0)
                lstrcpy(OpenLayoutEditorDialogID, argv[1]);
        }
    }
    char file[MAX_PATH];
    GetFullPath(file, argv[p - 1]);
    CloseChildWindows();
    Data.Clean();
    SetTitle();
    if (Data.LoadProject(file))
    {
        int qval = QuietValidate;
        int qtran = QuietTranslate;
        if (OpenLayoutEditorDialogID[0] != 0)
            Data.SelectedTreeItem = TREE_TYPE_DIALOG | atoi(OpenLayoutEditorDialogID);
        OpenProject(argv[1]);
        if (qval)
            PostMessage(HWindow, WM_VALIDATIONFAILED, 0, 0);
        if (qtran)
            PostMessage(HWindow, WM_UNTRANSLATEDFOUND, 0, 0);
        if (OpenLayoutEditorDialogID[0] != 0)
        {
            if (ReadSharedMemory())
                PostMessage(HWindow, WM_COMMAND, ID_TOOLS_EDITLAYOUT, 0);
        }
    }

    QuietValidate = 0; // ted uz se budeme chovat normalne, zadny dalsi automaticky kontroly a zavirani softu
    QuietTranslate = 0;
    QuietMarkChAsTrl = 0;
    QuietImport[0] = 0;
    QuietImportOnlyDlgLayout = 0;
    QuietImportTrlProp[0] = 0;
    QuietExportSLT[0] = 0;
    QuietExportSDC[0] = 0;
    QuietExportSLTForDiff = FALSE;
    QuietImportSLT[0] = 0;
    QuietExportSpellChecker[0] = 0;
}

BOOL CFrameWindow::OnSave()
{
    if (!IsProjectOpened() || Data.MUIMode)
        return FALSE;
    Data.Save();
    Data.SaveProject();
    SetTitle();
    return TRUE;
}

LRESULT
CFrameWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        CLIENTCREATESTRUCT ccs;

        // Retrieve the handle to the window menu and assign the
        // first child window identifier.

        ccs.hWindowMenu = GetSubMenu(GetMenu(HWindow), 0);
        ccs.idFirstChild = 111 /*IDM_WINDOWCHILD*/;

        // Create the MDI client window.

        HMDIClient = CreateWindowEx(WS_EX_CLIENTEDGE, "MDICLIENT", (LPCTSTR)NULL,
                                    WS_CHILD | WS_CLIPCHILDREN | MDIS_ALLCHILDSTYLES,
                                    0, 0, 0, 0, HWindow, (HMENU)0xCAC, HInstance, (LPSTR)&ccs);

        ShowWindow(HMDIClient, SW_SHOW);
        DragAcceptFiles(HWindow, TRUE);
        break;
    }

    case WM_SIZE:
    {
        if (HMDIClient != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            SetWindowPos(HMDIClient, NULL, 0, 0, r.right, r.bottom, SWP_NOZORDER);
        }
        break;
    }

    case WM_DESTROY:
    {
        if (!QuietMarkChAsTrl && !QuietTranslate && !QuietValidate && QuietImport[0] == 0 &&
            QuietImportTrlProp[0] == 0 && QuietExportSLT[0] == 0 && QuietExportSDC[0] == 0 &&
            QuietImportSLT[0] == 0 && QuietExportSpellChecker[0] == 0)
        {
            Config.Save();
        }
        DragAcceptFiles(HWindow, FALSE);
        PostQuitMessage(0);
        return 0;
    }

    case WM_CLOSE:
    {
        PostMessage(HWindow, WM_COMMAND, CM_EXIT, 0); // bezpecny close-window
        return 0;
    }

    case WM_AUTORELOAD:
    {
        if (Config.ReloadProjectAtStartup)
        {
            if (Config.LastProject[0] != 0)
                if (Data.LoadProject(Config.LastProject))
                    OpenProject();
        }
        break;
    }

    case WM_VALIDATIONFAILED:
    {
        MessageBox(GetMsgParent(), "Quiet validation has failed. See Output window for errors.",
                   ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return 0;
    }

    case WM_UNTRANSLATEDFOUND:
    {
        MessageBox(GetMsgParent(), "Untranslated strings have been found. See list of these strings in Output window.",
                   FRAMEWINDOW_NAME, MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    case WM_FOCLASTINOUTPUTWND:
    {
        OutWindow.FocusLastItem();
        return 0;
    }

    case WM_DROPFILES:
    {
        UINT drag;
        char path[MAX_PATH];

        drag = DragQueryFile((HDROP)wParam, 0xFFFFFFFF, NULL, 0); // kolik souboru nam hodili
        if (drag > 0)
        {
            DragQueryFile((HDROP)wParam, 0, path, MAX_PATH);
            if (QueryClose())
            {
                CloseChildWindows();
                Data.Clean();
                SetTitle();
                if (Data.LoadProject(path))
                    OpenProject();
            }
        }
        DragFinish((HDROP)wParam);
        break;
    }

    case WM_INITMENU:
    {
        HMENU hMenu = GetMenu(HWindow);
        BOOL open = IsProjectOpened();
        BOOL mui = Data.MUIMode;
        BOOL exe = Data.SalamanderExeFile[0] != 0;
        EnableMenuItem(hMenu, CM_SAVE, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_CLOSE, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_IMPORT, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_EXPORT, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        //      EnableMenuItem(hMenu, CM_VALIDATE, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_PROPERTIES, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_WND_NEXT, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_WND_PREVIOUS, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_WND_CASCADE, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_WND_TITLE, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_WND_TREE, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_WND_RH, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_WND_TEXT, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_WND_OUTPUT, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_WND_PREVIEW, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_EDIT_FIND, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_FILE_IMPORTTEXT, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_FILE_EXPORTTEXT, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_EDIT_FINDUNTRANSLATED, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_EDIT_FINDRELAYOUT, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_EDIT_FINDOUTER, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_HOTKEY_CONFLICTS, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);

        EnableMenuItem(hMenu, ID_TOOLS_RESTART, MF_BYCOMMAND | (open && !mui && exe) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_TOOLS_KILL, MF_BYCOMMAND | (open && !mui && exe) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_TOOLS_MARKASTR, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_TOOLS_RESETDLGSTOORIG, MF_BYCOMMAND | (open && !mui) ? MF_ENABLED : MF_GRAYED);

        EnableMenuItem(hMenu, ID_EDIT_TRANSLATED, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);

        BOOL bookmarkExist = Data.GetBookmarksCount() > 0;
        EnableMenuItem(hMenu, ID_EDIT_TOGGLEBOOKMARK, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_EDIT_NEXTBOOKMARK, MF_BYCOMMAND | open && bookmarkExist ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_EDIT_PREVIOUSEBOOKMARK, MF_BYCOMMAND | open && bookmarkExist ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_EDIT_CLEARBOOKMARKS, MF_BYCOMMAND | open && bookmarkExist ? MF_ENABLED : MF_GRAYED);

        int lines = OutWindow.GetSelectableLinesCount();
        EnableMenuItem(hMenu, CM_GOTO_NEXT, MF_BYCOMMAND | (lines > 0) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, CM_GOTO_PREV, MF_BYCOMMAND | (lines > 0) ? MF_ENABLED : MF_GRAYED);

        if (LayoutWindow != NULL)
            LayoutWindow->OnInitMenu(hMenu);

        BOOL canEditDialog = FALSE;
        if (open && PreviewWindow.HDialog != NULL)
            canEditDialog = TRUE;
        EnableMenuItem(hMenu, ID_TOOLS_EDITLAYOUT, MF_BYCOMMAND | (canEditDialog) ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_TOOLS_CLEARDLGRELAYOUT, MF_BYCOMMAND | (canEditDialog) ? MF_ENABLED : MF_GRAYED);

        EnableMenuItem(hMenu, ID_PREVIEW_EXTENDDLG, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_PREVIEW_OUTLINECTRLS, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, ID_PREVIEW_FILLCTRLS, MF_BYCOMMAND | open ? MF_ENABLED : MF_GRAYED);
        CheckMenuItem(hMenu, ID_PREVIEW_EXTENDDLG, MF_BYCOMMAND | (Config.PreviewExtendDialogs ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(hMenu, ID_PREVIEW_OUTLINECTRLS, MF_BYCOMMAND | (Config.PreviewOutlineControls ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(hMenu, ID_PREVIEW_FILLCTRLS, MF_BYCOMMAND | (Config.PreviewFillControls ? MF_CHECKED : MF_UNCHECKED));

        HMENU hFile = GetSubMenu(hMenu, 0);
        HMENU hRecent = GetSubMenu(hFile, 1);
        if (hRecent != NULL)
        {
            int count = GetMenuItemCount(hRecent);
            while (count >= 0)
            {
                DeleteMenu(hRecent, count - 1, MF_BYPOSITION);
                count--;
            }
            count = 0;
            for (int i = 0; i < RECENT_PROJECTS_COUNT; i++)
            {
                if (Config.RecentProjects[i][0] != 0)
                {
                    AppendMenu(hRecent, MF_STRING, CM_OPEN_RECENT_FIRST + i, Config.RecentProjects[i]);
                    count++;
                }
                else
                    break;
            }
            if (count == 0)
                AppendMenu(hRecent, MF_STRING | MF_GRAYED, CM_OPEN_RECENT_FIRST, "Empty");
        }

        break;
    }

    case WM_COMMAND:
    {
        if (!TextWindow.CanLeaveText())
            return 0;

        if (LayoutWindow != NULL && LOWORD(wParam) != ID_LAYOUT_FILE_EXIT && LOWORD(wParam) != CM_CLOSE &&
            LOWORD(wParam) != CM_EXIT && LOWORD(wParam) != CM_ABOUT && LOWORD(wParam) != ID_HELP_SHOWREADME)
        {
            return SendMessage(LayoutWindow->HWindow, uMsg, wParam, lParam);
        }

        if (LOWORD(wParam) >= CM_OPEN_RECENT_FIRST && LOWORD(wParam) <= CM_OPEN_RECENT_LAST)
        {
            int index = LOWORD(wParam) - CM_OPEN_RECENT_FIRST;
            if (Config.RecentProjects[index][0] != 0)
            {
                if (QueryClose())
                {
                    CloseChildWindows();
                    Data.Clean();
                    SetTitle();

                    if (Data.LoadProject(Config.RecentProjects[index]))
                    {
                        OpenProject();
                    }
                    else
                        Config.RemoveRecentProject(index);
                }
            }
            return 0;
        }

        switch (LOWORD(wParam))
        {
            /*
        case CM_NEW:
        {
          CNewDialog dlg(HWindow);
          {
            if (dlg.Execute() == IDOK)
            {

              if (QueryClose())
              {
                CloseChildWindows();
                Data.Clean();
                strcpy_s(Data.ProjectFile, dlg.ProjectFile);
                strcpy_s(Data.SourceFile, dlg.SourceFile);
                strcpy_s(Data.TargetFile, dlg.TargetFile);
                strcpy_s(Data.IncludeFile, dlg.IncludeFile);
                SetTitle();

                if (Data.SaveProject())
                {
                  OpenProject();
                }
              }
            }
          }

          return 0;
        }
*/

        case CM_OPEN:
        {
            char file[MAX_PATH];
            file[0] = 0;
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = HWindow;
            ofn.lpstrFilter = "Translator Project (*.atp)\0*.atp\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn))
            {
                if (QueryClose())
                {
                    CloseChildWindows();
                    Data.Clean();
                    SetTitle();

                    if (Data.LoadProject(file))
                    {
                        OpenProject();
                    }
                }
            }
            return 0;
        }

        case CM_SAVE:
        {
            return OnSave();
        }
            /*
        case CM_VALIDATE:
        {
          if (!IsProjectOpened())
            return 0;
          Data.Validate();
          return 0;
        }
*/
        case CM_CLOSE:
        {
            if (!IsProjectOpened())
                return 0;
            if (QueryClose())
            {
                CloseChildWindows();
                Data.Clean();
                SetTitle();
            }
            return 0;
        }

        case CM_IMPORT:
        {
            if (!IsProjectOpened() || Data.MUIMode)
                return 0;
            char project[MAX_PATH];
            project[0] = 0;

#ifdef _DEBUG
            // FIXME - pro ladeni
            strcpy_s(project, "C:\\TRANSLATOR\\Salamand 2.53 beta 1\\projects\\czech\\salamand.atp");
#endif

            CImportDialog dlg(HWindow, project);
            if (dlg.Execute() == IDOK)
            {
                if (Data.Import(project, FALSE, FALSE))
                {
                }
            }
            return 0;
        }

        case CM_EXPORT:
        {
            if (!IsProjectOpened() || Data.MUIMode)
                return 0;

            char fileName[MAX_PATH];
            strcpy_s(fileName, Data.ExportFile);
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = HWindow;
            ofn.lpstrFilter = "Text file (*.txt)\0*.txt\0";
            ofn.lpstrDefExt = "txt";
            ofn.lpstrFile = fileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.nFilterIndex = 1;
            ofn.lpstrTitle = "Export Translation";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_NOCHANGEDIR |
                        OFN_OVERWRITEPROMPT;

            if (GetSaveFileName(&ofn))
            {
                strcpy_s(Data.ExportFile, fileName);
                Data.SetDirty();
                Data.Export(fileName);
            }
            return 0;
        }

        case ID_FILE_EXPORTTEXT:
        {
            if (!IsProjectOpened() || Data.MUIMode)
                return 0;

            char fileName[MAX_PATH];
            strcpy_s(fileName, Data.ExportAsTextArchiveFile);
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = HWindow;
            ofn.lpstrFilter = "Salamander Language Translation (*.slt)\0*.slt\0";
            ofn.lpstrDefExt = "slt";
            ofn.lpstrFile = fileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.nFilterIndex = 1;
            ofn.lpstrTitle = "Export Translation as Text Archive";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_NOCHANGEDIR /* | 
                      OFN_OVERWRITEPROMPT*/
                ;                                                                              // overwrite prompt je az v Data.ExportAsTextArchive

            if (GetSaveFileName(&ofn))
            {
                strcpy_s(Data.ExportAsTextArchiveFile, fileName);
                Data.SetDirty();
                Data.ExportAsTextArchive(fileName, FALSE);
            }
            return 0;
        }

        case ID_FILE_IMPORTTEXT:
        {
            if (!IsProjectOpened() || Data.MUIMode)
                return 0;

            char fileName[MAX_PATH];
            strcpy_s(fileName, Data.ExportAsTextArchiveFile);
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = HWindow;
            ofn.lpstrFilter = "Salamander Language Translation (*.slt)\0*.slt\0";
            ofn.lpstrFile = fileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn))
            {
                strcpy_s(Data.ExportAsTextArchiveFile, fileName);
                if (Data.ImportTextArchive(fileName, TRUE))
                {
                    Data.ImportTextArchive(fileName, FALSE);
                    Data.SetDirty();
                    Data.UpdateAllNodes(); // nastavime translated stavy na treeview
                    Data.UpdateTexts();    // update list view v okne Texts
                }
            }
            return 0;
        }

        case ID_EDIT_TRANSLATED:
        {
            if (!IsProjectOpened())
                return 0;

            SendMessage(HMDIClient, WM_MDIACTIVATE, (WPARAM)TextWindow.HWindow, NULL);
            TextWindow.SetFocusTranslated();
            return 0;
        }

        case ID_FILE_OPENMUI:
        {
            CImportMUIDialog dlg(HWindow);
            {
                if (dlg.Execute() == IDOK)
                {
                    if (QueryClose())
                    {
                        CloseChildWindows();
                        Data.Clean();
                        SetTitle();

                        if (Data.LoadMUIPackages(Config.LastMUIOriginal, Config.LastMUITranslated))
                        {
                            OpenChildWindows();
                            SetTitle();
                        }
                    }
                }
            }

            return 0;
        }

        case CM_EXIT:
        {
            if (LayoutWindow != NULL)
            {
                // jsme v layout editoru
                PostMessage(HWindow, WM_COMMAND, ID_LAYOUT_FILE_EXIT, 0);
                return 0;
            }

            if (IsProjectOpened())
                strcpy_s(Config.LastProject, Data.ProjectFile);
            else
                Config.LastProject[0] = 0;

            if (QueryClose())
            {
                CloseChildWindows();
                DestroyWindow(HWindow);
            }
            return 0;
        }

        case CM_PROPERTIES:
        {
            if (!IsProjectOpened() | Data.MUIMode)
                return 0;
            CPropertiesDialog dlg(HWindow);
            if (dlg.Execute() == IDOK)
            {
            }
            return 0;
        }

        case CM_ABOUT:
        {
            CCommonDialog dlg(HInstance, IDD_ABOUT, HWindow);
            if (dlg.Execute() == IDOK)
            {
            }
            return 0;
        }

        case ID_HELP_SHOWREADME:
        {
            char readmePath[MAX_PATH];
            GetModuleFileName(NULL, readmePath, MAX_PATH);
            lstrcpy(strrchr(readmePath, '\\') + 1, "readme.txt");
            ShellExecute(HWindow, "open", readmePath, "", "", SW_SHOWNORMAL);
            return 0;
        }

        case CM_EDIT_FIND:
        {
            if (!IsProjectOpened())
                return 0;

            CFindDialog dlg(HWindow);
            if (dlg.Execute() == IDOK)
            {
                Data.Find();
            }
            return 0;
        }

        case ID_EDIT_FINDUNTRANSLATED:
        {
            if (!IsProjectOpened() || Data.MUIMode)
                return 0;

            Data.FindUntranslated();
            return 0;
        }

        case ID_EDIT_FINDRELAYOUT:
        {
            if (!IsProjectOpened() || Data.MUIMode)
                return 0;

            Data.FindDialogsForRelayout();
            return 0;
        }

        case ID_EDIT_FINDOUTER:
        {
            if (!IsProjectOpened())
                return 0;

            Data.FindDialogsWithOuterControls();
            return 0;
        }

        case CM_GOTO_NEXT:
        case CM_GOTO_PREV:
        {
            if (!IsProjectOpened() || OutWindow.GetSelectableLinesCount() < 1)
                return 0;

            OutWindow.Navigate(LOWORD(wParam) == CM_GOTO_NEXT);
            return 0;
        }

        case ID_TOOLS_RESTART:
        {
            BOOL exe = Data.SalamanderExeFile[0] != 0;
            if (!IsProjectOpened() || Data.MUIMode || !exe)
                return 0;
            RestartSalamander();
            return 0;
        }

        case ID_TOOLS_KILL:
        {
            BOOL exe = Data.SalamanderExeFile[0] != 0;
            if (!IsProjectOpened() || Data.MUIMode || !exe)
                return 0;
            KillSalamanders();
            return 0;
        }

        case CM_HOTKEY_CONFLICTS:
        {
            if (!IsProjectOpened() || Data.MUIMode)
                return 0;

            CValidateDialog dlg(HWindow);
            if (dlg.Execute() == IDOK)
                Data.ValidateTranslation(HWindow);
            return 0;
        }

        case ID_EDIT_TOGGLEBOOKMARK:
        {
            if (!IsProjectOpened())
                return 0;
            DWORD treeItem = TreeWindow.GetCurrentItem();
            WORD textItem = TextWindow.GetSelectIndex();
            Data.ToggleBookmark(treeItem, textItem);
            TextWindow.RefreshBookmarks();
            return 0;
        }

        case ID_EDIT_NEXTBOOKMARK:
        case ID_EDIT_PREVIOUSEBOOKMARK:
        {
            BOOL bookmarkExist = Data.GetBookmarksCount() > 0;
            if (!IsProjectOpened() || !bookmarkExist)
                return 0;
            BOOL next = LOWORD(wParam) == ID_EDIT_NEXTBOOKMARK;
            DWORD treeItem = TreeWindow.GetCurrentItem();
            WORD textItem = TextWindow.GetSelectIndex();
            const CBookmark* bookmark = Data.GotoBookmark(next, treeItem, textItem);
            if (bookmark != NULL)
            {
                HTREEITEM hItem = TreeWindow.GetItem(bookmark->TreeItem);
                if (hItem != NULL)
                {
                    TreeWindow.SelectItem(hItem);
                    TextWindow.SelectIndex(bookmark->TextItem);
                }
            }
            else
            {
                PostMessage(HWindow, WM_COMMAND, ID_EDIT_TRANSLATED, 0);
            }
            return 0;
        }

        case ID_EDIT_CLEARBOOKMARKS:
        {
            BOOL bookmarkExist = Data.GetBookmarksCount() > 0;
            if (!IsProjectOpened() || !bookmarkExist)
                return 0;
            Data.ClearBookmarks();
            TextWindow.RefreshBookmarks();
            return 0;
        }

        case ID_TOOLS_EDITLAYOUT:
        {
            if (!IsProjectOpened())
                return 0;

            OpenLayoutEditor();

            return 0;
        }

        case ID_TOOLS_MARKASTR:
        {
            if (!IsProjectOpened() || Data.MUIMode)
                return 0;

            if (MessageBox(HWindow,
                           "Do you really want to mark all changed texts in dialogs, menus, and "
                           "strings as Translated? Text is changed when Original text and Translated "
                           "text are different.",
                           FRAMEWINDOW_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
            {
                Data.MarkChangedTextsAsTranslated();
            }
            return 0;
        }

        case ID_TOOLS_RESETDLGSTOORIG:
        {
            if (!IsProjectOpened() || Data.MUIMode)
                return 0;

            if (MessageBox(HWindow,
                           "Do you really want to reset all dialogs layouts to original?",
                           FRAMEWINDOW_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
            {
                Data.ResetDialogsLayoutsToOriginal();
            }
            return 0;
        }

        case ID_TOOLS_CLEARDLGRELAYOUT:
        {
            if (!IsProjectOpened() || PreviewWindow.CurrentDialog == NULL)
                return 0;

            if (Data.RemoveRelayout(PreviewWindow.CurrentDialog->ID))
            {
                Data.SetDirty();
                Data.SLGSignature.SLTDataChanged();
            }
            return 0;
        }

        case ID_LAYOUT_FILE_EXIT:
        {
            if (CloseLayoutEditor())
            {
                if (OpenLayoutEditorDialogID[0] != 0)
                {
                    if (Data.IsDirty())
                    {
                        Data.Save();
                        Data.SaveProject();
                    }
                    PostMessage(HWindow, WM_COMMAND, CM_EXIT, 0); // bezpecny close-window
                }
            }
            return 0;
        }

        case CM_TOOLS_OPTIONS:
        {
            COptionsDialog dlg(HWindow);
            if (dlg.Execute() == IDOK)
            {
            }
            return 0;
        }

        case ID_PREVIEW_EXTENDDLG:
        {
            Config.PreviewExtendDialogs = !Config.PreviewExtendDialogs;
            PreviewWindow.RefreshDialog();
            return 0;
        }

        case ID_PREVIEW_OUTLINECTRLS:
        {
            Config.PreviewOutlineControls = !Config.PreviewOutlineControls;
            PreviewWindow.RefreshDialog();
            return 0;
        }

        case ID_PREVIEW_FILLCTRLS:
        {
            Config.PreviewFillControls = !Config.PreviewFillControls;
            PreviewWindow.RefreshDialog();
            return 0;
        }

        case CM_WND_NEXT:
        case CM_WND_PREVIOUS:
        {
            if (!IsProjectOpened())
                return 0;
            SendMessage(HMDIClient, WM_MDINEXT, NULL, (LPARAM)uMsg != CM_WND_NEXT);
            return 0;
        }

        case CM_WND_CASCADE:
        {
            if (!IsProjectOpened())
                return 0;
            SendMessage(HMDIClient, WM_MDICASCADE, NULL, NULL);
            return 0;
        }

        case CM_WND_TITLE:
        {
            if (!IsProjectOpened())
                return 0;
            SendMessage(HMDIClient, WM_MDITILE, 0, 0);
            return 0;
        }

        case CM_WND_TREE:
        case CM_WND_RH:
        case CM_WND_TEXT:
        case CM_WND_OUTPUT:
        case CM_WND_PREVIEW:
        {
            if (!IsProjectOpened())
                return 0;
            HWND hWnd = NULL;
            switch (LOWORD(wParam))
            {
            case CM_WND_TREE:
                hWnd = TreeWindow.HWindow;
                break;
            case CM_WND_RH:
                hWnd = RHWindow.HWindow;
                break;
            case CM_WND_TEXT:
                hWnd = TextWindow.HWindow;
                break;
            case CM_WND_OUTPUT:
                hWnd = OutWindow.HWindow;
                break;
            case CM_WND_PREVIEW:
                hWnd = PreviewWindow.HWindow;
                break;
            }

            if (hWnd != NULL)
                SendMessage(HMDIClient, WM_MDIACTIVATE, (WPARAM)hWnd, NULL);
            return 0;
        }
        }
        break;
    }
    }
    return DefFrameProc(HWindow, HMDIClient, uMsg, wParam, lParam);
}
