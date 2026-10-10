#include "Test.h"
#include "../input/InputService.h"
void InputRoutingTests() {
    using namespace xiaochuang;
    miniwindow::AppState state;int triggers=0;
    miniwindow::InputService service(nullptr,state,[&](auto,auto){++triggers;});
    service.BeginCapture(HotkeyAction::PlayPause);CHECK(service.Suppressed());service.CancelCapture();CHECK(!service.Suppressed());
    state.hotkeys[0].enabled=false;service.Dispatch(HotkeyAction::Immersion,HotkeyGesture::Trigger);CHECK(triggers==0);
    state.hotkeys[0].enabled=true;service.Dispatch(HotkeyAction::Immersion,HotkeyGesture::Trigger);CHECK(triggers==1);
}
