#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <winrt/Microsoft.Windows.Globalization.h>
namespace winrt::MiniWindow::implementation {
App::App() {
    // The portable app ships Simplified Chinese resources only.
    ULONG languageCount = 0;
    winrt::check_bool(SetProcessPreferredUILanguages(MUI_LANGUAGE_NAME, L"zh-CN\0", &languageCount));
    Microsoft::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride(L"zh-CN");
    InitializeComponent();
}
void App::OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {
    window_ = winrt::make<MainWindow>();
    window_.Activate();
}
}
