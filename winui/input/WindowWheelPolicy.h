#pragma once
#include "../core/Ports.h"
namespace miniwindow {
struct WheelContext {
 WindowMode mode=WindowMode::Normal;
 bool hidden=false,maximized=false,mainOrWebFocused=false,managementFocused=false;
 UINT modifiers=0;
};
bool ShouldResizeWindow(const WheelContext&,UINT configuredModifiers);
class WindowWheelAccumulator {
public: int Add(int delta);void Reset(){remainder_=0;}
private: int remainder_=0;
};
RECT ResizeWindowBySteps(RECT current,int steps,RECT workArea,SIZE minimum,int snapDistance=0);
std::wstring WheelModifierLabel(UINT modifiers);
}
