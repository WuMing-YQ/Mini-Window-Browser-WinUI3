#pragma once
#include "../core/Ports.h"
#include <shellapi.h>
namespace miniwindow {
constexpr UINT kTrayMessage=WM_APP+50;
enum class TrayAction {Show,RestoreNormal,ImmersionControls,Settings,Exit};
inline bool NeedsTray(bool immersion,bool hidden,bool minimizeToTray) {return immersion||hidden||minimizeToTray;}
class TrayController {
public:
    TrayController(HWND,std::function<void(TrayAction)>);
    ~TrayController();
    void Sync(bool immersion,bool hidden,bool minimizeToTray);
    void HandleMessage(UINT,WPARAM,LPARAM);
    bool Available() const noexcept {return present_;}
    const std::wstring& Error() const noexcept {return error_;}
private:
    HWND hwnd_;std::function<void(TrayAction)> action_;NOTIFYICONDATAW icon_{};
    bool present_=false,wanted_=false;std::wstring error_;UINT taskbarCreated_=0;
};
}
