#include "Test.h"
#include "../browser/WebLifecycle.h"
void WebLifecycleTests() {
    miniwindow::WebLifecycle life;
    const auto first=life.Begin();CHECK(life.IsCurrent(first));
    life.Close();CHECK(!life.IsCurrent(first));
    const auto second=life.Begin();CHECK(life.IsCurrent(second));CHECK(!life.IsCurrent(first));
    const auto third=life.Begin();CHECK(!life.IsCurrent(second));CHECK(life.IsCurrent(third));
    life.Close();life.Close();CHECK(!life.IsCurrent(third));
}
