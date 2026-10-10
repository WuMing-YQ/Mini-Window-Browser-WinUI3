#include "TrayController.h"
#include "AppIcon.h"
#pragma comment(lib,"shell32.lib")
namespace miniwindow {
TrayController::TrayController(HWND hwnd,std::function<void(TrayAction)> action):hwnd_(hwnd),action_(std::move(action)) {
    icon_.cbSize=sizeof(icon_);icon_.hWnd=hwnd_;icon_.uID=1;icon_.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
    icon_.uCallbackMessage=kTrayMessage;icon_.hIcon=ApplicationIcon();wcscpy_s(icon_.szTip,L"小窗浏览器 · 双击恢复普通窗口");
    taskbarCreated_=RegisterWindowMessageW(L"TaskbarCreated");
}
TrayController::~TrayController(){if(present_)Shell_NotifyIconW(NIM_DELETE,&icon_);}
void TrayController::Sync(bool immersion,bool hidden,bool minimizeToTray) {
    wanted_=NeedsTray(immersion,hidden,minimizeToTray);error_.clear();
    if(wanted_&&!present_){present_=Shell_NotifyIconW(NIM_ADD,&icon_)!=FALSE;if(!present_)error_=L"无法创建托盘恢复入口。";}
    else if(!wanted_&&present_){if(Shell_NotifyIconW(NIM_DELETE,&icon_))present_=false;else error_=L"托盘图标移除失败。";}
}
void TrayController::HandleMessage(UINT message,WPARAM,LPARAM parameter) {
    if(message==taskbarCreated_){present_=false;Sync(wanted_,false,false);return;}
    if(message!=kTrayMessage)return;
    if(parameter==WM_LBUTTONDBLCLK){action_(TrayAction::RestoreNormal);return;}
    if(parameter!=WM_RBUTTONUP&&parameter!=WM_CONTEXTMENU)return;
    HMENU menu=CreatePopupMenu();if(!menu)return;
    AppendMenuW(menu,MF_STRING,1,L"显示窗口");AppendMenuW(menu,MF_STRING,2,L"恢复普通窗口");
    AppendMenuW(menu,MF_STRING,3,L"沉浸控制");AppendMenuW(menu,MF_STRING,4,L"设置");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,5,L"退出");
    POINT cursor{};GetCursorPos(&cursor);SetForegroundWindow(hwnd_);
    const auto selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY, cursor.x,cursor.y,0,hwnd_,nullptr);
    DestroyMenu(menu);PostMessageW(hwnd_,WM_NULL,0,0);
    if(selected>=1&&selected<=5)action_(static_cast<TrayAction>(selected-1));
}
}
