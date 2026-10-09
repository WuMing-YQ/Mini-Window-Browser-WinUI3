#pragma once
#include "ThemeState.h"
#include <unknwn.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Windowing.h>
namespace miniwindow {
struct ThemeSync {
    static void Apply(HWND,xiaochuang::ThemeMode);
    static void Apply(HWND,xiaochuang::ThemeMode,const winrt::Microsoft::UI::Xaml::FrameworkElement&,const winrt::Microsoft::UI::Windowing::AppWindow&);
};
}
