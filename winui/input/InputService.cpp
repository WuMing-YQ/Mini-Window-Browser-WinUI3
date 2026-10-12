#include "InputService.h"
#include <windowsx.h>
namespace miniwindow {
InputService::InputService(HWND hwnd,AppState& state,std::function<void(HotkeyAction,HotkeyGesture)> action):hwnd_(hwnd),state_(state),action_(std::move(action)){}
InputService::~InputService(){Stop();}
void InputService::Start() {
    if(running_)return;running_=true;
    registered_=state_.hotkeys;
    hooksReady_=hotkeys_.Start(hwnd_,&state_.hotkeys,[this](auto action,auto gesture){Dispatch(action,gesture);});
    SetTimer(hwnd_,kInputTimer,100,nullptr);runtimeKnown_=false;RefreshRuntime();PublishErrors();
}
void InputService::Stop() {
    CancelGesture();CancelCapture();running_=false;
    if(hwnd_)KillTimer(hwnd_,kInputTimer);
    hotkeys_.Stop();runtimeKnown_=false;
}
void InputService::PublishErrors() {
    auto errors=hotkeys_.RegistrationErrors();if(!hooksReady_)errors.push_back(L"键盘钩子或鼠标侧键输入初始化失败。");
    if(errors_!=errors){errors_=std::move(errors);if(status_)status_();}
}
void InputService::RefreshRuntime() {
    if(state_.windowHidden)CancelGesture();
    if(!running_||refreshing_)return;refreshing_=true;
    const bool suppressed=Suppressed();
    if(!runtimeKnown_||lastImmersive_!=state_.immersionActive||lastHidden_!=state_.windowHidden||lastBackground_!=state_.settings.backgroundMediaHotkeys||lastSuppressed_!=suppressed) {
        runtimeKnown_=true;lastImmersive_=state_.immersionActive;lastHidden_=state_.windowHidden;lastBackground_=state_.settings.backgroundMediaHotkeys;lastSuppressed_=suppressed;
        if(suppressed)CancelGesture();
        hotkeys_.Refresh(lastHidden_,lastBackground_,lastSuppressed_,lastImmersive_);PublishErrors();
    }
    refreshing_=false;
}
void InputService::Dispatch(HotkeyAction action,HotkeyGesture gesture) {
    if(gesture==HotkeyGesture::HoldStop){if(!hold_||*hold_!=action)return;hold_.reset();if(action_)action_(action,gesture);return;}
    if(!state_.hotkeys[xiaochuang::HotkeyIndex(action)].enabled)return;
    if(gesture==HotkeyGesture::HoldStart){
        if(hold_&&*hold_==action)return;
        // The native manager owns the current hold; do not cancel it from its callback.
        if(hold_){auto previous=*hold_;hold_.reset();if(action_)action_(previous,HotkeyGesture::HoldStop);}
        hold_=action;
    }
    if(action_)action_(action,gesture);
}
void InputService::CancelGesture(){hotkeys_.CancelActiveGesture();if(hold_){auto action=*hold_;hold_.reset();if(action_)action_(action,HotkeyGesture::HoldStop);}}
void InputService::BeginCapture(HotkeyAction action) {
    CancelGesture();capture_=action;RefreshRuntime();
    hotkeys_.BeginMouseCapture([this](UINT key,UINT modifiers){CaptureKey(key,modifiers);});
    if(status_)status_();
}
void InputService::CancelCapture(){CancelGesture();capture_.reset();hotkeys_.CancelMouseCapture();RefreshRuntime();if(status_)status_();}
bool InputService::CaptureKey(UINT key,UINT modifiers) {
    if(!capture_)return false;
    if(key==VK_ESCAPE){CancelCapture();return true;}
    if(key==VK_CONTROL||key==VK_SHIFT||key==VK_MENU||key==VK_LWIN||key==VK_RWIN)return true;
    auto action=*capture_;auto binding=state_.hotkeys[xiaochuang::HotkeyIndex(action)];binding.virtualKey=key;binding.modifiers=modifiers;
    CancelCapture();if(captureHandler_)captureHandler_(action,std::move(binding));return true;
}
void InputService::OnMessage(UINT message,WPARAM parameter,LPARAM data) {
    if(message==WM_ACTIVATE&&LOWORD(parameter)==WA_INACTIVE){CancelGesture();return;}
    if(message==WM_TIMER&&parameter==kInputTimer){RefreshRuntime();hotkeys_.Tick();return;}
    if(message==WM_TIMER&&parameter==xiaochuang::kHotkeyHoldTimerId){hotkeys_.Tick();return;}
    if(message==WM_HOTKEY){RefreshRuntime();hotkeys_.HandleHotkeyMessage(static_cast<int>(parameter));}
    else if(message==xiaochuang::kMessageMouseHotkey){RefreshRuntime();hotkeys_.HandleMouseMessage(parameter,data);}
    else if(message==xiaochuang::kMessageKeyboardHotkey){RefreshRuntime();hotkeys_.HandleKeyboardMessage(parameter,data);}
}
OperationResult InputService::ValidateBinding(HotkeyAction action,const HotkeyBinding& value)const{
 const auto& old=state_.hotkeys[xiaochuang::HotkeyIndex(action)];
 if(!value.enabled||(old.enabled&&old.virtualKey==value.virtualKey&&old.modifiers==value.modifiers)||value.virtualKey==VK_LBUTTON||value.virtualKey==VK_RBUTTON||value.virtualKey==VK_MBUTTON||value.virtualKey==VK_XBUTTON1||value.virtualKey==VK_XBUTTON2)return {};
 constexpr int probe=0x5A10;
 if(!RegisterHotKey(hwnd_,probe,value.modifiers|MOD_NOREPEAT,value.virtualKey))return {false,L"此快捷键被系统或其他程序占用，保留原绑定。"};
 UnregisterHotKey(hwnd_,probe);return {};
}
OperationResult InputService::ReloadBindings(){
 bool changed=false;for(size_t i=0;i<registered_.size();++i){const auto& a=registered_[i];const auto& b=state_.hotkeys[i];if(a.enabled!=b.enabled||a.virtualKey!=b.virtualKey||a.modifiers!=b.modifiers)changed=true;}
 const auto previousErrors=errors_;if(changed){CancelGesture();registered_=state_.hotkeys;runtimeKnown_=false;}RefreshRuntime();
 if(changed)for(const auto& error:errors_)if(std::find(previousErrors.begin(),previousErrors.end(),error)==previousErrors.end())return {false,L"快捷键注册失败："+error};
 return {};
}
}
