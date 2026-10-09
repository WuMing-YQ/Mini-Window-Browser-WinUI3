#pragma once
namespace miniwindow {
enum class ChromeLayout {Full,Compact,Small,Minimal};
inline ChromeLayout CalculateChromeLayout(double widthDip){return widthDip>=600?ChromeLayout::Full:widthDip>=390?ChromeLayout::Compact:widthDip>=260?ChromeLayout::Small:ChromeLayout::Minimal;}
}
