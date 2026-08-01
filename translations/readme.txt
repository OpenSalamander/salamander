Open Salamander Translation System
====================================

This directory contains the translation infrastructure for Open Salamander.


Directory Structure
-------------------

  translations/
  |
  +-- !build_translations.cmd     - Main script: builds translated .slg files
  |                                 from .slt text archives
  +-- doc/                        - Documentation and reference materials for translator tool
  +-- projects/                   - Translator project directories (.atp and .slt files)
  +-- symbols/                    - Symbol files for translator (Resource IDs, check and ignore lists, etc.)


Automated Build Workflow
------------------------

  1. Build the main solution (salamand.sln) - produces english.slg files
  2. Build the translator ("Utils (Release)" config or translator.sln)
  3. Run: !build_translations.cmd [config] [arch]

     config  - Release or Debug (default: Release)
     arch    - x86 or x64 (default: x86)

  This script invokes translator tool for all `.atp` project files. Translator
  then reads binary `english.slg` file, makes its copy, and merges all the
  translated strings, menus, dialogs into it, as defined in `.slt` file.

  That's why translator needs to have the english .slg files to be compiled.


Interactive Translation Workflow (for translators)
--------------------------------------------------

  1. Build the main solution and translator as above
  2. Run: projects\!setup_projects.bat [config] [arch]
     This generates .atp project files and per-language wrapper scripts.
  3. cd projects\<language>
  4. Run !open_all.bat to open all modules in the translator GUI
     Or open individual .atp files with translator.exe
  5. After translating, run !export_slt.bat to export .slt files


File Formats
------------

  .atp   Translator project files. Define the source (english.slg),
         target (e.g. czech.slg), symbol files, and SLT archive paths
         for a single module in a single language.

  .slt   Text-based translation archives that can be version-controlled
         and diffed. They contain all translatable strings, menus, and
         dialog resources for a single module (main app or plugin).

  .slg   Binary resource DLLs loaded by Salamander at runtime.
         Produced by importing .slt files into english.slg (the original)
         using translator.exe.
