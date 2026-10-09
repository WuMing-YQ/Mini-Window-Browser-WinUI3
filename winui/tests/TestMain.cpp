#include "Test.h"
void SessionTests();
void CoordinatorTests();
int main() {
    try {
        SessionTests(); CoordinatorTests();
        std::cout << "PASS " << tests::checks << " WinUI core checks\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n'; return 1;
    }
}
