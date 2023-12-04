// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_gen) // aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

struct CFileData;
class CPluginDataInterfaceAbstract;

//
// ****************************************************************************
// CSalamanderGeneralAbstract
//
// obecne pouzitelne metody Salamandera (pro vsechny typy pluginu)

// typy message-boxu
#define MSGBOX_INFO 0
#define MSGBOX_ERROR 1
#define MSGBOX_EX_ERROR 2
#define MSGBOX_QUESTION 3
#define MSGBOX_EX_QUESTION 4
#define MSGBOX_WARNING 5
#define MSGBOX_EX_WARNING 6

// konstanty pro CSalamanderGeneralAbstract::SalMessageBoxEx
#define MSGBOXEX_OK 0x00000000                // MB_OK
#define MSGBOXEX_OKCANCEL 0x00000001          // MB_OKCANCEL
#define MSGBOXEX_ABORTRETRYIGNORE 0x00000002  // MB_ABORTRETRYIGNORE
#define MSGBOXEX_YESNOCANCEL 0x00000003       // MB_YESNOCANCEL
#define MSGBOXEX_YESNO 0x00000004             // MB_YESNO
#define MSGBOXEX_RETRYCANCEL 0x00000005       // MB_RETRYCANCEL
#define MSGBOXEX_CANCELTRYCONTINUE 0x00000006 // MB_CANCELTRYCONTINUE
#define MSGBOXEX_CONTINUEABORT 0x00000007     // MB_CONTINUEABORT
#define MSGBOXEX_YESNOOKCANCEL 0x00000008

#define MSGBOXEX_ICONHAND 0x00000010        // MB_ICONHAND / MB_ICONSTOP / MB_ICONERROR
#define MSGBOXEX_ICONQUESTION 0x00000020    // MB_ICONQUESTION
#define MSGBOXEX_ICONEXCLAMATION 0x00000030 // MB_ICONEXCLAMATION / MB_ICONWARNING
#define MSGBOXEX_ICONINFORMATION 0x00000040 // MB_ICONASTERISK / MB_ICONINFORMATION

#define MSGBOXEX_DEFBUTTON1 0x00000000 // MB_DEFBUTTON1
#define MSGBOXEX_DEFBUTTON2 0x00000100 // MB_DEFBUTTON2
#define MSGBOXEX_DEFBUTTON3 0x00000200 // MB_DEFBUTTON3
#define MSGBOXEX_DEFBUTTON4 0x00000300 // MB_DEFBUTTON4

#define MSGBOXEX_HELP 0x00004000 // MB_HELP (bit mask)

#define MSGBOXEX_SETFOREGROUND 0x00010000 // MB_SETFOREGROUND (bit mask)

// altap specific
#define MSGBOXEX_SILENT 0x10000000 // messagebox nevyda pri otevreni zadny zvuk (bit mask)
// v pripade MB_YESNO messageboxu povoli Escape (generuje IDNO); v MB_ABORTRETRYIGNORE messageboxu
// povoli Escape (generuje IDCANCEL) (bit mask)
#define MSGBOXEX_ESCAPEENABLED 0x20000000
#define MSGBOXEX_HINT 0x40000000 // pokud se pouziva CheckBoxText, bude v nem vyhledan oddelovac \t a zobrazen jako hint
// Vista: defaultni tlacitko bude mit stav "pozaduje elevaci" (zobrazi se elevated icon)
#define MSGBOXEX_SHIELDONDEFBTN 0x80000000

#define MSGBOXEX_TYPEMASK 0x0000000F // MB_TYPEMASK
#define MSGBOXEX_ICONMASK 0x000000F0 // MB_ICONMASK
#define MSGBOXEX_DEFMASK 0x00000F00  // MB_DEFMASK
#define MSGBOXEX_MODEMASK 0x00003000 // MB_MODEMASK
#define MSGBOXEX_MISCMASK 0x0000C000 // MB_MISCMASK
#define MSGBOXEX_EXMASK 0xF0000000

// navratove hodnoty message boxu
#define DIALOG_FAIL 0x00000000 // dialog se nepodarilo otevrit
// jednotliva tlacitka
#define DIALOG_OK 0x00000001       // IDOK
#define DIALOG_CANCEL 0x00000002   // IDCANCEL
#define DIALOG_ABORT 0x00000003    // IDABORT
#define DIALOG_RETRY 0x00000004    // IDRETRY
#define DIALOG_IGNORE 0x00000005   // IDIGNORE
#define DIALOG_YES 0x00000006      // IDYES
#define DIALOG_NO 0x00000007       // IDNO
#define DIALOG_TRYAGAIN 0x0000000a // IDTRYAGAIN
#define DIALOG_CONTINUE 0x0000000b // IDCONTINUE
// altap specific
#define DIALOG_SKIP 0x10000000
#define DIALOG_SKIPALL 0x20000000
#define DIALOG_ALL 0x30000000

typedef void(CALLBACK* MSGBOXEX_CALLBACK)(LPHELPINFO helpInfo);

struct MSGBOXEX_PARAMS
{
    HWND HParent;
    const char* Text;
    const char* Caption;
    DWORD Flags;
    HICON HIcon;
    DWORD ContextHelpId;
    MSGBOXEX_CALLBACK HelpCallback;
    const char* CheckBoxText;
    BOOL* CheckBoxValue;
    const char* AliasBtnNames;
    const char* URL;
    const char* URLText;
};

/*
HParent
  Handle to the owner window. Message box is centered to this window.
  If this parameter is NULL, the message box has no owner window.

Text
  Pointer to a null-terminated string that contains the message to be displayed.

Caption
  Pointer to a null-terminated string that contains the message box title.
  If this member is NULL, the default title "Error" is used.

Flags
  Specifies the contents and behavior of the message box.
  This parameter can be a combination of flags from the following groups of flags.

   To indicate the buttons displayed in the message box, specify one of the following values.
    MSGBOXEX_OK                   (MB_OK)
      The message box contains one push button: OK. This is the default.
      Message box can be closed using Escape and return value will be DIALOG_OK (IDOK).
    MSGBOXEX_OKCANCEL             (MB_OKCANCEL)
      The message box contains two push buttons: OK and Cancel.
    MSGBOXEX_ABORTRETRYIGNORE     (MB_ABORTRETRYIGNORE)
      The message box contains three push buttons: Abort, Retry, and Ignore.
      Message box can be closed using Escape when MSGBOXEX_ESCAPEENABLED flag is specified.
      In that case return value will be DIALOG_CANCEL (IDCANCEL).
    MSGBOXEX_YESNOCANCEL          (MB_YESNOCANCEL)
      The message box contains three push buttons: Yes, No, and Cancel.
    MSGBOXEX_YESNO                (MB_YESNO)
      The message box contains two push buttons: Yes and No.
      Message box can be closed using Escape when MSGBOXEX_ESCAPEENABLED flag is specified.
      In that case return value will be DIALOG_NO (IDNO).
    MSGBOXEX_RETRYCANCEL          (MB_RETRYCANCEL)
      The message box contains two push buttons: Retry and Cancel.
    MSGBOXEX_CANCELTRYCONTINUE    (MB_CANCELTRYCONTINUE)
      The message box contains three push buttons: Cancel, Try Again, Continue.

   To display an icon in the message box, specify one of the following values.
    MSGBOXEX_ICONHAND             (MB_ICONHAND / MB_ICONSTOP / MB_ICONERROR)
      A stop-sign icon appears in the message box.
    MSGBOXEX_ICONQUESTION         (MB_ICONQUESTION)
      A question-mark icon appears in the message box.
    MSGBOXEX_ICONEXCLAMATION      (MB_ICONEXCLAMATION / MB_ICONWARNING)
      An exclamation-point icon appears in the message box.
    MSGBOXEX_ICONINFORMATION      (MB_ICONASTERISK / MB_ICONINFORMATION)
      An icon consisting of a lowercase letter i in a circle appears in the message box.

   To indicate the default button, specify one of the following values.
    MSGBOXEX_DEFBUTTON1           (MB_DEFBUTTON1)
      The first button is the default button.
      MSGBOXEX_DEFBUTTON1 is the default unless MSGBOXEX_DEFBUTTON2, MSGBOXEX_DEFBUTTON3,
      or MSGBOXEX_DEFBUTTON4 is specified.
    MSGBOXEX_DEFBUTTON2           (MB_DEFBUTTON2)
      The second button is the default button.
    MSGBOXEX_DEFBUTTON3           (MB_DEFBUTTON3)
      The third button is the default button.
    MSGBOXEX_DEFBUTTON4           (MB_DEFBUTTON4)
      The fourth button is the default button.

   To specify other options, use one or more of the following values.
    MSGBOXEX_HELP                 (MB_HELP)
      Adds a Help button to the message box.
      When the user clicks the Help button or presses F1, the system sends a WM_HELP message to the owner
      or calls HelpCallback (see HelpCallback for details).
    MSGBOXEX_SETFOREGROUND        (MB_SETFOREGROUND)
      The message box becomes the foreground window. Internally, the system calls the SetForegroundWindow
      function for the message box.
    MSGBOXEX_SILENT
      No sound will be played when message box is displayed.
    MSGBOXEX_ESCAPEENABLED
      When MSGBOXEX_YESNO is specified, user can close message box using Escape key and DIALOG_NO (IDNO)
      will be returned. When MSGBOXEX_ABORTRETRYIGNORE is specified, user can close message box using
      Escape key and DIALOG_CANCEL (IDCANCEL) will be returned. Otherwise this option is ignored.

HIcon
  Handle to the icon to be drawn in the message box. Icon will not be destroyed when messagebox is closed.
  If this parameter is NULL, MSGBOXEX_ICONxxx style will be used.

ContextHelpId
  Identifies a help context. If a help event occurs, this value is specified in
  the HELPINFO structure that the message box sends to the owner window or callback function.

HelpCallback
  Pointer to the callback function that processes help events for the message box.
  The callback function has the following form:
    VOID CALLBACK MSGBOXEX_CALLBACK(LPHELPINFO helpInfo)
  If this member is NULL, the message box sends WM_HELP messages to the owner window
  when help events occur.

CheckBoxText
  Pointer to a null-terminated string that contains the checkbox text.
  If the MSGBOXEX_HINT flag is specified in the Flags, this text must contain HINT.
  Hint is separated from string by the TAB character. Hint is divided by the second TAB character
  on two parts. The first part is label, that will be displayed behind the check box.
  The second part is the text displayed when user clicks the hint label.

  Example: "This is text for checkbox\tHint Label\tThis text will be displayed when user click the Hint Label."
  If this member is NULL, checkbox will not be displayed.

CheckBoxValue
  Pointer to a BOOL variable contains the checkbox initial and return state (TRUE: checked, FALSE: unchecked).
  This parameter is ignored if CheckBoxText parameter is NULL. Otherwise this parameter must be set.

AliasBtnNames
  Pointer to a buffer containing pairs of id and alias strings. The last string in the
  buffer must be terminated by NULL character.

  The first string in each pair is a decimal number that specifies button ID.
  Number must be one of the DIALOG_xxx values. The second string specifies alias text
  for this button.

  First and second string in each pair are separated by TAB character.
  Pairs are separated by TAB character too.

  If this member is NULL, normal names of buttons will displayed.

  Example: sprintf(buffer, "%d\t%s\t%d\t%s", DIALOG_OK, "&Start", DIALOG_CANCEL, "E&xit");
           buffer: "1\t&Start\t2\tE&xit"

URL
  Pointer to a null-terminated string that contains the URL displayed below text.
  If this member is NULL, the URL is not displayed.

URLText
  Pointer to a null-terminated string that contains the URL text displayed below text.
  If this member is NULL, the URL is displayed instead.

*/

// identifikace panelu
#define PANEL_SOURCE 1 // zdrojovy panel (aktivni panel)
#define PANEL_TARGET 2 // cilovy panel (neaktivni panel)
#define PANEL_LEFT 3   // levy panel
#define PANEL_RIGHT 4  // pravy panel

// typy cest
#define PATH_TYPE_WINDOWS 1 // windowsova cesta ("c:\path" nebo UNC cesta)
#define PATH_TYPE_ARCHIVE 2 // cesta do archivu (archiv lezi na windowsove ceste)
#define PATH_TYPE_FS 3      // cesta na pluginovy file-system

// Z nasledujici skupiny flagu lze vybrat pouze jeden.
// Definuji sadu zobrazenych tlacitek v ruznych chybovych hlasenich.
#define BUTTONS_OK 0x00000000               // OK
#define BUTTONS_RETRYCANCEL 0x00000001      // Retry / Cancel
#define BUTTONS_SKIPCANCEL 0x00000002       // Skip / Skip all / Cancel
#define BUTTONS_RETRYSKIPCANCEL 0x00000003  // Retry / Skip / Skip all / Cancel
#define BUTTONS_YESALLSKIPCANCEL 0x00000004 // Yes / All / Skip / Skip all / Cancel
#define BUTTONS_YESNOCANCEL 0x00000005      // Yes / No / Cancel
#define BUTTONS_YESALLCANCEL 0x00000006     // Yes / All / Cancel
#define BUTTONS_MASK 0x000000FF             // interni maska, nepouzivat
// detekci zda kombinace ma tlacitko SKIP nebo YES nechavam zde ve forme inline, aby
// v pripade zavadeni novych kombinaci byla dobre na ocich a nezapomeli jsme ji doplnit
inline BOOL ButtonsContainsSkip(DWORD btn)
{
    return (btn & BUTTONS_MASK) == BUTTONS_SKIPCANCEL ||
           (btn & BUTTONS_MASK) == BUTTONS_RETRYSKIPCANCEL ||
           (btn & BUTTONS_MASK) == BUTTONS_YESALLSKIPCANCEL;
}
inline BOOL ButtonsContainsYes(DWORD btn)
{
    return (btn & BUTTONS_MASK) == BUTTONS_YESALLSKIPCANCEL ||
           (btn & BUTTONS_MASK) == BUTTONS_YESNOCANCEL ||
           (btn & BUTTONS_MASK) == BUTTONS_YESALLCANCEL;
}

// chybove konstanty pro CSalamanderGeneralAbstract::SalGetFullName
#define GFN_SERVERNAMEMISSING 1   // v UNC ceste chybi server name
#define GFN_SHARENAMEMISSING 2    // v UNC ceste chybi share name
#define GFN_TOOLONGPATH 3         // operaci by vznikla prilis dlouha cesta
#define GFN_INVALIDDRIVE 4        // u normalni cesty (c:\) neni pismenko A-Z (ani a-z)
#define GFN_INCOMLETEFILENAME 5   // relativni cesta bez zadaneho 'curDir' -> neresitelne
#define GFN_EMPTYNAMENOTALLOWED 6 // prazdny retezec 'name'
#define GFN_PATHISINVALID 7       // nelze vyloucit "..", napr. "c:\.."

// chybovy kod pro stav, kdy uzivatel prerusi CSalamanderGeneralAbstract::SalCheckPath klavesou ESC
#define ERROR_USER_TERMINATED -100

#define PATH_MAX_PATH 248 // limit pro max. delku cesty (plne jmeno adresare), pozor: v limitu uz je zapocteny null-terminator (max. delka retezce je 247 znaku)

// chybove konstanty pro CSalamanderGeneralAbstract::SalParsePath:
// vstupem byla prazdna cesta a 'curPath' bylo NULL (prazdna cesta se nahrazuje aktualni cestou,
// ale ta tu neni znama)
#define SPP_EMPTYPATHNOTALLOWED 1
// windowsova cesta (normal + UNC) neexistuje, neni pristupna nebo uzivatel prerusil test
// na pristupnost cesty (soucasti je i pokus o obnoveni sit. spojeni)
#define SPP_WINDOWSPATHERROR 2
// windowsova cesta zacina jmenem souboru, ktery ale neni archiv (jinak by slo o cestu do archivu)
#define SPP_NOTARCHIVEFILE 3
// FS cesta - jmeno pluginoveho FS (fs-name - pred ':' v ceste) neni zname (zadnemu pluginu
// nebylo toto jmeno zaregistrovano)
#define SPP_NOTPLUGINFS 4
// jde o relativni cestu, ale aktualni cesta neni znama nebo jde o FS (tam nelze poznat root
// a vubec nezname strukturu fs-user-part cesty, takze nelze provest prevod na absolutni cestu)
// je-li aktualni cesta FS ('curPathIsDiskOrArchive' je FALSE), nedojde v tomto pripade ke hlaseni
// chyby uzivateli (predpoklada se dalsi zpracovani na strane FS, ktere metodu SalParsePath volalo)
#define SPP_INCOMLETEPATH 5

// konstanty vnitrnich barev Salamandera
#define SALCOL_FOCUS_ACTIVE_NORMAL 0 // barvy pera pro ramecek kolem polozky
#define SALCOL_FOCUS_ACTIVE_SELECTED 1
#define SALCOL_FOCUS_FG_INACTIVE_NORMAL 2
#define SALCOL_FOCUS_FG_INACTIVE_SELECTED 3
#define SALCOL_FOCUS_BK_INACTIVE_NORMAL 4
#define SALCOL_FOCUS_BK_INACTIVE_SELECTED 5
#define SALCOL_ITEM_FG_NORMAL 6 // barvy textu polozek v panelu
#define SALCOL_ITEM_FG_SELECTED 7
#define SALCOL_ITEM_FG_FOCUSED 8
#define SALCOL_ITEM_FG_FOCSEL 9
#define SALCOL_ITEM_FG_HIGHLIGHT 10
#define SALCOL_ITEM_BK_NORMAL 11 // barvy pozadi polozek v panelu
#define SALCOL_ITEM_BK_SELECTED 12
#define SALCOL_ITEM_BK_FOCUSED 13
#define SALCOL_ITEM_BK_FOCSEL 14
#define SALCOL_ITEM_BK_HIGHLIGHT 15
#define SALCOL_ICON_BLEND_SELECTED 16 // barvy pro blend ikonek
#define SALCOL_ICON_BLEND_FOCUSED 17
#define SALCOL_ICON_BLEND_FOCSEL 18
#define SALCOL_PROGRESS_FG_NORMAL 19 // barvy progress bary
#define SALCOL_PROGRESS_FG_SELECTED 20
#define SALCOL_PROGRESS_BK_NORMAL 21
#define SALCOL_PROGRESS_BK_SELECTED 22
#define SALCOL_HOT_PANEL 23           // barva hot polozky v panelu
#define SALCOL_HOT_ACTIVE 24          //                   v aktivnim window caption
#define SALCOL_HOT_INACTIVE 25        //                   v neaktivni caption, statusbar,...
#define SALCOL_ACTIVE_CAPTION_FG 26   // barva textu v aktivnim titulku panelu
#define SALCOL_ACTIVE_CAPTION_BK 27   // barva pozadi v aktivnim titulku panelu
#define SALCOL_INACTIVE_CAPTION_FG 28 // barva textu v neaktivnim titulku panelu
#define SALCOL_INACTIVE_CAPTION_BK 29 // barva pozadi v neaktivnim titulku panelu
#define SALCOL_VIEWER_FG_NORMAL 30    // barva textu v internim text/hex vieweru
#define SALCOL_VIEWER_BK_NORMAL 31    // barva pozadi v internim text/hex vieweru
#define SALCOL_VIEWER_FG_SELECTED 32  // barva oznaceneho textu v internim text/hex vieweru
#define SALCOL_VIEWER_BK_SELECTED 33  // barva oznaceneho pozadi v internim text/hex vieweru
#define SALCOL_THUMBNAIL_NORMAL 34    // barvy pera pro ramecek kolem thumbnail
#define SALCOL_THUMBNAIL_SELECTED 35
#define SALCOL_THUMBNAIL_FOCUSED 36
#define SALCOL_THUMBNAIL_FOCSEL 37

// konstanty duvodu, proc metody CSalamanderGeneralAbstract::ChangePanelPathToXXX vratily neuspech:
#define CHPPFR_SUCCESS 0 // v panelu je nova cesta, uspech (navratova hodnota je TRUE)
// novou cestu (nebo jmeno archivu) nelze prevest z relativni na absolutni nebo
// nova cesta (nebo jmeno archivu) neni pristupna nebo
// cestu na FS nelze otevrit (neni plugin, odmita svuj load, odmita otevreni FS, fatalni chyba ChangePath)
#define CHPPFR_INVALIDPATH 1
#define CHPPFR_INVALIDARCHIVE 2  // soubor neni archiv nebo se jako archiv neda vylistovat
#define CHPPFR_CANNOTCLOSEPATH 4 // aktualni cestu nelze uzavrit
// v panelu je zkracena nova cesta,
// upresneni pro FS: v panelu je bud zkracena nova cesta nebo puvodni cesta nebo zkracena
// puvodni cesta - puvodni cesta se do panelu zkousi vratit jen pokud se nova cesta otevirala
// v aktualnim FS (metoda IsOurPath pro ni vratila TRUE) a pokud nova cesta neni pristupna
// (ani zadna jeji podcesta)
#define CHPPFR_SHORTERPATH 5
// v panelu je zkracena nova cesta; duvodem zkraceni bylo to, ze pozadovana cesta byla jmeno
// souboru - v panelu je cesta k souboru a soubor bude vyfokusen
#define CHPPFR_FILENAMEFOCUSED 6

// typy pro CSalamanderGeneralAbstract::ValidateVarString() a CSalamanderGeneralAbstract::ExpandVarString()
typedef const char*(WINAPI* FSalamanderVarStrGetValue)(HWND msgParent, void* param);
struct CSalamanderVarStrEntry
{
    const char* Name;                  // jmeno promenne v retezci (napr. u retezce "$(name)" je to "name")
    FSalamanderVarStrGetValue Execute; // funkce, ktera vraci text reprezentujici promennou
};

class CSalamanderRegistryAbstract;

// typ call-backu pouzivany pri load/save konfigurace pomoci
// CSalamanderGeneral::CallLoadOrSaveConfiguration; 'regKey' je NULL pokud jde o load
// defaultni konfigurace (save se pri 'regKey' == NULL nevola); 'registry' je objekt pro
// praci s registry; 'param' je uzivatelsky parametr funkce (viz
// CSalamanderGeneral::CallLoadOrSaveConfiguration)
typedef void(WINAPI* FSalLoadOrSaveConfiguration)(BOOL load, HKEY regKey,
                                                  CSalamanderRegistryAbstract* registry, void* param);

// zaklad struktury pro CSalamanderGeneralAbstract::ViewFileInPluginViewer (kazdy plugin
// viewer muze mit tuto strukturu rozsirenou o sve parametry - struktura se predava do
// CPluginInterfaceForViewerAbstract::ViewFile - parametry muzou byt napr. titulek okna,
// mod vieweru, offset od zacatku souboru, pozice oznaceni, atp.); POZOR !!! na pakovani
// struktur (pozadovane je 4 byty - viz "#pragma pack(4)")
struct CSalamanderPluginViewerData
{
    // kolik bytu od zacatku struktury je platnych (pro rozliseni verzi struktury)
    int Size;
    // jmeno souboru, ktery se ma otevrit ve viewru (nepouzivat v metode
    // CPluginInterfaceForViewerAbstract::ViewFile - jmeno souboru je dano parametrem 'name')
    const char* FileName;
};

// rozsireni struktury CSalamanderPluginViewerData pro interni text/hex viewer
struct CSalamanderPluginInternalViewerData : public CSalamanderPluginViewerData
{
    int Mode;            // 0 - textovy mod, 1 - hexa mod
    const char* Caption; // NULL -> obsahuje caption okna FileName, jinak Caption
    BOOL WholeCaption;   // ma vyznam pokud je Caption != NULL. TRUE -> v titulku
                         // vieweru bude zobrazen pouze retezec Caption; FALSE -> za
                         // Caption se pripoji standardni " - Viewer".
};

// konstanty typu parametru konfigurace Salamandera (viz CSalamanderGeneralAbstract::GetConfigParameter)
#define SALCFGTYPE_NOTFOUND 0 // parameter not found
#define SALCFGTYPE_BOOL 1     // TRUE/FALSE
#define SALCFGTYPE_INT 2      // 32-bit integer
#define SALCFGTYPE_STRING 3   // null-terminated multibyte string
#define SALCFGTYPE_LOGFONT 4  // Win32 LOGFONT structure

// konstanty parametru konfigurace Salamandera (viz CSalamanderGeneralAbstract::GetConfigParameter);
// v komentari je uveden typ parametru (BOOL, INT, STRING), za STRING je v zavorce potrebna
// velikost bufferu pro retezec
//
// general parameters
#define SALCFG_SELOPINCLUDEDIRS 1        // BOOL, select/deselect operations (num *, num +, num -) work also with directories
#define SALCFG_SAVEONEXIT 2              // BOOL, save configuration on Salamander exit
#define SALCFG_MINBEEPWHENDONE 3         // BOOL, should it beep (play sound) after end of work in inactive window?
#define SALCFG_HIDEHIDDENORSYSTEMFILES 4 // BOOL, should it hide system and/or hidden files?
#define SALCFG_ALWAYSONTOP 6             // BOOL, main window is Always On Top?
#define SALCFG_SORTUSESLOCALE 7          // BOOL, should it use regional settings when sorting?
#define SALCFG_SINGLECLICK 8             // BOOL, single click mode (single click to open file, etc.)
#define SALCFG_TOPTOOLBARVISIBLE 9       // BOOL, is top toolbar visible?
#define SALCFG_BOTTOMTOOLBARVISIBLE 10   // BOOL, is bottom toolbar visible?
#define SALCFG_USERMENUTOOLBARVISIBLE 11 // BOOL, is user-menu toolbar visible?
#define SALCFG_INFOLINECONTENT 12        // STRING (200), content of Information Line (string with parameters)
#define SALCFG_FILENAMEFORMAT 13         // INT, how to alter file name before displaying (parameter 'format' to CSalamanderGeneralAbstract::AlterFileName)
#define SALCFG_SAVEHISTORY 14            // BOOL, may history related data be stored to configuration?
#define SALCFG_ENABLECMDLINEHISTORY 15   // BOOL, is command line history enabled?
#define SALCFG_SAVECMDLINEHISTORY 16     // BOOL, may command line history be stored to configuration?
#define SALCFG_MIDDLETOOLBARVISIBLE 17   // BOOL, is middle toolbar visible?
#define SALCFG_SORTDETECTNUMBERS 18      // BOOL, should it use numerical sort for numbers contained in strings when sorting?
#define SALCFG_SORTBYEXTDIRSASFILES 19   // BOOL, should it treat dirs as files when sorting by extension? BTW, if TRUE, directories extensions are also displayed in separated Ext column. (directories have no extensions, only files have extensions, but many people have requested sort by extension and displaying extension in separated Ext column even for directories)
#define SALCFG_SIZEFORMAT 20             // INT, units for custom size columns, 0 - Bytes, 1 - KB, 2 - short (mixed B, KB, MB, GB, ...)
#define SALCFG_SELECTWHOLENAME 21        // BOOL, should be whole name selected (including extension) when entering new filename? (for dialog boxes F2:QuickRename, Alt+F5:Pack, etc)
// recycle bin parameters
#define SALCFG_USERECYCLEBIN 50   // INT, 0 - do not use, 1 - use for all, 2 - use for files matching at \
                                  //      least one of masks (see SALCFG_RECYCLEBINMASKS)
#define SALCFG_RECYCLEBINMASKS 51 // STRING (MAX_PATH), masks for SALCFG_USERECYCLEBIN==2
// time resolution of file compare (used in command Compare Directories)
#define SALCFG_COMPDIRSUSETIMERES 60 // BOOL, should it use time resolution? (FALSE==exact match)
#define SALCFG_COMPDIRTIMERES 61     // INT, time resolution for file compare (from 0 to 3600 second)
// confirmations
#define SALCFG_CNFRMFILEDIRDEL 70 // BOOL, files or directories delete
#define SALCFG_CNFRMNEDIRDEL 71   // BOOL, non-empty directory delete
#define SALCFG_CNFRMFILEOVER 72   // BOOL, file overwrite
#define SALCFG_CNFRMSHFILEDEL 73  // BOOL, system or hidden file delete
#define SALCFG_CNFRMSHDIRDEL 74   // BOOL, system or hidden directory delete
#define SALCFG_CNFRMSHFILEOVER 75 // BOOL, system or hidden file overwrite
#define SALCFG_CNFRMCREATEPATH 76 // BOOL, show "do you want to create target path?" in Copy/Move operations
#define SALCFG_CNFRMDIROVER 77    // BOOL, directory overwrite (copy/move selected directory: ask user if directory already exists on target path - standard behaviour is to join contents of both directories)
// drive specific settings
#define SALCFG_DRVSPECFLOPPYMON 88         // BOOL, floppy disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECFLOPPYSIM 89         // BOOL, floppy disks - use simple icons
#define SALCFG_DRVSPECREMOVABLEMON 90      // BOOL, removable disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECREMOVABLESIM 91      // BOOL, removable disks - use simple icons
#define SALCFG_DRVSPECFIXEDMON 92          // BOOL, fixed disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECFIXEDSIMPLE 93       // BOOL, fixed disks - use simple icons
#define SALCFG_DRVSPECREMOTEMON 94         // BOOL, remote (network) disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECREMOTESIMPLE 95      // BOOL, remote (network) disks - use simple icons
#define SALCFG_DRVSPECREMOTEDONOTREF 96    // BOOL, remote (network) disks - do not refresh on activation of Salamander
#define SALCFG_DRVSPECCDROMMON 97          // BOOL, CDROM disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECCDROMSIMPLE 98       // BOOL, CDROM disks - use simple icons
#define SALCFG_IFPATHISINACCESSIBLEGOTO 99 // STRING (MAX_PATH), path where to go if path in panel is inaccessible
// internal text/hex viewer
#define SALCFG_VIEWEREOLCRLF 120          // BOOL, accept CR-LF ("\r\n") line ends?
#define SALCFG_VIEWEREOLCR 121            // BOOL, accept CR ("\r") line ends?
#define SALCFG_VIEWEREOLLF 122            // BOOL, accept LF ("\n") line ends?
#define SALCFG_VIEWEREOLNULL 123          // BOOL, accept NULL ("\0") line ends?
#define SALCFG_VIEWERTABSIZE 124          // INT, size of tab ("\t") character in spaces
#define SALCFG_VIEWERSAVEPOSITION 125     // BOOL, TRUE = save position of viewer window, FALSE = always use position of main window
#define SALCFG_VIEWERFONT 126             // LOGFONT, viewer font
#define SALCFG_VIEWERWRAPTEXT 127         // BOOL, wrap text (divide long text line to more lines)
#define SALCFG_AUTOCOPYSELTOCLIPBOARD 128 // BOOL, TRUE = when user selects some text, this text is instantly copied to the cliboard
// archivers
#define SALCFG_ARCOTHERPANELFORPACK 140    // BOOL, should it pack to other panel path?
#define SALCFG_ARCOTHERPANELFORUNPACK 141  // BOOL, should it unpack to other panel path?
#define SALCFG_ARCSUBDIRBYARCFORUNPACK 142 // BOOL, should it unpack to subdirectory named by archive?
#define SALCFG_ARCUSESIMPLEICONS 143       // BOOL, should it use simple icons in archives?

// typ callbacku pouzivany v metode CSalamanderGeneral::SalSplitGeneralPath
typedef BOOL(WINAPI* SGP_IsTheSamePathF)(const char* path1, const char* path2);

// typ callbacku pouzivany v metode CSalamanderGeneralAbstract::CallPluginOperationFromDisk
// 'sourcePath' je zdrojova cesta na disku (ostatni cesty jsou od ni vztazeny relativne);
// oznacene soubory/adresare jsou zadany enumeracni funkci 'next' jejimz parametrem je
// 'nextParam'; 'param' je parametr predavany do CallPluginOperationFromDisk jako 'param'
typedef void(WINAPI* SalPluginOperationFromDisk)(const char* sourcePath, SalEnumSelection2 next,
                                                 void* nextParam, void* param);

// flagy pro textove vyhledavaci algoritmy (CSalamanderBMSearchData a CSalamanderREGEXPSearchData);
// flagy se daji logicky scitat
#define SASF_CASESENSITIVE 0x01 // velikost pismen je dulezita (pokud neni nastaven, hleda se bez ohledu na vel. pismen)
#define SASF_FORWARD 0x02       // hledani smerem dopredu (pokud neni nastaven, hleda se smerem zpet)

// ikony pro GetSalamanderIcon
#define SALICON_EXECUTABLE 1    // exe/bat/pif/com
#define SALICON_DIRECTORY 2     // dir
#define SALICON_NONASSOCIATED 3 // neasociovany soubor
#define SALICON_ASSOCIATED 4    // asociovany soubor
#define SALICON_UPDIR 5         // up-dir ".."
#define SALICON_ARCHIVE 6       // archiv

// velikosti ikon pro GetSalamanderIcon
#define SALICONSIZE_16 1 // 16x16
#define SALICONSIZE_32 2 // 32x32
#define SALICONSIZE_48 3 // 48x48

// interface objektu Boyer-Moorova algoritmu pro vyhledavani v textu
// POZOR: kazdy alokovany objekt je mozne pouzivat jen v ramci jednoho threadu
// (nemusi jit o hlavni thread, nemusi jit u vsech objektu o jeden thread)
class CSalamanderBMSearchData
{
public:
    // nastaveni vzorku; 'pattern' je null-terminated text vzorku; 'flags' jsou priznaky
    // algoritmu (viz konstanty SASF_XXX)
    virtual void WINAPI Set(const char* pattern, WORD flags) = 0;

    // nastaveni vzorku; 'pattern' je binarni vzorek o delce 'length' (buffer 'pattern' musi
    // mit delku alespon ('length' + 1) znaku - jen pro kompatibilitu s textovymi vzorky);
    // 'flags' jsou priznaky algoritmu (viz konstanty SASF_XXX)
    virtual void WINAPI Set(const char* pattern, const int length, WORD flags) = 0;

    // nastaveni priznaku algoritmu; 'flags' jsou priznaky algoritmu (viz konstanty SASF_XXX)
    virtual void WINAPI SetFlags(WORD flags) = 0;

    // vraci delku vzorku (pouzitelne az po uspesnem volani metody Set)
    virtual int WINAPI GetLength() const = 0;

    // vraci vzorek (pouzitelne az po uspesnem volani metody Set)
    virtual const char* WINAPI GetPattern() const = 0;

    // vraci TRUE pokud je mozne zacit vyhledavat (vzorek i priznaky byly uspesne nastaveny,
    // neuspech hrozi jen pri prazdnem vzorku)
    virtual BOOL WINAPI IsGood() const = 0;

    // hledani vzorku v textu 'text' o delce 'length' od offsetu 'start' smerem dopredu;
    // vraci offset nalezeneho vzorku nebo -1 pokud vzorek nebyl nalezen;
    // POZOR: algoritmus musi mit nastaveny priznak SASF_FORWARD
    virtual int WINAPI SearchForward(const char* text, int length, int start) = 0;

    // hledani vzorku v textu 'text' o delce 'length' smerem zpet (zacina hledat na konci textu);
    // vraci offset nalezeneho vzorku nebo -1 pokud vzorek nebyl nalezen;
    // POZOR: algoritmus nesmi mit nastaveny priznak SASF_FORWARD
    virtual int WINAPI SearchBackward(const char* text, int length) = 0;
};

// interface objektu algoritmu pro vyhledavani pomoci regularnich vyrazu v textu
// POZOR: kazdy alokovany objekt je mozne pouzivat jen v ramci jednoho threadu
// (nemusi jit o hlavni thread, nemusi jit u vsech objektu o jeden thread)
class CSalamanderREGEXPSearchData
{
public:
    // nastaveni regularniho vyrazu; 'pattern' je null-terminated text regularniho vyrazu; 'flags'
    // jsou priznaky algoritmu (viz konstanty SASF_XXX); pri chybe vraci FALSE a popis chyby
    // je mozne ziskat volanim metody GetLastErrorText
    virtual BOOL WINAPI Set(const char* pattern, WORD flags) = 0;

    // nastaveni priznaku algoritmu; 'flags' jsou priznaky algoritmu (viz konstanty SASF_XXX);
    // pri chybe vraci FALSE a popis chyby je mozne ziskat volanim metody GetLastErrorText
    virtual BOOL WINAPI SetFlags(WORD flags) = 0;

    // vraci text chyby, ktera nastala v poslednim volani Set nebo SetFlags (muze byt i NULL)
    virtual const char* WINAPI GetLastErrorText() const = 0;

    // vraci text regularniho vyrazu (pouzitelne az po uspesnem volani metody Set)
    virtual const char* WINAPI GetPattern() const = 0;

    // nastaveni radky textu (radka je od 'start' do 'end', 'end' ukazuje za posledni znak radky),
    // ve kterem se vyhledava; vraci vzdy TRUE
    virtual BOOL WINAPI SetLine(const char* start, const char* end) = 0;

    // hledani podretezce odpovidajiciho regularnimu vyrazu v radce nastavene metodou SetLine;
    // hleda od offsetu 'start' smerem dopredu; vraci offset nalezeneho podretezce a jeho delku
    // (ve 'foundLen') nebo -1 pokud podretezec nebyl nalezen;
    // POZOR: algoritmus musi mit nastaveny priznak SASF_FORWARD
    virtual int WINAPI SearchForward(int start, int& foundLen) = 0;

    // hledani podretezce odpovidajiciho regularnimu vyrazu v radce nastavene metodou SetLine;
    // hleda smerem zpet (zacina hledat na konci textu o delce 'length' od zacatku radky);
    // vraci offset nalezeneho podretezce a jeho delku (ve 'foundLen') nebo -1 pokud podretezec
    // nebyl nalezen;
    // POZOR: algoritmus nesmi mit nastaveny priznak SASF_FORWARD
    virtual int WINAPI SearchBackward(int length, int& foundLen) = 0;
};

// typy prikazu Salamandera pouzite v metode CSalamanderGeneralAbstract::EnumSalamanderCommands
#define sctyUnknown 0
#define sctyForFocusedFile 1                 // jen pro focusly soubor (napr. View)
#define sctyForFocusedFileOrDirectory 2      // pro focusly soubor nebo adresar (napr. Open)
#define sctyForSelectedFilesAndDirectories 3 // pro oznacene/fokusle soubory a adresare (napr. Copy)
#define sctyForCurrentPath 4                 // pro aktualni cestu v panelu (napr. Create Directory)
#define sctyForConnectedDrivesAndFS 5        // pro pripojene svazky a FS (napr. Disconnect)

// prikazy Salamandera pouzite v metodach CSalamanderGeneralAbstract::EnumSalamanderCommands
// a CSalamanderGeneralAbstract::PostSalamanderCommand
// (POZOR: pro cisla prikazu je rezerovan jen interval <0, 499>)
#define SALCMD_VIEW 0     // view (klavesa F3 v panelu)
#define SALCMD_ALTVIEW 1  // alternate view (klavesa Alt+F3 v panelu)
#define SALCMD_VIEWWITH 2 // view with (klavesa Ctrl+Shift+F3 v panelu)
#define SALCMD_EDIT 3     // edit (klavesa F4 v panelu)
#define SALCMD_EDITWITH 4 // edit with (klavesa Ctrl+Shift+F4 v panelu)

#define SALCMD_OPEN 20        // open (klavesa Enter v panelu)
#define SALCMD_QUICKRENAME 21 // quick rename (klavesa F2 v panelu)

#define SALCMD_COPY 40          // copy (klavesa F5 v panelu)
#define SALCMD_MOVE 41          // move/rename (klavesa F6 v panelu)
#define SALCMD_EMAIL 42         // email (klavesa Ctrl+E v panelu)
#define SALCMD_DELETE 43        // delete (klavesa Delete v panelu)
#define SALCMD_PROPERTIES 44    // show properties (klavesa Alt+Enter v panelu)
#define SALCMD_CHANGECASE 45    // change case (klavesa Ctrl+F7 v panelu)
#define SALCMD_CHANGEATTRS 46   // change attributes (klavesa Ctrl+F2 v panelu)
#define SALCMD_OCCUPIEDSPACE 47 // calculate occupied space (klavesa Alt+F10 v panelu)

#define SALCMD_EDITNEWFILE 70     // edit new file (klavesa Shift+F4 v panelu)
#define SALCMD_REFRESH 71         // refresh (klavesa Ctrl+R v panelu)
#define SALCMD_CREATEDIRECTORY 72 // create directory (klavesa F7 v panelu)
#define SALCMD_DRIVEINFO 73       // drive info (klavesa Ctrl+F1 v panelu)
#define SALCMD_CALCDIRSIZES 74    // calculate directory sizes (klavesa Ctrl+Shift+F10 v panelu)

#define SALCMD_DISCONNECT 90 // disconnect (network drive or plugin-fs) (klavesa F12 v panelu)

#define MAX_GROUPMASK 1001 // max. pocet znaku (vcetne nuly na konci) ve skupinove masce

// identifikatory sdilenych historii (posledne pouzitych hodnot v comboboxech) pro
// CSalamanderGeneral::GetStdHistoryValues()
#define SALHIST_QUICKRENAME 1 // jmena v Quick Rename dialogu (F2)
#define SALHIST_COPYMOVETGT 2 // cilove cesty v Copy/Move dialogu (F5/F6)
#define SALHIST_CREATEDIR 3   // jmena adresaru v Create Directory dialogu (F7)
#define SALHIST_CHANGEDIR 4   // cesty v Change Directory dialogu (Shift+F7)
#define SALHIST_EDITNEW 5     // jmena v Edit New dialogu (Shift+F4)
#define SALHIST_CONVERT 6     // jmena v Conver dialogu (Ctrl+K)

// interface objektu pro praci se skupinou souborovych masek
// POZOR: metody objektu nejsou synchronizovane, takze je mozne je pouzivat jen
//        v ramci jednoho threadu (nemusi jit o hlavni thread) nebo si jejich
//        synchronizaci musi zajistit plugin (nesmi se provadet "zapis" behem
//        provadeni jine metody; "zapis"=SetMasksString+PrepareMasks;
//        provadeni "cteni" je mozne z vice threadu najednou; "cteni"=GetMasksString+
//        AgreeMasks)
//
// Zivotni cyklus objektu:
//   1) Alokujeme metodou CSalamanderGeneralAbstract::AllocSalamanderMaskGroup
//   2) V metode SetMasksString predame skupinu masek.
//   3) Zavolame PrepareMasks pro stavbu vnitrnich dat; v pripade neuspechu
//      zobrazime chybne misto a po oprave masky se vracime do bodu (3)
//   4) Libovolne volame AgreeMasks pro zjisteni, zda jmeno odpovida skupine masek.
//   5) Po pripadnem zavolani SetMasksString pokracujeme od (3)
//   6) Destrukce objektu metodou CSalamanderGeneralAbstract::FreeSalamanderMaskGroup
//
// Maska:
//   '?' - libovolny znak
//   '*' - libovolny retezec (i prazdny)
//   '#' - libovolna cislice (pouze je-li 'extendedMode'==TRUE)
//
//   Priklady:
//     *     - vsechna jmena
//     *.*   - vsechna jmena
//     *.exe - jmena s priponou "exe"
//     *.t?? - jmena s priponou zacinajici znakem 't' a obsahujici jeste dva libovolne znaky
//     *.r## - jmena s priponou zacinajici znakem 'r' a obsahujici jeste dve libovolne cislice
//
class CSalamanderMaskGroup
{
public:
    // nastavi retezec masek (masky jsou oddelene ';' (escape sekvence pro ';' je ";;"));
    // 'masks' je retezec masek (max. delka vcetne nuly na konci je MAX_GROUPMASK)
    // pokud je 'extendedMode' rovno TRUE, znak '#' odpovida libovolne cislici ('0'-'9')
    // jako oddelovac lze pouzit znak '|'; nasledujici masky (uz zase oddelene ';')
    // budou vyhodnocovany inverzne, tedy pokud jim bude odpovidat jmeno,
    // AgreeMasks vrati FALSE; znak '|' muze stat na zacatku retezce
    //
    //   Priklady:
    //     *.txt;*.cpp - vsechna jmena s priponou txt nebo cpp
    //     *.h*|*.html - vsechna jmena s priponou zacinajici znakem 'h', ale ne jmena s priponou "html"
    //     |*.txt      - vsechna jmena s jinou priponou nez "txt"
    virtual void WINAPI SetMasksString(const char* masks, BOOL extendedMode) = 0;

    // vraci retezec masek; 'buffer' je buffer o delce alespon MAX_GROUPMASK
    virtual void WINAPI GetMasksString(char* buffer) = 0;

    // vraci 'extendedMode' nastaveny v metode SetMasksString
    virtual BOOL WINAPI GetExtendedMode() = 0;

    // prace se souborovymi maskami: ('?' lib. znak, '*' lib. retezec - i prazdny, pokud
    //  byl 'extendedMode' v metode SetMasksString TRUE, '#' lib. cislice - '0'..'9'):
    // 1) prevedeme masky do jednodussiho formatu; 'errorPos' vraci pozici chyby v retezci masek;
    //    vraci TRUE pokud nenastala chyba (vraci FALSE -> je nastaveno 'errorPos')
    virtual BOOL WINAPI PrepareMasks(int& errorPos) = 0;
    // 2) muzeme pouzit prevedene masky na test, jestli nektere z nich odpovida soubor 'filename';
    //    'fileExt' ukazuje bud na konec 'fileName' nebo na priponu (pokud existuje), 'fileExt'
    //    muze byt NULL (pripona se dohledava podle std. pravidel); vraci TRUE pokud soubor
    //    odpovida alespon jedne z masek
    virtual BOOL WINAPI AgreeMasks(const char* fileName, const char* fileExt) = 0;
};

// interface objektu pro vypocet MD5
//
// Zivotni cyklus objektu:
//
//   1) Alokujeme metodou CSalamanderGeneralAbstract::AllocSalamanderMD5
//   2) Postupne volame metodu Update() pro data, pro ktere chceme vypocitat MD5
//   3) Zavolame metodu Finalize()
//   4) Vyzvedneme vypocitanou MD5 pomoci metody GetDigest()
//   5) Pokud chceme objekt znovu pouzit, zavolame metodu Init()
//      (vola se automaticky v kroku (1)) a jdeme do kroku (2)
//   6) Destrukce objektu metodou CSalamanderGeneralAbstract::FreeSalamanderMD5
//
class CSalamanderMD5
{
public:
    // inicializace objektu, je automaticky zavolana v konstruktoru
    // metoda je publikovana pro vicenasobne pouziti alokovaneho objektu
    virtual void WINAPI Init() = 0;

    // aktualizuje vnitrni stav objektu na zaklade bloku dat urceneho promennou 'input',
    // 'input_length' udava velikost bufferu v bajtech
    virtual void WINAPI Update(const void* input, DWORD input_length) = 0;

    // pripravi MD5 pro vyzvednuti pomoci metody GetDigest
    // po zavolani metody Finalize lze volat pouze metodu GetDigest() a Init()
    virtual void WINAPI Finalize() = 0;

    // vyzvedne MD5, 'dest' musi ukazovat do bufferu o velikosti 16 bajtu
    // metodu lze volat pouze po zavolani metody Finalize()
    virtual void WINAPI GetDigest(void* dest) = 0;
};

#define SALPNG_GETALPHA 0x00000002    // pri vytvareni DIB se nastavi take alpha kanal (jinak bude roven 0)
#define SALPNG_PREMULTIPLE 0x00000004 // ma vyznam, pokud je nastaveno SALPNG_GETALPHA; prednasobi RGB slozky tak, aby bylo na bitmapu mozne zavolat AlphaBlend() s BLENDFUNCTION::AlphaFormat==AC_SRC_ALPHA

class CSalamanderPNGAbstract
{
public:
    // vytvori bitmapu na zaklade PNG resource; 'hInstance' a 'lpBitmapName' specifikuji resource,
    // 'flags' obsahuje 0 nebo bity z rodiny SALPNG_xxx
    // v pripade uspechu vraci handle bitmapy, jinak NULL
    // plugin je zodpovedny za destrukci bitmapy volanim DeleteObject()
    // mozne volat z libovolneho threadu
    virtual HBITMAP WINAPI LoadPNGBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName, DWORD flags, COLORREF unused) = 0;

    // vytvori bitmapu na zaklade PNG podaneho v pameti; 'rawPNG' je ukazatel na pamet obsahujici PNG
    // (napriklad nactene ze souboru) a 'rawPNGSize' urcuje velikost pameti obsazene PNG v bajtech,
    // 'flags' obsahuje 0 nebo bity z rodiny SALPNG_xxx
    // v pripade uspechu vraci handle bitmapy, jinak NULL
    // plugin je zodpovedny za destrukci bitmapy volanim DeleteObject()
    // mozne volat z libovolneho threadu
    virtual HBITMAP WINAPI LoadRawPNGBitmap(const void* rawPNG, DWORD rawPNGSize, DWORD flags, COLORREF unused) = 0;

    // poznamka 1: nacitane PNG je vhodne komprimovat pomoci PNGSlim, viz https://forum.altap.cz/viewtopic.php?f=15&t=3278
    // poznamka 2: ukazka primeho pristupu k datum DIB viz Demoplugin, funkce AlphaBlend
    // poznamka 3: podporeny jsou non-interlaced PNG typu Greyscale, Greyscale with alpha, Truecolour, Truecolour with alpha, Indexed-colour
    //             podminkou je 8 bitu na jeden kanal
};

// vsechny metody je mozne volat pouze z hlavniho threadu
class CSalamanderPasswordManagerAbstract
{
public:
    // vrati TRUE, pokud uzivatel nastavil master password v konfiguraci Salamandera, jinak vrati FALSE
    // (nesouvisi s tim, zda byl MP v teto session zadan)
    virtual BOOL WINAPI IsUsingMasterPassword() = 0;

    // vrati TRUE, pokud uzivatel v ramci teto session Salamandera zadal korektni master password, jinak vrati FALSE
    virtual BOOL WINAPI IsMasterPasswordSet() = 0;

    // zobrazi okno s predkem 'hParent' ve kterem vyzve k zadani master password
    // vraci TRUE, pokud byl spravny MP zadan, jinak vrati FALSE
    // pta se i v pripade, ze jiz byl master password v teto session zadan, viz IsMasterPasswordSet()
    // pokud uzivatel nepouziva master password, vraci FALSE, viz IsUsingMasterPassword()
    virtual BOOL WINAPI AskForMasterPassword(HWND hParent) = 0;

    // precte 'plainPassword' zakonceny nulou a na zaklade promenne 'encrypt' jej bud zasifruje (pokud je TRUE) pomoci AES nebo
    // pouze zascrambli (pokud je FALSE); alokovany vysledek ulozi do 'encryptedPassword' a jeho velikost vrati v promenne
    // 'encryptedPasswordSize'; vraci TRUE v pripade uspechu, jinak FALSE
    // pokud je 'encrypt'==TRUE, musi volajici pred volani funkce zajistit, ze je zadan master password, viz AskForMasterPassword()
    // poznamka: vraceny 'encryptedPassword' je alokovany na heapu Salamandera; pokud plugin nepouziva salrtl, musi buffer uvolnit
    // pomoci SalamanderGeneral->Free(), jinak staci volat free();
    virtual BOOL WINAPI EncryptPassword(const char* plainPassword, BYTE** encryptedPassword, int* encryptedPasswordSize, BOOL encrypt) = 0;

    // precte 'encryptedPassword' o velikosti 'encryptedPasswordSize' a prevede ho na otevrene heslo, ktere vrati
    // v alokovanem bufferu 'plainPassword'; vrati TRUE v pripade uspechu, jinak FALSE
    // poznamka: vraceny 'plainPassword' je alokovany na heapu Salamandera; pokud plugin nepouziva salrtl, musi buffer uvolnit
    // pomoci SalamanderGeneral->Free(), jinak staci volat free();
    virtual BOOL WINAPI DecryptPassword(const BYTE* encryptedPassword, int encryptedPasswordSize, char** plainPassword) = 0;

    // vrati TRUE, pokud je 'encyptedPassword' o delce 'encyptedPasswordSize' sifrovano pomoci AES; jinak vrati FALSE
    virtual BOOL WINAPI IsPasswordEncrypted(const BYTE* encyptedPassword, int encyptedPasswordSize) = 0;
};

// rezimy pro metodu CSalamanderGeneralAbstract::ExpandPluralFilesDirs
#define epfdmNormal 0   // XXX files and YYY directories
#define epfdmSelected 1 // XXX selected files and YYY selected directories
#define epfdmHidden 2   // XXX hidden files and YYY hidden directories

// prikazy pro HTML help: viz metoda CSalamanderGeneralAbstract::OpenHtmlHelp
enum CHtmlHelpCommand
{
    HHCDisplayTOC,     // viz HH_DISPLAY_TOC: dwData = 0 (zadny topic) nebo: pointer to a topic within a compiled help file
    HHCDisplayIndex,   // viz HH_DISPLAY_INDEX: dwData = 0 (zadny keyword) nebo: keyword to select in the index (.hhk) file
    HHCDisplaySearch,  // viz HH_DISPLAY_SEARCH: dwData = 0 (prazdne hledani) nebo: pointer to an HH_FTS_QUERY structure
    HHCDisplayContext, // viz HH_HELP_CONTEXT: dwData = numeric ID of the topic to display
};

// slouzi jako parametr OpenHtmlHelpForSalamander pri command==HHCDisplayContext
#define HTMLHELP_SALID_PWDMANAGER 1 // zobrazi napovedu pro Password Manager

class CPluginFSInterfaceAbstract;

class CSalamanderZLIBAbstract;

class CSalamanderBZIP2Abstract;

class CSalamanderCryptAbstract;

class CSalamanderGeneralAbstract
{
public:
    // zobrazi message-box se zadanym textem a titulkem, parent message-boxu je HWND
    // vracene metodou GetMsgBoxParent() (viz nize); pouziva SalMessageBox (viz nize)
    // type = MSGBOX_INFO        - informace (ok)
    // type = MSGBOX_ERROR       - chybova hlaska (ok)
    // type = MSGBOX_EX_ERROR    - chybova hlaska (ok/cancel) - vraci IDOK, IDCANCEL
    // type = MSGBOX_QUESTION    - dotaz (yes/no) - vraci IDYES, IDNO
    // type = MSGBOX_EX_QUESTION - dotaz (yes/no/cancel) - vraci IDYES, IDNO, IDCANCEL
    // type = MSGBOX_WARNING     - varovani (ok)
    // type = MSGBOX_EX_WARNING  - varovani (yes/no/cancel) - vraci IDYES, IDNO, IDCANCEL
    // v pripade chyby vraci 0
    // omezeni: hlavni thread
    virtual int WINAPI ShowMessageBox(const char* text, const char* title, int type) = 0;

    // SalMessageBox a SalMessageBoxEx vytvori, zobrazi a po zvoleni nektereho z tlacitek
    // zavrou message box. Message box muze obsahovat uzivatelem definovany titulek, zpravu,
    // tlacitka, ikonu, checkbox s nejakym textem.
    //
    // Pokud neni 'hParent' prave foreground window (msgbox v neaktivni aplikaci), dojde
    // pred vybalenim msgboxu k volani FlashWindow(mainwnd, TRUE) a po zavreni msgboxu
    // k volani FlashWindow(mainwnd, FALSE), mainwnd je v rade parentu okna 'hParent' to,
    // ktere uz nema parenta (typicky jde o hlavni okno Salamandera).
    //
    // SalMessageBox naplni strukturu MSGBOXEX_PARAMS (hParent->HParent, lpText->Text,
    // lpCaption->Caption a uType->Flags; ostatni polozky struktury vynuluje) a vola
    // SalMessageBoxEx, takze dale budeme popisovat pouze SalMessageBoxEx.
    //
    // SalMessageBoxEx se snazi v maximalni mire chovat stejne jako Windows API funkce
    // MessageBox a MessageBoxIndirect. Rozdily jsou:
    //   - message box je centrovany k hParent (pokud jde o child okno, dohleda se non-child parent)
    //   - v pripade MB_YESNO/MB_ABORTRETRYIGNORE message boxu lze povolit
    //     zavreni okna klavesou Escape nebo kliknutim na krizek v titulku (flag
    //     MSGBOXEX_ESCAPEENABLED); navratova hodnota pak bude IDNO/IDCANCEL
    //   - lze potlacit pipnuti (flag MSGBOXEX_SILENT)
    //
    // Komentar k uType viz komentar k MSGBOXEX_PARAMS::Flags
    //
    // Return Values
    //    DIALOG_FAIL       (0)            The function fails.
    //    DIALOG_OK         (IDOK)         'OK' button was selected.
    //    DIALOG_CANCEL     (IDCANCEL)     'Cancel' button was selected.
    //    DIALOG_ABORT      (IDABORT)      'Abort' button was selected.
    //    DIALOG_RETRY      (IDRETRY)      'Retry' button was selected.
    //    DIALOG_IGNORE     (IDIGNORE)     'Ignore' button was selected.
    //    DIALOG_YES        (IDYES)        'Yes' button was selected.
    //    DIALOG_NO         (IDNO)         'No' button was selected.
    //    DIALOG_TRYAGAIN   (IDTRYAGAIN)   'Try Again' button was selected.
    //    DIALOG_CONTINUE   (IDCONTINUE)   'Continue' button was selected.
    //    DIALOG_SKIP                      'Skip' button was selected.
    //    DIALOG_SKIPALL                   'Skip All' button was selected.
    //    DIALOG_ALL                       'All' button was selected.
    //
    // SalMessageBox a SalMessageBoxEx je mozne volat z libovolneho threadu
    virtual int WINAPI SalMessageBox(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType) = 0;
    virtual int WINAPI SalMessageBoxEx(const MSGBOXEX_PARAMS* params) = 0;

    // vraci HWND vhodneho parenta pro otevirane message-boxy (nebo jina modalni okna),
    // jde o hlavni okno, progress-dialog, Plugins/Plugins dialog nebo jine k hlavnimu oknu
    // otevrene modalni okno
    // omezeni: hlavni thread, vracene HWND je vzdy z hlavniho threadu
    virtual HWND WINAPI GetMsgBoxParent() = 0;

    // vraci handle hlavniho okna Salamandera
    // mozne volat z libovolneho threadu
    virtual HWND WINAPI GetMainWindowHWND() = 0;

    // obnovi fokus v panelu nebo v command line (podle toho co bylo naposledy aktivovano); toto
    // volani je potreba, pokud plugin disabluje/enabluje hlavni okno Salamandera (vznikaji tak
    // situace, kdy se aktivuje disablovane hl. okno - v disablovanem okne nelze nastavit
    // fokus - po enablovani hl. okna je treba fokus obnovit touto metodou)
    virtual void WINAPI RestoreFocusInSourcePanel() = 0;

    // casto pouzivane dialogy, parent dialogu 'parent', navratove hodnoty DIALOG_XXX;
    // pokud neni 'parent' prave foreground window (dialog v neaktivni aplikaci), dojde
    // pred vybalenim dialogu k volani FlashWindow(mainwnd, TRUE) a po zavreni dialogu
    // k volani FlashWindow(mainwnd, FALSE), mainwnd je v rade parentu okna 'parent' to,
    // ktere uz nema parenta (typicky jde o hlavni okno Salamandera)
    // ERROR: filename+error+title (pokud 'title' == NULL, jde o std. titulek "Error")
    //
    // Promenna 'flags' urcuje zobrazena tlacitka, pro DialogError lze pouzit jednu z hodnot:
    // BUTTONS_OK               // OK                                    (old DialogError3)
    // BUTTONS_RETRYCANCEL      // Retry / Cancel                        (old DialogError4)
    // BUTTONS_SKIPCANCEL       // Skip / Skip all / Cancel              (old DialogError2)
    // BUTTONS_RETRYSKIPCANCEL  // Retry / Skip / Skip all / Cancel      (old DialogError)
    //
    // vse je mozne volat z libovolneho threadu
    virtual int WINAPI DialogError(HWND parent, DWORD flags, const char* fileName, const char* error, const char* title) = 0;

    // CONFIRM FILE OVERWRITE: filename1+filedata1+filename2+filedata2
    // Promenna 'flags' urcuje zobrazena tlacitka, pro DialogOverwrite lze pouzit jednu z hodnot:
    // BUTTONS_YESALLSKIPCANCEL // Yes / All / Skip / Skip all / Cancel  (old DialogOverwrite)
    // BUTTONS_YESNOCANCEL      // Yes / No / Cancel                     (old DialogOverwrite2)
    virtual int WINAPI DialogOverwrite(HWND parent, DWORD flags, const char* fileName1, const char* fileData1,
                                       const char* fileName2, const char* fileData2) = 0;

    // QUESTION: filename+question+title (pokud 'title' == NULL, jde o std. titulek "Question")
    // Promenna 'flags' urcuje zobrazena tlacitka, pro DialogQuestion lze pouzit jednu z hodnot:
    // BUTTONS_YESALLSKIPCANCEL // Yes / All / Skip / Skip all / Cancel  (old DialogQuestion)
    // BUTTONS_YESNOCANCEL      // Yes / No / Cancel                     (old DialogQuestion2)
    // BUTTONS_YESALLCANCEL     // Yes / All / Cancel                    (old DialogQuestion3)
    virtual int WINAPI DialogQuestion(HWND parent, DWORD flags, const char* fileName,
                                      const char* question, const char* title) = 0;

    // pokud cesta 'dir' neexistuje, umozni ji vytvorit (zepta se usera; vytvari pripadne
    // i vice adresaru na konci cesty); pokud cesta existuje nebo je uspesne vytvorena vraci TRUE;
    // pokud cesta neexistuje a je-li 'quiet' TRUE, nepta se usera jestli chce vytvaret
    // cestu 'dir'; pokud je 'errBuf' NULL, ukazuje chyby v oknech; pokud 'errBuf' neni NULL,
    // dava popis chyb do bufferu 'errBuf' o velikosti 'errBufSize' (zadna chybova okna se
    // neoteviraji); vsechny otevirana okna maji za parenta 'parent', je-li 'parent' NULL,
    // pouziva se hlavni okno Salamandera; neni-li 'firstCreatedDir' NULL, jde o buffer
    // o velikosti MAX_PATH pro ulozeni plneho jmena prvniho vytvareneho adresare na ceste
    // 'dir' (vraci prazdny retezec pokud jiz cesta 'dir' existuje); je-li 'manualCrDir' TRUE,
    // nedovoli vytvorit adresar s mezerou na zacatku jmena (Windowsum to nevadi, ale je to
    // potencialne nebezpecne, napr. Explorer to taky nedovoluje)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI CheckAndCreateDirectory(const char* dir, HWND parent = NULL, BOOL quiet = TRUE,
                                                char* errBuf = NULL, int errBufSize = 0,
                                                char* firstCreatedDir = NULL, BOOL manualCrDir = FALSE) = 0;

    // zjisti volne misto na ceste path a pokud neni >= totalSize zepta se jestli chce user pokracovat;
    // okno dotazu ma parenta 'parent', vraci TRUE pokud je dost mista nebo pokud uzivatel odpovedel
    // "pokracovat"; pokud neni 'parent' prave foreground window (dialog v neaktivni aplikaci), dojde
    // pred vybalenim dialogu k volani FlashWindow(mainwnd, TRUE) a po zavreni dialogu
    // k volani FlashWindow(mainwnd, FALSE), mainwnd je v rade parentu okna 'parent' to,
    // ktere uz nema parenta (typicky jde o hlavni okno Salamandera)
    // 'messageTitle' bude zobrazena v titulku messageboxu s dotazem a melo by se jednat
    // o nazev pluginu, ktery metodu zavolal
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI TestFreeSpace(HWND parent, const char* path, const CQuadWord& totalSize,
                                      const char* messageTitle) = 0;

    // vraci v 'retValue' (nesmi byt NULL) volne misto na dane ceste (zatim nejkorektnejsi
    // udaj, jaky se da z Windows ziskat, na NT/W2K/XP/Vista umi pracovat i s reparse pointy
    // a substy (Salamander 2.5 pracuje jen s junction-pointy)); 'path' je cesta, na
    // ktere zjistujeme volne misto (nemusi byt koren); neni-li 'total' NULL, vraci se v nem
    // celkova velikost disku, pokud dojde k chybe, vraci CQuadWord(-1, -1)
    // mozne volat z libovolneho threadu
    virtual void WINAPI GetDiskFreeSpace(CQuadWord* retValue, const char* path, CQuadWord* total) = 0;

    // vlastni klon windowsove GetDiskFreeSpace: umi zjistit korektni udaje pro cesty obsahujici
    // substy a reparse pointy pod Windows 2000/XP/Vista/7 (Salamander 2.5 pracuje jen
    // s junction-pointy); 'path' je cesta, na ktere zjistujeme volne misto; ostatni parametry
    // odpovidaji standardni Win32 API funkci GetDiskFreeSpace
    //
    // POZOR: nepouzivat navratovky 'lpNumberOfFreeClusters' a 'lpTotalNumberOfClusters', protoze
    //        u vetsich disku jsou v nich nesmysly (DWORD nemusi stacit pro celkovy pocet clusteru),
    //        resit pres predchozi metodu GetDiskFreeSpace, ktera vraci 64-bitova cisla
    //
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalGetDiskFreeSpace(const char* path, LPDWORD lpSectorsPerCluster,
                                            LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                                            LPDWORD lpTotalNumberOfClusters) = 0;

    // vlastni klon windowsove GetVolumeInformation: umi zjistit korektni udaje i pro
    // cesty obsahujici substy a reparse pointy pod Windows 2000/XP/Vista (Salamander 2.5
    // pracuje jen s junction-pointy); 'path' je cesta, pro kterou zjistujeme informace;
    // v 'rootOrCurReparsePoint' (neni-li NULL, musi byt alespon MAX_PATH
    // znaku velky buffer) se vraci korenovy adresar nebo aktualni (posledni) lokalni reparse
    // point na ceste 'path' (Salamander 2.5 vraci cestu, pro kterou se podarilo zjistit
    // udaje nebo aspon korenovy adresar); ostatni parametry odpovidaji standardni Win32 API
    // funkci GetVolumeInformation
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalGetVolumeInformation(const char* path, char* rootOrCurReparsePoint, LPTSTR lpVolumeNameBuffer,
                                                DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                                                LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                                                LPTSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) = 0;

    // vlastni klon windowsove GetDriveType: umi zjistit korektni udaje i pro cesty
    // obsahujici substy a reparse pointy pod Windows 2000/XP/Vista (Salamander 2.5
    // pracuje jen s junction-pointy); 'path' je cesta, jejiz typ zjistujeme
    // mozne volat z libovolneho threadu
    virtual UINT WINAPI SalGetDriveType(const char* path) = 0;

    // protoze windowsova GetTempFileName nefunguje, napsali jsme si vlastni klon:
    // vytvori soubor/adresar (podle 'file') na ceste 'path' (NULL -> Windows TEMP dir),
    // s prefixem 'prefix', vraci jmeno vytvoreneho souboru v 'tmpName' (min. velikost MAX_PATH),
    // vraci uspech (pri neuspechu vraci v 'err' (neni-li NULL) kod Windows chyby)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalGetTempFileName(const char* path, const char* prefix, char* tmpName, BOOL file, DWORD* err) = 0;

    // odstraneni adresare vcetne jeho obsahu (SHFileOperation je priserne pomaly)
    // mozne volat z libovolneho threadu
    virtual void WINAPI RemoveTemporaryDir(const char* dir) = 0;

    // protoze windowsova verze MoveFile nezvlada prejmenovani souboru s read-only atributem na Novellu,
    // napsali jsme si vlastni (nastane-li chyba pri MoveFile, zkusi shodit read-only, provest operaci,
    // a pak ho zase nahodit); vraci uspech (pri neuspechu vraci v 'err' (neni-li NULL) kod Windows chyby)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalMoveFile(const char* srcName, const char* destName, DWORD* err) = 0;

    // varianta k windowsove verzi GetFileSize (ma jednodussi osetreni chyb); 'file' je otevreny
    // soubor pro volani GetFileSize(); v 'size' vraci ziskanou velikost souboru; vraci uspech,
    // pri FALSE (chyba) je v 'err' windowsovy kod chyby a v 'size' nula;
    // POZNAMKA: existuje varianta SalGetFileSize2(), ktera pracuje s plnym jmenem souboru
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalGetFileSize(HANDLE file, CQuadWord& size, DWORD& err) = 0;

    // otevre soubor/adresar 'name' na ceste 'path'; ridi se Windows asociacemi, otevira
    // pres polozku Open v kontextovem menu (muze pouzit i salopen.exe, zalezi na konfiguraci);
    // pred spustenim nastavi aktualni adresare na lokalnich discich podle panelu;
    // 'parent' je parent pripadnych oken (napr. pri otevirani neasociovaneho souboru)
    // omezeni: hlavni thread (jinak by nefungoval salopen.exe - pouziva jednu sdilenou pamet)
    virtual void WINAPI ExecuteAssociation(HWND parent, const char* path, const char* name) = 0;

    // otevre browse dialog, ve kterem uzivatel vybere cestu; 'parent' je parent browse dialogu;
    // 'hCenterWindow' - okno, ke kteremu bude dialog centrovan; 'title' je titulek browse dialogu;
    // 'comment' je komentar v browse dialogu; 'path' je buffer pro vyslednou cestu (min. MAX_PATH
    // znaku); je-li 'onlyNet' TRUE, lze prochazet jen sitove cesty (jinak neni omezeno); neni-li
    // 'initDit' NULL, obsahuje cestu na ktere se ma browse dialog otevrit; vraci TRUE pokud je
    // v 'path' nova vybrana cesta
    // POZOR: pokud se vola mimo hlavni thread, je nutne drive inicializovat COM (mozna lepe cele
    //        OLE - viz CoInitialize nebo OLEInitialize)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI GetTargetDirectory(HWND parent, HWND hCenterWindow, const char* title,
                                           const char* comment, char* path, BOOL onlyNet,
                                           const char* initDir) = 0;

    // prace se souborovymi maskami: ('?' lib. znak, '*' lib. retezec - i prazdny)
    // vse je mozne volat z libovolneho threadu
    // 1) prevedeme masku do jednodussiho formatu (src -> bufferu mask - min. velikost
    //    bufferu 'mask' je (strlen(src) + 1))
    virtual void WINAPI PrepareMask(char* mask, const char* src) = 0;
    // 2) muzeme pouzit prevedenou masku na test, jestli ji odpovida soubor filename,
    //    hasExtension = TRUE pokud soubor ma priponu
    //    vraci TRUE pokud soubor odpovida masce
    virtual BOOL WINAPI AgreeMask(const char* filename, const char* mask, BOOL hasExtension) = 0;
    // 3) neupravenou masku (nevolat pro ni PrepareMask) muzeme pouzit k vytvoreni jmena podle
    //    zadaneho jmena a masky ("a.txt" + "*.cpp" -> "a.cpp" atd.),
    //    buffer by mel byt velky alespon strlen(name)+strlen(mask) (hodi se 2*MAX_PATH)
    //    vraci vytvorene jmeno (ukazatel 'buffer')
    virtual char* WINAPI MaskName(char* buffer, int bufSize, const char* name, const char* mask) = 0;

    // prace s rozsirenymi souborovymi maskami: ('?' lib. znak, '*' lib. retezec - i prazdny,
    // '#' lib. cislice - '0'..'9')
    // vse je mozne volat z libovolneho threadu
    // 1) prevedeme masku do jednodussiho formatu (src -> bufferu mask - min. delka strlen(src) + 1)
    virtual void WINAPI PrepareExtMask(char* mask, const char* src) = 0;
    // 2) muzeme pouzit prevedenou masku na test, jestli ji odpovida soubor filename,
    //    hasExtension = TRUE pokud soubor ma priponu
    //    vraci TRUE pokud soubor odpovida masce
    virtual BOOL WINAPI AgreeExtMask(const char* filename, const char* mask, BOOL hasExtension) = 0;

    // alokuje novy objekt pro praci se skupinou souborovych masek
    // mozne volat z libovolneho threadu
    virtual CSalamanderMaskGroup* WINAPI AllocSalamanderMaskGroup() = 0;

    // uvolni objekt pro praci se skupinou souborovych masek (ziskany metodou AllocSalamanderMaskGroup)
    // mozne volat z libovolneho threadu
    virtual void WINAPI FreeSalamanderMaskGroup(CSalamanderMaskGroup* maskGroup) = 0;

    // alokace pameti na heapu Salamandera (pri pouziti salrtl9.dll zbytecne - staci klasicky malloc);
    // pri nedostatku pameti se zobrazi uzivateli hlaseni s tlacitky Retry (dalsi pokus o alokaci),
    // Abort (po dalsim dotazu terminuje aplikaci) a Ignore (pusteni chyby alokace do aplikace - po
    // upozorneni uzivatele, ze aplikace muze spadnout, vrati Alloc NULL;
    // osetreni na NULL ma smysl asi jen pro velke bloky pameti, napr. vic nez 500 MB, zde hrozi,
    // ze alokace nebude mozna z duvodu fragmentace adresniho prostoru nactenymi DLL knihovnami);
    // POZNAMKA: Realloc() byl pridan pozdeji, je nize v tomto modulu
    // mozne volat z libovolneho threadu
    virtual void* WINAPI Alloc(int size) = 0;
    // dealokace pameti z heapu Salamandera (pri pouziti salrtl9.dll zbytecne - staci klasicky free)
    // mozne volat z libovolneho threadu
    virtual void WINAPI Free(void* ptr) = 0;

    // duplikace stringu - alokace pameti (na heapu Salamadera - heapu dostupnem pres salrtl9.dll)
    // + kopie stringu; pri 'str'==NULL vraci NULL;
    // mozne volat z libovolneho threadu
    virtual char* WINAPI DupStr(const char* str) = 0;

    // vraci mapovaci tabulku na mala a velka pismena (pole 256 znaku - male/velke pismeno na
    // indexu zjistovaneho pismene); neni-li 'lowerCase' NULL, vraci se v nem tabulka malych pismen;
    // neni-li 'upperCase' NULL, vraci se v nem tabulka velkych pismen
    // mozne volat z libovolneho threadu
    virtual void WINAPI GetLowerAndUpperCase(unsigned char** lowerCase, unsigned char** upperCase) = 0;

    // prevod retezce 'str' na mala/velka pismena; narozdil od ANSI C tolower/toupper pracuje
    // rovnou s retezcem a podporuje nejen znaky 'A' az 'Z' (prevod na mala pismena provadi pres
    // pole inicializovane Win32 API funkci CharLower)
    virtual void WINAPI ToLowerCase(char* str) = 0;
    virtual void WINAPI ToUpperCase(char* str) = 0;

    //*****************************************************************************
    //
    // StrCmpEx
    //
    // Function compares two substrings.
    // If the two substrings are of different lengths, they are compared up to the
    // length of the shortest one. If they are equal to that point, then the return
    // value will indicate that the longer string is greater.
    //
    // Parameters
    //   s1, s2: strings to compare
    //   l1    : compared length of s1 (must be less or equal to strlen(s1))
    //   l2    : compared length of s2 (must be less or equal to strlen(s1))
    //
    // Return Values
    //   -1 if s1 < s2 (if substring pointed to by s1 is less than the substring pointed to by s2)
    //    0 if s1 = s2 (if the substrings are equal)
    //   +1 if s1 > s2 (if substring pointed to by s1 is greater than the substring pointed to by s2)
    //
    // Method can be called from any thread.
    virtual int WINAPI StrCmpEx(const char* s1, int l1, const char* s2, int l2) = 0;

    //*****************************************************************************
    //
    // StrICpy
    //
    // Function copies characters from source to destination. Upper case letters are mapped to
    // lower case using LowerCase array (filled using CharLower Win32 API call).
    //
    // Parameters
    //   dest: pointer to the destination string
    //   src: pointer to the null-terminated source string
    //
    // Return Values
    //   The StrICpy returns the number of bytes stored in buffer, not counting
    //   the terminating null character.
    //
    // Method can be called from any thread.
    virtual int WINAPI StrICpy(char* dest, const char* src) = 0;

    //*****************************************************************************
    //
    // StrICmp
    //
    // Function compares two strings. The comparison is not case sensitive and ignores
    // regional settings. For the purposes of the comparison, all characters are converted
    // to lower case using LowerCase array (filled using CharLower Win32 API call).
    //
    // Parameters
    //   s1, s2: null-terminated strings to compare
    //
    // Return Values
    //   -1 if s1 < s2 (if string pointed to by s1 is less than the string pointed to by s2)
    //    0 if s1 = s2 (if the strings are equal)
    //   +1 if s1 > s2 (if string pointed to by s1 is greater than the string pointed to by s2)
    //
    // Method can be called from any thread.
    virtual int WINAPI StrICmp(const char* s1, const char* s2) = 0;

    //*****************************************************************************
    //
    // StrICmpEx
    //
    // Function compares two substrings. The comparison is not case sensitive and ignores
    // regional settings. For the purposes of the comparison, all characters are converted
    // to lower case using LowerCase array (filled using CharLower Win32 API call).
    // If the two substrings are of different lengths, they are compared up to the
    // length of the shortest one. If they are equal to that point, then the return
    // value will indicate that the longer string is greater.
    //
    // Parameters
    //   s1, s2: strings to compare
    //   l1    : compared length of s1 (must be less or equal to strlen(s1))
    //   l2    : compared length of s2 (must be less or equal to strlen(s2))
    //
    // Return Values
    //   -1 if s1 < s2 (if substring pointed to by s1 is less than the substring pointed to by s2)
    //    0 if s1 = s2 (if the substrings are equal)
    //   +1 if s1 > s2 (if substring pointed to by s1 is greater than the substring pointed to by s2)
    //
    // Method can be called from any thread.
    virtual int WINAPI StrICmpEx(const char* s1, int l1, const char* s2, int l2) = 0;

    //*****************************************************************************
    //
    // StrNICmp
    //
    // Function compares two strings. The comparison is not case sensitive and ignores
    // regional settings. For the purposes of the comparison, all characters are converted
    // to lower case using LowerCase array (filled using CharLower Win32 API call).
    // The comparison stops after: (1) a difference between the strings is found,
    // (2) the end of the string is reached, or (3) n characters have been compared.
    //
    // Parameters
    //   s1, s2: strings to compare
    //   n:      maximum length to compare
    //
    // Return Values
    //   -1 if s1 < s2 (if substring pointed to by s1 is less than the substring pointed to by s2)
    //    0 if s1 = s2 (if the substrings are equal)
    //   +1 if s1 > s2 (if substring pointed to by s1 is greater than the substring pointed to by s2)
    //
    // Method can be called from any thread.
    virtual int WINAPI StrNICmp(const char* s1, const char* s2, int n) = 0;

    //*****************************************************************************
    //
    // MemICmp
    //
    // Compares n bytes of the two blocks of memory stored at buf1 and buf2.
    // Characters are converted to lowercase before comparing (not permanently;
    // using LowerCase array which was filled using CharLower Win32 API call),
    // so case is ignored in comparation.
    //
    // Parameters
    //   buf1, buf2: memory buffers to compare
    //   n:          maximum length to compare
    //
    // Return Values
    //   -1 if buf1 < buf2 (if buffer pointed to by buf1 is less than the buffer pointed to by buf2)
    //    0 if buf1 = buf2 (if the buffers are equal)
    //   +1 if buf1 > buf2 (if buffer pointed to by buf1 is greater than the buffer pointed to by buf2)
    //
    // Method can be called from any thread.
    virtual int WINAPI MemICmp(const void* buf1, const void* buf2, int n) = 0;

    // porovnani dvou retezcu 's1' a 's2' bez ohledu na velikost pismen (ignore-case),
    // je-li SALCFG_SORTUSESLOCALE TRUE, pouziva razeni podle regionalniho nastaveni Windows,
    // jinak porovnava stejne jako CSalamanderGeneral::StrICmp, je-li SALCFG_SORTDETECTNUMBERS
    // TRUE, pouziva ciselne razeni pro cisla obsazene v retezcich
    // vraci <0 ('s1' < 's2'), ==0 ('s1' == 's2'), >0 ('s1' > 's2')
    virtual int WINAPI RegSetStrICmp(const char* s1, const char* s2) = 0;

    // porovnani dvou retezcu 's1' a 's2' (o delkach 'l1' a 'l2') bez ohledu na velikost
    // pismen (ignore-case), je-li SALCFG_SORTUSESLOCALE TRUE, pouziva razeni podle
    // regionalniho nastaveni Windows, jinak porovnava stejne jako CSalamanderGeneral::StrICmp,
    // je-li SALCFG_SORTDETECTNUMBERS TRUE, pouziva ciselne razeni pro cisla obsazene
    // v retezcich; v 'numericalyEqual' (neni-li NULL) vraci TRUE, pokud jsou retezce
    // numericky shodne (napr. "a01" a "a1"), je automaticky TRUE pokud jsou retezce shodne
    // vraci <0 ('s1' < 's2'), ==0 ('s1' == 's2'), >0 ('s1' > 's2')
    virtual int WINAPI RegSetStrICmpEx(const char* s1, int l1, const char* s2, int l2,
                                       BOOL* numericalyEqual) = 0;

    // porovnani (case-sensitive) dvou retezcu 's1' a 's2', je-li SALCFG_SORTUSESLOCALE TRUE,
    // pouziva razeni podle regionalniho nastaveni Windows, jinak porovnava stejne jako
    // standardni C knihovni funkce strcmp, je-li SALCFG_SORTDETECTNUMBERS TRUE, pouziva
    // ciselne razeni pro cisla obsazene v retezcich
    // vraci <0 ('s1' < 's2'), ==0 ('s1' == 's2'), >0 ('s1' > 's2')
    virtual int WINAPI RegSetStrCmp(const char* s1, const char* s2) = 0;

    // porovnani (case-sensitive) dvou retezcu 's1' a 's2' (o delkach 'l1' a 'l2'), je-li
    // SALCFG_SORTUSESLOCALE TRUE, pouziva razeni podle regionalniho nastaveni Windows,
    // jinak porovnava stejne jako standardni C knihovni funkce strcmp, je-li
    // SALCFG_SORTDETECTNUMBERS TRUE, pouziva ciselne razeni pro cisla obsazene v retezcich;
    // v 'numericalyEqual' (neni-li NULL) vraci TRUE, pokud jsou retezce numericky shodne
    // (napr. "a01" a "a1"), je automaticky TRUE pokud jsou retezce shodne
    // vraci <0 ('s1' < 's2'), ==0 ('s1' == 's2'), >0 ('s1' > 's2')
    virtual int WINAPI RegSetStrCmpEx(const char* s1, int l1, const char* s2, int l2,
                                      BOOL* numericalyEqual) = 0;

    // vraci cestu v panelu; 'panel' je jeden z PANEL_XXX; 'buffer' je buffer pro cestu (muze
    // byt i NULL); 'bufferSize' je velikost bufferu 'buffer' (je-li 'buffer' NULL, musi zde
    // byt nula); 'type' neni-li NULL ukazuje na promennou, do ktere se ulozi typ cesty
    // (viz PATH_TYPE_XXX); pokud jde o archiv a 'archiveOrFS' neni NULL a 'buffer' neni NULL,
    // vraci 'archiveOrFS' nastavene do 'buffer' na pozici za soubor archivu;
    // pokud jde o file-system a 'archiveOrFS' neni NULL a 'buffer' neni NULL, vraci
    // 'archiveOrFS' nastavene do 'buffer' na ':' za nazvem file-systemu (za ':' je user-part
    // cesty file-systemu); je-li 'convertFSPathToExternal' TRUE a v panelu je cesta na FS,
    // najde se plugin jehoz cesta je (podle fs-name) a zavola se jeho
    // CPluginInterfaceForFSAbstract::ConvertPathToExternal(); vraci uspech (neni-li
    // 'bufferSize'==0, povazuje se za neuspech i to, kdyz se cesta nevejde do bufferu
    // 'buffer')
    // omezeni: hlavni thread
    virtual BOOL WINAPI GetPanelPath(int panel, char* buffer, int bufferSize, int* type,
                                     char** archiveOrFS, BOOL convertFSPathToExternal = FALSE) = 0;

    // vraci posledni navstivenou windowsovou cestu v panelu, hodi se pro navraty z FS (prijemnejsi nez
    // jit hned na fixed-drive); 'panel' je jeden z PANEL_XXX; 'buffer' je buffer pro cestu;
    // 'bufferSize' je velikost bufferu 'buffer'; vraci uspech
    // omezeni: hlavni thread
    virtual BOOL WINAPI GetLastWindowsPanelPath(int panel, char* buffer, int bufferSize) = 0;

    // vraci jmeno FS pridelene "dozivotne" pluginu Salamanderem (podle navrhu z SetBasicPluginData);
    // 'buf' je buffer o velikosti alespon MAX_PATH znaku; 'fsNameIndex' je index fs-name (index je
    // nula pro fs-name zadane v CSalamanderPluginEntryAbstract::SetBasicPluginData, u ostatnich
    // fs-name index vraci CSalamanderPluginEntryAbstract::AddFSName)
    // omezeni: hlavni thread (jinak muze dojit ke zmenam v konfiguraci pluginu behem volani),
    // v entry-pointu lze volat az po SetBasicPluginData, drive nemusi byt zname
    virtual void WINAPI GetPluginFSName(char* buf, int fsNameIndex) = 0;

    // vraci interface pluginoveho file-systemu (FS) otevreneho v panelu 'panel' (jeden z PANEL_XXX);
    // pokud v panelu neni otevren FS nebo jde o FS jineho pluginu (nepatri volajicimu pluginu), vraci
    // metoda NULL (nelze pracovat s objektem jineho pluginu, jeho struktura je neznama)
    // omezeni: hlavni thread
    virtual CPluginFSInterfaceAbstract* WINAPI GetPanelPluginFS(int panel) = 0;

    // vraci plugin data interface listingu panelu (muze byt i NULL), 'panel' je jeden z PANEL_XXX;
    // pokud plugin data interface existuje, ale nepatri tomuto (volajicimu) pluginu, vraci
    // metoda NULL (nelze pracovat s objektem jineho pluginu, jeho struktura je neznama)
    // omezeni: hlavni thread
    virtual CPluginDataInterfaceAbstract* WINAPI GetPanelPluginData(int panel) = 0;

    // vraci fokusenou polozku panelu (soubor/adresar/updir("..")), 'panel' je jeden z PANEL_XXX,
    // vraci NULL (zadna polozka v panelu) nebo data fokusene polozky; neni-li 'isDir' NULL,
    // vraci v nem FALSE pokud jde o soubor (jinak jde o adresar nebo updir)
    // POZOR: vracena data polozky jsou pouze pro cteni
    // omezeni: hlavni thread
    virtual const CFileData* WINAPI GetPanelFocusedItem(int panel, BOOL* isDir) = 0;

    // vraci postupne polozky panelu (nejprve adresare, pak soubory), 'panel' je jeden z PANEL_XXX,
    // 'index' je vstupne/vystupni promenna, ukazuje na int, ve kterem je pri prvnim volani 0,
    // hodnotu pro dalsi volani si funkce ulozi pri navratu (pouziti: na zacatku vynulovat, pak
    // nemenit), vraci NULL (jiz zadna polozka) nebo data dalsi (pripadne prvni) polozky;
    // neni-li 'isDir' NULL, vraci v nem FALSE pokud jde o soubor (jinak jde o adresar nebo updir)
    // POZOR: vracena data polozky jsou pouze pro cteni
    // omezeni: hlavni thread
    virtual const CFileData* WINAPI GetPanelItem(int panel, int* index, BOOL* isDir) = 0;

    // vraci postupne oznacene polozky panelu (nejprve adresare, pak soubory), 'panel' je jeden z
    // PANEL_XXX, 'index' je vstupne/vystupni promenna, ukazuje na int, ve kterem je pri prvnim volani 0,
    // hodnotu pro dalsi volani si funkce ulozi pri navratu (pouziti: na zacatku vynulovat, pak
    // nemenit), vraci NULL (jiz zadna polozka) nebo data dalsi (pripadne prvni) polozky;
    // neni-li 'isDir' NULL, vraci v nem FALSE pokud jde o soubor (jinak jde o adresar nebo updir)
    // POZOR: vracena data polozky jsou pouze pro cteni
    // omezeni: hlavni thread
    virtual const CFileData* WINAPI GetPanelSelectedItem(int panel, int* index, BOOL* isDir) = 0;

    // zjisti kolik souboru a adresaru je oznacenych v panelu; 'panel' je jeden z PANEL_XXX;
    // neni-li 'selectedFiles' NULL, vraci v nem pocet oznacenych souboru; neni-li 'selectedDirs'
    // NULL, vraci v nem pocet oznacenych adresaru; vraci TRUE pokud je oznaceny aspon jeden
    // soubor nebo adresar nebo pokud je fokus na souboru nebo adresari (je-li s cim
    // pracovat - fokus neni na up-diru)
    // omezeni: hlavni thread (jinak se muze obsah panelu menit)
    virtual BOOL WINAPI GetPanelSelection(int panel, int* selectedFiles, int* selectedDirs) = 0;

    // vraci top-index listboxu v panelu; 'panel' je jeden z PANEL_XXX
    // omezeni: hlavni thread (jinak se muze obsah panelu menit)
    virtual int WINAPI GetPanelTopIndex(int panel) = 0;

    // informuje hlavni okno Salamandera, ze se deaktivuje okno viewru, pokud se bude
    // bezprostredne aktivovat hlavni okno a v panelech budou neautomaticky refreshovane
    // disky nedojde k jejich refreshi (viewry nemeni obsah disku), nepovine (dojde
    // k mozna zbytecnemu refrehi)
    // mozne volat z libovolneho threadu
    virtual void WINAPI SkipOneActivateRefresh() = 0;

    // oznacuje/odznacuje polozku panelu, 'file' je ukazatel na menenou polozku ziskany predchozim
    // "get-item" volanim (metody GetPanelFocusedItem, GetPanelItem a GetPanelSelectedItem);
    // je nutne, aby od "get-item" volani nebyl opusten plugin a volani probehlo v hlavnim
    // threadu (aby nedoslo k obnove panelu - zneplatneni ukazatele); 'panel' musi byt shodny
    // s parametrem 'panel' prislusneho "get-item" volani; je-li 'select' TRUE dojde k oznaceni,
    // jinak dojde k odznaceni; po poslednim volani je nutne pouzit RepaintChangedItems('panel') pro
    // prekresleni panelu
    // omezeni: hlavni thread
    virtual void WINAPI SelectPanelItem(int panel, const CFileData* file, BOOL select) = 0;

    // provede prekresleni polozek panelu u kterych doslo ke zmenam (oznaceni); 'panel' je
    // jeden z PANEL_XXX
    // omezeni: hlavni thread
    virtual void WINAPI RepaintChangedItems(int panel) = 0;

    // oznacuje/odznacuje vsechny polozky v panelu, 'panel' je jeden z PANEL_XXX; je-li 'select'
    // TRUE dojde k oznaceni, jinak dojde k odznaceni; je-li 'repaint' TRUE prekresli se vsechny
    // zmenene polozky v panelu, jinak k prekresleni nedojde (mozne volat pozdeji RepaintChangedItems)
    // omezeni: hlavni thread
    virtual void WINAPI SelectAllPanelItems(int panel, BOOL select, BOOL repaint) = 0;

    // nastavi fokus v panelu, 'file' je ukazatel na fokusenou polozku ziskany predchozim
    // "get-item" volanim (metody GetPanelFocusedItem, GetPanelItem a GetPanelSelectedItem);
    // je nutne, aby od "get-item" volani nebyl opusten plugin a volani probehlo v hlavnim
    // threadu (aby nedoslo k obnove panelu - zneplatneni ukazatele); 'panel' musi byt shodny
    // s parametrem 'panel' prislusneho "get-item" volani; je-li 'partVis' TRUE a polozka bude
    // viditelna jen castecne, nedojde k odrolovani panelu pri fokusu, pri FALSE odroluje panel
    // tak, aby byla videt cela polozka
    // omezeni: hlavni thread
    virtual void WINAPI SetPanelFocusedItem(int panel, const CFileData* file, BOOL partVis) = 0;

    // zjisti jestli se v panelu pouziva filtr a pokud se pouziva, ziska retezec s maskami
    // tohoto filtru; 'panel' oznacuje panel, o ktery se zajimame (jedna z PANEL_XXX);
    // 'masks' je buffer pro masky filtru o velikosti minimalne 'masksBufSize' bytu (doporucena
    // velikost je MAX_GROUPMASK); vraci TRUE pokud se filtr pouziva a buffer 'masks' je
    // dost velky; vraci FALSE pokud se filtr nepouziva nebo se retezec masek nevesel
    // do 'masks'
    // omezeni: hlavni thread
    virtual BOOL WINAPI GetFilterFromPanel(int panel, char* masks, int masksBufSize) = 0;

    // vraci pozici zdrojoveho panelu (je vlevo nebo vpravo?), vraci PANEL_LEFT nebo PANEL_RIGHT
    // omezeni: hlavni thread
    virtual int WINAPI GetSourcePanel() = 0;

    // zjistuje, ve kterem panelu je otevreny 'pluginFS'; pokud neni ani v jednom panelu,
    // vraci FALSE; pokud vrati TRUE, je cislo panelu v 'panel' (PANEL_LEFT nebo PANEL_RIGHT)
    // omezeni: hlavni thread (jinak se muze obsah panelu menit)
    virtual BOOL WINAPI GetPanelWithPluginFS(CPluginFSInterfaceAbstract* pluginFS, int& panel) = 0;

    // aktivuje druhy panel (ala klavesa TAB); panely oznacene pres PANEL_SOURCE a PANEL_TARGET
    // se tim prirozene prohazuji
    // omezeni: hlavni thread
    virtual void WINAPI ChangePanel() = 0;

    // prevod cisla na "prehlednejsi" retezec (po trech cislicich mezera), retezec vraci v
    // 'buffer' (min. velikost 50 bytu), vraci 'buffer'
    // mozne volat z libovolneho threadu
    virtual char* WINAPI NumberToStr(char* buffer, const CQuadWord& number) = 0;

    // tisk velikosti mista na disku do 'buf' (min. velikost bufferu je 100 bytu),
    // mode==0 "1.23 MB", mode==1 "1 230 000 bytes, 1.23 MB", mode==2 "1 230 000 bytes",
    // mode==3 "12 KB" (vzdy v celych kilo bajtech), mode==4 (jako mode==0, ale vzdy
    // aspon 3 platne cislice, napr. "2.00 MB")
    // vraci 'buf'
    // mozne volat z libovolneho threadu
    virtual char* WINAPI PrintDiskSize(char* buf, const CQuadWord& size, int mode) = 0;

    // prevadi pocet sekund na retezec ("5 sec", "1 hr 34 min", atp.); 'buf' je
    // buffer pro vysledny text, musi byt velky aspon 100 znaku; 'secs' je pocet sekund;
    // vraci 'buf'
    // mozne volat z libovolneho threadu
    virtual char* WINAPI PrintTimeLeft(char* buf, const CQuadWord& secs) = 0;

    // porovna root normalni (c:\path) i UNC (\\server\share\path) cesty, vraci TRUE pokud je root shodny
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI HasTheSameRootPath(const char* path1, const char* path2) = 0;

    // Vrati pocet znaku spolecne cesty. Na normalni ceste musi byt root ukonceny zpetnym
    // lomitkem, jinak funkce vrati 0. ("C:\"+"C:"->0, "C:\A\B"+"C:\"->3, "C:\A\B\"+"C:\A"->4,
    // "C:\AA\BB"+"C:\AA\CC"->5)
    // Pracuje pro normalni i UNC cesty.
    virtual int WINAPI CommonPrefixLength(const char* path1, const char* path2) = 0;

    // Vraci TRUE, pokud je cesta 'prefix' zakladem cesty 'path'. Jinak vraci FALSE.
    // "C:\aa","C:\Aa\BB"->TRUE
    // "C:\aa","C:\aaa"->FALSE
    // "C:\aa\","C:\Aa"->TRUE
    // "\\server\share","\\server\share\aaa"->TRUE
    // Pracuje pro normalni i UNC cesty.
    virtual BOOL WINAPI PathIsPrefix(const char* prefix, const char* path) = 0;

    // porovna dve normalni (c:\path) i UNC (\\server\share\path) cesty, ignoruje mala/velka pismena,
    // ignoruje take jeden backslash na zacatku a konci cest, vraci TRUE pokud jsou cesty stejne
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI IsTheSamePath(const char* path1, const char* path2) = 0;

    // ziska root cestu z normalni (c:\path) i UNC (\\server\share\path) cesty 'path', v 'root' vraci
    // cestu ve formatu 'c:\' nebo '\\server\share\' (min. velikost 'root' bufferu je MAX_PATH),
    // vraci pocet znaku root cesty (bez null-terminatoru); pri UNC root ceste delsi nez MAX_PATH
    // dojde k orezu na MAX_PATH-2 znaku a doplneni backslashe (stejne to na 100% neni root cesta)
    // mozne volat z libovolneho threadu
    virtual int WINAPI GetRootPath(char* root, const char* path) = 0;

    // zkracuje normalni (c:\path) i UNC (\\server\share\path) cestu o posledni adresar
    // (zariznuti na poslednim backslashi - v oriznute ceste zustava na konci backslash
    // jen u 'c:\'), 'path' je in/out buffer (min. velikost strlen(path)+2 bytu),
    // v 'cutDir' (neni-li NULL) se vraci ukazatel (do bufferu 'path' za 1. null-terminator)
    // na posledni adresar (odriznutou cast), tato metoda nahrazuje PathRemoveFileSpec,
    // vraci TRUE pokud doslo ke zkraceni (neslo o root cestu)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI CutDirectory(char* path, char** cutDir = NULL) = 0;

    // pracuje s normalnimi (c:\path) i UNC (\\server\share\path) cestami,
    // spoji 'path' a 'name' do 'path', zajisti spojeni backslashem, 'path' je buffer alespon
    // 'pathSize' znaku, vraci TRUE pokud se 'name' veslo za 'path'; je-li 'path' nebo 'name'
    // prazdne, spojovaci (pocatecni/ukoncovaci) backslash nebude (napr. "c:\" + "" -> "c:")
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalPathAppend(char* path, const char* name, int pathSize) = 0;

    // pracuje s normalnimi (c:\path) i UNC (\\server\share\path) cestami,
    // pokud jeste 'path' nekonci na backslash, prida ho na konec 'path'; 'path' je buffer
    // alespon 'pathSize' znaku; vraci TRUE pokud se backslash vesel za 'path'; je-li 'path'
    // prazdne, backslash se neprida
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalPathAddBackslash(char* path, int pathSize) = 0;

    // pracuje s normalnimi (c:\path) i UNC (\\server\share\path) cestami,
    // pokud je v 'path' na konci backslash, odstrani ho
    // mozne volat z libovolneho threadu
    virtual void WINAPI SalPathRemoveBackslash(char* path) = 0;

    // pracuje s normalnimi (c:\path) i UNC (\\server\share\path) cestami,
    // z plneho jmena udela jmeno ("c:\path\file" -> "file")
    // mozne volat z libovolneho threadu
    virtual void WINAPI SalPathStripPath(char* path) = 0;

    // pracuje s normalnimi (c:\path) i UNC (\\server\share\path) cestami,
    // pokud je ve jmene pripona, odstrani ji
    // mozne volat z libovolneho threadu
    virtual void WINAPI SalPathRemoveExtension(char* path) = 0;

    // pracuje s normalnimi (c:\path) i UNC (\\server\share\path) cestami,
    // pokud ve jmenu 'path' jeste neni pripona, prida priponu 'extension' (napr. ".txt"),
    // 'path' je buffer alespon 'pathSize' znaku, vraci FALSE pokud buffer 'path' nestaci
    // pro vyslednou cestu
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalPathAddExtension(char* path, const char* extension, int pathSize) = 0;

    // pracuje s normalnimi (c:\path) i UNC (\\server\share\path) cestami,
    // zmeni/prida priponu 'extension' (napr. ".txt") ve jmenu 'path', 'path' je buffer
    // alespon 'pathSize' znaku, vraci FALSE pokud buffer 'path' nestaci pro vyslednou cestu
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalPathRenameExtension(char* path, const char* extension, int pathSize) = 0;

    // pracuje s normalnimi (c:\path) i UNC (\\server\share\path) cestami,
    // vraci ukazatel do 'path' na jmeno souboru/adresare (backslash na konci 'path' ignoruje),
    // pokud jmeno neobsahuje jine backslashe nez na konci retezce, vraci 'path'
    // mozne volat z libovolneho threadu
    virtual const char* WINAPI SalPathFindFileName(const char* path) = 0;

    // upravuje relativni nebo absolutni normalni (c:\path) i UNC (\\server\share\path) cestu
    // na absolutni bez '.', '..' a koncoveho backslashe (krom typu "c:\"); je-li 'curDir' NULL,
    // relativni cesty typu "\path" a "path" vraci chybu (neurcitelne), jinak je 'curDir' platna
    // upravena aktualni cesta (UNC i normalni); aktualni cesty ostatnich disku (mimo
    // 'curDir' + jen normalni, ne UNC) jsou v Salamandrovskem poli DefaultDir (pred pouzitim
    // je dobre zavolat metodu SalUpdateDefaultDir); 'name' - in/out buffer cesty alespon 'nameBufSize'
    // znaku; neni-li 'nextFocus' NULL a zadana relativni cesta neobsahuje backslash, provede se
    // strcpy(nextFocus, name); vraci TRUE - jmeno 'name' je pripraveno k pouziti, jinak neni-li
    // 'errTextID' NULL obsahuje chybu (konstanty GFN_XXX - text se da ziskat pres GetGFNErrorText)
    // POZOR: pred pouzitim je dobre zavolat metodu SalUpdateDefaultDir
    // omezeni: hlavni thread (jinak muze dojit ke zmenam DefaultDir v hl. threadu)
    virtual BOOL WINAPI SalGetFullName(char* name, int* errTextID = NULL, const char* curDir = NULL,
                                       char* nextFocus = NULL, int nameBufSize = MAX_PATH) = 0;

    // obnovi Salamandrovske pole DefaultDir podle cest v panelech, je-li 'activePrefered' TRUE,
    // bude mit prednost cesta v aktivnim panelu (zapise se pozdeji do DefaultDir), jinak ma
    // prednost cesta v neaktivnim panelu
    // omezeni: hlavni thread (jinak muze dojit ke zmenam DefaultDir v hl. threadu)
    virtual void WINAPI SalUpdateDefaultDir(BOOL activePrefered) = 0;

    // vraci textovou reprezentaci chybove konstanty GFN_XXX; vraci 'buf' (aby slo dat GetGFNErrorText
    // primo jako parametr funkce)
    // mozne volat z libovolneho threadu
    virtual char* WINAPI GetGFNErrorText(int GFN, char* buf, int bufSize) = 0;

    // vraci textovou reprezentaci systemove chyby (ERROR_XXX) v bufferu 'buf' o velikosti 'bufSize';
    // vraci 'buf' (aby slo dat GetErrorText primo jako parametr funkce); 'buf' muze byt NULL nebo
    // 'bufSize' 0, v tom pripade vraci text v internim bufferu (hrozi zmena textu diky zmene
    // interniho bufferu zpusobene dalsimi volanimi GetErrorText i z jinych pluginu nebo Salamandera;
    // buffer je dimenzovany na minimalne 10 textu, pak teprve hrozi prepis; pokud potrebujete text
    // pouzit az pozdeji, doporucujeme jej zkopirovat do lokalniho bufferu o velikosti MAX_PATH + 20)
    // mozne volat z libovolneho threadu
    virtual char* WINAPI GetErrorText(int err, char* buf = NULL, int bufSize = 0) = 0;

    // vraci vnitrni barvu Salamandera, 'color' je konstanta barvy (viz SALCOL_XXX)
    // mozne volat z libovolneho threadu
    virtual COLORREF WINAPI GetCurrentColor(int color) = 0;

    // zajisti aktivaci hlavniho okna Salamandera + focus souboru/adresare 'name' na ceste
    // 'path' v panelu 'panel'; pripadne zmeni cestu v panelu (je-li nutne); 'panel' je jeden
    // z PANEL_XXX; 'path' je libovolna cesta (windowsova (diskova), na FS nebo do archivu);
    // 'name' muze byt i prazdny string, pokud se nema nic focusit;
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract a CPluginDataInterfaceAbstract
    // (hrozi napr. zavreni FS otevreneho v panelu - metode by mohl prestat existovat 'this')
    virtual void WINAPI FocusNameInPanel(int panel, const char* path, const char* name) = 0;

    // zmena cesty v panelu - vstupem muze byt absolutni i relativni UNC (\\server\share\path)
    // i normalni (c:\path) cesta a to jak windowsova (diskova), tak do archivu nebo cesta
    // na FS (absolutni/relativni si resi primo plugin); pokud je vstupem cesta k souboru,
    // dojde k fokusu tohoto souboru; neni-li suggestedTopIndex -1, bude nastaven top-index
    // v panelu; neni-li suggestedFocusName NULL, zkusi se nalezt (ignore-case) a vyfokusit
    // polozka stejneho jmena; neni-li 'failReason' NULL, nastavuje se na jednu z konstant
    // CHPPFR_XXX (informuje o vysledku metody); je-li 'convertFSPathToInternal' TRUE a jde
    // o cestu na FS, najde se plugin jehoz cesta je (podle fs-name) a zavola se jeho
    // CPluginInterfaceForFSAbstract::ConvertPathToInternal(); vraci TRUE pokud se podarilo
    // vylistovat pozadovanou cestu;
    // POZNAMKA: pri zadani cesty na FS dojde k pokusu o otevreni cesty v tomto poradi: ve FS
    // v panelu, v odpojenem FS, nebo v novem FS (u FS z panelu a odpojenych FS se zjistuje
    // jestli odpovida plugin-fs-name a jestli metoda FS IsOurPath vraci pro zadanou cestu TRUE);
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract a CPluginDataInterfaceAbstract
    // (hrozi napr. zavreni FS otevreneho v panelu - metode by mohl prestat existovat 'this')
    virtual BOOL WINAPI ChangePanelPath(int panel, const char* path, int* failReason = NULL,
                                        int suggestedTopIndex = -1,
                                        const char* suggestedFocusName = NULL,
                                        BOOL convertFSPathToInternal = TRUE) = 0;

    // zmena cesty v panelu na relativni nebo absolutni UNC (\\server\share\path) nebo normalni (c:\path)
    // cestu, pokud neni nova cesta dostupna, zkousi uspet se zkracenymi cestami; pokud jde o zmenu
    // cesty v ramci jednoho disku (vcetne archivu na tomto disku) a na disku se nepodari nalezt
    // pristupnou cestu, zmeni cestu na root prvniho lokalniho fixed drivu (velka sance uspechu,
    // panel nezustane prazdny); pri prekladu relativni cesty na absolutni je uprednostnovana cesta
    // v panelu 'panel' (jen je-li to cesta na disk (i k archivu), jinak se nepouziva); 'panel' je
    // jeden z PANEL_XXX; 'path' je nova cesta; neni-li 'suggestedTopIndex' -1, bude nastaven jako
    // top-index v panelu (jen pro novou cestu, na zkracene (zmenene) ceste se nenastavuje); neni-li
    // 'suggestedFocusName' NULL, zkusi se nalezt (ignore-case) a vyfokusit polozka stejneho jmena
    // (jen pro novou cestu, na zkracene (zmenene) ceste se neprovadi); neni-li 'failReason' NULL,
    // nastavuje se na jednu z konstant CHPPFR_XXX (informuje o vysledku metody); vraci TRUE pokud
    // se podarilo vylistovat pozadovanou cestu (nezkracenou/nezmenenou)
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract a CPluginDataInterfaceAbstract
    // (hrozi napr. zavreni FS otevreneho v panelu - metode by mohl prestat existovat 'this')
    virtual BOOL WINAPI ChangePanelPathToDisk(int panel, const char* path, int* failReason = NULL,
                                              int suggestedTopIndex = -1,
                                              const char* suggestedFocusName = NULL) = 0;

    // zmena cesty v panelu do archivu, 'archiv' je relativni nebo absolutni UNC
    // (\\server\share\path\file) nebo normalni (c:\path\file) jmeno archivu, 'archivePath' je cesta
    // uvnitr archivu, pokud neni nova cesta v archivu dostupna, zkousi uspet se zkracenymi cestami;
    // pri prekladu relativni cesty na absolutni je uprednostnovana cesta v panelu 'panel'
    // (jen je-li to cesta na disk (i k archivu), jinak se nepouziva); 'panel' je jeden z PANEL_XXX;
    // neni-li 'suggestedTopIndex' -1, bude nastaven jako top-index v panelu (jen pro novou
    // cestu, na zkracene (zmenene) ceste se nenastavuje); neni-li 'suggestedFocusName' NULL,
    // zkusi se nalezt (ignore-case) a vyfokusit polozka stejneho jmena (jen pro novou cestu,
    // na zkracene (zmenene) ceste se neprovadi); je-li 'forceUpdate' TRUE a provadi se zmena cesty
    // uvnitr archivu 'archive' (archiv uz je v panelu otevreny), provadi se test zmeny souboru
    // archivu (kontrola size & time) a v pripade zmeny se archiv zavre (hrozi update editovanych
    // souboru) a znovu vylistuje nebo pokud soubor prestal existovat, provede se zmena cesty na disk
    // (zavreni archivu; pokud cesta na disk neni pristupna, zmeni cestu na root prvniho lokalniho
    // fixed drivu); je-li 'forceUpdate' FALSE, provadi se zmeny cesty uvnitr archivu 'archive' bez
    // kontroly souboru archivu; neni-li 'failReason' NULL, nastavuje se na jednu z konstant
    // CHPPFR_XXX (informuje o vysledku metody); vraci TRUE pokud se podarilo vylistovat
    // pozadovanou cestu (nezkracenou/nezmenenou)
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract a CPluginDataInterfaceAbstract
    // (hrozi napr. zavreni FS otevreneho v panelu - metode by mohl prestat existovat 'this')
    virtual BOOL WINAPI ChangePanelPathToArchive(int panel, const char* archive, const char* archivePath,
                                                 int* failReason = NULL, int suggestedTopIndex = -1,
                                                 const char* suggestedFocusName = NULL,
                                                 BOOL forceUpdate = FALSE) = 0;

    // zmena cesty v panelu do pluginoveho FS, 'fsName' je jmeno FS (viz GetPluginFSName; nemusi
    // byt nutne z tohoto pluginu), 'fsUserPart' je cesta v ramci FS; pokud neni nova cesta v FS
    // dostupna, zkousi uspet se zkracenymi cestami (opakovane volani ChangePath a ListCurrentPath,
    // viz CPluginFSInterfaceAbstract); pokud jde o zmenu cesty v ramci aktualniho FS (viz
    // CPluginFSInterfaceAbstract::IsOurPath) a nepodari se od nove cesty nalezt pristupnou cestu,
    // zkusi najit pristupnou cestu jeste od puvodni (aktualni) cesty, a pokud zklame i ta,
    // zmeni cestu na root prvniho lokalniho fixed drivu (velka sance uspechu, panel nezustane prazdny);
    // 'panel' je jeden z PANEL_XXX; neni-li 'suggestedTopIndex' -1, bude nastaven jako top-index
    // v panelu (jen pro novou cestu, na zkracene (zmenene) ceste se nenastavuje); neni-li
    // 'suggestedFocusName' NULL, zkusi se nalezt (ignore-case) a vyfokusit polozka stejneho jmena
    // (jen pro novou cestu, na zkracene (zmenene) ceste se neprovadi); je-li 'forceUpdate' TRUE,
    // neoptimalizuje se (cesta se normalne vylistuje) pripad zmeny cesty na aktualni cestu v panelu
    // (bud nova cesta odpovida aktualni ceste rovnou nebo na ni byla hned prvnim ChangePath
    // zkracena); je-li 'convertPathToInternal' TRUE, najde se podle 'fsName' plugin a zavola se
    // jeho metoda CPluginInterfaceForFSAbstract::ConvertPathToInternal() pro 'fsUserPart';
    // neni-li 'failReason' NULL, nastavuje se na jednu z konstant CHPPFR_XXX (informuje
    // o vysledku metody); vraci TRUE pokud se podarilo vylistovat pozadovanou cestu
    // (nezkracenou/nezmenenou)
    // POZNAMKA: pokud potrebujete, aby se FS cesta zkusila otevrit i v odpojenych FS, pouzijte metodu
    // ChangePanelPath (ChangePanelPathToPluginFS odpojene FS ignoruje - pracuje jen s FS otevrenym
    // v panelu nebo otevira nove FS);
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract a CPluginDataInterfaceAbstract
    // (hrozi napr. zavreni FS otevreneho v panelu - metode by mohl prestat existovat 'this')
    virtual BOOL WINAPI ChangePanelPathToPluginFS(int panel, const char* fsName, const char* fsUserPart,
                                                  int* failReason = NULL, int suggestedTopIndex = -1,
                                                  const char* suggestedFocusName = NULL,
                                                  BOOL forceUpdate = FALSE,
                                                  BOOL convertPathToInternal = FALSE) = 0;

    // zmena cesty v panelu do odpojeneho pluginoveho FS (viz FSE_DETACHED/FSE_ATTACHED),
    // 'detachedFS' je odpojeny pluginovy FS; pokud neni aktualni cesta v odpojenem FS dostupna,
    // zkousi uspet se zkracenymi cestami (opakovane volani ChangePath a ListCurrentPath, viz
    // CPluginFSInterfaceAbstract); 'panel' je jeden z PANEL_XXX; neni-li 'suggestedTopIndex' -1,
    // bude nastaven jako top-index v panelu (jen pro novou cestu, na zkracene (zmenene) ceste
    // se nenastavuje); neni-li 'suggestedFocusName' NULL, zkusi se nalezt (ignore-case) a vyfokusit
    // polozka stejneho jmena (jen pro novou cestu, na zkracene (zmenene) ceste se neprovadi);
    // neni-li 'failReason' NULL, nastavuje se na jednu z konstant CHPPFR_XXX (informuje o vysledku
    // metody); vraci TRUE pokud se podarilo vylistovat pozadovanou cestu (nezkracenou/nezmenenou)
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract a CPluginDataInterfaceAbstract
    // (hrozi napr. zavreni FS otevreneho v panelu - metode by mohl prestat existovat 'this')
    virtual BOOL WINAPI ChangePanelPathToDetachedFS(int panel, CPluginFSInterfaceAbstract* detachedFS,
                                                    int* failReason = NULL, int suggestedTopIndex = -1,
                                                    const char* suggestedFocusName = NULL) = 0;

    // zmena cesty v panelu na root prvniho lokalniho fixed drivu, jde o temer jistou zmenu
    // aktualni cesty v panelu; 'panel' je jeden z PANEL_XXX; neni-li 'failReason' NULL,
    // nastavuje se na jednu z konstant CHPPFR_XXX (informuje o vysledku metody); vraci
    // TRUE pokud se podarilo vylistovat root prvniho lokalniho fixed drivu
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract a CPluginDataInterfaceAbstract
    // (hrozi napr. zavreni FS otevreneho v panelu - metode by mohl prestat existovat 'this')
    virtual BOOL WINAPI ChangePanelPathToFixedDrive(int panel, int* failReason = NULL) = 0;

    // provede refresh cesty v panelu (znovu nacte listing a prenese oznaceni, ikony, fokus, atd.
    // do noveho obsahu panelu); diskove a FS cesty se vzdycky nacitaji znovu, cesty do archivu
    // se nacitaji jen pokud doslo ke zmene souboru archivu (kontrola size & time) nebo pokud
    // je 'forceRefresh' TRUE; thumbnaily na diskovych cestach se nacitaji znovu jen pri zmene
    // velikosti souboru nebo zmene datumu/casu posledniho zapisu do souboru nebo pokud je
    // 'forceRefresh' TRUE; 'panel' je jeden z PANEL_XXX; je-li 'focusFirstNewItem' TRUE a
    // v panelu pribyla jen jedina polozka, dojde k fokusu teto nove polozky (pouziva se napr.
    // pro fokus nove vytvoreneho souboru/adresare)
    // omezeni: hlavni thread a navic jen mimo metody CPluginFSInterfaceAbstract a
    // CPluginDataInterfaceAbstract (hrozi napr. zavreni FS otevreneho v panelu - metode by
    // mohl prestat existovat 'this')
    virtual void WINAPI RefreshPanelPath(int panel, BOOL forceRefresh = FALSE,
                                         BOOL focusFirstNewItem = FALSE) = 0;

    // postne panelu zpravu o tom, ze by se mel provest refresh cesty (znovu nacte listing a
    // prenese oznaceni, ikony, fokus, atd. do noveho obsahu panelu); refresh se provede az
    // dojde k aktivaci hlavniho okna Salamandera (az skonci suspend-mode); diskove
    // a FS cesty se vzdycky nacitaji znovu, cesty do archivu se nacitaji jen pokud doslo ke
    // zmene souboru archivu (kontrola size & time); 'panel' je jeden z PANEL_XXX; je-li
    // 'focusFirstNewItem' TRUE a v panelu pribyla jen jedina polozka, dojde k fokusu teto
    // nove polozky (pouziva se napr. pro fokus nove vytvoreneho souboru/adresare)
    // mozne volat z libovolneho threadu (pokud hlavni thread nespousti kod uvnitr pluginu,
    // probehne refresh co nejdrive, jinak refresh pocka minimalne do okamziku, kdy hlavni
    // thread opusti plugin)
    virtual void WINAPI PostRefreshPanelPath(int panel, BOOL focusFirstNewItem = FALSE) = 0;

    // postne panelu s aktivnim FS 'modifiedFS' zpravu o tom, ze by se mel
    // provest refresh cesty (znovu nacte listing a prenese oznaceni, ikony, fokus, atd. do
    // noveho obsahu panelu); refresh se provede az dojde k aktivaci hlavniho okna Salamandera
    // (az skonci suspend-mode); FS cesta se vzdycky nacte znovu; pokud 'modifiedFS' neni v zadnem
    // panelu, neprovede se nic; je-li 'focusFirstNewItem' TRUE a v panelu pribyla jen jedina
    // polozka, dojde k fokusu teto nove polozky (pouziva se napr. pro fokus nove vytvoreneho
    // souboru/adresare);
    // POZNAMKA: existuje jeste PostRefreshPanelFS2, ktera vraci TRUE pokud se provedl refresh,
    // FALSE pokud nebyl 'modifiedFS' nalezen ani v jednom panelu;
    // mozne volat z libovolneho threadu (pokud hlavni thread nespousti kod uvnitr pluginu,
    // probehne refresh co nejdrive, jinak refresh pocka minimalne do okamziku, kdy hlavni
    // thread opusti plugin)
    virtual void WINAPI PostRefreshPanelFS(CPluginFSInterfaceAbstract* modifiedFS,
                                           BOOL focusFirstNewItem = FALSE) = 0;

    // zavre odpojeny pluginovy FS (pokud se necha, viz CPluginFSInterfaceAbstract::TryCloseOrDetach),
    // 'detachedFS' je odpojeny pluginovy FS; vraci TRUE pri uspechu (FALSE znamena, ze se odpojeny
    // pluginovy FS nezavrel); 'parent' je parent pripadnych messageboxu (zatim je muze otevrit jen
    // CPluginFSInterfaceAbstract::ReleaseObject)
    // Poznamka: plugin FS otevreny v panelu se zavre napr. pomoci ChangePanelPathToRescuePathOrFixedDrive
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract (snazime se o zavreni
    // odpojeneho FS - metode by mohl prestat existovat 'this')
    virtual BOOL WINAPI CloseDetachedFS(HWND parent, CPluginFSInterfaceAbstract* detachedFS) = 0;

    // zdvojuje '&' - hodi se pro cesty zobrazovane v menu ('&&' se zobrazi jako '&');
    // 'buffer' je vstupne/vystupni retezec, 'bufferSize' je velikost 'buffer' v bytech;
    // vraci TRUE pokud zdvojenim nedoslo ke ztrate znaku z konce retezce (buffer byl dost
    // veliky)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI DuplicateAmpersands(char* buffer, int bufferSize) = 0;

    // odstrani '&' z textu; najde-li dvojici "&&", nahradi ji jednim znakem '&'
    // mozne volat z libovolneho threadu
    virtual void WINAPI RemoveAmpersands(char* text) = 0;

    // ValidateVarString a ExpandVarString:
    // metody pro overovani a expanzi retezcu s promennymi ve tvaru "$(var_name)", "$(var_name:num)"
    // (num je sirka promenne, jde o ciselnou hodnotu od 1 do 9999), "$(var_name:max)" ("max" je
    // symbol, ktery oznacuje, ze sirka promenne se ridi hodnotou v poli 'maxVarWidths', podrobnosti
    // u ExpandVarString) a "$[env_var]" (expanduje hodnotu promenne prostredi); pouziva se pokud si
    // uzivatel muze zadat format retezce (jako napr. v info-line) priklad retezce s promennymi:
    // "$(files) files and $(dirs) directories" - promenne 'files' a 'dirs';
    // zdrojovy kod pro pouziti v info-line (bez promennych ve tvaru "$(varname:max)") je v DEMOPLUG
    //
    // kontroluje syntaxi 'varText' (retezce s promennymi), vraci FALSE pokud najde chybu, jeji
    // pozici umisti do 'errorPos1' (offset zacatku chybne casti) a 'errorPos2' (offset konce chybne
    // casti); 'variables' je pole struktur CSalamanderVarStrEntry, ktere je ukonceno strukturou s
    // Name==NULL; 'msgParent' je parent message-boxu s chybami, je-li NULL, chyby se nevypisuji
    virtual BOOL WINAPI ValidateVarString(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2,
                                          const CSalamanderVarStrEntry* variables) = 0;
    //
    // naplni 'buffer' vysledkem expanze 'varText' (retezce s promennymi), vraci FALSE pokud je
    // 'buffer' maly (predpoklada overeni retezce s promennymi pres ValidateVarString, jinak
    // vraci FALSE i pri chybe syntaxe) nebo uzivatel kliknul na Cancel pri chybe environment-variable
    // (nenalezena nebo prilis velka); 'bufferLen' je velikost bufferu 'buffer';
    // 'variables' je pole struktur CSalamanderVarStrEntry, ktere je ukonceno strukturou
    // s Name==NULL; 'param' je ukazatel, ktery se predava do CSalamanderVarStrEntry::Execute
    // pri expanzi nalezene promenne; 'msgParent' je parent message-boxu s chybami, je-li NULL,
    // chyby se nevypisuji; je-li 'ignoreEnvVarNotFoundOrTooLong' TRUE, ignoruji se chyby
    // environment-variable (nenalezena nebo prilis velka), je-li FALSE, zobrazi se messagebox
    // s chybou; neni-li 'varPlacements' NULL, ukazuje na pole DWORDu o '*varPlacementsCount' polozkach,
    // ktere bude naplneno DWORDy slozenymi vzdy z pozice promenne ve vystupnim bufferu (spodni WORD)
    // a poctu znaku promenne (horni WORD); neni-li 'varPlacementsCount' NULL, vraci se v nem pocet
    // naplnenych polozek v poli 'varPlacements' (jde vlastne o pocet promennych ve vstupnim
    // retezci);
    // pokud se tato metoda pouziva jen k expanzi retezce pro jednu hodnotu 'param', meli by
    // byt nastaveny 'detectMaxVarWidths' na FALSE, 'maxVarWidths' na NULL a 'maxVarWidthsCount'
    // na 0; pokud se ovsem pouziva tato metoda pro expanzi retezce opakovane pro urcitou
    // mnozinu hodnot 'param' (napr. u Make File List je to expanze radky postupne pro vsechny
    // oznacene soubory a adresare), ma smysl pouzivat i promenne ve tvaru "$(varname:max)",
    // u techto promennych se sirka urci jako nejvetsi sirka expandovane promenne v ramci cele
    // mnoziny hodnot; omereni nejvetsi sirky expandovane promenne se provadi v prvnim cyklu
    // (pro vsechny hodnoty mnoziny) volani ExpandVarString, v prvnim cyklu ma parametr
    // 'detectMaxVarWidths' hodnotu TRUE a pole 'maxVarWidths' o 'maxVarWidthsCount' polozkach
    // je predem nulovane (slouzi pro ulozeni maxim mezi jednotlivymi volanimi ExpandVarString);
    // samotna expanze pak probiha v druhem cyklu (pro vsechny hodnoty mnoziny) volani
    // ExpandVarString, v druhem cyklu ma parametr 'detectMaxVarWidths' hodnotu FALSE a pole
    // 'maxVarWidths' o 'maxVarWidthsCount' polozkach obsahuje predpocitane nejvetsi sirky
    // (z prvniho cyklu)
    virtual BOOL WINAPI ExpandVarString(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                                        const CSalamanderVarStrEntry* variables, void* param,
                                        BOOL ignoreEnvVarNotFoundOrTooLong = FALSE,
                                        DWORD* varPlacements = NULL, int* varPlacementsCount = NULL,
                                        BOOL detectMaxVarWidths = FALSE, int* maxVarWidths = NULL,
                                        int maxVarWidthsCount = 0) = 0;

    // nastavi flag load-on-salamander-start (loadit plugin pri startu Salamandera?) pluginu;
    // 'start' je nova hodnota flagu; vraci starou hodnotu flagu; pokud se nikdy nevolala
    // SetFlagLoadOnSalamanderStart, je flag nastaven na FALSE (plugin se neloadi pri startu, ale
    // az v pripade potreby)
    // omezeni: hlavni thread (jinak muze dojit ke zmenam v konfiguraci pluginu behem volani)
    virtual BOOL WINAPI SetFlagLoadOnSalamanderStart(BOOL start) = 0;

    // nastavi volajicimu pluginu priznak, ze se ma pri nejblizsi mozne prilezitosti unloadnout
    // (jakmile budou provedeny vsechny postnute prikazy menu (viz PostMenuExtCommand), nebudou
    // v message-queue hl. threadu zadne message a Salamander nebude "busy");
    // POZOR: pokud se vola z jineho nez hlavniho threadu, muze dojit k zadosti o unload (probiha
    // v hlavnim threadu) dokonce drive nez skonci PostUnloadThisPlugin (dalsi informace o
    // unloadu - viz CPluginInterfaceAbstract::Release)
    // mozne volat z libovolneho threadu (ale az po ukonceni entry-pointu pluginu, dokud se
    // spousti entry-point, je mozne metodu volat jen z hlavniho threadu)
    virtual void WINAPI PostUnloadThisPlugin() = 0;

    // vraci postupne moduly Salamandera (spustitelny soubor a .spl soubory instalovanych
    // pluginu, vse vcetne verzi); 'index' je vstupne/vystupni promenna, ukazuje na int,
    // ve kterem je pri prvnim volani 0, hodnotu pro dalsi volani si funkce ulozi pri navratu
    // (pouziti: na zacatku vynulovat, pak nemenit); 'module' je buffer pro jmeno modulu
    // (min. velikost MAX_PATH znaku); 'version' je buffer pro verzi modulu (min. velikost
    // MAX_PATH znaku); vraci FALSE pokud 'module' + 'version' neni naplnene a jiz neni
    // zadny dalsi modul, vraci TRUE pokud 'module' + 'version' obsahuje dalsi modul
    // omezeni: hlavni thread (jinak muze dojit ke zmenam v konfiguraci pluginu behem
    // volani - add/remove)
    virtual BOOL WINAPI EnumInstalledModules(int* index, char* module, char* version) = 0;

    // vola 'loadOrSaveFunc' pro load nebo save konfigurace; je-li 'load' TRUE, jde o load
    // konfigurace, pokud plugin podporuje "load/save configuration" a v dobe volani existuje
    // soukromy klic pluginu v registry, vola se 'loadOrSaveFunc' pro tento klic, jinak
    // se vola load defaultni konfigurace (parametr 'regKey' funkce 'loadOrSaveFunc' je NULL);
    // je-li 'load' FALSE, jde o save konfigurace, 'loadOrSaveFunc' se vola jen tehdy, pokud
    // plugin podporuje "load/save configuration" a v dobe volani existuje klic Salamandera;
    // 'param' je uzivatelsky parametr a je predavan do 'loadOrSaveFunc'
    // omezeni: hlavni thread, v entry-pointu lze volat az po SetBasicPluginData,
    // drive nemusi byt znamo jestli existuje podpora pro "load/save configuration" a jmeno
    // soukromeho klice v registry
    virtual void WINAPI CallLoadOrSaveConfiguration(BOOL load, FSalLoadOrSaveConfiguration loadOrSaveFunc,
                                                    void* param) = 0;

    // ulozi 'text' o delce 'textLen' (-1 znamena "pouzij strlen") na clipboard a to jak multibyte,
    // tak Unicodove (jinak napr. Notepad nezvlada cestinu), v pripade uspechu muze (je-li 'echo' TRUE)
    // zobrazit hlaseni "Text was successfully copied to clipboard." (parent messageboxu bude
    // 'echoParent'); vraci TRUE pri uspechu
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI CopyTextToClipboard(const char* text, int textLen, BOOL showEcho, HWND echoParent) = 0;

    // ulozi unicodovy 'text' o delce 'textLen' (-1 znamena "pouzij wcslen") na clipboard a to jak
    // unicodove, tak multibyte (jinak napr. MSVC6.0 nezvlada cestinu), v pripade uspechu muze (je-li
    // 'echo' TRUE) zobrazit hlaseni "Text was successfully copied to clipboard." (parent messageboxu
    // bude 'echoParent'); vraci TRUE pri uspechu
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI CopyTextToClipboardW(const wchar_t* text, int textLen, BOOL showEcho, HWND echoParent) = 0;

    // spusteni prikazu menu s identifikacnim cislem 'id' v hlavnim threadu (volani
    // CPluginInterfaceForMenuExtAbstract::ExecuteMenuItem(salamander, main-window-hwnd, 'id', 0),
    // 'salamander' je NULL pokud je 'waitForSalIdle' FALSE, jinak obsahuje ukazatel na platnou
    // sadu pouzitelnych metod Salamandera pro provadeni operaci; navratova hodnota
    // se ignoruje); je-li 'waitForSalIdle' FALSE, pouzije se ke spusteni zprava postnuta
    // hlavnimu oknu (tuto zpravu doruci kazda bezici message-loopa v hl. threadu - i modalni
    // dialogy/messageboxy, i ty otevrene pluginem), takze hrozi vicenasobny vstup
    // do pluginu; je-li 'waitForSalIdle' TRUE, je 'id' omezeno na interval <0, 999999> a
    // prikaz se spusti jakmile nebudou v message-queue hl. threadu zadne zpravy a Salamander
    // nebude "busy" (nebude otevreny zadny modalni dialog a nebude se zpracovavat zadna zprava);
    // POZOR: pokud se vola z jineho nez hlavniho threadu, muze dojit ke spusteni prikazu menu
    // (probiha v hlavnim threadu) dokonce drive nez skonci PostMenuExtCommand
    // mozne volat z libovolneho threadu a je-li 'waitForSalIdle' FALSE, je nutne pockat az po volani
    // CPluginInterfaceAbstract::GetInterfaceForMenuExt (vola se po entry-pointu z hlavniho threadu)
    virtual void WINAPI PostMenuExtCommand(int id, BOOL waitForSalIdle) = 0;

    // zjisti jestli je velka sance (jiste se to urcit neda), ze Salamander v pristich par
    // okamzicich nebude "busy" (nebude otevreny zadny modalni dialog a nebude se zpracovavat
    // zadna zprava) - v tomto pripade vraci TRUE (jinak FALSE); neni-li 'lastIdleTime' NULL,
    // vraci se v nem GetTickCount() z okamziku posledniho prechodu z "idle" do "busy" stavu;
    // da se pouzit napr. jako predpoved pro doruceni prikazu postnuteho pomoci
    // CSalamanderGeneralAbstract::PostMenuExtCommand s parametrem 'waitForSalIdle'==TRUE;
    // je mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalamanderIsNotBusy(DWORD* lastIdleTime) = 0;

    // nastavi zpravu, kterou ma vypsat Bug Report dialog pokud dojde k padu uvnitr pluginu
    // (uvnitr pluginu = alespon jedna call-stack-message ulozena z pluginu) a umozni predefinovat
    // standardni bug-reportovou e-mailovou adresu (support@altap.cz); 'message' je nova zprava
    // (NULL znamena "zadna zprava"); 'email' je nova e-mailova adresa (NULL znamena "pouzij
    // standardni"; max. delka e-mailu je 100 znaku); tuto metodu je mozne volat opakovane, puvodni
    // zprava a e-mail se prepisuji; Salamander si pro dalsi spusteni nepamatuje ani zpravu ani
    // e-mail, takze je nutne tuto metodu vzdy pri loadu pluginu (nejlepe v entry-pointu) znovu
    // zavolat
    // omezeni: hlavni thread (jinak muze dojit ke zmenam v konfiguraci pluginu behem volani)
    virtual void WINAPI SetPluginBugReportInfo(const char* message, const char* email) = 0;

    // zjisti jestli je plugin nainstalovan (ovsem nezjisti, jestli se da naloadit - jestli ho
    // user napr. nesmazal jen z disku); 'pluginSPL' identifikuje plugin - jde o pozadovanou
    // koncovou cast plne cesty .SPL souboru pluginu (napr. "ieviewer\\ieviewer.spl" identifikuje
    // IEViewer dodavany se Salamanderem); vraci TRUE pokud je plugin nainstalovan
    // omezeni: hlavni thread (jinak muze dojit ke zmenam v konfiguraci pluginu behem volani)
    virtual BOOL WINAPI IsPluginInstalled(const char* pluginSPL) = 0;

    // otevre soubor ve vieweru implementovanem v pluginu nebo internim text/hex vieweru;
    // je-li 'pluginSPL' NULL, ma byt pouzit interni text/hex viewer, jinak identifikuje plugin
    // vieweru - jde o pozadovanou koncovou cast plne cesty .SPL souboru pluginu (napr.
    // "ieviewer\\ieviewer.spl" identifikuje IEViewer dodavany se Salamanderem); 'pluginData'
    // je struktura dat obsahujici jmeno viewovaneho souboru a nepovinne obsahuje take rozsirene
    // parametry vieweru (viz popis CSalamanderPluginViewerData); je-li 'useCache' FALSE jsou
    // 'rootTmpPath' a 'fileNameInCache' ignorovany a dojde jen k otevreni souboru ve viewru;
    // je-li 'useCache' TRUE, bude soubor nejprve presunut do diskove cache pod jmenem souboru
    // 'fileNameInCache' (jmeno je bez cesty), pak otevren ve vieweru a po uzavreni vieweru bude
    // z diskove cache odstranen, pokud je soubor 'pluginData->FileName' na stejnem disku jako
    // diskova cache, bude presun okamzity, jinak dojde ke kopirovani mezi svazky, ktere muze
    // trvat delsi dobu, ale zadny progress se neukazuje (je-li 'rootTmpPath' NULL, je diskova
    // cache ve Windows TEMP adresari, jinak je cesta do disk cache v 'rootTmpPath'; pro presun
    // do disk cache se pouziva SalMoveFile), idealni je pouziti SalGetTempFileName
    // s parametrem 'path' rovnym 'rootTmpPath'; vraci TRUE pri uspesnem otevreni souboru ve
    // vieweru; vraci FALSE pokud dojde k chybe pri otevirani (konkretni duvod je ulozen v
    // 'error' - 0 - uspech, 1 - nelze naloadit plugin, 2 - ViewFile z pluginu vratil
    // chybu, 3 - nelze presunout soubor do disk cache), je-li 'useCache' TRUE, dojde
    // k odstraneni souboru z disku (jako po uzavreni vieweru)
    // omezeni: hlavni thread (jinak muze dojit ke zmenam v konfiguraci pluginu behem volani),
    // navic neni mozne volat z entry-pointu (load pluginu neni reentrantni)
    virtual BOOL WINAPI ViewFileInPluginViewer(const char* pluginSPL,
                                               CSalamanderPluginViewerData* pluginData,
                                               BOOL useCache, const char* rootTmpPath,
                                               const char* fileNameInCache, int& error) = 0;

    // co nejdrive informuje Salamandera, pak vsechny nactene (loaded) pluginy a pak vsechny otevrene
    // FS (v panelech i odpojene) o zmene na ceste 'path' (diskova nebo FS cesta); je dulezite pro
    // cesty na nichz nelze monitorovat zmeny automaticky (viz FindFirstChangeNotification) nebo
    // si toto monitorovani (auto-refresh) uzivatel vypnul, u FS si monitorovani zmen zajistuje
    // sam plugin; informovani o zmenach probehne co nejdrive (pokud hlavni thread nespousti kod
    // uvnitr pluginu, probehne refresh po doruceni zpravy hlavnimu oknu a pripadne po opetovnem
    // povoleni refreshovani (po zavreni dialogu, atp.), jinak refresh pocka minimalne do okamziku,
    // kdy hlavni thread opusti plugin); 'includingSubdirs' je TRUE v pripade, ze se zmena muze
    // projevit i v podadresarich 'path';
    // POZOR: pokud se vola z jineho nez hlavniho threadu, muze dojit k informovani o zmenach
    // (probiha v hlavnim threadu) dokonce drive nez skonci PostChangeOnPathNotification
    // mozne volat z libovolneho threadu
    virtual void WINAPI PostChangeOnPathNotification(const char* path, BOOL includingSubdirs) = 0;

    // zkusi pristup na windowsovou cestu 'path' (normal nebo UNC), probiha ve vedlejsim threadu, takze
    // umoznuje prerusit zkousku klavesou ESC (po jiste dobe vybali okenko s hlasenim o ESC)
    // 'echo' TRUE znamena povoleny vypis chybove hlasky (pokud cesta nebude pristupna);
    // 'err' ruzne od ERROR_SUCCESS v kombinaci s 'echo' TRUE pouze zobrazi chybu (na cestu
    // se jiz nepristupuje); 'parent' je parent messageboxu; vraci ERROR_SUCCESS v pripade,
    // ze je cesta v poradku, jinak vraci standardni windowsovy kod chyby nebo ERROR_USER_TERMINATED
    // v pripade, ze uzivatel pouzil klavesu ESC k preruseni testu
    // omezeni: hlavni thread (opakovane volani neni mozne a hl. thread tuto metodu pouziva)
    virtual DWORD WINAPI SalCheckPath(BOOL echo, const char* path, DWORD err, HWND parent) = 0;

    // zkusi jestli je windowsova cesta 'path' pristupna, prip. obnovi sitova spojeni (pokud jde
    // o normalni cestu, zkusi ozivit zapamatovane sit. spojeni, pokud jde o UNC cestu, umozni login
    // s novym uzivatelskym jmenem a heslem); vraci TRUE pokud je cesta pristupna; 'parent' je parent
    // messageboxu a dialogu; 'tryNet' je TRUE pokud ma smysl zkouset obnovit sitova spojeni
    // (pri FALSE degraduje na SalCheckPath; je zde jen pro moznost optimalizace)
    // omezeni: hlavni thread (opakovane volani neni mozne a hl. thread tuto metodu pouziva)
    virtual BOOL WINAPI SalCheckAndRestorePath(HWND parent, const char* path, BOOL tryNet) = 0;

    // slozitejsi varianta metody SalCheckAndRestorePath; zkusi jestli je windowsovou cesta 'path'
    // pristupna, prip. ji zkrati; je-li 'tryNet' TRUE, zkusi i obnovit sitove spojeni a nastavi
    // 'tryNet' na FALSE (pokud jde o normalni cestu, zkusi ozivit zapamatovane sit. spojeni, pokud
    // jde o UNC cestu, umozni login s novym uzivatelskym jmenem a heslem); je-li 'donotReconnect'
    // TRUE, zjisti se pouze chyba, obnova spojeni uz se neprovede; vraci 'err' (windowsovy kod chyby
    // aktualni cesty), 'lastErr' (kod chyby vedouci ke zkraceni cesty), 'pathInvalid' (TRUE pokud
    // se zkusila obnova sit. spojeni bez uspechu), 'cut' (TRUE pokud je vysledna cesta zkracena);
    // 'parent' je parent messageboxu; vraci TRUE pokud je vysledna cesta 'path' pristupna
    // omezeni: hlavni thread (opakovane volani neni mozne a hl. thread tuto metodu pouziva)
    virtual BOOL WINAPI SalCheckAndRestorePathWithCut(HWND parent, char* path, BOOL& tryNet, DWORD& err,
                                                      DWORD& lastErr, BOOL& pathInvalid, BOOL& cut,
                                                      BOOL donotReconnect) = 0;

    // rozpozna o jaky typ cesty (FS/windowsova/archiv) jde a postara se o rozdeleni na
    // jeji casti (u FS jde o fs-name a fs-user-part, u archivu jde o path-to-archive a
    // path-in-archive, u windowsovych cest jde o existujici cast a zbytek cesty), u FS cest
    // se nic nekontroluje, u windowsovych (normal + UNC) cest se kontroluje kam az cesta existuje
    // (prip. obnovi sitove spojeni), u archivu se kontroluje existence souboru archivu
    // (rozliseni archivu dle pripony);
    // 'path' je plna nebo relativni cesta (buffer min. 'pathBufSize' znaku; u relativnich cest se
    // uvazuje aktualni cesta 'curPath' (neni-li NULL) jako zaklad pro vyhodnoceni plne cesty;
    // 'curPathIsDiskOrArchive' je TRUE pokud je 'curPath' windowsova nebo archivova cesta;
    // pokud je aktualni cesta archivova, obsahuje 'curArchivePath' jmeno archivu, jinak je NULL),
    // do 'path' se ulozi vysledna plna cesta (musi byt min. 'pathBufSize' znaku); vraci TRUE pri
    // uspesnem rozpoznani, pak 'type' je typ cesty (viz PATH_TYPE_XXX) a 'secondPart' je nastavene:
    // - do 'path' na pozici za existujici cestu (za '\\' nebo na konci retezce; existuje-li
    //   v ceste soubor, ukazuje za cestu k tomuto souboru) (typ cesty windows), POZOR: neresi
    //   se delka vracene casti cesty (cela cesta muze byt delsi nez MAX_PATH)
    // - za soubor archivu (typ cesty archiv), POZOR: neresi se delka cesty v archivu (muze byt
    //   delsi nez MAX_PATH)
    // - za ':' za nazvem file-systemu - user-part cesty file-systemu (typ cesty FS), POZOR: neresi
    //   se delka user-part cesty (muze byt delsi nez MAX_PATH);
    // pokud vraci TRUE je jeste 'isDir' nastavena na:
    // - TRUE pokud existujici cast cesty je adresar, FALSE == soubor (typ cesty windows)
    // - FALSE pro cesty typu archiv a FS;
    // pokud vrati FALSE, userovi byla vypsana chyba (az na jednu vyjimku - viz popis SPP_INCOMLETEPATH),
    // ktera pri rozpoznavani nastala (neni-li 'error' NULL, vraci se v nem jedna z konstant SPP_XXX);
    // 'errorTitle' je titulek messageboxu s chybou; pokud je 'nextFocus' != NULL a windowsova/archivova
    // cesta neobsahuje '\\' nebo na '\\' jen konci, nakopiruje se cesta do 'nextFocus' (viz SalGetFullName);
    // POZOR: pouziva SalGetFullName, proto je dobre napred zavolat metodu
    //        CSalamanderGeneralAbstract::SalUpdateDefaultDir
    // omezeni: hlavni thread (opakovane volani neni mozne a hl. thread tuto metodu pouziva)
    virtual BOOL WINAPI SalParsePath(HWND parent, char* path, int& type, BOOL& isDir, char*& secondPart,
                                     const char* errorTitle, char* nextFocus, BOOL curPathIsDiskOrArchive,
                                     const char* curPath, const char* curArchivePath, int* error,
                                     int pathBufSize) = 0;

    // ziska z windowsove cilove cesty existujici cast a operacni masku; pripadnou neexistujici cast
    // umozni vytvorit; pri uspechu vraci TRUE a existujici windowsovou cilovou cestu (v 'path')
    // a nalezenou operacni masku (v 'mask' - ukazuje do bufferu 'path', ale cesta a maska jsou oddelene
    // nulou; pokud v ceste neni maska, automaticky vytvori masku "*.*"); 'parent' - parent pripadnych
    // messageboxu; 'title' + 'errorTitle' jsou titulky messageboxu s informaci + chybou; 'selCount' je
    // pocet oznacenych souboru a adresaru; 'path' je na vstupu cilova cesta ke zpracovani, na vystupu
    // (alespon 2 * MAX_PATH znaku) existujici cilova cesta; 'secondPart' ukazuje do 'path' na pozici
    // za existujici cestu (za '\\' nebo na konec retezce; existuje-li v ceste soubor, ukazuje za cestu
    // k tomuto souboru); 'pathIsDir' je TRUE/FALSE pokud existujici cast cesty je adresar/soubor;
    // 'backslashAtEnd' je TRUE pokud byl pred provedenim "parse" na konci 'path' backslash (napr.
    // SalParsePath takovy backslash rusi); 'dirName' + 'curDiskPath' nejsou NULL pokud je oznaceny
    // max. jeden soubor/adresar (jeho jmeno bez cesty je v 'dirName'; pokud neni nic oznacene, bere
    // se focus) a aktualni cesta je windowsova (cesta je v 'curDiskPath'); 'mask' je na vystupu
    // ukazatel na operacni masku do bufferu 'path'; pokud je v ceste chyba, vraci metoda FALSE,
    // problem uz byl uzivateli ohlasen
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalSplitWindowsPath(HWND parent, const char* title, const char* errorTitle,
                                            int selCount, char* path, char* secondPart, BOOL pathIsDir,
                                            BOOL backslashAtEnd, const char* dirName,
                                            const char* curDiskPath, char*& mask) = 0;

    // ziska z cilove cesty existujici cast a operacni masku; pripadnou neexistujici cast rozpozna; pri
    // uspechu vraci TRUE, relativni cestu k vytvoreni (v 'newDirs'), existujici cilovou cestu (v 'path';
    // existujici jen za predpokladu vytvoreni relativni cesty 'newDirs') a nalezenou operacni masku
    // (v 'mask' - ukazuje do bufferu 'path', ale cesta a maska jsou oddelene nulou; pokud v ceste neni
    // maska, automaticky vytvori masku "*.*"); 'parent' - parent pripadnych messageboxu;
    // 'title' + 'errorTitle' jsou titulky messageboxu s informaci + chybou; 'selCount' je pocet oznacenych
    // souboru a adresaru; 'path' je na vstupu cilova cesta ke zpracovani, na vystupu (alespon 2 * MAX_PATH
    // znaku) existujici cilova cesta (vzdy konci backslashem); 'afterRoot' ukazuje do 'path' za root cesty
    // (za '\\' nebo na konec retezce); 'secondPart' ukazuje do 'path' na pozici za existujici cestu (za
    // '\\' nebo na konec retezce; existuje-li v ceste soubor, ukazuje za cestu k tomuto souboru);
    // 'pathIsDir' je TRUE/FALSE pokud existujici cast cesty je adresar/soubor; 'backslashAtEnd' je
    // TRUE pokud byl pred provedenim "parse" na konci 'path' backslash (napr. SalParsePath takovy
    // backslash rusi); 'dirName' + 'curPath' nejsou NULL pokud je oznaceny max. jeden soubor/adresar
    // (jeho jmeno bez cesty je v 'dirName'; jeho cesta je v 'curPath'; pokud neni nic oznacene, bere
    // se focus); 'mask' je na vystupu ukazatel na operacni masku do bufferu 'path'; neni-li 'newDirs' NULL,
    // pak jde o buffer (o velikosti alespon MAX_PATH) pro relativni cestu (vzhledem k existujici ceste
    // v 'path'), kterou je nutne vytvorit (uzivatel s vytvorenim souhlasi, byl pouzit stejny dotaz jako
    // u kopirovani z disku na disk; prazdny retezec = nic nevytvaret); je-li 'newDirs' NULL a je-li
    // potreba vytvorit nejakou relativni cestu, je jen vypsana chyba; 'isTheSamePathF' je funkce pro
    // porovnani dvou cest (potrebna jen pokud 'curPath' neni NULL), je-li NULL pouzije se IsTheSamePath;
    // pokud je v ceste chyba, vraci metoda FALSE, problem uz byl uzivateli ohlasen
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalSplitGeneralPath(HWND parent, const char* title, const char* errorTitle,
                                            int selCount, char* path, char* afterRoot, char* secondPart,
                                            BOOL pathIsDir, BOOL backslashAtEnd, const char* dirName,
                                            const char* curPath, char*& mask, char* newDirs,
                                            SGP_IsTheSamePathF isTheSamePathF) = 0;

    // odstrani ".." (vynecha ".." spolu s jednim podadresarem vlevo) a "." (vynecha jen ".")
    // z cesty; podminkou je backslash jako oddelovac podadresaru; 'afterRoot' ukazuje za root
    // zpracovavane cesty (zmeny cesty se deji jen za 'afterRoot'); vraci TRUE pokud se upravy
    // podarily, FALSE pokud se nedaji odstranit ".." (vlevo uz je root)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalRemovePointsFromPath(char* afterRoot) = 0;

    // vraci parametr z konfigurace Salamandera; 'paramID' identifikuje o jaky parametr jde
    // (viz konstanty SALCFG_XXX); 'buffer' ukazuje na buffer, do ktereho se nakopiruji
    // data parametru, velikost bufferu je 'bufferSize'; neni-li 'type' NULL, vraci se
    // v nem jedna z konstant SALCFGTYPE_XXX nebo SALCFGTYPE_NOTFOUND (pokud parametr s
    // 'paramID' nebyl nalezen); vraci TRUE pokud je 'paramID' platne a zaroven pokud se
    // hodnota parametru konfigurace vejde do bufferu 'buffer'
    // poznamka: zmeny v konfiguraci Salamandera jsou hlaseny pomoci udalosti
    //           PLUGINEVENT_CONFIGURATIONCHANGED (viz metoda CPluginInterfaceAbstract::Event)
    // omezeni: hlavni thread, zmeny konfigurace se deji jen v hl. threadu (neobsahuje jinou
    //          synchronizaci)
    virtual BOOL WINAPI GetConfigParameter(int paramID, void* buffer, int bufferSize, int* type) = 0;

    // meni velikost pismen ve jmene souboru (jmeno je bez cesty); 'tgtName' je buffer pro vysledek
    // (velikost je min. pro ulozeni retezce 'srcName'); 'srcName' je jmeno souboru (zapisuje se do nej,
    // ale pred navratem z metody je vzdy obnoveno); 'format' je format vysledku (1 - velka pocatecni
    // pismena slov, 2 - komplet mala pismena, 3 - komplet velka pismena, 4 - beze zmen, 5 - pokud
    // je DOS jmeno (8.3) -> velka pocatecni pismena slov, 6 - soubor malymi, adresar velkymi pismeny,
    // 7 - velka pocatecni pismena ve jmene a mala pismena v pripone);
    // 'changedParts' urcuje jake casti jmena se maji menit (0 - meni jmeno i priponu, 1 - meni jen
    // jmeno (mozne jen s format == 1, 2, 3, 4), 2 - meni jen priponu (mozne jen s
    // format == 1, 2, 3, 4)); 'isDir' je TRUE pokud jde o jmeno adresare
    // mozne volat z libovolneho threadu
    virtual void WINAPI AlterFileName(char* tgtName, char* srcName, int format, int changedParts,
                                      BOOL isDir) = 0;

    // zobrazi/schova zpravu v okenku ve vlastnim threadu (neodcerpa message-queue); zobrazi
    // najednou jen jednu zpravu, opakovane volani ohlasi chybu do TRACE (neni fatalni);
    // POZN.: pouziva se v SalCheckPath a dalsich rutinach, muze tedy dojit ke kolizi mezi
    //        pozadavky na otevreni okenek (neni fatalni, jen se nezobrazi)
    // vse je mozne volat z libovolneho threadu (ale okenko se musi obsluhovat jen
    // z jednoho threadu - nelze jej ukazat z jednoho threadu a schovat z druheho)
    //
    // otevreni okenka s textem 'message' se zpodenim 'delay' (v ms), jen pokud je 'hForegroundWnd' NULL
    // nebo identifikuje okno na popredi (foreground)
    // 'message' muze byt vicerakova; jednotlive radky se oddeluji znakem '\n'
    // 'caption' muze byt NULL: pouzije se pak "Open Salamander"
    // 'showCloseButton' udava, zda bude okenko obsahovat tlacitko Close; ekvivalent ke klavese Escape
    virtual void WINAPI CreateSafeWaitWindow(const char* message, const char* caption,
                                             int delay, BOOL showCloseButton, HWND hForegroundWnd) = 0;
    // zavreni okenka
    virtual void WINAPI DestroySafeWaitWindow() = 0;
    // schovani/zobrazeni okenka (je-li otevrene); volat jako reakci na WM_ACTIVATE z okna hForegroundWnd:
    //    case WM_ACTIVATE:
    //    {
    //      ShowSafeWaitWindow(LOWORD(wParam) != WA_INACTIVE);
    //      break;
    //    }
    // Pokud je thread (ze ktereho bylo okenko vytvoreno) zamestnan, nedochazi
    // k distribuci zprav, takze nedojde ani k doruceni WM_ACTIVATE pri kliknuti
    // na jinou aplikaci. Zpravy se doruci az ve chvili zobrazeni messageboxu,
    // coz presne potrebujeme: docasne se nechame schovat a pozdeji (po zavreni
    // messageboxu a aktivaci okna hForegroundWnd) zase zobrazit.
    virtual void WINAPI ShowSafeWaitWindow(BOOL show) = 0;
    // po zavolani CreateSafeWaitWindow nebo ShowSafeWaitWindow vraci funkce FALSE az do doby,
    // kdy user kliknul mysi na Close tlacitko (pokud je zobrazeno); pak vraci TRUE
    virtual BOOL WINAPI GetSafeWaitWindowClosePressed() = 0;
    // slouzi pro dodatecnou zmenu textu v okenku
    // POZOR: nedochazi k novemu layoutovani okna a pokud dojde k vetsimu natazeni
    // textu, bude orezan; pouzivat napriklad pro countdown: 60s, 55s, 50s, ...
    virtual void WINAPI SetSafeWaitWindowText(const char* message) = 0;

    // najde v disk-cache existujici kopii souboru a zamkne ji (zakaze jeji zruseni); 'uniqueFileName'
    // je unikatni nazev originalniho souboru (podle tohoto nazvu se prohledava disk-cache; melo by
    // stacit plne jmeno souboru v salamanderovske forme - "fs-name:fs-user-part"; POZOR: nazev se
    // porovnava "case-sensitive", pokud plugin vyzaduje "case-insensitive", musi vsechny nazvy
    // prevadet napr. na mala pismena - viz CSalamanderGeneralAbstract::ToLowerCase); v 'tmpName'
    // se vraci ukazatel (platny az do zruseni kopie souboru v disk-cache) na plne jmeno kopie souboru,
    // ktera je umistena v docasnem adresari; 'fileLock' je zamek kopie souboru, jde o systemovy event
    // ve stavu nonsignaled, ktery po zpracovani kopie souboru prejde do stavu signaled (je nutne
    // pouzit metodu UnlockFileInCache; plugin dava signal, ze kopii v disk-cache jiz lze zrusit);
    // pokud nebyla kopie nalezena vraci FALSE a 'tmpName' NULL (jinak vraci TRUE)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI GetFileFromCache(const char* uniqueFileName, const char*& tmpName, HANDLE fileLock) = 0;

    // odemkne zamek kopie souboru v disk-cache (nastavi 'fileLock' do stavu signaled, vyzve
    // disk-cache k provedeni kontroly zamku, a pak nastavi 'fileLock' zpet do stavu nonsignaled);
    // pokud slo o posledni zamek, muze dojit ke zruseni kopie, kdy dojde ke zruseni kopie zalezi
    // na velikosti disk-cache na disku; zamek muze byt pouzity i pro vice kopii souboru (zamek
    // musi byt typu "manual reset", jinak se po odemknuti prvni kopie zamek nastavi do stavu
    // nonsignaled a odemykani skonci), v tomto pripade probehne odemknuti u vsech kopii
    // mozne volat z libovolneho threadu
    virtual void WINAPI UnlockFileInCache(HANDLE fileLock) = 0;

    // vlozi (presune) kopii souboru do disk-cache (vkladana kopie neni zamcena, takze kdykoliv muze dojit
    // k jejimu zruseni); 'uniqueFileName' je unikatni nazev originalniho souboru (podle tohoto
    // nazvu se prohledava disk-cache; melo by stacit plne jmeno souboru v salamanderovske
    // forme - "fs-name:fs-user-part"; POZOR: nazev se porovnava "case-sensitive", pokud plugin
    // vyzaduje "case-insensitive", musi vsechny nazvy prevadet napr. na mala pismena - viz
    // CSalamanderGeneralAbstract::ToLowerCase); 'nameInCache' je jmeno kopie souboru, ktera bude umistena
    // v docasnem adresari (ocekava se zde posledni cast jmena originalniho souboru, aby pozdeji
    // uzivateli pripominala originalni soubor); 'newFileName' je plne jmeno ukladane kopie souboru,
    // ktera bude presunuta do disk-cache pod jmenem 'nameInCache', musi byt umistena na stejnem disku
    // jako diskova cache (je-li 'rootTmpPath' NULL, je diskova cache ve Windows TEMP adresari, jinak
    // je cesta do disk-cache v 'rootTmpPath'; pro prejmenovani do disk cache pres Win32 API funkci
    // MoveFile); 'newFileName' je idealni ziskat volanim SalGetTempFileName s parametrem 'path' rovnym
    // 'rootTmpPath'); v 'newFileSize' je velikost ukladane kopie souboru; vraci TRUE pri uspechu
    // (soubor byl presunut do disk-cache - zmizel z puvodniho mista na disku), vraci FALSE pri
    // interni chybe nebo pokud soubor jiz je v disk-cache (neni-li 'alreadyExists' NULL, vraci
    // se v nem TRUE pokud soubor jiz je v disk-cache)
    // POZN.: pokud plugin vyuziva disk-cache, mel by alespon pri unloadu pluginu zavolat
    //        CSalamanderGeneralAbstract::RemoveFilesFromCache("fs-name:"), jinak budou
    //        jeho kopie souboru zbytecne prekazet v disk-cache
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI MoveFileToCache(const char* uniqueFileName, const char* nameInCache,
                                        const char* rootTmpPath, const char* newFileName,
                                        const CQuadWord& newFileSize, BOOL* alreadyExists) = 0;

    // odstrani z disk-cache kopii souboru, jejiz unikatni nazev je 'uniqueFileName' (POZOR: nazev
    // se porovnava "case-sensitive", pokud plugin vyzaduje "case-insensitive", musi vsechny nazvy
    // prevadet napr. na mala pismena - viz CSalamanderGeneralAbstract::ToLowerCase); pokud se kopie
    // souboru jeste pouziva, odstrani se az to bude mozne (az se zavrou viewery), kazdopadne uz ji
    // disk-cache nikomu neposkytne jako platnou kopii souboru (je oznacena jako out-of-date)
    // mozne volat z libovolneho threadu
    virtual void WINAPI RemoveOneFileFromCache(const char* uniqueFileName) = 0;

    // odstrani z disk-cache vsechny kopie souboru, jejichz unikatni nazvy zacinaji na 'fileNamesRoot'
    // (pouziva se pri uzavreni file-systemu, kdyz uz je dale nezadouci cachovat downloadene kopie
    // souboru; POZOR: nazvy se porovnavaji "case-sensitive", pokud plugin vyzaduje "case-insensitive",
    // musi vsechny nazvy prevadet napr. na mala pismena - viz CSalamanderGeneralAbstract::ToLowerCase);
    // pokud se kopie souboru jeste pouzivaji, odstrani se az to bude mozne (az se odemknou
    // napr. po zavreni vieweru), kazdopadne uz je disk-cache nikomu neposkytne jako platne kopie
    // souboru (jsou oznacene jako out-of-date)
    // mozne volat z libovolneho threadu
    virtual void WINAPI RemoveFilesFromCache(const char* fileNamesRoot) = 0;

    // vraci postupne konverzni tabulky (nactene ze souboru convert\XXX\convert.cfg
    // v instalaci Salamandera - XXX je prave pouzivany adresar konverznich tabulek);
    // 'parent' je parent messageboxu (je-li NULL, je parent hlavni okno);
    // 'index' je vstupne/vystupni promenna, ukazuje na int, ve kterem je pri prvnim volani 0,
    // hodnotu pro dalsi volani si funkce ulozi pri navratu (pouziti: na zacatku vynulovat, pak
    // nemenit); vraci FALSE, pokud jiz neexistuje zadna dalsi tabulka; pokud vraci TRUE, obsahuje
    // 'name' (neni-li NULL) odkaz na jmeno konverze (muze obsahovat '&' - podtrzeny znak v menu) nebo NULL
    // jde-li o separator a 'table' (neni-li NULL) odkaz na 256-bytovou konverzni tabulku nebo NULL
    // jde-li o separator; odkazy 'name' a 'table' jsou platne po celou dobu behu Salamandera (neni
    // nutne kopirovat obsah)
    // POZOR: ukazatel 'table' pouzivat timto zpusobem (nutne pretypovani na "unsigned"):
    //        *s = table[(unsigned char)*s]
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI EnumConversionTables(HWND parent, int* index, const char** name, const char** table) = 0;

    // vraci konverzni tabulku 'table' (buffer min. 256 znaku) pro konverzi 'conversion' (jmeno
    // konverze viz soubor convert\XXX\convert.cfg v instalaci Salamandera, napr. "ISO-8859-2 - CP1250";
    // znaky <= ' ' a '-' a '&' ve jmene pri hledani nehraji roli; hleda se bez ohledu na velka a mala
    // pismena); 'parent' je parent messageboxu (je-li NULL, je parent hlavni okno); vraci TRUE
    // pokud byla konverze nalezena (jinak neni obsah 'table' platny);
    // POZOR: pouzivat timto zpusobem (nutne pretypovani na "unsigned"): *s = table[(unsigned char)*s]
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI GetConversionTable(HWND parent, char* table, const char* conversion) = 0;

    // vraci jmeno kodove stranky pouzivane ve Windows v tomto regionu (cerpa v convert\XXX\convert.cfg
    // v instalaci Salamandera); jde o normalne zobrazitelne kodovani, proto se pouziva v pripade,
    // ze je potreba zobrazit text, ktery byl vytvoren v jine kodove strance (zde se zada jako
    // "cilove" kodovani pri hledani konverzni tabulky, viz metoda GetConversionTable);
    // 'parent' je parent messageboxu (je-li NULL, je parent hlavni okno); 'codePage' je buffer
    // (min. 101 znaku) pro jmeno kodove stranky (pokud neni v souboru convert\XXX\convert.cfg toto jmeno
    // definovane, vraci se v bufferu prazdny retezec)
    // mozne volat z libovolneho threadu
    virtual void WINAPI GetWindowsCodePage(HWND parent, char* codePage) = 0;

    // zjisti z bufferu 'pattern' o delce 'patternLen' (napr. prvnich 10000 znaku) jestli jde
    // o text (existuje kodova stranka, ve ktere obsahuje jen povolene znaky - zobrazitelne
    // a ridici) a pokud jde o text, zjisti take jeho kodovou stranku (nejpravdepodobnejsi);
    // 'parent' je parent messageboxu (je-li NULL, je parent hlavni okno); je-li 'forceText'
    // TRUE, neprovadi se kontrola na nepovolene znaky (pouziva se, pokud 'pattern' obsahuje
    // text); neni-li 'isText' NULL, vraci se v nem TRUE pokud jde o text; neni-li 'codePage'
    // NULL, jde o buffer (min. 101 znaku) pro jmeno kodove stranky (nejpravdepodobnejsi)
    // mozne volat z libovolneho threadu
    virtual void WINAPI RecognizeFileType(HWND parent, const char* pattern, int patternLen, BOOL forceText,
                                          BOOL* isText, char* codePage) = 0;

    // zjisti z bufferu 'text' o delce 'textLen' jestli jde o ANSI text (obsahuje (v ANSI
    // znakove sade) jen povolene znaky - zobrazitelne a ridici); rozhoduje bez kontextu
    // (nezalezi na poctu znaku ani jak jdou po sobe - testovany text je mozne rozdelit
    // na libovolne casti a testovat je postupne); vraci TRUE pokud jde o ANSI text (jinak
    // je obsah bufferu 'text' binarni)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI IsANSIText(const char* text, int textLen) = 0;

    // zavola funkci 'callback' s parametry 'param' a funkci pro ziskavani oznacenych
    // souboru/adresaru (viz definice typu SalPluginOperationFromDisk) z panelu 'panel'
    // (v panelu musi byt otevrena windowsova cesta); 'panel' je jeden z PANEL_XXX
    // omezeni: hlavni thread
    virtual void WINAPI CallPluginOperationFromDisk(int panel, SalPluginOperationFromDisk callback,
                                                    void* param) = 0;

    // vraci standardni charset, ktery ma uzivatel nastaveny (soucast regionalniho
    // nastaveni); fonty je nutne konstruovat s timto charsetem, jinak nemusi byt
    // texty citelne (je-li text ve standardni kodove strance, viz Win32 API funkce
    // GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE, ...))
    // mozne volat z libovolneho threadu
    virtual BYTE WINAPI GetUserDefaultCharset() = 0;

    // alokuje novy objekt Boyer-Moorova vyhledavaciho algoritmu
    // mozne volat z libovolneho threadu
    virtual CSalamanderBMSearchData* WINAPI AllocSalamanderBMSearchData() = 0;

    // uvolni objekt Boyer-Moorova vyhledavaciho algoritmu (ziskany metodou AllocSalamanderBMSearchData)
    // mozne volat z libovolneho threadu
    virtual void WINAPI FreeSalamanderBMSearchData(CSalamanderBMSearchData* data) = 0;

    // alokuje novy objekt algoritmu pro vyhledavani pomoci regularnich vyrazu
    // mozne volat z libovolneho threadu
    virtual CSalamanderREGEXPSearchData* WINAPI AllocSalamanderREGEXPSearchData() = 0;

    // uvolni objekt algoritmu pro vyhledavani pomoci regularnich vyrazu (ziskany metodou
    // AllocSalamanderREGEXPSearchData)
    // mozne volat z libovolneho threadu
    virtual void WINAPI FreeSalamanderREGEXPSearchData(CSalamanderREGEXPSearchData* data) = 0;

    // vraci postupne prikazy Salamandera (postupuje v poradi definice konstant SALCMD_XXX);
    // 'index' je vstupne/vystupni promenna, ukazuje na int, ve kterem je pri prvnim volani 0,
    // hodnotu pro dalsi volani si funkce ulozi pri navratu (pouziti: na zacatku vynulovat, pak
    // nemenit); vraci FALSE, pokud jiz neexistuje zadny dalsi prikaz; pokud vraci TRUE, obsahuje
    // 'salCmd' (neni-li NULL) cislo prikazu Salamandera (viz konstanty SALCMD_XXX; cisla maji
    // rezervovany interval 0 az 499, pokud tedy maji v menu byt prikazy Salamandera spolecne s
    // jinymi prikazy, neni problem vytvorit vzajemne se neprekryvajici mnoziny hodnot prikazu
    // napr. posunutim vsech hodnot o zvolene cislo - priklad viz DEMOPLUGin -
    // CPluginFSInterface::ContextMenu), 'nameBuf' (buffer o velikosti 'nameBufSize' bytu)
    // obsahuje jmeno prikazu (jmeno je pripraveno pro pouziti v menu - ma zdvojene ampersandy,
    // podtrzene znaky oznacene ampersandy a za '\t' ma popisy klavesovych zkratek), 'enabled'
    // (neni-li NULL) obsahuje stav prikazu (TRUE/FALSE pokud je enabled/disabled), 'type'
    // (neni-li NULL) obsahuje typ prikazu (viz popis konstant sctyXXX)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI EnumSalamanderCommands(int* index, int* salCmd, char* nameBuf, int nameBufSize,
                                               BOOL* enabled, int* type) = 0;

    // vraci prikaz Salamandera s cislem 'salCmd' (viz konstanty SALCMD_XXX);
    // vraci FALSE, pokud takovy prikaz neexistuje; pokud vraci TRUE, obsahuje
    // 'nameBuf' (buffer o velikosti 'nameBufSize' bytu) jmeno prikazu (jmeno je pripraveno pro
    // pouziti v menu - ma zdvojene ampersandy, podtrzene znaky oznacene ampersandy a za '\t' ma
    // popisy klavesovych zkratek), 'enabled' (neni-li NULL) obsahuje stav prikazu (TRUE/FALSE
    // pokud je enabled/disabled), 'type' (neni-li NULL) obsahuje typ prikazu (viz popis konstant
    // sctyXXX)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI GetSalamanderCommand(int salCmd, char* nameBuf, int nameBufSize, BOOL* enabled,
                                             int* type) = 0;

    // nastavi volajicimu pluginu priznak, ze se ma pri nejblizsi mozne prilezitosti spustit
    // prikaz Salamandera s cislem 'salCmd' (jakmile nebudou v message-queue hl. threadu zadne
    // message a Salamander nebude "busy" (nebude otevreny zadny modalni dialog a nebude se
    // zpracovavat zadna zprava));
    // POZOR: pokud se vola z jineho nez hlavniho threadu, muze dojit ke spusteni prikazu Salamandera
    // (probiha v hlavnim threadu) dokonce drive nez skonci PostSalamanderCommand
    // mozne volat z libovolneho threadu
    virtual void WINAPI PostSalamanderCommand(int salCmd) = 0;

    // nastavi priznak "uzivatel pracoval s aktualni cestou" v panelu 'panel' (tento priznak
    // se vyuziva pri plneni seznamu pracovnich adresaru - List Of Working Directories (Alt+F12));
    // 'panel' je jeden z PANEL_XXX
    // omezeni: hlavni thread
    virtual void WINAPI SetUserWorkedOnPanelPath(int panel) = 0;

    // v panelu 'panel' (jedna z konstant PANEL_XXX) ulozi vybrana (Selected) jmena
    // do zvlastniho pole, ze ktereho muze uzivatel vyber obnovit prikazem Edit/Restore Selection
    // vyuziva se pro prikazy, ktere zrusi aktualni vyber, aby mel uzivatel moznost
    // se k nemu vratit a provest jeste dalsi operaci
    // omezeni: hlavni thread
    virtual void WINAPI StoreSelectionOnPanelPath(int panel) = 0;

    //
    // UpdateCrc32
    //   Updates CRC-32 (32-bit Cyclic Redundancy Check) with specified array of bytes.
    //
    // Parameters
    //   'buffer'
    //      [in] Pointer to the starting address of the block of memory to update 'crcVal' with.
    //
    //   'count'
    //      [in] Size, in bytes, of the block of memory to update 'crcVal' with.
    //
    //   'crcVal'
    //      [in] Initial crc value. Set this value to zero to calculate CRC-32 of the 'buffer'.
    //
    // Return Values
    //   Returns updated CRC-32 value.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual DWORD WINAPI UpdateCrc32(const void* buffer, DWORD count, DWORD crcVal) = 0;

    // alokuje novy objekt pro vypocet MD5
    // mozne volat z libovolneho threadu
    virtual CSalamanderMD5* WINAPI AllocSalamanderMD5() = 0;

    // uvolni objekt pro vypocet MD5 (ziskany metodou AllocSalamanderMD5)
    // mozne volat z libovolneho threadu
    virtual void WINAPI FreeSalamanderMD5(CSalamanderMD5* md5) = 0;

    // V textu nalezne pary '<' '>', vyradi je z bufferu a prida odkazy na
    // jejich obsah do 'varPlacements'. 'varPlacements' je pole DWORDu o '*varPlacementsCount'
    // polozkach, DWORDy jsou slozene vzdy z pozice odkazu ve vystupnim bufferu (spodni WORD)
    // a poctu znaku odkazu (horni WORD). Retezce "\<", "\>", "\\" jsou chapany
    // jako escape sekvence a budou nahrazeny znaky '<', '>' a '\\'.
    // Vraci TRUE v pripade uspechu, jinak FALSE; vzdy nastavi 'varPlacementsCount' na
    // pocet zpracovanych promennych.
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI LookForSubTexts(char* text, DWORD* varPlacements, int* varPlacementsCount) = 0;

    // cekani (maximalne 0.2 sekundy) na pusteni klavesy ESC; pouziva se pokud plugin obsahuje
    // akce, ktere se prerusuji klavesou ESC (monitorovani klavesy ESC pres
    // GetAsyncKeyState(VK_ESCAPE)) - zabranuje tomu, aby se po stisknuti ESC v dialogu/messageboxu
    // hned prerusila i nasledujici akce monitorujici klavesu ESC
    // mozne volat z libovolneho threadu
    virtual void WINAPI WaitForESCRelease() = 0;

    //
    // GetMouseWheelScrollLines
    //   An OS independent method to retrieve the number of wheel scroll lines.
    //
    // Return Values
    //   Number of scroll lines where WHEEL_PAGESCROLL (0xffffffff) indicates to scroll a page at a time.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual DWORD WINAPI GetMouseWheelScrollLines() = 0;

    //
    // GetTopVisibleParent
    //   Retrieves the visible root window by walking the chain of parent windows
    //   returned by GetParent.
    //
    // Parameters
    //   'hParent'
    //      [in] Handle to the window whose parent window handle is to be retrieved.
    //
    // Return Values
    //   The return value is the handle to the top Popup or Overlapped visible parent window.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual HWND WINAPI GetTopVisibleParent(HWND hParent) = 0;

    //
    // MultiMonGetDefaultWindowPos
    //   Retrieves the default position of the upper-left corner for a newly created window
    //   on the display monitor that has the largest area of intersection with the bounding
    //   rectangle of a specified window.
    //
    // Parameters
    //   'hByWnd'
    //      [in] Handle to the window of interest.
    //
    //   'p'
    //      [out] Pointer to a POINT structure that receives the virtual-screen coordinates
    //      of the upper-left corner for the window that would be created with CreateWindow
    //      with CW_USEDEFAULT in the 'x' parameter. Note that if the monitor is not the
    //      primary display monitor, some of the point's coordinates may be negative values.
    //
    // Return Values
    //   If the default window position lies on the primary monitor or some error occured,
    //   the return value is FALSE and you should use CreateWindow with CW_USEDEFAULT in
    //   the 'x' parameter.
    //
    //   Otherwise the return value is TRUE and coordinates from 'p' structure should be used
    //   in the CreateWindow 'x' and 'y' parameters.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual BOOL WINAPI MultiMonGetDefaultWindowPos(HWND hByWnd, POINT* p) = 0;

    //
    // MultiMonGetClipRectByRect
    //   Retrieves the bounding rectangle of the display monitor that has the largest
    //   area of intersection with a specified rectangle.
    //
    // Parameters
    //   'rect'
    //      [in] Pointer to a RECT structure that specifies the rectangle of interest
    //      in virtual-screen coordinates.
    //
    //   'workClipRect'
    //      [out] A RECT structure that specifies the work area rectangle of the
    //      display monitor, expressed in virtual-screen coordinates. Note that
    //      if the monitor is not the primary display monitor, some of the rectangle's
    //      coordinates may be negative values.
    //
    //   'monitorClipRect'
    //      [out] A RECT structure that specifies the display monitor rectangle,
    //      expressed in virtual-screen coordinates. Note that if the monitor is
    //      not the primary display monitor, some of the rectangle's coordinates
    //      may be negative values. This parameter can be NULL.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual void WINAPI MultiMonGetClipRectByRect(const RECT* rect, RECT* workClipRect, RECT* monitorClipRect) = 0;

    //
    // MultiMonGetClipRectByWindow
    //   Retrieves the bounding rectangle of the display monitor that has the largest
    //   area of intersection with the bounding rectangle of a specified window.
    //
    // Parameters
    //   'hByWnd'
    //      [in] Handle to the window of the interest. If this parameter is NULL,
    //      or window is not visible or is iconic, monitor with the currently active window
    //      from the same application will be used. Otherwise primary monitor will be used.
    //
    //   'workClipRect'
    //      [out] A RECT structure that specifies the work area rectangle of the
    //      display monitor, expressed in virtual-screen coordinates. Note that
    //      if the monitor is not the primary display monitor, some of the rectangle's
    //      coordinates may be negative values.
    //
    //   'monitorClipRect'
    //      [out] A RECT structure that specifies the display monitor rectangle,
    //      expressed in virtual-screen coordinates. Note that if the monitor is
    //      not the primary display monitor, some of the rectangle's coordinates
    //      may be negative values. This parameter can be NULL.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual void WINAPI MultiMonGetClipRectByWindow(HWND hByWnd, RECT* workClipRect, RECT* monitorClipRect) = 0;

    //
    // MultiMonCenterWindow
    //   Centers the window against a specified window or monitor.
    //
    // Parameters
    //   'hWindow'
    //      [in] Handle to the window whose parent window handle is to be retrieved.
    //
    //   'hByWnd'
    //      [in] Handle to the window against which to center. If this parameter is NULL,
    //      or window is not visible or is iconic, the method will center 'hWindow' against
    //      the working area of the monitor. Monitor with the currently active window
    //      from the same application will be used. Otherwise primary monitor will be used.
    //
    //   'findTopWindow'
    //      [in] If this parameter is TRUE, non child visible window will be used by walking
    //      the chain of parent windows of 'hByWnd' as the window against which to center.
    //
    //      If this parameter is FALSE, 'hByWnd' will be to the window against which to center.
    //
    // Remarks
    //   If centered window gets over working area of the monitor, the method positions
    //   the window to be whole visible.
    //
    //   Method can be called from any thread.
    //
    virtual void WINAPI MultiMonCenterWindow(HWND hWindow, HWND hByWnd, BOOL findTopWindow) = 0;

    //
    // MultiMonEnsureRectVisible
    //   Ensures that specified rectangle is either entirely or partially visible,
    //   adjusting the coordinates if necessary. All monitors are considered.
    //
    // Parameters
    //   'rect'
    //      [in/out] Pointer to the RECT structure that contain the coordinated to be
    //      adjusted. The rectangle is presumed to be in virtual-screen coordinates.
    //
    //   'partialOK'
    //      [in] Value specifying whether the rectangle must be entirely visible.
    //      If this parameter is TRUE, no moving occurs if the item is at least
    //      partially visible.
    //
    // Return Values
    //   If the rectangle is adjusted, the return value is TRUE.
    //
    //   If the rectangle is not adjusted, the return value is FALSE.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual BOOL WINAPI MultiMonEnsureRectVisible(RECT* rect, BOOL partialOK) = 0;

    //
    // InstallWordBreakProc
    //   Installs special word break procedure to the specified window. This procedure
    //   is inteded for easier cursor movevement in the single line edit controls.
    //   Delimiters '\\', '/', ' ', ';', ',', and '.' are used as cursor stops when user
    //   navigates using Ctrl+Left or Ctrl+Right keys.
    //   You can use Ctrl+Backspace to delete one word.
    //
    // Parameters
    //   'hWindow'
    //      [in] Handle to the window or control where word break proc is to be isntalled.
    //      Window may be either edit or combo box with edit control.
    //
    // Return Values
    //   The return value is TRUE if the word break proc is installed. It is FALSE if the
    //   window is neither edit nor combo box with edit control, some error occured, or
    //   this special word break proc is not supported on your OS.
    //
    // Remarks
    //   You needn't uninstall word break procedure before window is destroyed.
    //
    //   Method can be called from any thread.
    //
    virtual BOOL WINAPI InstallWordBreakProc(HWND hWindow) = 0;

    // Salamander 3 nebo novejsi: vraci TRUE, pokud byla tato instance Altap
    // Salamandera prvni spustena (v dobe startu instance se hledaji dalsi bezici
    // instance verze 3 nebo novejsi);
    //
    // Poznamky k ruznym SID / Session / Integrity Level (netyka se Salamandera 2.5 a 2.51):
    // funkce vrati TRUE i v pripade, ze jiz bezi instance Salamandera spustena
    // pod jinym SID; na session a integrity level nezalezi, takze pokud jiz bezi
    // instance Salamandera na jine session, pripadne s jinym integrity level, ale
    // se shodnym SID, vrati nove spustena instance FALSE
    //
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI IsFirstInstance3OrLater() = 0;

    // support for parameter dependent strings (dealing with singles/plurals);
    // 'format' is format string for resulting string - its description follows;
    // resulting string is copied to 'buffer' buffer which size is 'bufferSize' bytes;
    // 'parametersArray' is array of parameters; 'parametersCount' is count of
    // these parameters; returns length of the resulting string
    //
    // format string description:
    //   - each format string starts with signature "{!}"
    //   - format string can contain following escape sequences (it allows to use special
    //     character without its special meaning): "\\" = "\", "\{" = "{", "\}" = "}",
    //     "\:" = ":", and "\|" = "|" (do not forget to double backslashes when writing C++
    //     strings, this applies only to format strings placed directly in C++ source code)
    //   - text which is not placed in curly brackets goes directly to resulting string
    //     (only escape sequences are handled)
    //   - parameter dependent text is placed in curly brackets
    //   - each parameter dependent text uses one parameter from 'parametersArray'
    //     (it is 64-bit unsigned int)
    //   - parameter dependent text contains more variants of resulting text, which variant
    //     is used depends on parameter value, more precisely to which defined interval the
    //     value belongs
    //   - variants of resulting text and interval bounds are separated by "|" character
    //   - first interval is from 0 to first interval bound
    //   - last interval is from last interval bound plus one to infinity (2^64-1)
    //   - parameter dependent text "{}" is used to skip one parameter from 'parametersArray'
    //     (nothing goes to resulting string)
    //   - you can also specify index of parameter to use for parameter dependent text,
    //     just place its index (from one to number of parameters) to the beginning of
    //     parameter dependent text and follow it by ':' character
    //   - if you don't specify index of parameter to use, it is assigned automatically
    //     (starting from one to number of parameters)
    //   - if you specify index of parameter to use, the next index which is assigned
    //     automatically is not affected,
    //     e.g. in "{!}%d file{2:s|0||1|s} and %d director{y|1|ies}" the first parameter
    //     dependent text uses parameter with index 2 and second uses parameter with index 1
    //   - you can use any number of parameter dependent texts with specified index
    //     of parameter to use
    //
    // examples of format strings:
    //   - "{!}director{y|1|ies}": for parameter values from 0 to 1 resulting string will be
    //     "directory" and for parameter values from 2 to infinity (2^64-1) resulting string
    //     will be "directories"
    //   - "{!}%d soubor{u|0||1|y|4|u} a %d adresar{u|0||1|e|4|u}": it needs two parameters
    //     because there are two dependent texts in curly brackets, resulting string for
    //     choosen pairs of parameters (I believe it is not needed to show all possible variants):
    //       0, 0: "%d souboru a %d adresaru"
    //       1, 12: "%d soubor a %d adresaru"
    //       3, 4: "%d soubory a %d adresare"
    //       13, 1: "%d souboru a %d adresar"
    //
    // method can be called from any thread
    virtual int WINAPI ExpandPluralString(char* buffer, int bufferSize, const char* format,
                                          int parametersCount, const CQuadWord* parametersArray) = 0;

    // v aktualni jazykove verzi Salamandera pripravi retezec "XXX (selected/hidden)
    // files and YYY (selected/hidden) directories"; je-li XXX (hodnota parametru 'files')
    // nebo YYY (hodnota parametru 'dirs') nula, prislusna cast retezce se vypousti (oba
    // parametry zaroven nulove se neuvazuji); pouziti "selected" a "hidden" zavisi
    // na rezimu 'mode' - viz popis konstant epfdmXXX; vysledny text
    // se vraci v bufferu 'buffer' o velikosti 'bufferSize' bytu; vraci delku vysledneho
    // textu; 'forDlgCaption' je TRUE/FALSE pokud je/neni text urceny pro titulek dialogu
    // (v anglictine nutna velka pocatecni pismena)
    // mozne volat z libovolneho threadu
    virtual int WINAPI ExpandPluralFilesDirs(char* buffer, int bufferSize, int files, int dirs,
                                             int mode, BOOL forDlgCaption) = 0;

    // v aktualni jazykove verzi Salamandera pripravi retezec "BBB bytes in XXX selected
    // files and YYY selected directories"; BBB je hodnota parametru 'selectedBytes';
    // je-li XXX (hodnota parametru 'files') nebo YYY (hodnota parametru 'dirs') nula,
    // prislusna cast retezce se vypousti (oba parametry zaroven nulove se neuvazuji);
    // je-li 'useSubTexts' TRUE, uzavorkuje se BBB do '<' a '>', aby se s BBB dalo
    // dale pracovat na info-line (viz metoda CSalamanderGeneralAbstract::LookForSubTexts a
    // CPluginDataInterfaceAbstract::GetInfoLineContent); vysledny text se vraci v bufferu
    // 'buffer' o velikosti 'bufferSize' bytu; vraci delku vysledneho textu
    // mozne volat z libovolneho threadu
    virtual int WINAPI ExpandPluralBytesFilesDirs(char* buffer, int bufferSize,
                                                  const CQuadWord& selectedBytes, int files, int dirs,
                                                  BOOL useSubTexts) = 0;

    // vraci retezec popisujici s cim se pracuje (napr. "file "test.txt"" nebo "directory "test""
    // nebo "3 files and 1 directory"); 'sourceDescr' je buffer pro vysledek o velikosti
    // minimalne 'sourceDescrSize'; 'panel' popisuje zdrojovy panel operace (jedna z PANEL_XXX nebo -1
    // pokud operace nema zdrojovy panel (napr. CPluginFSInterfaceAbstract::CopyOrMoveFromDiskToFS));
    // 'selectedFiles'+'selectedDirs' - pokud ma operace zdrojovy panel, je zde pocet oznacenych
    // souboru a adresaru ve zdrojovem panelu, pokud jsou obe hodnoty nulove, pracuje se se
    // souborem/adresarem pod kurzorem (fokusem); 'selectedFiles'+'selectedDirs' - pokud operace nema
    // zdrojovy panel, je zde pocet souboru/adresaru, se kterymi operace pracuje;
    // 'fileOrDirName'+'isDir' - pouziva se jen pokud operace nema zdrojovy panel a pokud
    // 'selectedFiles + selectedDirs == 1', je zde jmeno souboru/adresare a jestli jde o soubor
    // nebo adresar ('isDir' je FALSE nebo TRUE); 'forDlgCaption' je TRUE/FALSE pokud je/neni
    // text urceny pro titulek dialogu (v anglictine nutna velka pocatecni pismena)
    // omezeni: hlavni thread (muze pracovat s panelem)
    virtual void WINAPI GetCommonFSOperSourceDescr(char* sourceDescr, int sourceDescrSize,
                                                   int panel, int selectedFiles, int selectedDirs,
                                                   const char* fileOrDirName, BOOL isDir,
                                                   BOOL forDlgCaption) = 0;

    // nakopiruje retezec 'srcStr' za retezec 'dstStr' (za jeho koncovou nulu);
    // 'dstStr' je buffer o velikosti 'dstBufSize' (musi byt nejmene rovno 2);
    // pokud se do bufferu oba retezce nevejdou, jsou zkraceny (vzdy tak, aby se
    // veslo co nejvice znaku z obou retezu)
    // mozne volat z libovolneho threadu
    virtual void WINAPI AddStrToStr(char* dstStr, int dstBufSize, const char* srcStr) = 0;

    // zjisti jestli je mozne retezec 'fileNameComponent' pouzit jako komponentu
    // jmena na Windows filesystemu (resi retezce delsi nez MAX_PATH-4 (4 = "C:\"
    // + null-terminator), prazdny retezec, retezce znaku '.', retezce white-spaces,
    // znaky "*?\\/<>|\":" a jednoducha jmena typu "prn" a "prn  .txt")
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalIsValidFileNameComponent(const char* fileNameComponent) = 0;

    // pretvori retezec 'fileNameComponent' tak, aby mohl byt pouzity jako komponenta
    // jmena na Windows filesystemu (resi retezce delsi nez MAX_PATH-4 (4 = "C:\"
    // + null-terminator), resi prazdny retezec, retezce znaku '.', retezce
    // white-spaces, znaky "*?\\/<>|\":" nahradi '_' + jednoducha jmena typu "prn"
    // a "prn  .txt" doplni o '_' na konci nazvu); 'fileNameComponent' musi jit
    // rozsirit alespon o jeden znak (maximalne se vsak z 'fileNameComponent'
    // pouzije MAX_PATH bytu)
    // mozne volat z libovolneho threadu
    virtual void WINAPI SalMakeValidFileNameComponent(char* fileNameComponent) = 0;

    // vraci TRUE pokud je zdroj enumerace panel, v 'panel' pak vraci PANEL_LEFT nebo
    // PANEL_RIGHT; pokud nebyl zdroj enumerace nalezen nebo jde o Find okno, vraci FALSE;
    // 'srcUID' je unikatni identifikator zdroje (predava se jako parametr pri otevirani
    // vieweru nebo jej lze ziskat volanim GetPanelEnumFilesParams)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI IsFileEnumSourcePanel(int srcUID, int* panel) = 0;

    // vraci dalsi jmeno souboru pro viewer ze zdroje (levy/pravy panel nebo Findy);
    // 'srcUID' je unikatni identifikator zdroje (predava se jako parametr pri otevirani
    // vieweru nebo jej lze ziskat volanim GetPanelEnumFilesParams); 'lastFileIndex'
    // (nesmi byt NULL) je IN/OUT parametr, ktery by plugin mel menit jen pokud chce vratit
    // jmeno prvniho souboru, v tomto pripade nastavit 'lastFileIndex' na -1; pocatecni
    // hodnota 'lastFileIndex' se predava jako parametr jak pri otevirani vieweru, tak
    // pri volani GetPanelEnumFilesParams; 'lastFileName' je plne jmeno aktualniho souboru
    // (prazdny retezec pokud neni zname, napr. je-li 'lastFileIndex' -1); je-li
    // 'preferSelected' TRUE a aspon jedno jmeno oznacene, budou se vracet oznacena jmena;
    // je-li 'onlyAssociatedExtensions' TRUE, vraci jen soubory s priponou asociovanou s
    // viewerem tohoto pluginu (F3 na tomto souboru by se pokusilo otevrit viewer tohoto
    // pluginu + ignoruje pripadne zastineni viewerem jineho pluginu); 'fileName' je buffer
    // pro ziskane jmeno (velikost alespon MAX_PATH); vraci TRUE pokud se podari jmeno
    // ziskat; vraci FALSE pri chybe: zadne dalsi jmeno souboru ve zdroji neni (neni-li
    // 'noMoreFiles' NULL, vraci se v nem TRUE), zdroj je zaneprazdnen (nezpracovava zpravy;
    // neni-li 'srcBusy' NULL, vraci se v nem TRUE), jinak zdroj prestal existovat (zmena
    // cesty v panelu, atp.);
    // mozne volat z libovolneho threadu; POZOR: pouziti z hlavniho threadu nedava smysl
    // (Salamander je pri volani metody pluginu zaneprazdeny, takze vzdy vrati FALSE + TRUE
    // v 'srcBusy')
    virtual BOOL WINAPI GetNextFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                                 BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                                 char* fileName, BOOL* noMoreFiles, BOOL* srcBusy) = 0;

    // vraci predchozi jmeno souboru pro viewer ze zdroje (levy/pravy panel nebo Findy);
    // 'srcUID' je unikatni identifikator zdroje (predava se jako parametr pri otevirani
    // vieweru nebo jej lze ziskat volanim GetPanelEnumFilesParams); 'lastFileIndex' (nesmi
    // byt NULL) je IN/OUT parametr, ktery by plugin mel menit jen pokud chce vratit jmeno
    // posledniho souboru, v tomto pripade nastavit 'lastFileIndex' na -1; pocatecni hodnota
    // 'lastFileIndex' se predava jako parametr jak pri otevirani vieweru, tak pri volani
    // GetPanelEnumFilesParams; 'lastFileName' je plne jmeno aktualniho souboru (prazdny
    // retezec pokud neni zname, napr. je-li 'lastFileIndex' -1); je-li 'preferSelected'
    // TRUE a aspon jedno jmeno oznacene, budou se vracet oznacena jmena; je-li
    // 'onlyAssociatedExtensions' TRUE, vraci jen soubory s priponou asociovanou s viewerem
    // tohoto pluginu (F3 na tomto souboru by se pokusilo otevrit viewer tohoto
    // pluginu + ignoruje pripadne zastineni viewerem jineho pluginu); 'fileName' je buffer
    // pro ziskane jmeno (velikost alespon MAX_PATH); vraci TRUE pokud se podari jmeno
    // ziskat; vraci FALSE pri chybe: zadne predchozi jmeno souboru ve zdroji neni (neni-li
    // 'noMoreFiles' NULL, vraci se v nem TRUE), zdroj je zaneprazdnen (nezpracovava zpravy;
    // neni-li 'srcBusy' NULL, vraci se v nem TRUE), jinak zdroj prestal existovat (zmena
    // cesty v panelu, atp.)
    // mozne volat z libovolneho threadu; POZOR: pouziti z hlavniho threadu nedava smysl
    // (Salamander je pri volani metody pluginu zaneprazdeny, takze vzdy vrati FALSE + TRUE
    // v 'srcBusy')
    virtual BOOL WINAPI GetPreviousFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                                     BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                                     char* fileName, BOOL* noMoreFiles, BOOL* srcBusy) = 0;

    // zjisti, jestli je aktualni soubor z vieweru oznaceny (selected) ve zdroji (levy/pravy
    // panel nebo Findy); 'srcUID' je unikatni identifikator zdroje (predava se jako parametr
    // pri otevirani vieweru nebo jej lze ziskat volanim GetPanelEnumFilesParams); 'lastFileIndex'
    // je parametr, ktery by plugin nemel menit, pocatecni hodnota 'lastFileIndex' se predava
    // jako parametr jak pri otevirani vieweru, tak pri volani GetPanelEnumFilesParams;
    // 'lastFileName' je plne jmeno aktualniho souboru; vraci TRUE pokud se podarilo zjistit,
    // jestli je aktualni soubor oznaceny, vysledek je v 'isFileSelected' (nesmi byt NULL);
    // vraci FALSE pri chybe: zdroj prestal existovat (zmena cesty v panelu, atp.) nebo soubor
    // 'lastFileName' uz ve zdroji neni (pri techto dvou chybach, neni-li 'srcBusy' NULL,
    // vraci se v nem FALSE), zdroj je zaneprazdnen (nezpracovava zpravy; pri teto chybe,
    // neni-li 'srcBusy' NULL, vraci se v nem TRUE)
    // mozne volat z libovolneho threadu; POZOR: pouziti z hlavniho threadu nedava smysl
    // (Salamander je pri volani metody pluginu zaneprazdeny, takze vzdy vrati FALSE + TRUE
    // v 'srcBusy')
    virtual BOOL WINAPI IsFileNameForViewerSelected(int srcUID, int lastFileIndex,
                                                    const char* lastFileName,
                                                    BOOL* isFileSelected, BOOL* srcBusy) = 0;

    // nastavi oznaceni (selectionu) na aktualnim souboru z vieweru ve zdroji (levy/pravy
    // panel nebo Findy); 'srcUID' je unikatni identifikator zdroje (predava se jako parametr
    // pri otevirani vieweru nebo jej lze ziskat volanim GetPanelEnumFilesParams);
    // 'lastFileIndex' je parametr, ktery by plugin nemel menit, pocatecni hodnota
    // 'lastFileIndex' se predava jako parametr jak pri otevirani vieweru, tak pri volani
    // GetPanelEnumFilesParams; 'lastFileName' je plne jmeno aktualniho souboru; 'select'
    // je TRUE/FALSE pokud se ma aktualni soubor oznacit/odznacit; vraci TRUE pri uspechu;
    // vraci FALSE pri chybe: zdroj prestal existovat (zmena cesty v panelu, atp.) nebo
    // soubor 'lastFileName' uz ve zdroji neni (pri techto dvou chybach, neni-li 'srcBusy'
    // NULL, vraci se v nem FALSE), zdroj je zaneprazdnen (nezpracovava zpravy; pri teto
    // chybe, neni-li 'srcBusy' NULL, vraci se v nem TRUE)
    // mozne volat z libovolneho threadu; POZOR: pouziti z hlavniho threadu nedava smysl
    // (Salamander je pri volani metody pluginu zaneprazdeny, takze vzdy vrati FALSE + TRUE
    // v 'srcBusy')
    virtual BOOL WINAPI SetSelectionOnFileNameForViewer(int srcUID, int lastFileIndex,
                                                        const char* lastFileName, BOOL select,
                                                        BOOL* srcBusy) = 0;

    // vraci odkaz na sdilenou historii (posledne pouzite hodnoty) zvoleneho comboboxu;
    // jde o pole alokovanych retezcu; pole ma pevny pocet retezcu, ten se vraci
    // v 'historyItemsCount' (nesmi byt NULL); odkaz na pole se vraci v 'historyArr'
    // (nesmi byt NULL); 'historyID' (jedna v SALHIST_XXX) urcuje, na kterou sdilenou historii se ma vracet
    // odkaz
    // omezeni: hlavni thread (sdilene historie se v jinem threadu nedaji pouzivat, pristup
    // do nich neni nijak synchronizovany)
    virtual BOOL WINAPI GetStdHistoryValues(int historyID, char*** historyArr, int* historyItemsCount) = 0;

    // prida do sdilene historie ('historyArr'+'historyItemsCount') naalokovanou kopii
    // nove hodnoty 'value'; je-li 'caseSensitiveValue' TRUE, hleda se hodnota (retezec)
    // v poli historie pomoci case-sensitive porovnani (FALSE = case-insensitive porovnani),
    // nalezena hodnota se pouze presouva na prvni misto v poli historie
    // omezeni: hlavni thread (sdilene historie se v jinem threadu nedaji pouzivat, pristup
    // do nich neni nijak synchronizovany)
    // POZNAMKA: pokud se pouziva pro jine nez sdilene historie, je mozne volat v libovolnem threadu
    virtual void WINAPI AddValueToStdHistoryValues(char** historyArr, int historyItemsCount,
                                                   const char* value, BOOL caseSensitiveValue) = 0;

    // prida do comboboxu ('combo') texty ze sdilene historie ('historyArr'+'historyItemsCount');
    // pred pridavanim provede reset obsahu comboboxu (viz CB_RESETCONTENT)
    // omezeni: hlavni thread (sdilene historie se v jinem threadu nedaji pouzivat, pristup
    // do nich neni nijak synchronizovany)
    // POZNAMKA: pokud se pouziva pro jine nez sdilene historie, je mozne volat v libovolnem threadu
    virtual void WINAPI LoadComboFromStdHistoryValues(HWND combo, char** historyArr, int historyItemsCount) = 0;

    // zjisti barevnou hloubku aktualniho zobrazeni a je-li vice nez 8-bit (256 barev), vraci TRUE
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI CanUse256ColorsBitmap() = 0;

    // zkontroluje jestli je enablovany-root-parent okna 'parent' foreground window, pokud ne,
    // udela se FlashWindow(root-parent okna 'parent', TRUE) a vrati root-parent okna 'parent',
    // jinak se vraci NULL
    // POUZITI:
    //    HWND mainWnd = GetWndToFlash(parent);
    //    CDlg(parent).Execute();
    //    if (mainWnd != NULL) FlashWindow(mainWnd, FALSE);  // pod W2K+ uz asi neni potreba: flashovani se musi odstranit rucne
    // mozne volat z libovolneho threadu
    virtual HWND WINAPI GetWndToFlash(HWND parent) = 0;

    // provede reaktivaci drop-targetu (po dropnuti pri drag&dropu) po otevreni naseho progress-
    // -okna (to se pri otevreni aktivuje, cimz deaktivuje drop-target); neni-li 'dropTarget'
    // NULL a zaroven nejde o panel v tomto Salamanderovi, provede aktivaci 'progressWnd' a nasledne
    // aktivaci nejvzdalenejsiho enablovaneho predka 'dropTarget' (tato kombinace nas zbavi aktivovaneho
    // stavu bez aktivni aplikace, ktery jinak obcas vznika)
    // mozne volat z libovolneho threadu
    virtual void WINAPI ActivateDropTarget(HWND dropTarget, HWND progressWnd) = 0;

    // naplanuje otevreni Pack dialogu s vybranym packerem z tohoho pluginu (viz
    // CSalamanderConnectAbstract::AddCustomPacker), pokud packer z tohoto pluginu
    // neexistuje (napr. protoze ho uzivatel smazal), vypise se userovi chybova
    // hlaska; dialog se otevre jakmile nebudou v message-queue hl. threadu zadne
    // zpravy a Salamander nebude "busy" (nebude otevreny zadny modalni dialog
    // a nebude se zpracovavat zadna zprava); opakovane volani teto metody pred
    // otevrenim Pack dialogu vede jen ke zmene parametru 'delFilesAfterPacking';
    // 'delFilesAfterPacking' ovlivnuje checkbox "Delete files after packing"
    // v Pack dialogu: 0=default, 1=zapnuty, 2=vypnuty
    // omezeni: hlavni thread
    virtual void WINAPI PostOpenPackDlgForThisPlugin(int delFilesAfterPacking) = 0;

    // naplanuje otevreni Unpack dialogu s vybranym unpackerem z tohoho pluginu (viz
    // CSalamanderConnectAbstract::AddCustomUnpacker), pokud unpacker z tohoto pluginu
    // neexistuje (napr. protoze ho uzivatel smazal), vypise se userovi chybova
    // hlaska; dialog se otevre jakmile nebudou v message-queue hl. threadu zadne
    // zpravy a Salamander nebude "busy" (nebude otevreny zadny modalni dialog
    // a nebude se zpracovavat zadna zprava); opakovane volani teto metody pred
    // otevrenim Unpack dialogu vede jen ke zmene parametru 'unpackMask';
    // 'unpackMask' ovlivnuje masku "Unpack files": NULL=default, jinak text masky
    // omezeni: hlavni thread
    virtual void WINAPI PostOpenUnpackDlgForThisPlugin(const char* unpackMask) = 0;

    // vytvoreni souboru se jmenem 'fileName' pres klasicke volani Win32 API
    // CreateFile (lpSecurityAttributes==NULL, dwCreationDisposition==CREATE_NEW,
    // hTemplateFile==NULL); tato metoda resi kolizi 'fileName' s dosovym nazvem
    // jiz existujiciho souboru/adresare (jen pokud nejde i o kolizi s dlouhym
    // jmenem souboru/adresare) - zajisti zmenu dosoveho jmena tak, aby se soubor se
    // jmenem 'fileName' mohl vytvorit (zpusob: docasne prejmenuje konfliktni
    // soubor/adresar na jine jmeno a po vytvoreni 'fileName' ho prejmenuje zpet);
    // vraci handle souboru nebo pri chybe INVALID_HANDLE_VALUE (vraci v 'err'
    // (neni-li NULL) kod Windows chyby)
    // mozne volat z libovolneho threadu
    virtual HANDLE WINAPI SalCreateFileEx(const char* fileName, DWORD desiredAccess, DWORD shareMode,
                                          DWORD flagsAndAttributes, DWORD* err) = 0;

    // vytvoreni adresare se jmenem 'name' pres klasicke volani Win32 API
    // CreateDirectory(lpSecurityAttributes==NULL); tato metoda resi kolizi 'name'
    // s dosovym nazvem jiz existujiciho souboru/adresare (jen pokud nejde i o
    // kolizi s dlouhym jmenem souboru/adresare) - zajisti zmenu dosoveho jmena
    // tak, aby se adresar se jmenem 'name' mohl vytvorit (zpusob: docasne prejmenuje
    // konfliktni soubor/adresar na jine jmeno a po vytvoreni 'name' ho
    // prejmenuje zpet); dale resi jmena koncici na mezery (umi je vytvorit, narozdil
    // od CreateDirectory, ktera mezery bez varovani orizne a vytvori tak vlastne
    // jiny adresar); vraci TRUE pri uspechu, pri chybe FALSE (vraci v 'err'
    // (neni-li NULL) kod Windows chyby)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI SalCreateDirectoryEx(const char* name, DWORD* err) = 0;

    // umozni odpojit/pripojit sledovani zmen (jen pro windows cesty a cesty do archivu)
    // na cestach prohlizenych v jednom z panelu; ucel: pokud vasemu kodu (formatovani
    // disku, shredovani disku, atp.) prekazi to, ze panel ma pro cestu otevreny handle
    // "ChangeNotification", touto metodou jej muzete docasne odpojit (po pripojeni se
    // vyvolava refresh pro cestu v panelu); 'panel' je jeden z PANEL_XXX; 'stopMonitoring'
    // je TRUE/FALSE (odpojeni/pripojeni)
    // omezeni: hlavni thread
    virtual void WINAPI PanelStopMonitoring(int panel, BOOL stopMonitoring) = 0;

    // alokuje novy objekt CSalamanderDirectory pro praci se soubory/adresari v archivu nebo
    // file-systemu; je-li 'isForFS' TRUE, je objekt prednastaven pro pouziti pro file-system,
    // jinak je objekt prednastaven pro pouziti pro archiv (defaultni flagy objektu se
    // lisi pro archiv a file-system, viz metoda CSalamanderDirectoryAbstract::SetFlags)
    // mozne volat z libovolneho threadu
    virtual CSalamanderDirectoryAbstract* WINAPI AllocSalamanderDirectory(BOOL isForFS) = 0;

    // uvolni objekt CSalamanderDirectory (ziskany metodou AllocSalamanderDirectory,
    // POZOR: nesmi se volat pro zadny jiny ukazatel CSalamanderDirectoryAbstract)
    // mozne volat z libovolneho threadu
    virtual void WINAPI FreeSalamanderDirectory(CSalamanderDirectoryAbstract* salDir) = 0;

    // prida novy timer pro objekt pluginoveho FS; az dojde k timeoutu timeru, zavola se metoda
    // CPluginFSInterfaceAbstract::Event() objektu pluginoveho FS 'timerOwner' s parametry
    // FSE_TIMER a 'timerParam'; 'timeout' je timeout timeru od jeho pridani (v milisekundach,
    // musi byt >= 0); timer se zrusi v okamziku sveho timeoutu (pred volanim
    // CPluginFSInterfaceAbstract::Event()) nebo pri zavreni objektu pluginoveho FS;
    // vraci TRUE, pokud byl timer uspesne pridan
    // omezeni: hlavni thread
    virtual BOOL WINAPI AddPluginFSTimer(int timeout, CPluginFSInterfaceAbstract* timerOwner,
                                         DWORD timerParam) = 0;

    // zrusi bud vsechny timery objektu pluginoveho FS 'timerOwner' (je-li 'allTimers' TRUE)
    // nebo jen vsechny timery s parametrem rovnym 'timerParam' (je-li 'allTimers' FALSE);
    // vraci pocet zrusenych timeru
    // omezeni: hlavni thread
    virtual int WINAPI KillPluginFSTimer(CPluginFSInterfaceAbstract* timerOwner, BOOL allTimers,
                                         DWORD timerParam) = 0;

    // zjistuje viditelnost polozky pro FS v Change Drive menu a v Drive barach; vraci TRUE,
    // pokud je polozka viditelna, jinak vraci FALSE
    // omezeni: hlavni thread (jinak muze dojit ke zmenam v konfiguraci pluginu behem volani)
    virtual BOOL WINAPI GetChangeDriveMenuItemVisibility() = 0;

    // nastavuje viditelnost polozky pro FS v Change Drive menu a v Drive barach; pouzivat
    // jen pri instalaci pluginu (jinak hrozi prenastaveni uzivatelem zvolene viditelnosti);
    // 'visible' je TRUE v pripade, ze polozka ma byt viditelna
    // omezeni: hlavni thread (jinak muze dojit ke zmenam v konfiguraci pluginu behem volani)
    virtual void WINAPI SetChangeDriveMenuItemVisibility(BOOL visible) = 0;

    // Nastavuje breakpoint na x-tou COM/OLE alokaci. Slouzi k dohledani COM/OLE leaku.
    // V release verzi Salamandera nedela nic. Debug verze Salamandera pri svem ukonceni
    // zobrazuje do Debug okna debuggeru a do Trace Serveru seznam COM/OLE leaku.
    // V hranatych zavorkach je poradi alokace, kterou predame jako 'alloc' do volani
    // OleSpySetBreak. Lze zavolat z libovolneho threadu.
    virtual void WINAPI OleSpySetBreak(int alloc) = 0;

    // Vraci kopie ikon, ktere Salamander pouziva v panelech. 'icon' urcuje ikonu a jde
    // o jednu z hodnot SALICON_xxx. 'iconSize' urcuje jakou ma mit vracena ikona velikost
    // a jde o jednu z hodnot SALICONSIZE_xxx.
    // V pripade uspechu vraci handle vytvorene ikony. Destrukci ikony musi zajistit
    // plugin pomoci volani API DestroyIcon. V pripade neuspechu vraci NULL.
    // omezeni: hlavni thread
    virtual HICON WINAPI GetSalamanderIcon(int icon, int iconSize) = 0;

    // GetFileIcon
    //   Function retrieves handle to large or small icon from the specified object,
    //   such as a file, a folder, a directory, or a drive root.
    //
    // Parameters
    //   'path'
    //      [in] Pointer to a null-terminated string that contains the path and file
    //      name. If the 'pathIsPIDL' parameter is TRUE, this parameter must be the
    //      address of an ITEMIDLIST (PIDL) structure that contains the list of item
    //      identifiers that uniquely identify the file within the Shell's namespace.
    //      The PIDL must be a fully qualified PIDL. Relative PIDLs are not allowed.
    //
    //   'pathIsPIDL'
    //      [in] Indicate that 'path' is the address of an ITEMIDLIST structure rather
    //      than a path name.
    //
    //   'hIcon'
    //      [out] Pointer to icon handle that receive handle to the icon extracted
    //      from the object.
    //
    //   'iconSize'
    //      [in] Required size of icon. SALICONSIZE_xxx
    //
    //   'fallbackToDefIcon'
    //      [in] Value specifying whether the default (simple) icon should be used if
    //      the icon of the specified object is not available. If this parameter is
    //      TRUE, function tries to return the default (simple) icon in this situation.
    //      Otherwise, it returns no icon (return value is FALSE).
    //
    //   'defIconIsDir'
    //      [in] Specifies whether the default (simple) icon for 'path' is icon of
    //      directory. This parameter is ignored unless 'fallbackToDefIcon' is TRUE.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // Remarks
    //   You are responsible for freeing returned icons with DestroyIcon when you
    //   no longer need them.
    //
    //   You must initialize COM with CoInitialize or OLEInitialize prior to
    //   calling GetFileIcon.
    //
    //   Method can be called from any thread.
    //
    virtual BOOL WINAPI GetFileIcon(const char* path, BOOL pathIsPIDL,
                                    HICON* hIcon, int iconSize, BOOL fallbackToDefIcon,
                                    BOOL defIconIsDir) = 0;

    // FileExists
    //   Function checks the existence of a file. It returns TRUE if the specified
    //   file exists. If the file does not exist, it returns 0. FileExists only checks
    //   the existence of files, directories are ignored.
    // lze volat z libovolneho threadu
    virtual BOOL WINAPI FileExists(const char* fileName) = 0;

    // provede zmenu cesty v panelu na posledni znamou diskovou cestu, pokud neni pristupna,
    // tak se provede zmena na uzivatelem zvolenou "zachranou" cestu (viz
    // SALCFG_IFPATHISINACCESSIBLEGOTO) a pokud i ta selze, tak na root prvniho lokalniho
    // fixed drivu (Salamander 2.5 a 2.51 dela jen zmenu na root prvniho lokalniho fixed drivu);
    // pouziva se pro zavreni file-systemu v panelu (disconnect); 'parent' je parent pripadnych
    // messageboxu; 'panel' je jeden z PANEL_XXX
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract a CPluginDataInterfaceAbstract
    // (hrozi napr. zavreni FS otevreneho v panelu - metode by mohl prestat existovat 'this')
    virtual void WINAPI DisconnectFSFromPanel(HWND parent, int panel) = 0;

    // vraci TRUE, pokud je nazev souboru 'name' asociovan v Archives Associations in Panels
    // k volajicimu pluginu
    // 'name' musi byt pouze nazev souboru, ne s plnou nebo relativni cestou
    // omezeni: hlavni thread
    virtual BOOL WINAPI IsArchiveHandledByThisPlugin(const char* name) = 0;

    // slouzi jako LR_xxx parametr pro API funkci LoadImage()
    // pokud uzivatel nema zapnute hi-color ikony v konfiguraci desktopu,
    // vraci LR_VGACOLOR, aby nedoslo k chybnemu nacteni vice barevne verze ikony
    // jinak vraci 0 (LR_DEFAULTCOLOR); vysledek funkce lze orovat s dalsimi LR_xxx flagy
    // lze volat z libovolneho threadu
    virtual DWORD WINAPI GetIconLRFlags() = 0;

    // zjisti podle pripony souboru, jestli jde o link ("lnk", "pif" nebo "url"); 'fileExtension'
    // je pripona souboru (ukazatel za tecku), nesmi byt NULL; vraci 1 pokud jde o link, jinak
    // vraci 0; POZNAMKA: pouziva se pro plneni CFileData::IsLink
    // lze volat z libovolneho threadu
    virtual int WINAPI IsFileLink(const char* fileExtension) = 0;

    // vrati ILC_COLOR??? podle verze Windows - odladene pro pouziti imagelistu v listviewech
    // typicke pouziti: ImageList_Create(16, 16, ILC_MASK | GetImageListColorFlags(), ???, ???)
    // lze volat z libovolneho threadu
    virtual DWORD WINAPI GetImageListColorFlags() = 0;

    // "bezpecna" verze GetOpenFileName()/GetSaveFileName() resi situaci, kdy podana cesta
    // v OPENFILENAME::lpstrFile neni platna (napriklad z:\); v tomto pripade std. API verze
    // funkce neotevre okenko a tise se vrati z FALSE a CommDlgExtendedError() vraci FNERR_INVALIDFILENAME.
    // Nasledujici dve funkce v tomto pripade zavolaji API jeste jednou, ale s "bezpecne"
    // existujici cestou (Documents, pripadne Desktop).
    virtual BOOL WINAPI SafeGetOpenFileName(LPOPENFILENAME lpofn) = 0;
    virtual BOOL WINAPI SafeGetSaveFileName(LPOPENFILENAME lpofn) = 0;

    // plugin musi pred pouzitim OpenHtmlHelp() zadat Salamanderovi jmeno sveho .chm souboru
    // bez cesty (napr. "demoplug.chm")
    // lze volat z libovolneho threadu, ale je potreba vyloucit soucasne volani s OpenHtmlHelp()
    virtual void WINAPI SetHelpFileName(const char* chmName) = 0;

    // otevre HTML help pluginu, jazyk helpu (adresar s .chm soubory) vybira takto:
    // -adresar ziskany z aktualniho .slg souboru Salamandera (viz SLGHelpDir v shared\versinfo.rc)
    // -HELP\ENGLISH\*.chm
    // -prvni nalezeny podadresar v podadresari HELP
    // plugin musi pred pouzitim OpenHtmlHelp() zavolat SetHelpFileName(); 'parent' je parent
    // messageboxu s chybou; 'command' je prikaz HTML helpu, viz HHCDisplayXXX; 'dwData' je parametr
    // prikazu HTML helpu, viz HHCDisplayXXX
    // lze volat z libovolneho threadu
    // poznamka: zobrazeni helpu Salamandera viz OpenHtmlHelpForSalamander
    virtual BOOL WINAPI OpenHtmlHelp(HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData,
                                     BOOL quiet) = 0;

    // vraci TRUE, pokud jsou cesty 'path1' a 'path2' ze stejneho svazku; v 'resIsOnlyEstimation'
    // (neni-li NULL) vraci TRUE, pokud neni vysledek jisty (jisty je jen v pripade shody cest nebo
    // pokud se podari ziskat "volume name" (GUID svazku) u obou cest, coz pripada v uvahu jen pro
    // lokalni cesty pod W2K nebo novejsimi z rady NT)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI PathsAreOnTheSameVolume(const char* path1, const char* path2,
                                                BOOL* resIsOnlyEstimation) = 0;

    // realokace pameti na heapu Salamandera (pri pouziti salrtl9.dll zbytecne - staci klasicky realloc);
    // pri nedostatku pameti zobrazi uzivateli hlaseni s tlacitky Retry a Cancel (po dalsim dotazu
    // terminuje aplikaci)
    // mozne volat z libovolneho threadu
    virtual void* WINAPI Realloc(void* ptr, int size) = 0;

    // vraci v 'enumFilesSourceUID' (nesmi byt NULL) unikatni identifikator zdroje pro panel
    // 'panel' (jeden z PANEL_XXX), pouziva se ve viewerech pri enumeraci souboru
    // z panelu (viz parametr 'srcUID' napr. v metode GetNextFileNameForViewer), tento
    // identifikator se meni napr. pri zmene cesty v panelu; neni-li 'enumFilesCurrentIndex'
    // NULL, vraci se v nem index fokusleho souboru (pokud neni fokusly soubor, vraci se -1);
    // omezeni: hlavni thread (jinak se muze obsah panelu menit)
    virtual void WINAPI GetPanelEnumFilesParams(int panel, int* enumFilesSourceUID,
                                                int* enumFilesCurrentIndex) = 0;

    // postne panelu s aktivnim FS 'modifiedFS' zpravu o tom, ze by se mel
    // provest refresh cesty (znovu nacte listing a prenese oznaceni, ikony, fokus, atd. do
    // noveho obsahu panelu); refresh se provede az dojde k aktivaci hlavniho okna Salamandera
    // (az skonci suspend-mode); FS cesta se vzdycky nacte znovu; pokud 'modifiedFS' neni v zadnem
    // panelu, neprovede se nic; je-li 'focusFirstNewItem' TRUE a v panelu pribyla jen jedina
    // polozka, dojde k fokusu teto nove polozky (pouziva se napr. pro fokus nove vytvoreneho
    // souboru/adresare); vraci TRUE pokud se provedl refresh, FALSE pokud nebyl 'modifiedFS'
    // nalezen ani v jednom panelu
    // mozne volat z libovolneho threadu (pokud hlavni thread nespousti kod uvnitr pluginu,
    // probehne refresh co nejdrive, jinak refresh pocka minimalne do okamziku, kdy hlavni
    // thread opusti plugin)
    virtual BOOL WINAPI PostRefreshPanelFS2(CPluginFSInterfaceAbstract* modifiedFS,
                                            BOOL focusFirstNewItem = FALSE) = 0;

    // nacte z resourcu modulu 'module' text s ID 'resID'; vraci text v internim bufferu (hrozi
    // zmena textu diky zmene interniho bufferu zpusobene dalsimi volanimi LoadStr i z jinych
    // pluginu nebo Salamandera; buffer je velky 10000 znaku, prepis hrozi teprve po jeho
    // zaplneni (pouziva se cyklicky); pokud potrebujete text pouzit az pozdeji, doporucujeme
    // jej zkopirovat do lokalniho bufferu); je-li 'module' NULL nebo 'resID' neni v modulu,
    // vraci se text "ERROR LOADING STRING" (a debug/SDK verze vypise TRACE_E)
    // mozne volat z libovolneho threadu
    virtual char* WINAPI LoadStr(HINSTANCE module, int resID) = 0;

    // nacte z resourcu modulu 'module' text s ID 'resID'; vraci text v internim bufferu (hrozi
    // zmena textu diky zmene interniho bufferu zpusobene dalsimi volanimi LoadStrW i z jinych
    // pluginu nebo Salamandera; buffer je velky 10000 znaku, prepis hrozi teprve po jeho
    // zaplneni (pouziva se cyklicky); pokud potrebujete text pouzit az pozdeji, doporucujeme
    // jej zkopirovat do lokalniho bufferu); je-li 'module' NULL nebo 'resID' neni v modulu,
    // vraci se text L"ERROR LOADING WIDE STRING" (a debug/SDK verze vypise TRACE_E)
    // mozne volat z libovolneho threadu
    virtual WCHAR* WINAPI LoadStrW(HINSTANCE module, int resID) = 0;

    // zmena cesty v panelu na uzivatelem zvolenou "zachranou" cestu (viz
    // SALCFG_IFPATHISINACCESSIBLEGOTO) a pokud i ta selze, tak na root prvniho lokalniho fixed
    // drivu, jde o temer jistou zmenu aktualni cesty v panelu; 'panel' je jeden z PANEL_XXX;
    // neni-li 'failReason' NULL, nastavuje se na jednu z konstant CHPPFR_XXX (informuje o vysledku
    // metody); vraci TRUE pokud se zmena cesty podarila (na "zachranou" nebo fixed drive)
    // omezeni: hlavni thread + mimo metody CPluginFSInterfaceAbstract a CPluginDataInterfaceAbstract
    // (hrozi napr. zavreni FS otevreneho v panelu - metode by mohl prestat existovat 'this')
    virtual BOOL WINAPI ChangePanelPathToRescuePathOrFixedDrive(int panel, int* failReason = NULL) = 0;

    // prihlasi plugin jako nahradu za Network polozku v Change Drive menu a v Drive barach,
    // plugin musi pridavat do Salamandera file-system, na kterem se pak oteviraji nekompletni
    // UNC cesty ("\\" a "\\server") z prikazu Change Directory a na ktery se odchazi
    // pres symbol up-diru ("..") z rootu UNC cest;
    // omezeni: volat jen z entry-pointu pluginu a to az po SetBasicPluginData
    virtual void WINAPI SetPluginIsNethood() = 0;

    // otevre systemove kontextove menu pro oznacene polozky nebo fokuslou polozku na sitove ceste
    // ('forItems' je TRUE) nebo pro sitovou cestu ('forItems' je FALSE), vybrany prikaz z menu
    // take provede; menu se ziskava prochazenim slozky CSIDL_NETWORK; 'parent' je navrzeny parent
    // kontextoveho menu; 'panel' identifikuje panel (PANEL_LEFT nebo PANEL_RIGHT), pro ktery se
    // ma kontextove menu otevrit (z tohoto panelu se ziskavaji fokusle/oznacene soubory/adresare,
    // se kterymi se pracuje); 'menuX' + 'menuY' jsou navrzene souradnice leveho horniho rohu
    // kontextoveho menu; 'netPath' je sitova cesta, povolene jsou jen "\\" a "\\server"; neni-li
    // 'newlyMappedDrive' NULL, vraci se v nem pismenko ('A' az 'Z') nove namapovaneho disku (pres
    // prikaz Map Network Drive z kontextoveho menu), pokud se v nem vraci nula, k zadnemu novemu
    // mapovani nedoslo
    // omezeni: hlavni thread
    virtual void WINAPI OpenNetworkContextMenu(HWND parent, int panel, BOOL forItems, int menuX,
                                               int menuY, const char* netPath,
                                               char* newlyMappedDrive) = 0;

    // zdvojuje '\\' - hodi se pro texty, ktere posilame do LookForSubTexts, ktera '\\\\'
    // zase zredukuje na '\\'; 'buffer' je vstupne/vystupni retezec, 'bufferSize' je velikost
    // 'buffer' v bytech; vraci TRUE pokud zdvojenim nedoslo ke ztrate znaku z konce retezce
    // (buffer byl dost veliky)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI DuplicateBackslashes(char* buffer, int bufferSize) = 0;

    // ukaze v panelu 'panel' throbber (animace informujici uzivatele o aktivite souvisejici
    // s panelem, napr. "nacitam data ze site") se zpozdenim 'delay' (v ms); 'panel' je jeden
    // z PANEL_XXX; neni-li 'tooltip' NULL, jde o text, ktery se ukaze po najeti mysi na
    // throbber (je-li NULL, zadny text se neukazuje); pokud je uz v panelu throbber zobrazeny
    // nebo ceka na zobrazeni, zmeni se jeho identifikacni cislo a tooltip (je-li zobrazeny,
    // 'delay' se ignoruje, ceka-li na zobrazeni, nastavi se nove zpozdeni podle 'delay');
    // vraci identifikacni cislo throbberu (nikdy neni -1, tedy -1 je mozne pouzit jako
    // prazdnou hodnotu + vsechna vracena cisla jsou unikatni, presneji receno opakovat se
    // zacnou po nerealnych 2^32 zobrazeni throbberu);
    // POZNAMKA: vhodne misto pro zobrazeni throbberu pro FS je prijem udalosti FSE_PATHCHANGED,
    // to uz je FS v panelu (jestli se ma nebo nema throbber zobrazit se muze urcit predem
    // v ChangePath nebo ListCurrentPath)
    // omezeni: hlavni thread
    virtual int WINAPI StartThrobber(int panel, const char* tooltip, int delay) = 0;

    // schova throbber s identifikacnim cislem 'id'; vraci TRUE pokud dojde ke schovani
    // throbberu; vraci FALSE pokud se jiz tento throbber schoval nebo se pres nej ukazal
    // jiny throbber;
    // POZNAMKA: throbber se automaticky schovava tesne pred zmenou cesty v panelu nebo
    // pred refreshem (u FS to znamena tesne po uspesnem volani ListCurrentPath, u archivu
    // je to po otevreni a vylistovani archivu, u disku je to po overeni pristupnosti cesty)
    // omezeni: hlavni thread
    virtual BOOL WINAPI StopThrobber(int id) = 0;

    // ukaze v panelu 'panel' ikonu zabezpeceni (zamknuty nebo odemknuty zamek, napr. u FTPS informuje
    // uzivatele o tom, ze je spojeni se serverem zabezpecene pomoci SSL a identita serveru je
    // overena (zamknuty zamek) nebo overena neni (odemknuty zamek)); 'panel' je jeden z PANEL_XXX;
    // je-li 'showIcon' TRUE, ikona se ukaze, jinak se schova; 'isLocked' urcuje, jestli jde
    // o zamknuty (TRUE) nebo odemknuty (FALSE) zamek; neni-li 'tooltip' NULL, jde o text, ktery se
    // ukaze po najeti mysi na ikonu (je-li NULL, zadny text se neukazuje); pokud se ma po kliknuti
    // na ikone zabezpeceni provest nejaka akce (napr. u FTPS se zobrazuje dialog s certifikatem
    // serveru), je nutne ji pridat do metody CPluginFSInterfaceAbstract::ShowSecurityInfo file-systemu
    // zobrazeneho v panelu;
    // POZNAMKA: vhodne misto pro zobrazeni ikony zabezpeceni pro FS je prijem udalosti
    // FSE_PATHCHANGED, to uz je FS v panelu (jestli se ma nebo nema ikona zobrazit se muze urcit
    // predem v ChangePath nebo ListCurrentPath)
    // POZNAMKA: ikona zabezpeceni se automaticky schovava tesne pred zmenou cesty v panelu nebo
    // pred refreshem (u FS to znamena tesne po uspesnem volani ListCurrentPath, u archivu
    // je to po otevreni a vylistovani archivu, u disku je to po overeni pristupnosti cesty)
    // omezeni: hlavni thread
    virtual void WINAPI ShowSecurityIcon(int panel, BOOL showIcon, BOOL isLocked,
                                         const char* tooltip) = 0;

    // odstrani aktualni cestu v panelu z historie adresaru zobrazenych v panelu (Alt+Left/Right)
    // a ze seznamu pracovnich cest (Alt+F12); pouziva se pro zneviditelneni prechodnych cest,
    // napr. "net:\Entire Network\Microsoft Windows Network\WORKGROUP\server\share" automaticky
    // prechazi na "\\server\share" a je nezadouci, aby se tento prechod delal pri pohybu v historii
    // omezeni: hlavni thread
    virtual void WINAPI RemoveCurrentPathFromHistory(int panel) = 0;

    // vracit TRUE, pokud je aktualni uzivatel clenem skupiny Administrators, jinak vraci FALSE
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI IsUserAdmin() = 0;

    // vraci TRUE, pokud Salamander bezi na vzdalene plose (RemoteDesktop), jinak vraci FALSE
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI IsRemoteSession() = 0;

    // ekvivalent volani WNetAddConnection2(lpNetResource, NULL, NULL, CONNECT_INTERACTIVE);
    // vyhodou je podrobnejsi zobrazeni chybovych stavu (napr. ze expirovalo heslo,
    // ze je spatne zadane heslo nebo jmeno, ze je potreba zmenit heslo, atd.)
    // mozne volat z libovolneho threadu
    virtual DWORD WINAPI SalWNetAddConnection2Interactive(LPNETRESOURCE lpNetResource) = 0;

    //
    // GetMouseWheelScrollChars
    //   An OS independent method to retrieve the number of wheel scroll chars.
    //
    // Return Values
    //   Number of scroll characters where WHEEL_PAGESCROLL (0xffffffff) indicates to scroll a page at a time.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual DWORD WINAPI GetMouseWheelScrollChars() = 0;

    //
    // GetSalamanderZLIB
    //   Provides simplified interface to the ZLIB library provided by Salamander,
    //   for details see spl_zlib.h.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual CSalamanderZLIBAbstract* WINAPI GetSalamanderZLIB() = 0;

    //
    // GetSalamanderPNG
    //   Provides interface to the PNG library provided by Salamander.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual CSalamanderPNGAbstract* WINAPI GetSalamanderPNG() = 0;

    //
    // GetSalamanderCrypt
    //   Provides interface to encryption libraries provided by Salamander,
    //   for details see spl_crypt.h.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual CSalamanderCryptAbstract* WINAPI GetSalamanderCrypt() = 0;

    // informuje Salamandera o tom, ze plugin pouziva Password Manager a tedy Salamander ma
    // pluginu hlasit zavedeni/zmenu/zruseni master passwordu (viz
    // CPluginInterfaceAbstract::PasswordManagerEvent)
    // omezeni: volat jen z entry-pointu pluginu a to az po SetBasicPluginData
    virtual void WINAPI SetPluginUsesPasswordManager() = 0;

    //
    // GetSalamanderPasswordManager
    //   Provides interface to Password Manager provided by Salamander.
    //
    // Remarks
    //   Method can be called from main thread only.
    virtual CSalamanderPasswordManagerAbstract* WINAPI GetSalamanderPasswordManager() = 0;

    // otevre HTML help samotneho Salamanadera (misto help pluginu, ktery se otevira pomoci OpenHtmlHelp()),
    // jazyk helpu (adresar s .chm soubory) vybira takto:
    // -adresar ziskany z aktualniho .slg souboru Salamandera (viz SLGHelpDir v shared\versinfo.rc)
    // -HELP\ENGLISH\*.chm
    // -prvni nalezeny podadresar v podadresari HELP
    // 'parent' je parent messageboxu s chybou; 'command' je prikaz HTML helpu, viz HHCDisplayXXX;
    // 'dwData' je parametr prikazu HTML helpu, viz HHCDisplayXXX; pokud je command==HHCDisplayContext,
    // musi byt hodnota 'dwData' z rodiny konstant HTMLHELP_SALID_XXX
    // lze volat z libovolneho threadu
    virtual BOOL WINAPI OpenHtmlHelpForSalamander(HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData, BOOL quiet) = 0;

    //
    // GetSalamanderBZIP2
    //   Provides simplified interface to the BZIP2 library provided by Salamander,
    //   for details see spl_bzip2.h.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual CSalamanderBZIP2Abstract* WINAPI GetSalamanderBZIP2() = 0;

    //
    // GetFocusedItemMenuPos
    //   Returns point (in screen coordinates) where the context menu for focused item in the
    //   active panel should be displayed. The upper left corner of the panel is returned when
    //   focused item is not visible
    //
    // Remarks
    //   Method can be called from main thread only.
    virtual void WINAPI GetFocusedItemMenuPos(POINT* pos) = 0;

    //
    // LockMainWindow
    //   Locks main window to pretend it is disabled. Main windows is still able to receive focus
    //   in the locked state. Set 'lock' to TRUE to lock main window and to FALSE to revert it back
    //   to normal state. 'hToolWnd' is reserverd parameter, set it to NULL. 'lockReason' is (optional,
    //   can be NULL) describes the reason for main window locked state. It will be displayed during
    //   attempt to close locked main window; content of string is copied to internal structure
    //   so buffer can be deallocated after return from LockMainWindow().
    //
    // Remarks
    //   Method can be called from main thread only.
    virtual void WINAPI LockMainWindow(BOOL lock, HWND hToolWnd, const char* lockReason) = 0;

    // jen pro pluginy "dynamic menu extension" (viz FUNCTION_DYNAMICMENUEXT):
    // nastavi volajicimu pluginu priznak, ze se ma pri nejblizsi mozne prilezitosti
    // (jakmile nebudou v message-queue hl. threadu zadne message a Salamander nebude
    // "busy" (nebude otevreny zadny modalni dialog a nebude se zpracovavat zadna zprava))
    // znovu sestavit menu volanim metody CPluginInterfaceForMenuExtAbstract::BuildMenu;
    // POZOR: pokud se vola z jineho nez hlavniho threadu, muze dojit k volani BuildMenu
    // (probiha v hlavnim threadu) dokonce drive nez skonci PostPluginMenuChanged
    // mozne volat z libovolneho threadu
    virtual void WINAPI PostPluginMenuChanged() = 0;

    //
    // GetMenuItemHotKey
    //   Search through plugin's menu items added with AddMenuItem() for item with 'id'.
    //   When such item is found, its 'hotKey' and 'hotKeyText' (up to 'hotKeyTextSize' characters)
    //   is set. Both 'hotKey' and 'hotKeyText' could be NULL.
    //   Returns TRUE when item with 'id' is found, otherwise returns FALSE.
    //
    //   Remarks
    //   Method can be called from main thread only.
    virtual BOOL WINAPI GetMenuItemHotKey(int id, WORD* hotKey, char* hotKeyText, int hotKeyTextSize) = 0;

    // nase varianty funkci RegQueryValue a RegQueryValueEx, narozdil od API variant
    // zajistuji pridani null-terminatoru pro typy REG_SZ, REG_MULTI_SZ a REG_EXPAND_SZ
    // POZOR: pri zjistovani potrebne velikosti bufferu vraci o jeden nebo dva (dva
    //        jen u REG_MULTI_SZ) znaky vic pro pripad, ze by string bylo potreba
    //        zakoncit nulou/nulami
    // mozne volat z libovolneho threadu
    virtual LONG WINAPI SalRegQueryValue(HKEY hKey, LPCSTR lpSubKey, LPSTR lpData, PLONG lpcbData) = 0;
    virtual LONG WINAPI SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                                           LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) = 0;

    // protoze windowsova verze GetFileAttributes neumi pracovat se jmeny koncicimi mezerou,
    // napsali jsme si vlastni (u techto jmen pridava backslash na konec, cimz uz pak
    // GetFileAttributes funguje spravne, ovsem jen pro adresare, pro soubory s mezerou na
    // konci reseni nemame, ale aspon se to nezjistuje od jineho souboru - windowsova verze
    // orizne mezery a pracuje tak s jinym souborem/adresarem)
    // mozne volat z libovolneho threadu
    virtual DWORD WINAPI SalGetFileAttributes(const char* fileName) = 0;

    // zatim neexistuje Win32 API pro detekci SSD, takze se jejich detekovani provadi heuristikou
    // na zaklade dotazu na podporu pro TRIM, StorageDeviceSeekPenaltyProperty, atd
    // funkce vraci TRUE, pokud disk na ceste 'path' vypada jako SSD; FALSE jindy
    // vysledek neni 100%, lide hlasi nefunkcnost algoritmu napriklad na SSD PCIe kartach:
    // http://stackoverflow.com/questions/23363115/detecting-ssd-in-windows/33359142#33359142
    // umi zjistit korektni udaje i pro cesty obsahujici substy a reparse pointy pod Windows
    // 2000/XP/Vista (Salamander 2.5 pracuje jen s junction-pointy); 'path' je cesta, pro kterou
    // zjistujeme informace; pokud cesta vede pres sitovou cestu, tise vraci FALSE
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI IsPathOnSSD(const char* path) = 0;

    // vraci TRUE, pokud jde o UNC cestu (detekuje oba formaty: \\server\share i \\?\UNC\server\share)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI IsUNCPath(const char* path) = 0;

    // nahradi substy v ceste 'resPath' jejich cilovymi cestami (prevod na cestu bez SUBST drive-letters);
    // 'resPath' musi ukazovat na buffer o minimalne 'MAX_PATH' znacich
    // vraci TRUE pri uspechu, FALSE pri chybe
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI ResolveSubsts(char* resPath) = 0;

    // volat jen pro cesty 'path', jejichz root (po odstraneni substu) je DRIVE_FIXED (jinde nema smysl hledat
    // reparse pointy); hledame cestu bez reparse pointu, vedouci na stejny svazek jako 'path'; pro cestu
    // obsahujici symlink vedouci na sitovou cestu (UNC nebo mapovanou) vracime jen root teto sitove cesty
    // (ani Vista neumi delat s reparse pointy na sitovych cestach, takze to je nejspis zbytecne drazdit);
    // pokud takova cesta neexistuje z duvodu, ze aktualni (posledni) lokalni reparse point je volume mount
    // point (nebo neznamy typ reparse pointu), vracime cestu k tomuto volume mount pointu (nebo reparse
    // pointu neznameho typu); pokud cesta obsahuje vice nez 50 reparse pointu (nejspis nekonecny cyklus),
    // vracime puvodni cestu;
    //
    // 'resPath' je buffer pro vysledek o velikosti MAX_PATH; 'path' je puvodni cesta; v 'cutResPathIsPossible'
    // (nesmi byt NULL) vracime FALSE pokud vysledna cesta v 'resPath' obsahuje na konci reparse point (volume
    // mount point nebo neznamy typ reparse pointu) a tudiz ji nesmime zkracovat (dostali bysme se tim nejspis
    // na jiny svazek); je-li 'rootOrCurReparsePointSet' neNULLove a obsahuje-li FALSE a na puvodni ceste je
    // aspon jeden lokalni reparse point (reparse pointy na sitove casti cesty ignorujeme), vracime v teto
    // promenne TRUE + v 'rootOrCurReparsePoint' (neni-li NULL) vracime plnou cestu k aktualnimu (poslednimu
    // lokalnimu) reparse pointu (pozor, ne kam vede); cilovou cestu aktualniho reparse pointu (jen je-li to
    // junction nebo symlink) vracime v 'junctionOrSymlinkTgt' (neni-li NULL) + typ vracime v 'linkType':
    // 2 (JUNCTION POINT), 3 (SYMBOLIC LINK); v 'netPath' (neni-li NULL) vracime sitovou cestu, na kterou
    // vede aktualni (posledni) lokalni symlink v ceste - v teto situaci se root sitove cesty vraci v 'resPath'
    // mozne volat z libovolneho threadu
    virtual void WINAPI ResolveLocalPathWithReparsePoints(char* resPath, const char* path,
                                                          BOOL* cutResPathIsPossible,
                                                          BOOL* rootOrCurReparsePointSet,
                                                          char* rootOrCurReparsePoint,
                                                          char* junctionOrSymlinkTgt, int* linkType,
                                                          char* netPath) = 0;

    // Provede resolve substu a reparse pointu pro cestu 'path', nasledne se pro mount-point cesty
    // (pokud chybi tak pro root cesty) pokusi ziskat GUID path. Pri neuspechu vrati FALSE. Pri
    // uspechu, vrati TRUE a nastavi 'mountPoint' a 'guidPath' (pokud jsou ruzne od NULL, musi
    // odkazovat na buffery o velikosti minimalne MAX_PATH; retezce budou zakonceny zpetnym lomitkem).
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI GetResolvedPathMountPointAndGUID(const char* path, char* mountPoint, char* guidPath) = 0;

    // nahradi v retezci posledni znak '.' decimalnim separatorem ziskanym ze systemu LOCALE_SDECIMAL
    // delka retezce muze narust, protoze separator muze mit podle MSDN az 4 znaky
    // vraci TRUE, pokud byl buffer dostatecne veliky a operaci se povedlo dokoncit, jinak vraci FALSE
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI PointToLocalDecimalSeparator(char* buffer, int bufferSize) = 0;

    // nastavi pro tento plugin pole icon-overlays; po nastaveni muze plugin v listingach vracet
    // index icon-overlaye (viz CFileData::IconOverlayIndex), ktery se ma zobrazit pres ikonu
    // polozky listingu, takto je mozne pouzit az 15 icon-overlays (indexy 0 az 14, protoze
    // index 15=ICONOVERLAYINDEX_NOTUSED aneb: nezobrazuj icon-overlay); 'iconOverlaysCount'
    // je pocet icon-overlays pro plugin; pole 'iconOverlays' obsahuje pro kazdy icon-overlay
    // postupne vsechny velikosti ikon: SALICONSIZE_16, SALICONSIZE_32 a SALICONSIZE_48 - tedy
    // v poli 'iconOverlays' je 3 * 'iconOverlaysCount' ikon; uvolneni ikon v poli 'iconOverlays'
    // zajisti Salamander (volani DestroyIcon()), samotne pole je vec volajiciho, pokud v poli
    // budou nejake NULL (napr. nezdaril se load ikony), funkce selze, ale platne ikony z pole
    // uvolni; pri zmene barev v systemu by mel plugin icon-overlays znovu nacist a znovu nastavit
    // touto funkci, idealni je reakce na PLUGINEVENT_COLORSCHANGED ve funkci
    // CPluginInterfaceAbstract::Event()
    // POZOR: pred Windows XP (ve W2K) je velikost ikony SALICONSIZE_48 jen 32 bodu!
    // omezeni: hlavni thread
    virtual void WINAPI SetPluginIconOverlays(int iconOverlaysCount, HICON* iconOverlays) = 0;

    // popis viz SalGetFileSize(), prvni rozdil je, ze se soubor zadava plnou cestou;
    // druhy je, ze 'err' muze byt NULL, pokud nestojime o kod chyby;
    virtual BOOL WINAPI SalGetFileSize2(const char* fileName, CQuadWord& size, DWORD* err) = 0;

    // zjisti velikost souboru, na ktery vede symlink 'fileName'; velikost vraci v 'size';
    // 'ignoreAll' je in + out, je-li TRUE vsechny chyby se ignoruji (pred akci je treba
    // priradit FALSE, jinak se okno s chybou vubec nezobrazi, pak uz nemenit);
    // pri chybe zobrazi standardni okno s dotazem Retry / Ignore / Ignore All / Cancel
    // s parentem 'parent'; pokud velikost uspesne zjisti, vraci TRUE; pri chybe a stisku
    // tlacitka Ignore / Igore All v okne s chybou, vraci FALSE a v 'cancel' vraci FALSE;
    // je-li 'ignoreAll' TRUE, okno se neukaze, na tlacitko se neceka, chova se jako by
    // uzivatel stiskl Ignore; pri chybe a stisku Cancel v okne s chybou vraci FALSE a
    // v 'cancel' vraci TRUE;
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI GetLinkTgtFileSize(HWND parent, const char* fileName, CQuadWord* size,
                                           BOOL* cancel, BOOL* ignoreAll) = 0;

    // smaze link na adresar (junction point, symbolic link, mount point); pri uspechu
    // vraci TRUE; pri chybe vraci FALSE a neni-li 'err' NULL, vraci kod chyby v 'err'
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI DeleteDirLink(const char* name, DWORD* err) = 0;

    // pokud ma soubor/adresar 'name' read-only atribut, pokusime se ho vypnout
    // (duvod: napr. aby sel smazat pres DeleteFile); pokud uz mame atributy 'name'
    // nactene, predame je v 'attr', je-li 'attr' -1, ctou se atributy 'name' z disku;
    // vraci TRUE pokud se provede pokus o zmenu atributu (uspech se nekontroluje);
    // POZNAMKA: vypina jen read-only atribut, aby v pripade vice hardlinku nedoslo
    // k zbytecne velke zmene atributu na zbyvajicich hardlinkach souboru (atributy
    // vsechny hardlinky sdili)
    // mozne volat z libovolneho threadu
    virtual BOOL WINAPI ClearReadOnlyAttr(const char* name, DWORD attr = -1) = 0;

    // zjisti, jestli prave probiha critical shutdown (nebo log off), pokud ano, vraci TRUE;
    // pri tomto shutdownu mame cas jen 5s na ulozeni konfigurace celeho programu
    // vcetne pluginu, takze casove narocnejsi operace musime vynechat, po uplynuti
    // 5s system nas process nasilne ukonci, vice viz WM_ENDSESSION, flag ENDSESSION_CRITICAL,
    // je to Vista+
    virtual BOOL WINAPI IsCriticalShutdown() = 0;

    // projde v threadu 'tid' (0 = aktualni) vsechna okna (EnumThreadWindows) a vsem enablovanym
    // a viditelnym dialogum (class name "#32770") vlastnenym oknem 'parent' postne WM_CLOSE;
    // pouziva se pri critical shutdown k odblokovani okna/dialogu, nad kterym jsou otevrene
    // modalni dialogy, hrozi-li vice vrstev, je nutne volat opakovane
    virtual void WINAPI CloseAllOwnedEnabledDialogs(HWND parent, DWORD tid = 0) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_gen)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
