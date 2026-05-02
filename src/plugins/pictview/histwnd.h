// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// CHistogramWindow
//

class CHistogramControl : public CWindow
{
public:
    enum
    {
        Luminosity,
        Red,
        Green,
        Blue,
        RGBSum,
        Background
    };
    enum
    {
        DefaultGraphHeight = 100,
        SeparatorHeight = 3,
        ToneBandHeight = 16,
        DefaultHeight = DefaultGraphHeight + SeparatorHeight + ToneBandHeight,
        DefaultWidth = 256,
    };
    CHistogramControl(LPDWORD* data);
    ~CHistogramControl();

    int GetChannel() { return Channel; }
    BOOL GetShowOtherChannels() { return ShowOtherChannels; }

    int SetChannel(int channel);
    BOOL SetShowOtherChannels(BOOL show);

protected:
    DWORD Colors[6];
    double Data[5][256];
    int Channel;
    BOOL ShowOtherChannels;
    CBackbufferedDC DC;
    HPEN Pen[5];
    HPEN LightHPen;
    HPEN FaceHPen;
    HPEN ShadowHPen;
    void Paint();
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CHistogramWindow
//

class CHistogramWindow : public CWindow
{
public:
    CHistogramWindow(CViewerWindow* viewer, LPDWORD* data);
    void Show();
    static void LoadConfiguration(HKEY key,
                                  CSalamanderRegistryAbstract* registry);
    static void SaveConfiguration(HKEY key,
                                  CSalamanderRegistryAbstract* registry);

protected:
    enum
    {
        cmdChannel = 1,
        cmdLuminosity = cmdChannel + CHistogramControl::Luminosity,
        cmdRed = cmdChannel + CHistogramControl::Red,
        cmdGreen = cmdChannel + CHistogramControl::Green,
        cmdBlue = cmdChannel + CHistogramControl::Blue,
        cmdRGBSum = cmdChannel + CHistogramControl::RGBSum,
        cmdOtherChannels = 6
    };

    struct CConfiguration
    {
        enum
        {
            CurrentVersion = 1
        };
        int Version;
        int WindowX;
        int WindowY;
        int HistogramHeight;
        int HistogramWidth;
        int Channel;
        int ShowOtherChannels;
        CConfiguration() : Version(CurrentVersion),
                           WindowX(-1), WindowY(-1),
                           HistogramHeight(CHistogramControl::DefaultHeight),
                           HistogramWidth(CHistogramControl::DefaultWidth),
                           Channel(CHistogramControl::RGBSum),
                           ShowOtherChannels(TRUE) {}
    };

    CViewerWindow* Viewer;
    int FrameWidth;
    CGUIToolBarAbstract* Toolbar;
    CHistogramControl Histogram;
    static CConfiguration Configuration;
    static LPCTSTR RegistryName;

    BOOL Init();
    void Destroy();
    void Layout();
    void UpdateToolbar();
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
