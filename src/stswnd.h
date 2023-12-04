// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//
// ****************************************************************************

class CMainToolBar;

enum CBorderLines
{
    blNone = 0x00,
    blTop = 0x01,
    blBottom = 0x02
};

enum CSecurityIconState
{
    sisNone = 0x00,      // ikona neni zobrazena
    sisUnsecured = 0x01, // zobrazena ikona odemceneho zamku
    sisSecured = 0x02    // zobrazena ikona zamceneho zamku
};

/*
enum
{
  otStatusWindow = otLastWinLibObject
};
*/

//
// CHotTrackItem
//
// polozka obsahuje index prvniho znaku, pocet znaku, offset prvniho znaku v bodech
// a jejich delku v bodech k zobrazene ceste se vytvori seznam techto polozek a drzi
// se v poli
//
// pro cestu "\\john\c\winnt
//
// se vytvori tyto polozky:
//
// (0, 9,  0, delka prvnich deviti znaku)   = \\john\c\
// (0, 14, 0, delka 14 znaku)              = \\john\c\winnt
//
// pro "DIR: 12"
//
// (0, 3, 0, delka tri znaku DIR)
// (5, 2, bodovy offset "12", delka dvou znaku "12")

struct CHotTrackItem
{
    WORD Offset;       // offset prvniho znaku ve znacich
    WORD Chars;        // pocet znaku
    WORD PixelsOffset; // offset prvniho znaku v bodech
    WORD Pixels;       // jejich delka v bodech
};

class CStatusWindow : public CWindow
{
public:
    CMainToolBar* ToolBar;
    CFilesWindow* FilesWindow;

protected:
    TDirectArray<CHotTrackItem> HotTrackItems;
    BOOL HotTrackItemsMeasured;

    int Border; // oddelovaci cara nahore/dole
    char* Text;
    int TextLen; // pocet znaku na ukazateli 'Text' bez terminatoru
    char* Size;
    int PathLen;          // -1 (cesta je cely Text), jinak delka cesty v Text (zbytek je filter)
    BOOL History;         // zobrazovat sipku mezi textem a size?
    BOOL Hidden;          // zobrazovat symbo filtru?
    int HiddenFilesCount; // kolik je odfiltrovanych souboru
    int HiddenDirsCount;  // a adresaru
    BOOL WholeTextVisible;

    BOOL ShowThrobber;             // TRUE pokud se ma zobrazovat 'progress' throbber za textem/hidden filtrem (nezalezi na existenci okna)
    BOOL DelayedThrobber;          // TRUE pokud uz bezi timer pro zobrazeni throbbera
    DWORD DelayedThrobberShowTime; // kolik bude GetTickCount() v okamziku, kdy se ma zobrazit zpozdeny throbber (0 = nezobrazujeme se zpozdenim)
    BOOL Throbber;                 // zobrazovat 'progress' throbber za textem/hidden filtrem? (TRUE jen pokud existuje okno)
    int ThrobberFrame;             // index aktualniho policka animace
    char* ThrobberTooltip;         // pokud je NULL, nebude zobrazen
    int ThrobberID;                // identifikacni cislo throbbera (-1 = neplatne)

    CSecurityIconState Security;
    char* SecurityTooltip; // pokud je NULL, nebude zobrazen

    int Allocated;
    int* AlpDX; // pole delek (od nulteho do Xteho znaku v retezci)
    BOOL Left;

    int ToolBarWidth; // aktualni sirka toolbary

    int EllipsedChars; // pocet vypustenych znaku za rootem; jinak -1
    int EllipsedWidth; // delka vypusteho retezce za rootem; jinak -1

    CHotTrackItem* HotItem;     // vysvicena polozka
    CHotTrackItem* LastHotItem; // posledni vysvicena poozka
    BOOL HotSize;               // vysvicena je polozka size
    BOOL HotHistory;            // vysvicena je polozka history
    BOOL HotZoom;               // vysvicena je polozka zoom
    BOOL HotHidden;             // vysviceny je symbol filtru
    BOOL HotSecurity;           // vysviceny je symbol zamku

    RECT TextRect;     // kam jsme vysmazili text
    RECT HiddenRect;   // kam jsme vysmazili symbol filtru
    RECT SizeRect;     // kam jsme vysmazili text size
    RECT HistoryRect;  // kam jsme vysmazili drop down pro history
    RECT ZoomRect;     // kam jsme vysmazili drop down pro history
    RECT ThrobberRect; // kam jsme vysmazili throbber
    RECT SecurityRect; // kam jsme vysmazili zamek
    int MaxTextRight;
    BOOL MouseCaptured;
    BOOL RButtonDown;
    BOOL LButtonDown;
    POINT LButtonDownPoint; // kde user stisknul LButton

    int Height;
    int Width; // rozmery

    BOOL NeedToInvalidate; // pro SetAutomatic() - nastala zmena, musime prekreslovat?

    DWORD* SubTexts;     // pole DWORDU: LOWORD pozice, HIWORD delka
    DWORD SubTextsCount; // pocet polozek v poli SubTexts

    IDropTarget* IDropTargetPtr;

public:
    CStatusWindow(CFilesWindow* filesWindow, int border, CObjectOrigin origin = ooAllocated);
    ~CStatusWindow();

    BOOL SetSubTexts(DWORD* subTexts, DWORD subTextsCount);
    // nastavuje text 'text' do status-line, 'pathLen' urcuje delku cesty (zbytek je filter),
    // pokud se 'pathLen' nepouziva (cesta je kompletni 'text') je rovno -1
    BOOL SetText(const char* text, int pathLen = -1);

    // sestavi pole HotTrackItems: pro disky a rchivatory na zaklade zpetnych lomitek
    // a pro FS se dopta pluginu
    void BuildHotTrackItems();

    void GetHotText(char* buffer, int bufSize);

    void DestroyWindow();

    int GetToolBarWidth() { return ToolBarWidth; }

    int GetNeededHeight();
    void SetSize(const CQuadWord& size);
    void SetHidden(int hiddenFiles, int hiddenDirs);
    void SetHistory(BOOL history);
    void SetThrobber(BOOL show, int delay = 0, BOOL calledFromDestroyWindow = FALSE); // volat pouze z hlavniho (GUI) threadu, stejne jako ostatni metody objektu
    // nastavi text, ktery se bude zobrazovat jako tooltip po najeti mysi na throbber, objekt si udela kopii
    // pokud je NULL, nebude tooltip zobrazen
    void SetThrobberTooltip(const char* throbberTooltip);
    int ChangeThrobberID(); // zmeni ThrobberID a vrati jeho novou hodnotu
    BOOL IsThrobberVisible(int throbberID) { return ShowThrobber && ThrobberID == throbberID; }
    void HideThrobberAndSecurityIcon();

    void SetSecurity(CSecurityIconState iconState);
    void SetSecurityTooltip(const char* tooltip);

    void InvalidateIfNeeded();

    void LayoutWindow();
    void Paint(HDC hdc, BOOL highlightText = FALSE, BOOL highlightHotTrackOnly = FALSE);
    void Repaint(BOOL flashText = FALSE, BOOL hotTrackOnly = FALSE);
    void InvalidateAndUpdate(BOOL update); // lze o volat i pro HWindow == NULL
    void FlashText(BOOL hotTrackOnly = FALSE);

    BOOL FindHotTrackItem(int xPos, int& index);

    void SetLeftPanel(BOOL left);
    BOOL ToggleToolBar();

    BOOL IsLeft() { return Left; }

    BOOL SetDriveIcon(HICON hIcon);     // ikona se okopiruje do imagelistu - destrukci musi zajistit volajici kod
    void SetDrivePressed(BOOL pressed); // zamackne drive ikonku

    BOOL GetTextFrameRect(RECT* r);   // vrati obdelnik kolem textu v souradnicich obrazovky
    BOOL GetFilterFrameRect(RECT* r); // vrati obdelnik kolem symbolu filtru v souradnicich obrazovky

    // mohlo dojit ke zmene barevne hloubky obrazovky; je treba prebuildit CacheBitmap
    void OnColorsChanged();

    void SetFont();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void RegisterDragDrop();
    void RevokeDragDrop();

    // vytvori imagelist s jednim prvkem, ktery bude pouzit pro zobrazovani prubehu tazeni
    // po ukonceni tazeni je treba tento imagelist uvolnit
    // vstupem je bod, ke kteremu se napocitaji offsety dxHotspot a dyHotspot
    HIMAGELIST CreateDragImage(const char* text, int& dxHotspot, int& dyHotspot, int& imgWidth, int& imgHeight);

    void PaintThrobber(HDC hDC);
    //    void RepaintThrobber();

    void PaintSecurity(HDC hDC);
};
