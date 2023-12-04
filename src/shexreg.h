// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

// #define ENABLE_SH_MENU_EXT     // je-li toto makro definovano, pripoji se i kontextove menu (nejen "copy hook")

//
// Shell Extensions Configuration
//

//
// ============================================= spolecna cast
//

//
// je soucasti Open Salamandera - pro ucely konfigurace + vzajemne komunikace
// a zaroven je soucasti SHELLEXT.DLL - pro ucely prezentace + vzajemne komunikace
//

//
// The class ID of this Shell extension class.
//
// salshext.dll (Servant Salamander 2.5 beta 1) class id:        c78b6130-f3ea-11d2-94a1-00e0292a01e3
// salexten.dll (Servant Salamander 2.5 beta 2 az RC1) class id: c78b6131-f3ea-11d2-94a1-00e0292a01e3 (kopirovane do TEMP adresare, sdilene mezi vice verzemi Salama)
// salamext.dll (Servant Salamander 2.5 RC2) class id:           c78b6132-f3ea-11d2-94a1-00e0292a01e3 (prvni verze, ktera zustava v instalaci Salama + kazda verze Salama ma svoji vlastni shell-extensionu)
// salamext.dll (Servant Salamander 2.5 RC3) class id:           c78b6133-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.5 RC3) class id:             c78b6134-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.5) class id:                 c78b6135-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.51) class id:                c78b6136-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.52 beta 1) class id:         c78b6137-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.52 beta 1) class id:         c78b6138-f3ea-11d2-94a1-00e0292a01e3 (zmena vytvareni/otevirani mutexu vedla k nekompatibilite se starsima verzema)
// salamext.dll (Altap Salamander 2.52 beta 1) class id:         c78b6139-f3ea-11d2-94a1-00e0292a01e3 (vytvareni mutexu s omezenymi pravy vedlo k tomu, ze starsi verze Salamandera (napr. 2.51) mutex vubec neotevrely, takze jsem zmenil jmena mutexu, pameti, atd.)
// salamext.dll (Altap Salamander 2.52 beta 2) class id:         c78b613a-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.52) class id:                c78b613b-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.53 beta 1) class id:         c78b613c-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.53) class id:                c78b613d-f3ea-11d2-94a1-00e0292a01e3 (nepouzilo se, vydali jsme nakonec 2.53 beta 2)
// salamext.dll (Altap Salamander 2.53 beta 2) class id:         c78b613e-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.53) class id:                c78b613f-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.54) class id:                c78b6140-f3ea-11d2-94a1-00e0292a01e3
// salamext.dll (Altap Salamander 2.55 beta 1) class id:         c78b6141-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.0 beta 1) class id: c78b6142-f3ea-11d2-94a1-00e0292a01e3 (prvni verze, kde se pouzivaji x86+x64 verze soucasne)
// salextx86.dll+salextx64.dll (Salamander 3.0 beta 2) class id: c78b6143-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.0 beta 3) class id: c78b6144-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.0 beta 4) class id: c78b6145-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.0) class id:        c78b6146-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.01) class id:       c78b6147-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.02) class id:       c78b6148-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.03) class id:       c78b6149-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.04) class id:       c78b614a-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.05) class id:       c78b614b-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.06) class id:       c78b614c-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.07) class id:       c78b614d-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 3.08) class id:       c78b614e-f3ea-11d2-94a1-00e0292a01e3
// salextx86.dll+salextx64.dll (Salamander 4.0) class id:        c78b614f-f3ea-11d2-94a1-00e0292a01e3
//

DEFINE_GUID(CLSID_ShellExtension, 0xc78b614fL, 0xf3ea, 0x11d2, 0x94, 0xa1, 0x00, 0xe0, 0x29, 0x2a, 0x01, 0xe3);

//
// pripojovany retezec pro zajisteni unikatnosti jmena shell extensiony v registry (SHEXREG_OPENSALAMANDER)
//
// Servant Salamander 2.5 RC2                - 25RC2
// Servant Salamander 2.5 RC3                - 25RC3
// Altap Salamander 2.5 RC3                  - 25RC3
// Altap Salamander 2.5                      - 25
// Altap Salamander 2.51                     - 251
// Altap Salamander 2.52 beta 1              - 252B1
// Altap Salamander 2.52 beta 1              - 252B1a  (zmena vytvareni/otevirani mutexu vedla k nekompatibilite se starsima verzema)
// Altap Salamander 2.52 beta 1              - 252B1b  (vytvareni mutexu s omezenymi pravy vedlo k tomu, ze starsi verze Salamandera (napr. 2.51) mutex vubec neotevrely, takze jsem zmenil jmena mutexu, pameti, atd.)
// Altap Salamander 2.52 beta 2              - 252B2
// Altap Salamander 2.52                     - 252
// Altap Salamander 2.53 beta 1              - 253B1
// Altap Salamander 2.53                     - 253     (nepouzilo se, vydali jsme nakonec 2.53 beta 2)
// Altap Salamander 2.53 beta 2              - 253B2
// Altap Salamander 2.53                     - 253R
// Altap Salamander 2.54                     - 254
// Altap Salamander 2.55 beta 1              - 255B1   (nepouzilo se, vydali jsme nakonec 3.0 beta 1)
// Altap Salamander 3.0 beta 1               - 300B1
// Altap Salamander 3.0 beta 2               - 300B2
// Altap Salamander 3.0 beta 3               - 300B3
// Altap Salamander 3.0 beta 4               - 300B4
// Altap Salamander 3.0                      - 300B5   (omylem zustalo "3.0 beta 5", misto ktere jsme vydali 3.0)
// Altap Salamander 3.1 beta 1 (nevydana)    - 310B1
// Altap Salamander 3.01                     - 301
// Altap Salamander 3.1 beta 1 (nevydana)    - 310B1_2 (2. pokus o vydani "3.1 beta 1")
// Altap Salamander 3.02                     - 302
// Altap Salamander 3.1 beta 1 (nevydana)    - 310B1_3 (3. pokus o vydani "3.1 beta 1")
// Altap Salamander 3.03                     - 303
// Altap Salamander 3.1 beta 1 (nevydana)    - 310B1_4 (4. pokus o vydani "3.1 beta 1")
// Altap Salamander 3.04                     - 304
// Altap Salamander 3.1 beta 1               - 310B1_5 (5. pokus o vydani "3.1 beta 1")
// Altap Salamander 3.05                     - 305
// Altap Salamander 3.1 beta 1               - 310B1_6 (6. pokus o vydani "3.1 beta 1")
// Altap Salamander 3.06                     - 306
// Altap Salamander 3.1 beta 1               - 310B1_7 (7. pokus o vydani "3.1 beta 1")
// Altap Salamander 3.07                     - 307
// Altap Salamander 4.0 beta 1               - 400B1
// Altap Salamander 3.08                     - 308
// Altap Salamander 4.0 beta 1               - 400B1_2 (2. pokus o vydani "4.0 beta 1")
// Altap Salamander 4.0                      - 400
// Open Salamander 5.0                       - 500

#define SALSHEXT_SHAREDNAMESAPPENDIX "500"

#ifdef ENABLE_SH_MENU_EXT

#define SEC_NAMEMAX 400

#define SEC_SUBMENUNAME_MAX 100

typedef struct CShellExtConfigItem CShellExtConfigItem;

// zakladni polozka reprezentujici jeden radek v kontextovem menu
// tyto polozky jsou drzeny v jednosmerne svazanem seznamu
struct CShellExtConfigItem
{
    char Name[SEC_NAMEMAX]; // text, pod ktery polozka vystupuje v kontextovem menu
    // podminky, kdy se tato polozka zobrazi v kontextovem menu
    BOOL OneFile;
    BOOL OneDirectory;
    BOOL MoreFiles;
    BOOL MoreDirectories;
    BOOL LogicalAnd;

    // neukladana data
    CShellExtConfigItem* Next; // dalsi polozka; je-li rovna NULL, je toto posledni
    UINT Cmd;                  // slouzi pro zpetne nalezeni polozky pri InvokeCommand
};

// nuluje polozku
void SECClearItem(CShellExtConfigItem* item);

// zacatek seznamu
extern CShellExtConfigItem* ShellExtConfigFirst;

// polozky z konfigurace
extern BOOL ShellExtConfigSubmenu;
extern char ShellExtConfigSubmenuName[SEC_SUBMENUNAME_MAX];

// aktualni cislo verze konfigurace
// pri ukladani konfigurace je potreba toto cislo zvetsit o jednicku
// pri nacitani je potreba provest kontrolu verze a je-li jina nez prave
// pouzivana, je potreba znovu reloudnout registry
extern DWORD ShellExtConfigVersion;

// vrati polozku s danym indexem
CShellExtConfigItem* SECGetItem(int index);

// pokud najde polozku se shodnym Cmd, naplni index a vrati TRUE; jinak vrati FALSE
BOOL SECGetItemIndex(UINT cmd, int* index);

BOOL SECLoadRegistry();

// vytahne nazev polozky
const char* SECGetName(int index);

// vyhodi ze seznamu vsechny polozky
void SECDeleteAllItems();

// vrati index zalozene polozky nebo -1 pri chybe
// refItem vraci ukazatel na pridanou plozku, muze byt == NULL
int SECAddItem(CShellExtConfigItem** refItem);

#endif // ENABLE_SH_MENU_EXT

//
// Data a konstanty pro vzajemnou komunikaci mezi Salamanderem a SalamExt
//

// jmeno mutexu pro pristup do sdilene pameti (pouziva se pri otevirani mutextu pres OpenMutex,
// drive vytvoreneho pres CreateMutex)
extern const char* SALSHEXT_SHAREDMEMMUTEXNAME;
// jmeno sdilene pameti (pouziva se pri otevirani sdilene pameti pres OpenFileMapping, drive
// vytvorene pres CreateFileMapping)
extern const char* SALSHEXT_SHAREDMEMNAME;
// jmeno eventu pro zaslani zadosti o provedeni Paste ve zdrojovem Salamanderovi, pouziva se jen pod
// Vista+ (pro starsi OS staci postnout message WM_USER_SALSHEXT_PASTE primo z copy-hooku; pod Vista+
// tento post nefunguje pro "as admin" spusteneho Salamandera)
extern const char* SALSHEXT_DOPASTEEVENTNAME;

// POZOR: konstanty se nesmi zmenit (stare verze musi byt kompatibilni)
#define WM_USER_SALSHEXT_PASTE WM_APP + 139      // [postMsgIndex, 0] - SalamExt zada o provedeni prikazu Paste
#define WM_USER_SALSHEXT_TRYRELDATA WM_APP + 143 // [0, 0] - SalamExt hlasi odblokovani paste-dat (viz CSalShExtSharedMem::BlockPasteDataRelease), nejsou-li data dale chranena, nechame je zrusit

#define SALSHEXT_NONE 0
#define SALSHEXT_COPY 1
#define SALSHEXT_MOVE 2

// struktura sdilene pameti
#pragma pack(push)
#pragma pack(4)
struct CSalShExtSharedMem
{
    int Size; // velikost struktury (pro urceni verze + opatreni proti prepisum pameti)

    // sekce pro drag&drop
    BOOL DoDragDropFromSalamander;      // TRUE = DoDragDrop spusteny ze Salamandera (jen s "fake" adresarem)
    char DragDropFakeDirName[MAX_PATH]; // plne jmeno "fake" adresare pouzivaneho pro drag&drop
    BOOL DropDone;                      // TRUE = doslo k dropu, operace spustena, TargetPath+Operation jsou platne

    // sekce pro copy/cut + paste
    BOOL DoPasteFromSalamander;       // TRUE = paste data objektu ze Salamandera (jen s "fake" adresarem)
    DWORD ClipDataObjLastGetDataTime; // cas posledniho volani GetData data-objektu na clipboardu
    char PasteFakeDirName[MAX_PATH];  // plne jmeno "fake" adresare pouzivaneho pro paste
    DWORD SalamanderMainWndPID;       // process ID hl. okna Salamandera, ktery dal na clipboard pasted data objekt (SalamExt mu musi poslat prikaz k provedeni operace paste)
    DWORD SalamanderMainWndTID;       // thread ID hl. okna Salamandera, ktery dal na clipboard pasted data objekt (SalamExt mu musi poslat prikaz k provedeni operace paste)
    UINT64 SalamanderMainWnd;         // hl. okno Salamandera, ktery dal na clipboard pasted data objekt (SalamExt mu musi poslat prikaz k provedeni operace paste) - HWND je 64-bit na x64 (i kdyz udajne z nej pouzivaji jen spodnich 32-bitu), x86 verze hornich 32-bitu nuluje
    int PostMsgIndex;                 // index postnute zpravy WM_USER_SALSHEXT_PASTE, na jejiz zpracovani SalamExt ceka (po timeoutu cekani dojde ke zvyseni indexu -> Salamander pak zpravu (az dojde) preskoci)
    BOOL BlockPasteDataRelease;       // od W2K+ uz asi neni potreba: je-li TRUE, fakedataobj->Release() nezrusi paste-data v Salamanderovi
    int SalBusyState;                 // 0 = zjistuje se jestli Salamander neni "busy"; 1 = Salamander neni "busy" a uz ceka na zadani operace paste; 2 = Salamander je "busy", paste odlozime na pozdeji
    DWORD PastedDataID;               // identifikace pasted dat (slouzi k identifikaci dat pro paste uvnitr Salamandera - co se ma vlastne pastnout vi jen Salamander, zde je jen ID techto dat)
    BOOL PasteDone;                   // TRUE = paste operace spustena, TargetPath+Operation jsou platne
    char ArcUnableToPaste1[300];      // pripravena hlaska pro copy-hook pro chybu pri Paste (viz IDS_ARCUNABLETOPASTE1)
    char ArcUnableToPaste2[300];      // pripravena hlaska pro copy-hook pro chybu pri Paste (viz IDS_ARCUNABLETOPASTE2)

    // vysledna operace
    char TargetPath[2 * MAX_PATH]; // cilova cesta (kam vybalit soubory/adresare z archivu nebo nakopirovat soubory/adresare z file-systemu)
    int Operation;                 // SALSHEXT_COPY nebo SALSHEXT_MOVE (nebo SALSHEXT_NONE po inicializaci struktury)
};
typedef struct CSalShExtSharedMem CSalShExtSharedMem;
#pragma pack(pop)

//
// ============================================= pouze Altap Salamander
//

#ifdef INSIDE_SALAMANDER

// zanese do registry polozky potrebne pro chod knihovny
// parametry jsou cesta ke knihovne, FALSE/TRUE testovat/netestovat verzi DLL tim, ze ho
// loadneme, 0 nebo do jake casti registry (32-bit nebo 64-bit) se ma zapisovat
BOOL SECRegisterToRegistry(const char* shellExtensionPath, BOOL doNotLoadDLL, REGSAM regView);

#ifdef ENABLE_SH_MENU_EXT

// ulozi data do registry
BOOL SECSaveRegistry();

// vrati pocet polozek v seznamu
int SECGetCount();

// vyhodi ze seznamu polozku
BOOL SECDeleteItem(int index);

// prohodi dve polozky v seznamu
BOOL SECSwapItems(int index1, int index2);

// nastavi nazev polozky
BOOL SECSetName(int index, const char* name);

#endif // ENABLE_SH_MENU_EXT

#endif //INSIDE_SALAMANDER
