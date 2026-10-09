#pragma once
#include "MainWindow.g.h"
#include "browser/BrowserHost.h"
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
    HWND hwnd_=nullptr;
    xiaochuang::AppState state_;
    std::unique_ptr<miniwindow::BrowserHost> browser_;
    Windows::Foundation::IAsyncAction initialization_{nullptr};
    bool closed_=false;
};
}
namespace winrt::MiniWindow::factory_implementation { struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {}; }
