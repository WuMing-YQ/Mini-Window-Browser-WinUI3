#include "PageSignals.h"
#include <winrt/Windows.Data.Json.h>
#include <cmath>
namespace miniwindow {
std::optional<PageSignal> ParsePageSignal(const std::wstring& text) {
    if(text.size()>8192) return {};
    try {
        using namespace winrt::Windows::Data::Json;
        JsonObject object;
        if(!JsonObject::TryParse(text,object)) return {};
        const auto kind=object.GetNamedString(L"kind",L"");
        if(kind==L"fullscreen") {
            const auto value=object.GetNamedValue(L"flag");
            if(value.ValueType()!=JsonValueType::Boolean) return {};
            return PageSignal{PageSignal::Fullscreen,value.GetBoolean()};
        }
        if(kind==L"aspect") {
            const auto value=object.GetNamedValue(L"aspect");
            if(value.ValueType()!=JsonValueType::Number) return {};
            const double aspect=value.GetNumber();
            if(!std::isfinite(aspect)||aspect<=0.1||aspect>=10) return {};
            return PageSignal{PageSignal::VideoAspect,false,aspect};
        }
    } catch(const winrt::hresult_error&) {}
    return {};
}
}
