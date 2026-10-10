#pragma once
#include <windows.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Interop.h>
#include "../AppResources.h"

namespace miniwindow {
// LoadIcon returns a shared resource handle valid for the process lifetime.
// Both the tray and WinUI windows use the icon embedded in this executable.
inline HICON ApplicationIcon() {
    return LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_MINIWINDOW));
}
inline void ApplyApplicationIcon(const winrt::Microsoft::UI::Windowing::AppWindow& window) {
    if (const auto icon = ApplicationIcon()) {
        window.SetIcon(winrt::Microsoft::UI::GetIconIdFromIcon(icon));
    }
}
}
