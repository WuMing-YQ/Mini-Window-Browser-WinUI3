#include "pch.h"
#include "ManagementWindow.xaml.h"
#include "ManagementWindow.g.cpp"
#include "../window/ThemeSync.h"
#include "../input/OwnedFocus.h"
#include <microsoft.ui.xaml.window.h>
namespace winrt::MiniWindow::implementation {
ManagementWindow::ManagementWindow(){InitializeComponent();
    using namespace Microsoft::UI::Xaml::Controls;
    // Typed symbols are checked by the compiler; arbitrary Icon strings fail at XAML load time.
    WindowItem().Icon(SymbolIcon(Symbol::ViewAll));
    ImmersionItem().Icon(SymbolIcon(Symbol::FullScreen));
    WebItem().Icon(SymbolIcon(Symbol::Globe));
    HotkeysItem().Icon(SymbolIcon(Symbol::Keyboard));
    AppearanceItem().Icon(SymbolIcon(Symbol::FontColor));
    DiagnosticsItem().Icon(SymbolIcon(Symbol::Help));
    BookmarksItem().Icon(SymbolIcon(Symbol::Favorite));
    PresetsItem().Icon(SymbolIcon(Symbol::Save));
check_hresult(try_as<IWindowNative>()->get_WindowHandle(&hwnd_));MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&monitor);const auto dpi=GetDpiForWindow(hwnd_);AppWindow().Resize({std::min(MulDiv(1000,dpi,96),static_cast<int>(monitor.rcWork.right-monitor.rcWork.left)-24),std::min(MulDiv(740,dpi,96),static_cast<int>(monitor.rcWork.bottom-monitor.rcWork.top)-24)});AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>().IsAlwaysOnTop(true);Navigation().SelectionChanged([this](auto&&,auto const& args){if(ready_&&!syncing_&&args.SelectedItem()){auto item=args.SelectedItem().as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();const auto tag=std::stoi(std::wstring(unbox_value<hstring>(item.Tag())));if(!pending_)NavigateAsync(tag>=2?miniwindow::ManagementPage::Settings:static_cast<miniwindow::ManagementPage>(tag),tag>=2?tag-2:-1);}});AppWindow().Closing([this](auto&&,Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args){if(!shuttingDown_){args.Cancel(true);if(!pending_)HideAsync();}});}
void ManagementWindow::Bind(HWND owner,miniwindow::AppCoordinator& app,std::function<void()> focus,std::function<bool(UINT,UINT)> capture,std::function<void(miniwindow::HotkeyAction)> beginCapture,std::function<void()> panel,std::function<void()> cancelCapture){cancelCapture_=std::move(cancelCapture);SetLastError(ERROR_SUCCESS);if(!SetWindowLongPtrW(hwnd_,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(owner))&&GetLastError()!=ERROR_SUCCESS)throw_hresult(HRESULT_FROM_WIN32(GetLastError()));app_=&app;bookmarks_=make_self<BookmarkPage>();presets_=make_self<PresetPage>();bookmarks_->Bind(app);presets_->Bind(app);settings_=make_self<SettingsPage>();settings_->Bind(app,std::move(beginCapture),std::move(panel),cancelCapture_);auto changed=[this,focus](auto&&,auto&&){DispatcherQueue().TryEnqueue([weak=get_weak(),focus]{if(auto self=weak.get();self&&!self->shuttingDown_)focus();});};Root().GotFocus(changed);Root().LostFocus(changed);Root().PreviewKeyDown([capture](auto&&,Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args){if(capture(static_cast<UINT>(args.Key()),miniwindow::CurrentKeyModifiers()))args.Handled(true);});ready_=true;SetPage(page_);RefreshTheme();}
void ManagementWindow::SyncNavigation(){
 syncing_=true;const int tag=page_==miniwindow::ManagementPage::Settings?2+settings_->Category():static_cast<int>(page_);
 auto items=tag>=2?Navigation().MenuItems():Navigation().FooterMenuItems();
 for(auto const& value:items){auto item=value.as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();if(std::stoi(std::wstring(unbox_value<hstring>(item.Tag())))==tag){Navigation().SelectedItem(item);break;}}
 syncing_=false;
}
void ManagementWindow::SetPage(miniwindow::ManagementPage page,int category){
 if(cancelCapture_)cancelCapture_();page_=page;
 if(page==miniwindow::ManagementPage::Bookmarks)PageHost().Content(bookmarks_.as<MiniWindow::BookmarkPage>());
 else if(page==miniwindow::ManagementPage::Presets)PageHost().Content(presets_.as<MiniWindow::PresetPage>());
 else {if(category>=0)settings_->SelectCategory(category);PageHost().Content(settings_.as<MiniWindow::SettingsPage>());}
 SyncNavigation();Refresh();
}
void ManagementWindow::Open(miniwindow::ManagementPage page){if(shuttingDown_||!ready_)return;Activate();if(page!=page_&&!pending_)NavigateAsync(page);else Refresh();}
void ManagementWindow::Refresh(){if(!ready_||shuttingDown_)return;bookmarks_->Refresh();presets_->Refresh();settings_->Refresh();RefreshTheme();}
void ManagementWindow::RefreshTheme(){if(app_)miniwindow::ThemeSync::Apply(hwnd_,app_->Snapshot().state.settings.themeMode,Root(),AppWindow());}
bool ManagementWindow::Dirty(){return app_&&((page_==miniwindow::ManagementPage::Bookmarks&&bookmarks_->Dirty())||(page_==miniwindow::ManagementPage::Presets&&presets_->Dirty())||(page_==miniwindow::ManagementPage::Settings&&settings_->Dirty()));}
bool ManagementWindow::ForegroundTextFocus(){return GetAncestor(GetForegroundWindow(),GA_ROOT)==hwnd_&&miniwindow::HasOwnedTextFocus(Root().XamlRoot());}
Windows::Foundation::IAsyncOperation<bool> ManagementWindow::CanLeaveAsync(){auto lifetime=get_strong();if(Dirty())Activate();if(page_==miniwindow::ManagementPage::Bookmarks)co_return co_await bookmarks_->CanLeaveAsync();if(page_==miniwindow::ManagementPage::Presets)co_return co_await presets_->CanLeaveAsync();if(page_==miniwindow::ManagementPage::Settings)co_return co_await settings_->CanLeaveAsync();co_return true;}
Windows::Foundation::IAsyncAction ManagementWindow::NavigateAsync(miniwindow::ManagementPage page,int category){auto lifetime=get_strong();pending_=true;SyncNavigation();try{if(co_await CanLeaveAsync())SetPage(page,category);}catch(const hresult_error&){}pending_=false;}

Windows::Foundation::IAsyncAction ManagementWindow::HideAsync(){auto lifetime=get_strong();pending_=true;try{if(co_await CanLeaveAsync()){if(cancelCapture_)cancelCapture_();ShowWindow(hwnd_,SW_HIDE);}}catch(const hresult_error&){}pending_=false;}
void ManagementWindow::Shutdown(){shuttingDown_=true;ready_=false;if(cancelCapture_)cancelCapture_();Close();}
}
