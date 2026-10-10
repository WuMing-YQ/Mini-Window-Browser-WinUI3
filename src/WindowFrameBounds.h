#pragma once
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace xiaochuang {
// Both rectangles use physical screen pixels in our PerMonitorV2 UI thread.
// DWM excludes the invisible resize border that GetWindowRect includes.
struct WindowFrameInsets {
    LONG left=0,top=0,right=0,bottom=0;
    RECT Visible(RECT outer) const {
        return {outer.left+left,outer.top+top,outer.right-right,outer.bottom-bottom};
    }
    RECT Outer(RECT visible) const {
        return {visible.left-left,visible.top-top,visible.right+right,visible.bottom+bottom};
    }
};
inline WindowFrameInsets ReadWindowFrameInsets(HWND window) {
    RECT outer{},visible{};
    if(!window||IsIconic(window)||!GetWindowRect(window,&outer)||
       FAILED(DwmGetWindowAttribute(window,DWMWA_EXTENDED_FRAME_BOUNDS,&visible,sizeof(visible)))||
       visible.right<=visible.left||visible.bottom<=visible.top||
       visible.left<outer.left||visible.top<outer.top||
       visible.right>outer.right||visible.bottom>outer.bottom)return {};
    return {visible.left-outer.left,visible.top-outer.top,
            outer.right-visible.right,outer.bottom-visible.bottom};
}
}
