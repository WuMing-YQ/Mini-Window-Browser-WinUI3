#include "WindowModeController.h"
#include "ImmersionPolicy.h"
#include "WindowFrameBounds.h"

#include "CoreLogic.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")

namespace xiaochuang {

namespace {
constexpr DWORD kNormalStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
constexpr DWORD kPopupStyle = WS_POPUP | WS_CLIPCHILDREN;
constexpr auto kDwmBorderColorAttribute = static_cast<DWMWINDOWATTRIBUTE>(34);
constexpr COLORREF kDwmColorNone = 0xFFFFFFFEU;
}

void WindowModeController::Initialize(HWND window, AppSettings* settings, RECT normalRect,
                                      bool maximized, int chromeHeight,
                                      VisibilityCallback chromeVisibility,
                                      LayoutCallback layoutCallback) {
    window_ = window;
    settings_ = settings;
    normalRect_ = EnsureVisible(normalRect);
    chromeHeight_ = chromeHeight;
    chromeVisibility_ = std::move(chromeVisibility);
    layoutCallback_ = std::move(layoutCallback);
    systemTray_ = settings ? settings->useSystemTray : false;
    visibleMode_ = maximized ? WindowMode::Maximized : WindowMode::Normal;
    ApplyVisibleMode(visibleMode_, true);
}

void WindowModeController::Shutdown() {
    if (window_) KillTimer(window_, kImmersionTimerId);
    window_ = nullptr;
    settings_ = nullptr;
    chromeVisibility_ = {};
    layoutCallback_ = {};
    frameHandler_ = {};
}

WindowMode WindowModeController::Mode() const noexcept {
    return hidden_ ? WindowMode::Hidden : visibleMode_;
}

void WindowModeController::ToggleMaximized() {
    if (!window_ || IsImmersive()) return;
    if (visibleMode_ == WindowMode::Maximized) {
        visibleMode_ = WindowMode::Normal;
        ApplyVisibleMode(visibleMode_, true);
        if (ShouldApplyVideoFullscreenFit(visibleMode_, webFullscreen_,
                settings_ && settings_->autoFitVideoFullscreen, webFullscreenAspect_)) {
            FitWebFullscreenAspect(webFullscreenAspect_);
        }
    } else {
        UpdateNormalRectFromWindow();
        visibleMode_ = WindowMode::Maximized;
        ApplyVisibleMode(visibleMode_, true);
    }
}

void WindowModeController::ToggleImmersion() {
    SetImmersion(!IsImmersive());
}

void WindowModeController::SetImmersion(bool enabled) {
    if (!window_) return;
    // Web fullscreen is scoped to the WebView content area. Immersion only changes
    // the host window chrome/style, so keep the DOM fullscreen element alive.
    if (enabled == IsImmersive()) {
        if (enabled) RefreshVisualState();
        return;
    }
    if (enabled) {
        returnFromImmersion_ = visibleMode_;
        GetWindowRect(window_, &preImmersionRect_);
        visibleMode_ = settings_ && settings_->immersionStyle == ImmersionStyle::AutoHide
            ? WindowMode::ImmersionAutoHide : WindowMode::ImmersionHole;
        ApplyVisibleMode(visibleMode_, true);
    } else {
        visibleMode_ = returnFromImmersion_;
        if (visibleMode_ == WindowMode::Normal &&
            preImmersionRect_.right > preImmersionRect_.left) {
            normalRect_ = preImmersionRect_;
        }
        ApplyVisibleMode(visibleMode_, true);
    }
}

void WindowModeController::SetHidden(bool hidden) {
    if (!window_ || hidden_ == hidden) return;
    hidden_ = hidden;
    if (hidden_) {
        KillTimer(window_, kImmersionTimerId);
        ShowWindow(window_, SW_HIDE);
    } else {
        ShowWindow(window_, IsIconic(window_) ? SW_RESTORE : SW_SHOWNOACTIVATE);
        ApplyVisibleMode(visibleMode_, false);
        ReassertTopmost();
    }
}

void WindowModeController::SetSystemTray(bool enabled) {
    systemTray_ = enabled;
    if (!window_ || IsImmersive() || hidden_) return;
    ApplyVisibleMode(visibleMode_, false);
}

void WindowModeController::EnterWebFullscreen() {
    if (!window_ || webFullscreen_) return;
    webFullscreen_ = true;
    webFullscreenAspect_ = 0.0;
}

void WindowModeController::LeaveWebFullscreen() {
    if (!window_ || !webFullscreen_) return;
    webFullscreen_ = false;
    webFullscreenAspect_ = 0.0;
    if (!videoFitActive_) return;

    videoFitActive_ = false;
    normalRect_ = EnsureVisible(preVideoFitRect_, false);
    if (visibleMode_ == WindowMode::Normal && !hidden_) {
        ApplyVisibleMode(visibleMode_, true);
    }
}

void WindowModeController::FitWebFullscreenAspect(double aspectRatio) {
    if (!window_ || !webFullscreen_ || !std::isfinite(aspectRatio) ||
        aspectRatio <= 0.1 || aspectRatio >= 10.0) return;
    webFullscreenAspect_ = aspectRatio;
    if (hidden_ || !ShouldApplyVideoFullscreenFit(visibleMode_, webFullscreen_,
            settings_ && settings_->autoFitVideoFullscreen, aspectRatio)) return;

    RECT current{};
    if (!GetWindowRect(window_, &current)) return;
    if (!videoFitActive_) {
        preVideoFitRect_ = current;
        videoFitActive_ = true;
    }

    const RECT monitor = CurrentMonitorRect(false);
    const RECT work = CurrentMonitorRect(true);
    const SIZE target = CalculateAspectFitWindowSize(
        current.right - current.left, chromeHeight_, aspectRatio,
        monitor.right - monitor.left, monitor.bottom - monitor.top);
    const RECT desired = PositionAspectFitRect(
        current, target, monitor, work, settings_ ? settings_->snapThreshold : 0);
    Position(window_, HWND_TOPMOST, desired.left, desired.top,
                 desired.right - desired.left, desired.bottom - desired.top,
                 SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    if (layoutCallback_) layoutCallback_();
}

bool WindowModeController::ConstrainWebFullscreenSizing(UINT sizingEdge, RECT* sizingRect) const {
    if (!sizingRect || !settings_ || !webFullscreen_ ||
        !settings_->autoFitVideoFullscreen || !settings_->lockVideoFullscreenAspect ||
        !std::isfinite(webFullscreenAspect_) || webFullscreenAspect_ <= 0.1) {
        return false;
    }
    const int visibleChromeHeight = IsImmersive() ? 0 : chromeHeight_;
    *sizingRect = ConstrainAspectSizingRect(*sizingRect, sizingEdge, visibleChromeHeight,
                                            webFullscreenAspect_);
    return true;
}

bool WindowModeController::RestoreMaximizedForDrag(POINT cursor) {
    if (!window_ || hidden_ || visibleMode_ != WindowMode::Maximized) return false;

    RECT maximizedRect{};
    if (!GetWindowRect(window_, &maximizedRect)) return false;
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    if (!GetMonitorInfoW(monitor, &monitorInfo)) return false;

    normalRect_ = CalculateRestoredDragRect(maximizedRect, normalRect_, cursor,
                                            monitorInfo.rcWork, chromeHeight_);
    visibleMode_ = WindowMode::Normal;
    ApplyVisibleMode(visibleMode_, true);
    return true;
}

void WindowModeController::ApplyPresetGeometry(const RECT& normalRect, bool maximized) {
    if (!window_) return;
    if (webFullscreen_) LeaveWebFullscreen();
    if (IsImmersive()) SetImmersion(false);
    normalRect_ = EnsureVisible(normalRect);
    visibleMode_ = maximized ? WindowMode::Maximized : WindowMode::Normal;
    ApplyVisibleMode(visibleMode_, true);
}

void WindowModeController::UpdateNormalRectFromWindow() {
    if (!window_ || applyingMode_ || visibleMode_ != WindowMode::Normal || hidden_ || IsIconic(window_)) return;
    RECT current{};
    if (GetWindowRect(window_, &current)) {
        normalRect_ = current;
        if (videoFitActive_) {
            // Once the user moves or snaps the auto-fitted video window, that new
            // position becomes authoritative and must survive leaving fullscreen.
            preVideoFitRect_ = current;
            videoFitActive_ = false;
        }
    }
}

void WindowModeController::OnImmersionTimer() {
    if (!window_ || !settings_ || !IsImmersive() || hidden_) return;
    POINT cursor{};
    if (!GetCursorPos(&cursor)) return;
    const bool excluded=cursorExcluded_&&cursorExcluded_(cursor);
    if (visibleMode_ == WindowMode::ImmersionHole) {
        RECT physicalRect{};GetWindowRect(window_,&physicalRect);
        constexpr int kCursorQuantum = 4;
        cursor.x = (cursor.x / kCursorQuantum) * kCursorQuantum;
        cursor.y = (cursor.y / kCursorQuantum) * kCursorQuantum;
        if (cursor.x == lastCursor_.x && cursor.y == lastCursor_.y&&EqualRect(&physicalRect,&lastHoleRect_)&&lastHoleRadius_==settings_->holeRadius&&lastExcluded_==excluded) return;
        lastHoleRect_=physicalRect;lastHoleRadius_=settings_->holeRadius;lastExcluded_=excluded;
        lastCursor_ = cursor;
        POINT client = cursor;
        ScreenToClient(window_, &client);
        RECT bounds{};
        GetClientRect(window_, &bounds);
        const int radius = std::max(0, settings_->holeRadius);
        const bool outside = excluded || radius == 0 || client.x + radius <= bounds.left ||
            client.x - radius >= bounds.right || client.y + radius <= bounds.top ||
            client.y - radius >= bounds.bottom;
        if (outside) {
            if (holeOutsideWindow_) return;
            holeOutsideWindow_ = true;
            Region(window_, nullptr, FALSE);
            RedrawWindow(window_, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
            return;
        }
        holeOutsideWindow_ = false;
        HRGN full = CreateRectRgn(0, 0, bounds.right, bounds.bottom);
        HRGN result = nullptr;
        if (radius > 0) {
            HRGN hole = CreateEllipticRgn(client.x - radius, client.y - radius,
                                          client.x + radius, client.y + radius);
            if (full && hole) {
                result = CreateRectRgn(0, 0, 0, 0);
                if (result) CombineRgn(result, full, hole, RGN_DIFF);
            }
            if (hole) DeleteObject(hole);
        }
        if (!result) result = full;
        else if (full) DeleteObject(full);
        if (!result) return;
        if (!Region(window_, result, FALSE)) {
            DeleteObject(result);
        } else {
            RedrawWindow(window_, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
        }
        return;
    }

    RECT windowRect{};
    GetWindowRect(window_, &windowRect);
    const bool candidate = !excluded&&CursorWithin(windowRect,cursor);
    const ULONGLONG now = GetTickCount64();
    if (candidate != hoverCandidate_) {
        hoverCandidate_ = candidate;
        hoverCandidateSince_ = now;
    }
    if (hoverApplied_ != hoverCandidate_ && now - hoverCandidateSince_ >= 80) {
        hoverApplied_ = hoverCandidate_;
        Alpha(window_, 0, ConfiguredAlpha(hoverApplied_), LWA_ALPHA);
    }
}

void WindowModeController::ReassertTopmost() const {
    if (!window_ || hidden_) return;
    const HWND activePopup = GetLastActivePopup(window_);
    if (activePopup && activePopup != window_ && IsWindowVisible(activePopup)) return;
    const HWND foreground = GetForegroundWindow();
    if (foreground && foreground != window_ &&
        GetAncestor(foreground, GA_ROOTOWNER) == window_) return;
    Position(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
}

void WindowModeController::SnapMovingRect(RECT* movingRect, bool controlHeld) const {
    if (!window_ || !settings_ || !movingRect || controlHeld || settings_->snapThreshold <= 0 ||
        visibleMode_ != WindowMode::Normal) return;
    const auto frame = ReadWindowFrameInsets(window_);
    const RECT visible = frame.Visible(*movingRect);
    HMONITOR monitor = MonitorFromRect(&visible, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    if (!GetMonitorInfoW(monitor, &info)) return;
    const RECT screen = info.rcWork;
    const int threshold = settings_->snapThreshold;
    LONG dx=0,dy=0;
    if (std::abs(visible.left - screen.left) <= threshold) dx=screen.left-visible.left;
    else if (std::abs(visible.right - screen.right) <= threshold) dx=screen.right-visible.right;
    if (std::abs(visible.top - screen.top) <= threshold) dy=screen.top-visible.top;
    else if (std::abs(visible.bottom - screen.bottom) <= threshold) dy=screen.bottom-visible.bottom;
    // Move only: retain the outer dimensions, with the invisible border offscreen.
    OffsetRect(movingRect,dx,dy);
}

void WindowModeController::RefreshVisualState() {
    if (!window_) return;
    if (!IsImmersive()) return;
    const WindowMode desired = settings_ && settings_->immersionStyle == ImmersionStyle::AutoHide
        ? WindowMode::ImmersionAutoHide : WindowMode::ImmersionHole;
    if (desired != visibleMode_) {
        visibleMode_ = desired;
        ClearRegionAndOpacity();
        ApplyVisibleMode(visibleMode_, false);
        return;
    }
    if (visibleMode_ == WindowMode::ImmersionHole) {
        lastCursor_ = {LONG_MIN, LONG_MIN};
        holeOutsideWindow_ = false;
        Alpha(window_, 0, ConfiguredAlpha(), LWA_ALPHA);
        OnImmersionTimer();
    } else {
        Alpha(window_, 0, ConfiguredAlpha(hoverApplied_), LWA_ALPHA);
    }
    ReassertTopmost();
}

void WindowModeController::RefreshFrameAppearance() const {
    if (!window_) return;
    const bool normalFrame = !IsImmersive();
    const DWMNCRENDERINGPOLICY nonClientPolicy = normalFrame
        ? DWMNCRP_ENABLED : DWMNCRP_DISABLED;
    DwmSetWindowAttribute(window_, DWMWA_NCRENDERING_POLICY,
                          &nonClientPolicy, sizeof(nonClientPolicy));
    const DWM_WINDOW_CORNER_PREFERENCE corners = visibleMode_ == WindowMode::Normal
        ? DWMWCP_ROUND : DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(window_, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    DwmSetWindowAttribute(window_, kDwmBorderColorAttribute,
                          &kDwmColorNone, sizeof(kDwmColorNone));
}

void WindowModeController::ApplyVisibleMode(WindowMode mode, bool restoreGeometry) {
    if (!window_ || !settings_) return;
    // Presenter/style changes synchronously emit WM_SIZE/WM_MOVE. Those are not
    // user resizes and must never overwrite the saved normal-window rectangle.
    struct ApplyingGuard {
        bool& flag;
        bool previous;
        ~ApplyingGuard() { flag = previous; }
    } guard{applyingMode_, applyingMode_};
    applyingMode_ = true;
    const bool immersive = mode == WindowMode::ImmersionHole || mode == WindowMode::ImmersionAutoHide;
    RECT target{};
    if (!GetWindowRect(window_, &target)) return;
    if (restoreGeometry) {
        // Preserve the OUTER window bounds: removing chrome gives the webpage
        // more room, and restoring chrome consumes that room without resizing.
        target = immersive ? preImmersionRect_ :
            (mode == WindowMode::Maximized ? CurrentMonitorRect(true) : normalRect_);
    }
    KillTimer(window_, kImmersionTimerId);
    if (frameHandler_) frameHandler_(mode);
    const DWORD baseExStyle = WS_EX_TOPMOST | (systemTray_ ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW);
    const DWORD style = immersive ? kPopupStyle :
        kNormalStyle | (mode == WindowMode::Maximized ? WS_MAXIMIZE : 0U);
    const DWORD exStyle = immersive
        ? baseExStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE
        : baseExStyle;
    if (!immersive) ClearRegionAndOpacity();
    ApplyStyle(style, exStyle);
    RefreshFrameAppearance();
    Position(window_, HWND_TOPMOST, target.left, target.top,
             target.right - target.left, target.bottom - target.top,
             SWP_NOACTIVATE | SWP_FRAMECHANGED | (hidden_ ? 0U : SWP_SHOWWINDOW));
    if (chromeVisibility_) chromeVisibility_(!immersive);
    if (immersive) {
        lastCursor_ = {LONG_MIN, LONG_MIN};
        hoverApplied_ = hoverCandidate_ = false;
        hoverCandidateSince_ = GetTickCount64();
        Alpha(window_, 0, ConfiguredAlpha(), LWA_ALPHA);
    } else {
        ClearRegionAndOpacity();
    }
    if (layoutCallback_) layoutCallback_();
    if (immersive && !hidden_) {
        OnImmersionTimer();
        SetTimer(window_, kImmersionTimerId, 50, nullptr);
    }
    RedrawWindow(window_, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
    ReassertTopmost();
}

void WindowModeController::ApplyStyle(DWORD style, DWORD exStyle) const {
    if ((GetWindowLongPtrW(window_, GWL_STYLE) & WS_VISIBLE) != 0) {
        style |= WS_VISIBLE;
    }
    Check(operations_.style(window_, GWL_STYLE, static_cast<LONG_PTR>(style)),L"设置窗口样式");
    Check(operations_.style(window_, GWL_EXSTYLE, static_cast<LONG_PTR>(exStyle)),L"设置扩展样式");
}

RECT WindowModeController::CurrentMonitorRect(bool workArea) const {
    HMONITOR monitor = MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    return workArea ? info.rcWork : info.rcMonitor;
}

BYTE WindowModeController::ConfiguredAlpha(bool cursorInside) const {
    if(!settings_)return 255;
    const auto basis=visibleMode_==WindowMode::ImmersionHole?settings_->holeOpacityPercent:settings_->autoHideOpacityPercent;
    return ToWindowAlpha(CalculateImmersionOpacity(visibleMode_,basis,settings_->hoverReductionPercent,cursorInside));
}

void WindowModeController::ClearRegionAndOpacity() const {
    if (!window_) return;
    Region(window_, nullptr, TRUE);
    if ((GetWindowLongPtrW(window_,GWL_EXSTYLE)&WS_EX_LAYERED)!=0) Alpha(window_, 0, 255, LWA_ALPHA);
}

RECT WindowModeController::EnsureVisible(RECT rect, bool useWorkArea) const {
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    if (!GetMonitorInfoW(monitor, &info)) return rect;
    // Clamp visible edges, allowing only the invisible border outside the area.
    const RECT work = ReadWindowFrameInsets(window_).Outer(useWorkArea ? info.rcWork : info.rcMonitor);
    int width = std::clamp(static_cast<int>(rect.right - rect.left), 160,
                           static_cast<int>(work.right - work.left));
    int height = std::clamp(static_cast<int>(rect.bottom - rect.top), 120,
                            static_cast<int>(work.bottom - work.top));
    const int left = std::clamp(static_cast<int>(rect.left), static_cast<int>(work.left),
                                static_cast<int>(work.right) - width);
    const int top = std::clamp(static_cast<int>(rect.top), static_cast<int>(work.top),
                               static_cast<int>(work.bottom) - height);
    return RECT{left, top, left + width, top + height};
}

bool WindowModeController::Check(bool success,const wchar_t* operation) const {
    if(!success){if(!operationError_.empty())operationError_+=L"；";operationError_+=std::wstring(operation)+L"失败（"+std::to_wstring(GetLastError())+L"）";}return success;
}
bool WindowModeController::Position(HWND w,HWND after,int x,int y,int width,int height,UINT flags) const {return Check(operations_.position(w,after,x,y,width,height,flags),L"调整窗口");}
bool WindowModeController::Region(HWND w,HRGN r,BOOL redraw) const {return Check(operations_.region(w,r,redraw),L"更新窗口区域");}
bool WindowModeController::Alpha(HWND w,COLORREF color,BYTE value,DWORD flags) const {return Check(operations_.alpha(w,color,value,flags),L"调整不透明度");}
} // namespace xiaochuang
