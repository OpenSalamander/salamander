----------------------------------------------------------------------

Zip2Sfx 2.9

Copyright © 2000-2023 Open Salamander Authors   Home page: www.altap.cz
                                            E-mail:    support@altap.cz

----------------------------------------------------------------------

Zip2Sfx is a tool which converts a ZIP archive to the self-extracting
archive.


Contents
-------
1. Introduction
2. Usage
3. Sfx packages
4. Settings files
5. Notes


Introduction
------------

What is a self-extracting archive?
Self-extracting archive is an executable file (unpacker), which
contains also archive data of the whole ZIP file from which was
created. Self-extracting archive could be extracted without need of
any other ZIP unpacker.

How does Zip2Sfx archive work?
It takes the executable code from the sfx package (file with extension
.sfx) and append the archive data to the new executable which is
created.


Usage
-----

zip2sfx [options] zipfile [exefile]


'zipfile' is the archive you want self-extracting archive create from.

'exefile' is the filename (optionally with full path) of the new
          self-extracting archive you wish to create.

Options are:
-o      Automatically overwrite output file.
-p<sfxpackage>  Use <sfxpackage> file containing executable and
            language specific data.
-s<settings>    Use given settings file containing advanced options.


Notes:

Values in the [] are not mandatory and could be omitted.

Switches '-p' and '-s' cannot be combined, you can use only one of
them.

Between switches and filenames there is no space. Write filename right
after '-p' or '-s'.

File names containing spaces must be enclosed in double quotes.
For example:
    zip2exe -ssample.set "C:\My Files\my archive.zip"

Settings file used by Zip2Sfx is of the same format as the settings
exported from ZIP plugin for Open Salamander.


Sfx packages
------------

Sfx package has usually extension .sfx, it contains executable code
and language dependent data, like the main sfx dialog and the default
text values - dialog title, text shown in the dialog and the text of
the 'extract button'.

For each language there is one standalone sfx package. Sfx packages
are distributed with ZIP plugin and Zip2Sfx.

If you are user of Open Salamander the installed sfx packages are
stored in the 'salamand\plugins\zip\sfx' directory, where salamand is
the directory, where you have installed Open Salamander.

To create self-extracting archive you must always specify the sfx
package you want to use.
There are three ways how you can do it:

1. You can create a system  environment variable SFX_DEFPACKAGE and
set it's value to the sfx package filename (if the package is not the
current directory when starting zip2sfx, you must type the filename
with full path). This variable is used when the sfx package is not
specified by any other way. If you don't how to create a system
environment variable see notes at the end of this document.
2. Run zip2sfx.exe with the switch '-p' and specify sfx package
directly.
3. Run zip2sfx.exe with the switch '-s' and specify sfx package in the
settings file.


Settings files
--------------

Settings file (usually with extension .set) are text files containing
advanced settings for the self-extractor.
Settings file could be also exported/imported from/to ZIP plugin for
Open Salamander.
For more details on settings files content and syntax see sample.set.


Notes
-----

Creating system environment variable:

Windows 7 (and newer) users:
  1) Open Start menu, right-click on Computer, go to Properties.
  2) In the left pane, click on Advanced system settings.
  3) On the Advanced tab at the bottom right click Environment Variables.
