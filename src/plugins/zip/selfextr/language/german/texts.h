// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// text.h - german version

STRING(STR_TITLE, "Selbstentpackendes ZIP Archiv")
STRING(STR_ERROR, "Fehler")
STRING(STR_QUESTION, "Abfrage")
STRING(STR_ERROR_FOPEN, "Archivdatei kann nicht geöffnet werden: ")
STRING(STR_ERROR_FSIZE, "Größe des Archivs ist nicht zu ermitteln: ")
STRING(STR_ERROR_MAXSIZE, "Maximale Größe des Archivs erreicht")
STRING(STR_ERROR_FMAPPING, "Das Archiv kann nicht in dem Speicher abgebildet werden: ")
STRING(STR_ERROR_FVIEWING, "Das Archiv kann aus dem Speicher nicht angezeigt werden: ")
STRING(STR_ERROR_NOEXESIG, "EXE-Signatur nicht gefunden.")
STRING(STR_ERROR_NOPESIG, "PE-Signatur nicht gefunden.")
STRING(STR_ERROR_INCOMPLETE, "Datenfehler - Datei nicht vollständig.")
STRING(STR_ERROR_SFXSIG, "Signatur des Self-Extractors nicht gefunden.")
STRING(STR_ERROR_TEMPPATH, "Pfad des temporären Verzeichnisses nicht zugreifbar: ")
STRING(STR_ERROR_TEMPNAME, "Temporäre Datei nicht zugreifbar: ")
STRING(STR_ERROR_DELFILE, "Temporäre Datei kann nicht gelöscht werden: ")
STRING(STR_ERROR_MKDIR, "Temporäres Verzeichnis kann nicht erstellt werden: ")
STRING(STR_ERROR_GETATTR, "Dateiattribute nicht zugreifbar: ")
STRING(STR_ERROR_GETCWD, "Arbeitsverzeichnis nicht zugreifbar: ")
STRING(STR_ERROR_BADDATA, "Fehler in komprimierten Daten.")
STRING(STR_ERROR_TOOLONGNAME, "Dateiname ist zu lang.")
STRING(STR_LOWMEM, "Fehler, zu wenig Speicher: ")
STRING(STR_NOTENOUGHTSPACE, "Ziellaufwerk scheint nicht genug Platz zu haben.\nFortfahren?\n\nBenötigt: %s Bytes\nFrei: %s Bytes\n%s")
STRING(STR_ERROR_TCREATE, "Nicht möglich, neuen Thread zu erzeugen: ")
STRING(STR_ERROR_METHOD, "Datei nicht entpackbar. Datei wurde mit nicht unterstützter Methode gepackt.")
STRING(STR_ERROR_CRC, "CRC-Fehler.")
STRING(STR_ERROR_ACCES, "Datei nicht zugreifbar: ")
STRING(STR_ERROR_WRITE, "Datei nicht schreibbar: ")
STRING(STR_ERROR_BREAK, "Wollen Sie das Entpacken der Dateien beenden?")
STRING(STR_ERROR_DLG, "Dialogfenster kann nicht erstellt werden: ")
STRING(STR_ERROR_FCREATE, "Datei kann nicht erstellt oder geöffnet werden: ")
STRING(STR_ERROR_WAIT, "Warten auf den Prozess misslungen: ")
STRING(STR_ERROR_WAIT2, "\nDrücken Sie OK, wenn temporäre Dateien wirklich gelöscht werden können.")
STRING(STR_ERROR_WAITTHREAD, "Warten auf neuen Thread misslungen: ")
STRING(STR_ERROR_DCREATE, "Neues Verzeichnis kann nicht erstellt werden: ")
STRING(STR_ERROR_DCREATE2, "Neues Verzeichnis kann nicht erstellt werden: Name schon für eine Datei verwendet.")
STRING(STR_ERROR_FCREATE2, "Neue Datei kann nicht erstellt werden: Name schon für ein Verzeichnis verwendet.")
STRING(STR_TARGET_NOEXIST, "Zielverzeichnis existiert nicht, soll es erstellt werden?")
STRING(STR_OVERWRITE_RO, "Wollen Sie die Datei, die eins der Attribute NUR LESEN, SYSTEM oder VERSTECKT hat, überschreiben?")
STRING(STR_STOP, "Beenden")
STRING(STR_STATNOTCOMPLETE, "Es wurde keine Datei entpackt.")
STRING(STR_SKIPPED, "Anzahl der übersprungenen Dateien:\t%d\n")
STRING(STR_STATOK, "Entpacken erfolgreich beendet.\n\nAnzahl der entpackten Dateien:\t\t%d\n%sGesamtgröße der entpackten Dateien:\t%s Bytes")
STRING(STR_BROWSEDIRTITLE, "Verzeichnis auswählen")
STRING(STR_BROWSEDIRCOMMENT, "Wählen Sie das Verzeichnis für die zu entpackenden Dateien.")
STRING(STR_OVERWRITEDLGTITLE, "Überschreiben der Datei bestätigen")
STRING(STR_TEMPDIR, "&Verzeichnis für Ablage der temporären Dateien:")
STRING(STR_EXTRDIR, "&Dateien in angegebenes Verzeichnis entpacken:")
STRING(STR_TOOLONGCOMMAND, "Befehl kann nicht ausgeführt werden: Dateiname zu lang.")
STRING(STR_ERROR_EXECSHELL, "Befehl kann nicht ausgeführt werden: ")
STRING(STR_ADVICE, "\nAnmerkung: Den Self-Extractor können Sie mit dem Parameter -s starten, der das Zielverzeichnis auswählen lässt")
STRING(STR_BADDRIVE, "Laufwerk oder Netzwerk-Freigabename ungültig.")
STRING(STR_ABOUT1, "Ü&ber >>")
STRING(STR_ABOUT2, "<< Ü&ber")
STRING(STR_OK, "OK")
STRING(STR_CANCEL, "Abbrechen")
STRING(STR_YES, "Ja")
STRING(STR_NO, "Nein")
STRING(STR_AGREE, "Annehmen")
STRING(STR_DISAGREE, "Ablehnen")
#ifndef EXT_VER
STRING(STR_ERROR_ENCRYPT, "Verschlüsselte Datei kann nicht entpackt werden.")
STRING(STR_PARAMTERS, "Befehlszeilenparameter:\n\n-s\tSicherheitsmodus. Zeigt Hauptdialogfenster\n\tund ermöglicht Wechsel des Zielverzeichnisses.")
#else  //EXT_VER
STRING(STR_ERROR_TEMPCREATE, "Temporäre Datei kann nicht erstellt werden: ")
STRING(STR_ERROR_GETMODULENAME, "Name des Moduls kann nicht ermittelt werden: ")
STRING(STR_ERROR_SPAWNPROCESS, "Neuer Prozess kann nicht erzeugt werden: ")
STRING(STR_INSERT_LAST_DISK, "Letztes Medium des Satzes einlegen und OK drücken.\n\n"
                             "Anmerkung: Zur genauen Festlegung der letzten Datei des Archivsatzes\n"
                             "verwenden Sie bitte den Befehlszeilenparameter '-l'\n"
                             "Mehr Informationen: Start mit '-?'")
STRING(STR_INSERT_NEXT_DISK, "Legen Sie bitte Medium Nr. %d ein!\nDrücken Sie danach OK.")
STRING(STR_ERROR_GETWD, "Das Windows-Verzeichnis ist nicht erreichbar: ")
STRING(STR_ADVICE2, "Anmerkung: Der Self-Extractor kann mit dem Parameter '-t' gestartet werden, dann Auswahl des temporären Verzeichnisses möglich.")
STRING(STR_PARAMTERS, "Befehlszeilenparameter:\n\n"
                      "-s\t\tSicherheitmodus. Öffnet das Hauptdialogfenster\n"
                      "\t\tund ermöglicht Wahl des Zielverzeichnisses.\n"
                      "-t [path]\tlegt temporäre Dateien im angegebenen Verzeichnis ab\n"
                      "-a [archiv]\tEntpackt das angegebene Archiv.\n"
                      "-l [archiv]\tWie -a, nimmt aber an, dass angegebener Archivname,\n"
                      "\t\tzur letzten Datei des Archivs gehört\n\n"
                      "Anmerkung: Lange Dateinamen müssen durch Anführungszeichen eingeschlossen sein.")
STRING(STR_ERROR_READMAPPED, "Archivdatei kann nicht gelesen werden.")
STRING(STR_PASSWORDDLGTITLE, "Passwort notwendig!")
#endif //EXT_VER

//STRING(STR_NOTSTARTED, "")
