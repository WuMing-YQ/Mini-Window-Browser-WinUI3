#include "Test.h"
#include "../core/ChromeLayout.h"
void LayoutPolicyTests(){using namespace miniwindow;
 CHECK(CalculateChromeLayout(600)==ChromeLayout::Full);CHECK(CalculateChromeLayout(599)==ChromeLayout::Compact);
 CHECK(CalculateChromeLayout(390)==ChromeLayout::Compact);CHECK(CalculateChromeLayout(389)==ChromeLayout::Small);
 CHECK(CalculateChromeLayout(260)==ChromeLayout::Small);CHECK(CalculateChromeLayout(259)==ChromeLayout::Minimal);
}
