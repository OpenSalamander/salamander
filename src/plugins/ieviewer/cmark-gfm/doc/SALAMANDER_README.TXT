To build required headers from 'build' directory:
1. clone https://github.com/github/cmark-gfm repo
2. set environment variables using "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
3. run nmake

Copy headers to build\src and build\extensions.

---

TODO for the IE Viewer plugin
Rename the plugin to Markup Viewer?
Add help text about Markdown support
In the generated HTML head we could add instructions for IE to render the latest version instead of IE6 (we already write this to the Windows registry, but this would make it certain)
