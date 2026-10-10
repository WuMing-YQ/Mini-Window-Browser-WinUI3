#include "pch.h"
#include "window/AppIcon.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#include "core/SessionController.h"
#include "core/ChromeLayout.h"
#include "management/UIHelpers.h"
#include "window/ThemeSync.h"
#include "input/OwnedFocus.h"
#include <commctrl.h>
#include <wil/resource.h>
#include <windowsx.h>
#pragma comment(lib,"comctl32.lib")
#include "../src/ConfigStore.h"
#include "../src/WindowFrameBounds.h"
#include <microsoft.ui.xaml.window.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Input.h>
#include <cmath>
#include <fstream>
#include <cwctype>
#include <winrt/Microsoft.UI.Windowing.h>
namespace winrt::MiniWindow::implementation {
MainWindow::MainWindow() {
    InitializeComponent();
    winrt::check_hresult(this->try_as<IWindowNative>()->get_WindowHandle(&hwnd_));
    miniwindow::ApplyApplicationIcon(AppWindow());

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
    windowService_=std::make_unique<miniwindow::WindowService>(hwnd_,state_.settings,[weak=get_weak()](bool visible){if(auto self=weak.get()){
        using Microsoft::UI::Xaml::Visibility;
        self->Chrome().Visibility(visible?Visibility::Visible:Visibility::Collapsed);
        self->ErrorPanel().Visibility(visible&&!self->ErrorText().Text().empty()?Visibility::Visible:Visibility::Collapsed);
    }},[weak=get_weak()]{if(auto self=weak.get()){
        self->UpdateChromeLayout();self->Root().UpdateLayout();self->LayoutBrowser(true);
    }});
    windowService_->SetContentRectProvider([weak=get_weak()]()->RECT{
        if(auto self=weak.get();self&&!self->closed_){
            self->UpdateChromeLayout();self->Root().UpdateLayout();
            auto rect=self->BrowserBounds();POINT origin{};
            if(ClientToScreen(self->hwnd_,&origin)){OffsetRect(&rect,origin.x,origin.y);return rect;}
        }
        return {};
    });
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
    input_->SetWheelHandler([weak=get_weak()](int steps)->miniwindow::OperationResult{if(auto self=weak.get();self&&!self->closed_){RECT current{};MONITORINFO monitor{sizeof(monitor)};if(!GetWindowRect(self->hwnd_,&current)||!GetMonitorInfoW(MonitorFromWindow(self->hwnd_,MONITOR_DEFAULTTONEAREST),&monitor))return {false,L"无法读取窗口工作区。"};const auto dpi=GetDpiForWindow(self->hwnd_);const auto frame=xiaochuang::ReadWindowFrameInsets(self->hwnd_);
        const SIZE minimum{std::max(1L,MulDiv(160,dpi,96)-frame.left-frame.right),std::max(1L,MulDiv(120,dpi,96)-frame.top-frame.bottom)};
        const auto target=miniwindow::ResizeWindowBySteps(frame.Visible(current),steps,monitor.rcMonitor,minimum,self->state_.settings.snapThreshold);
        return self->windowService_->ResizeNormal(frame.Outer(target));}return {false,L"窗口已关闭。"};});
    Root().AddHandler(Microsoft::UI::Xaml::UIElement::PointerWheelChangedEvent(),box_value(Microsoft::UI::Xaml::Input::PointerEventHandler([weak=get_weak()](auto&&,Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args){if(auto self=weak.get();self&&!self->closed_){const auto point=args.GetCurrentPoint(self->Root());if(!args.Handled()&&!point.Properties().IsHorizontalMouseWheel()&&self->input_->TryHandleWheel(point.Properties().MouseWheelDelta(),miniwindow::CurrentKeyModifiers()))args.Handled(true);}})),true);
    InitializeBrowserInput();
    input_->Start();
    Root().Loaded([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get();self&&!self->closed_)self->StartBrowser();});
    Closed([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get()){self->closed_=true;if(self->management_)self->management_->Shutdown();if(self->immersionPanel_)self->immersionPanel_->Shutdown();if(self->coordinator_)self->coordinator_->Subscribe({});self->browserPort_.host=nullptr;if(self->input_)self->input_->Stop();self->systemTheme_.ColorValuesChanged(self->themeToken_);RemoveWindowSubclass(self->hwnd_,WindowProcedure,1);self->windowService_.reset();self->tray_.reset();if(self->browser_)self->browser_->Close();if(self->initialization_)self->initialization_.Cancel();self->browser_.reset();if(self->profileLock_){CloseHandle(self->profileLock_);self->profileLock_=nullptr;}}});
}
void MainWindow::SetDialogActive(bool active){if(active)WebSlot().ReleasePointerCaptures();dialogDepth_=std::max(0,dialogDepth_+(active?1:-1));if(browser_)browser_->SetOccluded(dialogDepth_>0);}
void MainWindow::ShowError(const std::wstring& error) {const auto message=configError_+(configError_.empty()?L"":L"\n")+inputError_+error;ErrorText().Text(message);ErrorPanel().Visibility(message.empty()||(windowService_&&windowService_->Immersive())?Microsoft::UI::Xaml::Visibility::Collapsed:Microsoft::UI::Xaml::Visibility::Visible);}
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
        browser_->SetWheelHandler([weak=get_weak()](int delta,UINT modifiers){if(auto self=weak.get();self&&!self->closed_&&self->input_)return self->input_->TryHandleWheel(delta,modifiers);return false;});
        browserPort_.host=browser_.get();auto runningSettings=state_.settings;runningSettings.renderMode=coordinator_->RunningRenderMode();browser_->Configure(runningSettings);
        browser_->SetDocumentHandler([weak=get_weak()](std::wstring url,std::wstring title){if(auto self=weak.get();self&&!self->closed_){self->coordinator_->OnDocument(std::move(url),std::move(title));}});
        browser_->SetFocusHandler([weak=get_weak()](bool focused){if(auto self=weak.get();self&&!self->closed_){self->webFocused_=focused;self->RefreshOwnedFocus();}});
        browser_->SetNavigationHandlers([weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_){self->input_->CancelGesture();}},[weak=get_weak()](std::wstring url){if(auto self=weak.get();self&&!self->closed_){self->coordinator_->AddTab(std::move(url));}},[weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_){self->input_->CancelGesture();self->coordinator_->CloseTab(static_cast<size_t>(self->state_.currentTabIndex));}});
        browser_->SetPageSignalHandler([weak=get_weak()](miniwindow::PageSignal signal){if(auto self=weak.get();self&&!self->closed_){if(signal.kind==miniwindow::PageSignal::Fullscreen){self->webFullscreen_=signal.flag;self->windowService_->OnWebFullscreen(signal.flag);self->input_->SetImmersiveWebFullscreen(signal.flag&&self->windowService_->Immersive());}else if(signal.kind==miniwindow::PageSignal::VideoAspect&&self->webFullscreen_)self->windowService_->OnWebFullscreen(true,signal.aspect);}});
        LayoutBrowser();browser_->Navigate(state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url);
        co_await browser_->InitializeAsync();
    }catch(const winrt::hresult_error& error){if(!closed_)ShowError(std::wstring(error.message()));}
    catch(const std::exception&){if(!closed_)ShowError(L"浏览器数据目录初始化失败。");}
    starting_=false;
}
Windows::Foundation::IAsyncAction MainWindow::ConfirmCloseAsync(std::wstring error) {
    auto lifetime=get_strong();SetDialogActive(true);auto occlusion=wil::scope_exit([this]{SetDialogActive(false);});
    try {
        Microsoft::UI::Xaml::Controls::ContentDialog dialog;dialog.XamlRoot(Root().XamlRoot());
        dialog.Title(box_value(L"配置未保存"));dialog.Content(box_value(error));
        dialog.PrimaryButtonText(L"返回检查");dialog.CloseButtonText(L"不保存并退出");
        const auto result=co_await dialog.ShowAsync();closePending_=false;
        if(!closed_&&result==Microsoft::UI::Xaml::Controls::ContentDialogResult::None){closeApproved_=true;Close();}
    }catch(const winrt::hresult_error&){closePending_=false;}
}
RECT MainWindow::BrowserBounds() {
    const auto point=WebSlot().TransformToVisual(Root()).TransformPoint({0,0});
    const double scale=GetDpiForWindow(hwnd_)/96.0;
    return {static_cast<LONG>(std::round(point.X*scale)),static_cast<LONG>(std::round(point.Y*scale)),static_cast<LONG>(std::round((point.X+WebSlot().ActualWidth())*scale)),static_cast<LONG>(std::round((point.Y+WebSlot().ActualHeight())*scale))};
}
void MainWindow::LayoutBrowser(bool finishTransition) {
    // Presenter/style/visibility changes emit intermediate size events. Only
    // commit the final layout to WebView, keeping its viewport stable throughout.
    if(!browser_||closed_||(windowService_&&windowService_->ApplyingMode()&&!finishTransition))return;
    browser_->SetBounds(BrowserBounds());browser_->NotifyParentMoved();
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
    const bool normal=mode==miniwindow::WindowMode::Normal||mode==miniwindow::WindowMode::Maximized;
    const auto presenter=AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();
    ExtendsContentIntoTitleBar(false);SetTitleBar(nullptr);presenter.SetBorderAndTitleBar(normal,normal);
    miniwindow::ThemeSync::Apply(hwnd_,state_.settings.themeMode,Root(),AppWindow());
}
void MainWindow::SyncWindowState(){if(!windowService_)return;if(input_)input_->CancelGesture();state_.windowHidden=windowService_->Hidden();state_.immersionActive=windowService_->Immersive();state_.windowMode=windowService_->VisibleMode();if(state_.windowHidden||state_.immersionActive){WebSlot().ReleasePointerCaptures();if(browser_)browser_->CancelMouseInput();}if(browser_)browser_->SetVisible(!state_.windowHidden);if(input_){input_->RefreshRuntime();input_->SetImmersiveWebFullscreen(webFullscreen_&&state_.immersionActive);}if(coordinator_)coordinator_->OnWindowState(state_.windowMode,state_.windowHidden,state_.immersionActive);UpdateChromeLayout();LayoutBrowser();}
void MainWindow::MaximizeClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&){auto result=windowService_->SetMode(windowService_->Mode()==miniwindow::WindowMode::Maximized?miniwindow::WindowMode::Normal:miniwindow::WindowMode::Maximized);SyncWindowState();if(!result.ok)ShowError(result.message);}
void MainWindow::ImmersionClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&){auto result=windowService_->Immersive()?windowService_->SetMode(windowService_->ImmersionReturnMode()):windowService_->SetMode(state_.settings.immersionStyle==miniwindow::ImmersionStyle::Hole?miniwindow::WindowMode::ImmersionHole:miniwindow::WindowMode::ImmersionAutoHide);SyncWindowState();if(!result.ok)ShowError(result.message);}
void MainWindow::HideClick(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&){auto result=windowService_->SetHidden(true);SyncWindowState();if(!result.ok)ShowError(result.message);}
LRESULT CALLBACK MainWindow::WindowProcedure(HWND hwnd,UINT message,WPARAM parameter,LPARAM data,UINT_PTR,DWORD_PTR reference) {
    auto self=reinterpret_cast<MainWindow*>(reference);if(!self||self->closed_)return DefSubclassProc(hwnd,message,parameter,data);
    if(self->input_)self->input_->OnMessage(message,parameter,data);
    if(self->tray_)self->tray_->HandleMessage(message,parameter,data);
    if(self->browser_&&(message==WM_MOUSEWHEEL||message==WM_MOUSEHWHEEL)&&self->browser_->HandleParentWheel(message,parameter,data))return 0;
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

void MainWindow::InitializeBrowserInput(){
    using namespace Microsoft::UI::Xaml::Input;
    auto forward=[weak=get_weak()](UINT kind){return [weak,kind](auto&&,PointerRoutedEventArgs const& args){if(auto self=weak.get();self&&!self->closed_)self->ForwardBrowserPointer(kind,args);};};
    WebSlot().PointerPressed(forward(WM_LBUTTONDOWN));
    WebSlot().PointerReleased(forward(WM_LBUTTONUP));
    WebSlot().PointerMoved(forward(WM_MOUSEMOVE));
    WebSlot().PointerExited(forward(WM_MOUSELEAVE));
    WebSlot().PointerWheelChanged(forward(WM_MOUSEWHEEL));
    auto cancel=[weak=get_weak()](auto&&,PointerRoutedEventArgs const&){if(auto self=weak.get();self&&self->browser_){
        // Release only buttons forwarded to WebView; never leave a stuck drag
        // after capture loss, a dialog, or a mode switch.
        for(auto pair:{std::pair<UINT,UINT>{MK_LBUTTON,WM_LBUTTONUP},{MK_RBUTTON,WM_RBUTTONUP},{MK_MBUTTON,WM_MBUTTONUP},{MK_XBUTTON1,WM_XBUTTONUP},{MK_XBUTTON2,WM_XBUTTONUP}}){
            if(self->browserButtons_&pair.first){self->browserButtons_&=~pair.first;self->browser_->SendMouseInput(pair.second,self->browserButtons_,pair.first==MK_XBUTTON1?XBUTTON1:pair.first==MK_XBUTTON2?XBUTTON2:0,self->browserPointer_);}
        }
        self->browser_->SendMouseInput(WM_MOUSELEAVE,0,0,{});
    }};
    WebSlot().PointerCaptureLost(cancel);WebSlot().PointerCanceled(cancel);
}
void MainWindow::ForwardBrowserPointer(UINT kind,Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args){
    if(!browser_||dialogDepth_>0||windowService_->Immersive()||windowService_->Hidden())return;
    if(args.Pointer().PointerDeviceType()!=Microsoft::UI::Input::PointerDeviceType::Mouse)return;
    const auto point=args.GetCurrentPoint(WebSlot());const auto properties=point.Properties();
    const double scale=GetDpiForWindow(hwnd_)/96.0;
    POINT pixels{static_cast<LONG>(std::lround(point.Position().X*scale)),static_cast<LONG>(std::lround(point.Position().Y*scale))};
    UINT keys=(properties.IsLeftButtonPressed()?MK_LBUTTON:0)|(properties.IsRightButtonPressed()?MK_RBUTTON:0)|(properties.IsMiddleButtonPressed()?MK_MBUTTON:0)|(properties.IsXButton1Pressed()?MK_XBUTTON1:0)|(properties.IsXButton2Pressed()?MK_XBUTTON2:0);
    if(GetKeyState(VK_SHIFT)&0x8000)keys|=MK_SHIFT;if(GetKeyState(VK_CONTROL)&0x8000)keys|=MK_CONTROL;
    UINT data=0,button=0;const bool pressed=kind==WM_LBUTTONDOWN,released=kind==WM_LBUTTONUP;
    if(pressed||released){
        using Microsoft::UI::Input::PointerUpdateKind;
        switch(properties.PointerUpdateKind()){
        case PointerUpdateKind::LeftButtonPressed:case PointerUpdateKind::LeftButtonReleased:button=MK_LBUTTON;kind=pressed?WM_LBUTTONDOWN:WM_LBUTTONUP;break;
        case PointerUpdateKind::RightButtonPressed:case PointerUpdateKind::RightButtonReleased:button=MK_RBUTTON;kind=pressed?WM_RBUTTONDOWN:WM_RBUTTONUP;break;
        case PointerUpdateKind::MiddleButtonPressed:case PointerUpdateKind::MiddleButtonReleased:button=MK_MBUTTON;kind=pressed?WM_MBUTTONDOWN:WM_MBUTTONUP;break;
        case PointerUpdateKind::XButton1Pressed:case PointerUpdateKind::XButton1Released:button=MK_XBUTTON1;kind=pressed?WM_XBUTTONDOWN:WM_XBUTTONUP;data=XBUTTON1;break;
        case PointerUpdateKind::XButton2Pressed:case PointerUpdateKind::XButton2Released:button=MK_XBUTTON2;kind=pressed?WM_XBUTTONDOWN:WM_XBUTTONUP;data=XBUTTON2;break;
        default:return;
        }
        if(pressed){
            WebSlot().CapturePointer(args.Pointer());browser_->Focus();
            const auto now=GetTickCount64();
            if(lastBrowserButton_==button&&now-lastBrowserClick_<=GetDoubleClickTime()&&std::abs(pixels.x-lastBrowserPoint_.x)<=GetSystemMetrics(SM_CXDOUBLECLK)/2&&std::abs(pixels.y-lastBrowserPoint_.y)<=GetSystemMetrics(SM_CYDOUBLECLK)/2){kind+=2;lastBrowserButton_=0;}else{lastBrowserButton_=button;lastBrowserClick_=now;}
        }
    }else if(kind==WM_MOUSEWHEEL){
        if(!properties.IsHorizontalMouseWheel()&&input_->TryHandleWheel(properties.MouseWheelDelta(),miniwindow::CurrentKeyModifiers())){args.Handled(true);return;}
        kind=properties.IsHorizontalMouseWheel()?WM_MOUSEHWHEEL:WM_MOUSEWHEEL;data=static_cast<UINT>(properties.MouseWheelDelta());
    }else if(kind==WM_MOUSELEAVE&&browserButtons_)return;
    if(browser_->SendMouseInput(kind,keys,data,pixels)){
        args.Handled(true);browserPointer_=pixels;browserButtons_=keys&(MK_LBUTTON|MK_RBUTTON|MK_MBUTTON|MK_XBUTTON1|MK_XBUTTON2);
        if(pressed)lastBrowserPoint_=pixels;
    }
    if(released&&!browserButtons_)WebSlot().ReleasePointerCapture(args.Pointer());
}

void MainWindow::RefreshOwnedFocus(){if(input_)input_->RefreshRuntime();}
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
    PresetsButton().Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->ShowPresets(self->PresetsButton());});
    auto settings=[weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->OpenManagement(miniwindow::ManagementPage::Settings);};
    SettingsButton().Click(settings);ErrorSettingsButton().Click(settings);
    TabMenuButton().Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->ShowTabMenu();});
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
    using namespace Microsoft::UI::Xaml;using Controls::Grid;
    const double width=Root().ActualWidth();
    const bool stacked=width<400,tiny=width<200;
    // The address gets the remaining width. Small windows wrap the same
    // controls to additional rows instead of hiding them in an overflow menu.
    Grid::SetRow(AddressField(),0);Grid::SetColumn(AddressField(),stacked?0:1);Grid::SetColumnSpan(AddressField(),stacked?3:1);
    Grid::SetRow(NavigationButtons(),stacked?1:0);Grid::SetColumn(NavigationButtons(),0);
    Grid::SetRow(ActionButtons(),tiny?2:stacked?1:0);Grid::SetColumn(ActionButtons(),tiny?0:2);Grid::SetColumnSpan(ActionButtons(),tiny?3:1);
    ActionButtons().HorizontalAlignment(tiny?HorizontalAlignment::Left:HorizontalAlignment::Right);
    const bool tabs=width>=260;
    Tabs().Visibility(tabs?Visibility::Visible:Visibility::Collapsed);TabMenuButton().Visibility(tabs?Visibility::Collapsed:Visibility::Visible);
    StatusText().Visibility(width>=600&&!(windowService_&&windowService_->Immersive())&&!(windowService_&&windowService_->Hidden())&&Root().ActualHeight()>=240?Visibility::Visible:Visibility::Collapsed);
    StatusText().Text(state_.settings.windowWheelEnabled?L"普通浏览  ·  "+miniwindow::WheelModifierLabel(state_.settings.windowWheelModifiers)+L"+滚轮调整窗口大小":L"普通浏览");
}
void MainWindow::FillBookmarkMenu(Windows::Foundation::Collections::IVector<Microsoft::UI::Xaml::Controls::MenuFlyoutItemBase> const& items){
    const auto& bookmarks=coordinator_->Snapshot().state.bookmarks;
    if(bookmarks.empty())items.Append(MenuCommand(L"暂无书签，点击星标收藏",[]{},false));
    for(size_t i=0;i<bookmarks.size();++i){auto item=MenuCommand(bookmarks[i].title,[weak=get_weak(),i]{if(auto self=weak.get())self->coordinator_->OpenBookmark(i);});Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(item,box_value(bookmarks[i].title+L"\n"+bookmarks[i].url));items.Append(item);}
    items.Append(Microsoft::UI::Xaml::Controls::MenuFlyoutSeparator());items.Append(MenuCommand(L"管理书签…",[weak=get_weak()]{if(auto self=weak.get())self->OpenManagement(miniwindow::ManagementPage::Bookmarks);}));
}
void MainWindow::ShowBookmarks(Microsoft::UI::Xaml::FrameworkElement const& anchor){Microsoft::UI::Xaml::Controls::MenuFlyout flyout;FillBookmarkMenu(flyout.Items());flyout.ShowAt(anchor);}
void MainWindow::FillPresetMenu(Windows::Foundation::Collections::IVector<Microsoft::UI::Xaml::Controls::MenuFlyoutItemBase> const& items){
    const auto& presets=coordinator_->Snapshot().state.presets;
    if(presets.empty())items.Append(MenuCommand(L"暂无预设，可在管理页保存当前场景",[]{},false));
    for(const auto& preset:presets){
        auto item=MenuCommand(preset.name,[weak=get_weak(),name=preset.name]{if(auto self=weak.get())self->ApplyPresetAsync(name);});
        Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(item,box_value(std::to_wstring(preset.tabs.size())+L" 个标签 · "+std::to_wstring(preset.normalRect.right-preset.normalRect.left)+L" × "+std::to_wstring(preset.normalRect.bottom-preset.normalRect.top)));
        items.Append(item);
    }
    items.Append(Microsoft::UI::Xaml::Controls::MenuFlyoutSeparator());
    items.Append(MenuCommand(L"管理预设…",[weak=get_weak()]{if(auto self=weak.get())self->OpenManagement(miniwindow::ManagementPage::Presets);}));
}
void MainWindow::ShowPresets(Microsoft::UI::Xaml::FrameworkElement const& anchor){Microsoft::UI::Xaml::Controls::MenuFlyout flyout;FillPresetMenu(flyout.Items());flyout.ShowAt(anchor);}
Windows::Foundation::IAsyncAction MainWindow::ApplyPresetAsync(std::wstring name){
    auto lifetime=get_strong();
    if(dialogDepth_>0)co_return;
    try {
        if(management_&&!co_await management_->CanLeaveAsync())co_return;
        SetDialogActive(true);auto occlusion=wil::scope_exit([this]{SetDialogActive(false);});
        if(!co_await miniwindow::ui::Confirm(Root().XamlRoot(),L"应用“"+name+L"”？",L"将恢复保存的标签和窗口设置，当前网页输入可能丢失。",L"应用预设"))co_return;
        const auto& presets=coordinator_->Snapshot().state.presets;
        auto found=std::find_if(presets.begin(),presets.end(),[&](const auto& preset){return preset.name==name;});
        if(found==presets.end()){ShowError(L"该预设已被修改或删除，请重新打开列表。");co_return;}
        auto result=coordinator_->ApplyPreset(static_cast<size_t>(found-presets.begin()));SyncWindowState();if(!result.ok)ShowError(result.message);
    }catch(const hresult_error& error){ShowError(std::wstring(error.message()));}
}
void MainWindow::ShowTabMenu(){Microsoft::UI::Xaml::Controls::MenuFlyout flyout;for(size_t i=0;i<state_.tabs.size();++i)flyout.Items().Append(MenuCommand((static_cast<int>(i)==state_.currentTabIndex?L"● ":L"")+state_.tabs[i].title,[weak=get_weak(),i]{if(auto self=weak.get())self->coordinator_->SelectTab(i);}));flyout.Items().Append(Microsoft::UI::Xaml::Controls::MenuFlyoutSeparator());flyout.Items().Append(MenuCommand(L"新建标签",[weak=get_weak()]{if(auto self=weak.get())self->coordinator_->AddTab(self->state_.settings.homeUrl);}));flyout.Items().Append(MenuCommand(L"关闭当前标签",[weak=get_weak()]{if(auto self=weak.get())self->coordinator_->CloseTab(static_cast<size_t>(self->state_.currentTabIndex));}));flyout.Items().Append(MenuCommand(L"关闭其他标签",[weak=get_weak()]{if(auto self=weak.get())self->coordinator_->CloseOtherTabs(static_cast<size_t>(self->state_.currentTabIndex));}));flyout.Items().Append(MenuCommand(L"关闭全部标签",[weak=get_weak()]{if(auto self=weak.get())self->coordinator_->CloseAllTabs();}));flyout.ShowAt(TabMenuButton());}

void MainWindow::OpenManagement(miniwindow::ManagementPage page){
    com_ptr<ManagementWindow> candidate;
    const char* stage="create management window";
    try {
        if(!management_){
            candidate=make_self<ManagementWindow>();
            stage="bind management pages";
            candidate->Bind(hwnd_,*coordinator_,[weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_)self->RefreshOwnedFocus();},[weak=get_weak()](UINT key,UINT modifiers){if(auto self=weak.get();self&&!self->closed_)return self->input_->CaptureKey(key,modifiers);return false;},[weak=get_weak()](auto action){if(auto self=weak.get();self&&!self->closed_)self->input_->BeginCapture(action);},[weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_&&self->input_)self->input_->CancelCapture();});
            // Binding can publish input status. Expose the window only once every page is ready.
            management_=std::move(candidate);
        }
        stage="open management page";
        input_->CancelGesture();management_->Open(page);
    }catch(const hresult_error& error){
        if(candidate){try{candidate->Shutdown();}catch(const hresult_error&){}}
        std::ofstream log(directory_/L"management-error.log",std::ios::app);
        log<<stage<<" HRESULT=0x"<<std::hex<<static_cast<uint32_t>(error.code().value)
           <<" "<<winrt::to_string(error.message())<<"\n";
        ShowError(L"设置与管理窗口打开失败："+std::wstring(error.message()));
    }
}
void MainWindow::OpenImmersionPanel(){
 if(!immersionPanel_){immersionPanel_=make_self<ImmersionPanel>();immersionPanel_->Bind(hwnd_,*coordinator_,[weak=get_weak()]{if(auto self=weak.get();self&&!self->closed_)self->RefreshOwnedFocus();},[weak=get_weak()](UINT key,UINT modifiers){if(auto self=weak.get();self&&!self->closed_)return self->input_->CaptureKey(key,modifiers);return false;});windowService_->SetCursorExclusion([weak=get_weak()](POINT point){if(auto self=weak.get();self&&self->immersionPanel_)return self->immersionPanel_->ContainsScreenPoint(point);return false;});}
 input_->CancelGesture();immersionPanel_->Open();
}
Windows::Foundation::IAsyncAction MainWindow::ToggleBookmarkAsync(){auto lifetime=get_strong();SetDialogActive(true);auto occlusion=wil::scope_exit([this]{SetDialogActive(false);});const auto tab=state_.tabs[static_cast<size_t>(state_.currentTabIndex)];for(size_t i=0;i<state_.bookmarks.size();++i)if(state_.bookmarks[i].url==tab.url){if(co_await miniwindow::ui::Confirm(Root().XamlRoot(),L"取消收藏？",state_.bookmarks[i].title,L"取消收藏"))coordinator_->DeleteBookmark(i);co_return;}coordinator_->SaveBookmark({}, {tab.title.empty()?tab.url:tab.title,tab.url});}
Windows::Foundation::IAsyncAction MainWindow::RequestCloseAsync(){auto lifetime=get_strong();try{if(management_&&!co_await management_->CanLeaveAsync()){closePending_=false;co_return;}state_.session.normalRect=windowService_->NormalRect();state_.session.maximized=windowService_->VisibleMode()==miniwindow::WindowMode::Maximized;const auto result=settings_->Save(state_);if(result.ok){closeApproved_=true;Close();}else{windowService_->RestoreNormal();SyncWindowState();co_await ConfirmCloseAsync(result.message);}}catch(const hresult_error& error){ShowError(std::wstring(error.message()));}closePending_=false;}
}
