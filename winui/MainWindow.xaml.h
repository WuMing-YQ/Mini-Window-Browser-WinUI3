#pragma once
#include "MainWindow.g.h"
#include "browser/BrowserHost.h"
#include "settings/VersionedConfigStore.h"
namespace winrt::MiniWindow::implementation {
struct MainWindow : MainWindowT<MainWindow> {
    MainWindow();
    void AddressKeyDown(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const&);
    void NavigateClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void RetryClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void WebSlotSizeChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::SizeChangedEventArgs const&);
private:
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
    std::filesystem::path directory_,profile_;
    std::wstring configError_;
    HANDLE profileLock_=nullptr;
    Windows::Foundation::IAsyncAction closePrompt_{nullptr};
    bool closed_=false,starting_=false,closeApproved_=false,closePending_=false;
};
}
namespace winrt::MiniWindow::factory_implementation { struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {}; }
