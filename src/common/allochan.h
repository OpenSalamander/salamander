// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Zavadi handler pro reseni situace, kdy dosla pamet pri volani operatoru new nebo
// funkce malloc (tu pouzivaji calloc, realloc a dalsi, viz help). Zarucuje, ze ani
// operator new ani malloc nikdy bez vedomi uzivatele nevrati NULL. Zobrazuje
// messagebox s chybou "nedostatek pameti" a uzivatel muze po zavreni dalsich
// aplikaci zopakovat pokus o alokaci pameti. Uzivatel muze tez terminovat proces
// nebo nechat propadnout chybu alokace do aplikace (operator new nebo malloc
// vrati NULL, alokace velkych bloku pameti by na to mely byt pripravene, jinak
// dojde k padu - uzivatel je o tom informovany).

// nastaveni lokalizovane podoby hlasky o nedostatku pameti a varovnych hlasek
// (pokud se string nema menit, pouzijte NULL); ocekavany obsah zni:
// message:
// Insufficient memory to allocate %u bytes. Try to release some memory (e.g.
// close some running application) and click Retry. If it does not help, you can
// click Ignore to pass memory allocation error to this application or click Abort
// to terminate this application.
// title: (pouziva se pro oboji: "message" i "warning")
// doporucujeme pouzit jmeno aplikace, at user vi, ktera aplikace si stezuje
// warningIgnore:
// Do you really want to pass memory allocation error to this application?\n\n
// WARNING: Application may crash and then all unsaved data will be lost!\n
// HINT: We recommend to risk this action only if the application is trying to
// allocate extra large block of memory (i.e. more than 500 MB).
// warningAbort:
// Do you really want to terminate this application?\n\nWARNING: All unsaved data will be lost!
void SetAllocHandlerMessage(const TCHAR* message, const TCHAR* title,
                            const TCHAR* warningIgnore, const TCHAR* warningAbort);
