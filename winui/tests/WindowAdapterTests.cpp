#include "Test.h"
#include "../window/WindowService.h"
#include "../window/TrayController.h"
#include <commctrl.h>
void WindowAdapterTests() {
    HWND hwnd=CreateWindowExW(0,L"STATIC",L"window adapter fixture",WS_OVERLAPPEDWINDOW,100,100,700,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    CHECK(hwnd!=nullptr);
    miniwindow::AppSettings settings;bool chrome=true;unsigned layouts=0;
    {
        miniwindow::WindowService service(hwnd,settings,[&](bool visible){chrome=visible;},[&]{++layouts;});
        service.Initialize({100,100,800,600},false);
        const auto before=service.NormalRect();
        xiaochuang::WindowOperations operations;
        BYTE alpha=255;operations.alpha=[&](HWND,COLORREF,BYTE value,DWORD){alpha=value;return true;};
        auto realPosition=operations.position;bool fail=true;UINT lastFlags=0;
        operations.position=[&](HWND w,HWND after,int x,int y,int width,int height,UINT flags){lastFlags=flags;if(fail){fail=false;SetLastError(ERROR_ACCESS_DENIED);return false;}return realPosition(w,after,x,y,width,height,flags);};
        service.SetOperations(operations);
        CHECK(!service.SetMode(miniwindow::WindowMode::ImmersionHole).ok);
        CHECK(service.Mode()==miniwindow::WindowMode::Normal && chrome);
        RECT rect{};GetWindowRect(hwnd,&rect);CHECK(EqualRect(&before,&rect));
        CHECK((GetWindowLongPtrW(hwnd,GWL_EXSTYLE)&(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE))==0);
        CHECK(service.SetHidden(true).ok);lastFlags=0;CHECK(service.RefreshVisuals().ok);CHECK((lastFlags&SWP_SHOWWINDOW)==0);CHECK(!IsWindowVisible(hwnd));CHECK(service.SetHidden(false).ok);
        CHECK(service.SetMode(miniwindow::WindowMode::Maximized).ok);
        CHECK(service.Mode()==miniwindow::WindowMode::Maximized);
        CHECK(!service.RestoreForDrag({400,2}).ok);CHECK(service.Mode()==miniwindow::WindowMode::Maximized);
        settings.maximizedTopDragEnabled=true;CHECK(service.RestoreForDrag({400,2}).ok);CHECK(service.Mode()==miniwindow::WindowMode::Normal);
        CHECK(service.SetMode(miniwindow::WindowMode::Maximized).ok);
        settings.autoHideOpacityPercent=0;
        {auto result=service.SetMode(miniwindow::WindowMode::ImmersionAutoHide);if(!result.ok){std::string utf8(result.message.size()*4,'\0');utf8.resize(WideCharToMultiByte(CP_UTF8,0,result.message.data(),static_cast<int>(result.message.size()),utf8.data(),static_cast<int>(utf8.size()),nullptr,nullptr));std::cerr<<utf8<<"\n";}CHECK(result.ok);}
        CHECK(!chrome && service.RecoveryAvailable());
        CHECK(service.SetHidden(true).ok && service.Hidden());
        CHECK(service.RecoveryAvailable());
        CHECK(service.RefreshVisuals().ok);CHECK(!IsWindowVisible(hwnd));
        settings.immersionStyle=miniwindow::ImmersionStyle::Hole;CHECK(service.RefreshVisuals().ok);CHECK(!IsWindowVisible(hwnd));
        CHECK(service.RestoreNormal().ok);
        CHECK(service.Mode()==miniwindow::WindowMode::Normal && !service.Hidden() && chrome);
        const auto ex=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
        CHECK((ex&(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_LAYERED))==0);CHECK(alpha==255);
        HRGN region=CreateRectRgn(0,0,0,0);CHECK(GetWindowRgn(hwnd,region)==ERROR);DeleteObject(region);
        CHECK(layouts>0);
        service.RefreshTheme(xiaochuang::ThemeMode::Dark);CHECK(service.Theme()==xiaochuang::ThemeMode::Dark);
        CHECK(service.SetMode(miniwindow::WindowMode::ImmersionHole).ok);CHECK(service.RestoreNormal().ok);
        CHECK(service.Theme()==xiaochuang::ThemeMode::Dark);
        settings.autoFitVideoFullscreen=true;
        for(auto mode:{miniwindow::WindowMode::Normal,miniwindow::WindowMode::Maximized,miniwindow::WindowMode::ImmersionHole,miniwindow::WindowMode::ImmersionAutoHide}){
            CHECK(service.SetMode(mode).ok);RECT original{};GetWindowRect(hwnd,&original);
            service.OnWebFullscreen(true,16.0/9.0);
            service.OnWebFullscreen(true,4.0/3.0);
            service.OnWebFullscreen(false);RECT restored{};GetWindowRect(hwnd,&restored);
            CHECK(EqualRect(&original,&restored));CHECK(service.Mode()==mode);
        }
        CHECK(service.RestoreNormal().ok);
        // A failure during recovery must not skip the remaining recovery steps.
        CHECK(service.SetMode(miniwindow::WindowMode::ImmersionHole).ok);
        unsigned styles=0;auto realStyle=operations.style;
        operations.style=[&](HWND w,int index,LONG_PTR value){++styles;if(styles==1){SetLastError(ERROR_ACCESS_DENIED);return false;}return realStyle(w,index,value);};
        service.SetOperations(operations);
        CHECK(!service.RestoreNormal().ok);CHECK(styles>=2);CHECK(service.RecoveryAvailable());
        operations.style=realStyle;service.SetOperations(operations);CHECK(service.RestoreNormal().ok);
    }
    DestroyWindow(hwnd);
    CHECK(miniwindow::NeedsTray(true,false,false));CHECK(miniwindow::NeedsTray(false,true,false));
    CHECK(miniwindow::NeedsTray(false,false,true));CHECK(!miniwindow::NeedsTray(false,false,false));
}
