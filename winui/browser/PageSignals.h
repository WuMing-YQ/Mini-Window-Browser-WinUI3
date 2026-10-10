#pragma once
#include <optional>
#include <string>
namespace miniwindow {
struct PageSignal {enum Kind {Fullscreen,VideoAspect};Kind kind;bool flag=false;double aspect=0;};
std::optional<PageSignal> ParsePageSignal(const std::wstring&);
}
