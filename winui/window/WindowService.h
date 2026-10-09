#pragma once
#include "../core/Ports.h"
#include "../../src/WindowModeController.h"
#include "ThemeState.h"
namespace miniwindow {
class WindowService final:public IWindowPort {
public:
    WindowService(HWND,AppSettings&,std::function<void(bool)>,std::function<void()>);
    ~WindowService();
    void Initialize(RECT,bool);
    OperationResult SetMode(WindowMode) override;
    OperationResult SetHidden(bool) override;
    OperationResult RestoreNormal() override;
    RECT NormalRect() const override {return controller_.NormalRect();}
    WindowMode Mode() const {return controller_.Mode();}
    WindowMode VisibleMode() const {return Immersive()?(settings_.immersionStyle==ImmersionStyle::Hole?WindowMode::ImmersionHole:WindowMode::ImmersionAutoHide):(controller_.IsSeamlessMaximized()?WindowMode::Maximized:WindowMode::Normal);}
    bool Hidden() const {return controller_.IsHidden();}
    bool Immersive() const {return controller_.IsImmersive();}
    bool RecoveryAvailable() const {return recoveryForced_||Immersive()||Hidden()||settings_.useSystemTray;}
    void SetOperations(xiaochuang::WindowOperations operations){controller_.SetOperations(std::move(operations));}
    void SetFrameHandler(std::function<void(WindowMode)> handler){frame_=std::move(handler);}
    void SetThemeHandler(std::function<void(xiaochuang::ThemeMode)> handler){themeHandler_=std::move(handler);}
    void SetRecoveryHandler(std::function<bool(bool,bool,bool)> handler){recovery_=std::move(handler);}
    void SetErrorHandler(std::function<void(std::wstring)> handler){error_=std::move(handler);}
    void HandleMessage(UINT,WPARAM,LPARAM);
    void RefreshTheme(xiaochuang::ThemeMode);
    xiaochuang::ThemeMode Theme() const {return theme_;}
    void SetChromeHeight(int pixels){controller_.SetChromeHeight(pixels);}
    void OnWebFullscreen(bool,double aspect=0);
    OperationResult ApplyGeometry(RECT,bool) override;
    OperationResult RefreshVisuals();
    OperationResult RestoreForDrag(POINT);
    void ReassertTopmost(){controller_.ReassertTopmost();}
private:
    OperationResult Change(WindowMode,std::function<void()>);
    bool SyncRecovery(bool immersion,bool hidden);
    HWND hwnd_;AppSettings& settings_;xiaochuang::WindowModeController controller_;
    std::function<void(WindowMode)> frame_;std::function<void(xiaochuang::ThemeMode)> themeHandler_;
    std::function<bool(bool,bool,bool)> recovery_;std::function<void(std::wstring)> error_;
    xiaochuang::ThemeMode theme_=xiaochuang::ThemeMode::System;
    bool recoveryForced_=false;
};
}
