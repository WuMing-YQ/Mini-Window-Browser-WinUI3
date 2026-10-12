#pragma once
#include "../core/Ports.h"
#include <optional>
namespace miniwindow {
constexpr UINT_PTR kInputTimer=4403;
class InputService {
public:
    InputService(HWND,AppState&,std::function<void(HotkeyAction,HotkeyGesture)>);
    ~InputService();
    void Start();void Stop();void OnMessage(UINT,WPARAM,LPARAM);
    void CancelGesture();
    void BeginCapture(HotkeyAction);void CancelCapture();
    bool CaptureKey(UINT virtualKey,UINT modifiers);
    void SetCaptureHandler(std::function<void(HotkeyAction,HotkeyBinding)> handler){captureHandler_=std::move(handler);}
    void SetStatusHandler(std::function<void()> handler){status_=std::move(handler);}
    void Dispatch(HotkeyAction,HotkeyGesture);
    void RefreshRuntime();
    OperationResult ReloadBindings();
    OperationResult ValidateBinding(HotkeyAction,const HotkeyBinding&)const;
    std::wstring CaptureStatus()const{return capture_?L"请按新组合键（支持鼠标侧键），Esc 取消。":L"";}
    bool Suppressed() const {return Capturing();}
    bool Capturing() const {return capture_.has_value();}
    const std::vector<std::wstring>& Errors() const {return errors_;}
    void SetImmersiveWebFullscreen(bool active){hotkeys_.SetImmersiveWebFullscreen(active);}
private:
    void PublishErrors();
    HWND hwnd_;AppState& state_;std::function<void(HotkeyAction,HotkeyGesture)> action_;
    xiaochuang::HotkeyManager hotkeys_;
    std::optional<HotkeyAction> hold_,capture_;
    std::function<void(HotkeyAction,HotkeyBinding)> captureHandler_;
    std::function<void()> status_;std::vector<std::wstring> errors_;
    bool running_=false,refreshing_=false;
    bool runtimeKnown_=false,lastImmersive_=false,lastHidden_=false,lastBackground_=false,lastSuppressed_=false;
    bool hooksReady_=true;
    std::array<HotkeyBinding,xiaochuang::kHotkeyCount> registered_{};
};
}
