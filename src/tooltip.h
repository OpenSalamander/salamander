// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CToolTip
//
// Tento tooltip ma odstranit zakladni nevyhodu puvodni koncepce tooltipu.
// Kazde okno melo vytvoreny vlastni tooltip objekt. Druha nevyhoda byla,
// ze bylo nutne tomuto objektu predavat seznam oblasti, nad kteryma se
// maji tooltipy vybalit.
//
// Nova koncepce: CMainWindow bude vlastnit pouze jeden tooltip (instanci objektu).
// Okno tooltipu se vytvari az ve chvili kdy je potreba a to v threadu, ktery
// o zobrazeni pozadal. Duvod: potrebujeme, aby okno tooltipu v tomto threadu bezelo,
// do 2.6b6 vcetne bezelo okenko tooltipu v hlavnim threadu Salamandera a pokud
// ten stal, tooltipy se nezobrazovaly.
// Pri pohybu mysi nad controlem, ktery bude pouzivat tento tooltip, bude control
// pri vstupu do nove oblasti volat metodu SetCurrentID.
//
// Rozhrani pro praci s tooltipem bude v const.h, aby bylo dostupnem vsem
// controlum bez nutnosti includit mainwnd.h a tooltip.h.
//

// Pouzivane zpravy:
// WM_USER_TTGETTEXT - slouzi k dotazu na text s urcitym ID
//   wParam = ID predany pri SetCurrentToolTip
//   lParam = buffer (ukazuje do bufferu tooltipu) maxilmani pocet znaku je TOOLTIP_TEXT_MAX
//            pred volanim teto message je na nulty znak vlozen terminator
//            text muze obsahovat \n pro prechod na novy radek a \t pro vlozeni tabem
// pokud okno zapise do bufferu retezec terminovany nulou, bude zobrazen v tooltipu
// jinak nebude tooltip zobrazen
//

class CToolTip : public CWindow
{
    enum TipTimerModeEnum
    {
        ttmNone,         // nebezi zadny casovac
        ttmWaitingOpen,  // ceka se na otevreni tool tipu
        ttmWaitingClose, // ceka se na zavreni tool tipu
        ttmWaitingKill,  // ceka se na vystup z rezimu zobrazovani
    };

protected:
    char Text[TOOLTIP_TEXT_MAX];
    int TextLen;
    HWND HNotifyWindow;
    DWORD LastID;
    TipTimerModeEnum WaitingMode;
    DWORD HideCounter;
    DWORD HideCounterMax;
    POINT LastCursorPos;
    BOOL IsModal;     // je prave vykonvana nase message loop?
    BOOL ExitASAP;    // zavri se co nejdriv a prestan byt modalni
    UINT_PTR TimerID; // vracene ze SetTimer, potrebujeme pro KillTimer

public:
    CToolTip(CObjectOrigin origin = ooStatic);
    ~CToolTip();

    BOOL RegisterClass();

    // hParent je nezbytny, aby se pri jeho zavreni zavrel take tooltip
    // bez nej se nam delo, ze skoncil thread parenta, ale okno tooltipu zustalo
    // otevrene, ale uz neslo zavrit (neexistoval jeho thread) -> pady pri
    // ukonceni Salamandera (nastesti to bylo pred release 2.5b7)
    BOOL Create(HWND hParent);

    // Tato metoda spusti casovac a pokud do jeho vyprseni neni zavolana znovu
    // pozada okno 'hNotifyWindow' o text pomoci zpravy WM_USER_TTGETTEXT,
    // ktery pak zobrazi pod kurzor na jeho aktualnich souradnicich.
    // Promenna 'id' slouzi k rozliseni oblasti pri komunikaci s oknem 'hNotifyWindow'.
    // Pokud bude tato metoda zavolana vicekrat se stejnym parametrem 'id', budou
    // se tyto dalsi volani ignorovat.
    // Hodnota 0 parametru 'hNotifyWindow' je vyhrazena pro zhasnuti okna a preruseni
    // beziciho casovace.
    // parametr 'showDelay' ma vyznam pokud je 'hNotifyWindow' != NULL
    // pokud je vetsi nebo roven 1, urcuje za jak dlouho dojde ke zobrazeni tooltipu v [ms]
    // pokud je roven 0, pouzije se implicitni prodleva
    // pokud je -1, casovac se vubec nenastartuje
    void SetCurrentToolTip(HWND hNotifyWindow, DWORD id, int showDelay);

    // potlaci zobrazeni tooltipu na aktualnich souradnicich mysi
    // uzitecne volat pri aktivaci okna, ve kterem se tooltipy pouzivaji
    // nebude tak dochazet k nechtenemu zobrazeni tooltipu
    void SuppressToolTipOnCurrentMousePos();

    // pokud se podari text zobrazit, vrati TRUE; pokud neni dodan novy text, vrati FALSE
    // pokud je considerCursor==TRUE, omeri kurzor a posune tooltip pod nej
    // pokud je modal==TRUE, spusti messageloop, ktera hlida zpravy pro zavreni tooltipu a vrati se az po jeho zhasnuti
    BOOL Show(int x, int y, BOOL considerCursor, BOOL modal, HWND hParent);

    // zhasne tooltip
    void Hide();

    void OnTimer();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL GetText();
    void GetNeededWindowSize(SIZE* sz);

    void MessageLoop(); // pro modalni variantu tooltipu

    void MySetTimer(DWORD elapse);
    void MyKillTimer();

    DWORD GetTime(BOOL init);
};
