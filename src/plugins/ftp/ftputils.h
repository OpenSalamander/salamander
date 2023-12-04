// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* FTP_ANONYMOUS; // standardni jmeno pro anonymniho uzivatele
// standardni port FTP (21) je definovan v konstante: IPPORT_FTP

// konstanty typu cest na FTP serveru pro funkci GetFTPServerPathType
enum CFTPServerPathType
{
    ftpsptEmpty,   // prazdna hodnota (zatim vubec nebyl vyhodnocen)
    ftpsptUnknown, // neodpovida zadnemu z nasledujicich typu cest
    ftpsptUnix,    // napr. /pub/altap/salamand (ale i /\dir-with-backslash)
    ftpsptNetware, // napr. /pub/altap/salamand nebo \pub\altap\salamand
    ftpsptOpenVMS, // napr. PUB$DEVICE:[PUB.VMS] nebo [PUB.VMS]  (pojmenovano "OpenVMS" aby se nepletlo s "MVS")
    ftpsptMVS,     // napr. 'VEA0016.MAIN.CLIST'
    ftpsptWindows, // napr. /pub/altap/salamand nebo \pub\altap\salamand
    ftpsptIBMz_VM, // napr. ACADEM:ANONYMOU.PICS nebo ACADEM:ANONYMOU. (root)
    ftpsptOS2,     // napr. C:/DIR1/DIR2 nebo C:\DIR1\DIR2
    ftpsptTandem,  // napr. \SYSTEM.$VVVVV.SUBVOLUM.FILENAME
    ftpsptAS400,   // napr. /QSYS.LIB/GARY.LIB (nebo /QDLS/oetst)
};

// zjisti typ cesty na FTP serveru; neni-li 'serverFirstReply' NULL, jde o prvni odpoved
// serveru (casto v ni je verze serveru); neni-li 'serverSystem' NULL, jde o odpoved FTP
// serveru na prikaz SYST (nas ftpcmdSystem); 'path' je cesta na FTP serveru; vraci typ cesty
CFTPServerPathType GetFTPServerPathType(const char* serverFirstReply, const char* serverSystem,
                                        const char* path);

// vyparsuje z odpovedi serveru na prikaz SYST ulozene v 'serverSystem' jmeno systemu
// a ulozi ho do 'sysName' (buffer aspon 201 znaku); pokud neni mozne zjistit z teto
// odpovedi jmeno systemu, vraci prazdny retezec
void FTPGetServerSystem(const char* serverSystem, char* sysName);

// zkracuje cestu na FTP serveru o posledni adresar/soubor (oddelovace adresaru jsou zavisle
// na typu cesty - 'type'), 'path' je in/out buffer (min. velikost 'pathBufSize' bytu,
// pozor u VMS cest se muze stat, ze je treba do 'path' zapsat vic znaku, nez je
// delka 'path' - pri nedostatku mista v 'path' se retezec orizne), v 'cutDir'
// (buffer o min. velikosti 'cutDirBufSize') se vraci posledni adresar (odriznuta
// cast; pokud se retezec nevejde do bufferu, je oriznut); neni-li 'fileNameCouldBeCut'
// NULL, vraci se v nem TRUE pokud odriznuta cast muze byt jmeno souboru; vraci TRUE
// pokud doslo ke zkraceni (neslo o root cestu a 'type' je znamy typ cesty)
BOOL FTPCutDirectory(CFTPServerPathType type, char* path, int pathBufSize,
                     char* cutDir, int cutDirBufSize, BOOL* fileNameCouldBeCut);

// spoji cestu 'path' a 'name' (jmeno souboru/adresare - 'isDir' je FALSE/TRUE) do 'path',
// zajisti spojeni podle typu cesty - 'type'; 'path' je buffer alespon 'pathSize' znaku;
// vraci TRUE pokud se 'name' veslo do 'path'; je-li 'path' nebo 'name' prazdne, ke
// spojeni nedojde (muze dojit k uprave cesty 'path' - oriznuti na minimalni delku - napr.
// "/pub/" + "" = "/pub")
BOOL FTPPathAppend(CFTPServerPathType type, char* path, int pathSize, const char* name, BOOL isDir);

// zjisti jestli je cesta na FTP serveru (ne userpart cesta) 'path' platna a jestli nejde
// o root cestu (detekce podle typu cesty - 'type'); vraci TRUE je-li 'path' platna a neni
// to root cesta
BOOL FTPIsValidAndNotRootPath(CFTPServerPathType type, const char* path);

// prevede escape sekvence (napr. "%20" = " ") v retezci 'txt' na ASCII znaky
void FTPConvertHexEscapeSequences(char* txt);
// pripravi text 'txt' tak, aby bez uhony prezil naslednou konverzi escape sekvenci na ASCII
// znaky (napr. "%20" = "%2520"); 'txtSize' je velikost bufferu 'txt'; vraci FALSE je-li buffer
// prilis maly pro uspesnou konverzi
BOOL FTPAddHexEscapeSequences(char* txt, int txtSize);

// rozdeli user-part cestu na jednotlive casti (jmeno uzivatele, hostitele, port a cestu (bez '/'
// nebo '\\' na zacatku)); vlozi do retezce cesty 'p' nuly tak, aby byly jednotlive casti nulou
// ukoncene retezce; neni-li 'firstCharOfPath' NULL a obsahuje-li cesta na FTP cestu v ramci
// serveru (retezec 'path'), vlozi se do 'firstCharOfPath' oddelovac teto cesty ('/' nebo '\\');
// nemusi vracet vsechny casti ('user', 'host', 'port' i 'path' muzou byt NULL); pokud se
// nepodari ziskat danou cast cesty (cesta ji nemusi obsahovat), vraci se v prislusne promenne
// NULL; 'user', 'host' a 'port' vraci orezane o mezery z obou stran; 'userLength' je
// nula pokud netusime, jak dlouhe je uzivatelske jmeno nebo pokud v nem nejsou "zakazane"
// znaky, jinak je to ocekavana delka uzivatelskeho jmena; format cesty:
// "//user:password@host:port/path" nebo jen "user:password@host:port/path" (+ "/path" muze byt
// i "\path") + "user:password@", ":password" a ":port" muzou byt vynechane
void FTPSplitPath(char* p, char** user, char** password, char** host, char** port,
                  char** path, char* firstCharOfPath, int userLength);

// vraci delku username pro pouziti v parametrech "userLength" (FTPSplitPath, FTPFindPath, atd.);
// pro anonymniho usera a dalsi usernamy bez specialnich znaku ('@', '/', '\\', ':') vraci nulu;
// 'user' muze byt i NULL
int FTPGetUserLength(const char* user);

// vraci ukazatel na remote-path v FTP ceste (ukazatel do bufferu 'path'); 'path' je cesta
// ve formatu "//user:password@host:port/remotepath" nebo jen "user:password@host:port/remotepath"
// ("user:password@", ":password" a ":port" muzou byt vynechane); 'userLength' je
// nula pokud netusime, jak dlouhe je uzivatelske jmeno nebo pokud v nem nejsou "zakazane"
// znaky, jinak je to ocekavana delka uzivatelskeho jmena; pokud nenajde remote-path, vraci
// ukazatel na konec retezce 'path'
const char* FTPFindPath(const char* path, int userLength);

// podle typu cesty vraci ukazatel na remote-path v ceste (ukazatel do bufferu 'path');
// slouzi k preskoceni/nechani uvodniho slashe/backslashe v ceste; 'path' je cesta formatu
// "/remotepath" nebo "\remotepath" (cast cesty za hostem v user-part ceste); 'type' je
// typ cesty
const char* FTPGetLocalPath(const char* path, CFTPServerPathType type);

// porovnava dve cesty na FTP serveru (ne user-part cesty), vraci TRUE pokud jsou stejne;
// 'type' je typ aspon jedne z cest
BOOL FTPIsTheSameServerPath(CFTPServerPathType type, const char* p1, const char* p2);

// zjistuje, jestli je 'prefix' prefix 'path' - obe cesty jsou na FTP serveru (nejde o user-part
// cesty), vraci TRUE pokud jde o prefix; 'type' je typ aspon jedne z cest; je-li 'mustBeSame'
// TRUE, musi se 'prefix' a 'path' shodovat (shodna funkce s FTPIsTheSameServerPath())
BOOL FTPIsPrefixOfServerPath(CFTPServerPathType type, const char* prefix, const char* path,
                             BOOL mustBeSame = FALSE);

// porovna dve user-part cesty na FTP, vraci TRUE pokud jsou stejne; je-li 'sameIfPath2IsRelative'
// TRUE, vraci TRUE i pokud se 'p1' a 'p2' shodujici jen v user+host+port a 'p2' neobsahuje cestu
// v ramci FTP serveru (napr. "ftp://petr@localhost"); 'type' je typ aspon jedne z cest;
// 'userLength' je nula pokud netusime, jak dlouhe je uzivatelske jmeno nebo pokud v nem nejsou
// "zakazane" znaky, jinak je to ocekavana delka uzivatelskeho jmena
BOOL FTPIsTheSamePath(CFTPServerPathType type, const char* p1, const char* p2,
                      BOOL sameIfPath2IsRelative, int userLength);

// porovna rooty dvou user-part cest na FTP, vraci TRUE pokud jsou stejne;
// 'userLength' je nula pokud netusime, jak dlouhe je uzivatelske jmeno nebo pokud v nem nejsou
// "zakazane" znaky, jinak je to ocekavana delka uzivatelskeho jmena
BOOL FTPHasTheSameRootPath(const char* p1, const char* p2, int userLength);

// vraci popis chyby
char* FTPGetErrorText(int err, char* buf, int bufSize);

// vraci podle typu cesty jeden znak, ktery se pouziva k oddelovani komponent cest (podadresaru)
char FTPGetPathDelimiter(CFTPServerPathType pathType);

// jen pro typ cest ftpsptIBMz_VM: ziska root cestu z cesty 'path'; 'root'+'rootSize' je buffer
// pro vysledek; vraci uspech
BOOL FTPGetIBMz_VMRootPath(char* root, int rootSize, const char* path);

// jen pro typ cest ftpsptOS2: ziska root cestu z cesty 'path'; 'root'+'rootSize' je buffer
// pro vysledek; vraci uspech
BOOL FTPGetOS2RootPath(char* root, int rootSize, const char* path);

// ziska ciselnou hodnotu z retezce UNIXovych prav 'rights'; hodnotu vraci v actAttr (nesmi byt NULL);
// najde-li prava nenastavitelna pomoci "site chmod" ('s','t', atd.), naORuje prislusne
// bity do 'attrDiff' (nesmi byt NULL); pokud jde o UNIXova prava, vraci TRUE, jinak vraci
// FALSE (neznamy retezec prav nebo napr. ACL prava na UNIXU (napr. "drwxrwxr-x+"))
BOOL GetAttrsFromUNIXRights(DWORD* actAttr, DWORD* attrDiff, const char* rights);

// prevede 'attrs' (ciselna prava na UNIXu) na retezec UNIXovych prav (bez prvniho
// pismene) do bufferu 'buf' o velikosti 'bufSize'
void GetUNIXRightsStr(char* buf, int bufSize, DWORD attrs);

// vraci TRUE, pokud jsou 'rights' prava UNIXoveho linku; UNIXova prava = musi mit 10 znaku
// nebo 11 pokud konci na '+' (ACL prava); format prav linku: 'lrw?rw?rw?' + misto 'r' a 'w'
// muze byt '-'
BOOL IsUNIXLink(const char* rights);

// stejna funkce jako FTPGetErrorText, jen zajistuje na konci retezce CRLF;
// POZOR: 'bufSize' musi byt vetsi nez 2
void FTPGetErrorTextForLog(DWORD err, char* errBuf, int bufSize);

// metoda pro detekovani jestli jiz je v bufferu 'readBytes' (platnych 'readBytesCount'
// bytu, cte se od pozice 'readBytesOffset') cela odpoved od FTP serveru; vraci TRUE
// pri uspechu - v 'reply' (nesmi byt NULL) vraci ukazatel na zacatek odpovedi,
// v 'replySize' (nesmi byt NULL) vraci delku odpovedi, v 'replyCode' (neni-li NULL)
// vraci FTP kod odpovedi nebo -1 pokud odpoved nema zadny kod (nezacina na triciferne
// cislo); pokud jeste neni odpoved kompletni, vraci FALSE
BOOL FTPReadFTPReply(char* readBytes, int readBytesCount, int readBytesOffset,
                     char** reply, int* replySize, int* replyCode);

// z retezce 'reply' vyparsuje adresar (pravidla viz RFC 959 - FTP odpoved cislo "257");
// nevyzaduje striktne "257" na zacatku retezce (je-li nutne, osetrit pred volanim);
// vraci TRUE pokud se podarilo adresar ziskat
// mozne volat z libovolneho threadu
BOOL FTPGetDirectoryFromReply(const char* reply, int replySize, char* dirBuf, int dirBufSize);

// z retezce 'reply' o delce 'replySize' (-1 == pouzit 'strlen(reply)') vyparsuje IP+port
// (vraci v 'ip'+'port' (nesmi byt NULL); pravidla viz RFC 959 - FTP odpoved cislo 227);
// nevyzaduje striktne "227" na zacatku retezce (je-li nutne, osetrit pred volanim);
// vraci TRUE pokud se podarilo IP+port ziskat
// mozne volat z libovolneho threadu
BOOL FTPGetIPAndPortFromReply(const char* reply, int replySize, DWORD* ip, unsigned short* port);

// z retezce 'reply' o delce 'replySize' vyparsuje velikost dat a vraci je v 'size';
// pri uspechu vraci TRUE, jinak FALSE a 'size' nahodne nastavene
BOOL FTPGetDataSizeInfoFromSrvReply(CQuadWord& size, const char* reply, int replySize);

// vytvori VMS jmeno adresare (prida ".DIR;1")
void FTPMakeVMSDirName(char* vmsDirNameBuf, int vmsDirNameBufSize, const char* dirName);

// overi jestli je v ceste 'pathBeginning' escape sekvence pred znakem 'checkedChar'
// (napr. "^." je '.', kterou VMS nepovazuje za oddelovac cesty ani pripony, atd.);
// vraci TRUE pokud je pred znakem escape sekvence (aneb znak nema specialni vyznam)
BOOL FTPIsVMSEscapeSequence(const char* pathBeginning, const char* checkedChar);

// vraci TRUE pokud cesta 'path' typu 'type' konci oddelovacem komponent cesty
// (napr. "/pub/dir/" nebo "PUB$DEVICE:[PUB.VMS.]")
BOOL FTPPathEndsWithDelimiter(CFTPServerPathType type, const char* path);

// zkracuje IBM z/VM cestu na FTP serveru o posledni dve komponenty, 'path' je in/out
// buffer (min. velikost 'pathBufSize' bytu), v 'cutDir' (buffer o min. velikosti
// 'cutDirBufSize') se vraci posledni dve komponenty (odriznuta cast; pokud se retezec
// nevejde do bufferu, je oriznut); vraci TRUE pokud doslo ke zkraceni
BOOL FTPIBMz_VmCutTwoDirectories(char* path, int pathBufSize, char* cutDir, int cutDirBufSize);

// orezava cislo verze souboru ze jmena souboru na OpenVMS (napr. "a.txt;1" -> "a.txt");
// 'name' je jmeno; 'nameLen' je delka 'name' (-1 = delka neni znama, pouzit strlen(name));
// vraci TRUE pokud doslo k oriznuti
BOOL FTPVMSCutFileVersion(char* name, int nameLen);

// vraci TRUE pokud je 'path' relativni cesta na ceste typu 'pathType';
// POZOR: napr. "[pub]" na VMS nebo "/dir" na OS/2 jsou pro tuto funkci
// absolutni cesty, i kdyz na plnou absolutni cestu se musi doplnit
// volanim funkce FTPCompleteAbsolutePath
BOOL FTPIsPathRelative(CFTPServerPathType pathType, const char* path);

// vraci prvni podadresar na relativni ceste a zkrati o nej tuto relativni cestu;
// 'path' je relativni cesta na ceste typu 'pathType'; 'cut' je buffer pro
// oriznuty prvni podadresar o velikosti 'cutBufSize'; vraci TRUE pokud se
// oriznuti povedlo (po oriznuti posledniho podadresare z cesty je v 'path'
// prazdny retezec); POZOR: neumi pracovat se jmeny souboru (napr. VMS: chybne
// rozdeli "[.a]b.c;1" na "a", "b" a "c;1")
BOOL FTPCutFirstDirFromRelativePath(CFTPServerPathType pathType, char* path,
                                    char* cut, int cutBufSize);

// doplni absolutni cestu na plnou absolutni cestu (u VMS se doplnuje svazek - napr.
// "PUB$DEVICE:", u OS/2 disk - napr. "C:"); 'pathType' je typ cesty; 'path' je
// absolutni cesta; v 'path' (buffer alespon 'pathBufSize' znaku) vraci plnou
// absolutni cestu (nedostatek mista = oriznuty vysledek); 'workPath' je pracovni
// plna absolutni cesta (odkud se bere svazek/disk); vraci uspech (i v pripade, ze
// 'path' je plna absolutni cesta uz na vstupu - aneb neni co delat)
BOOL FTPCompleteAbsolutePath(CFTPServerPathType pathType, char* path, int pathBufSize,
                             const char* workPath);

// vypusti z cesty 'path' typu 'pathType' "." a ".." (tyka se jen vybranych typu
// cest, napr. na VMS tato funkce nic nedela); vraci FALSE jen pokud nelze vypustit
// ".." (jde o invalidni cestu: napr. "/..")
BOOL FTPRemovePointsFromPath(char* path, CFTPServerPathType pathType);

// vraci TRUE pokud je filesystem s typem cest 'pathType' case-sensitive (unix),
// jinak je case-insensitive (windows)
BOOL FTPIsCaseSensitive(CFTPServerPathType pathType);

// vraci TRUE pokud jde o chybovou odpoved na prikaz LIST, ktera rika jen, ze
// adresar je prazdny (proste nejde o chybu - VMS a Z/VM takove chyby nechyby
// bohuzel hlasi)
BOOL FTPIsEmptyDirListErrReply(const char* listErrReply);

// vraci TRUE, pokud jmeno 'name' muze byt jmeno jednoho souboru/adresare ('isDir' je
// FALSE/TRUE) na ceste 'path' typu 'pathType'; slouzi jen k zamezeni vytvoreni vice
// podadresaru misto jedineho (napr. "a.b.c" na VMS vytvori tri podadresare),
// syntaktickou chybu ve jmene vraci az server
BOOL FTPMayBeValidNameComponent(const char* name, const char* path, BOOL isDir,
                                CFTPServerPathType pathType);

// pro upload na server: prida k cilove ceste masku *.* nebo * (pro pozdejsi zpracovani
// operacni masky); 'pathType' je typ cesty; 'targetPath' je buffer (o velikosti
// 'targetPathBufSize') obsahujici na vstupu cilovou cestu (plnou, vcetne fs-name),
// na vystupu obohacenou o masku; 'noFilesSelected' je TRUE pokud se nemaji uploadit
// zadne soubory (v panelu jsou oznacene jen adresare)
void FTPAddOperationMask(CFTPServerPathType pathType, char* targetPath, int targetPathBufSize,
                         BOOL noFilesSelected);

// generuje nove jmeno pro jmeno 'originalName' (jde o soubor/adresar pokud je 'isDir'
// FALSE/TRUE) na ceste typu 'pathType'; 'phase' je IN/OUT faze generovani (0 = pocatecni
// faze, vraci -1 pokud jiz dalsi faze neni, jinak vraci cislo dalsi faze - pouzije se
// pri pripadnem nasledujicim volani teto funkce); 'newName' je buffer pro vygenerovane
// jmeno (velikost bufferu je MAX_PATH); 'index' je IN/OUT index jmena ve fazi 'phase'
// (pouziva se pro generovani dalsich jmen v jedne fazi), pri prvnim volani v ramci jedne
// faze je nula; 'alreadyRenamedFile' je TRUE jen pokud jde o soubor, ktery uz nejspis
// byl prejmenovan (zajistime "name (2)"->"name (3)" misto ->"name (2) (2)")
void FTPGenerateNewName(int* phase, char* newName, int* index, const char* originalName,
                        CFTPServerPathType pathType, BOOL isDir, BOOL alreadyRenamedFile);

// pro AS/400: pokud jde o jmeno formatu "???1.file/???2.mbr" (obe jmena ???1 a ???2 jsou shodna),
// nakopiruje do 'mbrName' (buffer o velikosti MAX_PATH) "???2.mbr", jinak nakopiruje
// "???1.???2.mbr"
void FTPAS400CutFileNamePart(char* mbrName, const char* name);

// pro AS/400: pokud jde o jmeno formatu "???.mbr", prepise 'name' (buffer o velikosti
// aspon 2*MAX_PATH) na "???.file/???.mbr" (obe jmena ??? jsou shodna), pokud jde o
// jmeno formatu "???1.???2.mbr", zapise do 'name' "???1.file.???2.mbr"
void FTPAS400AddFileNamePart(char* name);
