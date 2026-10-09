#include "ThemeSync.h"
#include <unknwn.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <dwmapi.h>
namespace miniwindow {
namespace {
bool SystemDark(){const auto color=winrt::Windows::UI::ViewManagement::UISettings().GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Background);return color.R+color.G+color.B<384;}
winrt::Windows::UI::Color Color(COLORREF value){return {255,GetRValue(value),GetGValue(value),GetBValue(value)};}
}
void ThemeSync::Apply(HWND hwnd,xiaochuang::ThemeMode mode) {
    const bool dark=ResolveDarkTheme(mode,SystemDark());const auto palette=CaptionPalette(dark);const BOOL useDark=dark;
    DwmSetWindowAttribute(hwnd,static_cast<DWMWINDOWATTRIBUTE>(20),&useDark,sizeof(useDark));
    // DWM colors also cover the native caption when AppWindow customization is unavailable.
    DwmSetWindowAttribute(hwnd,static_cast<DWMWINDOWATTRIBUTE>(35),&palette.background,sizeof(palette.background));
    DwmSetWindowAttribute(hwnd,static_cast<DWMWINDOWATTRIBUTE>(36),&palette.foreground,sizeof(palette.foreground));
}

void ThemeSync::Apply(HWND hwnd,xiaochuang::ThemeMode mode,const winrt::Microsoft::UI::Xaml::FrameworkElement& root,const winrt::Microsoft::UI::Windowing::AppWindow& app) {
    using winrt::Microsoft::UI::Xaml::ElementTheme;
    root.RequestedTheme(mode==xiaochuang::ThemeMode::System?ElementTheme::Default:(mode==xiaochuang::ThemeMode::Dark?ElementTheme::Dark:ElementTheme::Light));
    Apply(hwnd,mode);
    const bool dark=ResolveDarkTheme(mode,SystemDark());const auto palette=CaptionPalette(dark);
    if(winrt::Microsoft::UI::Windowing::AppWindowTitleBar::IsCustomizationSupported()) {
        const auto bar=app.TitleBar();const auto background=Color(palette.background),foreground=Color(palette.foreground);
        const auto hover=Color(dark?RGB(55,57,64):RGB(228,231,238));
        bar.BackgroundColor(background);bar.ForegroundColor(foreground);bar.ButtonBackgroundColor(background);bar.ButtonForegroundColor(foreground);
        bar.ButtonHoverBackgroundColor(hover);bar.ButtonHoverForegroundColor(foreground);bar.ButtonPressedBackgroundColor(hover);bar.ButtonPressedForegroundColor(foreground);
        bar.InactiveBackgroundColor(background);bar.InactiveForegroundColor(foreground);bar.ButtonInactiveBackgroundColor(background);bar.ButtonInactiveForegroundColor(foreground);
    }
}
}
