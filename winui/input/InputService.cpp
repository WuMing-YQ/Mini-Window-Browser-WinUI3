#include "InputService.h"
#include <windowsx.h>
namespace miniwindow {
InputService::InputService(HWND hwnd,AppState& state,std::function<void(HotkeyAction,HotkeyGesture)> action):hwnd_(hwnd),state_(state),action_(std::move(action)){}
InputService::~InputService(){Stop();}
void InputService::Start() {
    if(running_)return;running_=true;
    wheelOwner_=this;wheelHook_=SetWindowsHookExW(WH_GETMESSAGE,WheelMessageHook,nullptr,GetCurrentThreadId());
    registered_=state_.hotkeys;
    hooksReady_=hotkeys_.Start(hwnd_,&state_.hotkeys,[this](auto action,auto gesture){Dispatch(action,gesture);});
    guardReady_=guard_.Start(hwnd_,[this](bool){if(running_)RefreshRuntime();});
    SetTimer(hwnd_,kInputTimer,100,nullptr);runtimeKnown_=false;RefreshRuntime();PublishErrors();
}
void InputService::Stop() {
    CancelGesture();CancelCapture();running_=false;ResetWheel();
    if(wheelHook_){UnhookWindowsHookEx(wheelHook_);wheelHook_=nullptr;}if(wheelOwner_==this)wheelOwner_=nullptr;
    if(hwnd_)KillTimer(hwnd_,kInputTimer);
    hotkeys_.Stop();guard_.Stop();runtimeKnown_=false;
}
void InputService::PublishErrors() {
    auto errors=hotkeys_.RegistrationErrors();if(running_&&!wheelHook_)errors.push_back(L"滚轮窗口输入路由初始化失败。");if(!hooksReady_)errors.push_back(L"键盘钩子或鼠标侧键输入初始化失败。");if(!guardReady_)errors.push_back(L"系统输入保护初始化不完整。");
    if(errors_!=errors){errors_=std::move(errors);if(status_)status_();}
}
void InputService::RefreshRuntime() {
    RefreshWheelContext();
    if(state_.windowHidden)CancelGesture();
    if(!running_||refreshing_)return;refreshing_=true;
    guard_.SetEnabled(state_.settings.disableHotkeysOnTyping);guard_.Refresh();
    const bool suppressed=Suppressed();
    if(!runtimeKnown_||lastHidden_!=state_.windowHidden||lastBackground_!=state_.settings.backgroundMediaHotkeys||lastSuppressed_!=suppressed) {
        runtimeKnown_=true;lastHidden_=state_.windowHidden;lastBackground_=state_.settings.backgroundMediaHotkeys;lastSuppressed_=suppressed;
        if(suppressed)CancelGesture();
        hotkeys_.Refresh(lastHidden_,lastBackground_,lastSuppressed_);PublishErrors();
    }
    refreshing_=false;
}
void InputService::SetOwnedTextFocus(bool focused){guard_.SetOwnedTextFocus(focused);RefreshRuntime();}
void InputService::OnPageTyping(bool typing){guard_.SetWebTyping(typing);RefreshRuntime();}
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
    CancelGesture();ResetWheel();capture_=action;RefreshRuntime();
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
    if(message==WM_ACTIVATE&&LOWORD(parameter)==WA_INACTIVE){CancelGesture();ResetWheel();return;}
    if(message==WM_TIMER&&parameter==kInputTimer){RefreshRuntime();hotkeys_.Tick();return;}
    if(message==WM_TIMER&&parameter==xiaochuang::kHotkeyHoldTimerId){hotkeys_.Tick();return;}
    if(message==xiaochuang::kMessageInputGuardResult){guard_.HandleAsyncResult(data);RefreshRuntime();return;}
    if(message==xiaochuang::kMessageInputFocusChanged){if(guard_.ConsumePendingEvents())CancelGesture();RefreshRuntime();return;}
    if(message==WM_HOTKEY){RefreshRuntime();hotkeys_.HandleHotkeyMessage(static_cast<int>(parameter));}
    else if(message==xiaochuang::kMessageMouseHotkey){RefreshRuntime();hotkeys_.HandleMouseMessage(parameter,data);}
    else if(message==xiaochuang::kMessageKeyboardHotkey){RefreshRuntime();hotkeys_.HandleKeyboardMessage(parameter,data);}
}
namespace {
UINT WheelModifiers(){UINT value=0;if(GetKeyState(VK_CONTROL)&0x8000)value|=MOD_CONTROL;if(GetKeyState(VK_MENU)&0x8000)value|=MOD_ALT;if(GetKeyState(VK_SHIFT)&0x8000)value|=MOD_SHIFT;if((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)value|=MOD_WIN;return value;}
HWND WheelFocus(HWND main){GUITHREADINFO info{sizeof(info)};return GetGUIThreadInfo(GetWindowThreadProcessId(main,nullptr),&info)?info.hwndFocus:nullptr;}
}
WheelContext InputService::CurrentWheelContext(UINT modifiers)const{
 const auto foreground=GetForegroundWindow(),focus=WheelFocus(hwnd_);
 const bool mainFocus=foreground==hwnd_&&focus&&GetAncestor(focus,GA_ROOT)==hwnd_;
 return {state_.windowMode,state_.windowHidden||!IsWindowVisible(hwnd_),IsZoomed(hwnd_)!=FALSE,mainFocus,!mainFocus,modifiers};
}
void InputService::RefreshWheelContext(){
 const auto modifiers=WheelModifiers();const auto focus=WheelFocus(hwnd_);const auto configured=state_.settings.windowWheelModifiers;
 const bool allowed=running_&&state_.settings.windowWheelEnabled&&!Capturing()&&ShouldResizeWindow(CurrentWheelContext(modifiers),configured);
 if(!allowed||!wheelContextKnown_||focus!=lastWheelFocus_||modifiers!=lastWheelModifiers_||configured!=lastWheelConfigured_)wheel_.Reset();
 wheelContextKnown_=allowed;lastWheelFocus_=focus;lastWheelModifiers_=modifiers;lastWheelConfigured_=configured;
}
bool InputService::TryHandleWheel(int delta,UINT modifiers){
 RefreshWheelContext();
 if(!running_||!wheelHandler_||!state_.settings.windowWheelEnabled||Capturing()||!ShouldResizeWindow(CurrentWheelContext(modifiers),state_.settings.windowWheelModifiers)){ResetWheel();return false;}
 const int steps=wheel_.Add(delta);if(!steps)return true;
 const auto result=wheelHandler_(steps);if(!result.ok){ResetWheel();return false;}return true;
}
LRESULT CALLBACK InputService::WheelMessageHook(int code,WPARAM parameter,LPARAM data){
 auto* owner=wheelOwner_;
 if(code>=0&&parameter==PM_REMOVE&&owner&&data){auto* message=reinterpret_cast<MSG*>(data);DWORD process=0;GetWindowThreadProcessId(message->hwnd,&process);
  if(message->message==WM_MOUSEWHEEL&&process==GetCurrentProcessId()&&GetAncestor(message->hwnd,GA_ROOT)==owner->hwnd_){
   try{if(owner->TryHandleWheel(GET_WHEEL_DELTA_WPARAM(message->wParam),WheelModifiers())){message->message=WM_NULL;message->wParam=0;message->lParam=0;}}catch(...){owner->ResetWheel();}
  }
 }
 return CallNextHookEx(owner?owner->wheelHook_:nullptr,code,parameter,data);
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
