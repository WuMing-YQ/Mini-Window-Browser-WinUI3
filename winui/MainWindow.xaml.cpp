#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#include "core/SessionController.h"
#include "core/ChromeLayout.h"
#include "management/UIHelpers.h"
#include "window/ThemeSync.h"
#include "input/OwnedFocus.h"
#include <commctrl.h>
#include <windowsx.h>
#pragma comment(lib,"comctl32.lib")
#include "../src/ConfigStore.h"
#include <microsoft.ui.xaml.window.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Input.h>
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
        args.Cancel(true);self->closePending_=true;self->closePrompt_=self->RequestCloseAsync();
    });
    tray_=std::make_unique<miniwindow::TrayController>(hwnd_,[weak=get_weak()](miniwindow::TrayAction action){if(auto self=weak.get();self&&!self->closed_){
        if(action==miniwindow::TrayAction::Exit){self->Close();return;}
        if(action==miniwindow::TrayAction::Settings){self->OpenManagement(miniwindow::ManagementPage::Settings);return;}
        if(action==miniwindow::TrayAction::ImmersionControls){self->OpenImmersionPanel();return;}
        auto result=action==miniwindow::TrayAction::Show?self->windowService_->SetHidden(false):self->windowService_->RestoreNormal();
        self->SyncWindowState();if(!result.ok)self->ShowError(result.message);
    }});
    windowService_=std::make_unique<miniwindow::WindowService>(hwnd_,state_.settings,[weak=get_weak()](bool visible){if(auto self=weak.get())self->Chrome().Visibility(visible?Microsoft::UI::Xaml::Visibility::Visible:Microsoft::UI::Xaml::Visibility::Collapsed);},[weak=get_weak()]{if(auto self=weak.get())self->LayoutBrowser();});
    windowService_->SetFrameHandler([weak=get_weak()](auto mode){if(auto self=weak.get())self->ApplyFrame(mode);});
    windowService_->SetThemeHandler([weak=get_weak()](auto mode){if(auto self=weak.get()){miniwindow::ThemeSync::Apply(self->hwnd_,mode,self->Root(),self->AppWindow());if(self->management_)self->management_->RefreshTheme();if(self->immersionPanel_)self->immersionPanel_->RefreshTheme();}});
    windowService_->SetRecoveryHandler([weak=get_weak()](bool immersive,bool hidden,bool minimize){if(auto self=weak.get()){self->tray_->Sync(immersive,hidden,minimize);return !miniwindow::NeedsTray(immersive,hidden,minimize)||self->tray_->Available();}return false;});
    windowService_->SetErrorHandler([weak=get_weak()](auto error){if(auto self=weak.get()){self->SyncWindowState();self->ShowError(error);}});
    windowService_->Initialize(state_.session.normalRect,state_.session.maximized);windowService_->RefreshTheme(state_.settings.themeMode);
    SetWindowSubclass(hwnd_,WindowProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    themeToken_=systemTheme_.ColorValuesChanged([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->DispatcherQueue().TryEnqueue([weak]{if(auto window=weak.get();window&&!window->closed_)window->windowService_->RefreshTheme(window->state_.settings.themeMode);});});
    coordinator_=std::make_unique<miniwindow::AppCoordinator>(browserPort_,*windowService_,*settings_,state_);
    coordinator_->SetRuntimeHandler([weak=get_weak()]()->miniwindow::OperationResult{if(auto self=weak.get()){if(self->input_){auto result=self->input_->ReloadBindings();if(!result.ok)return result;}if(self->browser_){auto result=self->browser_->Configure(self->state_.settings);if(!result.ok)return result;}self->windowService_->RefreshTheme(self->state_.settings.themeMode);return self->windowService_->RefreshVisuals();}return {false,L"窗口已关闭。"};});
    coordinator_->Subscribe([weak=get_weak()](const auto& snapshot){if(auto self=weak.get();self&&!self->closed_)self->RenderSnapshot(snapshot);});
    InitializeChrome();
    input_=std::make_unique<miniwindow::InputService>(hwnd_,state_,[weak=get_weak()](auto action,auto gesture){if(auto self=weak.get();self&&!self->closed_)self->HandleHotkey(action,gesture);});
    input_->SetStatusHandler([weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_){self->inputError_.clear();for(const auto& error:self->input_->Errors())self->inputError_+=error+L"\n";self->ShowError(L"");if(self->coordinator_){self->coordinator_->OnHotkeyErrors(self->input_->Errors());self->coordinator_->OnCaptureStatus(self->input_->CaptureStatus());}}});
    auto focusChanged=[weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&!self->closed_)self->DispatcherQueue().TryEnqueue([weak]{if(auto window=weak.get();window&&!window->closed_)window->RefreshOwnedFocus();});};
    Root().GotFocus(focusChanged);Root().LostFocus(focusChanged);
    Root().PreviewKeyDown([weak=get_weak()](auto&&,Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args){if(auto self=weak.get();self&&!self->closed_&&self->input_&&self->input_->CaptureKey(static_cast<UINT>(args.Key()),miniwindow::CurrentKeyModifiers()))args.Handled(true);});
    input_->SetCaptureHandler([weak=get_weak()](auto action,auto binding){if(auto self=weak.get();self&&!self->closed_)self->coordinator_->UpdateHotkey(action,binding);});
    coordinator_->SetHotkeyValidator([weak=get_weak()](auto action,const auto& binding){if(auto self=weak.get();self&&!self->closed_)return self->input_->ValidateBinding(action,binding);return miniwindow::OperationResult{false,L"窗口已关闭。"};});
    input_->SetWheelHandler([weak=get_weak()](int steps)->miniwindow::OperationResult{if(auto self=weak.get();self&&!self->closed_){RECT current{};MONITORINFO monitor{sizeof(monitor)};if(!GetWindowRect(self->hwnd_,&current)||!GetMonitorInfoW(MonitorFromWindow(self->hwnd_,MONITOR_DEFAULTTONEAREST),&monitor))return {false,L"无法读取窗口工作区。"};const auto dpi=GetDpiForWindow(self->hwnd_);const auto target=miniwindow::ResizeWindowBySteps(current,steps,monitor.rcWork,{MulDiv(160,dpi,96),MulDiv(120,dpi,96)});return self->windowService_->ResizeNormal(target);}return {false,L"窗口已关闭。"};});
    Root().AddHandler(Microsoft::UI::Xaml::UIElement::PointerWheelChangedEvent(),box_value(Microsoft::UI::Xaml::Input::PointerEventHandler([weak=get_weak()](auto&&,Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args){if(auto self=weak.get();self&&!self->closed_){const auto point=args.GetCurrentPoint(self->Root());if(!point.Properties().IsHorizontalMouseWheel()&&self->input_->TryHandleWheel(point.Properties().MouseWheelDelta(),miniwindow::CurrentKeyModifiers()))args.Handled(true);}})),true);
    input_->Start();
    Root().Loaded([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&!self->closed_)self->StartBrowser();});
    Closed([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get()){self->closed_=true;if(self->management_)self->management_->Shutdown();if(self->immersionPanel_)self->immersionPanel_->Shutdown();if(self->coordinator_)self->coordinator_->Subscribe({});self->browserPort_.host=nullptr;if(self->input_)self->input_->Stop();self->systemTheme_.ColorValuesChanged(self->themeToken_);RemoveWindowSubclass(self->hwnd_,WindowProcedure,1);self->windowService_.reset();self->tray_.reset();if(self->browser_)self->browser_->Close();if(self->initialization_)self->initialization_.Cancel();self->browser_.reset();if(self->profileLock_){CloseHandle(self->profileLock_);self->profileLock_=nullptr;}}});
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
        webFocused_=false;browserPort_.host=nullptr;if(browser_)browser_->Close();
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
        browser_=std::make_unique<miniwindow::BrowserHost>(hwnd_,profile_,[weak=get_weak()](bool ready,bool loading,std::wstring error){
            if(auto self=weak.get();self&&!self->closed_) {if(!ready&&!error.empty()){self->windowService_->RestoreNormal();self->SyncWindowState();}self->coordinator_->OnBrowserState(ready,loading,error);self->LayoutBrowser();}
        });
        browserPort_.host=browser_.get();auto runningSettings=state_.settings;runningSettings.renderMode=coordinator_->RunningRenderMode();browser_->Configure(runningSettings);
        browser_->SetDocumentHandler([weak=get_weak()](std::wstring url,std::wstring title){if(auto self=weak.get();self&&!self->closed_){self->coordinator_->OnDocument(std::move(url),std::move(title));}});
        browser_->SetFocusHandler([weak=get_weak()](bool focused){if(auto self=weak.get();self&&!self->closed_){self->webFocused_=focused;self->RefreshOwnedFocus();}});
        browser_->SetNavigationHandlers([weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_){self->input_->CancelGesture();self->input_->OnPageTyping(false);}},[weak=get_weak()](std::wstring url){if(auto self=weak.get();self&&!self->closed_){self->coordinator_->AddTab(std::move(url));}},[weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_){self->input_->CancelGesture();self->coordinator_->CloseTab(static_cast<size_t>(self->state_.currentTabIndex));}});
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
    coordinator_->Navigate(std::wstring(Address().Text()));if(coordinator_->Snapshot().error.empty())browser_->Focus();
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
void MainWindow::SyncWindowState(){if(!windowService_)return;if(input_)input_->CancelGesture();state_.windowHidden=windowService_->Hidden();state_.immersionActive=windowService_->Immersive();state_.windowMode=windowService_->VisibleMode();if(browser_)browser_->SetVisible(!state_.windowHidden);if(input_){input_->RefreshRuntime();input_->SetImmersiveWebFullscreen(webFullscreen_&&state_.immersionActive);}if(coordinator_)coordinator_->OnWindowState(state_.windowMode,state_.windowHidden,state_.immersionActive);UpdateChromeLayout();LayoutBrowser();}
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

void MainWindow::RefreshOwnedFocus(){if(!input_)return;const auto foreground=GetAncestor(GetForegroundWindow(),GA_ROOT);const bool ownMain=foreground==hwnd_&&!webFocused_&&miniwindow::HasOwnedTextFocus(Root().XamlRoot());input_->SetOwnedTextFocus(ownMain||(management_&&management_->ForegroundTextFocus())||(immersionPanel_&&immersionPanel_->ForegroundTextFocus()));}
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

namespace {
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
MenuFlyoutItem MenuCommand(std::wstring title,std::function<void()> action,bool enabled=true){MenuFlyoutItem item;item.Text(title);item.MaxWidth(420);item.IsEnabled(enabled);item.Click([action](auto&&,auto&&){action();});return item;}
}
namespace winrt::MiniWindow::implementation {
void MainWindow::InitializeChrome(){
    Tabs().AddTabButtonClick([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->coordinator_->AddTab(self->state_.settings.homeUrl);});
    Tabs().SelectionChanged([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&!self->syncingTabs_&&self->Tabs().SelectedIndex()>=0)self->coordinator_->SelectTab(static_cast<size_t>(self->Tabs().SelectedIndex()));});
    Tabs().TabCloseRequested([weak=get_weak()](auto&&,auto const& args){if(auto self=weak.get()){uint32_t index=0;if(self->Tabs().TabItems().IndexOf(args.Tab(),index))self->coordinator_->CloseTab(index);}});
    BackButton().Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&self->browser_)self->browser_->GoBack();});
    RefreshButton().Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&self->browser_)self->browser_->RefreshOrStop();});
    StarButton().Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->ToggleBookmarkAsync();});
    BookmarksButton().Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->ShowBookmarks(self->BookmarksButton());});
    PresetsButton().Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->OpenManagement(miniwindow::ManagementPage::Presets);});
    auto settings=[weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->OpenManagement(miniwindow::ManagementPage::Settings);};
    SettingsButton().Click(settings);ErrorSettingsButton().Click(settings);
    TabMenuButton().Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->ShowTabMenu();});
    MoreButton().Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->ShowMoreMenu();});
    Root().SizeChanged([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get()){self->UpdateChromeLayout();self->LayoutBrowser();}});
    RenderSnapshot(coordinator_->Snapshot());
}
void MainWindow::RenderSnapshot(const miniwindow::AppSnapshot& snapshot){
    if(closed_)return;
    ShowError(snapshot.error);const auto& state=snapshot.state;
    const auto& active=state.tabs[static_cast<size_t>(state.currentTabIndex)];
    if(renderedTab_!=state.currentTabIndex||(renderedUrl_!=active.url&&!miniwindow::HasOwnedTextFocus(Root().XamlRoot())))Address().Text(active.url);
    renderedTab_=state.currentTabIndex;renderedUrl_=active.url;
    Title(active.title.empty()?L"小窗浏览器":active.title);ActiveTabTitle().Text(active.title.empty()?L"网页":active.title);
    std::wstring identity;for(const auto& tab:state.tabs)identity+=std::to_wstring(tab.title.size())+L":"+tab.title+L"|"+std::to_wstring(tab.url.size())+L":"+tab.url+L";";
    syncingTabs_=true;
    if(identity!=renderedTabs_){renderedTabs_=std::move(identity);Tabs().TabItems().Clear();for(size_t i=0;i<state.tabs.size();++i){
        Microsoft::UI::Xaml::Controls::TabViewItem item;item.Header(box_value(state.tabs[i].title));item.IsClosable(true);
        Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(item,box_value(state.tabs[i].title+L"\n"+state.tabs[i].url));
        Microsoft::UI::Xaml::Controls::MenuFlyout menu;
        menu.Items().Append(MenuCommand(L"关闭当前标签",[weak=get_weak(),i]{if(auto self=weak.get())self->coordinator_->CloseTab(i);}));
        menu.Items().Append(MenuCommand(L"关闭其他标签",[weak=get_weak(),i]{if(auto self=weak.get())self->coordinator_->CloseOtherTabs(i);}));
        menu.Items().Append(MenuCommand(L"关闭全部标签",[weak=get_weak()]{if(auto self=weak.get())self->coordinator_->CloseAllTabs();}));
        item.ContextFlyout(menu);Tabs().TabItems().Append(item);
    }}
    Tabs().SelectedIndex(state.currentTabIndex);syncingTabs_=false;
    BackButton().IsEnabled(browser_&&browser_->CanGoBack());RefreshButton().Content(box_value(snapshot.loading?L"\xE71A":L"\xE72C"));
    const bool bookmarked=std::any_of(state.bookmarks.begin(),state.bookmarks.end(),[&](const auto& item){return item.url==active.url;});StarButton().Content(box_value(bookmarked?L"\xE735":L"\xE734"));
    if(management_)management_->Refresh();if(immersionPanel_)immersionPanel_->Refresh();UpdateChromeLayout();
}
void MainWindow::UpdateChromeLayout(){
    const auto layout=miniwindow::CalculateChromeLayout(Root().ActualWidth());using Visibility=Microsoft::UI::Xaml::Visibility;
    const bool full=layout==miniwindow::ChromeLayout::Full,wide=full||layout==miniwindow::ChromeLayout::Compact;
    for(auto const& element:{StarButton(),BookmarksButton(),PresetsButton(),SettingsButton()})element.Visibility(full?Visibility::Visible:Visibility::Collapsed);
    BackButton().Visibility(wide?Visibility::Visible:Visibility::Collapsed);RefreshButton().Visibility(wide?Visibility::Visible:Visibility::Collapsed);
    Tabs().Visibility(wide?Visibility::Visible:Visibility::Collapsed);TabMenuButton().Visibility(wide?Visibility::Collapsed:Visibility::Visible);
    StatusText().Visibility(full&&!state_.immersionActive&&!state_.windowHidden&&Root().ActualHeight()>=240?Visibility::Visible:Visibility::Collapsed);
    StatusText().Text(state_.settings.windowWheelEnabled?L"普通浏览  ·  "+miniwindow::WheelModifierLabel(state_.settings.windowWheelModifiers)+L"+滚轮调整窗口大小":L"普通浏览");
}
void MainWindow::FillBookmarkMenu(Windows::Foundation::Collections::IVector<Microsoft::UI::Xaml::Controls::MenuFlyoutItemBase> const& items){
    const auto& bookmarks=coordinator_->Snapshot().state.bookmarks;
    if(bookmarks.empty())items.Append(MenuCommand(L"暂无书签，点击星标收藏",[]{},false));
    for(size_t i=0;i<bookmarks.size();++i){auto item=MenuCommand(bookmarks[i].title,[weak=get_weak(),i]{if(auto self=weak.get())self->coordinator_->OpenBookmark(i);});Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(item,box_value(bookmarks[i].title+L"\n"+bookmarks[i].url));items.Append(item);}
    items.Append(Microsoft::UI::Xaml::Controls::MenuFlyoutSeparator());items.Append(MenuCommand(L"管理书签…",[weak=get_weak()]{if(auto self=weak.get())self->OpenManagement(miniwindow::ManagementPage::Bookmarks);}));
}
void MainWindow::ShowBookmarks(Microsoft::UI::Xaml::FrameworkElement const& anchor){Microsoft::UI::Xaml::Controls::MenuFlyout flyout;FillBookmarkMenu(flyout.Items());flyout.ShowAt(anchor);}
void MainWindow::ShowTabMenu(){Microsoft::UI::Xaml::Controls::MenuFlyout flyout;for(size_t i=0;i<state_.tabs.size();++i)flyout.Items().Append(MenuCommand((static_cast<int>(i)==state_.currentTabIndex?L"● ":L"")+state_.tabs[i].title,[weak=get_weak(),i]{if(auto self=weak.get())self->coordinator_->SelectTab(i);}));flyout.Items().Append(Microsoft::UI::Xaml::Controls::MenuFlyoutSeparator());flyout.Items().Append(MenuCommand(L"新建标签",[weak=get_weak()]{if(auto self=weak.get())self->coordinator_->AddTab(self->state_.settings.homeUrl);}));flyout.Items().Append(MenuCommand(L"关闭当前标签",[weak=get_weak()]{if(auto self=weak.get())self->coordinator_->CloseTab(static_cast<size_t>(self->state_.currentTabIndex));}));flyout.Items().Append(MenuCommand(L"关闭其他标签",[weak=get_weak()]{if(auto self=weak.get())self->coordinator_->CloseOtherTabs(static_cast<size_t>(self->state_.currentTabIndex));}));flyout.Items().Append(MenuCommand(L"关闭全部标签",[weak=get_weak()]{if(auto self=weak.get())self->coordinator_->CloseAllTabs();}));flyout.ShowAt(TabMenuButton());}
void MainWindow::ShowMoreMenu(){
    Microsoft::UI::Xaml::Controls::MenuFlyout flyout;auto items=flyout.Items();
    items.Append(MenuCommand(L"后退",[weak=get_weak()]{if(auto self=weak.get();self&&self->browser_)self->browser_->GoBack();},browser_&&browser_->CanGoBack()));
    items.Append(MenuCommand(L"刷新 / 停止",[weak=get_weak()]{if(auto self=weak.get();self&&self->browser_)self->browser_->RefreshOrStop();}));
    items.Append(MenuCommand(L"收藏当前网页",[weak=get_weak()]{if(auto self=weak.get())self->ToggleBookmarkAsync();}));
    Microsoft::UI::Xaml::Controls::MenuFlyoutSubItem bookmarks;bookmarks.Text(L"书签");FillBookmarkMenu(bookmarks.Items());items.Append(bookmarks);
    items.Append(MenuCommand(L"场景预设",[weak=get_weak()]{if(auto self=weak.get())self->OpenManagement(miniwindow::ManagementPage::Presets);}));
    items.Append(MenuCommand(L"沉浸控制…",[weak=get_weak()]{if(auto self=weak.get())self->OpenImmersionPanel();}));
    items.Append(MenuCommand(L"设置",[weak=get_weak()]{if(auto self=weak.get())self->OpenManagement(miniwindow::ManagementPage::Settings);}));
    items.Append(MenuCommand(L"恢复普通模式",[weak=get_weak()]{if(auto self=weak.get()){self->coordinator_->RestoreNormal();self->SyncWindowState();}}));
    items.Append(MenuCommand(L"隐藏窗口",[weak=get_weak()]{if(auto self=weak.get())self->HideClick(nullptr,nullptr);}));
    items.Append(MenuCommand(L"最小化",[weak=get_weak()]{if(auto self=weak.get())ShowWindow(self->hwnd_,SW_MINIMIZE);}));
    items.Append(MenuCommand(L"退出",[weak=get_weak()]{if(auto self=weak.get())self->Close();}));flyout.ShowAt(MoreButton());
}
void MainWindow::OpenManagement(miniwindow::ManagementPage page){
    if(!management_){management_=make_self<ManagementWindow>();management_->Bind(hwnd_,*coordinator_,[weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_)self->RefreshOwnedFocus();},[weak=get_weak()](UINT key,UINT modifiers){if(auto self=weak.get();self&&!self->closed_)return self->input_->CaptureKey(key,modifiers);return false;},[weak=get_weak()](auto action){if(auto self=weak.get();self&&!self->closed_)self->input_->BeginCapture(action);},[weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_)self->OpenImmersionPanel();});}
    input_->CancelGesture();management_->Open(page);
}
void MainWindow::OpenImmersionPanel(){
 if(!immersionPanel_){immersionPanel_=make_self<ImmersionPanel>();immersionPanel_->Bind(hwnd_,*coordinator_,[weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_)self->RefreshOwnedFocus();},[weak=get_weak()](UINT key,UINT modifiers){if(auto self=weak.get();self&&!self->closed_)return self->input_->CaptureKey(key,modifiers);return false;});windowService_->SetCursorExclusion([weak=get_weak()](POINT point){if(auto self=weak.get();self&&self->immersionPanel_)return self->immersionPanel_->ContainsScreenPoint(point);return false;});}
 input_->CancelGesture();immersionPanel_->Open();
}
Windows::Foundation::IAsyncAction MainWindow::ToggleBookmarkAsync(){auto lifetime=get_strong();const auto tab=state_.tabs[static_cast<size_t>(state_.currentTabIndex)];for(size_t i=0;i<state_.bookmarks.size();++i)if(state_.bookmarks[i].url==tab.url){if(co_await miniwindow::ui::Confirm(Root().XamlRoot(),L"取消收藏？",state_.bookmarks[i].title,L"取消收藏"))coordinator_->DeleteBookmark(i);co_return;}coordinator_->SaveBookmark({}, {tab.title.empty()?tab.url:tab.title,tab.url});}
Windows::Foundation::IAsyncAction MainWindow::RequestCloseAsync(){auto lifetime=get_strong();try{if(management_&&!co_await management_->CanLeaveAsync()){closePending_=false;co_return;}state_.session.normalRect=windowService_->NormalRect();state_.session.maximized=windowService_->VisibleMode()==miniwindow::WindowMode::Maximized;const auto result=settings_->Save(state_);if(result.ok){closeApproved_=true;Close();}else{windowService_->RestoreNormal();SyncWindowState();co_await ConfirmCloseAsync(result.message);}}catch(const hresult_error& error){ShowError(std::wstring(error.message()));}closePending_=false;}
}
