#include "Test.h"
#include "../immersion/ImmersionPolicy.h"
void ImmersionPolicyTests(){using namespace miniwindow;
 CHECK(CalculateImmersionOpacity(WindowMode::Normal,0,100,true)==100);CHECK(CalculateImmersionOpacity(WindowMode::Maximized,1,100,true)==100);
 CHECK(CalculateImmersionOpacity(WindowMode::ImmersionAutoHide,80,50,true)==40);CHECK(CalculateImmersionOpacity(WindowMode::ImmersionAutoHide,80,50,false)==80);
 CHECK(CalculateImmersionOpacity(WindowMode::ImmersionAutoHide,80,0,true)==80);CHECK(CalculateImmersionOpacity(WindowMode::ImmersionAutoHide,80,100,true)==0);
 CHECK(CalculateImmersionOpacity(WindowMode::ImmersionHole,80,100,true)==80);CHECK(CalculateImmersionOpacity(WindowMode::ImmersionHole,80,100,false)==80);
 CHECK(CalculateImmersionOpacity(WindowMode::ImmersionAutoHide,0,50,true)==0);CHECK(ToWindowAlpha(40)==102);CHECK(ToWindowAlpha(100)==255);CHECK(ToWindowAlpha(0)==0);
 CHECK(CursorWithin({-400,-200,100,200},{-399,-199}));CHECK(!CursorWithin({-400,-200,100,200},{100,0}));
}
