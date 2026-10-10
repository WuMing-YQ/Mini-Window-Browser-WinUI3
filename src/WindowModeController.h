#pragma once

#include "AppModel.h"

#include <functional>

namespace xiaochuang {

constexpr UINT_PTR kImmersionTimerId = 4402;

// Host adapters can observe and inject failures without changing legacy callers.
struct WindowOperations {
    std::function<bool(HWND,int,LONG_PTR)> style=[](HWND w,int index,LONG_PTR value){SetLastError(ERROR_SUCCESS);const auto previous=SetWindowLongPtrW(w,index,value);return previous!=0||GetLastError()==ERROR_SUCCESS;};
    std::function<bool(HWND,HWND,int,int,int,int,UINT)> position=[](HWND w,HWND after,int x,int y,int width,int height,UINT flags){return SetWindowPos(w,after,x,y,width,height,flags)!=FALSE;};
    std::function<bool(HWND,HRGN,BOOL)> region=[](HWND w,HRGN r,BOOL redraw){return SetWindowRgn(w,r,redraw)!=0;};
    std::function<bool(HWND,COLORREF,BYTE,DWORD)> alpha=[](HWND w,COLORREF color,BYTE value,DWORD flags){return SetLayeredWindowAttributes(w,color,value,flags)!=FALSE;};
};
class WindowModeController {
public:
    using VisibilityCallback = std::function<void(bool)>;
    using LayoutCallback = std::function<void()>;

    void Initialize(HWND window, AppSettings* settings, RECT normalRect, bool maximized,
                    int chromeHeight, VisibilityCallback chromeVisibility,
                    LayoutCallback layoutCallback);
    void Shutdown();
    void SetCursorExclusion(std::function<bool(POINT)> excluded){cursorExcluded_=std::move(excluded);lastCursor_={LONG_MIN,LONG_MIN};}
    void SetOperations(WindowOperations operations) {operations_=std::move(operations);}
    void SetFrameHandler(std::function<void(WindowMode)> handler) { frameHandler_=std::move(handler); }
    void SetContentRectProvider(std::function<RECT()> provider) { contentRectProvider_=std::move(provider); }
    bool ApplyingMode() const noexcept { return applyingMode_; }
    WindowMode ImmersionReturnMode() const noexcept { return returnFromImmersion_; }
    void ClearOperationError() const {operationError_.clear();}
    const std::wstring& OperationError() const noexcept {return operationError_;}
    void RestoreGeometry(RECT rect) {Position(window_,HWND_TOPMOST,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,SWP_NOACTIVATE|SWP_FRAMECHANGED);}
    void Reapply() {ApplyVisibleMode(visibleMode_,true);if(hidden_)ShowWindow(window_,SW_HIDE);}


    WindowMode Mode() const noexcept;
    bool IsSeamlessMaximized() const noexcept { return visibleMode_ == WindowMode::Maximized; }
    bool IsImmersive() const noexcept {
        return visibleMode_ == WindowMode::ImmersionHole ||
            visibleMode_ == WindowMode::ImmersionAutoHide;
    }
    bool IsHidden() const noexcept { return hidden_; }
    bool IsWebFullscreen() const noexcept { return webFullscreen_; }
    const RECT& NormalRect() const noexcept { return normalRect_; }

    void ToggleMaximized();
    void ToggleImmersion();
    void SetImmersion(bool enabled);
    void SetHidden(bool hidden);
    void SetSystemTray(bool enabled);
    void SetChromeHeight(int chromeHeight) noexcept { chromeHeight_ = chromeHeight; }
    void EnterWebFullscreen();
    void LeaveWebFullscreen();
    void FitWebFullscreenAspect(double aspectRatio);
    bool ConstrainWebFullscreenSizing(UINT sizingEdge, RECT* sizingRect) const;
    bool RestoreMaximizedForDrag(POINT cursor);
    void ApplyPresetGeometry(const RECT& normalRect, bool maximized);
    void UpdateNormalRectFromWindow();
    void OnImmersionTimer();
    void ReassertTopmost() const;
    void SnapMovingRect(RECT* movingRect, bool controlHeld) const;
    void RefreshVisualState();
    void RefreshFrameAppearance() const;

private:
    void ApplyVisibleMode(WindowMode mode, bool restoreGeometry);
    void ApplyStyle(DWORD style, DWORD exStyle) const;
    RECT CurrentMonitorRect(bool workArea) const;
    RECT ContentInsets() const;
    BYTE ConfiguredAlpha(bool cursorInside=false) const;
    void ClearRegionAndOpacity() const;
    RECT EnsureVisible(RECT rect, bool useWorkArea = false) const;

    bool Check(bool success,const wchar_t* operation) const;
    bool Position(HWND,HWND,int,int,int,int,UINT) const;
    bool Region(HWND,HRGN,BOOL) const;
    bool Alpha(HWND,COLORREF,BYTE,DWORD) const;
    WindowOperations operations_;
    mutable std::wstring operationError_;
    HWND window_ = nullptr;
    AppSettings* settings_ = nullptr;
    VisibilityCallback chromeVisibility_;
    LayoutCallback layoutCallback_;
    std::function<void(WindowMode)> frameHandler_;
    bool applyingMode_ = false;
    RECT normalRect_{100, 100, 1100, 700};
    RECT preImmersionRect_{};
    RECT immersionRect_{};
    std::function<RECT()> contentRectProvider_;
    RECT preVideoFitRect_{};
    WindowMode visibleMode_ = WindowMode::Normal;
    WindowMode returnFromImmersion_ = WindowMode::Normal;
    int chromeHeight_ = 82;
    bool hidden_ = false;
    bool systemTray_ = false;
    bool webFullscreen_ = false;
    bool videoFitActive_ = false;
    double webFullscreenAspect_ = 0.0;
    POINT lastCursor_{LONG_MIN, LONG_MIN};
    bool holeOutsideWindow_ = false;
    RECT lastHoleRect_{};int lastHoleRadius_=-1;bool lastExcluded_=false;
    std::function<bool(POINT)> cursorExcluded_;
    bool hoverCandidate_ = false;
    bool hoverApplied_ = false;
    ULONGLONG hoverCandidateSince_ = 0;
};

} // namespace xiaochuang
