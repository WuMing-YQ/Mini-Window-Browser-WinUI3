#include "Test.h"
#include "../input/InputService.h"
void InputRoutingTests() {
    using namespace xiaochuang;
    CHECK(ShouldSuppressTyping(true,true,false,true,false));
    CHECK(!ShouldSuppressTyping(true,false,false,true,false));
    CHECK(!ShouldSuppressTyping(true,true,false,false,false));
    CHECK(ShouldSuppressTyping(true,false,true,true,false));
    CHECK(!ShouldSuppressTyping(true,false,true,false,false));
    CHECK(!ShouldSuppressTyping(false,true,true,true,true));
    CHECK(ShouldSuppressTyping(true,false,false,false,true));
    InputFocusSnapshot current{12,reinterpret_cast<HWND>(1),2,reinterpret_cast<HWND>(3),reinterpret_cast<HWND>(1),4};
    InputObservation observed{12,reinterpret_cast<HWND>(1),2,reinterpret_cast<HWND>(3),reinterpret_cast<HWND>(1),4,true};
    CHECK(IsCurrentInputObservation(observed,current));
    observed.sequence=11;CHECK(!IsCurrentInputObservation(observed,current));observed.sequence=12;
    observed.foreground=reinterpret_cast<HWND>(5);CHECK(!IsCurrentInputObservation(observed,current));observed.foreground=current.foreground;
    observed.focus=reinterpret_cast<HWND>(6);CHECK(!IsCurrentInputObservation(observed,current));observed.focus=current.focus;
    observed.automationRoot=reinterpret_cast<HWND>(9);observed.automationProcess=99;CHECK(!IsCurrentInputObservation(observed,current));
    observed.automationProcess=4;CHECK(IsCurrentInputObservation(observed,current));
    observed.automationProcess=99;observed.typing=false;CHECK(IsCurrentInputObservation(observed,current));
    miniwindow::AppState state;int triggers=0;
    miniwindow::InputService service(nullptr,state,[&](auto,auto){++triggers;});
    service.BeginCapture(HotkeyAction::PlayPause);CHECK(service.Suppressed());service.CancelCapture();CHECK(!service.Suppressed());
    state.hotkeys[0].enabled=false;service.Dispatch(HotkeyAction::Immersion,HotkeyGesture::Trigger);CHECK(triggers==0);
    state.hotkeys[0].enabled=true;service.Dispatch(HotkeyAction::Immersion,HotkeyGesture::Trigger);CHECK(triggers==1);
}
