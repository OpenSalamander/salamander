// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// text.h - slovak version

STRING(STR_TITLE, "Samorozbalovací ZIP archiv")
STRING(STR_ERROR, "Chyba")
STRING(STR_QUESTION, "Otázka")
STRING(STR_ERROR_FOPEN, "Nie je možné otvoriť archívny súbor: ")
STRING(STR_ERROR_FSIZE, "Nie je možné zistiť veľkosť archívu: ")
STRING(STR_ERROR_MAXSIZE, "Bola dosiahnutá maximálna veľkosť archívu")
STRING(STR_ERROR_FMAPPING, "Nie je možné namapovať archív do pamäti: ")
STRING(STR_ERROR_FVIEWING, "Nie je možné namapovať archív do pamäti: ")
STRING(STR_ERROR_NOEXESIG, "Nebola nájdená EXE signatura.")
STRING(STR_ERROR_NOPESIG, "Nebola nájdená PE signatura.")
STRING(STR_ERROR_INCOMPLETE, "Chyba v dátach - nekompletný súbor.")
STRING(STR_ERROR_SFXSIG, "Nebola nájdená signatura self-extractoru.")
STRING(STR_ERROR_TEMPPATH, "Nie je možné získať cestu adresára pre dočasné súbory: ")
STRING(STR_ERROR_TEMPNAME, "Nie je možné získať meno dočasného súboru: ")
STRING(STR_ERROR_DELFILE, "Nie je možné zmazať dočasný súbor: ")
STRING(STR_ERROR_MKDIR, "Nie je možné vytvoriť dočasný adresár: ")
STRING(STR_ERROR_GETATTR, "Nie je možné získať atribúty súboru: ")
STRING(STR_ERROR_GETCWD, "Nie je možné získať pracovný adresár: ")
STRING(STR_ERROR_BADDATA, "Chyba v skomprimovaných dátach.")
STRING(STR_ERROR_TOOLONGNAME, "Meno súboru je príliš dlhé.")
STRING(STR_LOWMEM, "Chyba, málo pamäti: ")
STRING(STR_NOTENOUGHTSPACE, "Zdá sa, že na cieľovom disku nie je dostatok miesta.\nPrajete si pokračovať?\n\nPožadované miesto: %s bytov.\nVoľné miesto: %s bytov.\n%s")
STRING(STR_ERROR_TCREATE, "Nie je možné vytvoriť nové vlákno: ")
STRING(STR_ERROR_METHOD, "Nie je možné vybaliť súbor. Súbor je skomprimovaný nepodporovanou metódou.")
STRING(STR_ERROR_CRC, "Chyba CRC.")
STRING(STR_ERROR_ACCES, "Nie je možné pracovať so súborom: ")
STRING(STR_ERROR_WRITE, "Nie je možné zapísať do súboru: ")
STRING(STR_ERROR_BREAK, "Prajete si prerušiť vybaľovanie súborov?")
STRING(STR_ERROR_DLG, "Nie je možné vytvoriť dialógové okno: ")
STRING(STR_ERROR_FCREATE, "Nie je možné vytvoriť alebo otvoriť súbor: ")
STRING(STR_ERROR_WAIT, "Zlyhalo čakanie na proces: ")
STRING(STR_ERROR_WAIT2, "\nStlačte OK, až bude možné bezpečne zmazať dočasné súbory.")
STRING(STR_ERROR_WAITTHREAD, "Zlyhalo čakanie na vlákno: ")
STRING(STR_ERROR_DCREATE, "Nie je možné vytvoriť nový adresár: ")
STRING(STR_ERROR_DCREATE2, "Nie je možné vytvoriť nový adresár: Názov je už použitý pre súbor.")
STRING(STR_ERROR_FCREATE2, "Nie je možné vytvoriť súbor: Názov je už použitý pre adresár.")
STRING(STR_TARGET_NOEXIST, "Cieľový adresár neexistuje, prajete si ho vytvoriť?")
STRING(STR_OVERWRITE_RO, "Prajete si prepísať súbor s atribútom LEN PRE ČÍTANIE, SYSTÉMOVÝ alebo SKRYTÝ?")
STRING(STR_STOP, "Skončiť")
STRING(STR_STATNOTCOMPLETE, "Žádné súbory neboly vybalené.")
STRING(STR_SKIPPED, "Počet preskočených súborov:\t\t%d\n")
STRING(STR_STATOK, "Rozbaľovanie bolo úspešne dokončené.\n\nPočet vybalených súborov:\t\t%d\n%sCelková veľkosť vybalených súborov:\t%s B  ")
STRING(STR_BROWSEDIRTITLE, "Vyberte adresár.")
STRING(STR_BROWSEDIRCOMMENT, "Vyberte adresár pre rozbalenie súborov.")
STRING(STR_OVERWRITEDLGTITLE, "Potvrďte prepísanie súboru")
STRING(STR_TEMPDIR, "&Zložka pre uloženie dočasných súborov:")
STRING(STR_EXTRDIR, "&Zložka pre rozbalenie súborov:")
STRING(STR_TOOLONGCOMMAND, "Nie je možné spustiť príkaz: meno súboru je príliš dlhé.")
STRING(STR_ERROR_EXECSHELL, "Nie je možné spustiť príkaz: ")
STRING(STR_ADVICE, "\nPoznámka: samorozbalovací archiv môžete spustiť s parametrom '-s', ktorý umožní výber zložky pre rozbalenie súborov")
STRING(STR_BADDRIVE, "Zadané meno disku alebo sieťového zdieľania je chybné.")
STRING(STR_ABOUT1, "&O programe >>")
STRING(STR_ABOUT2, "<< &O programe")
STRING(STR_OK, "OK")
STRING(STR_CANCEL, "Storno")
STRING(STR_YES, "Áno")
STRING(STR_NO, "Nie")
STRING(STR_AGREE, "Súhlasím")
STRING(STR_DISAGREE, "Nesúhlasím")
#ifndef EXT_VER
STRING(STR_ERROR_ENCRYPT, "Nie je možné vybaliť súbor. Súbor je zašifrovaný.")
STRING(STR_PARAMTERS, "Parametre pre spustenie z príkazového riadku:\n\n-s\tBezpečný mód. Zobrazí hlavné dialogové okienko\n\ta umožní zmenu zložky pre rozbalenie archívu.")
#else  //EXT_VER
STRING(STR_ERROR_TEMPCREATE, "Nie je možné vytvoriť dočasný súbor: ")
STRING(STR_ERROR_GETMODULENAME, "Nie je možné získať meno modulu: ")
STRING(STR_ERROR_SPAWNPROCESS, "Nie je možné spustiť nový proces: ")
STRING(STR_INSERT_LAST_DISK, "Vložte prosím poslednú disketu do mechaniky a stlačte OK.\n\n"
                             "Poznámka: Pre presné určenie posledného súboru archívnej sady\n"
                             "použite pri spustení parameter '-l' z príkazového riadku. Pre viac\n"
                             "informácií spustite samorozbalovací archiv s parametrom '-?'")
STRING(STR_INSERT_NEXT_DISK, "Vložte prosím disketu číslo %d.\na stlačte OK.")
STRING(STR_ERROR_GETWD, "Nie je možné zistiť adresár Windows: ")
STRING(STR_ADVICE2, "Poznámka: samorozbalovací archiv môžete spustiť s parametrom '-t', ktorý umožní výber zložky pre uloženie dočasných súborov.")
STRING(STR_PARAMTERS, "Parametre pre spustenie z príkazového riadku:\n\n"
                      "-s\t\tBezpečný mód. Zobrazí hlavné dialógové okno\n"
                      "\t\ta umožní zmenu zložky pre rozbalenie archívu.\n"
                      "-t [path]\t\tUloží dočasné súbory do zadanej zložky.\n"
                      "-a [archive]\tRozbalí zadaný archív.\n"
                      "-l [archive]\tRovnaké ako -a, ale predpokladá, že zadaný archív\n"
                      "\t\tje posledný súbor archívu\n\n"
                      "Poznámka: Dlhé názvy súborov musia byť uzavreté v úvodzovkách.")
STRING(STR_ERROR_READMAPPED, "Nie je možné čítať z archívneho súboru.")
STRING(STR_PASSWORDDLGTITLE, "Súbor je zašifrovaný")
#endif //EXT_VER

//STRING(STR_NOTSTARTED, "")
