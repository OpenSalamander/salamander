// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// text.h - english version

STRING(STR_TITLE, "Samorozbalitelný ZIP archiv")
STRING(STR_ERROR, "Chyba")
STRING(STR_QUESTION, "Dotaz")
STRING(STR_ERROR_FOPEN, "Nelze otevřít archivní soubor: ")
STRING(STR_ERROR_FSIZE, "Nelze získat velikost archivu: ")
STRING(STR_ERROR_MAXSIZE, "Dosaženo maximální velikosti archivu")
STRING(STR_ERROR_FMAPPING, "Nelze namapovat archiv do paměti: ")
STRING(STR_ERROR_FVIEWING, "Nelze namapovat archiv do paměti: ")
STRING(STR_ERROR_NOEXESIG, "Nebyla nalezena EXE signatura.")
STRING(STR_ERROR_NOPESIG, "Nebyla nalezena PE signatura.")
STRING(STR_ERROR_INCOMPLETE, "Chyba v datech - nekompletní soubor.")
STRING(STR_ERROR_SFXSIG, "Nebyla nalezena signatura self-extractoru.")
STRING(STR_ERROR_TEMPPATH, "Nelze získat cestu adresáře pro dočasné soubory: ")
STRING(STR_ERROR_TEMPNAME, "Nelze získat název dočasného souboru: ")
STRING(STR_ERROR_DELFILE, "Nelze smazat dočasný soubor: ")
STRING(STR_ERROR_MKDIR, "Nelze vytvořit dočasný adresář: ")
STRING(STR_ERROR_GETATTR, "Nelze získat atributy souboru: ")
STRING(STR_ERROR_GETCWD, "Nelze získat pracovní adresář: ")
STRING(STR_ERROR_BADDATA, "Chyba ve zkomprimovaných datech.")
STRING(STR_ERROR_TOOLONGNAME, "Název souboru je příliš dlouhý.")
STRING(STR_LOWMEM, "Chyba, nedostatek paměti: ")
STRING(STR_NOTENOUGHTSPACE, "Zdá se, že na cílovém disku není dostatek místa.\nPřejete si pokračovat?\n\nPožadovavé místo: %s bytů.\nVolné místo: %s bytů.\n%s")
STRING(STR_ERROR_TCREATE, "Nelze vytvořit nové programové vlákno: ")
STRING(STR_ERROR_METHOD, "Soubor nelze vybalit. Soubor je zkomprimován nepodporovanou metodou.")
STRING(STR_ERROR_CRC, "Chyba CRC.")
STRING(STR_ERROR_ACCES, "Nelze pracovat se souborem: ")
STRING(STR_ERROR_WRITE, "Nelze zapsat do souboru: ")
STRING(STR_ERROR_BREAK, "Přejete si přerušit vybalování souborů?")
STRING(STR_ERROR_DLG, "Nelze vytvořit dialogové okénko: ")
STRING(STR_ERROR_FCREATE, "Nelze vytvořit nebo otevřít soubor: ")
STRING(STR_ERROR_WAIT, "Selhalo čekání na proces: ")
STRING(STR_ERROR_WAIT2, "\nZmáčkněte OK, až bude možné bezpečně smazat dočasné soubory.")
STRING(STR_ERROR_WAITTHREAD, "Selhalo čekání na programové vlákno: ")
STRING(STR_ERROR_DCREATE, "Nelze vytvořit nový adresář: ")
STRING(STR_ERROR_DCREATE2, "Nelze vytvořit nový adresář: Název je již použit pro soubor.")
STRING(STR_ERROR_FCREATE2, "Nelze vytvořit soubor: Název je již použit pro adresář.")
STRING(STR_TARGET_NOEXIST, "Cílový adresář neexistuje, přejete si ho vytvořit?")
STRING(STR_OVERWRITE_RO, "Přejete si přepsat soubor s atributem POUZE PRO ČTENÍ, SYSTÉMOVÝ nebo SKRYTÝ?")
STRING(STR_STOP, "Skončit")
STRING(STR_STATNOTCOMPLETE, "Žádné soubory nebyly vybaleny.")
STRING(STR_SKIPPED, "Počet přeskočených souborů:\t\t%d\n")
STRING(STR_STATOK, "Rozbalování bylo úspěšně dokončeno.\n\nPočet vybalených souborů:\t\t%d\n%sCelková velikost vybalených souborů:\t%s B  ")
STRING(STR_BROWSEDIRTITLE, "Vyberte adresář.")
STRING(STR_BROWSEDIRCOMMENT, "Vyberte adresář pro rozbalení souborů.")
STRING(STR_OVERWRITEDLGTITLE, "Potvrďte přepsání souboru")
STRING(STR_TEMPDIR, "&Složka pro uložení dočasných souborů:")
STRING(STR_EXTRDIR, "&Složka pro rozbalení souborů:")
STRING(STR_TOOLONGCOMMAND, "Nelze spustit příkaz: Jméno souboru je příliš dlouhé.")
STRING(STR_ERROR_EXECSHELL, "Nelze spustit příkaz: ")
STRING(STR_ADVICE, "\nPoznámka: samorozbalitelný archiv můžete spustit s parametrem '-s', který umožní výběr složky pro rozbalení souborů")
STRING(STR_BADDRIVE, "Zadané jméno disku nebo síťového sdílení je chybné.")
STRING(STR_ABOUT1, "&O programu >>")
STRING(STR_ABOUT2, "<< &O programu")
STRING(STR_OK, "OK")
STRING(STR_CANCEL, "Storno")
STRING(STR_YES, "Ano")
STRING(STR_NO, "Ne")
STRING(STR_AGREE, "Souhlasím")
STRING(STR_DISAGREE, "Nesouhlasím")
#ifndef EXT_VER
STRING(STR_ERROR_ENCRYPT, "Nelze vybalit soubor. Soubor je zašifrovaný.")
STRING(STR_PARAMTERS, "Parametry pro spuštění z příkazové řádky:\n\n-s\tBezpečný mód. Zobrazí hlavní dialogové okénko\n\ta umožní změnu složky pro rozbalení archivu.")
#else  //EXT_VER
STRING(STR_ERROR_TEMPCREATE, "Nelze vytvořit dočasný soubor: ")
STRING(STR_ERROR_GETMODULENAME, "Nelze získat jméno modulu: ")
STRING(STR_ERROR_SPAWNPROCESS, "Nelze spustit nový proces: ")
STRING(STR_INSERT_LAST_DISK, "Vložte prosím poslední disketu do mechaniky a zmáčkněte OK.\n\n"
                             "Poznámka: Pro přesné určení posledního souboru archivní sady\n"
                             "použijte při spuštění parametr '-l' z příkazové řádky. Pro více\n"
                             "informací spusťte samorozbalitelný archiv s parametrem '-?'")
STRING(STR_INSERT_NEXT_DISK, "Vložte prosím disketu číslo %d.\na zmáčkněte OK.")
STRING(STR_ERROR_GETWD, "Nelze získat adresář Windows: ")
STRING(STR_ADVICE2, "Poznámka: samorozbalitelný archiv můžete spustit s parametrem '-t', který umožní výběr složky pro uložení dočasných souborů.")
STRING(STR_PARAMTERS, "Parametry pro spuštění z příkazové řádky:\n\n"
                      "-s\t\tBezpečný mód. Zobrazí hlavní dialogové okénko\n"
                      "\t\ta umožní změnu složky pro rozbalení archivu.\n"
                      "-t [path]\t\tUloží dočasné soubory do zadané složky.\n"
                      "-a [archive]\tRozbalí zadaný archiv.\n"
                      "-l [archive]\tStejné jako -a, ale předpokládá, že zadaný archiv\n"
                      "\t\tje poslední soubor archivu\n\n"
                      "Poznámka: Dlouhá názvy souborů musí být uzavřeny v uvozovkách.")
STRING(STR_ERROR_READMAPPED, "Nelze číst z archivního souboru.")
STRING(STR_PASSWORDDLGTITLE, "Soubor je zašifrovaný")
#endif //EXT_VER

//STRING(STR_NOTSTARTED, "")
