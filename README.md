<div align="center">
<img src="https://raw.githubusercontent.com/OpenSalamander/salamander/main/src/res/logo.svg" width="70" height="70"> <img src="https://raw.githubusercontent.com/OpenSalamander/salamander/main/src/res/os.svg" height="70" >

#
</div>

![GitHub License](https://img.shields.io/github/license/OpenSalamander/salamander)
[![clang-format Check](https://github.com/OpenSalamander/salamander/actions/workflows/clang-format-check.yml/badge.svg)](https://github.com/OpenSalamander/salamander/actions/workflows/clang-format-check.yml)

Open Salamander is a fast and reliable two-panel file manager for Windows.

## Origin

The original version of Servant Salamander was developed by Petr Šolín during his studies at the Czech Technical University. He released it as freeware in 1997. After graduation, Petr Šolín founded the company [Altap](https://www.altap.cz/) in cooperation with Jan Ryšavý. In 2001 they released the first shareware version of the program. In 2007 a new version was renamed to Altap Salamander 2.5. Many other programmers and translators [contributed](AUTHORS) to the project. In 2019, Altap was acquired by [Fine](https://www.finesoftware.eu/). After this acquisition, Altap Salamander 4.0 was released as freeware. In 2023, the project was open sourced under the GPLv2 license as Open Salamander 5.0.

The name Servant Salamander came about when Petr Šolín and his friend Pavel Schreib were brainstorming name for this project. At that time, the well-known file managers were the aging Norton Commander and the rising Windows Commander. They questioned why a file manager should be named Commander, which implied that it commanded instead of served. This thought led to the birth of the name Servant Salamander.

Please bear with us as Salamander was our first major project where we learned to program in C++. From a technology standpoint, it does not use [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines), smart pointers, [RAII](https://en.cppreference.com/w/cpp/language/raii), [STL](https://github.com/microsoft/STL), or [WIL](https://github.com/microsoft/wil), all of which were just beginning to evolve during the time Salamander was created. Many of the comments are written in Czech, but this is manageable due to recent progress in AI-powered translation. Salamander is a pure WinAPI application and does not use any frameworks, such as MFC.

We would like to thank [Fine company](https://www.finesoftware.eu/) for making the open sourced Salamander release possible.

## Development

### Prerequisites
- Windows 10 or newer
- [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/)
- [Desktop development with C++](https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation?view=msvc-170) workload installed in VS2022
- [Windows 11 (10.0.22621.0) SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/) optional component installed in VS2022

### Optional requirements
- [Git](https://git-scm.com/downloads)
- [PowerShell 7.2](https://learn.microsoft.com/en-us/powershell/scripting/install/installing-powershell-on-windows) or newer
- [HTMLHelp Workshop 1.3](https://learn.microsoft.com/en-us/answers/questions/265752/htmlhelp-workshop-download-for-chm-compiler-instal)
- Set the ```OPENSAL_BUILD_DIR``` environment variable to specify the build directory. Make sure the path has a trailing backslah, e.q. ```D:\Build\OpenSal\```

### Building

Solution ```\src\vcxproj\salamand.sln``` may be built from within Visual Studio or from the command-line using ```\src\vcxproj\rebuild.cmd```.

Use ```\src\vcxproj\!populate_build_dir.cmd``` to populate build directory with files required to run Open Salamander.

### Contributing

This project welcomes contributions to build and enhance Open Salamander!

## Repository Content

```
\convert         Conversion tables for the Convert command
\doc             Documentation
\help            User manual source files
\src             Open Salamander core source code
\src\common      Shared libraries
\src\common\dep  Shared third-party libraries
\src\lang        English resources
\src\plugins     Plugins source code
\src\reglib      Access to Windows Registry files
\src\res         Image resources
\src\salmon      Crash detecting and reporting
\src\salopen     Open files helper
\src\salspawn    Process spawning helper
\src\setup       Installer and uinstaller
\src\sfx7zip     Self-extractor based on 7-Zip
\src\shellext    Shell extension DLL
\src\translator  Translate Salamander UI to other languages
\src\tserver     Trace Server to display info and error messages
\src\vcxproj     Visual Studio project files
\tools           Minor utilities
\translations    Translations into other languages
```

A few Altap Salamander 4.0 plugins are either not included or cannot be compiled. For instance, the PictView engine ```pvw32cnv.dll``` is not open-sourced, so we should consider switching to [WIC](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-about-windows-imaging-codec) or another library. The Encrypt plugin is incompatible with modern SSD disks and has been deprecated. The UnRAR plugin lacks [unrar.dll](https://www.rarlab.com/rar_add.htm), and the FTP plugin is missing [OpenSSL](https://www.openssl.org/) libraries. Both issues are solvable as both projects are open source. To build WinSCP plugin you need Embarcadero C++ Builder.

All the source code uses UTF-8-BOM encoding and is formatted with ```clang-format```. Refer to the ```\normalize.ps1``` script for more information.

## Resources

- [Altap Salamander Website](https://www.altap.cz/)
- Altap Salamander 4.0 [features](https://www.altap.cz/salamander/features/)
- Altap Salamander 4.0 [documentation](https://www.altap.cz/salamander/help/)
- Servant Salamander and Altap Salamander [changelogs](https://www.altap.cz/salamander/changelogs/)
- [User Community Forum](https://forum.altap.cz/)
- Altap Salamander on [Wikipedia](https://en.wikipedia.org/wiki/Altap_Salamander)

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=OpenSalamander/salamander&type=Date)](https://star-history.com/#OpenSalamander/salamander&Date)

## License

Open Salamander is open source software licensed [GPLv2](doc/license_gpl.txt) and later.
Individual [files and libraries](doc/third_party.txt) have a different, but compatible license.
