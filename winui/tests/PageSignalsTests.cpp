#include "Test.h"
#include "../browser/PageSignals.h"
void PageSignalsTests() {
    using namespace miniwindow;
    CHECK(!ParsePageSignal(L"not json"));
    CHECK(!ParsePageSignal(L"{\"kind\":\"windowCommand\",\"flag\":true}"));
    CHECK(!ParsePageSignal(L"{\"kind\":\"aspect\",\"aspect\":1e999}"));
    CHECK(!ParsePageSignal(L"{\"kind\":\"aspect\",\"aspect\":0}"));
    CHECK(!ParsePageSignal(L"{\"kind\":\"typing\",\"flag\":\"true\"}"));
    CHECK(!ParsePageSignal(L"{\"kind\":\"typing\",\"flag\":true} trailing"));
    auto value=ParsePageSignal(L"{\"kind\":\"fullscreen\",\"flag\":false}");
    CHECK(value && value->kind==PageSignal::Fullscreen && !value->flag);
    value=ParsePageSignal(L"{\"kind\":\"aspect\",\"aspect\":1.77777777778}");
    CHECK(value && value->kind==PageSignal::VideoAspect && value->aspect>1.77 && value->aspect<1.78);
    CHECK(!ParsePageSignal(std::wstring(9000,L'x')));
}
