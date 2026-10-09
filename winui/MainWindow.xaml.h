#pragma once
#include "MainWindow.g.h"
#include <winrt/Windows.UI.ViewManagement.h>
#include "browser/BrowserHost.h"
#include "settings/VersionedConfigStore.h"
#include "window/WindowService.h"
#include "window/TrayController.h"
#include "input/InputService.h"
namespace winrt::MiniWindow::implementation {
struct MainWindow : MainWindowT<MainWindow> {
    MainWindow();
    void AddressKeyDown(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const&);
    void NavigateClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void RetryClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void WebSlotSizeChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::SizeChangedEventArgs const&);
    void MaximizeClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void ImmersionClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void HideClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
private:
    static LRESULT CALLBACK WindowProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    void ApplyFrame(miniwindow::WindowMode);
    void SyncWindowState();
    void HandleHotkey(miniwindow::HotkeyAction,miniwindow::HotkeyGesture);
    void RefreshOwnedFocus();
    void StartBrowser();
    void LayoutBrowser();
    Windows::Foundation::IAsyncAction InitializeBrowserAsync();
    Windows::Foundation::IAsyncAction ConfirmCloseAsync(std::wstring);
    void ShowError(const std::wstring&);
    bool AcquireProfile(const std::filesystem::path&);
    HWND hwnd_=nullptr;
    xiaochuang::AppState state_;
    std::unique_ptr<miniwindow::BrowserHost> browser_;
    Windows::Foundation::IAsyncAction initialization_{nullptr};
    std::unique_ptr<miniwindow::VersionedConfigStore> settings_;
    std::unique_ptr<miniwindow::WindowService> windowService_;
    std::unique_ptr<miniwindow::TrayController> tray_;
    std::unique_ptr<miniwindow::InputService> input_;
    bool webFullscreen_=false,webFocused_=false;
    std::wstring inputError_;
    winrt::Windows::UI::ViewManagement::UISettings systemTheme_;
    winrt::event_token themeToken_{};
    std::filesystem::path directory_,profile_;
    std::wstring configError_;
    HANDLE profileLock_=nullptr;
    Windows::Foundation::IAsyncAction closePrompt_{nullptr};
    bool closed_=false,starting_=false,closeApproved_=false,closePending_=false;
};
}
namespace winrt::MiniWindow::factory_implementation { struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {}; }
