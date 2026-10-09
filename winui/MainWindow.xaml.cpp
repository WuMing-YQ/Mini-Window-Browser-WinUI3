#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#include "core/SessionController.h"
#include "../src/ConfigStore.h"
#include <microsoft.ui.xaml.window.h>
#include <winrt/Windows.System.h>
#include <cmath>
namespace winrt::MiniWindow::implementation {
MainWindow::MainWindow() {
    InitializeComponent();
    winrt::check_hresult(this->try_as<IWindowNative>()->get_WindowHandle(&hwnd_));
    SetWindowPos(hwnd_,HWND_TOPMOST,100,100,1000,700,SWP_NOACTIVATE);
    miniwindow::SessionController(state_).CloseAll();
    Address().Text(state_.tabs[0].url);
    Root().Loaded([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&!self->closed_)self->StartBrowser();});
    Closed([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get()){self->closed_=true;if(self->browser_)self->browser_->Close();if(self->initialization_)self->initialization_.Cancel();self->browser_.reset();}});
}
void MainWindow::StartBrowser() {
    if(closed_)return;
    if(browser_)browser_->Close();
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    const auto profile=std::filesystem::path(executable).parent_path()/L"data"/L"webview-profile";
    browser_=std::make_unique<miniwindow::BrowserHost>(hwnd_,profile,[weak=get_weak()](bool,bool,std::wstring error){
        if(auto self=weak.get();self&&!self->closed_) {
            self->ErrorText().Text(error);self->ErrorPanel().Visibility(error.empty()?Microsoft::UI::Xaml::Visibility::Collapsed:Microsoft::UI::Xaml::Visibility::Visible);
            self->LayoutBrowser();
        }
    });
    browser_->SetDocumentHandler([weak=get_weak()](std::wstring url,std::wstring title){if(auto self=weak.get();self&&!self->closed_){auto& tab=self->state_.tabs[static_cast<size_t>(self->state_.currentTabIndex)];tab.url=std::move(url);tab.title=std::move(title);self->Address().Text(tab.url);self->Title(tab.title.empty()?L"小窗浏览器":tab.title);}});
    LayoutBrowser();
    browser_->Navigate(state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url);
    initialization_=browser_->InitializeAsync();
}
void MainWindow::LayoutBrowser() {
    if(!browser_||closed_)return;
    const auto point=WebSlot().TransformToVisual(Root()).TransformPoint({0,0});
    const double scale=GetDpiForWindow(hwnd_)/96.0;
    RECT bounds{static_cast<LONG>(std::round(point.X*scale)),static_cast<LONG>(std::round(point.Y*scale)),static_cast<LONG>(std::round((point.X+WebSlot().ActualWidth())*scale)),static_cast<LONG>(std::round((point.Y+WebSlot().ActualHeight())*scale))};
    browser_->SetBounds(bounds);browser_->NotifyParentMoved();
}
void MainWindow::AddressKeyDown(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args) {if(args.Key()==Windows::System::VirtualKey::Enter){NavigateClick(nullptr,nullptr);args.Handled(true);}}
void MainWindow::NavigateClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&) {
    if(!browser_)return;
    const auto url=xiaochuang::NormalizeInputUrl(std::wstring(Address().Text()));const auto result=browser_->Navigate(url);
    if(result.ok)state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url=url;
    else{ErrorText().Text(result.message);ErrorPanel().Visibility(Microsoft::UI::Xaml::Visibility::Visible);}
}
void MainWindow::RetryClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&) {StartBrowser();}
void MainWindow::WebSlotSizeChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::SizeChangedEventArgs const&) {LayoutBrowser();}
}
