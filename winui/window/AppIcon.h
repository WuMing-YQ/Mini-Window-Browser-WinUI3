#pragma once
#include <windows.h>
#include <map>
#include <wil/resource.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Interop.h>
#include "../AppResources.h"

namespace miniwindow {
// LoadIcon returns a shared resource handle valid for the process lifetime.
// Both the tray and WinUI windows use the icon embedded in this executable.
inline HICON ApplicationIcon() {
    return LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_MINIWINDOW));
}
// Called on the UI thread. Keep each owned size alive while windows reference it.
inline void ApplyCaptionIcon(HWND hwnd, UINT dpi = 0) {
    if (!dpi) dpi = GetDpiForWindow(hwnd);
    if (!dpi) dpi = 96;
    const auto size = std::make_pair(GetSystemMetricsForDpi(SM_CXSMICON, dpi),
                                     GetSystemMetricsForDpi(SM_CYSMICON, dpi));
    static std::map<std::pair<int, int>, wil::unique_hicon> icons;
    auto& icon = icons[size];
    // LR_SHARED ignores requested size when an image is already cached. Own and
    // cache each size explicitly, choosing the embedded small-icon frame directly.
    if (!icon) icon.reset(static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_MINIWINDOW), IMAGE_ICON, size.first, size.second, 0)));
    if (icon) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon.get()));
}
inline void ApplyApplicationIcon(const winrt::Microsoft::UI::Windowing::AppWindow& window, HWND hwnd) {
    if (const auto icon = ApplicationIcon()) {
        window.SetIcon(winrt::Microsoft::UI::GetIconIdFromIcon(icon));
    }
    ApplyCaptionIcon(hwnd);
}
}
