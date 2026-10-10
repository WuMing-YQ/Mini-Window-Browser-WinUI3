#pragma once

#include "AppModel.h"

#include <cstdint>
#include <string>
#include <vector>

namespace xiaochuang {

struct MouseButtonTransition {
    UINT virtualKey = 0;
    bool down = false;
};

std::vector<MouseButtonTransition> DecodeRawMouseButtons(std::uint16_t buttonFlags);
bool ShouldRecoverStaleKeyDown(bool knownDown, bool matchingGesturePending,
                               bool asynchronousKeyDown);
bool ShouldForceReleaseMessageGesture(std::uint64_t elapsedMilliseconds,
                                      bool asynchronousKeyDown);
std::wstring TrimWhitespace(std::wstring value);
bool HasPresetNameConflict(const std::vector<std::wstring>& names,
                           const std::wstring& candidate, size_t ignoredIndex = SIZE_MAX);

RECT CalculateRestoredDragRect(const RECT& maximizedRect, const RECT& normalRect,
                               POINT cursor, const RECT& workArea, int chromeHeight);
SIZE CalculateAspectFitWindowSize(int preferredWidth, int chromeHeight, double aspectRatio,
                                  int maximumWidth, int maximumHeight);
bool ShouldApplyVideoFullscreenFit(WindowMode visibleMode, bool webFullscreen,
                                   bool autoFitEnabled, double aspectRatio);
RECT PositionAspectFitRect(const RECT& currentRect, SIZE targetSize,
                           const RECT& monitorRect, const RECT& workArea,
                           int edgeThreshold);
RECT ConstrainAspectSizingRect(RECT proposedRect, UINT sizingEdge, int chromeHeight,
                               double aspectRatio);

} // namespace xiaochuang
