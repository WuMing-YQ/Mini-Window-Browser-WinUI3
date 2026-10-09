#include "InputService.h"
namespace miniwindow {
InputService::InputService(HWND hwnd,AppState& state,std::function<void(HotkeyAction,HotkeyGesture)> action):hwnd_(hwnd),state_(state),action_(std::move(action)){}
InputService::~InputService(){Stop();}
void InputService::Start() {
    if(running_)return;running_=true;
    hooksReady_=hotkeys_.Start(hwnd_,&state_.hotkeys,[this](auto action,auto gesture){Dispatch(action,gesture);});
    guardReady_=guard_.Start(hwnd_,[this](bool){if(running_)RefreshRuntime();});
    SetTimer(hwnd_,kInputTimer,100,nullptr);runtimeKnown_=false;RefreshRuntime();PublishErrors();
}
void InputService::Stop() {
    CancelGesture();CancelCapture();running_=false;
    if(hwnd_)KillTimer(hwnd_,kInputTimer);
    hotkeys_.Stop();guard_.Stop();runtimeKnown_=false;
}
void InputService::PublishErrors() {
    auto errors=hotkeys_.RegistrationErrors();if(!hooksReady_)errors.push_back(L"键盘钩子或鼠标侧键输入初始化失败。");if(!guardReady_)errors.push_back(L"系统输入保护初始化不完整。");
    if(errors_!=errors){errors_=std::move(errors);if(status_)status_();}
}
void InputService::RefreshRuntime() {
    if(state_.windowHidden)CancelGesture();
    if(!running_||refreshing_)return;refreshing_=true;
    guard_.SetEnabled(state_.settings.disableHotkeysOnTyping);guard_.Refresh();
    const bool suppressed=guard_.IsTyping();
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
    CancelGesture();capture_=action;
    hotkeys_.BeginMouseCapture([this](UINT key,UINT modifiers){CaptureKey(key,modifiers);});
    if(status_)status_();
}
void InputService::CancelCapture(){CancelGesture();capture_.reset();hotkeys_.CancelMouseCapture();if(status_)status_();}
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
    if(message==xiaochuang::kMessageInputGuardResult){guard_.HandleAsyncResult(data);RefreshRuntime();return;}
    if(message==xiaochuang::kMessageInputFocusChanged){if(guard_.ConsumePendingEvents())CancelGesture();RefreshRuntime();return;}
    if(message==WM_HOTKEY){RefreshRuntime();hotkeys_.HandleHotkeyMessage(static_cast<int>(parameter));}
    else if(message==xiaochuang::kMessageMouseHotkey){RefreshRuntime();hotkeys_.HandleMouseMessage(parameter,data);}
    else if(message==xiaochuang::kMessageKeyboardHotkey){RefreshRuntime();hotkeys_.HandleKeyboardMessage(parameter,data);}
}
}
