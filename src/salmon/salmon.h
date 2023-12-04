// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern HINSTANCE HLanguage;
extern char BugReportPath[MAX_PATH]; // cesta bude koncit zpetnym lomitkem
struct CBugReport
{
    char Name[MAX_PATH];
};
extern TDirectArray<CBugReport> BugReports;
extern BOOL ReportOldBugs;
extern char AppTitle[200];

extern CSalmonSharedMemory* SalmonSharedMemory;

char* LoadStr(int resID, HINSTANCE hInstance);
char* GetErrorText(DWORD error);

void OpenFolder(HWND hWnd, const char* szDir);

BOOL RestartSalamander(HWND hParent);

BOOL CleanBugReportsDirectory(BOOL keep7ZipArchives);

// vraci TRUE, pokud existuje bug report
// nastavuje globalku LatestBugReport na casove posledni nazev
BOOL GetBugReportNames();

int GetUniqueBugReportCount();

void GetReportBaseName(char* name, int nameSize, const char* targetPath, const char* shortName, DWORD64 uid, SYSTEMTIME lt);

BOOL SaveDescriptionAndEmail();

BOOL CompresBugReports();

extern BOOL AppIsBusy;

//#define WM_USER_THREAD_EXIT WM_APP + 100 // upload thread dobehnul