#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
namespace miniwindow {
// Temporary, bounded diagnostics for user-performed pointer input. Never log
// URLs, document text, typed keys, or pointer positions.
inline void TraceBrowserInput(const char* stage,UINT event=0,long long detail=0) noexcept {
    try {
        static std::map<std::string,unsigned> counts;
        if(++counts[stage]>24)return;
        static std::ofstream log([]{
            wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
            return std::filesystem::path(path).parent_path()/(L"browser-input-"+std::to_wstring(GetCurrentProcessId())+L".log");
        }(),std::ios::app);
        log<<GetTickCount64()<<" "<<stage<<" event="<<event<<" detail="<<detail<<std::endl;
    } catch(...) {}
}
inline void TraceBrowserWindows(HWND main) noexcept {
    TraceBrowserInput("window-main-style",0,GetWindowLongPtrW(main,GWL_STYLE));
    TraceBrowserInput("window-main-exstyle",0,GetWindowLongPtrW(main,GWL_EXSTYLE));
    EnumChildWindows(main,[](HWND child,LPARAM)->BOOL{
        char name[256]{};GetClassNameA(child,name,256);
        TraceBrowserInput(name,static_cast<UINT>(IsWindowEnabled(child)),GetWindowLongPtrW(child,GWL_EXSTYLE));
        return TRUE;
    },0);
}
}
