#include "CoreLogic.h"

#include <algorithm>
#include <cmath>
#include <cwctype>

namespace xiaochuang {

std::vector<MouseButtonTransition> DecodeRawMouseButtons(std::uint16_t buttonFlags) {
    std::vector<MouseButtonTransition> transitions;
    if ((buttonFlags & RI_MOUSE_BUTTON_3_DOWN) != 0) transitions.push_back({VK_MBUTTON, true});
    if ((buttonFlags & RI_MOUSE_BUTTON_3_UP) != 0) transitions.push_back({VK_MBUTTON, false});
    if ((buttonFlags & RI_MOUSE_BUTTON_4_DOWN) != 0) transitions.push_back({VK_XBUTTON1, true});
    if ((buttonFlags & RI_MOUSE_BUTTON_4_UP) != 0) transitions.push_back({VK_XBUTTON1, false});
    if ((buttonFlags & RI_MOUSE_BUTTON_5_DOWN) != 0) transitions.push_back({VK_XBUTTON2, true});
    if ((buttonFlags & RI_MOUSE_BUTTON_5_UP) != 0) transitions.push_back({VK_XBUTTON2, false});
    return transitions;
}

bool ShouldRecoverStaleKeyDown(bool knownDown, bool matchingGesturePending,
                               bool asynchronousKeyDown) {
    return knownDown && !matchingGesturePending && !asynchronousKeyDown;
}

bool ShouldForceReleaseMessageGesture(std::uint64_t elapsedMilliseconds,
                                      bool asynchronousKeyDown) {
    return elapsedMilliseconds >= 5000 && !asynchronousKeyDown;
}

std::wstring TrimWhitespace(std::wstring value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
        [](wchar_t character) { return std::iswspace(character) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
        [](wchar_t character) { return std::iswspace(character) != 0; }).base();
    if (first >= last) return {};
    return std::wstring(first, last);
}

bool HasPresetNameConflict(const std::vector<std::wstring>& names,
                           const std::wstring& candidate, size_t ignoredIndex) {
    std::wstring normalized = TrimWhitespace(candidate);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    if (normalized.empty()) return true;
    for (size_t index = 0; index < names.size(); ++index) {
        if (index == ignoredIndex) continue;
        std::wstring existing = TrimWhitespace(names[index]);
        std::transform(existing.begin(), existing.end(), existing.begin(),
            [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
        if (existing == normalized) return true;
    }
    return false;
}

RECT CalculateRestoredDragRect(const RECT& maximizedRect, const RECT& normalRect,
                               POINT cursor, const RECT& workArea, int chromeHeight) {
    const int workWidth = std::max(1, static_cast<int>(workArea.right - workArea.left));
    const int workHeight = std::max(1, static_cast<int>(workArea.bottom - workArea.top));
    const int width = std::clamp(static_cast<int>(normalRect.right - normalRect.left), 1, workWidth);
    const int height = std::clamp(static_cast<int>(normalRect.bottom - normalRect.top), 1, workHeight);
    const int maximizedWidth = std::max(1, static_cast<int>(maximizedRect.right - maximizedRect.left));
    const double horizontalRatio = std::clamp(
        static_cast<double>(cursor.x - maximizedRect.left) / maximizedWidth, 0.0, 1.0);
    int left = cursor.x - static_cast<int>(std::lround(width * horizontalRatio));
    const int captionOffset = std::clamp(static_cast<int>(cursor.y - maximizedRect.top), 0,
        std::max(0, std::min(chromeHeight - 1, height - 1)));
    int top = cursor.y - captionOffset;
    left = std::clamp(left, static_cast<int>(workArea.left),
                      static_cast<int>(workArea.right) - width);
    top = std::clamp(top, static_cast<int>(workArea.top),
                     static_cast<int>(workArea.bottom) - height);
    return RECT{left, top, left + width, top + height};
}

SIZE CalculateAspectFitWindowSize(int preferredWidth, int chromeHeight, double aspectRatio,
                                  int maximumWidth, int maximumHeight) {
    maximumWidth = std::max(1, maximumWidth);
    maximumHeight = std::max(1, maximumHeight);
    if (!std::isfinite(aspectRatio) || aspectRatio <= 0.05 || aspectRatio >= 20.0) {
        return SIZE{std::clamp(preferredWidth, 1, maximumWidth), maximumHeight};
    }
    int width = std::clamp(preferredWidth, 1, maximumWidth);
    int height = chromeHeight + static_cast<int>(std::lround(width / aspectRatio));
    if (height > maximumHeight) {
        height = maximumHeight;
        width = std::min(maximumWidth, std::max(1,
            static_cast<int>(std::lround(std::max(1, height - chromeHeight) * aspectRatio))));
    }
    return SIZE{width, std::clamp(height, 1, maximumHeight)};
}

bool ShouldApplyVideoFullscreenFit(WindowMode visibleMode, bool webFullscreen,
                                   bool autoFitEnabled, double aspectRatio) {
    return visibleMode != WindowMode::Hidden && webFullscreen && autoFitEnabled &&
        std::isfinite(aspectRatio) && aspectRatio > 0.1 && aspectRatio < 10.0;
}

RECT PositionAspectFitRect(const RECT& currentRect, SIZE targetSize,
                           const RECT& monitorRect, const RECT& workArea,
                           int edgeThreshold) {
    const int monitorWidth = std::max(1, static_cast<int>(monitorRect.right - monitorRect.left));
    const int monitorHeight = std::max(1, static_cast<int>(monitorRect.bottom - monitorRect.top));
    const int width = std::clamp(static_cast<int>(targetSize.cx), 1, monitorWidth);
    const int height = std::clamp(static_cast<int>(targetSize.cy), 1, monitorHeight);
    edgeThreshold = std::max(0, edgeThreshold);

    const auto edgeDistance = [](int value, int monitorEdge, int workEdge) {
        return std::min(std::abs(value - monitorEdge), std::abs(value - workEdge));
    };
    const int leftDistance = edgeDistance(currentRect.left, monitorRect.left, workArea.left);
    const int rightDistance = edgeDistance(currentRect.right, monitorRect.right, workArea.right);
    const int topDistance = edgeDistance(currentRect.top, monitorRect.top, workArea.top);
    const int bottomDistance = edgeDistance(currentRect.bottom, monitorRect.bottom, workArea.bottom);

    int left = currentRect.left +
        ((currentRect.right - currentRect.left) - width) / 2;
    if (std::min(leftDistance, rightDistance) <= edgeThreshold) {
        left = leftDistance <= rightDistance ? monitorRect.left : monitorRect.right - width;
    }
    int top = currentRect.top +
        ((currentRect.bottom - currentRect.top) - height) / 2;
    if (std::min(topDistance, bottomDistance) <= edgeThreshold) {
        top = topDistance <= bottomDistance ? monitorRect.top : monitorRect.bottom - height;
    }

    left = std::clamp(left, static_cast<int>(monitorRect.left),
                      static_cast<int>(monitorRect.right) - width);
    top = std::clamp(top, static_cast<int>(monitorRect.top),
                     static_cast<int>(monitorRect.bottom) - height);
    return RECT{left, top, left + width, top + height};
}

RECT ConstrainAspectSizingRect(RECT proposedRect, UINT sizingEdge, int chromeHeight,
                               double aspectRatio) {
    if (!std::isfinite(aspectRatio) || aspectRatio <= 0.05 || aspectRatio >= 20.0) {
        return proposedRect;
    }

    chromeHeight = std::max(0, chromeHeight);
    int width = std::max(1, static_cast<int>(proposedRect.right - proposedRect.left));
    int height = std::max(chromeHeight + 1,
                          static_cast<int>(proposedRect.bottom - proposedRect.top));
    const bool heightDriven = sizingEdge == WMSZ_TOP || sizingEdge == WMSZ_BOTTOM;
    if (heightDriven) {
        width = std::max(1, static_cast<int>(
            std::lround(std::max(1, height - chromeHeight) * aspectRatio)));
        proposedRect.right = proposedRect.left + width;
    } else {
        height = chromeHeight + std::max(1, static_cast<int>(std::lround(width / aspectRatio)));
        const bool topEdge = sizingEdge == WMSZ_TOP || sizingEdge == WMSZ_TOPLEFT ||
            sizingEdge == WMSZ_TOPRIGHT;
        if (topEdge) proposedRect.top = proposedRect.bottom - height;
        else proposedRect.bottom = proposedRect.top + height;
    }
    return proposedRect;
}

} // namespace xiaochuang
