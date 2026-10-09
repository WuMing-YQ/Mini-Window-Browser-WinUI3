#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#include "core/SessionController.h"
#include "../src/ConfigStore.h"
#include <microsoft.ui.xaml.window.h>
#include <winrt/Windows.System.h>
#include <cmath>
#include <cwctype>
#include <winrt/Microsoft.UI.Windowing.h>
namespace winrt::MiniWindow::implementation {
MainWindow::MainWindow() {
    InitializeComponent();
    winrt::check_hresult(this->try_as<IWindowNative>()->get_WindowHandle(&hwnd_));
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    directory_=std::filesystem::path(executable).parent_path();
    settings_=std::make_unique<miniwindow::VersionedConfigStore>(directory_/L"config.winui.ini",directory_/L"config.ini");
    const auto loaded=settings_->LoadOrMigrate(state_);
    if(!loaded.ok){configError_=loaded.message;ShowError(L"");miniwindow::SessionController(state_).CloseAll();}
    auto rect=state_.session.normalRect;
    SetWindowPos(hwnd_,HWND_TOPMOST,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,SWP_NOACTIVATE);
    Address().Text(state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url);
    AppWindow().Closing([weak=get_weak()](auto&&,Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args){
        auto self=weak.get();if(!self||self->closeApproved_)return;
        if(self->closePending_){args.Cancel(true);return;}
        GetWindowRect(self->hwnd_,&self->state_.session.normalRect);
        const auto result=self->settings_->Save(self->state_);
        if(!result.ok){args.Cancel(true);self->closePending_=true;self->closePrompt_=self->ConfirmCloseAsync(result.message);}
    });
    Root().Loaded([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&!self->closed_)self->StartBrowser();});
    Closed([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get()){self->closed_=true;if(self->browser_)self->browser_->Close();if(self->initialization_)self->initialization_.Cancel();self->browser_.reset();if(self->profileLock_){CloseHandle(self->profileLock_);self->profileLock_=nullptr;}}});
}
void MainWindow::ShowError(const std::wstring& error) {const auto message=configError_.empty()?error:configError_+(error.empty()?L"":L"\n"+error);ErrorText().Text(message);ErrorPanel().Visibility(message.empty()?Microsoft::UI::Xaml::Visibility::Collapsed:Microsoft::UI::Xaml::Visibility::Visible);}
bool MainWindow::AcquireProfile(const std::filesystem::path& path) {
    auto identity=std::filesystem::absolute(path).wstring();for(auto& c:identity)c=static_cast<wchar_t>(towlower(c));
    const auto name=L"Local\\MiniWindow.Profile."+std::to_wstring(std::hash<std::wstring>{}(identity));
    HANDLE lock=CreateMutexW(nullptr,FALSE,name.c_str());const DWORD error=GetLastError();
    if(!lock)return false;if(error==ERROR_ALREADY_EXISTS){CloseHandle(lock);return false;}
    profileLock_=lock;profile_=path;return true;
}
void MainWindow::StartBrowser() {if(closed_||starting_)return;starting_=true;initialization_=InitializeBrowserAsync();}
Windows::Foundation::IAsyncAction MainWindow::InitializeBrowserAsync() {
    auto lifetime=get_strong();
    try {
        if(browser_)browser_->Close();
        if(profile_.empty()) {
            const auto legacy=directory_/L"XiaoChuang.exe.WebView2";
            auto candidate=std::filesystem::exists(legacy)?legacy:directory_/L"data"/L"webview-profile";
            while((candidate==legacy&&miniwindow::LegacyBrowserRunning(directory_))||!AcquireProfile(candidate)) {
                Microsoft::UI::Xaml::Controls::ContentDialog dialog;
                dialog.XamlRoot(Root().XamlRoot());dialog.Title(box_value(L"浏览器数据正在使用"));
                dialog.Content(box_value(L"请关闭旧版或同一目录中的浏览器后重新检查，也可以使用独立测试数据。"));
                dialog.PrimaryButtonText(L"使用独立测试数据");dialog.SecondaryButtonText(L"重新检查");dialog.CloseButtonText(L"暂不打开");
                const auto choice=co_await dialog.ShowAsync();if(closed_)co_return;
                if(choice==Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary){candidate=directory_/L"data"/(L"test-profile-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(GetCurrentProcessId()));}
                else if(choice!=Microsoft::UI::Xaml::Controls::ContentDialogResult::Secondary){ShowError(L"尚未打开网页，可在关闭旧浏览器后点击重试。");starting_=false;co_return;}
            }
        }
        if(closed_)co_return;
        browser_=std::make_unique<miniwindow::BrowserHost>(hwnd_,profile_,[weak=get_weak()](bool,bool,std::wstring error){
            if(auto self=weak.get();self&&!self->closed_) {self->ShowError(error);self->LayoutBrowser();}
        });
        browser_->Configure(state_.settings);
        browser_->SetDocumentHandler([weak=get_weak()](std::wstring url,std::wstring title){if(auto self=weak.get();self&&!self->closed_){auto& tab=self->state_.tabs[static_cast<size_t>(self->state_.currentTabIndex)];tab.url=std::move(url);tab.title=std::move(title);self->Address().Text(tab.url);self->Title(tab.title.empty()?L"小窗浏览器":tab.title);}});
        LayoutBrowser();browser_->Navigate(state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url);
        co_await browser_->InitializeAsync();
    }catch(const winrt::hresult_error& error){if(!closed_)ShowError(std::wstring(error.message()));}
    catch(const std::exception&){if(!closed_)ShowError(L"浏览器数据目录初始化失败。");}
    starting_=false;
}
Windows::Foundation::IAsyncAction MainWindow::ConfirmCloseAsync(std::wstring error) {
    auto lifetime=get_strong();
    try {
        Microsoft::UI::Xaml::Controls::ContentDialog dialog;dialog.XamlRoot(Root().XamlRoot());
        dialog.Title(box_value(L"配置未保存"));dialog.Content(box_value(error));
        dialog.PrimaryButtonText(L"返回检查");dialog.CloseButtonText(L"不保存并退出");
        const auto result=co_await dialog.ShowAsync();closePending_=false;
        if(!closed_&&result==Microsoft::UI::Xaml::Controls::ContentDialogResult::None){closeApproved_=true;Close();}
    }catch(const winrt::hresult_error&){closePending_=false;}
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
