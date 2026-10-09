#include "Test.h"
#include "../input/InputService.h"
#include "../../src/CoreLogic.h"
void HoldCancelTests() {
    using namespace xiaochuang;
    HoldGestureState oldGesture;oldGesture.Press(1000);
    CHECK(oldGesture.Tick(1399)==HoldGestureResult::None);CHECK(oldGesture.Tick(1400)==HoldGestureResult::HoldStart);
    miniwindow::AppState state;std::vector<HotkeyGesture> gestures;
    miniwindow::InputService service(nullptr,state,[&](auto,auto gesture){gestures.push_back(gesture);});
    auto hold=[&]{service.Dispatch(HotkeyAction::SeekForward,HotkeyGesture::HoldStart);};
    auto count=[&](HotkeyGesture target){return std::count(gestures.begin(),gestures.end(),target);};
    hold();service.OnMessage(WM_ACTIVATE,WA_INACTIVE,0);service.CancelGesture();CHECK(count(HotkeyGesture::HoldStop)==1);
    hold();service.CancelGesture();service.CancelGesture();CHECK(count(HotkeyGesture::HoldStop)==2);
    hold();state.windowHidden=true;service.RefreshRuntime();service.RefreshRuntime();CHECK(count(HotkeyGesture::HoldStop)==3);state.windowHidden=false;
    hold();service.BeginCapture(HotkeyAction::PlayPause);service.CancelCapture();CHECK(count(HotkeyGesture::HoldStop)==4);
    hold();service.CancelCapture();service.CancelCapture();CHECK(count(HotkeyGesture::HoldStop)==5);
    hold();service.Stop();service.Stop();CHECK(count(HotkeyGesture::HoldStop)==6);
    service.Dispatch(HotkeyAction::SeekForward,HotkeyGesture::HoldStop);CHECK(count(HotkeyGesture::HoldStop)==6);
    CHECK(count(HotkeyGesture::HoldStart)==6);
}
