#pragma once
#include <iostream>
#include <stdexcept>
namespace tests {
inline int checks=0;
inline void Check(bool condition, const char* reason) {
    ++checks;
    if (!condition) throw std::runtime_error(reason);
}
}
#define CHECK(expression) tests::Check(!!(expression), #expression)
