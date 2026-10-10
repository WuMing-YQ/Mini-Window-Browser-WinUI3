#pragma once
#include <windows.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Interop.h>
#include "../AppResources.h"

namespace miniwindow {
inline void ApplyApplicationIcon(const winrt::Microsoft::UI::Windowing::AppWindow& window) {
    if (const auto icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_MINIWINDOW))) {
        window.SetIcon(winrt::Microsoft::UI::GetIconIdFromIcon(icon));
    }
}
}
