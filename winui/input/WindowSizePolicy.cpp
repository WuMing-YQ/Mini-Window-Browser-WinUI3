#include "WindowSizePolicy.h"
#include <algorithm>
#include <cmath>
namespace miniwindow {
namespace {
LONG ResizeAxis(LONG start,LONG end,LONG low,LONG high,LONG size,int snapDistance){
 const LONG tolerance=std::max(1,snapDistance);
 const bool atStart=start<=low+tolerance,atEnd=end>=high-tolerance;
 // Corners anchor both axes. A single docked edge anchors its axis while the
 // free axis grows equally around its current centre. Two opposite edges
 // release symmetrically when shrinking a screen-filling window.
 if(atStart&&!atEnd)return low;
 if(atEnd&&!atStart)return high-size;
 const double centre=(static_cast<double>(start)+end)/2.0;
 const LONG proposed=static_cast<LONG>(std::lround(centre-size/2.0));
 // A newly crossed boundary immediately becomes the anchor for this step,
 // rather than reducing growth to the distance from the old top-left corner.
 if(proposed<low)return low;
 if(proposed+size>high)return high-size;
 return proposed;
}
}
RECT ResizeWindowBySteps(RECT current,int steps,RECT area,SIZE minimum,int snapDistance){
 const LONG areaWidth=area.right-area.left,areaHeight=area.bottom-area.top;
 if(steps==0||areaWidth<=0||areaHeight<=0)return current;
 const double width=std::max(1L,current.right-current.left),height=std::max(1L,current.bottom-current.top);
 const LONG minWidth=std::clamp(minimum.cx,1L,areaWidth),minHeight=std::clamp(minimum.cy,1L,areaHeight);
 const double lower=std::max(minWidth/width,minHeight/height);
 const double upper=std::min(areaWidth/width,areaHeight/height);
 const double scale=std::min(upper,std::max(lower,std::pow(1.05,std::clamp(steps,-100,100))));
 const LONG resizedWidth=std::clamp(static_cast<LONG>(std::lround(width*scale)),minWidth,areaWidth);
 const LONG resizedHeight=std::clamp(static_cast<LONG>(std::lround(height*scale)),minHeight,areaHeight);
 const LONG left=ResizeAxis(current.left,current.right,area.left,area.right,resizedWidth,snapDistance);
 const LONG top=ResizeAxis(current.top,current.bottom,area.top,area.bottom,resizedHeight,snapDistance);
 return {left,top,left+resizedWidth,top+resizedHeight};
}
}
