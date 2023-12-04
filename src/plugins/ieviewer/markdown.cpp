// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "registry.h"

#include "ieviewer.h"
#include "dbg.h"

// TODO: MD viewer doesn't display images, one solution is documented in:
// https://blog.kowalczyk.info/article/g9ne/showing-html-from-memory-in-embedded-web-control-on-windows.html
// https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/utils/HtmlWindow.cpp (BSD license)

FILE* OpenMarkdownCSS()
{
    char path[MAX_PATH];
    if (GetModuleFileName(DLLInstance, path, MAX_PATH) == 0)
    {
        TRACE_E("GetModuleFileName() failed");
        return NULL;
    }
    char* name = strrchr(path, '\\');
    if (name == NULL)
    {
        TRACE_E("Extension not found");
        return NULL;
    }
    strcpy(name + 1, "css\\custom.css");
    FILE* fp = fopen(path, "r");
    if (fp == NULL)
    {
        TRACE_I(path << " not found, we will try githubmd.css instead");
        strcpy(name + 1, "css\\githubmd.css");
        fp = fopen(path, "r");
        if (fp == NULL)
        {
            TRACE_I(path << " not found, we will display unstyled html");
            return NULL;
        }
    }
    return fp;
}

const char* extension_names[] = {
    "autolink",
    "strikethrough",
    "table",
    "tagfilter",
    "tasklist",
    NULL,
};

IStream* ConvertMarkdownToHTML(const char* name)
{
    cmark_gfm_core_extensions_ensure_registered();

    int options = CMARK_OPT_DEFAULT; // Default options
    cmark_parser* parser = cmark_parser_new(options);

    for (const char** it = extension_names; *it; ++it)
    {
        const char* extension_name = *it;
        cmark_syntax_extension* syntax_extension = cmark_find_syntax_extension(extension_name);
        if (!syntax_extension)
        {
            TRACE_E("Invalid syntax extension: " << extension_name);
            cmark_release_plugins();
            return NULL;
        }
        cmark_parser_attach_syntax_extension(parser, syntax_extension);
    }

    FILE* fp = fopen(name, "r");
    if (fp == NULL)
    {
        TRACE_E("fopen failed");
        cmark_release_plugins();
        return NULL;
    }

    size_t bytes;
    char buffer[10000];
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        cmark_parser_feed(parser, buffer, bytes);
        if (bytes < sizeof(buffer))
        {
            break;
        }
    }
    fclose(fp);

    cmark_node* doc = cmark_parser_finish(parser);

    char* html = cmark_render_html(doc, options, NULL);

    cmark_node_free(doc);
    cmark_parser_free(parser);

    IStream* oStream = NULL;
    DWORD written;
    HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &oStream);
    if (FAILED(hr))
    {
        TRACE_E("CreateStreamOnHGlobal() failed");
        free(html);
        cmark_release_plugins();
        return NULL;
    }

    char buff[10 * 1024];
    //sprintf_s(buff, "<!DOCTYPE html><html lang=\"cs\" dir=\"ltr\"><head><meta charset=\"utf-8\"><title>zzzz</title><style>\n");
    sprintf_s(buff, "<!DOCTYPE html><html lang=\"cs\" dir=\"ltr\"><head><meta charset=\"utf-8\"><style>\n");
    oStream->Write(buff, (ULONG)strlen(buff), &written);

    // pokud nalezneme CSS, vlozime ho inline
    FILE* fpCSS = OpenMarkdownCSS();
    if (fpCSS != NULL)
    {
        size_t bytes;
        while ((bytes = fread(buff, 1, sizeof(buff), fpCSS)) > 0)
            oStream->Write(buff, (ULONG)bytes, &written);
        fclose(fpCSS);
        sprintf_s(buff, "\n");
        oStream->Write(buff, (ULONG)strlen(buff), &written);
    }

    sprintf_s(buff, "</style></head><body><article class=\"markdown-body\">\n");
    oStream->Write(buff, (ULONG)strlen(buff), &written);
    oStream->Write(html, (ULONG)strlen(html), &written);
    sprintf_s(buff, "</article></body></html>\n");
    oStream->Write(buff, (ULONG)strlen(buff), &written);

    // nastavime ukazovatko na zacatek streamu, IE z nej bude cist
    LARGE_INTEGER seek;
    seek.QuadPart = 0;
    oStream->Seek(seek, STREAM_SEEK_SET, NULL);

    free(html);
    cmark_release_plugins();

    return oStream;
}
