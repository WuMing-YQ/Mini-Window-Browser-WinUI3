#pragma once
#include "../../src/AppModel.h"
#include "../../src/HotkeyManager.h"
#include "../../src/MediaBridge.h"
#include <functional>
#include <cstdint>
namespace miniwindow {
using xiaochuang::AppState;
using xiaochuang::AppSettings;
using xiaochuang::WindowMode;
using xiaochuang::ImmersionStyle;
using xiaochuang::Bookmark;
using xiaochuang::Preset;
using xiaochuang::HotkeyBinding;
using xiaochuang::HotkeyAction;
using xiaochuang::HotkeyGesture;
using xiaochuang::MediaCommand;
struct OperationResult { bool ok=true; std::wstring message; };
struct AppSnapshot {
    uint64_t revision=1;
    AppState state;
    std::vector<uint64_t> bookmarkIds;
    bool browserReady=false, loading=false, pendingRestart=false;
    std::wstring error,captureStatus;
    std::vector<std::wstring> hotkeyErrors;
};
struct IBrowserPort {
    virtual ~IBrowserPort()=default;
    virtual OperationResult Navigate(const std::wstring&)=0;
    virtual void SetVisible(bool)=0;
    virtual void ExecuteMedia(MediaCommand,int)=0;
    virtual void Close()=0;
    virtual std::wstring RuntimeVersion()const{return {};}
    virtual void RequestDiagnostics(std::function<void(std::wstring)> done){done(L"浏览器尚未就绪。");}
};
struct IWindowPort {
    virtual ~IWindowPort()=default;
    virtual OperationResult SetMode(WindowMode)=0;
    virtual OperationResult SetHidden(bool)=0;
    virtual OperationResult RestoreNormal()=0;
    virtual WindowMode ImmersionReturnMode() const {return WindowMode::Normal;}
    virtual RECT NormalRect() const=0;
    virtual OperationResult ApplyGeometry(RECT,bool){return {false,L"几何操作不可用。"};}
};
struct ISettingsPort {
    virtual ~ISettingsPort()=default;
    virtual OperationResult Save(const AppState&)=0;
};
}
