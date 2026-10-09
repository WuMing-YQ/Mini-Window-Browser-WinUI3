#include "Test.h"
#include <winrt/base.h>
void SessionTests();
void CoordinatorTests();
void WebLifecycleTests();
void PageSignalsTests();
void AtomicFileTests();
void ConfigMigrationTests();
int main() {
    try {
        winrt::init_apartment();
        SessionTests(); CoordinatorTests(); WebLifecycleTests(); PageSignalsTests(); AtomicFileTests(); ConfigMigrationTests();
        std::cout << "PASS " << tests::checks << " WinUI core checks\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n'; return 1;
    }
}
