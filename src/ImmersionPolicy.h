#pragma once
#include "AppModel.h"
#include <cmath>
namespace xiaochuang {
inline bool CursorWithin(RECT bounds,POINT cursor){return cursor.x>=bounds.left&&cursor.x<bounds.right&&cursor.y>=bounds.top&&cursor.y<bounds.bottom;}
inline double CalculateImmersionOpacity(WindowMode mode,int basis,int reduction,bool cursorInside){
 if(mode!=WindowMode::ImmersionHole&&mode!=WindowMode::ImmersionAutoHide)return 100.0;
 const double value=std::clamp(basis,0,100);
 return mode==WindowMode::ImmersionAutoHide&&cursorInside?value*(1.0-std::clamp(reduction,0,100)/100.0):value;
}
inline BYTE ToWindowAlpha(double percent){return static_cast<BYTE>(std::lround(std::clamp(percent,0.0,100.0)*255.0/100.0));}
}
