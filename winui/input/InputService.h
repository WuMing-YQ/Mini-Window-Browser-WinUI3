#pragma once
#include "../core/Ports.h"
#include "../../src/InputGuard.h"
#include <optional>
#include "WindowWheelPolicy.h"
namespace miniwindow {
constexpr UINT_PTR kInputTimer=4403;
class InputService {
public:
    InputService(HWND,AppState&,std::function<void(HotkeyAction,HotkeyGesture)>);
    ~InputService();
    void SetWheelHandler(std::function<OperationResult(int)> handler){wheelHandler_=std::move(handler);}
    bool TryHandleWheel(int delta,UINT modifiers);
    void ResetWheel(){wheel_.Reset();wheelContextKnown_=false;}
    void Start();void Stop();void OnMessage(UINT,WPARAM,LPARAM);
    void SetOwnedTextFocus(bool);void OnPageTyping(bool);void CancelGesture();
    void BeginCapture(HotkeyAction);void CancelCapture();
    bool CaptureKey(UINT virtualKey,UINT modifiers);
    void SetCaptureHandler(std::function<void(HotkeyAction,HotkeyBinding)> handler){captureHandler_=std::move(handler);}
    void SetStatusHandler(std::function<void()> handler){status_=std::move(handler);}
    void Dispatch(HotkeyAction,HotkeyGesture);
    void RefreshRuntime();
    OperationResult ReloadBindings();
    OperationResult ValidateBinding(HotkeyAction,const HotkeyBinding&)const;
    std::wstring CaptureStatus()const{return capture_?L"请按新组合键（支持鼠标侧键），Esc 取消。":L"";}
    bool Suppressed() const {return guard_.IsTyping()||Capturing();}
    bool Capturing() const {return capture_.has_value();}
    const std::vector<std::wstring>& Errors() const {return errors_;}
    const std::wstring& GuardStatus() const {return guard_.StatusText();}
    void SetImmersiveWebFullscreen(bool active){hotkeys_.SetImmersiveWebFullscreen(active);}
private:
    static LRESULT CALLBACK WheelMessageHook(int,WPARAM,LPARAM);
    WheelContext CurrentWheelContext(UINT)const;
    void RefreshWheelContext();
    void PublishErrors();
    inline static thread_local InputService* wheelOwner_=nullptr;
    HHOOK wheelHook_=nullptr;WindowWheelAccumulator wheel_;
    HWND lastWheelFocus_=nullptr;UINT lastWheelModifiers_=0,lastWheelConfigured_=0;bool wheelContextKnown_=false;
    std::function<OperationResult(int)> wheelHandler_;
    HWND hwnd_;AppState& state_;std::function<void(HotkeyAction,HotkeyGesture)> action_;
    xiaochuang::HotkeyManager hotkeys_;xiaochuang::InputGuard guard_;
    std::optional<HotkeyAction> hold_,capture_;
    std::function<void(HotkeyAction,HotkeyBinding)> captureHandler_;
    std::function<void()> status_;std::vector<std::wstring> errors_;
    bool running_=false,refreshing_=false;
    bool runtimeKnown_=false,lastHidden_=false,lastBackground_=false,lastSuppressed_=false;
    bool hooksReady_=true,guardReady_=true;
    std::array<HotkeyBinding,xiaochuang::kHotkeyCount> registered_{};
};
}
