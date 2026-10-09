#include "WindowWheelPolicy.h"
#include <algorithm>
#include <cmath>
namespace miniwindow {
bool ShouldResizeWindow(const WheelContext& context,UINT configured){
 constexpr UINT mask=MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_WIN;
 return configured!=0&&(configured&~mask)==0&&context.modifiers==configured&&context.mode==WindowMode::Normal&&!context.hidden&&!context.maximized&&context.mainOrWebFocused&&!context.managementFocused;
}
int WindowWheelAccumulator::Add(int delta){const auto total=static_cast<long long>(remainder_)+delta;const auto steps=total/WHEEL_DELTA;remainder_=static_cast<int>(total%WHEEL_DELTA);return static_cast<int>(steps);}
RECT ResizeWindowBySteps(RECT current,int steps,RECT area,SIZE minimum){
 const LONG areaWidth=area.right-area.left,areaHeight=area.bottom-area.top;
 if(areaWidth<=0||areaHeight<=0)return current;
 const double width=std::max(1L,current.right-current.left),height=std::max(1L,current.bottom-current.top);
 const LONG minWidth=std::clamp(minimum.cx,1L,areaWidth),minHeight=std::clamp(minimum.cy,1L,areaHeight);
 const LONG left=std::clamp(current.left,area.left,area.right-minWidth),top=std::clamp(current.top,area.top,area.bottom-minHeight);
 const double lower=std::max(minWidth/width,minHeight/height);
 const double upper=std::min((area.right-left)/width,(area.bottom-top)/height);
 // If minimum dimensions and the current aspect cannot both fit, visible bounds win.
 const double scale=std::min(upper,std::max(lower,std::pow(1.05,std::clamp(steps,-100,100))));
 const LONG resizedWidth=std::clamp(static_cast<LONG>(std::lround(width*scale)),minWidth,area.right-left);
 const LONG resizedHeight=std::clamp(static_cast<LONG>(std::lround(height*scale)),minHeight,area.bottom-top);
 return {left,top,left+resizedWidth,top+resizedHeight};
}
std::wstring WheelModifierLabel(UINT modifiers){std::wstring text;for(auto item:{std::pair<UINT,const wchar_t*>{MOD_CONTROL,L"Ctrl"},{MOD_ALT,L"Alt"},{MOD_SHIFT,L"Shift"},{MOD_WIN,L"Win"}})if(modifiers&item.first){if(!text.empty())text+=L"+";text+=item.second;}return text;}
}
