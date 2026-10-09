#pragma once
#include "../../src/AppModel.h"
namespace miniwindow {
inline bool ResolveDarkTheme(xiaochuang::ThemeMode mode,bool systemDark) {return mode==xiaochuang::ThemeMode::Dark||(mode==xiaochuang::ThemeMode::System&&systemDark);}
struct CaptionColors {COLORREF background,foreground,buttonBackground;};
inline CaptionColors CaptionPalette(bool dark) {const auto background=dark?RGB(28,29,33):RGB(248,249,252);return {background,dark?RGB(245,246,250):RGB(30,32,38),background};}
}
