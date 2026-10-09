#include "Test.h"
#include "../window/ThemeState.h"
void ThemeStateTests() {
    using xiaochuang::ThemeMode;
    CHECK(miniwindow::ResolveDarkTheme(ThemeMode::System,true));
    CHECK(!miniwindow::ResolveDarkTheme(ThemeMode::System,false));
    CHECK(!miniwindow::ResolveDarkTheme(ThemeMode::Light,true));
    CHECK(miniwindow::ResolveDarkTheme(ThemeMode::Dark,false));
    const auto dark=miniwindow::CaptionPalette(true),light=miniwindow::CaptionPalette(false);
    CHECK(dark.background!=light.background && dark.foreground!=light.foreground);
    CHECK(dark.buttonBackground==dark.background && light.buttonBackground==light.background);
}
