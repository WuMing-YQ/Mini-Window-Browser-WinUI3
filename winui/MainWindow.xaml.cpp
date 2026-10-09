#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#include "core/SessionController.h"
#include "window/ThemeSync.h"
#include "input/OwnedFocus.h"
#include <commctrl.h>
#include <windowsx.h>
#pragma comment(lib,"comctl32.lib")
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
        if(self->windowService_){self->state_.session.normalRect=self->windowService_->NormalRect();self->state_.session.maximized=self->windowService_->VisibleMode()==miniwindow::WindowMode::Maximized;}
        const auto result=self->settings_->Save(self->state_);
        if(!result.ok){args.Cancel(true);self->closePending_=true;self->closePrompt_=self->ConfirmCloseAsync(result.message);}
    });
    tray_=std::make_unique<miniwindow::TrayController>(hwnd_,[weak=get_weak()](miniwindow::TrayAction action){if(auto self=weak.get();self&&!self->closed_){
        if(action==miniwindow::TrayAction::Exit){self->Close();return;}
        auto result=action==miniwindow::TrayAction::Show?self->windowService_->SetHidden(false):self->windowService_->RestoreNormal();
        self->SyncWindowState();if(!result.ok)self->ShowError(result.message);
    }});
    windowService_=std::make_unique<miniwindow::WindowService>(hwnd_,state_.settings,[weak=get_weak()](bool visible){if(auto self=weak.get())self->Chrome().Visibility(visible?Microsoft::UI::Xaml::Visibility::Visible:Microsoft::UI::Xaml::Visibility::Collapsed);},[weak=get_weak()]{if(auto self=weak.get())self->LayoutBrowser();});
    windowService_->SetFrameHandler([weak=get_weak()](auto mode){if(auto self=weak.get())self->ApplyFrame(mode);});
    windowService_->SetThemeHandler([weak=get_weak()](auto mode){if(auto self=weak.get())miniwindow::ThemeSync::Apply(self->hwnd_,mode,self->Root(),self->AppWindow());});
    windowService_->SetRecoveryHandler([weak=get_weak()](bool immersive,bool hidden,bool minimize){if(auto self=weak.get()){self->tray_->Sync(immersive,hidden,minimize);return !miniwindow::NeedsTray(immersive,hidden,minimize)||self->tray_->Available();}return false;});
    windowService_->SetErrorHandler([weak=get_weak()](auto error){if(auto self=weak.get()){self->SyncWindowState();self->ShowError(error);}});
    windowService_->Initialize(state_.session.normalRect,state_.session.maximized);windowService_->RefreshTheme(state_.settings.themeMode);
    SetWindowSubclass(hwnd_,WindowProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    themeToken_=systemTheme_.ColorValuesChanged([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->DispatcherQueue().TryEnqueue([weak]{if(auto window=weak.get();window&&!window->closed_)window->windowService_->RefreshTheme(window->state_.settings.themeMode);});});
    input_=std::make_unique<miniwindow::InputService>(hwnd_,state_,[weak=get_weak()](auto action,auto gesture){if(auto self=weak.get();self&&!self->closed_)self->HandleHotkey(action,gesture);});
    input_->SetStatusHandler([weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_){self->inputError_.clear();for(const auto& error:self->input_->Errors())self->inputError_+=error+L"\n";self->ShowError(L"");}});
    auto focusChanged=[weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&!self->closed_)self->DispatcherQueue().TryEnqueue([weak]{if(auto window=weak.get();window&&!window->closed_)window->RefreshOwnedFocus();});};
    Root().GotFocus(focusChanged);Root().LostFocus(focusChanged);
    Root().PreviewKeyDown([weak=get_weak()](auto&&,Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args){if(auto self=weak.get();self&&!self->closed_&&self->input_&&self->input_->CaptureKey(static_cast<UINT>(args.Key()),miniwindow::CurrentKeyModifiers()))args.Handled(true);});
    input_->Start();
    Root().Loaded([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&!self->closed_)self->StartBrowser();});
    Closed([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get()){self->closed_=true;if(self->input_)self->input_->Stop();self->systemTheme_.ColorValuesChanged(self->themeToken_);RemoveWindowSubclass(self->hwnd_,WindowProcedure,1);self->windowService_.reset();self->tray_.reset();if(self->browser_)self->browser_->Close();if(self->initialization_)self->initialization_.Cancel();self->browser_.reset();if(self->profileLock_){CloseHandle(self->profileLock_);self->profileLock_=nullptr;}}});
}
void MainWindow::ShowError(const std::wstring& error) {const auto message=configError_+(configError_.empty()?L"":L"\n")+inputError_+error;ErrorText().Text(message);ErrorPanel().Visibility(message.empty()?Microsoft::UI::Xaml::Visibility::Collapsed:Microsoft::UI::Xaml::Visibility::Visible);}
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
        webFocused_=false;if(browser_)browser_->Close();
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
        browser_->SetFocusHandler([weak=get_weak()](bool focused){if(auto self=weak.get();self&&!self->closed_){self->webFocused_=focused;self->RefreshOwnedFocus();}});
        browser_->SetNavigationHandlers([weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_){self->input_->CancelGesture();self->input_->OnPageTyping(false);}},[weak=get_weak()](std::wstring url){if(auto self=weak.get();self&&!self->closed_){miniwindow::SessionController(self->state_).AddTab(url);self->browser_->Navigate(url);self->Address().Text(url);}},[weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_){self->input_->CancelGesture();miniwindow::SessionController(self->state_).CloseTab(static_cast<size_t>(self->state_.currentTabIndex));self->browser_->Navigate(self->state_.tabs[static_cast<size_t>(self->state_.currentTabIndex)].url);}});
        browser_->SetPageSignalHandler([weak=get_weak()](miniwindow::PageSignal signal){if(auto self=weak.get();self&&!self->closed_){if(signal.kind==miniwindow::PageSignal::Typing)self->input_->OnPageTyping(signal.flag);else if(signal.kind==miniwindow::PageSignal::Fullscreen){self->webFullscreen_=signal.flag;self->windowService_->OnWebFullscreen(signal.flag);self->input_->SetImmersiveWebFullscreen(signal.flag&&self->windowService_->Immersive());}else if(signal.kind==miniwindow::PageSignal::VideoAspect)self->windowService_->OnWebFullscreen(true,signal.aspect);}});
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
    browser_->SetBounds(bounds);browser_->NotifyParentMoved();if(windowService_&&windowService_->Mode()==miniwindow::WindowMode::Normal)windowService_->SetChromeHeight(static_cast<int>(std::round(point.Y*scale))+GetSystemMetricsForDpi(SM_CYCAPTION,GetDpiForWindow(hwnd_))+2*GetSystemMetricsForDpi(SM_CYFRAME,GetDpiForWindow(hwnd_)));
}
void MainWindow::AddressKeyDown(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args) {if(args.Key()==Windows::System::VirtualKey::Enter){NavigateClick(nullptr,nullptr);args.Handled(true);}}
void MainWindow::NavigateClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&) {
    if(!browser_)return;
    const auto url=xiaochuang::NormalizeInputUrl(std::wstring(Address().Text()));const auto result=browser_->Navigate(url);
    if(result.ok){state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url=url;browser_->Focus();}
    else{ErrorText().Text(result.message);ErrorPanel().Visibility(Microsoft::UI::Xaml::Visibility::Visible);}
}
void MainWindow::RetryClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&) {StartBrowser();}
void MainWindow::WebSlotSizeChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::SizeChangedEventArgs const&) {LayoutBrowser();}
void MainWindow::ApplyFrame(miniwindow::WindowMode mode) {
    if(mode==miniwindow::WindowMode::Hidden)return;
    const bool normal=mode==miniwindow::WindowMode::Normal;
    const auto presenter=AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();
    ExtendsContentIntoTitleBar(false);SetTitleBar(nullptr);presenter.SetBorderAndTitleBar(normal,normal);
    miniwindow::ThemeSync::Apply(hwnd_,state_.settings.themeMode,Root(),AppWindow());
}
void MainWindow::SyncWindowState(){if(!windowService_)return;if(input_)input_->CancelGesture();state_.windowHidden=windowService_->Hidden();state_.immersionActive=windowService_->Immersive();state_.windowMode=windowService_->VisibleMode();if(browser_)browser_->SetVisible(!state_.windowHidden);if(input_){input_->RefreshRuntime();input_->SetImmersiveWebFullscreen(webFullscreen_&&state_.immersionActive);}LayoutBrowser();}
void MainWindow::MaximizeClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&){auto result=windowService_->SetMode(windowService_->Mode()==miniwindow::WindowMode::Maximized?miniwindow::WindowMode::Normal:miniwindow::WindowMode::Maximized);SyncWindowState();if(!result.ok)ShowError(result.message);}
void MainWindow::ImmersionClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&){auto result=windowService_->Immersive()?windowService_->RestoreNormal():windowService_->SetMode(state_.settings.immersionStyle==miniwindow::ImmersionStyle::Hole?miniwindow::WindowMode::ImmersionHole:miniwindow::WindowMode::ImmersionAutoHide);SyncWindowState();if(!result.ok)ShowError(result.message);}
void MainWindow::HideClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&){auto result=windowService_->SetHidden(true);SyncWindowState();if(!result.ok)ShowError(result.message);}
LRESULT CALLBACK MainWindow::WindowProcedure(HWND hwnd,UINT message,WPARAM parameter,LPARAM data,UINT_PTR,DWORD_PTR reference) {
    auto self=reinterpret_cast<MainWindow*>(reference);if(!self||self->closed_)return DefSubclassProc(hwnd,message,parameter,data);
    if(self->input_)self->input_->OnMessage(message,parameter,data);
    if(message==WM_KILLFOCUS&&self->input_)self->input_->SetOwnedTextFocus(false);
    if(self->tray_)self->tray_->HandleMessage(message,parameter,data);
    if(self->windowService_) {
        if(message==WM_SYSCOMMAND){const auto command=parameter&0xfff0;if(command==SC_MAXIMIZE){self->MaximizeClick(nullptr,nullptr);return 0;}if(command==SC_RESTORE&&self->windowService_->Mode()==miniwindow::WindowMode::Maximized){self->MaximizeClick(nullptr,nullptr);return 0;}if(command==SC_MINIMIZE&&self->state_.settings.useSystemTray){self->HideClick(nullptr,nullptr);return 0;}}
        if(message==WM_ACTIVATE&&LOWORD(parameter)!=WA_INACTIVE)self->windowService_->ReassertTopmost();
        if(message==WM_NCHITTEST&&self->windowService_->Immersive())return HTTRANSPARENT;
        if(message==WM_NCHITTEST&&self->windowService_->VisibleMode()==miniwindow::WindowMode::Maximized&&self->state_.settings.maximizedTopDragEnabled){POINT point{GET_X_LPARAM(data),GET_Y_LPARAM(data)};ScreenToClient(hwnd,&point);if(point.y>=0&&point.y<MulDiv(4,GetDpiForWindow(hwnd),96))return HTCAPTION;}
        if(message==WM_NCLBUTTONDOWN&&parameter==HTCAPTION&&self->windowService_->VisibleMode()==miniwindow::WindowMode::Maximized){auto result=self->windowService_->RestoreForDrag({GET_X_LPARAM(data),GET_Y_LPARAM(data)});self->SyncWindowState();if(!result.ok){self->ShowError(result.message);return 0;}}
        if(message==WM_GETMINMAXINFO){auto info=reinterpret_cast<MINMAXINFO*>(data);const bool immersive=self->windowService_->Immersive();const UINT dpi=GetDpiForWindow(hwnd);info->ptMinTrackSize={MulDiv(immersive?64:160,dpi,96),MulDiv(immersive?64:120,dpi,96)};return 0;}
        self->windowService_->HandleMessage(message,parameter,data);
        if(message==WM_DPICHANGED||message==WM_WINDOWPOSCHANGED)self->LayoutBrowser();
    }
    return DefSubclassProc(hwnd,message,parameter,data);
}

void MainWindow::RefreshOwnedFocus(){if(input_&&GetAncestor(GetForegroundWindow(),GA_ROOT)==hwnd_)input_->SetOwnedTextFocus(!webFocused_&&miniwindow::HasOwnedTextFocus(Root().XamlRoot()));}
void MainWindow::HandleHotkey(miniwindow::HotkeyAction action,miniwindow::HotkeyGesture gesture) {
    using miniwindow::HotkeyAction;using miniwindow::HotkeyGesture;using miniwindow::MediaCommand;
    if(action==HotkeyAction::Immersion){ImmersionClick(nullptr,nullptr);return;}
    if(action==HotkeyAction::ToggleHidden){auto result=windowService_->SetHidden(!state_.windowHidden);SyncWindowState();if(!result.ok)ShowError(result.message);return;}
    if(!browser_)return;
    MediaCommand command=MediaCommand::TogglePlay;
    if(gesture==HotkeyGesture::HoldStop)command=MediaCommand::StopHold;
    else if(action==HotkeyAction::SeekBackward)command=gesture==HotkeyGesture::HoldStart?MediaCommand::HoldBackward:MediaCommand::SeekBackward;
    else if(action==HotkeyAction::SeekForward)command=gesture==HotkeyGesture::HoldStart?MediaCommand::HoldForward:MediaCommand::SeekForward;
    else if(action==HotkeyAction::Previous)command=MediaCommand::Previous;
    else if(action==HotkeyAction::Next)command=MediaCommand::Next;
    browser_->ExecuteMedia(command,state_.settings.holdPlaybackRate);
}

}
