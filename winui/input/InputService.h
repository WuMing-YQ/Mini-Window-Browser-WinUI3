#pragma once
#include "../core/Ports.h"
#include "../../src/InputGuard.h"
#include <optional>
namespace miniwindow {
constexpr UINT_PTR kInputTimer=4403;
class InputService {
public:
    InputService(HWND,AppState&,std::function<void(HotkeyAction,HotkeyGesture)>);
    ~InputService();
    void Start();void Stop();void OnMessage(UINT,WPARAM,LPARAM);
    void SetOwnedTextFocus(bool);void OnPageTyping(bool);void CancelGesture();
    void BeginCapture(HotkeyAction);void CancelCapture();
    bool CaptureKey(UINT virtualKey,UINT modifiers);
    void SetCaptureHandler(std::function<void(HotkeyAction,HotkeyBinding)> handler){captureHandler_=std::move(handler);}
    void SetStatusHandler(std::function<void()> handler){status_=std::move(handler);}
    void Dispatch(HotkeyAction,HotkeyGesture);
    void RefreshRuntime();
    bool Suppressed() const {return guard_.IsTyping();}
    bool Capturing() const {return capture_.has_value();}
    const std::vector<std::wstring>& Errors() const {return errors_;}
    const std::wstring& GuardStatus() const {return guard_.StatusText();}
    void SetImmersiveWebFullscreen(bool active){hotkeys_.SetImmersiveWebFullscreen(active);}
private:
    void PublishErrors();
    HWND hwnd_;AppState& state_;std::function<void(HotkeyAction,HotkeyGesture)> action_;
    xiaochuang::HotkeyManager hotkeys_;xiaochuang::InputGuard guard_;
    std::optional<HotkeyAction> hold_,capture_;
    std::function<void(HotkeyAction,HotkeyBinding)> captureHandler_;
    std::function<void()> status_;std::vector<std::wstring> errors_;
    bool running_=false,refreshing_=false;
    bool runtimeKnown_=false,lastHidden_=false,lastBackground_=false,lastSuppressed_=false;
    bool hooksReady_=true,guardReady_=true;
};
}
