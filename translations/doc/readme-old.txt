What is desktop1.png and desktop2.png
=====================================

This is the recommended placement of windows in Altap Translator.


What is shell.reg
=================

This file contains registration of Altap Translator for opening of .atp project
files and its icon for these files to make finding project files easier.

If you want to install it: If you have altaptrl.exe on path C:\ALTAPTRL, just
import shell.reg to your registry (double click on shell.reg). If not, first
edit path to altaptrl.exe in shell.reg (two occurences + be aware of doubled
backslashes), and then import shell.reg to your registry.


What is shell_uninstal.reg
==========================

Execute this file to remove changes made by importing shell.reg (uninstall
Altap Translator registration from registry).


How to create new language version
==================================

Go to projects directory (e.g. ALTAPTRL\Salamand 3.06\projects) and open
makelang.js in your text editor. Set 'language' variable to your language,
please use english name of language. Then start makelang.js script in
Automation plugin (Shift+Ctrl+A to open context menu, then click
Run Focused Script). It should create subdirectory named by your language
(e.g. "czech"), place all project files to this directory and clone all
english.slg modules to your language .slg modules (e.g. czech.slg) in
bin directory (e.g. ALTAPTRL\Salamand 3.06\bin).

Copy needed batches from links directory (e.g. ALTAPTRL\Salamand 3.06\
projects\tools\links) to directory with projects for your language (e.g.
ALTAPTRL\Salamand 3.06\projects\czech), we recommend at least:
!translate1.bat, !translate2.bat, !validate1.bat, and !validate2.bat.

Open Salamander's project (e.g. ALTAPTRL\Salamand 3.06\projects\czech\
salamand.atp). Use menu File / Translation Properties and choose appropriate
locale, enter your name as author, etc.

If you want to copy Translation Properties also to all other modules
(plugins), copy !import_trlprop.bat batch from links directory (e.g.
ALTAPTRL\Salamand 3.06\projects\tools\links) to directory with projects
for your language (e.g. ALTAPTRL\Salamand 3.06\projects\czech) and run
this batch.


How to upgrade language version for new version of Salamander
=============================================================

Go to the directory of new version, create new language version for your
language as if it is new, see previous section "How to create new language
version" and skip last two paragraphs concerning setting Translation
Properties (they will be imported from older language version).

Then copy !import.bat batch file from links directory (e.g. ALTAPTRL\
Salamand 3.06\projects\tools\links) to directory with projects for your
language (e.g. ALTAPTRL\Salamand 3.06\projects\czech). Open
!set_import_salver.bat from projects directory (e.g. ALTAPTRL\Salamand 3.06\
projects) in text editor and check if the version in
IMPORT_MODULES_FROM_SALVER variable is correct, translated texts will be
imported from this older version, more precisely from its directory in
ALTAPTRL directory (e.g. ALTAPTRL\Salamand 3.05). Please check if
your older language version is placed in this directory. Run !import.bat,
if it reports some errors, let us know, it should import older texts without
any errors.

If dialog layout does not change between old and new original version (English
version), Altap Translator copies dialog layout from old translated
version to new translated version. Otherwise, dialog layout is not copied
(so original dialog layout is used) because it probably needs to be revised
manually. Such dialogs are marked for relayouting and when you run validation,
they are reported as errors until you edit their layout. If no change is
needed, use menu Tools / Clear Dialog Relayout Flag (Ctrl+J).


How to open project in Altap Translator
=======================================

If you have used shell.reg (see above), just double click project file.
Otherwise open Altap Translator and use menu File / Open Project.


How to translate
================

Go to Navigator window (Alt+1), choose dialog, menu, or string table you want
to translate. String tables contain up to 16 strings, each table is labeled
using identification number (ID) of the first string, if you need to find
string by ID, just find table with closest smaller or equal ID in label.

When you choose dialog, menu, or string table to translate, go to Texts window
(Alt+2) where you can write translated version of original text (use F3 key to
focus Translated text editbox in Texts window from anywhere). Press Enter
to mark text as translated and to move to the next text in Texts window. Use
double click in list of IDs to toggle translated/untranslated state of text.

Use menu Edit / Find Untranslated Texts (Ctrl+U) to find all texts which
need to be translated. They are displayed in Output window (Alt+4). Press
F4 key to move to next untranslated text, Shift+F4 key to prior. When you
translate text and press Ctrl+Enter in Texts window, text is marked as
translated and you will move to next untranslated text.

If you are not sure what is the correct translation of the original English
text, you can try to search it (or its fragments) in MS Language Portal
(http://www.microsoft.com/Language/en-US/Search.aspx) or in Windows Server
2008 R2 / Windows 7 MUI Language Packs to see how experts in MS translates
it. It is described in section "How to search Windows Server 2008 R2 /
Windows 7 MUI Language Packs" later in this file.

If you want to find already translated language modules with new or changed
strings to be translated or at least corrected, typically when new version
of Altap Salamander was released, you can use !translate1.bat or
!translate2.bat batch files placed in directory with projects (e.g.
ALTAPTRL\Salamand 3.06\projects\german). Batch !translate1.bat stops when
it finds first partially translated module, !translate2.bat does not, it is
designed to open all partially translated modules (one after another). When
such module is found, Altap Translator window is opened and you can finish
its translation. Otherwise, the window is not opened and searching continues
automatically on the next module. It also finds completely untranslated
modules and shows "FOUND" label in command prompt window, but it does not
open Altap Translator window for such modules, do it manually, see section
"How to open project in Altap Translator".

When all texts in dialog, menu, or string table are marked as translated,
they have icon with green check sign in Navigator window. Otherwise
need-to-translate icon is displayed.

Double click dialog in Navigator or Preview window to open it in Dialog
Layout Editor (also Ctrl+L). Editing is described in following section
"How to use Dialog Layout Editor".


How to validate translation
===========================

Go to menu Tools / Validate Translation (Ctrl+Q). It opens Validate
Translation dialog box, choose here what you want to validate, properly
translated module should pass all validations. Click OK to start validation,
errors are listed in Output window (Alt+4). Press F4 key to move to next
error, Shift+F4 key to prior error.

It may happen that some error is irrelevant, we can add it to ignore list
for the validated module (see ignore.lst file in symbols subdirectory,
e.g. ALTAPTRL\Salamand 3.06\symbols\plugins\ftp\ignore.lst). Let us know,
we will patch it or tell you why we need to fix even such error.

If you want to validate all language modules, you can use !validate1.bat
or !validate2.bat batch files placed in directory with projects (e.g.
ALTAPTRL\Salamand 3.06\projects\german). Batch !validate1.bat stops after
first module with validation error, !validate2.bat does not, it is designed
to validate all modules. When validation error is found, Altap Translator
window is opened and you can fix it. Otherwise, the window is not opened
and validation continues automatically on the next module.

When you solve hotkeys conflicts, you should know that hotkey is preceeded
by ampersand (&) character, if you need to use ampersand character in text,
double it (&&). All items with the same hotkey are listed one after another,
you should choose one of them which can use the hotkey and change hotkey
for others. We recommend to use some of the suggested not conflicting keys
offered in HINT at the end of error message. Start validation again
(Ctrl+Q, Enter) after every change, some errors may cease to exist. When
there are no keys in HINT, try to choose any other key in text as hotkey
and start validation again, it will find item using your choosen key and
it may contain some not conflicting key in HINT which can be used to solve
the conflict.

Format specifiers start with percent (%) character (e.g. %s (text),
%d (decimal number), %08X (hexadecimal number with 8 digits)). They are
places in text where Salamander fills various values at run time, e.g.
name of currently focused file in panel (e.g. Copy file "%s" to -> Copy
file "test.txt" to). You cannot change order of format specifiers nor
to add some or delete existing because it may crash Salamander e.g. due
to access to inaccesible memory location. That's why we validate them.

Control characters start with backslash (\) character (e.g. \r (CR),
\n (LF), \t (TAB)). They control text flow, e.g. \r\n means end of line.
Sometimes it will not harm anything if you change them, but there are
also situations where it causes problems like "last line of text is not
displayed", etc. So that's the reason why we want you not to change them.

Plural strings are described in following section "What is plural string".

We are validating beginnings and endings of text because we sometimes
build texts from more single texts and it usually uses some presumptions,
e.g. that space ( ) character is at the end of text, so other text can
be added seamlessly.

We have added also many validations for "good looking" dialogs, e.g.
buttons are spaced equally. To edit dialog layout, go to menu
Tools / Edit Dialog Layout (Ctrl+L). See also following section
"How to use Dialog Layout Editor".


How to test language version
============================

Go to menu Tools and click Save All and Restart Altap Salamander (F5 key).
You can see that started Salamander has green icon to be easily
distinguished from other instances. Then go to menu Options /
Configuration / Language, click Language button and select your language
version. Exit Salamander, it saves configuration (mainly that it should
start in your language). Start Salamander again using F5 key, it should
start in your language.

After any change in strings or dialog layouts, press F5 key to kill
running "green" Salamander (if any), save changes to .slg module, and
start "green" Salamander with new .slg module. Use menu Tools / Kill
Altap Salamander (Shift+F5 key) if you just want to kill "green"
Salamander.


How to send language version to ALTAP
=====================================

Please use !translate1.bat or !translate2.bat batch to check if you
have translated all texts in all language modules. If you have forgotten
to mark texts as translated, use Mark Changed Strings as Translated command
(Ctrl+K) from Tools menu. Then please check if all language modules are
valid, use !validate1.bat or !validate2.bat batch.

We prefer receiving language version in .slt files. They contain
all your changes to English version, so we can build your language
version easily. Moreover they are Unicode (UTF8) text files, so it
is possible to compare them against older versions, merge versions
from more translators, etc.

You can export .slt files simply by running !export_slt.bat batch from
directory with projects (e.g. ALTAPTRL\Salamand 3.06\projects\german).
Files are exported to slt directory. Pack slt directory and send it
as email attachment to support@altap.cz.


How to work in team on translation
==================================

Easiest way is to divide modules (Salamander and plugins) among
translators. So no collision is possible because one module is not
edited by more translators. One translator should collect new versions
from all others and when the version is done, this translator should
send it to ALTAP. To send translated module to other translator, use
.slt files.

You can export .slt files simply by running !export_slt.bat batch from
directory with projects (e.g. ALTAPTRL\Salamand 3.06\projects\german).
It will export complete translation - content of all .slg files from bin
directory and their appropriate project files. Files are exported to
slt directory. Each module has its own .slt file which is Unicode (UTF8)
text file containing all your changes to English version of this module.

Scenario for getting new version of translated module from other translator
(translator 2 is getting new version from translator 1):
-Translator 1: export .slt files: run !export_slt.bat.
-Translator 1: send appropriate .slt file (salamand.slt for main Salamander
 module, 7zip.slt for 7-ZIP plugin, etc.) to translator 2.
-Translator 2: export .slt files: run !export_slt.bat.
-Translator 2: overwrite (update) own version of .slt file received
 from translator 1.
-Translator 2: import .slt files: run !import_slt.bat from the same
 directory in which you started !export_slt.bat.

IMPORTANT NOTE for translator 2: between calls to !export_slt.bat and to
!import_slt.bat you cannot make any changes to translation because calling
to !import_slt.bat overwrites any changes in .slg (language modules) and
.atp (projects) files and so your changes would be lost.

If two or more translators want to work on one module, it is also possible.
We recommend to split work clearly, so they will work on different parts and
will not change the same text at the same time.

Scenario for merging work of two translators:
-Translator 1: export .slt files: run !export_slt.bat.
-Translator 2: export .slt files: run !export_slt.bat.
-Translator 1: send appropriate .slt file (salamand.slt for main Salamander
 module, 7zip.slt for 7-ZIP plugin, etc.) to translator 2.
-Translator 2:
  -Create some new directory.
  -Place received .slt file to this new directory and rename this .slt file,
   e.g. add suffix "_1" (e.g. 7zip_1.slt).
  -Copy own version of received .slt file to this new directory and rename
   this .slt file, e.g. add suffix "_2" (e.g. 7zip_2.slt).
  -Merge these two .slt files. Result should be stored into the same
   directory, use original .slt filename (e.g. 7zip.slt). You can use e.g.
   WinMerge (http://winmerge.org/) or if the merging is simple, you should
   manage with File Comparator in Salamander and some simple text editor
   supporting UTF8, e.g. SciTE (http://www.scintilla.org/SciTE.html).
   Or use CVS, SVN, or other revision control system for automatic merging,
   see later in this document.
-Translator 2: overwrite (update) own version of .slt file with resulting
 .slt file which contains changes of both translators.
-Translator 2: import .slt files: run !import_slt.bat from the same
 directory in which you started !export_slt.bat.
-Translator 2: if it all works well, send resulting .slt file to translator 1.
-Translator 1: overwrite (update) own version of .slt file with received
 .slt file which contains changes of both translators.
-Translator 1: import .slt files: run !import_slt.bat from the same
 directory in which you started !export_slt.bat.

IMPORTANT NOTE: between calls to !export_slt.bat and to !import_slt.bat you
cannot make any changes to translation because calling to !import_slt.bat
overwrites any changes in .slg (language modules) and .atp (projects) files
and so your changes would be lost.

Scenario for merging work of more translators using CVS, SVN, or other
revision control system:
-Setup - first translator:
  -Create your language version.
  -Export .slt files: run !export_slt.bat.
  -Add and commit your .slt files to CVS/SVN server. Your working directory
   should be the directory to which !export_slt.bat exports .slt files.
-Setup - other translators:
  -Create your language version.
  -Export .slt files: run !export_slt.bat.
  -Go to directory with .slt files, delete them all and check out .slt
   files from CVS/SVN server to this directory.
  -Import .slt files: run !import_slt.bat from the same directory in which you
   started !export_slt.bat.
-How to get new version of translation and commit your changes:
  -Export .slt files: run !export_slt.bat.
  -Update .slt files from CVS/SVN server. It should change .slt files exported
   in previous step. Solve conflicts (if any). Conflicts should not occur if you
   split the work on translation well.
  -Import .slt files: run !import_slt.bat from the same directory in which you
   started !export_slt.bat.
  -If import was successful, commit your .slt files to CVS/SVN server.


How to check spelling of translated texts
=========================================

Copy !export_spellchck.bat batch from links directory (e.g. ALTAPTRL\
Salamand 3.06\projects\tools\links) to directory with projects for your
language (e.g. ALTAPTRL\Salamand 3.06\projects\czech) and run this batch.
It will export all texts from all modules to directory "texts". Use your
favourite text editor to open these text files and do spell checking. If you
find some typos, correct them directly in Altap Translator, use Find (Ctrl+F)
to find text with typo.


What is checklist.txt
=====================

When you finish translation, please go through this file and test
potentially problematic places of your translated version. If you find
some other problematic places, please let us know, we will add them to
this file for other translators.


How to use Dialog Layout Editor
===============================

If you don't see outlines for all controls (editboxes, static texts, icons,
comboboxes, etc.), turn it on by pressing O key. Controls with clipped texts
(text is not fully visible due to small size of control) have red color of
outline, controls with sufficient size has blue outline. To select control
use Tab key or mouse, to select more controls use mouse while holding Shift
key. To move or resize controls use mouse or arrow keys. For advanced layout
commands see menu Position and Resize. Especially useful is command
Size to Content (S key), it sets width of all selected static texts,
checkboxes, and radio buttons according to contained text. We recommend to
select all controls (Ctrl+A) and use Size to Content (S key) before all other
changes to dialog layout. Use Undo (Ctrl+Z) and Redo (Ctrl+Y) commands to
revert or repeat your changes. See also following section "Dialog Layout
Editor Keyboard Shortcuts".

Please try not to change left-hand indent and alignment of controls. When
enlarging dialog, enlarge also controls reaching right margin of dialog,
e.g. editboxes, comboboxes, groups, horizontal lines (to prevent blank space
on the right side of dialog; BUT: do not enlarge buttons, checkboxes, radio
buttons, static texts, etc.). Also shift buttons to the right or center them
to new dialog width (see menu Position / Center Horizontal to Dialog, or
use C key).

All needed coordinates of selected control or dialog are located at bottom
of window, there are: Left, Top, Right, Bottom, Width, Height. Moreover
there is info about distances to other controls in dialog, see Margins line.

If you want to see the original layout (e.g. to check distance of some
control to right side of dialog when you want to preserve it in your wider
version of dialog), press R key (menu Tools / Reset to Original Layout)
and then return to your layout by Ctrl+Z key (menu Edit / Undo).


Dialog Layout Editor Keyboard Shortcuts
=======================================

* ESC or Alt+F4 or Ctrl+F4 - close Layout Editor, if you have changed layout,
  you can choose if to use new layout or discard changes.
* Tab and Shift+Tab keys - focus next/previous control (editbox, static text,
  icon, combobox, etc.), after last control in dialog you focus dialog itself
  (it is needed when you want to change size of dialog).
* Arrows - move focused control or group of selected controls.
* Shift+Arrows - change size of control.
* Ctrl+Shift+Arrows - move left top corner of control.
* Arrows repeating (like in unix "vi"): e.g. to repeat left arrow 30 times,
  just press: '3', '0', and Left Arrow; to repeat 30 times again, press 'a'
  key and arrow in needed direction (Shift and Ctrl+Shift modifiers for arrows
  can be used too)
* C - center selected controls horizontally to dialog
* H - equal horizontal spacing of selected controls, if you want exact spacing,
  type width before pressing 'H', e.g. to make distance between controls 6 units
  press: '6' and 'H'
* O - turn on/off outline for all controls, we recommend it to see all controls
  (even that without specified text).
* R - use original dialog layout (mostly English layout), we recommend it in
  connection with Undo (Ctrl+Z) to see how you have changed layout of dialog
  (press R and Ctrl+Z and again R and Ctrl+Z to see both layouts).
* S - selected static texts, checkboxes, and radio buttons change their widths
  to fit contained text, recommended as the first change of dialog layout.
* Shift+LClick on control - select or unselect control from selected group.
* Ctrl on beginning of selected controls move - controls will move right away;
  the "unintentional move test" will be suppressed.
* Shift while moving controls - lock move in one axis (only horizontal or vertical).
* Alt while starting the cage selection - control below mouse cursor will be
  ignored; cage could be started anywhere in dialog.


What is plural string
=====================

Salamander contains support for parameter dependent strings (dealing with
singles/plurals). E.g. simple solution is to always use "file(s)", but
correct is "0 files", "1 file", "2 files", etc. Correct solution is
possible with plural strings, here: "{!}file{s|0||1|s}".

Plural string format:
- each plural string starts with signature "{!}"
- plural string can contain following escape sequences (it allows to use special
  character without its special meaning): "\\\\" = "\\" (it is single backslash in
  resulting string because "\\" is common string escape sequence for backslash),
  "\\{" = "{", "\\}" = "}", "\\:" = ":", and "\\|" = "|"
- text which is not placed in curly brackets goes directly to resulting string
  (only escape sequences are handled)
- variable part (parameter dependent text) is placed in curly brackets
- each plural string is accepting predefined number of parameters (usually
  numbers of files, directories, bytes, etc.), each variable part (parameter
  dependent text) in curly brackets uses one of these parameters
- variable part contains more variants of resulting text, which variant
  is used depends on parameter value, more precisely to which defined
  interval the value belongs
- variants of resulting text and interval bounds are separated by "|" character
- first interval is from 0 to first interval bound
- last interval is from last interval bound plus one to infinity (2^64-1)
- if you need to skip one parameter, use variable part "{}" (nothing goes to
  resulting string)
- you can specify which parameter you want to use in variable part, just place
  its index (from one to number of parameters) to the beginning of variable part
  and follow it by colon (':')
- if you don't specify index of parameter, it is assigned automatically (starting
  from one to number of parameters)
- if you specify index of parameter, the next index which is assigned automatically
  is not affected, e.g. in "{!}%d file{2:s|0||1|s} and %d director{y|1|ies}" the
  first variable part uses parameter with index 2 and second uses parameter with
  index 1
- you can use any number of variable parts with specified index of parameter,
  e.g. see translation from English to German:
  English: {!}Do you want to shred %d temporary director{y|1|ies} used by previous
           instances of the Encrypt & Decrypt plugin?
  German:  {!}Möchten Sie {das|1|die} %d temporäre Verzeichni{1:s|1|sse}, die
           durch eine vorherige Instanz des Ver- & Entschüsselungsplugin erstellt
           wurde{1:|1|n}, schreddern?

Examples:
- "{!}director{y|1|ies}": for parameter values from 0 to 1 resulting string will be
  "directory" and for parameter values from 2 to infinity (2^64-1) resulting string
  will be "directories"
- "{!}%d soubor{u|0||1|y|4|u} a %d adresar{u|0||1|e|4|u}": it needs two parameters
  because there are two variable parts (parameter dependent texts) in curly brackets,
  resulting string for choosen pairs of parameters (I believe it is not needed to
  show all possible variants):
    0, 0: "%d souboru a %d adresaru"
    1, 12: "%d soubor a %d adresaru"
    3, 4: "%d soubory a %d adresare"
    13, 1: "%d souboru a %d adresar"


How to search Windows Server 2008 R2 / Windows 7 MUI Language Packs
===================================================================

First try if MS Language Portal (http://www.microsoft.com/Language/en-US/Search.aspx)
is not working sufficiently for you.

Start with downloading appropriate 32-bit (x86) language packs from this URL:
http://www.microsoft.com/downloads/details.aspx?familyid=3A7FB7A2-3519-495B-9BC5-2007082CA9A6&displaylang=en
Group 1 contains English, German, French, and Spanish versions and you will need it
as Original. For Czech translation download also Group 5.

Another option is use Windows 7 MUI packs instead, but it contains only half
of Windows 2008 R2 MUI strings.
http://www.vista123.net/content/download-windows-7-mui-language-packs-official-32-bit-and-64-bit-direct-download-links
In this case you will need to rename downloaded ".exe" to ".cab".

Extract downloaded DVD images and extract required .cab archive. In the second
step extract this CAB archive to some new empty directories (e.g.
C:\ALTAPTRL\R2_MUI\EN and C:\ALTAPTRL\R2_MUI\CZ).

Start new instance of Altap Translator. Go to menu File / Open MUI Packages,
choose English version to Original editbox (e.g. C:\ALTAPTRL\R2_MUI\EN) and
your language version to Translated editbox (e.g. C:\ALTAPTRL\R2_MUI\CZ).
Click OK and wait, there are many dialogs, so it will take some time (e.g.
one minute). It should read approx 12000 dialogs, 1200 menus, and several
thousands of strings (IDs are not sequential).

Then use Find (Ctrl+F) to search original and/or translated texts (see
"Look in Translated Texts" and "Look in Original Texts" checkboxes in Find
window). Use F4 key to move to next found occurence of searched text.


What is not covered by this package
===================================

Altap Self-Extractor packages (e.g. english.sfx and czech.sfx): if you want to
translate them, contact us, we will send you resource files to translate and then
we will build your language package from these translated resource files.

Libexif library in PictView plugin: go to http://translationproject.org/domain/libexif.html
find your language version, download translation for last release version
and place it (e.g. cs.po file) to exif directory (e.g. ALTAPTRL\Salamand 3.06\
bin\plugins\pictview\lang\exif). Then open PictView project in your language
directory (e.g. ALTAPTRL\Salamand 3.06\projects\czech\pictview.atp), find string
with ID 1142 (use Ctrl+F, "1142", Enter, F4) and change it to filename of
exif translation  (e.g. cs.po). Use 'E' key in PictView when you have opened
.jpg file from digital camera to see translated strings from libexif library.

WinSCP plugin: please contact us, we will send you two .ini files (texts for core and
for plugin) with texts to translate. Core is translated into many languages, so
you will probably translate only small .ini file for plugin. We will build your
language package from these .ini files.


Other Keyboard Shortcuts
========================

* Space in Resource Symbols ensures the selected row will be visible.
* Ctrl+Up/Down in Text windows switch to previous/next dialog/menu/string table.
