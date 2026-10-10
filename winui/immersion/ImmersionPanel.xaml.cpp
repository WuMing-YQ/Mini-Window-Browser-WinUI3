#include "pch.h"
#include "../window/AppIcon.h"
#include "ImmersionPanel.xaml.h"
#include "ImmersionPanel.g.cpp"
#include "ImmersionPolicy.h"
#include "../window/ThemeSync.h"
#include "../input/OwnedFocus.h"
#include <microsoft.ui.xaml.window.h>
#include <cwchar>
#include <cmath>
#include <sstream>
#include <iomanip>
namespace winrt::MiniWindow::implementation {
namespace {
std::wstring Percent(double value){std::wostringstream text;text<<std::fixed<<std::setprecision(value==std::floor(value)?0:1)<<value<<L"%";return text.str();}
}
ImmersionPanel::ImmersionPanel(){
 InitializeComponent();check_hresult(try_as<IWindowNative>()->get_WindowHandle(&hwnd_));
    miniwindow::ApplyApplicationIcon(AppWindow(),hwnd_);

 auto presenter=AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.IsAlwaysOnTop(true);presenter.IsMaximizable(false);presenter.IsMinimizable(false);
 const auto dpi=GetDpiForWindow(hwnd_);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&monitor);AppWindow().Resize({std::min(MulDiv(420,dpi,96),static_cast<int>(monitor.rcWork.right-monitor.rcWork.left)-24),std::min(MulDiv(680,dpi,96),static_cast<int>(monitor.rcWork.bottom-monitor.rcWork.top)-24)});
 AppWindow().Closing([this](auto&&,Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args){if(!shuttingDown_){args.Cancel(true);ClosePanel();}});
 Mode().SelectionChanged([this](auto&&,auto&&){if(syncing_||!app_)return;const auto& settings=app_->Snapshot().state.settings;syncing_=true;const auto basis=Mode().SelectedIndex()==0?settings.holeOpacityPercent:settings.autoHideOpacityPercent;Basis().Value(basis);BasisValue().Text(std::to_wstring(basis));syncing_=false;Apply();});
 Basis().ValueChanged([this](auto&&,auto&&){if(syncing_||!app_)return;syncing_=true;BasisValue().Text(std::to_wstring(static_cast<int>(std::round(Basis().Value()))));syncing_=false;Apply();});
 Reduction().ValueChanged([this](auto&&,auto&&){if(syncing_||!app_)return;syncing_=true;ReductionValue().Text(std::to_wstring(static_cast<int>(std::round(Reduction().Value()))));syncing_=false;Apply();});
 Radius().ValueChanged([this](auto&&,auto&&){if(syncing_||!app_)return;RadiusValue().Text(std::to_wstring(static_cast<int>(std::round(Radius().Value()))));Apply();});
 Toggle().Click([this](auto&&,auto&&){if(Apply())app_->ToggleImmersion();});Restore().Click([this](auto&&,auto&&){app_->RestoreNormal();});
 saveTimer_=Microsoft::UI::Xaml::DispatcherTimer();saveTimer_.Interval(std::chrono::milliseconds(300));saveTimer_.Tick([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get()){self->saveTimer_.Stop();self->Flush();}});
 statusTimer_=Microsoft::UI::Xaml::DispatcherTimer();statusTimer_.Interval(std::chrono::milliseconds(50));statusTimer_.Tick([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->UpdateCurrent();});
}
void ImmersionPanel::Bind(HWND owner,miniwindow::AppCoordinator& app,std::function<void()> focus,std::function<bool(UINT,UINT)> capture){
 owner_=owner;SetLastError(ERROR_SUCCESS);if(!SetWindowLongPtrW(hwnd_,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(owner))&&GetLastError()!=ERROR_SUCCESS)throw_hresult(HRESULT_FROM_WIN32(GetLastError()));app_=&app;
 auto changed=[weak=get_weak(),focus](auto&&,auto&&){if(auto self=weak.get())self->DispatcherQueue().TryEnqueue([weak,focus]{if(auto panel=weak.get();panel&&!panel->shuttingDown_)focus();});};Root().GotFocus(changed);Root().LostFocus(changed);Root().PreviewKeyDown([capture](auto&&,Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args){if(capture(static_cast<UINT>(args.Key()),miniwindow::CurrentKeyModifiers()))args.Handled(true);});Refresh();
}
void ImmersionPanel::Open(){if(shuttingDown_||!app_)return;dirty_=false;Refresh();RECT owner{};GetWindowRect(owner_,&owner);RECT panel{};GetWindowRect(hwnd_,&panel);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(owner_,MONITOR_DEFAULTTONEAREST),&monitor);const auto width=panel.right-panel.left,height=panel.bottom-panel.top;const auto x=std::clamp(owner.right+12,monitor.rcWork.left+12,std::max(monitor.rcWork.left+12,monitor.rcWork.right-width-12));const auto y=std::clamp(owner.top,monitor.rcWork.top+12,std::max(monitor.rcWork.top+12,monitor.rcWork.bottom-height-12));SetWindowPos(hwnd_,HWND_TOPMOST,x,y,0,0,SWP_NOSIZE|SWP_NOACTIVATE);Activate();statusTimer_.Start();}
void ImmersionPanel::QueueSave(){pendingSave_=true;saveTimer_.Stop();saveTimer_.Start();}
bool ImmersionPanel::Flush(){if(!pendingSave_||!app_)return true;const auto result=app_->FlushSettings();if(!result.ok){Feedback().Text(result.message);return false;}pendingSave_=false;return true;}
void ImmersionPanel::ClosePanel(){saveTimer_.Stop();if(!Flush())return;statusTimer_.Stop();ShowWindow(hwnd_,SW_HIDE);}
void ImmersionPanel::Shutdown(){shuttingDown_=true;saveTimer_.Stop();statusTimer_.Stop();Close();}
bool ImmersionPanel::Apply(){
 const int basis=static_cast<int>(std::round(Basis().Value()));
 const int reduction=static_cast<int>(std::round(Reduction().Value()));
 const int radius=static_cast<int>(std::round(Radius().Value()));
 const auto style=Mode().SelectedIndex()==0?miniwindow::ImmersionStyle::Hole:miniwindow::ImmersionStyle::AutoHide;
 dirty_=true;auto result=app_->UpdateImmersion(style,basis,reduction,radius,false);if(!result.ok){Feedback().Text(result.message);return false;}dirty_=false;Feedback().Text(L"");QueueSave();Refresh();return true;
}
void ImmersionPanel::Refresh(){if(!app_)return;const auto& snapshot=app_->Snapshot();const auto& settings=snapshot.state.settings;syncing_=true;if(!dirty_){const auto basis=settings.immersionStyle==miniwindow::ImmersionStyle::Hole?settings.holeOpacityPercent:settings.autoHideOpacityPercent;Mode().SelectedIndex(static_cast<int>(settings.immersionStyle));Basis().Value(basis);BasisValue().Text(std::to_wstring(basis));Reduction().Value(settings.hoverReductionPercent);ReductionValue().Text(std::to_wstring(settings.hoverReductionPercent));Radius().Value(settings.holeRadius);RadiusValue().Text(std::to_wstring(settings.holeRadius));}const bool hole=settings.immersionStyle==miniwindow::ImmersionStyle::Hole;Reduction().IsEnabled(!hole);ReductionValue().Opacity(hole?0.4:1.0);Radius().IsEnabled(hole);RadiusValue().Opacity(hole?1.0:0.4);Toggle().Content(box_value(snapshot.state.immersionActive?L"退出沉浸":L"进入沉浸"));syncing_=false;UpdateCurrent();RefreshTheme();}
void ImmersionPanel::UpdateCurrent(){if(!app_)return;const auto& state=app_->Snapshot().state;const auto& settings=state.settings;const auto basis=settings.immersionStyle==miniwindow::ImmersionStyle::Hole?settings.holeOpacityPercent:settings.autoHideOpacityPercent;const auto mode=settings.immersionStyle==miniwindow::ImmersionStyle::Hole?miniwindow::WindowMode::ImmersionHole:miniwindow::WindowMode::ImmersionAutoHide;const auto inside=miniwindow::CalculateImmersionOpacity(mode,basis,settings.hoverReductionPercent,true);Formula().Text(settings.immersionStyle==miniwindow::ImmersionStyle::Hole?L"孔外 "+Percent(basis)+L"，鼠标圆孔完全透明。":L"移出 "+Percent(basis)+L"；移入 "+Percent(basis)+L" × (1 − "+Percent(settings.hoverReductionPercent)+L") = "+Percent(inside));POINT cursor{};RECT bounds{};GetCursorPos(&cursor);GetWindowRect(owner_,&bounds);const bool inBrowser=miniwindow::CursorWithin(bounds,cursor)&&!ContainsScreenPoint(cursor);Current().Text(state.windowHidden?L"窗口已主动隐藏":L"当前显示不透明度："+Percent(miniwindow::CalculateImmersionOpacity(state.windowMode,basis,settings.hoverReductionPercent,inBrowser)));}
void ImmersionPanel::RefreshTheme(){if(app_)miniwindow::ThemeSync::Apply(hwnd_,app_->Snapshot().state.settings.themeMode,Root(),AppWindow());}
bool ImmersionPanel::ForegroundTextFocus(){return GetAncestor(GetForegroundWindow(),GA_ROOT)==hwnd_&&miniwindow::HasOwnedTextFocus(Root().XamlRoot());}
bool ImmersionPanel::ContainsScreenPoint(POINT point)const{RECT bounds{};return IsWindowVisible(hwnd_)&&GetWindowRect(hwnd_,&bounds)&&miniwindow::CursorWithin(bounds,point);}
}
