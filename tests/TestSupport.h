#pragma once
#include <cstdlib>
#include <iostream>

namespace BassFinisherTests {
inline void require(bool condition, const char* expression, const char* file, int line) {
    if (condition) return;
    std::cerr << "FAIL: " << expression << " at " << file << ":" << line << "\n";
    std::exit(EXIT_FAILURE);
}
}
#define BF_REQUIRE(expr) ::BassFinisherTests::require(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
