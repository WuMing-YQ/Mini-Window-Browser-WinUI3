#include "WindowService.h"
#include <windowsx.h>
#include <stdexcept>
namespace miniwindow {
WindowService::WindowService(HWND hwnd,AppSettings& settings,std::function<void(bool)> chrome,std::function<void()> layout):hwnd_(hwnd),settings_(settings) {
    controller_.Initialize(hwnd_,&settings_,{100,100,1100,700},false,82,std::move(chrome),std::move(layout));
}
WindowService::~WindowService(){controller_.Shutdown();}
void WindowService::Initialize(RECT rect,bool maximized){ApplyGeometry(rect,maximized);}
bool WindowService::SyncRecovery(bool immersion,bool hidden){return !recovery_||recovery_(immersion||recoveryForced_,hidden,settings_.useSystemTray);}
OperationResult WindowService::Change(WindowMode target,std::function<void()> operation) {
    const bool immersive=target==WindowMode::ImmersionHole||target==WindowMode::ImmersionAutoHide;
    if((immersive||target==WindowMode::Hidden)&&!SyncRecovery(immersive,target==WindowMode::Hidden))return {false,L"托盘恢复入口不可用，未隐藏窗口。"};
    auto previous=controller_;const auto previousStyle=settings_.immersionStyle;RECT geometry{};GetWindowRect(hwnd_,&geometry);controller_.ClearOperationError();std::wstring message;
    try {if(frame_)frame_(target);operation();message=controller_.OperationError();}catch(...){message=L"WinUI 窗口框架更新失败。";}
    if(!message.empty()) {
        controller_=std::move(previous);settings_.immersionStyle=previousStyle;controller_.ClearOperationError();recoveryForced_=true;
        try {if(frame_)frame_(controller_.Mode());}catch(...){message+=L" 窗口标题栏恢复失败。";}
        controller_.Reapply();
        // Geometry is restored even when another recovery API reports an error.
        controller_.RestoreGeometry(geometry);
        if(!controller_.OperationError().empty())message+=L" 恢复："+controller_.OperationError();
        SyncRecovery(controller_.IsImmersive(),controller_.IsHidden());if(error_)error_(message);return {false,std::move(message)};
    }
    recoveryForced_=false;SyncRecovery(controller_.IsImmersive(),controller_.IsHidden());RefreshTheme(theme_);return {};
}
OperationResult WindowService::SetMode(WindowMode mode) {
    if(mode==WindowMode::Hidden)return SetHidden(true);
    return Change(mode,[&]{
        controller_.SetHidden(false);
        if(mode==WindowMode::Normal){controller_.SetImmersion(false);if(controller_.IsSeamlessMaximized())controller_.ToggleMaximized();}
        else if(mode==WindowMode::Maximized){controller_.SetImmersion(false);if(!controller_.IsSeamlessMaximized())controller_.ToggleMaximized();}
        else {settings_.immersionStyle=mode==WindowMode::ImmersionHole?ImmersionStyle::Hole:ImmersionStyle::AutoHide;controller_.SetImmersion(true);}
    });
}
OperationResult WindowService::SetHidden(bool hidden) {return Change(hidden?WindowMode::Hidden:(controller_.IsImmersive()?(settings_.immersionStyle==ImmersionStyle::Hole?WindowMode::ImmersionHole:WindowMode::ImmersionAutoHide):(controller_.IsSeamlessMaximized()?WindowMode::Maximized:WindowMode::Normal)),[&]{controller_.SetHidden(hidden);});}
OperationResult WindowService::RestoreNormal(){return SetMode(WindowMode::Normal);}
OperationResult WindowService::ApplyGeometry(RECT rect,bool maximized){return Change(maximized?WindowMode::Maximized:WindowMode::Normal,[&]{controller_.SetHidden(false);controller_.ApplyPresetGeometry(rect,maximized);});}
OperationResult WindowService::RefreshVisuals(){return Change(controller_.Mode(),[&]{controller_.SetSystemTray(settings_.useSystemTray);controller_.RefreshVisualState();});}
OperationResult WindowService::RestoreForDrag(POINT cursor) {
    if(!settings_.maximizedTopDragEnabled||controller_.IsHidden()||!controller_.IsSeamlessMaximized())return {false,L"最大化顶部拖动未启用。"};
    return Change(WindowMode::Normal,[&]{if(!controller_.RestoreMaximizedForDrag(cursor))throw std::runtime_error("Unable to restore drag geometry");});
}
void WindowService::RefreshTheme(xiaochuang::ThemeMode theme){theme_=theme;if(themeHandler_)themeHandler_(theme);}
void WindowService::OnWebFullscreen(bool fullscreen,double aspect){Change(controller_.Mode(),[&]{if(fullscreen){controller_.EnterWebFullscreen();if(aspect>0)controller_.FitWebFullscreenAspect(aspect);}else controller_.LeaveWebFullscreen();});}
void WindowService::HandleMessage(UINT message,WPARAM parameter,LPARAM data) {
    if(message==WM_TIMER&&parameter==xiaochuang::kImmersionTimerId){controller_.ClearOperationError();controller_.OnImmersionTimer();if(!controller_.OperationError().empty()){auto failure=controller_.OperationError();RestoreNormal();recoveryForced_=true;SyncRecovery(false,false);if(error_)error_(failure);}}
    else if(message==WM_MOVE||message==WM_SIZE){controller_.UpdateNormalRectFromWindow();}
    else if(message==WM_MOVING)controller_.SnapMovingRect(reinterpret_cast<RECT*>(data),(GetKeyState(VK_CONTROL)&0x8000)!=0);
    else if(message==WM_SIZING)controller_.ConstrainWebFullscreenSizing(static_cast<UINT>(parameter),reinterpret_cast<RECT*>(data));
    else if(message==WM_DISPLAYCHANGE){if(controller_.IsSeamlessMaximized())ApplyGeometry(controller_.NormalRect(),true);}
}
}
